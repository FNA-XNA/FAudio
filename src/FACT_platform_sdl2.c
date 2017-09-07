/* FACT - XACT Reimplementation for FNA
 * Copyright 2009-2017 Ethan Lee and the MonoGame Team
 *
 * Released under the Microsoft Public License.
 * See LICENSE for details.
 */

#include "FACT_internal.h"

#include <SDL.h>

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
