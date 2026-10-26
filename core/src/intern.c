/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#include <string.h>

#include "intern.h"

struct ozni_intern {
  ozni_arena *arena;
  ozni_table *strings;
  ozni_index *slots;
  uint32_t cap;
};

static uint32_t hash(const char *s, size_t len) {
  uint32_t h = 2166136261u;
  for (size_t i = 0; i < len; i++) {
    h = (h ^ (unsigned char)s[i]) * 16777619u;
  }
  return h;
}

ozni_intern *ozni_intern_new(ozni_arena *ar) {
  ozni_intern *in = ozni_arena_alloc(ar, sizeof *in);
  if (in == nullptr) {
    return nullptr;
  }
  in->arena = ar;
  in->strings = ozni_table_new(ar, sizeof(ozni_str));
  in->cap = 256;
  in->slots = ozni_arena_alloc(ar, in->cap * sizeof(ozni_index));
  ozni_str none = {"", 0};
  if (in->strings == nullptr || in->slots == nullptr || !ozni_table_push(in->strings, &none, nullptr)) {
    return nullptr;
  }
  return in;
}

static bool grow(ozni_intern *in) {
  uint32_t cap = in->cap * 2;
  ozni_index *slots = ozni_arena_alloc(in->arena, cap * sizeof(ozni_index));
  if (slots == nullptr) {
    return false;
  }
  for (ozni_index id = 1; id < ozni_table_count(in->strings); id++) {
    const ozni_str *s = ozni_table_get(in->strings, id);
    uint32_t k = hash(s->p, s->len) & (cap - 1);
    while (slots[k] != 0) {
      k = (k + 1) & (cap - 1);
    }
    slots[k] = id;
  }
  in->slots = slots;
  in->cap = cap;
  return true;
}

bool ozni_intern_put(ozni_intern *in, const char *s, size_t len, ozni_index *id) {
  if (len > UINT32_MAX) {
    return false;
  }
  uint32_t k = hash(s, len) & (in->cap - 1);
  while (in->slots[k] != 0) {
    const ozni_str *e = ozni_table_get(in->strings, in->slots[k]);
    if (e->len == len && memcmp(e->p, s, len) == 0) {
      *id = in->slots[k];
      return true;
    }
    k = (k + 1) & (in->cap - 1);
  }
  if ((uint64_t)ozni_table_count(in->strings) * 10 >= (uint64_t)in->cap * 7) {
    if (!grow(in)) {
      return false;
    }
    return ozni_intern_put(in, s, len, id);
  }
  char *copy = ozni_arena_alloc(in->arena, len + 1);
  if (copy == nullptr) {
    return false;
  }
  memcpy(copy, s, len);
  ozni_str e = {copy, (uint32_t)len};
  ozni_index fresh;
  if (!ozni_table_push(in->strings, &e, &fresh)) {
    return false;
  }
  in->slots[k] = fresh;
  *id = fresh;
  return true;
}

ozni_str ozni_intern_get(const ozni_intern *in, ozni_index id) {
  const ozni_str *s = ozni_table_get(in->strings, id);
  return s != nullptr ? *s : (ozni_str){"", 0};
}

ozni_index ozni_intern_count(const ozni_intern *in) { return ozni_table_count(in->strings) - 1; }
