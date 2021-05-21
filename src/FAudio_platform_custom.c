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

#include "FAudio_internal.h"

#ifdef FAUDIO_PLATFORM_CALLBACKS

static const FAudioPlatform *platform;

FAUDIOAPI void FAudio_SetPlatform(const FAudioPlatform *callbacks)
{
        platform = callbacks;
}

static void FAUDIOCALL FAudio_INTERNAL_MixCallback(void *userdata, char *stream, int len)
{
        FAudio *audio = (FAudio*) userdata;

        FAudio_zero(stream, len);
        if (audio->active)
        {
                FAudio_INTERNAL_UpdateEngine(
                        audio,
                        (float*) stream
                );
        }
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
        FAudio_INTERNAL_InitSIMDFunctions(
                platform->pHasSSE2(),
                platform->pHasNEON()
        );
        platform->pPlatformInit(audio, flags, deviceIndex,
                                mixFormat, updateSize,
                                FAudio_INTERNAL_MixCallback,
                                platformDevice);
}

void FAudio_PlatformQuit(void* platformDevice)
{
        platform->pPlatformQuit(platformDevice);
}

void FAudio_PlatformAddRef()
{
        platform->pPlatformAddRef();
}

void FAudio_PlatformRelease()
{
        platform->pPlatformRelease();
}

uint32_t FAudio_PlatformGetDeviceCount(void)
{
        return platform->pGetDeviceCount();
}

uint32_t FAudio_PlatformGetDeviceDetails(
        uint32_t index,
        FAudioDeviceDetails *details
)
{
        return platform->pGetDeviceDetails(index, details);
}

FAudioMutex FAudio_PlatformCreateMutex(void)
{
        return platform->pCreateMutex();
}

void FAudio_PlatformLockMutex(FAudioMutex mutex)
{
        return platform->pLockMutex(mutex);
}

void FAudio_PlatformUnlockMutex(FAudioMutex mutex)
{
        return platform->pUnlockMutex(mutex);
}

void FAudio_PlatformDestroyMutex(FAudioMutex mutex)
{
        return platform->pDestroyMutex(mutex);
}

FAudioThread FAudio_PlatformCreateThread(
        FAudioThreadFunc func,
        const char *name,
        void* data
)
{
        return platform->pCreateThread(func, name, data);
}

void FAudio_PlatformWaitThread(FAudioThread thread, int32_t *retval)
{
        platform->pWaitThread(thread, retval);
}

void FAudio_PlatformThreadPriority(FAudioThreadPriority priority)
{
        platform->pThreadPriority(priority);
}

uint64_t FAudio_PlatformGetThreadID(void)
{
        return platform->pGetThreadID();
}

void FAudio_sleep(uint32_t ms)
{
        platform->psleep(ms);
}

uint32_t FAudio_timems()
{
        platform->ptimems();
}

/* FAudio I/O */

static size_t FAUDIOCALL FAudio_FILE_read(void *data, void *dst, size_t size, size_t count)
{
        if (!data) return 0;
        return fread(dst, size, count, data);
}

static int64_t FAUDIOCALL FAudio_FILE_seek(void *data, int64_t offset, int whence)
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
        FAudioIOStream *io = (FAudioIOStream*) FAudio_malloc(
                sizeof(FAudioIOStream)
        );
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

static size_t FAUDIOCALL FAudio_mem_read(void *data, void *dst, size_t size, size_t count)
{
        struct FAudio_mem *io = data;
        size_t len = size * count;
        if (!data) return 0;
        while (len && len > (io->len - io->pos)) len -= size;
        memcpy(dst, io->mem + io->pos, len);
        io->pos += len;
        return len;
}

static int64_t FAUDIOCALL FAudio_mem_seek(void *data, int64_t offset, int whence)
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
        FAudioIOStream *io = (FAudioIOStream*) FAudio_malloc(
                sizeof(FAudioIOStream)
        );
        struct FAudio_mem *data = FAudio_malloc(sizeof(struct FAudio_mem));
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

#endif
