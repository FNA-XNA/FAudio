/* FACT - XACT Reimplementation for FNA
 * Copyright 2009-2018 Ethan Lee and the MonoGame Team
 *
 * Released under the Microsoft Public License.
 * See LICENSE for details.
 */

#include "FACT_internal.h"

/* AudioEngine implementation */

uint32_t FACTCreateEngine(
	uint32_t dwCreationFlags,
	FACTAudioEngine **ppEngine
) {
	*ppEngine = (FACTAudioEngine*) FAudio_malloc(sizeof(FACTAudioEngine));
	if (*ppEngine == NULL)
	{
		return -1; /* TODO: E_OUTOFMEMORY */
	}
	FAudio_zero(*ppEngine, sizeof(FACTAudioEngine));
	return 0;
}

uint32_t FACTAudioEngine_GetRendererCount(
	FACTAudioEngine *pEngine,
	uint16_t *pnRendererCount
) {
	*pnRendererCount = (uint16_t) FAudio_PlatformGetDeviceCount();
	return 0;
}

uint32_t FACTAudioEngine_GetRendererDetails(
	FACTAudioEngine *pEngine,
	uint16_t nRendererIndex,
	FACTRendererDetails *pRendererDetails
) {
	FAudioDeviceDetails deviceDetails;
	FAudio_PlatformGetDeviceDetails(
		nRendererIndex,
		&deviceDetails
	);
	FAudio_memcpy(
		pRendererDetails->rendererID,
		deviceDetails.DeviceID,
		sizeof(int16_t) * 0xFF
	);
	FAudio_memcpy(
		pRendererDetails->displayName,
		deviceDetails.DisplayName,
		sizeof(int16_t) * 0xFF
	);
	/* FIXME: Which defaults does it care about...? */
	pRendererDetails->defaultDevice = (deviceDetails.Role & (
		GlobalDefaultDevice |
		DefaultGameDevice
	)) != 0;
	return 0;
}

uint32_t FACTAudioEngine_GetFinalMixFormat(
	FACTAudioEngine *pEngine,
	FAudioWaveFormatExtensible *pFinalMixFormat
) {
	FAudio_memcpy(
		pFinalMixFormat,
		pEngine->audio->mixFormat,
		sizeof(FAudioWaveFormatExtensible)
	);
	return 0;
}

uint32_t FACTAudioEngine_Initialize(
	FACTAudioEngine *pEngine,
	const FACTRuntimeParameters *pParams
) {
	uint32_t parseRet;
	uint32_t deviceIndex;

	/* Parse the file */
	parseRet = FACT_INTERNAL_ParseAudioEngine(pEngine, pParams);
	if (parseRet != 0)
	{
		return parseRet;
	}
	/* Init the FAudio subsystem */
	pEngine->audio = pParams->pXAudio2;
	if (pEngine->audio == NULL)
	{
		FAudioCreate(&pEngine->audio, 0, FAUDIO_DEFAULT_PROCESSOR);
	}

	/* We're being a bit dastardly and passing a struct that starts with
	 * the type that FAudio expects, but with extras at the end so we can
	 * do proper stuff with the engine
	 */
	pEngine->callback.callback.OnProcessingPassStart =
		FACT_INTERNAL_OnProcessingPassStart;
	pEngine->callback.engine = pEngine;
	FAudio_RegisterForCallbacks(
		pEngine->audio,
		(FAudioEngineCallback*) &pEngine->callback
	);

	/* Create the audio device */
	if (pParams->pRendererID == NULL)
	{
		deviceIndex = 0;
	}
	else
	{
		/* FIXME: wchar_t is an asshole */
		deviceIndex = pParams->pRendererID[0] - L'0';
		if (	deviceIndex < 0 ||
			deviceIndex > FAudio_PlatformGetDeviceCount()	)
		{
			deviceIndex = 0;
		}
	}
	FAudio_CreateMasteringVoice(
		pEngine->audio,
		&pEngine->master,
		FAUDIO_DEFAULT_CHANNELS,
		48000, /* Should be FAUDIO_DEFAULT_SAMPLERATE, but SDL... */
		0,
		deviceIndex,
		NULL
	);
	return 0;
}

uint32_t FACTAudioEngine_Shutdown(FACTAudioEngine *pEngine)
{
	uint32_t i;
	FACTSoundBank *sb;
	FACTWaveBank *wb;

	/* Stop the platform stream before freeing stuff! */
	FAudio_StopEngine(pEngine->audio);

	/* Unreference all the Banks */
	sb = pEngine->sbList;
	while (sb != NULL)
	{
		sb->parentEngine = NULL;
		sb = sb->next;
	}
	wb = pEngine->wbList;
	while (wb != NULL)
	{
		wb->parentEngine = NULL;
		wb = wb->next;
	}

	/* Category data */
	for (i = 0; i < pEngine->categoryCount; i += 1)
	{
		FAudio_free(pEngine->categoryNames[i]);
	}
	FAudio_free(pEngine->categoryNames);
	FAudio_free(pEngine->categories);

	/* Variable data */
	for (i = 0; i < pEngine->variableCount; i += 1)
	{
		FAudio_free(pEngine->variableNames[i]);
	}
	FAudio_free(pEngine->variableNames);
	FAudio_free(pEngine->variables);
	FAudio_free(pEngine->globalVariableValues);

	/* RPC data */
	for (i = 0; i < pEngine->rpcCount; i += 1)
	{
		FAudio_free(pEngine->rpcs[i].points);
	}
	FAudio_free(pEngine->rpcs);
	FAudio_free(pEngine->rpcCodes);

	/* DSP data */
	for (i = 0; i < pEngine->dspPresetCount; i += 1)
	{
		FAudio_free(pEngine->dspPresets[i].parameters);
	}
	FAudio_free(pEngine->dspPresets);
	FAudio_free(pEngine->dspPresetCodes);

	/* Finally. */
	FAudioVoice_DestroyVoice(pEngine->master);
	FAudioDestroy(pEngine->audio);
	FAudio_free(pEngine);
	return 0;
}

uint32_t FACTAudioEngine_DoWork(FACTAudioEngine *pEngine)
{
	return 0;
}

uint32_t FACTAudioEngine_CreateSoundBank(
	FACTAudioEngine *pEngine,
	const void *pvBuffer,
	uint32_t dwSize,
	uint32_t dwFlags,
	uint32_t dwAllocAttributes,
	FACTSoundBank **ppSoundBank
) {
	return FACT_INTERNAL_ParseSoundBank(
		pEngine,
		pvBuffer,
		dwSize,
		ppSoundBank
	);
}

uint32_t FACTAudioEngine_CreateInMemoryWaveBank(
	FACTAudioEngine *pEngine,
	const void *pvBuffer,
	uint32_t dwSize,
	uint32_t dwFlags,
	uint32_t dwAllocAttributes,
	FACTWaveBank **ppWaveBank
) {
	return FACT_INTERNAL_ParseWaveBank(
		pEngine,
		FAudio_memopen((void*) pvBuffer, dwSize),
		0,
		ppWaveBank
	);
}

uint32_t FACTAudioEngine_CreateStreamingWaveBank(
	FACTAudioEngine *pEngine,
	const FACTStreamingParameters *pParms,
	FACTWaveBank **ppWaveBank
) {
	FAudioIOStream *io = (FAudioIOStream*) pParms->file;
	io->seek(io, pParms->offset, 0);
	return FACT_INTERNAL_ParseWaveBank(
		pEngine,
		io,
		1,
		ppWaveBank
	);
}

uint32_t FACTAudioEngine_PrepareWave(
	FACTAudioEngine *pEngine,
	uint32_t dwFlags,
	const char *szWavePath,
	uint32_t wStreamingPacketSize,
	uint32_t dwAlignment,
	uint32_t dwPlayOffset,
	uint8_t nLoopCount,
	FACTWave **ppWave
) {
	/* TODO: FACTWave */
	return 0;
}

uint32_t FACTAudioEngine_PrepareInMemoryWave(
	FACTAudioEngine *pEngine,
	uint32_t dwFlags,
	FACTWaveBankEntry entry,
	uint32_t *pdwSeekTable, /* Optional! */
	uint8_t *pbWaveData,
	uint32_t dwPlayOffset,
	uint8_t nLoopCount,
	FACTWave **ppWave
) {
	/* TODO: FACTWave */
	return 0;
}

uint32_t FACTAudioEngine_PrepareStreamingWave(
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
) {
	/* TODO: FACTWave */
	return 0;
}

uint32_t FACTAudioEngine_RegisterNotification(
	FACTAudioEngine *pEngine,
	const FACTNotificationDescription *pNotificationDescription
) {
	/* TODO: Notifications */
	return 0;
}

uint32_t FACTAudioEngine_UnRegisterNotification(
	FACTAudioEngine *pEngine,
	const FACTNotificationDescription *pNotificationDescription
) {
	/* TODO: Notifications */
	return 0;
}

uint16_t FACTAudioEngine_GetCategory(
	FACTAudioEngine *pEngine,
	const char *szFriendlyName
) {
	uint16_t i;
	for (i = 0; i < pEngine->categoryCount; i += 1)
	{
		if (FAudio_strcmp(szFriendlyName, pEngine->categoryNames[i]) == 0)
		{
			return i;
		}
	}
	return FACTCATEGORY_INVALID;
}

uint8_t FACT_INTERNAL_IsInCategory(
	FACTAudioEngine *engine,
	uint16_t target,
	uint16_t category
) {
	FACTAudioCategory *cat;

	/* Same category, no need to go on a crazy hunt */
	if (category == target)
	{
		return 1;
	}

	/* Right, on with the crazy hunt */
	cat = &engine->categories[category];
	while (cat->parentCategory != -1)
	{
		if (cat->parentCategory == target)
		{
			return 1;
		}
		cat = &engine->categories[cat->parentCategory];
	}
	return 0;
}

#define ITERATE_CUES(action) \
	FACTCue *cue; \
	FACTSoundBank *sb = pEngine->sbList; \
	while (sb != NULL) \
	{ \
		cue = sb->cueList; \
		while (cue != NULL) \
		{ \
			if (	cue->active & 0x02 && \
				FACT_INTERNAL_IsInCategory( \
					cue->parentBank->parentEngine, \
					nCategory, \
					cue->playing.sound.sound->category \
				)	) \
			{ \
				action \
			} \
			cue = cue->next; \
		} \
		sb = sb->next; \
	}

uint32_t FACTAudioEngine_Stop(
	FACTAudioEngine *pEngine,
	uint16_t nCategory,
	uint32_t dwFlags
) {
	ITERATE_CUES(
		if (	dwFlags == FACT_FLAG_STOP_IMMEDIATE &&
			cue->managed	)
		{
			/* Just blow this up now */
			FACTCue_Destroy(cue);
		}
		else
		{
			/* If managed, the mixer will destroy for us */
			FACTCue_Stop(cue, dwFlags);
		}
	)
	return 0;
}

uint32_t FACTAudioEngine_SetVolume(
	FACTAudioEngine *pEngine,
	uint16_t nCategory,
	float volume
) {
	uint16_t i;
	pEngine->categories[nCategory].currentVolume = (
		pEngine->categories[nCategory].volume *
		volume
	);
	for (i = 0; i < pEngine->categoryCount; i += 1)
	{
		if (pEngine->categories[i].parentCategory == nCategory)
		{
			FACTAudioEngine_SetVolume(
				pEngine,
				i,
				pEngine->categories[i].currentVolume
			);
		}
	}
	return 0;
}

uint32_t FACTAudioEngine_Pause(
	FACTAudioEngine *pEngine,
	uint16_t nCategory,
	int32_t fPause
) {
	ITERATE_CUES(FACTCue_Pause(cue, fPause);)
	return 0;
}

#undef ITERATE_CUES

uint16_t FACTAudioEngine_GetGlobalVariableIndex(
	FACTAudioEngine *pEngine,
	const char *szFriendlyName
) {
	uint16_t i;
	for (i = 0; i < pEngine->variableCount; i += 1)
	{
		if (	FAudio_strcmp(szFriendlyName, pEngine->variableNames[i]) == 0 &&
			!(pEngine->variables[i].accessibility & 0x04)	)
		{
			return i;
		}
	}
	return FACTVARIABLEINDEX_INVALID;
}

uint32_t FACTAudioEngine_SetGlobalVariable(
	FACTAudioEngine *pEngine,
	uint16_t nIndex,
	float nValue
) {
	FACTVariable *var = &pEngine->variables[nIndex];
	FAudio_assert(var->accessibility & 0x01);
	FAudio_assert(!(var->accessibility & 0x02));
	FAudio_assert(!(var->accessibility & 0x04));
	pEngine->globalVariableValues[nIndex] = FAudio_clamp(
		nValue,
		var->minValue,
		var->maxValue
	);
	return 0;
}

uint32_t FACTAudioEngine_GetGlobalVariable(
	FACTAudioEngine *pEngine,
	uint16_t nIndex,
	float *pnValue
) {
	FACTVariable *var = &pEngine->variables[nIndex];
	FAudio_assert(var->accessibility & 0x01);
	FAudio_assert(!(var->accessibility & 0x04));
	*pnValue = pEngine->globalVariableValues[nIndex];
	return 0;
}
