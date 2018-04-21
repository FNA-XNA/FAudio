#ifndef FACT_CPP_XAUDIO2_H
#define FACT_CPP_XAUDIO2_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifdef FAUDIOCPP_EXPORTS
#define FAUDIOCPP_API  HRESULT __stdcall
#else
#define FAUDIOCPP_API __declspec(dllimport) HRESULT __stdcall
#endif

#ifndef XAUDIO2_VERSION
#define XAUDIO2_VERSION 7
#endif

#include <FAudio.h>

typedef uint32_t HRESULT;
typedef uint32_t UINT32;
typedef unsigned long ULONG;
typedef wchar_t WHCAR;
typedef const WHCAR *LPCWSTR;
typedef int BOOL;
typedef unsigned long DWORD;
typedef void *LPVOID;

typedef FAudioGUID GUID;
typedef GUID IID;
#define REFIID const IID &
#define REFCLSID const IID &

typedef FAudioProcessor XAUDIO2_PROCESSOR;
typedef FAudioDeviceDetails XAUDIO2_DEVICE_DETAILS;
typedef FAudioWaveFormatEx WAVEFORMATEX;
typedef FAudioVoiceSends XAUDIO2_VOICE_SENDS;
typedef FAudioEffectChain XAUDIO2_EFFECT_CHAIN;
typedef FAudioPerformanceData XAUDIO2_PERFORMANCE_DATA;
typedef FAudioDebugConfiguration XAUDIO2_DEBUG_CONFIGURATION;

typedef FAudioVoiceDetails XAUDIO2_VOICE_DETAILS;
typedef FAudioFilterParameters XAUDIO2_FILTER_PARAMETERS;
typedef FAudioBuffer XAUDIO2_BUFFER;
typedef FAudioBufferWMA XAUDIO2_BUFFER_WMA;
typedef FAudioVoiceState XAUDIO2_VOICE_STATE;

class IXAudio2EngineCallback;
class IXAudio2SourceVoice;
class IXAudio2SubmixVoice;
class IXAudio2MasteringVoice;
class IXAudio2VoiceCallback;

#define X2METHOD(rtype)		virtual rtype __stdcall 

class IUnknown {
public:
	X2METHOD(HRESULT) QueryInterface(REFIID riid, void** ppvInterface) = 0;		// FIXME: void * was REFIID
	X2METHOD(ULONG) AddRef() = 0;
	X2METHOD(ULONG) Release() = 0;
};

class IXAudio2Voice  {
public:
	X2METHOD(void) GetVoiceDetails (XAUDIO2_VOICE_DETAILS* pVoiceDetails) = 0;
	X2METHOD(HRESULT) SetOutputVoices (const XAUDIO2_VOICE_SENDS* pSendList) = 0;
	X2METHOD(HRESULT) SetEffectChain (const XAUDIO2_EFFECT_CHAIN* pEffectChain) = 0;
	X2METHOD(HRESULT) EnableEffect (
		UINT32 EffectIndex,
		UINT32 OperationSet = FAUDIO_COMMIT_NOW) = 0;
	X2METHOD(HRESULT) DisableEffect (
		UINT32 EffectIndex,
		UINT32 OperationSet  = FAUDIO_COMMIT_NOW) = 0;
	X2METHOD(void) GetEffectState (UINT32 EffectIndex, BOOL* pEnabled) = 0;
	X2METHOD(HRESULT) SetEffectParameters (
		UINT32 EffectIndex,
		const void* pParameters,
		UINT32 ParametersByteSize,
		UINT32 OperationSet  = FAUDIO_COMMIT_NOW) = 0;
	X2METHOD(HRESULT) GetEffectParameters (
		UINT32 EffectIndex,
		void* pParameters,
		UINT32 ParametersByteSize) = 0;
	X2METHOD(HRESULT) SetFilterParameters (
		const XAUDIO2_FILTER_PARAMETERS* pParameters,
		UINT32 OperationSet  = FAUDIO_COMMIT_NOW) = 0;
	X2METHOD(void) GetFilterParameters (XAUDIO2_FILTER_PARAMETERS* pParameters) = 0;
	X2METHOD(HRESULT) SetOutputFilterParameters (
		IXAudio2Voice* pDestinationVoice,
		const XAUDIO2_FILTER_PARAMETERS* pParameters,
		UINT32 OperationSet  = FAUDIO_COMMIT_NOW) = 0;
	X2METHOD(void) GetOutputFilterParameters (
		IXAudio2Voice* pDestinationVoice,
		XAUDIO2_FILTER_PARAMETERS* pParameters) = 0;
	X2METHOD(HRESULT) SetVolume (
		float Volume,
		UINT32 OperationSet = FAUDIO_COMMIT_NOW) = 0;
	X2METHOD(void) GetVolume (float* pVolume) = 0;
	X2METHOD(HRESULT) SetChannelVolumes (
		UINT32 Channels, 
		const float* pVolumes,
		UINT32 OperationSet  = FAUDIO_COMMIT_NOW) = 0;
	X2METHOD(void) GetChannelVolumes (UINT32 Channels, _Out_writes_(Channels) float* pVolumes) = 0;
	X2METHOD(HRESULT) SetOutputMatrix (
		IXAudio2Voice* pDestinationVoice,
		UINT32 SourceChannels, 
		UINT32 DestinationChannels,
		const float* pLevelMatrix,
		UINT32 OperationSet  = FAUDIO_COMMIT_NOW) = 0;
	X2METHOD(void) GetOutputMatrix (
		IXAudio2Voice* pDestinationVoice,
		UINT32 SourceChannels, 
		UINT32 DestinationChannels,
		float* pLevelMatrix) = 0;
	X2METHOD(void) DestroyVoice() = 0;
};

class IXAudio2SourceVoice : public IXAudio2Voice {
public:
	X2METHOD(HRESULT) Start (UINT32 Flags = 0, UINT32 OperationSet = FAUDIO_COMMIT_NOW) = 0;
	X2METHOD(HRESULT) Stop (UINT32 Flags = 0, UINT32 OperationSet = FAUDIO_COMMIT_NOW) = 0;
	X2METHOD(HRESULT) SubmitSourceBuffer (
		const XAUDIO2_BUFFER* pBuffer, 
		const XAUDIO2_BUFFER_WMA* pBufferWMA = NULL) = 0;
	X2METHOD(HRESULT) FlushSourceBuffers () = 0;
	X2METHOD(HRESULT) Discontinuity () = 0;
	X2METHOD(HRESULT) ExitLoop (UINT32 OperationSet = FAUDIO_COMMIT_NOW) = 0;
#if (XAUDIO2_VERSION <= 7)
	X2METHOD(void) GetState ( XAUDIO2_VOICE_STATE* pVoiceState) = 0;
#else
	X2METHOD(void) GetState ( XAUDIO2_VOICE_STATE* pVoiceState, UINT32 Flags = 0) = 0;
#endif
	X2METHOD(HRESULT) SetFrequencyRatio (
		float Ratio,
		UINT32 OperationSet = FAUDIO_COMMIT_NOW) = 0;
	X2METHOD(void) GetFrequencyRatio (float* pRatio) = 0;
	X2METHOD(HRESULT) SetSourceSampleRate (UINT32 NewSourceSampleRate) = 0;
};

class IXAudio2SubmixVoice : public IXAudio2Voice {

};

class IXAudio2MasteringVoice : public IXAudio2Voice {
public:
#if (XAUDIO2_VERSION >= 8)
	X2METHOD(HRESULT) GetChannelMask (DWORD* pChannelmask) = 0;
#endif
};

class IXAudio2VoiceCallback {
public:
	X2METHOD(void) OnVoiceProcessingPassStart (UINT32 BytesRequired) = 0;
	X2METHOD(void) OnVoiceProcessingPassEnd () = 0;
	X2METHOD(void) OnStreamEnd () = 0;
	X2METHOD(void) OnBufferStart (void* pBufferContext) = 0;
	X2METHOD(void) OnBufferEnd (void* pBufferContext) = 0;
	X2METHOD(void) OnLoopEnd (void* pBufferContext) = 0;
	X2METHOD(void) OnVoiceError (void* pBufferContext, HRESULT Error) = 0;
};

class IXAudio2 : public IUnknown {
public:
#if (XAUDIO2_VERSION <= 7)
	X2METHOD(HRESULT) GetDeviceCount(UINT32 *pCount) = 0;
	X2METHOD(HRESULT) GetDeviceDetails (UINT32 Index, XAUDIO2_DEVICE_DETAILS* pDeviceDetails) = 0;
	X2METHOD(HRESULT) Initialize (
		UINT32 Flags = 0,
		XAUDIO2_PROCESSOR XAudio2Processor = FAUDIO_DEFAULT_PROCESSOR) = 0;
#endif // XAUDIO2_VERSION <= 7

	X2METHOD(HRESULT) RegisterForCallbacks (IXAudio2EngineCallback* pCallback) = 0;
	X2METHOD(void) UnregisterForCallbacks ( IXAudio2EngineCallback* pCallback) = 0;

	X2METHOD(HRESULT) CreateSourceVoice (
		IXAudio2SourceVoice** ppSourceVoice,
		const WAVEFORMATEX* pSourceFormat,
		UINT32 Flags = 0,
		float MaxFrequencyRatio = FAUDIO_DEFAULT_FREQ_RATIO,
		IXAudio2VoiceCallback* pCallback = NULL,
		const XAUDIO2_VOICE_SENDS* pSendList = NULL,
		const XAUDIO2_EFFECT_CHAIN* pEffectChain = NULL) = 0;

	X2METHOD(HRESULT) CreateSubmixVoice(
		IXAudio2SubmixVoice** ppSubmixVoice,
		UINT32 InputChannels, 
		UINT32 InputSampleRate,
		UINT32 Flags = 0,
		UINT32 ProcessingStage = 0,
		const XAUDIO2_VOICE_SENDS* pSendList = NULL,
		const XAUDIO2_EFFECT_CHAIN* pEffectChain = NULL) = 0;

#if (XAUDIO2_VERSION <= 7)
	X2METHOD(HRESULT) CreateMasteringVoice(
		IXAudio2MasteringVoice** ppMasteringVoice,
		UINT32 InputChannels = FAUDIO_DEFAULT_CHANNELS,
		UINT32 InputSampleRate = FAUDIO_DEFAULT_SAMPLERATE,
		UINT32 Flags = 0,
		UINT32 DeviceIndex = 0,
		const XAUDIO2_EFFECT_CHAIN* pEffectChain = NULL) = 0;
#else
	X2METHOD(HRESULT) CreateMasteringVoice (
		IXAudio2MasteringVoice** ppMasteringVoice,
		UINT32 InputChannels = FAUDIO_DEFAULT_CHANNELS,
		UINT32 InputSampleRate = FAUDIO_DEFAULT_SAMPLERATE,
		UINT32 Flags = 0,
		LPCWSTR szDeviceId = NULL,
		const XAUDIO2_EFFECT_CHAIN* pEffectChain = NULL,
		int StreamCategory = 6) = 0;	// FIXME: type was AUDIO_STREAM_CATEGORY (scoped enum so int for now)
#endif

	X2METHOD(HRESULT) StartEngine() = 0;
	X2METHOD(void) StopEngine() = 0;

	X2METHOD(HRESULT) CommitChanges(UINT32 OperationSet) = 0;

	X2METHOD(void) GetPerformanceData(XAUDIO2_PERFORMANCE_DATA* pPerfData) = 0;

	X2METHOD(void) SetDebugConfiguration(
		const XAUDIO2_DEBUG_CONFIGURATION* pDebugConfiguration,
		void* pReserved = NULL) = 0;
};

class IClassFactory : public IUnknown {
public:
	X2METHOD(HRESULT) CreateInstance(
		IUnknown *pUnkOuter,
		REFIID riid,
		void **ppvObject) = 0;

	X2METHOD(HRESULT) LockServer(BOOL fLock) = 0;
};


FAUDIOCPP_API XAudio2Create(
	IXAudio2          **ppXAudio2,
	UINT32            Flags,
	XAUDIO2_PROCESSOR XAudio2Processor
);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif // FACT_CPP_XAUDIO2_H