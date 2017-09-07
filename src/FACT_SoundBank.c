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

	assert(0 && "Cue name not found!");
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

		assert(i < pSoundBank->variationCount && "Variation table not found!");

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
	/* TODO */
	return 0;
}

uint32_t FACTSoundBank_Play(
	FACTSoundBank *pSoundBank,
	uint16_t nCueIndex,
	uint32_t dwFlags,
	int32_t timeOffset,
	FACTCue** ppCue /* Optional! */
) {
	/* TODO */
	return 0;
}

uint32_t FACTSoundBank_Stop(
	FACTSoundBank *pSoundBank,
	uint16_t nCueIndex,
	uint32_t dwFlags
) {
	/* TODO */
	return 0;
}

uint32_t FACTSoundBank_Destroy(FACTSoundBank *pSoundBank)
{
	uint16_t i, j, k;
	FACTSoundBank *sb = pSoundBank;

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
		for (j = 0; j < sb->sounds[i].clipCount; j += 1)
		{
			for (k = 0; k < sb->sounds[i].clips[j].eventCount; k += 1)
			{
				#define MATCH(t) \
					sb->sounds[i].clips[j].events[k].type == t
				if (	MATCH(FACTEVENT_PLAYWAVE) ||
					MATCH(FACTEVENT_PLAYWAVETRACKVARIATION) ||
					MATCH(FACTEVENT_PLAYWAVEEFFECTVARIATION) ||
					MATCH(FACTEVENT_PLAYWAVETRACKEFFECTVARIATION)	)
				{
					if (sb->sounds[i].clips[j].events[k].wave.isComplex)
					{
						FACT_free(
							sb->sounds[i].clips[j].events[k].wave.complex.tracks
						);
						FACT_free(
							sb->sounds[i].clips[j].events[k].wave.complex.wavebanks
						);
						FACT_free(
							sb->sounds[i].clips[j].events[k].wave.complex.weights
						);
					}
				}
				#undef MATCH
			}
			FACT_free(sb->sounds[i].clips[j].events);
		}
		FACT_free(sb->sounds[i].clips);
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

