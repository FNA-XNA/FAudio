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
#if defined(_MSC_VER)
#define BREAK() __debugbreak()
#else
#include <assert.h>
#define BREAK() assert(0)
#endif

#define UNIMPLEMENTED() BREAK()
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

#define LERP(a, x, y) ((1.0f - a) * x + a * y)

/*
 * PARAMETER CHECK MACROS
 */

#if F3DAUDIO_DEBUG_PARAM_CHECKS
// TODO: to be changed to something different after dev
#if F3DAUDIO_DEBUG_PARAM_CHECKS_VERBOSE
#include <stdio.h>
// #define OUTPUT_DEBUG_STRING(s) do { fprintf(stderr, "%s", s); fflush(stderr); } while(0)
#pragma comment(lib, "Kernel32.lib")
void __stdcall OutputDebugStringA(const char* lpOutputString);
#define OUTPUT_DEBUG_STRING(s) do { OutputDebugStringA(s); } while(0)
#else
#define OUTPUT_DEBUG_STRING(s) do {} while(0)
#endif

#if F3DAUDIO_DEBUG_PARAM_CHECKS_BREAKPOINTS
#define STOP() BREAK()
#else
#define STOP() do { return PARAM_CHECK_FAIL; } while(0)
#endif
#define PARAM_CHECK(cond, msg) do { \
        if (!(cond)) { \
            OUTPUT_DEBUG_STRING("Check failed: " #cond "\n"); \
            OUTPUT_DEBUG_STRING(msg); \
            OUTPUT_DEBUG_STRING("\n"); \
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
        PARAM_CHECK(FAudio_fabsf(VectorLength(v) - 1.0f) <= 1e-5f, "Vector " #v " isn't normal"); \
    } while(0)

// To be considered orthonormal, a pair of vectors must have a magnitude of 1 +- 1x10-5 and a dot product of 0 +- 1x10-5.
#define VECTOR_BASE_CHECK(u, v) do { \
        PARAM_CHECK(FAudio_fabsf(VectorDot(u, v)) <= 1e-5f, "Vector u and v have non-negligible dot product"); \
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

static F3DAUDIO_VECTOR VectorScale(F3DAUDIO_VECTOR u, float s) {
    return Vec(u.x * s, u.y * s, u.z * s);
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
/* FIXME: may cause aliasing problems */
#define SPEAKERMASK(Instance)       *((uint32_t*)   &Instance[0])
#define SPEAKERCOUNT(Instance)      *((uint32_t*)   &Instance[4])
#define UNKNOWN_PARAM(Instance)     *((uint32_t*)   &Instance[8])
#define SPEEDOFSOUND(Instance)      *((float*)  &Instance[12])
#define ADJUSTED_SPEEDOFSOUND(Instance) *((float*)  &Instance[16])

/* -Adrien
 * first 4 bytes = channel mask
 * next 4 = number of speakers in the configuration
 * next 4 = bit 1 set if config has LFE speaker. bit 0 set if config has front center speaker.
 * next 4 = speed of sound as specified by user arguments
 * next 4 = speed of sound, minus 1 ULP. Why?
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
    /* Adrien: The docs don't clearly say this, but the debug dll does check it. */
    PARAM_CHECK(speakerMaskIsValid == 1, "SpeakerChannelMask is invalid. "
        "Needs to be one of MONO, STEREO, 2POINT1, QUAD, SURROUND, 4POINT1,"
        "5POINT1, 5POINT1_SURROUND, 7POINT1 or 7POINT1_SURROUND.");

    PARAM_CHECK(SpeedOfSound >= FLT_MIN, "SpeedOfSound needs to be >= FLT_MIN");

    return PARAM_CHECK_OK;
}

#if 0
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
#endif

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

/* Adrien: notes from MSDN
 * "The listener and emitter values must be valid. Floating-point specials (NaN, QNaN, +INF, -INF) can cause the entire audio output to go silent if introduced into a running audio graph." TODO: add paranoid checks
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
    if (!pCone)
    {
        return PARAM_CHECK_OK;
    }

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

static int CheckCurve(F3DAUDIO_DISTANCE_CURVE *pCurve) {
    F3DAUDIO_DISTANCE_CURVE_POINT *points;
    uint32_t i;
    if (!pCurve)
    {
        return PARAM_CHECK_OK;
    }

    points = pCurve->pPoints;
    POINTER_CHECK(points);
    PARAM_CHECK(pCurve->PointCount >= 2, "Invalid number of points for curve");

    for (i = 0; i < pCurve->PointCount; ++i) {
        FLOAT_BETWEEN_CHECK(points[i].Distance, 0.0f, 1.0f);
    }
    PARAM_CHECK(points[0].Distance == 0.0f, "First point in the curve must be at distance 0.0f");
    PARAM_CHECK(points[pCurve->PointCount - 1].Distance == 1.0f, "Last point in the curve must be at distance 1.0f");

    for (i = 0; i < (pCurve->PointCount - 1); ++i)
    {
        PARAM_CHECK(points[i].Distance < points[i].Distance, "Curve points must be in strict ascending order");
    }

    return PARAM_CHECK_OK;
}

int F3DAudioCheckCalculateParams(
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

    if (Flags & F3DAUDIO_CALCULATE_MATRIX)
    {
        POINTER_CHECK(pDSPSettings->pMatrixCoefficients);
    }

    ChannelCount = SPEAKERCOUNT(Instance);
    PARAM_CHECK(pDSPSettings->DstChannelCount == ChannelCount, "Invalid channel count, DSP settings and speaker configuration must agree");
    PARAM_CHECK(pDSPSettings->SrcChannelCount == pEmitter->ChannelCount, "Invalid channel count, DSP settings and emitter must agree");

    if (pListener->pCone)
    {
        PARAM_CHECK(CheckCone(pListener->pCone) == PARAM_CHECK_OK, "Invalid listener cone");
    }
    VECTOR_NORMAL_CHECK(pListener->OrientFront);
    VECTOR_NORMAL_CHECK(pListener->OrientTop);
    VECTOR_BASE_CHECK(pListener->OrientFront, pListener->OrientTop);

    if (pEmitter->pCone)
    {
        VECTOR_NORMAL_CHECK(pEmitter->OrientFront);
        PARAM_CHECK(CheckCone(pEmitter->pCone) == PARAM_CHECK_OK, "Invalid emitter cone");
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
    FLOAT_BETWEEN_CHECK(pEmitter->CurveDistanceScaler, FLT_MIN, FLT_MAX);
    FLOAT_BETWEEN_CHECK(pEmitter->DopplerScaler, 0.0f, FLT_MAX);


    PARAM_CHECK(CheckCurve(pEmitter->pVolumeCurve) == PARAM_CHECK_OK, "Invalid Volume curve");
    PARAM_CHECK(CheckCurve(pEmitter->pLFECurve) == PARAM_CHECK_OK, "Invalid LFE curve");
    PARAM_CHECK(CheckCurve(pEmitter->pLPFDirectCurve) == PARAM_CHECK_OK, "Invalid LPFDirect curve");
    PARAM_CHECK(CheckCurve(pEmitter->pLPFReverbCurve) == PARAM_CHECK_OK, "Invalid LPFReverb curve");
    PARAM_CHECK(CheckCurve(pEmitter->pReverbCurve) == PARAM_CHECK_OK, "Invalid Reverb curve");

    return PARAM_CHECK_OK;
}

static float FindCurveParam(
    F3DAUDIO_DISTANCE_CURVE *pCurve,
    float normalizedDistance
) {
    float alpha, val;
    F3DAUDIO_DISTANCE_CURVE_POINT* points = pCurve->pPoints;
    uint32_t n_points = pCurve->PointCount;
    /* Adrien: by definition, the first point in the curve must be 0.0f */
    size_t i = 1;
    while (i < n_points && normalizedDistance >= points[i].Distance) {
        ++i;
    }
    if (i == n_points) {
        return points[n_points - 1].DSPSetting;
    }

    alpha = (points[i].Distance - normalizedDistance) / (points[i].Distance - points[i - 1].Distance);

    val = LERP(alpha, points[i].DSPSetting, points[i - 1].DSPSetting);
    return val;
}

#define CONE_NULL_DISTANCE_TOLERANCE 1e-7 /* Adrien: determined experimentally */
static float ComputeConeParameter(
    float distance,
    float angle,
    float innerAngle,
    float outerAngle,
    float innerParam,
    float outerParam
) {
    float halfInnerAngle, halfOuterAngle;
    /* Quote X3DAudio.h: "Set both cone angles to 0 or X3DAUDIO_2PI for omnidirectionality using only the outer or inner values respectively." */
    if (innerAngle == 0.0f && outerAngle == 0.0f)
    {
        return outerParam;
    }

    if (innerAngle == F3DAUDIO_2PI && outerAngle == F3DAUDIO_2PI)
    {
        return innerParam;
    }

    halfInnerAngle = innerAngle / 2.0f;
    if (distance <= CONE_NULL_DISTANCE_TOLERANCE || angle <= halfInnerAngle)
    {
        return innerParam;
    }

    halfOuterAngle = outerAngle / 2.0f;
    if (angle <= halfOuterAngle)
    {
        float alpha = (angle - halfInnerAngle) / (halfOuterAngle - halfInnerAngle);
        return LERP(alpha, innerParam, outerParam);
    }

    return outerParam;
}

// X3DAudio.h declares one, but the default (if emitter has NULL) volume curve is a *computed* inverse law, while on the other hand a curve leads to a piecewise linear function.
// static F3DAUDIO_DISTANCE_CURVE_POINT DefaultVolumeCurvePoints[] = { {0.0f, 1.0f}, {1.0f, 0.0f} };
// static F3DAUDIO_DISTANCE_CURVE DefaultVolumeCurve = { DefaultVolumeCurvePoints, ARRAY_COUNT(DefaultVolumeCurvePoints) };

static void CalculateMatrix(
    uint32_t ChannelMask,
    uint32_t Flags,
    const F3DAUDIO_LISTENER *pListener,
    const F3DAUDIO_EMITTER *pEmitter,
    uint32_t SrcChannelCount,
    uint32_t DstChannelCount,
    F3DAUDIO_VECTOR emitterToListener,
    float eToLDistance,
    float* MatrixCoefficients
) {
    float attenuation = 1.0f;
    float LFEattenuation = 1.0f;

    if (DstChannelCount > 1 || SrcChannelCount > 1)
    {
        UNIMPLEMENTED();
        return;
    }

    if (pEmitter->pVolumeCurve)
    {
        float nd = eToLDistance / pEmitter->CurveDistanceScaler;
        float val = FindCurveParam(pEmitter->pVolumeCurve, nd);
        attenuation *= val;
    }
    else
    {
        if ((eToLDistance / pEmitter->CurveDistanceScaler) >= 1.0f)
        {
            attenuation = pEmitter->CurveDistanceScaler / eToLDistance;
        }
    }

    if (pEmitter->pLFECurve) {
        UNIMPLEMENTED();
    }
    else
    {
        // TODO;
    }

    if (pListener->pCone)
    {
        /* Adrien: negate the dot product because we need listenerToEmitter in this case */
        float angle = FAudio_acosf(-1.0f * VectorDot(pListener->OrientFront, emitterToListener) / eToLDistance);
        float listenerConeParam = ComputeConeParameter(
            eToLDistance,
            angle,
            pListener->pCone->InnerAngle,
            pListener->pCone->OuterAngle,
            pListener->pCone->InnerVolume,
            pListener->pCone->OuterVolume
        );
        attenuation *= listenerConeParam;
    }

    if (pEmitter->pCone)
    {
        float angle = FAudio_acosf(VectorDot(pEmitter->OrientFront, emitterToListener) / eToLDistance);
        float emitterConeParam = ComputeConeParameter(
            eToLDistance,
            angle,
            pEmitter->pCone->InnerAngle,
            pEmitter->pCone->OuterAngle,
            pEmitter->pCone->InnerVolume,
            pEmitter->pCone->OuterVolume
        );
        attenuation *= emitterConeParam;
    }

    // TODO: diffuse
    MatrixCoefficients[0] = attenuation;


    if (Flags & F3DAUDIO_CALCULATE_ZEROCENTER)
    {
        // Fills the center channel with silence. This flag allows you to keep a 6-channel matrix so you do not have to remap the channels, but the center channel will be silent. This flag is only valid if you also set X3DAUDIO_CALCULATE_MATRIX.
        UNIMPLEMENTED();
    }

    if (Flags & F3DAUDIO_CALCULATE_REDIRECT_TO_LFE)
    {
         // Applies an equal mix of all source channels to a low frequency effect (LFE) destination channel. It only applies to matrix calculations with a source that does not have an LFE channel and a destination that does have an LFE channel. This flag is only valid if you also set X3DAUDIO_CALCULATE_MATRIX.
        UNIMPLEMENTED();
    }
}

/* DopplerPitchScalar
 * Adapted from algorithm published as a part of the webaudio specification:
 * https://dvcs.w3.org/hg/audio/raw-file/tip/webaudio/specification.html#Spatialization-doppler-shift
 * -Chad
 */
static void CalculateDoppler(
    float SpeedOfSound,
    const F3DAUDIO_LISTENER* pListener,
    const F3DAUDIO_EMITTER* pEmitter,
    F3DAUDIO_VECTOR emitterToListener,
    float eToLDistance,
    float* listenerVelocityComponent,
    float* emitterVelocityComponent,
    float* DopplerFactor
) {
    // FIXME: div by zero here if emitter and listener at same pos -Adrien
    *DopplerFactor = 1.0f;
    if (pEmitter->DopplerScaler > 0.0f)
    {
        float scaledSpeedOfSound;
        scaledSpeedOfSound = SpeedOfSound / pEmitter->DopplerScaler;

        /* Project... */
        *listenerVelocityComponent =
            VectorDot(emitterToListener, pListener->Velocity) / eToLDistance;
        *emitterVelocityComponent =
            VectorDot(emitterToListener, pEmitter->Velocity) / eToLDistance;

        /* Clamp... */
        *listenerVelocityComponent = FAudio_min(
            *listenerVelocityComponent,
            scaledSpeedOfSound
        );
        *emitterVelocityComponent = FAudio_min(
            *emitterVelocityComponent,
            scaledSpeedOfSound
        );

        *DopplerFactor = (
            SpeedOfSound - pEmitter->DopplerScaler * *listenerVelocityComponent
        ) / (
            SpeedOfSound - pEmitter->DopplerScaler * *emitterVelocityComponent
        );
        if (isnan(*DopplerFactor))
        {
            *DopplerFactor = 1.0f;
        }

        /* Limit the pitch shifting to 2 octaves up and 1 octave down */
        *DopplerFactor = FAudio_clamp(
            *DopplerFactor,
            0.5f,
            4.0f
        );
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
    float eToLDistance;

    /* Distance */
    emitterToListener = VectorSub(pListener->Position, pEmitter->Position);

    eToLDistance = VectorLength(emitterToListener);
    pDSPSettings->EmitterToListenerDistance = eToLDistance;

    F3DAudioCheckCalculateParams(Instance, pListener, pEmitter, Flags, pDSPSettings);

    if (Flags & F3DAUDIO_CALCULATE_MATRIX)
    {
        CalculateMatrix(
            SPEAKERMASK(Instance),
            Flags,
            pListener,
            pEmitter,
            pDSPSettings->SrcChannelCount,
            pDSPSettings->DstChannelCount,
            emitterToListener,
            eToLDistance,
            pDSPSettings->pMatrixCoefficients
        );
    }

    if (Flags & F3DAUDIO_CALCULATE_DELAY)
    {
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

    if (Flags & F3DAUDIO_CALCULATE_DOPPLER)
    {
        CalculateDoppler(
            SPEEDOFSOUND(Instance),
            pListener,
            pEmitter,
            emitterToListener,
            eToLDistance,
            &pDSPSettings->ListenerVelocityComponent,
            &pDSPSettings->EmitterVelocityComponent,
            &pDSPSettings->DopplerFactor
        );

    }

    /* OrientationAngle */
    if (Flags & F3DAUDIO_CALCULATE_EMITTER_ANGLE)
    {
        /* Note: OrientFront is normalized. */
        pDSPSettings->EmitterToListenerAngle = FAudio_acosf(
            VectorDot(emitterToListener, pEmitter->OrientFront) / eToLDistance
        );
    }
}
