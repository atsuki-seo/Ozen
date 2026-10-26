/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#ifndef OZNI_INTERN_H
#define OZNI_INTERN_H

#include "arena.h"

typedef struct ozni_str {
  const char *p;
  uint32_t len;
} ozni_str;

typedef struct ozni_intern ozni_intern;

[[nodiscard]] ozni_intern *ozni_intern_new(ozni_arena *ar);
[[nodiscard]] bool ozni_intern_put(ozni_intern *in, const char *s, size_t len, ozni_index *id);
ozni_str ozni_intern_get(const ozni_intern *in, ozni_index id);
ozni_index ozni_intern_count(const ozni_intern *in);

#endif
