/* FACT - XACT Reimplementation for FNA
 * Copyright 2009-2017 Ethan Lee and the MonoGame Team
 *
 * Released under the Microsoft Public License.
 * See LICENSE for details.
 */

#ifndef FACT_H
#define FACT_H

#ifdef _WIN32
#define FACTAPI __declspec(dllexport)
#define FACTCALL __stdcall
#else
#define FACTAPI
#define FACTCALL
#endif

#include <stdint.h>
#include <wchar.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Type Declarations */

typedef struct FACTAudioEngine FACTAudioEngine;
typedef struct FACTSoundBank FACTSoundBank;
typedef struct FACTWaveBank FACTWaveBank;
typedef struct FACTWave FACTWave;
typedef struct FACTCue FACTCue;

typedef struct FACTRendererDetails
{
	wchar_t rendererID[0xFF];
	wchar_t displayName[0xFF];
	int32_t defaultDevice;
} FACTRendererDetails;

typedef struct FACTGUID
{
	uint32_t Data1;
	uint16_t Data2;
	uint16_t Data3;
	uint8_t Data4[8];
} FACTGUID;

#pragma pack(push, 1)
typedef struct FACTWaveFormatEx
{
	uint16_t wFormatTag;
	uint16_t nChannels;
	uint32_t nSamplesPerSec;
	uint32_t nAvgBytesPerSec;
	uint16_t nBlockAlign;
	uint16_t wBitsPerSample;
	uint16_t cbSize;
} FACTWaveFormatEx;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct FACTWaveFormatExtensible
{
	FACTWaveFormatEx Format;
	union
	{
		uint16_t wValidBitsPerSample;
		uint16_t wSamplesPerBlock;
		uint16_t wReserved;
	} Samples;
	uint32_t dwChannelMask;
	FACTGUID SubFormat;
} FACTWaveFormatExtensible;
#pragma pack(pop)

typedef struct FACTOverlapped
{
	void *Internal; /* ULONG_PTR */
	void *InternalHigh; /* ULONG_PTR */
	union
	{
		struct
		{
			uint32_t Offset;
			uint32_t OffsetHigh;
		};
		void *Pointer;
	};
	void *hEvent;
} FACTOverlapped;

typedef int32_t (FACTCALL * FACTReadFileCallback)(
	void *hFile,
	void *buffer,
	uint32_t nNumberOfBytesToRead,
	uint32_t *lpNumberOfBytesRead,
	FACTOverlapped *lpOverlapped
);

typedef int32_t (FACTCALL * FACTGetOverlappedResultCallback)(
	void *hFile,
	FACTOverlapped *lpOverlapped,
	int32_t bWait
);

typedef struct FACTFileIOCallbacks
{
	FACTReadFileCallback readFileCallback;
	FACTGetOverlappedResultCallback getOverlappedResultCallback;
} FACTFileIOCallbacks;

typedef struct FACTNotification
{
	uint8_t type;
	int32_t timeStamp;
	void *pvContext;
	union
	{
		uint32_t filler; /* TODO: Notifications */
	};
} FACTNotification;

typedef struct FACTNotificationDescription
{
	uint8_t type;
	uint8_t flags;
	FACTSoundBank *pSoundBank;
	FACTWaveBank *pWaveBank;
	FACTCue *pCue;
	FACTWave *pWave;
	uint16_t cueIndex;
	uint16_t waveIndex;
	void *pvContext;
} FACTNotificationDescription;

typedef void (FACTCALL * FACTNotificationCallback)(
	const FACTNotification *pNotification
);

typedef struct FACTRuntimeParameters
{
	uint32_t lookAheadTime;
	void *pGlobalSettingsBuffer;
	uint32_t globalSettingsBufferSize;
	uint32_t globalSettingsFlags;
	uint32_t globalSettingsAllocAttributes;
	FACTFileIOCallbacks fileIOCallbacks;
	FACTNotificationCallback fnNotificationCallback;
	wchar_t *pRendererID;
	void *pXAudio2;
	void *pMasteringVoice;
} FACTRuntimeParameters;

typedef struct FACTStreamingParameters
{
	void *file;
	uint32_t offset;
	uint32_t flags;
	uint16_t packetSize;
} FACTStreamingParameters;

typedef union FACTWaveBankMiniWaveFormat
{
	struct
	{
		uint32_t wFormatTag : 2;
		uint32_t nChannels : 3;
		uint32_t nSamplesPerSec : 18;
		uint32_t wBlockAlign : 8;
		uint32_t wBitsPerSample : 1;
	};
	uint32_t dwValue;
} FACTWaveBankMiniWaveFormat;

typedef struct FACTWaveBankRegion
{
	uint32_t dwOffset;
	uint32_t dwLength;
} FACTWaveBankRegion;

typedef struct FACTWaveBankSampleRegion
{
	uint32_t dwStartSample;
	uint32_t dwTotalSamples;
} FACTWaveBankSampleRegion;

typedef struct FACTWaveBankEntry
{
	union
	{
		struct
		{
			uint32_t dwFlags : 4;
			uint32_t Duration : 28;
		};
		uint32_t dwFlagsAndDuration;
	};
	FACTWaveBankMiniWaveFormat Format;
	FACTWaveBankRegion PlayRegion;
	FACTWaveBankSampleRegion LoopRegion;
} FACTWaveBankEntry;

typedef struct FACTWaveProperties
{
	char friendlyName[64];
	FACTWaveBankMiniWaveFormat format;
	uint32_t durationInSamples;
	FACTWaveBankSampleRegion loopRegion;
	int32_t streaming;
} FACTWaveProperties;

typedef struct FACTWaveInstanceProperties
{
	FACTWaveProperties properties;
	int32_t backgroundMusic;
} FACTWaveInstanceProperties;

typedef struct FACTCueProperties
{
	char friendlyName[0xFF];
	int32_t interactive;
	uint16_t iaVariableIndex;
	uint16_t numVariations;
	uint8_t maxInstances;
	uint8_t currentInstances;
} FACTCueProperties;

typedef struct FACTTrackProperties
{
	uint32_t duration;
	uint16_t numVariations;
	uint8_t numChannels;
	uint16_t waveVariation;
	uint8_t loopCount;
} FACTTrackProperties;

typedef struct FACTVariationProperties
{
	uint16_t index;
	uint8_t weight;
	float iaVariableMin;
	float iaVariableMax;
	int32_t linger;
} FACTVariationProperties;

typedef struct FACTSoundProperties
{
	uint16_t category;
	uint8_t priority;
	int16_t pitch;
	float volume;
	uint16_t numTracks;
	FACTTrackProperties arrTrackProperties[1];
} FACTSoundProperties;

typedef struct FACTSoundVariationProperties
{
	FACTVariationProperties variationProperties;
	FACTSoundProperties soundProperties;
} FACTSoundVariationProperties;

typedef struct FACTCueInstanceProperties
{
	uint32_t allocAttributes;
	FACTCueProperties cueProperties;
	FACTSoundVariationProperties activeVariationProperties;
} FACTCueInstanceProperties;

/* Constants */

#define FACT_CONTENT_VERSION 46

static const uint32_t FACT_FLAG_MANAGEDATA =		0x00000001;

static const uint32_t FACT_FLAG_STOP_RELEASE =		0x00000000;
static const uint32_t FACT_FLAG_STOP_IMMEDIATE =	0x00000001;

static const uint32_t FACT_STATE_CREATED =		0x00000001;
static const uint32_t FACT_STATE_PREPARING =		0x00000002;
static const uint32_t FACT_STATE_PREPARED =		0x00000004;
static const uint32_t FACT_STATE_PLAYING =		0x00000008;
static const uint32_t FACT_STATE_STOPPING =		0x00000010;
static const uint32_t FACT_STATE_STOPPED =		0x00000020;
static const uint32_t FACT_STATE_PAUSED =		0x00000040;
static const uint32_t FACT_STATE_INUSE =		0x00000080;
static const uint32_t FACT_STATE_PREPAREFAILED =	0x80000000;

/* AudioEngine Interface */

/* FIXME: Do we want to actually reproduce the COM stuff or what...? -flibit */
FACTAPI uint32_t FACTCreateEngine(
	uint32_t dwCreationFlags,
	FACTAudioEngine **ppEngine
);

/* FIXME: AddRef/Release? Or just ignore COM garbage... -flibit */

FACTAPI uint32_t FACTAudioEngine_GetRendererCount(
	FACTAudioEngine *pEngine,
	uint16_t *pnRendererCount
);

FACTAPI uint32_t FACTAudioEngine_GetRendererDetails(
	FACTAudioEngine *pEngine,
	uint16_t nRendererIndex,
	FACTRendererDetails *pRendererDetails
);

FACTAPI uint32_t FACTAudioEngine_GetFinalMixFormat(
	FACTAudioEngine *pEngine,
	FACTWaveFormatExtensible *pFinalMixFormat
);

FACTAPI uint32_t FACTAudioEngine_Initialize(
	FACTAudioEngine *pEngine,
	const FACTRuntimeParameters *pParams
);

FACTAPI uint32_t FACTAudioEngine_Shutdown(FACTAudioEngine *pEngine);

FACTAPI uint32_t FACTAudioEngine_DoWork(FACTAudioEngine *pEngine);

FACTAPI uint32_t FACTAudioEngine_CreateSoundBank(
	FACTAudioEngine *pEngine,
	const void *pvBuffer,
	uint32_t dwSize,
	uint32_t dwFlags,
	uint32_t dwAllocAttributes,
	FACTSoundBank **ppSoundBank
);

FACTAPI uint32_t FACTAudioEngine_CreateInMemoryWaveBank(
	FACTAudioEngine *pEngine,
	const void *pvBuffer,
	uint32_t dwSize,
	uint32_t dwFlags,
	uint32_t dwAllocAttributes,
	FACTWaveBank **ppWaveBank
);

FACTAPI uint32_t FACTAudioEngine_CreateStreamingWaveBank(
	FACTAudioEngine *pEngine,
	const FACTStreamingParameters *pParms,
	FACTWaveBank **ppWaveBank
);

FACTAPI uint32_t FACTAudioEngine_PrepareWave(
	FACTAudioEngine *pEngine,
	uint32_t dwFlags,
	const char *szWavePath,
	uint32_t wStreamingPacketSize,
	uint32_t dwAlignment,
	uint32_t dwPlayOffset,
	uint8_t nLoopCount,
	FACTWave **ppWave
);

FACTAPI uint32_t FACTAudioEngine_PrepareInMemoryWave(
	FACTAudioEngine *pEngine,
	uint32_t dwFlags,
	FACTWaveBankEntry entry,
	uint32_t *pdwSeekTable, /* Optional! */
	uint8_t *pbWaveData,
	uint32_t dwPlayOffset,
	uint8_t nLoopCount,
	FACTWave **ppWave
);

FACTAPI uint32_t FACTAudioEngine_PrepareStreamingWave(
	FACTAudioEngine *pEngine,
	uint32_t dwFlags,
	FACTWaveBankEntry entry,
	FACTStreamingParameters streamingParams,
	uint32_t dwAlignment,
	uint32_t *pdwSeekTable, /* Optional! */
	uint8_t *pbWaveData,
	uint32_t dwPlayOffset,
	uint8_t nLoopCount,
	FACTWave **ppWave
);

FACTAPI uint32_t FACTAudioEngine_RegisterNotification(
	FACTAudioEngine *pEngine,
	const FACTNotificationDescription *pNotificationDescription
);

FACTAPI uint32_t FACTAudioEngine_UnRegisterNotification(
	FACTAudioEngine *pEngine,
	const FACTNotificationDescription *pNotificationDescription
);

FACTAPI uint16_t FACTAudioEngine_GetCategory(
	FACTAudioEngine *pEngine,
	const char *szFriendlyName
);

FACTAPI uint32_t FACTAudioEngine_Stop(
	FACTAudioEngine *pEngine,
	uint16_t nCategory,
	uint32_t dwFlags
);

FACTAPI uint32_t FACTAudioEngine_SetVolume(
	FACTAudioEngine *pEngine,
	uint16_t nCategory,
	uint32_t dwFlags
);

FACTAPI uint32_t FACTAudioEngine_Pause(
	FACTAudioEngine *pEngine,
	uint16_t nCategory,
	int32_t fPause
);

FACTAPI uint16_t FACTAudioEngine_GetGlobalVariableIndex(
	FACTAudioEngine *pEngine,
	const char *szFriendlyName
);

FACTAPI uint32_t FACTAudioEngine_SetGlobalVariable(
	FACTAudioEngine *pEngine,
	uint16_t nIndex,
	float nValue
);

FACTAPI uint32_t FACTAudioEngine_GetGlobalVariable(
	FACTAudioEngine *pEngine,
	uint16_t nIndex,
	float *pnValue
);

/* SoundBank Interface */

FACTAPI uint16_t FACTSoundBank_GetCueIndex(
	FACTSoundBank *pSoundBank,
	const char *szFriendlyName
);

FACTAPI uint32_t FACTSoundBank_GetNumCues(
	FACTSoundBank *pSoundBank,
	uint16_t *pnNumCues
);

FACTAPI uint32_t FACTSoundBank_GetCueProperties(
	FACTSoundBank *pSoundBank,
	uint16_t nCueIndex,
	FACTCueProperties *pProperties
);

FACTAPI uint32_t FACTSoundBank_Prepare(
	FACTSoundBank *pSoundBank,
	uint16_t nCueIndex,
	uint32_t dwFlags,
	int32_t timeOffset,
	FACTCue** ppCue
);

FACTAPI uint32_t FACTSoundBank_Play(
	FACTSoundBank *pSoundBank,
	uint16_t nCueIndex,
	uint32_t dwFlags,
	int32_t timeOffset,
	FACTCue** ppCue /* Optional! */
);

FACTAPI uint32_t FACTSoundBank_Stop(
	FACTSoundBank *pSoundBank,
	uint16_t nCueIndex,
	uint32_t dwFlags
);

FACTAPI uint32_t FACTSoundBank_Destroy(FACTSoundBank *pSoundBank);

FACTAPI uint32_t FACTSoundBank_GetState(
	FACTSoundBank *pSoundBank,
	uint32_t *pdwState
);

/* WaveBank Interface */

FACTAPI uint32_t FACTWaveBank_Destroy(FACTWaveBank *pWaveBank);

FACTAPI uint32_t FACTWaveBank_GetState(
	FACTWaveBank *pWaveBank,
	uint32_t *pdwState
);

FACTAPI uint32_t FACTWaveBank_GetNumWaves(
	FACTWaveBank *pWaveBank,
	uint16_t pnNumWaves
);

FACTAPI uint16_t FACTWaveBank_GetWaveIndex(
	FACTWaveBank *pWaveBank,
	const char *szFriendlyName
);

FACTAPI uint32_t FACTWaveBank_GetWaveProperties(
	FACTWaveBank *pWaveBank,
	uint16_t nWaveIndex,
	FACTWaveProperties *pWaveProperties
);

FACTAPI uint32_t FACTWaveBank_Prepare(
	FACTWaveBank *pWaveBank,
	uint16_t nWaveIndex,
	uint32_t dwFlags,
	uint32_t dwPlayOffset,
	uint8_t nLoopCount,
	FACTWave **ppWave
);

FACTAPI uint32_t FACTWaveBank_Play(
	FACTWaveBank *pWaveBank,
	uint16_t nWaveIndex,
	uint32_t dwFlags,
	uint32_t dwPlayOffset,
	uint8_t nLoopCount,
	FACTWave **ppWave
);

FACTAPI uint32_t FACTWaveBank_Stop(
	FACTWaveBank *pWaveBank,
	uint16_t nWaveIndex,
	uint32_t dwFlags
);

/* Wave Interface */

FACTAPI uint32_t FACTWave_Destroy(FACTWave *pWave);

FACTAPI uint32_t FACTWave_Play(FACTWave *pWave);

FACTAPI uint32_t FACTWave_Stop(FACTWave *pWave, uint32_t dwFlags);

FACTAPI uint32_t FACTWave_Pause(FACTWave *pWave, int32_t fPause);

FACTAPI uint32_t FACTWave_GetState(FACTWave *pWave, uint32_t *pdwState);

FACTAPI uint32_t FACTWave_SetPitch(FACTWave *pWave, int16_t pitch);

FACTAPI uint32_t FACTWave_SetVolume(FACTWave *pWave, float volume);

FACTAPI uint32_t FACTWave_SetMatrixCoefficients(
	FACTWave *pWave,
	uint32_t uSrcChannelCount,
	uint32_t uDstChannelCount,
	float *pMatrixCoefficients
);

FACTAPI uint32_t FACTWave_GetProperties(
	FACTWave *pWave,
	FACTWaveInstanceProperties *pProperties
);

/* Cue Interface */

FACTAPI uint32_t FACTCue_Destroy(FACTCue *pCue);

FACTAPI uint32_t FACTCue_Play(FACTCue *pCue);

FACTAPI uint32_t FACTCue_Stop(FACTCue *pCue, uint32_t dwFlags);

FACTAPI uint32_t FACTCue_GetState(FACTCue *pCue, uint32_t *pdwState);

FACTAPI uint32_t FACTCue_SetMatrixCoefficients(
	FACTCue *pCue,
	uint32_t uSrcChannelCount,
	uint32_t uDstChannelCount,
	float *pMatrixCoefficients
);

FACTAPI uint16_t FACTCue_GetVariableIndex(
	FACTCue *pCue,
	const char *szFriendlyName
);

FACTAPI uint32_t FACTCue_SetVariable(
	FACTCue *pCue,
	uint16_t nIndex,
	float nValue
);

FACTAPI uint32_t FACTCue_GetVariable(
	FACTCue *pCue,
	uint16_t nIndex,
	float *nValue
);

FACTAPI uint32_t FACTCue_Pause(FACTCue *pCue, int32_t fPause);

FACTAPI uint32_t FACTCue_GetProperties(
	FACTCue *pCue,
	FACTCueInstanceProperties *ppProperties
);

/* FIXME: Can we reproduce these two functions...? -flibit */

FACTAPI uint32_t FACTCue_SetOutputVoices(
	FACTCue *pCue,
	const void *pSendList /* Optional XAUDIO2_VOICE_SENDS */
);

FACTAPI uint32_t FACTCue_SetOutputVoiceMatrix(
	FACTCue *pCue,
	const void *pDestinationVoice, /* Optional IXAudio2Voice */
	uint32_t SourceChannels,
	uint32_t DestinationChannels,
	const float *pLevelMatrix /* SourceChannels * DestinationChannels */
);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* FACT_H */
