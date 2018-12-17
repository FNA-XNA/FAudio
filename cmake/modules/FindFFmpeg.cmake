# Copyright (c) 2014 Matt Coffin <mcoffin13@gmail.com>
#
# This software is provided 'as-is', without any express or implied
# warranty. In no event will the authors be held liable for any damages
# arising from the use of this software.
#
# Permission is granted to anyone to use this software for any purpose,
# including commercial applications, and to alter it and redistribute it
# freely, subject to the following restrictions:
#
# 1. The origin of this software must not be misrepresented; you must not
#    claim that you wrote the original software. If you use this software
#    in a product, an acknowledgment in the product documentation would be
#    appreciated but is not required.
# 2. Altered source versions must be plainly marked as such, and must not be
#    misrepresented as being the original software.
# 3. This notice may not be removed or altered from any source distribution.

# - Try to find FFmpeg
# Once done this will define
#   FFmpeg_FOUND
#   FFmpeg_INCLUDE_DIRS
#   FFmpeg_LIBRARIES
#   FFmpeg_INCLUDE_FILES
# Author: Matt Coffin <mcoffin13@gmail.com>

# 2018-11-29: NeroBurner: add FFmpeg_INCLUDE_DIRS and FFmpeg_LIBRARY_DIRS as hints for all targets
# 2018-12-03: NeroBurner: use FFmpeg_INCLUDE_DIR to resolve name collision, ignore root path if set
# 2018-12-10: NeroBurner: fix detection whether FFmpeg_INCLUDE_DIR is set

include(FindPackageHandleStandardArgs)

if (NOT FFmpeg_FIND_COMPONENTS)
	set(FFmpeg_FIND_COMPONENTS avcodec avdevice avfilter avformat avutil swresample swscale)
endif(NOT FFmpeg_FIND_COMPONENTS)

# Generate component include files and requirements
foreach(comp ${FFmpeg_FIND_COMPONENTS})
	if(FFmpeg_FIND_REQUIRED_${comp})
		list(APPEND required "FFmpeg_${comp}_FOUND")
	endif()
endforeach(comp)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
	set(_lib_suffix 64)
else()
	set(_lib_suffix 32)
endif()

set(FFMPEG_PATH_ARCH FFmpegPath${_lib_suffix})
# Find libraries
find_package(PkgConfig QUIET)
foreach(comp ${FFmpeg_FIND_COMPONENTS})
	if(PKG_CONFIG_FOUND)
		pkg_check_modules(_${comp} QUIET lib${comp})
	endif()
	string(COMPARE NOTEQUAL "${FFmpeg_INCLUDE_DIR}" "" ffmpeg_include_dir_set)
	if(ffmpeg_include_dir_set)
		# search just provided directory
		find_path(FFmpeg_${comp}_INCLUDE_DIR
			"lib${comp}/${comp}.h"
			PATHS
			    ${FFmpeg_INCLUDE_DIR}
			PATH_SUFFIXES ffmpeg libav
			NO_CMAKE_FIND_ROOT_PATH
			NO_CMAKE_ENVIRONMENT_PATH
			NO_SYSTEM_ENVIRONMENT_PATH
			NO_DEFAULT_PATH)
		find_library(FFmpeg_${comp}_LIBRARY
			NAMES ${comp} ${comp}-ffmpeg ${_${comp}_LIBRARIES}
			PATHS
			    "${FFmpeg_${comp}_INCLUDE_DIR}/../lib"
				"${FFmpeg_${comp}_INCLUDE_DIR}/../lib${_lib_suffix}"
				"${FFmpeg_${comp}_INCLUDE_DIR}/../libs${_lib_suffix}"
				"${FFmpeg_${comp}_INCLUDE_DIR}/lib"
				"${FFmpeg_${comp}_INCLUDE_DIR}/lib${_lib_suffix}"
				${FFmpeg_LIBRARY_DIRS}
				${_${comp}_LIBRARY_DIRS}
			PATH_SUFFIXES ${comp} lib${comp}
			DOC "FFmpeg ${comp} library"
			NO_CMAKE_FIND_ROOT_PATH
			NO_CMAKE_ENVIRONMENT_PATH
			NO_SYSTEM_ENVIRONMENT_PATH
			NO_DEFAULT_PATH)
	endif()

	# find path with generic include paths
	find_path(FFmpeg_${comp}_INCLUDE_DIR
		NAMES "lib${comp}/${comp}.h"
		HINTS
			${FFmpeg_INCLUDE_DIR}
			${_${comp}_INCLUDE_DIRS}
			ENV FFmpegPath
			ENV ${FFMPEG_PATH_ARCH}
		PATHS
			/usr/include /usr/local/include /opt/local/include /sw/include
		PATH_SUFFIXES ffmpeg libav
		DOC "FFmpeg include directory")
	find_library(FFmpeg_${comp}_LIBRARY
		NAMES ${comp} ${comp}-ffmpeg ${_${comp}_LIBRARIES}
		HINTS
			${FFmpeg_LIBRARY_DIRS}
			${_${comp}_LIBRARY_DIRS}
			"${FFmpeg_${comp}_INCLUDE_DIR}/../lib"
			"${FFmpeg_${comp}_INCLUDE_DIR}/../lib${_lib_suffix}"
			"${FFmpeg_${comp}_INCLUDE_DIR}/../libs${_lib_suffix}"
			"${FFmpeg_${comp}_INCLUDE_DIR}/lib"
			"${FFmpeg_${comp}_INCLUDE_DIR}/lib${_lib_suffix}"
		PATHS
			/usr/lib /usr/local/lib /opt/local/lib /sw/lib
		PATH_SUFFIXES ${comp} lib${comp}
		DOC "FFmpeg ${comp} library")
	find_package_handle_standard_args(FFmpeg_${comp}
		FOUND_VAR FFmpeg_${comp}_FOUND
		REQUIRED_VARS FFmpeg_${comp}_LIBRARY FFmpeg_${comp}_INCLUDE_DIR)
	if(${FFmpeg_${comp}_FOUND})
		list(APPEND FFmpeg_INCLUDE_DIRS ${FFmpeg_${comp}_INCLUDE_DIR})
		list(APPEND FFmpeg_LIBRARIES ${FFmpeg_${comp}_LIBRARY})
	endif()
endforeach(comp)

# Run checks via find_package_handle_standard_args
find_package_handle_standard_args(FFmpeg
	FOUND_VAR FFmpeg_FOUND
	REQUIRED_VARS ${required} FFmpeg_INCLUDE_DIRS FFmpeg_LIBRARIES)
# remove duplicate include dir entries
list(REMOVE_DUPLICATES FFmpeg_INCLUDE_DIRS)
