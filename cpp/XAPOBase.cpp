#include "XAPOBase.h"

///////////////////////////////////////////////////////////////////////////////
//
// CXAPOBase
//

CXAPOBase::CXAPOBase(FAPOBase *base) 
	: fapo_base(base), 
	  own_fapo_base(false) 
{
}

CXAPOBase::CXAPOBase(const XAPO_REGISTRATION_PROPERTIES* pRegistrationProperties) 
	: fapo_base(new FAPOBase()),
	own_fapo_base(true) 
{
	CreateFAPOBase(fapo_base, pRegistrationProperties);
}

CXAPOBase::~CXAPOBase() 
{
	if (own_fapo_base) 
	{
		delete fapo_base;
	} 
	else if (fapo_base->Destructor) 
	{
		fapo_base->Destructor(fapo_base);
	}
}

HRESULT CXAPOBase::QueryInterface(REFIID riid, void** ppInterface) 
{
	if (guid_equals(riid, IID_IXAPO))
	{
		*ppInterface = static_cast<IXAPO *>(this);
	} 
	else if (guid_equals(riid, IID_IUnknown))
	{
		*ppInterface = static_cast<IUnknown *>(this);
	} 
	else 
	{
		*ppInterface = NULL;
		return E_NOINTERFACE;
	}

	reinterpret_cast<IUnknown *>(*ppInterface)->AddRef();

	return S_OK;
}

ULONG CXAPOBase::AddRef() 
{
	return FAPOBase_AddRef(fapo_base);
}

ULONG CXAPOBase::Release() 
{
	ULONG refcount = FAPOBase_Release(fapo_base);
	if (refcount == 0) 
	{
		delete this;
	}

	return refcount;
}

HRESULT CXAPOBase::GetRegistrationProperties(XAPO_REGISTRATION_PROPERTIES** ppRegistrationProperties) 
{
	return FAPOBase_GetRegistrationProperties(fapo_base, ppRegistrationProperties);
}

HRESULT CXAPOBase::IsInputFormatSupported(
	const WAVEFORMATEX* pOutputFormat,
	const WAVEFORMATEX* pRequestedInputFormat,
	WAVEFORMATEX** ppSupportedInputFormat
) {
	return FAPOBase_IsInputFormatSupported(
		fapo_base, 
		pOutputFormat, 
		pRequestedInputFormat, 
		ppSupportedInputFormat);
}

HRESULT CXAPOBase::IsOutputFormatSupported(
	const WAVEFORMATEX* pInputFormat,
	const WAVEFORMATEX* pRequestedOutputFormat,
	WAVEFORMATEX** ppSupportedOutputFormat
) {
	return FAPOBase_IsOutputFormatSupported(
		fapo_base, 
		pInputFormat, 
		pRequestedOutputFormat, 
		ppSupportedOutputFormat);
}

HRESULT CXAPOBase::Initialize(const void*pData, UINT32 DataByteSize) 
{
	return FAPOBase_Initialize(fapo_base, pData, DataByteSize);
}

void CXAPOBase::Reset() 
{
	FAPOBase_Reset(fapo_base);
}

HRESULT CXAPOBase::LockForProcess(
	UINT32 InputLockedParameterCount,
	const XAPO_LOCKFORPROCESS_BUFFER_PARAMETERS* pInputLockedParameters,
	UINT32 OutputLockedParameterCount,
	const XAPO_LOCKFORPROCESS_BUFFER_PARAMETERS* pOutputLockedParameters
) {
	return FAPOBase_LockForProcess(
		fapo_base, 
		InputLockedParameterCount, 
		pInputLockedParameters, 
		OutputLockedParameterCount, 
		pOutputLockedParameters);
}

void CXAPOBase::UnlockForProcess() 
{
	FAPOBase_UnlockForProcess(fapo_base);
}

UINT32 CXAPOBase::CalcInputFrames(UINT32 OutputFrameCount) 
{
	return FAPOBase_CalcInputFrames(fapo_base, OutputFrameCount);
}

UINT32 CXAPOBase::CalcOutputFrames(UINT32 InputFrameCount) 
{
	return FAPOBase_CalcOutputFrames(fapo_base, InputFrameCount);
}

// protected functions
HRESULT CXAPOBase::ValidateFormatDefault(WAVEFORMATEX* pFormat, BOOL fOverwrite) 
{
	return FAPOBase_ValidateFormatDefault(fapo_base, pFormat, fOverwrite);
}

HRESULT CXAPOBase::ValidateFormatPair(
	const WAVEFORMATEX* pSupportedFormat,
	WAVEFORMATEX* pRequestedFormat,
	BOOL fOverwrite
) {
	return FAPOBase_ValidateFormatPair(
		fapo_base, 
		pSupportedFormat, 
		pRequestedFormat, 
		fOverwrite);
}

void CXAPOBase::ProcessThru(
	void* pInputBuffer,
	FLOAT32* pOutputBuffer,
	UINT32 FrameCount,
	WORD InputChannelCount,
	WORD OutputChannelCount,
	BOOL MixWithOutput) 
{
	FAPOBase_ProcessThru(
		fapo_base, 
		pInputBuffer, 
		pOutputBuffer, 
		FrameCount, 
		InputChannelCount, 
		OutputChannelCount, 
		MixWithOutput);
}

BOOL CXAPOBase::IsLocked() 
{
	return fapo_base->m_fIsLocked;
}

///////////////////////////////////////////////////////////////////////////////
//
// CXAPOParametersBase
//

CXAPOParametersBase::CXAPOParametersBase(FAPOParametersBase *param_base)
	: fapo_param_base(param_base),
	  own_fapo_param_base(false),
	  CXAPOBase(&param_base->base) 
{
}

CXAPOParametersBase::CXAPOParametersBase(
	const XAPO_REGISTRATION_PROPERTIES* pRegistrationProperties,
	BYTE* pParameterBlocks,
	UINT32 uParameterBlockByteSize,
	BOOL fProducer) 
	: fapo_param_base(new FAPOParametersBase()),
	  own_fapo_param_base(true),
	  CXAPOBase(&fapo_param_base->base) 
{
	CreateFAPOParametersBase(
		fapo_param_base, 
		pRegistrationProperties, 
		pParameterBlocks, 
		uParameterBlockByteSize, 
		fProducer);
}

CXAPOParametersBase::~CXAPOParametersBase() 
{
	if (own_fapo_param_base) 
	{
		delete fapo_param_base;
	}
}

HRESULT CXAPOParametersBase::QueryInterface(REFIID riid, void** ppInterface) 
{
	if (guid_equals(riid, IID_IXAPOParameters))
	{
		*ppInterface = static_cast<IXAPOParameters *>(this);
		CXAPOBase::AddRef();
		return S_OK;
	} 
	else 
	{
		return CXAPOBase::QueryInterface(riid, ppInterface);
	}
}

ULONG CXAPOParametersBase::AddRef() 
{
	return CXAPOBase::AddRef();
}

ULONG CXAPOParametersBase::Release() 
{
	return CXAPOBase::Release();
}

void CXAPOParametersBase::SetParameters(const void* pParameters, UINT32 ParameterByteSize) 
{
	FAPOParametersBase_SetParameters(fapo_param_base, pParameters, ParameterByteSize);
}

void CXAPOParametersBase::GetParameters(void* pParameters, UINT32 ParameterByteSize) {
	FAPOParametersBase_GetParameters(fapo_param_base, pParameters, ParameterByteSize);
}

void CXAPOParametersBase::OnSetParameters(const void* pParameters, UINT32 ParameterByteSize) 
{
	FAPOParametersBase_OnSetParameters(fapo_param_base, pParameters, ParameterByteSize);
}

BOOL CXAPOParametersBase::ParametersChanged() 
{
	return FAPOParametersBase_ParametersChanged(fapo_param_base);
}

BYTE* CXAPOParametersBase::BeginProcess() 
{
	return FAPOParametersBase_BeginProcess(fapo_param_base);
}

void CXAPOParametersBase::EndProcess() 
{
	FAPOParametersBase_EndProcess(fapo_param_base);
}
