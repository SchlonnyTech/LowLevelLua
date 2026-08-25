#include "../codegen.h"
#include "keywords.h"
#include <llvm-c/Core.h>
#include <stdio.h>
#include <string.h>

void create_lll_syscall(CodeGenContext *ctx) {
  LLVMTypeRef i64 = LLVMInt64TypeInContext(ctx->llvm_ctx);
  LLVMTypeRef st =
      LLVMFunctionType(i64, (LLVMTypeRef[]){i64, i64, i64, i64}, 4, 0);
  LLVMValueRef fn = LLVMAddFunction(ctx->module, "lll_syscall", st);
  LLVMSetLinkage(fn, LLVMInternalLinkage);

  LLVMBasicBlockRef entry =
      LLVMAppendBasicBlockInContext(ctx->llvm_ctx, fn, "entry");
  LLVMPositionBuilderAtEnd(ctx->builder, entry);

  LLVMValueRef num = LLVMGetParam(fn, 0);
  LLVMValueRef a1 = LLVMGetParam(fn, 1);
  LLVMValueRef a2 = LLVMGetParam(fn, 2);
  LLVMValueRef a3 = LLVMGetParam(fn, 3);

  LLVMTypeRef asm_t =
      LLVMFunctionType(i64, (LLVMTypeRef[]){i64, i64, i64, i64}, 4, 0);
  LLVMValueRef asm_fn = LLVMConstInlineAsm(
      asm_t, "syscall", "={rax},{rax},{rdi},{rsi},{rdx},~{rcx},~{r11}", true,
      false);

  LLVMValueRef args[] = {num, a1, a2, a3};
  LLVMValueRef result =
      LLVMBuildCall2(ctx->builder, asm_t, asm_fn, args, 4, "sc");
  LLVMBuildRet(ctx->builder, result);
}

static LLVMValueRef lll_syscall(CodeGenContext *ctx, int syscall_num,
                                LLVMValueRef a1, LLVMValueRef a2,
                                LLVMValueRef a3) {
  LLVMTypeRef i64 = LLVMInt64TypeInContext(ctx->llvm_ctx);
  LLVMTypeRef st =
      LLVMFunctionType(i64, (LLVMTypeRef[]){i64, i64, i64, i64}, 4, 0);
  LLVMValueRef sf = LLVMGetNamedFunction(ctx->module, "lll_syscall");
  if (!sf)
    sf = LLVMAddFunction(ctx->module, "lll_syscall", st);
  LLVMValueRef args[] = {
      LLVMConstInt(i64, syscall_num, 0), a1 ? a1 : LLVMConstInt(i64, 0, 0),
      a2 ? a2 : LLVMConstInt(i64, 0, 0), a3 ? a3 : LLVMConstInt(i64, 0, 0)};
  return LLVMBuildCall2(ctx->builder, st, sf, args, 4, "syscall");
}

static LLVMValueRef kio_write(CodeGenContext *ctx, FILE *out, ASTNode *node) {
  (void)out;
  LLVMTypeRef i64 = LLVMInt64TypeInContext(ctx->llvm_ctx);
  LLVMValueRef fd = codegen_expr(ctx, node->keyword.args[0]);
  LLVMValueRef buf = codegen_expr(ctx, node->keyword.args[1]);
  LLVMValueRef len = codegen_expr(ctx, node->keyword.args[2]);
  if (LLVMGetTypeKind(LLVMTypeOf(buf)) != LLVMIntegerTypeKind)
    buf = LLVMBuildPtrToInt(ctx->builder, buf, i64, "b");
  if (LLVMGetTypeKind(LLVMTypeOf(fd)) != LLVMIntegerTypeKind)
    fd = LLVMBuildPtrToInt(ctx->builder, fd, i64, "f");
  if (LLVMGetTypeKind(LLVMTypeOf(len)) != LLVMIntegerTypeKind)
    len = LLVMBuildPtrToInt(ctx->builder, len, i64, "l");
  return lll_syscall(ctx, 1, fd, buf, len);
}

static LLVMValueRef kio_read(CodeGenContext *ctx, FILE *out, ASTNode *node) {
  (void)out;
  LLVMTypeRef i64 = LLVMInt64TypeInContext(ctx->llvm_ctx);
  LLVMValueRef fd = codegen_expr(ctx, node->keyword.args[0]);
  LLVMValueRef buf = codegen_expr(ctx, node->keyword.args[1]);
  LLVMValueRef len = codegen_expr(ctx, node->keyword.args[2]);
  if (LLVMGetTypeKind(LLVMTypeOf(buf)) != LLVMIntegerTypeKind)
    buf = LLVMBuildPtrToInt(ctx->builder, buf, i64, "b");
  if (LLVMGetTypeKind(LLVMTypeOf(fd)) != LLVMIntegerTypeKind)
    fd = LLVMBuildPtrToInt(ctx->builder, fd, i64, "f");
  if (LLVMGetTypeKind(LLVMTypeOf(len)) != LLVMIntegerTypeKind)
    len = LLVMBuildPtrToInt(ctx->builder, len, i64, "l");
  return lll_syscall(ctx, 0, fd, buf, len);
}

static LLVMValueRef kio_open(CodeGenContext *ctx, FILE *out, ASTNode *node) {
  (void)out;
  LLVMTypeRef i64 = LLVMInt64TypeInContext(ctx->llvm_ctx);
  LLVMValueRef path = codegen_expr(ctx, node->keyword.args[0]);
  LLVMValueRef flags = codegen_expr(ctx, node->keyword.args[1]);
  if (LLVMGetTypeKind(LLVMTypeOf(path)) != LLVMIntegerTypeKind)
    path = LLVMBuildPtrToInt(ctx->builder, path, i64, "p");
  if (LLVMGetTypeKind(LLVMTypeOf(flags)) != LLVMIntegerTypeKind)
    flags = LLVMBuildPtrToInt(ctx->builder, flags, i64, "f");
  LLVMValueRef mode = LLVMConstInt(i64, 0644, 0);
  return lll_syscall(ctx, 2, path, flags, mode);
}

static LLVMValueRef kio_close(CodeGenContext *ctx, FILE *out, ASTNode *node) {
  (void)out;
  LLVMTypeRef i64 = LLVMInt64TypeInContext(ctx->llvm_ctx);
  LLVMValueRef fd = codegen_expr(ctx, node->keyword.args[0]);
  if (LLVMGetTypeKind(LLVMTypeOf(fd)) != LLVMIntegerTypeKind)
    fd = LLVMBuildPtrToInt(ctx->builder, fd, i64, "f");
  return lll_syscall(ctx, 3, fd, NULL, NULL);
}

static LLVMValueRef kio_exit(CodeGenContext *ctx, FILE *out, ASTNode *node) {
  (void)out;
  LLVMTypeRef i64 = LLVMInt64TypeInContext(ctx->llvm_ctx);
  LLVMValueRef code = codegen_expr(ctx, node->keyword.args[0]);
  if (LLVMGetTypeKind(LLVMTypeOf(code)) != LLVMIntegerTypeKind)
    code = LLVMBuildPtrToInt(ctx->builder, code, i64, "c");
  return lll_syscall(ctx, 60, code, NULL, NULL);
}

static LLVMValueRef kio_mkdir(CodeGenContext *ctx, FILE *out, ASTNode *node) {
  (void)out;
  LLVMTypeRef i64 = LLVMInt64TypeInContext(ctx->llvm_ctx);
  LLVMValueRef path = codegen_expr(ctx, node->keyword.args[0]);
  LLVMValueRef mode = codegen_expr(ctx, node->keyword.args[1]);
  if (LLVMGetTypeKind(LLVMTypeOf(path)) != LLVMIntegerTypeKind)
    path = LLVMBuildPtrToInt(ctx->builder, path, i64, "p");
  if (LLVMGetTypeKind(LLVMTypeOf(mode)) != LLVMIntegerTypeKind)
    mode = LLVMBuildPtrToInt(ctx->builder, mode, i64, "m");
  return lll_syscall(ctx, 83, path, mode, NULL);
}

static LLVMValueRef kio_rmdir(CodeGenContext *ctx, FILE *out, ASTNode *node) {
  (void)out;
  LLVMTypeRef i64 = LLVMInt64TypeInContext(ctx->llvm_ctx);
  LLVMValueRef path = codegen_expr(ctx, node->keyword.args[0]);
  if (LLVMGetTypeKind(LLVMTypeOf(path)) != LLVMIntegerTypeKind)
    path = LLVMBuildPtrToInt(ctx->builder, path, i64, "p");
  return lll_syscall(ctx, 84, path, NULL, NULL);
}

static LLVMValueRef kio_unlink(CodeGenContext *ctx, FILE *out, ASTNode *node) {
  (void)out;
  LLVMTypeRef i64 = LLVMInt64TypeInContext(ctx->llvm_ctx);
  LLVMValueRef path = codegen_expr(ctx, node->keyword.args[0]);
  if (LLVMGetTypeKind(LLVMTypeOf(path)) != LLVMIntegerTypeKind)
    path = LLVMBuildPtrToInt(ctx->builder, path, i64, "p");
  return lll_syscall(ctx, 87, path, NULL, NULL);
}

static LLVMValueRef kio_rename(CodeGenContext *ctx, FILE *out, ASTNode *node) {
  (void)out;
  LLVMTypeRef i64 = LLVMInt64TypeInContext(ctx->llvm_ctx);
  LLVMValueRef from = codegen_expr(ctx, node->keyword.args[0]);
  LLVMValueRef to = codegen_expr(ctx, node->keyword.args[1]);
  if (LLVMGetTypeKind(LLVMTypeOf(from)) != LLVMIntegerTypeKind)
    from = LLVMBuildPtrToInt(ctx->builder, from, i64, "f");
  if (LLVMGetTypeKind(LLVMTypeOf(to)) != LLVMIntegerTypeKind)
    to = LLVMBuildPtrToInt(ctx->builder, to, i64, "t");
  return lll_syscall(ctx, 82, from, to, NULL);
}

static LLVMValueRef kio_chdir(CodeGenContext *ctx, FILE *out, ASTNode *node) {
  (void)out;
  LLVMTypeRef i64 = LLVMInt64TypeInContext(ctx->llvm_ctx);
  LLVMValueRef path = codegen_expr(ctx, node->keyword.args[0]);
  if (LLVMGetTypeKind(LLVMTypeOf(path)) != LLVMIntegerTypeKind)
    path = LLVMBuildPtrToInt(ctx->builder, path, i64, "p");
  return lll_syscall(ctx, 80, path, NULL, NULL);
}

static LLVMValueRef kio_puts(CodeGenContext *ctx, FILE *out, ASTNode *node) {
  (void)out;
  LLVMTypeRef i8p = LLVMPointerType(LLVMInt8TypeInContext(ctx->llvm_ctx), 0);
  LLVMTypeRef i32 = LLVMInt32TypeInContext(ctx->llvm_ctx);
  LLVMTypeRef ft = LLVMFunctionType(i32, (LLVMTypeRef[]){i8p}, 1, 0);
  LLVMValueRef fn = LLVMGetNamedFunction(ctx->module, "puts");
  if (!fn)
    fn = LLVMAddFunction(ctx->module, "puts", ft);
  LLVMValueRef val = codegen_expr(ctx, node->keyword.args[0]);
  if (LLVMGetTypeKind(LLVMTypeOf(val)) == LLVMIntegerTypeKind)
    val = LLVMBuildIntToPtr(ctx->builder, val, i8p, "cast");
  LLVMValueRef args[] = {val};
  return LLVMBuildCall2(ctx->builder, ft, fn, args, 1, "puts");
}

static KeywordHandler io_handlers[] = {
    {"io.write", kio_write, NULL, 3},   {"io.read", kio_read, NULL, 3},
    {"io.open", kio_open, NULL, 2},     {"io.close", kio_close, NULL, 1},
    {"io.exit", kio_exit, NULL, 1},     {"io.mkdir", kio_mkdir, NULL, 2},
    {"io.rmdir", kio_rmdir, NULL, 1},   {"io.unlink", kio_unlink, NULL, 1},
    {"io.rename", kio_rename, NULL, 2}, {"io.chdir", kio_chdir, NULL, 1},
    {"io.puts", kio_puts, NULL, 1},     {NULL, NULL, NULL, 0},
};

void register_io_keywords(void) {
  for (int i = 0; io_handlers[i].name; i++) {
    register_keyword_handler(&io_handlers[i]);
  }
}
