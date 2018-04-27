#include "xaudio2.h"

#define TRACING_ENABLE

#ifdef TRACING_ENABLE
#include <stdio.h>
#include <stdarg.h>

#define TRACE_FILE	"c:/temp/faudio_cpp.txt"
#define TRACE_FUNC()	do { trace_msg(__FUNCTION__); } while (0)
#define TRACE_PARAMS(f,...) do {trace_msg("%s: " # f, __FUNCTION__, __VA_ARGS__);} while (0)

static void trace_msg(const char *msg, ...) {
	va_list args;
	va_start(args, msg);

	FILE *fp = fopen(TRACE_FILE, "a");
	if (fp) {
		vfprintf(fp, msg, args);
		fputc('\n', fp);
		fclose(fp);
	}
	va_end(args);
}

#else
#define TRACE_FUNC()
#define TRACE_PARAM(f, ...)
#endif // TRACING_ENABLE

///////////////////////////////////////////////////////////////////////////////
//
// IXAudio2VoiceCallback
//

struct FAudioVoiceCppCallback {
	FAudioVoiceCallback	callbacks;
	IXAudio2VoiceCallback *com;
};

static void FAUDIOCALL OnBufferEnd(FAudioVoiceCallback *callback, void *pBufferContext) {
	reinterpret_cast<FAudioVoiceCppCallback *>(callback)->com->OnBufferEnd(pBufferContext);
}

static void FAUDIOCALL OnBufferStart(FAudioVoiceCallback *callback, void *pBufferContext) {
	reinterpret_cast<FAudioVoiceCppCallback *>(callback)->com->OnBufferStart(pBufferContext);
}

static void FAUDIOCALL OnLoopEnd(FAudioVoiceCallback *callback, void *pBufferContext) {
	reinterpret_cast<FAudioVoiceCppCallback *>(callback)->com->OnLoopEnd(pBufferContext);
}

static void FAUDIOCALL OnStreamEnd(FAudioVoiceCallback *callback) {
	reinterpret_cast<FAudioVoiceCppCallback *>(callback)->com->OnStreamEnd();
}
	
static void FAUDIOCALL OnVoiceError(FAudioVoiceCallback *callback, void *pBufferContext, uint32_t Error ) {
	reinterpret_cast<FAudioVoiceCppCallback *>(callback)->com->OnVoiceError(pBufferContext, Error);
}
	
static void FAUDIOCALL OnVoiceProcessingPassEnd(FAudioVoiceCallback *callback) {
	reinterpret_cast<FAudioVoiceCppCallback *>(callback)->com->OnVoiceProcessingPassEnd();
}
	
static void FAUDIOCALL OnVoiceProcessingPassStart(FAudioVoiceCallback *callback, uint32_t BytesRequired) {
	reinterpret_cast<FAudioVoiceCppCallback *>(callback)->com->OnVoiceProcessingPassStart(BytesRequired);
}

FAudioVoiceCppCallback *wrap_voice_callback(IXAudio2VoiceCallback *com_interface) {
	if (com_interface == NULL) {
		return NULL;
	}

	FAudioVoiceCppCallback *cb = new FAudioVoiceCppCallback();
	cb->callbacks.OnBufferEnd = OnBufferEnd;
	cb->callbacks.OnBufferStart = OnBufferStart;
	cb->callbacks.OnLoopEnd = OnLoopEnd;
	cb->callbacks.OnStreamEnd = OnStreamEnd;
	cb->callbacks.OnVoiceError = OnVoiceError;
	cb->callbacks.OnVoiceProcessingPassEnd = OnVoiceProcessingPassEnd;
	cb->callbacks.OnVoiceProcessingPassStart = OnVoiceProcessingPassStart;
	cb->com = com_interface;

	return cb;
}

///////////////////////////////////////////////////////////////////////////////
//
// IXAudio2EngineCallback
//

struct FAudioCppEngineCallback {
	FAudioEngineCallback callbacks;
	IXAudio2EngineCallback *com;

	FAudioCppEngineCallback *next;
};

static void FAUDIOCALL OnCriticalError(FAudioEngineCallback *callback, uint32_t Error) {
	reinterpret_cast<FAudioCppEngineCallback *>(callback)->com->OnCriticalError(Error);
}

static void FAUDIOCALL OnProcessingPassEnd(FAudioEngineCallback *callback) {
	reinterpret_cast<FAudioCppEngineCallback *>(callback)->com->OnProcessingPassEnd();
}

static void FAUDIOCALL OnProcessingPassStart(FAudioEngineCallback *callback) {
	reinterpret_cast<FAudioCppEngineCallback *>(callback)->com->OnProcessingPassStart();
}

static FAudioCppEngineCallback *wrap_engine_callback(IXAudio2EngineCallback *com_interface) {
	if (com_interface == NULL) {
		return NULL;
	}

	FAudioCppEngineCallback *cb = new FAudioCppEngineCallback();
	cb->callbacks.OnCriticalError = OnCriticalError;
	cb->callbacks.OnProcessingPassEnd = OnProcessingPassEnd;
	cb->callbacks.OnProcessingPassStart = OnProcessingPassStart;
	cb->com = com_interface;
	cb->next = NULL;

	return cb;
}

static FAudioCppEngineCallback *find_and_remove_engine_callback(FAudioCppEngineCallback *list, IXAudio2EngineCallback *com) {
	FAudioCppEngineCallback *last = list;

	for (FAudioCppEngineCallback *it = list->next; it != NULL; it = it->next) {
		if (it->com == com) {
			last->next = it->next;
			return it;
		}

		last = it;
	}

	return NULL;
}

///////////////////////////////////////////////////////////////////////////////
//
// XAUDIO2_VOICE_SENDS / XAUDIO2_SEND_DESCRIPTOR => FAudio
//

static FAudioVoiceSends *unwrap_voice_sends(const XAUDIO2_VOICE_SENDS *x_sends) {
	if (x_sends == NULL) {
		return NULL;
	}
	TRACE_PARAMS("SendCount = %d", x_sends->SendCount);

	FAudioVoiceSends *f_sends = new FAudioVoiceSends();
	f_sends->SendCount = x_sends->SendCount;
	f_sends->pSends = new FAudioSendDescriptor[f_sends->SendCount];

	for (uint32_t i = 0; i < f_sends->SendCount; ++i) {
		f_sends->pSends[i].Flags = x_sends->pSends[i].Flags;
		f_sends->pSends[i].pOutputVoice = x_sends->pSends[i].pOutputVoice->faudio_voice;
		TRACE_PARAMS("x : %x => f : %x", f_sends->pSends[i].pOutputVoice, x_sends->pSends[i].pOutputVoice);
	}

	return f_sends;
}

static void free_voice_sends(FAudioVoiceSends *f_sends) {
	if (f_sends != NULL) {
		delete[] f_sends->pSends;
		delete f_sends;
	}
}

///////////////////////////////////////////////////////////////////////////////
//
// IXAudio2SourceVoice implementation
//

class XAudio2SourceVoiceImpl : public IXAudio2SourceVoice {
public:
	XAudio2SourceVoiceImpl(
		FAudio *faudio,
		const WAVEFORMATEX* pSourceFormat,
		UINT32 Flags,
		float MaxFrequencyRatio,
		IXAudio2VoiceCallback* pCallback,
		const XAUDIO2_VOICE_SENDS* pSendList,
		const XAUDIO2_EFFECT_CHAIN* pEffectChain) {
		TRACE_PARAMS("Format=%d; nChannels=%d; nSamplesPerSec=%d", pSourceFormat->wFormatTag, pSourceFormat->nChannels, pSourceFormat->nSamplesPerSec);
		voice_callback = wrap_voice_callback(pCallback);
		voice_sends = unwrap_voice_sends(pSendList);
		FAudio_CreateSourceVoice(faudio, &faudio_voice, pSourceFormat, Flags, MaxFrequencyRatio, reinterpret_cast<FAudioVoiceCallback *>(voice_callback), voice_sends, NULL);
		TRACE_FUNC();
	}

	// IXAudio2Voice
	X2METHOD(void) GetVoiceDetails(XAUDIO2_VOICE_DETAILS* pVoiceDetails) {
		TRACE_FUNC();
		FAudioVoice_GetVoiceDetails(faudio_voice, pVoiceDetails);
	}

	X2METHOD(HRESULT) SetOutputVoices(const XAUDIO2_VOICE_SENDS* pSendList) {
		TRACE_FUNC();
		free_voice_sends(voice_sends);
		voice_sends = unwrap_voice_sends(pSendList);
		return FAudioVoice_SetOutputVoices(faudio_voice, voice_sends);
	}

	X2METHOD(HRESULT) SetEffectChain(const XAUDIO2_EFFECT_CHAIN* pEffectChain) {
		TRACE_FUNC();
		return FAudioVoice_SetEffectChain(faudio_voice, pEffectChain);
	}

	X2METHOD(HRESULT) EnableEffect(
		UINT32 EffectIndex,
		UINT32 OperationSet = FAUDIO_COMMIT_NOW) {
		TRACE_FUNC();
		return FAudioVoice_EnableEffect(faudio_voice, EffectIndex, OperationSet);
	}

	X2METHOD(HRESULT) DisableEffect(
		UINT32 EffectIndex,
		UINT32 OperationSet = FAUDIO_COMMIT_NOW) {
		TRACE_FUNC();
		return FAudioVoice_DisableEffect(faudio_voice, EffectIndex, OperationSet);
	}

	X2METHOD(void) GetEffectState(UINT32 EffectIndex, BOOL* pEnabled) {
		TRACE_FUNC();
		uint8_t result;
		FAudioVoice_GetEffectState(faudio_voice, EffectIndex, &result);
		if (pEnabled) {
			*pEnabled = result;
		}
	}

	X2METHOD(HRESULT) SetEffectParameters(
		UINT32 EffectIndex,
		const void* pParameters,
		UINT32 ParametersByteSize,
		UINT32 OperationSet = FAUDIO_COMMIT_NOW) {
		TRACE_FUNC();
		return FAudioVoice_SetEffectParameters(faudio_voice, EffectIndex, pParameters, ParametersByteSize, OperationSet);
	}

	X2METHOD(HRESULT) GetEffectParameters(
		UINT32 EffectIndex,
		void* pParameters,
		UINT32 ParametersByteSize) {
		TRACE_FUNC();
		return FAudioVoice_GetEffectParameters(faudio_voice, EffectIndex, pParameters, ParametersByteSize);
	}

	X2METHOD(HRESULT) SetFilterParameters(
		const XAUDIO2_FILTER_PARAMETERS* pParameters,
		UINT32 OperationSet = FAUDIO_COMMIT_NOW) {
		TRACE_FUNC();
		return FAudioVoice_SetFilterParameters(faudio_voice, pParameters, OperationSet);
	}	

	X2METHOD(void) GetFilterParameters(XAUDIO2_FILTER_PARAMETERS* pParameters) {
		TRACE_FUNC();
		FAudioVoice_GetFilterParameters(faudio_voice, pParameters);
	}

	X2METHOD(HRESULT) SetOutputFilterParameters(
		IXAudio2Voice* pDestinationVoice,
		const XAUDIO2_FILTER_PARAMETERS* pParameters,
		UINT32 OperationSet = FAUDIO_COMMIT_NOW) {
		TRACE_FUNC();
		return FAudioVoice_SetOutputFilterParameters(faudio_voice, ((XAudio2SourceVoiceImpl *) pDestinationVoice)->faudio_voice, pParameters, OperationSet);
	}

	X2METHOD(void) GetOutputFilterParameters(
		IXAudio2Voice* pDestinationVoice,
		XAUDIO2_FILTER_PARAMETERS* pParameters) {
		TRACE_FUNC();
		FAudioVoice_GetOutputFilterParameters(faudio_voice, ((XAudio2SourceVoiceImpl *)pDestinationVoice)->faudio_voice, pParameters);
	}

	X2METHOD(HRESULT) SetVolume(float Volume, UINT32 OperationSet = FAUDIO_COMMIT_NOW) {
		TRACE_FUNC();
		return FAudioVoice_SetVolume(faudio_voice, Volume, OperationSet);
	}

	X2METHOD(void) GetVolume(float* pVolume) {
		TRACE_FUNC();
		FAudioVoice_GetVolume(faudio_voice, pVolume);
	}

	X2METHOD(HRESULT) SetChannelVolumes(
		UINT32 Channels,
		const float* pVolumes,
		UINT32 OperationSet = FAUDIO_COMMIT_NOW) {
		TRACE_FUNC();
		return FAudioVoice_SetChannelVolumes(faudio_voice, Channels, pVolumes, OperationSet);
	}

	X2METHOD(void) GetChannelVolumes(UINT32 Channels, float* pVolumes) {
		TRACE_FUNC();
		FAudioVoice_GetChannelVolumes(faudio_voice, Channels, pVolumes);
	}

	X2METHOD(HRESULT) SetOutputMatrix(
		IXAudio2Voice* pDestinationVoice,
		UINT32 SourceChannels,
		UINT32 DestinationChannels,
		const float* pLevelMatrix,
		UINT32 OperationSet = FAUDIO_COMMIT_NOW) {
		TRACE_PARAMS("this = %x; pDestinationVoice = %x; SourceChannels = %d; DestinationChannels = %d", this, pDestinationVoice, SourceChannels, DestinationChannels);
		return FAudioVoice_SetOutputMatrix(faudio_voice, pDestinationVoice->faudio_voice, SourceChannels, DestinationChannels, pLevelMatrix, OperationSet);
	}

	X2METHOD(void) GetOutputMatrix(
		IXAudio2Voice* pDestinationVoice,
		UINT32 SourceChannels,
		UINT32 DestinationChannels,
		float* pLevelMatrix) {
		TRACE_FUNC();
		FAudioVoice_GetOutputMatrix(faudio_voice, pDestinationVoice->faudio_voice, SourceChannels, DestinationChannels, pLevelMatrix);
	}

	X2METHOD(void) DestroyVoice() {
		TRACE_FUNC();
		FAudioVoice_DestroyVoice(faudio_voice);
		// FIXME: in theory FAudioVoice_DestroyVoice can fail but how would we ever now ? -JS
		if (voice_callback) {
			delete voice_callback;
		}
		free_voice_sends(voice_sends);
		delete this;
	}

	// IXAudio2SourceVoice
	X2METHOD(HRESULT) Start(UINT32 Flags = 0, UINT32 OperationSet = FAUDIO_COMMIT_NOW) {
		TRACE_FUNC();
		return FAudioSourceVoice_Start(faudio_voice, Flags, OperationSet);
	}

	X2METHOD(HRESULT) Stop(UINT32 Flags = 0, UINT32 OperationSet = FAUDIO_COMMIT_NOW) {
		TRACE_FUNC();
		return FAudioSourceVoice_Stop(faudio_voice, Flags, OperationSet);
	}

	X2METHOD(HRESULT) SubmitSourceBuffer(
		const XAUDIO2_BUFFER* pBuffer,
		const XAUDIO2_BUFFER_WMA* pBufferWMA = NULL) {
		TRACE_FUNC();
		return FAudioSourceVoice_SubmitSourceBuffer(faudio_voice, pBuffer, pBufferWMA);
	}

	X2METHOD(HRESULT) FlushSourceBuffers() {
		TRACE_FUNC();
		return FAudioSourceVoice_FlushSourceBuffers(faudio_voice);
	}

	X2METHOD(HRESULT) Discontinuity() {
		TRACE_FUNC();
		return FAudioSourceVoice_Discontinuity(faudio_voice);
	}

	X2METHOD(HRESULT) ExitLoop(UINT32 OperationSet = FAUDIO_COMMIT_NOW) {
		TRACE_FUNC();
		return FAudioSourceVoice_ExitLoop(faudio_voice, OperationSet);
	}

#if (XAUDIO2_VERSION <= 7)
	X2METHOD(void) GetState(XAUDIO2_VOICE_STATE* pVoiceState) {
#else
	X2METHOD(void) GetState(XAUDIO2_VOICE_STATE* pVoiceState, UINT32 Flags = 0) {
#endif
		FAudioSourceVoice_GetState(faudio_voice, pVoiceState);
	}

	X2METHOD(HRESULT) SetFrequencyRatio(float Ratio, UINT32 OperationSet = FAUDIO_COMMIT_NOW) {
		TRACE_FUNC();
		return FAudioSourceVoice_SetFrequencyRatio(faudio_voice, Ratio, OperationSet);
	}

	X2METHOD(void) GetFrequencyRatio(float* pRatio) {
		TRACE_FUNC();
		FAudioSourceVoice_GetFrequencyRatio(faudio_voice, pRatio);
	}

	X2METHOD(HRESULT) SetSourceSampleRate(UINT32 NewSourceSampleRate) {
		TRACE_FUNC();
		return FAudioSourceVoice_SetSourceSampleRate(faudio_voice, NewSourceSampleRate);
	}

private:
	FAudioVoiceCppCallback *voice_callback;
	FAudioVoiceSends *voice_sends;
};

///////////////////////////////////////////////////////////////////////////////
//
// IXAudio2SubmixVoice implementation
//

class XAudio2SubmixVoiceImpl : public IXAudio2SubmixVoice {
public:
	XAudio2SubmixVoiceImpl(
		FAudio *faudio,
		UINT32 InputChannels,
		UINT32 InputSampleRate,
		UINT32 Flags,
		UINT32 ProcessingStage,
		const XAUDIO2_VOICE_SENDS* pSendList,
		const XAUDIO2_EFFECT_CHAIN* pEffectChain) {
		TRACE_PARAMS("InputChannels = %d; InputSampleRate = %d; Flags = %d; ProcessingState = %d; EffectChain = %x", InputChannels, InputSampleRate, Flags, ProcessingStage, pEffectChain);
		voice_sends = unwrap_voice_sends(pSendList);
		FAudio_CreateSubmixVoice(faudio, &faudio_voice, InputChannels, InputSampleRate, Flags, ProcessingStage, voice_sends, NULL);	// FIXME
	}

	// IXAudio2Voice
	X2METHOD(void) GetVoiceDetails(XAUDIO2_VOICE_DETAILS* pVoiceDetails) {
		TRACE_FUNC();
		FAudioVoice_GetVoiceDetails(faudio_voice, pVoiceDetails);
	}

	X2METHOD(HRESULT) SetOutputVoices(const XAUDIO2_VOICE_SENDS* pSendList) {
		TRACE_FUNC();
		free_voice_sends(voice_sends);
		voice_sends = unwrap_voice_sends(pSendList);
		return FAudioVoice_SetOutputVoices(faudio_voice, voice_sends);
	}

	X2METHOD(HRESULT) SetEffectChain(const XAUDIO2_EFFECT_CHAIN* pEffectChain) {
		TRACE_FUNC();
		return FAudioVoice_SetEffectChain(faudio_voice, pEffectChain);
	}

	X2METHOD(HRESULT) EnableEffect(
		UINT32 EffectIndex,
		UINT32 OperationSet = FAUDIO_COMMIT_NOW) {
		TRACE_FUNC();
		return S_OK;		// FIXME
		// return FAudioVoice_EnableEffect(faudio_voice, EffectIndex, OperationSet);
	}

	X2METHOD(HRESULT) DisableEffect(
		UINT32 EffectIndex,
		UINT32 OperationSet = FAUDIO_COMMIT_NOW) {
		TRACE_FUNC();
		return S_OK;		// FIXME
		// return FAudioVoice_DisableEffect(faudio_voice, EffectIndex, OperationSet);
	}

	X2METHOD(void) GetEffectState(UINT32 EffectIndex, BOOL* pEnabled) {
		TRACE_FUNC();
		uint8_t result;
		FAudioVoice_GetEffectState(faudio_voice, EffectIndex, &result);
		if (pEnabled) {
			*pEnabled = result;
		}
	}

	X2METHOD(HRESULT) SetEffectParameters(
		UINT32 EffectIndex,
		const void* pParameters,
		UINT32 ParametersByteSize,
		UINT32 OperationSet = FAUDIO_COMMIT_NOW) {
		TRACE_FUNC();
		return S_OK;	// FIXME
		// return FAudioVoice_SetEffectParameters(faudio_voice, EffectIndex, pParameters, ParametersByteSize, OperationSet);
	}

	X2METHOD(HRESULT) GetEffectParameters(
		UINT32 EffectIndex,
		void* pParameters,
		UINT32 ParametersByteSize) {
		TRACE_FUNC();
		return FAudioVoice_GetEffectParameters(faudio_voice, EffectIndex, pParameters, ParametersByteSize);
	}

	X2METHOD(HRESULT) SetFilterParameters(
		const XAUDIO2_FILTER_PARAMETERS* pParameters,
		UINT32 OperationSet = FAUDIO_COMMIT_NOW) {
		TRACE_FUNC();
		return FAudioVoice_SetFilterParameters(faudio_voice, pParameters, OperationSet);
	}

	X2METHOD(void) GetFilterParameters(XAUDIO2_FILTER_PARAMETERS* pParameters) {
		TRACE_FUNC();
		FAudioVoice_GetFilterParameters(faudio_voice, pParameters);
	}

	X2METHOD(HRESULT) SetOutputFilterParameters(
		IXAudio2Voice* pDestinationVoice,
		const XAUDIO2_FILTER_PARAMETERS* pParameters,
		UINT32 OperationSet = FAUDIO_COMMIT_NOW) {
		TRACE_FUNC();
		return FAudioVoice_SetOutputFilterParameters(faudio_voice, ((XAudio2SubmixVoiceImpl *)pDestinationVoice)->faudio_voice, pParameters, OperationSet);
	}

	X2METHOD(void) GetOutputFilterParameters(
		IXAudio2Voice* pDestinationVoice,
		XAUDIO2_FILTER_PARAMETERS* pParameters) {
		TRACE_FUNC();
		FAudioVoice_GetOutputFilterParameters(faudio_voice, ((XAudio2SubmixVoiceImpl *)pDestinationVoice)->faudio_voice, pParameters);
	}

	X2METHOD(HRESULT) SetVolume(float Volume, UINT32 OperationSet = FAUDIO_COMMIT_NOW) {
		TRACE_FUNC();
		return FAudioVoice_SetVolume(faudio_voice, Volume, OperationSet);
	}

	X2METHOD(void) GetVolume(float* pVolume) {
		TRACE_FUNC();
		FAudioVoice_GetVolume(faudio_voice, pVolume);
	}

	X2METHOD(HRESULT) SetChannelVolumes(
		UINT32 Channels,
		const float* pVolumes,
		UINT32 OperationSet = FAUDIO_COMMIT_NOW) {
		TRACE_FUNC();
		return FAudioVoice_SetChannelVolumes(faudio_voice, Channels, pVolumes, OperationSet);
	}

	X2METHOD(void) GetChannelVolumes(UINT32 Channels, float* pVolumes) {
		TRACE_FUNC();
		FAudioVoice_GetChannelVolumes(faudio_voice, Channels, pVolumes);
	}

	X2METHOD(HRESULT) SetOutputMatrix(
		IXAudio2Voice* pDestinationVoice,
		UINT32 SourceChannels,
		UINT32 DestinationChannels,
		const float* pLevelMatrix,
		UINT32 OperationSet = FAUDIO_COMMIT_NOW) {
		TRACE_FUNC();
		return FAudioVoice_SetOutputMatrix(faudio_voice, pDestinationVoice->faudio_voice, SourceChannels, DestinationChannels, pLevelMatrix, OperationSet);
	}

	X2METHOD(void) GetOutputMatrix(
		IXAudio2Voice* pDestinationVoice,
		UINT32 SourceChannels,
		UINT32 DestinationChannels,
		float* pLevelMatrix) {
		TRACE_FUNC();
		FAudioVoice_GetOutputMatrix(faudio_voice, pDestinationVoice->faudio_voice, SourceChannels, DestinationChannels, pLevelMatrix);
	}

	X2METHOD(void) DestroyVoice() {
		FAudioVoice_DestroyVoice(faudio_voice);
		// FIXME: in theory FAudioVoice_DestroyVoice can fail but how would we ever now ? -JS
		TRACE_FUNC();
		delete this;
	}

private:
	FAudioVoiceSends *voice_sends;
};


///////////////////////////////////////////////////////////////////////////////
//
// IXAudio2MasteringVoice implementation
//

class XAudio2MasteringVoiceImpl : public IXAudio2MasteringVoice {
public:
#if (XAUDIO2_VERSION <= 7)
	XAudio2MasteringVoiceImpl(
		FAudio *faudio,
		UINT32 InputChannels,
		UINT32 InputSampleRate,
		UINT32 Flags,
		UINT32 DeviceIndex,
		const XAUDIO2_EFFECT_CHAIN* pEffectChain) {
		TRACE_PARAMS("InputChannels = %d; InputSampleRate = %d; Flags = %d; EffectChain = %x", InputChannels, InputSampleRate, Flags, pEffectChain);
		voice_sends = NULL;
		FAudio_CreateMasteringVoice(faudio, &faudio_voice, InputChannels, InputSampleRate, Flags, DeviceIndex, NULL);
	}
#else
	XAudio2MasteringVoiceImpl(
		FAudio *faudio,
		UINT32 InputChannels,
		UINT32 InputSampleRate,
		UINT32 Flags,
		LPCWSTR szDeviceId,
		const XAUDIO2_EFFECT_CHAIN* pEffectChain,
		int StreamCategory) {
		TRACE_FUNC();
		// FIXME device index
		FAudio_CreateMasteringVoice(faudio, &faudio_voice, InputChannels, InputSampleRate, Flags, 0, pEffectChain);
	}
#endif

	// IXAudio2Voice
	X2METHOD(void) GetVoiceDetails(XAUDIO2_VOICE_DETAILS* pVoiceDetails) {
		TRACE_FUNC();
		FAudioVoice_GetVoiceDetails(faudio_voice, pVoiceDetails);
	}

	X2METHOD(HRESULT) SetOutputVoices(const XAUDIO2_VOICE_SENDS* pSendList) {
		TRACE_FUNC();
		free_voice_sends(voice_sends);
		voice_sends = unwrap_voice_sends(pSendList);
		return FAudioVoice_SetOutputVoices(faudio_voice, voice_sends);
	}

	X2METHOD(HRESULT) SetEffectChain(const XAUDIO2_EFFECT_CHAIN* pEffectChain) {
		TRACE_FUNC();
		return FAudioVoice_SetEffectChain(faudio_voice, pEffectChain);
	}

	X2METHOD(HRESULT) EnableEffect(
		UINT32 EffectIndex,
		UINT32 OperationSet = FAUDIO_COMMIT_NOW) {
		TRACE_FUNC();
		return FAudioVoice_EnableEffect(faudio_voice, EffectIndex, OperationSet);
	}

	X2METHOD(HRESULT) DisableEffect(
		UINT32 EffectIndex,
		UINT32 OperationSet = FAUDIO_COMMIT_NOW) {
		TRACE_FUNC();
		return FAudioVoice_DisableEffect(faudio_voice, EffectIndex, OperationSet);
	}

	X2METHOD(void) GetEffectState(UINT32 EffectIndex, BOOL* pEnabled) {
		TRACE_FUNC();
		uint8_t result;
		FAudioVoice_GetEffectState(faudio_voice, EffectIndex, &result);
		if (pEnabled) {
			*pEnabled = result;
		}
	}

	X2METHOD(HRESULT) SetEffectParameters(
		UINT32 EffectIndex,
		const void* pParameters,
		UINT32 ParametersByteSize,
		UINT32 OperationSet = FAUDIO_COMMIT_NOW) {
		TRACE_FUNC();
		return FAudioVoice_SetEffectParameters(faudio_voice, EffectIndex, pParameters, ParametersByteSize, OperationSet);
	}

	X2METHOD(HRESULT) GetEffectParameters(
		UINT32 EffectIndex,
		void* pParameters,
		UINT32 ParametersByteSize) {
		TRACE_FUNC();
		return FAudioVoice_GetEffectParameters(faudio_voice, EffectIndex, pParameters, ParametersByteSize);
	}

	X2METHOD(HRESULT) SetFilterParameters(
		const XAUDIO2_FILTER_PARAMETERS* pParameters,
		UINT32 OperationSet = FAUDIO_COMMIT_NOW) {
		TRACE_FUNC();
		return FAudioVoice_SetFilterParameters(faudio_voice, pParameters, OperationSet);
	}

	X2METHOD(void) GetFilterParameters(XAUDIO2_FILTER_PARAMETERS* pParameters) {
		TRACE_FUNC();
		FAudioVoice_GetFilterParameters(faudio_voice, pParameters);
	}

	X2METHOD(HRESULT) SetOutputFilterParameters(
		IXAudio2Voice* pDestinationVoice,
		const XAUDIO2_FILTER_PARAMETERS* pParameters,
		UINT32 OperationSet = FAUDIO_COMMIT_NOW) {
		TRACE_FUNC();
		return FAudioVoice_SetOutputFilterParameters(faudio_voice, ((XAudio2MasteringVoiceImpl *)pDestinationVoice)->faudio_voice, pParameters, OperationSet);
	}

	X2METHOD(void) GetOutputFilterParameters(
		IXAudio2Voice* pDestinationVoice,
		XAUDIO2_FILTER_PARAMETERS* pParameters) {
		TRACE_FUNC();
		FAudioVoice_GetOutputFilterParameters(faudio_voice, ((XAudio2MasteringVoiceImpl *)pDestinationVoice)->faudio_voice, pParameters);
	}

	X2METHOD(HRESULT) SetVolume(float Volume, UINT32 OperationSet = FAUDIO_COMMIT_NOW) {
		TRACE_FUNC();
		return FAudioVoice_SetVolume(faudio_voice, Volume, OperationSet);
	}

	X2METHOD(void) GetVolume(float* pVolume) {
		TRACE_FUNC();
		FAudioVoice_GetVolume(faudio_voice, pVolume);
	}

	X2METHOD(HRESULT) SetChannelVolumes(
		UINT32 Channels,
		const float* pVolumes,
		UINT32 OperationSet = FAUDIO_COMMIT_NOW) {
		TRACE_FUNC();
		return FAudioVoice_SetChannelVolumes(faudio_voice, Channels, pVolumes, OperationSet);
	}

	X2METHOD(void) GetChannelVolumes(UINT32 Channels, float* pVolumes) {
		TRACE_FUNC();
		FAudioVoice_GetChannelVolumes(faudio_voice, Channels, pVolumes);
	}

	X2METHOD(HRESULT) SetOutputMatrix(
		IXAudio2Voice* pDestinationVoice,
		UINT32 SourceChannels,
		UINT32 DestinationChannels,
		const float* pLevelMatrix,
		UINT32 OperationSet = FAUDIO_COMMIT_NOW) {
		TRACE_FUNC();
		return FAudioVoice_SetOutputMatrix(faudio_voice, pDestinationVoice->faudio_voice, SourceChannels, DestinationChannels, pLevelMatrix, OperationSet);
	}

	X2METHOD(void) GetOutputMatrix(
		IXAudio2Voice* pDestinationVoice,
		UINT32 SourceChannels,
		UINT32 DestinationChannels,
		float* pLevelMatrix) {
		TRACE_FUNC();
		FAudioVoice_GetOutputMatrix(faudio_voice, pDestinationVoice->faudio_voice, SourceChannels, DestinationChannels, pLevelMatrix);
	}

	X2METHOD(void) DestroyVoice() {
		FAudioVoice_DestroyVoice(faudio_voice);
		// FIXME: in theory FAudioVoice_DestroyVoice can fail but how would we ever now ? -JS
		TRACE_FUNC();
		delete this;
	}

	// IXAudio2MasteringVoice
#if (XAUDIO2_VERSION >= 8)
	X2METHOD(HRESULT) GetChannelMask(DWORD* pChannelmask) {
		// FIXME
		return S_OK;
	}
#endif

private:
	FAudioVoiceSends *voice_sends;
};

///////////////////////////////////////////////////////////////////////////////
//
// IXAudio2 implementation
//

class XAudio2Impl : public IXAudio2 {
public:
	XAudio2Impl() {
		callback_list.com = NULL;
		callback_list.next = NULL;
		FAudio_Construct(&faudio);
	}

	XAudio2Impl(UINT32 Flags, XAUDIO2_PROCESSOR XAudio2Processor) {
		callback_list.com = NULL;
		callback_list.next = NULL;
		FAudioCreate(&faudio, Flags, XAudio2Processor);
	}

	X2METHOD(HRESULT) QueryInterface(REFIID riid, void** ppvInterface) {
		TRACE_FUNC();

		if (riid == IID_IXAudio2) {
			*ppvInterface = static_cast<IXAudio2 *>(this);
		} else if (riid == IID_IUnknown) {
			*ppvInterface = static_cast<IUnknown *>(this);
		} else {
			*ppvInterface = NULL;
			return E_NOINTERFACE;
		}

		reinterpret_cast<IUnknown *>(*ppvInterface)->AddRef();

		return S_OK;
	}

	X2METHOD(ULONG) AddRef() {
		TRACE_FUNC();
		return FAudio_AddRef(faudio);
	}

	X2METHOD(ULONG) Release() {
		TRACE_FUNC();
		ULONG refcount = FAudio_Release(faudio);
		if (refcount == 0) {
			delete this;
		}
		return 1;
	}

#if (XAUDIO2_VERSION <= 7)
	X2METHOD(HRESULT) GetDeviceCount(UINT32 *pCount) {
		TRACE_FUNC();
		return FAudio_GetDeviceCount(faudio, pCount);
	}

	X2METHOD(HRESULT) GetDeviceDetails(UINT32 Index, XAUDIO2_DEVICE_DETAILS* pDeviceDetails) {
		TRACE_FUNC();
		return FAudio_GetDeviceDetails(faudio, Index, pDeviceDetails);
	}

	X2METHOD(HRESULT) Initialize(
		UINT32 Flags = 0,
		XAUDIO2_PROCESSOR XAudio2Processor = FAUDIO_DEFAULT_PROCESSOR) {
		TRACE_FUNC();
		return FAudio_Initialize(faudio, Flags, XAudio2Processor);
	}
#endif // XAUDIO2_VERSION <= 7

	X2METHOD(HRESULT) RegisterForCallbacks(IXAudio2EngineCallback* pCallback) {
		TRACE_FUNC();
		FAudioCppEngineCallback *cb = wrap_engine_callback(pCallback);
		cb->next = callback_list.next;
		callback_list.next = cb;

		return FAudio_RegisterForCallbacks(faudio, reinterpret_cast<FAudioEngineCallback *>(cb));
	}

	X2METHOD(void) UnregisterForCallbacks(IXAudio2EngineCallback* pCallback) {
		TRACE_FUNC();
		FAudioCppEngineCallback *cb = find_and_remove_engine_callback(&callback_list, pCallback);

		if (cb == NULL) {
			return;
		}

		FAudio_UnregisterForCallbacks(faudio, reinterpret_cast<FAudioEngineCallback *>(cb));
		delete cb;
	}

	X2METHOD(HRESULT) CreateSourceVoice(
		IXAudio2SourceVoice** ppSourceVoice,
		const WAVEFORMATEX* pSourceFormat,
		UINT32 Flags = 0,
		float MaxFrequencyRatio = FAUDIO_DEFAULT_FREQ_RATIO,
		IXAudio2VoiceCallback* pCallback = NULL,
		const XAUDIO2_VOICE_SENDS* pSendList = NULL,
		const XAUDIO2_EFFECT_CHAIN* pEffectChain = NULL) {
		TRACE_FUNC();
		*ppSourceVoice = new XAudio2SourceVoiceImpl(faudio, pSourceFormat, Flags, MaxFrequencyRatio, pCallback, pSendList, pEffectChain);
		return S_OK;
	}

	X2METHOD(HRESULT) CreateSubmixVoice(
		IXAudio2SubmixVoice** ppSubmixVoice,
		UINT32 InputChannels, 
		UINT32 InputSampleRate,
		UINT32 Flags = 0,
		UINT32 ProcessingStage = 0,
		const XAUDIO2_VOICE_SENDS* pSendList = NULL,
		const XAUDIO2_EFFECT_CHAIN* pEffectChain = NULL) {
		TRACE_FUNC();
		*ppSubmixVoice = new XAudio2SubmixVoiceImpl(faudio, InputChannels, InputSampleRate, Flags, ProcessingStage, pSendList, pEffectChain);
		return S_OK;
	}

#if (XAUDIO2_VERSION <= 7)
	X2METHOD(HRESULT) CreateMasteringVoice(
		IXAudio2MasteringVoice** ppMasteringVoice,
		UINT32 InputChannels = FAUDIO_DEFAULT_CHANNELS,
		UINT32 InputSampleRate = FAUDIO_DEFAULT_SAMPLERATE,
		UINT32 Flags = 0,
		UINT32 DeviceIndex = 0,
		const XAUDIO2_EFFECT_CHAIN* pEffectChain = NULL) {
		TRACE_PARAMS("InputChannels = %d InputSampleRate = %d", InputChannels, InputSampleRate);
		*ppMasteringVoice = new XAudio2MasteringVoiceImpl(faudio, InputChannels, InputSampleRate, Flags, DeviceIndex, NULL);	// FIXME
		return S_OK;
	}
#else
	X2METHOD(HRESULT) CreateMasteringVoice(
		IXAudio2MasteringVoice** ppMasteringVoice,
		UINT32 InputChannels = FAUDIO_DEFAULT_CHANNELS,
		UINT32 InputSampleRate = FAUDIO_DEFAULT_SAMPLERATE,
		UINT32 Flags = 0,
		LPCWSTR szDeviceId = NULL,
		const XAUDIO2_EFFECT_CHAIN* pEffectChain = NULL,
		int StreamCategory = 6) {
		*ppMasteringVoice = new XAudio2MasteringVoiceImpl(faudio, InputChannels, InputSampleRate, Flags, szDeviceId, pEffectChain, StreamCategory);
		return S_OK;
	}
#endif

	X2METHOD(HRESULT) StartEngine() {
		TRACE_FUNC();
		return FAudio_StartEngine(faudio);
	}

	X2METHOD(void) StopEngine() {
		TRACE_FUNC();
		FAudio_StopEngine(faudio);
	}

	X2METHOD(HRESULT) CommitChanges(UINT32 OperationSet) {
		TRACE_FUNC();
		return FAudio_CommitChanges(faudio);
	}

	X2METHOD(void) GetPerformanceData(XAUDIO2_PERFORMANCE_DATA* pPerfData) {
		TRACE_FUNC();
		FAudio_GetPerformanceData(faudio, pPerfData);
	}

	X2METHOD(void) SetDebugConfiguration(
		const XAUDIO2_DEBUG_CONFIGURATION* pDebugConfiguration,
		void* pReserved = NULL) {
		TRACE_FUNC();
		// FIXME: const parameter in faudio API?
		FAudio_SetDebugConfiguration(faudio, const_cast<XAUDIO2_DEBUG_CONFIGURATION *> (pDebugConfiguration), pReserved);
	}

private:
	FAudio *faudio;
	FAudioCppEngineCallback	callback_list;
};

///////////////////////////////////////////////////////////////////////////////
//
// Create function
//

void *CreateXAudio2Internal() {
	return new XAudio2Impl();
}

FAUDIOCPP_API XAudio2Create(
	IXAudio2          **ppXAudio2,
	UINT32            Flags,
	XAUDIO2_PROCESSOR XAudio2Processor
) {
	// FAudio only accepts one processor
	*ppXAudio2 = new XAudio2Impl(Flags, FAUDIO_DEFAULT_PROCESSOR);
	return S_OK;
}

