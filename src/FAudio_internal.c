/* FACT - XACT Reimplementation for FNA
 * Copyright 2009-2018 Ethan Lee and the MonoGame Team
 *
 * Released under the Microsoft Public License.
 * See LICENSE for details.
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
 * Steps are stored in fixed-point with 12 bits for the fraction:
 *
 * 00000000000000000000 000000000000
 * ^ Integer block (20) ^ Fraction block (12)
 *
 * For example, to get 1.5:
 * 00000000000000000001 100000000000
 *
 * The Integer block works exactly like you'd expect.
 * The Fraction block is divided by the Integer's "One" value.
 * So, the above Fraction represented visually...
 *   1 << 11
 *   -------
 *   1 << 12
 * ... which, simplified, is...
 *   1 << 0
 *   ------
 *   1 << 1
 * ... in other words, 1 / 2, or 0.5.
 */
#define FIXED_PRECISION		12
#define FIXED_ONE		(1 << FIXED_PRECISION)

/* Quick way to drop parts */
#define FIXED_FRACTION_MASK	(FIXED_ONE - 1)
#define FIXED_INTEGER_MASK	~RESAMPLE_FRACTION_MASK

/* Helper macros to convert fixed to float */
#define DOUBLE_TO_FIXED(dbl) \
	((uint32_t) (dbl * FIXED_ONE + 0.5)) /* FIXME: Just round, or ceil? */
#define FIXED_TO_DOUBLE(fxd) ( \
	(double) (fxd >> FIXED_PRECISION) + /* Integer part */ \
	((fxd & FIXED_FRACTION_MASK) * (1.0 / FIXED_ONE)) /* Fraction part */ \
)

void FAudio_INTERNAL_MixSource(FAudioSourceVoice *voice)
{
	uint32_t i, j;
	FAudioVoice *out;
	FAudioBuffer *buffer;
	uint32_t toDecode, decoded, resampled;
	uint32_t samples, end, endRead;
	int16_t *decodeCache;
	uint32_t cur;

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
	 * to get the "real" buffer size. But we also need to ceil() to get the
	 * extra sample needed for interpolating past the "end" of the
	 * unresampled buffer.
	 */
	toDecode = voice->src.decodeSamples * voice->src.resampleStep;
	toDecode += (
		/* If frac > 0, int goes up by one... */
		(voice->src.resampleOffset + FIXED_FRACTION_MASK) &
		/* ... then we chop off anything left */
		FIXED_FRACTION_MASK
	);
	toDecode >>= FIXED_PRECISION;
	decoded = 0;

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
	if (voice->src.resampleOffset & FIXED_FRACTION_MASK)
	{
		voice->src.decodeCache[0] = voice->src.pad[0];
		if (voice->src.format.nChannels == 2)
		{
			voice->src.decodeCache[1] = voice->src.pad[1];
			decoded += 1;
		}
		decoded += 1;
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
			endRead
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
		if (voice->src.resampleOffset & FIXED_FRACTION_MASK)
		{
			if (voice->src.format.nChannels == 2)
			{
				voice->src.pad[0] = voice->src.decodeCache[decoded - 2];
				voice->src.pad[1] = voice->src.decodeCache[decoded - 1];
			}
			else
			{
				voice->src.pad[0] = voice->src.decodeCache[decoded - 1];
			}
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

		/* Linear Resampler */
		decodeCache = voice->src.decodeCache;
		cur = voice->src.resampleOffset & FIXED_FRACTION_MASK;
		if (voice->src.format.nChannels == 2)
		{
			for (i = 0; i < resampled; i += 2)
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
			for (i = 0; i < resampled; i += 1)
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
	if (voice->src.resampleOffset & FIXED_FRACTION_MASK)
	{
		if (voice->src.format.nChannels == 2)
		{
			voice->src.pad[0] = voice->src.decodeCache[decoded - 2];
			voice->src.pad[1] = voice->src.decodeCache[decoded - 1];
		}
		else
		{
			voice->src.pad[0] = voice->src.decodeCache[decoded - 1];
		}
	}

	/* TODO: Effects, filters */

	/* Send float cache to sends */
	for (i = 0; i < voice->sends.SendCount; i += 1)
	{
		/* TODO: Use output matrix */
		out = voice->sends.pSends[i].pOutputVoice;
		if (out->type == FAUDIO_VOICE_MASTER)
		{
			for (j = 0; j < resampled; j += 1)
			{
				out->master.output[j] = FAudio_clamp(
					out->master.output[j] + voice->mix.outputResampleCache[j],
					-FAUDIO_MAX_VOLUME_LEVEL,
					FAUDIO_MAX_VOLUME_LEVEL
				);
			}
		}
		else
		{
			for (j = 0; j < resampled; j += 1)
			{
				out->mix.inputCache[j] = FAudio_clamp(
					out->mix.inputCache[j] + voice->mix.outputResampleCache[j],
					-FAUDIO_MAX_VOLUME_LEVEL,
					FAUDIO_MAX_VOLUME_LEVEL
				);
			}
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
	FAudioVoice *out;
	uint32_t resampled, i, j;

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

	/* TODO: Effects, filters */

	/* Send float cache to sends */
	for (i = 0; i < voice->sends.SendCount; i += 1)
	{
		/* TODO: Use output matrix */
		out = voice->sends.pSends[i].pOutputVoice;
		if (out->type == FAUDIO_VOICE_MASTER)
		{
			for (j = 0; j < resampled; j += 1)
			{
				out->master.output[j] = FAudio_clamp(
					out->master.output[j] + voice->mix.outputResampleCache[j],
					-FAUDIO_MAX_VOLUME_LEVEL,
					FAUDIO_MAX_VOLUME_LEVEL
				);
			}
		}
		else
		{
			for (j = 0; j < resampled; j += 1)
			{
				out->mix.inputCache[j] = FAudio_clamp(
					out->mix.inputCache[j] + voice->mix.outputResampleCache[j],
					-FAUDIO_MAX_VOLUME_LEVEL,
					FAUDIO_MAX_VOLUME_LEVEL
				);
			}
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

void FAudio_INTERNAL_DecodeMonoPCM8(
	FAudioBuffer *buffer,
	uint32_t curOffset,
	int16_t *decodeCache,
	uint32_t samples
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
	uint32_t samples
) {
	uint32_t i;
	const int8_t *buf = ((int8_t*) buffer->pAudioData) + (
		(buffer->PlayBegin + curOffset) * 2
	);
	for (i = 0; i < samples; i += 1)
	{
		*decodeCache++ = ((int16_t) *buf++) << 8;
	}
}

void FAudio_INTERNAL_DecodeMonoPCM16(
	FAudioBuffer *buffer,
	uint32_t curOffset,
	int16_t *decodeCache,
	uint32_t samples
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
	uint32_t samples
) {
	FAudio_memcpy(
		decodeCache,
		((int16_t*) buffer->pAudioData) + (
			(buffer->PlayBegin + curOffset) * 2
		),
		samples * 4
	);
}

void FAudio_INTERNAL_DecodeMonoMSADPCM(
	FAudioBuffer *buffer,
	uint32_t curOffset,
	int16_t *decodeCache,
	uint32_t samples
) {
	/* TODO */
	FAudio_zero(decodeCache, samples * 2);
}

void FAudio_INTERNAL_DecodeStereoMSADPCM(
	FAudioBuffer *buffer,
	uint32_t curOffset,
	int16_t *decodeCache,
	uint32_t samples
) {
	/* TODO */
	FAudio_zero(decodeCache, samples * 2);
}
