/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#ifndef OZNI_CTX_H
#define OZNI_CTX_H

#include "intern.h"

typedef struct ozni_span {
  ozni_index file;
  uint32_t start;
  uint32_t end;
} ozni_span;

typedef struct ozni_trail {
  ozni_span span;
  ozni_index prev;
} ozni_trail;

typedef struct ozni_diag {
  ozni_diag_id id;
  ozni_severity severity;
  uint16_t nargs;
  ozni_span span;
  ozni_index args;
  ozni_index trail;
} ozni_diag;

typedef struct ozni_ctx {
  const ozni_allocator *allocator;
  ozni_limits lim;
  ozni_settings set;
  ozni_arena *arena;
  ozni_intern *names;
  ozni_table *diags;
  ozni_table *diag_args;
  ozni_table *trails;
  const ozni_source *srcs;
  size_t nsrcs;
  size_t nuser;
  uint64_t nodes_used;
  uint64_t steps_used;
  bool limit_hit;
  bool oom;
} ozni_ctx;

[[nodiscard]] bool ozni_ctx_init(ozni_ctx *c, const ozni_allocator *a, const ozni_limits *lim,
                                 const ozni_source *srcs, size_t n);
void ozni_ctx_free(ozni_ctx *c);
bool ozni_diag_add(ozni_ctx *c, ozni_diag_id id, ozni_span span, ozni_index trail, const ozni_str *args,
                   uint16_t nargs);
ozni_str ozni_diag_arg(const ozni_ctx *c, const ozni_diag *d, uint16_t i);

#endif
