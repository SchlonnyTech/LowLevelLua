#include "codegen.h"
#include "lexer.h"
#include "lll.h"
#include "parser.h"
#include "platform.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#define DEBUG_MAIN 0
#define DEBUG_ARGS 0
#define DEBUG_PARSE 0
#define DEBUG_COMPILE 0
#define DEBUG_JIT 0
#define DEBUG_CODEGEN 0
#define DEBUG_FILE 0

#define DPRINTF_MAIN(fmt, ...)                                                 \
  if (DEBUG_MAIN)                                                              \
  fprintf(stderr, "[MAIN] " fmt, ##__VA_ARGS__)
#define DPRINTF_ARGS(fmt, ...)                                                 \
  if (DEBUG_ARGS)                                                              \
  fprintf(stderr, "[ARGS] " fmt, ##__VA_ARGS__)
#define DPRINTF_PARSE(fmt, ...)                                                \
  if (DEBUG_PARSE)                                                             \
  fprintf(stderr, "[PARSE] " fmt, ##__VA_ARGS__)
#define DPRINTF_COMPILE(fmt, ...)                                              \
  if (DEBUG_COMPILE)                                                           \
  fprintf(stderr, "[COMPILE] " fmt, ##__VA_ARGS__)
#define DPRINTF_JIT(fmt, ...)                                                  \
  if (DEBUG_JIT)                                                               \
  fprintf(stderr, "[JIT] " fmt, ##__VA_ARGS__)
#define DPRINTF_CODEGEN(fmt, ...)                                              \
  if (DEBUG_CODEGEN)                                                           \
  fprintf(stderr, "[CODEGEN] " fmt, ##__VA_ARGS__)
#define DPRINTF_FILE(fmt, ...)                                                 \
  if (DEBUG_FILE)                                                              \
  fprintf(stderr, "[FILE] " fmt, ##__VA_ARGS__)

static unsigned int generate_build_id(const char *filename) {
  struct timeval tv;
  gettimeofday(&tv, NULL);

  struct stat st;
  unsigned int file_size_factor = 0;
  if (stat(filename, &st) == 0) {
    file_size_factor = (unsigned int)(st.st_size % 1000000);
  }

  time_t now = tv.tv_sec;
  struct tm *tm_info = localtime(&now);

  unsigned int time_factor =
      (tm_info->tm_year + 1900) * 1000000 + (tm_info->tm_mon + 1) * 10000 +
      tm_info->tm_mday * 100 + tm_info->tm_hour * 4 + tm_info->tm_min;

  unsigned int nanosec = tv.tv_usec * 1000;
  unsigned int random_factor = rand() ^ (rand() << 16);

  unsigned int build_id =
      time_factor ^ file_size_factor ^ nanosec ^ random_factor;

  if (build_id == 0)
    build_id = 1;

  return build_id;
}

#ifndef BUILD_NUMBER
#define BUILD_NUMBER generate_build_id(__FILE__)
#endif

typedef struct {
  char *input_file, *output_file, *direct_input, *module_name;
  bool asm_only, verbose, arch_x86, is_module, jit_mode, interactive,
      keep_cfile;
} Options;

static void banner(void) {
  fprintf(stdout,
          "##############################################\n"
          "  LLL - Low Level Lua Compiler v%d.%d.%d (build %d)\n"
          "  Self-contained: No external dependencies beyond the compiler\n"
          "  Author: schlonny\n"
          "  Performance: Native-speed execution\n"
          "##############################################\n\n",
          LLL_VERSION_MAJOR, LLL_VERSION_MINOR, LLL_VERSION_PATCH,
          BUILD_NUMBER);
}

static Options parse_args(int argc, char **argv) {
  Options o = {0};
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
      o.asm_only = true;
    else if (a[0] == '-' && a[1] == 'v' && !a[2])
      o.verbose = true;
    else if (strcmp(a, "--jit") == 0)
      o.jit_mode = true;
    else if (strcmp(a, "--module") == 0)
      o.is_module = true;
    else if (strcmp(a, "--x86") == 0)
      o.arch_x86 = true;
    else if (strcmp(a, "--cfile") == 0)
      o.keep_cfile = true;
    else if (strcmp(a, "--about") == 0) {
      banner();
      exit(0);
    } else if (strcmp(a, "--help") == 0) {
      banner();
      printf("Usage: lllc [file] | -e code | -c code | (interactive)\n");
      printf("Options:\n");
      printf("  -o <file>    Output file\n");
      printf("  -S           Generate C only\n");
      printf("  --cfile      Keep generated C file\n");
      printf("  --jit        JIT mode\n");
      printf("  --module     Build as module\n");
      printf("  --x86        32-bit mode\n");
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

static double ms(void) {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
}

static ASTNode *do_parse(const char *src, int *tok_count) {
  DPRINTF_PARSE("do_parse START\n");
  parser_set_source(src);

  DPRINTF_PARSE("Creating lexer\n");
  Lexer *l = lexer_create(src);
  int n;
  Token *t = lexer_tokenize(l, &n);
  lexer_destroy(l);

  DPRINTF_PARSE("Lexer produced %d tokens\n", n);
  DPRINTF_PARSE("First 20 tokens:\n");
  for (int i = 0; i < n && i < 20; i++) {
    DPRINTF_PARSE("  Token %d: type=%d, text='%s', line=%d, col=%d\n", i,
                  t[i].type, t[i].text ? t[i].text : "(null)", t[i].line,
                  t[i].column);
  }

  DPRINTF_PARSE("Creating parser\n");
  Parser *p = parser_create(t, n);

  DPRINTF_PARSE("Calling parser_parse_program\n");
  ASTNode *ast = parser_parse_program(p);

  if (!ast) {
    DPRINTF_PARSE("PARSER RETURNED NULL\n");
  } else {
    DPRINTF_PARSE("PARSER SUCCEEDED\n");
  }

  DPRINTF_PARSE("Cleaning up tokens\n");
  for (int i = 0; i < n; i++)
    free(t[i].text);
  free(t);
  parser_destroy(p);
  *tok_count = n;

  DPRINTF_PARSE("do_parse COMPLETE\n");
  return ast;
}

static char *get_needed_libs(const char *cfile) {
  DPRINTF_COMPILE("get_needed_libs checking %s\n", cfile);
  FILE *f = fopen(cfile, "r");
  if (!f) {
    DPRINTF_COMPILE("Cannot open file, returning empty\n");
    return strdup("");
  }
  char line[512];
  char *result = strdup("");
  while (fgets(line, sizeof(line), f)) {
    if (strstr(line, "#include")) {
      char *tmp = string_format("%s -lm", result);
      free(result);
      result = tmp;
      DPRINTF_COMPILE("Found #include, libs='%s'\n", result);
      break;
    }
  }
  fclose(f);
  return result;
}

static int compile_c(const char *cf, const char *bf, bool arch_x86,
                     bool is_module) {
  DPRINTF_COMPILE("compile_c START cf=%s bf=%s\n", cf, bf);
  char *libs_str = get_needed_libs(cf);
  const char *cc = getenv("CC") ? getenv("CC") : "gcc";
  char *cmd = is_module
                  ? string_format("%s %s -shared -fPIC -o %s %s %s", cc,
                                  arch_x86 ? "-m32" : "", bf, cf, libs_str)
                  : string_format("%s %s -o %s %s -lm %s", cc,
                                  arch_x86 ? "-m32" : "", bf, cf, libs_str);
  DPRINTF_COMPILE("Command: %s\n", cmd);
  int ret = system(cmd);
  DPRINTF_COMPILE("Returned %d\n", ret);
  free(cmd);
  free(libs_str);
  return ret;
}

#ifdef LLL_JIT
static int run_jit(const char *src, Options *opts) {
  DPRINTF_JIT("run_jit START\n");
  PlatformInfo pl = platform_detect();
  int tc;
  ASTNode *ast = do_parse(src, &tc);
  if (!ast) {
    DPRINTF_JIT("Parse failed\n");
    return 1;
  }
  CodeGenContext ctx;
  codegen_init(&ctx, NULL, pl);
  ctx.arch_x86 = opts->arch_x86;
  int ret = codegen_jit_exec(&ctx, ast);
  DPRINTF_JIT("codegen_jit_exec returned %d\n", ret);
  ast_destroy_pools();
  return ret;
}
#else
static int run_jit_fallback(const char *src, Options *opts) {
  DPRINTF_JIT("run_jit_fallback START\n");
  PlatformInfo pl = platform_detect();
  int tc;
  ASTNode *ast = do_parse(src, &tc);
  if (!ast) {
    DPRINTF_JIT("Parse failed\n");
    return 1;
  }
  CodeGenContext ctx;
  codegen_init(&ctx, NULL, pl);
  ctx.arch_x86 = opts->arch_x86;
  DPRINTF_JIT("Calling codegen_jit_compile\n");
  codegen_jit_compile(&ctx, ast, "lll_jit.c");
  int ret = system("gcc lll_jit.c -o lll_jit -ltcc -lm 2>/dev/null");
  DPRINTF_JIT("gcc returned %d\n", ret);
  if (ret == 0) {
    system("./lll_jit");
    unlink("lll_jit");
    unlink("lll_jit.c");
    unlink("lll_jit_src.c");
  }
  ast_destroy_pools();
  return ret;
}
#define run_jit run_jit_fallback
#endif

static void print_stats(const char *status, const char *out, int tokens,
                        double total, double lex, double parse, double cg,
                        double cc) {
  printf("\n##############################################\n");
  printf("  Status: %s\n", status);
  if (out)
    printf("  Output: %s\n", out);
  printf("  Tokens: %d\n", tokens);
  printf("  Total:   %.2f ms\n", total);
  printf("  Lex:     %.2f ms\n", lex);
  printf("  Parse:   %.2f ms\n", parse);
  printf("  Codegen: %.2f ms\n", cg);
  if (cc > 0)
    printf("  CC:      %.2f ms\n", cc);
  printf("##############################################\n");
}

int main(int argc, char **argv) {
  DPRINTF_MAIN("main START argc=%d\n", argc);
  srand(time(NULL));

  Options opts = parse_args(argc, argv);
  if (opts.verbose) {
    DPRINTF_MAIN("Verbose mode\n");
    banner();
  }

  if (opts.interactive) {
    DPRINTF_MAIN("Interactive mode\n");
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
      run_jit(line, &opts);
    }
    printf("Goodbye!\n");
    return 0;
  }

  double t0 = ms();
  DPRINTF_MAIN("Reading input file\n");
  char *src = opts.direct_input ? strdup(opts.direct_input)
                                : read_file(opts.input_file);
  if (!src) {
    DPRINTF_MAIN("Failed to read input\n");
    fprintf(stderr, "Error reading input\n");
    return 1;
  }

  if (opts.jit_mode) {
    DPRINTF_MAIN("JIT mode\n");
    int ret = run_jit(src, &opts);
    free(src);
    return ret;
  }

  DPRINTF_MAIN("Calling do_parse\n");
  double t_lex_start = ms();
  int tc;
  ASTNode *ast = do_parse(src, &tc);
  double t_parse_end = ms();
  free(src);
  if (!ast) {
    DPRINTF_MAIN("do_parse returned NULL\n");
    return 1;
  }
  DPRINTF_MAIN("do_parse succeeded, tokens=%d\n", tc);

  if (opts.is_module) {
    DPRINTF_MAIN("Setting module info\n");
    ast->is_module = true;
    ast->module_name =
        opts.module_name
            ? strdup(opts.module_name)
            : basename_no_ext(opts.input_file ? opts.input_file : "module");
  }

  DPRINTF_MAIN("Detecting platform\n");
  PlatformInfo pl = platform_detect();
  char *cf = opts.output_file ? string_format("%s.c", opts.output_file)
             : opts.direct_input
                 ? strdup("lll_out.c")
                 : string_format("%s.c", basename_no_ext(opts.input_file));
  char *bf = opts.output_file ? strdup(opts.output_file)
             : opts.is_module
                 ? string_format("%s.so",
                                 ast->module_name ? ast->module_name : "module")
                 : strdup(opts.direct_input ? "lll_program"
                                            : basename_no_ext(opts.input_file));

  DPRINTF_MAIN("Output files: cf=%s bf=%s\n", cf, bf);
  DPRINTF_MAIN("Opening output file\n");
  FILE *f = fopen(cf, "w");
  if (!f) {
    DPRINTF_MAIN("Cannot open output file\n");
    fprintf(stderr, "Cannot write %s\n", cf);
    return 1;
  }
  fprintf(f, "/* LLL v%d.%d.%d */\n", LLL_VERSION_MAJOR, LLL_VERSION_MINOR,
          LLL_VERSION_PATCH);

  DPRINTF_CODEGEN("Initializing codegen\n");
  double t_cg_start = ms();
  CodeGenContext ctx;
  codegen_init(&ctx, f, pl);
  ctx.arch_x86 = opts.arch_x86;
  ctx.is_module = opts.is_module;

  DPRINTF_CODEGEN("Generating program\n");
  codegen_generate_program(&ctx, ast);
  fclose(f);
  double t_cg_end = ms();
  DPRINTF_CODEGEN("Codegen complete\n");

  if (opts.asm_only) {
    DPRINTF_MAIN("asm_only mode\n");
    double total = ms() - t0;
    print_stats("C generated", cf, tc, total, t_parse_end - t_lex_start, 0,
                t_cg_end - t_cg_start, 0);
    free(cf);
    free(bf);
    ast_destroy_pools();
    return 0;
  }

  DPRINTF_MAIN("Compiling C code\n");
  double t_cc_start = ms();
  int ret = compile_c(cf, bf, opts.arch_x86, opts.is_module);
  double t_cc_end = ms();
  double total = ms() - t0;

  if (ret == 0) {
    DPRINTF_MAIN("Compilation successful\n");
    print_stats("OK", bf, tc, total, t_parse_end - t_lex_start, 0,
                t_cg_end - t_cg_start, t_cc_end - t_cc_start);
    if (!opts.keep_cfile)
      unlink(cf);
  } else {
    DPRINTF_MAIN("Compilation failed\n");
    print_stats("FAILED", NULL, tc, total, t_parse_end - t_lex_start, 0,
                t_cg_end - t_cg_start, t_cc_end - t_cc_start);
  }
  free(cf);
  free(bf);
  ast_destroy_pools();
  DPRINTF_MAIN("main COMPLETE ret=%d\n", ret);
  return ret;
}
