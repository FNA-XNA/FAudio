/* FACT - XACT Reimplementation for FNA
 * Copyright 2009-2017 Ethan Lee and the MonoGame Team
 *
 * Released under the Microsoft Public License.
 * See LICENSE for details.
 */

#include "FACT.h"

/* FIXME: Remove this! */
#include <assert.h>

/* Internal Constants */

#define FACT_VOLUME_0 180

/* Internal AudioEngine Types */

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

/* Internal SoundBank Types */

typedef struct FACTCueData
{
	uint8_t flags;
	uint32_t sbCode;
	uint32_t transitionOffset;
	uint8_t instanceLimit;
	uint16_t fadeIn;
	uint16_t fadeOut;
	uint8_t maxInstanceBehavior;
	uint8_t instanceCount;
} FACTCueData;

typedef enum
{
	FACTEVENT_STOP =				0,
	FACTEVENT_PLAYWAVE =				1,
	FACTEVENT_PLAYWAVETRACKVARIATION =		3,
	FACTEVENT_PLAYWAVEEFFECTVARIATION =		4,
	FACTEVENT_PLAYWAVETRACKEFFECTVARIATION =	6,
	FACTEVENT_PITCH =				7,
	FACTEVENT_VOLUME =				8,
	FACTEVENT_MARKER =				9,
	FACTEVENT_PITCHREPEATING =			16,
	FACTEVENT_VOLUMEREPEATING =			17,
	FACTEVENT_MARKERREPEATING =			18
} FACTEventType;

typedef struct FACTEvent_PlayWave
{
	uint8_t flags;
	uint16_t position;
	uint16_t angle;

	/* Track Variation */
	uint8_t isComplex;
	union
	{
		struct
		{
			uint16_t track;
			uint8_t wavebank;
		} simple;
		struct
		{
			uint16_t variation;
			uint16_t trackCount;
			uint16_t *tracks;
			uint8_t *wavebanks;
			uint8_t *weights;
		} complex;
	};

	/* Effect Variation */
	int16_t minPitch;
	int16_t maxPitch;
	uint8_t minVolume;
	uint8_t maxVolume;
	float minFrequency;
	float maxFrequency;
	float minQFactor;
	float maxQFactor;
	uint16_t variationFlags;
} FACTEvent_PlayWave;

typedef struct FACTEvent_SetValue
{
	uint8_t settings;
	union
	{
		struct
		{
			float initialValue;
			float initialSlope;
			float slopeDelta;
			uint16_t duration;
		} ramp;
		struct
		{
			uint8_t flags;
			float value1;
			float value2;
		} equation;
	};
} FACTEvent_SetValue;

typedef struct FACTEvent_Stop
{
	uint8_t flags;
} FACTEvent_Stop;

typedef struct FACTEvent_Marker
{
	uint32_t marker;
	uint8_t repeating;
} FACTEvent_Marker;

typedef struct FACTEvent
{
	uint16_t type;
	uint16_t timestamp;
	uint16_t randomOffset;
	uint8_t loopCount;
	uint16_t frequency;
	union
	{
		FACTEvent_PlayWave wave;
		FACTEvent_SetValue value;
		FACTEvent_Stop stop;
		FACTEvent_Marker marker;
	};
} FACTEvent;

typedef struct FACTClip
{
	uint8_t volume;
	uint8_t filter;
	uint8_t qfactor;
	uint16_t frequency;

	uint8_t eventCount;
	FACTEvent *events;
} FACTClip;

typedef struct FACTSound
{
	uint8_t flags;
	uint16_t category;
	uint8_t volume;
	int16_t pitch;

	uint8_t clipCount;
	uint8_t rpcCodeCount;
	uint8_t dspCodeCount;

	FACTClip *clips;
	size_t *rpcCodes;
	size_t *dspCodes;
} FACTSound;

typedef struct FACTVariation
{
	uint8_t isComplex;
	union
	{
		struct
		{
			uint16_t track;
			uint8_t wavebank;
		} simple;
		size_t soundCode;
	};
	float minWeight;
	float maxWeight;
} FACTVariation;

typedef struct FACTVariationTable
{
	uint8_t flags;
	uint16_t variable;

	uint16_t entryCount;
	FACTVariation *entries;
} FACTVariationTable;

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

	FACTSoundBank *sbList;
	float *globalVariableValues;
};

struct FACTSoundBank
{
	/* Engine references */
	FACTAudioEngine *parentEngine;
	FACTSoundBank *next;
	FACTCue *cueList;

	/* Array sizes */
	uint16_t cueCount;
	uint8_t wavebankCount;
	uint16_t soundCount;
	uint16_t variationCount;

	/* Strings, strings everywhere! */
	char **wavebankNames;
	char **cueNames;

	/* Actual SoundBank information */
	char *name;
	FACTCueData *cues;
	FACTSound *sounds;
	size_t *soundCodes;
	FACTVariationTable *variations;
	size_t *variationCodes;
};

struct FACTWaveBank
{
	char *name;

	uint32_t entryCount;
	FACTWaveBankEntry *entries;
	uint32_t *entryRefs;

	uint16_t streaming;
	FACTIOStream *io;
};

struct FACTWave
{
	uint8_t TODO;
};

struct FACTCue
{
	/* Engine references */
	FACTSoundBank *parentBank;
	FACTCue *next;
	uint8_t managed;
	uint16_t index;

	/* Instance data */
	float *variableValues;

	/* Playback */
	uint32_t state;
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
int FACT_strcmp(const char *str1, const char *str2);
void FACT_strlcpy(char *dst, const char *src, size_t len);

#define FACT_clamp(val, min, max) \
	(nValue > max ? max : (nValue < min ? min : nValue))

typedef size_t (FACTCALL * FACT_readfunc)(
	void *data,
	void *dst,
	size_t size,
	size_t count
);
typedef int64_t (FACTCALL * FACT_seekfunc)(
	void *data,
	int64_t offset,
	int whence
);
typedef int (FACTCALL * FACT_closefunc)(
	void *data
);

struct FACTIOStream
{
	void *data;
	FACT_readfunc read;
	FACT_seekfunc seek;
	FACT_closefunc close;
};
