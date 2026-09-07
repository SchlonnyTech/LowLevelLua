#ifndef LLLMAKE_PARSE_H
#define LLLMAKE_PARSE_H

#include "codegen.h"
#include <stdbool.h>

int lllmake_build_from_file(const char *filename, BuildType build_type,
                            bool verbose);

#endif
