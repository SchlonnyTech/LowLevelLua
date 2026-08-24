#include "codegen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

LLVMValueRef codegen_int_literal(CodeGenContext *ctx, ASTNode *expr) {
  return LLVMConstInt(LLVMInt64TypeInContext(ctx->llvm_ctx),
                      expr->int_lit.value, 0);
}

LLVMValueRef codegen_float_literal(CodeGenContext *ctx, ASTNode *expr) {
  return LLVMConstReal(LLVMDoubleTypeInContext(ctx->llvm_ctx),
                       expr->float_lit.value);
}

LLVMValueRef codegen_string_literal(CodeGenContext *ctx, ASTNode *expr) {
  return codegen_string_create(ctx, expr->string_lit.value);
}

LLVMValueRef codegen_bool_literal(CodeGenContext *ctx, ASTNode *expr) {
  return LLVMConstInt(LLVMInt1TypeInContext(ctx->llvm_ctx),
                      expr->bool_lit.value, 0);
}
LLVMValueRef codegen_variable(CodeGenContext *ctx, ASTNode *expr) {
  LLVMValueRef var = codegen_scope_get(ctx, expr->variable.name);
  if (!var) {
    codegen_error(ctx, "Undefined variable '%s'", expr->variable.name);
    return LLVMConstInt(LLVMInt64TypeInContext(ctx->llvm_ctx), 0, 0);
  }
  LLVMTypeRef elem_type = codegen_scope_get_type(ctx, expr->variable.name);
  if (!elem_type) {
    elem_type = LLVMInt64TypeInContext(ctx->llvm_ctx);
  }
  if (LLVMGetTypeKind(elem_type) == LLVMArrayTypeKind ||
      LLVMGetTypeKind(elem_type) == LLVMStructTypeKind) {
    return var;
  }
  return LLVMBuildLoad2(ctx->builder, elem_type, var, expr->variable.name);
}
LLVMValueRef codegen_field_access(CodeGenContext *ctx, ASTNode *expr) {
  if (!expr || !expr->field_access.object || !expr->field_access.field)
    return LLVMConstInt(LLVMInt64TypeInContext(ctx->llvm_ctx), 0, 0);
  if (expr->field_access.object->type == NODE_VARIABLE) {
    const char *en = expr->field_access.object->variable.name;
    const char *fn = expr->field_access.field;
    char *nm = malloc(strlen(en) + strlen(fn) + 2);
    sprintf(nm, "%s_%s", en, fn);
    LLVMValueRef g = LLVMGetNamedGlobal(ctx->module, nm);
    free(nm);
    if (g)
      return LLVMBuildLoad2(ctx->builder, LLVMInt64TypeInContext(ctx->llvm_ctx),
                            g, "enum");
  }
  return LLVMConstInt(LLVMInt64TypeInContext(ctx->llvm_ctx), 0, 0);
}

LLVMValueRef codegen_table(CodeGenContext *ctx, ASTNode *expr) {
  int count = expr->table.field_count;
  LLVMTypeRef et = LLVMInt64TypeInContext(ctx->llvm_ctx);
  LLVMTypeRef at = LLVMArrayType(et, count + 1);
  LLVMValueRef arr = LLVMBuildAlloca(ctx->builder, at, "table");
  LLVMValueRef z = LLVMConstInt(LLVMInt64TypeInContext(ctx->llvm_ctx), 0, 0);

  for (int i = 0; i < count; i++) {
    LLVMValueRef idx =
        LLVMConstInt(LLVMInt64TypeInContext(ctx->llvm_ctx), i + 1, 0);
    LLVMValueRef ep =
        LLVMBuildGEP2(ctx->builder, at, arr, (LLVMValueRef[]){z, idx}, 2, "e");
    LLVMValueRef v = codegen_expr(ctx, expr->table.fields[i]);
    LLVMBuildStore(ctx->builder, v, ep);
  }
  return arr;
}

LLVMValueRef codegen_binary_op(CodeGenContext *ctx, ASTNode *expr) {
  const char *op = expr->binary.op;
  if (!op)
    return codegen_expr(ctx, expr->binary.left);
  if (strcmp(op, "[]") == 0) {
    LLVMValueRef arr = codegen_expr(ctx, expr->binary.left);
    LLVMValueRef idx;
    if (expr->binary.right->type == NODE_INT_LITERAL) {
      idx = LLVMConstInt(LLVMInt64TypeInContext(ctx->llvm_ctx),
                         expr->binary.right->int_lit.value - 1, 0);
    }
    LLVMValueRef ep =
        LLVMBuildGEP2(ctx->builder, LLVMInt64TypeInContext(ctx->llvm_ctx), arr,
                      (LLVMValueRef[]){idx}, 1, "i");
    return LLVMBuildLoad2(ctx->builder, LLVMInt64TypeInContext(ctx->llvm_ctx),
                          ep, "v");
  }
  if (strcmp(op, "..") == 0) {
    LLVMValueRef l = codegen_expr(ctx, expr->binary.left);
    LLVMValueRef r = codegen_expr(ctx, expr->binary.right);

    LLVMTypeRef i8p = LLVMPointerType(LLVMInt8TypeInContext(ctx->llvm_ctx), 0);
    LLVMTypeRef i64 = LLVMInt64TypeInContext(ctx->llvm_ctx);

    LLVMTypeRef slt = LLVMFunctionType(i64, (LLVMTypeRef[]){i8p}, 1, 0);
    LLVMValueRef sl = LLVMGetNamedFunction(ctx->module, "strlen");
    if (!sl)
      sl = LLVMAddFunction(ctx->module, "strlen", slt);

    LLVMValueRef ll = LLVMBuildCall2(ctx->builder, slt, sl, &l, 1, "l");
    LLVMValueRef rl = LLVMBuildCall2(ctx->builder, slt, sl, &r, 1, "r");
    LLVMValueRef tot = LLVMBuildAdd(ctx->builder, ll, rl, "t");
    LLVMValueRef sz =
        LLVMBuildAdd(ctx->builder, tot, LLVMConstInt(i64, 1, 0), "s");

    LLVMTypeRef mt = LLVMFunctionType(i8p, (LLVMTypeRef[]){i64}, 1, 0);
    LLVMValueRef mf = LLVMGetNamedFunction(ctx->module, "malloc");
    if (!mf)
      mf = LLVMAddFunction(ctx->module, "malloc", mt);
    LLVMValueRef res = LLVMBuildCall2(ctx->builder, mt, mf, &sz, 1, "m");

    LLVMBuildStore(ctx->builder,
                   LLVMConstInt(LLVMInt8TypeInContext(ctx->llvm_ctx), 0, 0),
                   res);

    LLVMTypeRef sct = LLVMFunctionType(i8p, (LLVMTypeRef[]){i8p, i8p}, 2, 0);
    LLVMValueRef scf = LLVMGetNamedFunction(ctx->module, "strcat");
    if (!scf)
      scf = LLVMAddFunction(ctx->module, "strcat", sct);

    LLVMValueRef a1[] = {res, l};
    LLVMBuildCall2(ctx->builder, sct, scf, a1, 2, "");
    LLVMValueRef a2[] = {res, r};
    LLVMBuildCall2(ctx->builder, sct, scf, a2, 2, "");

    return res;
  }

  LLVMValueRef l = codegen_expr(ctx, expr->binary.left);
  LLVMValueRef r = codegen_expr(ctx, expr->binary.right);

  if (strcmp(op, "+") == 0)
    return LLVMBuildAdd(ctx->builder, l, r, "a");
  if (strcmp(op, "-") == 0)
    return LLVMBuildSub(ctx->builder, l, r, "s");
  if (strcmp(op, "*") == 0)
    return LLVMBuildMul(ctx->builder, l, r, "m");
  if (strcmp(op, "/") == 0)
    return LLVMBuildSDiv(ctx->builder, l, r, "d");
  if (strcmp(op, "%") == 0)
    return LLVMBuildSRem(ctx->builder, l, r, "mod");
  if (strcmp(op, "==") == 0)
    return LLVMBuildICmp(ctx->builder, LLVMIntEQ, l, r, "e");
  if (strcmp(op, "!=") == 0)
    return LLVMBuildICmp(ctx->builder, LLVMIntNE, l, r, "ne");
  if (strcmp(op, "<") == 0)
    return LLVMBuildICmp(ctx->builder, LLVMIntSLT, l, r, "lt");
  if (strcmp(op, "<=") == 0)
    return LLVMBuildICmp(ctx->builder, LLVMIntSLE, l, r, "le");
  if (strcmp(op, ">") == 0)
    return LLVMBuildICmp(ctx->builder, LLVMIntSGT, l, r, "gt");
  if (strcmp(op, ">=") == 0)
    return LLVMBuildICmp(ctx->builder, LLVMIntSGE, l, r, "ge");
  if (strcmp(op, "and") == 0)
    return LLVMBuildAnd(ctx->builder, l, r, "and");
  if (strcmp(op, "or") == 0)
    return LLVMBuildOr(ctx->builder, l, r, "or");

  codegen_error(ctx, "Unknown operator '%s'", op);
  return l;
}

LLVMValueRef codegen_unary_op(CodeGenContext *ctx, ASTNode *expr) {
  LLVMValueRef op = codegen_expr(ctx, expr->unary.operand);
  const char *o = expr->unary.op;
  if (!o)
    return op;
  if (strcmp(o, "-") == 0)
    return LLVMBuildNeg(ctx->builder, op, "n");
  if (strcmp(o, "not") == 0 || strcmp(o, "!") == 0)
    return LLVMBuildNot(ctx->builder, op, "not");
  return op;
}

static LLVMValueRef builtin_min(CodeGenContext *ctx, ASTNode *c) {
  LLVMValueRef a = codegen_expr(ctx, c->call.args[0]);
  LLVMValueRef b = codegen_expr(ctx, c->call.args[1]);
  LLVMValueRef cond = LLVMBuildICmp(ctx->builder, LLVMIntSLT, a, b, "c");
  return LLVMBuildSelect(ctx->builder, cond, a, b, "min");
}

static LLVMValueRef builtin_max(CodeGenContext *ctx, ASTNode *c) {
  LLVMValueRef a = codegen_expr(ctx, c->call.args[0]);
  LLVMValueRef b = codegen_expr(ctx, c->call.args[1]);
  LLVMValueRef cond = LLVMBuildICmp(ctx->builder, LLVMIntSGT, a, b, "c");
  return LLVMBuildSelect(ctx->builder, cond, a, b, "max");
}

static LLVMValueRef builtin_abs(CodeGenContext *ctx, ASTNode *c) {
  LLVMValueRef v = codegen_expr(ctx, c->call.args[0]);
  LLVMValueRef z = LLVMConstInt(LLVMInt64TypeInContext(ctx->llvm_ctx), 0, 0);
  LLVMValueRef n = LLVMBuildNeg(ctx->builder, v, "n");
  LLVMValueRef cond = LLVMBuildICmp(ctx->builder, LLVMIntSLT, v, z, "c");
  return LLVMBuildSelect(ctx->builder, cond, n, v, "abs");
}

LLVMValueRef codegen_asm(CodeGenContext *ctx, ASTNode *expr) {
  if (!expr || !expr->asm_block.code)
    return NULL;
  LLVMTypeRef ft =
      LLVMFunctionType(LLVMVoidTypeInContext(ctx->llvm_ctx), NULL, 0, 0);
  LLVMValueRef ia =
      LLVMConstInlineAsm(ft, expr->asm_block.code, "", true, false);
  return LLVMBuildCall2(ctx->builder, ft, ia, NULL, 0, "asm");
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

  if (strcmp(name, "print") == 0) {
    for (int i = 0; i < expr->call.arg_count; i++) {
      ASTNode *arg = expr->call.args[i];
      LLVMTypeRef i8p =
          LLVMPointerType(LLVMInt8TypeInContext(ctx->llvm_ctx), 0);
      LLVMTypeRef i32t = LLVMInt32TypeInContext(ctx->llvm_ctx);
      LLVMTypeRef i64t = LLVMInt64TypeInContext(ctx->llvm_ctx);

      if (arg->type == NODE_BINARY_OP && arg->binary.op &&
          strcmp(arg->binary.op, "[]") == 0) {
        LLVMValueRef val = codegen_expr(ctx, arg);

        LLVMValueRef is_str = LLVMBuildICmp(ctx->builder, LLVMIntSLT, val,
                                            LLVMConstInt(i64t, 0, 0), "is_str");
        LLVMBasicBlockRef sb = LLVMAppendBasicBlockInContext(
            ctx->llvm_ctx, ctx->current_func.function, "sb");
        LLVMBasicBlockRef ib = LLVMAppendBasicBlockInContext(
            ctx->llvm_ctx, ctx->current_func.function, "ib");
        LLVMBasicBlockRef mb = LLVMAppendBasicBlockInContext(
            ctx->llvm_ctx, ctx->current_func.function, "mb");
        LLVMBuildCondBr(ctx->builder, is_str, sb, ib);

        LLVMPositionBuilderAtEnd(ctx->builder, sb);
        LLVMValueRef mask = LLVMConstInt(i64t, 0x7FFFFFFFFFFFFFFFULL, 0);
        LLVMValueRef ptr_val = LLVMBuildAnd(ctx->builder, val, mask, "clear");
        LLVMValueRef str_ptr =
            LLVMBuildIntToPtr(ctx->builder, ptr_val, i8p, "sp");
        LLVMTypeRef putst = LLVMFunctionType(i32t, (LLVMTypeRef[]){i8p}, 1, 0);
        LLVMValueRef putf = LLVMGetNamedFunction(ctx->module, "puts");
        if (!putf)
          putf = LLVMAddFunction(ctx->module, "puts", putst);
        LLVMValueRef sa[] = {str_ptr};
        LLVMBuildCall2(ctx->builder, putst, putf, sa, 1, "p");
        LLVMBuildBr(ctx->builder, mb);

        LLVMPositionBuilderAtEnd(ctx->builder, ib);
        LLVMTypeRef pt =
            LLVMFunctionType(i32t, (LLVMTypeRef[]){i8p, i64t}, 2, 0);
        LLVMValueRef pf = LLVMGetNamedFunction(ctx->module, "printf");
        if (!pf)
          pf = LLVMAddFunction(ctx->module, "printf", pt);
        LLVMBasicBlockRef saved = LLVMGetInsertBlock(ctx->builder);
        LLVMValueRef fmt =
            LLVMBuildGlobalStringPtr(ctx->builder, "%lld\n", "fmt");
        if (saved)
          LLVMPositionBuilderAtEnd(ctx->builder, saved);
        LLVMValueRef ia[] = {fmt, val};
        LLVMBuildCall2(ctx->builder, pt, pf, ia, 2, "p");
        LLVMBuildBr(ctx->builder, mb);

        LLVMPositionBuilderAtEnd(ctx->builder, mb);
        continue;
      }

      LLVMValueRef val = codegen_expr(ctx, arg);
      LLVMTypeRef val_type = LLVMTypeOf(val);
      LLVMTypeKind val_kind = LLVMGetTypeKind(val_type);

      if (val_kind == LLVMPointerTypeKind) {
        LLVMTypeRef putst = LLVMFunctionType(i32t, (LLVMTypeRef[]){i8p}, 1, 0);
        LLVMValueRef putf = LLVMGetNamedFunction(ctx->module, "puts");
        if (!putf)
          putf = LLVMAddFunction(ctx->module, "puts", putst);
        LLVMValueRef args[] = {val};
        LLVMBuildCall2(ctx->builder, putst, putf, args, 1, "print");
      } else {
        LLVMTypeRef pt =
            LLVMFunctionType(i32t, (LLVMTypeRef[]){i8p, i64t}, 2, 0);
        LLVMValueRef pf = LLVMGetNamedFunction(ctx->module, "printf");
        if (!pf)
          pf = LLVMAddFunction(ctx->module, "printf", pt);
        LLVMBasicBlockRef saved = LLVMGetInsertBlock(ctx->builder);
        LLVMValueRef fmt =
            LLVMBuildGlobalStringPtr(ctx->builder, "%lld\n", "fmt");
        if (saved)
          LLVMPositionBuilderAtEnd(ctx->builder, saved);
        LLVMValueRef args[] = {fmt, val};
        LLVMBuildCall2(ctx->builder, pt, pf, args, 2, "print");
      }
    }
    return LLVMConstInt(LLVMInt64TypeInContext(ctx->llvm_ctx), 0, 0);
  }

  LLVMValueRef func = NULL;
  LLVMTypeRef ft = NULL;
  for (int i = 0; i < ctx->functions.count; i++) {
    if (strcmp(ctx->functions.names[i], name) == 0) {
      func = ctx->functions.functions[i];
      ft = ctx->functions.types[i];
      break;
    }
  }

  if (!func) {
    if (bt == -1)
      codegen_error(ctx, "Undefined function '%s'", name);
    return LLVMConstInt(LLVMInt64TypeInContext(ctx->llvm_ctx), 0, 0);
  }

  LLVMTypeRef call_type = ft;
  if (LLVMGetTypeKind(ft) == LLVMPointerTypeKind)
    call_type = LLVMGetElementType(ft);

  LLVMValueRef *args = malloc(sizeof(LLVMValueRef) * expr->call.arg_count);
  for (int i = 0; i < expr->call.arg_count; i++)
    args[i] = codegen_expr(ctx, expr->call.args[i]);

  LLVMValueRef result = LLVMBuildCall2(ctx->builder, call_type, func, args,
                                       expr->call.arg_count, "call");
  free(args);
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
    return codegen_asm(ctx, expr);
  case NODE_NIL_LITERAL:
    return LLVMConstNull(
        LLVMPointerType(LLVMInt8TypeInContext(ctx->llvm_ctx), 0));
  default:
    codegen_error(ctx, "Unknown expr type %d", expr->type);
    return LLVMConstInt(LLVMInt64TypeInContext(ctx->llvm_ctx), 0, 0);
  }
}
