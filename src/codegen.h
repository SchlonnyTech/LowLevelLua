#ifndef CODEGEN_H
#define CODEGEN_H

#include "ast.h"
#include "platform.h"
#include <stdbool.h>
#include <stdio.h>

typedef struct {
  FILE *output;
  PlatformInfo platform;
  bool in_func;
  bool has_main;
  bool is_module;
  bool arch_x86;
  int defer_depth;
  char *def_names[8];
  ASTNode *def_exprs[8];
  bool needs_min, needs_max, needs_abs;
  bool needs_str_i, needs_str_d, needs_str_s, needs_str;
  bool needs_cat;
  bool needs_print;
} CodeGenContext;

void codegen_init(CodeGenContext *ctx, FILE *output, PlatformInfo platform);
void codegen_generate_program(CodeGenContext *ctx, ASTNode *program);
bool codegen_jit_compile(CodeGenContext *ctx, ASTNode *program,
                         const char *output_path);
void codegen_emit_includes(FILE *output);

#ifdef LLL_JIT
int codegen_jit_exec(CodeGenContext *ctx, ASTNode *program);
#endif

#endif
