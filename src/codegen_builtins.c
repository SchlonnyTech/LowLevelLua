#include "codegen.h"
#include <stdlib.h>
#include <string.h>

static void builtins_add(CodeGenContext *ctx, const char *name, int type) {
  if (ctx->builtins.count >= ctx->builtins.capacity) {
    int nc = ctx->builtins.capacity ? ctx->builtins.capacity * 2 : 32;
    ctx->builtins.names =
        realloc(ctx->builtins.names, nc * sizeof(const char *));
    ctx->builtins.types = realloc(ctx->builtins.types, nc * sizeof(int));
    ctx->builtins.capacity = nc;
  }
  ctx->builtins.names[ctx->builtins.count] = name;
  ctx->builtins.types[ctx->builtins.count] = type;
  ctx->builtins.count++;
}

void llvm_register_string_builtins(CodeGenContext *ctx) {
  LLVMTypeRef i8p = LLVMPointerType(LLVMInt8TypeInContext(ctx->llvm_ctx), 0);
  LLVMTypeRef i64 = LLVMInt64TypeInContext(ctx->llvm_ctx);
  LLVMTypeRef f64 = LLVMDoubleTypeInContext(ctx->llvm_ctx);

  LLVMTypeRef atoll_type = LLVMFunctionType(i64, (LLVMTypeRef[]){i8p}, 1, 0);
  LLVMAddFunction(ctx->module, "atoll", atoll_type);

  LLVMTypeRef atof_type = LLVMFunctionType(f64, (LLVMTypeRef[]){i8p}, 1, 0);
  LLVMAddFunction(ctx->module, "atof", atof_type);
}

void llvm_register_builtins(CodeGenContext *ctx) {
  LLVMTypeRef i8_ptr = LLVMPointerType(LLVMInt8TypeInContext(ctx->llvm_ctx), 0);
  LLVMTypeRef i32 = LLVMInt32TypeInContext(ctx->llvm_ctx);
  LLVMTypeRef i64 = LLVMInt64TypeInContext(ctx->llvm_ctx);
  LLVMTypeRef f64 = LLVMDoubleTypeInContext(ctx->llvm_ctx);

  LLVMTypeRef puts_params[] = {i8_ptr};
  LLVMTypeRef puts_type = LLVMFunctionType(i32, puts_params, 1, 0);
  LLVMValueRef puts_func = LLVMAddFunction(ctx->module, "puts", puts_type);

  codegen_func_map_add(ctx, "print_str", puts_func, puts_type);
  builtins_add(ctx, "print_str", BUILTIN_PRINT);

  codegen_func_map_add(ctx, "print", puts_func, puts_type);
  builtins_add(ctx, "print", BUILTIN_PRINT);

  LLVMTypeRef sqrt_params[] = {f64};
  LLVMTypeRef sqrt_type = LLVMFunctionType(f64, sqrt_params, 1, 0);
  LLVMValueRef sqrt_func = LLVMAddFunction(ctx->module, "sqrt", sqrt_type);

  codegen_func_map_add(ctx, "sqrt", sqrt_func, sqrt_type);
  builtins_add(ctx, "sqrt", BUILTIN_MATH);

  LLVMTypeRef strlen_params[] = {i8_ptr};
  LLVMTypeRef strlen_type = LLVMFunctionType(i64, strlen_params, 1, 0);
  LLVMValueRef strlen_func =
      LLVMAddFunction(ctx->module, "strlen", strlen_type);

  codegen_func_map_add(ctx, "strlen", strlen_func, strlen_type);
  builtins_add(ctx, "strlen", BUILTIN_STRING);

  LLVMTypeRef malloc_params[] = {i64};
  LLVMTypeRef malloc_type = LLVMFunctionType(i8_ptr, malloc_params, 1, 0);
  LLVMValueRef malloc_func =
      LLVMAddFunction(ctx->module, "malloc", malloc_type);

  codegen_func_map_add(ctx, "malloc", malloc_func, malloc_type);
  builtins_add(ctx, "malloc", BUILTIN_MEMORY);

  builtins_add(ctx, "min", BUILTIN_CUSTOM);
  builtins_add(ctx, "max", BUILTIN_CUSTOM);
  builtins_add(ctx, "abs", BUILTIN_CUSTOM);

  llvm_register_string_builtins(ctx);
}

int llvm_get_builtin_type(CodeGenContext *ctx, const char *name) {
  for (int i = 0; i < ctx->builtins.count; i++) {
    if (ctx->builtins.names[i] == name ||
        strcmp(ctx->builtins.names[i], name) == 0)
      return ctx->builtins.types[i];
  }
  return -1;
}

LLVMValueRef codegen_syscall(CodeGenContext *ctx, int syscall_num,
                             LLVMValueRef *args, int arg_count) {
  LLVMTypeRef i64 = LLVMInt64TypeInContext(ctx->llvm_ctx);

  LLVMTypeRef syscall_type = LLVMFunctionType(
      i64, (LLVMTypeRef[]){i64, i64, i64, i64, i64, i64}, 6, 0);
  LLVMValueRef syscall_func = LLVMGetNamedFunction(ctx->module, "syscall");
  if (!syscall_func)
    syscall_func = LLVMAddFunction(ctx->module, "syscall", syscall_type);

  LLVMValueRef syscall_args[6] = {0};
  syscall_args[0] = LLVMConstInt(i64, syscall_num, 0);

  for (int i = 0; i < arg_count && i < 5; i++)
    syscall_args[i + 1] = args[i] ? args[i] : LLVMConstInt(i64, 0, 0);

  for (int i = arg_count + 1; i < 6; i++)
    syscall_args[i] = LLVMConstInt(i64, 0, 0);

  return LLVMBuildCall2(ctx->builder, syscall_type, syscall_func, syscall_args,
                        6, "syscall");
}
