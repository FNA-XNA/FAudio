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

#include "FAudioFX.h"
#include "FAudio_internal.h"

/* Constants */

#define PI 3.1415926536f
#define DSP_DELAY_MAX_DELAY_MS 300

/* Utility Functions */

static inline float DbGainToFactor(float gain)
{
	return (float) FAudio_pow(10, gain / 20.0f);
}

static inline uint32_t MsToSamples(float msec, int32_t sampleRate)
{
	return (uint32_t) ((sampleRate * msec) / 1000.0f);
}

static inline float Undenormalize(float sample_in)
{
	union
	{
		float f;
		uint32_t i;
	} sampleHack;
	uint32_t exponent;
	int32_t denormal_factor;

	/* Set denormal (or subnormal) float values to zero.
	 * (See: https://en.wikipedia.org/wiki/Denormal_number)
	 * Based upon code available from:
	 * http://www.musicdsp.org/archive.php?classid=5#191)
	 */
	sampleHack.f = sample_in;
	exponent = sampleHack.i & 0x7F800000;

	/* exponent > 0 is 0 if denormalized, otherwise 1 */
	denormal_factor = exponent > 0;

	return sample_in * denormal_factor;
}

/* Component - Delay */

typedef struct DspDelay
{
	int32_t	 sampleRate;
	uint32_t capacity;	/* In samples */
	uint32_t delay;		/* In samples */
	uint32_t read_idx;
	uint32_t write_idx;
	float *buffer;
} DspDelay;

static void DspDelay_Initialize(
	DspDelay *filter,
	int32_t sampleRate,
	float delay_ms,
	FAudioMallocFunc pMalloc
) {
	FAudio_assert(delay_ms >= 0 && delay_ms <= DSP_DELAY_MAX_DELAY_MS);
	FAudio_assert(filter != NULL);

	filter->sampleRate = sampleRate;
	filter->capacity = MsToSamples(DSP_DELAY_MAX_DELAY_MS, sampleRate);
	filter->delay = MsToSamples(delay_ms, sampleRate);
	filter->read_idx = 0;
	filter->write_idx = filter->delay;
	filter->buffer = (float*) pMalloc(filter->capacity * sizeof(float));
	FAudio_zero(filter->buffer, filter->capacity * sizeof(float));
}

static void DspDelay_Change(DspDelay *filter, float delay_ms)
{
	FAudio_assert(filter != NULL);
	FAudio_assert(delay_ms >= 0 && delay_ms <= DSP_DELAY_MAX_DELAY_MS);

	/* length */
	filter->delay = MsToSamples(delay_ms, filter->sampleRate);
	filter->read_idx = (filter->write_idx - filter->delay + filter->capacity) % filter->capacity;
}

static inline float DspDelay_Read(DspDelay *filter)
{
	float delay_out;

	FAudio_assert(filter != NULL);
	FAudio_assert(filter->read_idx < filter->capacity);

	delay_out = filter->buffer[filter->read_idx];
	filter->read_idx = (filter->read_idx + 1) % filter->capacity;
	return delay_out;
}

static inline void DspDelay_Write(DspDelay *filter, float sample)
{
	FAudio_assert(filter != NULL);
	FAudio_assert(filter->write_idx < filter->capacity);

	filter->buffer[filter->write_idx] = sample;
	filter->write_idx = (filter->write_idx + 1) % filter->capacity;
}

static inline float DspDelay_Process(DspDelay *filter, float sample_in)
{
	float delay_out;

	FAudio_assert(filter != NULL);

	delay_out = DspDelay_Read(filter);
	DspDelay_Write(filter, sample_in);

	return delay_out;
}

static inline float DspDelay_Tap(DspDelay *filter, uint32_t delay)
{
	FAudio_assert(filter != NULL);
	FAudio_assert(delay <= filter->delay);
	return filter->buffer[(filter->write_idx - delay + filter->capacity) % filter->capacity];
}

static inline void DspDelay_Reset(DspDelay *filter)
{
	FAudio_assert(filter != NULL);
	filter->read_idx = 0;
	filter->write_idx = filter->delay;
	FAudio_zero(filter->buffer, filter->capacity * sizeof(float));
}

static void DspDelay_Destroy(DspDelay *filter, FAudioFreeFunc pFree)
{
	FAudio_assert(filter != NULL);
	pFree(filter->buffer);
}

/* Component - Comb Filter */

typedef struct DspComb
{
	DspDelay delay;
	float feedback_gain;
} DspComb;

static inline float DspComb_FeedbackFromRT60(DspComb *filter, float rt60_ms)
{
	float exponent;

	if (rt60_ms == 0)
	{
		return 0;
	}

	exponent = (
		(-3.0f * filter->delay.delay * 1000.0f) /
		(filter->delay.sampleRate * rt60_ms)
	);
	return (float) FAudio_pow(10.0f, exponent);
}

static void DspComb_Initialize(
	DspComb *filter,
	int32_t sampleRate,
	float delay_ms,
	float rt60_ms,
	FAudioMallocFunc pMalloc
) {
	FAudio_assert(filter != NULL);

	DspDelay_Initialize(&filter->delay, sampleRate, delay_ms, pMalloc);
	filter->feedback_gain = DspComb_FeedbackFromRT60(filter, rt60_ms);
}

static void DspComb_Change(DspComb *filter, float delay_ms, float rt60_ms)
{
	FAudio_assert(filter != NULL);

	DspDelay_Change(&filter->delay, delay_ms);
	filter->feedback_gain = DspComb_FeedbackFromRT60(filter, rt60_ms);
}

static inline float DspComb_Process(DspComb *filter, float sample_in)
{
	float delay_out, to_buf;

	FAudio_assert(filter != NULL);

	delay_out = DspDelay_Read(&filter->delay);

	to_buf = Undenormalize(sample_in + (filter->feedback_gain * delay_out));
	DspDelay_Write(&filter->delay, to_buf);

	return delay_out;
}

static inline void DspComb_Reset(DspComb *filter)
{
	DspDelay_Reset(&filter->delay);
}

static void DspComb_Destroy(DspComb *filter, FAudioFreeFunc pFree)
{
	FAudio_assert(filter != NULL);
	DspDelay_Destroy(&filter->delay, pFree);
}

/* Component - Bi-Quad Filter */

typedef enum DspBiQuadType
{
	DSP_BIQUAD_LOWPASS = 0,
	DSP_BIQUAD_HIGHPASS,
	DSP_BIQUAD_LOWSHELVING,
	DSP_BIQUAD_HIGHSHELVING
} DspBiQuadType;

typedef struct DspBiQuad
{
	DspBiQuadType type;
	int32_t sampleRate;
	float a0, a1, a2;
	float b1, b2;
	float c0, d0;
	float delay[2];
} DspBiQuad;

static void DspBiQuad_Change(
	DspBiQuad *filter,
	float frequency,
	float q,
	float gain
) {
	float theta_c;

	FAudio_assert(filter != NULL);
	theta_c = (2.0f * PI * frequency) / (float) filter->sampleRate;

	if (	filter->type == DSP_BIQUAD_LOWPASS ||
		filter->type == DSP_BIQUAD_HIGHPASS	)
	{
		float sin_theta_c = (float) FAudio_sin(theta_c);
		float cos_theta_c = (float) FAudio_cos(theta_c);
		float oneOverQ = 1.0f / q;
		float beta = (
			(1.0f - (oneOverQ * 0.5f * sin_theta_c)) /
			(1.0f + (oneOverQ *  0.5f * sin_theta_c))
		);
		float gamma = (0.5f + beta) * cos_theta_c;

		if (filter->type == DSP_BIQUAD_LOWPASS)
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
	else if (	filter->type == DSP_BIQUAD_LOWSHELVING ||
			filter->type == DSP_BIQUAD_HIGHSHELVING	)
	{
		float mu = DbGainToFactor(gain);
		float beta = (filter->type == DSP_BIQUAD_LOWSHELVING) ?
			4.0f / (1 + mu) :
			(1 + mu) / 4.0f;
		float delta = beta * (float) FAudio_tan(theta_c * 0.5f);
		float gamma = (1 - delta) / (1 + delta);

		if (filter->type == DSP_BIQUAD_LOWSHELVING)
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

static void DspBiQuad_Initialize(
	DspBiQuad *filter,
	int32_t sampleRate,
	DspBiQuadType type,
	float frequency,	/* Corner frequency */
	float q,		/* Only used by low/high-pass filters */
	float gain		/* Only used by low/high-shelving filters */
) {
	FAudio_assert(filter != NULL);

	FAudio_zero(filter, sizeof(DspBiQuad));
	filter->type = type;
	filter->sampleRate = sampleRate;
	DspBiQuad_Change(filter, frequency, q, gain);
}

static inline float DspBiQuad_Process(DspBiQuad *filter, float sample_in)
{
	/* Direct Form II Transposed:
	 * - Less delay registers than Direct Form I
	 * - More numerically stable than Direct Form II
	 */
	float result = (filter->a0 * sample_in) + filter->delay[0];
	filter->delay[0] = (filter->a1 * sample_in) - (filter->b1 * result) + filter->delay[1];
	filter->delay[1] = (filter->a2 * sample_in) - (filter->b2 * result);

	result = Undenormalize(
		(result * filter->c0) +
		(sample_in * filter->d0)
	);

	return  result;
}

static inline void DspBiQuad_Reset(DspBiQuad *filter)
{
	FAudio_assert(filter != NULL);
	FAudio_zero(&filter->delay, sizeof(filter->delay));
}

static void DspBiQuad_Destroy(DspBiQuad *filter)
{
}

/* Component - Comb Filter with Integrated Low/High Shelving Filters */

typedef struct DspCombShelving
{
	DspComb comb;
	DspBiQuad low_shelving;
	DspBiQuad high_shelving;
} DspCombShelving;

static void DspCombShelving_Initialize(
	DspCombShelving *filter,
	int32_t sampleRate,
	float delay_ms,
	float rt60_ms,
	float low_frequency,
	float low_gain,
	float high_frequency,
	float high_gain,
	FAudioMallocFunc pMalloc
) {
	FAudio_assert(filter != NULL);
	DspComb_Initialize(
		&filter->comb,
		sampleRate,
		delay_ms,
		rt60_ms,
		pMalloc
	);
	DspBiQuad_Initialize(
		&filter->low_shelving,
		sampleRate,
		DSP_BIQUAD_LOWSHELVING,
		low_frequency,
		0.0f,
		low_gain
	);
	DspBiQuad_Initialize(
		&filter->high_shelving,
		sampleRate,
		DSP_BIQUAD_HIGHSHELVING,
		high_frequency,
		0.0f,
		high_gain
	);
}

static inline float DspCombShelving_Process(
	DspCombShelving *filter,
	float sample_in
) {
	float delay_out, feedback, to_buf;

	FAudio_assert(filter != NULL);

	delay_out = DspDelay_Read(&filter->comb.delay);

	/* Apply shelving filters */
	feedback = DspBiQuad_Process(&filter->high_shelving, delay_out);
	feedback = DspBiQuad_Process(&filter->low_shelving, feedback);

	/* Apply comb filter */
	to_buf = Undenormalize(sample_in + (filter->comb.feedback_gain * feedback));
	DspDelay_Write(&filter->comb.delay, to_buf);

	return delay_out;
}

static void DspCombShelving_Reset(DspCombShelving *filter)
{
	FAudio_assert(filter != NULL);

	DspComb_Reset(&filter->comb);
	DspBiQuad_Reset(&filter->low_shelving);
	DspBiQuad_Reset(&filter->high_shelving);
}

static void DspCombShelving_Destroy(
	DspCombShelving *filter,
	FAudioFreeFunc pFree
) {
	FAudio_assert(filter != NULL);

	DspComb_Destroy(&filter->comb, pFree);
	DspBiQuad_Destroy(&filter->low_shelving);
	DspBiQuad_Destroy(&filter->high_shelving);
}

/* Component - Delaying All-Pass Filter */

typedef struct DspAllPass
{
	DspDelay delay;
	float feedback_gain;
} DspAllPass;

static void DspAllPass_Initialize(
	DspAllPass *filter,
	int32_t sampleRate,
	float delay_ms,
	float gain,
	FAudioMallocFunc pMalloc
) {
	FAudio_assert(filter != NULL);

	DspDelay_Initialize(&filter->delay, sampleRate, delay_ms, pMalloc);
	filter->feedback_gain = gain;
}

static void DspAllPass_Change(DspAllPass *filter, float delay_ms, float gain)
{
	FAudio_assert(filter != NULL);

	DspDelay_Change(&filter->delay, delay_ms);
	filter->feedback_gain = gain;
}

static inline float DspAllPass_Process(DspAllPass *filter, float sample_in)
{
	float delay_out, to_buf;

	FAudio_assert(filter != NULL);

	delay_out = DspDelay_Read(&filter->delay);

	to_buf = Undenormalize(sample_in + (filter->feedback_gain * delay_out));
	DspDelay_Write(&filter->delay, to_buf);

	return Undenormalize(delay_out - (filter->feedback_gain * to_buf));
}

static void DspAllPass_Reset(DspAllPass *filter)
{
	FAudio_assert(filter != NULL);
	DspDelay_Reset(&filter->delay);
}

static void DspAllPass_Destroy(DspAllPass *filter, FAudioFreeFunc pFree)
{
	FAudio_assert(filter != NULL);
	DspDelay_Destroy(&filter->delay, pFree);
}

/*
Reverb network - loosely based on the reverberator from
"Designing Audio Effect Plug-Ins in C++" by Will Pirkle and
the classic classic Schroeder-Moorer reverberator with modifications
to fit the XAudio2FX parameters.

                                                                         
    In    +--------+   +----+      +------------+      +-----+           
  ----|--->PreDelay---->APF1---+--->Sub LeftCh  |----->|     |   Left Out
      |   +--------+   +----+  |   +------------+      | Wet |-------->  
      |                        |   +------------+      |     |           
      |                        |---|Sub RightCh |----->| Dry |           
      |                            +------------+      |     |  Right Out
      |                                                | Mix |-------->  
      +----------------------------------------------->|     |           
                                                       +-----+           
 Sub routine per channel :                                                                    
                                                                         
     In  +-----+      +-----+ * cg                                       
   ---+->|Delay|--+---|Comb1|------+                                     
      |  +-----+  |   +-----+      |                                     
      |           |                |                                     
      |           |   +-----+ * cg |                                     
      |           +--->Comb2|------+                                     
      |           |   +-----+      |     +-----+                         
      |           |                +---->| SUM |--------+                
      |           |   +-----+ * cg |     +-----+        |                
      |           +--->...  |------+                    |                
      | * g0      |   +-----+      |                    |                
      |           |                |                    |                
      |           +--->-----+ * cg |                    |                
      |               |Comb8|------+                    |                
      |               +-----+                           |                
      v                                                 |                
   +-----+  g1   +----+   +----+   +----+   +----+      |                
   | SUM |<------|APF4|<--|APF3|<--|APF2|<--|APF1|<-----+                
   +-----+       +----+   +----+   +----+   +----+                       
      |                                                                  
      |                                                                  
      |            +-------------+          Out                          
      +----------->|RoomFilter   |------------------------>              
                   +-------------+                                       
                                                                         

Parameters:

float WetDryMix;		0 - 100 (0 = fully dry, 100 = fully wet)
uint32_t ReflectionsDelay;	0 - 300 ms
uint8_t ReverbDelay;		0 - 85 ms
uint8_t RearDelay;		0 - 5 ms
uint8_t PositionLeft;		0 - 30
uint8_t PositionRight;		0 - 30
uint8_t PositionMatrixLeft;	0 - 30
uint8_t PositionMatrixRight;	0 - 30
uint8_t EarlyDiffusion;		0 - 15
uint8_t LateDiffusion;		0 - 15
uint8_t LowEQGain;		0 - 12 (formula dB = LowEQGain - 8)
uint8_t LowEQCutoff;		0 - 9  (formula Hz = 50 + (LowEQCutoff * 50))
uint8_t HighEQGain;		0 - 8  (formula dB = HighEQGain - 8)
uint8_t HighEQCutoff;		0 - 14 (formula Hz = 1000 + (HighEqCutoff * 500))
float RoomFilterFreq;		20 - 20000Hz
float RoomFilterMain;		-100 - 0dB
float RoomFilterHF;		-100 - 0dB
float ReflectionsGain;		-100 - 20dB
float ReverbGain;		-100 - 20dB
float DecayTime;		0.1 - .... ms
float Density;			0 - 100 %
float RoomSize;			1 - 100 feet (NOT USED YET)

*/

#define REVERB_COUNT_COMB	8
#define REVERB_COUNT_APF_IN	1
#define REVERB_COUNT_APF_OUT	4

static float COMB_DELAYS[REVERB_COUNT_COMB] =
{
	25.31f,
	26.94f,
	28.96f,
	30.75f,
	32.24f,
	33.80f,
	35.31f,
	36.67f
};

static float APF_IN_DELAYS[REVERB_COUNT_APF_IN] =
{
	13.28f,
/*	28.13f */
};

static float APF_OUT_DELAYS[REVERB_COUNT_APF_OUT] =
{
	5.10f,
	12.61f,
	10.0f,
	7.73f
};

static const float STEREO_SPREAD[4] =
{
	0.0f,
	0.5216f,
	0.0f,
	0.5216f
};

typedef struct DspReverbChannel
{
	DspDelay reverb_delay;
	DspCombShelving	lpf_comb[REVERB_COUNT_COMB];
	DspAllPass	apf_out[REVERB_COUNT_APF_OUT];
	DspBiQuad room_high_shelf;
	float early_gain;
	float gain;
} DspReverbChannel;

typedef struct DspReverb
{
	DspDelay early_delay;
	DspAllPass apf_in[REVERB_COUNT_APF_IN];
	
	int32_t in_channels;
	int32_t out_channels;
	int32_t reverb_channels;
	DspReverbChannel channel[4];

	float early_gain;
	float reverb_gain;
	float room_gain;
	float wet_ratio;
	float dry_ratio;
} DspReverb;

static DspReverb* DspReverb_Create(
	int32_t sampleRate,
	int32_t in_channels,
	int32_t out_channels,
	FAudioMallocFunc pMalloc
) {
	DspReverb *reverb;
	int32_t i, c;

	FAudio_assert(in_channels == 1 || in_channels == 2);
	FAudio_assert(out_channels == 1 || out_channels == 2 || out_channels == 6);

	reverb = (DspReverb*) pMalloc(sizeof(DspReverb));
	FAudio_zero(reverb, sizeof(DspReverb));
	DspDelay_Initialize(&reverb->early_delay, sampleRate, 10, pMalloc);

	for (i = 0; i < REVERB_COUNT_APF_IN; i += 1)
	{
		DspAllPass_Initialize(
			&reverb->apf_in[i],
			sampleRate,
			APF_IN_DELAYS[i],
			0.5f,
			pMalloc
		);
	}

	reverb->reverb_channels = (out_channels == 6) ? 4 : out_channels;

	for (c = 0; c < reverb->reverb_channels; c += 1)
	{
		DspDelay_Initialize(
			&reverb->channel[c].reverb_delay,
			sampleRate,
			10,
			pMalloc
		);

		for (i = 0; i < REVERB_COUNT_COMB; i += 1)
		{
			DspCombShelving_Initialize(
				&reverb->channel[c].lpf_comb[i],
				sampleRate,
				COMB_DELAYS[i] + STEREO_SPREAD[c],
				500,
				500,
				-6,
				5000,
				-6,
				pMalloc
			);
		}

		for (i = 0; i < REVERB_COUNT_APF_OUT; i += 1)
		{
			DspAllPass_Initialize(
				&reverb->channel[c].apf_out[i],
				sampleRate,
				APF_OUT_DELAYS[i] + STEREO_SPREAD[c],
				0.5f,
				pMalloc
			);
		}

		DspBiQuad_Initialize(
			&reverb->channel[c].room_high_shelf,
			sampleRate,
			DSP_BIQUAD_HIGHSHELVING,
			5000,
			0,
			-10
		);
		reverb->channel[c].gain = 1.0f;
	}

	reverb->early_gain = 1.0f;
	reverb->reverb_gain = 1.0f;
	reverb->dry_ratio = 0.0f;
	reverb->wet_ratio = 1.0f;
	reverb->in_channels = in_channels;
	reverb->out_channels = out_channels;

	return reverb;
}

static void DspReverb_Destroy(DspReverb *reverb, FAudioFreeFunc pFree)
{
	int32_t i, c;

	DspDelay_Destroy(&reverb->early_delay, pFree);

	for (i = 0; i < REVERB_COUNT_APF_IN; i += 1)
	{
		DspAllPass_Destroy(&reverb->apf_in[i], pFree);
	}

	for (c = 0; c < reverb->reverb_channels; c += 1)
	{
		DspDelay_Destroy(&reverb->channel[c].reverb_delay, pFree);

		for (i = 0; i < REVERB_COUNT_COMB; i += 1)
		{
			DspCombShelving_Destroy(
				&reverb->channel[c].lpf_comb[i],
				pFree
			);
		}

		DspBiQuad_Destroy(&reverb->channel[c].room_high_shelf);

		for (i = 0; i < REVERB_COUNT_APF_OUT; i += 1)
		{
			DspAllPass_Destroy(
				&reverb->channel[c].apf_out[i],
				pFree
			);
		}
	}

	pFree(reverb);
}

static void DspReverb_SetParameters(
	DspReverb *reverb,
	FAudioFXReverbParameters *params
) {
	float early_diffusion, late_diffusion;
	float channel_delay[4] =
	{
		0.0f,
		0.0f,
		params->RearDelay,
		params->RearDelay
	};
	int32_t i, c;

	/* Pre-Delay */
	DspDelay_Change(&reverb->early_delay, (float) params->ReflectionsDelay);

	/* Early Reflections - Diffusion */
	early_diffusion = 0.6f - ((params->EarlyDiffusion / 15.0f) * 0.2f);

	for (i = 0; i < REVERB_COUNT_APF_IN; i += 1)
	{
		DspAllPass_Change(
			&reverb->apf_in[i],
			APF_IN_DELAYS[i],
			early_diffusion
		);
	}

	/* Reverberation */
	for (c = 0; c < reverb->reverb_channels; c += 1)
	{
		DspDelay_Change(
			&reverb->channel[c].reverb_delay,
			(float) params->ReverbDelay + channel_delay[c]
		);

		for (i = 0; i < REVERB_COUNT_COMB; i += 1)
		{
			/* Set decay time of comb filter */
			DspComb_Change(
				&reverb->channel[c].lpf_comb[i].comb,
				COMB_DELAYS[i] + STEREO_SPREAD[c],
				params->DecayTime * 1000.0f);

			/* High/Low shelving */
			DspBiQuad_Change(
				&reverb->channel[c].lpf_comb[i].low_shelving,
				50.0f + params->LowEQCutoff * 50.0f,
				0.0f,
				params->LowEQGain - 8.0f
			);
			DspBiQuad_Change(
				&reverb->channel[c].lpf_comb[i].high_shelving,
				1000 + params->HighEQCutoff * 500.0f,
				0.0f,
				params->HighEQGain - 8.0f
			);
		}
	}

	/* Gain */
	reverb->early_gain = DbGainToFactor(params->ReflectionsGain);
	reverb->reverb_gain = DbGainToFactor(params->ReverbGain);
	reverb->room_gain = DbGainToFactor(params->RoomFilterMain);

	/* Late Diffusion */
	late_diffusion = 0.6f - ((params->LateDiffusion / 15.0f) * 0.2f);

	for (c = 0; c < reverb->reverb_channels; c += 1)
	{
		for (i = 0; i < REVERB_COUNT_APF_OUT; i += 1)
		{
			DspAllPass_Change(
				&reverb->channel[c].apf_out[i],
				APF_OUT_DELAYS[i] + STEREO_SPREAD[c],
				late_diffusion
			);
		}

		DspBiQuad_Change(
			&reverb->channel[c].room_high_shelf,
			params->RoomFilterFreq,
			0.0f,
			params->RoomFilterMain + params->RoomFilterHF);

		reverb->channel[c].gain = 1.5f - (
			((c % 2 == 0 ?
				params->PositionMatrixLeft :
				params->PositionMatrixRight
			) / 27.0f) * 0.5f
		);
		if (c >= 2)
		{
			/* Rear-channel Attenuation */
			reverb->channel[c].gain *= 0.75f;
		}

		reverb->channel[c].early_gain = 1.2f - (
			((c % 2 == 0 ?
				params->PositionLeft :
				params->PositionRight
			) / 6.0f) * 0.2f
		);
		reverb->channel[c].early_gain = (
			reverb->channel[c].early_gain *
			reverb->early_gain
		);
	}

	/* Wet/Dry Mix (100 = fully wet, 0 = fully dry) */
	reverb->wet_ratio = params->WetDryMix / 100.0f;
	reverb->dry_ratio = 1.0f - reverb->wet_ratio;
}

static inline float DspReverb_INTERNAL_ProcessEarly(
	DspReverb *reverb,
	float sample_in
) {
	float early;
	int32_t i;

	/* Pre-Delay */
	early = DspDelay_Process(&reverb->early_delay, sample_in);

	/* Early Reflections */
	for (i = 0; i < REVERB_COUNT_APF_IN; i += 1)
	{
		early = DspAllPass_Process(&reverb->apf_in[i], early);
	}

	return early;
}

static inline float DspReverb_INTERNAL_ProcessChannel(
	DspReverb *reverb,
	DspReverbChannel *channel,
	float sample_in
) {
	float revdelay, early_late, sample_out;
	int32_t i;

	revdelay = DspDelay_Process(&channel->reverb_delay, sample_in);

	sample_out = 0.0f;
	for (i = 0; i < REVERB_COUNT_COMB; i += 1)
	{
		sample_out += DspCombShelving_Process(
			&channel->lpf_comb[i],
			revdelay
		);
	}
	sample_out /= (float) REVERB_COUNT_COMB;

	/* Output Diffusion */
	for (i = 0; i < REVERB_COUNT_APF_OUT; i += 1)
	{
		sample_out = DspAllPass_Process(
			&channel->apf_out[i],
			sample_out
		);
	}

	/* Combine early reflections and reverberation */
	early_late = (
		(sample_in * channel->early_gain) +
		(sample_out * reverb->reverb_gain)
	);

	/* Room filter */
	sample_out = DspBiQuad_Process(
		&channel->room_high_shelf,
		early_late * reverb->room_gain
	);

	/* PositionMatrixLeft/Right */
	return sample_out * channel->gain;
}

/* Reverb Process Functions */

static inline float DspReverb_INTERNAL_Process_1_to_1(
	DspReverb *reverb,
	float *restrict samples_in,
	float *restrict samples_out,
	size_t sample_count
) {
	const float *in_end = samples_in + sample_count;
	float in, early, late, out;
	float squared_sum = 0.0f;

	while (samples_in < in_end)
	{
		/* Input */
		in = *samples_in++;

		/* Early Reflections */
		early = DspReverb_INTERNAL_ProcessEarly(reverb, in);

		/* Reverberation */
		late = DspReverb_INTERNAL_ProcessChannel(
			reverb,
			&reverb->channel[0],
			early
		);

		/* Wet/Dry Mix */
		out = (late * reverb->wet_ratio) + (in * reverb->dry_ratio);
		squared_sum += out * out;

		/* Output */
		*samples_out++ = out;
	}

	return squared_sum;
}

static inline float DspReverb_INTERNAL_Process_1_to_5p1(
	DspReverb *reverb,
	float *restrict samples_in,
	float *restrict samples_out,
	size_t sample_count
) {
	const float *in_end = samples_in + sample_count;
	float in, in_ratio, early, late[4];
	float squared_sum = 0.0f;
	int32_t c;

	while (samples_in < in_end)
	{
		/* Input */
		in = *samples_in++;
		in_ratio = in * reverb->dry_ratio;

		/* Early Reflections */
		early = DspReverb_INTERNAL_ProcessEarly(reverb, in);

		/* Reverberation with Wet/Dry Mix */
		for (c = 0; c < 4; c += 1)
		{
			late[c] = (DspReverb_INTERNAL_ProcessChannel(
				reverb,
				&reverb->channel[c],
				early
			) * reverb->wet_ratio) + in_ratio;
			squared_sum += late[c] * late[c];
		}

		/* Output */
		*samples_out++ = late[0];	/* Front Left */
		*samples_out++ = late[1];	/* Front Right */
		*samples_out++ = 0.0f;		/* Center */
		*samples_out++ = 0.0f;		/* LFE */
		*samples_out++ = late[2];	/* Rear Left */
		*samples_out++ = late[3];	/* Rear Right */
	}

	return squared_sum;
}

static inline float DspReverb_INTERNAL_Process_2_to_2(
	DspReverb *reverb,
	float *restrict samples_in,
	float *restrict samples_out,
	size_t sample_count
) {
	const float *in_end = samples_in + sample_count;
	float in, in_ratio, early, late[2];
	float squared_sum = 0;

	while (samples_in < in_end)
	{
		/* Input - Combine 2 channels into 1 */
		in = (samples_in[0] + samples_in[1]) / 2.0f;
		in_ratio = in * reverb->dry_ratio;
		samples_in += 2;

		/* Early Reflections */
		early = DspReverb_INTERNAL_ProcessEarly(reverb, in);

		/* Reverberation with Wet/Dry Mix */
		late[0] = DspReverb_INTERNAL_ProcessChannel(
			reverb,
			&reverb->channel[0],
			early
		);
		late[1] = (DspReverb_INTERNAL_ProcessChannel(
			reverb,
			&reverb->channel[1],
			early
		) * reverb->wet_ratio) + in_ratio;
		squared_sum += (late[0] * late[0]) + (late[1] * late[1]);

		/* Output */
		*samples_out++ = late[0];
		*samples_out++ = late[1];
	}

	return squared_sum;
}

static inline float DspReverb_INTERNAL_Process_2_to_5p1(
	DspReverb *reverb,
	float *restrict samples_in,
	float *restrict samples_out,
	size_t sample_count
) {
	const float *in_end = samples_in + sample_count;
	float in, in_ratio, early, late[4];
	float squared_sum = 0;
	int32_t c;

	while (samples_in < in_end)
	{
		/* Input - Combine 2 channels into 1 */
		in = (samples_in[0] + samples_in[1]) / 2.0f;
		in_ratio = in * reverb->dry_ratio;
		samples_in += 2;

		/* Early Reflections */
		early = DspReverb_INTERNAL_ProcessEarly(reverb, in);

		/* Reverberation with Wet/Dry Mix */
		for (c = 0; c < 4; c += 1)
		{
			late[c] = (DspReverb_INTERNAL_ProcessChannel(
				reverb,
				&reverb->channel[c],
				early
			) * reverb->wet_ratio) + in_ratio;
			squared_sum += late[c] * late[c];
		}

		/* Output */
		*samples_out++ = late[0];	/* Front Left */
		*samples_out++ = late[1];	/* Front Right */
		*samples_out++ = 0.0f;		/* Center */
		*samples_out++ = 0.0f;		/* LFE */
		*samples_out++ = late[2];	/* Rear Left */
		*samples_out++ = late[3];	/* Rear Right */
	}

	return squared_sum;
}

#undef OUTPUT_SAMPLE

/* Reverb FAPO Implementation */

const FAudioGUID FAudioFX_CLSID_AudioReverb = /* 2.7 */
{
	0x6A93130E,
	0xCB4E,
	0x4CE1,
	{
		0xA9,
		0xCF,
		0xE7,
		0x58,
		0x80,
		0x0B,
		0xB1,
		0x79
	}
};

static FAPORegistrationProperties ReverbProperties =
{
	/* .clsid = */ {0},
	/*.FriendlyName = */
	{
		'R', 'e', 'v', 'e', 'r', 'b', '\0'
	},
	/*.CopyrightInfo = */ {
		'C', 'o', 'p', 'y', 'r', 'i', 'g', 'h', 't', ' ', '(', 'c', ')',
		'E', 't', 'h', 'a', 'n', ' ', 'L', 'e', 'e', '\0'
	},
	/*.MajorVersion = */ 0,
	/*.MinorVersion = */ 0,
	/*.Flags = */ (
		FAPO_FLAG_FRAMERATE_MUST_MATCH |
		FAPO_FLAG_BITSPERSAMPLE_MUST_MATCH |
		FAPO_FLAG_BUFFERCOUNT_MUST_MATCH |
		FAPO_FLAG_INPLACE_SUPPORTED
	),
	/*.MinInputBufferCount = */ 1,
	/*.MaxInputBufferCount = */ 1,
	/*.MinOutputBufferCount = */ 1,
	/*.MaxOutputBufferCount = */ 1
};

typedef struct FAudioFXReverb
{
	FAPOBase base;

	uint16_t inChannels;
	uint16_t outChannels;
	uint32_t sampleRate;
	uint16_t inBlockAlign;
	uint16_t outBlockAlign;

	DspReverb *reverb;
} FAudioFXReverb;

static inline int8_t IsFloatFormat(const FAudioWaveFormatEx *format)
{
	if (format->wFormatTag == FAUDIO_FORMAT_IEEE_FLOAT)
	{
		/* Plain ol' WaveFormatEx */
		return 1;
	}

	if (format->wFormatTag == FAUDIO_FORMAT_EXTENSIBLE)
	{
		/* WaveFormatExtensible, match GUID */
		#define MAKE_SUBFORMAT_GUID(guid, fmt) \
			static FAudioGUID KSDATAFORMAT_SUBTYPE_##guid = \
			{ \
				(uint16_t) (fmt), 0x0000, 0x0010, \
				{ \
					0x80, 0x00, 0x00, 0xaa, \
					0x00, 0x38, 0x9b, 0x71 \
				} \
			}
		MAKE_SUBFORMAT_GUID(IEEE_FLOAT, 3);
		#undef MAKE_SUBFORMAT_GUID

		if (FAudio_memcmp(
			&((FAudioWaveFormatExtensible*) format)->SubFormat,
			&KSDATAFORMAT_SUBTYPE_IEEE_FLOAT,
			sizeof(FAudioGUID)
		) == 0) {
			return 1;
		}
	}

	return 0;
}

uint32_t FAudioFXReverb_IsInputFormatSupported(
	FAPOBase *fapo,
	const FAudioWaveFormatEx *pOutputFormat,
	const FAudioWaveFormatEx *pRequestedInputFormat,
	FAudioWaveFormatEx **ppSupportedInputFormat
) {
	uint32_t result = 0;

#define SET_SUPPORTED_FIELD(field, value)	\
	result = 1;	\
	if (ppSupportedInputFormat && *ppSupportedInputFormat)	\
	{	\
		(*ppSupportedInputFormat)->field = (value);	\
	}

	/* sample rate */
	if (pOutputFormat->nSamplesPerSec != pRequestedInputFormat->nSamplesPerSec)
	{
		SET_SUPPORTED_FIELD(nSamplesPerSec, pOutputFormat->nSamplesPerSec);
	}

	/* data type */
	if (!IsFloatFormat(pRequestedInputFormat))
	{
		SET_SUPPORTED_FIELD(wFormatTag, FAUDIO_FORMAT_IEEE_FLOAT);
	}

	/* number of input / output channels */
	if (pOutputFormat->nChannels == 1 || pOutputFormat->nChannels == 2)
	{
		if (pRequestedInputFormat->nChannels != pOutputFormat->nChannels)
		{
			SET_SUPPORTED_FIELD(nChannels, pOutputFormat->nChannels);
		}
	}
	else if (pOutputFormat->nChannels == 6)
	{
		if (	pRequestedInputFormat->nChannels != 1 &&
			pRequestedInputFormat->nChannels != 2	)
		{
			SET_SUPPORTED_FIELD(nChannels, 1);
		}
	}
	else
	{
		SET_SUPPORTED_FIELD(nChannels, 1);
	}

#undef SET_SUPPORTED_FIELD

	return result;
}


uint32_t FAudioFXReverb_IsOutputFormatSupported(
	FAPOBase *fapo,
	const FAudioWaveFormatEx *pInputFormat,
	const FAudioWaveFormatEx *pRequestedOutputFormat,
	FAudioWaveFormatEx **ppSupportedOutputFormat
) {
	uint32_t result = 0;

#define SET_SUPPORTED_FIELD(field, value)	\
	result = 1;	\
	if (ppSupportedOutputFormat && *ppSupportedOutputFormat)	\
	{	\
		(*ppSupportedOutputFormat)->field = (value);	\
	}

	/* sample rate */
	if (pInputFormat->nSamplesPerSec != pRequestedOutputFormat->nSamplesPerSec)
	{
		SET_SUPPORTED_FIELD(nSamplesPerSec, pInputFormat->nSamplesPerSec);
	}

	/* data type */
	if (!IsFloatFormat(pRequestedOutputFormat))
	{
		SET_SUPPORTED_FIELD(wFormatTag, FAUDIO_FORMAT_IEEE_FLOAT);
	}

	/* number of input / output channels */
	if (pInputFormat->nChannels == 1 || pInputFormat->nChannels == 2)
	{
		if (	pRequestedOutputFormat->nChannels != pInputFormat->nChannels &&
			pRequestedOutputFormat->nChannels != 6)
		{
			SET_SUPPORTED_FIELD(nChannels, pInputFormat->nChannels);
		}
	}
	else
	{
		SET_SUPPORTED_FIELD(nChannels, 1);
	}

#undef SET_SUPPORTED_FIELD

	return result;
}

uint32_t FAudioFXReverb_Initialize(
	FAudioFXReverb *fapo,
	const void* pData,
	uint32_t DataByteSize
) {
	#define INITPARAMS(offset) \
		FAudio_memcpy( \
			fapo->base.m_pParameterBlocks + DataByteSize * offset, \
			pData, \
			DataByteSize \
		);
	INITPARAMS(0)
	INITPARAMS(1)
	INITPARAMS(2)
	#undef INITPARAMS
	return 0;
}

uint32_t FAudioFXReverb_LockForProcess(
	FAudioFXReverb *fapo,
	uint32_t InputLockedParameterCount,
	const FAPOLockForProcessBufferParameters *pInputLockedParameters,
	uint32_t OutputLockedParameterCount,
	const FAPOLockForProcessBufferParameters *pOutputLockedParameters
) {
	/* reverb specific validation */
	if (!IsFloatFormat(pInputLockedParameters->pFormat))
	{
		return FAPO_E_FORMAT_UNSUPPORTED;
	}

	if (	pInputLockedParameters->pFormat->nSamplesPerSec < FAUDIOFX_REVERB_MIN_FRAMERATE ||
		pInputLockedParameters->pFormat->nSamplesPerSec > FAUDIOFX_REVERB_MAX_FRAMERATE	)
	{
		return FAPO_E_FORMAT_UNSUPPORTED;
	}

	if (!(	(pInputLockedParameters->pFormat->nChannels == 1 &&
			(pOutputLockedParameters->pFormat->nChannels == 1 ||
			 pOutputLockedParameters->pFormat->nChannels == 6)) ||
		(pInputLockedParameters->pFormat->nChannels == 2 &&
			(pOutputLockedParameters->pFormat->nChannels == 2 ||
			 pOutputLockedParameters->pFormat->nChannels == 6))))
	{
		return FAPO_E_FORMAT_UNSUPPORTED;
	}

	/* save the things we care about */
	fapo->inChannels = pInputLockedParameters->pFormat->nChannels;
	fapo->outChannels = pOutputLockedParameters->pFormat->nChannels;
	fapo->sampleRate = pOutputLockedParameters->pFormat->nSamplesPerSec;
	fapo->inBlockAlign = pInputLockedParameters->pFormat->nBlockAlign;
	fapo->outBlockAlign = pOutputLockedParameters->pFormat->nBlockAlign;

	/* create the network if necessary */
	if (fapo->reverb == NULL)
	{
		fapo->reverb = DspReverb_Create(
			fapo->sampleRate,
			fapo->inChannels,
			fapo->outChannels,
			fapo->base.pMalloc
		);
	}

	/* call	parent to do basic validation */
	return FAPOBase_LockForProcess(
		&fapo->base,
		InputLockedParameterCount,
		pInputLockedParameters,
		OutputLockedParameterCount,
		pOutputLockedParameters
	);
}

static inline void FAudioFXReverb_CopyBuffer(
	FAudioFXReverb *fapo,
	float *restrict buffer_in,
	float *restrict buffer_out,
	size_t frames_in
) {
	/* In-place processing? */
	if (buffer_in == buffer_out)
	{
		return;
	}
	
	/* 1 -> 1 or 2 -> 2 */
	if (fapo->inBlockAlign == fapo->outBlockAlign)
	{
		FAudio_memcpy(
			buffer_out,
			buffer_in,
			fapo->inBlockAlign * frames_in
		);
		return;
	}

	/* 1 -> 5.1 */
	if (fapo->inChannels == 1 && fapo->outChannels == 6)
	{
		const float *in_end = buffer_in + frames_in;
		while (buffer_in < in_end)
		{
			*buffer_out++ = *buffer_in;
			*buffer_out++ = *buffer_in++;
			*buffer_out++ = 0.0f;
			*buffer_out++ = 0.0f;
			*buffer_out++ = 0.0f;
			*buffer_out++ = 0.0f;
		}
		return;
	}

	/* 2 -> 5.1 */
	if (fapo->inChannels == 2 && fapo->outChannels == 6)
	{
		const float *in_end = buffer_in + (frames_in * 2);
		while (buffer_in < in_end)
		{
			*buffer_out++ = *buffer_in++;
			*buffer_out++ = *buffer_in++;
			*buffer_out++ = 0.0f;
			*buffer_out++ = 0.0f;
			*buffer_out++ = 0.0f;
			*buffer_out++ = 0.0f;
		}
		return;
	}

	FAudio_assert(0 && "Unsupported channel combination");
	FAudio_zero(buffer_out, fapo->outBlockAlign * frames_in);
}

void FAudioFXReverb_Process(
	FAudioFXReverb *fapo,
	uint32_t InputProcessParameterCount,
	const FAPOProcessBufferParameters* pInputProcessParameters,
	uint32_t OutputProcessParameterCount,
	FAPOProcessBufferParameters* pOutputProcessParameters,
	int32_t IsEnabled
) {
	FAudioFXReverbParameters *params;
	uint8_t update_params = FAPOBase_ParametersChanged(&fapo->base);
	float total;
	
	/* handle disabled filter */
	if (IsEnabled == 0)
	{
		pOutputProcessParameters->BufferFlags = pInputProcessParameters->BufferFlags;

		if (pOutputProcessParameters->BufferFlags != FAPO_BUFFER_SILENT)
		{
			FAudioFXReverb_CopyBuffer(
				fapo,
				(float*) pInputProcessParameters->pBuffer,
				(float*) pOutputProcessParameters->pBuffer,
				pInputProcessParameters->ValidFrameCount
			);
		}

		return;
	}
	
	/* XAudio2 passes a 'silent' buffer when no input buffer is available to play the effect tail */
	if (pInputProcessParameters->BufferFlags == FAPO_BUFFER_SILENT)
	{
		/* make sure input data is usable */
		/* FIXME: Is this required? */
		FAudio_zero(
			pInputProcessParameters->pBuffer,
			pInputProcessParameters->ValidFrameCount * fapo->inBlockAlign
		);
	}

	params = (FAudioFXReverbParameters*) FAPOBase_BeginProcess(&fapo->base);

	/* update parameters  */
	if (update_params)
	{
		DspReverb_SetParameters(fapo->reverb, params);
	}

	/* run reverb effect */
	#define PROCESS(pin, pout) \
		DspReverb_INTERNAL_Process_##pin##_to_##pout( \
			fapo->reverb, \
			(float*) pInputProcessParameters->pBuffer, \
			(float*) pOutputProcessParameters->pBuffer, \
			pInputProcessParameters->ValidFrameCount * fapo->inChannels \
		)
	switch (fapo->reverb->out_channels)
	{
		case 1:
			total = PROCESS(1, 1);
			break;
		case 2:
			total = PROCESS(2, 2);
			break;
		default: /* 5.1 */
			if (fapo->reverb->in_channels == 1)
			{
				total = PROCESS(1, 5p1);
			}
			else
			{
				total = PROCESS(2, 5p1);
			}
			break;
	}
	#undef PROCESS

	/* set BufferFlags to silent so PLAY_TAILS knows when to stop */
	pOutputProcessParameters->BufferFlags = (total < 0.0000001f) ? FAPO_BUFFER_SILENT : FAPO_BUFFER_VALID;

	FAPOBase_EndProcess(&fapo->base);
}

void FAudioFXReverb_Reset(FAudioFXReverb *fapo)
{
	int32_t i, c;
	FAPOBase_Reset(&fapo->base);

	/* Reset the cached state of the reverb filter */
	DspDelay_Reset(&fapo->reverb->early_delay);

	for (i = 0; i < REVERB_COUNT_APF_IN; i += 1)
	{
		DspAllPass_Reset(&fapo->reverb->apf_in[i]);
	}

	for (c = 0; c < fapo->reverb->reverb_channels; c += 1)
	{
		DspDelay_Reset(&fapo->reverb->channel[c].reverb_delay);

		for (i = 0; i < REVERB_COUNT_COMB; i += 1)
		{
			DspCombShelving_Reset(&fapo->reverb->channel[c].lpf_comb[i]);
		}

		DspBiQuad_Reset(&fapo->reverb->channel[c].room_high_shelf);

		for (i = 0; i < REVERB_COUNT_APF_OUT; i += 1)
		{
			DspAllPass_Reset(&fapo->reverb->channel[c].apf_out[i]);
		}
	}
}

void FAudioFXReverb_Free(void* fapo)
{
	FAudioFXReverb *reverb = (FAudioFXReverb*) fapo;
	DspReverb_Destroy(reverb->reverb, reverb->base.pFree);
	reverb->base.pFree(reverb->base.m_pParameterBlocks);
	reverb->base.pFree(fapo);
}

/* Public API */

uint32_t FAudioCreateReverb(FAPO** ppApo, uint32_t Flags)
{
	return FAudioCreateReverbWithCustomAllocatorEXT(
		ppApo,
		Flags,
		FAudio_malloc,
		FAudio_free,
		FAudio_realloc
	);
}

uint32_t FAudioCreateReverbWithCustomAllocatorEXT(
	FAPO** ppApo,
	uint32_t Flags,
	FAudioMallocFunc customMalloc,
	FAudioFreeFunc customFree,
	FAudioReallocFunc customRealloc
) {
	const FAudioFXReverbParameters fxdefault =
	{
		FAUDIOFX_REVERB_DEFAULT_WET_DRY_MIX,
		FAUDIOFX_REVERB_DEFAULT_REFLECTIONS_DELAY,
		FAUDIOFX_REVERB_DEFAULT_REVERB_DELAY,
		FAUDIOFX_REVERB_DEFAULT_REAR_DELAY,
		FAUDIOFX_REVERB_DEFAULT_POSITION,
		FAUDIOFX_REVERB_DEFAULT_POSITION,
		FAUDIOFX_REVERB_DEFAULT_POSITION_MATRIX,
		FAUDIOFX_REVERB_DEFAULT_POSITION_MATRIX,
		FAUDIOFX_REVERB_DEFAULT_EARLY_DIFFUSION,
		FAUDIOFX_REVERB_DEFAULT_LATE_DIFFUSION,
		FAUDIOFX_REVERB_DEFAULT_LOW_EQ_GAIN,
		FAUDIOFX_REVERB_DEFAULT_LOW_EQ_CUTOFF,
		FAUDIOFX_REVERB_DEFAULT_HIGH_EQ_GAIN,
		FAUDIOFX_REVERB_DEFAULT_HIGH_EQ_CUTOFF,
		FAUDIOFX_REVERB_DEFAULT_ROOM_FILTER_FREQ,
		FAUDIOFX_REVERB_DEFAULT_ROOM_FILTER_MAIN,
		FAUDIOFX_REVERB_DEFAULT_ROOM_FILTER_HF,
		FAUDIOFX_REVERB_DEFAULT_REFLECTIONS_GAIN,
		FAUDIOFX_REVERB_DEFAULT_REVERB_GAIN,
		FAUDIOFX_REVERB_DEFAULT_DECAY_TIME,
		FAUDIOFX_REVERB_DEFAULT_DENSITY,
		FAUDIOFX_REVERB_DEFAULT_ROOM_SIZE
	};

	/* Allocate... */
	FAudioFXReverb *result = (FAudioFXReverb*) customMalloc(sizeof(FAudioFXReverb));
	uint8_t *params = (uint8_t*) customMalloc(
		sizeof(FAudioFXReverbParameters) * 3
	);
	#define INITPARAMS(offset) \
		FAudio_memcpy( \
			params + sizeof(FAudioFXReverbParameters) * offset, \
			&fxdefault, \
			sizeof(FAudioFXReverbParameters) \
		);
	INITPARAMS(0)
	INITPARAMS(1)
	INITPARAMS(2)
	#undef INITPARAMS

	/* Initialize... */
	FAudio_memcpy(
		&ReverbProperties.clsid,
		&FAudioFX_CLSID_AudioReverb,
		sizeof(FAudioGUID)
	);
	CreateFAPOBaseWithCustomAllocatorEXT(
		&result->base,
		&ReverbProperties,
		params,
		sizeof(FAudioFXReverbParameters),
		0,
		customMalloc,
		customFree,
		customRealloc
	);

	result->inChannels = 0;
	result->outChannels = 0;
	result->sampleRate = 0;
	result->reverb = NULL;

	/* Function table... */
	#define ASSIGN_VT(name) \
		result->base.base.name = (name##Func) FAudioFXReverb_##name;
	ASSIGN_VT(LockForProcess);
	ASSIGN_VT(IsInputFormatSupported);
	ASSIGN_VT(IsOutputFormatSupported);
	ASSIGN_VT(Initialize);
	ASSIGN_VT(Reset);
	ASSIGN_VT(Process);
	result->base.Destructor = FAudioFXReverb_Free;
	#undef ASSIGN_VT

	/* Finally. */
	*ppApo = &result->base.base;
	return 0;
}

void ReverbConvertI3DL2ToNative(
	const FAudioFXReverbI3DL2Parameters *pI3DL2,
	FAudioFXReverbParameters *pNative
) {
	float reflectionsDelay;
	float reverbDelay;

	pNative->RearDelay = FAUDIOFX_REVERB_DEFAULT_REAR_DELAY;
	pNative->PositionLeft = FAUDIOFX_REVERB_DEFAULT_POSITION;
	pNative->PositionRight = FAUDIOFX_REVERB_DEFAULT_POSITION;
	pNative->PositionMatrixLeft = FAUDIOFX_REVERB_DEFAULT_POSITION_MATRIX;
	pNative->PositionMatrixRight = FAUDIOFX_REVERB_DEFAULT_POSITION_MATRIX;
	pNative->RoomSize = FAUDIOFX_REVERB_DEFAULT_ROOM_SIZE;
	pNative->LowEQCutoff = 4;
	pNative->HighEQCutoff = 6;

	pNative->RoomFilterMain = (float) pI3DL2->Room / 100.0f;
	pNative->RoomFilterHF = (float) pI3DL2->RoomHF / 100.0f;

	if (pI3DL2->DecayHFRatio >= 1.0f)
	{
		int32_t index = (int32_t) (-4.0 * FAudio_log10(pI3DL2->DecayHFRatio));
		if (index < -8)
		{
			index = -8;
		}
		pNative->LowEQGain = (uint8_t) ((index < 0) ? index + 8 : 8);
		pNative->HighEQGain = 8;
		pNative->DecayTime = pI3DL2->DecayTime * pI3DL2->DecayHFRatio;
	}
	else
	{
		int32_t index = (int32_t) (4.0 * FAudio_log10(pI3DL2->DecayHFRatio));
		if (index < -8)
		{
			index = -8;
		}
		pNative->LowEQGain = 8;
		pNative->HighEQGain = (uint8_t) ((index < 0) ? index + 8 : 8);
		pNative->DecayTime = pI3DL2->DecayTime;
	}

	reflectionsDelay = pI3DL2->ReflectionsDelay * 1000.0f;
	if (reflectionsDelay >= FAUDIOFX_REVERB_MAX_REFLECTIONS_DELAY)
	{
		reflectionsDelay = (float) (FAUDIOFX_REVERB_MAX_REFLECTIONS_DELAY - 1);
	}
	else if (reflectionsDelay <= 1)
	{
		reflectionsDelay = 1;
	}
	pNative->ReflectionsDelay = (uint32_t) reflectionsDelay;

	reverbDelay = pI3DL2->ReverbDelay * 1000.0f;
	if (reverbDelay >= FAUDIOFX_REVERB_MAX_REVERB_DELAY)
	{
		reverbDelay = (float) (FAUDIOFX_REVERB_MAX_REVERB_DELAY - 1);
	}
	pNative->ReverbDelay = (uint8_t) reverbDelay;

	pNative->ReflectionsGain = pI3DL2->Reflections / 100.0f;
	pNative->ReverbGain = pI3DL2->Reverb / 100.0f;
	pNative->EarlyDiffusion = (uint8_t) (15.0f * pI3DL2->Diffusion / 100.0f);
	pNative->LateDiffusion = pNative->EarlyDiffusion;
	pNative->Density = pI3DL2->Density;
	pNative->RoomFilterFreq = pI3DL2->HFReference;

	pNative->WetDryMix = pI3DL2->WetDryMix;
}

/* vim: set noexpandtab shiftwidth=8 tabstop=8: */
