#ifndef FAUDIO_OPERATIONSET_H
#define FAUDIO_OPERATIONSET_H


#include "FAudio.h"
//OperationSet stuff by Tyler Glaiel
//list of OperationSet-supported functions from https://docs.microsoft.com/en-us/windows/desktop/xaudio2/xaudio2-operation-sets

typedef enum {
    FAUDIOOP_EXITLOOP,
    FAUDIOOP_SETFILTERPARAMETERS,
    FAUDIOOP_SETFREQUENCYRATIO,
    FAUDIOOP_DISABLEEFFECT,
    FAUDIOOP_ENABLEEFFECT,
    FAUDIOOP_SETCHANNELVOLUMES,
    FAUDIOOP_SETEFFECTPARAMETERS,
    FAUDIOOP_SETOUTPUTMATRIX,
    FAUDIOOP_SETVOLUME,
    FAUDIOOP_START,
    FAUDIOOP_STOP
} FAudioOp_Type; 


typedef struct {
    FAudioSourceVoice *voice;
} FAudioOpData_ExitLoop;

typedef struct {
    FAudioVoice *voice;
    const FAudioFilterParameters *pParameters;
} FAudioOpData_SetFilterParameters;

typedef struct {
    FAudioSourceVoice *voice;
    float Ratio;
} FAudioOpData_SetFrequencyRatio;

typedef struct {
    FAudioVoice *voice;
    uint32_t EffectIndex;
} FAudioOpData_DisableEffect;

typedef struct {
    FAudioVoice *voice;
    uint32_t EffectIndex;
} FAudioOpData_EnableEffect;

typedef struct {
    FAudioVoice *voice;
    uint32_t Channels;
    const float *pVolumes;
} FAudioOpData_SetChannelVolumes;

typedef struct {
    FAudioVoice *voice;
    uint32_t EffectIndex;
    const void *pParameters;
    uint32_t ParametersByteSize;
} FAudioOpData_SetEffectParameters;

typedef struct {
    FAudioVoice *voice;
    FAudioVoice *pDestinationVoice;
    uint32_t SourceChannels;
    uint32_t DestinationChannels;
    const float *pLevelMatrix;
} FAudioOpData_SetOutputMatrix;

typedef struct {
    FAudioVoice *voice;
    float Volume;
} FAudioOpData_SetVolume;

typedef struct {
    FAudioSourceVoice *voice;
    uint32_t Flags;
} FAudioOpData_Start;

typedef struct {
    FAudioSourceVoice *voice;
    uint32_t Flags;
} FAudioOpData_Stop;

struct FAudioOp_QueuedOperation;
typedef struct FAudioOp_QueuedOperation FAudioOp_QueuedOperation;
typedef struct FAudioOp_QueuedOperation {
    FAudioOp_Type operation;
    uint32_t OperationSet;

    union {
        FAudioOpData_ExitLoop ExitLoop;
        FAudioOpData_SetFilterParameters SetFilterParameters;
        FAudioOpData_SetFrequencyRatio SetFrequencyRatio;
        FAudioOpData_DisableEffect DisableEffect;
        FAudioOpData_EnableEffect EnableEffect;
        FAudioOpData_SetChannelVolumes SetChannelVolumes;
        FAudioOpData_SetEffectParameters SetEffectParameters;
        FAudioOpData_SetOutputMatrix SetOutputMatrix;
        FAudioOpData_SetVolume SetVolume;
        FAudioOpData_Start Start;
        FAudioOpData_Stop Stop;
    } opdata;

    FAudioOp_QueuedOperation* next;
};

void FAudioOp_ExecuteOperation(FAudioOp_QueuedOperation* op);


//construction helpers
void FAudioOp_Build_ExitLoop(FAudioOp_QueuedOperation* dst, 
    FAudioSourceVoice *voice,
    uint32_t OperationSet
);

void FAudioOp_Build_SetFilterParameters(FAudioOp_QueuedOperation* dst, 
    FAudioVoice *voice,
    const FAudioFilterParameters *pParameters,
    uint32_t OperationSet
);

void FAudioOp_Build_SetFrequencyRatio(FAudioOp_QueuedOperation* dst, 
    FAudioSourceVoice *voice,
    float Ratio,
    uint32_t OperationSet
);

void FAudioOp_Build_DisableEffect(FAudioOp_QueuedOperation* dst, 
    FAudioVoice *voice,
    uint32_t EffectIndex,
    uint32_t OperationSet
);

void FAudioOp_Build_EnableEffect(FAudioOp_QueuedOperation* dst, 
    FAudioVoice *voice,
    uint32_t EffectIndex,
    uint32_t OperationSet
);

void FAudioOp_Build_SetChannelVolumes(FAudioOp_QueuedOperation* dst, 
    FAudioVoice *voice,
    uint32_t Channels,
    const float *pVolumes,
    uint32_t OperationSet
);

void FAudioOp_Build_SetEffectParameters(FAudioOp_QueuedOperation* dst, 
    FAudioVoice *voice,
    uint32_t EffectIndex,
    const void *pParameters,
    uint32_t ParametersByteSize,
    uint32_t OperationSet
);

void FAudioOp_Build_SetOutputMatrix(FAudioOp_QueuedOperation* dst, 
    FAudioVoice *voice,
    FAudioVoice *pDestinationVoice,
    uint32_t SourceChannels,
    uint32_t DestinationChannels,
    const float *pLevelMatrix,
    uint32_t OperationSet
);

void FAudioOp_Build_SetVolume(FAudioOp_QueuedOperation* dst, 
    FAudioVoice *voice,
    float Volume,
    uint32_t OperationSet
);

void FAudioOp_Build_Start(FAudioOp_QueuedOperation* dst, 
    FAudioSourceVoice *voice,
    uint32_t Flags,
    uint32_t OperationSet
);

void FAudioOp_Build_Stop(FAudioOp_QueuedOperation* dst, 
    FAudioSourceVoice *voice,
    uint32_t Flags,
    uint32_t OperationSet
);


///////////////////
FAudioOp_QueuedOperation* FAudioOp_QueueOperation(FAudioOp_QueuedOperation** start, FAudioMallocFunc pMalloc);
void FAudioOp_DeleteOperation(FAudioOp_QueuedOperation** previous, FAudioOp_QueuedOperation* current, FAudioFreeFunc pFree);
void FAudioOp_ProcessOperations(FAudioOp_QueuedOperation** start, uint32_t OperationSet, FAudioFreeFunc pFree);
void FAudioOp_ClearAll(FAudioOp_QueuedOperation** start, FAudioFreeFunc pFree);
#endif
