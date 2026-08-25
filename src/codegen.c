#include "codegen.h"
#include "keywords.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void codegen_init(CodeGenContext *ctx, const char *module_name) {
  memset(ctx, 0, sizeof(*ctx));
  ctx->int_format = NULL;
  ctx->str_format = NULL;
  LLVMInitializeNativeTarget();
  LLVMInitializeNativeAsmPrinter();
  LLVMInitializeNativeAsmParser();

  ctx->llvm_ctx = LLVMContextCreate();
  ctx->module = LLVMModuleCreateWithNameInContext(module_name, ctx->llvm_ctx);
  ctx->builder = LLVMCreateBuilderInContext(ctx->llvm_ctx);

  ctx->target_triple = LLVMGetDefaultTargetTriple();
  LLVMSetTarget(ctx->module, ctx->target_triple);

  char *cpu = LLVMGetHostCPUName();
  ctx->cpu_name = cpu ? cpu : strdup("generic");
  ctx->cpu_features = LLVMGetHostCPUFeatures();

  ctx->module_name = module_name;
  ctx->temp_counter = 0;
  ctx->block_counter = 0;

  llvm_register_builtins(ctx);
  init_keywords();
  create_lll_syscall(ctx);
}

void codegen_destroy(CodeGenContext *ctx) {
  if (ctx->builder)
    LLVMDisposeBuilder(ctx->builder);
  if (ctx->module)
    LLVMDisposeModule(ctx->module);
  if (ctx->llvm_ctx)
    LLVMContextDispose(ctx->llvm_ctx);
  if (ctx->target_triple)
    free(ctx->target_triple);
  if (ctx->cpu_name)
    free(ctx->cpu_name);
  if (ctx->cpu_features)
    free(ctx->cpu_features);
}

LLVMTypeRef codegen_type_from_string(CodeGenContext *ctx,
                                     const char *type_name) {
  if (!type_name)
    return LLVMInt64TypeInContext(ctx->llvm_ctx);

  if (strcmp(type_name, "void") == 0)
    return LLVMVoidTypeInContext(ctx->llvm_ctx);
  if (strcmp(type_name, "bool") == 0)
    return LLVMInt1TypeInContext(ctx->llvm_ctx);
  if (strcmp(type_name, "int") == 0 || strcmp(type_name, "int64") == 0 ||
      strcmp(type_name, "i64") == 0)
    return LLVMInt64TypeInContext(ctx->llvm_ctx);
  if (strcmp(type_name, "int32") == 0 || strcmp(type_name, "i32") == 0)
    return LLVMInt32TypeInContext(ctx->llvm_ctx);
  if (strcmp(type_name, "double") == 0 || strcmp(type_name, "f64") == 0)
    return LLVMDoubleTypeInContext(ctx->llvm_ctx);
  if (strcmp(type_name, "float") == 0 || strcmp(type_name, "f32") == 0)
    return LLVMFloatTypeInContext(ctx->llvm_ctx);
  if (strcmp(type_name, "string") == 0 || strcmp(type_name, "str") == 0)
    return LLVMPointerType(LLVMInt8TypeInContext(ctx->llvm_ctx), 0);

  for (int i = 0; i < ctx->struct_types.count; i++) {
    if (strcmp(ctx->struct_types.names[i], type_name) == 0) {
      return LLVMPointerType(ctx->struct_types.types[i], 0);
    }
  }

  return LLVMInt64TypeInContext(ctx->llvm_ctx);
}

LLVMTypeRef codegen_type_from_node(CodeGenContext *ctx, ASTNode *type_node) {
  if (!type_node || type_node->type != NODE_TYPE_ANNOTATION) {
    return LLVMInt64TypeInContext(ctx->llvm_ctx);
  }

  LLVMTypeRef base =
      codegen_type_from_string(ctx, type_node->type_annot.type_name);

  for (int i = 0; i < type_node->type_annot.pointer_depth; i++) {
    base = LLVMPointerType(base, 0);
  }

  return base;
}

LLVMTypeRef codegen_infer_type(CodeGenContext *ctx, ASTNode *expr) {
  if (!expr)
    return LLVMInt64TypeInContext(ctx->llvm_ctx);

  switch (expr->type) {
  case NODE_INT_LITERAL:
    return LLVMInt64TypeInContext(ctx->llvm_ctx);
  case NODE_FLOAT_LITERAL:
    return LLVMDoubleTypeInContext(ctx->llvm_ctx);
  case NODE_BOOL_LITERAL:
    return LLVMInt1TypeInContext(ctx->llvm_ctx);
  case NODE_STRING_LITERAL:
    return LLVMPointerType(LLVMInt8TypeInContext(ctx->llvm_ctx), 0);
  default:
    return LLVMInt64TypeInContext(ctx->llvm_ctx);
  }
}

LLVMTypeRef codegen_scope_get_type(CodeGenContext *ctx, const char *name) {
  struct Scope *scope = ctx->current_scope;
  while (scope) {
    for (int i = scope->count - 1; i >= 0; i--) {
      if (strcmp(scope->names[i], name) == 0) {
        return scope->types[i];
      }
    }
    scope = scope->parent;
  }
  return NULL;
}

void codegen_error(CodeGenContext *ctx, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vsnprintf(ctx->error_msg, sizeof(ctx->error_msg), fmt, args);
  va_end(args);
  ctx->has_error = true;
  fprintf(stderr, "Codegen Error: %s\n", ctx->error_msg);
}
LLVMValueRef codegen_string_create(CodeGenContext *ctx, const char *str) {
  for (int i = 0; i < ctx->string_pool.count; i++) {
    if (strcmp(ctx->string_pool.strings[i], str) == 0) {
      return ctx->string_pool.values[i];
    }
  }

  LLVMValueRef global = LLVMBuildGlobalStringPtr(ctx->builder, str, "str");

  if (ctx->verbose)
    fprintf(stderr, "DEBUG: string created\n");

  ctx->string_pool.strings = realloc(
      ctx->string_pool.strings, sizeof(char *) * (ctx->string_pool.count + 1));
  ctx->string_pool.values =
      realloc(ctx->string_pool.values,
              sizeof(LLVMValueRef) * (ctx->string_pool.count + 1));
  ctx->string_pool.strings[ctx->string_pool.count] = strdup(str);
  ctx->string_pool.values[ctx->string_pool.count] = global;
  ctx->string_pool.count++;

  return global;
}

void codegen_scope_push(CodeGenContext *ctx) {
  struct Scope *scope = calloc(1, sizeof(struct Scope));
  scope->capacity = 32;
  scope->names = calloc(scope->capacity, sizeof(char *));
  scope->values = calloc(scope->capacity, sizeof(LLVMValueRef));
  scope->types = calloc(scope->capacity, sizeof(LLVMTypeRef));
  scope->parent = ctx->current_scope;
  ctx->current_scope = scope;
}

void codegen_scope_pop(CodeGenContext *ctx) {
  if (!ctx->current_scope)
    return;

  struct Scope *scope = ctx->current_scope;
  ctx->current_scope = scope->parent;

  for (int i = 0; i < scope->count; i++) {
    free(scope->names[i]);
  }
  free(scope->names);
  free(scope->values);
  free(scope->types);
  free(scope);
}

void codegen_scope_add(CodeGenContext *ctx, const char *name,
                       LLVMValueRef value, LLVMTypeRef type) {
  struct Scope *scope = ctx->current_scope;
  if (!scope) {
    codegen_scope_push(ctx);
    scope = ctx->current_scope;
    ctx->global_scope = scope;
  }

  if (ctx->verbose)
    fprintf(stderr, "DEBUG: scope_add name=%s count=%d\n", name, scope->count);

  if (scope->count >= scope->capacity) {
    scope->capacity *= 2;
    scope->names = realloc(scope->names, scope->capacity * sizeof(char *));
    scope->values =
        realloc(scope->values, scope->capacity * sizeof(LLVMValueRef));
    scope->types = realloc(scope->types, scope->capacity * sizeof(LLVMTypeRef));
  }

  scope->names[scope->count] = strdup(name);
  scope->values[scope->count] = value;
  scope->types[scope->count] = type;
  scope->count++;
}

LLVMValueRef codegen_scope_get(CodeGenContext *ctx, const char *name) {
  struct Scope *scope = ctx->current_scope;

  while (scope) {
    for (int i = scope->count - 1; i >= 0; i--) {
      if (strcmp(scope->names[i], name) == 0) {
        if (ctx->verbose)
          fprintf(stderr, "DEBUG: scope_get found name=%s\n", name);
        return scope->values[i];
      }
    }
    scope = scope->parent;
  }

  if (ctx->verbose)
    fprintf(stderr, "DEBUG: scope_get NOT FOUND name=%s\n", name);
  return NULL;
}
void codegen_loop_push(CodeGenContext *ctx, LLVMBasicBlockRef cont,
                       LLVMBasicBlockRef brk) {
  struct LoopContext *loop = malloc(sizeof(struct LoopContext));
  loop->continue_block = cont;
  loop->break_block = brk;
  loop->parent = ctx->loop_stack;
  ctx->loop_stack = loop;
}

void codegen_loop_pop(CodeGenContext *ctx) {
  if (!ctx->loop_stack)
    return;
  struct LoopContext *loop = ctx->loop_stack;
  ctx->loop_stack = loop->parent;
  free(loop);
}

LLVMBasicBlockRef codegen_get_break_block(CodeGenContext *ctx) {
  return ctx->loop_stack ? ctx->loop_stack->break_block : NULL;
}

LLVMBasicBlockRef codegen_get_continue_block(CodeGenContext *ctx) {
  return ctx->loop_stack ? ctx->loop_stack->continue_block : NULL;
}

bool codegen_compile_to_object(CodeGenContext *ctx, const char *output_file) {
  char *error = NULL;
  LLVMTargetRef target = NULL;

  if (LLVMGetTargetFromTriple(ctx->target_triple, &target, &error) != 0) {
    codegen_error(ctx, "Failed to get target: %s", error ? error : "unknown");
    if (error)
      LLVMDisposeMessage(error);
    return false;
  }

  ctx->target_machine = LLVMCreateTargetMachine(
      target, ctx->target_triple, ctx->cpu_name, ctx->cpu_features,
      LLVMCodeGenLevelDefault, LLVMRelocDefault, LLVMCodeModelDefault);

  if (!ctx->target_machine) {
    codegen_error(ctx, "Failed to create target machine");
    return false;
  }

  if (LLVMTargetMachineEmitToFile(ctx->target_machine, ctx->module,
                                  (char *)output_file, LLVMObjectFile,
                                  &error) != 0) {
    codegen_error(ctx, "Failed to emit object file: %s",
                  error ? error : "unknown error");
    if (error)
      LLVMDisposeMessage(error);
    return false;
  }

  return true;
}
