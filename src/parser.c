#include "parser.h"
#include "keywords/keywords.h"
#include "utils.h"
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NODE_POOL_SIZE 4096
#define DEBUG_PARSER 0
#define DPRINTF_PARSER(fmt, ...)                                               \
  if (DEBUG_PARSER)                                                            \
  fprintf(stderr, "[PARSER] " fmt, ##__VA_ARGS__)

typedef struct NodePool {
  ASTNode nodes[NODE_POOL_SIZE];
  int next;
  struct NodePool *prev;
} NodePool;

static NodePool *current_pool = NULL;
jmp_buf error_jmp;
bool error_jmp_set = false;
static char **g_lines = NULL;
static int g_line_count = 0;

static ASTNode *ast_alloc_node(void) {
  if (!current_pool || current_pool->next >= NODE_POOL_SIZE) {
    NodePool *pool = calloc(1, sizeof(NodePool));
    if (!pool) {
      fprintf(stderr, "Fatal: Out of memory\n");
      abort();
    }
    pool->prev = current_pool;
    pool->next = 0;
    current_pool = pool;
  }
  ASTNode *n = &current_pool->nodes[current_pool->next++];
  memset(n, 0, sizeof(ASTNode));
  return n;
}

ASTNode *ast_create_node(NodeType type, int line, int column) {
  ASTNode *n = ast_alloc_node();
  n->type = type;
  n->line = line;
  n->column = column;
  return n;
}

void ast_destroy(ASTNode *n) { (void)n; }

void ast_destroy_pools(void) {
  while (current_pool) {
    NodePool *prev = current_pool->prev;
    for (int i = 0; i < current_pool->next; i++) {
      ASTNode *n = &current_pool->nodes[i];
      switch (n->type) {
      case NODE_KEYWORD:
        free(n->keyword.name);
        free(n->keyword.args);
        break;
      case NODE_CALL:
        free(n->call.name);
        free(n->call.args);
        break;
      case NODE_FUNCTION:
        free(n->func.name);
        free(n->func.params);
        free(n->func.param_types);
        break;
      case NODE_BLOCK:
      case NODE_PROGRAM:
        free(n->block.statements);
        break;
      case NODE_STRING_LITERAL:
        free(n->string_lit.value);
        break;
      case NODE_VARIABLE:
        free(n->variable.name);
        break;
      case NODE_BINARY_OP:
        free(n->binary.op);
        break;
      case NODE_UNARY_OP:
        free(n->unary.op);
        break;
      case NODE_STRUCT:
        free(n->struct_def.name);
        free(n->struct_def.fields);
        free(n->struct_def.field_names);
        free(n->struct_def.field_values);
        if (n->struct_def.has_methods)
          free(n->struct_def.methods);
        break;
      case NODE_ENUM:
        free(n->enum_def.name);
        free(n->enum_def.values);
        free(n->enum_def.value_exprs);
        break;
      case NODE_FIELD_ACCESS:
        free(n->field_access.field);
        break;
      case NODE_ASM_BLOCK:
        free(n->asm_block.code);
        break;
      case NODE_IMPORT:
        free(n->import.module_path);
        break;
      case NODE_TABLE:
        free(n->table.fields);
        free(n->table.field_names);
        break;
      case NODE_FOR:
        free(n->for_stmt.var);
        break;
      case NODE_LOCAL_VAR:
        free(n->local_var.name);
        break;
      case NODE_TYPE_CAST:
        free(n->cast.type_name);
        break;
      case NODE_ASSIGN:
        free(n->assign.op);
        break;
      default:
        break;
      }
    }
    free(current_pool);
    current_pool = prev;
  }
}

void parser_set_source(const char *source) {
  if (g_lines) {
    for (int i = 0; i < g_line_count; i++)
      free(g_lines[i]);
    free(g_lines);
  }

  int cap = 16;
  g_lines = malloc(sizeof(char *) * cap);
  g_line_count = 0;

  const char *start = source;
  const char *end = source;
  while (*end) {
    if (*end == '\n') {
      if (g_line_count >= cap) {
        cap *= 2;
        g_lines = realloc(g_lines, sizeof(char *) * cap);
      }
      int len = end - start;
      g_lines[g_line_count] = malloc(len + 1);
      strncpy(g_lines[g_line_count], start, len);
      g_lines[g_line_count][len] = '\0';
      g_line_count++;
      start = end + 1;
    }
    end++;
  }
  if (*start) {
    if (g_line_count >= cap) {
      cap *= 2;
      g_lines = realloc(g_lines, sizeof(char *) * cap);
    }
    g_lines[g_line_count] = strdup(start);
    g_line_count++;
  }
}

void parser_error(int line, int column, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);

  fprintf(stderr, "\033[1;31mError\033[0m");
  if (line > 0) {
    fprintf(stderr, " at line %d, column %d", line, column);
  }
  fprintf(stderr, ": ");
  vfprintf(stderr, fmt, args);
  fprintf(stderr, "\n");
  va_end(args);

  if (line > 0 && g_lines && line <= g_line_count) {
    const char *src = g_lines[line - 1];
    fprintf(stderr, "\n  %4d | %s\n", line, src);
    fprintf(stderr, "       | ");
    for (int i = 0; i < column - 1 && i < (int)strlen(src); i++) {
      fprintf(stderr, src[i] == '\t' ? "\t" : " ");
    }
    fprintf(stderr, "\033[31m");
    for (int i = 0; i < 10; i++) {
      fprintf(stderr, i % 2 ? "^" : "~");
    }
    fprintf(stderr, "\033[0m\n");
  }
}

void parser_warning(int line, int column, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);

  fprintf(stderr, "\033[1;33mWarning\033[0m");
  if (line > 0) {
    fprintf(stderr, " at line %d, column %d", line, column);
  }
  fprintf(stderr, ": ");
  vfprintf(stderr, fmt, args);
  fprintf(stderr, "\n");
  va_end(args);

  if (line > 0 && g_lines && line <= g_line_count) {
    const char *src = g_lines[line - 1];
    fprintf(stderr, "\n  %4d | %s\n", line, src);
    fprintf(stderr, "       | ");
    for (int i = 0; i < column - 1 && i < (int)strlen(src); i++) {
      fprintf(stderr, src[i] == '\t' ? "\t" : " ");
    }
    fprintf(stderr, "\033[33m");
    for (int i = 0; i < 10; i++) {
      fprintf(stderr, i % 2 ? "^" : "~");
    }
    fprintf(stderr, "\033[0m\n");
  }
}

void parser_advance(Parser *p) {
  DPRINTF_PARSER("parser_advance: pos=%d/%d\n", p->pos, p->token_count);
  if (p->pos < p->token_count) {
    p->current = p->tokens[p->pos++];
    DPRINTF_PARSER("  advanced to token type=%d text='%s'\n", p->current.type,
                   p->current.text ? p->current.text : "(null)");
  }
}

bool parser_check(Parser *p, TokenType t) { return p->current.type == t; }

bool parser_match(Parser *p, TokenType t) {
  if (parser_check(p, t)) {
    parser_advance(p);
    return true;
  }
  return false;
}

void parser_expect(Parser *p, TokenType t, const char *m) {
  if (p->current.type != t) {
    parser_error(p->current.line, p->current.column, "Expected %s but got '%s'",
                 m, p->current.text);
    if (error_jmp_set)
      longjmp(error_jmp, 1);
    exit(1);
  }
  parser_advance(p);
}

Parser *parser_create(Token *tokens, int count) {
  DPRINTF_PARSER("parser_create: %d tokens\n", count);
  Parser *p = calloc(1, sizeof(Parser));
  p->tokens = tokens;
  p->token_count = count;
  p->pos = 0;
  parser_advance(p);
  return p;
}

void parser_destroy(Parser *p) {
  DPRINTF_PARSER("parser_destroy\n");
  free(p);
}

Token *peek(Parser *p) {
  return p->pos < p->token_count ? &p->tokens[p->pos] : NULL;
}

char *toktext(Parser *p) { return strdup(p->current.text); }

bool is_import_call(Parser *p) {
  return parser_check(p, TOKEN_IDENT) &&
         strcmp(p->current.text, "import") == 0 && peek(p) &&
         peek(p)->type == TOKEN_LPAREN;
}

static bool is_keyword_import(const char *name) {
  for (int i = 0; keyword_handlers[i].name; i++) {
    if (strcmp(keyword_handlers[i].name, name) == 0)
      return true;
  }
  return false;
}

static void check_import_exists(ASTNode *node) {
  if (node->type == NODE_IMPORT && node->import.module_path) {
    const char *path = node->import.module_path;

    if (is_keyword_import(path)) {
      mark_import(path);
      return;
    }

    if (file_exists(path))
      return;

    char *lll_path = string_format("%s.lll", path);
    if (file_exists(lll_path)) {
      free(lll_path);
      return;
    }
    free(lll_path);

    char *so_path = string_format("%s.so", path);
    if (file_exists(so_path)) {
      free(so_path);
      return;
    }
    free(so_path);

    parser_error(node->line, node->column,
                 "Import '%s' not found (checked: %s, %s.lll, %s.so)", path,
                 path, path, path);
  }
}

static void check_type_annotations(ASTNode *node) {
  if (node->type == NODE_LOCAL_VAR && !node->local_var.type &&
      node->local_var.init) {
    parser_warning(node->line, node->column,
                   "Missing type annotation, inferred from initialization");
  }
  if (node->type == NODE_FUNCTION && !node->func.return_type) {
    parser_warning(node->line, node->column,
                   "Function '%s' missing return type annotation",
                   node->func.name ? node->func.name : "anonymous");
  }
}

void validate_ast(ASTNode *node) {
  if (!node)
    return;

  check_import_exists(node);
  check_type_annotations(node);

  switch (node->type) {
  case NODE_PROGRAM:
  case NODE_BLOCK:
    for (int i = 0; i < node->block.statement_count; i++) {
      validate_ast(node->block.statements[i]);
    }
    break;
  case NODE_FUNCTION:
    validate_ast(node->func.body);
    break;
  case NODE_IF:
    validate_ast(node->if_stmt.then_branch);
    if (node->if_stmt.else_branch)
      validate_ast(node->if_stmt.else_branch);
    break;
  case NODE_WHILE:
    validate_ast(node->while_stmt.body);
    break;
  case NODE_FOR:
    validate_ast(node->for_stmt.body);
    break;
  case NODE_REPEAT:
    validate_ast(node->repeat_stmt.body);
    break;
  default:
    break;
  }
}

ASTNode *parser_parse_program(Parser *p) {
  DPRINTF_PARSER("parser_parse_program START\n");
  error_jmp_set = true;

  if (setjmp(error_jmp) != 0) {
    DPRINTF_PARSER("longjmp caught, error recovery\n");
    error_jmp_set = false;
    ast_destroy_pools();
    return NULL;
  }

  ASTNode *prog = ast_create_node(NODE_PROGRAM, 1, 1);
  int cap = 16, cnt = 0;
  ASTNode **nodes = malloc(sizeof(ASTNode *) * cap);

  int max_iterations = 10000;
  int iterations = 0;

  while (!parser_check(p, TOKEN_EOF) && iterations < max_iterations) {
    iterations++;

    if (cnt >= cap) {
      cap *= 2;
      nodes = realloc(nodes, sizeof(ASTNode *) * cap);
    }

    int prev_pos = p->pos;
    int prev_token_type = p->current.type;

    ASTNode *stmt = parse_statement(p);

    if (stmt) {
      nodes[cnt++] = stmt;
      DPRINTF_PARSER("Parsed statement %d, type=%d, pos moved from %d to %d\n",
                     cnt, stmt->type, prev_pos, p->pos);
    } else {
      DPRINTF_PARSER("parse_statement returned NULL\n");
      if (p->pos <= prev_pos) {
        DPRINTF_PARSER(
            "PARSER STUCK! Position went backwards or stayed same: %d -> %d\n",
            prev_pos, p->pos);
        parser_error(p->current.line, p->current.column,
                     "Parser stuck - unable to parse statement at position %d",
                     p->pos);
        if (error_jmp_set) {
          longjmp(error_jmp, 1);
        }
        break;
      }
    }
  }

  if (iterations >= max_iterations) {
    DPRINTF_PARSER("Exceeded maximum iterations at position %d\n", p->pos);
    parser_error(p->current.line, p->current.column,
                 "Parser exceeded maximum iteration count");
    free(nodes);
    ast_destroy_pools();
    error_jmp_set = false;
    return NULL;
  }

  DPRINTF_PARSER("Program loop complete: %d iterations, %d statements\n",
                 iterations, cnt);
  prog->block.statement_count = cnt;
  prog->block.statements = nodes;

  DPRINTF_PARSER("Validating AST\n");
  validate_ast(prog);

  DPRINTF_PARSER("parser_parse_program COMPLETE\n");
  error_jmp_set = false;
  return prog;
}
