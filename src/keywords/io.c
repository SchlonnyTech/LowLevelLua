#include "../utils.h"
#include "keywords.h"
#include <stdlib.h>

ASTNode *parse_io(Parser *p, int line, int col) {
  parser_advance(p);
  parser_expect(p, TOKEN_DOT, ".");
  char *method = string_copy(p->current.text);
  parser_expect(p, TOKEN_IDENT, "method name");
  parser_expect(p, TOKEN_LPAREN, "(");
  ASTNode *node = ast_create_node(NODE_KEYWORD, line, col);
  node->keyword.name = string_format("io.%s", method);
  Array *args = array_create(8);
  if (!parser_check(p, TOKEN_RPAREN)) {
    array_push(args, parse_expression(p));
    while (parser_match(p, TOKEN_COMMA))
      array_push(args, parse_expression(p));
  }
  parser_expect(p, TOKEN_RPAREN, ")");
  node->keyword.arg_count = args->count;
  node->keyword.args = malloc(sizeof(ASTNode *) * args->count);
  for (int i = 0; i < args->count; i++)
    node->keyword.args[i] = (ASTNode *)args->items[i];
  array_destroy(args);
  free(method);
  return node;
}

void codegen_io_c(FILE *out, ASTNode *node) {
  const char *m = node->keyword.name;
  ASTNode **a = node->keyword.args;
  int n = node->keyword.arg_count;

  if (string_equals(m, "io.print")) {
    fprintf(out, "printf(");
    for (int i = 0; i < n; i++) {
      if (i > 0)
        fprintf(out, ", ");
      if (a[i]->type == NODE_STRING_LITERAL)
        fprintf(out, "\"%s\"", a[i]->string_lit.value);
      else if (a[i]->type == NODE_INT_LITERAL)
        fprintf(out, "%lld", (long long)a[i]->int_lit.value);
      else
        fprintf(out, "%s", a[i]->variable.name);
    }
    fprintf(out, ");\n");
  } else if (string_equals(m, "io.open")) {
    fprintf(out, "fopen(");
    fprintf(out, a[0]->type == NODE_STRING_LITERAL ? "\"%s\"" : "%s",
            a[0]->type == NODE_STRING_LITERAL ? a[0]->string_lit.value
                                              : a[0]->variable.name);
    fprintf(out, ", ");
    fprintf(out, a[1]->type == NODE_STRING_LITERAL ? "\"%s\"" : "%s",
            a[1]->type == NODE_STRING_LITERAL ? a[1]->string_lit.value
                                              : a[1]->variable.name);
    fprintf(out, ")");
  } else if (string_equals(m, "io.close"))
    fprintf(out, "fclose(%s)", a[0]->variable.name);
  else if (string_equals(m, "io.remove"))
    fprintf(out, "remove(\"%s\")", a[0]->string_lit.value);
  else if (string_equals(m, "io.rename"))
    fprintf(out, "rename(\"%s\", \"%s\")", a[0]->string_lit.value,
            a[1]->string_lit.value);
  else if (string_equals(m, "io.exit"))
    fprintf(out, "exit(%s)", n > 0 ? a[0]->variable.name : "0");
  else if (string_equals(m, "io.system"))
    fprintf(out, "system(\"%s\")", a[0]->string_lit.value);
  else
    fprintf(out, "0");
}
