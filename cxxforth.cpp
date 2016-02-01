#include "cxxforth.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>


namespace {

#ifndef CXXFORTH_DATA_SIZE
#define CXXFORTH_DATA_SIZE (64 * 1024)
#endif

using Cell = uintptr_t;
using SCell = intptr_t;

using Char = unsigned char;
using SChar = signed char;

using CAddr = Char*;
using AAddr = Cell*;

#define CELL(x)  reinterpret_cast<Cell>(x)
#define CADDR(x) reinterpret_cast<Char*>(x)
#define AADDR(x) reinterpret_cast<AAddr>(x)

using CellStack = std::vector<Cell>;

constexpr auto CellSize = sizeof(Cell);

constexpr auto True = static_cast<Cell>(-1);
constexpr auto False = static_cast<Cell>(0);

using Code = void(*)();

struct Word {
    Code        code = nullptr;
    AAddr       parameters = nullptr;
    std::string name;
    Char        flags = 0;
};

using Dictionary = std::vector<Word>;
using DataSpace = std::vector<Char>;

CellStack  dStack;
CellStack  rStack;
DataSpace  dataSpace;
Dictionary dictionary;

CAddr dataPointer = nullptr;
CAddr instructionPointer = nullptr;

size_t commandLineArgCount = 0;
const char** commandLineArgVector = nullptr;

/*

    Runtime checks

*/

#ifdef CXXFORTH_SKIP_RUNTIME_CHECKS

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

template<typename T>
void checkAligned(T addr, const char* name) {
    RUNTIME_ERROR_IF((CELL(addr) % CellSize) != 0, std::string(name) + ": unaligned address");
}

void requireStackDepth(size_t n, const char* name) {
    RUNTIME_ERROR_IF(dStack.size() < n, std::string(name) + ": stack underflow");
}

void checkValidHere(const char* name) {
    auto lowLimit = &dataSpace[0];
    auto highLimit = &dataSpace[dataSpace.size()];
    RUNTIME_ERROR_IF(dataPointer < lowLimit || highLimit <= dataPointer,
                     std::string(name) + ": HERE outside data space");
}

#endif // CXXFORTH_SKIP_RUNTIME_CHECKS

/*

    Stack manipulation primitives

*/

// Push cell onto data stack.
void push(Cell x) {
    dStack.push_back(x);
}

// Pop cell from data stack.
void pop() {
    dStack.pop_back();
}

// Get reference to the top-of-stack cell.
Cell& topOfStack() {
    return dStack.back();
}

// DROP ( x -- )
void drop() {
    REQUIRE_STACK_DEPTH(1, "DROP");
    pop();
}

// DUP ( x -- x x )
void dup() {
    REQUIRE_STACK_DEPTH(1, "DUP");
    push(topOfStack());
}

// OVER ( x1 x2 -- x1 x2 x1 )
void over() {
    REQUIRE_STACK_DEPTH(2, "OVER");
    push(dStack[dStack.size() - 2]);
}

// SWAP ( x1 x2 -- x2 x1 )
void swap() {
    REQUIRE_STACK_DEPTH(2, "SWAP");
    auto size = dStack.size();
    std::swap(dStack[size - 1], dStack[size - 2]);
}

// ROT ( x1 x2 x3 -- x2 x3 x1 )
void rot() {
    REQUIRE_STACK_DEPTH(3, "ROT");
    auto size = dStack.size();
    auto x1 = dStack[size - 3];
    auto x2 = dStack[size - 2];
    auto x3 = dStack[size - 1];
    dStack[size - 2] = x3;
    dStack[size - 3] = x2;
    dStack[size - 1] = x1;
}

// PICK ( xu ... x1 x0 u -- xu ... x1 x0 xu )
void pick() {
    REQUIRE_STACK_DEPTH(1, "PICK");
    auto n = topOfStack();
    REQUIRE_STACK_DEPTH(n + 2, "PICK");
    topOfStack() = dStack[dStack.size() - 2 - n];
}

// ROLL ( xu xu-1 ... x0 u -- xu-1 ... x0 xu )
void roll() {
    REQUIRE_STACK_DEPTH(1, "ROLL");
    auto n = topOfStack();
    pop();
    if (n > 0) {
        auto size = dStack.size();
        auto x = dStack[size - 1 - n];
        std::memmove(&dStack[size - 1 - n], &dStack[size - n], n * sizeof(Cell));
        topOfStack() = x;
    }
}

// ?DUP ( x -- 0 | x x )
void qdup() {
    REQUIRE_STACK_DEPTH(1, "?DUP");
    Cell top = topOfStack();
    if (top != 0) {
        push(top);
    }
}

// >R ( x -- ) ( R:  -- x )
void toR() {
    REQUIRE_STACK_DEPTH(1, ">R");
    rStack.push_back(topOfStack());
    pop();
}

// R> ( -- x ) ( R: x -- )
void rFrom() {
    RUNTIME_ERROR_IF(rStack.size() < 1, "R>: return stack underflow");
    push(rStack.back());
    rStack.pop_back();
}

// R@ ( -- x ) ( R: x -- x )
void rFetch() {
    RUNTIME_ERROR_IF(rStack.size() < 1, "R@: return stack underflow");
    push(rStack.back());
}

/*
 
    Dictionary data-space

*/

template<typename T>
AAddr alignAddress(T addr) {
    auto offset = CELL(addr) % CellSize;
    if (offset == 0) {
        return AADDR(addr);
    }
    return AADDR(CADDR(addr) + (CellSize - offset));
}

// ALIGN ( -- )
void align() {
    dataPointer = CADDR(alignAddress(dataPointer));
    REQUIRE_VALID_HERE("ALIGN");
}

// ALIGNED ( addr -- a-addr )
void aligned() {
    REQUIRE_STACK_DEPTH(1, "ALIGNED");
    topOfStack() = CELL(alignAddress(topOfStack()));
}

// HERE ( -- addr )
void here() {
    push(CELL(dataPointer));
}

// ALLOT ( n -- )
void allot() {
    REQUIRE_STACK_DEPTH(1, "ALLOT");
    dataPointer += topOfStack();
    REQUIRE_VALID_HERE("ALLOT");
    pop();
}

// CELL+ ( a-addr1 -- a-addr2 )
void cellPlus() {
    REQUIRE_STACK_DEPTH(1, "CELL+");
    topOfStack() += CellSize;
}

// CELLS ( n1 -- n2 )
void cells() {
    REQUIRE_STACK_DEPTH(1, "CELLS");
    topOfStack() *= CellSize;
}

// CHAR+ ( c-addr1 -- c-addr2 )
void charPlus() {
    REQUIRE_STACK_DEPTH(1, "CHAR+");
    ++topOfStack();
}

// Store a cell to data space.
void data(Cell x) {
    REQUIRE_VALID_HERE(",");
    REQUIRE_ALIGNED(dataPointer, ",");
    *(AADDR(dataPointer)) = x;
    dataPointer += CellSize;
}

// , ( x -- )
void comma() {
    REQUIRE_STACK_DEPTH(1, ",");
    data(topOfStack());
    pop();
}

// Store a char to data space.
void cdata(Cell x) {
    REQUIRE_VALID_HERE("C,");
    *dataPointer = static_cast<Char>(x);
    dataPointer += 1;
}

// C, ( char -- )
void ccomma() {
    REQUIRE_STACK_DEPTH(1, "C,");
    cdata(topOfStack());
    pop();
}

/*

    Memory access primitives

*/

// ! ( x a-addr -- )
void store() {
    REQUIRE_STACK_DEPTH(2, "!");
    auto aaddr = AADDR(topOfStack());
    REQUIRE_ALIGNED(aaddr, "!");
    pop();
    auto x = topOfStack();
    pop();
    *aaddr = x;
}

// @ ( a-addr -- x )
void fetch() {
    REQUIRE_STACK_DEPTH(1, "@");
    auto aaddr = AADDR(topOfStack());
    REQUIRE_ALIGNED(aaddr, "@");
    topOfStack() = *aaddr;
}

// c! ( char c-addr -- )
void cstore() {
    REQUIRE_STACK_DEPTH(2, "C!");
    auto caddr = CADDR(topOfStack());
    pop();
    auto x = topOfStack();
    pop();
    *caddr = static_cast<Char>(x);
}

// c@ ( c-addr -- char )
void cfetch() {
    REQUIRE_STACK_DEPTH(1, "C@");
    auto caddr = CADDR(topOfStack());
    REQUIRE_ALIGNED(caddr, "C@");
    topOfStack() = static_cast<Cell>(*caddr);
}

/*

    I/O primitives

*/

// EMIT ( x -- )
void emit() {
    REQUIRE_STACK_DEPTH(1, "EMIT");
    auto cell = topOfStack();
    pop();
    std::cout.put(static_cast<char>(cell));
}

// KEY ( -- x )
void key() {
    push(static_cast<Cell>(std::cin.get()));
}

/*

    Arithmetic primitives

*/

// + ( n1 n2 -- n3 )
void plus() {
    REQUIRE_STACK_DEPTH(2, "+");
    auto n2 = static_cast<SCell>(topOfStack());
    pop();
    auto n1 = static_cast<SCell>(topOfStack());
    topOfStack() = static_cast<Cell>(n1 + n2);
}

// - ( n1 n2 -- n3 )
void minus() {
    REQUIRE_STACK_DEPTH(2, "-");
    auto n2 = static_cast<SCell>(topOfStack());
    pop();
    auto n1 = static_cast<SCell>(topOfStack());
    topOfStack() = static_cast<Cell>(n1 - n2);
}

// * ( n1 n2 -- n3 )
void star() {
    REQUIRE_STACK_DEPTH(2, "*");
    auto n2 = static_cast<SCell>(topOfStack());
    pop();
    auto n1 = static_cast<SCell>(topOfStack());
    topOfStack() = static_cast<Cell>(n1 * n2);
}

// / ( n1 n2 -- n3 )
void slash() {
    REQUIRE_STACK_DEPTH(2, "/");
    auto n2 = static_cast<SCell>(topOfStack());
    pop();
    auto n1 = static_cast<SCell>(topOfStack());
    RUNTIME_ERROR_IF(n2 == 0, "/: zero divisor");
    topOfStack() = static_cast<Cell>(n1 / n2);
}

// /MOD ( n1 n2 -- n3 n4 )
void slashMod() {
    REQUIRE_STACK_DEPTH(2, "/MOD");
    auto n2 = static_cast<SCell>(topOfStack());
    auto n1 = static_cast<SCell>(dStack[dStack.size() - 2]);
    RUNTIME_ERROR_IF(n2 == 0, "/MOD: zero divisor");
    auto result = std::ldiv(n1, n2);
    dStack[dStack.size() - 2] = static_cast<Cell>(result.rem);
    topOfStack() = static_cast<Cell>(result.quot);
}

// NEGATE ( n1 -- n2 )
void negate() {
    REQUIRE_STACK_DEPTH(1, "NEGATE");
    topOfStack() = static_cast<Cell>(-static_cast<SCell>(topOfStack()));
}

/*

    Logical and relational primitives

*/

// AND ( x1 x2 -- x3 )
void bitwiseAnd() {
    REQUIRE_STACK_DEPTH(2, "AND");
    auto x2 = topOfStack();
    pop();
    topOfStack() = topOfStack() & x2;
}

// OR ( x1 x2 -- x3 )
void bitwiseOr() {
    REQUIRE_STACK_DEPTH(2, "OR");
    auto x2 = topOfStack();
    pop();
    topOfStack() = topOfStack() | x2;
}

// XOR ( x1 x2 -- x3 )
void bitwiseXor() {
    REQUIRE_STACK_DEPTH(2, "XOR");
    auto x2 = topOfStack();
    pop();
    topOfStack() = topOfStack() ^ x2;
}

// INVERT ( x1 -- x2 )
void invert() {
    REQUIRE_STACK_DEPTH(1, "INVERT");
    topOfStack() = ~topOfStack();
}

// LSHIFT ( x1 u -- x2 )
void lshift() {
    REQUIRE_STACK_DEPTH(2, "LSHIFT");
    auto n = topOfStack();
    pop();
    topOfStack() <<= n;
}

// RSHIFT ( x1 u -- x2 )
void rshift() {
    REQUIRE_STACK_DEPTH(2, "RSHIFT");
    auto n = topOfStack();
    pop();
    topOfStack() >>= n;
}

// = ( x1 x2 -- flag )
void equals() {
    REQUIRE_STACK_DEPTH(2, "=");
    auto n2 = topOfStack();
    pop();
    topOfStack() = topOfStack() == n2 ? True : False;
}

// < ( n1 n2 -- flag )
void lessThan() {
    REQUIRE_STACK_DEPTH(2, "<");
    auto n2 = static_cast<SCell>(topOfStack());
    pop();
    topOfStack() = static_cast<SCell>(topOfStack()) < n2 ? True : False;
}

// > ( n1 n2 -- flag )
void greaterThan() {
    REQUIRE_STACK_DEPTH(2, ">");
    auto n2 = static_cast<SCell>(topOfStack());
    pop();
    topOfStack() = static_cast<SCell>(topOfStack()) > n2 ? True : False;
}

/*

    Environmental primitives

*/

// #ARG ( -- n )
// Provide count of command-line arguments.
// Not an ANS Forth word.
void argCount() {
    push(commandLineArgCount);
}

// ARG ( n -- c-addr u )
// Provide the Nth command-line argument.
// Not an ANS Forth word.`
void argAtIndex() {
    auto index = static_cast<size_t>(topOfStack());
    RUNTIME_ERROR_IF(index >= commandLineArgCount, "ARG: invalid index");
    auto value = commandLineArgVector[index];
    topOfStack() = CELL(value);
    push(std::strlen(value));
}

/*

    Other system words

*/

// BYE ( -- )
void bye() {
    std::exit(EXIT_SUCCESS);
}

/*

    Dictionary

*/

void doCreate() {
    // TODO
}

void doConstant() {
    // TODO
}

void doColon() {
    // TODO
}

void defineCode(const char* name, Code code) {
    Word word;
    word.code = code;
    word.name = name;

    align();
    word.parameters = AADDR(dataPointer);

    dictionary.emplace_back(std::move(word));
}

bool doNamesMatch(CAddr name1, CAddr name2, Cell nameLength) {
    for (size_t i = 0; i < nameLength; ++i) {
        if (std::toupper(name1[i]) != std::toupper(name2[i])) {
            return false;
        }
    }
    return true;
}

Word* findWord(CAddr nameToFind, Cell nameLength) {
    for (auto i = dictionary.rbegin(); i != dictionary.rend(); ++i) {
        auto& word = *i;
        auto& wordName = word.name;
        if (wordName.length() == nameLength) {
            auto wordNameCAddr = CADDR(const_cast<char*>(wordName.c_str()));
            if (doNamesMatch(nameToFind, wordNameCAddr, nameLength)) {
                return &word;
            }
        }
    }
    return nullptr;
}

void words() {
    for (auto i = dictionary.rbegin(); i != dictionary.rend(); ++i) {
        std::cout << i->name << " ";
    }
}

void initializeDictionary() {
    dictionary.clear();
    dictionary.reserve(128);

    static struct { const char* name; Code code; } primitives[] = {
        {"!",       store},
        {"#ARG",    argCount},
        {"*",       star},
        {"+",       plus},
        {",",       comma},
        {"-",       minus},
        {"/",       slash},
        {"/MOD",    slashMod},
        {"<",       lessThan},
        {"=",       equals},
        {">",       greaterThan},
        {">R",      toR},
        {"?DUP",    qdup},
        {"@",       fetch},
        {"ALIGN",   align},
        {"ALIGNED", aligned},
        {"ALLOT",   allot},
        {"AND",     bitwiseAnd},
        {"ARG",     argAtIndex},
        {"BYE",     bye},
        {"C!",      cstore},
        {"C,",      ccomma},
        {"C@",      cfetch},
        {"CELL+",   cellPlus},
        {"CELLS",   cells},
        {"CHAR+",   charPlus},
        {"DROP",    drop},
        {"DUP",     dup},
        {"EMIT",    emit},
        {"HERE",    here},
        {"INVERT",  invert},
        {"KEY",     key},
        {"LSHIFT",  lshift},
        {"NEGATE",  negate},
        {"OR",      bitwiseOr},
        {"OVER",    over},
        {"PICK",    pick},
        {"R>",      rFrom},
        {"R@",      rFetch},
        {"ROLL",    roll},
        {"ROT",     rot},
        {"RSHIFT",  rshift},
        {"SWAP",    swap},
        {"WORDS",   words},
        {"XOR",     bitwiseXor}
    };
    for (auto& p: primitives) {
        defineCode(p.name, p.code);
    }
}

} // end anonymous namespace

extern "C" void resetForth() {
    dStack.clear();
    dStack.reserve(64);

    rStack.clear();
    rStack.reserve(64);

    dataSpace.clear();
    dataSpace.resize(CXXFORTH_DATA_SIZE);
    dataPointer = &dataSpace[0];

    initializeDictionary();
}

extern "C" int runForth(int argc, const char** argv) {
    try {
        commandLineArgCount = static_cast<size_t>(argc);
        commandLineArgVector = argv;
        resetForth();
        return 0;
    }
    catch (const std::exception& ex) {
        std::cerr << "Exception: " << ex.what() << std::endl;
        return -1;
    }
}

#ifndef CXXFORTH_NO_MAIN
int main(int argc, const char** argv) {
    return runForth(argc, argv);
}
#endif


