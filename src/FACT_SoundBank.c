/* FACT - XACT Reimplementation for FNA
 * Copyright 2009-2017 Ethan Lee and the MonoGame Team
 *
 * Released under the Microsoft Public License.
 * See LICENSE for details.
 */

#include "FACT_internal.h"

uint16_t FACTSoundBank_GetCueIndex(
	FACTSoundBank *pSoundBank,
	const char *szFriendlyName
) {
	uint16_t i;
	for (i = 0; i < pSoundBank->cueCount; i += 1)
	{
		if (FACT_strcmp(szFriendlyName, pSoundBank->cueNames[i]) == 0)
		{
			return i;
		}
	}

	FACT_assert(0 && "Cue name not found!");
	return 0;
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
	FACT_strlcpy(
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

		FACT_assert(i < pSoundBank->variationCount && "Variation table not found!");

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

	*ppCue = (FACTCue*) FACT_malloc(sizeof(FACTCue));
	FACT_zero(*ppCue, sizeof(FACTCue));

	/* Engine references */
	(*ppCue)->parentBank = pSoundBank;
	(*ppCue)->next = NULL;
	(*ppCue)->managed = 0;
	(*ppCue)->index = nCueIndex;

	/* Sound data */
	(*ppCue)->data = &pSoundBank->cues[nCueIndex];
	if (!((*ppCue)->data->flags & 0x04))
	{
		for (i = 0; i < pSoundBank->variationCount; i += 1)
		{
			if ((*ppCue)->data->sbCode == pSoundBank->variationCodes[i])
			{
				(*ppCue)->sound.variation = &pSoundBank->variations[i];
			}
		}
		(*ppCue)->active.variation = NULL;
	}
	else
	{
		for (i = 0; i < pSoundBank->soundCount; i += 1)
		{
			if ((*ppCue)->data->sbCode == pSoundBank->soundCodes[i])
			{
				(*ppCue)->sound.sound = &pSoundBank->sounds[i];
			}
		}
		(*ppCue)->active.sound = NULL;
	}

	/* Instance data */
	(*ppCue)->variableValues = (float*) FACT_malloc(
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
	FACTCue_Play(result);
	if (ppCue != NULL)
	{
		*ppCue = result;
	}
	else
	{
		/* AKA we get to Destroy() this ourselves */
		result->managed = 1;
	}
	return 0;
}

uint32_t FACTSoundBank_Stop(
	FACTSoundBank *pSoundBank,
	uint16_t nCueIndex,
	uint32_t dwFlags
) {
	FACTCue *cue = pSoundBank->cueList;
	while (cue != NULL)
	{
		if (cue->index == nCueIndex)
		{
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
		}
		cue = cue->next;
	}
	return 0;
}

uint32_t FACTSoundBank_Destroy(FACTSoundBank *pSoundBank)
{
	uint16_t i, j, k;
	FACTSoundBank *prev, *sb = pSoundBank;

	/* SoundBank Name */
	FACT_free(sb->name);

	/* Cue data */
	FACT_free(sb->cues);

	/* WaveBank Name data */
	for (i = 0; i < sb->wavebankCount; i += 1)
	{
		FACT_free(sb->wavebankNames[i]);
	}
	FACT_free(sb->wavebankNames);

	/* Sound data */
	for (i = 0; i < sb->soundCount; i += 1)
	{
		for (j = 0; j < sb->sounds[i].trackCount; j += 1)
		{
			for (k = 0; k < sb->sounds[i].tracks[j].eventCount; k += 1)
			{
				#define MATCH(t) \
					sb->sounds[i].tracks[j].events[k].type == t
				if (	MATCH(FACTEVENT_PLAYWAVE) ||
					MATCH(FACTEVENT_PLAYWAVETRACKVARIATION) ||
					MATCH(FACTEVENT_PLAYWAVEEFFECTVARIATION) ||
					MATCH(FACTEVENT_PLAYWAVETRACKEFFECTVARIATION)	)
				{
					if (sb->sounds[i].tracks[j].events[k].wave.isComplex)
					{
						FACT_free(
							sb->sounds[i].tracks[j].events[k].wave.complex.tracks
						);
						FACT_free(
							sb->sounds[i].tracks[j].events[k].wave.complex.wavebanks
						);
						FACT_free(
							sb->sounds[i].tracks[j].events[k].wave.complex.weights
						);
					}
				}
				#undef MATCH
			}
			FACT_free(sb->sounds[i].tracks[j].events);
		}
		FACT_free(sb->sounds[i].tracks);
		FACT_free(sb->sounds[i].rpcCodes);
		FACT_free(sb->sounds[i].dspCodes);
	}
	FACT_free(sb->sounds);
	FACT_free(sb->soundCodes);

	/* Variation data */
	for (i = 0; i < sb->variationCount; i += 1)
	{
		FACT_free(sb->variations[i].entries);
	}
	FACT_free(sb->variations);
	FACT_free(sb->variationCodes);

	/* Cue Name data */
	for (i = 0; i < sb->cueCount; i += 1)
	{
		FACT_free(sb->cueNames[i]);
	}
	FACT_free(sb->cueNames);

	/* Remove this SoundBank from the Engine list */
	sb = pSoundBank->parentEngine->sbList;
	prev = sb;
	while (sb != NULL)
	{
		if (sb == pSoundBank)
		{
			if (sb == prev) /* First in list */
			{
				pSoundBank->parentEngine->sbList = sb->next;
			}
			else
			{
				prev->next = sb->next;
			}
			break;
		}
		prev = sb;
		sb = sb->next;
	}
	FACT_assert(sb != NULL && "Could not find SoundBank reference!");

	/* Finally. */
	FACT_free(sb);
	return 0;
}

uint32_t FACTSoundBank_GetState(
	FACTSoundBank *pSoundBank,
	uint32_t *pdwState
) {
	/* FIXME: Is there more to this than just checking INUSE? */
	uint16_t i;
	for (i = 0; i < pSoundBank->cueCount; i += 1)
	{
		if (pSoundBank->cues[i].instanceCount > 0)
		{
			return FACT_STATE_INUSE;
		}
	}
	return 0;
}

