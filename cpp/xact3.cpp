#include "xact3.h"

//#define TRACING_ENABLE
#include "trace.h"

#include "SDL.h"

/* IXACT3Cue Implementation */

class XACT3CueImpl : public IXACT3Cue
{
public:
	XACT3CueImpl(FACTCue *newCue)
	{
		cue = newCue;
	}
	COM_METHOD(HRESULT) Play()
	{
		TRACE_FUNC();
		return FACTCue_Play(cue);
	}
	COM_METHOD(HRESULT) Stop(uint32_t dwFlags)
	{
		TRACE_FUNC();
		return FACTCue_Stop(cue, dwFlags);
	};
	COM_METHOD(HRESULT) GetState(uint32_t *pdwState)
	{
		TRACE_FUNC();
		return FACTCue_GetState(cue, pdwState);
	}
	COM_METHOD(HRESULT) Destroy()
	{
		TRACE_FUNC();
		return FACTCue_Destroy(cue);
	}
	COM_METHOD(HRESULT) SetMatrixCoefficients(
		uint32_t uSrcChannelCount,
		uint32_t uDstChannelCount,
		float *pMatrixCoefficients
	) {
		TRACE_FUNC();
		return FACTCue_SetMatrixCoefficients(
			cue,
			uSrcChannelCount,
			uDstChannelCount,
			pMatrixCoefficients
		);
	}
	COM_METHOD(uint16_t) GetVariableIndex(const char *szFriendlyName)
	{
		TRACE_FUNC();
		return FACTCue_GetVariableIndex(cue, szFriendlyName);
	}
	COM_METHOD(HRESULT) SetVariable(uint16_t nIndex, float nValue)
	{
		TRACE_FUNC();
		return FACTCue_SetVariable(cue, nIndex, nValue);
	}
	COM_METHOD(HRESULT) GetVariable(uint16_t nIndex, float *nValue)
	{
		TRACE_FUNC();
		return FACTCue_GetVariable(cue, nIndex, nValue);
	}
	COM_METHOD(HRESULT) Pause(int32_t fPause)
	{
		TRACE_FUNC();
		return FACTCue_Pause(cue, fPause);
	}
	COM_METHOD(HRESULT) GetProperties(
		XACT_CUE_INSTANCE_PROPERTIES *ppProperties
	) {
		TRACE_FUNC();
		return FACTCue_GetProperties(cue, ppProperties);
	}
#if XACT3_VERSION >= 5
	COM_METHOD(HRESULT) SetOutputVoices(
		const XAUDIO2_VOICE_SENDS *pSendList /* Optional! */
	) {
		TRACE_FUNC();
		/* TODO: SetOutputVoices */
		return 0;
	}
	COM_METHOD(HRESULT) SetOutputVoiceMatrix(
		const IXAudio2Voice *pDestinationVoice, /* Optional! */
		uint32_t SourceChannels,
		uint32_t DestinationChannels,
		const float *pLevelMatrix /* SourceChannels * DestinationChannels */
	) {
		TRACE_FUNC();
		/* TODO: SetOutputVoiceMatrix */
		return 0;
	}
#endif /* #if XACT3_VERSION >= 5 */

	FACTCue *cue;
};

/* IXACT3Wave Implementation */

class XACT3WaveImpl : public IXACT3Wave
{
public:
	XACT3WaveImpl(FACTWave *newWave)
	{
		wave = newWave;
	}
	COM_METHOD(HRESULT) Destroy()
	{
		TRACE_FUNC();
		return FACTWave_Destroy(wave);
	}
	COM_METHOD(HRESULT) Play()
	{
		TRACE_FUNC();
		return FACTWave_Play(wave);
	}
	COM_METHOD(HRESULT) Stop(uint32_t dwFlags)
	{
		TRACE_FUNC();
		return FACTWave_Stop(wave, dwFlags);
	}
	COM_METHOD(HRESULT) Pause(int32_t fPause)
	{
		TRACE_FUNC();
		return FACTWave_Pause(wave, fPause);
	}
	COM_METHOD(HRESULT) GetState(uint32_t *pdwState)
	{
		TRACE_FUNC();
		return FACTWave_GetState(wave, pdwState);
	}
	COM_METHOD(HRESULT) SetPitch(int16_t pitch)
	{
		TRACE_FUNC();
		return FACTWave_SetPitch(wave, pitch);
	}
	COM_METHOD(HRESULT) SetVolume(float volume)
	{
		TRACE_FUNC();
		return FACTWave_SetVolume(wave, volume);
	}
	COM_METHOD(HRESULT) SetMatrixCoefficients(
		uint32_t uSrcChannelCount,
		uint32_t uDstChannelCount,
		float *pMatrixCoefficients
	) {
		TRACE_FUNC();
		return FACTWave_SetMatrixCoefficients(
			wave,
			uSrcChannelCount,
			uDstChannelCount,
			pMatrixCoefficients
		);
	}
	COM_METHOD(HRESULT) GetProperties(
		XACT_WAVE_INSTANCE_PROPERTIES *pProperties
	) {
		TRACE_FUNC();
		return FACTWave_GetProperties(wave, pProperties);
	}

	FACTWave *wave;
};

/* IXACT3SoundBank Implementation */

class XACT3SoundBankImpl : public IXACT3SoundBank
{
public:
	XACT3SoundBankImpl(FACTSoundBank *newSoundBank)
	{
		soundBank = newSoundBank;
	}
	COM_METHOD(uint16_t) GetCueIndex(const char *szFriendlyName)
	{
		TRACE_FUNC();
		return FACTSoundBank_GetCueIndex(soundBank, szFriendlyName);
	}
	COM_METHOD(HRESULT) GetNumCues(uint16_t *pnNumCues)
	{
		TRACE_FUNC();
		return FACTSoundBank_GetNumCues(soundBank, pnNumCues);
	}
	COM_METHOD(HRESULT) GetCueProperties(
		uint16_t nCueIndex,
		XACT_CUE_PROPERTIES *pProperties
	) {
		TRACE_FUNC();
		return FACTSoundBank_GetCueProperties(
			soundBank,
			nCueIndex,
			pProperties
		);
	}
	COM_METHOD(HRESULT) Prepare(
		uint16_t nCueIndex,
		uint32_t dwFlags,
		int32_t timeOffset,
		IXACT3Cue** ppCue
	) {
		TRACE_FUNC();
		FACTCue *cue;
		HRESULT retval = FACTSoundBank_Prepare(
			soundBank,
			nCueIndex,
			dwFlags,
			timeOffset,
			&cue
		);
		*ppCue = new XACT3CueImpl(cue);
		return retval;
	}
	COM_METHOD(HRESULT) Play(
		uint16_t nCueIndex,
		uint32_t dwFlags,
		int32_t timeOffset,
		IXACT3Cue** ppCue /* Optional! */
	) {
		TRACE_FUNC();
		if (ppCue == NULL)
		{
			return FACTSoundBank_Play(
				soundBank,
				nCueIndex,
				dwFlags,
				timeOffset,
				NULL
			);
		}
		FACTCue *cue;
		HRESULT retval = FACTSoundBank_Play(
			soundBank,
			nCueIndex,
			dwFlags,
			timeOffset,
			&cue
		);
		*ppCue = new XACT3CueImpl(cue);
		return retval;
	}
	COM_METHOD(HRESULT) Stop(uint16_t nCueIndex, uint32_t dwFlags)
	{
		return FACTSoundBank_Stop(soundBank, nCueIndex, dwFlags);
	}
	COM_METHOD(HRESULT) Destroy()
	{
		return FACTSoundBank_Destroy(soundBank);
	}
	COM_METHOD(HRESULT) GetState(uint32_t *pdwState)
	{
		return FACTSoundBank_GetState(soundBank, pdwState);
	}

	FACTSoundBank *soundBank;
};

/* IXACT3WaveBank Implementation */

class XACT3WaveBankImpl : public IXACT3WaveBank
{
public:
	XACT3WaveBankImpl(FACTWaveBank *newWaveBank)
	{
		waveBank = newWaveBank;
	}
	COM_METHOD(HRESULT) Destroy()
	{
		TRACE_FUNC();
		return FACTWaveBank_Destroy(waveBank);
	}
	COM_METHOD(HRESULT) GetNumWaves(uint16_t *pnNumWaves)
	{
		TRACE_FUNC();
		return FACTWaveBank_GetNumWaves(waveBank, pnNumWaves);
	}
	COM_METHOD(uint16_t) GetWaveIndex(const char *szFriendlyName)
	{
		TRACE_FUNC();
		return FACTWaveBank_GetWaveIndex(waveBank, szFriendlyName);
	}
	COM_METHOD(HRESULT) GetWaveProperties(
		uint16_t nWaveIndex,
		XACT_WAVE_PROPERTIES *pWaveProperties
	) {
		TRACE_FUNC();
		return FACTWaveBank_GetWaveProperties(
			waveBank,
			nWaveIndex,
			pWaveProperties
		);
	}
	COM_METHOD(HRESULT) Prepare(
		uint16_t nWaveIndex,
		uint32_t dwFlags,
		uint32_t dwPlayOffset,
		uint8_t nLoopCount,
		IXACT3Wave **ppWave
	) {
		TRACE_FUNC();
		FACTWave *wave;
		HRESULT retval = FACTWaveBank_Prepare(
			waveBank,
			nWaveIndex,
			dwFlags,
			dwPlayOffset,
			nLoopCount,
			&wave
		);
		*ppWave = new XACT3WaveImpl(wave);
		return retval;
	}
	COM_METHOD(HRESULT) Play(
		uint16_t nWaveIndex,
		uint32_t dwFlags,
		uint32_t dwPlayOffset,
		uint8_t nLoopCount,
		IXACT3Wave **ppWave
	) {
		TRACE_FUNC();
		if (ppWave == NULL)
		{
			return FACTWaveBank_Play(
				waveBank,
				nWaveIndex,
				dwFlags,
				dwPlayOffset,
				nLoopCount,
				NULL
			);
		}
		FACTWave *wave;
		HRESULT retval = FACTWaveBank_Play(
			waveBank,
			nWaveIndex,
			dwFlags,
			dwPlayOffset,
			nLoopCount,
			&wave
		);
		*ppWave = new XACT3WaveImpl(wave);
		return retval;
	}
	COM_METHOD(HRESULT) Stop(
		uint16_t nWaveIndex,
		uint32_t dwFlags
	) {
		TRACE_FUNC();
		return FACTWaveBank_Stop(waveBank, nWaveIndex, dwFlags);
	}
	COM_METHOD(HRESULT) GetState(uint32_t *pdwState)
	{
		TRACE_FUNC();
		return FACTWaveBank_GetState(waveBank, pdwState);
	}

	FACTWaveBank *waveBank;
};

/* IXACT3Engine Implementation */

extern "C" size_t FAUDIOCALL wrap_io_read(
	void *data,
	void *dst,
	size_t size,
	size_t count
);
extern "C" int64_t FAUDIOCALL wrap_io_seek(void *data, int64_t offset, int whence);
extern "C" int FAUDIOCALL wrap_io_close(void *data);

class XACT3EngineImpl : public IXACT3Engine
{
public:
	XACT3EngineImpl()
	{
		FACTAudioEngine_Construct(&engine);
	}
	COM_METHOD(HRESULT) QueryInterface(REFIID riid, void **ppvInterface)
	{
		TRACE_FUNC();

		if (guid_equals(riid, IID_IXACT3Engine))
		{
			*ppvInterface = static_cast<IXACT3Engine *>(this);
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
		return FACTAudioEngine_AddRef(engine);
	}
	COM_METHOD(ULONG) Release()
	{
		TRACE_FUNC();
		ULONG refcount = FACTAudioEngine_Release(engine);
		if (refcount == 0)
		{
			delete this;
		}
		return 1;
	}
	COM_METHOD(HRESULT) GetRendererCount(
		uint16_t *pnRendererCount
	) {
		TRACE_FUNC();
		return FACTAudioEngine_GetRendererCount(
			engine,
			pnRendererCount
		);
	}
	COM_METHOD(HRESULT) GetRendererDetails(
		uint16_t nRendererIndex,
		XACT_RENDERER_DETAILS *pRendererDetails
	) {
		TRACE_FUNC();
		return FACTAudioEngine_GetRendererDetails(
			engine,
			nRendererIndex,
			pRendererDetails
		);
	}
	COM_METHOD(HRESULT) GetFinalMixFormat(
		WAVEFORMATEXTENSIBLE *pFinalMixFormat
	) {
		TRACE_FUNC();
		return FACTAudioEngine_GetFinalMixFormat(
			engine,
			pFinalMixFormat
		);
	}
	COM_METHOD(HRESULT) Initialize(
		const XACT_RUNTIME_PARAMETERS *pParams
	) {
		TRACE_FUNC();

		/* TODO: Unwrap FAudio/FAudioMasteringVoice */
		SDL_assert(pParams->pXAudio2 == NULL);
		SDL_assert(pParams->pMasteringVoice == NULL);

		return FACTAudioEngine_Initialize(engine, pParams);
	}
	COM_METHOD(HRESULT) ShutDown()
	{
		TRACE_FUNC();
		return FACTAudioEngine_ShutDown(engine);
	}
	COM_METHOD(HRESULT) DoWork()
	{
		TRACE_FUNC();
		return FACTAudioEngine_DoWork(engine);
	}
	COM_METHOD(HRESULT) CreateSoundBank(
		const void *pvBuffer,
		uint32_t dwSize,
		uint32_t dwFlags,
		uint32_t dwAllocAttributes,
		IXACT3SoundBank **ppSoundBank
	) {
		TRACE_FUNC();
		FACTSoundBank *soundBank;
		HRESULT retval = FACTAudioEngine_CreateSoundBank(
			engine,
			pvBuffer,
			dwSize,
			dwFlags,
			dwAllocAttributes,
			&soundBank
		);
		*ppSoundBank = new XACT3SoundBankImpl(soundBank);
		return retval;
	}
	COM_METHOD(HRESULT) CreateInMemoryWaveBank(
		const void *pvBuffer,
		uint32_t dwSize,
		uint32_t dwFlags,
		uint32_t dwAllocAttributes,
		IXACT3WaveBank **ppWaveBank
	) {
		TRACE_FUNC();
		FACTWaveBank *waveBank;
		HRESULT retval = FACTAudioEngine_CreateInMemoryWaveBank(
			engine,
			pvBuffer,
			dwSize,
			dwFlags,
			dwAllocAttributes,
			&waveBank
		);
		*ppWaveBank = new XACT3WaveBankImpl(waveBank);
		return retval;
	}
	COM_METHOD(HRESULT) CreateStreamingWaveBank(
		const XACT_STREAMING_PARAMETERS *pParms,
		IXACT3WaveBank **ppWaveBank
	) {
		TRACE_FUNC();

		/* We have to wrap the file around an IOStream first! */
		XACT_STREAMING_PARAMETERS fakeParms;
		FAudioIOStream *fake = (FAudioIOStream*) SDL_malloc(
			sizeof(FAudioIOStream)
		);
		fake->data = pParms->file;
		fake->read = wrap_io_read;
		fake->seek = wrap_io_seek;
		fake->close = wrap_io_close;
		fakeParms.file = fake;
		fakeParms.flags = pParms->flags;
		fakeParms.offset = pParms->offset;
		fakeParms.packetSize = pParms->packetSize;

		FACTWaveBank *waveBank;
		HRESULT retval = FACTAudioEngine_CreateStreamingWaveBank(
			engine,
			&fakeParms,
			&waveBank
		);
		*ppWaveBank = new XACT3WaveBankImpl(waveBank);
		return retval;
	}
	COM_METHOD(HRESULT) PrepareWave(
		uint32_t dwFlags,
		const char *szWavePath,
		uint32_t wStreamingPacketSize,
		uint32_t dwAlignment,
		uint32_t dwPlayOffset,
		uint8_t nLoopCount,
		IXACT3Wave **ppWave
	) {
		TRACE_FUNC();
		/* TODO: See FACT.c */
		return 0;
	}
	COM_METHOD(HRESULT) PrepareInMemoryWave(
		uint32_t dwFlags,
		WAVEBANKENTRY entry,
		uint32_t *pdwSeekTable, /* Optional! */
		uint8_t *pbWaveData,
		uint32_t dwPlayOffset,
		uint8_t nLoopCount,
		IXACT3Wave **ppWave
	) {
		TRACE_FUNC();
		/* TODO: See FACT.c */
		return 0;
	}
	COM_METHOD(HRESULT) PrepareStreamingWave(
		uint32_t dwFlags,
		WAVEBANKENTRY entry,
		XACT_STREAMING_PARAMETERS streamingParams,
		uint32_t dwAlignment,
		uint32_t *pdwSeekTable, /* Optional! */
		uint8_t *pbWaveData,
		uint32_t dwPlayOffset,
		uint8_t nLoopCount,
		IXACT3Wave **ppWave
	) {
		TRACE_FUNC();
		/* TODO: See FACT.c */
		return 0;
	}
	COM_METHOD(HRESULT) RegisterNotification(
		const XACT_NOTIFICATION_DESCRIPTION *pNotificationDescription
	) {
		TRACE_FUNC();

		/* We have to unwrap the FACT object first! */
		FACTNotificationDescription desc;
		desc.type = pNotificationDescription->type;
		desc.flags = pNotificationDescription->flags;
		desc.cueIndex = pNotificationDescription->cueIndex;
		desc.waveIndex = pNotificationDescription->waveIndex;
		desc.pvContext = pNotificationDescription->pvContext;
		if (desc.type == FACTNOTIFICATIONTYPE_CUEDESTROYED)
		{
			desc.pCue = ((XACT3CueImpl*) pNotificationDescription->pCue)->cue;
		}
		else if (desc.type == FACTNOTIFICATIONTYPE_SOUNDBANKDESTROYED)
		{
			desc.pSoundBank = ((XACT3SoundBankImpl*) pNotificationDescription->pSoundBank)->soundBank;
		}
		else if (desc.type == FACTNOTIFICATIONTYPE_WAVEBANKDESTROYED)
		{
			desc.pWaveBank = ((XACT3WaveBankImpl*) pNotificationDescription->pWaveBank)->waveBank;
		}
		else if (desc.type == FACTNOTIFICATIONTYPE_WAVEDESTROYED)
		{
			desc.pWave = ((XACT3WaveImpl*) pNotificationDescription->pWave)->wave;
		}
		// If you didn't hit an above if, get ready for an assert!

		return FACTAudioEngine_RegisterNotification(engine, &desc);
	}
	COM_METHOD(HRESULT) UnRegisterNotification(
		const XACT_NOTIFICATION_DESCRIPTION *pNotificationDescription
	) {
		TRACE_FUNC();

		/* We have to unwrap the FACT object first! */
		FACTNotificationDescription desc;
		desc.type = pNotificationDescription->type;
		desc.flags = pNotificationDescription->flags;
		desc.cueIndex = pNotificationDescription->cueIndex;
		desc.waveIndex = pNotificationDescription->waveIndex;
		desc.pvContext = pNotificationDescription->pvContext;
		if (desc.type == FACTNOTIFICATIONTYPE_CUEDESTROYED)
		{
			desc.pCue = ((XACT3CueImpl*) pNotificationDescription->pCue)->cue;
		}
		else if (desc.type == FACTNOTIFICATIONTYPE_SOUNDBANKDESTROYED)
		{
			desc.pSoundBank = ((XACT3SoundBankImpl*) pNotificationDescription->pSoundBank)->soundBank;
		}
		else if (desc.type == FACTNOTIFICATIONTYPE_WAVEBANKDESTROYED)
		{
			desc.pWaveBank = ((XACT3WaveBankImpl*) pNotificationDescription->pWaveBank)->waveBank;
		}
		else if (desc.type == FACTNOTIFICATIONTYPE_WAVEDESTROYED)
		{
			desc.pWave = ((XACT3WaveImpl*) pNotificationDescription->pWave)->wave;
		}
		// If you didn't hit an above if, get ready for an assert!

		return FACTAudioEngine_UnRegisterNotification(engine, &desc);
	}
	COM_METHOD(uint16_t) GetCategory(const char *szFriendlyName)
	{
		TRACE_FUNC();
		return FACTAudioEngine_GetCategory(engine, szFriendlyName);
	}
	COM_METHOD(HRESULT) Stop(uint16_t nCategory, uint32_t dwFlags)
	{
		TRACE_FUNC();
		return FACTAudioEngine_Stop(engine, nCategory, dwFlags);
	}
	COM_METHOD(HRESULT) SetVolume(uint16_t nCategory, float volume)
	{
		TRACE_FUNC();
		return FACTAudioEngine_SetVolume(engine, nCategory, volume);
	}
	COM_METHOD(HRESULT) Pause(uint16_t nCategory, int32_t fPause)
	{
		TRACE_FUNC();
		return FACTAudioEngine_Pause(engine, nCategory, fPause);
	}
	COM_METHOD(uint16_t) GetGlobalVariableIndex(
		const char *szFriendlyName
	) {
		TRACE_FUNC();
		return FACTAudioEngine_GetGlobalVariableIndex(
			engine,
			szFriendlyName
		);
	}
	COM_METHOD(HRESULT) SetGlobalVariable(
		uint16_t nIndex,
		float nValue
	) {
		TRACE_FUNC();
		return FACTAudioEngine_SetGlobalVariable(
			engine,
			nIndex,
			nValue
		);
	}
	COM_METHOD(HRESULT) GetGlobalVariable(
		uint16_t nIndex,
		float *pnValue
	) {
		TRACE_FUNC();
		return FACTAudioEngine_GetGlobalVariable(
			engine,
			nIndex,
			pnValue
		);
		
	}
private:
	FACTAudioEngine *engine;
};

/* Create Function */

void *CreateXACT3EngineInternal()
{
	return new XACT3EngineImpl();
}
