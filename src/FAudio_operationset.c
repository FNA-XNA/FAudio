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

/* FAudio_operationset.c originally written by Tyler Glaiel */

#include "FAudio_internal.h"

/* Used by both Commit and Clear routines */

static inline void DeleteOperation(
	FAudio_OPERATIONSET_Operation *op,
	FAudioFreeFunc pFree
) {
	if (op->Type == FAUDIOOP_SETEFFECTPARAMETERS)
	{
		pFree(op->Data.SetEffectParameters.pParameters);
	}
	else if (op->Type == FAUDIOOP_SETCHANNELVOLUMES)
	{
		pFree(op->Data.SetChannelVolumes.pVolumes);
	}
	else if (op->Type == FAUDIOOP_SETOUTPUTMATRIX)
	{
		pFree(op->Data.SetOutputMatrix.pLevelMatrix);
	}
	pFree(op);
}

/* OperationSet Execution */

static inline void ExecuteOperation(FAudio_OPERATIONSET_Operation *op)
{
	switch (op->Type)
	{
	case FAUDIOOP_ENABLEEFFECT:
		FAudioVoice_EnableEffect(
			op->Data.EnableEffect.voice,
			op->Data.EnableEffect.EffectIndex,
			FAUDIO_COMMIT_NOW
		);
	break;

	case FAUDIOOP_DISABLEEFFECT:
		FAudioVoice_DisableEffect(
			op->Data.DisableEffect.voice,
			op->Data.DisableEffect.EffectIndex,
			FAUDIO_COMMIT_NOW
		);
	break;

	case FAUDIOOP_SETEFFECTPARAMETERS:
		FAudioVoice_SetEffectParameters(
			op->Data.SetEffectParameters.voice,
			op->Data.SetEffectParameters.EffectIndex,
			op->Data.SetEffectParameters.pParameters,
			op->Data.SetEffectParameters.ParametersByteSize,
			FAUDIO_COMMIT_NOW
		);
	break;

	case FAUDIOOP_SETFILTERPARAMETERS:
		FAudioVoice_SetFilterParameters(
			op->Data.SetFilterParameters.voice,
			&op->Data.SetFilterParameters.Parameters,
			FAUDIO_COMMIT_NOW
		);
	break;

	case FAUDIOOP_SETOUTPUTFILTERPARAMETERS:
		FAudioVoice_SetOutputFilterParameters(
			op->Data.SetOutputFilterParameters.voice,
			op->Data.SetOutputFilterParameters.pDestinationVoice,
			&op->Data.SetOutputFilterParameters.Parameters,
			FAUDIO_COMMIT_NOW
		);
	break;

	case FAUDIOOP_SETVOLUME:
		FAudioVoice_SetVolume(
			op->Data.SetVolume.voice,
			op->Data.SetVolume.Volume,
			FAUDIO_COMMIT_NOW
		);
	break;

	case FAUDIOOP_SETCHANNELVOLUMES:
		FAudioVoice_SetChannelVolumes(
			op->Data.SetChannelVolumes.voice,
			op->Data.SetChannelVolumes.Channels,
			op->Data.SetChannelVolumes.pVolumes,
			FAUDIO_COMMIT_NOW
		);
	break;

	case FAUDIOOP_SETOUTPUTMATRIX:
		FAudioVoice_SetOutputMatrix(
			op->Data.SetOutputMatrix.voice,
			op->Data.SetOutputMatrix.pDestinationVoice,
			op->Data.SetOutputMatrix.SourceChannels,
			op->Data.SetOutputMatrix.DestinationChannels,
			op->Data.SetOutputMatrix.pLevelMatrix,
			FAUDIO_COMMIT_NOW
		);
	break;

	case FAUDIOOP_START:
		FAudioSourceVoice_Start(
			op->Data.Start.voice,
			op->Data.Start.Flags,
			FAUDIO_COMMIT_NOW
		);
	break;

	case FAUDIOOP_STOP:
		FAudioSourceVoice_Stop(
			op->Data.Stop.voice,
			op->Data.Stop.Flags,
			FAUDIO_COMMIT_NOW
		);
	break;

	case FAUDIOOP_EXITLOOP:
		FAudioSourceVoice_ExitLoop(
			op->Data.ExitLoop.voice,
			FAUDIO_COMMIT_NOW
		);
	break;

	case FAUDIOOP_SETFREQUENCYRATIO:
		FAudioSourceVoice_SetFrequencyRatio(
			op->Data.SetFrequencyRatio.voice,
			op->Data.SetFrequencyRatio.Ratio,
			FAUDIO_COMMIT_NOW
		);
	break;

	default:
		FAudio_assert(0 && "Unrecognized operation type!");
	break;
	}
}

void FAudio_OPERATIONSET_CommitAll(FAudio *audio)
{
	FAudio_OPERATIONSET_Operation *op, *next;

	FAudio_PlatformLockMutex(audio->operationLock);
	LOG_MUTEX_LOCK(audio, audio->operationLock);

	op = audio->queuedOperations;
	while (op != NULL)
	{
		next = op->next;
		ExecuteOperation(op);
		DeleteOperation(op, audio->pFree);
		op = next;
	}
	audio->queuedOperations = NULL;

	FAudio_PlatformUnlockMutex(audio->operationLock);
	LOG_MUTEX_UNLOCK(audio, audio->operationLock);
}

void FAudio_OPERATIONSET_Commit(FAudio *audio, uint32_t OperationSet)
{
	FAudio_OPERATIONSET_Operation *op, *next, *prev;

	FAudio_PlatformLockMutex(audio->operationLock);
	LOG_MUTEX_LOCK(audio, audio->operationLock);

	op = audio->queuedOperations;
	prev = NULL;
	while (op != NULL)
	{
		next = op->next;
		if (op->OperationSet == OperationSet)
		{
			ExecuteOperation(op);
			if (prev == NULL) /* Start of linked list */
			{
				audio->queuedOperations = next;
			}
			else
			{
				prev->next = next;
			}
			DeleteOperation(op, audio->pFree);
		}
		else
		{
			prev = op;
		}
		op = next;
	}

	FAudio_PlatformUnlockMutex(audio->operationLock);
	LOG_MUTEX_UNLOCK(audio, audio->operationLock);
}

/* OperationSet Compilation */

static inline FAudio_OPERATIONSET_Operation* QueueOperation(
	FAudio_OPERATIONSET_Operation **start,
	FAudio_OPERATIONSET_Type type,
	uint32_t operationSet,
	FAudioMallocFunc pMalloc
) {
	FAudio_OPERATIONSET_Operation *latest;
	FAudio_OPERATIONSET_Operation *newop = pMalloc(sizeof(FAudio_OPERATIONSET_Operation));

	newop->Type = type;
	newop->OperationSet = operationSet;
	newop->next = NULL;

	if (*start == NULL)
	{
		*start = newop;
	}
	else
	{
		latest = *start;
		while (latest->next != NULL)
		{
			latest = latest->next;
		}
		latest->next = newop;
	}

	return newop;
}

void FAudio_OPERATIONSET_QueueEnableEffect(
	FAudioVoice *voice,
	uint32_t EffectIndex,
	uint32_t OperationSet
) {
	FAudio_OPERATIONSET_Operation *op;

	FAudio_PlatformLockMutex(voice->audio->operationLock);
	LOG_MUTEX_LOCK(voice->audio, voice->audio->operationLock)

	op = QueueOperation(
		&voice->audio->queuedOperations,
		FAUDIOOP_ENABLEEFFECT,
		OperationSet,
		voice->audio->pMalloc
	);

	op->Data.EnableEffect.voice = voice;
	op->Data.EnableEffect.EffectIndex = EffectIndex;

	FAudio_PlatformUnlockMutex(voice->audio->operationLock);
	LOG_MUTEX_UNLOCK(voice->audio, voice->audio->operationLock)
}

void FAudio_OPERATIONSET_QueueDisableEffect(
	FAudioVoice *voice,
	uint32_t EffectIndex,
	uint32_t OperationSet
) {
	FAudio_OPERATIONSET_Operation *op;

	FAudio_PlatformLockMutex(voice->audio->operationLock);
	LOG_MUTEX_LOCK(voice->audio, voice->audio->operationLock)

	op = QueueOperation(
		&voice->audio->queuedOperations,
		FAUDIOOP_DISABLEEFFECT,
		OperationSet,
		voice->audio->pMalloc
	);

	op->Data.DisableEffect.voice = voice;
	op->Data.DisableEffect.EffectIndex = EffectIndex;

	FAudio_PlatformUnlockMutex(voice->audio->operationLock);
	LOG_MUTEX_UNLOCK(voice->audio, voice->audio->operationLock)
}

void FAudio_OPERATIONSET_QueueSetEffectParameters(
	FAudioVoice *voice,
	uint32_t EffectIndex,
	const void *pParameters,
	uint32_t ParametersByteSize,
	uint32_t OperationSet
) {
	FAudio_OPERATIONSET_Operation *op;

	FAudio_PlatformLockMutex(voice->audio->operationLock);
	LOG_MUTEX_LOCK(voice->audio, voice->audio->operationLock)

	op = QueueOperation(
		&voice->audio->queuedOperations,
		FAUDIOOP_SETEFFECTPARAMETERS,
		OperationSet,
		voice->audio->pMalloc
	);

	op->Data.SetEffectParameters.voice = voice;
	op->Data.SetEffectParameters.EffectIndex = EffectIndex;
	op->Data.SetEffectParameters.pParameters = voice->audio->pMalloc(
		ParametersByteSize
	);
	FAudio_memcpy(
		op->Data.SetEffectParameters.pParameters,
		pParameters,
		ParametersByteSize
	);

	FAudio_PlatformUnlockMutex(voice->audio->operationLock);
	LOG_MUTEX_UNLOCK(voice->audio, voice->audio->operationLock)
}

void FAudio_OPERATIONSET_QueueSetFilterParameters(
	FAudioVoice *voice,
	const FAudioFilterParameters *pParameters,
	uint32_t OperationSet
) {
	FAudio_OPERATIONSET_Operation *op;

	FAudio_PlatformLockMutex(voice->audio->operationLock);
	LOG_MUTEX_LOCK(voice->audio, voice->audio->operationLock)

	op = QueueOperation(
		&voice->audio->queuedOperations,
		FAUDIOOP_SETFILTERPARAMETERS,
		OperationSet,
		voice->audio->pMalloc
	);

	op->Data.SetFilterParameters.voice = voice;
	FAudio_memcpy(
		&op->Data.SetFilterParameters.Parameters,
		pParameters,
		sizeof(FAudioFilterParameters)
	);

	FAudio_PlatformUnlockMutex(voice->audio->operationLock);
	LOG_MUTEX_UNLOCK(voice->audio, voice->audio->operationLock)
}

void FAudio_OPERATIONSET_QueueSetOutputFilterParameters(
	FAudioVoice *voice,
	FAudioVoice *pDestinationVoice,
	const FAudioFilterParameters *pParameters,
	uint32_t OperationSet
) {
	FAudio_OPERATIONSET_Operation *op;

	FAudio_PlatformLockMutex(voice->audio->operationLock);
	LOG_MUTEX_LOCK(voice->audio, voice->audio->operationLock)

	op = QueueOperation(
		&voice->audio->queuedOperations,
		FAUDIOOP_SETOUTPUTFILTERPARAMETERS,
		OperationSet,
		voice->audio->pMalloc
	);

	op->Data.SetOutputFilterParameters.voice = voice;
	op->Data.SetOutputFilterParameters.pDestinationVoice = pDestinationVoice;
	FAudio_memcpy(
		&op->Data.SetOutputFilterParameters.Parameters,
		pParameters,
		sizeof(FAudioFilterParameters)
	);

	FAudio_PlatformUnlockMutex(voice->audio->operationLock);
	LOG_MUTEX_UNLOCK(voice->audio, voice->audio->operationLock)
}

void FAudio_OPERATIONSET_QueueSetVolume(
	FAudioVoice *voice,
	float Volume,
	uint32_t OperationSet
) {
	FAudio_OPERATIONSET_Operation *op;

	FAudio_PlatformLockMutex(voice->audio->operationLock);
	LOG_MUTEX_LOCK(voice->audio, voice->audio->operationLock)

	op = QueueOperation(
		&voice->audio->queuedOperations,
		FAUDIOOP_SETVOLUME,
		OperationSet,
		voice->audio->pMalloc
	);

	op->Data.SetVolume.voice = voice;
	op->Data.SetVolume.Volume = Volume;

	FAudio_PlatformUnlockMutex(voice->audio->operationLock);
	LOG_MUTEX_UNLOCK(voice->audio, voice->audio->operationLock)
}

void FAudio_OPERATIONSET_QueueSetChannelVolumes(
	FAudioVoice *voice,
	uint32_t Channels,
	const float *pVolumes,
	uint32_t OperationSet
) {
	FAudio_OPERATIONSET_Operation *op;

	FAudio_PlatformLockMutex(voice->audio->operationLock);
	LOG_MUTEX_LOCK(voice->audio, voice->audio->operationLock)

	op = QueueOperation(
		&voice->audio->queuedOperations,
		FAUDIOOP_SETCHANNELVOLUMES,
		OperationSet,
		voice->audio->pMalloc
	);

	op->Data.SetChannelVolumes.voice = voice;
	op->Data.SetChannelVolumes.Channels = Channels;
	op->Data.SetChannelVolumes.pVolumes = voice->audio->pMalloc(
		sizeof(float) * Channels
	);
	FAudio_memcpy(
		op->Data.SetChannelVolumes.pVolumes,
		pVolumes,
		sizeof(float) * Channels
	);

	FAudio_PlatformUnlockMutex(voice->audio->operationLock);
	LOG_MUTEX_UNLOCK(voice->audio, voice->audio->operationLock)
}

void FAudio_OPERATIONSET_QueueSetOutputMatrix(
	FAudioVoice *voice,
	FAudioVoice *pDestinationVoice,
	uint32_t SourceChannels,
	uint32_t DestinationChannels,
	const float *pLevelMatrix,
	uint32_t OperationSet
) {
	FAudio_OPERATIONSET_Operation *op;

	FAudio_PlatformLockMutex(voice->audio->operationLock);
	LOG_MUTEX_LOCK(voice->audio, voice->audio->operationLock)

	op = QueueOperation(
		&voice->audio->queuedOperations,
		FAUDIOOP_SETOUTPUTMATRIX,
		OperationSet,
		voice->audio->pMalloc
	);

	op->Data.SetOutputMatrix.voice = voice;
	op->Data.SetOutputMatrix.pDestinationVoice = pDestinationVoice;
	op->Data.SetOutputMatrix.SourceChannels = SourceChannels;
	op->Data.SetOutputMatrix.DestinationChannels = DestinationChannels;
	op->Data.SetOutputMatrix.pLevelMatrix = voice->audio->pMalloc(
		sizeof(float) * SourceChannels * DestinationChannels
	);
	FAudio_memcpy(
		op->Data.SetOutputMatrix.pLevelMatrix,
		pLevelMatrix,
		sizeof(float) * SourceChannels * DestinationChannels
	);

	FAudio_PlatformUnlockMutex(voice->audio->operationLock);
	LOG_MUTEX_UNLOCK(voice->audio, voice->audio->operationLock)
}

void FAudio_OPERATIONSET_QueueStart(
	FAudioSourceVoice *voice,
	uint32_t Flags,
	uint32_t OperationSet
) {
	FAudio_OPERATIONSET_Operation *op;

	FAudio_PlatformLockMutex(voice->audio->operationLock);
	LOG_MUTEX_LOCK(voice->audio, voice->audio->operationLock)

	op = QueueOperation(
		&voice->audio->queuedOperations,
		FAUDIOOP_START,
		OperationSet,
		voice->audio->pMalloc
	);

	op->Data.Start.voice = voice;
	op->Data.Start.Flags = Flags;

	FAudio_PlatformUnlockMutex(voice->audio->operationLock);
	LOG_MUTEX_UNLOCK(voice->audio, voice->audio->operationLock)
}

void FAudio_OPERATIONSET_QueueStop(
	FAudioSourceVoice *voice,
	uint32_t Flags,
	uint32_t OperationSet
) {
	FAudio_OPERATIONSET_Operation *op;

	FAudio_PlatformLockMutex(voice->audio->operationLock);
	LOG_MUTEX_LOCK(voice->audio, voice->audio->operationLock)

	op = QueueOperation(
		&voice->audio->queuedOperations,
		FAUDIOOP_STOP,
		OperationSet,
		voice->audio->pMalloc
	);

	op->Data.Stop.voice = voice;
	op->Data.Stop.Flags = Flags;

	FAudio_PlatformUnlockMutex(voice->audio->operationLock);
	LOG_MUTEX_UNLOCK(voice->audio, voice->audio->operationLock)
}

void FAudio_OPERATIONSET_QueueExitLoop(
	FAudioSourceVoice *voice,
	uint32_t OperationSet
) {
	FAudio_OPERATIONSET_Operation *op;

	FAudio_PlatformLockMutex(voice->audio->operationLock);
	LOG_MUTEX_LOCK(voice->audio, voice->audio->operationLock)

	op = QueueOperation(
		&voice->audio->queuedOperations,
		FAUDIOOP_EXITLOOP,
		OperationSet,
		voice->audio->pMalloc
	);

	op->Data.ExitLoop.voice = voice;

	FAudio_PlatformUnlockMutex(voice->audio->operationLock);
	LOG_MUTEX_UNLOCK(voice->audio, voice->audio->operationLock)
}

void FAudio_OPERATIONSET_QueueSetFrequencyRatio(
	FAudioSourceVoice *voice,
	float Ratio,
	uint32_t OperationSet
) {
	FAudio_OPERATIONSET_Operation *op;

	FAudio_PlatformLockMutex(voice->audio->operationLock);
	LOG_MUTEX_LOCK(voice->audio, voice->audio->operationLock)

	op = QueueOperation(
		&voice->audio->queuedOperations,
		FAUDIOOP_SETFREQUENCYRATIO,
		OperationSet,
		voice->audio->pMalloc
	);

	op->Data.SetFrequencyRatio.voice = voice;
	op->Data.SetFrequencyRatio.Ratio = Ratio;

	FAudio_PlatformUnlockMutex(voice->audio->operationLock);
	LOG_MUTEX_UNLOCK(voice->audio, voice->audio->operationLock)
}

/* Called when releasing the engine */

void FAudio_OPERATIONSET_ClearAll(FAudio *audio)
{
	FAudio_OPERATIONSET_Operation *current, *next;

	FAudio_PlatformLockMutex(audio->operationLock);
	LOG_MUTEX_LOCK(audio, audio->operationLock);

	current = audio->queuedOperations;
	while (current != NULL)
	{
		next = current->next;
		DeleteOperation(current, audio->pFree);
		current = next;
	}
	audio->queuedOperations = NULL;

	FAudio_PlatformUnlockMutex(audio->operationLock);
	LOG_MUTEX_UNLOCK(audio, audio->operationLock)
}
