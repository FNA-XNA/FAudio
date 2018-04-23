#include "XAudio2fx.h"
#include "XAPOBase.h"


class XAudio2VolumeMeter : public CXAPOParametersBase {
public:
	XAudio2VolumeMeter(UINT32 Flags) : XAudio2VolumeMeter() {
		FAudioCreateVolumeMeter(&fapo_object, Flags);
	}

	XAudio2VolumeMeter() : CXAPOParametersBase(reinterpret_cast<FAPOParametersBase *>(fapo_object)) {
	}

	FACOM_METHOD(void) Process(
		UINT32 InputProcessParameterCount,
		const XAPO_PROCESS_BUFFER_PARAMETERS* pInputProcessParameters,
		UINT32 OutputProcessParameterCount,
		XAPO_PROCESS_BUFFER_PARAMETERS* pOutputProcessParameters,
		BOOL IsEnabled) {
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

class XAudio2Reverb : public CXAPOParametersBase {
public:
	XAudio2Reverb(UINT32 Flags) : XAudio2Reverb() {
		FAudioCreateReverb(&fapo_object, Flags);
	}

	XAudio2Reverb() : CXAPOParametersBase(reinterpret_cast<FAPOParametersBase *>(fapo_object)) {
	}

	FACOM_METHOD(void) Process(
		UINT32 InputProcessParameterCount,
		const XAPO_PROCESS_BUFFER_PARAMETERS* pInputProcessParameters,
		UINT32 OutputProcessParameterCount,
		XAPO_PROCESS_BUFFER_PARAMETERS* pOutputProcessParameters,
		BOOL IsEnabled) {
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

void *CreateAudioVolumeMeterInternal() {
	return new XAudio2VolumeMeter(0);
}

void *CreateAudioReverbInternal() {
	return new XAudio2Reverb(0);
}
