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
	ASSIGN_VT(Process)
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
		ppRegistrationProperties,
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
	/* TODO */
	return 0;
}

uint32_t FAPOBase_IsOutputFormatSupported(
	FAPOBase *fapo,
	const FAudioWaveFormatEx *pInputFormat,
	const FAudioWaveFormatEx *pRequestedOutputFormat,
	FAudioWaveFormatEx **ppSupportedOutputFormat
) {
	/* TODO */
	return 0;
}

uint32_t FAPOBase_Initialize(
	FAPOBase *fapo,
	const void* pData,
	uint32_t DataByteSize
) {
	/* TODO */
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
	/* TODO */
	return 0;
}

void FAPOBase_UnlockForProcess(FAPOBase *fapo)
{
	/* TODO */
}

void FAPOBase_Process(
	FAPOBase *fapo,
	uint32_t InputProcessParameterCount,
	const FAPOProcessBufferParameters* pInputProcessParameters,
	uint32_t OutputProcessParameterCount,
	FAPOProcessBufferParameters* pOutputProcessParameters,
	uint8_t IsEnabled
) {
	/* TODO */
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
	/* TODO */
	return 0;
}

uint32_t FAPOBase_ValidateFormatPair(
	const FAudioWaveFormatEx *pSupportedFormat,
	FAudioWaveFormatEx *pRequestedFormat,
	uint8_t fOverwrite
) {
	/* TODO */
	return 0;
}

void FAPOBase_ProcessThru(
	void* pInputBuffer,
	float *pOutputBuffer,
	uint32_t FrameCount,
	uint16_t InputChannelCount,
	uint16_t OutputChannelCount,
	uint8_t MixWithOutput
) {
	/* TODO */
}

/* FAPOBaseParameters Interface */

void CreateFAPOBaseParameters(
	FAPOBaseParameters *fapoParameters,
	const FAPORegistrationProperties *pRegistrationProperties
) {
	/* Base Classes/Interfaces */
	CreateFAPOBase(&fapoParameters->base, pRegistrationProperties);
	#define ASSIGN_VT(name) \
		fapoParameters->parameters.name = (name##Func) FAPOBaseParameters_##name;
	ASSIGN_VT(SetParameters)
	ASSIGN_VT(GetParameters)
	#undef ASSIGN_VT

	/* Public Virtual Functions */
	fapoParameters->OnSetParameters = (OnSetParametersFunc)
		FAPOBaseParameters_OnSetParameters;

	/* Private Variables */
	fapoParameters->m_pParameterBlocks = NULL; /* FIXME */
	fapoParameters->m_pCurrentParameters = NULL; /* FIXME */
	fapoParameters->m_pCurrentParametersInternal = NULL; /* FIXME */
	fapoParameters->m_uCurrentParametersIndex = 0; /* FIXME */
	fapoParameters->m_uParameterBlockByteSize = 0; /* FIXME */
	fapoParameters->m_fNewerResultsReady = 0; /* FIXME */
	fapoParameters->m_fProducer = 0; /* FIXME */
}

int32_t FAPOBaseParameters_AddRef(FAPOBaseParameters *fapoParameters)
{
	return FAPOBase_AddRef(&fapoParameters->base);
}

int32_t FAPOBaseParameters_Release(FAPOBaseParameters *fapoParameters)
{
	return FAPOBase_Release(&fapoParameters->base);
}

/* FIXME: QueryInterface? Or just ignore COM garbage... -flibit */

void FAPOBaseParameters_SetParameters(
	FAPOBaseParameters *fapoParameters,
	const void* pParameters,
	uint32_t ParameterByteSize
) {
	/* TODO */
}

void FAPOBaseParameters_GetParameters(
	FAPOBaseParameters *fapoParameters,
	const void* pParameters,
	uint32_t ParameterByteSize
) {
	/* TODO */
}

void FAPOBaseParameters_OnSetParameters(
	const void* parameters,
	uint32_t parametersSize
) {
}

uint8_t FAPOBaseParameters_ParametersChanged(FAPOBaseParameters *fapoParameters)
{
	/* TODO */
	return 0;
}

uint8_t* FAPOBaseParameters_BeginProcess(FAPOBaseParameters *fapoParameters)
{
	/* TODO */
	return NULL;
}

void FAPOBaseParameters_EndProcess(FAPOBaseParameters *fapoParameters)
{
	/* TODO */
}
