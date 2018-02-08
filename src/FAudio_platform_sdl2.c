/* FACT - XACT Reimplementation for FNA
 * Copyright 2009-2018 Ethan Lee and the MonoGame Team
 *
 * Released under the Microsoft Public License.
 * See LICENSE for details.
 */

#include "FACT_internal.h"

#include <SDL.h>

/* Internal Types */

typedef struct FACTEngineEntry FACTEngineEntry;
struct FACTEngineEntry
{
	FACTAudioEngine *engine;
	FACTEngineEntry *next;
};

typedef struct FACTPlatformDevice FACTPlatformDevice;
struct FACTPlatformDevice
{
	const char *name;
	SDL_AudioDeviceID device;
	FAudioWaveFormatExtensible format;
	FACTEngineEntry *engineList;
	FACTPlatformDevice *next;

	int16_t* decodeCache[2];
	float* resampleCache[2];
	uint32_t bufferSize;
};

/* Globals */

#define devlist FACTdevlist /* Doing this to reduce diffs */
FACTPlatformDevice *devlist = NULL;

/* Mixer Thread */

void FACT_INTERNAL_MixCallback(void *userdata, Uint8 *stream, int len)
{
	FACTPlatformDevice *device = (FACTPlatformDevice*) userdata;
	FACTEngineEntry *engine = device->engineList;
	FACTSoundBank *sb;
	FACTWaveBank *wb;
	FACTCue *cue, *backup;
	FACTWave *wave;
	uint32_t samples;
	uint32_t timestamp;
	uint32_t i, j, k;
	float *out = (float*) stream;

	FAudio_zero(stream, len);

	/* We want the timestamp to be uniform across all Cues.
	 * Oftentimes many Cues are played at once with the expectation
	 * that they will sync, so give them all the same timestamp
	 * so all the various actions will go together even if it takes
	 * an extra millisecond to get through the whole Cue list.
	 */
	timestamp = FAudio_timems();

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

				/* Destroy if it's done and not user-handled. */
				if (cue->managed && (cue->state & FACT_STATE_STOPPED))
				{
					backup = cue->next;
					FACTCue_Destroy(cue);
					cue = backup;
				}
				else
				{
					cue = cue->next;
				}
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

				/* Decode and resample wavedata */
				samples = FACT_INTERNAL_GetWave(
					wave,
					device->decodeCache[0],
					device->decodeCache[1],
					device->resampleCache[0],
					device->resampleCache[1],
					device->bufferSize
				);

				/* ... then mix, finally. */
				if (samples > 0)
				{
					for (i = 0; i < samples; i += 1)
					for (j = 0; j < wave->dstChannels; j += 1)
					for (k = 0; k < wave->srcChannels; k += 1)
					{
						out[i * device->format.Format.nChannels + j] = FAudio_clamp(
							out[i * device->format.Format.nChannels + j] + (
								device->resampleCache[k][i] *
								wave->volume *
								wave->matrixCoefficients[j * wave->srcChannels + k]
							),
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

void FACT_PlatformInitEngine(FACTAudioEngine *engine, int16_t *id)
{
	FACTEngineEntry *entry, *entryList;
	FACTPlatformDevice *device, *deviceList;
	SDL_AudioSpec want, have;
	const char *name;
	int32_t renderer;

	if (!SDL_WasInit(SDL_INIT_AUDIO))
	{
		SDL_InitSubSystem(SDL_INIT_AUDIO);
	}

	/* The entry to be added to the audio device */
	entry = (FACTEngineEntry*) FAudio_malloc(sizeof(FACTEngineEntry));
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
		device = (FACTPlatformDevice*) FAudio_malloc(
			sizeof(FACTPlatformDevice)
		);
		device->name = name;
		device->engineList = entry;

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
		want.freq = 48000; /* FIXME: Make this 0 */
		want.format = AUDIO_F32;
		want.channels = 0;
		want.silence = 0;
		want.samples = 4096; /* FIXME: Make this 1024 */
		want.callback = FACT_INTERNAL_MixCallback;
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
		if (have.channels == 2)
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

		/* Alloc decode/resample caches */
		device->decodeCache[0] = (int16_t*) FAudio_malloc(
			2 * (have.samples * MAX_RESAMPLE_STEP + RESAMPLE_PADDING)
		);
		device->decodeCache[1] = (int16_t*) FAudio_malloc(
			2 * (have.samples * MAX_RESAMPLE_STEP + RESAMPLE_PADDING)
		);
		device->resampleCache[0] = (float*) FAudio_malloc(4 * have.samples);
		device->resampleCache[1] = (float*) FAudio_malloc(4 * have.samples);
		device->bufferSize = have.samples;

		/* Give the output format to the engine */
		engine->mixFormat = &device->format;

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
		/* But give us the output format first! */
		engine->mixFormat = &device->format;

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
	FACTPlatformDevice *dev = devlist;
	FACTPlatformDevice *prevDev = devlist;
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
				FAudio_free(dev->decodeCache[0]);
				FAudio_free(dev->decodeCache[1]);
				FAudio_free(dev->resampleCache[0]);
				FAudio_free(dev->resampleCache[1]);
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

	/* No more devices? We're done here... */
	if (devlist == NULL)
	{
		SDL_QuitSubSystem(SDL_INIT_AUDIO);
	}
}
#undef devlist /* Doing this to reduce diffs */

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
	FAudioEntry *audio;
	FAudioSourceVoiceEntry *source;
	FAudioSubmixVoiceEntry *submix;
	FAudioPlatformDevice *device = (FAudioPlatformDevice*) userdata;
	uint32_t i;

	FAudio_zero(stream, len);
	while (device != NULL)
	{
		audio = device->engineList;
		while (audio != NULL)
		{
			if (audio->audio->active)
			{
				audio->audio->master->master.output = (float*) stream;
				source = audio->audio->sources;
				while (source != NULL)
				{
					/* TODO: Mix sources */
					source = source->next;
				}
				for (i = 0; i < audio->audio->submixStages; i += 1)
				{
					submix = audio->audio->submixes;
					while (submix != NULL)
					{
						if (submix->voice->mix.processingStage == i)
						{
							/* TODO: Mix submixes */
						}
						submix = submix->next;
					}
				}
			}
			audio = audio->next;
		}
		device = device->next;
	}
}

/* Platform Functions */

void FAudio_PlatformInit(FAudio *audio)
{
	FAudioEntry *entry, *entryList;
	FAudioPlatformDevice *device, *deviceList;
	SDL_AudioSpec want, have;
	const char *name;

	if (!SDL_WasInit(SDL_INIT_AUDIO))
	{
		SDL_InitSubSystem(SDL_INIT_AUDIO);
	}

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
		want.freq = 48000; /* FIXME: Make this 0 */
		want.format = AUDIO_F32;
		want.channels = 0;
		want.silence = 0;
		want.samples = 4096; /* FIXME: Make this 1024 */
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
		if (have.channels == 2)
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

	/* No more devices? We're done here... */
	if (devlist == NULL)
	{
		SDL_QuitSubSystem(SDL_INIT_AUDIO);
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
}

/* stdlib Functions */

void* FAudio_malloc(size_t size)
{
	return SDL_malloc(size);
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

#if !SDL_VERSION_ATLEAST(2, 0, 8) /* FIXME: REMOVE ME! */
extern double SDL_uclibc_log10(double x);
#endif

double FAudio_log10(double x)
{
#if SDL_VERSION_ATLEAST(2, 0, 8)
	return SDL_log10(x);
#else
	return SDL_uclibc_log10(x); /* FIXME: REMOVE ME! */
#endif
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

float FAudio_rng()
{
	/* TODO: Random number generator */
	static float butt = 0.0f;
	float result = butt;
	butt += 0.2;
	if (butt > 1.0f)
	{
		butt = 0.0f;
	}
	return result;
}

uint32_t FAudio_timems()
{
	return SDL_GetTicks();
}

/* FACT I/O */

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
