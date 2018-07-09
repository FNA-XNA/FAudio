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
#include "FAudioFX.h"
#include "FAudio_internal.h"

/* constants */
#define PI 3.1415926536f
#define DSP_DELAY_MAX_DELAY_MS 300

/* utility functions */
static inline float FAudioFX_INTERNAL_DbGainToFactor(float gain)
{
	return (float)FAudio_pow(10, gain / 20.0f);
}

static inline uint32_t FAudioFX_INTERNAL_MsToSamples(float msec, int32_t sampleRate)
{
	return (uint32_t)((sampleRate * msec) / 1000.0f);
}

static inline float FAudioFX_INTERNAL_undenormalize(float sample_in)
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

/* component - delay */
typedef struct DspDelay
{
	int32_t	 sampleRate;
	uint32_t capacity;		/* in samples */
	uint32_t delay;			/* in samples */
	uint32_t read_idx;
	uint32_t write_idx;
	float *buffer;
} DspDelay;

static void DspDelay_Initialize(DspDelay *filter, int32_t sampleRate, float delay_ms)
{
	FAudio_assert(delay_ms >= 0 && delay_ms <= DSP_DELAY_MAX_DELAY_MS);
	FAudio_assert(filter != NULL);

	filter->sampleRate = sampleRate;
	filter->capacity = FAudioFX_INTERNAL_MsToSamples(DSP_DELAY_MAX_DELAY_MS, sampleRate);
	filter->delay = FAudioFX_INTERNAL_MsToSamples(delay_ms, sampleRate);
	filter->read_idx = 0;
	filter->write_idx = filter->delay;
	filter->buffer = (float *)FAudio_malloc(filter->capacity * sizeof(float));
	FAudio_zero(filter->buffer, filter->capacity * sizeof(float));
}

static void DspDelay_Change(DspDelay *filter, float delay_ms)
{
	FAudio_assert(filter != NULL);
	FAudio_assert(delay_ms >= 0 && delay_ms <= DSP_DELAY_MAX_DELAY_MS);

	/* length */
	filter->delay = FAudioFX_INTERNAL_MsToSamples(delay_ms, filter->sampleRate);
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

static void DspDelay_Destroy(DspDelay *filter)
{
	FAudio_assert(filter != NULL);
	FAudio_free(filter->buffer);
}

/* component - comb filter */
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

	exponent = (-3.0f * filter->delay.delay * 1000.0f) / (filter->delay.sampleRate * rt60_ms);
	return (float)FAudio_pow(10.0f, exponent);
}

static void DspComb_Initialize(DspComb *filter, int32_t sampleRate, float delay_ms, float rt60_ms)
{
	FAudio_assert(filter != NULL);

	DspDelay_Initialize(&filter->delay, sampleRate, delay_ms);
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

	to_buf = FAudioFX_INTERNAL_undenormalize(sample_in + (filter->feedback_gain * delay_out));
	DspDelay_Write(&filter->delay, to_buf);

	return delay_out;
}

static inline void DspComb_Reset(DspComb *filter)
{
	DspDelay_Reset(&filter->delay);
}

static void DspComb_Destroy(DspComb *filter)
{
	FAudio_assert(filter != NULL);
	DspDelay_Destroy(&filter->delay);
}

/* component - bi-quad filter */
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

static void DspBiQuad_Change(DspBiQuad *filter, float frequency, float q, float gain);

static void DspBiQuad_Initialize(
	DspBiQuad *filter,
	int32_t sampleRate,
	DspBiQuadType type,
	float frequency,		/* corner frequency */
	float q,				/* only used by low/high-pass filters */
	float gain				/* only used by low/high-shelving filters */
) {
	FAudio_assert(filter != NULL);

	FAudio_zero(filter, sizeof(DspBiQuad));
	filter->type = type;
	filter->sampleRate = sampleRate;
	DspBiQuad_Change(filter, frequency, q, gain);
}

static void DspBiQuad_Change(DspBiQuad *filter, float frequency, float q, float gain)
{
	float theta_c;

	FAudio_assert(filter != NULL);
	theta_c = (2.0f * PI * frequency) / (float)filter->sampleRate;

	if (filter->type == DSP_BIQUAD_LOWPASS || filter->type == DSP_BIQUAD_HIGHPASS)
	{
		float sin_theta_c = (float)FAudio_sin(theta_c);
		float cos_theta_c = (float)FAudio_cos(theta_c);
		float oneOverQ = 1.0f / q;
		float beta = (1.0f - (oneOverQ * 0.5f * sin_theta_c)) / 
					 (1.0f + (oneOverQ *  0.5f * sin_theta_c));
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
	else if (filter->type == DSP_BIQUAD_LOWSHELVING || filter->type == DSP_BIQUAD_HIGHSHELVING)
	{
		float mu = FAudioFX_INTERNAL_DbGainToFactor(gain);
		float beta = (filter->type == DSP_BIQUAD_LOWSHELVING) ? 4.0f / (1 + mu) : (1 + mu) / 4.0f;
		float delta = beta * (float)FAudio_tan(theta_c * 0.5f);
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

static inline float DspBiQuad_Process(DspBiQuad *filter, float sample_in)
{
	/* Direct Form II Transposed:
		- less delay registers than Direct Form I
		- more numerically stable than Direct Form II */
	float result = (filter->a0 * sample_in) + filter->delay[0];
	filter->delay[0] = (filter->a1 * sample_in) - (filter->b1 * result) + filter->delay[1];
	filter->delay[1] = (filter->a2 * sample_in) - (filter->b2 * result);

	result = FAudioFX_INTERNAL_undenormalize(
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

/* component - comb filter with integrated low shelving and high shelving filter */
typedef struct DspCombShelving {
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
	float high_gain
) {
	FAudio_assert(filter != NULL);
	DspComb_Initialize(&filter->comb, sampleRate, delay_ms, rt60_ms);
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

static inline float DspCombShelving_Process(DspCombShelving *filter, float sample_in)
{
	float delay_out, feedback, to_buf;

	FAudio_assert(filter != NULL);

	delay_out = DspDelay_Read(&filter->comb.delay);

	/* apply shelving filters */
	feedback = DspBiQuad_Process(&filter->high_shelving, delay_out);
	feedback = DspBiQuad_Process(&filter->low_shelving, feedback);

	/* apply comb filter */
	to_buf = FAudioFX_INTERNAL_undenormalize(sample_in + (filter->comb.feedback_gain * feedback));
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

static void DspCombShelving_Destroy(DspCombShelving *filter)
{
	FAudio_assert(filter != NULL);

	DspComb_Destroy(&filter->comb);
	DspBiQuad_Destroy(&filter->low_shelving);
	DspBiQuad_Destroy(&filter->high_shelving);
}

/* component: delaying all-pass filter */
typedef struct DspAllPass
{
	DspDelay delay;
	float feedback_gain;
} DspAllPass;

static void DspAllPass_Initialize(DspAllPass *filter, int32_t sampleRate, float delay_ms, float gain)
{
	FAudio_assert(filter != NULL);

	DspDelay_Initialize(&filter->delay, sampleRate, delay_ms);
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

	to_buf = FAudioFX_INTERNAL_undenormalize(sample_in + (filter->feedback_gain * delay_out));
	DspDelay_Write(&filter->delay, to_buf);

	return FAudioFX_INTERNAL_undenormalize(delay_out - (filter->feedback_gain * to_buf));
}

static void DspAllPass_Reset(DspAllPass *filter)
{
	FAudio_assert(filter != NULL);
	DspDelay_Reset(&filter->delay);
}

static void DspAllPass_Destroy(DspAllPass *filter)
{
	FAudio_assert(filter != NULL);
	DspDelay_Destroy(&filter->delay);
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

float WetDryMix;				0 - 100 (0 = fully dry, 100 = fully wet) 
uint32_t ReflectionsDelay;		0 - 300 ms 
uint8_t ReverbDelay;			0 - 85 ms 
uint8_t RearDelay;				0 - 5 ms
uint8_t PositionLeft;			0 - 30 
uint8_t PositionRight;			0 - 30 
uint8_t PositionMatrixLeft;		0 - 30 
uint8_t PositionMatrixRight;	0 - 30 
uint8_t EarlyDiffusion;			0 - 15 
uint8_t LateDiffusion;			0 - 15 
uint8_t LowEQGain;				0 - 12 (formula dB = LowEQGain - 8) 
uint8_t LowEQCutoff;			0 - 9  (formula Hz = 50 + (LowEQCutoff * 50)) 
uint8_t HighEQGain;				0 - 8  (formula dB = HighEQGain - 8)
uint8_t HighEQCutoff;			0 - 14 (formula Hz = 1000 + (HighEqCutoff * 500))
float RoomFilterFreq;			20 - 20000Hz 
float RoomFilterMain;			-100 - 0dB 
float RoomFilterHF;				-100 - 0dB 
float ReflectionsGain;			-100 - 20dB 
float ReverbGain;				-100 - 20dB 
float DecayTime;				0.1 - .... ms 
float Density;					0 - 100 %
float RoomSize;					1 - 100 feet (NOT USED YET) 

*/

#define REVERB_COUNT_COMB 8
#define REVERB_COUNT_APF_IN	1
#define REVERB_COUNT_APF_OUT 4

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

DspReverb *DspReverb_Create(int32_t sampleRate, int32_t in_channels, int32_t out_channels)
{
	DspReverb *reverb;
	int32_t i, c;

	FAudio_assert(in_channels == 1 || in_channels == 2);
	FAudio_assert(out_channels == 1 || out_channels == 2 || out_channels == 6);

	reverb = (DspReverb *)FAudio_malloc(sizeof(DspReverb));
	FAudio_zero(reverb, sizeof(DspReverb));
	DspDelay_Initialize(&reverb->early_delay, sampleRate, 10);

	for (i = 0; i < REVERB_COUNT_APF_IN; ++i)
	{
		DspAllPass_Initialize(&reverb->apf_in[i], sampleRate, APF_IN_DELAYS[i], 0.5f);
	}

	reverb->reverb_channels = (out_channels == 6) ? 4 : out_channels;

	for (c = 0; c < reverb->reverb_channels; ++c)
	{
		DspDelay_Initialize(&reverb->channel[c].reverb_delay, sampleRate, 10);

		for (i = 0; i < REVERB_COUNT_COMB; ++i)
		{
			DspCombShelving_Initialize(
				&reverb->channel[c].lpf_comb[i], 
				sampleRate, 
				COMB_DELAYS[i] + STEREO_SPREAD[c], 
				500, 
				500, 
				-6, 
				5000, 
				-6
			);
		}

		for (i = 0; i < REVERB_COUNT_APF_OUT; ++i)
		{
			DspAllPass_Initialize(
				&reverb->channel[c].apf_out[i], 
				sampleRate, 
				APF_OUT_DELAYS[i] + STEREO_SPREAD[c], 
				0.5f
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

void DspReverb_SetParameters(DspReverb *reverb, FAudioFXReverbParameters *params)
{
	float early_diffusion, late_diffusion;
	float channel_delay[4] = { 0.0f, 0.0f, params->RearDelay, params->RearDelay };
	int32_t i, c;

	/* pre delay */
	DspDelay_Change(&reverb->early_delay, (float)params->ReflectionsDelay);

	/* early reflections - diffusion */
	early_diffusion = 0.6f - ((params->EarlyDiffusion / 15.0f) * 0.2f);

	for (i = 0; i < REVERB_COUNT_APF_IN; ++i)
	{
		DspAllPass_Change(&reverb->apf_in[i], APF_IN_DELAYS[i], early_diffusion);
	}

	/* reverberation */
	for (c = 0; c < reverb->reverb_channels; ++c)
	{
		DspDelay_Change(&reverb->channel[c].reverb_delay, (float) params->ReverbDelay + channel_delay[c]);

		for (i = 0; i < REVERB_COUNT_COMB; ++i)
		{
			/* set decay time of comb filter */
			DspComb_Change(
				&reverb->channel[c].lpf_comb[i].comb, 
				COMB_DELAYS[i] + STEREO_SPREAD[c], 
				params->DecayTime * 1000.0f);

			/* high/low shelving */
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

	/* gain */
	reverb->early_gain = FAudioFX_INTERNAL_DbGainToFactor(params->ReflectionsGain);
	reverb->reverb_gain = FAudioFX_INTERNAL_DbGainToFactor(params->ReverbGain);
	reverb->room_gain = FAudioFX_INTERNAL_DbGainToFactor(params->RoomFilterMain);

	/* late diffusion */
	late_diffusion = 0.6f - ((params->LateDiffusion / 15.0f) * 0.2f);

	for (c = 0; c < reverb->reverb_channels; ++c)
	{
		for (i = 0; i < REVERB_COUNT_APF_OUT; ++i)
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

		reverb->channel[c].gain = 1.5f - (((c % 2 == 0 ? params->PositionMatrixLeft : params->PositionMatrixRight) / 27.0f) * 0.5f);
		if (c >= 2)
		{
			/* rear-channel attenuation */
			reverb->channel[c].gain *= 0.75f;
		}

		reverb->channel[c].early_gain = 1.2f - (((c % 2 == 0 ? params->PositionLeft : params->PositionRight) / 6.0f) * 0.2f);
		reverb->channel[c].early_gain = reverb->channel[c].early_gain * reverb->early_gain;
	}

	/* wet/dry mix (100 = fully wet / 0 = fully dry) */
	reverb->wet_ratio = params->WetDryMix / 100.0f;
	reverb->dry_ratio = 1.0f - reverb->wet_ratio;
}

static inline float DspReverb_INTERNAL_ProcessEarly(DspReverb *reverb, float sample_in)
{
	float delay_in, early;
	int32_t i;

	/* pre delay */
	delay_in = DspDelay_Process(&reverb->early_delay, sample_in);

	/* early reflections */
	early = delay_in;

	for (i = 0; i < REVERB_COUNT_APF_IN; ++i)
	{
		early = DspAllPass_Process(&reverb->apf_in[i], early);
	}

	return early;
}

static inline float DspReverb_INTERNAL_ProcessChannel(DspReverb *reverb, DspReverbChannel *channel, float sample_in)
{
	float revdelay, comb_out, comb_gain;
	float late, early_late, out;
	int32_t i;

	revdelay = DspDelay_Process(&channel->reverb_delay, sample_in);

	comb_out = 0.0f;
	comb_gain = 1.0f / REVERB_COUNT_COMB;

	for (i = 0; i < REVERB_COUNT_COMB; ++i)
	{
		comb_out += comb_gain * DspCombShelving_Process(&channel->lpf_comb[i], revdelay);
	}

	/* output diffusion */
	late = comb_out;
	for (i = 0; i < REVERB_COUNT_APF_OUT; ++i)
	{
		late = DspAllPass_Process(&channel->apf_out[i], late);
	}

	/* combine early reflections and reverberation */
	early_late = (channel->early_gain * sample_in) + (reverb->reverb_gain * late);

	/* room filter */
	out = early_late * reverb->room_gain;
	out = DspBiQuad_Process(&channel->room_high_shelf, out);

	/* PositionMatrixLeft/Rigth */
	out = out * channel->gain;

	return out;
}

#define OUTPUT_SAMPLE(x)	\
	*out_ptr = (x);	\
	squared_sum += *out_ptr * *out_ptr; \
	out_ptr += 1;

static inline float DspReverb_INTERNAL_Process_1_to_1(DspReverb *reverb, const float *samples_in, float *samples_out, size_t sample_count)
{
	float *out_ptr = samples_out;
	const float *in_ptr = samples_in;
	const float *in_end = samples_in + sample_count;
	float squared_sum = 0;

	while (in_ptr < in_end)
	{
		float early, late, out;

		/* input */
		float in = *in_ptr++;

		/* early reflections */
		early = DspReverb_INTERNAL_ProcessEarly(reverb, in);

		/* reverberation */
		late = DspReverb_INTERNAL_ProcessChannel(reverb, &reverb->channel[0], early);

		/* wet/dry mix */
		out = (late * reverb->wet_ratio) + (in * reverb->dry_ratio);

		/* output */
		OUTPUT_SAMPLE(out);
	}

	return squared_sum;
}

static inline float DspReverb_INTERNAL_Process_1_to_5p1(DspReverb *reverb, const float *samples_in, float *samples_out, size_t sample_count)
{
	float *out_ptr = samples_out;
	const float *in_ptr = samples_in;
	const float *in_end = samples_in + sample_count;
	float squared_sum = 0;
	int32_t c;

	while (in_ptr < in_end)
	{
		float early, late[4];

		/* input */
		float in = *in_ptr++;

		/* early reflections */
		early = DspReverb_INTERNAL_ProcessEarly(reverb, in);

		/* reverberation */
		for (c = 0; c < 4; ++c)
		{
			late[c] = DspReverb_INTERNAL_ProcessChannel(reverb, &reverb->channel[c], early);
		}

		/* wet/dry mix -> output */
		OUTPUT_SAMPLE((late[0] * reverb->wet_ratio) + (in * reverb->dry_ratio));		/* front-left */
		OUTPUT_SAMPLE((late[1] * reverb->wet_ratio) + (in * reverb->dry_ratio));		/* front-right */
		OUTPUT_SAMPLE(0.0f);															/* center */
		OUTPUT_SAMPLE(0.0f);															/* lfe */
		OUTPUT_SAMPLE((late[2] * reverb->wet_ratio) + (in * reverb->dry_ratio));		/* rear-left */
		OUTPUT_SAMPLE((late[3] * reverb->wet_ratio) + (in * reverb->dry_ratio));		/* rear-right */
	}

	return squared_sum;
}

static inline float DspReverb_INTERNAL_Process_2_to_2(DspReverb *reverb, const float *samples_in, float *samples_out, size_t sample_count)
{
	float *out_ptr = samples_out;
	const float *in_ptr = samples_in;
	const float *in_end = samples_in + sample_count;
	float squared_sum = 0;

	while (in_ptr < in_end)
	{
		float early, late[2];

		/* input - combine 2 channel in 1 */
		float in = *in_ptr++;
		in = 0.5f * (in + *in_ptr++);

		/* early reflections */
		early = DspReverb_INTERNAL_ProcessEarly(reverb, in);

		/* reverberation */
		late[0] = DspReverb_INTERNAL_ProcessChannel(reverb, &reverb->channel[0], early);
		late[1] = DspReverb_INTERNAL_ProcessChannel(reverb, &reverb->channel[1], early);

		/* wet/dry mix -> output */
		OUTPUT_SAMPLE((late[0] * reverb->wet_ratio) + (in * reverb->dry_ratio));
		OUTPUT_SAMPLE((late[1] * reverb->wet_ratio) + (in * reverb->dry_ratio));
	}

	return squared_sum;
}

static inline float DspReverb_INTERNAL_Process_2_to_5p1(DspReverb *reverb, const float *samples_in, float *samples_out, size_t sample_count)
{
	float *out_ptr = samples_out;
	const float *in_ptr = samples_in;
	const float *in_end = samples_in + sample_count;
	float squared_sum = 0;
	int32_t c;

	while (in_ptr < in_end)
	{
		float early, late[4];

		/* input - combine 2 channel in 1 */
		float in = *in_ptr++;
		in = 0.5f * (in + *in_ptr++);

		/* early reflections */
		early = DspReverb_INTERNAL_ProcessEarly(reverb, in);

		/* reverberation */
		for (c = 0; c < 4; ++c)
		{
			late[c] = DspReverb_INTERNAL_ProcessChannel(reverb, &reverb->channel[c], early);
		}

		/* wet/dry mix -> output */
		OUTPUT_SAMPLE((late[0] * reverb->wet_ratio) + (in * reverb->dry_ratio));		/* front-left */
		OUTPUT_SAMPLE((late[1] * reverb->wet_ratio) + (in * reverb->dry_ratio));		/* front-right */
		OUTPUT_SAMPLE(0.0f);															/* center */
		OUTPUT_SAMPLE(0.0f);															/* lfe */
		OUTPUT_SAMPLE((late[2] * reverb->wet_ratio) + (in * reverb->dry_ratio));		/* rear-left */
		OUTPUT_SAMPLE((late[3] * reverb->wet_ratio) + (in * reverb->dry_ratio));		/* rear-right */
	}

	return squared_sum;
}

#undef OUTPUT_SAMPLE

float DspReverb_Process(
	DspReverb *reverb, 
	const float *samples_in, 
	float *samples_out, 
	size_t sample_count, 
	int32_t num_channels
) {
	FAudio_assert(reverb != NULL);
	FAudio_assert(samples_in != NULL);
	FAudio_assert(samples_out != NULL);

	switch (reverb->out_channels)
	{
		case 1:
			return DspReverb_INTERNAL_Process_1_to_1(reverb, samples_in, samples_out, sample_count);
		case 2:
			return DspReverb_INTERNAL_Process_2_to_2(reverb, samples_in, samples_out, sample_count);
		default:	/* 5.1 */
			if (reverb->in_channels == 1)
			{
				return DspReverb_INTERNAL_Process_1_to_5p1(reverb, samples_in, samples_out, sample_count);
			}
			else
			{
				return DspReverb_INTERNAL_Process_2_to_5p1(reverb, samples_in, samples_out, sample_count);
			}
	}
}

void DspReverb_Reset(DspReverb *reverb)
{
	int32_t i, c;

	DspDelay_Reset(&reverb->early_delay);

	for (i = 0; i < REVERB_COUNT_APF_IN; ++i)
	{
		DspAllPass_Reset(&reverb->apf_in[i]);
	}

	for (c = 0; c < reverb->reverb_channels; ++c)
	{
		DspDelay_Reset(&reverb->channel[c].reverb_delay);

		for (i = 0; i < REVERB_COUNT_COMB; ++i)
		{
			DspCombShelving_Reset(&reverb->channel[c].lpf_comb[i]);
		}

		DspBiQuad_Reset(&reverb->channel[c].room_high_shelf);

		for (i = 0; i < REVERB_COUNT_APF_OUT; ++i)
		{
			DspAllPass_Reset(&reverb->channel[c].apf_out[i]);
		}
	}
}

void DspReverb_Destroy(DspReverb *reverb)
{
	int32_t i, c;

	DspDelay_Destroy(&reverb->early_delay);

	for (i = 0; i < REVERB_COUNT_APF_IN; ++i)
	{
		DspAllPass_Destroy(&reverb->apf_in[i]);
	}

	for (c = 0; c < reverb->reverb_channels; ++c)
	{
		DspDelay_Destroy(&reverb->channel[c].reverb_delay);

		for (i = 0; i < REVERB_COUNT_COMB; ++i)
		{
			DspCombShelving_Destroy(&reverb->channel[c].lpf_comb[i]);
		}

		DspBiQuad_Destroy(&reverb->channel[c].room_high_shelf);

		for (i = 0; i < REVERB_COUNT_APF_OUT; ++i)
		{
			DspAllPass_Destroy(&reverb->channel[c].apf_out[i]);
		}
	}

	FAudio_free(reverb);
}
