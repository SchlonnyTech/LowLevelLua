#ifndef LLL_H
#define LLL_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define LLL_VERSION_MAJOR 1
#define LLL_VERSION_MINOR 2
#define LLL_VERSION_PATCH 1

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

typedef enum {
  LLL_CHECK_MAJOR = 0,
  LLL_CHECK_MINOR = 1,
  LLL_CHECK_PATCH = 2
} LLLVersionStrictness;

static inline Platform lll_get_platform(void) {
#if defined(_WIN32)
  return PLATFORM_WINDOWS;
#elif defined(__linux__)
  return PLATFORM_LINUX;
#elif defined(__APPLE__)
  return PLATFORM_MACOS;
#else
  return PLATFORM_UNKNOWN;
#endif
}

static inline const char *lll_platform_name(Platform platform) {
  switch (platform) {
  case PLATFORM_WINDOWS:
    return "windows";
  case PLATFORM_LINUX:
    return "linux";
  case PLATFORM_MACOS:
    return "macos";
  default:
    return "unknown";
  }
}

static inline int lll_version_compatible_full(int req_major, int req_minor,
                                              int req_patch) {
  if (LLL_VERSION_MAJOR != req_major)
    return 0;
  if (LLL_VERSION_MINOR < req_minor)
    return 0;
  if (LLL_VERSION_MINOR == req_minor && LLL_VERSION_PATCH < req_patch)
    return 0;
  return 1;
}

static inline int lll_version_compatible(int req_major, int req_minor) {
  return lll_version_compatible_full(req_major, req_minor, 0);
}

static inline int lll_version_at_least(int major, int minor, int patch) {
  return lll_version_compatible_full(major, minor, patch);
}

static inline int lll_version_exact(int major, int minor, int patch) {
  return LLL_VERSION_MAJOR == major && LLL_VERSION_MINOR == minor &&
         LLL_VERSION_PATCH == patch;
}

static inline void lll_require_version(int major, int minor, int patch) {
  if (!lll_version_compatible_full(major, minor, patch)) {
    fprintf(stderr,
            "LLL: This script requires v%d.%d.%d+, but you have v%d.%d.%d\n",
            major, minor, patch, LLL_VERSION_MAJOR, LLL_VERSION_MINOR,
            LLL_VERSION_PATCH);
    exit(1);
  }
}

static inline void lll_require_version_exact(int major, int minor, int patch) {
  if (!lll_version_exact(major, minor, patch)) {
    fprintf(
        stderr,
        "LLL: This script requires exactly v%d.%d.%d, but you have v%d.%d.%d\n",
        major, minor, patch, LLL_VERSION_MAJOR, LLL_VERSION_MINOR,
        LLL_VERSION_PATCH);
    exit(1);
  }
}

static inline void lll_require_platform(Platform platform) {
  Platform current = lll_get_platform();
  if (current != platform) {
    fprintf(stderr,
            "LLL: This script requires platform '%s', but you are on '%s'\n",
            lll_platform_name(platform), lll_platform_name(current));
    exit(1);
  }
}

static inline void lll_require_platforms(Platform *platforms, int count) {
  Platform current = lll_get_platform();
  for (int i = 0; i < count; i++) {
    if (platforms[i] == current)
      return;
  }
  fprintf(stderr, "LLL: This script does not support platform '%s'\n",
          lll_platform_name(current));
  exit(1);
}

#endif
