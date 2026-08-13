#ifndef CODEGEN_H
#define CODEGEN_H

#include "ast.h"
#include "platform.h"
#include <stdbool.h>
#include <stdio.h>

typedef struct {
  FILE *output;
  PlatformInfo platform;
  bool arch_x86;
  bool has_main;
  bool is_module;
  const char *current_file;
  int last_line;
  int defer_depth;
  char *def_names[256];
  ASTNode *def_exprs[256];
  bool in_func;
  bool jit_mode;
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
