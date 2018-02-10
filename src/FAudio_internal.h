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

typedef void (FAUDIOCALL * FAudioDecodeCallback)(
	FAudioBuffer *buffer,
	uint32_t curOffset,
	int16_t *decodeCache,
	uint32_t samples,
	FAudioWaveFormatEx *format,
	int16_t *msadpcmCache,
	uint16_t *msadpcmExtra
);

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
			int16_t msadpcmCache[1024];
			uint16_t msadpcmExtra;

			/* Resampler */
			float resampleFreqRatio;
			uint32_t resampleStep;
			uint32_t resampleOffset;
			int16_t pad[2]; /* Assuming stereo input */

			/* Read-only */
			float maxFreqRatio;
			FAudioWaveFormatEx format;
			FAudioDecodeCallback decode;
			FAudioVoiceCallback *callback;

			/* Dynamic */
			uint8_t active;
			float freqRatio;
			uint64_t totalSamples;
			FAudioBufferEntry *bufferList;
			uint32_t curBufferOffset;
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

void FAudio_INTERNAL_UpdateEngine(FAudio *audio, float *output);

#define DECODE_FUNC(type) \
	extern void FAudio_INTERNAL_Decode##type( \
		FAudioBuffer *buffer, \
		uint32_t curOffset, \
		int16_t *decodeCache, \
		uint32_t samples, \
		FAudioWaveFormatEx *format, \
		int16_t *msadpcmCache, \
		uint16_t *msadpcmExtra \
	);
DECODE_FUNC(MonoPCM8)
DECODE_FUNC(MonoPCM16)
DECODE_FUNC(MonoMSADPCM)
DECODE_FUNC(StereoPCM8)
DECODE_FUNC(StereoPCM16)
DECODE_FUNC(StereoMSADPCM)
#undef DECODE_FUNC

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
uint32_t FAudio_timems();

#define FAudio_min(val1, val2) \
	(val1 < val2 ? val1 : val2)
#define FAudio_max(val1, val2) \
	(val1 > val2 ? val1 : val2)
#define FAudio_clamp(val, min, max) \
	(val > max ? max : (val < min ? min : val))
