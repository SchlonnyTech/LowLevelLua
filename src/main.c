#include "codegen.h"
#include "lexer.h"
#include "lll.h"
#include "lllmake_parse.h"
#include "parser.h"
#include "utils.h"
#include <llvm-c/ExecutionEngine.h>
#include <llvm-c/Target.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#define DEBUG_MAIN 0
#define DPRINTF_MAIN(fmt, ...)                                                 \
  if (DEBUG_MAIN)                                                              \
  fprintf(stderr, "[MAIN] " fmt, ##__VA_ARGS__)

#ifndef BUILD_NUMBER
#define BUILD_NUMBER 0
#endif

typedef struct {
  char *input_file;
  char *output_file;
  char *direct_input;
  char *module_name;
  bool emit_llvm;
  bool emit_object;
  bool verbose;
  bool arch_x86;
  bool is_module;
  bool jit_mode;
  bool interactive;
  bool build_mode;
  BuildType build_type;
} Options;

static void banner(void) {
  fprintf(stdout,
          "##############################################\n"
          "  LLL - Low Level Language Compiler v%d.%d.%d (build %s)\n"
          "  Direct LLVM IR Compilation\n"
          "  Author: schlonny\n"
          "  Performance: Native-speed execution\n"
          "##############################################\n\n",
          LLL_VERSION_MAJOR, LLL_VERSION_MINOR, LLL_VERSION_PATCH,
          BUILD_NUMBER);
}

static void version(void) {
  printf("LLL Compiler version %d.%d.%d (build %s)\n", LLL_VERSION_MAJOR,
         LLL_VERSION_MINOR, LLL_VERSION_PATCH, BUILD_NUMBER);
}

static Options parse_args(int argc, char **argv) {
  Options o = {0};
  o.build_type = BUILD_DEBUG;

  if (argc == 1) {
    o.interactive = true;
    return o;
  }

  for (int i = 1; i < argc; i++) {
    char *a = argv[i];
    if (a[0] == '-' && a[1] == 'o' && !a[2] && i + 1 < argc)
      o.output_file = argv[++i];
    else if (a[0] == '-' && a[1] == 'e' && !a[2] && i + 1 < argc) {
      o.direct_input = argv[++i];
      o.jit_mode = true;
    } else if (a[0] == '-' && a[1] == 'c' && !a[2] && i + 1 < argc)
      o.direct_input = argv[++i];
    else if (a[0] == '-' && a[1] == 'S' && !a[2])
      o.emit_llvm = true;
    else if (a[0] == '-' && a[1] == 'v' && !a[2])
      o.verbose = true;
    else if (strcmp(a, "--jit") == 0)
      o.jit_mode = true;
    else if (strcmp(a, "--module") == 0)
      o.is_module = true;
    else if (strcmp(a, "--build") == 0 || strcmp(a, "build") == 0)
      o.build_mode = true;
    else if (strcmp(a, "--x86") == 0)
      o.arch_x86 = true;
    else if (strcmp(a, "--release") == 0 || strcmp(a, "-release") == 0)
      o.build_type = BUILD_RELEASE;
    else if (strcmp(a, "--debug") == 0 || strcmp(a, "-debug") == 0)
      o.build_type = BUILD_DEBUG;
    else if (strcmp(a, "--about") == 0) {
      banner();
      exit(0);
    } else if (strcmp(a, "--version") == 0 || strcmp(a, "-version") == 0) {
      version();
      exit(0);
    } else if (strcmp(a, "--help") == 0) {
      banner();
      printf(
          "Usage: lllc [file] | -e code | -c code | --build | (interactive)\n");
      printf("Options:\n");
      printf("  -o <file>    Output file\n");
      printf("  -S           Emit LLVM IR only\n");
      printf("  -v           Verbose mode (show LLVM IR)\n");
      printf("  --jit        JIT mode\n");
      printf("  --module     Build as module\n");
      printf("  --build      Build using .lllmake file\n");
      printf("  --x86        32-bit mode\n");
      printf("  --release    Release build (optimized)\n");
      printf("  --debug      Debug build (default)\n");
      printf("  --version    Show version information\n");
      printf("  --about      Show banner\n");
      exit(0);
    } else if (strcmp(a, "--modname") == 0 && i + 1 < argc)
      o.module_name = argv[++i];
    else if (!o.input_file && !o.direct_input)
      o.input_file = a;
  }
  return o;
}

static char *basename_no_ext(const char *p) {
  const char *b = strrchr(p, '/');
  b = b ? b + 1 : p;
  const char *d = strrchr(b, '.');
  if (d) {
    int n = d - b;
    char *r = malloc(n + 1);
    memcpy(r, b, n);
    r[n] = 0;
    return r;
  }
  return strdup(b);
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

static double ms(void) {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
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

static int run_jit(const char *src, Options *opts) {
  int tc;
  ASTNode *ast = do_parse(src, &tc);
  if (!ast)
    return 1;

  CodeGenContext ctx;
  codegen_init(&ctx, "lll_jit", opts->build_type);
  ctx.verbose = opts->verbose;

  bool success = codegen_generate(&ctx, ast);
  if (!success) {
    fprintf(stderr, "Codegen failed: %s\n", ctx.error_msg);
    return 1;
  }

  LLVMExecutionEngineRef engine = NULL;
  char *error = NULL;

  if (LLVMCreateExecutionEngineForModule(&engine, ctx.module, &error) != 0) {
    fprintf(stderr, "JIT compilation failed: %s\n", error);
    LLVMDisposeMessage(error);
    return 1;
  }

  LLVMValueRef main_func = LLVMGetNamedFunction(ctx.module, "main");
  if (!main_func) {
    fprintf(stderr, "No main function found\n");
    return 1;
  }

  LLVMGenericValueRef result = LLVMRunFunction(engine, main_func, 0, NULL);
  int ret = result ? (int)LLVMGenericValueToInt(result, 0) : 0;

  return ret;
}

static void print_stats(const char *status, const char *out, int tokens,
                        double total, double parse, double cg,
                        BuildType build_type) {
  printf("\n##############################################\n");
  printf("  Status: %s\n", status);
  if (out)
    printf("  Output: %s\n", out);
  printf("  Build:  %s\n", build_type == BUILD_RELEASE ? "Release" : "Debug");
  printf("  Tokens: %d\n", tokens);
  printf("  Total:   %.2f ms\n", total);
  printf("  Parse:   %.2f ms\n", parse);
  printf("  Codegen: %.2f ms\n", cg);
  printf("##############################################\n");
}

int main(int argc, char **argv) {
  srand(time(NULL));

  Options opts = parse_args(argc, argv);
  if (opts.verbose)
    banner();

  if (opts.build_mode) {
    return lllmake_build_from_file(".lllmake", opts.build_type, opts.verbose);
  }

  if (opts.interactive) {
    printf("LLL Interactive Mode (exit to quit)\n\n");
    char line[4096];
    while (printf("lll> "), fflush(stdout), fgets(line, sizeof(line), stdin)) {
      size_t n = strlen(line);
      if (n && line[n - 1] == '\n')
        line[n - 1] = 0;
      if (!line[0])
        continue;
      if (!strcmp(line, "exit") || !strcmp(line, "quit"))
        break;
      if (!strcmp(line, "about")) {
        banner();
        continue;
      }
      if (!strcmp(line, "version")) {
        version();
        continue;
      }
      run_jit(line, &opts);
    }
    printf("Goodbye!\n");
    return 0;
  }

  double t0 = ms();
  char *src = opts.direct_input ? strdup(opts.direct_input)
                                : read_file(opts.input_file);
  if (!src) {
    fprintf(stderr, "Error reading input\n");
    return 1;
  }

  if (opts.jit_mode) {
    int ret = run_jit(src, &opts);
    free(src);
    return ret;
  }

  char *src_dir =
      opts.direct_input ? strdup(".") : get_dirname(opts.input_file);
  chdir(src_dir);
  free(src_dir);

  double t_parse_start = ms();
  int tc;
  ASTNode *ast = do_parse(src, &tc);
  double t_parse_end = ms();
  free(src);

  if (!ast)
    return 1;

  char *output_base = opts.output_file    ? strdup(opts.output_file)
                      : opts.direct_input ? strdup("lll_out")
                                          : basename_no_ext(opts.input_file);

  double t_cg_start = ms();
  CodeGenContext ctx;
  codegen_init(&ctx, output_base, opts.build_type);
  ctx.verbose = opts.verbose;
  ctx.is_module = opts.is_module;

  bool success = codegen_generate(&ctx, ast);
  double t_cg_end = ms();

  if (!success) {
    fprintf(stderr, "Codegen failed: %s\n", ctx.error_msg);
    codegen_destroy(&ctx);
    ast_destroy_pools();
    free(output_base);
    return 1;
  }

  char *llvm_file = string_format("%s.ll", output_base);
  char *obj_file = string_format("%s.o", output_base);

  int ret = 0;

  if (opts.emit_llvm) {
    LLVMPrintModuleToFile(ctx.module, llvm_file, NULL);
    print_stats("LLVM IR generated", llvm_file, tc, ms() - t0,
                t_parse_end - t_parse_start, t_cg_end - t_cg_start,
                opts.build_type);
  } else {
    if (codegen_compile_to_object(&ctx, obj_file)) {
      char *link_cmd =
          string_format("gcc -no-pie %s -o %s", obj_file, output_base);
      ret = system(link_cmd);
      free(link_cmd);

      if (ret == 0) {
        print_stats("OK", output_base, tc, ms() - t0,
                    t_parse_end - t_parse_start, t_cg_end - t_cg_start,
                    opts.build_type);
      } else {
        print_stats("Linking failed", NULL, tc, ms() - t0,
                    t_parse_end - t_parse_start, t_cg_end - t_cg_start,
                    opts.build_type);
      }
    }
  }

  free(output_base);
  free(llvm_file);
  free(obj_file);

  return ret;
}
