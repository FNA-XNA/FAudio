/* FACT - XACT Reimplementation for FNA
 * Copyright 2009-2018 Ethan Lee and the MonoGame Team
 *
 * Released under the Microsoft Public License.
 * See LICENSE for details.
 */

#include "FAudio_internal.h"

void FAudio_INTERNAL_InitResampler(FAudioResampleState *resample)
{
	FAudio_zero(resample, sizeof(FAudioResampleState));
	resample->pitch = 0xFFFF; /* Force update on first poll */
}

void FAudio_INTERNAL_MixSource(FAudioSourceVoice *voice)
{
	uint32_t i, j;
	FAudioVoice *out;
	FAudioBuffer *buffer;
	uint32_t toDecode, decoded;
	uint32_t samples, end, endRead;

	/* Nothing to do? */
	if (voice->src.bufferList == 0)
	{
		return;
	}

	/* Decode length depends on pitch */
	toDecode = (uint32_t) (
		voice->src.decodeSamples *
		voice->src.freqRatio /* FIXME: Imprecise! */
	);
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
	if (voice->src.hasPad)
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
		end = (buffer->LoopCount > 0) ?
			FAudio_min(
				buffer->LoopLength,
				buffer->PlayLength
			) : buffer->PlayLength;
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
		decoded += endRead;
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

	/* Convert to float, resampling if necessary */
	if (voice->sends.SendCount > 0)
	{
		if (toDecode == voice->src.outputSamples)
		{
			/* Just convert to float... */
			for (i = 0; i < decoded; i += 1)
			{
				voice->src.outputResampleCache[i] =
					(float) voice->src.decodeCache[i] / 32768.0f;
			}
		}
		else
		{
			/* TODO: Resample! */
		}
	}

	/* Assign padding */
	if (voice->src.format.nChannels == 2)
	{
		voice->src.pad[0] = voice->src.decodeCache[decoded - 2];
		voice->src.pad[1] = voice->src.decodeCache[decoded - 1];
	}
	else
	{
		voice->src.pad[0] = voice->src.decodeCache[decoded - 1];
	}
	voice->src.hasPad = 1;

	/* TODO: Effects, filters */

	/* Send float cache to sends */
	for (i = 0; i < voice->sends.SendCount; i += 1)
	{
		/* TODO: Use output matrix */
		out = voice->sends.pSends[i].pOutputVoice;
		if (out->type == FAUDIO_VOICE_MASTER)
		{
			for (j = 0; j < decoded; j += 1)
			{
				out->master.output[j] *=
					voice->src.outputResampleCache[j];
			}
		}
		else
		{
			for (j = 0; j < decoded; j += 1)
			{
				out->mix.inputCache[j] *=
					voice->src.outputResampleCache[j];
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
				out->master.output[j] *=
					voice->mix.outputResampleCache[j];
			}
		}
		else
		{
			for (j = 0; j < resampled; j += 1)
			{
				out->mix.inputCache[j] *=
					voice->mix.outputResampleCache[j];
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
		FAudio_INTERNAL_MixSource(source->voice);
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
			buffer->PlayBegin + curOffset
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
