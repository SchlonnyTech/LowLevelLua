#ifndef AST_H
#define AST_H
#include <stdbool.h>
#include <stdint.h>

typedef enum {
  NODE_PROGRAM,
  NODE_FUNCTION,
  NODE_BINARY_OP,
  NODE_UNARY_OP,
  NODE_CALL,
  NODE_VARIABLE,
  NODE_INT_LITERAL,
  NODE_FLOAT_LITERAL,
  NODE_STRING_LITERAL,
  NODE_BOOL_LITERAL,
  NODE_NIL_LITERAL,
  NODE_RETURN,
  NODE_IF,
  NODE_WHILE,
  NODE_FOR,
  NODE_FOR_IN,
  NODE_GENERIC_FOR,
  NODE_REPEAT,
  NODE_ASM_BLOCK,
  NODE_LOCAL_VAR,
  NODE_STRUCT,
  NODE_ENUM,
  NODE_FIELD_ACCESS,
  NODE_POINTER_DEREF,
  NODE_ADDRESS_OF,
  NODE_BLOCK,
  NODE_DEFER,
  NODE_TYPE_ANNOTATION,
  NODE_ASSIGN,
  NODE_IMPORT,
  NODE_KEYWORD,
  NODE_TABLE,
  NODE_TYPE_CAST,
  NODE_TERNARY,
  NODE_BREAK,
  NODE_CONTINUE
} NodeType;

typedef struct ASTNode {
  NodeType type;
  int line;
  int column;
  struct ASTNode *next;
  bool is_module;
  char *module_name;
  union {
    struct {
      char *name;
      struct ASTNode **params;
      int param_count;
      struct ASTNode **param_types;
      struct ASTNode *return_type;
      struct ASTNode *body;
      bool is_exported;
      bool is_variadic;
      bool is_local;
    } func;
    struct {
      struct ASTNode *left;
      struct ASTNode *right;
      char *op;
    } binary;
    struct {
      struct ASTNode *operand;
      char *op;
    } unary;
    struct {
      char *name;
      struct ASTNode **args;
      int arg_count;
    } call;
    struct {
      char *name;
    } variable;
    struct {
      int64_t value;
    } int_lit;
    struct {
      double value;
    } float_lit;
    struct {
      char *value;
    } string_lit;
    struct {
      bool value;
    } bool_lit;
    struct {
      struct ASTNode *expr;
    } return_stmt;
    struct {
      struct ASTNode *condition;
      struct ASTNode *then_branch;
      struct ASTNode *else_branch;
    } if_stmt;
    struct {
      struct ASTNode *condition;
      struct ASTNode *body;
    } while_stmt;
    struct {
      struct ASTNode *condition;
      struct ASTNode *body;
    } repeat_stmt;
    struct {
      char *var;
      struct ASTNode *var_type;
      struct ASTNode *start;
      struct ASTNode *end;
      struct ASTNode *step;
      struct ASTNode *body;
    } for_stmt;
    struct {
      char *code;
    } asm_block;
    struct {
      char *name;
      struct ASTNode *type;
      struct ASTNode *init;
    } local_var;
    struct {
      char *name;
      struct ASTNode **fields;
      char **field_names;
      struct ASTNode **field_values;
      int field_count;
      bool is_union;
      bool has_methods;
      struct ASTNode **methods;
      int method_count;
    } struct_def;
    struct {
      char *name;
      char **values;
      struct ASTNode **value_exprs;
      int value_count;
    } enum_def;
    struct {
      struct ASTNode *object;
      char *field;
    } field_access;
    struct {
      struct ASTNode *operand;
    } pointer_deref;
    struct {
      struct ASTNode *operand;
    } address_of;
    struct {
      struct ASTNode **statements;
      int statement_count;
    } block;
    struct {
      struct ASTNode *expr;
    } defer_stmt;
    struct {
      char *type_name;
      int pointer_depth;
    } type_annot;
    struct {
      struct ASTNode *target;
      struct ASTNode *value;
      char *op;
    } assign;
    struct {
      char *module_path;
    } import;
    struct {
      char *name;
      struct ASTNode **args;
      int arg_count;
    } keyword;
    struct {
      struct ASTNode **fields;
      char **field_names;
      int field_count;
    } table;
    struct {
      char *type_name;
      struct ASTNode *expr;
    } cast;
    struct {
      struct ASTNode *condition;
      struct ASTNode *then_expr;
      struct ASTNode *else_expr;
    } ternary;
  };
} ASTNode;

ASTNode *ast_create_node(NodeType type, int line, int column);
void ast_destroy(ASTNode *node);
void ast_destroy_pools(void);
#endif
