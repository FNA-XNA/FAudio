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

#include "FAudio_internal.h"

#include <SDL.h>

/* Internal Types */

typedef struct FAudioEntry FAudioEntry;
struct FAudioEntry
{
	FAudio *audio;
	FAudioEntry *next;
};

typedef struct FAudioPlatformDevice FAudioPlatformDevice;
struct FAudioPlatformDevice
{
	const char *name;
	uint32_t bufferSize;
	SDL_AudioDeviceID device;
	FAudioWaveFormatExtensible format;
	FAudioEntry *engineList;
	FAudioPlatformDevice *next;
};

/* Globals */

FAudioPlatformDevice *devlist = NULL;

/* Mixer Thread */

void FAudio_INTERNAL_MixCallback(void *userdata, Uint8 *stream, int len)
{
	FAudioPlatformDevice *device = (FAudioPlatformDevice*) userdata;
	FAudioEntry *audio;

	FAudio_zero(stream, len);
	audio = device->engineList;
	while (audio != NULL)
	{
		FAudio_INTERNAL_UpdateEngine(
			audio->audio,
			(float*) stream
		);
		audio = audio->next;
	}
}

/* Platform Functions */

void FAudio_PlatformAddRef()
{
	/* SDL tracks ref counts for each subsystem */
	SDL_InitSubSystem(SDL_INIT_AUDIO);
}

void FAudio_PlatformRelease()
{
	/* SDL tracks ref counts for each subsystem */
	SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

void FAudio_PlatformInit(FAudio *audio)
{
	FAudioEntry *entry, *entryList;
	FAudioPlatformDevice *device, *deviceList;
	SDL_AudioSpec want, have;
	const char *name;

	/* The entry to be added to the audio device */
	entry = (FAudioEntry*) FAudio_malloc(sizeof(FAudioEntry));
	entry->audio = audio;
	entry->next = NULL;

	/* Use the device that the engine tells us to use */
	name = SDL_GetAudioDeviceName(audio->master->master.deviceIndex, 0);

	/* Check to see if the device is already opened */
	device = devlist;
	while (device != NULL)
	{
		if (FAudio_strcmp(device->name, name) == 0)
		{
			break;
		}
		device = device->next;
	}

	/* Create a new device if the requested one is not in use yet */
	if (device == NULL)
	{
		/* Allocate a new device container*/
		device = (FAudioPlatformDevice*) FAudio_malloc(
			sizeof(FAudioPlatformDevice)
		);
		device->name = name;
		device->engineList = entry;
		device->next = NULL;

		/* Enforce a default device format.
		 * FIXME: The way SDL picks device defaults is fucking stupid.
		 * It's basically just a bunch of hardcoding and the values used
		 * are just awful:
		 *
		 * https://hg.libsdl.org/SDL/file/c3446901fc1c/src/audio/SDL_audio.c#l1087
		 *
		 * We should step in and see if we can't pull this from the OS.
		 * -flibit
		 */
		want.freq = audio->master->master.inputSampleRate;
		want.format = AUDIO_F32;
		want.channels = audio->master->master.inputChannels;
		want.silence = 0;
		want.samples = 1024;
		want.callback = FAudio_INTERNAL_MixCallback;
		want.userdata = device;

		/* Open the device, finally */
		device->device = SDL_OpenAudioDevice(
			device->name,
			0,
			&want,
			&have,
			(
				SDL_AUDIO_ALLOW_CHANNELS_CHANGE |
				SDL_AUDIO_ALLOW_FREQUENCY_CHANGE
			)
		);
		if (device->device == 0)
		{
			FAudio_free(device);
			FAudio_free(entry);
			SDL_Log("%s\n", SDL_GetError());
			FAudio_assert(0 && "Failed to open audio device!");
			return;
		}

		/* Write up the format */
		device->format.Format.wFormatTag = 1;
		device->format.Format.nChannels = have.channels;
		device->format.Format.nSamplesPerSec = have.freq;
		device->format.Format.nAvgBytesPerSec = have.freq * 4;
		device->format.Format.nBlockAlign = 0; /* ? */
		device->format.Format.wBitsPerSample = 32;
		device->format.Format.cbSize = 0; /* ? */
		device->format.Samples.wValidBitsPerSample = 32;
		if (have.channels == 1)
		{
			device->format.dwChannelMask = SPEAKER_MONO;
		}
		else if (have.channels == 2)
		{
			device->format.dwChannelMask = SPEAKER_STEREO;
		}
		else if (have.channels == 3)
		{
			device->format.dwChannelMask = SPEAKER_2POINT1;
		}
		else if (have.channels == 4)
		{
			device->format.dwChannelMask = SPEAKER_QUAD;
		}
		else if (have.channels == 5)
		{
			device->format.dwChannelMask = SPEAKER_4POINT1;
		}
		else if (have.channels == 6)
		{
			device->format.dwChannelMask = SPEAKER_5POINT1;
		}
		else if (have.channels == 8)
		{
			device->format.dwChannelMask = SPEAKER_7POINT1;
		}
		else
		{
			FAudio_assert(0 && "Unrecognized speaker layout!");
		}
		FAudio_zero(&device->format.SubFormat, sizeof(FAudioGUID)); /* ? */
		device->bufferSize = have.samples;

		/* Give the output format to the engine */
		audio->updateSize = device->bufferSize;
		audio->mixFormat = &device->format;

		/* Maybe also give it to the master voice */
		if (audio->master->master.inputChannels == 0)
		{
			audio->master->master.inputChannels = have.channels;
		}
		if (audio->master->master.inputSampleRate == 0)
		{
			audio->master->master.inputSampleRate = have.freq;
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
	}
	else /* Just add us to the existing device */
	{
		/* But give us the output format first! */
		audio->updateSize = device->bufferSize;
		audio->mixFormat = &device->format;

		/* Someone else was here first, you get their format! */
		audio->master->master.inputChannels =
			device->format.Format.nChannels;
		audio->master->master.inputSampleRate =
			device->format.Format.nSamplesPerSec;

		entryList = device->engineList;
		while (entryList->next != NULL)
		{
			entryList = entryList->next;
		}
		entryList->next = entry;
	}
}

void FAudio_PlatformQuit(FAudio *audio)
{
	FAudioEntry *entry, *prevEntry;
	FAudioPlatformDevice *dev = devlist;
	FAudioPlatformDevice *prevDev = devlist;
	uint8_t found = 0;
	while (dev != NULL)
	{
		entry = dev->engineList;
		prevEntry = dev->engineList;
		while (entry != NULL)
		{
			if (entry->audio == audio)
			{
				if (entry == prevEntry) /* First in list */
				{
					dev->engineList = entry->next;
					FAudio_free(entry);
					entry = dev->engineList;
					prevEntry = dev->engineList;
				}
				else
				{
					prevEntry->next = entry->next;
					FAudio_free(entry);
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
				FAudio_free(dev);
			}
			dev = NULL;
		}
		else
		{
			prevDev = dev;
			dev = dev->next;
		}
	}
}

void FAudio_PlatformStart(FAudio *audio)
{
	FAudioEntry *entry;
	FAudioPlatformDevice *dev = devlist;
	while (dev != NULL)
	{
		entry = dev->engineList;
		while (entry != NULL)
		{
			if (entry->audio == audio)
			{
				SDL_PauseAudioDevice(dev->device, 0);
				return;
			}
			entry = entry->next;
		}
		dev = dev->next;
	}
}

void FAudio_PlatformStop(FAudio *audio)
{
	FAudioEntry *entry;
	FAudioPlatformDevice *dev = devlist;
	while (dev != NULL)
	{
		entry = dev->engineList;
		while (entry != NULL)
		{
			if (entry->audio == audio)
			{
				SDL_PauseAudioDevice(dev->device, 1);
				return;
			}
			entry = entry->next;
		}
		dev = dev->next;
	}
}

void FAudio_PlatformLockAudio(FAudio *audio)
{
	FAudioEntry *entry;
	FAudioPlatformDevice *dev = devlist;
	while (dev != NULL)
	{
		entry = dev->engineList;
		while (entry != NULL)
		{
			if (entry->audio == audio)
			{
				SDL_LockAudioDevice(dev->device);
				return;
			}
			entry = entry->next;
		}
		dev = dev->next;
	}
}

void FAudio_PlatformUnlockAudio(FAudio *audio)
{
	FAudioEntry *entry;
	FAudioPlatformDevice *dev = devlist;
	while (dev != NULL)
	{
		entry = dev->engineList;
		while (entry != NULL)
		{
			if (entry->audio == audio)
			{
				SDL_UnlockAudioDevice(dev->device);
				return;
			}
			entry = entry->next;
		}
		dev = dev->next;
	}
}

uint32_t FAudio_PlatformGetDeviceCount()
{
	return SDL_GetNumAudioDevices(0);
}

void FAudio_PlatformGetDeviceDetails(
	uint32_t index,
	FAudioDeviceDetails *details
) {
	const char *name;
	size_t len, i;

	FAudio_zero(details, sizeof(FAudioDeviceDetails));
	if (index > FAudio_PlatformGetDeviceCount())
	{
		return;
	}

	/* FIXME: wchar_t is an asshole */
	details->DeviceID[0] = L'0' + index;
	name = SDL_GetAudioDeviceName(index, 0);
	len = FAudio_min(FAudio_strlen(name), 0xFF);
	for (i = 0; i < len; i += 1)
	{
		details->DisplayName[i] = name[i];
	}

	details->Role = (index == 0) ?
		GlobalDefaultDevice :
		NotDefaultDevice;

	/* FIXME: SDL needs a device format query function! */
	details->OutputFormat.dwChannelMask = SPEAKER_STEREO;
	details->OutputFormat.Format.nChannels = 2;
	details->OutputFormat.Format.nSamplesPerSec = 48000;
	details->OutputFormat.Samples.wValidBitsPerSample = 4;
}

FAudioPlatformFixedRateSRC FAudio_PlatformInitFixedRateSRC(
	uint32_t channels,
	uint32_t inputRate,
	uint32_t outputRate
) {
	return (FAudioPlatformFixedRateSRC) SDL_NewAudioStream(
		AUDIO_F32,
		channels,
		inputRate,
		AUDIO_F32,
		channels,
		outputRate
	);
}

void FAudio_PlatformCloseFixedRateSRC(FAudioPlatformFixedRateSRC resampler)
{
	SDL_FreeAudioStream((SDL_AudioStream*) resampler);
}

uint32_t FAudio_PlatformResample(
	FAudioPlatformFixedRateSRC resampler,
	float *input,
	uint32_t inLen,
	float *output,
	uint32_t outLen
) {
	SDL_AudioStream *stream = (SDL_AudioStream*) resampler;
	SDL_AudioStreamPut(stream, input, inLen * sizeof(float));
	return SDL_AudioStreamGet(
		stream,
		output,
		outLen * sizeof(float)
	) / sizeof(float);
}

/* stdlib Functions */

void* FAudio_malloc(size_t size)
{
	return SDL_malloc(size);
}

void* FAudio_realloc(void* ptr, size_t size)
{
	return SDL_realloc(ptr, size);
}

void FAudio_free(void *ptr)
{
	SDL_free(ptr);
}

void FAudio_zero(void *ptr, size_t size)
{
	SDL_memset(ptr, '\0', size);
}

void FAudio_memcpy(void *dst, const void *src, size_t size)
{
	SDL_memcpy(dst, src, size);
}

void FAudio_memmove(void *dst, void *src, size_t size)
{
	SDL_memmove(dst, src, size);
}

size_t FAudio_strlen(const char *ptr)
{
	return SDL_strlen(ptr);
}

int FAudio_strcmp(const char *str1, const char *str2)
{
	return SDL_strcmp(str1, str2);
}

void FAudio_strlcpy(char *dst, const char *src, size_t len)
{
	SDL_strlcpy(dst, src, len);
}

double FAudio_pow(double x, double y)
{
	return SDL_pow(x, y);
}

double FAudio_log10(double x)
{
	return SDL_log10(x);
}

double FAudio_sqrt(double x)
{
	return SDL_sqrt(x);
}

double FAudio_acos(double x)
{
	return SDL_acos(x);
}

double FAudio_ceil(double x)
{
	return SDL_ceil(x);
}

double FAudio_fabs(double x)
{
	return SDL_fabs(x);
}


float FAudio_cosf(float x)
{
	return SDL_cosf(x);
}

float FAudio_sinf(float x)
{
	return SDL_sinf(x);
}

float FAudio_sqrtf(float x)
{
	return SDL_sqrtf(x);
}

float FAudio_acosf(float x)
{
	return SDL_acosf(x);
}

float FAudio_atan2f(float y, float x)
{
	return SDL_atan2f(y, x);
}

float FAudio_fabsf(float x)
{
	return SDL_fabsf(x);
}

uint32_t FAudio_timems()
{
	return SDL_GetTicks();
}

/* FAudio I/O */

FAudioIOStream* FAudio_fopen(const char *path)
{
	FAudioIOStream *io = (FAudioIOStream*) SDL_malloc(
		sizeof(FAudioIOStream)
	);
	SDL_RWops *rwops = SDL_RWFromFile(path, "rb");
	io->data = rwops;
	io->read = (FAudio_readfunc) rwops->read;
	io->seek = (FAudio_seekfunc) rwops->seek;
	io->close = (FAudio_closefunc) rwops->close;
	return io;
}

FAudioIOStream* FAudio_memopen(void *mem, int len)
{
	FAudioIOStream *io = (FAudioIOStream*) SDL_malloc(
		sizeof(FAudioIOStream)
	);
	SDL_RWops *rwops = SDL_RWFromMem(mem, len);
	io->data = rwops;
	io->read = (FAudio_readfunc) rwops->read;
	io->seek = (FAudio_seekfunc) rwops->seek;
	io->close = (FAudio_closefunc) rwops->close;
	return io;
}

uint8_t* FAudio_memptr(FAudioIOStream *io, size_t offset)
{
	SDL_RWops *rwops = (SDL_RWops*) io->data;
	FAudio_assert(rwops->type == SDL_RWOPS_MEMORY);
	return rwops->hidden.mem.base + offset;
}

void FAudio_close(FAudioIOStream *io)
{
	io->close(io->data);
	SDL_free(io);
}
