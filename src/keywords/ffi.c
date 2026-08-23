#include "../utils.h"
#include "keywords.h"
#include <stdlib.h>
#include <string.h>

ASTNode *parse_ffi(Parser *p, int line, int col) {
  parser_advance(p);
  parser_expect(p, TOKEN_DOT, ".");
  char *method = string_copy(p->current.text);
  parser_expect(p, TOKEN_IDENT, "method name");
  parser_expect(p, TOKEN_LPAREN, "(");

  ASTNode *node = ast_create_node(NODE_KEYWORD, line, col);
  node->keyword.name = string_format("ffi.%s", method);

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

static void emit_expr_arg(FILE *out, ASTNode *a) {
  if (!a) {
    fprintf(out, "NULL");
    return;
  }
  switch (a->type) {
  case NODE_STRING_LITERAL:
    fprintf(out, "\"%s\"", a->string_lit.value);
    break;
  case NODE_INT_LITERAL:
    fprintf(out, "(void*)%lld", (long long)a->int_lit.value);
    break;
  case NODE_FLOAT_LITERAL:
    fprintf(out, "(void*)%f", a->float_lit.value);
    break;
  case NODE_VARIABLE:
    fprintf(out, "(void*)%s", a->variable.name);
    break;
  case NODE_NIL_LITERAL:
    fprintf(out, "NULL");
    break;
  default:
    fprintf(out, "NULL");
    break;
  }
}

void codegen_ffi_c(FILE *out, ASTNode *node) {
  const char *m = node->keyword.name;
  ASTNode **a = node->keyword.args;
  int count = node->keyword.arg_count;

  if (strcmp(m, "ffi.load") == 0) {
#ifdef _WIN32
    fprintf(out, "(void*)LoadLibraryA(");
#else
    fprintf(out, "(void*)dlopen(");
#endif
    if (count > 0 && a[0]->type == NODE_STRING_LITERAL)
      fprintf(out, "\"%s\"", a[0]->string_lit.value);
    else if (count > 0 && a[0]->type == NODE_VARIABLE)
      fprintf(out, "%s", a[0]->variable.name);
    else
      fprintf(out, "NULL");
#ifdef _WIN32
    fprintf(out, ")");
#else
    fprintf(out, ", RTLD_NOW)");
#endif
  }

  else if (strcmp(m, "ffi.symbol") == 0) {
#ifdef _WIN32
    fprintf(out, "(void*)GetProcAddress((HMODULE)");
#else
    fprintf(out, "(void*)dlsym((void*)");
#endif
    if (count > 0 && a[0]->type == NODE_VARIABLE)
      fprintf(out, "%s", a[0]->variable.name);
    else
      fprintf(out, "NULL");
    fprintf(out, ", ");
    if (count > 1 && a[1]->type == NODE_STRING_LITERAL)
      fprintf(out, "\"%s\"", a[1]->string_lit.value);
    else if (count > 1 && a[1]->type == NODE_VARIABLE)
      fprintf(out, "%s", a[1]->variable.name);
    else
      fprintf(out, "NULL");
    fprintf(out, ")");
  }

  else if (strcmp(m, "ffi.close") == 0) {
#ifdef _WIN32
    fprintf(out, "FreeLibrary((HMODULE)");
#else
    fprintf(out, "dlclose((void*)");
#endif
    if (count > 0 && a[0]->type == NODE_VARIABLE)
      fprintf(out, "%s", a[0]->variable.name);
    else
      fprintf(out, "NULL");
    fprintf(out, ")");
  }

  else if (strcmp(m, "ffi.call") == 0) {
    if (count < 1) {
      fprintf(out, "NULL");
      return;
    }

    fprintf(out, "((void*(*)(");
    for (int i = 1; i < count; i++) {
      if (i > 1)
        fprintf(out, ", ");
      fprintf(out, "void*");
    }
    fprintf(out, "))");

    if (a[0]->type == NODE_VARIABLE)
      fprintf(out, "%s", a[0]->variable.name);
    else if (a[0]->type == NODE_INT_LITERAL)
      fprintf(out, "(void*)%lld", (long long)a[0]->int_lit.value);
    else
      fprintf(out, "NULL");

    fprintf(out, ")(");
    for (int i = 1; i < count; i++) {
      if (i > 1)
        fprintf(out, ", ");
      emit_expr_arg(out, a[i]);
    }
    fprintf(out, ")");
  }

  else if (strcmp(m, "ffi.type") == 0) {
    if (count < 1) {
      fprintf(out, "0");
      return;
    }
    fprintf(out, "sizeof(");
    if (a[0]->type == NODE_STRING_LITERAL) {
      const char *tn = a[0]->string_lit.value;
      if (strcmp(tn, "int32") == 0)
        fprintf(out, "int32_t");
      else if (strcmp(tn, "int64") == 0)
        fprintf(out, "int64_t");
      else if (strcmp(tn, "float32") == 0)
        fprintf(out, "float");
      else if (strcmp(tn, "float64") == 0)
        fprintf(out, "double");
      else if (strcmp(tn, "uint8") == 0)
        fprintf(out, "uint8_t");
      else if (strcmp(tn, "uint64") == 0)
        fprintf(out, "uint64_t");
      else if (strcmp(tn, "string") == 0)
        fprintf(out, "char*");
      else if (strcmp(tn, "boolean") == 0)
        fprintf(out, "bool");
      else if (strcmp(tn, "void") == 0)
        fprintf(out, "void");
      else
        fprintf(out, "%s", tn);
    } else if (a[0]->type == NODE_VARIABLE) {
      fprintf(out, "%s", a[0]->variable.name);
    } else {
      fprintf(out, "void*");
    }
    fprintf(out, ")");
  }

  else {
    fprintf(out, "NULL");
  }
}
