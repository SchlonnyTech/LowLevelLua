#ifndef LLL_PLUGIN_H
#define LLL_PLUGIN_H

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#define LLL_PLUGIN_API_VERSION 1

// Remove these - they conflict with codegen.h
// typedef struct CodeGenContext CodeGenContext;
// typedef struct ASTNode ASTNode;

typedef struct PluginAPI {
  int version;

  void *(*register_keyword)(const char *name, void *handler);
  void *(*register_type)(const char *name, void *handler);
  void *(*register_function)(const char *name, void *handler);
  void *(*register_variable)(const char *name, void *handler);
  void *(*register_operator)(const char *op, void *handler);
  void *(*register_statement)(const char *name, void *handler);
  void *(*register_expression)(const char *name, void *handler);
  void *(*register_directive)(const char *name, void *handler);

  int (*emit_code)(void *ctx, void *node, FILE *out);
  int (*handle_expr)(void *ctx, void *node, FILE *out);
  int (*handle_stmt)(void *ctx, void *node, FILE *out);
  void (*emit_preamble)(void *ctx, FILE *out);
  void (*emit_epilogue)(void *ctx, FILE *out);
  void (*emit_header)(void *ctx, FILE *out);
  void (*emit_footer)(void *ctx, FILE *out);

  void *(*parse_statement)(void *parser, void *token);
  void *(*parse_expression)(void *parser, void *token);

  void *(*get_context)(void);
  void (*error)(int line, int col, const char *fmt, ...);
  void (*warning)(int line, int col, const char *fmt, ...);
  void (*debug)(const char *fmt, ...);

  void *(*malloc)(size_t size);
  void *(*calloc)(size_t nmemb, size_t size);
  void *(*realloc)(void *ptr, size_t size);
  void (*free)(void *ptr);
  char *(*strdup)(const char *s);

  char *(*string_format)(const char *fmt, ...);
  char *(*string_copy)(const char *s);

  bool (*file_exists)(const char *path);
  char *(*read_file)(const char *path);
  bool (*write_file)(const char *path, const char *content);

  void *(*get_plugin_data)(void);
  void (*set_plugin_data)(void *data);

  void *reserved[16];
} PluginAPI;

typedef struct PluginInfo {
  const char *name;
  const char *version;
  const char *author;
  const char *description;
  const char *license;
  const char *website;
  int api_version;
  int plugin_version;
} PluginInfo;

typedef int (*PluginInitFunc)(PluginAPI *api, PluginInfo *info);
typedef void (*PluginFiniFunc)(PluginAPI *api);

void lll_plugins_init(void);
void lll_plugins_cleanup(void);
int lll_plugins_count(void);
PluginAPI *lll_plugins_get_api(int index);
PluginInfo *lll_plugins_get_info(int index);
void lll_plugins_emit_preamble(void *ctx, FILE *out);
void lll_plugins_emit_epilogue(void *ctx, FILE *out);
int lll_plugins_handle_expr(void *ctx, void *node, FILE *out);
int lll_plugins_handle_stmt(void *ctx, void *node, FILE *out);

#endif
