/* FACT - XACT Reimplementation for FNA
 * Copyright 2009-2018 Ethan Lee and the MonoGame Team
 *
 * Released under the Microsoft Public License.
 * See LICENSE for details.
 */

#include "FAudio_internal.h"

/* FAudio Interface */

uint32_t FAudioCreate(
	FAudio **ppFAudio,
	uint32_t Flags,
	FAudioProcessor XAudio2Processor
) {
	*ppFAudio = (FAudio*) FAudio_malloc(sizeof(FAudio));
	FAudio_zero(*ppFAudio, sizeof(FAudio));
	FAudio_Initialize(*ppFAudio, Flags, XAudio2Processor);
	return 0;
}

void FAudioDestroy(FAudio *audio)
{
	/* TODO: Delete all of the voices still allocated */
	FAudio_StopEngine(audio);
	FAudio_free(audio);
}

uint32_t FAudio_GetDeviceCount(FAudio *audio, uint32_t *pCount)
{
	*pCount = FAudio_PlatformGetDeviceCount();
	return 0;
}

uint32_t FAudio_GetDeviceDetails(
	FAudio *audio,
	uint32_t Index,
	FAudioDeviceDetails *pDeviceDetails
) {
	FAudio_PlatformGetDeviceDetails(Index, pDeviceDetails);
	return 0;
}

uint32_t FAudio_Initialize(
	FAudio *audio,
	uint32_t Flags,
	FAudioProcessor XAudio2Processor
) {
	FAudio_assert(Flags == 0);
	FAudio_assert(XAudio2Processor == FAUDIO_DEFAULT_PROCESSOR);
	FAudio_StartEngine(audio);
	return 0;
}

uint32_t FAudio_RegisterForCallbacks(
	FAudio *audio,
	FAudioEngineCallback *pCallback
) {
	FAudioEngineCallbackEntry *latest;
	FAudioEngineCallbackEntry *entry = (FAudioEngineCallbackEntry*) FAudio_malloc(
		sizeof(FAudioEngineCallbackEntry)
	);
	entry->callback = pCallback;
	entry->next = NULL;
	if (audio->callbacks == NULL)
	{
		audio->callbacks = entry;
	}
	else
	{
		latest = audio->callbacks;
		while (latest->next != NULL)
		{
			latest = latest->next;
		}
		latest->next = entry;
	}
	return 0;
}

void FAudio_UnregisterForCallbacks(
	FAudio *audio,
	FAudioEngineCallback *pCallback
) {
	FAudioEngineCallbackEntry *entry, *prev;
	if (audio->callbacks == NULL)
	{
		return;
	}
	entry = audio->callbacks;
	prev = audio->callbacks;
	while (entry != NULL)
	{
		if (entry->callback == pCallback)
		{
			if (entry == prev) /* First in list */
			{
				audio->callbacks = entry->next;
			}
			else
			{
				prev->next = entry->next;
			}
			FAudio_free(entry);
			return;
		}
		entry = entry->next;
	}
}

uint32_t FAudio_CreateSourceVoice(
	FAudio *audio,
	FAudioSourceVoice **ppSourceVoice,
	const FAudioWaveFormatEx *pSourceFormat,
	uint32_t Flags,
	float MaxFrequencyRatio,
	FAudioVoiceCallback *pCallback,
	const FAudioVoiceSends *pSendList,
	const FAudioEffectChain *pEffectChain
) {
	*ppSourceVoice = (FAudioSourceVoice*) FAudio_malloc(sizeof(FAudioVoice));
	(*ppSourceVoice)->type = FAUDIO_VOICE_SOURCE;
	(*ppSourceVoice)->filter.Type = (FAudioFilterType) 0xFF;

	/* Default Levels */
	(*ppSourceVoice)->volume = 1.0f;
	(*ppSourceVoice)->channelVolume[0] = 1.0f;
	(*ppSourceVoice)->channelVolume[1] = 1.0f;
	FAudio_assert(pSourceFormat->nChannels > 0 && pSourceFormat->nChannels < 3);
	(*ppSourceVoice)->srcChannels = pSourceFormat->nChannels;
	(*ppSourceVoice)->dstChannels = 2; /* FIXME: ??? */
	if (pSourceFormat->nChannels == 2)
	{
		(*ppSourceVoice)->matrixCoefficients[0] = 1.0f;
		(*ppSourceVoice)->matrixCoefficients[1] = 0.0f;
		(*ppSourceVoice)->matrixCoefficients[2] = 0.0f;
		(*ppSourceVoice)->matrixCoefficients[3] = 1.0f;
	}
	else
	{
		(*ppSourceVoice)->matrixCoefficients[0] = 1.0f;
		(*ppSourceVoice)->matrixCoefficients[1] = 1.0f;
	}

	/* Sends/Effects */
	FAudioVoice_SetOutputVoices(*ppSourceVoice, pSendList);
	FAudioVoice_SetEffectChain(*ppSourceVoice, pEffectChain);

	/* Source Properties */
	FAudio_assert(MaxFrequencyRatio <= FAUDIO_MAX_FREQ_RATIO);
	(*ppSourceVoice)->src.maxFreqRatio = MaxFrequencyRatio;
	FAudio_memcpy(
		&(*ppSourceVoice)->src.format,
		pSourceFormat,
		sizeof(FAudioWaveFormatEx)
	);
	(*ppSourceVoice)->src.callback = pCallback;
	(*ppSourceVoice)->src.active = 0;
	(*ppSourceVoice)->src.freqRatio = 1.0f;
	(*ppSourceVoice)->src.totalSamples = 0;
	(*ppSourceVoice)->src.bufferList = NULL;
	return 0;
}

uint32_t FAudio_CreateSubmixVoice(
	FAudio *audio,
	FAudioSubmixVoice **ppSubmixVoice,
	uint32_t InputChannels,
	uint32_t InputSampleRate,
	uint32_t Flags,
	uint32_t ProcessingStage,
	const FAudioVoiceSends *pSendList,
	const FAudioEffectChain *pEffectChain
) {
	*ppSubmixVoice = (FAudioSubmixVoice*) FAudio_malloc(sizeof(FAudioVoice));
	(*ppSubmixVoice)->type = FAUDIO_VOICE_SUBMIX;
	(*ppSubmixVoice)->filter.Type = (FAudioFilterType) 0xFF;

	/* Default Levels */
	(*ppSubmixVoice)->volume = 1.0f;
	(*ppSubmixVoice)->channelVolume[0] = 1.0f;
	(*ppSubmixVoice)->channelVolume[1] = 1.0f;
	FAudio_assert(InputChannels > 0 && InputChannels < 3);
	(*ppSubmixVoice)->srcChannels = InputChannels;
	(*ppSubmixVoice)->dstChannels = 2; /* FIXME: ??? */
	if (InputChannels == 2)
	{
		(*ppSubmixVoice)->matrixCoefficients[0] = 1.0f;
		(*ppSubmixVoice)->matrixCoefficients[1] = 0.0f;
		(*ppSubmixVoice)->matrixCoefficients[2] = 0.0f;
		(*ppSubmixVoice)->matrixCoefficients[3] = 1.0f;
	}
	else
	{
		(*ppSubmixVoice)->matrixCoefficients[0] = 1.0f;
		(*ppSubmixVoice)->matrixCoefficients[1] = 1.0f;
	}

	/* Sends/Effects */
	FAudioVoice_SetOutputVoices(*ppSubmixVoice, pSendList);
	FAudioVoice_SetEffectChain(*ppSubmixVoice, pEffectChain);

	/* Submix Properties */
	(*ppSubmixVoice)->mix.inputChannels = InputChannels;
	(*ppSubmixVoice)->mix.inputSampleRate = InputSampleRate;
	/* TODO: ProcessingStage */
	return 0;
}

uint32_t FAudio_CreateMasteringVoice(
	FAudio *audio,
	FAudioMasteringVoice **ppMasteringVoice,
	uint32_t InputChannels,
	uint32_t InputSampleRate,
	uint32_t Flags,
	uint32_t DeviceIndex,
	const FAudioEffectChain *pEffectChain
) {
	/* For now we only support one allocated master voice at a time */
	FAudio_assert(audio->master == NULL);

	*ppMasteringVoice = (FAudioMasteringVoice*) FAudio_malloc(sizeof(FAudioVoice));
	(*ppMasteringVoice)->type = FAUDIO_VOICE_MASTER;
	(*ppMasteringVoice)->filter.Type = (FAudioFilterType) 0xFF;

	/* Default Levels */
	(*ppMasteringVoice)->volume = 1.0f;
	(*ppMasteringVoice)->channelVolume[0] = 1.0f;
	(*ppMasteringVoice)->channelVolume[1] = 1.0f;
	/* FIXME: Master matrix coefficients */
	FAudio_assert(InputChannels > 0 && InputChannels < 3);
	(*ppMasteringVoice)->srcChannels = InputChannels;
	(*ppMasteringVoice)->dstChannels = 2;
	if (InputChannels == 2)
	{
		(*ppMasteringVoice)->matrixCoefficients[0] = 1.0f;
		(*ppMasteringVoice)->matrixCoefficients[1] = 0.0f;
		(*ppMasteringVoice)->matrixCoefficients[2] = 0.0f;
		(*ppMasteringVoice)->matrixCoefficients[3] = 1.0f;
	}
	else
	{
		(*ppMasteringVoice)->matrixCoefficients[0] = 1.0f;
		(*ppMasteringVoice)->matrixCoefficients[1] = 1.0f;
	}

	/* Sends/Effects */
	FAudio_zero(&(*ppMasteringVoice)->sends, sizeof(FAudioVoiceSends));
	FAudioVoice_SetEffectChain(*ppMasteringVoice, pEffectChain);

	/* Master Properties */
	(*ppMasteringVoice)->master.inputChannels = InputChannels;
	(*ppMasteringVoice)->master.inputSampleRate = InputSampleRate;
	(*ppMasteringVoice)->master.deviceIndex = DeviceIndex;
	(*ppMasteringVoice)->master.audio = audio;

	/* Platform Device */
	audio->master = *ppMasteringVoice;
	FAudio_PlatformInit(audio);
	if (audio->active)
	{
		FAudio_PlatformStart(audio);
	}
	return 0;
}

uint32_t FAudio_StartEngine(FAudio *audio)
{
	if (!audio->active)
	{
		audio->active = 1;
		if (audio->master != NULL)
		{
			FAudio_PlatformStart(audio);
		}
	}
	return 0;
}

void FAudio_StopEngine(FAudio *audio)
{
	if (audio->active)
	{
		audio->active = 0;
		if (audio->master != NULL)
		{
			FAudio_PlatformStop(audio);
		}
	}
}

uint32_t FAudio_CommitChanges(FAudio *audio)
{
	FAudio_assert(0 && "Batching is not supported!");
	return 0;
}

void FAudio_GetPerformanceData(
	FAudio *audio,
	FAudioPerformanceData *pPerfData
) {
	FAudio_assert(0 && "TODO: Performance metrics!");
}

void FAudio_SetDebugConfiguration(
	FAudio *audio,
	FAudioDebugConfiguration *pDebugConfiguration,
	void* pReserved
) {
	FAudio_assert(0 && "TODO: Debug configuration!");
}

/* FAudioVoice Interface */

void FAudioVoice_GetVoiceDetails(
	FAudioVoice *voice,
	FAudioVoiceDetails *pVoiceDetails
) {
	pVoiceDetails->CreationFlags = voice->flags;
	if (voice->type == FAUDIO_VOICE_SOURCE)
	{
		pVoiceDetails->InputChannels = voice->src.format.nChannels;
		pVoiceDetails->InputSampleRate = voice->src.format.nSamplesPerSec;
	}
	else if (voice->type == FAUDIO_VOICE_SUBMIX)
	{
		pVoiceDetails->InputChannels = voice->mix.inputChannels;
		pVoiceDetails->InputSampleRate = voice->mix.inputSampleRate;
	}
	else if (voice->type == FAUDIO_VOICE_MASTER)
	{
		pVoiceDetails->InputChannels = voice->master.inputChannels;
		pVoiceDetails->InputSampleRate = voice->master.inputSampleRate;
	}
	else
	{
		FAudio_assert(0 && "Unknown voice type!");
	}
}

uint32_t FAudioVoice_SetOutputVoices(
	FAudioVoice *voice,
	const FAudioVoiceSends *pSendList
) {
	/* FIXME: This is lazy... */
	if (voice->sends.pSends != NULL)
	{
		FAudio_free(voice->sends.pSends);
	}

	if (pSendList == NULL)
	{
		FAudio_zero(&voice->sends, sizeof(FAudioVoiceSends));
	}
	else
	{
		voice->sends.SendCount = pSendList->SendCount;
		voice->sends.pSends = (FAudioSendDescriptor*) FAudio_malloc(
			pSendList->SendCount * sizeof(FAudioSendDescriptor)
		);
		FAudio_memcpy(
			voice->sends.pSends,
			pSendList->pSends,
			pSendList->SendCount * sizeof(FAudioSendDescriptor)
		);
	}
	return 0;
}

uint32_t FAudioVoice_SetEffectChain(
	FAudioVoice *voice,
	const FAudioEffectChain *pEffectChain
) {
	/* FIXME: This is lazy... */
	if (voice->effects.pEffectDescriptors != NULL)
	{
		FAudio_free(voice->effects.pEffectDescriptors);
	}

	if (pEffectChain == NULL)
	{
		FAudio_zero(&voice->effects, sizeof(FAudioEffectChain));
	}
	else
	{
		voice->effects.EffectCount = pEffectChain->EffectCount;
		voice->effects.pEffectDescriptors = (FAudioEffectDescriptor*) FAudio_malloc(
			pEffectChain->EffectCount * sizeof(FAudioEffectDescriptor)
		);
		FAudio_memcpy(
			voice->effects.pEffectDescriptors,
			pEffectChain->pEffectDescriptors,
			pEffectChain->EffectCount * sizeof(FAudioEffectDescriptor)
		);
	}
	return 0;
}

uint32_t FAudioVoice_EnableEffect(
	FAudioVoice *voice,
	uint32_t EffectIndex,
	uint32_t OperationSet
) {
	FAudio_assert(OperationSet == FAUDIO_COMMIT_NOW);

	voice->effects.pEffectDescriptors[EffectIndex].InitialState = 1;
	return 0;
}

uint32_t FAudioVoice_DisableEffect(
	FAudioVoice *voice,
	uint32_t EffectIndex,
	uint32_t OperationSet
) {
	FAudio_assert(OperationSet == FAUDIO_COMMIT_NOW);

	voice->effects.pEffectDescriptors[EffectIndex].InitialState = 0;
	return 0;
}

void FAudioVoice_GetEffectState(
	FAudioVoice *voice,
	uint32_t EffectIndex,
	uint8_t *pEnabled
) {
	*pEnabled = voice->effects.pEffectDescriptors[EffectIndex].InitialState;
}

uint32_t FAudioVoice_SetEffectParameters(
	FAudioVoice *voice,
	uint32_t EffectIndex,
	const void *pParameters,
	uint32_t ParametersByteSize,
	uint32_t OperationSet
) {
	/* TODO: XAPO */
	FAudio_assert(OperationSet == FAUDIO_COMMIT_NOW);
	return 0;
}

uint32_t FAudioVoice_GetEffectParameters(
	FAudioVoice *voice,
	void *pParameters,
	uint32_t ParametersByteSize
) {
	/* TODO: XAPO */
	return 0;
}

uint32_t FAudioVoice_SetFilterParameters(
	FAudioVoice *voice,
	const FAudioFilterParameters *pParameters,
	uint32_t OperationSet
) {
	FAudio_assert(OperationSet == FAUDIO_COMMIT_NOW);

	FAudio_memcpy(
		&voice->filter,
		pParameters,
		sizeof(FAudioFilterParameters)
	);
	return 0;
}

void FAudioVoice_GetFilterParameters(
	FAudioVoice *voice,
	FAudioFilterParameters *pParameters
) {
	FAudio_memcpy(
		pParameters,
		&voice->filter,
		sizeof(FAudioFilterParameters)
	);
}

uint32_t FAudioVoice_SetOutputFilterParameters(
	FAudioVoice *voice,
	FAudioVoice *pDestinationVoice,
	const FAudioFilterParameters *pParameters,
	uint32_t OperationSet
) {
	FAudio_assert(OperationSet == FAUDIO_COMMIT_NOW);
	FAudio_assert(0 && "Output filters are not supported!");
	return 0;
}

void FAudioVoice_GetOutputFilterParameters(
	FAudioVoice *voice,
	FAudioVoice *pDestinationVoice,
	FAudioFilterParameters *pParameters
) {
	FAudio_assert(0 && "Output filters are not supported!");
}

uint32_t FAudioVoice_SetVolume(
	FAudioVoice *voice,
	float Volume,
	uint32_t OperationSet
) {
	FAudio_assert(OperationSet == FAUDIO_COMMIT_NOW);

	FAudio_assert(Volume <= FAUDIO_MAX_VOLUME_LEVEL);
	voice->volume = Volume;
	return 0;
}

void FAudioVoice_GetVolume(
	FAudioVoice *voice,
	float *pVolume
) {
	*pVolume = voice->volume;
}

uint32_t FAudioVoice_SetChannelVolumes(
	FAudioVoice *voice,
	uint32_t Channels,
	const float *pVolumes,
	uint32_t OperationSet
) {
	FAudio_assert(OperationSet == FAUDIO_COMMIT_NOW);

	FAudio_assert(Channels > 0 && Channels < 3);
	FAudio_memcpy(
		voice->channelVolume,
		pVolumes,
		sizeof(float) * Channels
	);
	return 0;
}

void FAudioVoice_GetChannelVolumes(
	FAudioVoice *voice,
	uint32_t Channels,
	float *pVolumes
) {
	FAudio_assert(Channels > 0 && Channels < 3);
	FAudio_memcpy(
		pVolumes,
		voice->channelVolume,
		sizeof(float) * Channels
	);
}

uint32_t FAudioVoice_SetOutputMatrix(
	FAudioVoice *voice,
	FAudioVoice *pDestinationVoice,
	uint32_t SourceChannels,
	uint32_t DestinationChannels,
	const float *pLevelMatrix,
	uint32_t OperationSet
) {
	FAudio_assert(OperationSet == FAUDIO_COMMIT_NOW);

	FAudio_assert(SourceChannels > 0 && SourceChannels < 3);
	FAudio_assert(DestinationChannels > 0 && DestinationChannels < 9);
	voice->srcChannels = SourceChannels;
	voice->dstChannels = DestinationChannels;
	FAudio_memcpy(
		voice->matrixCoefficients,
		pLevelMatrix,
		sizeof(float) * SourceChannels * DestinationChannels
	);
	return 0;
}

void FAudioVoice_GetOutputMatrix(
	FAudioVoice *voice,
	FAudioVoice *pDestinationVoice,
	uint32_t SourceChannels,
	uint32_t DestinationChannels,
	float *pLevelMatrix
) {
	FAudio_assert(SourceChannels > 0 && SourceChannels < 3);
	FAudio_assert(DestinationChannels > 0 && DestinationChannels < 9);
	voice->srcChannels = SourceChannels;
	voice->dstChannels = DestinationChannels;
	FAudio_memcpy(
		pLevelMatrix,
		voice->matrixCoefficients,
		sizeof(float) * SourceChannels * DestinationChannels
	);
}

void FAudioVoice_DestroyVoice(FAudioVoice *voice)
{
	/* TODO: Lock, check for dependencies and fail if still in use */
	if (voice->type == FAUDIO_VOICE_MASTER)
	{
		FAudio_PlatformQuit(voice->master.audio);
		voice->master.audio->master = NULL;
	}
	if (voice->sends.pSends != NULL)
	{
		FAudio_free(voice->sends.pSends);
	}
	if (voice->effects.pEffectDescriptors != NULL)
	{
		FAudio_free(voice->effects.pEffectDescriptors);
	}
	FAudio_free(voice);
}

/* FAudioSourceVoice Interface */

uint32_t FAudioSourceVoice_Start(
	FAudioSourceVoice *voice,
	uint32_t Flags,
	uint32_t OperationSet
) {
	FAudio_assert(OperationSet == FAUDIO_COMMIT_NOW);
	FAudio_assert(voice->type == FAUDIO_VOICE_SOURCE);

	FAudio_assert(Flags == 0);
	voice->src.active = 1;
	return 0;
}

uint32_t FAudioSourceVoice_Stop(
	FAudioSourceVoice *voice,
	uint32_t Flags,
	uint32_t OperationSet
) {
	FAudio_assert(OperationSet == FAUDIO_COMMIT_NOW);
	FAudio_assert(voice->type == FAUDIO_VOICE_SOURCE);

	FAudio_assert(!(Flags & FAUDIO_PLAY_TAILS)); /* FIXME: ??? */
	voice->src.active = 0;
	return 0;
}

uint32_t FAudioSourceVoice_SubmitSourceBuffer(
	FAudioSourceVoice *voice,
	const FAudioBuffer *pBuffer,
	const FAudioBufferWMA *pBufferWMA
) {
	FAudioBufferEntry *entry, *list;
	FAudio_assert(voice->type == FAUDIO_VOICE_SOURCE);
	FAudio_assert(pBufferWMA == NULL);

	entry = (FAudioBufferEntry*) FAudio_malloc(sizeof(FAudioBufferEntry));
	FAudio_memcpy(&entry->buffer, pBuffer, sizeof(FAudioBuffer));
	entry->next = NULL;
	if (voice->src.bufferList == NULL)
	{
		voice->src.bufferList = entry;
	}
	else
	{
		list = voice->src.bufferList;
		while (list->next != NULL)
		{
			list = list->next;
		}
		list->next = entry;
	}
	return 0;
}

uint32_t FAudioSourceVoice_FlushSourceBuffers(
	FAudioSourceVoice *voice
) {
	FAudioBufferEntry *entry, *next;
	FAudio_assert(voice->type == FAUDIO_VOICE_SOURCE);

	/* If the source is playing, don't flush the active buffer */
	entry = voice->src.bufferList;
	if (voice->src.active && entry != NULL)
	{
		entry = entry->next;
	}

	/* Go through each buffer, send an event for each one before deleting */
	while (entry != NULL)
	{
		if (voice->src.callback->OnBufferEnd != NULL)
		{
			voice->src.callback->OnBufferEnd(
				voice->src.callback,
				entry->buffer.pContext
			);
		}
		next = entry->next;
		FAudio_free(entry);
		entry = next;
	}
	return 0;
}

uint32_t FAudioSourceVoice_Discontinuity(
	FAudioSourceVoice *voice
) {
	FAudio_assert(voice->type == FAUDIO_VOICE_SOURCE);

	/* As far as I know this doesn't matter for us...?
	 * This exists so the engine doesn't try to spit out random memory,
	 * but like... can't we just not send samples with no buffers?
	 * -flibit
	 */
	return 0;
}

uint32_t FAudioSourceVoice_ExitLoop(
	FAudioSourceVoice *voice,
	uint32_t OperationSet
) {
	FAudio_assert(OperationSet == FAUDIO_COMMIT_NOW);
	FAudio_assert(voice->type == FAUDIO_VOICE_SOURCE);

	if (voice->src.bufferList != NULL)
	{
		voice->src.bufferList->buffer.LoopCount = 0;
	}
	return 0;
}

void FAudioSourceVoice_GetState(
	FAudioSourceVoice *voice,
	FAudioVoiceState *pVoiceState
) {
	FAudioBufferEntry *entry;
	FAudio_assert(voice->type == FAUDIO_VOICE_SOURCE);

	pVoiceState->BuffersQueued = 0;
	pVoiceState->SamplesPlayed = voice->src.totalSamples;
	if (voice->src.bufferList != NULL)
	{
		entry = voice->src.bufferList;
		pVoiceState->pCurrentBufferContext = entry->buffer.pContext;
		do
		{
			pVoiceState->BuffersQueued += 1;
			entry = entry->next;
		} while (entry != NULL);
	}
	else
	{
		pVoiceState->pCurrentBufferContext = NULL;
	}
}

uint32_t FAudioSourceVoice_SetFrequencyRatio(
	FAudioSourceVoice *voice,
	float Ratio,
	uint32_t OperationSet
) {
	FAudio_assert(OperationSet == FAUDIO_COMMIT_NOW);
	FAudio_assert(voice->type == FAUDIO_VOICE_SOURCE);

	FAudio_assert(	Ratio >= FAUDIO_MIN_FREQ_RATIO &&
			Ratio <= voice->src.maxFreqRatio	);
	voice->src.freqRatio = Ratio;
	return 0;
}

void FAudioSourceVoice_GetFrequencyRatio(
	FAudioSourceVoice *voice,
	float *pRatio
) {
	FAudio_assert(voice->type == FAUDIO_VOICE_SOURCE);

	*pRatio = voice->src.freqRatio;
}

uint32_t FAudioSourceVoice_SetSourceSampleRate(
	FAudioSourceVoice *voice,
	uint32_t NewSourceSampleRate
) {
	FAudio_assert(voice->type == FAUDIO_VOICE_SOURCE);

	FAudio_assert(	NewSourceSampleRate >= FAUDIO_MIN_SAMPLE_RATE &&
			NewSourceSampleRate <= FAUDIO_MAX_SAMPLE_RATE	);
	voice->src.format.nSamplesPerSec = NewSourceSampleRate;
	return 0;
}
