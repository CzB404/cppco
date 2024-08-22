cmake_minimum_required(VERSION 3.8)

project(libco)

option(LIBCO_FORCE_FALLBACK "Force the use of the fallback implementation. (Fibers on Windows, SJLJ otherwise.)" OFF)
option(LIBCO_MPROTECT "[amd64, arm, ppc, x86]:
by default, co_swap_function is marked as a text (code) section
if not supported, uncomment the below line to use mprotect instead" OFF)
option(LIBCO_NO_SSE "Win64 only: provides a substantial speed-up, but will thrash XMM regs
do not use this unless you are certain your application won't use SSE" OFF)

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
	src/msvc_section_fix.ipp
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

add_library(libco STATIC ${LIBCO_ALL_FILES})
set_source_files_properties(${LIBCO_ADDITIONAL_SRCS} PROPERTIES HEADER_FILE_ONLY ON)
if(LIBCO_FORCE_FALLBACK)
	set_source_files_properties(${LIBCO_MAIN_SRCS} PROPERTIES HEADER_FILE_ONLY ON)
	if(WIN32)
		set_source_files_properties(${LIBCO_DIR}/fiber.c PROPERTIES HEADER_FILE_ONLY OFF)
	else(WIN32)
		set_source_files_properties(${LIBCO_DIR}/sjlj.c PROPERTIES HEADER_FILE_ONLY OFF)
	endif(WIN32)
endif(LIBCO_FORCE_FALLBACK)
if(LIBCO_MPROTECT)
	target_compile_definitions(libco PRIVATE LIBCO_MPROTECT=)
else(LIBCO_MPROTECT)
	if(WIN32)
		set_source_files_properties(${LIBCO_DIR}/libco.c PROPERTIES COMPILE_FLAGS /FI"${CMAKE_CURRENT_LIST_DIR}/src/msvc_section_fix.ipp")
	endif(WIN32)
endif(LIBCO_MPROTECT)
if(LIBCO_NO_SSE)
	target_compile_definitions(libco PRIVATE LIBCO_NO_SSE=)
endif(LIBCO_NO_SSE)
source_group(TREE ${LIBCO_DIR} PREFIX "Source Files" FILES ${LIBCO_ALL_SRCS})
source_group(TREE ${LIBCO_DIR} PREFIX "Header Files" FILES ${LIBCO_ALL_HDRS})
target_include_directories(libco PUBLIC ${LIBCO_DIR})
set_property(TARGET libco PROPERTY C_STANDARD 11)
set_property(TARGET libco PROPERTY C_EXTENSIONS OFF)
if(MSVC)
	target_compile_definitions(libco PRIVATE alignas=_Alignas)
endif(MSVC)
