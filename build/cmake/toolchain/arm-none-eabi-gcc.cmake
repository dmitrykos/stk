#
#  SuperTinyKernel: Minimalistic thread scheduling kernel for Embedded systems.
#
#  Source: http://github.com/dmitrykos/stk
#
#  Copyright (c) 2022 Dmitry Kostjucenko <dmitry.kostjucenko@gmail.com>
#  License: MIT License, see LICENSE for a full text.
#

# Identity
set(CMAKE_CROSSCOMPILING   TRUE)
set(CMAKE_SYSTEM_NAME      Linux)
set(CMAKE_SYSTEM_VERSION   0)
set(CMAKE_SYSTEM_PROCESSOR arm)

# Options
option(ENABLE_LTO       "enable LTO"               OFF)
option(ENABLE_HARD_FP   "enable Hard FP"           OFF)
option(ENABLE_SMALL     "enable Small build (Os)"  OFF)
option(TARGET_CORTEX_M0 "target Arm Cortex-M0 CPU" OFF)
option(TARGET_CORTEX_M3 "target Arm Cortex-M3 CPU" OFF)

# CPU platform
set(ARM TRUE)

# Host platform
if (WIN32)
	set(HOST_COMPILER_EXT ".exe")
else()
	set(HOST_COMPILER_EXT)
endif()

# Toolchain compiler ABI
set(TOOLCHAIN_COMPILER_ABI ${CMAKE_SYSTEM_PROCESSOR}-none-eabi)

# Find toolchain path
if (NOT TOOLCHAIN_PATH)
    if (MINGW OR CYGWIN OR WIN32)
        set(SEARCH_CMD where)
    elseif (UNIX OR APPLE)
        set(SEARCH_CMD which)
    endif()
    execute_process(
        COMMAND ${SEARCH_CMD} ${TOOLCHAIN_COMPILER_ABI}-gcc${HOST_COMPILER_EXT}
        OUTPUT_VARIABLE GCC_PATH
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    message(STATUS "Found compiler at path: ${GCC_PATH}")
    cmake_path(GET GCC_PATH PARENT_PATH TOOLCHAIN_PATH_BIN_RAW)
    #get_filename_component(TOOLCHAIN_PATH_BIN_RAW ${GCC_PATH} DIRECTORY)
    string(REPLACE "\\" "/" TOOLCHAIN_PATH_BIN ${TOOLCHAIN_PATH_BIN_RAW})
    message(STATUS "Using compiler path: ${TOOLCHAIN_PATH_BIN}")
    set(TOOLCHAIN_PATH ${TOOLCHAIN_PATH_BIN}/..)
endif()

# Set toolchain dirs
set(TOOLCHAIN_TOOLCHAIN_BASE           ${TOOLCHAIN_PATH}/${TOOLCHAIN_COMPILER_ABI})
set(TOOLCHAIN_TOOLCHAIN_LIBS           ${TOOLCHAIN_TOOLCHAIN_BASE}/lib)
set(TOOLCHAIN_TOOLCHAIN_INCLUDES       ${TOOLCHAIN_TOOLCHAIN_BASE}/include)

# Set paths
set(CMAKE_C_COMPILER                   ${TOOLCHAIN_PATH_BIN}/${TOOLCHAIN_COMPILER_ABI}-gcc${HOST_COMPILER_EXT})
set(CMAKE_CXX_COMPILER                 ${TOOLCHAIN_PATH_BIN}/${TOOLCHAIN_COMPILER_ABI}-g++${HOST_COMPILER_EXT})
set(CMAKE_ASM_COMPILER                 ${TOOLCHAIN_PATH_BIN}/${TOOLCHAIN_COMPILER_ABI}-as${HOST_COMPILER_EXT})
set(CMAKE_SIZE                         ${TOOLCHAIN_PATH_BIN}/${TOOLCHAIN_COMPILER_ABI}-size${HOST_COMPILER_EXT})
set(CMAKE_ROOT_PATH                    ${TOOLCHAIN_TOOLCHAIN_BASE})
set(CMAKE_COMPILER_IS_GNUCC            TRUE)
set(CMAKE_COMPILER_IS_GNUCXX           TRUE)

# Set root paths
set(CMAKE_SYSROOT                      ${CMAKE_ROOT_PATH})
set(CMAKE_FIND_ROOT_PATH               ${CMAKE_ROOT_PATH})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM  NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY  ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE  ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE  ONLY)

# Shared libs are not supported
set_property(GLOBAL PROPERTY TARGET_SUPPORTS_SHARED_LIBS FALSE)

# Static library for try-compile
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Preserve debugging ability for Release builds
set(ENABLE_DEBUG_ABILITY FALSE)
if (${CMAKE_BUILD_TYPE} MATCHES "Debug")
	set(IS_DEBUG_BUILD       TRUE)
	set(ENABLE_DEBUG_ABILITY TRUE)
endif()
if (${CMAKE_BUILD_TYPE} MATCHES "RelWithDebInfo" OR IS_DEBUG_BUILD)
	set(ENABLE_DEBUG_ABILITY TRUE)
endif()

# Release optimization level
set(OPT_LEVEL_RELEASE 3)
if (ENABLE_SMALL)
	set(OPT_LEVEL_RELEASE s)
endif()

# Common all
if (ENABLE_DEBUG_ABILITY)
    set(TOOLCHAIN_COMMON_FLAGS "-O0 -g3 -DDEBUG")
else()
    set(TOOLCHAIN_COMMON_FLAGS "-O${OPT_LEVEL_RELEASE} -g -DNDEBUG")
endif()
set(TOOLCHAIN_COMMON_FLAGS "${TOOLCHAIN_COMMON_FLAGS} -pipe -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections -fno-common -fstrict-aliasing -fmerge-constants -Wall -Wextra -Wno-missing-field-initializers")

# Common C and C++ (C++ excludes RTTI always)
set(TOOLCHAIN_COMMON_C_FLAGS "-std=c11")
set(TOOLCHAIN_COMMON_CXX_FLAGS "-std=c++11 -fabi-version=0 -fno-use-cxa-atexit -fno-threadsafe-statics")

# Small build excludes exceptions
if (ENABLE_SMALL)
	set(TOOLCHAIN_COMMON_CXX_FLAGS "${TOOLCHAIN_COMMON_CXX_FLAGS} -ffreestanding -fno-exceptions -fno-rtti")
endif()

# Common other
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${TOOLCHAIN_COMMON_FLAGS} ${TOOLCHAIN_COMMON_C_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${TOOLCHAIN_COMMON_FLAGS} ${TOOLCHAIN_COMMON_CXX_FLAGS}")

# Optimizing flags
set(TOOLCHAIN_LTO_FLAGS)
if (NOT ENABLE_DEBUG_ABILITY)
    set(TOOLCHAIN_OPT_FLAGS "${TOOLCHAIN_OPT_FLAGS}")
	if (ENABLE_LTO)
        # Note: fat-lto-objects required for ARM GCC toolchain which does not have LTO linker plugin (which understands slim LTO objects) unlike xPack ARM GCC therefore use it for compatibility with all cases
        set(TOOLCHAIN_OPT_FLAGS "${TOOLCHAIN_OPT_FLAGS} -flto -ffat-lto-objects")
	endif()
endif()

# CPU
if (TARGET_CORTEX_M0)
    set(TOOLCHAIN_CPU "-mcpu=cortex-m0")
elseif (TARGET_CORTEX_M3)
    set(TOOLCHAIN_CPU "-mcpu=cortex-m3")
else()
    set(TOOLCHAIN_CPU "-mcpu=cortex-m4")
    set(TARGET_CORTEX_M4 TRUE)
endif()
set(TOOLCHAIN_CPU "${TOOLCHAIN_CPU} -mthumb")
set(TOOLCHAIN_CPU_LINKER_FLAGS "${TOOLCHAIN_CPU_LINKER_FLAGS} ${TOOLCHAIN_CPU}")

# FPU
if (ENABLE_HARD_FP)
    set(TOOLCHAIN_FPU "-mfloat-abi=hard -mfpu=fpv4-sp-d16")
else()
    set(TOOLCHAIN_FPU "-mfloat-abi=soft")
endif()
set(TOOLCHAIN_FPU_LINKER_FLAGS "${TOOLCHAIN_FPU_LINKER_FLAGS} ${TOOLCHAIN_FPU}")

# Compiler flags
set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} ${TOOLCHAIN_CPU} ${TOOLCHAIN_FPU} ${TOOLCHAIN_OPT_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${TOOLCHAIN_CPU} ${TOOLCHAIN_FPU} ${TOOLCHAIN_OPT_FLAGS}")

# Linker flags
set(TOOLCHAIN_LINKER_FLAGS "-Wl,--gc-sections -nostartfiles -Wl,--print-memory-usage")

# Use NewLib-nano for small builds
if (ENABLE_SMALL)
    set(TOOLCHAIN_LINKER_FLAGS "${TOOLCHAIN_LINKER_FLAGS} --specs=nano.specs")
endif()

# Append CPU & FPU flags to linker
set(TOOLCHAIN_LINKER_FLAGS "${TOOLCHAIN_LINKER_FLAGS} ${TOOLCHAIN_CPU_LINKER_FLAGS} ${TOOLCHAIN_FPU_LINKER_FLAGS}")

# Linker flags
set(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS   "${TOOLCHAIN_LINKER_FLAGS}")
set(CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS "${TOOLCHAIN_LINKER_FLAGS}")
set(CMAKE_SHARED_LINKER_FLAGS           "${CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS}")
set(CMAKE_C_LINK_FLAGS                  "${CMAKE_SHARED_LIBRARY_LINK_C_FLAGS}")
set(CMAKE_CXX_LINK_FLAGS                "${CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS}")

# Log
if (NOT ONCE)
    message(STATUS "Host processor: ${CMAKE_HOST_SYSTEM_PROCESSOR}")
    message(STATUS "Target processor: ${CMAKE_SYSTEM_PROCESSOR}")
    message(STATUS "CXX flags: ${CMAKE_CXX_FLAGS}")
    message(STATUS "C flags: ${CMAKE_C_FLAGS}")
    message(STATUS "Linker C flags: ${CMAKE_C_LINK_FLAGS}")
    message(STATUS "Linker CXX flags: ${CMAKE_CXX_LINK_FLAGS}")
    set(ONCE TRUE)
endif()
