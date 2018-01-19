/* FACT - XACT Reimplementation for FNA
 * Copyright 2009-2018 Ethan Lee and the MonoGame Team
 *
 * Released under the Microsoft Public License.
 * See LICENSE for details.
 */

#include "FACT_internal.h"

/* FIXME: What is even inside Instance, wtf -flibit */
#define INSTANCE_SPEAKERMASK \
	*((uint32_t*) &Instance[0])
#define INSTANCE_SPEEDOFSOUND \
	*((float*) &Instance[sizeof(uint32_t)])

void FACT3DAudioInitialize(
	uint32_t SpeakerChannelMask,
	float SpeedOfSound,
	FACT3DAUDIO_HANDLE Instance
) {
	/* FIXME: What is even inside Instance, wtf -flibit */
	INSTANCE_SPEAKERMASK = SpeakerChannelMask;
	INSTANCE_SPEEDOFSOUND = SpeedOfSound;
}

void FACT3DAudioCalculate(
	const FACT3DAUDIO_HANDLE Instance,
	const FACT3DAUDIO_LISTENER *pListener,
	const FACT3DAUDIO_EMITTER *pEmitter,
	uint32_t Flags,
	FACT3DAUDIO_DSP_SETTINGS *pDSPSettings
) {
	FACT3DAUDIO_VECTOR emitterToListener;
	float scaledSpeedOfSound;
	float projectedListenerVelocity, projectedEmitterVelocity;
#if 0 /* TODO: FACT3DAUDIO_CALCULATE_MATRIX */
	uint32_t i;
	float *matrix = pDSPSettings->pMatrixCoefficients;
#endif

	/* Distance */
	emitterToListener.x = pListener->Position.x - pEmitter->Position.x;
	emitterToListener.y = pListener->Position.y - pEmitter->Position.y;
	emitterToListener.z = pListener->Position.z - pEmitter->Position.z;
	pDSPSettings->EmitterToListenerDistance = (float) FACT_sqrt(
		(emitterToListener.x * emitterToListener.x) +
		(emitterToListener.y * emitterToListener.y) +
		(emitterToListener.z * emitterToListener.z)
	);

	/* MatrixCoefficients */
	if (Flags & FACT3DAUDIO_CALCULATE_MATRIX)
	{
#if 0 /* TODO: Combine speaker azimuths with emitter/listener vector blah */
		for (i = 0; i < pDSPSettings->SrcChannelCount; i += 1)
		{
			#define SPEAKER(pos) \
				if (INSTANCE_SPEAKERMASK & SPEAKER_##pos) \
				{ \
					*matrix++ = pos##_AZIMUTH; \
				}
			#define SIDE_LEFT_AZIMUTH LEFT_AZIMUTH
			#define SIDE_RIGHT_AZIMUTH RIGHT_AZIMUTH
			SPEAKER(FRONT_LEFT)
			SPEAKER(FRONT_RIGHT)
			/* TODO: FACT3DAUDIO_CALCULATE_ZEROCENTER */
			SPEAKER(FRONT_CENTER)
			/* TODO: FACT3DAUDIO_CALCULATE_REDIRECT_TO_LFE */
			SPEAKER(LOW_FREQUENCY)
			SPEAKER(BACK_LEFT)
			SPEAKER(BACK_RIGHT)
			SPEAKER(FRONT_LEFT_OF_CENTER)
			SPEAKER(FRONT_RIGHT_OF_CENTER)
			SPEAKER(BACK_CENTER)
			SPEAKER(SIDE_LEFT)
			SPEAKER(SIDE_RIGHT)
			#undef SPEAKER
			#undef SIDE_LEFT_AZIMUTH
			#undef SIDE_RIGHT_AZIMUTH
			/* TODO: SPEAKER_TOP_*, Atmos support...? */
		}
#endif
	}

	/* TODO: FACT3DAUDIO_CALCULATE_DELAY */
	/* TODO: FACT3DAUDIO_CALCULATE_LPF_DIRECT */
	/* TODO: FACT3DAUDIO_CALCULATE_LPF_REVERB */
	/* TODO: FACT3DAUDIO_CALCULATE_REVERB */

	/* DopplerPitchScalar
	 * Adapted from algorithm published as a part of the webaudio specification:
	 * https://dvcs.w3.org/hg/audio/raw-file/tip/webaudio/specification.html#Spatialization-doppler-shift
	 * -Chad
	 */
	if (Flags & FACT3DAUDIO_CALCULATE_DOPPLER)
	{
		pDSPSettings->DopplerFactor = 1.0f;
		if (pEmitter->DopplerScaler > 0.0f)
		{
			scaledSpeedOfSound = INSTANCE_SPEEDOFSOUND / pEmitter->DopplerScaler;

			projectedListenerVelocity = (
				(emitterToListener.x * pListener->Velocity.x) +
				(emitterToListener.y * pListener->Velocity.y) +
				(emitterToListener.z * pListener->Velocity.z)
			) / pDSPSettings->EmitterToListenerDistance;
			projectedEmitterVelocity = (
				(emitterToListener.x * pEmitter->Velocity.x) +
				(emitterToListener.y * pEmitter->Velocity.y) +
				(emitterToListener.z * pEmitter->Velocity.z)
			) / pDSPSettings->EmitterToListenerDistance;

			projectedListenerVelocity = FACT_min(
				projectedListenerVelocity,
				scaledSpeedOfSound
			);
			projectedEmitterVelocity = FACT_min(
				projectedEmitterVelocity,
				scaledSpeedOfSound
			);

			pDSPSettings->DopplerFactor = (
				INSTANCE_SPEEDOFSOUND - pEmitter->DopplerScaler * projectedListenerVelocity
			) / (
				INSTANCE_SPEEDOFSOUND - pEmitter->DopplerScaler * projectedEmitterVelocity
			);
			/* FIXME: Check isnan(DopplerFactor) */

			/* Limit the pitch shifting to 2 octaves up and 1 octave down */
			pDSPSettings->DopplerFactor = FACT_clamp(
				pDSPSettings->DopplerFactor,
				0.5f,
				4.0f
			);
		}

		/* TODO: EmitterVelocityComponent */
		/* TODO: ListenerVelocityComponent */
	}

	/* OrientationAngle */
	if (Flags & FACT3DAUDIO_CALCULATE_EMITTER_ANGLE)
	{
		emitterToListener.x /= pDSPSettings->EmitterToListenerDistance;
		emitterToListener.y /= pDSPSettings->EmitterToListenerDistance;
		emitterToListener.z /= pDSPSettings->EmitterToListenerDistance;
		pDSPSettings->EmitterToListenerAngle = (float) FACT_acos(
			(emitterToListener.x * pListener->OrientFront.x) +
			(emitterToListener.y * pListener->OrientFront.y) +
			(emitterToListener.z * pListener->OrientFront.z)
		);
	}
}

uint32_t FACT3DInitialize(
	FACTAudioEngine *pEngine,
	FACT3DAUDIO_HANDLE F3DInstance
) {
	float nSpeedOfSound;
	FACTWaveFormatExtensible wfxFinalMixFormat;

	if (pEngine == NULL)
	{
		return 0;
	}

	FACTAudioEngine_GetGlobalVariable(
		pEngine,
		FACTAudioEngine_GetGlobalVariableIndex(
			pEngine,
			"SpeedOfSound"
		),
		&nSpeedOfSound
	);
	FACTAudioEngine_GetFinalMixFormat(
		pEngine,
		&wfxFinalMixFormat
	);
	FACT3DAudioInitialize(
		wfxFinalMixFormat.dwChannelMask,
		nSpeedOfSound,
		F3DInstance
	);
	return 0;
}

uint32_t FACT3DCalculate(
	FACT3DAUDIO_HANDLE F3DInstance,
	const FACT3DAUDIO_LISTENER *pListener,
	FACT3DAUDIO_EMITTER *pEmitter,
	FACT3DAUDIO_DSP_SETTINGS *pDSPSettings
) {
	static FACT3DAUDIO_DISTANCE_CURVE_POINT DefaultCurvePoints[2] =
	{
		{ 0.0f, 1.0f },
		{ 1.0f, 1.0f }
	};
	static FACT3DAUDIO_DISTANCE_CURVE DefaultCurve =
	{
		(FACT3DAUDIO_DISTANCE_CURVE_POINT*) &DefaultCurvePoints[0], 2
	};

	if (pListener == NULL || pEmitter == NULL || pDSPSettings == NULL)
	{
		return 0;
	}

	if (pEmitter->ChannelCount > 1 && pEmitter->pChannelAzimuths == NULL)
	{
		pEmitter->ChannelRadius = 1.0f;

		if (pEmitter->ChannelCount == 2)
		{
			pEmitter->pChannelAzimuths = (float*) &aStereoLayout[0];
		}
		else if (pEmitter->ChannelCount == 3)
		{
			pEmitter->pChannelAzimuths = (float*) &a2Point1Layout[0];
		}
		else if (pEmitter->ChannelCount == 4)
		{
			pEmitter->pChannelAzimuths = (float*) &aQuadLayout[0];
		}
		else if (pEmitter->ChannelCount == 5)
		{
			pEmitter->pChannelAzimuths = (float*) &a4Point1Layout[0];
		}
		else if (pEmitter->ChannelCount == 6)
		{
			pEmitter->pChannelAzimuths = (float*) &a5Point1Layout[0];
		}
		else if (pEmitter->ChannelCount == 8)
		{
			pEmitter->pChannelAzimuths = (float*) &a7Point1Layout[0];
		}
		else
		{
			return 0;
		}
	}

	if (pEmitter->pVolumeCurve == NULL)
	{
		pEmitter->pVolumeCurve = &DefaultCurve;
	}
	if (pEmitter->pLFECurve == NULL)
	{
		pEmitter->pLFECurve = &DefaultCurve;
	}

	FACT3DAudioCalculate(
		F3DInstance,
		pListener,
		pEmitter,
		(
			FACT3DAUDIO_CALCULATE_MATRIX |
			FACT3DAUDIO_CALCULATE_DOPPLER |
			FACT3DAUDIO_CALCULATE_EMITTER_ANGLE
		),
		pDSPSettings
	);
	return 0;
}

uint32_t FACT3DApply(
	FACT3DAUDIO_DSP_SETTINGS *pDSPSettings,
	FACTCue *pCue
) {
	if (pDSPSettings == NULL || pCue == NULL)
	{
		return 0;
	}

	FACTCue_SetMatrixCoefficients(
		pCue,
		pDSPSettings->SrcChannelCount,
		pDSPSettings->DstChannelCount,
		pDSPSettings->pMatrixCoefficients
	);
	FACTCue_SetVariable(
		pCue,
		FACTCue_GetVariableIndex(pCue, "Distance"),
		pDSPSettings->EmitterToListenerDistance
	);
	FACTCue_SetVariable(
		pCue,
		FACTCue_GetVariableIndex(pCue, "DopplerPitchScalar"),
		pDSPSettings->DopplerFactor
	);
	FACTCue_SetVariable(
		pCue,
		FACTCue_GetVariableIndex(pCue, "OrientationAngle"),
		pDSPSettings->EmitterToListenerAngle * (180.0f / FACT3DAUDIO_PI)
	);
	return 0;
}
