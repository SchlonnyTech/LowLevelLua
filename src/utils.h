#ifndef UTILS_H
#define UTILS_H
#include <stdint.h>
#include <stdbool.h>
char* string_copy(const char *src);
char* string_concat(const char *a, const char *b);
char* string_format(const char *fmt, ...);
bool string_equals(const char *a, const char *b);
int string_hash(const char *str);
typedef struct { void **items; int count; int capacity; } Array;
Array* array_create(int initial_capacity);
void array_push(Array *array, void *item);
void* array_pop(Array *array);
void array_destroy(Array *array);
typedef struct HashEntry { char *key; void *value; struct HashEntry *next; } HashEntry;
typedef struct { HashEntry **buckets; int size; } HashMap;
HashMap* hashmap_create(int size);
void hashmap_insert(HashMap *map, const char *key, void *value);
void* hashmap_get(HashMap *map, const char *key);
bool hashmap_has(HashMap *map, const char *key);
void hashmap_destroy(HashMap *map);
void error_report(int line, int column, const char *fmt, ...);
void warning_report(int line, int column, const char *fmt, ...);
char* read_file(const char *path);
bool file_exists(const char *path);
#endif
