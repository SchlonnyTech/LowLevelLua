#include "codegen.h"
#include "keywords.h"
#include <llvm-c/Transforms/PassBuilder.h>
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
  unsigned long h = 5381;
  int c;
  while ((c = *str++))
    h = ((h << 5) + h) + (unsigned char)c;
  return h;
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
  (void)ctx;
  type_cache_size = 256;
  type_cache = calloc(type_cache_size, sizeof(TypeCacheEntry *));
}

static void init_string_pool_hash(CodeGenContext *ctx) {
  (void)ctx;
  string_pool_hash_size = 256;
  string_pool_hash = calloc(string_pool_hash_size, sizeof(StringPoolEntry *));
}

static FieldLookup *field_lookup_build(LLVMTypeRef st, const char **names,
                                       int n) {
  FieldLookup *fl = calloc(1, sizeof(FieldLookup));
  fl->type = st;
  fl->names = names;
  fl->count = n;
  fl->bucket_count = 16;
  while (fl->bucket_count < n * 2)
    fl->bucket_count *= 2;
  fl->buckets = calloc(fl->bucket_count, sizeof(uint32_t));
  fl->next = calloc(n ? n : 1, sizeof(uint32_t));
  for (int i = 0; i < n; i++) {
    unsigned long h = hash_string(names[i]) & (fl->bucket_count - 1);
    fl->next[i] = fl->buckets[h];
    fl->buckets[h] = (uint32_t)(i + 1);
  }
  return fl;
}

int codegen_struct_field_index(CodeGenContext *ctx, LLVMTypeRef struct_type,
                               const char *field_name) {
  for (int i = 0; i < ctx->struct_types.count; i++) {
    if (ctx->struct_types.types[i] != struct_type)
      continue;
    FieldLookup *fl = ctx->struct_types.lookups[i];
    if (!fl)
      return -1;
    unsigned long h = hash_string(field_name) & (fl->bucket_count - 1);
    uint32_t e = fl->buckets[h];
    while (e) {
      int idx = (int)e - 1;
      if (strcmp(fl->names[idx], field_name) == 0)
        return idx;
      e = fl->next[idx];
    }
    return -1;
  }
  return -1;
}

static void func_map_grow_buckets(FuncMap *m) {
  int ns = m->bucket_count ? m->bucket_count * 2 : 64;
  uint32_t *nb = calloc(ns, sizeof(uint32_t));
  for (int i = 0; i < m->count; i++) {
    unsigned long h = hash_string(m->names[i]) & (ns - 1);
    m->next[i] = nb[h];
    nb[h] = (uint32_t)(i + 1);
  }
  free(m->buckets);
  m->buckets = nb;
  m->bucket_count = ns;
}

void codegen_func_map_add(CodeGenContext *ctx, const char *name,
                          LLVMValueRef fn, LLVMTypeRef ft) {
  FuncMap *m = &ctx->func_map;
  if (m->count >= m->capacity) {
    int nc = m->capacity ? m->capacity * 2 : 32;
    m->entries = realloc(m->entries, nc * sizeof(FuncEntry));
    m->names = realloc(m->names, nc * sizeof(const char *));
    m->next = realloc(m->next, nc * sizeof(uint32_t));
    m->capacity = nc;
  }
  m->names[m->count] = name;
  m->entries[m->count].value = fn;
  m->entries[m->count].type = ft;
  m->count++;
  if (!m->bucket_count || m->count * 4 > m->bucket_count * 3)
    func_map_grow_buckets(m);
  else {
    unsigned long h = hash_string(name) & (m->bucket_count - 1);
    m->next[m->count - 1] = m->buckets[h];
    m->buckets[h] = (uint32_t)m->count;
  }
}

FuncEntry *codegen_func_map_find(CodeGenContext *ctx, const char *name) {
  FuncMap *m = &ctx->func_map;
  if (!m->bucket_count)
    return NULL;
  unsigned long h = hash_string(name) & (m->bucket_count - 1);
  uint32_t e = m->buckets[h];
  while (e) {
    int idx = (int)e - 1;
    if (m->names[idx] == name || strcmp(m->names[idx], name) == 0)
      return &m->entries[idx];
    e = m->next[idx];
  }
  return NULL;
}

void codegen_init(CodeGenContext *ctx, const char *module_name,
                  BuildType build_type) {
  memset(ctx, 0, sizeof(*ctx));
  ctx->build_type = build_type;

  init_llvm_once();
  init_type_cache(ctx);
  init_string_pool_hash(ctx);

  ctx->llvm_ctx = LLVMContextCreate();
  ctx->module = LLVMModuleCreateWithNameInContext(module_name, ctx->llvm_ctx);
  ctx->builder = LLVMCreateBuilderInContext(ctx->llvm_ctx);

  char *triple = LLVMGetDefaultTargetTriple();
  ctx->target_triple = strdup(triple ? triple : "x86_64-unknown-linux-gnu");
  if (triple)
    LLVMDisposeMessage(triple);

  char *cpu = LLVMGetHostCPUName();
  ctx->cpu_name = strdup(cpu ? cpu : "generic");
  if (cpu)
    LLVMDisposeMessage(cpu);

  if (build_type == BUILD_RELEASE) {
    char *feat = LLVMGetHostCPUFeatures();
    ctx->cpu_features = strdup(feat ? feat : "");
    if (feat)
      LLVMDisposeMessage(feat);
    ctx->optimization_level = LLVMCodeGenLevelAggressive;
  } else {
    ctx->cpu_features = strdup("");
    ctx->optimization_level = LLVMCodeGenLevelNone;
  }

  ctx->module_name = module_name;
  ctx->temp_counter = 0;
  ctx->block_counter = 0;
  ctx->string_counter = 0;

  llvm_register_builtins(ctx);
  init_keywords();
  create_lll_syscall(ctx);
}

void codegen_destroy(CodeGenContext *ctx) {
  if (!ctx)
    return;

  if (ctx->builder) {
    LLVMDisposeBuilder(ctx->builder);
    ctx->builder = NULL;
  }
  if (ctx->module) {
    LLVMDisposeModule(ctx->module);
    ctx->module = NULL;
  }
  if (ctx->llvm_ctx) {
    LLVMContextDispose(ctx->llvm_ctx);
    ctx->llvm_ctx = NULL;
  }

  free(ctx->target_triple);
  free(ctx->cpu_name);
  free(ctx->cpu_features);
  ctx->target_triple = NULL;
  ctx->cpu_name = NULL;
  ctx->cpu_features = NULL;

  if (ctx->target_machine) {
    LLVMDisposeTargetMachine(ctx->target_machine);
    ctx->target_machine = NULL;
  }

  for (int i = 0; i < type_cache_size; i++) {
    TypeCacheEntry *e = type_cache[i];
    while (e) {
      TypeCacheEntry *n = e->next;
      free(e);
      e = n;
    }
  }
  free(type_cache);
  type_cache = NULL;
  type_cache_size = 0;

  for (int i = 0; i < string_pool_hash_size; i++) {
    StringPoolEntry *e = string_pool_hash[i];
    while (e) {
      StringPoolEntry *n = e->next;
      free(e);
      e = n;
    }
  }
  free(string_pool_hash);
  string_pool_hash = NULL;
  string_pool_hash_size = 0;

  free(ctx->func_map.entries);
  free(ctx->func_map.names);
  free(ctx->func_map.buckets);
  free(ctx->func_map.next);
  ctx->func_map.entries = NULL;
  ctx->func_map.names = NULL;
  ctx->func_map.buckets = NULL;
  ctx->func_map.next = NULL;
  ctx->func_map.count = 0;
  ctx->func_map.capacity = 0;
  ctx->func_map.bucket_count = 0;

  free(ctx->builtins.names);
  free(ctx->builtins.types);
  ctx->builtins.names = NULL;
  ctx->builtins.types = NULL;
  ctx->builtins.count = 0;
  ctx->builtins.capacity = 0;

  for (int i = 0; i < ctx->str_cache.count; i++)
    free((void *)ctx->str_cache.keys[i]);
  free(ctx->str_cache.keys);
  free(ctx->str_cache.vals);
  free(ctx->str_cache.buckets);
  free(ctx->str_cache.next);
  ctx->str_cache.keys = NULL;
  ctx->str_cache.vals = NULL;
  ctx->str_cache.buckets = NULL;
  ctx->str_cache.next = NULL;
  ctx->str_cache.count = 0;
  ctx->str_cache.capacity = 0;
  ctx->str_cache.bucket_count = 0;

  for (int i = 0; i < ctx->struct_types.count; i++) {
    FieldLookup *fl =
        ctx->struct_types.lookups ? ctx->struct_types.lookups[i] : NULL;
    if (fl) {
      free(fl->buckets);
      free(fl->next);
      free(fl);
    }
  }
  free(ctx->struct_types.names);
  free(ctx->struct_types.types);
  free(ctx->struct_types.field_counts);
  free(ctx->struct_types.field_names);
  free(ctx->struct_types.lookups);
  ctx->struct_types.names = NULL;
  ctx->struct_types.types = NULL;
  ctx->struct_types.field_counts = NULL;
  ctx->struct_types.field_names = NULL;
  ctx->struct_types.lookups = NULL;
  ctx->struct_types.count = 0;
  ctx->struct_types.capacity = 0;
}

LLVMTypeRef codegen_type_from_string(CodeGenContext *ctx,
                                     const char *type_name) {
  if (!type_name)
    return LLVMInt64TypeInContext(ctx->llvm_ctx);
  unsigned long hash = hash_string(type_name) % type_cache_size;
  TypeCacheEntry *entry = type_cache[hash];
  while (entry) {
    if (entry->key == type_name || strcmp(entry->key, type_name) == 0)
      return entry->type;
    entry = entry->next;
  }
  LLVMTypeRef type;
  if (type_name[0] == 'v' && strcmp(type_name, "void") == 0)
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
      if (ctx->struct_types.names[i] == type_name ||
          strcmp(ctx->struct_types.names[i], type_name) == 0) {
        type = ctx->struct_types.types[i];
        entry = malloc(sizeof(TypeCacheEntry));
        entry->key = type_name;
        entry->type = type;
        entry->next = type_cache[hash];
        type_cache[hash] = entry;
        return type;
      }
    }
    type = LLVMInt64TypeInContext(ctx->llvm_ctx);
  }
  entry = malloc(sizeof(TypeCacheEntry));
  entry->key = type_name;
  entry->type = type;
  entry->next = type_cache[hash];
  type_cache[hash] = entry;
  return type;
}

static bool ensure_target_machine(CodeGenContext *ctx) {
  if (ctx->target_machine)
    return true;
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
      ctx->optimization_level, LLVMRelocDefault, LLVMCodeModelDefault);
  if (!ctx->target_machine) {
    codegen_error(ctx, "Failed to create target machine");
    return false;
  }
  return true;
}

void codegen_run_opt_passes(CodeGenContext *ctx) {
  if (ctx->build_type != BUILD_RELEASE)
    return;
  if (!ensure_target_machine(ctx))
    return;

  LLVMPassBuilderOptionsRef opts = LLVMCreatePassBuilderOptions();

  LLVMPassBuilderOptionsSetLoopVectorization(opts, 1);
  LLVMPassBuilderOptionsSetLoopUnrolling(opts, 1);

  LLVMErrorRef r =
      LLVMRunPasses(ctx->module, "default<O3>", ctx->target_machine, opts);
  if (r) {
    char *msg = LLVMGetErrorMessage(r);
    fprintf(stderr, "opt failed: %s\n", msg);
    LLVMDisposeErrorMessage(msg);
  }
  LLVMDisposePassBuilderOptions(opts);
}

LLVMTypeRef codegen_type_from_node(CodeGenContext *ctx, ASTNode *type_node) {
  if (!type_node || type_node->type != NODE_TYPE_ANNOTATION)
    return LLVMInt64TypeInContext(ctx->llvm_ctx);
  LLVMTypeRef base =
      codegen_type_from_string(ctx, type_node->type_annot.type_name);
  for (int i = 0; i < type_node->type_annot.pointer_depth; i++)
    base = LLVMPointerType(base, 0);
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
  case NODE_CALL: {
    const char *name = expr->call.name;
    if (name && strncmp(name, "string.", 7) == 0) {
      const char *n = name + 7;
      if (strcmp(n, "len") == 0 || strcmp(n, "find") == 0 ||
          strcmp(n, "byte") == 0 || strcmp(n, "to_int") == 0)
        return LLVMInt64TypeInContext(ctx->llvm_ctx);
      if (strcmp(n, "to_float") == 0)
        return LLVMDoubleTypeInContext(ctx->llvm_ctx);
      return LLVMPointerType(LLVMInt8TypeInContext(ctx->llvm_ctx), 0);
    }
    return LLVMInt64TypeInContext(ctx->llvm_ctx);
  }
  case NODE_BINARY_OP:
    if (expr->binary.op_kind == OP_RANGE)
      return LLVMPointerType(LLVMInt8TypeInContext(ctx->llvm_ctx), 0);
    return LLVMInt64TypeInContext(ctx->llvm_ctx);
  default:
    return LLVMInt64TypeInContext(ctx->llvm_ctx);
  }
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
    if (entry->key == str || strcmp(entry->key, str) == 0)
      return entry->value;
    entry = entry->next;
  }
  LLVMValueRef global = LLVMBuildGlobalStringPtr(ctx->builder, str, "str");
  entry = malloc(sizeof(StringPoolEntry));
  entry->key = str;
  entry->value = global;
  entry->next = string_pool_hash[hash];
  string_pool_hash[hash] = entry;
  return global;
}

static ScopeVar *scope_find(Scope *s, const char *name) {
  if (!s || !s->bucket_count)
    return NULL;
  unsigned long h = hash_string(name) & (s->bucket_count - 1);
  ScopeVar *v = s->buckets[h];
  while (v) {
    if (v->name == name || strcmp(v->name, name) == 0)
      return v;
    v = v->next;
  }
  return NULL;
}

static void scope_grow(Scope *s) {
  int ns = s->bucket_count ? s->bucket_count * 2 : 16;
  ScopeVar **nb = calloc(ns, sizeof(ScopeVar *));
  for (int i = 0; i < s->bucket_count; i++) {
    ScopeVar *v = s->buckets[i];
    while (v) {
      ScopeVar *nx = v->next;
      unsigned long h = v->hash & (ns - 1);
      v->next = nb[h];
      nb[h] = v;
      v = nx;
    }
  }
  free(s->buckets);
  s->buckets = nb;
  s->bucket_count = ns;
}

void codegen_scope_push(CodeGenContext *ctx) {
  Scope *s = calloc(1, sizeof(Scope));
  s->bucket_count = 16;
  s->buckets = calloc(s->bucket_count, sizeof(ScopeVar *));
  s->parent = ctx->current_scope;
  ctx->current_scope = s;
}

void codegen_scope_pop(CodeGenContext *ctx) {
  Scope *s = ctx->current_scope;
  if (!s)
    return;
  ctx->current_scope = s->parent;
  for (int i = 0; i < s->bucket_count; i++) {
    ScopeVar *v = s->buckets[i];
    while (v) {
      ScopeVar *nx = v->next;
      free(v);
      v = nx;
    }
  }
  free(s->buckets);
  free(s);
}

void codegen_scope_add(CodeGenContext *ctx, const char *name,
                       LLVMValueRef value, LLVMTypeRef type) {
  Scope *s = ctx->current_scope;
  if (!s) {
    codegen_scope_push(ctx);
    s = ctx->current_scope;
    ctx->global_scope = s;
  }
  if (s->count * 4 > s->bucket_count * 3)
    scope_grow(s);
  ScopeVar *v = malloc(sizeof(ScopeVar));
  v->name = name;
  v->value = value;
  v->type = type;
  v->hash = hash_string(name);
  unsigned long h = v->hash & (s->bucket_count - 1);
  v->next = s->buckets[h];
  s->buckets[h] = v;
  s->count++;
}

LLVMValueRef codegen_scope_get(CodeGenContext *ctx, const char *name) {
  Scope *s = ctx->current_scope;
  while (s) {
    ScopeVar *v = scope_find(s, name);
    if (v)
      return v->value;
    s = s->parent;
  }
  return NULL;
}

LLVMTypeRef codegen_scope_get_type(CodeGenContext *ctx, const char *name) {
  Scope *s = ctx->current_scope;
  while (s) {
    ScopeVar *v = scope_find(s, name);
    if (v)
      return v->type;
    s = s->parent;
  }
  return NULL;
}

bool codegen_scope_lookup(CodeGenContext *ctx, const char *name,
                          ScopeEntry *out) {
  Scope *s = ctx->current_scope;
  while (s) {
    ScopeVar *v = scope_find(s, name);
    if (v) {
      out->value = v->value;
      out->type = v->type;
      return true;
    }
    s = s->parent;
  }
  return false;
}

void codegen_loop_push(CodeGenContext *ctx, LLVMBasicBlockRef cont,
                       LLVMBasicBlockRef brk) {
  struct LoopContext *l = malloc(sizeof(struct LoopContext));
  l->continue_block = cont;
  l->break_block = brk;
  l->parent = ctx->loop_stack;
  ctx->loop_stack = l;
}

void codegen_loop_pop(CodeGenContext *ctx) {
  if (!ctx->loop_stack)
    return;
  struct LoopContext *l = ctx->loop_stack;
  ctx->loop_stack = l->parent;
  free(l);
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
                  error ? error : "unknown");
    if (error)
      LLVMDisposeMessage(error);
    return false;
  }
  return true;
}
