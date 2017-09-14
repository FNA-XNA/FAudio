/* FACT - XACT Reimplementation for FNA
 * Copyright 2009-2017 Ethan Lee and the MonoGame Team
 *
 * Released under the Microsoft Public License.
 * See LICENSE for details.
 */

#include "FACT_internal.h"

/* Various internal math functions */

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

	/* TODO: Interactive Cues */

	/* Trigger events for each track */
	if (cue->soundInstance.exists)
	{
		for (i = 0; i < cue->active.sound->clipCount; i += 1)
		for (j = 0; i < cue->active.sound->clips[i].eventCount; j += 1)
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
	}
	else
	{
		/* TODO: Simple variation fake PlayWaveEvent */
	}

	/* TODO: Clear out Waves as they finish */

	/* TODO: Fade in/out */

	/* TODO: If everything has been played and finished, set STOPPED */

	/* TODO: RPC updates */

	/* TODO: Wave updates */

	/* Finally. */
	return 0;
}

void FACT_INTERNAL_MixWave(FACTWave *wave, uint8_t *stream, uint32_t len)
{
	/* TODO */
}
