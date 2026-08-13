#ifndef PLATFORM_H
#define PLATFORM_H
#include "lll.h"
PlatformInfo platform_detect(void);
const char* platform_get_nasm_preamble(PlatformInfo *info);
const char* platform_get_c_runtime_prefix(PlatformInfo *info);
#endif
