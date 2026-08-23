#ifndef AST_H
#define AST_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
  NODE_PROGRAM,
  NODE_BLOCK,
  NODE_FUNCTION,
  NODE_RETURN,
  NODE_LOCAL_VAR,
  NODE_IF,
  NODE_WHILE,
  NODE_REPEAT,
  NODE_FOR,
  NODE_BREAK,
  NODE_CONTINUE,
  NODE_DEFER,
  NODE_ASSIGN,
  NODE_CALL,
  NODE_INT_LITERAL,
  NODE_FLOAT_LITERAL,
  NODE_STRING_LITERAL,
  NODE_BOOL_LITERAL,
  NODE_NIL_LITERAL,
  NODE_VARIABLE,
  NODE_BINARY_OP,
  NODE_UNARY_OP,
  NODE_POINTER_DEREF,
  NODE_ADDRESS_OF,
  NODE_FIELD_ACCESS,
  NODE_KEYWORD,
  NODE_TYPE_ANNOTATION,
  NODE_TYPE_CAST,
  NODE_STRUCT,
  NODE_ENUM,
  NODE_TERNARY,
  NODE_TABLE,
  NODE_IMPORT,
  NODE_ASM_BLOCK,
  NODE_CBLOCK
} NodeType;

typedef struct ASTNode ASTNode;

struct ASTNode {
  NodeType type;
  int line;
  int column;
  bool is_module;
  char *module_name;
  union {
    struct {
      ASTNode **statements;
      int statement_count;
    } block;
    struct {
      char *name;
      ASTNode **params;
      int param_count;
      ASTNode **param_types;
      ASTNode *return_type;
      ASTNode *body;
      bool is_local;
      bool is_exported;
    } func;
    struct {
      ASTNode *expr;
    } return_stmt;
    struct {
      char *name;
      ASTNode *type;
      ASTNode *init;
    } local_var;
    struct {
      ASTNode *condition;
      ASTNode *then_branch;
      ASTNode *else_branch;
    } if_stmt;
    struct {
      ASTNode *condition;
      ASTNode *body;
    } while_stmt;
    struct {
      ASTNode *body;
      ASTNode *condition;
    } repeat_stmt;
    struct {
      char *var;
      ASTNode *start;
      ASTNode *end;
      ASTNode *step;
      ASTNode *body;
    } for_stmt;
    struct {
      ASTNode *expr;
    } defer_stmt;
    struct {
      ASTNode *target;
      ASTNode *value;
      char *op;
    } assign;
    struct {
      char *name;
      ASTNode **args;
      int arg_count;
    } call;
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
      char *name;
    } variable;
    struct {
      ASTNode *left;
      ASTNode *right;
      char *op;
    } binary;
    struct {
      ASTNode *operand;
      char *op;
    } unary;
    struct {
      ASTNode *operand;
    } pointer_deref;
    struct {
      ASTNode *operand;
    } address_of;
    struct {
      ASTNode *object;
      char *field;
    } field_access;
    struct {
      char *name;
      ASTNode **args;
      int arg_count;
    } keyword;
    struct {
      char *type_name;
      int pointer_depth;
    } type_annot;
    struct {
      char *type_name;
      ASTNode *expr;
    } cast;
    struct {
      char *name;
      ASTNode **fields;
      char **field_names;
      ASTNode **field_values;
      int field_count;
      bool has_methods;
      void *methods;
    } struct_def;
    struct {
      char *name;
      char **values;
      ASTNode **value_exprs;
      int value_count;
    } enum_def;
    struct {
      ASTNode *condition;
      ASTNode *then_expr;
      ASTNode *else_expr;
    } ternary;
    struct {
      ASTNode **fields;
      char **field_names;
      int field_count;
    } table;
    struct {
      char *module_path;
    } import;
    struct {
      char *code;
    } asm_block;
    struct {
      char *code;
    } cblock;
  };
};

ASTNode *ast_create_node(NodeType type, int line, int column);
void ast_destroy(ASTNode *n);
void ast_destroy_pools(void);

#endif
