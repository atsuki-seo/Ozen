/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#include <string.h>

#include "arena.h"

#if defined(__SANITIZE_ADDRESS__)
#define OZNI_ASAN 1
#elif defined(__has_feature)
#if __has_feature(address_sanitizer)
#define OZNI_ASAN 1
#endif
#endif

#ifdef OZNI_ASAN
/* Declared here instead of including <sanitizer/asan_interface.h>, because the core may only
   include the C standard headers (S181). Every allocation is followed by a poisoned red zone so
   that reads past an arena array are reported by ASan. */
void __asan_poison_memory_region(void const volatile *addr, size_t size);
void __asan_unpoison_memory_region(void const volatile *addr, size_t size);
enum { OZNI_REDZONE = 16 };
#else
enum { OZNI_REDZONE = 0 };
#endif

enum { OZNI_CHUNK_SIZE = 64 * 1024, OZNI_TABLE_BASE = 64, OZNI_TABLE_BLOCKS = 26 };

struct ozni_arena_chunk {
  struct ozni_arena_chunk *next;
  size_t size;
  size_t used;
  max_align_t data[];
};

struct ozni_arena {
  const ozni_allocator *allocator;
  struct ozni_arena_chunk *head;
  uint32_t failures;
};

struct ozni_arena_table {
  ozni_arena *arena;
  size_t elem_size;
  ozni_index count;
  unsigned char *blocks[OZNI_TABLE_BLOCKS];
};

static size_t align_up(size_t n) {
  size_t a = _Alignof(max_align_t);
  return (n + a - 1) / a * a;
}

ozni_arena *ozni_arena_new(const ozni_allocator *a) {
  ozni_arena *ar = a->alloc(a->ctx, sizeof *ar);
  if (ar != nullptr) {
    *ar = (ozni_arena){a, nullptr, 0};
  }
  return ar;
}

void *ozni_arena_alloc(ozni_arena *ar, size_t size) {
  size_t need = align_up(size + OZNI_REDZONE);
  if (need < size) {
    ar->failures++;
    return nullptr;
  }
  struct ozni_arena_chunk *c = ar->head;
  if (c == nullptr || c->size - c->used < need) {
    size_t cap = need > OZNI_CHUNK_SIZE ? need : OZNI_CHUNK_SIZE;
    c = ar->allocator->alloc(ar->allocator->ctx, sizeof(struct ozni_arena_chunk) + cap);
    if (c == nullptr) {
      ar->failures++;
      return nullptr;
    }
    c->next = ar->head;
    c->size = cap;
    c->used = 0;
    ar->head = c;
#ifdef OZNI_ASAN
    __asan_poison_memory_region(c->data, cap);
#endif
  }
  unsigned char *p = (unsigned char *)c->data + c->used;
  c->used += need;
#ifdef OZNI_ASAN
  __asan_unpoison_memory_region(p, size);
#endif
  memset(p, 0, size);
  return p;
}

uint32_t ozni_arena_failures(const ozni_arena *ar) { return ar->failures; }

void ozni_arena_free(ozni_arena *ar) {
  if (ar == nullptr) {
    return;
  }
  struct ozni_arena_chunk *c = ar->head;
  while (c != nullptr) {
    struct ozni_arena_chunk *next = c->next;
#ifdef OZNI_ASAN
    __asan_unpoison_memory_region(c->data, c->size);
#endif
    ar->allocator->release(ar->allocator->ctx, c, sizeof(struct ozni_arena_chunk) + c->size);
    c = next;
  }
  ar->allocator->release(ar->allocator->ctx, ar, sizeof *ar);
}

ozni_table *ozni_table_new(ozni_arena *ar, size_t elem_size) {
  ozni_table *t = ozni_arena_alloc(ar, sizeof *t);
  if (t != nullptr) {
    t->arena = ar;
    t->elem_size = elem_size;
  }
  return t;
}

static unsigned char *slot(const ozni_table *t, ozni_index i) {
  uint64_t start = 0;
  for (unsigned k = 0; k < OZNI_TABLE_BLOCKS; k++) {
    uint64_t size = (uint64_t)OZNI_TABLE_BASE << k;
    if (i < start + size) {
      return t->blocks[k] == nullptr ? nullptr : t->blocks[k] + (size_t)(i - start) * t->elem_size;
    }
    start += size;
  }
  return nullptr;
}

bool ozni_table_push(ozni_table *t, const void *elem, ozni_index *out) {
  if (t->count == UINT32_MAX) {
    return false;
  }
  uint64_t start = 0;
  for (unsigned k = 0; k < OZNI_TABLE_BLOCKS; k++) {
    uint64_t size = (uint64_t)OZNI_TABLE_BASE << k;
    if (t->count < start + size) {
      if (t->blocks[k] == nullptr) {
        t->blocks[k] = ozni_arena_alloc(t->arena, (size_t)size * t->elem_size);
        if (t->blocks[k] == nullptr) {
          return false;
        }
      }
      break;
    }
    start += size;
  }
  unsigned char *p = slot(t, t->count);
  if (p == nullptr) {
    return false;
  }
  memcpy(p, elem, t->elem_size);
  if (out != nullptr) {
    *out = t->count;
  }
  t->count++;
  return true;
}

void *ozni_table_at(ozni_table *t, ozni_index i) { return i < t->count ? slot(t, i) : nullptr; }

const void *ozni_table_get(const ozni_table *t, ozni_index i) { return i < t->count ? slot(t, i) : nullptr; }

ozni_index ozni_table_count(const ozni_table *t) { return t->count; }
