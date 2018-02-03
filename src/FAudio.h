/* FACT - XACT Reimplementation for FNA
 * Copyright 2009-2018 Ethan Lee and the MonoGame Team
 *
 * Released under the Microsoft Public License.
 * See LICENSE for details.
 */

#ifndef FAUDIO_H
#define FAUDIO_H

#ifdef _WIN32
#define FAUDIOAPI __declspec(dllexport)
#define FAUDIOCALL __cdecl
#else
#define FAUDIOAPI
#define FAUDIOCALL
#endif

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Type Declarations */

typedef struct FAudio FAudio;
typedef struct FAudioVoice FAudioVoice;
typedef FAudioVoice FAudioSourceVoice;
typedef FAudioVoice FAudioSubmixVoice;
typedef FAudioVoice FAudioMasteringVoice;
typedef struct FAudioEngineCallback FAudioEngineCallback;
typedef struct FAudioVoiceCallback FAudioVoiceCallback;

/* Enumerations */

typedef enum FAUDIO_DEVICE_ROLE
{
	NotDefaultDevice =		0x0,
	DefaultConsoleDevice =		0x1,
	DefaultMultimediaDevice =	0x2,
	DefaultCommunicationsDevice =	0x4,
	DefaultGameDevice =		0x8,
	GlobalDefaultDevice =		0xF,
	InvalidDeviceRole = ~GlobalDefaultDevice
} FAUDIO_DEVICE_ROLE;

typedef enum FAUDIO_FILTER_TYPE
{
	LowPassFilter,
	BandPassFilter,
	HighPassFilter,
	NotchFilter
} FAUDIO_FILTER_TYPE;

/* Structures */

typedef struct FAudioGUID
{
	uint32_t Data1;
	uint16_t Data2;
	uint16_t Data3;
	uint8_t Data4[8];
} FAudioGUID;

#pragma pack(push, 1)
typedef struct FAudioWaveFormatEx
{
	uint16_t wFormatTag;
	uint16_t nChannels;
	uint32_t nSamplesPerSec;
	uint32_t nAvgBytesPerSec;
	uint16_t nBlockAlign;
	uint16_t wBitsPerSample;
	uint16_t cbSize;
} FAudioWaveFormatEx;

typedef struct FAudioWaveFormatExtensible
{
	FAudioWaveFormatEx Format;
	union
	{
		uint16_t wValidBitsPerSample;
		uint16_t wSamplesPerBlock;
		uint16_t wReserved;
	} Samples;
	uint32_t dwChannelMask;
	FAudioGUID SubFormat;
} FAudioWaveFormatExtensible;
#pragma pack(pop)

/* FAudio Interface */

/* FIXME: Do we want to actually reproduce the COM stuff or what...? -flibit */
FAUDIOAPI uint32_t FAudioCreate(FAudio **ppFAudio);

/* FIXME: AddRef/Release? Or just ignore COM garbage... -flibit */

/* FAudioVoice Interface */

/* FAudioSourceVoice Interface */

/* FAudioEngineCallback Interface */

typedef void (FAUDIOCALL * OnCriticalErrorFunc)(
	FAudioEngineCallback *callback,
	uint32_t Error
);
typedef void (FAUDIOCALL * OnProcessingPassEndFunc)(
	FAudioEngineCallback *callback
);
typedef void (FAUDIOCALL * OnProcessingPassStartFunc)(
	FAudioEngineCallback *callback
);

struct FAudioEngineCallback
{
	OnCriticalErrorFunc OnCriticalError;
	OnProcessingPassEndFunc OnProcessingPassEnd;
	OnProcessingPassStartFunc OnProcessingPassStart;
};

/* FAudioVoiceCallback Interface */

typedef void (FAUDIOCALL * OnBufferEndFunc)(
	FAudioVoiceCallback *callback,
	void *pBufferContext
);
typedef void (FAUDIOCALL * OnBufferStartFunc)(
	FAudioVoiceCallback *callback,
	void *pBufferContext
);
typedef void (FAUDIOCALL * OnLoopEndFunc)(
	FAudioVoiceCallback *callback,
	void *pBufferContext
);
typedef void (FAUDIOCALL * OnStreamEndFunc)(
	FAudioVoiceCallback *callback
);
typedef void (FAUDIOCALL * OnVoiceErrorFunc)(
	FAudioVoiceCallback *callback,
	void *pBufferContext,
	uint32_t Error
);
typedef void (FAUDIOCALL * OnVoiceProcessingPassEndFunc)(
	FAudioVoiceCallback *callback
);
typedef void (FAUDIOCALL * OnVoiceProcessingPassStartFunc)(
	FAudioVoiceCallback *callback,
	uint32_t BytesRequired
);

struct FAudioVoiceCallback
{
	OnBufferEndFunc OnBufferEnd;
	OnBufferStartFunc OnBufferStart;
	OnLoopEndFunc OnLoopEnd;
	OnStreamEndFunc OnStreamEnd;
	OnVoiceErrorFunc OnVoiceError;
	OnVoiceProcessingPassEndFunc OnVoiceProcessingPassEnd;
	OnVoiceProcessingPassStartFunc OnVoiceProcessingPassStart;
};

/* Functions */

FAUDIOAPI uint32_t FAudioCreateReverb(void **ppApo, uint32_t Flags);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* FAUDIO_H */
