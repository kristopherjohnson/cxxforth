#include "forth.hpp"

#include <algorithm>
#include <iostream>
#include <vector>

#ifndef DICTIONARY_CELL_COUNT
#define DICTIONARY_CELL_COUNT (16 * 1024)
#endif

#ifdef SKIP_RUNTIME_CHECKS
#define RUNTIME_ERROR(msg) do { } while (0)
#define RUNTIME_ERROR_IF(cond, msg) do { } while (0)
#define REQUIRE_STACK_DEPTH(n, name) do { } while (0)
#else
#define RUNTIME_ERROR(msg) do { throw std::runtime_error(msg); } while (0)
#define RUNTIME_ERROR_IF(cond, msg) do { if (cond) RUNTIME_ERROR(msg); } while (0)
#define REQUIRE_STACK_DEPTH(n, name) RUNTIME_ERROR_IF(dStack.size() < (n), name ": stack underflow")
#endif

namespace {

using Cell = uintptr_t;
constexpr auto CellSize = sizeof(Cell);

using CellStack = std::vector<Cell>;
using Dictionary = std::vector<Cell>;

typedef void* Addr;
typedef char* CAddr;
typedef Cell* AAddr;

CellStack dStack;
CellStack rStack;
Dictionary dictionary(DICTIONARY_CELL_COUNT);

Addr dataPointer = reinterpret_cast<Addr>(&dictionary[0]);

/*

    Stack manipulation primitives

*/

void DROP() {
    REQUIRE_STACK_DEPTH(1, "DROP");
    dStack.pop_back();
}

void DUP() {
    REQUIRE_STACK_DEPTH(1, "DUP");
    dStack.push_back(dStack.back());
}

void OVER() {
    REQUIRE_STACK_DEPTH(2, "OVER");
    dStack.push_back(dStack[dStack.size() - 2]);
}

void SWAP() {
    REQUIRE_STACK_DEPTH(2, "SWAP");
    auto size = dStack.size();
    std::swap(dStack[size - 1], dStack[size - 2]);
}

void QDUP() {
    REQUIRE_STACK_DEPTH(1, "?DUP");
    auto top = dStack.back();
    if (top != 0) {
        dStack.push_back(top);
    }
}

void DDROP() {
    REQUIRE_STACK_DEPTH(2, "2DROP");
    dStack.pop_back();
    dStack.pop_back();
}

void DDUP() {
    REQUIRE_STACK_DEPTH(2, "2DUP");
    auto size = dStack.size();
    dStack.push_back(dStack[size - 2]);
    dStack.push_back(dStack[size - 1]);
}

void DOVER() {
    REQUIRE_STACK_DEPTH(4, "2OVER");
    auto size = dStack.size();
    dStack.push_back(dStack[size - 4]);
    dStack.push_back(dStack[size - 3]);
}

void DSWAP() {
    REQUIRE_STACK_DEPTH(4, "2SWAP");
    auto size = dStack.size();
    std::swap(dStack[size - 1], dStack[size - 3]);
    std::swap(dStack[size - 2], dStack[size - 4]);
}

/*

    I/O primitives

*/

void EMIT() {
    REQUIRE_STACK_DEPTH(1, "EMIT");
    auto cell = dStack.back();
    dStack.pop_back();
    std::cout.put(static_cast<char>(cell));
}

void KEY() {
    dStack.push_back(static_cast<Cell>(std::cin.get()));
}

/*
 
    Dictionary data-space

*/

void HERE() {
    dStack.push_back(reinterpret_cast<Cell>(dataPointer));
}

} // end anonymous namespace

int runForth(int argc, const char** argv) {
    try {
        return 0;
    }
    catch (const std::exception& ex) {
        std::cerr << "Exception: " << ex.what() << std::endl;
        return -1;
    }
}

