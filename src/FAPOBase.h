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

#ifndef FAPOBASE_H
#define FAPOBASE_H

#include "FAPO.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Constants */

#define FAPOBASE_DEFAULT_FORMAT_TAG		3 /* IEEE_FLOAT */
#define FAPOBASE_DEFAULT_FORMAT_MIN_CHANNELS	FAPO_MIN_CHANNELS
#define FAPOBASE_DEFAULT_FORMAT_MAX_CHANNELS	FAPO_MAX_CHANNELS
#define FAPOBASE_DEFAULT_FORMAT_MIN_FRAMERATE	FAPO_MIN_FRAMERATE
#define FAPOBASE_DEFAULT_FORMAT_MAX_FRAMERATE	FAPO_MAX_FRAMERATE
#define FAPOBASE_DEFAULT_FORMAT_BITSPERSAMPLE	32

#define FAPOBASE_DEFAULT_FLAG ( \
	FAPO_FLAG_CHANNELS_MUST_MATCH | \
	FAPO_FLAG_FRAMERATE_MUST_MATCH | \
	FAPO_FLAG_BITSPERSAMPLE_MUST_MATCH | \
	FAPO_FLAG_BUFFERCOUNT_MUST_MATCH | \
	FAPO_FLAG_INPLACE_SUPPORTED \
)

#define FAPOBASE_DEFAULT_BUFFER_COUNT 1

/* FAPOBase Interface */

#pragma pack(push, 8)
typedef struct FAPOBase
{
	/* Base Classes/Interfaces */
	FAPO base;
	void (FAPOCALL *Destructor)(void*);

	/* Private Variables */
	const FAPORegistrationProperties *m_pRegistrationProperties;
	void* m_pfnMatrixMixFunction;
	float *m_pfl32MatrixCoefficients;
	uint32_t m_nSrcFormatType;
	uint8_t m_fIsScalarMatrix;
	uint8_t m_fIsLocked;

	/* Protected Variables */
	int32_t m_lReferenceCount; /* LONG */
} FAPOBase;
#pragma pack(pop)

FAPOAPI void CreateFAPOBase(
	FAPOBase *fapo,
	const FAPORegistrationProperties *pRegistrationProperties
);

FAPOAPI int32_t FAPOBase_AddRef(FAPOBase *fapo);

FAPOAPI int32_t FAPOBase_Release(FAPOBase *fapo);

/* FIXME: QueryInterface? Or just ignore COM garbage... -flibit */

FAPOAPI uint32_t FAPOBase_GetRegistrationProperties(
	FAPOBase *fapo,
	FAPORegistrationProperties **ppRegistrationProperties
);

FAPOAPI uint32_t FAPOBase_IsInputFormatSupported(
	FAPOBase *fapo,
	const FAudioWaveFormatEx *pOutputFormat,
	const FAudioWaveFormatEx *pRequestedInputFormat,
	FAudioWaveFormatEx **ppSupportedInputFormat
);

FAPOAPI uint32_t FAPOBase_IsOutputFormatSupported(
	FAPOBase *fapo,
	const FAudioWaveFormatEx *pInputFormat,
	const FAudioWaveFormatEx *pRequestedOutputFormat,
	FAudioWaveFormatEx **ppSupportedOutputFormat
);

FAPOAPI uint32_t FAPOBase_Initialize(
	FAPOBase *fapo,
	const void* pData,
	uint32_t DataByteSize
);

FAPOAPI void FAPOBase_Reset(FAPOBase *fapo);

FAPOAPI uint32_t FAPOBase_LockForProcess(
	FAPOBase *fapo,
	uint32_t InputLockedParameterCount,
	const FAPOLockForProcessBufferParameters *pInputLockedParameters,
	uint32_t OutputLockedParameterCount,
	const FAPOLockForProcessBufferParameters *pOutputLockedParameters
);

FAPOAPI void FAPOBase_UnlockForProcess(FAPOBase *fapo);

FAPOAPI void FAPOBase_Process(
	FAPOBase *fapo,
	uint32_t InputProcessParameterCount,
	const FAPOProcessBufferParameters* pInputProcessParameters,
	uint32_t OutputProcessParameterCount,
	FAPOProcessBufferParameters* pOutputProcessParameters,
	uint8_t IsEnabled
);

FAPOAPI uint32_t FAPOBase_CalcInputFrames(
	FAPOBase *fapo,
	uint32_t OutputFrameCount
);

FAPOAPI uint32_t FAPOBase_CalcOutputFrames(
	FAPOBase *fapo,
	uint32_t InputFrameCount
);

FAPOAPI uint32_t FAPOBase_ValidateFormatDefault(
	FAPOBase *fapo,
	FAudioWaveFormatEx *pFormat,
	uint8_t fOverwrite
);

FAPOAPI uint32_t FAPOBase_ValidateFormatPair(
	const FAudioWaveFormatEx *pSupportedFormat,
	FAudioWaveFormatEx *pRequestedFormat,
	uint8_t fOverwrite
);

FAPOAPI void FAPOBase_ProcessThru(
	void* pInputBuffer,
	float *pOutputBuffer,
	uint32_t FrameCount,
	uint16_t InputChannelCount,
	uint16_t OutputChannelCount,
	uint8_t MixWithOutput
);

/* FAPOBaseParameters Interface */

typedef void (FAPOCALL * OnSetParametersFunc)(
	const void* parameters,
	uint32_t parametersSize
);

#pragma pack(push, 8)
typedef struct FAPOBaseParameters
{
	/* Base Classes/Interfaces */
	FAPOBase base;
	FAPOParameters parameters;

	/* Public Virtual Functions */
	OnSetParametersFunc OnSetParameters;

	/* Private Variables */
	uint8_t *m_pParameterBlocks;
	uint8_t *m_pCurrentParameters;
	uint8_t *m_pCurrentParametersInternal;
	uint32_t m_uCurrentParametersIndex;
	uint32_t m_uParameterBlockByteSize;
	uint8_t m_fNewerResultsReady;
	uint8_t m_fProducer;
} FAPOBaseParameters;
#pragma pack(pop)

FAPOAPI void CreateFAPOBaseParameters(
	FAPOBaseParameters *fapoParameters,
	const FAPORegistrationProperties *pRegistrationProperties
);

FAPOAPI int32_t FAPOBaseParameters_AddRef(FAPOBaseParameters *fapoParameters);

FAPOAPI int32_t FAPOBaseParameters_Release(FAPOBaseParameters *fapoParameters);

/* FIXME: QueryInterface? Or just ignore COM garbage... -flibit */

FAPOAPI void FAPOBaseParameters_SetParameters(
	FAPOBaseParameters *fapoParameters,
	const void* pParameters,
	uint32_t ParameterByteSize
);

FAPOAPI void FAPOBaseParameters_GetParameters(
	FAPOBaseParameters *fapoParameters,
	const void* pParameters,
	uint32_t ParameterByteSize
);

FAPOAPI void FAPOBaseParameters_OnSetParameters(
	const void* parameters,
	uint32_t parametersSize
);

FAPOAPI uint8_t FAPOBaseParameters_ParametersChanged(FAPOBaseParameters *fapoParameters);

FAPOAPI uint8_t* FAPOBaseParameters_BeginProcess(FAPOBaseParameters *fapoParameters);

FAPOAPI void FAPOBaseParameters_EndProcess(FAPOBaseParameters *fapoParameters);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* FAPOBASE_H */
