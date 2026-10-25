/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#include <string.h>

#include "ozpi.h"

#if defined(__SANITIZE_ADDRESS__)
#define OZPI_ASAN 1
#elif defined(__has_feature)
#if __has_feature(address_sanitizer)
#define OZPI_ASAN 1
#endif
#endif

#ifdef OZPI_ASAN
/* Declared here instead of including <sanitizer/asan_interface.h>, because the core may only
   include the standard headers listed in the plan (S117). Every allocation is followed by a
   poisoned red zone so that reads past an arena array are reported by ASan (S148). */
void __asan_poison_memory_region(void const volatile *addr, size_t size);
void __asan_unpoison_memory_region(void const volatile *addr, size_t size);
enum { OZPI_REDZONE = 16 };
#else
enum { OZPI_REDZONE = 0 };
#endif

enum { OZPI_CHUNK_SIZE = 64 * 1024 };

struct ozpi_chunk {
  ozpi_chunk *next;
  size_t size;
  size_t used;
  max_align_t data[];
};

static size_t align_up(size_t n) {
  size_t a = _Alignof(max_align_t);
  return (n + a - 1) / a * a;
}

void ozpi_arena_init(ozpi_arena *ar, const ozp_allocator *allocator) {
  ar->allocator = allocator;
  ar->head = nullptr;
  ar->failed = false;
}

void *ozpi_arena_alloc(ozpi_arena *ar, size_t size) {
  if (ar->failed) {
    return nullptr;
  }
  size_t need = align_up(size + OZPI_REDZONE);
  if (need < size) {
    ar->failed = true;
    return nullptr;
  }
  ozpi_chunk *c = ar->head;
  if (c == nullptr || c->size - c->used < need) {
    size_t cap = need > OZPI_CHUNK_SIZE ? need : OZPI_CHUNK_SIZE;
    c = ar->allocator->alloc(ar->allocator->ctx, sizeof(ozpi_chunk) + cap);
    if (c == nullptr) {
      ar->failed = true;
      return nullptr;
    }
    c->next = ar->head;
    c->size = cap;
    c->used = 0;
    ar->head = c;
#ifdef OZPI_ASAN
    __asan_poison_memory_region(c->data, cap);
#endif
  }
  unsigned char *p = (unsigned char *)c->data + c->used;
  c->used += need;
#ifdef OZPI_ASAN
  __asan_unpoison_memory_region(p, size);
#endif
  memset(p, 0, size);
  return p;
}

void ozpi_arena_free_all(ozpi_arena *ar) {
  ozpi_chunk *c = ar->head;
  while (c != nullptr) {
    ozpi_chunk *next = c->next;
#ifdef OZPI_ASAN
    __asan_unpoison_memory_region(c->data, c->size);
#endif
    ar->allocator->release(ar->allocator->ctx, c, sizeof(ozpi_chunk) + c->size);
    c = next;
  }
  ar->head = nullptr;
}
