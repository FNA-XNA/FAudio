#include "xaudio2.h"

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

	FAudioCppEngineCallback *prev, *next;
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
	cb->prev = NULL;
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
		voice_callback = wrap_voice_callback(pCallback);
		FAudio_CreateSourceVoice(faudio, &faudio_voice, pSourceFormat, Flags, MaxFrequencyRatio, reinterpret_cast<FAudioVoiceCallback *>(voice_callback), pSendList, pEffectChain);
	}

	// IXAudio2Voice
	X2METHOD(void) GetVoiceDetails(XAUDIO2_VOICE_DETAILS* pVoiceDetails) {
		FAudioVoice_GetVoiceDetails(faudio_voice, pVoiceDetails);
	}

	X2METHOD(HRESULT) SetOutputVoices(const XAUDIO2_VOICE_SENDS* pSendList) {
		return FAudioVoice_SetOutputVoices(faudio_voice, pSendList);
	}

	X2METHOD(HRESULT) SetEffectChain(const XAUDIO2_EFFECT_CHAIN* pEffectChain) {
		return FAudioVoice_SetEffectChain(faudio_voice, pEffectChain);
	}

	X2METHOD(HRESULT) EnableEffect(
		UINT32 EffectIndex,
		UINT32 OperationSet = FAUDIO_COMMIT_NOW) {
		return FAudioVoice_EnableEffect(faudio_voice, EffectIndex, OperationSet);
	}

	X2METHOD(HRESULT) DisableEffect(
		UINT32 EffectIndex,
		UINT32 OperationSet = FAUDIO_COMMIT_NOW) {
		return FAudioVoice_DisableEffect(faudio_voice, EffectIndex, OperationSet);
	}

	X2METHOD(void) GetEffectState(UINT32 EffectIndex, BOOL* pEnabled) {
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
		return FAudioVoice_SetEffectParameters(faudio_voice, EffectIndex, pParameters, ParametersByteSize, OperationSet);
	}

	X2METHOD(HRESULT) GetEffectParameters(
		UINT32 EffectIndex,
		void* pParameters,
		UINT32 ParametersByteSize) {
		return FAudioVoice_GetEffectParameters(faudio_voice, EffectIndex, pParameters, ParametersByteSize);
	}

	X2METHOD(HRESULT) SetFilterParameters(
		const XAUDIO2_FILTER_PARAMETERS* pParameters,
		UINT32 OperationSet = FAUDIO_COMMIT_NOW) {
		return FAudioVoice_SetFilterParameters(faudio_voice, pParameters, OperationSet);
	}	

	X2METHOD(void) GetFilterParameters(XAUDIO2_FILTER_PARAMETERS* pParameters) {
		FAudioVoice_GetFilterParameters(faudio_voice, pParameters);
	}

	X2METHOD(HRESULT) SetOutputFilterParameters(
		IXAudio2Voice* pDestinationVoice,
		const XAUDIO2_FILTER_PARAMETERS* pParameters,
		UINT32 OperationSet = FAUDIO_COMMIT_NOW) {
		return FAudioVoice_SetOutputFilterParameters(faudio_voice, ((XAudio2SourceVoiceImpl *) pDestinationVoice)->faudio_voice, pParameters, OperationSet);
	}

	X2METHOD(void) GetOutputFilterParameters(
		IXAudio2Voice* pDestinationVoice,
		XAUDIO2_FILTER_PARAMETERS* pParameters) {
		FAudioVoice_GetOutputFilterParameters(faudio_voice, ((XAudio2SourceVoiceImpl *)pDestinationVoice)->faudio_voice, pParameters);
	}

	X2METHOD(HRESULT) SetVolume(float Volume, UINT32 OperationSet = FAUDIO_COMMIT_NOW) {
		return FAudioVoice_SetVolume(faudio_voice, Volume, OperationSet);
	}

	X2METHOD(void) GetVolume(float* pVolume) {
		FAudioVoice_GetVolume(faudio_voice, pVolume);
	}

	X2METHOD(HRESULT) SetChannelVolumes(
		UINT32 Channels,
		const float* pVolumes,
		UINT32 OperationSet = FAUDIO_COMMIT_NOW) {
		return FAudioVoice_SetChannelVolumes(faudio_voice, Channels, pVolumes, OperationSet);
	}

	X2METHOD(void) GetChannelVolumes(UINT32 Channels, float* pVolumes) {
		FAudioVoice_GetChannelVolumes(faudio_voice, Channels, pVolumes);
	}

	X2METHOD(HRESULT) SetOutputMatrix(
		IXAudio2Voice* pDestinationVoice,
		UINT32 SourceChannels,
		UINT32 DestinationChannels,
		const float* pLevelMatrix,
		UINT32 OperationSet = FAUDIO_COMMIT_NOW) {
		return FAudioVoice_SetOutputMatrix(faudio_voice, ((XAudio2SourceVoiceImpl *) pDestinationVoice)->faudio_voice, SourceChannels, DestinationChannels, pLevelMatrix, OperationSet);
	}

	X2METHOD(void) GetOutputMatrix(
		IXAudio2Voice* pDestinationVoice,
		UINT32 SourceChannels,
		UINT32 DestinationChannels,
		float* pLevelMatrix) {
		FAudioVoice_GetOutputMatrix(faudio_voice, ((XAudio2SourceVoiceImpl *)pDestinationVoice)->faudio_voice, SourceChannels, DestinationChannels, pLevelMatrix);
	}

	X2METHOD(void) DestroyVoice() {
		FAudioVoice_DestroyVoice(faudio_voice);
		// FIXME: in theory FAudioVoice_DestroyVoice can fail but how would we ever now ? -JS
		if (voice_callback) {
			delete voice_callback;
		}
		delete this;
	}

	// IXAudio2SourceVoice
	X2METHOD(HRESULT) Start(UINT32 Flags = 0, UINT32 OperationSet = FAUDIO_COMMIT_NOW) {
		return FAudioSourceVoice_Start(faudio_voice, Flags, OperationSet);
	}

	X2METHOD(HRESULT) Stop(UINT32 Flags = 0, UINT32 OperationSet = FAUDIO_COMMIT_NOW) {
		return FAudioSourceVoice_Stop(faudio_voice, Flags, OperationSet);
	}

	X2METHOD(HRESULT) SubmitSourceBuffer(
		const XAUDIO2_BUFFER* pBuffer,
		const XAUDIO2_BUFFER_WMA* pBufferWMA = NULL) {
		return FAudioSourceVoice_SubmitSourceBuffer(faudio_voice, pBuffer, pBufferWMA);
	}

	X2METHOD(HRESULT) FlushSourceBuffers() {
		return FAudioSourceVoice_FlushSourceBuffers(faudio_voice);
	}

	X2METHOD(HRESULT) Discontinuity() {
		return FAudioSourceVoice_Discontinuity(faudio_voice);
	}

	X2METHOD(HRESULT) ExitLoop(UINT32 OperationSet = FAUDIO_COMMIT_NOW) {
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
		return FAudioSourceVoice_SetFrequencyRatio(faudio_voice, Ratio, OperationSet);
	}

	X2METHOD(void) GetFrequencyRatio(float* pRatio) {
		FAudioSourceVoice_GetFrequencyRatio(faudio_voice, pRatio);
	}

	X2METHOD(HRESULT) SetSourceSampleRate(UINT32 NewSourceSampleRate) {
		return FAudioSourceVoice_SetSourceSampleRate(faudio_voice, NewSourceSampleRate);
	}

private:
	FAudioSourceVoice *faudio_voice;
	FAudioVoiceCppCallback *voice_callback;
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
		FAudio_CreateSubmixVoice(faudio, &faudio_voice, InputChannels, InputSampleRate, Flags, ProcessingStage, pSendList, pEffectChain);
	}

	// IXAudio2Voice
	X2METHOD(void) GetVoiceDetails(XAUDIO2_VOICE_DETAILS* pVoiceDetails) {
		FAudioVoice_GetVoiceDetails(faudio_voice, pVoiceDetails);
	}

	X2METHOD(HRESULT) SetOutputVoices(const XAUDIO2_VOICE_SENDS* pSendList) {
		return FAudioVoice_SetOutputVoices(faudio_voice, pSendList);
	}

	X2METHOD(HRESULT) SetEffectChain(const XAUDIO2_EFFECT_CHAIN* pEffectChain) {
		return FAudioVoice_SetEffectChain(faudio_voice, pEffectChain);
	}

	X2METHOD(HRESULT) EnableEffect(
		UINT32 EffectIndex,
		UINT32 OperationSet = FAUDIO_COMMIT_NOW) {
		return FAudioVoice_EnableEffect(faudio_voice, EffectIndex, OperationSet);
	}

	X2METHOD(HRESULT) DisableEffect(
		UINT32 EffectIndex,
		UINT32 OperationSet = FAUDIO_COMMIT_NOW) {
		return FAudioVoice_DisableEffect(faudio_voice, EffectIndex, OperationSet);
	}

	X2METHOD(void) GetEffectState(UINT32 EffectIndex, BOOL* pEnabled) {
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
		return FAudioVoice_SetEffectParameters(faudio_voice, EffectIndex, pParameters, ParametersByteSize, OperationSet);
	}

	X2METHOD(HRESULT) GetEffectParameters(
		UINT32 EffectIndex,
		void* pParameters,
		UINT32 ParametersByteSize) {
		return FAudioVoice_GetEffectParameters(faudio_voice, EffectIndex, pParameters, ParametersByteSize);
	}

	X2METHOD(HRESULT) SetFilterParameters(
		const XAUDIO2_FILTER_PARAMETERS* pParameters,
		UINT32 OperationSet = FAUDIO_COMMIT_NOW) {
		return FAudioVoice_SetFilterParameters(faudio_voice, pParameters, OperationSet);
	}

	X2METHOD(void) GetFilterParameters(XAUDIO2_FILTER_PARAMETERS* pParameters) {
		FAudioVoice_GetFilterParameters(faudio_voice, pParameters);
	}

	X2METHOD(HRESULT) SetOutputFilterParameters(
		IXAudio2Voice* pDestinationVoice,
		const XAUDIO2_FILTER_PARAMETERS* pParameters,
		UINT32 OperationSet = FAUDIO_COMMIT_NOW) {
		return FAudioVoice_SetOutputFilterParameters(faudio_voice, ((XAudio2SubmixVoiceImpl *)pDestinationVoice)->faudio_voice, pParameters, OperationSet);
	}

	X2METHOD(void) GetOutputFilterParameters(
		IXAudio2Voice* pDestinationVoice,
		XAUDIO2_FILTER_PARAMETERS* pParameters) {
		FAudioVoice_GetOutputFilterParameters(faudio_voice, ((XAudio2SubmixVoiceImpl *)pDestinationVoice)->faudio_voice, pParameters);
	}

	X2METHOD(HRESULT) SetVolume(float Volume, UINT32 OperationSet = FAUDIO_COMMIT_NOW) {
		return FAudioVoice_SetVolume(faudio_voice, Volume, OperationSet);
	}

	X2METHOD(void) GetVolume(float* pVolume) {
		FAudioVoice_GetVolume(faudio_voice, pVolume);
	}

	X2METHOD(HRESULT) SetChannelVolumes(
		UINT32 Channels,
		const float* pVolumes,
		UINT32 OperationSet = FAUDIO_COMMIT_NOW) {
		return FAudioVoice_SetChannelVolumes(faudio_voice, Channels, pVolumes, OperationSet);
	}

	X2METHOD(void) GetChannelVolumes(UINT32 Channels, float* pVolumes) {
		FAudioVoice_GetChannelVolumes(faudio_voice, Channels, pVolumes);
	}

	X2METHOD(HRESULT) SetOutputMatrix(
		IXAudio2Voice* pDestinationVoice,
		UINT32 SourceChannels,
		UINT32 DestinationChannels,
		const float* pLevelMatrix,
		UINT32 OperationSet = FAUDIO_COMMIT_NOW) {
		return FAudioVoice_SetOutputMatrix(faudio_voice, ((XAudio2SubmixVoiceImpl *)pDestinationVoice)->faudio_voice, SourceChannels, DestinationChannels, pLevelMatrix, OperationSet);
	}

	X2METHOD(void) GetOutputMatrix(
		IXAudio2Voice* pDestinationVoice,
		UINT32 SourceChannels,
		UINT32 DestinationChannels,
		float* pLevelMatrix) {
		FAudioVoice_GetOutputMatrix(faudio_voice, ((XAudio2SubmixVoiceImpl *)pDestinationVoice)->faudio_voice, SourceChannels, DestinationChannels, pLevelMatrix);
	}

	X2METHOD(void) DestroyVoice() {
		FAudioVoice_DestroyVoice(faudio_voice);
		// FIXME: in theory FAudioVoice_DestroyVoice can fail but how would we ever now ? -JS
		delete this;
	}

private:
	FAudioSourceVoice *faudio_voice;
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
		FAudio_CreateMasteringVoice(faudio, &faudio_voice, InputChannels, InputSampleRate, Flags, DeviceIndex, pEffectChain);
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
		// FIXME device index
		FAudio_CreateMasteringVoice(faudio, &faudio_voice, InputChannels, InputSampleRate, Flags, 0, pEffectChain);
	}
#endif

	// IXAudio2Voice
	X2METHOD(void) GetVoiceDetails(XAUDIO2_VOICE_DETAILS* pVoiceDetails) {
		FAudioVoice_GetVoiceDetails(faudio_voice, pVoiceDetails);
	}

	X2METHOD(HRESULT) SetOutputVoices(const XAUDIO2_VOICE_SENDS* pSendList) {
		return FAudioVoice_SetOutputVoices(faudio_voice, pSendList);
	}

	X2METHOD(HRESULT) SetEffectChain(const XAUDIO2_EFFECT_CHAIN* pEffectChain) {
		return FAudioVoice_SetEffectChain(faudio_voice, pEffectChain);
	}

	X2METHOD(HRESULT) EnableEffect(
		UINT32 EffectIndex,
		UINT32 OperationSet = FAUDIO_COMMIT_NOW) {
		return FAudioVoice_EnableEffect(faudio_voice, EffectIndex, OperationSet);
	}

	X2METHOD(HRESULT) DisableEffect(
		UINT32 EffectIndex,
		UINT32 OperationSet = FAUDIO_COMMIT_NOW) {
		return FAudioVoice_DisableEffect(faudio_voice, EffectIndex, OperationSet);
	}

	X2METHOD(void) GetEffectState(UINT32 EffectIndex, BOOL* pEnabled) {
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
		return FAudioVoice_SetEffectParameters(faudio_voice, EffectIndex, pParameters, ParametersByteSize, OperationSet);
	}

	X2METHOD(HRESULT) GetEffectParameters(
		UINT32 EffectIndex,
		void* pParameters,
		UINT32 ParametersByteSize) {
		return FAudioVoice_GetEffectParameters(faudio_voice, EffectIndex, pParameters, ParametersByteSize);
	}

	X2METHOD(HRESULT) SetFilterParameters(
		const XAUDIO2_FILTER_PARAMETERS* pParameters,
		UINT32 OperationSet = FAUDIO_COMMIT_NOW) {
		return FAudioVoice_SetFilterParameters(faudio_voice, pParameters, OperationSet);
	}

	X2METHOD(void) GetFilterParameters(XAUDIO2_FILTER_PARAMETERS* pParameters) {
		FAudioVoice_GetFilterParameters(faudio_voice, pParameters);
	}

	X2METHOD(HRESULT) SetOutputFilterParameters(
		IXAudio2Voice* pDestinationVoice,
		const XAUDIO2_FILTER_PARAMETERS* pParameters,
		UINT32 OperationSet = FAUDIO_COMMIT_NOW) {
		return FAudioVoice_SetOutputFilterParameters(faudio_voice, ((XAudio2MasteringVoiceImpl *)pDestinationVoice)->faudio_voice, pParameters, OperationSet);
	}

	X2METHOD(void) GetOutputFilterParameters(
		IXAudio2Voice* pDestinationVoice,
		XAUDIO2_FILTER_PARAMETERS* pParameters) {
		FAudioVoice_GetOutputFilterParameters(faudio_voice, ((XAudio2MasteringVoiceImpl *)pDestinationVoice)->faudio_voice, pParameters);
	}

	X2METHOD(HRESULT) SetVolume(float Volume, UINT32 OperationSet = FAUDIO_COMMIT_NOW) {
		return FAudioVoice_SetVolume(faudio_voice, Volume, OperationSet);
	}

	X2METHOD(void) GetVolume(float* pVolume) {
		FAudioVoice_GetVolume(faudio_voice, pVolume);
	}

	X2METHOD(HRESULT) SetChannelVolumes(
		UINT32 Channels,
		const float* pVolumes,
		UINT32 OperationSet = FAUDIO_COMMIT_NOW) {
		return FAudioVoice_SetChannelVolumes(faudio_voice, Channels, pVolumes, OperationSet);
	}

	X2METHOD(void) GetChannelVolumes(UINT32 Channels, float* pVolumes) {
		FAudioVoice_GetChannelVolumes(faudio_voice, Channels, pVolumes);
	}

	X2METHOD(HRESULT) SetOutputMatrix(
		IXAudio2Voice* pDestinationVoice,
		UINT32 SourceChannels,
		UINT32 DestinationChannels,
		const float* pLevelMatrix,
		UINT32 OperationSet = FAUDIO_COMMIT_NOW) {
		return FAudioVoice_SetOutputMatrix(faudio_voice, ((XAudio2MasteringVoiceImpl *)pDestinationVoice)->faudio_voice, SourceChannels, DestinationChannels, pLevelMatrix, OperationSet);
	}

	X2METHOD(void) GetOutputMatrix(
		IXAudio2Voice* pDestinationVoice,
		UINT32 SourceChannels,
		UINT32 DestinationChannels,
		float* pLevelMatrix) {
		FAudioVoice_GetOutputMatrix(faudio_voice, ((XAudio2MasteringVoiceImpl *)pDestinationVoice)->faudio_voice, SourceChannels, DestinationChannels, pLevelMatrix);
	}

	X2METHOD(void) DestroyVoice() {
		FAudioVoice_DestroyVoice(faudio_voice);
		// FIXME: in theory FAudioVoice_DestroyVoice can fail but how would we ever now ? -JS
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
	FAudioSourceVoice *faudio_voice;
};

///////////////////////////////////////////////////////////////////////////////
//
// IXAudio2 implementation
//

class XAudio2Impl : public IXAudio2 {
public:
	XAudio2Impl() : callback_list({0}) {
		FAudio_Construct(&faudio);
	}

	XAudio2Impl(UINT32 Flags, XAUDIO2_PROCESSOR XAudio2Processor) : callback_list({0}) {
		FAudioCreate(&faudio, Flags, XAudio2Processor);
	}

	X2METHOD(HRESULT) QueryInterface(REFIID riid, void** ppvInterface) {
		
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
		return FAudio_AddRef(faudio);
	}

	X2METHOD(ULONG) Release() {
		ULONG refcount = FAudio_Release(faudio);
		if (refcount == 0) {
			delete this;
		}
		return 1;
	}

#if (XAUDIO2_VERSION <= 7)
	X2METHOD(HRESULT) GetDeviceCount(UINT32 *pCount) {
		return FAudio_GetDeviceCount(faudio, pCount);
	}

	X2METHOD(HRESULT) GetDeviceDetails(UINT32 Index, XAUDIO2_DEVICE_DETAILS* pDeviceDetails) {
		return FAudio_GetDeviceDetails(faudio, Index, pDeviceDetails);
	}

	X2METHOD(HRESULT) Initialize(
		UINT32 Flags = 0,
		XAUDIO2_PROCESSOR XAudio2Processor = FAUDIO_DEFAULT_PROCESSOR) {
		return FAudio_Initialize(faudio, Flags, XAudio2Processor);
	}
#endif // XAUDIO2_VERSION <= 7

	X2METHOD(HRESULT) RegisterForCallbacks(IXAudio2EngineCallback* pCallback) {
		FAudioCppEngineCallback *cb = wrap_engine_callback(pCallback);
		cb->next = callback_list.next;
		callback_list.next = cb;

		return FAudio_RegisterForCallbacks(faudio, reinterpret_cast<FAudioEngineCallback *>(cb));
	}

	X2METHOD(void) UnregisterForCallbacks(IXAudio2EngineCallback* pCallback) {
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
		*ppMasteringVoice = new XAudio2MasteringVoiceImpl(faudio, InputChannels, InputSampleRate, Flags, DeviceIndex, pEffectChain);
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
		return FAudio_StartEngine(faudio);
	}

	X2METHOD(void) StopEngine() {
		FAudio_StopEngine(faudio);
	}

	X2METHOD(HRESULT) CommitChanges(UINT32 OperationSet) {
		return FAudio_CommitChanges(faudio);
	}

	X2METHOD(void) GetPerformanceData(XAUDIO2_PERFORMANCE_DATA* pPerfData) {
		FAudio_GetPerformanceData(faudio, pPerfData);
	}

	X2METHOD(void) SetDebugConfiguration(
		const XAUDIO2_DEBUG_CONFIGURATION* pDebugConfiguration,
		void* pReserved = NULL) {
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

IUnknown *CreateXAudio2Internal(void) {
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

