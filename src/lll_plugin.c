#include "lll_plugin.h"
#include "utils.h"
#include <dirent.h>
#include <dlfcn.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_PLUGINS 128
#define PLUGIN_DIR "llladdons"
#define PLUGIN_EXT ".lllplugin"

typedef struct {
  void *handle;
  char *name;
  char *path;
  PluginInfo info;
  PluginAPI api;
  void *plugin_data;
} LoadedPlugin;

static LoadedPlugin g_plugins[MAX_PLUGINS];
static int g_plugin_count = 0;

static void *plugin_register_keyword(const char *n, void *h) {
  (void)n;
  (void)h;
  return NULL;
}
static void *plugin_register_type(const char *n, void *h) {
  (void)n;
  (void)h;
  return NULL;
}
static void *plugin_register_function(const char *n, void *h) {
  (void)n;
  (void)h;
  return NULL;
}
static void *plugin_register_variable(const char *n, void *h) {
  (void)n;
  (void)h;
  return NULL;
}
static void *plugin_register_operator(const char *n, void *h) {
  (void)n;
  (void)h;
  return NULL;
}
static void *plugin_register_statement(const char *n, void *h) {
  (void)n;
  (void)h;
  return NULL;
}
static void *plugin_register_expression(const char *n, void *h) {
  (void)n;
  (void)h;
  return NULL;
}
static void *plugin_register_directive(const char *n, void *h) {
  (void)n;
  (void)h;
  return NULL;
}

static int plugin_emit_code(void *c, void *n, FILE *o) {
  (void)c;
  (void)n;
  (void)o;
  return 0;
}
static void *plugin_get_context(void) { return NULL; }

static void plugin_error(int line, int col, const char *fmt, ...) {
  va_list a;
  va_start(a, fmt);
  fprintf(stderr, "Plugin Error at %d:%d: ", line, col);
  vfprintf(stderr, fmt, a);
  fprintf(stderr, "\n");
  va_end(a);
}

static void plugin_warning(int line, int col, const char *fmt, ...) {
  va_list a;
  va_start(a, fmt);
  fprintf(stderr, "Plugin Warning at %d:%d: ", line, col);
  vfprintf(stderr, fmt, a);
  fprintf(stderr, "\n");
  va_end(a);
}

static void plugin_debug(const char *fmt, ...) {
  va_list a;
  va_start(a, fmt);
  fprintf(stderr, "Plugin Debug: ");
  vfprintf(stderr, fmt, a);
  fprintf(stderr, "\n");
  va_end(a);
}

static char *plugin_string_format(const char *fmt, ...) {
  va_list a;
  va_start(a, fmt);
  char *r = string_format(fmt, a);
  va_end(a);
  return r;
}

static bool plugin_file_exists(const char *p) { return file_exists(p); }
static char *plugin_read_file(const char *p) { return read_file(p); }

static bool plugin_write_file(const char *p, const char *c) {
  FILE *f = fopen(p, "w");
  if (!f)
    return false;
  fputs(c, f);
  fclose(f);
  return true;
}

static void load_plugins_from_dir(const char *dir) {
  DIR *d = opendir(dir);
  if (!d)
    return;
  struct dirent *e;
  while ((e = readdir(d)) && g_plugin_count < MAX_PLUGINS) {
    if (e->d_name[0] == '.')
      continue;
    size_t len = strlen(e->d_name), ext = strlen(PLUGIN_EXT);
    if (len < ext || strcmp(e->d_name + len - ext, PLUGIN_EXT))
      continue;

    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", dir, e->d_name);
    void *h = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!h) {
      fprintf(stderr, "Warning: Cannot load %s: %s\n", path, dlerror());
      continue;
    }

    PluginInitFunc init = (PluginInitFunc)dlsym(h, "lll_plugin_init");
    if (!init) {
      fprintf(stderr, "Warning: %s missing init\n", path);
      dlclose(h);
      continue;
    }

    LoadedPlugin *p = &g_plugins[g_plugin_count];
    memset(p, 0, sizeof(*p));
    p->handle = h;
    p->name = strdup(e->d_name);
    p->path = strdup(path);
    p->api.version = LLL_PLUGIN_API_VERSION;
    p->api.register_keyword = plugin_register_keyword;
    p->api.register_type = plugin_register_type;
    p->api.register_function = plugin_register_function;
    p->api.register_variable = plugin_register_variable;
    p->api.register_operator = plugin_register_operator;
    p->api.register_statement = plugin_register_statement;
    p->api.register_expression = plugin_register_expression;
    p->api.register_directive = plugin_register_directive;
    p->api.emit_code = plugin_emit_code;
    p->api.get_context = plugin_get_context;
    p->api.error = plugin_error;
    p->api.warning = plugin_warning;
    p->api.debug = plugin_debug;
    p->api.malloc = malloc;
    p->api.calloc = calloc;
    p->api.realloc = realloc;
    p->api.free = free;
    p->api.strdup = strdup;
    p->api.string_format = plugin_string_format;
    p->api.string_copy = string_copy;
    p->api.file_exists = plugin_file_exists;
    p->api.read_file = plugin_read_file;
    p->api.write_file = plugin_write_file;

    if (init(&p->api, &p->info)) {
      fprintf(stderr, "Warning: %s init failed\n", path);
      free(p->name);
      free(p->path);
      dlclose(h);
      continue;
    }
    g_plugin_count++;
    fprintf(stderr, "Loaded: %s v%s by %s\n", p->info.name ?: "?",
            p->info.version ?: "?", p->info.author ?: "?");
  }
  closedir(d);
}

void lll_plugins_init(void) {
  static bool loaded = false;
  if (loaded)
    return;
  loaded = true;
  load_plugins_from_dir(PLUGIN_DIR);
  load_plugins_from_dir(".");
  load_plugins_from_dir("/usr/lib/llladdons");
  load_plugins_from_dir("/usr/local/lib/llladdons");
  char exe[1024];
  ssize_t len = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
  if (len > 0) {
    exe[len] = 0;
    char *s = strrchr(exe, '/');
    if (s) {
      *s = 0;
      char p[1024];
      snprintf(p, sizeof(p), "%s/%s", exe, PLUGIN_DIR);
      load_plugins_from_dir(p);
    }
  }
}

void lll_plugins_cleanup(void) {
  for (int i = 0; i < g_plugin_count; i++) {
    if (g_plugins[i].handle) {
      PluginFiniFunc f =
          (PluginFiniFunc)dlsym(g_plugins[i].handle, "lll_plugin_fini");
      if (f)
        f(&g_plugins[i].api);
      dlclose(g_plugins[i].handle);
    }
    free(g_plugins[i].name);
    free(g_plugins[i].path);
  }
  g_plugin_count = 0;
}

void lll_plugins_emit_preamble(void *ctx, FILE *out) {
  for (int i = 0; i < g_plugin_count; i++)
    if (g_plugins[i].api.emit_preamble)
      g_plugins[i].api.emit_preamble(ctx, out);
}

void lll_plugins_emit_epilogue(void *ctx, FILE *out) {
  for (int i = 0; i < g_plugin_count; i++)
    if (g_plugins[i].api.emit_epilogue)
      g_plugins[i].api.emit_epilogue(ctx, out);
}

int lll_plugins_handle_expr(void *ctx, void *node, FILE *out) {
  for (int i = 0; i < g_plugin_count; i++)
    if (g_plugins[i].api.handle_expr &&
        g_plugins[i].api.handle_expr(ctx, node, out) == 1)
      return 1;
  return 0;
}

int lll_plugins_handle_stmt(void *ctx, void *node, FILE *out) {
  for (int i = 0; i < g_plugin_count; i++)
    if (g_plugins[i].api.handle_stmt &&
        g_plugins[i].api.handle_stmt(ctx, node, out) == 1)
      return 1;
  return 0;
}
