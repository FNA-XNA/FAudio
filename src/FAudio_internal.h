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

#include "FAudio.h"
#include "FAPOBase.h"

#ifdef FAUDIO_UNKNOWN_PLATFORM
#include <assert.h>
#define FAudio_assert assert
#else
#include <SDL_assert.h>
#define FAudio_assert SDL_assert
#endif

/* Windows/Visual Studio cruft */
#ifdef _WIN32
#define inline __inline
#if defined(_MSC_VER)
	#if (_MSC_VER >= 1700) /* VS2012+ */
		#define restrict __restrict
	#else /* VS2010- */
		#define restrict
	#endif
#endif
#endif

/* C++ does not have restrict (though VS2012+ does have __restrict) */
#if defined(__cplusplus) && !defined(restrict)
#define restrict
#endif

/* Linked Lists */

typedef struct LinkedList LinkedList;
struct LinkedList
{
	void* entry;
	LinkedList *next;
};
void LinkedList_AddEntry(LinkedList **start, void* toAdd);
void LinkedList_RemoveEntry(LinkedList **start, void* toRemove);

/* Internal FAudio Types */

typedef enum FAudioVoiceType
{
	FAUDIO_VOICE_SOURCE,
	FAUDIO_VOICE_SUBMIX,
	FAUDIO_VOICE_MASTER
} FAudioVoiceType;

typedef struct FAudioBufferEntry FAudioBufferEntry;
struct FAudioBufferEntry
{
	FAudioBuffer buffer;
	FAudioBufferEntry *next;
};

typedef void (FAUDIOCALL * FAudioDecodeCallback)(
	FAudioBuffer *buffer,
	uint32_t curOffset,
	float *decodeCache,
	uint32_t samples,
	FAudioWaveFormatEx *format
);

typedef void* FAudioPlatformFixedRateSRC;

typedef float FAudioFilterState[4];

/* Public FAudio Types */

struct FAudio
{
	uint8_t active;
	uint32_t refcount;
	uint32_t updateSize;
	uint32_t submixStages;
	FAudioMasteringVoice *master;
	LinkedList *submixes;
	LinkedList *sources;
	LinkedList *callbacks;
	FAudioWaveFormatExtensible *mixFormat;

	/* Temp storage for processing, interleaved PCM32F */
	#define EXTRA_DECODE_PADDING 2
	uint32_t decodeSamples;
	uint32_t resampleSamples;
	float *decodeCache;
	float *resampleCache;
};

struct FAudioVoice
{
	FAudio *audio;
	uint32_t flags;
	FAudioVoiceType type;

	FAudioVoiceSends sends;
	float **sendCoefficients;
	struct
	{
		uint32_t count;
		FAudioEffectDescriptor *desc;
		void **parameters;
		uint32_t *parameterSizes;
		uint8_t *parameterUpdates;
	} effects;
	FAudioFilterParameters filter;
	FAudioFilterState *filterState;

	float volume;
	float *channelVolume;

	union
	{
		struct
		{
			/* Sample storage */
			uint32_t decodeSamples;
			uint32_t resampleSamples;

			/* Resampler */
			float resampleFreqRatio;
			uint64_t resampleStep;
			uint64_t resampleOffset;
			uint64_t curBufferOffsetDec;
			uint32_t curBufferOffset;

			/* Read-only */
			float maxFreqRatio;
			FAudioWaveFormatEx format; /* TODO: WaveFormatExtensible! */
			FAudioDecodeCallback decode;
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
			uint32_t outputSamples;
			float *inputCache;
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

void FAudio_INTERNAL_UpdateEngine(FAudio *audio, float *output);
void FAudio_INTERNAL_ResizeDecodeCache(FAudio *audio, uint32_t size);
void FAudio_INTERNAL_ResizeResampleCache(FAudio *audio, uint32_t size);
void FAudio_INTERNAL_SetDefaultMatrix(
	float *matrix,
	uint32_t srcChannels,
	uint32_t dstChannels
);
void FAudio_INTERNAL_InitConverterFunctions(uint8_t hasSSE2, uint8_t hasNEON);

#define DECODE_FUNC(type) \
	extern void FAudio_INTERNAL_Decode##type( \
		FAudioBuffer *buffer, \
		uint32_t curOffset, \
		float *decodeCache, \
		uint32_t samples, \
		FAudioWaveFormatEx *format \
	);
DECODE_FUNC(PCM8)
DECODE_FUNC(PCM16)
DECODE_FUNC(PCM32F)
DECODE_FUNC(MonoMSADPCM)
DECODE_FUNC(StereoMSADPCM)
#undef DECODE_FUNC

/* Platform Functions */

void FAudio_PlatformAddRef();
void FAudio_PlatformRelease();
void FAudio_PlatformInit(FAudio *audio);
void FAudio_PlatformQuit(FAudio *audio);
void FAudio_PlatformStart(FAudio *audio);
void FAudio_PlatformStop(FAudio *audio);
void FAudio_PlatformLockAudio(FAudio *audio);
void FAudio_PlatformUnlockAudio(FAudio *audio);

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
void* FAudio_realloc(void* ptr, size_t size);
void FAudio_free(void *ptr);
void FAudio_zero(void *ptr, size_t size);
void FAudio_memcpy(void *dst, const void *src, size_t size);
void FAudio_memmove(void *dst, void *src, size_t size);
int FAudio_memcmp(const void *ptr1, const void *ptr2, size_t size);

size_t FAudio_strlen(const char *ptr);
int FAudio_strcmp(const char *str1, const char *str2);
void FAudio_strlcpy(char *dst, const char *src, size_t len);

double FAudio_pow(double x, double y);
double FAudio_log10(double x);
double FAudio_sqrt(double x);
double FAudio_sin(double x);
double FAudio_cos(double x);
double FAudio_tan(double x);
double FAudio_acos(double x);
double FAudio_ceil(double x);
double FAudio_fabs(double x);

float FAudio_cosf(float x);
float FAudio_sinf(float x);
float FAudio_sqrtf(float x);
float FAudio_acosf(float x);
float FAudio_atan2f(float y, float x);
float FAudio_fabsf(float x);

uint32_t FAudio_timems();

#define FAudio_min(val1, val2) \
	(val1 < val2 ? val1 : val2)
#define FAudio_max(val1, val2) \
	(val1 > val2 ? val1 : val2)
#define FAudio_clamp(val, min, max) \
	(val > max ? max : (val < min ? min : val))
