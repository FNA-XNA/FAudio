#include "com_utils.h"

///////////////////////////////////////////////////////////////////////////////
//
// Class Factory: fun COM stuff
//

IUnknown *CreateXAudio2Internal(void);

class XAudio2Factory : public IClassFactory {
public:
	FACOM_METHOD(HRESULT) QueryInterface(REFIID riid, void** ppvInterface) {
		if ((riid == IID_IUnknown) || (riid == IID_IClassFactory)) {
			*ppvInterface = static_cast<IClassFactory *>(this);
		}
		else {
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

		IUnknown *obj = NULL;

		if (riid == IID_IXAudio2) {
			obj = CreateXAudio2Internal();
		} else {
			*ppvObject = NULL;
			return E_NOINTERFACE;
		}

		if (obj == NULL) {
			return E_OUTOFMEMORY;
		}

		return obj->QueryInterface(riid, ppvObject);
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

HRESULT __stdcall DllCanUnloadNow() {
	return S_FALSE;
}

HRESULT __stdcall DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID *ppv) {
	if (rclsid == CLSID_XAudio2_6 || rclsid == CLSID_XAudio2_7) {
		XAudio2Factory *factory = new XAudio2Factory();
		if (!factory) {
			return E_OUTOFMEMORY;
		}
		return factory->QueryInterface(riid, ppv);
	}

	return CLASS_E_CLASSNOTAVAILABLE;
}

HRESULT __stdcall DllRegisterServer() {
	return S_OK;
}

HRESULT __stdcall DllUnregisterServer() {
	return S_OK;
}
