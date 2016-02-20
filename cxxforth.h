#ifndef cxxforth_hpp_included
#define cxxforth_hpp_included

#include "cxxforthconfig.h"

extern const char* cxxforth_version;

#ifdef __cplusplus
extern "C" {
#endif

void cxxforth_reset();
int cxxforth_main(int argc, const char** argv);

#ifdef __cplusplus
}
#endif

#endif // cxxforth_hpp_included

