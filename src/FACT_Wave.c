/* FACT - XACT Reimplementation for FNA
 * Copyright 2009-2018 Ethan Lee and the MonoGame Team
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

	if (pWave->parentBank != NULL)
	{
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
		FAudio_assert(wave != NULL && "Could not find Wave reference!");
	}

	FAudio_free(pWave);
	return 0;
}

uint32_t FACTWave_Play(FACTWave *pWave)
{
	FAudio_assert(!(pWave->state & (FACT_STATE_PLAYING | FACT_STATE_STOPPING)));
	pWave->state |= FACT_STATE_PLAYING;
	pWave->state &= ~(
		FACT_STATE_PAUSED |
		FACT_STATE_STOPPED
	);
	FAudioSourceVoice_Start(pWave->voice, 0, 0);
	return 0;
}

uint32_t FACTWave_Stop(FACTWave *pWave, uint32_t dwFlags)
{
	/* There are two ways that a Wave might be stopped immediately:
	 * 1. The program explicitly asks for it
	 * 2. The Wave is paused and therefore we can't do fade/release effects
	 */
	if (	dwFlags & FACT_FLAG_STOP_IMMEDIATE ||
		pWave->state & FACT_STATE_PAUSED	)
	{
		pWave->state |= FACT_STATE_STOPPED;
		pWave->state &= ~(
			FACT_STATE_PLAYING |
			FACT_STATE_STOPPING |
			FACT_STATE_PAUSED
		);
		FAudioSourceVoice_Stop(pWave->voice, 0, 0);
		FAudioSourceVoice_FlushSourceBuffers(pWave->voice);
	}
	else
	{
		pWave->state |= FACT_STATE_STOPPING;
		/* TODO: TAILS or something fancier? -flibit */
		FAudioSourceVoice_Stop(pWave->voice, 0, 0);
		FAudioSourceVoice_FlushSourceBuffers(pWave->voice);
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
		FAudioSourceVoice_Stop(pWave->voice, 0, 0);
	}
	else
	{
		pWave->state &= ~FACT_STATE_PAUSED;
		FAudioSourceVoice_Start(pWave->voice, 0, 0);
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
	pWave->pitch = FAudio_clamp(
		pitch,
		FACTPITCH_MIN_TOTAL,
		FACTPITCH_MAX_TOTAL
	);
	FAudioSourceVoice_SetFrequencyRatio(
		pWave->voice,
		(float) FAudio_pow(2.0, pWave->pitch / 1200.0),
		0
	);
	return 0;
}

uint32_t FACTWave_SetVolume(FACTWave *pWave, float volume)
{
	pWave->volume = FAudio_clamp(
		volume,
		FACTVOLUME_MIN,
		FACTVOLUME_MAX
	);
	FAudioVoice_SetVolume(
		pWave->voice,
		volume,
		0
	);
	return 0;
}

uint32_t FACTWave_SetMatrixCoefficients(
	FACTWave *pWave,
	uint32_t uSrcChannelCount,
	uint32_t uDstChannelCount,
	float *pMatrixCoefficients
) {
	FAudioVoice_SetOutputMatrix(
		pWave->voice,
		pWave->voice->sends.pSends->pOutputVoice,
		uSrcChannelCount,
		uDstChannelCount,
		pMatrixCoefficients,
		0
	);
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
