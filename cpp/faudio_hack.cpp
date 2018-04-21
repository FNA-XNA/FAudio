#include <FAudio.h>
#include <FAudio_internal.h>
#include <SDL.h>

void FAudio_PlatformAddRef()
{
	/* SDL tracks ref counts for each subsystem */
	SDL_InitSubSystem(SDL_INIT_AUDIO);
}

void* FAudio_malloc(size_t size)
{
	return SDL_malloc(size);
}

void FAudio_zero(void *ptr, size_t size)
{
	SDL_memset(ptr, '\0', size);
}

uint32_t FAudioCreate_NoInit(FAudio **ppFAudio) {
	FAudio_PlatformAddRef();
	*ppFAudio = (FAudio*)FAudio_malloc(sizeof(FAudio));
	FAudio_zero(*ppFAudio, sizeof(FAudio));
	(*ppFAudio)->refcount = 0;
	return 0;
}
