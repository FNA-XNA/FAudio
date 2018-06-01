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
#include "FAudioFX_internal.h"
#include "FAudio_internal.h"

/* Volume Meter Implementation */
static FAPORegistrationProperties VolumeMeterProperties =
{
	/* .clsid = */ {0},
	/* .FriendlyName = */
	{
		'V', 'o', 'l', 'u', 'm', 'e', 'M', 'e', 't', 'e', 'r', '\0'
	},
	/*.CopyrightInfo = */
	{
		'C', 'o', 'p', 'y', 'r', 'i', 'g', 'h', 't', ' ', '(', 'c', ')',
		'E', 't', 'h', 'a', 'n', ' ', 'L', 'e', 'e', '\0'
	},
	/*.MajorVersion = */ 0,
	/*.MinorVersion = */ 0,
	/*.Flags = */(
		FAPO_FLAG_FRAMERATE_MUST_MATCH |
		FAPO_FLAG_BITSPERSAMPLE_MUST_MATCH |
		FAPO_FLAG_BUFFERCOUNT_MUST_MATCH |
		FAPO_FLAG_INPLACE_SUPPORTED |
		FAPO_FLAG_INPLACE_REQUIRED
	),
	/*.MinInputBufferCount = */ 1,
	/*.MaxInputBufferCount = */  1,
	/*.MinOutputBufferCount = */ 1,
	/*.MaxOutputBufferCount =*/ 1
};

typedef struct FAudioFXVolumeMeter
{
	FAPOParametersBase base;

	/* TODO */
} FAudioFXVolumeMeter;

void FAudioFXVolumeMeter_Process(
	FAudioFXVolumeMeter *fapo,
	uint32_t InputProcessParameterCount,
	const FAPOProcessBufferParameters* pInputProcessParameters,
	uint32_t OutputProcessParameterCount,
	FAPOProcessBufferParameters* pOutputProcessParameters,
	uint8_t IsEnabled
) {
	FAudioFXVolumeMeterLevels *levels = (FAudioFXVolumeMeterLevels*)
		FAPOParametersBase_BeginProcess(&fapo->base);

	/* TODO */
	(void) levels;

	FAPOParametersBase_EndProcess(&fapo->base);
}

void FAudioFXVolumeMeter_Free(void* fapo)
{
	FAudio_free(fapo);
}

uint32_t FAudioCreateVolumeMeter(void** ppApo, uint32_t Flags)
{
	/* Allocate... */
	FAudioFXVolumeMeter *result = (FAudioFXVolumeMeter*) FAudio_malloc(
		sizeof(FAudioFXVolumeMeter)
	);

	/* Initialize... */
	CreateFAPOParametersBase(
		&result->base,
		&VolumeMeterProperties,
		NULL, /* FIXME */
		0, /* sizeof(FAudioFXVolumeMeterLevels), */
		1
	);

	/* Function table... */
	result->base.base.base.Process = (ProcessFunc)
		FAudioFXVolumeMeter_Process;
	result->base.base.Destructor = FAudioFXVolumeMeter_Free;

	/* Finally. */
	*ppApo = result;
	return 0;
}

/* Reverb Implementation */

static FAPORegistrationProperties ReverbProperties =
{
	/* .clsid = */ {0},
	/*.FriendlyName = */
	{
		'R', 'e', 'v', 'e', 'r', 'b', '\0'
	},
	/*.CopyrightInfo = */ {
		'C', 'o', 'p', 'y', 'r', 'i', 'g', 'h', 't', ' ', '(', 'c', ')',
		'E', 't', 'h', 'a', 'n', ' ', 'L', 'e', 'e', '\0'
	},
	/*.MajorVersion = */ 0,
	/*.MinorVersion = */ 0,
	/*.Flags = */ (
		FAPO_FLAG_FRAMERATE_MUST_MATCH |
		FAPO_FLAG_BITSPERSAMPLE_MUST_MATCH |
		FAPO_FLAG_BUFFERCOUNT_MUST_MATCH |
		FAPO_FLAG_INPLACE_SUPPORTED
	),
	/*.MinInputBufferCount = */ 1,
	/*.MaxInputBufferCount = */ 1,
	/*.MinOutputBufferCount = */ 1,
	/*.MaxOutputBufferCount = */ 1
};


/*

Room Reverb structure (loosely based on the reverberator from
"Designing Audio Effect Plug-Ins in C++" by Will Pirkle.

Only mono to mono in this prototype


                        Diffusion

In    +--------+     +----+    +----+              * g0
 +----+PreDelay+-----+APF1+----+APF2+-----------------------------+
      +--------+     +----+    +----+                             |
                                                                  |
 +----------------------------------------------------------------+
 |                                                                |
 |    +-----+        +--------+   * c0                            |
 +----+Delay+---+----+Comb1   +-----------+                       |
      +-----+   |    +--------+           |                       |
                |                         |                       |
                |    +--------+   * c1    |                       |
                +----+Comb2   +-----------+                       | 
                |    +--------+           |    +-----+  * g1      |
                |                         +----+ SUM +----------+ |
                |    +--------+   * cx    |    +-----+          | |
                +----+...     +-----------+                     | |
                |    +--------+           |                     | |
                |                         |                     | |
                |    +--------+   * c7    |                   +-+-+-+
                +----+Comb8   +-----------+                   | SUM |
                     +--------+                               +--+--+
                                                                 |
 +---------------------------------------------------------------+
 |
 |    +----+    +----+    +----+    +----+    * g2    +------------+     Out
 +----|APF3|----|APF4|----|APF5|----|APF6|------------+HighShelving+------>
      +----+    +----+    +----+    +----+            +------------+     

                                             +---- Room Damping ----+    


*/

typedef struct RoomReverb
{
	FAudioFXFilterDelay		*early_delay;
	FAudioFXFilterAllPass	*apf_in[2];

	FAudioFXFilterDelay		*reverb_delay;
	FAudioFXFilterDelay		*comb[2];
	FAudioFXFilterCombShelving	*lpf_comb[8];

	float					early_gain;
	float					reverb_gain;

	FAudioFXFilterBiQuad	*room_high_shelf;
	float					room_gain;

	FAudioFXFilterAllPass	*apf_out[4];

	float					wet_ratio;
	float					dry_ratio;

} RoomReverb;

typedef struct FAudioFXReverb
{
	FAPOParametersBase base;

	uint16_t inChannels;
	uint16_t outChannels;
	uint32_t sampleRate;

	RoomReverb network;
} FAudioFXReverb;


void RoomReverb_Create(FAudioFXReverb *fapo)
{
	fapo->network.early_delay = FAudioFXFilterDelay_Create(fapo->sampleRate, 10, 0.0f);

	fapo->network.apf_in[0] = FAudioFXFilterAllPass_Create(fapo->sampleRate, 10, 0.0f);
	fapo->network.apf_in[1] = FAudioFXFilterAllPass_Create(fapo->sampleRate, 10, 0.0f);

	fapo->network.reverb_delay = FAudioFXFilterDelay_Create(fapo->sampleRate, 10, 0.0f);
	fapo->network.comb[0] = FAudioFXFilterDelay_Create(fapo->sampleRate, 10, 0.0f);
	fapo->network.comb[1] = FAudioFXFilterDelay_Create(fapo->sampleRate, 10, 0.0f);
	
	for (int32_t i = 0; i < 8; ++i)
	{
		fapo->network.lpf_comb[i] = FAudioFXFilterCombShelving_Create(fapo->sampleRate, 10.0f, 100.0f);
	}

	fapo->network.early_gain = 1.0f;
	fapo->network.reverb_gain = 1.0f;

	fapo->network.room_high_shelf = FAudioFXFilterBiQuad_Create(fapo->sampleRate, FAUDIOFX_BIQUAD_HIGHSHELVING, 4000, 0, -10);
	fapo->network.apf_out[0] = FAudioFXFilterAllPass_Create(fapo->sampleRate, 13.28f, 0.5f);
	fapo->network.apf_out[1] = FAudioFXFilterAllPass_Create(fapo->sampleRate, 23.18f, 0.5f);
	fapo->network.apf_out[2] = FAudioFXFilterAllPass_Create(fapo->sampleRate, 23.18f, 0.5f);
	fapo->network.apf_out[3] = FAudioFXFilterAllPass_Create(fapo->sampleRate, 23.18f, 0.5f);
}

uint32_t FAudioFXReverb_LockForProcess(
	FAudioFXReverb *fapo,
	uint32_t InputLockedParameterCount,
	const FAPOLockForProcessBufferParameters *pInputLockedParameters,
	uint32_t OutputLockedParameterCount,
	const FAPOLockForProcessBufferParameters *pOutputLockedParameters
) {
	/* call	parent to do basic validation */
	uint32_t result = FAPOBase_LockForProcess(
		&fapo->base.base,
		InputLockedParameterCount,
		pInputLockedParameters,
		OutputLockedParameterCount,
		pOutputLockedParameters
	);

	if (result != 0)
	{
		return result;
	}

	/* FIXME: do advanced validation */

	/* save the things we care about */
	fapo->inChannels = pInputLockedParameters->pFormat->nChannels;
	fapo->outChannels = pOutputLockedParameters->pFormat->nChannels;
	fapo->sampleRate = pOutputLockedParameters->pFormat->nSamplesPerSec;

	/* create the network if necessary */
	if (fapo->network.early_delay == NULL) 
	{
		RoomReverb_Create(fapo);
	}

	return 0;
}

void FAudioFXReverb_Process(
	FAudioFXReverb *fapo,
	uint32_t InputProcessParameterCount,
	const FAPOProcessBufferParameters* pInputProcessParameters,
	uint32_t OutputProcessParameterCount,
	FAPOProcessBufferParameters* pOutputProcessParameters,
	uint8_t IsEnabled
) {
	if (IsEnabled == 0)
	{	
		/* FIXME: properly handle a disabled effect (check MSDN for what to do) */
		return;
	}

	/*FAudioFXReverbParameters *params = (FAudioFXReverbParameters*)
		FAPOParametersBase_BeginProcess(&fapo->base);*/
	FAudioFXReverbTestParameters *params = (FAudioFXReverbTestParameters*)
		FAPOParametersBase_BeginProcess(&fapo->base);

	const float *input_buffer = (const float *)pInputProcessParameters->pBuffer;
	float *output_buffer = (float *) pOutputProcessParameters->pBuffer;

	/* update parameters (FIXME: only if necessary?) */
	RoomReverb *reverb = &fapo->network;
	FAudioFXFilterDelay_Change(reverb->early_delay, (float) params->ReflectionsDelay, 0.0f);


	float early_diffusion = 0.6f - ((params->EarlyDiffusion / 15.0f) * 0.2f);
	FAudioFXFilterAllPass_Change(reverb->apf_in[0], params->InDiffusionLength1, early_diffusion);
	FAudioFXFilterAllPass_Change(reverb->apf_in[1], params->InDiffusionLength2, -early_diffusion);

	uint32_t delay = (params->ReverbDelay > params->ReflectionsDelay) ? params->ReverbDelay - params->ReflectionsDelay : params->ReverbDelay;
	FAudioFXFilterDelay_Change(reverb->reverb_delay, (float) delay, 0.0f);

	float comb_delays[] = { 25.31f, 26.94f, 28.96f, 30.75f, 32.24f, 33.8f, 35.31f, 36.67f };
	for (int32_t comb = 0; comb < 8; ++comb)
	{
		FAudioFXFilterCombShelving_Change(reverb->lpf_comb[comb], comb_delays[comb], params->DecayTime * 1000.0f * 0.2f);

		FAudioFXFilterBiQuad_Change(
			&reverb->lpf_comb[comb]->low_shelving,
			50.0f + params->LowEQCutoff * 50.0f,
			0.0f,
			params->LowEQGain - 8.0f);
		FAudioFXFilterBiQuad_Change(
			&reverb->lpf_comb[comb]->high_shelving,
			1000 + params->HighEQCutoff * 500.0f,
			0.0f,
			params->HighEQGain - 8.0f);
	}
	
	reverb->early_gain = FAudioFX_INTERNAL_DbGainToFactor(params->ReflectionsGain);
	reverb->reverb_gain = FAudioFX_INTERNAL_DbGainToFactor(params->ReverbGain);

	reverb->room_gain = FAudioFX_INTERNAL_DbGainToFactor(params->RoomFilterMain);
	FAudioFXFilterBiQuad_Change(fapo->network.room_high_shelf, params->RoomFilterFreq, 0.0f, params->RoomFilterMain + params->RoomFilterHF);

	float late_diffusion = 0.6f - ((params->LateDiffusion / 15.0f) * 0.2f);
	float allpass_delays[] = { 5.10f, 12.61f, 10.0f, 7.73f };

	for (int32_t ap = 0; ap < 4; ++ap)
	{
		FAudioFXFilterAllPass_Change(reverb->apf_out[ap], allpass_delays[ap], late_diffusion);
	}

	reverb->wet_ratio = params->WetDryMix / 100.0f;
	reverb->dry_ratio = 1.0f - reverb->wet_ratio;

	/* run the reverberation effect */
	for (size_t idx = 0; idx < pInputProcessParameters->ValidFrameCount; ++idx)
	{
		float in = input_buffer[idx];

		/* pre delay */
		float delay_in = FAudioFXFilterDelay_Process(reverb->early_delay, in);

		/* early reflections */
		float early = FAudioFXFilterAllPass_Process(reverb->apf_in[0], delay_in);
		early = FAudioFXFilterAllPass_Process(reverb->apf_in[0], early);

		early = early * reverb->early_gain;

		/* reverberation */
		float revdelay = FAudioFXFilterDelay_Process(reverb->reverb_delay, early);
		float comb_out = 0.0f;
		float gain = 0.125f;
		for (int32_t comb = 0; comb < 8; ++comb)
		{
			comb_out += gain * FAudioFXFilterCombShelving_Process(reverb->lpf_comb[comb], revdelay);
			gain = -gain;
		}

		/* combine early reflections and reverberation */
		float early_late = 0.5f * early + 0.5f * (reverb->reverb_gain * comb_out);
		
		/* output diffusion */
		float out = early_late;
		for (int32_t ap = 0; ap < 4; ++ap)
		{
			out = FAudioFXFilterAllPass_Process(reverb->apf_out[ap], out);
		}

		/* room filter */
		float room = out * reverb->room_gain;
		room = FAudioFXFilterBiQuad_Process(reverb->room_high_shelf, room);

		/* wet/dry mix */
		output_buffer[idx] = (room * reverb->wet_ratio) + (in * reverb->dry_ratio);
	}

	FAPOParametersBase_EndProcess(&fapo->base);
}

void FAudioFXReverb_Reset(FAudioFXReverb *fapo)
{
	FAPOBase_Reset(&fapo->base.base);

	// FIXME: figure out when this gets/should get called
}

void FAudioFXReverb_Free(void* fapo)
{
	FAudioFXReverb *reverb = (FAudioFXReverb*) fapo;
	FAudio_free(reverb->base.m_pParameterBlocks);
	FAudio_free(fapo);
}

uint32_t FAudioCreateReverb(void** ppApo, uint32_t Flags)
{
	/* Allocate... */
	FAudioFXReverb *result = (FAudioFXReverb*) FAudio_malloc(sizeof(FAudioFXReverb));
	uint8_t *params = (uint8_t*) FAudio_malloc(
		sizeof(FAudioFXReverbTestParameters) * 3
	);

	/* Initialize... */
	CreateFAPOParametersBase(
		&result->base,
		&ReverbProperties,
		params,
		sizeof(FAudioFXReverbTestParameters),
		0
	);

	FAudio_zero(&result->network, sizeof(result->network));

	/* Function table... */
	result->base.base.base.LockForProcess = (LockForProcessFunc)
		FAudioFXReverb_LockForProcess;
	result->base.base.base.Reset = (ResetFunc)
		FAudioFXReverb_Reset;
	result->base.base.base.Process = (ProcessFunc)
		FAudioFXReverb_Process;
	result->base.base.Destructor = FAudioFXReverb_Free;
	
	/* Finally. */
	*ppApo = result;
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
