#ifndef KEYWORDS_H
#define KEYWORDS_H

#include "parser.h"
#include <stdbool.h>
#include <stdio.h>

typedef struct {
  const char *name;
  const char **includes;
  int include_count;
  ASTNode *(*parse)(Parser *p, int line, int col);
  void (*codegen_c)(FILE *out, ASTNode *node);
  bool used;
} KeywordHandler;

extern KeywordHandler keyword_handlers[];

ASTNode *parse_keyword_statement(Parser *p);
void codegen_keyword_c(FILE *out, ASTNode *node);
void codegen_emit_includes(FILE *out);
void mark_import(const char *name);

ASTNode *parse_io(Parser *p, int line, int col);
void codegen_io_c(FILE *out, ASTNode *node);
ASTNode *parse_memory(Parser *p, int line, int col);
void codegen_memory_c(FILE *out, ASTNode *node);
ASTNode *parse_zstd(Parser *p, int line, int col);
void codegen_zstd_c(FILE *out, ASTNode *node);
ASTNode *parse_ffi(Parser *p, int line, int col);
void codegen_ffi_c(FILE *out, ASTNode *node);
#endif
