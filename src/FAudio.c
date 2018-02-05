/* FACT - XACT Reimplementation for FNA
 * Copyright 2009-2018 Ethan Lee and the MonoGame Team
 *
 * Released under the Microsoft Public License.
 * See LICENSE for details.
 */

#include "FAudio_internal.h"

/* FAudio Interface */

uint32_t FAudioCreate(FAudio **ppFAudio)
{
	*ppFAudio = (FAudio*) FAudio_malloc(sizeof(FAudio));
	FAudio_zero(*ppFAudio, sizeof(FAudio));
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
	/* TODO */
	return 0;
}

uint32_t FAudio_RegisterForCallbacks(
	FAudio *audio,
	FAudioEngineCallback *pCallback
) {
	/* TODO */
	return 0;
}

void FAudio_UnregisterForCallbacks(
	FAudio *audio,
	FAudioEngineCallback *pCallback
) {
	/* TODO */
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
	/* TODO */
	return 0;
}

uint32_t FAudio_CreateSubmixVoice(
	FAudio *audio,
	uint32_t InputChannels,
	uint32_t InputSampleRate,
	uint32_t Flags,
	uint32_t ProcessingStage,
	const FAudioVoiceSends *pSendList,
	const FAudioEffectChain *pEffectChain
) {
	/* TODO */
	return 0;
}

uint32_t FAudio_CreateMasteringVoice(
	FAudio *audio,
	uint32_t InputChannels,
	uint32_t InputSampleRate,
	uint32_t Flags,
	uint32_t DeviceIndex,
	const FAudioEffectChain *pEffectChain
) {
	/* TODO */
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
	/* TODO */
	return 0;
}

void FAudio_GetPerformanceData(
	FAudio *audio,
	FAudioPerformanceData *pPerfData
) {
	/* TODO */
}

void FAudio_SetDebugConfiguration(
	FAudio *audio,
	FAudioDebugConfiguration *pDebugConfiguration,
	void* pReserved
) {
	/* TODO */
}

/* FAudioVoice Interface */

void FAudioVoice_GetVoiceDetails(
	FAudioVoice *voice,
	FAudioVoiceDetails *pVoiceDetails
) {
	/* TODO */
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
	return 0;
}

uint32_t FAudioVoice_DisableEffect(
	FAudioVoice *voice,
	uint32_t EffectIndex,
	uint32_t OperationSet
) {
	/* TODO */
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
	/* TODO */
}

/* FAudioSourceVoice Interface */

uint32_t FAudioSourceVoice_Start(
	FAudioSourceVoice *voice,
	uint32_t Flags,
	uint32_t OperationSet
) {
	/* TODO */
	return 0;
}

uint32_t FAudioSourceVoice_Stop(
	FAudioSourceVoice *voice,
	uint32_t Flags,
	uint32_t OperationSet
) {
	/* TODO */
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
