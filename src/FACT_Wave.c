/* FACT - XACT Reimplementation for FNA
 * Copyright 2009-2017 Ethan Lee and the MonoGame Team
 *
 * Released under the Microsoft Public License.
 * See LICENSE for details.
 */

#include "FACT_internal.h"

uint32_t FACTWave_Destroy(FACTWave *pWave)
{
	FACTWave *wave, *prev;

	/* Stop before we start deleting everything */
	FACTWave_Stop(pWave, FACT_FLAG_STOP_IMMEDIATE);

	/* Remove this Wave from the WaveBank list */
	wave = pWave->parentBank->waveList;
	prev = wave;
	while (wave != NULL)
	{
		if (wave == pWave)
		{
			if (wave == prev) /* First in list */
			{
				pWave->parentBank->waveList = wave->next;
			}
			else
			{
				prev->next = wave->next;
			}
			break;
		}
		prev = wave;
		wave = wave->next;
	}
	assert(wave != NULL && "Could not find Wave reference!");

	FACT_free(pWave);
	return 0;
}

uint32_t FACTWave_Play(FACTWave *pWave)
{
	/* TODO: Init playback state */
	pWave->state |= FACT_STATE_PLAYING;
	/* FIXME: How do we deal with STOPPING? -flibit */
	pWave->state &= ~(
		FACT_STATE_PAUSED |
		FACT_STATE_STOPPED
	);
	return 0;
}

uint32_t FACTWave_Stop(FACTWave *pWave, uint32_t dwFlags)
{
	if (dwFlags & FACT_FLAG_STOP_IMMEDIATE)
	{
		pWave->state |= FACT_STATE_STOPPED;
		pWave->state &= ~(
			FACT_STATE_PLAYING |
			FACT_STATE_STOPPING |
			FACT_STATE_PAUSED
		);
		pWave->position = pWave->initialPosition;
	}
	else
	{
		/* FIXME: How do we deal with PAUSED? -flibit */
		pWave->state |= FACT_STATE_STOPPING;
	}
	return 0;
}

uint32_t FACTWave_Pause(FACTWave *pWave, int32_t fPause)
{
	/* FIXME: Does the Cue STOPPING/STOPPED rule apply here too? */
	if (pWave->state & (FACT_STATE_STOPPING | FACT_STATE_STOPPED))
	{
		return 0;
	}

	/* All we do is set the flag, the mixer handles the rest */
	if (fPause)
	{
		pWave->state |= FACT_STATE_PAUSED;
	}
	else
	{
		pWave->state &= ~FACT_STATE_PAUSED;
	}
	return 0;
}

uint32_t FACTWave_GetState(FACTWave *pWave, uint32_t *pdwState)
{
	*pdwState = pWave->state;
	return 0;
}

uint32_t FACTWave_SetPitch(FACTWave *pWave, int16_t pitch)
{
	pWave->pitch = FACT_clamp(
		pitch,
		FACTPITCH_MIN_TOTAL,
		FACTPITCH_MAX_TOTAL
	);
	return 0;
}

uint32_t FACTWave_SetVolume(FACTWave *pWave, float volume)
{
	pWave->volume = FACT_clamp(
		volume,
		FACTVOLUME_MIN,
		FACTVOLUME_MAX
	);
	return 0;
}

uint32_t FACTWave_SetMatrixCoefficients(
	FACTWave *pWave,
	uint32_t uSrcChannelCount,
	uint32_t uDstChannelCount,
	float *pMatrixCoefficients
) {
	/* TODO */
	return 0;
}

uint32_t FACTWave_GetProperties(
	FACTWave *pWave,
	FACTWaveInstanceProperties *pProperties
) {
	FACTWaveBank_GetWaveProperties(
		pWave->parentBank,
		pWave->index,
		&pProperties->properties
	);

	/* FIXME: This is unsupported on PC, do we care about this? */
	pProperties->backgroundMusic = 0;

	return 0;
}
