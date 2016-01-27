cmake_minimum_required (VERSION 3.4)
project (cxxforth)
add_executable(cxxforth cxxforth.c forth.cpp)

set(CMAKE_C_FLAGS "-O3 -Weverything")
set(CMAKE_CXX_FLAGS "-O3 -std=c++14 -Weverything -Wno-c++98-compat -Wno-unused-parameter -Wno-global-constructors -Wno-exit-time-destructors -Wno-unused-function -Wno-unused-const-variable -Wno-unused-macros")

option(CONFIG_SKIP_RUNTIME_CHECKS "Skip runtime checks" OFF)
configure_file(forthconfig.h.in forthconfig.h)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

