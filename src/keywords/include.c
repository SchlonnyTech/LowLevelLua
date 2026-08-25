#include "../codegen.h"
#include "../lexer.h"
#include "../parser.h"
#include "../utils.h"
#include "keywords.h"
#include <llvm-c/Core.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *read_include_file(const char *path) {
  char *full_path = string_format("%s.lll", path);
  char *src = read_file(full_path);
  if (!src) {
    src = read_file(path);
  }
  free(full_path);
  return src;
}

static LLVMValueRef kinc_include(CodeGenContext *ctx, FILE *out,
                                 ASTNode *node) {
  (void)out;
  if (node->keyword.arg_count < 1)
    return NULL;

  ASTNode *arg = node->keyword.args[0];
  if (arg->type != NODE_STRING_LITERAL)
    return NULL;

  const char *path = arg->string_lit.value;
  char *src = read_include_file(path);
  if (!src) {
    codegen_error(ctx, "Cannot include file '%s'", path);
    return NULL;
  }

  parser_set_source(src);
  Lexer *lexer = lexer_create(src);
  int token_count = 0;
  Token *tokens = lexer_tokenize(lexer, &token_count);
  lexer_destroy(lexer);

  Parser *parser = parser_create(tokens, token_count);
  ASTNode *included_ast = parser_parse_program(parser);
  parser_destroy(parser);

  free(tokens);
  free(src);

  if (!included_ast)
    return NULL;

  LLVMBasicBlockRef saved_block = LLVMGetInsertBlock(ctx->builder);

  for (int i = 0; i < included_ast->block.statement_count; i++) {
    ASTNode *stmt = included_ast->block.statements[i];
    if (!stmt)
      continue;

    if (stmt->type == NODE_FUNCTION) {
      codegen_function(ctx, stmt);
      LLVMPositionBuilderAtEnd(ctx->builder, saved_block);
    } else {
      codegen_stmt(ctx, stmt);
    }
  }

  LLVMPositionBuilderAtEnd(ctx->builder, saved_block);
  return NULL;
}

static LLVMValueRef kinc_loadlib(CodeGenContext *ctx, FILE *out,
                                 ASTNode *node) {
  (void)out;
  if (node->keyword.arg_count < 1)
    return NULL;

  ASTNode *arg = node->keyword.args[0];
  if (arg->type != NODE_STRING_LITERAL)
    return NULL;

  const char *libname = arg->string_lit.value;
  LLVMTypeRef i8p = LLVMPointerType(LLVMInt8TypeInContext(ctx->llvm_ctx), 0);
  LLVMTypeRef i32 = LLVMInt32TypeInContext(ctx->llvm_ctx);

#ifdef _WIN32
  LLVMTypeRef ft = LLVMFunctionType(i8p, (LLVMTypeRef[]){i8p}, 1, 0);
  LLVMValueRef fn = LLVMGetNamedFunction(ctx->module, "LoadLibraryA");
  if (!fn)
    fn = LLVMAddFunction(ctx->module, "LoadLibraryA", ft);
  LLVMValueRef path = codegen_string_create(ctx, libname);
  LLVMValueRef args[] = {path};
  return LLVMBuildCall2(ctx->builder, ft, fn, args, 1, "loadlib");
#else
  LLVMTypeRef ft = LLVMFunctionType(i8p, (LLVMTypeRef[]){i8p, i32}, 2, 0);
  LLVMValueRef fn = LLVMGetNamedFunction(ctx->module, "dlopen");
  if (!fn)
    fn = LLVMAddFunction(ctx->module, "dlopen", ft);
  LLVMValueRef path = codegen_string_create(ctx, libname);
  LLVMValueRef flags = LLVMConstInt(i32, 2, 0);
  LLVMValueRef args[] = {path, flags};
  return LLVMBuildCall2(ctx->builder, ft, fn, args, 2, "dlopen");
#endif
}

static LLVMValueRef kinc_import(CodeGenContext *ctx, FILE *out, ASTNode *node) {
  return kinc_include(ctx, out, node);
}

static KeywordHandler include_handlers[] = {
    {"include", kinc_include, NULL, 1},
    {"import", kinc_import, NULL, 1},
    {"loadlib", kinc_loadlib, NULL, 1},
    {NULL, NULL, NULL, 0},
};

void register_include_keywords(void) {
  for (int i = 0; include_handlers[i].name; i++) {
    register_keyword_handler(&include_handlers[i]);
  }
}
