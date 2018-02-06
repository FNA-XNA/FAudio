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

typedef enum FAudioDeviceRole
{
	NotDefaultDevice =		0x0,
	DefaultConsoleDevice =		0x1,
	DefaultMultimediaDevice =	0x2,
	DefaultCommunicationsDevice =	0x4,
	DefaultGameDevice =		0x8,
	GlobalDefaultDevice =		0xF,
	InvalidDeviceRole = ~GlobalDefaultDevice
} FAudioDeviceRole;

typedef enum FAudioFilterType
{
	LowPassFilter,
	BandPassFilter,
	HighPassFilter,
	NotchFilter
} FAudioFilterType;

/* FIXME: The original enum violates ISO C and is platform specific anyway... */
typedef uint32_t FAudioProcessor;
#define FAUDIO_DEFAULT_PROCESSOR 0xFFFFFFFF

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

typedef struct FAudioDeviceDetails
{
	int16_t DeviceID[256]; /* Win32 wchar_t */
	int16_t DisplayName[256]; /* Win32 wchar_t */
	FAudioDeviceRole Role;
	FAudioWaveFormatExtensible OutputFormat;
} FAudioDeviceDetails;

typedef struct FAudioVoiceDetails
{
	uint32_t CreationFlags;
	uint32_t InputChannels;
	uint32_t InputSampleRate;
} FAudioVoiceDetails;

typedef struct FAudioSendDescriptor
{
	uint32_t Flags;
	FAudioVoice *pOutputVoice;
} FAudioSendDescriptor;

typedef struct FAudioVoiceSends
{
	uint32_t SendCount;
	FAudioSendDescriptor *pSends;
} FAudioVoiceSends;

typedef struct FAudioEffectDescriptor
{
	void *pEffect;
	uint8_t InitialState;
	uint32_t OutputChannels;
} FAudioEffectDescriptor;

typedef struct FAudioEffectChain
{
	uint32_t EffectCount;
	FAudioEffectDescriptor *pEffectDescriptors;
} FAudioEffectChain;

typedef struct FAudioFilterParameters
{
	FAudioFilterType Type;
	float Frequency;
	float OneOverQ;
} FAudioFilterParameters;

typedef struct FAudioBuffer
{
	uint32_t Flags;
	uint32_t AudioBytes;
	const uint8_t *pAudioData;
	uint32_t PlayBegin;
	uint32_t PlayLength;
	uint32_t LoopBegin;
	uint32_t LoopLength;
	uint32_t LoopCount;
	void *pContext;
} FAudioBuffer;

typedef struct FAudioBufferWMA
{
	const uint32_t *pDecodedPacketCumulativeBytes;
	uint32_t PacketCount;
} FAudioBufferWMA;

typedef struct FAudioVoiceState
{
	void *pCurrentBufferContext;
	uint32_t BuffersQueued;
	uint64_t SamplesPlayed;
} FAudioVoiceState;

typedef struct FAudioPerformanceData
{
	uint64_t AudioCyclesSinceLastQuery;
	uint64_t TotalCyclesSinceLastQuery;
	uint32_t MinimumCyclesPerQuantum;
	uint32_t MaximumCyclesPerQuantum;
	uint32_t MemoryUsageInBytes;
	uint32_t CurrentLatencyInSamples;
	uint32_t GlitchesSinceEngineStarted;
	uint32_t ActiveSourceVoiceCount;
	uint32_t TotalSourceVoiceCount;
	uint32_t ActiveSubmixVoiceCount;
	uint32_t ActiveResamplerCount;
	uint32_t ActiveMatrixMixCount;
	uint32_t ActiveXmaSourceVoices;
	uint32_t ActiveXmaStreams;
} FAudioPerformanceData;

typedef struct FAudioDebugConfiguration
{
	uint32_t TraceMask;
	uint32_t BreakMask;
	uint8_t LogThreadID;
	uint8_t LogFileline;
	uint8_t LogFunctionName;
	uint8_t LogTiming;
} FAudioDebugConfiguration;

/* Constants */

#define FAUDIO_MAX_BUFFER_BYTES		0x80000000
#define FAUDIO_MAX_QUEUED_BUFFERS	64
#define FAUDIO_MAX_AUDIO_CHANNELS	64
#define FAUDIO_MIN_SAMPLE_RATE		1000
#define FAUDIO_MAX_SAMPLE_RATE		200000
#define FAUDIO_MAX_VOLUME_LEVEL		16777216.0f
#define FAUDIO_MIN_FREQ_RATIO		(1.0f / 1024.0f)
#define FAUDIO_MAX_FREQ_RATIO		1024.0f
#define FAUDIO_DEFAULT_FREQ_RATIO	2.0f
#define FAUDIO_MAX_FILTER_ONEOVERQ	1.5f
#define FAUDIO_MAX_FILTER_FREQUENCY	1.0f
#define FAUDIO_MAX_LOOP_COUNT		254

#define FAUDIO_COMMIT_NOW		0
#define FAUDIO_COMMIT_ALL		0
#define FAUDIO_INVALID_OPSET		(uint32_t) (-1)
#define FAUDIO_NO_LOOP_REGION		0
#define FAUDIO_LOOP_INFINITE		255
#define FAUDIO_DEFAULT_CHANNELS		0
#define FAUDIO_DEFAULT_SAMPLERATE	0

#define FAUDIO_DEBUG_ENGINE		0x01
#define FAUDIO_VOICE_NOPITCH		0x02
#define FAUDIO_VOICE_NOSRC		0x04
#define FAUDIO_VOICE_USEFILTER		0x08
#define FAUDIO_VOICE_MUSIC		0x10
#define FAUDIO_PLAY_TAILS		0x20
#define FAUDIO_END_OF_STREAM		0x40
#define FAUDIO_SEND_USEFILTER		0x80

#define FAUDIO_DEFAULT_FILTER_TYPE	LowPassFilter
#define FAUDIO_DEFAULT_FILTER_FREQUENCY	FAUDIO_MAX_FILTER_FREQUENCY
#define FAUDIO_DEFAULT_FILTER_ONEOVERQ	1.0f

#define FAUDIO_LOG_ERRORS		0x0001
#define FAUDIO_LOG_WARNINGS		0x0002
#define FAUDIO_LOG_INFO			0x0004
#define FAUDIO_LOG_DETAIL		0x0008
#define FAUDIO_LOG_API_CALLS		0x0010
#define FAUDIO_LOG_FUNC_CALLS		0x0020
#define FAUDIO_LOG_TIMING		0x0040
#define FAUDIO_LOG_LOCKS		0x0080
#define FAUDIO_LOG_MEMORY		0x0100
#define FAUDIO_LOG_STREAMING		0x1000

/* FAudio Interface */

/* FIXME: Do we want to actually reproduce the COM stuff or what...? -flibit */
FAUDIOAPI uint32_t FAudioCreate(
	FAudio **ppFAudio,
	uint32_t Flags,
	FAudioProcessor XAudio2Processor
);

FAUDIOAPI void FAudioDestroy(FAudio *audio); /* FIXME: NOT XAUDIO2 SPEC! */

/* FIXME: AddRef/Release/Query? Or just ignore COM garbage... -flibit */

FAUDIOAPI uint32_t FAudio_GetDeviceCount(FAudio *audio, uint32_t *pCount);

FAUDIOAPI uint32_t FAudio_GetDeviceDetails(
	FAudio *audio,
	uint32_t Index,
	FAudioDeviceDetails *pDeviceDetails
);

FAUDIOAPI uint32_t FAudio_Initialize(
	FAudio *audio,
	uint32_t Flags,
	FAudioProcessor XAudio2Processor
);

FAUDIOAPI uint32_t FAudio_RegisterForCallbacks(
	FAudio *audio,
	FAudioEngineCallback *pCallback
);

FAUDIOAPI void FAudio_UnregisterForCallbacks(
	FAudio *audio,
	FAudioEngineCallback *pCallback
);

FAUDIOAPI uint32_t FAudio_CreateSourceVoice(
	FAudio *audio,
	FAudioSourceVoice **ppSourceVoice,
	const FAudioWaveFormatEx *pSourceFormat,
	uint32_t Flags,
	float MaxFrequencyRatio,
	FAudioVoiceCallback *pCallback,
	const FAudioVoiceSends *pSendList,
	const FAudioEffectChain *pEffectChain
);

FAUDIOAPI uint32_t FAudio_CreateSubmixVoice(
	FAudio *audio,
	FAudioSubmixVoice **ppSubmixVoice,
	uint32_t InputChannels,
	uint32_t InputSampleRate,
	uint32_t Flags,
	uint32_t ProcessingStage,
	const FAudioVoiceSends *pSendList,
	const FAudioEffectChain *pEffectChain
);

FAUDIOAPI uint32_t FAudio_CreateMasteringVoice(
	FAudio *audio,
	FAudioMasteringVoice **ppMasteringVoice,
	uint32_t InputChannels,
	uint32_t InputSampleRate,
	uint32_t Flags,
	uint32_t DeviceIndex,
	const FAudioEffectChain *pEffectChain
);

FAUDIOAPI uint32_t FAudio_StartEngine(FAudio *audio);

FAUDIOAPI void FAudio_StopEngine(FAudio *audio);

FAUDIOAPI uint32_t FAudio_CommitChanges(FAudio *audio);

FAUDIOAPI void FAudio_GetPerformanceData(
	FAudio *audio,
	FAudioPerformanceData *pPerfData
);

FAUDIOAPI void FAudio_SetDebugConfiguration(
	FAudio *audio,
	FAudioDebugConfiguration *pDebugConfiguration,
	void* pReserved
);

/* FAudioVoice Interface */

FAUDIOAPI void FAudioVoice_GetVoiceDetails(
	FAudioVoice *voice,
	FAudioVoiceDetails *pVoiceDetails
);

FAUDIOAPI uint32_t FAudioVoice_SetOutputVoices(
	FAudioVoice *voice,
	const FAudioVoiceSends *pSendList
);

FAUDIOAPI uint32_t FAudioVoice_SetEffectChain(
	FAudioVoice *voice,
	const FAudioEffectChain *pEffectChain
);

FAUDIOAPI uint32_t FAudioVoice_EnableEffect(
	FAudioVoice *voice,
	uint32_t EffectIndex,
	uint32_t OperationSet
);

FAUDIOAPI uint32_t FAudioVoice_DisableEffect(
	FAudioVoice *voice,
	uint32_t EffectIndex,
	uint32_t OperationSet
);

FAUDIOAPI void FAudioVoice_GetEffectState(
	FAudioVoice *voice,
	uint32_t EffectIndex,
	uint8_t *pEnabled
);

FAUDIOAPI uint32_t FAudioVoice_SetEffectParameters(
	FAudioVoice *voice,
	uint32_t EffectIndex,
	const void *pParameters,
	uint32_t ParametersByteSize,
	uint32_t OperationSet
);

FAUDIOAPI uint32_t FAudioVoice_GetEffectParameters(
	FAudioVoice *voice,
	void *pParameters,
	uint32_t ParametersByteSize
);

FAUDIOAPI uint32_t FAudioVoice_SetFilterParameters(
	FAudioVoice *voice,
	const FAudioFilterParameters *pParameters,
	uint32_t OperationSet
);

FAUDIOAPI void FAudioVoice_GetFilterParameters(
	FAudioVoice *voice,
	FAudioFilterParameters *pParameters
);

FAUDIOAPI uint32_t FAudioVoice_SetOutputFilterParameters(
	FAudioVoice *voice,
	FAudioVoice *pDestinationVoice,
	const FAudioFilterParameters *pParameters,
	uint32_t OperationSet
);

FAUDIOAPI void FAudioVoice_GetOutputFilterParameters(
	FAudioVoice *voice,
	FAudioVoice *pDestinationVoice,
	FAudioFilterParameters *pParameters
);

FAUDIOAPI uint32_t FAudioVoice_SetVolume(
	FAudioVoice *voice,
	float Volume,
	uint32_t OperationSet
);

FAUDIOAPI void FAudioVoice_GetVolume(
	FAudioVoice *voice,
	float *pVolume
);

FAUDIOAPI uint32_t FAudioVoice_SetChannelVolumes(
	FAudioVoice *voice,
	uint32_t Channels,
	const float *pVolumes,
	uint32_t OperationSet
);

FAUDIOAPI void FAudioVoice_GetChannelVolumes(
	FAudioVoice *voice,
	uint32_t Channels,
	float *pVolumes
);

FAUDIOAPI uint32_t FAudioVoice_SetOutputMatrix(
	FAudioVoice *voice,
	FAudioVoice *pDestinationVoice,
	uint32_t SourceChannels,
	uint32_t DestinationChannels,
	const float *pLevelMatrix,
	uint32_t OperationSet
);

FAUDIOAPI void FAudioVoice_GetOutputMatrix(
	FAudioVoice *voice,
	FAudioVoice *pDestinationVoice,
	uint32_t SourceChannels,
	uint32_t DestinationChannels,
	float *pLevelMatrix
);

FAUDIOAPI void FAudioVoice_DestroyVoice(FAudioVoice *voice);

/* FAudioSourceVoice Interface */

FAUDIOAPI uint32_t FAudioSourceVoice_Start(
	FAudioSourceVoice *voice,
	uint32_t Flags,
	uint32_t OperationSet
);

FAUDIOAPI uint32_t FAudioSourceVoice_Stop(
	FAudioSourceVoice *voice,
	uint32_t Flags,
	uint32_t OperationSet
);

FAUDIOAPI uint32_t FAudioSourceVoice_SubmitSourceBuffer(
	FAudioSourceVoice *voice,
	const FAudioBuffer *pBuffer,
	const FAudioBufferWMA *pBufferWMA
);

FAUDIOAPI uint32_t FAudioSourceVoice_FlushSourceBuffers(
	FAudioSourceVoice *voice
);

FAUDIOAPI uint32_t FAudioSourceVoice_Discontinuity(
	FAudioSourceVoice *voice
);

FAUDIOAPI uint32_t FAudioSourceVoice_ExitLoop(
	FAudioSourceVoice *voice,
	uint32_t OperationSet
);

FAUDIOAPI void FAudioSourceVoice_GetState(
	FAudioSourceVoice *voice,
	FAudioVoiceState *pVoiceState
);

FAUDIOAPI uint32_t FAudioSourceVoice_SetFrequencyRatio(
	FAudioSourceVoice *voice,
	float Ratio,
	uint32_t OperationSet
);

FAUDIOAPI void FAudioSourceVoice_GetFrequencyRatio(
	FAudioSourceVoice *voice,
	float *pRatio
);

FAUDIOAPI uint32_t FAudioSourceVoice_SetSourceSampleRate(
	FAudioSourceVoice *voice,
	uint32_t NewSourceSampleRate
);

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
