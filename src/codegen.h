#ifndef LLL_CODEGEN_H
#define LLL_CODEGEN_H

#include "ast.h"
#include <llvm-c/Analysis.h>
#include <llvm-c/BitWriter.h>
#include <llvm-c/Core.h>
#include <llvm-c/ExecutionEngine.h>
#include <llvm-c/Target.h>
#include <llvm-c/TargetMachine.h>

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
  } current_func;

  struct LoopContext {
    LLVMBasicBlockRef continue_block;
    LLVMBasicBlockRef break_block;
    struct LoopContext *parent;
  } *loop_stack;

  struct Scope {
    char **names;
    LLVMValueRef *values;
    LLVMTypeRef *types;
    int count;
    int capacity;
    struct Scope *parent;
  } *current_scope;

  struct Scope *global_scope;

  struct {
    char **names;
    LLVMValueRef *functions;
    LLVMTypeRef *types;
    int *builtin_types;
    int *arg_counts;
    int count;
    int capacity;
  } functions;

  struct {
    char **names;
    LLVMTypeRef *types;
    int *field_counts;
    int count;
    int capacity;
  } struct_types;

  LLVMTargetMachineRef target_machine;
  LLVMTargetDataRef target_data;
  char *target_triple;
  char *cpu_name;
  char *cpu_features;

  LLVMPassManagerRef pass_manager;
  int opt_level;

  const char *module_name;
  bool is_module;
  bool verbose;

  int temp_counter;
  int block_counter;
  int string_counter;

  struct {
    char **strings;
    LLVMValueRef *values;
    int count;
  } string_pool;

  char error_msg[1024];
  bool has_error;

  int functions_generated;
  int instructions_generated;
  LLVMValueRef int_format;
  LLVMValueRef str_format;
} CodeGenContext;

void codegen_init(CodeGenContext *ctx, const char *module_name);
void codegen_destroy(CodeGenContext *ctx);
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
LLVMTypeRef codegen_scope_get_type(CodeGenContext *ctx, const char *name);
#endif
