#include "../utils.h"
#include "keywords.h"
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>

extern jmp_buf error_jmp;
extern bool error_jmp_set;

static void validate_arg_count(const char *method, int expected, int actual,
                               int line, int col) {
  if (actual != expected) {
    error_report(line, col,
                 "mem.%s requires exactly %d argument%s, but %d %s provided",
                 method, expected, expected == 1 ? "" : "s", actual,
                 actual == 1 ? "was" : "were");
    if (error_jmp_set)
      longjmp(error_jmp, 1);
  }
}

ASTNode *parse_memory(Parser *p, int line, int col) {
  parser_advance(p);
  parser_expect(p, TOKEN_DOT, ".");
  char *method = string_copy(p->current.text);
  parser_expect(p, TOKEN_IDENT, "method name");
  parser_expect(p, TOKEN_LPAREN, "(");

  ASTNode *node = ast_create_node(NODE_KEYWORD, line, col);
  node->keyword.name = string_format("mem.%s", method);

  Array *args = array_create(8);
  if (!parser_check(p, TOKEN_RPAREN)) {
    array_push(args, parse_expression(p));
    while (parser_match(p, TOKEN_COMMA))
      array_push(args, parse_expression(p));
  }
  parser_expect(p, TOKEN_RPAREN, ")");

  int argc = args->count;

  if (string_equals(method, "alloc")) {
    validate_arg_count(method, 1, argc, line, col);
  } else if (string_equals(method, "free")) {
    validate_arg_count(method, 1, argc, line, col);
  } else if (string_equals(method, "realloc")) {
    validate_arg_count(method, 2, argc, line, col);
  } else if (string_equals(method, "copy")) {
    validate_arg_count(method, 3, argc, line, col);
  } else if (string_equals(method, "move")) {
    validate_arg_count(method, 3, argc, line, col);
  } else if (string_equals(method, "set")) {
    validate_arg_count(method, 3, argc, line, col);
  } else if (string_equals(method, "zero")) {
    validate_arg_count(method, 2, argc, line, col);
  } else if (string_equals(method, "compare")) {
    validate_arg_count(method, 3, argc, line, col);
  } else if (string_equals(method, "find")) {
    validate_arg_count(method, 3, argc, line, col);
  } else if (string_equals(method, "swap")) {
    validate_arg_count(method, 3, argc, line, col);
  } else if (string_equals(method, "dup")) {
    validate_arg_count(method, 2, argc, line, col);
  } else if (string_equals(method, "sizeof")) {
    validate_arg_count(method, 1, argc, line, col);
  }

  node->keyword.arg_count = argc;
  node->keyword.args = malloc(sizeof(ASTNode *) * argc);
  for (int i = 0; i < argc; i++)
    node->keyword.args[i] = (ASTNode *)args->items[i];
  array_destroy(args);
  free(method);
  return node;
}

static void emit_arg(FILE *out, ASTNode *a) {
  if (!a) {
    fprintf(out, "0");
    return;
  }
  if (a->type == NODE_STRING_LITERAL)
    fprintf(out, "\"%s\"", a->string_lit.value);
  else if (a->type == NODE_INT_LITERAL)
    fprintf(out, "%lld", (long long)a->int_lit.value);
  else if (a->type == NODE_FLOAT_LITERAL)
    fprintf(out, "%f", a->float_lit.value);
  else if (a->type == NODE_VARIABLE)
    fprintf(out, "%s", a->variable.name);
  else if (a->type == NODE_KEYWORD)
    codegen_keyword_c(out, a);
  else
    fprintf(out, "0");
}

void codegen_memory_c(FILE *out, ASTNode *node) {
  const char *m = node->keyword.name;
  ASTNode **a = node->keyword.args;
  int count = node->keyword.arg_count;

  if (string_equals(m, "mem.alloc")) {
    if (count < 1) {
      fprintf(out, "NULL");
      return;
    }
    fprintf(out, "malloc(");
    emit_arg(out, a[0]);
    fprintf(out, ")");
  } else if (string_equals(m, "mem.free")) {
    if (count < 1) {
      fprintf(out, ";");
      return;
    }
    fprintf(out, "free(");
    emit_arg(out, a[0]);
    fprintf(out, ");\n");
  } else if (string_equals(m, "mem.realloc")) {
    if (count < 2) {
      fprintf(out, "NULL");
      return;
    }
    fprintf(out, "realloc(");
    emit_arg(out, a[0]);
    fprintf(out, ", ");
    emit_arg(out, a[1]);
    fprintf(out, ")");
  } else if (string_equals(m, "mem.copy")) {
    if (count < 3) {
      fprintf(out, ";");
      return;
    }
    fprintf(out, "memcpy(");
    emit_arg(out, a[0]);
    fprintf(out, ", ");
    emit_arg(out, a[1]);
    fprintf(out, ", ");
    emit_arg(out, a[2]);
    fprintf(out, ");\n");
  } else if (string_equals(m, "mem.move")) {
    if (count < 3) {
      fprintf(out, ";");
      return;
    }
    fprintf(out, "memmove(");
    emit_arg(out, a[0]);
    fprintf(out, ", ");
    emit_arg(out, a[1]);
    fprintf(out, ", ");
    emit_arg(out, a[2]);
    fprintf(out, ");\n");
  } else if (string_equals(m, "mem.set")) {
    if (count < 3) {
      fprintf(out, ";");
      return;
    }
    fprintf(out, "memset(");
    emit_arg(out, a[0]);
    fprintf(out, ", ");
    emit_arg(out, a[1]);
    fprintf(out, ", ");
    emit_arg(out, a[2]);
    fprintf(out, ");\n");
  } else if (string_equals(m, "mem.zero")) {
    if (count < 2) {
      fprintf(out, ";");
      return;
    }
    fprintf(out, "memset(");
    emit_arg(out, a[0]);
    fprintf(out, ", 0, ");
    emit_arg(out, a[1]);
    fprintf(out, ");\n");
  } else if (string_equals(m, "mem.compare")) {
    if (count < 3) {
      fprintf(out, "0");
      return;
    }
    fprintf(out, "memcmp(");
    emit_arg(out, a[0]);
    fprintf(out, ", ");
    emit_arg(out, a[1]);
    fprintf(out, ", ");
    emit_arg(out, a[2]);
    fprintf(out, ")");
  } else if (string_equals(m, "mem.find")) {
    if (count < 3) {
      fprintf(out, "NULL");
      return;
    }
    fprintf(out, "memchr(");
    emit_arg(out, a[0]);
    fprintf(out, ", ");
    emit_arg(out, a[1]);
    fprintf(out, ", ");
    emit_arg(out, a[2]);
    fprintf(out, ")");
  } else if (string_equals(m, "mem.swap")) {
    if (count < 3) {
      fprintf(out, ";");
      return;
    }
    fprintf(out, "{\n    unsigned char _t[");
    emit_arg(out, a[2]);
    fprintf(out, "];\n    memcpy(_t, ");
    emit_arg(out, a[0]);
    fprintf(out, ", ");
    emit_arg(out, a[2]);
    fprintf(out, ");\n    memcpy(");
    emit_arg(out, a[0]);
    fprintf(out, ", ");
    emit_arg(out, a[1]);
    fprintf(out, ", ");
    emit_arg(out, a[2]);
    fprintf(out, ");\n    memcpy(");
    emit_arg(out, a[1]);
    fprintf(out, ", _t, ");
    emit_arg(out, a[2]);
    fprintf(out, ");\n}\n");
  } else if (string_equals(m, "mem.dup")) {
    if (count < 2) {
      fprintf(out, "NULL");
      return;
    }
    fprintf(out, "lll_mem_dup(");
    emit_arg(out, a[0]);
    fprintf(out, ", ");
    emit_arg(out, a[1]);
    fprintf(out, ")");
  } else if (string_equals(m, "mem.sizeof")) {
    if (count < 1) {
      fprintf(out, "0");
      return;
    }
    fprintf(out, "sizeof(");
    emit_arg(out, a[0]);
    fprintf(out, ")");
  }
}
