#include "com_utils.h"
#include <SDL.h>

/* GUIDs */
const IID IID_IUnknown = {0x00000000, 0x0000, 0x0000, {0xC0, 00, 00, 00, 00, 00, 00, 0x46}};
const IID IID_IClassFactory = {0x00000001, 0x0000, 0x0000, {0xC0, 00, 00, 00, 00, 00, 00, 0x46}};

const IID IID_IXACT3Engine = {0xb1ee676a, 0xd9cd, 0x4d2a, {0x89, 0xa8, 0xfa, 0x53, 0xeb, 0x9e, 0x48, 0x0b}};
const IID CLSID_XACTEngine = {0xbcc782bc, 0x6492, 0x4c22, {0x8c, 0x35, 0xf5, 0xd7, 0x2f, 0xe7, 0x3c, 0x6e}};

bool guid_equals(REFIID a, REFIID b)
{
	return SDL_memcmp(&a, &b, sizeof(IID)) == 0;
}

///////////////////////////////////////////////////////////////////////////////
//
// Class Factory: fun COM stuff
//

typedef void *(*FACTORY_FUNC)(void);
void *CreateXACT3EngineInternal(void);

template <REFIID fact_iid, FACTORY_FUNC fact_creator> class ClassFactory : public IClassFactory
{
public:
	COM_METHOD(HRESULT) QueryInterface(REFIID riid, void **ppvInterface)
	{
		if (guid_equals(riid, IID_IUnknown) || guid_equals(riid, IID_IClassFactory))
		{
			*ppvInterface = static_cast<IClassFactory *>(this);
		}
		else
		{
			*ppvInterface = NULL;
			return E_NOINTERFACE;
		}

		reinterpret_cast<IUnknown *>(*ppvInterface)->AddRef();

		return S_OK;
	}

	COM_METHOD(ULONG) AddRef()
	{
		// FIXME: atomics
		return ++refcount;
	}

	COM_METHOD(ULONG) Release()
	{
		// FIXME: atomics
		long rc = --refcount;

		if (rc == 0)
		{
			delete this;
		}

		return rc;
	}

	COM_METHOD(HRESULT) CreateInstance(IUnknown *pUnkOuter, REFIID riid, void **ppvObject)
	{
		if (pUnkOuter != NULL)
		{
			return CLASS_E_NOAGGREGATION;
		}

		void *obj = NULL;

		if (guid_equals(riid, fact_iid))
		{
			obj = fact_creator();
		}
		else
		{
			*ppvObject = NULL;
			return E_NOINTERFACE;
		}

		if (obj == NULL)
		{
			return E_OUTOFMEMORY;
		}

		return reinterpret_cast<IUnknown *>(obj)->QueryInterface(riid, ppvObject);
	}

	COM_METHOD(HRESULT) LockServer(BOOL fLock) { return E_NOTIMPL; }

private:
	long refcount;
};

///////////////////////////////////////////////////////////////////////////////
//
// COM DLL interface functions
//

static void *DllHandle = NULL;

BOOL __stdcall DllMain(void *hinstDLL, DWORD dwReason, LPVOID lpvReserved)
{
	if (dwReason == 1 /*DLL_PROCESS_ATTACH*/)
	{
		DllHandle = hinstDLL;
	}
	return 1;
}

extern "C" HRESULT __stdcall DllCanUnloadNow() 
{ 
	return S_FALSE; 
}

extern "C" HRESULT __stdcall DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID *ppv)
{
	IClassFactory *factory = NULL;

	if (guid_equals(rclsid, CLSID_XACTEngine))
	{
		factory = new ClassFactory<IID_IXACT3Engine, CreateXACT3EngineInternal>();
	}
	else
	{
		return CLASS_E_CLASSNOTAVAILABLE;
	}

	if (!factory)
	{
		return E_OUTOFMEMORY;
	}

	return factory->QueryInterface(riid, ppv);
}

extern "C" HRESULT register_faudio_dll(void *, REFIID);
extern "C" HRESULT unregister_faudio_dll(void *, REFIID);

extern "C" HRESULT __stdcall DllRegisterServer(void)
{
#ifndef __WINE__
	register_faudio_dll(DllHandle, CLSID_XACTEngine);
#endif
	return S_OK;
}

extern "C" HRESULT __stdcall DllUnregisterServer(void)
{
#ifndef __WINE__
	unregister_faudio_dll(DllHandle, CLSID_XACTEngine);
#endif
	return S_OK;
}
