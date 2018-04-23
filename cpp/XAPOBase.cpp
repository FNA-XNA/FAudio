#include "XAPOBase.h"

static const IID IID_IXAPO = { 0xA90BC001, 0xE897, 0xE897, {0x55, 0xE4, 0x9E, 0x47, 0x00, 0x00, 0x00, 0x00 } };
static const IID IID_IXAPOParameters = { 0xA90BC001, 0xE897, 0xE897, {0x55, 0xE4, 0x9E, 0x47, 0x00, 0x00, 0x00, 0x01 } };

///////////////////////////////////////////////////////////////////////////////
//
// CXAPOBase
//

CXAPOBase::CXAPOBase(const XAPO_REGISTRATION_PROPERTIES* pRegistrationProperties) {
	CreateFAPOBase(&fapo_base.base , pRegistrationProperties);
}

CXAPOBase::~CXAPOBase() {
}

HRESULT CXAPOBase::QueryInterface(REFIID riid, void** ppInterface) {
	if (riid == IID_IXAPO) {
		*ppInterface = static_cast<IXAPO *>(this);
	} else if (riid == IID_IUnknown) {
		*ppInterface = static_cast<IUnknown *>(this);
	} else {
		*ppInterface = NULL;
		return E_NOINTERFACE;
	}

	reinterpret_cast<IUnknown *>(*ppInterface)->AddRef();

	return S_OK;
}

ULONG CXAPOBase::AddRef() {
	return FAPOBase_AddRef(&fapo_base.base);
}

ULONG CXAPOBase::Release() {
	ULONG refcount = FAPOBase_Release(&fapo_base.base);
	if (refcount == 0) {
		delete this;
	}

	return refcount;
}

HRESULT CXAPOBase::GetRegistrationProperties(XAPO_REGISTRATION_PROPERTIES** ppRegistrationProperties) {
	return FAPOBase_GetRegistrationProperties(&fapo_base.base, ppRegistrationProperties);
}

HRESULT CXAPOBase::IsInputFormatSupported(
	const WAVEFORMATEX* pOutputFormat,
	const WAVEFORMATEX* pRequestedInputFormat,
	WAVEFORMATEX** ppSupportedInputFormat) {
	return FAPOBase_IsInputFormatSupported(&fapo_base.base, pOutputFormat, pRequestedInputFormat, ppSupportedInputFormat);
}

HRESULT CXAPOBase::IsOutputFormatSupported(
	const WAVEFORMATEX* pInputFormat,
	const WAVEFORMATEX* pRequestedOutputFormat,
	WAVEFORMATEX** ppSupportedOutputFormat) {
	return FAPOBase_IsOutputFormatSupported(&fapo_base.base, pInputFormat, pRequestedOutputFormat, ppSupportedOutputFormat);
}

HRESULT CXAPOBase::Initialize(const void*pData, UINT32 DataByteSize) {
	return FAPOBase_Initialize(&fapo_base.base, pData, DataByteSize);
}

void CXAPOBase::Reset() {
	FAPOBase_Reset(&fapo_base.base);
}

HRESULT CXAPOBase::LockForProcess(
	UINT32 InputLockedParameterCount,
	const XAPO_LOCKFORPROCESS_BUFFER_PARAMETERS* pInputLockedParameters,
	UINT32 OutputLockedParameterCount,
	const XAPO_LOCKFORPROCESS_BUFFER_PARAMETERS* pOutputLockedParameters) {
	return FAPOBase_LockForProcess(&fapo_base.base, InputLockedParameterCount, pInputLockedParameters, OutputLockedParameterCount, pOutputLockedParameters);
}

void CXAPOBase::UnlockForProcess() {
	FAPOBase_UnlockForProcess(&fapo_base.base);
}

UINT32 CXAPOBase::CalcInputFrames(UINT32 OutputFrameCount) {
	return FAPOBase_CalcInputFrames(&fapo_base.base, OutputFrameCount);
}

UINT32 CXAPOBase::CalcOutputFrames(UINT32 InputFrameCount) {
	return FAPOBase_CalcOutputFrames(&fapo_base.base, InputFrameCount);
}

// protected functions
HRESULT CXAPOBase::ValidateFormatDefault(WAVEFORMATEX* pFormat, BOOL fOverwrite) {
	return FAPOBase_ValidateFormatDefault(&fapo_base.base, pFormat, fOverwrite);
}

HRESULT CXAPOBase::ValidateFormatPair(
	const WAVEFORMATEX* pSupportedFormat,
	WAVEFORMATEX* pRequestedFormat,
	BOOL fOverwrite) {
	return FAPOBase_ValidateFormatPair(&fapo_base.base, pSupportedFormat, pRequestedFormat, fOverwrite);
}

void CXAPOBase::ProcessThru(
	void* pInputBuffer,
	FLOAT32* pOutputBuffer,
	UINT32 FrameCount,
	WORD InputChannelCount,
	WORD OutputChannelCount,
	BOOL MixWithOutput) {
	FAPOBase_ProcessThru(&fapo_base.base, pInputBuffer, pOutputBuffer, FrameCount, InputChannelCount, OutputChannelCount, MixWithOutput);
}

BOOL CXAPOBase::IsLocked() {
	return fapo_base.base.m_fIsLocked;
}

///////////////////////////////////////////////////////////////////////////////
//
// CXAPOParametersBase
//

CXAPOParametersBase::CXAPOParametersBase(
	const XAPO_REGISTRATION_PROPERTIES* pRegistrationProperties,
	BYTE* pParameterBlocks,
	UINT32 uParameterBlockByteSize,
	BOOL fProducer) {
	CreateFAPOParametersBase(&fapo_base, pRegistrationProperties, pParameterBlocks, uParameterBlockByteSize, fProducer);
}

CXAPOParametersBase::~CXAPOParametersBase() {
}

HRESULT CXAPOParametersBase::QueryInterface(REFIID riid, void** ppInterface) {
	if (riid == IID_IXAPOParameters) {
		*ppInterface = static_cast<IXAPOParameters *>(this);
		CXAPOBase::AddRef();
		return S_OK;
	} else {
		return CXAPOBase::QueryInterface(riid, ppInterface);
	}
}

ULONG CXAPOParametersBase::AddRef() {
	return CXAPOBase::AddRef();
}

ULONG CXAPOParametersBase::Release() { 
	return CXAPOBase::Release();
}

void CXAPOParametersBase::SetParameters(const void* pParameters, UINT32 ParameterByteSize) {
	FAPOParametersBase_SetParameters(&fapo_base, pParameters, ParameterByteSize);
}

void CXAPOParametersBase::GetParameters(void* pParameters, UINT32 ParameterByteSize) {
	FAPOParametersBase_GetParameters(&fapo_base, pParameters, ParameterByteSize);
}

void CXAPOParametersBase::OnSetParameters(const void* pParameters, UINT32 ParameterByteSize) {
	FAPOParametersBase_OnSetParameters(&fapo_base, pParameters, ParameterByteSize);
}

BOOL CXAPOParametersBase::ParametersChanged() {
	return FAPOParametersBase_ParametersChanged(&fapo_base);
}

BYTE* CXAPOParametersBase::BeginProcess() {
	return FAPOParametersBase_BeginProcess(&fapo_base);
}

void CXAPOParametersBase::EndProcess() {
	FAPOParametersBase_EndProcess(&fapo_base);
}
