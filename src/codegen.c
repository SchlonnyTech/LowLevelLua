#include "codegen.h"
#include "keywords.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool llvm_initialized = false;
static pthread_mutex_t llvm_init_mutex = PTHREAD_MUTEX_INITIALIZER;

static TypeCacheEntry **type_cache = NULL;
static int type_cache_size = 0;
static StringPoolEntry **string_pool_hash = NULL;
static int string_pool_hash_size = 0;

static unsigned long hash_string(const char *str) {
  unsigned long hash = 5381;
  int c;
  while ((c = *str++)) {
    hash = ((hash << 5) + hash) + c;
  }
  return hash;
}

static void init_llvm_once(void) {
  pthread_mutex_lock(&llvm_init_mutex);
  if (!llvm_initialized) {
    LLVMInitializeNativeTarget();
    LLVMInitializeNativeAsmPrinter();
    LLVMInitializeNativeAsmParser();
    llvm_initialized = true;
  }
  pthread_mutex_unlock(&llvm_init_mutex);
}

static void init_type_cache(CodeGenContext *ctx) {
  type_cache_size = 256;
  type_cache = calloc(type_cache_size, sizeof(TypeCacheEntry *));
}

static void init_string_pool_hash(CodeGenContext *ctx) {
  string_pool_hash_size = 256;
  string_pool_hash = calloc(string_pool_hash_size, sizeof(StringPoolEntry *));
}

void codegen_init(CodeGenContext *ctx, const char *module_name,
                  BuildType build_type) {
  memset(ctx, 0, sizeof(*ctx));
  ctx->build_type = build_type;
  ctx->int_format = NULL;
  ctx->str_format = NULL;

  init_llvm_once();
  init_type_cache(ctx);
  init_string_pool_hash(ctx);

  ctx->llvm_ctx = LLVMContextCreate();
  ctx->module = LLVMModuleCreateWithNameInContext(module_name, ctx->llvm_ctx);
  ctx->builder = LLVMCreateBuilderInContext(ctx->llvm_ctx);

  ctx->target_triple = LLVMGetDefaultTargetTriple();
  LLVMSetTarget(ctx->module, ctx->target_triple);

  char *cpu = LLVMGetHostCPUName();
  ctx->cpu_name = cpu ? cpu : strdup("generic");

  if (build_type == BUILD_RELEASE) {
    ctx->cpu_features = LLVMGetHostCPUFeatures();
    ctx->optimization_level = LLVMCodeGenLevelAggressive;
  } else {
    ctx->cpu_features = strdup("");
    ctx->optimization_level = LLVMCodeGenLevelNone;
  }

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
  if (ctx->target_machine)
    LLVMDisposeTargetMachine(ctx->target_machine);

  for (int i = 0; i < type_cache_size; i++) {
    TypeCacheEntry *entry = type_cache[i];
    while (entry) {
      TypeCacheEntry *next = entry->next;
      free(entry->key);
      free(entry);
      entry = next;
    }
  }
  free(type_cache);

  for (int i = 0; i < string_pool_hash_size; i++) {
    StringPoolEntry *entry = string_pool_hash[i];
    while (entry) {
      StringPoolEntry *next = entry->next;
      free(entry->key);
      free(entry);
      entry = next;
    }
  }
  free(string_pool_hash);

  if (ctx->functions.names) {
    for (int i = 0; i < ctx->functions.count; i++) {
      free(ctx->functions.names[i]);
    }
    free(ctx->functions.names);
    free(ctx->functions.functions);
    free(ctx->functions.types);
    free(ctx->functions.builtin_types);
    free(ctx->functions.arg_counts);
  }
}

LLVMTypeRef codegen_type_from_string(CodeGenContext *ctx,
                                     const char *type_name) {
  if (!type_name)
    return LLVMInt64TypeInContext(ctx->llvm_ctx);

  unsigned long hash = hash_string(type_name) % type_cache_size;
  TypeCacheEntry *entry = type_cache[hash];

  while (entry) {
    if (strcmp(entry->key, type_name) == 0) {
      return entry->type;
    }
    entry = entry->next;
  }

  LLVMTypeRef type;
  if (strcmp(type_name, "void") == 0)
    type = LLVMVoidTypeInContext(ctx->llvm_ctx);
  else if (strcmp(type_name, "bool") == 0 || strcmp(type_name, "boolean") == 0)
    type = LLVMInt1TypeInContext(ctx->llvm_ctx);
  else if (strcmp(type_name, "int") == 0 || strcmp(type_name, "int64") == 0 ||
           strcmp(type_name, "i64") == 0 || strcmp(type_name, "number") == 0)
    type = LLVMInt64TypeInContext(ctx->llvm_ctx);
  else if (strcmp(type_name, "int32") == 0 || strcmp(type_name, "i32") == 0)
    type = LLVMInt32TypeInContext(ctx->llvm_ctx);
  else if (strcmp(type_name, "double") == 0 || strcmp(type_name, "f64") == 0 ||
           strcmp(type_name, "float64") == 0)
    type = LLVMDoubleTypeInContext(ctx->llvm_ctx);
  else if (strcmp(type_name, "float") == 0 || strcmp(type_name, "f32") == 0 ||
           strcmp(type_name, "float32") == 0)
    type = LLVMFloatTypeInContext(ctx->llvm_ctx);
  else if (strcmp(type_name, "string") == 0 || strcmp(type_name, "str") == 0)
    type = LLVMPointerType(LLVMInt8TypeInContext(ctx->llvm_ctx), 0);
  else if (strcmp(type_name, "uint8") == 0 || strcmp(type_name, "u8") == 0)
    type = LLVMInt8TypeInContext(ctx->llvm_ctx);
  else if (strcmp(type_name, "uint64") == 0 || strcmp(type_name, "u64") == 0)
    type = LLVMInt64TypeInContext(ctx->llvm_ctx);
  else if (strcmp(type_name, "any") == 0 || strcmp(type_name, "thread") == 0)
    type = LLVMPointerType(LLVMInt8TypeInContext(ctx->llvm_ctx), 0);
  else {
    for (int i = 0; i < ctx->struct_types.count; i++) {
      if (strcmp(ctx->struct_types.names[i], type_name) == 0) {
        type = LLVMPointerType(ctx->struct_types.types[i], 0);
        entry = malloc(sizeof(TypeCacheEntry));
        entry->key = strdup(type_name);
        entry->type = type;
        entry->next = type_cache[hash];
        type_cache[hash] = entry;
        return type;
      }
    }
    type = LLVMInt64TypeInContext(ctx->llvm_ctx);
  }

  entry = malloc(sizeof(TypeCacheEntry));
  entry->key = strdup(type_name);
  entry->type = type;
  entry->next = type_cache[hash];
  type_cache[hash] = entry;

  return type;
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
  unsigned long hash = hash_string(str) % string_pool_hash_size;
  StringPoolEntry *entry = string_pool_hash[hash];

  while (entry) {
    if (strcmp(entry->key, str) == 0) {
      return entry->value;
    }
    entry = entry->next;
  }

  LLVMValueRef global = LLVMBuildGlobalStringPtr(ctx->builder, str, "str");

  if (ctx->verbose)
    fprintf(stderr, "DEBUG: string created\n");

  entry = malloc(sizeof(StringPoolEntry));
  entry->key = strdup(str);
  entry->value = global;
  entry->next = string_pool_hash[hash];
  string_pool_hash[hash] = entry;

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

  if (!ctx->target_machine) {
    if (LLVMGetTargetFromTriple(ctx->target_triple, &target, &error) != 0) {
      codegen_error(ctx, "Failed to get target: %s", error ? error : "unknown");
      if (error)
        LLVMDisposeMessage(error);
      return false;
    }

    ctx->target_machine = LLVMCreateTargetMachine(
        target, ctx->target_triple, ctx->cpu_name, ctx->cpu_features,
        ctx->optimization_level, LLVMRelocDefault, LLVMCodeModelDefault);
  }

  if (!ctx->target_machine) {
    codegen_error(ctx, "Failed to create target machine");
    return false;
  }

  if (ctx->build_type == BUILD_DEBUG && !ctx->module_verified) {
    char *verify_error = NULL;
    if (LLVMVerifyModule(ctx->module, LLVMReturnStatusAction, &verify_error)) {
      codegen_error(ctx, "Module verification failed: %s",
                    verify_error ? verify_error : "unknown error");
      if (verify_error)
        LLVMDisposeMessage(verify_error);
      return false;
    }
    if (verify_error)
      LLVMDisposeMessage(verify_error);
    ctx->module_verified = true;
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
