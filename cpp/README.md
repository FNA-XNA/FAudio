# C++ / COM Interface for FAudio

Use FAudio in place of XAudio2 without changing / recompiling the host application.
Currently works both on Windows and on Linux with Wine (either native and builtin shared libraries).

## Building

### Building Windows DLLs with Visual Studio
A Visual Studio 2010 solution is present in the visualc directory. 
To compile the 64-bit libraries with VS2010 the Windows 7.1 SDK has to be installed.

Newer Visual Studio versions should be able to upgrade the 2010 solution. If there are any issues check if the Platform Toolset is set to something reasonable.

The resulting DLLs can also be used as native DLLs with Wine.

### Building native Wine DLLs with mingw-w64
Because cross-compiling is fun.

#### Prerequisites
- mingw-w64 has to be present on your system
- install the SDL2 development libraries for mingw

#### Building
The project includes two shell scripts to setup your environment for cross-compilation: one for 32-bit and another for 64-bit Windows DLLs.

- source either scrips/cross_compile_32 or scripts/cross_compile_64
- cross-compile FACT: make clean all in the root directory of FACT
- cross-compile the C++/COM wrapper: change to the cpp subdirectory and run make

The results are stored in either the build_win32 or build_win64 subdirectory.

### Building builtin Wine DLLs 
The C++/COM wrapper has a separate Makefile for building Wine linux shared libraries. It's assumed that your running a 64-bit Linux system.

#### Building 64-bit shared libraries
- will be used by 64-bit Windows applications
- check if the WINEDIR variable in Makefile.wine points to the Wine directory on your system.
- build FACT as usual
- build the C++/COM wrapper: make -f Makefile.wine

#### Building 32-bit shared libraries
- will be used by 32-bit Windows applications
- check if the WINEDIR variable in Makefile.wine points to the Wine directory on your system.
- Note: your sytem has to be setup to build 32-bit binaries (e.g. multilib). You'll also need a 32-bit SDL2 with working sound drivers.
- make sure the 32-bit sdl-config is first on your PATH
- build 32-bit FACT: CFLAGS=-m32 make clean all
- build 32-bit C++/COM wrapper: ARCH=32 make -f Makefile.wine clean all

## Using the wrapper

### Windows - XAudio version 2.8+
The COM registration was dropped in XAudio 2.8. To override the system XAudio2 DLLs place the FAudio wrapper DLLs in the same directory as the executable of the application you want to test. Make sure that FAudio.dll and SDL2.dll are also on the DLL search path.

### Windows - XAudio version 2.7 or earlier
To use XAudio 2.7 or earlier we'll have to hijack to COM registration in the registry. COM information is stored under HKEY_CLASSES_ROOT but in modern (Vista+?) Windows version this is actually a merged view of HKEY_LOCAL_MACHINE/Software/Classes (for system defaults) and HKEY_CURRENT_USER/Software/Classes for user specific configuration. We use this to our advantage to register the FAudio wrapper DLLs for the current user. The standard Windows tool regsvr32 can be used to register/unregister the DLLs. Just make sure to specify an absolute path to the wrapper DLLs to prevent regsvr32 from using the standard Windows versions. 

The scripts subdirectory contains Windows cmd-scripts to (un)register all the DLLs in one step. Run the script from the directory containing the wrapper DLLs.

### Wine - native DLLs
A Wine prefix is a directory that contains a Wine configuration, registry, and native Windows DLLs for compatibility. It's trivial to replace the XAudio2 DLLs in a prefix with the FAudio wrapper DLLs and still use another prefix with the original DLLs.

The wine_setup_native script (in the scripts subdirectory) does just this. Run it from a directory containing the wrapper DLLs (32 or 64 bit) and it will create symbolic links in the Wine prefix and modify the Wine registry to make sure Wine only uses the native DLL's.

### Wine - builtin DLLs
The native DLLs use Wine's sound layers, replacing the builtin DLLs lets you bypass these. The WINEDLLPATH environment variable can be used to add dll search directories to Wine but unfortunately these do not override the standard directories. In order to use the wrapper DLLs the standard Wine shared libraries (e.g in /opt/wine-staging/lib(64)/wine) have to be removed / renamed first. Also Wine has to be configured to use the builtin DLLs before the native DLLs for xaudio (see winecfg).

Please note: the version 2.8+ wrapper builtin DLLs do not implement device enumeration. The Windows device id's are pretty meaningless when pass to a linux SDL implementation.
