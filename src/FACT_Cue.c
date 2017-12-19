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
	union
	{
		float maxf;
		uint8_t maxi;
	} max;
	FACTCue *tmp, *wnr;
	uint16_t categoryIndex;
	FACTAudioCategory *category;
	FACTCueData *data = &pCue->parentBank->cues[pCue->index];

	FACT_assert(!(pCue->state & (FACT_STATE_PLAYING | FACT_STATE_STOPPING)));

	/* Need an initial sound to play */
	FACT_INTERNAL_SelectSound(pCue);

	/* Instance Limits */
	#define INSTANCE_BEHAVIOR(obj, match) \
		wnr = NULL; \
		tmp = pCue->parentBank->cueList; \
		if (obj->maxInstanceBehavior == 0) /* Fail */ \
		{ \
			return 1; \
		} \
		else if (obj->maxInstanceBehavior == 1) /* Queue */ \
		{ \
			/* FIXME: How is this different from Replace Oldest? */ \
			while (tmp != NULL) \
			{ \
				if (match) \
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
				if (match) \
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
				if (	match /*&&*/ \
					/*FIXME: tmp->soundInstance->volume < max.maxf*/) \
				{ \
					wnr = tmp; \
					/* max.maxf = tmp->soundInstance->volume; */ \
				} \
				tmp = tmp->next; \
			} \
		} \
		else if (obj->maxInstanceBehavior == 4) /* Replace Lowest Priority */ \
		{ \
			max.maxi = 0xFF; \
			while (tmp != NULL) \
			{ \
				if (	match && \
					tmp->soundInstance.sound->priority < max.maxi	) \
				{ \
					wnr = tmp; \
					max.maxi = tmp->soundInstance.sound->priority; \
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
			if (obj->fadeOutMS > 0) \
			{ \
				FACT_INTERNAL_BeginFadeOut(wnr); \
			} \
			else \
			{ \
				FACTCue_Stop(wnr, 0); \
			} \
		}
	if (data->instanceCount >= data->instanceLimit)
	{
		INSTANCE_BEHAVIOR(
			data,
			tmp->index == pCue->index
		)
	}
	else if (pCue->soundInstance.exists)
	{
		categoryIndex = pCue->soundInstance.sound->category;
		category = &pCue->parentBank->parentEngine->categories[categoryIndex];
		if (category->instanceCount >= category->instanceLimit)
		{
			INSTANCE_BEHAVIOR(
				category,
				tmp->soundInstance.sound->category == categoryIndex
			)
		}
	}
	#undef INSTANCE_BEHAVIOR

	pCue->state |= FACT_STATE_PLAYING;
	pCue->state &= ~(
		FACT_STATE_PAUSED |
		FACT_STATE_STOPPED
	);
	pCue->start = FACT_timems();
	return 0;
}

uint32_t FACTCue_Stop(FACTCue *pCue, uint32_t dwFlags)
{
	/* There are two ways that a Cue might be stopped immediately:
	 * 1. The program explicitly asks for it
	 * 2. The Cue is paused and therefore we can't do fade/release effects
	 */
	if (	dwFlags & FACT_FLAG_STOP_IMMEDIATE ||
		pCue->state & FACT_STATE_PAUSED	)
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
		pCue->soundInstance.exists = 0;
		FACT_free(pCue->soundInstance.tracks);
	}
	else
	{
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
	return FACTVARIABLEINDEX_INVALID;
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
	/* "A stopping or stopped cue cannot be paused." */
	if (pCue->state & (FACT_STATE_STOPPING | FACT_STATE_STOPPED))
	{
		return 0;
	}

	/* Store elapsed time */
	pCue->elapsed += FACT_timems() - pCue->start;

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
			varProps->index = 0; /* TODO: Index of what...? */
			/* TODO: This is just max - min right? */
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
			varProps->linger = pCue->active.variation->linger;

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
				/* TODO: u8->float volume conversion crap */
				sndProps->category = pCue->active.sound->category;
				sndProps->priority = pCue->active.sound->priority;
				sndProps->pitch = pCue->active.sound->pitch;
				sndProps->volume = pCue->active.sound->volume;
				sndProps->numTracks = pCue->active.sound->trackCount;
				/* TODO: arrTrackProperties[0] */
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
			/* TODO: u8->float volume conversion crap */
			sndProps->category = pCue->active.sound->category;
			sndProps->priority = pCue->active.sound->priority;
			sndProps->pitch = pCue->active.sound->pitch;
			sndProps->volume = pCue->active.sound->volume;
			sndProps->numTracks = pCue->active.sound->trackCount;
			/* TODO: arrTrackProperties[0] */
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
