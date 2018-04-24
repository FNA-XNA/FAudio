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

void LinkedList_AddEntry(LinkedList **start, void* toAdd)
{
	LinkedList *newEntry, *latest;
	newEntry = (LinkedList*) FAudio_malloc(sizeof(LinkedList));
	newEntry->entry = toAdd;
	newEntry->next = NULL;
	if (*start == NULL)
	{
		*start = newEntry;
	}
	else
	{
		latest = *start;
		while (latest->next != NULL)
		{
			latest = latest->next;
		}
		latest->next = newEntry;
	}
}

void LinkedList_RemoveEntry(LinkedList **start, void* toRemove)
{
	LinkedList *latest, *prev;
	latest = *start;
	prev = latest;
	while (latest != NULL)
	{
		if (latest->entry == toRemove)
		{
			if (latest == prev) /* First in list */
			{
				*start = latest->next;
			}
			else
			{
				prev->next = latest->next;
			}
			FAudio_free(latest);
			return;
		}
		prev = latest;
		latest = latest->next;
	}
	FAudio_assert(0 && "LinkedList element not found!");
}

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

static uint32_t FAudio_INTERNAL_DecodeBuffers(
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
		decoding = (uint32_t) *toDecode - decoded;

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
		end = (buffer->LoopCount > 0) ?
			(buffer->LoopBegin + buffer->LoopLength) :
			buffer->PlayLength;
		endRead = FAudio_min(
			end - voice->src.curBufferOffset,
			decoding
		);

		/* Decode... */
		voice->src.decode(
			buffer,
			voice->src.curBufferOffset,
			voice->audio->decodeCache + decoded,
			voice->audio->decodeCache + voice->src.decodeSamples + decoded,
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
				if (buffer->LoopCount < FAUDIO_LOOP_INFINITE)
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
							voice->audio->decodeCache + (
								(i * voice->src.decodeSamples) +
								(decoded + endRead)
							),
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

static void FAudio_INTERNAL_ResamplePCM(
	FAudioSourceVoice *voice,
	float **resampleCache,
	uint64_t toResample
) {
	/* Linear Resampler */
	/* TODO: SSE */
	uint32_t i, j;
	float *dCache[2] =
	{
		voice->audio->decodeCache,
		voice->audio->decodeCache + voice->src.decodeSamples
	};
	uint64_t cur = voice->src.resampleOffset & FIXED_FRACTION_MASK;
	for (i = 0; i < toResample; i += 1)
	{
		for (j = 0; j < voice->src.format.nChannels; j += 1)
		{
			/* lerp, then convert to float value */
			*(*resampleCache)++ = (float) (
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

static inline void FAudio_INTERNAL_FilterVoice(
	const FAudioFilterParameters *filter,
	FAudioFilterState *filterState,
	float *samples,
	uint32_t numSamples,
	uint16_t numChannels)
{
	uint32_t j, ci;

	/* Apply a digital state-variable filter to the voice.
	 * The difference equations of the filter are:
	 *
	 * Yl(n) = F Yb(n - 1) + Yl(n - 1)
	 * Yh(n) = x(n) - Yl(n) - OneOverQ Yb(n - 1)
	 * Yb(n) = F Yh(n) + Yb(n - 1)
	 * Yn(n) = Yl(n) + Yh(n)
	 *
	 * Please note that FAudioFilterParameters.Frequency is defined as:
	 *
	 * (2 * sin(pi * (desired filter cutoff frequency) / sampleRate))
	 *
	 * - @JohanSmet
	 */

	for (j = 0; j < numSamples; j += 1)
	for (ci = 0; ci < numChannels; ci += 1)
	{
		filterState[ci][LowPassFilter] = filterState[ci][LowPassFilter] + (filter->Frequency * filterState[ci][BandPassFilter]);
		filterState[ci][HighPassFilter] = samples[j * numChannels + ci] - filterState[ci][LowPassFilter] - (filter->OneOverQ * filterState[ci][BandPassFilter]);
		filterState[ci][BandPassFilter] = (filter->Frequency * filterState[ci][HighPassFilter]) + filterState[ci][BandPassFilter];
		filterState[ci][NotchFilter] = filterState[ci][HighPassFilter] + filterState[ci][LowPassFilter];
		samples[j * numChannels + ci] = filterState[ci][filter->Type];
	}
}

static inline void FAudio_INTERNAL_ProcessEffectChain(
	FAudioVoice *voice,
	uint32_t channels,
	uint32_t sampleRate,
	float *buffer,
	uint32_t samples
) {
	uint32_t i;
	FAPOParametersBase *fapo;
	FAudioWaveFormatEx fmt;
	FAPOLockForProcessBufferParameters lockParams;
	FAPOProcessBufferParameters params;

	/* FIXME: This always assumes in-place processing is supported */

	/* Lock in formats that the APO will expect for processing */
	fmt.wFormatTag = 3;
	fmt.nChannels = channels;
	fmt.nSamplesPerSec = sampleRate;
	fmt.nAvgBytesPerSec = sampleRate * channels * 4;
	fmt.nBlockAlign = 4;
	fmt.wBitsPerSample = 32;
	fmt.cbSize = 0;
	lockParams.pFormat = &fmt;
	lockParams.MaxFrameCount = samples;

	/* Set up the buffer to be written into */
	params.pBuffer = buffer;
	params.BufferFlags = FAPO_BUFFER_VALID;
	params.ValidFrameCount = samples;

	/* Update parameters, process! */
	for (i = 0; i < voice->effects.count; i += 1)
	{
		fapo = (FAPOParametersBase*) voice->effects.desc[i].pEffect;
		if (voice->effects.parameterUpdates[i])
		{
			fapo->parameters.SetParameters(
				fapo,
				voice->effects.parameters[i],
				voice->effects.parameterSizes[i]
			);
			voice->effects.parameterUpdates[i] = 0;
		}
		fapo->base.base.LockForProcess(
			fapo,
			1,
			&lockParams,
			1,
			&lockParams
		);
		fapo->base.base.Process(
			fapo,
			1,
			&params,
			1,
			&params,
			voice->effects.desc[i].InitialState
		);
		fapo->base.base.UnlockForProcess(fapo);
	}
}

static void FAudio_INTERNAL_MixSource(FAudioSourceVoice *voice)
{
	/* Iterators */
	uint32_t i, j, co, ci;
	/* Decode/Resample variables */
	uint64_t toDecode;
	uint64_t toResample;
	uint32_t resetOffset;
	float *resampleCache;
	/* Output mix variables */
	float *stream;
	uint32_t mixed;
	uint32_t oChan;
	FAudioVoice *out;
	uint32_t outputRate;
	double stepd;

	/* Calculate the resample stepping value */
	if (voice->src.resampleFreqRatio != voice->src.freqRatio)
	{
		out = (voice->sends.SendCount == 0) ?
			voice->audio->master : /* Barf */
			voice->sends.pSends->pOutputVoice;
		outputRate = (out->type == FAUDIO_VOICE_MASTER) ?
			out->master.inputSampleRate :
			out->mix.inputSampleRate;
		stepd = (
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
	resampleCache = voice->audio->resampleCache;
	while (mixed < voice->src.resampleSamples && voice->src.bufferList != NULL)
	{

		/* Base decode size, int to fixed... */
		toDecode = (voice->src.resampleSamples - mixed) * voice->src.resampleStep;
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
		toResample = FAudio_min(toResample, voice->src.resampleSamples - mixed);

		/* Resample... */
		if (voice->src.resampleStep == FIXED_ONE)
		{
			/* Actually, just copy directly... */
			for (i = 0; i < toResample; i += 1)
			for (j = 0; j < voice->src.format.nChannels; j += 1)
			{
				/* TODO: SSE */
				*resampleCache++ = *(voice->audio->decodeCache + (
					(j * voice->src.decodeSamples) +
					i
				));
			}
		}
		else
		{
			FAudio_INTERNAL_ResamplePCM(voice, &resampleCache, toResample);
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
		mixed += (uint32_t) toResample;
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

	/* Filters */
	if (voice->flags & FAUDIO_VOICE_USEFILTER)
	{
		FAudio_INTERNAL_FilterVoice(
			&voice->filter,
			voice->filterState,
			voice->audio->resampleCache,
			mixed,
			voice->src.format.nChannels
		);
	}

	/* Process effect chain */
	if (voice->effects.count > 0)
	{
		FAudio_INTERNAL_ProcessEffectChain(
			voice,
			voice->src.format.nChannels,
			voice->src.format.nSamplesPerSec,
			voice->audio->resampleCache,
			mixed
		);
	}

	/* Send float cache to sends */
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

		/* TODO: SSE */
		for (j = 0; j < mixed; j += 1)
		for (co = 0; co < oChan; co += 1)
		for (ci = 0; ci < voice->src.format.nChannels; ci += 1)
		{
			/* Include source/channel volumes in the mix! */
			stream[j * oChan + co] += (
				voice->audio->resampleCache[
					j * voice->src.format.nChannels + ci
				] *
				voice->channelVolume[ci] *
				voice->volume *
				voice->sendCoefficients[i][
					co * voice->src.format.nChannels + ci
				]
			);
			stream[j * oChan + co] = FAudio_clamp(
				stream[j * oChan + co],
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

static void FAudio_INTERNAL_MixSubmix(FAudioSubmixVoice *voice)
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
		voice->audio->resampleCache,
		voice->mix.outputSamples
	);

	/* Submix overall volume is applied _before_ effects/filters, blech! */
	for (i = 0; i < resampled; i += 1)
	{
		/* TODO: SSE */
		voice->audio->resampleCache[i] *= voice->volume;
		voice->audio->resampleCache[i] = FAudio_clamp(
			voice->audio->resampleCache[i],
			-FAUDIO_MAX_VOLUME_LEVEL,
			FAUDIO_MAX_VOLUME_LEVEL
		);
	}
	resampled /= voice->mix.inputChannels;

	/* Filters */
	if (voice->flags & FAUDIO_VOICE_USEFILTER)
	{
		FAudio_INTERNAL_FilterVoice(
			&voice->filter,
			voice->filterState,
			voice->audio->resampleCache,
			resampled,
			voice->mix.inputChannels
		);
	}

	/* Process effect chain */
	if (voice->effects.count > 0)
	{
		FAudio_INTERNAL_ProcessEffectChain(
			voice,
			voice->mix.inputChannels,
			voice->mix.inputSampleRate,
			voice->audio->resampleCache,
			resampled
		);
	}

	/* Send float cache to sends */
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

		/* TODO: SSE */
		for (j = 0; j < resampled; j += 1)
		for (co = 0; co < oChan; co += 1)
		for (ci = 0; ci < voice->mix.inputChannels; ci += 1)
		{
			stream[j * oChan + co] += (
				voice->audio->resampleCache[
					j * voice->mix.inputChannels + ci
				] *
				voice->channelVolume[ci] *
				voice->sendCoefficients[i][
					co * voice->mix.inputChannels + ci
				]
			);
			stream[j * oChan + co] = FAudio_clamp(
				stream[j * oChan + co],
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
	LinkedList *list;
	FAudioSourceVoice *source;
	FAudioSubmixVoice *submix;
	FAudioEngineCallback *callback;

	if (!audio->active)
	{
		return;
	}

	/* ProcessingPassStart callbacks */
	list = audio->callbacks;
	while (list != NULL)
	{
		callback = (FAudioEngineCallback*) list->entry;
		if (callback->OnProcessingPassStart != NULL)
		{
			callback->OnProcessingPassStart(
				callback
			);
		}
		list = list->next;
	}

	/* Writes to master will directly write to output */
	audio->master->master.output = output;

	/* Mix sources */
	list = audio->sources;
	while (list != NULL)
	{
		source = (FAudioSourceVoice*) list->entry;
		if (source->src.active)
		{
			FAudio_INTERNAL_MixSource(source);
		}
		list = list->next;
	}

	/* Mix submixes, ordered by processing stage */
	for (i = 0; i < audio->submixStages; i += 1)
	{
		list = audio->submixes;
		while (list != NULL)
		{
			submix = (FAudioSubmixVoice*) list->entry;
			if (submix->mix.processingStage == i)
			{
				FAudio_INTERNAL_MixSubmix(submix);
			}
			list = list->next;
		}
	}

	/* Apply master volume */
	totalSamples = audio->updateSize * audio->master->master.inputChannels;
	for (i = 0; i < totalSamples; i += 1)
	{
		/* TODO: SSE */
		output[i] = FAudio_clamp(
			output[i] * audio->master->volume,
			-FAUDIO_MAX_VOLUME_LEVEL,
			FAUDIO_MAX_VOLUME_LEVEL
		);
	}

	/* Process master effect chain */
	if (audio->master->effects.count > 0)
	{
		FAudio_INTERNAL_ProcessEffectChain(
			audio->master,
			audio->master->master.inputChannels,
			audio->master->master.inputSampleRate,
			output,
			audio->updateSize
		);
	}

	/* OnProcessingPassEnd callbacks */
	list = audio->callbacks;
	while (list != NULL)
	{
		callback = (FAudioEngineCallback*) list->entry;
		if (callback->OnProcessingPassEnd != NULL)
		{
			callback->OnProcessingPassEnd(
				callback
			);
		}
		list = list->next;
	}
}

void FAudio_INTERNAL_ResizeDecodeCache(FAudio *audio, uint32_t samples)
{
	if (samples > audio->decodeSamples)
	{
		audio->decodeSamples = samples;
		audio->decodeCache = (float*) FAudio_realloc(
			audio->decodeCache,
			sizeof(float) * audio->decodeSamples
		);
	}
}

void FAudio_INTERNAL_ResizeResampleCache(FAudio *audio, uint32_t samples)
{
	if (samples > audio->resampleSamples)
	{
		audio->resampleSamples = samples;
		audio->resampleCache = (float*) FAudio_realloc(
			audio->resampleCache,
			sizeof(float) * audio->resampleSamples
		);
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
	const int8_t *buf = ((int8_t*) buffer->pAudioData) + curOffset;
	for (i = 0; i < samples; i += 1)
	{
		/* TODO: SSE */
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
	const int8_t *buf = ((int8_t*) buffer->pAudioData) + (curOffset * 2);
	for (i = 0; i < samples; i += 1)
	{
		/* TODO: SSE */
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
	const int16_t *buf = ((int16_t*) buffer->pAudioData) + curOffset;
	for (i = 0; i < samples; i += 1)
	{
		/* TODO: SSE */
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
	const int16_t *buf = ((int16_t*) buffer->pAudioData) + (curOffset * 2);
	for (i = 0; i < samples; i += 1)
	{
		/* TODO: SSE */
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
	const float *buf = ((float*) buffer->pAudioData) + curOffset;
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
	const float *buf = ((float*) buffer->pAudioData) + (curOffset * 2);
	for (i = 0; i < samples; i += 1)
	{
		/* TODO: SSE */
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

	/* Preamble */
	READ(predictor, uint8_t)
	READ(delta, int16_t)
	READ(sample1, int16_t)
	READ(sample2, int16_t)
	align -= 7;

	/* Samples */
	*blockCache++ = sample2;
	*blockCache++ = sample1;
	for (i = 0; i < align; i += 1, *buf += 1)
	{
		*blockCache++ = FAudio_INTERNAL_ParseNibble(
			*(*buf) >> 4,
			predictor,
			&delta,
			&sample1,
			&sample2
		);
		*blockCache++ = FAudio_INTERNAL_ParseNibble(
			*(*buf) & 0x0F,
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

	/* Samples */
	*blockCache++ = l_sample2;
	*blockCache++ = r_sample2;
	*blockCache++ = l_sample1;
	*blockCache++ = r_sample1;
	for (i = 0; i < align; i += 1, *buf += 1)
	{
		*blockCache++ = FAudio_INTERNAL_ParseNibble(
			*(*buf) >> 4,
			l_predictor,
			&l_delta,
			&l_sample1,
			&l_sample2
		);
		*blockCache++ = FAudio_INTERNAL_ParseNibble(
			*(*buf) & 0x0F,
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
		/* TODO: SSE */
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
		/* TODO: SSE */
		for (i = 0, off = midOffset * 2; i < copy; i += 1)
		{
			*decodeCacheL++ = blockCache[off++] / 32768.0f;
			*decodeCacheR++ = blockCache[off++] / 32768.0f;
		}
		samples -= copy;
		midOffset = 0;
	}
}
