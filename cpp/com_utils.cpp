#include "com_utils.h"

const IID IID_IUnknown = { 0x00000000, 0x0000, 0x0000, {0xC0, 00, 00, 00, 00, 00, 00, 0x46}};
const IID IID_IClassFactory = { 0x00000001, 0x0000, 0x0000, {0xC0, 00, 00, 00, 00, 00, 00, 0x46}};
const IID IID_IXAudio2 = { 0x8bcf1f58, 0x9fe7, 0x4583, {0x8a, 0xc6, 0xe2, 0xad, 0xc4, 0x65, 0xc8, 0xbb }};

///////////////////////////////////////////////////////////////////////////////
//
// Class Factory: fun COM stuff
//

typedef void *(*FACTORY_FUNC)(void);
void *CreateXAudio2Internal(void);
void *CreateAudioVolumeMeterInternal(void);
void *CreateAudioReverbInternal(void);

template <REFIID fact_iid, FACTORY_FUNC fact_creator>
class ClassFactory : public IClassFactory {
public:
	FACOM_METHOD(HRESULT) QueryInterface(REFIID riid, void** ppvInterface) {
		if ((riid == IID_IUnknown) || (riid == IID_IClassFactory)) {
			*ppvInterface = static_cast<IClassFactory *>(this);
		} else {
			*ppvInterface = NULL;
			return E_NOINTERFACE;
		}

		reinterpret_cast<IUnknown *>(*ppvInterface)->AddRef();

		return S_OK;
	}

	FACOM_METHOD(ULONG) AddRef() {
		// FIXME: locking
		return ++refcount;
	}

	FACOM_METHOD(ULONG) Release() {
		// FIXME: locking
		long rc = --refcount;

		if (rc == 0) {
			delete this;
		}

		return rc;
	}

	FACOM_METHOD(HRESULT) CreateInstance(IUnknown *pUnkOuter, REFIID riid, void **ppvObject) {
		if (pUnkOuter != NULL) {
			return CLASS_E_NOAGGREGATION;
		}

		void *obj = NULL;

		if (riid == fact_iid) {
			obj = fact_creator();
		} else {
			*ppvObject = NULL;
			return E_NOINTERFACE;
		}

		if (obj == NULL) {
			return E_OUTOFMEMORY;
		}

		return reinterpret_cast<IUnknown *>(obj)->QueryInterface(riid, ppvObject);
	}

	FACOM_METHOD(HRESULT) LockServer(BOOL fLock) {
		return E_NOTIMPL;
	}

private:
	long refcount;
};

///////////////////////////////////////////////////////////////////////////////
//
// COM DLL interface functions
//

extern "C" HRESULT __stdcall DllCanUnloadNow() {
	return S_FALSE;
}

extern "C" HRESULT __stdcall DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID *ppv) {
	IClassFactory *factory = NULL;

	if (rclsid == CLSID_XAudio2_6 || rclsid == CLSID_XAudio2_7) {
		factory = new ClassFactory<IID_IXAudio2, CreateXAudio2Internal>();
	} else if (rclsid == CLSID_AudioVolumeMeter) {
		factory = new ClassFactory<IID_IUnknown, CreateAudioVolumeMeterInternal>();
	} else if (rclsid == CLSID_AudioReverb) {
		factory = new ClassFactory<IID_IUnknown, CreateAudioReverbInternal>();
	} else {
		return CLASS_E_CLASSNOTAVAILABLE;
	}

	if (!factory) {
		return E_OUTOFMEMORY;
	}

	return factory->QueryInterface(riid, ppv);

}

extern "C" HRESULT __stdcall DllRegisterServer() {
	return S_OK;
}

extern "C" HRESULT __stdcall DllUnregisterServer() {
	return S_OK;
}
