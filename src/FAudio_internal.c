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

#include "FAudio_internal.h"

/* Resampling */

/* Okay, so here's what all this fixed-point goo is for:
 *
 * Inevitably you're going to run into weird sample rates,
 * both from WaveBank data and from pitch shifting changes.
 *
 * How we deal with this is by calculating a fixed "step"
 * value that steps from sample to sample at the speed needed
 * to get the correct output sample rate, and the offset
 * is stored as separate integer and fraction values.
 *
 * This allows us to do weird fractional steps between samples,
 * while at the same time not letting it drift off into death
 * thanks to floating point madness.
 *
 * Steps are stored in fixed-point with 32 bits for the fraction:
 *
 * 00000000000000000000000000000000 00000000000000000000000000000000
 * ^ Integer block (32)             ^ Fraction block (32)
 *
 * For example, to get 1.5:
 * 00000000000000000000000000000001 10000000000000000000000000000000
 *
 * The Integer block works exactly like you'd expect.
 * The Fraction block is divided by the Integer's "One" value.
 * So, the above Fraction represented visually...
 *   1 << 32
 *   -------
 *   1 << 31
 * ... which, simplified, is...
 *   1 << 1
 *   ------
 *   1 << 0
 * ... in other words, 2 / 1, or 1.5.
 */
#define FIXED_PRECISION		32
#define FIXED_ONE		(1LL << FIXED_PRECISION)

/* Quick way to drop parts */
#define FIXED_FRACTION_MASK	(FIXED_ONE - 1)
#define FIXED_INTEGER_MASK	~FIXED_FRACTION_MASK

/* Helper macros to convert fixed to float */
#define DOUBLE_TO_FIXED(dbl) \
	((uint64_t) (dbl * FIXED_ONE + 0.5))
#define FIXED_TO_DOUBLE(fxd) ( \
	(double) (fxd >> FIXED_PRECISION) + /* Integer part */ \
	((fxd & FIXED_FRACTION_MASK) * (1.0 / FIXED_ONE)) /* Fraction part */ \
)

uint32_t FAudio_INTERNAL_DecodeBuffers(
	FAudioSourceVoice *voice,
	uint64_t *toDecode
) {
	uint32_t i, end, endRead, decoding, decoded = 0, resetOffset = 0;
	FAudioBuffer *buffer = &voice->src.bufferList->buffer;

	/* ... FIXME: I keep going past the buffer so fuck it */
	*toDecode += EXTRA_DECODE_PADDING;

	/* This should never go past the max ratio size */
	FAudio_assert(*toDecode <= voice->src.decodeSamples);

	while (decoded < *toDecode && buffer != NULL)
	{
		decoding = *toDecode - decoded;

		/* Start-of-buffer behavior */
		if (	voice->src.curBufferOffset == buffer->PlayBegin &&
			voice->src.callback != NULL &&
			voice->src.callback->OnBufferStart != NULL	)
		{
			voice->src.callback->OnBufferStart(
				voice->src.callback,
				buffer->pContext
			);
		}

		/* Check for end-of-buffer */
		end = (buffer->LoopCount > 0 && buffer->LoopLength > 0) ?
			buffer->LoopLength :
			buffer->PlayLength;
		endRead = FAudio_min(
			end - voice->src.curBufferOffset,
			decoding
		);

		/* Decode... */
		voice->src.decode(
			buffer,
			voice->src.curBufferOffset,
			voice->src.decodeCache[0] + decoded,
			voice->src.decodeCache[1] + decoded,
			endRead,
			&voice->src.format
		);

		/* End-of-buffer behavior */
		if (endRead < decoding)
		{
			resetOffset += endRead;
			if (buffer->LoopCount > 0)
			{
				voice->src.curBufferOffset = buffer->LoopBegin;
				if (buffer->LoopCount < 0xFF)
				{
					buffer->LoopCount -= 1;
				}
				if (	voice->src.callback != NULL &&
					voice->src.callback->OnLoopEnd != NULL	)
				{
					voice->src.callback->OnLoopEnd(
						voice->src.callback,
						buffer->pContext
					);
				}
			}
			else
			{
				/* For EOS we can stop storing fraction offsets */
				if (buffer->Flags & FAUDIO_END_OF_STREAM)
				{
					voice->src.curBufferOffsetDec = 0;
				}

				/* Callbacks */
				if (voice->src.callback != NULL)
				{
					if (voice->src.callback->OnBufferEnd != NULL)
					{
						voice->src.callback->OnBufferEnd(
							voice->src.callback,
							buffer->pContext
						);
					}
					if (	buffer->Flags & FAUDIO_END_OF_STREAM &&
						voice->src.callback->OnStreamEnd != NULL	)
					{
						voice->src.callback->OnStreamEnd(
							voice->src.callback
						);
					}
				}

				/* Change active buffer, delete finished buffer */
				voice->src.bufferList = voice->src.bufferList->next;
				FAudio_free(buffer);
				if (voice->src.bufferList != NULL)
				{
					buffer = &voice->src.bufferList->buffer;
					voice->src.curBufferOffset = buffer->PlayBegin;
				}
				else
				{
					buffer = NULL;

					/* FIXME: I keep going past the buffer so fuck it */
					for (i = 0; i < voice->src.format.nChannels; i += 1)
					{
						FAudio_zero(
							voice->src.decodeCache[i] + (decoded + endRead),
							(decoding - endRead) * sizeof(float)
						);
					}
				}
			}
		}

		/* Finally. */
		decoded += endRead;
	}

	/* ... FIXME: I keep going past the buffer so fuck it */
	*toDecode = decoded - EXTRA_DECODE_PADDING;
	return resetOffset;
}

void FAudio_INTERNAL_ResamplePCM(
	FAudioSourceVoice *voice,
	float **resampleCache,
	uint64_t toResample
) {
	/* Linear Resampler */
	uint32_t i, j;
	float *dCache[2] = { voice->src.decodeCache[0], voice->src.decodeCache[1] };
	uint64_t cur = voice->src.resampleOffset & FIXED_FRACTION_MASK;
	for (i = 0; i < toResample; i += 1)
	{
		for (j = 0; j < voice->src.format.nChannels; j += 1)
		{
			/* lerp, then convert to float value */
			*resampleCache[j]++ = (float) (
				dCache[j][0] +
				(dCache[j][1] - dCache[j][0]) *
				FIXED_TO_DOUBLE(cur)
			);
		}

		/* Increment fraction offset by the stepping value */
		voice->src.resampleOffset += voice->src.resampleStep;
		cur += voice->src.resampleStep;

		/* Only increment the sample offset by integer values.
		 * Sometimes this will be 0 until cur accumulates
		 * enough steps, especially for "slow" rates.
		 */
		for (j = 0; j < voice->src.format.nChannels; j += 1)
		{
			dCache[j] += cur >> FIXED_PRECISION;
		}

		/* Now that any integer has been added, drop it.
		 * The offset pointer will preserve the total.
		 */
		cur &= FIXED_FRACTION_MASK;
	}
}

void FAudio_INTERNAL_MixSource(FAudioSourceVoice *voice)
{
	/* Iterators */
	uint32_t i, j, co, ci;
	/* Decode/Resample variables */
	uint64_t toDecode;
	uint64_t toResample;
	uint32_t resetOffset;
	float *resampleCache[2];
	/* Output mix variables */
	float *stream;
	uint32_t mixed;
	uint32_t oChan;
	FAudioVoice *out;

	/* Calculate the resample stepping value */
	if (voice->src.resampleFreqRatio != voice->src.freqRatio)
	{
		out = (voice->sends.SendCount == 0) ?
			voice->audio->master : /* Barf */
			voice->sends.pSends->pOutputVoice;
		const uint32_t outputRate = (out->type == FAUDIO_VOICE_MASTER) ?
			out->master.inputSampleRate :
			out->mix.inputSampleRate;
		const double stepd = (
			voice->src.freqRatio *
			(double) voice->src.format.nSamplesPerSec /
			(double) outputRate
		);
		voice->src.resampleStep = DOUBLE_TO_FIXED(stepd);
		voice->src.resampleFreqRatio = voice->src.freqRatio;
	}

	/* Last call for buffer data! */
	if (	voice->src.callback != NULL &&
		voice->src.callback->OnVoiceProcessingPassStart != NULL)
	{
		voice->src.callback->OnVoiceProcessingPassStart(
			voice->src.callback,
			voice->src.decodeSamples * sizeof(int16_t)
		);
	}

	/* Nothing to do? */
	if (voice->src.bufferList == NULL)
	{
		goto end;
	}

	mixed = 0;
	resampleCache[0] = voice->src.outputResampleCache;
	resampleCache[1] = voice->src.outputResampleCache + voice->src.outputSamples;
	while (mixed < voice->src.outputSamples && voice->src.bufferList != NULL)
	{

		/* Base decode size, int to fixed... */
		toDecode = (voice->src.outputSamples - mixed) * voice->src.resampleStep;
		/* ... rounded up based on current offset... */
		toDecode += voice->src.curBufferOffsetDec + FIXED_FRACTION_MASK;
		/* ... fixed to int, truncating extra fraction from rounding. */
		toDecode >>= FIXED_PRECISION;

		/* Decode... */
		resetOffset = FAudio_INTERNAL_DecodeBuffers(voice, &toDecode);

		/* int to fixed... */
		toResample = toDecode << FIXED_PRECISION;
		/* ... round back down based on current offset... */
		toResample -= voice->src.curBufferOffsetDec;
		/* ... undo step size, fixed to int. */
		toResample /= voice->src.resampleStep;
		/* FIXME: I feel like this should be an assert but I suck */
		toResample = FAudio_min(toResample, voice->src.outputSamples - mixed);

		/* Resample... */
		if (voice->src.resampleStep == FIXED_ONE)
		{
			/* Actually, just copy directly... */
			for (i = 0; i < voice->src.format.nChannels; i += 1)
			{
				FAudio_memcpy(
					resampleCache[i],
					voice->src.decodeCache[i],
					sizeof(float) * toResample
				);
				resampleCache[i] += toResample;
			}
		}
		else
		{
			FAudio_INTERNAL_ResamplePCM(voice, resampleCache, toResample);
		}

		/* Update buffer offsets */
		if (voice->src.bufferList != NULL)
		{
			/* Increment fixed offset by resample size, int to fixed... */
			voice->src.curBufferOffsetDec += toResample * voice->src.resampleStep;
			/* ... increment int offset by fixed offset, may be 0! */
			voice->src.curBufferOffset += voice->src.curBufferOffsetDec >> FIXED_PRECISION;
			/* ... subtract any increment not applicable to our possibly new buffer... */
			voice->src.curBufferOffset -= FAudio_min(resetOffset, voice->src.curBufferOffset);
			/* ... chop off any ints we got from the above increment */
			voice->src.curBufferOffsetDec &= FIXED_FRACTION_MASK;
		}
		else
		{
			voice->src.curBufferOffsetDec = 0;
			voice->src.curBufferOffset = 0;
		}

		/* Finally. */
		mixed += toResample;
	}
	if (mixed == 0)
	{
		goto end;
	}

	/* Nowhere to send it? Just skip resampling...*/
	if (voice->sends.SendCount == 0)
	{
		goto end;
	}

	/* TODO: Effects, filters */

	/* Send float cache to sends */
	resampleCache[0] = voice->src.outputResampleCache;
	resampleCache[1] = voice->src.outputResampleCache + voice->src.outputSamples;
	for (i = 0; i < voice->sends.SendCount; i += 1)
	{
		out = voice->sends.pSends[i].pOutputVoice;
		if (out->type == FAUDIO_VOICE_MASTER)
		{
			stream = out->master.output;
			oChan = out->master.inputChannels;
		}
		else
		{
			stream = out->mix.inputCache;
			oChan = out->mix.inputChannels;
		}

		for (j = 0; j < mixed; j += 1)
		for (co = 0; co < oChan; co += 1)
		for (ci = 0; ci < voice->src.format.nChannels; ci += 1)
		{
			/* Include source/channel volumes in the mix! */
			stream[j * oChan + co] = FAudio_clamp(
				stream[j * oChan + co] + (
					resampleCache[ci][j] *
					voice->channelVolume[ci] *
					voice->volume *
					voice->sendCoefficients[i][
						co * voice->src.format.nChannels + ci
					]
				),
				-FAUDIO_MAX_VOLUME_LEVEL,
				FAUDIO_MAX_VOLUME_LEVEL
			);
		}
	}

	/* Done, finally. */
end:
	if (	voice->src.callback != NULL &&
		voice->src.callback->OnVoiceProcessingPassEnd != NULL)
	{
		voice->src.callback->OnVoiceProcessingPassEnd(
			voice->src.callback
		);
	}
}

void FAudio_INTERNAL_MixSubmix(FAudioSubmixVoice *voice)
{
	uint32_t i, j, co, ci;
	float *stream;
	uint32_t oChan;
	FAudioVoice *out;
	uint32_t resampled;

	/* Nothing to do? */
	if (voice->sends.SendCount == 0)
	{
		goto end;
	}

	/* Resample (if necessary) */
	resampled = FAudio_PlatformResample(
		voice->mix.resampler,
		voice->mix.inputCache,
		voice->mix.inputSamples,
		voice->mix.outputResampleCache,
		voice->mix.outputSamples
	);

	/* Submix volumes are applied _before_ effects/filters, blech! */
	resampled /= voice->mix.inputChannels;
	for (i = 0; i < resampled; i += 1)
	for (ci = 0; ci < voice->mix.inputChannels; ci += 1)
	{
		/* FIXME: Clip volume? */
		voice->mix.outputResampleCache[
			i * voice->mix.inputChannels + ci
		] = (
			voice->mix.outputResampleCache[
				i * voice->mix.inputChannels + ci
			] *
			voice->channelVolume[ci] *
			voice->volume
		);
	}
	resampled *= voice->mix.inputChannels;

	/* TODO: Deinterleave? Effects, filters */

	/* Send float cache to sends */
	resampled /= voice->mix.inputChannels;
	for (i = 0; i < voice->sends.SendCount; i += 1)
	{
		out = voice->sends.pSends[i].pOutputVoice;
		if (out->type == FAUDIO_VOICE_MASTER)
		{
			stream = out->master.output;
			oChan = out->master.inputChannels;
		}
		else
		{
			stream = out->mix.inputCache;
			oChan = out->mix.inputChannels;
		}

		for (j = 0; j < resampled; j += 1)
		for (co = 0; co < oChan; co += 1)
		for (ci = 0; ci < voice->mix.inputChannels; ci += 1)
		{
			stream[j * oChan + co] = FAudio_clamp(
				stream[j * oChan + co] + (
					voice->mix.outputResampleCache[
						j * voice->mix.inputChannels + ci
					] *
					voice->sendCoefficients[i][
						co * voice->mix.inputChannels + ci
					]
				),
				-FAUDIO_MAX_VOLUME_LEVEL,
				FAUDIO_MAX_VOLUME_LEVEL
			);
		}
	}

	/* Zero this at the end, for the next update */
end:
	FAudio_zero(
		voice->mix.inputCache,
		sizeof(float) * voice->mix.inputSamples
	);
}

void FAudio_INTERNAL_UpdateEngine(FAudio *audio, float *output)
{
	uint32_t i, totalSamples;
	FAudioSourceVoiceEntry *source;
	FAudioSubmixVoiceEntry *submix;
	FAudioEngineCallbackEntry *callback;

	if (!audio->active)
	{
		return;
	}

	/* ProcessingPassStart callbacks */
	callback = audio->callbacks;
	while (callback != NULL)
	{
		if (callback->callback->OnProcessingPassStart != NULL)
		{
			callback->callback->OnProcessingPassStart(
				callback->callback
			);
		}
		callback = callback->next;
	}

	/* Writes to master will directly write to output */
	audio->master->master.output = output;

	/* Mix sources */
	source = audio->sources;
	while (source != NULL)
	{
		if (source->voice->src.active)
		{
			FAudio_INTERNAL_MixSource(source->voice);
		}
		source = source->next;
	}

	/* Mix submixes, ordered by processing stage */
	for (i = 0; i < audio->submixStages; i += 1)
	{
		submix = audio->submixes;
		while (submix != NULL)
		{
			if (submix->voice->mix.processingStage == i)
			{
				FAudio_INTERNAL_MixSubmix(submix->voice);
			}
			submix = submix->next;
		}
	}

	/* Apply master volume */
	totalSamples = audio->updateSize * audio->master->master.inputChannels;
	for (i = 0; i < totalSamples; i += 1)
	{
		output[i] = FAudio_clamp(
			output[i] * audio->master->volume,
			-FAUDIO_MAX_VOLUME_LEVEL,
			FAUDIO_MAX_VOLUME_LEVEL
		);
	}

	/* TODO: Master effect chain processing */

	/* OnProcessingPassEnd callbacks */
	callback = audio->callbacks;
	while (callback != NULL)
	{
		if (callback->callback->OnProcessingPassEnd != NULL)
		{
			callback->callback->OnProcessingPassEnd(
				callback->callback
			);
		}
		callback = callback->next;
	}
}

/* 8-bit PCM Decoding */

void FAudio_INTERNAL_DecodeMonoPCM8(
	FAudioBuffer *buffer,
	uint32_t curOffset,
	float *decodeCache,
	float *UNUSED1,
	uint32_t samples,
	FAudioWaveFormatEx *UNUSED2
) {
	uint32_t i;
	const int8_t *buf = ((int8_t*) buffer->pAudioData) + (
		buffer->PlayBegin + curOffset
	);
	for (i = 0; i < samples; i += 1)
	{
		*decodeCache++ = *buf++ / 128.0f;
	}
}

void FAudio_INTERNAL_DecodeStereoPCM8(
	FAudioBuffer *buffer,
	uint32_t curOffset,
	float *decodeCacheL,
	float *decodeCacheR,
	uint32_t samples,
	FAudioWaveFormatEx *UNUSED
) {
	uint32_t i;
	const int8_t *buf = ((int8_t*) buffer->pAudioData) + (
		(buffer->PlayBegin + curOffset) * 2
	);
	for (i = 0; i < samples; i += 1)
	{
		*decodeCacheL++ = *buf++ / 128.0f;
		*decodeCacheR++ = *buf++ / 128.0f;
	}
}

/* 16-bit PCM Decoding */

void FAudio_INTERNAL_DecodeMonoPCM16(
	FAudioBuffer *buffer,
	uint32_t curOffset,
	float *decodeCache,
	float *UNUSED1,
	uint32_t samples,
	FAudioWaveFormatEx *UNUSED2
) {
	uint32_t i;
	const int16_t *buf = ((int16_t*) buffer->pAudioData) + (
		buffer->PlayBegin + curOffset
	);
	for (i = 0; i < samples; i += 1)
	{
		*decodeCache++ = *buf++ / 32768.0f;
	}
}

void FAudio_INTERNAL_DecodeStereoPCM16(
	FAudioBuffer *buffer,
	uint32_t curOffset,
	float *decodeCacheL,
	float *decodeCacheR,
	uint32_t samples,
	FAudioWaveFormatEx *UNUSED
) {
	uint32_t i;
	const int16_t *buf = ((int16_t*) buffer->pAudioData) + (
		(buffer->PlayBegin + curOffset) * 2
	);
	for (i = 0; i < samples; i += 1)
	{
		*decodeCacheL++ = *buf++ / 32768.0f;
		*decodeCacheR++ = *buf++ / 32768.0f;
	}
}

/* 32-bit float PCM Decoding */

void FAudio_INTERNAL_DecodeMonoPCM32F(
	FAudioBuffer *buffer,
	uint32_t curOffset,
	float *decodeCache,
	float *UNUSED1,
	uint32_t samples,
	FAudioWaveFormatEx *UNUSED2
) {
	const float *buf = ((float*) buffer->pAudioData) + (
		buffer->PlayBegin + curOffset
	);
	FAudio_memcpy(
		decodeCache,
		buf,
		sizeof(float) * samples
	);
}

void FAudio_INTERNAL_DecodeStereoPCM32F(
	FAudioBuffer *buffer,
	uint32_t curOffset,
	float *decodeCacheL,
	float *decodeCacheR,
	uint32_t samples,
	FAudioWaveFormatEx *UNUSED
) {
	uint32_t i;
	const float *buf = ((float*) buffer->pAudioData) + (
		(buffer->PlayBegin + curOffset) * 2
	);
	for (i = 0; i < samples; i += 1)
	{
		*decodeCacheL++ = *buf++;
		*decodeCacheR++ = *buf++;
	}
}

/* MSADPCM Decoding */

static inline int16_t FAudio_INTERNAL_ParseNibble(
	uint8_t nibble,
	uint8_t predictor,
	int16_t *delta,
	int16_t *sample1,
	int16_t *sample2
) {
	static const int32_t AdaptionTable[16] =
	{
		230, 230, 230, 230, 307, 409, 512, 614,
		768, 614, 512, 409, 307, 230, 230, 230
	};
	static const int32_t AdaptCoeff_1[7] =
	{
		256, 512, 0, 192, 240, 460, 392
	};
	static const int32_t AdaptCoeff_2[7] =
	{
		0, -256, 0, 64, 0, -208, -232
	};

	int8_t signedNibble;
	int32_t sampleInt;
	int16_t sample;

	signedNibble = (int8_t) nibble;
	if (signedNibble & 0x08)
	{
		signedNibble -= 0x10;
	}

	sampleInt = (
		(*sample1 * AdaptCoeff_1[predictor]) +
		(*sample2 * AdaptCoeff_2[predictor])
	) / 256;
	sampleInt += signedNibble * (*delta);
	sample = FAudio_clamp(sampleInt, -32768, 32767);

	*sample2 = *sample1;
	*sample1 = sample;
	*delta = (int16_t) (AdaptionTable[nibble] * (int32_t) (*delta) / 256);
	if (*delta < 16)
	{
		*delta = 16;
	}
	return sample;
}

#define READ(item, type) \
	item = *((type*) *buf); \
	*buf += sizeof(type);

static inline void FAudio_INTERNAL_DecodeMonoMSADPCMBlock(
	uint8_t **buf,
	int16_t *blockCache,
	uint32_t align
) {
	uint32_t i;

	/* Temp storage for ADPCM blocks */
	uint8_t predictor;
	int16_t delta;
	int16_t sample1;
	int16_t sample2;
	uint8_t nibbles[255]; /* Max align size */

	/* Preamble */
	READ(predictor, uint8_t)
	READ(delta, int16_t)
	READ(sample1, int16_t)
	READ(sample2, int16_t)
	align -= 7;
	FAudio_memcpy(
		nibbles,
		*buf,
		align
	);
	*buf += align;

	/* Samples */
	*blockCache++ = sample1;
	*blockCache++ = sample2;
	for (i = 0; i < align; i += 1)
	{
		*blockCache++ = FAudio_INTERNAL_ParseNibble(
			nibbles[i] >> 4,
			predictor,
			&delta,
			&sample1,
			&sample2
		);
		*blockCache++ = FAudio_INTERNAL_ParseNibble(
			nibbles[i] & 0x0F,
			predictor,
			&delta,
			&sample1,
			&sample2
		);
	}
}

static inline void FAudio_INTERNAL_DecodeStereoMSADPCMBlock(
	uint8_t **buf,
	int16_t *blockCache,
	uint32_t align
) {
	uint32_t i;

	/* Temp storage for ADPCM blocks */
	uint8_t l_predictor;
	uint8_t r_predictor;
	int16_t l_delta;
	int16_t r_delta;
	int16_t l_sample1;
	int16_t r_sample1;
	int16_t l_sample2;
	int16_t r_sample2;
	uint8_t nibbles[510]; /* Max align size */

	/* Preamble */
	READ(l_predictor, uint8_t)
	READ(r_predictor, uint8_t)
	READ(l_delta, int16_t)
	READ(r_delta, int16_t)
	READ(l_sample1, int16_t)
	READ(r_sample1, int16_t)
	READ(l_sample2, int16_t)
	READ(r_sample2, int16_t)
	align -= 14;
	FAudio_memcpy(
		nibbles,
		*buf,
		align
	);
	*buf += align;

	/* Samples */
	*blockCache++ = l_sample2;
	*blockCache++ = r_sample2;
	*blockCache++ = l_sample1;
	*blockCache++ = r_sample1;
	for (i = 0; i < align; i += 1)
	{
		*blockCache++ = FAudio_INTERNAL_ParseNibble(
			nibbles[i] >> 4,
			l_predictor,
			&l_delta,
			&l_sample1,
			&l_sample2
		);
		*blockCache++ = FAudio_INTERNAL_ParseNibble(
			nibbles[i] & 0x0F,
			r_predictor,
			&r_delta,
			&r_sample1,
			&r_sample2
		);
	}
}

#undef READ

void FAudio_INTERNAL_DecodeMonoMSADPCM(
	FAudioBuffer *buffer,
	uint32_t curOffset,
	float *decodeCache,
	float *UNUSED,
	uint32_t samples,
	FAudioWaveFormatEx *format
) {
	/* Loop variables */
	uint32_t i, off, copy;

	/* Read pointers */
	uint8_t *buf;
	int32_t midOffset;

	/* PCM block cache */
	int16_t blockCache[512]; /* Max block size */

	/* Block size */
	uint32_t bsize = (format->nBlockAlign - 6) * 2;

	/* Where are we starting? */
	buf = (uint8_t*) buffer->pAudioData + (
		(curOffset / bsize) *
		format->nBlockAlign
	);

	/* Are we starting in the middle? */
	midOffset = (curOffset % bsize);

	/* Read in each block directly to the decode cache */
	while (samples > 0)
	{
		copy = FAudio_min(samples, bsize - midOffset);
		FAudio_INTERNAL_DecodeMonoMSADPCMBlock(
			&buf,
			blockCache,
			format->nBlockAlign
		);
		for (i = 0, off = midOffset; i < copy; i += 1)
		{
			*decodeCache++ = blockCache[off++] / 32768.0f;
		}
		samples -= copy;
		midOffset = 0;
	}
}

void FAudio_INTERNAL_DecodeStereoMSADPCM(
	FAudioBuffer *buffer,
	uint32_t curOffset,
	float *decodeCacheL,
	float *decodeCacheR,
	uint32_t samples,
	FAudioWaveFormatEx *format
) {
	/* Loop variables */
	uint32_t i, off, copy;

	/* Read pointers */
	uint8_t *buf;
	int32_t midOffset;

	/* PCM block cache */
	int16_t blockCache[1024]; /* Max block size */

	/* Align, block size */
	uint32_t bsize = ((format->nBlockAlign / 2) - 6) * 2;

	/* Where are we starting? */
	buf = (uint8_t*) buffer->pAudioData + (
		(curOffset / bsize) *
		format->nBlockAlign
	);

	/* Are we starting in the middle? */
	midOffset = (curOffset % bsize);

	/* Read in each block directly to the decode cache */
	while (samples > 0)
	{
		copy = FAudio_min(samples, bsize - midOffset);
		FAudio_INTERNAL_DecodeStereoMSADPCMBlock(
			&buf,
			blockCache,
			format->nBlockAlign
		);
		for (i = 0, off = midOffset * 2; i < copy; i += 1)
		{
			*decodeCacheL++ = blockCache[off++] / 32768.0f;
			*decodeCacheR++ = blockCache[off++] / 32768.0f;
		}
		samples -= copy;
		midOffset = 0;
	}
}
