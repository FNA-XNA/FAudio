/* FACT - XACT Reimplementation for FNA
 * Copyright 2009-2017 Ethan Lee and the MonoGame Team
 *
 * Released under the Microsoft Public License.
 * See LICENSE for details.
 */

#include "FACT_internal.h"

uint32_t FACTCue_Destroy(FACTCue *pCue)
{
	return 0;
}

uint32_t FACTCue_Play(FACTCue *pCue)
{
	return 0;
}

uint32_t FACTCue_Stop(FACTCue *pCue, uint32_t dwFlags)
{
	return 0;
}

uint32_t FACTCue_GetState(FACTCue *pCue, uint32_t *pdwState)
{
	return 0;
}

uint32_t FACTCue_SetMatrixCoefficients(
	FACTCue *pCue,
	uint32_t uSrcChannelCount,
	uint32_t uDstChannelCount,
	float *pMatrixCoefficients
) {
	return 0;
}

uint16_t FACTCue_GetVariableIndex(
	FACTCue *pCue,
	const char *szFriendlyName
) {
	return 0;
}

uint32_t FACTCue_SetVariable(
	FACTCue *pCue,
	uint16_t nIndex,
	float nValue
) {
	return 0;
}

uint32_t FACTCue_GetVariable(
	FACTCue *pCue,
	uint16_t nIndex,
	float *nValue
) {
	return 0;
}

uint32_t FACTCue_Pause(FACTCue *pCue, int32_t fPause)
{
	return 0;
}

uint32_t FACTCue_GetProperties(
	FACTCue *pCue,
	FACTCueInstanceProperties *ppProperties
) {
	return 0;
}

uint32_t FACTCue_SetOutputVoices(
	FACTCue *pCue,
	const void *pSendList /* Optional XAUDIO2_VOICE_SENDS */
) {
	return 0;
}

uint32_t FACTCue_SetOutputVoiceMatrix(
	FACTCue *pCue,
	const void *pDestinationVoice, /* Optional IXAudio2Voice */
	uint32_t SourceChannels,
	uint32_t DestinationChannels,
	const float *pLevelMatrix /* SourceChannels * DestinationChannels */
) {
	return 0;
}
