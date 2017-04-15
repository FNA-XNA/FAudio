/* FACT - XACT Reimplementation for FNA
 * Copyright 2009-2017 Ethan Lee and the MonoGame Team
 *
 * Released under the Microsoft Public License.
 * See LICENSE for details.
 */

#include "FACT.h"
#include "FACT_platform.h"

uint32_t FACTWaveBank_Destroy(FACTWaveBank *pWaveBank)
{
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
	uint16_t pnNumWaves
) {
	return 0;
}

uint16_t FACTWaveBank_GetWaveIndex(
	FACTWaveBank *pWaveBank,
	const char *szFriendlyName
) {
	return 0;
}

uint32_t FACTWaveBank_GetWaveProperties(
	FACTWaveBank *pWaveBank,
	uint16_t nWaveIndex,
	FACTWaveProperties *pWaveProperties
) {
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
