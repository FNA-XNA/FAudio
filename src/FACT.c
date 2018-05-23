/* FAudio - XAudio Reimplementation for FNA
 *
 * Copyright (c) 2011-2018 Ethan Lee, Luigi Auriemma, and the MonoGame Team
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * In no event will the authors be held liable for any damages arising from
 * the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 * claim that you wrote the original software. If you use this software in a
 * product, an acknowledgment in the product documentation would be
 * appreciated but is not required.
 *
 * 2. Altered source versions must be plainly marked as such, and must not be
 * misrepresented as being the original software.
 *
 * 3. This notice may not be removed or altered from any source distribution.
 *
 * Ethan "flibitijibibo" Lee <flibitijibibo@flibitijibibo.com>
 *
 */

#include "FAudioFX.h"
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
	(*ppEngine)->refcount = 1;
	return 0;
}

uint32_t FACTAudioEngine_AddRef(FACTAudioEngine *pEngine)
{
	pEngine->refcount += 1;
	return pEngine->refcount;
}

uint32_t FACTAudioEngine_Release(FACTAudioEngine *pEngine)
{
	pEngine->refcount -= 1;
	if (pEngine->refcount > 0)
	{
		return pEngine->refcount;
	}
	FACTAudioEngine_ShutDown(pEngine);
	FAudio_free(pEngine);
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
#if 0 /* TODO: Implement reverb first! */
	FAudioVoiceDetails masterDetails;
	FAudioEffectDescriptor reverbDesc;
	FAudioEffectChain reverbChain;
#endif

	/* Parse the file */
	parseRet = FACT_INTERNAL_ParseAudioEngine(pEngine, pParams);
	if (parseRet != 0)
	{
		return parseRet;
	}

	/* Assign the notification callback */
	pEngine->notificationCallback = pParams->fnNotificationCallback;

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
		if (deviceIndex > FAudio_PlatformGetDeviceCount())
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

#if 0 /* TODO: Implement reverb first! */
	/* Create the reverb effect, if applicable */
	if (pEngine->dspPresetCount > 0) /* Never more than 1...? */
	{
		FAudioVoice_GetVoiceDetails(pEngine->master, &masterDetails);

		/* Reverb effect chain... */
		FAudioCreateReverb(&reverbDesc.pEffect, 0);
		reverbDesc.InitialState = 1;
		reverbDesc.OutputChannels = masterDetails.InputChannels;
		reverbChain.EffectCount = 1;
		reverbChain.pEffectDescriptors = &reverbDesc;

		/* Reverb submix voice... */
		FAudio_CreateSubmixVoice(
			pEngine->audio,
			&pEngine->reverbVoice,
			1, /* Reverb will be omnidirectional */
			masterDetails.InputSampleRate,
			0,
			0,
			NULL,
			&reverbChain
		);

		/* We can release now, the submix owns this! */
		FAPOBase_Release(reverbDesc.pEffect);
	}
#endif
	return 0;
}

uint32_t FACTAudioEngine_ShutDown(FACTAudioEngine *pEngine)
{
	uint32_t i, refcount;

	/* Stop the platform stream before freeing stuff! */
	FAudio_StopEngine(pEngine->audio);

	/* This method destroys all existing cues, sound banks, and wave banks.
	 * It blocks until all cues are destroyed.
	 */
	while (pEngine->wbList != NULL)
	{
		FACTWaveBank_Destroy((FACTWaveBank*) pEngine->wbList->entry);
	}
	while (pEngine->sbList != NULL)
	{
		FACTSoundBank_Destroy((FACTSoundBank*) pEngine->sbList->entry);
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

	/* Audio resources */
	if (pEngine->reverbVoice != NULL)
	{
		FAudioVoice_DestroyVoice(pEngine->reverbVoice);
	}
	FAudioVoice_DestroyVoice(pEngine->master);
	FAudio_Release(pEngine->audio);

	/* Finally. */
	refcount = pEngine->refcount;
	FAudio_zero(pEngine, sizeof(FACTAudioEngine));
	pEngine->refcount = refcount;
	return 0;
}

uint32_t FACTAudioEngine_DoWork(FACTAudioEngine *pEngine)
{
	uint8_t i;
	FACTCue *cue;
	LinkedList *list;
	FAudio_PlatformLockAudio(pEngine->audio);
	list = pEngine->sbList;
	while (list != NULL)
	{
		cue = ((FACTSoundBank*) list->entry)->cueList;
		while (cue != NULL)
		{
			if (cue->active & 0x02)
			for (i = 0; i < cue->playing.sound.sound->trackCount; i += 1)
			{
				if (	cue->playing.sound.tracks[i].upcomingWave.wave == NULL &&
					cue->playing.sound.tracks[i].waveEvtInst->loopCount > 0	)
				{
					FACT_INTERNAL_GetNextWave(
						cue,
						cue->playing.sound.sound,
						&cue->playing.sound.sound->tracks[i],
						&cue->playing.sound.tracks[i],
						cue->playing.sound.tracks[i].waveEvt,
						cue->playing.sound.tracks[i].waveEvtInst
					);
				}
			}
			cue = cue->next;
		}
		list = list->next;
	}
	FAudio_PlatformUnlockAudio(pEngine->audio);
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
	io->seek(io->data, pParms->offset, 0);
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
	FAudio_assert(pEngine != NULL);
	FAudio_assert(pNotificationDescription != NULL);
	FAudio_assert(pEngine->notificationCallback != NULL);
	if (pNotificationDescription->type == FACTNOTIFICATIONTYPE_CUEDESTROYED)
	{
		pNotificationDescription->pCue->notifyOnDestroy = 1;
	}
	else if (pNotificationDescription->type == FACTNOTIFICATIONTYPE_SOUNDBANKDESTROYED)
	{
		pNotificationDescription->pSoundBank->notifyOnDestroy = 1;
	}
	else if (pNotificationDescription->type == FACTNOTIFICATIONTYPE_WAVEBANKDESTROYED)
	{
		pNotificationDescription->pWaveBank->notifyOnDestroy = 1;
	}
	else if (pNotificationDescription->type == FACTNOTIFICATIONTYPE_WAVEDESTROYED)
	{
		pNotificationDescription->pWave->notifyOnDestroy = 1;
	}
	else
	{
		FAudio_assert(0 && "TODO: Unimplemented notification!");
	}
	return 0;
}

uint32_t FACTAudioEngine_UnRegisterNotification(
	FACTAudioEngine *pEngine,
	const FACTNotificationDescription *pNotificationDescription
) {
	FAudio_assert(pEngine != NULL);
	FAudio_assert(pNotificationDescription != NULL);
	FAudio_assert(pEngine->notificationCallback != NULL);
	if (pNotificationDescription->type == FACTNOTIFICATIONTYPE_CUEDESTROYED)
	{
		pNotificationDescription->pCue->notifyOnDestroy = 0;
	}
	else if (pNotificationDescription->type == FACTNOTIFICATIONTYPE_SOUNDBANKDESTROYED)
	{
		pNotificationDescription->pSoundBank->notifyOnDestroy = 0;
	}
	else if (pNotificationDescription->type == FACTNOTIFICATIONTYPE_WAVEBANKDESTROYED)
	{
		pNotificationDescription->pWaveBank->notifyOnDestroy = 0;
	}
	else if (pNotificationDescription->type == FACTNOTIFICATIONTYPE_WAVEDESTROYED)
	{
		pNotificationDescription->pWave->notifyOnDestroy = 0;
	}
	else
	{
		FAudio_assert(0 && "TODO: Unimplemented notification!");
	}
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
	LinkedList *list = pEngine->sbList; \
	while (list != NULL) \
	{ \
		cue = ((FACTSoundBank*) list->entry)->cueList; \
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
		list = list->next; \
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

/* SoundBank implementation */

uint16_t FACTSoundBank_GetCueIndex(
	FACTSoundBank *pSoundBank,
	const char *szFriendlyName
) {
	uint16_t i;
	for (i = 0; i < pSoundBank->cueCount; i += 1)
	{
		if (FAudio_strcmp(szFriendlyName, pSoundBank->cueNames[i]) == 0)
		{
			return i;
		}
	}
	return FACTINDEX_INVALID;
}

uint32_t FACTSoundBank_GetNumCues(
	FACTSoundBank *pSoundBank,
	uint16_t *pnNumCues
) {
	*pnNumCues = pSoundBank->cueCount;
	return 0;
}

uint32_t FACTSoundBank_GetCueProperties(
	FACTSoundBank *pSoundBank,
	uint16_t nCueIndex,
	FACTCueProperties *pProperties
) {
	uint16_t i;
	FAudio_strlcpy(
		pProperties->friendlyName,
		pSoundBank->cueNames[nCueIndex],
		0xFF
	);
	if (!(pSoundBank->cues[nCueIndex].flags & 0x04))
	{
		for (i = 0; i < pSoundBank->variationCount; i += 1)
		{
			if (pSoundBank->variationCodes[i] == pSoundBank->cues[nCueIndex].sbCode)
			{
				break;
			}
		}

		FAudio_assert(i < pSoundBank->variationCount && "Variation table not found!");

		if (pSoundBank->variations[i].flags == 3)
		{
			pProperties->interactive = 1;
			pProperties->iaVariableIndex = pSoundBank->variations[i].variable;
		}
		else
		{
			pProperties->interactive = 0;
			pProperties->iaVariableIndex = 0;
		}
		pProperties->numVariations = pSoundBank->variations[i].entryCount;
	}
	else
	{
		pProperties->interactive = 0;
		pProperties->iaVariableIndex = 0;
		pProperties->numVariations = 0;
	}
	pProperties->maxInstances = pSoundBank->cues[nCueIndex].instanceLimit;
	pProperties->currentInstances = pSoundBank->cues[nCueIndex].instanceCount;
	return 0;
}

uint32_t FACTSoundBank_Prepare(
	FACTSoundBank *pSoundBank,
	uint16_t nCueIndex,
	uint32_t dwFlags,
	int32_t timeOffset,
	FACTCue** ppCue
) {
	uint16_t i;
	FACTCue *latest;

	*ppCue = (FACTCue*) FAudio_malloc(sizeof(FACTCue));
	FAudio_zero(*ppCue, sizeof(FACTCue));

	/* Engine references */
	(*ppCue)->parentBank = pSoundBank;
	(*ppCue)->next = NULL;
	(*ppCue)->managed = 0;
	(*ppCue)->index = nCueIndex;

	/* Sound data */
	(*ppCue)->data = &pSoundBank->cues[nCueIndex];
	if ((*ppCue)->data->flags & 0x04)
	{
		for (i = 0; i < pSoundBank->soundCount; i += 1)
		{
			if ((*ppCue)->data->sbCode == pSoundBank->soundCodes[i])
			{
				(*ppCue)->sound = &pSoundBank->sounds[i];
				break;
			}
		}
	}
	else
	{
		for (i = 0; i < pSoundBank->variationCount; i += 1)
		{
			if ((*ppCue)->data->sbCode == pSoundBank->variationCodes[i])
			{
				(*ppCue)->variation = &pSoundBank->variations[i];
				break;
			}
		}
		if ((*ppCue)->variation->flags == 3)
		{
			(*ppCue)->interactive = pSoundBank->parentEngine->variables[
				(*ppCue)->variation->variable
			].initialValue;
		}
	}

	/* Instance data */
	(*ppCue)->variableValues = (float*) FAudio_malloc(
		sizeof(float) * pSoundBank->parentEngine->variableCount
	);
	for (i = 0; i < pSoundBank->parentEngine->variableCount; i += 1)
	{
		(*ppCue)->variableValues[i] =
			pSoundBank->parentEngine->variables[i].initialValue;
	}

	/* Playback */
	(*ppCue)->state = FACT_STATE_PREPARED;

	/* Add to the SoundBank Cue list */
	if (pSoundBank->cueList == NULL)
	{
		pSoundBank->cueList = *ppCue;
	}
	else
	{
		latest = pSoundBank->cueList;
		while (latest->next != NULL)
		{
			latest = latest->next;
		}
		latest->next = *ppCue;
	}

	return 0;
}

uint32_t FACTSoundBank_Play(
	FACTSoundBank *pSoundBank,
	uint16_t nCueIndex,
	uint32_t dwFlags,
	int32_t timeOffset,
	FACTCue** ppCue /* Optional! */
) {
	FACTCue *result;
	FACTSoundBank_Prepare(
		pSoundBank,
		nCueIndex,
		dwFlags,
		timeOffset,
		&result
	);
	if (ppCue != NULL)
	{
		*ppCue = result;
	}
	else
	{
		/* AKA we get to Destroy() this ourselves */
		result->managed = 1;
	}
	FACTCue_Play(result);
	return 0;
}

uint32_t FACTSoundBank_Play3D(
	FACTSoundBank *pSoundBank,
	uint16_t nCueIndex,
	uint32_t dwFlags,
	int32_t timeOffset,
	F3DAUDIO_DSP_SETTINGS *pDSPSettings,
	FACTCue** ppCue /* Optional! */
) {
	FACTCue *result;
	FACTSoundBank_Prepare(
		pSoundBank,
		nCueIndex,
		dwFlags,
		timeOffset,
		&result
	);
	if (ppCue != NULL)
	{
		*ppCue = result;
	}
	else
	{
		/* AKA we get to Destroy() this ourselves */
		result->managed = 1;
	}
	FACT3DApply(pDSPSettings, result);
	FACTCue_Play(result);
	return 0;
}

uint32_t FACTSoundBank_Stop(
	FACTSoundBank *pSoundBank,
	uint16_t nCueIndex,
	uint32_t dwFlags
) {
	FACTCue *backup, *cue = pSoundBank->cueList;
	while (cue != NULL)
	{
		if (cue->index == nCueIndex)
		{
			if (	dwFlags == FACT_FLAG_STOP_IMMEDIATE &&
				cue->managed	)
			{
				/* Just blow this up now */
				backup = cue->next;
				FACTCue_Destroy(cue);
				cue = backup;
			}
			else
			{
				/* If managed, the mixer will destroy for us */
				FACTCue_Stop(cue, dwFlags);
				cue = cue->next;
			}
		}
		else
		{
			cue = cue->next;
		}
	}
	return 0;
}

uint32_t FACTSoundBank_Destroy(FACTSoundBank *pSoundBank)
{
	uint16_t i, j, k;
	FACTNotification note;

	/* Synchronously destroys all cues that are associated */
	while (pSoundBank->cueList != NULL)
	{
		FACTCue_Destroy(pSoundBank->cueList);
	}

	if (pSoundBank->parentEngine != NULL)
	{
		/* Remove this SoundBank from the Engine list */
		LinkedList_RemoveEntry(&pSoundBank->parentEngine->sbList, pSoundBank);
	}

	/* SoundBank Name */
	FAudio_free(pSoundBank->name);

	/* Cue data */
	FAudio_free(pSoundBank->cues);

	/* WaveBank Name data */
	for (i = 0; i < pSoundBank->wavebankCount; i += 1)
	{
		FAudio_free(pSoundBank->wavebankNames[i]);
	}
	FAudio_free(pSoundBank->wavebankNames);

	/* Sound data */
	for (i = 0; i < pSoundBank->soundCount; i += 1)
	{
		for (j = 0; j < pSoundBank->sounds[i].trackCount; j += 1)
		{
			for (k = 0; k < pSoundBank->sounds[i].tracks[j].eventCount; k += 1)
			{
				#define MATCH(t) \
					pSoundBank->sounds[i].tracks[j].events[k].type == t
				if (	MATCH(FACTEVENT_PLAYWAVE) ||
					MATCH(FACTEVENT_PLAYWAVETRACKVARIATION) ||
					MATCH(FACTEVENT_PLAYWAVEEFFECTVARIATION) ||
					MATCH(FACTEVENT_PLAYWAVETRACKEFFECTVARIATION)	)
				{
					if (pSoundBank->sounds[i].tracks[j].events[k].wave.isComplex)
					{
						FAudio_free(
							pSoundBank->sounds[i].tracks[j].events[k].wave.complex.tracks
						);
						FAudio_free(
							pSoundBank->sounds[i].tracks[j].events[k].wave.complex.wavebanks
						);
						FAudio_free(
							pSoundBank->sounds[i].tracks[j].events[k].wave.complex.weights
						);
					}
				}
				#undef MATCH
			}
			FAudio_free(pSoundBank->sounds[i].tracks[j].events);
		}
		FAudio_free(pSoundBank->sounds[i].tracks);
		FAudio_free(pSoundBank->sounds[i].rpcCodes);
		FAudio_free(pSoundBank->sounds[i].dspCodes);
	}
	FAudio_free(pSoundBank->sounds);
	FAudio_free(pSoundBank->soundCodes);

	/* Variation data */
	for (i = 0; i < pSoundBank->variationCount; i += 1)
	{
		FAudio_free(pSoundBank->variations[i].entries);
	}
	FAudio_free(pSoundBank->variations);
	FAudio_free(pSoundBank->variationCodes);

	/* Cue Name data */
	for (i = 0; i < pSoundBank->cueCount; i += 1)
	{
		FAudio_free(pSoundBank->cueNames[i]);
	}
	FAudio_free(pSoundBank->cueNames);

	/* Finally. */
	if (pSoundBank->notifyOnDestroy)
	{
		note.type = FACTNOTIFICATIONTYPE_SOUNDBANKDESTROYED;
		note.soundBank.pSoundBank = pSoundBank;
		pSoundBank->parentEngine->notificationCallback(&note);
	}
	FAudio_free(pSoundBank);
	return 0;
}

uint32_t FACTSoundBank_GetState(
	FACTSoundBank *pSoundBank,
	uint32_t *pdwState
) {
	uint16_t i;
	if (pSoundBank == NULL)
	{
		*pdwState = 0;
		return 0;
	}
	*pdwState = FACT_STATE_PREPARED;
	for (i = 0; i < pSoundBank->cueCount; i += 1)
	{
		if (pSoundBank->cues[i].instanceCount > 0)
		{
			*pdwState |= FACT_STATE_INUSE;
			return 0;
		}
	}
	return 0;
}

/* WaveBank implementation */

uint32_t FACTWaveBank_Destroy(FACTWaveBank *pWaveBank)
{
	FACTWave *wave;
	FACTNotification note;

	/* Synchronously destroys any cues that are using the wavebank */
	while (pWaveBank->waveList != NULL)
	{
		wave = (FACTWave*) pWaveBank->waveList->entry;
		if (wave->parentCue != NULL)
		{
			/* Destroying this Cue destroys the Wave */
			FACTCue_Destroy(wave->parentCue);
		}
		else
		{
			FACTWave_Destroy(wave);
		}
	}

	if (pWaveBank->parentEngine != NULL)
	{
		/* Remove this WaveBank from the Engine list */
		LinkedList_RemoveEntry(&pWaveBank->parentEngine->wbList, pWaveBank);
	}

	/* Free everything, finally. */
	FAudio_free(pWaveBank->name);
	FAudio_free(pWaveBank->entries);
	FAudio_free(pWaveBank->entryRefs);
	FAudio_close(pWaveBank->io);
	if (pWaveBank->notifyOnDestroy)
	{
		note.type = FACTNOTIFICATIONTYPE_WAVEBANKDESTROYED;
		note.waveBank.pWaveBank = pWaveBank;
		pWaveBank->parentEngine->notificationCallback(&note);
	}
	FAudio_free(pWaveBank);
	return 0;
}

uint32_t FACTWaveBank_GetState(
	FACTWaveBank *pWaveBank,
	uint32_t *pdwState
) {
	uint32_t i;
	if (pWaveBank == NULL)
	{
		*pdwState = 0;
		return 0;
	}
	*pdwState = FACT_STATE_PREPARED;
	for (i = 0; i < pWaveBank->entryCount; i += 1)
	{
		if (pWaveBank->entryRefs[i] > 0)
		{
			*pdwState |= FACT_STATE_INUSE;
			return 0;
		}
	}
	return 0;
}

uint32_t FACTWaveBank_GetNumWaves(
	FACTWaveBank *pWaveBank,
	uint16_t *pnNumWaves
) {
	*pnNumWaves = pWaveBank->entryCount;
	return 0;
}

uint16_t FACTWaveBank_GetWaveIndex(
	FACTWaveBank *pWaveBank,
	const char *szFriendlyName
) {
	FAudio_assert(0 && "WaveBank name tables are not supported!");
	return 0;
}

uint32_t FACTWaveBank_GetWaveProperties(
	FACTWaveBank *pWaveBank,
	uint16_t nWaveIndex,
	FACTWaveProperties *pWaveProperties
) {
	FACTWaveBankEntry *entry = &pWaveBank->entries[nWaveIndex];

	/* FIXME: Name tables! -flibit */
	FAudio_zero(
		pWaveProperties->friendlyName,
		sizeof(pWaveProperties->friendlyName)
	);

	pWaveProperties->format = entry->Format;
	pWaveProperties->durationInSamples = (
		entry->LoopRegion.dwStartSample +
		entry->LoopRegion.dwTotalSamples
	); /* FIXME: Do we just want the full wave block? -flibit */
	pWaveProperties->loopRegion = entry->LoopRegion;
	pWaveProperties->streaming = pWaveBank->streaming;
	return 0;
}

uint32_t FACTWaveBank_Prepare(
	FACTWaveBank *pWaveBank,
	uint16_t nWaveIndex,
	uint32_t dwFlags,
	uint32_t dwPlayOffset,
	uint8_t nLoopCount,
	FACTWave **ppWave
) {
	FAudioBuffer buffer;
	FAudioVoiceSends sends;
	FAudioSendDescriptor send;
	FAudioADPCMWaveFormat format;
	FACTWaveBankEntry *entry = &pWaveBank->entries[nWaveIndex];

	*ppWave = (FACTWave*) FAudio_malloc(sizeof(FACTWave));

	/* Engine references */
	(*ppWave)->parentBank = pWaveBank;
	(*ppWave)->parentCue = NULL;
	(*ppWave)->index = nWaveIndex;
	(*ppWave)->notifyOnDestroy = 0;

	/* Playback */
	(*ppWave)->state = FACT_STATE_PREPARED;
	(*ppWave)->volume = 1.0f;
	(*ppWave)->pitch = 0;
	if (dwFlags & FACT_FLAG_UNITS_MS)
	{
		(*ppWave)->initialPosition = (uint32_t) (
			( /* Samples per millisecond... */
				(float) entry->Format.nSamplesPerSec /
				1000.0f
			) * (float) dwPlayOffset
		);
	}
	else
	{
		(*ppWave)->initialPosition = dwPlayOffset;
	}
	(*ppWave)->loopCount = nLoopCount;

	/* Create the voice */
	send.Flags = 0;
	send.pOutputVoice = pWaveBank->parentEngine->master;
	sends.SendCount = 1;
	sends.pSends = &send;
	format.wfx.wFormatTag = entry->Format.wFormatTag;
	format.wfx.nChannels = entry->Format.nChannels;
	format.wfx.nSamplesPerSec = entry->Format.nSamplesPerSec;
	format.wfx.wBitsPerSample = 8 << entry->Format.wBitsPerSample;
	if (format.wfx.wFormatTag == 0)
	{
		format.wfx.wFormatTag = 1; /* PCM */
		format.wfx.nBlockAlign = format.wfx.nChannels * format.wfx.wBitsPerSample / 8;
		format.wfx.cbSize = 0;
	}
	else if (format.wfx.wFormatTag == 2)
	{
		format.wfx.nBlockAlign = (entry->Format.wBlockAlign + 22) * format.wfx.nChannels;
		format.wfx.cbSize = (
			sizeof(FAudioADPCMWaveFormat) -
			sizeof(FAudioWaveFormatEx)
		);
		format.wSamplesPerBlock = (
			((format.wfx.nBlockAlign / format.wfx.nChannels) - 6) * 2
		);
	}
	else /* Includes 0x1 - XMA, 0x3 - WMA */
	{
		FAudio_assert(0 && "Rebuild your WaveBanks with ADPCM!");
	}
	(*ppWave)->callback.callback.OnBufferEnd = pWaveBank->streaming ?
		FACT_INTERNAL_OnBufferEnd :
		NULL;
	(*ppWave)->callback.callback.OnBufferStart = NULL;
	(*ppWave)->callback.callback.OnLoopEnd = NULL;
	(*ppWave)->callback.callback.OnStreamEnd = FACT_INTERNAL_OnStreamEnd;
	(*ppWave)->callback.callback.OnVoiceError = NULL;
	(*ppWave)->callback.callback.OnVoiceProcessingPassEnd = NULL;
	(*ppWave)->callback.callback.OnVoiceProcessingPassStart = NULL;
	(*ppWave)->callback.wave = *ppWave;
	FAudio_CreateSourceVoice(
		pWaveBank->parentEngine->audio,
		&(*ppWave)->voice,
		&format.wfx,
		0,
		4.0f,
		(FAudioVoiceCallback*) &(*ppWave)->callback,
		&sends,
		NULL
	);
	if (pWaveBank->streaming)
	{
		/* Init stream cache info */
		if (format.wfx.wFormatTag == 1)
		{
			(*ppWave)->streamSize = (
				format.wfx.nSamplesPerSec *
				format.wfx.nChannels *
				(format.wfx.wBitsPerSample / 8)
			);
		}
		else if (format.wfx.wFormatTag == 2)
		{
			(*ppWave)->streamSize = (
				format.wfx.nSamplesPerSec /
				format.wSamplesPerBlock *
				format.wfx.nBlockAlign
			);
		}
		(*ppWave)->streamCache = (uint8_t*) FAudio_malloc((*ppWave)->streamSize);
		(*ppWave)->streamOffset = entry->PlayRegion.dwOffset;

		/* Read and submit first buffer from the WaveBank */
		FACT_INTERNAL_OnBufferEnd(&(*ppWave)->callback.callback, NULL);
	}
	else
	{
		buffer.Flags = FAUDIO_END_OF_STREAM;
		buffer.AudioBytes = entry->PlayRegion.dwLength;
		buffer.pAudioData = FAudio_memptr(
			pWaveBank->io,
			entry->PlayRegion.dwOffset
		);
		buffer.PlayBegin = (*ppWave)->initialPosition;
		buffer.PlayLength = entry->PlayRegion.dwLength;
		if (format.wfx.wFormatTag == 1)
		{
			buffer.PlayLength /= format.wfx.wBitsPerSample / 8;
			buffer.PlayLength /= format.wfx.nChannels;
		}
		else if (format.wfx.wFormatTag == 2)
		{
			buffer.PlayLength = (
				buffer.PlayLength /
				format.wfx.nBlockAlign *
				format.wSamplesPerBlock
			);
		}
		if (nLoopCount == 0)
		{
			buffer.LoopBegin = 0;
			buffer.LoopLength = 0;
			buffer.LoopCount = 0;
		}
		else
		{
			buffer.LoopBegin = entry->LoopRegion.dwStartSample;
			buffer.LoopLength = entry->LoopRegion.dwTotalSamples;
			buffer.LoopCount = nLoopCount;
		}
		buffer.pContext = NULL;
		FAudioSourceVoice_SubmitSourceBuffer((*ppWave)->voice, &buffer, NULL);
	}

	/* Add to the WaveBank Wave list */
	LinkedList_AddEntry(&pWaveBank->waveList, *ppWave);

	return 0;
}

uint32_t FACTWaveBank_Play(
	FACTWaveBank *pWaveBank,
	uint16_t nWaveIndex,
	uint32_t dwFlags,
	uint32_t dwPlayOffset,
	uint8_t nLoopCount,
	FACTWave **ppWave
) {
	FACTWaveBank_Prepare(
		pWaveBank,
		nWaveIndex,
		dwFlags,
		dwPlayOffset,
		nLoopCount,
		ppWave
	);
	FACTWave_Play(*ppWave);
	return 0;
}

uint32_t FACTWaveBank_Stop(
	FACTWaveBank *pWaveBank,
	uint16_t nWaveIndex,
	uint32_t dwFlags
) {
	FACTWave *wave;
	LinkedList *list = pWaveBank->waveList;
	while (list != NULL)
	{
		wave = (FACTWave*) list->entry;
		if (wave->index == nWaveIndex)
		{
			FACTWave_Stop(wave, dwFlags);
		}
		list = list->next;
	}
	return 0;
}

/* Wave implementation */

uint32_t FACTWave_Destroy(FACTWave *pWave)
{
	FACTNotification note;

	/* Stop before we start deleting everything */
	FACTWave_Stop(pWave, FACT_FLAG_STOP_IMMEDIATE);

	LinkedList_RemoveEntry(&pWave->parentBank->waveList, pWave);

	FAudioVoice_DestroyVoice(pWave->voice);
	if (pWave->notifyOnDestroy)
	{
		note.type = FACTNOTIFICATIONTYPE_WAVEDESTROYED;
		note.wave.pWave = pWave;
		pWave->parentBank->parentEngine->notificationCallback(&note);
	}
	FAudio_free(pWave);
	return 0;
}

uint32_t FACTWave_Play(FACTWave *pWave)
{
	FAudio_assert(!(pWave->state & (FACT_STATE_PLAYING | FACT_STATE_STOPPING)));
	pWave->state |= FACT_STATE_PLAYING;
	pWave->state &= ~(
		FACT_STATE_PAUSED |
		FACT_STATE_STOPPED
	);
	FAudioSourceVoice_Start(pWave->voice, 0, 0);
	return 0;
}

uint32_t FACTWave_Stop(FACTWave *pWave, uint32_t dwFlags)
{
	/* There are two ways that a Wave might be stopped immediately:
	 * 1. The program explicitly asks for it
	 * 2. The Wave is paused and therefore we can't do fade/release effects
	 */
	if (	dwFlags & FACT_FLAG_STOP_IMMEDIATE ||
		pWave->state & FACT_STATE_PAUSED	)
	{
		pWave->state |= FACT_STATE_STOPPED;
		pWave->state &= ~(
			FACT_STATE_PLAYING |
			FACT_STATE_STOPPING |
			FACT_STATE_PAUSED
		);
		FAudioSourceVoice_Stop(pWave->voice, 0, 0);
		FAudioSourceVoice_FlushSourceBuffers(pWave->voice);
	}
	else
	{
		pWave->state |= FACT_STATE_STOPPING;
		FAudioSourceVoice_ExitLoop(pWave->voice, 0);
	}
	return 0;
}

uint32_t FACTWave_Pause(FACTWave *pWave, int32_t fPause)
{
	/* FIXME: Does the Cue STOPPING/STOPPED rule apply here too? */
	if (pWave->state & (FACT_STATE_STOPPING | FACT_STATE_STOPPED))
	{
		return 0;
	}

	/* All we do is set the flag, the mixer handles the rest */
	if (fPause)
	{
		pWave->state |= FACT_STATE_PAUSED;
		FAudioSourceVoice_Stop(pWave->voice, 0, 0);
	}
	else
	{
		pWave->state &= ~FACT_STATE_PAUSED;
		FAudioSourceVoice_Start(pWave->voice, 0, 0);
	}
	return 0;
}

uint32_t FACTWave_GetState(FACTWave *pWave, uint32_t *pdwState)
{
	*pdwState = pWave->state;
	return 0;
}

uint32_t FACTWave_SetPitch(FACTWave *pWave, int16_t pitch)
{
	pWave->pitch = FAudio_clamp(
		pitch,
		FACTPITCH_MIN_TOTAL,
		FACTPITCH_MAX_TOTAL
	);
	FAudioSourceVoice_SetFrequencyRatio(
		pWave->voice,
		(float) FAudio_pow(2.0, pWave->pitch / 1200.0),
		0
	);
	return 0;
}

uint32_t FACTWave_SetVolume(FACTWave *pWave, float volume)
{
	pWave->volume = FAudio_clamp(
		volume,
		FACTVOLUME_MIN,
		FACTVOLUME_MAX
	);
	FAudioVoice_SetVolume(
		pWave->voice,
		volume,
		0
	);
	return 0;
}

uint32_t FACTWave_SetMatrixCoefficients(
	FACTWave *pWave,
	uint32_t uSrcChannelCount,
	uint32_t uDstChannelCount,
	float *pMatrixCoefficients
) {
	FAudioVoice_SetOutputMatrix(
		pWave->voice,
		pWave->voice->sends.pSends->pOutputVoice,
		uSrcChannelCount,
		uDstChannelCount,
		pMatrixCoefficients,
		0
	);
	return 0;
}

uint32_t FACTWave_GetProperties(
	FACTWave *pWave,
	FACTWaveInstanceProperties *pProperties
) {
	FACTWaveBank_GetWaveProperties(
		pWave->parentBank,
		pWave->index,
		&pProperties->properties
	);

	/* FIXME: This is unsupported on PC, do we care about this? */
	pProperties->backgroundMusic = 0;

	return 0;
}

/* Cue implementation */

uint32_t FACTCue_Destroy(FACTCue *pCue)
{
	FACTCue *cue, *prev;
	FACTNotification note;

	/* Stop before we start deleting everything */
	FACTCue_Stop(pCue, FACT_FLAG_STOP_IMMEDIATE);

	if (pCue->parentBank != NULL)
	{
		/* Remove this Cue from the SoundBank list */
		cue = pCue->parentBank->cueList;
		prev = cue;
		while (cue != NULL)
		{
			if (cue == pCue)
			{
				if (cue == prev) /* First in list */
				{
					pCue->parentBank->cueList = cue->next;
				}
				else
				{
					prev->next = cue->next;
				}
				break;
			}
			prev = cue;
			cue = cue->next;
		}
		FAudio_assert(cue != NULL && "Could not find Cue reference!");
	}

	FAudio_free(pCue->variableValues);
	if (pCue->notifyOnDestroy)
	{
		note.type = FACTNOTIFICATIONTYPE_CUEDESTROYED;
		note.cue.pCue = pCue;
		pCue->parentBank->parentEngine->notificationCallback(&note);
	}
	FAudio_free(pCue);
	return 0;
}

uint32_t FACTCue_Play(FACTCue *pCue)
{
	union
	{
		float maxf;
		uint8_t maxi;
	} max;
	FACTCue *tmp, *wnr;
	uint16_t categoryIndex;
	FACTAudioCategory *category;
	FACTCueData *data = &pCue->parentBank->cues[pCue->index];

	FAudio_assert(!(pCue->state & (FACT_STATE_PLAYING | FACT_STATE_STOPPING)));

	FAudio_PlatformLockAudio(pCue->parentBank->parentEngine->audio);

	/* Instance Limits */
	#define INSTANCE_BEHAVIOR(obj, match) \
		wnr = NULL; \
		tmp = pCue->parentBank->cueList; \
		if (obj->maxInstanceBehavior == 0) /* Fail */ \
		{ \
			pCue->state |= FACT_STATE_STOPPED; \
			pCue->state &= ~( \
				FACT_STATE_PLAYING | \
				FACT_STATE_STOPPING | \
				FACT_STATE_PAUSED \
			); \
			FAudio_PlatformUnlockAudio(pCue->parentBank->parentEngine->audio); \
			return 1; \
		} \
		else if (obj->maxInstanceBehavior == 1) /* Queue */ \
		{ \
			/* FIXME: How is this different from Replace Oldest? */ \
			while (tmp != NULL) \
			{ \
				if (	tmp != pCue && \
					match && \
					!(tmp->state & (FACT_STATE_STOPPING | FACT_STATE_STOPPED))	) \
				{ \
					wnr = tmp; \
					break; \
				} \
				tmp = tmp->next; \
			} \
		} \
		else if (obj->maxInstanceBehavior == 2) /* Replace Oldest */ \
		{ \
			while (tmp != NULL) \
			{ \
				if (	tmp != pCue && \
					match && \
					!(tmp->state & (FACT_STATE_STOPPING | FACT_STATE_STOPPED))	) \
				{ \
					wnr = tmp; \
					break; \
				} \
				tmp = tmp->next; \
			} \
		} \
		else if (obj->maxInstanceBehavior == 3) /* Replace Quietest */ \
		{ \
			max.maxf = FACTVOLUME_MAX; \
			while (tmp != NULL) \
			{ \
				if (	tmp != pCue && \
					match && \
					/*FIXME: tmp->playing.sound.volume < max.maxf &&*/ \
					!(tmp->state & (FACT_STATE_STOPPING | FACT_STATE_STOPPED))	) \
				{ \
					wnr = tmp; \
					/* max.maxf = tmp->playing.sound.volume; */ \
				} \
				tmp = tmp->next; \
			} \
		} \
		else if (obj->maxInstanceBehavior == 4) /* Replace Lowest Priority */ \
		{ \
			max.maxi = 0xFF; \
			while (tmp != NULL) \
			{ \
				if (	tmp != pCue && \
					match && \
					tmp->playing.sound.sound->priority < max.maxi && \
					!(tmp->state & (FACT_STATE_STOPPING | FACT_STATE_STOPPED))	) \
				{ \
					wnr = tmp; \
					max.maxi = tmp->playing.sound.sound->priority; \
				} \
				tmp = tmp->next; \
			} \
		} \
		if (wnr != NULL) \
		{ \
			if (obj->fadeInMS > 0) \
			{ \
				FACT_INTERNAL_BeginFadeIn(pCue); \
			} \
			FACTCue_Stop(wnr, 0); \
		}
	if (data->instanceCount >= data->instanceLimit)
	{
		INSTANCE_BEHAVIOR(
			data,
			tmp->index == pCue->index
		)
	}
	else if (pCue->active & 0x02)
	{
		categoryIndex = pCue->playing.sound.sound->category;
		category = &pCue->parentBank->parentEngine->categories[categoryIndex];
		if (category->instanceCount >= category->instanceLimit)
		{
			INSTANCE_BEHAVIOR(
				category,
				tmp->playing.sound.sound->category == categoryIndex
			)
		}
		category->instanceCount += 1;
	}
	data->instanceCount += 1;
	#undef INSTANCE_BEHAVIOR

	/* Need an initial sound to play */
	FACT_INTERNAL_SelectSound(pCue);

	pCue->state |= FACT_STATE_PLAYING;
	pCue->state &= ~(
		FACT_STATE_PAUSED |
		FACT_STATE_STOPPED
	);
	pCue->start = FAudio_timems();

	/* If it's a simple wave, just play it! */
	if (pCue->active & 0x01)
	{
		if (pCue->active3D)
		{
			FACTWave_SetMatrixCoefficients(
				pCue->playing.wave,
				pCue->srcChannels,
				pCue->dstChannels,
				pCue->matrixCoefficients
			);
		}
		FACTWave_Play(pCue->playing.wave);
	}

	FAudio_PlatformUnlockAudio(pCue->parentBank->parentEngine->audio);

	return 0;
}

uint32_t FACTCue_Stop(FACTCue *pCue, uint32_t dwFlags)
{
	if (pCue->state & FACT_STATE_STOPPED)
	{
		return 0;
	}

	/* There are three ways that a Cue might be stopped immediately:
	 * 1. The program explicitly asks for it
	 * 2. The Cue is paused and therefore we can't do fade/release effects
	 * 3. The Cue is stopped "as authored" and has no fade effects
	 */
	if (	dwFlags & FACT_FLAG_STOP_IMMEDIATE ||
		pCue->state & FACT_STATE_PAUSED	||
		pCue->parentBank->cues[pCue->index].fadeOutMS == 0	)
	{
		pCue->start = 0;
		pCue->elapsed = 0;
		pCue->state |= FACT_STATE_STOPPED;
		pCue->state &= ~(
			FACT_STATE_PLAYING |
			FACT_STATE_STOPPING |
			FACT_STATE_PAUSED
		);

		/* FIXME: Lock audio mixer before freeing this! */
		if (pCue->active & 0x01)
		{
			FACTWave_Destroy(pCue->playing.wave);
		}
		else if (pCue->active & 0x02)
		{
			FACT_INTERNAL_DestroySound(pCue);
		}
		pCue->data->instanceCount -= 1;
		pCue->active = 0;
	}
	else
	{
		FACT_INTERNAL_BeginFadeOut(pCue);
		pCue->state |= FACT_STATE_STOPPING;
	}
	return 0;
}

uint32_t FACTCue_GetState(FACTCue *pCue, uint32_t *pdwState)
{
	*pdwState = pCue->state;
	return 0;
}

uint32_t FACTCue_SetMatrixCoefficients(
	FACTCue *pCue,
	uint32_t uSrcChannelCount,
	uint32_t uDstChannelCount,
	float *pMatrixCoefficients
) {
	uint8_t i;

	/* See FACTCue.matrixCoefficients declaration */
	FAudio_assert(uSrcChannelCount > 0 && uSrcChannelCount < 3);
	FAudio_assert(uDstChannelCount > 0 && uDstChannelCount < 9);

	/* Local storage */
	pCue->srcChannels = uSrcChannelCount;
	pCue->dstChannels = uDstChannelCount;
	FAudio_memcpy(
		pCue->matrixCoefficients,
		pMatrixCoefficients,
		sizeof(float) * uSrcChannelCount * uDstChannelCount
	);
	pCue->active3D = 1;

	/* Apply to Waves if they exist */
	if (pCue->active & 0x01)
	{
		FACTWave_SetMatrixCoefficients(
			pCue->playing.wave,
			uSrcChannelCount,
			uDstChannelCount,
			pMatrixCoefficients
		);
	}
	else if (pCue->active & 0x02)
	{
		for (i = 0; i < pCue->playing.sound.sound->trackCount; i += 1)
		{
			if (pCue->playing.sound.tracks[i].activeWave.wave != NULL)
			{
				FACTWave_SetMatrixCoefficients(
					pCue->playing.sound.tracks[i].activeWave.wave,
					uSrcChannelCount,
					uDstChannelCount,
					pMatrixCoefficients
				);
			}
		}
	}
	return 0;
}

uint16_t FACTCue_GetVariableIndex(
	FACTCue *pCue,
	const char *szFriendlyName
) {
	uint16_t i;
	for (i = 0; i < pCue->parentBank->parentEngine->variableCount; i += 1)
	{
		if (	FAudio_strcmp(szFriendlyName, pCue->parentBank->parentEngine->variableNames[i]) == 0 &&
			pCue->parentBank->parentEngine->variables[i].accessibility & 0x04	)
		{
			return i;
		}
	}
	return FACTVARIABLEINDEX_INVALID;
}

uint32_t FACTCue_SetVariable(
	FACTCue *pCue,
	uint16_t nIndex,
	float nValue
) {
	FACTVariable *var = &pCue->parentBank->parentEngine->variables[nIndex];
	FAudio_assert(var->accessibility & 0x01);
	FAudio_assert(!(var->accessibility & 0x02));
	FAudio_assert(var->accessibility & 0x04);
	pCue->variableValues[nIndex] = FAudio_clamp(
		nValue,
		var->minValue,
		var->maxValue
	);
	return 0;
}

uint32_t FACTCue_GetVariable(
	FACTCue *pCue,
	uint16_t nIndex,
	float *nValue
) {
	FACTVariable *var = &pCue->parentBank->parentEngine->variables[nIndex];
	FAudio_assert(var->accessibility & 0x01);
	FAudio_assert(var->accessibility & 0x04);

	if (nIndex == 0) /* NumCueInstances */
	{
		*nValue = pCue->parentBank->cues[pCue->index].instanceCount;
	}
	else
	{
		*nValue = pCue->variableValues[nIndex];
	}
	return 0;
}

uint32_t FACTCue_Pause(FACTCue *pCue, int32_t fPause)
{
	uint8_t i;

	/* "A stopping or stopped cue cannot be paused." */
	if (pCue->state & (FACT_STATE_STOPPING | FACT_STATE_STOPPED))
	{
		return 0;
	}

	/* Store elapsed time */
	pCue->elapsed += FAudio_timems() - pCue->start;

	/* All we do is set the flag, not much to see here */
	if (fPause)
	{
		pCue->state |= FACT_STATE_PAUSED;
	}
	else
	{
		pCue->state &= ~FACT_STATE_PAUSED;
	}

	/* Pause the Waves */
	if (pCue->active & 0x01)
	{
		FACTWave_Pause(pCue->playing.wave, fPause);
	}
	else if (pCue->active & 0x02)
	{
		for (i = 0; i < pCue->playing.sound.sound->trackCount; i += 1)
		{
			if (pCue->playing.sound.tracks[i].activeWave.wave != NULL)
			{
				FACTWave_Pause(
					pCue->playing.sound.tracks[i].activeWave.wave,
					fPause
				);
			}
		}
	}

	return 0;
}

uint32_t FACTCue_GetProperties(
	FACTCue *pCue,
	FACTCueInstanceProperties *ppProperties
) {
	FACTVariationProperties *varProps;
	FACTSoundProperties *sndProps;

	ppProperties->allocAttributes = 0;
	FACTSoundBank_GetCueProperties(
		pCue->parentBank,
		pCue->index,
		&ppProperties->cueProperties
	);

	varProps = &ppProperties->activeVariationProperties.variationProperties;
	sndProps = &ppProperties->activeVariationProperties.soundProperties;

	/* Variation Properties */
	if (pCue->playingVariation != NULL)
	{
		varProps->index = 0; /* TODO: Index of what...? */
		/* TODO: This is just max - min right? Also why u8 wtf */
		varProps->weight = (uint8_t) (
			pCue->playingVariation->maxWeight -
			pCue->playingVariation->minWeight
		);
		if (pCue->variation->flags == 3)
		{
			varProps->iaVariableMin =
				pCue->playingVariation->minWeight;
			varProps->iaVariableMax =
				pCue->playingVariation->maxWeight;
		}
		else
		{
			varProps->iaVariableMin = 0;
			varProps->iaVariableMax = 0;
		}
		varProps->linger = pCue->playingVariation->linger;
	}
	else
	{
		/* No variations here! */
		FAudio_zero(
			varProps,
			sizeof(FACTVariationProperties)
		);
	}

	/* Sound Properties */
	if (pCue->active & 0x02)
	{
		/* TODO: u8->float volume conversion crap */
		sndProps->category = pCue->playing.sound.sound->category;
		sndProps->priority = pCue->playing.sound.sound->priority;
		sndProps->pitch = pCue->playing.sound.sound->pitch;
		sndProps->volume = pCue->playing.sound.sound->volume;
		sndProps->numTracks = pCue->playing.sound.sound->trackCount;
		/* TODO: arrTrackProperties[0] */
	}
	else
	{
		/* It's either a simple wave or nothing's playing */
		FAudio_zero(
			sndProps,
			sizeof(FACTSoundProperties)
		);
	}

	return 0;
}

uint32_t FACTCue_SetOutputVoices(
	FACTCue *pCue,
	const FAudioVoiceSends *pSendList /* Optional XAUDIO2_VOICE_SENDS */
) {
	/* TODO */
	return 0;
}

uint32_t FACTCue_SetOutputVoiceMatrix(
	FACTCue *pCue,
	const FAudioVoice *pDestinationVoice, /* Optional! */
	uint32_t SourceChannels,
	uint32_t DestinationChannels,
	const float *pLevelMatrix /* SourceChannels * DestinationChannels */
) {
	/* TODO */
	return 0;
}
