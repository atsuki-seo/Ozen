/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#include <string.h>

#include "arena.h"
#include "check.h"

void test_arena_basic(void) {
  test_alloc t = {0};
  ozni_allocator a = test_allocator(&t);
  ozni_arena *ar = ozni_arena_new(&a);
  CHECK(ar != nullptr);
  unsigned char *p = ozni_arena_alloc(ar, 10);
  unsigned char *q = ozni_arena_alloc(ar, 10);
  CHECK(p != nullptr && q != nullptr && p != q);
  CHECK(p[0] == 0 && p[9] == 0);
  memset(p, 0xab, 10);
  CHECK(q[0] == 0);
  CHECK(t.allocs == 2);
  ozni_arena_free(ar);
  CHECK(t.live == 0);
}

void test_arena_alignment(void) {
  test_alloc t = {0};
  ozni_allocator a = test_allocator(&t);
  ozni_arena *ar = ozni_arena_new(&a);
  for (size_t n = 1; n < 40; n++) {
    void *p = ozni_arena_alloc(ar, n);
    CHECK(p != nullptr && (uintptr_t)p % _Alignof(max_align_t) == 0);
  }
  ozni_arena_free(ar);
  CHECK(t.live == 0);
}

void test_arena_large(void) {
  test_alloc t = {0};
  ozni_allocator a = test_allocator(&t);
  ozni_arena *ar = ozni_arena_new(&a);
  unsigned char *big = ozni_arena_alloc(ar, 200000);
  CHECK(big != nullptr);
  if (big != nullptr) {
    big[199999] = 1;
  }
  CHECK(ozni_arena_alloc(ar, 8) != nullptr);
  CHECK(t.allocs == 3);
  ozni_arena_free(ar);
  CHECK(t.live == 0);
}

void test_arena_oom(void) {
  test_alloc t = {.fail_at = 3};
  ozni_allocator a = test_allocator(&t);
  ozni_arena *ar = ozni_arena_new(&a);
  CHECK(ozni_arena_alloc(ar, 8) != nullptr);
  CHECK(ozni_arena_alloc(ar, 100000) == nullptr);
  CHECK(ozni_arena_failures(ar) == 1);
  CHECK(ozni_arena_alloc(ar, 100000) != nullptr);
  CHECK(ozni_arena_failures(ar) == 1);
  ozni_arena_free(ar);
  CHECK(t.live == 0);
}

#if defined(__SANITIZE_ADDRESS__)
#define TEST_ASAN 1
#elif defined(__has_feature)
#if __has_feature(address_sanitizer)
#define TEST_ASAN 1
#endif
#endif

void test_arena_poison_detects(void) {
#ifndef TEST_ASAN
  fputs("skipped: the red zones are poisoned only in ASan builds\n", stderr);
  return;
#endif
  test_alloc t = {0};
  ozni_allocator a = test_allocator(&t);
  ozni_arena *ar = ozni_arena_new(&a);
  volatile unsigned char *p = ozni_arena_alloc(ar, 24);
  (void)ozni_arena_alloc(ar, 24);
  unsigned char past = p[24];
  ozni_arena_free(ar);
  CHECK(past == 0);
}

void test_index_is_32bit(void) {
  CHECK(sizeof(ozni_index) == 4);
  test_alloc t = {0};
  ozni_allocator a = test_allocator(&t);
  ozni_arena *ar = ozni_arena_new(&a);
  ozni_table *tab = ozni_table_new(ar, sizeof(uint64_t));
  int *first = nullptr;
  for (uint64_t v = 0; v < 1000; v++) {
    ozni_index i = 0;
    CHECK(ozni_table_push(tab, &v, &i) && i == (ozni_index)v);
    if (v == 0) {
      first = ozni_table_at(tab, 0);
    }
  }
  CHECK(ozni_table_count(tab) == 1000);
  CHECK(first == ozni_table_at(tab, 0));
  const uint64_t *last = ozni_table_get(tab, 999);
  CHECK(last != nullptr && *last == 999);
  CHECK(ozni_table_at(tab, 1000) == nullptr);
  CHECK(ozni_table_get(tab, UINT32_MAX) == nullptr);
  ozni_arena_free(ar);
  CHECK(t.live == 0);
}
