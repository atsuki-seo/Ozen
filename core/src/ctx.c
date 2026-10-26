/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#include "ctx.h"

bool ozni_ctx_init(ozni_ctx *c, const ozni_allocator *a, const ozni_limits *lim, const ozni_source *srcs,
                   size_t n) {
  *c = (ozni_ctx){.allocator = a, .lim = *lim, .srcs = srcs, .nsrcs = n};
  c->arena = ozni_arena_new(a);
  if (c->arena == nullptr) {
    return false;
  }
  c->names = ozni_intern_new(c->arena);
  c->diags = ozni_table_new(c->arena, sizeof(ozni_diag));
  c->diag_args = ozni_table_new(c->arena, sizeof(ozni_index));
  c->trails = ozni_table_new(c->arena, sizeof(ozni_trail));
  if (c->names == nullptr || c->diags == nullptr || c->diag_args == nullptr || c->trails == nullptr) {
    ozni_ctx_free(c);
    return false;
  }
  return true;
}

void ozni_ctx_free(ozni_ctx *c) {
  ozni_arena_free(c->arena);
  c->arena = nullptr;
}

bool ozni_diag_add(ozni_ctx *c, ozni_diag_id id, ozni_span span, ozni_index trail, const ozni_str *args,
                   uint16_t nargs) {
  ozni_diag d = {id, ozni_diag_default_severity(id), nargs, span, ozni_table_count(c->diag_args), trail};
  for (uint16_t i = 0; i < nargs; i++) {
    ozni_index sid;
    if (!ozni_intern_put(c->names, args[i].p, args[i].len, &sid) || !ozni_table_push(c->diag_args, &sid, nullptr)) {
      c->oom = true;
      return false;
    }
  }
  if (!ozni_table_push(c->diags, &d, nullptr)) {
    c->oom = true;
    return false;
  }
  return true;
}

ozni_str ozni_diag_arg(const ozni_ctx *c, const ozni_diag *d, uint16_t i) {
  const ozni_index *sid = ozni_table_get(c->diag_args, d->args + i);
  return sid != nullptr ? ozni_intern_get(c->names, *sid) : (ozni_str){"", 0};
}
