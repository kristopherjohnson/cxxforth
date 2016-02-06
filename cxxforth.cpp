#include "cxxforth.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>


namespace {

#ifndef CXXFORTH_DATA_SIZE
#define CXXFORTH_DATA_SIZE (64 * 1024)
#endif

#ifndef CXXFORTH_RSTACK_COUNT
#define CXXFORTH_SRTACK_COUNT (256)
#endif

#ifndef CXXFORTH_RSTACK_COUNT
#define CXXFORTH_RSTACK_COUNT (256)
#endif

#ifndef CXXFORTH_DICTIONARY_COUNT
#define CXXFORTH_DICTIONARY_COUNT (4096)
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

constexpr auto CellSize = sizeof(Cell);

constexpr auto True = static_cast<Cell>(-1);
constexpr auto False = static_cast<Cell>(0);

using Code = void(*)();

struct Word {
    Code        code      = nullptr;
    AAddr       parameter = nullptr;
    Cell        flags     = 0;   
    std::string name;

    enum {
        FlagImmediate = 1,
        FlagHidden = 2
    };

    const char* nameAddress() const {
        return name.c_str();
    }

    std::size_t nameLength() const {
        return name.length();
    }

    bool isHidden() const {
        return (flags & FlagImmediate) != 0;
    }

    bool isImmediate() const {
        return (flags & FlagHidden) != 0;
    }

    void reset() {
        code = nullptr;
        parameter = nullptr;
        flags = 0;
        name.clear();
    }
};

Cell dStack[CXXFORTH_DSTACK_COUNT];
Cell rStack[CXXFORTH_RSTACK_COUNT];
Word dictionary[CXXFORTH_DICTIONARY_COUNT];
Char dataSpace[CXXFORTH_DATASPACE_SIZE];

constexpr AAddr dStackLimit     = &dStack[CXXFORTH_DSTACK_COUNT];
constexpr AAddr rStackLimit     = &rStack[CXXFORTH_RSTACK_COUNT];
constexpr Word* dictionaryLimit = &dictionary[CXXFORTH_DICTIONARY_COUNT];
constexpr CAddr dataSpaceLimit  = &dataSpace[CXXFORTH_DATASPACE_SIZE];

AAddr dTop = nullptr;
AAddr rTop = nullptr;
Word* latestDefinition = nullptr;
CAddr dataPointer = nullptr;

Word* currentWord = nullptr;
Word* nextWord = nullptr;

Cell isCompiling = True;

size_t commandLineArgCount = 0;
const char** commandLineArgVector = nullptr;


/*

    Runtime checks

*/

std::ptrdiff_t dStackDepth() {
    return dTop - dStack + 1;
}

std::ptrdiff_t rStackDepth() {
    return rTop - rStack + 1;
}

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

#define RUNTIME_ERROR(msg)                   do { throw std::runtime_error(msg); } while (0)
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

/*

    Stack manipulation primitives

*/

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

/*

    Interpreter

*/

void next() {
    currentWord = nextWord;
    ++nextWord;
}

void doColon() {
    rpush(CELL(nextWord));
    nextWord = reinterpret_cast<Word*>(currentWord->parameter);
}

// EXIT ( -- ) ( R: nest-sys -- )
void exit() {
    nextWord = reinterpret_cast<Word*>(*rTop);
    rpop();
    next();
}

// LIT ( -- x )
// Compiled by LITERAL.
// Not an ANS Forth word.
void lit() {
    push(CELL(nextWord));
    ++nextWord;
    next();
}

/*

    Stack manipulation words

*/

// DROP ( x -- )
void drop() {
    REQUIRE_DSTACK_DEPTH(1, "DROP");
    pop();
    next();
}

// DUP ( x -- x x )
void dup() {
    REQUIRE_DSTACK_DEPTH(1, "DUP");
    REQUIRE_DSTACK_AVAILABLE(1, "DUP");
    push(*dTop);
    next();
}

// OVER ( x1 x2 -- x1 x2 x1 )
void over() {
    REQUIRE_DSTACK_DEPTH(2, "OVER");
    REQUIRE_DSTACK_AVAILABLE(1, "OVER");
    push(*(dTop - 1));
    next();
}

// SWAP ( x1 x2 -- x2 x1 )
void swap() {
    REQUIRE_DSTACK_DEPTH(2, "SWAP");
    auto temp = *dTop;
    *dTop = *(dTop - 1);
    *(dTop - 1) = temp;
    next();
}

// ROT ( x1 x2 x3 -- x2 x3 x1 )
void rot() {
    REQUIRE_DSTACK_DEPTH(3, "ROT");
    auto x1 = *(dTop - 2);
    auto x2 = *(dTop - 1);
    auto x3 = *dTop;
    *dTop = x1;
    *(dTop - 1) = x3;
    *(dTop - 2) = x2;
    next();
}

// PICK ( xu ... x1 x0 u -- xu ... x1 x0 xu )
void pick() {
    REQUIRE_DSTACK_DEPTH(1, "PICK");
    auto index = *dTop;
    REQUIRE_DSTACK_DEPTH(index + 2, "PICK");
    *dTop = *(dTop - index - 1);
    next();
}

// ROLL ( xu xu-1 ... x0 u -- xu-1 ... x0 xu )
void roll() {
    REQUIRE_DSTACK_DEPTH(1, "ROLL");
    auto index = *dTop;
    pop();
    if (index > 0) {
        REQUIRE_DSTACK_DEPTH(index + 1, "ROLL");
        auto x = *(dTop - index - 1);
        std::memmove(dTop - index - 1, dTop - index, index * sizeof(Cell));
        *dTop = x;
    }
    next();
}

// ?DUP ( x -- 0 | x x )
void qdup() {
    REQUIRE_DSTACK_DEPTH(1, "?DUP");
    Cell top = *dTop;
    if (top != 0) {
        REQUIRE_DSTACK_AVAILABLE(1, "?DUP");
        push(top);
    }
    next();
}

// >R ( x -- ) ( R:  -- x )
void toR() {
    REQUIRE_DSTACK_DEPTH(1, ">R");
    REQUIRE_RSTACK_AVAILABLE(1, ">R");
    rpush(*dTop);
    pop();
    next();
}

// R> ( -- x ) ( R: x -- )
void rFrom() {
    REQUIRE_RSTACK_DEPTH(1, "R>");
    REQUIRE_DSTACK_AVAILABLE(1, "R>");
    push(*rTop);
    rpop();
    next();
}

// R@ ( -- x ) ( R: x -- x )
void rFetch() {
    REQUIRE_RSTACK_DEPTH(1, "R@");
    REQUIRE_DSTACK_AVAILABLE(1, "R@");
    push(*rTop);
    next();
}

/*
 
    Data space

*/

template<typename T>
AAddr alignAddress(T addr) {
    auto offset = CELL(addr) % CellSize;
    return (offset == 0) ? AADDR(addr) : AADDR(CADDR(addr) + (CellSize - offset));
}

void alignDataPointer() {
    dataPointer = CADDR(alignAddress(dataPointer));
    REQUIRE_VALID_HERE("ALIGN");
}

// ALIGN ( -- )
void align() {
    alignDataPointer();
    next();
}

// ALIGNED ( addr -- a-addr )
void aligned() {
    REQUIRE_DSTACK_DEPTH(1, "ALIGNED");
    *dTop = CELL(alignAddress(*dTop));
    next();
}

// HERE ( -- addr )
void here() {
    REQUIRE_DSTACK_AVAILABLE(1, "HERE");
    push(CELL(dataPointer));
    next();
}

// ALLOT ( n -- )
void allot() {
    REQUIRE_DSTACK_DEPTH(1, "ALLOT");
    REQUIRE_VALID_HERE("ALLOT");
    REQUIRE_DATASPACE_AVAILABLE(CellSize, "ALLOT");
    dataPointer += *dTop;
    pop();
    next();
}

// CELL+ ( a-addr1 -- a-addr2 )
void cellPlus() {
    REQUIRE_DSTACK_DEPTH(1, "CELL+");
    *dTop += CellSize;
    next();
}

// CELLS ( n1 -- n2 )
void cells() {
    REQUIRE_DSTACK_DEPTH(1, "CELLS");
    *dTop *= CellSize;
    next();
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
    REQUIRE_DSTACK_DEPTH(1, ",");
    data(*dTop);
    pop();
    next();
}

// Store a char to data space.
void cdata(Cell x) {
    REQUIRE_VALID_HERE("C,");
    REQUIRE_DATASPACE_AVAILABLE(1, "C,");
    *dataPointer = static_cast<Char>(x);
    dataPointer += 1;
    next();
}

// C, ( char -- )
void ccomma() {
    REQUIRE_DSTACK_DEPTH(1, "C,");
    cdata(*dTop);
    pop();
    next();
}

/*

    Memory access primitives

*/

// ! ( x a-addr -- )
void store() {
    REQUIRE_DSTACK_DEPTH(2, "!");
    auto aaddr = AADDR(*dTop);
    REQUIRE_ALIGNED(aaddr, "!");
    pop();
    auto x = *dTop;
    pop();
    *aaddr = x;
    next();
}

// @ ( a-addr -- x )
void fetch() {
    REQUIRE_DSTACK_DEPTH(1, "@");
    auto aaddr = AADDR(*dTop);
    REQUIRE_ALIGNED(aaddr, "@");
    *dTop = *aaddr;
    next();
}

// c! ( char c-addr -- )
void cstore() {
    REQUIRE_DSTACK_DEPTH(2, "C!");
    auto caddr = CADDR(*dTop);
    pop();
    auto x = *dTop;
    pop();
    *caddr = static_cast<Char>(x);
    next();
}

// c@ ( c-addr -- char )
void cfetch() {
    REQUIRE_DSTACK_DEPTH(1, "C@");
    auto caddr = CADDR(*dTop);
    REQUIRE_ALIGNED(caddr, "C@");
    *dTop = static_cast<Cell>(*caddr);
    next();
}

/*

    I/O primitives

*/

// EMIT ( x -- )
void emit() {
    REQUIRE_DSTACK_DEPTH(1, "EMIT");
    auto cell = *dTop;
    pop();
    std::cout.put(static_cast<char>(cell));
    next();
}

// KEY ( -- x )
void key() {
    REQUIRE_DSTACK_AVAILABLE(1, "KEY");
    push(static_cast<Cell>(std::cin.get()));
    next();
}

/*

    Arithmetic primitives

*/

// + ( n1 n2 -- n3 )
void plus() {
    REQUIRE_DSTACK_DEPTH(2, "+");
    auto n2 = static_cast<SCell>(*dTop);
    pop();
    auto n1 = static_cast<SCell>(*dTop);
    *dTop = static_cast<Cell>(n1 + n2);
    next();
}

// - ( n1 n2 -- n3 )
void minus() {
    REQUIRE_DSTACK_DEPTH(2, "-");
    auto n2 = static_cast<SCell>(*dTop);
    pop();
    auto n1 = static_cast<SCell>(*dTop);
    *dTop = static_cast<Cell>(n1 - n2);
    next();
}

// * ( n1 n2 -- n3 )
void star() {
    REQUIRE_DSTACK_DEPTH(2, "*");
    auto n2 = static_cast<SCell>(*dTop);
    pop();
    auto n1 = static_cast<SCell>(*dTop);
    *dTop = static_cast<Cell>(n1 * n2);
    next();
}

// / ( n1 n2 -- n3 )
void slash() {
    REQUIRE_DSTACK_DEPTH(2, "/");
    auto n2 = static_cast<SCell>(*dTop);
    pop();
    auto n1 = static_cast<SCell>(*dTop);
    RUNTIME_ERROR_IF(n2 == 0, "/: zero divisor");
    *dTop = static_cast<Cell>(n1 / n2);
    next();
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
    next();
}

// NEGATE ( n1 -- n2 )
void negate() {
    REQUIRE_DSTACK_DEPTH(1, "NEGATE");
    *dTop = static_cast<Cell>(-static_cast<SCell>(*dTop));
    next();
}

/*

    Logical and relational primitives

*/

// AND ( x1 x2 -- x3 )
void bitwiseAnd() {
    REQUIRE_DSTACK_DEPTH(2, "AND");
    auto x2 = *dTop;
    pop();
    *dTop = *dTop & x2;
    next();
}

// OR ( x1 x2 -- x3 )
void bitwiseOr() {
    REQUIRE_DSTACK_DEPTH(2, "OR");
    auto x2 = *dTop;
    pop();
    *dTop = *dTop | x2;
    next();
}

// XOR ( x1 x2 -- x3 )
void bitwiseXor() {
    REQUIRE_DSTACK_DEPTH(2, "XOR");
    auto x2 = *dTop;
    pop();
    *dTop = *dTop ^ x2;
    next();
}

// INVERT ( x1 -- x2 )
void invert() {
    REQUIRE_DSTACK_DEPTH(1, "INVERT");
    *dTop = ~*dTop;
    next();
}

// LSHIFT ( x1 u -- x2 )
void lshift() {
    REQUIRE_DSTACK_DEPTH(2, "LSHIFT");
    auto n = *dTop;
    pop();
    *dTop <<= n;
    next();
}

// RSHIFT ( x1 u -- x2 )
void rshift() {
    REQUIRE_DSTACK_DEPTH(2, "RSHIFT");
    auto n = *dTop;
    pop();
    *dTop >>= n;
    next();
}

// = ( x1 x2 -- flag )
void equals() {
    REQUIRE_DSTACK_DEPTH(2, "=");
    auto n2 = *dTop;
    pop();
    *dTop = *dTop == n2 ? True : False;
    next();
}

// < ( n1 n2 -- flag )
void lessThan() {
    REQUIRE_DSTACK_DEPTH(2, "<");
    auto n2 = static_cast<SCell>(*dTop);
    pop();
    *dTop = static_cast<SCell>(*dTop) < n2 ? True : False;
    next();
}

// > ( n1 n2 -- flag )
void greaterThan() {
    REQUIRE_DSTACK_DEPTH(2, ">");
    auto n2 = static_cast<SCell>(*dTop);
    pop();
    *dTop = static_cast<SCell>(*dTop) > n2 ? True : False;
    next();
}

/*

    Environmental primitives

*/

// #ARG ( -- n )
// Provide count of command-line arguments.
// Not an ANS Forth word.
void argCount() {
    REQUIRE_DSTACK_AVAILABLE(1, "#ARG");
    push(commandLineArgCount);
    next();
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
    next();
}

/*

    Other system words

*/

// BYE ( -- )
void bye() {
    std::exit(EXIT_SUCCESS);
}

/* 
 
    Compilation

*/

// STATE ( -- a-addr )
void state() {
    REQUIRE_DSTACK_AVAILABLE(1, "STATE");
    push(CELL(&isCompiling));
    next();
}

/*

    Dictionary

*/

void defineCodeWord(const char* name, Code code) {
    alignDataPointer();

    auto word = ++latestDefinition;
    word->code = code;
    word->parameter = AADDR(dataPointer);
    word->name = name;
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
    for (auto word = latestDefinition; word >= &dictionary[0]; ++word) {
        if (word->isHidden())
            continue;
        auto& wordName = word->name;
        if (wordName.length() == nameLength) {
            auto wordNameCAddr = CADDR(const_cast<char*>(wordName.c_str()));
            if (doNamesMatch(nameToFind, wordNameCAddr, nameLength)) {
                return word;
            }
        }
    }
    return nullptr;
}

// FIND ( c-addr -- c-addr 0  |  xt 1  |  xt -1 )
void find() {
    REQUIRE_DSTACK_DEPTH(1, "FIND");
    REQUIRE_DSTACK_AVAILABLE(1, "FIND");
    auto caddr = CADDR(*dTop);
    auto length = static_cast<Cell>(*caddr);
    auto name = caddr + 1;
    auto word = findWord(name, length);
    if (word == nullptr) {
        push(0);
    }
    else {
        *dTop = CELL(word);
        push(word->isImmediate() ? 1 : Cell(-1));
    }
    next();
}

// WORDS ( -- )
void words() {
    for (auto word = latestDefinition; word >= &dictionary[0]; ++word) {
        std::cout << word->name << " ";
    }
    next();
}

void initializeDictionary() {
    latestDefinition = dictionary - 1;
    for (auto word = dictionary; word < dictionaryLimit; ++word) {
        word->reset();
    }

    static struct { const char* name; Code code; } codeWords[] = {
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
        {"DROP",    drop},
        {"DUP",     dup},
        {"EMIT",    emit},
        {"EXIT",    exit},
        {"FIND",    find},
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
        {"STATE",   state},
        {"SWAP",    swap},
        {"WORDS",   words},
        {"XOR",     bitwiseXor}
    };
    for (auto& w: codeWords) {
        defineCodeWord(w.name, w.code);
    }
}

} // end anonymous namespace

extern "C" void resetForth() {

    dTop = dStack - 1;
    rTop = rStack - 1;

    dataPointer = dataSpace;

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


