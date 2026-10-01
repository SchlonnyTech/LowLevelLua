#include "asm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int parse_operand_index(const char **p) {
  const char *s = *p;
  if (*s != '{')
    return -1;
  s++;
  if (*s < '0' || *s > '9')
    return -1;
  int n = 0;
  while (*s >= '0' && *s <= '9') {
    n = n * 10 + (*s - '0');
    s++;
  }
  if (*s != '}')
    return -1;
  *p = s + 1;
  return n;
}

char *asm_translate(const char *src, int *max_operand) {
  size_t cap = strlen(src) * 2 + 64;
  char *out = malloc(cap);
  size_t o = 0;
  int max_op = -1;

  for (const char *p = src; *p;) {
    if (o + 8 >= cap) {
      cap *= 2;
      out = realloc(out, cap);
    }

    if (*p == '$') {
      if (p[1] == '$') {
        out[o++] = '$';
        out[o++] = '$';
        p += 2;
      } else {
        out[o++] = '$';
        out[o++] = '$';
        p += 1;
      }
      continue;
    }

    if (*p == '{') {
      const char *q = p;
      int idx = parse_operand_index(&q);
      if (idx >= 0) {
        if (idx > max_op)
          max_op = idx;
        o += snprintf(out + o, cap - o, "${%d}", idx);
        p = q;
        continue;
      }
    }

    out[o++] = *p++;
  }

  out[o] = 0;
  if (max_operand)
    *max_operand = max_op;
  return out;
}

char *asm_build_constraints(const char *clobbers, int operand_count) {
  size_t cap = 64 + (clobbers ? strlen(clobbers) * 2 : 0) + operand_count * 2;
  char *out = malloc(cap);
  size_t o = 0;
  out[0] = 0;

  for (int i = 0; i < operand_count; i++) {
    if (i > 0)
      out[o++] = ',';
    out[o++] = 'r';
  }

  if (clobbers && *clobbers) {
    const char *p = clobbers;
    while (*p) {
      while (*p == ' ' || *p == ',' || *p == '\t')
        p++;
      if (!*p)
        break;
      if (o + 4 >= cap) {
        cap *= 2;
        out = realloc(out, cap);
      }
      if (o > 0 && out[o - 1] != ',')
        out[o++] = ',';
      if (*p == '~') {
        out[o++] = '~';
        p++;
      }
      if (*p == '{') {
        while (*p && *p != '}')
          out[o++] = *p++;
        if (*p == '}')
          out[o++] = *p++;
      } else {
        while (*p && *p != ' ' && *p != ',')
          out[o++] = *p++;
      }
    }
  }

  out[o] = 0;
  return out;
}

LLVMValueRef asm_emit(CodeGenContext *ctx, ASTNode *expr) {
  if (!expr || expr->type != NODE_ASM_BLOCK || !expr->asm_block.code)
    return NULL;

  int max_op = -1;
  char *translated = asm_translate(expr->asm_block.code, &max_op);
  int n_operands = max_op >= 0 ? max_op + 1 : 0;

  int user_operands = expr->asm_block.operand_count;
  const char *clobbers =
      expr->asm_block.constraints ? expr->asm_block.constraints : "";

  int has_output = (clobbers[0] == '=');

  LLVMValueRef *arg_vals =
      n_operands ? malloc(sizeof(LLVMValueRef) * n_operands) : NULL;
  LLVMTypeRef *arg_types =
      n_operands ? malloc(sizeof(LLVMTypeRef) * n_operands) : NULL;

  for (int i = 0; i < n_operands; i++) {
    if (i < user_operands)
      arg_vals[i] = codegen_expr(ctx, expr->asm_block.operands[i]);
    else
      arg_vals[i] = LLVMConstInt(LLVMInt64TypeInContext(ctx->llvm_ctx), 0, 0);
    arg_types[i] = LLVMTypeOf(arg_vals[i]);
  }

  LLVMTypeRef ret_ty = has_output ? LLVMInt64TypeInContext(ctx->llvm_ctx)
                                  : LLVMVoidTypeInContext(ctx->llvm_ctx);

  LLVMTypeRef fn_ty = LLVMFunctionType(ret_ty, arg_types, n_operands, 0);

  char *constraints = asm_build_constraints(clobbers, n_operands);

  LLVMInlineAsmDialect dialect = expr->asm_block.dialect
                                     ? LLVMInlineAsmDialectIntel
                                     : LLVMInlineAsmDialectATT;

  LLVMValueRef ia =
      LLVMGetInlineAsm(fn_ty, translated, strlen(translated), constraints,
                       strlen(constraints), 1, 0, dialect, 0);

  const char *call_name =
      LLVMGetTypeKind(ret_ty) == LLVMVoidTypeKind ? "" : "asm";
  LLVMValueRef call =
      LLVMBuildCall2(ctx->builder, fn_ty, ia, arg_vals, n_operands, call_name);

  free(translated);
  free(constraints);
  free(arg_vals);
  free(arg_types);
  return call;
}
