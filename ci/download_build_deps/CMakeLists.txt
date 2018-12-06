cmake_minimum_required(VERSION 3.7)
project(build_dependency_downloader NONE) # we need no compiler language for downloading

include(ExternalProject)

# download SDL2
ExternalProject_Add(
	sdl2_win32_vc
	URL "https://www.libsdl.org/release/SDL2-devel-2.0.9-VC.zip"
	SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/sdl2_win32_vc
	CONFIGURE_COMMAND ""
	# copy hand written cmake config file into lib directories,
	# the upstream package does not supply one
	BUILD_COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/sdl2-config.cmake" "${CMAKE_CURRENT_SOURCE_DIR}/sdl2_win32_vc/lib/x64/sdl2-config.cmake"
	INSTALL_COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/sdl2-config.cmake" "${CMAKE_CURRENT_SOURCE_DIR}/sdl2_win32_vc/lib/x86/sdl2-config.cmake"
)

# download FFMpeg
# download development package
ExternalProject_Add(
	ffmpeg_win64
	URL "https://ffmpeg.zeranoe.com/builds/win64/dev/ffmpeg-4.1-win64-dev.zip"
	SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/ffmpeg_win64
	CONFIGURE_COMMAND ""
	BUILD_COMMAND ""
	INSTALL_COMMAND ""
)

# download FFMpeg shared runtime binaries
ExternalProject_Add(
	ffmpeg_win64_bin_shared
	URL "https://ffmpeg.zeranoe.com/builds/win64/shared/ffmpeg-4.1-win64-shared.zip"
	SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/ffmpeg_win64_bin
	CONFIGURE_COMMAND ""
	# copy the shared libraries into the development folder
	BUILD_COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/ffmpeg_win64_bin/bin" "${CMAKE_CURRENT_SOURCE_DIR}/ffmpeg_win64/bin"
	INSTALL_COMMAND ""
	DEPENDS ffmpeg_win64
)

