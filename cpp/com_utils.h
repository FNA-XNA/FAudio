#ifndef FACT_CPP_FAUDIO_COM_H
#define FACT_CPP_FAUDIO_COM_H

#include <FAudio.h>

// common windows types
#ifndef FAUDIO_USE_STD_TYPES

typedef uint8_t BYTE;
typedef uint16_t WORD;
typedef uint32_t HRESULT;
typedef uint32_t UINT32;
typedef long LONG;
typedef unsigned long ULONG;
typedef wchar_t WHCAR;
typedef const WHCAR *LPCWSTR;
typedef int BOOL;
typedef unsigned long DWORD;
typedef void *LPVOID;
typedef float FLOAT32;

#endif

// HRESULT stuff
#define S_OK 0
#define S_FALSE 1
#define E_NOTIMPL 80004001
#define E_NOINTERFACE 80004002
#define E_OUTOFMEMORY 0x8007000E
#define CLASS_E_NOAGGREGATION 0x80040110
#define CLASS_E_CLASSNOTAVAILABLE 0x80040111

// GUID, IID stuff
typedef FAudioGUID GUID;
typedef GUID IID;
#define REFIID const IID &
#define REFCLSID const IID &

inline bool operator==(const IID &a, const IID &b) {
	return a.Data1 == b.Data1 &&
		a.Data2 == b.Data2 &&
		a.Data3 == b.Data3 &&
		a.Data4[0] == b.Data4[0] &&
		a.Data4[1] == b.Data4[1] &&
		a.Data4[2] == b.Data4[2] &&
		a.Data4[3] == b.Data4[3] &&
		a.Data4[4] == b.Data4[4] &&
		a.Data4[5] == b.Data4[5] &&
		a.Data4[6] == b.Data4[6] &&
		a.Data4[7] == b.Data4[7];
}

static const IID IID_IUnknown = { 0x00000000, 0x0000, 0x0000, {0xC0, 00, 00, 00, 00, 00, 00, 0x46}};
static const IID IID_IClassFactory = { 0x00000001, 0x0000, 0x0000, {0xC0, 00, 00, 00, 00, 00, 00, 0x46}};
static const IID IID_IXAudio2 = { 0x8bcf1f58, 0x9fe7, 0x4583, {0x8a, 0xc6, 0xe2, 0xad, 0xc4, 0x65, 0xc8, 0xbb }};

static const IID CLSID_XAudio2_6 = { 0x3eda9b49, 0x2085, 0x498b, {0x9b, 0xb2, 0x39, 0xa6, 0x77, 0x84, 0x93, 0xde }};
static const IID CLSID_XAudio2_7 = { 0x5a508685, 0xa254, 0x4fba, {0x9b, 0x82, 0x9a, 0x24, 0xb0, 0x03, 0x06, 0xaf }};

// quality of life macro's
#define FACOM_METHOD(rtype)		virtual rtype __stdcall 

// common interfaces
class IUnknown {
public:
	FACOM_METHOD(HRESULT) QueryInterface(REFIID riid, void** ppvInterface) = 0;
	FACOM_METHOD(ULONG) AddRef() = 0;
	FACOM_METHOD(ULONG) Release() = 0;
};

class IClassFactory : public IUnknown {
public:
	FACOM_METHOD(HRESULT) CreateInstance(
		IUnknown *pUnkOuter,
		REFIID riid,
		void **ppvObject) = 0;

	FACOM_METHOD(HRESULT) LockServer(BOOL fLock) = 0;
};



#endif // FACT_CPP_FAUDIO_COM_H
