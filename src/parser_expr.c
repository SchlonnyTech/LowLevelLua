#include "keywords/keywords.h"
#include "parser.h"
#include "utils.h"
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEBUG_EXPR 0
#define DPRINTF_EXPR(fmt, ...)                                                 \
  if (DEBUG_EXPR)                                                              \
  fprintf(stderr, "[EXPR] " fmt, ##__VA_ARGS__)

extern jmp_buf error_jmp;
extern bool error_jmp_set;
extern void parser_error(int line, int column, const char *fmt, ...);

#define MAX_RECURSION_DEPTH 256
static int recursion_depth = 0;

static inline bool is_unary_op(TokenType t) {
  return t == TOKEN_MINUS || t == TOKEN_BANG || t == TOKEN_STAR ||
         t == TOKEN_BITAND || t == TOKEN_SHARP || t == TOKEN_NOT;
}

static inline bool is_mul_op(TokenType t) {
  return t == TOKEN_STAR || t == TOKEN_SLASH || t == TOKEN_PERCENT ||
         t == TOKEN_FLOOR_DIV;
}

static inline bool is_add_op(TokenType t) {
  return t == TOKEN_PLUS || t == TOKEN_MINUS;
}

static inline bool is_compare_op(TokenType t) {
  return t == TOKEN_LT || t == TOKEN_GT || t == TOKEN_LTE || t == TOKEN_GTE;
}

static inline bool is_eq_op(TokenType t) {
  return t == TOKEN_EQ || t == TOKEN_NEQ;
}

static inline bool is_assign_op(Parser *p) {
  if (parser_check(p, TOKEN_EQUALS) || parser_check(p, TOKEN_WALRUS))
    return true;
  if ((parser_check(p, TOKEN_PLUS) || parser_check(p, TOKEN_MINUS) ||
       parser_check(p, TOKEN_STAR) || parser_check(p, TOKEN_SLASH) ||
       parser_check(p, TOKEN_PERCENT)) &&
      peek(p) && peek(p)->type == TOKEN_EQUALS)
    return true;
  return false;
}

static ASTNode *parse_call_args(Parser *p, const char *name, int line,
                                int col) {
  DPRINTF_EXPR("parse_call_args: %s\n", name);
  parser_advance(p);
  ASTNode *n = ast_create_node(NODE_CALL, line, col);
  n->call.name = string_copy(name);
  int cap = 8, cnt = 0;
  ASTNode **args = malloc(sizeof(ASTNode *) * cap);

  if (!parser_check(p, TOKEN_RPAREN)) {
    do {
      if (cnt >= cap) {
        cap *= 2;
        args = realloc(args, sizeof(ASTNode *) * cap);
      }

      int arg_line = p->current.line;
      int arg_col = p->current.column;
      args[cnt++] = parse_expression(p);

      if (p->current.line > arg_line && !parser_check(p, TOKEN_COMMA) &&
          !parser_check(p, TOKEN_RPAREN)) {
        int last_col = arg_col;
        Token *prev_tok = &p->tokens[p->pos - 1];
        if (prev_tok && prev_tok->line == arg_line) {
          last_col =
              prev_tok->column + (prev_tok->text ? strlen(prev_tok->text) : 0);
        }
        parser_error(arg_line, last_col,
                     "Missing ')' after argument in function call '%s'", name);
        if (error_jmp_set)
          longjmp(error_jmp, 1);
        exit(1);
      }
    } while (parser_match(p, TOKEN_COMMA));
  }

  if (!parser_check(p, TOKEN_RPAREN)) {
    Token *last_tok = p->pos > 0 ? &p->tokens[p->pos - 1] : NULL;
    int err_line = last_tok ? last_tok->line : line;
    int err_col =
        last_tok
            ? (last_tok->column + (last_tok->text ? strlen(last_tok->text) : 0))
            : col;
    parser_error(err_line, err_col,
                 "Missing closing ')' for function call '%s'", name);
    if (error_jmp_set)
      longjmp(error_jmp, 1);
    exit(1);
  }

  parser_advance(p);
  n->call.arg_count = cnt;
  n->call.args = args;
  return n;
}

static ASTNode *parse_postfix(Parser *p, ASTNode *base) {
  ASTNode *result = base;

  while (parser_check(p, TOKEN_DOT) || parser_check(p, TOKEN_LBRACK)) {
    if (parser_check(p, TOKEN_DOT)) {
      parser_advance(p);
      char *field = toktext(p);
      parser_expect(p, TOKEN_IDENT, "field");

      if (parser_check(p, TOKEN_LPAREN)) {
        char *method_name = string_format(
            "%s.%s",
            result->type == NODE_VARIABLE       ? result->variable.name
            : result->type == NODE_FIELD_ACCESS ? result->field_access.field
                                                : "obj",
            field);
        result = parse_call_args(p, method_name, base->line, base->column);
        free(method_name);
        free(field);
      } else {
        ASTNode *a =
            ast_create_node(NODE_FIELD_ACCESS, base->line, base->column);
        a->field_access.object = result;
        a->field_access.field = field;
        result = a;
      }
    } else if (parser_check(p, TOKEN_LBRACK)) {
      parser_advance(p);
      ASTNode *index = parse_expression(p);
      parser_expect(p, TOKEN_RBRACK, "]");
      ASTNode *a = ast_create_node(NODE_BINARY_OP, base->line, base->column);
      a->binary.op = string_copy("[]");
      a->binary.left = result;
      a->binary.right = index;
      result = a;
    }
  }

  return result;
}

static ASTNode *parse_prefix(Parser *p) {
  int line = p->current.line, col = p->current.column;
  DPRINTF_EXPR("parse_prefix: type=%d text='%s'\n", p->current.type,
               p->current.text ? p->current.text : "(null)");

  switch (p->current.type) {
  case TOKEN_STRING: {
    ASTNode *n = ast_create_node(NODE_STRING_LITERAL, line, col);
    n->string_lit.value = toktext(p);
    parser_advance(p);
    return n;
  }
  case TOKEN_NUMBER_INT: {
    ASTNode *n = ast_create_node(NODE_INT_LITERAL, line, col);
    n->int_lit.value = p->current.int_value;
    parser_advance(p);
    return n;
  }
  case TOKEN_NUMBER_FLOAT: {
    ASTNode *n = ast_create_node(NODE_FLOAT_LITERAL, line, col);
    n->float_lit.value = p->current.float_value;
    parser_advance(p);
    return n;
  }
  case TOKEN_TRUE:
    parser_advance(p);
    {
      ASTNode *t = ast_create_node(NODE_BOOL_LITERAL, line, col);
      t->bool_lit.value = true;
      return t;
    }
  case TOKEN_FALSE:
    parser_advance(p);
    {
      ASTNode *f = ast_create_node(NODE_BOOL_LITERAL, line, col);
      f->bool_lit.value = false;
      return f;
    }
  case TOKEN_NIL:
    parser_advance(p);
    return ast_create_node(NODE_NIL_LITERAL, line, col);
  default:
    break;
  }

  if (is_import_call(p)) {
    parser_advance(p);
    parser_advance(p);
    ASTNode *n = ast_create_node(NODE_IMPORT, line, col);
    if (parser_check(p, TOKEN_STRING)) {
      n->import.module_path = toktext(p);
      parser_advance(p);
    } else {
      n->import.module_path = toktext(p);
      parser_expect(p, TOKEN_IDENT, "module name");
    }
    parser_expect(p, TOKEN_RPAREN, ")");
    mark_import(n->import.module_path);
    return n;
  }

  if (parser_check(p, TOKEN_IDENT)) {
    char *name = toktext(p);
    parser_advance(p);

    if (parser_check(p, TOKEN_LPAREN)) {
      ASTNode *n = parse_call_args(p, name, line, col);
      free(name);
      return n;
    }

    ASTNode *n = ast_create_node(NODE_VARIABLE, line, col);
    n->variable.name = name;
    return parse_postfix(p, n);
  }

  if (parser_match(p, TOKEN_LPAREN)) {
    ASTNode *n = parse_expression(p);
    parser_expect(p, TOKEN_RPAREN, ")");
    return n;
  }

  if (parser_match(p, TOKEN_LBRACE)) {
    ASTNode *n = ast_create_node(NODE_TABLE, line, col);
    int cap = 8, cnt = 0;
    ASTNode **fields = malloc(sizeof(ASTNode *) * cap);
    char **names = malloc(sizeof(char *) * cap);

    if (!parser_check(p, TOKEN_RBRACE)) {
      do {
        if (cnt >= cap) {
          cap *= 2;
          fields = realloc(fields, sizeof(ASTNode *) * cap);
          names = realloc(names, sizeof(char *) * cap);
        }

        if (parser_check(p, TOKEN_LBRACK)) {
          parser_advance(p);
          ASTNode *key_expr = parse_expression(p);
          parser_expect(p, TOKEN_RBRACK, "]");
          parser_expect(p, TOKEN_EQUALS, "=");

          if (key_expr->type == NODE_INT_LITERAL)
            names[cnt] =
                string_format("[%lld]", (long long)key_expr->int_lit.value);
          else if (key_expr->type == NODE_STRING_LITERAL)
            names[cnt] = string_format("[\"%s\"]", key_expr->string_lit.value);
          else
            names[cnt] = string_copy("[expr]");

          fields[cnt] = parse_expression(p);
          cnt++;
        } else if (parser_check(p, TOKEN_IDENT) && peek(p) &&
                   peek(p)->type == TOKEN_EQUALS) {
          names[cnt] = toktext(p);
          parser_advance(p);
          parser_advance(p);
          fields[cnt] = parse_expression(p);
          cnt++;
        } else {
          char key[16];
          snprintf(key, 16, "%d", cnt);
          names[cnt] = string_copy(key);
          fields[cnt] = parse_expression(p);
          cnt++;
        }
      } while (parser_match(p, TOKEN_COMMA) ||
               parser_match(p, TOKEN_SEMICOLON));
    }

    parser_expect(p, TOKEN_RBRACE, "}");
    n->table.field_count = cnt;
    n->table.fields = fields;
    n->table.field_names = names;
    return n;
  }

  if (parser_match(p, TOKEN_IF)) {
    ASTNode *n = ast_create_node(NODE_TERNARY, line, col);
    n->ternary.condition = parse_expression(p);
    parser_expect(p, TOKEN_THEN, "then");
    n->ternary.then_expr = parse_expression(p);
    parser_expect(p, TOKEN_ELSE, "else");
    n->ternary.else_expr = parse_expression(p);
    return n;
  }

  if (parser_match(p, TOKEN_FUNCTION)) {
    ASTNode *n = ast_create_node(NODE_FUNCTION, line, col);
    n->func.name = string_copy("");
    parser_expect(p, TOKEN_LPAREN, "(");

    int cap = 4, cnt = 0;
    char **params = malloc(sizeof(char *) * cap);
    ASTNode **types = malloc(sizeof(ASTNode *) * cap);

    if (!parser_check(p, TOKEN_RPAREN)) {
      do {
        if (cnt >= cap) {
          cap *= 2;
          params = realloc(params, sizeof(char *) * cap);
          types = realloc(types, sizeof(ASTNode *) * cap);
        }
        params[cnt] = toktext(p);
        parser_expect(p, TOKEN_IDENT, "param");
        if (parser_match(p, TOKEN_COLON))
          types[cnt] = parse_type(p);
        else
          types[cnt] = ast_create_node(NODE_TYPE_ANNOTATION, line, col);
        cnt++;
      } while (parser_match(p, TOKEN_COMMA));
    }

    parser_expect(p, TOKEN_RPAREN, ")");
    if (parser_match(p, TOKEN_COLON))
      n->func.return_type = parse_type(p);
    else {
      n->func.return_type = ast_create_node(NODE_TYPE_ANNOTATION, line, col);
      n->func.return_type->type_annot.type_name = "int32";
    }

    n->func.body = parse_block(p);
    parser_expect(p, TOKEN_END, "end");
    n->func.param_count = cnt;
    n->func.params = malloc(sizeof(ASTNode *) * cnt);
    n->func.param_types = malloc(sizeof(ASTNode *) * cnt);

    for (int i = 0; i < cnt; i++) {
      ASTNode *pn = ast_create_node(NODE_VARIABLE, line, col);
      pn->variable.name = params[i];
      n->func.params[i] = pn;
      n->func.param_types[i] = types[i];
    }

    n->func.is_local = true;
    n->func.is_exported = false;
    free(params);
    free(types);
    return n;
  }

  parser_error(line, col, "Unexpected token '%s'", p->current.text);
  if (error_jmp_set)
    longjmp(error_jmp, 1);
  exit(1);
}

static ASTNode *parse_unary(Parser *p) {
  int line = p->current.line, col = p->current.column;

  if (is_unary_op(p->current.type)) {
    char *op_str_val;
    NodeType node_type = NODE_UNARY_OP;

    if (parser_match(p, TOKEN_NOT))
      op_str_val = string_copy("!");
    else if (parser_match(p, TOKEN_MINUS))
      op_str_val = string_copy("-");
    else if (parser_match(p, TOKEN_BANG))
      op_str_val = string_copy("!");
    else if (parser_match(p, TOKEN_STAR)) {
      node_type = NODE_POINTER_DEREF;
      op_str_val = string_copy("*");
    } else if (parser_match(p, TOKEN_BITAND)) {
      node_type = NODE_ADDRESS_OF;
      op_str_val = string_copy("&");
    } else if (parser_match(p, TOKEN_SHARP))
      op_str_val = string_copy("#");
    else
      return parse_prefix(p);

    ASTNode *n = ast_create_node(node_type, line, col);
    if (node_type == NODE_UNARY_OP) {
      n->unary.op = op_str_val;
      n->unary.operand = parse_unary(p);
    } else if (node_type == NODE_POINTER_DEREF)
      n->pointer_deref.operand = parse_unary(p);
    else if (node_type == NODE_ADDRESS_OF)
      n->address_of.operand = parse_unary(p);
    return n;
  }

  return parse_prefix(p);
}

static ASTNode *parse_factor(Parser *p) {
  ASTNode *l = parse_unary(p);

  while (is_mul_op(p->current.type)) {
    char *op_str = toktext(p);
    parser_advance(p);
    ASTNode *r = parse_unary(p);
    ASTNode *n = ast_create_node(NODE_BINARY_OP, l->line, l->column);
    n->binary.op = op_str;
    n->binary.left = l;
    n->binary.right = r;
    l = n;
  }

  return l;
}

static ASTNode *parse_term(Parser *p) {
  ASTNode *l = parse_factor(p);

  while (is_add_op(p->current.type)) {
    char *op_str = toktext(p);
    parser_advance(p);
    ASTNode *r = parse_factor(p);
    ASTNode *n = ast_create_node(NODE_BINARY_OP, l->line, l->column);
    n->binary.op = op_str;
    n->binary.left = l;
    n->binary.right = r;
    l = n;
  }

  return l;
}

static ASTNode *parse_concat(Parser *p) {
  ASTNode *l = parse_term(p);

  while (parser_match(p, TOKEN_CONCAT)) {
    char *op_str = string_copy("..");
    ASTNode *r = parse_term(p);
    ASTNode *n = ast_create_node(NODE_BINARY_OP, l->line, l->column);
    n->binary.op = op_str;
    n->binary.left = l;
    n->binary.right = r;
    l = n;
  }

  return l;
}

static ASTNode *parse_compare(Parser *p) {
  ASTNode *l = parse_concat(p);

  while (is_compare_op(p->current.type)) {
    char *op_str = toktext(p);
    parser_advance(p);
    ASTNode *r = parse_concat(p);
    ASTNode *n = ast_create_node(NODE_BINARY_OP, l->line, l->column);
    n->binary.op = op_str;
    n->binary.left = l;
    n->binary.right = r;
    l = n;
  }

  return l;
}

static ASTNode *parse_eq(Parser *p) {
  ASTNode *l = parse_compare(p);

  while (is_eq_op(p->current.type)) {
    char *op_str = toktext(p);
    parser_advance(p);
    ASTNode *r = parse_compare(p);
    ASTNode *n = ast_create_node(NODE_BINARY_OP, l->line, l->column);
    n->binary.op = op_str;
    n->binary.left = l;
    n->binary.right = r;
    l = n;
  }

  return l;
}

static ASTNode *parse_and(Parser *p) {
  ASTNode *l = parse_eq(p);

  while (parser_check(p, TOKEN_AND) || parser_check(p, TOKEN_BITAND)) {
    parser_advance(p);
    ASTNode *r = parse_eq(p);
    ASTNode *n = ast_create_node(NODE_BINARY_OP, l->line, l->column);
    n->binary.op = string_copy("&&");
    n->binary.left = l;
    n->binary.right = r;
    l = n;
  }

  return l;
}

static ASTNode *parse_or(Parser *p) {
  ASTNode *l = parse_and(p);

  while (parser_check(p, TOKEN_OR) || parser_check(p, TOKEN_BITOR)) {
    parser_advance(p);
    ASTNode *r = parse_and(p);
    ASTNode *n = ast_create_node(NODE_BINARY_OP, l->line, l->column);
    n->binary.op = string_copy("||");
    n->binary.left = l;
    n->binary.right = r;
    l = n;
  }

  return l;
}

ASTNode *parse_expression(Parser *p) {
  if (++recursion_depth > MAX_RECURSION_DEPTH) {
    parser_error(p->current.line, p->current.column,
                 "Maximum recursion depth exceeded");
    if (error_jmp_set)
      longjmp(error_jmp, 1);
    exit(1);
  }

  DPRINTF_EXPR("parse_expression: type=%d text='%s'\n", p->current.type,
               p->current.text ? p->current.text : "(null)");

  ASTNode *l = parse_or(p);

  if (is_assign_op(p)) {
    char *op;

    if (parser_match(p, TOKEN_WALRUS))
      op = string_copy(":=");
    else if (parser_match(p, TOKEN_EQUALS))
      op = string_copy("=");
    else if (parser_match(p, TOKEN_PLUS) && parser_match(p, TOKEN_EQUALS))
      op = string_copy("+=");
    else if (parser_match(p, TOKEN_MINUS) && parser_match(p, TOKEN_EQUALS))
      op = string_copy("-=");
    else if (parser_match(p, TOKEN_STAR) && parser_match(p, TOKEN_EQUALS))
      op = string_copy("*=");
    else if (parser_match(p, TOKEN_SLASH) && parser_match(p, TOKEN_EQUALS))
      op = string_copy("/=");
    else if (parser_match(p, TOKEN_PERCENT) && parser_match(p, TOKEN_EQUALS))
      op = string_copy("%=");
    else
      op = string_copy("=");

    ASTNode *n = ast_create_node(NODE_ASSIGN, l->line, l->column);
    n->assign.op = op;
    n->assign.target = l;
    n->assign.value = parse_expression(p);
    recursion_depth--;
    return n;
  }

  recursion_depth--;
  return l;
}

ASTNode *parse_type(Parser *p) {
  int ptr = 0;
  while (parser_match(p, TOKEN_STAR))
    ptr++;

  ASTNode *n =
      ast_create_node(NODE_TYPE_ANNOTATION, p->current.line, p->current.column);
  n->type_annot.pointer_depth = ptr;
  n->type_annot.type_name = "int32";

  static const struct {
    TokenType tok;
    const char *name;
  } types[] = {{TOKEN_TYPE_INT32, "int32"},     {TOKEN_TYPE_INT64, "int64"},
               {TOKEN_TYPE_FLOAT32, "float32"}, {TOKEN_TYPE_FLOAT64, "float64"},
               {TOKEN_TYPE_UINT8, "uint8"},     {TOKEN_TYPE_UINT64, "uint64"},
               {TOKEN_TYPE_VOID, "void"},       {TOKEN_TYPE_STRING, "string"},
               {TOKEN_TYPE_BOOLEAN, "boolean"}, {TOKEN_TYPE_NUMBER, "number"},
               {TOKEN_TYPE_INT, "int"},         {TOKEN_EOF, NULL}};

  for (int i = 0; types[i].name; i++) {
    if (parser_match(p, types[i].tok)) {
      n->type_annot.type_name = (char *)types[i].name;
      while (parser_match(p, TOKEN_STAR))
        n->type_annot.pointer_depth++;
      return n;
    }
  }

  if (parser_check(p, TOKEN_IDENT)) {
    n->type_annot.type_name = toktext(p);
    parser_advance(p);
  }

  while (parser_match(p, TOKEN_STAR))
    n->type_annot.pointer_depth++;
  return n;
}
