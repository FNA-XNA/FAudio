cmake_minimum_required(VERSION 2.6)
project(FAudio)

# Options
option(FORCE32 "Force 32-bit on 64-bit (ignored on macOS)" OFF)
option(FFMPEG "Enable FFmpeg support (WMA, XMA)" OFF)
option(BUILD_UTILS "Build utils/ folder" OFF)
option(BUILD_CPP "Build cpp/ folder (COM for Win32, Wine for Unix)" OFF)
option(XNASONG "Build with XNA_Song.c" ON)

# Platform Flags
if(APPLE)
	set(CMAKE_OSX_DEPLOYMENT_TARGET 10.7)
elseif(FORCE32)
	set(CMAKE_C_FLAGS "-m32")
	set(CMAKE_CXX_FLAGS "-m32")
endif()

# SDL2 Dependency
find_package(SDL2)

# Base FAudio Sources
set(FAUDIO_SRC
	src/F3DAudio.c
	src/FAudio.c
	src/FAudio_internal.c
	src/FAudio_internal_simd.c
	src/FAudioFX_reverb.c
	src/FAudioFX_volumemeter.c
	src/FACT.c
	src/FACT3D.c
	src/FACT_internal.c
	src/FAPOBase.c
	src/FAPOFX.c
	src/FAPOFX_eq.c
	src/FAPOFX_masteringlimiter.c
	src/FAPOFX_reverb.c
	src/FAPOFX_echo.c
	src/FAudio_platform_sdl2.c
)

# FFmpeg Support
if(FFMPEG)
	# TODO: Add libavcodec/libavutil as dependencies
	add_definitions(-DHAVE_FFMPEG=1)
	list(APPEND FAUDIO_SRC src/FAudio_ffmpeg.c)
endif(FFMPEG)

# XNA_Song Support
if(XNASONG)
	list(APPEND FAUDIO_SRC src/XNA_Song.c)
endif(XNASONG)

# FAudio Shared Library
add_library(FAudio SHARED ${FAUDIO_SRC})
include_directories(FAudio ${SDL2_INCLUDE_DIRS})
target_link_libraries(FAudio ${SDL2_LIBRARY})

# TODO: utils/

# TODO: cpp/ COM Wrapper if MinGW

# TODO: cpp/ Wine DLL if Linux
