# Copyright(C) 2024 by Balazs Cziraki <balazs.cziraki@gmail.com>
#
# Permission to use, copy, modify, and /or distribute this software for any
# purpose with or without fee is hereby granted, provided that the above copyright
# notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
# REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
# FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
# INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
# OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
# TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF
# THIS SOFTWARE.

cmake_minimum_required(VERSION 3.10)

project(cppco)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

option(CPPCO_TEST "Build tests" OFF)
option(CPPCO_USE_INTERNAL_LIBCO "Use the `libco` library bundled with `cppco`" ON)

if (CPPCO_USE_INTERNAL_LIBCO)
	if (MSVC)
		set(LIBCO_FORCE_FALLBACK ON CACHE BOOL "`cppco` uses exceptions to correctly handle object lifetimes, but libco's default configuration has a bug that causes exceptions to not be caught in Windows builds.")
	endif(MSVC)
	add_subdirectory(thirdparty/libco_cmake)
	set_property(TARGET libco PROPERTY FOLDER "thirdparty")
endif (CPPCO_USE_INTERNAL_LIBCO)

set(CPPCO_HDRS ${CMAKE_CURRENT_LIST_DIR}/include/co.hpp ${CMAKE_CURRENT_LIST_DIR}/include/co.ipp)
if (${CMAKE_VERSION} VERSION_GREATER_EQUAL 3.19)
	add_library(cppco INTERFACE ${CPPCO_HDRS})
else (${CMAKE_VERSION} VERSION_GREATER_EQUAL 3.19)
	add_library(cppco INTERFACE)
	target_sources(cppco INTERFACE ${CPPCO_HDRS})
endif (${CMAKE_VERSION} VERSION_GREATER_EQUAL 3.19)
source_group(TREE ${CMAKE_CURRENT_LIST_DIR} PREFIX "Header Files" FILES ${CPPCO_HDRS})
set_source_files_properties(${CPPCO_HDRS} PROPERTIES HEADER_FILE_ONLY ON)
if (CPPCO_USE_INTERNAL_LIBCO)
	target_link_libraries(cppco INTERFACE libco)
	target_include_directories(cppco INTERFACE include)
endif (CPPCO_USE_INTERNAL_LIBCO)
install(FILES ${CPPCO_HDRS} DESTINATION include)

if(CPPCO_TEST)
	include(CTest)

	if (MSVC)
		set(gtest_force_shared_crt ON CACHE BOOL "Use the Shared C Runtime Library for GTest")
	endif (MSVC)
	set(INSTALL_GTEST OFF CACHE BOOL "Do not install GTest")
	add_subdirectory(thirdparty/googletest)
	set_property(TARGET gtest PROPERTY FOLDER "thirdparty/googletest")
	set_property(TARGET gmock PROPERTY FOLDER "thirdparty/googletest")
	set_property(TARGET gtest_main PROPERTY FOLDER "thirdparty/googletest")
	set_property(TARGET gmock_main PROPERTY FOLDER "thirdparty/googletest")

	set(TEST_SOURCES 
			test/example.cpp
			test/test.cpp
			test/libco_mock.hpp
			test/fixture.hpp
			test/fixture.cpp
	)

	function(make_test)
		set(options INTEROP CUSTOM_STATUS)
		set(oneValueArgs TARGET_NAME)
		set(multiValueArgs SOURCES)
		cmake_parse_arguments(MAKE_TEST "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
		add_executable(${MAKE_TEST_TARGET_NAME}
			${MAKE_TEST_SOURCES}
		)
		target_link_libraries(${MAKE_TEST_TARGET_NAME} PRIVATE cppco gmock_main)
		if(MAKE_TEST_CUSTOM_STATUS)
			target_compile_definitions(${MAKE_TEST_TARGET_NAME} PRIVATE CPPCO_CUSTOM_STATUS)
		endif(MAKE_TEST_CUSTOM_STATUS)
		if(MAKE_TEST_INTEROP)
			target_compile_definitions(test_cppco_libco_interop PRIVATE CPPCO_LIBCO_INTEROP)
		endif(MAKE_TEST_INTEROP)
		set_property(TARGET ${MAKE_TEST_TARGET_NAME} PROPERTY CXX_STANDARD_REQUIRED ON)
		set_property(TARGET ${MAKE_TEST_TARGET_NAME} PROPERTY CXX_STANDARD 11)
		set_property(TARGET ${MAKE_TEST_TARGET_NAME} PROPERTY CXX_EXTENSIONS OFF)
		if(MSVC)
			target_compile_options(${MAKE_TEST_TARGET_NAME} PRIVATE /W4 /WX /permissive- $<$<CONFIG:DEBUG>:/ZI>)
			target_link_options(${MAKE_TEST_TARGET_NAME} PRIVATE $<$<CONFIG:DEBUG>:/INCREMENTAL>)
		else(MSVC)
			target_compile_options(${MAKE_TEST_TARGET_NAME} PRIVATE -Wall -Wextra -Werror -pedantic -pedantic-errors)
		endif(MSVC)
		if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
			target_compile_options(${MAKE_TEST_TARGET_NAME} PRIVATE -Wno-c++17-attribute-extensions)
		endif (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
		add_test(NAME ${MAKE_TEST_TARGET_NAME} COMMAND ${MAKE_TEST_TARGET_NAME})
	endfunction(make_test)

	make_test(TARGET_NAME test_cppco SOURCES ${TEST_SOURCES} CUSTOM_STATUS)
	make_test(TARGET_NAME test_cppco_libco_interop SOURCES ${TEST_SOURCES} CUSTOM_STATUS INTEROP)
	make_test(TARGET_NAME test_cppco_compile SOURCES test/compile.cpp)
	make_test(TARGET_NAME test_cppco_compile_libco_interop SOURCES test/compile.cpp INTEROP)

	set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT test_cppco)

endif(CPPCO_TEST)
