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

#include "F3DAudio.h"
#include "FAudio_internal.h"

/* FIXME: What is even inside Instance, wtf -flibit */
#define INSTANCE_SPEAKERMASK \
	*((uint32_t*) &Instance[0])
#define INSTANCE_SPEEDOFSOUND \
	*((float*) &Instance[sizeof(uint32_t)])

void F3DAudioInitialize(
	uint32_t SpeakerChannelMask,
	float SpeedOfSound,
	F3DAUDIO_HANDLE Instance
) {
	/* FIXME: What is even inside Instance, wtf -flibit */
	INSTANCE_SPEAKERMASK = SpeakerChannelMask;
	INSTANCE_SPEEDOFSOUND = SpeedOfSound;
}

void F3DAudioCalculate(
	const F3DAUDIO_HANDLE Instance,
	const F3DAUDIO_LISTENER *pListener,
	const F3DAUDIO_EMITTER *pEmitter,
	uint32_t Flags,
	F3DAUDIO_DSP_SETTINGS *pDSPSettings
) {
	F3DAUDIO_VECTOR emitterToListener;
	float scaledSpeedOfSound;
	float projectedListenerVelocity, projectedEmitterVelocity;
#if 0 /* TODO: F3DAUDIO_CALCULATE_MATRIX */
	uint32_t i;
	float *matrix = pDSPSettings->pMatrixCoefficients;
#endif

	/* Distance */
	emitterToListener.x = pListener->Position.x - pEmitter->Position.x;
	emitterToListener.y = pListener->Position.y - pEmitter->Position.y;
	emitterToListener.z = pListener->Position.z - pEmitter->Position.z;
	pDSPSettings->EmitterToListenerDistance = (float) FAudio_sqrt(
		(emitterToListener.x * emitterToListener.x) +
		(emitterToListener.y * emitterToListener.y) +
		(emitterToListener.z * emitterToListener.z)
	);

	/* MatrixCoefficients */
	if (Flags & F3DAUDIO_CALCULATE_MATRIX)
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
			/* TODO: F3DAUDIO_CALCULATE_ZEROCENTER */
			SPEAKER(FRONT_CENTER)
			/* TODO: F3DAUDIO_CALCULATE_REDIRECT_TO_LFE */
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

	/* TODO: F3DAUDIO_CALCULATE_DELAY */
	/* TODO: F3DAUDIO_CALCULATE_LPF_DIRECT */
	/* TODO: F3DAUDIO_CALCULATE_LPF_REVERB */
	/* TODO: F3DAUDIO_CALCULATE_REVERB */

	/* DopplerPitchScalar
	 * Adapted from algorithm published as a part of the webaudio specification:
	 * https://dvcs.w3.org/hg/audio/raw-file/tip/webaudio/specification.html#Spatialization-doppler-shift
	 * -Chad
	 */
	if (Flags & F3DAUDIO_CALCULATE_DOPPLER)
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

			projectedListenerVelocity = FAudio_min(
				projectedListenerVelocity,
				scaledSpeedOfSound
			);
			projectedEmitterVelocity = FAudio_min(
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
			pDSPSettings->DopplerFactor = FAudio_clamp(
				pDSPSettings->DopplerFactor,
				0.5f,
				4.0f
			);
		}

		/* TODO: EmitterVelocityComponent */
		/* TODO: ListenerVelocityComponent */
	}

	/* OrientationAngle */
	if (Flags & F3DAUDIO_CALCULATE_EMITTER_ANGLE)
	{
		emitterToListener.x /= pDSPSettings->EmitterToListenerDistance;
		emitterToListener.y /= pDSPSettings->EmitterToListenerDistance;
		emitterToListener.z /= pDSPSettings->EmitterToListenerDistance;
		pDSPSettings->EmitterToListenerAngle = (float) FAudio_acos(
			(emitterToListener.x * pListener->OrientFront.x) +
			(emitterToListener.y * pListener->OrientFront.y) +
			(emitterToListener.z * pListener->OrientFront.z)
		);
	}
}
