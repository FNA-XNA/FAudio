cmake_minimum_required(VERSION 3.7)
project(FAudio)

# Options
option(FORCE32 "Force 32-bit on 64-bit (ignored on macOS)" OFF)
option(FFMPEG "Enable FFmpeg support (WMA, XMA)" OFF)
option(BUILD_UTILS "Build utils/ folder" OFF)
option(BUILD_CPP "Build cpp/ folder (COM for Win32, Wine for Unix)" OFF)
option(BUILD_TESTS "Build tests/ folder for unit tests to be executed on the host against FAudio" OFF)
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
elseif(WIN32)
	# we want windows libraries like FAudio.dll instead of libFAudio.dll
	set(CMAKE_SHARED_LIBRARY_PREFIX "")
elseif(FORCE32)
	set(CMAKE_C_FLAGS "-m32")
	set(CMAKE_CXX_FLAGS "-m32")
endif()

# install all libraries to DESTINATION shared_lib # symlinks are resolved and both the symlink and the target are installed
function(install_shared_libs)
	set(shared_libs_destination ${ARGV0})
	list(REMOVE_AT ARGN 0)
	foreach(lib ${ARGN})
		if(TARGET ${lib})
			# special handling for cmake targets
			get_target_property(lib_location ${lib} IMPORTED_LOCATION)
			if(NOT lib_location)
				get_target_property(lib_location ${lib} IMPORTED_LOCATION_RELEASE)
			endif()
			if(NOT lib_location)
				get_target_property(lib_location ${lib} IMPORTED_LOCATION_NOCONFIG)
			endif()
			if(NOT lib_location)
				get_target_property(lib_location ${lib} IMPORTED_LOCATION_DEBUG)
			endif()
			install_shared_libs(${shared_libs_destination} ${lib_location})
			continue()
		endif()

		# check if it is a linker flag
		# for example  -L/usr/x86_64-w64-mingw32/lib  -lmingw32 -lSDL2main -lSDL2 -mwindows
		string(REGEX MATCH "-L[^ \t]+" linker_search_dir ${lib})
		string(REGEX MATCHALL "[ \t;]-l[^ \t]+" linker_libs " ${lib}")
		string(REGEX REPLACE "^-L" "" linker_search_dir "${linker_search_dir}")
		if(linker_libs)
			foreach(linker_lib ${linker_libs})
				string(REGEX REPLACE "^[ \t;]-l" "" linker_lib ${linker_lib})
				find_library(lib_path ${linker_lib}
					HINTS "${linker_search_dir}")
				if(lib_path)
					install_shared_libs(${shared_libs_destination} ${lib_path})
					unset(lib_path CACHE)
					continue()
				endif()
			endforeach()
			continue()
		endif()

		# got an absolute path for a library
		string(REGEX MATCH "\.(dll.a$)" is_static "${lib}")
		if(is_static)
			# lib is a dynamic wrapper ending in .dll.a, search for the linked
			# dll in the ../bin directory
			get_filename_component(dynlib_name ${lib} NAME_WE)
			string(REGEX REPLACE "^lib" "" dynlib_simple "${dynlib_name}")
			get_filename_component(linker_search_dir ${lib} DIRECTORY)
			get_filename_component(linker_search_dir ${linker_search_dir} DIRECTORY)
			file(GLOB dyn_lib
				"${linker_search_dir}/bin/${dynlib_simple}*.dll"
				LIST_DIRECTORIES false)
			if(dyn_lib)
				# found the dynamic library
				set(lib "${dyn_lib}")
			endif()
		endif()
		string(REGEX MATCH "\.(so|dll$)" is_shared_lib "${lib}")
		if(NOT is_shared_lib)
			# don't install static libraries
			message(STATUS "ignore static lib: ${lib}")
			continue()
		endif()
		message(STATUS "install shared lib: ${lib}")
		# resolve symlink
		get_filename_component(lib_REAL ${lib} REALPATH)
		# install symlink and realpath
		install(FILES ${lib}      DESTINATION ${shared_libs_destination})
		install(FILES ${lib_REAL} DESTINATION ${shared_libs_destination})
	endforeach()
endfunction()

option(INSTALL_DEPENDENT_LIBRARIES "Add dependent libraries to install target" OFF)

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
	find_package(FFmpeg COMPONENTS avcodec
		avutil # dep of avcodec
		swresample # dep of avcodec
		REQUIRED)
	target_include_directories(FAudio PRIVATE ${FFmpeg_INCLUDE_DIRS})
	target_link_libraries(FAudio PUBLIC ${FFmpeg_LIBRARIES})
	if(INSTALL_DEPENDENT_LIBRARIES)
		install_shared_libs(bin ${FFmpeg_LIBRARIES})
	endif()
endif(FFMPEG)

# MinGW builds should statically link libgcc
if(MINGW)
	target_link_libraries(FAudio PRIVATE -static-libgcc)
endif()

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
	if(INSTALL_DEPENDENT_LIBRARIES)
		install_shared_libs(bin SDL2::SDL2)
	endif()
elseif (TARGET SDL2)
	message(STATUS "using TARGET SDL2")
	target_link_libraries(FAudio PUBLIC SDL2)
	if(INSTALL_DEPENDENT_LIBRARIES)
		install_shared_libs(bin SDL2)
	endif()
else()
	message(STATUS "no TARGET SDL2::SDL2, or SDL2, using variables")
	target_include_directories(FAudio PUBLIC ${SDL2_INCLUDE_DIRS})
	target_link_libraries(FAudio PUBLIC ${SDL2_LIBRARIES})
	if(INSTALL_DEPENDENT_LIBRARIES)
		install_shared_libs(bin ${SDL2_LIBRARIES})
	endif()
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

if(BUILD_CPP)
	# cpp/ COM Wrapper if MinGW
	# TODO: only if MinGW
	# TODO: cpp/ Wine DLL if Linux

	add_library(XAPOBase STATIC
		cpp/XAPOBase.cpp
		)
	target_compile_definitions(XAPOBase PUBLIC
		FAUDIOCPP_EXPORTS
		_WIN32_WINNT=0x0600)
	target_link_libraries(XAPOBase PUBLIC
		ole32
		FAudio
		)
	if(GCC)
		target_link_libraries(XAPOBase PUBLIC
			"-Wl,--enable-stdcall-fixup"
			"-static-libgcc -static-libstdc++"
			)
		# enable various compiler warnings
		target_link_libraries(XAPOBase PUBLIC
			-Wall -pedantic
			-Wsign-compare
			-Wshadow
			)
	endif()

	set(FAudio_CPP_libs "")
	# Source lists
	foreach(lib_version RANGE 0 7)
		set(faudio_cpp_target "xaudio2_${lib_version}")
		add_library(${faudio_cpp_target} SHARED
			cpp/com_utils.cpp
			cpp/xaudio2.cpp
			cpp/XAudio2fx.cpp
			cpp/win_registry.cpp
			)
		target_compile_definitions(${faudio_cpp_target} PRIVATE
			"XAUDIO2_VERSION=${lib_version}")
		list(APPEND FAudio_CPP_libs ${faudio_cpp_target})
	endforeach()

	foreach(lib_version RANGE 8 9)
		set(faudio_cpp_target "xaudio2_${lib_version}")
		add_library(${faudio_cpp_target} SHARED
			cpp/com_utils.cpp
			cpp/xaudio2.cpp
			cpp/XAudio2fx.cpp
			cpp/win_device.cpp
			cpp/X3DAudio.cpp
			cpp/XAPOFX.cpp
			)
		target_compile_definitions(${faudio_cpp_target} PRIVATE
			"XAUDIO2_VERSION=${lib_version}"
			UNICODE)
		target_link_libraries(${faudio_cpp_target} PRIVATE setupapi)
		list(APPEND FAudio_CPP_libs ${faudio_cpp_target})
	endforeach()

	foreach(lib_version RANGE 3 7)
		set(faudio_cpp_target "x3daudio1_${lib_version}")
		add_library(${faudio_cpp_target} SHARED
			cpp/X3DAudio.cpp
			)
		list(APPEND FAudio_CPP_libs ${faudio_cpp_target})
	endforeach()

	foreach(lib_version RANGE 1 5)
		set(faudio_cpp_target "xapofx1_${lib_version}")
		add_library(${faudio_cpp_target} SHARED
			cpp/com_utils_xapofx.cpp
			cpp/XAPOFX.cpp
			)
		list(APPEND FAudio_CPP_libs ${faudio_cpp_target})
	endforeach()

	foreach(lib_version RANGE 0 7)
		set(faudio_cpp_target "xactengine3_${lib_version}")
		add_library(${faudio_cpp_target} SHARED
			cpp/com_utils_xact.cpp
			cpp/xact3.cpp
			cpp/win_registry.cpp
			cpp/win_iostream.cpp
			)
		target_compile_definitions(${faudio_cpp_target} PRIVATE
			"XACT3_VERSION=${lib_version}")
		list(APPEND FAudio_CPP_libs ${faudio_cpp_target})
	endforeach()

	# link base libarary to all other libraries
	foreach(faudio_cpp_target ${FAudio_CPP_libs})
		target_link_libraries(${faudio_cpp_target} PRIVATE XAPOBase)
		# MinGW builds should statically link libgcc
		if(MINGW)
			target_link_libraries(${faudio_cpp_target} PRIVATE
				-static-libgcc -static-libstdc++)
		endif()
	endforeach()

	# add libraries to install target
	install(TARGETS ${FAudio_CPP_libs}
		RUNTIME DESTINATION bin
		LIBRARY DESTINATION lib
		ARCHIVE DESTINATION lib/static)
	# add library install script to install target
	install(PROGRAMS cpp/scripts/wine_setup_native
		DESTINATION bin)
	if(MINGW AND INSTALL_DEPENDENT_LIBRARIES)
		# install runtime dependency
		install_shared_libs(bin -lwinpthread-1 -lwinpthread -lSDL2 -liconv)
	endif()
endif() # BUILD_CPP

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
