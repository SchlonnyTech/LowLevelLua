#include "codegen.h"
#include <stdlib.h>
#include <string.h>
void llvm_register_builtins(CodeGenContext *ctx) {
  ctx->functions.capacity = 256;
  ctx->functions.names = calloc(ctx->functions.capacity, sizeof(char *));
  ctx->functions.functions =
      calloc(ctx->functions.capacity, sizeof(LLVMValueRef));
  ctx->functions.types = calloc(ctx->functions.capacity, sizeof(LLVMTypeRef));
  ctx->functions.builtin_types = calloc(ctx->functions.capacity, sizeof(int));
  ctx->functions.arg_counts = calloc(ctx->functions.capacity, sizeof(int));
  ctx->functions.count = 0;

  LLVMTypeRef i8_ptr = LLVMPointerType(LLVMInt8TypeInContext(ctx->llvm_ctx), 0);
  LLVMTypeRef i32 = LLVMInt32TypeInContext(ctx->llvm_ctx);
  LLVMTypeRef i64 = LLVMInt64TypeInContext(ctx->llvm_ctx);
  LLVMTypeRef f64 = LLVMDoubleTypeInContext(ctx->llvm_ctx);

  LLVMTypeRef puts_params[] = {i8_ptr};
  LLVMTypeRef puts_type = LLVMFunctionType(i32, puts_params, 1, 0);
  LLVMValueRef puts_func = LLVMAddFunction(ctx->module, "puts", puts_type);

  ctx->functions.names[ctx->functions.count] = strdup("print_str");
  ctx->functions.functions[ctx->functions.count] = puts_func;
  ctx->functions.types[ctx->functions.count] = puts_type;
  ctx->functions.builtin_types[ctx->functions.count] = BUILTIN_PRINT;
  ctx->functions.arg_counts[ctx->functions.count] = 1;
  ctx->functions.count++;

  ctx->functions.names[ctx->functions.count] = strdup("print");
  ctx->functions.functions[ctx->functions.count] = puts_func;
  ctx->functions.types[ctx->functions.count] = puts_type;
  ctx->functions.builtin_types[ctx->functions.count] = BUILTIN_PRINT;
  ctx->functions.arg_counts[ctx->functions.count] = 1;
  ctx->functions.count++;

  LLVMTypeRef sqrt_params[] = {f64};
  LLVMTypeRef sqrt_type = LLVMFunctionType(f64, sqrt_params, 1, 0);
  LLVMValueRef sqrt_func = LLVMAddFunction(ctx->module, "sqrt", sqrt_type);

  ctx->functions.names[ctx->functions.count] = strdup("sqrt");
  ctx->functions.functions[ctx->functions.count] = sqrt_func;
  ctx->functions.types[ctx->functions.count] = sqrt_type;
  ctx->functions.builtin_types[ctx->functions.count] = BUILTIN_MATH;
  ctx->functions.arg_counts[ctx->functions.count] = 1;
  ctx->functions.count++;

  LLVMTypeRef strlen_params[] = {i8_ptr};
  LLVMTypeRef strlen_type = LLVMFunctionType(i64, strlen_params, 1, 0);
  LLVMValueRef strlen_func =
      LLVMAddFunction(ctx->module, "strlen", strlen_type);

  ctx->functions.names[ctx->functions.count] = strdup("strlen");
  ctx->functions.functions[ctx->functions.count] = strlen_func;
  ctx->functions.types[ctx->functions.count] = strlen_type;
  ctx->functions.builtin_types[ctx->functions.count] = BUILTIN_STRING;
  ctx->functions.arg_counts[ctx->functions.count] = 1;
  ctx->functions.count++;

  LLVMTypeRef malloc_params[] = {i64};
  LLVMTypeRef malloc_type = LLVMFunctionType(i8_ptr, malloc_params, 1, 0);
  LLVMValueRef malloc_func =
      LLVMAddFunction(ctx->module, "malloc", malloc_type);

  ctx->functions.names[ctx->functions.count] = strdup("malloc");
  ctx->functions.functions[ctx->functions.count] = malloc_func;
  ctx->functions.types[ctx->functions.count] = malloc_type;
  ctx->functions.builtin_types[ctx->functions.count] = BUILTIN_MEMORY;
  ctx->functions.arg_counts[ctx->functions.count] = 1;
  ctx->functions.count++;

  ctx->functions.names[ctx->functions.count] = strdup("min");
  ctx->functions.functions[ctx->functions.count] = NULL;
  ctx->functions.types[ctx->functions.count] = NULL;
  ctx->functions.builtin_types[ctx->functions.count] = BUILTIN_CUSTOM;
  ctx->functions.arg_counts[ctx->functions.count] = 2;
  ctx->functions.count++;

  ctx->functions.names[ctx->functions.count] = strdup("max");
  ctx->functions.functions[ctx->functions.count] = NULL;
  ctx->functions.types[ctx->functions.count] = NULL;
  ctx->functions.builtin_types[ctx->functions.count] = BUILTIN_CUSTOM;
  ctx->functions.arg_counts[ctx->functions.count] = 2;
  ctx->functions.count++;

  ctx->functions.names[ctx->functions.count] = strdup("abs");
  ctx->functions.functions[ctx->functions.count] = NULL;
  ctx->functions.types[ctx->functions.count] = NULL;
  ctx->functions.builtin_types[ctx->functions.count] = BUILTIN_CUSTOM;
  ctx->functions.arg_counts[ctx->functions.count] = 1;
  ctx->functions.count++;
}
int llvm_get_builtin_type(CodeGenContext *ctx, const char *name) {
  for (int i = 0; i < ctx->functions.count; i++) {
    if (strcmp(ctx->functions.names[i], name) == 0) {
      return ctx->functions.builtin_types[i];
    }
  }
  return -1;
}

LLVMValueRef codegen_syscall(CodeGenContext *ctx, int syscall_num,
                             LLVMValueRef *args, int arg_count) {
  LLVMTypeRef i64 = LLVMInt64TypeInContext(ctx->llvm_ctx);

  LLVMTypeRef syscall_type = LLVMFunctionType(
      i64, (LLVMTypeRef[]){i64, i64, i64, i64, i64, i64}, 6, 0);
  LLVMValueRef syscall_func = LLVMGetNamedFunction(ctx->module, "syscall");
  if (!syscall_func) {
    syscall_func = LLVMAddFunction(ctx->module, "syscall", syscall_type);
  }

  LLVMValueRef syscall_args[6] = {0};
  syscall_args[0] = LLVMConstInt(i64, syscall_num, 0);

  for (int i = 0; i < arg_count && i < 5; i++) {
    if (args[i]) {
      syscall_args[i + 1] = args[i];
    } else {
      syscall_args[i + 1] = LLVMConstInt(i64, 0, 0);
    }
  }

  for (int i = arg_count + 1; i < 6; i++) {
    syscall_args[i] = LLVMConstInt(i64, 0, 0);
  }

  return LLVMBuildCall2(ctx->builder, syscall_type, syscall_func, syscall_args,
                        6, "syscall");
}
