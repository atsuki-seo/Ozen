/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#include "sched.h"

static bool add(ozni_rat a, ozni_rat b, ozni_rat *out) { return ozni_rat_add(a, b, out) == OZNI_OK; }

static ozni_rat later(ozni_rat a, ozni_rat b) { return ozni_rat_cmp(a, b) > 0 ? a : b; }

typedef struct hist {
  uint32_t task;
  uint32_t group;
  uint32_t next;
} hist;

typedef struct state {
  ozni_rat *cook_free;
  ozni_rat *unit_free;
  uint32_t *head;
  uint32_t *owner;
  bool *soaked;
  hist *hist;
  uint32_t nhist;
  uint32_t hist_cap;
} state;

static bool case_matches(const ozni_sched *s, const ozni_sched_case *k, const ozni_sched_mark *m,
                         const ozni_sched_task *l, const ozni_sched_alt *la, uint32_t lg) {
  if (k->when == OZNI_WHEN_SAME_FOOD) {
    return m->food != 0 && m->food == l->food;
  }
  if (k->when == OZNI_WHEN_SAME_TOOL) {
    for (uint32_t i = 0; i < la->nmarks; i++) {
      const ozni_sched_mark *o = &s->marks[la->first_mark + i];
      if (o->group == lg && o->dirt == m->dirt) {
        return true;
      }
    }
    return false;
  }
  return (l->targets & k->bits) != 0;
}

static uint8_t level_between(const ozni_sched *s, const ozni_sched_alt *ea, uint32_t eg, const ozni_sched_task *l,
                             const ozni_sched_alt *la, uint32_t lg, uint32_t *reason) {
  uint8_t best = OZNI_CLEAN_NONE;
  for (uint32_t i = 0; i < ea->nmarks; i++) {
    const ozni_sched_mark *m = &s->marks[ea->first_mark + i];
    if (m->group != eg) {
      continue;
    }
    const ozni_sched_dirt *d = &s->dirts[m->dirt];
    uint32_t c = 0;
    while (c < d->ncases) {
      uint32_t rule = s->cases[d->first_case + c].rule;
      int def = -1, hit = -1;
      for (; c < d->ncases && s->cases[d->first_case + c].rule == rule; c++) {
        const ozni_sched_case *k = &s->cases[d->first_case + c];
        if (k->when == OZNI_WHEN_DEFAULT) {
          def = def < 0 ? k->level : def;
        } else if (hit < 0 && case_matches(s, k, m, l, la, lg)) {
          hit = k->level;
        }
      }
      int lvl = hit >= 0 ? hit : (def >= 0 ? def : OZNI_CLEAN_NONE);
      if (lvl > best) {
        best = (uint8_t)lvl;
        if (reason != nullptr) {
          *reason = m->dirt;
        }
      }
    }
  }
  return best;
}

static const ozni_sched_alt *chosen_alt(const ozni_sched *s, const ozni_sched_task *t) {
  return &s->alts[t->first_alt + t->chosen];
}

static uint8_t unit_need(const ozni_sched *s, const state *st, uint32_t unit, uint32_t ti, const ozni_sched_alt *alt,
                         uint32_t g, uint32_t *reason) {
  if (s->unit_info == nullptr || s->dirts == nullptr) {
    return OZNI_CLEAN_NONE;
  }
  uint8_t best = OZNI_CLEAN_NONE;
  for (uint32_t h = st->head[unit]; h != 0; h = st->hist[h - 1].next) {
    const hist *e = &st->hist[h - 1];
    const ozni_sched_task *other = &s->tasks[e->task];
    uint32_t why = UINT32_MAX;
    uint8_t lvl = s->backward ? level_between(s, alt, g, other, chosen_alt(s, other), e->group, &why)
                              : level_between(s, chosen_alt(s, other), e->group, &s->tasks[ti], alt, g, &why);
    if (lvl > best) {
      best = lvl;
      if (reason != nullptr) {
        *reason = why;
      }
    }
  }
  return best;
}

static bool fit(const ozni_sched *s, const state *st, uint32_t ti, const ozni_sched_alt *alt, ozni_rat now,
                ozni_rat *start, uint32_t *cook) {
  *start = now;
  *cook = 0;
  if (alt->hands && s->cooks > 0) {
    bool found = false;
    ozni_rat best = now;
    for (uint32_t c = 1; c <= s->cooks; c++) {
      if (c <= 64 && (alt->banned >> (c - 1) & 1) != 0) {
        continue;
      }
      ozni_rat at = later(st->cook_free[c], now);
      if (!found || ozni_rat_cmp(at, best) < 0) {
        best = at;
        *cook = c;
        found = true;
      }
    }
    if (!found) {
      return false;
    }
    *start = best;
  }
  ozni_rat cook_start = *start;
  for (uint32_t g = 0; g < alt->ngroups; g++) {
    ozni_sched_group *grp = &s->groups[alt->first_group + g];
    grp->pick = UINT32_MAX;
    grp->wash = OZNI_CLEAN_NONE;
    ozni_rat best = now;
    for (uint32_t u = 0; u < grp->nunits; u++) {
      uint32_t unit = s->units[grp->first_unit + u];
      bool taken = st->owner[unit] != 0 && st->owner[unit] != ti + 1;
      for (uint32_t h = 0; h < g; h++) {
        taken = taken || s->groups[alt->first_group + h].pick == unit;
      }
      if (taken) {
        continue;
      }
      ozni_rat at = later(st->unit_free[unit], now);
      uint8_t wash = unit_need(s, st, unit, ti, alt, g, nullptr);
      if (wash != OZNI_CLEAN_NONE) {
        at = later(at, cook_start);
        if (!add(at, s->unit_info[unit].clean_hi, &at) ||
            (wash == OZNI_CLEAN_SOAK_WASH && !st->soaked[unit] && !add(at, s->soak_hi, &at))) {
          return false;
        }
      }
      int r = grp->pick == UINT32_MAX ? -1 : ozni_rat_cmp(at, best);
      if (r < 0 || (r == 0 && wash == OZNI_CLEAN_NONE && grp->wash != OZNI_CLEAN_NONE)) {
        best = at;
        grp->pick = unit;
        grp->wash = wash;
      }
    }
    if (grp->pick == UINT32_MAX) {
      return false;
    }
    *start = later(*start, best);
  }
  return true;
}

static bool push_extra(ozni_sched *s, ozni_sched_extra e) {
  if (s->nextras >= s->extras_cap) {
    return false;
  }
  s->extras[s->nextras++] = e;
  return true;
}

static bool clean_unit(ozni_sched *s, state *st, uint32_t ti, uint32_t unit, uint8_t level, uint32_t reason,
                       uint32_t prefer, ozni_rat now, bool *placed) {
  if (ozni_rat_cmp(st->unit_free[unit], now) > 0) {
    return true;
  }
  const ozni_sched_unit *ui = &s->unit_info[unit];
  bool two = level == OZNI_CLEAN_SOAK_WASH;
  bool soak_now = two && (s->backward ? st->soaked[unit] : !st->soaked[unit]);
  if (soak_now) {
    ozni_sched_extra e = {.kind = OZNI_EXTRA_SOAK, .level = level, .unit = unit, .sink = UINT32_MAX, .dirt = reason,
                          .task = ti, .start = now};
    if (!add(now, s->soak_hi, &e.end_hi) || !add(now, s->soak_lo, &e.end_lo) || !push_extra(s, e)) {
      return false;
    }
    st->unit_free[unit] = e.end_hi;
    st->owner[unit] = ti + 1;
    st->soaked[unit] = !st->soaked[unit];
    if (!s->backward) {
      *placed = true;
      return true;
    }
    st->head[unit] = 0;
    *placed = true;
    return true;
  }
  uint32_t sink = UINT32_MAX;
  bool any_sink = false;
  for (uint32_t k = 0; k < s->nunits; k++) {
    if (!s->unit_info[k].sink) {
      continue;
    }
    any_sink = true;
    if (sink == UINT32_MAX && ozni_rat_cmp(st->unit_free[k], now) <= 0 && st->owner[k] == 0) {
      sink = k;
    }
  }
  if (any_sink && sink == UINT32_MAX) {
    return true;
  }
  uint32_t cook = 0;
  if (prefer != 0 && ozni_rat_cmp(st->cook_free[prefer], now) <= 0) {
    cook = prefer;
  }
  for (uint32_t c = 1; cook == 0 && c <= s->cooks; c++) {
    if (ozni_rat_cmp(st->cook_free[c], now) <= 0) {
      cook = c;
    }
  }
  if (cook == 0 && s->cooks > 0) {
    return true;
  }
  ozni_sched_extra e = {.kind = level == OZNI_CLEAN_RINSE ? OZNI_EXTRA_RINSE : OZNI_EXTRA_WASH,
                        .level = level,
                        .unit = unit,
                        .sink = sink,
                        .cook = cook,
                        .dirt = reason,
                        .task = ti,
                        .start = now};
  if (!add(now, ui->clean_hi, &e.end_hi) || !add(now, ui->clean_lo, &e.end_lo) || !push_extra(s, e)) {
    return false;
  }
  st->unit_free[unit] = e.end_hi;
  if (sink != UINT32_MAX) {
    st->unit_free[sink] = e.end_hi;
  }
  if (cook != 0) {
    st->cook_free[cook] = e.end_hi;
  }
  st->owner[unit] = ti + 1;
  if (two && s->backward) {
    st->soaked[unit] = true;
  } else {
    st->head[unit] = 0;
    st->soaked[unit] = false;
  }
  *placed = true;
  return true;
}

static bool shares_unit(const ozni_sched *s, const ozni_sched_group *a, const ozni_sched_group *b) {
  for (uint32_t i = 0; i < a->nunits; i++) {
    for (uint32_t j = 0; j < b->nunits; j++) {
      if (s->units[a->first_unit + i] == s->units[b->first_unit + j]) {
        return true;
      }
    }
  }
  return false;
}

static uint8_t first_cost(const ozni_sched *s, uint32_t i, uint32_t j) {
  const ozni_sched_task *ti = &s->tasks[i], *tj = &s->tasks[j];
  const ozni_sched_alt *ai = &s->alts[ti->first_alt], *aj = &s->alts[tj->first_alt];
  uint8_t worst = OZNI_CLEAN_NONE;
  for (uint32_t gi = 0; gi < ai->ngroups; gi++) {
    for (uint32_t gj = 0; gj < aj->ngroups; gj++) {
      if (!shares_unit(s, &s->groups[ai->first_group + gi], &s->groups[aj->first_group + gj])) {
        continue;
      }
      uint8_t lvl = s->backward ? level_between(s, aj, gj, ti, ai, gi, nullptr)
                                : level_between(s, ai, gi, tj, aj, gj, nullptr);
      worst = lvl > worst ? lvl : worst;
    }
  }
  return worst;
}

static bool run(ozni_arena *ar, ozni_sched *s, const uint32_t *dep_first, const uint32_t *dep_count,
                const uint32_t *deps) {
  uint32_t n = s->ntasks;
  ozni_rat *prio = ozni_arena_alloc(ar, (n + 1) * sizeof *prio);
  ozni_rat *best = ozni_arena_alloc(ar, (n + 1) * sizeof *best);
  uint32_t *ready = ozni_arena_alloc(ar, (n + 1) * sizeof *ready);
  bool *defer = ozni_arena_alloc(ar, (n + 1) * sizeof *defer);
  state st = {ozni_arena_alloc(ar, (s->cooks + 1) * sizeof *st.cook_free),
              ozni_arena_alloc(ar, (s->nunits + 1) * sizeof *st.unit_free),
              ozni_arena_alloc(ar, (s->nunits + 1) * sizeof *st.head),
              ozni_arena_alloc(ar, (s->nunits + 1) * sizeof *st.owner),
              ozni_arena_alloc(ar, (s->nunits + 1) * sizeof *st.soaked),
              nullptr,
              0,
              0};
  uint32_t total_groups = 0;
  for (uint32_t i = 0; i < n; i++) {
    for (uint32_t a = 0; a < s->tasks[i].nalts; a++) {
      total_groups += s->alts[s->tasks[i].first_alt + a].ngroups;
    }
  }
  st.hist_cap = total_groups + 1;
  st.hist = ozni_arena_alloc(ar, st.hist_cap * sizeof *st.hist);
  if (prio == nullptr || best == nullptr || ready == nullptr || defer == nullptr || st.cook_free == nullptr ||
      st.unit_free == nullptr || st.head == nullptr || st.owner == nullptr || st.soaked == nullptr ||
      st.hist == nullptr) {
    return false;
  }
  for (uint32_t i = 0; i < n; i++) {
    best[i] = ozni_rat_int(0);
  }
  for (uint32_t k = n; k > 0; k--) {
    uint32_t i = k - 1;
    if (!add(s->alts[s->tasks[i].first_alt].hi, best[i], &prio[i])) {
      return false;
    }
    for (uint32_t d = 0; d < dep_count[i]; d++) {
      uint32_t p = deps[dep_first[i] + d];
      if (ozni_rat_cmp(prio[i], best[p]) > 0) {
        best[p] = prio[i];
      }
    }
  }
  for (uint32_t c = 0; c <= s->cooks; c++) {
    st.cook_free[c] = ozni_rat_int(0);
  }
  for (uint32_t u = 0; u <= s->nunits; u++) {
    st.unit_free[u] = ozni_rat_int(0);
    st.head[u] = 0;
    st.owner[u] = 0;
    st.soaked[u] = false;
  }
  ozni_rat now = ozni_rat_int(0);
  uint32_t left = n;
  uint64_t rounds = 0, limit = (uint64_t)(n + s->nunits + 1) * (n + s->nunits + 1) * 16 + 64;
  while (left > 0) {
    if (++rounds > limit) {
      return false;
    }
    uint32_t nready = 0;
    for (uint32_t i = 0; i < n; i++) {
      if (s->tasks[i].placed) {
        continue;
      }
      bool ok = true;
      for (uint32_t d = 0; d < dep_count[i] && ok; d++) {
        const ozni_sched_task *p = &s->tasks[deps[dep_first[i] + d]];
        ok = p->placed && ozni_rat_cmp(p->end_hi, now) <= 0;
      }
      if (ok && s->tasks[i].cleanup && s->cleanup == OZNI_CLEANUP_AFTER_MEAL && !s->backward) {
        for (uint32_t j = 0; j < n && ok; j++) {
          const ozni_sched_task *o = &s->tasks[j];
          ok = o->cleanup || (o->placed && ozni_rat_cmp(o->end_hi, now) <= 0);
        }
      }
      if (ok) {
        ready[nready++] = i;
      }
    }
    for (uint32_t r = 0; r < nready; r++) {
      defer[ready[r]] = false;
      for (uint32_t q = 0; q < nready && s->dirts != nullptr; q++) {
        uint32_t i = ready[r], j = ready[q];
        if (i != j && ozni_rat_cmp(prio[i], prio[j]) == 0 && first_cost(s, i, j) != OZNI_CLEAN_NONE &&
            first_cost(s, j, i) == OZNI_CLEAN_NONE) {
          defer[i] = true;
        }
      }
    }
    for (uint32_t r = 1; r < nready; r++) {
      uint32_t i = ready[r], j = r;
      while (j > 0) {
        uint32_t a = ready[j - 1];
        int c = ozni_rat_cmp(prio[a], prio[i]);
        if (c > 0 || (c == 0 && defer[a] < defer[i]) ||
            (c == 0 && defer[a] == defer[i] && s->tasks[a].order < s->tasks[i].order)) {
          break;
        }
        ready[j] = a;
        j--;
      }
      ready[j] = i;
    }
    bool placed = false;
    for (uint32_t r = 0; r < nready; r++) {
      uint32_t ti = ready[r];
      ozni_sched_task *t = &s->tasks[ti];
      uint32_t pick = UINT32_MAX, pick_cook = 0;
      ozni_rat pick_start = now, pick_end = now;
      for (uint32_t a = 0; a < t->nalts; a++) {
        const ozni_sched_alt *alt = &s->alts[t->first_alt + a];
        ozni_rat start, end;
        uint32_t cook;
        if (!fit(s, &st, ti, alt, now, &start, &cook) || !add(start, alt->hi, &end)) {
          continue;
        }
        if (pick == UINT32_MAX || ozni_rat_cmp(end, pick_end) < 0) {
          pick = a;
          pick_cook = cook;
          pick_start = start;
          pick_end = end;
        }
      }
      if (pick == UINT32_MAX) {
        continue;
      }
      const ozni_sched_alt *alt = &s->alts[t->first_alt + pick];
      uint32_t cook;
      ozni_rat start;
      if (!fit(s, &st, ti, alt, now, &start, &cook)) {
        continue;
      }
      bool washing = false;
      for (uint32_t g = 0; g < alt->ngroups; g++) {
        const ozni_sched_group *grp = &s->groups[alt->first_group + g];
        if (grp->wash == OZNI_CLEAN_NONE) {
          continue;
        }
        washing = true;
        uint32_t reason = UINT32_MAX;
        unit_need(s, &st, grp->pick, ti, alt, g, &reason);
        if (!clean_unit(s, &st, ti, grp->pick, grp->wash, reason, pick_cook, now, &placed)) {
          return false;
        }
      }
      if (washing || ozni_rat_cmp(pick_start, now) > 0) {
        continue;
      }
      t->chosen = pick;
      t->start = now;
      if (!add(now, alt->hi, &t->end_hi) || !add(now, alt->lo, &t->end_lo)) {
        return false;
      }
      t->cook = pick_cook;
      t->placed = true;
      placed = true;
      left--;
      if (pick_cook != 0) {
        st.cook_free[pick_cook] = t->end_hi;
      }
      for (uint32_t u = 0; u < s->nunits; u++) {
        st.owner[u] = st.owner[u] == ti + 1 ? 0 : st.owner[u];
      }
      for (uint32_t g = 0; g < alt->ngroups; g++) {
        uint32_t unit = s->groups[alt->first_group + g].pick;
        st.unit_free[unit] = t->end_hi;
        if (st.nhist < st.hist_cap) {
          st.hist[st.nhist] = (hist){ti, g, st.head[unit]};
          st.head[unit] = ++st.nhist;
        }
      }
    }
    if (left == 0) {
      break;
    }
    if (placed) {
      continue;
    }
    bool found = false;
    ozni_rat next = now;
    for (uint32_t i = 0; i < n; i++) {
      const ozni_sched_task *t = &s->tasks[i];
      if (t->placed && ozni_rat_cmp(t->end_hi, now) > 0 && (!found || ozni_rat_cmp(t->end_hi, next) < 0)) {
        next = t->end_hi;
        found = true;
      }
    }
    for (uint32_t k = 0; k < s->nextras; k++) {
      const ozni_sched_extra *e = &s->extras[k];
      if (ozni_rat_cmp(e->end_hi, now) > 0 && (!found || ozni_rat_cmp(e->end_hi, next) < 0)) {
        next = e->end_hi;
        found = true;
      }
    }
    if (!found) {
      return false;
    }
    now = next;
  }
  return true;
}

static uint8_t default_level(const ozni_sched *s, uint32_t dirt) {
  const ozni_sched_dirt *d = &s->dirts[dirt];
  uint8_t best = OZNI_CLEAN_NONE;
  uint32_t c = 0;
  while (c < d->ncases) {
    uint32_t rule = s->cases[d->first_case + c].rule;
    int def = -1;
    for (; c < d->ncases && s->cases[d->first_case + c].rule == rule; c++) {
      const ozni_sched_case *k = &s->cases[d->first_case + c];
      def = def < 0 && k->when == OZNI_WHEN_DEFAULT ? k->level : def;
    }
    best = def > best ? (uint8_t)def : best;
  }
  return best;
}

static bool cleanup(ozni_arena *ar, ozni_sched *s) {
  if (s->cleanup == OZNI_CLEANUP_NONE || s->unit_info == nullptr) {
    return true;
  }
  uint32_t n = s->ntasks, nu = s->nunits;
  ozni_rat *unit_free = ozni_arena_alloc(ar, (nu + 1) * sizeof *unit_free);
  ozni_rat *cleaned = ozni_arena_alloc(ar, (nu + 1) * sizeof *cleaned);
  ozni_rat *cook_free = ozni_arena_alloc(ar, (s->cooks + 1) * sizeof *cook_free);
  bool *was_cleaned = ozni_arena_alloc(ar, (nu + 1) * sizeof *was_cleaned);
  bool *used = ozni_arena_alloc(ar, (nu + 1) * sizeof *used);
  uint32_t *reason = ozni_arena_alloc(ar, (nu + 1) * sizeof *reason);
  int *level = ozni_arena_alloc(ar, (nu + 1) * sizeof *level);
  uint32_t *order = ozni_arena_alloc(ar, (n + nu + 1) * sizeof *order);
  if (unit_free == nullptr || cleaned == nullptr || cook_free == nullptr || was_cleaned == nullptr ||
      used == nullptr || reason == nullptr || level == nullptr || order == nullptr) {
    return false;
  }
  ozni_rat serve = ozni_rat_int(0);
  for (uint32_t u = 0; u < nu; u++) {
    unit_free[u] = ozni_rat_int(0);
    was_cleaned[u] = false;
    used[u] = false;
    reason[u] = UINT32_MAX;
    level[u] = -1;
  }
  for (uint32_t c = 0; c <= s->cooks; c++) {
    cook_free[c] = ozni_rat_int(0);
  }
  for (uint32_t k = 0; k < s->nextras; k++) {
    const ozni_sched_extra *e = &s->extras[k];
    unit_free[e->unit] = later(unit_free[e->unit], e->end_hi);
    if (e->sink != UINT32_MAX) {
      unit_free[e->sink] = later(unit_free[e->sink], e->end_hi);
    }
    if (e->cook != 0) {
      cook_free[e->cook] = later(cook_free[e->cook], e->end_hi);
    }
    if (e->kind != OZNI_EXTRA_SOAK && (!was_cleaned[e->unit] || ozni_rat_cmp(e->end_hi, cleaned[e->unit]) > 0)) {
      cleaned[e->unit] = e->end_hi;
      was_cleaned[e->unit] = true;
    }
    serve = later(serve, e->end_hi);
  }
  for (uint32_t i = 0; i < n; i++) {
    uint32_t j = i;
    while (j > 0) {
      const ozni_sched_task *a = &s->tasks[order[j - 1]];
      int r = ozni_rat_cmp(a->start, s->tasks[i].start);
      if (r < 0 || (r == 0 && a->order < s->tasks[i].order)) {
        break;
      }
      order[j] = order[j - 1];
      j--;
    }
    order[j] = i;
  }
  for (uint32_t k = 0; k < n; k++) {
    const ozni_sched_task *t = &s->tasks[order[k]];
    const ozni_sched_alt *alt = chosen_alt(s, t);
    if (!t->cleanup) {
      serve = later(serve, t->end_hi);
    }
    if (t->cook != 0) {
      cook_free[t->cook] = later(cook_free[t->cook], t->end_hi);
    }
    for (uint32_t g = 0; g < alt->ngroups; g++) {
      uint32_t u = s->groups[alt->first_group + g].pick;
      unit_free[u] = later(unit_free[u], t->end_hi);
      if (was_cleaned[u] && ozni_rat_cmp(t->start, cleaned[u]) < 0) {
        continue;
      }
      used[u] = true;
      for (uint32_t m = 0; m < alt->nmarks && s->dirts != nullptr; m++) {
        const ozni_sched_mark *mk = &s->marks[alt->first_mark + m];
        int lvl = mk->group == g ? default_level(s, mk->dirt) : -1;
        if (lvl > level[u]) {
          level[u] = lvl;
          reason[u] = mk->dirt;
        }
      }
    }
  }
  ozni_rat floor = s->cleanup == OZNI_CLEANUP_AFTER_MEAL ? serve : ozni_rat_int(0);
  uint32_t nlist = 0;
  for (uint32_t u = 0; u < nu; u++) {
    uint8_t cat = s->unit_info[u].category;
    bool take = used[u] && (cat == OZNI_UNIT_TOOL ||
                            (cat == OZNI_UNIT_PLACE && ozni_rat_cmp(s->unit_info[u].clean_hi, ozni_rat_int(0)) > 0) ||
                            (cat == OZNI_UNIT_VESSEL && s->cleanup == OZNI_CLEANUP_AFTER_MEAL));
    if (!take) {
      continue;
    }
    uint32_t j = nlist++;
    while (j > 0 && ozni_rat_cmp(unit_free[order[j - 1]], unit_free[u]) > 0) {
      order[j] = order[j - 1];
      j--;
    }
    order[j] = u;
  }
  for (uint32_t k = 0; k < nlist; k++) {
    uint32_t u = order[k];
    const ozni_sched_unit *ui = &s->unit_info[u];
    ozni_rat at = later(unit_free[u], floor);
    bool wipe = ui->category == OZNI_UNIT_PLACE;
    uint8_t lvl = wipe || level[u] < OZNI_CLEAN_WASH ? OZNI_CLEAN_WASH : (uint8_t)level[u];
    if (lvl == OZNI_CLEAN_SOAK_WASH) {
      ozni_sched_extra e = {.kind = OZNI_EXTRA_SOAK, .level = lvl, .unit = u, .sink = UINT32_MAX,
                            .dirt = reason[u], .task = UINT32_MAX, .start = at};
      if (!add(at, s->soak_hi, &e.end_hi) || !add(at, s->soak_lo, &e.end_lo) || !push_extra(s, e)) {
        return false;
      }
      at = e.end_hi;
    }
    uint32_t cook = 0;
    for (uint32_t c = 1; c <= s->cooks; c++) {
      if (cook == 0 || ozni_rat_cmp(cook_free[c], cook_free[cook]) < 0) {
        cook = c;
      }
    }
    uint32_t sink = UINT32_MAX;
    for (uint32_t q = 0; q < nu && !wipe; q++) {
      if (s->unit_info[q].sink && (sink == UINT32_MAX || ozni_rat_cmp(unit_free[q], unit_free[sink]) < 0)) {
        sink = q;
      }
    }
    if (cook != 0) {
      at = later(at, cook_free[cook]);
    }
    if (sink != UINT32_MAX) {
      at = later(at, unit_free[sink]);
    }
    ozni_sched_extra e = {.kind = wipe ? OZNI_EXTRA_WIPE : OZNI_EXTRA_WASH,
                          .level = lvl,
                          .unit = u,
                          .sink = sink,
                          .cook = cook,
                          .dirt = wipe ? UINT32_MAX : reason[u],
                          .task = UINT32_MAX,
                          .start = at};
    if (!add(at, ui->clean_hi, &e.end_hi) || !add(at, ui->clean_lo, &e.end_lo) || !push_extra(s, e)) {
      return false;
    }
    unit_free[u] = e.end_hi;
    if (sink != UINT32_MAX) {
      unit_free[sink] = e.end_hi;
    }
    if (cook != 0) {
      cook_free[cook] = e.end_hi;
    }
  }
  if (!s->backward || s->cleanup != OZNI_CLEANUP_BEFORE_MEAL) {
    return true;
  }
  ozni_rat end = s->serve_at;
  for (uint32_t k = 0; k < s->nextras; k++) {
    end = later(end, s->extras[k].end_hi);
  }
  ozni_rat shift;
  if (ozni_rat_sub(end, s->serve_at, &shift) != OZNI_OK) {
    return false;
  }
  for (uint32_t i = 0; i < n; i++) {
    ozni_sched_task *t = &s->tasks[i];
    if (ozni_rat_sub(t->start, shift, &t->start) != OZNI_OK || ozni_rat_sub(t->end_hi, shift, &t->end_hi) != OZNI_OK ||
        ozni_rat_sub(t->end_lo, shift, &t->end_lo) != OZNI_OK) {
      return false;
    }
  }
  for (uint32_t k = 0; k < s->nextras; k++) {
    ozni_sched_extra *e = &s->extras[k];
    if (ozni_rat_sub(e->start, shift, &e->start) != OZNI_OK || ozni_rat_sub(e->end_hi, shift, &e->end_hi) != OZNI_OK ||
        ozni_rat_sub(e->end_lo, shift, &e->end_lo) != OZNI_OK) {
      return false;
    }
  }
  return true;
}

bool ozni_schedule(ozni_arena *ar, ozni_sched *s) {
  uint32_t n = s->ntasks;
  uint32_t total = 0, groups = 0;
  for (uint32_t i = 0; i < n; i++) {
    total += s->tasks[i].ndeps;
    for (uint32_t a = 0; a < s->tasks[i].nalts; a++) {
      groups += s->alts[s->tasks[i].first_alt + a].ngroups;
    }
  }
  s->extras_cap = 4 * (groups + 1) + 2 * s->nunits + 8;
  s->extras = ozni_arena_alloc(ar, s->extras_cap * sizeof *s->extras);
  s->nextras = 0;
  uint32_t *first = ozni_arena_alloc(ar, (n + 1) * sizeof *first);
  uint32_t *count = ozni_arena_alloc(ar, (n + 1) * sizeof *count);
  uint32_t *deps = ozni_arena_alloc(ar, (total + 1) * sizeof *deps);
  if (first == nullptr || count == nullptr || deps == nullptr || s->extras == nullptr) {
    return false;
  }
  if (!s->backward) {
    for (uint32_t i = 0; i < n; i++) {
      first[i] = s->tasks[i].first_dep;
      count[i] = s->tasks[i].ndeps;
    }
    return run(ar, s, first, count, s->deps) && cleanup(ar, s);
  }
  for (uint32_t i = 0; i < n; i++) {
    count[i] = 0;
  }
  for (uint32_t i = 0; i < n; i++) {
    for (uint32_t d = 0; d < s->tasks[i].ndeps; d++) {
      count[s->deps[s->tasks[i].first_dep + d]]++;
    }
  }
  uint32_t at = 0;
  for (uint32_t i = 0; i < n; i++) {
    first[i] = at;
    at += count[i];
    count[i] = 0;
  }
  for (uint32_t i = 0; i < n; i++) {
    for (uint32_t d = 0; d < s->tasks[i].ndeps; d++) {
      uint32_t p = s->deps[s->tasks[i].first_dep + d];
      deps[first[p] + count[p]++] = n - 1 - i;
    }
  }
  ozni_sched_task *rev = ozni_arena_alloc(ar, (n + 1) * sizeof *rev);
  uint32_t *rfirst = ozni_arena_alloc(ar, (n + 1) * sizeof *rfirst);
  uint32_t *rcount = ozni_arena_alloc(ar, (n + 1) * sizeof *rcount);
  if (rev == nullptr || rfirst == nullptr || rcount == nullptr) {
    return false;
  }
  for (uint32_t i = 0; i < n; i++) {
    rev[n - 1 - i] = s->tasks[i];
    rfirst[n - 1 - i] = first[i];
    rcount[n - 1 - i] = count[i];
  }
  ozni_sched rs = *s;
  rs.tasks = rev;
  if (!run(ar, &rs, rfirst, rcount, deps)) {
    return false;
  }
  s->nextras = rs.nextras;
  for (uint32_t i = 0; i < n; i++) {
    const ozni_sched_task *r = &rev[n - 1 - i];
    ozni_sched_task *t = &s->tasks[i];
    if (ozni_rat_sub(s->serve_at, r->end_hi, &t->start) != OZNI_OK ||
        ozni_rat_sub(s->serve_at, r->start, &t->end_hi) != OZNI_OK ||
        !add(t->start, s->alts[t->first_alt + r->chosen].lo, &t->end_lo)) {
      return false;
    }
    t->cook = r->cook;
    t->chosen = r->chosen;
    t->placed = true;
  }
  for (uint32_t k = 0; k < s->nextras; k++) {
    ozni_sched_extra *e = &s->extras[k];
    ozni_rat lo, start, end;
    if (ozni_rat_sub(e->end_lo, e->start, &lo) != OZNI_OK || ozni_rat_sub(s->serve_at, e->end_hi, &start) != OZNI_OK ||
        ozni_rat_sub(s->serve_at, e->start, &end) != OZNI_OK || !add(start, lo, &e->end_lo)) {
      return false;
    }
    e->start = start;
    e->end_hi = end;
    e->task = e->task != UINT32_MAX ? n - 1 - e->task : UINT32_MAX;
  }
  return cleanup(ar, s);
}
