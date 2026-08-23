#ifndef LLL_FFI_H
#define LLL_FFI_H

#include <stdbool.h>
#include <stdint.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

void *ffi_load_library(const char *path);
void *ffi_get_symbol(void *handle, const char *name);
void ffi_close_library(void *handle);

bool ffi_read_memory(void *address, void *buffer, size_t size);
bool ffi_write_memory(void *address, const void *buffer, size_t size);
void *ffi_allocate_executable(size_t size);
bool ffi_protect_executable(void *address, size_t size);

#endif
