#include "../codegen.h"
#include "keywords.h"
#include <llvm-c/Core.h>
#include <stdio.h>
#include <string.h>

static LLVMValueRef kio_memcpy(CodeGenContext *ctx, FILE *out, ASTNode *node) {
  (void)out;
  LLVMTypeRef i8p = LLVMPointerType(LLVMInt8TypeInContext(ctx->llvm_ctx), 0);
  LLVMTypeRef i64 = LLVMInt64TypeInContext(ctx->llvm_ctx);
  LLVMTypeRef ft = LLVMFunctionType(i8p, (LLVMTypeRef[]){i8p, i8p, i64}, 3, 0);
  LLVMValueRef fn = LLVMGetNamedFunction(ctx->module, "memcpy");
  if (!fn)
    fn = LLVMAddFunction(ctx->module, "memcpy", ft);
  LLVMValueRef dst = codegen_expr(ctx, node->keyword.args[0]);
  LLVMValueRef src = codegen_expr(ctx, node->keyword.args[1]);
  LLVMValueRef len = codegen_expr(ctx, node->keyword.args[2]);
  if (LLVMGetTypeKind(LLVMTypeOf(dst)) == LLVMIntegerTypeKind)
    dst = LLVMBuildIntToPtr(ctx->builder, dst, i8p, "c");
  if (LLVMGetTypeKind(LLVMTypeOf(src)) == LLVMIntegerTypeKind)
    src = LLVMBuildIntToPtr(ctx->builder, src, i8p, "c");
  LLVMValueRef args[] = {dst, src, len};
  return LLVMBuildCall2(ctx->builder, ft, fn, args, 3, "");
}

static LLVMValueRef kio_memset(CodeGenContext *ctx, FILE *out, ASTNode *node) {
  (void)out;
  LLVMTypeRef i8p = LLVMPointerType(LLVMInt8TypeInContext(ctx->llvm_ctx), 0);
  LLVMTypeRef i64 = LLVMInt64TypeInContext(ctx->llvm_ctx);
  LLVMTypeRef ft = LLVMFunctionType(i8p, (LLVMTypeRef[]){i8p, i64, i64}, 3, 0);
  LLVMValueRef fn = LLVMGetNamedFunction(ctx->module, "memset");
  if (!fn)
    fn = LLVMAddFunction(ctx->module, "memset", ft);
  LLVMValueRef dst = codegen_expr(ctx, node->keyword.args[0]);
  LLVMValueRef val = codegen_expr(ctx, node->keyword.args[1]);
  LLVMValueRef len = codegen_expr(ctx, node->keyword.args[2]);
  if (LLVMGetTypeKind(LLVMTypeOf(dst)) == LLVMIntegerTypeKind)
    dst = LLVMBuildIntToPtr(ctx->builder, dst, i8p, "c");
  LLVMValueRef args[] = {dst, val, len};
  return LLVMBuildCall2(ctx->builder, ft, fn, args, 3, "");
}

static LLVMValueRef kio_malloc(CodeGenContext *ctx, FILE *out, ASTNode *node) {
  (void)out;
  LLVMTypeRef i8p = LLVMPointerType(LLVMInt8TypeInContext(ctx->llvm_ctx), 0);
  LLVMTypeRef i64 = LLVMInt64TypeInContext(ctx->llvm_ctx);
  LLVMTypeRef ft = LLVMFunctionType(i8p, (LLVMTypeRef[]){i64}, 1, 0);
  LLVMValueRef fn = LLVMGetNamedFunction(ctx->module, "malloc");
  if (!fn)
    fn = LLVMAddFunction(ctx->module, "malloc", ft);
  LLVMValueRef size = codegen_expr(ctx, node->keyword.args[0]);
  LLVMValueRef args[] = {size};
  return LLVMBuildCall2(ctx->builder, ft, fn, args, 1, "");
}

static LLVMValueRef kio_free(CodeGenContext *ctx, FILE *out, ASTNode *node) {
  (void)out;
  LLVMTypeRef i8p = LLVMPointerType(LLVMInt8TypeInContext(ctx->llvm_ctx), 0);
  LLVMTypeRef void_t = LLVMVoidTypeInContext(ctx->llvm_ctx);
  LLVMTypeRef ft = LLVMFunctionType(void_t, (LLVMTypeRef[]){i8p}, 1, 0);
  LLVMValueRef fn = LLVMGetNamedFunction(ctx->module, "free");
  if (!fn)
    fn = LLVMAddFunction(ctx->module, "free", ft);
  LLVMValueRef ptr = codegen_expr(ctx, node->keyword.args[0]);
  if (LLVMGetTypeKind(LLVMTypeOf(ptr)) == LLVMIntegerTypeKind)
    ptr = LLVMBuildIntToPtr(ctx->builder, ptr, i8p, "c");
  LLVMValueRef args[] = {ptr};
  return LLVMBuildCall2(ctx->builder, ft, fn, args, 1, "");
}

static LLVMValueRef kio_alloc(CodeGenContext *ctx, FILE *out, ASTNode *node) {
  (void)out;
  LLVMTypeRef i8p = LLVMPointerType(LLVMInt8TypeInContext(ctx->llvm_ctx), 0);
  LLVMTypeRef i64 = LLVMInt64TypeInContext(ctx->llvm_ctx);
  LLVMTypeRef ft = LLVMFunctionType(i8p, (LLVMTypeRef[]){i64, i64}, 2, 0);
  LLVMValueRef fn = LLVMGetNamedFunction(ctx->module, "calloc");
  if (!fn)
    fn = LLVMAddFunction(ctx->module, "calloc", ft);
  LLVMValueRef count = codegen_expr(ctx, node->keyword.args[0]);
  LLVMValueRef size = codegen_expr(ctx, node->keyword.args[1]);
  LLVMValueRef args[] = {count, size};
  return LLVMBuildCall2(ctx->builder, ft, fn, args, 2, "");
}

static LLVMValueRef kio_realloc(CodeGenContext *ctx, FILE *out, ASTNode *node) {
  (void)out;
  LLVMTypeRef i8p = LLVMPointerType(LLVMInt8TypeInContext(ctx->llvm_ctx), 0);
  LLVMTypeRef i64 = LLVMInt64TypeInContext(ctx->llvm_ctx);
  LLVMTypeRef ft = LLVMFunctionType(i8p, (LLVMTypeRef[]){i8p, i64}, 2, 0);
  LLVMValueRef fn = LLVMGetNamedFunction(ctx->module, "realloc");
  if (!fn)
    fn = LLVMAddFunction(ctx->module, "realloc", ft);
  LLVMValueRef ptr = codegen_expr(ctx, node->keyword.args[0]);
  LLVMValueRef size = codegen_expr(ctx, node->keyword.args[1]);
  if (LLVMGetTypeKind(LLVMTypeOf(ptr)) == LLVMIntegerTypeKind)
    ptr = LLVMBuildIntToPtr(ctx->builder, ptr, i8p, "c");
  LLVMValueRef args[] = {ptr, size};
  return LLVMBuildCall2(ctx->builder, ft, fn, args, 2, "");
}

static LLVMValueRef kio_memcmp(CodeGenContext *ctx, FILE *out, ASTNode *node) {
  (void)out;
  LLVMTypeRef i8p = LLVMPointerType(LLVMInt8TypeInContext(ctx->llvm_ctx), 0);
  LLVMTypeRef i32 = LLVMInt32TypeInContext(ctx->llvm_ctx);
  LLVMTypeRef i64 = LLVMInt64TypeInContext(ctx->llvm_ctx);
  LLVMTypeRef ft = LLVMFunctionType(i32, (LLVMTypeRef[]){i8p, i8p, i64}, 3, 0);
  LLVMValueRef fn = LLVMGetNamedFunction(ctx->module, "memcmp");
  if (!fn)
    fn = LLVMAddFunction(ctx->module, "memcmp", ft);
  LLVMValueRef a = codegen_expr(ctx, node->keyword.args[0]);
  LLVMValueRef b = codegen_expr(ctx, node->keyword.args[1]);
  LLVMValueRef len = codegen_expr(ctx, node->keyword.args[2]);
  if (LLVMGetTypeKind(LLVMTypeOf(a)) == LLVMIntegerTypeKind)
    a = LLVMBuildIntToPtr(ctx->builder, a, i8p, "c");
  if (LLVMGetTypeKind(LLVMTypeOf(b)) == LLVMIntegerTypeKind)
    b = LLVMBuildIntToPtr(ctx->builder, b, i8p, "c");
  LLVMValueRef args[] = {a, b, len};
  return LLVMBuildCall2(ctx->builder, ft, fn, args, 3, "");
}

static LLVMValueRef kio_memmove(CodeGenContext *ctx, FILE *out, ASTNode *node) {
  (void)out;
  LLVMTypeRef i8p = LLVMPointerType(LLVMInt8TypeInContext(ctx->llvm_ctx), 0);
  LLVMTypeRef i64 = LLVMInt64TypeInContext(ctx->llvm_ctx);
  LLVMTypeRef ft = LLVMFunctionType(i8p, (LLVMTypeRef[]){i8p, i8p, i64}, 3, 0);
  LLVMValueRef fn = LLVMGetNamedFunction(ctx->module, "memmove");
  if (!fn)
    fn = LLVMAddFunction(ctx->module, "memmove", ft);
  LLVMValueRef dst = codegen_expr(ctx, node->keyword.args[0]);
  LLVMValueRef src = codegen_expr(ctx, node->keyword.args[1]);
  LLVMValueRef len = codegen_expr(ctx, node->keyword.args[2]);
  if (LLVMGetTypeKind(LLVMTypeOf(dst)) == LLVMIntegerTypeKind)
    dst = LLVMBuildIntToPtr(ctx->builder, dst, i8p, "c");
  if (LLVMGetTypeKind(LLVMTypeOf(src)) == LLVMIntegerTypeKind)
    src = LLVMBuildIntToPtr(ctx->builder, src, i8p, "c");
  LLVMValueRef args[] = {dst, src, len};
  return LLVMBuildCall2(ctx->builder, ft, fn, args, 3, "");
}

static KeywordHandler mem_handlers[] = {
    {"mem.memcpy", kio_memcpy, NULL, 3},
    {"mem.memset", kio_memset, NULL, 3},
    {"mem.malloc", kio_malloc, NULL, 1},
    {"mem.free", kio_free, NULL, 1},
    {"mem.alloc", kio_alloc, NULL, 2},
    {"mem.realloc", kio_realloc, NULL, 2},
    {"mem.memcmp", kio_memcmp, NULL, 3},
    {"mem.memmove", kio_memmove, NULL, 3},
    {NULL, NULL, NULL, 0},
};

void register_memory_keywords(void) {
  for (int i = 0; mem_handlers[i].name; i++) {
    register_keyword_handler(&mem_handlers[i]);
  }
}
