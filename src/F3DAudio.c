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

/* Set to 1 to use more complex, closer to X3DAudio (but also more *wrong* in terms of 3D audio maths...) behaviour emulation. */
#define F3DAUDIO_HIGH_ACCURACY 0

#define F3DAUDIO_OUTPUT_CHECK 1

#define F3DAUDIO_DEBUG_PARAM_CHECKS 1
#define F3DAUDIO_DEBUG_PARAM_CHECKS_VERBOSE 0
#define F3DAUDIO_DEBUG_PARAM_CHECKS_BREAKPOINTS 0


/*
 * UTILITY MACROS
 */

 // TODO: For development only. Remove afterwards.
#define UNIMPLEMENTED() BREAK()

#if defined(_MSC_VER)
#define BREAK() __debugbreak()
#else
#define BREAK() SDL_assert(0)
#endif

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

#if defined(_MSC_VER)
#pragma comment(lib, "Kernel32.lib")
void __stdcall OutputDebugStringA(const char* lpOutputString);
#define OUTPUT_DEBUG_STRING(s) do { OutputDebugStringA(s); } while(0)
#else /* _MSC_VER */
#include <stdio.h>
#define OUTPUT_DEBUG_STRING(s) do { fprintf(stderr, "%s", s); fflush(stderr); } while(0)
#endif /* _MSC_VER */

#else /* F3DAUDIO_DEBUG_PARAM_CHECKS_VERBOSE */

#define OUTPUT_DEBUG_STRING(s) do {} while(0)
#endif /* F3DAUDIO_DEBUG_PARAM_CHECKS_VERBOSE */

#if F3DAUDIO_DEBUG_PARAM_CHECKS_BREAKPOINTS
#define CHECK_FAILED() BREAK()
#else
#define CHECK_FAILED() do { return PARAM_CHECK_FAIL; } while(0)
#endif
#define PARAM_CHECK(cond, msg) do { \
        if (!(cond)) { \
            OUTPUT_DEBUG_STRING("Check failed: " #cond "\n"); \
            OUTPUT_DEBUG_STRING(msg); \
            OUTPUT_DEBUG_STRING("\n"); \
            CHECK_FAILED(); \
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

// TODO: switch to square length (to save CPU)
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

static F3DAUDIO_VECTOR VectorNormalize(F3DAUDIO_VECTOR v, float* norm) {
    F3DAUDIO_VECTOR res;
    *norm = VectorLength(v);
    res.x = v.x / *norm;
    res.y = v.y / *norm;
    res.z = v.z / *norm;
    return res;
}

static float VectorDot(F3DAUDIO_VECTOR u, F3DAUDIO_VECTOR v) {
    return u.x*v.x + u.y*v.y + u.z*v.z;
}

/*
 * Generic utility functions
 */
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

// TODO: remove.
uint32_t popcount_naive(uint32_t bits) {
    uint32_t count = 0;
    while (bits != 0)
    {
        count += bits & 1;
        bits >>= 1;
    }
    return count;
}

uint32_t popcount(uint32_t bits) {
    uint32_t naive = popcount_naive(bits);
    uint32_t count = 0;
    while (bits) {
        count += 1;
        bits &= bits - 1;
    }
    SDL_assert(naive == count);
    return count;
}

/* Adrien:
 * first 4 bytes = channel mask
 * next 4 = number of speakers in the configuration
 * next 4 = bit 1 set if config has LFE speaker. bit 0 set if config has front center speaker. Alternative theory: this is the index
 * next 4 = speed of sound as specified by user arguments
 * next 4 = speed of sound, minus 1 ULP. Not sure how X3DAudio uses this.
 */
/* TODO: may cause aliasing problems */
#define SPEAKERMASK(Instance)       *((uint32_t*)   &Instance[0])
#define SPEAKERCOUNT(Instance)      *((uint32_t*)   &Instance[4])
#define SPEAKER_LF_INDEX(Instance)     *((uint32_t*)   &Instance[8])
#define SPEEDOFSOUND(Instance)      *((float*)  &Instance[12])
#define ADJUSTED_SPEEDOFSOUND(Instance) *((float*)  &Instance[16])

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

void F3DAudioInitialize(
    uint32_t SpeakerChannelMask,
    float SpeedOfSound,
    F3DAUDIO_HANDLE Instance
) {
    uint32_t punnedSpeedOfSound;
    if (!F3DAudioCheckInitParams(SpeakerChannelMask, SpeedOfSound, Instance))
    {
        return;
    }

    SPEAKERMASK(Instance) = SpeakerChannelMask;
    SPEAKERCOUNT(Instance) = popcount(SpeakerChannelMask);

    SPEAKER_LF_INDEX(Instance) = 0xFFFFFFFF;
    if (SpeakerChannelMask & SPEAKER_LOW_FREQUENCY) {
        if (SpeakerChannelMask & SPEAKER_FRONT_CENTER) {
            SPEAKER_LF_INDEX(Instance) = 3;
        } else {
            SPEAKER_LF_INDEX(Instance) = 2;
        }
    }

    SPEEDOFSOUND(Instance) = SpeedOfSound;

    punnedSpeedOfSound = float_to_uint32(SpeedOfSound);
    punnedSpeedOfSound -= 1;
    ADJUSTED_SPEEDOFSOUND(Instance) = uint32_to_float(punnedSpeedOfSound);
}

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
    if (Flags & F3DAUDIO_CALCULATE_ZEROCENTER)
    {
        uint32_t isCalculateMatrix = (Flags & F3DAUDIO_CALCULATE_MATRIX);
        uint32_t hasCenter = SPEAKERMASK(Instance) & SPEAKER_FRONT_CENTER;
        PARAM_CHECK(isCalculateMatrix && hasCenter, "F3DAUDIO_CALCULATE_ZEROCENTER is only valid for matrix calculations with an output format that has a center channel");
    }

    if (Flags & F3DAUDIO_CALCULATE_REDIRECT_TO_LFE)
    {
        uint32_t isCalculateMatrix = (Flags & F3DAUDIO_CALCULATE_MATRIX);
        uint32_t hasLF = SPEAKERMASK(Instance) & SPEAKER_LOW_FREQUENCY;
        PARAM_CHECK(isCalculateMatrix && hasLF, "F3DAUDIO_CALCULATE_REDIRECT_TO_LFE is only valid for matrix calculations with an output format that has a low-frequency channel");
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
                float currentAzimuth = pEmitter->pChannelAzimuths[i];
                FLOAT_BETWEEN_CHECK(currentAzimuth, 0.0f, F3DAUDIO_2PI);
                if (currentAzimuth == F3DAUDIO_2PI)
                {
                    PARAM_CHECK(!(Flags & F3DAUDIO_CALCULATE_REDIRECT_TO_LFE), "F3DAUDIO_CALCULATE_REDIRECT_TO_LFE valid only for matrix calculations with emitters that have no LFE channel");
                }
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

        /* Adrien: Sooo... This is awkward. MSDN doesn't say anything, but X3DAudio.h says that this should be lerped. However in practice the behaviour of X3DAudio isn't a lerp at all. It's easy to see with big InnerAngle / OuterAngle values. If we want accurate emulation, we'll need to either find what formula they use, or use a more advanced interpolation, like tricubic.
        */
        return LERP(alpha, innerParam, outerParam);
    }

    return outerParam;
}

/* Adrien: X3DAudio.h declares something like this, but the default (if emitter has NULL) volume curve is a *computed* inverse law, while on the other hand a curve leads to a piecewise linear function. So a "default curve" like this is pointless, not sure what X3DAudio does with it...
 */
#if 0
static F3DAUDIO_DISTANCE_CURVE_POINT DefaultVolumeCurvePoints[] = { {0.0f, 1.0f}, {1.0f, 0.0f} };
static F3DAUDIO_DISTANCE_CURVE DefaultVolumeCurve = { DefaultVolumeCurvePoints, ARRAY_COUNT(DefaultVolumeCurvePoints) };
#endif

/* Adrien: here we declare the azimuths of every speaker for every speaker configuration, ordered by increasing angle, as well as the index to which they map in the final matrix for their respective configuration. It had to be reverse engineered by looking at the data from various X3DAudioCalculate() matrix results for the various speaker configurations; *in particular*, the azimuths are different from the ones in F3DAudio.h (and X3DAudio.h) for SPEAKER_STEREO (which is declared has having front L and R speakers in the bit mask, but in fact has L and R *side* speakers). LF speakers are deliberately not included in the SpeakerInfo list, rather, we store the index into a separate field (with a -1 sentinel value if it has no LF speaker).  */
typedef struct {
    float azimuth;
    uint32_t matrixIdx;
} SpeakerInfo;

typedef struct {
    uint32_t configMask;
    const SpeakerInfo *speakers;
    uint32_t numNonLFSpeakers; /* not strictly necessary because it can be inferred from the SpeakerCount field of the F3DAUDIO_HANDLE, but makes code much cleaner and less error prone */
    uint32_t LFSpeakerIdx;
} ConfigInfo;

/* Adrien: it is absolutely necessary that these are stored in increasing, *positive* azimuth order (i.e. all angles between [0; 2PI]), as we'll do a linear interval search inside FindSpeakerAzimuths. */

#define SPEAKER_AZIMUTH_CENTER                (F3DAUDIO_PI *  0.0f       )
#define SPEAKER_AZIMUTH_FRONT_RIGHT_OF_CENTER (F3DAUDIO_PI *  1.0f / 8.0f)
#define SPEAKER_AZIMUTH_FRONT_RIGHT           (F3DAUDIO_PI *  1.0f / 4.0f)
#define SPEAKER_AZIMUTH_SIDE_RIGHT            (F3DAUDIO_PI *  1.0f / 2.0f)
#define SPEAKER_AZIMUTH_BACK_RIGHT            (F3DAUDIO_PI *  3.0f / 4.0f)
#define SPEAKER_AZIMUTH_BACK_CENTER           (F3DAUDIO_PI *  1.0f       )
#define SPEAKER_AZIMUTH_BACK_LEFT             (F3DAUDIO_PI *  5.0f / 4.0f)
#define SPEAKER_AZIMUTH_SIDE_LEFT             (F3DAUDIO_PI *  3.0f / 2.0f)
#define SPEAKER_AZIMUTH_FRONT_LEFT            (F3DAUDIO_PI *  7.0f / 4.0f)
#define SPEAKER_AZIMUTH_FRONT_LEFT_OF_CENTER  (F3DAUDIO_PI * 15.0f / 8.0f)

const SpeakerInfo kMonoConfigSpeakers[] = {
    {     SPEAKER_AZIMUTH_CENTER, 0 },
};
const SpeakerInfo kStereoConfigSpeakers[] = {
    { SPEAKER_AZIMUTH_SIDE_RIGHT, 1 },
    { SPEAKER_AZIMUTH_SIDE_LEFT , 0 },
};
const SpeakerInfo k2Point1ConfigSpeakers[] = {
    { SPEAKER_AZIMUTH_SIDE_RIGHT, 1 },
    { SPEAKER_AZIMUTH_SIDE_LEFT,  0 },
};
const SpeakerInfo kSurroundConfigSpeakers[] = {
    { SPEAKER_AZIMUTH_CENTER     , 2 },
    { SPEAKER_AZIMUTH_FRONT_RIGHT, 1 },
    { SPEAKER_AZIMUTH_BACK_CENTER, 3 },
    { SPEAKER_AZIMUTH_FRONT_LEFT,  0 },
};
const SpeakerInfo kQuadConfigSpeakers[] = {
    { SPEAKER_AZIMUTH_FRONT_RIGHT, 1 },
    { SPEAKER_AZIMUTH_BACK_RIGHT,  3 },
    { SPEAKER_AZIMUTH_BACK_LEFT,   2 },
    { SPEAKER_AZIMUTH_FRONT_LEFT,  0 },
};
const SpeakerInfo k4Point1ConfigSpeakers[] = {
    { SPEAKER_AZIMUTH_FRONT_RIGHT, 1 },
    { SPEAKER_AZIMUTH_BACK_RIGHT,  4 },
    { SPEAKER_AZIMUTH_BACK_LEFT,   3 },
    { SPEAKER_AZIMUTH_FRONT_LEFT,  0 },
};
const SpeakerInfo k5Point1ConfigSpeakers[] = {
    { SPEAKER_AZIMUTH_CENTER,      2 },
    { SPEAKER_AZIMUTH_FRONT_RIGHT, 1 },
    { SPEAKER_AZIMUTH_BACK_RIGHT,  5 },
    { SPEAKER_AZIMUTH_BACK_LEFT,   4 },
    { SPEAKER_AZIMUTH_FRONT_LEFT,  0 },
};
const SpeakerInfo k7Point1ConfigSpeakers[] = {
    { SPEAKER_AZIMUTH_CENTER,                2 },
    { SPEAKER_AZIMUTH_FRONT_RIGHT_OF_CENTER, 7 },
    { SPEAKER_AZIMUTH_FRONT_RIGHT,           1 },
    { SPEAKER_AZIMUTH_BACK_RIGHT,            5 },
    { SPEAKER_AZIMUTH_BACK_LEFT,             4 },
    { SPEAKER_AZIMUTH_FRONT_LEFT,            0 },
    { SPEAKER_AZIMUTH_FRONT_LEFT_OF_CENTER,  6 },
};
const SpeakerInfo k5Point1SurroundConfigSpeakers[] = {
    { SPEAKER_AZIMUTH_CENTER,      2 },
    { SPEAKER_AZIMUTH_FRONT_RIGHT, 1 },
    { SPEAKER_AZIMUTH_SIDE_RIGHT,  5 },
    { SPEAKER_AZIMUTH_SIDE_LEFT ,  4 },
    { SPEAKER_AZIMUTH_FRONT_LEFT,  0 },
};
const SpeakerInfo k7Point1SurroundConfigSpeakers[] = {
    { SPEAKER_AZIMUTH_CENTER,      2 },
    { SPEAKER_AZIMUTH_FRONT_RIGHT, 1 },
    { SPEAKER_AZIMUTH_SIDE_RIGHT,  7 },
    { SPEAKER_AZIMUTH_BACK_RIGHT,  5 },
    { SPEAKER_AZIMUTH_BACK_LEFT,   4 },
    { SPEAKER_AZIMUTH_SIDE_LEFT ,  6 },
    { SPEAKER_AZIMUTH_FRONT_LEFT,  0 },
};

/* Adrien: hmmm... with that organization, the index of the LF speaker into the matrix array strangely looks *exactly* like the mystery field in the F3DAUDIO_HANDLE!! */
const ConfigInfo kSpeakersConfigInfo[] = {
    { SPEAKER_MONO,             kMonoConfigSpeakers,            ARRAY_COUNT(kMonoConfigSpeakers),            -1 },
    { SPEAKER_STEREO,           kStereoConfigSpeakers,          ARRAY_COUNT(kStereoConfigSpeakers),          -1 },
    { SPEAKER_2POINT1,          k2Point1ConfigSpeakers,         ARRAY_COUNT(k2Point1ConfigSpeakers),          2 },
    { SPEAKER_SURROUND,         kSurroundConfigSpeakers,        ARRAY_COUNT(kSurroundConfigSpeakers),        -1 },
    { SPEAKER_QUAD,             kQuadConfigSpeakers,            ARRAY_COUNT(kQuadConfigSpeakers),            -1 },
    { SPEAKER_4POINT1,          k4Point1ConfigSpeakers,         ARRAY_COUNT(k4Point1ConfigSpeakers),          2 },
    { SPEAKER_5POINT1,          k5Point1ConfigSpeakers,         ARRAY_COUNT(k5Point1ConfigSpeakers),          3 },
    { SPEAKER_7POINT1,          k7Point1ConfigSpeakers,         ARRAY_COUNT(k7Point1ConfigSpeakers),          3 },
    { SPEAKER_5POINT1_SURROUND, k5Point1SurroundConfigSpeakers, ARRAY_COUNT(k5Point1SurroundConfigSpeakers),  3 },
    { SPEAKER_7POINT1_SURROUND, k7Point1SurroundConfigSpeakers, ARRAY_COUNT(k7Point1SurroundConfigSpeakers),  3 },
};

/* Adrien: this function does some sanity checks on the config info tables. It should only be enabled to validate the tables after adjusting them. */
static int CheckConfigurations() {
    int i;
    for (i = 0; i < ARRAY_COUNT(kSpeakersConfigInfo); ++i) {
        const ConfigInfo* curCfg = &kSpeakersConfigInfo[i];
        uint32_t nTotalSpeakers = popcount(curCfg->configMask);
        int hasLF = (curCfg->configMask & SPEAKER_LOW_FREQUENCY) ? 1 : 0;
        uint32_t expectedAzimuthsCount = nTotalSpeakers - hasLF;
        uint32_t i, s, expectedSum;

        if (expectedAzimuthsCount != curCfg->numNonLFSpeakers) {
            BREAK();
        }
        if (hasLF && curCfg->LFSpeakerIdx == -1) {
            BREAK();
        }

        for (i = 0; i < curCfg->numNonLFSpeakers - 1; ++i) {
            if (curCfg->speakers[i].azimuth >= curCfg->speakers[i+1].azimuth) {
                BREAK();
            }
        }

        /* This is an easy way to check that all indices are accounted for, and that there's no duplicate. */
        expectedSum = nTotalSpeakers * (nTotalSpeakers + 1) / 2;
        s = 0;
        for (i = 0; i < curCfg->numNonLFSpeakers; ++i) {
            s += (curCfg->speakers[i].matrixIdx + 1);
        }
        if (hasLF) {
            s += (curCfg->LFSpeakerIdx + 1);
        }
        if (s != expectedSum) {
            BREAK();
        }
    }

    return 1;
}

/* A simple linear search is OK for 10 elements. */
static const ConfigInfo* GetConfigInfo(uint32_t speakerConfigMask) {
    uint32_t i;
    for (i = 0; i < ARRAY_COUNT(kSpeakersConfigInfo); ++i)
    {
        if (kSpeakersConfigInfo[i].configMask == speakerConfigMask) {
            return &kSpeakersConfigInfo[i];
        }
    }
    // TODO: this should never happen. maybe assert in debug builds?
    return NULL;
}

/* Given a speaker mask, this function finds azimuths of the two closest speakers for a given emitter azimuth (in the basis of the emitter) */
static void FindSpeakerAzimuths(const ConfigInfo* config, float emitterAzimuth, int skipCenter, const SpeakerInfo **speakerInfo)
{
    uint32_t i, nexti;
    float a0, a1;
    if (!config)
    {
        // TODO: assert
        // return 0;
    }

    for (i = 0; i < config->numNonLFSpeakers; ++i)
    {
        a0 = config->speakers[i].azimuth;
        nexti = (i + 1) % config->numNonLFSpeakers;
        a1 = config->speakers[nexti].azimuth;
        if (a0 < a1)
        {
            if (emitterAzimuth >= a0 && emitterAzimuth < a1)
            {
                break;
            }
        }
        else
        {
            if (emitterAzimuth >= a0 || emitterAzimuth < a1)
            {
                break;
            }
        }
    }
    SDL_assert(emitterAzimuth >= a0 || emitterAzimuth < a1);

    if (skipCenter)
    {
        if (a0 == 0.0f)
        {
            if (i == 0)
            {
                i = config->numNonLFSpeakers - 1;
            }
            else
            {
                i -= 1;
            }
        }
        else if (a1 == 0.0f)
        {
            nexti += 1;
            if (nexti >= config->numNonLFSpeakers)
            {
                nexti -= config->numNonLFSpeakers;
            }
        }
    }
    speakerInfo[0] = &config->speakers[i];
    speakerInfo[1] = &config->speakers[nexti];
}

/* Constants for the "diffusion" step (the part where we attribute the attenuations to the various speakers). If the emitter to listener distance is less than the "null-distance", we spread equally over all speakers. Above the "non-null distance", we used the regular calculation. Between the two, it seems that X3DAudio does an "InnerRadius"-like calculation, spreading most of the signal towards the closest two speakers, but distributing some of it equally to the others. These constants have been determined roughly, using trial and error. */

// TODO: investigate whether the behaviour is truly InnerRadius-like
// NOTE: confirmed. Even with 0 innerradius, there's always a cylinder of radius NON_NULL_DISTANCE pointing towards listener->Top that diffuses to all channels.
#define DIFFUSION_NULL_DISTANCE_DISK_RADIUS 1e-7f
#define DIFFUSION_NON_NULL_DISTANCE_DISK_RADIUS 4e-7f

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
    const ConfigInfo* curConfig = GetConfigInfo(ChannelMask);
    float attenuation = 1.0f;
    float LFEattenuation = 1.0f;

    // TODO: Handle this.
    if (SrcChannelCount > 1)
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
        // UNIMPLEMENTED();
    }
    else
    {
        // TODO;
    }

    if (pListener->pCone)
    {
        /* Adrien: negate the dot product because we need listenerToEmitter in this case */
        /* Here the angle might be NaN or infinite if distance == 0... ComputeConeParameter *does* check for this special case. */
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
        /* Here the angle might be NaN or infinite if distance == 0... ComputeConeParameter *does* check for this special case. */
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

    if (DstChannelCount == 1)
    {
        MatrixCoefficients[0] = attenuation;
    }
    // TODO: this needs to be removed and factored into an InnerRadius behaviour
    else if (eToLDistance <= DIFFUSION_NULL_DISTANCE_DISK_RADIUS)
    {
        // TODO handle NON_NULL_DISTANCE
        // TODO: handle REDIRECT TO LFE
        uint32_t i, centerChannelIdx = -1;
        uint32_t nChannelsToDiffuseTo = curConfig->numNonLFSpeakers;
        float equalAttenuation;

        if ((Flags & F3DAUDIO_CALCULATE_ZEROCENTER))
        {
            nChannelsToDiffuseTo -= 1;
            SDL_assert(curConfig->speakers[0].azimuth == SPEAKER_AZIMUTH_CENTER);
            centerChannelIdx = curConfig->speakers[0].matrixIdx;
        }
        equalAttenuation = attenuation / nChannelsToDiffuseTo;
        for (i = 0; i < DstChannelCount; ++i)
        {
            if (i == centerChannelIdx || i == curConfig->LFSpeakerIdx)
            {
                continue;
            }
            MatrixCoefficients[i] = equalAttenuation;
        }
        if (Flags & F3DAUDIO_CALCULATE_REDIRECT_TO_LFE)
        {
            SDL_assert(curConfig->LFSpeakerIdx != -1);
            MatrixCoefficients[curConfig->LFSpeakerIdx] = attenuation;
        }
    }
    else
    {
        // TODO: InnerAngle / InnerRadius here.
        // TODO: change emitterToListener to listenerToEmitter everywhere else
        F3DAUDIO_VECTOR listenerToEmitter = VectorScale(emitterToListener, -1.0f);
        F3DAUDIO_VECTOR projTopVec, projPlane, orientRight;
        float elevation = VectorDot(pListener->OrientTop, listenerToEmitter);
        float radialDistance;
        float x, y;
        float emitterAzimuth;
        const SpeakerInfo* infos[2];
        int skipCenter = (Flags & F3DAUDIO_CALCULATE_ZEROCENTER) ? 1 : 0;
        float a0, a1, val;
        uint32_t i0, i1;

        projTopVec = VectorScale(pListener->OrientTop, elevation);
        /* projPlane is the projection of the listenerToEmitter vector, that is, the coordinates of emitter in the listener basis, onto the plane defined by the normal vector OrientTop */
        projPlane = VectorSub(listenerToEmitter, projTopVec);
        radialDistance = VectorLength(projPlane);

        /* Remember here that the coordinate system is Left-Handed. */
        orientRight = VectorCross(pListener->OrientTop, pListener->OrientFront);

        x = VectorDot(pListener->OrientFront, projPlane);
        y = VectorDot(orientRight, projPlane);

        /* Having checked for "small distances" before, our atan2f should be well-conditioned. (That is, projPlane has non-zero length) */
        emitterAzimuth = FAudio_atan2f(y, x);
        if (emitterAzimuth < 0.0f) {
            emitterAzimuth += F3DAUDIO_2PI;
        }
        // TODO: sprinkle asserts

        FindSpeakerAzimuths(curConfig, emitterAzimuth, skipCenter, infos);
        a0 = infos[0]->azimuth;
        a1 = infos[1]->azimuth;
        if (a0 > a1)
        {
            if (emitterAzimuth >= a0)
            {
                emitterAzimuth -= F3DAUDIO_2PI;
            }
            a0 -= F3DAUDIO_2PI;
        }
        SDL_assert(emitterAzimuth >= a0 && emitterAzimuth <= a1);
        val = (emitterAzimuth - a0) / (a1 - a0);

        i0 = infos[0]->matrixIdx;
        i1 = infos[1]->matrixIdx;

        FAudio_zero(MatrixCoefficients, sizeof(float) * SrcChannelCount * DstChannelCount);

        // TODO Clamp in release, assert in debug
        // TODO multi emitters.
        // for (int iEm = 0; iEm < SrcChannelCount; ++iEm) {
        MatrixCoefficients[i0] = (1.0f - val) * attenuation;
        MatrixCoefficients[i1] = (       val) * attenuation;
        // }


        if (Flags & F3DAUDIO_CALCULATE_REDIRECT_TO_LFE)
        {
            SDL_assert(curConfig->LFSpeakerIdx != -1);

            MatrixCoefficients[curConfig->LFSpeakerIdx] = attenuation;
        }
    }

    // TODO: add post check to valide values
    // (sum < 1, all values > 0, no Inf / NaN..
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
    // TODO: div by zero here if emitter and listener at same pos -Adrien
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

void F3DAudioCalculate(
    const F3DAUDIO_HANDLE Instance,
    const F3DAUDIO_LISTENER *pListener,
    const F3DAUDIO_EMITTER *pEmitter,
    uint32_t Flags,
    F3DAUDIO_DSP_SETTINGS *pDSPSettings
) {
    F3DAUDIO_VECTOR emitterToListener, normalizedEToL;
    float eToLDistance, dummy;

    /* Distance */
    emitterToListener = VectorSub(pListener->Position, pEmitter->Position);
    normalizedEToL = VectorNormalize(emitterToListener, &dummy);
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


// TODO: Move internal checks to an external test driver.
void F3DAudioInternalChecks() {
    if (!CheckConfigurations())
    {
        BREAK();
    }

    {
        const ConfigInfo* config = GetConfigInfo(SPEAKER_5POINT1);
        float emitterAzimuth = 0.0f;
        int skipCenter = 0;
        const SpeakerInfo* infos[2];
        float a0, a1, ea;
        uint32_t i;
        float dummy = 0.0f;
        struct {
            float azimuth;
            float a0;
            float a1;
        } kTestAzimuths[] = {
            { F3DAUDIO_PI * 13.0f / 8.0f, SPEAKER_AZIMUTH_BACK_LEFT, SPEAKER_AZIMUTH_FRONT_LEFT },
            { F3DAUDIO_PI *  7.0f / 4.0f, SPEAKER_AZIMUTH_FRONT_LEFT, SPEAKER_AZIMUTH_CENTER },
            { F3DAUDIO_PI * 15.0f / 8.0f, SPEAKER_AZIMUTH_FRONT_LEFT, SPEAKER_AZIMUTH_CENTER },
            {                       0.0f, SPEAKER_AZIMUTH_CENTER, SPEAKER_AZIMUTH_FRONT_RIGHT },
            { F3DAUDIO_PI *  1.0f / 8.0f, SPEAKER_AZIMUTH_CENTER, SPEAKER_AZIMUTH_FRONT_RIGHT },
            { F3DAUDIO_PI *  1.0f / 4.0f, SPEAKER_AZIMUTH_FRONT_RIGHT, SPEAKER_AZIMUTH_BACK_RIGHT },
            { F3DAUDIO_PI *  3.0f / 8.0f, SPEAKER_AZIMUTH_FRONT_RIGHT, SPEAKER_AZIMUTH_BACK_RIGHT },
            { F3DAUDIO_PI *  1.0f / 2.0f, SPEAKER_AZIMUTH_FRONT_RIGHT, SPEAKER_AZIMUTH_BACK_RIGHT },
            { F3DAUDIO_PI *  3.0f / 4.0f, SPEAKER_AZIMUTH_BACK_RIGHT, SPEAKER_AZIMUTH_BACK_LEFT  },
            { F3DAUDIO_PI               , SPEAKER_AZIMUTH_BACK_RIGHT, SPEAKER_AZIMUTH_BACK_LEFT  },
            { F3DAUDIO_PI *  5.0f / 4.0f, SPEAKER_AZIMUTH_BACK_LEFT, SPEAKER_AZIMUTH_FRONT_LEFT  },
        };
        // const SpeakerInfo* kExpectedInfos[][2] = {
        //     {},
        // };

        for (i = 0; i < ARRAY_COUNT(kTestAzimuths); ++i) {
            emitterAzimuth = kTestAzimuths[i].azimuth;
            FindSpeakerAzimuths(config, emitterAzimuth, skipCenter, infos);

            a0 = infos[0]->azimuth;
            a1 = infos[1]->azimuth;
            ea = emitterAzimuth;

            if (a0 != kTestAzimuths[i].a0) {
                BREAK();
            }
            if (a1 != kTestAzimuths[i].a1) {
                BREAK();
            }

            dummy += 1.0f;
        }
    }
}
