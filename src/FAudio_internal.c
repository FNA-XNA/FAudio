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
#if 0
	uint32_t toDecode = (uint32_t) (
		voice->src.decodeSamples *
		voice->src.freqRatio /* FIXME: Imprecise! */
	);
#endif
}

void FAudio_INTERNAL_MixSubmix(FAudioSubmixVoice *voice)
{
	/* Zero this at the end, for the next update */
	FAudio_zero(
		voice->mix.inputCache,
		sizeof(float) * voice->mix.inputSamples
	);
}
