/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#include "sched.h"

static bool add(ozni_rat a, ozni_rat b, ozni_rat *out) { return ozni_rat_add(a, b, out) == OZNI_OK; }

static ozni_rat later(ozni_rat a, ozni_rat b) { return ozni_rat_cmp(a, b) > 0 ? a : b; }

static bool fit(const ozni_sched *s, const ozni_sched_alt *alt, const ozni_rat *cook_free, const ozni_rat *unit_free,
                ozni_rat now, ozni_rat *start, uint32_t *cook) {
  *start = now;
  *cook = 0;
  if (alt->hands && s->cooks > 0) {
    bool found = false;
    ozni_rat best = now;
    for (uint32_t c = 1; c <= s->cooks; c++) {
      if (c <= 64 && (alt->banned >> (c - 1) & 1) != 0) {
        continue;
      }
      ozni_rat at = later(cook_free[c], now);
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
  for (uint32_t g = 0; g < alt->ngroups; g++) {
    ozni_sched_group *grp = &s->groups[alt->first_group + g];
    grp->pick = UINT32_MAX;
    ozni_rat best = now;
    for (uint32_t u = 0; u < grp->nunits; u++) {
      uint32_t unit = s->units[grp->first_unit + u];
      bool taken = false;
      for (uint32_t h = 0; h < g; h++) {
        taken = taken || s->groups[alt->first_group + h].pick == unit;
      }
      ozni_rat at = later(unit_free[unit], now);
      if (!taken && (grp->pick == UINT32_MAX || ozni_rat_cmp(at, best) < 0)) {
        best = at;
        grp->pick = unit;
      }
    }
    if (grp->pick == UINT32_MAX) {
      return false;
    }
    *start = later(*start, best);
  }
  return true;
}

static bool run(ozni_arena *ar, ozni_sched *s, const uint32_t *dep_first, const uint32_t *dep_count,
                const uint32_t *deps) {
  uint32_t n = s->ntasks;
  ozni_rat *prio = ozni_arena_alloc(ar, (n + 1) * sizeof *prio);
  ozni_rat *best = ozni_arena_alloc(ar, (n + 1) * sizeof *best);
  ozni_rat *cook_free = ozni_arena_alloc(ar, (s->cooks + 1) * sizeof *cook_free);
  ozni_rat *unit_free = ozni_arena_alloc(ar, (s->nunits + 1) * sizeof *unit_free);
  uint32_t *ready = ozni_arena_alloc(ar, (n + 1) * sizeof *ready);
  if (prio == nullptr || best == nullptr || cook_free == nullptr || unit_free == nullptr || ready == nullptr) {
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
    cook_free[c] = ozni_rat_int(0);
  }
  for (uint32_t u = 0; u <= s->nunits; u++) {
    unit_free[u] = ozni_rat_int(0);
  }
  ozni_rat now = ozni_rat_int(0);
  uint32_t left = n;
  while (left > 0) {
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
      if (ok) {
        uint32_t j = nready++;
        while (j > 0) {
          const ozni_sched_task *a = &s->tasks[ready[j - 1]];
          int r = ozni_rat_cmp(prio[ready[j - 1]], prio[i]);
          if (r > 0 || (r == 0 && a->order < s->tasks[i].order)) {
            break;
          }
          ready[j] = ready[j - 1];
          j--;
        }
        ready[j] = i;
      }
    }
    for (uint32_t r = 0; r < nready; r++) {
      ozni_sched_task *t = &s->tasks[ready[r]];
      uint32_t pick = UINT32_MAX, pick_cook = 0;
      ozni_rat pick_start = now, pick_end = now;
      for (uint32_t a = 0; a < t->nalts; a++) {
        const ozni_sched_alt *alt = &s->alts[t->first_alt + a];
        ozni_rat start, end;
        uint32_t cook;
        if (!fit(s, alt, cook_free, unit_free, now, &start, &cook) || !add(start, alt->hi, &end)) {
          continue;
        }
        if (pick == UINT32_MAX || ozni_rat_cmp(end, pick_end) < 0) {
          pick = a;
          pick_cook = cook;
          pick_start = start;
          pick_end = end;
        }
      }
      if (pick == UINT32_MAX || ozni_rat_cmp(pick_start, now) > 0) {
        continue;
      }
      const ozni_sched_alt *alt = &s->alts[t->first_alt + pick];
      uint32_t cook;
      ozni_rat start;
      if (!fit(s, alt, cook_free, unit_free, now, &start, &cook)) {
        continue;
      }
      t->chosen = pick;
      t->start = now;
      if (!add(now, alt->hi, &t->end_hi) || !add(now, alt->lo, &t->end_lo)) {
        return false;
      }
      t->cook = pick_cook;
      t->placed = true;
      left--;
      if (pick_cook != 0) {
        cook_free[pick_cook] = t->end_hi;
      }
      for (uint32_t g = 0; g < alt->ngroups; g++) {
        unit_free[s->groups[alt->first_group + g].pick] = t->end_hi;
      }
    }
    if (left == 0) {
      break;
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
    if (!found) {
      return false;
    }
    now = next;
  }
  return true;
}

bool ozni_schedule(ozni_arena *ar, ozni_sched *s) {
  uint32_t n = s->ntasks;
  uint32_t total = 0;
  for (uint32_t i = 0; i < n; i++) {
    total += s->tasks[i].ndeps;
  }
  uint32_t *first = ozni_arena_alloc(ar, (n + 1) * sizeof *first);
  uint32_t *count = ozni_arena_alloc(ar, (n + 1) * sizeof *count);
  uint32_t *deps = ozni_arena_alloc(ar, (total + 1) * sizeof *deps);
  if (first == nullptr || count == nullptr || deps == nullptr) {
    return false;
  }
  if (!s->backward) {
    for (uint32_t i = 0; i < n; i++) {
      first[i] = s->tasks[i].first_dep;
      count[i] = s->tasks[i].ndeps;
    }
    return run(ar, s, first, count, s->deps);
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
  return true;
}
