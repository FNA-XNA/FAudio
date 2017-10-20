/* FACT - XACT Reimplementation for FNA
 * Copyright 2009-2017 Ethan Lee and the MonoGame Team
 *
 * Released under the Microsoft Public License.
 * See LICENSE for details.
 */

#include "FACT_internal.h"

#include <SDL.h>

typedef struct FACTEngineDevice FACTEngineDevice;
struct FACTEngineDevice
{
	FACTEngineDevice *next;
	FACTAudioEngine *engine;
	SDL_AudioDeviceID device;
};

FACTEngineDevice *devlist = NULL;

void FACT_MixCallback(void *userdata, Uint8 *stream, int len)
{
	/* TODO */
	SDL_memset(stream, '\0', len);
}

void FACT_PlatformInitEngine(FACTAudioEngine *engine, wchar_t *id)
{
	SDL_AudioSpec want, have;
	FACTEngineDevice *device;
	FACTEngineDevice *list;
	const char *name;
	int renderer;

	if (!SDL_WasInit(SDL_INIT_AUDIO))
	{
		SDL_InitSubSystem(SDL_INIT_AUDIO);
	}

	/* Pair the upcoming audio device with the audio engine */
	device = (FACTEngineDevice*) FACT_malloc(sizeof(FACTEngineDevice));
	device->next = NULL;
	device->engine = engine;

	/* By default, let's aim for a 48KHz float stream */
	want.freq = 48000;
	want.format = AUDIO_F32;
	want.channels = 2; /* FIXME: Maybe aim for 5.1? */
	want.silence = 0;
	want.samples = 4096;
	want.callback = FACT_MixCallback;
	want.userdata = device;

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
	device->device = SDL_OpenAudioDevice(
		name,
		0,
		&want,
		&have,
		SDL_AUDIO_ALLOW_FORMAT_CHANGE
	);
	if (device->device == 0)
	{
		FACT_free(device);
		SDL_Log("%s\n", SDL_GetError());
		SDL_assert(0 && "Failed to open audio device!");
	}
	else
	{

		/* Keep track of which engine is on which device */
		if (devlist == NULL)
		{
			devlist = device;
		}
		else
		{
			list = devlist;
			while (list->next != NULL)
			{
				list = list->next;
			}
			list->next = device;
		}

		/* We're good to start! */
		SDL_PauseAudioDevice(device->device, 0);
	}
}

void FACT_PlatformCloseEngine(FACTAudioEngine *engine)
{
	FACTEngineDevice *dev = devlist;
	FACTEngineDevice *prev = devlist;
	while (dev != NULL)
	{
		if (dev->engine == engine)
		{
			/* FIXME: Multiple engines on one device...? */
			SDL_CloseAudioDevice(dev->device);

			if (dev == prev) /* First in list */
			{
				devlist = dev->next;
				FACT_free(dev);
				dev = devlist;
				prev = devlist;
			}
			else
			{
				prev->next = dev->next;
				FACT_free(dev);
				dev = prev->next;
			}
			break;
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
