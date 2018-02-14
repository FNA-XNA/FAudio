/* FAudio - XAudio Reimplementation for FNA
 *
 * Copyright (c) 2018 Ethan Lee.
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

typedef struct XNASong
{
	uint8_t TODO;
} XNASong;

FAUDIOAPI XNASong* XNA_GenSong(const char* name)
{
	/* TODO */
	return NULL;
}

FAUDIOAPI void XNA_DisposeSong(XNASong *song)
{
	/* TODO */
}

FAUDIOAPI void XNA_PlaySong(XNASong *song)
{
	/* TODO */
}

FAUDIOAPI void XNA_PauseSong(XNASong *song)
{
	/* TODO */
}

FAUDIOAPI void XNA_ResumeSong(XNASong *song)
{
	/* TODO */
}

FAUDIOAPI void XNA_StopSong(XNASong *song)
{
	/* TODO */
}

FAUDIOAPI void XNA_SetSongVolume(XNASong *song, float volume)
{
	/* TODO */
}

FAUDIOAPI uint32_t XNA_GetSongEnded(XNASong *song)
{
	/* TODO */
	return 0;
}
