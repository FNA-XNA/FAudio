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
	return 0;
}

uint32_t FACTSoundBank_GetNumCues(
	FACTSoundBank *pSoundBank,
	uint16_t *pnNumCues
) {
	return 0;
}

uint32_t FACTSoundBank_GetCueProperties(
	FACTSoundBank *pSoundBank,
	uint16_t nCueIndex,
	FACTCueProperties *pProperties
) {
	return 0;
}

uint32_t FACTSoundBank_Prepare(
	FACTSoundBank *pSoundBank,
	uint16_t nCueIndex,
	uint32_t dwFlags,
	int32_t timeOffset,
	FACTCue** ppCue
) {
	return 0;
}

uint32_t FACTSoundBank_Play(
	FACTSoundBank *pSoundBank,
	uint16_t nCueIndex,
	uint32_t dwFlags,
	int32_t timeOffset,
	FACTCue** ppCue /* Optional! */
) {
	return 0;
}

uint32_t FACTSoundBank_Stop(
	FACTSoundBank *pSoundBank,
	uint16_t nCueIndex,
	uint32_t dwFlags
) {
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
	return 0;
}

