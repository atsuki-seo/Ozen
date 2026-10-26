/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#ifndef OZN_TEST_CHECK_H
#define OZN_TEST_CHECK_H

#include <stdio.h>
#include <stdlib.h>

#include "ozni.h"

extern int ozn_test_failures;

#define CHECK(cond)                                                            \
  do {                                                                         \
    if (!(cond)) {                                                             \
      fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
      ozn_test_failures++;                                                     \
    }                                                                          \
  } while (0)

typedef struct test_alloc {
  long live;
  unsigned long allocs;
  unsigned long fail_at;
} test_alloc;

ozni_allocator test_allocator(test_alloc *t);

#endif
