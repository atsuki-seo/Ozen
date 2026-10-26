/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#include <string.h>

#include "sema.h"
#include "text.h"

constexpr ozni_index NONE = UINT32_MAX;

typedef struct unit_info {
  ozni_index name;
  ozni_index mod;
  ozni_index def;
  ozni_index file;
  ozni_index node;
  uint8_t state;
  bool ok;
  bool base;
  ozni_dimvec d;
  ozni_ival f;
} unit_info;

typedef struct set_item {
  ozni_index item;
  ozni_index args;
} set_item;

typedef enum axis_kind : uint8_t { AX_TAGGED, AX_ORDERED, AX_SET } axis_kind;

typedef struct axis_state {
  ozni_index axis;
  ozni_index file;
  ozni_index node;
  axis_kind kind;
  ozni_index value;
  ozni_index args;
  ozni_index style;
  ozni_table *set;
} axis_state;

typedef struct food {
  ozni_index type;
  ozni_index file;
  ozni_index node;
  ozni_index part;
  uint32_t naxes;
  axis_state *axes;
  ozni_qty amount;
  bool vague;
  bool consumed;
  bool portion;
  uint32_t consumed_group;
  uint32_t consumed_branch;
  ozni_index label;
  ozni_span label_span;
  ozni_table *derived;
} food;

typedef enum vkind : uint8_t { V_UNKNOWN, V_NONE, V_QTY, V_FOOD, V_ENUM, V_STR, V_BOOL, V_LIST, V_TUPLE } vkind;

typedef struct value {
  vkind kind;
  ozni_qty q;
  food *f;
  ozni_index name;
  ozni_index ref;
  ozni_table *items;
} value;

typedef struct binding {
  ozni_index name;
  value v;
} binding;

typedef struct sema {
  ozni_project *pr;
  ozni_ctx *c;
  ozni_table *dims;
  ozni_index base_unit[OZNI_MAX_DIMS];
  ozni_table *units;
  ozni_index unit_stack[256];
  uint32_t unit_depth;
  ozni_buf *out;
  bool steps;
  ozni_index mod;
  ozni_index file;
  ozni_table *scope;
  uint32_t step_no;
  bool failed;
  uint32_t group;
  uint32_t branch;
  uint32_t next_id;
  ozni_table *portions;
  value last;
  ozni_index last_node;
} sema;

static ozni_str S(const sema *s, ozni_index id) { return ozni_intern_get(s->c->names, id); }

static bool str_is(ozni_str x, const char *z) {
  size_t n = strlen(z);
  return x.len == n && memcmp(x.p, z, n) == 0;
}

static ozni_index intern(sema *s, const char *p, size_t n) {
  ozni_index id = 0;
  if (!ozni_intern_put(s->c->names, p, n, &id)) {
    s->c->oom = true;
  }
  return id;
}

static const ozni_node *nd(const sema *s, ozni_index file, ozni_index i) { return ozni_project_node(s->pr, file, i); }

static const ozni_module *mod_at(const sema *s, ozni_index m) { return ozni_project_module(s->pr, m); }

static const ozni_def *def_at(const sema *s, ozni_index m, ozni_index d) {
  return ozni_table_get(mod_at(s, m)->defs, d);
}

static void diag(sema *s, ozni_diag_id id, ozni_index file, uint32_t start, uint32_t end, const ozni_str *args,
                 uint16_t nargs) {
  if (!ozni_diag_add(s->c, id, (ozni_span){file, start, end}, 0, args, nargs)) {
    s->c->oom = true;
    return;
  }
  if (s->c->set.strict && id == OZNI_D_TYP_vagueAmount) {
    ozni_diag *d = ozni_table_at(s->c->diags, ozni_table_count(s->c->diags) - 1);
    d->severity = OZNI_SEV_ERROR;
  }
}

static void diag_node(sema *s, ozni_diag_id id, ozni_index file, const ozni_node *n, const ozni_str *args,
                      uint16_t nargs) {
  diag(s, id, file, n->span.start, n->span.end, args, nargs);
}

/* Rationals and intervals */

static bool rat_parse(const char *p, size_t n, ozni_rat *out) {
  int64_t num = 0, den = 1;
  size_t i = 0;
  bool frac = false, slash = false;
  int64_t part2 = 0;
  for (; i < n; i++) {
    char ch = p[i];
    if (ch == '.') {
      frac = true;
      continue;
    }
    if (ch == '/') {
      slash = true;
      continue;
    }
    if (ch < '0' || ch > '9') {
      return false;
    }
    int64_t dgt = ch - '0';
    if (slash) {
      if (part2 > (INT64_MAX - dgt) / 10) {
        return false;
      }
      part2 = part2 * 10 + dgt;
    } else {
      if (num > (INT64_MAX - dgt) / 10) {
        return false;
      }
      num = num * 10 + dgt;
      if (frac) {
        if (den > INT64_MAX / 10) {
          return false;
        }
        den *= 10;
      }
    }
  }
  if (slash) {
    den = part2;
  }
  return ozni_rat_make(num, den, out) == OZNI_OK;
}

static bool ival_parse(const char *p, size_t n, ozni_ival *out) {
  for (size_t i = 0; i + 1 < n; i++) {
    if (p[i] == '.' && p[i + 1] == '.') {
      return rat_parse(p, i, &out->lo) && rat_parse(p + i + 2, n - i - 2, &out->hi);
    }
  }
  if (!rat_parse(p, n, &out->lo)) {
    return false;
  }
  out->hi = out->lo;
  return true;
}

static bool ival_mul(ozni_ival a, ozni_ival b, ozni_ival *out) {
  ozni_rat c[4];
  if (ozni_rat_mul(a.lo, b.lo, &c[0]) != OZNI_OK || ozni_rat_mul(a.lo, b.hi, &c[1]) != OZNI_OK ||
      ozni_rat_mul(a.hi, b.lo, &c[2]) != OZNI_OK || ozni_rat_mul(a.hi, b.hi, &c[3]) != OZNI_OK) {
    return false;
  }
  ozni_rat lo = c[0], hi = c[0];
  for (int k = 1; k < 4; k++) {
    if (ozni_rat_cmp(c[k], lo) < 0) {
      lo = c[k];
    }
    if (ozni_rat_cmp(c[k], hi) > 0) {
      hi = c[k];
    }
  }
  *out = (ozni_ival){lo, hi};
  return true;
}

static bool ival_divide(ozni_ival a, ozni_ival b, ozni_ival *out) {
  if (ozni_rat_cmp(b.lo, ozni_rat_int(0)) <= 0 || ozni_rat_cmp(b.hi, ozni_rat_int(0)) <= 0) {
    return false;
  }
  ozni_ival inv;
  if (ozni_rat_div(ozni_rat_int(1), b.hi, &inv.lo) != OZNI_OK ||
      ozni_rat_div(ozni_rat_int(1), b.lo, &inv.hi) != OZNI_OK) {
    return false;
  }
  return ival_mul(a, inv, out);
}

static bool dims_equal(ozni_dimvec a, ozni_dimvec b) { return memcmp(a.e, b.e, sizeof a.e) == 0; }

static ozni_dimvec dims_combine(ozni_dimvec a, ozni_dimvec b, int sign) {
  ozni_dimvec r;
  for (int k = 0; k < OZNI_MAX_DIMS; k++) {
    r.e[k] = (int8_t)(a.e[k] + sign * b.e[k]);
  }
  return r;
}

static int dim_index(const sema *s, ozni_index name) {
  for (ozni_index k = 0; k < ozni_table_count(s->dims); k++) {
    if (*(const ozni_index *)ozni_table_get(s->dims, k) == name) {
      return (int)k;
    }
  }
  return -1;
}

static int dim_named(sema *s, const char *z) { return dim_index(s, intern(s, z, strlen(z))); }

/* Rendering */

static void put_rat(ozni_buf *b, ozni_rat r) {
  if (r.num < 0) {
    ozni_buf_puts(b, "-");
  }
  uint64_t num = r.num < 0 ? (uint64_t)0 - (uint64_t)r.num : (uint64_t)r.num;
  uint64_t den = (uint64_t)r.den;
  if (den == 1) {
    ozni_buf_putu(b, num);
    return;
  }
  uint64_t d = den;
  int twos = 0, fives = 0;
  while (d % 2 == 0) {
    d /= 2;
    twos++;
  }
  while (d % 5 == 0) {
    d /= 5;
    fives++;
  }
  int k = twos > fives ? twos : fives;
  if (d == 1 && k <= 18) {
    uint64_t scale = 1;
    for (int i = 0; i < k; i++) {
      scale *= 10;
    }
    ozni_u128 scaled = ozni_u128_mul64(num, scale / den);
    if (scaled.hi == 0) {
      uint64_t whole = scaled.lo / scale, frac = scaled.lo % scale;
      ozni_buf_putu(b, whole);
      if (frac != 0) {
        char digits[24];
        int n = 0;
        for (uint64_t sc = scale / 10; sc > 0; sc /= 10) {
          digits[n++] = (char)('0' + (frac / sc) % 10);
        }
        while (n > 0 && digits[n - 1] == '0') {
          n--;
        }
        ozni_buf_puts(b, ".");
        ozni_buf_put(b, digits, (size_t)n);
      }
      return;
    }
  }
  ozni_buf_putu(b, num);
  ozni_buf_puts(b, "/");
  ozni_buf_putu(b, den);
}

static void put_dims(sema *s, ozni_buf *b, ozni_dimvec d) {
  bool first = true;
  for (int pass = 0; pass < 2; pass++) {
    for (int k = 0; k < OZNI_MAX_DIMS; k++) {
      int e = d.e[k];
      if ((pass == 0 && e <= 0) || (pass == 1 && e >= 0)) {
        continue;
      }
      ozni_buf_puts(b, pass == 1 ? "/" : first ? "" : "*");
      first = false;
      ozni_index unit = s->base_unit[k];
      ozni_str name = unit != 0 ? S(s, unit) : S(s, *(const ozni_index *)ozni_table_get(s->dims, (ozni_index)k));
      ozni_buf_put(b, name.p, name.len);
      int mag = e < 0 ? -e : e;
      if (mag > 1) {
        ozni_buf_putu(b, (uint64_t)mag);
      }
    }
  }
}

static bool dimensionless(ozni_dimvec d) {
  for (int k = 0; k < OZNI_MAX_DIMS; k++) {
    if (d.e[k] != 0) {
      return false;
    }
  }
  return true;
}

static void put_qty(sema *s, ozni_buf *b, ozni_qty q) {
  put_rat(b, q.v.lo);
  if (ozni_rat_cmp(q.v.lo, q.v.hi) != 0) {
    ozni_buf_puts(b, "..");
    put_rat(b, q.v.hi);
  }
  if (!dimensionless(q.d)) {
    ozni_buf_puts(b, " ");
    put_dims(s, b, q.d);
  }
}

static ozni_index dim_name_text(sema *s, ozni_dimvec d) {
  if (dimensionless(d)) {
    return intern(s, "Dimensionless", 13);
  }
  char buf[128];
  size_t n = 0;
  bool first = true;
  for (int pass = 0; pass < 2; pass++) {
    for (int k = 0; k < OZNI_MAX_DIMS; k++) {
      int e = d.e[k];
      if ((pass == 0 && e <= 0) || (pass == 1 && e >= 0)) {
        continue;
      }
      const char *sep = pass == 1 ? "/" : first ? "" : "*";
      first = false;
      ozni_str name = S(s, *(const ozni_index *)ozni_table_get(s->dims, (ozni_index)k));
      if (n + strlen(sep) + name.len + 4 >= sizeof buf) {
        break;
      }
      ozni_copy(buf + n, sep, strlen(sep));
      n += strlen(sep);
      ozni_copy(buf + n, name.p, name.len);
      n += name.len;
    }
  }
  return intern(s, buf, n);
}

/* Units */

static unit_info *unit_for(sema *s, ozni_index mod, ozni_index def) {
  for (ozni_index k = 0; k < ozni_table_count(s->units); k++) {
    unit_info *u = ozni_table_at(s->units, k);
    if (u->mod == mod && u->def == def) {
      return u;
    }
  }
  return nullptr;
}

static bool unit_resolve(sema *s, unit_info *u);

static bool unit_lookup(sema *s, ozni_index mod, ozni_index name, unit_info **out) {
  ozni_index m, d;
  if (!ozni_project_find(s->pr, mod, name, &m, &d)) {
    return false;
  }
  unit_info *u = unit_for(s, m, d);
  if (u == nullptr) {
    return false;
  }
  *out = u;
  return true;
}

static bool unit_path(sema *s, ozni_index mod, ozni_str path, ozni_ival *f, ozni_dimvec *d, ozni_str *bad) {
  *f = (ozni_ival){ozni_rat_int(1), ozni_rat_int(1)};
  memset(d, 0, sizeof *d);
  if (str_is(path, "%")) {
    ozni_rat pct;
    if (ozni_rat_make(1, 100, &pct) != OZNI_OK) {
      return false;
    }
    *f = (ozni_ival){pct, pct};
    return true;
  }
  size_t start = 0;
  int sign = 1;
  for (size_t i = 0; i <= path.len; i++) {
    if (i < path.len && path.p[i] != '/') {
      continue;
    }
    ozni_index name = intern(s, path.p + start, i - start);
    unit_info *u = nullptr;
    if (!unit_lookup(s, mod, name, &u)) {
      *bad = (ozni_str){path.p + start, (uint32_t)(i - start)};
      return false;
    }
    if (!unit_resolve(s, u)) {
      return false;
    }
    ozni_ival nf;
    if (!(sign > 0 ? ival_mul(*f, u->f, &nf) : ival_divide(*f, u->f, &nf))) {
      return false;
    }
    *f = nf;
    *d = dims_combine(*d, u->d, sign);
    sign = -1;
    start = i + 1;
  }
  return true;
}

static bool qty_of(sema *s, ozni_index mod, ozni_index file, ozni_index node, ozni_qty *out, bool report) {
  const ozni_node *n = nd(s, file, node);
  *out = (ozni_qty){0};
  bool first = true;
  for (ozni_index c = n->first; c != 0; c = nd(s, file, c)->next) {
    const ozni_node *part = nd(s, file, c);
    if (part->kind != OZNI_N_QtyPart) {
      continue;
    }
    ozni_str amount = S(s, part->text), unit = S(s, part->text2);
    ozni_ival a, f;
    ozni_dimvec d;
    ozni_str bad = {"", 0};
    if (!ival_parse(amount.p, amount.len, &a)) {
      return false;
    }
    if (!unit_path(s, mod, unit, &f, &d, &bad)) {
      if (report && bad.len > 0) {
        diag_node(s, OZNI_D_NAM_unknownUnit, file, n, &bad, 1);
      }
      return false;
    }
    ozni_ival v;
    if (!ival_mul(a, f, &v)) {
      return false;
    }
    if (first) {
      out->v = v;
      out->d = d;
      first = false;
    } else if (!dims_equal(out->d, d)) {
      if (report) {
        ozni_str args[2] = {S(s, dim_name_text(s, out->d)), S(s, dim_name_text(s, d))};
        diag_node(s, OZNI_D_TYP_dimension, file, n, args, 2);
      }
      return false;
    } else {
      ozni_ival sum;
      if (ozni_ival_add(out->v, v, &sum) != OZNI_OK) {
        return false;
      }
      out->v = sum;
    }
  }
  out->known = !first;
  return out->known;
}

static unit_info *stack_unit(sema *s, uint32_t k) { return ozni_table_at(s->units, s->unit_stack[k]); }

static void report_cycle(sema *s, uint32_t from) {
  uint32_t n = s->unit_depth;
  uint32_t best = from;
  for (uint32_t k = from; k < n; k++) {
    unit_info *a = stack_unit(s, k), *b = stack_unit(s, best);
    if (a->file < b->file ||
        (a->file == b->file && nd(s, a->file, a->node)->span.start < nd(s, b->file, b->node)->span.start)) {
      best = k;
    }
  }
  uint32_t len = n - from;
  ozni_str args[32];
  uint16_t nargs = 0;
  for (uint32_t k = 0; k <= len && nargs < 32; k++) {
    args[nargs++] = S(s, stack_unit(s, from + (best - from + k) % len)->name);
  }
  unit_info *at = stack_unit(s, best);
  diag_node(s, OZNI_D_TYP_unitCycle, at->file, nd(s, at->file, at->node), args, nargs);
  for (uint32_t k = from; k < n; k++) {
    stack_unit(s, k)->ok = false;
    stack_unit(s, k)->state = 2;
  }
}

static bool unit_resolve(sema *s, unit_info *u) {
  if (u->state == 2) {
    return u->ok;
  }
  ozni_index self = 0;
  for (ozni_index k = 0; k < ozni_table_count(s->units); k++) {
    if (ozni_table_at(s->units, k) == u) {
      self = k;
    }
  }
  if (u->state == 1) {
    for (uint32_t k = 0; k < s->unit_depth; k++) {
      if (s->unit_stack[k] == self) {
        report_cycle(s, k);
        break;
      }
    }
    return false;
  }
  if (s->unit_depth >= 256) {
    return false;
  }
  u->state = 1;
  s->unit_stack[s->unit_depth++] = self;
  const ozni_node *n = nd(s, u->file, u->node);
  int dim = -1;
  ozni_index valnode = 0;
  for (ozni_index c = n->first; c != 0; c = nd(s, u->file, c)->next) {
    const ozni_node *cn = nd(s, u->file, c);
    if (cn->kind == OZNI_N_TypeRef) {
      dim = dim_index(s, cn->text);
    } else if (cn->kind == OZNI_N_Base || cn->kind == OZNI_N_Quantity) {
      valnode = c;
    }
  }
  bool ok = false;
  if (valnode != 0 && nd(s, u->file, valnode)->kind == OZNI_N_Base && dim >= 0) {
    u->base = true;
    memset(&u->d, 0, sizeof u->d);
    u->d.e[dim] = 1;
    u->f = (ozni_ival){ozni_rat_int(1), ozni_rat_int(1)};
    ok = true;
  } else if (valnode != 0 && nd(s, u->file, valnode)->kind == OZNI_N_Quantity) {
    ozni_qty q;
    if (qty_of(s, u->mod, u->file, valnode, &q, false)) {
      u->d = q.d;
      u->f = q.v;
      ok = true;
    }
  }
  s->unit_depth--;
  if (u->state != 2) {
    u->state = 2;
    u->ok = ok;
  }
  return u->ok;
}

static void build_units(sema *s) {
  ozni_index nm = ozni_table_count(s->pr->modules);
  for (int pass = 0; pass < 2; pass++) {
    for (ozni_index mi = 0; mi < nm; mi++) {
      const ozni_module *m = mod_at(s, mi);
      if (m->is_std != (pass == 0)) {
        continue;
      }
      for (ozni_index d = 0; d < ozni_table_count(m->defs); d++) {
        const ozni_def *e = ozni_table_get(m->defs, d);
        const ozni_node *n = nd(s, m->file, e->node);
        if (n->kind == OZNI_N_Name && ozni_table_count(s->dims) < OZNI_MAX_DIMS && dim_index(s, e->name) < 0) {
          if (!ozni_table_push(s->dims, &e->name, nullptr)) {
            s->c->oom = true;
          }
        } else if (n->kind == OZNI_N_Unit) {
          unit_info u = {e->name, mi, d, m->file, e->node, 0, false, false, {{0}}, {{0, 1}, {0, 1}}};
          if (!ozni_table_push(s->units, &u, nullptr)) {
            s->c->oom = true;
          }
        }
      }
    }
  }
  for (ozni_index k = 0; k < ozni_table_count(s->units); k++) {
    unit_info *u = ozni_table_at(s->units, k);
    unit_resolve(s, u);
    if (!u->base || !u->ok) {
      continue;
    }
    int dim = -1;
    for (int j = 0; j < OZNI_MAX_DIMS; j++) {
      if (u->d.e[j] == 1) {
        dim = j;
      }
    }
    if (dim < 0) {
      continue;
    }
    if (s->base_unit[dim] == 0) {
      s->base_unit[dim] = u->name;
    } else {
      ozni_str args[3] = {S(s, u->name), S(s, *(const ozni_index *)ozni_table_get(s->dims, (ozni_index)dim)),
                          S(s, s->base_unit[dim])};
      diag_node(s, OZNI_D_TYP_baseUnitChanged, u->file, nd(s, u->file, u->node), args, 3);
    }
  }
}

/* Definitions and foods */

static bool find_def(sema *s, ozni_index name, ozni_index *file, ozni_index *node) {
  ozni_index m, d;
  if (!ozni_project_find(s->pr, s->mod, name, &m, &d)) {
    return false;
  }
  *file = mod_at(s, m)->file;
  *node = def_at(s, m, d)->node;
  return true;
}

static ozni_index field_first(const sema *s, ozni_index file, ozni_index decl, const char *name) {
  for (ozni_index c = nd(s, file, decl)->first; c != 0; c = nd(s, file, c)->next) {
    const ozni_node *cn = nd(s, file, c);
    if (cn->kind == OZNI_N_Field && str_is(S(s, cn->text), name)) {
      return c;
    }
  }
  return 0;
}

static ozni_index section_field(const sema *s, ozni_index file, ozni_index decl, const char *section,
                                const char *name) {
  for (ozni_index c = nd(s, file, decl)->first; c != 0; c = nd(s, file, c)->next) {
    const ozni_node *cn = nd(s, file, c);
    if (cn->kind == OZNI_N_Section && str_is(S(s, cn->text), section)) {
      return field_first(s, file, c, name);
    }
  }
  return 0;
}

static food *food_new(sema *s, ozni_index type) {
  food *f = ozni_arena_alloc(s->c->arena, sizeof *f);
  if (f == nullptr) {
    s->c->oom = true;
    return nullptr;
  }
  f->type = type;
  ozni_index file, node;
  if (!find_def(s, type, &file, &node) || nd(s, file, node)->kind != OZNI_N_Ingredient) {
    f->file = NONE;
    return f;
  }
  f->file = file;
  f->node = node;
  ozni_index axes = field_first(s, file, node, "axes");
  uint32_t count = 0;
  for (ozni_index v = axes != 0 ? nd(s, file, axes)->first : 0; v != 0; v = nd(s, file, v)->next) {
    count += nd(s, file, v)->kind == OZNI_N_Name;
  }
  f->axes = ozni_arena_alloc(s->c->arena, (count + 1) * sizeof *f->axes);
  if (f->axes == nullptr) {
    s->c->oom = true;
    return f;
  }
  ozni_index initial = field_first(s, file, node, "initial");
  ozni_index init_rec = initial != 0 ? nd(s, file, initial)->first : 0;
  for (ozni_index v = axes != 0 ? nd(s, file, axes)->first : 0; v != 0; v = nd(s, file, v)->next) {
    const ozni_node *an = nd(s, file, v);
    if (an->kind != OZNI_N_Name) {
      continue;
    }
    ozni_index afile, anode;
    axis_state *st = &f->axes[f->naxes];
    *st = (axis_state){an->text, NONE, 0, AX_TAGGED, 0, 0, 0, nullptr};
    if (!find_def(s, an->text, &afile, &anode) || nd(s, afile, anode)->kind != OZNI_N_Axis) {
      continue;
    }
    st->file = afile;
    st->node = anode;
    ozni_str kind = S(s, nd(s, afile, anode)->text2);
    st->kind = str_is(kind, "ordered") ? AX_ORDERED : str_is(kind, "set") ? AX_SET : AX_TAGGED;
    if (st->kind == AX_SET) {
      st->set = ozni_table_new(s->c->arena, sizeof(set_item));
    } else {
      for (ozni_index val = nd(s, afile, anode)->first; val != 0; val = nd(s, afile, val)->next) {
        if (nd(s, afile, val)->kind == OZNI_N_Value) {
          st->value = nd(s, afile, val)->text;
          break;
        }
      }
    }
    if (init_rec != 0 && nd(s, file, init_rec)->kind == OZNI_N_Record) {
      for (ozni_index e = nd(s, file, init_rec)->first; e != 0; e = nd(s, file, e)->next) {
        const ozni_node *en = nd(s, file, e);
        if (en->text == an->text && en->first != 0 && nd(s, file, en->first)->kind == OZNI_N_Name) {
          st->value = nd(s, file, en->first)->text;
        }
      }
    }
    f->naxes++;
  }
  return f;
}

static food *food_copy(sema *s, const food *src) {
  food *f = ozni_arena_alloc(s->c->arena, sizeof *f);
  if (f == nullptr) {
    s->c->oom = true;
    return nullptr;
  }
  *f = *src;
  f->consumed = false;
  f->portion = false;
  f->label = 0;
  f->axes = ozni_arena_alloc(s->c->arena, (src->naxes + 1) * sizeof *f->axes);
  if (f->axes == nullptr) {
    s->c->oom = true;
    return f;
  }
  for (uint32_t k = 0; k < src->naxes; k++) {
    f->axes[k] = src->axes[k];
    if (src->axes[k].set != nullptr) {
      f->axes[k].set = ozni_table_new(s->c->arena, sizeof(set_item));
      for (ozni_index i = 0; f->axes[k].set != nullptr && i < ozni_table_count(src->axes[k].set); i++) {
        if (!ozni_table_push(f->axes[k].set, ozni_table_get(src->axes[k].set, i), nullptr)) {
          s->c->oom = true;
        }
      }
    }
  }
  return f;
}

static ozni_index initial_value(const sema *s, const axis_state *st) {
  if (st->file == NONE) {
    return 0;
  }
  for (ozni_index val = nd(s, st->file, st->node)->first; val != 0; val = nd(s, st->file, val)->next) {
    if (nd(s, st->file, val)->kind == OZNI_N_Value) {
      return nd(s, st->file, val)->text;
    }
  }
  return 0;
}

static void put_food_type(sema *s, ozni_buf *b, const food *f) {
  ozni_str t = S(s, f->type);
  ozni_buf_put(b, t.p, t.len);
  bool open = false;
  ozni_index base_init = 0;
  ozni_index init_rec = 0;
  if (f->file != NONE) {
    ozni_index initial = field_first(s, f->file, f->node, "initial");
    init_rec = initial != 0 ? nd(s, f->file, initial)->first : 0;
  }
  if (f->part != 0) {
    ozni_buf_puts(b, "<");
    ozni_str p = S(s, f->part);
    ozni_buf_put(b, p.p, p.len);
    open = true;
  }
  for (uint32_t k = 0; k < f->naxes; k++) {
    const axis_state *st = &f->axes[k];
    ozni_str axis = S(s, st->axis);
    if (st->kind == AX_SET) {
      for (ozni_index i = 0; st->set != nullptr && i < ozni_table_count(st->set); i++) {
        const set_item *it = ozni_table_get(st->set, i);
        ozni_buf_puts(b, open ? ", " : "<");
        open = true;
        ozni_buf_put(b, axis.p, axis.len);
        ozni_buf_puts(b, " has ");
        ozni_str name = S(s, it->item);
        ozni_buf_put(b, name.p, name.len);
        if (it->args != 0) {
          ozni_str a = S(s, it->args);
          ozni_buf_put(b, a.p, a.len);
        }
      }
      continue;
    }
    base_init = initial_value(s, st);
    if (init_rec != 0 && nd(s, f->file, init_rec)->kind == OZNI_N_Record) {
      for (ozni_index e = nd(s, f->file, init_rec)->first; e != 0; e = nd(s, f->file, e)->next) {
        const ozni_node *en = nd(s, f->file, e);
        if (en->text == st->axis && en->first != 0) {
          base_init = nd(s, f->file, en->first)->text;
        }
      }
    }
    if (st->value == base_init && st->args == 0) {
      continue;
    }
    ozni_buf_puts(b, open ? ", " : "<");
    open = true;
    ozni_buf_put(b, axis.p, axis.len);
    ozni_buf_puts(b, ": ");
    ozni_str v = S(s, st->value);
    ozni_buf_put(b, v.p, v.len);
    if (st->args != 0) {
      ozni_str a = S(s, st->args);
      ozni_buf_put(b, a.p, a.len);
    }
  }
  if (open) {
    ozni_buf_puts(b, ">");
  }
}

static void put_value(sema *s, ozni_buf *b, const value *v) {
  if (v->ref != 0) {
    ozni_str r = S(s, v->ref);
    ozni_buf_put(b, r.p, r.len);
  } else if (v->kind == V_QTY && v->q.known) {
    put_qty(s, b, v->q);
  } else if (v->kind == V_ENUM || v->kind == V_STR) {
    ozni_str n = S(s, v->name);
    ozni_buf_put(b, n.p, n.len);
  } else if (v->kind == V_LIST) {
    ozni_buf_puts(b, "[");
    for (ozni_index i = 0; v->items != nullptr && i < ozni_table_count(v->items); i++) {
      ozni_buf_puts(b, i == 0 ? "" : ", ");
      put_value(s, b, ozni_table_get(v->items, i));
    }
    ozni_buf_puts(b, "]");
  } else {
    ozni_buf_puts(b, "?");
  }
}

static void put_food_line(sema *s, ozni_buf *b, const food *f) {
  put_food_type(s, b, f);
  if (f->vague) {
    ozni_buf_puts(b, " [any amount]");
  } else if (f->amount.known) {
    ozni_buf_puts(b, " ");
    put_qty(s, b, f->amount);
  }
  ozni_index n = f->derived != nullptr ? ozni_table_count(f->derived) : 0;
  ozni_index order[32];
  for (ozni_index k = 0; k < n && k < 32; k++) {
    order[k] = *(const ozni_index *)ozni_table_get(f->derived, k);
    ozni_index j = k;
    while (j > 0) {
      ozni_str x = S(s, order[j - 1]), y = S(s, order[j]);
      size_t m = x.len < y.len ? x.len : y.len;
      int r = memcmp(x.p, y.p, m);
      if (r < 0 || (r == 0 && x.len <= y.len)) {
        break;
      }
      ozni_index tmp = order[j - 1];
      order[j - 1] = order[j];
      order[j] = tmp;
      j--;
    }
  }
  for (ozni_index k = 0; k < n && k < 32; k++) {
    ozni_buf_puts(b, k == 0 ? " with " : ", ");
    ozni_str tr = S(s, order[k]);
    ozni_buf_put(b, tr.p, tr.len);
  }
}

/* Amounts */

static bool food_prop(sema *s, const food *f, const char *key, ozni_qty *out) {
  if (f->file == NONE) {
    return false;
  }
  ozni_index field = section_field(s, f->file, f->node, "props", key);
  if (field == 0 || nd(s, f->file, field)->first == 0 || nd(s, f->file, nd(s, f->file, field)->first)->kind != OZNI_N_Quantity) {
    return false;
  }
  ozni_index saved = s->mod;
  for (ozni_index mi = 0; mi < ozni_table_count(s->pr->modules); mi++) {
    if (mod_at(s, mi)->file == f->file) {
      s->mod = mi;
    }
  }
  bool ok = qty_of(s, s->mod, f->file, nd(s, f->file, field)->first, out, false);
  s->mod = saved;
  return ok;
}

static bool to_mass(sema *s, const food *f, ozni_qty *out) {
  int mass = dim_named(s, "Mass"), volume = dim_named(s, "Volume"), count = dim_named(s, "Count");
  if (!f->amount.known || mass < 0) {
    return false;
  }
  ozni_dimvec want = {{0}};
  want.e[mass] = 1;
  if (dims_equal(f->amount.d, want)) {
    *out = f->amount;
    return true;
  }
  const char *key = nullptr;
  if (volume >= 0 && f->amount.d.e[volume] == 1) {
    key = "density";
  } else if (count >= 0 && f->amount.d.e[count] == 1) {
    key = "unitWeight";
  }
  ozni_qty k;
  if (key == nullptr || !food_prop(s, f, key, &k)) {
    return false;
  }
  ozni_dimvec d = dims_combine(f->amount.d, k.d, 1);
  if (!dims_equal(d, want)) {
    return false;
  }
  ozni_ival v;
  if (!ival_mul(f->amount.v, k.v, &v)) {
    return false;
  }
  *out = (ozni_qty){v, want, true};
  return true;
}

/* Steps */

static ozni_index step_ref(sema *s) {
  char buf[24];
  size_t n = 0;
  char rev[24];
  uint32_t v = s->step_no;
  do {
    rev[n++] = (char)('0' + v % 10);
    v /= 10;
  } while (v != 0);
  buf[0] = '%';
  for (size_t k = 0; k < n; k++) {
    buf[k + 1] = rev[n - 1 - k];
  }
  return intern(s, buf, n + 1);
}

static ozni_index record_step(sema *s, ozni_index action, const value *args, uint32_t nargs, const ozni_index *names,
                              const food *out) {
  s->step_no++;
  ozni_index ref = step_ref(s);
  if (s->steps && s->out != nullptr) {
    ozni_buf *b = s->out;
    ozni_buf_puts(b, "  ");
    ozni_str r = S(s, ref);
    ozni_buf_put(b, r.p, r.len);
    ozni_buf_puts(b, " = ");
    ozni_str a = S(s, action);
    ozni_buf_put(b, a.p, a.len);
    ozni_buf_puts(b, "(");
    for (uint32_t k = 0; k < nargs; k++) {
      ozni_buf_puts(b, k == 0 ? "" : ", ");
      if (names != nullptr && names[k] != 0) {
        ozni_str nm = S(s, names[k]);
        ozni_buf_put(b, nm.p, nm.len);
        ozni_buf_puts(b, ": ");
      }
      put_value(s, b, &args[k]);
    }
    ozni_buf_puts(b, ")");
    if (out != nullptr) {
      ozni_buf_puts(b, " : ");
      put_food_line(s, b, out);
    }
    ozni_buf_puts(b, "\n");
  }
  return ref;
}

/* Evaluation */

static value eval(sema *s, ozni_index node);
static int axis_value_index(const sema *s, const axis_state *st, ozni_index name);
static bool food_has(sema *s, const food *f, ozni_index name);
static value eval_split(sema *s, const ozni_node *call);

static value unknown(void) { return (value){.kind = V_UNKNOWN}; }

static bool lookup_local(const sema *s, ozni_index name, value *out) {
  for (ozni_index k = ozni_table_count(s->scope); k > 0; k--) {
    const binding *b = ozni_table_get(s->scope, k - 1);
    if (b->name == name) {
      *out = b->v;
      return true;
    }
  }
  return false;
}

static void bind(sema *s, ozni_index name, value v) {
  binding b = {name, v};
  if (!ozni_table_push(s->scope, &b, nullptr)) {
    s->c->oom = true;
  }
}

static ozni_str src_text(const sema *s, ozni_index file, const ozni_node *n) {
  const ozni_source *src = &s->c->srcs[file];
  return (ozni_str){src->text + n->span.start, n->span.end - n->span.start};
}

static void consume(sema *s, const value *v, ozni_index file, ozni_index argnode) {
  if (v->kind != V_FOOD || v->f == nullptr) {
    return;
  }
  food *f = v->f;
  if (f->consumed) {
    if (argnode == 0) {
      return;
    }
    const ozni_node *n = nd(s, file, argnode);
    ozni_str arg = src_text(s, file, n);
    bool parallel = s->group != 0 && f->consumed_group == s->group && f->consumed_branch != s->branch;
    diag_node(s, parallel ? OZNI_D_LIN_parallelConsume : OZNI_D_LIN_doubleUse, file, n, &arg, 1);
    return;
  }
  f->consumed = true;
  f->consumed_group = s->group;
  f->consumed_branch = s->branch;
}

static void add_derived(sema *s, food *f, ozni_index trait) {
  if (f->derived == nullptr) {
    f->derived = ozni_table_new(s->c->arena, sizeof(ozni_index));
    if (f->derived == nullptr) {
      s->c->oom = true;
      return;
    }
  }
  for (ozni_index k = 0; k < ozni_table_count(f->derived); k++) {
    if (*(const ozni_index *)ozni_table_get(f->derived, k) == trait) {
      return;
    }
  }
  if (!ozni_table_push(f->derived, &trait, nullptr)) {
    s->c->oom = true;
  }
}

static void inherit_traits(sema *s, food *into, const food *from) {
  ozni_index hazard = intern(s, "Hazard", 6);
  if (food_has(s, from, hazard)) {
    add_derived(s, into, hazard);
  }
  for (ozni_index k = 0; from->derived != nullptr && k < ozni_table_count(from->derived); k++) {
    ozni_index tr = *(const ozni_index *)ozni_table_get(from->derived, k);
    ozni_str ts = S(s, tr);
    if (ts.len >= 9 && memcmp(ts.p, "Allergen(", 9) == 0) {
      add_derived(s, into, tr);
    }
  }
  if (from->file == NONE) {
    return;
  }
  for (ozni_index c = nd(s, from->file, from->node)->first; c != 0; c = nd(s, from->file, c)->next) {
    const ozni_node *cn = nd(s, from->file, c);
    if (cn->kind == OZNI_N_TypeRef && str_is(S(s, cn->text), "Allergen")) {
      ozni_str text = src_text(s, from->file, cn);
      add_derived(s, into, intern(s, text.p, text.len));
    }
  }
}

static value combine(sema *s, const value *items, uint32_t n, ozni_index file, const ozni_node *at) {
  food *mix = food_new(s, intern(s, "Mixture", 7));
  if (mix == nullptr) {
    return unknown();
  }
  ozni_qty total = {0};
  bool all_known = true, same = true;
  for (uint32_t k = 0; k < n; k++) {
    const food *f = items[k].kind == V_FOOD ? items[k].f : nullptr;
    if (f == nullptr || !f->amount.known) {
      all_known = false;
      continue;
    }
    if (k > 0 && items[0].kind == V_FOOD && items[0].f->amount.known && !dims_equal(items[0].f->amount.d, f->amount.d)) {
      same = false;
    }
  }
  bool ok = all_known;
  bool first = true;
  for (uint32_t k = 0; k < n && ok; k++) {
    ozni_qty m;
    if (!to_mass(s, items[k].f, &m)) {
      if (!same) {
        ozni_str arg = S(s, items[k].f->type);
        diag_node(s, OZNI_D_TYP_densityMissing, file, at, &arg, 1);
        ok = false;
        break;
      }
      ok = false;
      break;
    }
    if (first) {
      total = m;
      first = false;
    } else {
      ozni_ival sum;
      if (ozni_ival_add(total.v, m.v, &sum) != OZNI_OK) {
        ok = false;
        break;
      }
      total.v = sum;
    }
  }
  if (!ok && all_known && same && n > 0) {
    total = items[0].f->amount;
    ok = true;
    for (uint32_t k = 1; k < n; k++) {
      ozni_ival sum;
      if (ozni_ival_add(total.v, items[k].f->amount.v, &sum) != OZNI_OK) {
        ok = false;
        break;
      }
      total.v = sum;
    }
  }
  if (ok && n > 0) {
    mix->amount = total;
  }
  int min_done = -1;
  axis_state *mix_done = nullptr;
  for (uint32_t k = 0; k < mix->naxes; k++) {
    if (str_is(S(s, mix->axes[k].axis), "doneness")) {
      mix_done = &mix->axes[k];
    }
  }
  for (uint32_t k = 0; k < n; k++) {
    const food *f = items[k].kind == V_FOOD ? items[k].f : nullptr;
    if (f == nullptr) {
      continue;
    }
    inherit_traits(s, mix, f);
    for (uint32_t a = 0; mix_done != nullptr && a < f->naxes; a++) {
      if (f->axes[a].axis == mix_done->axis) {
        int idx = axis_value_index(s, mix_done, f->axes[a].value);
        if (idx >= 0 && (min_done < 0 || idx < min_done)) {
          min_done = idx;
          mix_done->value = f->axes[a].value;
        }
      }
    }
  }
  ozni_index item_node = at->kind == OZNI_N_List ? at->first : 0;
  for (uint32_t k = 0; k < n; k++) {
    while (item_node != 0 && nd(s, file, item_node)->kind == OZNI_N_Annotation) {
      item_node = nd(s, file, item_node)->next;
    }
    consume(s, &items[k], file, item_node != 0 ? item_node : 0);
    if (item_node != 0) {
      item_node = nd(s, file, item_node)->next;
    }
  }
  value out = {.kind = V_FOOD, .f = mix};
  value list = {.kind = V_LIST};
  list.items = ozni_table_new(s->c->arena, sizeof(value));
  for (uint32_t k = 0; list.items != nullptr && k < n; k++) {
    if (!ozni_table_push(list.items, &items[k], nullptr)) {
      s->c->oom = true;
    }
  }
  out.ref = record_step(s, intern(s, "combine", 7), &list, 1, nullptr, mix);
  return out;
}

static ozni_index type_var_of(const sema *s, ozni_index file, ozni_index type_node) {
  const ozni_node *t = nd(s, file, type_node);
  if (t->kind != OZNI_N_TypeRef) {
    return 0;
  }
  ozni_str name = S(s, t->text);
  return name.len == 1 ? t->text : 0;
}

static int axis_value_index(const sema *s, const axis_state *st, ozni_index name) {
  if (st->file == NONE) {
    return -1;
  }
  int k = 0;
  for (ozni_index val = nd(s, st->file, st->node)->first; val != 0; val = nd(s, st->file, val)->next) {
    if (nd(s, st->file, val)->kind != OZNI_N_Value) {
      continue;
    }
    if (nd(s, st->file, val)->text == name) {
      return k;
    }
    k++;
  }
  return -1;
}

static ozni_index axis_value_node(const sema *s, const axis_state *st, ozni_index name) {
  for (ozni_index val = st->file != NONE ? nd(s, st->file, st->node)->first : 0; val != 0;
       val = nd(s, st->file, val)->next) {
    if (nd(s, st->file, val)->kind == OZNI_N_Value && nd(s, st->file, val)->text == name) {
      return val;
    }
  }
  return 0;
}

static bool finer_enum(sema *s, const axis_state *st, ozni_index val, ozni_index *efile, ozni_index *enode) {
  ozni_index vn = axis_value_node(s, st, val);
  if (vn == 0) {
    return false;
  }
  ozni_index param = nd(s, st->file, vn)->first;
  if (param == 0 || nd(s, st->file, param)->kind != OZNI_N_Param || nd(s, st->file, param)->first == 0) {
    return false;
  }
  const ozni_node *pt = nd(s, st->file, nd(s, st->file, param)->first);
  ozni_index saved = s->mod;
  for (ozni_index mi = 0; mi < ozni_table_count(s->pr->modules); mi++) {
    if (mod_at(s, mi)->file == st->file) {
      s->mod = mi;
    }
  }
  bool ok = pt->kind == OZNI_N_TypeRef && find_def(s, pt->text, efile, enode) && nd(s, *efile, *enode)->kind == OZNI_N_Enum;
  s->mod = saved;
  if (!ok) {
    return false;
  }
  for (ozni_index c = nd(s, *efile, *enode)->first; c != 0; c = nd(s, *efile, c)->next) {
    if (nd(s, *efile, c)->kind == OZNI_N_Finer) {
      return true;
    }
  }
  return false;
}

static bool finer_reach(const sema *s, ozni_index efile, ozni_index enode, ozni_index from, ozni_index to) {
  ozni_index todo[64], seen[64];
  uint32_t nt = 0, ns = 0;
  todo[nt++] = from;
  while (nt > 0) {
    ozni_index cur = todo[--nt];
    for (ozni_index c = nd(s, efile, enode)->first; c != 0; c = nd(s, efile, c)->next) {
      const ozni_node *fn = nd(s, efile, c);
      if (fn->kind != OZNI_N_Finer || fn->text != cur) {
        continue;
      }
      for (ozni_index x = fn->first; x != 0; x = nd(s, efile, x)->next) {
        ozni_index target = nd(s, efile, x)->text;
        if (target == to) {
          return true;
        }
        bool dup = false;
        for (uint32_t k = 0; k < ns; k++) {
          dup = dup || seen[k] == target;
        }
        if (!dup && ns < 64 && nt < 64) {
          seen[ns++] = target;
          todo[nt++] = target;
        }
      }
    }
  }
  return false;
}

static bool decl_has_trait(sema *s, ozni_index file, ozni_index decl, ozni_index name, uint32_t depth) {
  if (depth > 32) {
    return false;
  }
  for (ozni_index c = nd(s, file, decl)->first; c != 0; c = nd(s, file, c)->next) {
    const ozni_node *cn = nd(s, file, c);
    if (cn->kind != OZNI_N_TypeRef) {
      continue;
    }
    if (cn->text == name) {
      return true;
    }
    ozni_index tfile, tnode;
    ozni_index saved = s->mod;
    for (ozni_index mi = 0; mi < ozni_table_count(s->pr->modules); mi++) {
      if (mod_at(s, mi)->file == file) {
        s->mod = mi;
      }
    }
    bool found = find_def(s, cn->text, &tfile, &tnode);
    s->mod = saved;
    if (found && nd(s, tfile, tnode)->kind == OZNI_N_Trait && decl_has_trait(s, tfile, tnode, name, depth + 1)) {
      return true;
    }
  }
  return false;
}

static bool food_has(sema *s, const food *f, ozni_index name) {
  if (f->part == name || f->type == name) {
    return true;
  }
  for (ozni_index k = 0; f->derived != nullptr && k < ozni_table_count(f->derived); k++) {
    if (*(const ozni_index *)ozni_table_get(f->derived, k) == name) {
      return true;
    }
  }
  return f->file != NONE && decl_has_trait(s, f->file, f->node, name, 0);
}

static bool satisfies_alt(sema *s, const food *f, ozni_index afile, ozni_index alt) {
  for (ozni_index a = nd(s, afile, alt)->first; a != 0; a = nd(s, afile, a)->next) {
    const ozni_node *an = nd(s, afile, a);
    if (an->kind == OZNI_N_BareArg) {
      if (!food_has(s, f, an->text)) {
        return false;
      }
      continue;
    }
    const axis_state *st = nullptr;
    for (uint32_t k = 0; k < f->naxes; k++) {
      if (f->axes[k].axis == an->text) {
        st = &f->axes[k];
      }
    }
    if (st == nullptr || an->first == 0) {
      return false;
    }
    const ozni_node *v = nd(s, afile, an->first);
    ozni_index want = v->kind == OZNI_N_Call && v->first != 0 ? nd(s, afile, v->first)->text : v->text;
    ozni_str op = S(s, an->text2);
    if (str_is(op, "has")) {
      bool found = false;
      for (ozni_index i = 0; st->set != nullptr && i < ozni_table_count(st->set); i++) {
        found = found || ((const set_item *)ozni_table_get(st->set, i))->item == want;
      }
      if (!found) {
        return false;
      }
    } else if (str_is(op, ":")) {
      if (st->kind == AX_SET || st->value != want) {
        return false;
      }
    } else {
      int cur = axis_value_index(s, st, st->value), w = axis_value_index(s, st, want);
      if (cur < 0 || w < 0 || (str_is(op, ">=") ? cur < w : cur > w)) {
        return false;
      }
    }
  }
  return true;
}

static bool satisfies(sema *s, const food *f, ozni_index afile, ozni_index type) {
  const ozni_node *t = nd(s, afile, type);
  if (t->kind != OZNI_N_TypeRef) {
    return true;
  }
  if (S(s, t->text).len > 1 && t->text != f->type) {
    return false;
  }
  for (ozni_index ta = t->first; ta != 0; ta = nd(s, afile, ta)->next) {
    if (nd(s, afile, ta)->kind != OZNI_N_TypeArgs) {
      continue;
    }
    for (ozni_index alt = nd(s, afile, ta)->first; alt != 0; alt = nd(s, afile, alt)->next) {
      if (satisfies_alt(s, f, afile, alt)) {
        return true;
      }
    }
    return false;
  }
  return true;
}

static void apply_effects(sema *s, food *f, ozni_index afile, ozni_index output_type, const value *params,
                          const ozni_index *pnames, uint32_t np, ozni_index cfile, const ozni_node *call) {
  const ozni_node *t = nd(s, afile, output_type);
  for (ozni_index ta = t->first; ta != 0; ta = nd(s, afile, ta)->next) {
    if (nd(s, afile, ta)->kind != OZNI_N_TypeArgs) {
      continue;
    }
    ozni_index alt = nd(s, afile, ta)->first;
    for (ozni_index a = alt != 0 ? nd(s, afile, alt)->first : 0; a != 0; a = nd(s, afile, a)->next) {
      const ozni_node *an = nd(s, afile, a);
      if (an->kind != OZNI_N_StateArg || an->first == 0) {
        continue;
      }
      axis_state *st = nullptr;
      for (uint32_t k = 0; k < f->naxes; k++) {
        if (f->axes[k].axis == an->text) {
          st = &f->axes[k];
        }
      }
      if (st == nullptr) {
        continue;
      }
      const ozni_node *v = nd(s, afile, an->first);
      ozni_index name = 0, args = 0, style = 0;
      if (v->kind == OZNI_N_Name) {
        name = v->text;
      } else if (v->kind == OZNI_N_Call && v->first != 0) {
        name = nd(s, afile, v->first)->text;
        ozni_index first_arg = nd(s, afile, v->first)->next;
        if (first_arg != 0 && nd(s, afile, first_arg)->kind == OZNI_N_Name) {
          style = nd(s, afile, first_arg)->text;
          for (uint32_t k = 0; k < np; k++) {
            if (pnames[k] == style) {
              style = params[k].kind == V_ENUM ? params[k].name : 0;
            }
          }
        }
        ozni_buf b = {.a = s->c->allocator};
        ozni_buf_puts(&b, "(");
        bool first = true;
        for (ozni_index arg = nd(s, afile, v->first)->next; arg != 0; arg = nd(s, afile, arg)->next) {
          const ozni_node *argn = nd(s, afile, arg);
          ozni_buf_puts(&b, first ? "" : ", ");
          first = false;
          bool shown = false;
          for (uint32_t k = 0; argn->kind == OZNI_N_Name && k < np; k++) {
            if (pnames[k] == argn->text) {
              put_value(s, &b, &params[k]);
              shown = true;
            }
          }
          if (!shown && argn->kind == OZNI_N_Name) {
            ozni_str an_text = S(s, argn->text);
            ozni_buf_put(&b, an_text.p, an_text.len);
          }
        }
        ozni_buf_puts(&b, ")");
        if (!b.failed) {
          args = intern(s, b.p, b.len);
        } else {
          s->c->oom = true;
        }
        ozni_buf_discard(&b);
      }
      if (name == 0) {
        continue;
      }
      ozni_str op = S(s, an->text2);
      if (st->kind == AX_SET && str_is(op, "has")) {
        bool updated = false;
        for (ozni_index i = 0; st->set != nullptr && i < ozni_table_count(st->set); i++) {
          set_item *it = ozni_table_at(st->set, i);
          if (it->item == name) {
            it->args = args;
            updated = true;
          }
        }
        set_item it = {name, args};
        if (!updated && st->set != nullptr && !ozni_table_push(st->set, &it, nullptr)) {
          s->c->oom = true;
        }
      } else if (st->kind == AX_ORDERED && str_is(op, ":")) {
        int cur = -1, want = -1, k = 0;
        for (ozni_index val = nd(s, st->file, st->node)->first; val != 0; val = nd(s, st->file, val)->next) {
          if (nd(s, st->file, val)->kind != OZNI_N_Value) {
            continue;
          }
          if (nd(s, st->file, val)->text == st->value) {
            cur = k;
          }
          if (nd(s, st->file, val)->text == name) {
            want = k;
          }
          k++;
        }
        if (want >= cur) {
          st->value = name;
        }
      } else if (str_is(op, ":")) {
        ozni_index efile, enode;
        if (finer_enum(s, st, name, &efile, &enode) && style != 0) {
          int cur = axis_value_index(s, st, st->value), want = axis_value_index(s, st, name);
          if (st->value == name && st->style != 0 && !finer_reach(s, efile, enode, st->style, style)) {
            ozni_str a2[2] = {S(s, st->style), S(s, style)};
            diag_node(s, OZNI_D_TYP_cutCoarser, cfile, call, a2, 2);
          } else if (cur > want && want >= 0) {
            ozni_str a2[2] = {S(s, st->value), S(s, style)};
            diag_node(s, OZNI_D_TYP_cutCoarser, cfile, call, a2, 2);
          }
        }
        st->value = name;
        st->args = args;
        st->style = style;
      }
    }
  }
}

static value call_action(sema *s, ozni_index file, const ozni_node *call, ozni_index afile, ozni_index anode) {
  const ozni_node *act = nd(s, afile, anode);
  value params[32];
  ozni_index pnames[32], pnodes[32], argnodes[32];
  bool set[32];
  uint32_t np = 0;
  for (ozni_index c = act->first; c != 0 && np < 32; c = nd(s, afile, c)->next) {
    if (nd(s, afile, c)->kind == OZNI_N_Param) {
      pnames[np] = nd(s, afile, c)->text;
      pnodes[np] = c;
      set[np] = false;
      argnodes[np] = 0;
      params[np++] = unknown();
    }
  }
  uint32_t pos = 0;
  value shown[32];
  ozni_index shown_names[32];
  uint32_t shown_of[32];
  uint32_t nshown = 0;
  for (uint32_t k = 0; k < 32; k++) {
    shown_of[k] = UINT32_MAX;
  }
  for (ozni_index arg = nd(s, file, call->first)->next; arg != 0; arg = nd(s, file, arg)->next) {
    const ozni_node *an = nd(s, file, arg);
    if (an->kind == OZNI_N_Annotation) {
      continue;
    }
    ozni_index name = 0;
    value v;
    if (an->kind == OZNI_N_NamedArg) {
      name = an->text;
      v = an->first != 0 ? eval(s, an->first) : unknown();
      for (uint32_t k = 0; k < np; k++) {
        if (pnames[k] == name) {
          params[k] = v;
          set[k] = true;
          shown_of[k] = nshown;
          argnodes[k] = an->first;
        }
      }
    } else {
      v = eval(s, arg);
      while (pos < np && set[pos]) {
        pos++;
      }
      if (pos < np) {
        params[pos] = v;
        set[pos] = true;
        shown_of[pos] = nshown;
        argnodes[pos] = arg;
        pos++;
      }
    }
    if (nshown < 32) {
      shown_names[nshown] = name;
      shown[nshown++] = v;
    }
  }
  for (uint32_t k = 0; k < np; k++) {
    if (set[k]) {
      continue;
    }
    for (ozni_index c = nd(s, afile, pnodes[k])->first; c != 0; c = nd(s, afile, c)->next) {
      if (nd(s, afile, c)->kind == OZNI_N_Default && nd(s, afile, c)->first != 0) {
        ozni_index saved_file = s->file;
        s->file = afile;
        params[k] = eval(s, nd(s, afile, c)->first);
        s->file = saved_file;
      }
    }
  }
  for (uint32_t k = 0; k < np; k++) {
    if (params[k].kind == V_LIST) {
      uint32_t n = ozni_table_count(params[k].items);
      value items[64];
      for (uint32_t i = 0; i < n && i < 64; i++) {
        items[i] = *(const value *)ozni_table_get(params[k].items, i);
      }
      bool foods = true;
      for (uint32_t i = 0; i < n && i < 64; i++) {
        foods = foods && items[i].kind == V_FOOD;
      }
      ozni_index ptype = 0;
      for (ozni_index c = nd(s, afile, pnodes[k])->first; c != 0; c = nd(s, afile, c)->next) {
        if (nd(s, afile, c)->kind != OZNI_N_Default) {
          ptype = c;
        }
      }
      if (foods && ptype != 0 && nd(s, afile, ptype)->kind == OZNI_N_TypeRef) {
        const ozni_node *argn = nullptr;
        for (ozni_index arg = nd(s, file, call->first)->next; arg != 0; arg = nd(s, file, arg)->next) {
          if (nd(s, file, arg)->kind == OZNI_N_List && argn == nullptr) {
            argn = nd(s, file, arg);
          }
        }
        params[k] = combine(s, items, n < 64 ? n : 64, file, argn != nullptr ? argn : call);
        if (shown_of[k] < nshown) {
          shown[shown_of[k]] = params[k];
        }
      }
    }
  }
  for (uint32_t k = 0; k < np; k++) {
    if (params[k].kind != V_FOOD || argnodes[k] == 0) {
      continue;
    }
    ozni_index ptype = 0;
    for (ozni_index c = nd(s, afile, pnodes[k])->first; c != 0; c = nd(s, afile, c)->next) {
      if (nd(s, afile, c)->kind != OZNI_N_Default) {
        ptype = c;
      }
    }
    if (ptype != 0 && params[k].f->file != NONE && !satisfies(s, params[k].f, afile, ptype)) {
      const ozni_node *argn = nd(s, file, argnodes[k]);
      const ozni_source *src = &s->c->srcs[file];
      ozni_str args[3] = {S(s, act->text), S(s, pnames[k]),
                          {src->text + argn->span.start, argn->span.end - argn->span.start}};
      diag_node(s, OZNI_D_TYP_stateRequired, file, argn, args, 3);
    }
  }
  for (uint32_t k = 0; k < np; k++) {
    if (params[k].kind == V_FOOD && argnodes[k] != 0 && nd(s, file, argnodes[k])->kind != OZNI_N_List) {
      consume(s, &params[k], file, argnodes[k]);
    }
  }
  food *out = nullptr;
  ozni_index output = 0;
  for (ozni_index c = act->first; c != 0; c = nd(s, afile, c)->next) {
    if (nd(s, afile, c)->kind == OZNI_N_Output) {
      output = nd(s, afile, c)->first;
    }
  }
  if (field_first(s, afile, anode, "combine") != 0) {
    for (uint32_t k = 0; k < np; k++) {
      if (params[k].kind == V_FOOD) {
        return params[k];
      }
    }
  }
  if (output != 0) {
    ozni_index tv = type_var_of(s, afile, output);
    if (tv != 0) {
      for (uint32_t k = 0; k < np && out == nullptr; k++) {
        ozni_index ptype = 0;
        for (ozni_index c = nd(s, afile, pnodes[k])->first; c != 0; c = nd(s, afile, c)->next) {
          if (nd(s, afile, c)->kind != OZNI_N_Default) {
            ptype = c;
          }
        }
        if (ptype != 0 && type_var_of(s, afile, ptype) == tv && params[k].kind == V_FOOD) {
          out = food_copy(s, params[k].f);
        }
      }
    } else if (nd(s, afile, output)->kind == OZNI_N_TypeRef) {
      out = food_new(s, nd(s, afile, output)->text);
      for (uint32_t k = 0; out != nullptr && k < np; k++) {
        if (params[k].kind == V_FOOD && params[k].f->amount.known) {
          out->amount = params[k].f->amount;
          break;
        }
      }
    }
    if (out != nullptr) {
      apply_effects(s, out, afile, output, params, pnames, np, file, call);
    }
  }
  value result = out != nullptr ? (value){.kind = V_FOOD, .f = out} : (value){.kind = V_NONE};
  result.ref = record_step(s, act->text, shown, nshown, shown_names, out);
  return result;
}

static value eval_binary(sema *s, const ozni_node *n) {
  value l = eval(s, n->first), r = eval(s, nd(s, s->file, n->first)->next);
  ozni_str op = S(s, n->text);
  if (l.kind != V_QTY || r.kind != V_QTY || !l.q.known || !r.q.known) {
    return unknown();
  }
  value out = {.kind = V_QTY};
  if (str_is(op, "+") || str_is(op, "-")) {
    if (!dims_equal(l.q.d, r.q.d)) {
      ozni_str args[2] = {S(s, dim_name_text(s, l.q.d)), S(s, dim_name_text(s, r.q.d))};
      diag_node(s, OZNI_D_TYP_dimension, s->file, n, args, 2);
      return unknown();
    }
    ozni_status st = str_is(op, "+") ? ozni_ival_add(l.q.v, r.q.v, &out.q.v) : ozni_ival_sub(l.q.v, r.q.v, &out.q.v);
    if (st != OZNI_OK) {
      return unknown();
    }
    out.q.d = l.q.d;
    out.q.known = true;
    return out;
  }
  if (str_is(op, "*") || str_is(op, "/")) {
    bool ok = str_is(op, "*") ? ival_mul(l.q.v, r.q.v, &out.q.v) : ival_divide(l.q.v, r.q.v, &out.q.v);
    if (!ok) {
      return unknown();
    }
    out.q.d = dims_combine(l.q.d, r.q.d, str_is(op, "*") ? 1 : -1);
    out.q.known = true;
    return out;
  }
  return unknown();
}

static ozni_index sub_ref(sema *s, ozni_index ref, int k) {
  ozni_str r = S(s, ref);
  char buf[32];
  if (r.len + 2 >= sizeof buf) {
    return ref;
  }
  ozni_copy(buf, r.p, r.len);
  buf[r.len] = '.';
  buf[r.len + 1] = (char)('0' + k);
  return intern(s, buf, r.len + 2);
}

static bool amount_in(sema *s, const food *f, ozni_qty q, ozni_qty *have, ozni_qty *need) {
  if (dims_equal(f->amount.d, q.d)) {
    *have = f->amount;
    *need = q;
    return true;
  }
  food tmp = *f;
  tmp.amount = q;
  return to_mass(s, f, have) && to_mass(s, &tmp, need);
}

static value eval_split(sema *s, const ozni_node *call) {
  ozni_index fnode = nd(s, s->file, call->first)->next;
  ozni_index qnode = nd(s, s->file, fnode)->next;
  value fv = eval(s, fnode), qv = eval(s, qnode);
  if (fv.kind != V_FOOD || qv.kind != V_QTY || !fv.f->amount.known || !qv.q.known) {
    return unknown();
  }
  ozni_qty have, need;
  if (!amount_in(s, fv.f, qv.q, &have, &need)) {
    return unknown();
  }
  if (ozni_rat_cmp(need.v.hi, have.v.lo) > 0) {
    ozni_buf nb = {.a = s->c->allocator}, hb = {.a = s->c->allocator};
    put_qty(s, &nb, need);
    put_qty(s, &hb, have);
    if (nb.failed || hb.failed) {
      s->c->oom = true;
    } else {
      ozni_str args[3] = {src_text(s, s->file, nd(s, s->file, fnode)), {nb.p, (uint32_t)nb.len},
                          {hb.p, (uint32_t)hb.len}};
      diag_node(s, OZNI_D_LIN_insufficient, s->file, call, args, 3);
    }
    ozni_buf_discard(&nb);
    ozni_buf_discard(&hb);
    consume(s, &fv, s->file, fnode);
    return unknown();
  }
  consume(s, &fv, s->file, fnode);
  food *portion = food_copy(s, fv.f), *rest = food_copy(s, fv.f);
  if (portion == nullptr || rest == nullptr) {
    return unknown();
  }
  portion->amount = need;
  rest->amount = have;
  if (ozni_ival_sub(have.v, need.v, &rest->amount.v) != OZNI_OK) {
    return unknown();
  }
  if (ozni_rat_cmp(rest->amount.v.lo, ozni_rat_int(0)) < 0) {
    rest->amount.v.lo = ozni_rat_int(0);
  }
  portion->portion = rest->portion = true;
  if (!ozni_table_push(s->portions, &portion, nullptr) || !ozni_table_push(s->portions, &rest, nullptr)) {
    s->c->oom = true;
  }
  s->step_no++;
  ozni_index ref = step_ref(s);
  if (s->steps && s->out != nullptr) {
    ozni_buf *b = s->out;
    ozni_buf_puts(b, "  ");
    ozni_str r = S(s, ref);
    ozni_buf_put(b, r.p, r.len);
    ozni_buf_puts(b, " = split(");
    put_value(s, b, &fv);
    ozni_buf_puts(b, ", ");
    put_value(s, b, &qv);
    ozni_buf_puts(b, ") : (");
    put_food_line(s, b, portion);
    ozni_buf_puts(b, ", ");
    put_food_line(s, b, rest);
    ozni_buf_puts(b, ")\n");
  }
  value out = {.kind = V_TUPLE};
  out.items = ozni_table_new(s->c->arena, sizeof(value));
  value a = {.kind = V_FOOD, .f = portion, .ref = sub_ref(s, ref, 1)};
  value b2 = {.kind = V_FOOD, .f = rest, .ref = sub_ref(s, ref, 2)};
  if (out.items == nullptr || !ozni_table_push(out.items, &a, nullptr) || !ozni_table_push(out.items, &b2, nullptr)) {
    s->c->oom = true;
  }
  return out;
}

static value eval(sema *s, ozni_index node) {
  const ozni_node *n = nd(s, s->file, node);
  if (s->c->oom) {
    return unknown();
  }
  if (n->kind == OZNI_N_Name) {
    value v;
    if (lookup_local(s, n->text, &v)) {
      return v;
    }
    ozni_index file, def;
    if (find_def(s, n->text, &file, &def)) {
      return unknown();
    }
    return (value){.kind = V_ENUM, .name = n->text};
  }
  if (n->kind == OZNI_N_Quantity) {
    value v = {.kind = V_QTY};
    if (!qty_of(s, s->mod, s->file, node, &v.q, true)) {
      return unknown();
    }
    return v;
  }
  if (n->kind == OZNI_N_Number || n->kind == OZNI_N_NumberRange) {
    value v = {.kind = V_QTY};
    ozni_str t = S(s, n->text);
    if (!ival_parse(t.p, t.len, &v.q.v)) {
      return unknown();
    }
    v.q.known = true;
    return v;
  }
  if (n->kind == OZNI_N_String || n->kind == OZNI_N_MlString) {
    return (value){.kind = V_STR, .name = n->text};
  }
  if (n->kind == OZNI_N_Binary) {
    return eval_binary(s, n);
  }
  if (n->kind == OZNI_N_List) {
    value v = {.kind = V_LIST};
    v.items = ozni_table_new(s->c->arena, sizeof(value));
    for (ozni_index c = n->first; v.items != nullptr && c != 0; c = nd(s, s->file, c)->next) {
      if (nd(s, s->file, c)->kind == OZNI_N_Annotation) {
        continue;
      }
      value item = eval(s, c);
      if (!ozni_table_push(v.items, &item, nullptr)) {
        s->c->oom = true;
      }
    }
    return v;
  }
  if (n->kind == OZNI_N_Call && n->first != 0) {
    const ozni_node *callee = nd(s, s->file, n->first);
    ozni_index afile, anode;
    if (callee->kind == OZNI_N_Name && find_def(s, callee->text, &afile, &anode) &&
        nd(s, afile, anode)->kind == OZNI_N_Action) {
      return call_action(s, s->file, n, afile, anode);
    }
    if (callee->kind == OZNI_N_Name && str_is(S(s, callee->text), "split") && callee->next != 0 &&
        nd(s, s->file, callee->next)->next != 0) {
      return eval_split(s, n);
    }
    for (ozni_index c = callee->next; c != 0; c = nd(s, s->file, c)->next) {
      eval(s, c);
    }
    return unknown();
  }
  return unknown();
}

static void eval_ingredients(sema *s, const ozni_node *n) {
  for (ozni_index it = n->first; it != 0; it = nd(s, s->file, it)->next) {
    const ozni_node *item = nd(s, s->file, it);
    ozni_index type = 0, amount = 0;
    for (ozni_index c = item->first; c != 0; c = nd(s, s->file, c)->next) {
      const ozni_node *cn = nd(s, s->file, c);
      if (cn->kind == OZNI_N_TypeRef) {
        type = cn->text;
      } else if (cn->kind != OZNI_N_Annotation) {
        amount = c;
      }
    }
    if (type == 0) {
      ozni_str nm = S(s, item->text);
      char buf[256];
      if (nm.len == 0 || nm.len >= sizeof buf) {
        continue;
      }
      ozni_copy(buf, nm.p, nm.len);
      if (buf[0] >= 'a' && buf[0] <= 'z') {
        buf[0] = (char)(buf[0] - 'a' + 'A');
      }
      type = intern(s, buf, nm.len);
    }
    food *f = food_new(s, type);
    if (f == nullptr) {
      return;
    }
    if (amount != 0 && nd(s, s->file, amount)->kind == OZNI_N_Vague) {
      f->vague = true;
      ozni_str arg = S(s, item->text);
      diag_node(s, OZNI_D_TYP_vagueAmount, s->file, nd(s, s->file, amount), &arg, 1);
    } else if (amount != 0) {
      value v = eval(s, amount);
      if (v.kind == V_QTY && v.q.known) {
        f->amount = v.q;
      }
    }
    value fv = {.kind = V_FOOD, .f = f, .ref = item->text};
    bind(s, item->text, fv);
    if (s->steps && s->out != nullptr) {
      ozni_buf_puts(s->out, "  ");
      ozni_str nm = S(s, item->text);
      ozni_buf_put(s->out, nm.p, nm.len);
      ozni_buf_puts(s->out, " : ");
      put_food_line(s, s->out, f);
      ozni_buf_puts(s->out, "\n");
    }
  }
}

static void label(food *f, ozni_index name, ozni_span span) {
  if (f != nullptr && f->label == 0) {
    f->label = name;
    f->label_span = span;
  }
}

static void eval_block(sema *s, ozni_index block, bool top) {
  for (ozni_index st = nd(s, s->file, block)->first; st != 0 && !s->c->oom; st = nd(s, s->file, st)->next) {
    const ozni_node *n = nd(s, s->file, st);
    if (n->kind == OZNI_N_Ingredients) {
      eval_ingredients(s, n);
    } else if (n->kind == OZNI_N_Let) {
      value v = n->first != 0 ? eval(s, n->first) : unknown();
      if (v.kind == V_FOOD) {
        label(v.f, n->text, (ozni_span){s->file, n->name_start, n->name_end});
        if (top) {
          s->last = v;
          s->last_node = n->first;
        }
      }
      bind(s, n->text, v);
    } else if (n->kind == OZNI_N_LetTuple) {
      value v = unknown();
      ozni_index expr = 0;
      for (ozni_index c = n->first; c != 0; c = nd(s, s->file, c)->next) {
        if (nd(s, s->file, c)->kind != OZNI_N_BindName) {
          expr = c;
        }
      }
      if (expr != 0) {
        v = eval(s, expr);
      }
      ozni_index k = 0;
      for (ozni_index c = n->first; c != 0; c = nd(s, s->file, c)->next) {
        const ozni_node *bn = nd(s, s->file, c);
        if (bn->kind != OZNI_N_BindName) {
          continue;
        }
        value item = unknown();
        if (v.kind == V_TUPLE && v.items != nullptr && k < ozni_table_count(v.items)) {
          item = *(const value *)ozni_table_get(v.items, k);
        }
        if (item.kind == V_FOOD) {
          label(item.f, bn->text, bn->span);
        }
        bind(s, bn->text, item);
        k++;
      }
    } else if (n->kind == OZNI_N_Parallel) {
      uint32_t saved_group = s->group, saved_branch = s->branch;
      s->group = ++s->next_id;
      for (ozni_index b = n->first; b != 0; b = nd(s, s->file, b)->next) {
        s->branch = ++s->next_id;
        if (nd(s, s->file, b)->first != 0) {
          eval_block(s, nd(s, s->file, b)->first, false);
        }
      }
      s->group = saved_group;
      s->branch = saved_branch;
    } else if (n->kind == OZNI_N_If || n->kind == OZNI_N_ForIn || n->kind == OZNI_N_Repeat ||
               n->kind == OZNI_N_Serve) {
      continue;
    } else {
      value v = eval(s, st);
      if (top && v.kind == V_FOOD) {
        s->last = v;
        s->last_node = st;
      }
    }
  }
}

static void finish_dish(sema *s) {
  if (s->last.kind == V_FOOD && s->last.f != nullptr && !s->last.f->consumed) {
    food *f = s->last.f;
    if (food_has(s, f, intern(s, "Hazard", 6)) && !food_has(s, f, intern(s, "RawEdible", 9))) {
      for (uint32_t k = 0; k < f->naxes; k++) {
        if (!str_is(S(s, f->axes[k].axis), "doneness")) {
          continue;
        }
        int cur = axis_value_index(s, &f->axes[k], f->axes[k].value);
        int done = axis_value_index(s, &f->axes[k], intern(s, "done", 4));
        if (cur >= 0 && done >= 0 && cur < done) {
          ozni_str args[2] = {S(s, f->type), S(s, f->axes[k].value)};
          diag_node(s, OZNI_D_SAF_undercooked, s->file, nd(s, s->file, s->last_node), args, 2);
        }
      }
    }
  }
  for (ozni_index k = 0; k < ozni_table_count(s->portions); k++) {
    food *f = *(food *const *)ozni_table_get(s->portions, k);
    if (f->consumed || (s->last.kind == V_FOOD && s->last.f == f) || f->label == 0) {
      continue;
    }
    ozni_buf ab = {.a = s->c->allocator};
    if (f->amount.known) {
      put_qty(s, &ab, f->amount);
    }
    if (ab.failed) {
      s->c->oom = true;
    } else {
      ozni_str args[2] = {S(s, f->label), {ab.p != nullptr ? ab.p : "", (uint32_t)ab.len}};
      diag(s, OZNI_D_LIN_leftoverPart, f->label_span.file, f->label_span.start, f->label_span.end, args, 2);
    }
    ozni_buf_discard(&ab);
  }
}

static void eval_dish(sema *s, ozni_index mi, const ozni_def *d) {
  const ozni_module *m = mod_at(s, mi);
  const ozni_node *n = nd(s, m->file, d->node);
  s->mod = mi;
  s->file = m->file;
  s->step_no = 0;
  s->scope = ozni_table_new(s->c->arena, sizeof(binding));
  if (s->scope == nullptr) {
    s->c->oom = true;
    return;
  }
  if (s->steps && s->out != nullptr) {
    ozni_buf_puts(s->out, n->kind == OZNI_N_Menu ? "menu " : "recipe ");
    ozni_str name = S(s, n->text);
    ozni_buf_put(s->out, name.p, name.len);
    ozni_buf_puts(s->out, "\n");
  }
  s->portions = ozni_table_new(s->c->arena, sizeof(food *));
  s->last = unknown();
  s->last_node = 0;
  if (s->portions == nullptr) {
    s->c->oom = true;
    return;
  }
  for (ozni_index c = n->first; c != 0; c = nd(s, m->file, c)->next) {
    if (nd(s, m->file, c)->kind == OZNI_N_Block) {
      eval_block(s, c, true);
    }
  }
  finish_dish(s);
}

static bool effect_reaches(sema *s, ozni_index file, ozni_index type, ozni_index axis, ozni_index wanted) {
  const ozni_node *t = nd(s, file, type);
  for (ozni_index ta = t->kind == OZNI_N_TypeRef ? t->first : 0; ta != 0; ta = nd(s, file, ta)->next) {
    if (nd(s, file, ta)->kind != OZNI_N_TypeArgs) {
      continue;
    }
    for (ozni_index alt = nd(s, file, ta)->first; alt != 0; alt = nd(s, file, alt)->next) {
      for (ozni_index a = nd(s, file, alt)->first; a != 0; a = nd(s, file, a)->next) {
        const ozni_node *an = nd(s, file, a);
        if (an->kind != OZNI_N_StateArg || an->text != axis || an->first == 0) {
          continue;
        }
        const ozni_node *v = nd(s, file, an->first);
        ozni_index name = v->kind == OZNI_N_Call && v->first != 0 ? nd(s, file, v->first)->text : v->text;
        if (name == wanted) {
          return true;
        }
      }
    }
  }
  return false;
}

static bool value_reachable(sema *s, ozni_index axis, ozni_index wanted) {
  for (ozni_index mi = 0; mi < ozni_table_count(s->pr->modules); mi++) {
    const ozni_module *m = mod_at(s, mi);
    for (ozni_index d = 0; d < ozni_table_count(m->defs); d++) {
      const ozni_node *n = nd(s, m->file, def_at(s, mi, d)->node);
      if (n->kind == OZNI_N_Action) {
        for (ozni_index c = n->first; c != 0; c = nd(s, m->file, c)->next) {
          if (nd(s, m->file, c)->kind == OZNI_N_Output && nd(s, m->file, c)->first != 0 &&
              effect_reaches(s, m->file, nd(s, m->file, c)->first, axis, wanted)) {
            return true;
          }
        }
      } else if (n->kind == OZNI_N_Ingredient) {
        ozni_index init = field_first(s, m->file, def_at(s, mi, d)->node, "initial");
        ozni_index rec = init != 0 ? nd(s, m->file, init)->first : 0;
        for (ozni_index e = rec != 0 && nd(s, m->file, rec)->kind == OZNI_N_Record ? nd(s, m->file, rec)->first : 0;
             e != 0; e = nd(s, m->file, e)->next) {
          const ozni_node *en = nd(s, m->file, e);
          if (en->text == axis && en->first != 0 && nd(s, m->file, en->first)->text == wanted) {
            return true;
          }
        }
      }
    }
  }
  return false;
}

static void check_contradictions(sema *s, ozni_index mi, ozni_index axis, ozni_index afile, ozni_index anode) {
  const ozni_module *m = mod_at(s, mi);
  axis_state st = {axis, afile, anode, AX_ORDERED, 0, 0, 0, nullptr};
  for (ozni_index d = 0; d < ozni_table_count(m->defs); d++) {
    const ozni_node *n = nd(s, m->file, def_at(s, mi, d)->node);
    if (n->kind != OZNI_N_Action) {
      continue;
    }
    int need = -1;
    ozni_index set_to = 0;
    for (ozni_index c = n->first; c != 0; c = nd(s, m->file, c)->next) {
      const ozni_node *cn = nd(s, m->file, c);
      ozni_index type = cn->first;
      if ((cn->kind != OZNI_N_Param && cn->kind != OZNI_N_Output) || type == 0 ||
          nd(s, m->file, type)->kind != OZNI_N_TypeRef) {
        continue;
      }
      for (ozni_index ta = nd(s, m->file, type)->first; ta != 0; ta = nd(s, m->file, ta)->next) {
        if (nd(s, m->file, ta)->kind != OZNI_N_TypeArgs) {
          continue;
        }
        ozni_index alt = nd(s, m->file, ta)->first;
        for (ozni_index a = alt != 0 ? nd(s, m->file, alt)->first : 0; a != 0; a = nd(s, m->file, a)->next) {
          const ozni_node *an = nd(s, m->file, a);
          if (an->kind != OZNI_N_StateArg || an->text != axis || an->first == 0) {
            continue;
          }
          ozni_index v = nd(s, m->file, an->first)->text;
          ozni_str op = S(s, an->text2);
          if (cn->kind == OZNI_N_Param && str_is(op, ">=")) {
            need = axis_value_index(s, &st, v);
          } else if (cn->kind == OZNI_N_Output && str_is(op, ":")) {
            set_to = v;
          }
        }
      }
    }
    int to = set_to != 0 ? axis_value_index(s, &st, set_to) : -1;
    if (need >= 0 && to >= 0 && to < need) {
      ozni_str args[3] = {{"contradiction", 13}, S(s, axis), S(s, set_to)};
      diag_node(s, OZNI_D_TYP_axisDefinition, m->file, n, args, 3);
    }
  }
}

static void check_axes(sema *s) {
  for (ozni_index mi = 0; mi < ozni_table_count(s->pr->modules); mi++) {
    const ozni_module *m = mod_at(s, mi);
    if (m->is_std) {
      continue;
    }
    for (ozni_index d = 0; d < ozni_table_count(m->defs); d++) {
      const ozni_def *e = def_at(s, mi, d);
      const ozni_node *n = nd(s, m->file, e->node);
      if (n->kind != OZNI_N_Axis) {
        continue;
      }
      bool first = true;
      for (ozni_index v = n->first; v != 0; v = nd(s, m->file, v)->next) {
        const ozni_node *vn = nd(s, m->file, v);
        if (vn->kind != OZNI_N_Value) {
          continue;
        }
        bool set_axis = str_is(S(s, n->text2), "set");
        if ((!first || set_axis) && !value_reachable(s, n->text, vn->text)) {
          ozni_str args[3] = {{"unreachable", 11}, S(s, n->text), S(s, vn->text)};
          diag_node(s, OZNI_D_TYP_axisDefinition, m->file, vn, args, 3);
        }
        first = false;
      }
      if (str_is(S(s, n->text2), "ordered")) {
        for (ozni_index mj = 0; mj < ozni_table_count(s->pr->modules); mj++) {
          if (!mod_at(s, mj)->is_std) {
            check_contradictions(s, mj, n->text, m->file, e->node);
          }
        }
      }
    }
  }
}

bool ozni_sema_run(ozni_project *pr, bool steps, ozni_buf *out) {
  sema s = {.pr = pr, .c = pr->c, .out = out, .steps = steps};
  s.dims = ozni_table_new(pr->c->arena, sizeof(ozni_index));
  s.units = ozni_table_new(pr->c->arena, sizeof(unit_info));
  if (s.dims == nullptr || s.units == nullptr) {
    return false;
  }
  build_units(&s);
  check_axes(&s);
  for (ozni_index mi = 0; mi < ozni_table_count(pr->modules) && !pr->c->oom; mi++) {
    const ozni_module *m = mod_at(&s, mi);
    if (m->is_std) {
      continue;
    }
    for (ozni_index d = 0; d < ozni_table_count(m->defs); d++) {
      const ozni_def *e = ozni_table_get(m->defs, d);
      ozni_node_kind k = nd(&s, m->file, e->node)->kind;
      if (k == OZNI_N_Recipe || k == OZNI_N_Menu) {
        eval_dish(&s, mi, e);
      }
    }
  }
  return !pr->c->oom;
}
