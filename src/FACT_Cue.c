/* FACT - XACT Reimplementation for FNA
 * Copyright 2009-2017 Ethan Lee and the MonoGame Team
 *
 * Released under the Microsoft Public License.
 * See LICENSE for details.
 */

#include "FACT_internal.h"

uint32_t FACTCue_Destroy(FACTCue *pCue)
{
	/* TODO */
	return 0;
}

uint32_t FACTCue_Play(FACTCue *pCue)
{
	/* TODO */
	return 0;
}

uint32_t FACTCue_Stop(FACTCue *pCue, uint32_t dwFlags)
{
	/* TODO */
	return 0;
}

uint32_t FACTCue_GetState(FACTCue *pCue, uint32_t *pdwState)
{
	/* TODO */
	return 0;
}

uint32_t FACTCue_SetMatrixCoefficients(
	FACTCue *pCue,
	uint32_t uSrcChannelCount,
	uint32_t uDstChannelCount,
	float *pMatrixCoefficients
) {
	/* TODO */
	return 0;
}

uint16_t FACTCue_GetVariableIndex(
	FACTCue *pCue,
	const char *szFriendlyName
) {
	uint16_t i;
	for (i = 0; i < pCue->parentEngine->variableCount; i += 1)
	{
		if (	FACT_strcmp(szFriendlyName, pCue->parentEngine->variableNames[i]) == 0 &&
			!(pCue->parentEngine->variables[i].accessibility & 0x04)	)
		{
			return i;
		}
	}
	assert(0 && "Variable name not found!");
	return 0;
}

uint32_t FACTCue_SetVariable(
	FACTCue *pCue,
	uint16_t nIndex,
	float nValue
) {
	FACTVariable *var = &pCue->parentEngine->variables[nIndex];
	assert(var->accessibility & 0x01);
	assert(!(var->accessibility & 0x02));
	assert(!(var->accessibility & 0x04));
	pCue->variableValues[nIndex] = FACT_clamp(
		nValue,
		var->minValue,
		var->maxValue
	);
	return 0;
}

uint32_t FACTCue_GetVariable(
	FACTCue *pCue,
	uint16_t nIndex,
	float *pnValue
) {
	FACTVariable *var = &pCue->parentEngine->variables[nIndex];
	assert(var->accessibility & 0x01);
	assert(!(var->accessibility & 0x04));
	*pnValue = pCue->variableValues[nIndex];
	return 0;
}

uint32_t FACTCue_Pause(FACTCue *pCue, int32_t fPause)
{
	/* TODO */
	return 0;
}

uint32_t FACTCue_GetProperties(
	FACTCue *pCue,
	FACTCueInstanceProperties *ppProperties
) {
	/* TODO */
	return 0;
}

uint32_t FACTCue_SetOutputVoices(
	FACTCue *pCue,
	const void *pSendList /* Optional XAUDIO2_VOICE_SENDS */
) {
	/* TODO */
	return 0;
}

uint32_t FACTCue_SetOutputVoiceMatrix(
	FACTCue *pCue,
	const void *pDestinationVoice, /* Optional IXAudio2Voice */
	uint32_t SourceChannels,
	uint32_t DestinationChannels,
	const float *pLevelMatrix /* SourceChannels * DestinationChannels */
) {
	/* TODO */
	return 0;
}
