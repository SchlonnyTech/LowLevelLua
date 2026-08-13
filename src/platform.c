#include "platform.h"
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#endif
PlatformInfo platform_detect(void) {
    PlatformInfo info;
    memset(&info, 0, sizeof(info));
#ifdef _WIN32
    info.platform = PLATFORM_WINDOWS;
    info.nasm_format = "win64";
    info.obj_extension = ".obj";
    info.executable_extension = ".exe";
    info.linker_command = "link";
    info.uses_underscore_prefix = false;
    info.calling_convention = "windows";
#elif __APPLE__
    info.platform = PLATFORM_MACOS;
    info.nasm_format = "macho64";
    info.obj_extension = ".o";
    info.executable_extension = "";
    info.linker_command = "ld -lSystem -macos_version_min 10.13";
    info.uses_underscore_prefix = true;
    info.calling_convention = "systemv";
#else
    info.platform = PLATFORM_LINUX;
    info.nasm_format = "elf64";
    info.obj_extension = ".o";
    info.executable_extension = "";
    info.linker_command = "ld";
    info.uses_underscore_prefix = false;
    info.calling_convention = "systemv";
#endif
    return info;
}
const char* platform_get_nasm_preamble(PlatformInfo *info) {
    switch (info->platform) {
        case PLATFORM_WINDOWS: return "BITS 64\ndefault rel\n";
        case PLATFORM_LINUX:   return "BITS 64\ndefault rel\n";
        case PLATFORM_MACOS:   return "BITS 64\ndefault rel\n";
        default: return "BITS 64\n";
    }
}
const char* platform_get_c_runtime_prefix(PlatformInfo *info) {
    if (info->uses_underscore_prefix) return "_";
    return "";
}
