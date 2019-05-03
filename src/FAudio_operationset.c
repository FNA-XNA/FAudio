#include "FAudio_operationset.h"
#include "FAudio_internal.h"
#include "FAudio.h"
//OperationSet stuff by Tyler Glaiel

void FAudioOp_ExecuteOperation(FAudioOp_QueuedOperation* op){
    switch(op->operation){
        case FAUDIOOP_EXITLOOP:
            FAudioSourceVoice_ExitLoop(
                op->opdata.ExitLoop.voice, 
                FAUDIO_COMMIT_NOW
            );
        break;

        case FAUDIOOP_SETFILTERPARAMETERS:
            FAudioVoice_SetFilterParameters(
                op->opdata.SetFilterParameters.voice, 
                &op->opdata.SetFilterParameters.pParameters,
                FAUDIO_COMMIT_NOW
            );
        break;

        case FAUDIOOP_SETFREQUENCYRATIO:
            FAudioSourceVoice_SetFrequencyRatio(
                op->opdata.SetFrequencyRatio.voice, 
                op->opdata.SetFrequencyRatio.Ratio,
                FAUDIO_COMMIT_NOW
            );
        break;

        case FAUDIOOP_DISABLEEFFECT:
            FAudioVoice_DisableEffect(
                op->opdata.DisableEffect.voice, 
                op->opdata.DisableEffect.EffectIndex, 
                FAUDIO_COMMIT_NOW
            );
        break;

        case FAUDIOOP_ENABLEEFFECT:
            FAudioVoice_EnableEffect(
                op->opdata.EnableEffect.voice, 
                op->opdata.EnableEffect.EffectIndex, 
                FAUDIO_COMMIT_NOW
            );
        break;

        case FAUDIOOP_SETCHANNELVOLUMES:
            FAudioVoice_SetChannelVolumes(
                op->opdata.SetChannelVolumes.voice, 
                op->opdata.SetChannelVolumes.Channels, 
                op->opdata.SetChannelVolumes.pVolumes, 
                FAUDIO_COMMIT_NOW
            );
        break;

        case FAUDIOOP_SETEFFECTPARAMETERS:
            FAudioVoice_SetEffectParameters(
                op->opdata.SetEffectParameters.voice, 
                op->opdata.SetEffectParameters.EffectIndex, 
                op->opdata.SetEffectParameters.pParameters, 
                op->opdata.SetEffectParameters.ParametersByteSize, 
                FAUDIO_COMMIT_NOW
            );
        break;

        case FAUDIOOP_SETOUTPUTMATRIX:
            FAudioVoice_SetOutputMatrix(
                op->opdata.SetOutputMatrix.voice, 
                op->opdata.SetOutputMatrix.pDestinationVoice, 
                op->opdata.SetOutputMatrix.SourceChannels, 
                op->opdata.SetOutputMatrix.DestinationChannels, 
                op->opdata.SetOutputMatrix.pLevelMatrix, 
                FAUDIO_COMMIT_NOW
            );
        break;

        case FAUDIOOP_SETVOLUME:
            FAudioVoice_SetVolume(
                op->opdata.SetVolume.voice, 
                op->opdata.SetVolume.Volume, 
                FAUDIO_COMMIT_NOW
            );
        break;

        case FAUDIOOP_START:
            FAudioSourceVoice_Start(
                op->opdata.Start.voice, 
                op->opdata.Start.Flags, 
                FAUDIO_COMMIT_NOW
            );
        break;

        case FAUDIOOP_STOP:
            FAudioSourceVoice_Stop(
                op->opdata.Stop.voice, 
                op->opdata.Stop.Flags, 
                FAUDIO_COMMIT_NOW
            );
        break;
    }
}




void FAudioOp_Build_ExitLoop(FAudioOp_QueuedOperation* dst, 
    FAudioSourceVoice *voice,
    uint32_t OperationSet
){
    dst->operation = FAUDIOOP_EXITLOOP;
    dst->OperationSet = OperationSet;
    dst->opdata.ExitLoop.voice = voice;
}

void FAudioOp_Build_SetFilterParameters(FAudioOp_QueuedOperation* dst, 
    FAudioVoice *voice,
    const FAudioFilterParameters *pParameters,
    uint32_t OperationSet
){
    dst->operation = FAUDIOOP_SETFILTERPARAMETERS;
    dst->OperationSet = OperationSet;
    dst->opdata.SetFilterParameters.voice = voice;
    dst->opdata.SetFilterParameters.pParameters = *pParameters; //copy it
}

void FAudioOp_Build_SetFrequencyRatio(FAudioOp_QueuedOperation* dst, 
    FAudioSourceVoice *voice,
    float Ratio,
    uint32_t OperationSet
){
    dst->operation = FAUDIOOP_SETFREQUENCYRATIO;
    dst->OperationSet = OperationSet;
    dst->opdata.SetFrequencyRatio.voice = voice;
    dst->opdata.SetFrequencyRatio.Ratio = Ratio;
}

void FAudioOp_Build_DisableEffect(FAudioOp_QueuedOperation* dst, 
    FAudioVoice *voice,
    uint32_t EffectIndex,
    uint32_t OperationSet
){
    dst->operation = FAUDIOOP_DISABLEEFFECT;
    dst->OperationSet = OperationSet;
    dst->opdata.DisableEffect.voice = voice;
    dst->opdata.DisableEffect.EffectIndex = EffectIndex;
}

void FAudioOp_Build_EnableEffect(FAudioOp_QueuedOperation* dst, 
    FAudioVoice *voice,
    uint32_t EffectIndex,
    uint32_t OperationSet
){
    dst->operation = FAUDIOOP_ENABLEEFFECT;
    dst->OperationSet = OperationSet;
    dst->opdata.EnableEffect.voice = voice;
    dst->opdata.EnableEffect.EffectIndex = EffectIndex;
}

void FAudioOp_Build_SetChannelVolumes(FAudioOp_QueuedOperation* dst, 
    FAudioVoice *voice,
    uint32_t Channels,
    const float *pVolumes,
    uint32_t OperationSet
){
    dst->operation = FAUDIOOP_SETCHANNELVOLUMES;
    dst->OperationSet = OperationSet;
    dst->opdata.SetChannelVolumes.voice = voice;
    dst->opdata.SetChannelVolumes.Channels = Channels;
    dst->opdata.SetChannelVolumes.pVolumes = voice->audio->pMalloc(sizeof(float)*Channels);
    FAudio_memcpy(dst->opdata.SetChannelVolumes.pVolumes, pVolumes, sizeof(float)*Channels);
}

void FAudioOp_Build_SetEffectParameters(FAudioOp_QueuedOperation* dst, 
    FAudioVoice *voice,
    uint32_t EffectIndex,
    const void *pParameters,
    uint32_t ParametersByteSize,
    uint32_t OperationSet
){
    dst->operation = FAUDIOOP_SETEFFECTPARAMETERS;
    dst->OperationSet = OperationSet;
    dst->opdata.SetEffectParameters.voice = voice;
    dst->opdata.SetEffectParameters.EffectIndex = EffectIndex;
    dst->opdata.SetEffectParameters.pParameters = pParameters;
    dst->opdata.SetEffectParameters.ParametersByteSize = ParametersByteSize;
}

void FAudioOp_Build_SetOutputMatrix(FAudioOp_QueuedOperation* dst, 
    FAudioVoice *voice,
    FAudioVoice *pDestinationVoice,
    uint32_t SourceChannels,
    uint32_t DestinationChannels,
    const float *pLevelMatrix,
    uint32_t OperationSet
){
    dst->operation = FAUDIOOP_SETOUTPUTMATRIX;
    dst->OperationSet = OperationSet;
    dst->opdata.SetOutputMatrix.voice = voice;
    dst->opdata.SetOutputMatrix.pDestinationVoice = pDestinationVoice;
    dst->opdata.SetOutputMatrix.SourceChannels = SourceChannels;
    dst->opdata.SetOutputMatrix.DestinationChannels = DestinationChannels;
    dst->opdata.SetOutputMatrix.pLevelMatrix = voice->audio->pMalloc(sizeof(float)*SourceChannels*DestinationChannels);
    FAudio_memcpy(dst->opdata.SetOutputMatrix.pLevelMatrix, pLevelMatrix, sizeof(float)*SourceChannels*DestinationChannels);
}

void FAudioOp_Build_SetVolume(FAudioOp_QueuedOperation* dst, 
    FAudioVoice *voice,
    float Volume,
    uint32_t OperationSet
){
    dst->operation = FAUDIOOP_SETVOLUME;
    dst->OperationSet = OperationSet;
    dst->opdata.SetVolume.voice = voice;
    dst->opdata.SetVolume.Volume = Volume;
}

void FAudioOp_Build_Start(FAudioOp_QueuedOperation* dst, 
    FAudioSourceVoice *voice,
    uint32_t Flags,
    uint32_t OperationSet
){
    dst->operation = FAUDIOOP_START;
    dst->OperationSet = OperationSet;
    dst->opdata.Start.voice = voice;
    dst->opdata.Start.Flags = Flags;
}

void FAudioOp_Build_Stop(FAudioOp_QueuedOperation* dst, 
    FAudioSourceVoice *voice,
    uint32_t Flags,
    uint32_t OperationSet
){
    dst->operation = FAUDIOOP_STOP;
    dst->OperationSet = OperationSet;
    dst->opdata.Stop.voice = voice;
    dst->opdata.Stop.Flags = Flags;
}


FAudioOp_QueuedOperation* FAudioOp_QueueOperation(FAudioOp_QueuedOperation** start, FAudioMallocFunc pMalloc){
    FAudioOp_QueuedOperation* newop = pMalloc(sizeof(FAudioOp_QueuedOperation));
    FAudioOp_QueuedOperation* latest;

    FAudio_zero(newop, sizeof(FAudioOp_QueuedOperation));

    if(*start == NULL){
        *start = newop;
    } else {
        latest = *start;
        while (latest->next != NULL)
        {
            latest = latest->next;
        }
        latest->next = newop;
    }

    return newop;
}

void FAudioOp_DeleteOperation(FAudioOp_QueuedOperation** previous, FAudioOp_QueuedOperation* current, FAudioFreeFunc pFree){
    if(*previous == current){ //start of linked list
        *previous = current->next;
    } else {
        (*previous)->next = current->next;
    }

    if(current->operation == FAUDIOOP_SETCHANNELVOLUMES){
        pFree(current->opdata.SetChannelVolumes.pVolumes);
    }
    if(current->operation == FAUDIOOP_SETOUTPUTMATRIX){
        pFree(current->opdata.SetOutputMatrix.pLevelMatrix);
    }

    pFree(current);
}


void FAudioOp_ProcessOperations(FAudioOp_QueuedOperation** start, uint32_t OperationSet, FAudioFreeFunc pFree){
    FAudioOp_QueuedOperation** prev = start;
    FAudioOp_QueuedOperation* current = *start;
    FAudioOp_QueuedOperation* next;

    if(*start == NULL) return;


    while(current){
        if(OperationSet == FAUDIO_COMMIT_ALL || current->OperationSet == OperationSet){
            FAudioOp_ExecuteOperation(current);

            next = current->next;
            FAudioOp_DeleteOperation(prev, current, pFree);

            current = next;
        } else {
            prev = &current;
            current = current->next;
        }
    }
}

void FAudioOp_ClearAll(FAudioOp_QueuedOperation** start, FAudioFreeFunc pFree){
    FAudioOp_QueuedOperation* current = *start;
    FAudioOp_QueuedOperation* next;

    while(current){
        next = current->next;

        if(current->operation == FAUDIOOP_SETCHANNELVOLUMES){
            pFree(current->opdata.SetChannelVolumes.pVolumes);
        }
        if(current->operation == FAUDIOOP_SETOUTPUTMATRIX){
            pFree(current->opdata.SetOutputMatrix.pLevelMatrix);
        }

        pFree(current);
        current = next;
    }

    *start = NULL;
}
