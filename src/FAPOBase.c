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

#include "FAPOBase.h"

/* FAPOBase Interface */

void CreateFAPOBase(
	FAPOBase *fapo,
	const FAPORegistrationProperties *pRegistrationProperties
) {
}

int32_t FAPOBase_AddRef(FAPOBase *fapo)
{
	return 0;
}

int32_t FAPOBase_Release(FAPOBase *fapo)
{
	return 0;
}

/* FIXME: QueryInterface? Or just ignore COM garbage... -flibit */

uint32_t FAPOBase_GetRegistrationProperties(
	FAPOBase *fapo,
	FAPORegistrationProperties **ppRegistrationProperties
) {
	return 0;
}

uint32_t FAPOBase_IsInputFormatSupported(
	FAPOBase *fapo,
	const FAudioWaveFormatEx *pOutputFormat,
	const FAudioWaveFormatEx *pRequestedInputFormat,
	FAudioWaveFormatEx **ppSupportedInputFormat
) {
	return 0;
}

uint32_t FAPOBase_IsOutputFormatSupported(
	FAPOBase *fapo,
	const FAudioWaveFormatEx *pInputFormat,
	const FAudioWaveFormatEx *pRequestedOutputFormat,
	FAudioWaveFormatEx **ppSupportedOutputFormat
) {
	return 0;
}

uint32_t FAPOBase_Initialize(
	FAPOBase *fapo,
	const void* pData,
	uint32_t DataByteSize
) {
	return 0;
}

void FAPOBase_Reset(FAPOBase *fapo)
{
}

uint32_t FAPOBase_LockForProcess(
	FAPOBase *fapo,
	uint32_t InputLockedParameterCount,
	const FAPOLockForProcessBufferParameters *pInputLockedParameters,
	uint32_t OutputLockedParameterCount,
	const FAPOLockForProcessBufferParameters *pOutputLockedParameters
) {
	return 0;
}

void FAPOBase_UnlockForProcess(FAPOBase *fapo)
{
}

void FAPOBase_Process(
	FAPOBase *fapo,
	uint32_t InputProcessParameterCount,
	const FAPOProcessBufferParameters* pInputProcessParameters,
	uint32_t OutputProcessParameterCount,
	FAPOProcessBufferParameters* pOutputProcessParameters,
	uint8_t IsEnabled
) {
}

uint32_t FAPOBase_CalcInputFrames(FAPOBase *fapo, uint32_t OutputFrameCount)
{
	return OutputFrameCount;
}

uint32_t FAPOBase_CalcOutputFrames(FAPOBase *fapo, uint32_t InputFrameCount)
{
	return InputFrameCount;
}

uint32_t FAPOBase_ValidateFormatDefault(
	FAPOBase *fapo,
	FAudioWaveFormatEx *pFormat,
	uint8_t fOverwrite
) {
	return 0;
}

uint32_t FAPOBase_ValidateFormatPair(
	const FAudioWaveFormatEx *pSupportedFormat,
	FAudioWaveFormatEx *pRequestedFormat,
	uint8_t fOverwrite
) {
	return 0;
}

void FAPOBase_ProcessThru(
	void* pInputBuffer,
	float *pOutputBuffer,
	uint32_t FrameCount,
	uint16_t InputChannelCount,
	uint16_t OutputChannelCount,
	uint8_t MixWithOutput
) {
}

/* FAPOBaseParameters Interface */

void CreateFAPOBaseParameters(
	FAPOBaseParameters *fapoParameters,
	const FAPORegistrationProperties *pRegistrationProperties
) {
}

int32_t FAPOBaseParameters_AddRef(FAPOBaseParameters *fapoParameters)
{
	return 0;
}

int32_t FAPOBaseParameters_Release(FAPOBaseParameters *fapoParameters)
{
	return 0;
}

/* FIXME: QueryInterface? Or just ignore COM garbage... -flibit */

void FAPOBaseParameters_SetParameters(
	FAPOBaseParameters *fapoParameters,
	const void* pParameters,
	uint32_t ParameterByteSize
) {
}

void FAPOBaseParameters_GetParameters(
	FAPOBaseParameters *fapoParameters,
	const void* pParameters,
	uint32_t ParameterByteSize
) {
}

uint8_t FAPOBaseParameters_ParametersChanged(FAPOBaseParameters *fapoParameters)
{
	return 0;
}

uint8_t* FAPOBaseParameters_BeginProcess(FAPOBaseParameters *fapoParameters)
{
	return NULL;
}

void FAPOBaseParameters_EndProcess(FAPOBaseParameters *fapoParameters)
{
}
