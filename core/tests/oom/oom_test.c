/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ozni.h"

typedef struct counting {
  unsigned long allocs;
  unsigned long fail_at;
  long live;
} counting;

static void *count_alloc(void *ctx, size_t size) {
  counting *c = ctx;
  if (++c->allocs == c->fail_at) {
    return nullptr;
  }
  void *p = malloc(size);
  if (p != nullptr) {
    c->live++;
  }
  return p;
}

static void count_release(void *ctx, void *ptr, size_t size) {
  counting *c = ctx;
  (void)size;
  if (ptr != nullptr) {
    c->live--;
    free(ptr);
  }
}

static char *slurp(const char *path, size_t *len) {
  FILE *f = fopen(path, "rb");
  if (f == nullptr) {
    return nullptr;
  }
  fseek(f, 0, SEEK_END);
  long n = ftell(f);
  fseek(f, 0, SEEK_SET);
  char *buf = malloc((size_t)n + 1);
  if (buf != nullptr && fread(buf, 1, (size_t)n, f) != (size_t)n) {
    free(buf);
    buf = nullptr;
  }
  fclose(f);
  *len = (size_t)n;
  return buf;
}

static char *run(counting *c, const ozni_source *src) {
  ozni_allocator a = {count_alloc, count_release, c};
  ozni_limits lim = ozni_default_limits();
  int has_errors = 0;
  char *out = ozni_run_text(&a, &lim, src, 1, OZNI_MODE_CHECK, nullptr, &has_errors);
  char *copy = nullptr;
  if (out != nullptr) {
    size_t n = strlen(out);
    copy = malloc(n + 1);
    if (copy != nullptr) {
      memcpy(copy, out, n + 1);
    }
    ozni_free_text(&a, out);
  }
  return copy;
}

int main(int argc, char **argv) {
  unsigned long points = 0, failures = 0;
  int files = 0;
  for (int k = 1; k < argc; k++) {
    size_t len = 0;
    char *text = slurp(argv[k], &len);
    if (text == nullptr) {
      fprintf(stderr, "cannot read %s\n", argv[k]);
      failures++;
      continue;
    }
    files++;
    ozni_source src = {argv[k], text, len};
    counting base = {0, 0, 0};
    char *want = run(&base, &src);
    if (want == nullptr || base.live != 0) {
      fprintf(stderr, "%s: the run without failures returned nothing or leaked\n", argv[k]);
      failures++;
    }
    for (unsigned long i = 1; i <= base.allocs + 1; i++) {
      counting c = {0, i, 0};
      char *got = run(&c, &src);
      points++;
      bool ok = c.live == 0;
      if (i <= base.allocs) {
        ok = ok && (got == nullptr || strstr(got, " MAC004 outOfMemory ") != nullptr);
      } else {
        ok = ok && got != nullptr && want != nullptr && strcmp(got, want) == 0;
      }
      if (!ok) {
        fprintf(stderr, "%s: failing allocation %lu of %lu: %s\n", argv[k], i, base.allocs,
                c.live != 0 ? "leaked" : "unexpected result");
        failures++;
      }
      free(got);
    }
    free(want);
    free(text);
  }
  printf("oom: %d files, %lu points, %lu failures\n", files, points, failures);
  return failures == 0 && files > 0 ? 0 : 1;
}
