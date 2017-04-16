/* FACT - XACT Reimplementation for FNA
 * Copyright 2009-2017 Ethan Lee and the MonoGame Team
 *
 * Released under the Microsoft Public License.
 * See LICENSE for details.
 */

#include "FACT.h"

/* XACT Types */

struct FACTAudioEngine
{
	uint8_t TODO;
};

struct FACTSoundBank
{
	uint8_t TODO;
};

struct FACTWaveBank
{
	uint8_t TODO;
};

struct FACTWave
{
	uint8_t TODO;
};

struct FACTCue
{
	uint8_t TODO;
};

/* Platform Functions */

void* FACT_malloc(size_t size);
void FACT_free(void *ptr);
void FACT_zero(void *ptr, size_t size);
