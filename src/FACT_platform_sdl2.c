/* FACT - XACT Reimplementation for FNA
 * Copyright 2009-2018 Ethan Lee and the MonoGame Team
 *
 * Released under the Microsoft Public License.
 * See LICENSE for details.
 */

#include "FACT_internal.h"

#include <SDL.h>

#define DEVICE_FORMAT AUDIO_F32
#define DEVICE_FORMAT_SIZE 4
#define DEVICE_FREQUENCY 48000
#define DEVICE_CHANNELS 2
#define DEVICE_BUFFERSIZE 4096

/* Internal Types */

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
	FACTWaveFormatExtensible format;
	FACTEngineEntry *engineList;
	FACTAudioDevice *next;
};

/* Globals */

FACTAudioDevice *devlist = NULL;

/* Resampling */

void FACT_INTERNAL_CalculateStep(FACTWave *wave)
{
	double stepd = (
		SDL_pow(2.0, wave->pitch / 1200.0) *
		(double) wave->parentBank->entries[wave->index].Format.nSamplesPerSec /
		(double) DEVICE_FREQUENCY
	);
	wave->resample.step = DOUBLE_TO_FIXED(stepd);
}

/* Mixer Thread */

void FACT_INTERNAL_MixCallback(void *userdata, Uint8 *stream, int len)
{
	FACTAudioDevice *device = (FACTAudioDevice*) userdata;
	FACTEngineEntry *engine = device->engineList;
	FACTSoundBank *sb;
	FACTWaveBank *wb;
	FACTCue *cue;
	FACTWave *wave;
	uint32_t samples;
	int16_t decodeCache[2][DEVICE_BUFFERSIZE + RESAMPLE_PADDING];
	float resampleCache[2][DEVICE_BUFFERSIZE];
	uint32_t timestamp;
	uint32_t i;
	float *out = (float*) stream;

	FACT_zero(stream, len);

	/* We want the timestamp to be uniform across all Cues.
	 * Oftentimes many Cues are played at once with the expectation
	 * that they will sync, so give them all the same timestamp
	 * so all the various actions will go together even if it takes
	 * an extra millisecond to get through the whole Cue list.
	 */
	timestamp = FACT_timems();

	while (engine != NULL)
	{
		FACT_INTERNAL_UpdateEngine(engine->engine);

		sb = engine->engine->sbList;
		while (sb != NULL)
		{
			cue = sb->cueList;
			while (cue != NULL)
			{
				FACT_INTERNAL_UpdateCue(cue, timestamp);
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

				/* Update stepping interval if needed */
				if (wave->pitch != wave->resample.pitch)
				{
					wave->resample.pitch = wave->pitch;
					FACT_INTERNAL_CalculateStep(wave);
				}

				/* Decode and resample wavedata */
				samples = FACT_INTERNAL_GetWave(
					wave,
					decodeCache[0],
					decodeCache[1],
					resampleCache[0],
					resampleCache[1],
					DEVICE_BUFFERSIZE
				);

				/* ... then mix, finally. */
				if (samples > 0)
				{
					for (i = 0; i < samples; i += 1)
					{
						out[i * 2] = FACT_clamp(
							out[i * 2] + (resampleCache[0][i] * wave->volume),
							-FACTVOLUME_MAX,
							FACTVOLUME_MAX
						);
						out[i * 2 + 1] = FACT_clamp(
							out[i * 2 + 1] + (resampleCache[wave->stereo][i] * wave->volume),
							-FACTVOLUME_MAX,
							FACTVOLUME_MAX
						);
					}
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

		/* Enforce a default device format */
		want.freq = DEVICE_FREQUENCY;
		want.format = DEVICE_FORMAT;
		want.channels = DEVICE_CHANNELS;
		want.silence = 0;
		want.samples = DEVICE_BUFFERSIZE;
		want.callback = FACT_INTERNAL_MixCallback;
		want.userdata = device;

		/* Open the device, finally */
		device->device = SDL_OpenAudioDevice(
			device->name,
			0,
			&want,
			&have,
			SDL_AUDIO_ALLOW_CHANNELS_CHANGE
		);
		if (device->device == 0)
		{
			FACT_free(device);
			FACT_free(entry);
			SDL_Log("%s\n", SDL_GetError());
			FACT_assert(0 && "Failed to open audio device!");
			return;
		}

		/* Write up the format */
		device->format.Format.wFormatTag = 1;
		device->format.Format.nChannels = have.channels;
		device->format.Format.nSamplesPerSec = have.freq;
		device->format.Format.nAvgBytesPerSec = have.freq * DEVICE_FORMAT_SIZE;
		device->format.Format.nBlockAlign = 0; /* ? */
		device->format.Format.wBitsPerSample = DEVICE_FORMAT_SIZE * 8;
		device->format.Format.cbSize = 0; /* ? */
		device->format.Samples.wValidBitsPerSample = DEVICE_FORMAT_SIZE * 8;
		device->format.dwChannelMask = SPEAKER_STEREO; /* FIXME */
		FACT_zero(&device->format.SubFormat, sizeof(FACTGUID)); /* ? */

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

void FACT_PlatformInitResampler(FACTWave *wave)
{
	FACT_INTERNAL_CalculateStep(wave);
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

void FACT_PlatformGetFinalMixFormat(
	FACTAudioEngine *pEngine,
	FACTWaveFormatExtensible *pFinalMixFormat
) {
	FACTEngineEntry *entry;
	FACTAudioDevice *dev = devlist;
	while (dev != NULL)
	{
		entry = dev->engineList;
		while (entry != NULL)
		{
			if (entry->engine == pEngine)
			{
				FACT_memcpy(
					pFinalMixFormat,
					&dev->format,
					sizeof(FACTWaveFormatExtensible)
				);
				return;
			}
			entry = entry->next;
		}
		dev = dev->next;
	}
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

float FACT_rng()
{
	return 0.0f; /* TODO: Random number generator */
}

uint32_t FACT_timems()
{
	return SDL_GetTicks();
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
