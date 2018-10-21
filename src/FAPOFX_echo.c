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

#include "FAPOFX.h"
#include "FAudio_internal.h"

/* FXEcho FAPO Implementation */

const FAudioGUID FAPOFX_CLSID_FXEcho =
{
	0xA90BC001,
	0xE897,
	0xE897,
	{
		0x74,
		0x39,
		0x43,
		0x55,
		0x00,
		0x00,
		0x00,
		0x03
	}
};

static FAPORegistrationProperties FXEchoProperties =
{
	/* .clsid = */ FAPOFX_CLSID_FXEcho,
	/* .FriendlyName = */
	{
		'F', 'X', 'E', 'c', 'h', 'o', '\0'
	},
	/*.CopyrightInfo = */
	{
		'C', 'o', 'p', 'y', 'r', 'i', 'g', 'h', 't', ' ', '(', 'c', ')',
		'E', 't', 'h', 'a', 'n', ' ', 'L', 'e', 'e', '\0'
	},
	/*.MajorVersion = */ 0,
	/*.MinorVersion = */ 0,
	/*.Flags = */(
		FAPO_FLAG_FRAMERATE_MUST_MATCH |
		FAPO_FLAG_BITSPERSAMPLE_MUST_MATCH |
		FAPO_FLAG_BUFFERCOUNT_MUST_MATCH |
		FAPO_FLAG_INPLACE_SUPPORTED |
		FAPO_FLAG_INPLACE_REQUIRED
	),
	/*.MinInputBufferCount = */ 1,
	/*.MaxInputBufferCount = */  1,
	/*.MinOutputBufferCount = */ 1,
	/*.MaxOutputBufferCount =*/ 1
};

typedef struct FAPOFXEcho
{
	FAPOBase base;

	/* TODO */
} FAPOFXEcho;

void FAPOFXEcho_Process(
	FAPOFXEcho *fapo,
	uint32_t InputProcessParameterCount,
	const FAPOProcessBufferParameters* pInputProcessParameters,
	uint32_t OutputProcessParameterCount,
	FAPOProcessBufferParameters* pOutputProcessParameters,
	uint8_t IsEnabled
) {
	FAPOBase_BeginProcess(&fapo->base);

	/* TODO */

	FAPOBase_EndProcess(&fapo->base);
}

void FAPOFXEcho_Free(void* fapo)
{
	FAPOFXEcho *echo = (FAPOFXEcho*) fapo;
	FAudio_free(echo->base.m_pParameterBlocks);
	FAudio_free(fapo);
}

/* Public API */

uint32_t FAPOFXCreateEcho(FAPO **pEffect)
{
	/* Allocate... */
	FAPOFXEcho *result = (FAPOFXEcho*) FAudio_malloc(
		sizeof(FAPOFXEcho)
	);
	uint8_t *params = (uint8_t*) FAudio_malloc(
		sizeof(FAPOFXEchoParameters) * 3
	);
	FAudio_zero(params, sizeof(FAPOFXEchoParameters) * 3);

	/* Initialize... */
	CreateFAPOBase(
		&result->base,
		&FXEchoProperties,
		params,
		sizeof(FAPOFXEchoParameters),
		1
	);

	/* Function table... */
	result->base.base.Process = (ProcessFunc)
		FAPOFXEcho_Process;
	result->base.Destructor = FAPOFXEcho_Free;

	/* Finally. */
	*pEffect = &result->base.base;
	return 0;
}
