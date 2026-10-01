#ifndef LLL_CODEGEN_H
#define LLL_CODEGEN_H

#include "ast.h"
#include "lll.h"
#include <llvm-c/Analysis.h>
#include <llvm-c/BitWriter.h>
#include <llvm-c/Core.h>
#include <llvm-c/ExecutionEngine.h>
#include <llvm-c/Target.h>
#include <llvm-c/TargetMachine.h>
#include <pthread.h>
#include <stdint.h>

typedef enum {
  BUILTIN_PRINT,
  BUILTIN_MATH,
  BUILTIN_STRING,
  BUILTIN_MEMORY,
  BUILTIN_TYPE,
  BUILTIN_SYSCALL,
  BUILTIN_CUSTOM
} BuiltinType;

typedef enum {
  SYSCALL_READ,
  SYSCALL_WRITE,
  SYSCALL_OPEN,
  SYSCALL_CLOSE,
  SYSCALL_EXIT,
  SYSCALL_MMAP,
  SYSCALL_MUNMAP,
  SYSCALL_BRK,
  SYSCALL_IOCTL,
  SYSCALL_GETPID,
  SYSCALL_SLEEP
} SyscallType;

typedef enum { BUILD_DEBUG, BUILD_RELEASE } BuildType;

typedef struct TypeCacheEntry {
  const char *key;
  LLVMTypeRef type;
  struct TypeCacheEntry *next;
} TypeCacheEntry;

typedef struct StringPoolEntry {
  const char *key;
  LLVMValueRef value;
  struct StringPoolEntry *next;
} StringPoolEntry;

typedef struct {
  LLVMValueRef value;
  LLVMTypeRef type;
} FuncEntry;

typedef struct {
  uint32_t *buckets;
  uint32_t *next;
  FuncEntry *entries;
  const char **names;
  int count, capacity;
  int bucket_count;
} FuncMap;

typedef struct {
  uint32_t *buckets;
  uint32_t *next;
  LLVMValueRef *vals;
  const char **keys;
  int count, capacity;
  int bucket_count;
} StrCache;

typedef struct {
  LLVMValueRef value;
  LLVMTypeRef type;
} ScopeEntry;

typedef struct ScopeVar {
  const char *name;
  LLVMValueRef value;
  LLVMTypeRef type;
  uint64_t hash;
  struct ScopeVar *next;
} ScopeVar;

typedef struct Scope {
  ScopeVar **buckets;
  int bucket_count;
  int count;
  struct Scope *parent;
} Scope;

typedef struct FieldLookup {
  LLVMTypeRef type;
  const char **names;
  int count;
  uint32_t *buckets;
  uint32_t *next;
  int bucket_count;
} FieldLookup;

typedef struct CodeGenContext {
  LLVMContextRef llvm_ctx;
  LLVMModuleRef module;
  LLVMBuilderRef builder;

  struct {
    LLVMValueRef function;
    LLVMBasicBlockRef entry_block;
    LLVMBasicBlockRef current_block;
    LLVMBasicBlockRef return_block;
    LLVMValueRef return_value;
    LLVMTypeRef return_type;
    bool has_return;
    Scope *scope;
  } current_func;

  struct LoopContext {
    LLVMBasicBlockRef continue_block;
    LLVMBasicBlockRef break_block;
    struct LoopContext *parent;
  } *loop_stack;

  Scope *current_scope;
  Scope *global_scope;

  FuncMap func_map;
  StrCache str_cache;

  LLVMValueRef printf_fn;
  LLVMValueRef puts_fn;
  LLVMValueRef strlen_fn;
  LLVMValueRef strcmp_fn;
  LLVMValueRef strcat_fn;
  LLVMValueRef malloc_fn;
  LLVMValueRef dlopen_fn;
  LLVMValueRef dlsym_fn;
  LLVMValueRef dlclose_fn;
  LLVMTypeRef dlopen_type;
  LLVMTypeRef dlsym_type;
  LLVMTypeRef dlclose_type;
  LLVMValueRef fmt_int;
  LLVMValueRef fmt_flt;
  LLVMTypeRef printf_type;
  LLVMTypeRef puts_type;

  struct {
    const char **names;
    LLVMTypeRef *types;
    int *field_counts;
    const char ***field_names;
    FieldLookup **lookups;
    int count;
    int capacity;
  } struct_types;

  struct {
    const char **names;
    int *types;
    int count, capacity;
  } builtins;

  LLVMTargetMachineRef target_machine;
  LLVMTargetDataRef target_data;
  char *target_triple;
  char *cpu_name;
  char *cpu_features;

  LLVMPassManagerRef pass_manager;
  int opt_level;
  BuildType build_type;
  LLVMCodeGenOptLevel optimization_level;
  bool module_verified;

  const char *module_name;
  bool is_module;
  bool verbose;

  int temp_counter;
  int block_counter;
  int string_counter;

  char error_msg[1024];
  bool has_error;

  int functions_generated;
  int instructions_generated;
  LLVMValueRef int_format;
  LLVMValueRef str_format;

  Platform target_platform;
  char *target_triple_override;
} CodeGenContext;

void codegen_init(CodeGenContext *ctx, const char *module_name,
                  BuildType build_type, const char *target_triple_override);
void codegen_destroy(CodeGenContext *ctx);
void codegen_set_target(CodeGenContext *ctx, const char *triple);
bool codegen_generate(CodeGenContext *ctx, ASTNode *program);
bool codegen_compile_to_file(CodeGenContext *ctx, const char *output_file);
bool codegen_compile_to_object(CodeGenContext *ctx, const char *output_file);
bool codegen_jit_execute(CodeGenContext *ctx, ASTNode *program);

LLVMTypeRef codegen_type_from_string(CodeGenContext *ctx,
                                     const char *type_name);
LLVMTypeRef codegen_type_from_node(CodeGenContext *ctx, ASTNode *type_node);
LLVMTypeRef codegen_infer_type(CodeGenContext *ctx, ASTNode *expr);

LLVMValueRef codegen_expr(CodeGenContext *ctx, ASTNode *expr);
LLVMValueRef codegen_int_literal(CodeGenContext *ctx, ASTNode *expr);
LLVMValueRef codegen_float_literal(CodeGenContext *ctx, ASTNode *expr);
LLVMValueRef codegen_string_literal(CodeGenContext *ctx, ASTNode *expr);
LLVMValueRef codegen_bool_literal(CodeGenContext *ctx, ASTNode *expr);
LLVMValueRef codegen_variable(CodeGenContext *ctx, ASTNode *expr);
LLVMValueRef codegen_binary_op(CodeGenContext *ctx, ASTNode *expr);
LLVMValueRef codegen_unary_op(CodeGenContext *ctx, ASTNode *expr);
LLVMValueRef codegen_call(CodeGenContext *ctx, ASTNode *expr);
LLVMValueRef codegen_field_access(CodeGenContext *ctx, ASTNode *expr);
LLVMValueRef codegen_ternary(CodeGenContext *ctx, ASTNode *expr);
LLVMValueRef codegen_cast(CodeGenContext *ctx, ASTNode *expr);
LLVMValueRef codegen_table(CodeGenContext *ctx, ASTNode *expr);
LLVMValueRef codegen_asm(CodeGenContext *ctx, ASTNode *expr);

void codegen_stmt(CodeGenContext *ctx, ASTNode *stmt);
void codegen_block(CodeGenContext *ctx, ASTNode *block);
void codegen_return(CodeGenContext *ctx, ASTNode *stmt);
void codegen_if(CodeGenContext *ctx, ASTNode *stmt);
void codegen_while(CodeGenContext *ctx, ASTNode *stmt);
void codegen_repeat(CodeGenContext *ctx, ASTNode *stmt);
void codegen_for(CodeGenContext *ctx, ASTNode *stmt);
void codegen_local_var(CodeGenContext *ctx, ASTNode *stmt);
void codegen_assign(CodeGenContext *ctx, ASTNode *stmt);
void codegen_function(CodeGenContext *ctx, ASTNode *func);
void codegen_struct(CodeGenContext *ctx, ASTNode *struct_def);
void codegen_enum(CodeGenContext *ctx, ASTNode *enum_def);
void codegen_defer(CodeGenContext *ctx, ASTNode *stmt);

void codegen_scope_push(CodeGenContext *ctx);
void codegen_scope_pop(CodeGenContext *ctx);
void codegen_scope_add(CodeGenContext *ctx, const char *name,
                       LLVMValueRef value, LLVMTypeRef type);
LLVMValueRef codegen_scope_get(CodeGenContext *ctx, const char *name);
LLVMTypeRef codegen_scope_get_type(CodeGenContext *ctx, const char *name);
bool codegen_scope_lookup(CodeGenContext *ctx, const char *name,
                          ScopeEntry *out);

void codegen_loop_push(CodeGenContext *ctx, LLVMBasicBlockRef cont,
                       LLVMBasicBlockRef brk);
void codegen_loop_pop(CodeGenContext *ctx);
LLVMBasicBlockRef codegen_get_break_block(CodeGenContext *ctx);
LLVMBasicBlockRef codegen_get_continue_block(CodeGenContext *ctx);

char *codegen_temp_name(CodeGenContext *ctx, const char *prefix);
void codegen_error(CodeGenContext *ctx, const char *fmt, ...);
LLVMValueRef codegen_string_create(CodeGenContext *ctx, const char *str);
int llvm_get_builtin_type(CodeGenContext *ctx, const char *name);

void llvm_register_builtins(CodeGenContext *ctx);
LLVMValueRef codegen_syscall(CodeGenContext *ctx, int syscall_num,
                             LLVMValueRef *args, int arg_count);

int codegen_struct_field_index(CodeGenContext *ctx, LLVMTypeRef struct_type,
                               const char *field_name);
void codegen_func_map_add(CodeGenContext *ctx, const char *name,
                          LLVMValueRef fn, LLVMTypeRef ft);
FuncEntry *codegen_func_map_find(CodeGenContext *ctx, const char *name);

int codegen_op_from_string(const char *s);
int codegen_is_float_ty(LLVMTypeRef t);
int codegen_is_str_ty(LLVMTypeRef t);
LLVMValueRef codegen_arith_binop(CodeGenContext *ctx, OpKind op, LLVMValueRef l,
                                 LLVMValueRef r);
LLVMValueRef codegen_arith_strconcat(CodeGenContext *ctx, LLVMValueRef l,
                                     LLVMValueRef r);
LLVMValueRef codegen_arith_strcmp(CodeGenContext *ctx, OpKind op,
                                  LLVMValueRef l, LLVMValueRef r);
LLVMValueRef codegen_arith_unary(CodeGenContext *ctx, const char *op,
                                 LLVMValueRef v);

void codegen_run_opt_passes(CodeGenContext *ctx);
LLVMValueRef codegen_ffi_open(CodeGenContext *ctx, ASTNode *expr);
LLVMValueRef codegen_ffi_sym(CodeGenContext *ctx, ASTNode *expr);
LLVMValueRef codegen_ffi_call(CodeGenContext *ctx, ASTNode *expr);
LLVMValueRef codegen_indirect_call(CodeGenContext *ctx, ASTNode *expr);
#endif
