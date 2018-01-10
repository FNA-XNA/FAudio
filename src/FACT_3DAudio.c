/* FACT - XACT Reimplementation for FNA
 * Copyright 2009-2018 Ethan Lee and the MonoGame Team
 *
 * Released under the Microsoft Public License.
 * See LICENSE for details.
 */

#include "FACT_internal.h"

void FACT3DAudioInitialize(
	uint32_t SpeakerChannelMask,
	float SpeedOfSound,
	FACT3DAUDIO_HANDLE Instance
) {
	/* TODO */
}

void FACT3DAudioCalculate(
	const FACT3DAUDIO_HANDLE Instance,
	const FACT3DAUDIO_LISTENER *pListener,
	const FACT3DAUDIO_EMITTER *pEmitter,
	uint32_t Flags,
	FACT3DAUDIO_DSP_SETTINGS *pDSPSettings
) {
	/* TODO */
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
