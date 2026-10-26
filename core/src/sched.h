/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#ifndef OZNI_SCHED_H
#define OZNI_SCHED_H

#include "arena.h"
#include "num.h"

typedef struct ozni_sched_alt {
  ozni_rat lo;
  ozni_rat hi;
  uint32_t first_group;
  uint32_t ngroups;
  uint64_t banned;
  bool hands;
} ozni_sched_alt;

typedef struct ozni_sched_task {
  uint32_t first_dep;
  uint32_t ndeps;
  uint32_t first_alt;
  uint32_t nalts;
  uint32_t order;
  ozni_rat start;
  ozni_rat end_hi;
  ozni_rat end_lo;
  uint32_t cook;
  uint32_t chosen;
  bool placed;
} ozni_sched_task;

typedef struct ozni_sched_group {
  uint32_t first_unit;
  uint32_t nunits;
  uint32_t pick;
} ozni_sched_group;

typedef struct ozni_sched {
  ozni_sched_task *tasks;
  uint32_t ntasks;
  const uint32_t *deps;
  const ozni_sched_alt *alts;
  ozni_sched_group *groups;
  const uint32_t *units;
  uint32_t nunits;
  uint32_t cooks;
  bool backward;
  ozni_rat serve_at;
} ozni_sched;

[[nodiscard]] bool ozni_schedule(ozni_arena *ar, ozni_sched *s);

#endif
