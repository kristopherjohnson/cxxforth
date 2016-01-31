#ifndef cxxforth_hpp_included
#define cxxforth_hpp_included

#include "cxxforthconfig.h"

#ifdef __cplusplus
extern "C" {
#endif

void resetForth();
int runForth(int argc, const char** argv);

#ifdef __cplusplus
}
#endif

#endif // cxxforth_hpp_included

