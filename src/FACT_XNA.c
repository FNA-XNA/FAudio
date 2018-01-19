/* FACT - XACT Reimplementation for FNA
 * Copyright 2009-2018 Ethan Lee and the MonoGame Team
 *
 * Released under the Microsoft Public License.
 * See LICENSE for details.
 */

#include "FACT_internal.h"

void FACT_XNA_Init()
{
	/* TODO */
}

void FACT_XNA_Quit()
{
	/* TODO */
}

float FACT_XNA_GetMasterVolume()
{
	/* TODO */
	return 1.0f;
}

void FACT_XNA_SetMasterVolume(float volume)
{
	/* TODO */
}

float FACT_XNA_GetDistanceScale()
{
	/* TODO */
	return 1.0f;
}

void FACT_XNA_SetDistanceScale(float scale)
{
	/* TODO */
}

float FACT_XNA_GetDopplerScale()
{
	/* TODO */
	return 1.0f;
}

void FACT_XNA_SetDopplerScale(float scale)
{
	/* TODO */
}

float FACT_XNA_GetSpeedOfSound()
{
	/* TODO */
	return 0.0f;
}

void FACT_XNA_SetSpeedOfSound(float speedOfSound)
{
	/* TODO */
}

FACTXNABuffer* FACT_XNA_GenBuffer(
	uint8_t *buffer,
	int32_t offset,
	int32_t count,
	int32_t sampleRate,
	int32_t channels,
	int32_t loopStart,
	int32_t loopLength,
	uint16_t format,
	uint32_t formatParameter
) {
	/* TODO */
	return NULL;
}

void FACT_XNA_DisposeBuffer(FACTXNABuffer *buffer)
{
	/* TODO */
}

int32_t FACT_XNA_GetBufferDuration(FACTXNABuffer *buffer)
{
	/* TODO */
	return 0;
}

uint32_t FACT_XNA_PlayBuffer(
	FACTXNABuffer *buffer,
	float volume,
	float pitch,
	float pan
) {
	/* TODO */
	return 0;
}

FACTXNASource* FACT_XNA_GenSource(FACTXNABuffer *buffer)
{
	/* TODO */
	return NULL;
}

FACTXNASource* FACT_XNA_GenDynamicSource(
	int32_t sampleRate,
	int32_t channels
) {
	return NULL;
}

void FACT_XNA_DisposeSource(FACTXNASource *source)
{
	/* TODO */
}

int32_t FACT_XNA_GetSourceState(FACTXNASource *source)
{
	/* TODO */
	return 0;
}

int32_t FACT_XNA_GetSourceLooped(FACTXNASource *source)
{
	/* TODO */
	return 0;
}

void FACT_XNA_SetSourceLooped(
	FACTXNASource *source,
	int32_t looped
) {
	/* TODO */
}

float FACT_XNA_GetSourcePan(FACTXNASource *source)
{
	/* TODO */
	return 0.0f;
}

void FACT_XNA_SetSourcePan(
	FACTXNASource *source,
	float pan
) {
	/* TODO */
}

float FACT_XNA_GetSourcePitch(FACTXNASource *source)
{
	/* TODO */
	return 1.0f;
}

void FACT_XNA_SetSourcePitch(
	FACTXNASource *source,
	float pitch
) {
	/* TODO */
}

float FACT_XNA_GetSourceVolume(FACTXNASource *source)
{
	/* TODO */
	return 1.0f;
}

void FACT_XNA_SetSourceVolume(
	FACTXNASource *source,
	float volume
) {
	/* TODO */
}

void FACT_XNA_ApplySource3D(
	FACTXNASource *source,
	FACT3DAUDIO_LISTENER *listener,
	FACT3DAUDIO_EMITTER *emitter
) {
	/* TODO */
}

void FACT_XNA_PlaySource(FACTXNASource *source)
{
	/* TODO */
}

void FACT_XNA_PauseSource(FACTXNASource *source)
{
	/* TODO */
}

void FACT_XNA_ResumeSource(FACTXNASource *source)
{
	/* TODO */
}

void FACT_XNA_StopSource(
	FACTXNASource *source,
	int32_t immediate
) {
	/* TODO */
}

void FACT_XNA_ApplySourceReverb(
	FACTXNASource *source,
	float rvGain
) {
	/* TODO */
}

void FACT_XNA_ApplySourceLowPass(
	FACTXNASource *source,
	float hfGain
) {
	/* TODO */
}

void FACT_XNA_ApplySourceHighPass(
	FACTXNASource *source,
	float lfGain
) {
	/* TODO */
}

void FACT_XNA_ApplySourceBandPass(
	FACTXNASource *source,
	float hfGain,
	float lfGain
) {
	/* TODO */
}

int32_t FACT_XNA_GetSourceBufferCount(FACTXNASource *source)
{
	/* TODO */
	return 0;
}

void FACT_XNA_QueueSourceBufferB(
	FACTXNASource *source,
	uint8_t *buffer,
	int32_t offset,
	int32_t count
) {
	/* TODO */
}

void FACT_XNA_QueueSourceBufferF(
	FACTXNASource *source,
	float *buffer,
	int32_t offset,
	int32_t count
) {
	/* TODO */
}

FACTXNASong* FACT_XNA_GenSong(const char* name)
{
	/* TODO */
	return NULL;
}

void FACT_XNA_DisposeSong(FACTXNASong *song)
{
	/* TODO */
}

void FACT_XNA_PlaySong(FACTXNASong *song)
{
	/* TODO */
}

void FACT_XNA_PauseSong(FACTXNASong *song)
{
	/* TODO */
}

void FACT_XNA_ResumeSong(FACTXNASong *song)
{
	/* TODO */
}

void FACT_XNA_StopSong(FACTXNASong *song)
{
	/* TODO */
}

void FACT_XNA_SetSongVolume(
	FACTXNASong *song,
	float volume
) {
	/* TODO */
}

uint32_t FACT_XNA_GetSongEnded(FACTXNASong *song)
{
	/* TODO */
	return 0;
}
