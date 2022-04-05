if	(NOT CXX_STANDARD_VERSION_FLAG)
	message(FATAL_ERROR "Cannot compile modules without CXX_STANDARD_VERSION_FLAG")
endif()

if	(NOT CXX_STANDARD_LIBRARY_FLAG)
	message(WARNING "Compiling modules without CXX_STANDARD_LIBRARY_FLAG")
endif()

if	(NOT WARNING_FLAGS)
	message(WARNING "Compiling modules without WARNING_FLAGS")
endif()

if	(NOT ADDITIONAL_COMPILE_OPTIONS)
	message(WARNING "Compiling modules without ADDITIONAL_COMPILE_OPTIONS")
endif()

set(PREBUILT_MODULE_PATH
	${CMAKE_BINARY_DIR}/modules
CACHE STRING
	"The directory in which prebuild modules are stored."
)
set(MODULE_CACHE_PATH
	${CMAKE_BINARY_DIR}/modules/cache
CACHE STRING
	"The directory in which prebuild header units are stored."
)

file(
MAKE_DIRECTORY
	${PREBUILT_MODULE_PATH}
)
file(
MAKE_DIRECTORY
	${MODULE_CACHE_PATH}
)

set(MODULE_INTERFACE_EXTENSION
	.pcm
CACHE STRING
	"The extension used for prebuild module files."
)
set(MODULE_FLAGS
	-fmodules
	-fprebuilt-module-path=${PREBUILT_MODULE_PATH}
	-fbuiltin-module-map
	-fmodules-cache-path=${MODULE_CACHE_PATH}
	-Xclang -fdisable-module-hash
)

set(CLANG_PROJECT_MODULE_MAP
	${CMAKE_SOURCE_DIR}/module.modulemap
CACHE STRING
	"A file used by clang which specifies how headers are mapped to modules."
)

if (EXISTS ${CLANG_PROJECT_MODULE_MAP})
	list(APPEND MODULE_FLAGS -fmodule-map-file=${CLANG_PROJECT_MODULE_MAP})
else()
	message(WARNING "Building header units with clang requires file ${CLANG_PROJECT_MODULE_MAP}!")
endif()

function(
	add_module_source_header_units
	target_name
	source_file
	module_header_units
)
	# handled implicitly
endfunction()

function(
	get_compile_module_interface_command
	module_interface_file
	module_binary
	include_directories
	out_command
)
	if	(include_directories)
		set(absolute_include_dirs "")
		foreach(include_dir IN LISTS include_directories)
			file(REAL_PATH ${include_dir} absolute_dir)
			list(APPEND absolute_include_dirs -I${absolute_dir})
		endforeach()
	endif()

	file(REAL_PATH
		${module_interface_file}
		real_module_interface_file
		EXPAND_TILDE
	)

	set(
	command
		${CMAKE_CXX_COMPILER}
		${CXX_STANDARD_VERSION_FLAG}
		${CXX_STANDARD_LIBRARY_FLAG}
		${WARNING_FLAGS}
		${MODULE_FLAGS}
		${ADDITIONAL_COMPILE_OPTIONS}
		${absolute_include_dirs}
		--compile ${real_module_interface_file}
		-Xclang -emit-module-interface
		--output ${module_binary}
	)

	set(
	${out_command}
		${command}
	PARENT_SCOPE
	)

endfunction()
