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
}

void FAudio_INTERNAL_MixSubmix(FAudioSubmixVoice *voice)
{
	/* Zero this at the end, for the next update */
	FAudio_zero(
		(*ppSubmixVoice)->mix.inputSamples,
		(*ppSubmixVoice)->mix.inputBufferSize
	);
}
