/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#ifndef OZNI_SCHED_H
#define OZNI_SCHED_H

#include "arena.h"
#include "num.h"

enum { OZNI_CLEAN_NONE, OZNI_CLEAN_RINSE, OZNI_CLEAN_WASH, OZNI_CLEAN_SOAK_WASH };

enum { OZNI_WHEN_DEFAULT, OZNI_WHEN_SAME_FOOD, OZNI_WHEN_SAME_TOOL, OZNI_WHEN_TARGET };

enum { OZNI_UNIT_OTHER, OZNI_UNIT_TOOL, OZNI_UNIT_PLACE, OZNI_UNIT_VESSEL };

enum { OZNI_EXTRA_WASH, OZNI_EXTRA_RINSE, OZNI_EXTRA_SOAK, OZNI_EXTRA_WIPE, OZNI_EXTRA_RUN };

enum { OZNI_CLEANUP_NONE, OZNI_CLEANUP_BEFORE_MEAL, OZNI_CLEANUP_AFTER_MEAL };

typedef struct ozni_sched_alt {
  ozni_rat lo;
  ozni_rat hi;
  uint32_t first_group;
  uint32_t ngroups;
  uint32_t first_mark;
  uint32_t nmarks;
  uint64_t banned;
  uint64_t hold_mask;
  bool hands;
} ozni_sched_alt;

typedef struct ozni_sched_task {
  uint32_t first_dep;
  uint32_t ndeps;
  uint32_t first_alt;
  uint32_t nalts;
  uint32_t order;
  uint32_t food;
  uint64_t targets;
  ozni_rat start;
  ozni_rat end_hi;
  ozni_rat end_lo;
  ozni_rat release;
  ozni_rat hold_until;
  uint32_t cook;
  uint32_t chosen;
  bool placed;
  bool cleanup;
  bool has_release;
  bool has_hold;
} ozni_sched_task;

typedef struct ozni_sched_group {
  uint32_t first_unit;
  uint32_t nunits;
  uint32_t pick;
  uint8_t wash;
} ozni_sched_group;

typedef struct ozni_sched_mark {
  uint32_t dirt;
  uint32_t group;
  uint32_t food;
} ozni_sched_mark;

typedef struct ozni_sched_case {
  uint32_t rule;
  uint8_t level;
  uint8_t when;
  uint64_t bits;
} ozni_sched_case;

typedef struct ozni_sched_dirt {
  uint32_t first_case;
  uint32_t ncases;
} ozni_sched_dirt;

typedef struct ozni_sched_unit {
  ozni_rat clean_lo;
  ozni_rat clean_hi;
  uint8_t category;
  bool sink;
  bool dishwasher;
} ozni_sched_unit;

typedef struct ozni_sched_extra {
  uint8_t kind;
  uint8_t level;
  uint32_t unit;
  uint32_t sink;
  uint32_t cook;
  uint32_t dirt;
  uint32_t task;
  uint32_t first_tool;
  uint32_t ntools;
  ozni_rat start;
  ozni_rat end_hi;
  ozni_rat end_lo;
} ozni_sched_extra;

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
  const ozni_sched_mark *marks;
  const ozni_sched_case *cases;
  const ozni_sched_dirt *dirts;
  const ozni_sched_unit *unit_info;
  ozni_rat soak_lo;
  ozni_rat soak_hi;
  uint8_t cleanup;
  ozni_sched_extra *extras;
  uint32_t nextras;
  uint32_t extras_cap;
  uint32_t *tools;
  uint32_t ntools;
  const ozni_rat *blocks;
  uint32_t nblocks;
  ozni_rat first_serve;
  ozni_rat last_serve;
  bool serves_known;
} ozni_sched;

[[nodiscard]] bool ozni_schedule(ozni_arena *ar, ozni_sched *s);

#endif
