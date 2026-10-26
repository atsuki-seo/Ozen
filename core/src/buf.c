/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#include <string.h>

#include "buf.h"

void ozni_buf_put(ozni_buf *b, const char *s, size_t n) {
  if (b->failed) {
    return;
  }
  if (b->len + n + 1 > b->cap) {
    size_t cap = b->cap == 0 ? 256 : b->cap;
    while (cap < b->len + n + 1) {
      cap *= 2;
    }
    char *p = b->a->alloc(b->a->ctx, cap);
    if (p == nullptr) {
      b->failed = true;
      return;
    }
    if (b->p != nullptr) {
      memcpy(p, b->p, b->len);
      b->a->release(b->a->ctx, b->p, b->cap);
    }
    b->p = p;
    b->cap = cap;
  }
  memcpy(b->p + b->len, s, n);
  b->len += n;
  b->p[b->len] = '\0';
}

void ozni_buf_puts(ozni_buf *b, const char *s) { ozni_buf_put(b, s, strlen(s)); }

void ozni_buf_putu(ozni_buf *b, uint64_t v) {
  char rev[24];
  size_t n = 0;
  do {
    rev[n++] = (char)('0' + v % 10);
    v /= 10;
  } while (v != 0);
  char out[24];
  for (size_t i = 0; i < n; i++) {
    out[i] = rev[n - 1 - i];
  }
  ozni_buf_put(b, out, n);
}

void ozni_buf_quoted(ozni_buf *b, const char *s, size_t n) {
  ozni_buf_put(b, "\"", 1);
  size_t start = 0;
  for (size_t i = 0; i < n; i++) {
    const char *rep = s[i] == '"' ? "\\\"" : s[i] == '\\' ? "\\\\" : s[i] == '\n' ? "\\n" : nullptr;
    if (rep != nullptr) {
      ozni_buf_put(b, s + start, i - start);
      ozni_buf_puts(b, rep);
      start = i + 1;
    }
  }
  ozni_buf_put(b, s + start, n - start);
  ozni_buf_put(b, "\"", 1);
}

void ozni_buf_json(ozni_buf *b, const char *s, size_t n) {
  static const char hex[] = "0123456789abcdef";
  ozni_buf_put(b, "\"", 1);
  size_t start = 0;
  for (size_t i = 0; i < n; i++) {
    unsigned char ch = (unsigned char)s[i];
    if (ch != '"' && ch != '\\' && ch >= 0x20) {
      continue;
    }
    ozni_buf_put(b, s + start, i - start);
    if (ch == '"' || ch == '\\') {
      char esc[2] = {'\\', (char)ch};
      ozni_buf_put(b, esc, 2);
    } else {
      char esc[6] = {'\\', 'u', '0', '0', hex[ch >> 4], hex[ch & 15]};
      ozni_buf_put(b, esc, 6);
    }
    start = i + 1;
  }
  ozni_buf_put(b, s + start, n - start);
  ozni_buf_put(b, "\"", 1);
}

char *ozni_buf_finish(ozni_buf *b) {
  if (b->failed) {
    ozni_buf_discard(b);
    return nullptr;
  }
  char *out = b->a->alloc(b->a->ctx, b->len + 1);
  if (out != nullptr) {
    if (b->len > 0) {
      memcpy(out, b->p, b->len);
    }
    out[b->len] = '\0';
  }
  ozni_buf_discard(b);
  return out;
}

void ozni_buf_discard(ozni_buf *b) {
  if (b->p != nullptr) {
    b->a->release(b->a->ctx, b->p, b->cap);
  }
  b->p = nullptr;
  b->len = b->cap = 0;
}
