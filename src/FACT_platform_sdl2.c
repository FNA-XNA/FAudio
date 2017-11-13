/* FACT - XACT Reimplementation for FNA
 * Copyright 2009-2017 Ethan Lee and the MonoGame Team
 *
 * Released under the Microsoft Public License.
 * See LICENSE for details.
 */

#include "FACT_internal.h"

#include <SDL.h>

#define DEVICE_FORMAT AUDIO_F32
#define DEVICE_FORMAT_SIZE 4
#define DEVICE_FREQUENCY 48000
#define DEVICE_CHANNELS 1
#define DEVICE_BUFFERSIZE 4096

/* Internal Types */

/* FIXME: Arbitrary 1-pre 1-post */
#define RESAMPLE_PADDING 1

typedef struct FACTEngineEntry FACTEngineEntry;
struct FACTEngineEntry
{
	FACTAudioEngine *engine;
	FACTEngineEntry *next;
};

typedef struct FACTAudioDevice FACTAudioDevice;
struct FACTAudioDevice
{
	const char *name;
	SDL_AudioDeviceID device;
	FACTEngineEntry *engineList;
	uint16_t decodeCache[DEVICE_BUFFERSIZE * 2 + RESAMPLE_PADDING * 2];
	float resampleCache[DEVICE_BUFFERSIZE * 2];
	FACTAudioDevice *next;
};

/* Globals */

FACTAudioDevice *devlist = NULL;

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

/* Callbacks for resampling wavedata, varies based on bit depth */
typedef void (FACTCALL * FACTResample)(
	float *dst,
	void *srcRaw,
	fixed32 *offset,
	fixed32 step,
	uint32_t dstLen
);
typedef void (FACTCALL * FACTFastConvert)(
	float *dst,
	void *srcRaw,
	uint32_t dstLen
);

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
	float padding[2][RESAMPLE_PADDING * 2];

	/* Resample func, varies based on bit depth */
	FACTResample resample;
	FACTFastConvert fastcvt;
} FACTResampleState;

void FACT_INTERNAL_CalculateStep(FACTWave *wave)
{
	FACTResampleState *state = (FACTResampleState*) wave->cvt;
	double stepd = (
		SDL_pow(2.0, wave->pitch / 1200.0) *
		(double) wave->parentBank->entries[wave->index].Format.nSamplesPerSec /
		(double) DEVICE_FREQUENCY
	);
	state->step = DOUBLE_TO_FIXED(stepd);
}

/* TODO: Something fancier than a linear resampler */
#define RESAMPLE_FUNC(type, div) \
	void FACT_INTERNAL_Resample_##type( \
		float *dst, \
		void *srcRaw, \
		fixed32 *offset, \
		fixed32 step, \
		uint32_t dstLen \
	) { \
		uint32_t i; \
		fixed32 cur = *offset & FIXED_FRACTION_MASK; \
		type *src = (type*) srcRaw; \
		for (i = 0; i < dstLen; i += 1) \
		{ \
			/* lerp, then convert to float value */ \
			dst[i] = (float) ( \
				src[0] + \
				(src[1] - src[0]) * FIXED_TO_DOUBLE(cur) \
			) / div; \
			/* Increment fraction offset by the stepping value */ \
			*offset += step; \
			cur += step; \
			/* Only increment the sample offset by integer values.
			 * Sometimes this will be 0 until cur accumulates
			 * enough steps, especially for "slow" rates.
			 */ \
			src += cur >> FIXED_PRECISION; \
			/* Now that any integer has been added, drop it.
			 * The offset pointer will preserve the total.
			 */ \
			cur &= FIXED_FRACTION_MASK; \
		} \
	}
RESAMPLE_FUNC(int8_t, 128.0f)
RESAMPLE_FUNC(int16_t, 32768.0f)
#undef RESAMPLE_FUNC

/* Fast path for input rate == output rate */
#define RESAMPLE_FUNC(type, div) \
	void FACT_INTERNAL_FastConvert_##type( \
		float *dst, \
		void *srcRaw, \
		uint32_t dstLen \
	) { \
		uint32_t i; \
		type *src = (type*) srcRaw; \
		for (i = 0; i < dstLen; i += 1) \
		{ \
			dst[i] = src[i] / div; \
		} \
	}
RESAMPLE_FUNC(int8_t, 128.0f)
RESAMPLE_FUNC(int16_t, 32768.0f)
#undef RESAMPLE_FUNC

/* Mixer Thread */

void FACT_MixCallback(void *userdata, Uint8 *stream, int len)
{
	FACTAudioDevice *device = (FACTAudioDevice*) userdata;
	FACTEngineEntry *engine = device->engineList;
	FACTSoundBank *sb;
	FACTWaveBank *wb;
	FACTCue *cue;
	FACTWave *wave;
	FACTResampleState *state;
	uint64_t sizeRequest;
	uint32_t decodeLength, resampleLength;

	FACT_zero(stream, len);

	while (engine != NULL)
	{
		FACT_INTERNAL_UpdateEngine(engine->engine);

		sb = engine->engine->sbList;
		while (sb != NULL)
		{
			cue = sb->cueList;
			while (cue != NULL)
			{
				FACT_INTERNAL_UpdateCue(cue);
				cue = cue->next;
			}
			sb = sb->next;
		}

		wb = engine->engine->wbList;
		while (wb != NULL)
		{
			wave = wb->waveList;
			while (wave != NULL)
			{
				/* Skip waves that aren't doing anything */
				if (	(wave->state & FACT_STATE_PAUSED) ||
					(wave->state & FACT_STATE_STOPPED)	)
				{
					wave = wave->next;
					continue;
				}

				/* TODO: Downmix stereo to mono for 3D audio,
				 * Otherwise have a special stereo mix path
				 */

				/* Grab resampler, update pitch if needed */
				state = (FACTResampleState*) wave->cvt;
				if (wave->pitch != state->pitch)
				{
					state->pitch = wave->pitch;
					FACT_INTERNAL_CalculateStep(wave);
				}

				/* If the sample rates match, skip a LOT of work
				 * and go straight to the mixer
				 */
				if (state->step == FIXED_ONE)
				{
					decodeLength = wave->decode(
						wave,
						device->decodeCache,
						DEVICE_BUFFERSIZE
					);
					state->fastcvt(
						device->resampleCache,
						device->decodeCache,
						decodeLength
					);
					resampleLength = decodeLength;
					state->offset += decodeLength * FIXED_ONE;
					goto mixjmp;
				}

				/* The easy part is just multiplying the final
				 * output size with the step to get the "real"
				 * buffer size. But we also need to ceil() to
				 * get the extra sample needed for interpolating
				 * past the "end" of the unresampled buffer.
				 */
				sizeRequest = DEVICE_BUFFERSIZE * state->step;
				sizeRequest += (
					/* If frac > 0, int goes up by one... */
					(state->offset + FIXED_FRACTION_MASK) &
					/* ... then we chop off anything left */
					FIXED_FRACTION_MASK
				);
				sizeRequest >>= FIXED_PRECISION;

				/* Only add half the padding length!
				 * For the starting buffer, we have no pre-pad,
				 * for all remaining buffers we memcpy the end
				 * and that becomes the new pre-pad.
				 */
				sizeRequest += RESAMPLE_PADDING;

				/* FIXME: Can high sample rates ruin this? */
				decodeLength = (uint32_t) sizeRequest;
				FACT_assert(decodeLength == sizeRequest);

				/* Decode... */
				if (state->offset == 0)
				{
					decodeLength = wave->decode(
						wave,
						device->decodeCache,
						decodeLength
					);
				}
				else
				{
					/* Copy the end to the start first! */
					FACT_memcpy(
						device->decodeCache,
						(
							device->decodeCache +
							(DEVICE_BUFFERSIZE * 2) +
							RESAMPLE_PADDING
						),
						RESAMPLE_PADDING * 2
					);
					/* Don't overwrite the start! */
					decodeLength = wave->decode(
						wave,
						device->decodeCache + RESAMPLE_PADDING,
						decodeLength
					);
				}

				/* Now that we have the raw samples, now we have
				 * to reverse some of the math to get the real
				 * output size (read: Drop the padding numbers)
				 */
				sizeRequest = decodeLength - RESAMPLE_PADDING;
				/* uint32_t to fixed32 */
				sizeRequest <<= FIXED_PRECISION;
				/* Division also turns fixed32 into uint32_t */
				sizeRequest /= state->step;

				/* ... then Resample... */
				resampleLength = (uint32_t) sizeRequest;
				FACT_assert(resampleLength == sizeRequest);
				state->resample(
					device->resampleCache,
					device->decodeCache,
					&state->offset,
					state->step,
					resampleLength
				);

				/* ... then Mix, finally. */
mixjmp:
				if (resampleLength > 0)
				{
					/* TODO: Volume mixing goes beyond what
					 * MixAudioFormat can do. We need our own mix!
					 * -flibit
					 */
					SDL_MixAudioFormat(
						stream,
						(Uint8*) device->resampleCache,
						DEVICE_FORMAT,
						resampleLength * 4,
						wave->volume * SDL_MIX_MAXVOLUME
					);
				}
				else
				{
					/* Looks like we're out, stop! */
					wave->state |= FACT_STATE_STOPPED;
					wave->state &= ~FACT_STATE_PLAYING;
				}

				wave = wave->next;
			}
			wb = wb->next;
		}

		engine = engine->next;
	}
}

/* Platform Functions */

void FACT_PlatformInitEngine(FACTAudioEngine *engine, wchar_t *id)
{
	FACTEngineEntry *entry, *entryList;
	FACTAudioDevice *device, *deviceList;
	SDL_AudioSpec want, have;
	const char *name;
	int32_t renderer;

	if (!SDL_WasInit(SDL_INIT_AUDIO))
	{
		SDL_InitSubSystem(SDL_INIT_AUDIO);
	}

	/* The entry to be added to the audio device */
	entry = (FACTEngineEntry*) FACT_malloc(sizeof(FACTEngineEntry));
	entry->engine = engine;
	entry->next = NULL;

	/* Use the device that the engine tells us to use */
	if (id == NULL)
	{
		name = NULL;
	}
	else
	{
		/* FIXME: wchar_t is an asshole */
		renderer = id[0] - L'0';
		if (	renderer < 0 ||
			renderer > SDL_GetNumAudioDevices(0)	)
		{
			renderer = 0;
		}
		name = SDL_GetAudioDeviceName(renderer, 0);
	}

	/* Check to see if the device is already opened */
	device = devlist;
	while (device != NULL)
	{
		if (FACT_strcmp(device->name, name) == 0)
		{
			break;
		}
		device = device->next;
	}

	/* Create a new device if the requested one is not in use yet */
	if (device == NULL)
	{
		/* Allocate a new device container*/
		device = (FACTAudioDevice*) FACT_malloc(
			sizeof(FACTAudioDevice)
		);
		device->name = name;
		device->engineList = entry;
		device->next = NULL;

		/* Enforce a default device format */
		want.freq = DEVICE_FREQUENCY;
		want.format = DEVICE_FORMAT;
		want.channels = DEVICE_CHANNELS;
		want.silence = 0;
		want.samples = DEVICE_BUFFERSIZE;
		want.callback = FACT_MixCallback;
		want.userdata = device;

		/* Open the device, finally */
		device->device = SDL_OpenAudioDevice(
			device->name,
			0,
			&want,
			&have,
			SDL_AUDIO_ALLOW_FORMAT_CHANGE
		);
		if (device->device == 0)
		{
			FACT_free(device);
			FACT_free(entry);
			SDL_Log("%s\n", SDL_GetError());
			FACT_assert(0 && "Failed to open audio device!");
			return;
		}

		/* Add to the device list */
		if (devlist == NULL)
		{
			devlist = device;
		}
		else
		{
			deviceList = devlist;
			while (deviceList->next != NULL)
			{
				deviceList = deviceList->next;
			}
			deviceList->next = device;
		}

		/* We're good to start! */
		SDL_PauseAudioDevice(device->device, 0);
	}
	else /* Just add us to the existing device */
	{
		entryList = device->engineList;
		while (entryList->next != NULL)
		{
			entryList = entryList->next;
		}
		entryList->next = entry;
	}
}

void FACT_PlatformCloseEngine(FACTAudioEngine *engine)
{
	FACTEngineEntry *entry, *prevEntry;
	FACTAudioDevice *dev = devlist;
	FACTAudioDevice *prevDev = devlist;
	uint8_t found = 0;
	while (dev != NULL)
	{
		entry = dev->engineList;
		prevEntry = dev->engineList;
		while (entry != NULL)
		{
			if (entry->engine == engine)
			{
				if (entry == prevEntry) /* First in list */
				{
					dev->engineList = entry->next;
					FACT_free(entry);
					entry = dev->engineList;
					prevEntry = dev->engineList;
				}
				else
				{
					prevEntry->next = entry->next;
					FACT_free(entry);
					entry = prevEntry->next;
				}
				found = 1;
				break;
			}
			else
			{
				prevEntry = entry;
				entry = entry->next;
			}
		}

		if (found)
		{
			/* No more engines? We're done with this device. */
			if (dev->engineList == NULL)
			{
				SDL_CloseAudioDevice(dev->device);
				if (dev == prevDev) /* First in list */
				{
					devlist = dev->next;
				}
				else
				{
					prevDev = dev->next;
				}
				FACT_free(dev);
			}
			dev = NULL;
		}
		else
		{
			prevDev = dev;
			dev = dev->next;
		}
	}

	/* No more devices? We're done here... */
	if (devlist == NULL)
	{
		SDL_QuitSubSystem(SDL_INIT_AUDIO);
	}
}

void FACT_PlatformInitConverter(FACTWave *wave)
{
	FACTWaveBankMiniWaveFormat *fmt = &wave->parentBank->entries[wave->index].Format;
	FACTResampleState *state = (FACTResampleState*) FACT_malloc(
		sizeof(FACTResampleState)
	);
	FACT_zero(state, sizeof(FACTResampleState));
	state->pitch = wave->pitch;
	if (fmt->wFormatTag == 0x0 && fmt->wBitsPerSample == 0)
	{
		state->resample = FACT_INTERNAL_Resample_int8_t;
		state->fastcvt = FACT_INTERNAL_FastConvert_int8_t;
	}
	else
	{
		state->resample = FACT_INTERNAL_Resample_int16_t;
		state->fastcvt = FACT_INTERNAL_FastConvert_int16_t;
	}
	wave->cvt = (FACTPlatformConverter*) state;
	FACT_INTERNAL_CalculateStep(wave);
}

void FACT_PlatformCloseConverter(FACTWave *wave)
{
	FACT_free(wave->cvt);
}

uint16_t FACT_PlatformGetRendererCount()
{
	return SDL_GetNumAudioDevices(0);
}

void FACT_PlatformGetRendererDetails(
	uint16_t index,
	FACTRendererDetails *details
) {
	const char *name;
	size_t len, i;

	FACT_zero(details, sizeof(FACTRendererDetails));
	if (index > SDL_GetNumAudioDevices(0))
	{
		return;
	}

	/* FIXME: wchar_t is an asshole */
	details->rendererID[0] = L'0' + index;
	name = SDL_GetAudioDeviceName(index, 0);
	len = SDL_min(FACT_strlen(name), 0xFF);
	for (i = 0; i < len; i += 1)
	{
		details->displayName[i] = name[i];
	}
	details->defaultDevice = (index == 0);
}

void* FACT_malloc(size_t size)
{
	return SDL_malloc(size);
}

void FACT_free(void *ptr)
{
	SDL_free(ptr);
}

void FACT_zero(void *ptr, size_t size)
{
	SDL_memset(ptr, '\0', size);
}

void FACT_memcpy(void *dst, void *src, size_t size)
{
	SDL_memcpy(dst, src, size);
}

void FACT_memmove(void *dst, void *src, size_t size)
{
	SDL_memmove(dst, src, size);
}

size_t FACT_strlen(const char *ptr)
{
	return SDL_strlen(ptr);
}

int FACT_strcmp(const char *str1, const char *str2)
{
	return SDL_strcmp(str1, str2);
}

void FACT_strlcpy(char *dst, const char *src, size_t len)
{
	SDL_strlcpy(dst, src, len);
}

FACTIOStream* FACT_fopen(const char *path)
{
	FACTIOStream *io = (FACTIOStream*) SDL_malloc(
		sizeof(FACTIOStream)
	);
	SDL_RWops *rwops = SDL_RWFromFile(path, "rb");
	io->data = rwops;
	io->read = (FACT_readfunc) rwops->read;
	io->seek = (FACT_seekfunc) rwops->seek;
	io->close = (FACT_closefunc) rwops->close;
	return io;
}

FACTIOStream* FACT_memopen(void *mem, int len)
{
	FACTIOStream *io = (FACTIOStream*) SDL_malloc(
		sizeof(FACTIOStream)
	);
	SDL_RWops *rwops = SDL_RWFromMem(mem, len);
	io->data = rwops;
	io->read = (FACT_readfunc) rwops->read;
	io->seek = (FACT_seekfunc) rwops->seek;
	io->close = (FACT_closefunc) rwops->close;
	return io;
}

void FACT_close(FACTIOStream *io)
{
	io->close(io->data);
	SDL_free(io);
}
