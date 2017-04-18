/* FACT - XACT Reimplementation for FNA
 * Copyright 2009-2017 Ethan Lee and the MonoGame Team
 *
 * Released under the Microsoft Public License.
 * See LICENSE for details.
 */

#include "FACT_internal.h"

uint32_t FACTCreateAudioEngine(
	uint32_t dwCreationFlags,
	FACTAudioEngine **ppEngine
) {
	*ppEngine = (FACTAudioEngine*) FACT_malloc(sizeof(FACTAudioEngine));
	if (*ppEngine == NULL)
	{
		return -1; /* TODO: E_OUTOFMEMORY */
	}
	FACT_zero(*ppEngine, sizeof(FACTAudioEngine));
	return 0;
}

uint32_t FACTAudioEngine_GetRendererCount(
	FACTAudioEngine *pEngine,
	uint16_t *pnRendererCount
) {
	return 0;
}

uint32_t FACTAudioEngine_GetRendererDetails(
	FACTAudioEngine *pEngine,
	uint16_t nRendererIndex,
	FACTRendererDetails *pRendererDetails
) {
	return 0;
}

uint32_t FACTAudioEngine_GetFinalMixFormat(
	FACTAudioEngine *pEngine,
	FACTWaveFormatExtensible *pFinalMixFormat
) {
	return 0;
}

uint32_t FACTAudioEngine_Initialize(
	FACTAudioEngine *pEngine,
	const FACTRuntimeParameters *pParams
) {
	uint32_t	categoryOffset,
			variableOffset,
			blob1Offset,
			categoryNameIndexOffset,
			blob2Offset,
			variableNameIndexOffset,
			categoryNameOffset,
			variableNameOffset,
			rpcOffset,
			dspPresetOffset,
			dspParameterOffset;
	uint16_t blob1Count, blob2Count;
	uint8_t *ptr = pParams->pGlobalSettingsBuffer;
	uint8_t *start = ptr;
	size_t memsize;
	uint16_t i, j;

	if (	read_u32(&ptr) != 0x46534758 /* 'XGSF' */ ||
		read_u16(&ptr) != FACT_CONTENT_VERSION ||
		read_u16(&ptr) != 42 /* Tool version */	)
	{
		return -1; /* TODO: NOT XACT FILE */
	}

	/* Unknown value */
	ptr += 2;

	/* Last modified, unused */
	ptr += 8;

	/* XACT Version */
	if (read_u8(&ptr) != 3)
	{
		return -1; /* TODO: VERSION TOO OLD */
	}

	/* Object counts */
	pEngine->categoryCount = read_u16(&ptr);
	pEngine->variableCount = read_u16(&ptr);
	blob1Count = read_u16(&ptr);
	blob2Count = read_u16(&ptr);
	pEngine->rpcCount = read_u16(&ptr);
	pEngine->dspPresetCount = read_u16(&ptr);
	pEngine->dspParameterCount = read_u16(&ptr);

	/* Object offsets */
	categoryOffset = read_u32(&ptr);
	variableOffset = read_u32(&ptr);
	blob1Offset = read_u32(&ptr);
	categoryNameIndexOffset = read_u32(&ptr);
	blob2Offset = read_u32(&ptr);
	variableNameIndexOffset = read_u32(&ptr);
	categoryNameOffset = read_u32(&ptr);
	variableNameOffset = read_u32(&ptr);
	rpcOffset = read_u32(&ptr);
	dspPresetOffset = read_u32(&ptr);
	dspParameterOffset = read_u32(&ptr);

	/* Category data */
	assert((ptr - start) == categoryOffset);
	memsize = sizeof(FACTAudioCategory) * pEngine->categoryCount;
	pEngine->categories = (FACTAudioCategory*) FACT_malloc(memsize);
	FACT_memcpy(pEngine->categories, ptr, memsize);
	ptr += memsize;

	/* Variable data */
	assert((ptr - start) == variableOffset);
	memsize = sizeof(FACTVariable) * pEngine->variableCount;
	pEngine->variables = (FACTVariable*) FACT_malloc(memsize);
	FACT_memcpy(pEngine->variables, ptr, memsize);
	ptr += memsize;

	/* RPC data */
	assert((ptr - start) == rpcOffset);
	pEngine->rpcs = (FACTRPC*) FACT_malloc(
		sizeof(FACTRPC) *
		pEngine->rpcCount
	);
	pEngine->rpcCodes = (uint8_t**) FACT_malloc(
		sizeof(uint8_t*) *
		pEngine->rpcCount
	);
	for (i = 0; i < pEngine->rpcCount; i += 1)
	{
		pEngine->rpcCodes[i] = ptr;
		pEngine->rpcs[i].variable = read_u16(&ptr);
		pEngine->rpcs[i].pointCount = read_u8(&ptr);
		pEngine->rpcs[i].parameter = read_u16(&ptr);
		pEngine->rpcs[i].points = (FACTRPCPoint*) FACT_malloc(
			sizeof(FACTRPCPoint) *
			pEngine->rpcs[i].pointCount
		);
		for (j = 0; j < pEngine->rpcs[i].pointCount; j += 1)
		{
			pEngine->rpcs[i].points[j].x = read_f32(&ptr);
			pEngine->rpcs[i].points[j].y = read_f32(&ptr);
			pEngine->rpcs[i].points[j].type = read_u8(&ptr);
		}
	}

	/* DSP Preset data */
	assert((ptr - start) == dspPresetOffset);
	pEngine->dspPresets = (FACTDSPPreset*) FACT_malloc(
		sizeof(FACTDSPPreset) *
		pEngine->dspPresetCount
	);
	pEngine->dspPresetCodes = (uint8_t**) FACT_malloc(
		sizeof(uint8_t*) *
		pEngine->dspPresetCount
	);
	for (i = 0; i < pEngine->dspPresetCount; i += 1)
	{
		pEngine->dspPresetCodes[i] = ptr;
		pEngine->dspPresets[i].accessibility = read_u8(&ptr);
		pEngine->dspPresets[i].parameterCount = read_u32(&ptr);
		pEngine->dspPresets[i].parameters = (FACTDSPParameter*) FACT_malloc(
			sizeof(FACTDSPParameter) *
			pEngine->dspPresets[i].parameterCount
		); /* This will be filled in just a moment... */
	}

	/* DSP Parameter data */
	assert((ptr - start) == dspParameterOffset);
	for (i = 0; i < pEngine->dspPresetCount; i += 1)
	{
		memsize = (
			sizeof(FACTDSPParameter) *
			pEngine->dspPresets[i].parameterCount
		);
		FACT_memcpy(
			pEngine->dspPresets[i].parameters,
			ptr,
			memsize
		);
		ptr += memsize;
	}

	/* Blob #1, no idea what this is... */
	assert((ptr - start) == blob1Offset);
	ptr += blob1Count * 2;

	/* Category Name Index data */
	assert((ptr - start) == categoryNameIndexOffset);
	ptr += pEngine->categoryCount * 6; /* FIXME: index as assert value? */

	/* Category Name data */
	assert((ptr - start) == categoryNameOffset);
	pEngine->categoryNames = (char**) FACT_malloc(
		sizeof(char*) *
		pEngine->categoryCount
	);
	for (i = 0; i < pEngine->categoryCount; i += 1)
	{
		memsize = FACT_strlen((char*) ptr) + 1; /* Dastardly! */
		pEngine->categoryNames[i] = (char*) FACT_malloc(memsize);
		FACT_memcpy(pEngine->categoryNames[i], ptr, memsize);
		ptr += memsize;
	}

	/* Blob #2, no idea what this is... */
	assert((ptr - start) == blob2Offset);
	ptr += blob2Count * 2;

	/* Variable Name Index data */
	assert((ptr - start) == variableNameIndexOffset);
	ptr += pEngine->variableCount * 6; /* FIXME: index as assert value? */

	/* Variable Name data */
	assert((ptr - start) == variableNameOffset);
	pEngine->variableNames = (char**) FACT_malloc(
		sizeof(char*) *
		pEngine->variableCount
	);
	for (i = 0; i < pEngine->variableCount; i += 1)
	{
		memsize = FACT_strlen((char*) ptr) + 1; /* Dastardly! */
		pEngine->variableNames[i] = (char*) FACT_malloc(memsize);
		FACT_memcpy(pEngine->variableNames[i], ptr, memsize);
		ptr += memsize;
	}

	/* Finally. */
	assert((ptr - start) == pParams->globalSettingsBufferSize);
	return 0;
}

uint32_t FACTAudioEngine_Shutdown(FACTAudioEngine *pEngine)
{
	int i;

	/* Category data */
	for (i = 0; i < pEngine->categoryCount; i += 1)
	{
		FACT_free(pEngine->categoryNames[i]);
	}
	FACT_free(pEngine->categoryNames);
	FACT_free(pEngine->categories);

	/* Variable data */
	for (i = 0; i < pEngine->variableCount; i += 1)
	{
		FACT_free(pEngine->variableNames[i]);
	}
	FACT_free(pEngine->variableNames);
	FACT_free(pEngine->variables);

	/* RPC data */
	for (i = 0; i < pEngine->rpcCount; i += 1)
	{
		FACT_free(pEngine->rpcs[i].points);
	}
	FACT_free(pEngine->rpcs);
	FACT_free(pEngine->rpcCodes);

	/* DSP data */
	for (i = 0; i < pEngine->dspPresetCount; i += 1)
	{
		FACT_free(pEngine->dspPresets[i].parameters);
	}
	FACT_free(pEngine->dspPresets);
	FACT_free(pEngine->dspPresetCodes);

	/* Finally. */
	FACT_free(pEngine);
	return 0;
}

uint32_t FACTAudioEngine_DoWork(FACTAudioEngine *pEngine)
{
	return 0;
}

uint32_t FACTAudioEngine_CreateSoundBank(
	FACTAudioEngine *pEngine,
	const void *pvBuffer,
	uint32_t dwSize,
	uint32_t dwFlags,
	uint32_t dwAllocAttributes,
	FACTSoundBank **ppSoundBank
) {
	return 0;
}

uint32_t FACTAudioEngine_CreateInMemoryWaveBank(
	FACTAudioEngine *pEngine,
	const void *pvBuffer,
	uint32_t dwSize,
	uint32_t dwFlags,
	uint32_t dwAllocAttributes,
	FACTWaveBank **ppWaveBank
) {
	return 0;
}

uint32_t FACTAudioEngine_CreateStreamingWaveBank(
	FACTAudioEngine *pEngine,
	const FACTStreamingParameters *pParms,
	FACTWaveBank **ppWaveBank
) {
	return 0;
}

uint32_t FACTAudioEngine_PrepareWave(
	FACTAudioEngine *pEngine,
	uint32_t dwFlags,
	const char *szWavePath,
	uint32_t wStreamingPacketSize,
	uint32_t dwAlignment,
	uint32_t dwPlayOffset,
	uint8_t nLoopCount,
	FACTWave **ppWave
) {
	return 0;
}

uint32_t FACTAudioEngine_PrepareInMemoryWave(
	FACTAudioEngine *pEngine,
	uint32_t dwFlags,
	FACTWaveBankEntry entry,
	uint32_t *pdwSeekTable, /* Optional! */
	uint8_t *pbWaveData,
	uint32_t dwPlayOffset,
	uint8_t nLoopCount,
	FACTWave **ppWave
) {
	return 0;
}

uint32_t FACTAudioEngine_PrepareStreamingWave(
	FACTAudioEngine *pEngine,
	uint32_t dwFlags,
	FACTWaveBankEntry entry,
	FACTStreamingParameters streamingParams,
	uint32_t dwAlignment,
	uint32_t *pdwSeekTable, /* Optional! */
	uint8_t *pbWaveData,
	uint32_t dwPlayOffset,
	uint8_t nLoopCount,
	FACTWave **ppWave
) {
	return 0;
}

uint32_t FACTAudioEngine_RegisterNotification(
	FACTAudioEngine *pEngine,
	const FACTNotificationDescription *pNotificationDescription
) {
	return 0;
}

uint32_t FACTAudioEngine_UnRegisterNotification(
	FACTAudioEngine *pEngine,
	const FACTNotificationDescription *pNotificationDescription
) {
	return 0;
}

uint16_t FACTAudioEngine_GetCategory(
	FACTAudioEngine *pEngine,
	const char *szFriendlyName
) {
	return 0;
}

uint32_t FACTAudioEngine_Stop(
	FACTAudioEngine *pEngine,
	uint16_t nCategory,
	uint32_t dwFlags
) {
	return 0;
}

uint32_t FACTAudioEngine_SetVolume(
	FACTAudioEngine *pEngine,
	uint16_t nCategory,
	uint32_t dwFlags
) {
	return 0;
}

uint32_t FACTAudioEngine_Pause(
	FACTAudioEngine *pEngine,
	uint16_t nCategory,
	int32_t fPause
) {
	return 0;
}

uint16_t FACTAudioEngine_GetGlobalVariableIndex(
	FACTAudioEngine *pEngine,
	const char *szFriendlyName
) {
	return 0;
}

uint32_t FACTAudioEngine_SetGlobalVariable(
	FACTAudioEngine *pEngine,
	uint16_t nIndex,
	float nValue
) {
	return 0;
}

uint32_t FACTAudioEngine_GetGlobalVariable(
	FACTAudioEngine *pEngine,
	uint16_t nIndex,
	float *pnValue
) {
	return 0;
}
