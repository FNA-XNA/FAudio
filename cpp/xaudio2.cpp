#include "xaudio2.h"

#define S_OK 0
#define S_FALSE 1
#define E_NOTIMPL 80004001
#define E_NOINTERFACE 80004002
#define E_OUTOFMEMORY 0x8007000E
#define CLASS_E_NOAGGREGATION 0x80040110
#define CLASS_E_CLASSNOTAVAILABLE 0x80040111

const IID IID_IUnknown = { 0x00000000, 0x0000, 0x0000, {0xC0, 00, 00, 00, 00, 00, 00, 0x46}};
const IID IID_IClassFactory = { 0x00000001, 0x0000, 0x0000, {0xC0, 00, 00, 00, 00, 00, 00, 0x46}};
const IID IID_IXAudio2 = { 0x8bcf1f58, 0x9fe7, 0x4583, {0x8a, 0xc6, 0xe2, 0xad, 0xc4, 0x65, 0xc8, 0xbb }};

const IID CLSID_XAudio2_6 = { 0x3eda9b49, 0x2085, 0x498b, {0x9b, 0xb2, 0x39, 0xa6, 0x77, 0x84, 0x93, 0xde }};
const IID CLSID_XAudio2_7 = { 0x5a508685, 0xa254, 0x4fba, {0x9b, 0x82, 0x9a, 0x24, 0xb0, 0x03, 0x06, 0xaf }};

bool operator==(const IID &a, const IID &b) {
	return a.Data1 == b.Data1 &&
		   a.Data2 == b.Data2 &&
		   a.Data3 == b.Data3 &&
		   a.Data4[0] == b.Data4[0] &&
		   a.Data4[1] == b.Data4[1] &&
		   a.Data4[2] == b.Data4[2] &&
		   a.Data4[3] == b.Data4[3] &&
		   a.Data4[4] == b.Data4[4] &&
		   a.Data4[5] == b.Data4[5] &&
		   a.Data4[6] == b.Data4[6] &&
		   a.Data4[7] == b.Data4[7];
}

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

FAudioVoiceCallback *wrap_voice_callback(IXAudio2VoiceCallback *com_interface) {
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

	return reinterpret_cast<FAudioVoiceCallback *>(cb);
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
		FAudio_CreateSourceVoice(faudio, &faudio_voice, pSourceFormat, Flags, MaxFrequencyRatio, wrap_voice_callback(pCallback), pSendList, pEffectChain);
		
		// shady af
		format = pSourceFormat->wFormatTag;
		nBlockAlign = pSourceFormat->nBlockAlign;

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
		// XXX free?
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
		
		// quick hack
		if (pBuffer && pBuffer->PlayBegin == 0 && pBuffer->PlayLength == 0) {
			if (format == 1 /* PCM */ || format == 3 /* Float */) {
				((XAUDIO2_BUFFER *)pBuffer)->PlayLength = pBuffer->AudioBytes / nBlockAlign;
			} else if (format == 2 /* ADPCM */) {
				((XAUDIO2_BUFFER *)pBuffer)->PlayLength = pBuffer->AudioBytes;
			}
		}

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
	uint16_t format;
	uint16_t nBlockAlign;
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
		// XXX free?
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
		// XXX free?
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
	XAudio2Impl() {
		FAudio_Construct(&faudio);
	}

	XAudio2Impl(UINT32 Flags, XAUDIO2_PROCESSOR XAudio2Processor) {
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
		// FIXME
		return S_OK;
	}

	X2METHOD(void) UnregisterForCallbacks(IXAudio2EngineCallback* pCallback) {
		// FIXME
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
};

///////////////////////////////////////////////////////////////////////////////
//
// XAudio2Factory: fun COM stuff
//

class XAudio2Factory : public IClassFactory {
public:
	X2METHOD(HRESULT) QueryInterface(REFIID riid, void** ppvInterface) {
		if ((riid == IID_IUnknown) || (riid == IID_IClassFactory)) {
			*ppvInterface = static_cast<IClassFactory *>(this);
		} else {
			*ppvInterface = NULL;
			return E_NOINTERFACE;
		}
		
		reinterpret_cast<IUnknown *>(*ppvInterface)->AddRef();

		return S_OK;
	}

	X2METHOD(ULONG) AddRef() {
		// FIXME: locking
		return ++refcount;
	}

	X2METHOD(ULONG) Release() {
		// FIXME: locking
		long rc = --refcount;
	
		if (rc == 0) {
			delete this;
		}

		return rc;
	}

	X2METHOD(HRESULT) CreateInstance(IUnknown *pUnkOuter, REFIID riid, void **ppvObject) {
		if (pUnkOuter != NULL) {
			return CLASS_E_NOAGGREGATION;
		}
		
		if (!(riid == IID_IXAudio2)) {
			*ppvObject = NULL;
			return E_NOINTERFACE;
		}

		XAudio2Impl *xaudio2 = new XAudio2Impl();
		if (xaudio2 == NULL) {
			return E_OUTOFMEMORY;
		}

		return xaudio2->QueryInterface(riid, ppvObject);
	}

	X2METHOD(HRESULT) LockServer(BOOL fLock) {
		return E_NOTIMPL;
	}

private:
	long refcount;
};


///////////////////////////////////////////////////////////////////////////////
//
// Create function
//

FAUDIOCPP_API XAudio2Create(
	IXAudio2          **ppXAudio2,
	UINT32            Flags,
	XAUDIO2_PROCESSOR XAudio2Processor
) {
	// FAudio only accepts one processor
	*ppXAudio2 = new XAudio2Impl(Flags, FAUDIO_DEFAULT_PROCESSOR);
	return S_OK;
}

///////////////////////////////////////////////////////////////////////////////
//
// DLL functions
//

FAUDIOCPP_API DllCanUnloadNow() {
	return S_FALSE;
}

FAUDIOCPP_API DllGetClassObject(
	REFCLSID rclsid,
	REFIID   riid,
	LPVOID   *ppv) {

	if (rclsid == CLSID_XAudio2_6 || rclsid == CLSID_XAudio2_7) {
		XAudio2Factory *factory = new XAudio2Factory();
		if (!factory) {
			return E_OUTOFMEMORY;
		}
		return factory->QueryInterface(riid, ppv);
	}

	return CLASS_E_CLASSNOTAVAILABLE;
}

FAUDIOCPP_API DllRegisterServer() {
	return S_OK;
}

FAUDIOCPP_API DllUnregisterServer() {
	return S_OK;
}
