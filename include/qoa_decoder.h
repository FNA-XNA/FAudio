/* FAudio - XAudio Reimplementation for FNA
 *
 * Copyright (c) 2011-2023 Ethan Lee, Luigi Auriemma, and the MonoGame Team
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

#ifndef FAUDIO_QOA_DECODER_H
#define FAUDIO_QOA_DECODER_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct qoa qoa;

/* NOTE: this API only supports "static" type QOA files. "streaming" type files are not supported!! */
FAUDIOAPI qoa *qoa_open(unsigned char *bytes, unsigned int size);
FAUDIOAPI void qoa_attributes(qoa *qoa, unsigned int *channels, unsigned int *samplerate, unsigned int *samples_per_channel_per_frame, unsigned int *total_samples_per_channel);
FAUDIOAPI unsigned int qoa_decode_next_frame(qoa *qoa, short *sample_data); /* decode the next frame into a preallocated buffer */
FAUDIOAPI void qoa_seek_frame(qoa *qoa, int frame_index);
FAUDIOAPI short *qoa_load(qoa *qoa); /* return the entire qoa data decoded */
FAUDIOAPI void qoa_free(short *sample_data); /* free data from qoa_free */
FAUDIOAPI void qoa_close(qoa *qoa);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* FAUDIO_QOA_DECODER_H */

/* vim: set noexpandtab shiftwidth=8 tabstop=8: */
