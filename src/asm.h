#ifndef LLL_ASM_H
#define LLL_ASM_H

#include "ast.h"
#include "codegen.h"
#include <llvm-c/Core.h>

LLVMValueRef asm_emit(CodeGenContext *ctx, ASTNode *expr);

char *asm_translate(const char *src, int *max_operand);
char *asm_build_constraints(const char *clobbers, int operand_count);

#endif
