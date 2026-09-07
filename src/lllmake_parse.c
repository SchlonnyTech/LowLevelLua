#include "lllmake_parse.h"
#include "codegen.h"
#include "lexer.h"
#include "parser.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#define MAX_LINE_LENGTH 4096

static char *trim_whitespace(char *str) {
  while (*str == ' ' || *str == '\t' || *str == '\n' || *str == '\r')
    str++;
  char *end = str + strlen(str) - 1;
  while (end > str &&
         (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r'))
    end--;
  *(end + 1) = '\0';
  return str;
}

static char *get_dirname(const char *p) {
  char *copy = strdup(p);
  char *slash = strrchr(copy, '/');
  if (slash) {
    *slash = '\0';
    return copy;
  }
  free(copy);
  return strdup(".");
}

static ASTNode *do_parse(const char *src, int *tok_count) {
  parser_set_source(src);
  Lexer *l = lexer_create(src);
  int n;
  Token *t = lexer_tokenize(l, &n);
  lexer_destroy(l);
  Parser *p = parser_create(t, n);
  ASTNode *ast = parser_parse_program(p);
  for (int i = 0; i < n; i++)
    free(t[i].text);
  free(t);
  parser_destroy(p);
  *tok_count = n;
  return ast;
}

int lllmake_build_from_file(const char *filename, BuildType build_type,
                            bool verbose) {
  FILE *fp = fopen(filename, "r");
  if (!fp) {
    fprintf(stderr, "Error: Cannot open %s\n", filename);
    return 1;
  }

  char line[MAX_LINE_LENGTH];
  char current_section[64] = "";
  char *sources[256] = {0};
  int source_count = 0;
  char *output_name = NULL;
  char *output_dir = NULL;

  char buildfile_dir[1024];
  getcwd(buildfile_dir, sizeof(buildfile_dir));

  char *dir = get_dirname(filename);
  chdir(dir);
  free(dir);

  getcwd(buildfile_dir, sizeof(buildfile_dir));

  while (fgets(line, sizeof(line), fp)) {
    char *trimmed = trim_whitespace(line);

    if (trimmed[0] == '\0' || trimmed[0] == '#')
      continue;

    if (trimmed[0] == '[') {
      char *end = strchr(trimmed, ']');
      if (end) {
        *end = '\0';
        strncpy(current_section, trimmed + 1, sizeof(current_section) - 1);
      }
      continue;
    }

    char *equals = strchr(trimmed, '=');
    if (!equals)
      continue;

    *equals = '\0';
    char *key = trim_whitespace(trimmed);
    char *value = trim_whitespace(equals + 1);

    if (strcmp(current_section, "project") == 0) {
      if (strcmp(key, "output") == 0 || strcmp(key, "output_name") == 0)
        output_name = strdup(value);
      else if (strcmp(key, "output_dir") == 0)
        output_dir = strdup(value);
    } else if (strcmp(current_section, "sources") == 0) {
      if (strcmp(key, "files") == 0) {
        char *token = strtok(value, " \t,");
        while (token && source_count < 256) {
          sources[source_count++] = strdup(token);
          token = strtok(NULL, " \t,");
        }
      }
    }
  }

  fclose(fp);

  if (!output_name)
    output_name = strdup("a");
  if (!output_dir)
    output_dir = strdup("build");

  if (source_count == 0) {
    fprintf(stderr, "Error: No sources specified\n");
    free(output_name);
    free(output_dir);
    return 1;
  }

  printf("Building %s (%s mode)\n", output_name,
         build_type == BUILD_RELEASE ? "release" : "debug");

  int failed = 0;

  for (int i = 0; i < source_count; i++) {
    printf("\r\033[K[%d/%d] Building %s", i + 1, source_count, sources[i]);
    fflush(stdout);

    char *src_content = read_file(sources[i]);
    if (!src_content) {
      fprintf(stderr, "\nError: Cannot read %s\n", sources[i]);
      failed++;
      continue;
    }

    char *src_dir = get_dirname(sources[i]);
    chdir(src_dir);
    free(src_dir);

    int tc;
    ASTNode *ast = do_parse(src_content, &tc);
    free(src_content);

    if (!ast) {
      fprintf(stderr, "\nError: Failed to parse %s\n", sources[i]);
      chdir(buildfile_dir);
      failed++;
      continue;
    }

    CodeGenContext ctx;
    codegen_init(&ctx, sources[i], build_type);
    ctx.verbose = verbose;

    bool success = codegen_generate(&ctx, ast);
    if (!success) {
      fprintf(stderr, "\nError: Codegen failed: %s\n", ctx.error_msg);
      codegen_destroy(&ctx);
      chdir(buildfile_dir);
      failed++;
      continue;
    }

    char *obj_name = string_format("%s.o", sources[i]);
    if (!codegen_compile_to_object(&ctx, obj_name)) {
      fprintf(stderr, "\nError: Failed to compile %s\n", sources[i]);
      codegen_destroy(&ctx);
      free(obj_name);
      chdir(buildfile_dir);
      failed++;
      continue;
    }

    codegen_destroy(&ctx);
    free(obj_name);
    chdir(buildfile_dir);
  }

  if (failed > 0) {
    printf("\n%d file(s) failed\n", failed);
    for (int i = 0; i < source_count; i++)
      free(sources[i]);
    free(output_name);
    free(output_dir);
    return 1;
  }

  chdir(buildfile_dir);

  char *mkdir_cmd = malloc(1024);
  snprintf(mkdir_cmd, 1024, "mkdir -p %s", output_dir);
  system(mkdir_cmd);
  free(mkdir_cmd);

  printf("\nLinking %s/%s\n", output_dir, output_name);

  char *link_cmd = malloc(8192);
  int offset =
      snprintf(link_cmd, 8192, "gcc -no-pie -o %s/%s", output_dir, output_name);

  for (int i = 0; i < source_count; i++) {
    offset += snprintf(link_cmd + offset, 8192 - offset, " %s.o", sources[i]);
  }

  if (verbose)
    printf("Link: %s\n", link_cmd);

  int ret = system(link_cmd);

  for (int i = 0; i < source_count; i++) {
    char *obj_file = string_format("%s.o", sources[i]);
    unlink(obj_file);
    free(obj_file);
    free(sources[i]);
  }
  free(link_cmd);
  free(output_name);
  free(output_dir);

  if (ret == 0) {
    printf("Build successful!\n");
  } else {
    printf("Build failed!\n");
  }

  return ret;
}
