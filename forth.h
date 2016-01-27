#ifndef forth_hpp_included
#define forth_hpp_included

#include "forthconfig.h"

#ifdef __cplusplus
extern "C" {
#endif

void resetForth();
int runForth(int argc, const char** argv);

#ifdef __cplusplus
}
#endif

#endif // forth_hpp_included

