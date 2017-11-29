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

	FACT_assert(0 && "RPC code not found!");
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
				FACT_assert(0 && "Unhandled RPC parameter type!");
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
			FACT_assert(0 && "Unrecognized clip event type!");
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

uint32_t FACT_INTERNAL_GetWave(
	FACTWave *wave,
	int16_t *decodeCacheL,
	int16_t *decodeCacheR,
	float *resampleCacheL,
	float *resampleCacheR,
	uint32_t samples
) {
	uint32_t i;
	fixed32 cur;
	uint64_t sizeRequest;
	uint32_t decodeLength, resampleLength = 0;

	/* If the sample rates match, just decode and convert to float */
	if (wave->resample.step == FIXED_ONE)
	{
		decodeLength = samples;
		if (wave->resample.offset & FIXED_FRACTION_MASK)
		{
			FACT_memcpy(
				decodeCacheL,
				wave->resample.padding[0],
				RESAMPLE_PADDING * 2
			);
			FACT_memcpy(
				decodeCacheR,
				wave->resample.padding[1],
				RESAMPLE_PADDING * 2
			);
			decodeLength = wave->decode(
				wave,
				decodeCacheL + RESAMPLE_PADDING,
				decodeCacheR + RESAMPLE_PADDING,
				samples - RESAMPLE_PADDING
			);
			wave->resample.offset += decodeLength * FIXED_ONE;
			decodeLength += RESAMPLE_PADDING;
		}
		else
		{
			decodeLength = wave->decode(
				wave,
				decodeCacheL,
				decodeCacheR,
				samples
			);
			wave->resample.offset += decodeLength * FIXED_ONE;
		}
		resampleLength = decodeLength;
		if (resampleLength > 0)
		{
			FACT_memcpy(
				wave->resample.padding[0],
				(
					decodeCacheL +
					decodeLength -
					RESAMPLE_PADDING
				),
				RESAMPLE_PADDING * 2
			);
			FACT_memcpy(
				wave->resample.padding[1],
				(
					decodeCacheR +
					decodeLength -
					RESAMPLE_PADDING
				),
				RESAMPLE_PADDING * 2
			);

			/* Lazy int16_t to float */
			if (wave->stereo)
			{
				for (i = 0; i < resampleLength; i += 1)
				{
					resampleCacheL[i] = decodeCacheL[i] / 32768.0f;
					resampleCacheR[i] = decodeCacheR[i] / 32768.0f;
				}
			}
			else
			{
				for (i = 0; i < resampleLength; i += 1)
				{
					resampleCacheL[i] = decodeCacheL[i] / 32768.0f;
				}
			}
		}
		return resampleLength;
	}

	/* The easy part is just multiplying the final output size with the step
	 * to get the "real" buffer size. But we also need to ceil() to get the
	 * extra sample needed for interpolating past the "end" of the
	 * unresampled buffer.
	 */
	sizeRequest = samples * wave->resample.step;
	sizeRequest += (
		/* If frac > 0, int goes up by one... */
		(wave->resample.offset + FIXED_FRACTION_MASK) &
		/* ... then we chop off anything left */
		FIXED_FRACTION_MASK
	);
	sizeRequest >>= FIXED_PRECISION;

	/* Only add half the padding length!
	 * For the starting buffer, we have no pre-pad, for all remaining
	 * buffers we memcpy the end and that becomes the new pre-pad.
	 */
	sizeRequest += RESAMPLE_PADDING;

	/* FIXME: Can high sample rates ruin this? */
	decodeLength = (uint32_t) sizeRequest;
	FACT_assert(decodeLength == sizeRequest);

	/* Decode... */
	if (wave->resample.offset == 0)
	{
		decodeLength = wave->decode(
			wave,
			decodeCacheL,
			decodeCacheR,
			decodeLength
		);
	}
	else
	{
		/* Copy the end to the start first! */
		FACT_memcpy(
			decodeCacheL,
			wave->resample.padding[0],
			RESAMPLE_PADDING * 2
		);
		FACT_memcpy(
			decodeCacheR,
			wave->resample.padding[1],
			RESAMPLE_PADDING * 2
		);

		/* Don't overwrite the start! */
		decodeLength = wave->decode(
			wave,
			decodeCacheL + RESAMPLE_PADDING,
			decodeCacheR + RESAMPLE_PADDING,
			decodeLength
		);
	}

	/* ... then Resample... */
	if (decodeLength > 0)
	{
		/* The end will be the start next time */
		FACT_memcpy(
			wave->resample.padding[0],
			(
				decodeCacheL +
				decodeLength -
				RESAMPLE_PADDING
			),
			RESAMPLE_PADDING * 2
		);
		FACT_memcpy(
			wave->resample.padding[1],
			(
				decodeCacheR +
				decodeLength -
				RESAMPLE_PADDING
			),
			RESAMPLE_PADDING * 2
		);

		/* Now that we have the raw samples, we have to reverse some of
		 * the math to get the real output size (read: Drop the padding
		 * numbers)
		 */
		sizeRequest = decodeLength - RESAMPLE_PADDING;
		/* uint32_t to fixed32 */
		sizeRequest <<= FIXED_PRECISION;
		/* Division also turns fixed32 into uint32_t */
		sizeRequest /= wave->resample.step;

		resampleLength = (uint32_t) sizeRequest;
		FACT_assert(resampleLength == sizeRequest);

		/* TODO: Something fancier than a linear resampler */
		cur = wave->resample.offset & FIXED_FRACTION_MASK;
		if (wave->stereo)
		{
			for (i = 0; i < resampleLength; i += 1)
			{
				/* lerp, then convert to float value */
				resampleCacheL[i] = (float) (
					decodeCacheL[0] +
					(decodeCacheL[1] - decodeCacheL[0]) * FIXED_TO_DOUBLE(cur)
				) / 32768.0f;
				resampleCacheR[i] = (float) (
					decodeCacheR[0] +
					(decodeCacheR[1] - decodeCacheR[0]) * FIXED_TO_DOUBLE(cur)
				) / 32768.0f;

				/* Increment fraction offset by the stepping value */
				wave->resample.offset += wave->resample.step;
				cur += wave->resample.step;

				/* Only increment the sample offset by integer values.
				 * Sometimes this will be 0 until cur accumulates
				 * enough steps, especially for "slow" rates.
				 */
				decodeCacheL += cur >> FIXED_PRECISION;
				decodeCacheR += cur >> FIXED_PRECISION;

				/* Now that any integer has been added, drop it.
				 * The offset pointer will preserve the total.
				 */
				cur &= FIXED_FRACTION_MASK;
			}
		}
		else
		{
			for (i = 0; i < resampleLength; i += 1)
			{
				/* lerp, then convert to float value */
				resampleCacheL[i] = (float) (
					decodeCacheL[0] +
					(decodeCacheL[1] - decodeCacheL[0]) * FIXED_TO_DOUBLE(cur)
				) / 32768.0f;

				/* Increment fraction offset by the stepping value */
				wave->resample.offset += wave->resample.step;
				cur += wave->resample.step;

				/* Only increment the sample offset by integer values.
				 * Sometimes this will be 0 until cur accumulates
				 * enough steps, especially for "slow" rates.
				 */
				decodeCacheL += cur >> FIXED_PRECISION;

				/* Now that any integer has been added, drop it.
				 * The offset pointer will preserve the total.
				 */
				cur &= FIXED_FRACTION_MASK;
			}
		}
	}
	return resampleLength;
}

/* 8-bit PCM Decoding */

uint32_t FACT_INTERNAL_DecodeMonoPCM8(
	FACTWave *wave,
	int16_t *decodeCacheL,
	int16_t *decodeCacheR,
	uint32_t samples
) {
	uint32_t i;
	int8_t sample;

	/* Don't go past the end of the wave data. TODO: Loop Points */
	uint32_t len = FACT_min(
		wave->parentBank->entries[wave->index].PlayRegion.dwLength -
			wave->position,
		samples
	);

	/* Go to the spot in the WaveBank where our samples start */
	wave->parentBank->io->seek(
		wave->parentBank->io->data,
		wave->parentBank->entries[wave->index].PlayRegion.dwOffset +
			wave->position,
		0
	);

	/* Read each sample, convert to 16-bit. Slow as hell. */
	for (i = 0; i < len; i += 1)
	{
		wave->parentBank->io->read(
			wave->parentBank->io->data,
			&sample,
			1,
			1
		);
		decodeCacheL[i] = (int16_t) sample << 8;
	}

	wave->position += len;
	return len;
}

uint32_t FACT_INTERNAL_DecodeStereoPCM8(
	FACTWave *wave,
	int16_t *decodeCacheL,
	int16_t *decodeCacheR,
	uint32_t samples
) {
	uint32_t i;
	int8_t sample[2];

	/* Don't go past the end of the wave data. TODO: Loop Points */
	uint32_t len = FACT_min(
		wave->parentBank->entries[wave->index].PlayRegion.dwLength -
			wave->position,
		samples * 2
	);

	/* Go to the spot in the WaveBank where our samples start */
	wave->parentBank->io->seek(
		wave->parentBank->io->data,
		wave->parentBank->entries[wave->index].PlayRegion.dwOffset +
			wave->position,
		0
	);

	/* Read each sample, convert to 16-bit. Slow as hell. */
	for (i = 0; i < len; i += 2)
	{
		wave->parentBank->io->read(
			wave->parentBank->io->data,
			sample,
			1,
			2
		);
		decodeCacheL[i] = (int16_t) sample[0] << 8;
		decodeCacheR[i] = (int16_t) sample[1] << 8;
	}

	wave->position += len;
	return len / 2;
}

uint32_t FACT_INTERNAL_DecodeStereoToMonoPCM8(
	FACTWave *wave,
	int16_t *decodeCacheL,
	int16_t *decodeCacheR,
	uint32_t samples
) {
	uint32_t i;
	int8_t sample[2];

	/* Don't go past the end of the wave data. TODO: Loop Points */
	uint32_t len = FACT_min(
		wave->parentBank->entries[wave->index].PlayRegion.dwLength -
			wave->position,
		samples * 2
	);

	/* Go to the spot in the WaveBank where our samples start */
	wave->parentBank->io->seek(
		wave->parentBank->io->data,
		wave->parentBank->entries[wave->index].PlayRegion.dwOffset +
			wave->position,
		0
	);

	/* Read each sample, convert to 16-bit. Slow as hell. */
	for (i = 0; i < len; i += 2)
	{
		wave->parentBank->io->read(
			wave->parentBank->io->data,
			sample,
			1,
			2
		);
		decodeCacheL[i] = (
			(int16_t) sample[0] + (int16_t) sample[1]
		) << 7;
	}

	wave->position += len;
	return len / 2;
}

/* 16-bit PCM Decoding */

uint32_t FACT_INTERNAL_DecodeMonoPCM16(
	FACTWave *wave,
	int16_t *decodeCacheL,
	int16_t *decodeCacheR,
	uint32_t samples
) {
	/* Don't go past the end of the wave data. TODO: Loop Points */
	uint32_t len = FACT_min(
		wave->parentBank->entries[wave->index].PlayRegion.dwLength -
			wave->position,
		samples * 2
	);

	/* Go to the spot in the WaveBank where our samples start */
	wave->parentBank->io->seek(
		wave->parentBank->io->data,
		wave->parentBank->entries[wave->index].PlayRegion.dwOffset +
			wave->position,
		0
	);

	/* Just dump it straight into the decode cache */
	wave->parentBank->io->read(
		wave->parentBank->io->data,
		decodeCacheL,
		len,
		1
	);

	wave->position += len;
	return len / 2;
}

uint32_t FACT_INTERNAL_DecodeStereoPCM16(
	FACTWave *wave,
	int16_t *decodeCacheL,
	int16_t *decodeCacheR,
	uint32_t samples
) {
	uint32_t i;

	/* Don't go past the end of the wave data. TODO: Loop Points */
	uint32_t len = FACT_min(
		wave->parentBank->entries[wave->index].PlayRegion.dwLength -
			wave->position,
		samples * 4
	);

	/* Go to the spot in the WaveBank where our samples start */
	wave->parentBank->io->seek(
		wave->parentBank->io->data,
		wave->parentBank->entries[wave->index].PlayRegion.dwOffset +
			wave->position,
		0
	);

	/* Read each sample into the right channel. Slow as hell. */
	for (i = 0; i < (len / 4); i += 1)
	{
		wave->parentBank->io->read(
			wave->parentBank->io->data,
			&decodeCacheL[i],
			2,
			1
		);
		wave->parentBank->io->read(
			wave->parentBank->io->data,
			&decodeCacheR[i],
			2,
			1
		);
	}

	wave->position += len;
	return len / 4;
}

uint32_t FACT_INTERNAL_DecodeStereoToMonoPCM16(
	FACTWave *wave,
	int16_t *decodeCacheL,
	int16_t *decodeCacheR,
	uint32_t samples
) {
	uint32_t i;

	/* Don't go past the end of the wave data. TODO: Loop Points */
	uint32_t len = FACT_min(
		wave->parentBank->entries[wave->index].PlayRegion.dwLength -
			wave->position,
		samples * 4
	);

	/* Go to the spot in the WaveBank where our samples start */
	wave->parentBank->io->seek(
		wave->parentBank->io->data,
		wave->parentBank->entries[wave->index].PlayRegion.dwOffset +
			wave->position,
		0
	);

	/* Read each sample into the right channel. Slow as hell. */
	for (i = 0; i < (len / 4); i += 1)
	{
		wave->parentBank->io->read(
			wave->parentBank->io->data,
			decodeCacheR,
			4,
			1
		);
		decodeCacheL[i] = (int16_t) ((
			(int32_t) decodeCacheR[0] + (int32_t) decodeCacheR[1]
		) / 2);
	}

	wave->position += len;
	return len / 4;
}

/* MSADPCM Decoding */

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

/* Mono */

#define DECODE_MONO_BLOCK(target) \
	PARSE_NIBBLE( \
		target, \
		(nibbles[i] >> 4), \
		predictor, \
		sample1, \
		sample2, \
		delta \
	) \
	PARSE_NIBBLE( \
		target, \
		(nibbles[i] & 0x0F), \
		predictor, \
		sample1, \
		sample2, \
		delta \
	)
#define READ_MONO_PREAMBLE(target) \
	wave->parentBank->io->read( \
		wave->parentBank->io->data, \
		&predictor, \
		sizeof(predictor), \
		1 \
	); \
	wave->parentBank->io->read( \
		wave->parentBank->io->data, \
		&delta, \
		sizeof(delta), \
		1 \
	); \
	wave->parentBank->io->read( \
		wave->parentBank->io->data, \
		&sample1, \
		sizeof(sample1), \
		1 \
	); \
	wave->parentBank->io->read( \
		wave->parentBank->io->data, \
		&sample2, \
		sizeof(sample2), \
		1 \
	); \
	*target++ = sample2; \
	*target++ = sample1;

#define DECODE_FUNC(type, align, bsize) \
	uint32_t FACT_INTERNAL_Decode##type( \
		FACTWave *wave, \
		int16_t *decodeCacheL, \
		int16_t *decodeCacheR, \
		uint32_t samples \
	) { \
		/* Iterators */ \
		uint8_t b, i; \
		/* Temp storage for ADPCM blocks */ \
		uint8_t predictor; \
		int16_t delta; \
		int16_t sample1; \
		int16_t sample2; \
		uint8_t nibbles[(align + 15)]; \
		int8_t signedNibble; \
		int32_t sampleInt; \
		int16_t sample; \
		/* Keep decodeCache as-is to calculate return value */ \
		int16_t *pcm = decodeCacheL; \
		int16_t *pcmExtra = wave->msadpcmCache; \
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
			(blocks + (extra > 0)) * ((align + 22)) \
		); \
		/* len might be 0 if we just came back for tail samples */ \
		if (len == 0) \
		{ \
			return (pcm - decodeCacheL) * 2; \
		} \
		if (len < ((blocks + (extra > 0)) * ((16 + 22)))) \
		{ \
			blocks = len / (16 + 22); \
			extra = len % (16 + 22); \
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
			READ_MONO_PREAMBLE(pcm) \
			wave->parentBank->io->read( \
				wave->parentBank->io->data, \
				nibbles, \
				sizeof(nibbles), \
				1 \
			); \
			for (i = 0; i < (align + 15); i += 1) \
			{ \
				DECODE_MONO_BLOCK(*pcm++) \
			} \
		} \
		/* Have extra? Go to the MSADPCM cache */ \
		if (extra > 0) \
		{ \
			READ_MONO_PREAMBLE(pcmExtra) \
			wave->parentBank->io->read( \
				wave->parentBank->io->data, \
				nibbles, \
				sizeof(nibbles), \
				1 \
			); \
			for (i = 0; i < (align + 15); i += 1) \
			{ \
				DECODE_MONO_BLOCK(*pcmExtra++) \
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
		wave->position += len; \
		return (pcm - decodeCacheL); \
	}
DECODE_FUNC(MonoMSADPCM32,	  0,  32)
DECODE_FUNC(MonoMSADPCM64,	 16,  64)
DECODE_FUNC(MonoMSADPCM128,	 48, 128)
DECODE_FUNC(MonoMSADPCM256,	112, 256)
DECODE_FUNC(MonoMSADPCM512,	240, 512)
#undef DECODE_FUNC

/* Stereo Shared Macros */

#define DECODE_STEREO_BLOCK(target1, target2) \
	PARSE_NIBBLE( \
		target1, \
		(nibbles[i] >> 4), \
		l_predictor, \
		l_sample1, \
		l_sample2, \
		l_delta \
	) \
	PARSE_NIBBLE( \
		target2, \
		(nibbles[i] & 0x0F), \
		r_predictor, \
		r_sample1, \
		r_sample2, \
		r_delta \
	)
#define READ_STEREO_PREAMBLE \
	wave->parentBank->io->read( \
		wave->parentBank->io->data, \
		&l_predictor, \
		sizeof(l_predictor), \
		1 \
	); \
	wave->parentBank->io->read( \
		wave->parentBank->io->data, \
		&r_predictor, \
		sizeof(r_predictor), \
		1 \
	); \
	wave->parentBank->io->read( \
		wave->parentBank->io->data, \
		&l_delta, \
		sizeof(l_delta), \
		1 \
	); \
	wave->parentBank->io->read( \
		wave->parentBank->io->data, \
		&r_delta, \
		sizeof(r_delta), \
		1 \
	); \
	wave->parentBank->io->read( \
		wave->parentBank->io->data, \
		&l_sample1, \
		sizeof(l_sample1), \
		1 \
	); \
	wave->parentBank->io->read( \
		wave->parentBank->io->data, \
		&r_sample1, \
		sizeof(r_sample1), \
		1 \
	); \
	wave->parentBank->io->read( \
		wave->parentBank->io->data, \
		&l_sample2, \
		sizeof(l_sample2), \
		1 \
	); \
	wave->parentBank->io->read( \
		wave->parentBank->io->data, \
		&r_sample2, \
		sizeof(r_sample2), \
		1 \
	);

/* Stereo */
#define DECODE_FUNC(type, align, bsize) \
	uint32_t FACT_INTERNAL_Decode##type( \
		FACTWave *wave, \
		int16_t *decodeCacheL, \
		int16_t *decodeCacheR, \
		uint32_t samples \
	) { \
		/* Iterators */ \
		uint8_t b, i; \
		/* Temp storage for ADPCM blocks */ \
		uint8_t l_predictor; \
		uint8_t r_predictor; \
		int16_t l_delta; \
		int16_t r_delta; \
		int16_t l_sample1; \
		int16_t r_sample1; \
		int16_t l_sample2; \
		int16_t r_sample2; \
		uint8_t nibbles[(align + 15) * 2]; \
		int8_t signedNibble; \
		int32_t sampleInt; \
		int16_t sample; \
		/* Keep decodeCache as-is to calculate return value */ \
		int16_t *pcmL = decodeCacheL; \
		int16_t *pcmR = decodeCacheR; \
		int16_t *pcmExtra = wave->msadpcmCache; \
		/* Have extra? Throw it in! */ \
		if (wave->msadpcmExtra > 0) \
		{ \
			for (i = 0; i < wave->msadpcmExtra; i += 2) \
			{ \
				*pcmL++ = wave->msadpcmCache[i]; \
				*pcmR++ = wave->msadpcmCache[i + 1]; \
			} \
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
			(blocks + (extra > 0)) * ((align + 22) * 2) \
		); \
		/* len might be 0 if we just came back for tail samples */ \
		if (len == 0) \
		{ \
			return (pcmL - decodeCacheL) * 2; \
		} \
		if (len < ((blocks + (extra > 0)) * ((16 + 22) * 2))) \
		{ \
			blocks = len / ((16 + 22) * 2); \
			extra = len % ((16 + 22) * 2); \
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
			READ_STEREO_PREAMBLE \
			*pcmL++ = l_sample1; \
			*pcmR++ = r_sample1; \
			*pcmL++ = l_sample2; \
			*pcmR++ = r_sample2; \
			wave->parentBank->io->read( \
				wave->parentBank->io->data, \
				nibbles, \
				sizeof(nibbles), \
				1 \
			); \
			for (i = 0; i < ((align + 15) * 2); i += 1) \
			{ \
				DECODE_STEREO_BLOCK(*pcmL++, *pcmR++) \
			} \
		} \
		/* Have extra? Go to the MSADPCM cache */ \
		if (extra > 0) \
		{ \
			READ_STEREO_PREAMBLE \
			*pcmExtra++ = l_sample1; \
			*pcmExtra++ = r_sample1; \
			*pcmExtra++ = l_sample2; \
			*pcmExtra++ = r_sample2; \
			wave->parentBank->io->read( \
				wave->parentBank->io->data, \
				nibbles, \
				sizeof(nibbles), \
				1 \
			); \
			for (i = 0; i < ((align + 15) * 2); i += 1) \
			{ \
				DECODE_STEREO_BLOCK(*pcmExtra++, *pcmExtra++) \
			} \
			wave->msadpcmExtra = bsize - extra; \
			for (i = 0; i < extra; i += 2) \
			{ \
				*pcmL++ = wave->msadpcmCache[i]; \
				*pcmR++ = wave->msadpcmCache[i + 1]; \
			} \
			FACT_memmove( \
				wave->msadpcmCache, \
				wave->msadpcmCache + extra, \
				wave->msadpcmExtra * 2 \
			); \
		} \
		wave->position += len; \
		return (pcmL - decodeCacheL); \
	}
DECODE_FUNC(StereoMSADPCM32,	  0,  32)
DECODE_FUNC(StereoMSADPCM64,	 16,  64)
DECODE_FUNC(StereoMSADPCM128,	 48, 128)
DECODE_FUNC(StereoMSADPCM256,	112, 256)
DECODE_FUNC(StereoMSADPCM512,	240, 512)
#undef DECODE_FUNC

/* StereoToMono */
#define DECODE_FUNC(type, align, bsize) \
	uint32_t FACT_INTERNAL_Decode##type( \
		FACTWave *wave, \
		int16_t *decodeCacheL, \
		int16_t *decodeCacheR, \
		uint32_t samples \
	) { \
		/* Iterators */ \
		uint8_t b, i; \
		/* Temp storage for ADPCM blocks */ \
		uint8_t l_predictor; \
		uint8_t r_predictor; \
		int16_t l_delta; \
		int16_t r_delta; \
		int16_t l_sample1; \
		int16_t r_sample1; \
		int16_t l_sample2; \
		int16_t r_sample2; \
		uint8_t nibbles[(align + 15) * 2]; \
		int8_t signedNibble; \
		int32_t sampleInt; \
		int16_t sample; \
		/* Keep decodeCache as-is to calculate return value */ \
		int16_t *pcm = decodeCacheL; \
		int16_t *pcmExtra = wave->msadpcmCache; \
		/* Have extra? Throw it in! */ \
		if (wave->msadpcmExtra > 0) \
		{ \
			for (i = 0; i < wave->msadpcmExtra; i += 2) \
			{ \
				*pcm++ = (int16_t) (( \
					(int32_t) wave->msadpcmCache[i] + \
					(int32_t) wave->msadpcmCache[i + 1] \
				) / 2); \
			} \
			samples -= wave->msadpcmExtra / 2; \
			wave->msadpcmExtra = 0; \
		} \
		/* How many blocks do we need? */ \
		uint32_t blocks = samples / bsize; \
		uint16_t extra = samples % bsize; \
		/* Don't go past the end of the wave data. TODO: Loop Points */ \
		uint32_t len = FACT_min( \
			wave->parentBank->entries[wave->index].PlayRegion.dwLength - \
				wave->position, \
			(blocks + (extra > 0)) * ((align + 22) * 2) \
		); \
		/* len might be 0 if we just came back for tail samples */ \
		if (len == 0) \
		{ \
			return (pcm - decodeCacheL) * 2; \
		} \
		if (len < ((blocks + (extra > 0)) * ((16 + 22) * 2))) \
		{ \
			blocks = len / ((16 + 22) * 2); \
			extra = len % ((16 + 22) * 2); \
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
			READ_STEREO_PREAMBLE \
			*pcm++ = (int16_t) (( \
				(int32_t) l_sample1 + (int32_t) r_sample1 \
			) / 2); \
			*pcm++ = (int16_t) (( \
				(int32_t) l_sample2 + (int32_t) r_sample2 \
			) / 2); \
			wave->parentBank->io->read( \
				wave->parentBank->io->data, \
				nibbles, \
				sizeof(nibbles), \
				1 \
			); \
			for (i = 0; i < ((align + 15) * 2); i += 1) \
			{ \
				DECODE_STEREO_BLOCK(decodeCacheR[0], decodeCacheR[1]) \
				*pcm++ = (int16_t) (( \
					(int32_t) decodeCacheR[0] + (int32_t) decodeCacheR[1] \
				) / 2); \
			} \
		} \
		/* Have extra? Go to the MSADPCM cache */ \
		if (extra > 0) \
		{ \
			READ_STEREO_PREAMBLE \
			*pcmExtra++ = l_sample1; \
			*pcmExtra++ = r_sample1; \
			*pcmExtra++ = l_sample2; \
			*pcmExtra++ = r_sample2; \
			wave->parentBank->io->read( \
				wave->parentBank->io->data, \
				nibbles, \
				sizeof(nibbles), \
				1 \
			); \
			for (i = 0; i < ((align + 15) * 2); i += 1) \
			{ \
				DECODE_STEREO_BLOCK(*pcmExtra++, *pcmExtra++) \
			} \
			wave->msadpcmExtra = bsize - extra; \
			for (i = 0; i < extra; i += 2) \
			{ \
				*pcm++ = (int16_t) (( \
					(int32_t) wave->msadpcmCache[i] + \
					(int32_t) wave->msadpcmCache[i + 1] \
				) / 2); \
			} \
			FACT_memmove( \
				wave->msadpcmCache, \
				wave->msadpcmCache + extra, \
				wave->msadpcmExtra * 2 \
			); \
		} \
		wave->position += len; \
		return (pcm - decodeCacheL); \
	}
DECODE_FUNC(StereoToMonoMSADPCM32,	  0,  32)
DECODE_FUNC(StereoToMonoMSADPCM64,	 16,  64)
DECODE_FUNC(StereoToMonoMSADPCM128,	 48, 128)
DECODE_FUNC(StereoToMonoMSADPCM256,	112, 256)
DECODE_FUNC(StereoToMonoMSADPCM512,	240, 512)
#undef DECODE_FUNC
