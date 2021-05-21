/* FAudio - XAudio Reimplementation for FNA
 *
 * Copyright (c) 2011-2020 Ethan Lee, Luigi Auriemma, and the MonoGame Team
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

#ifdef FAUDIO_WIN32_PLATFORM

#include "FAudio_internal.h"

#define COBJMACROS
#include <windows.h>

#include <initguid.h>
#include <audioclient.h>
#include <mmdeviceapi.h>

static CRITICAL_SECTION faudio_cs = { NULL, -1, 0, 0, 0, 0 };
static IMMDeviceEnumerator *device_enumerator;
static HRESULT init_hr;

struct FAudioWin32PlatformData
{
	IAudioClient *client;
	HANDLE audioThread;
	HANDLE stopEvent;
};

struct FAudioAudioClientThreadArgs
{
	WAVEFORMATEXTENSIBLE format;
	IAudioClient *client;
	HANDLE events[2];
	FAudio *audio;
	UINT updateSize;
};

void FAudio_Log(char const *msg)
{
	OutputDebugStringA(msg);
}

static HRESULT FAudio_FillAudioClientBuffer(
	struct FAudioAudioClientThreadArgs *args,
	IAudioRenderClient *client,
	UINT frames,
	UINT padding
)
{
	HRESULT hr = S_OK;
	BYTE *buffer;

	while (padding + args->updateSize <= frames)
	{
		hr = IAudioRenderClient_GetBuffer(
			client,
			frames - padding,
			&buffer
		);
		if (FAILED(hr)) return hr;

		FAudio_zero(
			buffer,
			args->updateSize * args->format.Format.nBlockAlign
		);

		if (args->audio->active)
		{
			FAudio_INTERNAL_UpdateEngine(
				args->audio,
				(float*) buffer
			);
		}

		hr = IAudioRenderClient_ReleaseBuffer(
			client,
			args->updateSize,
			0
		);
		if (FAILED(hr)) return hr;

		padding += args->updateSize;
	}

	return hr;
}

static DWORD WINAPI FAudio_AudioClientThread(void *user)
{
	struct FAudioAudioClientThreadArgs *args = user;
	IAudioRenderClient *render_client;
	HRESULT hr = S_OK;
	UINT frames, padding = 0;

	hr = IAudioClient_GetService(
		args->client,
		&IID_IAudioRenderClient,
		(void **)&render_client
	);
	FAudio_assert(!FAILED(hr) && "Failed to get IAudioRenderClient service!");

	hr = IAudioClient_GetBufferSize(args->client, &frames);
	FAudio_assert(!FAILED(hr) && "Failed to get IAudioClient buffer size!");

	hr = FAudio_FillAudioClientBuffer(args, render_client, frames, 0);
	FAudio_assert(!FAILED(hr) && "Failed to initialize IAudioClient buffer!");

	hr = IAudioClient_Start(args->client);
	FAudio_assert(!FAILED(hr) && "Failed to start IAudioClient!");

	while (WaitForMultipleObjects(2, args->events, FALSE, INFINITE) == WAIT_OBJECT_0)
	{
		hr = IAudioClient_GetCurrentPadding(args->client, &padding);
		FAudio_assert(!FAILED(hr) && "Failed to get IAudioClient current padding!");

		hr = FAudio_FillAudioClientBuffer(args, render_client, frames, padding);
		FAudio_assert(!FAILED(hr) && "Failed to fill IAudioClient buffer!");
	}

	hr = IAudioClient_Stop(args->client);
	FAudio_assert(!FAILED(hr) && "Failed to stop IAudioClient!");

	IAudioRenderClient_Release(render_client);
	FAudio_free(args);
	return 0;
}

void FAudio_PlatformInit(
	FAudio *audio,
	uint32_t flags,
	uint32_t deviceIndex,
	FAudioWaveFormatExtensible *mixFormat,
	uint32_t *updateSize,
	void** platformDevice
)
{
	struct FAudioAudioClientThreadArgs *args;
	struct FAudioWin32PlatformData *data;
	REFERENCE_TIME duration;
	WAVEFORMATEX *closest;
	IMMDevice *device = NULL;
	HRESULT hr;
	HANDLE audioEvent = NULL;
	BOOL has_sse2 = IsProcessorFeaturePresent(PF_XMMI64_INSTRUCTIONS_AVAILABLE);

	FAudio_INTERNAL_InitSIMDFunctions(has_sse2, FALSE);

	FAudio_PlatformAddRef();

	*platformDevice = NULL;
	if (deviceIndex > 0) return;

	args = FAudio_malloc(sizeof(*args));
	FAudio_assert(!!args && "Failed to allocate FAudio thread args!");

	data = FAudio_malloc(sizeof(*data));
	FAudio_assert(!!data && "Failed to allocate FAudio platform data!");
	FAudio_zero(data, sizeof(*data));

	args->format.Format.wFormatTag = mixFormat->Format.wFormatTag;
	args->format.Format.nChannels = mixFormat->Format.nChannels;
	args->format.Format.nSamplesPerSec = mixFormat->Format.nSamplesPerSec;
	args->format.Format.nAvgBytesPerSec = mixFormat->Format.nAvgBytesPerSec;
	args->format.Format.nBlockAlign = mixFormat->Format.nBlockAlign;
	args->format.Format.wBitsPerSample = mixFormat->Format.wBitsPerSample;
	args->format.Format.cbSize = mixFormat->Format.cbSize;

	if (args->format.Format.wFormatTag == WAVE_FORMAT_EXTENSIBLE)
	{
		args->format.Samples.wValidBitsPerSample = mixFormat->Samples.wValidBitsPerSample;
		args->format.dwChannelMask = mixFormat->dwChannelMask;
		FAudio_memcpy(
			&args->format.SubFormat,
			&mixFormat->SubFormat,
			sizeof(GUID)
		);
	}

	audioEvent = CreateEventW(NULL, FALSE, FALSE, NULL);
	FAudio_assert(!!audioEvent && "Failed to create FAudio thread buffer event!");

	data->stopEvent = CreateEventW(NULL, FALSE, FALSE, NULL);
	FAudio_assert(!!data->stopEvent && "Failed to create FAudio thread stop event!");

	hr = IMMDeviceEnumerator_GetDefaultAudioEndpoint(
		device_enumerator,
		eRender,
		eConsole,
		&device
	);
	FAudio_assert(!FAILED(hr) && "Failed to get default audio endpoint!");

	hr = IMMDevice_Activate(
		device,
		&IID_IAudioClient,
		CLSCTX_ALL,
		NULL,
		(void **)&data->client
	);
	FAudio_assert(!FAILED(hr) && "Failed to create audio client!");
	IMMDevice_Release(device);

	if (flags & FAUDIO_1024_QUANTUM) duration = 21330;
	else duration = 30000;

	hr = IAudioClient_IsFormatSupported(
		data->client,
		AUDCLNT_SHAREMODE_SHARED,
		&args->format.Format,
		&closest
	);
	FAudio_assert(!FAILED(hr) && "Failed to find supported audio format!");

	if (closest)
	{
		if (closest->wFormatTag != WAVE_FORMAT_EXTENSIBLE) args->format.Format = *closest;
		else args->format = *(WAVEFORMATEXTENSIBLE *)closest;
		CoTaskMemFree(closest);
	}

	hr = IAudioClient_Initialize(
		data->client,
		AUDCLNT_SHAREMODE_SHARED,
		AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
		duration,
		0,
		&args->format.Format,
		&GUID_NULL
	);
	FAudio_assert(!FAILED(hr) && "Failed to initialize audio client!");

	hr = IAudioClient_SetEventHandle(data->client, audioEvent);
	FAudio_assert(!FAILED(hr) && "Failed to set audio client event!");

	mixFormat->Format.wFormatTag = args->format.Format.wFormatTag;
	mixFormat->Format.nChannels = args->format.Format.nChannels;
	mixFormat->Format.nSamplesPerSec = args->format.Format.nSamplesPerSec;
	mixFormat->Format.nAvgBytesPerSec = args->format.Format.nAvgBytesPerSec;
	mixFormat->Format.nBlockAlign = args->format.Format.nBlockAlign;
	mixFormat->Format.wBitsPerSample = args->format.Format.wBitsPerSample;

	if (args->format.Format.wFormatTag == WAVE_FORMAT_EXTENSIBLE)
	{
		mixFormat->Format.cbSize = sizeof(FAudioWaveFormatExtensible) - sizeof(FAudioWaveFormatEx);
		mixFormat->Samples.wValidBitsPerSample = args->format.Samples.wValidBitsPerSample;
		mixFormat->dwChannelMask = args->format.dwChannelMask;
		FAudio_memcpy(
			&mixFormat->SubFormat,
			&args->format.SubFormat,
			sizeof(GUID)
		);
	}
	else
	{
		mixFormat->Format.cbSize = sizeof(FAudioWaveFormatEx);
	}

	args->client = data->client;
	args->events[0] = audioEvent;
	args->events[1] = data->stopEvent;
	args->audio = audio;
	args->updateSize = args->format.Format.nSamplesPerSec / 100;

	data->audioThread = CreateThread(NULL, 0, &FAudio_AudioClientThread, args, 0, NULL);
	FAudio_assert(!!data->audioThread && "Failed to create audio client thread!");

	*updateSize = args->updateSize;
	*platformDevice = data;
	return;
}

void FAudio_PlatformQuit(void* platformDevice)
{
	struct FAudioWin32PlatformData *data = platformDevice;

	SetEvent(data->stopEvent);
	WaitForSingleObject(data->audioThread, INFINITE);
	if (data->client) IAudioClient_Release(data->client);
	FAudio_PlatformRelease();
}

void FAudio_PlatformAddRef()
{
	HRESULT hr;
	EnterCriticalSection(&faudio_cs);
	if (!device_enumerator)
	{
		init_hr = CoInitialize(NULL);
		hr = CoCreateInstance(
			&CLSID_MMDeviceEnumerator,
			NULL,
			CLSCTX_INPROC_SERVER,
			&IID_IMMDeviceEnumerator,
			(void**)&device_enumerator
		);
		FAudio_assert(!FAILED(hr) && "CoCreateInstance failed!");
	}
	else IMMDeviceEnumerator_AddRef(device_enumerator);
	LeaveCriticalSection(&faudio_cs);
}

void FAudio_PlatformRelease()
{
	EnterCriticalSection(&faudio_cs);
	if (!IMMDeviceEnumerator_Release(device_enumerator))
	{
		device_enumerator = NULL;
		if (SUCCEEDED(init_hr)) CoUninitialize();
	}
	LeaveCriticalSection(&faudio_cs);
}

uint32_t FAudio_PlatformGetDeviceCount(void)
{
	IMMDevice *device;
	uint32_t count;
	HRESULT hr;

	FAudio_PlatformAddRef();
	hr = IMMDeviceEnumerator_GetDefaultAudioEndpoint(
		device_enumerator,
		eRender,
		eConsole,
		&device
	);
	FAudio_assert(!FAILED(hr) && "Failed to get default audio endpoint!");

	IMMDevice_Release(device);
	FAudio_PlatformRelease();

	return 1;
}

uint32_t FAudio_PlatformGetDeviceDetails(
		uint32_t index,
		FAudioDeviceDetails *details
)
{
	WAVEFORMATEXTENSIBLE *ext;
	WAVEFORMATEX *format;
	IAudioClient *client;
	IMMDevice *device;
	uint32_t ret = 0;
	HRESULT hr;
	WCHAR *str;

	FAudio_memset(details, 0, sizeof(FAudioDeviceDetails));
	if (index > 0) return FAUDIO_E_INVALID_CALL;

	FAudio_PlatformAddRef();

	hr = IMMDeviceEnumerator_GetDefaultAudioEndpoint(
		device_enumerator,
		eRender,
		eConsole,
		&device
	);
	FAudio_assert(!FAILED(hr) && "Failed to get default audio endpoint!");

	details->Role = FAudioGlobalDefaultDevice;

	hr = IMMDevice_GetId(device, &str);
	FAudio_assert(!FAILED(hr) && "Failed to get audio endpoint id!");

	FAudio_memcpy(details->DeviceID, str, 0xff * sizeof(WCHAR));
	details->DeviceID[0xff] = 0;
	FAudio_memcpy(details->DisplayName, str, 0xff * sizeof(WCHAR));
	details->DisplayName[0xff] = 0;
	CoTaskMemFree(str);

	hr = IMMDevice_Activate(
		device,
		&IID_IAudioClient,
		CLSCTX_ALL,
		NULL,
		(void **)&client
	);
	FAudio_assert(!FAILED(hr) && "Failed to activate audio client!");

	hr = IAudioClient_GetMixFormat(client, &format);
	FAudio_assert(!FAILED(hr) && "Failed to get audio client mix format!");

	details->OutputFormat.Format.wFormatTag = format->wFormatTag;
	details->OutputFormat.Format.nChannels = format->nChannels;
	details->OutputFormat.Format.nSamplesPerSec = format->nSamplesPerSec;
	details->OutputFormat.Format.nAvgBytesPerSec = format->nAvgBytesPerSec;
	details->OutputFormat.Format.nBlockAlign = format->nBlockAlign;
	details->OutputFormat.Format.wBitsPerSample = format->wBitsPerSample;
	details->OutputFormat.Format.cbSize = format->cbSize;

	if (format->wFormatTag == WAVE_FORMAT_EXTENSIBLE)
	{
		ext = (WAVEFORMATEXTENSIBLE *)format;
		details->OutputFormat.Samples.wValidBitsPerSample = ext->Samples.wValidBitsPerSample;
		details->OutputFormat.dwChannelMask = ext->dwChannelMask;
		FAudio_memcpy(
			&details->OutputFormat.SubFormat,
			&ext->SubFormat,
			sizeof(GUID)
		);
	}

	IAudioClient_Release(client);

	IMMDevice_Release(device);

	FAudio_PlatformRelease();

	return ret;
}

FAudioMutex FAudio_PlatformCreateMutex(void)
{
	CRITICAL_SECTION *cs;

	cs = FAudio_malloc(sizeof(CRITICAL_SECTION));
	if (!cs) return NULL;

	InitializeCriticalSection(cs);

	return cs;
}

void FAudio_PlatformLockMutex(FAudioMutex mutex)
{
	if (mutex) EnterCriticalSection(mutex);
}

void FAudio_PlatformUnlockMutex(FAudioMutex mutex)
{
	if (mutex) LeaveCriticalSection(mutex);
}

void FAudio_PlatformDestroyMutex(FAudioMutex mutex)
{
	if (mutex) DeleteCriticalSection(mutex);
	FAudio_free(mutex);
}

struct FAudioThreadArgs
{
	FAudioThreadFunc func;
	const char *name;
	void* data;
};

static DWORD WINAPI FaudioThreadWrapper(void *user)
{
	struct FAudioThreadArgs *args = user;
	DWORD ret;

	ret = args->func(args->data);

	FAudio_free(args);
	return ret;
}

FAudioThread FAudio_PlatformCreateThread(
	FAudioThreadFunc func,
	const char *name,
	void* data
)
{
	struct FAudioThreadArgs *args;

	if (!(args = FAudio_malloc(sizeof(*args)))) return NULL;
	args->func = func;
	args->name = name;
	args->data = data;

	return CreateThread(NULL, 0, &FaudioThreadWrapper, args, 0, NULL);
}

void FAudio_PlatformWaitThread(FAudioThread thread, int32_t *retval)
{
	WaitForSingleObject(thread, INFINITE);
	GetExitCodeThread(thread, (DWORD *)retval);
}

void FAudio_PlatformThreadPriority(FAudioThreadPriority priority)
{
	/* FIXME */
}

uint64_t FAudio_PlatformGetThreadID(void)
{
	return GetCurrentThreadId();
}

void FAudio_sleep(uint32_t ms)
{
	Sleep(ms);
}

uint32_t FAudio_timems()
{
	return GetTickCount();
}

/* FAudio I/O */

static size_t FAUDIOCALL FAudio_FILE_read(
	void *data,
	void *dst,
	size_t size,
	size_t count
)
{
	if (!data) return 0;
	return fread(dst, size, count, data);
}

static int64_t FAUDIOCALL FAudio_FILE_seek(
	void *data,
	int64_t offset,
	int whence
)
{
	if (!data) return -1;
	fseek(data, offset, whence);
	return ftell(data);
}

static int FAUDIOCALL FAudio_FILE_close(void *data)
{
	if (!data) return 0;
	fclose(data);
	return 0;
}

FAudioIOStream* FAudio_fopen(const char *path)
{
	FAudioIOStream *io;

	io = (FAudioIOStream*) FAudio_malloc(sizeof(FAudioIOStream));
	if (!io) return NULL;

	io->data = fopen(path, "rb");
	io->read = FAudio_FILE_read;
	io->seek = FAudio_FILE_seek;
	io->close = FAudio_FILE_close;
	io->lock = FAudio_PlatformCreateMutex();

	return io;
}

struct FAudio_mem
{
	char *mem;
	int64_t len;
	int64_t pos;
};

static size_t FAUDIOCALL FAudio_mem_read(
	void *data,
	void *dst,
	size_t size,
	size_t count
)
{
	struct FAudio_mem *io = data;
	size_t len = size * count;

	if (!data) return 0;

	while (len && len > (io->len - io->pos)) len -= size;
	FAudio_memcpy(dst, io->mem + io->pos, len);
	io->pos += len;

	return len;
}

static int64_t FAUDIOCALL FAudio_mem_seek(
	void *data,
	int64_t offset,
	int whence
)
{
	struct FAudio_mem *io = data;
	if (!data) return -1;

	if (whence == SEEK_SET)
	{
		if (io->len > offset) io->pos = offset;
		else io->pos = io->len;
	}
	if (whence == SEEK_CUR)
	{
		if (io->len > io->pos + offset) io->pos += offset;
		else io->pos = io->len;
	}
	if (whence == SEEK_END)
	{
		if (io->len > offset) io->pos = io->len - offset;
		else io->pos = 0;
	}

	return io->pos;
}

static int FAUDIOCALL FAudio_mem_close(void *data)
{
	if (!data) return 0;
	FAudio_free(data);
}

FAudioIOStream* FAudio_memopen(void *mem, int len)
{
	struct FAudio_mem *data;
	FAudioIOStream *io;

	io = (FAudioIOStream*) FAudio_malloc(sizeof(FAudioIOStream));
	if (!io) return NULL;

	data = FAudio_malloc(sizeof(struct FAudio_mem));
	if (!data)
	{
		FAudio_free(io);
		return NULL;
	}

	data->mem = mem;
	data->len = len;
	data->pos = 0;

	io->data = data;
	io->read = FAudio_mem_read;
	io->seek = FAudio_mem_seek;
	io->close = FAudio_mem_close;
	io->lock = FAudio_PlatformCreateMutex();
	return io;
}

uint8_t* FAudio_memptr(FAudioIOStream *io, size_t offset)
{
	struct FAudio_mem *memio = io->data;
	return memio->mem + offset;
}

void FAudio_close(FAudioIOStream *io)
{
	io->close(io->data);
	FAudio_PlatformDestroyMutex((FAudioMutex) io->lock);
	FAudio_free(io);
}

#else

extern int this_tu_is_empty;

#endif /* FAUDIO_WIN32_PLATFORM */
