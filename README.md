[![Build Status](https://travis-ci.org/kristopherjohnson/cxxforth.svg?branch=master)](https://travis-ci.org/kristopherjohnson/cxxforth)

cxxforth: A Simple Forth Implementation in C++
==============================================

by Kristopher Johnson

cxxforth is a simple implementation of a [Forth][forth] system in C++.

The source code is written as a tutorial for implementing Forth, similar in
spirit to [JONESFORTH][jonesforth].  A [Markdown][markdown]-format rendition of
the source code is available in [cxxforth.cpp.md](cxxforth.cpp.md).

cxxforth implements a subset of the words in the [ANS Forth draft
standard][dpans].   Refer to that document for descriptions of those words.
Additional non-ANS words are described in the cxxforth source code.

This is free and unencumbered software released into the public domain.  See
[UNLICENSE.txt](UNLICENSE.txt) for details.

[forth]: https://en.wikipedia.org/wiki/Forth_(programming_language) "Forth (programming language)"

[jonesforth]: http://git.annexia.org/?p=jonesforth.git;a=blob;f=jonesforth.S;h=45e6e854a5d2a4c3f26af264dfce56379d401425;hb=HEAD

[markdown]: https://daringfireball.net/projects/markdown/ "Markdown"

[dpans]: http://forth.sourceforge.net/std/dpans/dpansf.htm "Alphabetic list of words"

----

Building cxxforth
-----------------

Building the `cxxforth` executable and other targets is easiest if you are on a
UNIX-ish system that has `make`, `cmake`, and Clang or GCC.  If you have those
components, you can probably build `cxxforth` by just entering these commands:

    cd wherever_your_files_are/cxxforth
    make

If successful, the `cxxforth` executable will be built in the
`MyFiles/cxxforth/build/` subdirectory.

If you don't have one of those components, or if 'make' doesn't work, then it's
not too hard to build it manually.  You will need to create a file called
`cxxforthconfig.h`, which can be empty, then you need to invoke your C++
compiler on the `cxxforth.cpp` source file, enabling whatever options might be
needed for C++14 compatibility and to link with the necessary C++ and system
libraries.  For example, on a Linux system with gcc, you should be able to
build it by entering these commands:

    cd wherever_your_files_are/cxxforth
    touch cxxforthconfig.h
    g++ -std=c++14 -o cxxforth cxxforth.cpp

