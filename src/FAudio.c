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
	*ppSourceVoice = FAudio_malloc(sizeof(FAudioVoice));
	(*ppSourceVoice)->type = FAUDIO_VOICE_SOURCE;

	/* Sends/Effects */
	if (pSendList == NULL)
	{
		FAudio_zero(&(*ppSourceVoice)->sends, sizeof(FAudioVoiceSends));
	}
	else
	{
		(*ppSourceVoice)->sends.SendCount = pSendList->SendCount;
		(*ppSourceVoice)->sends.pSends = (FAudioSendDescriptor*) FAudio_malloc(
			pSendList->SendCount * sizeof(FAudioSendDescriptor)
		);
		FAudio_memcpy(
			(*ppSourceVoice)->sends.pSends,
			pSendList->pSends,
			pSendList->SendCount * sizeof(FAudioSendDescriptor)
		);
	}
	if (pEffectChain == NULL)
	{
		FAudio_zero(&(*ppSourceVoice)->effects, sizeof(FAudioEffectChain));
	}
	else
	{
		(*ppSourceVoice)->effects.EffectCount = pEffectChain->EffectCount;
		(*ppSourceVoice)->effects.pEffectDescriptors = (FAudioEffectDescriptor*) FAudio_malloc(
			pEffectChain->EffectCount * sizeof(FAudioEffectDescriptor)
		);
		FAudio_memcpy(
			(*ppSourceVoice)->effects.pEffectDescriptors,
			pEffectChain->pEffectDescriptors,
			pEffectChain->EffectCount * sizeof(FAudioEffectDescriptor)
		);
	}

	/* Source Properties */
	(*ppSourceVoice)->src.maxFreqRatio = MaxFrequencyRatio;
	FAudio_memcpy(
		&(*ppSourceVoice)->src.format,
		pSourceFormat,
		sizeof(FAudioWaveFormatEx)
	);
	(*ppSourceVoice)->src.callback = pCallback;
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
	*ppSubmixVoice = FAudio_malloc(sizeof(FAudioVoice));
	(*ppSubmixVoice)->type = FAUDIO_VOICE_SUBMIX;

	/* Sends/Effects */
	if (pSendList == NULL)
	{
		FAudio_zero(&(*ppSubmixVoice)->sends, sizeof(FAudioVoiceSends));
	}
	else
	{
		(*ppSubmixVoice)->sends.SendCount = pSendList->SendCount;
		(*ppSubmixVoice)->sends.pSends = (FAudioSendDescriptor*) FAudio_malloc(
			pSendList->SendCount * sizeof(FAudioSendDescriptor)
		);
		FAudio_memcpy(
			(*ppSubmixVoice)->sends.pSends,
			pSendList->pSends,
			pSendList->SendCount * sizeof(FAudioSendDescriptor)
		);
	}
	if (pEffectChain == NULL)
	{
		FAudio_zero(&(*ppSubmixVoice)->effects, sizeof(FAudioEffectChain));
	}
	else
	{
		(*ppSubmixVoice)->effects.EffectCount = pEffectChain->EffectCount;
		(*ppSubmixVoice)->effects.pEffectDescriptors = (FAudioEffectDescriptor*) FAudio_malloc(
			pEffectChain->EffectCount * sizeof(FAudioEffectDescriptor)
		);
		FAudio_memcpy(
			(*ppSubmixVoice)->effects.pEffectDescriptors,
			pEffectChain->pEffectDescriptors,
			pEffectChain->EffectCount * sizeof(FAudioEffectDescriptor)
		);
	}

	/* Submix Properties */
	(*ppSubmixVoice)->mix.inputChannels = InputChannels;
	(*ppSubmixVoice)->mix.inputSampleRate = InputSampleRate;
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
	*ppMasteringVoice = FAudio_malloc(sizeof(FAudioVoice));
	(*ppMasteringVoice)->type = FAUDIO_VOICE_MASTER;

	/* Sends/Effects */
	FAudio_zero(&(*ppMasteringVoice)->sends, sizeof(FAudioVoiceSends));
	if (pEffectChain == NULL)
	{
		FAudio_zero(&(*ppMasteringVoice)->effects, sizeof(FAudioEffectChain));
	}
	else
	{
		(*ppMasteringVoice)->effects.EffectCount = pEffectChain->EffectCount;
		(*ppMasteringVoice)->effects.pEffectDescriptors = (FAudioEffectDescriptor*) FAudio_malloc(
			pEffectChain->EffectCount * sizeof(FAudioEffectDescriptor)
		);
		FAudio_memcpy(
			(*ppMasteringVoice)->effects.pEffectDescriptors,
			pEffectChain->pEffectDescriptors,
			pEffectChain->EffectCount * sizeof(FAudioEffectDescriptor)
		);
	}

	/* Master Properties */
	(*ppMasteringVoice)->master.inputChannels = InputChannels;
	(*ppMasteringVoice)->master.inputSampleRate = InputSampleRate;
	(*ppMasteringVoice)->master.deviceIndex = DeviceIndex;
	return 0;
}

uint32_t FAudio_StartEngine(FAudio *audio)
{
	/* TODO */
	return 0;
}

void FAudio_StopEngine(FAudio *audio)
{
	/* TODO */
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
	FAudioVoiceSends *pSendList
) {
	/* TODO */
	return 0;
}

uint32_t FAudioVoice_SetEffectChain(
	FAudioVoice *voice,
	FAudioEffectChain *pEffectChain
) {
	/* TODO */
	return 0;
}

uint32_t FAudioVoice_EnableEffect(
	FAudioVoice *voice,
	uint32_t EffectIndex,
	uint32_t OperationSet
) {
	/* TODO */
	FAudio_assert(OperationSet == 0);
	return 0;
}

uint32_t FAudioVoice_DisableEffect(
	FAudioVoice *voice,
	uint32_t EffectIndex,
	uint32_t OperationSet
) {
	/* TODO */
	FAudio_assert(OperationSet == 0);
	return 0;
}

void FAudioVoice_GetEffectState(
	FAudioVoice *voice,
	uint32_t EffectIndex,
	uint8_t *pEnabled
) {
	/* TODO */
}

uint32_t FAudioVoice_SetEffectParameters(
	FAudioVoice *voice,
	uint32_t EffectIndex,
	const void *pParameters,
	uint32_t ParametersByteSize,
	uint32_t OperationSet
) {
	/* TODO */
	FAudio_assert(OperationSet == 0);
	return 0;
}

uint32_t FAudioVoice_GetEffectParameters(
	FAudioVoice *voice,
	void *pParameters,
	uint32_t ParametersByteSize
) {
	/* TODO */
	return 0;
}

uint32_t FAudioVoice_SetFilterParameters(
	FAudioVoice *voice,
	const FAudioFilterParameters *pParameters,
	uint32_t OperationSet
) {
	/* TODO */
	FAudio_assert(OperationSet == 0);
	return 0;
}

void FAudioVoice_GetFilterParameters(
	FAudioVoice *voice,
	FAudioFilterParameters *pParameters
) {
	/* TODO */
}

uint32_t FAudioVoice_SetOutputFilterParameters(
	FAudioVoice *voice,
	FAudioVoice *pDestinationVoice,
	const FAudioFilterParameters *pParameters,
	uint32_t OperationSet
) {
	/* TODO */
	FAudio_assert(OperationSet == 0);
	return 0;
}

void FAudioVoice_GetOutputFilterParameters(
	FAudioVoice *voice,
	FAudioVoice *pDestinationVoice,
	FAudioFilterParameters *pParameters
) {
	/* TODO */
}

uint32_t FAudioVoice_SetVolume(
	FAudioVoice *voice,
	float Volume,
	uint32_t OperationSet
) {
	/* TODO */
	FAudio_assert(OperationSet == 0);
	return 0;
}

void FAudioVoice_GetVolume(
	FAudioVoice *voice,
	float *pVolume
) {
	/* TODO */
}

uint32_t FAudioVoice_SetChannelVolumes(
	FAudioVoice *voice,
	uint32_t Channels,
	const float *pVolumes,
	uint32_t OperationSet
) {
	/* TODO */
	FAudio_assert(OperationSet == 0);
	return 0;
}

void FAudioVoice_GetChannelVolumes(
	FAudioVoice *voice,
	uint32_t Channels,
	float *pVolumes
) {
	/* TODO */
}

uint32_t FAudioVoice_SetOutputMatrix(
	FAudioVoice *voice,
	FAudioVoice *pDestinationVoice,
	uint32_t SourceChannels,
	uint32_t DestinationChannels,
	const float *pLevelMatrix,
	uint32_t OperationSet
) {
	/* TODO */
	FAudio_assert(OperationSet == 0);
	return 0;
}

void FAudioVoice_GetOutputMatrix(
	FAudioVoice *voice,
	FAudioVoice *pDestinationVoice,
	uint32_t SourceChannels,
	uint32_t DestinationChannels,
	float *pLevelMatrix
) {
	/* TODO */
}

void FAudioVoice_DestroyVoice(FAudioVoice *voice)
{
	/* TODO: Lock, check for dependencies and fail if still in use */
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
	/* TODO */
	FAudio_assert(OperationSet == 0);
	return 0;
}

uint32_t FAudioSourceVoice_Stop(
	FAudioSourceVoice *voice,
	uint32_t Flags,
	uint32_t OperationSet
) {
	/* TODO */
	FAudio_assert(OperationSet == 0);
	return 0;
}

uint32_t FAudioSourceVoice_SubmitSourceBuffer(
	FAudioSourceVoice *voice,
	const FAudioBuffer *pBuffer,
	const FAudioBufferWMA *pBufferWMA
) {
	/* TODO */
	return 0;
}

uint32_t FAudioSourceVoice_FlushSourceBuffers(
	FAudioSourceVoice *voice
) {
	/* TODO */
	return 0;
}

uint32_t FAudioSourceVoice_Discontinuity(
	FAudioSourceVoice *voice
) {
	/* TODO */
	return 0;
}

uint32_t FAudioSourceVoice_ExitLoop(
	FAudioSourceVoice *voice,
	uint32_t OperationSet
) {
	/* TODO */
	FAudio_assert(OperationSet == 0);
	return 0;
}

void FAudioSourceVoice_GetState(
	FAudioSourceVoice *voice,
	FAudioVoiceState *pVoiceState
) {
	/* TODO */
}

uint32_t FAudioSourceVoice_SetFrequencyRatio(
	FAudioSourceVoice *voice,
	float Ratio,
	uint32_t OperationSet
) {
	/* TODO */
	FAudio_assert(OperationSet == 0);
	return 0;
}

void FAudioSourceVoice_GetFrequencyRatio(
	FAudioSourceVoice *voice,
	float *pRatio
) {
	/* TODO */
}

uint32_t FAudioSourceVoice_SetSourceSampleRate(
	FAudioSourceVoice *voice,
	uint32_t NewSourceSampleRate
) {
	/* TODO */
	return 0;
}
