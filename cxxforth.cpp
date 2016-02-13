/****

cxxforth: A Simple Forth Implementation in C++
==============================================

by Kristopher Johnson

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

For more information, please refer to <http://unlicense.org/>

----

`cxxforth` is a simple implementation of a [Forth][forth] system in C++.  There
are many examples of Forth implementations available on the Internet, but most
of them are written in assembly language or low-level C, with a focus in
maximizing efficiency and demonstrating traditional Forth implementation
techniques.  This Forth is different: Our goal is to use modern C++ to create a
Forth implementation that is easy to understand, easy to port, and easy to
extend.  We're not going to talk about register assignments or addressing modes
or opcodes or the trade-offs between indirect threaded code, direct threaded
code, subroutine threaded code, and token threaded code.  We are just going to
build a working Forth system in a couple thousand lines of code.

An inspiration for this implementation is Richard W.M. Jones's
[JONESFORTH][jonesforth].  JONESFORTH is a low-level assembly-language Forth
implementation written as a very readable tutorial, and we are going to copy
its style for our higher-level implementation.  Our Forth kernel is written as
C++ file with large comment blocks, and we have a utility, `cpp2md`, that will
take that C++ file and convert it to a [Markdown][markdown] format document
with nicely formatted commentary sections between the C++ code blocks.  If you
are currently reading `cxxforth.cpp` online, consider reading the Markdown file
[cxxforth.cpp.md](cxxforth.cpp.md) instead, as BitBucket or GitHub will
automtically display the Markdown file as formatted HTML.

As in other Forth systems, the basic design of this Forth is to create a small
kernel in native code (C++, in our case), and then implement the rest of the
system with Forth code.  The kernel has to provide the basic primitives needed
for memory access, arithmetic, logical operations, and operating system access.
With those primitives, we can then write Forth code to flesh out the system.

We are writing C++ conforming to the C++14 standard.  If your C++ compiler
does not support C++14 yet, you may need to make some modifications to the code
to get it to build.

The Forth words provided by cxxforth are based on those in the [ANS Forth draft
standard][dpans].  We don't claim conformance to the standard, but the reader
can use the draft standard as a crude form of documentation for the Forth words
we implement.  cxxforth implements many of the words from the standard's Core
and Core Extension word sets, and a smattering of words from other standard
word sets.

It is assumed that the reader has some familiarity in C++ and Forth.  It is
recommended that the reader first read the JONESFORTH source or the source of
some other Forth implementation to get the basic gist of how Forth is usually
implemented.

[forth]: https://en.wikipedia.org/wiki/Forth_(programming_language) "Forth (programming language)"

[jonesforth]: http://git.annexia.org/?p=jonesforth.git;a=blob;f=jonesforth.S;h=45e6e854a5d2a4c3f26af264dfce56379d401425;hb=HEAD

[markdown]: https://daringfireball.net/projects/markdown/ "Markdown"

[dpans]: http://forth.sourceforge.net/std/dpans/dpansf.htm "Alphabetic list of words"

----

Building cxxforth
-----------------

Building the `cxxforth` executable and other targets is easiest if you are on a
UNIX-ish system that has `make`, `cmake`, and `clang` or `gcc`.  If you have
those components, you can probably build by just entering these commands:

    cd myfiles/cxxforth
    make

If successful, the `cxxforth` executable will be built in the `cxxforth/build/`
subdirectory.

If you don't have one of those components, or if 'make' doesn't work, then it's
not too hard to build it manually.  You will need to create a file called
`cxxforthconfig.h`, which can be empty, then you need to invoke your C++
compiler on the `cxxforth.cpp` source file, enabling whatever options might be
needed for C++14 compatibility.  For example, on a Linux system with gcc, you
should be able to build it by entering these commands:

    cd myfiles/cxxforth
    touch cxxforthconfig.h
    g++ -std=c++14 -o cxxforth cxxforth.cpp

----

The Code
--------

In `cxxforth.cpp`, we start by including headers from the C++ Standard Library.
We also include `cxxforth.h`, which declares exported functions and includes
the `cxxforthconfig.h` file produced by the CMake build.

****/

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

/****

cxxforht can use the GNU Readline library for user input if it is available.

The CMake build will detect whether the library is available, and if so define
`CXXFORTH_USE_READLINE`.  However, you may not want to link your executable
with GNU Readline due to its licensing terms.  You can pass
`-DCXXFORTH_DISABLE_READLINE=ON` to `cmake` to prevent it from searching for
the library.

****/

#ifdef CXXFORTH_USE_READLINE
#include "readline/readline.h"
#include "readline/history.h"
#endif

/****

We have a few macros to define the size of the Forth data space, the maximum
numbers of cells on the data and return stacks, and the maximum number of word
definitions in the dictionary.

These macros are usually defined in the `cxxforthconfig.h` header that is
generated by CMake and included by `cxxforth.h`, but we provide default values
in case they have not been defined.

****/

#ifndef CXXFORTH_DATASPACE_SIZE
#define CXXFORTH_DATASPACE_SIZE (16 * 1024 * sizeof(Cell))
#endif

#ifndef CXXFORTH_DSTACK_COUNT
#define CXXFORTH_DSTACK_COUNT (256)
#endif

#ifndef CXXFORTH_RSTACK_COUNT
#define CXXFORTH_RSTACK_COUNT (256)
#endif

/****

We'll start by defining some basic types.  A `Cell` is the basic Forth type.
We define our cell using the C++ `uintptr_t` type to ensure it is large enough
to hold an address.  This generally means that it will be a 32-bit value on
32-bit platforms and a 64-bit value on 64-bit platforms.  (If you want to build
a 32-bit Forth on a 64-bit platform with clang or gcc, you can pass the `-m32`
flag to the compiler and linker.)

We won't be providing any of the double-cell operations that traditional Forths
provide.  Double-cell operations were important in the days of 8-bit and 16-bit
Forths, but with cells of 32 bits or more, many applications have no need for
them.

We also aren't dealing with floating-point values.  Floating-point support
would be useful, but we're trying to keep this simple.

Forth doesn't require type declarations; a cell can be used as an address, an
unsigned integer, a signed integer, or a variety of other uses.  However, in
C++ we will have to be explicit about types to perform the operations we want
to perform.  So, we define a few types, and a few macros to avoid littering our
code with `reinterpret_cast<>()`.

****/

namespace {

using Cell = uintptr_t;
using SCell = intptr_t;

using Char = unsigned char;
using SChar = signed char;

using CAddr = Char*;  // Any address
using AAddr = Cell*;  // Cell-aligned address

#define CELL(x)    reinterpret_cast<Cell>(x)
#define CADDR(x)   reinterpret_cast<Char*>(x)
#define AADDR(x)   reinterpret_cast<AAddr>(x)
#define CHARPTR(x) reinterpret_cast<char*>(x)
#define SIZE_T(x)  static_cast<std::size_t>(x)

constexpr auto CellSize = sizeof(Cell);

/****

We define constants for Forth true and false boolean flag values.

Note that the Forth standard says that a true flag is a cell with all bits set,
unlike the C++ convention of using 1 or any other non-zero value to mean true,
so we need to be sure to use these constants for all Forth words that return a
boolean flag.

****/

constexpr Cell False = 0;
constexpr Cell True = ~False;

/****

Our first big difference from most traditional Forth implementations is how
we'll store the word definitions for our Forth dictionary.  Traditional Forths
intersperse the word names in the shared data space along with code and data,
using a linked list to navigate through them.  We are going to just have an
array of `Definition` structs.

One of the members of `Definition` is a `std::string` to hold the name, so we
won't need to worry about managing the memory for that variable-length field.
We can use its `data()` function when we need a pointer to the raw character
data.

A `Definition` also needs a `code` field that points to the native code
associated with the word, a `parameter` field that points to the data-space
elements associated with the word, and some bit flags to keep track of whether
the word is `IMMEDIATE` and/or `HIDDEN`.  We will explore the use of these
fields later when we talk about the interpreter.

Note that the `code` field is the first member of the `Definition` struct.
This means that the address of a `Definition` is also the address of a code
field.  We will use such an address as our Forth _execution token_ (xt).

`Definition` has a static field `executingWord` that contains the address
of the `Definition` that was most recently executed.  This can be used by
`Code` functions to refer to their definitions.

****/

using Code = void(*)();

struct Definition {
    Code        code      = nullptr;
    AAddr       does      = nullptr;
    AAddr       parameter = nullptr;
    Cell        flags     = 0;
    std::string name;

    static constexpr Cell FlagHidden    = (1 << 1);
    static constexpr Cell FlagImmediate = (1 << 2);

    static const Definition* executingWord;

    void execute() const {
        auto previousValue = executingWord;
        executingWord = this;

        code();

        executingWord = previousValue;
    }

    const char* nameAddress() const {
        return name.data();
    }

    std::size_t nameLength() const {
        return name.length();
    }

    bool isHidden() const {
        return (flags & FlagHidden) != 0;
    }

    void toggleHidden() {
        flags ^= FlagHidden;
    }

    bool isImmediate() const {
        return (flags & FlagImmediate) != 0;
    }

    void toggleImmediate() {
        flags ^= FlagImmediate;
    }
};

/****

With our types defined, we can define our global variables.  We start with the
Forth data space, data and return stacks.

For each of these arrays, we also define constants that point to the end of the
array, so we can easily test whether we have filled them and need to report an
overflow.

****/

Char dataSpace[CXXFORTH_DATASPACE_SIZE];
Cell dStack[CXXFORTH_DSTACK_COUNT];
Cell rStack[CXXFORTH_RSTACK_COUNT];

constexpr CAddr dataSpaceLimit = &dataSpace[CXXFORTH_DATASPACE_SIZE];
constexpr AAddr dStackLimit    = &dStack[CXXFORTH_DSTACK_COUNT];
constexpr AAddr rStackLimit    = &rStack[CXXFORTH_RSTACK_COUNT];

/****

The Forth dictionary is a list of Definitions.  The most recent definition is
at the back of the list.

****/

std::list<Definition> definitions;

/****

For each of the global arrays, we need a pointer to the current location.

For the data space, we have the `dataPointer`, which corresponds to Forth's
`HERE`.

For each of the stacks, we need a pointer to the element at the top of the
stack.  The stacks grow upward.  When a stack is empty, the associated pointer
points to an address below the actual bottom of the array, so we will need to
avoid dereferencing these pointers under those circumstances.

****/

CAddr dataPointer = nullptr;
AAddr dTop        = nullptr;
AAddr rTop        = nullptr;

/****

The colon-definition interpreter needs a pointer to the next instruction to be
executed.

****/

Definition** next = nullptr;

/****

We have to define the static `executingWord` member we declared in
`Definition`.

****/

const Definition* Definition::executingWord = nullptr;

/****

There are a few special words whose addresses we will use frequently when
compiling or executing.  Rather than looking them up in the dictionary as
needed, we'll cache their values.

****/

Definition* doLiteralXt = nullptr;
Definition* setDoesXt   = nullptr;
Definition* exitXt      = nullptr;

/****

We need a flag to track whether we are in interpreting or compiling state.
This corresponds to Forth's `STATE` variable.

****/

Cell isCompiling = False;

/****

We provide a variable that controls the numeric base used for conversion
between numbers and text.  This corresponds to the Forth `BASE` variable.

****/

Cell numericBase = 10;

/****

The input buffer is a `std::string`.  This makes it easy to use the C++ I/O
facilities, and frees us from the need to allocate a statically sized buffer
that could overflow.  We also have a current offset into this buffer,
corresponding to the Forth `>IN` variable.

****/

std::string inputBuffer;
Cell inputOffset = 0;

/****

We need a buffer to store the result of the Forth `WORD` word.  As with the
input buffer, we use a `std::string` so we don't need to worry about memory
management.

Note that while we are using a `std:string`, we can't just use it like one.
The buffer returned by `WORD` has the word length as its first character.
So if we need to display the contents of this buffer, we will need to
convert it to a real string.

****/

std::string wordBuffer;

/****

We need a buffer to store the result of the Forth `PARSE` word.  Unlike `WORD`,
we won't need to store the count at the beginning of the buffer.

****/

std::string parseBuffer;

/****

We store the `argc` and `argv` values passed to `main()`, for use by the Forth
program.  These are made available by our non-standard `#ARG` and `ARG` Forth
words.

****/

size_t commandLineArgCount = 0;
const char** commandLineArgVector = nullptr;

/****

Stack Primitives
----------------

We will be spending a lot of time pushing and popping values to and from our
data and return stacks, so in lieu of sprinkling pointer arithmetic all through
our code, we'll define a few simple functions to handle those operations.  We
can expect the compiler to expand calls to these functions inline, so we aren't
losing any efficiency.

We will use the expressions `*dTop` and `*rTop` when accessing the top-of-stack
values without pushing/popping.  We will also use expressions like `*(dTop -
1)` and `*(dTop - 2)` to reference the items beneath the top of stack.

****/

void resetDStack() {
    dTop = dStack - 1;
}

void resetRStack() {
    rTop = rStack - 1;
}

std::ptrdiff_t dStackDepth() {
    return dTop - dStack + 1;
}

std::ptrdiff_t rStackDepth() {
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

/****

Exceptions
----------

Forth provides the `ABORT` and `ABORT"` words, which interrupt execution and
return control to the main `QUIT` loop.  We will implement this functionality
using a C++ exception to return control to the top-level interpreter.

****/

class AbortException: public std::runtime_error {
public:
    AbortException(const std::string& msg): std::runtime_error(msg) {}
    AbortException(const char* msg): std::runtime_error(msg) {}
};

// ABORT ( i*x -- ) ( R: j*x -- )
void abort() {
    throw AbortException("");
}

// ABORT-MESSAGE ( i*x c-addr u -- ) ( R: j*x -- )
// Same semantics as ABORT", but takes a string address and length instead
// of parsing message.
void abortMessage() {
    auto count = SIZE_T(*dTop); pop();
    auto caddr = CHARPTR(*dTop); pop();
    std::string message(caddr, count);
    throw AbortException(message);
}

/****

Runtime Safety Checks
---------------------

Old-school Forths are implemented by super-programmers who never make coding
mistakes and so don't want the overhead of bounds-checking or other nanny
hand-holding.  However, we're just dumb C++ programmers here, and we'd like
some help to catch our mistakes.

To that end, we have a set of macros and functions that verify that we have the
expected number of arguments available on our stacks, that we aren't going to
run off the end of an array, that we aren't going to try to divide by zero, and
so on.

We can define the macro `CXXFORTH_SKIP_RUNTIME_CHECKS` to generate an
executable that doesn't include these checks, so when we have a fully debugged
Forth application we can run it on that optimized executable for improvied
performance.

When the `CXXFORTH_SKIP_RUNTIME_CHECKS` macro is not defined, these macros
will check conditions and throw an `AbortException` if the assertions fail.
We won't go into the details of these macros here.  Later you will see them
used in the definitions of our primitive Forth words.

****/

#ifdef CXXFORTH_SKIP_RUNTIME_CHECKS

#define RUNTIME_ERROR(msg)                   do { } while (0)
#define RUNTIME_ERROR_IF(cond, msg)          do { } while (0)
#define REQUIRE_DSTACK_DEPTH(n, name)        do { } while (0)
#define REQUIRE_DSTACK_AVAILABLE(n, name)    do { } while (0)
#define REQUIRE_RSTACK_DEPTH(n, name)        do { } while (0)
#define REQUIRE_RSTACK_AVAILABLE(n, name)    do { } while (0)
#define REQUIRE_ALIGNED(addr, name)          do { } while (0)
#define REQUIRE_VALID_HERE(name)             do { } while (0)
#define REQUIRE_DATASPACE_AVAILABLE(n, name) do { } while (0)

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
                     std::string(name) + ": unaligned address");
}

void requireDStackDepth(std::size_t n, const char* name) {
    RUNTIME_ERROR_IF(dStackDepth() < static_cast<std::ptrdiff_t>(n),
                     std::string(name) + ": stack underflow");
}

void requireDStackAvailable(std::size_t n, const char* name) {
    RUNTIME_ERROR_IF((dTop + n) >= dStackLimit,
                     std::string(name) + ": stack overflow");
}

void requireRStackDepth(std::size_t n, const char* name) {
    RUNTIME_ERROR_IF(rStackDepth() < std::ptrdiff_t(n),
                     std::string(name) + ": return stack underflow");
}

void requireRStackAvailable(std::size_t n, const char* name) {
    RUNTIME_ERROR_IF((rTop + n) >= rStackLimit,
                     std::string(name) + ": return stack overflow");
}

void checkValidHere(const char* name) {
    RUNTIME_ERROR_IF(dataPointer < dataSpace || dataSpaceLimit <= dataPointer,
                     std::string(name) + ": HERE outside data space");
}

void requireDataSpaceAvailable(std::size_t n, const char* name) {
    RUNTIME_ERROR_IF((dataPointer + n) >= dataSpaceLimit,
                     std::string(name) + ": data space overflow");
}

#endif // CXXFORTH_SKIP_RUNTIME_CHECKS

/****

Next we'll define the basic Forth stack manipulation words.  When changing the
stack, we don't change the stack depth any more than necessary.  For example,
`SWAP` and `DROP` just rearrange elements on the stack, rather than doing any
popping or pushing.

Note that for C++ functions that implement primitive Forth words, we will
include the Forth names and stack effects in comments. You can look up the
Forth names in the [ANS Forth draft standard][dpans] to learn what these words
are supposed to do.

****/

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

// OVER ( x1 x2 -- x1 x2 x1 )
void over() {
    REQUIRE_DSTACK_DEPTH(2, "OVER");
    REQUIRE_DSTACK_AVAILABLE(1, "OVER");
    push(*(dTop - 1));
}

// SWAP ( x1 x2 -- x2 x1 )
void swap() {
    REQUIRE_DSTACK_DEPTH(2, "SWAP");
    auto temp = *dTop;
    *dTop = *(dTop - 1);
    *(dTop - 1) = temp;
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

// TRUE ( -- flag )
void pushTrue() {
    REQUIRE_DSTACK_AVAILABLE(1, "TRUE");
    push(True);
}

// FALSE ( -- flag )
void pushFalse() {
    REQUIRE_DSTACK_AVAILABLE(1, "FALSE");
    push(False);
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

/****

Another important set of Forth primitives are those for reading and writing
values in data space.

****/

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
    auto x = *dTop; pop();
    *caddr = static_cast<Char>(x);
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

/****

Now we will define the Forth words for accessing and manipulating the data
space pointer.

****/

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

// , ( x -- )
void comma() {
    auto value = *dTop; pop();
    data(value);
}

// Store a char to data space.
void cdata(Cell x) {
    REQUIRE_VALID_HERE("C,");
    REQUIRE_DATASPACE_AVAILABLE(1, "C,");
    *dataPointer = static_cast<Char>(x);
    dataPointer += 1;
}

// C, ( char -- )
void cComma() {
    auto value = *dTop; pop();
    cdata(value);
}

// UNUSED ( -- u )
void unused() {
    REQUIRE_DSTACK_AVAILABLE(1, "UNUSED");
    push(static_cast<Cell>(dataSpaceLimit - dataPointer));
}

/****

Here we will define our I/O primitives.  We keep things easy and portable by
using C++ iostream objects.

****/

// EMIT ( x -- )
void emit() {
    REQUIRE_DSTACK_DEPTH(1, "EMIT");
    auto cell = *dTop; pop();
    std::cout.put(static_cast<char>(cell));
}

// TYPE ( c-addr u -- )
void type() {
    REQUIRE_DSTACK_DEPTH(2, "TYPE");
    auto length = static_cast<std::streamsize>(*dTop); pop();
    auto caddr = CHARPTR(*dTop); pop();
    std::cout.write(caddr, length);
}

// CR ( -- )
void cr() {
    std::cout << std::endl;
}

// . ( n -- )
void dot() {
    REQUIRE_DSTACK_DEPTH(1, ".");
    std::cout << std::setbase(static_cast<int>(numericBase)) << static_cast<SCell>(*dTop) << " ";
    pop();
}

void uDot() {
    REQUIRE_DSTACK_DEPTH(1, "U.");
    std::cout << std::setbase(static_cast<int>(numericBase)) << *dTop << " ";
    pop();
}

// BASE ( -- a-addr )
void base() {
    REQUIRE_DSTACK_AVAILABLE(1, "BASE");
    push(CELL(&numericBase));
}

// SOURCE ( -- c-addr u )
void source() {
    REQUIRE_DSTACK_AVAILABLE(2, "SOURCE");
    push(CELL(inputBuffer.data()));
    push(inputBuffer.length());
}

// >IN ( -- a-addr )
void in() {
    REQUIRE_DSTACK_AVAILABLE(1, ">IN");
    push(CELL(&inputOffset));
}

/****

`REFILL` reads a line from the user input device.  If successful, it puts the
result into `inputBuffer`, sets `inputOffset` to 0, and pushes a `TRUE` flag
onto the stack.  If not successful, it pushes a `FALSE` flag.

We use GNU Readline if configured to do so.  Otherwise we use the C++
`std::getline()` function.

****/

// REFILL ( -- flag )
void refill() {
    REQUIRE_DSTACK_AVAILABLE(1, "REFILL");

#ifdef CXXFORTH_USE_READLINE
    char* line = readline("");
    if (line) {
        inputBuffer = line;
        inputOffset = 0;
        if (*line)
            add_history(line);
        std::free(line);
        pushTrue();
    }
    else {
        pushFalse();
    }
#else
    if (std::getline(std::cin, inputBuffer)) {
        inputOffset = 0;
        pushTrue();
    }
    else {
        pushFalse();
    }
#endif
}

/****

The text interpreter and many Forth words use `WORD` to parse a set of
characters from the input.  `WORD` skips any delimiters at the current input
position, then reads characters until it finds the delimiter again.  It returns
the address of a buffer with the length in the first byte, followed by the
characters that made up the word.

In a few places below, you will see the call sequence `bl(); word(); count();`
This corresponds to the Forth phrase `BL WORD COUNT`, which is how many Forth
definitions read a space-delimited word from the input and get its address and
length.

The ANS Forth draft standard specifies that the buffer must contain a space
character after the character data, but we aren't going to worry about this
obsolescent requirement.

****/

// WORD ( char "<chars>ccc<char>" -- c-addr )
void word() {
    REQUIRE_DSTACK_DEPTH(1, "WORD");
    auto delim = static_cast<char>(*dTop);

    wordBuffer.clear();
    wordBuffer.push_back(0);  // First char of buffer is length.

    auto inputSize = inputBuffer.size();

    // Skip leading delimiters
    while (inputOffset < inputSize && inputBuffer[inputOffset] == delim)
        ++inputOffset;

    // Copy characters until we see the delimiter again.
    while (inputOffset < inputSize && inputBuffer[inputOffset] != delim) {
        wordBuffer.push_back(inputBuffer[inputOffset]);
        ++inputOffset;
    }

    // Update the count at the beginning of the buffer.
    wordBuffer[0] = static_cast<char>(wordBuffer.size() - 1);

    *dTop = CELL(wordBuffer.data());
}

// PARSE ( char "ccc<char>" -- c-addr u )
void parse() {
    REQUIRE_DSTACK_DEPTH(1, "PARSE");
    REQUIRE_DSTACK_AVAILABLE(1, "PARSE");
    
    auto delim = static_cast<char>(*dTop);

    parseBuffer.clear();

    auto inputSize = inputBuffer.size();

    // Copy characters until we see the delimiter.
    while (inputOffset < inputSize && inputBuffer[inputOffset] != delim) {
        parseBuffer.push_back(inputBuffer[inputOffset]);
        ++inputOffset;
    }

    *dTop = CELL(parseBuffer.data());
    push(static_cast<Cell>(parseBuffer.size()));
}

/****

`BL` puts a space character on the stack.  It is often used as `BL WORD` to
parse a space-delimited word.

****/

// BL ( -- char )
void bl() {
    REQUIRE_DSTACK_AVAILABLE(1, "BL");
    push(' ');
}

/****

Define arithmetic primitives.

****/

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

// NEGATE ( n1 -- n2 )
void negate() {
    REQUIRE_DSTACK_DEPTH(1, "NEGATE");
    *dTop = static_cast<Cell>(-static_cast<SCell>(*dTop));
}

/****

Define logical and relational primitives.

****/

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

// INVERT ( x1 -- x2 )
void invert() {
    REQUIRE_DSTACK_DEPTH(1, "INVERT");
    *dTop = ~*dTop;
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

// > ( n1 n2 -- flag )
void greaterThan() {
    REQUIRE_DSTACK_DEPTH(2, ">");
    auto n2 = static_cast<SCell>(*dTop); pop();
    *dTop = static_cast<SCell>(*dTop) > n2 ? True : False;
}

/****

Define system and environmental primitives

****/

// #ARG ( -- n )
// Provide count of command-line arguments.
// Not an ANS Forth word.
void argCount() {
    REQUIRE_DSTACK_AVAILABLE(1, "#ARG");
    push(commandLineArgCount);
}

// ARG ( n -- c-addr u )
// Provide the Nth command-line argument.
// Not an ANS Forth word.`
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

// MS ( u -- )
void ms() {
    REQUIRE_DSTACK_DEPTH(1, "MS");
    auto period = *dTop; pop();
    std::this_thread::sleep_for(std::chrono::milliseconds(period));
}

// TIME&DATE ( -- +n1 +n2 +n3 +n4 +n5 +n6 )
void timeAndDate () {
    REQUIRE_DSTACK_AVAILABLE(6, "TIME&DATE");
    auto t = std::time(0);
    auto tm = std::localtime(&t);
    push(static_cast<Cell>(tm->tm_sec));
    push(static_cast<Cell>(tm->tm_min));
    push(static_cast<Cell>(tm->tm_hour));
    push(static_cast<Cell>(tm->tm_mday));
    push(static_cast<Cell>(tm->tm_mon + 1));
    push(static_cast<Cell>(tm->tm_year + 1900));
}

// UTCTIME&DATE ( -- +n1 +n2 +n3 +n4 +n5 +n6 )
// Not an ANS Forth word.
// Like TIME&DATE, but returns UTC rather than local time.
void utcTimeAndDate () {
    REQUIRE_DSTACK_AVAILABLE(6, "UTCTIME&DATE");
    auto t = std::time(0);
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
    std::cout << std::setbase(static_cast<int>(numericBase)) << "<" << depth << "> ";
    for (auto i = depth; i > 0; --i) {
        std::cout << static_cast<SCell>(*(dTop - i + 1)) << " ";
    }
}

/****

Inner Interpreter
-----------------

A Forth system is said to have two interpreters: an "outer interpreter" which
reads input and interprets it, and an "inner interpreter" which executes
compiled Forth definitions.  We'll start with the inner interpreter.

There are basically two kinds of words in a Forth system:

- primitive code: native code fragments that are executed directly by the CPU
- colon definition: a sequence of Forth words compiled by `:` (colon), `:NONAME`, or `DOES>`.

Every defined word has a `code` field that points to native code.  In the case
of "code" words, the `code` field points to a routine that performs the
operation.  In the case of a colon definition, the `code` field points to the
`doColon()` function, which saves the current program state and then starts
executing the words that make up the colon definition.

Each colon definition ends with a call to `EXIT`, which sets up a return to the
colon definition that called the current word.

****/

void doColon() {
    rpush(CELL(next));

    auto defn = Definition::executingWord;

    next = reinterpret_cast<Definition**>(defn->does);
    while (*next != exitXt) {
        (*(next++))->execute();
    }

    next = reinterpret_cast<Definition**>(*rTop); rpop();
}

// EXIT ( -- ) ( R: nest-sys -- )
void exit() {
    throw std::runtime_error("EXIT should not be executed");
}

// (literal) ( -- x )
// Compiled by LITERAL.
// Not an ANS Forth word.
void doLiteral() {
    REQUIRE_DSTACK_AVAILABLE(1, "(literal)");
    push(CELL(*next));
    ++next;
}

// LITERAL Compilation: ( x -- )  Runtime: ( -- x )
void literal() {
    REQUIRE_DSTACK_DEPTH(1, "LITERAL");
    data(CELL(doLiteralXt));
    data(*dTop); pop();
}

// EXECUTE ( i*x xt -- j*x )
void execute() {
    REQUIRE_DSTACK_DEPTH(1, "EXECUTE");
    auto defn = reinterpret_cast<Definition*>(*dTop); pop();
    defn->execute();
}

/****

Compilation
-----------

****/

// Return reference to the latest Definition.
// Undefined behavior if the definitions list is empty.
Definition& lastDefinition() {
    return definitions.back();
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
    defn.name = std::string(caddr, length);
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

void doDoes() {
    doCreate();
    doColon();
}

void setDoes() {
    auto& latest = lastDefinition();
    latest.code = doDoes;
    latest.does = AADDR(next) + 1;
}

// DOES>
void does() {
    data(CELL(setDoesXt));
    data(CELL(exitXt));
}

// ; ( C: colon-sys -- )
void semicolon() {
    data(CELL(exitXt));
    isCompiling = false;
    lastDefinition().toggleHidden();
}

// IMMEDIATE ( -- )
//
// Note: Unlike ANS Forth, our IMMEDIATE toggles the immediacy bit of the most
// recent definition, rather than always setting it true.
void immediate() { lastDefinition().toggleImmediate(); }

// HIDDEN ( -- )
// Not an ANS Forth word.
// Toggles the hidden bit of the ost recent definition.
void hidden() { lastDefinition().toggleHidden(); }

void defineCodeWord(const char* name, Code code) {
    alignDataPointer();

    Definition defn;
    defn.code = code;
    defn.parameter = defn.does = AADDR(dataPointer);
    defn.name = name;
    definitions.emplace_back(std::move(defn));
}

bool doNamesMatch(CAddr name1, CAddr name2, Cell nameLength) {
    for (size_t i = 0; i < nameLength; ++i) {
        if (std::toupper(name1[i]) != std::toupper(name2[i])) {
            return false;
        }
    }
    return true;
}

Definition* findDefinition(CAddr nameToFind, Cell nameLength) {
    if (nameLength == 0)
        return nullptr;

    for (auto i = definitions.rbegin(); i != definitions.rend(); ++i) {
        auto& defn = *i;
        if (defn.isHidden())
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

Definition* findDefinition(const std::string& name) {
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

// WORDS ( -- )
void words() {
    std::for_each(definitions.rbegin(), definitions.rend(), [](auto& word) {
        if (!word.isHidden()) std::cout << word.name << " ";
    });
}

/****

Outer Interpreter
-----------------

See [section 3.4 of the ANS Forth draft standard][dpans_3_4] for a description
of the Forth text interpreter.

[dpans_3_4]: http://forth.sourceforge.net/std/dpans/dpans3.htm#3.4 "3.4 The Forth text interpreter"

****/

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

Cell digitValue(Char c) {
    if (c >= 'a')
        return c - 'a' + 10;
    else if (c >= 'A')
        return c - 'A' + 10;
    else
        return c - '0';
}

// >UNUM ( u0 c-addr1 u1 -- u c-addr2 u2 )
// Not an ANS Forth word.
//
// This word is similar to ANS Forth's >NUMBER, but provides a single-cell result.
void parseUnsignedNumber() {
    REQUIRE_DSTACK_DEPTH(3, ">UNUM");

    auto length = SIZE_T(*dTop);
    auto caddr = CADDR(*(dTop - 1));
    auto value = *(dTop - 2);

    auto i = std::size_t(0);
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
// Not an ANS Forth word.
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
        *(dTop - 2) = static_cast<Cell>(SCell(-1) * static_cast<SCell>(*(dTop - 2)));
    }
    else {
        parseUnsignedNumber();
    }
}

// INTERPRET ( i*x -- j*x )
// Not an ANS Forth word.
// Reads words from the input buffer and executes/compiles them.
void interpret() {
    auto inputSize = inputBuffer.size();
    while (inputOffset < inputSize) {
        bl(); word(); find();

        auto found = static_cast<int>(*dTop); pop();

        if (found) {
            auto xt = reinterpret_cast<Definition*>(*dTop); pop();
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
                    throw AbortException(std::string("unrecognized word: ") + std::string(caddr, length));
                }
            }
            else {
                pop();
                return;
            }
        }
    }
}

// PROMPT ( -- )
// Not an ANS Forth word.
// Displays "ok" prompt if in interpretation mode.
void prompt() {
    if (!isCompiling) {
        std::cout << "  ok";
        cr();
    }
}

/****

`QUIT` is the top-level outer interpreter loop. It calls `REFILL` to read a
line, `INTERPRET` to parse and execute that line, then `PROMPT` and repeat
until there is no more input.

There is an exception handler for `AbortException` that prints an error
message, resets the stacks, and continues.

If end-of-input occurs, then it exits the loop and calls `CR` and `BYE`.

If `QUIT` is called from a word called by `QUIT`, control returns to the
top-level loop.

****/

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
            std::string msg(abortEx.what());
            if (msg.length() > 0) {
                std::cout << "<<< Error: " << msg << " >>>" << std::endl;
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

// EVALUATE ( i*x c-addr u -- j*x )
void evaluate() {
    REQUIRE_DSTACK_DEPTH(2, "EVALUATE");

    auto length = static_cast<std::size_t>(*dTop); pop();
    auto caddr = CHARPTR(*dTop); pop();

    auto savedInput = std::move(inputBuffer);
    auto savedOffset = inputOffset;

    inputBuffer = std::string(caddr, length);
    inputOffset = 0;
    interpret();

    inputBuffer = std::move(savedInput);
    inputOffset = savedOffset;
}

/****

Initialization
--------------

In `initializeDefinitions()`, we set up the initial contents of the dictionary.
This is the Forth kernel that Forth code can use to implement the rest of a
working system.

****/

void definePrimitives() {
    static struct {
        const char* name;
        Code code;
    } immediateCodeWords[] = {
        // name           code
        // ------------------------------
        {";",             semicolon},
        {"DOES>",         does},
        {"IMMEDIATE",     immediate},
        {"LITERAL",       literal},
    };
    for (auto& w: immediateCodeWords) {
        defineCodeWord(w.name, w.code);
        immediate();
    }

    static struct {
        const char* name;
        Code code;
    } codeWords[] = {
        // name           code
        // ------------------------------
        {"!",             store},
        {"#ARG",          argCount},
        {"(does)",        setDoes},
        {"(literal)",     doLiteral},
        {"*",             star},
        {"+",             plus},
        {",",             comma},
        {"-",             minus},
        {".",             dot},
        {".S",            dotS},
        {"/",             slash},
        {"/MOD",          slashMod},
        {":",             colon},
        {"<",             lessThan},
        {"=",             equals},
        {">",             greaterThan},
        {">IN",           in},
        {">NUM",          parseSignedNumber},
        {">R",            toR},
        {">UNUM",         parseUnsignedNumber},
        {"@",             fetch},
        {"ABORT",         abort},
        {"ABORT-MESSAGE", abortMessage},
        {"ALIGN",         align},
        {"ALIGNED",       aligned},
        {"ALLOT",         allot},
        {"AND",           bitwiseAnd},
        {"ARG",           argAtIndex},
        {"BASE",          base},
        {"BL",            bl},
        {"BYE",           bye},
        {"C!",            cstore},
        {"C,",            cComma},
        {"C@",            cfetch},
        {"CELLS",         cells},
        {"COUNT",         count},
        {"CR",            cr},
        {"CREATE",        create},
        {"DEPTH",         depth},
        {"DROP",          drop},
        {"DUP",           dup},
        {"EMIT",          emit},
        {"EVALUATE",      evaluate},
        {"EXECUTE",       execute},
        {"EXIT",          exit},
        {"FALSE",         pushFalse},
        {"FIND",          find},
        {"HERE",          here},
        {"HIDDEN",        hidden},
        {"INTERPRET",     interpret},
        {"INVERT",        invert},
        {"LSHIFT",        lshift},
        {"MS",            ms},
        {"NEGATE",        negate},
        {"OR",            bitwiseOr},
        {"OVER",          over},
        {"PARSE",         parse},
        {"PICK",          pick},
        {"PROMPT",        prompt},
        {"QUIT",          quit},
        {"R>",            rFrom},
        {"R@",            rFetch},
        {"REFILL",        refill},
        {"ROLL",          roll},
        {"RSHIFT",        rshift},
        {"SOURCE",        source},
        {"STATE",         state},
        {"SWAP",          swap},
        {"TIME&DATE",     timeAndDate},
        {"TRUE",          pushTrue},
        {"TYPE",          type},
        {"U.",            uDot},
        {"UNUSED",        unused},
        {"UTCTIME&DATE",  utcTimeAndDate},
        {"WORD",          word},
        {"WORDS",         words},
        {"XOR",           bitwiseXor},
    };
    for (auto& w: codeWords) {
        defineCodeWord(w.name, w.code);
    }

    doLiteralXt = findDefinition("(literal)");
    if (doLiteralXt == nullptr) throw std::runtime_error("Can't find (literal) in kernel dictionary");
    doLiteralXt->toggleHidden();

    setDoesXt = findDefinition("(does)");
    if (setDoesXt == nullptr) throw std::runtime_error("Can't find (does) in kernel dictionary");
    setDoesXt->toggleHidden();

    exitXt = findDefinition("EXIT");
    if (exitXt == nullptr) throw std::runtime_error("Can't find EXIT in kernel dictionary");
}

void defineBuiltins() {
    static const char* lines[] = {

        ": [  FALSE STATE ! ; IMMEDIATE",
        ": ]  TRUE STATE ! ;",

        ": ROT    2 ROLL ;",
        ": NIP    SWAP DROP ;",
        ": TUCK   SWAP OVER ;",
        ": 2DROP  DROP DROP ;",
        ": 2DUP   OVER OVER ;",
        ": 2OVER  3 PICK 3 PICK ;",
        ": 2SWAP  3 ROLL 3 ROLL ;",
        ": 2>R    SWAP >R >R ;",
        ": 2R>    R> R> SWAP ;",
        ": 2R@    R> R> 2DUP >R >R SWAP ;",

        ": 1+  1 + ;",
        ": 1-  1 - ;",
        ": +!  DUP >R @ + R> ! ;",

        ": CELL+  1 CELLS + ;",
        ": CHAR+  1+ ;",
        ": CHARS  ;",

        ": <>   = INVERT ;",
        ": 0<   0 < ;",
        ": 0>   0 > ;",
        ": 0=   0 = ;",
        ": 0<>  0= INVERT ;",

        ": 2!  SWAP OVER ! CELL+ ! ;",

        ": 2*  1 LSHIFT ;",
        ": 2/  1 RSHIFT ;",

        ": VARIABLE  CREATE 0 , ;",
        ": ?         @ . ;",

        ": CONSTANT   CREATE ,    DOES>  @ ;",
        ": 2CONSTANT  CREATE , ,  DOES>  DUP CELL+ @ SWAP @ ;",

        ": DECIMAL  10 BASE ! ;",
        ": HEX      16 BASE ! ;",

        ": '        BL WORD FIND DROP ;",
        ": POSTPONE ' , ; IMMEDIATE",
        ": [']      ' POSTPONE LITERAL ; IMMEDIATE",

        ": CHAR    BL WORD CHAR+ C@ ;",
        ": [CHAR]  CHAR POSTPONE LITERAL ; IMMEDIATE",
    };
    static std::size_t lineCount = sizeof(lines) / sizeof(lines[0]);
    for (std::size_t i = 0; i < lineCount; ++i) {
        auto line = lines[i];
        auto length = std::strlen(line);
        // std::cerr << "Init: " << line << std::endl;
        push(CELL(line));
        push(CELL(length));
        evaluate();
    }
}

void initializeDefinitions() {
    definitions.clear();
    definePrimitives();
    defineBuiltins();
}

} // end anonymous namespace

const char* cxxforthVersion = "1.0.0";

extern "C" void cxxforthReset() {

    std::memset(dStack, 0, sizeof(dStack));
    dTop = dStack - 1;

    std::memset(rStack, 0, sizeof(rStack));
    rTop = rStack - 1;

    std::memset(dataSpace, 0, sizeof(dataSpace));
    dataPointer = dataSpace;

    initializeDefinitions();
}

extern "C" int cxxforthRun(int argc, const char** argv) {
    try {
        commandLineArgCount = static_cast<size_t>(argc);
        commandLineArgVector = argv;

        cxxforthReset();

        auto quit = findDefinition("QUIT");
        if (!quit)
            throw std::runtime_error("QUIT not defined");
        quit->execute();

        return 0;
    }
    catch (const std::exception& ex) {
        std::cerr << "cxxforth: " << ex.what() << std::endl;
        return -1;
    }
}

/****

Finally we have our `main()`. If there are no command-line arguments, it prints
a banner and help message. Then it calls `cxxforthRun()`.

You can define the macro `CXXFORTH_NO_MAIN` to inhibit generation of `main()`.
This is useful for incorporating `cxxforth.cpp` into another application or
library.

****/

#ifndef CXXFORTH_NO_MAIN

int main(int argc, const char** argv) {
    if (argc == 1) {
        std::cout << "cxxforth "
                  << cxxforthVersion
                  << " <https://bitbucket.org/KristopherJohnson/cxxforth>\n"
                  << "Type \"bye\" to exit." << std::endl;
    }

    return cxxforthRun(argc, argv);
}

#endif // CXXFORTH_NO_MAIN

