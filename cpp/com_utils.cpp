#include "com_utils.h"

const IID IID_IUnknown = { 0x00000000, 0x0000, 0x0000,{ 0xC0, 00, 00, 00, 00, 00, 00, 0x46 } };
const IID IID_IClassFactory = { 0x00000001, 0x0000, 0x0000,{ 0xC0, 00, 00, 00, 00, 00, 00, 0x46 } };

const IID IID_IXAudio2 = { 0x8bcf1f58, 0x9fe7, 0x4583,{ 0x8a, 0xc6, 0xe2, 0xad, 0xc4, 0x65, 0xc8, 0xbb } };

const IID IID_IXAPO = { 0xA90BC001, 0xE897, 0xE897,{ 0x55, 0xE4, 0x9E, 0x47, 0x00, 0x00, 0x00, 0x00 } };
const IID IID_IXAPOParameters = { 0xA90BC001, 0xE897, 0xE897,{ 0x55, 0xE4, 0x9E, 0x47, 0x00, 0x00, 0x00, 0x01 } };

const IID CLSID_XAudio2_0 = { 0xfac23f48, 0x31f5, 0x45a8,{ 0xb4, 0x9b, 0x52, 0x25, 0xd6, 0x14, 0x01, 0xaa } };
const IID CLSID_XAudio2_1 = { 0xe21a7345, 0xeb21, 0x468e,{ 0xbe, 0x50, 0x80, 0x4d, 0xb9, 0x7c, 0xf7, 0x08 } };
const IID CLSID_XAudio2_2 = { 0xb802058a, 0x464a, 0x42db,{ 0xbc, 0x10, 0xb6, 0x50, 0xd6, 0xf2, 0x58, 0x6a } };
const IID CLSID_XAudio2_3 = { 0x4c5e637a, 0x16c7, 0x4de3,{ 0x9c, 0x46, 0x5e, 0xd2, 0x21, 0x81, 0x96, 0x2d } };
const IID CLSID_XAudio2_4 = { 0x03219e78, 0x5bc3, 0x44d1,{ 0xb9, 0x2e, 0xf6, 0x3d, 0x89, 0xcc, 0x65, 0x26 } };
const IID CLSID_XAudio2_5 = { 0x4c9b6dde, 0x6809, 0x46e6,{ 0xa2, 0x78, 0x9b, 0x6a, 0x97, 0x58, 0x86, 0x70 } };
const IID CLSID_XAudio2_6 = { 0x3eda9b49, 0x2085, 0x498b,{ 0x9b, 0xb2, 0x39, 0xa6, 0x77, 0x84, 0x93, 0xde } };
const IID CLSID_XAudio2_7 = { 0x5a508685, 0xa254, 0x4fba,{ 0x9b, 0x82, 0x9a, 0x24, 0xb0, 0x03, 0x06, 0xaf } };

const IID CLSID_AudioVolumeMeter = { 0xcac1105f, 0x619b, 0x4d04,{ 0x83, 0x1a, 0x44, 0xe1, 0xcb, 0xf1, 0x2d, 0x57 } };
const IID CLSID_AudioReverb = { 0x6a93130e, 0x1d53, 0x41d1,{ 0xa9, 0xcf, 0xe7, 0x58, 0x80, 0x0b, 0xb1, 0x79 } };

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

#if XAUDIO2_VERSION <= 7

extern "C" HRESULT __stdcall DllCanUnloadNow() {
	return S_FALSE;
}

extern "C" HRESULT __stdcall DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID *ppv) {
	IClassFactory *factory = NULL;

	if (rclsid == CLSID_XAudio2_0 || rclsid == CLSID_XAudio2_1 || rclsid == CLSID_XAudio2_2 ||
	    rclsid == CLSID_XAudio2_3 || rclsid == CLSID_XAudio2_4 || rclsid == CLSID_XAudio2_5 ||
	    rclsid == CLSID_XAudio2_6 || rclsid == CLSID_XAudio2_7) {
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

#endif // XAUDIO2_VERSION <= 7
