#ifndef KEYWORDS_H
#define KEYWORDS_H

#include "ast.h"
#include "codegen.h"
#include <stdbool.h>
#include <stdio.h>

typedef struct KeywordHandler {
  const char *name;
  LLVMValueRef (*codegen)(CodeGenContext *ctx, FILE *out, ASTNode *node);
  ASTNode *(*parse)(void *p, int line, int col);
  int builtin_type;
} KeywordHandler;

extern KeywordHandler keyword_handlers[];

void register_keyword_handler(KeywordHandler *handler);
int keyword_handlers_count(void);
void mark_import(const char *name);
void codegen_keyword_c(FILE *out, ASTNode *node);

void init_keywords(void);
KeywordHandler *find_keyword(const char *name);

void register_memory_keywords(void);
void register_io_keywords(void);
void register_include_keywords(void);

LLVMValueRef codegen_string_lib(CodeGenContext *ctx, const char *name,
                                ASTNode *expr);

void llvm_register_string_builtins(CodeGenContext *ctx);
void create_lll_syscall(CodeGenContext *ctx);

#endif
