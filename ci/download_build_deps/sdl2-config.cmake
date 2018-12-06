#*!
#*******************************************************************************
#*
#* \file    sdl2-config.cmake
#* \brief   CMake config file for SDL2 library C/C++ interface
#* \version 2018-12-10/NeroBurner: Initial version
#*
#*  In the project's CMakeLists.txt file write:
#*
#*      find_package(SDL2)
#*
#*  which will define two libraries: "SDL2::SDL2" and "SDL2::SDL2main" that
#*  can be used in the target_link_libraries command like so:
#*
#*      target_link_libraries( my_program SDL2::SDL2 )
#*
#*******************************************************************************
#*/

message(STATUS "SDL2: providing targets SDL2::SDL2 and SDL2::SDL2main")

# get root directory of the rdb package
get_filename_component(_IMPORT_PREFIX "${CMAKE_CURRENT_LIST_FILE}" PATH)
get_filename_component(SDL2_ROOT_DIR ${_IMPORT_PREFIX}/ DIRECTORY)
get_filename_component(SDL2_ROOT_DIR ${SDL2_ROOT_DIR}/ DIRECTORY)

# define include and library dir
set (SDL2_INCLUDE_DIR  "${SDL2_ROOT_DIR}/include")
set (SDL2_BIN_DIR      "${SDL2_ROOT_DIR}/bin")
set (SDL2_LIB_DIR      "${CMAKE_CURRENT_LIST_DIR}")

# create imported target SDL2::SDL2
if(TARGET SDL2::SDL2)
	get_target_property(_old_include_dir SDL2::SDL2 INTERFACE_INCLUDE_DIRECTORIES)
	string(COMPARE NOTEQUAL "${_old_include_dir}" "${SDL2_INCLUDE_DIR}" _include_dirs_differ)
	if(_include_dirs_differ)
		message(FATAL_ERROR "Target SDL2::SDL2 was previously defined with a different INCLUDE_DIRECTORY
			old: ${_old_include_dir}
			new: ${SDL2_INCLUDE_DIR}")
	endif()
else()
	add_library(SDL2::SDL2 SHARED IMPORTED GLOBAL)
	if(WIN32)
		set_property(TARGET SDL2::SDL2 PROPERTY IMPORTED_LOCATION "${SDL2_LIB_DIR}/SDL2.dll")
		set_property(TARGET SDL2::SDL2 PROPERTY IMPORTED_IMPLIB   "${SDL2_LIB_DIR}/SDL2.lib")
	else() #expect g++ on linux
		set_property(TARGET SDL2::SDL2 PROPERTY IMPORTED_LOCATION "${SDL2_LIB_DIR}/SDL2.so")
		set_property(TARGET SDL2::SDL2 PROPERTY IMPORTED_IMPLIB   "${SDL2_LIB_DIR}/SDL2.so")
	endif()
	# add libraries for SDL2::SDL2
	set_property(TARGET SDL2::SDL2 PROPERTY
		INTERFACE_INCLUDE_DIRECTORIES
		"${SDL2_INCLUDE_DIR}"
		)
endif()

# create compiled target SDL2::SDL2main
if(TARGET SDL2::SDL2main)
	get_target_property(_old_include_dir SDL2::SDL2main INTERFACE_INCLUDE_DIRECTORIES)
	string(COMPARE EQUAL "${_old_include_dir}" "${SDL2_INCLUDE_CPP_DIR}" _matches)
	if(NOT _matches)
		message(FATAL_ERROR "Target SDL2::SDL2main was previously defined with a different INCLUDE_DIRECTORY
			old: ${_old_include_dir}
			new: ${SDL2_INCLUDE_CPP_DIR}")
	endif()
else()
	add_library(SDL2::SDL2main STATIC IMPORTED GLOBAL)
	if(WIN32)
		set_property(TARGET SDL2::SDL2main PROPERTY IMPORTED_LOCATION "${SDL2_LIB_DIR}/SDL2main.dll")
		set_property(TARGET SDL2::SDL2main PROPERTY IMPORTED_IMPLIB   "${SDL2_LIB_DIR}/SDL2main.lib")
	else() #expect g++ on linux
		set_property(TARGET SDL2::SDL2main PROPERTY IMPORTED_LOCATION "${SDL2_LIB_DIR}/SDL2main.so")
		set_property(TARGET SDL2::SDL2main PROPERTY IMPORTED_IMPLIB   "${SDL2_LIB_DIR}/SDL2main.so")
	endif()
	# add libraries for SDL2::SDL2main
	set_property(TARGET SDL2::SDL2main PROPERTY
		INTERFACE_INCLUDE_DIRECTORIES
		"${SDL2_INCLUDE_DIR}"
		)
	target_link_libraries(SDL2::SDL2main INTERFACE SDL2::SDL2)
endif()

set(_IMPORT_PREFIX)
