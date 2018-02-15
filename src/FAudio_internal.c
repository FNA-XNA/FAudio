/* FAudio - XAudio Reimplementation for FNA
 *
 * Copyright (c) 2018 Ethan Lee.
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
 *   1 << 31
 *   -------
 *   1 << 32
 * ... which, simplified, is...
 *   1 << 0
 *   ------
 *   1 << 1
 * ... in other words, 1 / 2, or 0.5.
 */
#define FIXED_PRECISION		32
#define FIXED_ONE		(1L << FIXED_PRECISION)

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

void FAudio_INTERNAL_MixSource(FAudioSourceVoice *voice)
{
	uint32_t i, j, co, ci;
	float *stream;
	uint32_t oChan;
	FAudioVoice *out;
	FAudioBuffer *buffer;
	uint64_t toDecode, decoded, resampled;
	uint32_t samples, end, endRead;
	int16_t *decodeCache;
	uint64_t cur;

	/* Nothing to do? */
	if (voice->src.bufferList == 0)
	{
		return;
	}

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

	/* The easy part is just multiplying the final output size with the step
	 * to get the "real" buffer size...
	 */
	toDecode = (
		voice->audio->updateSize *
		voice->src.format.nChannels *
		voice->src.resampleStep
	);

	/* ... but we also need to ceil() to get the extra sample needed for
	 * interpolating past the "end" of the unresampled buffer.
	 */
	toDecode = (
		(toDecode >> FIXED_PRECISION) +
		((toDecode & FIXED_FRACTION_MASK) > 0) +
		((voice->src.resampleOffset & FIXED_FRACTION_MASK) > 0)
	);

	/* Also, stereo size MUST be a multiple of two! */
	if (voice->src.format.nChannels == 2)
	{
		toDecode = (toDecode + 1) & ~1;
	}

	/* Last call for buffer data! */
	if (	voice->src.callback != NULL &&
		voice->src.callback->OnVoiceProcessingPassStart != NULL)
	{
		voice->src.callback->OnVoiceProcessingPassStart(
			voice->src.callback,
			toDecode * sizeof(int16_t)
		);
	}

	/* Decode... */
	decoded = voice->src.totalPad;
	for (i = 0; i < voice->src.totalPad; i += 1)
	{
		voice->src.decodeCache[i] = voice->src.pad[i];
	}
	while (decoded < toDecode && voice->src.bufferList != NULL)
	{
		/* Start-of-buffer behavior */
		buffer = &voice->src.bufferList->buffer;
		if (	voice->src.curBufferOffset == 0 &&
			voice->src.callback != NULL &&
			voice->src.callback->OnBufferStart != NULL	)
		{
			voice->src.callback->OnBufferStart(
				voice->src.callback,
				buffer->pContext
			);
		}

		/* Oh look it's the actual dang decoding part */
		samples = (toDecode - decoded) / voice->src.format.nChannels;
		end = (buffer->LoopCount > 0 && buffer->LoopLength > 0) ?
			buffer->LoopLength :
			buffer->PlayLength;
		endRead = FAudio_min(
			end - voice->src.curBufferOffset,
			samples
		);
		voice->src.decode(
			buffer,
			voice->src.curBufferOffset,
			voice->src.decodeCache + decoded,
			endRead,
			&voice->src.format,
			voice->src.msadpcmCache,
			&voice->src.msadpcmExtra
		);
		decoded += endRead * voice->src.format.nChannels;
		voice->src.curBufferOffset += endRead;

		/* End-of-buffer behavior */
		if (endRead < samples || voice->src.curBufferOffset == end)
		{
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
				voice->src.bufferList = voice->src.bufferList->next;
				FAudio_free(buffer);
			}
		}
	}
	if (decoded == 0)
	{
		goto end;
	}

	/* Nowhere to send it? Just skip resampling...*/
	if (voice->sends.SendCount == 0)
	{
		/* Assign padding? */
		voice->src.totalPad = 0; /* FIXME: How do we calculate this? */
		if (voice->src.resampleOffset & FIXED_FRACTION_MASK)
		{
			voice->src.totalPad += voice->src.format.nChannels;
		}
		for (i = 0; i < voice->src.totalPad; i += 1)
		{
			voice->src.pad[i] = voice->src.decodeCache[
				decoded - (voice->src.totalPad + i)
			];
		}
		goto end;
	}

	/* Convert to float, resampling if necessary */
	if (voice->src.resampleStep == FIXED_ONE)
	{
		/* Just convert to float... */
		for (i = 0; i < decoded; i += 1)
		{
			voice->src.outputResampleCache[i] =
				(float) voice->src.decodeCache[i] / 32768.0f;
		}
		resampled = decoded;
	}
	else
	{
		/* uint32_t to fixed32 */
		resampled = decoded << FIXED_PRECISION;
		/* Division also turns fixed32 into uint32_t */
		resampled /= voice->src.resampleStep;
		/* Don't go past the end of the resample cache, the lerp will
		 * do the work of going to the extra samples for us
		 */
		resampled = FAudio_min(resampled, voice->src.outputSamples);

		/* Linear Resampler */
		decodeCache = voice->src.decodeCache;
		cur = voice->src.resampleOffset & FIXED_FRACTION_MASK;
		if (voice->src.format.nChannels == 2)
		{
			i = 0;
			while (1) /* Scary! */
			{
				/* lerp, then convert to float value */
				voice->src.outputResampleCache[i] = (float) (
					decodeCache[0] +
					(decodeCache[2] - decodeCache[0]) *
					FIXED_TO_DOUBLE(cur)
				) / 32768.0f;
				voice->src.outputResampleCache[i + 1] = (float) (
					decodeCache[1] +
					(decodeCache[3] - decodeCache[1]) *
					FIXED_TO_DOUBLE(cur)
				) / 32768.0f;

				/* Increment fraction offset by the stepping value */
				voice->src.resampleOffset += voice->src.resampleStep;
				cur += voice->src.resampleStep;

				/* Break before the decodeCache increment! */
				i += 2;
				if (i >= resampled)
				{
					break;
				}

				/* Only increment the sample offset by integer values.
				 * Sometimes this will be 0 until cur accumulates
				 * enough steps, especially for "slow" rates.
				 */
				decodeCache += cur >> FIXED_PRECISION;
				decodeCache += cur >> FIXED_PRECISION;

				/* Now that any integer has been added, drop it.
				 * The offset pointer will preserve the total.
				 */
				cur &= FIXED_FRACTION_MASK;
			}
		}
		else
		{
			i = 0;
			while (1) /* Scary! */
			{
				/* lerp, then convert to float value */
				voice->src.outputResampleCache[i] = (float) (
					decodeCache[0] +
					(decodeCache[1] - decodeCache[0]) *
					FIXED_TO_DOUBLE(cur)
				) / 32768.0f;

				/* Increment fraction offset by the stepping value */
				voice->src.resampleOffset += voice->src.resampleStep;
				cur += voice->src.resampleStep;

				/* Break before the decodeCache increment! */
				i += 1;
				if (i >= resampled)
				{
					break;
				}

				/* Only increment the sample offset by integer values.
				 * Sometimes this will be 0 until cur accumulates
				 * enough steps, especially for "slow" rates.
				 */
				decodeCache += cur >> FIXED_PRECISION;

				/* Now that any integer has been added, drop it.
				 * The offset pointer will preserve the total.
				 */
				cur &= FIXED_FRACTION_MASK;
			}
		}
	}

	/* Assign padding? */
	voice->src.totalPad = decoded - (decodeCache - voice->src.decodeCache);
	for (i = 0; i < voice->src.totalPad; i += 1)
	{
		voice->src.pad[i] = voice->src.decodeCache[
			decoded - (voice->src.totalPad + i)
		];
	}

	/* TODO: Effects, filters */

	/* Send float cache to sends */
	resampled /= voice->src.format.nChannels;
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
		for (ci = 0; ci < voice->src.format.nChannels; ci += 1)
		{
			/* Include source/channel volumes in the mix! */
			stream[j * oChan + co] = FAudio_clamp(
				stream[j * oChan + co] + (
					voice->src.outputResampleCache[
						j * voice->src.format.nChannels + ci
					] *
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

	/* TODO: Effects, filters */

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
	uint32_t i;
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
	int16_t *decodeCache,
	uint32_t samples,
	FAudioWaveFormatEx *UNUSED1,
	int16_t *UNUSED2,
	uint16_t *UNUSED3
) {
	uint32_t i;
	const int8_t *buf = ((int8_t*) buffer->pAudioData) + (
		buffer->PlayBegin + curOffset
	);
	for (i = 0; i < samples; i += 1)
	{
		*decodeCache++ = ((int16_t) *buf++) << 8;
	}
}

void FAudio_INTERNAL_DecodeStereoPCM8(
	FAudioBuffer *buffer,
	uint32_t curOffset,
	int16_t *decodeCache,
	uint32_t samples,
	FAudioWaveFormatEx *UNUSED1,
	int16_t *UNUSED2,
	uint16_t *UNUSED3
) {
	uint32_t i;
	const int8_t *buf = ((int8_t*) buffer->pAudioData) + (
		(buffer->PlayBegin + curOffset) * 2
	);
	for (i = 0; i < samples; i += 1)
	{
		*decodeCache++ = ((int16_t) *buf++) << 8;
		*decodeCache++ = ((int16_t) *buf++) << 8;
	}
}

/* 16-bit PCM Decoding */

void FAudio_INTERNAL_DecodeMonoPCM16(
	FAudioBuffer *buffer,
	uint32_t curOffset,
	int16_t *decodeCache,
	uint32_t samples,
	FAudioWaveFormatEx *UNUSED1,
	int16_t *UNUSED2,
	uint16_t *UNUSED3
) {
	FAudio_memcpy(
		decodeCache,
		((int16_t*) buffer->pAudioData) + (
			buffer->PlayBegin + curOffset
		),
		samples * 2
	);
}

void FAudio_INTERNAL_DecodeStereoPCM16(
	FAudioBuffer *buffer,
	uint32_t curOffset,
	int16_t *decodeCache,
	uint32_t samples,
	FAudioWaveFormatEx *UNUSED1,
	int16_t *UNUSED2,
	uint16_t *UNUSED3
) {
	FAudio_memcpy(
		decodeCache,
		((int16_t*) buffer->pAudioData) + (
			(buffer->PlayBegin + curOffset) * 2
		),
		samples * 4
	);
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
	*item = *((type*) *buf); \
	*buf += sizeof(type);

static inline void FAudio_INTERNAL_ReadMonoPreamble(
	uint8_t **buf,
	uint8_t *predictor,
	int16_t *delta,
	int16_t *sample1,
	int16_t *sample2
) {
	READ(predictor, uint8_t)
	READ(delta, int16_t)
	READ(sample1, int16_t)
	READ(sample2, int16_t)
}

static inline void FAudio_INTERNAL_ReadStereoPreamble(
	uint8_t **buf,
	uint8_t *predictor_l,
	uint8_t *predictor_r,
	int16_t *delta_l,
	int16_t *delta_r,
	int16_t *sample1_l,
	int16_t *sample1_r,
	int16_t *sample2_l,
	int16_t *sample2_r
) {
	READ(predictor_l, uint8_t)
	READ(predictor_r, uint8_t)
	READ(delta_l, int16_t)
	READ(delta_r, int16_t)
	READ(sample1_l, int16_t)
	READ(sample1_r, int16_t)
	READ(sample2_l, int16_t)
	READ(sample2_r, int16_t)
}

#undef READ

void FAudio_INTERNAL_DecodeMonoMSADPCM(
	FAudioBuffer *buffer,
	uint32_t curOffset,
	int16_t *decodeCache,
	uint32_t samples,
	FAudioWaveFormatEx *format,
	int16_t *msadpcmCache,
	uint16_t *msadpcmExtra
) {
	/* Iterators */
	uint8_t b, i;

	/* Read pointers */
	uint8_t *buf;
	int16_t *pcmExtra = msadpcmCache;

	/* Read sizes */
	uint32_t blocks, extra;

	/* Temp storage for ADPCM blocks */
	uint8_t predictor;
	int16_t delta;
	int16_t sample1;
	int16_t sample2;
	uint8_t nibbles[255]; /* Max align size */

	/* Align, block size */
	uint32_t align = format->nBlockAlign;
	uint32_t bsize = (align + 16) * 2;

	/* Do we only need to copy from the extra cache? */
	if (*msadpcmExtra >= samples)
	{
		FAudio_memcpy(
			decodeCache,
			msadpcmCache,
			samples * sizeof(int16_t)
		);
		if (*msadpcmExtra > samples)
		{
			FAudio_memmove(
				msadpcmCache,
				msadpcmCache + samples,
				(*msadpcmExtra - samples) * sizeof(int16_t)
			);
		}
		*msadpcmExtra -= samples;
		return;
	}

	/* Copy the extra samples we got from last time into the output */
	if (*msadpcmExtra > 0)
	{
		FAudio_memcpy(
			decodeCache,
			msadpcmCache,
			*msadpcmExtra * sizeof(int16_t)
		);
		decodeCache += *msadpcmExtra;
		samples -= *msadpcmExtra;
		*msadpcmExtra = 0;
	}

	/* Where are we starting? */
	blocks = curOffset / bsize;
	if (curOffset % bsize)
	{
		blocks += 1;
	}
	buf = (uint8_t*) buffer->pAudioData + (blocks * (align + 22));

	/* How many blocks do we need? */
	blocks = samples / bsize;
	extra = samples % bsize;

	/* Read in each block directly to the decode cache */
	for (b = 0; b < blocks; b += 1)
	{
		FAudio_INTERNAL_ReadMonoPreamble(
			&buf,
			&predictor,
			&delta,
			&sample1,
			&sample2
		);
		*decodeCache++ = sample2;
		*decodeCache++ = sample1;
		FAudio_memcpy(
			nibbles,
			buf,
			align + 15
		);
		buf += align + 15;
		for (i = 0; i < (align + 15); i += 1)
		{
			*decodeCache++ = FAudio_INTERNAL_ParseNibble(
				nibbles[i] >> 4,
				predictor,
				&delta,
				&sample1,
				&sample2
			);
			*decodeCache++ = FAudio_INTERNAL_ParseNibble(
				nibbles[i] & 0x0F,
				predictor,
				&delta,
				&sample1,
				&sample2
			);
		}
	}

	/* Have extra? Go to the MSADPCM cache */
	if (extra > 0)
	{
		FAudio_INTERNAL_ReadMonoPreamble(
			&buf,
			&predictor,
			&delta,
			&sample1,
			&sample2
		);
		*pcmExtra++ = sample2;
		*pcmExtra++ = sample1;
		FAudio_memcpy(
			nibbles,
			buf,
			align + 15
		);
		buf += align + 15;
		for (i = 0; i < (align + 15); i += 1)
		{
			*pcmExtra++ = FAudio_INTERNAL_ParseNibble(
				nibbles[i] >> 4,
				predictor,
				&delta,
				&sample1,
				&sample2
			);
			*pcmExtra++ = FAudio_INTERNAL_ParseNibble(
				nibbles[i] & 0x0F,
				predictor,
				&delta,
				&sample1,
				&sample2
			);
		}
		*msadpcmExtra = bsize - extra;
		FAudio_memcpy(
			decodeCache,
			msadpcmCache,
			extra * sizeof(int16_t)
		);
		FAudio_memmove(
			msadpcmCache,
			msadpcmCache + extra,
			*msadpcmExtra * sizeof(int16_t)
		);
	}
}

void FAudio_INTERNAL_DecodeStereoMSADPCM(
	FAudioBuffer *buffer,
	uint32_t curOffset,
	int16_t *decodeCache,
	uint32_t samples,
	FAudioWaveFormatEx *format,
	int16_t *msadpcmCache,
	uint16_t *msadpcmExtra
) {
	/* Iterators */
	uint8_t b, i;

	/* Read pointers */
	uint8_t *buf;
	int16_t *pcmExtra = msadpcmCache;

	/* Read sizes */
	uint32_t blocks, extra;

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

	/* Align, block size */
	uint32_t align = format->nBlockAlign;
	uint32_t bsize = (align + 16) * 2;

	/* Do we only need to copy from the extra cache? */
	if (*msadpcmExtra >= samples)
	{
		FAudio_memcpy(
			decodeCache,
			msadpcmCache,
			samples * sizeof(int16_t) * 2
		);
		if (*msadpcmExtra > samples)
		{
			FAudio_memmove(
				msadpcmCache,
				msadpcmCache + (samples * 2),
				(*msadpcmExtra - samples) * sizeof(int16_t) * 2
			);
		}
		*msadpcmExtra -= samples;
		return;
	}

	/* Copy the extra samples we got from last time into the output */
	if (*msadpcmExtra > 0)
	{
		FAudio_memcpy(
			decodeCache,
			msadpcmCache,
			*msadpcmExtra * sizeof(int16_t) * 2
		);
		decodeCache += *msadpcmExtra * 2;
		samples -= *msadpcmExtra;
		*msadpcmExtra = 0;
	}

	/* Where are we starting? */
	blocks = curOffset / bsize;
	if (curOffset % bsize)
	{
		blocks += 1;
	}
	buf = (uint8_t*) buffer->pAudioData + (blocks * ((align + 22) * 2));

	/* How many blocks do we need? */
	blocks = samples / bsize;
	extra = samples % bsize;

	/* Read in each block directly to the decode cache */
	for (b = 0; b < blocks; b += 1)
	{
		FAudio_INTERNAL_ReadStereoPreamble(
			&buf,
			&l_predictor,
			&r_predictor,
			&l_delta,
			&r_delta,
			&l_sample1,
			&r_sample1,
			&l_sample2,
			&r_sample2
		);
		*decodeCache++ = l_sample2;
		*decodeCache++ = r_sample2;
		*decodeCache++ = l_sample1;
		*decodeCache++ = r_sample1;
		FAudio_memcpy(
			nibbles,
			buf,
			(align + 15) * 2
		);
		buf += (align + 15) * 2;
		for (i = 0; i < ((align + 15) * 2); i += 1)
		{
			*decodeCache++ = FAudio_INTERNAL_ParseNibble(
				nibbles[i] >> 4,
				l_predictor,
				&l_delta,
				&l_sample1,
				&l_sample2
			);
			*decodeCache++ = FAudio_INTERNAL_ParseNibble(
				nibbles[i] & 0x0F,
				r_predictor,
				&r_delta,
				&r_sample1,
				&r_sample2
			);
		}
	}

	/* Have extra? Go to the MSADPCM cache */
	if (extra > 0)
	{
		FAudio_INTERNAL_ReadStereoPreamble(
			&buf,
			&l_predictor,
			&r_predictor,
			&l_delta,
			&r_delta,
			&l_sample1,
			&r_sample1,
			&l_sample2,
			&r_sample2
		);
		*pcmExtra++ = l_sample2;
		*pcmExtra++ = r_sample2;
		*pcmExtra++ = l_sample1;
		*pcmExtra++ = r_sample1;
		FAudio_memcpy(
			nibbles,
			buf,
			(align + 15) * 2
		);
		buf += (align + 15) * 2;
		for (i = 0; i < ((align + 15) * 2); i += 1)
		{
			*pcmExtra++ = FAudio_INTERNAL_ParseNibble(
				nibbles[i] >> 4,
				l_predictor,
				&l_delta,
				&l_sample1,
				&l_sample2
			);
			*pcmExtra++ = FAudio_INTERNAL_ParseNibble(
				nibbles[i] & 0x0F,
				r_predictor,
				&r_delta,
				&r_sample1,
				&r_sample2
			);
		}
		*msadpcmExtra = bsize - extra;
		FAudio_memcpy(
			decodeCache,
			msadpcmCache,
			extra * sizeof(int16_t) * 2
		);
		FAudio_memmove(
			msadpcmCache,
			msadpcmCache + (extra * 2),
			*msadpcmExtra * sizeof(int16_t) * 2
		);
	}
}
