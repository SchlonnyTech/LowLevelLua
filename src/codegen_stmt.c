#include "codegen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void codegen_block(CodeGenContext *ctx, ASTNode *block) {
  codegen_scope_push(ctx);
  for (int i = 0; i < block->block.statement_count; i++)
    codegen_stmt(ctx, block->block.statements[i]);
  codegen_scope_pop(ctx);
}

void codegen_return(CodeGenContext *ctx, ASTNode *stmt) {
  if (stmt->return_stmt.expr) {
    LLVMValueRef v = codegen_expr(ctx, stmt->return_stmt.expr);
    LLVMBuildRet(ctx->builder, v);
  } else {
    LLVMBuildRetVoid(ctx->builder);
  }
  ctx->current_func.has_return = true;
}

void codegen_if(CodeGenContext *ctx, ASTNode *stmt) {
  LLVMValueRef c = codegen_expr(ctx, stmt->if_stmt.condition);
  LLVMBasicBlockRef tb = LLVMAppendBasicBlockInContext(
      ctx->llvm_ctx, ctx->current_func.function, "then");
  LLVMBasicBlockRef eb =
      stmt->if_stmt.else_branch
          ? LLVMAppendBasicBlockInContext(ctx->llvm_ctx,
                                          ctx->current_func.function, "else")
          : NULL;
  LLVMBasicBlockRef mb = LLVMAppendBasicBlockInContext(
      ctx->llvm_ctx, ctx->current_func.function, "merge");
  if (eb)
    LLVMBuildCondBr(ctx->builder, c, tb, eb);
  else
    LLVMBuildCondBr(ctx->builder, c, tb, mb);
  LLVMPositionBuilderAtEnd(ctx->builder, tb);
  codegen_stmt(ctx, stmt->if_stmt.then_branch);
  if (!ctx->current_func.has_return)
    LLVMBuildBr(ctx->builder, mb);
  if (eb) {
    LLVMPositionBuilderAtEnd(ctx->builder, eb);
    codegen_stmt(ctx, stmt->if_stmt.else_branch);
    if (!ctx->current_func.has_return)
      LLVMBuildBr(ctx->builder, mb);
  }
  if (!ctx->current_func.has_return)
    LLVMPositionBuilderAtEnd(ctx->builder, mb);
}

void codegen_while(CodeGenContext *ctx, ASTNode *stmt) {
  LLVMBasicBlockRef cb = LLVMAppendBasicBlockInContext(
      ctx->llvm_ctx, ctx->current_func.function, "wc");
  LLVMBasicBlockRef bb = LLVMAppendBasicBlockInContext(
      ctx->llvm_ctx, ctx->current_func.function, "wb");
  LLVMBasicBlockRef mb = LLVMAppendBasicBlockInContext(
      ctx->llvm_ctx, ctx->current_func.function, "wm");
  LLVMBuildBr(ctx->builder, cb);
  LLVMPositionBuilderAtEnd(ctx->builder, cb);
  LLVMValueRef c = codegen_expr(ctx, stmt->while_stmt.condition);
  LLVMBuildCondBr(ctx->builder, c, bb, mb);
  LLVMPositionBuilderAtEnd(ctx->builder, bb);
  codegen_loop_push(ctx, cb, mb);
  codegen_stmt(ctx, stmt->while_stmt.body);
  codegen_loop_pop(ctx);
  LLVMBuildBr(ctx->builder, cb);
  LLVMPositionBuilderAtEnd(ctx->builder, mb);
}

void codegen_repeat(CodeGenContext *ctx, ASTNode *stmt) {
  LLVMBasicBlockRef bb = LLVMAppendBasicBlockInContext(
      ctx->llvm_ctx, ctx->current_func.function, "rb");
  LLVMBasicBlockRef cb = LLVMAppendBasicBlockInContext(
      ctx->llvm_ctx, ctx->current_func.function, "rc");
  LLVMBasicBlockRef mb = LLVMAppendBasicBlockInContext(
      ctx->llvm_ctx, ctx->current_func.function, "rm");
  LLVMBuildBr(ctx->builder, bb);
  LLVMPositionBuilderAtEnd(ctx->builder, bb);
  codegen_loop_push(ctx, cb, mb);
  codegen_stmt(ctx, stmt->repeat_stmt.body);
  codegen_loop_pop(ctx);
  LLVMBuildBr(ctx->builder, cb);
  LLVMPositionBuilderAtEnd(ctx->builder, cb);
  LLVMValueRef c = codegen_expr(ctx, stmt->repeat_stmt.condition);
  LLVMBuildCondBr(ctx->builder, c, mb, bb);
  LLVMPositionBuilderAtEnd(ctx->builder, mb);
}

void codegen_for(CodeGenContext *ctx, ASTNode *stmt) {
  LLVMValueRef sv = codegen_expr(ctx, stmt->for_stmt.start);
  LLVMValueRef ev = codegen_expr(ctx, stmt->for_stmt.end);
  LLVMValueRef step =
      stmt->for_stmt.step
          ? codegen_expr(ctx, stmt->for_stmt.step)
          : LLVMConstInt(LLVMInt64TypeInContext(ctx->llvm_ctx), 1, 0);
  LLVMTypeRef vt = LLVMTypeOf(sv);
  LLVMValueRef va = LLVMBuildAlloca(ctx->builder, vt, stmt->for_stmt.var);
  LLVMBuildStore(ctx->builder, sv, va);
  codegen_scope_add(ctx, stmt->for_stmt.var, va, vt);
  LLVMBasicBlockRef cb = LLVMAppendBasicBlockInContext(
      ctx->llvm_ctx, ctx->current_func.function, "fc");
  LLVMBasicBlockRef bb = LLVMAppendBasicBlockInContext(
      ctx->llvm_ctx, ctx->current_func.function, "fb");
  LLVMBasicBlockRef ib = LLVMAppendBasicBlockInContext(
      ctx->llvm_ctx, ctx->current_func.function, "fi");
  LLVMBasicBlockRef mb = LLVMAppendBasicBlockInContext(
      ctx->llvm_ctx, ctx->current_func.function, "fm");
  LLVMBuildBr(ctx->builder, cb);
  LLVMPositionBuilderAtEnd(ctx->builder, cb);
  LLVMValueRef vv = LLVMBuildLoad2(ctx->builder, vt, va, "v");
  LLVMValueRef c = LLVMBuildICmp(ctx->builder, LLVMIntSLE, vv, ev, "c");
  LLVMBuildCondBr(ctx->builder, c, bb, mb);
  LLVMPositionBuilderAtEnd(ctx->builder, bb);
  codegen_loop_push(ctx, ib, mb);
  codegen_stmt(ctx, stmt->for_stmt.body);
  codegen_loop_pop(ctx);
  LLVMBuildBr(ctx->builder, ib);
  LLVMPositionBuilderAtEnd(ctx->builder, ib);
  LLVMValueRef cv = LLVMBuildLoad2(ctx->builder, vt, va, "cv");
  LLVMValueRef nv = LLVMBuildAdd(ctx->builder, cv, step, "nv");
  LLVMBuildStore(ctx->builder, nv, va);
  LLVMBuildBr(ctx->builder, cb);
  LLVMPositionBuilderAtEnd(ctx->builder, mb);
}

void codegen_local_var(CodeGenContext *ctx, ASTNode *stmt) {
  if (stmt->local_var.init && stmt->local_var.init->type == NODE_TABLE) {
    int count = stmt->local_var.init->table.field_count;
    LLVMTypeRef i64t = LLVMInt64TypeInContext(ctx->llvm_ctx);
    LLVMTypeRef arr_t = LLVMArrayType(i64t, count);
    LLVMValueRef va = LLVMBuildArrayAlloca(
        ctx->builder, i64t, LLVMConstInt(i64t, count, 0), stmt->local_var.name);
    codegen_scope_add(ctx, stmt->local_var.name, va, arr_t);

    for (int i = 0; i < count; i++) {
      ASTNode *field = stmt->local_var.init->table.fields[i];
      LLVMValueRef v;
      if (field->type == NODE_STRING_LITERAL) {
        v = LLVMBuildPtrToInt(ctx->builder,
                              LLVMBuildGlobalStringPtr(
                                  ctx->builder, field->string_lit.value, "s"),
                              i64t, "str");
        v = LLVMBuildOr(ctx->builder, v, LLVMConstInt(i64t, 1ULL << 63, 0),
                        "tag");
      } else if (field->type == NODE_INT_LITERAL) {
        v = LLVMConstInt(i64t, field->int_lit.value, 0);
      } else if (field->type == NODE_VARIABLE) {
        LLVMTypeRef ft = codegen_scope_get_type(ctx, field->variable.name);
        if (ft && LLVMGetTypeKind(ft) == LLVMPointerTypeKind) {
          v = LLVMBuildPtrToInt(ctx->builder, codegen_variable(ctx, field),
                                i64t, "str");
          v = LLVMBuildOr(ctx->builder, v, LLVMConstInt(i64t, 1ULL << 63, 0),
                          "tag");
        } else {
          v = codegen_variable(ctx, field);
        }
      } else {
        v = LLVMConstInt(i64t, 0, 0);
      }
      LLVMValueRef ep =
          LLVMBuildGEP2(ctx->builder, i64t, va,
                        (LLVMValueRef[]){LLVMConstInt(i64t, i, 0)}, 1, "e");
      LLVMBuildStore(ctx->builder, v, ep);
    }
    return;
  }

  LLVMTypeRef vt;
  if (stmt->local_var.type) {
    vt = codegen_type_from_node(ctx, stmt->local_var.type);
  } else if (stmt->local_var.init) {
    vt = codegen_infer_type(ctx, stmt->local_var.init);
  } else {
    vt = LLVMInt64TypeInContext(ctx->llvm_ctx);
  }
  LLVMValueRef va = LLVMBuildAlloca(ctx->builder, vt, stmt->local_var.name);
  if (stmt->local_var.init) {
    LLVMValueRef iv = codegen_expr(ctx, stmt->local_var.init);
    LLVMBuildStore(ctx->builder, iv, va);
  } else {
    LLVMBuildStore(ctx->builder, LLVMConstNull(vt), va);
  }
  codegen_scope_add(ctx, stmt->local_var.name, va, vt);
}

void codegen_assign(CodeGenContext *ctx, ASTNode *stmt) {
  if (stmt->assign.target->type == NODE_VARIABLE) {
    LLVMValueRef target =
        codegen_scope_get(ctx, stmt->assign.target->variable.name);
    if (!target) {
      codegen_error(ctx, "Undefined variable '%s'",
                    stmt->assign.target->variable.name);
      return;
    }
    LLVMValueRef value = codegen_expr(ctx, stmt->assign.value);
    LLVMBuildStore(ctx->builder, value, target);
  }
}

void codegen_defer(CodeGenContext *ctx, ASTNode *stmt) {
  if (stmt->defer_stmt.expr) {
    codegen_expr(ctx, stmt->defer_stmt.expr);
  }
}

void codegen_function(CodeGenContext *ctx, ASTNode *func) {
  LLVMTypeRef rt = func->func.return_type
                       ? codegen_type_from_node(ctx, func->func.return_type)
                       : LLVMVoidTypeInContext(ctx->llvm_ctx);
  LLVMTypeRef *pts = malloc(sizeof(LLVMTypeRef) * func->func.param_count);
  for (int i = 0; i < func->func.param_count; i++) {
    if (func->func.param_types && func->func.param_types[i])
      pts[i] = codegen_type_from_node(ctx, func->func.param_types[i]);
    else
      pts[i] = LLVMInt64TypeInContext(ctx->llvm_ctx);
  }
  LLVMTypeRef ft = LLVMFunctionType(rt, pts, func->func.param_count, 0);
  LLVMValueRef fn = LLVMAddFunction(ctx->module, func->func.name, ft);
  LLVMSetLinkage(fn, func->func.is_exported ? LLVMExternalLinkage
                                            : LLVMInternalLinkage);

  if (ctx->functions.count >= ctx->functions.capacity) {
    ctx->functions.capacity =
        ctx->functions.capacity ? ctx->functions.capacity * 2 : 16;
    ctx->functions.names =
        realloc(ctx->functions.names, ctx->functions.capacity * sizeof(char *));
    ctx->functions.functions =
        realloc(ctx->functions.functions,
                ctx->functions.capacity * sizeof(LLVMValueRef));
    ctx->functions.types = realloc(
        ctx->functions.types, ctx->functions.capacity * sizeof(LLVMTypeRef));
    ctx->functions.builtin_types = realloc(
        ctx->functions.builtin_types, ctx->functions.capacity * sizeof(int));
    ctx->functions.arg_counts = realloc(ctx->functions.arg_counts,
                                        ctx->functions.capacity * sizeof(int));
  }
  ctx->functions.names[ctx->functions.count] = strdup(func->func.name);
  ctx->functions.functions[ctx->functions.count] = fn;
  ctx->functions.types[ctx->functions.count] = ft;
  ctx->functions.builtin_types[ctx->functions.count] = -1;
  ctx->functions.arg_counts[ctx->functions.count] = func->func.param_count;
  ctx->functions.count++;

  ctx->current_func.function = fn;
  ctx->current_func.return_type = rt;
  ctx->current_func.has_return = false;
  ctx->current_func.entry_block =
      LLVMAppendBasicBlockInContext(ctx->llvm_ctx, fn, "entry");
  LLVMPositionBuilderAtEnd(ctx->builder, ctx->current_func.entry_block);
  codegen_scope_push(ctx);
  for (int i = 0; i < func->func.param_count; i++) {
    LLVMValueRef p = LLVMGetParam(fn, i);
    LLVMValueRef al = LLVMBuildAlloca(ctx->builder, pts[i],
                                      func->func.params[i]->variable.name);
    LLVMBuildStore(ctx->builder, p, al);
    codegen_scope_add(ctx, func->func.params[i]->variable.name, al, pts[i]);
  }
  codegen_stmt(ctx, func->func.body);
  if (!ctx->current_func.has_return) {
    if (rt == LLVMVoidTypeInContext(ctx->llvm_ctx))
      LLVMBuildRetVoid(ctx->builder);
    else
      LLVMBuildRet(ctx->builder, LLVMConstNull(rt));
  }
  codegen_scope_pop(ctx);
  ctx->functions_generated++;
  free(pts);
}

void codegen_struct(CodeGenContext *ctx, ASTNode *sd) {
  LLVMTypeRef *fts = malloc(sizeof(LLVMTypeRef) * sd->struct_def.field_count);
  for (int i = 0; i < sd->struct_def.field_count; i++)
    fts[i] = codegen_type_from_node(ctx, sd->struct_def.fields[i]);
  LLVMTypeRef st = LLVMStructCreateNamed(ctx->llvm_ctx, sd->struct_def.name);
  LLVMStructSetBody(st, fts, sd->struct_def.field_count, 0);
  if (ctx->struct_types.count >= ctx->struct_types.capacity) {
    ctx->struct_types.capacity =
        ctx->struct_types.capacity ? ctx->struct_types.capacity * 2 : 16;
    ctx->struct_types.names = realloc(
        ctx->struct_types.names, ctx->struct_types.capacity * sizeof(char *));
    ctx->struct_types.types =
        realloc(ctx->struct_types.types,
                ctx->struct_types.capacity * sizeof(LLVMTypeRef));
    ctx->struct_types.field_counts =
        realloc(ctx->struct_types.field_counts,
                ctx->struct_types.capacity * sizeof(int));
  }
  ctx->struct_types.names[ctx->struct_types.count] =
      strdup(sd->struct_def.name);
  ctx->struct_types.types[ctx->struct_types.count] = st;
  ctx->struct_types.field_counts[ctx->struct_types.count] =
      sd->struct_def.field_count;
  ctx->struct_types.count++;
  free(fts);
}

void codegen_enum(CodeGenContext *ctx, ASTNode *ed) {
  if (!ed || !ed->enum_def.name)
    return;
  for (int i = 0; i < ed->enum_def.value_count; i++) {
    if (!ed->enum_def.values || !ed->enum_def.values[i])
      continue;
    char *nm =
        malloc(strlen(ed->enum_def.name) + strlen(ed->enum_def.values[i]) + 2);
    sprintf(nm, "%s_%s", ed->enum_def.name, ed->enum_def.values[i]);
    int64_t v = i;
    if (ed->enum_def.value_exprs && ed->enum_def.value_exprs[i] &&
        ed->enum_def.value_exprs[i]->type == NODE_INT_LITERAL)
      v = ed->enum_def.value_exprs[i]->int_lit.value;
    LLVMValueRef g =
        LLVMAddGlobal(ctx->module, LLVMInt64TypeInContext(ctx->llvm_ctx), nm);
    LLVMSetInitializer(
        g, LLVMConstInt(LLVMInt64TypeInContext(ctx->llvm_ctx), v, 0));
    LLVMSetLinkage(g, LLVMInternalLinkage);
    LLVMSetGlobalConstant(g, true);
    free(nm);
  }
}

void codegen_stmt(CodeGenContext *ctx, ASTNode *stmt) {
  if (!stmt)
    return;
  switch (stmt->type) {
  case NODE_BLOCK:
    codegen_block(ctx, stmt);
    break;
  case NODE_RETURN:
    codegen_return(ctx, stmt);
    break;
  case NODE_IF:
    codegen_if(ctx, stmt);
    break;
  case NODE_WHILE:
    codegen_while(ctx, stmt);
    break;
  case NODE_REPEAT:
    codegen_repeat(ctx, stmt);
    break;
  case NODE_FOR:
    codegen_for(ctx, stmt);
    break;
  case NODE_LOCAL_VAR:
    codegen_local_var(ctx, stmt);
    break;
  case NODE_ASSIGN:
    codegen_assign(ctx, stmt);
    break;
  case NODE_CALL:
    codegen_expr(ctx, stmt);
    break;
  case NODE_DEFER:
    codegen_defer(ctx, stmt);
    break;
  case NODE_ASM_BLOCK:
    codegen_asm(ctx, stmt);
    break;
  case NODE_BREAK: {
    LLVMBasicBlockRef b = codegen_get_break_block(ctx);
    if (b)
      LLVMBuildBr(ctx->builder, b);
    break;
  }
  case NODE_CONTINUE: {
    LLVMBasicBlockRef c = codegen_get_continue_block(ctx);
    if (c)
      LLVMBuildBr(ctx->builder, c);
    break;
  }
  default:
    break;
  }
}

bool codegen_generate(CodeGenContext *ctx, ASTNode *program) {
  if (!program || program->type != NODE_PROGRAM) {
    codegen_error(ctx, "Invalid program AST");
    return false;
  }
  codegen_scope_push(ctx);
  ctx->global_scope = ctx->current_scope;
  for (int i = 0; i < program->block.statement_count; i++) {
    ASTNode *n = program->block.statements[i];
    if (!n)
      continue;
    if (n->type == NODE_STRUCT)
      codegen_struct(ctx, n);
    else if (n->type == NODE_ENUM)
      codegen_enum(ctx, n);
  }
  for (int i = 0; i < program->block.statement_count; i++) {
    ASTNode *n = program->block.statements[i];
    if (!n)
      continue;
    if (n->type == NODE_FUNCTION)
      codegen_function(ctx, n);
  }
  LLVMClearInsertionPosition(ctx->builder);
  bool has_main = false;
  for (int i = 0; i < ctx->functions.count; i++) {
    if (strcmp(ctx->functions.names[i], "main") == 0) {
      has_main = true;
      break;
    }
  }
  if (!has_main) {
    LLVMTypeRef mt =
        LLVMFunctionType(LLVMInt32TypeInContext(ctx->llvm_ctx), NULL, 0, 0);
    LLVMValueRef mf = LLVMAddFunction(ctx->module, "main", mt);
    LLVMBasicBlockRef entry =
        LLVMAppendBasicBlockInContext(ctx->llvm_ctx, mf, "entry");
    LLVMPositionBuilderAtEnd(ctx->builder, entry);

    ctx->int_format =
        LLVMBuildGlobalStringPtr(ctx->builder, "%lld\n", "int_fmt");
    ctx->str_format = LLVMBuildGlobalStringPtr(ctx->builder, "%s\n", "str_fmt");

    ctx->current_func.function = mf;
    ctx->current_func.return_type = LLVMInt32TypeInContext(ctx->llvm_ctx);
    ctx->current_func.has_return = false;

    for (int i = 0; i < program->block.statement_count; i++) {
      ASTNode *n = program->block.statements[i];
      if (!n)
        continue;
      if (n->type != NODE_STRUCT && n->type != NODE_ENUM &&
          n->type != NODE_FUNCTION)
        codegen_stmt(ctx, n);
    }

    if (!ctx->current_func.has_return)
      LLVMBuildRet(ctx->builder,
                   LLVMConstInt(LLVMInt32TypeInContext(ctx->llvm_ctx), 0, 0));
  }
  if (ctx->verbose) {
    fprintf(stderr, "\n=== LLVM IR ===\n");
    LLVMDumpModule(ctx->module);
    fprintf(stderr, "=== End LLVM IR ===\n\n");
  }
  return !ctx->has_error;
}
