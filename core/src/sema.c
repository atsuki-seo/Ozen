/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#include <string.h>

#include "sched.h"
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
  ozni_qty size;
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
  ozni_index irn;
  uint32_t irpart;
  ozni_index irdiv;
  ozni_index in_label;
  const ozni_table *allergens;
  ozni_index made_file;
  ozni_index made_node;
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
  bool ir_print;
  bool schedule;
  ozni_ir *ir_keep;
  ozni_ir ir;
  ozni_index ir_base;
  uint32_t wave_no;
  ozni_table *wave_names;
  uint32_t ir_count;
  ozni_table *join;
  bool in_branch;
  ozni_index branch_last;
  ozni_index ir_call_file;
  ozni_index ir_call_node;
  ozni_table *wrules;
  ozni_table *wmarks;
  ozni_table *wnodes;
  ozni_table *picks;
  ozni_table *allergens;
  ozni_table *small_seen;
  ozni_index dish_file;
  ozni_index dish_node;
  ozni_table *plate_infos;
  ozni_table *plate_units;
  ozni_table *wave_infos;
  ozni_table *vessel_used;
  ozni_table *ir_meta;
  ozni_table *ir_args;
  ozni_table *choices;
  ozni_table *choice_opts;
  ozni_table *assemble_infos;
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
    *st = (axis_state){.axis = an->text, .file = NONE, .kind = AX_TAGGED};
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
  f->irn = 0;
  f->irpart = 0;
  f->irdiv = 0;
  f->in_label = 0;
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

/* IR */

static ozni_str src_text(const sema *s, ozni_index file, const ozni_node *n);
typedef struct value value;
static value eval_in(sema *s, ozni_index file, ozni_index node, const value *params, const ozni_index *pnames,
                     uint32_t np);

static bool ir_on(const sema *s) { return s->ir.nodes != nullptr; }

static ozni_index ir_add(sema *s, ozni_ir_node node) {
  ozni_index at;
  if (node.kind != OZNI_IR_WAVE) {
    node.number = ++s->ir_count;
  }
  if (!ozni_table_push(s->ir.nodes, &node, &at)) {
    s->c->oom = true;
    return 0;
  }
  if (s->in_branch && (node.kind == OZNI_IR_OP || node.kind == OZNI_IR_CHOOSE || node.kind == OZNI_IR_PLATE)) {
    s->branch_last = at + 1;
  }
  return at + 1;
}

static void ir_afters(sema *s, ozni_buf *b, ozni_ir_node *node) {
  node->first_after = ozni_table_count(s->ir.afters);
  ozni_index list[64];
  uint32_t n = 0;
  for (ozni_index k = 0; s->join != nullptr && k < ozni_table_count(s->join) && n < 64; k++) {
    list[n++] = *(const ozni_index *)ozni_table_get(s->join, k);
  }
  if (s->in_branch && s->branch_last != 0 && n < 64) {
    list[n++] = s->branch_last;
  }
  for (uint32_t k = 0; k < n; k++) {
    const ozni_ir_node *dep = ozni_table_get(s->ir.nodes, list[k] - 1);
    ozni_buf_puts(b, k == 0 ? " after %" : ", %");
    ozni_buf_putu(b, dep != nullptr ? dep->number : 0);
    if (!ozni_table_push(s->ir.afters, &list[k], nullptr)) {
      s->c->oom = true;
    }
  }
  node->nafter = n;
}

static ozni_index ir_text(sema *s, ozni_buf *b) {
  ozni_index id = 0;
  if (b->failed) {
    s->c->oom = true;
  } else {
    id = intern(s, b->p != nullptr ? b->p : "", b->len);
  }
  ozni_buf_discard(b);
  return id;
}

typedef struct ir_meta {
  ozni_index irn;
  ozni_qty qty;
  uint32_t first_arg;
  uint32_t nargs;
  bool has_qty;
  uint32_t nallergens;
  ozni_index allergens[8];
} ir_meta;

typedef struct choice {
  ozni_index at;
  ozni_index mode;
  uint32_t first_opt;
  uint32_t nopts;
} choice;

typedef struct choice_opt {
  ozni_index irn;
  ozni_index label;
  uint32_t count;
} choice_opt;

static uint32_t food_allergens(sema *s, const food *f, ozni_index *out, uint32_t cap);

static bool name_after(ozni_str a, ozni_str b) {
  uint32_t n = a.len < b.len ? a.len : b.len;
  int c = n > 0 ? memcmp(a.p, b.p, n) : 0;
  return c > 0 || (c == 0 && a.len > b.len);
}

typedef struct ir_arg {
  ozni_index name;
  ozni_index value;
} ir_arg;

static void put_value(sema *s, ozni_buf *b, const value *v);

static void meta_push(sema *s, ozni_index irn, const food *out, const value *params, const ozni_index *pnames,
                      uint32_t np) {
  if (s->ir_meta == nullptr || irn == 0) {
    return;
  }
  ir_meta m = {.irn = irn, .first_arg = ozni_table_count(s->ir_args)};
  if (out != nullptr && out->amount.known) {
    m.qty = out->amount;
    m.has_qty = true;
  }
  m.nallergens = out != nullptr ? food_allergens(s, out, m.allergens, 8) : 0;
  for (uint32_t i = 1; i < m.nallergens; i++) {
    for (uint32_t j = i; j > 0 && name_after(S(s, m.allergens[j - 1]), S(s, m.allergens[j])); j--) {
      ozni_index t = m.allergens[j];
      m.allergens[j] = m.allergens[j - 1];
      m.allergens[j - 1] = t;
    }
  }
  for (uint32_t k = 0; k < np; k++) {
    if (params[k].kind != V_QTY && params[k].kind != V_ENUM && params[k].kind != V_STR && params[k].kind != V_BOOL) {
      continue;
    }
    ozni_buf b = {.a = s->c->allocator};
    put_value(s, &b, &params[k]);
    ir_arg a = {pnames[k], b.failed ? 0 : intern(s, b.p != nullptr ? b.p : "", b.len)};
    ozni_buf_discard(&b);
    if (!ozni_table_push(s->ir_args, &a, nullptr)) {
      s->c->oom = true;
    }
    m.nargs++;
  }
  if (!ozni_table_push(s->ir_meta, &m, nullptr)) {
    s->c->oom = true;
  }
}

static ozni_index ir_source(sema *s, food *f) {
  if (!ir_on(s) || f == nullptr || f->irn != 0) {
    return f != nullptr ? f->irn : 0;
  }
  ozni_buf b = {.a = s->c->allocator};
  ozni_buf_puts(&b, " : ");
  put_food_line(s, &b, f);
  ozni_ir_node node = {.kind = OZNI_IR_IN, .label = f->in_label != 0 ? f->in_label : f->type, .text = ir_text(s, &b)};
  f->irn = ir_add(s, node);
  meta_push(s, f->irn, f, nullptr, nullptr, 0);
  return f->irn;
}

static void ir_put_ref(sema *s, ozni_buf *b, food *f) {
  ozni_index n = ir_source(s, f);
  const ozni_ir_node *src = n != 0 ? ozni_table_get(s->ir.nodes, n - 1) : nullptr;
  ozni_buf_puts(b, "%");
  ozni_buf_putu(b, src != nullptr ? src->number : 0);
  if (f->irpart != 0) {
    ozni_buf_puts(b, ".");
    ozni_buf_putu(b, f->irpart);
  }
  if (f->irdiv != 0) {
    ozni_str d = S(s, f->irdiv);
    ozni_buf_puts(b, "/");
    ozni_buf_put(b, d.p, d.len);
  }
  ozni_ir_input in = {n, f->irpart, f->irdiv};
  if (!ozni_table_push(s->ir.inputs, &in, nullptr)) {
    s->c->oom = true;
  }
}

static void ir_put_uses(sema *s, ozni_buf *b, ozni_index file, ozni_index holder, const value *params,
                        const ozni_index *pnames, uint32_t np, ozni_rat *lo, ozni_rat *hi, bool *unattended,
                        uint32_t *nreqs, bool *provisional) {
  bool first = true, timed = false;
  *lo = *hi = ozni_rat_int(0);
  for (ozni_index c = nd(s, file, holder)->first; c != 0; c = nd(s, file, c)->next) {
    const ozni_node *cn = nd(s, file, c);
    if (cn->kind == OZNI_N_Annotation && str_is(S(s, cn->text), "unattended")) {
      *unattended = true;
    }
    if (cn->kind != OZNI_N_Uses) {
      continue;
    }
    for (ozni_index u = cn->first; u != 0; u = nd(s, file, u)->next) {
      const ozni_node *un = nd(s, file, u);
      if (un->kind == OZNI_N_TypeRef) {
        ozni_str text = src_text(s, file, un);
        ozni_buf_puts(b, first ? "" : ", ");
        ozni_buf_put(b, text.p, text.len);
        first = false;
        ozni_ir_req req = {intern(s, text.p, text.len), file, u};
        if (!ozni_table_push(s->ir.reqs, &req, nullptr)) {
          s->c->oom = true;
        }
        (*nreqs)++;
      } else if (un->kind == OZNI_N_For && un->first != 0) {
        ozni_qty q;
        if (qty_of(s, mod_of_file(s, file), file, un->first, &q, false)) {
          *lo = q.v.lo;
          *hi = q.v.hi;
          timed = true;
        }
      } else if (un->kind == OZNI_N_Annotation && str_is(S(s, un->text), "unattended")) {
        *unattended = true;
      } else if (un->kind == OZNI_N_Annotation && str_is(S(s, un->text), "source") && un->first != 0 &&
                 str_is(S(s, nd(s, file, un->first)->text), "provisional")) {
        *provisional = true;
      }
    }
  }
  for (ozni_index c = nd(s, file, holder)->first; !timed && c != 0; c = nd(s, file, c)->next) {
    if (nd(s, file, c)->kind == OZNI_N_Takes && nd(s, file, c)->first != 0) {
      value v = eval_in(s, file, nd(s, file, c)->first, params, pnames, np);
      if (v.kind == V_QTY && v.q.known) {
        *lo = v.q.v.lo;
        *hi = v.q.v.hi;
        timed = true;
      }
    }
  }
  for (uint32_t k = 0; !timed && k < np; k++) {
    if (str_is(S(s, pnames[k]), "time") && params[k].kind == V_QTY && params[k].q.known) {
      *lo = params[k].q.v.lo;
      *hi = params[k].q.v.hi;
    }
  }
  int tdim = dim_named(s, "Time");
  ozni_qty shown = {{*lo, *hi}, {{0}}, true};
  if (tdim >= 0) {
    shown.d.e[tdim] = 1;
  }
  ozni_buf_puts(b, first ? "" : "; ");
  put_qty(s, b, shown);
  if (*unattended) {
    ozni_buf_puts(b, "; unattended");
  }
}

static ozni_index ir_simple(sema *s, const char *label, food *const *ins, uint32_t nin, const char *argname,
                            const value *arg, const food *out, food *const *outs, uint32_t nouts) {
  if (!ir_on(s)) {
    return 0;
  }
  ozni_ir_node node = {.kind = OZNI_IR_OP, .label = intern(s, label, strlen(label)),
                       .first_input = ozni_table_count(s->ir.inputs), .first_req = ozni_table_count(s->ir.reqs),
                       .ninputs = nin, .time_lo = ozni_rat_int(0), .time_hi = ozni_rat_int(0)};
  ozni_buf b = {.a = s->c->allocator};
  for (uint32_t k = 0; k < nin; k++) {
    ozni_buf_puts(&b, k == 0 ? " " : ", ");
    ir_put_ref(s, &b, ins[k]);
  }
  if (argname != nullptr) {
    ozni_buf_puts(&b, " ");
    ozni_buf_puts(&b, argname);
    ozni_buf_puts(&b, "=");
    put_value(s, &b, arg);
  }
  ir_afters(s, &b, &node);
  ozni_buf_puts(&b, " :");
  for (uint32_t k = 0; k < nin; k++) {
    ozni_buf_puts(&b, k == 0 ? " " : ", ");
    put_food_type(s, &b, ins[k]);
  }
  if (out != nullptr) {
    ozni_buf_puts(&b, " -> ");
    put_food_line(s, &b, out);
  } else if (nouts > 0) {
    ozni_buf_puts(&b, " -> (");
    for (uint32_t k = 0; k < nouts; k++) {
      ozni_buf_puts(&b, k == 0 ? "" : ", ");
      put_food_line(s, &b, outs[k]);
    }
    ozni_buf_puts(&b, ")");
  }
  ozni_buf_puts(&b, " [0 s]");
  node.text = ir_text(s, &b);
  ozni_index irn = ir_add(s, node);
  meta_push(s, irn, out, nullptr, nullptr, 0);
  if (s->ir_meta != nullptr && irn != 0 && ozni_table_count(s->ir_meta) > 0) {
    ((ir_meta *)ozni_table_at(s->ir_meta, ozni_table_count(s->ir_meta) - 1))->has_qty = false;
  }
  return irn;
}

/* Evaluation */

static value eval(sema *s, ozni_index node);
static int axis_value_index(const sema *s, const axis_state *st, ozni_index name);
static bool food_has(sema *s, const food *f, ozni_index name);
static value eval_split(sema *s, const ozni_node *call);
static void eval_serve(sema *s, const ozni_node *serve);
static value eval_make(sema *s, const ozni_node *mk);
static value eval_buy(sema *s, const ozni_node *n);
static value eval_assemble(sema *s, const ozni_node *n);
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
  if (ir_on(s)) {
    food *ins[64];
    uint32_t nin = 0;
    for (uint32_t k = 0; k < n && nin < 64; k++) {
      if (items[k].kind == V_FOOD) {
        ins[nin++] = items[k].f;
      }
    }
    mix->irn = ir_simple(s, "combine", ins, nin, nullptr, nullptr, mix, nullptr, 0);
  }
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
      ozni_qty size = {0};
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
              if (params[k].kind == V_QTY && str_is(S(s, pnames[k]), "size")) {
                size = params[k].q;
              }
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
        st->size = size;
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
  if (nd(s, vfile, vnode)->kind == OZNI_N_Construct && find_from(s, tfile, bn->text, &df, &dn) &&
      nd(s, df, dn)->kind == OZNI_N_Type) {
    for (ozni_index c = nd(s, vfile, vnode)->first; c != 0; c = nd(s, vfile, c)->next) {
      const ozni_node *cn = nd(s, vfile, c);
      ozni_index ff, fn;
      if (cn->kind != OZNI_N_Entry) {
        continue;
      }
      if (!type_field(s, df, dn, cn->text, &ff, &fn, 0)) {
        ozni_str args[2] = {S(s, bn->text), S(s, cn->text)};
        diag_node(s, OZNI_D_NAM_unknownArgument, vfile, cn, args, 2);
      } else if (cn->first != 0 && field_type_node(s, ff, fn) != 0) {
        check_value(s, vfile, cn->first, ff, field_type_node(s, ff, fn), cn->text);
      }
    }
    return;
  }
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
      if (cn->kind == OZNI_N_Entry && !type_field(s, e.sfile, e.snode, cn->text, &ff, &fn, 0)) {
        ozni_str args[2] = {S(s, kind_name), S(s, cn->text)};
        diag_node(s, OZNI_D_NAM_unknownArgument, vfile, cn, args, 2);
      } else if (cn->kind == OZNI_N_Entry && cn->first != 0 && field_type_node(s, ff, fn) != 0) {
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
  ozni_index found = 0;
  bool merge = s->c->set.options != nullptr && s->c->set.options[0] == '<';
  for (ozni_index d = merge ? nd(s, s->ofile, ozni_project_module(s->pr, mod_of_file(s, s->ofile))->root)->first
                            : s->onode;
       s->onode != 0 && d != 0; d = merge ? nd(s, s->ofile, d)->next : 0) {
    for (ozni_index c = nd(s, s->ofile, d)->kind == OZNI_N_Kitchen ? nd(s, s->ofile, d)->first : 0; c != 0;
         c = nd(s, s->ofile, c)->next) {
      if (nd(s, s->ofile, c)->kind == OZNI_N_Field && nd(s, s->ofile, c)->text == id) {
        found = nd(s, s->ofile, c)->first;
      }
    }
  }
  return found;
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
  if (name[0] == '<') {
    ozni_index found = 0;
    for (ozni_index mi = 0; mi < ozni_table_count(s->pr->modules); mi++) {
      const ozni_module *m = mod_at(s, mi);
      if (m->is_std || ozni_strcmp(s->c->srcs[m->file].path, name) != 0) {
        continue;
      }
      for (ozni_index c = nd(s, m->file, m->root)->first; c != 0; c = nd(s, m->file, c)->next) {
        if (nd(s, m->file, c)->kind == OZNI_N_Kitchen && (found == 0 || name[1] == 'o')) {
          s->mod = mi;
          found = c;
        }
      }
    }
    if (found == 0) {
      ozni_str arg = {name, (uint32_t)strlen(name)};
      diag(s, OZNI_D_NAM_undefined, 0, 0, 0, &arg, 1);
    }
    return found;
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

static void options_levels(sema *s) {
  ozni_index strict = options_value(s, intern(s, "strict", 6));
  if (strict != 0 && nd(s, s->ofile, strict)->kind == OZNI_N_Name) {
    s->c->set.strict = str_is(S(s, nd(s, s->ofile, strict)->text), "on");
  }
  static const char *const fields[] = {"allow", "warn", "deny"};
  static const uint8_t kinds[] = {OZNI_LEVEL_ALLOW, OZNI_LEVEL_WARN, OZNI_LEVEL_DENY};
  uint32_t total = 0;
  ozni_index lists[3];
  for (uint32_t k = 0; k < 3; k++) {
    lists[k] = options_value(s, intern(s, fields[k], strlen(fields[k])));
    for (ozni_index v = lists[k] != 0 ? nd(s, s->ofile, lists[k])->first : 0; v != 0; v = nd(s, s->ofile, v)->next) {
      total++;
    }
  }
  if (total == 0) {
    return;
  }
  const char **names = ozni_arena_alloc(s->c->arena, (total + s->c->set.nlevels) * sizeof *names);
  uint8_t *levels = ozni_arena_alloc(s->c->arena, total + s->c->set.nlevels);
  if (names == nullptr || levels == nullptr) {
    s->c->oom = true;
    return;
  }
  size_t n = 0;
  for (size_t k = 0; k < s->c->set.nlevels; k++, n++) {
    names[n] = s->c->set.level_names[k];
    levels[n] = s->c->set.level_kinds[k];
  }
  for (uint32_t k = 0; k < 3; k++) {
    for (ozni_index v = lists[k] != 0 ? nd(s, s->ofile, lists[k])->first : 0; v != 0; v = nd(s, s->ofile, v)->next) {
      ozni_str text = nd(s, s->ofile, v)->kind == OZNI_N_String ? S(s, nd(s, s->ofile, v)->text) : S(s, 0);
      char *copy = ozni_arena_alloc(s->c->arena, text.len + 1);
      if (copy == nullptr) {
        s->c->oom = true;
        return;
      }
      ozni_copy(copy, text.p, text.len);
      copy[text.len] = '\0';
      names[n] = copy;
      levels[n++] = kinds[k];
    }
  }
  s->c->set.level_names = names;
  s->c->set.level_kinds = levels;
  s->c->set.nlevels = n;
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
  options_levels(s);
  bool merge = s->c->set.options != nullptr && s->c->set.options[0] == '<';
  ozni_index first_decl = merge ? nd(s, s->ofile, mod_at(s, mod_of_file(s, s->ofile))->root)->first : s->onode;
  for (ozni_index d = first_decl; d != 0; d = merge ? nd(s, s->ofile, d)->next : 0) {
  for (ozni_index c = nd(s, s->ofile, d)->kind == OZNI_N_Kitchen ? nd(s, s->ofile, d)->first : 0; c != 0;
       c = nd(s, s->ofile, c)->next) {
    const ozni_node *cn = nd(s, s->ofile, c);
    ozni_index ff, fn;
    if (cn->kind == OZNI_N_Field && !type_field(s, tf, tn, cn->text, &ff, &fn, 0)) {
      ozni_str args[2] = {S(s, nd(s, tf, tn)->text), S(s, cn->text)};
      diag_node(s, OZNI_D_NAM_unknownArgument, s->ofile, cn, args, 2);
    }
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

typedef struct wrule {
  ozni_index file;
  ozni_index node;
  ozni_index after;
  bool fixed;
} wrule;

typedef struct wmark {
  ozni_index irn;
  uint32_t variant;
  ozni_index dirt;
  ozni_index dfile;
  ozni_index dnode;
  ozni_index allergy;
  ozni_index cfile;
  ozni_index cnode;
  ozni_index food;
} wmark;

typedef struct wnode {
  ozni_index irn;
  ozni_index food;
  ozni_index ffile;
  ozni_index fnode;
  ozni_table *derived;
} wnode;

typedef struct pick_range {
  ozni_index first;
  ozni_index end;
  uint64_t people;
} pick_range;

static ozni_index allergen_kind(sema *s, ozni_str text) {
  size_t open = 0;
  while (open < text.len && text.p[open] != '(') {
    open++;
  }
  if (open + 2 > text.len || text.p[text.len - 1] != ')') {
    return 0;
  }
  return intern(s, text.p + open + 1, text.len - open - 2);
}

static uint32_t food_allergens(sema *s, const food *f, ozni_index *out, uint32_t cap) {
  uint32_t n = 0;
  for (ozni_index c = f->file != NONE ? nd(s, f->file, f->node)->first : 0; c != 0 && n < cap;
       c = nd(s, f->file, c)->next) {
    const ozni_node *cn = nd(s, f->file, c);
    if (cn->kind == OZNI_N_TypeRef && str_is(S(s, cn->text), "Allergen")) {
      ozni_index k = allergen_kind(s, src_text(s, f->file, cn));
      if (k != 0) {
        out[n++] = k;
      }
    }
  }
  for (ozni_index d = 0; f->allergens != nullptr && d < ozni_table_count(f->allergens) && n < cap; d++) {
    ozni_index k = *(const ozni_index *)ozni_table_get(f->allergens, d);
    bool dup = false;
    for (uint32_t i = 0; i < n; i++) {
      dup = dup || out[i] == k;
    }
    if (!dup) {
      out[n++] = k;
    }
  }
  for (ozni_index d = 0; f->derived != nullptr && d < ozni_table_count(f->derived) && n < cap; d++) {
    ozni_str text = S(s, *(const ozni_index *)ozni_table_get(f->derived, d));
    if (text.len > 9 && memcmp(text.p, "Allergen(", 9) == 0) {
      ozni_index k = allergen_kind(s, text);
      bool dup = false;
      for (uint32_t i = 0; i < n; i++) {
        dup = dup || out[i] == k;
      }
      if (k != 0 && !dup) {
        out[n++] = k;
      }
    }
  }
  return n;
}

static void collect_allergens(sema *s, const food *f) {
  ozni_index kinds[16];
  uint32_t n = s->allergens != nullptr ? food_allergens(s, f, kinds, 16) : 0;
  for (uint32_t k = 0; k < n; k++) {
    bool dup = false;
    for (ozni_index i = 0; i < ozni_table_count(s->allergens); i++) {
      dup = dup || *(const ozni_index *)ozni_table_get(s->allergens, i) == kinds[k];
    }
    if (!dup && !ozni_table_push(s->allergens, &kinds[k], nullptr)) {
      s->c->oom = true;
    }
  }
}

static bool food_raw(sema *s, const food *f) {
  ozni_index doneness = intern(s, "doneness", 8);
  for (uint32_t k = 0; k < f->naxes; k++) {
    if (f->axes[k].axis == doneness) {
      return f->axes[k].value == 0 || f->axes[k].value == initial_value(s, &f->axes[k]);
    }
  }
  return true;
}

static ozni_index food_key(sema *s, const food *f) {
  if (f == nullptr) {
    return 0;
  }
  ozni_index doneness = intern(s, "doneness", 8);
  ozni_str type = S(s, f->type), state = {"-", 1};
  for (uint32_t k = 0; k < f->naxes; k++) {
    if (f->axes[k].axis == doneness && f->axes[k].value != 0) {
      state = S(s, f->axes[k].value);
    }
  }
  char buf[256];
  if (type.len + state.len + 1 > sizeof buf) {
    return f->type;
  }
  ozni_copy(buf, type.p, type.len);
  buf[type.len] = '/';
  ozni_copy(buf + type.len + 1, state.p, state.len);
  return intern(s, buf, type.len + 1 + state.len);
}

static void push_mark(sema *s, wmark m) {
  if (!ozni_table_push(s->wmarks, &m, nullptr)) {
    s->c->oom = true;
  }
}

static void dirt_marks(sema *s, wmark base, ozni_index from_file, ozni_index name, const food *f) {
  ozni_index df, dn;
  if (!find_from(s, from_file, name, &df, &dn) || nd(s, df, dn)->kind != OZNI_N_Dirt) {
    return;
  }
  if (f != nullptr && !food_raw(s, f) && decl_has_trait(s, df, dn, intern(s, "Hazard", 6), 0)) {
    return;
  }
  base.dfile = df;
  base.dnode = dn;
  base.dirt = name;
  if (f == nullptr || !decl_has_trait(s, df, dn, intern(s, "Allergen", 8), 0)) {
    push_mark(s, base);
    return;
  }
  ozni_index kinds[16];
  uint32_t n = food_allergens(s, f, kinds, 16);
  ozni_str dname = S(s, name);
  for (uint32_t k = 0; k < n; k++) {
    ozni_str kn = S(s, kinds[k]);
    char buf[256];
    if (dname.len + kn.len + 2 > sizeof buf) {
      continue;
    }
    ozni_copy(buf, dname.p, dname.len);
    buf[dname.len] = '(';
    ozni_copy(buf + dname.len + 1, kn.p, kn.len);
    buf[dname.len + 1 + kn.len] = ')';
    base.dirt = intern(s, buf, dname.len + kn.len + 2);
    base.allergy = kinds[k];
    push_mark(s, base);
  }
}

static void leaves_marks(sema *s, ozni_index irn, uint32_t variant, ozni_index file, ozni_index holder,
                         const value *params, const ozni_index *pnames, uint32_t np) {
  ozni_index field = field_first(s, file, holder, "leaves");
  for (ozni_index item = field != 0 ? nd(s, file, field)->first : 0; item != 0; item = nd(s, file, item)->next) {
    const ozni_node *on = nd(s, file, item);
    if (on->kind != OZNI_N_On || on->first == 0 || nd(s, file, on->first)->next == 0) {
      continue;
    }
    ozni_index what = nd(s, file, on->first)->text;
    wmark base = {.irn = irn, .variant = variant, .cfile = file, .cnode = nd(s, file, on->first)->next};
    const food *f = nullptr;
    for (uint32_t k = 0; k < np; k++) {
      if (pnames[k] == what && params[k].kind == V_FOOD) {
        f = params[k].f;
      }
    }
    if (f == nullptr) {
      base.food = 0;
      for (uint32_t k = 0; k < np && base.food == 0; k++) {
        base.food = params[k].kind == V_FOOD ? food_key(s, params[k].f) : 0;
      }
      dirt_marks(s, base, file, what, nullptr);
      continue;
    }
    base.food = food_key(s, f);
    ozni_index own = f->file != NONE ? field_first(s, f->file, f->node, "leaves") : 0;
    for (ozni_index d = own != 0 ? nd(s, f->file, own)->first : 0; d != 0; d = nd(s, f->file, d)->next) {
      if (nd(s, f->file, d)->kind == OZNI_N_Name) {
        dirt_marks(s, base, f->file, nd(s, f->file, d)->text, f);
      }
    }
  }
}

static void wash_record(sema *s, ozni_index irn, ozni_index afile, ozni_index anode, const act_ref *vars,
                        uint32_t nv, const value *params, const ozni_index *pnames, uint32_t np) {
  if (s->wmarks == nullptr || s->wnodes == nullptr || irn == 0) {
    return;
  }
  const food *in = nullptr;
  for (uint32_t k = 0; k < np && in == nullptr; k++) {
    in = params[k].kind == V_FOOD ? params[k].f : nullptr;
  }
  wnode w = {irn, food_key(s, in), in != nullptr ? in->file : NONE, in != nullptr ? in->node : 0,
             in != nullptr ? in->derived : nullptr};
  if (!ozni_table_push(s->wnodes, &w, nullptr)) {
    s->c->oom = true;
  }
  if (nv == 0) {
    leaves_marks(s, irn, UINT32_MAX, afile, anode, params, pnames, np);
  }
  for (uint32_t k = 0; k < nv; k++) {
    leaves_marks(s, irn, k, vars[k].file, vars[k].node, params, pnames, np);
  }
}

static ozni_index ir_record_call(sema *s, const ozni_node *act, ozni_index afile, ozni_index anode,
                                 const value *params, const ozni_index *pnames, uint32_t np, const food *out,
                                 food *const *outs, uint32_t nouts) {
  if (!ir_on(s)) {
    return 0;
  }
  act_ref vars[32];
  uint32_t nv = variant_list(s, afile, anode, vars, 32);
  ozni_ir_node node = {.kind = nv > 0 ? OZNI_IR_CHOOSE : OZNI_IR_OP, .label = act->text,
                       .first_input = ozni_table_count(s->ir.inputs), .first_req = ozni_table_count(s->ir.reqs),
                       .first_variant = ozni_table_count(s->ir.variants), .nvariants = nv};
  ozni_buf b = {.a = s->c->allocator};
  for (uint32_t k = 0; k < np; k++) {
    if (params[k].kind == V_FOOD && params[k].f != nullptr) {
      ozni_buf_puts(&b, node.ninputs == 0 ? " " : ", ");
      ir_put_ref(s, &b, params[k].f);
      node.ninputs++;
    }
  }
  for (uint32_t k = 0; k < np; k++) {
    if (params[k].kind == V_QTY || params[k].kind == V_ENUM || params[k].kind == V_STR || params[k].kind == V_BOOL) {
      ozni_str nm = S(s, pnames[k]);
      ozni_buf_puts(&b, " ");
      ozni_buf_put(&b, nm.p, nm.len);
      ozni_buf_puts(&b, "=");
      put_value(s, &b, &params[k]);
    }
  }
  ir_afters(s, &b, &node);
  node.file = s->ir_call_file;
  node.src = s->ir_call_node;
  ozni_buf_puts(&b, " :");
  bool first = true;
  for (uint32_t k = 0; k < np; k++) {
    if (params[k].kind == V_FOOD && params[k].f != nullptr) {
      ozni_buf_puts(&b, first ? " " : ", ");
      put_food_type(s, &b, params[k].f);
      first = false;
    }
  }
  if (out != nullptr) {
    ozni_buf_puts(&b, " -> ");
    put_food_line(s, &b, out);
  } else if (nouts > 0) {
    ozni_buf_puts(&b, " -> (");
    for (uint32_t k = 0; k < nouts; k++) {
      ozni_buf_puts(&b, k == 0 ? "" : ", ");
      put_food_line(s, &b, outs[k]);
    }
    ozni_buf_puts(&b, ")");
  }
  if (nv == 0) {
    ozni_buf_puts(&b, " [");
    ir_put_uses(s, &b, afile, anode, params, pnames, np, &node.time_lo, &node.time_hi, &node.unattended,
                &node.nreqs, &node.provisional);
    ozni_buf_puts(&b, "]");
  } else {
    ozni_buf_puts(&b, " { ");
    for (uint32_t k = 0; k < nv; k++) {
      ozni_ir_variant v = {.name = nd(s, vars[k].file, vars[k].node)->text, .first_req = ozni_table_count(s->ir.reqs)};
      ozni_str vn = S(s, v.name);
      ozni_buf_puts(&b, k == 0 ? "" : " | ");
      ozni_buf_put(&b, vn.p, vn.len);
      ozni_buf_puts(&b, " [");
      ir_put_uses(s, &b, vars[k].file, vars[k].node, params, pnames, np, &v.time_lo, &v.time_hi, &v.unattended,
                  &v.nreqs, &v.provisional);
      ozni_buf_puts(&b, "]");
      if (!ozni_table_push(s->ir.variants, &v, nullptr)) {
        s->c->oom = true;
      }
    }
    ozni_buf_puts(&b, " }");
  }
  node.text = ir_text(s, &b);
  ozni_index at = ir_add(s, node);
  wash_record(s, at, afile, anode, vars, nv, params, pnames, np);
  meta_push(s, at, out, params, pnames, np);
  return at;
}

static void name_span(const ozni_node *n, ozni_str name, uint32_t *start, uint32_t *end);

static bool small_tool(sema *s, ozni_index name) {
  ozni_index pf, pn;
  if (!find_def(s, intern(s, "smallTools", 10), &pf, &pn) || nd(s, pf, pn)->first == 0 ||
      nd(s, pf, nd(s, pf, pn)->first)->kind != OZNI_N_List) {
    return false;
  }
  for (ozni_index v = nd(s, pf, nd(s, pf, pn)->first)->first; v != 0; v = nd(s, pf, v)->next) {
    if (nd(s, pf, v)->text == name) {
      return true;
    }
  }
  return false;
}

static void assume_small(sema *s, ozni_index name) {
  if (s->small_seen == nullptr || s->dish_node == 0) {
    return;
  }
  for (ozni_index k = 0; k < ozni_table_count(s->small_seen); k++) {
    if (*(const ozni_index *)ozni_table_get(s->small_seen, k) == name) {
      return;
    }
  }
  if (!ozni_table_push(s->small_seen, &name, nullptr)) {
    s->c->oom = true;
    return;
  }
  const ozni_node *dish = nd(s, s->dish_file, s->dish_node);
  uint32_t start, end;
  name_span(dish, S(s, dish->text), &start, &end);
  ozni_str arg = S(s, name);
  diag(s, OZNI_D_ASM_smallToolsAssumed, s->dish_file, start, end, &arg, 1);
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
          if (small_tool(s, nd(s, afile, r)->text)) {
            assume_small(s, nd(s, afile, r)->text);
          } else {
            diag_node(s, OZNI_D_KIT_requiresUnmet, file, call, &arg, 1);
          }
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

static value butcher(sema *s, const ozni_node *act, ozni_index afile, ozni_index anode, ozni_index output,
                     const value *params, const ozni_index *pnames, const ozni_index *pnodes, uint32_t np,
                     const value *shown, uint32_t nshown, const ozni_index *shown_names) {
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
  ozni_index irn = ir_record_call(s, act, afile, anode, params, pnames, np, nullptr, outs, n);
  for (uint32_t k = 0; k < n; k++) {
    outs[k]->irn = irn;
    outs[k]->irpart = k + 1;
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

static ozni_index hardness_of(sema *s, const food *f) {
  ozni_index field = f->file != NONE ? section_field(s, f->file, f->node, "props", "hardness") : 0;
  return field != 0 && nd(s, f->file, field)->first != 0 ? nd(s, f->file, nd(s, f->file, field)->first)->text : 0;
}

static bool heated(sema *s, const food *f) {
  for (uint32_t k = 0; k < f->naxes; k++) {
    if (str_is(S(s, f->axes[k].axis), "doneness")) {
      return axis_value_index(s, &f->axes[k], f->axes[k].value) > 0;
    }
  }
  return false;
}

static void soft_before_hard(sema *s, const value *items, uint32_t n, ozni_index file, const ozni_node *call) {
  for (uint32_t a = 0; a < n; a++) {
    if (items[a].kind != V_FOOD || !heated(s, items[a].f) || !str_is(S(s, hardness_of(s, items[a].f)), "soft")) {
      continue;
    }
    for (uint32_t b = 0; b < n; b++) {
      if (items[b].kind == V_FOOD && !heated(s, items[b].f) && str_is(S(s, hardness_of(s, items[b].f)), "hard")) {
        ozni_str args[2] = {S(s, items[a].f->type), S(s, items[b].f->type)};
        diag_node(s, OZNI_D_SCH_softBeforeHard, file, call, args, 2);
        return;
      }
    }
  }
}

static value call_composite(sema *s, ozni_index file, const ozni_node *call, ozni_index afile, ozni_index anode,
                            ozni_index block, const value *params, const ozni_index *pnames, uint32_t np) {
  ozni_index saved_file = s->file, saved_mod = s->mod, saved_node = s->last_node;
  ozni_table *saved_scope = s->scope;
  value saved_last = s->last;
  bool saved_top = s->top;
  s->scope = ozni_table_new(s->c->arena, sizeof(binding));
  if (s->scope == nullptr) {
    s->c->oom = true;
    s->scope = saved_scope;
    return unknown();
  }
  for (uint32_t k = 0; k < np; k++) {
    bind(s, pnames[k], params[k]);
  }
  s->file = afile;
  s->mod = mod_of_file(s, afile);
  s->last = unknown();
  eval_block(s, block, true);
  value result = s->last;
  s->file = saved_file;
  s->mod = saved_mod;
  s->scope = saved_scope;
  s->last = saved_last;
  s->last_node = saved_node;
  s->top = saved_top;
  ozni_index output = 0;
  for (ozni_index c = nd(s, afile, anode)->first; c != 0; c = nd(s, afile, c)->next) {
    if (nd(s, afile, c)->kind == OZNI_N_Output) {
      output = nd(s, afile, c)->first;
    }
  }
  if (result.kind == V_FOOD && output != 0 && !satisfies(s, result.f, afile, output)) {
    ozni_buf b = {.a = s->c->allocator};
    put_food_type(s, &b, result.f);
    if (b.failed) {
      s->c->oom = true;
    } else {
      ozni_str args[2] = {S(s, nd(s, afile, anode)->text), {b.p, (uint32_t)b.len}};
      diag_node(s, OZNI_D_TYP_outputMismatch, file, call, args, 2);
    }
    ozni_buf_discard(&b);
  }
  return result;
}

static value call_action(sema *s, ozni_index file, ozni_index callnode, ozni_index afile, ozni_index anode) {
  const ozni_node *call = nd(s, file, callnode);
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
    if (params[k].kind == V_LIST && params[k].items != nullptr) {
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
        soft_before_hard(s, items, n < 64 ? n : 64, file, call);
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
  for (ozni_index c = act->first; c != 0; c = nd(s, afile, c)->next) {
    if (nd(s, afile, c)->kind == OZNI_N_Steps && nd(s, afile, c)->first != 0) {
      return call_composite(s, file, call, afile, anode, nd(s, afile, c)->first, params, pnames, np);
    }
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
    return butcher(s, act, afile, anode, output, params, pnames, pnodes, np, shown, nshown, shown_names);
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
  s->ir_call_file = file;
  s->ir_call_node = callnode;
  ozni_index irn = ir_record_call(s, act, afile, anode, params, pnames, np, out, nullptr, 0);
  if (out != nullptr) {
    out->irn = irn;
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
  int count = dim_named(s, "Count");
  bool number = dimensionless(r.q.d) || (count >= 0 && r.q.d.e[count] == 1 && !dims_equal(f->amount.d, r.q.d));
  if (!number) {
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
  part->irn = ir_source(s, f);
  part->irpart = f->irpart;
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
    ozni_buf d = {.a = s->c->allocator};
    put_rat(&d, r.q.v.lo);
    part->irdiv = ir_text(s, &d);
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
  if (ir_on(s)) {
    ozni_index irn = ir_simple(s, "split", &fv.f, 1, "amount", &qv, nullptr, outs, 2);
    for (uint32_t k = 0; k < 2; k++) {
      outs[k]->irn = irn;
      outs[k]->irpart = k + 1;
    }
  }
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

static bool rat_power(ozni_rat r, int64_t k, ozni_rat *out) {
  *out = ozni_rat_int(1);
  for (int64_t i = 0; i < k && i < 16; i++) {
    if (ozni_rat_mul(*out, r, out) != OZNI_OK) {
      return false;
    }
  }
  return true;
}

static bool heating_field(sema *s, ozni_index file, ozni_index section, const char *name, ozni_qty *out) {
  return prop_at(s, file, field_first(s, file, section, name), out);
}

static bool cut_thickness(sema *s, const food *f, ozni_qty *out) {
  for (uint32_t k = 0; k < f->naxes; k++) {
    const axis_state *st = &f->axes[k];
    if (!str_is(S(s, st->axis), "shape") || !str_is(S(s, st->value), "cut")) {
      continue;
    }
    ozni_index pf, pn;
    if (st->style != 0 && find_def(s, intern(s, "nominalCutSize", 14), &pf, &pn) && nd(s, pf, pn)->first != 0 &&
        nd(s, pf, nd(s, pf, pn)->first)->kind == OZNI_N_Record) {
      for (ozni_index e = nd(s, pf, nd(s, pf, pn)->first)->first; e != 0; e = nd(s, pf, e)->next) {
        if (nd(s, pf, e)->text == st->style && nd(s, pf, e)->first != 0 &&
            qty_of(s, mod_of_file(s, pf), pf, nd(s, pf, e)->first, out, false)) {
          return true;
        }
      }
    }
    if (st->size.known) {
      *out = st->size;
      return true;
    }
  }
  return false;
}

static value eval_heating(sema *s, const ozni_node *call) {
  ozni_index a0 = nd(s, s->file, call->first)->next;
  ozni_index a1 = a0 != 0 ? nd(s, s->file, a0)->next : 0;
  ozni_index a2 = a1 != 0 ? nd(s, s->file, a1)->next : 0;
  value fv = eval(s, a0);
  value nv = a2 != 0 ? eval(s, a2) : unknown();
  if (fv.kind != V_FOOD || fv.f->file == NONE || a1 == 0 || nv.kind != V_QTY || !nv.q.known) {
    return unknown();
  }
  const food *f = fv.f;
  ozni_index props = child_section(s, f->file, f->node, "props");
  ozni_index heat = props != 0 ? child_section(s, f->file, props, "heating") : 0;
  ozni_str method = S(s, nd(s, s->file, a1)->text);
  char buf[64];
  if (heat == 0 || method.len >= sizeof buf) {
    return unknown();
  }
  ozni_copy(buf, method.p, method.len);
  buf[method.len] = '\0';
  ozni_index m = child_section(s, f->file, heat, buf);
  value out = {.kind = V_QTY};
  if (m == 0) {
    return unknown();
  }
  if (heating_field(s, f->file, m, "direct", &out.q)) {
    return out;
  }
  ozni_qty up, at, soften, thick;
  if (!heating_field(s, f->file, m, "heatUp", &up) || !heating_field(s, f->file, m, "at", &at) ||
      !heating_field(s, f->file, m, "soften", &soften)) {
    return unknown();
  }
  if (!cut_thickness(s, f, &thick) || !dims_equal(thick.d, at.d)) {
    thick = at;
  }
  ozni_ival ratio;
  if (!ival_divide(thick.v, at.v, &ratio)) {
    return unknown();
  }
  int64_t kl = ozni_rat_floor(nv.q.v.lo).num, kh = ozni_rat_ceil(nv.q.v.hi).num;
  bool grows = ozni_rat_cmp(ratio.lo, ozni_rat_int(1)) >= 0;
  ozni_rat pl, ph;
  if (!rat_power(grows ? ratio.lo : ratio.hi, grows ? kl : kh, &pl) ||
      !rat_power(grows ? ratio.hi : ratio.lo, grows ? kh : kl, &ph)) {
    return unknown();
  }
  ozni_ival scaled;
  if (!ival_mul(up.v, (ozni_ival){pl, ph}, &scaled) || ozni_ival_add(scaled, soften.v, &out.q.v) != OZNI_OK) {
    return unknown();
  }
  out.q.d = up.d;
  out.q.known = true;
  return out;
}

static value eval_member(sema *s, const ozni_node *n) {
  const ozni_node *inner = n->first != 0 ? nd(s, s->file, n->first) : nullptr;
  if (inner == nullptr || inner->kind != OZNI_N_Member || inner->first == 0 ||
      nd(s, s->file, inner->first)->kind != OZNI_N_Name || !str_is(S(s, nd(s, s->file, inner->first)->text), "kitchen")) {
    return unknown();
  }
  ozni_str read = src_text(s, s->file, n);
  diag_node(s, OZNI_D_SCH_kitchenDependent, s->file, n, &read, 1);
  if (s->knode == 0) {
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
  if (n->kind == OZNI_N_Assemble) {
    return eval_assemble(s, n);
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
      return call_action(s, s->file, node, afile, anode);
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
      ozni_str read = src_text(s, s->file, n);
      diag_node(s, OZNI_D_SCH_kitchenDependent, s->file, n, &read, 1);
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
    if (callee->kind == OZNI_N_Name && str_is(S(s, callee->text), "heating") && callee->next != 0) {
      return eval_heating(s, n);
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
    collect_allergens(s, f);
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
    f->in_label = item->text;
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
      bool saved_in = s->in_branch;
      ozni_index saved_last = s->branch_last;
      ozni_index first_node = ir_on(s) ? ozni_table_count(s->ir.nodes) : 0;
      s->group = ++s->next_id;
      for (ozni_index b = n->first; b != 0 && !s->c->limit_hit; b = nd(s, s->file, b)->next) {
        s->branch = ++s->next_id;
        s->in_branch = true;
        s->branch_last = 0;
        if (tick(s, s->file, nd(s, s->file, b)) && nd(s, s->file, b)->first != 0) {
          eval_scoped(s, nd(s, s->file, b)->first, false);
        }
      }
      s->group = saved_group;
      s->branch = saved_branch;
      s->in_branch = saved_in;
      s->branch_last = saved_last;
      if (ir_on(s)) {
        ozni_table *join = ozni_table_new(s->c->arena, sizeof(ozni_index));
        for (ozni_index k = first_node; join != nullptr && k < ozni_table_count(s->ir.nodes); k++) {
          ozni_ir_kind kind = ((const ozni_ir_node *)ozni_table_get(s->ir.nodes, k))->kind;
          ozni_index id = k + 1;
          if ((kind == OZNI_IR_OP || kind == OZNI_IR_CHOOSE || kind == OZNI_IR_PLATE) &&
              !ozni_table_push(join, &id, nullptr)) {
            s->c->oom = true;
          }
        }
        if (join == nullptr) {
          s->c->oom = true;
        }
        s->join = join;
      }
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

typedef struct wave_info {
  ozni_index irn;
  ozni_rat gap;
} wave_info;

enum { TIMING_NONE, TIMING_WITHIN, TIMING_WARM, TIMING_CHILLED, TIMING_FROZEN };

typedef struct plate_info {
  ozni_index irn;
  uint8_t timing;
  ozni_rat limit;
  ozni_index on;
  uint32_t first_unit;
  uint32_t nunits;
  ozni_index file;
  ozni_index items[16];
  uint32_t nitems;
} plate_info;

typedef struct plate_unit {
  ozni_index entry;
  uint32_t number;
} plate_unit;

static bool within_of(sema *s, ozni_index file, ozni_index call, ozni_rat *limit) {
  const ozni_node *cn = nd(s, file, call);
  ozni_qty q;
  if (cn->kind != OZNI_N_Call || cn->first == 0 || !str_is(S(s, nd(s, file, cn->first)->text), "serveWithin")) {
    return false;
  }
  ozni_index arg = nd(s, file, cn->first)->next;
  while (arg != 0 && nd(s, file, arg)->kind == OZNI_N_CallArgs) {
    arg = nd(s, file, arg)->first;
  }
  if (arg == 0 || !qty_of(s, mod_of_file(s, file), file, arg, &q, false)) {
    return false;
  }
  *limit = q.v.hi;
  return true;
}

static uint8_t timing_of(sema *s, ozni_index file, ozni_index holder, ozni_rat *limit, ozni_index *on) {
  for (ozni_index c = nd(s, file, holder)->first; c != 0; c = nd(s, file, c)->next) {
    const ozni_node *an = nd(s, file, c);
    if (an->kind != OZNI_N_Annotation) {
      continue;
    }
    ozni_str name = S(s, an->text);
    ozni_qty q;
    ozni_index pf, pn;
    if (str_is(name, "chilled")) {
      return TIMING_CHILLED;
    }
    if (str_is(name, "frozen")) {
      return TIMING_FROZEN;
    }
    if (str_is(name, "serveWithin") && an->first != 0 && qty_of(s, mod_of_file(s, file), file, an->first, &q, false)) {
      *limit = q.v.hi;
      return TIMING_WITHIN;
    }
    if (str_is(name, "keepWarm")) {
      *limit = ozni_rat_int(0);
      for (ozni_index a = an->first; a != 0; a = nd(s, file, a)->next) {
        const ozni_node *na = nd(s, file, a);
        if (na->kind == OZNI_N_NamedArg && na->first != 0 && str_is(S(s, na->text), "on")) {
          *on = nd(s, file, na->first)->text;
        } else if (na->kind == OZNI_N_NamedArg && na->first != 0 && str_is(S(s, na->text), "max") &&
                   qty_of(s, mod_of_file(s, file), file, na->first, &q, false)) {
          *limit = q.v.hi;
        }
      }
      return TIMING_WARM;
    }
    if (find_from(s, file, an->text, &pf, &pn) && nd(s, pf, pn)->kind == OZNI_N_Property && nd(s, pf, pn)->first != 0 &&
        within_of(s, pf, nd(s, pf, pn)->first, limit)) {
      return TIMING_WITHIN;
    }
  }
  return TIMING_NONE;
}

static uint32_t plate_people(sema *s) {
  value pv;
  ozni_qty q;
  if (lookup_local(s, intern(s, "people", 6), &pv) && pv.kind == V_QTY && pv.q.known && pv.q.v.lo.den == 1 &&
      pv.q.v.lo.num > 0 && pv.q.v.lo.num < 10000) {
    return (uint32_t)pv.q.v.lo.num;
  }
  if (s->servings.known && s->servings.v.lo.den == 1 && s->servings.v.lo.num > 0 && s->servings.v.lo.num < 10000) {
    return (uint32_t)s->servings.v.lo.num;
  }
  ozni_index sv = s->onode != 0 ? options_value(s, intern(s, "servings", 8)) : 0;
  if (sv != 0 && value_qty(s, s->ofile, sv, &q) && q.v.lo.den == 1 && q.v.lo.num > 0 && q.v.lo.num < 10000) {
    return (uint32_t)q.v.lo.num;
  }
  return 1;
}

static uint32_t vessel_names(sema *s, ozni_index file, ozni_index node, ozni_index *out, uint32_t n, uint32_t cap) {
  const ozni_node *vn = nd(s, file, node);
  if (n >= cap) {
    return n;
  }
  if (vn->kind == OZNI_N_Name || vn->kind == OZNI_N_TypeRef) {
    ozni_index pf, pn;
    const ozni_node *body = find_from(s, file, vn->text, &pf, &pn) && nd(s, pf, pn)->kind == OZNI_N_Property &&
                                    nd(s, pf, pn)->first != 0
                                ? nd(s, pf, nd(s, pf, pn)->first)
                                : nullptr;
    if (body != nullptr && body->kind == OZNI_N_Call && body->first != 0 &&
        str_is(S(s, nd(s, pf, body->first)->text), "vesselOrder")) {
      for (ozni_index a = nd(s, pf, body->first)->next; a != 0; a = nd(s, pf, a)->next) {
        n = vessel_names(s, pf, a, out, n, cap);
      }
      return n;
    }
    out[n++] = vn->text;
    return n;
  }
  for (ozni_index c = vn->kind == OZNI_N_NamedArg ? 0 : vn->first; c != 0; c = nd(s, file, c)->next) {
    n = vessel_names(s, file, c, out, n, cap);
  }
  return n;
}

static bool kind_is(sema *s, ozni_index kfile, ozni_index knode, ozni_index name, uint32_t depth);

static void allocate_vessels(sema *s, const ozni_node *pn, ozni_index on, const food *dish, plate_info *info) {
  ozni_index names[16];
  uint32_t nn = 0;
  if (on != 0) {
    for (ozni_index v = nd(s, s->file, on)->first; v != 0; v = nd(s, s->file, v)->next) {
      nn = vessel_names(s, s->file, v, names, nn, 16);
    }
  } else if (dish != nullptr && dish->made_node != 0) {
    for (ozni_index c = nd(s, dish->made_file, dish->made_node)->first; c != 0;
         c = nd(s, dish->made_file, c)->next) {
      const ozni_node *an = nd(s, dish->made_file, c);
      for (ozni_index a = an->kind == OZNI_N_Annotation && str_is(S(s, an->text), "vessel") ? an->first : 0; a != 0;
           a = nd(s, dish->made_file, a)->next) {
        nn = vessel_names(s, dish->made_file, a, names, nn, 16);
      }
    }
  }
  bool any = false;
  for (ozni_index k = 0; s->kentries != nullptr && k < ozni_table_count(s->kentries); k++) {
    const kentry *e = ozni_table_get(s->kentries, k);
    any = any || nd(s, e->kfile, e->knode)->kind == OZNI_N_Vessel;
  }
  if (!any || nn == 0 || s->vessel_used == nullptr) {
    return;
  }
  uint32_t need = str_is(S(s, pn->text), "shared") ? 1 : plate_people(s), got = 0, have = 0;
  info->first_unit = ozni_table_count(s->plate_units);
  for (uint32_t i = 0; i < nn; i++) {
    uint32_t from_this = 0;
    for (ozni_index k = 0; k < ozni_table_count(s->kentries); k++) {
      const kentry *e = ozni_table_get(s->kentries, k);
      if (nd(s, e->kfile, e->knode)->kind != OZNI_N_Vessel || !kind_is(s, e->kfile, e->knode, names[i], 0)) {
        continue;
      }
      ozni_qty count;
      uint32_t total = entry_field(s, e, "count", &count) && count.v.lo.den == 1 ? (uint32_t)count.v.lo.num : 1;
      uint32_t *used = nullptr;
      for (ozni_index u = 0; u + 1 < ozni_table_count(s->vessel_used); u += 2) {
        if (*(const uint32_t *)ozni_table_get(s->vessel_used, u) == k) {
          used = ozni_table_at(s->vessel_used, u + 1);
        }
      }
      if (used == nullptr) {
        uint32_t pair[2] = {k, 0};
        if (!ozni_table_push(s->vessel_used, &pair[0], nullptr) || !ozni_table_push(s->vessel_used, &pair[1], nullptr)) {
          s->c->oom = true;
          return;
        }
        used = ozni_table_at(s->vessel_used, ozni_table_count(s->vessel_used) - 1);
      }
      bool seen = false;
      for (uint32_t j = 0; j < i; j++) {
        seen = seen || kind_is(s, e->kfile, e->knode, names[j], 0);
      }
      if (seen) {
        continue;
      }
      have += total;
      while (got < need && *used < total) {
        plate_unit pu = {e->name, ++*used};
        if (!ozni_table_push(s->plate_units, &pu, nullptr)) {
          s->c->oom = true;
        }
        got++;
        from_this++;
        info->nunits++;
      }
    }
    if (i > 0 && from_this > 0) {
      char num[16];
      size_t len = 0;
      char rev[16];
      for (uint32_t x = from_this; len == 0 || x != 0; x /= 10) {
        rev[len++] = (char)('0' + x % 10);
      }
      for (size_t j = 0; j < len; j++) {
        num[j] = rev[len - 1 - j];
      }
      ozni_str args[2] = {S(s, names[i]), {num, (uint32_t)len}};
      diag_node(s, OZNI_D_KIT_vesselSubstituted, s->file, pn, args, 2);
    }
  }
  if (got < need) {
    ozni_buf b = {.a = s->c->allocator};
    for (uint32_t i = 0; i < nn; i++) {
      ozni_str nm = S(s, names[i]);
      ozni_buf_puts(&b, i == 0 ? "" : " or ");
      ozni_buf_put(&b, nm.p, nm.len);
    }
    char need_txt[16], have_txt[16], rev[16];
    size_t nl = 0, hl = 0, len = 0;
    for (uint32_t x = need; len == 0 || x != 0; x /= 10) {
      rev[len++] = (char)('0' + x % 10);
    }
    while (len > 0) {
      need_txt[nl++] = rev[--len];
    }
    for (uint32_t x = have; len == 0 || x != 0; x /= 10) {
      rev[len++] = (char)('0' + x % 10);
    }
    while (len > 0) {
      have_txt[hl++] = rev[--len];
    }
    if (b.failed) {
      s->c->oom = true;
    } else {
      ozni_str args[3] = {{b.p != nullptr ? b.p : "", (uint32_t)b.len}, {need_txt, (uint32_t)nl},
                          {have_txt, (uint32_t)hl}};
      diag_node(s, OZNI_D_KIT_vesselShortage, s->file, pn, args, 3);
    }
    ozni_buf_discard(&b);
  }
}

static void ir_wave(sema *s, const ozni_node *serve) {
  ozni_index pair[2] = {serve->text, ++s->wave_no};
  if (!ozni_table_push(s->wave_names, pair, nullptr) || !ozni_table_push(s->wave_names, pair + 1, nullptr)) {
    s->c->oom = true;
  }
  ozni_buf b = {.a = s->c->allocator};
  ozni_buf_putu(&b, s->wave_no);
  for (ozni_index c = serve->first; c != 0; c = nd(s, s->file, c)->next) {
    const ozni_node *cn = nd(s, s->file, c);
    if (cn->kind != OZNI_N_After) {
      continue;
    }
    uint32_t prev = 0;
    for (ozni_index k = 0; k + 1 < ozni_table_count(s->wave_names); k += 2) {
      if (*(const ozni_index *)ozni_table_get(s->wave_names, k) == cn->text) {
        prev = *(const ozni_index *)ozni_table_get(s->wave_names, k + 1);
      }
    }
    ozni_buf_puts(&b, " after ");
    ozni_buf_putu(&b, prev);
    ozni_qty gap;
    if (cn->first != 0 && qty_of(s, s->mod, s->file, cn->first, &gap, false)) {
      ozni_buf_puts(&b, " + ");
      put_qty(s, &b, gap);
    }
  }
  ozni_ir_node node = {.kind = OZNI_IR_WAVE, .label = serve->text, .wave = s->wave_no, .text = ir_text(s, &b)};
  wave_info w = {ir_add(s, node), ozni_rat_int(0)};
  for (ozni_index c = serve->first; c != 0; c = nd(s, s->file, c)->next) {
    ozni_qty gap;
    if (nd(s, s->file, c)->kind == OZNI_N_After && nd(s, s->file, c)->first != 0 &&
        qty_of(s, s->mod, s->file, nd(s, s->file, c)->first, &gap, false)) {
      w.gap = gap.v.lo;
    }
  }
  if (s->wave_infos != nullptr && !ozni_table_push(s->wave_infos, &w, nullptr)) {
    s->c->oom = true;
  }
}

static ozni_index ir_plate(sema *s, ozni_index pidx, ozni_index on, food *const *items, uint32_t n) {
  const ozni_node *pn = nd(s, s->file, pidx);
  ozni_ir_node node = {.kind = OZNI_IR_PLATE, .label = pn->text, .wave = s->wave_no, .ninputs = n,
                       .first_input = ozni_table_count(s->ir.inputs), .first_req = ozni_table_count(s->ir.reqs),
                       .file = s->file, .src = pidx};
  ozni_buf b = {.a = s->c->allocator};
  for (uint32_t k = 0; k < n; k++) {
    ozni_buf_puts(&b, k == 0 ? " " : ", ");
    ir_put_ref(s, &b, items[k]);
  }
  ir_afters(s, &b, &node);
  bool first = true;
  for (ozni_index vs = on != 0 ? nd(s, s->file, on)->first : 0; vs != 0; vs = nd(s, s->file, vs)->next) {
    ozni_str vname = S(s, nd(s, s->file, vs)->text);
    ozni_buf_puts(&b, first ? " on " : " or ");
    ozni_buf_put(&b, vname.p, vname.len);
    first = false;
  }
  ozni_index pf, pd;
  ozni_buf_puts(&b, " [");
  if (find_def(s, intern(s, "plate", 5), &pf, &pd) && nd(s, pf, pd)->kind == OZNI_N_Action) {
    ir_put_uses(s, &b, pf, pd, nullptr, nullptr, 0, &node.time_lo, &node.time_hi, &node.unattended, &node.nreqs,
                &node.provisional);
  }
  ozni_buf_puts(&b, "]");
  node.text = ir_text(s, &b);
  return ir_add(s, node);
}

static void eval_serve(sema *s, const ozni_node *serve) {
  ozni_index hold = intern(s, "HoldsLiquid", 11);
  if (s->vessel_used != nullptr) {
    ozni_table_truncate(s->vessel_used, 0);
  }
  if (printing(s)) {
    put_serve_header(s, serve);
  }
  if (ir_on(s)) {
    ir_wave(s, serve);
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
    food *plated[16];
    uint32_t nplated = 0;
    plate_info info = {.file = s->file};
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
      consume(s, &v, s->file, c);
      if (nplated < 16) {
        info.items[nplated] = c;
        plated[nplated++] = v.f;
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
    info.nitems = nplated;
    info.timing = timing_of(s, s->file, p, &info.limit, &info.on);
    if (info.timing == TIMING_NONE && nplated > 0 && plated[0]->made_node != 0) {
      info.timing = timing_of(s, plated[0]->made_file, plated[0]->made_node, &info.limit, &info.on);
    }
    allocate_vessels(s, pn, on, nplated > 0 ? plated[0] : nullptr, &info);
    if (ir_on(s)) {
      info.irn = ir_plate(s, p, on, plated, nplated);
      if (s->plate_infos != nullptr && !ozni_table_push(s->plate_infos, &info, nullptr)) {
        s->c->oom = true;
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
  s->last = unknown();
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
    f->in_label = pn->text;
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

static void made_from(sema *s, food *o, const ozni_table *made) {
  o->allergens = made;
  collect_allergens(s, o);
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
      ir_source(s, v.f);
      food *copy = food_copy(s, v.f);
      if (copy == nullptr) {
        return unknown();
      }
      copy->irn = v.f->irn;
      copy->irpart = v.f->irpart;
      copy->irdiv = v.f->irdiv;
      v.f = copy;
    }
    vals[k] = v;
    set[k] = true;
  }
  saved_dish saved = save_dish(s);
  ozni_table *outer_allergens = s->allergens;
  s->allergens = ozni_table_new(s->c->arena, sizeof(ozni_index));
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
  ozni_table *made_allergens = s->allergens;
  s->allergens = outer_allergens;
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
      o->irn = item->f->irn;
      o->irpart = item->f->irpart;
      o->portion = true;
      made_from(s, o, made_allergens);
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
  o->irn = last.f->irn;
  o->irpart = last.f->irpart;
  o->irdiv = last.f->irdiv;
  o->type = output != 0 && nd(s, rfile, output)->kind == OZNI_N_TypeRef ? nd(s, rfile, output)->text
                                                                        : capitalized(s, rec->text);
  made_from(s, o, made_allergens);
  o->made_file = rfile;
  o->made_node = rnode;
  value out = {.kind = V_FOOD, .f = o};
  out.ref = record_step(s, action, shown, nshown, shown_names, o, nullptr, 0);
  return out;
}

static value eval_assemble(sema *s, const ozni_node *n) {
  value items[16];
  food *ins[16];
  ozni_index item_nodes[16];
  uint32_t k = 0, nin = 0;
  bool known = true;
  for (ozni_index c = n->first; c != 0 && k < 16; c = nd(s, s->file, c)->next) {
    if (nd(s, s->file, c)->kind == OZNI_N_Annotation) {
      continue;
    }
    items[k] = eval(s, c);
    if (items[k].kind == V_FOOD && items[k].f != nullptr) {
      consume(s, &items[k], s->file, c);
      ir_source(s, items[k].f);
      known = known && items[k].f->amount.known &&
              (nin == 0 || dims_equal(ins[0]->amount.d, items[k].f->amount.d));
      item_nodes[nin] = c;
      ins[nin++] = items[k].f;
    }
    k++;
  }
  food *out = food_new(s, intern(s, "Assembly", 8));
  if (out == nullptr) {
    return unknown();
  }
  for (uint32_t i = 0; known && i < nin; i++) {
    ozni_ival sum;
    if (i == 0) {
      out->amount = ins[0]->amount;
    } else if (ozni_ival_add(out->amount.v, ins[i]->amount.v, &sum) == OZNI_OK) {
      out->amount.v = sum;
    } else {
      out->amount.known = false;
    }
  }
  if (ir_on(s)) {
    ozni_ir_node node = {.kind = OZNI_IR_OP, .label = intern(s, "assemble", 8), .ninputs = nin,
                         .first_input = ozni_table_count(s->ir.inputs), .first_req = ozni_table_count(s->ir.reqs),
                         .file = s->ir_call_file, .src = s->ir_call_node};
    ozni_buf b = {.a = s->c->allocator};
    for (uint32_t i = 0; i < nin; i++) {
      ozni_buf_puts(&b, i == 0 ? " " : ", ");
      ir_put_ref(s, &b, ins[i]);
    }
    ir_afters(s, &b, &node);
    ozni_buf_puts(&b, " :");
    for (uint32_t i = 0; i < nin; i++) {
      ozni_buf_puts(&b, i == 0 ? " " : ", ");
      put_food_type(s, &b, ins[i]);
    }
    ozni_buf_puts(&b, " -> ");
    put_food_line(s, &b, out);
    ozni_index lf, ld;
    ozni_buf_puts(&b, " [");
    if (find_def(s, intern(s, "layer", 5), &lf, &ld) && nd(s, lf, ld)->kind == OZNI_N_Action) {
      ir_put_uses(s, &b, lf, ld, nullptr, nullptr, 0, &node.time_lo, &node.time_hi, &node.unattended, &node.nreqs,
                  &node.provisional);
    }
    ozni_buf_puts(&b, "]");
    node.text = ir_text(s, &b);
    out->irn = ir_add(s, node);
    meta_push(s, out->irn, out, nullptr, nullptr, 0);
    plate_info info = {.irn = out->irn, .file = s->file, .nitems = nin};
    for (uint32_t i = 0; i < nin; i++) {
      info.items[i] = item_nodes[i];
    }
    if (s->assemble_infos != nullptr && out->irn != 0 && !ozni_table_push(s->assemble_infos, &info, nullptr)) {
      s->c->oom = true;
    }
  }
  value v = {.kind = V_FOOD, .f = out};
  v.ref = record_step(s, intern(s, "assemble", 8), items, k, nullptr, out, nullptr, 0);
  return v;
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
  if (ir_on(s)) {
    ozni_buf ib = {.a = s->c->allocator};
    ozni_buf_puts(&ib, " : ");
    put_food_line(s, &ib, f);
    ozni_str ty = src_text(s, s->file, nd(s, s->file, tnode));
    ozni_ir_node node = {.kind = OZNI_IR_BUY, .label = intern(s, ty.p, ty.len), .text = ir_text(s, &ib)};
    f->irn = ir_add(s, node);
    meta_push(s, f->irn, f, nullptr, nullptr, 0);
  }
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

static bool person_allergic(sema *s, ozni_index file, ozni_index person, ozni_index kind) {
  for (ozni_index e = nd(s, file, person)->first; e != 0; e = nd(s, file, e)->next) {
    const ozni_node *en = nd(s, file, e);
    if (en->kind != OZNI_N_Entry || !str_is(S(s, en->text), "allergies") || en->first == 0) {
      continue;
    }
    for (ozni_index v = nd(s, file, en->first)->first; v != 0; v = nd(s, file, v)->next) {
      if (nd(s, file, v)->text == kind) {
        return true;
      }
    }
  }
  return false;
}

static value eval_choose(sema *s, const ozni_node *n) {
  value opts[16];
  ozni_index onodes[16], ir_from[16], ir_to[16];
  uint64_t masks[16] = {0};
  uint32_t k = 0, counts[16] = {0}, total = 0;
  for (ozni_index c = n->first; c != 0 && k < 16; c = nd(s, s->file, c)->next) {
    if (nd(s, s->file, c)->kind != OZNI_N_Annotation) {
      onodes[k] = c;
      ir_from[k] = ir_on(s) ? ozni_table_count(s->ir.nodes) : 0;
      opts[k] = eval(s, c);
      ir_to[k] = ir_on(s) ? ozni_table_count(s->ir.nodes) : 0;
      k++;
    }
  }
  if (k == 0) {
    return unknown();
  }
  ozni_index people = s->onode != 0 ? options_value(s, intern(s, "people", 6)) : 0;
  if (people != 0 && nd(s, s->ofile, people)->kind == OZNI_N_List && nd(s, s->ofile, people)->first != 0) {
    uint32_t who = 0;
    uint64_t reported[16] = {0};
    for (ozni_index p = nd(s, s->ofile, people)->first; p != 0; p = nd(s, s->ofile, p)->next, who++) {
      uint32_t pick = 0;
      for (uint32_t j = k; j > 0; j--) {
        if (opts[j - 1].kind == V_FOOD && picks_type(s, s->ofile, p, opts[j - 1].f->type)) {
          pick = j - 1;
        }
      }
      counts[pick]++;
      total++;
      masks[pick] |= who < 64 ? (uint64_t)1 << who : 0;
      ozni_index kinds[16];
      uint32_t nk = opts[pick].kind == V_FOOD ? food_allergens(s, opts[pick].f, kinds, 16) : 0;
      for (uint32_t a = 0; a < nk && a < 64; a++) {
        if ((reported[pick] >> a & 1) == 0 && person_allergic(s, s->ofile, p, kinds[a])) {
          reported[pick] |= (uint64_t)1 << a;
          ozni_str args[2] = {S(s, kinds[a]), src_text(s, s->file, nd(s, s->file, onodes[pick]))};
          diag_node(s, OZNI_D_SAF_allergen, s->file, n, args, 2);
        }
      }
    }
    for (uint32_t j = 0; j < k && s->picks != nullptr; j++) {
      pick_range r = {ir_from[j], ir_to[j], masks[j]};
      if (masks[j] != 0 && ir_to[j] > ir_from[j] && !ozni_table_push(s->picks, &r, nullptr)) {
        s->c->oom = true;
      }
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
  if (s->choices != nullptr && ir_on(s)) {
    choice ch = {ir_from[0], n->text, ozni_table_count(s->choice_opts), k};
    for (uint32_t j = 0; j < k; j++) {
      const ozni_node *on = nd(s, s->file, onodes[j]);
      ozni_index lab = on->first != 0 ? nd(s, s->file, on->first)->text : 0;
      if (on->kind == OZNI_N_Make && on->first != 0 && nd(s, s->file, on->first)->kind == OZNI_N_Call &&
          nd(s, s->file, on->first)->first != 0) {
        lab = nd(s, s->file, nd(s, s->file, on->first)->first)->text;
      }
      choice_opt co = {ir_to[j] > ir_from[j] ? ir_to[j] : 0, lab, counts[j]};
      if (!ozni_table_push(s->choice_opts, &co, nullptr)) {
        s->c->oom = true;
      }
    }
    if (!ozni_table_push(s->choices, &ch, nullptr)) {
      s->c->oom = true;
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

static void put_json_rat(ozni_buf *b, ozni_rat r);

static void put_json_range(ozni_buf *b, ozni_rat lo, ozni_rat hi) {
  ozni_buf_puts(b, "{\"lo\":");
  put_json_rat(b, lo);
  ozni_buf_puts(b, ",\"hi\":");
  put_json_rat(b, hi);
  ozni_buf_puts(b, "}");
}

static void put_json_qty(sema *s, ozni_buf *b, ozni_qty q) {
  ozni_buf_puts(b, "{\"value\":");
  if (ozni_rat_cmp(q.v.lo, q.v.hi) == 0) {
    put_json_rat(b, q.v.lo);
  } else {
    put_json_range(b, q.v.lo, q.v.hi);
  }
  ozni_buf_puts(b, ",\"unit\":");
  if (dimensionless(q.d)) {
    ozni_buf_puts(b, "\"1\"");
  } else {
    ozni_buf ub = {.a = s->c->allocator};
    put_dims(s, &ub, q.d);
    ozni_buf_json(b, ub.p != nullptr ? ub.p : "", ub.failed ? 0 : ub.len);
    ozni_buf_discard(&ub);
  }
  ozni_buf_puts(b, "}");
}

static void put_json_reqs(sema *s, ozni_buf *b, uint32_t first, uint32_t n) {
  ozni_buf_puts(b, "[");
  for (uint32_t r = 0; r < n; r++) {
    const ozni_ir_req *req = ozni_table_get(s->ir.reqs, first + r);
    ozni_str t = req->type != 0 ? src_text(s, req->file, nd(s, req->file, req->type)) : S(s, req->text);
    ozni_buf_puts(b, r == 0 ? "" : ",");
    ozni_buf_json(b, t.p, t.len);
  }
  ozni_buf_puts(b, "]");
}

static void ir_json(sema *s, ozni_index name) {
  ozni_buf *b = s->c->ir_json;
  static const char *const words[] = {"in", "buy", "op", "choose", "plate", "wave"};
  ozni_str nm = S(s, name);
  ozni_buf_puts(b, b->len > 0 ? ",{\"name\":" : "{\"name\":");
  ozni_buf_json(b, nm.p, nm.len);
  ozni_buf_puts(b, ",\"nodes\":[");
  bool first = true;
  for (ozni_index k = s->ir_base; k < ozni_table_count(s->ir.nodes); k++) {
    const ozni_ir_node *node = ozni_table_get(s->ir.nodes, k);
    if (node->kind == OZNI_IR_WAVE) {
      continue;
    }
    ozni_buf_puts(b, first ? "{\"id\":" : ",{\"id\":");
    first = false;
    ozni_buf_putu(b, node->number);
    ozni_buf_puts(b, ",\"kind\":\"");
    ozni_buf_puts(b, words[node->kind]);
    ozni_buf_puts(b, "\",\"label\":");
    ozni_str label = S(s, node->label);
    ozni_buf_json(b, label.p, label.len);
    ozni_buf_puts(b, ",\"inputs\":[");
    for (uint32_t i = 0; i < node->ninputs; i++) {
      const ozni_ir_input *in = ozni_table_get(s->ir.inputs, node->first_input + i);
      const ozni_ir_node *src = in->node != 0 ? ozni_table_get(s->ir.nodes, in->node - 1) : nullptr;
      ozni_buf_puts(b, i == 0 ? "{\"node\":" : ",{\"node\":");
      ozni_buf_putu(b, src != nullptr ? src->number : 0);
      ozni_buf_puts(b, ",\"part\":");
      if (in->part != 0) {
        ozni_buf_putu(b, in->part);
      } else {
        ozni_buf_puts(b, "null");
      }
      ozni_buf_puts(b, "}");
    }
    ozni_buf_puts(b, "],\"after\":[");
    for (uint32_t i = 0; i < node->nafter; i++) {
      ozni_index a = *(const ozni_index *)ozni_table_get(s->ir.afters, node->first_after + i);
      ozni_buf_puts(b, i == 0 ? "" : ",");
      ozni_buf_putu(b, a != 0 ? ((const ozni_ir_node *)ozni_table_get(s->ir.nodes, a - 1))->number : 0);
    }
    ozni_buf_puts(b, "],\"requires\":");
    put_json_reqs(s, b, node->first_req, node->kind == OZNI_IR_CHOOSE ? 0 : node->nreqs);
    ozni_buf_puts(b, ",\"time\":");
    put_json_range(b, node->time_lo, node->time_hi);
    ozni_buf_puts(b, node->unattended ? ",\"unattended\":true,\"wave\":" : ",\"unattended\":false,\"wave\":");
    if (node->kind == OZNI_IR_PLATE && node->wave != 0) {
      ozni_buf_putu(b, node->wave);
    } else {
      ozni_buf_puts(b, "null");
    }
    ozni_buf_puts(b, ",\"variants\":[");
    for (uint32_t v = 0; node->kind == OZNI_IR_CHOOSE && v < node->nvariants; v++) {
      const ozni_ir_variant *var = ozni_table_get(s->ir.variants, node->first_variant + v);
      ozni_str vn = S(s, var->name);
      ozni_buf_puts(b, v == 0 ? "{\"name\":" : ",{\"name\":");
      ozni_buf_json(b, vn.p, vn.len);
      ozni_buf_puts(b, ",\"requires\":");
      put_json_reqs(s, b, var->first_req, var->nreqs);
      ozni_buf_puts(b, ",\"time\":");
      put_json_range(b, var->time_lo, var->time_hi);
      ozni_buf_puts(b, var->unattended ? ",\"unattended\":true}" : ",\"unattended\":false}");
    }
    const ir_meta *meta = nullptr;
    for (ozni_index m = 0; s->ir_meta != nullptr && m < ozni_table_count(s->ir_meta); m++) {
      const ir_meta *mm = ozni_table_get(s->ir_meta, m);
      meta = mm->irn == k + 1 ? mm : meta;
    }
    ozni_buf_puts(b, "],\"args\":[");
    for (uint32_t a = 0; meta != nullptr && a < meta->nargs; a++) {
      const ir_arg *arg = ozni_table_get(s->ir_args, meta->first_arg + a);
      ozni_str an = S(s, arg->name), av = S(s, arg->value);
      ozni_buf_puts(b, a == 0 ? "{\"name\":" : ",{\"name\":");
      ozni_buf_json(b, an.p, an.len);
      ozni_buf_puts(b, ",\"value\":");
      ozni_buf_json(b, av.p, av.len);
      ozni_buf_puts(b, "}");
    }
    ozni_buf_puts(b, "],\"quantity\":");
    if (meta != nullptr && meta->has_qty) {
      put_json_qty(s, b, meta->qty);
    } else {
      ozni_buf_puts(b, "null");
    }
    ozni_buf_puts(b, ",\"allergens\":[");
    for (uint32_t a = 0; meta != nullptr && a < meta->nallergens; a++) {
      ozni_str an = S(s, meta->allergens[a]);
      ozni_buf_puts(b, a == 0 ? "" : ",");
      ozni_buf_json(b, an.p, an.len);
    }
    ozni_buf_puts(b, "]}");
  }
  ozni_buf_puts(b, "],\"waves\":[");
  first = true;
  uint32_t prev = 0;
  for (ozni_index k = s->ir_base; k < ozni_table_count(s->ir.nodes); k++) {
    const ozni_ir_node *node = ozni_table_get(s->ir.nodes, k);
    if (node->kind != OZNI_IR_WAVE) {
      continue;
    }
    ozni_rat gap = ozni_rat_int(0);
    for (ozni_index w = 0; s->wave_infos != nullptr && w < ozni_table_count(s->wave_infos); w++) {
      const wave_info *wi = ozni_table_get(s->wave_infos, w);
      gap = wi->irn == k + 1 ? wi->gap : gap;
    }
    ozni_str wn = S(s, node->label);
    ozni_buf_puts(b, first ? "{\"id\":" : ",{\"id\":");
    first = false;
    ozni_buf_putu(b, node->wave);
    ozni_buf_puts(b, ",\"name\":");
    ozni_buf_json(b, wn.p, wn.len);
    ozni_buf_puts(b, ",\"after\":");
    if (prev != 0) {
      ozni_buf_putu(b, prev);
    } else {
      ozni_buf_puts(b, "null");
    }
    ozni_buf_puts(b, ",\"gap\":");
    put_json_rat(b, gap);
    ozni_buf_puts(b, "}");
    prev = node->wave;
  }
  ozni_buf_puts(b, "],\"choices\":[");
  first = true;
  for (ozni_index c = 0; s->choices != nullptr && c < ozni_table_count(s->choices); c++) {
    const choice *ch = ozni_table_get(s->choices, c);
    if (ch->at < s->ir_base) {
      continue;
    }
    ozni_str mode = S(s, ch->mode);
    ozni_buf_puts(b, first ? "{\"mode\":" : ",{\"mode\":");
    first = false;
    ozni_buf_json(b, mode.p, mode.len);
    ozni_buf_puts(b, ",\"options\":[");
    for (uint32_t o = 0; o < ch->nopts; o++) {
      const choice_opt *co = ozni_table_get(s->choice_opts, ch->first_opt + o);
      ozni_buf_puts(b, o == 0 ? "{\"node\":" : ",{\"node\":");
      if (co->irn != 0) {
        ozni_buf_putu(b, ((const ozni_ir_node *)ozni_table_get(s->ir.nodes, co->irn - 1))->number);
      } else {
        ozni_buf_puts(b, "null");
      }
      ozni_str lab = S(s, co->label);
      ozni_buf_puts(b, ",\"label\":");
      ozni_buf_json(b, lab.p, lab.len);
      ozni_buf_puts(b, ",\"count\":");
      ozni_buf_putu(b, co->count);
      ozni_buf_puts(b, "}");
    }
    ozni_buf_puts(b, "]}");
  }
  ozni_buf_puts(b, "]}");
}

static void ir_dump(sema *s, ozni_index name) {
  ozni_buf *b = s->out;
  static const char *const words[] = {"in", "buy", "op", "choose", "plate", "wave"};
  ozni_str nm = S(s, name);
  ozni_buf_puts(b, "ir ");
  ozni_buf_put(b, nm.p, nm.len);
  ozni_buf_puts(b, "\n");
  for (ozni_index k = s->ir_base; k < ozni_table_count(s->ir.nodes); k++) {
    ozni_ir_node *node = ozni_table_at(s->ir.nodes, k);
    node->dish = name;
    ozni_str label = S(s, node->label), text = S(s, node->text);
    if (node->kind == OZNI_IR_WAVE) {
      ozni_buf_puts(b, "  wave ");
    } else {
      ozni_buf_puts(b, "  %");
      ozni_buf_putu(b, node->number);
      ozni_buf_puts(b, " = ");
      ozni_buf_puts(b, words[node->kind]);
      ozni_buf_puts(b, " ");
      ozni_buf_put(b, label.p, label.len);
    }
    ozni_buf_put(b, text.p, text.len);
    ozni_buf_puts(b, "\n");
  }
}

typedef struct sched_unit {
  ozni_index entry;
  uint32_t number;
  ozni_index kfile;
  ozni_index knode;
  ozni_index label;
} sched_unit;

typedef struct sched_dirt {
  ozni_index name;
  ozni_index dfile;
  ozni_index dnode;
  ozni_index allergy;
  bool stain;
} sched_dirt;

typedef struct sched_hold {
  ozni_rat start;
  ozni_rat end;
  ozni_index file;
  ozni_index item;
  uint32_t unit;
  ozni_index plate;
} sched_hold;

typedef struct sched_out {
  ozni_sched plan;
  const ozni_rat *serves;
  const ozni_rat *gaps;
  const ozni_index *wave_names;
  uint32_t nwaves;
  const sched_hold *holds;
  uint32_t nholds;
  const uint32_t *merged;
  const uint32_t *task_of;
  const uint32_t *node_of;
  const ozni_table *units;
  const ozni_table *alt_variant;
  const ozni_table *dirts;
  const ozni_table *cases;
  const ozni_table *targets;
  uint32_t cooks;
  uint32_t last_number;
} sched_out;

static const char *const clean_words[] = {"none", "rinse", "wash", "soakThenWash"};

static void put_json_rat(ozni_buf *b, ozni_rat r) {
  ozni_buf_puts(b, "{\"num\":");
  if (r.num < 0) {
    ozni_buf_puts(b, "-");
  }
  ozni_buf_putu(b, (uint64_t)(r.num < 0 ? -r.num : r.num));
  ozni_buf_puts(b, ",\"den\":");
  ozni_buf_putu(b, r.den == 0 ? 1 : (uint64_t)r.den);
  ozni_buf_puts(b, "}");
}

static void put_json_str(ozni_buf *b, ozni_str v) {
  ozni_buf_puts(b, "\"");
  ozni_buf_put(b, v.p, v.len);
  ozni_buf_puts(b, "\"");
}

static ozni_index unit_label(sema *s, ozni_index entry, uint32_t number) {
  ozni_str en = S(s, entry);
  char buf[160];
  if (en.len + 12 > sizeof buf) {
    return entry;
  }
  ozni_copy(buf, en.p, en.len);
  size_t len = en.len;
  buf[len++] = '#';
  char rev[12];
  size_t nd_len = 0;
  do {
    rev[nd_len++] = (char)('0' + number % 10);
    number /= 10;
  } while (number != 0);
  while (nd_len > 0) {
    buf[len++] = rev[--nd_len];
  }
  return intern(s, buf, len);
}

static const sched_unit *unit_at(const sched_out *o, uint32_t u) { return ozni_table_get(o->units, u); }

static bool kind_is(sema *s, ozni_index kfile, ozni_index knode, ozni_index name, uint32_t depth) {
  if (depth > 16) {
    return false;
  }
  if (nd(s, kfile, knode)->text == name) {
    return true;
  }
  for (ozni_index c = nd(s, kfile, knode)->first; c != 0; c = nd(s, kfile, c)->next) {
    ozni_index tf, tn;
    if (nd(s, kfile, c)->kind != OZNI_N_TypeRef) {
      continue;
    }
    if (nd(s, kfile, c)->text == name) {
      return true;
    }
    if (find_from(s, kfile, nd(s, kfile, c)->text, &tf, &tn) &&
        (nd(s, tf, tn)->kind == OZNI_N_Kind || nd(s, tf, tn)->kind == OZNI_N_Trait ||
         nd(s, tf, tn)->kind == OZNI_N_Vessel) &&
        kind_is(s, tf, tn, name, depth + 1)) {
      return true;
    }
  }
  return false;
}

static bool kind_prop(sema *s, ozni_index kfile, ozni_index knode, const char *key, ozni_qty *out, uint32_t depth) {
  if (depth > 16) {
    return false;
  }
  if (prop_at(s, kfile, section_field(s, kfile, knode, "props", key), out)) {
    return true;
  }
  for (ozni_index c = nd(s, kfile, knode)->first; c != 0; c = nd(s, kfile, c)->next) {
    ozni_index tf, tn;
    if (nd(s, kfile, c)->kind == OZNI_N_TypeRef && find_from(s, kfile, nd(s, kfile, c)->text, &tf, &tn) &&
        nd(s, tf, tn)->kind == OZNI_N_Kind && kind_prop(s, tf, tn, key, out, depth + 1)) {
      return true;
    }
  }
  return false;
}

static bool std_qty(sema *s, const char *name, ozni_qty *out) {
  ozni_index pf, pn;
  return find_def(s, intern(s, name, strlen(name)), &pf, &pn) && nd(s, pf, pn)->first != 0 &&
         value_qty(s, pf, nd(s, pf, pn)->first, out);
}

static ozni_rat later_rat(ozni_rat a, ozni_rat b) { return ozni_rat_cmp(a, b) > 0 ? a : b; }

static ozni_sched_unit unit_info_for(sema *s, const sched_unit *su) {
  ozni_sched_unit ui = {ozni_rat_int(0), ozni_rat_int(0), OZNI_UNIT_OTHER, false, false};
  ozni_qty q;
  bool has = false;
  if (nd(s, su->kfile, su->knode)->kind == OZNI_N_Vessel) {
    ui.category = OZNI_UNIT_VESSEL;
    has = std_qty(s, "tablewareWashTime", &q);
  } else if (kind_is(s, su->kfile, su->knode, intern(s, "Tool", 4), 0)) {
    ui.category = OZNI_UNIT_TOOL;
    has = kind_prop(s, su->kfile, su->knode, "washTime", &q, 0);
  } else if (kind_is(s, su->kfile, su->knode, intern(s, "Dishwasher", 10), 0)) {
    ui.dishwasher = true;
    has = kind_prop(s, su->kfile, su->knode, "runTime", &q, 0);
  } else if (kind_is(s, su->kfile, su->knode, intern(s, "Place", 5), 0)) {
    ui.category = OZNI_UNIT_PLACE;
    ui.sink = kind_is(s, su->kfile, su->knode, intern(s, "Sink", 4), 0);
    has = kind_prop(s, su->kfile, su->knode, "wipeTime", &q, 0);
  }
  if (has) {
    ui.clean_lo = q.v.lo;
    ui.clean_hi = q.v.hi;
  }
  return ui;
}

static uint32_t target_bit(ozni_table *targets, ozni_index name) {
  for (ozni_index k = 0; k < ozni_table_count(targets); k++) {
    if (*(const ozni_index *)ozni_table_get(targets, k) == name) {
      return k;
    }
  }
  if (ozni_table_count(targets) >= 64 || !ozni_table_push(targets, &name, nullptr)) {
    return UINT32_MAX;
  }
  return ozni_table_count(targets) - 1;
}

static ozni_index prefixed_allergy(sema *s, ozni_index allergy) {
  ozni_str k = S(s, allergy);
  char buf[160];
  if (k.len + 16 > sizeof buf) {
    return 0;
  }
  ozni_copy(buf, "allergicPerson(", 15);
  ozni_copy(buf + 15, k.p, k.len);
  buf[15 + k.len] = ')';
  return intern(s, buf, k.len + 16);
}

static void dirt_cases(sema *s, const sched_dirt *d, ozni_table *cases, ozni_table *targets,
                       ozni_sched_dirt *out) {
  out->first_case = ozni_table_count(cases);
  out->ncases = 0;
  for (ozni_index t = nd(s, d->dfile, d->dnode)->first; t != 0; t = nd(s, d->dfile, t)->next) {
    if (nd(s, d->dfile, t)->kind != OZNI_N_TypeRef) {
      continue;
    }
    for (ozni_index r = 0; s->wrules != nullptr && r < ozni_table_count(s->wrules); r++) {
      const wrule *w = ozni_table_get(s->wrules, r);
      if (w->after != nd(s, d->dfile, t)->text) {
        continue;
      }
      for (ozni_index c = nd(s, w->file, w->node)->first; c != 0; c = nd(s, w->file, c)->next) {
        const ozni_node *rc = nd(s, w->file, c);
        if (rc->kind != OZNI_N_RuleCase) {
          continue;
        }
        ozni_sched_case k = {.rule = r, .when = OZNI_WHEN_DEFAULT};
        for (uint8_t l = 0; l < 4; l++) {
          k.level = str_is(S(s, rc->text), clean_words[l]) ? l : k.level;
        }
        const ozni_node *target = rc->first != 0 ? nd(s, w->file, rc->first) : nullptr;
        if (target != nullptr && target->kind == OZNI_N_TypeRef) {
          ozni_str tn = S(s, target->text);
          if (str_is(tn, "sameFoodSameState")) {
            k.when = OZNI_WHEN_SAME_FOOD;
          } else if (str_is(tn, "sameTool")) {
            k.when = OZNI_WHEN_SAME_TOOL;
          } else {
            k.when = OZNI_WHEN_TARGET;
            ozni_index name = str_is(tn, "allergicPerson") ? (d->allergy != 0 ? prefixed_allergy(s, d->allergy) : 0)
                                                           : target->text;
            uint32_t bit = name != 0 ? target_bit(targets, name) : UINT32_MAX;
            k.bits = bit < 64 ? (uint64_t)1 << bit : 0;
          }
        }
        if (!ozni_table_push(cases, &k, nullptr)) {
          s->c->oom = true;
        }
        out->ncases++;
      }
    }
  }
}

static uint32_t dirt_id(sema *s, ozni_table *dirts, const wmark *m) {
  for (ozni_index k = 0; k < ozni_table_count(dirts); k++) {
    if (((const sched_dirt *)ozni_table_get(dirts, k))->name == m->dirt) {
      return k;
    }
  }
  sched_dirt d = {m->dirt, m->dfile, m->dnode, m->allergy,
                  decl_has_trait(s, m->dfile, m->dnode, intern(s, "Staining", 8), 0)};
  if (!ozni_table_push(dirts, &d, nullptr)) {
    s->c->oom = true;
    return 0;
  }
  return ozni_table_count(dirts) - 1;
}

static const wnode *wnode_for(const sema *s, ozni_index irn) {
  for (ozni_index k = 0; s->wnodes != nullptr && k < ozni_table_count(s->wnodes); k++) {
    const wnode *w = ozni_table_get(s->wnodes, k);
    if (w->irn == irn) {
      return w;
    }
  }
  return nullptr;
}

static bool wnode_has(sema *s, const wnode *w, ozni_index name) {
  if (w == nullptr || w->ffile == NONE) {
    return false;
  }
  if (decl_has_trait(s, w->ffile, w->fnode, name, 0)) {
    return true;
  }
  for (ozni_index d = 0; w->derived != nullptr && d < ozni_table_count(w->derived); d++) {
    if (*(const ozni_index *)ozni_table_get(w->derived, d) == name) {
      return true;
    }
  }
  return false;
}

static uint64_t task_targets(sema *s, const ozni_table *targets, ozni_index table_index, const wnode *w,
                             ozni_index people) {
  uint64_t mask = 0, out = 0;
  for (ozni_index k = 0; s->picks != nullptr && k < ozni_table_count(s->picks); k++) {
    const pick_range *r = ozni_table_get(s->picks, k);
    mask |= table_index >= r->first && table_index < r->end ? r->people : 0;
  }
  for (ozni_index b = 0; b < ozni_table_count(targets); b++) {
    ozni_index name = *(const ozni_index *)ozni_table_get(targets, b);
    ozni_str text = S(s, name);
    bool hit = false;
    if (text.len > 15 && memcmp(text.p, "allergicPerson(", 15) == 0) {
      ozni_index kind = allergen_kind(s, text);
      uint32_t who = 0;
      for (ozni_index p = people != 0 && nd(s, s->ofile, people)->kind == OZNI_N_List
                              ? nd(s, s->ofile, people)->first
                              : 0;
           p != 0 && !hit; p = nd(s, s->ofile, p)->next, who++) {
        hit = who < 64 && (mask >> who & 1) != 0 && person_allergic(s, s->ofile, p, kind);
      }
    } else {
      hit = wnode_has(s, w, name) || wnode_has(s, w, capitalized(s, name));
    }
    out |= hit ? (uint64_t)1 << b : 0;
  }
  return out;
}

static const plate_info *plate_info_for(const sema *s, ozni_index irn) {
  for (ozni_index k = 0; s->plate_infos != nullptr && k < ozni_table_count(s->plate_infos); k++) {
    const plate_info *pi = ozni_table_get(s->plate_infos, k);
    if (pi->irn == irn) {
      return pi;
    }
  }
  return nullptr;
}

static ozni_index cut_key(sema *s, const ozni_ir_node *node) {
  bool cutting = false;
  for (uint32_t r = 0; r < node->nreqs; r++) {
    cutting = cutting || str_is(S(s, ((const ozni_ir_req *)ozni_table_get(s->ir.reqs, node->first_req + r))->text),
                                "CuttingSurface");
  }
  if (!cutting || node->kind != OZNI_IR_OP) {
    return 0;
  }
  ozni_str label = S(s, node->label), text = S(s, node->text);
  char buf[512];
  size_t len = 0;
  for (size_t i = 0; i < label.len && len < sizeof buf; i++) {
    buf[len++] = label.p[i];
  }
  bool output = false;
  for (size_t i = 0; i < text.len && len < sizeof buf; i++) {
    if (text.p[i] == '%') {
      while (i + 1 < text.len && ((text.p[i + 1] >= '0' && text.p[i + 1] <= '9') || text.p[i + 1] == '.')) {
        i++;
      }
      continue;
    }
    if (i + 4 <= text.len && memcmp(text.p + i, " -> ", 4) == 0) {
      output = true;
    }
    if (text.p[i] == '[') {
      output = false;
    }
    if (!output) {
      buf[len++] = text.p[i];
    }
  }
  return len < sizeof buf ? intern(s, buf, len) : 0;
}

static const ozni_ir_node *task_node(const sema *s, const sched_out *o, uint32_t i) {
  return ozni_table_get(s->ir.nodes, s->ir_base + o->node_of[i]);
}

static ozni_str dirt_text(sema *s, const sched_out *o, uint32_t dirt) {
  return dirt != UINT32_MAX ? S(s, ((const sched_dirt *)ozni_table_get(o->dirts, dirt))->name)
                            : (ozni_str){"use", 3};
}

static void schedule_json(sema *s, const sched_out *o) {
  ozni_buf *b = s->out;
  const ozni_sched *p = &o->plan;
  static const char *const cleanups[] = {"none", "beforeMeal", "afterMeal"};
  static const char *const categories[] = {"other", "tool", "place", "vessel"};
  static const char *const extra_kinds[] = {"wash", "rinse", "soak", "wipe", "run"};
  ozni_buf_puts(b, "{\"schemaVersion\":1,\"cooks\":");
  ozni_buf_putu(b, o->cooks);
  ozni_buf_puts(b, ",\"cleanup\":\"");
  ozni_buf_puts(b, cleanups[p->cleanup]);
  ozni_buf_puts(b, "\",\"rules\":[");
  for (ozni_index d = 0; d < ozni_table_count(o->dirts); d++) {
    const ozni_sched_dirt *sd = &p->dirts[d];
    ozni_buf_puts(b, d == 0 ? "{\"dirt\":" : ",{\"dirt\":");
    put_json_str(b, dirt_text(s, o, d));
    ozni_buf_puts(b, ",\"cases\":[");
    for (uint32_t c = 0; c < sd->ncases; c++) {
      const ozni_sched_case *k = &p->cases[sd->first_case + c];
      ozni_buf_puts(b, c == 0 ? "{\"rule\":" : ",{\"rule\":");
      ozni_buf_putu(b, k->rule);
      ozni_buf_puts(b, ",\"level\":\"");
      ozni_buf_puts(b, clean_words[k->level]);
      ozni_buf_puts(b, "\",\"when\":");
      if (k->when == OZNI_WHEN_DEFAULT) {
        ozni_buf_puts(b, "null");
      } else if (k->when == OZNI_WHEN_SAME_FOOD) {
        ozni_buf_puts(b, "\"sameFoodSameState\"");
      } else if (k->when == OZNI_WHEN_SAME_TOOL) {
        ozni_buf_puts(b, "\"sameTool\"");
      } else {
        uint32_t bit = 0;
        while (bit < 64 && (k->bits >> bit & 1) == 0) {
          bit++;
        }
        put_json_str(b, bit < ozni_table_count(o->targets)
                            ? S(s, *(const ozni_index *)ozni_table_get(o->targets, bit))
                            : (ozni_str){"", 0});
      }
      ozni_buf_puts(b, "}");
    }
    ozni_buf_puts(b, "]}");
  }
  ozni_buf_puts(b, "],\"waves\":[");
  for (uint32_t w = 0; w < o->nwaves; w++) {
    ozni_buf_puts(b, w == 0 ? "{\"id\":" : ",{\"id\":");
    ozni_buf_putu(b, w + 1);
    ozni_buf_puts(b, ",\"serveAt\":");
    put_json_rat(b, o->serves[w]);
    ozni_buf_puts(b, ",\"after\":");
    if (w > 0) {
      ozni_buf_putu(b, w);
    } else {
      ozni_buf_puts(b, "null");
    }
    ozni_buf_puts(b, ",\"gap\":");
    put_json_rat(b, o->gaps[w]);
    ozni_buf_puts(b, "}");
  }
  ozni_buf_puts(b, "],\"dishes\":[");
  bool first_dish = true;
  for (uint32_t i = 0; i < p->ntasks; i++) {
    const ozni_ir_node *node = task_node(s, o, i);
    const plate_info *pi = node->kind == OZNI_IR_PLATE ? plate_info_for(s, s->ir_base + o->node_of[i] + 1) : nullptr;
    bool limited = pi != nullptr && (pi->timing == TIMING_WITHIN ||
                                     (pi->timing == TIMING_WARM && ozni_rat_cmp(pi->limit, ozni_rat_int(0)) > 0));
    for (uint32_t j = 0; limited && j < node->ninputs; j++) {
      ozni_index src = ((const ozni_ir_input *)ozni_table_get(s->ir.inputs, node->first_input + j))->node;
      uint32_t r = src > s->ir_base ? o->task_of[src - 1 - s->ir_base] : UINT32_MAX;
      if (r == UINT32_MAX) {
        continue;
      }
      ozni_buf_puts(b, first_dish ? "{\"id\":" : ",{\"id\":");
      first_dish = false;
      ozni_buf_putu(b, task_node(s, o, r)->number);
      ozni_buf_puts(b, ",\"serveWithin\":");
      put_json_rat(b, pi->limit);
      ozni_buf_puts(b, ",\"wave\":");
      ozni_buf_putu(b, node->wave);
      ozni_buf_puts(b, ",\"ready\":");
      ozni_buf_putu(b, task_node(s, o, r)->number);
      ozni_buf_puts(b, "}");
    }
  }
  ozni_buf_puts(b, "],\"resources\":[");
  for (ozni_index u = 0; u < ozni_table_count(o->units); u++) {
    const sched_unit *su = unit_at(o, u);
    ozni_buf_puts(b, u == 0 ? "{\"id\":" : ",{\"id\":");
    put_json_str(b, S(s, su->label));
    ozni_buf_puts(b, ",\"kind\":");
    put_json_str(b, S(s, nd(s, su->kfile, su->knode)->text));
    const ozni_sched_unit *ui = &p->unit_info[u];
    bool fixture = ui->category == OZNI_UNIT_PLACE && ozni_rat_cmp(ui->clean_hi, ozni_rat_int(0)) == 0;
    ozni_buf_puts(b, ",\"capacity\":1,\"category\":\"");
    ozni_buf_puts(b, fixture ? "fixture" : categories[ui->category]);
    ozni_buf_puts(b, "\"}");
  }
  ozni_buf_puts(b, "],\"tasks\":[");
  for (uint32_t i = 0; i < p->ntasks; i++) {
    const ozni_sched_task *t = &p->tasks[i];
    const ozni_ir_node *node = task_node(s, o, i);
    const ozni_sched_alt *alt = &p->alts[t->first_alt + t->chosen];
    ozni_buf_puts(b, i == 0 ? "{\"id\":" : ",{\"id\":");
    ozni_buf_putu(b, node->number);
    ozni_buf_puts(b, node->kind == OZNI_IR_PLATE ? ",\"kind\":\"plate\",\"op\":" : ",\"kind\":\"op\",\"op\":");
    put_json_str(b, S(s, node->label));
    ozni_buf_puts(b, ",\"deps\":[");
    for (uint32_t d = 0; d < t->ndeps; d++) {
      ozni_buf_puts(b, d == 0 ? "" : ",");
      ozni_buf_putu(b, task_node(s, o, p->deps[t->first_dep + d])->number);
    }
    ozni_buf_puts(b, "],\"start\":");
    put_json_rat(b, t->start);
    ozni_buf_puts(b, ",\"endHi\":");
    put_json_rat(b, t->end_hi);
    ozni_buf_puts(b, ",\"endLo\":");
    put_json_rat(b, t->end_lo);
    ozni_buf_puts(b, ",\"cook\":");
    if (t->cook != 0) {
      ozni_buf_putu(b, t->cook);
    } else {
      ozni_buf_puts(b, "null");
    }
    ozni_buf_puts(b, ",\"uses\":[");
    for (uint32_t g = 0; g < alt->ngroups; g++) {
      ozni_buf_puts(b, g == 0 ? "" : ",");
      put_json_str(b, S(s, unit_at(o, p->groups[alt->first_group + g].pick)->label));
    }
    ozni_buf_puts(b, "],\"leaves\":[");
    for (uint32_t m = 0; m < alt->nmarks; m++) {
      const ozni_sched_mark *mk = &p->marks[alt->first_mark + m];
      ozni_buf_puts(b, m == 0 ? "{\"resource\":" : ",{\"resource\":");
      put_json_str(b, S(s, unit_at(o, p->groups[alt->first_group + mk->group].pick)->label));
      ozni_buf_puts(b, ",\"dirt\":");
      put_json_str(b, dirt_text(s, o, mk->dirt));
      ozni_buf_puts(b, ",\"food\":");
      if (mk->food != 0) {
        put_json_str(b, S(s, mk->food));
      } else {
        ozni_buf_puts(b, "null");
      }
      ozni_buf_puts(b, "}");
    }
    ozni_buf_puts(b, "],\"food\":");
    if (t->food != 0) {
      put_json_str(b, S(s, t->food));
    } else {
      ozni_buf_puts(b, "null");
    }
    ozni_buf_puts(b, ",\"targets\":[");
    bool first = true;
    for (ozni_index k = 0; k < ozni_table_count(o->targets); k++) {
      if ((t->targets >> k & 1) != 0) {
        ozni_buf_puts(b, first ? "" : ",");
        put_json_str(b, S(s, *(const ozni_index *)ozni_table_get(o->targets, k)));
        first = false;
      }
    }
    ozni_buf_puts(b, alt->hands ? "],\"unattended\":false,\"wave\":" : "],\"unattended\":true,\"wave\":");
    if (node->kind == OZNI_IR_PLATE && node->wave != 0) {
      ozni_buf_putu(b, node->wave);
    } else {
      ozni_buf_puts(b, "null");
    }
    ozni_buf_puts(b, "}");
  }
  for (uint32_t k = 0; k < p->nextras; k++) {
    const ozni_sched_extra *e = &p->extras[k];
    ozni_buf_puts(b, p->ntasks + k == 0 ? "{\"id\":" : ",{\"id\":");
    ozni_buf_putu(b, o->last_number + 1 + k);
    ozni_buf_puts(b, ",\"kind\":\"");
    ozni_buf_puts(b, extra_kinds[e->kind]);
    ozni_buf_puts(b, "\",\"op\":\"");
    ozni_buf_puts(b, extra_kinds[e->kind]);
    ozni_buf_puts(b, "\",\"deps\":[],\"start\":");
    put_json_rat(b, e->start);
    ozni_buf_puts(b, ",\"endHi\":");
    put_json_rat(b, e->end_hi);
    ozni_buf_puts(b, ",\"endLo\":");
    put_json_rat(b, e->end_lo);
    ozni_buf_puts(b, ",\"cook\":");
    if (e->cook != 0) {
      ozni_buf_putu(b, e->cook);
    } else {
      ozni_buf_puts(b, "null");
    }
    ozni_buf_puts(b, ",\"uses\":[");
    if (e->sink != UINT32_MAX) {
      put_json_str(b, S(s, unit_at(o, e->sink)->label));
      ozni_buf_puts(b, ",");
    }
    put_json_str(b, S(s, unit_at(o, e->unit)->label));
    for (uint32_t q = 0; e->kind == OZNI_EXTRA_RUN && q < e->ntools; q++) {
      ozni_buf_puts(b, ",");
      put_json_str(b, S(s, unit_at(o, p->tools[e->first_tool + q])->label));
    }
    ozni_buf_puts(b, "],\"leaves\":[],\"food\":null,\"targets\":[]");
    if (e->kind == OZNI_EXTRA_RUN) {
      ozni_buf_puts(b, ",\"cleans\":[");
      for (uint32_t q = 0; q < e->ntools; q++) {
        ozni_buf_puts(b, q == 0 ? "{\"resource\":" : ",{\"resource\":");
        put_json_str(b, S(s, unit_at(o, p->tools[e->first_tool + q])->label));
        ozni_buf_puts(b, ",\"level\":\"wash\"}");
      }
      ozni_buf_puts(b, "]");
    } else if (e->kind != OZNI_EXTRA_SOAK) {
      ozni_buf_puts(b, ",\"cleans\":{\"resource\":");
      put_json_str(b, S(s, unit_at(o, e->unit)->label));
      ozni_buf_puts(b, ",\"level\":\"");
      ozni_buf_puts(b, e->kind == OZNI_EXTRA_WIPE ? "wipe" : clean_words[e->level]);
      ozni_buf_puts(b, "\"}");
    }
    ozni_buf_puts(b, e->cook != 0 ? ",\"unattended\":false,\"wave\":null}" : ",\"unattended\":true,\"wave\":null}");
  }
  ozni_buf_puts(b, "]}\n");
}

static ozni_index calibration_for(sema *s, const ozni_ir_req *req) {
  const ozni_node *rn = nd(s, req->file, req->type);
  if (!str_is(S(s, rn->text), "HeatSource")) {
    return 0;
  }
  ozni_index section = child_section(s, s->kfile, s->knode, "calibration");
  for (ozni_index ta = rn->first; section != 0 && ta != 0; ta = nd(s, req->file, ta)->next) {
    ozni_index alt = nd(s, req->file, ta)->kind == OZNI_N_TypeArgs ? nd(s, req->file, ta)->first : 0;
    for (ozni_index a = alt != 0 ? nd(s, req->file, alt)->first : 0; a != 0; a = nd(s, req->file, a)->next) {
      for (ozni_index f = nd(s, s->kfile, section)->first; f != 0; f = nd(s, s->kfile, f)->next) {
        if (nd(s, s->kfile, f)->kind == OZNI_N_Field && nd(s, s->kfile, f)->text == nd(s, req->file, a)->text) {
          return nd(s, s->kfile, f)->first;
        }
      }
    }
  }
  return 0;
}

static bool person_avoids(sema *s, ozni_index file, ozni_index person, ozni_index text) {
  for (ozni_index e = nd(s, file, person)->first; e != 0; e = nd(s, file, e)->next) {
    const ozni_node *en = nd(s, file, e);
    if (en->kind != OZNI_N_Entry || !str_is(S(s, en->text), "avoids") || en->first == 0) {
      continue;
    }
    for (ozni_index v = nd(s, file, en->first)->first; v != 0; v = nd(s, file, v)->next) {
      if (nd(s, file, v)->text == text) {
        return true;
      }
    }
  }
  return false;
}

static void put_count(char *buf, size_t *len, uint32_t x) {
  char rev[16];
  size_t n = 0;
  do {
    rev[n++] = (char)('0' + x % 10);
    x /= 10;
  } while (x != 0);
  *len = 0;
  while (n > 0) {
    buf[(*len)++] = rev[--n];
  }
}

static void schedule_text(sema *s, const sched_out *o, ozni_index name) {
  const ozni_sched *p = &o->plan;
  ozni_buf *b = s->out;
  ozni_str nm = S(s, name);
  ozni_buf_puts(b, "schedule ");
  ozni_buf_put(b, nm.p, nm.len);
  ozni_buf_puts(b, " level=");
  ozni_buf_putu(b, (uint64_t)s->level);
  if (s->level >= 1) {
    ozni_buf_puts(b, " cooks=");
    ozni_buf_putu(b, o->cooks);
  }
  ozni_buf_puts(b, "\n");
  uint32_t total = p->ntasks + p->nextras + o->nholds;
  uint32_t *order = ozni_arena_alloc(s->c->arena, (total + 1) * sizeof *order);
  bool *stained = ozni_arena_alloc(s->c->arena, (ozni_table_count(o->units) + 1) * sizeof *stained);
  if (order == nullptr || stained == nullptr) {
    s->c->oom = true;
    return;
  }
  for (ozni_index u = 0; u < ozni_table_count(o->units); u++) {
    stained[u] = false;
  }
  ozni_rat end = ozni_rat_int(0);
  uint32_t nt = p->ntasks, nx = p->ntasks + p->nextras;
  for (uint32_t i = 0; i < total; i++) {
    ozni_rat si = i < nt ? p->tasks[i].start : i < nx ? p->extras[i - nt].start : o->holds[i - nx].start;
    ozni_rat ei = i < nt ? p->tasks[i].end_hi : i < nx ? p->extras[i - nt].end_hi : o->holds[i - nx].end;
    uint32_t ci = i < nt ? 0 : i < nx ? 1 : 2;
    uint32_t j = i;
    while (j > 0) {
      uint32_t a = order[j - 1];
      ozni_rat sa = a < nt ? p->tasks[a].start : a < nx ? p->extras[a - nt].start : o->holds[a - nx].start;
      uint32_t ca = a < nt ? 0 : a < nx ? 1 : 2;
      int r = ozni_rat_cmp(sa, si);
      bool before = r < 0 || (r == 0 && ca < ci) ||
                    (r == 0 && ca == 0 && ci == 0 && p->tasks[a].order < p->tasks[i].order) ||
                    (r == 0 && ca == ci && ca != 0 && a < i);
      if (before) {
        break;
      }
      order[j] = a;
      j--;
    }
    order[j] = i;
    end = i == 0 || ozni_rat_cmp(ei, end) > 0 ? ei : end;
  }
  ozni_table *seen = ozni_table_new(s->c->arena, sizeof(ozni_index));
  if (seen == nullptr) {
    s->c->oom = true;
    return;
  }
  for (uint32_t k = 0; k < total; k++) {
    uint32_t i = order[k];
    if (i < nt && o->merged[i] != UINT32_MAX) {
      continue;
    }
    ozni_buf_puts(b, "  ");
    if (i >= nx) {
      const sched_hold *h = &o->holds[i - nx];
      put_rat(b, h->start);
      ozni_buf_puts(b, "..");
      put_rat(b, h->end);
      ozni_buf_puts(b, " hold ");
      ozni_str it = src_text(s, h->file, nd(s, h->file, h->item)), ul = S(s, unit_at(o, h->unit)->label);
      ozni_buf_put(b, it.p, it.len);
      ozni_buf_puts(b, " [");
      ozni_buf_put(b, ul.p, ul.len);
      ozni_buf_puts(b, "]\n");
      continue;
    }
    if (i >= p->ntasks) {
      static const char *const words[] = {"wash ", "rinse ", "soak ", "wipe ", "run "};
      const ozni_sched_extra *e = &p->extras[i - p->ntasks];
      put_rat(b, e->start);
      ozni_buf_puts(b, "..");
      put_rat(b, e->end_hi);
      ozni_buf_puts(b, " (");
      put_rat(b, e->end_lo);
      ozni_buf_puts(b, ") ");
      ozni_buf_puts(b, words[e->kind]);
      ozni_str ul = S(s, unit_at(o, e->unit)->label), dt = dirt_text(s, o, e->dirt);
      ozni_buf_put(b, ul.p, ul.len);
      ozni_buf_puts(b, " after ");
      ozni_buf_put(b, dt.p, dt.len);
      if (e->cook != 0) {
        ozni_buf_puts(b, " by cook");
        ozni_buf_putu(b, e->cook);
      } else {
        ozni_buf_puts(b, " unattended");
      }
      ozni_buf_puts(b, " [");
      if (e->sink != UINT32_MAX) {
        ozni_str sl = S(s, unit_at(o, e->sink)->label);
        ozni_buf_put(b, sl.p, sl.len);
        ozni_buf_puts(b, ", ");
      }
      ozni_buf_put(b, ul.p, ul.len);
      for (uint32_t q = 0; e->kind == OZNI_EXTRA_RUN && q < e->ntools; q++) {
        ozni_str tl = S(s, unit_at(o, p->tools[e->first_tool + q])->label);
        ozni_buf_puts(b, ", ");
        ozni_buf_put(b, tl.p, tl.len);
      }
      ozni_buf_puts(b, "]\n");
      if (e->task != UINT32_MAX && e->kind != OZNI_EXTRA_SOAK && e->kind != OZNI_EXTRA_WIPE) {
        const ozni_ir_node *served = task_node(s, o, e->task);
        ozni_str args[2] = {ul, dt};
        if (served->src != 0) {
          diag_node(s, OZNI_D_SCH_washInserted, served->file, nd(s, served->file, served->src), args, 2);
        }
      }
      continue;
    }
    const ozni_sched_task *t = &p->tasks[i];
    const ozni_ir_node *node = task_node(s, o, i);
    ozni_str label = S(s, node->label);
    put_rat(b, t->start);
    ozni_buf_puts(b, "..");
    put_rat(b, t->end_hi);
    ozni_buf_puts(b, " (");
    put_rat(b, t->end_lo);
    ozni_buf_puts(b, ") %");
    ozni_buf_putu(b, node->number);
    for (uint32_t m = 0; m < nt; m++) {
      if (o->merged[m] == i) {
        ozni_buf_puts(b, "+%");
        ozni_buf_putu(b, task_node(s, o, m)->number);
      }
    }
    ozni_buf_puts(b, node->kind == OZNI_IR_PLATE ? " plate " : " ");
    ozni_buf_put(b, label.p, label.len);
    const ozni_sched_alt *alt = &p->alts[t->first_alt + t->chosen];
    uint32_t vindex = *(const uint32_t *)ozni_table_get(o->alt_variant, t->first_alt + t->chosen);
    const ozni_ir_variant *var =
        vindex != UINT32_MAX ? ozni_table_get(s->ir.variants, node->first_variant + vindex) : nullptr;
    if (var != nullptr) {
      ozni_str vn = S(s, var->name);
      ozni_buf_puts(b, "/");
      ozni_buf_put(b, vn.p, vn.len);
      if (vindex != 0 && node->src != 0) {
        const ozni_ir_variant *first = ozni_table_get(s->ir.variants, node->first_variant);
        bool first_usable = *(const uint32_t *)ozni_table_get(o->alt_variant, t->first_alt) == 0;
        const char *why = first_usable ? "earlier" : "unavailable";
        ozni_str args[4] = {label, vn, S(s, first->name), {why, (uint32_t)strlen(why)}};
        diag_node(s, OZNI_D_SCH_variantChosen, node->file, nd(s, node->file, node->src), args, 4);
      }
    }
    if (s->level >= 1) {
      if (t->cook != 0) {
        ozni_buf_puts(b, " by cook");
        ozni_buf_putu(b, t->cook);
      } else {
        ozni_buf_puts(b, " unattended");
      }
    }
    for (uint32_t g = 0; g < alt->ngroups; g++) {
      ozni_str ul = S(s, unit_at(o, p->groups[alt->first_group + g].pick)->label);
      ozni_buf_puts(b, g == 0 ? " [" : ", ");
      ozni_buf_put(b, ul.p, ul.len);
    }
    ozni_buf_puts(b, alt->ngroups > 0 ? "]\n" : "\n");
    for (uint32_t m = 0; m < alt->nmarks && node->src != 0; m++) {
      const ozni_sched_mark *mk = &p->marks[alt->first_mark + m];
      uint32_t unit = p->groups[alt->first_group + mk->group].pick;
      if (((const sched_dirt *)ozni_table_get(o->dirts, mk->dirt))->stain && !stained[unit]) {
        stained[unit] = true;
        ozni_str args[2] = {S(s, unit_at(o, unit)->label), dirt_text(s, o, mk->dirt)};
        diag_node(s, OZNI_D_SCH_stainLeft, node->file, nd(s, node->file, node->src), args, 2);
      }
    }
    ozni_index what = node->kind == OZNI_IR_PLATE ? intern(s, "plate", 5) : node->label;
    if (s->entry_dish && (node->provisional || (var != nullptr && var->provisional))) {
      bool dup = false;
      for (ozni_index q = 0; q < ozni_table_count(seen); q++) {
        dup = dup || *(const ozni_index *)ozni_table_get(seen, q) == what;
      }
      if (!dup && node->src != 0) {
        ozni_str arg = S(s, what);
        diag_node(s, OZNI_D_ASM_provisionalValue, node->file, nd(s, node->file, node->src), &arg, 1);
      }
      if (!dup && !ozni_table_push(seen, &what, nullptr)) {
        s->c->oom = true;
      }
    }
  }
  for (uint32_t w = 0; w < o->nwaves; w++) {
    ozni_str wn = S(s, o->wave_names[w]);
    ozni_buf_puts(b, "  serve ");
    ozni_buf_put(b, wn.p, wn.len);
    ozni_buf_puts(b, " at ");
    put_rat(b, o->serves[w]);
    ozni_buf_puts(b, "\n");
  }
  ozni_buf_puts(b, "  total ");
  put_rat(b, end);
  ozni_buf_puts(b, "\n");
}

typedef struct sched_score {
  bool feasible;
  ozni_rat total;
  ozni_rat metric[4];
  uint8_t order[4];
  uint32_t norder;
} sched_score;

static ozni_rat sched_total(const ozni_sched *p) {
  ozni_rat end = ozni_rat_int(0);
  for (uint32_t i = 0; i < p->ntasks; i++) {
    end = later_rat(end, p->tasks[i].end_hi);
  }
  for (uint32_t k = 0; k < p->nextras; k++) {
    end = later_rat(end, p->extras[k].end_hi);
  }
  return end;
}

static ozni_rat sched_start(const ozni_sched *p) {
  ozni_rat start = p->ntasks > 0 ? p->tasks[0].start : ozni_rat_int(0);
  for (uint32_t i = 0; i < p->ntasks; i++) {
    start = ozni_rat_cmp(p->tasks[i].start, start) < 0 ? p->tasks[i].start : start;
  }
  for (uint32_t k = 0; k < p->nextras; k++) {
    start = ozni_rat_cmp(p->extras[k].start, start) < 0 ? p->extras[k].start : start;
  }
  return start;
}

static ozni_rat busy_overlap(sema *s, const ozni_sched *p, uint32_t cook, bool waiting) {
  uint32_t n = p->ntasks + p->nextras;
  ozni_rat *pts = ozni_arena_alloc(s->c->arena, (2 * n + 1) * sizeof *pts);
  ozni_rat sum = ozni_rat_int(0);
  if (pts == nullptr) {
    s->c->oom = true;
    return sum;
  }
  uint32_t np = 0;
  for (uint32_t i = 0; i < n; i++) {
    bool task = i < p->ntasks;
    uint32_t c = task ? p->tasks[i].cook : p->extras[i - p->ntasks].cook;
    if (waiting && c != cook) {
      continue;
    }
    pts[np++] = task ? p->tasks[i].start : p->extras[i - p->ntasks].start;
    pts[np++] = task ? p->tasks[i].end_hi : p->extras[i - p->ntasks].end_hi;
  }
  for (uint32_t i = 1; i < np; i++) {
    for (uint32_t j = i; j > 0 && ozni_rat_cmp(pts[j - 1], pts[j]) > 0; j--) {
      ozni_rat x = pts[j - 1];
      pts[j - 1] = pts[j];
      pts[j] = x;
    }
  }
  for (uint32_t k = 0; k + 1 < np; k++) {
    ozni_rat mid, len;
    if (ozni_rat_add(pts[k], pts[k + 1], &mid) != OZNI_OK || ozni_rat_cmp(pts[k], pts[k + 1]) == 0) {
      continue;
    }
    uint32_t active = 0;
    for (uint32_t i = 0; i < n; i++) {
      bool task = i < p->ntasks;
      uint32_t c = task ? p->tasks[i].cook : p->extras[i - p->ntasks].cook;
      ozni_rat a = task ? p->tasks[i].start : p->extras[i - p->ntasks].start;
      ozni_rat b = task ? p->tasks[i].end_hi : p->extras[i - p->ntasks].end_hi;
      ozni_rat a2, b2;
      if ((waiting && c != cook) || ozni_rat_add(a, a, &a2) != OZNI_OK || ozni_rat_add(b, b, &b2) != OZNI_OK) {
        continue;
      }
      active += ozni_rat_cmp(a2, mid) < 0 && ozni_rat_cmp(mid, b2) < 0 ? 1 : 0;
    }
    if ((waiting ? active == 0 : active >= 2) && ozni_rat_sub(pts[k + 1], pts[k], &len) == OZNI_OK &&
        ozni_rat_add(sum, len, &len) == OZNI_OK) {
      sum = len;
    }
  }
  return sum;
}

static sched_score score_schedule(sema *s, const sched_out *o, const ozni_table *units, const ozni_rat *serves,
                                  uint32_t nwaves, ozni_index prefer, bool enforce) {
  const ozni_sched *p = &o->plan;
  sched_score sc = {.feasible = true, .total = sched_total(p)};
  if (p->backward) {
    ozni_rat start = sched_start(p);
    if (ozni_rat_sub(ozni_rat_int(0), start, &sc.total) != OZNI_OK) {
      sc.total = ozni_rat_int(0);
    }
  }
  for (uint32_t i = 0; i < p->ntasks; i++) {
    const ozni_ir_node *node = task_node(s, o, i);
    const plate_info *pi = node->kind == OZNI_IR_PLATE ? plate_info_for(s, s->ir_base + o->node_of[i] + 1) : nullptr;
    bool limited = pi != nullptr && (pi->timing == TIMING_WITHIN ||
                                     (pi->timing == TIMING_WARM && ozni_rat_cmp(pi->limit, ozni_rat_int(0)) > 0));
    for (uint32_t j = 0; limited && node->wave >= 1 && node->wave <= nwaves && j < node->ninputs; j++) {
      ozni_index src = ((const ozni_ir_input *)ozni_table_get(s->ir.inputs, node->first_input + j))->node;
      uint32_t r = src > s->ir_base ? o->task_of[src - 1 - s->ir_base] : UINT32_MAX;
      ozni_rat wait;
      if (r != UINT32_MAX && ozni_rat_sub(serves[node->wave - 1], p->tasks[r].end_lo, &wait) == OZNI_OK &&
          ozni_rat_cmp(wait, pi->limit) > 0) {
        sc.feasible = false;
      }
    }
  }
  (void)enforce;
  for (ozni_index v = prefer != 0 && nd(s, s->ofile, prefer)->kind == OZNI_N_List ? nd(s, s->ofile, prefer)->first : 0;
       v != 0 && sc.norder < 4; v = nd(s, s->ofile, v)->next) {
    static const char *const names[] = {"fewerWashes", "lessWaiting", "fewerTools", "steadyPace"};
    for (uint8_t k = 0; k < 4; k++) {
      if (str_is(S(s, nd(s, s->ofile, v)->text), names[k])) {
        sc.order[sc.norder++] = k;
      }
    }
  }
  uint32_t washes = 0, tools = 0;
  for (uint32_t k = 0; k < p->nextras; k++) {
    washes += p->extras[k].task != UINT32_MAX && p->extras[k].kind != OZNI_EXTRA_WIPE ? 1 : 0;
  }
  sc.metric[0] = ozni_rat_int(washes);
  ozni_rat idle = ozni_rat_int(0);
  for (uint32_t c = 1; c <= p->cooks; c++) {
    bool any = false;
    ozni_rat first = ozni_rat_int(0), last = ozni_rat_int(0);
    for (uint32_t i = 0; i < p->ntasks + p->nextras; i++) {
      bool task = i < p->ntasks;
      if ((task ? p->tasks[i].cook : p->extras[i - p->ntasks].cook) != c) {
        continue;
      }
      ozni_rat a = task ? p->tasks[i].start : p->extras[i - p->ntasks].start;
      ozni_rat b = task ? p->tasks[i].end_hi : p->extras[i - p->ntasks].end_hi;
      first = !any || ozni_rat_cmp(a, first) < 0 ? a : first;
      last = !any || ozni_rat_cmp(b, last) > 0 ? b : last;
      any = true;
    }
    ozni_rat gap = busy_overlap(s, p, c, true), sum;
    if (any && ozni_rat_cmp(first, last) < 0 && ozni_rat_add(idle, gap, &sum) == OZNI_OK) {
      idle = sum;
    }
  }
  sc.metric[1] = idle;
  for (ozni_index u = 0; u < ozni_table_count(units); u++) {
    bool used = false;
    for (uint32_t i = 0; i < p->ntasks && !used && p->unit_info != nullptr; i++) {
      const ozni_sched_alt *alt = &p->alts[p->tasks[i].first_alt + p->tasks[i].chosen];
      for (uint32_t g = 0; g < alt->ngroups; g++) {
        used = used || p->groups[alt->first_group + g].pick == u;
      }
    }
    tools += used && p->unit_info[u].category == OZNI_UNIT_TOOL ? 1 : 0;
  }
  sc.metric[2] = ozni_rat_int(tools);
  sc.metric[3] = busy_overlap(s, p, 0, false);
  return sc;
}

static bool better_score(const sched_score *a, const sched_score *b) {
  if (a->feasible != b->feasible) {
    return a->feasible;
  }
  int r = ozni_rat_cmp(a->total, b->total);
  if (r != 0) {
    return r < 0;
  }
  for (uint32_t k = 0; k < a->norder; k++) {
    int m = ozni_rat_cmp(a->metric[a->order[k]], b->metric[a->order[k]]);
    if (m != 0) {
      return m < 0;
    }
  }
  return false;
}

static ozni_index root_label(const sema *s, ozni_index irn) {
  for (uint32_t guard = 0; irn != 0 && guard < 1000; guard++) {
    const ozni_ir_node *node = ozni_table_get(s->ir.nodes, irn - 1);
    if ((node->kind != OZNI_IR_OP && node->kind != OZNI_IR_CHOOSE) || node->ninputs == 0) {
      return node->label;
    }
    irn = ((const ozni_ir_input *)ozni_table_get(s->ir.inputs, node->first_input))->node;
  }
  return 0;
}

static void steps_json(sema *s, const sched_out *o, ozni_index name) {
  const ozni_sched *p = &o->plan;
  ozni_buf *b = s->c->steps_json;
  uint32_t nt = p->ntasks, nx = nt + p->nextras, total = nx + o->nholds;
  uint32_t *order = ozni_arena_alloc(s->c->arena, (total + 1) * sizeof *order);
  if (order == nullptr) {
    s->c->oom = true;
    return;
  }
  for (uint32_t i = 0; i < total; i++) {
    ozni_rat si = i < nt ? p->tasks[i].start : i < nx ? p->extras[i - nt].start : o->holds[i - nx].start;
    uint32_t ci = i < nt ? 0 : i < nx ? 1 : 2, j = i;
    while (j > 0) {
      uint32_t a = order[j - 1];
      ozni_rat sa = a < nt ? p->tasks[a].start : a < nx ? p->extras[a - nt].start : o->holds[a - nx].start;
      uint32_t ca = a < nt ? 0 : a < nx ? 1 : 2;
      int r = ozni_rat_cmp(sa, si);
      if (r < 0 || (r == 0 && ca < ci) || (r == 0 && ca == 0 && ci == 0 && p->tasks[a].order < p->tasks[i].order) ||
          (r == 0 && ca == ci && ca != 0 && a < i)) {
        break;
      }
      order[j] = a;
      j--;
    }
    order[j] = i;
  }
  static const char *const extra_kinds[] = {"wash", "rinse", "soak", "wipe", "run"};
  ozni_str nm = S(s, name);
  ozni_buf_puts(b, b->len > 0 ? ",{\"name\":" : "{\"name\":");
  ozni_buf_json(b, nm.p, nm.len);
  ozni_buf_puts(b, ",\"steps\":[");
  bool first = true;
  for (uint32_t k = 0; k < total; k++) {
    uint32_t i = order[k];
    if (i < nt && o->merged[i] != UINT32_MAX) {
      continue;
    }
    ozni_buf_puts(b, first ? "{\"start\":" : ",{\"start\":");
    first = false;
    if (i < nt) {
      const ozni_sched_task *t = &p->tasks[i];
      const ozni_ir_node *node = task_node(s, o, i);
      const ozni_sched_alt *alt = &p->alts[t->first_alt + t->chosen];
      put_json_rat(b, t->start);
      ozni_buf_puts(b, ",\"end\":");
      put_json_rat(b, t->end_hi);
      ozni_buf_puts(b, node->kind == OZNI_IR_PLATE ? ",\"kind\":\"plate\",\"action\":" : ",\"kind\":\"op\",\"action\":");
      ozni_str label = S(s, node->kind == OZNI_IR_PLATE ? intern(s, "plate", 5) : node->label);
      ozni_buf_json(b, label.p, label.len);
      const ir_meta *meta = nullptr;
      for (ozni_index m = 0; s->ir_meta != nullptr && m < ozni_table_count(s->ir_meta); m++) {
        const ir_meta *mm = ozni_table_get(s->ir_meta, m);
        meta = mm->irn == s->ir_base + o->node_of[i] + 1 ? mm : meta;
      }
      ozni_buf_puts(b, ",\"args\":[");
      for (uint32_t a = 0; meta != nullptr && a < meta->nargs; a++) {
        const ir_arg *arg = ozni_table_get(s->ir_args, meta->first_arg + a);
        ozni_str an = S(s, arg->name), av = S(s, arg->value);
        ozni_buf_puts(b, a == 0 ? "{\"name\":" : ",{\"name\":");
        ozni_buf_json(b, an.p, an.len);
        ozni_buf_puts(b, ",\"value\":");
        ozni_buf_json(b, av.p, av.len);
        ozni_buf_puts(b, "}");
      }
      ozni_buf_puts(b, "],\"foods\":[");
      for (uint32_t f = 0; f < node->ninputs; f++) {
        const ozni_ir_input *in = ozni_table_get(s->ir.inputs, node->first_input + f);
        ozni_str fl = S(s, root_label(s, in->node));
        ozni_buf_puts(b, f == 0 ? "" : ",");
        ozni_buf_json(b, fl.p, fl.len);
      }
      ozni_buf_puts(b, "],\"resources\":[");
      for (uint32_t g = 0; g < alt->ngroups; g++) {
        ozni_str ul = S(s, unit_at(o, p->groups[alt->first_group + g].pick)->label);
        ozni_buf_puts(b, g == 0 ? "" : ",");
        ozni_buf_json(b, ul.p, ul.len);
      }
      ozni_buf_puts(b, "],\"reason\":null,\"cook\":");
      if (t->cook != 0) {
        ozni_buf_putu(b, t->cook);
      } else {
        ozni_buf_puts(b, "null");
      }
    } else if (i < nx) {
      const ozni_sched_extra *e = &p->extras[i - nt];
      put_json_rat(b, e->start);
      ozni_buf_puts(b, ",\"end\":");
      put_json_rat(b, e->end_hi);
      ozni_buf_puts(b, ",\"kind\":\"");
      ozni_buf_puts(b, extra_kinds[e->kind]);
      ozni_buf_puts(b, "\",\"action\":\"");
      ozni_buf_puts(b, extra_kinds[e->kind]);
      ozni_buf_puts(b, "\",\"args\":[],\"foods\":[],\"resources\":[");
      if (e->sink != UINT32_MAX) {
        ozni_str sl = S(s, unit_at(o, e->sink)->label);
        ozni_buf_json(b, sl.p, sl.len);
        ozni_buf_puts(b, ",");
      }
      ozni_str ul = S(s, unit_at(o, e->unit)->label);
      ozni_buf_json(b, ul.p, ul.len);
      for (uint32_t q = 0; e->kind == OZNI_EXTRA_RUN && q < e->ntools; q++) {
        ozni_str tl = S(s, unit_at(o, p->tools[e->first_tool + q])->label);
        ozni_buf_puts(b, ",");
        ozni_buf_json(b, tl.p, tl.len);
      }
      ozni_buf_puts(b, "],\"reason\":");
      ozni_str why = dirt_text(s, o, e->dirt);
      ozni_buf_json(b, why.p, why.len);
      ozni_buf_puts(b, ",\"cook\":");
      if (e->cook != 0) {
        ozni_buf_putu(b, e->cook);
      } else {
        ozni_buf_puts(b, "null");
      }
    } else {
      const sched_hold *h = &o->holds[i - nx];
      put_json_rat(b, h->start);
      ozni_buf_puts(b, ",\"end\":");
      put_json_rat(b, h->end);
      ozni_buf_puts(b, ",\"kind\":\"hold\",\"action\":\"hold\",\"args\":[],\"foods\":[");
      ozni_str it = src_text(s, h->file, nd(s, h->file, h->item));
      ozni_buf_json(b, it.p, it.len);
      ozni_buf_puts(b, "],\"resources\":[");
      ozni_str ul = S(s, unit_at(o, h->unit)->label);
      ozni_buf_json(b, ul.p, ul.len);
      ozni_buf_puts(b, "],\"reason\":null,\"cook\":null");
    }
    ozni_buf_puts(b, "}");
  }
  ozni_buf_puts(b, "]}");
}

static void schedule_dish(sema *s, ozni_index name) {
  uint32_t total = ozni_table_count(s->ir.nodes) - s->ir_base;
  ozni_sched_task *tasks = ozni_arena_alloc(s->c->arena, (total + 1) * sizeof *tasks);
  uint32_t *task_of = ozni_arena_alloc(s->c->arena, (total + 1) * sizeof *task_of);
  uint32_t *node_of = ozni_arena_alloc(s->c->arena, (total + 1) * sizeof *node_of);
  ozni_table *deps = ozni_table_new(s->c->arena, sizeof(uint32_t));
  ozni_table *groups = ozni_table_new(s->c->arena, sizeof(ozni_sched_group));
  ozni_table *lists = ozni_table_new(s->c->arena, sizeof(uint32_t));
  ozni_table *units = ozni_table_new(s->c->arena, sizeof(sched_unit));
  ozni_table *alts = ozni_table_new(s->c->arena, sizeof(ozni_sched_alt));
  ozni_table *alt_variant = ozni_table_new(s->c->arena, sizeof(uint32_t));
  ozni_table *marks = ozni_table_new(s->c->arena, sizeof(ozni_sched_mark));
  ozni_table *dirts = ozni_table_new(s->c->arena, sizeof(sched_dirt));
  ozni_table *cases = ozni_table_new(s->c->arena, sizeof(ozni_sched_case));
  ozni_table *targets = ozni_table_new(s->c->arena, sizeof(ozni_index));
  ozni_index *cut_keys = ozni_arena_alloc(s->c->arena, (total + 1) * sizeof *cut_keys);
  uint32_t *merged = ozni_arena_alloc(s->c->arena, (total + 1) * sizeof *merged);
  if (cut_keys == nullptr || merged == nullptr) {
    s->c->oom = true;
    return;
  }
  if (tasks == nullptr || task_of == nullptr || node_of == nullptr || deps == nullptr || groups == nullptr ||
      lists == nullptr || units == nullptr || alts == nullptr || alt_variant == nullptr || marks == nullptr ||
      dirts == nullptr || cases == nullptr || targets == nullptr) {
    s->c->oom = true;
    return;
  }
  for (ozni_index k = 0; s->level == 2 && k < ozni_table_count(s->kentries); k++) {
    const kentry *e = ozni_table_get(s->kentries, k);
    ozni_qty count, burners;
    uint32_t n = entry_field(s, e, "count", &count) && count.v.lo.den == 1 ? (uint32_t)count.v.lo.num : 1;
    if (entry_field(s, e, "burners", &burners) && burners.v.lo.den == 1) {
      n *= (uint32_t)burners.v.lo.num;
    }
    for (uint32_t u = 1; u <= n && u < 100; u++) {
      sched_unit su = {e->name, u, e->kfile, e->knode, unit_label(s, e->name, u)};
      if (!ozni_table_push(units, &su, nullptr)) {
        s->c->oom = true;
      }
    }
  }
  uint32_t cooks = 0;
  ozni_qty q;
  if (s->level >= 1) {
    ozni_index cv = s->onode != 0 ? options_value(s, intern(s, "cooks", 5)) : 0;
    cooks = cv != 0 && value_qty(s, s->ofile, cv, &q) && q.v.lo.den == 1 && q.v.lo.num > 0 ? (uint32_t)q.v.lo.num : 1;
  }
  ozni_index people = s->onode != 0 ? options_value(s, intern(s, "people", 6)) : 0;
  ozni_index cleanup = s->onode != 0 ? options_value(s, intern(s, "cleanup", 7)) : 0;
  ozni_str cleanup_text = cleanup != 0 ? S(s, nd(s, s->ofile, cleanup)->text) : (ozni_str){"", 0};
  uint32_t ntasks = 0, last_number = 0;
  for (uint32_t k = 0; k < total; k++) {
    const ozni_ir_node *node = ozni_table_get(s->ir.nodes, s->ir_base + k);
    task_of[k] = UINT32_MAX;
    last_number = node->number > last_number ? node->number : last_number;
    if (node->kind != OZNI_IR_OP && node->kind != OZNI_IR_CHOOSE && node->kind != OZNI_IR_PLATE) {
      continue;
    }
    const wnode *wn = wnode_for(s, s->ir_base + k + 1);
    ozni_sched_task *t = &tasks[ntasks];
    *t = (ozni_sched_task){.first_dep = ozni_table_count(deps),
                           .first_alt = ozni_table_count(alts),
                           .order = node->number,
                           .food = wn != nullptr ? wn->food : 0,
                           .cleanup = node->kind == OZNI_IR_OP && str_is(S(s, node->label), "store")};
    uint32_t nvar = node->kind == OZNI_IR_CHOOSE ? node->nvariants : 1;
    bool any_usable = false;
    for (uint32_t pass = 0; pass < 2 && !any_usable; pass++) {
      for (uint32_t v = 0; v < nvar; v++) {
        const ozni_ir_variant *var = node->kind == OZNI_IR_CHOOSE
                                         ? ozni_table_get(s->ir.variants, node->first_variant + v)
                                         : nullptr;
        uint32_t first_req = var != nullptr ? var->first_req : node->first_req;
        uint32_t nreqs = var != nullptr ? var->nreqs : node->nreqs;
        bool usable = true;
        for (uint32_t r = 0; pass == 0 && var != nullptr && s->knode != 0 && r < nreqs && usable; r++) {
          const ozni_ir_req *req = ozni_table_get(s->ir.reqs, first_req + r);
          usable = str_is(S(s, req->text), "hands") || req_met(s, req->file, req->type, nullptr, nullptr, 0);
        }
        if (!usable) {
          continue;
        }
        any_usable = true;
        ozni_sched_alt alt = {.lo = var != nullptr ? var->time_lo : node->time_lo,
                              .hi = var != nullptr ? var->time_hi : node->time_hi,
                              .first_group = ozni_table_count(groups)};
        bool unattended = var != nullptr ? var->unattended : node->unattended;
        for (uint32_t r = 0; r < nreqs; r++) {
          const ozni_ir_req *req = ozni_table_get(s->ir.reqs, first_req + r);
          ozni_str text = S(s, req->text);
          if (str_is(text, "hands")) {
            alt.hands = !unattended;
            continue;
          }
          ozni_index cal = s->knode != 0 ? calibration_for(s, req) : 0;
          ozni_qty factor;
          ozni_ival scaled;
          if (cal != 0 && value_qty(s, s->kfile, cal, &factor) &&
              ival_mul((ozni_ival){alt.lo, alt.hi}, factor.v, &scaled)) {
            alt.lo = scaled.lo;
            alt.hi = scaled.hi;
          }
          for (ozni_index pi = people != 0 && nd(s, s->ofile, people)->kind == OZNI_N_List
                                   ? nd(s, s->ofile, people)->first
                                   : 0,
                          who = 0;
               pi != 0; pi = nd(s, s->ofile, pi)->next, who++) {
            if (who < 64 && person_avoids(s, s->ofile, pi, req->text)) {
              alt.banned |= (uint64_t)1 << who;
            }
          }
          if (s->level < 2) {
            continue;
          }
          uint32_t same = 0;
          for (uint32_t r2 = 0; r2 < r; r2++) {
            same += ((const ozni_ir_req *)ozni_table_get(s->ir.reqs, first_req + r2))->text == req->text ? 1 : 0;
          }
          if (node->kind == OZNI_IR_PLATE && str_is(text, "Vessel")) {
            const plate_info *pi = plate_info_for(s, s->ir_base + k + 1);
            for (uint32_t a = 0; pi != nullptr && a < pi->nunits; a++) {
              const plate_unit *pu = ozni_table_get(s->plate_units, pi->first_unit + a);
              for (ozni_index u = 0; u < ozni_table_count(units); u++) {
                const sched_unit *su = ozni_table_get(units, u);
                ozni_sched_group one = {ozni_table_count(lists), 1, 0, 0};
                if (su->entry == pu->entry && su->number == pu->number &&
                    (!ozni_table_push(lists, &u, nullptr) || !ozni_table_push(groups, &one, nullptr))) {
                  s->c->oom = true;
                }
                alt.ngroups += su->entry == pu->entry && su->number == pu->number ? 1 : 0;
              }
            }
            continue;
          }
          ozni_sched_group g = {ozni_table_count(lists), 0, 0, 0};
          for (ozni_index u = 0; u < ozni_table_count(units); u++) {
            const sched_unit *su = ozni_table_get(units, u);
            if (kind_has(s, su->kfile, su->knode, req->file, nd(s, req->file, req->type), 0)) {
              if (!ozni_table_push(lists, &u, nullptr)) {
                s->c->oom = true;
              }
              g.nunits++;
            }
          }
          if (g.nunits > 0 && same >= g.nunits) {
            if (same == g.nunits && node->src != 0) {
              uint32_t needed = same + 1;
              for (uint32_t r2 = r + 1; r2 < nreqs; r2++) {
                needed += ((const ozni_ir_req *)ozni_table_get(s->ir.reqs, first_req + r2))->text == req->text;
              }
              char need_txt[16], have_txt[16];
              size_t nl, hl;
              put_count(need_txt, &nl, needed);
              put_count(have_txt, &hl, g.nunits);
              ozni_str args[3] = {text, {need_txt, (uint32_t)nl}, {have_txt, (uint32_t)hl}};
              diag_node(s, OZNI_D_SCH_capacityShortage, node->file, nd(s, node->file, node->src), args, 3);
            }
            continue;
          }
          if (g.nunits > 0 && !ozni_table_push(groups, &g, nullptr)) {
            s->c->oom = true;
          }
          alt.ngroups += g.nunits > 0 ? 1 : 0;
        }
        alt.first_mark = ozni_table_count(marks);
        for (ozni_index mi = 0; s->wmarks != nullptr && mi < ozni_table_count(s->wmarks); mi++) {
          const wmark *wm = ozni_table_get(s->wmarks, mi);
          if (wm->irn != s->ir_base + k + 1 || (wm->variant != UINT32_MAX && (var == nullptr || wm->variant != v))) {
            continue;
          }
          for (uint32_t gi = 0; gi < alt.ngroups; gi++) {
            const ozni_sched_group *grp = ozni_table_get(groups, alt.first_group + gi);
            const sched_unit *su = ozni_table_get(units, *(const uint32_t *)ozni_table_get(lists, grp->first_unit));
            if (kind_has(s, su->kfile, su->knode, wm->cfile, nd(s, wm->cfile, wm->cnode), 0)) {
              ozni_sched_mark mk = {dirt_id(s, dirts, wm), gi, wm->food};
              if (!ozni_table_push(marks, &mk, nullptr)) {
                s->c->oom = true;
              }
              alt.nmarks++;
              break;
            }
          }
        }
        uint32_t vindex = var != nullptr ? v : UINT32_MAX;
        if (!ozni_table_push(alts, &alt, nullptr) || !ozni_table_push(alt_variant, &vindex, nullptr)) {
          s->c->oom = true;
        }
        t->nalts++;
      }
    }
    for (uint32_t i = 0; i < node->ninputs + node->nafter; i++) {
      ozni_index src = i < node->ninputs
                           ? ((const ozni_ir_input *)ozni_table_get(s->ir.inputs, node->first_input + i))->node
                           : *(const ozni_index *)ozni_table_get(s->ir.afters, node->first_after + i - node->ninputs);
      if (src > s->ir_base && task_of[src - 1 - s->ir_base] != UINT32_MAX) {
        uint32_t d = task_of[src - 1 - s->ir_base];
        if (!ozni_table_push(deps, &d, nullptr)) {
          s->c->oom = true;
        }
        t->ndeps++;
      }
    }
    ozni_index key = t->ndeps == 0 ? cut_key(s, node) : 0;
    merged[ntasks] = UINT32_MAX;
    for (uint32_t i = 0; key != 0 && i < ntasks; i++) {
      if (cut_keys[i] != key || merged[i] != UINT32_MAX) {
        continue;
      }
      ozni_sched_alt *only = ozni_table_at(alts, t->first_alt);
      *only = (ozni_sched_alt){.lo = ozni_rat_int(0), .hi = ozni_rat_int(0), .first_group = ozni_table_count(groups),
                               .first_mark = ozni_table_count(marks)};
      t->nalts = 1;
      if (!ozni_table_push(deps, &i, nullptr)) {
        s->c->oom = true;
      }
      t->ndeps = 1;
      merged[ntasks] = i;
      key = 0;
      break;
    }
    cut_keys[ntasks] = key;
    task_of[k] = ntasks;
    node_of[ntasks++] = k;
  }
  if (s->c->oom) {
    return;
  }
  uint32_t ndirt = ozni_table_count(dirts);
  ozni_sched_dirt *dirt_array = ozni_arena_alloc(s->c->arena, (ndirt + 1) * sizeof *dirt_array);
  if (dirt_array == nullptr) {
    s->c->oom = true;
    return;
  }
  for (uint32_t d = 0; d < ndirt; d++) {
    dirt_cases(s, ozni_table_get(dirts, d), cases, targets, &dirt_array[d]);
  }
  for (uint32_t i = 0; i < ntasks; i++) {
    tasks[i].targets =
        task_targets(s, targets, s->ir_base + node_of[i], wnode_for(s, s->ir_base + node_of[i] + 1), people);
  }
  uint32_t ndep = ozni_table_count(deps), ngroup = ozni_table_count(groups), nlist = ozni_table_count(lists);
  uint32_t nalt = ozni_table_count(alts), nmark = ozni_table_count(marks), ncase = ozni_table_count(cases);
  uint32_t nunit = ozni_table_count(units);
  uint32_t *dep_array = ozni_arena_alloc(s->c->arena, (ndep + 1) * sizeof *dep_array);
  ozni_sched_group *group_array = ozni_arena_alloc(s->c->arena, (ngroup + 1) * sizeof *group_array);
  uint32_t *list_array = ozni_arena_alloc(s->c->arena, (nlist + 1) * sizeof *list_array);
  ozni_sched_alt *alt_array = ozni_arena_alloc(s->c->arena, (nalt + 1) * sizeof *alt_array);
  ozni_sched_mark *mark_array = ozni_arena_alloc(s->c->arena, (nmark + 1) * sizeof *mark_array);
  ozni_sched_case *case_array = ozni_arena_alloc(s->c->arena, (ncase + 1) * sizeof *case_array);
  ozni_sched_unit *unit_array = ozni_arena_alloc(s->c->arena, (nunit + 1) * sizeof *unit_array);
  if (dep_array == nullptr || group_array == nullptr || list_array == nullptr || alt_array == nullptr ||
      mark_array == nullptr || case_array == nullptr || unit_array == nullptr) {
    s->c->oom = true;
    return;
  }
  for (uint32_t k = 0; k < ndep; k++) {
    dep_array[k] = *(const uint32_t *)ozni_table_get(deps, k);
  }
  for (uint32_t k = 0; k < ngroup; k++) {
    group_array[k] = *(const ozni_sched_group *)ozni_table_get(groups, k);
  }
  for (uint32_t k = 0; k < nlist; k++) {
    list_array[k] = *(const uint32_t *)ozni_table_get(lists, k);
  }
  for (uint32_t k = 0; k < nalt; k++) {
    alt_array[k] = *(const ozni_sched_alt *)ozni_table_get(alts, k);
  }
  for (uint32_t k = 0; k < nmark; k++) {
    mark_array[k] = *(const ozni_sched_mark *)ozni_table_get(marks, k);
  }
  for (uint32_t k = 0; k < ncase; k++) {
    case_array[k] = *(const ozni_sched_case *)ozni_table_get(cases, k);
  }
  for (uint32_t k = 0; k < nunit; k++) {
    unit_array[k] = unit_info_for(s, ozni_table_get(units, k));
  }
  ozni_index serve = s->onode != 0 ? options_value(s, intern(s, "serveAt", 7)) : 0;
  ozni_qty soak = {0};
  bool has_soak = std_qty(s, "soakTime", &soak);
  sched_out o = {.plan = {.tasks = tasks,
                          .ntasks = ntasks,
                          .deps = dep_array,
                          .alts = alt_array,
                          .groups = group_array,
                          .units = list_array,
                          .nunits = nunit,
                          .cooks = cooks,
                          .backward = serve != 0 && value_qty(s, s->ofile, serve, &q),
                          .serve_at = serve != 0 ? q.v.lo : ozni_rat_int(0),
                          .marks = mark_array,
                          .cases = case_array,
                          .dirts = s->level == 2 ? dirt_array : nullptr,
                          .unit_info = s->level == 2 ? unit_array : nullptr,
                          .soak_lo = has_soak ? soak.v.lo : ozni_rat_int(0),
                          .soak_hi = has_soak ? soak.v.hi : ozni_rat_int(0),
                          .cleanup = str_is(cleanup_text, "beforeMeal")  ? OZNI_CLEANUP_BEFORE_MEAL
                                     : str_is(cleanup_text, "afterMeal") ? OZNI_CLEANUP_AFTER_MEAL
                                                                         : OZNI_CLEANUP_NONE},
                 .node_of = node_of,
                 .units = units,
                 .alt_variant = alt_variant,
                 .dirts = dirts,
                 .cases = cases,
                 .targets = targets,
                 .cooks = cooks,
                 .last_number = last_number};
  o.merged = merged;
  o.task_of = task_of;
  ozni_rat gaps[64], serves[64], fresh[64], blocks[128];
  ozni_index wave_names[64];
  uint32_t nwaves = 0, nblocks = 0;
  for (uint32_t w = 0; w < 64; w++) {
    gaps[w] = serves[w] = ozni_rat_int(0);
    wave_names[w] = 0;
  }
  for (uint32_t k = 0; k < total; k++) {
    const ozni_ir_node *node = ozni_table_get(s->ir.nodes, s->ir_base + k);
    if (node->kind != OZNI_IR_WAVE || node->wave == 0 || node->wave > 64) {
      continue;
    }
    wave_names[node->wave - 1] = node->label;
    nwaves = node->wave > nwaves ? node->wave : nwaves;
    for (ozni_index w = 0; s->wave_infos != nullptr && w < ozni_table_count(s->wave_infos); w++) {
      const wave_info *wi = ozni_table_get(s->wave_infos, w);
      gaps[node->wave - 1] = wi->irn == s->ir_base + k + 1 ? wi->gap : gaps[node->wave - 1];
    }
  }
  ozni_sched_task *tmpl = ozni_arena_alloc(s->c->arena, (ntasks + 1) * sizeof *tmpl);
  if (tmpl == nullptr) {
    s->c->oom = true;
    return;
  }
  for (uint32_t i = 0; i < ntasks; i++) {
    tmpl[i] = tasks[i];
  }
  for (uint32_t i = 0; i < ntasks; i++) {
    const ozni_ir_node *node = task_node(s, &o, i);
    const plate_info *pi = node->kind == OZNI_IR_PLATE ? plate_info_for(s, s->ir_base + node_of[i] + 1) : nullptr;
    for (uint32_t j = 0; pi != nullptr && pi->timing == TIMING_WARM && pi->on != 0 && j < node->ninputs; j++) {
      ozni_index src = ((const ozni_ir_input *)ozni_table_get(s->ir.inputs, node->first_input + j))->node;
      uint32_t r = src > s->ir_base ? task_of[src - 1 - s->ir_base] : UINT32_MAX;
      for (uint32_t a = 0; r != UINT32_MAX && a < tmpl[r].nalts; a++) {
        ozni_sched_alt *alt = &alt_array[tmpl[r].first_alt + a];
        for (uint32_t g = 0; g < alt->ngroups && g < 64; g++) {
          const sched_unit *su = ozni_table_get(units, list_array[group_array[alt->first_group + g].first_unit]);
          alt->hold_mask |= kind_is(s, su->kfile, su->knode, pi->on, 0) ? (uint64_t)1 << g : 0;
        }
      }
    }
  }
  ozni_index prefer = s->onode != 0 ? options_value(s, intern(s, "prefer", 6)) : 0;
  bool constrained = false;
  for (ozni_index k = 0; s->plate_infos != nullptr && k < ozni_table_count(s->plate_infos); k++) {
    const plate_info *pi = ozni_table_get(s->plate_infos, k);
    constrained = constrained || (pi->irn > s->ir_base && pi->timing != TIMING_NONE);
  }
  ozni_rat *rank = ozni_arena_alloc(s->c->arena, (ntasks + 1) * sizeof *rank);
  ozni_rat *prev_rank = ozni_arena_alloc(s->c->arena, (ntasks + 1) * sizeof *prev_rank);
  if (rank == nullptr || prev_rank == nullptr) {
    s->c->oom = true;
    return;
  }
  sched_score best_score = {0};
  uint32_t best = 0;
  for (uint32_t attempt = 0; attempt <= 6; attempt++) {
    uint32_t pick = attempt == 6 ? best : attempt;
    bool enforce = pick != 5;
    o.plan.tie_reverse = pick == 1 || pick == 3;
    o.plan.reuse_units = pick == 2 || pick == 3;
    o.plan.rank = nullptr;
    if (pick == 4 && (nwaves > 1 || constrained || o.plan.backward)) {
      continue;
    }
    for (uint32_t i = 0; i < ntasks; i++) {
      tasks[i] = tmpl[i];
    }
    nblocks = 0;
    bool known = false;
    bool rank_valid = false, prev_valid = false, reverting = false;
    ozni_rat prev_total = ozni_rat_int(0);
    for (uint32_t round = 0;; round++) {
      o.plan.rank = rank_valid ? rank : nullptr;
      for (uint32_t i = 0; i < ntasks; i++) {
        tasks[i] = tmpl[i];
      }
      known = false;
      nblocks = 0;
    for (uint32_t iter = 0; iter < 8; iter++) {
      for (uint32_t i = 0; i < ntasks; i++) {
        ozni_sched_task keep = tasks[i];
        tasks[i] = tmpl[i];
        tasks[i].release = keep.release;
        tasks[i].has_release = keep.has_release;
        tasks[i].hold_until = keep.hold_until;
        tasks[i].has_hold = keep.has_hold;
      }
      o.plan.blocks = blocks;
      o.plan.nblocks = nblocks;
      o.plan.serves_known = known && nwaves > 0;
      o.plan.first_serve = serves[0];
      o.plan.last_serve = nwaves > 0 ? serves[nwaves - 1] : ozni_rat_int(0);
      if (!ozni_schedule(s->c->arena, &o.plan)) {
        return;
      }
      bool changed = !known;
      for (uint32_t w = 1; w <= nwaves; w++) {
        bool any = false;
        ozni_rat at = ozni_rat_int(0), after;
        for (uint32_t i = 0; i < ntasks; i++) {
          const ozni_ir_node *node = task_node(s, &o, i);
          if (node->kind == OZNI_IR_PLATE && node->wave == w) {
            at = any ? later_rat(at, tasks[i].end_hi) : tasks[i].end_hi;
            any = true;
          }
        }
        if (w > 1 && ozni_rat_add(fresh[w - 2], gaps[w - 1], &after) == OZNI_OK) {
          at = any ? later_rat(at, after) : after;
        }
        if (o.plan.backward && w == nwaves) {
          at = o.plan.serve_at;
        }
        fresh[w - 1] = at;
      }
      if (nwaves > 0 && o.plan.cleanup == OZNI_CLEANUP_BEFORE_MEAL) {
        for (uint32_t e = 0; e < o.plan.nextras; e++) {
          if (o.plan.extras[e].task == UINT32_MAX) {
            fresh[nwaves - 1] = later_rat(fresh[nwaves - 1], o.plan.extras[e].end_hi);
          }
        }
      }
      for (uint32_t w = 0; w < nwaves; w++) {
        changed = changed || ozni_rat_cmp(fresh[w], serves[w]) != 0;
        serves[w] = fresh[w];
      }
      known = true;
      uint32_t nb = 0;
      for (uint32_t w = 0; !o.plan.backward && w + 1 < nwaves && nb < 64; w++) {
        ozni_rat end;
        if (ozni_rat_cmp(gaps[w + 1], ozni_rat_int(0)) > 0 && ozni_rat_add(serves[w], gaps[w + 1], &end) == OZNI_OK) {
          changed = changed || nb >= nblocks || ozni_rat_cmp(blocks[2 * nb], serves[w]) != 0 ||
                    ozni_rat_cmp(blocks[2 * nb + 1], end) != 0;
          blocks[2 * nb] = serves[w];
          blocks[2 * nb + 1] = end;
          nb++;
        }
      }
      changed = changed || nb != nblocks;
      nblocks = nb;
      for (uint32_t i = 0; i < ntasks; i++) {
        const ozni_ir_node *node = task_node(s, &o, i);
        const plate_info *pi = node->kind == OZNI_IR_PLATE ? plate_info_for(s, s->ir_base + node_of[i] + 1) : nullptr;
        if (pi == nullptr || node->wave == 0 || node->wave > nwaves) {
          continue;
        }
        ozni_rat at = serves[node->wave - 1];
        for (uint32_t j = 0; j < node->ninputs; j++) {
          ozni_index src = ((const ozni_ir_input *)ozni_table_get(s->ir.inputs, node->first_input + j))->node;
          uint32_t r = src > s->ir_base ? task_of[src - 1 - s->ir_base] : UINT32_MAX;
          if (r == UINT32_MAX) {
            continue;
          }
          ozni_rat wait, excess, rel;
          bool limited = pi->timing == TIMING_WITHIN ||
                         (pi->timing == TIMING_WARM && ozni_rat_cmp(pi->limit, ozni_rat_int(0)) > 0);
          if (enforce && limited && ozni_rat_sub(at, tasks[r].end_lo, &wait) == OZNI_OK && ozni_rat_cmp(wait, pi->limit) > 0 &&
              ozni_rat_sub(wait, pi->limit, &excess) == OZNI_OK &&
              ozni_rat_add(tasks[r].start, excess, &rel) == OZNI_OK &&
              (!tasks[r].has_release || ozni_rat_cmp(rel, tasks[r].release) > 0)) {
            tasks[r].release = rel;
            tasks[r].has_release = true;
            changed = true;
          }
          if (pi->timing == TIMING_WARM && (!tasks[r].has_hold || ozni_rat_cmp(tasks[r].hold_until, at) != 0)) {
            tasks[r].hold_until = at;
            tasks[r].has_hold = true;
            changed = true;
          }
        }
      }
      if (!changed) {
        break;
      }
    }
      if (pick != 4 || reverting) {
        break;
      }
      ozni_rat now_total = sched_total(&o.plan);
      if (round > 0 && ozni_rat_cmp(now_total, prev_total) >= 0) {
        rank_valid = prev_valid;
        for (uint32_t i = 0; i < ntasks; i++) {
          rank[i] = prev_rank[i];
        }
        reverting = true;
        continue;
      }
      if (round == 8) {
        break;
      }
      prev_total = now_total;
      prev_valid = rank_valid;
      for (uint32_t i = 0; i < ntasks; i++) {
        prev_rank[i] = rank[i];
        rank[i] = tasks[i].end_hi;
      }
      o.plan.rank = rank;
      o.plan.backward = true;
      o.plan.serve_at = now_total;
      o.plan.nblocks = 0;
      for (uint32_t i = 0; i < ntasks; i++) {
        tasks[i] = tmpl[i];
      }
      bool ok = ozni_schedule(s->c->arena, &o.plan);
      o.plan.backward = false;
      o.plan.serve_at = ozni_rat_int(0);
      if (!ok) {
        return;
      }
      for (uint32_t i = 0; i < ntasks; i++) {
        if (ozni_rat_sub(now_total, tasks[i].start, &rank[i]) != OZNI_OK) {
          return;
        }
      }
      rank_valid = true;
    }
    if (attempt == 6) {
      break;
    }
    sched_score sc = score_schedule(s, &o, units, serves, nwaves, prefer, enforce);
    if (attempt == 0 || better_score(&sc, &best_score)) {
      best_score = sc;
      best = pick;
    }
  }
  o.serves = serves;
  o.gaps = gaps;
  o.wave_names = wave_names;
  o.nwaves = nwaves;
  sched_hold *holds = ozni_arena_alloc(s->c->arena, (2 * total + 1) * sizeof *holds);
  if (holds == nullptr) {
    s->c->oom = true;
    return;
  }
  uint32_t nholds = 0;
  ozni_qty long_wait;
  bool has_long = std_qty(s, "longWait", &long_wait);
  for (uint32_t i = 0; i < ntasks; i++) {
    const ozni_ir_node *node = task_node(s, &o, i);
    const plate_info *pi = node->kind == OZNI_IR_PLATE ? plate_info_for(s, s->ir_base + node_of[i] + 1) : nullptr;
    const plate_info *ai = nullptr;
    for (ozni_index a = 0; node->kind == OZNI_IR_OP && s->assemble_infos != nullptr &&
                           a < ozni_table_count(s->assemble_infos);
         a++) {
      const plate_info *info = ozni_table_get(s->assemble_infos, a);
      ai = info->irn == s->ir_base + node_of[i] + 1 ? info : ai;
    }
    if (ai != nullptr) {
      pi = ai;
    } else if (pi == nullptr || node->wave == 0 || node->wave > nwaves) {
      continue;
    }
    ozni_rat at = ai != nullptr ? tasks[i].start : serves[node->wave - 1];
    for (uint32_t j = 0; j < node->ninputs && j < pi->nitems; j++) {
      ozni_index src = ((const ozni_ir_input *)ozni_table_get(s->ir.inputs, node->first_input + j))->node;
      uint32_t r = src > s->ir_base ? task_of[src - 1 - s->ir_base] : UINT32_MAX;
      ozni_rat from = r != UINT32_MAX ? tasks[r].end_hi : ozni_rat_int(0);
      sched_hold h = {from, at, pi->file, pi->items[j], UINT32_MAX, node->src};
      ozni_rat item_limit;
      ozni_index item_on = 0;
      uint8_t item_timing = timing_of(s, pi->file, pi->items[j], &item_limit, &item_on);
      uint8_t timing = item_timing == TIMING_CHILLED || item_timing == TIMING_FROZEN ? item_timing
                       : ai != nullptr                                              ? TIMING_NONE
                                                                                    : pi->timing;
      if (timing == TIMING_WARM && r != UINT32_MAX) {
        const ozni_sched_alt *alt = &alt_array[tasks[r].first_alt + tasks[r].chosen];
        for (uint32_t g = 0; g < alt->ngroups && g < 64; g++) {
          h.unit = (alt->hold_mask >> g & 1) != 0 ? group_array[alt->first_group + g].pick : h.unit;
        }
      } else if (timing == TIMING_CHILLED || timing == TIMING_FROZEN) {
        ozni_index cap = intern(s, timing == TIMING_CHILLED ? "Chilling" : "Freezing", 8);
        for (uint32_t u = nunit; s->level == 2 && u > 0; u--) {
          const sched_unit *su = ozni_table_get(units, u - 1);
          h.unit = kind_is(s, su->kfile, su->knode, cap, 0) ? u - 1 : h.unit;
        }
      } else if (timing == TIMING_NONE && ai == nullptr && r != UINT32_MAX && has_long) {
        ozni_rat wait;
        if (ozni_rat_sub(at, tasks[r].end_lo, &wait) == OZNI_OK && ozni_rat_cmp(wait, long_wait.v.hi) > 0) {
          ozni_buf b = {.a = s->c->allocator};
          put_rat(&b, wait);
          ozni_buf_puts(&b, " s");
          ozni_str args[2] = {src_text(s, pi->file, nd(s, pi->file, pi->items[j])),
                              {b.p != nullptr ? b.p : "", (uint32_t)b.len}};
          if (!b.failed) {
            diag_node(s, OZNI_D_SCH_longWait, pi->file, nd(s, pi->file, pi->items[j]), args, 2);
          }
          ozni_buf_discard(&b);
        }
      }
      if (h.unit != UINT32_MAX) {
        holds[nholds++] = h;
      }
    }
  }
  for (uint32_t u = 0; u < nunit; u++) {
    const sched_unit *su = ozni_table_get(units, u);
    ozni_qty capacity;
    const kentry *entry = nullptr;
    for (ozni_index k = 0; k < ozni_table_count(s->kentries); k++) {
      const kentry *e = ozni_table_get(s->kentries, k);
      entry = e->name == su->entry ? e : entry;
    }
    if (entry == nullptr || !entry_field(s, entry, "capacity", &capacity) || capacity.v.lo.den != 1 ||
        (!kind_is(s, su->kfile, su->knode, intern(s, "Chilling", 8), 0) &&
         !kind_is(s, su->kfile, su->knode, intern(s, "Freezing", 8), 0))) {
      continue;
    }
    for (uint32_t h = 0; h < nholds; h++) {
      uint32_t active = 0;
      for (uint32_t g = 0; g < nholds; g++) {
        active += holds[g].unit == u && ozni_rat_cmp(holds[g].start, holds[h].start) <= 0 &&
                          ozni_rat_cmp(holds[h].start, holds[g].end) < 0
                      ? 1
                      : 0;
      }
      if (holds[h].unit == u && active > (uint64_t)capacity.v.lo.num) {
        char need_txt[16], have_txt[16];
        size_t nl, hl;
        put_count(need_txt, &nl, active);
        put_count(have_txt, &hl, (uint32_t)capacity.v.lo.num);
        ozni_str args[3] = {S(s, nd(s, su->kfile, su->knode)->text), {need_txt, (uint32_t)nl},
                            {have_txt, (uint32_t)hl}};
        diag_node(s, OZNI_D_SCH_capacityShortage, holds[h].file, nd(s, holds[h].file, holds[h].plate), args, 3);
        break;
      }
    }
  }
  o.holds = holds;
  o.nholds = nholds;
  o.plan.unit_info = unit_array;
  o.plan.dirts = dirt_array;
  if (s->level == 0 && s->dish_node != 0) {
    const ozni_node *dish = nd(s, s->dish_file, s->dish_node);
    uint32_t start, end;
    name_span(dish, S(s, dish->text), &start, &end);
    ozni_buf b = {.a = s->c->allocator};
    put_rat(&b, sched_total(&o.plan));
    ozni_buf_puts(&b, " s");
    ozni_str arg = {b.p != nullptr ? b.p : "", (uint32_t)b.len};
    if (!b.failed) {
      diag(s, OZNI_D_SCH_criticalPath, s->dish_file, start, end, &arg, 1);
    }
    ozni_buf_discard(&b);
  }
  if (s->c->steps_json != nullptr) {
    steps_json(s, &o, name);
  }
  if (s->c->set.json) {
    if (s->c->set.api) {
      ozni_str nm = S(s, name);
      ozni_buf_puts(s->out, s->out->len > 0 ? ",{\"name\":" : "{\"name\":");
      ozni_buf_json(s->out, nm.p, nm.len);
      ozni_buf_puts(s->out, ",\"schedule\":");
    }
    schedule_json(s, &o);
    if (s->c->set.api) {
      ozni_buf_puts(s->out, "}");
    }
    return;
  }
  schedule_text(s, &o, name);
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
  s->dish_file = m->file;
  s->dish_node = d->node;
  s->small_seen = ozni_table_new(s->c->arena, sizeof(ozni_index));
  s->picks = ozni_table_new(s->c->arena, sizeof(pick_range));
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
  s->wave_names = ozni_table_new(s->c->arena, sizeof(ozni_index));
  s->wave_no = 0;
  s->ir_count = 0;
  s->join = nullptr;
  s->in_branch = false;
  s->branch_last = 0;
  s->ir_base = ir_on(s) ? ozni_table_count(s->ir.nodes) : 0;
  if (s->wave_names == nullptr) {
    s->c->oom = true;
    return;
  }
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
  if (s->ir_print && s->out != nullptr && (s->c->set.entry == nullptr || s->entry_dish)) {
    ir_dump(s, n->text);
  }
  if (s->c->ir_json != nullptr && ir_on(s) && (s->c->set.entry == nullptr || s->entry_dish)) {
    ir_json(s, n->text);
  }
  if (s->schedule && s->out != nullptr && (s->c->set.entry == nullptr || s->entry_dish)) {
    schedule_dish(s, n->text);
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
  axis_state st = {.axis = axis, .file = afile, .node = anode, .kind = AX_ORDERED};
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

static void load_rules(sema *s) {
  s->wrules = ozni_table_new(s->c->arena, sizeof(wrule));
  if (s->wrules == nullptr) {
    s->c->oom = true;
    return;
  }
  for (int pass = 0; pass < 2; pass++) {
    for (ozni_index mi = 0; mi < ozni_table_count(s->pr->modules); mi++) {
      const ozni_module *m = mod_at(s, mi);
      if (m->is_std != (pass == 0)) {
        continue;
      }
      for (ozni_index d = nd(s, m->file, m->root)->first; d != 0; d = nd(s, m->file, d)->next) {
        const ozni_node *dn = nd(s, m->file, d);
        ozni_index after = dn->kind == OZNI_N_Rule ? dn->first : 0;
        if (after == 0 || nd(s, m->file, after)->kind != OZNI_N_TypeRef) {
          continue;
        }
        wrule r = {m->file, d, nd(s, m->file, after)->text, false};
        for (ozni_index c = dn->first; c != 0; c = nd(s, m->file, c)->next) {
          r.fixed = r.fixed || (nd(s, m->file, c)->kind == OZNI_N_Annotation &&
                                str_is(S(s, nd(s, m->file, c)->text), "fixed"));
        }
        bool done = false;
        for (ozni_index k = 0; k < ozni_table_count(s->wrules) && !done; k++) {
          wrule *old = ozni_table_at(s->wrules, k);
          if (old->after != r.after) {
            continue;
          }
          done = true;
          if (old->fixed) {
            ozni_str arg = S(s, r.after);
            bool allergen = str_is(arg, "Allergen");
            diag_node(s, allergen ? OZNI_D_SAF_allergen : OZNI_D_SAF_unwashedHazard, m->file, nd(s, m->file, after),
                      &arg, 1);
          } else {
            *old = r;
          }
        }
        if (!done && !ozni_table_push(s->wrules, &r, nullptr)) {
          s->c->oom = true;
        }
      }
    }
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

typedef struct {
  ozni_index file, node;
} dish_ref;

static bool dish_listed(ozni_table *refs, ozni_index file, ozni_index node) {
  for (ozni_index i = 0; i < ozni_table_count(refs); i++) {
    const dish_ref *r = ozni_table_get(refs, i);
    if (r->file == file && r->node == node) {
      return true;
    }
  }
  return false;
}

static void dish_makes(sema *s, ozni_table *refs, ozni_index file, ozni_index node, uint32_t depth) {
  if (depth > 256) {
    return;
  }
  for (ozni_index c = nd(s, file, node)->first; c != 0; c = nd(s, file, c)->next) {
    const ozni_node *cn = nd(s, file, c);
    ozni_index call = cn->kind == OZNI_N_Make ? cn->first : 0;
    ozni_index callee = call != 0 && nd(s, file, call)->kind == OZNI_N_Call ? nd(s, file, call)->first : call;
    ozni_index df, dn;
    if (callee != 0 && nd(s, file, callee)->kind == OZNI_N_Name &&
        find_from(s, file, nd(s, file, callee)->text, &df, &dn) &&
        (nd(s, df, dn)->kind == OZNI_N_Recipe || nd(s, df, dn)->kind == OZNI_N_Menu) &&
        !dish_listed(refs, df, dn)) {
      dish_ref r = {df, dn};
      if (!ozni_table_push(refs, &r, nullptr)) {
        s->c->oom = true;
        return;
      }
    }
    dish_makes(s, refs, file, c, depth + 1);
  }
}

static ozni_table *entry_dishes(sema *s) {
  ozni_table *refs = ozni_table_new(s->c->arena, sizeof(dish_ref));
  if (refs == nullptr) {
    s->c->oom = true;
    return nullptr;
  }
  for (ozni_index mi = 0; mi < ozni_table_count(s->pr->modules); mi++) {
    const ozni_module *m = mod_at(s, mi);
    for (ozni_index d = 0; !m->is_std && d < ozni_table_count(m->defs); d++) {
      const ozni_def *e = ozni_table_get(m->defs, d);
      ozni_node_kind k = nd(s, m->file, e->node)->kind;
      if ((k == OZNI_N_Recipe || k == OZNI_N_Menu) && str_is(ozni_intern_get(s->c->names, e->name), s->c->set.entry)) {
        dish_ref r = {m->file, e->node};
        if (!ozni_table_push(refs, &r, nullptr)) {
          s->c->oom = true;
          return nullptr;
        }
      }
    }
  }
  for (ozni_index i = 0; i < ozni_table_count(refs) && !s->c->oom; i++) {
    const dish_ref *r = ozni_table_get(refs, i);
    dish_makes(s, refs, r->file, r->node, 0);
  }
  return refs;
}

bool ozni_sema_run(ozni_project *pr, ozni_sema_out what, ozni_buf *out, ozni_ir *ir) {
  sema s = {.pr = pr,
            .c = pr->c,
            .out = out,
            .steps = what == OZNI_OUT_STEPS,
            .requires = what == OZNI_OUT_REQUIRES,
            .ir_print = what == OZNI_OUT_IR,
            .schedule = what == OZNI_OUT_SCHEDULE,
            .ir_keep = ir};
  s.dims = ozni_table_new(pr->c->arena, sizeof(ozni_index));
  s.units = ozni_table_new(pr->c->arena, sizeof(unit_info));
  if (s.dims == nullptr || s.units == nullptr) {
    return false;
  }
  if (s.ir_print || s.schedule || s.ir_keep != nullptr || pr->c->ir_json != nullptr) {
    s.ir = (ozni_ir){ozni_table_new(pr->c->arena, sizeof(ozni_ir_node)),
                     ozni_table_new(pr->c->arena, sizeof(ozni_ir_input)),
                     ozni_table_new(pr->c->arena, sizeof(ozni_ir_req)),
                     ozni_table_new(pr->c->arena, sizeof(ozni_ir_variant)),
                     ozni_table_new(pr->c->arena, sizeof(ozni_index))};
    if (s.ir.nodes == nullptr || s.ir.inputs == nullptr || s.ir.reqs == nullptr || s.ir.variants == nullptr ||
        s.ir.afters == nullptr) {
      return false;
    }
  }
  build_units(&s);
  check_axes(&s);
  check_definitions(&s);
  setup_configs(&s);
  load_rules(&s);
  if (pr->c->ir_json != nullptr || pr->c->steps_json != nullptr) {
    s.ir_meta = ozni_table_new(pr->c->arena, sizeof(ir_meta));
    s.ir_args = ozni_table_new(pr->c->arena, sizeof(ir_arg));
    s.choices = ozni_table_new(pr->c->arena, sizeof(choice));
    s.choice_opts = ozni_table_new(pr->c->arena, sizeof(choice_opt));
    if (s.ir_meta == nullptr || s.ir_args == nullptr || s.choices == nullptr || s.choice_opts == nullptr) {
      return false;
    }
  }
  s.plate_infos = ozni_table_new(pr->c->arena, sizeof(plate_info));
  s.assemble_infos = ozni_table_new(pr->c->arena, sizeof(plate_info));
  s.plate_units = ozni_table_new(pr->c->arena, sizeof(plate_unit));
  s.wave_infos = ozni_table_new(pr->c->arena, sizeof(wave_info));
  s.vessel_used = ozni_table_new(pr->c->arena, sizeof(uint32_t));
  if (s.plate_infos == nullptr || s.plate_units == nullptr || s.wave_infos == nullptr || s.vessel_used == nullptr ||
      s.assemble_infos == nullptr) {
    return false;
  }
  if (s.schedule) {
    s.wmarks = ozni_table_new(pr->c->arena, sizeof(wmark));
    s.wnodes = ozni_table_new(pr->c->arena, sizeof(wnode));
    if (s.wmarks == nullptr || s.wnodes == nullptr) {
      return false;
    }
  }
  ozni_table *reached = pr->c->set.api && pr->c->set.entry != nullptr ? entry_dishes(&s) : nullptr;
  for (ozni_index mi = 0; mi < ozni_table_count(pr->modules) && !pr->c->oom; mi++) {
    const ozni_module *m = mod_at(&s, mi);
    if (m->is_std) {
      continue;
    }
    for (ozni_index d = 0; d < ozni_table_count(m->defs); d++) {
      const ozni_def *e = ozni_table_get(m->defs, d);
      ozni_node_kind k = nd(&s, m->file, e->node)->kind;
      bool other = reached != nullptr && !dish_listed(reached, m->file, e->node);
      if ((k == OZNI_N_Recipe || k == OZNI_N_Menu) && !other) {
        eval_dish(&s, mi, e);
      }
    }
  }
  if (s.ir_keep != nullptr) {
    *s.ir_keep = s.ir;
  }
  pr->c->level = s.level;
  return !pr->c->oom;
}
