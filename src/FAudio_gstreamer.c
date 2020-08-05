/* FAudio - XAudio Reimplementation for FNA
 *
 * Copyright (c) 2011-2018 Ethan Lee, Luigi Auriemma, and the MonoGame Team
 * Copyright (c) 2020 Andrew Eikum for CodeWeavers, Inc
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * In no event will the authors be held liable for any damages arising from
 * the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 * claim that you wrote the original software. If you use this software in a
 * product, an acknowledgment in the product documentation would be
 * appreciated but is not required.
 *
 * 2. Altered source versions must be plainly marked as such, and must not be
 * misrepresented as being the original software.
 *
 * 3. This notice may not be removed or altered from any source distribution.
 *
 * Ethan "flibitijibibo" Lee <flibitijibibo@flibitijibibo.com>
 *
 */

#ifdef HAVE_GSTREAMER

#include "FAudio_internal.h"

#include <gst/gst.h>
#include <gst/audio/audio.h>
#include <gst/app/gstappsink.h>

typedef struct FAudioGSTREAMER
{
	GstPad *srcpad;
	GstElement *pipeline;
	GstElement *dst;
	GstElement *resampler;
	GstSegment segment;
	uint8_t *convertCache, *prevConvertCache;
	size_t convertCacheLen, prevConvertCacheLen;
	uint32_t curBlock, prevBlock;
	size_t *blockSizes;
} FAudioGSTREAMER;

#define SIZE_FROM_DST(sample) \
	((sample) * voice->src.format->nChannels * sizeof(float))
#define SIZE_FROM_SRC(sample) \
	((sample) * voice->src.format->nChannels * (voice->src.format->wBitsPerSample / 8))
#define SIZE_FROM_DST(sample) \
	((sample) * voice->src.format->nChannels * sizeof(float))
#define SAMPLES_FROM_SRC(len) \
	((len) / voice->src.format->nChannels / (voice->src.format->wBitsPerSample / 8))
#define SAMPLES_FROM_DST(len) \
	((len) / voice->src.format->nChannels / sizeof(float))
#define SIZE_SRC_TO_DST(len) \
	((len) * (sizeof(float) / (voice->src.format->wBitsPerSample / 8)))
#define SIZE_DST_TO_SRC(len) \
	((len) / (sizeof(float) / (voice->src.format->wBitsPerSample / 8)))

static int FAudio_GSTREAMER_RestartDecoder(FAudioGSTREAMER *gstreamer)
{
	GstEvent *event;

	event = gst_event_new_flush_start();
	gst_pad_push_event(gstreamer->srcpad, event);

	event = gst_event_new_flush_stop(TRUE);
	gst_pad_push_event(gstreamer->srcpad, event);

	event = gst_event_new_segment(&gstreamer->segment);
	gst_pad_push_event(gstreamer->srcpad, event);

	gstreamer->curBlock = ~0u;
	gstreamer->prevBlock = ~0u;

	if (gst_element_set_state(gstreamer->pipeline, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE)
	{
		return 0;
	}

	return 1;
}

static int FAudio_GSTREAMER_CheckForBusErrors(FAudioVoice *voice)
{
	GstBus *bus;
	GstMessage *msg;
	GError *err = NULL;
	gchar *debug_info = NULL;
	int ret = 0;

	bus = gst_pipeline_get_bus(GST_PIPELINE(voice->src.gstreamer->pipeline));

	while((msg = gst_bus_pop_filtered(bus, GST_MESSAGE_ERROR)))
	{
		switch(GST_MESSAGE_TYPE(msg))
		{
		case GST_MESSAGE_ERROR:
			gst_message_parse_error(msg, &err, &debug_info);
			LOG_ERROR(
				voice->audio,
				"Got gstreamer bus error from %s: %s (%s)",
				GST_OBJECT_NAME(msg->src),
				err->message,
				debug_info ? debug_info : "none"
			)
			g_clear_error(&err);
			g_free(debug_info);
			gst_message_unref(msg);
			ret = 1;
			break;
		default:
			gst_message_unref(msg);
			break;
		}
	}

	gst_object_unref(bus);

	return ret;
}

static size_t FAudio_GSTREAMER_FillConvertCache(
	FAudioVoice *voice,
	FAudioBuffer *buffer,
	size_t maxBytes
) {
	GstBuffer *src, *dst;
	GstSample *sample;
	GstMapInfo info;
	size_t pulled, clipStartBytes, clipEndBytes, toCopyBytes;
	GstAudioClippingMeta *cmeta;
	GstFlowReturn push_ret;

	LOG_FUNC_ENTER(voice->audio)

	/* Write current block to input buffer, push to pipeline */
	src = gst_buffer_new_allocate(
		NULL,
		voice->src.format->nBlockAlign,
		NULL
	);

	if (	gst_buffer_fill(
			src,
			0,
			buffer->pAudioData + (
				voice->src.format->nBlockAlign *
				voice->src.gstreamer->curBlock
			),
			voice->src.format->nBlockAlign
		) != voice->src.format->nBlockAlign	)
	{
		LOG_ERROR(
			voice->audio,
			"for voice %p, failed to copy whole chunk into buffer",
			voice
		);
		gst_buffer_unref(src);
		return (size_t) -1;
	}

	push_ret = gst_pad_push(voice->src.gstreamer->srcpad, src);
	if(	push_ret != GST_FLOW_OK &&
		push_ret != GST_FLOW_EOS	)
	{
		LOG_ERROR(
			voice->audio,
			"for voice %p, pushing buffer failed: 0x%x",
			voice,
			push_ret
		);
		return (size_t) -1;
	}

	pulled = 0;
	while (1)
	{
		/* Pull frames one into cache */
		sample = gst_app_sink_try_pull_sample(
			GST_APP_SINK(voice->src.gstreamer->dst),
			0
		);

		if (!sample)
		{
			/* done decoding */
			break;
		}
		dst = gst_sample_get_buffer(sample);
		gst_buffer_map(dst, &info, GST_MAP_READ);

		cmeta = gst_buffer_get_audio_clipping_meta(dst);
		if (cmeta)
		{
			FAudio_assert(cmeta->format == GST_FORMAT_DEFAULT /* == samples */);
			clipStartBytes = SIZE_FROM_DST(cmeta->start);
			clipEndBytes = SIZE_FROM_DST(cmeta->end);
		}
		else
		{
			clipStartBytes = 0;
			clipEndBytes = 0;
		}

		toCopyBytes = FAudio_min(info.size - (clipStartBytes + clipEndBytes), maxBytes - pulled);

		if (voice->src.gstreamer->convertCacheLen < pulled + toCopyBytes)
		{
			voice->src.gstreamer->convertCacheLen = pulled + toCopyBytes;
			voice->src.gstreamer->convertCache = (uint8_t*) voice->audio->pRealloc(
				voice->src.gstreamer->convertCache,
				pulled + toCopyBytes
			);
		}


		FAudio_memcpy(voice->src.gstreamer->convertCache + pulled,
			info.data + clipStartBytes,
			toCopyBytes
		);

		gst_buffer_unmap(dst, &info);
		gst_sample_unref(sample);

		pulled += toCopyBytes;
	}

	LOG_FUNC_EXIT(voice->audio)

	return pulled;
}

static int FAudio_GSTREAMER_DecodeBlock(FAudioVoice *voice, FAudioBuffer *buffer, uint32_t block, size_t maxBytes)
{
	FAudioGSTREAMER *gstreamer = voice->src.gstreamer;
	uint8_t *tmpBuf;
	size_t tmpLen;

	if (gstreamer->curBlock != ~0u && block != gstreamer->curBlock + 1)
	{
		/* XAudio2 allows looping back to start of XMA buffers, but nothing else */
		if (block != 0)
		{
			LOG_ERROR(
				voice->audio,
				"for voice %p, out of sequence block: %u (cur: %d)\n",
				voice,
				block,
				gstreamer->curBlock
			);
		}
		FAudio_assert(block == 0);
		if (!FAudio_GSTREAMER_RestartDecoder(gstreamer))
		{
			LOG_WARNING(
				voice->audio,
				"%s",
				"Restarting decoder failed!"
			)
		}
	}

	/* store previous block to allow for minor rewinds (FAudio quirk) */
	tmpBuf = gstreamer->prevConvertCache;
	tmpLen = gstreamer->prevConvertCacheLen;
	gstreamer->prevConvertCache = gstreamer->convertCache;
	gstreamer->prevConvertCacheLen = gstreamer->convertCacheLen;
	gstreamer->convertCache = tmpBuf;
	gstreamer->convertCacheLen = tmpLen;

	gstreamer->prevBlock = gstreamer->curBlock;
	gstreamer->curBlock = block;

	gstreamer->blockSizes[block] = FAudio_GSTREAMER_FillConvertCache(
		voice,
		buffer,
		maxBytes
	);

	return gstreamer->blockSizes[block] != (size_t) -1;
}

static void FAudio_INTERNAL_DecodeGSTREAMER(
	FAudioVoice *voice,
	FAudioBuffer *buffer,
	float *decodeCache,
	uint32_t samples
) {
	size_t byteOffset, siz, maxBytes;
	uint32_t curBlock, curBufferOffset;
	uint8_t *convertCache;
	int error = 0;
	FAudioGSTREAMER *gstreamer = voice->src.gstreamer;

	LOG_FUNC_ENTER(voice->audio)

	if (!gstreamer->blockSizes)
	{
		size_t sz = voice->src.bufferList->bufferWMA.PacketCount * sizeof(*gstreamer->blockSizes);
		gstreamer->blockSizes = (size_t *) voice->audio->pMalloc(sz);
		memset(gstreamer->blockSizes, 0xff, sz);
	}

	curBufferOffset = voice->src.curBufferOffset;
decode:
	byteOffset = SIZE_FROM_DST(curBufferOffset);

	/* the last block size can truncate the length of the buffer */
	maxBytes = SIZE_SRC_TO_DST(voice->src.bufferList->bufferWMA.pDecodedPacketCumulativeBytes[voice->src.bufferList->bufferWMA.PacketCount - 1]);
	for (curBlock = 0; curBlock < voice->src.bufferList->bufferWMA.PacketCount; curBlock += 1)
	{
		/* decode to get real size */
		if (gstreamer->blockSizes[curBlock] == (size_t)-1)
		{
			if (!FAudio_GSTREAMER_DecodeBlock(voice, buffer, curBlock, maxBytes))
			{
				error = 1;
				goto done;
			}
		}

		if (gstreamer->blockSizes[curBlock] > byteOffset)
		{
			/* ensure curBlock is decoded in either slot */
			if (gstreamer->curBlock != curBlock && gstreamer->prevBlock != curBlock)
			{
				if (!FAudio_GSTREAMER_DecodeBlock(voice, buffer, curBlock, maxBytes))
				{
					error = 1;
					goto done;
				}
			}
			break;
		}

		byteOffset -= gstreamer->blockSizes[curBlock];
		maxBytes -= gstreamer->blockSizes[curBlock];
		if(maxBytes == 0)
			break;
	}

	if (curBlock >= voice->src.bufferList->bufferWMA.PacketCount || maxBytes == 0)
	{
		goto done;
	}

	/* If we're in a different block from last time, decode! */
	if (curBlock == gstreamer->curBlock)
	{
		convertCache = gstreamer->convertCache;
	}
	else if (curBlock == gstreamer->prevBlock)
	{
		convertCache = gstreamer->prevConvertCache;
	}
	else
	{
		convertCache = NULL;
		FAudio_assert(0 && "Somehow got an undecoded curBlock!");
	}

	/* Copy to decodeCache, finally. */
	siz = FAudio_min(SIZE_FROM_DST(samples), gstreamer->blockSizes[curBlock] - byteOffset);
	if (convertCache)
	{
		FAudio_memcpy(
			decodeCache,
			convertCache + byteOffset,
			siz
		);
	}
	else
	{
		FAudio_memset(
			decodeCache,
			0,
			siz
		);
	}

	/* Increment pointer, decrement remaining sample count */
	decodeCache += siz / sizeof(float);
	samples -= SAMPLES_FROM_DST(siz);
	curBufferOffset += SAMPLES_FROM_DST(siz);

done:
	if (FAudio_GSTREAMER_CheckForBusErrors(voice))
	{
		LOG_ERROR(
			voice->audio,
			"%s",
			"Got a bus error after decoding!"
		)

		error = 1;
	}

	/* If the cache isn't filled yet, keep decoding! */
	if (samples > 0)
	{
		if (	!error &&
			curBlock < voice->src.bufferList->bufferWMA.PacketCount - 1	)
		{
			goto decode;
		}

		/* out of stuff to decode, write blank and exit */
		FAudio_memset(decodeCache, 0, SIZE_FROM_DST(samples));
	}

	LOG_FUNC_EXIT(voice->audio)
}

void FAudio_GSTREAMER_end_buffer(FAudioSourceVoice *pSourceVoice)
{
	FAudioGSTREAMER *gstreamer = pSourceVoice->src.gstreamer;

	LOG_FUNC_ENTER(pSourceVoice->audio)

	pSourceVoice->audio->pFree(gstreamer->blockSizes);
	gstreamer->blockSizes = NULL;

	gstreamer->curBlock = ~0u;
	gstreamer->prevBlock = ~0u;

	LOG_FUNC_EXIT(pSourceVoice->audio)
}

static void FAudio_GSTREAMER_NewDecodebinPad(GstElement *decodebin,
		GstPad *pad, gpointer user)
{
	FAudioGSTREAMER *gstreamer = user;
	GstPad *ac_sink;

	ac_sink = gst_element_get_static_pad(gstreamer->resampler, "sink");
	if (GST_PAD_IS_LINKED(ac_sink))
	{
		gst_object_unref(ac_sink);
		return;
	}

	gst_pad_link(pad, ac_sink);

	gst_object_unref(ac_sink);
}

uint32_t FAudio_GSTREAMER_init(FAudioSourceVoice *pSourceVoice, uint32_t type)
{
	FAudioGSTREAMER *result;
	GstElement *decoder = NULL, *converter = NULL;
	GstCaps *caps;
	int version;
	GstBuffer *codec_data;
	size_t codec_data_size;
	uint8_t *extradata;
	uint8_t fakeextradata[16];
	GstPad *decoder_sink;
	GstEvent *event;

	LOG_FUNC_ENTER(pSourceVoice->audio)

	/* Init GStreamer statically. The docs tell us not to exit, so I guess
	 * we're supposed to just leak!
	 */
	if (!gst_is_initialized())
	{
		/* Apparently they ask for this to leak... */
		gst_init(NULL, NULL);
	}

	/* Match the format with the codec */
	switch (type)
	{
	#define GSTTYPE(fmt, ver) \
		case FAUDIO_FORMAT_##fmt: version = ver; break;
	GSTTYPE(WMAUDIO2, 2)
	GSTTYPE(WMAUDIO3, 3)
	GSTTYPE(WMAUDIO_LOSSLESS, 4)
	/* FIXME: XMA2 */
	#undef GSTTYPE
	default:
		LOG_ERROR(
			pSourceVoice->audio,
			"%X codec not supported!",
			type
		)
		LOG_FUNC_EXIT(pSourceVoice->audio)
		return FAUDIO_E_UNSUPPORTED_FORMAT;
	}

	/* Set up the GStreamer pipeline.
	 * Note that we're not assigning names, since many pipelines will exist
	 * at the same time (one per source voice).
	 */
	result = (FAudioGSTREAMER*) pSourceVoice->audio->pMalloc(sizeof(FAudioGSTREAMER));
	FAudio_zero(result, sizeof(FAudioGSTREAMER));

	result->pipeline = gst_pipeline_new(NULL);

	decoder = gst_element_factory_make("decodebin", NULL);
	if (!decoder)
	{
		LOG_ERROR(
			pSourceVoice->audio,
			"Unable to create gstreamer decodebin; is %zu-bit gst-plugins-base installed?",
			sizeof(void *) * 8
		)
		goto free_without_bin;
	}

	g_signal_connect(decoder, "pad-added", G_CALLBACK(FAudio_GSTREAMER_NewDecodebinPad), result);

	result->srcpad = gst_pad_new(NULL, GST_PAD_SRC);

	result->resampler = gst_element_factory_make("audioresample", NULL);
	if (!result->resampler)
	{
		LOG_ERROR(
			pSourceVoice->audio,
			"Unable to create gstreamer audioresample; is %zu-bit gst-plugins-base installed?",
			sizeof(void *) * 8
		)
		goto free_without_bin;
	}

	converter = gst_element_factory_make("audioconvert", NULL);
	if (!converter)
	{
		LOG_ERROR(
			pSourceVoice->audio,
			"Unable to create gstreamer audioconvert; is %zu-bit gst-plugins-base installed?",
			sizeof(void *) * 8
		)
		goto free_without_bin;
	}

	result->dst = gst_element_factory_make("appsink", NULL);
	if (!result->dst)
	{
		LOG_ERROR(
			pSourceVoice->audio,
			"Unable to create gstreamer appsink; is %zu-bit gst-plugins-base installed?",
			sizeof(void *) * 8
		)
		goto free_without_bin;
	}

	/* turn off sync so we can pull data without waiting for it to "play" in realtime */
	g_object_set(G_OBJECT(result->dst), "sync", FALSE, "async", TRUE, NULL);

	/* Compile the pipeline, finally. */
	if (!gst_pad_set_active(result->srcpad, TRUE))
	{
		LOG_ERROR(
			pSourceVoice->audio,
			"%s",
			"Unable to activate srcpad"
		)
		goto free_without_bin;
	}

	gst_bin_add_many(
		GST_BIN(result->pipeline),
		decoder,
		result->resampler,
		converter,
		result->dst,
		NULL
	);

	decoder_sink = gst_element_get_static_pad(decoder, "sink");

	if (gst_pad_link(result->srcpad, decoder_sink) != GST_PAD_LINK_OK)
	{
		LOG_ERROR(
			pSourceVoice->audio,
			"%s",
			"Unable to get link our src pad to decoder sink pad"
		)
		gst_object_unref(decoder_sink);
		goto free_with_bin;
	}

	gst_object_unref(decoder_sink);

	if (!gst_element_link_many(
		result->resampler,
		converter,
		result->dst,
		NULL))
	{
		LOG_ERROR(
			pSourceVoice->audio,
			"%s",
			"Unable to get link pipeline"
		)
		goto free_with_bin;
	}

	/* send stream-start */
	event = gst_event_new_stream_start("faudio/gstreamer");
	gst_pad_push_event(result->srcpad, event);

	/* Prepare the input format */
	if (type == FAUDIO_FORMAT_WMAUDIO3)
	{
		const FAudioWaveFormatExtensible *wfx =
			(FAudioWaveFormatExtensible*) pSourceVoice->src.format;
		extradata = (uint8_t*) &wfx->Samples;
		codec_data_size = pSourceVoice->src.format->cbSize;
	}
	else if (type == FAUDIO_FORMAT_WMAUDIO2)
	{
		FAudio_zero(fakeextradata, sizeof(fakeextradata));
		fakeextradata[4] = 31;

		extradata = fakeextradata;
		codec_data_size = sizeof(fakeextradata);
	}
	else
	{
		extradata = NULL;
		FAudio_assert(0 && "Unrecognized WMA format!");
	}
	codec_data = gst_buffer_new_and_alloc(codec_data_size);
	gst_buffer_fill(codec_data, 0, extradata, codec_data_size);
	caps = gst_caps_new_simple(
		"audio/x-wma",
		"wmaversion",	G_TYPE_INT, version,
		"bitrate",	G_TYPE_INT, pSourceVoice->src.format->nAvgBytesPerSec * 8,
		"channels",	G_TYPE_INT, pSourceVoice->src.format->nChannels,
		"rate",		G_TYPE_INT, pSourceVoice->src.format->nSamplesPerSec,
		"block_align",	G_TYPE_INT, pSourceVoice->src.format->nBlockAlign,
		"depth",	G_TYPE_INT, pSourceVoice->src.format->wBitsPerSample,
		"codec_data",	GST_TYPE_BUFFER, codec_data,
		NULL
	);
	event = gst_event_new_caps(caps);
	gst_pad_push_event(result->srcpad, event);
	gst_caps_unref(caps);
	gst_buffer_unref(codec_data);

	/* Prepare the output format */
	caps = gst_caps_new_simple(
		"audio/x-raw",
		"format",	G_TYPE_STRING, gst_audio_format_to_string(GST_AUDIO_FORMAT_F32),
		"layout",	G_TYPE_STRING, "interleaved",
		"channels",	G_TYPE_INT, pSourceVoice->src.format->nChannels,
		"rate",		G_TYPE_INT, pSourceVoice->src.format->nSamplesPerSec,
		NULL
	);

	gst_app_sink_set_caps(GST_APP_SINK(result->dst), caps);
	gst_caps_unref(caps);

	gst_segment_init(&result->segment, GST_FORMAT_TIME);

	if (!FAudio_GSTREAMER_RestartDecoder(result))
	{
		LOG_ERROR(
			pSourceVoice->audio,
			"%s",
			"Starting decoder failed!"
		)
		goto free_with_bin;
	}

	pSourceVoice->src.gstreamer = result;
	pSourceVoice->src.decode = FAudio_INTERNAL_DecodeGSTREAMER;

	if (FAudio_GSTREAMER_CheckForBusErrors(pSourceVoice))
	{
		LOG_ERROR(
			pSourceVoice->audio,
			"%s",
			"Got a bus error after creation!"
		)

		pSourceVoice->src.gstreamer = NULL;
		pSourceVoice->src.decode = NULL;

		goto free_with_bin;
	}

	LOG_FUNC_EXIT(pSourceVoice->audio)
	return 0;

free_without_bin:
	if (result->dst)
	{
		gst_object_unref(result->dst);
	}
	if (converter)
	{
		gst_object_unref(converter);
	}
	if (result->resampler)
	{
		gst_object_unref(result->resampler);
	}
	if (result->srcpad)
	{
		gst_object_unref(result->srcpad);
	}
	if (decoder)
	{
		gst_object_unref(decoder);
	}
	if (result->pipeline)
	{
		gst_object_unref(result->pipeline);
	}
	pSourceVoice->audio->pFree(result);
	LOG_FUNC_EXIT(pSourceVoice->audio)
	return FAUDIO_E_UNSUPPORTED_FORMAT;

free_with_bin:
	gst_object_unref(result->srcpad);
	gst_object_unref(result->pipeline);
	pSourceVoice->audio->pFree(result);
	LOG_FUNC_EXIT(pSourceVoice->audio)
	return FAUDIO_E_UNSUPPORTED_FORMAT;
}

void FAudio_GSTREAMER_free(FAudioSourceVoice *voice)
{
	LOG_FUNC_ENTER(voice->audio)
	gst_element_set_state(voice->src.gstreamer->pipeline, GST_STATE_NULL);
	gst_object_unref(voice->src.gstreamer->pipeline);
	gst_object_unref(voice->src.gstreamer->srcpad);
	voice->audio->pFree(voice->src.gstreamer->convertCache);
	voice->audio->pFree(voice->src.gstreamer->prevConvertCache);
	voice->audio->pFree(voice->src.gstreamer->blockSizes);
	voice->audio->pFree(voice->src.gstreamer);
	voice->src.gstreamer = NULL;
	LOG_FUNC_EXIT(voice->audio)
}

#else

extern int this_tu_is_empty;

#endif /* HAVE_GSTREAMER */

/* vim: set noexpandtab shiftwidth=8 tabstop=8: */
