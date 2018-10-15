#ifdef HAVE_FFMPEG

#include "FAudio_internal.h"
#include <libavcodec/avcodec.h>

typedef struct FAudioFFmpeg 
{
	AVCodecContext *av_ctx;
	AVFrame *av_frame;

	uint32_t encOffset;	/* current position in encoded stream (in bytes) */

	/* buffer used to decode the last frame */
	size_t paddingBytes;
	uint8_t *paddingBuffer;

	/* buffer to receive an entire decoded frame */
	uint32_t convertCapacity;
	uint32_t convertSamples;
	uint32_t convertOffset;
	float *convertCache;
} FAudioFFmpeg;

#include <stdio.h>
#define TRACE(msg,...)  fprintf(stderr, "TRACE: " msg, __VA_ARGS__)
#define WARN(msg,...)  fprintf(stderr, "WARN:  " msg, __VA_ARGS__)

uint32_t FAudio_FFMPEG_init(FAudioSourceVoice *pSourceVoice)
{
    AVCodecContext *av_ctx;
    AVFrame *av_frame;
	AVCodec *codec;

	pSourceVoice->src.decode = FAudio_INTERNAL_DecodeFFMPEG;

	/* initialize ffmpeg state */
	codec = avcodec_find_decoder(AV_CODEC_ID_WMAV2);
	if (!codec)
    {
	    return FAUDIO_E_UNSUPPORTED_FORMAT;
	}

	av_ctx = avcodec_alloc_context3(codec);
	if(!av_ctx)
    {
        return FAUDIO_E_UNSUPPORTED_FORMAT;
    }

	av_ctx->bit_rate = pSourceVoice->src.format->nAvgBytesPerSec * 8;
	av_ctx->channels = pSourceVoice->src.format->nChannels;
	av_ctx->sample_rate = pSourceVoice->src.format->nSamplesPerSec;
	av_ctx->block_align = pSourceVoice->src.format->nBlockAlign;
	av_ctx->bits_per_coded_sample = pSourceVoice->src.format->wBitsPerSample;
	av_ctx->extradata_size = pSourceVoice->src.format->cbSize;
	av_ctx->request_sample_fmt = AV_SAMPLE_FMT_FLT;

	/* pSourceVoice->src.format is actually pointing to a WAVEFORMATEXTENSIBLE struct, not just a WAVEFORMATEX struct.
	   That means there's always at least 22 bytes following the struct, I assume the WMA data is behind that.
	   XXX-JS Need to verify! */
	if (pSourceVoice->src.format->cbSize > 22)
    {
		av_ctx->extradata = (uint8_t *) FAudio_malloc(pSourceVoice->src.format->cbSize + AV_INPUT_BUFFER_PADDING_SIZE - 22);
		FAudio_memcpy(av_ctx->extradata, (&pSourceVoice->src.format->cbSize) + 23, pSourceVoice->src.format->cbSize - 22);
	}
    else
    {
		/* xWMA doesn't provide the extradata info that FFmpeg needs to
		 * decode WMA data, so we create some fake extradata. This is taken
		 * from <ffmpeg/libavformat/xwma.c>. */
		av_ctx->extradata_size = 6;
		av_ctx->extradata = (uint8_t *) FAudio_malloc(AV_INPUT_BUFFER_PADDING_SIZE);
        FAudio_zero(av_ctx->extradata, AV_INPUT_BUFFER_PADDING_SIZE);
		av_ctx->extradata[4] = 31;
	}

	if (avcodec_open2(av_ctx, codec, NULL) < 0)
    {
		FAudio_free(av_ctx->extradata);
		av_free(av_ctx);
        return FAUDIO_E_UNSUPPORTED_FORMAT;
	}

	av_frame = av_frame_alloc();
	if (!av_frame) {
		avcodec_close(av_ctx);
		FAudio_free(av_ctx->extradata);
		av_free(av_ctx);
        return FAUDIO_E_UNSUPPORTED_FORMAT;
	}

	if(av_ctx->sample_fmt != AV_SAMPLE_FMT_FLT && av_ctx->sample_fmt != AV_SAMPLE_FMT_FLTP)
    {
		FAudio_assert(0 && "Got non-float format!!!");
	}

	pSourceVoice->src.ffmpeg = (FAudioFFmpeg *) FAudio_malloc(sizeof(FAudioFFmpeg));
	FAudio_zero(pSourceVoice->src.ffmpeg, sizeof(FAudioFFmpeg));

	pSourceVoice->src.ffmpeg->av_ctx = av_ctx;
    pSourceVoice->src.ffmpeg->av_frame = av_frame;
    return 0;
}

void FAudio_FFMPEG_free(FAudioSourceVoice *voice)
{
	FAudioFFmpeg *ffmpeg = voice->src.ffmpeg;

	avcodec_close(ffmpeg->av_ctx);
	FAudio_free(ffmpeg->av_ctx->extradata);
	av_free(ffmpeg->av_ctx);

	FAudio_free(ffmpeg);
	voice->src.ffmpeg = NULL;
}

void FAudio_INTERNAL_ResizeConvertCache(FAudioVoice *voice, uint32_t samples)
{
	if (samples > voice->src.ffmpeg->convertCapacity)
	{
		voice->src.ffmpeg->convertCapacity = samples;
		voice->src.ffmpeg->convertCache = (float*) FAudio_realloc(
			voice->src.ffmpeg->convertCache,
			sizeof(float) * voice->src.ffmpeg->convertCapacity
		);
	}
}

void FAudio_INTERNAL_FillConvertCache(FAudioVoice *voice, FAudioBuffer *buffer)
{
	FAudioFFmpeg *ffmpeg = voice->src.ffmpeg;
	AVPacket avpkt = {0};
	int averr;
	uint32_t total_samples;

	avpkt.size = voice->src.format->nBlockAlign;
	avpkt.data = (unsigned char *) buffer->pAudioData + ffmpeg->encOffset;

	for(;;)
	{
		averr = avcodec_receive_frame(ffmpeg->av_ctx, ffmpeg->av_frame);
		if (averr == AVERROR(EAGAIN))
        {
			/* ffmpeg needs more data to decode */
			avpkt.pts = avpkt.dts = AV_NOPTS_VALUE;

            if (ffmpeg->encOffset >= buffer->AudioBytes)
            {
				/* no more data in this buffer */
				break;
            }

			if (ffmpeg->encOffset + avpkt.size + AV_INPUT_BUFFER_PADDING_SIZE > buffer->AudioBytes)
            {
				size_t remain = buffer->AudioBytes - ffmpeg->encOffset;
				/* Unfortunately, the FFmpeg API requires that a number of
				 * extra bytes must be available past the end of the buffer.
				 * The xaudio2 client probably hasn't done this, so we have to
				 * perform a copy near the end of the buffer. */
				TRACE("hitting end of buffer. copying %lu + %u bytes into %lu buffer\n",
						remain, AV_INPUT_BUFFER_PADDING_SIZE, ffmpeg->paddingBytes);

				if (ffmpeg->paddingBytes < remain + AV_INPUT_BUFFER_PADDING_SIZE)
                {
					ffmpeg->paddingBytes = remain + AV_INPUT_BUFFER_PADDING_SIZE;
					TRACE("buffer too small, expanding to %lu\n", ffmpeg->paddingBytes);
                    ffmpeg->paddingBuffer = (uint8_t *) FAudio_realloc(
						ffmpeg->paddingBuffer, 
						ffmpeg->paddingBytes
					);
				}
				FAudio_memcpy(ffmpeg->paddingBuffer, buffer->pAudioData + ffmpeg->encOffset, remain);
				FAudio_memset(ffmpeg->paddingBuffer + remain, 0, AV_INPUT_BUFFER_PADDING_SIZE);
				avpkt.data = ffmpeg->paddingBuffer;
			}

			averr = avcodec_send_packet(ffmpeg->av_ctx, &avpkt);
			if (averr)
            {
				WARN("avcodec_send_packet failed: %d\n", averr);
				break;
			}

			ffmpeg->encOffset += avpkt.size;
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

    total_samples = ffmpeg->av_frame->nb_samples * ffmpeg->av_ctx->channels;

	FAudio_INTERNAL_ResizeConvertCache(voice, total_samples);

	if (av_sample_fmt_is_planar(ffmpeg->av_ctx->sample_fmt))
	{
		int32_t s, c;
		uint8_t **src = ffmpeg->av_frame->data;
		uint32_t *dst = (uint32_t *) ffmpeg->convertCache;

		for(s = 0; s < ffmpeg->av_frame->nb_samples; ++s)
			for(c = 0; c < ffmpeg->av_ctx->channels; ++c)
				*dst++ = ((uint32_t*)(src[c]))[s];
	}
	else
	{
		FAudio_memcpy(ffmpeg->convertCache, ffmpeg->av_frame->data[0], total_samples * sizeof(float));
	}

	ffmpeg->convertSamples = total_samples;
	ffmpeg->convertOffset = 0;
}

void FAudio_INTERNAL_DecodeFFMPEG(
	FAudioVoice *voice,
	FAudioBuffer *buffer,
	uint32_t *samples,
	uint32_t end,
	float *decodeCache
) {
	FAudioFFmpeg *ffmpeg = voice->src.ffmpeg;
    uint32_t done = 0, available, todo;

	while (done < *samples) 
	{	
		available = (ffmpeg->convertSamples - ffmpeg->convertOffset) / voice->src.format->nChannels;

		if (available <= 0) 
		{
			FAudio_INTERNAL_FillConvertCache(voice, buffer);
			available = (ffmpeg->convertSamples - ffmpeg->convertOffset) / voice->src.format->nChannels;

			if (available <= 0)
			{
				break;
			}
		}

		todo = FAudio_min(available, *samples - done);
		FAudio_memcpy(decodeCache + (done * voice->src.format->nChannels), ffmpeg->convertCache + ffmpeg->convertOffset, todo * voice->src.format->nChannels * sizeof(float));
		done += todo;
		ffmpeg->convertOffset += todo * voice->src.format->nChannels;
	}

    *samples = done;
}

#endif