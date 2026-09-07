#ifndef LLL_AST_H
#define LLL_AST_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
  NODE_PROGRAM,
  NODE_BLOCK,
  NODE_FUNCTION,
  NODE_CALL,
  NODE_VARIABLE,
  NODE_INT_LITERAL,
  NODE_FLOAT_LITERAL,
  NODE_STRING_LITERAL,
  NODE_BOOL_LITERAL,
  NODE_NIL_LITERAL,
  NODE_BINARY_OP,
  NODE_UNARY_OP,
  NODE_ASSIGN,
  NODE_RETURN,
  NODE_IF,
  NODE_WHILE,
  NODE_REPEAT,
  NODE_FOR,
  NODE_BREAK,
  NODE_CONTINUE,
  NODE_LOCAL_VAR,
  NODE_STRUCT,
  NODE_ENUM,
  NODE_FIELD_ACCESS,
  NODE_TERNARY,
  NODE_TYPE_CAST,
  NODE_TABLE,
  NODE_ASM_BLOCK,
  NODE_CBLOCK,
  NODE_DEFER,
  NODE_IMPORT,
  NODE_MODULE,
  NODE_POINTER_DEREF,
  NODE_ADDRESS_OF,
  NODE_TYPE_ANNOTATION,
  NODE_KEYWORD,
  NODE_EXPORT
} NodeType;

typedef struct ASTNode ASTNode;

typedef struct {
  ASTNode **statements;
  int statement_count;
} BlockNode;

typedef struct {
  char *name;
  ASTNode **params;
  ASTNode **param_types;
  int param_count;
  ASTNode *body;
  ASTNode *return_type;
  bool is_local;
  bool is_exported;
} FunctionNode;

typedef struct {
  char *name;
  ASTNode **args;
  int arg_count;
} CallNode;

typedef struct {
  char *name;
} VariableNode;

typedef struct {
  int64_t value;
} IntLiteralNode;

typedef struct {
  double value;
} FloatLiteralNode;

typedef struct {
  char *value;
} StringLiteralNode;

typedef struct {
  bool value;
} BoolLiteralNode;

typedef struct {
  char *op;
  ASTNode *left;
  ASTNode *right;
} BinaryOpNode;

typedef struct {
  char *op;
  ASTNode *operand;
} UnaryOpNode;

typedef struct {
  char *op;
  ASTNode *target;
  ASTNode *value;
} AssignNode;

typedef struct {
  ASTNode *expr;
} ReturnNode;

typedef struct {
  ASTNode *condition;
  ASTNode *then_branch;
  ASTNode *else_branch;
} IfNode;

typedef struct {
  ASTNode *condition;
  ASTNode *body;
} WhileNode;

typedef struct {
  ASTNode *body;
  ASTNode *condition;
} RepeatNode;

typedef struct {
  char *var;
  ASTNode *start;
  ASTNode *end;
  ASTNode *step;
  ASTNode *body;
} ForNode;

typedef struct {
  char *name;
  ASTNode *type;
  ASTNode *init;
  bool is_exported;
} LocalVarNode;

typedef struct {
  char *name;
  ASTNode **fields;
  char **field_names;
  ASTNode **field_values;
  int field_count;
  bool has_methods;
  ASTNode **methods;
} StructNode;

typedef struct {
  char *name;
  char **values;
  ASTNode **value_exprs;
  int value_count;
} EnumNode;

typedef struct {
  ASTNode *object;
  char *field;
} FieldAccessNode;

typedef struct {
  ASTNode *condition;
  ASTNode *then_expr;
  ASTNode *else_expr;
} TernaryNode;

typedef struct {
  char *type_name;
  ASTNode *expr;
} TypeCastNode;

typedef struct {
  ASTNode **fields;
  char **field_names;
  int field_count;
} TableNode;

typedef struct {
  char *code;
} AsmBlockNode;

typedef struct {
  char *code;
} CBlockNode;

typedef struct {
  ASTNode *expr;
} DeferNode;

typedef struct {
  char *module_path;
} ImportNode;

typedef struct {
  char *name;
  ASTNode *body;
} ModuleNode;

typedef struct {
  ASTNode *operand;
} PointerDerefNode;

typedef struct {
  ASTNode *operand;
} AddressOfNode;

typedef struct {
  char *type_name;
  int pointer_depth;
} TypeAnnotationNode;

typedef struct {
  char *name;
  ASTNode **args;
  int arg_count;
} KeywordNode;

struct ASTNode {
  NodeType type;
  int line;
  int column;
  bool is_module;
  char *module_name;

  union {
    BlockNode block;
    FunctionNode func;
    CallNode call;
    VariableNode variable;
    IntLiteralNode int_lit;
    FloatLiteralNode float_lit;
    StringLiteralNode string_lit;
    BoolLiteralNode bool_lit;
    BinaryOpNode binary;
    UnaryOpNode unary;
    AssignNode assign;
    ReturnNode return_stmt;
    IfNode if_stmt;
    WhileNode while_stmt;
    RepeatNode repeat_stmt;
    ForNode for_stmt;
    LocalVarNode local_var;
    StructNode struct_def;
    EnumNode enum_def;
    FieldAccessNode field_access;
    TernaryNode ternary;
    TypeCastNode cast;
    TableNode table;
    AsmBlockNode asm_block;
    CBlockNode cblock;
    DeferNode defer_stmt;
    ImportNode import;
    ModuleNode module;
    PointerDerefNode pointer_deref;
    AddressOfNode address_of;
    TypeAnnotationNode type_annot;
    KeywordNode keyword;
  };
};
void ast_destroy_pools(void);
ASTNode *ast_create_node(NodeType type, int line, int column);
#endif
