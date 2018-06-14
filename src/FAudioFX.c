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

typedef struct FAudioFXReverb
{
	FAPOParametersBase base;

	uint16_t inChannels;
	uint16_t outChannels;
	uint32_t sampleRate;
	uint16_t inBlockAlign;
	uint16_t outBlockAlign;

	DspReverb *reverb;
} FAudioFXReverb;

static inline int8_t IsFloatFormat(const FAudioWaveFormatEx *format)
{
	if (format->wFormatTag == 3)
	{
		/* Plain ol' WaveFormatEx */
		return 1;
	}

	if (format->wFormatTag == 0xFFFE)
	{
		/* WaveFormatExtensible, match GUID */
#define MAKE_SUBFORMAT_GUID(guid, fmt)	static FAudioGUID KSDATAFORMAT_SUBTYPE_##guid = {(uint16_t)(fmt), 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}}
		MAKE_SUBFORMAT_GUID(IEEE_FLOAT, 3);
#undef MAKE_SUBFORMAT_GUID

		if (FAudio_memcmp(&((FAudioWaveFormatExtensible *)format)->SubFormat, &KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, sizeof(FAudioGUID)) == 0)
		{
			return 1;
		}
	}

	return 0;
}

uint32_t FAudioFXReverb_IsInputFormatSupported(
	FAPOBase *fapo,
	const FAudioWaveFormatEx *pOutputFormat,
	const FAudioWaveFormatEx *pRequestedInputFormat,
	FAudioWaveFormatEx **ppSupportedInputFormat
) {
	uint32_t result = 0;

#define SET_SUPPORTED_FIELD(field, value)	\
	result = 1;	\
	if (ppSupportedInputFormat && *ppSupportedInputFormat)	\
	{	\
		(*ppSupportedInputFormat)->field = (value);	\
	}

	/* sample rate */
	if (pOutputFormat->nSamplesPerSec != pRequestedInputFormat->nSamplesPerSec)
	{
		SET_SUPPORTED_FIELD(nSamplesPerSec, pOutputFormat->nSamplesPerSec);
	}

	/* data type */
	if (!IsFloatFormat(pRequestedInputFormat))
	{
		SET_SUPPORTED_FIELD(wFormatTag, 3);
	}

	/* number of input / output channels */
	if (pOutputFormat->nChannels == 1 || pOutputFormat->nChannels == 2)
	{
		if (pRequestedInputFormat->nChannels != pOutputFormat->nChannels)
		{
			SET_SUPPORTED_FIELD(nChannels, pOutputFormat->nChannels);
		}
	}
	else if (pOutputFormat->nChannels == 6)
	{
		if (pRequestedInputFormat->nChannels != 1 || pRequestedInputFormat->nChannels != 2)
		{
			SET_SUPPORTED_FIELD(nChannels, 1);
		}
	}
	else
	{
		SET_SUPPORTED_FIELD(nChannels, 1);
		result = 1;
	}

#undef SET_SUPPORTED_FIELD

	return result;
}


uint32_t FAudioFXReverb_IsOutputFormatSupported(
	FAPOBase *fapo,
	const FAudioWaveFormatEx *pInputFormat,
	const FAudioWaveFormatEx *pRequestedOutputFormat,
	FAudioWaveFormatEx **ppSupportedOutputFormat
) {
	uint32_t result = 0;

#define SET_SUPPORTED_FIELD(field, value)	\
	result = 1;	\
	if (ppSupportedOutputFormat && *ppSupportedOutputFormat)	\
	{	\
		(*ppSupportedOutputFormat)->field = (value);	\
	}

	/* sample rate */
	if (pInputFormat->nSamplesPerSec != pRequestedOutputFormat->nSamplesPerSec)
	{
		SET_SUPPORTED_FIELD(nSamplesPerSec, pInputFormat->nSamplesPerSec);
	}

	/* data type */
	if (!IsFloatFormat(pRequestedOutputFormat))
	{
		SET_SUPPORTED_FIELD(wFormatTag, 3);
	}

	/* number of input / output channels */
	if (pInputFormat->nChannels == 1 || pInputFormat->nChannels == 2)
	{
		if (pRequestedOutputFormat->nChannels != pInputFormat->nChannels)
		{
			SET_SUPPORTED_FIELD(nChannels, pInputFormat->nChannels);
		}
	}
	else if (pInputFormat->nChannels == 6)
	{
		if (pRequestedOutputFormat->nChannels != 1 || pRequestedOutputFormat->nChannels != 2)
		{
			SET_SUPPORTED_FIELD(nChannels, 1);
		}
	}
	else
	{
		SET_SUPPORTED_FIELD(nChannels, 1);
		result = 1;
	}

#undef SET_SUPPORTED_FIELD

	return result;
}

uint32_t FAudioFXReverb_LockForProcess(
	FAudioFXReverb *fapo,
	uint32_t InputLockedParameterCount,
	const FAPOLockForProcessBufferParameters *pInputLockedParameters,
	uint32_t OutputLockedParameterCount,
	const FAPOLockForProcessBufferParameters *pOutputLockedParameters
) {
	/* reverb specific validation */
	if (!IsFloatFormat(pInputLockedParameters->pFormat))
	{
		return 1;
	}

	if (pInputLockedParameters->pFormat->nSamplesPerSec < FAUDIOFX_REVERB_MIN_FRAMERATE ||
		pInputLockedParameters->pFormat->nSamplesPerSec > FAUDIOFX_REVERB_MAX_FRAMERATE)
	{
		return 1;
	}

	if (!((pInputLockedParameters->pFormat->nChannels == 1 &&
			(pOutputLockedParameters->pFormat->nChannels == 1 ||
			 pOutputLockedParameters->pFormat->nChannels == 6)) ||
		  (pInputLockedParameters->pFormat->nChannels == 2 &&
			(pOutputLockedParameters->pFormat->nChannels == 2 ||
			 pOutputLockedParameters->pFormat->nChannels == 6))))
	{
		return 1;
	}

	/* save the things we care about */
	fapo->inChannels = pInputLockedParameters->pFormat->nChannels;
	fapo->outChannels = pOutputLockedParameters->pFormat->nChannels;
	fapo->sampleRate = pOutputLockedParameters->pFormat->nSamplesPerSec;
	fapo->inBlockAlign = pInputLockedParameters->pFormat->nBlockAlign;
	fapo->outBlockAlign = pOutputLockedParameters->pFormat->nBlockAlign;

	/* create the network if necessary */
	if (fapo->reverb == NULL) 
	{
		fapo->reverb = DspReverb_Create(fapo->sampleRate, fapo->inChannels, fapo->outChannels);
	}

	/* call	parent to do basic validation */
	return FAPOBase_LockForProcess(
		&fapo->base.base,
		InputLockedParameterCount,
		pInputLockedParameters,
		OutputLockedParameterCount,
		pOutputLockedParameters
	);
}

static inline void FAudioFXReverb_CopyBuffer(FAudioFXReverb *fapo, const float *buffer_in, float *buffer_out, size_t frames_in)
{
	/* in-place processing ? */
	if (buffer_in == buffer_out)
	{
		return;
	}
	
	/* 1 -> 1 or 2 -> 2 */
	if (fapo->inBlockAlign == fapo->outBlockAlign)
	{
		FAudio_memcpy(buffer_out, buffer_in, fapo->inBlockAlign * frames_in);
		return;
	}

	/* 1 -> 5.1 */
	FAudio_zero(buffer_out, fapo->outBlockAlign * frames_in);

	if (fapo->inChannels == 1 && fapo->outChannels == 6)
	{
		const float *in_end = buffer_in + frames_in;
		const float *in_ptr = buffer_in;
		float *out_ptr = buffer_out;
		
		while (in_ptr < in_end)
		{
			*out_ptr++ = *in_ptr;
			*out_ptr++ = *in_ptr++;
			out_ptr += 4;
		}
		return;
	}

	/* 2 -> 5.1 */
	if (fapo->inChannels == 2 && fapo->outChannels == 6)
	{
		const float *in_end = buffer_in + (frames_in * 2);
		const float *in_ptr = buffer_in;
		float *out_ptr = buffer_out;

		while (in_ptr < in_end)
		{
			*out_ptr++ = *in_ptr++;
			*out_ptr++ = *in_ptr++;
			out_ptr += 4;
		}
		return;
	}

	FAudio_assert(0 && "Unsupported channel combination");
}

void FAudioFXReverb_Process(
	FAudioFXReverb *fapo,
	uint32_t InputProcessParameterCount,
	const FAPOProcessBufferParameters* pInputProcessParameters,
	uint32_t OutputProcessParameterCount,
	FAPOProcessBufferParameters* pOutputProcessParameters,
	uint8_t IsEnabled
) {
	FAudioFXReverbParameters *params;
	uint8_t update_params = FAPOParametersBase_ParametersChanged(&fapo->base);
	float total;
	
	/* handle disabled filter */
	if (IsEnabled == 0) 
	{
		pOutputProcessParameters->BufferFlags = pInputProcessParameters->BufferFlags;

		if (pOutputProcessParameters->BufferFlags != FAPO_BUFFER_SILENT)
		{
			FAudioFXReverb_CopyBuffer(
				fapo,
				(const float *)pInputProcessParameters->pBuffer,
				(float *)pOutputProcessParameters->pBuffer,
				pInputProcessParameters->ValidFrameCount
			);
		}

		return;
	}
	
	/* XAudio2 passes a 'silent' buffer when no input buffer is available to play the effect tail */
	if (pInputProcessParameters->BufferFlags == FAPO_BUFFER_SILENT)
	{
		/* make sure input data is usable */
		FAudio_zero(
			pInputProcessParameters->pBuffer, 
			pInputProcessParameters->ValidFrameCount * fapo->inBlockAlign
		);
	}

	params = (FAudioFXReverbParameters*) FAPOParametersBase_BeginProcess(&fapo->base);

	/* update parameters  */
	if (update_params)
	{
		DspReverb_SetParameters(fapo->reverb, params);
	}

	/* run reverb effect */
	total = DspReverb_Process(
		fapo->reverb,
		(const float *)pInputProcessParameters->pBuffer,
		(float *)pOutputProcessParameters->pBuffer,
		pInputProcessParameters->ValidFrameCount * fapo->inChannels,
		fapo->inChannels
	);

	/* set BufferFlags to silent so PLAY_TAILS knows when to stop */
	pOutputProcessParameters->BufferFlags = (total < 0.0000001f) ? FAPO_BUFFER_SILENT : FAPO_BUFFER_VALID;

	FAPOParametersBase_EndProcess(&fapo->base);
}

void FAudioFXReverb_Reset(FAudioFXReverb *fapo)
{
	FAPOBase_Reset(&fapo->base.base);

	/* reset the cached state of the reverb filter */
	DspReverb_Reset(fapo->reverb);
}

void FAudioFXReverb_Free(void* fapo)
{
	FAudioFXReverb *reverb = (FAudioFXReverb*) fapo;
	DspReverb_Destroy(reverb->reverb);
	FAudio_free(reverb->base.m_pParameterBlocks);
	FAudio_free(fapo);
}

uint32_t FAudioCreateReverb(void** ppApo, uint32_t Flags)
{
	/* Allocate... */
	FAudioFXReverb *result = (FAudioFXReverb*) FAudio_malloc(sizeof(FAudioFXReverb));
	uint8_t *params = (uint8_t*) FAudio_malloc(
		sizeof(FAudioFXReverbParameters) * 3
	);

	/* Initialize... */
	CreateFAPOParametersBase(
		&result->base,
		&ReverbProperties,
		params,
		sizeof(FAudioFXReverbParameters),
		0
	);

	result->inChannels = 0;
	result->outChannels = 0;
	result->sampleRate = 0;
	result->reverb = NULL;

	/* Function table... */
	#define ASSIGN_VT(name) \
		result->base.base.base.name = (name##Func) FAudioFXReverb_##name;
	ASSIGN_VT(LockForProcess);
	ASSIGN_VT(IsInputFormatSupported);
	ASSIGN_VT(IsOutputFormatSupported);
	ASSIGN_VT(Reset);
	ASSIGN_VT(Process);
	result->base.base.Destructor = FAudioFXReverb_Free;
	#undef ASSIGN_VT

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
