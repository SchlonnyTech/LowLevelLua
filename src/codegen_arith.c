#include "codegen.h"
#include <string.h>

static LLVMValueRef get_strlen(CodeGenContext *ctx);
static LLVMValueRef get_malloc(CodeGenContext *ctx);
static LLVMValueRef get_strcat(CodeGenContext *ctx);

int codegen_op_from_string(const char *s) {
  if (!s)
    return OP_NONE;
  switch (s[0]) {
  case '+':
    return OP_ADD;
  case '-':
    return OP_SUB;
  case '*':
    return OP_MUL;
  case '/':
    return OP_DIV;
  case '%':
    return OP_MOD;
  case '=':
    return s[1] == '=' ? OP_EQ : OP_NONE;
  case '!':
    return s[1] == '=' ? OP_NE : OP_NONE;
  case '<':
    return s[1] == '=' ? OP_LE : OP_LT;
  case '>':
    return s[1] == '=' ? OP_GE : OP_GT;
  case '[':
    return s[1] == ']' ? OP_INDEX : OP_NONE;
  case '.':
    return s[1] == '.' ? OP_RANGE : OP_NONE;
  case 'a':
    return (s[1] == 'n' && s[2] == 'd' && s[3] == '\0') ? OP_AND : OP_NONE;
  case 'o':
    return (s[1] == 'r' && s[2] == '\0') ? OP_OR : OP_NONE;
  default:
    return OP_NONE;
  }
}

int codegen_is_float_ty(LLVMTypeRef t) {
  LLVMTypeKind k = LLVMGetTypeKind(t);
  return k == LLVMDoubleTypeKind || k == LLVMFloatTypeKind;
}

int codegen_is_str_ty(LLVMTypeRef t) {
  return LLVMGetTypeKind(t) == LLVMPointerTypeKind;
}

static LLVMValueRef as_double(CodeGenContext *ctx, LLVMValueRef v,
                              LLVMTypeRef t) {
  if (LLVMGetTypeKind(t) == LLVMDoubleTypeKind)
    return v;
  if (LLVMGetTypeKind(t) == LLVMFloatTypeKind)
    return LLVMBuildFPExt(ctx->builder, v,
                          LLVMDoubleTypeInContext(ctx->llvm_ctx), "f64");
  return LLVMBuildSIToFP(ctx->builder, v,
                         LLVMDoubleTypeInContext(ctx->llvm_ctx), "i2f");
}

LLVMValueRef codegen_arith_binop(CodeGenContext *ctx, OpKind op, LLVMValueRef l,
                                 LLVMValueRef r) {
  LLVMTypeRef lt = LLVMTypeOf(l);
  LLVMTypeRef rt = LLVMTypeOf(r);
  int lf = codegen_is_float_ty(lt);
  int rf = codegen_is_float_ty(rt);
  int isf = lf || rf;

  if (isf) {
    LLVMTypeRef dt = LLVMDoubleTypeInContext(ctx->llvm_ctx);
    if (!lf)
      l = LLVMBuildSIToFP(ctx->builder, l, dt, "l.f");
    else if (LLVMGetTypeKind(lt) == LLVMFloatTypeKind)
      l = LLVMBuildFPExt(ctx->builder, l, dt, "l.f64");
    if (!rf)
      r = LLVMBuildSIToFP(ctx->builder, r, dt, "r.f");
    else if (LLVMGetTypeKind(rt) == LLVMFloatTypeKind)
      r = LLVMBuildFPExt(ctx->builder, r, dt, "r.f64");
  }

  switch (op) {
  case OP_ADD:
    return isf ? LLVMBuildFAdd(ctx->builder, l, r, "fa")
               : LLVMBuildAdd(ctx->builder, l, r, "a");
  case OP_SUB:
    return isf ? LLVMBuildFSub(ctx->builder, l, r, "fs")
               : LLVMBuildSub(ctx->builder, l, r, "s");
  case OP_MUL:
    return isf ? LLVMBuildFMul(ctx->builder, l, r, "fm")
               : LLVMBuildMul(ctx->builder, l, r, "m");
  case OP_DIV:
    return isf ? LLVMBuildFDiv(ctx->builder, l, r, "fd")
               : LLVMBuildSDiv(ctx->builder, l, r, "d");
  case OP_MOD:
    if (isf)
      return LLVMBuildFRem(ctx->builder, l, r, "frem");
    return LLVMBuildSRem(ctx->builder, l, r, "mod");
  case OP_EQ:
    return isf ? LLVMBuildFCmp(ctx->builder, LLVMRealOEQ, l, r, "fe")
               : LLVMBuildICmp(ctx->builder, LLVMIntEQ, l, r, "e");
  case OP_NE:
    return isf ? LLVMBuildFCmp(ctx->builder, LLVMRealONE, l, r, "fne")
               : LLVMBuildICmp(ctx->builder, LLVMIntNE, l, r, "ne");
  case OP_LT:
    return isf ? LLVMBuildFCmp(ctx->builder, LLVMRealOLT, l, r, "flt")
               : LLVMBuildICmp(ctx->builder, LLVMIntSLT, l, r, "lt");
  case OP_LE:
    return isf ? LLVMBuildFCmp(ctx->builder, LLVMRealOLE, l, r, "fle")
               : LLVMBuildICmp(ctx->builder, LLVMIntSLE, l, r, "le");
  case OP_GT:
    return isf ? LLVMBuildFCmp(ctx->builder, LLVMRealOGT, l, r, "fgt")
               : LLVMBuildICmp(ctx->builder, LLVMIntSGT, l, r, "gt");
  case OP_GE:
    return isf ? LLVMBuildFCmp(ctx->builder, LLVMRealOGE, l, r, "fge")
               : LLVMBuildICmp(ctx->builder, LLVMIntSGE, l, r, "ge");
  case OP_AND:
    return LLVMBuildAnd(ctx->builder, l, r, "and");
  case OP_OR:
    return LLVMBuildOr(ctx->builder, l, r, "or");
  default:
    break;
  }
  codegen_error(ctx, "codegen_arith_binop: bad op %d", (int)op);
  return l;
}

LLVMValueRef codegen_arith_strconcat(CodeGenContext *ctx, LLVMValueRef l,
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

LLVMValueRef codegen_arith_strcmp(CodeGenContext *ctx, OpKind op,
                                  LLVMValueRef l, LLVMValueRef r) {
  LLVMTypeRef i8p = LLVMPointerType(LLVMInt8TypeInContext(ctx->llvm_ctx), 0);
  LLVMTypeRef i32t = LLVMInt32TypeInContext(ctx->llvm_ctx);
  LLVMTypeRef ft = LLVMFunctionType(i32t, (LLVMTypeRef[]){i8p, i8p}, 2, 0);
  LLVMValueRef fn = LLVMGetNamedFunction(ctx->module, "strcmp");
  if (!fn)
    fn = LLVMAddFunction(ctx->module, "strcmp", ft);
  LLVMValueRef args[] = {l, r};
  LLVMValueRef cmp = LLVMBuildCall2(ctx->builder, ft, fn, args, 2, "strcmp");
  LLVMValueRef zero = LLVMConstInt(i32t, 0, 0);
  switch (op) {
  case OP_EQ:
    return LLVMBuildICmp(ctx->builder, LLVMIntEQ, cmp, zero, "streq");
  case OP_NE:
    return LLVMBuildICmp(ctx->builder, LLVMIntNE, cmp, zero, "strne");
  case OP_LT:
    return LLVMBuildICmp(ctx->builder, LLVMIntSLT, cmp, zero, "strlt");
  case OP_LE:
    return LLVMBuildICmp(ctx->builder, LLVMIntSLE, cmp, zero, "strle");
  case OP_GT:
    return LLVMBuildICmp(ctx->builder, LLVMIntSGT, cmp, zero, "strgt");
  case OP_GE:
    return LLVMBuildICmp(ctx->builder, LLVMIntSGE, cmp, zero, "strge");
  default:
    break;
  }
  codegen_error(ctx, "codegen_arith_strcmp: bad op %d", (int)op);
  return zero;
}

LLVMValueRef codegen_arith_unary(CodeGenContext *ctx, const char *op,
                                 LLVMValueRef v) {
  if (!op)
    return v;
  LLVMTypeRef vt = LLVMTypeOf(v);
  if (codegen_is_float_ty(vt)) {
    if (strcmp(op, "-") == 0)
      return LLVMBuildFNeg(ctx->builder, v, "fn");
  }
  if (strcmp(op, "-") == 0)
    return LLVMBuildNeg(ctx->builder, v, "n");
  if (strcmp(op, "not") == 0 || strcmp(op, "!") == 0)
    return LLVMBuildNot(ctx->builder, v, "not");
  return v;
}

static LLVMValueRef get_strlen(CodeGenContext *ctx) {
  if (ctx->strlen_fn)
    return ctx->strlen_fn;
  LLVMTypeRef i8p = LLVMPointerType(LLVMInt8TypeInContext(ctx->llvm_ctx), 0);
  LLVMTypeRef i64 = LLVMInt64TypeInContext(ctx->llvm_ctx);
  LLVMTypeRef ft = LLVMFunctionType(i64, (LLVMTypeRef[]){i8p}, 1, 0);
  ctx->strlen_fn = LLVMGetNamedFunction(ctx->module, "strlen");
  if (!ctx->strlen_fn)
    ctx->strlen_fn = LLVMAddFunction(ctx->module, "strlen", ft);
  return ctx->strlen_fn;
}

static LLVMValueRef get_malloc(CodeGenContext *ctx) {
  if (ctx->malloc_fn)
    return ctx->malloc_fn;
  LLVMTypeRef i8p = LLVMPointerType(LLVMInt8TypeInContext(ctx->llvm_ctx), 0);
  LLVMTypeRef i64 = LLVMInt64TypeInContext(ctx->llvm_ctx);
  LLVMTypeRef ft = LLVMFunctionType(i8p, (LLVMTypeRef[]){i64}, 1, 0);
  ctx->malloc_fn = LLVMGetNamedFunction(ctx->module, "malloc");
  if (!ctx->malloc_fn)
    ctx->malloc_fn = LLVMAddFunction(ctx->module, "malloc", ft);
  return ctx->malloc_fn;
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
