/* FACT - XACT Reimplementation for FNA
 * Copyright 2009-2017 Ethan Lee and the MonoGame Team
 *
 * Released under the Microsoft Public License.
 * See LICENSE for details.
 */

#include "FACT_internal.h"

uint32_t FACTWave_Destroy(FACTWave *pWave)
{
	return 0;
}

uint32_t FACTWave_Play(FACTWave *pWave)
{
	return 0;
}

uint32_t FACTWave_Stop(FACTWave *pWave, uint32_t dwFlags)
{
	return 0;
}

uint32_t FACTWave_Pause(FACTWave *pWave, int32_t fPause)
{
	return 0;
}

uint32_t FACTWave_GetState(FACTWave *pWave, uint32_t *pdwState)
{
	return 0;
}

uint32_t FACTWave_SetPitch(FACTWave *pWave, int16_t pitch)
{
	return 0;
}

uint32_t FACTWave_SetVolume(FACTWave *pWave, float volume)
{
	return 0;
}

uint32_t FACTWave_SetMatrixCoefficients(
	FACTWave *pWave,
	uint32_t uSrcChannelCount,
	uint32_t uDstChannelCount,
	float *pMatrixCoefficients
) {
	return 0;
}

uint32_t FACTWave_GetProperties(
	FACTWave *pWave,
	FACTWaveInstanceProperties *pProperties
) {
	return 0;
}
