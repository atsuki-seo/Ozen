/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#include <string.h>

#include "resolve.h"
#include "stdembed.h"
#include "text.h"

typedef struct binding {
  ozni_index name;
  ozni_index module;
  ozni_index def;
  ozni_index target;
} binding;

constexpr ozni_index NONE = UINT32_MAX;

typedef struct scope_entry {
  ozni_index name;
} scope_entry;

typedef struct walker {
  ozni_project *pr;
  ozni_index mod;
  ozni_ast *ast;
  ozni_table *scope;
  bool in_action;
  bool in_dish;
  uint32_t depth;
} walker;

static ozni_str str_of(const ozni_ctx *c, ozni_index id) { return ozni_intern_get(c->names, id); }

static bool str_eq(ozni_str s, const char *z) {
  size_t n = strlen(z);
  return s.len == n && memcmp(s.p, z, n) == 0;
}

static ozni_index intern(ozni_ctx *c, const char *s, size_t n) {
  ozni_index id = 0;
  if (!ozni_intern_put(c->names, s, n, &id)) {
    c->oom = true;
  }
  return id;
}

static const ozni_node *node(const ozni_project *pr, ozni_index file, ozni_index i) {
  return ozni_node_get(&pr->asts[file], i);
}

static ozni_module *M(ozni_project *pr, ozni_index i) { return ozni_table_at(pr->modules, i); }
static ozni_package *P(ozni_project *pr, ozni_index i) { return ozni_table_at(pr->packages, i); }

const ozni_module *ozni_project_module(const ozni_project *pr, ozni_index i) { return ozni_table_get(pr->modules, i); }

static void diag(ozni_project *pr, ozni_diag_id id, ozni_index file, uint32_t start, uint32_t end,
                 const ozni_str *args, uint16_t nargs) {
  if (!ozni_diag_add(pr->c, id, (ozni_span){file, start, end}, 0, args, nargs)) {
    pr->c->oom = true;
  }
}

static void diag_node(ozni_project *pr, ozni_diag_id id, ozni_index file, const ozni_node *n, const ozni_str *args,
                      uint16_t nargs) {
  diag(pr, id, file, n->span.start, n->span.end, args, nargs);
}

static void diag_name(ozni_project *pr, ozni_diag_id id, ozni_index file, const ozni_node *n, const ozni_str *args,
                      uint16_t nargs) {
  if (n->name_end > n->name_start) {
    diag(pr, id, file, n->name_start, n->name_end, args, nargs);
  } else {
    diag_node(pr, id, file, n, args, nargs);
  }
}

/* Definitions of a module, looked up by interned name. */

static bool defs_insert(ozni_project *pr, ozni_module *m, ozni_index name, ozni_index def) {
  if (m->slots == nullptr || (uint64_t)ozni_table_count(m->defs) * 2 >= m->cap) {
    uint32_t cap = m->cap == 0 ? 64 : m->cap * 2;
    ozni_index *slots = ozni_arena_alloc(pr->c->arena, cap * sizeof *slots);
    if (slots == nullptr) {
      pr->c->oom = true;
      return false;
    }
    for (uint32_t k = 0; k < cap; k++) {
      slots[k] = NONE;
    }
    for (ozni_index d = 0; d < ozni_table_count(m->defs); d++) {
      if (d == def) {
        continue;
      }
      const ozni_def *e = ozni_table_get(m->defs, d);
      uint32_t k = (e->name * 2654435761u) & (cap - 1);
      while (slots[k] != NONE) {
        k = (k + 1) & (cap - 1);
      }
      slots[k] = d;
    }
    m->slots = slots;
    m->cap = cap;
  }
  uint32_t k = (name * 2654435761u) & (m->cap - 1);
  while (m->slots[k] != NONE) {
    k = (k + 1) & (m->cap - 1);
  }
  m->slots[k] = def;
  return true;
}

static ozni_index defs_find(const ozni_module *m, ozni_index name) {
  if (m->slots == nullptr) {
    return NONE;
  }
  uint32_t k = (name * 2654435761u) & (m->cap - 1);
  while (m->slots[k] != NONE) {
    const ozni_def *e = ozni_table_get(m->defs, m->slots[k]);
    if (e->name == name) {
      return m->slots[k];
    }
    k = (k + 1) & (m->cap - 1);
  }
  return NONE;
}

static const ozni_def *def_at(const ozni_module *m, ozni_index d) { return ozni_table_get(m->defs, d); }

/* Sources, packages and modules */

static bool starts_with(const char *s, const char *prefix) { return ozni_strncmp(s, prefix, strlen(prefix)) == 0; }

static bool admit(ozni_ctx *c, ozni_index file) {
  const ozni_source *s = &c->srcs[file];
  if (file < c->nuser && s->len > c->lim.input_bytes) {
    char a[24], b[24];
    size_t na = 0, nb = 0;
    uint64_t v = s->len, w = c->lim.input_bytes;
    char rev[24];
    do {
      rev[na++] = (char)('0' + v % 10);
      v /= 10;
    } while (v != 0);
    for (size_t i = 0; i < na; i++) {
      a[i] = rev[na - 1 - i];
    }
    do {
      rev[nb++] = (char)('0' + w % 10);
      w /= 10;
    } while (w != 0);
    for (size_t i = 0; i < nb; i++) {
      b[i] = rev[nb - 1 - i];
    }
    ozni_str args[2] = {{a, (uint32_t)na}, {b, (uint32_t)nb}};
    ozni_diag_add(c, OZNI_D_SYN_inputTooLarge, (ozni_span){file, 0, 0}, 0, args, 2);
    return false;
  }
  size_t bad = 0;
  if (!ozni_check_utf8(s->text, s->len, &bad)) {
    ozni_diag_add(c, OZNI_D_SYN_invalidUtf8, (ozni_span){file, (uint32_t)bad, (uint32_t)bad + 1}, 0, nullptr, 0);
    return false;
  }
  return true;
}

typedef struct version {
  uint64_t part[3];
  int n;
} version;

static bool parse_version(ozni_str s, version *v, int min_parts, int max_parts) {
  *v = (version){{0, 0, 0}, 0};
  size_t i = 0;
  while (i < s.len) {
    if (v->n == max_parts || s.p[i] < '0' || s.p[i] > '9') {
      return false;
    }
    uint64_t x = 0;
    while (i < s.len && s.p[i] >= '0' && s.p[i] <= '9') {
      x = x * 10 + (uint64_t)(s.p[i] - '0');
      if (x > 1000000000u) {
        return false;
      }
      i++;
    }
    v->part[v->n++] = x;
    if (i < s.len) {
      if (s.p[i] != '.' || i + 1 == s.len) {
        return false;
      }
      i++;
    }
  }
  return v->n >= min_parts;
}

static int version_cmp(const version *a, const version *b) {
  for (int k = 0; k < 3; k++) {
    if (a->part[k] != b->part[k]) {
      return a->part[k] < b->part[k] ? -1 : 1;
    }
  }
  return 0;
}

static ozni_index string_value(const ozni_project *pr, ozni_index file, ozni_index field) {
  const ozni_node *f = node(pr, file, field);
  const ozni_node *v = f->first != 0 ? node(pr, file, f->first) : nullptr;
  return v != nullptr && v->kind == OZNI_N_String ? v->text : 0;
}

static void load_manifest(ozni_project *pr, ozni_index file, const char *prefix, size_t prefix_len, bool is_std) {
  ozni_ctx *c = pr->c;
  const ozni_node *root = node(pr, file, pr->asts[file].root);
  ozni_index decl = 0;
  for (ozni_index i = root->first; i != 0; i = node(pr, file, i)->next) {
    if (node(pr, file, i)->kind == OZNI_N_Package) {
      decl = i;
      break;
    }
  }
  if (decl == 0) {
    return;
  }
  const ozni_node *d = node(pr, file, decl);
  ozni_package pkg = {d->text, 0, file, decl, prefix, prefix_len, is_std};
  if (!is_std && str_eq(str_of(c, d->text), "std")) {
    ozni_str arg = str_of(c, d->text);
    diag_node(pr, OZNI_D_PKG_stdReserved, file, d, &arg, 1);
  }
  for (ozni_index i = d->first; i != 0; i = node(pr, file, i)->next) {
    const ozni_node *f = node(pr, file, i);
    ozni_str name = str_of(c, f->text);
    if (f->kind == OZNI_N_Field && str_eq(name, "version")) {
      pkg.version = string_value(pr, file, i);
      version v;
      if (pkg.version == 0 || !parse_version(str_of(c, pkg.version), &v, 3, 3)) {
        ozni_str arg = pkg.version != 0 ? str_of(c, pkg.version) : (ozni_str){"", 0};
        diag_node(pr, OZNI_D_PKG_badVersion, file, f, &arg, 1);
        pkg.version = 0;
      }
    } else if (f->kind == OZNI_N_Field && (str_eq(name, "requires") || str_eq(name, "license"))) {
      continue;
    } else {
      diag_node(pr, OZNI_D_PKG_unknownPackageField, file, f, &name, 1);
    }
  }
  if (pkg.version == 0) {
    bool has_field = false;
    for (ozni_index i = d->first; i != 0; i = node(pr, file, i)->next) {
      has_field = has_field || str_eq(str_of(c, node(pr, file, i)->text), "version");
    }
    if (!has_field) {
      ozni_str arg = {"version", 7};
      diag_node(pr, OZNI_D_PKG_missingPackageField, file, d, &arg, 1);
    }
  }
  if (!ozni_table_push(pr->packages, &pkg, nullptr)) {
    c->oom = true;
  }
}

static ozni_index package_of(const ozni_project *pr, const char *path) {
  ozni_index best = NONE;
  size_t best_len = 0;
  for (ozni_index i = 0; i < ozni_table_count(pr->packages); i++) {
    const ozni_package *p = ozni_table_get(pr->packages, i);
    if (ozni_strncmp(path, p->prefix, p->prefix_len) == 0 && (best == NONE || p->prefix_len > best_len)) {
      best = i;
      best_len = p->prefix_len;
    }
  }
  return best;
}

static ozni_index module_name_for(ozni_project *pr, ozni_index file, ozni_index pkg) {
  ozni_ctx *c = pr->c;
  const char *path = c->srcs[file].path;
  size_t skip = 0;
  char buf[1024];
  size_t n = 0;
  if (pkg != NONE) {
    const ozni_package *p = ozni_table_get(pr->packages, pkg);
    skip = p->prefix_len;
    ozni_str pn = str_of(c, p->name);
    if (pn.len + 1 >= sizeof buf) {
      return 0;
    }
    ozni_copy(buf, pn.p, pn.len);
    n = pn.len;
    buf[n++] = '.';
  }
  size_t len = strlen(path);
  if (len >= 5 && ozni_strcmp(path + len - 5, ".ozen") == 0) {
    len -= 5;
  }
  for (size_t i = skip; i < len && n < sizeof buf; i++) {
    buf[n++] = path[i] == '/' ? '.' : path[i];
  }
  return intern(c, buf, n);
}

static ozni_index add_module(ozni_project *pr, ozni_index name, ozni_index file, ozni_index pkg, ozni_index root,
                             bool is_std) {
  ozni_module m = {name, file, pkg, root, is_std, false, false, nullptr, nullptr, nullptr, 0};
  m.defs = ozni_table_new(pr->c->arena, sizeof(ozni_def));
  m.bindings = ozni_table_new(pr->c->arena, sizeof(binding));
  ozni_index i = 0;
  if (m.defs == nullptr || m.bindings == nullptr || !ozni_table_push(pr->modules, &m, &i)) {
    pr->c->oom = true;
    return NONE;
  }
  return i;
}

static bool names_decl(ozni_node_kind k) {
  static const ozni_node_kind kinds[] = {
      OZNI_N_Unit,     OZNI_N_Axis,     OZNI_N_Enum,   OZNI_N_Trait,    OZNI_N_Ingredient, OZNI_N_Kind,
      OZNI_N_Vessel,   OZNI_N_Dirt,     OZNI_N_Resource, OZNI_N_Tool,   OZNI_N_Alias,      OZNI_N_Property,
      OZNI_N_Source,   OZNI_N_Action,   OZNI_N_Recipe, OZNI_N_Menu,     OZNI_N_Kitchen,    OZNI_N_Type,
      OZNI_N_Macro,    OZNI_N_Rule,
  };
  for (size_t i = 0; i < sizeof kinds / sizeof kinds[0]; i++) {
    if (kinds[i] == k) {
      return true;
    }
  }
  return false;
}

static void add_def(ozni_project *pr, ozni_index mi, ozni_index name, ozni_index at, bool local) {
  ozni_module *m = M(pr, mi);
  ozni_index prev = defs_find(m, name);
  if (prev != NONE) {
    ozni_str arg = str_of(pr->c, name);
    diag_node(pr, OZNI_D_NAM_duplicateDefinition, m->file, node(pr, m->file, at), &arg, 1);
    return;
  }
  ozni_def d = {name, at, local};
  ozni_index di = 0;
  if (!ozni_table_push(m->defs, &d, &di) || !defs_insert(pr, M(pr, mi), name, di)) {
    pr->c->oom = true;
  }
}

static void collect_values(ozni_project *pr, ozni_index file, const ozni_node *decl) {
  for (ozni_index v = decl->first; v != 0; v = node(pr, file, v)->next) {
    const ozni_node *vn = node(pr, file, v);
    if (vn->kind == OZNI_N_Value && !ozni_table_push(pr->values, &vn->text, nullptr)) {
      pr->c->oom = true;
    }
  }
}

static void collect_items(ozni_project *pr, ozni_index mi, ozni_index file, ozni_index root);

static void collect_item(ozni_project *pr, ozni_index mi, ozni_index file, ozni_index item, bool local) {
  const ozni_node *n = node(pr, file, item);
  if (n->kind == OZNI_N_Local) {
    if (n->first != 0) {
      collect_item(pr, mi, file, n->first, true);
    }
    return;
  }
  if (n->kind == OZNI_N_Module) {
    ozni_str name = str_of(pr->c, n->text);
    if (name.len >= 3 && memcmp(name.p, "std", 3) == 0 && (name.len == 3 || name.p[3] == '.') &&
        !M(pr, mi)->is_std) {
      diag_node(pr, OZNI_D_PKG_stdReserved, file, n, &name, 1);
      return;
    }
    ozni_index inner = add_module(pr, n->text, file, M(pr, mi)->pkg, item, M(pr, mi)->is_std);
    if (inner != NONE) {
      collect_items(pr, inner, file, item);
    }
    return;
  }
  if (n->kind == OZNI_N_Dimension) {
    for (ozni_index c = n->first; c != 0; c = node(pr, file, c)->next) {
      if (node(pr, file, c)->kind == OZNI_N_Name) {
        add_def(pr, mi, node(pr, file, c)->text, c, local);
      }
    }
    return;
  }
  if (n->kind == OZNI_N_Recipe) {
    M(pr, mi)->has_recipe = true;
  } else if (n->kind == OZNI_N_Menu) {
    M(pr, mi)->has_menu = true;
  }
  if (n->kind == OZNI_N_Axis || n->kind == OZNI_N_Enum) {
    collect_values(pr, file, n);
  }
  if (names_decl(n->kind) && n->text != 0) {
    add_def(pr, mi, n->text, item, local);
  }
}

static void collect_items(ozni_project *pr, ozni_index mi, ozni_index file, ozni_index root) {
  for (ozni_index i = node(pr, file, root)->first; i != 0; i = node(pr, file, i)->next) {
    collect_item(pr, mi, file, i, false);
  }
}

static ozni_index find_module(const ozni_project *pr, ozni_index name) {
  for (ozni_index i = 0; i < ozni_table_count(pr->modules); i++) {
    if (((const ozni_module *)ozni_table_get(pr->modules, i))->name == name) {
      return i;
    }
  }
  return NONE;
}

static ozni_index resolve_module_name(ozni_project *pr, ozni_index from, ozni_str q) {
  ozni_ctx *c = pr->c;
  const ozni_module *m = ozni_table_get(pr->modules, from);
  char buf[1024];
  if (m->pkg != NONE) {
    ozni_str pn = str_of(c, ((const ozni_package *)ozni_table_get(pr->packages, m->pkg))->name);
    if (pn.len + 1 + q.len < sizeof buf) {
      ozni_copy(buf, pn.p, pn.len);
      buf[pn.len] = '.';
      ozni_copy(buf + pn.len + 1, q.p, q.len);
      ozni_index r = find_module(pr, intern(c, buf, pn.len + 1 + q.len));
      if (r != NONE) {
        return r;
      }
    }
  }
  return find_module(pr, intern(c, q.p, q.len));
}

static void add_binding(ozni_project *pr, ozni_index mi, binding b) {
  if (!ozni_table_push(M(pr, mi)->bindings, &b, nullptr)) {
    pr->c->oom = true;
  }
}

static ozni_index last_segment(ozni_ctx *c, ozni_str q) {
  size_t k = q.len;
  while (k > 0 && q.p[k - 1] != '.') {
    k--;
  }
  return intern(c, q.p + k, q.len - k);
}

static void resolve_imports(ozni_project *pr, ozni_index mi) {
  ozni_ctx *c = pr->c;
  ozni_index file = M(pr, mi)->file;
  for (ozni_index i = node(pr, file, M(pr, mi)->root)->first; i != 0; i = node(pr, file, i)->next) {
    const ozni_node *imp = node(pr, file, i);
    if (imp->kind != OZNI_N_Import) {
      continue;
    }
    ozni_str q = str_of(c, imp->text);
    ozni_index target = resolve_module_name(pr, mi, q);
    if (target == NONE) {
      diag_node(pr, OZNI_D_NAM_unknownModule, file, imp, &q, 1);
      continue;
    }
    bool names = false;
    for (ozni_index k = imp->first; k != 0; k = node(pr, file, k)->next) {
      const ozni_node *in = node(pr, file, k);
      if (in->kind == OZNI_N_ImportName) {
        names = true;
        ozni_index d = defs_find(M(pr, target), in->text);
        if (d == NONE) {
          ozni_str arg = str_of(c, in->text);
          diag_node(pr, OZNI_D_NAM_undefined, file, in, &arg, 1);
        } else if (def_at(M(pr, target), d)->local) {
          ozni_str args[2] = {str_of(c, in->text), str_of(c, M(pr, target)->name)};
          diag_node(pr, OZNI_D_NAM_importLocal, file, in, args, 2);
        } else {
          add_binding(pr, mi, (binding){in->text, target, d, i});
        }
      } else if (in->kind == OZNI_N_ImportAlias) {
        names = true;
        add_binding(pr, mi, (binding){in->text, target, NONE, i});
      }
    }
    if (!names) {
      add_binding(pr, mi, (binding){last_segment(c, q), target, NONE, i});
    }
  }
}

static void check_overrides(ozni_project *pr, ozni_index mi) {
  ozni_module *m = M(pr, mi);
  if (m->is_std) {
    return;
  }
  const ozni_module *prelude = pr->prelude != NONE ? ozni_table_get(pr->modules, pr->prelude) : nullptr;
  for (ozni_index d = 0; d < ozni_table_count(m->defs); d++) {
    const ozni_def *e = def_at(m, d);
    ozni_index from = NONE;
    for (ozni_index b = 0; b < ozni_table_count(m->bindings) && from == NONE; b++) {
      const binding *bd = ozni_table_get(m->bindings, b);
      if (bd->name == e->name && bd->def != NONE) {
        from = bd->module;
      }
    }
    if (from == NONE && prelude != nullptr && defs_find(prelude, e->name) != NONE) {
      from = pr->prelude;
    }
    if (from != NONE) {
      ozni_str args[2] = {str_of(pr->c, e->name), str_of(pr->c, M(pr, from)->name)};
      diag_node(pr, OZNI_D_NAM_localOverride, m->file, node(pr, m->file, e->node), args, 2);
    }
  }
}

/* Import graph checks */

static void import_edges(ozni_project *pr, ozni_index mi, ozni_table *out) {
  ozni_module *m = M(pr, mi);
  for (ozni_index b = 0; b < ozni_table_count(m->bindings); b++) {
    const binding *bd = ozni_table_get(m->bindings, b);
    bool dup = false;
    for (ozni_index k = 0; k < ozni_table_count(out); k++) {
      const binding *o = ozni_table_get(out, k);
      dup = dup || o->module == bd->module;
    }
    if (!dup && !ozni_table_push(out, bd, nullptr)) {
      pr->c->oom = true;
    }
  }
}

static int name_cmp(const ozni_project *pr, ozni_index a, ozni_index b) {
  ozni_str x = str_of(pr->c, ((const ozni_module *)ozni_table_get(pr->modules, a))->name);
  ozni_str y = str_of(pr->c, ((const ozni_module *)ozni_table_get(pr->modules, b))->name);
  size_t n = x.len < y.len ? x.len : y.len;
  int r = n == 0 ? 0 : memcmp(x.p, y.p, n);
  return r != 0 ? r : (x.len < y.len ? -1 : x.len > y.len ? 1 : 0);
}

typedef struct cycle_state {
  ozni_project *pr;
  uint8_t *color;
  ozni_index *stack;
  uint32_t depth;
  ozni_table *reported;
} cycle_state;

static void report_cycle(cycle_state *st, uint32_t from) {
  ozni_project *pr = st->pr;
  uint32_t len = st->depth - from;
  uint32_t best = from;
  for (uint32_t k = from; k < st->depth; k++) {
    if (name_cmp(pr, st->stack[k], st->stack[best]) < 0) {
      best = k;
    }
  }
  ozni_index first = st->stack[best];
  for (ozni_index r = 0; r < ozni_table_count(st->reported); r++) {
    if (*(const ozni_index *)ozni_table_get(st->reported, r) == first) {
      return;
    }
  }
  if (!ozni_table_push(st->reported, &first, nullptr)) {
    pr->c->oom = true;
    return;
  }
  ozni_index next = st->stack[from + (best - from + 1) % len];
  ozni_str args[32];
  uint16_t nargs = 0;
  for (uint32_t k = 0; k <= len && nargs < 32; k++) {
    args[nargs++] = str_of(pr->c, M(pr, st->stack[from + (best - from + k) % len])->name);
  }
  ozni_module *m = M(pr, first);
  for (ozni_index b = 0; b < ozni_table_count(m->bindings); b++) {
    const binding *bd = ozni_table_get(m->bindings, b);
    if (bd->module == next) {
      diag_node(pr, OZNI_D_PKG_importCycle, m->file, node(pr, m->file, bd->target), args, nargs);
      return;
    }
  }
}

static void visit(cycle_state *st, ozni_index mi) {
  ozni_project *pr = st->pr;
  st->color[mi] = 1;
  st->stack[st->depth++] = mi;
  ozni_table *edges = ozni_table_new(pr->c->arena, sizeof(binding));
  if (edges == nullptr) {
    pr->c->oom = true;
    return;
  }
  import_edges(pr, mi, edges);
  for (ozni_index k = 0; k < ozni_table_count(edges); k++) {
    ozni_index t = ((const binding *)ozni_table_get(edges, k))->module;
    if (M(pr, t)->is_std) {
      continue;
    }
    if (st->color[t] == 1) {
      for (uint32_t s = 0; s < st->depth; s++) {
        if (st->stack[s] == t) {
          report_cycle(st, s);
        }
      }
    } else if (st->color[t] == 0) {
      visit(st, t);
    }
  }
  st->depth--;
  st->color[mi] = 2;
}

static void check_graph(ozni_project *pr) {
  ozni_index n = ozni_table_count(pr->modules);
  cycle_state st = {pr, ozni_arena_alloc(pr->c->arena, n + 1), ozni_arena_alloc(pr->c->arena, (n + 1) * sizeof(ozni_index)),
                    0, ozni_table_new(pr->c->arena, sizeof(ozni_index))};
  ozni_index *order = ozni_arena_alloc(pr->c->arena, (n + 1) * sizeof(ozni_index));
  if (st.color == nullptr || st.stack == nullptr || st.reported == nullptr || order == nullptr) {
    pr->c->oom = true;
    return;
  }
  for (ozni_index i = 0; i < n; i++) {
    order[i] = i;
    ozni_index j = i;
    while (j > 0 && name_cmp(pr, order[j - 1], i) > 0) {
      order[j] = order[j - 1];
      j--;
    }
    order[j] = i;
  }
  for (ozni_index k = 0; k < n; k++) {
    if (!M(pr, order[k])->is_std && st.color[order[k]] == 0) {
      visit(&st, order[k]);
    }
  }
  for (ozni_index mi = 0; mi < n; mi++) {
    ozni_module *m = M(pr, mi);
    if (!m->has_recipe || m->has_menu) {
      continue;
    }
    for (ozni_index b = 0; b < ozni_table_count(m->bindings); b++) {
      const binding *bd = ozni_table_get(m->bindings, b);
      if (M(pr, bd->module)->has_menu) {
        ozni_str args[2] = {str_of(pr->c, m->name), str_of(pr->c, M(pr, bd->module)->name)};
        diag_node(pr, OZNI_D_PKG_recipeImportsMenu, m->file, node(pr, m->file, bd->target), args, 2);
      }
    }
  }
}

/* Package-level checks */

static ozni_index requires_field(ozni_project *pr, const ozni_package *p) {
  for (ozni_index i = node(pr, p->file, p->decl)->first; i != 0; i = node(pr, p->file, i)->next) {
    const ozni_node *f = node(pr, p->file, i);
    if (f->kind == OZNI_N_Field && str_eq(str_of(pr->c, f->text), "requires") && f->first != 0 &&
        node(pr, p->file, f->first)->kind == OZNI_N_Record) {
      return f->first;
    }
  }
  return 0;
}

static ozni_index package_named(const ozni_project *pr, ozni_index name, bool want_std) {
  for (ozni_index i = 0; i < ozni_table_count(pr->packages); i++) {
    const ozni_package *p = ozni_table_get(pr->packages, i);
    if (p->name == name && p->is_std == want_std && i != pr->root_pkg) {
      return i;
    }
  }
  return NONE;
}

static void check_requires(ozni_project *pr) {
  ozni_ctx *c = pr->c;
  if (pr->root_pkg == NONE) {
    return;
  }
  const ozni_package *root = P(pr, pr->root_pkg);
  ozni_index rec = requires_field(pr, root);
  ozni_index std_name = intern(c, "std", 3);
  for (ozni_index e = rec != 0 ? node(pr, root->file, rec)->first : 0; e != 0; e = node(pr, root->file, e)->next) {
    const ozni_node *en = node(pr, root->file, e);
    const ozni_node *val = en->first != 0 ? node(pr, root->file, en->first) : nullptr;
    if (en->kind != OZNI_N_Entry || val == nullptr || val->kind != OZNI_N_String) {
      continue;
    }
    ozni_str want = str_of(c, val->text);
    ozni_str name = str_of(c, en->text);
    bool range = false;
    for (size_t k = 0; k < want.len; k++) {
      range = range || ozni_strchr("<>=~^* ", want.p[k]) != nullptr;
    }
    if (range) {
      ozni_str args[2] = {name, want};
      diag_node(pr, OZNI_D_PKG_requiresRange, root->file, en, args, 2);
      continue;
    }
    version wv;
    if (!parse_version(want, &wv, 2, 3)) {
      diag_node(pr, OZNI_D_PKG_badVersion, root->file, en, &want, 1);
      continue;
    }
    ozni_index dep = package_named(pr, en->text, en->text == std_name);
    if (dep == NONE || P(pr, dep)->version == 0) {
      continue;
    }
    ozni_str have = str_of(c, P(pr, dep)->version);
    version hv;
    if (!parse_version(have, &hv, 3, 3)) {
      continue;
    }
    ozni_str args[3] = {name, want, have};
    if (wv.part[0] != hv.part[0]) {
      diag_node(pr, OZNI_D_PKG_requiresIncompatible, root->file, en, args, 3);
    } else if (version_cmp(&wv, &hv) > 0) {
      diag_node(pr, OZNI_D_PKG_requiresNewer, root->file, en, args, 3);
    }
  }
  for (ozni_index i = 0; i < ozni_table_count(pr->packages); i++) {
    const ozni_package *p = P(pr, i);
    if (p->is_std || i == pr->root_pkg) {
      continue;
    }
    for (ozni_index j = 0; j < i; j++) {
      const ozni_package *q = P(pr, j);
      if (q->is_std || j == pr->root_pkg || q->name != p->name) {
        continue;
      }
      version a, b;
      bool pa = p->version != 0 && parse_version(str_of(c, p->version), &a, 3, 3);
      bool pb = q->version != 0 && parse_version(str_of(c, q->version), &b, 3, 3);
      ozni_str lo = str_of(c, q->version), hi = str_of(c, p->version);
      if (pa && pb && version_cmp(&a, &b) < 0) {
        lo = str_of(c, p->version);
        hi = str_of(c, q->version);
      }
      ozni_str args[3] = {str_of(c, p->name), lo, hi};
      diag_node(pr, OZNI_D_PKG_twoVersions, p->file, node(pr, p->file, p->decl), args, 3);
      break;
    }
  }
  for (ozni_index i = 0; i < pr->nuser; i++) {
    const char *path = c->srcs[i].path;
    const char *slash = ozni_strchr(path, '/');
    if (slash == nullptr || starts_with(path, "deps/") || package_of(pr, path) != pr->root_pkg) {
      continue;
    }
    ozni_index top = intern(c, path, (size_t)(slash - path));
    ozni_index dep = package_named(pr, top, false);
    if (dep == NONE) {
      continue;
    }
    bool seen = false;
    for (ozni_index k = 0; k < i && !seen; k++) {
      const char *other = c->srcs[k].path;
      seen = ozni_strncmp(other, path, (size_t)(slash - path) + 1) == 0 && package_of(pr, other) == pr->root_pkg;
    }
    if (seen) {
      continue;
    }
    ozni_str arg = str_of(c, top);
    const ozni_node *where = node(pr, P(pr, dep)->file, P(pr, dep)->decl);
    ozni_index file = P(pr, dep)->file;
    for (ozni_index e = rec != 0 ? node(pr, root->file, rec)->first : 0; e != 0; e = node(pr, root->file, e)->next) {
      if (node(pr, root->file, e)->text == top) {
        where = node(pr, root->file, e);
        file = root->file;
      }
    }
    diag_node(pr, OZNI_D_PKG_topFolderClash, file, where, &arg, 1);
  }
}

/* Name references */

typedef enum found_kind : uint8_t { F_NONE, F_LOCAL, F_DEF, F_MODULE, F_OTHER, F_AMBIGUOUS } found_kind;

typedef struct found {
  found_kind kind;
  ozni_index module;
  ozni_index def;
} found;

static bool is_builtin(ozni_str s) {
  static const char *const names[] = {"ceil", "floor", "min", "max", "len", "split", "volume"};
  for (size_t k = 0; k < sizeof names / sizeof names[0]; k++) {
    if (str_eq(s, names[k])) {
      return true;
    }
  }
  return false;
}

static bool scope_has(const walker *w, ozni_index name) {
  for (ozni_index k = 0; k < ozni_table_count(w->scope); k++) {
    if (((const scope_entry *)ozni_table_get(w->scope, k))->name == name) {
      return true;
    }
  }
  return false;
}

static found lookup_global(walker *w, ozni_index name, ozni_index file, const ozni_node *at) {
  ozni_project *pr = w->pr;
  ozni_module *m = M(pr, w->mod);
  if (!m->is_std) {
    if (pr->c->limit_hit) {
      return (found){F_OTHER, NONE, NONE};
    }
    if (++pr->c->steps_used > pr->c->lim.steps) {
      char num[24], rev[24];
      size_t n = 0;
      uint64_t v = pr->c->lim.steps;
      do {
        rev[n++] = (char)('0' + v % 10);
        v /= 10;
      } while (v != 0);
      for (size_t k = 0; k < n; k++) {
        num[k] = rev[n - 1 - k];
      }
      ozni_str arg = {num, (uint32_t)n};
      diag_node(pr, OZNI_D_MAC_fuelExhausted, file, at, &arg, 1);
      pr->c->limit_hit = true;
      return (found){F_OTHER, NONE, NONE};
    }
  }
  ozni_index d = defs_find(m, name);
  if (d != NONE) {
    return (found){F_DEF, w->mod, d};
  }
  found hit = {F_NONE, NONE, NONE};
  ozni_index other_mod = NONE;
  for (ozni_index b = 0; b < ozni_table_count(m->bindings); b++) {
    const binding *bd = ozni_table_get(m->bindings, b);
    if (bd->name != name) {
      continue;
    }
    found f = bd->def == NONE ? (found){F_MODULE, bd->module, NONE} : (found){F_DEF, bd->module, bd->def};
    if (hit.kind == F_NONE) {
      hit = f;
    } else if (hit.module != f.module || hit.def != f.def) {
      other_mod = f.module;
    }
  }
  if (other_mod != NONE) {
    ozni_index a = hit.module, b = other_mod;
    if (name_cmp(pr, a, b) > 0) {
      ozni_index t = a;
      a = b;
      b = t;
    }
    ozni_str args[3] = {str_of(pr->c, name), str_of(pr->c, M(pr, a)->name), str_of(pr->c, M(pr, b)->name)};
    diag_node(pr, OZNI_D_NAM_ambiguous, file, at, args, 3);
    return (found){F_AMBIGUOUS, NONE, NONE};
  }
  if (hit.kind != F_NONE) {
    return hit;
  }
  if (pr->prelude != NONE && w->mod != pr->prelude) {
    d = defs_find(M(pr, pr->prelude), name);
    if (d != NONE) {
      return (found){F_DEF, pr->prelude, d};
    }
  }
  if (is_builtin(str_of(pr->c, name))) {
    return (found){F_OTHER, NONE, NONE};
  }
  for (ozni_index v = 0; v < ozni_table_count(pr->values); v++) {
    if (*(const ozni_index *)ozni_table_get(pr->values, v) == name) {
      return (found){F_OTHER, NONE, NONE};
    }
  }
  return (found){F_NONE, NONE, NONE};
}

static void report_undefined(walker *w, ozni_index name, ozni_index file, const ozni_node *at, bool use_name_span) {
  ozni_str arg = str_of(w->pr->c, name);
  if (use_name_span) {
    diag_name(w->pr, OZNI_D_NAM_undefined, file, at, &arg, 1);
  } else {
    diag_node(w->pr, OZNI_D_NAM_undefined, file, at, &arg, 1);
  }
}

static found resolve_name(walker *w, ozni_index name, ozni_index file, const ozni_node *at) {
  if (scope_has(w, name)) {
    return (found){F_LOCAL, NONE, NONE};
  }
  found f = lookup_global(w, name, file, at);
  if (f.kind == F_NONE) {
    report_undefined(w, name, file, at, false);
  } else if (f.kind == F_DEF && w->in_dish) {
    const ozni_def *d = def_at(M(w->pr, f.module), f.def);
    if (node(w->pr, M(w->pr, f.module)->file, d->node)->kind == OZNI_N_Kitchen) {
      ozni_str arg = str_of(w->pr->c, name);
      diag_node(w->pr, OZNI_D_NAM_kitchenReference, file, at, &arg, 1);
    }
  }
  return f;
}

static void resolve_type_ref(walker *w, ozni_index file, ozni_index i) {
  const ozni_node *t = node(w->pr, file, i);
  ozni_str q = str_of(w->pr->c, t->text);
  const char *dot = ozni_memchr(q.p, '.', q.len);
  if (dot != nullptr) {
    ozni_index head = intern(w->pr->c, q.p, (size_t)(dot - q.p));
    found f = lookup_global(w, head, file, t);
    if (f.kind == F_MODULE) {
      ozni_index rest = intern(w->pr->c, dot + 1, q.len - (size_t)(dot - q.p) - 1);
      ozni_index d = defs_find(M(w->pr, f.module), rest);
      if (d == NONE) {
        report_undefined(w, t->text, file, t, false);
      } else if (def_at(M(w->pr, f.module), d)->local) {
        ozni_str args[2] = {str_of(w->pr->c, rest), str_of(w->pr->c, M(w->pr, f.module)->name)};
        diag_node(w->pr, OZNI_D_NAM_importLocal, file, t, args, 2);
      }
    } else if (f.kind != F_AMBIGUOUS) {
      report_undefined(w, t->text, file, t, false);
    }
    return;
  }
  found f = lookup_global(w, t->text, file, t);
  if (f.kind == F_NONE && !(w->in_action && q.len == 1)) {
    report_undefined(w, t->text, file, t, false);
  }
}

static void resolve_types(walker *w, ozni_index file, ozni_index i) {
  const ozni_node *n = node(w->pr, file, i);
  if (n->kind == OZNI_N_TypeRef) {
    resolve_type_ref(w, file, i);
  } else if (n->kind == OZNI_N_RangedType) {
    if (n->first != 0) {
      resolve_types(w, file, n->first);
    }
  } else if (n->kind == OZNI_N_UnionType || n->kind == OZNI_N_OptionalType || n->kind == OZNI_N_ListType ||
             n->kind == OZNI_N_TupleType || n->kind == OZNI_N_RecordType || n->kind == OZNI_N_TypeField) {
    for (ozni_index c = n->first; c != 0; c = node(w->pr, file, c)->next) {
      if (node(w->pr, file, c)->kind != OZNI_N_Default) {
        resolve_types(w, file, c);
      }
    }
  }
}

static bool push_scope(walker *w, ozni_table **saved) {
  *saved = w->scope;
  ozni_table *copy = ozni_table_new(w->pr->c->arena, sizeof(scope_entry));
  if (copy == nullptr) {
    w->pr->c->oom = true;
    return false;
  }
  for (ozni_index k = 0; k < ozni_table_count(*saved); k++) {
    if (!ozni_table_push(copy, ozni_table_get(*saved, k), nullptr)) {
      w->pr->c->oom = true;
      return false;
    }
  }
  w->scope = copy;
  return true;
}

static void bind(walker *w, ozni_index name, ozni_index file, const ozni_node *at, bool name_span) {
  if (scope_has(w, name)) {
    ozni_str arg = str_of(w->pr->c, name);
    if (name_span) {
      diag_name(w->pr, OZNI_D_NAM_rebind, file, at, &arg, 1);
    } else {
      diag_node(w->pr, OZNI_D_NAM_rebind, file, at, &arg, 1);
    }
    return;
  }
  scope_entry e = {name};
  if (!ozni_table_push(w->scope, &e, nullptr)) {
    w->pr->c->oom = true;
  }
}

static void resolve_expr(walker *w, ozni_index file, ozni_index i);

static void resolve_children(walker *w, ozni_index file, ozni_index i) {
  for (ozni_index c = node(w->pr, file, i)->first; c != 0; c = node(w->pr, file, c)->next) {
    resolve_expr(w, file, c);
  }
}

static void resolve_member(walker *w, ozni_index file, ozni_index i) {
  const ozni_node *n = node(w->pr, file, i);
  const ozni_node *base = n->first != 0 ? node(w->pr, file, n->first) : nullptr;
  if (base == nullptr || base->kind != OZNI_N_Name || scope_has(w, base->text)) {
    resolve_children(w, file, i);
    return;
  }
  found f = lookup_global(w, base->text, file, base);
  if (f.kind == F_MODULE) {
    ozni_index d = defs_find(M(w->pr, f.module), n->text);
    if (d == NONE) {
      report_undefined(w, n->text, file, n, false);
    } else if (def_at(M(w->pr, f.module), d)->local) {
      ozni_str args[2] = {str_of(w->pr->c, n->text), str_of(w->pr->c, M(w->pr, f.module)->name)};
      diag_node(w->pr, OZNI_D_NAM_importLocal, file, n, args, 2);
    }
  } else if (f.kind == F_NONE) {
    report_undefined(w, base->text, file, base, false);
  }
}

static bool enter_depth(walker *w, ozni_index file, const ozni_node *n) {
  ozni_ctx *c = w->pr->c;
  uint32_t limit = M(w->pr, w->mod)->is_std ? ozni_default_limits().depth : c->lim.depth;
  if (c->limit_hit) {
    return false;
  }
  if (++w->depth > limit) {
    char num[24], rev[24];
    size_t len = 0;
    uint32_t v = limit;
    do {
      rev[len++] = (char)('0' + v % 10);
      v /= 10;
    } while (v != 0);
    for (size_t k = 0; k < len; k++) {
      num[k] = rev[len - 1 - k];
    }
    ozni_str arg = {num, (uint32_t)len};
    diag_node(w->pr, OZNI_D_MAC_depthExceeded, file, n, &arg, 1);
    c->limit_hit = true;
    w->depth--;
    return false;
  }
  return true;
}

static void resolve_expr_inner(walker *w, ozni_index file, ozni_index i);

static void resolve_expr(walker *w, ozni_index file, ozni_index i) {
  if (enter_depth(w, file, node(w->pr, file, i))) {
    resolve_expr_inner(w, file, i);
    w->depth--;
  }
}

static void resolve_expr_inner(walker *w, ozni_index file, ozni_index i) {
  const ozni_node *n = node(w->pr, file, i);
  ozni_node_kind k = n->kind;
  if (k == OZNI_N_Name) {
    resolve_name(w, n->text, file, n);
  } else if (k == OZNI_N_Member) {
    resolve_member(w, file, i);
  } else if (k == OZNI_N_TypeRef) {
    resolve_type_ref(w, file, i);
  } else if (k == OZNI_N_Construct) {
    resolve_name(w, n->text, file, n);
    resolve_children(w, file, i);
  } else if (k == OZNI_N_Annotation || k == OZNI_N_Quote || k == OZNI_N_Splice || k == OZNI_N_Quantity ||
             k == OZNI_N_Number || k == OZNI_N_NumberRange || k == OZNI_N_String || k == OZNI_N_MlString ||
             k == OZNI_N_Bool || k == OZNI_N_Vague) {
    return;
  } else {
    resolve_children(w, file, i);
  }
}

static void resolve_block(walker *w, ozni_index file, ozni_index block, bool new_scope);

static void resolve_ingredients(walker *w, ozni_index file, const ozni_node *n) {
  for (ozni_index it = n->first; it != 0; it = node(w->pr, file, it)->next) {
    const ozni_node *item = node(w->pr, file, it);
    bool typed = false;
    for (ozni_index c = item->first; c != 0; c = node(w->pr, file, c)->next) {
      if (node(w->pr, file, c)->kind == OZNI_N_TypeRef) {
        typed = true;
        resolve_type_ref(w, file, c);
      } else {
        resolve_expr(w, file, c);
      }
    }
    ozni_str s = str_of(w->pr->c, item->text);
    char buf[256];
    if (!typed && s.len > 0 && s.len < sizeof buf) {
      ozni_copy(buf, s.p, s.len);
      if (buf[0] >= 'a' && buf[0] <= 'z') {
        buf[0] = (char)(buf[0] - 'a' + 'A');
      }
      ozni_index tname = intern(w->pr->c, buf, s.len);
      if (lookup_global(w, tname, file, item).kind == F_NONE) {
        report_undefined(w, tname, file, item, true);
      }
    }
    bind(w, item->text, file, item, true);
  }
}

static void resolve_serve(walker *w, ozni_index file, const ozni_node *n) {
  for (ozni_index c = n->first; c != 0; c = node(w->pr, file, c)->next) {
    const ozni_node *cn = node(w->pr, file, c);
    if (cn->kind == OZNI_N_After) {
      resolve_children(w, file, c);
      continue;
    }
    if (cn->kind != OZNI_N_Plate) {
      continue;
    }
    for (ozni_index pc = cn->first; pc != 0; pc = node(w->pr, file, pc)->next) {
      const ozni_node *pn = node(w->pr, file, pc);
      if (pn->kind == OZNI_N_On) {
        for (ozni_index t = pn->first; t != 0; t = node(w->pr, file, t)->next) {
          resolve_type_ref(w, file, t);
        }
      } else {
        resolve_expr(w, file, pc);
      }
    }
  }
}

static void resolve_stmt(walker *w, ozni_index file, ozni_index i) {
  const ozni_node *n = node(w->pr, file, i);
  ozni_node_kind k = n->kind;
  if (k == OZNI_N_Let) {
    resolve_children(w, file, i);
    ozni_str name = str_of(w->pr->c, n->text);
    if (name.len > 0 && name.p[0] != '$') {
      bind(w, n->text, file, n, true);
    }
  } else if (k == OZNI_N_LetTuple) {
    for (ozni_index c = n->first; c != 0; c = node(w->pr, file, c)->next) {
      if (node(w->pr, file, c)->kind != OZNI_N_BindName) {
        resolve_expr(w, file, c);
      }
    }
    for (ozni_index c = n->first; c != 0; c = node(w->pr, file, c)->next) {
      if (node(w->pr, file, c)->kind == OZNI_N_BindName) {
        bind(w, node(w->pr, file, c)->text, file, node(w->pr, file, c), false);
      }
    }
  } else if (k == OZNI_N_Parallel) {
    for (ozni_index b = n->first; b != 0; b = node(w->pr, file, b)->next) {
      if (node(w->pr, file, b)->first != 0) {
        resolve_block(w, file, node(w->pr, file, b)->first, false);
      }
    }
  } else if (k == OZNI_N_If || k == OZNI_N_Repeat) {
    for (ozni_index c = n->first; c != 0; c = node(w->pr, file, c)->next) {
      ozni_node_kind ck = node(w->pr, file, c)->kind;
      if (ck == OZNI_N_Block) {
        resolve_block(w, file, c, true);
      } else if (ck == OZNI_N_If) {
        resolve_stmt(w, file, c);
      } else {
        resolve_expr(w, file, c);
      }
    }
  } else if (k == OZNI_N_ForIn) {
    if (n->first == 0) {
      return;
    }
    resolve_expr(w, file, n->first);
    ozni_table *saved;
    if (!push_scope(w, &saved)) {
      return;
    }
    bind(w, n->text, file, n, true);
    ozni_index body = node(w->pr, file, n->first)->next;
    if (body != 0) {
      resolve_block(w, file, body, false);
    }
    w->scope = saved;
  } else if (k == OZNI_N_Ingredients) {
    resolve_ingredients(w, file, n);
  } else if (k == OZNI_N_Serve) {
    resolve_serve(w, file, n);
  } else {
    resolve_expr(w, file, i);
  }
}

static void resolve_block(walker *w, ozni_index file, ozni_index block, bool new_scope) {
  ozni_table *saved = w->scope;
  if (new_scope && !push_scope(w, &saved)) {
    return;
  }
  for (ozni_index s = node(w->pr, file, block)->first; s != 0 && !w->pr->c->oom; s = node(w->pr, file, s)->next) {
    resolve_stmt(w, file, s);
  }
  w->scope = saved;
}

static void resolve_decl_types(walker *w, ozni_index file, ozni_index d) {
  const ozni_node *n = node(w->pr, file, d);
  for (ozni_index c = n->first; c != 0; c = node(w->pr, file, c)->next) {
    const ozni_node *cn = node(w->pr, file, c);
    ozni_node_kind k = cn->kind;
    if (k == OZNI_N_TypeRef) {
      if (n->kind != OZNI_N_Rule || c == n->first) {
        resolve_type_ref(w, file, c);
      }
    } else if (k == OZNI_N_Param || k == OZNI_N_Output || k == OZNI_N_Where || k == OZNI_N_Requires ||
               k == OZNI_N_TypeField) {
      for (ozni_index t = cn->first; t != 0; t = node(w->pr, file, t)->next) {
        bool names_output =
            k == OZNI_N_Output && n->kind == OZNI_N_Recipe && node(w->pr, file, t)->kind == OZNI_N_TypeRef;
        if (node(w->pr, file, t)->kind != OZNI_N_Default && !names_output) {
          resolve_types(w, file, t);
        }
      }
    } else if (k == OZNI_N_Value || k == OZNI_N_Variant) {
      resolve_decl_types(w, file, c);
    } else if (k == OZNI_N_UnionType || k == OZNI_N_OptionalType || k == OZNI_N_ListType || k == OZNI_N_TupleType ||
               k == OZNI_N_RecordType || k == OZNI_N_RangedType) {
      resolve_types(w, file, c);
    } else if (k == OZNI_N_Uses) {
      for (ozni_index t = cn->first; t != 0; t = node(w->pr, file, t)->next) {
        if (node(w->pr, file, t)->kind == OZNI_N_TypeRef) {
          resolve_type_ref(w, file, t);
        }
      }
    } else if (k == OZNI_N_Steps && cn->first != 0) {
      resolve_block(w, file, cn->first, true);
    }
  }
}

static void bind_params(walker *w, ozni_index file, ozni_index d) {
  for (ozni_index c = node(w->pr, file, d)->first; c != 0; c = node(w->pr, file, c)->next) {
    const ozni_node *cn = node(w->pr, file, c);
    if (cn->kind == OZNI_N_Param) {
      bind(w, cn->text, file, cn, true);
    }
  }
}

static void resolve_bodies(ozni_project *pr, ozni_index mi) {
  ozni_module *m = M(pr, mi);
  walker w = {pr, mi, &pr->asts[m->file], nullptr, false, false, 0};
  for (ozni_index d = 0; d < ozni_table_count(m->defs) && !pr->c->oom; d++) {
    ozni_index dn = def_at(m, d)->node;
    const ozni_node *n = node(pr, m->file, dn);
    w.scope = ozni_table_new(pr->c->arena, sizeof(scope_entry));
    if (w.scope == nullptr) {
      pr->c->oom = true;
      return;
    }
    w.in_action = n->kind == OZNI_N_Action;
    w.in_dish = n->kind == OZNI_N_Recipe || n->kind == OZNI_N_Menu;
    if (n->kind == OZNI_N_Macro) {
      continue;
    }
    bind_params(&w, m->file, dn);
    resolve_decl_types(&w, m->file, dn);
    if (w.in_dish) {
      for (ozni_index c = n->first; c != 0; c = node(pr, m->file, c)->next) {
        if (node(pr, m->file, c)->kind == OZNI_N_Block) {
          resolve_block(&w, m->file, c, false);
        }
      }
    }
  }
  for (ozni_index i = node(pr, m->file, m->root)->first; i != 0; i = node(pr, m->file, i)->next) {
    const ozni_node *n = node(pr, m->file, i);
    if (n->kind == OZNI_N_Extend || (n->kind == OZNI_N_Rule && n->text == 0)) {
      w.scope = ozni_table_new(pr->c->arena, sizeof(scope_entry));
      if (w.scope == nullptr) {
        pr->c->oom = true;
        return;
      }
      w.in_action = n->kind == OZNI_N_Extend;
      w.in_dish = false;
      resolve_decl_types(&w, m->file, i);
    }
  }
}

/* Loading */

bool ozni_project_load(ozni_ctx *c, const ozni_source *user, size_t nuser, ozni_project *pr) {
  size_t total = nuser + ozni_std_file_count;
  ozni_source *all = ozni_arena_alloc(c->arena, (total + 1) * sizeof *all);
  *pr = (ozni_project){.c = c, .nuser = nuser, .root_pkg = NONE, .std_pkg = NONE, .prelude = NONE};
  pr->asts = ozni_arena_alloc(c->arena, (total + 1) * sizeof *pr->asts);
  pr->modules = ozni_table_new(c->arena, sizeof(ozni_module));
  pr->packages = ozni_table_new(c->arena, sizeof(ozni_package));
  pr->values = ozni_table_new(c->arena, sizeof(ozni_index));
  if (all == nullptr || pr->asts == nullptr || pr->modules == nullptr || pr->packages == nullptr ||
      pr->values == nullptr) {
    return false;
  }
  for (size_t i = 0; i < nuser; i++) {
    all[i] = user[i];
  }
  for (size_t k = 0; k < ozni_std_file_count; k++) {
    size_t n = strlen(ozni_std_files[k].path);
    char *path = ozni_arena_alloc(c->arena, n + 5);
    if (path == nullptr) {
      return false;
    }
    ozni_copy(path, "std/", 4);
    ozni_copy(path + 4, ozni_std_files[k].path, n + 1);
    all[nuser + k] = (ozni_source){path, ozni_std_files[k].text, ozni_std_files[k].len};
  }
  c->srcs = all;
  c->nsrcs = total;
  c->nuser = nuser;
  bool *parsed = ozni_arena_alloc(c->arena, total + 1);
  if (parsed == nullptr) {
    return false;
  }
  for (ozni_index f = 0; f < total && !c->limit_hit; f++) {
    if (admit(c, f)) {
      if (!ozni_parse(c, f, &pr->asts[f])) {
        return false;
      }
      parsed[f] = true;
    }
  }
  if (ozni_table_count(c->diags) > 0) {
    pr->syntax_errors = true;
    return !c->oom;
  }
  for (ozni_index f = 0; f < total; f++) {
    const char *path = all[f].path;
    if (ozni_strcmp(path, "package.ozen") == 0) {
      pr->root_pkg = ozni_table_count(pr->packages);
      load_manifest(pr, f, "", 0, false);
      if (pr->root_pkg == ozni_table_count(pr->packages)) {
        pr->root_pkg = NONE;
      }
    } else if (ozni_strcmp(path, "std/package.ozen") == 0) {
      pr->std_pkg = ozni_table_count(pr->packages);
      load_manifest(pr, f, "std/", 4, true);
    } else if (starts_with(path, "deps/")) {
      const char *slash = ozni_strchr(path + 5, '/');
      if (slash != nullptr && ozni_strcmp(slash, "/package.ozen") == 0) {
        load_manifest(pr, f, path, (size_t)(slash - path) + 1, false);
      }
    }
  }
  for (ozni_index f = 0; f < total && !c->oom; f++) {
    const char *path = all[f].path;
    size_t len = strlen(path);
    bool manifest = (len >= 12 && ozni_strcmp(path + len - 12, "package.ozen") == 0 &&
                     (len == 12 || path[len - 13] == '/'));
    if (!parsed[f] || manifest) {
      continue;
    }
    ozni_index pkg = package_of(pr, path);
    bool is_std = starts_with(path, "std/");
    ozni_index name = module_name_for(pr, f, pkg);
    ozni_index mi = add_module(pr, name, f, pkg, pr->asts[f].root, is_std);
    if (mi == NONE) {
      break;
    }
    if (is_std && str_eq(str_of(c, name), "std.prelude")) {
      pr->prelude = mi;
    }
    collect_items(pr, mi, f, pr->asts[f].root);
  }
  for (ozni_index mi = 0; mi < ozni_table_count(pr->modules) && !c->oom; mi++) {
    resolve_imports(pr, mi);
  }
  for (ozni_index mi = 0; mi < ozni_table_count(pr->modules) && !c->oom; mi++) {
    check_overrides(pr, mi);
  }
  if (!c->oom) {
    check_graph(pr);
    check_requires(pr);
  }
  for (ozni_index mi = 0; mi < ozni_table_count(pr->modules) && !c->oom; mi++) {
    resolve_bodies(pr, mi);
  }
  return !c->oom;
}

/* Lookups for later phases */

bool ozni_project_find(const ozni_project *pr, ozni_index mod, ozni_index name, ozni_index *out_mod,
                       ozni_index *out_def) {
  const ozni_module *m = ozni_table_get(pr->modules, mod);
  ozni_index d = defs_find(m, name);
  if (d != NONE) {
    *out_mod = mod;
    *out_def = d;
    return true;
  }
  ozni_index hit_mod = NONE, hit_def = NONE;
  for (ozni_index b = 0; b < ozni_table_count(m->bindings); b++) {
    const binding *bd = ozni_table_get(m->bindings, b);
    if (bd->name == name && bd->def != NONE) {
      if (hit_mod != NONE && (hit_mod != bd->module || hit_def != bd->def)) {
        return false;
      }
      hit_mod = bd->module;
      hit_def = bd->def;
    }
  }
  if (hit_mod != NONE) {
    *out_mod = hit_mod;
    *out_def = hit_def;
    return true;
  }
  if (pr->prelude != NONE && mod != pr->prelude) {
    d = defs_find(ozni_table_get(pr->modules, pr->prelude), name);
    if (d != NONE) {
      *out_mod = pr->prelude;
      *out_def = d;
      return true;
    }
  }
  return false;
}

ozni_index ozni_project_find_module(const ozni_project *pr, ozni_index name) { return find_module(pr, name); }

const ozni_node *ozni_project_node(const ozni_project *pr, ozni_index file, ozni_index i) {
  return ozni_node_get(&pr->asts[file], i);
}

/* Debug output */

bool ozni_render_modules(const ozni_project *pr, ozni_buf *b) {
  ozni_index n = ozni_table_count(pr->modules);
  ozni_index *order = ozni_arena_alloc(pr->c->arena, (n + 1) * sizeof *order);
  if (order == nullptr) {
    return false;
  }
  ozni_index k = 0;
  for (ozni_index i = 0; i < n; i++) {
    if (((const ozni_module *)ozni_table_get(pr->modules, i))->is_std) {
      continue;
    }
    ozni_index j = k++;
    while (j > 0 && name_cmp(pr, order[j - 1], i) > 0) {
      order[j] = order[j - 1];
      j--;
    }
    order[j] = i;
  }
  for (ozni_index i = 0; i < k; i++) {
    const ozni_module *m = ozni_table_get(pr->modules, order[i]);
    ozni_str s = str_of(pr->c, m->name);
    ozni_buf_put(b, s.p, s.len);
    ozni_buf_puts(b, " ");
    ozni_buf_puts(b, pr->c->srcs[m->file].path);
    ozni_buf_puts(b, "\n");
  }
  return !b->failed;
}

bool ozni_render_entry(ozni_project *pr, const char *entry, ozni_buf *b, bool *ok) {
  ozni_ctx *c = pr->c;
  *ok = false;
  ozni_index menus[64], recipes[64];
  uint32_t nm = 0, nr = 0;
  for (ozni_index mi = 0; mi < ozni_table_count(pr->modules); mi++) {
    const ozni_module *m = ozni_table_get(pr->modules, mi);
    if (m->is_std) {
      continue;
    }
    for (ozni_index d = 0; d < ozni_table_count(m->defs); d++) {
      const ozni_def *e = ozni_table_get(m->defs, d);
      ozni_node_kind k = node(pr, m->file, e->node)->kind;
      if (entry != nullptr && !str_eq(str_of(c, e->name), entry)) {
        continue;
      }
      if (k == OZNI_N_Menu && nm < 64) {
        menus[nm++] = e->name;
      } else if (k == OZNI_N_Recipe && nr < 64) {
        recipes[nr++] = e->name;
      }
    }
  }
  ozni_index *pick = nm > 0 ? menus : recipes;
  uint32_t np = nm > 0 ? nm : nr;
  if (np == 1) {
    ozni_buf_puts(b, nm > 0 ? "entry menu " : "entry recipe ");
    ozni_str s = str_of(c, pick[0]);
    ozni_buf_put(b, s.p, s.len);
    ozni_buf_puts(b, "\n");
    *ok = true;
    return !b->failed;
  }
  if (np == 0) {
    if (entry != nullptr) {
      ozni_str arg = {entry, (uint32_t)strlen(entry)};
      ozni_diag_add(c, OZNI_D_NAM_noEntry, (ozni_span){0, 0, 0}, 0, &arg, 1);
    } else {
      ozni_diag_add(c, OZNI_D_NAM_noEntry, (ozni_span){0, 0, 0}, 0, nullptr, 0);
    }
    return true;
  }
  ozni_str args[64];
  for (uint32_t k = 0; k < np; k++) {
    args[k] = str_of(c, pick[k]);
  }
  for (uint32_t k = 1; k < np; k++) {
    ozni_str x = args[k];
    uint32_t j = k;
    while (j > 0) {
      ozni_str y = args[j - 1];
      size_t n = x.len < y.len ? x.len : y.len;
      int r = memcmp(y.p, x.p, n);
      if (r < 0 || (r == 0 && y.len <= x.len)) {
        break;
      }
      args[j] = args[j - 1];
      j--;
    }
    args[j] = x;
  }
  ozni_diag_add(c, OZNI_D_NAM_ambiguousEntry, (ozni_span){0, 0, 0}, 0, args, (uint16_t)np);
  return true;
}
