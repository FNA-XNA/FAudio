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

#ifndef FAPO_H
#define FAPO_H

#include "FAudio.h"

#define FAPOAPI FAUDIOAPI
#define FAPOCALL FAUDIOCALL

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Enumerations */

typedef enum FAPOBufferFlags
{
	FAPO_BUFFER_SILENT,
	FAPO_BUFFER_VALID
} FAPOBufferFlags;

/* Structures */

#pragma pack(push, 1)

typedef struct FAPORegistrationProperties
{
	FAudioGUID clsid;
	int16_t FriendlyName[256]; /* Win32 wchar_t */
	int16_t CopyrightInfo[256]; /* Win32 wchar_t */
	uint32_t MajorVersion;
	uint32_t MinorVersion;
	uint32_t Flags;
	uint32_t MinInputBufferCount;
	uint32_t MaxInputBufferCount;
	uint32_t MinOutputBufferCount;
	uint32_t MaxOutputBufferCount;
} FAPORegistrationProperties;

typedef struct FAPOLockForProcessBufferParameters
{
	const FAudioWaveFormatEx *pFormat;
	uint32_t MaxFrameCount;
} FAPOLockForProcessBufferParameters;

typedef struct FAPOProcessBufferParameters
{
	void* pBuffer;
	FAPOBufferFlags BufferFlags;
	uint32_t ValidFrameCount;
} FAPOProcessBufferParameters;

#pragma pack(pop)

/* Constants */

#define FAPO_MIN_CHANNELS 1
#define FAPO_MAX_CHANNELS 64

#define FAPO_MIN_FRAMERATE 1000
#define FAPO_MAX_FRAMERATE 200000

#define FAPO_REGISTRATION_STRING_LENGTH 256

#define FAPO_FLAG_CHANNELS_MUST_MATCH		0x00000001
#define FAPO_FLAG_FRAMERATE_MUST_MATCH		0x00000002
#define FAPO_FLAG_BITSPERSAMPLE_MUST_MATCH	0x00000004
#define FAPO_FLAG_BUFFERCOUNT_MUST_MATCH	0x00000008
#define FAPO_FLAG_INPLACE_REQUIRED		0x00000020
#define FAPO_FLAG_INPLACE_SUPPORTED		0x00000010

/* FAPO Interface */

typedef struct FAPO FAPO;

typedef uint32_t (FAPOCALL * GetRegistrationPropertiesFunc)(
	FAPO *fapo,
	FAPORegistrationProperties **ppRegistrationProperties
);
typedef uint32_t (FAPOCALL * IsInputFormatSupportedFunc)(
	FAPO *fapo,
	const FAudioWaveFormatEx *pOutputFormat,
	const FAudioWaveFormatEx *pRequestedInputFormat,
	FAudioWaveFormatEx **ppSupportedInputFormat
);
typedef uint32_t (FAPOCALL * IsOutputFormatSupportedFunc)(
	FAPO *fapo,
	const FAudioWaveFormatEx *pInputFormat,
	const FAudioWaveFormatEx *pRequestedOutputFormat,
	FAudioWaveFormatEx **ppSupportedOutputFormat
);
typedef uint32_t (FAPOCALL * InitializeFunc)(
	FAPO *fapo,
	const void* pData,
	uint32_t DataByteSize
);
typedef void (FAPOCALL * ResetFunc)(
	FAPO *fapo
);
typedef uint32_t (FAPOCALL * LockForProcessFunc)(
	FAPO *fapo,
	uint32_t InputLockedParameterCount,
	const FAPOLockForProcessBufferParameters *pInputLockedParameters,
	uint32_t OutputLockedParameterCount,
	const FAPOLockForProcessBufferParameters *pOutputLockedParameters
);
typedef void (FAPOCALL * UnlockForProcessFunc)(
	FAPO *fapo
);
typedef void (FAPOCALL * ProcessFunc)(
	FAPO *fapo,
	uint32_t InputProcessParameterCount,
	const FAPOProcessBufferParameters* pInputProcessParameters,
	uint32_t OutputProcessParameterCount,
	FAPOProcessBufferParameters* pOutputProcessParameters,
	uint8_t IsEnabled
);
typedef uint32_t (FAPOCALL * CalcInputFramesFunc)(
	FAPO *fapo,
	uint32_t OutputFrameCount
);
typedef uint32_t (FAPOCALL * CalcOutputFramesFunc)(
	FAPO *fapo,
	uint32_t InputFrameCount
);

struct FAPO
{
	GetRegistrationPropertiesFunc GetRegistrationProperties;
	IsInputFormatSupportedFunc IsInputFormatSupported;
	IsOutputFormatSupportedFunc IsOutputFormatSupported;
	InitializeFunc Initialize;
	ResetFunc Reset;
	LockForProcessFunc LockForProcess;
	UnlockForProcessFunc UnlockForProcess;
	ProcessFunc Process;
	CalcInputFramesFunc CalcInputFrames;
	CalcOutputFramesFunc CalcOutputFrames;
};

/* FAPOParameters Interface */

typedef struct FAPOParameters FAPOParameters;

typedef void (FAPOCALL * SetParametersFunc)(
	FAPOParameters *fapoParameters,
	const void* pParameters,
	uint32_t ParameterByteSize
);
typedef void (FAPOCALL * GetParametersFunc)(
	FAPOParameters *fapoParameters,
	const void* pParameters,
	uint32_t ParameterByteSize
);

struct FAPOParameters
{
	SetParametersFunc SetParameters;
	GetParametersFunc GetParameters;
};

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* FAPO_H */
