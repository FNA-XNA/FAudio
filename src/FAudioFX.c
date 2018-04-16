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

#include "FAudioFX.h"
#include "FAPOBase.h"
#include "FAudio_internal.h"

uint32_t FAudioCreateVolumeMeter(void** ppApo, uint32_t Flags)
{
	return 0;
}

uint32_t FAudioCreateReverb(void** ppApo, uint32_t Flags)
{
	return 0;
}

void ReverbConvertI3DL2ToNative(
	const FAudioFXReverbI3DL2Parameters *pI3DL2,
	FAudioFXReverbParameters *pNative
) {
	float reflectionsDelay;
	float reverbDelay;

	pNative->RearDelay = FAUDIOFX_REVERB_DEFAULT_REAR_DELAY;
	pNative->PositionLeft = FAUDIOFX_REVERB_DEFAULT_POSITION;
	pNative->PositionRight = FAUDIOFX_REVERB_DEFAULT_POSITION;
	pNative->PositionMatrixLeft = FAUDIOFX_REVERB_DEFAULT_POSITION_MATRIX;
	pNative->PositionMatrixRight = FAUDIOFX_REVERB_DEFAULT_POSITION_MATRIX;
	pNative->RoomSize = FAUDIOFX_REVERB_DEFAULT_ROOM_SIZE;
	pNative->LowEQCutoff = 4;
	pNative->HighEQCutoff = 6;

	pNative->RoomFilterMain = (float) pI3DL2->Room / 100.0f;
	pNative->RoomFilterHF = (float) pI3DL2->RoomHF / 100.0f;

	if (pI3DL2->DecayHFRatio >= 1.0f)
	{
		int32_t index = (int32_t) (-4.0 * FAudio_log10(pI3DL2->DecayHFRatio));
		if (index < -8)
		{
			index = -8;
		}
		pNative->LowEQGain = (uint8_t) ((index < 0) ? index + 8 : 8);
		pNative->HighEQGain = 8;
		pNative->DecayTime = pI3DL2->DecayTime * pI3DL2->DecayHFRatio;
	}
	else
	{
		int32_t index = (int32_t) (4.0 * FAudio_log10(pI3DL2->DecayHFRatio));
		if (index < -8)
		{
			index = -8;
		}
		pNative->LowEQGain = 8;
		pNative->HighEQGain = (uint8_t) ((index < 0) ? index + 8 : 8);
		pNative->DecayTime = pI3DL2->DecayTime;
	}

	reflectionsDelay = pI3DL2->ReflectionsDelay * 1000.0f;
	if (reflectionsDelay >= FAUDIOFX_REVERB_MAX_REFLECTIONS_DELAY)
	{
		reflectionsDelay = (float) (FAUDIOFX_REVERB_MAX_REFLECTIONS_DELAY - 1);
	}
	else if (reflectionsDelay <= 1)
	{
		reflectionsDelay = 1;
	}
	pNative->ReflectionsDelay = (uint32_t) reflectionsDelay;

	reverbDelay = pI3DL2->ReverbDelay * 1000.0f;
	if (reverbDelay >= FAUDIOFX_REVERB_MAX_REVERB_DELAY)
	{
		reverbDelay = (float) (FAUDIOFX_REVERB_MAX_REVERB_DELAY - 1);
	}
	pNative->ReverbDelay = (uint8_t) reverbDelay;

	pNative->ReflectionsGain = pI3DL2->Reflections / 100.0f;
	pNative->ReverbGain = pI3DL2->Reverb / 100.0f;
	pNative->EarlyDiffusion = (uint8_t) (15.0f * pI3DL2->Diffusion / 100.0f);
	pNative->LateDiffusion = pNative->EarlyDiffusion;
	pNative->Density = pI3DL2->Density;
	pNative->RoomFilterFreq = pI3DL2->HFReference;

	pNative->WetDryMix = pI3DL2->WetDryMix;
}
