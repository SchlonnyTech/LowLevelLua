#include "keywords.h"
#include "utils.h"
#include <stdlib.h>
#include <string.h>

ASTNode *parse_zstd(Parser *p, int line, int col) {
  parser_advance(p);
  parser_expect(p, TOKEN_DOT, ".");
  char *method = string_copy(p->current.text);
  parser_expect(p, TOKEN_IDENT, "method name");
  parser_expect(p, TOKEN_LPAREN, "(");
  ASTNode *node = ast_create_node(NODE_KEYWORD, line, col);
  node->keyword.name = string_format("zstd.%s", method);
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

static void ea(FILE *out, ASTNode *a) {
  if (!a) {
    fprintf(out, "0");
    return;
  }
  switch (a->type) {
  case NODE_STRING_LITERAL:
    fprintf(out, "\"%s\"", a->string_lit.value);
    break;
  case NODE_INT_LITERAL:
    fprintf(out, "%lldLL", (long long)a->int_lit.value);
    break;
  case NODE_VARIABLE:
    fprintf(out, "%s", a->variable.name);
    break;
  case NODE_CALL:
    fprintf(out, "%s(", a->call.name);
    for (int i = 0; i < a->call.arg_count; i++) {
      if (i)
        fputc(',', out);
      ea(out, a->call.args[i]);
    }
    fputc(')', out);
    break;
  default:
    fprintf(out, "0");
    break;
  }
}

void codegen_zstd_c(FILE *out, ASTNode *node) {
  const char *m = node->keyword.name;
  ASTNode **a = node->keyword.args;

  if (strcmp(m, "zstd.compress") == 0) {
    fputs("({size_t _os=", out);
    ea(out, a[1]);
    fputs(";size_t _bs=ZSTD_compressBound(_os)+16;void*_d=malloc(_bs);", out);
    fputs("*(size_t*)_d=_os;size_t _cs=ZSTD_compress((char*)_d+16,_bs-16,",
          out);
    ea(out, a[0]);
    fputs(",_os,3);((size_t*)_d)[1]=_cs;(char*)_d;})", out);
  } else if (strcmp(m, "zstd.decompress") == 0) {
    fputs("({size_t _os=((size_t*)(", out);
    ea(out, a[0]);
    fputs("))[0];size_t _cs=((size_t*)(", out);
    ea(out, a[0]);
    fputs("))[1];void*_d=malloc(_os+1);ZSTD_decompress(_d,_os,(char*)(", out);
    ea(out, a[0]);
    fputs(")+16,_cs);((char*)_d)[_os]=0;(char*)_d;})", out);
  } else if (strcmp(m, "zstd.compressBound") == 0) {
    fputs("(int64_t)ZSTD_compressBound(", out);
    ea(out, a[0]);
    fputc(')', out);
  } else if (strcmp(m, "zstd.compressedSize") == 0) {
    fputs("(int64_t)(((size_t*)(", out);
    ea(out, a[0]);
    fputs("))[1])", out);
  } else if (strcmp(m, "zstd.originalSize") == 0) {
    fputs("(int64_t)(((size_t*)(", out);
    ea(out, a[0]);
    fputs("))[0])", out);
  } else if (strcmp(m, "zstd.isError") == 0) {
    fputs("(int64_t)ZSTD_isError(", out);
    ea(out, a[0]);
    fputc(')', out);
  } else if (strcmp(m, "zstd.maxCLevel") == 0) {
    fputs("(int64_t)ZSTD_maxCLevel()", out);
  } else if (strcmp(m, "zstd.version") == 0) {
    fputs("(int64_t)ZSTD_versionNumber()", out);
  } else {
    fputs("0", out);
  }
}
