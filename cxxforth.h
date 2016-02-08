#ifndef cxxforth_hpp_included
#define cxxforth_hpp_included

#include "cxxforthconfig.h"

extern const char* cxxforthVersion;

#ifdef __cplusplus
extern "C" {
#endif

void cxxforthReset();
int cxxforthRun(int argc, const char** argv);

#ifdef __cplusplus
}
#endif

#endif // cxxforth_hpp_included

