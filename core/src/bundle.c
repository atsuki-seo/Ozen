/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#include <string.h>

#include "render.h"
#include "resolve.h"
#include "sema.h"
#include "text.h"

constexpr ozni_index NONE = UINT32_MAX;

typedef struct item {
  ozni_index mod;
  ozni_index node;
} item;

typedef struct bundler {
  ozni_project *pr;
  ozni_ctx *c;
  bool **marked;
  item *work;
  size_t nwork;
  size_t cap;
  ozni_index *stack;
  size_t stack_cap;
} bundler;

static ozni_str str_of(const ozni_ctx *c, ozni_index id) { return ozni_intern_get(c->names, id); }

static ozni_index intern(ozni_ctx *c, const char *s, size_t n) {
  ozni_index id = 0;
  if (!ozni_intern_put(c->names, s, n, &id)) {
    c->oom = true;
  }
  return id;
}

static const ozni_node *nd(const bundler *b, ozni_index file, ozni_index i) {
  return ozni_project_node(b->pr, file, i);
}

static const ozni_module *mod(const bundler *b, ozni_index mi) { return ozni_project_module(b->pr, mi); }

static bool user_module(const bundler *b, ozni_index mi) { return !mod(b, mi)->is_std; }

static ozni_index top_item(const bundler *b, ozni_index mi, ozni_index at) {
  const ozni_module *m = mod(b, mi);
  const ozni_node *n = nd(b, m->file, at);
  for (ozni_index i = nd(b, m->file, m->root)->first; i != 0; i = nd(b, m->file, i)->next) {
    const ozni_node *t = nd(b, m->file, i);
    if (t->span.start <= n->span.start && n->span.end <= t->span.end) {
      return i;
    }
  }
  return 0;
}

static ozni_node_kind item_kind(const bundler *b, ozni_index file, ozni_index i) {
  const ozni_node *n = nd(b, file, i);
  return n->kind == OZNI_N_Local && n->first != 0 ? nd(b, file, n->first)->kind : n->kind;
}

static void take(bundler *b, ozni_index mi, ozni_index top) {
  ozni_index file = mod(b, mi)->file;
  if (top == 0 || !user_module(b, mi) || b->marked[file][top] || item_kind(b, file, top) == OZNI_N_Kitchen) {
    return;
  }
  b->marked[file][top] = true;
  if (b->nwork < b->cap) {
    b->work[b->nwork++] = (item){mi, top};
  }
}

static void take_name(bundler *b, ozni_index mi, ozni_index name) {
  ozni_index mods[32], defs[32];
  size_t n = ozni_project_candidates(b->pr, mi, name, mods, defs, 32);
  for (size_t k = 0; k < n; k++) {
    const ozni_module *m = mod(b, mods[k]);
    const ozni_def *d = ozni_table_get(m->defs, defs[k]);
    take(b, mods[k], top_item(b, mods[k], d->node));
  }
}

static void scan(bundler *b, item it) {
  ozni_index file = mod(b, it.mod)->file;
  size_t sp = 0;
  b->stack[sp++] = it.node;
  while (sp > 0) {
    const ozni_node *n = nd(b, file, b->stack[--sp]);
    const ozni_index texts[3] = {n->text, n->text2, n->text3};
    for (int k = 0; k < 3; k++) {
      if (texts[k] != 0) {
        take_name(b, it.mod, texts[k]);
      }
    }
    for (ozni_index ch = n->first; ch != 0 && sp < b->stack_cap; ch = nd(b, file, ch)->next) {
      b->stack[sp++] = ch;
    }
  }
}

static bool module_used(const bundler *b, ozni_index mi) {
  const ozni_module *m = mod(b, mi);
  for (ozni_index i = nd(b, m->file, m->root)->first; i != 0; i = nd(b, m->file, i)->next) {
    if (b->marked[m->file][i]) {
      return true;
    }
  }
  return false;
}

static ozni_index last_segment(const bundler *b, ozni_index name) {
  ozni_str s = str_of(b->c, name);
  size_t k = s.len;
  while (k > 0 && s.p[k - 1] != '.') {
    k--;
  }
  return k == 0 ? name : intern(b->c, s.p + k, s.len - k);
}

static void take_extensions(bundler *b) {
  bool changed = true;
  while (changed && !b->c->oom) {
    changed = false;
    for (ozni_index mi = 0; mi < ozni_table_count(b->pr->modules); mi++) {
      const ozni_module *m = mod(b, mi);
      if (!user_module(b, mi) || !module_used(b, mi)) {
        continue;
      }
      for (ozni_index i = nd(b, m->file, m->root)->first; i != 0; i = nd(b, m->file, i)->next) {
        ozni_node_kind k = item_kind(b, m->file, i);
        if (b->marked[m->file][i] || (k != OZNI_N_Extend && k != OZNI_N_Check)) {
          continue;
        }
        bool wanted = k == OZNI_N_Check;
        if (k == OZNI_N_Extend) {
          const ozni_node *e = nd(b, m->file, i);
          e = e->kind == OZNI_N_Local ? nd(b, m->file, e->first) : e;
          ozni_index mods[32], defs[32];
          size_t n = ozni_project_candidates(b->pr, mi, last_segment(b, e->text), mods, defs, 32);
          for (size_t c = 0; c < n && !wanted; c++) {
            const ozni_module *dm = mod(b, mods[c]);
            ozni_index top = top_item(b, mods[c], ((const ozni_def *)ozni_table_get(dm->defs, defs[c]))->node);
            wanted = top != 0 && b->marked[dm->file][top];
          }
        }
        if (wanted) {
          take(b, mi, i);
          changed = true;
        }
      }
      while (b->nwork > 0) {
        scan(b, b->work[--b->nwork]);
      }
    }
  }
}

static ozni_index def_top(const bundler *b, ozni_index mi, ozni_index name) {
  ozni_index mods[32], defs[32];
  size_t n = ozni_project_candidates(b->pr, mi, name, mods, defs, 32);
  if (n == 0 || mods[0] != mi) {
    return 0;
  }
  const ozni_def *d = ozni_table_get(mod(b, mi)->defs, defs[0]);
  return top_item(b, mi, d->node);
}

static void put_str(ozni_buf *out, ozni_str s) { ozni_buf_put(out, s.p, s.len); }

static void put_imports(bundler *b, ozni_buf *out, ozni_index mi, const bool *used) {
  const ozni_module *m = mod(b, mi);
  for (ozni_index i = nd(b, m->file, m->root)->first; i != 0; i = nd(b, m->file, i)->next) {
    const ozni_node *imp = nd(b, m->file, i);
    if (imp->kind != OZNI_N_Import) {
      continue;
    }
    ozni_index target = ozni_project_import_target(b->pr, mi, i);
    if (target == NONE || !(mod(b, target)->is_std || used[target])) {
      continue;
    }
    bool names = false, any = false, alias = false;
    for (ozni_index k = imp->first; k != 0; k = nd(b, m->file, k)->next) {
      const ozni_node *in = nd(b, m->file, k);
      names = names || in->kind == OZNI_N_ImportName;
      alias = alias || in->kind == OZNI_N_ImportAlias;
      if (in->kind == OZNI_N_ImportName) {
        ozni_index top = mod(b, target)->is_std ? 1 : def_top(b, target, in->text);
        any = any || (top != 0 && (mod(b, target)->is_std || b->marked[mod(b, target)->file][top]));
      }
    }
    if (names && !any) {
      continue;
    }
    ozni_buf_puts(out, "import ");
    put_str(out, str_of(b->c, imp->text));
    if (names) {
      ozni_buf_puts(out, ".{");
      bool first = true;
      for (ozni_index k = imp->first; k != 0; k = nd(b, m->file, k)->next) {
        const ozni_node *in = nd(b, m->file, k);
        if (in->kind != OZNI_N_ImportName) {
          continue;
        }
        ozni_index top = mod(b, target)->is_std ? 1 : def_top(b, target, in->text);
        if (top == 0 || (!mod(b, target)->is_std && !b->marked[mod(b, target)->file][top])) {
          continue;
        }
        ozni_buf_puts(out, first ? "" : ", ");
        put_str(out, str_of(b->c, in->text));
        first = false;
      }
      ozni_buf_puts(out, "}");
    }
    for (ozni_index k = imp->first; alias && k != 0; k = nd(b, m->file, k)->next) {
      const ozni_node *in = nd(b, m->file, k);
      if (in->kind == OZNI_N_ImportAlias) {
        ozni_buf_puts(out, " as ");
        put_str(out, str_of(b->c, in->text));
      }
    }
    ozni_buf_puts(out, "\n");
  }
}

static uint32_t leading_comments(const char *t, uint32_t start) {
  uint32_t ls = start;
  while (ls > 0 && t[ls - 1] != '\n') {
    ls--;
  }
  for (uint32_t k = ls; k < start; k++) {
    if (t[k] != ' ' && t[k] != '\t') {
      return start;
    }
  }
  while (ls > 0) {
    uint32_t pe = ls - 1, pls = pe;
    while (pls > 0 && t[pls - 1] != '\n') {
      pls--;
    }
    uint32_t k = pls;
    while (k < pe && (t[k] == ' ' || t[k] == '\t')) {
      k++;
    }
    if (pe - k < 2 || t[k] != '/' || t[k + 1] != '/') {
      break;
    }
    ls = pls;
  }
  return ls;
}

static void put_items(bundler *b, ozni_buf *out, ozni_index mi) {
  const ozni_module *m = mod(b, mi);
  const char *t = b->c->srcs[m->file].text;
  bool first = true;
  for (ozni_index i = nd(b, m->file, m->root)->first; i != 0; i = nd(b, m->file, i)->next) {
    if (!b->marked[m->file][i]) {
      continue;
    }
    const ozni_node *n = nd(b, m->file, i);
    uint32_t from = leading_comments(t, n->span.start);
    ozni_buf_puts(out, first ? "" : "\n");
    ozni_buf_put(out, t + from, n->span.end - from);
    ozni_buf_puts(out, "\n");
    first = false;
  }
}

static int str_order(ozni_str a, ozni_str x) {
  size_t n = a.len < x.len ? a.len : x.len;
  int r = memcmp(a.p, x.p, n);
  return r != 0 ? r : (a.len < x.len ? -1 : a.len > x.len ? 1 : 0);
}

static void sort_by_name(const bundler *b, ozni_index *ids, size_t n, bool packages) {
  for (size_t k = 1; k < n; k++) {
    ozni_index x = ids[k];
    size_t j = k;
    while (j > 0) {
      ozni_index y = ids[j - 1];
      ozni_index xn = packages ? ((const ozni_package *)ozni_table_get(b->pr->packages, x))->name : mod(b, x)->name;
      ozni_index yn = packages ? ((const ozni_package *)ozni_table_get(b->pr->packages, y))->name : mod(b, y)->name;
      if (str_order(str_of(b->c, yn), str_of(b->c, xn)) <= 0) {
        break;
      }
      ids[j] = y;
      j--;
    }
    ids[j] = x;
  }
}

static ozni_index field_string(const bundler *b, const ozni_package *p, const char *field) {
  for (ozni_index i = nd(b, p->file, p->decl)->first; i != 0; i = nd(b, p->file, i)->next) {
    const ozni_node *f = nd(b, p->file, i);
    ozni_str name = str_of(b->c, f->text);
    if (f->kind == OZNI_N_Field && name.len == strlen(field) && memcmp(name.p, field, name.len) == 0 &&
        f->first != 0 && nd(b, p->file, f->first)->kind == OZNI_N_String) {
      return nd(b, p->file, f->first)->text;
    }
  }
  return 0;
}

static void put_from(const bundler *b, ozni_buf *out, ozni_index pi) {
  const ozni_package *p = ozni_table_get(b->pr->packages, pi);
  ozni_buf_puts(out, "  from: ");
  put_str(out, str_of(b->c, p->name));
  ozni_buf_puts(out, " ");
  ozni_str v = p->version != 0 ? str_of(b->c, p->version) : (ozni_str){"", 0};
  ozni_buf_quoted(out, v.p, v.len);
  ozni_buf_puts(out, "\n");
}

static ozni_index phrase_package(const bundler *b, const char *path) {
  if (ozni_strncmp(path, "phrases/", 8) == 0) {
    return b->pr->root_pkg;
  }
  for (ozni_index i = 0; i < ozni_table_count(b->pr->packages); i++) {
    const ozni_package *p = ozni_table_get(b->pr->packages, i);
    if (!p->is_std && !p->bundle && p->prefix_len > 0 && ozni_strncmp(path, p->prefix, p->prefix_len) == 0 &&
        ozni_strncmp(path + p->prefix_len, "phrases/", 8) == 0) {
      return i;
    }
  }
  return NONE;
}

static ozni_str phrase_lang(const char *path) {
  const char *base = path;
  for (const char *s = path; *s != '\0'; s++) {
    base = *s == '/' ? s + 1 : base;
  }
  size_t n = strlen(base);
  return (ozni_str){base, (uint32_t)(n > 4 ? n - 4 : n)};
}

static void put_phrases(const bundler *b, ozni_buf *out, const ozni_source *ph, size_t nph, const ozni_index *pkgs,
                        size_t npkg) {
  ozni_str langs[16];
  size_t nl = 0;
  for (size_t i = 0; i < nph; i++) {
    ozni_index pi = phrase_package(b, ph[i].path);
    bool wanted = false;
    for (size_t k = 0; k < npkg; k++) {
      wanted = wanted || pkgs[k] == pi;
    }
    ozni_str l = phrase_lang(ph[i].path);
    bool seen = false;
    for (size_t k = 0; k < nl; k++) {
      seen = seen || str_order(langs[k], l) == 0;
    }
    if (wanted && !seen && nl < 16) {
      size_t j = nl++;
      while (j > 0 && str_order(langs[j - 1], l) > 0) {
        langs[j] = langs[j - 1];
        j--;
      }
      langs[j] = l;
    }
  }
  for (size_t li = 0; li < nl; li++) {
    ozni_buf_puts(out, "\nphrases ");
    put_str(out, langs[li]);
    ozni_buf_puts(out, " \"\"\"\n");
    for (size_t k = 0; k < npkg; k++) {
      for (size_t i = 0; i < nph; i++) {
        if (phrase_package(b, ph[i].path) != pkgs[k] || str_order(phrase_lang(ph[i].path), langs[li]) != 0) {
          continue;
        }
        ozni_buf_puts(out, "### ");
        put_str(out, str_of(b->c, ((const ozni_package *)ozni_table_get(b->pr->packages, pkgs[k]))->name));
        ozni_buf_puts(out, "\n");
        ozni_buf_put(out, ph[i].text, ph[i].len);
        if (ph[i].len > 0 && ph[i].text[ph[i].len - 1] != '\n') {
          ozni_buf_puts(out, "\n");
        }
      }
    }
    ozni_buf_puts(out, "\"\"\"\n");
  }
}

static bool entry_name(ozni_project *pr, const ozni_allocator *a, ozni_index *out) {
  ozni_buf tmp = {.a = a};
  bool ok = false;
  bool done = ozni_render_entry(pr, pr->c->set.entry, &tmp, &ok);
  if (!done || !ok || tmp.p == nullptr) {
    ozni_buf_discard(&tmp);
    return false;
  }
  const char *s = tmp.p, *sp = ozni_memchr(s + 6, ' ', tmp.len - 6);
  size_t start = sp != nullptr ? (size_t)(sp - s) + 1 : 0, end = tmp.len;
  while (end > start && s[end - 1] == '\n') {
    end--;
  }
  *out = intern(pr->c, s + start, end - start);
  ozni_buf_discard(&tmp);
  return *out != 0;
}

static bool any_error(const ozni_ctx *c) {
  for (ozni_index i = 0; i < ozni_table_count(c->diags); i++) {
    const ozni_diag *d = ozni_table_get(c->diags, i);
    if (d->severity == OZNI_SEV_ERROR && !d->hidden) {
      return true;
    }
  }
  return false;
}

static bool build(bundler *b, ozni_index entry, ozni_buf *out, const ozni_source *ph, size_t nph) {
  ozni_project *pr = b->pr;
  ozni_ctx *c = b->c;
  size_t nmod = ozni_table_count(pr->modules), total = 1, biggest = 1;
  b->marked = ozni_arena_alloc(c->arena, (c->nsrcs + 1) * sizeof *b->marked);
  bool *used = ozni_arena_alloc(c->arena, nmod + 1);
  if (b->marked == nullptr || used == nullptr) {
    return false;
  }
  for (size_t f = 0; f < c->nsrcs; f++) {
    size_t n = pr->asts[f].nodes != nullptr ? ozni_table_count(pr->asts[f].nodes) + 1 : 1;
    biggest = n > biggest ? n : biggest;
    total += n;
    b->marked[f] = ozni_arena_alloc(c->arena, n);
    if (b->marked[f] == nullptr) {
      return false;
    }
    memset(b->marked[f], 0, n);
  }
  b->cap = total;
  b->work = ozni_arena_alloc(c->arena, total * sizeof *b->work);
  b->stack_cap = biggest;
  b->stack = ozni_arena_alloc(c->arena, biggest * sizeof *b->stack);
  if (b->work == nullptr || b->stack == nullptr) {
    return false;
  }
  for (ozni_index mi = 0; mi < nmod; mi++) {
    if (!user_module(b, mi)) {
      continue;
    }
    const ozni_module *m = mod(b, mi);
    for (ozni_index d = 0; d < ozni_table_count(m->defs); d++) {
      const ozni_def *e = ozni_table_get(m->defs, d);
      ozni_node_kind k = nd(b, m->file, e->node)->kind;
      if (e->name == entry && (k == OZNI_N_Menu || k == OZNI_N_Recipe)) {
        take(b, mi, top_item(b, mi, e->node));
      }
    }
  }
  while (b->nwork > 0) {
    scan(b, b->work[--b->nwork]);
  }
  take_extensions(b);
  ozni_index *mods = ozni_arena_alloc(c->arena, (nmod + 1) * sizeof *mods);
  ozni_index *pkgs = ozni_arena_alloc(c->arena, (ozni_table_count(pr->packages) + 1) * sizeof *pkgs);
  if (mods == nullptr || pkgs == nullptr) {
    return false;
  }
  size_t nm = 0, np = 0;
  for (ozni_index mi = 0; mi < nmod; mi++) {
    used[mi] = user_module(b, mi) && module_used(b, mi);
    if (used[mi]) {
      mods[nm++] = mi;
      ozni_index pi = mod(b, mi)->pkg;
      bool seen = pi == NONE || pi == pr->root_pkg;
      for (size_t k = 0; k < np; k++) {
        seen = seen || pkgs[k] == pi;
      }
      if (!seen) {
        pkgs[np++] = pi;
      }
    }
  }
  sort_by_name(b, mods, nm, false);
  sort_by_name(b, pkgs, np, true);
  ozni_buf_puts(out, "bundle {\n  std: ");
  ozni_str sv = {"", 0};
  if (pr->std_pkg != NONE && ((const ozni_package *)ozni_table_get(pr->packages, pr->std_pkg))->version != 0) {
    sv = str_of(c, ((const ozni_package *)ozni_table_get(pr->packages, pr->std_pkg))->version);
  }
  ozni_buf_quoted(out, sv.p, sv.len);
  ozni_buf_puts(out, "\n  entry: ");
  put_str(out, str_of(c, entry));
  ozni_buf_puts(out, "\n");
  if (pr->root_pkg != NONE) {
    put_from(b, out, pr->root_pkg);
  }
  for (size_t k = 0; k < np; k++) {
    put_from(b, out, pkgs[k]);
  }
  ozni_index lic = pr->root_pkg != NONE ? field_string(b, ozni_table_get(pr->packages, pr->root_pkg), "license") : 0;
  if (lic != 0) {
    ozni_str l = str_of(c, lic);
    ozni_buf_puts(out, "  license: ");
    ozni_buf_quoted(out, l.p, l.len);
    ozni_buf_puts(out, "\n");
  }
  ozni_buf_puts(out, "}\n");
  for (size_t k = 0; k < nm; k++) {
    ozni_buf_puts(out, "\nmodule ");
    put_str(out, str_of(c, mod(b, mods[k])->name));
    ozni_buf_puts(out, " {\n");
    size_t before = out->len;
    put_imports(b, out, mods[k], used);
    if (out->len != before) {
      ozni_buf_puts(out, "\n");
    }
    put_items(b, out, mods[k]);
    ozni_buf_puts(out, "}\n");
  }
  if (pr->root_pkg != NONE) {
    pkgs[np++] = pr->root_pkg;
  }
  put_phrases(b, out, ph, nph, pkgs, np);
  return !out->failed && !c->oom;
}

char *ozni_bundle_text(const ozni_allocator *a, const ozni_limits *lim, const ozni_settings *set,
                       const ozni_source *srcs, size_t n, const ozni_source *phrases, size_t nphrases, int *has_errors) {
  *has_errors = 0;
  ozni_ctx c;
  if (!ozni_ctx_init(&c, a, lim, srcs, n)) {
    return nullptr;
  }
  if (set != nullptr) {
    c.set = *set;
  }
  ozni_project pr;
  ozni_index entry = 0;
  bool loaded = ozni_project_load(&c, srcs, n, &pr);
  bool picked = loaded && !c.oom && !pr.syntax_errors && !c.limit_hit && !any_error(&c) && entry_name(&pr, a, &entry);
  if (picked) {
    ozni_str e = str_of(&c, entry);
    char *copy = ozni_arena_alloc(c.arena, e.len + 1);
    if (copy != nullptr) {
      ozni_copy(copy, e.p, e.len);
      copy[e.len] = '\0';
      c.set.entry = copy;
      c.set.api = true;
    }
    loaded = copy != nullptr && ozni_sema_run(&pr, OZNI_OUT_NONE, nullptr, nullptr);
  }
  if (loaded && !c.oom && !ozni_apply_levels(&pr)) {
    c.oom = true;
  }
  if (!loaded || c.oom || ozni_arena_failures(c.arena) > 0) {
    c.oom = true;
    ozni_diag_add(&c, OZNI_D_MAC_outOfMemory, (ozni_span){0, 0, 0}, 0, nullptr, 0);
  }
  ozni_buf out = {.a = a};
  bundler b = {.pr = &pr, .c = &c};
  bool done = picked && !c.oom && !any_error(&c) && build(&b, entry, &out, phrases, nphrases);
  char *text = nullptr;
  if (done) {
    text = ozni_buf_finish(&out);
  } else {
    ozni_buf_discard(&out);
    bool errors = false;
    if (ozni_render_check(&c, &out, &errors)) {
      text = ozni_buf_finish(&out);
    } else {
      ozni_buf_discard(&out);
    }
    *has_errors = 1;
  }
  ozni_ctx_free(&c);
  return text;
}
