/* FACT - XACT Reimplementation for FNA
 * Copyright 2009-2018 Ethan Lee and the MonoGame Team
 *
 * Released under the Microsoft Public License.
 * See LICENSE for details.
 */

#include "FACT_internal.h"

uint32_t FACTWaveBank_Destroy(FACTWaveBank *pWaveBank)
{
	FACTWaveBank *wb, *prev;
	FACTWave *wave = pWaveBank->waveList;

	/* Stop as many Waves as we can */
	while (wave != NULL)
	{
		FACTWave_Stop(wave, FACT_FLAG_STOP_IMMEDIATE);

		/* We use this to detect Waves deleted after the Bank */
		wave->parentBank = NULL;

		wave = wave->next;
	}

	if (pWaveBank->parentEngine != NULL)
	{
		/* Remove this WaveBank from the Engine list */
		wb = pWaveBank->parentEngine->wbList;
		prev = wb;
		while (wb != NULL)
		{
			if (wb == pWaveBank)
			{
				if (wb == prev) /* First in list */
				{
					pWaveBank->parentEngine->wbList = wb->next;
				}
				else
				{
					prev->next = wb->next;
				}
				break;
			}
			prev = wb;
			wb = wb->next;
		}
		FAudio_assert(wb != NULL && "Could not find WaveBank reference!");
	}

	/* Free everything, finally. */
	FAudio_free(pWaveBank->name);
	FAudio_free(pWaveBank->entries);
	FAudio_free(pWaveBank->entryRefs);
	FAudio_close(pWaveBank->io);
	FAudio_free(pWaveBank);
	return 0;
}

uint32_t FACTWaveBank_GetState(
	FACTWaveBank *pWaveBank,
	uint32_t *pdwState
) {
	/* FIXME: Is there more to this than just checking INUSE? */
	uint32_t i;
	for (i = 0; i < pWaveBank->entryCount; i += 1)
	{
		if (pWaveBank->entryRefs[i] > 0)
		{
			return FACT_STATE_INUSE;
		}
	}
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
	FAudio_assert(0 && "WaveBank name tables are not supported!");
	return 0;
}

uint32_t FACTWaveBank_GetWaveProperties(
	FACTWaveBank *pWaveBank,
	uint16_t nWaveIndex,
	FACTWaveProperties *pWaveProperties
) {
	FACTWaveBankEntry *entry = &pWaveBank->entries[nWaveIndex];

	/* FIXME: Name tables! -flibit */
	FAudio_zero(
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
	FAudioBuffer buffer;
	FAudioVoiceSends sends;
	FAudioSendDescriptor send;
	FAudioWaveFormatEx format;
	FACTWave *latest;
	FACTWaveBankEntry *entry = &pWaveBank->entries[nWaveIndex];

	*ppWave = (FACTWave*) FAudio_malloc(sizeof(FACTWave));
	(*ppWave)->next = NULL;

	/* Engine references */
	(*ppWave)->parentBank = pWaveBank;
	(*ppWave)->index = nWaveIndex;

	/* Playback */
	(*ppWave)->state = FACT_STATE_PREPARED;
	(*ppWave)->volume = 1.0f;
	(*ppWave)->pitch = 0;
	if (dwFlags & FACT_FLAG_UNITS_MS)
	{
		(*ppWave)->initialPosition = (uint32_t) (
			( /* Samples per millisecond... */
				(float) entry->Format.nSamplesPerSec /
				1000.0f
			) * (float) dwPlayOffset
		);
	}
	else
	{
		(*ppWave)->initialPosition = dwPlayOffset;
	}
	(*ppWave)->loopCount = nLoopCount;

	/* Create the voice */
	send.Flags = 0;
	send.pOutputVoice = pWaveBank->parentEngine->master;
	sends.SendCount = 1;
	sends.pSends = &send;
	format.wFormatTag = entry->Format.wFormatTag;
	format.nChannels = entry->Format.nChannels;
	format.nSamplesPerSec = entry->Format.nSamplesPerSec;
	format.nBlockAlign = entry->Format.wBlockAlign;
	format.wBitsPerSample = 8 << entry->Format.wBitsPerSample;
	if (format.wFormatTag == 0)
	{
		format.wFormatTag = 1; /* PCM */
	}
	else if (format.wFormatTag != 2) /* Includes 0x1 - XMA, 0x3 - WMA */
	{
		FAudio_assert(0 && "Rebuild your WaveBanks with ADPCM!");
	}
	(*ppWave)->callback.callback.OnBufferEnd = FACT_INTERNAL_OnBufferEnd;
	(*ppWave)->callback.callback.OnBufferStart = NULL;
	(*ppWave)->callback.callback.OnLoopEnd = NULL;
	(*ppWave)->callback.callback.OnStreamEnd = FACT_INTERNAL_OnStreamEnd;
	(*ppWave)->callback.callback.OnVoiceError = NULL;
	(*ppWave)->callback.callback.OnVoiceProcessingPassEnd = NULL;
	(*ppWave)->callback.callback.OnVoiceProcessingPassStart = NULL;
	(*ppWave)->callback.wave = *ppWave;
	(*ppWave)->callback.cue = NULL;
	(*ppWave)->callback.event = NULL;
	(*ppWave)->callback.eventInstance = NULL;
	FAudio_CreateSourceVoice(
		pWaveBank->parentEngine->audio,
		&(*ppWave)->voice,
		&format,
		0,
		4.0f,
		(FAudioVoiceCallback*) &(*ppWave)->callback,
		&sends,
		NULL
	);
	if (pWaveBank->streaming)
	{
		FAudio_assert(0 && "TODO: Streaming WaveBanks!");
	}
	else
	{
		buffer.Flags = FAUDIO_END_OF_STREAM;
		buffer.AudioBytes = entry->PlayRegion.dwLength;
		buffer.pAudioData = FAudio_memptr(
			pWaveBank->io,
			entry->PlayRegion.dwOffset
		);
		buffer.PlayBegin = (*ppWave)->initialPosition;
		buffer.PlayLength = entry->PlayRegion.dwLength;
		if (format.wFormatTag == 1)
		{
			if (format.wBitsPerSample == 16)
			{
				buffer.PlayLength /= 2;
			}
			buffer.PlayLength /= format.nChannels;
		}
		else if (format.wFormatTag == 2)
		{
			buffer.PlayLength = (
				buffer.PlayLength /
				((format.nBlockAlign + 22) * format.nChannels) *
				((format.nBlockAlign + 16) * 2)
			);
		}
		buffer.LoopBegin = entry->LoopRegion.dwStartSample;
		buffer.LoopLength = entry->LoopRegion.dwTotalSamples;
		buffer.LoopCount = nLoopCount;
	}
	buffer.pContext = NULL;
	FAudioSourceVoice_SubmitSourceBuffer((*ppWave)->voice, &buffer, NULL);

	/* Add to the WaveBank Wave list */
	if (pWaveBank->waveList == NULL)
	{
		pWaveBank->waveList = *ppWave;
	}
	else
	{
		latest = pWaveBank->waveList;
		while (latest->next != NULL)
		{
			latest = latest->next;
		}
		latest->next = *ppWave;
	}

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
	FACTWaveBank_Prepare(
		pWaveBank,
		nWaveIndex,
		dwFlags,
		dwPlayOffset,
		nLoopCount,
		ppWave
	);
	FACTWave_Play(*ppWave);
	return 0;
}

uint32_t FACTWaveBank_Stop(
	FACTWaveBank *pWaveBank,
	uint16_t nWaveIndex,
	uint32_t dwFlags
) {
	FACTWave *wave = pWaveBank->waveList;
	while (wave != NULL)
	{
		if (wave->index == nWaveIndex)
		{
			FACTWave_Stop(wave, dwFlags);
		}
		wave = wave->next;
	}
	return 0;
}
