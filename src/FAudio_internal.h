/* FACT - XACT Reimplementation for FNA
 * Copyright 2009-2018 Ethan Lee and the MonoGame Team
 *
 * Released under the Microsoft Public License.
 * See LICENSE for details.
 */

#include "FAudio.h"

#ifdef FAUDIO_UNKNOWN_PLATFORM
#include <assert.h>
#define FAudio_assert assert
#else
#include <SDL_assert.h>
#define FAudio_assert SDL_assert
#endif

#ifdef _WIN32
#define inline __inline
#endif

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

/* Based on...
 * Max pitch: 2400, 2^2 = 4.0
 * Max sample rate: 96000 FIXME: Assumption!
 * Min device frequency: 22050 FIXME: Assumption!
 */
#define MAX_RESAMPLE_STEP 18

typedef struct FAudioResampleState
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
} FAudioResampleState;

/* Internal FAudio Types */

typedef struct FAudioEngineCallbackEntry FAudioEngineCallbackEntry;
struct FAudioEngineCallbackEntry
{
	FAudioEngineCallback *callback;
	FAudioEngineCallbackEntry *next;
};

/* Public FAudio Types */

struct FAudio
{
	FAudioEngineCallbackEntry *callbacks;
};

typedef enum FAudioVoiceType
{
	FAUDIO_VOICE_SOURCE,
	FAUDIO_VOICE_SUBMIX,
	FAUDIO_VOICE_MASTER
} FAudioVoiceType;

struct FAudioVoice
{
	FAudioVoiceType type;
	FAudioVoiceSends sends;
	FAudioEffectChain effects;
	uint32_t flags;
	union
	{
		struct
		{
			float maxFreqRatio;
			FAudioWaveFormatEx format;
			FAudioVoiceCallback *callback;
		} src;
		struct
		{
			uint32_t inputChannels;
			uint32_t inputSampleRate;
		} mix;
		struct
		{
			uint32_t inputChannels;
			uint32_t inputSampleRate;
			uint32_t deviceIndex;
		} master;
	};
};

/* Internal Functions */

void FAudio_INTERNAL_InitResampler(FAudioResampleState *resample);

/* Platform Functions */

/* FIXME: Re-do these for FAudio... */
typedef struct FACTAudioEngine FACTAudioEngine;
void FAudio_PlatformInitEngine(FACTAudioEngine *engine, int16_t *id);
void FAudio_PlatformCloseEngine(FACTAudioEngine *engine);

uint32_t FAudio_PlatformGetDeviceCount();
void FAudio_PlatformGetDeviceDetails(
	uint32_t index,
	FAudioDeviceDetails *details
);

void* FAudio_malloc(size_t size);
void FAudio_free(void *ptr);
void FAudio_zero(void *ptr, size_t size);
void FAudio_memcpy(void *dst, const void *src, size_t size);
void FAudio_memmove(void *dst, void *src, size_t size);
size_t FAudio_strlen(const char *ptr);
int FAudio_strcmp(const char *str1, const char *str2);
void FAudio_strlcpy(char *dst, const char *src, size_t len);
double FAudio_pow(double x, double y);
double FAudio_log10(double x);
double FAudio_sqrt(double x);
double FAudio_acos(double x);
float FAudio_rng();
uint32_t FAudio_timems();

#define FAudio_min(val1, val2) \
	(val1 < val2 ? val1 : val2)
#define FAudio_max(val1, val2) \
	(val1 > val2 ? val1 : val2)
#define FAudio_clamp(val, min, max) \
	(val > max ? max : (val < min ? min : val))
