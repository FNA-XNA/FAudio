#include "XAudio2fx.h"
#include "XAPOBase.h"

#include <FAudioFX.h>


class XAudio2VolumeMeter : public CXAPOParametersBase 
{
public:
	XAudio2VolumeMeter(void *object) : fapo_object(object),
									   CXAPOParametersBase(reinterpret_cast<FAPOParametersBase *>(object)) 
	{
	}

	COM_METHOD(void) Process(
		UINT32 InputProcessParameterCount,
		const XAPO_PROCESS_BUFFER_PARAMETERS* pInputProcessParameters,
		UINT32 OutputProcessParameterCount,
		XAPO_PROCESS_BUFFER_PARAMETERS* pOutputProcessParameters,
		BOOL IsEnabled
	) {
		reinterpret_cast<FAPO *>(fapo_object)->Process(
			fapo_object,
			InputProcessParameterCount,
			pInputProcessParameters,
			OutputProcessParameterCount,
			pOutputProcessParameters,
			IsEnabled);
	}

private:
	void *fapo_object;
};

class XAudio2Reverb : public CXAPOParametersBase 
{
public:
	XAudio2Reverb(void *object) : fapo_object(object),
								  CXAPOParametersBase(reinterpret_cast<FAPOParametersBase *>(object)) 
	{
	}

	COM_METHOD(void) Process(
		UINT32 InputProcessParameterCount,
		const XAPO_PROCESS_BUFFER_PARAMETERS* pInputProcessParameters,
		UINT32 OutputProcessParameterCount,
		XAPO_PROCESS_BUFFER_PARAMETERS* pOutputProcessParameters,
		BOOL IsEnabled
	) {
		reinterpret_cast<FAPO *>(fapo_object)->Process(
			fapo_object,
			InputProcessParameterCount,
			pInputProcessParameters,
			OutputProcessParameterCount,
			pOutputProcessParameters,
			IsEnabled);
	}

private:
	void *fapo_object;
};



///////////////////////////////////////////////////////////////////////////////
//
// Create functions
//

void *CreateAudioVolumeMeterInternal() 
{
	void *fapo_object = NULL;
	FAudioCreateVolumeMeter(&fapo_object, 0);
	return new XAudio2VolumeMeter(fapo_object);
}

void *CreateAudioReverbInternal() 
{
	void *fapo_object = NULL;
	FAudioCreateReverb(&fapo_object, 0);
	return new XAudio2Reverb(fapo_object);
}

#if XAUDIO2_VERSION >=8

FAUDIOCPP_API CreateAudioVolumeMeter(class IUnknown** ppApo) 
{
	*ppApo = reinterpret_cast<IUnknown *> (CreateAudioVolumeMeterInternal());
	return S_OK;
}

FAUDIOCPP_API CreateAudioReverb(class IUnknown** ppApo) 
{
	*ppApo = reinterpret_cast<IUnknown *> (CreateAudioReverbInternal());
	return S_OK;
}

#endif // XAUDIO2_VERSION >=8
