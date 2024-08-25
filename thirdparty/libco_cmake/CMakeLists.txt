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

# This is a supplementary CMakeLists file for libco made for the cppco wrapper
# library, as libco doesn't have its own CMakeLists.txt file.

cmake_minimum_required(VERSION 3.10)

project(libco)

# Options
option(LIBCO_FORCE_FALLBACK "Force the use of the fallback implementation. (Fibers on Windows, SJLJ otherwise.)" OFF)
option(LIBCO_MPROTECT "libco uses text sections to mark code as execuable. If this is not supported, set this option to ON to use mprotect instead. (Applicable to amd64, arm, ppc, x86)" "$<BOOL:$<IF:$<CXX_COMPILER_ID:MSVC>>:ON:OFF>>")
option(LIBCO_NO_SSE "Win64 only: Provides a substantial speed-up, but will thrash XMM regs. Do not use this unless you are certain your application won't use SSE!" OFF)

# Sources
set(LIBCO_DIR ${CMAKE_CURRENT_LIST_DIR}/../libco)

set(LIBCO_MAIN_HDRS
	${LIBCO_DIR}/libco.h
)
set(LIBCO_MAIN_SRCS
	${LIBCO_DIR}/libco.c
)
set(LIBCO_MAIN_FILES ${LIBCO_MAIN_HDRS} ${LIBCO_MAIN_SRCS})

set(LIBCO_ADDITIONAL_HDRS
	${LIBCO_DIR}/settings.h
	${LIBCO_DIR}/valgrind.h
)
set(LIBCO_ADDITIONAL_SRCS
	${LIBCO_DIR}/aarch64.c
	${LIBCO_DIR}/amd64.c
	${LIBCO_DIR}/arm.c
	${LIBCO_DIR}/fiber.c
	${LIBCO_DIR}/ppc.c
	${LIBCO_DIR}/ppc64v2.c
	${LIBCO_DIR}/sjlj.c
	${LIBCO_DIR}/ucontext.c
	${LIBCO_DIR}/x86.c
)
set(LIBCO_ADDITIONAL_FILES ${LIBCO_ADDITIONAL_HDRS} ${LIBCO_ADDITIONAL_SRCS})

set(LIBCO_ALL_FILES ${LIBCO_MAIN_FILES} ${LIBCO_ADDITIONAL_FILES})
set(LIBCO_ALL_HDRS ${LIBCO_MAIN_HDRS} ${LIBCO_ADDITIONAL_HDRS})
set(LIBCO_ALL_SRCS ${LIBCO_MAIN_SRCS} ${LIBCO_ADDITIONAL_SRCS})

# Target
add_library(libco STATIC ${LIBCO_ALL_FILES})
set_target_properties(libco PROPERTIES PREFIX "") # `libco` already contains `lib`
target_include_directories(libco PUBLIC ${LIBCO_DIR})
set_property(TARGET libco PROPERTY C_STANDARD_REQUIRED ON)
set_property(TARGET libco PROPERTY C_STANDARD 11)
set_property(TARGET libco PROPERTY C_EXTENSIONS OFF)
if(MSVC)
	target_compile_options(libco PRIVATE /Zc:__STDC__)
endif(MSVC)
install(TARGETS libco)
install(FILES ${LIBCO_MAIN_HDRS} DESTINATION include)

# Source groups
source_group(TREE ${LIBCO_DIR} PREFIX "Source Files" FILES ${LIBCO_ALL_SRCS})
source_group(TREE ${LIBCO_DIR} PREFIX "Header Files" FILES ${LIBCO_ALL_HDRS})

# Set included C files as headers
set_source_files_properties(${LIBCO_ADDITIONAL_SRCS} PROPERTIES HEADER_FILE_ONLY ON)

# Option LIBCO_FORCE_FALLBACK
if(LIBCO_FORCE_FALLBACK)
	set_source_files_properties(${LIBCO_MAIN_SRCS} PROPERTIES HEADER_FILE_ONLY ON)
	if(WIN32)
		set_source_files_properties(${LIBCO_DIR}/fiber.c PROPERTIES HEADER_FILE_ONLY OFF)
	else(WIN32)
		set_source_files_properties(${LIBCO_DIR}/sjlj.c PROPERTIES HEADER_FILE_ONLY OFF)
	endif(WIN32)
endif(LIBCO_FORCE_FALLBACK)

# Option LIBCO_MPROTECT
if(LIBCO_MPROTECT)
	target_compile_definitions(libco PRIVATE LIBCO_MPROTECT=)
endif(LIBCO_MPROTECT)

# Option LIBCO_NO_SSE
if(LIBCO_NO_SSE)
	target_compile_definitions(libco PRIVATE LIBCO_NO_SSE=)
endif(LIBCO_NO_SSE)
