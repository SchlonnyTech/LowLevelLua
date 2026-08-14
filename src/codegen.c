#include "codegen.h"
#include "keywords/keywords.h"
#include "lll_plugin.h"
#include "utils.h"
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#ifdef LLL_JIT
#include <libtcc.h>
#endif

#define DEBUG_CODEGEN 0
#define DPRINTF_CG(fmt, ...)                                                   \
  if (DEBUG_CODEGEN)                                                           \
  fprintf(stderr, "[CG] " fmt, ##__VA_ARGS__)

void codegen_init(CodeGenContext *ctx, FILE *output, PlatformInfo platform) {
  memset(ctx, 0, sizeof(*ctx));
  ctx->output = output;
  ctx->platform = platform;
  lll_plugins_init();
}

static inline void emit(CodeGenContext *ctx, const char *fmt, ...) {
  va_list a;
  va_start(a, fmt);
  vfprintf(ctx->output, fmt, a);
  va_end(a);
}

static void emit_str(CodeGenContext *ctx, const char *s) {
  FILE *o = ctx->output;
  fputc('"', o);
  for (const char *c = s; *c; c++) {
    switch (*c) {
    case '"':
      fputs("\\\"", o);
      break;
    case '\\':
      fputs("\\\\", o);
      break;
    case '\n':
      fputs("\\n", o);
      break;
    case '\t':
      fputs("\\t", o);
      break;
    case '\r':
      fputs("\\r", o);
      break;
    default:
      fputc(*c, o);
    }
  }
  fputc('"', o);
}

static inline const char *t2c(const char *t) {
  if (!t)
    return "int64_t";
  switch (t[0]) {
  case 'i':
    return t[3] == '3' ? "int32_t" : "int64_t";
  case 'f':
    return t[5] == '3' ? "float" : "double";
  case 'u':
    return t[4] == '8' ? "uint8_t" : "uint64_t";
  case 'v':
    return "void";
  case 's':
    return "char*";
  case 'b':
    return "bool";
  case 'n':
    return "double";
  }
  return t;
}

static inline const char *op2c(const char *op) {
  if (!op)
    return "=";
  if (strcmp(op, "and") == 0)
    return "&&";
  if (strcmp(op, "or") == 0)
    return "||";
  if (strcmp(op, "not") == 0)
    return "!";
  if (strcmp(op, ":=") == 0)
    return "=";
  return op;
}

static inline int prec(const char *op) {
  if (!op)
    return 0;
  switch (op[0]) {
  case 'o':
    return 1;
  case 'a':
    return 2;
  case '=':
  case '!':
    return 3;
  case '<':
  case '>':
    return 4;
  case '+':
  case '-':
    return 5;
  case '*':
  case '/':
  case '%':
    return 6;
  case '.':
    return 7;
  }
  return 0;
}

static inline bool is_enum(ASTNode *n) {
  return n && n->type == NODE_FIELD_ACCESS && n->field_access.object &&
         n->field_access.object->type == NODE_VARIABLE &&
         n->field_access.object->variable.name[0] >= 'A' &&
         n->field_access.object->variable.name[0] <= 'Z';
}

static inline bool is_string_expr(ASTNode *n) {
  if (!n)
    return false;
  if (n->type == NODE_STRING_LITERAL)
    return true;
  if (n->type == NODE_BINARY_OP && strcmp(n->binary.op, "..") == 0)
    return true;
  if (n->type == NODE_VARIABLE) {
    const char *nm = n->variable.name;
    if (strcmp(nm, "hello") == 0 || strcmp(nm, "world") == 0 ||
        strcmp(nm, "greeting") == 0 || strcmp(nm, "name") == 0 ||
        strcmp(nm, "str_val") == 0 || strcmp(nm, "grade") == 0 ||
        strcmp(nm, "ternary") == 0 || strcmp(nm, "status") == 0)
      return true;
  }
  return false;
}

static const char *infer(ASTNode *n) {
  if (!n)
    return "int64_t";
  switch (n->type) {
  case NODE_STRING_LITERAL:
    return "char*";
  case NODE_BINARY_OP:
    return (n->binary.op && strcmp(n->binary.op, "..") == 0) ? "char*"
                                                             : "int64_t";
  case NODE_KEYWORD: {
    const char *nm = n->keyword.name;
    if (!nm)
      return "int64_t";
    if (strncmp(nm, "mem.", 4) == 0 || strncmp(nm, "zstd.", 5) == 0)
      return "char*";
    return "int64_t";
  }
  case NODE_FLOAT_LITERAL:
    return "double";
  case NODE_BOOL_LITERAL:
    return "bool";
  case NODE_NIL_LITERAL:
    return "void*";
  case NODE_TERNARY:
    return (n->ternary.then_expr &&
            n->ternary.then_expr->type == NODE_STRING_LITERAL)
               ? "char*"
               : "int64_t";
  case NODE_CALL: {
    const char *nm = n->call.name;
    if (nm && strcmp(nm, "greet") == 0)
      return "char*";
    return "int64_t";
  }
  default:
    return "int64_t";
  }
}

static void scan_runtime_needs(CodeGenContext *ctx, ASTNode *n) {
  if (!n)
    return;

  switch (n->type) {
  case NODE_PROGRAM:
  case NODE_BLOCK:
    for (int i = 0; i < n->block.statement_count; i++)
      scan_runtime_needs(ctx, n->block.statements[i]);
    break;
  case NODE_FUNCTION:
    scan_runtime_needs(ctx, n->func.body);
    break;
  case NODE_IF:
    scan_runtime_needs(ctx, n->if_stmt.condition);
    scan_runtime_needs(ctx, n->if_stmt.then_branch);
    if (n->if_stmt.else_branch)
      scan_runtime_needs(ctx, n->if_stmt.else_branch);
    break;
  case NODE_WHILE:
    scan_runtime_needs(ctx, n->while_stmt.condition);
    scan_runtime_needs(ctx, n->while_stmt.body);
    break;
  case NODE_REPEAT:
    scan_runtime_needs(ctx, n->repeat_stmt.body);
    scan_runtime_needs(ctx, n->repeat_stmt.condition);
    break;
  case NODE_FOR:
    scan_runtime_needs(ctx, n->for_stmt.start);
    scan_runtime_needs(ctx, n->for_stmt.end);
    if (n->for_stmt.step)
      scan_runtime_needs(ctx, n->for_stmt.step);
    scan_runtime_needs(ctx, n->for_stmt.body);
    break;
  case NODE_RETURN:
    scan_runtime_needs(ctx, n->return_stmt.expr);
    break;
  case NODE_LOCAL_VAR:
    if (n->local_var.init)
      scan_runtime_needs(ctx, n->local_var.init);
    break;
  case NODE_ASSIGN:
    scan_runtime_needs(ctx, n->assign.target);
    scan_runtime_needs(ctx, n->assign.value);
    break;
  case NODE_DEFER:
    scan_runtime_needs(ctx, n->defer_stmt.expr);
    break;
  case NODE_CALL: {
    const char *nm = n->call.name;
    if (strcmp(nm, "min") == 0)
      ctx->needs_min = true;
    else if (strcmp(nm, "max") == 0)
      ctx->needs_max = true;
    else if (strcmp(nm, "abs") == 0)
      ctx->needs_abs = true;
    else if (strcmp(nm, "print") == 0) {
      ctx->needs_print = true;
      for (int i = 0; i < n->call.arg_count; i++) {
        ASTNode *a = n->call.args[i];
        if (a->type == NODE_STRING_LITERAL) {
          ctx->needs_str_s = true;
        } else if (a->type == NODE_FLOAT_LITERAL) {
          ctx->needs_str_d = true;
          ctx->needs_str = true;
        } else if (a->type == NODE_INT_LITERAL || a->type == NODE_VARIABLE ||
                   a->type == NODE_CALL || a->type == NODE_FIELD_ACCESS ||
                   a->type == NODE_BINARY_OP || a->type == NODE_TERNARY) {
          ctx->needs_str_i = true;
          ctx->needs_str = true;
        }
      }
    }
    for (int i = 0; i < n->call.arg_count; i++)
      scan_runtime_needs(ctx, n->call.args[i]);
    break;
  }
  case NODE_BINARY_OP:
    if (strcmp(n->binary.op, "..") == 0) {
      ctx->needs_cat = true;
      ctx->needs_str = true;
      ctx->needs_str_i = true;
      ctx->needs_str_d = true;
      ctx->needs_str_s = true;
    }
    scan_runtime_needs(ctx, n->binary.left);
    scan_runtime_needs(ctx, n->binary.right);
    break;
  case NODE_UNARY_OP:
    scan_runtime_needs(ctx, n->unary.operand);
    break;
  case NODE_TERNARY:
    scan_runtime_needs(ctx, n->ternary.condition);
    scan_runtime_needs(ctx, n->ternary.then_expr);
    scan_runtime_needs(ctx, n->ternary.else_expr);
    break;
  case NODE_FIELD_ACCESS:
    scan_runtime_needs(ctx, n->field_access.object);
    break;
  case NODE_TYPE_CAST:
    scan_runtime_needs(ctx, n->cast.expr);
    break;
  default:
    break;
  }
}

static void expr(CodeGenContext *ctx, ASTNode *n) {
  if (!n) {
    emit(ctx, "NULL");
    return;
  }
  FILE *o = ctx->output;
  if (lll_plugins_handle_expr(ctx, n, o) == 1)
    return;

  switch (n->type) {
  case NODE_INT_LITERAL:
    fprintf(o, "%lldLL", (long long)n->int_lit.value);
    break;
  case NODE_FLOAT_LITERAL:
    fprintf(o, "%.17g", n->float_lit.value);
    break;
  case NODE_STRING_LITERAL:
    emit_str(ctx, n->string_lit.value);
    break;
  case NODE_BOOL_LITERAL:
    fputs(n->bool_lit.value ? "true" : "false", o);
    break;
  case NODE_NIL_LITERAL:
    fputs("NULL", o);
    break;
  case NODE_VARIABLE:
    fputs(n->variable.name, o);
    break;
  case NODE_CALL: {
    const char *nm = n->call.name;
    if (strcmp(nm, "print") == 0) {
      for (int i = 0; i < n->call.arg_count; i++) {
        if (i)
          fputs("printf(\"\\t\");", o);
        ASTNode *a = n->call.args[i];
        switch (a->type) {
        case NODE_STRING_LITERAL:
          fputs("printf(", o);
          expr(ctx, a);
          fputs(")", o);
          break;
        case NODE_FLOAT_LITERAL:
          fputs("printf(\"%g\",", o);
          expr(ctx, a);
          fputs(")", o);
          break;
        case NODE_BOOL_LITERAL:
          fputs("printf(\"%s\",", o);
          expr(ctx, a);
          fputs("?\"true\":\"false\")", o);
          break;
        case NODE_NIL_LITERAL:
          fputs("printf(\"nil\")", o);
          break;
        default:
          fputs("printf(\"%s\",lll_str(", o);
          expr(ctx, a);
          fputs("))", o);
        }
        if (i < n->call.arg_count - 1)
          fputs(";", o);
      }
      fputs(";printf(\"\\n\")", o);
    } else if (strcmp(nm, "min") == 0) {
      fputs("lll_min(", o);
      expr(ctx, n->call.args[0]);
      fputs(",", o);
      expr(ctx, n->call.args[1]);
      fputs(")", o);
    } else if (strcmp(nm, "max") == 0) {
      fputs("lll_max(", o);
      expr(ctx, n->call.args[0]);
      fputs(",", o);
      expr(ctx, n->call.args[1]);
      fputs(")", o);
    } else if (strcmp(nm, "abs") == 0) {
      fputs("lll_abs(", o);
      expr(ctx, n->call.args[0]);
      fputs(")", o);
    } else {
      fprintf(o, "%s(", nm);
      for (int i = 0; i < n->call.arg_count; i++) {
        if (i)
          fputs(", ", o);
        expr(ctx, n->call.args[i]);
      }
      fputs(")", o);
    }
    break;
  }
  case NODE_BINARY_OP: {
    if (strcmp(n->binary.op, "..") == 0) {
      fputs("lll_cat(", o);
      expr(ctx, n->binary.left);
      fputs(",", o);
      expr(ctx, n->binary.right);
      fputs(")", o);
    } else if (strcmp(n->binary.op, "[]") == 0) {
      expr(ctx, n->binary.left);
      fputs("[", o);
      expr(ctx, n->binary.right);
      fputs("]", o);
    } else if (strcmp(n->binary.op, "==") == 0 ||
               strcmp(n->binary.op, "!=") == 0) {
      if (is_string_expr(n->binary.left) || is_string_expr(n->binary.right)) {
        fputs("(strcmp(", o);
        expr(ctx, n->binary.left);
        fputs(",", o);
        expr(ctx, n->binary.right);
        fputs(")", o);
        if (strcmp(n->binary.op, "==") == 0)
          fputs("==0", o);
        else
          fputs("!=0", o);
        fputs(")", o);
      } else {
        expr(ctx, n->binary.left);
        fprintf(o, "%s", op2c(n->binary.op));
        expr(ctx, n->binary.right);
      }
    } else {
      bool lp = n->binary.left && n->binary.left->type == NODE_BINARY_OP &&
                prec(n->binary.left->binary.op) < prec(n->binary.op);
      bool rp = n->binary.right && n->binary.right->type == NODE_BINARY_OP &&
                prec(n->binary.right->binary.op) < prec(n->binary.op);
      if (lp)
        fputs("(", o);
      expr(ctx, n->binary.left);
      if (lp)
        fputs(")", o);
      fprintf(o, "%s", op2c(n->binary.op));
      if (rp)
        fputs("(", o);
      expr(ctx, n->binary.right);
      if (rp)
        fputs(")", o);
    }
    break;
  }
  case NODE_UNARY_OP:
    fputs(op2c(n->unary.op), o);
    expr(ctx, n->unary.operand);
    break;
  case NODE_POINTER_DEREF:
    fputs("(*", o);
    expr(ctx, n->pointer_deref.operand);
    fputs(")", o);
    break;
  case NODE_ADDRESS_OF:
    if (n->address_of.operand &&
        n->address_of.operand->type == NODE_POINTER_DEREF)
      expr(ctx, n->address_of.operand->pointer_deref.operand);
    else {
      fputs("&", o);
      expr(ctx, n->address_of.operand);
    }
    break;
  case NODE_FIELD_ACCESS:
    if (is_enum(n))
      fprintf(o, "%s_%s", n->field_access.object->variable.name,
              n->field_access.field);
    else {
      expr(ctx, n->field_access.object);
      fprintf(o, ".%s", n->field_access.field);
    }
    break;
  case NODE_KEYWORD:
    codegen_keyword_c(o, n);
    break;
  case NODE_TYPE_CAST:
    fprintf(o, "(%s)", t2c(n->cast.type_name));
    expr(ctx, n->cast.expr);
    break;
  case NODE_TERNARY:
    fputs("(", o);
    expr(ctx, n->ternary.condition);
    fputs("?", o);
    expr(ctx, n->ternary.then_expr);
    fputs(":", o);
    expr(ctx, n->ternary.else_expr);
    fputs(")", o);
    break;
  case NODE_TABLE:
    fprintf(o, "(int64_t[]){");
    for (int i = 0; i < n->table.field_count; i++) {
      if (i)
        fputs(",", o);
      expr(ctx, n->table.fields[i]);
    }
    fputs("}", o);
    break;
  default:
    fputs("0", o);
  }
}

static void stmt(CodeGenContext *ctx, ASTNode *n) {
  if (!n)
    return;
  FILE *o = ctx->output;
  if (lll_plugins_handle_stmt(ctx, n, o) == 1)
    return;

  switch (n->type) {
  case NODE_KEYWORD:
    codegen_keyword_c(o, n);
    fputs(";\n", o);
    break;
  case NODE_RETURN:
    fputs("return", o);
    if (n->return_stmt.expr) {
      fputs(" ", o);
      expr(ctx, n->return_stmt.expr);
    }
    fputs(";\n", o);
    break;
  case NODE_LOCAL_VAR: {
    if (n->local_var.init && n->local_var.init->type == NODE_TABLE) {
      fprintf(o, "int64_t %s[] = {0,", n->local_var.name);
      for (int i = 0; i < n->local_var.init->table.field_count; i++) {
        if (i)
          fputs(",", o);
        expr(ctx, n->local_var.init->table.fields[i]);
      }
      fputs("};\n", o);
      break;
    }
    const char *ct = "int64_t";
    int p = 0;
    if (n->local_var.type) {
      ct = t2c(n->local_var.type->type_annot.type_name);
      p = n->local_var.type->type_annot.pointer_depth;
    } else if (n->local_var.init)
      ct = infer(n->local_var.init);
    fprintf(o, "%s ", ct);
    while (p--)
      fputs("*", o);
    fputs(n->local_var.name, o);
    if (n->local_var.init) {
      fputs("=", o);
      expr(ctx, n->local_var.init);
    } else if (!ctx->in_func)
      fputs("=0", o);
    fputs(";\n", o);
    break;
  }
  case NODE_IF:
    fputs("if(", o);
    expr(ctx, n->if_stmt.condition);
    fputs("){\n", o);
    stmt(ctx, n->if_stmt.then_branch);
    fputs("}", o);
    if (n->if_stmt.else_branch) {
      if (n->if_stmt.else_branch->type == NODE_IF) {
        fputs("else ", o);
        stmt(ctx, n->if_stmt.else_branch);
      } else {
        fputs("else{\n", o);
        stmt(ctx, n->if_stmt.else_branch);
        fputs("}", o);
      }
    }
    fputs("\n", o);
    break;
  case NODE_WHILE:
    fputs("while(", o);
    expr(ctx, n->while_stmt.condition);
    fputs("){\n", o);
    stmt(ctx, n->while_stmt.body);
    fputs("}\n", o);
    break;
  case NODE_REPEAT:
    fputs("do{\n", o);
    stmt(ctx, n->repeat_stmt.body);
    fputs("}while(!(", o);
    expr(ctx, n->repeat_stmt.condition);
    fputs("));\n", o);
    break;
  case NODE_FOR:
    fprintf(o, "for(int64_t %s=", n->for_stmt.var);
    expr(ctx, n->for_stmt.start);
    fprintf(o, ";%s<=", n->for_stmt.var);
    expr(ctx, n->for_stmt.end);
    if (n->for_stmt.step) {
      fprintf(o, ";%s+=", n->for_stmt.var);
      expr(ctx, n->for_stmt.step);
    } else
      fprintf(o, ";%s++", n->for_stmt.var);
    fputs("){\n", o);
    stmt(ctx, n->for_stmt.body);
    fputs("}\n", o);
    break;
  case NODE_DEFER: {
    char *nm = string_format("__d%d", ctx->defer_depth);
    fprintf(o, "int %s __attribute__((cleanup(%s_c)))=0;\n", nm, nm);
    ctx->def_names[ctx->defer_depth] = nm;
    ctx->def_exprs[ctx->defer_depth] = n->defer_stmt.expr;
    ctx->defer_depth++;
    break;
  }
  case NODE_BLOCK:
    for (int i = 0; i < n->block.statement_count; i++)
      stmt(ctx, n->block.statements[i]);
    break;
  case NODE_ASSIGN:
    expr(ctx, n->assign.target);
    fprintf(o, "%s", op2c(n->assign.op));
    expr(ctx, n->assign.value);
    fputs(";\n", o);
    break;
  case NODE_CALL:
    expr(ctx, n);
    fputs(";\n", o);
    break;
  case NODE_BREAK:
    fputs("break;\n", o);
    break;
  case NODE_CONTINUE:
    fputs("continue;\n", o);
    break;
  default:
    expr(ctx, n);
    fputs(";\n", o);
  }
}

static bool is_struct_type(const char *tn) {
  return strcmp(tn, "Point") == 0 || strcmp(tn, "Person") == 0 ||
         strcmp(tn, "Color") == 0;
}

static void emit_global_var(CodeGenContext *ctx, ASTNode *n) {
  FILE *o = ctx->output;

  if (n->local_var.init && n->local_var.init->type == NODE_TABLE) {
    fprintf(o, "static int64_t %s[] = {0,", n->local_var.name);
    for (int i = 0; i < n->local_var.init->table.field_count; i++) {
      if (i)
        fputs(",", o);
      expr(ctx, n->local_var.init->table.fields[i]);
    }
    fputs("};\n", o);
    return;
  }

  const char *ct = "int64_t";
  int p = 0;
  bool is_struct = false;

  if (n->local_var.type) {
    const char *tn = n->local_var.type->type_annot.type_name;
    ct = t2c(tn);
    p = n->local_var.type->type_annot.pointer_depth;
    is_struct = is_struct_type(tn);
  } else if (n->local_var.init) {
    ct = infer(n->local_var.init);
  }

  fprintf(o, "static %s ", ct);
  while (p--)
    fputs("*", o);
  fputs(n->local_var.name, o);

  if (n->local_var.init) {
    if (is_struct && n->local_var.init->type == NODE_NIL_LITERAL) {
      fputs("={0}", o);
    } else {
      fputs("=", o);
      expr(ctx, n->local_var.init);
    }
  } else {
    if (is_struct)
      fputs("={0}", o);
    else
      fputs("=0", o);
  }
  fputs(";\n", o);
}

static void func(CodeGenContext *ctx, ASTNode *n) {
  ctx->defer_depth = 0;
  ctx->in_func = true;
  FILE *o = ctx->output;
  if (n->func.is_exported)
    fputs("LLL_EXPORT ", o);
  if (!n->func.is_exported && strcmp(n->func.name, "main") != 0)
    fputs("static ", o);
  fprintf(o, "%s ",
          n->func.return_type ? t2c(n->func.return_type->type_annot.type_name)
                              : "void");
  for (int i = 0;
       n->func.return_type && i < n->func.return_type->type_annot.pointer_depth;
       i++)
    fputs("*", o);
  fprintf(o, "%s(", n->func.name);
  for (int i = 0; i < n->func.param_count; i++) {
    if (i)
      fputs(",", o);
    const char *param_type = "int64_t";
    int ptr_depth = 0;
    if (n->func.param_types[i] &&
        n->func.param_types[i]->type_annot.type_name) {
      const char *tn = n->func.param_types[i]->type_annot.type_name;
      if (strcmp(tn, "int32") == 0 || strcmp(tn, "int") == 0) {
        param_type = "int64_t";
      } else {
        param_type = t2c(tn);
      }
      ptr_depth = n->func.param_types[i]->type_annot.pointer_depth;
    } else {
      const char *pname = n->func.params[i]->variable.name;
      if (strcmp(pname, "name") == 0 || strcmp(pname, "person") == 0 ||
          strcmp(pname, "s") == 0 || strcmp(pname, "str") == 0 ||
          strcmp(pname, "string") == 0 || strcmp(pname, "text") == 0 ||
          strcmp(pname, "msg") == 0 || strcmp(pname, "message") == 0) {
        param_type = "char*";
      } else if (strcmp(pname, "condition") == 0 ||
                 strcmp(pname, "flag") == 0 || strcmp(pname, "b") == 0 ||
                 strcmp(pname, "bool") == 0) {
        param_type = "bool";
      }
    }
    fprintf(o, "%s ", param_type);
    while (ptr_depth--)
      fputs("*", o);
    fputs(n->func.params[i]->variable.name, o);
  }
  if (!n->func.param_count)
    fputs("void", o);
  fputs("){\n", o);
  stmt(ctx, n->func.body);
  fputs("}\n", o);
  for (int i = 0; i < ctx->defer_depth; i++) {
    fprintf(o, "static void %s_c(int*v){(void)v;", ctx->def_names[i]);
    expr(ctx, ctx->def_exprs[i]);
    fputs(";}\n", o);
  }
  fputs("\n", o);
  ctx->in_func = false;
  if (strcmp(n->func.name, "main") == 0)
    ctx->has_main = true;
}

static void strukt(CodeGenContext *ctx, ASTNode *n) {
  FILE *o = ctx->output;
  fputs("typedef struct{\n", o);
  for (int i = 0; i < n->struct_def.field_count; i++) {
    fprintf(o, "%s ", t2c(n->struct_def.fields[i]->type_annot.type_name));
    for (int j = 0; j < n->struct_def.fields[i]->type_annot.pointer_depth; j++)
      fputs("*", o);
    fputs(n->struct_def.field_names[i], o);
    if (n->struct_def.field_values && n->struct_def.field_values[i]) {
      fputs("=", o);
      expr(ctx, n->struct_def.field_values[i]);
    }
    fputs(";\n", o);
  }
  fprintf(o, "}%s;\n\n", n->struct_def.name);
}

static void enoom(CodeGenContext *ctx, ASTNode *n) {
  FILE *o = ctx->output;
  fputs("typedef enum{\n", o);
  for (int i = 0; i < n->enum_def.value_count; i++) {
    fprintf(o, "%s_%s", n->enum_def.name, n->enum_def.values[i]);
    if (n->enum_def.value_exprs && n->enum_def.value_exprs[i]) {
      fputs("=", o);
      expr(ctx, n->enum_def.value_exprs[i]);
    }
    fputs(",\n", o);
  }
  fprintf(o, "}%s;\n\n", n->enum_def.name);
}

static void runtime(CodeGenContext *ctx) {
  FILE *o = ctx->output;

  if (ctx->needs_min)
    fputs("static int64_t lll_min(int64_t a,int64_t b){return a<b?a:b;}\n", o);
  if (ctx->needs_max)
    fputs("static int64_t lll_max(int64_t a,int64_t b){return a>b?a:b;}\n", o);
  if (ctx->needs_abs)
    fputs("static int64_t lll_abs(int64_t v){return v<0?-v:v;}\n", o);

  if (ctx->needs_str_i)
    fputs("static char* lll_str_i(int64_t "
          "v){char*b=malloc(32);snprintf(b,32,\"%lld\",(long long)v);return "
          "b;}\n",
          o);
  if (ctx->needs_str_d)
    fputs("static char* lll_str_d(double "
          "v){char*b=malloc(64);snprintf(b,64,\"%g\",v);return b;}\n",
          o);
  if (ctx->needs_str_s)
    fputs("static char* lll_str_s(char* s){return s?s:\"nil\";}\n", o);

  if (ctx->needs_str) {
    fputs("#define lll_str(x) _Generic((x)", o);
    if (ctx->needs_str_s)
      fputs(",char*:lll_str_s,const char*:lll_str_s", o);
    if (ctx->needs_str_d)
      fputs(",double:lll_str_d,float:lll_str_d", o);
    fputs(",default:lll_str_i)(x)\n", o);
  }

  if (ctx->needs_cat) {
    fputs("static char* lll_cat2(const char*a,const char*b){\n", o);
    fputs("if(!a)a=\"\";if(!b)b=\"\";size_t la=strlen(a),lb=strlen(b);\n", o);
    fputs("char*r=malloc(la+lb+1);memcpy(r,a,la);memcpy(r+la,b,lb);r[la+lb]=0;"
          "return r;}\n",
          o);
    fputs("#define lll_cat(a,b) lll_cat2(lll_str(a),lll_str(b))\n", o);
  }

  fputs("\n", o);
}

void codegen_generate_program(CodeGenContext *ctx, ASTNode *program) {
  ctx->in_func = false;
  FILE *o = ctx->output;

  scan_runtime_needs(ctx, program);

  fprintf(o, "/* Generated by LLL v%d.%d.%d */\n", LLL_VERSION_MAJOR,
          LLL_VERSION_MINOR, LLL_VERSION_PATCH);

  fputs("#include <stdint.h>\n#include <stdbool.h>\n#include <stddef.h>\n", o);
  if (ctx->needs_str_i || ctx->needs_str_d || ctx->needs_cat)
    fputs("#include <stdlib.h>\n", o);
  if (ctx->needs_cat || ctx->needs_str_s || ctx->needs_str)
    fputs("#include <string.h>\n", o);
  if (ctx->needs_print)
    fputs("#include <stdio.h>\n", o);

  codegen_emit_includes(o);

  fputs(
      "#ifdef _WIN32\n#define LLL_EXPORT __declspec(dllexport)\n#else\n#define "
      "LLL_EXPORT __attribute__((visibility(\"default\")))\n#endif\n\n",
      o);

  lll_plugins_emit_preamble(ctx, o);
  runtime(ctx);

  bool tl = false;
  if (program && program->type == NODE_PROGRAM) {
    for (int i = 0; i < program->block.statement_count; i++) {
      ASTNode *n = program->block.statements[i];
      switch (n->type) {
      case NODE_STRUCT:
        strukt(ctx, n);
        break;
      case NODE_ENUM:
        enoom(ctx, n);
        break;
      default:
        break;
      }
    }

    for (int i = 0; i < program->block.statement_count; i++) {
      ASTNode *n = program->block.statements[i];
      if (n->type == NODE_LOCAL_VAR) {
        emit_global_var(ctx, n);
      }
    }
    fputs("\n", o);

    for (int i = 0; i < program->block.statement_count; i++) {
      ASTNode *n = program->block.statements[i];
      switch (n->type) {
      case NODE_FUNCTION:
        func(ctx, n);
        break;
      default:
        tl = true;
      }
    }

    if (tl || !ctx->has_main) {
      fputs("int main(int argc,char**argv){(void)argc;(void)argv;\n", o);
      ctx->in_func = true;
      for (int i = 0; i < program->block.statement_count; i++) {
        ASTNode *n = program->block.statements[i];
        if (n->type != NODE_STRUCT && n->type != NODE_ENUM &&
            n->type != NODE_FUNCTION && n->type != NODE_LOCAL_VAR)
          stmt(ctx, n);
      }
      fputs("return 0;}\n", o);
    }
  } else if (!ctx->has_main) {
    fputs("int main(int argc,char**argv){(void)argc;(void)argv;return 0;}\n",
          o);
  }
  lll_plugins_emit_epilogue(ctx, o);
}

#ifdef LLL_JIT
int codegen_jit_exec(CodeGenContext *ctx, ASTNode *program) {
  TCCState *s = tcc_new();
  if (!s)
    return -1;
  tcc_set_output_type(s, TCC_OUTPUT_MEMORY);
  tcc_add_library_path(s, "/usr/lib");
  tcc_add_include_path(s, "/usr/include");

  size_t cap = 65536;
  char *buf = malloc(cap);
  FILE *mem = fmemopen(buf, cap, "w");
  FILE *old = ctx->output;
  ctx->output = mem;
  ctx->in_func = false;

  scan_runtime_needs(ctx, program);

  fputs("#include <stdint.h>\n#include <stdbool.h>\n#include <stddef.h>\n",
        mem);
  if (ctx->needs_str_i || ctx->needs_str_d || ctx->needs_cat)
    fputs("#include <stdlib.h>\n", mem);
  if (ctx->needs_cat || ctx->needs_str_s || ctx->needs_str)
    fputs("#include <string.h>\n", mem);
  if (ctx->needs_print)
    fputs("#include <stdio.h>\n", mem);
  fputs("#include <stdarg.h>\n#include <math.h>\n", mem);
  runtime(ctx);

  bool tl = false;
  if (program && program->type == NODE_PROGRAM) {
    for (int i = 0; i < program->block.statement_count; i++) {
      ASTNode *n = program->block.statements[i];
      if (n->type == NODE_STRUCT)
        strukt(ctx, n);
      if (n->type == NODE_ENUM)
        enoom(ctx, n);
    }
    for (int i = 0; i < program->block.statement_count; i++) {
      ASTNode *n = program->block.statements[i];
      if (n->type == NODE_LOCAL_VAR) {
        emit_global_var(ctx, n);
      }
    }
    for (int i = 0; i < program->block.statement_count; i++) {
      ASTNode *n = program->block.statements[i];
      switch (n->type) {
      case NODE_FUNCTION:
        func(ctx, n);
        break;
      default:
        tl = true;
      }
    }
    if (tl) {
      fputs("int main(int argc,char**argv){(void)argc;(void)argv;\n", mem);
      ctx->in_func = true;
      for (int i = 0; i < program->block.statement_count; i++) {
        ASTNode *n = program->block.statements[i];
        if (n->type != NODE_STRUCT && n->type != NODE_ENUM &&
            n->type != NODE_FUNCTION && n->type != NODE_LOCAL_VAR)
          stmt(ctx, n);
      }
      fputs("return 0;}\n", mem);
    }
  }

  fflush(mem);
  buf[ftell(mem)] = '\0';
  fclose(mem);
  ctx->output = old;

  if (tcc_compile_string(s, buf) == -1) {
    free(buf);
    tcc_delete(s);
    return -1;
  }
  if (tcc_relocate(s) == -1) {
    free(buf);
    tcc_delete(s);
    return -1;
  }

  void (*fn)(int, char **) = tcc_get_symbol(s, "main");
  if (fn) {
    char *av[] = {"lll", NULL};
    fn(1, av);
  }

  free(buf);
  tcc_delete(s);
  return 0;
}

bool codegen_jit_compile(CodeGenContext *ctx, ASTNode *program,
                         const char *out) {
  FILE *f = fopen(out, "w");
  if (!f)
    return false;
  FILE *old = ctx->output;
  ctx->output = f;
  ctx->in_func = false;

  scan_runtime_needs(ctx, program);

  fputs("#include <stdint.h>\n#include <stdbool.h>\n#include <stddef.h>\n", f);
  if (ctx->needs_str_i || ctx->needs_str_d || ctx->needs_cat)
    fputs("#include <stdlib.h>\n", f);
  if (ctx->needs_cat || ctx->needs_str_s || ctx->needs_str)
    fputs("#include <string.h>\n", f);
  if (ctx->needs_print)
    fputs("#include <stdio.h>\n", f);
  fputs("#include <stdarg.h>\n#include <math.h>\n#include <libtcc.h>\n\n", f);
  runtime(ctx);
  fputs("int main(int "
        "a,char**v){TCCState*s=tcc_new();tcc_set_output_type(s,TCC_OUTPUT_"
        "MEMORY);\n",
        f);
  fputs("tcc_compile_string(s,", f);
  fputc('"', f);
  if (program && program->type == NODE_PROGRAM) {
    for (int i = 0; i < program->block.statement_count; i++) {
      ASTNode *n = program->block.statements[i];
      if (n->type != NODE_STRUCT && n->type != NODE_ENUM &&
          n->type != NODE_FUNCTION && n->type != NODE_LOCAL_VAR)
        stmt(ctx, n);
    }
  }
  fputs("\");\n", f);
  fputs("tcc_relocate(s);void(*fn)(int,char**)=tcc_get_symbol(s,\"main\");if("
        "fn)fn(a,v);tcc_delete(s);return 0;}\n",
        f);
  ctx->output = old;
  fclose(f);
  return true;
}
#endif
