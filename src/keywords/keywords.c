#include "keywords.h"
#include <stdlib.h>
#include <string.h>

extern ASTNode *parse_print(Parser *p, int line, int col);
extern void codegen_print_c(FILE *out, ASTNode *node);

static const char *io_includes[] = {"<stdio.h>", "<stdlib.h>", "<unistd.h>"};
static const char *mem_includes[] = {"<stdlib.h>", "<string.h>"};
static const char *zstd_includes[] = {"<zstd.h>"};

KeywordHandler keyword_handlers[] = {
    {"io", io_includes, 3, parse_io, codegen_io_c, false},
    {"mem", mem_includes, 2, parse_memory, codegen_memory_c, false},
    {"zstd", zstd_includes, 1, parse_zstd, codegen_zstd_c, false},
    {NULL, NULL, 0, NULL, NULL, false}};

void mark_import(const char *name) {
  for (int i = 0; keyword_handlers[i].name; i++) {
    if (strcmp(name, keyword_handlers[i].name) == 0) {
      keyword_handlers[i].used = true;
      return;
    }
  }
}

static int match_prefix(const char *name, const char *prefix) {
  int len = strlen(prefix);
  return strncmp(name, prefix, len) == 0;
}

ASTNode *parse_keyword_statement(Parser *p) {
  int line = p->current.line, col = p->current.column;
  if (p->current.type != TOKEN_IDENT)
    return NULL;
  for (int i = 0; keyword_handlers[i].name; i++)
    if (strcmp(p->current.text, keyword_handlers[i].name) == 0)
      return keyword_handlers[i].parse(p, line, col);
  return NULL;
}

void codegen_keyword_c(FILE *out, ASTNode *node) {
  for (int i = 0; keyword_handlers[i].name; i++)
    if (match_prefix(node->keyword.name, keyword_handlers[i].name)) {
      keyword_handlers[i].codegen_c(out, node);
      return;
    }
}

void codegen_emit_includes(FILE *out) {
  for (int i = 0; keyword_handlers[i].name; i++) {
    if (!keyword_handlers[i].used)
      continue;
    for (int j = 0; j < keyword_handlers[i].include_count; j++)
      fprintf(out, "#include %s\n", keyword_handlers[i].includes[j]);
  }
}
