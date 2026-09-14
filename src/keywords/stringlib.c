#include "codegen.h"
#include "keywords.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

LLVMValueRef codegen_string_lib(CodeGenContext *ctx, const char *name,
                                ASTNode *expr) {
  LLVMTypeRef i8p = LLVMPointerType(LLVMInt8TypeInContext(ctx->llvm_ctx), 0);
  LLVMTypeRef i64 = LLVMInt64TypeInContext(ctx->llvm_ctx);
  LLVMTypeRef i8 = LLVMInt8TypeInContext(ctx->llvm_ctx);
  LLVMTypeRef i32 = LLVMInt32TypeInContext(ctx->llvm_ctx);
  LLVMTypeRef f64 = LLVMDoubleTypeInContext(ctx->llvm_ctx);

  if (strcmp(name, "string.len") == 0) {
    LLVMValueRef str = codegen_expr(ctx, expr->call.args[0]);
    LLVMTypeRef ft = LLVMFunctionType(i64, (LLVMTypeRef[]){i8p}, 1, 0);
    LLVMValueRef fn = LLVMGetNamedFunction(ctx->module, "strlen");
    if (!fn)
      fn = LLVMAddFunction(ctx->module, "strlen", ft);
    return LLVMBuildCall2(ctx->builder, ft, fn, &str, 1, "len");
  }

  if (strcmp(name, "string.sub") == 0) {
    LLVMValueRef str = codegen_expr(ctx, expr->call.args[0]);
    LLVMValueRef start = codegen_expr(ctx, expr->call.args[1]);
    LLVMValueRef end = codegen_expr(ctx, expr->call.args[2]);

    LLVMTypeRef strlen_type = LLVMFunctionType(i64, (LLVMTypeRef[]){i8p}, 1, 0);
    LLVMValueRef strlen_fn = LLVMGetNamedFunction(ctx->module, "strlen");
    if (!strlen_fn)
      strlen_fn = LLVMAddFunction(ctx->module, "strlen", strlen_type);

    LLVMTypeRef malloc_type = LLVMFunctionType(i8p, (LLVMTypeRef[]){i64}, 1, 0);
    LLVMValueRef malloc_fn = LLVMGetNamedFunction(ctx->module, "malloc");
    if (!malloc_fn)
      malloc_fn = LLVMAddFunction(ctx->module, "malloc", malloc_type);

    LLVMTypeRef memcpy_type =
        LLVMFunctionType(i8p, (LLVMTypeRef[]){i8p, i8p, i64}, 3, 0);
    LLVMValueRef memcpy_fn = LLVMGetNamedFunction(ctx->module, "memcpy");
    if (!memcpy_fn)
      memcpy_fn = LLVMAddFunction(ctx->module, "memcpy", memcpy_type);

    LLVMValueRef len =
        LLVMBuildCall2(ctx->builder, strlen_type, strlen_fn, &str, 1, "len");
    LLVMValueRef zero = LLVMConstInt(i64, 0, 0);
    LLVMValueRef start_cmp =
        LLVMBuildICmp(ctx->builder, LLVMIntSLT, start, zero, "start_lt0");
    LLVMValueRef start_clamped =
        LLVMBuildSelect(ctx->builder, start_cmp, zero, start, "start_c");
    LLVMValueRef end_cmp =
        LLVMBuildICmp(ctx->builder, LLVMIntSGT, end, len, "end_gtlen");
    LLVMValueRef end_clamped =
        LLVMBuildSelect(ctx->builder, end_cmp, len, end, "end_c");
    LLVMValueRef sub_len =
        LLVMBuildSub(ctx->builder, end_clamped, start_clamped, "sub_len");
    LLVMValueRef sub_len_pos =
        LLVMBuildICmp(ctx->builder, LLVMIntSLT, sub_len, zero, "sub_neg");
    LLVMValueRef sub_len_ok =
        LLVMBuildSelect(ctx->builder, sub_len_pos, zero, sub_len, "sub_ok");
    LLVMValueRef size =
        LLVMBuildAdd(ctx->builder, sub_len_ok, LLVMConstInt(i64, 1, 0), "size");

    LLVMValueRef buf =
        LLVMBuildCall2(ctx->builder, malloc_type, malloc_fn, &size, 1, "buf");
    LLVMValueRef src_offset =
        LLVMBuildGEP2(ctx->builder, i8, str, &start_clamped, 1, "src_off");
    LLVMValueRef args[] = {buf, src_offset, sub_len_ok};
    LLVMBuildCall2(ctx->builder, memcpy_type, memcpy_fn, args, 3, "");
    LLVMValueRef end_ptr =
        LLVMBuildGEP2(ctx->builder, i8, buf, &sub_len_ok, 1, "end_ptr");
    LLVMBuildStore(ctx->builder, LLVMConstInt(i8, 0, 0), end_ptr);
    return buf;
  }

  if (strcmp(name, "string.find") == 0) {
    LLVMValueRef str = codegen_expr(ctx, expr->call.args[0]);
    LLVMValueRef needle = codegen_expr(ctx, expr->call.args[1]);
    LLVMTypeRef ft = LLVMFunctionType(i8p, (LLVMTypeRef[]){i8p, i8p}, 2, 0);
    LLVMValueRef fn = LLVMGetNamedFunction(ctx->module, "strstr");
    if (!fn)
      fn = LLVMAddFunction(ctx->module, "strstr", ft);
    LLVMValueRef args[] = {str, needle};
    LLVMValueRef ptr =
        LLVMBuildCall2(ctx->builder, ft, fn, args, 2, "find_ptr");
    LLVMValueRef is_null = LLVMBuildIsNull(ctx->builder, ptr, "is_null");
    LLVMValueRef str_int = LLVMBuildPtrToInt(ctx->builder, str, i64, "si");
    LLVMValueRef res_int = LLVMBuildPtrToInt(ctx->builder, ptr, i64, "ri");
    LLVMValueRef diff = LLVMBuildSub(ctx->builder, res_int, str_int, "diff");
    LLVMValueRef minus_one = LLVMConstInt(i64, -1, 0);
    return LLVMBuildSelect(ctx->builder, is_null, minus_one, diff, "find");
  }

  if (strcmp(name, "string.upper") == 0 || strcmp(name, "string.lower") == 0) {
    int is_upper = strcmp(name, "string.upper") == 0;
    LLVMValueRef str = codegen_expr(ctx, expr->call.args[0]);

    LLVMTypeRef strlen_type = LLVMFunctionType(i64, (LLVMTypeRef[]){i8p}, 1, 0);
    LLVMValueRef strlen_fn = LLVMGetNamedFunction(ctx->module, "strlen");
    if (!strlen_fn)
      strlen_fn = LLVMAddFunction(ctx->module, "strlen", strlen_type);

    LLVMTypeRef malloc_type = LLVMFunctionType(i8p, (LLVMTypeRef[]){i64}, 1, 0);
    LLVMValueRef malloc_fn = LLVMGetNamedFunction(ctx->module, "malloc");
    if (!malloc_fn)
      malloc_fn = LLVMAddFunction(ctx->module, "malloc", malloc_type);

    LLVMValueRef len =
        LLVMBuildCall2(ctx->builder, strlen_type, strlen_fn, &str, 1, "len");
    LLVMValueRef size =
        LLVMBuildAdd(ctx->builder, len, LLVMConstInt(i64, 1, 0), "size");
    LLVMValueRef buf =
        LLVMBuildCall2(ctx->builder, malloc_type, malloc_fn, &size, 1, "buf");

    LLVMValueRef idx = LLVMBuildAlloca(ctx->builder, i64, "i");
    LLVMBuildStore(ctx->builder, LLVMConstInt(i64, 0, 0), idx);

    LLVMBasicBlockRef cond_bb = LLVMAppendBasicBlockInContext(
        ctx->llvm_ctx, ctx->current_func.function, "case_cond");
    LLVMBasicBlockRef body_bb = LLVMAppendBasicBlockInContext(
        ctx->llvm_ctx, ctx->current_func.function, "case_body");
    LLVMBasicBlockRef end_bb = LLVMAppendBasicBlockInContext(
        ctx->llvm_ctx, ctx->current_func.function, "case_end");
    LLVMBuildBr(ctx->builder, cond_bb);

    LLVMPositionBuilderAtEnd(ctx->builder, cond_bb);
    LLVMValueRef i_val = LLVMBuildLoad2(ctx->builder, i64, idx, "i_val");
    LLVMValueRef cond =
        LLVMBuildICmp(ctx->builder, LLVMIntSLT, i_val, len, "loop_c");
    LLVMBuildCondBr(ctx->builder, cond, body_bb, end_bb);

    LLVMPositionBuilderAtEnd(ctx->builder, body_bb);
    LLVMValueRef src_p =
        LLVMBuildGEP2(ctx->builder, i8, str, &i_val, 1, "src_p");
    LLVMValueRef ch = LLVMBuildLoad2(ctx->builder, i8, src_p, "ch");
    LLVMValueRef ch_i64 = LLVMBuildZExt(ctx->builder, ch, i64, "ch64");
    LLVMValueRef cmp =
        LLVMBuildICmp(ctx->builder, LLVMIntSGE, ch_i64,
                      LLVMConstInt(i64, is_upper ? 'a' : 'A', 0), "cmp");
    LLVMValueRef cmp2 =
        LLVMBuildICmp(ctx->builder, LLVMIntSLE, ch_i64,
                      LLVMConstInt(i64, is_upper ? 'z' : 'Z', 0), "cmp2");
    LLVMValueRef both = LLVMBuildAnd(ctx->builder, cmp, cmp2, "both");
    LLVMValueRef offset = LLVMConstInt(i64, 32, 0);
    LLVMValueRef shifted =
        is_upper ? LLVMBuildSub(ctx->builder, ch_i64, offset, "s")
                 : LLVMBuildAdd(ctx->builder, ch_i64, offset, "s");
    LLVMValueRef new_ch_i64 =
        LLVMBuildSelect(ctx->builder, both, shifted, ch_i64, "new64");
    LLVMValueRef new_ch = LLVMBuildTrunc(ctx->builder, new_ch_i64, i8, "new8");
    LLVMValueRef dst_p =
        LLVMBuildGEP2(ctx->builder, i8, buf, &i_val, 1, "dst_p");
    LLVMBuildStore(ctx->builder, new_ch, dst_p);
    LLVMValueRef next =
        LLVMBuildAdd(ctx->builder, i_val, LLVMConstInt(i64, 1, 0), "next");
    LLVMBuildStore(ctx->builder, next, idx);
    LLVMBuildBr(ctx->builder, cond_bb);

    LLVMPositionBuilderAtEnd(ctx->builder, end_bb);
    LLVMValueRef end_p = LLVMBuildGEP2(ctx->builder, i8, buf, &len, 1, "end_p");
    LLVMBuildStore(ctx->builder, LLVMConstInt(i8, 0, 0), end_p);
    return buf;
  }

  if (strcmp(name, "string.byte") == 0) {
    LLVMValueRef str = codegen_expr(ctx, expr->call.args[0]);
    LLVMValueRef idx = codegen_expr(ctx, expr->call.args[1]);
    LLVMValueRef ptr = LLVMBuildGEP2(ctx->builder, i8, str, &idx, 1, "bp");
    LLVMValueRef byte = LLVMBuildLoad2(ctx->builder, i8, ptr, "byte");
    return LLVMBuildZExt(ctx->builder, byte, i64, "byte_int");
  }

  if (strcmp(name, "string.char") == 0) {
    LLVMValueRef n = codegen_expr(ctx, expr->call.args[0]);
    LLVMTypeRef mt = LLVMFunctionType(i8p, (LLVMTypeRef[]){i64}, 1, 0);
    LLVMValueRef mf = LLVMGetNamedFunction(ctx->module, "malloc");
    if (!mf)
      mf = LLVMAddFunction(ctx->module, "malloc", mt);
    LLVMValueRef two = LLVMConstInt(i64, 2, 0);
    LLVMValueRef buf = LLVMBuildCall2(ctx->builder, mt, mf, &two, 1, "charbuf");
    LLVMValueRef byte = LLVMBuildTrunc(ctx->builder, n, i8, "byte");
    LLVMValueRef zero = LLVMConstInt(i64, 0, 0);
    LLVMValueRef one = LLVMConstInt(i64, 1, 0);
    LLVMValueRef p0 = LLVMBuildGEP2(ctx->builder, i8, buf, &zero, 1, "p0");
    LLVMValueRef p1 = LLVMBuildGEP2(ctx->builder, i8, buf, &one, 1, "p1");
    LLVMBuildStore(ctx->builder, byte, p0);
    LLVMBuildStore(ctx->builder, LLVMConstInt(i8, 0, 0), p1);
    return buf;
  }

  if (strcmp(name, "string.to_int") == 0) {
    LLVMValueRef str = codegen_expr(ctx, expr->call.args[0]);
    LLVMTypeRef ft = LLVMFunctionType(i64, (LLVMTypeRef[]){i8p}, 1, 0);
    LLVMValueRef fn = LLVMGetNamedFunction(ctx->module, "atoll");
    if (!fn)
      fn = LLVMAddFunction(ctx->module, "atoll", ft);
    return LLVMBuildCall2(ctx->builder, ft, fn, &str, 1, "to_int");
  }

  if (strcmp(name, "string.to_float") == 0) {
    LLVMValueRef str = codegen_expr(ctx, expr->call.args[0]);
    LLVMTypeRef ft = LLVMFunctionType(f64, (LLVMTypeRef[]){i8p}, 1, 0);
    LLVMValueRef fn = LLVMGetNamedFunction(ctx->module, "atof");
    if (!fn)
      fn = LLVMAddFunction(ctx->module, "atof", ft);
    return LLVMBuildCall2(ctx->builder, ft, fn, &str, 1, "to_float");
  }

  if (strcmp(name, "string.format") == 0) {
    int n = expr->call.arg_count;
    LLVMValueRef *args = malloc(sizeof(LLVMValueRef) * n);
    for (int i = 0; i < n; i++)
      args[i] = codegen_expr(ctx, expr->call.args[i]);

    LLVMTypeRef snprintf_type =
        LLVMFunctionType(i32, (LLVMTypeRef[]){i8p, i64, i8p}, 3, true);
    LLVMValueRef snprintf_func = LLVMGetNamedFunction(ctx->module, "snprintf");
    if (!snprintf_func)
      snprintf_func = LLVMAddFunction(ctx->module, "snprintf", snprintf_type);

    LLVMValueRef null_ptr = LLVMConstNull(i8p);
    LLVMValueRef zero = LLVMConstInt(i64, 0, 0);
    LLVMValueRef size_args[] = {null_ptr, zero, args[0]};
    LLVMValueRef needed = LLVMBuildCall2(ctx->builder, snprintf_type,
                                         snprintf_func, size_args, 3, "needed");
    LLVMValueRef size64 = LLVMBuildSExt(ctx->builder, needed, i64, "size64");
    LLVMValueRef one = LLVMConstInt(i64, 1, 0);
    LLVMValueRef size = LLVMBuildAdd(ctx->builder, size64, one, "size");

    LLVMTypeRef mt = LLVMFunctionType(i8p, (LLVMTypeRef[]){i64}, 1, 0);
    LLVMValueRef mf = LLVMGetNamedFunction(ctx->module, "malloc");
    if (!mf)
      mf = LLVMAddFunction(ctx->module, "malloc", mt);
    LLVMValueRef buf = LLVMBuildCall2(ctx->builder, mt, mf, &size, 1, "buf");

    LLVMValueRef *final_args = malloc(sizeof(LLVMValueRef) * (n + 2));
    final_args[0] = buf;
    final_args[1] = size;
    for (int i = 0; i < n; i++)
      final_args[i + 2] = args[i];
    LLVMBuildCall2(ctx->builder, snprintf_type, snprintf_func, final_args,
                   n + 2, "formatted");

    free(args);
    free(final_args);
    return buf;
  }

  codegen_error(ctx, "Unknown string function: %s", name);
  return LLVMConstNull(i8p);
}
