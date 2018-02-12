/* FACT - XACT Reimplementation for FNA
 * Copyright 2009-2018 Ethan Lee and the MonoGame Team
 *
 * Released under the Microsoft Public License.
 * See LICENSE for details.
 */

#include "FACT_internal.h"

/* RNG (TODO) */

float FACT_INTERNAL_rng()
{
	static float butt = 0.0f;
	float result = butt;
	butt += 0.2;
	if (butt > 1.0f)
	{
		butt = 0.0f;
	}
	return result;
}

/* Internal Functions */

float FACT_INTERNAL_CalculateAmplitudeRatio(float decibel)
{
	return FAudio_pow(10.0, decibel / 2000.0);
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
			next = FACT_INTERNAL_rng() * max;

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
				if (FAudio_strcmp(wbName, wb->name) == 0)
				{
					break;
				}
				wb = wb->next;
			}
			FAudio_assert(wb != NULL);

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
		cue->playing.sound.tracks = (FACTTrackInstance*) FAudio_malloc(
			sizeof(FACTTrackInstance) * cue->playing.sound.sound->trackCount
		);
		for (i = 0; i < cue->playing.sound.sound->trackCount; i += 1)
		{
			cue->playing.sound.tracks[i].events = (FACTEventInstance*) FAudio_malloc(
				sizeof(FACTEventInstance) * cue->playing.sound.sound->tracks[i].eventCount
			);
			for (j = 0; j < cue->playing.sound.sound->tracks[i].eventCount; j += 1)
			{
				cue->playing.sound.tracks[i].events[j].timestamp =
					cue->playing.sound.sound->tracks[i].events[j].timestamp;
				cue->playing.sound.tracks[i].events[j].loopCount =
					cue->playing.sound.sound->tracks[i].events[j].loopCount;
				cue->playing.sound.tracks[i].events[j].finished = 0;

				FAudio_zero(
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

/* RPC Helper Functions */

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

	FAudio_assert(0 && "RPC code not found!");
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
				if (FAudio_strcmp(
					engine->variableNames[rpc->variable],
					"AttackTime"
				) == 0) {
					/* TODO: AttackTime */
					rpcResult = 0.0f;
				}
				else if (FAudio_strcmp(
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
				FAudio_assert(0 && "Unhandled RPC parameter type!");
			}
		}
	}
}

/* Engine Update Function */

void FACT_INTERNAL_UpdateEngine(FACTAudioEngine *engine)
{
	uint16_t i, j, par;
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
					par = engine->rpcs[i].parameter - RPC_PARAMETER_COUNT;
					engine->dspPresets[j].parameters[par].value = FAudio_clamp(
						FACT_INTERNAL_CalculateRPC(
							&engine->rpcs[i],
							engine->globalVariableValues[
								engine->rpcs[i].variable
							]
						),
						engine->dspPresets[j].parameters[par].minVal,
						engine->dspPresets[j].parameters[par].maxVal
					);
				}
			}
		}
	}
}

/* Cue Update Functions */

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
			if (FAudio_strcmp(wbName, wb->name) == 0)
			{
				break;
			}
			wb = wb->next;
		}
		FAudio_assert(wb != NULL);

		/* Generate the wave... */
		FACTWaveBank_Prepare(
			wb,
			wbTrack,
			evt->wave.flags,
			0,
			0, /* FIXME: evtInst->loopCount? */
			&evtInst->data.wave.wave
		);
		evtInst->data.wave.wave->callback.cue = cue;
		evtInst->data.wave.wave->callback.event = evt;
		evtInst->data.wave.wave->callback.eventInstance = evtInst;

		/* 3D Audio */
		if (cue->active3D)
		{
			FACTWave_SetMatrixCoefficients(
				evtInst->data.wave.wave,
				cue->srcChannels,
				cue->dstChannels,
				cue->matrixCoefficients
			);
		}
		else
		{
			/* TODO: Position/Angle/UseCenterSpeaker */
		}

		/* Pitch Variation */
		if (evt->wave.variationFlags & 0x1000)
		{
			const int16_t rngPitch = (int16_t) (
				FACT_INTERNAL_rng() *
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
				FACT_INTERNAL_rng() *
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
				FACT_INTERNAL_rng() *
				(evt->wave.maxQFactor - evt->wave.minQFactor)
			);
			const float rngFrequency = (
				FACT_INTERNAL_rng() *
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
			) * FAudio_clamp(
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
				svResult = evt->value.equation.value1 + FACT_INTERNAL_rng() * (
					evt->value.equation.value2 -
					evt->value.equation.value1
				);
			}
			else
			{
				svResult = 0.0f;
				FAudio_assert(0 && "Equation flags?");
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
		FAudio_assert(0 && "Unknown event type!");
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
			 * - Fade in/out
			 */
		}
	}

	/* If everything has been played and finished, set STOPPED */
	if (finished)
	{
		cue->state |= FACT_STATE_STOPPED;
		cue->state &= ~(FACT_STATE_PLAYING | FACT_STATE_STOPPING);

		cue->parentBank->parentEngine->categories[
			active->sound->category
		].instanceCount -= 1;
		cue->data->instanceCount -= 1;
	}
}

/* FAudio callbacks */

void FACT_INTERNAL_OnProcessingPassStart(FAudioEngineCallback *callback)
{
	FACTAudioEngineCallback *c = (FACTAudioEngineCallback*) callback;
	FACTAudioEngine *engine = c->engine;
	FACTSoundBank *sb;
	FACTCue *cue, *backup;
	uint32_t timestamp;

	/* We want the timestamp to be uniform across all Cues.
	 * Oftentimes many Cues are played at once with the expectation
	 * that they will sync, so give them all the same timestamp
	 * so all the various actions will go together even if it takes
	 * an extra millisecond to get through the whole Cue list.
	 */
	timestamp = FAudio_timems();

	FACT_INTERNAL_UpdateEngine(engine);

	sb = engine->sbList;
	while (sb != NULL)
	{
		cue = sb->cueList;
		while (cue != NULL)
		{
			FACT_INTERNAL_UpdateCue(cue, timestamp);

			/* Destroy if it's done and not user-handled. */
			if (cue->managed && (cue->state & FACT_STATE_STOPPED))
			{
				backup = cue->next;
				FACTCue_Destroy(cue);
				cue = backup;
			}
			else
			{
				cue = cue->next;
			}
		}
		sb = sb->next;
	}
}

void FACT_INTERNAL_OnBufferEnd(FAudioVoiceCallback *callback, void* pContext)
{
	/* TODO: FACTWaveCallback *c = (FACTWaveCallback*) callback; */
}

void FACT_INTERNAL_OnStreamEnd(FAudioVoiceCallback *callback)
{
	/* TODO: FACTWaveCallback *c = (FACTWaveCallback*) callback; */
}
