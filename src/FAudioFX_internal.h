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

#ifndef FAUDIOFX_DSP_H
#define FAUDIOFX_DSP_H

#include <stddef.h>
#include <stdint.h>

/* forward declarations */
typedef struct DspReverb DspReverb;
typedef struct FAudioFXReverbParameters FAudioFXReverbParameters;

/* interface functions */
DspReverb *DspReverb_Create(int32_t sampleRate);
void DspReverb_SetParameters(DspReverb *reverb, FAudioFXReverbParameters *params);
void DspReverb_Process(DspReverb *reverb, const float *samples_in, float *samples_out, size_t sample_count, int32_t num_channels);
void DspReverb_Reset(DspReverb *reverb);
void DspReverb_Destroy(DspReverb *reverb);

#endif // FAUDIOFX_DSP_h
