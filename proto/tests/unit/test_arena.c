/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#include <string.h>

#include "check.h"

void test_arena_basic(void) {
  test_alloc t = {0};
  ozp_allocator a = test_allocator(&t);
  ozpi_arena ar;
  ozpi_arena_init(&ar, &a);
  unsigned char *p = ozpi_arena_alloc(&ar, 10);
  unsigned char *q = ozpi_arena_alloc(&ar, 10);
  CHECK(p != nullptr && q != nullptr && p != q);
  CHECK(p[0] == 0 && p[9] == 0);
  memset(p, 0xab, 10);
  CHECK(q[0] == 0);
  CHECK(t.allocs == 1);
  ozpi_arena_free_all(&ar);
  CHECK(t.live == 0);
}

void test_arena_alignment(void) {
  test_alloc t = {0};
  ozp_allocator a = test_allocator(&t);
  ozpi_arena ar;
  ozpi_arena_init(&ar, &a);
  for (size_t n = 1; n < 40; n++) {
    void *p = ozpi_arena_alloc(&ar, n);
    CHECK(p != nullptr && (uintptr_t)p % _Alignof(max_align_t) == 0);
  }
  ozpi_arena_free_all(&ar);
  CHECK(t.live == 0);
}

void test_arena_large(void) {
  test_alloc t = {0};
  ozp_allocator a = test_allocator(&t);
  ozpi_arena ar;
  ozpi_arena_init(&ar, &a);
  unsigned char *big = ozpi_arena_alloc(&ar, 200000);
  CHECK(big != nullptr);
  if (big != nullptr) {
    big[199999] = 1;
  }
  CHECK(ozpi_arena_alloc(&ar, 8) != nullptr);
  CHECK(t.allocs == 2);
  ozpi_arena_free_all(&ar);
  CHECK(t.live == 0);
}

void test_arena_oom(void) {
  test_alloc t = {.fail_at = 2};
  ozp_allocator a = test_allocator(&t);
  ozpi_arena ar;
  ozpi_arena_init(&ar, &a);
  CHECK(ozpi_arena_alloc(&ar, 8) != nullptr);
  CHECK(ozpi_arena_alloc(&ar, 100000) == nullptr);
  CHECK(ar.failed);
  CHECK(ozpi_arena_alloc(&ar, 8) == nullptr);
  ozpi_arena_free_all(&ar);
  CHECK(t.live == 0);
}

void test_arena_poison_detects(void) {
  test_alloc t = {0};
  ozp_allocator a = test_allocator(&t);
  ozpi_arena ar;
  ozpi_arena_init(&ar, &a);
  volatile unsigned char *p = ozpi_arena_alloc(&ar, 8);
  unsigned char past = p[8];
  fprintf(stderr, "read past the allocation: %u\n", past);
  ozpi_arena_free_all(&ar);
}
