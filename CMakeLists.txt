cmake_minimum_required(VERSION 3.0)
project(cxxforth)

add_executable(cxxforth cxxforth.cpp)
add_executable(cpp2md   cpp2md.cpp)

add_custom_target(cxxforth.cpp.md ALL build/cpp2md cxxforth.cpp cxxforth.cpp.md
    DEPENDS cxxforth.cpp cpp2md
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Generate cxxforth.cpp.md" VERBATIM
    SOURCES cxxforth.cpp
)

add_custom_target(cpp2md.cpp.md ALL build/cpp2md cpp2md.cpp cpp2md.cpp.md
    DEPENDS cpp2md
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Generate cpp2md.cpp.md" VERBATIM
    SOURCES cpp2md.cpp
)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(CMAKE_CXX_FLAGS "-std=c++14 -Weverything -Wno-c++98-compat -Wno-unused-parameter -Wno-global-constructors -Wno-exit-time-destructors -Wno-unused-function -Wno-unused-const-variable -Wno-unused-macros -Wno-unused-member-function -Wno-unused-variable -Wno-padded -Wno-missing-noreturn -Wno-array-bounds-pointer-arithmetic")
else()
    # Settings for gcc
    set(CMAKE_CXX_FLAGS "-std=c++14 -Wall -Wno-unused-variable -Wno-unused-function")
endif()

set(CXXFORTH_DATASPACE_SIZE    "16 * 1024 * sizeof(Cell)"  CACHE STRING "Size of Forth dataspace in bytes")
set(CXXFORTH_DSTACK_COUNT      "256"                       CACHE STRING "Maximum number of cells in Forth data stack")
set(CXXFORTH_RSTACK_COUNT      "256"                       CACHE STRING "Maximum number of cells in Forth return stack")
set(CXXFORTH_DEFINITIONS_COUNT "4096"                      CACHE STRING "Maximum number of words in Forth dictionary")

option(CXXFORTH_OPTIMIZED           "Enable compiler optimization"            OFF)
option(CXXFORTH_SKIP_RUNTIME_CHECKS "Skip Forth runtime safety checks"        OFF)
option(CXXFORTH_DISABLE_MAIN        "Do not include a main() in cxxforth.cpp" OFF)
option(CXXFORTH_32BIT               "Force 32-bit build on 64-bit platform"   OFF)

if (CXXFORTH_OPTIMIZED)
    set(CMAKE_CXX_FLAGS "-O3 ${CMAKE_CXX_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS "-O3 ${CMAKE_CXX_FLAGS}")
endif()

if (CXXFORTH_32BIT)
    set(CMAKE_CXX_FLAGS "-m32 ${CMAKE_CXX_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS "-m32 ${CMAKE_EXE_LINKER_FLAGS}")
endif()

configure_file(cxxforthconfig.h.in cxxforthconfig.h)

include_directories(${CMAKE_CURRENT_BINARY_DIR})


