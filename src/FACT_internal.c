/* FACT - XACT Reimplementation for FNA
 * Copyright 2009-2018 Ethan Lee and the MonoGame Team
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
				if (FACT_strcmp(
					engine->variableNames[rpc->variable],
					"AttackTime"
				) == 0) {
					/* TODO: AttackTime */
					rpcResult = 0.0f;
				}
				else if (FACT_strcmp(
					engine->variableNames[rpc->variable],
					"ReleaseTime"
				) == 0) {
					/* TODO: ReleaseTime */
					rpcResult = 0.0f;
				}
				else
				{
					rpcResult = FACT_INTERNAL_CalculateRPC(
						rpc,
						cue->variableValues[rpc->variable]
					);
				}
			}
			else
			{
				rpcResult = FACT_INTERNAL_CalculateRPC(
					rpc,
					engine->globalVariableValues[rpc->variable]
				);
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

float FACT_INTERNAL_CalculateAmplitudeRatio(float decibel)
{
	return FACT_pow(10.0, decibel / 2000.0);
}

void FACT_INTERNAL_SelectSound(FACTCue *cue)
{
	uint16_t i, j;
	float max, next, weight;
	const char *wbName;
	FACTWaveBank *wb;

	if (cue->data->flags & 0x04)
	{
		/* Sound */
		cue->playing.sound.sound = cue->sound.sound;
		cue->active = 0x02;
	}
	else
	{
		/* Variation */
		if (cue->sound.variation->flags == 3)
		{
			/* Interactive */
			if (cue->parentBank->parentEngine->variables[cue->sound.variation->variable].accessibility & 0x04)
			{
				FACTCue_GetVariable(
					cue,
					cue->sound.variation->variable,
					&next
				);
			}
			else
			{
				FACTAudioEngine_GetGlobalVariable(
					cue->parentBank->parentEngine,
					cue->sound.variation->variable,
					&next
				);
			}
			for (i = 0; i < cue->sound.variation->entryCount; i += 1)
			{
				if (	next <= cue->sound.variation->entries[i].maxWeight &&
					next >= cue->sound.variation->entries[i].minWeight	)
				{
					break;
				}
			}

			/* This should only happen when the user control
			 * variable is none of the sound probabilities, in
			 * which case we are just silent. But, we should still
			 * claim to be "playing" in the meantime.
			 */
			if (i == cue->sound.variation->entryCount)
			{
				cue->active = 0x00;
				return;
			}
		}
		else
		{
			/* Random */
			max = 0.0f;
			for (i = 0; i < cue->sound.variation->entryCount; i += 1)
			{
				max += (
					cue->sound.variation->entries[i].maxWeight -
					cue->sound.variation->entries[i].minWeight
				);
			}
			next = FACT_rng() * max;

			/* Use > 0, not >= 0. If we hit 0, that's it! */
			for (i = cue->sound.variation->entryCount - 1; i > 0; i -= 1)
			{
				weight = (
					cue->sound.variation->entries[i].maxWeight -
					cue->sound.variation->entries[i].minWeight
				);
				if (next > (max - weight))
				{
					break;
				}
				max -= weight;
			}
		}

		if (cue->sound.variation->isComplex)
		{
			/* Grab the Sound via the code. FIXME: Do this at load time? */
			for (j = 0; j < cue->parentBank->soundCount; j += 1)
			{
				if (cue->sound.variation->entries[i].soundCode == cue->parentBank->soundCodes[j])
				{
					cue->playing.sound.sound = &cue->parentBank->sounds[j];
					break;
				}
			}
			cue->active = 0x02;
		}
		else
		{
			/* Pull in the WaveBank... */
			wbName = cue->parentBank->wavebankNames[
				cue->sound.variation->entries[i].simple.wavebank
			];
			wb = cue->parentBank->parentEngine->wbList;
			while (wb != NULL)
			{
				if (FACT_strcmp(wbName, wb->name) == 0)
				{
					break;
				}
				wb = wb->next;
			}
			FACT_assert(wb != NULL);

			/* Generate the wave... */
			FACTWaveBank_Prepare(
				wb,
				cue->sound.variation->entries[i].simple.track,
				0,
				0,
				0,
				&cue->playing.wave
			);
			cue->active = 0x01;
		}
	}

	/* Alloc SoundInstance variables */
	if (cue->active & 0x02)
	{
		cue->playing.sound.tracks = (FACTTrackInstance*) FACT_malloc(
			sizeof(FACTTrackInstance) * cue->playing.sound.sound->trackCount
		);
		for (i = 0; i < cue->playing.sound.sound->trackCount; i += 1)
		{
			cue->playing.sound.tracks[i].events = (FACTEventInstance*) FACT_malloc(
				sizeof(FACTEventInstance) * cue->playing.sound.sound->tracks[i].eventCount
			);
			for (j = 0; j < cue->playing.sound.sound->tracks[i].eventCount; j += 1)
			{
				cue->playing.sound.tracks[i].events[j].timestamp =
					cue->playing.sound.sound->tracks[i].events[j].timestamp;
				cue->playing.sound.tracks[i].events[j].loopCount =
					cue->playing.sound.sound->tracks[i].events[j].loopCount;
				cue->playing.sound.tracks[i].events[j].finished = 0;

				FACT_zero(
					&cue->playing.sound.tracks[i].events[j].data,
					sizeof(cue->playing.sound.tracks[i].events[j].data)
				);
			}

			cue->playing.sound.tracks[i].rpcData.rpcVolume = 1.0f;
			cue->playing.sound.tracks[i].rpcData.rpcPitch = 0.0f;
			cue->playing.sound.tracks[i].rpcData.rpcFilterFreq = 0.0f; /* FIXME */
		}
	}
}

void FACT_INTERNAL_BeginFadeIn(FACTCue *cue)
{
	/* TODO */
}

void FACT_INTERNAL_BeginFadeOut(FACTCue *cue)
{
	/* TODO */
}

void FACT_INTERNAL_ActivateEvent(
	FACTCue *cue,
	FACTSound *sound,
	FACTTrack *track,
	FACTTrackInstance *trackInst,
	FACTEvent *evt,
	FACTEventInstance *evtInst,
	uint32_t elapsed
) {
	uint8_t i;
	float svResult;
	const char *wbName;
	uint16_t wbTrack;
	uint8_t wbIndex;
	FACTWaveBank *wb;
	uint8_t skipLoopCheck = 0;

	/* STOP */
	if (evt->type == FACTEVENT_STOP)
	{
		/* Stop Cue */
		if (evt->stop.flags & 0x02)
		{
			FACTCue_Stop(cue, evt->stop.flags & 0x01);
		}

		/* Stop track */
		else for (i = 0; i < track->eventCount; i += 1)
		if (	track->events[i].type == FACTEVENT_PLAYWAVE ||
			track->events[i].type == FACTEVENT_PLAYWAVETRACKVARIATION ||
			track->events[i].type == FACTEVENT_PLAYWAVEEFFECTVARIATION ||
			track->events[i].type == FACTEVENT_PLAYWAVETRACKEFFECTVARIATION	)
		{
			if (trackInst->events[i].data.wave.wave != NULL)
			{
				FACTWave_Stop(
					trackInst->events[i].data.wave.wave,
					evt->stop.flags & 0x01
				);
			}
		}
	}

	/* PLAYWAVE */
	else if (	evt->type == FACTEVENT_PLAYWAVE ||
			evt->type == FACTEVENT_PLAYWAVETRACKVARIATION ||
			evt->type == FACTEVENT_PLAYWAVEEFFECTVARIATION ||
			evt->type == FACTEVENT_PLAYWAVETRACKEFFECTVARIATION	)
	{
		/* Track Variation */
		if (evt->wave.isComplex)
		{
			/* TODO: Complex wave selection */
			wbIndex = evt->wave.complex.wavebanks[0];
			wbTrack = evt->wave.complex.tracks[0];
		}
		else
		{
			wbIndex = evt->wave.simple.wavebank;
			wbTrack = evt->wave.simple.track;
		}
		wbName = cue->parentBank->wavebankNames[wbIndex];
		wb = cue->parentBank->parentEngine->wbList;
		while (wb != NULL)
		{
			if (FACT_strcmp(wbName, wb->name) == 0)
			{
				break;
			}
			wb = wb->next;
		}
		FACT_assert(wb != NULL);

		/* Generate the wave... */
		FACTWaveBank_Prepare(
			wb,
			wbTrack,
			evt->wave.flags,
			0,
			0, /* FIXME: evtInst->loopCount? */
			&evtInst->data.wave.wave
		);

		/* TODO: Position/Angle */

		/* Pitch Variation */
		if (evt->wave.variationFlags & 0x1000)
		{
			const int16_t rngPitch = (int16_t) (
				FACT_rng() *
				(evt->wave.maxPitch - evt->wave.minPitch)
			);
			if (evtInst->loopCount < evt->loopCount)
			{
				/* Variation on Loop */
				if (evt->wave.variationFlags & 0x0100)
				{
					/* Add/Replace */
					if (evt->wave.variationFlags & 0x0004)
					{
						evtInst->data.wave.basePitch += rngPitch;
					}
					else
					{
						evtInst->data.wave.basePitch = rngPitch + sound->pitch;
					}
				}
			}
			else
			{
				/* Initial Pitch Variation */
				evtInst->data.wave.basePitch = rngPitch + sound->pitch;
			}
		}
		else
		{
			evtInst->data.wave.basePitch = sound->pitch;
		}

		/* Volume Variation */
		if (evt->wave.variationFlags & 0x2000)
		{
			const float rngVolume = track->volume + (
				FACT_rng() *
				(evt->wave.maxVolume - evt->wave.minVolume)
			);
			if (evtInst->loopCount < evt->loopCount)
			{
				/* Variation on Loop */
				if (evt->wave.variationFlags & 0x0200)
				{
					/* Add/Replace */
					if (evt->wave.variationFlags & 0x0001)
					{
						evtInst->data.wave.baseVolume += rngVolume;
					}
					else
					{
						evtInst->data.wave.baseVolume = rngVolume + sound->volume;
					}
				}
			}
			else
			{
				/* Initial Volume Variation */
				evtInst->data.wave.baseVolume = rngVolume + sound->volume;
			}
		}
		else
		{
			evtInst->data.wave.baseVolume = sound->volume + track->volume;
		}

		/* Filter Variation, QFactor/Freq are always together */
		if (evt->wave.variationFlags & 0xC000)
		{
			const float rngQFactor = (
				FACT_rng() *
				(evt->wave.maxQFactor - evt->wave.minQFactor)
			);
			const float rngFrequency = (
				FACT_rng() *
				(evt->wave.maxFrequency - evt->wave.minFrequency)
			);
			if (evtInst->loopCount < evt->loopCount)
			{
				/* Variation on Loop */
				if (evt->wave.variationFlags & 0x0C00)
				{
					/* TODO: Add/Replace */
					/* FIXME: Which is QFactor/Freq?
					if (evt->wave.variationFlags & 0x0010)
					{
					}
					else
					{
					}
					if (evt->wave.variationFlags & 0x0040)
					{
					}
					else
					{
					}
					*/
					evtInst->data.wave.baseQFactor = rngQFactor;
					evtInst->data.wave.baseFrequency = rngFrequency;
				}
			}
			else
			{
				/* Initial Filter Variation */
				evtInst->data.wave.baseQFactor = rngQFactor;
				evtInst->data.wave.baseFrequency = rngFrequency;
			}
		}
		else
		{
			evtInst->data.wave.baseQFactor = track->qfactor;
			evtInst->data.wave.baseFrequency = track->frequency;
		}

		/* For infinite loops with no variation, let Wave do the work */
		if (evt->loopCount == 255 && !(evt->wave.variationFlags & 0x0F00))
		{
			evtInst->data.wave.wave->loopCount = 255;
			evtInst->loopCount = 0;
		}

		/* Play, finally. */
		FACTWave_Play(evtInst->data.wave.wave);
	}

	/* SETVALUE */
	else if (	evt->type == FACTEVENT_PITCH ||
			evt->type == FACTEVENT_PITCHREPEATING ||
			evt->type == FACTEVENT_VOLUME ||
			evt->type == FACTEVENT_VOLUMEREPEATING	)
	{
		/* Ramp/Equation */
		if (evt->value.settings & 0x01)
		{
			/* FIXME: Incorporate 2nd derivative into the interpolated pitch */
			skipLoopCheck = elapsed <= (evtInst->timestamp + evt->value.ramp.duration);
			svResult = (
				evt->value.ramp.initialSlope *
				evt->value.ramp.duration *
				10 /* "Slices" */
			) + evt->value.ramp.initialValue;
			svResult = (
				evt->value.ramp.initialValue +
				(svResult - evt->value.ramp.initialValue)
			) * FACT_clamp(
				(elapsed - evtInst->timestamp) / evt->value.ramp.duration,
				0.0f,
				1.0f
			);
		}
		else
		{
			/* Value/Random */
			if (evt->value.equation.flags & 0x04)
			{
				svResult = evt->value.equation.value1;
			}
			else if (evt->value.equation.flags & 0x08)
			{
				svResult = evt->value.equation.value1 + FACT_rng() * (
					evt->value.equation.value2 -
					evt->value.equation.value1
				);
			}
			else
			{
				svResult = 0.0f;
				FACT_assert(0 && "Equation flags?");
			}

			/* Add/Replace */
			if (evt->value.equation.flags & 0x01)
			{
				evtInst->data.value += svResult;
			}
			else
			{
				evtInst->data.value = svResult;
			}
		}

		/* Set the result, finally. */
		if (	evt->type == FACTEVENT_PITCH ||
			evt->type == FACTEVENT_PITCHREPEATING	)
		{
			/* TODO: FACT_INTERNAL_SetPitch(evt->value*) */
		}
		else
		{
			/* TODO: FACT_INTERNAL_SetVolume(evt->value*) */
		}

		if (skipLoopCheck)
		{
			return;
		}
	}

	/* MARKER */
	else if (	evt->type == FACTEVENT_MARKER ||
			evt->type == FACTEVENT_MARKERREPEATING	)
	{
		/* TODO: FACT_INTERNAL_Marker(evt->marker*) */
	}

	/* ??? */
	else
	{
		FACT_assert(0 && "Unknown event type!");
	}

	/* Either loop or mark this event as complete */
	if (evtInst->loopCount > 0)
	{
		if (evtInst->loopCount != 0xFF)
		{
			evtInst->loopCount -= 1;
		}

		evtInst->timestamp += evt->timestamp;
	}
	else
	{
		evtInst->finished = 1;
	}
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
			if (!(engine->variables[engine->rpcs[i].variable].accessibility & 0x04))
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

void FACT_INTERNAL_UpdateCue(FACTCue *cue, uint32_t elapsed)
{
	uint8_t i, j;
	uint32_t waveState;
	FACTSoundInstance *active;
	FACTEvent *evt;
	FACTEventInstance *evtInst;
	uint8_t finished = 1;

	/* If we're not running, save some instructions... */
	if (cue->state & (FACT_STATE_PAUSED | FACT_STATE_STOPPED))
	{
		return;
	}

	/* There's only something to do if we're a Sound. Waves are simple! */
	if (cue->active & 0x01)
	{
		/* TODO: FadeIn/FadeOut? */
		cue->state = cue->playing.wave->state;
		return;
	}
	else if (!cue->active)
	{
		return;
	}

	/* To get the time on a single Cue, subtract from the global time
	 * the latest start time minus the total time elapsed (minus pause time)
	 */
	elapsed -= cue->start - cue->elapsed;

	/* FIXME: Multiple sounds may exist for interactive Cues? */
	active = &cue->playing.sound;

	/* TODO: Interactive Cues, will set `active` based on variable */

	/* RPC updates */
	FACT_INTERNAL_UpdateRPCs(
		cue,
		active->sound->rpcCodeCount,
		active->sound->rpcCodes,
		&active->rpcData
	);
	for (i = 0; i < active->sound->trackCount; i += 1)
	{
		FACT_INTERNAL_UpdateRPCs(
			cue,
			active->sound->tracks[i].rpcCodeCount,
			active->sound->tracks[i].rpcCodes,
			&active->tracks[i].rpcData
		);
	}

	/* Go through each event for each track */
	for (i = 0; i < active->sound->trackCount; i += 1)
	for (j = 0; j < active->sound->tracks[i].eventCount; j += 1)
	{
		evt = &active->sound->tracks[i].events[j];
		evtInst = &active->tracks[i].events[j];

		if (!evtInst->finished)
		{
			/* Cue's not done yet...! */
			finished = 0;

			/* Trigger events at the right time */
			if (elapsed > evtInst->timestamp)
			{
				FACT_INTERNAL_ActivateEvent(
					cue,
					active->sound,
					&active->sound->tracks[i],
					&active->tracks[i],
					evt,
					evtInst,
					elapsed
				);
			}
		}

		/* Wave updates */
		if (	evt->type == FACTEVENT_PLAYWAVE ||
			evt->type == FACTEVENT_PLAYWAVETRACKVARIATION ||
			evt->type == FACTEVENT_PLAYWAVEEFFECTVARIATION ||
			evt->type == FACTEVENT_PLAYWAVETRACKEFFECTVARIATION	)
		{
			if (evtInst->data.wave.wave == NULL)
			{
				continue;
			}
			finished = 0;

			/* Clear out Waves as they finish */
			FACTWave_GetState(
				evtInst->data.wave.wave,
				&waveState
			);
			if (waveState & FACT_STATE_STOPPED)
			{
				FACTWave_Destroy(
					evtInst->data.wave.wave
				);
				evtInst->data.wave.wave = NULL;
				continue;
			}

			/* TODO: Event volume/pitch values */
			FACTWave_SetVolume(
				evtInst->data.wave.wave,
				FACT_INTERNAL_CalculateAmplitudeRatio(
					evtInst->data.wave.baseVolume +
					active->rpcData.rpcVolume +
					active->tracks[i].rpcData.rpcVolume
				) * cue->parentBank->parentEngine->categories[active->sound->category].currentVolume
			);
			FACTWave_SetPitch(
				evtInst->data.wave.wave,
				(
					evtInst->data.wave.basePitch +
					active->rpcData.rpcPitch +
					active->tracks[i].rpcData.rpcPitch
				)
			);
			/* TODO: Wave updates:
			 * - Filter (QFactor/Frequency)
			 * - Reverb
			 * - 3D
			 * - Fade in/out
			 */
		}
	}

	/* If everything has been played and finished, set STOPPED */
	if (finished)
	{
		cue->state |= FACT_STATE_STOPPED;
		cue->state &= ~(FACT_STATE_PLAYING | FACT_STATE_STOPPING);
	}
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

	/* Nothing to see here... */
	if (wave->state & (FACT_STATE_PAUSED | FACT_STATE_STOPPED))
	{
		return 0;
	}

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
	uint32_t end, len;

	/* Don't go past the end of the wave data */
	if (	wave->loopCount > 0 &&
		wave->parentBank->entries[wave->index].LoopRegion.dwTotalSamples != 0	)
	{
		end = wave->parentBank->entries[wave->index].LoopRegion.dwTotalSamples;
	}
	else
	{
		end = wave->parentBank->entries[wave->index].PlayRegion.dwLength;
	}
	len = FACT_min(end - wave->position, samples);

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

	/* Increment I/O offset, len now represents samples read */
	wave->position += len;

	/* Loop recursion. TODO: Notify Cue on loop! */
	if (len < samples && wave->loopCount > 0)
	{
		wave->position = wave->parentBank->entries[wave->index].LoopRegion.dwStartSample;
		wave->loopCount -= 1;
		return len + FACT_INTERNAL_DecodeMonoPCM8(
			wave,
			decodeCacheL + len,
			decodeCacheR + len,
			samples - len
		);
	}
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
	uint32_t end, len;

	/* Don't go past the end of the wave data */
	if (	wave->loopCount > 0 &&
		wave->parentBank->entries[wave->index].LoopRegion.dwTotalSamples != 0	)
	{
		end = wave->parentBank->entries[wave->index].LoopRegion.dwTotalSamples * 2;
	}
	else
	{
		end = wave->parentBank->entries[wave->index].PlayRegion.dwLength;
	}
	len = FACT_min(end - wave->position, samples * 2);

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

	/* Increment I/O offset, len now represents samples read */
	wave->position += len;
	len >>= 1;

	/* Loop recursion. TODO: Notify Cue on loop! */
	if (len < samples && wave->loopCount > 0)
	{
		wave->position = wave->parentBank->entries[wave->index].LoopRegion.dwStartSample * 2;
		wave->loopCount -= 1;
		return len + FACT_INTERNAL_DecodeStereoPCM8(
			wave,
			decodeCacheL + len,
			decodeCacheR + len,
			samples - len
		);
	}

	return len;
}

uint32_t FACT_INTERNAL_DecodeStereoToMonoPCM8(
	FACTWave *wave,
	int16_t *decodeCacheL,
	int16_t *decodeCacheR,
	uint32_t samples
) {
	uint32_t i;
	int8_t sample[2];
	uint32_t end, len;

	/* Don't go past the end of the wave data */
	if (	wave->loopCount > 0 &&
		wave->parentBank->entries[wave->index].LoopRegion.dwTotalSamples != 0	)
	{
		end = wave->parentBank->entries[wave->index].LoopRegion.dwTotalSamples * 2;
	}
	else
	{
		end = wave->parentBank->entries[wave->index].PlayRegion.dwLength;
	}
	len = FACT_min(end - wave->position, samples * 2);

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

	/* Increment I/O offset, len now represents samples read */
	wave->position += len;
	len >>= 1;

	/* Loop recursion. TODO: Notify Cue on loop! */
	if (len < samples && wave->loopCount > 0)
	{
		wave->position = wave->parentBank->entries[wave->index].LoopRegion.dwStartSample * 2;
		wave->loopCount -= 1;
		return len + FACT_INTERNAL_DecodeStereoToMonoPCM8(
			wave,
			decodeCacheL + len,
			decodeCacheR + len,
			samples - len
		);
	}
	return len;
}

/* 16-bit PCM Decoding */

uint32_t FACT_INTERNAL_DecodeMonoPCM16(
	FACTWave *wave,
	int16_t *decodeCacheL,
	int16_t *decodeCacheR,
	uint32_t samples
) {
	uint32_t end, len;

	/* Don't go past the end of the wave data */
	if (	wave->loopCount > 0 &&
		wave->parentBank->entries[wave->index].LoopRegion.dwTotalSamples != 0	)
	{
		end = wave->parentBank->entries[wave->index].LoopRegion.dwTotalSamples * 2;
	}
	else
	{
		end = wave->parentBank->entries[wave->index].PlayRegion.dwLength;
	}
	len = FACT_min(end - wave->position, samples * 2);

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

	/* Increment I/O offset, len now represents samples read */
	wave->position += len;
	len >>= 1;

	/* Loop recursion. TODO: Notify Cue on loop! */
	if (len < samples && wave->loopCount > 0)
	{
		wave->position = wave->parentBank->entries[wave->index].LoopRegion.dwStartSample * 2;
		wave->loopCount -= 1;
		return len + FACT_INTERNAL_DecodeMonoPCM16(
			wave,
			decodeCacheL + len,
			decodeCacheR + len,
			samples - len
		);
	}
	return len;
}

uint32_t FACT_INTERNAL_DecodeStereoPCM16(
	FACTWave *wave,
	int16_t *decodeCacheL,
	int16_t *decodeCacheR,
	uint32_t samples
) {
	uint32_t i;
	uint32_t end, len;

	/* Don't go past the end of the wave data */
	if (	wave->loopCount > 0 &&
		wave->parentBank->entries[wave->index].LoopRegion.dwTotalSamples != 0	)
	{
		end = wave->parentBank->entries[wave->index].LoopRegion.dwTotalSamples * 4;
	}
	else
	{
		end = wave->parentBank->entries[wave->index].PlayRegion.dwLength;
	}
	len = FACT_min(end - wave->position, samples * 4);

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

	/* Increment I/O offset, len now represents samples read */
	wave->position += len;
	len >>= 2;

	/* Loop recursion. TODO: Notify Cue on loop! */
	if (len < samples && wave->loopCount > 0)
	{
		wave->position = wave->parentBank->entries[wave->index].LoopRegion.dwStartSample * 4;
		wave->loopCount -= 1;
		return len + FACT_INTERNAL_DecodeStereoPCM16(
			wave,
			decodeCacheL + len,
			decodeCacheR + len,
			samples - len
		);
	}
	return len;
}

uint32_t FACT_INTERNAL_DecodeStereoToMonoPCM16(
	FACTWave *wave,
	int16_t *decodeCacheL,
	int16_t *decodeCacheR,
	uint32_t samples
) {
	uint32_t i;
	uint32_t end, len;

	/* Don't go past the end of the wave data */
	if (	wave->loopCount > 0 &&
		wave->parentBank->entries[wave->index].LoopRegion.dwTotalSamples != 0	)
	{
		end = wave->parentBank->entries[wave->index].LoopRegion.dwTotalSamples * 4;
	}
	else
	{
		end = wave->parentBank->entries[wave->index].PlayRegion.dwLength;
	}
	len = FACT_min(end - wave->position, samples * 4);

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

	/* Increment I/O offset, len now represents samples read */
	wave->position += len;
	len >>= 2;

	/* Loop recursion. TODO: Notify Cue on loop! */
	if (len < samples && wave->loopCount > 0)
	{
		wave->position = wave->parentBank->entries[wave->index].LoopRegion.dwStartSample * 4;
		wave->loopCount -= 1;
		return len + FACT_INTERNAL_DecodeStereoToMonoPCM16(
			wave,
			decodeCacheL + len,
			decodeCacheR + len,
			samples - len
		);
	}
	return len;
}

/* MSADPCM Decoding */

static inline int16_t FACT_INTERNAL_ParseNibble(
	uint8_t nibble,
	uint8_t predictor,
	int16_t *delta,
	int16_t *sample1,
	int16_t *sample2
) {
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

	int8_t signedNibble;
	int32_t sampleInt;
	int16_t sample;

	signedNibble = (int8_t) nibble;
	if (signedNibble & 0x08)
	{
		signedNibble -= 0x10;
	}

	sampleInt = (
		(*sample1 * AdaptCoeff_1[predictor]) +
		(*sample2 * AdaptCoeff_2[predictor])
	) / 256;
	sampleInt += signedNibble * (*delta);
	sample = FACT_clamp(sampleInt, -32768, 32767);

	*sample2 = *sample1;
	*sample1 = sample;
	*delta = (int16_t) (AdaptionTable[nibble] * (int32_t) (*delta) / 256);
	if (*delta < 16)
	{
		*delta = 16;
	}
	return sample;
}

static inline void FACT_INTERNAL_ReadMonoPreamble(
	FACTIOStream *io,
	uint8_t *predictor,
	int16_t *delta,
	int16_t *sample1,
	int16_t *sample2
) {
	io->read(io->data, predictor,	sizeof(*predictor),	1);
	io->read(io->data, delta,	sizeof(*delta),		1);
	io->read(io->data, sample1,	sizeof(*sample1),	1);
	io->read(io->data, sample2,	sizeof(*sample2),	1);
}

static inline void FACT_INTERNAL_ReadStereoPreamble(
	FACTIOStream *io,
	uint8_t *predictor_l,
	uint8_t *predictor_r,
	int16_t *delta_l,
	int16_t *delta_r,
	int16_t *sample1_l,
	int16_t *sample1_r,
	int16_t *sample2_l,
	int16_t *sample2_r
) {
	io->read(io->data, predictor_l,	sizeof(*predictor_l),	1);
	io->read(io->data, predictor_r,	sizeof(*predictor_r),	1);
	io->read(io->data, delta_l,	sizeof(*delta_l),	1);
	io->read(io->data, delta_r,	sizeof(*delta_r),	1);
	io->read(io->data, sample1_l,	sizeof(*sample1_l),	1);
	io->read(io->data, sample1_r,	sizeof(*sample1_r),	1);
	io->read(io->data, sample2_l,	sizeof(*sample2_l),	1);
	io->read(io->data, sample2_r,	sizeof(*sample2_r),	1);
}

uint32_t FACT_INTERNAL_DecodeMonoMSADPCM(
	FACTWave *wave,
	int16_t *decodeCacheL,
	int16_t *decodeCacheR,
	uint32_t samples
) {
	/* Iterators */
	uint8_t b, i;

	/* Read sizes */
	uint32_t blocks, extra, end, len;

	/* Temp storage for ADPCM blocks */
	uint8_t predictor;
	int16_t delta;
	int16_t sample1;
	int16_t sample2;
	uint8_t nibbles[255]; /* Max align size */

	/* Keep decodeCache as-is to calculate return value */
	int16_t *pcm = decodeCacheL;
	int16_t *pcmExtra = wave->msadpcmCache;

	/* Align, block size */
	uint32_t align = wave->parentBank->entries[wave->index].Format.wBlockAlign;
	uint32_t bsize = (align + 16) * 2;

	/* Have extra? Throw it in! */
	if (wave->msadpcmExtra > 0)
	{
		FACT_memcpy(pcm, wave->msadpcmCache, wave->msadpcmExtra * 2);
		pcm += wave->msadpcmExtra;
		samples -= wave->msadpcmExtra;
		wave->msadpcmExtra = 0;
	}

	/* How many blocks do we need? */
	blocks = samples / bsize;
	extra = samples % bsize;

	/* Don't go past the end of the wave data */
	if (	wave->loopCount > 0 &&
		wave->parentBank->entries[wave->index].LoopRegion.dwTotalSamples != 0	)
	{
		end = wave->parentBank->entries[wave->index].LoopRegion.dwTotalSamples;
		end = end / bsize * (align + 22);
	}
	else
	{
		end = wave->parentBank->entries[wave->index].PlayRegion.dwLength;
	}
	len = FACT_min(
		end - wave->position,
		(blocks + (extra > 0)) * (align + 22)
	);

	/* len might be 0 if we just came back for tail samples */
	if (len == 0)
	{
		goto end;
	}

	/* Saturate len to be at least the size of a full block */
	if (len < ((blocks + (extra > 0)) * (align + 22)))
	{
		blocks = len / (align + 22);
		extra = len % (align + 22);
	}

	/* Go to the spot in the WaveBank where our samples start */
	wave->parentBank->io->seek(
		wave->parentBank->io->data,
		wave->parentBank->entries[wave->index].PlayRegion.dwOffset +
			wave->position,
		0
	);

	/* Read in each block directly to the decode cache */
	for (b = 0; b < blocks; b += 1)
	{
		FACT_INTERNAL_ReadMonoPreamble(
			wave->parentBank->io,
			&predictor,
			&delta,
			&sample1,
			&sample2
		);
		*pcm++ = sample2;
		*pcm++ = sample1;
		wave->parentBank->io->read(
			wave->parentBank->io->data,
			nibbles,
			align + 15,
			1
		);
		for (i = 0; i < (align + 15); i += 1)
		{
			*pcm++ = FACT_INTERNAL_ParseNibble(
				nibbles[i] >> 4,
				predictor,
				&delta,
				&sample1,
				&sample2
			);
			*pcm++ = FACT_INTERNAL_ParseNibble(
				nibbles[i] & 0x0F,
				predictor,
				&delta,
				&sample1,
				&sample2
			);
		}
	}

	/* Have extra? Go to the MSADPCM cache */
	if (extra > 0)
	{
		FACT_INTERNAL_ReadMonoPreamble(
			wave->parentBank->io,
			&predictor,
			&delta,
			&sample1,
			&sample2
		);
		*pcmExtra++ = sample2;
		*pcmExtra++ = sample1;
		wave->parentBank->io->read(
			wave->parentBank->io->data,
			nibbles,
			align + 15,
			1
		);
		for (i = 0; i < (align + 15); i += 1)
		{
			*pcmExtra++ = FACT_INTERNAL_ParseNibble(
				nibbles[i] >> 4,
				predictor,
				&delta,
				&sample1,
				&sample2
			);
			*pcmExtra++ = FACT_INTERNAL_ParseNibble(
				nibbles[i] & 0x0F,
				predictor,
				&delta,
				&sample1,
				&sample2
			);
		}
		wave->msadpcmExtra = bsize - extra;
		FACT_memcpy(pcm, wave->msadpcmCache, extra * 2);
		FACT_memmove(
			wave->msadpcmCache,
			wave->msadpcmCache + extra,
			wave->msadpcmExtra * 2
		);
		pcm += extra;
	}

	/* Increment I/O offset */
	wave->position += len;

end:
	/* len now represents samples read */
	len = (pcm - decodeCacheL);

	/* Loop recursion. TODO: Notify Cue on loop! */
	if (len < samples && wave->loopCount > 0)
	{
		wave->position = wave->parentBank->entries[wave->index].LoopRegion.dwStartSample;
		wave->position = wave->position / bsize * (align + 22);
		wave->loopCount -= 1;
		return len + FACT_INTERNAL_DecodeMonoMSADPCM(
			wave,
			decodeCacheL + len,
			decodeCacheR + len,
			samples - len
		);
	}
	return len;
}

uint32_t FACT_INTERNAL_DecodeStereoMSADPCM(
	FACTWave *wave,
	int16_t *decodeCacheL,
	int16_t *decodeCacheR,
	uint32_t samples
) {
	/* Iterators */
	uint8_t b, i;

	/* Read sizes */
	uint32_t blocks, extra, end, len;

	/* Temp storage for ADPCM blocks */
	uint8_t l_predictor;
	uint8_t r_predictor;
	int16_t l_delta;
	int16_t r_delta;
	int16_t l_sample1;
	int16_t r_sample1;
	int16_t l_sample2;
	int16_t r_sample2;
	uint8_t nibbles[510]; /* Max align size */

	/* Keep decodeCache as-is to calculate return value */
	int16_t *pcmL = decodeCacheL;
	int16_t *pcmR = decodeCacheR;
	int16_t *pcmExtra = wave->msadpcmCache;

	/* Align, block size */
	uint32_t align = wave->parentBank->entries[wave->index].Format.wBlockAlign;
	uint32_t bsize = (align + 16) * 2;

	/* Have extra? Throw it in! */
	if (wave->msadpcmExtra > 0)
	{
		for (i = 0; i < (wave->msadpcmExtra * 2); i += 2)
		{
			*pcmL++ = wave->msadpcmCache[i];
			*pcmR++ = wave->msadpcmCache[i + 1];
		}
		samples -= wave->msadpcmExtra;
		wave->msadpcmExtra = 0;
	}

	/* How many blocks do we need? */
	blocks = samples / bsize;
	extra = samples % bsize;

	/* Don't go past the end of the wave data */
	if (	wave->loopCount > 0 &&
		wave->parentBank->entries[wave->index].LoopRegion.dwTotalSamples != 0	)
	{
		end = wave->parentBank->entries[wave->index].LoopRegion.dwTotalSamples;
		end = end / bsize * ((align + 22) * 2);
	}
	else
	{
		end = wave->parentBank->entries[wave->index].PlayRegion.dwLength;
	}
	len = FACT_min(
		end - wave->position,
		(blocks + (extra > 0)) * ((align + 22) * 2)
	);

	/* len might be 0 if we just came back for tail samples */
	if (len == 0)
	{
		goto end;
	}

	/* Saturate len to be at least the size of a full block */
	if (len < ((blocks + (extra > 0)) * ((align + 22) * 2)))
	{
		blocks = len / ((align + 22) * 2);
		extra = len % ((align + 22) * 2);
	}

	/* Go to the spot in the WaveBank where our samples start */
	wave->parentBank->io->seek(
		wave->parentBank->io->data,
		wave->parentBank->entries[wave->index].PlayRegion.dwOffset +
			wave->position,
		0
	);

	/* Read in each block directly to the decode cache */
	for (b = 0; b < blocks; b += 1)
	{
		FACT_INTERNAL_ReadStereoPreamble(
			wave->parentBank->io,
			&l_predictor,
			&r_predictor,
			&l_delta,
			&r_delta,
			&l_sample1,
			&r_sample1,
			&l_sample2,
			&r_sample2
		);
		*pcmL++ = l_sample2;
		*pcmR++ = r_sample2;
		*pcmL++ = l_sample1;
		*pcmR++ = r_sample1;
		wave->parentBank->io->read(
			wave->parentBank->io->data,
			nibbles,
			(align + 15) * 2,
			1
		);
		for (i = 0; i < ((align + 15) * 2); i += 1)
		{
			*pcmL++ = FACT_INTERNAL_ParseNibble(
				nibbles[i] >> 4,
				l_predictor,
				&l_delta,
				&l_sample1,
				&l_sample2
			);
			*pcmR++ = FACT_INTERNAL_ParseNibble(
				nibbles[i] & 0x0F,
				r_predictor,
				&r_delta,
				&r_sample1,
				&r_sample2
			);
		}
	}

	/* Have extra? Go to the MSADPCM cache */
	if (extra > 0)
	{
		FACT_INTERNAL_ReadStereoPreamble(
			wave->parentBank->io,
			&l_predictor,
			&r_predictor,
			&l_delta,
			&r_delta,
			&l_sample1,
			&r_sample1,
			&l_sample2,
			&r_sample2
		);
		*pcmExtra++ = l_sample2;
		*pcmExtra++ = r_sample2;
		*pcmExtra++ = l_sample1;
		*pcmExtra++ = r_sample1;
		wave->parentBank->io->read(
			wave->parentBank->io->data,
			nibbles,
			(align + 15) * 2,
			1
		);
		for (i = 0; i < ((align + 15) * 2); i += 1)
		{
			*pcmExtra++ = FACT_INTERNAL_ParseNibble(
				nibbles[i] >> 4,
				l_predictor,
				&l_delta,
				&l_sample1,
				&l_sample2
			);
			*pcmExtra++ = FACT_INTERNAL_ParseNibble(
				nibbles[i] & 0x0F,
				r_predictor,
				&r_delta,
				&r_sample1,
				&r_sample2
			);
		}
		wave->msadpcmExtra = bsize - extra;
		for (i = 0; i < (extra * 2); i += 2)
		{
			*pcmL++ = wave->msadpcmCache[i];
			*pcmR++ = wave->msadpcmCache[i + 1];
		}
		FACT_memmove(
			wave->msadpcmCache,
			wave->msadpcmCache + (extra * 2),
			wave->msadpcmExtra * 2
		);
	}

	/* Increment I/O offset */
	wave->position += len;

end:
	/* len now represents samples read */
	len = (pcmL - decodeCacheL);

	/* Loop recursion. TODO: Notify Cue on loop! */
	if (len < samples && wave->loopCount > 0)
	{
		wave->position = wave->parentBank->entries[wave->index].LoopRegion.dwStartSample;
		wave->position = wave->position / bsize * ((align + 22) * 2);
		wave->loopCount -= 1;
		return len + FACT_INTERNAL_DecodeStereoMSADPCM(
			wave,
			decodeCacheL + len,
			decodeCacheR + len,
			samples - len
		);
	}
	return len;
}

uint32_t FACT_INTERNAL_DecodeStereoToMonoMSADPCM(
	FACTWave *wave,
	int16_t *decodeCacheL,
	int16_t *decodeCacheR,
	uint32_t samples
) {
	/* Iterators */
	uint8_t b, i;

	/* Read sizes */
	uint32_t blocks, extra, end, len;

	/* Temp storage for ADPCM blocks */
	uint8_t l_predictor;
	uint8_t r_predictor;
	int16_t l_delta;
	int16_t r_delta;
	int16_t l_sample1;
	int16_t r_sample1;
	int16_t l_sample2;
	int16_t r_sample2;
	uint8_t nibbles[510]; /* Max align size */

	/* Keep decodeCache as-is to calculate return value */
	int16_t *pcm = decodeCacheL;
	int16_t *pcmExtra = wave->msadpcmCache;

	/* Align, block size */
	uint32_t align = wave->parentBank->entries[wave->index].Format.wBlockAlign;
	uint32_t bsize = (align + 16) * 2;

	/* Have extra? Throw it in! */
	if (wave->msadpcmExtra > 0)
	{
		for (i = 0; i < (wave->msadpcmExtra * 2); i += 2)
		{
			*pcm++ = (int16_t) ((
				(int32_t) wave->msadpcmCache[i] +
				(int32_t) wave->msadpcmCache[i + 1]
			) / 2);
		}
		samples -= wave->msadpcmExtra;
		wave->msadpcmExtra = 0;
	}

	/* How many blocks do we need? */
	blocks = samples / bsize;
	extra = samples % bsize;

	/* Don't go past the end of the wave data */
	if (	wave->loopCount > 0 &&
		wave->parentBank->entries[wave->index].LoopRegion.dwTotalSamples != 0	)
	{
		end = wave->parentBank->entries[wave->index].LoopRegion.dwTotalSamples;
		end = end / bsize * ((align + 22) * 2);
	}
	else
	{
		end = wave->parentBank->entries[wave->index].PlayRegion.dwLength;
	}
	len = FACT_min(
		end - wave->position,
		(blocks + (extra > 0)) * ((align + 22) * 2)
	);

	/* len might be 0 if we just came back for tail samples */
	if (len == 0)
	{
		goto end;
	}

	/* Saturate len to be at least the size of a full block */
	if (len < ((blocks + (extra > 0)) * ((align + 22) * 2)))
	{
		blocks = len / ((align + 22) * 2);
		extra = len % ((align + 22) * 2);
	}

	/* Go to the spot in the WaveBank where our samples start */
	wave->parentBank->io->seek(
		wave->parentBank->io->data,
		wave->parentBank->entries[wave->index].PlayRegion.dwOffset +
			wave->position,
		0
	);

	/* Read in each block directly to the decode cache */
	for (b = 0; b < blocks; b += 1)
	{
		FACT_INTERNAL_ReadStereoPreamble(
			wave->parentBank->io,
			&l_predictor,
			&r_predictor,
			&l_delta,
			&r_delta,
			&l_sample1,
			&r_sample1,
			&l_sample2,
			&r_sample2
		);
		*pcm++ = (int16_t) ((
			(int32_t) l_sample2 + (int32_t) r_sample2
		) / 2);
		*pcm++ = (int16_t) ((
			(int32_t) l_sample1 + (int32_t) r_sample1
		) / 2);
		wave->parentBank->io->read(
			wave->parentBank->io->data,
			nibbles,
			(align + 15) * 2,
			1
		);
		for (i = 0; i < ((align + 15) * 2); i += 1)
		{
			decodeCacheR[0] = FACT_INTERNAL_ParseNibble(
				nibbles[i] >> 4,
				l_predictor,
				&l_delta,
				&l_sample1,
				&l_sample2
			);
			decodeCacheR[1] = FACT_INTERNAL_ParseNibble(
				nibbles[i] & 0x0F,
				r_predictor,
				&r_delta,
				&r_sample1,
				&r_sample2
			);
			*pcm++ = (int16_t) ((
				(int32_t) decodeCacheR[0] +
				(int32_t) decodeCacheR[1]
			) / 2);
		}
	}

	/* Have extra? Go to the MSADPCM cache */
	if (extra > 0)
	{
		FACT_INTERNAL_ReadStereoPreamble(
			wave->parentBank->io,
			&l_predictor,
			&r_predictor,
			&l_delta,
			&r_delta,
			&l_sample1,
			&r_sample1,
			&l_sample2,
			&r_sample2
		);
		*pcmExtra++ = l_sample2;
		*pcmExtra++ = r_sample2;
		*pcmExtra++ = l_sample1;
		*pcmExtra++ = r_sample1;
		wave->parentBank->io->read(
			wave->parentBank->io->data,
			nibbles,
			(align + 15) * 2,
			1
		);
		for (i = 0; i < ((align + 15) * 2); i += 1)
		{
			*pcmExtra++ = FACT_INTERNAL_ParseNibble(
				nibbles[i] >> 4,
				l_predictor,
				&l_delta,
				&l_sample1,
				&l_sample2
			);
			*pcmExtra++ = FACT_INTERNAL_ParseNibble(
				nibbles[i] & 0x0F,
				r_predictor,
				&r_delta,
				&r_sample1,
				&r_sample2
			);
		}
		wave->msadpcmExtra = bsize - extra;
		for (i = 0; i < (extra * 2); i += 2)
		{
			*pcm++ = (int16_t) ((
				(int32_t) wave->msadpcmCache[i] +
				(int32_t) wave->msadpcmCache[i + 1]
			) / 2);
		}
		FACT_memmove(
			wave->msadpcmCache,
			wave->msadpcmCache + (extra * 2),
			wave->msadpcmExtra * 2
		);
	}

	/* Increment I/O offset */
	wave->position += len;

end:
	/* len now represents samples read */
	len = (pcm - decodeCacheL);

	/* Loop recursion. TODO: Notify Cue on loop! */
	if (len < samples && wave->loopCount > 0)
	{
		wave->position = wave->parentBank->entries[wave->index].LoopRegion.dwStartSample;
		wave->position = wave->position / bsize * (align + 22);
		wave->loopCount -= 1;
		return len + FACT_INTERNAL_DecodeStereoToMonoMSADPCM(
			wave,
			decodeCacheL + len,
			decodeCacheR + len,
			samples - len
		);
	}
	return len;
}
