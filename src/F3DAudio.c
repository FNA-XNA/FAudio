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
#include <float.h> /* Adrien: required for FLT_MIN/FLT_MAX */




 // TODO: For development only. Remove afterwards.
#define UNIMPLEMENTED() __debugbreak()
#define F3DAUDIO_DEBUG_PARAM_CHECKS 1
#define F3DAUDIO_DEBUG_PARAM_CHECKS_VERBOSE 0
#define F3DAUDIO_DEBUG_PARAM_CHECKS_BREAKPOINTS 0

/*
 * UTILITY MACROS
 */


/* Adrien VS2010 doesn't define isnan (which is C99), so here it is. */
#if defined(_MSC_VER) && !defined(isnan)
#define isnan(x) _isnan(x)
#endif

#define ARRAY_COUNT(x) (sizeof(x) / sizeof(x[0]))

/*
 * PARAMETER CHECK MACROS
 */

#if F3DAUDIO_DEBUG_PARAM_CHECKS
// TODO: to be changed to something different after dev
#if F3DAUDIO_DEBUG_PARAM_CHECKS_VERBOSE
#include <stdio.h>
#define OutputDebugString(s) do { fprintf(stderr, "%s", s); fflush(stderr); } while(0) 
#else
#define OutputDebugString(s) do {} while(0)
#endif

#if F3DAUDIO_DEBUG_PARAM_CHECKS_BREAKPOINTS
#define STOP() __debugbreak()
#else
#define STOP() do { return PARAM_CHECK_FAIL; } while(0)
#endif
#define PARAM_CHECK(cond, msg) do { \
        if (!(cond)) { \
            OutputDebugString("Check failed: " #cond "\n"); \
            OutputDebugString(msg); \
            OutputDebugString("\n"); \
            STOP(); \
        } \
    } while(0)
#else /* F3DAUDIO_DEBUG_PARAM_CHECKS */
#define PARAM_CHECK(cond, msg) do {} while(0)
#endif /* F3DAUDIO_DEBUG_PARAM_CHECKS */ 

#define POINTER_CHECK(p) PARAM_CHECK(p != NULL, "Pointer " #p " must be != NULL")

#define FLOAT_VALID_CHECK(f) PARAM_CHECK(!isnan(f) && !isinf(f), "Float is either NaN or Infinity")

#define FLOAT_BETWEEN_CHECK(f, a, b) do { \
    PARAM_CHECK(f >= a, "Value" #f " is too low"); \
    PARAM_CHECK(f <= b, "Value" #f " is too big"); \
    } while(0)

#define VECTOR_NORMAL_CHECK(v) do { \
        PARAM_CHECK(FAudio_abs(VectorLength(v) - 1.0) <= 1e-5, "Vector " #v " isn't normal"); \
    } while(0)

// To be considered orthonormal, a pair of vectors must have a magnitude of 1 +- 1x10-5 and a dot product of 0 +- 1x10-5.
#define VECTOR_BASE_CHECK(u, v) do { \
        PARAM_CHECK(FAudio_abs(VectorDot(u, v)) <= 1e-5, "Vector u and v have non-negligible dot product"); \
    } while(0)


/*
 * VECTOR UTILITIES
 */

static F3DAUDIO_VECTOR Vec(float x, float y, float z) {
    F3DAUDIO_VECTOR res;
    res.x = x;
    res.y = y;
    res.z = z;
    return res;
}

static F3DAUDIO_VECTOR VectorSub(F3DAUDIO_VECTOR u, F3DAUDIO_VECTOR v) {
    return Vec(u.x - v.x, u.y - v.y, u.z - v.z);
}

static F3DAUDIO_VECTOR VectorCross(F3DAUDIO_VECTOR u, F3DAUDIO_VECTOR v) {
    return Vec(u.y*v.z - u.z*v.y, u.z*v.x - u.x*v.z, u.x*v.y - u.y*v.x);
}

static float VectorSquareLength(F3DAUDIO_VECTOR v) {
    return v.x * v.x + v.y * v.y + v.z * v.z;
}

static float VectorLength(F3DAUDIO_VECTOR v) {
    return FAudio_sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
}

static float VectorDot(F3DAUDIO_VECTOR u, F3DAUDIO_VECTOR v) {
    return u.x*v.x + u.y*v.y + u.z*v.z;
}

/* F3DAUDIO_HANDLE Structure */
#define SPEAKERMASK(Instance)       *((uint32_t*)   &Instance[0])
#define SPEAKERCOUNT(Instance)      *((uint32_t*)   &Instance[4])
#define UNKNOWN_PARAM(Instance)     *((uint32_t*)   &Instance[8])
#define SPEEDOFSOUND(Instance)      *((float*)  &Instance[12])
#define ADJUSTED_SPEEDOFSOUND(Instance) *((float*)  &Instance[16])

/* -Adrien
 * first 4 bytes = channel mask
 * next 4 = some kind of value derived from channel mask
 * next 4 = again
 * next 4 = speed of sound as specified by user arguments
 * next 4 = speed of sound, minus some small epsilon. Perhaps a trick to not deal with denormals?
 *
 * I have no idea why they do this. I'll investigate further,
 * but I don't think that's too important for now.
 * TODO: change to a proper struct?
 * ASK: do we care about maintaining strict binary compatibility with X3DAudio?
 */

int F3DAudioCheckInitParams(
    uint32_t SpeakerChannelMask,
    float SpeedOfSound,
    F3DAUDIO_HANDLE instance
) {
    const uint32_t kAllowedSpeakerMasks[] = {
        SPEAKER_MONO,
        SPEAKER_STEREO,
        SPEAKER_2POINT1,
        SPEAKER_QUAD,
        SPEAKER_SURROUND,
        SPEAKER_4POINT1,
        SPEAKER_5POINT1,
        SPEAKER_5POINT1_SURROUND,
        SPEAKER_7POINT1,
        SPEAKER_7POINT1_SURROUND,
    };
    int speakerMaskIsValid = 0;
    int i;

    POINTER_CHECK(instance);

    for (i = 0; i < ARRAY_COUNT(kAllowedSpeakerMasks); ++i) {
        if (SpeakerChannelMask == kAllowedSpeakerMasks[i]) {
            speakerMaskIsValid = 1;
            break;
        }
    }
    /* Adrien: The docs don't clearly say this, but the debug dll DOES check it. */
    PARAM_CHECK(speakerMaskIsValid == 1, "SpeakerChannelMask is invalid. "
        "Needs to be one of MONO, STEREO, 2POINT1, QUAD, SURROUND, 4POINT1,"
        "5POINT1, 5POINT1_SURROUND, 7POINT1 or 7POINT1_SURROUND.");

    PARAM_CHECK(SpeedOfSound >= FLT_MIN, "SpeedOfSound needs to be >= FLT_MIN");

    return PARAM_CHECK_OK;
}

static uint32_t float_to_uint32(float x) {
    uint32_t res;
    FAudio_memcpy(&res, &x, sizeof(res));
    return res;
}

static float uint32_to_float(uint32_t x) {
    float res;
    FAudio_memcpy(&res, &x, sizeof(res));
    return res;
}

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

    if (!F3DAudioCheckInitParams(SpeakerChannelMask, SpeedOfSound, Instance))
    {
        return;
    }

    /* Required by MSDN: https://msdn.microsoft.com/en-us/library/windows/desktop/microsoft.directx_sdk.x3daudio.x3daudioinitialize(v=vs.85).aspx */

    SPEAKERMASK(Instance) = SpeakerChannelMask;
    while (SpeakerChannelMask != 0)
    {
        speakerCount += SpeakerChannelMask & 1;
        SpeakerChannelMask >>= 1;
    }
    SPEAKERCOUNT(Instance) = speakerCount;
    UNKNOWN_PARAM(Instance) = 0xFFFFFFFF; /* lolwut */
    SPEEDOFSOUND(Instance) = SpeedOfSound;

    /* Adrien: AFAIK the portable way to do type punning is through memcpy */
    /* "Convert" raw float to int... */
    epsilonHack.f = SpeedOfSound;
    /* ... Subtract epsilon value... */
    epsilonHack.i -= 1;
    /* ... Convert back to float. */
    ADJUSTED_SPEEDOFSOUND(Instance) = epsilonHack.f;
}

static float GetConeAttenuation(F3DAUDIO_CONE* pCone, F3DAUDIO_VECTOR pos, F3DAUDIO_VECTOR orient) {
    return 1.0; // TODO
}

/* Adrien: notes from MSDN
 * "The listener and emitter values must be valid. Floating-point specials (NaN, QNaN, +INF, -INF) can cause the entire audio output to go silent if introduced into a running audio graph."
 *
 * Listener:
 *  - if pCone is NULL use OrientFront for matrix and delay. WHen pCone isn't NULL, OrientFront is used for matrix, LPF (direct and reverb). What about delay??
 *  Must be orthonormal to OrientTop.
 *  - OrientTop: only for matrix and delay 
 *  - Position: does not affect velocity. In world units. Considering default speed 1 unit = 1 meter.
 *  - Velocity: used only for doppler calculations. wu/s.
 *  - "To be considered orthonormal, a pair of vectors must have a magnitude of 1 +- 1x10-5 and a dot product of 0 +- 1x10-5."
 *
 * Emitter: 
 *  - pCone: only for single channel emitters. NULL means omnidirectional emitter. Non-NULL: used for matrix, LPF reverb and direct, and reverb.
 *  - OrientFront: MUST BE NORMALIZED when used. Must be orthonormal to OrientTop. Single channel: only used for emitter angle. Multi-channel or single with cones: used for matrix, LPF (direct and reverb) and reverb.
 *  - OrientTop: only used for multi-channel for matrix. Must be orthonormal to OrientFront.
 *  - Pos/Velocity: same as Listener.
 *  - InnerRadius: must be between 0.0f and MAX_FLT. If 0, no inner radius used, but innerradiusangle may still be used.
 *  - InnerRadiusAngle: must be between 0.0f and X3DAUDIO_PI/4.0.
 *  - ChannelCount: number of emitters.
 *  - ChannelRadius: distance of emitters from center pos. Only used when ChannelCount >1 for matrix calculations.
 *  - pChannelAzimuths: table of channel azimuths. 2PI means LFE. LFE channels are placed in the center and only respect pLFEcurve, not pVolumeCurve. Can be NULL if ChannelCount = 1, otherwise must have ChannelCount elements. All values between 0 and 2Pi. USed for matrix calculations only.
 *  - pVolumeCurve/pLFECurve: only for matrix, for their respective channels. Default: if <= CurveDistanceScaler, no attenuation. Above is CurveDistanceScaler/distance attenuation. (Adrien: why does this mention and inverse SQUARE law then?)
 *  - pLPFDirectCurve: default curve: [0.0f, 1.0f], [1.0f, 0.75f]
 *  - pLPFReverbCurve: default curve: [0.0f, 0.75f], [1.0f, 0.75f]
 *  - pReverbCurve: default curve:  [0.0f, 1.0f], [1.0f, 0.0f].
 *  - CurveDistanceScaler: must be between FLT_MIN to FLT_MAX. Only for matrix, LPF (both) and reverb.
 *  - DopplerScaler: must be between 0.0f to FLT_MAX. Only for doppler calculations.
 *  - Remarks: cone only for single-emitter (doesnt make sense otherwise). Multi-point useful to avoid duplicate calculations like doppler (Adrien: implementation hint).
 * See the MSDN page for illustration of cone. 
 */

static int CheckCone(F3DAUDIO_CONE *pCone) {
    FLOAT_BETWEEN_CHECK(pCone->InnerAngle, 0.0f, F3DAUDIO_2PI);
    FLOAT_BETWEEN_CHECK(pCone->OuterAngle, pCone->InnerAngle, F3DAUDIO_2PI);

    FLOAT_BETWEEN_CHECK(pCone->InnerVolume, 0.0f, 2.0f);
    FLOAT_BETWEEN_CHECK(pCone->OuterVolume, 0.0f, 2.0f);

    FLOAT_BETWEEN_CHECK(pCone->InnerLPF, 0.0f, 1.0f);
    FLOAT_BETWEEN_CHECK(pCone->OuterLPF, 0.0f, 1.0f);

    FLOAT_BETWEEN_CHECK(pCone->InnerReverb, 0.0f, 2.0f);
    FLOAT_BETWEEN_CHECK(pCone->OuterReverb, 0.0f, 2.0f);

    return PARAM_CHECK_OK;
}

static int F3DAudioCheckCalculateParams(
    const F3DAUDIO_HANDLE Instance,
    const F3DAUDIO_LISTENER *pListener,
    const F3DAUDIO_EMITTER *pEmitter,
    uint32_t Flags,
    F3DAUDIO_DSP_SETTINGS *pDSPSettings
) {
    uint32_t i, ChannelCount;

    POINTER_CHECK(Instance);
    POINTER_CHECK(pListener);
    POINTER_CHECK(pEmitter);
    POINTER_CHECK(pDSPSettings);

    ChannelCount = SPEAKERCOUNT(Instance);
    PARAM_CHECK(pDSPSettings->DstChannelCount == ChannelCount, "Invalid channel count");
    PARAM_CHECK(pDSPSettings->SrcChannelCount == pEmitter->ChannelCount, "Invalid channel count");

    if (pListener->pCone)
    {
        CheckCone(pListener->pCone);
    }
    VECTOR_NORMAL_CHECK(pListener->OrientFront);
    VECTOR_NORMAL_CHECK(pListener->OrientTop);
    VECTOR_BASE_CHECK(pListener->OrientFront, pListener->OrientTop);

    if (pEmitter->pCone)
    {
        VECTOR_NORMAL_CHECK(pEmitter->OrientFront);
        CheckCone(pEmitter->pCone);
    }
    else if (Flags & F3DAUDIO_CALCULATE_EMITTER_ANGLE)
    {
        VECTOR_NORMAL_CHECK(pEmitter->OrientFront);
    }
    if (pEmitter->ChannelCount > 1) {
        /* Only used for multi-channel emitters */
        VECTOR_NORMAL_CHECK(pEmitter->OrientFront);
        VECTOR_NORMAL_CHECK(pEmitter->OrientTop);
        VECTOR_BASE_CHECK(pEmitter->OrientFront, pEmitter->OrientTop);
    }
    FLOAT_BETWEEN_CHECK(pEmitter->InnerRadius, 0.0f, FLT_MAX);
    FLOAT_BETWEEN_CHECK(pEmitter->InnerRadiusAngle, 0.0f, F3DAUDIO_2PI / 4.0f);
    PARAM_CHECK(pEmitter->ChannelCount > 0, "Invalid channel count for emitter");
    PARAM_CHECK(pEmitter->ChannelRadius >= 0.0f, "Invalid channel radius for emitter");
    if (pEmitter->ChannelCount > 1)
    {
        PARAM_CHECK(pEmitter->pChannelAzimuths != NULL, "Invalid channel azimuths for multi-channel emitter");
        if (pEmitter->pChannelAzimuths)
        {
            for (i = 0; i < pEmitter->ChannelCount; ++i)
            {
                FLOAT_BETWEEN_CHECK(pEmitter->pChannelAzimuths[i], 0.0f, F3DAUDIO_2PI);
            }
        }
    }
    // TODO: validate curves
    FLOAT_BETWEEN_CHECK(pEmitter->CurveDistanceScaler, FLT_MIN, FLT_MAX);
    FLOAT_BETWEEN_CHECK(pEmitter->DopplerScaler, 0.0f, FLT_MAX);

    return PARAM_CHECK_OK;
}

static void CalculateMatrix(
    uint32_t ChannelMask,
    uint32_t Flags,
    const F3DAUDIO_LISTENER *pListener,
    const F3DAUDIO_EMITTER *pEmitter,
    float* MatrixCoefficients,
    uint32_t SrcChannelCount,
    uint32_t DstChannelCount
) {
    F3DAUDIO_VECTOR listenerToEmitter;
    float attenuation;
    float distance;

    if (DstChannelCount > 1 || SrcChannelCount > 1)
    {
        UNIMPLEMENTED();
        return;
    }

    // TODO: reuse from F3DAudioCalculate rather than recompute?
    listenerToEmitter = VectorSub(pEmitter->Position, pListener->Position);

    distance = VectorLength(listenerToEmitter);
    if ((distance / pEmitter->CurveDistanceScaler) >= 1.0f)
    {
        attenuation = pEmitter->CurveDistanceScaler / distance;
    }
    else
    {
        attenuation = 1.0f;
    }
    // TODO: triple check this...
    // TODO: careful with angle calculations required by cones if distance is close to zero...
    {
        float emitterConeAttenuation = GetConeAttenuation(pEmitter->pCone, listenerToEmitter, pEmitter->OrientFront);
        float listenerConeAttenuation = GetConeAttenuation(pListener->pCone, listenerToEmitter, pListener->OrientFront);

        attenuation *= emitterConeAttenuation;
        attenuation *= listenerConeAttenuation;
    }
    MatrixCoefficients[0] = attenuation;

    // Adrien: X3DAUDIO_CALCULATE_REDIRECT_TO_LFE / X3DAUDIO_CALCULATE_ZEROCENTER should be easy to implement.
    if (Flags & F3DAUDIO_CALCULATE_REDIRECT_TO_LFE)
    {
        UNIMPLEMENTED();
    }

    if (Flags & F3DAUDIO_CALCULATE_ZEROCENTER)
    {
        UNIMPLEMENTED();
    }
}

/* Adrien: notes from MSDN
 * "The listener and emitter values must be valid. Floating-point specials (NaN, QNaN, +INF, -INF) can cause the entire audio output to go silent if introduced into a running audio graph."
 *
 * Listener:
 *  - if pCone is NULL use OrientFront for matrix and delay. WHen pCone isn't NULL, OrientFront is used for matrix, LPF (direct and reverb). What about delay??
 *  Must be orthonormal to OrientTop.
 *  - OrientTop: only for matrix and delay 
 *  - Position: does not affect velocity. In world units. Considering default speed 1 unit = 1 meter.
 *  - Velocity: used only for doppler calculations. wu/s.
 *  - "To be considered orthonormal, a pair of vectors must have a magnitude of 1 +- 1x10-5 and a dot product of 0 +- 1x10-5."
 *
 * Emitter: 
 *  - pCone: only for single channel emitters. NULL means omnidirectional emitter. Non-NULL: used for matrix, LPF reverb and direct, and reverb.
 *  - OrientFront: MUST BE NORMALIZED when used. Must be orthonormal to OrientTop. Single channel: only used for emitter angle. Multi-channel or single with cones: used for matrix, LPF (direct and reverb) and reverb.
 *  - OrientTop: only used for multi-channel for matrix. Must be orthonormal to OrientFront.
 *  - Pos/Velocity: same as Listener.
 *  - InnerRadius: must be between 0.0f and MAX_FLT. If 0, no inner radius used, but innerradiusangle may still be used.
 *  - InnerRadiusAngle: must be between 0.0f and X3DAUDIO_PI/4.0.
 *  - ChannelCount: number of emitters.
 *  - ChannelRadius: distance of emitters from center pos. Only used when ChannelCount >1 for matrix calculations.
 *  - pChannelAzimuths: table of channel azimuths. 2PI means LFE. LFE channels are placed in the center and only respect pLFEcurve, not pVolumeCurve. Can be NULL if ChannelCount = 1, otherwise must have ChannelCount elements. All values between 0 and 2Pi. USed for matrix calculations only.
 *  - pVolumeCurve/pLFECurve: only for matrix, for their respective channels. Default: if <= CurveDistanceScaler, no attenuation. Above is CurveDistanceScaler/distance attenuation. (Adrien: why does this mention and inverse SQUARE law then?)
 *  - pLPFDirectCurve: default curve: [0.0f, 1.0f], [1.0f, 0.75f]
 *  - pLPFReverbCurve: default curve: [0.0f, 0.75f], [1.0f, 0.75f]
 *  - pReverbCurve: default curve:  [0.0f, 1.0f], [1.0f, 0.0f].
 *  - CurveDistanceScaler: must be between FLT_MIN to FLT_MAX. Only for matrix, LPF (both) and reverb.
 *  - DopplerScaler: must be between 0.0f to FLT_MAX. Only for doppler calculations.
 *  - Remarks: cone only for single-emitter (doesnt make sense otherwise). Multi-point useful to avoid duplicate calculations like doppler (Adrien: implementation hint).
 * See the MSDN page for illustration of cone. 
 */

void F3DAudioCalculate(
    const F3DAUDIO_HANDLE Instance,
    const F3DAUDIO_LISTENER *pListener,
    const F3DAUDIO_EMITTER *pEmitter,
    uint32_t Flags,
    F3DAUDIO_DSP_SETTINGS *pDSPSettings
) {
    F3DAUDIO_VECTOR emitterToListener;
    uint32_t ChannelMask, ChannelCount;

    /* Distance */
    emitterToListener.x = pListener->Position.x - pEmitter->Position.x;
    emitterToListener.y = pListener->Position.y - pEmitter->Position.y;
    emitterToListener.z = pListener->Position.z - pEmitter->Position.z;
    pDSPSettings->EmitterToListenerDistance = VectorLength(emitterToListener);

    ChannelMask = SPEAKERMASK(Instance);
    ChannelCount = SPEAKERCOUNT(Instance);
    F3DAudioCheckCalculateParams(Instance, pListener, pEmitter, Flags, pDSPSettings);

    if (Flags & F3DAUDIO_CALCULATE_MATRIX)
    {
        CalculateMatrix(
            ChannelMask,
            Flags,
            pListener,
            pEmitter,
            pDSPSettings->pMatrixCoefficients,
            pDSPSettings->SrcChannelCount,
            pDSPSettings->DstChannelCount);
    }

    if (Flags & F3DAUDIO_CALCULATE_DELAY)
    {
        //CalculateDelay(pDSPSettings->pDelayTimes, pDSPSettings->DstChannelCount);
        UNIMPLEMENTED();
    }

    if (Flags & F3DAUDIO_CALCULATE_LPF_DIRECT)
    {
        UNIMPLEMENTED();
    }

    if (Flags & F3DAUDIO_CALCULATE_LPF_REVERB)
    {
        UNIMPLEMENTED();
    }

    if (Flags & F3DAUDIO_CALCULATE_REVERB)
    {
        UNIMPLEMENTED();
    }

    /* DopplerPitchScalar
     * Adapted from algorithm published as a part of the webaudio specification:
     * https://dvcs.w3.org/hg/audio/raw-file/tip/webaudio/specification.html#Spatialization-doppler-shift
     * -Chad
     */

     // Adrien: split into different func
    if (Flags & F3DAUDIO_CALCULATE_DOPPLER)
    {
        // Adrien: div by zero problem here if emitter and listener at same pos...
        pDSPSettings->DopplerFactor = 1.0f;
        if (pEmitter->DopplerScaler > 0.0f)
        {
            float scaledSpeedOfSound;
            scaledSpeedOfSound = SPEEDOFSOUND(Instance) / pEmitter->DopplerScaler;

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
+               pDSPSettings->EmitterVelocityComponent,
                scaledSpeedOfSound
            );

            pDSPSettings->DopplerFactor = (
                SPEEDOFSOUND(Instance) - pEmitter->DopplerScaler * pDSPSettings->ListenerVelocityComponent
            ) / (
                SPEEDOFSOUND(Instance) - pEmitter->DopplerScaler * pDSPSettings->EmitterVelocityComponent
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
    /* As it is implemented, this needs to happen LAST, because it
     * modifies the emitterToListener vector used elsewhere. */
    if (Flags & F3DAUDIO_CALCULATE_EMITTER_ANGLE)
    {
        emitterToListener.x /= pDSPSettings->EmitterToListenerDistance;
        emitterToListener.y /= pDSPSettings->EmitterToListenerDistance;
        emitterToListener.z /= pDSPSettings->EmitterToListenerDistance;
        pDSPSettings->EmitterToListenerAngle = FAudio_acosf(
            (emitterToListener.x * pListener->OrientFront.x) +
            (emitterToListener.y * pListener->OrientFront.y) +
            (emitterToListener.z * pListener->OrientFront.z)
        );
    }
}
