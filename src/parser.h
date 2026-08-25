#ifndef PARSER_H
#define PARSER_H

#include "ast.h"
#include "lexer.h"

typedef struct Parser {
  Token *tokens;
  int token_count;
  int pos;
  Token current;
} Parser;

Parser *parser_create(Token *tokens, int count);
void parser_destroy(Parser *p);
void parser_advance(Parser *p);
bool parser_check(Parser *p, TokenType t);
bool parser_match(Parser *p, TokenType t);
void parser_expect(Parser *p, TokenType t, const char *m);

void parser_error(int line, int column, const char *fmt, ...);
void parser_warning(int line, int column, const char *fmt, ...);

ASTNode *parse_expression(Parser *p);
ASTNode *parse_type(Parser *p);
ASTNode *parse_statement(Parser *p);
ASTNode *parse_block(Parser *p);
ASTNode *parser_parse_program(Parser *p);
void parser_set_source(const char *source);

Token *peek(Parser *p);
char *toktext(Parser *p);
bool is_import_call(Parser *p);
ASTNode *parse_keyword_statement(Parser *p);
#endif
