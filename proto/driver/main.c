/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ozenproto.h"

typedef struct counter {
  unsigned long allocs;
  unsigned long fail_at;
  long live;
} counter;

static void *count_alloc(void *ctx, size_t size) {
  counter *c = ctx;
  c->allocs++;
  if (c->fail_at != 0 && c->allocs == c->fail_at) {
    return nullptr;
  }
  void *p = malloc(size);
  if (p != nullptr) {
    c->live++;
  }
  return p;
}

static void count_release(void *ctx, void *ptr, size_t size) {
  counter *c = ctx;
  (void)size;
  if (ptr != nullptr) {
    c->live--;
    free(ptr);
  }
}

static int usage(void) {
  fputs("usage: ozenproto [--entry NAME] [--param name=expr]... [--limit-steps N] [--limit-depth N]\n"
        "                 [--limit-nodes N] [--limit-input N] [--stats] [--count-allocs] [--fail-alloc-at N] FILE\n",
        stderr);
  return 2;
}

static bool parse_u32(const char *s, uint32_t *out) {
  char *end = nullptr;
  errno = 0;
  unsigned long v = strtoul(s, &end, 10);
  if (errno != 0 || end == s || *end != '\0' || v > UINT32_MAX) {
    return false;
  }
  *out = (uint32_t)v;
  return true;
}

static char *read_file(const char *path, size_t *len) {
  FILE *f = fopen(path, "rb");
  if (f == nullptr) {
    return nullptr;
  }
  size_t cap = 4096, n = 0;
  char *buf = malloc(cap);
  while (buf != nullptr) {
    size_t got = fread(buf + n, 1, cap - n, f);
    n += got;
    if (n < cap) {
      break;
    }
    char *grown = realloc(buf, cap * 2);
    if (grown == nullptr) {
      free(buf);
      buf = nullptr;
      break;
    }
    buf = grown;
    cap *= 2;
  }
  bool bad = buf == nullptr || ferror(f);
  fclose(f);
  if (bad) {
    free(buf);
    return nullptr;
  }
  *len = n;
  return buf;
}

int main(int argc, char **argv) {
  ozp_options opt = {0};
  opt.limits = ozp_default_limits();
  counter cnt = {0};
  ozp_allocator allocator = {count_alloc, count_release, &cnt};
  opt.allocator = &allocator;
  bool count_allocs = false;
  const char *file = nullptr;
  size_t params_len = 0;
  char *params = nullptr;
  int rc = 2;
  for (int i = 1; i < argc; i++) {
    const char *a = argv[i];
    const char *v = i + 1 < argc ? argv[i + 1] : nullptr;
    uint32_t n = 0;
    if (strcmp(a, "--entry") == 0 && v != nullptr) {
      opt.entry = v;
      i++;
    } else if (strcmp(a, "--param") == 0 && v != nullptr) {
      size_t vl = strlen(v);
      char *grown = realloc(params, params_len + vl + 2);
      if (grown == nullptr) {
        goto done;
      }
      params = grown;
      if (params_len > 0) {
        params[params_len++] = ';';
      }
      memcpy(params + params_len, v, vl + 1);
      params_len += vl;
      i++;
    } else if (strcmp(a, "--limit-steps") == 0 && v != nullptr && parse_u32(v, &n)) {
      opt.limits.steps = n;
      i++;
    } else if (strcmp(a, "--limit-depth") == 0 && v != nullptr && parse_u32(v, &n)) {
      opt.limits.depth = n;
      i++;
    } else if (strcmp(a, "--limit-nodes") == 0 && v != nullptr && parse_u32(v, &n)) {
      opt.limits.nodes = n;
      i++;
    } else if (strcmp(a, "--limit-input") == 0 && v != nullptr && parse_u32(v, &n)) {
      opt.limits.input_bytes = n;
      i++;
    } else if (strcmp(a, "--fail-alloc-at") == 0 && v != nullptr && parse_u32(v, &n)) {
      cnt.fail_at = n;
      i++;
    } else if (strcmp(a, "--stats") == 0) {
      opt.stats = 1;
    } else if (strcmp(a, "--count-allocs") == 0) {
      count_allocs = true;
    } else if (a[0] == '-' && a[1] == '-') {
      rc = usage();
      goto done;
    } else if (file == nullptr) {
      file = a;
    } else {
      rc = usage();
      goto done;
    }
  }
  if (file == nullptr) {
    rc = usage();
    goto done;
  }
  opt.params = params;
  size_t len = 0;
  char *src = read_file(file, &len);
  if (src == nullptr) {
    fprintf(stderr, "ozenproto: cannot read %s\n", file);
    goto done;
  }
  char *out = ozp_expand(src, len, &opt);
  free(src);
  if (out == nullptr) {
    fputs("ozenproto: out of memory\n", stderr);
    goto counts;
  }
  fputs(out, stdout);
  rc = strstr(out, "\nresult ok ") != nullptr || strncmp(out, "result ok ", 10) == 0 ? 0 : 1;
  ozp_free_string(&opt, out);
counts:
  if (count_allocs) {
    fprintf(stderr, "allocs: %lu live: %ld\n", cnt.allocs, cnt.live);
  }
done:
  free(params);
  return rc;
}
