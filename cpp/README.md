## C++ / COM Interface voor FAudio

Use FAudio in place of XAudio2 without changing / recompiling the host application.
Currently only working on Windows with API version 2.9 (no COM) and version 2.7 (COM).
Linux/wine support coming soon.

## Using APÏ version 2.9
Microsoft stopped using COM in XAudio2 version 2.8. Just put our XAudio2_9.dll (and FAudio.dll and SDL2.dll) in the same directory as the executable.
This path has priority when Windows searches for DLLs and it will override the system DLL.

## Using API version 2.7 with 32-bit applications
You will need to override the COM registration of the DLL. For now this means manually changing the registry. Later support for regsvr32 will probably be added.
Open the registry key HKEY_CLASSES_ROOT\WOW6432Node\CLSID\{5a508685-a254-4fba-9b82-9a24b00306af}\InProcServer32 and change the (Default) string to point to our DLL.
Make sure to remember the original value so you can restore it later.

## Using API version 2.6 with 32-bit applications
Our 2.7 DLL should also work with applications using XAudio2.6 (e.g. Super Meat Boy). Same procedure as version 2.7 but change the key at HKEY_CLASSES_ROOT\WOW6432Node\CLSID\{3eda9b49-2085-498b-9bb2-39a6778493de}\InProcServer32.

## Todo
- Make projects for VC2010 and test compilation (currently using with VS2015).
- XAudio2: EngineCallbacks aren't implemented
- XAudio2: reference counting and freeing memory
- Linux / Wine integration
- X3DAudio wrapper
- XAPO wrapper
- XAudio2FX wrapper
- Code cleanup
