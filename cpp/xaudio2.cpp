#include "xaudio2.h"
#include "XAPO.h"

#include <FAPOBase.h>

//#define TRACING_ENABLE
#include "trace.h"

///////////////////////////////////////////////////////////////////////////////
//
// IXAudio2VoiceCallback
//

struct FAudioVoiceCppCallback
{
	FAudioVoiceCallback callbacks;
	IXAudio2VoiceCallback *com;
};

static void FAUDIOCALL OnBufferEnd(FAudioVoiceCallback *callback, void *pBufferContext)
{
	reinterpret_cast<FAudioVoiceCppCallback *>(callback)->com->OnBufferEnd(pBufferContext);
}

static void FAUDIOCALL OnBufferStart(FAudioVoiceCallback *callback, void *pBufferContext)
{
	reinterpret_cast<FAudioVoiceCppCallback *>(callback)->com->OnBufferStart(pBufferContext);
}

static void FAUDIOCALL OnLoopEnd(FAudioVoiceCallback *callback, void *pBufferContext)
{
	reinterpret_cast<FAudioVoiceCppCallback *>(callback)->com->OnLoopEnd(pBufferContext);
}

static void FAUDIOCALL OnStreamEnd(FAudioVoiceCallback *callback)
{
	reinterpret_cast<FAudioVoiceCppCallback *>(callback)->com->OnStreamEnd();
}

static void FAUDIOCALL OnVoiceError(
	FAudioVoiceCallback *callback, 
	void *pBufferContext, 
	uint32_t Error
) {
	reinterpret_cast<FAudioVoiceCppCallback *>(callback)->com->OnVoiceError(pBufferContext, Error);
}

static void FAUDIOCALL OnVoiceProcessingPassEnd(FAudioVoiceCallback *callback)
{
	reinterpret_cast<FAudioVoiceCppCallback *>(callback)->com->OnVoiceProcessingPassEnd();
}

static void FAUDIOCALL OnVoiceProcessingPassStart(
	FAudioVoiceCallback *callback, 
	uint32_t BytesRequired
) {
#if XAUDIO2_VERSION >= 1
	reinterpret_cast<FAudioVoiceCppCallback *>(callback)->com->OnVoiceProcessingPassStart(
		BytesRequired);
#else
	reinterpret_cast<FAudioVoiceCppCallback *>(callback)->com->OnVoiceProcessingPassStart();
#endif // XAUDIO2_VERSION >= 1
}

FAudioVoiceCppCallback *wrap_voice_callback(IXAudio2VoiceCallback *com_interface)
{
	if (com_interface == NULL)
	{
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

struct FAudioCppEngineCallback
{
	FAudioEngineCallback callbacks;
	IXAudio2EngineCallback *com;

	FAudioCppEngineCallback *next;
};

static void FAUDIOCALL OnCriticalError(FAudioEngineCallback *callback, uint32_t Error)
{
	reinterpret_cast<FAudioCppEngineCallback *>(callback)->com->OnCriticalError(Error);
}

static void FAUDIOCALL OnProcessingPassEnd(FAudioEngineCallback *callback)
{
	reinterpret_cast<FAudioCppEngineCallback *>(callback)->com->OnProcessingPassEnd();
}

static void FAUDIOCALL OnProcessingPassStart(FAudioEngineCallback *callback)
{
	reinterpret_cast<FAudioCppEngineCallback *>(callback)->com->OnProcessingPassStart();
}

static FAudioCppEngineCallback *wrap_engine_callback(IXAudio2EngineCallback *com_interface)
{
	if (com_interface == NULL)
	{
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

static FAudioCppEngineCallback *find_and_remove_engine_callback(
	FAudioCppEngineCallback *list, 
	IXAudio2EngineCallback *com
) {
	FAudioCppEngineCallback *last = list;

	for (FAudioCppEngineCallback *it = list->next; it != NULL; it = it->next)
	{
		if (it->com == com)
		{
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

static FAudioVoiceSends *unwrap_voice_sends(const XAUDIO2_VOICE_SENDS *x_sends)
{
	if (x_sends == NULL)
	{
		return NULL;
	}
	TRACE_MSG("SendCount = %d", x_sends->SendCount);

	FAudioVoiceSends *f_sends = new FAudioVoiceSends;
	f_sends->SendCount = x_sends->SendCount;
	f_sends->pSends = new FAudioSendDescriptor[f_sends->SendCount];

	for (uint32_t i = 0; i < f_sends->SendCount; ++i)
	{
#if XAUDIO2_VERSION >= 4
		f_sends->pSends[i].Flags = x_sends->pSends[i].Flags;
		f_sends->pSends[i].pOutputVoice = x_sends->pSends[i].pOutputVoice->faudio_voice;
#else
		f_sends->pSends[i].Flags = 0;
		f_sends->pSends[i].pOutputVoice = x_sends->pSends[i]->faudio_voice;
#endif // XAUDIO2_VERSION >= 4
	}

	return f_sends;
}

static void free_voice_sends(FAudioVoiceSends *f_sends)
{
	if (f_sends != NULL)
	{
		delete[] f_sends->pSends;
		delete f_sends;
	}
}

///////////////////////////////////////////////////////////////////////////////
//
// XAUDIO2_EFFECT_CHAIN / XAUDIO2_EFFECT_DESCRIPTOR => FAudio
//

struct FAPOCppParametersBase
{
	FAPOParametersBase fapo;
	IXAPO *xapo;
	IXAPOParameters *xapo_params;
};

static uint32_t FAPOCALL GetRegistrationProperties(
	void *fapo, 
	FAPORegistrationProperties **ppRegistrationProperties)
{
	TRACE_FUNC();
	IXAPO *xapo = reinterpret_cast<FAPOCppParametersBase *>(fapo)->xapo;
	return xapo->GetRegistrationProperties(ppRegistrationProperties);
}

static uint32_t FAPOCALL IsInputFormatSupported(
	void *fapo,
	const FAudioWaveFormatEx *pOutputFormat,
	const FAudioWaveFormatEx *pRequestedInputFormat,
	FAudioWaveFormatEx **ppSupportedInputFormat
) {
	TRACE_FUNC();
	IXAPO *xapo = reinterpret_cast<FAPOCppParametersBase *>(fapo)->xapo;
	return xapo->IsInputFormatSupported(
		pOutputFormat, 
		pRequestedInputFormat, 
		ppSupportedInputFormat);
}

static uint32_t FAPOCALL IsOutputFormatSupported(
	void *fapo,
	const FAudioWaveFormatEx *pInputFormat,
	const FAudioWaveFormatEx *pRequestedOutputFormat,
	FAudioWaveFormatEx **ppSupportedOutputFormat
) {
	TRACE_FUNC();
	IXAPO *xapo = reinterpret_cast<FAPOCppParametersBase *>(fapo)->xapo;
	return xapo->IsOutputFormatSupported(
		pInputFormat, 
		pRequestedOutputFormat, 
		ppSupportedOutputFormat);
}

static uint32_t FAPOCALL Initialize(void *fapo, const void *pData, uint32_t DataByteSize)
{
	TRACE_FUNC();
	IXAPO *xapo = reinterpret_cast<FAPOCppParametersBase *>(fapo)->xapo;
	return xapo->Initialize(pData, DataByteSize);
}

static void FAPOCALL Reset(void *fapo)
{
	TRACE_FUNC();
	IXAPO *xapo = reinterpret_cast<FAPOCppParametersBase *>(fapo)->xapo;
	xapo->Reset();
}

static uint32_t FAPOCALL LockForProcess(
	void *fapo,
	uint32_t InputLockedParameterCount,
	const FAPOLockForProcessBufferParameters *pInputLockedParameters,
	uint32_t OutputLockedParameterCount,
	const FAPOLockForProcessBufferParameters *pOutputLockedParameters
) {
	TRACE_FUNC();
	IXAPO *xapo = reinterpret_cast<FAPOCppParametersBase *>(fapo)->xapo;
	return xapo->LockForProcess(
		InputLockedParameterCount,
		pInputLockedParameters,
		OutputLockedParameterCount,
		pOutputLockedParameters);
}

static void FAPOCALL UnlockForProcess(void *fapo)
{
	TRACE_FUNC();
	IXAPO *xapo = reinterpret_cast<FAPOCppParametersBase *>(fapo)->xapo;
	xapo->UnlockForProcess();
}

static void FAPOCALL Process(
	void *fapo,
	uint32_t InputProcessParameterCount,
	const FAPOProcessBufferParameters *pInputProcessParameters,
	uint32_t OutputProcessParameterCount,
	FAPOProcessBufferParameters *pOutputProcessParameters,
	uint8_t IsEnabled
) {
	TRACE_FUNC();
	IXAPO *xapo = reinterpret_cast<FAPOCppParametersBase *>(fapo)->xapo;
	xapo->Process(
		InputProcessParameterCount,
		pInputProcessParameters,
		OutputProcessParameterCount,
		pOutputProcessParameters,
		IsEnabled);
}

static uint32_t FAPOCALL CalcInputFrames(void *fapo, uint32_t OutputFrameCount)
{
	TRACE_FUNC();
	IXAPO *xapo = reinterpret_cast<FAPOCppParametersBase *>(fapo)->xapo;
	return xapo->CalcInputFrames(OutputFrameCount);
}

static uint32_t FAPOCALL CalcOutputFrames(void *fapo, uint32_t InputFrameCount)
{
	TRACE_FUNC();
	IXAPO *xapo = reinterpret_cast<FAPOCppParametersBase *>(fapo)->xapo;
	return xapo->CalcOutputFrames(InputFrameCount);
}

static void FAPOCALL SetParameters(
	void *fapoParameters, 
	const void *pParameters, 
	uint32_t ParameterByteSize
) {
	TRACE_FUNC();
	IXAPOParameters *xapo_params = reinterpret_cast<FAPOCppParametersBase *>(fapoParameters)->xapo_params;
	return xapo_params->SetParameters(pParameters, ParameterByteSize); 
}

static void FAPOCALL GetParameters(void *fapoParameters, void *pParameters, uint32_t ParameterByteSize)
{
	TRACE_FUNC();
	IXAPOParameters *xapo_params = reinterpret_cast<FAPOCppParametersBase *>(fapoParameters)->xapo_params;
	return xapo_params->GetParameters(pParameters, ParameterByteSize);
}

static void FAPOCALL Destructor(void *fapo)
{
	TRACE_FUNC();
	delete reinterpret_cast<FAPOCppParametersBase *>(fapo);
}

static void *wrap_xapo_effect(IUnknown *xapo)
{
	if (xapo == NULL)
	{
		return NULL;
	}

	TRACE_FUNC();

	// FIXME: assumes that all effects are derived from CXAPOParametersBase
	FAPOCppParametersBase *f_effect = new FAPOCppParametersBase;
	xapo->QueryInterface(IID_IXAPO, (void **)&f_effect->xapo);
	xapo->QueryInterface(IID_IXAPOParameters, (void **)&f_effect->xapo_params);

	TRACE_MSG("IXAPO: %x; IXAPOParameters: %x", f_effect->xapo, f_effect->xapo_params);

	f_effect->fapo.base.base.GetRegistrationProperties = GetRegistrationProperties;
	f_effect->fapo.base.base.IsInputFormatSupported = IsInputFormatSupported;
	f_effect->fapo.base.base.IsOutputFormatSupported = IsOutputFormatSupported;
	f_effect->fapo.base.base.Initialize = Initialize;
	f_effect->fapo.base.base.Reset = Reset;
	f_effect->fapo.base.base.LockForProcess = LockForProcess;
	f_effect->fapo.base.base.UnlockForProcess = UnlockForProcess;
	f_effect->fapo.base.base.Process = Process;
	f_effect->fapo.base.base.CalcInputFrames = CalcInputFrames;
	f_effect->fapo.base.base.CalcOutputFrames = CalcOutputFrames;

	f_effect->fapo.parameters.GetParameters = GetParameters;
	f_effect->fapo.parameters.SetParameters = SetParameters;

	f_effect->fapo.base.Destructor = Destructor;
	return f_effect;
}

static FAudioEffectChain *wrap_effect_chain(const XAUDIO2_EFFECT_CHAIN *x_chain)
{
	if (x_chain == NULL)
	{
		return NULL;
	}

	TRACE_MSG("EffectCount = %d", x_chain->EffectCount);

	FAudioEffectChain *f_chain = new FAudioEffectChain;
	f_chain->EffectCount = x_chain->EffectCount;
	f_chain->pEffectDescriptors = new FAudioEffectDescriptor[f_chain->EffectCount];

	for (uint32_t i = 0; i < f_chain->EffectCount; ++i)
	{
		f_chain->pEffectDescriptors[i].InitialState = x_chain->pEffectDescriptors[i].InitialState;
		f_chain->pEffectDescriptors[i].OutputChannels =
			x_chain->pEffectDescriptors[i].OutputChannel;
		f_chain->pEffectDescriptors[i].pEffect =
			wrap_xapo_effect(x_chain->pEffectDescriptors[i].pEffect);
	}

	return f_chain;
}

static void free_effect_chain(FAudioEffectChain *f_chain)
{
	if (f_chain != NULL)
	{
		delete[] f_chain->pEffectDescriptors;
		delete f_chain;
	}
}

///////////////////////////////////////////////////////////////////////////////
//
// IXAudio2SourceVoice implementation
//

class XAudio2SourceVoiceImpl : public IXAudio2SourceVoice
{
public:
	XAudio2SourceVoiceImpl(
		FAudio *faudio,
		const WAVEFORMATEX *pSourceFormat,
		UINT32 Flags,
		float MaxFrequencyRatio,
		IXAudio2VoiceCallback *pCallback,
		const XAUDIO2_VOICE_SENDS *pSendList,
		const XAUDIO2_EFFECT_CHAIN *pEffectChain)
	{
		TRACE_MSG("Format=%d; nChannels=%d; nSamplesPerSec=%d",
			pSourceFormat->wFormatTag,
			pSourceFormat->nChannels,
			pSourceFormat->nSamplesPerSec);
		TRACE_MSG("Sends = %d", pSendList ? pSendList->SendCount : -1);
		voice_callback = wrap_voice_callback(pCallback);
		voice_sends = unwrap_voice_sends(pSendList);
		effect_chain = wrap_effect_chain(pEffectChain);
		FAudio_CreateSourceVoice(
			faudio,
			&faudio_voice,
			pSourceFormat,
			Flags,
			MaxFrequencyRatio,
			reinterpret_cast<FAudioVoiceCallback *>(voice_callback),
			voice_sends,
			effect_chain);
	}

	// IXAudio2Voice
	COM_METHOD(void) GetVoiceDetails(XAUDIO2_VOICE_DETAILS *pVoiceDetails)
	{
		TRACE_FUNC();
#if XAUDIO2_VERSION <= 7
		FAudioVoice_GetVoiceDetails(faudio_voice, pVoiceDetails);
#else
		FAudioVoiceDetails fDetails;
		FAudioVoice_GetVoiceDetails(faudio_voice, &fDetails);
		pVoiceDetails->CreationFlags = fDetails.CreationFlags;
		pVoiceDetails->ActiveFlags = fDetails.CreationFlags;
		pVoiceDetails->InputChannels = fDetails.InputChannels;
		pVoiceDetails->InputSampleRate = fDetails.InputSampleRate;
#endif // XAUDIO2_VERSION <= 7
	}

	COM_METHOD(HRESULT) SetOutputVoices(const XAUDIO2_VOICE_SENDS *pSendList)
	{
		TRACE_FUNC();
		free_voice_sends(voice_sends);
		voice_sends = unwrap_voice_sends(pSendList);
		return FAudioVoice_SetOutputVoices(faudio_voice, voice_sends);
	}

	COM_METHOD(HRESULT) SetEffectChain(const XAUDIO2_EFFECT_CHAIN *pEffectChain)
	{
		TRACE_FUNC();
		free_effect_chain(effect_chain);
		effect_chain = wrap_effect_chain(pEffectChain);
		return FAudioVoice_SetEffectChain(faudio_voice, effect_chain);
	}

	COM_METHOD(HRESULT) EnableEffect(UINT32 EffectIndex, UINT32 OperationSet = FAUDIO_COMMIT_NOW)
	{
		TRACE_FUNC();
		return FAudioVoice_EnableEffect(faudio_voice, EffectIndex, OperationSet);
	}

	COM_METHOD(HRESULT) DisableEffect(UINT32 EffectIndex, UINT32 OperationSet = FAUDIO_COMMIT_NOW)
	{
		TRACE_FUNC();
		return FAudioVoice_DisableEffect(faudio_voice, EffectIndex, OperationSet);
	}

	COM_METHOD(void) GetEffectState(UINT32 EffectIndex, BOOL *pEnabled)
	{
		TRACE_FUNC();
		uint8_t result;
		FAudioVoice_GetEffectState(faudio_voice, EffectIndex, &result);
		if (pEnabled)
		{
			*pEnabled = result;
		}
	}

	COM_METHOD(HRESULT) SetEffectParameters(
		UINT32 EffectIndex,
		const void *pParameters,
		UINT32 ParametersByteSize,
		UINT32 OperationSet = FAUDIO_COMMIT_NOW
	) {
		TRACE_FUNC();
		return FAudioVoice_SetEffectParameters(
			faudio_voice, 
			EffectIndex, 
			pParameters, 
			ParametersByteSize, 
			OperationSet);
	}

	COM_METHOD(HRESULT) GetEffectParameters(
		UINT32 EffectIndex, 
		void *pParameters, 
		UINT32 ParametersByteSize)
	{
		TRACE_FUNC();
		return FAudioVoice_GetEffectParameters(
			faudio_voice,
			EffectIndex, 
			pParameters, 
			ParametersByteSize);
	}

	COM_METHOD(HRESULT) SetFilterParameters(
		const XAUDIO2_FILTER_PARAMETERS *pParameters,
		UINT32 OperationSet = FAUDIO_COMMIT_NOW)
	{
		TRACE_FUNC();
		return FAudioVoice_SetFilterParameters(faudio_voice, pParameters, OperationSet);
	}

	COM_METHOD(void) GetFilterParameters(XAUDIO2_FILTER_PARAMETERS *pParameters)
	{
		TRACE_FUNC();
		FAudioVoice_GetFilterParameters(faudio_voice, pParameters);
	}

#if XAUDIO2_VERSION >= 4
	COM_METHOD(HRESULT) SetOutputFilterParameters(
		IXAudio2Voice *pDestinationVoice,
		const XAUDIO2_FILTER_PARAMETERS *pParameters,
		UINT32 OperationSet = FAUDIO_COMMIT_NOW
	) {
		TRACE_FUNC();
		return FAudioVoice_SetOutputFilterParameters(
			faudio_voice,
			((XAudio2SourceVoiceImpl *)pDestinationVoice)->faudio_voice,
			pParameters,
			OperationSet);
	}

	COM_METHOD(void) GetOutputFilterParameters(
		IXAudio2Voice *pDestinationVoice,
		XAUDIO2_FILTER_PARAMETERS *pParameters
	) {
		TRACE_FUNC();
		FAudioVoice_GetOutputFilterParameters(
			faudio_voice, 
			((XAudio2SourceVoiceImpl *)pDestinationVoice)->faudio_voice, 
			pParameters);
	}
#endif // XAUDIO2_VERSION >= 4

	COM_METHOD(HRESULT) SetVolume(float Volume, UINT32 OperationSet = FAUDIO_COMMIT_NOW)
	{
		TRACE_FUNC();
		return FAudioVoice_SetVolume(faudio_voice, Volume, OperationSet);
	}

	COM_METHOD(void) GetVolume(float *pVolume)
	{
		TRACE_FUNC();
		FAudioVoice_GetVolume(faudio_voice, pVolume);
	}

	COM_METHOD(HRESULT) SetChannelVolumes(
		UINT32 Channels,
		const float *pVolumes,
		UINT32 OperationSet = FAUDIO_COMMIT_NOW
	) {
		TRACE_FUNC();
		return FAudioVoice_SetChannelVolumes(faudio_voice, Channels, pVolumes, OperationSet);
	}

	COM_METHOD(void) GetChannelVolumes(UINT32 Channels, float *pVolumes)
	{
		TRACE_FUNC();
		FAudioVoice_GetChannelVolumes(faudio_voice, Channels, pVolumes);
	}

	COM_METHOD(HRESULT) SetOutputMatrix(
		IXAudio2Voice *pDestinationVoice,
		UINT32 SourceChannels,
		UINT32 DestinationChannels,
		const float *pLevelMatrix,
		UINT32 OperationSet = FAUDIO_COMMIT_NOW
	) {
		TRACE_MSG(
			"this = %x; pDestinationVoice = %x; SourceChannels = %d; DestinationChannels = %d",
			this,
			pDestinationVoice,
			SourceChannels,
			DestinationChannels);
		FAudioVoice *dest = (pDestinationVoice) ? pDestinationVoice->faudio_voice : NULL;
		return FAudioVoice_SetOutputMatrix(
			faudio_voice, dest, SourceChannels, DestinationChannels, pLevelMatrix, OperationSet);
	}

#if XAUDIO2_VERSION >= 1
	COM_METHOD(void) GetOutputMatrix(
#else
	COM_METHOD(HRESULT) GetOutputMatrix(
#endif
		IXAudio2Voice *pDestinationVoice,
		UINT32 SourceChannels,
		UINT32 DestinationChannels,
		float *pLevelMatrix
	) {
		TRACE_FUNC();
		FAudioVoice_GetOutputMatrix(
			faudio_voice,
			pDestinationVoice->faudio_voice,
			SourceChannels,
			DestinationChannels,
			pLevelMatrix);
#if XAUDIO2_VERSION < 1
		return S_OK;
#endif // XAUDIO2_VERSION < 1
	}

	COM_METHOD(void) DestroyVoice()
	{
		TRACE_FUNC();
		FAudioVoice_DestroyVoice(faudio_voice);
		// FIXME: in theory FAudioVoice_DestroyVoice can fail but how would we ever now ? -JS
		if (voice_callback)
		{
			delete voice_callback;
		}
		free_voice_sends(voice_sends);
		free_effect_chain(effect_chain);
		delete this;
	}

	// IXAudio2SourceVoice
	COM_METHOD(HRESULT) Start(UINT32 Flags = 0, UINT32 OperationSet = FAUDIO_COMMIT_NOW)
	{
		TRACE_FUNC();
		return FAudioSourceVoice_Start(faudio_voice, Flags, OperationSet);
	}

	COM_METHOD(HRESULT) Stop(UINT32 Flags = 0, UINT32 OperationSet = FAUDIO_COMMIT_NOW)
	{
		TRACE_FUNC();
		return FAudioSourceVoice_Stop(faudio_voice, Flags, OperationSet);
	}

	COM_METHOD(HRESULT) SubmitSourceBuffer(
		const XAUDIO2_BUFFER *pBuffer, 
		const XAUDIO2_BUFFER_WMA *pBufferWMA = NULL)
	{
		TRACE_FUNC();
		return FAudioSourceVoice_SubmitSourceBuffer(faudio_voice, pBuffer, pBufferWMA);
	}

	COM_METHOD(HRESULT) FlushSourceBuffers()
	{
		TRACE_FUNC();
		return FAudioSourceVoice_FlushSourceBuffers(faudio_voice);
	}

	COM_METHOD(HRESULT) Discontinuity()
	{
		TRACE_FUNC();
		return FAudioSourceVoice_Discontinuity(faudio_voice);
	}

	COM_METHOD(HRESULT) ExitLoop(UINT32 OperationSet = FAUDIO_COMMIT_NOW)
	{
		TRACE_FUNC();
		return FAudioSourceVoice_ExitLoop(faudio_voice, OperationSet);
	}

#if (XAUDIO2_VERSION <= 7)
	COM_METHOD(void) GetState(XAUDIO2_VOICE_STATE *pVoiceState)
#else
	COM_METHOD(void) GetState(XAUDIO2_VOICE_STATE *pVoiceState, UINT32 Flags = 0)
#endif
	{
		TRACE_FUNC();
		FAudioSourceVoice_GetState(faudio_voice, pVoiceState);
	}

	COM_METHOD(HRESULT) SetFrequencyRatio(float Ratio, UINT32 OperationSet = FAUDIO_COMMIT_NOW)
	{
		TRACE_FUNC();
		return FAudioSourceVoice_SetFrequencyRatio(faudio_voice, Ratio, OperationSet);
	}

	COM_METHOD(void) GetFrequencyRatio(float *pRatio)
	{
		TRACE_FUNC();
		FAudioSourceVoice_GetFrequencyRatio(faudio_voice, pRatio);
	}

#if XAUDIO2_VERSION >= 4
	COM_METHOD(HRESULT) SetSourceSampleRate(UINT32 NewSourceSampleRate)
	{
		TRACE_FUNC();
		return FAudioSourceVoice_SetSourceSampleRate(faudio_voice, NewSourceSampleRate);
	}
#endif // XAUDIO2_VERSION >= 4

private:
	FAudioVoiceCppCallback *voice_callback;
	FAudioVoiceSends *voice_sends;
	FAudioEffectChain *effect_chain;
};

///////////////////////////////////////////////////////////////////////////////
//
// IXAudio2SubmixVoice implementation
//

class XAudio2SubmixVoiceImpl : public IXAudio2SubmixVoice
{
public:
	XAudio2SubmixVoiceImpl(
		FAudio *faudio,
		UINT32 InputChannels,
		UINT32 InputSampleRate,
		UINT32 Flags,
		UINT32 ProcessingStage,
		const XAUDIO2_VOICE_SENDS *pSendList,
		const XAUDIO2_EFFECT_CHAIN *pEffectChain
	) {
		TRACE_MSG( "InputChannels = %d; InputSampleRate = %d; Flags = %d; ProcessingState = %d; "
			"EffectChain = %x",
			InputChannels,
			InputSampleRate,
			Flags,
			ProcessingStage,
			pEffectChain);
		voice_sends = unwrap_voice_sends(pSendList);
		effect_chain = wrap_effect_chain(pEffectChain);
		FAudio_CreateSubmixVoice(
			faudio,
			&faudio_voice,
			InputChannels,
			InputSampleRate,
			Flags,
			ProcessingStage,
			voice_sends,
			effect_chain);
	}

	// IXAudio2Voice
	COM_METHOD(void) GetVoiceDetails(XAUDIO2_VOICE_DETAILS *pVoiceDetails)
	{
		TRACE_FUNC();
#if XAUDIO2_VERSION <= 7
		FAudioVoice_GetVoiceDetails(faudio_voice, pVoiceDetails);
#else
		FAudioVoiceDetails fDetails;
		FAudioVoice_GetVoiceDetails(faudio_voice, &fDetails);
		pVoiceDetails->CreationFlags = fDetails.CreationFlags;
		pVoiceDetails->ActiveFlags = fDetails.CreationFlags;
		pVoiceDetails->InputChannels = fDetails.InputChannels;
		pVoiceDetails->InputSampleRate = fDetails.InputSampleRate;
#endif
	}

	COM_METHOD(HRESULT) SetOutputVoices(const XAUDIO2_VOICE_SENDS *pSendList)
	{
		TRACE_FUNC();
		free_voice_sends(voice_sends);
		voice_sends = unwrap_voice_sends(pSendList);
		return FAudioVoice_SetOutputVoices(faudio_voice, voice_sends);
	}

	COM_METHOD(HRESULT) SetEffectChain(const XAUDIO2_EFFECT_CHAIN *pEffectChain)
	{
		TRACE_FUNC();
		free_effect_chain(effect_chain);
		effect_chain = wrap_effect_chain(pEffectChain);
		return FAudioVoice_SetEffectChain(faudio_voice, effect_chain);
	}

	COM_METHOD(HRESULT) EnableEffect(UINT32 EffectIndex, UINT32 OperationSet = FAUDIO_COMMIT_NOW)
	{
		TRACE_FUNC();
		return FAudioVoice_EnableEffect(faudio_voice, EffectIndex, OperationSet);
	}

	COM_METHOD(HRESULT) DisableEffect(UINT32 EffectIndex, UINT32 OperationSet = FAUDIO_COMMIT_NOW)
	{
		TRACE_FUNC();
		return FAudioVoice_DisableEffect(faudio_voice, EffectIndex, OperationSet);
	}

	COM_METHOD(void) GetEffectState(UINT32 EffectIndex, BOOL *pEnabled)
	{
		TRACE_FUNC();
		uint8_t result;
		FAudioVoice_GetEffectState(faudio_voice, EffectIndex, &result);
		if (pEnabled)
		{
			*pEnabled = result;
		}
	}

	COM_METHOD(HRESULT) SetEffectParameters(
		UINT32 EffectIndex,
		const void *pParameters,
		UINT32 ParametersByteSize,
		UINT32 OperationSet = FAUDIO_COMMIT_NOW
	) {
		TRACE_FUNC();
		return FAudioVoice_SetEffectParameters(
			faudio_voice, 
			EffectIndex, 
			pParameters, 
			ParametersByteSize, 
			OperationSet);
	}

	COM_METHOD(HRESULT) GetEffectParameters(
		UINT32 EffectIndex, 
		void *pParameters, 
		UINT32 ParametersByteSize
	) {
		TRACE_FUNC();
		return FAudioVoice_GetEffectParameters(
			faudio_voice, 
			EffectIndex, 
			pParameters, 
			ParametersByteSize);
	}

	COM_METHOD(HRESULT) SetFilterParameters(
		const XAUDIO2_FILTER_PARAMETERS *pParameters,
		UINT32 OperationSet = FAUDIO_COMMIT_NOW
	) {
		TRACE_FUNC();
		return FAudioVoice_SetFilterParameters(faudio_voice, pParameters, OperationSet);
	}

	COM_METHOD(void) GetFilterParameters(XAUDIO2_FILTER_PARAMETERS *pParameters)
	{
		TRACE_FUNC();
		FAudioVoice_GetFilterParameters(faudio_voice, pParameters);
	}

#if XAUDIO2_VERSION >= 4
	COM_METHOD(HRESULT) SetOutputFilterParameters(
		IXAudio2Voice *pDestinationVoice,
		const XAUDIO2_FILTER_PARAMETERS *pParameters,
		UINT32 OperationSet = FAUDIO_COMMIT_NOW
	) {
		TRACE_FUNC();
		return FAudioVoice_SetOutputFilterParameters(
			faudio_voice,
			((XAudio2SubmixVoiceImpl *)pDestinationVoice)->faudio_voice,
			pParameters,
			OperationSet);
	}

	COM_METHOD(void) GetOutputFilterParameters(
		IXAudio2Voice *pDestinationVoice,
		XAUDIO2_FILTER_PARAMETERS *pParameters
	) {
		TRACE_FUNC();
		FAudioVoice_GetOutputFilterParameters(
			faudio_voice, ((XAudio2SubmixVoiceImpl *)pDestinationVoice)->faudio_voice, pParameters);
	}
#endif // XAUDIO2_VERSION >= 4

	COM_METHOD(HRESULT) SetVolume(float Volume, UINT32 OperationSet = FAUDIO_COMMIT_NOW)
	{
		TRACE_FUNC();
		return FAudioVoice_SetVolume(faudio_voice, Volume, OperationSet);
	}

	COM_METHOD(void) GetVolume(float *pVolume)
	{
		TRACE_FUNC();
		FAudioVoice_GetVolume(faudio_voice, pVolume);
	}

	COM_METHOD(HRESULT) SetChannelVolumes(
		UINT32 Channels,
		const float *pVolumes,
		UINT32 OperationSet = FAUDIO_COMMIT_NOW
	) {
		TRACE_FUNC();
		return FAudioVoice_SetChannelVolumes(faudio_voice, Channels, pVolumes, OperationSet);
	}

	COM_METHOD(void) GetChannelVolumes(UINT32 Channels, float *pVolumes)
	{
		TRACE_FUNC();
		FAudioVoice_GetChannelVolumes(faudio_voice, Channels, pVolumes);
	}

	COM_METHOD(HRESULT) SetOutputMatrix(
		IXAudio2Voice *pDestinationVoice,
		UINT32 SourceChannels,
		UINT32 DestinationChannels,
		const float *pLevelMatrix,
		UINT32 OperationSet = FAUDIO_COMMIT_NOW
	) {
		TRACE_FUNC();
		FAudioVoice *dest = (pDestinationVoice) ? pDestinationVoice->faudio_voice : NULL;
		return FAudioVoice_SetOutputMatrix(
			faudio_voice, 
			dest, 
			SourceChannels, 
			DestinationChannels, 
			pLevelMatrix, 
			OperationSet);
	}

#if XAUDIO2_VERSION >= 1
	COM_METHOD(void) GetOutputMatrix(
#else
	COM_METHOD(HRESULT) GetOutputMatrix(
#endif
		IXAudio2Voice *pDestinationVoice,
		UINT32 SourceChannels,
		UINT32 DestinationChannels,
		float *pLevelMatrix
	) {
		TRACE_FUNC();
		FAudioVoice_GetOutputMatrix(
			faudio_voice,
			pDestinationVoice->faudio_voice,
			SourceChannels,
			DestinationChannels,
			pLevelMatrix);
#if XAUDIO2_VERSION < 1
		return S_OK;
#endif // XAUDIO2_VERSION < 1
	}

	COM_METHOD(void) DestroyVoice()
	{
		TRACE_FUNC();
		FAudioVoice_DestroyVoice(faudio_voice);
		// FIXME: in theory FAudioVoice_DestroyVoice can fail but how would we ever now ? -JS
		free_voice_sends(voice_sends);
		free_effect_chain(effect_chain);
		delete this;
	}

private:
	FAudioVoiceSends *voice_sends;
	FAudioEffectChain *effect_chain;
};

///////////////////////////////////////////////////////////////////////////////
//
// IXAudio2MasteringVoice implementation
//

#if (XAUDIO2_VERSION >= 8) && !defined(__WINE__)
uint32_t device_index_from_device_id(FAudio *faudio, LPCWSTR deviceId);
#endif // (XAUDIO2_VERSION >= 8) && !defined(__WINE__)

class XAudio2MasteringVoiceImpl : public IXAudio2MasteringVoice
{
public:
#if (XAUDIO2_VERSION <= 7)
	XAudio2MasteringVoiceImpl(
		FAudio *faudio,
		UINT32 InputChannels,
		UINT32 InputSampleRate,
		UINT32 Flags,
		UINT32 DeviceIndex,
		const XAUDIO2_EFFECT_CHAIN *pEffectChain
	) {
		TRACE_MSG(
			"InputChannels = %d; InputSampleRate = %d; Flags = %d; EffectChain = %x",
			InputChannels,
			InputSampleRate,
			Flags,
			pEffectChain);
		voice_sends = NULL;
		effect_chain = wrap_effect_chain(pEffectChain);
		FAudio_CreateMasteringVoice(
			faudio,
			&faudio_voice,
			InputChannels,
			InputSampleRate,
			Flags,
			DeviceIndex,
			effect_chain);
	}
#else
	XAudio2MasteringVoiceImpl(
		FAudio *faudio,
		UINT32 InputChannels,
		UINT32 InputSampleRate,
		UINT32 Flags,
		LPCWSTR szDeviceId,
		const XAUDIO2_EFFECT_CHAIN *pEffectChain,
		int StreamCategory
	) {
		TRACE_FUNC();

		uint32_t device_index = 0;

#ifndef __WINE__
		if (szDeviceId != NULL)
		{
			device_index = device_index_from_device_id(faudio, szDeviceId);
		}
#endif // !__WINE__

		effect_chain = wrap_effect_chain(pEffectChain);
		FAudio_CreateMasteringVoice(
			faudio,
			&faudio_voice,
			InputChannels,
			InputSampleRate,
			Flags,
			device_index,
			effect_chain);
	}
#endif

	// IXAudio2Voice
	COM_METHOD(void) GetVoiceDetails(XAUDIO2_VOICE_DETAILS *pVoiceDetails)
	{
		TRACE_FUNC();
#if XAUDIO2_VERSION <= 7
		FAudioVoice_GetVoiceDetails(faudio_voice, pVoiceDetails);
#else
		FAudioVoiceDetails fDetails;
		FAudioVoice_GetVoiceDetails(faudio_voice, &fDetails);
		pVoiceDetails->CreationFlags = fDetails.CreationFlags;
		pVoiceDetails->ActiveFlags = fDetails.CreationFlags;
		pVoiceDetails->InputChannels = fDetails.InputChannels;
		pVoiceDetails->InputSampleRate = fDetails.InputSampleRate;
#endif
	}

	COM_METHOD(HRESULT) SetOutputVoices(const XAUDIO2_VOICE_SENDS *pSendList)
	{
		TRACE_FUNC();
		free_voice_sends(voice_sends);
		voice_sends = unwrap_voice_sends(pSendList);
		return FAudioVoice_SetOutputVoices(faudio_voice, voice_sends);
	}

	COM_METHOD(HRESULT) SetEffectChain(const XAUDIO2_EFFECT_CHAIN *pEffectChain)
	{
		TRACE_FUNC();
		free_effect_chain(effect_chain);
		effect_chain = wrap_effect_chain(pEffectChain);
		return FAudioVoice_SetEffectChain(faudio_voice, effect_chain);
	}

	COM_METHOD(HRESULT) EnableEffect(UINT32 EffectIndex, UINT32 OperationSet = FAUDIO_COMMIT_NOW)
	{
		TRACE_FUNC();
		return FAudioVoice_EnableEffect(faudio_voice, EffectIndex, OperationSet);
	}

	COM_METHOD(HRESULT) DisableEffect(UINT32 EffectIndex, UINT32 OperationSet = FAUDIO_COMMIT_NOW)
	{
		TRACE_FUNC();
		return FAudioVoice_DisableEffect(faudio_voice, EffectIndex, OperationSet);
	}

	COM_METHOD(void) GetEffectState(UINT32 EffectIndex, BOOL *pEnabled)
	{
		TRACE_FUNC();
		uint8_t result;
		FAudioVoice_GetEffectState(faudio_voice, EffectIndex, &result);
		if (pEnabled)
		{
			*pEnabled = result;
		}
	}

	COM_METHOD(HRESULT) SetEffectParameters(
		UINT32 EffectIndex,
		const void *pParameters,
		UINT32 ParametersByteSize,
		UINT32 OperationSet = FAUDIO_COMMIT_NOW
	) {
		TRACE_FUNC();
		return FAudioVoice_SetEffectParameters(
			faudio_voice, 
			EffectIndex, 
			pParameters, 
			ParametersByteSize, 
			OperationSet);
	}

	COM_METHOD(HRESULT) GetEffectParameters(
		UINT32 EffectIndex, 
		void *pParameters, 
		UINT32 ParametersByteSize
	) {
		TRACE_FUNC();
		return FAudioVoice_GetEffectParameters(
			faudio_voice, 
			EffectIndex, 
			pParameters, 
			ParametersByteSize);
	}

	COM_METHOD(HRESULT) SetFilterParameters(
		const XAUDIO2_FILTER_PARAMETERS *pParameters,
		UINT32 OperationSet = FAUDIO_COMMIT_NOW
	) {
		TRACE_FUNC();
		return FAudioVoice_SetFilterParameters(faudio_voice, pParameters, OperationSet);
	}

	COM_METHOD(void) GetFilterParameters(XAUDIO2_FILTER_PARAMETERS *pParameters)
	{
		TRACE_FUNC();
		FAudioVoice_GetFilterParameters(faudio_voice, pParameters);
	}

#if XAUDIO2_VERSION >= 4
	COM_METHOD(HRESULT) SetOutputFilterParameters(
		IXAudio2Voice *pDestinationVoice,
		const XAUDIO2_FILTER_PARAMETERS *pParameters,
		UINT32 OperationSet = FAUDIO_COMMIT_NOW
	) {
		TRACE_FUNC();
		return FAudioVoice_SetOutputFilterParameters(
			faudio_voice,
			((XAudio2MasteringVoiceImpl *)pDestinationVoice)->faudio_voice,
			pParameters,
			OperationSet);
	}

	COM_METHOD(void) GetOutputFilterParameters(
		IXAudio2Voice *pDestinationVoice,
		XAUDIO2_FILTER_PARAMETERS *pParameters
	) {
		TRACE_FUNC();
		FAudioVoice_GetOutputFilterParameters(
			faudio_voice,
			((XAudio2MasteringVoiceImpl *)pDestinationVoice)->faudio_voice,
			pParameters);
	}
#endif // XAUDIO2_VERSION >= 4

	COM_METHOD(HRESULT) SetVolume(float Volume, UINT32 OperationSet = FAUDIO_COMMIT_NOW)
	{
		TRACE_FUNC();
		return FAudioVoice_SetVolume(faudio_voice, Volume, OperationSet);
	}

	COM_METHOD(void) GetVolume(float *pVolume)
	{
		TRACE_FUNC();
		FAudioVoice_GetVolume(faudio_voice, pVolume);
	}

	COM_METHOD(HRESULT) SetChannelVolumes(
		UINT32 Channels,
		const float *pVolumes,
		UINT32 OperationSet = FAUDIO_COMMIT_NOW
	) {
		TRACE_FUNC();
		return FAudioVoice_SetChannelVolumes(faudio_voice, Channels, pVolumes, OperationSet);
	}

	COM_METHOD(void) GetChannelVolumes(UINT32 Channels, float *pVolumes)
	{
		TRACE_FUNC();
		FAudioVoice_GetChannelVolumes(faudio_voice, Channels, pVolumes);
	}

	COM_METHOD(HRESULT) SetOutputMatrix(
		IXAudio2Voice *pDestinationVoice,
		UINT32 SourceChannels,
		UINT32 DestinationChannels,
		const float *pLevelMatrix,
		UINT32 OperationSet = FAUDIO_COMMIT_NOW
	) {
		TRACE_FUNC();
		FAudioVoice *dest = (pDestinationVoice) ? pDestinationVoice->faudio_voice : NULL;
		return FAudioVoice_SetOutputMatrix(
			faudio_voice,
			dest, 
			SourceChannels, 
			DestinationChannels, 
			pLevelMatrix, 
			OperationSet);
	}

#if XAUDIO2_VERSION >= 1
	COM_METHOD(void) GetOutputMatrix(
#else
	COM_METHOD(HRESULT) GetOutputMatrix(
#endif
		IXAudio2Voice *pDestinationVoice,
		UINT32 SourceChannels,
		UINT32 DestinationChannels,
		float *pLevelMatrix
	) {
		TRACE_FUNC();
		FAudioVoice_GetOutputMatrix(
			faudio_voice,
			pDestinationVoice->faudio_voice,
			SourceChannels,
			DestinationChannels,
			pLevelMatrix);
#if XAUDIO2_VERSION < 1
		return S_OK;
#endif // XAUDIO2_VERSION < 1
	}

	COM_METHOD(void) DestroyVoice()
	{
		TRACE_FUNC();
		FAudioVoice_DestroyVoice(faudio_voice);
		// FIXME: in theory FAudioVoice_DestroyVoice can fail but how would we ever now ? -JS
		free_voice_sends(voice_sends);
		free_effect_chain(effect_chain);
		delete this;
	}

	// IXAudio2MasteringVoice
#if XAUDIO2_VERSION >= 8
	COM_METHOD(HRESULT) GetChannelMask(uint32_t *pChannelmask)
	{
		return FAudioMasteringVoice_GetChannelMask(faudio_voice, pChannelmask);
	}
#endif

private:
	FAudioVoiceSends *voice_sends;
	FAudioEffectChain *effect_chain;
};

///////////////////////////////////////////////////////////////////////////////
//
// IXAudio2 implementation
//

class XAudio2Impl : public IXAudio2
{
public:
	XAudio2Impl()
	{
		callback_list.com = NULL;
		callback_list.next = NULL;
		FAudio_Construct(&faudio);
	}

	XAudio2Impl(UINT32 Flags, XAUDIO2_PROCESSOR XAudio2Processor)
	{
		callback_list.com = NULL;
		callback_list.next = NULL;
		FAudioCreate(&faudio, Flags, XAudio2Processor);
	}

	COM_METHOD(HRESULT) QueryInterface(REFIID riid, void **ppvInterface)
	{
		TRACE_FUNC();

		if (guid_equals(riid, IID_IXAudio2))
		{
			*ppvInterface = static_cast<IXAudio2 *>(this);
		}
		else if (guid_equals(riid, IID_IUnknown))
		{
			*ppvInterface = static_cast<IUnknown *>(this);
		}
		else
		{
			*ppvInterface = NULL;
			return E_NOINTERFACE;
		}

		reinterpret_cast<IUnknown *>(*ppvInterface)->AddRef();

		return S_OK;
	}

	COM_METHOD(ULONG) AddRef()
	{
		TRACE_FUNC();
		return FAudio_AddRef(faudio);
	}

	COM_METHOD(ULONG) Release()
	{
		TRACE_FUNC();
		ULONG refcount = FAudio_Release(faudio);
		if (refcount == 0)
		{
			delete this;
		}
		return 1;
	}

#if (XAUDIO2_VERSION <= 7)
	COM_METHOD(HRESULT) GetDeviceCount(UINT32 *pCount)
	{
		TRACE_FUNC();
		return FAudio_GetDeviceCount(faudio, pCount);
	}

	COM_METHOD(HRESULT) GetDeviceDetails(UINT32 Index, XAUDIO2_DEVICE_DETAILS *pDeviceDetails)
	{
		TRACE_FUNC();
		return FAudio_GetDeviceDetails(faudio, Index, pDeviceDetails);
	}

	COM_METHOD(HRESULT) Initialize(
		UINT32 Flags = 0, 
		XAUDIO2_PROCESSOR XAudio2Processor = FAUDIO_DEFAULT_PROCESSOR
	) {
		TRACE_FUNC();
		return FAudio_Initialize(faudio, Flags, XAudio2Processor);
	}
#endif // XAUDIO2_VERSION <= 7

	COM_METHOD(HRESULT) RegisterForCallbacks(IXAudio2EngineCallback *pCallback)
	{
		TRACE_FUNC();
		FAudioCppEngineCallback *cb = wrap_engine_callback(pCallback);
		cb->next = callback_list.next;
		callback_list.next = cb;

		return FAudio_RegisterForCallbacks(faudio, reinterpret_cast<FAudioEngineCallback *>(cb));
	}

	COM_METHOD(void) UnregisterForCallbacks(IXAudio2EngineCallback *pCallback)
	{
		TRACE_FUNC();
		FAudioCppEngineCallback *cb = find_and_remove_engine_callback(&callback_list, pCallback);

		if (cb == NULL)
		{
			return;
		}

		FAudio_UnregisterForCallbacks(faudio, reinterpret_cast<FAudioEngineCallback *>(cb));
		delete cb;
	}

	COM_METHOD(HRESULT) CreateSourceVoice(
		IXAudio2SourceVoice **ppSourceVoice,
		const WAVEFORMATEX *pSourceFormat,
		UINT32 Flags = 0,
		float MaxFrequencyRatio = FAUDIO_DEFAULT_FREQ_RATIO,
		IXAudio2VoiceCallback *pCallback = NULL,
		const XAUDIO2_VOICE_SENDS *pSendList = NULL,
		const XAUDIO2_EFFECT_CHAIN *pEffectChain = NULL
	) {
		TRACE_FUNC();
		*ppSourceVoice = new XAudio2SourceVoiceImpl(
			faudio, pSourceFormat, Flags, MaxFrequencyRatio, pCallback, pSendList, pEffectChain);
		return S_OK;
	}

	COM_METHOD(HRESULT) CreateSubmixVoice(
		IXAudio2SubmixVoice **ppSubmixVoice,
		UINT32 InputChannels,
		UINT32 InputSampleRate,
		UINT32 Flags = 0,
		UINT32 ProcessingStage = 0,
		const XAUDIO2_VOICE_SENDS *pSendList = NULL,
		const XAUDIO2_EFFECT_CHAIN *pEffectChain = NULL
	) {
		TRACE_FUNC();
		*ppSubmixVoice = new XAudio2SubmixVoiceImpl(
			faudio,
			InputChannels,
			InputSampleRate,
			Flags,
			ProcessingStage,
			pSendList,
			pEffectChain);
		return S_OK;
	}

#if (XAUDIO2_VERSION <= 7)
	COM_METHOD(HRESULT) CreateMasteringVoice(
		IXAudio2MasteringVoice **ppMasteringVoice,
		UINT32 InputChannels = FAUDIO_DEFAULT_CHANNELS,
		UINT32 InputSampleRate = FAUDIO_DEFAULT_SAMPLERATE,
		UINT32 Flags = 0,
		UINT32 DeviceIndex = 0,
		const XAUDIO2_EFFECT_CHAIN *pEffectChain = NULL
	) {
		TRACE_MSG("InputChannels = %d InputSampleRate = %d", InputChannels, InputSampleRate);
		*ppMasteringVoice = new XAudio2MasteringVoiceImpl(
			faudio, InputChannels, InputSampleRate, Flags, DeviceIndex, pEffectChain);
		return S_OK;
	}
#else
	COM_METHOD(HRESULT) CreateMasteringVoice(
		IXAudio2MasteringVoice **ppMasteringVoice,
		UINT32 InputChannels = FAUDIO_DEFAULT_CHANNELS,
		UINT32 InputSampleRate = FAUDIO_DEFAULT_SAMPLERATE,
		UINT32 Flags = 0,
		LPCWSTR szDeviceId = NULL,
		const XAUDIO2_EFFECT_CHAIN *pEffectChain = NULL,
		int StreamCategory = 6
	) {
		*ppMasteringVoice = new XAudio2MasteringVoiceImpl(
			faudio,
			InputChannels,
			InputSampleRate,
			Flags,
			szDeviceId,
			pEffectChain,
			StreamCategory);
		return S_OK;
	}
#endif

	COM_METHOD(HRESULT) StartEngine()
	{
		TRACE_FUNC();
		return FAudio_StartEngine(faudio);
	}

	COM_METHOD(void) StopEngine()
	{
		TRACE_FUNC();
		FAudio_StopEngine(faudio);
	}

	COM_METHOD(HRESULT) CommitChanges(UINT32 OperationSet)
	{
		TRACE_FUNC();
		return FAudio_CommitChanges(faudio);
	}

	COM_METHOD(void) GetPerformanceData(XAUDIO2_PERFORMANCE_DATA *pPerfData)
	{
		TRACE_FUNC();
#if XAUDIO2_VERSION >= 3
		FAudio_GetPerformanceData(faudio, pPerfData);
#else
		FAudioPerformanceData fPerfData;
		FAudio_GetPerformanceData(faudio, &fPerfData);

		pPerfData->AudioCyclesSinceLastQuery = fPerfData.AudioCyclesSinceLastQuery;
		pPerfData->TotalCyclesSinceLastQuery = fPerfData.TotalCyclesSinceLastQuery;
		pPerfData->MinimumCyclesPerQuantum = fPerfData.MinimumCyclesPerQuantum;
		pPerfData->MaximumCyclesPerQuantum = fPerfData.MaximumCyclesPerQuantum;
		pPerfData->MemoryUsageInBytes = fPerfData.MemoryUsageInBytes;
		pPerfData->CurrentLatencyInSamples = fPerfData.CurrentLatencyInSamples;
		pPerfData->GlitchesSinceEngineStarted = fPerfData.GlitchesSinceEngineStarted;
		pPerfData->ActiveSourceVoiceCount = fPerfData.ActiveSourceVoiceCount;
		pPerfData->TotalSourceVoiceCount = fPerfData.TotalSourceVoiceCount;
		pPerfData->ActiveSubmixVoiceCount = fPerfData.ActiveSubmixVoiceCount;
		pPerfData->TotalSubmixVoiceCount = fPerfData.ActiveSubmixVoiceCount;
		pPerfData->ActiveXmaSourceVoices = fPerfData.ActiveXmaSourceVoices;
		pPerfData->ActiveXmaStreams = fPerfData.ActiveXmaStreams;
#endif // XAUDIO2_VERSION >= 3
	}

	COM_METHOD(void)
	SetDebugConfiguration(
		XAUDIO2_DEBUG_CONFIGURATION *pDebugConfiguration,
		void *pReserved = NULL
	) {
		TRACE_FUNC();
		FAudio_SetDebugConfiguration(
			faudio, 
			pDebugConfiguration, 
			pReserved);
	}

private:
	FAudio *faudio;
	FAudioCppEngineCallback callback_list;
};

///////////////////////////////////////////////////////////////////////////////
//
// Create function
//

void *CreateXAudio2Internal() 
{ 
	return new XAudio2Impl(); 
}

#if XAUDIO2_VERSION >= 8

FAUDIOCPP_API XAudio2Create(IXAudio2 **ppXAudio2, UINT32 Flags, XAUDIO2_PROCESSOR XAudio2Processor)
{
	// FAudio only accepts one processor
	*ppXAudio2 = new XAudio2Impl(Flags, FAUDIO_DEFAULT_PROCESSOR);
	return S_OK;
}

#endif // XAUDIO2_VERSION >= 8
