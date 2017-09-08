/* FACT - XACT Reimplementation for FNA
 * Copyright 2009-2017 Ethan Lee and the MonoGame Team
 *
 * Released under the Microsoft Public License.
 * See LICENSE for details.
 */

#include "FACT_internal.h"

uint32_t FACTCue_Destroy(FACTCue *pCue)
{
	FACTCue_Stop(pCue, FACT_FLAG_STOP_IMMEDIATE);
	FACT_free(pCue->variableValues);
	pCue->state = 0; /* '0' is used to detect destroyed Cues */
	return 0;
}

uint32_t FACTCue_Play(FACTCue *pCue)
{
	/* TODO: Init Sound state */
	pCue->state |= FACT_STATE_PLAYING;
	/* FIXME: How do we deal with STOPPING? -flibit */
	pCue->state &= ~(
		FACT_STATE_PAUSED |
		FACT_STATE_STOPPED
	);
	return 0;
}

uint32_t FACTCue_Stop(FACTCue *pCue, uint32_t dwFlags)
{
	if (dwFlags & FACT_FLAG_STOP_IMMEDIATE)
	{
		pCue->state |= FACT_STATE_STOPPED;
		pCue->state &= ~(
			FACT_STATE_PLAYING |
			FACT_STATE_STOPPING |
			FACT_STATE_PAUSED
		);
		/* TODO: Reset Sound state */
	}
	else
	{
		/* FIXME: How do we deal with PAUSED? -flibit */
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
	/* TODO */
	return 0;
}

uint16_t FACTCue_GetVariableIndex(
	FACTCue *pCue,
	const char *szFriendlyName
) {
	uint16_t i;
	for (i = 0; i < pCue->parentBank->parentEngine->variableCount; i += 1)
	{
		if (	FACT_strcmp(szFriendlyName, pCue->parentBank->parentEngine->variableNames[i]) == 0 &&
			!(pCue->parentBank->parentEngine->variables[i].accessibility & 0x04)	)
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
	FACTVariable *var = &pCue->parentBank->parentEngine->variables[nIndex];
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
	float *nValue
) {
	FACTVariable *var = &pCue->parentBank->parentEngine->variables[nIndex];
	assert(var->accessibility & 0x01);
	assert(!(var->accessibility & 0x04));
	*nValue = pCue->variableValues[nIndex];
	return 0;
}

uint32_t FACTCue_Pause(FACTCue *pCue, int32_t fPause)
{
	/* "A stopping or stopped cue cannot be paused." */
	if (pCue->state & (FACT_STATE_STOPPING | FACT_STATE_STOPPED))
	{
		return 0;
	}

	/* All we do is set the flag, the mixer handles the rest */
	if (fPause)
	{
		pCue->state |= FACT_STATE_PAUSED;
	}
	else
	{
		pCue->state &= ~FACT_STATE_PAUSED;
	}
	return 0;
}

uint32_t FACTCue_GetProperties(
	FACTCue *pCue,
	FACTCueInstanceProperties *ppProperties
) {
	ppProperties->allocAttributes = 0;
	FACTSoundBank_GetCueProperties(
		pCue->parentBank,
		pCue->index,
		&ppProperties->cueProperties
	);
	/* TODO: activeVariationProperties */
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
