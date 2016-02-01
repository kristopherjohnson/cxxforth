cmake_minimum_required (VERSION 3.4)
project (cxxforth)
add_executable(cxxforth cxxforth.cpp)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(CMAKE_CXX_FLAGS "-O3 -std=c++14 -Weverything -Wno-c++98-compat -Wno-unused-parameter -Wno-global-constructors -Wno-exit-time-destructors -Wno-unused-function -Wno-unused-const-variable -Wno-unused-macros -Wno-unused-member-function -Wno-unused-variable -Wno-padded -Wno-missing-noreturn")
endif()

set(CXXFORTH_DATA_SIZE 65536 CACHE STRING "Size of Forth dataspace in bytes")
option(CXXFORTH_SKIP_RUNTIME_CHECKS "Skip runtime checks" OFF)
option(CXXFORTH_DISABLE_MAIN "Do not include a main() in cxxforth.cpp" OFF)
configure_file(cxxforthconfig.h.in cxxforthconfig.h)

include_directories(${CMAKE_CURRENT_BINARY_DIR})


