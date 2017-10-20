/* FACT - XACT Reimplementation for FNA
 * Copyright 2009-2017 Ethan Lee and the MonoGame Team
 *
 * Released under the Microsoft Public License.
 * See LICENSE for details.
 */

#include "FACT_internal.h"

#include <SDL.h>

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
	FACTAudioDevice *next;
};

FACTAudioDevice *devlist = NULL;

void FACT_MixCallback(void *userdata, Uint8 *stream, int len)
{
	FACTAudioDevice *device = (FACTAudioDevice*) userdata;
	FACTEngineEntry *engine = device->engineList;
	FACTSoundBank *sb;
	FACTWaveBank *wb;
	FACTCue *cue;
	FACTWave *wave;

	/* FIXME: Can we avoid zeroing every time? Blech! */
	FACT_zero(stream, len);

	while (engine != NULL)
	{
		sb = engine->engine->sbList;
		while (sb != NULL)
		{
			cue = sb->cueList;
			while (cue != NULL)
			{
				/* TODO: Updates! */
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
				/* TODO: Mixing! */
				wave = wave->next;
			}
			wb = wb->next;
		}

		engine = engine->next;
	}
}

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

		/* By default, let's aim for a 48KHz float stream */
		want.freq = 48000;
		want.format = AUDIO_F32;
		want.channels = 2; /* FIXME: Maybe aim for 5.1? */
		want.silence = 0;
		want.samples = 4096;
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
			SDL_assert(0 && "Failed to open audio device!");
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
