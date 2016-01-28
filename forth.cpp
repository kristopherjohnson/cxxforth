#include "forth.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <vector>


namespace {

#ifndef DICTIONARY_CELL_COUNT
#define DICTIONARY_CELL_COUNT (16 * 1024)
#endif

#ifdef CONFIG_SKIP_RUNTIME_CHECKS
#define RUNTIME_ERROR(msg)            do { } while (0)
#define RUNTIME_ERROR_IF(cond, msg)   do { } while (0)
#define REQUIRE_STACK_DEPTH(n, name)  do { } while (0)
#define REQUIRE_ALIGNED(addr, name)   do { } while (0)
#define REQUIRE_VALID_HERE(name)      do { } while (0)
#else
#define RUNTIME_ERROR(msg)            do { throw std::runtime_error(msg); } while (0)
#define RUNTIME_ERROR_IF(cond, msg)   do { if (cond) RUNTIME_ERROR(msg); } while (0)
#define REQUIRE_STACK_DEPTH(n, name)  requireStackDepth(n, name)
#define REQUIRE_ALIGNED(addr, name)   checkAligned(addr, name)
#define REQUIRE_VALID_HERE(name)      checkValidHere(name)
#endif

using Cell = uintptr_t;
using SCell = intptr_t;

using Addr = void*;
using CAddr = char*;
using AAddr = Cell*;

#define CELL(x)  reinterpret_cast<Cell>(x)
#define ADDR(x)  reinterpret_cast<Addr>(x)
#define CADDR(x) reinterpret_cast<char*>(x)
#define AADDR(x) reinterpret_cast<AAddr>(x)

using CellStack = std::vector<Cell>;
using Dictionary = std::vector<Cell>;

constexpr auto CellSize = sizeof(Cell);

constexpr auto True = static_cast<Cell>(-1);
constexpr auto False = static_cast<Cell>(0);

CellStack dStack;
CellStack rStack;
Dictionary dictionary(DICTIONARY_CELL_COUNT);
auto dataPointer = ADDR(&dictionary[0]);

/*

    Runtime checks

*/

// Used by REQUIRE_ALIGNED()
template<typename T>
void checkAligned(T addr, const char* name) {
    RUNTIME_ERROR_IF((CELL(addr) % CellSize) != 0, std::string(name) + ": unaligned address");
}

// Used by REQUIRE_STACK_DEPTH()
void requireStackDepth(int n, const char* name) {
    RUNTIME_ERROR_IF(dStack.size() < size_t(n), std::string(name) + ": stack underflow");
}

// Used by REQUIRE_VALID_HERE()
void checkValidHere(const char* name) {
    auto lowLimit = &dictionary[0];
    auto highLimit = &dictionary[dictionary.size()];
    RUNTIME_ERROR_IF(dataPointer < lowLimit || highLimit <= dataPointer,
                     std::string(name) + ": HERE outside dictionary space");
}

/*
 
    Dictionary data-space

*/

template<typename T>
Addr alignAddress(T addr) {
    auto offset = CELL(addr) % CellSize;
    if (offset == 0) {
        return ADDR(addr);
    }
    return CADDR(addr) + (CellSize - offset);
}

// ALIGN ( -- )
void align() {
    dataPointer = alignAddress(dataPointer);
    REQUIRE_VALID_HERE("ALIGN");
}

// ALIGNED ( addr -- a-addr )
void aligned() {
    REQUIRE_STACK_DEPTH(1, "ALIGNED");
    dStack.back() = CELL(alignAddress(dStack.back()));
}

// HERE ( -- addr )
void here() {
    dStack.push_back(CELL(dataPointer));
}

typedef void (*Code)(Addr codeFieldAddress);

struct DictionaryEntry {
    DictionaryEntry* nextEntry;
    unsigned char    flags;
    unsigned char    nameLength;
    char             nameStart;

    const char* name() {
        return &nameStart;
    }

    Addr codeFieldAddress() {
        return alignAddress(CADDR(&nameStart) + nameLength + 1);
    }

    void setCodeField(Code code) {
        *AADDR(codeFieldAddress()) = CELL(code);
    }

    Code code() {
        return reinterpret_cast<Code>(*AADDR(codeFieldAddress()));
    }

    Addr dataFieldAddress() {
        return CADDR(codeFieldAddress()) + CellSize;
    }
};

void doCreate(Addr codeFieldAddress) {
    dStack.push_back(CELL(CADDR(codeFieldAddress) + CellSize));
}

void doConstant(Addr codeFieldAddress) {
    // TODO
}

void doColon(Addr codeFieldAddress) {
    // TODO
}

/*

    Memory access primitives

*/

// ! ( x a-addr -- )
void store() {
    REQUIRE_STACK_DEPTH(2, "!");
    auto aaddr = AADDR(dStack.back());
    REQUIRE_ALIGNED(aaddr, "!");
    dStack.pop_back();
    auto x = dStack.back();
    dStack.pop_back();
    *aaddr = x;
}

// @ ( a-addr -- x )
void fetch() {
    REQUIRE_STACK_DEPTH(1, "@");
    auto aaddr = AADDR(dStack.back());
    REQUIRE_ALIGNED(aaddr, "@");
    dStack.back() = *aaddr;
}

// c! ( char c-addr -- )
void cstore() {
    REQUIRE_STACK_DEPTH(2, "C!");
    auto caddr = CADDR(dStack.back());
    dStack.pop_back();
    auto x = dStack.back();
    dStack.pop_back();
    *caddr = static_cast<char>(x);
}

// c@ ( c-addr -- char )
void cfetch() {
    REQUIRE_STACK_DEPTH(1, "C@");
    auto caddr = CADDR(dStack.back());
    REQUIRE_ALIGNED(caddr, "C@");
    dStack.back() = static_cast<Cell>(*caddr);
}

/*

    Stack manipulation primitives

*/

// DROP ( x -- )
void drop() {
    REQUIRE_STACK_DEPTH(1, "DROP");
    dStack.pop_back();
}

// DUP ( x -- x x )
void dup() {
    REQUIRE_STACK_DEPTH(1, "DUP");
    dStack.push_back(dStack.back());
}

// OVER ( x1 x2 -- x1 x2 x1 )
void over() {
    REQUIRE_STACK_DEPTH(2, "OVER");
    dStack.push_back(dStack[dStack.size() - 2]);
}

// SWAP ( x1 x2 -- x2 x1 )
void swap() {
    REQUIRE_STACK_DEPTH(2, "SWAP");
    auto size = dStack.size();
    std::swap(dStack[size - 1], dStack[size - 2]);
}

// ?DUP ( x -- 0 | x x )
void qdup() {
    REQUIRE_STACK_DEPTH(1, "?DUP");
    auto top = dStack.back();
    if (top != 0) {
        dStack.push_back(top);
    }
}

/*

    I/O primitives

*/

// EMIT ( x -- )
void emit() {
    REQUIRE_STACK_DEPTH(1, "EMIT");
    auto cell = dStack.back();
    dStack.pop_back();
    std::cout.put(static_cast<char>(cell));
}

// KEY ( -- x )
void key() {
    dStack.push_back(static_cast<Cell>(std::cin.get()));
}

/*

    Arithmetic primitives

*/

// + ( n1 n2 -- n3 )
void plus() {
    REQUIRE_STACK_DEPTH(2, "+");
    auto n2 = static_cast<SCell>(dStack.back());
    dStack.pop_back();
    auto n1 = static_cast<SCell>(dStack.back());
    dStack.back() = static_cast<Cell>(n1 + n2);
}

// - ( n1 n2 -- n3 )
void minus() {
    REQUIRE_STACK_DEPTH(2, "-");
    auto n2 = static_cast<SCell>(dStack.back());
    dStack.pop_back();
    auto n1 = static_cast<SCell>(dStack.back());
    dStack.back() = static_cast<Cell>(n1 - n2);
}

// * ( n1 n2 -- n3 )
void star() {
    REQUIRE_STACK_DEPTH(2, "*");
    auto n2 = static_cast<SCell>(dStack.back());
    dStack.pop_back();
    auto n1 = static_cast<SCell>(dStack.back());
    dStack.back() = static_cast<Cell>(n1 * n2);
}

// / ( n1 n2 -- n3 )
void slash() {
    REQUIRE_STACK_DEPTH(2, "/");
    auto n2 = static_cast<SCell>(dStack.back());
    dStack.pop_back();
    auto n1 = static_cast<SCell>(dStack.back());
    RUNTIME_ERROR_IF(n2 == 0, "/: zero divisor");
    dStack.back() = static_cast<Cell>(n1 / n2);
}

// /MOD ( n1 n2 -- n3 n4 )
void slashMod() {
    REQUIRE_STACK_DEPTH(2, "/MOD");
    auto n2 = static_cast<SCell>(dStack.back());
    auto n1 = static_cast<SCell>(dStack[dStack.size() - 2]);
    RUNTIME_ERROR_IF(n2 == 0, "/MOD: zero divisor");
    auto result = std::ldiv(n1, n2);
    dStack[dStack.size() - 2] = static_cast<Cell>(result.rem);
    dStack.back() = static_cast<Cell>(result.quot);
}

/*

    Logical primitives

*/

// = ( x1 x2 -- flag )
void equals() {
    REQUIRE_STACK_DEPTH(2, "=");
    auto n2 = dStack.back();
    dStack.pop_back();
    dStack.back() = dStack.back() == n2 ? True : False;
}

// < ( n1 n2 -- flag )
void lessThan() {
    REQUIRE_STACK_DEPTH(2, "<");
    auto n2 = static_cast<SCell>(dStack.back());
    dStack.pop_back();
    dStack.back() = static_cast<SCell>(dStack.back()) < n2 ? True : False;
}

// > ( n1 n2 -- flag )
void greaterThan() {
    REQUIRE_STACK_DEPTH(2, ">");
    auto n2 = static_cast<SCell>(dStack.back());
    dStack.pop_back();
    dStack.back() = static_cast<SCell>(dStack.back()) > n2 ? True : False;
}

} // end anonymous namespace

extern "C" void resetForth() {
    dStack.clear();
    rStack.clear();
    dictionary.clear();
}

extern "C" int runForth(int argc, const char** argv) {
    try {
        return 0;
    }
    catch (const std::exception& ex) {
        std::cerr << "Exception: " << ex.what() << std::endl;
        return -1;
    }
}

