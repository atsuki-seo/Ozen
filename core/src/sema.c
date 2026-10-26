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

typedef struct cand {
  ozni_index type;
  ozni_index file;
  ozni_index node;
  ozni_index part;
  ozni_index pfile;
  ozni_index pnode;
} cand;

typedef struct food {
  ozni_index type;
  ozni_index file;
  ozni_index node;
  ozni_index part;
  ozni_index pfile;
  ozni_index pnode;
  cand *cands;
  uint32_t ncands;
  bool discard;
  uint32_t naxes;
  axis_state *axes;
  ozni_qty amount;
  bool vague;
  bool liquid;
  bool consumed;
  bool portion;
  uint32_t consumed_group;
  uint32_t consumed_branch;
  ozni_index label;
  ozni_span label_span;
  ozni_table *derived;
  bool divided;
  ozni_rat used;
} food;

typedef enum vkind : uint8_t {
  V_UNKNOWN,
  V_NONE,
  V_QTY,
  V_FOOD,
  V_ENUM,
  V_STR,
  V_BOOL,
  V_LIST,
  V_TUPLE,
  V_UNDECIDED
} vkind;

typedef struct value {
  vkind kind;
  bool b;
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
  bool requires;
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
  ozni_table *reqs;
  ozni_table *variant_acts;
  ozni_index kfile;
  ozni_index knode;
  ozni_index ofile;
  ozni_index onode;
  ozni_index otfile;
  ozni_index otnode;
  ozni_table *kentries;
  ozni_qty servings;
  uint32_t quiet;
  uint32_t make_depth;
  bool entry_dish;
  ozni_index trail;
  const struct mframe *frame;
  uint32_t depth;
  bool top;
  ozni_index deprecated_unit;
  int level;
} sema;

typedef struct mframe {
  const struct mframe *parent;
  ozni_index mfile;
  ozni_index mnode;
  ozni_index cfile;
  ozni_index cmod;
  ozni_table *cscope;
  ozni_index ctrail;
  ozni_index args[16];
  ozni_index params[16];
  uint32_t nargs;
} mframe;

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
  if (s->quiet > 0) {
    return;
  }
  if (!ozni_diag_add(s->c, id, (ozni_span){file, start, end}, s->trail, args, nargs)) {
    s->c->oom = true;
    return;
  }
}

static bool printing(const sema *s) { return s->steps && s->out != nullptr && s->quiet == 0; }

static void limit_diag(sema *s, ozni_diag_id id, ozni_index file, const ozni_node *n, uint64_t limit) {
  char num[24], rev[24];
  size_t len = 0;
  do {
    rev[len++] = (char)('0' + limit % 10);
    limit /= 10;
  } while (limit != 0);
  for (size_t k = 0; k < len; k++) {
    num[k] = rev[len - 1 - k];
  }
  ozni_str arg = {num, (uint32_t)len};
  if (!ozni_diag_add(s->c, id, (ozni_span){file, n->span.start, n->span.end}, s->trail, &arg, 1)) {
    s->c->oom = true;
  }
  s->c->limit_hit = true;
}

static bool tick(sema *s, ozni_index file, const ozni_node *n) {
  if (s->c->limit_hit || s->c->oom) {
    return false;
  }
  if (file >= s->c->nuser) {
    return true;
  }
  if (++s->c->steps_used > s->c->lim.steps) {
    limit_diag(s, OZNI_D_MAC_fuelExhausted, file, n, s->c->lim.steps);
    return false;
  }
  return true;
}

static bool enter(sema *s, ozni_index file, const ozni_node *n) {
  if (++s->depth > s->c->lim.depth && file < s->c->nuser) {
    limit_diag(s, OZNI_D_MAC_depthExceeded, file, n, s->c->lim.depth);
    return false;
  }
  return !s->c->limit_hit;
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
    for (ozni_index c = nd(s, u->file, u->node)->first; c != 0; c = nd(s, u->file, c)->next) {
      if (nd(s, u->file, c)->kind == OZNI_N_Annotation && str_is(S(s, nd(s, u->file, c)->text), "deprecated")) {
        s->deprecated_unit = u->name;
      }
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
    s->deprecated_unit = 0;
    if (!unit_path(s, mod, unit, &f, &d, &bad)) {
      if (report && bad.len > 0) {
        diag_node(s, OZNI_D_NAM_unknownUnit, file, n, &bad, 1);
      }
      return false;
    }
    if (report && s->deprecated_unit != 0 && file < s->c->nuser) {
      ozni_str arg = S(s, s->deprecated_unit);
      diag_node(s, OZNI_D_PKG_deprecated, file, n, &arg, 1);
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

static ozni_index mod_of_file(const sema *s, ozni_index file) {
  ozni_index found = s->mod;
  for (ozni_index mi = 0; mi < ozni_table_count(s->pr->modules); mi++) {
    if (mod_at(s, mi)->file == file) {
      found = mi;
    }
  }
  return found;
}

static bool find_from(sema *s, ozni_index from_file, ozni_index name, ozni_index *file, ozni_index *node) {
  ozni_index saved = s->mod;
  s->mod = mod_of_file(s, from_file);
  bool found = find_def(s, name, file, node);
  s->mod = saved;
  return found;
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
    f->naxes = 0;
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

/* Parts */

static bool decl_has_trait(sema *s, ozni_index file, ozni_index decl, ozni_index name, uint32_t depth);

static ozni_index child_section(const sema *s, ozni_index file, ozni_index node, const char *name) {
  for (ozni_index c = nd(s, file, node)->first; c != 0; c = nd(s, file, c)->next) {
    const ozni_node *cn = nd(s, file, c);
    if (cn->kind == OZNI_N_Section && str_is(S(s, cn->text), name)) {
      return c;
    }
  }
  return 0;
}

static bool is_part_node(const ozni_node *n) { return n->kind == OZNI_N_Marker || n->kind == OZNI_N_Section; }

static bool part_lookup(sema *s, ozni_index file, ozni_index decl, ozni_index name, ozni_index *pfile,
                        ozni_index *pnode, uint32_t depth) {
  if (depth > 32) {
    return false;
  }
  ozni_index ps = child_section(s, file, decl, "parts");
  for (ozni_index p = ps != 0 ? nd(s, file, ps)->first : 0; p != 0; p = nd(s, file, p)->next) {
    if (is_part_node(nd(s, file, p)) && nd(s, file, p)->text == name) {
      *pfile = file;
      *pnode = p;
      return true;
    }
  }
  for (ozni_index c = nd(s, file, decl)->first; c != 0; c = nd(s, file, c)->next) {
    ozni_index tf, tn;
    if (nd(s, file, c)->kind == OZNI_N_TypeRef && find_from(s, file, nd(s, file, c)->text, &tf, &tn) &&
        nd(s, tf, tn)->kind == OZNI_N_Trait && part_lookup(s, tf, tn, name, pfile, pnode, depth + 1)) {
      return true;
    }
  }
  return false;
}

static uint32_t collect_parts(sema *s, ozni_index file, ozni_index decl, ozni_index *files, ozni_index *nodes,
                              uint32_t n, uint32_t cap, uint32_t depth) {
  if (depth > 32) {
    return n;
  }
  ozni_index ps = child_section(s, file, decl, "parts");
  for (ozni_index p = ps != 0 ? nd(s, file, ps)->first : 0; p != 0 && n < cap; p = nd(s, file, p)->next) {
    if (is_part_node(nd(s, file, p))) {
      files[n] = file;
      nodes[n++] = p;
    }
  }
  for (ozni_index c = nd(s, file, decl)->first; c != 0; c = nd(s, file, c)->next) {
    ozni_index tf, tn;
    if (nd(s, file, c)->kind == OZNI_N_TypeRef && find_from(s, file, nd(s, file, c)->text, &tf, &tn) &&
        nd(s, tf, tn)->kind == OZNI_N_Trait) {
      n = collect_parts(s, tf, tn, files, nodes, n, cap, depth + 1);
    }
  }
  return n;
}

static bool part_is(sema *s, ozni_index ofile, ozni_index odecl, ozni_index pfile, ozni_index pnode, ozni_index name,
                    uint32_t depth) {
  if (depth > 32) {
    return false;
  }
  if (nd(s, pfile, pnode)->text == name) {
    return true;
  }
  ozni_index traits = field_first(s, pfile, pnode, "traits");
  for (ozni_index v = traits != 0 ? nd(s, pfile, traits)->first : 0; v != 0; v = nd(s, pfile, v)->next) {
    ozni_index tf, tn;
    if (nd(s, pfile, v)->text == name ||
        (find_from(s, pfile, nd(s, pfile, v)->text, &tf, &tn) && nd(s, tf, tn)->kind == OZNI_N_Trait &&
         decl_has_trait(s, tf, tn, name, 0))) {
      return true;
    }
  }
  ozni_index ext = field_first(s, pfile, pnode, "extends");
  ozni_index ef, en;
  return ext != 0 && nd(s, pfile, ext)->first != 0 &&
         part_lookup(s, ofile, odecl, nd(s, pfile, nd(s, pfile, ext)->first)->text, &ef, &en, 0) &&
         part_is(s, ofile, odecl, ef, en, name, depth + 1);
}

static bool part_discard(sema *s, ozni_index pfile, ozni_index pnode) {
  ozni_index f = field_first(s, pfile, pnode, "discard");
  return f != 0 && nd(s, pfile, f)->first != 0 && str_is(S(s, nd(s, pfile, nd(s, pfile, f)->first)->text), "true");
}

/* Amounts */

static bool prop_at(sema *s, ozni_index file, ozni_index field, ozni_qty *out) {
  if (field == 0 || nd(s, file, field)->first == 0 || nd(s, file, nd(s, file, field)->first)->kind != OZNI_N_Quantity) {
    return false;
  }
  return qty_of(s, mod_of_file(s, file), file, nd(s, file, field)->first, out, false);
}

static bool part_prop(sema *s, const cand *c, ozni_index pfile, ozni_index pnode, const char *key, ozni_qty *out,
                      uint32_t depth) {
  if (depth > 32) {
    return false;
  }
  ozni_index props = child_section(s, pfile, pnode, "props");
  if (props != 0 && prop_at(s, pfile, field_first(s, pfile, props, key), out)) {
    return true;
  }
  ozni_index ext = field_first(s, pfile, pnode, "extends");
  ozni_index ef, en;
  return ext != 0 && nd(s, pfile, ext)->first != 0 &&
         part_lookup(s, c->file, c->node, nd(s, pfile, nd(s, pfile, ext)->first)->text, &ef, &en, 0) &&
         part_prop(s, c, ef, en, key, out, depth + 1);
}

static bool decl_prop(sema *s, ozni_index file, ozni_index decl, const char *key, ozni_qty *out, uint32_t depth);

static bool contains_prop(sema *s, ozni_index file, ozni_index decl, const char *key, ozni_qty *out, uint32_t depth) {
  ozni_index field = field_first(s, file, decl, "contains");
  ozni_index rec = field != 0 ? nd(s, file, field)->first : 0;
  if (rec == 0 || nd(s, file, rec)->kind != OZNI_N_Record) {
    return false;
  }
  ozni_ival frac[16], val[16];
  uint32_t n = 0;
  ozni_dimvec d = {{0}};
  for (ozni_index e = nd(s, file, rec)->first; e != 0; e = nd(s, file, e)->next) {
    const ozni_node *en = nd(s, file, e);
    ozni_index cf, cn;
    ozni_qty fq, vq;
    if (n == 16 || en->first == 0 || !qty_of(s, mod_of_file(s, file), file, en->first, &fq, false) ||
        !dimensionless(fq.d) || !find_from(s, file, en->text, &cf, &cn) || nd(s, cf, cn)->kind != OZNI_N_Ingredient ||
        !decl_prop(s, cf, cn, key, &vq, depth + 1) || (n > 0 && !dims_equal(d, vq.d))) {
      return false;
    }
    d = vq.d;
    frac[n] = fq.v;
    val[n++] = vq.v;
  }
  ozni_rat lo_sum = ozni_rat_int(0), hi_sum = ozni_rat_int(0);
  for (uint32_t k = 0; k < n; k++) {
    if (ozni_rat_add(lo_sum, frac[k].lo, &lo_sum) != OZNI_OK || ozni_rat_add(hi_sum, frac[k].hi, &hi_sum) != OZNI_OK) {
      return false;
    }
  }
  if (n == 0 || ozni_rat_cmp(lo_sum, ozni_rat_int(1)) > 0 || ozni_rat_cmp(hi_sum, ozni_rat_int(1)) < 0) {
    return false;
  }
  ozni_rat bound[2];
  for (int side = 0; side < 2; side++) {
    ozni_rat w[16], rem;
    bool used[16] = {false};
    if (ozni_rat_sub(ozni_rat_int(1), lo_sum, &rem) != OZNI_OK) {
      return false;
    }
    for (uint32_t k = 0; k < n; k++) {
      w[k] = frac[k].lo;
    }
    for (uint32_t step = 0; step < n; step++) {
      uint32_t best = n;
      for (uint32_t k = 0; k < n; k++) {
        ozni_rat vk = side == 0 ? val[k].lo : val[k].hi;
        if (!used[k] && (best == n || (side == 0 ? ozni_rat_cmp(vk, val[best].lo) < 0
                                                 : ozni_rat_cmp(vk, val[best].hi) > 0))) {
          best = k;
        }
      }
      used[best] = true;
      ozni_rat room, add;
      if (ozni_rat_sub(frac[best].hi, frac[best].lo, &room) != OZNI_OK) {
        return false;
      }
      add = ozni_rat_cmp(room, rem) < 0 ? room : rem;
      if (ozni_rat_add(w[best], add, &w[best]) != OZNI_OK || ozni_rat_sub(rem, add, &rem) != OZNI_OK) {
        return false;
      }
    }
    bound[side] = ozni_rat_int(0);
    for (uint32_t k = 0; k < n; k++) {
      ozni_rat term;
      if (ozni_rat_mul(w[k], side == 0 ? val[k].lo : val[k].hi, &term) != OZNI_OK ||
          ozni_rat_add(bound[side], term, &bound[side]) != OZNI_OK) {
        return false;
      }
    }
  }
  *out = (ozni_qty){{bound[0], bound[1]}, d, true};
  return true;
}

static bool decl_prop(sema *s, ozni_index file, ozni_index decl, const char *key, ozni_qty *out, uint32_t depth) {
  if (depth > 8) {
    return false;
  }
  return prop_at(s, file, section_field(s, file, decl, "props", key), out) ||
         contains_prop(s, file, decl, key, out, depth);
}

static bool cand_prop(sema *s, const cand *c, const char *key, ozni_qty *out) {
  if (c->file == NONE) {
    return false;
  }
  return (c->pnode != 0 && part_prop(s, c, c->pfile, c->pnode, key, out, 0)) ||
         decl_prop(s, c->file, c->node, key, out, 0);
}

static cand self_cand(const food *f) { return (cand){f->type, f->file, f->node, f->part, f->pfile, f->pnode}; }

static bool food_prop(sema *s, const food *f, const char *key, ozni_qty *out) {
  if (f->ncands == 0) {
    cand c = self_cand(f);
    return cand_prop(s, &c, key, out);
  }
  for (uint32_t k = 0; k < f->ncands; k++) {
    ozni_qty q;
    if (!cand_prop(s, &f->cands[k], key, &q) || (k > 0 && !dims_equal(q.d, out->d))) {
      return false;
    }
    if (k == 0) {
      *out = q;
    } else {
      out->v.lo = ozni_rat_cmp(q.v.lo, out->v.lo) < 0 ? q.v.lo : out->v.lo;
      out->v.hi = ozni_rat_cmp(q.v.hi, out->v.hi) > 0 ? q.v.hi : out->v.hi;
    }
  }
  return true;
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

static bool to_volume(sema *s, const food *f, ozni_qty *out) {
  int volume = dim_named(s, "Volume");
  if (!f->amount.known || volume < 0) {
    return false;
  }
  ozni_dimvec want = {{0}};
  want.e[volume] = 1;
  if (dims_equal(f->amount.d, want)) {
    *out = f->amount;
    return true;
  }
  ozni_qty m, density;
  ozni_ival v;
  if (!to_mass(s, f, &m) || !food_prop(s, f, "density", &density) || !ival_divide(m.v, density.v, &v) ||
      !dims_equal(dims_combine(m.d, density.d, -1), want)) {
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
                              const food *out, food *const *outs, uint32_t nouts) {
  s->step_no++;
  ozni_index ref = step_ref(s);
  if (printing(s)) {
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
    } else if (nouts > 0) {
      ozni_buf_puts(b, " : (");
      for (uint32_t k = 0; k < nouts; k++) {
        ozni_buf_puts(b, k == 0 ? "" : ", ");
        put_food_line(s, b, outs[k]);
      }
      ozni_buf_puts(b, ")");
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
static void eval_serve(sema *s, const ozni_node *serve);
static value eval_make(sema *s, const ozni_node *mk);
static value eval_buy(sema *s, const ozni_node *n);
static value eval_choose(sema *s, const ozni_node *n);
static value make_tuple(sema *s, ozni_index ref, food *const *outs, uint32_t n);
static void eval_block(sema *s, ozni_index block, bool top);
static const mframe *frame_for(const sema *s, ozni_index param, uint32_t *index);

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

static void add_contains_traits(sema *s, food *f, ozni_index file, ozni_index decl, uint32_t depth) {
  ozni_index field = field_first(s, file, decl, "contains");
  ozni_index rec = field != 0 ? nd(s, file, field)->first : 0;
  if (depth > 8 || rec == 0 || nd(s, file, rec)->kind != OZNI_N_Record) {
    return;
  }
  ozni_index hazard = intern(s, "Hazard", 6);
  for (ozni_index e = nd(s, file, rec)->first; e != 0; e = nd(s, file, e)->next) {
    ozni_index cf, cn;
    if (!find_from(s, file, nd(s, file, e)->text, &cf, &cn) || nd(s, cf, cn)->kind != OZNI_N_Ingredient) {
      continue;
    }
    if (decl_has_trait(s, cf, cn, hazard, 0)) {
      add_derived(s, f, hazard);
    }
    for (ozni_index c = nd(s, cf, cn)->first; c != 0; c = nd(s, cf, c)->next) {
      const ozni_node *tn = nd(s, cf, c);
      if (tn->kind == OZNI_N_TypeRef && str_is(S(s, tn->text), "Allergen")) {
        ozni_str text = src_text(s, cf, tn);
        add_derived(s, f, intern(s, text.p, text.len));
      }
    }
    add_contains_traits(s, f, cf, cn, depth + 1);
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
    mix->liquid = mix->liquid || f->liquid || food_has(s, f, intern(s, "Liquid", 6));
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
  out.ref = record_step(s, intern(s, "combine", 7), &list, 1, nullptr, mix, nullptr, 0);
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
    if (find_from(s, file, cn->text, &tfile, &tnode) && nd(s, tfile, tnode)->kind == OZNI_N_Trait &&
        decl_has_trait(s, tfile, tnode, name, depth + 1)) {
      return true;
    }
  }
  return false;
}

static bool cand_has(sema *s, const cand *c, ozni_index name) {
  if (c->type == name || c->part == name) {
    return true;
  }
  if (c->pnode != 0 && part_is(s, c->file, c->node, c->pfile, c->pnode, name, 0)) {
    return true;
  }
  return c->file != NONE && decl_has_trait(s, c->file, c->node, name, 0);
}

static bool food_has(sema *s, const food *f, ozni_index name) {
  for (ozni_index k = 0; f->derived != nullptr && k < ozni_table_count(f->derived); k++) {
    if (*(const ozni_index *)ozni_table_get(f->derived, k) == name) {
      return true;
    }
  }
  if (f->type == name) {
    return true;
  }
  if (f->ncands == 0) {
    cand c = self_cand(f);
    return cand_has(s, &c, name);
  }
  bool any = false, all = true;
  for (uint32_t k = 0; k < f->ncands; k++) {
    bool h = cand_has(s, &f->cands[k], name);
    any = any || h;
    all = all && h;
  }
  return str_is(S(s, name), "Hazard") ? any : all;
}

static bool type_matches(sema *s, const food *f, ozni_index name) {
  if (f->type == name) {
    return true;
  }
  if (f->ncands > 0) {
    bool all = true;
    for (uint32_t k = 0; k < f->ncands; k++) {
      all = all && f->cands[k].type == name;
    }
    if (all) {
      return true;
    }
  }
  ozni_index tf, tn;
  return find_def(s, name, &tf, &tn) && nd(s, tf, tn)->kind == OZNI_N_Trait && food_has(s, f, name);
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
  if (S(s, t->text).len > 1 && !type_matches(s, f, t->text)) {
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

static food *food_new_in(sema *s, ozni_index file, ozni_index type) {
  ozni_index saved = s->mod;
  s->mod = mod_of_file(s, file);
  food *f = food_new(s, type);
  s->mod = saved;
  return f;
}

static uint32_t push_cand(cand *cs, uint32_t n, cand c) {
  for (uint32_t k = 0; k < n; k++) {
    if (cs[k].type == c.type && cs[k].pfile == c.pfile && cs[k].pnode == c.pnode) {
      return n;
    }
  }
  if (n < 64) {
    cs[n++] = c;
  }
  return n;
}

static bool set_cands(sema *s, food *f, const cand *cs, uint32_t n) {
  f->cands = ozni_arena_alloc(s->c->arena, n * sizeof *cs);
  if (f->cands == nullptr) {
    s->c->oom = true;
    return false;
  }
  for (uint32_t k = 0; k < n; k++) {
    f->cands[k] = cs[k];
  }
  f->ncands = n;
  return true;
}

static food *food_typed(sema *s, ozni_index file, ozni_index tnode, bool report, uint32_t depth);

static food *food_alias(sema *s, ozni_index afile, ozni_index anode, ozni_index name, uint32_t depth) {
  ozni_index ty = nd(s, afile, anode)->first;
  while (ty != 0 && nd(s, afile, ty)->kind == OZNI_N_Annotation) {
    ty = nd(s, afile, ty)->next;
  }
  if (ty == 0 || depth > 8) {
    return food_new(s, name);
  }
  cand cs[64];
  uint32_t n = 0;
  food *f = nullptr;
  bool is_union = nd(s, afile, ty)->kind == OZNI_N_UnionType;
  for (ozni_index m = is_union ? nd(s, afile, ty)->first : ty; m != 0; m = is_union ? nd(s, afile, m)->next : 0) {
    if (nd(s, afile, m)->kind != OZNI_N_TypeRef) {
      continue;
    }
    food *mf = food_typed(s, afile, m, false, depth + 1);
    if (mf == nullptr) {
      return nullptr;
    }
    if (f == nullptr) {
      f = mf;
    }
    if (mf->ncands > 0) {
      for (uint32_t k = 0; k < mf->ncands; k++) {
        n = push_cand(cs, n, mf->cands[k]);
      }
    } else {
      n = push_cand(cs, n, self_cand(mf));
    }
  }
  if (f == nullptr) {
    return food_new(s, name);
  }
  if (!set_cands(s, f, cs, n)) {
    return f;
  }
  f->type = name;
  f->part = 0;
  f->pfile = 0;
  f->pnode = 0;
  return f;
}

static food *food_typed(sema *s, ozni_index file, ozni_index tnode, bool report, uint32_t depth) {
  const ozni_node *t = nd(s, file, tnode);
  ozni_index df, dn;
  if (find_from(s, file, t->text, &df, &dn) && nd(s, df, dn)->kind == OZNI_N_Alias) {
    return food_alias(s, df, dn, t->text, depth);
  }
  food *f = food_new_in(s, file, t->text);
  if (f == nullptr || f->file == NONE) {
    return f;
  }
  add_contains_traits(s, f, f->file, f->node, 0);
  ozni_index targs = 0;
  for (ozni_index c = t->first; c != 0; c = nd(s, file, c)->next) {
    if (nd(s, file, c)->kind == OZNI_N_TypeArgs) {
      targs = c;
    }
  }
  if (targs == 0) {
    return f;
  }
  cand cs[64];
  uint32_t n = 0, nalts = 0, nbare = 0;
  ozni_index single = 0;
  bool states = false;
  for (ozni_index alt = nd(s, file, targs)->first; alt != 0; alt = nd(s, file, alt)->next) {
    nalts++;
    for (ozni_index a = nd(s, file, alt)->first; a != 0; a = nd(s, file, a)->next) {
      const ozni_node *an = nd(s, file, a);
      if (an->kind == OZNI_N_StateArg) {
        states = states || nalts == 1;
        continue;
      }
      if (an->kind != OZNI_N_BareArg) {
        continue;
      }
      if (nalts == 1) {
        nbare++;
        single = a;
      }
      ozni_index pf, pn, tf, tn;
      if (part_lookup(s, f->file, f->node, an->text, &pf, &pn, 0)) {
        n = push_cand(cs, n, (cand){f->type, f->file, f->node, an->text, pf, pn});
        continue;
      }
      ozni_index pfs[64], pns[64];
      uint32_t np = collect_parts(s, f->file, f->node, pfs, pns, 0, 64, 0);
      if (np == 0) {
        if (report) {
          ozni_str args[2] = {S(s, f->type), S(s, an->text)};
          diag_node(s, OZNI_D_TYP_partless, file, an, args, 2);
        }
      } else if (find_from(s, file, an->text, &tf, &tn) && nd(s, tf, tn)->kind == OZNI_N_Trait) {
        for (uint32_t k = 0; k < np; k++) {
          if (part_is(s, f->file, f->node, pfs[k], pns[k], an->text, 0)) {
            n = push_cand(cs, n, (cand){f->type, f->file, f->node, nd(s, pfs[k], pns[k])->text, pfs[k], pns[k]});
          }
        }
      } else if (report) {
        ozni_str arg = S(s, an->text);
        diag_node(s, OZNI_D_NAM_undefined, file, an, &arg, 1);
      }
    }
  }
  if (nalts == 1 && nbare == 1 && n == 1 && cs[0].part == nd(s, file, single)->text) {
    f->part = cs[0].part;
    f->pfile = cs[0].pfile;
    f->pnode = cs[0].pnode;
    f->discard = part_discard(s, f->pfile, f->pnode);
  } else if (n > 0 && set_cands(s, f, cs, n)) {
    f->part = nalts == 1 && nbare == 1 ? nd(s, file, single)->text : 0;
  }
  if (states) {
    apply_effects(s, f, file, tnode, nullptr, nullptr, 0, file, t);
  }
  return f;
}

typedef struct act_ref {
  ozni_index file;
  ozni_index node;
  uint32_t usable;
} act_ref;

/* Kitchen */

typedef struct kentry {
  ozni_index name;
  ozni_index kfile;
  ozni_index knode;
  ozni_index vfile;
  ozni_index vnode;
  ozni_index sfile;
  ozni_index snode;
} kentry;

static bool type_field(sema *s, ozni_index tfile, ozni_index tnode, ozni_index name, ozni_index *ffile,
                       ozni_index *fnode, uint32_t depth) {
  if (depth > 16) {
    return false;
  }
  for (ozni_index c = nd(s, tfile, tnode)->first; c != 0; c = nd(s, tfile, c)->next) {
    const ozni_node *cn = nd(s, tfile, c);
    if (cn->kind == OZNI_N_TypeField && cn->text == name) {
      *ffile = tfile;
      *fnode = c;
      return true;
    }
  }
  for (ozni_index c = nd(s, tfile, tnode)->first; c != 0; c = nd(s, tfile, c)->next) {
    ozni_index sf, sn;
    if (nd(s, tfile, c)->kind == OZNI_N_TypeRef && find_from(s, tfile, nd(s, tfile, c)->text, &sf, &sn) &&
        nd(s, sf, sn)->kind == OZNI_N_Type && type_field(s, sf, sn, name, ffile, fnode, depth + 1)) {
      return true;
    }
  }
  return false;
}

static ozni_index field_type_node(const sema *s, ozni_index file, ozni_index field) {
  ozni_index c = nd(s, file, field)->first;
  return c != 0 && nd(s, file, c)->kind != OZNI_N_Default ? c : 0;
}

static ozni_index field_default(const sema *s, ozni_index file, ozni_index field) {
  for (ozni_index c = nd(s, file, field)->first; c != 0; c = nd(s, file, c)->next) {
    if (nd(s, file, c)->kind == OZNI_N_Default) {
      return nd(s, file, c)->first;
    }
  }
  return 0;
}

static bool value_qty(sema *s, ozni_index file, ozni_index node, ozni_qty *out) {
  const ozni_node *n = nd(s, file, node);
  *out = (ozni_qty){0};
  if (n->kind == OZNI_N_Number) {
    ozni_str txt = S(s, n->text);
    out->known = ival_parse(txt.p, txt.len, &out->v);
    return out->known;
  }
  if (n->kind == OZNI_N_Unary && str_is(S(s, n->text), "-") && n->first != 0 && value_qty(s, file, n->first, out)) {
    ozni_rat lo = ozni_rat_neg(out->v.hi), hi = ozni_rat_neg(out->v.lo);
    out->v = (ozni_ival){lo, hi};
    return true;
  }
  if (n->kind == OZNI_N_Quantity) {
    return qty_of(s, mod_of_file(s, file), file, node, out, false);
  }
  return false;
}

static ozni_index base_type(const sema *s, ozni_index tfile, ozni_index tnode, ozni_index *range) {
  const ozni_node *tn = nd(s, tfile, tnode);
  *range = 0;
  if (tn->kind == OZNI_N_OptionalType) {
    tnode = tn->first;
    tn = nd(s, tfile, tnode);
  }
  if (tn->kind == OZNI_N_RangedType) {
    *range = nd(s, tfile, tn->first)->next;
    return tn->first;
  }
  return tnode;
}

static bool typed_qty(sema *s, ozni_index vfile, ozni_index vnode, ozni_index tfile, ozni_index tnode, ozni_qty *out) {
  ozni_index range;
  ozni_index base = base_type(s, tfile, tnode, &range);
  if (!value_qty(s, vfile, vnode, out)) {
    return false;
  }
  int dim = nd(s, tfile, base)->kind == OZNI_N_TypeRef ? dim_index(s, nd(s, tfile, base)->text) : -1;
  if (dim >= 0 && dimensionless(out->d) && str_is(S(s, nd(s, tfile, base)->text), "Count")) {
    out->d.e[dim] = 1;
  }
  return true;
}

static bool range_bound(sema *s, ozni_index tfile, ozni_index node, ozni_qty *out) {
  return value_qty(s, tfile, node, out);
}

static void check_value(sema *s, ozni_index vfile, ozni_index vnode, ozni_index tfile, ozni_index tnode,
                        ozni_index name) {
  const ozni_node *tn = nd(s, tfile, tnode);
  if (tn->kind == OZNI_N_ListType) {
    for (ozni_index c = nd(s, vfile, vnode)->first; nd(s, vfile, vnode)->kind == OZNI_N_List && c != 0;
         c = nd(s, vfile, c)->next) {
      check_value(s, vfile, c, tfile, tn->first, name);
    }
    return;
  }
  ozni_index range;
  ozni_index base = base_type(s, tfile, tnode, &range);
  const ozni_node *bn = nd(s, tfile, base);
  if (bn->kind != OZNI_N_TypeRef) {
    return;
  }
  ozni_index df, dn;
  if (dim_index(s, bn->text) >= 0) {
    ozni_qty v;
    if (!typed_qty(s, vfile, vnode, tfile, tnode, &v)) {
      return;
    }
    ozni_dimvec want = {{0}};
    want.e[dim_index(s, bn->text)] = 1;
    if (!dims_equal(v.d, want)) {
      ozni_str args[2] = {S(s, dim_name_text(s, v.d)), S(s, bn->text)};
      diag_node(s, OZNI_D_TYP_dimension, vfile, nd(s, vfile, vnode), args, 2);
      return;
    }
    if (range == 0) {
      return;
    }
    const ozni_node *rn = nd(s, tfile, range);
    ozni_qty lo = {0}, hi = {0};
    bool has_lo = false, has_hi = false;
    if (rn->kind == OZNI_N_NumberRange) {
      ozni_str txt = S(s, rn->text);
      ozni_ival iv;
      has_lo = has_hi = ival_parse(txt.p, txt.len, &iv);
      lo.v.lo = iv.lo;
      hi.v.hi = iv.hi;
      lo.v.hi = iv.lo;
      hi.v.lo = iv.hi;
    } else if (rn->kind == OZNI_N_Range && rn->first != 0) {
      has_lo = range_bound(s, tfile, rn->first, &lo);
      has_hi = nd(s, tfile, rn->first)->next != 0 && range_bound(s, tfile, nd(s, tfile, rn->first)->next, &hi);
    } else if (rn->kind == OZNI_N_RangeFrom && rn->first != 0) {
      has_lo = range_bound(s, tfile, rn->first, &lo);
    } else if (rn->kind == OZNI_N_RangeTo && rn->first != 0) {
      has_hi = range_bound(s, tfile, rn->first, &hi);
    }
    if ((has_lo && ozni_rat_cmp(v.v.lo, lo.v.lo) < 0) || (has_hi && ozni_rat_cmp(v.v.hi, hi.v.hi) > 0)) {
      ozni_str args[3] = {S(s, name), src_text(s, vfile, nd(s, vfile, vnode)), src_text(s, tfile, rn)};
      diag_node(s, OZNI_D_TYP_rangeViolation, vfile, nd(s, vfile, vnode), args, 3);
    }
  } else if (find_from(s, tfile, bn->text, &df, &dn) && nd(s, df, dn)->kind == OZNI_N_Enum) {
    const ozni_node *vn = nd(s, vfile, vnode);
    bool found = false;
    for (ozni_index c = nd(s, df, dn)->first; vn->kind == OZNI_N_Name && c != 0; c = nd(s, df, c)->next) {
      found = found || (nd(s, df, c)->kind == OZNI_N_Value && nd(s, df, c)->text == vn->text);
    }
    if (!found) {
      ozni_str args[2] = {S(s, bn->text), src_text(s, vfile, vn)};
      diag_node(s, OZNI_D_TYP_axisValue, vfile, vn, args, 2);
    }
  }
}

static bool kind_has(sema *s, ozni_index kfile, ozni_index knode, ozni_index rfile, const ozni_node *req,
                     uint32_t depth) {
  if (depth > 16) {
    return false;
  }
  ozni_index rargs = 0;
  for (ozni_index c = req->first; c != 0; c = nd(s, rfile, c)->next) {
    if (nd(s, rfile, c)->kind == OZNI_N_TypeArgs) {
      rargs = nd(s, rfile, c)->first;
    }
  }
  bool bare = false;
  for (ozni_index a = rargs != 0 ? nd(s, rfile, rargs)->first : 0; a != 0; a = nd(s, rfile, a)->next) {
    bare = bare || nd(s, rfile, a)->kind == OZNI_N_BareArg;
  }
  if (nd(s, kfile, knode)->text == req->text && !bare) {
    return true;
  }
  for (ozni_index c = nd(s, kfile, knode)->first; c != 0; c = nd(s, kfile, c)->next) {
    const ozni_node *cn = nd(s, kfile, c);
    if (cn->kind != OZNI_N_TypeRef) {
      continue;
    }
    if (cn->text == req->text) {
      bool all = true;
      for (ozni_index a = rargs != 0 ? nd(s, rfile, rargs)->first : 0; a != 0; a = nd(s, rfile, a)->next) {
        if (nd(s, rfile, a)->kind != OZNI_N_BareArg) {
          continue;
        }
        bool found = false;
        for (ozni_index ta = cn->first; ta != 0; ta = nd(s, kfile, ta)->next) {
          ozni_index alt = nd(s, kfile, ta)->kind == OZNI_N_TypeArgs ? nd(s, kfile, ta)->first : 0;
          for (ozni_index b = alt != 0 ? nd(s, kfile, alt)->first : 0; b != 0; b = nd(s, kfile, b)->next) {
            found = found || nd(s, kfile, b)->text == nd(s, rfile, a)->text;
          }
        }
        all = all && found;
      }
      if (all) {
        return true;
      }
    }
    ozni_index sf, sn;
    if (find_from(s, kfile, cn->text, &sf, &sn) &&
        (nd(s, sf, sn)->kind == OZNI_N_Kind || nd(s, sf, sn)->kind == OZNI_N_Trait ||
         nd(s, sf, sn)->kind == OZNI_N_Vessel) &&
        kind_has(s, sf, sn, rfile, req, depth + 1)) {
      return true;
    }
  }
  return false;
}

static bool spec_search(sema *s, ozni_index kfile, ozni_index knode, ozni_index *sfile, ozni_index *snode,
                        uint32_t depth) {
  if (depth > 16) {
    return false;
  }
  ozni_str name = S(s, nd(s, kfile, knode)->text);
  char buf[128];
  if (name.len + 4 < sizeof buf) {
    ozni_copy(buf, name.p, name.len);
    ozni_copy(buf + name.len, "Spec", 4);
    if (find_from(s, kfile, intern(s, buf, name.len + 4), sfile, snode) && nd(s, *sfile, *snode)->kind == OZNI_N_Type) {
      return true;
    }
  }
  for (ozni_index c = nd(s, kfile, knode)->first; c != 0; c = nd(s, kfile, c)->next) {
    ozni_index tf, tn;
    if (nd(s, kfile, c)->kind == OZNI_N_TypeRef && find_from(s, kfile, nd(s, kfile, c)->text, &tf, &tn) &&
        (nd(s, tf, tn)->kind == OZNI_N_Kind || nd(s, tf, tn)->kind == OZNI_N_Trait) &&
        spec_search(s, tf, tn, sfile, snode, depth + 1)) {
      return true;
    }
  }
  return false;
}

static void spec_for(sema *s, ozni_index kfile, ozni_index knode, ozni_index *sfile, ozni_index *snode) {
  if (spec_search(s, kfile, knode, sfile, snode, 0)) {
    return;
  }
  const char *fallback = nd(s, kfile, knode)->kind == OZNI_N_Vessel ? "VesselSpec" : "EquipmentSpec";
  if (!find_from(s, kfile, intern(s, fallback, strlen(fallback)), sfile, snode)) {
    *snode = 0;
  }
}

static bool entry_field(sema *s, const kentry *e, const char *fname, ozni_qty *out) {
  ozni_index name = intern(s, fname, strlen(fname));
  ozni_index ff, fn;
  bool typed = e->snode != 0 && type_field(s, e->sfile, e->snode, name, &ff, &fn, 0) &&
               field_type_node(s, ff, fn) != 0;
  for (ozni_index c = e->vnode != 0 ? nd(s, e->vfile, e->vnode)->first : 0; c != 0; c = nd(s, e->vfile, c)->next) {
    const ozni_node *cn = nd(s, e->vfile, c);
    if (cn->kind == OZNI_N_Entry && cn->text == name && cn->first != 0) {
      return typed ? typed_qty(s, e->vfile, cn->first, ff, field_type_node(s, ff, fn), out)
                   : value_qty(s, e->vfile, cn->first, out);
    }
  }
  ozni_index def = typed ? field_default(s, ff, fn) : 0;
  return def != 0 && typed_qty(s, ff, def, ff, field_type_node(s, ff, fn), out);
}

static void add_entry(sema *s, ozni_index name, ozni_index vfile, ozni_index vnode, ozni_index kind_name) {
  ozni_index kf, kn;
  if (!find_from(s, vfile, kind_name, &kf, &kn) ||
      (nd(s, kf, kn)->kind != OZNI_N_Kind && nd(s, kf, kn)->kind != OZNI_N_Vessel)) {
    return;
  }
  kentry e = {name, kf, kn, vfile, vnode, 0, 0};
  spec_for(s, kf, kn, &e.sfile, &e.snode);
  if (vnode != 0 && e.snode != 0) {
    for (ozni_index c = nd(s, vfile, vnode)->first; c != 0; c = nd(s, vfile, c)->next) {
      const ozni_node *cn = nd(s, vfile, c);
      ozni_index ff, fn;
      if (cn->kind == OZNI_N_Entry && cn->first != 0 && type_field(s, e.sfile, e.snode, cn->text, &ff, &fn, 0) &&
          field_type_node(s, ff, fn) != 0) {
        check_value(s, vfile, cn->first, ff, field_type_node(s, ff, fn), cn->text);
      }
    }
  }
  if (!ozni_table_push(s->kentries, &e, nullptr)) {
    s->c->oom = true;
  }
}

static void load_kitchen(sema *s) {
  ozni_index kf = s->kfile, kn = s->knode;
  for (ozni_index sec = nd(s, kf, kn)->first; sec != 0; sec = nd(s, kf, sec)->next) {
    if (nd(s, kf, sec)->kind != OZNI_N_Section) {
      continue;
    }
    for (ozni_index f = nd(s, kf, sec)->first; f != 0; f = nd(s, kf, f)->next) {
      const ozni_node *fn = nd(s, kf, f);
      const ozni_node *v = fn->kind == OZNI_N_Field && fn->first != 0 ? nd(s, kf, fn->first) : nullptr;
      if (v != nullptr && v->kind == OZNI_N_Construct) {
        add_entry(s, fn->text, kf, fn->first, v->text);
      } else if (v != nullptr && v->kind == OZNI_N_Name) {
        add_entry(s, fn->text, kf, 0, v->text);
      }
    }
  }
  ozni_index tf, tn, pf, pn;
  if (!find_def(s, intern(s, "Kitchen", 7), &tf, &tn) || !type_field(s, tf, tn, intern(s, "places", 6), &pf, &pn, 0)) {
    return;
  }
  ozni_index rec = field_type_node(s, pf, pn);
  for (ozni_index c = rec != 0 ? nd(s, pf, rec)->first : 0; c != 0; c = nd(s, pf, c)->next) {
    const ozni_node *cn = nd(s, pf, c);
    ozni_index def = cn->kind == OZNI_N_TypeField ? field_default(s, pf, c) : 0;
    bool present = false;
    for (ozni_index k = 0; k < ozni_table_count(s->kentries); k++) {
      present = present || ((const kentry *)ozni_table_get(s->kentries, k))->name == cn->text;
    }
    if (!present && def != 0 && nd(s, pf, def)->kind == OZNI_N_Name) {
      add_entry(s, cn->text, pf, 0, nd(s, pf, def)->text);
    }
  }
}

static ozni_index options_value(const sema *s, ozni_index id) {
  for (ozni_index c = s->onode != 0 ? nd(s, s->ofile, s->onode)->first : 0; c != 0; c = nd(s, s->ofile, c)->next) {
    if (nd(s, s->ofile, c)->kind == OZNI_N_Field && nd(s, s->ofile, c)->text == id) {
      return nd(s, s->ofile, c)->first;
    }
  }
  return 0;
}

static bool check_operand(sema *s, ozni_index tfile, ozni_index node, ozni_qty *out) {
  const ozni_node *n = nd(s, tfile, node);
  if (!tick(s, tfile, n)) {
    return false;
  }
  if (n->kind == OZNI_N_Member && n->first != 0 && !tick(s, tfile, nd(s, tfile, n->first))) {
    return false;
  }
  if (n->kind == OZNI_N_Member && n->first != 0 && nd(s, tfile, n->first)->first != 0 &&
      !tick(s, tfile, nd(s, tfile, nd(s, tfile, n->first)->first))) {
    return false;
  }
  if (n->kind == OZNI_N_Name) {
    ozni_index tf = s->otfile, tn = s->otnode, ff, fn;
    if (tn == 0 || !type_field(s, tf, tn, n->text, &ff, &fn, 0) || field_type_node(s, ff, fn) == 0) {
      return false;
    }
    ozni_index v = options_value(s, n->text);
    if (v != 0) {
      return typed_qty(s, s->ofile, v, ff, field_type_node(s, ff, fn), out);
    }
    ozni_index def = field_default(s, ff, fn);
    return def != 0 && typed_qty(s, ff, def, ff, field_type_node(s, ff, fn), out);
  }
  if (n->kind == OZNI_N_Member && n->first != 0) {
    const ozni_node *inner = nd(s, tfile, n->first);
    if (inner->kind == OZNI_N_Member && inner->first != 0 && nd(s, tfile, inner->first)->kind == OZNI_N_Name &&
        str_is(S(s, nd(s, tfile, inner->first)->text), "kitchen")) {
      for (ozni_index k = 0; s->kentries != nullptr && k < ozni_table_count(s->kentries); k++) {
        const kentry *e = ozni_table_get(s->kentries, k);
        if (e->name == inner->text) {
          ozni_str field = S(s, n->text);
          char buf[64];
          if (field.len >= sizeof buf) {
            return false;
          }
          ozni_copy(buf, field.p, field.len);
          buf[field.len] = '\0';
          return entry_field(s, e, buf, out);
        }
      }
    }
    return false;
  }
  return value_qty(s, tfile, node, out);
}

static bool refers_kitchen(const sema *s, ozni_index file, ozni_index node) {
  const ozni_node *n = nd(s, file, node);
  if (n->kind == OZNI_N_Name && str_is(S(s, n->text), "kitchen")) {
    return true;
  }
  for (ozni_index c = n->first; c != 0; c = nd(s, file, c)->next) {
    if (refers_kitchen(s, file, c)) {
      return true;
    }
  }
  return false;
}

static void run_checks(sema *s, ozni_index tfile, ozni_index tnode, uint32_t depth) {
  if (depth > 16) {
    return;
  }
  for (ozni_index c = nd(s, tfile, tnode)->first; c != 0; c = nd(s, tfile, c)->next) {
    ozni_index sf, sn;
    if (nd(s, tfile, c)->kind == OZNI_N_TypeRef && find_from(s, tfile, nd(s, tfile, c)->text, &sf, &sn) &&
        nd(s, sf, sn)->kind == OZNI_N_Type) {
      run_checks(s, sf, sn, depth + 1);
    }
  }
  for (ozni_index c = nd(s, tfile, tnode)->first; c != 0; c = nd(s, tfile, c)->next) {
    const ozni_node *cn = nd(s, tfile, c);
    const ozni_node *expr = cn->kind == OZNI_N_Check && cn->first != 0 ? nd(s, tfile, cn->first) : nullptr;
    if (expr == nullptr || expr->kind != OZNI_N_Binary || (refers_kitchen(s, tfile, cn->first) && s->knode == 0) ||
        !tick(s, tfile, expr)) {
      continue;
    }
    ozni_qty l, r;
    if (!check_operand(s, tfile, expr->first, &l) || !check_operand(s, tfile, nd(s, tfile, expr->first)->next, &r)) {
      continue;
    }
    int count = dim_named(s, "Count");
    if (count >= 0 && dimensionless(r.d) && l.d.e[count] == 1) {
      r.d = l.d;
    } else if (count >= 0 && dimensionless(l.d) && r.d.e[count] == 1) {
      l.d = r.d;
    }
    if (!dims_equal(l.d, r.d)) {
      continue;
    }
    ozni_str op = S(s, expr->text);
    int lo = ozni_rat_cmp(l.v.hi, r.v.lo), hi = ozni_rat_cmp(l.v.lo, r.v.hi);
    bool holds = str_is(op, "<=") ? lo <= 0 : str_is(op, "<") ? lo < 0 : str_is(op, ">=") ? hi >= 0
               : str_is(op, ">") ? hi > 0 : true;
    if (holds) {
      continue;
    }
    ozni_diag_id id;
    ozni_str code = S(s, cn->text2);
    bool user = tfile < s->c->nuser;
    if (cn->text2 == 0 || (!user && !ozni_diag_lookup(code.p, code.len, &id))) {
      continue;
    }
    const ozni_node *on = nd(s, s->ofile, s->onode);
    diag(s, user ? OZNI_D_TYP_type : id, s->ofile, on->name_start, on->name_end, nullptr, 0);
    if (user && ozni_table_count(s->c->diags) > 0) {
      ozni_diag *d = ozni_table_at(s->c->diags, ozni_table_count(s->c->diags) - 1);
      ozni_buf b = {.a = s->c->allocator};
      if (s->pr->root_pkg != NONE) {
        const ozni_package *pkg = ozni_table_get(s->pr->packages, s->pr->root_pkg);
        ozni_str pname = S(s, pkg->name);
        ozni_buf_put(&b, pname.p, pname.len);
        ozni_buf_puts(&b, ":");
      }
      ozni_buf_put(&b, code.p, code.len);
      if (b.failed) {
        s->c->oom = true;
      } else {
        d->code_text = intern(s, b.p, b.len) + 1;
        d->name_text = cn->text + 1;
      }
      ozni_buf_discard(&b);
    }
    ozni_str sev = S(s, cn->text3);
    uint32_t nd_count = ozni_table_count(s->c->diags);
    if (nd_count > 0) {
      ozni_diag *d = ozni_table_at(s->c->diags, nd_count - 1);
      d->severity = str_is(sev, "warn") ? OZNI_SEV_WARNING : str_is(sev, "info") ? OZNI_SEV_INFO : OZNI_SEV_ERROR;
    }
  }
}

static ozni_index find_config(sema *s, const char *name) {
  if (name == nullptr) {
    return 0;
  }
  ozni_index id = intern(s, name, strlen(name));
  for (ozni_index mi = 0; mi < ozni_table_count(s->pr->modules); mi++) {
    const ozni_module *m = mod_at(s, mi);
    for (ozni_index c = m->is_std ? 0 : nd(s, m->file, m->root)->first; c != 0; c = nd(s, m->file, c)->next) {
      if (nd(s, m->file, c)->kind == OZNI_N_Kitchen && nd(s, m->file, c)->text == id) {
        s->mod = mi;
        return c;
      }
    }
  }
  ozni_str arg = S(s, id);
  diag(s, OZNI_D_NAM_undefined, 0, 0, 0, &arg, 1);
  return 0;
}

static void setup_configs(sema *s) {
  s->kentries = ozni_table_new(s->c->arena, sizeof(kentry));
  if (s->kentries == nullptr) {
    s->c->oom = true;
    return;
  }
  s->knode = find_config(s, s->c->set.kitchen);
  s->kfile = mod_at(s, s->mod)->file;
  if (s->knode != 0) {
    load_kitchen(s);
    s->level = 2;
  }
  s->onode = find_config(s, s->c->set.options);
  s->ofile = mod_at(s, s->mod)->file;
  ozni_index tf, tn;
  ozni_index annot = s->onode != 0 ? nd(s, s->ofile, s->onode)->first : 0;
  bool typed = annot != 0 && nd(s, s->ofile, annot)->kind == OZNI_N_TypeRef &&
               find_from(s, s->ofile, nd(s, s->ofile, annot)->text, &tf, &tn) && nd(s, tf, tn)->kind == OZNI_N_Type;
  if (s->onode == 0 || (!typed && !find_def(s, intern(s, "Options", 7), &tf, &tn))) {
    return;
  }
  s->otfile = tf;
  s->otnode = tn;
  for (ozni_index c = nd(s, s->ofile, s->onode)->first; c != 0; c = nd(s, s->ofile, c)->next) {
    const ozni_node *cn = nd(s, s->ofile, c);
    ozni_index ff, fn;
    if (cn->kind == OZNI_N_Field && cn->first != 0 && type_field(s, tf, tn, cn->text, &ff, &fn, 0) &&
        field_type_node(s, ff, fn) != 0) {
      check_value(s, s->ofile, cn->first, ff, field_type_node(s, ff, fn), cn->text);
      for (ozni_index a = nd(s, ff, fn)->first; s->knode == 0 && a != 0; a = nd(s, ff, a)->next) {
        const ozni_node *an = nd(s, ff, a);
        if (an->kind == OZNI_N_Annotation && str_is(S(s, an->text), "needs") && an->first != 0 &&
            str_is(S(s, nd(s, ff, an->first)->text), "kitchen")) {
          ozni_str arg = S(s, cn->text);
          diag_node(s, OZNI_D_TYP_optionLevel, s->ofile, nd(s, s->ofile, cn->first), &arg, 1);
        }
      }
    }
    if (cn->kind == OZNI_N_Field && (str_is(S(s, cn->text), "servings") || str_is(S(s, cn->text), "cooks")) &&
        s->level < 1) {
      s->level = 1;
    }
  }
  run_checks(s, tf, tn, 0);
}

static value eval_in(sema *s, ozni_index file, ozni_index node, const value *params, const ozni_index *pnames,
                     uint32_t np) {
  ozni_index saved_file = s->file, saved_mod = s->mod;
  ozni_table *saved_scope = s->scope;
  s->scope = ozni_table_new(s->c->arena, sizeof(binding));
  if (s->scope == nullptr) {
    s->c->oom = true;
    s->scope = saved_scope;
    return unknown();
  }
  for (uint32_t k = 0; k < np; k++) {
    binding b = {pnames[k], params[k]};
    if (!ozni_table_push(s->scope, &b, nullptr)) {
      s->c->oom = true;
    }
  }
  s->file = file;
  s->mod = mod_of_file(s, file);
  value v = eval(s, node);
  s->file = saved_file;
  s->mod = saved_mod;
  s->scope = saved_scope;
  return v;
}

static bool req_met(sema *s, ozni_index rfile, ozni_index rnode, const value *params, const ozni_index *pnames,
                    uint32_t np) {
  const ozni_node *req = nd(s, rfile, rnode);
  ozni_index df, dn;
  if (find_from(s, rfile, req->text, &df, &dn) && nd(s, df, dn)->kind == OZNI_N_Resource) {
    return true;
  }
  for (ozni_index k = 0; k < ozni_table_count(s->kentries); k++) {
    const kentry *e = ozni_table_get(s->kentries, k);
    ozni_qty count;
    if ((entry_field(s, e, "count", &count) && ozni_rat_cmp(count.v.hi, ozni_rat_int(0)) <= 0) ||
        !kind_has(s, e->kfile, e->knode, rfile, req, 0)) {
      continue;
    }
    bool ok = true;
    for (ozni_index ta = req->first; ta != 0; ta = nd(s, rfile, ta)->next) {
      ozni_index alt = nd(s, rfile, ta)->kind == OZNI_N_TypeArgs ? nd(s, rfile, ta)->first : 0;
      for (ozni_index a = alt != 0 ? nd(s, rfile, alt)->first : 0; a != 0; a = nd(s, rfile, a)->next) {
        const ozni_node *an = nd(s, rfile, a);
        if (an->kind != OZNI_N_StateArg || an->first == 0) {
          continue;
        }
        ozni_str field = S(s, an->text);
        char buf[64];
        ozni_qty have;
        if (field.len >= sizeof buf) {
          continue;
        }
        ozni_copy(buf, field.p, field.len);
        buf[field.len] = '\0';
        value need = eval_in(s, rfile, an->first, params, pnames, np);
        if (!entry_field(s, e, buf, &have) || need.kind != V_QTY || !need.q.known || !dims_equal(have.d, need.q.d)) {
          continue;
        }
        ozni_str op = S(s, an->text2);
        if ((str_is(op, ">=") && ozni_rat_cmp(have.v.lo, need.q.v.hi) < 0) ||
            (str_is(op, "<=") && ozni_rat_cmp(have.v.hi, need.q.v.lo) > 0)) {
          ok = false;
        }
      }
    }
    if (ok) {
      return true;
    }
  }
  return false;
}

static bool variant_met(sema *s, ozni_index vfile, ozni_index vnode, const value *params, const ozni_index *pnames,
                        uint32_t np) {
  for (ozni_index u = nd(s, vfile, vnode)->first; u != 0; u = nd(s, vfile, u)->next) {
    if (nd(s, vfile, u)->kind != OZNI_N_Uses) {
      continue;
    }
    for (ozni_index r = nd(s, vfile, u)->first; r != 0; r = nd(s, vfile, r)->next) {
      if (nd(s, vfile, r)->kind == OZNI_N_TypeRef && !req_met(s, vfile, r, params, pnames, np)) {
        return false;
      }
    }
  }
  return true;
}

static int str_order(ozni_str x, ozni_str y) {
  size_t m = x.len < y.len ? x.len : y.len;
  int r = memcmp(x.p, y.p, m);
  return r != 0 ? r : (x.len < y.len ? -1 : x.len > y.len ? 1 : 0);
}

static uint32_t action_extends(sema *s, ozni_index afile, ozni_index anode, act_ref *out, uint32_t cap) {
  uint32_t n = 0;
  ozni_str pkgs[64];
  for (ozni_index mi = 0; mi < ozni_table_count(s->pr->modules); mi++) {
    const ozni_module *m = mod_at(s, mi);
    const ozni_package *pkg = m->pkg != NONE ? ozni_table_get(s->pr->packages, m->pkg) : nullptr;
    ozni_str pname = pkg != nullptr ? S(s, pkg->name) : (ozni_str){"", 0};
    for (ozni_index c = nd(s, m->file, m->root)->first; c != 0 && n < cap && n < 64; c = nd(s, m->file, c)->next) {
      ozni_index tf, tn;
      if (nd(s, m->file, c)->kind != OZNI_N_Extend || !find_from(s, m->file, nd(s, m->file, c)->text, &tf, &tn) ||
          tf != afile || tn != anode) {
        continue;
      }
      uint32_t j = n++;
      while (j > 0 && str_order(pkgs[j - 1], pname) > 0) {
        out[j] = out[j - 1];
        pkgs[j] = pkgs[j - 1];
        j--;
      }
      out[j] = (act_ref){m->file, c, 0};
      pkgs[j] = pname;
    }
  }
  return n;
}

static void push_req(sema *s, ozni_index text) {
  for (ozni_index k = 0; k < ozni_table_count(s->reqs); k++) {
    if (*(const ozni_index *)ozni_table_get(s->reqs, k) == text) {
      return;
    }
  }
  if (!ozni_table_push(s->reqs, &text, nullptr)) {
    s->c->oom = true;
  }
}

static void push_uses(sema *s, ozni_index file, ozni_index uses) {
  for (ozni_index c = nd(s, file, uses)->first; c != 0; c = nd(s, file, c)->next) {
    const ozni_node *cn = nd(s, file, c);
    ozni_index rf, rn;
    if (cn->kind != OZNI_N_TypeRef ||
        (find_from(s, file, cn->text, &rf, &rn) && nd(s, rf, rn)->kind == OZNI_N_Resource)) {
      continue;
    }
    ozni_str text = src_text(s, file, cn);
    push_req(s, intern(s, text.p, text.len));
  }
}

static uint32_t variant_list(sema *s, ozni_index afile, ozni_index anode, act_ref *out, uint32_t cap) {
  act_ref ext[16];
  uint32_t n = 0, next = action_extends(s, afile, anode, ext, 16);
  for (uint32_t e = 0; e <= next; e++) {
    ozni_index f = e == 0 ? afile : ext[e - 1].file, d = e == 0 ? anode : ext[e - 1].node;
    for (ozni_index c = nd(s, f, d)->first; c != 0 && n < cap; c = nd(s, f, c)->next) {
      if (nd(s, f, c)->kind == OZNI_N_Variant) {
        out[n++] = (act_ref){f, c, 0};
      }
    }
  }
  return n;
}

static void note_requirements(sema *s, ozni_index file, const ozni_node *call, ozni_index afile, ozni_index anode,
                              const value *params, const ozni_index *pnames, uint32_t np) {
  act_ref vars[32];
  uint32_t nv = variant_list(s, afile, anode, vars, 32);
  if (nv == 0) {
    for (ozni_index c = nd(s, afile, anode)->first; c != 0; c = nd(s, afile, c)->next) {
      if (nd(s, afile, c)->kind != OZNI_N_Uses) {
        continue;
      }
      push_uses(s, afile, c);
      for (ozni_index r = nd(s, afile, c)->first; s->knode != 0 && r != 0; r = nd(s, afile, r)->next) {
        if (nd(s, afile, r)->kind == OZNI_N_TypeRef && !req_met(s, afile, r, params, pnames, np)) {
          ozni_str arg = src_text(s, afile, nd(s, afile, r));
          diag_node(s, OZNI_D_KIT_requiresUnmet, file, call, &arg, 1);
        }
      }
    }
    return;
  }
  uint32_t usable = 0;
  for (uint32_t k = 0; k < nv; k++) {
    if (s->knode == 0 || variant_met(s, vars[k].file, vars[k].node, params, pnames, np)) {
      usable |= 1u << k;
    }
  }
  if (s->knode == 0 && s->entry_dish && nv > 1) {
    ozni_buf b = {.a = s->c->allocator};
    for (uint32_t k = 1; k < nv; k++) {
      ozni_buf_puts(&b, k == 1 ? "" : ", ");
      ozni_str v = S(s, nd(s, vars[k].file, vars[k].node)->text);
      ozni_buf_put(&b, v.p, v.len);
    }
    if (b.failed) {
      s->c->oom = true;
    } else {
      ozni_str args[3] = {S(s, nd(s, afile, anode)->text), S(s, nd(s, vars[0].file, vars[0].node)->text),
                          {b.p, (uint32_t)b.len}};
      diag_node(s, OZNI_D_ASM_firstVariant, file, call, args, 3);
    }
    ozni_buf_discard(&b);
  }
  if (usable == 0) {
    ozni_buf b = {.a = s->c->allocator};
    for (uint32_t k = 0; k < nv; k++) {
      ozni_buf_puts(&b, k == 0 ? "" : ", ");
      ozni_str v = S(s, nd(s, vars[k].file, vars[k].node)->text);
      ozni_buf_put(&b, v.p, v.len);
    }
    if (b.failed) {
      s->c->oom = true;
    } else {
      ozni_str args[2] = {S(s, nd(s, afile, anode)->text), {b.p, (uint32_t)b.len}};
      diag_node(s, OZNI_D_KIT_noVariant, file, call, args, 2);
    }
    ozni_buf_discard(&b);
  }
  for (ozni_index k = 0; k < ozni_table_count(s->variant_acts); k++) {
    const act_ref *r = ozni_table_get(s->variant_acts, k);
    if (r->file == afile && r->node == anode) {
      return;
    }
  }
  act_ref r = {afile, anode, usable};
  if (!ozni_table_push(s->variant_acts, &r, nullptr)) {
    s->c->oom = true;
  }
}

static void put_requirements(sema *s) {
  ozni_buf *b = s->out;
  uint32_t n = ozni_table_count(s->reqs);
  ozni_index order[64];
  for (uint32_t k = 0; k < n && k < 64; k++) {
    order[k] = *(const ozni_index *)ozni_table_get(s->reqs, k);
    for (uint32_t j = k; j > 0 && str_order(S(s, order[j - 1]), S(s, order[j])) > 0; j--) {
      ozni_index tmp = order[j - 1];
      order[j - 1] = order[j];
      order[j] = tmp;
    }
  }
  for (uint32_t k = 0; k < n && k < 64; k++) {
    ozni_buf_puts(b, k == 0 ? "  requires " : ", ");
    ozni_str r = S(s, order[k]);
    ozni_buf_put(b, r.p, r.len);
  }
  if (n > 0) {
    ozni_buf_puts(b, "\n");
  }
  for (ozni_index k = 0; k < ozni_table_count(s->variant_acts); k++) {
    const act_ref *r = ozni_table_get(s->variant_acts, k);
    ozni_buf_puts(b, "  variants ");
    ozni_str name = S(s, nd(s, r->file, r->node)->text);
    ozni_buf_put(b, name.p, name.len);
    ozni_buf_puts(b, ": ");
    act_ref vars[32];
    uint32_t nv = variant_list(s, r->file, r->node, vars, 32);
    bool first = true;
    for (uint32_t v = 0; v < nv; v++) {
      if ((r->usable & (1u << v)) == 0) {
        continue;
      }
      ozni_buf_puts(b, first ? "" : " | ");
      ozni_str vn = S(s, nd(s, vars[v].file, vars[v].node)->text);
      ozni_buf_put(b, vn.p, vn.len);
      first = false;
    }
    ozni_buf_puts(b, "\n");
  }
}

static value butcher(sema *s, const ozni_node *act, ozni_index afile, ozni_index output, const value *params,
                     const ozni_index *pnodes, uint32_t np, const value *shown, uint32_t nshown,
                     const ozni_index *shown_names) {
  const food *in = nullptr;
  for (ozni_index item = nd(s, afile, output)->first; item != 0 && in == nullptr; item = nd(s, afile, item)->next) {
    ozni_index tv = type_var_of(s, afile, item);
    for (uint32_t k = 0; tv != 0 && k < np && in == nullptr; k++) {
      ozni_index ptype = nd(s, afile, pnodes[k])->first;
      if (ptype != 0 && type_var_of(s, afile, ptype) == tv && params[k].kind == V_FOOD) {
        in = params[k].f;
      }
    }
  }
  food *outs[9];
  uint32_t n = 0, open = 0;
  ozni_ival used = {ozni_rat_int(0), ozni_rat_int(0)};
  bool amounts = in != nullptr && in->amount.known;
  for (ozni_index item = nd(s, afile, output)->first; item != 0 && n < 9; item = nd(s, afile, item)->next) {
    food *o = in != nullptr ? food_copy(s, in) : nullptr;
    if (o == nullptr) {
      return unknown();
    }
    o->portion = true;
    ozni_index part = 0;
    for (ozni_index ta = nd(s, afile, item)->first; ta != 0; ta = nd(s, afile, ta)->next) {
      ozni_index alt = nd(s, afile, ta)->kind == OZNI_N_TypeArgs ? nd(s, afile, ta)->first : 0;
      for (ozni_index a = alt != 0 ? nd(s, afile, alt)->first : 0; a != 0; a = nd(s, afile, a)->next) {
        if (nd(s, afile, a)->kind == OZNI_N_BareArg) {
          part = nd(s, afile, a)->text;
        }
      }
    }
    ozni_index pf, pn;
    ozni_qty y;
    o->part = part;
    o->pfile = o->pnode = 0;
    o->ncands = 0;
    o->cands = nullptr;
    if (part != 0 && in->file != NONE && part_lookup(s, in->file, in->node, part, &pf, &pn, 0)) {
      o->pfile = pf;
      o->pnode = pn;
      o->discard = part_discard(s, pf, pn);
    }
    cand c = self_cand(o);
    if (amounts && o->pnode != 0 && part_prop(s, &c, o->pfile, o->pnode, "yield", &y, 0) && dimensionless(y.d)) {
      ozni_ival v;
      if (!ival_mul(in->amount.v, y.v, &v) || ozni_ival_add(used, v, &used) != OZNI_OK) {
        amounts = false;
      }
      o->amount.v = v;
    } else {
      o->amount.known = false;
      open++;
    }
    outs[n++] = o;
  }
  ozni_ival rest;
  if (amounts && ozni_ival_sub(in->amount.v, used, &rest) == OZNI_OK) {
    if (ozni_rat_cmp(rest.lo, ozni_rat_int(0)) < 0) {
      rest.lo = ozni_rat_int(0);
    }
    for (uint32_t k = 0; k < n; k++) {
      if (!outs[k]->amount.known) {
        outs[k]->amount = in->amount;
        outs[k]->amount.v = rest;
        if (open > 1) {
          outs[k]->amount.v.lo = ozni_rat_int(0);
        }
      }
    }
  } else {
    for (uint32_t k = 0; k < n; k++) {
      outs[k]->amount.known = false;
    }
  }
  for (uint32_t k = 0; k < n; k++) {
    if (!ozni_table_push(s->portions, &outs[k], nullptr)) {
      s->c->oom = true;
    }
  }
  ozni_index ref = record_step(s, act->text, shown, nshown, shown_names, nullptr, outs, n);
  return make_tuple(s, ref, outs, n);
}

static bool fix_candidate(sema *s, ozni_index file, ozni_index node, const food *f, ozni_index afile,
                          ozni_index ptype) {
  const ozni_node *act = nd(s, file, node);
  ozni_index food_type = 0, output = 0;
  uint32_t required = 0;
  for (ozni_index c = act->first; c != 0; c = nd(s, file, c)->next) {
    const ozni_node *cn = nd(s, file, c);
    if (cn->kind == OZNI_N_Param && field_default(s, file, c) == 0) {
      required++;
      food_type = field_type_node(s, file, c);
    } else if (cn->kind == OZNI_N_Output) {
      output = cn->first;
    }
  }
  if (required != 1 || food_type == 0 || output == 0 || type_var_of(s, file, food_type) == 0 ||
      type_var_of(s, file, output) == 0 || !satisfies(s, f, file, food_type)) {
    return false;
  }
  food *trial = food_copy(s, f);
  if (trial == nullptr) {
    return false;
  }
  s->quiet++;
  apply_effects(s, trial, file, output, nullptr, nullptr, 0, file, act);
  s->quiet--;
  return satisfies(s, trial, afile, ptype);
}

static void suggest_fix(sema *s, const food *f, ozni_index afile, ozni_index anode, ozni_index ptype, ozni_str arg) {
  ozni_index order[2] = {s->pr->prelude, s->mod};
  for (int pass = 0; pass < 2; pass++) {
    if (order[pass] == NONE) {
      continue;
    }
    const ozni_module *m = mod_at(s, order[pass]);
    for (ozni_index d = 0; d < ozni_table_count(m->defs); d++) {
      const ozni_def *e = def_at(s, order[pass], d);
      if ((m->file == afile && e->node == anode) || nd(s, m->file, e->node)->kind != OZNI_N_Action ||
          !fix_candidate(s, m->file, e->node, f, afile, ptype)) {
        continue;
      }
      ozni_buf b = {.a = s->c->allocator};
      ozni_str name = S(s, nd(s, m->file, e->node)->text);
      ozni_buf_put(&b, name.p, name.len);
      ozni_buf_puts(&b, "(");
      ozni_buf_put(&b, arg.p, arg.len);
      ozni_buf_puts(&b, ")");
      uint32_t count = ozni_table_count(s->c->diags);
      if (b.failed) {
        s->c->oom = true;
      } else if (count > 0) {
        ((ozni_diag *)ozni_table_at(s->c->diags, count - 1))->fix = intern(s, b.p, b.len) + 1;
      }
      ozni_buf_discard(&b);
      return;
    }
  }
}

static void arg_site(const sema *s, ozni_index file, ozni_index node, ozni_index *sfile, ozni_index *snode,
                     ozni_index *strail) {
  const mframe *f = s->frame;
  *sfile = file;
  *snode = node;
  *strail = s->trail;
  while (f != nullptr && *sfile == f->mfile && nd(s, *sfile, *snode)->kind == OZNI_N_Splice) {
    ozni_index param = nd(s, *sfile, *snode)->text;
    uint32_t k = f->nargs;
    for (uint32_t j = 0; j < f->nargs; j++) {
      k = f->params[j] == param ? j : k;
    }
    if (k == f->nargs || f->args[k] == 0) {
      return;
    }
    *sfile = f->cfile;
    *snode = f->args[k];
    *strail = f->ctrail;
    f = f->parent;
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
      bool known = false;
      for (uint32_t k = 0; k < np; k++) {
        if (pnames[k] != name) {
          continue;
        }
        known = true;
        if (set[k]) {
          ozni_str args[2] = {S(s, act->text), S(s, name)};
          diag_node(s, OZNI_D_NAM_duplicateArgument, file, an, args, 2);
          continue;
        }
        params[k] = v;
        set[k] = true;
        shown_of[k] = nshown;
        argnodes[k] = an->first;
      }
      if (!known) {
        ozni_str args[2] = {S(s, act->text), S(s, name)};
        diag_node(s, OZNI_D_NAM_unknownArgument, file, an, args, 2);
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
      } else {
        ozni_str arg_name = S(s, act->text);
        diag_node(s, OZNI_D_NAM_arity, file, an, &arg_name, 1);
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
    bool has_default = false;
    for (ozni_index c = nd(s, afile, pnodes[k])->first; c != 0; c = nd(s, afile, c)->next) {
      if (nd(s, afile, c)->kind == OZNI_N_Default && nd(s, afile, c)->first != 0) {
        has_default = true;
        ozni_index saved_file = s->file;
        s->file = afile;
        params[k] = eval(s, nd(s, afile, c)->first);
        s->file = saved_file;
      }
    }
    if (!has_default) {
      ozni_str args[2] = {S(s, act->text), S(s, pnames[k])};
      diag_node(s, OZNI_D_NAM_arity, file, call, args, 2);
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
  note_requirements(s, file, call, afile, anode, params, pnames, np);
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
    const food *pf = params[k].f;
    ozni_index sfile, snode, strail, saved_trail = s->trail;
    arg_site(s, file, argnodes[k], &sfile, &snode, &strail);
    const ozni_node *argn = nd(s, sfile, snode);
    ozni_str arg_text = src_text(s, sfile, argn);
    s->trail = strail;
    if (ptype != 0 && pf->file != NONE && !satisfies(s, pf, afile, ptype)) {
      ozni_index failing = 0;
      bool some = false;
      for (uint32_t c = 0; c < pf->ncands; c++) {
        food one = *pf;
        const cand *cd = &pf->cands[c];
        one.type = cd->type;
        one.file = cd->file;
        one.node = cd->node;
        one.part = cd->part;
        one.pfile = cd->pfile;
        one.pnode = cd->pnode;
        one.ncands = 0;
        one.cands = nullptr;
        if (satisfies(s, &one, afile, ptype)) {
          some = true;
        } else if (failing == 0) {
          failing = cd->part != 0 ? cd->part : cd->type;
        }
      }
      if (some && failing != 0) {
        ozni_str args[4] = {S(s, act->text), S(s, pnames[k]), arg_text, S(s, failing)};
        diag_node(s, OZNI_D_TYP_aliasCandidates, sfile, argn, args, 4);
      } else {
        ozni_str args[3] = {S(s, act->text), S(s, pnames[k]), arg_text};
        diag_node(s, OZNI_D_TYP_stateRequired, sfile, argn, args, 3);
        if (s->quiet == 0) {
          suggest_fix(s, pf, afile, anode, ptype, arg_text);
        }
      }
    }
    ozni_index tv = ptype != 0 ? type_var_of(s, afile, ptype) : 0;
    for (ozni_index w = act->first; tv != 0 && w != 0; w = nd(s, afile, w)->next) {
      const ozni_node *wn = nd(s, afile, w);
      if (wn->kind != OZNI_N_Where || wn->text != tv || wn->first == 0) {
        continue;
      }
      ozni_index trait = nd(s, afile, wn->first)->text;
      if (!food_has(s, pf, trait)) {
        ozni_str args[4] = {S(s, act->text), S(s, pnames[k]), arg_text, S(s, trait)};
        diag_node(s, OZNI_D_TYP_whereUnsatisfied, sfile, argn, args, 4);
      }
    }
    s->trail = saved_trail;
  }
  for (uint32_t k = 0; k < np; k++) {
    if (params[k].kind == V_FOOD && argnodes[k] != 0 && nd(s, file, argnodes[k])->kind != OZNI_N_List) {
      ozni_index sfile, snode, strail, saved_trail = s->trail;
      arg_site(s, file, argnodes[k], &sfile, &snode, &strail);
      s->trail = strail;
      consume(s, &params[k], sfile, snode);
      s->trail = saved_trail;
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
  if (output != 0 && nd(s, afile, output)->kind == OZNI_N_TupleType) {
    return butcher(s, act, afile, output, params, pnodes, np, shown, nshown, shown_names);
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
  result.ref = record_step(s, act->text, shown, nshown, shown_names, out, nullptr, 0);
  return result;
}

static value compare(sema *s, const ozni_node *n, ozni_str op, value l, value r) {
  if (!dims_equal(l.q.d, r.q.d)) {
    ozni_str args[2] = {S(s, dim_name_text(s, l.q.d)), S(s, dim_name_text(s, r.q.d))};
    diag_node(s, OZNI_D_TYP_dimension, s->file, n, args, 2);
    return unknown();
  }
  bool lt_sure = ozni_rat_cmp(l.q.v.hi, r.q.v.lo) < 0, le_sure = ozni_rat_cmp(l.q.v.hi, r.q.v.lo) <= 0;
  bool gt_sure = ozni_rat_cmp(l.q.v.lo, r.q.v.hi) > 0, ge_sure = ozni_rat_cmp(l.q.v.lo, r.q.v.hi) >= 0;
  bool points = ozni_rat_cmp(l.q.v.lo, l.q.v.hi) == 0 && ozni_rat_cmp(r.q.v.lo, r.q.v.hi) == 0;
  bool eq_sure = points && ozni_rat_cmp(l.q.v.lo, r.q.v.lo) == 0, ne_sure = lt_sure || gt_sure;
  bool yes, no;
  if (str_is(op, "<")) {
    yes = lt_sure, no = ge_sure;
  } else if (str_is(op, "<=")) {
    yes = le_sure, no = gt_sure;
  } else if (str_is(op, ">")) {
    yes = gt_sure, no = le_sure;
  } else if (str_is(op, ">=")) {
    yes = ge_sure, no = lt_sure;
  } else if (str_is(op, "==")) {
    yes = eq_sure, no = ne_sure;
  } else {
    yes = ne_sure, no = eq_sure;
  }
  if (!yes && !no) {
    return (value){.kind = V_UNDECIDED};
  }
  return (value){.kind = V_BOOL, .b = yes};
}

static value divide_food(sema *s, value l, value r) {
  food *f = l.f;
  if (!f->amount.known || !r.q.known) {
    return unknown();
  }
  value out = {.kind = V_QTY};
  if (!dimensionless(r.q.d)) {
    if (!dims_equal(f->amount.d, r.q.d) || !ival_divide(f->amount.v, r.q.v, &out.q.v)) {
      return unknown();
    }
    out.q.known = true;
    return out;
  }
  food *part = food_copy(s, f);
  if (part == nullptr || !ival_divide(f->amount.v, r.q.v, &part->amount.v)) {
    return unknown();
  }
  ozni_rat share;
  if (ozni_rat_cmp(r.q.v.lo, r.q.v.hi) != 0 || ozni_rat_div(ozni_rat_int(1), r.q.v.lo, &share) != OZNI_OK) {
    return unknown();
  }
  if (!f->divided) {
    f->divided = true;
    f->used = ozni_rat_int(0);
  }
  if (ozni_rat_add(f->used, share, &f->used) != OZNI_OK) {
    return unknown();
  }
  if (ozni_rat_cmp(f->used, ozni_rat_int(1)) >= 0) {
    f->consumed = true;
  }
  ozni_buf b = {.a = s->c->allocator};
  ozni_str src = S(s, l.ref);
  ozni_buf_put(&b, src.p, src.len);
  ozni_buf_puts(&b, "/");
  put_rat(&b, r.q.v.lo);
  value pv = {.kind = V_FOOD, .f = part};
  if (b.failed) {
    s->c->oom = true;
  } else {
    pv.ref = intern(s, b.p, b.len);
  }
  ozni_buf_discard(&b);
  return pv;
}

static value eval_binary(sema *s, const ozni_node *n) {
  ozni_str op = S(s, n->text);
  value l = eval(s, n->first);
  if ((str_is(op, "and") || str_is(op, "or")) && l.kind == V_BOOL) {
    if (l.b == str_is(op, "or")) {
      return l;
    }
    value r = eval(s, nd(s, s->file, n->first)->next);
    return r.kind == V_BOOL || r.kind == V_UNDECIDED ? r : unknown();
  }
  value r = eval(s, nd(s, s->file, n->first)->next);
  if (l.kind == V_FOOD && r.kind == V_QTY && str_is(op, "/")) {
    return divide_food(s, l, r);
  }
  if (l.kind != V_QTY || r.kind != V_QTY || !l.q.known || !r.q.known) {
    return unknown();
  }
  if (str_is(op, "<") || str_is(op, "<=") || str_is(op, ">") || str_is(op, ">=") || str_is(op, "==") ||
      str_is(op, "!=")) {
    return compare(s, n, op, l, r);
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

static value make_tuple(sema *s, ozni_index ref, food *const *outs, uint32_t n) {
  value out = {.kind = V_TUPLE, .ref = ref};
  out.items = ozni_table_new(s->c->arena, sizeof(value));
  if (out.items == nullptr) {
    s->c->oom = true;
    return out;
  }
  for (uint32_t k = 0; k < n; k++) {
    value item = {.kind = V_FOOD, .f = outs[k], .ref = sub_ref(s, ref, (int)k + 1)};
    if (!ozni_table_push(out.items, &item, nullptr)) {
      s->c->oom = true;
    }
  }
  return out;
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
  value shown[2] = {fv, qv};
  food *outs[2] = {portion, rest};
  ozni_index ref = record_step(s, intern(s, "split", 5), shown, 2, nullptr, nullptr, outs, 2);
  return make_tuple(s, ref, outs, 2);
}

static bool counted(ozni_node_kind k) {
  return k == OZNI_N_Name || k == OZNI_N_Quantity || k == OZNI_N_Number || k == OZNI_N_NumberRange ||
         k == OZNI_N_String || k == OZNI_N_MlString || k == OZNI_N_Bool || k == OZNI_N_List || k == OZNI_N_Record ||
         k == OZNI_N_Binary || k == OZNI_N_Unary || k == OZNI_N_Not || k == OZNI_N_Member || k == OZNI_N_Call ||
         k == OZNI_N_Make || k == OZNI_N_Buy || k == OZNI_N_Choose || k == OZNI_N_Assemble;
}

static value eval_inner(sema *s, ozni_index node);

static value eval(sema *s, ozni_index node) {
  const ozni_node *n = nd(s, s->file, node);
  if (s->c->oom || s->c->limit_hit) {
    return unknown();
  }
  if (!enter(s, s->file, n) || (counted(n->kind) && !tick(s, s->file, n))) {
    s->depth--;
    return unknown();
  }
  value v = eval_inner(s, node);
  s->depth--;
  return v;
}

static value eval_splice(sema *s, const ozni_node *n) {
  uint32_t k;
  const mframe *f = frame_for(s, n->text, &k);
  if (f == nullptr || f->args[k] == 0) {
    return unknown();
  }
  ozni_index saved_file = s->file, saved_mod = s->mod, saved_trail = s->trail;
  ozni_table *saved_scope = s->scope;
  const mframe *saved_frame = s->frame;
  s->file = f->cfile;
  s->mod = f->cmod;
  s->scope = f->cscope;
  s->trail = f->ctrail;
  s->frame = f->parent;
  value v = eval(s, f->args[k]);
  s->file = saved_file;
  s->mod = saved_mod;
  s->scope = saved_scope;
  s->trail = saved_trail;
  s->frame = saved_frame;
  return v;
}

static uint32_t subtree_size(const sema *s, ozni_index file, ozni_index node) {
  uint32_t n = 1;
  for (ozni_index c = nd(s, file, node)->first; c != 0; c = nd(s, file, c)->next) {
    n += subtree_size(s, file, c);
  }
  return n;
}

static value eval_macro(sema *s, const ozni_node *call, ozni_index mfile, ozni_index mnode) {
  const ozni_node *macro = nd(s, mfile, mnode);
  mframe f = {.parent = s->frame, .mfile = mfile, .mnode = mnode, .cfile = s->file, .cmod = s->mod,
              .cscope = s->scope, .ctrail = s->trail};
  uint32_t np = 0;
  for (ozni_index c = macro->first; c != 0; c = nd(s, mfile, c)->next) {
    if (nd(s, mfile, c)->kind == OZNI_N_MacroParam && np < 16) {
      f.params[np++] = nd(s, mfile, c)->text;
    }
  }
  uint32_t na = 0;
  for (ozni_index a = nd(s, s->file, call->first)->next; a != 0; a = nd(s, s->file, a)->next) {
    if (nd(s, s->file, a)->kind == OZNI_N_Annotation) {
      continue;
    }
    if (na < 16) {
      f.args[na] = a;
    }
    na++;
  }
  if (na != np) {
    ozni_str arg = S(s, macro->text);
    diag_node(s, OZNI_D_NAM_arity, s->file, call, &arg, 1);
    return unknown();
  }
  f.nargs = np;
  ozni_index quote_block = 0;
  for (ozni_index c = macro->first; c != 0; c = nd(s, mfile, c)->next) {
    const ozni_node *cn = nd(s, mfile, c);
    if (cn->kind == OZNI_N_Block && cn->first != 0 && nd(s, mfile, cn->first)->kind == OZNI_N_Quote) {
      quote_block = nd(s, mfile, cn->first)->first;
    }
  }
  if (quote_block == 0) {
    return unknown();
  }
  if (mfile < s->c->nuser) {
    s->c->nodes_used += subtree_size(s, mfile, quote_block);
    if (s->c->nodes_used > s->c->lim.nodes) {
      limit_diag(s, OZNI_D_MAC_nodeLimit, s->file, call, s->c->lim.nodes);
      return unknown();
    }
  }
  ozni_trail tr = {{s->file, call->span.start, call->span.end}, s->trail};
  ozni_index tr_index;
  if (!ozni_table_push(s->c->trails, &tr, &tr_index)) {
    s->c->oom = true;
    return unknown();
  }
  ozni_index saved_file = s->file, saved_mod = s->mod, saved_trail = s->trail;
  ozni_table *saved_scope = s->scope;
  const mframe *saved_frame = s->frame;
  bool top = s->top;
  s->scope = ozni_table_new(s->c->arena, sizeof(binding));
  if (s->scope == nullptr) {
    s->c->oom = true;
    s->scope = saved_scope;
    return unknown();
  }
  s->file = mfile;
  s->mod = mod_of_file(s, mfile);
  s->trail = tr_index + 1;
  s->frame = &f;
  eval_block(s, quote_block, top);
  s->file = saved_file;
  s->mod = saved_mod;
  s->scope = saved_scope;
  s->trail = saved_trail;
  s->frame = saved_frame;
  return unknown();
}

static value eval_member(sema *s, const ozni_node *n) {
  const ozni_node *inner = n->first != 0 ? nd(s, s->file, n->first) : nullptr;
  if (inner == nullptr || inner->kind != OZNI_N_Member || inner->first == 0 ||
      nd(s, s->file, inner->first)->kind != OZNI_N_Name || !str_is(S(s, nd(s, s->file, inner->first)->text), "kitchen") ||
      s->knode == 0) {
    return unknown();
  }
  for (ozni_index k = 0; k < ozni_table_count(s->kentries); k++) {
    const kentry *e = ozni_table_get(s->kentries, k);
    if (e->name != inner->text) {
      continue;
    }
    ozni_str field = S(s, n->text);
    char buf[64];
    value v = {.kind = V_QTY};
    if (field.len >= sizeof buf) {
      return unknown();
    }
    ozni_copy(buf, field.p, field.len);
    buf[field.len] = '\0';
    return entry_field(s, e, buf, &v.q) ? v : unknown();
  }
  return unknown();
}

static value eval_builtin(sema *s, const ozni_node *call, ozni_str name) {
  ozni_index a0 = nd(s, s->file, call->first)->next;
  ozni_index a1 = a0 != 0 ? nd(s, s->file, a0)->next : 0;
  if (str_is(name, "ceil") || str_is(name, "floor")) {
    value v = a0 != 0 ? eval(s, a0) : unknown();
    if (v.kind != V_QTY || !v.q.known) {
      return unknown();
    }
    bool up = str_is(name, "ceil");
    v.q.v.lo = up ? ozni_rat_ceil(v.q.v.lo) : ozni_rat_floor(v.q.v.lo);
    v.q.v.hi = up ? ozni_rat_ceil(v.q.v.hi) : ozni_rat_floor(v.q.v.hi);
    return v;
  }
  if ((str_is(name, "min") || str_is(name, "max")) && a1 != 0) {
    value l = eval(s, a0), r = eval(s, a1);
    if (l.kind != V_QTY || r.kind != V_QTY || !l.q.known || !r.q.known || !dims_equal(l.q.d, r.q.d)) {
      return unknown();
    }
    bool mn = str_is(name, "min");
    value out = l;
    out.q.v.lo = (ozni_rat_cmp(l.q.v.lo, r.q.v.lo) < 0) == mn ? l.q.v.lo : r.q.v.lo;
    out.q.v.hi = (ozni_rat_cmp(l.q.v.hi, r.q.v.hi) < 0) == mn ? l.q.v.hi : r.q.v.hi;
    return out;
  }
  if (str_is(name, "len") && a0 != 0) {
    value l = eval(s, a0);
    if (l.kind != V_LIST || l.items == nullptr) {
      return unknown();
    }
    value out = {.kind = V_QTY};
    out.q.v.lo = out.q.v.hi = ozni_rat_int(ozni_table_count(l.items));
    out.q.known = true;
    return out;
  }
  return unknown();
}

static value eval_inner(sema *s, ozni_index node) {
  const ozni_node *n = nd(s, s->file, node);
  if (n->kind == OZNI_N_Splice) {
    return eval_splice(s, n);
  }
  if (n->kind == OZNI_N_Bool) {
    return (value){.kind = V_BOOL, .b = str_is(S(s, n->text), "true")};
  }
  if (n->kind == OZNI_N_Not && n->first != 0) {
    value v = eval(s, n->first);
    if (v.kind == V_BOOL) {
      v.b = !v.b;
    }
    return v;
  }
  if (n->kind == OZNI_N_Unary && n->first != 0) {
    value v = eval(s, n->first);
    if (v.kind == V_QTY && v.q.known && str_is(S(s, n->text), "-")) {
      ozni_rat lo = ozni_rat_neg(v.q.v.hi), hi = ozni_rat_neg(v.q.v.lo);
      v.q.v = (ozni_ival){lo, hi};
    }
    return v;
  }
  if (n->kind == OZNI_N_Member) {
    return eval_member(s, n);
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
  if (n->kind == OZNI_N_Make) {
    return eval_make(s, n);
  }
  if (n->kind == OZNI_N_Buy) {
    return eval_buy(s, n);
  }
  if (n->kind == OZNI_N_Choose) {
    return eval_choose(s, n);
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
    if (callee->kind == OZNI_N_Name && find_def(s, callee->text, &afile, &anode) &&
        nd(s, afile, anode)->kind == OZNI_N_Macro) {
      return eval_macro(s, n, afile, anode);
    }
    if (callee->kind == OZNI_N_Member && str_is(S(s, callee->text), "has") && callee->first != 0 &&
        nd(s, s->file, callee->first)->kind == OZNI_N_Name &&
        str_is(S(s, nd(s, s->file, callee->first)->text), "kitchen") && callee->next != 0) {
      value out = {.kind = V_BOOL, .b = s->knode == 0};
      const ozni_node *want = nd(s, s->file, callee->next);
      for (ozni_index k = 0; s->knode != 0 && k < ozni_table_count(s->kentries); k++) {
        const kentry *e = ozni_table_get(s->kentries, k);
        ozni_qty count;
        bool none = entry_field(s, e, "count", &count) && ozni_rat_cmp(count.v.hi, ozni_rat_int(0)) <= 0;
        out.b = out.b || (!none && kind_has(s, e->kfile, e->knode, s->file, want, 0));
      }
      return out;
    }
    if (callee->kind == OZNI_N_Name && (str_is(S(s, callee->text), "ceil") || str_is(S(s, callee->text), "floor") ||
                                        str_is(S(s, callee->text), "min") || str_is(S(s, callee->text), "max") ||
                                        str_is(S(s, callee->text), "len"))) {
      return eval_builtin(s, n, S(s, callee->text));
    }
    if (callee->kind == OZNI_N_Name && str_is(S(s, callee->text), "volume") && callee->next != 0) {
      value fv = eval(s, callee->next);
      value out = {.kind = V_QTY};
      return fv.kind == V_FOOD && to_volume(s, fv.f, &out.q) ? out : unknown();
    }
    for (ozni_index c = callee->next; c != 0; c = nd(s, s->file, c)->next) {
      eval(s, c);
    }
    return unknown();
  }
  return unknown();
}

static void eval_ingredients(sema *s, const ozni_node *n) {
  for (ozni_index it = n->first; it != 0 && !s->c->limit_hit; it = nd(s, s->file, it)->next) {
    const ozni_node *item = nd(s, s->file, it);
    if (!tick(s, s->file, item)) {
      return;
    }
    ozni_index type = 0, amount = 0, tnode = 0;
    for (ozni_index c = item->first; c != 0; c = nd(s, s->file, c)->next) {
      const ozni_node *cn = nd(s, s->file, c);
      if (cn->kind == OZNI_N_TypeRef) {
        type = cn->text;
        tnode = c;
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
    food *f = tnode != 0 ? food_typed(s, s->file, tnode, true, 0) : food_new(s, type);
    if (f == nullptr) {
      return;
    }
    if (tnode == 0 && f->file != NONE) {
      add_contains_traits(s, f, f->file, f->node, 0);
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
    if (printing(s)) {
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

static const char *kind_word(vkind k) {
  return k == V_QTY ? "Quantity" : k == V_FOOD ? "Food" : k == V_STR ? "String" : k == V_LIST ? "List"
       : k == V_UNDECIDED ? "Range" : k == V_BOOL ? "Bool" : "Value";
}

static void type_error(sema *s, const ozni_node *at, const char *want, vkind got) {
  ozni_str args[2] = {{want, (uint32_t)strlen(want)}, {kind_word(got), (uint32_t)strlen(kind_word(got))}};
  diag_node(s, OZNI_D_TYP_type, s->file, at, args, 2);
}

static bool action_call(sema *s, ozni_index node) {
  const ozni_node *n = nd(s, s->file, node);
  ozni_index df, dn;
  if (n->kind == OZNI_N_Make) {
    return true;
  }
  return n->kind == OZNI_N_Call && n->first != 0 && nd(s, s->file, n->first)->kind == OZNI_N_Name &&
         find_def(s, nd(s, s->file, n->first)->text, &df, &dn) &&
         (nd(s, df, dn)->kind == OZNI_N_Action || nd(s, df, dn)->kind == OZNI_N_Macro);
}

static void check_targets(sema *s, const ozni_node *holder, ozni_index expr) {
  for (ozni_index c = holder->first; c != 0; c = nd(s, s->file, c)->next) {
    const ozni_node *an = nd(s, s->file, c);
    if (an->kind == OZNI_N_Annotation && str_is(S(s, an->text), "unattended") && (expr == 0 || !action_call(s, expr))) {
      ozni_str arg = S(s, an->text);
      diag_node(s, OZNI_D_TYP_annotationTarget, s->file, an, &arg, 1);
    }
  }
}

static void tick_annotations(sema *s, const ozni_node *n) {
  for (ozni_index c = n->first; c != 0; c = nd(s, s->file, c)->next) {
    if (nd(s, s->file, c)->kind == OZNI_N_Annotation) {
      tick(s, s->file, nd(s, s->file, c));
    }
  }
}

static void eval_scoped(sema *s, ozni_index block, bool top) {
  ozni_index mark = ozni_table_count(s->scope);
  eval_block(s, block, top);
  ozni_table_truncate(s->scope, mark);
}

static void eval_if(sema *s, const ozni_node *n, bool top) {
  if (!tick(s, s->file, n) || n->first == 0) {
    return;
  }
  value cond = eval(s, n->first);
  ozni_index then = nd(s, s->file, n->first)->next;
  ozni_index other = then != 0 ? nd(s, s->file, then)->next : 0;
  if (cond.kind == V_BOOL) {
    ozni_index pick = cond.b ? then : other;
    if (pick != 0 && nd(s, s->file, pick)->kind == OZNI_N_If) {
      eval_if(s, nd(s, s->file, pick), top);
    } else if (pick != 0) {
      eval_scoped(s, pick, top);
    }
  } else if (cond.kind != V_UNKNOWN && cond.kind != V_NONE && cond.kind != V_ENUM) {
    type_error(s, nd(s, s->file, n->first), "Bool", cond.kind);
  }
}

static bool count_of(const value *v, uint32_t *out) {
  if (v->kind != V_QTY || !v->q.known || ozni_rat_cmp(v->q.v.lo, v->q.v.hi) != 0 || v->q.v.lo.den != 1 ||
      v->q.v.lo.num < 0 || v->q.v.lo.num > 100000) {
    return false;
  }
  *out = (uint32_t)v->q.v.lo.num;
  return true;
}

static void eval_repeat(sema *s, const ozni_node *n, bool top) {
  if (!tick(s, s->file, n) || n->first == 0) {
    return;
  }
  value count = eval(s, n->first);
  ozni_index body = nd(s, s->file, n->first)->next;
  uint32_t times;
  if (count.kind == V_UNKNOWN) {
    return;
  }
  if (!count_of(&count, &times)) {
    type_error(s, nd(s, s->file, n->first), "Count", count.kind == V_QTY ? V_UNDECIDED : count.kind);
    return;
  }
  for (uint32_t k = 0; k < times && body != 0 && !s->c->limit_hit && !s->c->oom; k++) {
    eval_scoped(s, body, top);
  }
}

static void eval_for(sema *s, const ozni_node *n, bool top) {
  if (!tick(s, s->file, n) || n->first == 0) {
    return;
  }
  value list = eval(s, n->first);
  ozni_index body = nd(s, s->file, n->first)->next;
  if (list.kind == V_UNKNOWN) {
    return;
  }
  if (list.kind != V_LIST || list.items == nullptr) {
    type_error(s, nd(s, s->file, n->first), "List", list.kind);
    return;
  }
  for (ozni_index k = 0; k < ozni_table_count(list.items) && body != 0 && !s->c->limit_hit; k++) {
    ozni_index mark = ozni_table_count(s->scope);
    bind(s, n->text, *(const value *)ozni_table_get(list.items, k));
    eval_block(s, body, top);
    ozni_table_truncate(s->scope, mark);
  }
}

static const mframe *frame_for(const sema *s, ozni_index param, uint32_t *index) {
  for (uint32_t k = 0; s->frame != nullptr && k < s->frame->nargs; k++) {
    if (s->frame->params[k] == param) {
      *index = k;
      return s->frame;
    }
  }
  return nullptr;
}

static void bind_splice(sema *s, const ozni_node *let, value v) {
  ozni_str name = S(s, let->text);
  uint32_t k;
  const mframe *f = name.len > 1 ? frame_for(s, intern(s, name.p + 1, name.len - 1), &k) : nullptr;
  if (f == nullptr || f->args[k] == 0) {
    return;
  }
  const ozni_node *arg = nd(s, f->cfile, f->args[k]);
  if (arg->kind != OZNI_N_Name) {
    diag_node(s, OZNI_D_TYP_spliceNotName, s->file, let, &name, 1);
    return;
  }
  binding b = {arg->text, v};
  if (!ozni_table_push(f->cscope, &b, nullptr)) {
    s->c->oom = true;
  }
}

static void eval_block(sema *s, ozni_index block, bool top) {
  const ozni_node *bn = nd(s, s->file, block);
  if (!enter(s, s->file, bn) || !tick(s, s->file, bn)) {
    s->depth--;
    return;
  }
  for (ozni_index st = bn->first; st != 0 && !s->c->oom && !s->c->limit_hit; st = nd(s, s->file, st)->next) {
    const ozni_node *n = nd(s, s->file, st);
    s->top = top;
    if (n->kind == OZNI_N_Ingredients) {
      if (tick(s, s->file, n)) {
        eval_ingredients(s, n);
      }
    } else if (n->kind == OZNI_N_Let) {
      if (!tick(s, s->file, n)) {
        break;
      }
      tick_annotations(s, n);
      check_targets(s, n, n->first);
      value v = n->first != 0 ? eval(s, n->first) : unknown();
      if (v.kind == V_FOOD) {
        label(v.f, n->text, (ozni_span){s->file, n->name_start, n->name_end});
        if (top) {
          s->last = v;
          s->last_node = n->first;
        }
      }
      if (S(s, n->text).len > 0 && S(s, n->text).p[0] == '$') {
        bind_splice(s, n, v);
      } else {
        bind(s, n->text, v);
      }
    } else if (n->kind == OZNI_N_LetTuple) {
      if (!tick(s, s->file, n)) {
        break;
      }
      value v = unknown();
      ozni_index expr = 0;
      for (ozni_index c = n->first; c != 0; c = nd(s, s->file, c)->next) {
        if (nd(s, s->file, c)->kind != OZNI_N_BindName && nd(s, s->file, c)->kind != OZNI_N_Annotation) {
          expr = c;
        }
      }
      tick_annotations(s, n);
      if (expr != 0) {
        v = eval(s, expr);
      }
      if (top && v.kind == V_TUPLE) {
        s->last = v;
        s->last_node = expr;
      }
      ozni_index k = 0;
      for (ozni_index c = n->first; c != 0; c = nd(s, s->file, c)->next) {
        const ozni_node *bnd = nd(s, s->file, c);
        if (bnd->kind != OZNI_N_BindName) {
          continue;
        }
        value item = unknown();
        if (v.kind == V_TUPLE && v.items != nullptr && k < ozni_table_count(v.items)) {
          item = *(const value *)ozni_table_get(v.items, k);
        }
        if (item.kind == V_FOOD) {
          label(item.f, bnd->text, bnd->span);
        }
        bind(s, bnd->text, item);
        k++;
      }
    } else if (n->kind == OZNI_N_Parallel) {
      if (!tick(s, s->file, n)) {
        break;
      }
      uint32_t saved_group = s->group, saved_branch = s->branch;
      s->group = ++s->next_id;
      for (ozni_index b = n->first; b != 0 && !s->c->limit_hit; b = nd(s, s->file, b)->next) {
        s->branch = ++s->next_id;
        if (tick(s, s->file, nd(s, s->file, b)) && nd(s, s->file, b)->first != 0) {
          eval_scoped(s, nd(s, s->file, b)->first, false);
        }
      }
      s->group = saved_group;
      s->branch = saved_branch;
    } else if (n->kind == OZNI_N_Serve) {
      if (tick(s, s->file, n)) {
        eval_serve(s, n);
      }
    } else if (n->kind == OZNI_N_If) {
      eval_if(s, n, top);
    } else if (n->kind == OZNI_N_ForIn) {
      eval_for(s, n, top);
    } else if (n->kind == OZNI_N_Repeat) {
      eval_repeat(s, n, top);
    } else {
      if (n->kind == OZNI_N_Call) {
        check_targets(s, n, st);
      }
      value v = eval(s, st);
      s->top = top;
      if (top && v.kind == V_FOOD) {
        s->last = v;
        s->last_node = st;
      }
    }
  }
  s->depth--;
}

static void plate_capacity(sema *s, const ozni_node *item, const food *f, ozni_index vessel) {
  ozni_qty per = f->amount;
  if (!per.known || (s->servings.known && !ival_divide(per.v, s->servings.v, &per.v))) {
    return;
  }
  for (ozni_index k = 0; s->knode != 0 && k < ozni_table_count(s->kentries); k++) {
    const kentry *e = ozni_table_get(s->kentries, k);
    ozni_qty cap;
    if (nd(s, e->kfile, e->knode)->text != vessel || !entry_field(s, e, "capacity", &cap)) {
      continue;
    }
    food tmp = *f;
    tmp.amount = per;
    ozni_qty have, need;
    if (amount_in(s, &tmp, cap, &need, &have) && ozni_rat_cmp(need.v.hi, have.v.lo) > 0) {
      ozni_buf pb = {.a = s->c->allocator}, cb = {.a = s->c->allocator};
      put_qty(s, &pb, per);
      put_qty(s, &cb, cap);
      if (pb.failed || cb.failed) {
        s->c->oom = true;
      } else {
        ozni_str args[3] = {S(s, vessel), {pb.p, (uint32_t)pb.len}, {cb.p, (uint32_t)cb.len}};
        diag_node(s, OZNI_D_SCH_vesselOverflow, s->file, item, args, 3);
      }
      ozni_buf_discard(&pb);
      ozni_buf_discard(&cb);
      return;
    }
  }
}

static void put_serve_header(sema *s, const ozni_node *serve) {
  ozni_buf *b = s->out;
  ozni_buf_puts(b, "  serve ");
  ozni_str name = S(s, serve->text);
  ozni_buf_put(b, name.p, name.len);
  for (ozni_index c = serve->first; c != 0; c = nd(s, s->file, c)->next) {
    const ozni_node *cn = nd(s, s->file, c);
    if (cn->kind != OZNI_N_After) {
      continue;
    }
    ozni_buf_puts(b, " after ");
    ozni_str prev = S(s, cn->text);
    ozni_buf_put(b, prev.p, prev.len);
    ozni_qty gap;
    if (cn->first != 0 && qty_of(s, s->mod, s->file, cn->first, &gap, true)) {
      ozni_buf_puts(b, " + ");
      put_qty(s, b, gap);
    }
  }
  ozni_buf_puts(b, "\n");
}

static void plate_annotations(sema *s, const ozni_node *plate) {
  for (ozni_index c = plate->first; c != 0; c = nd(s, s->file, c)->next) {
    const ozni_node *cn = nd(s, s->file, c);
    ozni_index pf, pnode;
    if (cn->kind != OZNI_N_Annotation || !find_def(s, cn->text, &pf, &pnode) ||
        nd(s, pf, pnode)->kind != OZNI_N_Property) {
      continue;
    }
    const ozni_node *body = nd(s, pf, pnode)->first != 0 ? nd(s, pf, nd(s, pf, pnode)->first) : nullptr;
    if (body == nullptr || body->kind != OZNI_N_Call) {
      continue;
    }
  }
}

static void eval_serve(sema *s, const ozni_node *serve) {
  ozni_index hold = intern(s, "HoldsLiquid", 11);
  if (printing(s)) {
    put_serve_header(s, serve);
  }
  for (ozni_index p = serve->first; p != 0; p = nd(s, s->file, p)->next) {
    const ozni_node *pn = nd(s, s->file, p);
    if (pn->kind != OZNI_N_Plate) {
      continue;
    }
    if (!tick(s, s->file, pn)) {
      return;
    }
    plate_annotations(s, pn);
    ozni_buf line = {.a = s->c->allocator};
    ozni_buf_puts(&line, "    plate ");
    ozni_str pname = S(s, pn->text);
    ozni_buf_put(&line, pname.p, pname.len);
    bool first_item = true;
    ozni_index on = 0;
    for (ozni_index c = pn->first; c != 0; c = nd(s, s->file, c)->next) {
      if (nd(s, s->file, c)->kind == OZNI_N_On) {
        on = c;
      }
    }
    if (on != 0) {
      bool first_vessel = true;
      ozni_buf_puts(&line, " on");
      for (ozni_index vs = nd(s, s->file, on)->first; vs != 0; vs = nd(s, s->file, vs)->next) {
        ozni_buf_puts(&line, first_vessel ? " " : " or ");
        ozni_str vname = S(s, nd(s, s->file, vs)->text);
        ozni_buf_put(&line, vname.p, vname.len);
        first_vessel = false;
      }
    }
    for (ozni_index c = pn->first; c != 0; c = nd(s, s->file, c)->next) {
      const ozni_node *item = nd(s, s->file, c);
      if (item->kind == OZNI_N_On || item->kind == OZNI_N_Annotation) {
        continue;
      }
      value v = eval(s, c);
      ozni_buf_puts(&line, first_item ? " { " : ", ");
      first_item = false;
      if (v.kind == V_FOOD || v.kind == V_TUPLE) {
        ozni_str r = S(s, v.ref);
        ozni_buf_put(&line, r.p, r.len);
      }
      if (v.kind != V_FOOD) {
        continue;
      }
      bool liquid = v.f->liquid || food_has(s, v.f, intern(s, "Liquid", 6));
      bool first = true;
      for (ozni_index vs = on != 0 ? nd(s, s->file, on)->first : 0; vs != 0; vs = nd(s, s->file, vs)->next) {
        const ozni_node *vn = nd(s, s->file, vs);
        ozni_index df, dn;
        if (vn->kind != OZNI_N_TypeRef || !find_def(s, vn->text, &df, &dn)) {
          continue;
        }
        if (liquid && vn->text != hold && !decl_has_trait(s, df, dn, hold, 0)) {
          ozni_str args[2] = {src_text(s, s->file, item), S(s, vn->text)};
          diag_node(s, OZNI_D_TYP_vesselLiquid, s->file, item, args, 2);
        }
        if (first) {
          plate_capacity(s, item, v.f, vn->text);
          first = false;
        }
      }
    }
    ozni_buf_puts(&line, first_item ? "\n" : " }\n");
    if (line.failed) {
      s->c->oom = true;
    } else if (printing(s)) {
      ozni_buf_put(s->out, line.p, line.len);
    }
    ozni_buf_discard(&line);
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
    bool output = s->last.kind == V_FOOD && s->last.f == f;
    for (ozni_index i = 0; s->last.kind == V_TUPLE && s->last.items != nullptr && i < ozni_table_count(s->last.items);
         i++) {
      output = output || ((const value *)ozni_table_get(s->last.items, i))->f == f;
    }
    if (f->consumed || f->discard || output || f->label == 0) {
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

static bool is_material(sema *s, ozni_index file, ozni_index type) {
  ozni_index range;
  const ozni_node *bn = nd(s, file, base_type(s, file, type, &range));
  ozni_index df, dn;
  if (bn->kind != OZNI_N_TypeRef || dim_index(s, bn->text) >= 0 || !find_from(s, file, bn->text, &df, &dn)) {
    return false;
  }
  ozni_node_kind k = nd(s, df, dn)->kind;
  return k == OZNI_N_Ingredient || k == OZNI_N_Alias || k == OZNI_N_Trait;
}

static void name_span(const ozni_node *n, ozni_str name, uint32_t *start, uint32_t *end) {
  *start = n->name_end > n->name_start ? n->name_start : n->span.start;
  *end = n->name_end > n->name_start ? n->name_end : n->span.start + name.len;
}

static void put_vessels(sema *s, ozni_buf *b, ozni_index file, ozni_index node) {
  const ozni_node *n = nd(s, file, node);
  if (n->kind == OZNI_N_Binary && n->first != 0) {
    put_vessels(s, b, file, n->first);
    ozni_buf_puts(b, " or ");
    put_vessels(s, b, file, nd(s, file, n->first)->next);
  } else {
    ozni_str v = S(s, n->text);
    ozni_buf_put(b, v.p, v.len);
  }
}

static void wrap_entry(sema *s, ozni_index file, const ozni_node *dish) {
  ozni_index vessel = 0;
  for (ozni_index c = dish->first; c != 0; c = nd(s, file, c)->next) {
    if (nd(s, file, c)->kind == OZNI_N_Annotation && str_is(S(s, nd(s, file, c)->text), "vessel") &&
        nd(s, file, c)->first != 0) {
      vessel = nd(s, file, c)->first;
    }
  }
  if (vessel == 0) {
    ozni_str arg = S(s, dish->text);
    uint32_t start, end;
    name_span(dish, arg, &start, &end);
    diag(s, OZNI_D_ASM_noPlating, file, start, end, &arg, 1);
    return;
  }
  if (!printing(s)) {
    return;
  }
  ozni_buf *b = s->out;
  ozni_buf_puts(b, "  serve main\n    plate perPerson on ");
  put_vessels(s, b, file, vessel);
  ozni_buf_puts(b, " { ");
  if (s->last.kind == V_FOOD || s->last.kind == V_TUPLE) {
    ozni_str r = S(s, s->last.ref);
    ozni_buf_put(b, r.p, r.len);
  }
  ozni_buf_puts(b, " }\n");
}

static void eval_param(sema *s, ozni_index file, ozni_index param) {
  const ozni_node *pn = nd(s, file, param);
  ozni_index type = field_type_node(s, file, param), def = field_default(s, file, param);
  value bound;
  if (lookup_local(s, pn->text, &bound)) {
    return;
  }
  if (type != 0 && is_material(s, file, type)) {
    food *f = food_typed(s, file, type, false, 0);
    if (f == nullptr) {
      return;
    }
    bind(s, pn->text, (value){.kind = V_FOOD, .f = f, .ref = pn->text});
    if (s->entry_dish) {
      ozni_str args[2] = {S(s, pn->text), src_text(s, file, nd(s, file, type))};
      uint32_t start, end;
      name_span(pn, args[0], &start, &end);
      diag(s, OZNI_D_ASM_externalIngredient, file, start, end, args, 2);
    }
    return;
  }
  ozni_index vfile = file, vnode = def;
  ozni_index over = s->onode != 0 ? options_value(s, pn->text) : 0;
  if (over != 0) {
    vfile = s->ofile;
    vnode = over;
  }
  if (vnode == 0) {
    return;
  }
  ozni_qty q = {0};
  if (type != 0) {
    check_value(s, vfile, vnode, file, type, pn->text);
    typed_qty(s, vfile, vnode, file, type, &q);
  } else {
    value_qty(s, vfile, vnode, &q);
  }
  if (str_is(S(s, pn->text), "servings")) {
    s->servings = q;
  }
  if (q.known) {
    bind(s, pn->text, (value){.kind = V_QTY, .q = q});
  }
}

static ozni_index capitalized(sema *s, ozni_index name) {
  ozni_str nm = S(s, name);
  char buf[256];
  if (nm.len == 0 || nm.len >= sizeof buf) {
    return name;
  }
  ozni_copy(buf, nm.p, nm.len);
  if (buf[0] >= 'a' && buf[0] <= 'z') {
    buf[0] = (char)(buf[0] - 'a' + 'A');
  }
  return intern(s, buf, nm.len);
}

static ozni_index prefixed(sema *s, const char *prefix, ozni_index name) {
  ozni_str nm = S(s, name);
  char buf[256];
  size_t n = strlen(prefix);
  if (n + nm.len >= sizeof buf) {
    return name;
  }
  ozni_copy(buf, prefix, n);
  ozni_copy(buf + n, nm.p, nm.len);
  return intern(s, buf, n + nm.len);
}

typedef struct saved_dish {
  ozni_index file, mod;
  ozni_table *scope, *portions, *reqs, *variant_acts;
  value last;
  ozni_index last_node;
  uint32_t step_no, group, branch;
  ozni_qty servings;
  bool entry_dish;
} saved_dish;

static saved_dish save_dish(const sema *s) {
  return (saved_dish){s->file,     s->mod,       s->scope,   s->portions, s->reqs,       s->variant_acts,
                      s->last,     s->last_node, s->step_no, s->group,    s->branch,     s->servings,
                      s->entry_dish};
}

static void restore_dish(sema *s, const saved_dish *d) {
  s->file = d->file;
  s->mod = d->mod;
  s->scope = d->scope;
  s->portions = d->portions;
  s->reqs = d->reqs;
  s->variant_acts = d->variant_acts;
  s->last = d->last;
  s->last_node = d->last_node;
  s->step_no = d->step_no;
  s->group = d->group;
  s->branch = d->branch;
  s->servings = d->servings;
  s->entry_dish = d->entry_dish;
}

static value eval_make(sema *s, const ozni_node *mk) {
  const ozni_node *call = mk->first != 0 ? nd(s, s->file, mk->first) : nullptr;
  ozni_index rfile, rnode;
  if (call == nullptr || call->kind != OZNI_N_Call || call->first == 0 || s->make_depth > 16 ||
      nd(s, s->file, call->first)->kind != OZNI_N_Name ||
      !find_def(s, nd(s, s->file, call->first)->text, &rfile, &rnode) ||
      nd(s, rfile, rnode)->kind != OZNI_N_Recipe) {
    return unknown();
  }
  const ozni_node *rec = nd(s, rfile, rnode);
  ozni_index pnames[32], pnodes[32];
  value vals[32], shown[32];
  ozni_index shown_names[32];
  bool set[32];
  uint32_t np = 0, nshown = 0, pos = 0;
  for (ozni_index c = rec->first; c != 0 && np < 32; c = nd(s, rfile, c)->next) {
    if (nd(s, rfile, c)->kind == OZNI_N_Param) {
      pnames[np] = nd(s, rfile, c)->text;
      pnodes[np] = c;
      set[np++] = false;
    }
  }
  for (ozni_index arg = nd(s, s->file, call->first)->next; arg != 0; arg = nd(s, s->file, arg)->next) {
    const ozni_node *an = nd(s, s->file, arg);
    if (an->kind == OZNI_N_Annotation) {
      continue;
    }
    bool named = an->kind == OZNI_N_NamedArg;
    ozni_index vnode = named ? an->first : arg;
    value v = vnode != 0 ? eval(s, vnode) : unknown();
    uint32_t k = np;
    if (named) {
      for (uint32_t j = 0; j < np; j++) {
        k = pnames[j] == an->text && !set[j] ? j : k;
      }
    } else {
      while (pos < np && set[pos]) {
        pos++;
      }
      k = pos;
    }
    if (nshown < 32) {
      shown_names[nshown] = named ? an->text : 0;
      shown[nshown++] = v;
    }
    if (k == np) {
      continue;
    }
    if (v.kind == V_FOOD) {
      ozni_index ptype = field_type_node(s, rfile, pnodes[k]);
      if (ptype != 0 && v.f->file != NONE && !satisfies(s, v.f, rfile, ptype)) {
        ozni_str args[3] = {S(s, rec->text), S(s, pnames[k]), src_text(s, s->file, nd(s, s->file, vnode))};
        diag_node(s, OZNI_D_TYP_stateRequired, s->file, nd(s, s->file, vnode), args, 3);
      }
      consume(s, &v, s->file, vnode);
      food *copy = food_copy(s, v.f);
      if (copy == nullptr) {
        return unknown();
      }
      v.f = copy;
    }
    vals[k] = v;
    set[k] = true;
  }
  saved_dish saved = save_dish(s);
  s->quiet++;
  s->make_depth++;
  s->file = rfile;
  s->mod = mod_of_file(s, rfile);
  s->scope = ozni_table_new(s->c->arena, sizeof(binding));
  s->portions = ozni_table_new(s->c->arena, sizeof(food *));
  s->reqs = ozni_table_new(s->c->arena, sizeof(ozni_index));
  s->variant_acts = ozni_table_new(s->c->arena, sizeof(act_ref));
  s->last = unknown();
  s->last_node = 0;
  s->group = s->branch = 0;
  s->servings = (ozni_qty){0};
  s->entry_dish = false;
  value last = unknown();
  if (s->scope != nullptr && s->portions != nullptr && s->reqs != nullptr && s->variant_acts != nullptr) {
    for (uint32_t k = 0; k < np; k++) {
      if (set[k]) {
        bind(s, pnames[k], vals[k]);
      }
    }
    for (ozni_index c = rec->first; c != 0; c = nd(s, rfile, c)->next) {
      if (nd(s, rfile, c)->kind == OZNI_N_Param) {
        eval_param(s, rfile, c);
      } else if (nd(s, rfile, c)->kind == OZNI_N_Block) {
        eval_block(s, c, true);
      }
    }
    last = s->last;
  } else {
    s->c->oom = true;
  }
  s->quiet--;
  s->make_depth--;
  restore_dish(s, &saved);
  ozni_index output = 0;
  for (ozni_index c = rec->first; c != 0; c = nd(s, rfile, c)->next) {
    if (nd(s, rfile, c)->kind == OZNI_N_Output) {
      output = nd(s, rfile, c)->first;
    }
  }
  ozni_index action = prefixed(s, "make ", rec->text);
  if (last.kind == V_TUPLE && last.items != nullptr) {
    food *outs[9];
    uint32_t n = 0;
    for (ozni_index k = 0; k < ozni_table_count(last.items) && n < 9; k++) {
      const value *item = ozni_table_get(last.items, k);
      food *o = item->kind == V_FOOD ? food_copy(s, item->f) : nullptr;
      if (o == nullptr) {
        return unknown();
      }
      o->portion = true;
      if (!ozni_table_push(s->portions, &o, nullptr)) {
        s->c->oom = true;
      }
      outs[n++] = o;
    }
    ozni_index ref = record_step(s, action, shown, nshown, shown_names, nullptr, outs, n);
    return make_tuple(s, ref, outs, n);
  }
  if (last.kind != V_FOOD) {
    record_step(s, action, shown, nshown, shown_names, nullptr, nullptr, 0);
    return unknown();
  }
  food *o = food_copy(s, last.f);
  if (o == nullptr) {
    return unknown();
  }
  o->type = output != 0 && nd(s, rfile, output)->kind == OZNI_N_TypeRef ? nd(s, rfile, output)->text
                                                                        : capitalized(s, rec->text);
  value out = {.kind = V_FOOD, .f = o};
  out.ref = record_step(s, action, shown, nshown, shown_names, o, nullptr, 0);
  return out;
}

static value eval_buy(sema *s, const ozni_node *n) {
  ozni_index tnode = n->first, qnode = tnode != 0 ? nd(s, s->file, tnode)->next : 0;
  food *f = tnode != 0 ? food_typed(s, s->file, tnode, true, 0) : nullptr;
  if (f == nullptr) {
    return unknown();
  }
  if (qnode != 0) {
    value q = eval(s, qnode);
    if (q.kind == V_QTY && q.q.known) {
      f->amount = q.q;
    }
  }
  s->step_no++;
  value out = {.kind = V_FOOD, .f = f, .ref = step_ref(s)};
  if (printing(s)) {
    ozni_buf *b = s->out;
    ozni_buf_puts(b, "  ");
    ozni_str r = S(s, out.ref);
    ozni_buf_put(b, r.p, r.len);
    ozni_buf_puts(b, " = buy ");
    ozni_str ty = src_text(s, s->file, nd(s, s->file, tnode));
    ozni_buf_put(b, ty.p, ty.len);
    if (qnode != 0) {
      ozni_str q = src_text(s, s->file, nd(s, s->file, qnode));
      ozni_buf_puts(b, " ");
      ozni_buf_put(b, q.p, q.len);
    }
    ozni_buf_puts(b, " : ");
    put_food_line(s, b, f);
    ozni_buf_puts(b, "\n");
  }
  return out;
}

static bool picks_type(sema *s, ozni_index file, ozni_index person, ozni_index type) {
  for (ozni_index e = nd(s, file, person)->first; e != 0; e = nd(s, file, e)->next) {
    const ozni_node *en = nd(s, file, e);
    if (en->kind != OZNI_N_Entry || !str_is(S(s, en->text), "picks") || en->first == 0) {
      continue;
    }
    for (ozni_index v = nd(s, file, en->first)->first; v != 0; v = nd(s, file, v)->next) {
      if (nd(s, file, v)->kind == OZNI_N_Name && nd(s, file, v)->text == type) {
        return true;
      }
    }
  }
  return false;
}

static value eval_choose(sema *s, const ozni_node *n) {
  value opts[16];
  uint32_t k = 0, counts[16] = {0}, total = 0;
  for (ozni_index c = n->first; c != 0 && k < 16; c = nd(s, s->file, c)->next) {
    if (nd(s, s->file, c)->kind != OZNI_N_Annotation) {
      opts[k++] = eval(s, c);
    }
  }
  if (k == 0) {
    return unknown();
  }
  ozni_index people = s->onode != 0 ? options_value(s, intern(s, "people", 6)) : 0;
  if (people != 0 && nd(s, s->ofile, people)->kind == OZNI_N_List && nd(s, s->ofile, people)->first != 0) {
    for (ozni_index p = nd(s, s->ofile, people)->first; p != 0; p = nd(s, s->ofile, p)->next) {
      uint32_t pick = 0;
      for (uint32_t j = k; j > 0; j--) {
        if (opts[j - 1].kind == V_FOOD && picks_type(s, s->ofile, p, opts[j - 1].f->type)) {
          pick = j - 1;
        }
      }
      counts[pick]++;
      total++;
    }
  } else {
    value pv;
    if (lookup_local(s, intern(s, "people", 6), &pv) && pv.kind == V_QTY && pv.q.known && pv.q.v.lo.den == 1 &&
        pv.q.v.lo.num > 0 && pv.q.v.lo.num < 100000) {
      total = (uint32_t)pv.q.v.lo.num;
    } else {
      total = 1;
    }
    for (uint32_t j = 0; j < k; j++) {
      counts[j] = total / k + (j < total % k ? 1 : 0);
    }
    if (s->entry_dish) {
      char num[24];
      size_t len = 0;
      char rev[24];
      uint32_t v = total;
      do {
        rev[len++] = (char)('0' + v % 10);
        v /= 10;
      } while (v != 0);
      for (size_t i = 0; i < len; i++) {
        num[i] = rev[len - 1 - i];
      }
      ozni_str args[2] = {S(s, n->text), {num, (uint32_t)len}};
      diag_node(s, OZNI_D_ASM_evenChoice, s->file, n, args, 2);
    }
  }
  s->step_no++;
  ozni_index ref = step_ref(s);
  if (printing(s)) {
    ozni_buf *b = s->out;
    ozni_buf_puts(b, "  ");
    ozni_str r = S(s, ref);
    ozni_buf_put(b, r.p, r.len);
    ozni_buf_puts(b, " = choose ");
    ozni_str name = S(s, n->text);
    ozni_buf_put(b, name.p, name.len);
    ozni_buf_puts(b, " { ");
    for (uint32_t j = 0; j < k; j++) {
      ozni_buf_puts(b, j == 0 ? "" : ", ");
      ozni_str o = S(s, opts[j].ref);
      ozni_buf_put(b, o.p, o.len);
      ozni_buf_puts(b, ": ");
      ozni_buf_putu(b, counts[j]);
    }
    ozni_buf_puts(b, " }\n");
  }
  return unknown();
}

static void level_assumptions(sema *s, ozni_index file, const ozni_node *dish) {
  uint32_t start, end;
  name_span(dish, S(s, dish->text), &start, &end);
  if (s->level < 2) {
    diag(s, OZNI_D_ASM_unlimitedEquipment, file, start, end, nullptr, 0);
  }
  for (ozni_index c = dish->first; c != 0; c = nd(s, file, c)->next) {
    const ozni_node *cn = nd(s, file, c);
    ozni_index def = cn->kind == OZNI_N_Param && str_is(S(s, cn->text), "servings") ? field_default(s, file, c) : 0;
    if (def != 0 && (s->onode == 0 || options_value(s, cn->text) == 0)) {
      ozni_str arg = src_text(s, file, nd(s, file, def));
      diag(s, OZNI_D_ASM_defaultServings, file, start, end, &arg, 1);
    }
  }
}

static void eval_dish(sema *s, ozni_index mi, const ozni_def *d) {
  const ozni_module *m = mod_at(s, mi);
  const ozni_node *n = nd(s, m->file, d->node);
  s->mod = mi;
  s->file = m->file;
  s->step_no = 0;
  s->entry_dish = s->c->set.entry != nullptr && str_is(S(s, n->text), s->c->set.entry);
  s->scope = ozni_table_new(s->c->arena, sizeof(binding));
  if (s->scope == nullptr) {
    s->c->oom = true;
    return;
  }
  if ((s->steps || s->requires) && s->out != nullptr) {
    ozni_buf_puts(s->out, n->kind == OZNI_N_Menu ? "menu " : "recipe ");
    ozni_str name = S(s, n->text);
    ozni_buf_put(s->out, name.p, name.len);
    ozni_buf_puts(s->out, "\n");
  }
  s->portions = ozni_table_new(s->c->arena, sizeof(food *));
  s->reqs = ozni_table_new(s->c->arena, sizeof(ozni_index));
  s->variant_acts = ozni_table_new(s->c->arena, sizeof(act_ref));
  s->last = unknown();
  s->last_node = 0;
  if (s->portions == nullptr || s->reqs == nullptr || s->variant_acts == nullptr) {
    s->c->oom = true;
    return;
  }
  s->servings = (ozni_qty){0};
  for (ozni_index c = n->first; c != 0; c = nd(s, m->file, c)->next) {
    const ozni_node *cn = nd(s, m->file, c);
    if (cn->kind == OZNI_N_Requires) {
      push_uses(s, m->file, c);
      for (ozni_index r = cn->first; s->knode != 0 && r != 0; r = nd(s, m->file, r)->next) {
        if (nd(s, m->file, r)->kind == OZNI_N_TypeRef && !req_met(s, m->file, r, nullptr, nullptr, 0)) {
          ozni_str arg = src_text(s, m->file, nd(s, m->file, r));
          diag_node(s, OZNI_D_KIT_requiresUnmet, m->file, nd(s, m->file, r), &arg, 1);
        }
      }
    } else if (cn->kind == OZNI_N_Param) {
      eval_param(s, m->file, c);
    }
  }
  for (ozni_index c = n->first; c != 0; c = nd(s, m->file, c)->next) {
    if (nd(s, m->file, c)->kind == OZNI_N_Block) {
      eval_block(s, c, true);
    }
  }
  if (s->requires && s->out != nullptr) {
    put_requirements(s);
  }
  if (s->entry_dish && n->kind == OZNI_N_Recipe) {
    wrap_entry(s, m->file, n);
  }
  if (s->entry_dish) {
    level_assumptions(s, m->file, n);
  }
  finish_dish(s);
  s->entry_dish = false;
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

static void check_uses_time(sema *s, ozni_index file, ozni_index node) {
  int time = dim_named(s, "Time");
  for (ozni_index c = nd(s, file, node)->first; c != 0; c = nd(s, file, c)->next) {
    const ozni_node *cn = nd(s, file, c);
    if (cn->kind == OZNI_N_Variant) {
      check_uses_time(s, file, c);
    }
    if (cn->kind != OZNI_N_Uses || time < 0) {
      continue;
    }
    for (ozni_index u = cn->first; u != 0; u = nd(s, file, u)->next) {
      const ozni_node *un = nd(s, file, u);
      ozni_qty q;
      if (un->kind != OZNI_N_For || un->first == 0 || nd(s, file, un->first)->kind != OZNI_N_Quantity ||
          !qty_of(s, mod_of_file(s, file), file, un->first, &q, true)) {
        continue;
      }
      ozni_dimvec want = {{0}};
      want.e[time] = 1;
      if (!dims_equal(q.d, want)) {
        ozni_str args[2] = {S(s, dim_name_text(s, q.d)), S(s, dim_name_text(s, want))};
        diag_node(s, OZNI_D_TYP_dimension, file, nd(s, file, un->first), args, 2);
      }
    }
  }
}

static void check_part_names(sema *s, ozni_index file, ozni_index decl) {
  ozni_index ps = child_section(s, file, decl, "parts");
  for (ozni_index p = ps != 0 ? nd(s, file, ps)->first : 0; p != 0; p = nd(s, file, p)->next) {
    const ozni_node *pn = nd(s, file, p);
    ozni_index tf, tn;
    if (is_part_node(pn) && find_from(s, file, pn->text, &tf, &tn) && nd(s, tf, tn)->kind == OZNI_N_Trait) {
      ozni_str arg = S(s, pn->text);
      uint32_t end = pn->name_end > pn->name_start ? pn->name_end : pn->span.end;
      uint32_t start = pn->name_end > pn->name_start ? pn->name_start : pn->span.start;
      diag(s, OZNI_D_NAM_partTraitClash, file, start, end, &arg, 1);
    }
  }
}

static bool body_binds(const sema *s, ozni_index file, ozni_index node, ozni_index name) {
  const ozni_node *n = nd(s, file, node);
  if ((n->kind == OZNI_N_Let || n->kind == OZNI_N_BindName || n->kind == OZNI_N_ForIn) && n->text == name) {
    return true;
  }
  for (ozni_index c = n->first; c != 0; c = nd(s, file, c)->next) {
    if (body_binds(s, file, c, name)) {
      return true;
    }
  }
  return false;
}

static bool is_value_name(const sema *s, ozni_index name) {
  for (ozni_index v = 0; v < ozni_table_count(s->pr->values); v++) {
    if (*(const ozni_index *)ozni_table_get(s->pr->values, v) == name) {
      return true;
    }
  }
  return false;
}

static void check_macro_names(sema *s, ozni_index file, ozni_index body, ozni_index node) {
  const ozni_node *n = nd(s, file, node);
  if (n->kind == OZNI_N_Splice || n->kind == OZNI_N_TypeRef || n->kind == OZNI_N_Annotation) {
    return;
  }
  if (n->kind == OZNI_N_Name) {
    static const char *const builtins[] = {"ceil", "floor", "min", "max", "len", "split", "volume", "kitchen"};
    bool ok = body_binds(s, file, body, n->text) || is_value_name(s, n->text);
    for (size_t k = 0; !ok && k < sizeof builtins / sizeof builtins[0]; k++) {
      ok = str_is(S(s, n->text), builtins[k]);
    }
    ozni_index df, dn;
    if (!ok && !find_def(s, n->text, &df, &dn)) {
      ozni_str arg = S(s, n->text);
      diag_node(s, OZNI_D_NAM_undefinedInMacro, file, n, &arg, 1);
    }
    return;
  }
  if (n->kind == OZNI_N_Member && n->first != 0) {
    check_macro_names(s, file, body, n->first);
    return;
  }
  for (ozni_index c = n->first; c != 0; c = nd(s, file, c)->next) {
    check_macro_names(s, file, body, c);
  }
}

static void check_codes(sema *s) {
  ozni_table *seen = ozni_table_new(s->c->arena, sizeof(ozni_index));
  if (seen == nullptr) {
    s->c->oom = true;
    return;
  }
  for (ozni_index mi = 0; mi < ozni_table_count(s->pr->modules); mi++) {
    const ozni_module *m = mod_at(s, mi);
    if (m->is_std) {
      continue;
    }
    for (ozni_index d = nd(s, m->file, m->root)->first; d != 0; d = nd(s, m->file, d)->next) {
      if (nd(s, m->file, d)->kind != OZNI_N_Type) {
        continue;
      }
      for (ozni_index c = nd(s, m->file, d)->first; c != 0; c = nd(s, m->file, c)->next) {
        const ozni_node *cn = nd(s, m->file, c);
        if (cn->kind != OZNI_N_Check || cn->text2 == 0) {
          continue;
        }
        ozni_str code = S(s, cn->text2);
        bool shape = code.len == 6;
        for (uint32_t k = 0; shape && k < 6; k++) {
          shape = k < 3 ? code.p[k] >= 'A' && code.p[k] <= 'Z' : code.p[k] >= '0' && code.p[k] <= '9';
        }
        bool duplicate = false;
        ozni_diag_id id;
        for (ozni_index k = 0; k < ozni_table_count(seen); k++) {
          duplicate = duplicate || *(const ozni_index *)ozni_table_get(seen, k) == cn->text2;
        }
        if (duplicate || ozni_diag_lookup(code.p, code.len, &id)) {
          diag_node(s, OZNI_D_PKG_checkCodeDuplicate, m->file, cn, &code, 1);
        } else if (!shape || code.p[3] != '9') {
          diag_node(s, OZNI_D_PKG_checkCodeRange, m->file, cn, &code, 1);
        }
        if (!duplicate && !ozni_table_push(seen, &cn->text2, nullptr)) {
          s->c->oom = true;
        }
      }
    }
  }
}

static void check_definitions(sema *s) {
  check_codes(s);
  for (ozni_index mi = 0; mi < ozni_table_count(s->pr->modules); mi++) {
    const ozni_module *m = mod_at(s, mi);
    if (m->is_std) {
      continue;
    }
    s->mod = mi;
    for (ozni_index c = nd(s, m->file, m->root)->first; c != 0; c = nd(s, m->file, c)->next) {
      ozni_node_kind k = nd(s, m->file, c)->kind;
      if (k == OZNI_N_Action || k == OZNI_N_Extend) {
        check_uses_time(s, m->file, c);
      } else if (k == OZNI_N_Ingredient || k == OZNI_N_Trait) {
        check_part_names(s, m->file, c);
      } else if (k == OZNI_N_Macro) {
        check_macro_names(s, m->file, c, c);
      }
    }
  }
}

bool ozni_sema_run(ozni_project *pr, bool steps, bool requires, ozni_buf *out) {
  sema s = {.pr = pr, .c = pr->c, .out = out, .steps = steps, .requires = requires};
  s.dims = ozni_table_new(pr->c->arena, sizeof(ozni_index));
  s.units = ozni_table_new(pr->c->arena, sizeof(unit_info));
  if (s.dims == nullptr || s.units == nullptr) {
    return false;
  }
  build_units(&s);
  check_axes(&s);
  check_definitions(&s);
  setup_configs(&s);
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
