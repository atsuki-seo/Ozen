/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#include <string.h>

#include "core.h"

void ozpi_oom(ozpi_ctx *c) {
  if (c->oom) {
    return;
  }
  c->oom = true;
  c->stop = true;
  if (c->ndiags < c->capdiags) {
    c->diags[c->ndiags++] = (ozpi_diag){.id = OZPI_D_MAC_outOfMemory};
  }
}

void *ozpi_new(ozpi_ctx *c, size_t size) {
  void *p = ozpi_arena_alloc(&c->ar, size);
  if (p == nullptr) {
    ozpi_oom(c);
  }
  return p;
}

bool ozpi_grow(ozpi_ctx *c, void **arr, uint32_t *cap, size_t elem, uint32_t need) {
  if (need <= *cap) {
    return true;
  }
  uint32_t ncap = *cap < 8 ? 8 : *cap * 2;
  while (ncap < need) {
    ncap *= 2;
  }
  void *p = ozpi_new(c, elem * ncap);
  if (p == nullptr) {
    return false;
  }
  if (*cap > 0) {
    memcpy(p, *arr, elem * *cap);
  }
  *arr = p;
  *cap = ncap;
  return true;
}

bool ozpi_str_eq(ozpi_str a, ozpi_str b) { return a.len == b.len && (a.len == 0 || memcmp(a.p, b.p, a.len) == 0); }

bool ozpi_str_eqz(ozpi_str a, const char *z) { return ozpi_str_eq(a, ozpi_strz(z)); }

size_t ozpi_strlen(const char *z) {
  size_t n = 0;
  while (z[n] != '\0') {
    n++;
  }
  return n;
}

ozpi_str ozpi_strz(const char *z) { return (ozpi_str){z, (uint32_t)ozpi_strlen(z)}; }

ozpi_str ozpi_str_dup(ozpi_ctx *c, const char *p, size_t len) {
  char *q = ozpi_new(c, len + 1);
  if (q == nullptr) {
    return (ozpi_str){"", 0};
  }
  if (len > 0) {
    memcpy(q, p, len);
  }
  return (ozpi_str){q, (uint32_t)len};
}

void ozpi_diag_add(ozpi_ctx *c, ozpi_diag_id id, ozpi_span span, uint32_t exp, const ozpi_str *args, uint32_t nargs) {
  if (c->oom) {
    return;
  }
  if (!ozpi_grow(c, (void **)&c->diags, &c->capdiags, sizeof(ozpi_diag), c->ndiags + 2)) {
    return;
  }
  ozpi_str *copy = nullptr;
  if (nargs > 0) {
    copy = ozpi_new(c, sizeof(ozpi_str) * nargs);
    if (copy == nullptr) {
      return;
    }
    for (uint32_t i = 0; i < nargs; i++) {
      copy[i] = ozpi_str_dup(c, args[i].p, args[i].len);
    }
    if (c->oom) {
      return;
    }
  }
  c->diags[c->ndiags++] = (ozpi_diag){id, span, exp, copy, nargs};
}

void ozpi_diag0(ozpi_ctx *c, ozpi_diag_id id, ozpi_span span, uint32_t exp) { ozpi_diag_add(c, id, span, exp, nullptr, 0); }

void ozpi_diag1(ozpi_ctx *c, ozpi_diag_id id, ozpi_span span, uint32_t exp, ozpi_str a) {
  ozpi_diag_add(c, id, span, exp, &a, 1);
}

void ozpi_diag2(ozpi_ctx *c, ozpi_diag_id id, ozpi_span span, uint32_t exp, ozpi_str a, ozpi_str b) {
  ozpi_str v[2] = {a, b};
  ozpi_diag_add(c, id, span, exp, v, 2);
}

void ozpi_diag3(ozpi_ctx *c, ozpi_diag_id id, ozpi_span span, uint32_t exp, ozpi_str a, ozpi_str b, ozpi_str d) {
  ozpi_str v[3] = {a, b, d};
  ozpi_diag_add(c, id, span, exp, v, 3);
}

bool ozpi_buf_put(ozpi_ctx *c, ozpi_buf *b, const char *p, size_t len) {
  if (b->len + len + 1 > b->cap) {
    size_t ncap = b->cap < 64 ? 64 : b->cap * 2;
    while (ncap < b->len + len + 1) {
      ncap *= 2;
    }
    char *q = ozpi_new(c, ncap);
    if (q == nullptr) {
      return false;
    }
    if (b->len > 0) {
      memcpy(q, b->p, b->len);
    }
    b->p = q;
    b->cap = ncap;
  }
  if (len > 0) {
    memcpy(b->p + b->len, p, len);
  }
  b->len += len;
  b->p[b->len] = '\0';
  return true;
}

bool ozpi_buf_puts(ozpi_ctx *c, ozpi_buf *b, const char *z) { return ozpi_buf_put(c, b, z, ozpi_strlen(z)); }

bool ozpi_buf_putu(ozpi_ctx *c, ozpi_buf *b, uint64_t v) {
  char tmp[24];
  size_t n = 0;
  do {
    tmp[sizeof tmp - 1 - n++] = (char)('0' + v % 10);
    v /= 10;
  } while (v != 0);
  return ozpi_buf_put(c, b, tmp + sizeof tmp - n, n);
}

bool ozpi_buf_puti(ozpi_ctx *c, ozpi_buf *b, int64_t v) {
  if (v < 0) {
    return ozpi_buf_put(c, b, "-", 1) && ozpi_buf_putu(c, b, (uint64_t)0 - (uint64_t)v);
  }
  return ozpi_buf_putu(c, b, (uint64_t)v);
}

bool ozpi_buf_putrat(ozpi_ctx *c, ozpi_buf *b, ozpi_rat r) {
  if (!ozpi_buf_puti(c, b, r.num)) {
    return false;
  }
  if (r.den != 1) {
    return ozpi_buf_put(c, b, "/", 1) && ozpi_buf_putu(c, b, (uint64_t)r.den);
  }
  return true;
}

ozpi_str ozpi_buf_str(ozpi_buf *b) { return b->p == nullptr ? (ozpi_str){"", 0} : (ozpi_str){b->p, (uint32_t)b->len}; }

ozpi_str ozpi_fmt_u(ozpi_ctx *c, uint64_t v) {
  ozpi_buf b = {0};
  ozpi_buf_putu(c, &b, v);
  return ozpi_buf_str(&b);
}
