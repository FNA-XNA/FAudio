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

#include <math.h> /* ONLY USE THIS FOR isnan! */
#include <float.h> /* _isnan for MSVC 2010 */
#if defined(_MSC_VER) && !defined(isnan)
#define isnan(x) _isnan(x)
#endif

/* F3DAUDIO_HANDLE Structure */
#define INSTANCE_SPEAKERMASK		*((uint32_t*)	&Instance[0])
#define INSTANCE_SPEAKERCOUNT		*((uint32_t*)	&Instance[4])
#define INSTANCE_UNKNOWN1		*((uint32_t*)	&Instance[8])
#define INSTANCE_SPEEDOFSOUND		*((float*)	&Instance[12])
#define INSTANCE_SPEEDOFSOUNDEPSILON	*((float*)	&Instance[16])

void F3DAudioInitialize(
	uint32_t SpeakerChannelMask,
	float SpeedOfSound,
	F3DAUDIO_HANDLE Instance
) {
	union
	{
		float f;
		uint32_t i;
	} epsilonHack;
	uint8_t speakerCount = 0;
	INSTANCE_SPEAKERMASK = SpeakerChannelMask;
	while (SpeakerChannelMask != 0)
	{
		speakerCount += SpeakerChannelMask & 1;
		SpeakerChannelMask >>= 1;
	}
	INSTANCE_SPEAKERCOUNT = speakerCount;
	INSTANCE_UNKNOWN1 = 0xFFFFFFFF; /* lolwut */
	INSTANCE_SPEEDOFSOUND = SpeedOfSound;

	/* "Convert" raw float to int... */
	epsilonHack.f = SpeedOfSound;
	/* ... Subtract epsilon value... */
	epsilonHack.i -= 1;
	/* ... Convert back to float. */
	INSTANCE_SPEEDOFSOUNDEPSILON = epsilonHack.f;
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
		/* TODO: F3DAUDIO_CALCULATE_MATRIX */
		FAudio_INTERNAL_SetDefaultMatrix(
			pDSPSettings->pMatrixCoefficients,
			pDSPSettings->SrcChannelCount,
			pDSPSettings->DstChannelCount
		);
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

			/* Project... */
			pDSPSettings->ListenerVelocityComponent = (
				(emitterToListener.x * pListener->Velocity.x) +
				(emitterToListener.y * pListener->Velocity.y) +
				(emitterToListener.z * pListener->Velocity.z)
			) / pDSPSettings->EmitterToListenerDistance;
			pDSPSettings->EmitterVelocityComponent = (
				(emitterToListener.x * pEmitter->Velocity.x) +
				(emitterToListener.y * pEmitter->Velocity.y) +
				(emitterToListener.z * pEmitter->Velocity.z)
			) / pDSPSettings->EmitterToListenerDistance;

			/* Clamp... */
			pDSPSettings->ListenerVelocityComponent = FAudio_min(
				pDSPSettings->ListenerVelocityComponent,
				scaledSpeedOfSound
			);
			pDSPSettings->EmitterVelocityComponent = FAudio_min(
				pDSPSettings->EmitterVelocityComponent,
				scaledSpeedOfSound
			);

			/* ... then Multiply. */
			pDSPSettings->DopplerFactor = (
				INSTANCE_SPEEDOFSOUND - pEmitter->DopplerScaler * pDSPSettings->ListenerVelocityComponent
			) / (
				INSTANCE_SPEEDOFSOUND - pEmitter->DopplerScaler * pDSPSettings->EmitterVelocityComponent
			);
			if (isnan(pDSPSettings->DopplerFactor))
			{
				pDSPSettings->DopplerFactor = 1.0f;
			}

			/* Limit the pitch shifting to 2 octaves up and 1 octave down */
			pDSPSettings->DopplerFactor = FAudio_clamp(
				pDSPSettings->DopplerFactor,
				0.5f,
				4.0f
			);
		}
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
