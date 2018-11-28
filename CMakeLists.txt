cmake_minimum_required(VERSION 3.7)
project(FAudio)

# Options
option(FORCE32 "Force 32-bit on 64-bit (ignored on macOS)" OFF)
option(FFMPEG "Enable FFmpeg support (WMA, XMA)" OFF)
option(BUILD_UTILS "Build utils/ folder" OFF)
option(BUILD_CPP "Build cpp/ folder (COM for Win32, Wine for Unix)" OFF)
option(BUILD_TESTS "Build tests/ folder for unit tests to be executed on the host against FAudio" ON)
option(XNASONG "Build with XNA_Song.c" ON)

# set build type to Release if none is specified
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	message(STATUS "Setting build type to 'Release' as none was specified.")
	set(CMAKE_BUILD_TYPE "Release" CACHE
		STRING "Choose the type of build." FORCE)
	# Set the possible values of build type for cmake-gui
	set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
		"Debug" "Release" "RelWithDebInfo")
endif()

# set custom module path for findFFmpeg
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules")

# Platform Flags
if(APPLE)
	set(CMAKE_OSX_DEPLOYMENT_TARGET 10.7)
elseif(FORCE32)
	set(CMAKE_C_FLAGS "-m32")
	set(CMAKE_CXX_FLAGS "-m32")
endif()

# Source lists
add_library(FAudio SHARED
	# public header
	include/FAPOFX.h
	include/FACT.h
	include/FAPOBase.h
	include/FAudio.h
	include/FACT3D.h
	include/F3DAudio.h
	include/FAPO.h
	include/FAudioFX.h
	# internal header
	src/FACT_internal.h
	src/FAudio_internal.h
	src/stb.h
	src/stb_vorbis.h
	# source files
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
# include directory for other targets to consume
target_include_directories(FAudio PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)

# FFmpeg Support
if(FFMPEG)
	# add source file to FAudio target
	target_sources(FAudio PRIVATE src/FAudio_ffmpeg.c)
	target_compile_definitions(FAudio PRIVATE HAVE_FFMPEG=1)

	# find and link add FFMPEG
	find_package(FFmpeg REQUIRED)
	target_include_directories(FAudio PUBLIC ${FFMPEG_INCLUDE_DIR})
	target_link_libraries(FAudio PUBLIC ${FFMPEG_LIBRARIES})
endif(FFMPEG)

# XNA_Song Support
if(XNASONG)
	# add source file to FAudio target
	target_sources(FAudio PRIVATE src/XNA_Song.c)
endif(XNASONG)

# link SDL2 library
find_package(SDL2 CONFIG REQUIRED)
if (TARGET SDL2::SDL2)
	message(STATUS "using TARGET SDL2::SDL2")
	target_link_libraries(FAudio PUBLIC SDL2::SDL2)
elseif (TARGET SDL2)
	message(STATUS "using TARGET SDL2")
	target_link_libraries(FAudio PUBLIC SDL2)
else()
	message(STATUS "no TARGET SDL2::SDL2, or SDL2, using variables")
	target_include_directories(FAudio PUBLIC ${SDL2_INCLUDE_DIRS})
	target_link_libraries(FAudio PUBLIC ${SDL2_LIBRARIES})
endif()

if(BUILD_UTILS)
	# add common ui library used by util targets
	add_library(uicommon STATIC
		utils/uicommon/FAudioUI_main.cpp
		utils/uicommon/FAudioUI_ui.cpp
		utils/uicommon/glfuncs.h
		utils/uicommon/glmacros.h
		utils/uicommon/imconfig.h
		utils/uicommon/imgui.cpp
		utils/uicommon/imgui_demo.cpp
		utils/uicommon/imgui_draw.cpp
		utils/uicommon/imgui.h
		utils/uicommon/imgui_internal.h
		utils/uicommon/stb_rect_pack.h
		utils/uicommon/stb_textedit.h
		utils/uicommon/stb_truetype.h
		)
	target_link_libraries(uicommon PUBLIC FAudio)

	# add wavs library for other libraries to use
	add_library(wavs STATIC utils/wavcommon/wavs.cpp)
	target_compile_definitions(wavs PUBLIC
		"RESOURCE_PATH=${CMAKE_SOURCE_DIR/utils/wavcommon/resources}")

	# add tools
	add_executable(testparse utils/testparse/testparse.c)
	target_link_libraries(testparse PRIVATE uicommon)

	add_executable(facttool utils/facttool/facttool.cpp)
	target_link_libraries(facttool PRIVATE uicommon)

	# add testreverb tool using wavs lib
	add_executable(testreverb
		utils/testreverb/audio.cpp
		utils/testreverb/audio_faudio.cpp
		utils/testreverb/audio.h
		utils/testreverb/audio_xaudio.cpp
		utils/testreverb/testreverb.cpp
		)
	target_link_libraries(testreverb PRIVATE uicommon wavs)

	# add testxma tool using wavs lib
	add_executable(testvolumemeter
		utils/testvolumemeter/audio.cpp
		utils/testvolumemeter/audio_faudio.cpp
		utils/testvolumemeter/audio.h
		utils/testvolumemeter/testvolumemeter.cpp
		)
	target_link_libraries(testvolumemeter PRIVATE uicommon wavs)

	# add testxma tool using just FAudio
	add_executable(testxwma
		utils/testxwma/testxwma.cpp
		)
	target_link_libraries(testxwma PRIVATE FAudio)

	# add testfilter tool
	add_executable(testfilter
		utils/testfilter/audio.cpp
		utils/testfilter/audio_faudio.cpp
		utils/testfilter/audio.h
		utils/testfilter/audio_player.h
		utils/testfilter/audio_xaudio.cpp
		utils/testfilter/oscillator.cpp
		utils/testfilter/oscillator.h
		utils/testfilter/testfilter.cpp
		)
	target_link_libraries(testfilter PRIVATE uicommon)
endif()

# TODO: cpp/ COM Wrapper if MinGW

# TODO: cpp/ Wine DLL if Linux

if(BUILD_TESTS)
	add_executable(faudio_tests
		tests/xaudio2.c
		)
	target_link_libraries(faudio_tests PRIVATE FAudio)
endif()

# install FAudio target and generate cmake-config file
include(CMakePackageConfigHelpers)

configure_package_config_file(
	cmake/config.cmake.in
	${CMAKE_CURRENT_BINARY_DIR}/generated/${PROJECT_NAME}-config.cmake
	INSTALL_DESTINATION lib/cmake/${PROJECT_NAME})

install(
	FILES ${CMAKE_CURRENT_BINARY_DIR}/generated/${PROJECT_NAME}-config.cmake
	DESTINATION lib/cmake/${PROJECT_NAME})

# install public header file
install(
	DIRECTORY include/
	DESTINATION include)

install(
	TARGETS ${PROJECT_NAME}
	EXPORT ${PROJECT_NAME}-targets
	INCLUDES DESTINATION include
	RUNTIME DESTINATION bin
	LIBRARY DESTINATION lib
	ARCHIVE DESTINATION lib/static)

install(
	EXPORT ${PROJECT_NAME}-targets
	NAMESPACE ${PROJECT_NAME}::
	DESTINATION lib/cmake/${PROJECT_NAME})
