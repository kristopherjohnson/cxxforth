cmake_minimum_required(VERSION 3.0)
project(cxxforth)

add_executable(cxxforth cxxforth.cpp)

add_custom_target(cxxforth.cpp.md ALL cxxforth cpp2md.fs
    DEPENDS cxxforth.cpp cpp2md.fs
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Generate cxxforth.cpp.md" VERBATIM
    SOURCES cxxforth.cpp
)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(CMAKE_CXX_FLAGS "-std=c++14 -Weverything -Wno-unused-macros -Wno-unused-const-variable -Wno-unused-function -Wno-c++98-compat -Wno-c++98-compat-pedantic -Wno-global-constructors -Wno-exit-time-destructors -Wno-padded -Wno-missing-noreturn -Wno-array-bounds-pointer-arithmetic")
else()
    # Settings for gcc
    set(CMAKE_CXX_FLAGS "-std=c++1y -Wall -Wno-unused-function -Wno-array-bounds")
endif()

set(CXXFORTH_DATASPACE_SIZE "(16 * 1024 * sizeof(Cell))" CACHE STRING "Size of Forth dataspace in bytes")
set(CXXFORTH_DSTACK_COUNT   "256"                        CACHE STRING "Maximum number of cells in Forth data stack")
set(CXXFORTH_RSTACK_COUNT   "256"                        CACHE STRING "Maximum number of cells in Forth return stack")

option(CXXFORTH_OPTIMIZED           "Enable compiler optimization"                 ON)
option(CXXFORTH_SKIP_RUNTIME_CHECKS "Skip Forth runtime safety checks"             OFF)
option(CXXFORTH_DISABLE_MAIN        "Do not include a main() in cxxforth.cpp"      OFF)
option(CXXFORTH_32BIT               "Force 32-bit build on 64-bit platform"        OFF)
option(CXXFORTH_DISABLE_READLINE    "Do not use GNU Readline library if available" OFF)
option(CXXFORTH_DISABLE_FILE_ACCESS "Disable the File Access words"                OFF)

if (CXXFORTH_OPTIMIZED)
    set(CMAKE_CXX_FLAGS "-O3 ${CMAKE_CXX_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS "-O3 ${CMAKE_CXX_FLAGS}")
endif()

if (CXXFORTH_32BIT)
    set(CMAKE_CXX_FLAGS "-m32 ${CMAKE_CXX_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS "-m32 ${CMAKE_EXE_LINKER_FLAGS}")
endif()

include_directories(${CMAKE_CURRENT_BINARY_DIR})

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules ${CMAKE_MODULE_PATH})

if (NOT CXXFORTH_DISABLE_READLINE)
    find_package(Readline)
    if (READLINE_FOUND)
        set(CXXFORTH_USE_READLINE ON)
        include_directories(${READLINE_INCLUDE_DIR})
        target_link_libraries(cxxforth ${READLINE_LIBRARY})
    endif()
endif()

configure_file(cxxforthconfig.h.in cxxforthconfig.h)

