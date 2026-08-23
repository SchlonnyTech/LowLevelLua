#include "ffi.h"
#include <stdlib.h>
#include <string.h>

void *ffi_load_library(const char *path) {
#ifdef _WIN32
  return LoadLibraryA(path);
#else
  return dlopen(path, RTLD_NOW | RTLD_GLOBAL);
#endif
}

void *ffi_get_symbol(void *handle, const char *name) {
#ifdef _WIN32
  return GetProcAddress((HMODULE)handle, name);
#else
  return dlsym(handle, name);
#endif
}

void ffi_close_library(void *handle) {
#ifdef _WIN32
  FreeLibrary((HMODULE)handle);
#else
  dlclose(handle);
#endif
}

bool ffi_read_memory(void *address, void *buffer, size_t size) {
#ifdef _WIN32
  SIZE_T read = 0;
  return ReadProcessMemory(GetCurrentProcess(), address, buffer, size, &read) &&
         read == size;
#else
  memcpy(buffer, address, size);
  return true;
#endif
}

bool ffi_write_memory(void *address, const void *buffer, size_t size) {
#ifdef _WIN32
  DWORD old_protect;
  VirtualProtect(address, size, PAGE_EXECUTE_READWRITE, &old_protect);
  SIZE_T written = 0;
  bool ok = WriteProcessMemory(GetCurrentProcess(), address, buffer, size,
                               &written) &&
            written == size;
  VirtualProtect(address, size, old_protect, &old_protect);
  return ok;
#else
  size_t page_size = sysconf(_SC_PAGESIZE);
  void *page_start = (void *)((uintptr_t)address & ~(page_size - 1));
  mprotect(page_start, size + ((uintptr_t)address - (uintptr_t)page_start),
           PROT_READ | PROT_WRITE | PROT_EXEC);
  memcpy(address, buffer, size);
  return true;
#endif
}

void *ffi_allocate_executable(size_t size) {
#ifdef _WIN32
  return VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE,
                      PAGE_EXECUTE_READWRITE);
#else
  return mmap(NULL, size, PROT_READ | PROT_WRITE | PROT_EXEC,
              MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
#endif
}

bool ffi_protect_executable(void *address, size_t size) {
#ifdef _WIN32
  DWORD old_protect;
  return VirtualProtect(address, size, PAGE_EXECUTE_READ, &old_protect);
#else
  return mprotect(address, size, PROT_READ | PROT_EXEC) == 0;
#endif
}
