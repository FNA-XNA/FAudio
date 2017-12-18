/* FACT - XACT Reimplementation for FNA
 * Copyright 2009-2017 Ethan Lee and the MonoGame Team
 *
 * Released under the Microsoft Public License.
 * See LICENSE for details.
 */

#include "FACT.h"

#ifdef FACT_UNKNOWN_PLATFORM
#include <assert.h>
#define FACT_assert assert
#else
#include <SDL_assert.h>
#define FACT_assert SDL_assert
#endif

/* Internal Constants */

#define FACT_VOLUME_0 180

/* Resampling */

/* Steps are stored in fixed-point with 12 bits for the fraction:
 *
 * 00000000000000000000 000000000000
 * ^ Integer block (20) ^ Fraction block (12)
 *
 * For example, to get 1.5:
 * 00000000000000000001 100000000000
 *
 * The Integer block works exactly like you'd expect.
 * The Fraction block is divided by the Integer's "One" value.
 * So, the above Fraction represented visually...
 *   1 << 11
 *   -------
 *   1 << 12
 * ... which, simplified, is...
 *   1 << 0
 *   ------
 *   1 << 1
 * ... in other words, 1 / 2, or 0.5.
 */
typedef uint32_t fixed32;
#define FIXED_PRECISION		12
#define FIXED_ONE		(1 << FIXED_PRECISION)

/* Quick way to drop parts */
#define FIXED_FRACTION_MASK	(FIXED_ONE - 1)
#define FIXED_INTEGER_MASK	~RESAMPLE_FRACTION_MASK

/* Helper macros to convert fixed to float */
#define DOUBLE_TO_FIXED(dbl) \
	((fixed32) (dbl * FIXED_ONE + 0.5)) /* FIXME: Just round, or ceil? */
#define FIXED_TO_DOUBLE(fxd) ( \
	(double) (fxd >> FIXED_PRECISION) + /* Integer part */ \
	((fxd & FIXED_FRACTION_MASK) * (1.0 / FIXED_ONE)) /* Fraction part */ \
)

/* FIXME: Arbitrary 1-pre 1-post */
#define RESAMPLE_PADDING 1

typedef struct FACTResampleState
{
	/* Checked against wave->pitch for redundancy */
	uint16_t pitch;

	/* Okay, so here's what all that fixed-point goo is for:
	 *
	 * Inevitably you're going to run into weird sample rates,
	 * both from WaveBank data and from pitch shifting changes.
	 *
	 * How we deal with this is by calculating a fixed "step"
	 * value that steps from sample to sample at the speed needed
	 * to get the correct output sample rate, and the offset
	 * is stored as separate integer and fraction values.
	 *
	 * This allows us to do weird fractional steps between samples,
	 * while at the same time not letting it drift off into death
	 * thanks to floating point madness.
	 */
	fixed32 step;
	fixed32 offset;

	/* Padding used for smooth resampling from block to block */
	int16_t padding[2][RESAMPLE_PADDING];
} FACTResampleState;

/* Internal AudioEngine Types */

typedef struct FACTAudioCategory
{
	uint8_t instanceLimit;
	uint16_t fadeInMS;
	uint16_t fadeOutMS;
	uint8_t maxInstanceBehavior;
	int16_t parentCategory;
	uint8_t volume;
	uint8_t visibility;
	uint8_t instanceCount;
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

typedef enum FACTRPCParameter
{
	RPC_PARAMETER_VOLUME,
	RPC_PARAMETER_PITCH,
	RPC_PARAMETER_REVERBSEND,
	RPC_PARAMETER_FILTERFREQUENCY,
	RPC_PARAMETER_FILTERQFACTOR,
	RPC_PARAMETER_COUNT /* If >=, DSP Parameter! */
} FACTRPCParameter;

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
	uint32_t code;

	uint8_t volume;
	uint8_t filter;
	uint8_t qfactor;
	uint16_t frequency;

	uint8_t rpcCodeCount;
	uint32_t *rpcCodes;

	uint8_t eventCount;
	FACTEvent *events;
} FACTClip;

typedef struct FACTSound
{
	uint8_t flags;
	uint16_t category;
	uint8_t volume;
	int16_t pitch;
	uint8_t priority;

	uint8_t clipCount;
	uint8_t rpcCodeCount;
	uint8_t dspCodeCount;

	FACTClip *clips;
	uint32_t *rpcCodes;
	uint32_t *dspCodes;
} FACTSound;

typedef struct FACTInstanceRPCData
{
	float rpcVolume;
	float rpcPitch;
	float rpcFilterFreq;
} FACTInstanceRPCData;

typedef struct FACTEventInstance
{
	uint16_t timestamp;
	uint8_t loopCount;
	uint8_t finished;
	union
	{
		FACTWave *wave;
		float value;
	} data;
} FACTEventInstance;

typedef struct FACTClipInstance
{
	/* Tracks which events have fired */
	FACTEventInstance *events;

	/* RPC instance data */
	FACTInstanceRPCData rpcData;
} FACTClipInstance;

typedef struct FACTSoundInstance
{
	/* Who wants to malloc anyway? */
	uint8_t exists;

	/* Base Sound reference */
	FACTSound *sound;

	/* Per-instance clip information */
	FACTClipInstance *clips;

	/* RPC instance data */
	FACTInstanceRPCData rpcData;
} FACTSoundInstance;

typedef struct FACTVariation
{
	uint8_t isComplex;
	union
	{
		struct
		{
			uint16_t track;
			uint8_t wavebank;
		};
		uint32_t soundCode;
	};
	float minWeight;
	float maxWeight;
	uint32_t linger;
} FACTVariation;

typedef struct FACTVariationTable
{
	uint8_t flags;
	int16_t variable;

	uint16_t entryCount;
	FACTVariation *entries;
} FACTVariationTable;

/* Internal Wave Types */

typedef uint32_t (FACTCALL * FACTDecodeCallback)(
	FACTWave *wave,
	int16_t *decodeCacheL,
	int16_t *decodeCacheR,
	uint32_t samples
);

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
	uint32_t *rpcCodes;
	uint32_t *dspPresetCodes;

	FACTAudioCategory *categories;
	FACTVariable *variables;
	FACTRPC *rpcs;
	FACTDSPPreset *dspPresets;

	/* Engine references */
	FACTSoundBank *sbList;
	FACTWaveBank *wbList;
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
	uint32_t *soundCodes;
	FACTVariationTable *variations;
	uint32_t *variationCodes;
};

struct FACTWaveBank
{
	/* Engine references */
	FACTAudioEngine *parentEngine;
	FACTWave *waveList;
	FACTWaveBank *next;

	/* Guess what this is? */
	char *name;

	/* Actual WaveBank information */
	uint32_t entryCount;
	FACTWaveBankEntry *entries;
	uint32_t *entryRefs;

	/* I/O information */
	uint16_t streaming;
	FACTIOStream *io;
};

struct FACTWave
{
	/* Engine references */
	FACTWaveBank *parentBank;
	FACTWave *next;
	uint16_t index;

	/* Playback */
	uint32_t state;
	float volume;
	int16_t pitch;
	uint32_t position;
	uint32_t initialPosition;
	uint8_t loopCount;

	/* Decoding */
	FACTDecodeCallback decode;
	FACTResampleState resample;
	int16_t msadpcmCache[1024];
	uint16_t msadpcmExtra;
	uint8_t stereo; /* Forced to 0 on Apply3D */
};

struct FACTCue
{
	/* Engine references */
	FACTSoundBank *parentBank;
	FACTCue *next;
	uint8_t managed;
	uint16_t index;

	/* Sound data */
	FACTCueData *data;
	union
	{
		FACTSound *sound;
		FACTVariationTable *variation;
	} sound;

	/* Instance data */
	float *variableValues;

	/* Playback */
	uint32_t state;
	union
	{
		FACTSound *sound;
		FACTVariation *variation;
	} active;
	FACTSoundInstance soundInstance;

	/* Timer */
	uint32_t start;
	uint32_t elapsed;
};

/* Internal functions */

void FACT_INTERNAL_UpdateEngine(FACTAudioEngine *engine);
uint8_t FACT_INTERNAL_UpdateCue(FACTCue *cue, uint32_t elapsed);
uint32_t FACT_INTERNAL_GetWave(
	FACTWave *wave,
	int16_t *decodeCacheL,
	int16_t *decodeCacheR,
	float *resampleCacheL,
	float *resampleCacheR,
	uint32_t samples
);

#define DECODE_FUNC(type) \
	extern uint32_t FACT_INTERNAL_Decode##type( \
		FACTWave *wave, \
		int16_t *decodeCacheL, \
		int16_t *decodeCacheR, \
		uint32_t samples \
	);
DECODE_FUNC(MonoPCM8)
DECODE_FUNC(MonoPCM16)
DECODE_FUNC(MonoMSADPCM)
DECODE_FUNC(StereoPCM8)
DECODE_FUNC(StereoPCM16)
DECODE_FUNC(StereoMSADPCM)
DECODE_FUNC(StereoToMonoPCM8)
DECODE_FUNC(StereoToMonoPCM16)
DECODE_FUNC(StereoToMonoMSADPCM)
#undef DECODE_FUNC

/* Platform Functions */

void FACT_PlatformInitEngine(FACTAudioEngine *engine, wchar_t *id);
void FACT_PlatformCloseEngine(FACTAudioEngine *engine);

void FACT_PlatformInitResampler(FACTWave *wave);

uint16_t FACT_PlatformGetRendererCount();
void FACT_PlatformGetRendererDetails(
	uint16_t index,
	FACTRendererDetails *details
);
void FACT_PlatformGetFinalMixFormat(
	FACTAudioEngine *pEngine,
	FACTWaveFormatExtensible *pFinalMixFormat
);

void* FACT_malloc(size_t size);
void FACT_free(void *ptr);
void FACT_zero(void *ptr, size_t size);
void FACT_memcpy(void *dst, void *src, size_t size);
void FACT_memmove(void *dst, void *src, size_t size);
size_t FACT_strlen(const char *ptr);
int FACT_strcmp(const char *str1, const char *str2);
void FACT_strlcpy(char *dst, const char *src, size_t len);
float FACT_rng();
uint32_t FACT_timems();

#define FACT_min(val1, val2) \
	(val1 < val2 ? val1 : val2)
#define FACT_max(val1, val2) \
	(val1 > val2 ? val1 : val2)
#define FACT_clamp(val, min, max) \
	(val > max ? max : (val < min ? min : val))

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
