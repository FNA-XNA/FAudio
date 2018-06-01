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

#ifndef FAUDIOFX_INTERNAL_H
#define FAUDIOFX_INTERNAL_H

#include <stddef.h>
#include <stdint.h>

/* utiliy functions */
float FAudioFX_INTERNAL_DbGainToFactor(float gain);
uint32_t FAudioFX_INTERNAL_MsToSamples(float msec, int32_t sampleRate);

/* basic delay (rt60_ms == 0) or comb filter (rt60_ms > 0) */
#define FAUDIOFX_DELAY_MAX_DELAY_MS 300

typedef struct FAudioFXFilterDelay
{
	int32_t	 sampleRate;
	uint32_t capacity;		/* in samples */
	uint32_t delay;			/* in samples */
	float feedback;
	uint32_t read_idx;
	uint32_t write_idx;
	float denormal;
	float buffer[];
} FAudioFXFilterDelay;

FAudioFXFilterDelay *FAudioFXFilterDelay_Create(int32_t sampleRate, float delay_ms, float rt60_ms);
void FAudioFXFilterDelay_Change(FAudioFXFilterDelay *filter, float delay_ms, float rt60_ms);
float FAudioFXFilterDelay_Process(FAudioFXFilterDelay *filter, float sample);
float FAudioFXFilterDelay_Tap(FAudioFXFilterDelay *filter, uint32_t delay);
void FAudioFXFilterDelay_Destroy(FAudioFXFilterDelay *filter);

/* comb-filter with integrated lowpass filter */
typedef struct FAudioFXFilterCombLpf
{	
	float lpf_gain;
	float lpf_buffer;
	FAudioFXFilterDelay delay;
} FAudioFXFilterCombLpf;

FAudioFXFilterCombLpf *FAudioFXFilterCombLpf_Create(int32_t sampleRate, float delay_ms, float rt60_ms, float lpf_gain);
void FAudioFXFilterCombLpf_Change(FAudioFXFilterCombLpf *filter, float delay_ms, float rt60_ms, float lpf_gain);
float FAudioFXFilterCombLpf_Process(FAudioFXFilterCombLpf *filter, float sample);


/* biquad filter */
#define FAUDIOFX_BIQUAD_LOWPASS 0
#define FAUDIOFX_BIQUAD_HIGHPASS 1
#define FAUDIOFX_BIQUAD_LOWSHELVING 2
#define FAUDIOFX_BIQUAD_HIGHSHELVING 3

typedef struct FAudioFXFilterBiQuad
{
	int32_t type;
	int32_t sampleRate;
	float a0, a1, a2;
	float b1, b2;
	float c0, d0;
	float delay_x[2];
	float delay_y[2];
} FAudioFXFilterBiQuad;

FAudioFXFilterBiQuad *FAudioFXFilterBiQuad_Create(int32_t sampleRate, int type, float frequency, float q, float gain);
void FAudioFXFilterBiQuad_Change(FAudioFXFilterBiQuad *filter, float frequency, float q, float gain);
float FAudioFXFilterBiQuad_Process(FAudioFXFilterBiQuad *filter, float sample);
void FAudioFXFilterBiQuad_Destroy(FAudioFXFilterBiQuad *filter);

/* comb-filter with integrated low and high shelving filter */
typedef struct FAudioFXFilterCombShelving {
	FAudioFXFilterBiQuad low_shelving;
	FAudioFXFilterBiQuad high_shelving;
	FAudioFXFilterDelay delay;
} FAudioFXFilterCombShelving;

FAudioFXFilterCombShelving *FAudioFXFilterCombShelving_Create(int32_t sampleRate, float delay_ms, float rt60_ms);
void FAudioFXFilterCombShelving_Change(FAudioFXFilterCombShelving *filter, float delay_ms, float rt60_ms);
float FAudioFXFilterCombShelving_Process(FAudioFXFilterCombShelving *filter, float sample);


/* state variable filter */
#define FAUDIOFX_STATEVARIABLE_LOWPASS 0
#define FAUDIOFX_STATEVARIABLE_BANDPASS 1
#define FAUDIOFX_STATEVARIABLE_HIGHPASS 2
#define FAUDIOFX_STATEVARIABLE_NOTCH 3

typedef struct FAudioFXFilterStateVariable 
{
	int32_t sampleRate;
	float frequency;
	float oneOverQ;
	int32_t type;
	float state[4];
} FAudioFXFilterStateVariable;

FAudioFXFilterStateVariable *FAudioFXFilterStateVariable_Create(
	int32_t sampleRate,
	int32_t type,
	float frequency,
	float q
);
void FAudioFXFilterStateVariable_Change(FAudioFXFilterStateVariable *filter, float frequency, float q);
float FAudioFXFilterStateVariable_Process(FAudioFXFilterStateVariable *filter, float sample);
void FAudioFXFilterStateVariable_Destroy(FAudioFXFilterStateVariable *filter);

/* delaying all-pass filter */
typedef struct FAudioFXFilterAllPass {
	int32_t sampleRate;
	uint32_t capacity;		/* in samples */
	uint32_t delay;			/* in samples */
	uint32_t read_idx;
	uint32_t write_idx;
	float feedback_gain;
	float buffer[];
} FAudioFXFilterAllPass;

FAudioFXFilterAllPass *FAudioFXFilterAllPass_Create(
	int32_t sampleRate,
	float delay_ms,
	float feedback_gain
);
void FAudioFXFilterAllPass_Change(FAudioFXFilterAllPass *filter, float delay_ms, float gain);
float FAudioFXFilterAllPass_Process(FAudioFXFilterAllPass *filter, float sample);
float FAudioFXFilterAllPass_Tap(FAudioFXFilterAllPass *filter, uint32_t delay);
void FAudioFXFilterAllPass_Destroy(FAudioFXFilterAllPass *filter);

#endif /* FAUDIOFX_INTERNAL_H */