/* FACT - XACT Reimplementation for FNA
 * Copyright 2009-2017 Ethan Lee and the MonoGame Team
 *
 * Released under the Microsoft Public License.
 * See LICENSE for details.
 */

#include "FACT_internal.h"

uint32_t FACTWaveBank_Destroy(FACTWaveBank *pWaveBank)
{
	FACT_free(pWaveBank->name);
	FACT_free(pWaveBank->entries);
	FACT_close(pWaveBank->io);
	FACT_free(pWaveBank);
	return 0;
}

uint32_t FACTWaveBank_GetState(
	FACTWaveBank *pWaveBank,
	uint32_t *pdwState
) {
	return 0;
}

uint32_t FACTWaveBank_GetNumWaves(
	FACTWaveBank *pWaveBank,
	uint16_t *pnNumWaves
) {
	*pnNumWaves = pWaveBank->entryCount;
	return 0;
}

uint16_t FACTWaveBank_GetWaveIndex(
	FACTWaveBank *pWaveBank,
	const char *szFriendlyName
) {
	assert(0 && "WaveBank name tables are not supported!");
	return 0;
}

uint32_t FACTWaveBank_GetWaveProperties(
	FACTWaveBank *pWaveBank,
	uint16_t nWaveIndex,
	FACTWaveProperties *pWaveProperties
) {
	FACTWaveBankEntry *entry = &pWaveBank->entries[nWaveIndex];

	/* FIXME: Name tables! -flibit */
	FACT_zero(
		pWaveProperties->friendlyName,
		sizeof(pWaveProperties->friendlyName)
	);

	pWaveProperties->format = entry->Format;
	pWaveProperties->durationInSamples = (
		entry->LoopRegion.dwStartSample +
		entry->LoopRegion.dwTotalSamples
	); /* FIXME: Do we just want the full wave block? -flibit */
	pWaveProperties->loopRegion = entry->LoopRegion;
	pWaveProperties->streaming = pWaveBank->streaming;
	return 0;
}

uint32_t FACTWaveBank_Prepare(
	FACTWaveBank *pWaveBank,
	uint16_t nWaveIndex,
	uint32_t dwFlags,
	uint32_t dwPlayOffset,
	uint8_t nLoopCount,
	FACTWave **ppWave
) {
	return 0;
}

uint32_t FACTWaveBank_Play(
	FACTWaveBank *pWaveBank,
	uint16_t nWaveIndex,
	uint32_t dwFlags,
	uint32_t dwPlayOffset,
	uint8_t nLoopCount,
	FACTWave **ppWave
) {
	return 0;
}

uint32_t FACTWaveBank_Stop(
	FACTWaveBank *pWaveBank,
	uint16_t nWaveIndex,
	uint32_t dwFlags
) {
	return 0;
}
