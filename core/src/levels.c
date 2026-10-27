/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#include <string.h>

#include "resolve.h"
#include "text.h"

constexpr ozni_index NONE = UINT32_MAX;

typedef struct note {
  ozni_index file;
  uint32_t start;
  uint32_t end;
  uint8_t level;
  ozni_index sel;
  ozni_index node;
  bool used;
  bool fixed;
} note;

typedef struct member {
  ozni_index group;
  ozni_index name;
} member;

typedef struct levels {
  ozni_project *pr;
  ozni_ctx *c;
  ozni_table *groups;
  ozni_table *notes;
  ozni_table *checks;
} levels;

static ozni_str text_of(const ozni_ctx *c, ozni_index i) { return ozni_intern_get(c->names, i); }

static const ozni_node *nd(const levels *l, ozni_index file, ozni_index i) {
  return ozni_project_node(l->pr, file, i);
}

static bool same(ozni_str a, const char *b) {
  size_t n = strlen(b);
  return n == a.len && ozni_strncmp(a.p, b, n) == 0;
}

static bool str_equal(ozni_str a, ozni_str b) { return a.len == b.len && ozni_strncmp(a.p, b.p, a.len) == 0; }

static ozni_str diag_name(const levels *l, const ozni_diag *d) {
  if (d->name_text != 0) {
    return text_of(l->c, d->name_text - 1);
  }
  const char *n = ozni_diag_name(d->id);
  return (ozni_str){n, (uint32_t)strlen(n)};
}

static ozni_str diag_code(const levels *l, const ozni_diag *d) {
  if (d->code_text != 0) {
    ozni_str code = text_of(l->c, d->code_text - 1);
    const char *colon = ozni_memchr(code.p, ':', code.len);
    if (colon != nullptr) {
      return (ozni_str){colon + 1, (uint32_t)(code.len - (size_t)(colon + 1 - code.p))};
    }
    return code;
  }
  const char *n = ozni_diag_code(d->id);
  return (ozni_str){n, (uint32_t)strlen(n)};
}

static bool is_category(ozni_str s) {
  if (s.len != 3) {
    return false;
  }
  for (uint32_t k = 0; k < 3; k++) {
    if (s.p[k] < 'A' || s.p[k] > 'Z') {
      return false;
    }
  }
  return true;
}

static bool in_group(const levels *l, ozni_str group, ozni_str name) {
  for (ozni_index k = 0; k < ozni_table_count(l->groups); k++) {
    const member *m = ozni_table_get(l->groups, k);
    if (str_equal(text_of(l->c, m->group), group) && str_equal(text_of(l->c, m->name), name)) {
      return true;
    }
  }
  return false;
}

static bool known(const levels *l, ozni_str sel) {
  if (is_category(sel)) {
    return true;
  }
  for (int id = 0; id < OZNI_D_COUNT; id++) {
    if (same(sel, ozni_diag_name((ozni_diag_id)id))) {
      return true;
    }
  }
  for (ozni_index k = 0; k < ozni_table_count(l->groups); k++) {
    if (str_equal(text_of(l->c, ((const member *)ozni_table_get(l->groups, k))->group), sel)) {
      return true;
    }
  }
  for (ozni_index k = 0; k < ozni_table_count(l->checks); k++) {
    if (str_equal(text_of(l->c, *(const ozni_index *)ozni_table_get(l->checks, k)), sel)) {
      return true;
    }
  }
  return false;
}

static bool matches(const levels *l, ozni_str sel, const ozni_diag *d) {
  ozni_str name = diag_name(l, d);
  if (str_equal(sel, name) || in_group(l, sel, name)) {
    return true;
  }
  ozni_str code = diag_code(l, d);
  return is_category(sel) && code.len >= 3 && ozni_strncmp(code.p, sel.p, 3) == 0;
}

static void collect_groups(levels *l) {
  for (ozni_index mi = 0; mi < ozni_table_count(l->pr->modules); mi++) {
    const ozni_module *m = ozni_project_module(l->pr, mi);
    for (ozni_index c = nd(l, m->file, m->root)->first; c != 0; c = nd(l, m->file, c)->next) {
      const ozni_node *n = nd(l, m->file, c);
      if (n->kind == OZNI_N_Type) {
        for (ozni_index k = n->first; k != 0; k = nd(l, m->file, k)->next) {
          if (nd(l, m->file, k)->kind == OZNI_N_Check && !ozni_table_push(l->checks, &nd(l, m->file, k)->text, nullptr)) {
            l->c->oom = true;
          }
        }
      }
      const ozni_node *v = n->kind == OZNI_N_Property && n->first != 0 ? nd(l, m->file, n->first) : nullptr;
      if (v == nullptr || v->kind != OZNI_N_Call || v->first == 0 ||
          !same(text_of(l->c, nd(l, m->file, v->first)->text), "diagnostics")) {
        continue;
      }
      for (ozni_index a = nd(l, m->file, v->first)->next; a != 0; a = nd(l, m->file, a)->next) {
        member e = {n->text, nd(l, m->file, a)->text};
        if (nd(l, m->file, a)->kind == OZNI_N_Name && !ozni_table_push(l->groups, &e, nullptr)) {
          l->c->oom = true;
        }
      }
    }
  }
}

static void add_diag(levels *l, ozni_diag_id id, ozni_index file, uint32_t start, uint32_t end, ozni_str arg) {
  if (!ozni_diag_add(l->c, id, (ozni_span){file, start, end}, 0, &arg, 1)) {
    l->c->oom = true;
  }
}

static void collect_notes(levels *l, ozni_index file, ozni_index node, ozni_index parent) {
  const ozni_node *n = nd(l, file, node);
  if (n->kind == OZNI_N_Annotation && parent != 0) {
    ozni_str name = text_of(l->c, n->text);
    int level = same(name, "allow") ? OZNI_LEVEL_ALLOW : same(name, "warn") ? OZNI_LEVEL_WARN
              : same(name, "deny")  ? OZNI_LEVEL_DENY  : same(name, "expect") ? OZNI_LEVEL_EXPECT : -1;
    const ozni_node *pn = nd(l, file, parent);
    for (ozni_index a = n->first; level >= 0 && a != 0; a = nd(l, file, a)->next) {
      const ozni_node *an = nd(l, file, a);
      if (an->kind != OZNI_N_Name) {
        continue;
      }
      if (!known(l, text_of(l->c, an->text))) {
        add_diag(l, OZNI_D_NAM_unknownDiagnostic, file, an->span.start, an->span.end, text_of(l->c, an->text));
        continue;
      }
      note e = {file, pn->span.start, pn->span.end, (uint8_t)level, an->text, node, false, false};
      if (!ozni_table_push(l->notes, &e, nullptr)) {
        l->c->oom = true;
      }
    }
    return;
  }
  for (ozni_index c = n->first; c != 0; c = nd(l, file, c)->next) {
    if (nd(l, file, c)->kind != OZNI_N_Module) {
      collect_notes(l, file, c, node);
    }
  }
}

static bool dependency_at(const levels *l, ozni_span at) {
  const ozni_module *best = nullptr;
  uint32_t width = UINT32_MAX;
  for (ozni_index mi = 0; mi < ozni_table_count(l->pr->modules); mi++) {
    const ozni_module *m = ozni_project_module(l->pr, mi);
    if (m->file != at.file) {
      continue;
    }
    const ozni_node *r = nd(l, m->file, m->root);
    bool inside = r->span.start <= at.start && at.end <= r->span.end;
    if (best == nullptr || (inside && r->span.end - r->span.start < width)) {
      best = m;
      width = inside ? r->span.end - r->span.start : UINT32_MAX;
    }
  }
  return best != nullptr && !best->is_std && l->pr->root_pkg != NONE && best->pkg != NONE &&
         best->pkg != l->pr->root_pkg;
}

static int apply_level(int level, ozni_severity *sev) {
  if (level == OZNI_LEVEL_WARN) {
    *sev = OZNI_SEV_WARNING;
  } else if (level == OZNI_LEVEL_DENY) {
    *sev = OZNI_SEV_ERROR;
  }
  return level == OZNI_LEVEL_ALLOW || level == OZNI_LEVEL_EXPECT;
}

bool ozni_apply_levels(ozni_project *pr) {
  levels l = {pr, pr->c, nullptr, nullptr, nullptr};
  l.groups = ozni_table_new(pr->c->arena, sizeof(member));
  l.notes = ozni_table_new(pr->c->arena, sizeof(note));
  l.checks = ozni_table_new(pr->c->arena, sizeof(ozni_index));
  if (l.groups == nullptr || l.notes == nullptr || l.checks == nullptr) {
    return false;
  }
  ozni_index total = ozni_table_count(pr->c->diags);
  collect_groups(&l);
  for (ozni_index mi = 0; mi < ozni_table_count(pr->modules); mi++) {
    const ozni_module *m = ozni_project_module(pr, mi);
    if (!m->is_std) {
      collect_notes(&l, m->file, m->root, 0);
    }
  }
  const ozni_settings *set = &pr->c->set;
  for (size_t k = 0; k < set->nlevels; k++) {
    ozni_str sel = {set->level_names[k], (uint32_t)strlen(set->level_names[k])};
    if (!known(&l, sel)) {
      add_diag(&l, OZNI_D_NAM_unknownDiagnostic, 0, 0, 0, sel);
    }
  }
  for (ozni_index i = 0; i < total && !pr->c->oom; i++) {
    ozni_diag *d = ozni_table_at(pr->c->diags, i);
    if (d->hidden) {
      continue;
    }
    ozni_severity base = d->severity, sev = d->severity;
    bool hide = false;
    if (set->strict && matches(&l, (ozni_str){"strict", 6}, d)) {
      sev = OZNI_SEV_ERROR;
    }
    for (size_t k = 0; k < set->nlevels; k++) {
      ozni_str sel = {set->level_names[k], (uint32_t)strlen(set->level_names[k])};
      if (matches(&l, sel, d)) {
        hide = apply_level(set->level_kinds[k], &sev);
      }
    }
    note *best = nullptr;
    for (ozni_index k = 0; k < ozni_table_count(l.notes); k++) {
      note *e = ozni_table_at(l.notes, k);
      if (e->file == d->span.file && e->start <= d->span.start && d->span.end <= e->end &&
          matches(&l, text_of(pr->c, e->sel), d) && (best == nullptr || e->end - e->start < best->end - best->start)) {
        best = e;
      }
    }
    if (best != nullptr) {
      hide = apply_level(best->level, &sev);
      best->used = best->used || best->level == OZNI_LEVEL_EXPECT;
    }
    ozni_str code = diag_code(&l, d);
    bool saf = code.len >= 3 && ozni_strncmp(code.p, "SAF", 3) == 0 && base == OZNI_SEV_ERROR;
    if (saf && (hide || sev != OZNI_SEV_ERROR)) {
      hide = false;
      sev = OZNI_SEV_ERROR;
      if (best != nullptr && !best->fixed) {
        best->fixed = true;
        const ozni_node *an = nd(&l, best->file, best->node);
        add_diag(&l, OZNI_D_SAF_fixedSeverity, best->file, an->span.start, an->span.end, text_of(pr->c, best->sel));
        d = ozni_table_at(pr->c->diags, i);
      }
    }
    if (dependency_at(&l, d->span) && !(saf && sev == OZNI_SEV_ERROR)) {
      hide = true;
    }
    d->severity = sev;
    d->hidden = hide;
  }
  for (ozni_index k = 0; k < ozni_table_count(l.notes); k++) {
    const note *e = ozni_table_get(l.notes, k);
    if (e->level == OZNI_LEVEL_EXPECT && !e->used) {
      const ozni_node *an = nd(&l, e->file, e->node);
      add_diag(&l, OZNI_D_NAM_expectUnfulfilled, e->file, an->span.start, an->span.end, text_of(pr->c, e->sel));
    }
  }
  return !pr->c->oom;
}
