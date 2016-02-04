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
    Code        code      = nullptr;
    AAddr       parameter = nullptr;
    AAddr       extra     = nullptr;
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
};

using Xt = Word*;

using Dictionary = std::vector<Word>;

using DataSpace = std::vector<Char>;

CellStack  dStack;
CellStack  rStack;
DataSpace  dataSpace;
Dictionary dictionary;

CAddr dataPointer = nullptr;

Word* currentWord = nullptr;
Word* nextWord = nullptr;

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

// Push cell onto return stack.
void rpush(Cell x) {
    rStack.push_back(x);
}

// Pop cell from return stack.
void rpop() {
    rStack.pop_back();
}

// Get reference to the top-of-stack cell on the return stack.
Cell& topOfReturnStack() {
    return rStack.back();
}

/*

    Interpreter

*/

void startColon() {
    rpush(CELL(nextWord));
    nextWord = reinterpret_cast<Word*>(currentWord->parameter);
}

void endColon() {
    nextWord = reinterpret_cast<Word*>(topOfReturnStack());
    rpop();
}

void next() {
    currentWord = nextWord;
    ++nextWord;
}

/*

    Stack manipulation words

*/

// DROP ( x -- )
void drop() {
    REQUIRE_STACK_DEPTH(1, "DROP");
    pop();
    next();
}

// DUP ( x -- x x )
void dup() {
    REQUIRE_STACK_DEPTH(1, "DUP");
    push(topOfStack());
    next();
}

// OVER ( x1 x2 -- x1 x2 x1 )
void over() {
    REQUIRE_STACK_DEPTH(2, "OVER");
    push(dStack[dStack.size() - 2]);
    next();
}

// SWAP ( x1 x2 -- x2 x1 )
void swap() {
    REQUIRE_STACK_DEPTH(2, "SWAP");
    auto size = dStack.size();
    std::swap(dStack[size - 1], dStack[size - 2]);
    next();
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
    next();
}

// PICK ( xu ... x1 x0 u -- xu ... x1 x0 xu )
void pick() {
    REQUIRE_STACK_DEPTH(1, "PICK");
    auto index = topOfStack();
    REQUIRE_STACK_DEPTH(index + 2, "PICK");
    topOfStack() = dStack[dStack.size() - 2 - index];
    next();
}

// ROLL ( xu xu-1 ... x0 u -- xu-1 ... x0 xu )
void roll() {
    REQUIRE_STACK_DEPTH(1, "ROLL");
    auto index = topOfStack();
    pop();
    if (index > 0) {
        REQUIRE_STACK_DEPTH(index + 1, "ROLL");
        auto size = dStack.size();
        auto x = dStack[size - 1 - index];
        std::memmove(&dStack[size - 1 - index], &dStack[size - index], index * sizeof(Cell));
        topOfStack() = x;
    }
    next();
}

// ?DUP ( x -- 0 | x x )
void qdup() {
    REQUIRE_STACK_DEPTH(1, "?DUP");
    Cell top = topOfStack();
    if (top != 0) {
        push(top);
    }
    next();
}

// >R ( x -- ) ( R:  -- x )
void toR() {
    REQUIRE_STACK_DEPTH(1, ">R");
    rStack.push_back(topOfStack());
    pop();
    next();
}

// R> ( -- x ) ( R: x -- )
void rFrom() {
    RUNTIME_ERROR_IF(rStack.size() < 1, "R>: return stack underflow");
    push(rStack.back());
    rStack.pop_back();
    next();
}

// R@ ( -- x ) ( R: x -- x )
void rFetch() {
    RUNTIME_ERROR_IF(rStack.size() < 1, "R@: return stack underflow");
    push(rStack.back());
    next();
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
    REQUIRE_STACK_DEPTH(1, "ALIGNED");
    topOfStack() = CELL(alignAddress(topOfStack()));
    next();
}

// HERE ( -- addr )
void here() {
    push(CELL(dataPointer));
    next();
}

// ALLOT ( n -- )
void allot() {
    REQUIRE_STACK_DEPTH(1, "ALLOT");
    dataPointer += topOfStack();
    REQUIRE_VALID_HERE("ALLOT");
    pop();
    next();
}

// CELL+ ( a-addr1 -- a-addr2 )
void cellPlus() {
    REQUIRE_STACK_DEPTH(1, "CELL+");
    topOfStack() += CellSize;
    next();
}

// CELLS ( n1 -- n2 )
void cells() {
    REQUIRE_STACK_DEPTH(1, "CELLS");
    topOfStack() *= CellSize;
    next();
}

// CHAR+ ( c-addr1 -- c-addr2 )
void charPlus() {
    REQUIRE_STACK_DEPTH(1, "CHAR+");
    ++topOfStack();
    next();
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
    next();
}

// Store a char to data space.
void cdata(Cell x) {
    REQUIRE_VALID_HERE("C,");
    *dataPointer = static_cast<Char>(x);
    dataPointer += 1;
    next();
}

// C, ( char -- )
void ccomma() {
    REQUIRE_STACK_DEPTH(1, "C,");
    cdata(topOfStack());
    pop();
    next();
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
    next();
}

// @ ( a-addr -- x )
void fetch() {
    REQUIRE_STACK_DEPTH(1, "@");
    auto aaddr = AADDR(topOfStack());
    REQUIRE_ALIGNED(aaddr, "@");
    topOfStack() = *aaddr;
    next();
}

// c! ( char c-addr -- )
void cstore() {
    REQUIRE_STACK_DEPTH(2, "C!");
    auto caddr = CADDR(topOfStack());
    pop();
    auto x = topOfStack();
    pop();
    *caddr = static_cast<Char>(x);
    next();
}

// c@ ( c-addr -- char )
void cfetch() {
    REQUIRE_STACK_DEPTH(1, "C@");
    auto caddr = CADDR(topOfStack());
    REQUIRE_ALIGNED(caddr, "C@");
    topOfStack() = static_cast<Cell>(*caddr);
    next();
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
    next();
}

// KEY ( -- x )
void key() {
    push(static_cast<Cell>(std::cin.get()));
    next();
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
    next();
}

// - ( n1 n2 -- n3 )
void minus() {
    REQUIRE_STACK_DEPTH(2, "-");
    auto n2 = static_cast<SCell>(topOfStack());
    pop();
    auto n1 = static_cast<SCell>(topOfStack());
    topOfStack() = static_cast<Cell>(n1 - n2);
    next();
}

// * ( n1 n2 -- n3 )
void star() {
    REQUIRE_STACK_DEPTH(2, "*");
    auto n2 = static_cast<SCell>(topOfStack());
    pop();
    auto n1 = static_cast<SCell>(topOfStack());
    topOfStack() = static_cast<Cell>(n1 * n2);
    next();
}

// / ( n1 n2 -- n3 )
void slash() {
    REQUIRE_STACK_DEPTH(2, "/");
    auto n2 = static_cast<SCell>(topOfStack());
    pop();
    auto n1 = static_cast<SCell>(topOfStack());
    RUNTIME_ERROR_IF(n2 == 0, "/: zero divisor");
    topOfStack() = static_cast<Cell>(n1 / n2);
    next();
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
    next();
}

// NEGATE ( n1 -- n2 )
void negate() {
    REQUIRE_STACK_DEPTH(1, "NEGATE");
    topOfStack() = static_cast<Cell>(-static_cast<SCell>(topOfStack()));
    next();
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
    next();
}

// OR ( x1 x2 -- x3 )
void bitwiseOr() {
    REQUIRE_STACK_DEPTH(2, "OR");
    auto x2 = topOfStack();
    pop();
    topOfStack() = topOfStack() | x2;
    next();
}

// XOR ( x1 x2 -- x3 )
void bitwiseXor() {
    REQUIRE_STACK_DEPTH(2, "XOR");
    auto x2 = topOfStack();
    pop();
    topOfStack() = topOfStack() ^ x2;
    next();
}

// INVERT ( x1 -- x2 )
void invert() {
    REQUIRE_STACK_DEPTH(1, "INVERT");
    topOfStack() = ~topOfStack();
    next();
}

// LSHIFT ( x1 u -- x2 )
void lshift() {
    REQUIRE_STACK_DEPTH(2, "LSHIFT");
    auto n = topOfStack();
    pop();
    topOfStack() <<= n;
    next();
}

// RSHIFT ( x1 u -- x2 )
void rshift() {
    REQUIRE_STACK_DEPTH(2, "RSHIFT");
    auto n = topOfStack();
    pop();
    topOfStack() >>= n;
    next();
}

// = ( x1 x2 -- flag )
void equals() {
    REQUIRE_STACK_DEPTH(2, "=");
    auto n2 = topOfStack();
    pop();
    topOfStack() = topOfStack() == n2 ? True : False;
    next();
}

// < ( n1 n2 -- flag )
void lessThan() {
    REQUIRE_STACK_DEPTH(2, "<");
    auto n2 = static_cast<SCell>(topOfStack());
    pop();
    topOfStack() = static_cast<SCell>(topOfStack()) < n2 ? True : False;
    next();
}

// > ( n1 n2 -- flag )
void greaterThan() {
    REQUIRE_STACK_DEPTH(2, ">");
    auto n2 = static_cast<SCell>(topOfStack());
    pop();
    topOfStack() = static_cast<SCell>(topOfStack()) > n2 ? True : False;
    next();
}

/*

    Environmental primitives

*/

// #ARG ( -- n )
// Provide count of command-line arguments.
// Not an ANS Forth word.
void argCount() {
    push(commandLineArgCount);
    next();
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

    Dictionary

*/

void defineCodeWord(const char* name, Code code) {
    alignDataPointer();

    Word word;
    word.code = code;
    word.parameter = AADDR(dataPointer);
    word.name = name;

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
        if (word.isHidden())
            continue;
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

// FIND ( c-addr -- c-addr 0  |  xt 1  |  xt -1 )
void find() {
    auto caddr = CADDR(topOfStack());
    auto length = static_cast<Cell>(*caddr);
    auto name = caddr + 1;
    auto word = findWord(name, length);
    if (word == nullptr) {
        push(0);
    }
    else {
        topOfStack() = CELL(word);
        push(word->isImmediate() ? 1 : Cell(-1));
    }
}

void words() {
    for (auto i = dictionary.rbegin(); i != dictionary.rend(); ++i) {
        std::cout << i->name << " ";
    }
}

void initializeDictionary() {
    dictionary.clear();
    dictionary.reserve(128);

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
        {"CHAR+",   charPlus},
        {"DROP",    drop},
        {"DUP",     dup},
        {"EMIT",    emit},
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


