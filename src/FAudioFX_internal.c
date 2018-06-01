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

#include "FAudioFX_internal.h"
#include "FAudio_internal.h"

#define  PI 3.1415926536f
#define  DENORMAL 1.0e-20f

/* gain / attenuation */
float FAudioFX_INTERNAL_DbGainToFactor(float gain)
{
	return (float)FAudio_pow(10, gain / 20.0f);
}

uint32_t FAudioFX_INTERNAL_MsToSamples(float msec, int32_t sampleRate)
{
	return (uint32_t)((sampleRate * msec) / 1000.0f);
}

/* basic delay filter - no feedback */
FAudioFXFilterDelay *FAudioFXFilterDelay_Create(int32_t sampleRate, float delay_ms, float rt60_ms)
{
	FAudio_assert(delay_ms >= 0 && delay_ms <= FAUDIOFX_DELAY_MAX_DELAY_MS);

	/* FIXME: round up to nearest power of 2 to use bitmask instead of mod */
	uint32_t cap = FAudioFX_INTERNAL_MsToSamples(FAUDIOFX_DELAY_MAX_DELAY_MS, sampleRate);
	
	FAudioFXFilterDelay *filter = (FAudioFXFilterDelay *) 
		FAudio_malloc(sizeof(FAudioFXFilterDelay) + cap * sizeof(float));

	filter->sampleRate = sampleRate;
	filter->capacity = cap;
	filter->delay = FAudioFX_INTERNAL_MsToSamples(delay_ms, sampleRate);
	filter->read_idx = 0;
	filter->write_idx = filter->delay;
	filter->denormal = DENORMAL;
	FAudio_zero(filter->buffer, cap);
	FAudioFXFilterDelay_Change(filter, delay_ms, rt60_ms);

	return filter;
}

void FAudioFXFilterDelay_Change(FAudioFXFilterDelay *filter, float delay_ms, float rt60_ms)
{
	FAudio_assert(filter != NULL);
	FAudio_assert(delay_ms >= 0 && delay_ms <= FAUDIOFX_DELAY_MAX_DELAY_MS);

	/* length */
	filter->delay = FAudioFX_INTERNAL_MsToSamples(delay_ms, filter->sampleRate);
	filter->read_idx = (filter->write_idx - filter->delay + filter->capacity) % filter->capacity;

	/* comb filter gain (convert from RT60) */
	if (rt60_ms > 0)
	{
		float exponent = (-3.0f * filter->delay * 1000.0f) / (filter->sampleRate * rt60_ms);
		filter->feedback = (float)FAudio_pow(10.0f, exponent);
	}
	else
	{
		filter->feedback = 0.0f;
	}
}

float FAudioFXFilterDelay_Process(FAudioFXFilterDelay *filter, float sample)
{
	FAudio_assert(filter != NULL);
	FAudio_assert(filter->read_idx < filter->capacity);
	FAudio_assert(filter->write_idx < filter->capacity);

	float delay_out = filter->buffer[filter->read_idx] + filter->denormal;
	filter->buffer[filter->write_idx] = sample + (filter->feedback * delay_out);

	filter->read_idx = (filter->read_idx + 1) % filter->capacity;
	filter->write_idx = (filter->write_idx + 1) % filter->capacity;
	filter->denormal = -filter->denormal;

	return delay_out;
}

float FAudioFXFilterDelay_Tap(FAudioFXFilterDelay *filter, uint32_t delay)
{
	return filter->buffer[(filter->write_idx - delay + filter->capacity) % filter->capacity];
}

void FAudioFXFilterDelay_Destroy(FAudioFXFilterDelay *filter)
{
	FAudio_free(filter);
}

/* comb filter with integrated lowpass filter */
FAudioFXFilterCombLpf *FAudioFXFilterCombLpf_Create(int32_t sampleRate, float delay_ms, float rt60_ms, float lpf_gain)
{
	FAudio_assert(delay_ms > 0 && delay_ms < FAUDIOFX_DELAY_MAX_DELAY_MS);

	/* FIXME: round up to nearest power of 2 to use bitmask instead of mod */
	uint32_t cap = FAudioFX_INTERNAL_MsToSamples(FAUDIOFX_DELAY_MAX_DELAY_MS, sampleRate);
	
	FAudioFXFilterCombLpf *filter = (FAudioFXFilterCombLpf *)
		FAudio_malloc(sizeof(FAudioFXFilterCombLpf) + cap * sizeof(float));

	filter->delay.sampleRate = sampleRate;
	filter->delay.capacity = cap;
	filter->delay.delay = FAudioFX_INTERNAL_MsToSamples(delay_ms, sampleRate);
	filter->delay.read_idx = 0;
	filter->delay.write_idx = filter->delay.delay;
	filter->delay.denormal = DENORMAL;
	filter->lpf_buffer = 0.0f;
	FAudio_zero(filter->delay.buffer, cap);
	FAudioFXFilterCombLpf_Change(filter, delay_ms, rt60_ms, lpf_gain);

	return filter;
}

void FAudioFXFilterCombLpf_Change(FAudioFXFilterCombLpf *filter, float delay_ms, float rt60_ms, float lpf_gain)
{
	FAudioFXFilterDelay_Change(&filter->delay, delay_ms, rt60_ms);

	/* low pass filter gain */
	filter->lpf_gain = lpf_gain * (1.0f - filter->delay.feedback);
}

float FAudioFXFilterCombLpf_Process(FAudioFXFilterCombLpf *filter, float sample)
{
	FAudio_assert(filter != NULL);
	FAudio_assert(filter->delay.read_idx < filter->delay.capacity);
	FAudio_assert(filter->delay.write_idx < filter->delay.capacity);

	float delay_out = filter->delay.buffer[filter->delay.read_idx];
	
	// lpf filter
	filter->lpf_buffer = delay_out + filter->lpf_gain * filter->lpf_buffer + filter->delay.denormal;

	// comb filter gain
	filter->delay.buffer[filter->delay.write_idx] = sample + (filter->delay.feedback * filter->lpf_buffer) + filter->delay.denormal;

	filter->delay.read_idx = (filter->delay.read_idx + 1) % filter->delay.capacity;
	filter->delay.write_idx = (filter->delay.write_idx + 1) % filter->delay.capacity;
	filter->delay.denormal = -filter->delay.denormal;

	return delay_out;
}


/* biquad filter */
FAudioFXFilterBiQuad *FAudioFXFilterBiQuad_Create(int32_t sampleRate, int type, float frequency, float q, float gain)
{
	FAudioFXFilterBiQuad *filter = (FAudioFXFilterBiQuad *)
		FAudio_malloc(sizeof(FAudioFXFilterBiQuad));
	FAudio_zero(filter, sizeof(FAudioFXFilterBiQuad));

	filter->type = type;
	filter->sampleRate = sampleRate;
	FAudioFXFilterBiQuad_Change(filter, frequency, q, gain);
	return filter;
}

void FAudioFXFilterBiQuad_Change(FAudioFXFilterBiQuad *filter, float frequency, float q, float gain)
{
	float theta_c = (2.0f * PI * frequency) / (float)filter->sampleRate;

	if (filter->type == FAUDIOFX_BIQUAD_LOWPASS || filter->type == FAUDIOFX_BIQUAD_HIGHPASS)
	{ 
		float sin_theta_c = (float) FAudio_sin(theta_c);
		float cos_theta_c = (float) FAudio_cos(theta_c);
		float oneOverQ = 1.0f / q;
		float beta = (1.0f - (oneOverQ * 0.5f * sin_theta_c)) / (1.0f + (oneOverQ *  0.5f * sin_theta_c));
		float gamma = (0.5f + beta) * cos_theta_c;

		if (filter->type == FAUDIOFX_BIQUAD_LOWPASS) 
		{
			filter->a0 = (0.5f + beta - gamma) * 0.5f;
			filter->a1 = filter->a0 * 2.0f;
			filter->a2 = (0.5f + beta - gamma) * 0.5f;
			filter->b1 = -2.0f * gamma;
			filter->b2 = 2.0f * beta;
		}
		else
		{
			filter->a0 = (0.5f + beta + gamma) * 0.5f;
			filter->a1 = filter->a0 * -2.0f;
			filter->a2 = (0.5f + beta + gamma) * 0.5f;
			filter->b1 = -2.0f * gamma;
			filter->b2 = 2.0f * beta;
		}
		filter->c0 = 1.0f;
		filter->d0 = 0.0f;
	}
	else if (filter->type == FAUDIOFX_BIQUAD_LOWSHELVING || filter->type == FAUDIOFX_BIQUAD_HIGHSHELVING)
	{
		float mu = FAudioFX_INTERNAL_DbGainToFactor(gain);
		float beta = (filter->type == FAUDIOFX_BIQUAD_LOWSHELVING) ? 4.0f / (1 + mu) : (1 + mu) / 4.0f;
		float delta = beta * (float) FAudio_tan(theta_c * 0.5f);
		float gamma = (1 - delta) / (1 + delta);

		if (filter->type == FAUDIOFX_BIQUAD_LOWSHELVING)
		{
			filter->a0 = (1 - gamma) * 0.5f;
			filter->a1 = filter->a0;
		}
		else
		{
			filter->a0 = (1 + gamma) * 0.5f;
			filter->a1 = -filter->a0;
		}

		filter->a2 = 0.0f;
		filter->b1 = -gamma;
		filter->b2 = 0.0f;
		filter->c0 = mu - 1.0f;
		filter->d0 = 1.0f;
	}
}

float FAudioFXFilterBiQuad_Process(FAudioFXFilterBiQuad *filter, float sample)
{
	float result = 
		filter->a0 * sample + 
		filter->a1 * filter->delay_x[0] + 
		filter->a2 * filter->delay_x[1] - 
		filter->b1 * filter->delay_y[0] - 
		filter->b2 * filter->delay_y[1];

	filter->delay_y[1] = filter->delay_y[0];
	filter->delay_y[0] = result;
	filter->delay_x[1] = filter->delay_x[0];
	filter->delay_x[0] = sample;

	result = (result * filter->c0) + (sample * filter->d0);

	return  result;
}

void FAudioFXFilterBiQuad_Destroy(FAudioFXFilterBiQuad *filter)
{
	FAudio_free(filter);
}

/* state variable filter */
FAudioFXFilterStateVariable *FAudioFXFilterStateVariable_Create(
	int32_t sampleRate,
	int32_t type,
	float frequency,
	float q
) {
	FAudioFXFilterStateVariable *filter = (FAudioFXFilterStateVariable *)
		FAudio_malloc(sizeof(FAudioFXFilterStateVariable));

	filter->type = type;
	filter->sampleRate = sampleRate;
	FAudio_zero(filter->state, 4 * sizeof(float));
	FAudioFXFilterStateVariable_Change(filter, frequency, q);

	return filter;
}

void FAudioFXFilterStateVariable_Change(FAudioFXFilterStateVariable *filter, float frequency, float q)
{
	filter->frequency = 2.0f * (float)FAudio_sin((3.1415926535897931 * frequency) / filter->sampleRate);
	filter->oneOverQ = 1.0f / q;
}

float FAudioFXFilterStateVariable_Process(FAudioFXFilterStateVariable *filter, float sample)
{
	filter->state[FAUDIOFX_STATEVARIABLE_LOWPASS] = 
		filter->state[FAUDIOFX_STATEVARIABLE_LOWPASS] + 
		(filter->frequency * filter->state[FAUDIOFX_STATEVARIABLE_BANDPASS]);
	filter->state[FAUDIOFX_STATEVARIABLE_HIGHPASS] = 
		sample - filter->state[FAUDIOFX_STATEVARIABLE_LOWPASS] - 
		(filter->oneOverQ * filter->state[FAUDIOFX_STATEVARIABLE_BANDPASS]);
	filter->state[FAUDIOFX_STATEVARIABLE_BANDPASS] = 
		(filter->frequency * filter->state[FAUDIOFX_STATEVARIABLE_HIGHPASS]) + 
		filter->state[FAUDIOFX_STATEVARIABLE_BANDPASS];
	filter->state[FAUDIOFX_STATEVARIABLE_NOTCH] = 
		filter->state[FAUDIOFX_STATEVARIABLE_HIGHPASS] + 
		filter->state[FAUDIOFX_STATEVARIABLE_LOWPASS];
	return filter->state[filter->type];
}

void FAudioFXFilterStateVariable_Destroy(FAudioFXFilterStateVariable *filter)
{
	FAudio_free(filter);
}

/* delaying all-pass filter */
FAudioFXFilterAllPass *FAudioFXFilterAllPass_Create(
	int32_t sampleRate,
	float delay_ms,
	float feedback_gain
) {
	FAudio_assert(delay_ms > 0 && delay_ms < FAUDIOFX_DELAY_MAX_DELAY_MS);

	/* FIXME: round up to nearest power of 2 to use bitmask instead of mod */
	uint32_t cap = FAudioFX_INTERNAL_MsToSamples(FAUDIOFX_DELAY_MAX_DELAY_MS, sampleRate);
	size_t size = sizeof(FAudioFXFilterAllPass) + cap * sizeof(float);

	FAudioFXFilterAllPass *filter = (FAudioFXFilterAllPass *) FAudio_malloc(size);
	FAudio_zero(filter, size);

	filter->sampleRate = sampleRate;
	filter->capacity = cap;
	filter->delay = FAudioFX_INTERNAL_MsToSamples(delay_ms, sampleRate);
	filter->read_idx = 0;
	filter->write_idx = filter->delay;
	filter->feedback_gain = feedback_gain;

	return filter;
}

void FAudioFXFilterAllPass_Change(FAudioFXFilterAllPass *filter, float delay_ms, float gain)
{
	/* gain */
	filter->feedback_gain = gain;

	/* length */
	filter->delay = FAudioFX_INTERNAL_MsToSamples(delay_ms, filter->sampleRate);
	filter->read_idx = (filter->write_idx - filter->delay + filter->capacity) % filter->capacity;
}

float FAudioFXFilterAllPass_Process(FAudioFXFilterAllPass *filter, float sample)
{
	float delay_out = filter->buffer[filter->read_idx];
	filter->read_idx = (filter->read_idx + 1) % filter->capacity;

	float buffer = sample + filter->feedback_gain * delay_out;

	filter->buffer[filter->write_idx] = buffer;
	filter->write_idx = (filter->write_idx + 1) % filter->capacity;

	return -filter->feedback_gain * buffer + delay_out;
}

float FAudioFXFilterAllPass_Tap(FAudioFXFilterAllPass *filter, uint32_t delay)
{
	return filter->buffer[(filter->write_idx - delay + filter->capacity) % filter->capacity];
}

void FAudioFXFilterAllPass_Destroy(FAudioFXFilterAllPass *filter)
{
	FAudio_free(filter);
}