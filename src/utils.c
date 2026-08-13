#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
char* string_copy(const char *src) {
    if (!src) return NULL;
    char *dst = malloc(strlen(src) + 1);
    if (dst) strcpy(dst, src);
    return dst;
}
char* string_concat(const char *a, const char *b) {
    size_t la = a ? strlen(a) : 0;
    size_t lb = b ? strlen(b) : 0;
    char *r = malloc(la + lb + 1);
    if (!r) return NULL;
    if (a) strcpy(r, a);
    if (b) strcpy(r + la, b);
    return r;
}
char* string_format(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(NULL, 0, fmt, args);
    va_end(args);
    char *buf = malloc(n + 1);
    if (!buf) return NULL;
    va_start(args, fmt);
    vsnprintf(buf, n + 1, fmt, args);
    va_end(args);
    return buf;
}
bool string_equals(const char *a, const char *b) {
    if (!a && !b) return true;
    if (!a || !b) return false;
    return strcmp(a, b) == 0;
}
int string_hash(const char *str) {
    unsigned hash = 5381;
    int c;
    while ((c = *str++)) hash = ((hash << 5) + hash) + c;
    return hash;
}
Array* array_create(int initial_capacity) {
    Array *arr = malloc(sizeof(Array));
    arr->items = malloc(sizeof(void*) * initial_capacity);
    arr->count = 0;
    arr->capacity = initial_capacity;
    return arr;
}
void array_push(Array *array, void *item) {
    if (array->count >= array->capacity) {
        array->capacity *= 2;
        array->items = realloc(array->items, sizeof(void*) * array->capacity);
    }
    array->items[array->count++] = item;
}
void* array_pop(Array *array) {
    if (array->count == 0) return NULL;
    return array->items[--array->count];
}
void array_destroy(Array *array) {
    free(array->items);
    free(array);
}
HashMap* hashmap_create(int size) {
    HashMap *map = malloc(sizeof(HashMap));
    map->buckets = calloc(size, sizeof(HashEntry*));
    map->size = size;
    return map;
}
void hashmap_insert(HashMap *map, const char *key, void *value) {
    unsigned idx = string_hash(key) % map->size;
    HashEntry *entry = malloc(sizeof(HashEntry));
    entry->key = string_copy(key);
    entry->value = value;
    entry->next = map->buckets[idx];
    map->buckets[idx] = entry;
}
void* hashmap_get(HashMap *map, const char *key) {
    unsigned idx = string_hash(key) % map->size;
    HashEntry *entry = map->buckets[idx];
    while (entry) {
        if (string_equals(entry->key, key)) return entry->value;
        entry = entry->next;
    }
    return NULL;
}
bool hashmap_has(HashMap *map, const char *key) {
    return hashmap_get(map, key) != NULL;
}
void hashmap_destroy(HashMap *map) {
    for (int i = 0; i < map->size; i++) {
        HashEntry *entry = map->buckets[i];
        while (entry) {
            HashEntry *next = entry->next;
            free(entry->key);
            free(entry);
            entry = next;
        }
    }
    free(map->buckets);
    free(map);
}
void error_report(int line, int column, const char *fmt, ...) {
    va_list args;
    fprintf(stderr, "Error: %d:%d - ", line, column);
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
}
void warning_report(int line, int column, const char *fmt, ...) {
    va_list args;
    fprintf(stderr, "Warning: %d:%d - ", line, column);
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
}
char* read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(size + 1);
    if (!buf) { fclose(f); return NULL; }
    fread(buf, 1, size, f);
    buf[size] = '\0';
    fclose(f);
    return buf;
}
bool file_exists(const char *path) {
    FILE *f = fopen(path, "r");
    if (f) { fclose(f); return true; }
    return false;
}
