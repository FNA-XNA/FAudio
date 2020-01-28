/* FAudio - XAudio Reimplementation for FNA
 *
 * Copyright (c) 2011-2018 Ethan Lee, Luigi Auriemma, and the MonoGame Team
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
#include <gst/app/gstappsrc.h>
#include <gst/app/gstappsink.h>

#define REMOVE_ME 0
#if REMOVE_ME
static int intendedTotal = 0, actualTotal = 0, printFinal = 1;
#endif

typedef struct FAudioGSTREAMER
{
	GstElement *pipeline;
	GstElement *src;
	GstElement *dst;
	uint8_t *convertCache;
	uint32_t curBlock;
} FAudioGSTREAMER;

static void FAudio_GSTREAMER_FillConvertCache(
	FAudioVoice *voice,
	FAudioBuffer *buffer,
	size_t totalSize
) {
	GstBuffer *src, *dst;
	GstSample *sample;
	GstMapInfo info;

	LOG_FUNC_ENTER(voice->audio)

	/* Write current block to input buffer, push to pipeline */
	src = gst_buffer_new_allocate(
		NULL,
		voice->src.format->nBlockAlign,
		NULL
	);
	g_assert(gst_buffer_fill(
		src,
		0,
		buffer->pAudioData + (
			voice->src.format->nBlockAlign *
			voice->src.gstreamer->curBlock
		),
		voice->src.format->nBlockAlign
	) == voice->src.format->nBlockAlign);
	g_assert(gst_app_src_push_buffer(
		GST_APP_SRC(voice->src.gstreamer->src),
		src
	) == GST_FLOW_OK);

	/* Pull frames one into cache */
	sample = gst_app_sink_pull_sample(
		GST_APP_SINK(voice->src.gstreamer->dst)
	);
	g_assert(!gst_app_sink_is_eos(
		GST_APP_SINK(voice->src.gstreamer->dst)
	));
	g_assert(sample);
	dst = gst_sample_get_buffer(sample);
	gst_buffer_map(dst, &info, GST_MAP_READ);
#if REMOVE_ME
	intendedTotal += totalSize;
	actualTotal += info.size;
	printf("dpds Intended %d Actual %d\n", totalSize, info.size);
#endif
	FAudio_memcpy(voice->src.gstreamer->convertCache, info.data, totalSize);
	if (totalSize > info.size)
	{
		/* FIXME: Do we have to loop to get all the block samples? */
		FAudio_zero(
			voice->src.gstreamer->convertCache + info.size,
			totalSize - info.size
		);
	}
	gst_buffer_unmap(dst, &info);
	gst_sample_unref(sample);

	LOG_FUNC_EXIT(voice->audio)
}

static void FAudio_INTERNAL_DecodeGSTREAMER(
	FAudioVoice *voice,
	FAudioBuffer *buffer,
	float *decodeCache,
	uint32_t samples
) {
	size_t byteOffset, siz, totalSize;
	uint32_t curBlock, curBufferOffset;
	uint32_t i;
	FAudioGSTREAMER *gstreamer = voice->src.gstreamer;

	LOG_FUNC_ENTER(voice->audio)

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

	/* Resize convert cache */
	voice->src.gstreamer->convertCache = (uint8_t*) voice->audio->pRealloc(
		voice->src.gstreamer->convertCache,
		/* This is overkill, but whatever... */
		SIZE_SRC_TO_DST(voice->src.bufferList->bufferWMA.pDecodedPacketCumulativeBytes[
			voice->src.bufferList->bufferWMA.PacketCount - 1
		])
	);

#if REMOVE_ME
	if (printFinal)
	{
		printFinal = 0;
		printf(
			"FINAL SIZE %d\n",
			voice->src.bufferList->bufferWMA.pDecodedPacketCumulativeBytes[
				voice->src.bufferList->bufferWMA.PacketCount - 1
			]
		);
	}
#endif

	curBufferOffset = voice->src.curBufferOffset;
decode:
	/* Figure out which packet we're in right now... */
	byteOffset = SIZE_FROM_SRC(curBufferOffset);
	for (i = 0; i < voice->src.bufferList->bufferWMA.PacketCount; i += 1)
	{
		if (byteOffset < voice->src.bufferList->bufferWMA.pDecodedPacketCumulativeBytes[i])
		{
			curBlock = i;
			break;
		}
	}
	FAudio_assert(i < voice->src.bufferList->bufferWMA.PacketCount);

	/* Determine the copy size, maximum sample block size */
	totalSize = voice->src.bufferList->bufferWMA.pDecodedPacketCumulativeBytes[i];
	if (i > 0)
	{
		byteOffset -= voice->src.bufferList->bufferWMA.pDecodedPacketCumulativeBytes[i - 1];
		totalSize -= voice->src.bufferList->bufferWMA.pDecodedPacketCumulativeBytes[i - 1];
	}

	/* If we're in a different block from last time, decode! */
	if (curBlock != gstreamer->curBlock)
	{
		gstreamer->curBlock = curBlock;
		FAudio_GSTREAMER_FillConvertCache(
			voice,
			buffer,
			SIZE_SRC_TO_DST(totalSize)
		);
	}

	/* Copy to decodeCache, finally. */
	siz = SAMPLES_FROM_SRC(totalSize - byteOffset);
	siz = SIZE_FROM_DST(FAudio_min(siz, samples));
	byteOffset = SIZE_SRC_TO_DST(byteOffset);
	FAudio_memcpy(
		decodeCache,
		voice->src.gstreamer->convertCache + byteOffset,
		siz
	);

	/* Increment pointer, decrement remaining sample count */
	decodeCache += siz / sizeof(float);
	samples -= SAMPLES_FROM_DST(siz);
	curBufferOffset += SAMPLES_FROM_DST(siz);

	/* If the cache isn't filled yet, keep decoding! */
	if (samples > 0)
	{
		goto decode;
	}

	#undef SIZE_FROM_SRC
	#undef SIZE_FROM_DST
	#undef SAMPLES_FROM_SRC
	#undef SAMPLES_FROM_DST
	#undef SIZE_SRC_TO_DST
	#undef SIZE_DST_TO_SRC

	LOG_FUNC_EXIT(voice->audio)
}

static uint8_t gstreamerWasInit = 0;

uint32_t FAudio_GSTREAMER_init(FAudioSourceVoice *pSourceVoice, uint32_t type)
{
	FAudioGSTREAMER *result;
	const char *codec;
	GstElement *decoder;
	GstElement *converter;
	GstCaps *caps;
	int version;
	GstBuffer *codec_data;
	size_t codec_data_size;
	uint8_t *extradata;
	uint8_t fakeextradata[16];

	LOG_FUNC_ENTER(pSourceVoice->audio)

	/* Init GStreamer statically. The docs tell us not to exit, so I guess
	 * we're supposed to just leak!
	 */
	if (!gstreamerWasInit)
	{
		/* Apparently they ask for this to leak... */
		gst_init(NULL, NULL);
		gstreamerWasInit = 1;
	}

	/* Match the format with the codec */
	switch (type)
	{
	#define GSTTYPE(fmt, dec, ver) \
		case FAUDIO_FORMAT_##fmt: codec = dec; version = ver; break;
	GSTTYPE(WMAUDIO2, "avdec_wmav2", 2)
	GSTTYPE(WMAUDIO3, "avdec_wmapro", 3)
	GSTTYPE(WMAUDIO_LOSSLESS, "avdec_wmalossless", 4)
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
	result->curBlock = ~0u;
	decoder = gst_element_factory_make("fluwmadec", NULL);
	if (decoder == NULL)
	{
		LOG_ERROR(
			pSourceVoice->audio,
			"Fluendo WMA Decoder not found! Falling back to %s...",
			codec
		)
		decoder = gst_element_factory_make(codec, NULL);
		if (decoder == NULL)
		{
			LOG_ERROR(
				pSourceVoice->audio,
				"%s is unsupported on this configuration!",
				codec
			)
			FAudio_free(result);
			LOG_FUNC_EXIT(pSourceVoice->audio)
			return FAUDIO_E_UNSUPPORTED_FORMAT;
		}
	}
	result->pipeline = gst_pipeline_new(NULL);
	g_assert(result->pipeline);
	result->src = gst_element_factory_make("appsrc", NULL);
	g_assert(result->src);
	converter = gst_element_factory_make("audioconvert", NULL);
	g_assert(converter);
	result->dst = gst_element_factory_make("appsink", NULL);
	g_assert(result->dst);

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
	g_assert(caps);
	gst_app_src_set_caps(GST_APP_SRC(result->src), caps);
	g_object_set(
		G_OBJECT(result->src),
		"stream-type",	0,
		"format",	GST_FORMAT_TIME,
		NULL
	);
	gst_caps_unref(caps);
	gst_buffer_unref(codec_data);

	/* Prepare the output format */
	caps = gst_caps_new_simple(
		"audio/x-raw",
		"format",	G_TYPE_STRING, gst_audio_format_to_string(GST_AUDIO_FORMAT_F32),
		"layout",	G_TYPE_STRING, "interleaved",
		NULL
	);
	g_assert(caps);
	gst_app_sink_set_caps(GST_APP_SINK(result->dst), caps);
	gst_caps_unref(caps);

	/* Compile the pipeline, finally. */
	gst_bin_add_many(
		GST_BIN(result->pipeline),
		result->src,
		decoder,
		converter,
		result->dst,
		NULL
	);
	g_assert(gst_element_link_many(
		result->src,
		decoder,
		converter,
		result->dst,
		NULL
	));

	pSourceVoice->src.gstreamer = result;
	pSourceVoice->src.decode = FAudio_INTERNAL_DecodeGSTREAMER;
	gst_element_set_state(result->pipeline, GST_STATE_PLAYING);
	LOG_FUNC_EXIT(pSourceVoice->audio)
	return 0;
}

void FAudio_GSTREAMER_free(FAudioSourceVoice *voice)
{
#if REMOVE_ME
	printf("INTENDED: %d ACTUAL: %d\n", intendedTotal / 2, actualTotal / 2);
#endif
	LOG_FUNC_ENTER(voice->audio)
	gst_element_set_state(voice->src.gstreamer->pipeline, GST_STATE_NULL);
	gst_object_unref(voice->src.gstreamer->pipeline);
	voice->audio->pFree(voice->src.gstreamer->convertCache);
	voice->audio->pFree(voice->src.gstreamer);
	LOG_FUNC_EXIT(voice->audio)
}

#else

extern int this_tu_is_empty;

#endif /* HAVE_GSTREAMER */

/* vim: set noexpandtab shiftwidth=8 tabstop=8: */
