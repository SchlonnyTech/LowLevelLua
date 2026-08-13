#include "keywords/keywords.h"
#include "parser.h"
#include "utils.h"
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEBUG_STMT 0
#define DPRINTF_STMT(fmt, ...)                                                 \
  if (DEBUG_STMT)                                                              \
  fprintf(stderr, "[STMT] " fmt, ##__VA_ARGS__)

extern jmp_buf error_jmp;
extern bool error_jmp_set;

static ASTNode *parse_function_common(Parser *p, int line, int col,
                                      bool is_local) {
  ASTNode *n = ast_create_node(NODE_FUNCTION, line, col);
  if (parser_check(p, TOKEN_IDENT)) {
    n->func.name = toktext(p);
    parser_advance(p);
  } else
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
  n->func.is_local = is_local;
  n->func.is_exported = !is_local;
  free(params);
  free(types);
  return n;
}

static ASTNode *parse_local(Parser *p, int line, int col) {
  if (parser_check(p, TOKEN_FUNCTION)) {
    parser_advance(p);
    return parse_function_common(p, line, col, true);
  }
  ASTNode *n = ast_create_node(NODE_LOCAL_VAR, line, col);
  if (parser_check(p, TOKEN_IDENT)) {
    n->local_var.name = toktext(p);
    parser_advance(p);
    if (parser_match(p, TOKEN_COMMA)) {
      ASTNode *block = ast_create_node(NODE_BLOCK, line, col);
      int cap = 4, cnt = 0;
      ASTNode **stmts = malloc(sizeof(ASTNode *) * cap);
      stmts[cnt++] = n;
      do {
        if (cnt >= cap) {
          cap *= 2;
          stmts = realloc(stmts, sizeof(ASTNode *) * cap);
        }
        ASTNode *v = ast_create_node(NODE_LOCAL_VAR, line, col);
        v->local_var.name = toktext(p);
        parser_expect(p, TOKEN_IDENT, "var name");
        stmts[cnt++] = v;
      } while (parser_match(p, TOKEN_COMMA));
      if (parser_match(p, TOKEN_EQUALS) || parser_match(p, TOKEN_WALRUS))
        for (int i = 0; i < cnt; i++) {
          if (i > 0 && !parser_match(p, TOKEN_COMMA))
            break;
          stmts[i]->local_var.init = parse_expression(p);
        }
      block->block.statement_count = cnt;
      block->block.statements = stmts;
      return block;
    }
    if (parser_match(p, TOKEN_COLON))
      n->local_var.type = parse_type(p);
    if (parser_match(p, TOKEN_EQUALS) || parser_match(p, TOKEN_WALRUS)) {
      if (is_import_call(p)) {
        parser_advance(p);
        parser_advance(p);
        ASTNode *imp = ast_create_node(NODE_IMPORT, line, col);
        if (parser_check(p, TOKEN_STRING)) {
          imp->import.module_path = toktext(p);
          parser_advance(p);
        } else {
          imp->import.module_path = toktext(p);
          parser_expect(p, TOKEN_IDENT, "module");
        }
        parser_expect(p, TOKEN_RPAREN, ")");
        mark_import(imp->import.module_path);
        n->local_var.init = imp;
      } else
        n->local_var.init = parse_expression(p);
    }
  }
  return n;
}

static ASTNode *parse_if(Parser *p, int line, int col) {
  ASTNode *n = ast_create_node(NODE_IF, line, col);
  n->if_stmt.condition = parse_expression(p);
  parser_expect(p, TOKEN_THEN, "then");
  n->if_stmt.then_branch = parse_block(p);
  ASTNode *current = n;
  while (parser_match(p, TOKEN_ELSEIF)) {
    ASTNode *elif =
        ast_create_node(NODE_IF, p->current.line, p->current.column);
    elif->if_stmt.condition = parse_expression(p);
    parser_expect(p, TOKEN_THEN, "then");
    elif->if_stmt.then_branch = parse_block(p);
    current->if_stmt.else_branch = elif;
    current = elif;
  }
  if (parser_match(p, TOKEN_ELSE))
    current->if_stmt.else_branch = parse_block(p);
  parser_expect(p, TOKEN_END, "end");
  return n;
}

static ASTNode *parse_while(Parser *p, int line, int col) {
  ASTNode *n = ast_create_node(NODE_WHILE, line, col);
  n->while_stmt.condition = parse_expression(p);
  parser_expect(p, TOKEN_DO, "do");
  n->while_stmt.body = parse_block(p);
  parser_expect(p, TOKEN_END, "end");
  return n;
}

static ASTNode *parse_repeat(Parser *p, int line, int col) {
  ASTNode *n = ast_create_node(NODE_REPEAT, line, col);
  n->repeat_stmt.body = parse_block(p);
  parser_expect(p, TOKEN_UNTIL, "until");
  n->repeat_stmt.condition = parse_expression(p);
  return n;
}

static ASTNode *parse_for(Parser *p, int line, int col) {
  ASTNode *n = ast_create_node(NODE_FOR, line, col);
  n->for_stmt.var = toktext(p);
  parser_expect(p, TOKEN_IDENT, "loop var");
  parser_expect(p, TOKEN_EQUALS, "=");
  n->for_stmt.start = parse_expression(p);
  parser_expect(p, TOKEN_COMMA, ",");
  n->for_stmt.end = parse_expression(p);
  if (parser_match(p, TOKEN_COMMA))
    n->for_stmt.step = parse_expression(p);
  parser_expect(p, TOKEN_DO, "do");
  n->for_stmt.body = parse_block(p);
  parser_expect(p, TOKEN_END, "end");
  return n;
}

static ASTNode *parse_struct(Parser *p, int line, int col) {
  ASTNode *n = ast_create_node(NODE_STRUCT, line, col);
  n->struct_def.name = toktext(p);
  parser_expect(p, TOKEN_IDENT, "struct name");
  int cap = 8, cnt = 0;
  ASTNode **fields = malloc(sizeof(ASTNode *) * cap);
  char **names = malloc(sizeof(char *) * cap);
  ASTNode **values = calloc(cap, sizeof(ASTNode *));
  while (!parser_check(p, TOKEN_END) && !parser_check(p, TOKEN_EOF)) {
    if (!parser_check(p, TOKEN_IDENT))
      break;
    if (cnt >= cap) {
      cap *= 2;
      fields = realloc(fields, sizeof(ASTNode *) * cap);
      names = realloc(names, sizeof(char *) * cap);
      values = realloc(values, sizeof(ASTNode *) * cap);
    }
    names[cnt] = toktext(p);
    parser_advance(p);
    parser_expect(p, TOKEN_COLON, ":");
    fields[cnt] = parse_type(p);
    if (parser_match(p, TOKEN_EQUALS))
      values[cnt] = parse_expression(p);
    cnt++;
  }
  parser_expect(p, TOKEN_END, "end");
  n->struct_def.field_count = cnt;
  n->struct_def.fields = fields;
  n->struct_def.field_names = names;
  n->struct_def.field_values = values;
  return n;
}

static ASTNode *parse_enum(Parser *p, int line, int col) {
  ASTNode *n = ast_create_node(NODE_ENUM, line, col);
  n->enum_def.name = toktext(p);
  parser_expect(p, TOKEN_IDENT, "enum name");
  int cap = 8, cnt = 0;
  char **values = malloc(sizeof(char *) * cap);
  ASTNode **exprs = calloc(cap, sizeof(ASTNode *));
  while (!parser_check(p, TOKEN_END) && !parser_check(p, TOKEN_EOF)) {
    if (cnt >= cap) {
      cap *= 2;
      values = realloc(values, sizeof(char *) * cap);
      exprs = realloc(exprs, sizeof(ASTNode *) * cap);
    }
    values[cnt] = toktext(p);
    parser_expect(p, TOKEN_IDENT, "enum value");
    if (parser_match(p, TOKEN_EQUALS))
      exprs[cnt] = parse_expression(p);
    cnt++;
  }
  parser_expect(p, TOKEN_END, "end");
  n->enum_def.value_count = cnt;
  n->enum_def.values = values;
  n->enum_def.value_exprs = exprs;
  return n;
}

ASTNode *parse_statement(Parser *p) {
  int line = p->current.line, col = p->current.column;
  DPRINTF_STMT("parse_statement: type=%d text='%s' pos=%d\n", p->current.type,
               p->current.text ? p->current.text : "(null)", p->pos);

  ASTNode *kw = parse_keyword_statement(p);
  if (kw)
    return kw;

  if (parser_match(p, TOKEN_LOCAL))
    return parse_local(p, line, col);
  if (parser_match(p, TOKEN_IF))
    return parse_if(p, line, col);
  if (parser_match(p, TOKEN_WHILE))
    return parse_while(p, line, col);
  if (parser_match(p, TOKEN_REPEAT))
    return parse_repeat(p, line, col);
  if (parser_match(p, TOKEN_FOR))
    return parse_for(p, line, col);
  if (parser_match(p, TOKEN_FUNCTION))
    return parse_function_common(p, line, col, false);
  if (parser_match(p, TOKEN_STRUCT))
    return parse_struct(p, line, col);
  if (parser_match(p, TOKEN_ENUM))
    return parse_enum(p, line, col);

  if (parser_match(p, TOKEN_RETURN)) {
    ASTNode *n = ast_create_node(NODE_RETURN, line, col);
    if (!parser_check(p, TOKEN_END) && !parser_check(p, TOKEN_EOF) &&
        !parser_check(p, TOKEN_ELSE) && !parser_check(p, TOKEN_ELSEIF) &&
        !parser_check(p, TOKEN_UNTIL)) {
      n->return_stmt.expr = parse_expression(p);
      while (parser_match(p, TOKEN_COMMA))
        parse_expression(p);
    }
    return n;
  }

  if (parser_match(p, TOKEN_DO)) {
    ASTNode *n = parse_block(p);
    parser_expect(p, TOKEN_END, "end");
    return n;
  }

  if (parser_match(p, TOKEN_BREAK))
    return ast_create_node(NODE_BREAK, line, col);
  if (parser_match(p, TOKEN_CONTINUE))
    return ast_create_node(NODE_CONTINUE, line, col);
  if (parser_match(p, TOKEN_DEFER)) {
    ASTNode *n = ast_create_node(NODE_DEFER, line, col);
    n->defer_stmt.expr = parse_expression(p);
    return n;
  }

  if (parser_match(p, TOKEN_ASM)) {
    parser_expect(p, TOKEN_LPAREN, "(");
    ASTNode *n = ast_create_node(NODE_ASM_BLOCK, line, col);
    n->asm_block.code = toktext(p);
    parser_expect(p, TOKEN_STRING, "asm code");
    parser_expect(p, TOKEN_RPAREN, ")");
    return n;
  }

  return parse_expression(p);
}

ASTNode *parse_block(Parser *p) {
  int line = p->current.line, col = p->current.column;
  ASTNode *block = ast_create_node(NODE_BLOCK, line, col);
  int cap = 16, cnt = 0;
  ASTNode **stmts = malloc(sizeof(ASTNode *) * cap);
  while (!parser_check(p, TOKEN_END) && !parser_check(p, TOKEN_ELSE) &&
         !parser_check(p, TOKEN_ELSEIF) && !parser_check(p, TOKEN_UNTIL) &&
         !parser_check(p, TOKEN_EOF)) {
    if (cnt >= cap) {
      cap *= 2;
      stmts = realloc(stmts, sizeof(ASTNode *) * cap);
    }
    stmts[cnt++] = parse_statement(p);
    parser_match(p, TOKEN_SEMICOLON);
  }
  block->block.statement_count = cnt;
  block->block.statements = stmts;
  return block;
}
