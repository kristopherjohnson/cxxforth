cmake_minimum_required (VERSION 3.4)
project (cxxforth)
add_executable(cxxforth cxxforth.cpp forth.cpp)

set(CMAKE_CXX_FLAGS "-std=c++14 -Wall -Weverything -Wno-c++98-compat -Wno-unused-parameter -Wno-global-constructors -Wno-exit-time-destructors -Wno-unused-function -Wno-unused-const-variable")
