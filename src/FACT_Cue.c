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
	FACT_assert(0 && "Variable name not found!");
	return 0;
}

uint32_t FACTCue_SetVariable(
	FACTCue *pCue,
	uint16_t nIndex,
	float nValue
) {
	FACTVariable *var = &pCue->parentBank->parentEngine->variables[nIndex];
	FACT_assert(var->accessibility & 0x01);
	FACT_assert(!(var->accessibility & 0x02));
	FACT_assert(!(var->accessibility & 0x04));
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
	FACT_assert(var->accessibility & 0x01);
	FACT_assert(!(var->accessibility & 0x04));
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
	if (!(pCue->data->flags & 0x04))
	{
		/* Nothing active? Zero, bail. */
		if (pCue->active.variation == NULL)
		{
			FACT_zero(
				varProps,
				sizeof(FACTVariationProperties)
			);
			FACT_zero(
				sndProps,
				sizeof(FACTSoundProperties)
			);
		}
		else
		{
			/* TODO:
			 * - index?
			 * - weight is just the max - min?
			 * - linger?
			 */
			varProps->index = 0;
			varProps->weight = (
				pCue->active.variation->maxWeight -
				pCue->active.variation->minWeight
			);
			if (pCue->sound.variation->flags == 3)
			{
				varProps->iaVariableMin =
					pCue->active.variation->minWeight;
				varProps->iaVariableMax =
					pCue->active.variation->maxWeight;
			}
			else
			{
				varProps->iaVariableMin = 0;
				varProps->iaVariableMax = 0;
			}
			varProps->linger = 0;

			/* No Sound, no SoundProperties */
			if (!pCue->active.variation->isComplex)
			{
				FACT_zero(
					sndProps,
					sizeof(FACTSoundProperties)
				);
			}
			else
			{
				/* TODO:
				 * - u8->float volume conversion crap
				 * - "Track" vs. "Clip"?
				 * - arrTrackProperties?
				 */
				sndProps->category = pCue->active.sound->category;
				sndProps->priority = pCue->active.sound->priority;
				sndProps->pitch = pCue->active.sound->pitch;
				sndProps->volume = pCue->active.sound->volume;
				sndProps->numTracks = pCue->active.sound->clipCount;
				/* arrTrackProperties[0] */
			}
		}
	}
	else
	{
		/* No variations here! */
		FACT_zero(
			varProps,
			sizeof(FACTVariationProperties)
		);

		/* Nothing active? Zero, bail. */
		if (pCue->active.sound == NULL)
		{
			FACT_zero(
				sndProps,
				sizeof(FACTSoundProperties)
			);
		}
		else
		{
			/* TODO:
			 * - u8->float volume conversion crap
			 * - "Track" vs. "Clip"?
			 * - arrTrackProperties?
			 */
			sndProps->category = pCue->active.sound->category;
			sndProps->priority = pCue->active.sound->priority;
			sndProps->pitch = pCue->active.sound->pitch;
			sndProps->volume = pCue->active.sound->volume;
			sndProps->numTracks = pCue->active.sound->clipCount;
			/* arrTrackProperties[0] */
		}
	}

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
