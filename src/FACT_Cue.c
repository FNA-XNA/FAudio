/* FACT - XACT Reimplementation for FNA
 * Copyright 2009-2018 Ethan Lee and the MonoGame Team
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
					/*FIXME: tmp->playing.sound.volume < max.maxf*/) \
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
				if (	match && \
					tmp->playing.sound.sound->priority < max.maxi	) \
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
	}
	#undef INSTANCE_BEHAVIOR

	pCue->state |= FACT_STATE_PLAYING;
	pCue->state &= ~(
		FACT_STATE_PAUSED |
		FACT_STATE_STOPPED
	);
	pCue->start = FACT_timems();

	/* If it's a simple wave, just play it! */
	if (pCue->active & 0x01)
	{
		/* TODO: Apply3D */
		FACTWave_Play(pCue->playing.wave);
	}

	return 0;
}

uint32_t FACTCue_Stop(FACTCue *pCue, uint32_t dwFlags)
{
	uint8_t i;

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
		if (pCue->active & 0x01)
		{
			FACTWave_Destroy(pCue->playing.wave);
		}
		else if (pCue->active & 0x02)
		{
			for (i = 0; i < pCue->playing.sound.sound->trackCount; i += 1)
			{
				FACT_free(pCue->playing.sound.tracks[i].events);
			}
			FACT_free(pCue->playing.sound.tracks);
		}
		pCue->active = 0;
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
	/* TODO: Internal storage...? */
	FACTEvent *evt;
	FACTWave *wave;
	uint8_t i, j;
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
		for (j = 0; j < pCue->playing.sound.sound->tracks[i].eventCount; j += 1)
		{
			evt = &pCue->playing.sound.sound->tracks[i].events[j];
			if (	evt->type == FACTEVENT_PLAYWAVE ||
				evt->type == FACTEVENT_PLAYWAVETRACKVARIATION ||
				evt->type == FACTEVENT_PLAYWAVEEFFECTVARIATION ||
				evt->type == FACTEVENT_PLAYWAVETRACKEFFECTVARIATION	)
			{
				wave = pCue->playing.sound.tracks[i].events[j].data.wave.wave;
				if (wave != NULL)
				{
					FACTWave_SetMatrixCoefficients(
						wave,
						uSrcChannelCount,
						uDstChannelCount,
						pMatrixCoefficients
					);
				}
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
		if (	FACT_strcmp(szFriendlyName, pCue->parentBank->parentEngine->variableNames[i]) == 0 &&
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
	FACT_assert(var->accessibility & 0x01);
	FACT_assert(!(var->accessibility & 0x02));
	FACT_assert(var->accessibility & 0x04);
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
	FACT_assert(var->accessibility & 0x04);

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

	/* ... unless it's a plain Wave */
	if (pCue->active & 0x01)
	{
		FACTWave_Pause(pCue->playing.wave, fPause);
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
		/* TODO: This is just max - min right? */
		varProps->weight = (
			pCue->playingVariation->maxWeight -
			pCue->playingVariation->minWeight
		);
		if (pCue->sound.variation->flags == 3)
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
		FACT_zero(
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
		FACT_zero(
			sndProps,
			sizeof(FACTSoundProperties)
		);
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
