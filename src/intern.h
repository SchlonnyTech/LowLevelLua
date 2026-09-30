#ifndef LLL_INTERN_H
#define LLL_INTERN_H

#include <stddef.h>
#include <stdint.h>

typedef struct InternPool InternPool;

InternPool *intern_pool_create(void);
void intern_pool_destroy(InternPool *p);
const char *intern(InternPool *p, const char *s);
const char *intern_n(InternPool *p, const char *s, size_t n);

#endif
