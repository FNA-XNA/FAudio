# Written by @NeroBurner
# install all libraries to DESTINATION shared_lib
# symlinks are resolved and both the symlink and the target are installed

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
		string(REGEX MATCH "\\.(dll.a$|lib$)" is_static "${lib}")
		if(is_static)
			# lib is a dynamic wrapper ending in .dll.a, search for the linked
			# dll in the ../bin directory
			get_filename_component(dynlib_name ${lib} NAME_WE)
			string(REGEX REPLACE "^lib" "" dynlib_simple "${dynlib_name}")
			get_filename_component(linker_search_dir ${lib} DIRECTORY)
			get_filename_component(linker_search_dir ${linker_search_dir} DIRECTORY)
			file(GLOB dyn_lib
				"${linker_search_dir}/bin/${dynlib_simple}*.dll"
				"${linker_search_dir}/lib/${dynlib_simple}*.dll"
				LIST_DIRECTORIES false)
			if(dyn_lib)
				# found the dynamic library
				set(lib "${dyn_lib}")
			endif()
		endif()
		string(REGEX MATCH "\\.(so|dll$)" is_shared_lib "${lib}")
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
