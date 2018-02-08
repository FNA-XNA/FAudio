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
	uint32_t toDecode, decoded = 0;

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
			voice->src.decodeCache[toDecode] = voice->src.pad[1];
		}
		decoded += 1;
	}
	while (decoded < toDecode && voice->src.bufferList != NULL)
	{
		decoded = toDecode;/*TODO: += voice->src.bufferList->decode(
			toDecode - decoded,
			voice->src.bufferList,
			voice->src.decodeCache + decoded,
			voice->src.decodeCache + toDecode + decoded,
			voice->src.callback,
			&voice->src.format
		);*/
	}
	if (decoded == 0)
	{
		goto end;
	}

	/* Convert to float, resampling if necessary */
	if (voice->sends.SendCount > 0)
	{
		if (toDecode == (voice->src.outputSamples / voice->src.format.nChannels))
		{
			/* Just convert to float... */
			for (i = 0; i < voice->src.outputSamples; i += 1)
			{
				voice->src.outputResampleCache[i] =
					(float) voice->src.decodeCache[i] / 32768.0f;
			}
		}
		else
		{
			/* TODO: Resample! */
		}
		voice->src.pad[0] = voice->src.decodeCache[toDecode - 1];
		if (voice->src.format.nChannels == 2)
		{
			voice->src.pad[1] =
				voice->src.decodeCache[toDecode * 2 - 1];
		}
		voice->src.hasPad = 1;
	}

	/* TODO: Effects, filters */

	/* Send float cache to sends */
	for (i = 0; i < voice->sends.SendCount; i += 1)
	{
		/* TODO: Use output matrix */
		out = voice->sends.pSends[i].pOutputVoice;
		if (out->type == FAUDIO_VOICE_MASTER)
		{
			for (j = 0; j < voice->src.outputSamples; j += 1)
			{
				out->master.output[j] *=
					voice->src.outputResampleCache[j];
			}
		}
		else
		{
			for (j = 0; j < voice->src.outputSamples; j += 1)
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
	uint32_t i, j;
	FAudioVoice *out;

	/* Nothing to do? */
	if (voice->sends.SendCount == 0)
	{
		return;
	}

	/* Convert to float, resampling if necessary */
	if (voice->sends.SendCount > 0)
	{
		if (voice->mix.inputSamples == voice->mix.outputSamples)
		{
			/* Just copy it... */
			FAudio_memcpy(
				voice->mix.outputResampleCache,
				voice->mix.inputCache,
				sizeof(float) * voice->mix.outputSamples
			);
		}
		else
		{
			/* TODO: Resample! */
		}
		voice->mix.pad[0] =
			voice->mix.inputCache[voice->mix.inputSamples - 1];
		if (voice->mix.inputChannels == 2)
		{
			voice->mix.pad[1] =
				voice->mix.inputCache[voice->mix.inputSamples * 2 - 1];
		}
		voice->mix.hasPad = 1;
	}

	/* TODO: Effects, filters */

	/* Send float cache to sends */
	for (i = 0; i < voice->sends.SendCount; i += 1)
	{
		/* TODO: Use output matrix */
		out = voice->sends.pSends[i].pOutputVoice;
		if (out->type == FAUDIO_VOICE_MASTER)
		{
			for (j = 0; j < voice->mix.outputSamples; j += 1)
			{
				out->master.output[j] *=
					voice->mix.outputResampleCache[j];
			}
		}
		else
		{
			for (j = 0; j < voice->mix.outputSamples; j += 1)
			{
				out->mix.inputCache[j] *=
					voice->mix.outputResampleCache[j];
			}
		}
	}

	/* Zero this at the end, for the next update */
	FAudio_zero(
		voice->mix.inputCache,
		sizeof(float) * voice->mix.inputSamples
	);
}

#if 0
	/* TODO: For decode callbacks.... */
	if (voice->src.callback != NULL)
	{
		if (voice->src.callback->OnBufferEnd != NULL)
		{
			voice->src.callback->OnBufferEnd(
				voice->src.callback,
				voice->src.bufferList->buffer.pContext
			);
		}
		if (	voice->src.bufferList->buffer.Flags & FAUDIO_END_OF_STREAM &&
			voice->src.callback->OnStreamEnd != NULL	)
		{
			voice->src.callback->OnStreamEnd(
				voice->src.callback
			);
		}
	}
#endif
