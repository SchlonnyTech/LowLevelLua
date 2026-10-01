#include "asm.h"
#include "codegen.h"
#include "keywords.h"
#include <stdlib.h>
#include <string.h>

static LLVMValueRef get_printf(CodeGenContext *ctx) {
  if (ctx->printf_fn)
    return ctx->printf_fn;
  LLVMTypeRef i8p = LLVMPointerType(LLVMInt8TypeInContext(ctx->llvm_ctx), 0);
  LLVMTypeRef i32t = LLVMInt32TypeInContext(ctx->llvm_ctx);
  ctx->printf_type = LLVMFunctionType(i32t, (LLVMTypeRef[]){i8p}, 1, true);
  ctx->printf_fn = LLVMGetNamedFunction(ctx->module, "printf");
  if (!ctx->printf_fn)
    ctx->printf_fn = LLVMAddFunction(ctx->module, "printf", ctx->printf_type);
  return ctx->printf_fn;
}

static LLVMValueRef get_puts(CodeGenContext *ctx) {
  if (ctx->puts_fn)
    return ctx->puts_fn;
  LLVMTypeRef i8p = LLVMPointerType(LLVMInt8TypeInContext(ctx->llvm_ctx), 0);
  LLVMTypeRef i32t = LLVMInt32TypeInContext(ctx->llvm_ctx);
  ctx->puts_type = LLVMFunctionType(i32t, (LLVMTypeRef[]){i8p}, 1, 0);
  ctx->puts_fn = LLVMGetNamedFunction(ctx->module, "puts");
  if (!ctx->puts_fn)
    ctx->puts_fn = LLVMAddFunction(ctx->module, "puts", ctx->puts_type);
  return ctx->puts_fn;
}

static LLVMValueRef get_strlen(CodeGenContext *ctx) {
  if (ctx->strlen_fn)
    return ctx->strlen_fn;
  LLVMTypeRef i8p = LLVMPointerType(LLVMInt8TypeInContext(ctx->llvm_ctx), 0);
  LLVMTypeRef i64t = LLVMInt64TypeInContext(ctx->llvm_ctx);
  LLVMTypeRef ft = LLVMFunctionType(i64t, (LLVMTypeRef[]){i8p}, 1, 0);
  ctx->strlen_fn = LLVMGetNamedFunction(ctx->module, "strlen");
  if (!ctx->strlen_fn)
    ctx->strlen_fn = LLVMAddFunction(ctx->module, "strlen", ft);
  return ctx->strlen_fn;
}

static LLVMValueRef get_strcmp(CodeGenContext *ctx) {
  if (ctx->strcmp_fn)
    return ctx->strcmp_fn;
  LLVMTypeRef i8p = LLVMPointerType(LLVMInt8TypeInContext(ctx->llvm_ctx), 0);
  LLVMTypeRef i32t = LLVMInt32TypeInContext(ctx->llvm_ctx);
  LLVMTypeRef ft = LLVMFunctionType(i32t, (LLVMTypeRef[]){i8p, i8p}, 2, 0);
  ctx->strcmp_fn = LLVMGetNamedFunction(ctx->module, "strcmp");
  if (!ctx->strcmp_fn)
    ctx->strcmp_fn = LLVMAddFunction(ctx->module, "strcmp", ft);
  return ctx->strcmp_fn;
}

static LLVMValueRef get_strcat(CodeGenContext *ctx) {
  if (ctx->strcat_fn)
    return ctx->strcat_fn;
  LLVMTypeRef i8p = LLVMPointerType(LLVMInt8TypeInContext(ctx->llvm_ctx), 0);
  LLVMTypeRef ft = LLVMFunctionType(i8p, (LLVMTypeRef[]){i8p, i8p}, 2, 0);
  ctx->strcat_fn = LLVMGetNamedFunction(ctx->module, "strcat");
  if (!ctx->strcat_fn)
    ctx->strcat_fn = LLVMAddFunction(ctx->module, "strcat", ft);
  return ctx->strcat_fn;
}

static LLVMValueRef get_malloc(CodeGenContext *ctx) {
  if (ctx->malloc_fn)
    return ctx->malloc_fn;
  LLVMTypeRef i8p = LLVMPointerType(LLVMInt8TypeInContext(ctx->llvm_ctx), 0);
  LLVMTypeRef i64t = LLVMInt64TypeInContext(ctx->llvm_ctx);
  LLVMTypeRef ft = LLVMFunctionType(i8p, (LLVMTypeRef[]){i64t}, 1, 0);
  ctx->malloc_fn = LLVMGetNamedFunction(ctx->module, "malloc");
  if (!ctx->malloc_fn)
    ctx->malloc_fn = LLVMAddFunction(ctx->module, "malloc", ft);
  return ctx->malloc_fn;
}

static LLVMValueRef cached_string_literal(CodeGenContext *ctx, const char *s) {
  StrCache *c = &ctx->str_cache;
  for (int i = 0; i < c->count; i++)
    if (strcmp(c->keys[i], s) == 0)
      return c->vals[i];
  if (c->count >= c->capacity) {
    int nc = c->capacity ? c->capacity * 2 : 32;
    c->keys = realloc(c->keys, nc * sizeof(char *));
    c->vals = realloc(c->vals, nc * sizeof(LLVMValueRef));
    c->capacity = nc;
  }
  LLVMValueRef g = LLVMBuildGlobalStringPtr(ctx->builder, s, ".str");
  c->keys[c->count] = strdup(s);
  c->vals[c->count] = g;
  c->count++;
  return g;
}

LLVMValueRef codegen_int_literal(CodeGenContext *ctx, ASTNode *expr) {
  return LLVMConstInt(LLVMInt64TypeInContext(ctx->llvm_ctx),
                      expr->int_lit.value, 0);
}

LLVMValueRef codegen_float_literal(CodeGenContext *ctx, ASTNode *expr) {
  return LLVMConstReal(LLVMDoubleTypeInContext(ctx->llvm_ctx),
                       expr->float_lit.value);
}

LLVMValueRef codegen_string_literal(CodeGenContext *ctx, ASTNode *expr) {
  return cached_string_literal(ctx, expr->string_lit.value);
}

LLVMValueRef codegen_bool_literal(CodeGenContext *ctx, ASTNode *expr) {
  return LLVMConstInt(LLVMInt1TypeInContext(ctx->llvm_ctx),
                      expr->bool_lit.value, 0);
}

LLVMValueRef codegen_variable(CodeGenContext *ctx, ASTNode *expr) {
  ScopeEntry se;
  if (!codegen_scope_lookup(ctx, expr->variable.name, &se)) {
    codegen_error(ctx, "Undefined variable '%s'", expr->variable.name);
    return LLVMConstInt(LLVMInt64TypeInContext(ctx->llvm_ctx), 0, 0);
  }
  if (LLVMGetTypeKind(se.type) == LLVMArrayTypeKind)
    return se.value;
  return LLVMBuildLoad2(ctx->builder, se.type, se.value, expr->variable.name);
}

static LLVMValueRef field_ptr(CodeGenContext *ctx, LLVMValueRef obj_ptr,
                              LLVMTypeRef obj_type, const char *field,
                              LLVMTypeRef *out_field_type) {
  int idx = codegen_struct_field_index(ctx, obj_type, field);
  if (idx < 0)
    return NULL;
  LLVMValueRef zero = LLVMConstInt(LLVMInt32TypeInContext(ctx->llvm_ctx), 0, 0);
  LLVMValueRef fi = LLVMConstInt(LLVMInt32TypeInContext(ctx->llvm_ctx), idx, 0);
  LLVMValueRef idxs[] = {zero, fi};
  if (out_field_type)
    *out_field_type = LLVMStructGetTypeAtIndex(obj_type, idx);
  return LLVMBuildGEP2(ctx->builder, obj_type, obj_ptr, idxs, 2, "field");
}

LLVMValueRef codegen_field_access(CodeGenContext *ctx, ASTNode *expr) {
  if (!expr || !expr->field_access.object || !expr->field_access.field) {
    codegen_error(ctx, "Invalid field access");
    return LLVMConstNull(LLVMInt64TypeInContext(ctx->llvm_ctx));
  }
  ASTNode *object = expr->field_access.object;
  if (object->type != NODE_VARIABLE) {
    codegen_error(ctx, "Struct field access currently requires a variable");
    return LLVMConstNull(LLVMInt64TypeInContext(ctx->llvm_ctx));
  }
  ScopeEntry se;
  if (!codegen_scope_lookup(ctx, object->variable.name, &se)) {
    codegen_error(ctx, "Undefined variable '%s'", object->variable.name);
    return LLVMConstNull(LLVMInt64TypeInContext(ctx->llvm_ctx));
  }
  if (LLVMGetTypeKind(se.type) != LLVMStructTypeKind) {
    codegen_error(ctx, "Variable '%s' is not a struct", object->variable.name);
    return LLVMConstNull(LLVMInt64TypeInContext(ctx->llvm_ctx));
  }
  LLVMTypeRef ftype;
  LLVMValueRef fp =
      field_ptr(ctx, se.value, se.type, expr->field_access.field, &ftype);
  if (!fp) {
    codegen_error(ctx, "Unknown field '%s' in struct",
                  expr->field_access.field);
    return LLVMConstNull(LLVMInt64TypeInContext(ctx->llvm_ctx));
  }
  return LLVMBuildLoad2(ctx->builder, ftype, fp, "field.value");
}

LLVMValueRef codegen_table(CodeGenContext *ctx, ASTNode *expr) {
  int count = expr->table.field_count;
  LLVMTypeRef i64t = LLVMInt64TypeInContext(ctx->llvm_ctx);
  LLVMTypeRef i8p = LLVMPointerType(LLVMInt8TypeInContext(ctx->llvm_ctx), 0);

  LLVMTypeRef et;
  switch (expr->table.kind) {
  case TABLE_INT:
    et = i64t;
    break;
  case TABLE_STR:
    et = i8p;
    break;
  case TABLE_FLOAT:
    et = LLVMDoubleTypeInContext(ctx->llvm_ctx);
    break;
  default:
    et = i64t;
    break;
  }
  int typed = expr->table.kind != TABLE_DYN;

  LLVMTypeRef at = LLVMArrayType(et, count ? count : 1);
  LLVMValueRef arr = LLVMBuildAlloca(ctx->builder, at, "table");
  LLVMValueRef z = LLVMConstInt(i64t, 0, 0);

  for (int i = 0; i < count; i++) {
    LLVMValueRef idx = LLVMConstInt(i64t, i, 0);
    LLVMValueRef ep =
        LLVMBuildGEP2(ctx->builder, at, arr, (LLVMValueRef[]){z, idx}, 2, "e");
    LLVMValueRef v = codegen_expr(ctx, expr->table.fields[i]);

    if (typed) {
      LLVMTypeRef vt = LLVMTypeOf(v);
      LLVMTypeKind vk = LLVMGetTypeKind(vt);
      LLVMTypeKind ek = LLVMGetTypeKind(et);
      if (vt == et) {
      } else if (ek == LLVMIntegerTypeKind && vk == LLVMDoubleTypeKind) {
        v = LLVMBuildFPToSI(ctx->builder, v, et, "f2i");
      } else if (ek == LLVMDoubleTypeKind && vk == LLVMIntegerTypeKind) {
        v = LLVMBuildSIToFP(ctx->builder, v, et, "i2f");
      } else if (ek == LLVMIntegerTypeKind && vk == LLVMIntegerTypeKind) {
        unsigned sw = LLVMGetIntTypeWidth(vt);
        unsigned dw = LLVMGetIntTypeWidth(et);
        if (sw < dw)
          v = LLVMBuildSExt(ctx->builder, v, et, "sext");
        else if (sw > dw)
          v = LLVMBuildTrunc(ctx->builder, v, et, "trunc");
      } else if (ek == LLVMPointerTypeKind && vk == LLVMIntegerTypeKind) {
        v = LLVMBuildIntToPtr(ctx->builder, v, et, "i2p");
      } else if (ek == LLVMIntegerTypeKind && vk == LLVMPointerTypeKind) {
        v = LLVMBuildPtrToInt(ctx->builder, v, et, "p2i");
      }
    } else {
      LLVMTypeRef vt = LLVMTypeOf(v);
      LLVMTypeKind vk = LLVMGetTypeKind(vt);
      if (vk == LLVMPointerTypeKind) {
        v = LLVMBuildPtrToInt(ctx->builder, v, i64t, "str");
        v = LLVMBuildOr(ctx->builder, v, LLVMConstInt(i64t, 1ULL << 63, 0),
                        "tag");
      } else if (vk == LLVMDoubleTypeKind) {
        v = LLVMBuildFPToSI(ctx->builder, v, i64t, "f2i");
      } else if (vt != i64t && vk == LLVMIntegerTypeKind) {
        unsigned sw = LLVMGetIntTypeWidth(vt);
        unsigned dw = LLVMGetIntTypeWidth(i64t);
        if (sw < dw)
          v = LLVMBuildSExt(ctx->builder, v, i64t, "sext");
        else if (sw > dw)
          v = LLVMBuildTrunc(ctx->builder, v, i64t, "trunc");
      }
    }
    LLVMBuildStore(ctx->builder, v, ep);
  }
  return arr;
}

static const char *llvm_type_name(CodeGenContext *ctx, LLVMTypeRef t) {
  if (!t)
    return "null";
  switch (LLVMGetTypeKind(t)) {
  case LLVMIntegerTypeKind:
    return (t == LLVMInt1TypeInContext(ctx->llvm_ctx)) ? "bool" : "int";
  case LLVMDoubleTypeKind:
    return "f64";
  case LLVMFloatTypeKind:
    return "f32";
  case LLVMPointerTypeKind:
    return "ptr";
  case LLVMVoidTypeKind:
    return "void";
  case LLVMArrayTypeKind:
    return "array";
  case LLVMStructTypeKind:
    return "struct";
  default:
    return "?";
  }
}

static LLVMValueRef codegen_index(CodeGenContext *ctx, ASTNode *expr) {
  LLVMValueRef arr = codegen_expr(ctx, expr->binary.left);
  LLVMValueRef idx;
  if (expr->binary.right->type == NODE_INT_LITERAL) {
    idx = LLVMConstInt(LLVMInt64TypeInContext(ctx->llvm_ctx),
                       expr->binary.right->int_lit.value - 1, 0);
  } else {
    idx = codegen_expr(ctx, expr->binary.right);
  }

  if (expr->binary.left->type == NODE_VARIABLE) {
    ScopeEntry se;
    if (codegen_scope_lookup(ctx, expr->binary.left->variable.name, &se)) {
      if (LLVMGetTypeKind(se.type) == LLVMArrayTypeKind) {
        LLVMValueRef z =
            LLVMConstInt(LLVMInt64TypeInContext(ctx->llvm_ctx), 0, 0);
        LLVMValueRef ep = LLVMBuildGEP2(ctx->builder, se.type, arr,
                                        (LLVMValueRef[]){z, idx}, 2, "e");
        return LLVMBuildLoad2(ctx->builder,
                              LLVMInt64TypeInContext(ctx->llvm_ctx), ep, "v");
      }
      if (LLVMGetTypeKind(se.type) == LLVMPointerTypeKind) {
        LLVMValueRef ep =
            LLVMBuildGEP2(ctx->builder, LLVMInt8TypeInContext(ctx->llvm_ctx),
                          arr, (LLVMValueRef[]){idx}, 1, "b");
        LLVMValueRef byte_val = LLVMBuildLoad2(
            ctx->builder, LLVMInt8TypeInContext(ctx->llvm_ctx), ep, "byte");
        return LLVMBuildZExt(ctx->builder, byte_val,
                             LLVMInt64TypeInContext(ctx->llvm_ctx), "zext");
      }
    }
  }

  LLVMValueRef ep =
      LLVMBuildGEP2(ctx->builder, LLVMInt64TypeInContext(ctx->llvm_ctx), arr,
                    (LLVMValueRef[]){idx}, 1, "i");
  return LLVMBuildLoad2(ctx->builder, LLVMInt64TypeInContext(ctx->llvm_ctx), ep,
                        "v");
}

static LLVMValueRef codegen_range(CodeGenContext *ctx, ASTNode *expr) {
  LLVMValueRef l = codegen_expr(ctx, expr->binary.left);
  LLVMValueRef r = codegen_expr(ctx, expr->binary.right);
  LLVMTypeRef i8p = LLVMPointerType(LLVMInt8TypeInContext(ctx->llvm_ctx), 0);
  LLVMTypeRef i64 = LLVMInt64TypeInContext(ctx->llvm_ctx);

  LLVMValueRef sl = get_strlen(ctx);
  LLVMTypeRef slt = LLVMFunctionType(i64, (LLVMTypeRef[]){i8p}, 1, 0);
  LLVMValueRef ll = LLVMBuildCall2(ctx->builder, slt, sl, &l, 1, "l");
  LLVMValueRef rl = LLVMBuildCall2(ctx->builder, slt, sl, &r, 1, "r");
  LLVMValueRef tot = LLVMBuildAdd(ctx->builder, ll, rl, "t");
  LLVMValueRef sz =
      LLVMBuildAdd(ctx->builder, tot, LLVMConstInt(i64, 1, 0), "s");

  LLVMValueRef mf = get_malloc(ctx);
  LLVMTypeRef mt = LLVMFunctionType(i8p, (LLVMTypeRef[]){i64}, 1, 0);
  LLVMValueRef res = LLVMBuildCall2(ctx->builder, mt, mf, &sz, 1, "m");
  LLVMBuildStore(ctx->builder,
                 LLVMConstInt(LLVMInt8TypeInContext(ctx->llvm_ctx), 0, 0), res);

  LLVMValueRef scf = get_strcat(ctx);
  LLVMTypeRef sct = LLVMFunctionType(i8p, (LLVMTypeRef[]){i8p, i8p}, 2, 0);
  LLVMValueRef a1[] = {res, l};
  LLVMBuildCall2(ctx->builder, sct, scf, a1, 2, "");
  LLVMValueRef a2[] = {res, r};
  LLVMBuildCall2(ctx->builder, sct, scf, a2, 2, "");
  return res;
}

static LLVMValueRef codegen_string_concat(CodeGenContext *ctx, LLVMValueRef l,
                                          LLVMValueRef r) {
  LLVMTypeRef i8p = LLVMPointerType(LLVMInt8TypeInContext(ctx->llvm_ctx), 0);
  LLVMTypeRef i64 = LLVMInt64TypeInContext(ctx->llvm_ctx);
  LLVMValueRef sl = get_strlen(ctx);
  LLVMTypeRef slt = LLVMFunctionType(i64, (LLVMTypeRef[]){i8p}, 1, 0);
  LLVMValueRef ll = LLVMBuildCall2(ctx->builder, slt, sl, &l, 1, "l");
  LLVMValueRef rl = LLVMBuildCall2(ctx->builder, slt, sl, &r, 1, "r");
  LLVMValueRef tot = LLVMBuildAdd(ctx->builder, ll, rl, "t");
  LLVMValueRef sz =
      LLVMBuildAdd(ctx->builder, tot, LLVMConstInt(i64, 1, 0), "s");
  LLVMValueRef mf = get_malloc(ctx);
  LLVMTypeRef mt = LLVMFunctionType(i8p, (LLVMTypeRef[]){i64}, 1, 0);
  LLVMValueRef res = LLVMBuildCall2(ctx->builder, mt, mf, &sz, 1, "m");
  LLVMBuildStore(ctx->builder,
                 LLVMConstInt(LLVMInt8TypeInContext(ctx->llvm_ctx), 0, 0), res);
  LLVMValueRef scf = get_strcat(ctx);
  LLVMTypeRef sct = LLVMFunctionType(i8p, (LLVMTypeRef[]){i8p, i8p}, 2, 0);
  LLVMValueRef a1[] = {res, l};
  LLVMBuildCall2(ctx->builder, sct, scf, a1, 2, "");
  LLVMValueRef a2[] = {res, r};
  LLVMBuildCall2(ctx->builder, sct, scf, a2, 2, "");
  return res;
}

LLVMValueRef codegen_binary_op(CodeGenContext *ctx, ASTNode *expr) {
  OpKind op = expr->binary.op_kind;
  if (op == OP_NONE)
    op = codegen_op_from_string(expr->binary.op);

  if (op == OP_NONE) {
    codegen_error(ctx, "Unknown operator '%s'",
                  expr->binary.op ? expr->binary.op : "(null)");
    return LLVMConstInt(LLVMInt64TypeInContext(ctx->llvm_ctx), 0, 0);
  }

  if (op == OP_INDEX) {
    LLVMValueRef arr = codegen_expr(ctx, expr->binary.left);
    LLVMValueRef idx;
    if (expr->binary.right->type == NODE_INT_LITERAL)
      idx = LLVMConstInt(LLVMInt64TypeInContext(ctx->llvm_ctx),
                         expr->binary.right->int_lit.value - 1, 0);
    else
      idx = codegen_expr(ctx, expr->binary.right);

    if (expr->binary.left->type == NODE_VARIABLE) {
      ScopeEntry se;
      if (codegen_scope_lookup(ctx, expr->binary.left->variable.name, &se)) {
        if (LLVMGetTypeKind(se.type) == LLVMArrayTypeKind) {
          LLVMTypeRef elem = LLVMGetElementType(se.type);
          LLVMValueRef z =
              LLVMConstInt(LLVMInt64TypeInContext(ctx->llvm_ctx), 0, 0);
          LLVMValueRef ep = LLVMBuildGEP2(ctx->builder, se.type, arr,
                                          (LLVMValueRef[]){z, idx}, 2, "e");
          return LLVMBuildLoad2(ctx->builder, elem, ep, "v");
        }
        if (LLVMGetTypeKind(se.type) == LLVMPointerTypeKind) {
          LLVMValueRef ep =
              LLVMBuildGEP2(ctx->builder, LLVMInt8TypeInContext(ctx->llvm_ctx),
                            arr, (LLVMValueRef[]){idx}, 1, "b");
          LLVMValueRef b = LLVMBuildLoad2(
              ctx->builder, LLVMInt8TypeInContext(ctx->llvm_ctx), ep, "byte");
          return LLVMBuildZExt(ctx->builder, b,
                               LLVMInt64TypeInContext(ctx->llvm_ctx), "zext");
        }
      }
    }

    LLVMValueRef ep =
        LLVMBuildGEP2(ctx->builder, LLVMInt64TypeInContext(ctx->llvm_ctx), arr,
                      (LLVMValueRef[]){idx}, 1, "i");
    return LLVMBuildLoad2(ctx->builder, LLVMInt64TypeInContext(ctx->llvm_ctx),
                          ep, "v");
  }

  LLVMValueRef l = codegen_expr(ctx, expr->binary.left);
  LLVMValueRef r = codegen_expr(ctx, expr->binary.right);

  if (op == OP_RANGE) {
    LLVMTypeRef i8p = LLVMPointerType(LLVMInt8TypeInContext(ctx->llvm_ctx), 0);
    if (LLVMGetTypeKind(LLVMTypeOf(l)) != LLVMPointerTypeKind)
      l = LLVMBuildIntToPtr(ctx->builder, l, i8p, "l.i2p");
    if (LLVMGetTypeKind(LLVMTypeOf(r)) != LLVMPointerTypeKind)
      r = LLVMBuildIntToPtr(ctx->builder, r, i8p, "r.i2p");
    return codegen_arith_strconcat(ctx, l, r);
  }

  int ls = codegen_is_str_ty(LLVMTypeOf(l));
  int rs = codegen_is_str_ty(LLVMTypeOf(r));

  if (ls && rs &&
      (op == OP_EQ || op == OP_NE || op == OP_LT || op == OP_LE ||
       op == OP_GT || op == OP_GE))
    return codegen_arith_strcmp(ctx, op, l, r);

  if (op == OP_ADD && (ls || rs)) {
    LLVMTypeRef i8p = LLVMPointerType(LLVMInt8TypeInContext(ctx->llvm_ctx), 0);
    if (!ls)
      l = LLVMBuildIntToPtr(ctx->builder, l, i8p, "l.i2p");
    if (!rs)
      r = LLVMBuildIntToPtr(ctx->builder, r, i8p, "r.i2p");
    return codegen_arith_strconcat(ctx, l, r);
  }

  if (ls || rs) {
    codegen_error(ctx,
                  "type mismatch at %d:%d: left '%s' right '%s' (op_kind=%d)",
                  expr->line, expr->column, llvm_type_name(ctx, LLVMTypeOf(l)),
                  llvm_type_name(ctx, LLVMTypeOf(r)), (int)op);
    return LLVMConstInt(LLVMInt64TypeInContext(ctx->llvm_ctx), 0, 0);
  }

  return codegen_arith_binop(ctx, op, l, r);
}

LLVMValueRef codegen_unary_op(CodeGenContext *ctx, ASTNode *expr) {
  LLVMValueRef v = codegen_expr(ctx, expr->unary.operand);
  return codegen_arith_unary(ctx, expr->unary.op, v);
}

static LLVMValueRef builtin_min(CodeGenContext *ctx, ASTNode *c) {
  LLVMValueRef a = codegen_expr(ctx, c->call.args[0]);
  LLVMValueRef b = codegen_expr(ctx, c->call.args[1]);
  if (LLVMGetTypeKind(LLVMTypeOf(a)) == LLVMDoubleTypeKind) {
    LLVMValueRef cond = LLVMBuildFCmp(ctx->builder, LLVMRealOLT, a, b, "c");
    return LLVMBuildSelect(ctx->builder, cond, a, b, "min");
  }
  LLVMValueRef cond = LLVMBuildICmp(ctx->builder, LLVMIntSLT, a, b, "c");
  return LLVMBuildSelect(ctx->builder, cond, a, b, "min");
}

static LLVMValueRef builtin_max(CodeGenContext *ctx, ASTNode *c) {
  LLVMValueRef a = codegen_expr(ctx, c->call.args[0]);
  LLVMValueRef b = codegen_expr(ctx, c->call.args[1]);
  if (LLVMGetTypeKind(LLVMTypeOf(a)) == LLVMDoubleTypeKind) {
    LLVMValueRef cond = LLVMBuildFCmp(ctx->builder, LLVMRealOGT, a, b, "c");
    return LLVMBuildSelect(ctx->builder, cond, a, b, "max");
  }
  LLVMValueRef cond = LLVMBuildICmp(ctx->builder, LLVMIntSGT, a, b, "c");
  return LLVMBuildSelect(ctx->builder, cond, a, b, "max");
}

static LLVMValueRef builtin_abs(CodeGenContext *ctx, ASTNode *c) {
  LLVMValueRef v = codegen_expr(ctx, c->call.args[0]);
  if (LLVMGetTypeKind(LLVMTypeOf(v)) == LLVMDoubleTypeKind) {
    LLVMTypeRef dt = LLVMDoubleTypeInContext(ctx->llvm_ctx);
    LLVMTypeRef ft = LLVMFunctionType(dt, (LLVMTypeRef[]){dt}, 1, 0);
    LLVMValueRef fn = LLVMGetNamedFunction(ctx->module, "fabs");
    if (!fn)
      fn = LLVMAddFunction(ctx->module, "fabs", ft);
    return LLVMBuildCall2(ctx->builder, ft, fn, &v, 1, "fabs");
  }
  LLVMValueRef z = LLVMConstInt(LLVMInt64TypeInContext(ctx->llvm_ctx), 0, 0);
  LLVMValueRef n = LLVMBuildNeg(ctx->builder, v, "n");
  LLVMValueRef cond = LLVMBuildICmp(ctx->builder, LLVMIntSLT, v, z, "c");
  return LLVMBuildSelect(ctx->builder, cond, n, v, "abs");
}

static LLVMValueRef codegen_print(CodeGenContext *ctx, ASTNode *expr) {
  LLVMTypeRef i8p = LLVMPointerType(LLVMInt8TypeInContext(ctx->llvm_ctx), 0);
  LLVMTypeRef i64t = LLVMInt64TypeInContext(ctx->llvm_ctx);
  LLVMTypeRef f64t = LLVMDoubleTypeInContext(ctx->llvm_ctx);

  LLVMValueRef printf_fn = get_printf(ctx);
  LLVMValueRef puts_fn = get_puts(ctx);

  if (!ctx->fmt_int) {
    ctx->fmt_int = LLVMBuildGlobalStringPtr(ctx->builder, "%lld\n", "fmt_int");
    ctx->fmt_flt = LLVMBuildGlobalStringPtr(ctx->builder, "%f\n", "fmt_flt");
  }

  for (int i = 0; i < expr->call.arg_count; i++) {
    ASTNode *arg = expr->call.args[i];

    if (arg->type == NODE_VARIABLE) {
      ScopeEntry se;
      if (codegen_scope_lookup(ctx, arg->variable.name, &se) &&
          LLVMGetTypeKind(se.type) == LLVMArrayTypeKind) {
        LLVMValueRef arr = se.value;
        int count = LLVMGetArrayLength(se.type);
        for (int j = 0; j < count; j++) {
          LLVMValueRef idx = LLVMConstInt(i64t, j, 0);
          LLVMValueRef z = LLVMConstInt(i64t, 0, 0);
          LLVMValueRef ep = LLVMBuildGEP2(ctx->builder, se.type, arr,
                                          (LLVMValueRef[]){z, idx}, 2, "e");
          LLVMValueRef val = LLVMBuildLoad2(ctx->builder, i64t, ep, "v");
          LLVMValueRef is_str =
              LLVMBuildICmp(ctx->builder, LLVMIntSLT, val,
                            LLVMConstInt(i64t, 0, 0), "is_str");
          LLVMValueRef mask = LLVMConstInt(i64t, 0x7FFFFFFFFFFFFFFFULL, 0);
          LLVMValueRef ptr_val = LLVMBuildAnd(ctx->builder, val, mask, "clear");
          LLVMValueRef str_ptr =
              LLVMBuildIntToPtr(ctx->builder, ptr_val, i8p, "sp");
          LLVMValueRef int_ptr =
              LLVMBuildIntToPtr(ctx->builder, val, i8p, "ip");
          LLVMValueRef selected =
              LLVMBuildSelect(ctx->builder, is_str, str_ptr, int_ptr, "sel");
          LLVMValueRef sa[] = {selected};
          LLVMBuildCall2(ctx->builder, ctx->puts_type, puts_fn, sa, 1, "p");
        }
        continue;
      }
    }

    LLVMValueRef val = codegen_expr(ctx, arg);
    LLVMTypeRef vt = LLVMTypeOf(val);
    LLVMTypeKind vk = LLVMGetTypeKind(vt);

    if (vk == LLVMPointerTypeKind) {
      LLVMValueRef args[] = {val};
      LLVMBuildCall2(ctx->builder, ctx->puts_type, puts_fn, args, 1, "print");
    } else if (vk == LLVMDoubleTypeKind) {
      LLVMValueRef args[] = {ctx->fmt_flt, val};
      LLVMBuildCall2(ctx->builder, ctx->printf_type, printf_fn, args, 2,
                     "print");
    } else if (vk == LLVMFloatTypeKind) {
      LLVMValueRef f64 = LLVMBuildFPExt(ctx->builder, val, f64t, "f64");
      LLVMValueRef args[] = {ctx->fmt_flt, f64};
      LLVMBuildCall2(ctx->builder, ctx->printf_type, printf_fn, args, 2,
                     "print");
    } else {
      LLVMValueRef args[] = {ctx->fmt_int, val};
      LLVMBuildCall2(ctx->builder, ctx->printf_type, printf_fn, args, 2,
                     "print");
    }
  }
  return LLVMConstInt(LLVMInt64TypeInContext(ctx->llvm_ctx), 0, 0);
}

LLVMValueRef codegen_indirect_call(CodeGenContext *ctx, ASTNode *expr) {
  LLVMValueRef fn_ptr = codegen_expr(ctx, expr->call.callee);
  if (!fn_ptr) {
    codegen_error(ctx, "indirect call: callee expression is null");
    return LLVMConstInt(LLVMInt64TypeInContext(ctx->llvm_ctx), 0, 0);
  }

  int ac = expr->call.arg_count;
  LLVMValueRef *args = ac ? malloc(sizeof(LLVMValueRef) * ac) : NULL;
  LLVMTypeRef *types = ac ? malloc(sizeof(LLVMTypeRef) * ac) : NULL;

  LLVMTypeRef i64 = LLVMInt64TypeInContext(ctx->llvm_ctx);

  for (int i = 0; i < ac; i++) {
    LLVMValueRef a = codegen_expr(ctx, expr->call.args[i]);
    LLVMTypeRef at = LLVMTypeOf(a);
    if (LLVMGetTypeKind(at) == LLVMPointerTypeKind)
      a = LLVMBuildPtrToInt(ctx->builder, a, i64, "p2i");
    else if (at != i64)
      a = LLVMBuildSExt(ctx->builder, a, i64, "ext");
    args[i] = a;
    types[i] = i64;
  }

  LLVMTypeRef fn_ty = LLVMFunctionType(i64, types, ac, 0);
  LLVMValueRef casted = LLVMBuildIntToPtr(ctx->builder, fn_ptr,
                                          LLVMPointerType(fn_ty, 0), "fnptr");
  LLVMValueRef result =
      LLVMBuildCall2(ctx->builder, fn_ty, casted, args, ac, "ffi");

  free(args);
  free(types);
  return result;
}

LLVMValueRef codegen_call(CodeGenContext *ctx, ASTNode *expr) {
  const char *name = expr->call.name;

  int bt = llvm_get_builtin_type(ctx, name);
  if (bt == BUILTIN_CUSTOM) {
    if (strcmp(name, "min") == 0)
      return builtin_min(ctx, expr);
    if (strcmp(name, "max") == 0)
      return builtin_max(ctx, expr);
    if (strcmp(name, "abs") == 0)
      return builtin_abs(ctx, expr);
  }

  if (strncmp(name, "string.", 7) == 0)
    return codegen_string_lib(ctx, name, expr);

  KeywordHandler *kh = find_keyword(name);
  if (kh && kh->codegen)
    return kh->codegen(ctx, NULL, expr);

  if (strcmp(name, "print") == 0)
    return codegen_print(ctx, expr);

  FuncEntry *fe = codegen_func_map_find(ctx, name);
  if (fe) {
    int ac = expr->call.arg_count;
    LLVMValueRef stack_args[16];
    LLVMValueRef *call_args =
        ac <= 16 ? stack_args : malloc(sizeof(LLVMValueRef) * ac);

    LLVMTypeRef call_type = fe->type;
    if (LLVMGetTypeKind(call_type) == LLVMPointerTypeKind)
      call_type = LLVMGetElementType(call_type);

    for (int i = 0; i < ac; i++)
      call_args[i] = codegen_expr(ctx, expr->call.args[i]);

    LLVMValueRef result = LLVMBuildCall2(ctx->builder, call_type, fe->value,
                                         call_args, ac, "call");
    if (call_args != stack_args)
      free(call_args);
    return result;
  }

  ScopeEntry se;
  if (codegen_scope_lookup(ctx, name, &se)) {
    LLVMValueRef fn_ptr = codegen_variable(ctx, expr);

    if (fn_ptr) {
      int ac = expr->call.arg_count;
      LLVMValueRef *args = ac ? malloc(sizeof(LLVMValueRef) * ac) : NULL;
      LLVMTypeRef *types = ac ? malloc(sizeof(LLVMTypeRef) * ac) : NULL;
      LLVMTypeRef i64 = LLVMInt64TypeInContext(ctx->llvm_ctx);

      for (int i = 0; i < ac; i++) {
        LLVMValueRef a = codegen_expr(ctx, expr->call.args[i]);
        LLVMTypeRef at = LLVMTypeOf(a);
        if (LLVMGetTypeKind(at) == LLVMPointerTypeKind)
          a = LLVMBuildPtrToInt(ctx->builder, a, i64, "p2i");
        else if (at != i64)
          a = LLVMBuildSExt(ctx->builder, a, i64, "ext");
        args[i] = a;
        types[i] = i64;
      }

      LLVMTypeRef fn_ty = LLVMFunctionType(i64, types, ac, 0);
      LLVMValueRef casted = LLVMBuildIntToPtr(
          ctx->builder, fn_ptr, LLVMPointerType(fn_ty, 0), "fnptr");
      LLVMValueRef result =
          LLVMBuildCall2(ctx->builder, fn_ty, casted, args, ac, "ffi");

      free(args);
      free(types);
      return result;
    }
  }

  int ac = expr->call.arg_count;
  LLVMValueRef stack_args[16];
  LLVMValueRef *call_args =
      ac <= 16 ? stack_args : malloc(sizeof(LLVMValueRef) * ac);
  LLVMTypeRef stack_ptypes[16];
  LLVMTypeRef *ptypes =
      ac <= 16 ? stack_ptypes : malloc(sizeof(LLVMTypeRef) * ac);

  for (int i = 0; i < ac; i++) {
    call_args[i] = codegen_expr(ctx, expr->call.args[i]);
    ptypes[i] = LLVMTypeOf(call_args[i]);
  }

  LLVMTypeRef rt = LLVMInt64TypeInContext(ctx->llvm_ctx);
  LLVMTypeRef ft = LLVMFunctionType(rt, ptypes, ac, false);
  LLVMValueRef func = LLVMAddFunction(ctx->module, name, ft);
  LLVMSetLinkage(func, LLVMExternalLinkage);
  LLVMValueRef result =
      LLVMBuildCall2(ctx->builder, ft, func, call_args, ac, "call");
  codegen_func_map_add(ctx, name, func, ft);

  if (call_args != stack_args)
    free(call_args);
  if (ptypes != stack_ptypes)
    free(ptypes);
  return result;
}

LLVMValueRef codegen_ternary(CodeGenContext *ctx, ASTNode *expr) {
  LLVMValueRef c = codegen_expr(ctx, expr->ternary.condition);
  LLVMValueRef t = codegen_expr(ctx, expr->ternary.then_expr);
  LLVMValueRef e = codegen_expr(ctx, expr->ternary.else_expr);
  return LLVMBuildSelect(ctx->builder, c, t, e, "tern");
}

LLVMValueRef codegen_cast(CodeGenContext *ctx, ASTNode *expr) {
  LLVMValueRef v = codegen_expr(ctx, expr->cast.expr);
  LLVMTypeRef tt = codegen_type_from_string(ctx, expr->cast.type_name);
  LLVMTypeRef st = LLVMTypeOf(v);
  if (LLVMGetTypeKind(st) == LLVMIntegerTypeKind &&
      LLVMGetTypeKind(tt) == LLVMDoubleTypeKind)
    return LLVMBuildSIToFP(ctx->builder, v, tt, "c");
  if (LLVMGetTypeKind(st) == LLVMDoubleTypeKind &&
      LLVMGetTypeKind(tt) == LLVMIntegerTypeKind)
    return LLVMBuildFPToSI(ctx->builder, v, tt, "c");
  return v;
}

LLVMValueRef codegen_expr(CodeGenContext *ctx, ASTNode *expr) {
  if (!expr)
    return NULL;
  switch (expr->type) {
  case NODE_INT_LITERAL:
    return codegen_int_literal(ctx, expr);
  case NODE_FLOAT_LITERAL:
    return codegen_float_literal(ctx, expr);
  case NODE_STRING_LITERAL:
    return codegen_string_literal(ctx, expr);
  case NODE_BOOL_LITERAL:
    return codegen_bool_literal(ctx, expr);
  case NODE_VARIABLE:
    return codegen_variable(ctx, expr);
  case NODE_BINARY_OP:
    return codegen_binary_op(ctx, expr);
  case NODE_UNARY_OP:
    return codegen_unary_op(ctx, expr);
  case NODE_CALL:
    return codegen_call(ctx, expr);
  case NODE_FIELD_ACCESS:
    return codegen_field_access(ctx, expr);
  case NODE_TERNARY:
    return codegen_ternary(ctx, expr);
  case NODE_TYPE_CAST:
    return codegen_cast(ctx, expr);
  case NODE_TABLE:
    return codegen_table(ctx, expr);
  case NODE_ASM_BLOCK:
    return asm_emit(ctx, expr);
  case NODE_NIL_LITERAL:
    return LLVMConstNull(
        LLVMPointerType(LLVMInt8TypeInContext(ctx->llvm_ctx), 0));
  default:
    codegen_error(ctx, "Unknown expr type %d", expr->type);
    return LLVMConstInt(LLVMInt64TypeInContext(ctx->llvm_ctx), 0, 0);
  }
}
