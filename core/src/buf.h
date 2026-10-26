/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#ifndef OZNI_BUF_H
#define OZNI_BUF_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ozni.h"

typedef struct ozni_buf {
  const ozni_allocator *a;
  char *p;
  size_t len;
  size_t cap;
  bool failed;
} ozni_buf;

void ozni_buf_put(ozni_buf *b, const char *s, size_t n);
void ozni_buf_puts(ozni_buf *b, const char *s);
void ozni_buf_putu(ozni_buf *b, uint64_t v);
void ozni_buf_quoted(ozni_buf *b, const char *s, size_t n);
void ozni_buf_json(ozni_buf *b, const char *s, size_t n);
char *ozni_buf_finish(ozni_buf *b);
void ozni_buf_discard(ozni_buf *b);

#endif
