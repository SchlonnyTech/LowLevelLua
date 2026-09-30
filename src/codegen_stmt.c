#include "codegen.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEBUG_STMT 0
#define DPRINTF_STMT(fmt, ...)                                                 \
  if (DEBUG_STMT)                                                              \
  fprintf(stderr, "[STMT] " fmt, ##__VA_ARGS__)

static LLVMValueRef sext_or_trunc(LLVMBuilderRef b, LLVMValueRef v,
                                  LLVMTypeRef dst, const char *name) {
  unsigned sw = LLVMGetIntTypeWidth(LLVMTypeOf(v));
  unsigned dw = LLVMGetIntTypeWidth(dst);
  if (sw < dw)
    return LLVMBuildSExt(b, v, dst, name);
  if (sw > dw)
    return LLVMBuildTrunc(b, v, dst, name);
  return v;
}

static int block_terminated(CodeGenContext *ctx) {
  LLVMBasicBlockRef bb = LLVMGetInsertBlock(ctx->builder);
  if (!bb)
    return 1;
  return LLVMGetBasicBlockTerminator(bb) != NULL;
}

void codegen_block(CodeGenContext *ctx, ASTNode *block) {
  DPRINTF_STMT("block enter: %d statements\n", block->block.statement_count);
  codegen_scope_push(ctx);
  for (int i = 0; i < block->block.statement_count; i++) {
    if (block_terminated(ctx)) {
      DPRINTF_STMT("block: terminated early at stmt %d/%d\n", i,
                   block->block.statement_count);
      break;
    }
    DPRINTF_STMT("block stmt %d/%d kind=%d\n", i, block->block.statement_count,
                 block->block.statements[i] ? block->block.statements[i]->type
                                            : -1);
    codegen_stmt(ctx, block->block.statements[i]);
  }
  codegen_scope_pop(ctx);
  DPRINTF_STMT("block exit\n");
}

void codegen_return(CodeGenContext *ctx, ASTNode *stmt) {
  DPRINTF_STMT("return has_expr=%d\n", stmt->return_stmt.expr ? 1 : 0);
  if (stmt->return_stmt.expr) {
    LLVMValueRef v = codegen_expr(ctx, stmt->return_stmt.expr);
    DPRINTF_STMT("return value kind=%d\n",
                 v ? (int)LLVMGetTypeKind(LLVMTypeOf(v)) : -1);
    LLVMBuildRet(ctx->builder, v);
  } else {
    LLVMBuildRetVoid(ctx->builder);
  }
  ctx->current_func.has_return = true;
}

void codegen_if(CodeGenContext *ctx, ASTNode *stmt) {
  DPRINTF_STMT("if has_else=%d\n", stmt->if_stmt.else_branch ? 1 : 0);
  LLVMValueRef c = codegen_expr(ctx, stmt->if_stmt.condition);
  DPRINTF_STMT("if cond kind=%d\n", (int)LLVMGetTypeKind(LLVMTypeOf(c)));
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
  int then_falls = !block_terminated(ctx);
  if (then_falls)
    LLVMBuildBr(ctx->builder, mb);

  int else_falls = 0;
  if (eb) {
    LLVMPositionBuilderAtEnd(ctx->builder, eb);
    codegen_stmt(ctx, stmt->if_stmt.else_branch);
    else_falls = !block_terminated(ctx);
    if (else_falls)
      LLVMBuildBr(ctx->builder, mb);
  } else {
    else_falls = 1;
  }

  DPRINTF_STMT("if then_falls=%d else_falls=%d\n", then_falls, else_falls);
  if (then_falls || else_falls)
    LLVMPositionBuilderAtEnd(ctx->builder, mb);
}

void codegen_while(CodeGenContext *ctx, ASTNode *stmt) {
  DPRINTF_STMT("while\n");
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
  if (!block_terminated(ctx))
    LLVMBuildBr(ctx->builder, cb);
  LLVMPositionBuilderAtEnd(ctx->builder, mb);
}

void codegen_repeat(CodeGenContext *ctx, ASTNode *stmt) {
  DPRINTF_STMT("repeat\n");
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
  if (!block_terminated(ctx))
    LLVMBuildBr(ctx->builder, cb);
  LLVMPositionBuilderAtEnd(ctx->builder, cb);
  LLVMValueRef c = codegen_expr(ctx, stmt->repeat_stmt.condition);
  LLVMBuildCondBr(ctx->builder, c, mb, bb);
  LLVMPositionBuilderAtEnd(ctx->builder, mb);
}

void codegen_for(CodeGenContext *ctx, ASTNode *stmt) {
  DPRINTF_STMT("for var=%s has_step=%d\n", stmt->for_stmt.var,
               stmt->for_stmt.step ? 1 : 0);
  LLVMValueRef sv = codegen_expr(ctx, stmt->for_stmt.start);
  LLVMValueRef ev = codegen_expr(ctx, stmt->for_stmt.end);
  LLVMValueRef step =
      stmt->for_stmt.step
          ? codegen_expr(ctx, stmt->for_stmt.step)
          : LLVMConstInt(LLVMInt64TypeInContext(ctx->llvm_ctx), 1, 0);
  LLVMTypeRef vt = LLVMTypeOf(sv);
  DPRINTF_STMT("for value kind=%d\n", (int)LLVMGetTypeKind(vt));
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
  if (!block_terminated(ctx))
    LLVMBuildBr(ctx->builder, ib);
  LLVMPositionBuilderAtEnd(ctx->builder, ib);
  LLVMValueRef cv = LLVMBuildLoad2(ctx->builder, vt, va, "cv");
  LLVMValueRef nv = LLVMBuildAdd(ctx->builder, cv, step, "nv");
  LLVMBuildStore(ctx->builder, nv, va);
  LLVMBuildBr(ctx->builder, cb);
  LLVMPositionBuilderAtEnd(ctx->builder, mb);
}

void codegen_local_var(CodeGenContext *ctx, ASTNode *stmt) {
  DPRINTF_STMT("local_var name=%s has_type=%d has_init=%d init_kind=%d\n",
               stmt->local_var.name, stmt->local_var.type ? 1 : 0,
               stmt->local_var.init ? 1 : 0,
               stmt->local_var.init ? stmt->local_var.init->type : -1);
  if (stmt->local_var.type) {
    DPRINTF_STMT("  type_node kind=%d name=%s depth=%d\n",
                 stmt->local_var.type->type,
                 stmt->local_var.type->type_annot.type_name
                     ? stmt->local_var.type->type_annot.type_name
                     : "(null)",
                 stmt->local_var.type->type_annot.pointer_depth);
  }

  if (stmt->local_var.init && stmt->local_var.init->type == NODE_TABLE) {
    DPRINTF_STMT("  table init, %d fields\n",
                 stmt->local_var.init->table.field_count);
    int count = stmt->local_var.init->table.field_count;
    LLVMTypeRef i64t = LLVMInt64TypeInContext(ctx->llvm_ctx);
    LLVMTypeRef arr_t = LLVMArrayType(i64t, count ? count : 1);
    LLVMValueRef va =
        LLVMBuildAlloca(ctx->builder, arr_t, stmt->local_var.name);
    codegen_scope_add(ctx, stmt->local_var.name, va, arr_t);

    for (int i = 0; i < count; i++) {
      ASTNode *field = stmt->local_var.init->table.fields[i];
      LLVMValueRef v;
      if (field->type == NODE_STRING_LITERAL) {
        v = LLVMBuildPtrToInt(ctx->builder, codegen_string_literal(ctx, field),
                              i64t, "str");
        v = LLVMBuildOr(ctx->builder, v, LLVMConstInt(i64t, 1ULL << 63, 0),
                        "tag");
      } else if (field->type == NODE_INT_LITERAL) {
        v = LLVMConstInt(i64t, field->int_lit.value, 0);
      } else if (field->type == NODE_VARIABLE) {
        ScopeEntry se;
        if (codegen_scope_lookup(ctx, field->variable.name, &se) &&
            LLVMGetTypeKind(se.type) == LLVMPointerTypeKind) {
          v = LLVMBuildPtrToInt(ctx->builder, codegen_variable(ctx, field),
                                i64t, "str");
          v = LLVMBuildOr(ctx->builder, v, LLVMConstInt(i64t, 1ULL << 63, 0),
                          "tag");
        } else {
          LLVMValueRef fv = codegen_expr(ctx, field);
          if (LLVMGetTypeKind(LLVMTypeOf(fv)) == LLVMPointerTypeKind)
            fv = LLVMBuildPtrToInt(ctx->builder, fv, i64t, "str");
          v = fv;
        }
      } else {
        LLVMValueRef fv = codegen_expr(ctx, field);
        LLVMTypeRef ft = LLVMTypeOf(fv);
        LLVMTypeKind fk = LLVMGetTypeKind(ft);
        if (fk == LLVMPointerTypeKind) {
          v = LLVMBuildPtrToInt(ctx->builder, fv, i64t, "str");
          v = LLVMBuildOr(ctx->builder, v, LLVMConstInt(i64t, 1ULL << 63, 0),
                          "tag");
        } else if (fk == LLVMDoubleTypeKind) {
          v = LLVMBuildFPToSI(ctx->builder, fv, i64t, "f2i");
        } else if (ft == i64t) {
          v = fv;
        } else {
          v = sext_or_trunc(ctx->builder, fv, i64t, "c");
        }
      }
      LLVMValueRef z = LLVMConstInt(i64t, 0, 0);
      LLVMValueRef idx = LLVMConstInt(i64t, i, 0);
      LLVMValueRef ep = LLVMBuildGEP2(ctx->builder, arr_t, va,
                                      (LLVMValueRef[]){z, idx}, 2, "e");
      LLVMBuildStore(ctx->builder, v, ep);
    }
    return;
  }

  LLVMTypeRef vt;
  if (stmt->local_var.type) {
    vt = codegen_type_from_node(ctx, stmt->local_var.type);
    DPRINTF_STMT("  vt from annotation kind=%d\n", (int)LLVMGetTypeKind(vt));
  } else if (stmt->local_var.init) {
    vt = codegen_infer_type(ctx, stmt->local_var.init);
    DPRINTF_STMT("  vt from inference kind=%d\n", (int)LLVMGetTypeKind(vt));
  } else {
    vt = LLVMInt64TypeInContext(ctx->llvm_ctx);
    DPRINTF_STMT("  vt default int\n");
  }

  LLVMValueRef va = LLVMBuildAlloca(ctx->builder, vt, stmt->local_var.name);

  if (stmt->local_var.init) {
    LLVMValueRef iv = codegen_expr(ctx, stmt->local_var.init);
    if (iv) {
      LLVMTypeRef ivt = LLVMTypeOf(iv);
      LLVMTypeKind ik = LLVMGetTypeKind(ivt);
      LLVMTypeKind vk = LLVMGetTypeKind(vt);
      DPRINTF_STMT("  init kind=%d slot kind=%d\n", (int)ik, (int)vk);

      if (ik == LLVMPointerTypeKind && vk == LLVMIntegerTypeKind) {
        DPRINTF_STMT("  coercion: slot was int, promoting to ptr\n");
        vt = LLVMPointerType(LLVMInt8TypeInContext(ctx->llvm_ctx), 0);
        va = LLVMBuildAlloca(ctx->builder, vt, stmt->local_var.name);
      } else if (ik == LLVMIntegerTypeKind && vk == LLVMPointerTypeKind) {
        DPRINTF_STMT("  coercion: init int -> ptr slot\n");
        iv = LLVMBuildIntToPtr(ctx->builder, iv, vt, "i2p");
      } else if (ik == LLVMDoubleTypeKind && vk == LLVMIntegerTypeKind) {
        DPRINTF_STMT("  coercion: init double -> int slot\n");
        iv = LLVMBuildFPToSI(ctx->builder, iv, vt, "f2i");
      } else if (ik == LLVMIntegerTypeKind && vk == LLVMDoubleTypeKind) {
        DPRINTF_STMT("  coercion: init int -> double slot\n");
        iv = LLVMBuildSIToFP(ctx->builder, iv, vt, "i2f");
      } else if (ik == LLVMIntegerTypeKind && vk == LLVMIntegerTypeKind &&
                 ivt != vt) {
        DPRINTF_STMT("  coercion: int width change\n");
        iv = sext_or_trunc(ctx->builder, iv, vt, "c");
      }

      LLVMBuildStore(ctx->builder, iv, va);
    } else {
      DPRINTF_STMT("  no init value, storing null\n");
      LLVMBuildStore(ctx->builder, LLVMConstNull(vt), va);
    }
  } else {
    LLVMBuildStore(ctx->builder, LLVMConstNull(vt), va);
  }

  DPRINTF_STMT("  scope_add name=%s kind=%d\n", stmt->local_var.name,
               (int)LLVMGetTypeKind(vt));
  codegen_scope_add(ctx, stmt->local_var.name, va, vt);
}

void codegen_assign(CodeGenContext *ctx, ASTNode *stmt) {
  ASTNode *target = stmt->assign.target;
  DPRINTF_STMT("assign target_kind=%d\n", target ? target->type : -1);
  if (target->type == NODE_VARIABLE) {
    DPRINTF_STMT("assign var name=%s\n", target->variable.name);
    ScopeEntry se;
    if (!codegen_scope_lookup(ctx, target->variable.name, &se)) {
      codegen_error(ctx, "Undefined variable '%s'", target->variable.name);
      return;
    }
    DPRINTF_STMT("  target slot kind=%d\n", (int)LLVMGetTypeKind(se.type));
    LLVMValueRef value = codegen_expr(ctx, stmt->assign.value);
    if (!value)
      return;
    DPRINTF_STMT("  value kind=%d\n", (int)LLVMGetTypeKind(LLVMTypeOf(value)));
    if (LLVMGetTypeKind(se.type) == LLVMPointerTypeKind &&
        LLVMGetTypeKind(LLVMTypeOf(value)) == LLVMIntegerTypeKind)
      value = LLVMBuildIntToPtr(
          ctx->builder, value,
          LLVMPointerType(LLVMInt8TypeInContext(ctx->llvm_ctx), 0), "i2p");
    LLVMBuildStore(ctx->builder, value, se.value);
    return;
  }

  if (target->type == NODE_FIELD_ACCESS) {
    ASTNode *object = target->field_access.object;
    const char *field_name = target->field_access.field;
    DPRINTF_STMT("assign field %s\n", field_name ? field_name : "(null)");
    if (!object || object->type != NODE_VARIABLE) {
      codegen_error(ctx, "Invalid struct field assignment");
      return;
    }
    ScopeEntry se;
    if (!codegen_scope_lookup(ctx, object->variable.name, &se)) {
      codegen_error(ctx, "Undefined variable '%s'", object->variable.name);
      return;
    }
    if (LLVMGetTypeKind(se.type) != LLVMStructTypeKind) {
      codegen_error(ctx, "Variable '%s' is not a struct",
                    object->variable.name);
      return;
    }
    int fi = codegen_struct_field_index(ctx, se.type, field_name);
    if (fi < 0) {
      codegen_error(ctx, "Unknown field '%s' in struct", field_name);
      return;
    }
    LLVMValueRef zero =
        LLVMConstInt(LLVMInt32TypeInContext(ctx->llvm_ctx), 0, 0);
    LLVMValueRef index =
        LLVMConstInt(LLVMInt32TypeInContext(ctx->llvm_ctx), fi, 0);
    LLVMValueRef indices[] = {zero, index};
    LLVMValueRef fp =
        LLVMBuildGEP2(ctx->builder, se.type, se.value, indices, 2, "field");
    LLVMValueRef value = codegen_expr(ctx, stmt->assign.value);
    if (!value)
      return;
    LLVMBuildStore(ctx->builder, value, fp);
    return;
  }

  codegen_error(ctx, "Invalid assignment target");
}

void codegen_defer(CodeGenContext *ctx, ASTNode *stmt) {
  DPRINTF_STMT("defer\n");
  if (stmt->defer_stmt.expr)
    codegen_expr(ctx, stmt->defer_stmt.expr);
}

void codegen_function(CodeGenContext *ctx, ASTNode *func) {
  DPRINTF_STMT("function %s params=%d exported=%d\n",
               func->func.name ? func->func.name : "(anon)",
               func->func.param_count, func->func.is_exported);

  LLVMTypeRef rt = func->func.return_type
                       ? codegen_type_from_node(ctx, func->func.return_type)
                       : LLVMVoidTypeInContext(ctx->llvm_ctx);
  DPRINTF_STMT("  return kind=%d\n", (int)LLVMGetTypeKind(rt));

  int pc = func->func.param_count;
  LLVMTypeRef stack_pt[16];
  LLVMTypeRef *pts = pc <= 16 ? stack_pt : malloc(sizeof(LLVMTypeRef) * pc);
  for (int i = 0; i < pc; i++) {
    if (func->func.param_types && func->func.param_types[i])
      pts[i] = codegen_type_from_node(ctx, func->func.param_types[i]);
    else
      pts[i] = LLVMInt64TypeInContext(ctx->llvm_ctx);
    DPRINTF_STMT("  param %d kind=%d\n", i, (int)LLVMGetTypeKind(pts[i]));
  }
  LLVMTypeRef ft = LLVMFunctionType(rt, pts, pc, 0);
  LLVMValueRef fn = LLVMAddFunction(ctx->module, func->func.name, ft);

  if (func->func.is_exported || strcmp(func->func.name, "main") == 0)
    LLVMSetLinkage(fn, LLVMExternalLinkage);
  else
    LLVMSetLinkage(fn, LLVMInternalLinkage);

  codegen_func_map_add(ctx, func->func.name, fn, ft);
  DPRINTF_STMT("  registered in func_map\n");

  ctx->current_func.function = fn;
  ctx->current_func.return_type = rt;
  ctx->current_func.has_return = false;
  ctx->current_func.entry_block =
      LLVMAppendBasicBlockInContext(ctx->llvm_ctx, fn, "entry");
  LLVMPositionBuilderAtEnd(ctx->builder, ctx->current_func.entry_block);
  codegen_scope_push(ctx);
  for (int i = 0; i < pc; i++) {
    LLVMValueRef p = LLVMGetParam(fn, i);
    const char *pname = func->func.params[i]->variable.name;
    LLVMValueRef al = LLVMBuildAlloca(ctx->builder, pts[i], pname);
    LLVMBuildStore(ctx->builder, p, al);
    codegen_scope_add(ctx, pname, al, pts[i]);
  }
  codegen_stmt(ctx, func->func.body);
  if (!block_terminated(ctx)) {
    DPRINTF_STMT("  no terminator, adding default ret\n");
    if (rt == LLVMVoidTypeInContext(ctx->llvm_ctx))
      LLVMBuildRetVoid(ctx->builder);
    else
      LLVMBuildRet(ctx->builder, LLVMConstNull(rt));
  }
  codegen_scope_pop(ctx);
  ctx->functions_generated++;
  if (pts != stack_pt)
    free(pts);
  LLVMClearInsertionPosition(ctx->builder);
  DPRINTF_STMT("function %s done\n", func->func.name);
}

void codegen_struct(CodeGenContext *ctx, ASTNode *sd) {
  DPRINTF_STMT("struct %s fields=%d\n", sd->struct_def.name,
               sd->struct_def.field_count);
  int count = sd->struct_def.field_count;
  LLVMTypeRef stack_ft[32];
  LLVMTypeRef *fts =
      count <= 32 ? stack_ft : malloc(sizeof(LLVMTypeRef) * count);
  for (int i = 0; i < count; i++)
    fts[i] = codegen_type_from_node(ctx, sd->struct_def.fields[i]);
  LLVMTypeRef st = LLVMStructCreateNamed(ctx->llvm_ctx, sd->struct_def.name);
  LLVMStructSetBody(st, fts, count, 0);
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
    ctx->struct_types.field_names =
        realloc(ctx->struct_types.field_names,
                ctx->struct_types.capacity * sizeof(char **));
    ctx->struct_types.lookups =
        realloc(ctx->struct_types.lookups,
                ctx->struct_types.capacity * sizeof(FieldLookup *));
  }
  int index = ctx->struct_types.count;
  ctx->struct_types.names[index] = sd->struct_def.name;
  ctx->struct_types.types[index] = st;
  ctx->struct_types.field_counts[index] = count;
  ctx->struct_types.field_names[index] = sd->struct_def.field_names;
  ctx->struct_types.lookups[index] = NULL;
  ctx->struct_types.count++;

  if (count > 0) {
    FieldLookup *fl = calloc(1, sizeof(FieldLookup));
    fl->type = st;
    fl->names = sd->struct_def.field_names;
    fl->count = count;
    fl->bucket_count = 16;
    while (fl->bucket_count < count * 2)
      fl->bucket_count *= 2;
    fl->buckets = calloc(fl->bucket_count, sizeof(uint32_t));
    fl->next = calloc(count, sizeof(uint32_t));
    for (int i = 0; i < count; i++) {
      unsigned long h = 5381;
      const char *s = sd->struct_def.field_names[i];
      int c;
      while ((c = *s++))
        h = ((h << 5) + h) + (unsigned char)c;
      h &= (fl->bucket_count - 1);
      fl->next[i] = fl->buckets[h];
      fl->buckets[h] = (uint32_t)(i + 1);
    }
    ctx->struct_types.lookups[index] = fl;
  }

  if (fts != stack_ft)
    free(fts);
}

void codegen_enum(CodeGenContext *ctx, ASTNode *ed) {
  if (!ed || !ed->enum_def.name)
    return;
  DPRINTF_STMT("enum %s values=%d\n", ed->enum_def.name,
               ed->enum_def.value_count);

  int64_t next_int = 0;
  bool prev_is_int = true;

  for (int i = 0; i < ed->enum_def.value_count; i++) {
    if (!ed->enum_def.values || !ed->enum_def.values[i])
      continue;

    size_t nlen = strlen(ed->enum_def.name);
    size_t vlen = strlen(ed->enum_def.values[i]);
    char *nm = malloc(nlen + vlen + 2);
    memcpy(nm, ed->enum_def.name, nlen);
    nm[nlen] = '_';
    memcpy(nm + nlen + 1, ed->enum_def.values[i], vlen);
    nm[nlen + 1 + vlen] = '\0';

    ASTNode *expr =
        ed->enum_def.value_exprs ? ed->enum_def.value_exprs[i] : NULL;

    LLVMValueRef init;
    LLVMTypeRef gtype;

    if (expr) {
      init = codegen_expr(ctx, expr);
      gtype = LLVMTypeOf(init);
      if (LLVMGetTypeKind(gtype) == LLVMIntegerTypeKind) {
        if (!LLVMIsConstant(init)) {
          codegen_error(
              ctx, "Enum value for '%s.%s' must be a compile-time constant",
              ed->enum_def.name, ed->enum_def.values[i]);
          free(nm);
          continue;
        }
        next_int = LLVMConstIntGetSExtValue(init) + 1;
        prev_is_int = true;
      } else {
        prev_is_int = false;
      }
    } else if (prev_is_int) {
      gtype = LLVMInt64TypeInContext(ctx->llvm_ctx);
      init = LLVMConstInt(gtype, next_int, 0);
      next_int++;
    } else {
      gtype = LLVMInt64TypeInContext(ctx->llvm_ctx);
      init = LLVMConstInt(gtype, 0, 0);
      next_int = 1;
      prev_is_int = true;
    }

    LLVMValueRef g = LLVMAddGlobal(ctx->module, gtype, nm);
    LLVMSetInitializer(g, init);
    LLVMSetLinkage(g, LLVMInternalLinkage);
    LLVMSetGlobalConstant(g, true);
    DPRINTF_STMT("  enum val %s\n", nm);
    free(nm);
  }
}

void codegen_stmt(CodeGenContext *ctx, ASTNode *stmt) {
  if (!stmt)
    return;
  DPRINTF_STMT("stmt kind=%d line=%d col=%d\n", stmt->type, stmt->line,
               stmt->column);
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
    DPRINTF_STMT("break\n");
    LLVMBasicBlockRef b = codegen_get_break_block(ctx);
    if (b)
      LLVMBuildBr(ctx->builder, b);
    break;
  }
  case NODE_CONTINUE: {
    DPRINTF_STMT("continue\n");
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

  DPRINTF_STMT("generate: %d top-level statements\n",
               program->block.statement_count);

  bool is_module_file = false;
  for (int i = 0; i < program->block.statement_count; i++) {
    ASTNode *node = program->block.statements[i];
    if (node && node->type == NODE_MODULE) {
      is_module_file = true;
      break;
    }
  }

  codegen_scope_push(ctx);
  ctx->global_scope = ctx->current_scope;

  for (int i = 0; i < program->block.statement_count; i++) {
    ASTNode *node = program->block.statements[i];
    if (!node)
      continue;
    if (node->type == NODE_STRUCT) {
      DPRINTF_STMT("pass1 struct %s\n", node->struct_def.name);
      codegen_struct(ctx, node);
    } else if (node->type == NODE_ENUM) {
      DPRINTF_STMT("pass1 enum %s\n", node->enum_def.name);
      codegen_enum(ctx, node);
    }
  }

  for (int i = 0; i < program->block.statement_count; i++) {
    ASTNode *node = program->block.statements[i];
    if (!node)
      continue;
    if (node->type == NODE_FUNCTION) {
      DPRINTF_STMT("pass2 function %s\n", node->func.name);
      codegen_function(ctx, node);
    } else if (node->type == NODE_MODULE) {
      DPRINTF_STMT("pass2 module %s\n", node->module.name);
      if (node->module.body) {
        for (int j = 0; j < node->module.body->block.statement_count; j++) {
          ASTNode *mn = node->module.body->block.statements[j];
          if (!mn)
            continue;
          if (mn->type == NODE_FUNCTION) {
            char *full =
                string_format("%s.%s", node->module.name, mn->func.name);
            DPRINTF_STMT("  module fn %s -> %s\n", mn->func.name, full);
            mn->func.name = full;
            codegen_function(ctx, mn);
          }
        }
      }
    }
  }

  LLVMClearInsertionPosition(ctx->builder);
  codegen_run_opt_passes(ctx);
  if (!is_module_file) {
    bool has_main = false;
    for (int i = 0; i < ctx->func_map.count; i++) {
      DPRINTF_STMT("func_map[%d] = %s\n", i, ctx->func_map.names[i]);
      if (strcmp(ctx->func_map.names[i], "main") == 0) {
        has_main = true;
        break;
      }
    }
    DPRINTF_STMT("has_main=%d\n", has_main);
    if (!has_main) {
      codegen_error(
          ctx, "No main function found. Program must have a 'main' function.");
      return false;
    }
  }

  if (ctx->verbose) {
    fprintf(stderr, "\n=== LLVM IR ===\n");
    LLVMDumpModule(ctx->module);
    fprintf(stderr, "=== End LLVM IR ===\n\n");
  }

  return !ctx->has_error;
}
