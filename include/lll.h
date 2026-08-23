#ifndef LLL_H
#define LLL_H
#include <stdbool.h>
#include <stdint.h>
#define LLL_VERSION_MAJOR 1
#define LLL_VERSION_MINOR 0
#define LLL_VERSION_PATCH 2
typedef enum {
  PLATFORM_WINDOWS,
  PLATFORM_LINUX,
  PLATFORM_MACOS,
  PLATFORM_UNKNOWN
} Platform;
typedef struct {
  Platform platform;
  const char *nasm_format;
  const char *obj_extension;
  const char *executable_extension;
  const char *linker_command;
  bool uses_underscore_prefix;
  const char *calling_convention;
} PlatformInfo;
#endif
