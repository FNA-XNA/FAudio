/* FACT - XACT Reimplementation for FNA
 * Copyright 2009-2017 Ethan Lee and the MonoGame Team
 *
 * Released under the Microsoft Public License.
 * See LICENSE for details.
 */

#include "FACT_internal.h"

/* Various internal math functions */

FACTRPC* FACT_INTERNAL_GetRPC(
	FACTAudioEngine *engine,
	uint32_t code
) {
	uint16_t i;
	for (i = 0; i < engine->rpcCount; i += 1)
	{
		if (engine->rpcCodes[i] == code)
		{
			return &engine->rpcs[i];
		}
	}

	assert(0 && "RPC code not found!");
	return NULL;
}

float FACT_INTERNAL_CalculateRPC(
	FACTRPC *rpc,
	float var
) {
	float result;
	uint8_t i;

	/* Min/Max */
	if (var <= rpc->points[0].x)
	{
		/* Zero to first defined point */
		return rpc->points[0].y;
	}
	if (var >= rpc->points[rpc->pointCount - 1].x)
	{
		/* Last defined point to infinity */
		return rpc->points[rpc->pointCount - 1].y;
	}

	/* Something between points... TODO: Non-linear curves */
	result = 0.0f;
	for (i = 0; i < rpc->pointCount - 1; i += 1)
	{
		/* y = b */
		result = rpc->points[i].y;
		if (var >= rpc->points[i].x && var <= rpc->points[i + 1].x)
		{
			/* y += mx */
			result +=
				((rpc->points[i + 1].y - rpc->points[i].y) /
				(rpc->points[i + 1].x - rpc->points[i].x)) *
					(var - rpc->points[i].x);

			/* Pre-algebra, rockin'! */
			break;
		}
	}
	return result;
}

void FACT_INTERNAL_UpdateRPCs(
	FACTCue *cue,
	uint8_t codeCount,
	uint32_t *codes,
	FACTInstanceRPCData *data
) {
	uint8_t i;
	FACTRPC *rpc;
	float rpcResult;
	FACTAudioEngine *engine = cue->parentBank->parentEngine;

	if (codeCount > 0)
	{
		data->rpcVolume = 0.0f;
		data->rpcPitch = 0.0f;
		data->rpcFilterFreq = 0.0f; /* FIXME: Starting value? */
		for (i = 0; i < codeCount; i += 1)
		{
			rpc = FACT_INTERNAL_GetRPC(
				engine,
				codes[i]
			);
			if (engine->variables[rpc->variable].accessibility & 0x04)
			{
				rpcResult = FACT_INTERNAL_CalculateRPC(
					rpc,
					engine->globalVariableValues[rpc->variable]
				);
			}
			else
			{
				if (FACT_strcmp(
					engine->variableNames[rpc->variable],
					"AttackTime"
				) == 0) {
					/* TODO: AttackTime */
				}
				else if (FACT_strcmp(
					engine->variableNames[rpc->variable],
					"ReleaseTime"
				) == 0) {
					/* TODO: ReleaseTime */
				}
				else
				{
					rpcResult = FACT_INTERNAL_CalculateRPC(
						rpc,
						cue->variableValues[rpc->variable]
					);
				}
			}
			if (rpc->parameter == RPC_PARAMETER_VOLUME)
			{
				data->rpcVolume += rpcResult;
			}
			else if (rpc->parameter == RPC_PARAMETER_PITCH)
			{
				data->rpcPitch += rpcResult;
			}
			else if (rpc->parameter == RPC_PARAMETER_FILTERFREQUENCY)
			{
				data->rpcFilterFreq += rpcResult;
			}
			else
			{
				assert(0 && "Unhandled RPC parameter type!");
			}
		}
	}
}

void FACT_INTERNAL_SetDSPParameter(
	FACTDSPPreset *dsp,
	FACTRPC *rpc,
	float var
) {
	uint16_t par = rpc->parameter - RPC_PARAMETER_COUNT;
	dsp->parameters[par].value = FACT_clamp(
		FACT_INTERNAL_CalculateRPC(rpc, var),
		dsp->parameters[par].minVal,
		dsp->parameters[par].maxVal
	);
}

/* The functions below should be called by the platform mixer! */

void FACT_INTERNAL_UpdateEngine(FACTAudioEngine *engine)
{
	uint16_t i, j;
	for (i = 0; i < engine->rpcCount; i += 1)
	{
		if (engine->rpcs[i].parameter >= RPC_PARAMETER_COUNT)
		{
			/* FIXME: Why did I make this global vars only...? */
			if (engine->variables[engine->rpcs[i].variable].accessibility & 0x04)
			{
				for (j = 0; j < engine->dspPresetCount; j += 1)
				{
					/* FIXME: This affects all DSP presets!
					 * What if there's more than one?
					 */
					FACT_INTERNAL_SetDSPParameter(
						&engine->dspPresets[j],
						&engine->rpcs[i],
						engine->globalVariableValues[engine->rpcs[i].variable]
					);
				}
			}
		}
	}
}

uint8_t FACT_INTERNAL_UpdateCue(FACTCue *cue)
{
	uint8_t i, j;

	/* If we're not running, save some instructions... */
	if (cue->state & FACT_STATE_PAUSED)
	{
		return 0;
	}

	/* FIXME: I think this will always be true except for first play? */
	if (!cue->soundInstance.exists)
	{
		return 0;
	}

	/* TODO: Interactive Cues */

	/* Trigger events for each track */
	for (i = 0; i < cue->soundInstance.sound->clipCount; i += 1)
	for (j = 0; i < cue->soundInstance.sound->clips[i].eventCount; j += 1)
	if (	!cue->soundInstance.clips[i].eventFinished[j] &&
		1 /* TODO: timer > cue->soundInstance.clips[i].eventTimestamp */	)
	{
		/* Activate the event */
		switch (cue->soundInstance.sound->clips[i].events[j].type)
		{
		case FACTEVENT_STOP:
			/* TODO: FACT_INTERNAL_Stop(Stop*) */
			break;
		case FACTEVENT_PLAYWAVE:
		case FACTEVENT_PLAYWAVETRACKVARIATION:
		case FACTEVENT_PLAYWAVEEFFECTVARIATION:
		case FACTEVENT_PLAYWAVETRACKEFFECTVARIATION:
			/* TODO: FACT_INTERNAL_Play(PlayWave*) */
			break;
		case FACTEVENT_PITCH:
		case FACTEVENT_PITCHREPEATING:
			/* TODO: FACT_INTERNAL_SetPitch(SetValue*) */
			break;
		case FACTEVENT_VOLUME:
		case FACTEVENT_VOLUMEREPEATING:
			/* TODO: FACT_INTERNAL_SetVolume(SetValue*) */
			break;
		case FACTEVENT_MARKER:
		case FACTEVENT_MARKERREPEATING:
			/* TODO: FACT_INTERNAL_Marker(Marker*) */
			break;
		default:
			assert(0 && "Unrecognized clip event type!");
		}

		/* Either loop or mark this event as complete */
		if (cue->soundInstance.clips[i].eventLoopsLeft[j] > 0)
		{
			if (cue->soundInstance.sound->clips[i].events[j].loopCount != 0xFF)
			{
				cue->soundInstance.clips[i].eventLoopsLeft[j] -= 1;
			}

			/* TODO: Push timestamp forward for "looping" */
		}
		else
		{
			cue->soundInstance.clips[i].eventFinished[j] = 1;
		}
	}

	/* TODO: Clear out Waves as they finish */

	/* TODO: Fade in/out */

	/* TODO: If everything has been played and finished, set STOPPED */

	/* RPC updates */
	FACT_INTERNAL_UpdateRPCs(
		cue,
		cue->soundInstance.sound->rpcCodeCount,
		cue->soundInstance.sound->rpcCodes,
		&cue->soundInstance.rpcData
	);
	for (i = 0; i < cue->soundInstance.sound->clipCount; i += 1)
	{
		FACT_INTERNAL_UpdateRPCs(
			cue,
			cue->soundInstance.sound->clips[i].rpcCodeCount,
			cue->soundInstance.sound->clips[i].rpcCodes,
			&cue->soundInstance.clips[i].rpcData
		);
	}

	/* TODO: Wave updates:
	 * - Volume
	 * - Pitch
	 * - Filter
	 * - Reverb
	 * - 3D
	 */

	/* Finally. */
	return 0;
}

void FACT_INTERNAL_MixWave(FACTWave *wave, uint8_t *stream, uint32_t len)
{
	/* TODO */
}

#define DECODE_FUNC(type, depth) \
	uint32_t FACT_INTERNAL_Decode##type( \
		FACTWave *wave, \
		uint16_t *decodeCache, \
		uint32_t samples \
	) { \
		/* Don't go past the end of the wave data. TODO: Loop Points */ \
		uint32_t len = FACT_min( \
			wave->parentBank->entries[wave->index].PlayRegion.dwLength - \
				wave->position, \
			samples * depth \
		); \
		/* Go to the spot in the WaveBank where our samples start */ \
		wave->parentBank->io->seek( \
			wave->parentBank->io->data, \
			wave->parentBank->entries[wave->index].PlayRegion.dwOffset + \
				wave->position, \
			0 \
		); \
		/* Just dump it straight into the decode cache */ \
		wave->parentBank->io->read( \
			wave->parentBank->io->data, \
			decodeCache, \
			len, \
			1 \
		); \
		/* EOS? Stop! TODO: Loop Points */ \
		wave->position += len; \
		if (wave->position >= wave->parentBank->entries[wave->index].PlayRegion.dwLength) \
		{ \
			wave->state |= FACT_STATE_STOPPED; \
			wave->state &= ~FACT_STATE_PLAYING; \
		} \
		return len; \
	}
DECODE_FUNC(MonoPCM8, 1)
DECODE_FUNC(StereoPCM8, 2)
DECODE_FUNC(MonoPCM16, 2)
DECODE_FUNC(StereoPCM16, 4)
#undef DECODE_FUNC

typedef struct FACTMSADPCM1
{
	uint8_t predictor;
	int16_t delta;
	int16_t sample1;
	int16_t sample2;
} FACTMSADPCM1;
#define READ_MONO_PREAMBLE(target) \
	wave->parentBank->io->read( \
		wave->parentBank->io->data, \
		&preamble.predictor, \
		sizeof(preamble.predictor), \
		1 \
	); \
	wave->parentBank->io->read( \
		wave->parentBank->io->data, \
		&preamble.delta, \
		sizeof(preamble.delta), \
		1 \
	); \
	wave->parentBank->io->read( \
		wave->parentBank->io->data, \
		&preamble.sample1, \
		sizeof(preamble.sample1), \
		1 \
	); \
	wave->parentBank->io->read( \
		wave->parentBank->io->data, \
		&preamble.sample2, \
		sizeof(preamble.sample2), \
		1 \
	); \
	target[0] = preamble.sample2; \
	target[1] = preamble.sample1;

typedef struct FACTMSADPCM2
{
	uint8_t l_predictor;
	uint8_t r_predictor;
	int16_t l_delta;
	int16_t r_delta;
	int16_t l_sample1;
	int16_t r_sample1;
	int16_t l_sample2;
	int16_t r_sample2;
} FACTMSADPCM2;
#define READ_STEREO_PREAMBLE(target) \
	wave->parentBank->io->read( \
		wave->parentBank->io->data, \
		&preamble.l_predictor, \
		sizeof(preamble.l_predictor), \
		1 \
	); \
	wave->parentBank->io->read( \
		wave->parentBank->io->data, \
		&preamble.r_predictor, \
		sizeof(preamble.r_predictor), \
		1 \
	); \
	wave->parentBank->io->read( \
		wave->parentBank->io->data, \
		&preamble.l_delta, \
		sizeof(preamble.l_delta), \
		1 \
	); \
	wave->parentBank->io->read( \
		wave->parentBank->io->data, \
		&preamble.r_delta, \
		sizeof(preamble.r_delta), \
		1 \
	); \
	wave->parentBank->io->read( \
		wave->parentBank->io->data, \
		&preamble.l_sample1, \
		sizeof(preamble.l_sample1), \
		1 \
	); \
	wave->parentBank->io->read( \
		wave->parentBank->io->data, \
		&preamble.r_sample1, \
		sizeof(preamble.r_sample1), \
		1 \
	); \
	wave->parentBank->io->read( \
		wave->parentBank->io->data, \
		&preamble.l_sample2, \
		sizeof(preamble.l_sample2), \
		1 \
	); \
	wave->parentBank->io->read( \
		wave->parentBank->io->data, \
		&preamble.r_sample2, \
		sizeof(preamble.r_sample2), \
		1 \
	); \
	target[0] = preamble.l_sample2; \
	target[1] = preamble.r_sample2; \
	target[2] = preamble.l_sample1; \
	target[3] = preamble.r_sample1;

static const int32_t AdaptionTable[16] =
{
	230, 230, 230, 230, 307, 409, 512, 614,
	768, 614, 512, 409, 307, 230, 230, 230
};
static const int32_t AdaptCoeff_1[7] =
{
	256, 512, 0, 192, 240, 460, 392
};
static const int32_t AdaptCoeff_2[7] =
{
	0, -256, 0, 64, 0, -208, -232
};

#define PARSE_NIBBLE(tgt, nib, pdct, s1, s2, dlta) \
	signedNibble = (int8_t) nib; \
	if (signedNibble & 0x08) \
	{ \
		signedNibble -= 0x10; \
	} \
	sampleInt = ( \
		( \
			(s1 * AdaptCoeff_1[pdct]) + \
			(s2 * AdaptCoeff_2[pdct]) \
		) / 256 \
	); \
	sampleInt += signedNibble * dlta; \
	sample = FACT_clamp(sampleInt, -32768, 32767); \
	s2 = s1; \
	s1 = sample; \
	dlta = (int16_t) (AdaptionTable[nib] * (int32_t) dlta / 256); \
	if (dlta < 16) \
	{ \
		dlta = 16; \
	} \
	tgt = sample;
#define DECODE_MONO_BLOCK(target) \
	PARSE_NIBBLE( \
		target, \
		(nibbles[i] >> 4), \
		preamble.predictor, \
		preamble.sample1, \
		preamble.sample2, \
		preamble.delta \
	) \
	PARSE_NIBBLE( \
		target, \
		(nibbles[i] & 0x0F), \
		preamble.predictor, \
		preamble.sample1, \
		preamble.sample2, \
		preamble.delta \
	)
#define DECODE_STEREO_BLOCK(target) \
	PARSE_NIBBLE( \
		target, \
		(nibbles[i] >> 4), \
		preamble.l_predictor, \
		preamble.l_sample1, \
		preamble.l_sample2, \
		preamble.l_delta \
	) \
	PARSE_NIBBLE( \
		target, \
		(nibbles[i] & 0x0F), \
		preamble.r_predictor, \
		preamble.r_sample1, \
		preamble.r_sample2, \
		preamble.r_delta \
	)
#define DECODE_FUNC(type, align, bsize, chans, readpreamble, decodeblock) \
	uint32_t FACT_INTERNAL_Decode##type( \
		FACTWave *wave, \
		uint16_t *decodeCache, \
		uint32_t samples \
	) { \
		/* Iterators */ \
		uint8_t b, i; \
		/* Temp storage for ADPCM blocks */ \
		FACTMSADPCM##chans preamble; \
		uint8_t nibbles[(align + 15) * chans]; \
		int8_t signedNibble; \
		int32_t sampleInt; \
		int16_t sample; \
		/* Keep decodeCache as-is to calculate return value */ \
		uint16_t *pcm = decodeCache; \
		/* Have extra? Throw it in! */ \
		if (wave->msadpcmExtra > 0) \
		{ \
			FACT_memcpy(pcm, wave->msadpcmCache, wave->msadpcmExtra * 2); \
			pcm += wave->msadpcmExtra; \
			samples -= wave->msadpcmExtra; \
			wave->msadpcmExtra = 0; \
		} \
		/* How many blocks do we need? */ \
		uint32_t blocks = samples / bsize; \
		uint16_t extra = samples % bsize; \
		/* Don't go past the end of the wave data. TODO: Loop Points */ \
		uint32_t len = FACT_min( \
			wave->parentBank->entries[wave->index].PlayRegion.dwLength - \
				wave->position, \
			(blocks + (extra > 0)) * ((align + 22) * chans) \
		); \
		if (len < ((blocks + (extra > 0)) * ((16 + 22) * chans))) \
		{ \
			blocks = len / ((16 + 22) * chans); \
			extra = 0; /* FIXME: ??? */ \
		} \
		/* Go to the spot in the WaveBank where our samples start */ \
		wave->parentBank->io->seek( \
			wave->parentBank->io->data, \
			wave->parentBank->entries[wave->index].PlayRegion.dwOffset + \
				wave->position, \
			0 \
		); \
		/* Read in each block directly to the decode cache */ \
		for (b = 0; b < blocks; b += 1) \
		{ \
			readpreamble(pcm) \
			pcm += 2 * chans; \
			wave->parentBank->io->read( \
				wave->parentBank->io->data, \
				nibbles, \
				sizeof(nibbles), \
				1 \
			); \
			for (i = 0; i < ((align + 15) * chans); i += 1) \
			{ \
				decodeblock(*pcm++) \
			} \
		} \
		/* Have extra? Go to the MSADPCM cache */ \
		if (extra > 0) \
		{ \
			readpreamble(wave->msadpcmCache) \
			wave->parentBank->io->read( \
				wave->parentBank->io->data, \
				nibbles, \
				sizeof(nibbles), \
				1 \
			); \
			for (i = 0; i < ((align + 15) * chans); i += 1) \
			{ \
				decodeblock(wave->msadpcmCache[i + (2 * chans)]) \
			} \
			wave->msadpcmExtra = bsize - extra; \
			FACT_memcpy(pcm, wave->msadpcmCache, extra * 2); \
			FACT_memmove( \
				wave->msadpcmCache, \
				wave->msadpcmCache + extra, \
				wave->msadpcmExtra * 2 \
			); \
			pcm += extra; \
		} \
		/* EOS? Stop! TODO: Loop Points */ \
		wave->position += len; \
		if (wave->position >= wave->parentBank->entries[wave->index].PlayRegion.dwLength) \
		{ \
			wave->state |= FACT_STATE_STOPPED; \
			wave->state &= ~FACT_STATE_PLAYING; \
		} \
		return pcm - decodeCache; \
	}
DECODE_FUNC(MonoMSADPCM32,	  0,  32, 1, READ_MONO_PREAMBLE, DECODE_MONO_BLOCK)
DECODE_FUNC(MonoMSADPCM64,	 16,  64, 1, READ_MONO_PREAMBLE, DECODE_MONO_BLOCK)
DECODE_FUNC(MonoMSADPCM128,	 48, 128, 1, READ_MONO_PREAMBLE, DECODE_MONO_BLOCK)
DECODE_FUNC(MonoMSADPCM256,	112, 256, 1, READ_MONO_PREAMBLE, DECODE_MONO_BLOCK)
DECODE_FUNC(MonoMSADPCM512,	240, 512, 1, READ_MONO_PREAMBLE, DECODE_MONO_BLOCK)
DECODE_FUNC(StereoMSADPCM32,	  0,  32, 2, READ_STEREO_PREAMBLE, DECODE_STEREO_BLOCK)
DECODE_FUNC(StereoMSADPCM64,	 16,  64, 2, READ_STEREO_PREAMBLE, DECODE_STEREO_BLOCK)
DECODE_FUNC(StereoMSADPCM128,	 48, 128, 2, READ_STEREO_PREAMBLE, DECODE_STEREO_BLOCK)
DECODE_FUNC(StereoMSADPCM256,	112, 256, 2, READ_STEREO_PREAMBLE, DECODE_STEREO_BLOCK)
DECODE_FUNC(StereoMSADPCM512,	240, 512, 2, READ_STEREO_PREAMBLE, DECODE_STEREO_BLOCK)
#undef DECODE_FUNC
