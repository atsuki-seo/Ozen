/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#include <string.h>

#include "core.h"

typedef enum vk : uint8_t { V_ERR, V_NONE, V_BOOL, V_QTY, V_STR, V_LIST, V_REF } vk;

typedef struct value value;
struct value {
  vk kind;
  bool b;
  ozpi_dim dim;
  bool has_amount;
  ozpi_dim adim;
  uint32_t step;
  uint32_t n;
  ozpi_ival q;
  ozpi_str s;
  ozpi_str name;
  value *items;
  ozpi_rat portion;
  ozpi_ival amount;
};

typedef struct binding {
  ozpi_str name;
  uint32_t exp;
  value v;
} binding;


static const value ERR = {.kind = V_ERR};

static const char *const dim_names[] = {"Dimensionless", "Mass", "Volume", "Length", "Time", "Count"};
static const char *const base_units[] = {"", "g", "ml", "mm", "s", "pcs"};

static const struct {
  const char *name;
  ozpi_dim dim;
  int64_t factor;
} units[] = {
    {"g", DIM_MASS, 1},     {"kg", DIM_MASS, 1000},  {"ml", DIM_VOLUME, 1}, {"L", DIM_VOLUME, 1000},
    {"tbsp", DIM_VOLUME, 15}, {"tsp", DIM_VOLUME, 5}, {"mm", DIM_LENGTH, 1}, {"cm", DIM_LENGTH, 10},
    {"s", DIM_TIME, 1},     {"min", DIM_TIME, 60},   {"h", DIM_TIME, 3600}, {"pcs", DIM_COUNT, 1},
};

static const char *const builtins[] = {"ceil", "floor", "min", "max", "len"};

static ozpi_str kind_name(const value *v) {
  switch (v->kind) {
  case V_BOOL:
    return ozpi_strz("Bool");
  case V_QTY:
    return ozpi_strz("Quantity");
  case V_STR:
    return ozpi_strz("String");
  case V_LIST:
    return ozpi_strz("List");
  case V_REF:
    return ozpi_strz("Ref");
  default:
    return ozpi_strz("None");
  }
}

static value qty(ozpi_dim dim, ozpi_rat lo, ozpi_rat hi) { return (value){.kind = V_QTY, .dim = dim, .q = {lo, hi}}; }

static value boolean(bool b) { return (value){.kind = V_BOOL, .b = b}; }

static bool is_point(const value *v) { return ozpi_ival_is_point(v->q); }

static bool render_value(ozpi_ctx *c, ozpi_buf *b, const value *v) {
  switch (v->kind) {
  case V_BOOL:
    return ozpi_buf_puts(c, b, v->b ? "true" : "false");
  case V_QTY: {
    bool ok = ozpi_buf_putrat(c, b, v->q.lo);
    if (!is_point(v)) {
      ok = ok && ozpi_buf_puts(c, b, "..") && ozpi_buf_putrat(c, b, v->q.hi);
    }
    if (v->dim != DIM_NONE) {
      ok = ok && ozpi_buf_puts(c, b, " ") && ozpi_buf_puts(c, b, base_units[v->dim]);
    }
    return ok;
  }
  case V_STR:
    return ozpi_buf_puts(c, b, "\"") && ozpi_put_escaped(c, b, v->s) && ozpi_buf_puts(c, b, "\"");
  case V_LIST: {
    bool ok = ozpi_buf_puts(c, b, "[");
    for (uint32_t i = 0; ok && i < v->n; i++) {
      ok = (i == 0 || ozpi_buf_puts(c, b, ", ")) && render_value(c, b, &v->items[i]);
    }
    return ok && ozpi_buf_puts(c, b, "]");
  }
  case V_REF: {
    bool ok;
    if (v->step != 0) {
      ok = ozpi_buf_puts(c, b, "%") && ozpi_buf_putu(c, b, v->step);
    } else {
      ok = ozpi_buf_put(c, b, v->name.p, v->name.len);
    }
    if (ok && !(v->portion.num == 1 && v->portion.den == 1)) {
      ok = ozpi_buf_puts(c, b, "*") && ozpi_buf_puti(c, b, v->portion.num) && ozpi_buf_puts(c, b, "/") &&
           ozpi_buf_putu(c, b, (uint64_t)v->portion.den);
    }
    return ok;
  }
  default:
    return ozpi_buf_puts(c, b, "none");
  }
}

static ozpi_str value_str(ozpi_ctx *c, const value *v) {
  ozpi_buf b = {0};
  render_value(c, &b, v);
  return ozpi_buf_str(&b);
}

static bool tick(ozpi_ctx *c, const ozpi_node *n, uint32_t depth) {
  if (c->stop) {
    return false;
  }
  if (c->fuel >= c->lim.steps) {
    ozpi_diag1(c, OZPI_D_MAC_fuelExhausted, n->span, n->exp, ozpi_fmt_u(c, c->lim.steps));
    c->stop = true;
    return false;
  }
  c->fuel++;
  if (depth > c->lim.depth) {
    ozpi_diag1(c, OZPI_D_MAC_depthExceeded, n->span, n->exp, ozpi_fmt_u(c, c->lim.depth));
    c->stop = true;
    return false;
  }
  if (depth > c->maxdepth) {
    c->maxdepth = depth;
  }
  return true;
}

static bool bind(ozpi_ctx *c, ozpi_str name, uint32_t exp, value v) {
  if (!ozpi_grow(c, &c->bind, &c->capbind, sizeof(binding), c->nbind + 1)) {
    return false;
  }
  ((binding *)c->bind)[c->nbind++] = (binding){name, exp, v};
  return true;
}

static binding *lookup_c(ozpi_ctx *c, ozpi_str name, uint32_t exp) {
  binding *b = c->bind;
  for (uint32_t i = c->nbind; i-- > 0;) {
    if (b[i].exp == exp && ozpi_str_eq(b[i].name, name)) {
      return &b[i];
    }
  }
  return nullptr;
}

static value *push_values(ozpi_ctx *c, uint32_t n) {
  if (!ozpi_grow(c, &c->vstack, &c->capv, sizeof(value), c->nv + n)) {
    return nullptr;
  }
  value *v = (value *)c->vstack + c->nv;
  c->nv += n;
  return v;
}

static const ozpi_decl *find_decl(const ozpi_ctx *c, ozpi_str name) {
  for (uint32_t i = 0; i < c->ndecls; i++) {
    if (c->decls[i].kind != D_RECIPE && ozpi_str_eq(c->decls[i].name, name)) {
      return &c->decls[i];
    }
  }
  return nullptr;
}

static int find_builtin(ozpi_str name) {
  for (int i = 0; i < (int)(sizeof builtins / sizeof builtins[0]); i++) {
    if (ozpi_str_eqz(name, builtins[i])) {
      return i;
    }
  }
  return -1;
}

static void diag_status(ozpi_ctx *c, ozpi_status st, const ozpi_node *n) {
  ozpi_diag0(c, st == OZPI_DIVZERO ? OZPI_D_MAC_divisionByZero : OZPI_D_MAC_overflow, n->span, n->exp);
}

static value eval(ozpi_ctx *c, const ozpi_node *n, uint32_t depth);
static value eval_block(ozpi_ctx *c, const ozpi_node *n, uint32_t depth, bool new_scope);

static value arith(ozpi_ctx *c, const ozpi_node *n, value l, value r) {
  ozpi_op op = n->op;
  if (l.kind == V_REF && op == OP_DIV) {
    if (r.kind != V_QTY || !is_point(&r)) {
      ozpi_diag2(c, OZPI_D_TYP_type, n->span, n->exp, ozpi_strz("Quantity"), kind_name(&r));
      return ERR;
    }
    if (r.dim == DIM_NONE) {
      if (r.q.lo.num <= 0) {
        ozpi_diag2(c, OZPI_D_TYP_type, n->span, n->exp, ozpi_strz("positive"), value_str(c, &r));
        return ERR;
      }
      value out = l;
      ozpi_status st = ozpi_rat_div(l.portion, r.q.lo, &out.portion);
      if (st != OZPI_OK) {
        diag_status(c, st, n);
        return ERR;
      }
      return out;
    }
    if (!l.has_amount) {
      ozpi_diag1(c, OZPI_D_TYP_amountUnknown, n->span, n->exp, value_str(c, &l));
      return ERR;
    }
    if (l.adim != r.dim) {
      ozpi_diag2(c, OZPI_D_TYP_dimension, n->span, n->exp, ozpi_strz(dim_names[l.adim]), ozpi_strz(dim_names[r.dim]));
      return ERR;
    }
    value out = qty(DIM_NONE, l.amount.lo, l.amount.hi);
    ozpi_status st = ozpi_ival_div(l.amount, r.q.lo, &out.q);
    if (st != OZPI_OK) {
      diag_status(c, st, n);
      return ERR;
    }
    return out;
  }
  if (l.kind != V_QTY || r.kind != V_QTY) {
    const value *bad = l.kind != V_QTY ? &l : &r;
    ozpi_diag2(c, OZPI_D_TYP_type, n->span, n->exp, ozpi_strz("Quantity"), kind_name(bad));
    return ERR;
  }
  ozpi_status st = OZPI_OK;
  value out = l;
  switch (op) {
  case OP_ADD:
  case OP_SUB:
    if (l.dim != r.dim) {
      ozpi_diag2(c, OZPI_D_TYP_dimension, n->span, n->exp, ozpi_strz(dim_names[l.dim]), ozpi_strz(dim_names[r.dim]));
      return ERR;
    }
    st = op == OP_ADD ? ozpi_ival_add(l.q, r.q, &out.q) : ozpi_ival_sub(l.q, r.q, &out.q);
    break;
  case OP_MUL:
    if (l.dim != DIM_NONE && r.dim != DIM_NONE) {
      ozpi_diag2(c, OZPI_D_TYP_unsupportedDimension, n->span, n->exp, ozpi_strz(dim_names[l.dim]),
                 ozpi_strz(dim_names[r.dim]));
      return ERR;
    }
    if (!is_point(&l) && !is_point(&r)) {
      ozpi_diag1(c, OZPI_D_TYP_range, n->span, n->exp, ozpi_strz("*"));
      return ERR;
    }
    out.dim = l.dim != DIM_NONE ? l.dim : r.dim;
    st = is_point(&r) ? ozpi_ival_scale(l.q, r.q.lo, &out.q) : ozpi_ival_scale(r.q, l.q.lo, &out.q);
    break;
  case OP_DIV:
    if (l.dim == DIM_NONE && r.dim != DIM_NONE) {
      ozpi_diag2(c, OZPI_D_TYP_unsupportedDimension, n->span, n->exp, ozpi_strz(dim_names[l.dim]),
                 ozpi_strz(dim_names[r.dim]));
      return ERR;
    }
    if (r.dim != DIM_NONE && l.dim != r.dim) {
      ozpi_diag2(c, OZPI_D_TYP_dimension, n->span, n->exp, ozpi_strz(dim_names[l.dim]), ozpi_strz(dim_names[r.dim]));
      return ERR;
    }
    if (!is_point(&r)) {
      ozpi_diag1(c, OZPI_D_TYP_range, n->span, n->exp, ozpi_strz("/"));
      return ERR;
    }
    out.dim = r.dim != DIM_NONE ? DIM_NONE : l.dim;
    st = ozpi_ival_div(l.q, r.q.lo, &out.q);
    break;
  default:
    return ERR;
  }
  if (st != OZPI_OK) {
    diag_status(c, st, n);
    return ERR;
  }
  return out;
}

static const char *op_text(ozpi_op op) {
  switch (op) {
  case OP_EQ:
    return "==";
  case OP_NE:
    return "!=";
  case OP_LT:
    return "<";
  case OP_LE:
    return "<=";
  case OP_GT:
    return ">";
  default:
    return ">=";
  }
}

static value compare(ozpi_ctx *c, const ozpi_node *n, value l, value r) {
  ozpi_op op = n->op;
  bool eqop = op == OP_EQ || op == OP_NE;
  if (l.kind == V_QTY && r.kind == V_QTY) {
    if (l.dim != r.dim) {
      ozpi_diag2(c, OZPI_D_TYP_dimension, n->span, n->exp, ozpi_strz(dim_names[l.dim]), ozpi_strz(dim_names[r.dim]));
      return ERR;
    }
    if (!is_point(&l) || !is_point(&r)) {
      ozpi_diag1(c, OZPI_D_TYP_range, n->span, n->exp, ozpi_strz(op_text(op)));
      return ERR;
    }
    int k = ozpi_rat_cmp(l.q.lo, r.q.lo);
    switch (op) {
    case OP_EQ:
      return boolean(k == 0);
    case OP_NE:
      return boolean(k != 0);
    case OP_LT:
      return boolean(k < 0);
    case OP_LE:
      return boolean(k <= 0);
    case OP_GT:
      return boolean(k > 0);
    default:
      return boolean(k >= 0);
    }
  }
  if (eqop && l.kind == r.kind && (l.kind == V_BOOL || l.kind == V_STR)) {
    bool same = l.kind == V_BOOL ? l.b == r.b : ozpi_str_eq(l.s, r.s);
    return boolean(op == OP_EQ ? same : !same);
  }
  if (!eqop) {
    const value *bad = l.kind != V_QTY ? &l : &r;
    ozpi_diag2(c, OZPI_D_TYP_type, n->span, n->exp, ozpi_strz("Quantity"), kind_name(bad));
  } else {
    ozpi_diag2(c, OZPI_D_TYP_type, n->span, n->exp, kind_name(&l), kind_name(&r));
  }
  return ERR;
}

static value eval_binary(ozpi_ctx *c, const ozpi_node *n, uint32_t depth) {
  if (n->op == OP_AND || n->op == OP_OR) {
    value l = eval(c, n->kids[0], depth + 1);
    if (l.kind == V_ERR) {
      return ERR;
    }
    if (l.kind != V_BOOL) {
      ozpi_diag2(c, OZPI_D_TYP_type, n->kids[0]->span, n->kids[0]->exp, ozpi_strz("Bool"), kind_name(&l));
      return ERR;
    }
    if ((n->op == OP_AND && !l.b) || (n->op == OP_OR && l.b)) {
      return l;
    }
    value r = eval(c, n->kids[1], depth + 1);
    if (r.kind == V_ERR) {
      return ERR;
    }
    if (r.kind != V_BOOL) {
      ozpi_diag2(c, OZPI_D_TYP_type, n->kids[1]->span, n->kids[1]->exp, ozpi_strz("Bool"), kind_name(&r));
      return ERR;
    }
    return r;
  }
  value l = eval(c, n->kids[0], depth + 1);
  value r = eval(c, n->kids[1], depth + 1);
  if (l.kind == V_ERR || r.kind == V_ERR) {
    return ERR;
  }
  if (n->op >= OP_EQ && n->op <= OP_GE) {
    return compare(c, n, l, r);
  }
  return arith(c, n, l, r);
}

static bool render_args(ozpi_ctx *c, ozpi_buf *b, const ozpi_node *n, const value *vals) {
  bool ok = true;
  for (uint32_t i = 0; ok && i < n->nkids; i++) {
    if (i > 0) {
      ok = ozpi_buf_puts(c, b, ", ");
    }
    if (ok && i >= n->npos) {
      ozpi_str name = n->names[i - n->npos];
      ok = ozpi_buf_put(c, b, name.p, name.len) && ozpi_buf_puts(c, b, ": ");
    }
    ok = ok && render_value(c, b, &vals[i]);
  }
  return ok;
}

static value *eval_args(ozpi_ctx *c, const ozpi_node *n, uint32_t depth, bool *poisoned) {
  uint32_t base = c->nv;
  *poisoned = false;
  if (n->nkids == 0) {
    return nullptr;
  }
  if (push_values(c, n->nkids) == nullptr) {
    return nullptr;
  }
  for (uint32_t i = 0; i < n->nkids; i++) {
    value v = eval(c, n->kids[i], depth + 1);
    ((value *)c->vstack)[base + i] = v;
    if (v.kind == V_ERR) {
      *poisoned = true;
    }
    if (c->stop) {
      *poisoned = true;
      break;
    }
  }
  return (value *)c->vstack + base;
}

static value call_action_inner(ozpi_ctx *c, const ozpi_node *n, const ozpi_decl *d, uint32_t depth);

static value call_action(ozpi_ctx *c, const ozpi_node *n, const ozpi_decl *d, uint32_t depth) {
  uint32_t base = c->nv;
  value v = call_action_inner(c, n, d, depth);
  c->nv = base;
  return v;
}

static value call_action_inner(ozpi_ctx *c, const ozpi_node *n, const ozpi_decl *d, uint32_t depth) {
  bool poisoned = false;
  value *vals = eval_args(c, n, depth, &poisoned);
  if (c->stop || (n->nkids > 0 && vals == nullptr)) {
    return ERR;
  }
  for (uint32_t i = n->npos; i < n->nkids; i++) {
    ozpi_str name = n->names[i - n->npos];
    int idx = -1;
    for (uint32_t k = 0; k < d->nparams; k++) {
      if (ozpi_str_eq(d->params[k], name)) {
        idx = (int)k;
      }
    }
    bool dup = idx >= 0 && (uint32_t)idx < n->npos;
    for (uint32_t j = n->npos; j < i && !dup; j++) {
      dup = ozpi_str_eq(n->names[j - n->npos], name);
    }
    if (idx < 0 || dup) {
      ozpi_diag2(c, OZPI_D_NAM_arity, n->span, n->exp, d->name, name);
      return ERR;
    }
  }
  if (n->nkids != d->nparams) {
    ozpi_diag3(c, OZPI_D_NAM_arity, n->span, n->exp, d->name, ozpi_fmt_u(c, d->nparams), ozpi_fmt_u(c, n->nkids));
    return ERR;
  }
  if (poisoned) {
    return ERR;
  }
  if (c->nodes >= c->lim.nodes) {
    ozpi_diag1(c, OZPI_D_MAC_nodeLimit, n->span, n->exp, ozpi_fmt_u(c, c->lim.nodes));
    c->stop = true;
    return ERR;
  }
  if (!ozpi_grow(c, (void **)&c->steps, &c->capsteps, sizeof(ozpi_step), c->nsteps + 1)) {
    return ERR;
  }
  c->nodes++;
  ozpi_step *st = &c->steps[c->nsteps];
  *st = (ozpi_step){0};
  if (!ozpi_buf_put(c, &st->call, d->name.p, d->name.len) || !ozpi_buf_puts(c, &st->call, "(") ||
      !render_args(c, &st->call, n, vals) || !ozpi_buf_puts(c, &st->call, ")")) {
    return ERR;
  }
  c->nsteps++;
  c->annot_node = n;
  c->annot_step = c->nsteps;
  return (value){.kind = V_REF, .step = c->nsteps, .portion = ozpi_rat_int(1)};
}

static value call_builtin_inner(ozpi_ctx *c, const ozpi_node *n, int which, uint32_t depth);

static value call_builtin(ozpi_ctx *c, const ozpi_node *n, int which, uint32_t depth) {
  uint32_t base = c->nv;
  value v = call_builtin_inner(c, n, which, depth);
  c->nv = base;
  return v;
}

static value call_builtin_inner(ozpi_ctx *c, const ozpi_node *n, int which, uint32_t depth) {
  bool poisoned = false;
  value *vals = eval_args(c, n, depth, &poisoned);
  if (c->stop || (n->nkids > 0 && vals == nullptr)) {
    return ERR;
  }
  uint32_t want = which == 2 || which == 3 ? 2 : 1;
  if (n->nkids != want || n->npos != n->nkids) {
    ozpi_diag3(c, OZPI_D_NAM_arity, n->span, n->exp, n->text, ozpi_fmt_u(c, want), ozpi_fmt_u(c, n->nkids));
    return ERR;
  }
  if (poisoned) {
    return ERR;
  }
  if (which == 4) {
    if (vals[0].kind != V_LIST) {
      ozpi_diag2(c, OZPI_D_TYP_type, n->span, n->exp, ozpi_strz("List"), kind_name(&vals[0]));
      return ERR;
    }
    return qty(DIM_NONE, ozpi_rat_int(vals[0].n), ozpi_rat_int(vals[0].n));
  }
  for (uint32_t i = 0; i < want; i++) {
    if (vals[i].kind != V_QTY) {
      ozpi_diag2(c, OZPI_D_TYP_type, n->span, n->exp, ozpi_strz("Quantity"), kind_name(&vals[i]));
      return ERR;
    }
  }
  if (want == 2 && vals[0].dim != vals[1].dim) {
    ozpi_diag2(c, OZPI_D_TYP_dimension, n->span, n->exp, ozpi_strz(dim_names[vals[0].dim]),
               ozpi_strz(dim_names[vals[1].dim]));
    return ERR;
  }
  for (uint32_t i = 0; i < want; i++) {
    if (!is_point(&vals[i])) {
      ozpi_diag1(c, OZPI_D_TYP_range, n->span, n->exp, n->text);
      return ERR;
    }
  }
  ozpi_rat r;
  switch (which) {
  case 0:
    r = ozpi_rat_ceil(vals[0].q.lo);
    break;
  case 1:
    r = ozpi_rat_floor(vals[0].q.lo);
    break;
  case 2:
    r = ozpi_rat_cmp(vals[0].q.lo, vals[1].q.lo) <= 0 ? vals[0].q.lo : vals[1].q.lo;
    break;
  default:
    r = ozpi_rat_cmp(vals[0].q.lo, vals[1].q.lo) >= 0 ? vals[0].q.lo : vals[1].q.lo;
    break;
  }
  return qty(vals[0].dim, r, r);
}

typedef struct copier {
  ozpi_ctx *c;
  const ozpi_decl *macro;
  const ozpi_node *call;
  uint32_t exp;
  uint64_t copies;
} copier;

static ozpi_node *copy_node(copier *cp, const ozpi_node *src, bool keep_exp);

static ozpi_node *copy_plain(copier *cp, const ozpi_node *src, bool keep_exp) {
  ozpi_node *n = ozpi_new(cp->c, sizeof(ozpi_node));
  if (n == nullptr) {
    return nullptr;
  }
  *n = *src;
  cp->copies++;
  if (!keep_exp) {
    n->exp = cp->exp;
    n->bexp = cp->exp;
  }
  if (src->nkids > 0) {
    n->kids = ozpi_new(cp->c, sizeof(ozpi_node *) * src->nkids);
    if (n->kids == nullptr) {
      return nullptr;
    }
    for (uint32_t i = 0; i < src->nkids; i++) {
      n->kids[i] = copy_node(cp, src->kids[i], keep_exp);
      if (n->kids[i] == nullptr) {
        return nullptr;
      }
    }
  }
  if (src->nannots > 0) {
    n->annots = ozpi_new(cp->c, sizeof(ozpi_node *) * src->nannots);
    if (n->annots == nullptr) {
      return nullptr;
    }
    for (uint32_t i = 0; i < src->nannots; i++) {
      n->annots[i] = copy_node(cp, src->annots[i], keep_exp);
      if (n->annots[i] == nullptr) {
        return nullptr;
      }
    }
  }
  return n;
}

static const ozpi_node *macro_arg(const copier *cp, ozpi_str name) {
  for (uint32_t i = 0; i < cp->macro->nparams; i++) {
    if (ozpi_str_eq(cp->macro->params[i], name)) {
      return cp->call->kids[i];
    }
  }
  return nullptr;
}

static ozpi_node *copy_node(copier *cp, const ozpi_node *src, bool keep_exp) {
  if (!keep_exp && src->kind == N_SPLICE) {
    const ozpi_node *arg = macro_arg(cp, src->text);
    if (arg == nullptr) {
      ozpi_node *n = copy_plain(cp, src, false);
      if (n != nullptr) {
        n->kind = N_ERROR;
        n->bad = true;
      }
      return n;
    }
    return copy_node(cp, arg, true);
  }
  ozpi_node *n = copy_plain(cp, src, keep_exp);
  if (n != nullptr && !keep_exp && src->kind == N_LET && src->flag) {
    const ozpi_node *arg = macro_arg(cp, src->text);
    if (arg != nullptr && arg->kind == N_NAME) {
      n->text = arg->text;
      n->bexp = arg->exp;
      n->flag = false;
    } else {
      n->bad = true;
    }
  }
  return n;
}

static value call_macro(ozpi_ctx *c, const ozpi_node *n, const ozpi_decl *d, uint32_t depth) {
  if (n->nkids != d->nparams || n->npos != n->nkids) {
    ozpi_diag3(c, OZPI_D_NAM_arity, n->span, n->exp, d->name, ozpi_fmt_u(c, d->nparams), ozpi_fmt_u(c, n->nkids));
    return ERR;
  }
  if (d->body == nullptr) {
    return ERR;
  }
  if (!ozpi_grow(c, (void **)&c->exps, &c->capexps, sizeof(ozpi_expansion), c->nexps + 1)) {
    return ERR;
  }
  if (c->nexps == 0) {
    c->exps[c->nexps++] = (ozpi_expansion){0};
    if (!ozpi_grow(c, (void **)&c->exps, &c->capexps, sizeof(ozpi_expansion), c->nexps + 1)) {
      return ERR;
    }
  }
  uint32_t exp = c->nexps;
  c->exps[c->nexps++] = (ozpi_expansion){n->span, n->exp};
  copier cp = {c, d, n, exp, 0};
  ozpi_node *body = copy_node(&cp, d->body, false);
  if (body == nullptr) {
    return ERR;
  }
  if (c->nodes + cp.copies > c->lim.nodes) {
    ozpi_diag1(c, OZPI_D_MAC_nodeLimit, n->span, n->exp, ozpi_fmt_u(c, c->lim.nodes));
    c->stop = true;
    return ERR;
  }
  c->nodes += cp.copies;
  value v = eval_block(c, body, depth + 1, false);
  return v.kind == V_ERR ? ERR : (value){.kind = V_NONE};
}

static value eval_call(ozpi_ctx *c, const ozpi_node *n, uint32_t depth) {
  binding *local = lookup_c(c, n->text, n->exp);
  if (local != nullptr) {
    ozpi_diag2(c, OZPI_D_TYP_type, n->span, n->exp, ozpi_strz("Action"), kind_name(&local->v));
    return ERR;
  }
  const ozpi_decl *d = find_decl(c, n->text);
  if (d != nullptr && d->kind == D_ACTION) {
    return call_action(c, n, d, depth);
  }
  if (d != nullptr && d->kind == D_MACRO) {
    return call_macro(c, n, d, depth);
  }
  int b = find_builtin(n->text);
  if (b >= 0) {
    return call_builtin(c, n, b, depth);
  }
  ozpi_diag1(c, OZPI_D_NAM_undefined, n->span2, n->exp, n->text);
  return ERR;
}

static value eval_quantity(ozpi_ctx *c, const ozpi_node *n) {
  for (size_t i = 0; i < sizeof units / sizeof units[0]; i++) {
    if (ozpi_str_eqz(n->text, units[i].name)) {
      ozpi_ival q;
      ozpi_status st = ozpi_ival_scale((ozpi_ival){n->lo, n->hi}, ozpi_rat_int(units[i].factor), &q);
      if (st != OZPI_OK) {
        diag_status(c, st, n);
        return ERR;
      }
      if (ozpi_rat_cmp(q.lo, q.hi) > 0) {
        ozpi_diag1(c, OZPI_D_TYP_range, n->span, n->exp, ozpi_strz(".."));
        return ERR;
      }
      return qty(units[i].dim, q.lo, q.hi);
    }
  }
  ozpi_diag1(c, OZPI_D_NAM_unknownUnit, n->span, n->exp, n->text);
  return ERR;
}

static void check_rebind(ozpi_ctx *c, const ozpi_node *n, ozpi_str name, uint32_t exp) {
  if (lookup_c(c, name, exp) != nullptr) {
    ozpi_diag1(c, OZPI_D_NAM_rebind, n->span, n->exp, name);
  }
}

static bool count_ok(const value *v, int64_t *out) {
  if (v->kind != V_QTY || (v->dim != DIM_NONE && v->dim != DIM_COUNT) || !is_point(v) || v->q.lo.den != 1 ||
      v->q.lo.num < 0) {
    return false;
  }
  *out = v->q.lo.num;
  return true;
}

static void eval_annots(ozpi_ctx *c, const ozpi_node *stmt, uint32_t depth, bool target_err) {
  const ozpi_node *target = stmt->kind == N_LET ? stmt->kids[0] : stmt;
  for (uint32_t i = 0; i < stmt->nannots && !c->stop; i++) {
    const ozpi_node *a = stmt->annots[i];
    if (!tick(c, a, depth)) {
      return;
    }
    bool poisoned = false;
    uint32_t vbase = c->nv;
    value *vals = eval_args(c, a, depth, &poisoned);
    if (c->stop || poisoned || (a->nkids > 0 && vals == nullptr)) {
      c->nv = vbase;
      continue;
    }
    if (c->annot_node == target && c->annot_step > 0) {
      ozpi_step *st = &c->steps[c->annot_step - 1];
      bool ok = ozpi_buf_puts(c, &st->annots, " @") && ozpi_buf_put(c, &st->annots, a->text.p, a->text.len);
      if (ok && a->nkids > 0) {
        ok = ozpi_buf_puts(c, &st->annots, "(") && render_args(c, &st->annots, a, vals) &&
             ozpi_buf_puts(c, &st->annots, ")");
      }
      c->nv = vbase;
      continue;
    }
    c->nv = vbase;
    if (target->kind == N_CALL && target_err) {
      continue;
    }
    ozpi_diag1(c, OZPI_D_TYP_annotationTarget, a->span, a->exp, a->text);
  }
}

static value eval_stmt(ozpi_ctx *c, const ozpi_node *n, uint32_t depth) {
  c->annot_node = nullptr;
  c->let_err = false;
  value v = eval(c, n, depth);
  bool target_err = n->kind == N_LET ? c->let_err : v.kind == V_ERR;
  if (n->nannots > 0 && !c->stop) {
    eval_annots(c, n, depth, target_err);
  }
  return v;
}

static value eval_block(ozpi_ctx *c, const ozpi_node *n, uint32_t depth, bool new_scope) {
  if (!tick(c, n, depth)) {
    return ERR;
  }
  uint32_t base = c->nbind;
  for (uint32_t i = 0; i < n->nkids && !c->stop; i++) {
    eval_stmt(c, n->kids[i], depth + 1);
  }
  if (new_scope) {
    c->nbind = base;
  }
  return c->stop ? ERR : (value){.kind = V_NONE};
}

static value eval_inner(ozpi_ctx *c, const ozpi_node *n, uint32_t depth) {
  if (n->kind == N_BLOCK) {
    return eval_block(c, n, depth, true);
  }
  if (!tick(c, n, depth)) {
    return ERR;
  }
  switch (n->kind) {
  case N_NUMBER:
    return qty(DIM_NONE, n->lo, n->lo);
  case N_QUANTITY:
    return eval_quantity(c, n);
  case N_STRING:
    return (value){.kind = V_STR, .s = n->text};
  case N_BOOL:
    return boolean(n->flag);
  case N_NAME: {
    binding *b = lookup_c(c, n->text, n->exp);
    if (b != nullptr) {
      return b->v;
    }
    if (find_decl(c, n->text) != nullptr || find_builtin(n->text) >= 0) {
      ozpi_diag2(c, OZPI_D_TYP_type, n->span, n->exp, ozpi_strz("value"), n->text);
      return ERR;
    }
    ozpi_diag1(c, OZPI_D_NAM_undefined, n->span, n->exp, n->text);
    return ERR;
  }
  case N_LIST: {
    value out = {.kind = V_LIST, .n = n->nkids};
    out.items = n->nkids > 0 ? ozpi_new(c, sizeof(value) * n->nkids) : nullptr;
    if (n->nkids > 0 && out.items == nullptr) {
      return ERR;
    }
    bool bad = false;
    for (uint32_t i = 0; i < n->nkids; i++) {
      out.items[i] = eval(c, n->kids[i], depth + 1);
      bad = bad || out.items[i].kind == V_ERR;
      if (c->stop) {
        return ERR;
      }
    }
    return bad ? ERR : out;
  }
  case N_BINARY:
    return eval_binary(c, n, depth);
  case N_UNARY: {
    value v = eval(c, n->kids[0], depth + 1);
    if (v.kind == V_ERR) {
      return ERR;
    }
    if (n->op == OP_NOT) {
      if (v.kind != V_BOOL) {
        ozpi_diag2(c, OZPI_D_TYP_type, n->span, n->exp, ozpi_strz("Bool"), kind_name(&v));
        return ERR;
      }
      return boolean(!v.b);
    }
    if (v.kind != V_QTY) {
      ozpi_diag2(c, OZPI_D_TYP_type, n->span, n->exp, ozpi_strz("Quantity"), kind_name(&v));
      return ERR;
    }
    return qty(v.dim, ozpi_rat_neg(v.q.hi), ozpi_rat_neg(v.q.lo));
  }
  case N_CALL:
    return eval_call(c, n, depth);
  case N_LET: {
    if (n->bad) {
      ozpi_diag0(c, OZPI_D_TYP_spliceNotName, n->span, n->exp);
      return ERR;
    }
    value v = eval(c, n->kids[0], depth + 1);
    c->let_err = v.kind == V_ERR;
    if (c->stop) {
      return ERR;
    }
    check_rebind(c, n, n->text, n->bexp);
    bind(c, n->text, n->bexp, v);
    return (value){.kind = V_NONE};
  }
  case N_IF: {
    value cond = eval(c, n->kids[0], depth + 1);
    if (cond.kind == V_ERR) {
      return ERR;
    }
    if (cond.kind != V_BOOL) {
      ozpi_diag2(c, OZPI_D_TYP_type, n->kids[0]->span, n->kids[0]->exp, ozpi_strz("Bool"), kind_name(&cond));
      return ERR;
    }
    if (cond.b) {
      return eval(c, n->kids[1], depth + 1);
    }
    if (n->nkids > 2) {
      return eval(c, n->kids[2], depth + 1);
    }
    return (value){.kind = V_NONE};
  }
  case N_FOR: {
    value list = eval(c, n->kids[0], depth + 1);
    if (list.kind == V_ERR) {
      return ERR;
    }
    if (list.kind != V_LIST) {
      ozpi_diag2(c, OZPI_D_TYP_type, n->kids[0]->span, n->kids[0]->exp, ozpi_strz("List"), kind_name(&list));
      return ERR;
    }
    for (uint32_t i = 0; i < list.n && !c->stop; i++) {
      uint32_t base = c->nbind;
      if (!bind(c, n->text, n->exp, list.items[i])) {
        return ERR;
      }
      eval(c, n->kids[1], depth + 1);
      c->nbind = base;
    }
    return (value){.kind = V_NONE};
  }
  case N_REPEAT: {
    value count = eval(c, n->kids[0], depth + 1);
    if (count.kind == V_ERR) {
      return ERR;
    }
    int64_t times = 0;
    if (!count_ok(&count, &times)) {
      ozpi_diag1(c, OZPI_D_TYP_count, n->kids[0]->span, n->kids[0]->exp, value_str(c, &count));
      return ERR;
    }
    for (int64_t i = 0; i < times && !c->stop; i++) {
      eval(c, n->kids[1], depth + 1);
    }
    return (value){.kind = V_NONE};
  }
  case N_INGREDIENTS: {
    for (uint32_t i = 0; i < n->nkids && !c->stop; i++) {
      const ozpi_node *item = n->kids[i];
      if (!tick(c, item, depth + 1)) {
        return ERR;
      }
      value amount = eval(c, item->kids[0], depth + 2);
      if (amount.kind == V_ERR) {
        continue;
      }
      if (amount.kind != V_QTY) {
        ozpi_diag2(c, OZPI_D_TYP_type, item->kids[0]->span, item->kids[0]->exp, ozpi_strz("Quantity"), kind_name(&amount));
        continue;
      }
      value ref = {.kind = V_REF, .name = item->text, .portion = ozpi_rat_int(1), .has_amount = true,
                   .adim = amount.dim, .amount = amount.q};
      check_rebind(c, item, item->text, item->bexp);
      bind(c, item->text, item->bexp, ref);
    }
    return (value){.kind = V_NONE};
  }
  default:
    return ERR;
  }
}

static value eval(ozpi_ctx *c, const ozpi_node *n, uint32_t depth) { return eval_inner(c, n, depth); }

static void collect_locals(const ozpi_node *n, ozpi_str *names, uint32_t *count, uint32_t cap) {
  if (n == nullptr) {
    return;
  }
  if (((n->kind == N_LET && !n->flag) || n->kind == N_FOR || n->kind == N_ITEM) && *count < cap) {
    names[(*count)++] = n->text;
  }
  for (uint32_t i = 0; i < n->nkids; i++) {
    collect_locals(n->kids[i], names, count, cap);
  }
  for (uint32_t i = 0; i < n->nannots; i++) {
    collect_locals(n->annots[i], names, count, cap);
  }
}

static uint32_t count_nodes(const ozpi_node *n) {
  uint32_t total = 1;
  for (uint32_t i = 0; i < n->nkids; i++) {
    total += count_nodes(n->kids[i]);
  }
  for (uint32_t i = 0; i < n->nannots; i++) {
    total += count_nodes(n->annots[i]);
  }
  return total;
}

static bool known_name(const ozpi_ctx *c, ozpi_str name, const ozpi_str *locals, uint32_t nlocals) {
  for (uint32_t i = 0; i < nlocals; i++) {
    if (ozpi_str_eq(locals[i], name)) {
      return true;
    }
  }
  return find_decl(c, name) != nullptr || find_builtin(name) >= 0;
}

static void check_names(ozpi_ctx *c, const ozpi_node *n, const ozpi_str *locals, uint32_t nlocals) {
  if (n->kind == N_NAME && !known_name(c, n->text, locals, nlocals)) {
    ozpi_diag1(c, OZPI_D_NAM_undefinedInMacro, n->span, 0, n->text);
  }
  if (n->kind == N_CALL && !known_name(c, n->text, locals, nlocals)) {
    ozpi_diag1(c, OZPI_D_NAM_undefinedInMacro, n->span2, 0, n->text);
  }
  for (uint32_t i = 0; i < n->nkids; i++) {
    check_names(c, n->kids[i], locals, nlocals);
  }
  for (uint32_t i = 0; i < n->nannots; i++) {
    check_names(c, n->annots[i], locals, nlocals);
  }
}

void ozpi_check_macros(ozpi_ctx *c) {
  for (uint32_t i = 0; i < c->ndecls && !c->oom; i++) {
    const ozpi_decl *d = &c->decls[i];
    if (d->kind != D_MACRO || d->body == nullptr) {
      continue;
    }
    uint32_t cap = count_nodes(d->body), n = 0;
    ozpi_str *locals = ozpi_new(c, sizeof(ozpi_str) * cap);
    if (locals == nullptr) {
      return;
    }
    collect_locals(d->body, locals, &n, cap);
    check_names(c, d->body, locals, n);
  }
}

static const ozpi_decl *select_entry(ozpi_ctx *c, const char *entry) {
  const ozpi_span none = {{0, 0}, {0, 0}};
  uint32_t recipes = 0;
  const ozpi_decl *only = nullptr;
  for (uint32_t i = 0; i < c->ndecls; i++) {
    if (c->decls[i].kind == D_RECIPE) {
      recipes++;
      only = &c->decls[i];
      if (entry != nullptr && ozpi_str_eqz(c->decls[i].name, entry)) {
        return &c->decls[i];
      }
    }
  }
  if (entry != nullptr) {
    ozpi_diag1(c, OZPI_D_NAM_undefined, none, 0, ozpi_strz(entry));
    return nullptr;
  }
  if (recipes <= 1) {
    return only;
  }
  ozpi_str *names = ozpi_new(c, sizeof(ozpi_str) * recipes);
  if (names == nullptr) {
    return nullptr;
  }
  uint32_t k = 0;
  for (uint32_t i = 0; i < c->ndecls; i++) {
    if (c->decls[i].kind == D_RECIPE) {
      names[k++] = c->decls[i].name;
    }
  }
  ozpi_diag_add(c, OZPI_D_NAM_ambiguousEntry, none, 0, names, recipes);
  return nullptr;
}

void ozpi_evaluate(ozpi_ctx *c, const char *entry, const char *params) {
  const ozpi_decl *r = select_entry(c, entry);
  if (r == nullptr || r->body == nullptr) {
    return;
  }
  ozpi_node **overrides = r->nparams > 0 ? ozpi_new(c, sizeof(ozpi_node *) * r->nparams) : nullptr;
  if (r->nparams > 0 && overrides == nullptr) {
    return;
  }
  const ozpi_span none = {{0, 0}, {0, 0}};
  bool params_ok = true;
  if (params != nullptr) {
    size_t len = ozpi_strlen(params), i = 0;
    while (i < len) {
      size_t end = i;
      while (end < len && params[end] != ';') {
        end++;
      }
      size_t eq = i;
      while (eq < end && params[eq] != '=') {
        eq++;
      }
      ozpi_str name = {params + i, (uint32_t)(eq - i)};
      int idx = -1;
      for (uint32_t k = 0; k < r->nparams; k++) {
        if (ozpi_str_eq(r->params[k], name)) {
          idx = (int)k;
        }
      }
      if (idx < 0 || eq >= end) {
        ozpi_diag1(c, OZPI_D_NAM_undefined, none, 0, name);
      } else {
        ozpi_span where = {{0, (uint32_t)(eq + 2)}, {0, (uint32_t)(eq + 2)}};
        ozpi_node *e = ozpi_parse_expr_text(c, params, end, &where);
        if (e == nullptr) {
          params_ok = false;
        }
        overrides[idx] = e;
      }
      i = end + 1;
    }
  }
  if (!params_ok || c->oom) {
    return;
  }
  for (uint32_t k = 0; k < r->nparams && !c->stop; k++) {
    const ozpi_node *e = overrides[k] != nullptr ? overrides[k] : r->defaults[k];
    value v = eval(c, e, 1);
    if (!bind(c, r->params[k], 0, v)) {
      return;
    }
  }
  if (!c->stop) {
    eval_block(c, r->body, 1, true);
  }
}
