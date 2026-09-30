#include "intern.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
  char *str;
  size_t len;
  uint64_t hash;
} InternEntry;

struct InternPool {
  InternEntry *entries;
  size_t count, capacity;
  uint32_t *table;
  size_t table_size;
};

static uint64_t fnv64(const char *s, size_t n) {
  uint64_t h = 1469598103934665603ULL;
  for (size_t i = 0; i < n; i++) {
    h ^= (unsigned char)s[i];
    h *= 1099511628211ULL;
  }
  return h;
}

InternPool *intern_pool_create(void) {
  InternPool *p = calloc(1, sizeof(InternPool));
  p->capacity = 256;
  p->entries = malloc(p->capacity * sizeof(InternEntry));
  p->table_size = 1024;
  p->table = calloc(p->table_size, sizeof(uint32_t));
  return p;
}

void intern_pool_destroy(InternPool *p) {
  if (!p)
    return;
  for (size_t i = 0; i < p->count; i++)
    free(p->entries[i].str);
  free(p->entries);
  free(p->table);
  free(p);
}

static void grow_table(InternPool *p) {
  size_t ns = p->table_size * 2;
  uint32_t *nt = calloc(ns, sizeof(uint32_t));
  for (size_t i = 0; i < p->count; i++) {
    size_t h = p->entries[i].hash & (ns - 1);
    while (nt[h])
      h = (h + 1) & (ns - 1);
    nt[h] = (uint32_t)(i + 1);
  }
  free(p->table);
  p->table = nt;
  p->table_size = ns;
}

const char *intern_n(InternPool *p, const char *s, size_t n) {
  uint64_t h = fnv64(s, n);
  size_t idx = h & (p->table_size - 1);
  while (p->table[idx]) {
    InternEntry *e = &p->entries[p->table[idx] - 1];
    if (e->hash == h && e->len == n && memcmp(e->str, s, n) == 0)
      return e->str;
    idx = (idx + 1) & (p->table_size - 1);
  }
  if (p->count >= p->capacity) {
    p->capacity *= 2;
    p->entries = realloc(p->entries, p->capacity * sizeof(InternEntry));
  }
  char *copy = malloc(n + 1);
  memcpy(copy, s, n);
  copy[n] = 0;
  p->entries[p->count] = (InternEntry){copy, n, h};
  p->table[idx] = (uint32_t)(p->count + 1);
  p->count++;
  if (p->count * 4 > p->table_size * 3)
    grow_table(p);
  return copy;
}

const char *intern(InternPool *p, const char *s) {
  return intern_n(p, s, strlen(s));
}
