#ifdef HAVE_FFMPEG

#include "FAudio_internal.h"
#include <libavcodec/avcodec.h>

#include <stdio.h>

#define TRACE(msg,...)  fprintf(stderr, "TRACE: " msg, __VA_ARGS__)
#define WARN(msg,...)  fprintf(stderr, "WARN:  " msg, __VA_ARGS__)

uint32_t FAudio_FFMPEG_init(FAudioSourceVoice *pSourceVoice)
{
    AVCodecContext *conv_ctx;
    AVFrame *conv_frame;
	AVCodec *codec;

	pSourceVoice->src.decode = FAudio_INTERNAL_DecodeFFMPEG;

	/* initialize ffmpeg state */
	codec = avcodec_find_decoder(AV_CODEC_ID_WMAV2);
	if (!codec)
    {
	    return FAUDIO_E_UNSUPPORTED_FORMAT;
	}

	conv_ctx = avcodec_alloc_context3(codec);
	if(!conv_ctx)
    {
        return FAUDIO_E_UNSUPPORTED_FORMAT;
    }

	conv_ctx->bit_rate = pSourceVoice->src.format->nAvgBytesPerSec * 8;
	conv_ctx->channels = pSourceVoice->src.format->nChannels;
	conv_ctx->sample_rate = pSourceVoice->src.format->nSamplesPerSec;
	conv_ctx->block_align = pSourceVoice->src.format->nBlockAlign;
	conv_ctx->bits_per_coded_sample = pSourceVoice->src.format->wBitsPerSample;
	conv_ctx->extradata_size = pSourceVoice->src.format->cbSize;
	conv_ctx->request_sample_fmt = AV_SAMPLE_FMT_FLT;

	if (pSourceVoice->src.format->cbSize)
    {
		conv_ctx->extradata = (uint8_t *) FAudio_malloc(pSourceVoice->src.format->cbSize + AV_INPUT_BUFFER_PADDING_SIZE);
		FAudio_memcpy(conv_ctx->extradata, (&pSourceVoice->src.format->cbSize) + 1, pSourceVoice->src.format->cbSize);
	}
    else
    {
		/* xWMA doesn't provide the extradata info that FFmpeg needs to
		 * decode WMA data, so we create some fake extradata. This is taken
		 * from <ffmpeg/libavformat/xwma.c>. */
		conv_ctx->extradata_size = 6;
		conv_ctx->extradata = (uint8_t *) FAudio_malloc(AV_INPUT_BUFFER_PADDING_SIZE);
        FAudio_zero(conv_ctx->extradata, AV_INPUT_BUFFER_PADDING_SIZE);
		conv_ctx->extradata[4] = 31;
	}

	if (avcodec_open2(conv_ctx, codec, NULL) < 0)
    {
		FAudio_free(pSourceVoice->src.conv_ctx->extradata);
		av_free(conv_ctx);
        return FAUDIO_E_UNSUPPORTED_FORMAT;
	}

	conv_frame = av_frame_alloc();
	if (!conv_frame) {
		avcodec_close(conv_ctx);
		FAudio_free(conv_ctx->extradata);
		av_free(conv_ctx);
        return FAUDIO_E_UNSUPPORTED_FORMAT;
	}

	if(conv_ctx->sample_fmt != AV_SAMPLE_FMT_FLT && conv_ctx->sample_fmt != AV_SAMPLE_FMT_FLTP)
    {
		FAudio_assert(0 && "Got non-float format!!!");
	}

	pSourceVoice->src.conv_ctx = conv_ctx;
    pSourceVoice->src.conv_frame = conv_frame;
    pSourceVoice->src.convert_bytes = 0;
    pSourceVoice->src.convert_buf = NULL;
    return 0;
}

void FAudio_INTERNAL_ResizeConvertCache(FAudioVoice *voice, uint32_t samples)
{
	if (samples > voice->src.convertCapacity)
	{
		voice->src.convertCapacity = samples;
		voice->src.convertCache = (float*) FAudio_realloc(
			voice->src.convertCache,
			sizeof(float) * voice->src.convertCapacity
		);
	}
}

void FAudio_INTERNAL_FillConvertCache(FAudioVoice *voice, FAudioBuffer *buffer)
{
	AVPacket avpkt = {0};
	int averr;
	uint32_t total_samples;

	avpkt.size = voice->src.format->nBlockAlign;
	avpkt.data = (unsigned char *) buffer->pAudioData + voice->src.curBufferOffset;

	for(;;)
	{
		averr = avcodec_receive_frame(voice->src.conv_ctx, voice->src.conv_frame);
		if (averr == AVERROR(EAGAIN))
        {
			/* ffmpeg needs more data to decode */
			avpkt.pts = avpkt.dts = AV_NOPTS_VALUE;

            if (voice->src.curBufferOffset >= buffer->AudioBytes)
            {
				/* no more data in this buffer */
				break;
            }

			if (voice->src.curBufferOffset + avpkt.size + AV_INPUT_BUFFER_PADDING_SIZE > buffer->AudioBytes)
            {
				size_t remain = buffer->AudioBytes - voice->src.curBufferOffset;
				/* Unfortunately, the FFmpeg API requires that a number of
				 * extra bytes must be available past the end of the buffer.
				 * The xaudio2 client probably hasn't done this, so we have to
				 * perform a copy near the end of the buffer. */
				TRACE("hitting end of buffer. copying %lu + %u bytes into %lu buffer\n",
						remain, AV_INPUT_BUFFER_PADDING_SIZE, voice->src.convert_bytes);

				if(voice->src.convert_bytes < remain + AV_INPUT_BUFFER_PADDING_SIZE)
                {
					voice->src.convert_bytes = remain + AV_INPUT_BUFFER_PADDING_SIZE;
					TRACE("buffer too small, expanding to %lu\n", voice->src.convert_bytes);
                    voice->src.convert_buf = (uint8_t *) FAudio_realloc(voice->src.convert_buf, voice->src.convert_bytes);
				}
				FAudio_memcpy(voice->src.convert_buf, buffer->pAudioData + voice->src.curBufferOffset, remain);
				FAudio_memset(voice->src.convert_buf + remain, 0, AV_INPUT_BUFFER_PADDING_SIZE);
				avpkt.data = voice->src.convert_buf;
			}

			averr = avcodec_send_packet(voice->src.conv_ctx, &avpkt);
			if (averr)
            {
				WARN("avcodec_send_packet failed: %d\n", averr);
				break;
			}

			voice->src.curBufferOffset += avpkt.size;
			avpkt.data += avpkt.size;

			/* data sent, try receive again */
			continue;
		}

		if (averr) 
        {
            WARN("avcodec_receive_frame failed: %d\n", averr);
            return; 
		}
		else
		{
			break;
		}
	}

    total_samples = voice->src.conv_frame->nb_samples * voice->src.conv_ctx->channels;

	FAudio_INTERNAL_ResizeConvertCache(voice, total_samples);

	if (av_sample_fmt_is_planar(voice->src.conv_ctx->sample_fmt))
	{
		int32_t s, c;
		uint8_t **src = voice->src.conv_frame->data;
		uint32_t *dst = (uint32_t *) voice->src.convertCache;

		for(s = 0; s < voice->src.conv_frame->nb_samples; ++s)
			for(c = 0; c < voice->src.conv_ctx->channels; ++c)
				*dst++ = ((uint32_t*)(src[c]))[s];
	}
	else
	{
		FAudio_memcpy(voice->src.convertCache, voice->src.conv_frame->data[0], total_samples * sizeof(float));
	}

	voice->src.convertSamples = total_samples;
	voice->src.convertOffset = 0;
}

void FAudio_INTERNAL_DecodeFFMPEG(
	FAudioVoice *voice,
	FAudioBuffer *buffer,
	uint32_t *samples,
	uint32_t end,
	float *decodeCache
) {
    uint32_t done = 0, available, todo;

	while (done < *samples) 
	{	
		available = (voice->src.convertSamples - voice->src.convertOffset) / voice->src.format->nChannels;

		if (available <= 0) 
		{
			FAudio_INTERNAL_FillConvertCache(voice, buffer);
			available = (voice->src.convertSamples - voice->src.convertOffset) / voice->src.format->nChannels;

			if (available <= 0)
			{
				break;
			}
		}

		todo = FAudio_min(available, *samples - done);
		FAudio_memcpy(decodeCache + (done * voice->src.format->nChannels), voice->src.convertCache + voice->src.convertOffset, todo * voice->src.format->nChannels * sizeof(float));
		done += todo;
		voice->src.convertOffset += todo * voice->src.format->nChannels;
	}

    *samples = done;
}

#endif