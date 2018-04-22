/* FAudio - XAudio Reimplementation for FNA
 *
 * Copyright (c) 2011-2018 Ethan Lee, Luigi Auriemma, and the MonoGame Team
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * In no event will the authors be held liable for any damages arising from
 * the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 * claim that you wrote the original software. If you use this software in a
 * product, an acknowledgment in the product documentation would be
 * appreciated but is not required.
 *
 * 2. Altered source versions must be plainly marked as such, and must not be
 * misrepresented as being the original software.
 *
 * 3. This notice may not be removed or altered from any source distribution.
 *
 * Ethan "flibitijibibo" Lee <flibitijibibo@flibitijibibo.com>
 *
 */

#include "FAudio_internal.h"

#include "stb_vorbis.h" /* TODO: Remove CRT dependency */

/* Globals */

uint32_t songRefcount = 0;
float songVolume = 1.0f;
FAudio *songAudio = NULL;
FAudioMasteringVoice *songMaster = NULL;

FAudioSourceVoice *songVoice = NULL;
FAudioVoiceCallback callbacks;
stb_vorbis *activeSong = NULL;
stb_vorbis_info activeSongInfo;
uint8_t *songCache;

/* Internal Functions */

void XNA_SongSubmitBuffer(FAudioVoiceCallback *callback, void *pBufferContext)
{
	FAudioBuffer buffer;
	uint32_t decoded = stb_vorbis_get_samples_short_interleaved(
		activeSong,
		activeSongInfo.channels,
		(short*) songCache,
		activeSongInfo.sample_rate
	);
	if (decoded == 0)
	{
		return;
	}
	buffer.Flags = (decoded < activeSongInfo.sample_rate) ?
		FAUDIO_END_OF_STREAM :
		0;
	buffer.AudioBytes = decoded * activeSongInfo.channels * sizeof(int16_t);
	buffer.pAudioData = songCache;
	buffer.PlayBegin = 0;
	buffer.PlayLength = decoded;
	buffer.LoopBegin = 0;
	buffer.LoopLength = 0;
	buffer.LoopCount = 0;
	buffer.pContext = NULL;
	FAudioSourceVoice_SubmitSourceBuffer(
		songVoice,
		&buffer,
		NULL
	);
}

void XNA_SongKill()
{
	if (songVoice != NULL)
	{
		FAudioSourceVoice_Stop(songVoice, 0, 0);
		FAudioVoice_DestroyVoice(songVoice);
		songVoice = NULL;
	}
	if (songCache != NULL)
	{
		FAudio_free(songCache);
		songCache = NULL;
	}
	activeSong = NULL;
}

void XNA_SongDevice_AddRef()
{
	songRefcount += 1;
	if (songRefcount == 1)
	{
		FAudioCreate(&songAudio, 0, FAUDIO_DEFAULT_PROCESSOR);
		FAudio_CreateMasteringVoice(
			songAudio,
			&songMaster,
			FAUDIO_DEFAULT_CHANNELS,
			48000, /* Should be 0, but SDL... */
			0,
			0,
			NULL
		);
	}
}

void XNA_SongDevice_Release()
{
	if (songRefcount == 0)
	{
		return;
	}
	songRefcount -= 1;
	if (songRefcount == 0)
	{
		XNA_SongKill();
		FAudioVoice_DestroyVoice(songMaster);
		FAudio_Release(songAudio);
	}
}

/* "Public" API */

FAUDIOAPI stb_vorbis* XNA_GenSong(const char* name, float *seconds)
{
	stb_vorbis *result = stb_vorbis_open_filename(name, NULL, NULL);
	if (result != NULL)
	{
		XNA_SongDevice_AddRef();
	}
	*seconds = stb_vorbis_stream_length_in_seconds(result);
	return result;
}

FAUDIOAPI void XNA_DisposeSong(stb_vorbis *song)
{
	if (song == NULL)
	{
		return;
	}
	if (song == activeSong)
	{
		XNA_SongKill();
	}
	stb_vorbis_close(song);
	XNA_SongDevice_Release();
}

FAUDIOAPI void XNA_PlaySong(stb_vorbis *song)
{
	FAudioWaveFormatEx format;
	if (song == NULL || song == activeSong)
	{
		return;
	}
	XNA_SongKill();

	/* Set format info */
	activeSongInfo = stb_vorbis_get_info(song);
	format.wFormatTag = 1;
	format.nChannels = activeSongInfo.channels;
	format.nSamplesPerSec = activeSongInfo.sample_rate;
	format.wBitsPerSample = sizeof(int16_t) * 8;
	format.nBlockAlign = format.nChannels * format.wBitsPerSample / 8;
	format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;
	format.cbSize = 0;

	/* Allocate decode cache */
	songCache = (uint8_t*) FAudio_malloc(format.nAvgBytesPerSec);

	/* Init voice */
	FAudio_zero(&callbacks, sizeof(FAudioVoiceCallback));
	callbacks.OnBufferEnd = XNA_SongSubmitBuffer;
	FAudio_CreateSourceVoice(
		songAudio,
		&songVoice,
		&format,
		0,
		1.0f, /* No pitch shifting here! */
		&callbacks,
		NULL,
		NULL
	);
	FAudioVoice_SetVolume(songVoice, songVolume, 0);

	/* Okay, this song is decoding now */
	activeSong = song;
	stb_vorbis_seek_start(activeSong);
	XNA_SongSubmitBuffer(NULL, NULL);

	/* Finally. */
	FAudioSourceVoice_Start(songVoice, 0, 0);
}

FAUDIOAPI void XNA_PauseSong()
{
	if (songVoice == NULL)
	{
		return;
	}
	FAudioSourceVoice_Stop(songVoice, 0, 0);
}

FAUDIOAPI void XNA_ResumeSong()
{
	if (songVoice == NULL)
	{
		return;
	}
	FAudioSourceVoice_Start(songVoice, 0, 0);
}

FAUDIOAPI void XNA_StopSong()
{
	XNA_SongKill();
}

FAUDIOAPI void XNA_SetSongVolume(float volume)
{
	songVolume = volume;
	if (songVoice != NULL)
	{
		FAudioVoice_SetVolume(songVoice, songVolume, 0);
	}
}

FAUDIOAPI uint32_t XNA_GetSongEnded()
{
	FAudioVoiceState state;
	if (songVoice == NULL || activeSong == NULL)
	{
		return 1;
	}
	FAudioSourceVoice_GetState(songVoice, &state);
	return state.BuffersQueued == 0;
}
