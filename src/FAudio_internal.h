/* FACT - XACT Reimplementation for FNA
 * Copyright 2009-2018 Ethan Lee and the MonoGame Team
 *
 * Released under the Microsoft Public License.
 * See LICENSE for details.
 */

#include "FAudio.h"

struct FAudio
{
	uint8_t TODO;
};

struct FAudioVoice
{
	uint8_t TODO;
};

/* Platform Functions */

/* FIXME: Re-do these for FAudio... */
typedef struct FACTAudioEngine FACTAudioEngine;
typedef struct FACTRendererDetails FACTRendererDetails;
void FAudio_PlatformInitEngine(FACTAudioEngine *engine, int16_t *id);
void FAudio_PlatformCloseEngine(FACTAudioEngine *engine);
uint16_t FAudio_PlatformGetRendererCount();
void FAudio_PlatformGetRendererDetails(
	uint16_t index,
	FACTRendererDetails *details
);

void* FAudio_malloc(size_t size);
void FAudio_free(void *ptr);
void FAudio_zero(void *ptr, size_t size);
void FAudio_memcpy(void *dst, const void *src, size_t size);
void FAudio_memmove(void *dst, void *src, size_t size);
size_t FAudio_strlen(const char *ptr);
int FAudio_strcmp(const char *str1, const char *str2);
void FAudio_strlcpy(char *dst, const char *src, size_t len);
double FAudio_pow(double x, double y);
double FAudio_log10(double x);
double FAudio_sqrt(double x);
double FAudio_acos(double x);
float FAudio_rng();
uint32_t FAudio_timems();

#define FAudio_min(val1, val2) \
	(val1 < val2 ? val1 : val2)
#define FAudio_max(val1, val2) \
	(val1 > val2 ? val1 : val2)
#define FAudio_clamp(val, min, max) \
	(val > max ? max : (val < min ? min : val))
