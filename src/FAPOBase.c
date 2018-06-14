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

#include "FAPOBase.h"
#include "FAudio_internal.h"

/* FAPOBase Interface */

void CreateFAPOBase(
	FAPOBase *fapo,
	const FAPORegistrationProperties *pRegistrationProperties
) {
	/* Base Classes/Interfaces */
	#define ASSIGN_VT(name) \
		fapo->base.name = (name##Func) FAPOBase_##name;
	ASSIGN_VT(GetRegistrationProperties)
	ASSIGN_VT(IsInputFormatSupported)
	ASSIGN_VT(IsOutputFormatSupported)
	ASSIGN_VT(Initialize)
	ASSIGN_VT(Reset)
	ASSIGN_VT(LockForProcess)
	ASSIGN_VT(UnlockForProcess)
	ASSIGN_VT(CalcInputFrames)
	ASSIGN_VT(CalcOutputFrames)
	#undef ASSIGN_VT

	/* Private Variables */
	fapo->m_pRegistrationProperties = pRegistrationProperties; /* FIXME */
	fapo->m_pfnMatrixMixFunction = NULL; /* FIXME */
	fapo->m_pfl32MatrixCoefficients = NULL; /* FIXME */
	fapo->m_nSrcFormatType = 0; /* FIXME */
	fapo->m_fIsScalarMatrix = 0; /* FIXME: */
	fapo->m_fIsLocked = 0;

	/* Protected Variables */
	fapo->m_lReferenceCount = 1;
}

int32_t FAPOBase_AddRef(FAPOBase *fapo)
{
	fapo->m_lReferenceCount += 1;
	return fapo->m_lReferenceCount;
}

int32_t FAPOBase_Release(FAPOBase *fapo)
{
	fapo->m_lReferenceCount -= 1;
	if (fapo->m_lReferenceCount == 0)
	{
		fapo->Destructor(fapo);
		return 0;
	}
	return fapo->m_lReferenceCount;
}

/* FIXME: QueryInterface? Or just ignore COM garbage... -flibit */

uint32_t FAPOBase_GetRegistrationProperties(
	FAPOBase *fapo,
	FAPORegistrationProperties **ppRegistrationProperties
) {
	FAudio_memcpy(
		*ppRegistrationProperties,
		fapo->m_pRegistrationProperties,
		sizeof(FAPORegistrationProperties)
	);
	return 0;
}

uint32_t FAPOBase_IsInputFormatSupported(
	FAPOBase *fapo,
	const FAudioWaveFormatEx *pOutputFormat,
	const FAudioWaveFormatEx *pRequestedInputFormat,
	FAudioWaveFormatEx **ppSupportedInputFormat
) {
	if (	pRequestedInputFormat->wFormatTag != FAPOBASE_DEFAULT_FORMAT_TAG ||
		pRequestedInputFormat->nChannels < FAPOBASE_DEFAULT_FORMAT_MIN_CHANNELS ||
		pRequestedInputFormat->nChannels > FAPOBASE_DEFAULT_FORMAT_MAX_CHANNELS ||
		pRequestedInputFormat->nSamplesPerSec < FAPOBASE_DEFAULT_FORMAT_MIN_FRAMERATE ||
		pRequestedInputFormat->nSamplesPerSec > FAPOBASE_DEFAULT_FORMAT_MAX_FRAMERATE ||
		pRequestedInputFormat->wBitsPerSample != FAPOBASE_DEFAULT_FORMAT_BITSPERSAMPLE	)
	{
		if (ppSupportedInputFormat != NULL)
		{
			(*ppSupportedInputFormat)->wFormatTag =
				FAPOBASE_DEFAULT_FORMAT_TAG;
			(*ppSupportedInputFormat)->nChannels = FAudio_clamp(
				pRequestedInputFormat->nChannels,
				FAPOBASE_DEFAULT_FORMAT_MIN_CHANNELS,
				FAPOBASE_DEFAULT_FORMAT_MAX_CHANNELS
			);
			(*ppSupportedInputFormat)->nSamplesPerSec = FAudio_clamp(
				pRequestedInputFormat->nSamplesPerSec,
				FAPOBASE_DEFAULT_FORMAT_MIN_FRAMERATE,
				FAPOBASE_DEFAULT_FORMAT_MAX_FRAMERATE
			);
			(*ppSupportedInputFormat)->wBitsPerSample =
				FAPOBASE_DEFAULT_FORMAT_BITSPERSAMPLE;
		}
		return 1;
	}
	return 0;
}

uint32_t FAPOBase_IsOutputFormatSupported(
	FAPOBase *fapo,
	const FAudioWaveFormatEx *pInputFormat,
	const FAudioWaveFormatEx *pRequestedOutputFormat,
	FAudioWaveFormatEx **ppSupportedOutputFormat
) {
	if (	pRequestedOutputFormat->wFormatTag != FAPOBASE_DEFAULT_FORMAT_TAG ||
		pRequestedOutputFormat->nChannels < FAPOBASE_DEFAULT_FORMAT_MIN_CHANNELS ||
		pRequestedOutputFormat->nChannels > FAPOBASE_DEFAULT_FORMAT_MAX_CHANNELS ||
		pRequestedOutputFormat->nSamplesPerSec < FAPOBASE_DEFAULT_FORMAT_MIN_FRAMERATE ||
		pRequestedOutputFormat->nSamplesPerSec > FAPOBASE_DEFAULT_FORMAT_MAX_FRAMERATE ||
		pRequestedOutputFormat->wBitsPerSample != FAPOBASE_DEFAULT_FORMAT_BITSPERSAMPLE	)
	{
		if (ppSupportedOutputFormat != NULL)
		{
			(*ppSupportedOutputFormat)->wFormatTag =
				FAPOBASE_DEFAULT_FORMAT_TAG;
			(*ppSupportedOutputFormat)->nChannels = FAudio_clamp(
				pRequestedOutputFormat->nChannels,
				FAPOBASE_DEFAULT_FORMAT_MIN_CHANNELS,
				FAPOBASE_DEFAULT_FORMAT_MAX_CHANNELS
			);
			(*ppSupportedOutputFormat)->nSamplesPerSec = FAudio_clamp(
				pRequestedOutputFormat->nSamplesPerSec,
				FAPOBASE_DEFAULT_FORMAT_MIN_FRAMERATE,
				FAPOBASE_DEFAULT_FORMAT_MAX_FRAMERATE
			);
			(*ppSupportedOutputFormat)->wBitsPerSample =
				FAPOBASE_DEFAULT_FORMAT_BITSPERSAMPLE;
		}
		return 1;
	}
	return 0;
}

uint32_t FAPOBase_Initialize(
	FAPOBase *fapo,
	const void* pData,
	uint32_t DataByteSize
) {
	return 0;
}

void FAPOBase_Reset(FAPOBase *fapo)
{
}

uint32_t FAPOBase_LockForProcess(
	FAPOBase *fapo,
	uint32_t InputLockedParameterCount,
	const FAPOLockForProcessBufferParameters *pInputLockedParameters,
	uint32_t OutputLockedParameterCount,
	const FAPOLockForProcessBufferParameters *pOutputLockedParameters
) {
	/* Verify parameter counts... */
	if (	InputLockedParameterCount < fapo->m_pRegistrationProperties->MinInputBufferCount ||
		InputLockedParameterCount > fapo->m_pRegistrationProperties->MaxInputBufferCount ||
		OutputLockedParameterCount < fapo->m_pRegistrationProperties->MinOutputBufferCount ||
		OutputLockedParameterCount > fapo->m_pRegistrationProperties->MaxOutputBufferCount	)
	{
		return 1;
	}


	/* Validate input/output formats */
	#define VERIFY_FORMAT_FLAG(flag, prop) \
		if (	(fapo->m_pRegistrationProperties->Flags & flag) && \
			(pInputLockedParameters->pFormat->prop != pOutputLockedParameters->pFormat->prop)	) \
		{ \
			return 1; \
		}
	VERIFY_FORMAT_FLAG(FAPO_FLAG_CHANNELS_MUST_MATCH, nChannels)
	VERIFY_FORMAT_FLAG(FAPO_FLAG_FRAMERATE_MUST_MATCH, nSamplesPerSec)
	VERIFY_FORMAT_FLAG(FAPO_FLAG_BITSPERSAMPLE_MUST_MATCH, wBitsPerSample)
	#undef VERIFY_FORMAT_FLAG
	if (	(fapo->m_pRegistrationProperties->Flags & FAPO_FLAG_BUFFERCOUNT_MUST_MATCH) &&
		(InputLockedParameterCount != OutputLockedParameterCount)	)
	{
		return 1;
	}
	fapo->m_fIsLocked = 1;
	return 0;
}

void FAPOBase_UnlockForProcess(FAPOBase *fapo)
{
	fapo->m_fIsLocked = 0;
}

uint32_t FAPOBase_CalcInputFrames(FAPOBase *fapo, uint32_t OutputFrameCount)
{
	return OutputFrameCount;
}

uint32_t FAPOBase_CalcOutputFrames(FAPOBase *fapo, uint32_t InputFrameCount)
{
	return InputFrameCount;
}

uint32_t FAPOBase_ValidateFormatDefault(
	FAPOBase *fapo,
	FAudioWaveFormatEx *pFormat,
	uint8_t fOverwrite
) {
	if (	pFormat->wFormatTag != FAPOBASE_DEFAULT_FORMAT_TAG ||
		pFormat->nChannels < FAPOBASE_DEFAULT_FORMAT_MIN_CHANNELS ||
		pFormat->nChannels > FAPOBASE_DEFAULT_FORMAT_MAX_CHANNELS ||
		pFormat->nSamplesPerSec < FAPOBASE_DEFAULT_FORMAT_MIN_FRAMERATE ||
		pFormat->nSamplesPerSec > FAPOBASE_DEFAULT_FORMAT_MAX_FRAMERATE ||
		pFormat->wBitsPerSample != FAPOBASE_DEFAULT_FORMAT_BITSPERSAMPLE	)
	{
		if (fOverwrite)
		{
			pFormat->wFormatTag =
				FAPOBASE_DEFAULT_FORMAT_TAG;
			pFormat->nChannels = FAudio_clamp(
				pFormat->nChannels,
				FAPOBASE_DEFAULT_FORMAT_MIN_CHANNELS,
				FAPOBASE_DEFAULT_FORMAT_MAX_CHANNELS
			);
			pFormat->nSamplesPerSec = FAudio_clamp(
				pFormat->nSamplesPerSec,
				FAPOBASE_DEFAULT_FORMAT_MIN_FRAMERATE,
				FAPOBASE_DEFAULT_FORMAT_MAX_FRAMERATE
			);
			pFormat->wBitsPerSample =
				FAPOBASE_DEFAULT_FORMAT_BITSPERSAMPLE;
		}
		return 1;
	}
	return 0;
}

uint32_t FAPOBase_ValidateFormatPair(
	FAPOBase *fapo,
	const FAudioWaveFormatEx *pSupportedFormat,
	FAudioWaveFormatEx *pRequestedFormat,
	uint8_t fOverwrite
) {
	if (	pRequestedFormat->wFormatTag != FAPOBASE_DEFAULT_FORMAT_TAG ||
		pRequestedFormat->nChannels < FAPOBASE_DEFAULT_FORMAT_MIN_CHANNELS ||
		pRequestedFormat->nChannels > FAPOBASE_DEFAULT_FORMAT_MAX_CHANNELS ||
		pRequestedFormat->nSamplesPerSec < FAPOBASE_DEFAULT_FORMAT_MIN_FRAMERATE ||
		pRequestedFormat->nSamplesPerSec > FAPOBASE_DEFAULT_FORMAT_MAX_FRAMERATE ||
		pRequestedFormat->wBitsPerSample != FAPOBASE_DEFAULT_FORMAT_BITSPERSAMPLE	)
	{
		if (fOverwrite)
		{
			pRequestedFormat->wFormatTag =
				FAPOBASE_DEFAULT_FORMAT_TAG;
			pRequestedFormat->nChannels = FAudio_clamp(
				pRequestedFormat->nChannels,
				FAPOBASE_DEFAULT_FORMAT_MIN_CHANNELS,
				FAPOBASE_DEFAULT_FORMAT_MAX_CHANNELS
			);
			pRequestedFormat->nSamplesPerSec = FAudio_clamp(
				pRequestedFormat->nSamplesPerSec,
				FAPOBASE_DEFAULT_FORMAT_MIN_FRAMERATE,
				FAPOBASE_DEFAULT_FORMAT_MAX_FRAMERATE
			);
			pRequestedFormat->wBitsPerSample =
				FAPOBASE_DEFAULT_FORMAT_BITSPERSAMPLE;
		}
		return 1;
	}
	return 0;
}

void FAPOBase_ProcessThru(
	FAPOBase *fapo,
	void* pInputBuffer,
	float *pOutputBuffer,
	uint32_t FrameCount,
	uint16_t InputChannelCount,
	uint16_t OutputChannelCount,
	uint8_t MixWithOutput
) {
	uint32_t i, co, ci;
	float *input = (float*) pInputBuffer;

	if (MixWithOutput)
	{
		/* TODO: SSE */
		for (i = 0; i < FrameCount; i += 1)
		for (co = 0; co < OutputChannelCount; co += 1)
		for (ci = 0; ci < InputChannelCount; ci += 1)
		{
			/* Add, don't overwrite! */
			pOutputBuffer[i * OutputChannelCount + co] +=
				input[i * InputChannelCount + ci];
			pOutputBuffer[i * OutputChannelCount + co] = FAudio_clamp(
				pOutputBuffer[i * OutputChannelCount + co],
				-FAUDIO_MAX_VOLUME_LEVEL,
				FAUDIO_MAX_VOLUME_LEVEL
			);
		}
	}
	else
	{
		/* TODO: SSE */
		for (i = 0; i < FrameCount; i += 1)
		for (co = 0; co < OutputChannelCount; co += 1)
		for (ci = 0; ci < InputChannelCount; ci += 1)
		{
			/* Overwrite, don't add! */
			pOutputBuffer[i * OutputChannelCount + co] =
				input[i * InputChannelCount + ci];
			pOutputBuffer[i * OutputChannelCount + co] = FAudio_clamp(
				pOutputBuffer[i * OutputChannelCount + co],
				-FAUDIO_MAX_VOLUME_LEVEL,
				FAUDIO_MAX_VOLUME_LEVEL
			);
		}
	}
}

/* FAPOParametersBase Interface */

void CreateFAPOParametersBase(
	FAPOParametersBase *fapoParameters,
	const FAPORegistrationProperties *pRegistrationProperties,
	uint8_t *pParameterBlocks,
	uint32_t uParameterBlockByteSize,
	uint8_t fProducer
) {
	/* Base Classes/Interfaces */
	CreateFAPOBase(&fapoParameters->base, pRegistrationProperties);
	#define ASSIGN_VT(name) \
		fapoParameters->parameters.name = (name##Func) FAPOParametersBase_##name;
	ASSIGN_VT(SetParameters)
	ASSIGN_VT(GetParameters)
	#undef ASSIGN_VT

	/* Public Virtual Functions */
	fapoParameters->OnSetParameters = (OnSetParametersFunc)
		FAPOParametersBase_OnSetParameters;

	/* Private Variables */
	fapoParameters->m_pParameterBlocks = pParameterBlocks;
	fapoParameters->m_pCurrentParameters = pParameterBlocks;
	fapoParameters->m_pCurrentParametersInternal = pParameterBlocks;
	fapoParameters->m_uCurrentParametersIndex = 0;
	fapoParameters->m_uParameterBlockByteSize = uParameterBlockByteSize;
	fapoParameters->m_fNewerResultsReady = 0;
	fapoParameters->m_fProducer = fProducer;
}

int32_t FAPOParametersBase_AddRef(FAPOParametersBase *fapoParameters)
{
	return FAPOBase_AddRef(&fapoParameters->base);
}

int32_t FAPOParametersBase_Release(FAPOParametersBase *fapoParameters)
{
	return FAPOBase_Release(&fapoParameters->base);
}

/* FIXME: QueryInterface? Or just ignore COM garbage... -flibit */

void FAPOParametersBase_SetParameters(
	FAPOParametersBase *fapoParameters,
	const void* pParameters,
	uint32_t ParameterByteSize
) {
	FAudio_assert(!fapoParameters->m_fProducer);

	/* User callback for validation */
	fapoParameters->OnSetParameters(
		fapoParameters,
		pParameters,
		ParameterByteSize
	);

	/* Increment parameter block index... */
	fapoParameters->m_uCurrentParametersIndex += 1;
	if (fapoParameters->m_uCurrentParametersIndex == 3)
	{
		fapoParameters->m_uCurrentParametersIndex = 0;
	}
	fapoParameters->m_pCurrentParametersInternal = fapoParameters->m_pParameterBlocks + (
		fapoParameters->m_uParameterBlockByteSize *
		fapoParameters->m_uCurrentParametersIndex
	);

	/* Copy to what will eventually be the next parameter update */
	FAudio_memcpy(
		fapoParameters->m_pCurrentParametersInternal,
		pParameters,
		ParameterByteSize
	);
}

void FAPOParametersBase_GetParameters(
	FAPOParametersBase *fapoParameters,
	void* pParameters,
	uint32_t ParameterByteSize
) {
	/* Copy what's current as of the last Process */
	FAudio_memcpy(
		pParameters,
		fapoParameters->m_pCurrentParameters,
		ParameterByteSize
	);
}

void FAPOParametersBase_OnSetParameters(
	FAPOParametersBase *fapoParameters,
	const void* parameters,
	uint32_t parametersSize
) {
}

uint8_t FAPOParametersBase_ParametersChanged(FAPOParametersBase *fapoParameters)
{
	/* Internal will get updated when SetParameters is called */
	return fapoParameters->m_pCurrentParametersInternal != fapoParameters->m_pCurrentParameters;
}

uint8_t* FAPOParametersBase_BeginProcess(FAPOParametersBase *fapoParameters)
{
	/* Set the latest block as "current", this is what Process will use now */
	fapoParameters->m_pCurrentParameters = fapoParameters->m_pCurrentParametersInternal;
	return fapoParameters->m_pCurrentParameters;
}

void FAPOParametersBase_EndProcess(FAPOParametersBase *fapoParameters)
{
	/* I'm 100% sure my parameter block increment is wrong... */
}
