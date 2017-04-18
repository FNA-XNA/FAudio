/* FACT - XACT Reimplementation for FNA
 * Copyright 2009-2017 Ethan Lee and the MonoGame Team
 *
 * Released under the Microsoft Public License.
 * See LICENSE for details.
 */

#include "FACT.h"

/* FIXME: Remove this! */
#include <assert.h>

/* Internal XACT Types */

typedef struct FACTAudioCategory
{
	uint8_t maxInstances;
	uint16_t fadeInMS;
	uint16_t fadeOutMS;
	uint8_t instanceBehavior;
	int16_t parentCategory;
	uint8_t volume;
	uint8_t visibility;
} FACTAudioCategory;

typedef struct FACTVariable
{
	uint8_t accessibility;
	float initialValue;
	float minValue;
	float maxValue;
} FACTVariable;

typedef struct FACTRPCPoint
{
	float x;
	float y;
	uint8_t type;
} FACTRPCPoint;

typedef struct FACTRPC
{
	uint16_t variable;
	uint8_t pointCount;
	uint16_t parameter;
	FACTRPCPoint *points;
} FACTRPC;

typedef struct FACTDSPParameter
{
	uint8_t type;
	float value;
	float minVal;
	float maxVal;
	uint16_t unknown;
} FACTDSPParameter;

typedef struct FACTDSPPreset
{
	uint8_t accessibility;
	uint32_t parameterCount;
	FACTDSPParameter *parameters;
} FACTDSPPreset;

/* Public XACT Types */

struct FACTAudioEngine
{
	uint16_t categoryCount;
	uint16_t variableCount;
	uint16_t rpcCount;
	uint16_t dspPresetCount;
	uint16_t dspParameterCount;

	char **categoryNames;
	char **variableNames;
	size_t *rpcCodes;
	size_t *dspPresetCodes;

	FACTAudioCategory *categories;
	FACTVariable *variables;
	FACTRPC *rpcs;
	FACTDSPPreset *dspPresets;
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

/* Helper Functions */

#define READ_FUNC(type, size, suffix) \
	static inline type read_##suffix(uint8_t **ptr) \
	{ \
		type result = *((type*) *ptr); \
		*ptr += size; \
		return result; \
	}

READ_FUNC(uint8_t, 1, u8)
READ_FUNC(uint16_t, 2, u16)
READ_FUNC(uint32_t, 4, u32)
READ_FUNC(uint64_t, 8, u64)
READ_FUNC(int16_t, 2, s16)
READ_FUNC(float, 4, f32)

#undef READ_FUNC

/* Platform Functions */

void* FACT_malloc(size_t size);
void FACT_free(void *ptr);
void FACT_zero(void *ptr, size_t size);
void FACT_memcpy(void *dst, void *src, size_t size);
size_t FACT_strlen(const char *ptr);
