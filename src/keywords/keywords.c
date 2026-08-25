#include "keywords.h"
#include <stdio.h>
#include <string.h>

#define MAX_KEYWORDS 64

static KeywordHandler registered[MAX_KEYWORDS];
static int registered_count = 0;

KeywordHandler keyword_handlers[] = {
    {NULL, NULL, NULL, 0},
};
void register_keyword_handler(KeywordHandler *handler) {
  if (registered_count < MAX_KEYWORDS) {
    registered[registered_count] = *handler;
    registered_count++;
  }
}

int keyword_handlers_count(void) { return registered_count; }

void mark_import(const char *name) { (void)name; }

void codegen_keyword_c(FILE *out, ASTNode *node) {
  (void)out;
  (void)node;
}
#include "keywords.h"

static bool keywords_initialized = false;

void init_keywords(void) {
  if (keywords_initialized)
    return;
  keywords_initialized = true;
  register_memory_keywords();
  register_io_keywords();
  register_include_keywords();
}

KeywordHandler *find_keyword(const char *name) {
  for (int i = 0; i < registered_count; i++) {
    if (strcmp(registered[i].name, name) == 0) {
      return &registered[i];
    }
  }
  return NULL;
}
