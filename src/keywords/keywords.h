#ifndef LLL_KEYWORDS_H
#define LLL_KEYWORDS_H

#include "../ast.h"
#include "../codegen.h"
#include <stdbool.h>
#include <stdio.h>

struct Parser;

typedef struct KeywordHandler {
  const char *name;
  LLVMValueRef (*codegen)(CodeGenContext *ctx, FILE *out, ASTNode *node);
  ASTNode *(*parse)(struct Parser *p, int line, int col);
  int arg_count;
} KeywordHandler;

extern KeywordHandler keyword_handlers[];

void register_keyword_handler(KeywordHandler *handler);
void mark_import(const char *name);
int keyword_handlers_count(void);
void codegen_keyword_c(FILE *out, ASTNode *node);
void init_keywords(void);
KeywordHandler *find_keyword(const char *name);

void register_memory_keywords(void);
void register_io_keywords(void);
void register_include_keywords(void);

void create_lll_syscall(CodeGenContext *ctx);

#endif
