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

typedef enum FAudioVoiceType
{
	FAUDIO_VOICE_SOURCE,
	FAUDIO_VOICE_SUBMIX,
	FAUDIO_VOICE_MASTER
} FAudioVoiceType;

typedef struct FAudioEngineCallbackEntry FAudioEngineCallbackEntry;
struct FAudioEngineCallbackEntry
{
	FAudioEngineCallback *callback;
	FAudioEngineCallbackEntry *next;
};

typedef struct FAudioSubmixVoiceEntry FAudioSubmixVoiceEntry;
struct FAudioSubmixVoiceEntry
{
	FAudioSubmixVoice *voice;
	FAudioSubmixVoiceEntry *next;
};

typedef struct FAudioSourceVoiceEntry FAudioSourceVoiceEntry;
struct FAudioSourceVoiceEntry
{
	FAudioSourceVoice *voice;
	FAudioSourceVoiceEntry *next;
};

typedef struct FAudioBufferEntry FAudioBufferEntry;
struct FAudioBufferEntry
{
	FAudioBuffer buffer;
	FAudioBufferEntry *next;
};

typedef void* FAudioPlatformFixedRateSRC;

/* Public FAudio Types */

struct FAudio
{
	uint8_t active;
	uint32_t updateSize;
	uint32_t submixStages;
	FAudioMasteringVoice *master;
	FAudioSubmixVoiceEntry *submixes;
	FAudioSourceVoiceEntry *sources;
	FAudioEngineCallbackEntry *callbacks;
	FAudioWaveFormatExtensible *mixFormat;
};

struct FAudioVoice
{
	FAudio *audio;
	FAudioVoiceType type;
	FAudioVoiceSends sends;
	FAudioEffectChain effects;
	FAudioFilterParameters filter;
	uint32_t flags;

	float volume;
	float channelVolume[2]; /* Assuming stereo input */
	uint32_t srcChannels;
	uint32_t dstChannels;
	float matrixCoefficients[2 * 8]; /* Assuming stereo input, 7.1 output */

	union
	{
		struct
		{
			/* Sample storage */
			uint32_t decodeSamples;
			int16_t *decodeCache;
			uint32_t outputSamples;
			float *outputResampleCache;
			uint8_t hasPad;
			int16_t pad[2]; /* Assuming stereo input */

			/* Read-only */
			float maxFreqRatio;
			FAudioWaveFormatEx format;
			FAudioVoiceCallback *callback;

			/* Dynamic */
			uint8_t active;
			float freqRatio;
			uint64_t totalSamples;
			FAudioBufferEntry *bufferList;
		} src;
		struct
		{
			/* Sample storage */
			uint32_t inputSamples;
			float *inputCache;
			uint32_t outputSamples;
			float *outputResampleCache;
			FAudioPlatformFixedRateSRC resampler;

			/* Read-only */
			uint32_t inputChannels;
			uint32_t inputSampleRate;
			uint32_t processingStage;
		} mix;
		struct
		{
			/* Output stream, allocated by Platform */
			float *output;

			/* Read-only */
			uint32_t inputChannels;
			uint32_t inputSampleRate;
			uint32_t deviceIndex;
		} master;
	};
};

/* Internal Functions */

void FAudio_INTERNAL_InitResampler(FAudioResampleState *resample);
void FAudio_INTERNAL_UpdateEngine(FAudio *audio, float *output);

/* Platform Functions */

void FAudio_PlatformInit(FAudio *audio);
void FAudio_PlatformQuit(FAudio *audio);
void FAudio_PlatformStart(FAudio *audio);
void FAudio_PlatformStop(FAudio *audio);

uint32_t FAudio_PlatformGetDeviceCount();
void FAudio_PlatformGetDeviceDetails(
	uint32_t index,
	FAudioDeviceDetails *details
);

FAudioPlatformFixedRateSRC FAudio_PlatformInitFixedRateSRC(
	uint32_t channels,
	uint32_t inputRate,
	uint32_t outputRate
);
void FAudio_PlatformCloseFixedRateSRC(FAudioPlatformFixedRateSRC resampler);
uint32_t FAudio_PlatformResample(
	FAudioPlatformFixedRateSRC resampler,
	float *input,
	uint32_t inLen,
	float *output,
	uint32_t outLen
);

/* stdlib Functions */

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
double FAudio_ceil(double x);
float FAudio_rng();
uint32_t FAudio_timems();

#define FAudio_min(val1, val2) \
	(val1 < val2 ? val1 : val2)
#define FAudio_max(val1, val2) \
	(val1 > val2 ? val1 : val2)
#define FAudio_clamp(val, min, max) \
	(val > max ? max : (val < min ? min : val))
