/* FACT - XACT Reimplementation for FNA
 * Copyright 2009-2018 Ethan Lee and the MonoGame Team
 *
 * Released under the Microsoft Public License.
 * See LICENSE for details.
 */

#ifndef FACT_H
#define FACT_H

#ifdef _WIN32
#define FACTAPI __declspec(dllexport)
#define FACTCALL __cdecl
#else
#define FACTAPI
#define FACTCALL
#endif

#include <stdint.h>
#include <stddef.h>

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
	int16_t rendererID[0xFF]; /* Win32 wchar_t */
	int16_t displayName[0xFF]; /* Win32 wchar_t */
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
	int16_t *pRendererID; /* Win32 wchar_t* */
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

#define FACT_WAVEBANK_TYPE_BUFFER		0x00000000
#define FACT_WAVEBANK_TYPE_STREAMING		0x00000001
#define FACT_WAVEBANK_TYPE_MASK			0x00000001

#define FACT_WAVEBANK_FLAGS_ENTRYNAMES		0x00010000
#define FACT_WAVEBANK_FLAGS_COMPACT		0x00020000
#define FACT_WAVEBANK_FLAGS_SYNC_DISABLED	0x00040000
#define FACT_WAVEBANK_FLAGS_SEEKTABLES		0x00080000
#define FACT_WAVEBANK_FLAGS_MASK		0x000F0000

typedef enum FACTWaveBankSegIdx
{
	FACT_WAVEBANK_SEGIDX_BANKDATA = 0,
	FACT_WAVEBANK_SEGIDX_ENTRYMETADATA,
	FACT_WAVEBANK_SEGIDX_SEEKTABLES,
	FACT_WAVEBANK_SEGIDX_ENTRYNAMES,
	FACT_WAVEBANK_SEGIDX_ENTRYWAVEDATA,
	FACT_WAVEBANK_SEGIDX_COUNT
} FACTWaveBankSegIdx;

#pragma pack(push, 1)

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

typedef struct FACTWaveBankHeader
{
	uint32_t dwSignature;
	uint32_t dwVersion;
	uint32_t dwHeaderVersion;
	FACTWaveBankRegion Segments[FACT_WAVEBANK_SEGIDX_COUNT];
} FACTWaveBankHeader;

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

typedef struct FACTWaveBankEntryCompact
{
	uint32_t dwOffset : 21;
	uint32_t dwLengthDeviation : 11;
} FACTWaveBankEntryCompact;

typedef struct FACTWaveBankData
{
	uint32_t dwFlags;
	uint32_t dwEntryCount;
	char szBankName[64];
	uint32_t dwEntryMetaDataElementSize;
	uint32_t dwEntryNameElementSize;
	uint32_t dwAlignment;
	FACTWaveBankMiniWaveFormat CompactFormat;
	uint64_t BuildTime;
} FACTWaveBankData;

#pragma pack(pop)

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

static const uint32_t FACT_FLAG_BACKGROUND_MUSIC =	0x00000002;
static const uint32_t FACT_FLAG_UNITS_MS =		0x00000004;
static const uint32_t FACT_FLAG_UNITS_SAMPLES =		0x00000008;

static const uint32_t FACT_STATE_CREATED =		0x00000001;
static const uint32_t FACT_STATE_PREPARING =		0x00000002;
static const uint32_t FACT_STATE_PREPARED =		0x00000004;
static const uint32_t FACT_STATE_PLAYING =		0x00000008;
static const uint32_t FACT_STATE_STOPPING =		0x00000010;
static const uint32_t FACT_STATE_STOPPED =		0x00000020;
static const uint32_t FACT_STATE_PAUSED =		0x00000040;
static const uint32_t FACT_STATE_INUSE =		0x00000080;
static const uint32_t FACT_STATE_PREPAREFAILED =	0x80000000;

static const int16_t FACTPITCH_MIN =		-1200;
static const int16_t FACTPITCH_MAX =		 1200;
static const int16_t FACTPITCH_MIN_TOTAL =	-2400;
static const int16_t FACTPITCH_MAX_TOTAL =	 2400;

static const float FACTVOLUME_MIN = 0.0f;
static const float FACTVOLUME_MAX = 16777216.0f;

static const uint16_t FACTINDEX_INVALID =		0xFFFF;
static const uint16_t FACTVARIABLEINDEX_INVALID =	0xFFFF;
static const uint16_t FACTCATEGORY_INVALID =		0xFFFF;

#define FACT_ENGINE_LOOKAHEAD_DEFAULT 250

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
	float volume
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

typedef struct FACT3DAUDIO_DSP_SETTINGS FACT3DAUDIO_DSP_SETTINGS;

FACTAPI uint32_t FACTSoundBank_Play3D(
	FACTSoundBank *pSoundBank,
	uint16_t nCueIndex,
	uint32_t dwFlags,
	int32_t timeOffset,
	FACT3DAUDIO_DSP_SETTINGS *pDSPSettings,
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
	uint16_t *pnNumWaves
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

/* 3D Audio API */

#define FACT3DAUDIO_PI			3.141592654f
#define FACT3DAUDIO_2PI			6.283185307f

#define LEFT_AZIMUTH			(3.0f * FACT3DAUDIO_PI / 2.0f)
#define RIGHT_AZIMUTH			(FACT3DAUDIO_PI / 2.0f)
#define FRONT_LEFT_AZIMUTH		(7.0f * FACT3DAUDIO_PI / 4.0f)
#define FRONT_RIGHT_AZIMUTH		(FACT3DAUDIO_PI / 4.0f)
#define FRONT_CENTER_AZIMUTH		0.0f
#define LOW_FREQUENCY_AZIMUTH		FACT3DAUDIO_2PI
#define BACK_LEFT_AZIMUTH		(5.0f * FACT3DAUDIO_PI / 4.0f)
#define BACK_RIGHT_AZIMUTH		(3.0f * FACT3DAUDIO_PI / 4.0f)
#define BACK_CENTER_AZIMUTH		FACT3DAUDIO_PI
#define FRONT_LEFT_OF_CENTER_AZIMUTH	(15.0f * FACT3DAUDIO_PI / 8.0f)
#define FRONT_RIGHT_OF_CENTER_AZIMUTH	(FACT3DAUDIO_PI / 8.0f)

#define SPEAKER_FRONT_LEFT		0x00000001
#define SPEAKER_FRONT_RIGHT		0x00000002
#define SPEAKER_FRONT_CENTER		0x00000004
#define SPEAKER_LOW_FREQUENCY		0x00000008
#define SPEAKER_BACK_LEFT		0x00000010
#define SPEAKER_BACK_RIGHT		0x00000020
#define SPEAKER_FRONT_LEFT_OF_CENTER	0x00000040
#define SPEAKER_FRONT_RIGHT_OF_CENTER	0x00000080
#define SPEAKER_BACK_CENTER		0x00000100
#define SPEAKER_SIDE_LEFT		0x00000200
#define SPEAKER_SIDE_RIGHT		0x00000400
#define SPEAKER_TOP_CENTER		0x00000800
#define SPEAKER_TOP_FRONT_LEFT		0x00001000
#define SPEAKER_TOP_FRONT_CENTER	0x00002000
#define SPEAKER_TOP_FRONT_RIGHT		0x00004000
#define SPEAKER_TOP_BACK_LEFT		0x00008000
#define SPEAKER_TOP_BACK_CENTER		0x00010000
#define SPEAKER_TOP_BACK_RIGHT		0x00020000

#define SPEAKER_MONO	SPEAKER_FRONT_CENTER
#define SPEAKER_STEREO	(SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT)
#define SPEAKER_2POINT1 \
	(	SPEAKER_FRONT_LEFT	| \
		SPEAKER_FRONT_RIGHT	| \
		SPEAKER_LOW_FREQUENCY	)
#define SPEAKER_SURROUND \
	(	SPEAKER_FRONT_LEFT	| \
		SPEAKER_FRONT_RIGHT	| \
		SPEAKER_FRONT_CENTER	| \
		SPEAKER_BACK_CENTER	)
#define SPEAKER_QUAD \
	(	SPEAKER_FRONT_LEFT	| \
		SPEAKER_FRONT_RIGHT	| \
		SPEAKER_BACK_LEFT	| \
		SPEAKER_BACK_RIGHT	)
#define SPEAKER_4POINT1 \
	(	SPEAKER_FRONT_LEFT	| \
		SPEAKER_FRONT_RIGHT	| \
		SPEAKER_LOW_FREQUENCY	| \
		SPEAKER_BACK_LEFT	| \
		SPEAKER_BACK_RIGHT	)
#define SPEAKER_5POINT1 \
	(	SPEAKER_FRONT_LEFT	| \
		SPEAKER_FRONT_RIGHT	| \
		SPEAKER_FRONT_CENTER	| \
		SPEAKER_LOW_FREQUENCY	| \
		SPEAKER_BACK_LEFT	| \
		SPEAKER_BACK_RIGHT	)
#define SPEAKER_7POINT1 \
	(	SPEAKER_FRONT_LEFT		| \
		SPEAKER_FRONT_RIGHT		| \
		SPEAKER_FRONT_CENTER		| \
		SPEAKER_LOW_FREQUENCY		| \
		SPEAKER_BACK_LEFT		| \
		SPEAKER_BACK_RIGHT		| \
		SPEAKER_FRONT_LEFT_OF_CENTER	| \
		SPEAKER_FRONT_RIGHT_OF_CENTER	)
#define SPEAKER_5POINT1_SURROUND \
	(	SPEAKER_FRONT_LEFT	| \
		SPEAKER_FRONT_RIGHT	| \
		SPEAKER_FRONT_CENTER	| \
		SPEAKER_LOW_FREQUENCY	| \
		SPEAKER_SIDE_LEFT	| \
		SPEAKER_SIDE_RIGHT	)
#define SPEAKER_7POINT1_SURROUND \
	(	SPEAKER_FRONT_LEFT	| \
		SPEAKER_FRONT_RIGHT	| \
		SPEAKER_FRONT_CENTER	| \
		SPEAKER_LOW_FREQUENCY	| \
		SPEAKER_BACK_LEFT	| \
		SPEAKER_BACK_RIGHT	| \
		SPEAKER_SIDE_LEFT	| \
		SPEAKER_SIDE_RIGHT	)
#define SPEAKER_XBOX SPEAKER_5POINT1

#define FACT3DAUDIO_CALCULATE_MATRIX		0x00000001
#define FACT3DAUDIO_CALCULATE_DELAY		0x00000002
#define FACT3DAUDIO_CALCULATE_LPF_DIRECT	0x00000004
#define FACT3DAUDIO_CALCULATE_LPF_REVERB	0x00000008
#define FACT3DAUDIO_CALCULATE_REVERB		0x00000010
#define FACT3DAUDIO_CALCULATE_DOPPLER		0x00000020
#define FACT3DAUDIO_CALCULATE_EMITTER_ANGLE	0x00000040
#define FACT3DAUDIO_CALCULATE_ZEROCENTER	0x00010000
#define FACT3DAUDIO_CALCULATE_REDIRECT_TO_LFE	0x00020000

#define FACT3DAUDIO_HANDLE_BYTESIZE 20
typedef uint8_t FACT3DAUDIO_HANDLE[FACT3DAUDIO_HANDLE_BYTESIZE];

typedef struct FACT3DAUDIO_VECTOR
{
	float x;
	float y;
	float z;
} FACT3DAUDIO_VECTOR;

typedef struct FACT3DAUDIO_DISTANCE_CURVE_POINT
{
	float Distance;
	float DSPSetting;
} FACT3DAUDIO_DISTANCE_CURVE_POINT;

typedef struct FACT3DAUDIO_DISTANCE_CURVE
{
	FACT3DAUDIO_DISTANCE_CURVE_POINT *pPoints;
	uint32_t PointCount;
} FACT3DAUDIO_DISTANCE_CURVE;

typedef struct FACT3DAUDIO_CONE
{
	float InnerAngle;
	float OuterAngle;
	float InnerVolume;
	float OuterVolume;
	float InnerLPF;
	float OuterLPF;
	float InnerReverb;
	float OuterReverb;
} FACT3DAUDIO_CONE;

typedef struct FACT3DAUDIO_LISTENER
{
	FACT3DAUDIO_VECTOR OrientFront;
	FACT3DAUDIO_VECTOR OrientTop;
	FACT3DAUDIO_VECTOR Position;
	FACT3DAUDIO_VECTOR Velocity;
	FACT3DAUDIO_CONE *pCone;
} FACT3DAUDIO_LISTENER;

typedef struct FACT3DAUDIO_EMITTER
{
	FACT3DAUDIO_CONE *pCone;
	FACT3DAUDIO_VECTOR OrientFront;
	FACT3DAUDIO_VECTOR OrientTop;
	FACT3DAUDIO_VECTOR Position;
	FACT3DAUDIO_VECTOR Velocity;
	float InnerRadius;
	float InnerRadiusAngle;
	uint32_t ChannelCount;
	float ChannelRadius;
	float *pChannelAzimuths;
	FACT3DAUDIO_DISTANCE_CURVE *pVolumeCurve;
	FACT3DAUDIO_DISTANCE_CURVE *pLFECurve;
	FACT3DAUDIO_DISTANCE_CURVE *pLPFDirectCurve;
	FACT3DAUDIO_DISTANCE_CURVE *pLPFReverbCurve;
	FACT3DAUDIO_DISTANCE_CURVE *pReverbCurve;
	float CurveDistanceScaler;
	float DopplerScaler;
} FACT3DAUDIO_EMITTER;

struct FACT3DAUDIO_DSP_SETTINGS
{
	float *pMatrixCoefficients;
	float *pDelayTimes;
	uint32_t SrcChannelCount;
	uint32_t DstChannelCount;
	float LPFDirectCoefficient;
	float LPFReverbCoefficient;
	float ReverbLevel;
	float DopplerFactor;
	float EmitterToListenerAngle;
	float EmitterToListenerDistance;
	float EmitterVelocityComponent;
	float ListenerVelocityComponent;
};

static const float aStereoLayout[] =
{
	LEFT_AZIMUTH,
	RIGHT_AZIMUTH
};
static const float a2Point1Layout[] =
{
	LEFT_AZIMUTH,
	RIGHT_AZIMUTH,
	LOW_FREQUENCY_AZIMUTH
};
static const float aQuadLayout[] =
{
	FRONT_LEFT_AZIMUTH,
	FRONT_RIGHT_AZIMUTH,
	BACK_LEFT_AZIMUTH,
	BACK_RIGHT_AZIMUTH
};
static const float a4Point1Layout[] =
{
	FRONT_LEFT_AZIMUTH,
	FRONT_RIGHT_AZIMUTH,
	LOW_FREQUENCY_AZIMUTH,
	BACK_LEFT_AZIMUTH,
	BACK_RIGHT_AZIMUTH
};
static const float a5Point1Layout[] =
{
	FRONT_LEFT_AZIMUTH,
	FRONT_RIGHT_AZIMUTH,
	FRONT_CENTER_AZIMUTH,
	LOW_FREQUENCY_AZIMUTH,
	BACK_LEFT_AZIMUTH,
	BACK_RIGHT_AZIMUTH
};
static const float a7Point1Layout[] =
{
	FRONT_LEFT_AZIMUTH,
	FRONT_RIGHT_AZIMUTH,
	FRONT_CENTER_AZIMUTH,
	LOW_FREQUENCY_AZIMUTH,
	BACK_LEFT_AZIMUTH,
	BACK_RIGHT_AZIMUTH,
	LEFT_AZIMUTH,
	RIGHT_AZIMUTH
};

FACTAPI void FACT3DAudioInitialize(
	uint32_t SpeakerChannelMask,
	float SpeedOfSound,
	FACT3DAUDIO_HANDLE Instance
);

FACTAPI void FACT3DAudioCalculate(
	const FACT3DAUDIO_HANDLE Instance,
	const FACT3DAUDIO_LISTENER *pListener,
	const FACT3DAUDIO_EMITTER *pEmitter,
	uint32_t Flags,
	FACT3DAUDIO_DSP_SETTINGS *pDSPSettings
);

FACTAPI uint32_t FACT3DInitialize(
	FACTAudioEngine *pEngine,
	FACT3DAUDIO_HANDLE F3DInstance
);

FACTAPI uint32_t FACT3DCalculate(
	FACT3DAUDIO_HANDLE F3DInstance,
	const FACT3DAUDIO_LISTENER *pListener,
	FACT3DAUDIO_EMITTER *pEmitter,
	FACT3DAUDIO_DSP_SETTINGS *pDSPSettings
);

FACTAPI uint32_t FACT3DApply(
	FACT3DAUDIO_DSP_SETTINGS *pDSPSettings,
	FACTCue *pCue
);

/* XNA Sound API */

typedef struct FACTXNABuffer FACTXNABuffer;
typedef struct FACTXNASource FACTXNASource;
typedef struct FACTXNASong FACTXNASong;

/* This is meant to duplicate the XNA SoundState enum */
typedef enum FACTXNASoundState
{
	FACT_XNA_STATE_PLAYING,
	FACT_XNA_STATE_PAUSED,
	FACT_XNA_STATE_STOPPED
} FACTXNASoundState;

FACTAPI void FACT_XNA_Init();

FACTAPI void FACT_XNA_Quit();

FACTAPI float FACT_XNA_GetMasterVolume();

FACTAPI void FACT_XNA_SetMasterVolume(float volume);

FACTAPI float FACT_XNA_GetDistanceScale();

FACTAPI void FACT_XNA_SetDistanceScale(float scale);

FACTAPI float FACT_XNA_GetDopplerScale();

FACTAPI void FACT_XNA_SetDopplerScale(float scale);

FACTAPI float FACT_XNA_GetSpeedOfSound();

FACTAPI void FACT_XNA_SetSpeedOfSound(float speedOfSound);

FACTAPI FACTXNABuffer* FACT_XNA_GenBuffer(
	uint8_t *buffer,
	int32_t offset,
	int32_t count,
	int32_t sampleRate,
	int32_t channels,
	int32_t loopStart,
	int32_t loopLength,
	uint16_t format,
	uint32_t formatParameter
);

FACTAPI void FACT_XNA_DisposeBuffer(FACTXNABuffer *buffer);

FACTAPI int32_t FACT_XNA_GetBufferDuration(FACTXNABuffer *buffer);

FACTAPI uint32_t FACT_XNA_PlayBuffer(
	FACTXNABuffer *buffer,
	float volume,
	float pitch,
	float pan
);

FACTAPI FACTXNASource* FACT_XNA_GenSource(FACTXNABuffer *buffer);

FACTAPI FACTXNASource* FACT_XNA_GenDynamicSource(
	int32_t sampleRate,
	int32_t channels
);

FACTAPI void FACT_XNA_DisposeSource(FACTXNASource *source);

FACTAPI int32_t FACT_XNA_GetSourceState(FACTXNASource *source);

FACTAPI int32_t FACT_XNA_GetSourceLooped(FACTXNASource *source);

FACTAPI void FACT_XNA_SetSourceLooped(
	FACTXNASource *source,
	int32_t looped
);

FACTAPI float FACT_XNA_GetSourcePan(FACTXNASource *source);

FACTAPI void FACT_XNA_SetSourcePan(
	FACTXNASource *source,
	float pan
);

FACTAPI float FACT_XNA_GetSourcePitch(FACTXNASource *source);

FACTAPI void FACT_XNA_SetSourcePitch(
	FACTXNASource *source,
	float pitch
);

FACTAPI float FACT_XNA_GetSourceVolume(FACTXNASource *source);

FACTAPI void FACT_XNA_SetSourceVolume(
	FACTXNASource *source,
	float volume
);

FACTAPI void FACT_XNA_ApplySource3D(
	FACTXNASource *source,
	FACT3DAUDIO_LISTENER *listener,
	FACT3DAUDIO_EMITTER *emitter
);

FACTAPI void FACT_XNA_PlaySource(FACTXNASource *source);

FACTAPI void FACT_XNA_PauseSource(FACTXNASource *source);

FACTAPI void FACT_XNA_ResumeSource(FACTXNASource *source);

FACTAPI void FACT_XNA_StopSource(
	FACTXNASource *source,
	int32_t immediate
);

FACTAPI void FACT_XNA_ApplySourceReverb(
	FACTXNASource *source,
	float rvGain
);

FACTAPI void FACT_XNA_ApplySourceLowPass(
	FACTXNASource *source,
	float hfGain
);

FACTAPI void FACT_XNA_ApplySourceHighPass(
	FACTXNASource *source,
	float lfGain
);

FACTAPI void FACT_XNA_ApplySourceBandPass(
	FACTXNASource *source,
	float hfGain,
	float lfGain
);

FACTAPI int32_t FACT_XNA_GetSourceBufferCount(FACTXNASource *source);

FACTAPI void FACT_XNA_QueueSourceBufferB(
	FACTXNASource *source,
	uint8_t *buffer,
	int32_t offset,
	int32_t count
);

FACTAPI void FACT_XNA_QueueSourceBufferF(
	FACTXNASource *source,
	float *buffer,
	int32_t offset,
	int32_t count
);

FACTAPI FACTXNASong* FACT_XNA_GenSong(const char* name);

FACTAPI void FACT_XNA_DisposeSong(FACTXNASong *song);

FACTAPI void FACT_XNA_PlaySong(FACTXNASong *song);

FACTAPI void FACT_XNA_PauseSong(FACTXNASong *song);

FACTAPI void FACT_XNA_ResumeSong(FACTXNASong *song);

FACTAPI void FACT_XNA_StopSong(FACTXNASong *song);

FACTAPI void FACT_XNA_SetSongVolume(
	FACTXNASong *song,
	float volume
);

FACTAPI uint32_t FACT_XNA_GetSongEnded(FACTXNASong *song);

/* FACT I/O API */

typedef struct FACTIOStream FACTIOStream;

FACTAPI FACTIOStream* FACT_fopen(const char *path);
FACTAPI FACTIOStream* FACT_memopen(void *mem, int len);
FACTAPI void FACT_close(FACTIOStream *io);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* FACT_H */
