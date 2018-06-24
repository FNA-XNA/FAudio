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

typedef struct FAudioPlatformDevice
{
	const char *name;
	uint32_t bufferSize;
	SDL_AudioDeviceID device;
	FAudioWaveFormatExtensible format;
	LinkedList *engineList;
	FAudioSpinLock engineLock;
} FAudioPlatformDevice;

/* Globals */

LinkedList *devlist = NULL;
FAudioSpinLock devlock = 0;

/* Mixer Thread */

void FAudio_INTERNAL_MixCallback(void *userdata, Uint8 *stream, int len)
{
	FAudioPlatformDevice *device = (FAudioPlatformDevice*) userdata;
	LinkedList *audio;

	FAudio_zero(stream, len);
	audio = device->engineList;
	while (audio != NULL)
	{
		FAudio_INTERNAL_UpdateEngine(
			(FAudio*) audio->entry,
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
	FAudio_INTERNAL_InitConverterFunctions(
		SDL_HasSSE2(),
		SDL_HasNEON()
	);
}

void FAudio_PlatformRelease()
{
	/* SDL tracks ref counts for each subsystem */
	SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

void FAudio_PlatformInit(FAudio *audio, uint32_t deviceIndex)
{
	LinkedList *deviceList;
	FAudioPlatformDevice *device;
	SDL_AudioSpec want, have;
	const char *name;

	/* Use the device that the engine tells us to use, then check to see if
	 * another instance has opened the device.
	 */
	if (deviceIndex == 0)
	{
		name = NULL;
		deviceList = devlist;
		while (deviceList != NULL)
		{
			if (((FAudioPlatformDevice*) deviceList->entry)->name == NULL)
			{
				break;
			}
			deviceList = deviceList->next;
		}
	}
	else
	{
		name = SDL_GetAudioDeviceName(deviceIndex - 1, 0);
		deviceList = devlist;
		while (deviceList != NULL)
		{
			if (FAudio_strcmp(((FAudioPlatformDevice*) deviceList->entry)->name, name) == 0)
			{
				break;
			}
			deviceList = deviceList->next;
		}
	}

	/* Create a new device if the requested one is not in use yet */
	if (deviceList == NULL)
	{
		/* Allocate a new device container*/
		device = (FAudioPlatformDevice*) FAudio_malloc(
			sizeof(FAudioPlatformDevice)
		);
		device->name = name;
		device->engineList = NULL;
		device->engineLock = 0;
		LinkedList_AddEntry(
			&device->engineList,
			audio,
			&device->engineLock
		);

		/* Build the device format */
		want.freq = audio->master->master.inputSampleRate;
		want.format = AUDIO_F32;
		want.channels = audio->master->master.inputChannels;
		want.silence = 0;
		want.samples = 1024;
		want.callback = FAudio_INTERNAL_MixCallback;
		want.userdata = device;

		/* Open the device, finally. */
		device->device = SDL_OpenAudioDevice(
			device->name,
			0,
			&want,
			&have,
			0
		);
		if (device->device == 0)
		{
			LinkedList_RemoveEntry(
				&device->engineList,
				audio,
				&device->engineLock
			);
			FAudio_free(device);
			SDL_Log("%s\n", SDL_GetError());
			FAudio_assert(0 && "Failed to open audio device!");
			return;
		}

		/* Write up the format */
		device->format.Samples.wValidBitsPerSample = 32;
		device->format.Format.wBitsPerSample = 32;
		device->format.Format.wFormatTag = 3;
		device->format.Format.nChannels = have.channels;
		device->format.Format.nSamplesPerSec = have.freq;
		device->format.Format.nBlockAlign = (
			device->format.Format.nChannels *
			(device->format.Format.wBitsPerSample / 8)
		);
		device->format.Format.nAvgBytesPerSec = (
			device->format.Format.nSamplesPerSec *
			device->format.Format.nBlockAlign
		);
		device->format.Format.cbSize = 0;
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

		/* Also give some info to the master voice */
		audio->master->master.inputChannels = have.channels;
		audio->master->master.inputSampleRate = have.freq;

		/* Add to the device list */
		LinkedList_AddEntry(&devlist, device, &devlock);
	}
	else /* Just add us to the existing device */
	{
		device = (FAudioPlatformDevice*) deviceList->entry;

		/* But give us the output format first! */
		audio->updateSize = device->bufferSize;
		audio->mixFormat = &device->format;

		/* Someone else was here first, you get their format! */
		audio->master->master.inputChannels =
			device->format.Format.nChannels;
		audio->master->master.inputSampleRate =
			device->format.Format.nSamplesPerSec;

		LinkedList_AddEntry(
			&device->engineList,
			audio,
			&device->engineLock
		);
	}
}

void FAudio_PlatformQuit(FAudio *audio)
{
	FAudioPlatformDevice *device;
	LinkedList *dev, *entry;
	uint8_t found = 0;

	dev = devlist;
	while (dev != NULL)
	{
		device = (FAudioPlatformDevice*) dev->entry;
		entry = device->engineList;
		while (entry != NULL)
		{
			if (entry->entry == audio)
			{
				found = 1;
				break;
			}
			entry = entry->next;
		}

		if (found)
		{
			LinkedList_RemoveEntry(
				&device->engineList,
				audio,
				&device->engineLock
			);

			if (device->engineList == NULL)
			{
				SDL_CloseAudioDevice(
					device->device
				);
				LinkedList_RemoveEntry(
					&devlist,
					device,
					&devlock
				);
				FAudio_free(device);
			}

			return;
		}
		dev = dev->next;
	}
}

void FAudio_PlatformStart(FAudio *audio)
{
	LinkedList *dev, *entry;

	dev = devlist;
	while (dev != NULL)
	{
		entry = ((FAudioPlatformDevice*) dev->entry)->engineList;
		while (entry != NULL)
		{
			if (((FAudio*) entry->entry) == audio)
			{
				SDL_PauseAudioDevice(
					((FAudioPlatformDevice*) dev->entry)->device,
					0
				);
				return;
			}
			entry = entry->next;
		}
		dev = dev->next;
	}
}

void FAudio_PlatformStop(FAudio *audio)
{
	LinkedList *dev, *entry;

	dev = devlist;
	while (dev != NULL)
	{
		entry = ((FAudioPlatformDevice*) dev->entry)->engineList;
		while (entry != NULL)
		{
			if (((FAudio*) entry->entry) == audio)
			{
				SDL_PauseAudioDevice(
					((FAudioPlatformDevice*) dev->entry)->device,
					1
				);
				return;
			}
			entry = entry->next;
		}
		dev = dev->next;
	}
}

void FAudio_PlatformLockAudio(FAudio *audio)
{
	LinkedList *dev, *entry;

	dev = devlist;
	while (dev != NULL)
	{
		entry = ((FAudioPlatformDevice*) dev->entry)->engineList;
		while (entry != NULL)
		{
			if (((FAudio*) entry->entry) == audio)
			{
				SDL_LockAudioDevice(
					((FAudioPlatformDevice*) dev->entry)->device
				);
				return;
			}
			entry = entry->next;
		}
		dev = dev->next;
	}
}

void FAudio_PlatformUnlockAudio(FAudio *audio)
{
	LinkedList *dev, *entry;

	dev = devlist;
	while (dev != NULL)
	{
		entry = ((FAudioPlatformDevice*) dev->entry)->engineList;
		while (entry != NULL)
		{
			if (((FAudio*) entry->entry) == audio)
			{
				SDL_UnlockAudioDevice(
					((FAudioPlatformDevice*) dev->entry)->device
				);
				return;
			}
			entry = entry->next;
		}
		dev = dev->next;
	}
}

uint32_t FAudio_PlatformGetDeviceCount()
{
	return SDL_GetNumAudioDevices(0) + 1;
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
	if (index == 0)
	{
		name = "Default Device";
		details->Role = GlobalDefaultDevice;
	}
	else
	{
		name = SDL_GetAudioDeviceName(index - 1, 0);
		details->Role = NotDefaultDevice;
	}
	len = FAudio_min(FAudio_strlen(name), 0xFF);
	for (i = 0; i < len; i += 1)
	{
		details->DisplayName[i] = name[i];
	}

	/* FIXME: SDL needs a device format query function! */
	details->OutputFormat.dwChannelMask = SPEAKER_STEREO;
	details->OutputFormat.Samples.wValidBitsPerSample = 32;
	details->OutputFormat.Format.wBitsPerSample = 32;
	details->OutputFormat.Format.wFormatTag = 3;
	details->OutputFormat.Format.nChannels = 2;
	details->OutputFormat.Format.nSamplesPerSec = 48000;
	details->OutputFormat.Format.nBlockAlign = (
		details->OutputFormat.Format.nChannels *
		(details->OutputFormat.Format.wBitsPerSample / 8)
	);
	details->OutputFormat.Format.nAvgBytesPerSec = (
		details->OutputFormat.Format.nSamplesPerSec *
		details->OutputFormat.Format.nBlockAlign
	);
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

/* Spinlocks */

void FAudio_PlatformLock(FAudioSpinLock *lock)
{
	SDL_AtomicLock((SDL_SpinLock*) lock);
}

void FAudio_PlatformUnlock(FAudioSpinLock *lock)
{
	SDL_AtomicUnlock((SDL_SpinLock*) lock);
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

int FAudio_memcmp(const void *ptr1, const void *ptr2, size_t size)
{
	return SDL_memcmp(ptr1, ptr2, size);
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

double FAudio_sin(double x)
{
	return SDL_sin(x);
}

double FAudio_cos(double x)
{
	return SDL_cos(x);
}

double FAudio_tan(double x)
{
	return SDL_tan(x);
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
