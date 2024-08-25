# Copyright(C) 2024 by Balazs Cziraki <balazs.cziraki@gmail.com>
#
# Permission to use, copy, modify, and /or distribute this software for any
# purpose with or without fee is hereby granted, provided that the above copyright
# notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
# REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
# FITNESS.IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
# INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
# OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
# TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF
# THIS SOFTWARE.

cmake_minimum_required(VERSION 3.10)

project(cppco)

option(CPPCO_TEST "Build tests" OFF)
option(CPPCO_USE_INTERNAL_LIBCO "Use the `libco` library bundled with `cppco`" ON)

if (CPPCO_USE_INTERNAL_LIBCO)
	if (MSVC)
		set(LIBCO_FORCE_FALLBACK ON CACHE BOOL "`cppco` uses exceptions to correctly handle object lifetimes, but libco's default configuration has a bug that causes exceptions to not be caught in Windows builds.")
	endif(MSVC)
	add_subdirectory(thirdparty/libco_cmake)
endif (CPPCO_USE_INTERNAL_LIBCO)

set(CPPCO_HDRS ${CMAKE_CURRENT_LIST_DIR}/include/co.hpp ${CMAKE_CURRENT_LIST_DIR}/include/co.ipp)
if (CMAKE_VERSION VERSION_GREATER_EQUAL 3.19)
	add_library(cppco INTERFACE ${CPPCO_HDRS})
else (CMAKE_VERSION VERSION_GREATER_EQUAL 3.19)
	add_library(cppco INTERFACE)
	target_sources(cppco INTERFACE ${CPPCO_HDRS})
endif (CMAKE_VERSION VERSION_GREATER_EQUAL 3.19)
source_group(TREE ${CMAKE_CURRENT_LIST_DIR} PREFIX "Header Files" FILES ${CPPCO_HDRS})
set_source_files_properties(${CPPCO_HDRS} PROPERTIES HEADER_FILE_ONLY ON)
if (CPPCO_USE_INTERNAL_LIBCO)
	target_link_libraries(cppco INTERFACE libco)
	target_include_directories(cppco INTERFACE include)
endif (CPPCO_USE_INTERNAL_LIBCO)

if(CPPCO_TEST)
	include(CTest)
	if (MSVC)
		option(gtest_force_shared_crt "Use the Shared C Runtime Library for GTest" ON)
	endif (MSVC)
	add_subdirectory(thirdparty/googletest)
	add_executable(test_cppco test/main.cpp test/libco_mock.hpp)
	target_link_libraries(test_cppco PRIVATE cppco gmock_main)
	set_property(TARGET test_cppco PROPERTY CXX_STANDARD_REQUIRED ON)
	set_property(TARGET test_cppco PROPERTY CXX_STANDARD 17)
	set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT test_cppco)
	if(MSVC)
		target_compile_options(test_cppco PRIVATE /W4 /WX /permissive- $<$<CONFIG:DEBUG>:/ZI>)
		target_link_options(test_cppco PRIVATE $<$<CONFIG:DEBUG>:/INCREMENTAL>)
	else(MSVC)
		target_compile_options(test_cppco PRIVATE -Wall -Wextra -Werror -pedantic -pedantic-errors)
	endif(MSVC)
	add_test(NAME test_cppco COMMAND test_cppco)
endif(CPPCO_TEST)
