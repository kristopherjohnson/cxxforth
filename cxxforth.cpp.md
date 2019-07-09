
cxxforth: A Simple Forth Implementation in C++
==============================================

by Kristopher Johnson

<https://github.com/kristopherjohnson/cxxforth>

----

This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or distribute this
software, either in source code form or as a compiled binary, for any purpose,
commercial or non-commercial, and by any means.

In jurisdictions that recognize copyright laws, the author or authors of this
software dedicate any and all copyright interest in the software to the public
domain. We make this dedication for the benefit of the public at large and to
the detriment of our heirs and successors. We intend this dedication to be an
overt act of relinquishment in perpetuity of all present and future rights to
this software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

----

`cxxforth` is a simple implementation of [Forth][forth] in C++.

There are many examples of Forth implementations available on the Internet, but
most of them are written in assembly language or low-level C, with a focus in
maximizing efficiency and demonstrating traditional Forth implementation
techniques.  This Forth is different: My goal is to use modern C++ to create a
Forth implementation that is easy to understand, easy to port, and easy to
extend.  I'm not going to talk about register assignments or addressing modes
or opcodes or the trade-offs between indirect threaded code, direct threaded
code, subroutine threaded code, and token threaded code.  I'm just going to
build a working Forth system in a couple thousand lines of C++ and Forth.

An inspiration for this implementation is Richard W.M. Jones's
[JONESFORTH][jonesforth].  JONESFORTH is a Forth implementation written as a
very readable tutorial, and I am adopting its style for our higher-level
implementation.  This Forth kernel is written as a [C++ file](cxxforth.cpp)
with large comment blocks, and there is a utility, [cpp2md](cpp2md.fs),
that takes that C++ file and converts it to a [Markdown][markdown]-format
document [cxxforth.cpp.md](cxxforth.cpp.md) with nicely formatted commentary
sections between the C++ code blocks.

As in other Forth systems, the basic design of this Forth is to create a small
kernel in native code (C++, in this case), and then implement the rest of the
system with Forth code.  The kernel has to provide the basic primitives needed
for memory access, arithmetic, logical operations, and operating system access.
With those primitives, we can then write Forth code to extend the system.

I am writing C++ conforming to the C++14 standard.  If your C++ compiler does
not support C++14 yet, you may need to make some modifications to the code to
get it built.

The Forth words provided by cxxforth are based on those in the [ANS Forth draft
standard][dpans] and [Forth 2012 standard][forth2012].  I don't claim
conformance to any standard, but you can use these standards as a crude form of
documentation for the Forth words that are implemented here.  cxxforth
implements many of the words from the standards' core word sets, and a
smattering of words from other standard word sets.

In addition to words from the standards, cxxforth provides a few non-standard
words.  Each of these is marked with "Not a standard word" in accompanying
comments.

While this Forth can be seen as a toy implementation, I do want it to be usable
for real-world applications.  Forth was originally designed to be something
simple you could build yourself and extend and customize as needed to solve
your problem.  I hope people can use cxxforth like that.

It is assumed that the reader has some familiarity with C++ and Forth.  You may
want to first read the JONESFORTH source or the source of some other Forth
implementation to get the basic gist of how Forth is usually implemented.

[forth]: https://en.wikipedia.org/wiki/Forth_(programming_language) "Forth (programming language)"

[jonesforth]: http://git.annexia.org/?p=jonesforth.git;a=blob;f=jonesforth.S;h=45e6e854a5d2a4c3f26af264dfce56379d401425;hb=HEAD

[markdown]: https://daringfireball.net/projects/markdown/ "Markdown"

[dpans]: http://forth.sourceforge.net/std/dpans/dpansf.htm "Alphabetic list of words"

[forth2012]: http://forth-standard.org/standard/alpha "Forth 2012"

----

Building cxxforth
-----------------

Building the `cxxforth` executable and other targets is easiest if you are on a
UNIX-ish system that has `make`, `cmake`, and Clang or GCC.  If you have those
components, you can probably build `cxxforth` by just entering these commands:

    cd wherever_your_files_are/cxxforth
    make

If successful, the `cxxforth` executable will be built in the
`wherever_your_files_are/cxxforth/build/` subdirectory.

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

----

Running cxxforth
----------------

Once the `cxxforth` executable is built, you can run it like any other command-line utility.

If you run it without any additional arguments, it will display a welcome
message and then allow you to enter Forth commands.  Enter "bye" to exit.

If there are any additional arguments, cxxforth will load and interpret those
files.  For example, the `cxxforth/tests` directory contains a file `hello.fs`
that defines a word `hello`.  So, if you are in the `cxxforth` directory and
you enter this:

    build/cxxforth tests/hello.fs

Then cxxforth will load that file, and you can enter `hello` to execute the
word that was loaded from `hello.fs`.

----

The Code
--------

I start by including headers from the C++ Standard Library.  I also include
`cxxforth.h`, which declares exported functions and includes the
`cxxforthconfig.h` file produced by the CMake build.

A macro `CXXFORTH_DISABLE_FILE_ACCESS` can be defined to prevent cxxforth from
defining words for opening, reading, and writing files.  You may want to do
this on a platform that does not support file access, or if you don't need
those words and want a smaller executable.

    
    #include "cxxforth.h"
    
    #include <algorithm>
    #include <cctype>
    #include <chrono>
    #include <cstdlib>
    #include <cstring>
    #include <ctime>
    #include <iomanip>
    #include <iostream>
    #include <list>
    #include <stdexcept>
    #include <string>
    #include <thread>
    
    #ifndef CXXFORTH_DISABLE_FILE_ACCESS
    #include <cstdio>
    #include <fstream>
    #endif
    
    using std::cerr;
    using std::cout;
    using std::endl;
    using std::exception;
    using std::isspace;
    using std::ptrdiff_t;
    using std::runtime_error;
    using std::size_t;
    using std::string;
    

GNU Readline Support
--------------------

cxxforth can use the [GNU Readline][readline] library for user input if it is
available.

The CMake build will automatically detect whether the library is available, and
if so define `CXXFORTH_USE_READLINE`.

However, even if the GNU Readline library is available, you may not want to
link your executable with it due to its GPL licensing terms.  You can pass
`-DCXXFORTH_DISABLE_READLINE=ON` to `cmake` to prevent it from searching for
the library.

[readline]: https://cnswww.cns.cwru.edu/php/chet/readline/rltop.html

    
    #ifdef CXXFORTH_USE_READLINE
    #include "readline/readline.h"
    #include "readline/history.h"
    #endif
    

Configuration Constants
-----------------------

I have a few macros to define the size of the Forth data space, the maximum
numbers of cells on the data and return stacks, and the maximum number of word
definitions in the dictionary.

These macros are usually defined in the `cxxforthconfig.h` header that is
generated by CMake and included by `cxxforth.h`, but I provide default values
in case they have not been defined.

    
    #ifndef CXXFORTH_VERSION
    #define CXXFORTH_VERSION "1.4.1"
    #endif
    
    #ifndef CXXFORTH_DATASPACE_SIZE
    #define CXXFORTH_DATASPACE_SIZE (16 * 1024 * sizeof(Cell))
    #endif
    
    #ifndef CXXFORTH_DSTACK_COUNT
    #define CXXFORTH_DSTACK_COUNT (256)
    #endif
    
    #ifndef CXXFORTH_RSTACK_COUNT
    #define CXXFORTH_RSTACK_COUNT (256)
    #endif
    

----

Data Types
----------

Next I define some types.

A `Cell` is the basic Forth type.  I define the `Cell` type using the C++
`uintptr_t` type to ensure it is large enough to hold an address.  This
generally means that it will be a 32-bit value on 32-bit platforms and a 64-bit
value on 64-bit platforms.  (If you want to build a 32-bit Forth on a 64-bit
platform with clang or gcc, you can pass the `-m32` flag to the compiler and
linker.)

I won't be providing any of the double-cell operations that traditional Forths
provide.  Double-cell operations were important in the days of 8-bit and 16-bit
Forths, but with cells of 32 bits or more, many applications have no need for
them.

I'm also not dealing with floating-point values.  Floating-point support would
be useful, but I'm trying to keep this simple.

Forth doesn't require type declarations; a cell can be used as an address, an
unsigned integer, a signed integer, or a variety of other uses.  However, in
C++ we will have to be explicit about types to perform the operations we want
to perform.  So I define a few additional types to represent the ways that a
`Cell` can be used, and a few macros to cast between types without littering
the code with a lot of `reinterpret_cast<...>(...)` expressions.

    
    namespace {
    
    using Cell  = uintptr_t;      // unsigned value
    using SCell = intptr_t;       // signed value
    
    using Char  = unsigned char;
    using SChar = signed char;
    
    using CAddr = Char*;          // Any address
    using AAddr = Cell*;          // Cell-aligned address
    
    #define CELL(x)    reinterpret_cast<Cell>(x)
    #define CADDR(x)   reinterpret_cast<Char*>(x)
    #define AADDR(x)   reinterpret_cast<AAddr>(x)
    #define CHARPTR(x) reinterpret_cast<char*>(x)
    #define SIZE_T(x)  static_cast<size_t>(x)
    
    constexpr auto CellSize = sizeof(Cell);
    

Boolean Constants
-----------------

Here I define constants for Forth _true_ and _false_ Boolean flag values.

Note that the Forth convention is that a true flag is a cell with all bits set,
unlike the C++ convention of using 1 or any other non-zero value to mean true,
so we need to be sure to use these constants for all Forth words that return a
Boolean flag.

    
    constexpr Cell False = 0;
    constexpr Cell True  = ~False;
    

----

The Definition Struct
---------------------

My first big departure from traditional Forth implementations is how I will
store the word definitions for the Forth dictionary.  Traditional Forths
intersperse the word names in the shared data space along with code and data,
using a linked list to navigate through them.  I am going to use a `std::list`
of `Definition` structs, outside of the data space.

Use of `std::list` has these benefits:

- The `Definition` structures won't use data space.  The C++ library will take care of allocating heap space as needed.
- The `Definition` structures won't move around in memory after being added to the list.  (In contrast, use of `std::vector` or other C++ collections might move elements as they are modified.)

One of the members of `Definition` is a C++ `std::string` to hold the name.  I
won't need to worry about managing the memory for that variable-length field.
The `name` field will be empty for a `:NONAME` definition.

A `Definition` also has a `code` field that points to the native code
associated with the word, a `does` field pointing to associated Forth
instructions, a `parameter` field that points to associated data-space
elements, and some bit flags to keep track of whether the word is `IMMEDIATE`
and/or `HIDDEN`.  We will explore the use of these fields later when I talk
about the inner and outer interpreters.

`Definition` has a static field `executingWord` that contains the address
of the `Definition` that was most recently executed.  This can be used by
`Code` functions to refer to their definitions.

Finally, `Definition` has a few member functions for executing the code and for
accessing the _hidden_ and _immediate_ flags.

    
    using Code = void(*)();
    
    struct Definition {
        Code   code      = nullptr;
        AAddr  does      = nullptr;
        AAddr  parameter = nullptr;
        Cell   flags     = 0;
        string name;
    
        static constexpr Cell FlagHidden    = (1 << 1);
        static constexpr Cell FlagImmediate = (1 << 2);
    
        static const Definition* executingWord;
    
        void execute() const {
            auto saved = executingWord;
            executingWord = this;
            code();
            executingWord = saved;
        }
    
        bool isHidden() const    { return (flags & FlagHidden) != 0; }
    
        void toggleHidden()      { flags ^= FlagHidden; }
    
        bool isImmediate() const { return (flags & FlagImmediate) != 0; }
    
        void toggleImmediate()   { flags ^= FlagImmediate; }
    
        bool isFindable() const  { return !name.empty() && !isHidden(); }
    };
    

I will use a pointer to a `Definition` as the Forth _execution token_ (XT).

    
    using Xt = Definition*;
    
    #define XT(x) reinterpret_cast<Xt>(x)
    

----

Global Variables
----------------

With the types defined, next I define global variables, starting with the Forth
data space and the data and return stacks.

For each of these arrays, there are constants that point to the end of the
array, so I can easily test whether I need to report an overflow.

    
    Char dataSpace[CXXFORTH_DATASPACE_SIZE];
    Cell dStack[CXXFORTH_DSTACK_COUNT];
    Cell rStack[CXXFORTH_RSTACK_COUNT];
    
    constexpr CAddr dataSpaceLimit = &dataSpace[CXXFORTH_DATASPACE_SIZE];
    constexpr AAddr dStackLimit    = &dStack[CXXFORTH_DSTACK_COUNT];
    constexpr AAddr rStackLimit    = &rStack[CXXFORTH_RSTACK_COUNT];
    

The Forth dictionary is a list of `Definition`s.  The most recent definition is
at the back of the list.

    
    std::list<Definition> definitions;
    

For each of the global arrays, I need a pointer to the current location.

For the data space, I have the `dataPointer`, which corresponds to Forth's
`HERE`.

For each of the stacks, I need a pointer to the element at the top of the
stack.  The stacks grow upward.  When a stack is empty, the associated pointer
points to an address below the actual bottom of the array, so I will need to
avoid dereferencing these pointers under those circumstances.

    
    CAddr dataPointer = nullptr;
    AAddr dTop        = nullptr;
    AAddr rTop        = nullptr;
    

The inner-definition interpreter needs a pointer to the next instruction to be
executed.  This will be explained below in the **Inner Interpreter** section.

    
    Xt* nextInstruction = nullptr;
    

I have to define the static `executingWord` member declared in `Definition`.

    
    const Definition* Definition::executingWord = nullptr;
    

There are a few special words whose XTs I will use frequently when compiling
or executing.  Rather than looking them up in the dictionary as needed, I'll
cache their values during initialization.

    
    Xt doLiteralXt       = nullptr;
    Xt setDoesXt         = nullptr;
    Xt exitXt            = nullptr;
    Xt endOfDefinitionXt = nullptr;
    

I need a flag to track whether we are in interpreting or compiling state.
This corresponds to Forth's `STATE` variable.

    
    Cell isCompiling = False;
    

I provide a variable that controls the numeric base used for conversion
between numbers and text.  This corresponds to the Forth `BASE` variable.

Whenever using C++ stream output operators, I will need to ensure the stream's
numeric output base matches `numericBase`.  To make this easy, I'll define a
macro `SETBASE()` that calls the `std::setbase` I/O manipulator and use it
whenever writing numeric data using the stream operators.

    
    Cell numericBase = 10;
    
    #define SETBASE() std::setbase(static_cast<int>(numericBase))
    

The input buffer is a `std::string`.  This makes it easy to use the C++ I/O
facilities, and frees me from the need to allocate a statically sized buffer
that could overflow.  I also have a current offset into this buffer,
corresponding to the Forth `>IN` variable.

    
    string sourceBuffer;
    Cell sourceOffset = 0;
    

I need a buffer to store the result of the Forth `WORD` word.  As with the
input buffer, I use a `string` so I don't need to worry about memory
management.

Note that while this is a `std:string`, its format is not that of a typical C++
string.  The buffer returned by `WORD` has the word length as its first
character.  That is, it is a Forth _counted string_.

    
    string wordBuffer;
    

I need a buffer to store the result of the Forth `PARSE` word.  Unlike `WORD`,
this is a "normal" string and I won't need to store the count at the beginning
of this buffer.

    
    string parseBuffer;
    

I store the `argc` and `argv` values passed to `main()` so I can make them
available for use by the Forth program by our non-standard `#ARGS` and `ARG`
Forth words, defined later.

    
    size_t commandLineArgCount = 0;
    const char** commandLineArgVector = nullptr;
    

I need a variable to store the result of the last call to `SYSTEM`, which
the user can retrieve by using `$?`.

    
    int systemResult = 0;
    
    

----

Stack Primitives
----------------

I will be doing a lot of pushing and popping values to and from our data and
return stacks, so in lieu of sprinkling pointer arithmetic throughout our code,
I'll define a few simple functions to handle those operations.  I expect the
compiler to expand calls to these functions inline, so this isn't inefficient.

Above I defined the global variables `dTop` and `rTop` to point to the top of
the data stack and return stack.  I will use the expressions `*dTop` and
`*rTop` when accessing the top-of-stack values.  I will also use expressions
like `*(dTop - 1)` and `*(dTop - 2)` to reference the items beneath the top of
stack.

When I need to both read and remove a top-of-stack value, my convention will
be to put both operations on the same line, like this:

    Cell x = *dTop; pop();

A more idiomatic C++ way to write this might be `Cell x = *(dTop--);`, but I
think that's less clear.

    
    // Make the data stack empty.
    void resetDStack() {
        dTop = dStack - 1;
    }
    
    // Make the return stack empty.
    void resetRStack() {
        rTop = rStack - 1;
    }
    
    // Return the depth of the data stack.
    ptrdiff_t dStackDepth() {
        return dTop - dStack + 1;
    }
    
    // Return the depth of the return stack.
    ptrdiff_t rStackDepth() {
        return rTop - rStack + 1;
    }
    
    // Push cell onto data stack.
    void push(Cell x) {
        *(++dTop) = x;
    }
    
    // Pop cell from data stack.
    void pop() {
        --dTop;
    }
    
    // Push cell onto return stack.
    void rpush(Cell x) {
        *(++rTop) = x;
    }
    
    // Pop cell from return stack.
    void rpop() {
        --rTop;
    }
    

----

Exceptions
----------

Forth provides the `ABORT` and `ABORT"` words, which interrupt execution and
return control to the main `QUIT` loop.  I will implement this functionality
using a C++ exception to return control to the top-level interpreter.

The C++ functions `abort()` and `abortMessage()` defined here are the first
primitive functions that will be exposed as Forth words.  For each such word, I
will spell out the Forth name of the primitive in all-caps, and provide a Forth
comment showing the stack effects.  For words described in the standards, I
will generally not provide any more information, but for words that are not
standard words, I'll provide a brief description.

    
    class AbortException: public runtime_error {
    public:
        explicit AbortException(const string& msg): runtime_error(msg) {}
        explicit AbortException(const char* msg): runtime_error(msg) {}
        explicit AbortException(const char* caddr, size_t count)
            : runtime_error(string(caddr, count)) {}
    };
    
    // ABORT ( i*x -- ) ( R: j*x -- )
    void abort() {
        throw AbortException("");
    }
    
    // ABORT-MESSAGE ( i*x c-addr u -- ) ( R: j*x -- )
    //
    // Not a standard word.
    //
    // Same semantics as the standard ABORT", but takes a string address and length
    // instead of parsing the message string.
    void abortMessage() {
        auto count = SIZE_T(*dTop); pop();
        auto caddr = CHARPTR(*dTop); pop();
        throw AbortException(caddr, count);
    }
    

----

Runtime Safety Checks
---------------------

Old-school Forths are implemented by super-programmers who never make coding
mistakes and so don't want the overhead of bounds-checking or other nanny
hand-holding.  However, I'm just a dumb C++ programmer, and I'd like some help
to catch mistakes.

To that end, I have a set of macros and functions that verify that I have the
expected number of arguments available on the stacks, that I'm not going to run
off the end of an array, that I'm not going to try to divide by zero, and so
on.

You can define the macro `CXXFORTH_SKIP_RUNTIME_CHECKS` to generate an
executable that doesn't include these checks, so when you have a fully debugged
Forth application you can run it on that optimized executable for improved
performance.

When the `CXXFORTH_SKIP_RUNTIME_CHECKS` macro is not defined, these macros will
check conditions and throw an `AbortException` if the assertions fail.  I won't
go into the details of these macros here.  Later we will see them used in the
definitions of our primitive Forth words.

    
    #ifdef CXXFORTH_SKIP_RUNTIME_CHECKS
    
    #define RUNTIME_NO_OP()                      do { } while (0)
    #define RUNTIME_ERROR(msg)                   RUNTIME_NO_OP()
    #define RUNTIME_ERROR_IF(cond, msg)          RUNTIME_NO_OP()
    #define REQUIRE_DSTACK_DEPTH(n, name)        RUNTIME_NO_OP()
    #define REQUIRE_DSTACK_AVAILABLE(n, name)    RUNTIME_NO_OP()
    #define REQUIRE_RSTACK_DEPTH(n, name)        RUNTIME_NO_OP()
    #define REQUIRE_RSTACK_AVAILABLE(n, name)    RUNTIME_NO_OP()
    #define REQUIRE_ALIGNED(addr, name)          RUNTIME_NO_OP()
    #define REQUIRE_VALID_HERE(name)             RUNTIME_NO_OP()
    #define REQUIRE_DATASPACE_AVAILABLE(n, name) RUNTIME_NO_OP()
    
    #else
    
    #define RUNTIME_ERROR(msg)                   do { throw AbortException(msg); } while (0)
    #define RUNTIME_ERROR_IF(cond, msg)          do { if (cond) RUNTIME_ERROR(msg); } while (0)
    #define REQUIRE_DSTACK_DEPTH(n, name)        requireDStackDepth(n, name)
    #define REQUIRE_DSTACK_AVAILABLE(n, name)    requireDStackAvailable(n, name)
    #define REQUIRE_RSTACK_DEPTH(n, name)        requireRStackDepth(n, name)
    #define REQUIRE_RSTACK_AVAILABLE(n, name)    requireRStackAvailable(n, name)
    #define REQUIRE_ALIGNED(addr, name)          checkAligned(addr, name)
    #define REQUIRE_VALID_HERE(name)             checkValidHere(name)
    #define REQUIRE_DATASPACE_AVAILABLE(n, name) requireDataSpaceAvailable(n, name)
    
    template<typename T>
    void checkAligned(T addr, const char* name) {
        RUNTIME_ERROR_IF((CELL(addr) % CellSize) != 0,
                         string(name) + ": unaligned address");
    }
    
    void requireDStackDepth(size_t n, const char* name) {
        RUNTIME_ERROR_IF(dStackDepth() < static_cast<ptrdiff_t>(n),
                         string(name) + ": stack underflow");
    }
    
    void requireDStackAvailable(size_t n, const char* name) {
        RUNTIME_ERROR_IF((dTop + n) >= dStackLimit,
                         string(name) + ": stack overflow");
    }
    
    void requireRStackDepth(size_t n, const char* name) {
        RUNTIME_ERROR_IF(rStackDepth() < ptrdiff_t(n),
                         string(name) + ": return stack underflow");
    }
    
    void requireRStackAvailable(size_t n, const char* name) {
        RUNTIME_ERROR_IF((rTop + n) >= rStackLimit,
                         string(name) + ": return stack overflow");
    }
    
    void checkValidHere(const char* name) {
        RUNTIME_ERROR_IF(dataPointer < dataSpace || dataSpaceLimit <= dataPointer,
                         string(name) + ": HERE outside data space");
    }
    
    void requireDataSpaceAvailable(size_t n, const char* name) {
        RUNTIME_ERROR_IF((dataPointer + n) >= dataSpaceLimit,
                         string(name) + ": data space overflow");
    }
    
    #endif // CXXFORTH_SKIP_RUNTIME_CHECKS
    

----

Forth Primitives
----------------

Now I will start defining the primitive operations that are exposed as Forth
words.  You can think of these as the opcodes of a virtual Forth processor.
Once I have the primitive operations defined, I can then write definitions in
Forth that use these primitives to build more-complex words.

Each of these primitives is a function that takes no arguments and returns no
result, other than its effects on the Forth data stack, return stack, and data
space.  Such a function can be assigned to the `code` field of a `Definition`.

When changing the stack, the primitives don't change the stack depth any more
than necessary.  For example, `PICK` just replaces the top-of-stack value with
a different value, and `ROLL` uses `std::memmove()` to rearrange elements
rather than individually popping and pushng them.

You can peek ahead to the `definePrimitives()` function to see how these
primitives are added to the Forth dictionary.

Forth Stack Operations
----------------------

Let's start with some basic Forth stack manipulation words.  These differ from
the push/pop/rpush/rpop/etc. primitives above in that they are intended to be
called from Forth code rather than from the C++ kernel code.  So I include
runtime checks and use the stacks rather than passing arguments or returning
values via C++ call/return mechanisms.

Note that for C++ functions that implement primitive Forth words, I will
include the Forth names and stack effects in comments. You can look up the
Forth names in the standards to learn what these words are supposed to do.

    
    // DEPTH ( -- +n )
    void depth() {
        REQUIRE_DSTACK_AVAILABLE(1, "DEPTH");
        push(static_cast<Cell>(dStackDepth()));
    }
    
    // DROP ( x -- )
    void drop() {
        REQUIRE_DSTACK_DEPTH(1, "DROP");
        pop();
    }
    
    // DUP ( x -- x x )
    void dup() {
        REQUIRE_DSTACK_DEPTH(1, "DUP");
        REQUIRE_DSTACK_AVAILABLE(1, "DUP");
        push(*dTop);
    }
    
    // SWAP ( x0 x1 -- x1 x0 )
    void swap() {
        REQUIRE_DSTACK_DEPTH(2, "SWAP");
        std::swap(*dTop, *(dTop - 1));
    }
    
    // PICK ( xu ... x1 x0 u -- xu ... x1 x0 xu )
    void pick() {
        REQUIRE_DSTACK_DEPTH(1, "PICK");
        auto index = *dTop;
        REQUIRE_DSTACK_DEPTH(index + 2, "PICK");
        *dTop = *(dTop - index - 1);
    }
    
    // ROLL ( xu xu-1 ... x0 u -- xu-1 ... x0 xu )
    void roll() {
        REQUIRE_DSTACK_DEPTH(1, "ROLL");
        auto n = *dTop; pop();
        if (n > 0) {
            REQUIRE_DSTACK_DEPTH(n + 1, "ROLL");
            auto x = *(dTop - n);
            std::memmove(dTop - n, dTop - n + 1, n * sizeof(Cell));
            *dTop = x;
        }
    }
    
    // >R ( x -- ) ( R:  -- x )
    void toR() {
        REQUIRE_DSTACK_DEPTH(1, ">R");
        REQUIRE_RSTACK_AVAILABLE(1, ">R");
        rpush(*dTop); pop();
    }
    
    // R> ( -- x ) ( R: x -- )
    void rFrom() {
        REQUIRE_RSTACK_DEPTH(1, "R>");
        REQUIRE_DSTACK_AVAILABLE(1, "R>");
        push(*rTop); rpop();
    }
    
    // R@ ( -- x ) ( R: x -- x )
    void rFetch() {
        REQUIRE_RSTACK_DEPTH(1, "R@");
        REQUIRE_DSTACK_AVAILABLE(1, "R@");
        push(*rTop);
    }
    
    // EXIT ( -- ) ( R: nest-sys -- )
    void exit() {
        REQUIRE_RSTACK_DEPTH(1, "EXIT");
        nextInstruction = reinterpret_cast<Xt*>(*rTop); rpop();
    }
    

Another important set of Forth primitives are those for reading and writing
values in data space.

    
    // ! ( x a-addr -- )
    void store() {
        REQUIRE_DSTACK_DEPTH(2, "!");
        auto aaddr = AADDR(*dTop); pop();
        REQUIRE_ALIGNED(aaddr, "!");
        auto x = *dTop; pop();
        *aaddr = x;
    }
    
    // @ ( a-addr -- x )
    void fetch() {
        REQUIRE_DSTACK_DEPTH(1, "@");
        auto aaddr = AADDR(*dTop);
        REQUIRE_ALIGNED(aaddr, "@");
        *dTop = *aaddr;
    }
    
    // c! ( char c-addr -- )
    void cstore() {
        REQUIRE_DSTACK_DEPTH(2, "C!");
        auto caddr = CADDR(*dTop); pop();
        auto x = static_cast<Char>(*dTop); pop();
        *caddr = x;
    }
    
    // c@ ( c-addr -- char )
    void cfetch() {
        REQUIRE_DSTACK_DEPTH(1, "C@");
        auto caddr = CADDR(*dTop);
        *dTop = static_cast<Cell>(*caddr);
    }
    
    // COUNT ( c-addr1 -- c-addr2 u )
    void count() {
        REQUIRE_DSTACK_DEPTH(1, "COUNT");
        REQUIRE_DSTACK_AVAILABLE(1, "COUNT");
        auto caddr = CADDR(*dTop);
        auto count = static_cast<Cell>(*caddr);
        *dTop = CELL(caddr + 1);
        push(count);
    }
    

Next, I'll define some primitives for accessing and manipulating the data-space
pointer, `HERE`.

    
    template<typename T>
    AAddr alignAddress(T addr) {
        auto offset = CELL(addr) % CellSize;
        return (offset == 0) ? AADDR(addr) : AADDR(CADDR(addr) + (CellSize - offset));
    }
    
    void alignDataPointer() {
        dataPointer = CADDR(alignAddress(dataPointer));
    }
    
    // ALIGN ( -- )
    void align() {
        alignDataPointer();
        REQUIRE_VALID_HERE("ALIGN");
    }
    
    // ALIGNED ( addr -- a-addr )
    void aligned() {
        REQUIRE_DSTACK_DEPTH(1, "ALIGNED");
        *dTop = CELL(alignAddress(*dTop));
    }
    
    // HERE ( -- addr )
    void here() {
        REQUIRE_DSTACK_AVAILABLE(1, "HERE");
        push(CELL(dataPointer));
    }
    
    // ALLOT ( n -- )
    void allot() {
        REQUIRE_DSTACK_DEPTH(1, "ALLOT");
        REQUIRE_VALID_HERE("ALLOT");
        REQUIRE_DATASPACE_AVAILABLE(CellSize, "ALLOT");
        dataPointer += *dTop; pop();
    }
    
    // CELLS ( n1 -- n2 )
    void cells() {
        REQUIRE_DSTACK_DEPTH(1, "CELLS");
        *dTop *= CellSize;
    }
    
    // Store a cell to data space.
    void data(Cell x) {
        REQUIRE_VALID_HERE(",");
        REQUIRE_DATASPACE_AVAILABLE(CellSize, ",");
        REQUIRE_ALIGNED(dataPointer, ",");
        *(AADDR(dataPointer)) = x;
        dataPointer += CellSize;
    }
    
    // UNUSED ( -- u )
    void unused() {
        REQUIRE_DSTACK_AVAILABLE(1, "UNUSED");
        push(static_cast<Cell>(dataSpaceLimit - dataPointer));
    }
    

I could implement memory-block words like `CMOVE`, `CMOVE>`, `FILL`, and
`COMPARE` in Forth, but speed is often important for these, so I will make them
native primitives.

    
    // CMOVE ( c-addr1 c-addr2 u -- )
    void cMove() {
        REQUIRE_DSTACK_DEPTH(3, "CMOVE");
        auto length = SIZE_T(*dTop); pop();
        auto dest = CHARPTR(*dTop); pop();
        auto src = CHARPTR(*dTop); pop();
        std::memcpy(dest, src, length);
    }
    
    // CMOVE> ( c-addr1 c-addr2 u -- )
    void cMoveUp() {
        REQUIRE_DSTACK_DEPTH(3, "CMOVE>");
        auto length = SIZE_T(*dTop); pop();
        auto dst = CHARPTR(*dTop); pop();
        auto src = CHARPTR(*dTop); pop();
        if (length > 0) {
            auto offset = length - 1;
            for (;;)
            {
                *(dst + offset) = *(src + offset);
                if (offset == 0)
                    break;
                offset--;
            }
        }
    }
    
    // FILL ( c-addr u char -- )
    void fill() {
        REQUIRE_DSTACK_DEPTH(3, "FILL");
        auto ch = static_cast<Char>(*dTop); pop();
        auto length = SIZE_T(*dTop); pop();
        auto caddr = CHARPTR(*dTop); pop();
        std::fill(caddr, caddr + length, ch);
    }
    
    // COMPARE ( c-addr1 u1 c-addr2 u2 -- n )
    void compare() {
        REQUIRE_DSTACK_DEPTH(4, "COMPARE");
        auto len2 = SIZE_T(*dTop); pop();
        auto caddr2 = CHARPTR(*dTop); pop();
        auto len1 = SIZE_T(*dTop); pop();
        auto caddr1 = CHARPTR(*dTop);
    
        auto minLen = std::min(len1, len2);
        auto cmpResult = std::memcmp(caddr1, caddr2, minLen);
    
        if (cmpResult < 0) {
            *dTop = static_cast<Cell>(-1);
        }
        else if (cmpResult > 0) {
            *dTop = static_cast<Cell>(1);
        }
        else if (len1 < len2) {
            *dTop = static_cast<Cell>(-1);
        }
        else if (len1 > len2) {
            *dTop = static_cast<Cell>(1);
        }
        else {
            *dTop = 0;
        }
    }
    

Next I will define I/O primitives.

I keep things simple and portable by using C++ iostream objects.

    
    // KEY ( -- char )
    void key() {
        REQUIRE_DSTACK_AVAILABLE(1, "KEY");
        auto ch = static_cast<Cell>(std::cin.get());
        push(ch);
    }
    
    // EMIT ( x -- )
    void emit() {
        REQUIRE_DSTACK_DEPTH(1, "EMIT");
        auto cell = *dTop; pop();
        cout.put(static_cast<char>(cell));
    }
    
    // TYPE ( c-addr u -- )
    void type() {
        REQUIRE_DSTACK_DEPTH(2, "TYPE");
        auto length = static_cast<std::streamsize>(*dTop); pop();
        auto caddr = CHARPTR(*dTop); pop();
        cout.write(caddr, length);
    }
    
    // CR ( -- )
    void cr() {
        cout << endl;
    }
    
    // . ( n -- )
    void dot() {
        REQUIRE_DSTACK_DEPTH(1, ".");
        cout << SETBASE() << static_cast<SCell>(*dTop);
        pop();
    }
    
    // U. ( x -- )
    void uDot() {
        REQUIRE_DSTACK_DEPTH(1, "U.");
        cout << SETBASE() << *dTop;
        pop();
    }
    
    // .R ( n1 n2 -- )
    void dotR() {
        REQUIRE_DSTACK_DEPTH(2, ".R");
        auto width = static_cast<int>(*dTop); pop();
        auto n = static_cast<SCell>(*dTop); pop();
        cout << SETBASE() << std::setw(width) << n;
    }
    
    // BASE ( -- a-addr )
    void base() {
        REQUIRE_DSTACK_AVAILABLE(1, "BASE");
        push(CELL(&numericBase));
    }
    
    // SOURCE ( -- c-addr u )
    void source() {
        REQUIRE_DSTACK_AVAILABLE(2, "SOURCE");
        push(CELL(sourceBuffer.data()));
        push(sourceBuffer.length());
    }
    
    // >IN ( -- a-addr )
    void toIn() {
        REQUIRE_DSTACK_AVAILABLE(1, ">IN");
        push(CELL(&sourceOffset));
    }
    

`REFILL` reads a line from the user input device.  If successful, it puts the
result into `sourceBuffer`, sets `sourceOffset` to 0, and pushes a `TRUE` flag
onto the stack.  If not successful, it pushes a `FALSE` flag.

This uses GNU Readline if configured to do so.  Otherwise it uses the C++
`std::getline()` function.

    
    // REFILL ( -- flag )
    void refill() {
        REQUIRE_DSTACK_AVAILABLE(1, "REFILL");
    
    #ifdef CXXFORTH_USE_READLINE
        char* line = readline("");
        if (line) {
            sourceBuffer = line;
            sourceOffset = 0;
            if (*line)
                add_history(line);
            std::free(line);
            push(True);
        }
        else {
            push(False);
        }
    #else
        if (std::getline(std::cin, sourceBuffer)) {
            sourceOffset = 0;
            push(True);
        }
        else {
            push(False);
        }
    #endif
    }
    

`ACCEPT` is similar to `REFILL`, but puts the result into a caller-supplied
buffer.

    
    // ACCEPT ( c-addr +n1 -- +n2 )
    void accept() {
        REQUIRE_DSTACK_AVAILABLE(2, "ACCEPT");
    
        auto bufferSize = SIZE_T(*dTop); pop();
        auto buffer = CHARPTR(*dTop);
    
    #ifdef CXXFORTH_USE_READLINE
        char* line = readline("");
        if (line) {
            auto lineSize = std::strlen(line);
            auto copySize = std::min(lineSize, bufferSize);
            std::memcpy(buffer, line, copySize);
            *dTop = static_cast<Cell>(copySize);
            if (*line)
                add_history(line);
            std::free(line);
        }
        else {
            *dTop = 0;
        }
    #else
        string line;
        if (std::getline(std::cin, line)) {
            auto copySize = std::min(line.length(), bufferSize);
            std::memcpy(buffer, line.data(), copySize);
            *dTop = static_cast<Cell>(copySize);
        }
        else {
            *dTop = 0;
        }
    #endif
    }
    

The text interpreter and other Forth words use `WORD` to parse a
blank-delimited sequence of characters from the input.  `WORD` skips any
delimiters at the current input position, then reads characters until it finds
the delimiter again.  It returns the address of a buffer with the length in the
first byte, followed by the characters that made up the word.

In a few places later in the C++ code, you will see the call sequence `bl();
word(); count();`.  This corresponds to the Forth phrase `BL WORD COUNT`, which
is how Forth code typically reads a space-delimited word from the input and
gets its address and length.

If the delimiter is BL (space), then in addition to treating a space as the
delimiter, we will treat any of the other standard C++ whitespace characters as
a delimiter.  This allows us to process input indented with tabs or split
across lines.  This behavior is allowed by the Forth standard.

The standards specify that the `WORD` buffer must contain a space character
after the character data, but I'm not going to worry about this obsolescent
requirement.

    
    bool isWordDelimiterMatch(char delim, char c) {
        if (delim == ' ')
            return isspace(c);
        else
            return delim == c;
    }
    
    // WORD ( char "<chars>ccc<char>" -- c-addr )
    void word() {
        REQUIRE_DSTACK_DEPTH(1, "WORD");
        auto delim = static_cast<char>(*dTop);
    
        wordBuffer.clear();
        wordBuffer.push_back(0);  // First char of buffer is length.
    
        auto inputSize = sourceBuffer.size();
    
        // Skip leading delimiters
        while (sourceOffset < inputSize && isWordDelimiterMatch(delim, sourceBuffer[sourceOffset]))
            ++sourceOffset;
    
        // Copy characters until we see a delimiter again.
        while (sourceOffset < inputSize && !isWordDelimiterMatch(delim, sourceBuffer[sourceOffset])) {
            wordBuffer.push_back(sourceBuffer[sourceOffset]);
            ++sourceOffset;
        }
    
        if (sourceOffset < inputSize) {
            ++sourceOffset;
        }
    
        // Update the count at the beginning of the buffer.
        wordBuffer[0] = static_cast<char>(wordBuffer.size() - 1);
    
        *dTop = CELL(wordBuffer.data());
    }
    

`PARSE` is similar to `WORD`, but does not skip leading delimiters and provides
an address-length result.

    
    // PARSE ( char "ccc<char>" -- c-addr u )
    void parse() {
        REQUIRE_DSTACK_DEPTH(1, "PARSE");
        REQUIRE_DSTACK_AVAILABLE(1, "PARSE");
    
        auto delim = static_cast<char>(*dTop);
    
        parseBuffer.clear();
    
        auto inputSize = sourceBuffer.size();
    
        // Copy characters until we see the delimiter.
        while (sourceOffset < inputSize && sourceBuffer[sourceOffset] != delim) {
            parseBuffer.push_back(sourceBuffer[sourceOffset]);
            ++sourceOffset;
        }
    
        if (sourceOffset == inputSize)
            throw AbortException(string("PARSE: Did not find expected delimiter \'" + string(1, delim) + "\'"));
    
        // Skip over the delimiter
        ++sourceOffset;
    
        *dTop = CELL(parseBuffer.data());
        push(static_cast<Cell>(parseBuffer.size()));
    }
    

`BL` puts a space character on the stack.  It is often seen in the phrase `BL
WORD` to parse a space-delimited word, and will be seen later in the Forth
definition of `SPACE`.

    
    // BL ( -- char )
    void bl() {
        REQUIRE_DSTACK_AVAILABLE(1, "BL");
        push(' ');
    }
    

Next I define arithmetic primitives.

Note that I need to use the `SCell` type for signed operations, and the `Cell`
type for unsigned operations.

    
    // + ( n1 n2 -- n3 )
    void plus() {
        REQUIRE_DSTACK_DEPTH(2, "+");
        auto n2 = static_cast<SCell>(*dTop); pop();
        auto n1 = static_cast<SCell>(*dTop);
        *dTop = static_cast<Cell>(n1 + n2);
    }
    
    // - ( n1 n2 -- n3 )
    void minus() {
        REQUIRE_DSTACK_DEPTH(2, "-");
        auto n2 = static_cast<SCell>(*dTop); pop();
        auto n1 = static_cast<SCell>(*dTop);
        *dTop = static_cast<Cell>(n1 - n2);
    }
    
    // * ( n1 n2 -- n3 )
    void star() {
        REQUIRE_DSTACK_DEPTH(2, "*");
        auto n2 = static_cast<SCell>(*dTop); pop();
        auto n1 = static_cast<SCell>(*dTop);
        *dTop = static_cast<Cell>(n1 * n2);
    }
    
    // / ( n1 n2 -- n3 )
    void slash() {
        REQUIRE_DSTACK_DEPTH(2, "/");
        auto n2 = static_cast<SCell>(*dTop); pop();
        auto n1 = static_cast<SCell>(*dTop);
        RUNTIME_ERROR_IF(n2 == 0, "/: zero divisor");
        *dTop = static_cast<Cell>(n1 / n2);
    }
    
    // /MOD ( n1 n2 -- n3 n4 )
    void slashMod() {
        REQUIRE_DSTACK_DEPTH(2, "/MOD");
        auto n2 = static_cast<SCell>(*dTop);
        auto n1 = static_cast<SCell>(*(dTop - 1));
        RUNTIME_ERROR_IF(n2 == 0, "/MOD: zero divisor");
        auto result = std::ldiv(n1, n2);
        *(dTop - 1) = static_cast<Cell>(result.rem);
        *dTop = static_cast<Cell>(result.quot);
    }
    

Next, I define logical and relational primitives.

    
    // AND ( x1 x2 -- x3 )
    void bitwiseAnd() {
        REQUIRE_DSTACK_DEPTH(2, "AND");
        auto x2 = *dTop; pop();
        *dTop = *dTop & x2;
    }
    
    // OR ( x1 x2 -- x3 )
    void bitwiseOr() {
        REQUIRE_DSTACK_DEPTH(2, "OR");
        auto x2 = *dTop; pop();
        *dTop = *dTop | x2;
    }
    
    // XOR ( x1 x2 -- x3 )
    void bitwiseXor() {
        REQUIRE_DSTACK_DEPTH(2, "XOR");
        auto x2 = *dTop; pop();
        *dTop = *dTop ^ x2;
    }
    
    // LSHIFT ( x1 u -- x2 )
    void lshift() {
        REQUIRE_DSTACK_DEPTH(2, "LSHIFT");
        auto n = *dTop; pop();
        *dTop <<= n;
    }
    
    // RSHIFT ( x1 u -- x2 )
    void rshift() {
        REQUIRE_DSTACK_DEPTH(2, "RSHIFT");
        auto n = *dTop; pop();
        *dTop >>= n;
    }
    
    // = ( x1 x2 -- flag )
    void equals() {
        REQUIRE_DSTACK_DEPTH(2, "=");
        auto n2 = *dTop; pop();
        *dTop = *dTop == n2 ? True : False;
    }
    
    // < ( n1 n2 -- flag )
    void lessThan() {
        REQUIRE_DSTACK_DEPTH(2, "<");
        auto n2 = static_cast<SCell>(*dTop); pop();
        *dTop = static_cast<SCell>(*dTop) < n2 ? True : False;
    }
    
    // U< ( u1 u2 -- flag )
    void uLessThan() {
        REQUIRE_DSTACK_DEPTH(2, "U<");
        auto u2 = static_cast<Cell>(*dTop); pop();
        *dTop = static_cast<Cell>(*dTop) < u2 ? True : False;
    }
    

Now I will define a few primitives that give access to operating-system and
environmental data.

    
    // #ARG ( -- n )
    //
    // Not a standard word.
    //
    // Provide count of command-line arguments.
    void argCount() {
        REQUIRE_DSTACK_AVAILABLE(1, "#ARG");
        push(commandLineArgCount);
    }
    
    // ARG ( n -- c-addr u )
    //
    // Not a standard word.
    //
    // Provide the Nth command-line argument.
    void argAtIndex() {
        REQUIRE_DSTACK_DEPTH(1, "ARG");
        REQUIRE_DSTACK_AVAILABLE(1, "ARG");
        auto index = static_cast<size_t>(*dTop);
        RUNTIME_ERROR_IF(index >= commandLineArgCount, "ARG: invalid index");
        auto value = commandLineArgVector[index];
        *dTop = CELL(value);
        push(std::strlen(value));
    }
    
    // BYE ( -- )
    void bye() {
        std::exit(EXIT_SUCCESS);
    }
    
    // SYSTEM ( c-addr u -- )
    //
    // Not a standard word.
    //
    // Executes a system command in a subshell.
    //
    // See the documentation for the C++ `std::system()` library call for more
    // details.
    void system() {
        REQUIRE_DSTACK_DEPTH(2, "SYSTEM");
        auto length = SIZE_T(*dTop); pop();
        auto caddr = CHARPTR(*dTop); pop();
        string command(caddr, length);
        systemResult = std::system(command.c_str());
    }
    
    // $? ( -- n )
    //
    // Not a standard word.
    //
    // Returns an implementation-defined status code from the last call to
    // `SYSTEM`.  See the documentation for the C++ `std::system()` library call
    // for details.
    void lastSystemResult() {
        REQUIRE_DSTACK_AVAILABLE(1, "$?");
        push(static_cast<Cell>(static_cast<SCell>(systemResult)));
    }
    
    // MS ( u -- )
    void ms() {
        REQUIRE_DSTACK_DEPTH(1, "MS");
        auto period = *dTop; pop();
        std::this_thread::sleep_for(std::chrono::milliseconds(period));
    }
    
    // TIME&DATE ( -- +n1 +n2 +n3 +n4 +n5 +n6 )
    void timeAndDate () {
        REQUIRE_DSTACK_AVAILABLE(6, "TIME&DATE");
        auto t = std::time(nullptr);
        auto tm = std::localtime(&t);
        push(static_cast<Cell>(tm->tm_sec));
        push(static_cast<Cell>(tm->tm_min));
        push(static_cast<Cell>(tm->tm_hour));
        push(static_cast<Cell>(tm->tm_mday));
        push(static_cast<Cell>(tm->tm_mon + 1));
        push(static_cast<Cell>(tm->tm_year + 1900));
    }
    
    // UTCTIME&DATE ( -- +n1 +n2 +n3 +n4 +n5 +n6 )
    //
    // Not a standard word.
    //
    // Like TIME&DATE, but returns UTC rather than local time.
    void utcTimeAndDate () {
        REQUIRE_DSTACK_AVAILABLE(6, "UTCTIME&DATE");
        auto t = std::time(nullptr);
        auto tm = std::gmtime(&t);
        push(static_cast<Cell>(tm->tm_sec));
        push(static_cast<Cell>(tm->tm_min));
        push(static_cast<Cell>(tm->tm_hour));
        push(static_cast<Cell>(tm->tm_mday));
        push(static_cast<Cell>(tm->tm_mon + 1));
        push(static_cast<Cell>(tm->tm_year + 1900));
    }
    
    // .S ( -- )
    void dotS() {
        auto depth = dStackDepth();
        cout << SETBASE() << "<" << depth << "> ";
        for (auto i = depth; i > 0; --i) {
            cout << static_cast<SCell>(*(dTop - i + 1)) << " ";
        }
    }
    
    // .RS ( -- )
    //
    // Not a standard word.
    //
    // Like .S, but prints the contents of the return stack instead of the data
    // stack.
    void dotRS() {
        auto depth = rStackDepth();
        cout << SETBASE() << "<<" << depth << ">> ";
        for (auto i = depth; i > 0; --i) {
            cout << static_cast<SCell>(*(rTop - i + 1)) << " ";
        }
    }
    

----

Inner Interpreter
-----------------

A Forth system is said to have two interpreters: an "outer interpreter" which
reads input and interprets it, and an "inner interpreter" which executes
compiled Forth definitions.  I will start with the inner interpreter.

There are basically two kinds of words in a Forth system:

- primitive code: native subroutines that are executed directly by the CPU
- colon definition: a sequence of Forth words compiled by `:` (colon), `:NONAME`, or `DOES>`.

Every defined word has a `code` field that points to native code.  In the case
of primitive words, the `code` field points to a routine that performs the
operation.  In the case of a colon definition, the `code` field points to the
`doColon()` function, which saves the current program state and then starts
executing the words that make up the colon definition.

Each colon definition ends with a call to `EXIT`, which sets up a return to the
colon definition that called the current word.  In many traditional Forths, the
`EXIT` instruction is implemented as a jump/branch to the next machine-code
instruction to be executed.  But that's not easy to do in a portable way in
C++, so my `doColon()` just keeps going until it sees an `EXIT` instruction,
then returns to the caller without actually executing it.

In many Forth implementations, the return stack is used to store the address of
the next instruction to be invoked upon returning from the routine.  But in
C++, I can just use a temporary variable to achieve the same thing.  So in this
Forth, the return stack is really just a secondary stack; it doesn't have
anything to do with "returning".

    
    void doColon() {
        auto savedNext = nextInstruction;
    
        auto defn = Definition::executingWord;
    
        nextInstruction = reinterpret_cast<Xt*>(defn->does);
        while (*nextInstruction != exitXt) {
            (*(nextInstruction++))->execute();
        }
    
        nextInstruction = savedNext;
    }
    

----

Compilation
-----------

Now that we know how the inner interpreter works, I can define the words that
compile definitions to be executed by that interpreter.

The kernel provides three words that can add a word to the dictionary:
`CREATE`, `:`, and `:NONAME`.  Each of them constructs a `Definition`, fills in
its `name`, `code`, `parameter`, and `does` field appropriately, and then adds
it to the `definitions` list.

`:` and `:NONAME` then put the interpreter into compilation mode.  While in
compilation mode, the interpreter will add the XT's of non-immediate words to
the current definition.  This continues until the `;` word ends the definition.

The word `DOES>` can be used after `CREATE` to define execution semantics for
that word.  As with `:`, this puts the interpreter into compilation state until
`:` is encountered.

    
    // Return reference to the latest Definition.
    // Undefined behavior if the definitions list is empty.
    Definition& lastDefinition() {
        return definitions.back();
    }
    
    // LATEST ( -- xt )
    //
    // Not a standard word.
    //
    // Puts the execution token of the most recently CREATEd word on the stack.
    void latest() {
        push(CELL(&lastDefinition()));
    }
    
    // STATE ( -- a-addr )
    void state() {
        REQUIRE_DSTACK_AVAILABLE(1, "STATE");
        push(CELL(&isCompiling));
    }
    
    void doCreate() {
        auto defn = Definition::executingWord;
        REQUIRE_DSTACK_AVAILABLE(1, defn->name.c_str());
        push(CELL(defn->parameter));
    }
    
    // CREATE ( "<spaces>name" -- )  Execution: ( -- a-addr )
    void create() {
        alignDataPointer();
    
        bl(); word(); count();
        auto length = SIZE_T(*dTop); pop();
        auto caddr = CHARPTR(*dTop); pop();
    
        RUNTIME_ERROR_IF(length < 1, "CREATE: could not parse name");
    
        Definition defn;
        defn.code = doCreate;
        defn.parameter = defn.does = AADDR(dataPointer);
        defn.name = string(caddr, length);
        definitions.emplace_back(std::move(defn));
    }
    
    // : ( C: "<spaces>name" -- colon-sys )
    void colon() {
        create();
        isCompiling = true;
    
        auto& latest = lastDefinition();
        latest.code = doColon;
        latest.toggleHidden();
    }
    
    // :NONAME ( C:  -- colon-sys )  ( S:  -- xt )
    void noname() {
        alignDataPointer();
    
        Definition defn;
        defn.code = doColon;
        defn.parameter = defn.does = AADDR(dataPointer);
        definitions.emplace_back(std::move(defn));
    
        isCompiling = true;
        latest();
    }
    
    void doDoes() {
        doCreate();
        doColon();
    }
    
    void setDoes() {
        auto& latest = lastDefinition();
        latest.code = doDoes;
        latest.does = AADDR(nextInstruction) + 1;
    }
    
    // DOES>
    void does() {
        data(CELL(setDoesXt));
        data(CELL(exitXt));
    }
    
    // (;) ( -- )
    //
    // Not a standard word.
    //
    // This word is compiled by ; after the EXIT.  It is never executed, but serves
    // as a marker for use in debugging.
    void endOfDefinition() {
        throw AbortException("(;) should never be executed");
    }
    
    // ; ( C: colon-sys -- )
    void semicolon() {
        data(CELL(exitXt));
        data(CELL(endOfDefinitionXt));
        isCompiling = false;
        lastDefinition().toggleHidden();
    }
    
    // IMMEDIATE ( -- )
    //
    // Unlike the standard specification, my IMMEDIATE toggles the immediacy bit
    // of the most recent definition, rather than always setting it true.
    void immediate() { lastDefinition().toggleImmediate(); }
    
    // HIDDEN ( -- )
    //
    // Not a standard word.
    //
    // Toggles the hidden bit of the most recent definition.
    void hidden() { lastDefinition().toggleHidden(); }
    

Next I'll define a few "special words".  They are special in that they are used
to implement features of the inner interpreter, and are not generally used by
Forth application code.  As a signifier of their special nature, the words'
names start and end with with parentheses.

    
    // (lit) ( -- x )
    //
    // Not a standard word.
    //
    // This instruction gets the value of the next cell, puts that on the data
    // stack, and then moves the instruction pointer to the next instruction.  It
    // is used by `LITERAL` and other Forth words that need to specify a cell value
    // to put on the stack during execution.
    void doLiteral() {
        REQUIRE_DSTACK_AVAILABLE(1, "(lit)");
        push(CELL(*nextInstruction));
        ++nextInstruction;
    }
    
    // (branch) ( -- )
    //
    // Not a standard word.
    //
    // Used by branching/looping constructs.  Unconditionally adds an offset to
    // `next`.  The offset is in the cell following the instruction.
    //
    // The offset is in character units, but must be a multiple of the cell size.
    void branch() {
        auto offset = reinterpret_cast<SCell>(*nextInstruction);
        nextInstruction += offset / static_cast<SCell>(CellSize);
    }
    
    // (zbranch) ( flag -- )
    //
    // Not a standard word.
    //
    // Used by branching/looping constructinos.  Adds an offset to `next` if the
    // top-of-stack value is zero.  The offset is in the cell following the
    // instruction.  If top-of-stack is not zero, then continue to the next
    // instruction.
    void zbranch() {
        REQUIRE_DSTACK_DEPTH(1, "(zbranch)");
        auto flag = *dTop; pop();
        if (flag == False)
            branch();
        else
            ++nextInstruction;
    }
    

Dictionary
----------

The next section contains words that create elements in the `definitions` list,
look up elements by name, or traverse the list to perform some operation.

    
    // Create a new definition with specified name and code.
    void definePrimitive(const char* name, Code code) {
        alignDataPointer();
    
        Definition defn;
        defn.code = code;
        defn.parameter = defn.does = AADDR(dataPointer);
        defn.name = name;
        definitions.emplace_back(std::move(defn));
    }
    
    // Determine whether two names are equivalent, using case-insensitive matching.
    bool doNamesMatch(CAddr name1, CAddr name2, Cell nameLength) {
        for (size_t i = 0; i < nameLength; ++i) {
            if (std::toupper(name1[i]) != std::toupper(name2[i])) {
                return false;
            }
        }
        return true;
    }
    
    // Find a definition by name.
    Xt findDefinition(CAddr nameToFind, Cell nameLength) {
        if (nameLength == 0)
            return nullptr;
    
        for (auto i = definitions.rbegin(); i != definitions.rend(); ++i) {
            auto& defn = *i;
            if (!defn.isFindable())
                continue;
            auto& name = defn.name;
            if (name.length() == nameLength) {
                auto nameCAddr = CADDR(const_cast<char*>(name.data()));
                if (doNamesMatch(nameToFind, nameCAddr, nameLength)) {
                    return &defn;
                }
            }
        }
        return nullptr;
    }
    
    // Find a definition by name.
    Xt findDefinition(const string& name) {
        return findDefinition(CADDR(const_cast<char*>(name.data())), static_cast<Cell>(name.length()));
    }
    
    // FIND ( c-addr -- c-addr 0  |  xt 1  |  xt -1 )
    void find() {
        REQUIRE_DSTACK_DEPTH(1, "FIND");
        REQUIRE_DSTACK_AVAILABLE(1, "FIND");
        auto caddr = CADDR(*dTop);
        auto length = static_cast<Cell>(*caddr);
        auto name = caddr + 1;
        auto word = findDefinition(name, length);
        if (word == nullptr) {
            push(0);
        }
        else {
            *dTop = CELL(word);
            push(word->isImmediate() ? 1 : Cell(-1));
        }
    }
    
    // EXECUTE ( i*x xt -- j*x )
    void execute() {
        REQUIRE_DSTACK_DEPTH(1, "EXECUTE");
        auto defn = XT(*dTop); pop();
        defn->execute();
    }
    
    // >BODY ( xt -- a-addr )
    void toBody() {
        REQUIRE_DSTACK_DEPTH(1, ">BODY");
        auto xt = XT(*dTop);
        *dTop = CELL(xt->parameter);
    }
    
    // XT>NAME ( xt -- c-addr u )
    //
    // Not a standard word.
    //
    // Gives the name associated with an XT.
    void xtToName() {
        REQUIRE_DSTACK_DEPTH(1, "XT>NAME");
        REQUIRE_DSTACK_AVAILABLE(1, "XT>NAME");
        auto xt = XT(*dTop);
        auto& name = xt->name;
        *dTop = CELL(name.data());
        push(static_cast<Cell>(name.length()));
    }
    
    // WORDS ( -- )
    void words() {
        std::for_each(definitions.rbegin(), definitions.rend(), [](auto& defn) {
            if (defn.isFindable()) cout << defn.name << " ";
        });
    }
    

SEE
---

Most Forth systems provide a word `SEE` that will print out the definition of a
word.

My implementation of this word walks through the contents of a definition and
tries to "decompile" each cell.  If the cell contains the XT of a defined word,
then it prints that word's name.  Otherwise, it just prints the cell value.

This generally gives a readable view of the word's definition, but it is not
exactly equal to the original source text.  For example, for this definition:

    : add-1-and-2 ( -- )   1 2 + . ;

`SEE add-1-and-2` gives this output:

    : add-1-and-2 (lit) 1 (lit) 2 + . EXIT ;

It gets even messier when decompiling words that contain branches and string
literals, but it works well as a debugging tool when trying to determine why a
word is not compiling as expected.

Note that the kernel doesn't know about `IF`...`THEN`, or
`BEGIN`...`WHILE`...`THEN`, or `CONSTANT`, or `VARIABLE`, or various other
constructs that would be needed to provide a more accurate representation of
the original definition.  A better definition of `SEE` would need to be written
in Forth, after those constructs have been defined.  But I haven't done that
yet.

    
    // Given a cell that might be an XT, search for it in the definitions list.
    //
    // Returns a pointer to the definition if found, or nullptr if not.
    Xt findXt(Cell x) {
        for (auto i = definitions.rbegin(); i != definitions.rend(); ++i) {
            auto& defn = *i;
            if (&defn == reinterpret_cast<Xt>(x))
                return &defn;
        }
        return nullptr;
    }
    
    /// Display the words that make up a colon or DOES> definition.
    void seeDoes(AAddr does) {
        while (XT(*does) != endOfDefinitionXt) {
            auto xt = findXt(*does);
            if (xt)
                cout << " " << xt->name;
            else
                cout << " " << SETBASE() << static_cast<SCell>(*does);
            ++does;
        }
        cout << " ;";
    }
    
    // SEE ( "<spaces>name" -- )
    void see() {
        bl(); word(); find();
    
        auto found = *dTop; pop();
        if (!found) throw AbortException("SEE: undefined word");
    
        auto defn = XT(*dTop); pop();
        if (defn->code == doColon) {
            cout << ": " << defn->name;
            seeDoes(defn->does);
        }
        else if (defn->code == doCreate || defn->code == doDoes) {
            cout << "CREATE " << defn->name << " ( " << CELL(defn->parameter) << " )";
            if (defn->code == doDoes) {
                cout << " DOES>";
                seeDoes(defn->does);
            }
        }
        else {
            cout << ": " << defn->name << " <primitive " << SETBASE() << CELL(defn->code) << "> ;";
        }
        if (defn->isImmediate()) cout << " immediate";
    }
    

Outer Interpreter
-----------------

The word `INTERPRET` below implements the outer interpreter.  It looks at the
`sourceBuffer`, and repeats the following until it has processed all the
characters in the buffer:

- Parse a space-delimited word.
- Look up that word in the dictionary.
- If the word is found:
   - If not in compilation mode, or if the word is an immediate word, then execute it.
   - Otherwise (in compilation mode), compile a call to the word.
- If the word is not found:
   - Try to parse it as a number.
   - If it is a number:
      - If in compilation mode, then compile it as a literal.
      - Otherwise, put the value on the stack.
   - If it is not a number, then signal an error.

See [section 3.4 of the ANS Forth draft standard][dpans_3_4] for a more complete description
of the Forth text interpreter.

[dpans_3_4]: http://forth.sourceforge.net/std/dpans/dpans3.htm#3.4 "3.4 The Forth text interpreter"

    
    // Determine whether specified character is a valid numeric digit for current BASE.
    bool isValidDigit(Char c) {
        if (numericBase > 10) {
            if (('A' <= c) && (c < ('A' + numericBase - 10)))
                return true;
            if (('a' <= c) && (c < ('a' + numericBase - 10)))
                return true;
        }
        return ('0' <= c) && (c < ('0' + numericBase));
    }
    
    // Return numeric value associated with a character.
    Cell digitValue(Char c) {
        if (c >= 'a')
            return c - 'a' + 10;
        else if (c >= 'A')
            return c - 'A' + 10;
        else
            return c - '0';
    }
    
    // >UNUM ( u0 c-addr1 u1 -- u c-addr2 u2 )
    //
    // Not a standard word.
    //
    // This word is similar to the standard >NUMBER, but provides a single-cell
    // result.
    void parseUnsignedNumber() {
        REQUIRE_DSTACK_DEPTH(3, ">UNUM");
    
        auto length = SIZE_T(*dTop);
        auto caddr = CADDR(*(dTop - 1));
        auto value = *(dTop - 2);
    
        auto i = size_t(0);
        while (i < length) {
            auto c = caddr[i];
            if (isValidDigit(c)) {
                auto n = digitValue(c);
                value = value * numericBase + n;
                ++i;
            }
            else {
                break;
            }
        }
    
        *(dTop - 2) = value;
        *(dTop - 1) = CELL(caddr + i);
        *dTop = length - i;
    }
    
    // >NUM ( n c-addr1 u1 -- n c-addr2 u2 )
    //
    // Not a standard word.
    //
    // Similar to >UNUM, but looks for a '-' character at the beginning, and
    // negates the result if found.
    void parseSignedNumber() {
        REQUIRE_DSTACK_DEPTH(3, ">NUM");
    
        auto length = SIZE_T(*dTop);
        auto caddr = CHARPTR(*(dTop - 1));
    
        if (length > 1 && *caddr == '-') {
            *dTop = static_cast<Cell>(length - 1);
            *(dTop - 1) = CELL(caddr + 1);
            parseUnsignedNumber();
            *(dTop - 2) = static_cast<Cell>(-static_cast<SCell>(*(dTop - 2)));
        }
        else {
            parseUnsignedNumber();
        }
    }
    
    // INTERPRET ( i*x -- j*x )
    //
    // Not a standard word.
    //
    // Reads words from the input buffer and executes/compiles them.
    void interpret() {
        auto inputSize = sourceBuffer.size();
        while (sourceOffset < inputSize) {
            bl(); word(); find();
    
            auto found = static_cast<int>(*dTop); pop();
    
            if (found) {
                auto xt = XT(*dTop); pop();
                if (isCompiling && !xt->isImmediate()) {
                    data(CELL(xt));
                }
                else {
                    xt->execute();
                }
            }
            else {
                // find() left the counted string on the stack.
                // Try to parse it as a number.
    
                count();
                auto length = SIZE_T(*dTop); pop();
                auto caddr = CHARPTR(*dTop); pop();
    
                if (length > 0) {
                    push(0);
                    push(CELL(caddr));
                    push(length);
                    parseSignedNumber();
    
                    auto remainingLength = SIZE_T(*dTop); pop();
                    pop();
    
                    // Note: The parsed number is now on the top of the stack.
    
                    if (remainingLength == 0) {
                        if (isCompiling) {
                            data(CELL(doLiteralXt));
                            data(*dTop); pop();
                        }
                    }
                    else {
                        throw AbortException(string("unrecognized word: ") + string(caddr, length));
                    }
                }
                else {
                    pop();
                    return;
                }
            }
        }
    }
    

`EVALUATE` can be used to invoke `INTERPRET` with a string as the source
buffer.

    
    // EVALUATE ( i*x c-addr u -- j*x )
    void evaluate() {
        REQUIRE_DSTACK_DEPTH(2, "EVALUATE");
    
        auto length = static_cast<size_t>(*dTop); pop();
        auto caddr = CHARPTR(*dTop); pop();
    
        auto savedInput = std::move(sourceBuffer);
        auto savedOffset = sourceOffset;
    
        sourceBuffer = string(caddr, length);
        sourceOffset = 0;
        interpret();
    
        sourceBuffer = std::move(savedInput);
        sourceOffset = savedOffset;
    }
    

`QUIT` is the top-level outer interpreter loop. It calls `REFILL` to read a
line, `INTERPRET` to parse and execute that line, then `PROMPT` and repeat
until there is no more input.

There is an exception handler for `AbortException` that prints an error
message, resets the stacks, and continues.

If end-of-input occurs, then it exits the loop and calls `CR` and `BYE`.

If `QUIT` is called from a word called by `QUIT`, control returns to the
top-level loop.

    
    // PROMPT ( -- )
    //
    // Not a standard word.
    //
    // Displays "ok" prompt if in interpretation mode.
    void prompt() {
        if (!isCompiling) {
            cout << "  ok";
            cr();
        }
    }
    
    // QUIT ( -- )
    void quit() {
        static bool alreadyRunning = false;
        if (alreadyRunning)
            abort();
        alreadyRunning = true;
    
        resetRStack();
        isCompiling = false;
    
        for (;;) {
            try {
                refill();
                auto refilled = *dTop; pop();
                if (!refilled) // end-of-input
                    break;
    
                interpret();
            }
            catch (const AbortException& abortEx) {
                string msg(abortEx.what());
                if (msg.length() > 0) {
                    cout << "<<< " << msg << " >>>" << endl;
                }
                resetDStack();
                resetRStack();
                isCompiling = false;
            }
    
            prompt();
        }
    
        cr();
        bye();
    }
    

File Access Words
-----------------

One of my goals is to make cxxforth useful for writing simple shell-like
scripts and utilities, and so being able to read and write files and execute
Forth scripts is a necessity.  I am providing a subset of the [File-Access and
File-Access extension wordsets][dpansFileAccess] from the standards.

[dpansFileAccess]: http://forth.sourceforge.net/std/dpans/dpans11.htm "File Access words"

As with the user input, I will use C++ iostreams to implement the file access
words.  This means that a Forth _fileid_ is going to be a pointer to a
`std::fstream` instance.

On some platforms, the C++ iostreams library may be unavailable or incomplete,
or you may not want the overhead of linking in these functions.  In that case,
define the macro `CXXFORTH_DISABLE_FILE_ACCESS` to disable compilation of these
words.

    
    #ifndef CXXFORTH_DISABLE_FILE_ACCESS
    
    #define FILEID(x) reinterpret_cast<std::fstream*>(x)
    
    // R/O ( -- fam )
    void readOnly() {
        REQUIRE_DSTACK_AVAILABLE(1, "R/O");
        push(static_cast<Cell>(std::ios_base::in));
    }
    
    // R/W ( -- fam )
    void readWrite() {
        REQUIRE_DSTACK_AVAILABLE(1, "R/W");
        push(static_cast<Cell>(std::ios_base::in | std::ios_base::out));
    }
    
    // W/O ( -- fam )
    void writeOnly() {
        REQUIRE_DSTACK_AVAILABLE(1, "W/O");
        push(static_cast<Cell>(std::ios_base::out));
    }
    
    // BIN ( fam1 -- fam2 )
    void bin() {
        REQUIRE_DSTACK_DEPTH(1, "BIN");
        *dTop = *dTop | static_cast<Cell>(std::ios_base::binary);
    }
    
    // CREATE-FILE ( c-addr u fam -- fileid ior )
    void createFile() {
        REQUIRE_DSTACK_DEPTH(3, "CREATE-FILE");
    
        auto caddr = CHARPTR(*(dTop - 2));
        auto length = SIZE_T(*(dTop - 1));
        auto fam = static_cast<std::ios_base::openmode>(*dTop); pop();
    
        string filename(caddr, length);
        auto f = new std::fstream(filename, fam | std::ios_base::trunc);
        if (f->is_open()) {
            *(dTop - 1) = CELL(f);
            *dTop = 0;
        }
        else {
            delete f;
            *(dTop - 1) = 0;
            *dTop = Cell(-1);
        }
    }
    
    // OPEN-FILE ( c-addr u fam -- fileid ior )
    void openFile() {
        REQUIRE_DSTACK_DEPTH(3, "OPEN-FILE");
    
        auto caddr = CHARPTR(*(dTop - 2));
        auto length = SIZE_T(*(dTop - 1));
        auto fam = static_cast<std::ios_base::openmode>(*dTop); pop();
    
        string filename(caddr, length);
        auto f = new std::fstream(filename, fam);
        if (f->is_open()) {
            *(dTop - 1) = CELL(f);
            *dTop = 0;
        }
        else {
            delete f;
            *(dTop - 1) = 0;
            *dTop = Cell(-1);
        }
    }
    
    // READ-FILE ( c-addr u1 fileid -- u2 ior )
    void readFile() {
        REQUIRE_DSTACK_DEPTH(3, "READ-FILE");
        auto f = FILEID(*dTop); pop();
        if (f == nullptr) throw AbortException("READ-FILE: not a valid file ID");
        auto length = SIZE_T(*dTop);
        auto caddr = CHARPTR(*(dTop - 1));
        f->read(caddr, static_cast<std::streamsize>(length));
        *dTop = f->bad() ? Cell(-1) : 0;
        *(dTop - 1) = static_cast<Cell>(f->gcount());
    }
    
    // READ-LINE ( c-addr u1 fileid -- u2 flag ior )
    void readLine() {
        REQUIRE_DSTACK_DEPTH(3, "READ-FILE");
        auto f = FILEID(*dTop);
        if (f == nullptr) throw AbortException("READ-FILE: not a valid file ID");
        if (f->eof()) {
            *dTop = 0;
            *(dTop - 1) = False;
            *(dTop - 2) = 0;
            return;
        }
        auto length = SIZE_T(*(dTop - 1));
        auto caddr = CHARPTR(*(dTop - 2));
        f->getline(caddr, static_cast<std::streamsize>(length) + 1);
        if (f->bad()) {
            *dTop = static_cast<Cell>(-1);
            *(dTop - 1) = 0;
            *(dTop - 2) = 0;
        }
        else if (f->eof() && std::strlen(caddr) == 0) {
            *dTop = 0;
            *(dTop - 1) = False;
            *(dTop - 2) = 0;
        }
        else {
            *dTop = 0;
            *(dTop - 1) = True;
            *(dTop - 2) = std::strlen(caddr);
        }
    }
    
    // READ-CHAR ( fileid -- char ior )
    //
    // Not a standard word.
    //
    // Reads a single character from the specified file.
    // On success, ior is 0 and char is the character read.
    // On failure, ior is non-zero and char is undefined.
    void readChar() {
        REQUIRE_DSTACK_DEPTH(1, "READ-CHAR");
        REQUIRE_DSTACK_AVAILABLE(1, "READ-CHAR");
        auto f = FILEID(*dTop);
        if (f == nullptr) throw AbortException("READ-CHAR: not a valid file ID");
        auto ch = static_cast<unsigned char>(f->get());
        *dTop = static_cast<Cell>(ch);
        if (f->bad()) push(static_cast<Cell>(-1)); else push(0);
    }
    
    // WRITE-FILE ( c-addr u fileid -- ior )
    void writeFile() {
        REQUIRE_DSTACK_DEPTH(3, "WRITE-FILE");
        auto f = FILEID(*dTop); pop();
        if (f == nullptr) throw AbortException("WRITE-FILE: not a valid file ID");
        auto length = SIZE_T(*dTop); pop();
        auto caddr = CHARPTR(*dTop);
        f->write(caddr, static_cast<std::streamsize>(length));
        *dTop = f->bad() ? Cell(-1) : 0;
    }
    
    // WRITE-LINE ( c-addr u fileid -- ior )
    void writeLine() {
        REQUIRE_DSTACK_DEPTH(3, "WRITE-LINE");
        auto f = FILEID(*dTop); pop();
        if (f == nullptr) throw AbortException("WRITE-FILE: not a valid file ID");
        auto length = SIZE_T(*dTop); pop();
        auto caddr = CHARPTR(*dTop);
        f->write(caddr, static_cast<std::streamsize>(length));
        (*f) << endl;
        *dTop = f->bad() ? Cell(-1) : 0;
    }
    
    // WRITE-CHAR ( char fileid -- ior )
    //
    // Not a standard word.
    //
    // Writes a single character to the specified file.
    void writeChar() {
        REQUIRE_DSTACK_DEPTH(2, "WRITE-CHAR");
        auto f = FILEID(*dTop); pop();
        if (f == nullptr) throw AbortException("WRITE-CHAR: not a valid file ID");
        auto ch = static_cast<char>(*dTop);
        f->put(ch);
        *dTop = f->bad() ? Cell(-1) : 0;
    }
    
    // FLUSH-FILE ( fileid -- ior )
    void flushFile() {
        REQUIRE_DSTACK_DEPTH(1, "FLUSH-FILE");
        auto f = FILEID(*dTop);
        if (f == nullptr) throw AbortException("FLUSH-FILE: not a valid file ID");
        f->flush();
        *dTop = f->bad() ? Cell(-1) : 0;
    }
    
    // CLOSE-FILE ( fileid -- ior )
    void closeFile() {
        REQUIRE_DSTACK_DEPTH(1, "CLOSE-FILE");
        auto f = FILEID(*dTop);
        if (f == nullptr) throw AbortException("CLOSE-FILE: not a valid file ID");
        f->close();
        delete f;
        *dTop = 0;
    }
    
    // DELETE-FILE ( c-addr u -- ior )
    void deleteFile() {
        REQUIRE_DSTACK_DEPTH(2, "DELETE-FILE");
    
        auto caddr = CHARPTR(*(dTop - 1));
        auto length = SIZE_T(*dTop); pop();
    
        string filename(caddr, length);
        auto result = std::remove(filename.c_str());
        *dTop = static_cast<Cell>(result);
    }
    
    // RENAME-FILE ( c-addr1 u1 c-addr2 u2 -- ior )
    void renameFile() {
        REQUIRE_DSTACK_DEPTH(4, "RENAME-FILE");
    
        auto lengthNew = SIZE_T(*dTop); pop();
        auto caddrNew = CHARPTR(*dTop); pop();
        auto lengthOld = SIZE_T(*dTop); pop();
        auto caddrOld = CHARPTR(*dTop);
    
        string oldName(caddrOld, lengthOld);
        string newName(caddrNew, lengthNew);
        auto result = std::rename(oldName.c_str(), newName.c_str());
    
        *dTop = static_cast<Cell>(result);
    }
    
    // INCLUDE-FILE ( i*x fileid -- j*x )
    void includeFile() {
        REQUIRE_DSTACK_DEPTH(1, "INCLUDE-FILE");
    
        auto f = FILEID(*dTop); pop();
        if (f == nullptr) throw AbortException("INCLUDE-FILE: invalid file ID");
    
        string line;
        while (std::getline(*f, line)) {
            push(CELL(line.data()));
            push(static_cast<Cell>(line.length()));
            evaluate();
        }
    }
    
    #endif // #ifndef CXXFORTH_DISABLE_FILE_ACCESS
    

Memory Allocation
-----------------

By default, cxxforth's data space is 16K cells.  This may be enough for
moderate needs, but to process large chunks of data it may be insufficent.  One
way around this is to define `CXXFORTH_DATASPACE_SIZE` to the size you need,
but a better solution might be to allocate and free memory as needed.

`ALLOCATE`, `RESIZE` and `FREE` are Forth wrappers for C++'s `std::malloc()`,
`std::realloc()`, and `std::free()`.

    
    // ALLOCATE ( u -- a-addr ior )
    void memAllocate() {
        REQUIRE_DSTACK_DEPTH(1, "ALLOCATE");
        REQUIRE_DSTACK_AVAILABLE(1, "ALLOCATE");
        auto size = SIZE_T(*dTop);
        auto p = std::malloc(size);
        if (p) {
            *dTop = CELL(p);
            push(0);
        }
        else {
            *dTop = 0;
            push(static_cast<Cell>(-1));
        }
    }
    
    // RESIZE ( a-addr1 u -- a-addr2 ior )
    void memResize() {
        REQUIRE_DSTACK_DEPTH(2, "RESIZE");
        auto size = SIZE_T(*dTop);
        auto addr = AADDR(*(dTop - 1));
        auto p = std::realloc(addr, size);
        if (p) {
            *dTop = 0;
            *(dTop - 1) = CELL(p);
        }
        else {
            *dTop = static_cast<Cell>(-1);
            *(dTop - 1) = 0;
        }
    }
    
    // FREE ( a-addr -- ior )
    void memFree() {
        REQUIRE_DSTACK_DEPTH(1, "FREE");
        auto addr = AADDR(*dTop);
        std::free(addr);
        *dTop = 0;
    }
    

Initialization
--------------

In `initializeDefinitions()`, I set up the initial contents of the dictionary.
This is the Forth kernel that Forth code can use to implement the rest of a
working system.

    
    void definePrimitives() {
        static struct {
            const char* name;
            Code code;
        } immediateCodeWords[] = {
            // name             code
            // ------------------------------
            {";",               semicolon},
            {"does>",           does},
            {"immediate",       immediate},
        };
        for (auto& w: immediateCodeWords) {
            definePrimitive(w.name, w.code);
            immediate();
        }
    
        static struct {
            const char* name;
            Code code;
        } codeWords[] = {
            // name           code
            // ------------------------------
            {"!",               store},
            {"#args",           argCount},
            {"$?",              lastSystemResult},
            {"(;)",             endOfDefinition},
            {"(branch)",        branch},
            {"(does)",          setDoes},
            {"(lit)",           doLiteral},
            {"(zbranch)",       zbranch},
            {"*",               star},
            {"+",               plus},
            {"-",               minus},
            {".",               dot},
            {".r",              dotR},
            {".rs",             dotRS},
            {".s",              dotS},
            {"/",               slash},
            {"/mod",            slashMod},
            {":",               colon},
            {":noname",         noname},
            {"<",               lessThan},
            {"=",               equals},
            {">body",           toBody},
            {">in",             toIn},
            {">num",            parseSignedNumber},
            {">r",              toR},
            {">unum",           parseUnsignedNumber},
            {"@",               fetch},
            {"abort",           abort},
            {"abort-message",   abortMessage},
            {"accept",          accept},
            {"align",           align},
            {"aligned",         aligned},
            {"allocate",        memAllocate},
            {"allot",           allot},
            {"and",             bitwiseAnd},
            {"arg",             argAtIndex},
            {"base",            base},
            {"bl",              bl},
            {"bye",             bye},
            {"c!",              cstore},
            {"c@",              cfetch},
            {"cells",           cells},
            {"cmove",           cMove},
            {"cmove>",          cMoveUp},
            {"compare",         compare},
            {"count",           count},
            {"cr",              cr},
            {"create",          create},
            {"depth",           depth},
            {"drop",            drop},
            {"dup",             dup},
            {"emit",            emit},
            {"evaluate",        evaluate},
            {"execute",         execute},
            {"exit",            exit},
            {"fill",            fill},
            {"find",            find},
            {"free",            memFree},
            {"here",            here},
            {"hidden",          hidden},
            {"interpret",       interpret},
            {"key",             key},
            {"latest",          latest},
            {"lshift",          lshift},
            {"ms",              ms},
            {"or",              bitwiseOr},
            {"parse",           parse},
            {"pick",            pick},
            {"prompt",          prompt},
            {"quit",            quit},
            {"r>",              rFrom},
            {"r@",              rFetch},
            {"refill",          refill},
            {"resize",          memResize},
            {"roll",            roll},
            {"rshift",          rshift},
            {"see",             see},
            {"source",          source},
            {"state",           state},
            {"swap",            swap},
            {"system",          system},
            {"time&date",       timeAndDate},
            {"type",            type},
            {"u.",              uDot},
            {"u<",              uLessThan},
            {"unused",          unused},
            {"utctime&date",    utcTimeAndDate},
            {"word",            word},
            {"words",           words},
            {"xor",             bitwiseXor},
            {"xt>name",         xtToName},
    #ifndef CXXFORTH_DISABLE_FILE_ACCESS
            {"bin",             bin},
            {"close-file",      closeFile},
            {"create-file",     createFile},
            {"delete-file",     deleteFile},
            {"flush-file",      flushFile},
            {"include-file",    includeFile},
            {"open-file",       openFile},
            {"r/o",             readOnly},
            {"r/w",             readWrite},
            {"read-char",       readChar},
            {"read-file",       readFile},
            {"read-line",       readLine},
            {"rename-file",     renameFile},
            {"w/o",             writeOnly},
            {"write-char",      writeChar},
            {"write-file",      writeFile},
            {"write-line",      writeLine},
    #endif
        };
        for (auto& w: codeWords) {
            definePrimitive(w.name, w.code);
        }
    
        doLiteralXt = findDefinition("(lit)");
        if (doLiteralXt == nullptr) throw runtime_error("Can't find (lit) in kernel dictionary");
    
        setDoesXt = findDefinition("(does)");
        if (setDoesXt == nullptr) throw runtime_error("Can't find (does) in kernel dictionary");
    
        exitXt = findDefinition("exit");
        if (exitXt == nullptr) throw runtime_error("Can't find EXIT in kernel dictionary");
    
        endOfDefinitionXt = findDefinition("(;)");
        if (endOfDefinitionXt == nullptr) throw runtime_error("Can't find (;) in kernel dictionary");
    }
    

The Forth Part
--------------

With our C++ kernel defined, now I can define the remainder of the system using
Forth.  To do this, I will create an array of Forth text lines to be evaluated
when cxxforth initializes itself.

In this section, I won't go into the details of every word defined.  In most
cases, referring to the standards will be enough to understand what the word is
supposed to do and the definition will be easy to understand.  But I will
provide commentary for a few complicated definitions.

Writing Forth definitions as C++ strings is a little awkward in that I have to
escape every `"` and backslash with a backslash.

    
    static const char* forthDefinitions[] = {
    

I'll start by defining the remaining basic stack operations.  `PICK` and
`ROLL` are the basis for many of them.

Note that while I'm not implementing any of the Forth double-cell arithmetic
operations, double-cell stack operations are still useful.

    
        ": over    1 pick ;",
        ": rot     2 roll ;",
        ": nip     swap drop ;",
        ": tuck    swap over ;",
        ": 2drop   drop drop ;",
        ": 2dup    over over ;",
        ": 2over   3 pick 3 pick ;",
        ": 2swap   3 roll 3 roll ;",
        ": 2>r     swap >r >r ;",
        ": 2r>     r> r> swap ;",
        ": 2r@     r> r> 2dup >r >r swap ;",
    

`FALSE` and `TRUE` are useful constants.

    
        ": false   0 ;",
        ": true    -1 ;",
    

`]` enters compilation mode.

`[` exits compilation mode.

    
        ": ]   true state ! ;",
        ": [   false state ! ; immediate",
    

Forth has a few words for incrementing/decrementing the top-of-stack value.

    
        ": 1+   1 + ;",
        ": 1-   1 - ;",
    
        ": cell+   1 cells + ;",
        ": char+   1+ ;",
        ": chars   ;",
    

`+! ( n|u a-addr -- )` adds a value to a cell in memory.

    
        ": +!   dup >r @ + r> ! ;",
    

`NEGATE` and `INVERT` can be implemented in terms of other primitives.

    
        ": negate   0 swap - ;",
        ": invert   true xor ;",
    

`, ( x -- )` places a cell value in dataspace.

`C, ( char -- )` places a character value in dataspace.

    
        ": ,    here  1 cells allot  ! ;",
        ": c,   here  1 chars allot  c! ;",
    

`ERASE` fills a region with zeros.

    
        ": erase  0 fill ;",
    

We have a few extended relational operators based upon the kernel's relational
operators.  In a lower-level Forth system, these might have a one-to-one
mapping to CPU opcodes, but in this system, they are just abbreviations.

    
        ": >     swap < ;",
        ": u>    swap u< ;",
        ": <>    = invert ;",
        ": 0<    0 < ;",
        ": 0>    0 > ;",
        ": 0=    0 = ;",
        ": 0<>   0= invert ;",
    

`2*` and `2/` multiply or divide a value by 2 by shifting the bits left or
right.

    
        ": 2*   1 lshift ;",
        ": 2/   1 rshift ;",
    

A Forth variable is just a named location in dataspace.  I will use `CREATE`
and reserve a cell.

    
        ": variable   create 0 , ;",
        ": ?          @ . ;",
    

A Forth constant is similar to a variable in that it is a value stored in
dataspace, but using the name automatically puts the value on the stack.  I can
implement this using `CREATE...DOES>`.

    
        ": constant    create ,    does>  @ ;",
        ": 2constant   create , ,  does>  dup cell+ @ swap @ ;",
    

`/CELL` is not a standard word, but it is useful to be able to get the size
of a cell without using `1 CELLS`.

    
        "1 cells   constant /cell",
    

`DECIMAL` and `HEX` switch the numeric base to 10 or 16, respectively.

    
        ": decimal   10 base ! ;",
        ": hex       16 base ! ;",
    

`'` gets the next word from the input stream and looks up its execution token.

    
        ": '   bl word find drop ;",
    

The word `LITERAL` takes a cell from the stack at compile time, and at runtime
will put that value onto the stack.  I implement this by compiling a call to
`(lit)` word followed by the value.

Because I will be using `(lit)` in other word definitions, I'll create a
constant `'(lit)` containing its XT.

    
        "' (lit)     constant '(lit)",
        ": literal   '(lit) , , ; immediate",
    

`[']` is like `'`, but is an immediate compiling word that causes the XT to beR
put on the stack at runtime.

    
        ": [']   ' '(lit) , , ; immediate",
    

`RECURSE` compiles a call to the word currently being defined.

    
        ": recurse     latest , ; immediate",
    

`CHAR` gets the next character and puts its ASCII value on the stack.

`[CHAR]` is like `CHAR`, but is an immediate compiling word.

    
        ": char     bl word char+ c@ ;",
        ": [char]   char '(lit) , , ; immediate",
    

Control Structures
------------------

See the [Control Structures][jonesforthControlStructures] section of
`jonesforth.f` for an explanation of how these words work.

[jonesforthControlStructures]: http://git.annexia.org/?p=jonesforth.git;a=blob;f=jonesforth.f;h=5c1309574ae1165195a43250c19c822ab8681671;hb=HEAD#l118

One word we have here that is not described in JONESFORTH is `AHEAD`, which is
essentially equivalent to `FALSE IF`.  It is the start of an unconditional
forward jump.  It is useful for words, like `SLITERAL` below, that need to
store data while compiling.  Such words can use `AHEAD`, then use words like
`,`, `C,`, or `ALLOT` to put data into the dictionary at the current
compilation point, then use `THEN` so that at runtime the inner interpreter
will jump over that data.

    
        ": if       ['] (zbranch) , here 0 , ; immediate",
        ": then     dup  here swap -  swap ! ; immediate",
        ": else     ['] (branch) , here 0 ,  swap dup here swap -  swap ! ; immediate",
        ": ahead    ['] (branch) , here 0 , ; immediate",
    
        ": begin    here ; immediate",
        ": again    ['] (branch) ,  here - , ; immediate",
        ": until    ['] (zbranch) ,  here - , ; immediate",
        ": while    ['] (zbranch) ,  here swap  0 , ; immediate",
        ": repeat   ['] (branch) ,  here - ,  dup  here swap -  swap ! ; immediate",
    

Here are some common Forth words I can define now that I have control
structures.

    
        ": ?dup       dup if dup then ;",
    
        ": abs        dup 0 < if negate then ;",
    
        ": max        2dup < if swap then drop ;",
        ": min        2dup > if swap then drop ;",
    
        ": space      bl emit ;",
        ": spaces     begin  dup 0> while  space 1-  repeat  drop ;",
    

I wish I could explain Forth's `POSTPONE`, but I can't, so you will just have
to Google it.

    
        ": postpone   bl word find  1 = if , else  '(lit) , ,  ['] , ,  then ; immediate",
    

A Forth `VALUE` is just like a constant in that it puts a value on the stack
when invoked.  However, the stored value can be modified with `TO`.

`VALUE` is, in fact, exactly the same as `CONSTANT` in this Forth.  And so you
could use `TO` to change the value of a constant, but that's against the rules.

    
        ": value    constant ;",
    
        ": value!   >body ! ;",
    
        ": to       state @ if",
        "               postpone ['] postpone value!",
        "           else",
        "               ' value!",
        "           then ; immediate",
    

`DEFER` and `IS` are similar to `VALUE` and `TO`, except that the value is an
execution token, and when the created word is used it invokes that xt.  `IS`
can be used to change the execution token.  In C++ terms, you can think of this
as a pointer to a function pointer.

`DEFER` and `IS` are not ANS Forth standard words, but are in common use, and
are described formally at <http://forth-standard.org/standard/core/DEFER>.

    
        ": defer       create ['] abort ,",
        "              does> @ execute ;",
    
        ": defer@      >body @ ;",
        ": defer!      >body ! ;",
    
        ": is          state @ if",
        "                  postpone ['] postpone defer!",
        "              else",
        "                  ' defer!",
        "              then ; immediate",
    
        ": action-of   state @ if",
        "                  postpone ['] postpone defer@",
        "              else",
        "                  ' defer@",
        "              then ; immediate",
    

Strings
-------

`S" ( "ccc<quote>" -- caddr u )`

This word parses input until it finds a `"` (double quote) and then puts the
resulting string's address and length on the stack.  It works in both
compilation and interpretation mode.

In interpretation mode, it just returns the address and length of the string in
the input buffer.

In compilation mode, I have to copy the string somewhere where it can be found
at execution time.  The word `SLITERAL` implements this.  It compiles a
forward-branch instruction, then copies the string's characters into the
current definition between the branch and its target instruction, then at the
branch target location I use a couple of `LITERAL`s to put the address and
length of the word in the definition onto the stack.

`." ( "ccc<quote>" -- )`

This word prints the given string.  We can implement it in terms of `S"` and
`TYPE`.

`.( ( "ccc<quote>" -- )`

This is like `."`, but is an immediate word.  It can be used to display output
during the compilation of words.

    
        ": sliteral",                      // ( c-addr len )
        "    postpone ahead",              // ( c-addr len orig )
        "    2dup swap >r >r",             // ( c-addr len orig ) ( R: len orig )
        "    cell+ swap",                  // copy into the first byte after the offset
        "    dup allot  cmove align",      // allocate dataspace and copy string into it
        "    r> dup postpone then",        // resolve the branch
        "    cell+ postpone literal",      // compile literal for address
        "    r> postpone literal",         // compile literal for length
        "; immediate",
    
        ": s\"   [char] \" parse",
        "        state @ if postpone sliteral then ; immediate",
    
        ": .\"   postpone s\" postpone type ; immediate",
    
        ": .(    [char] ) parse type ; immediate",
    

`/STRING ( c-addr1 u1 n1 -- c-addr2 u2 )` adjusts a character string by adding
to the address and subtracting from the length.

    
        ": /string   dup >r - swap r> + swap ;",
    

`ABORT"` checks whether a result is non-zero, and if so, it throws an exception
that will be caught by `QUIT`, which will print the given message and then
continue the interpreter loop.

    
        ": (abort\")   rot if abort-message then 2drop ;",
        ": abort\"     postpone s\" postpone (abort\") ; immediate",
    

`INCLUDED` is the word for reading additional source files. For example, you
can include the file `tests/hello.fs` and then run its `hello` word by doing
the following:

    s" tests/hello.fs" INCLUDED
    hello

`INCLUDE` is a simpler variation, used like this:

    INCLUDE tests/hello.fs
    hello

`INCLUDE` is not part of the AND standard, but is in Forth 2012.

    
    #ifndef CXXFORTH_DISABLE_FILE_ACCESS
    
        ": included",
        "    r/o open-file abort\" included: unable to open file\"",
        "    dup include-file",
        "    close-file abort\" included: unable to close file\" ;",
    
        ": include   bl word count included ;",
    
    #endif // #ifndef CXXFORTH_DISABLE_FILE_ACCESS
    

Comments
--------

There is a good reason that none of the Forth defintions above have had any
stack diagrams or other comments: our Forth doesn't support comments yet.  I
have to define words to implement comments.

I will support two standard kinds of Forth comments:

- If `\` (backslash) appears on a line, the rest of the line is ignored.
- Text between `(` and `)` on a single line is ignored.

Also, I will support `#!` as a synonym for `\`, so that we can start a UNIX
shell script with something like this:

    #! /usr/local/bin/cxxforth

Note that a space is required after the `\`, `(`, or `#!` that starts a
comment.  They are blank-delimited words just like every other Forth word.

To-Do: `(` should support multi-line comments.

    
        ": \\   source nip >in ! ; immediate",
        ": #!   postpone \\ ; immediate",
        ": (    [char] ) parse 2drop ; immediate",
    

`ABOUT` is not a standard word.  It just prints licensing and credit
information.

`.DQUOT` is also not a standard word.  It prints a double-quote (") character.

    
        ": .dquot   [char] \" emit ;",
    
        ": about",
        "      cr",
        "      .\" cxxforth " CXXFORTH_VERSION "\" cr",
        "      .\" by Kristopher Johnson\" cr",
        "      cr",
        "      .\" This is free and unencumbered software released into the public domain.\" cr",
        "      cr",
        "      .\" Anyone is free to copy, modify, publish, use, compile, sell, or distribute this\" cr",
        "      .\" software, either in source code form or as a compiled binary, for any purpose,\" cr",
        "      .\" commercial or non-commercial, and by any means.\" cr",
        "      cr",
        "      .\" In jurisdictions that recognize copyright laws, the author or authors of this\" cr",
        "      .\" software dedicate any and all copyright interest in the software to the public\" cr",
        "      .\" domain. We make this dedication for the benefit of the public at large and to\" cr",
        "      .\" the detriment of our heirs and successors. We intend this dedication to be an\" cr",
        "      .\" overt act of relinquishment in perpetuity of all present and future rights to\" cr",
        "      .\" this software under copyright law.\" cr",
        "      cr",
        "      .\" THE SOFTWARE IS PROVIDED \" .dquot .\" AS IS\" .dquot .\"  WITHOUT WARRANTY OF ANY KIND, EXPRESS OR\" cr",
        "      .\" IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,\" cr",
        "      .\" FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE\" cr",
        "      .\" AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN\" cr",
        "      .\" ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION\" cr",
        "      .\" WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.\" cr",
        "      cr",
        "      .\" For more, visit <https://github.com/kristopherjohnson/cxxforth>.\" cr ;",
    

The C++ `main()` function will look for the Forth word `MAIN` and execute it.

The `MAIN` word calls `PROCESS-ARGS`, which is not a standard word.  It looks
at the number of command-line arguments.  If there are no arguments other than
the executable path, then it prints the `WELCOME` message.  If there are
arguments, then it attempts to call `INCLUDED` on each of them.

If you want to write your own custom startup code, `MAIN` is the place to put
it.

    
        ": welcome",
        "    .\" cxxforth " CXXFORTH_VERSION "\" cr",
        "    .\" Type \" .dquot .\" about\" .dquot .\"  for more information.  \"",
        "    .\" Type \" .dquot .\" bye\" .dquot .\"  to exit.\" cr ;",
    
        ": process-args",
        "    #args 1 = if welcome exit then",
        "    1 begin",
        "        dup #args <",
        "    while",
        "        dup arg included cr",
        "        1+",
        "    repeat",
        "    drop ;",
    
        ": main   process-args quit ;",
    };
    
    

That is the end of the built-in Forth definitions.

With the `forthDefinitions` array filled, all I need to do is call `EVALUATE`
on each line to load them into the system.

    
    void defineForthWords() {
        static size_t lineCount = sizeof(forthDefinitions) / sizeof(forthDefinitions[0]);
        for (size_t i = 0; i < lineCount; ++i) {
            auto line = forthDefinitions[i];
            auto length = std::strlen(line);
            push(CELL(line));
            push(CELL(length));
            evaluate();
        }
    }
    
    void initializeDefinitions() {
        definitions.clear();
        definePrimitives();
        defineForthWords();
    }
    
    } // end anonymous namespace
    
    const char* cxxforth_version = CXXFORTH_VERSION;
    
    extern "C" void cxxforth_reset() {
    
        std::memset(dStack, 0, sizeof(dStack));
        dTop = dStack - 1;
    
        std::memset(rStack, 0, sizeof(rStack));
        rTop = rStack - 1;
    
        std::memset(dataSpace, 0, sizeof(dataSpace));
        dataPointer = dataSpace;
    
        initializeDefinitions();
    }
    
    extern "C" int cxxforth_main(int argc, const char** argv) {
        try {
            commandLineArgCount = static_cast<size_t>(argc);
            commandLineArgVector = argv;
    
            cxxforth_reset();
    
            auto mainXt = findDefinition("MAIN");
            if (!mainXt)
                throw runtime_error("MAIN not defined");
            mainXt->execute();
    
            return 0;
        }
        catch (const exception& ex) {
            cerr << "cxxforth: " << ex.what() << endl;
            return -1;
        }
    }
    

Finally we have our `main()`, which simply calls `cxxforth_main()`.

You can define the macro `CXXFORTH_NO_MAIN` to inhibit generation of `main()`.
This is useful for incorporating `cxxforth.cpp` into another application or
library.

    
    #ifndef CXXFORTH_NO_MAIN
    
    int main(int argc, const char** argv) {
        return cxxforth_main(argc, argv);
    }
    
    #endif // CXXFORTH_NO_MAIN
    
