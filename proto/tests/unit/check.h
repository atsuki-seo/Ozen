/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#ifndef OZP_TEST_CHECK_H
#define OZP_TEST_CHECK_H

#include <stdio.h>
#include <stdlib.h>

#include "ozpi.h"

extern int ozp_test_failures;
extern int ozp_test_argc;
extern char **ozp_test_argv;

#define CHECK(cond)                                                       \
  do {                                                                    \
    if (!(cond)) {                                                        \
      fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
      ozp_test_failures++;                                                \
    }                                                                     \
  } while (0)

typedef struct test_alloc {
  long live;
  unsigned long allocs;
  unsigned long fail_at;
} test_alloc;

void *test_alloc_fn(void *ctx, size_t size);
void test_release_fn(void *ctx, void *ptr, size_t size);
ozp_allocator test_allocator(test_alloc *t);

#endif
