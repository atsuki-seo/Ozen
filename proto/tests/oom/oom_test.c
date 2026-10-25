/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#include <errno.h>
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
  if (buf != nullptr) {
    buf[n] = '\0';
    *len = (size_t)n;
  }
  return buf;
}

static void apply_args(const char *src, ozp_options *opt, char *params, size_t cap) {
  if (strncmp(src, "// args: ", 9) != 0) {
    return;
  }
  const char *p = src + 9;
  const char *end = strchr(p, '\n');
  char line[512];
  size_t n = end != nullptr ? (size_t)(end - p) : strlen(p);
  if (n >= sizeof line) {
    n = sizeof line - 1;
  }
  memcpy(line, p, n);
  line[n] = '\0';
  for (char *tok = strtok(line, " "); tok != nullptr; tok = strtok(nullptr, " ")) {
    char *val = nullptr;
    if (strcmp(tok, "--stats") == 0) {
      opt->stats = 1;
      continue;
    }
    val = strtok(nullptr, " ");
    if (val == nullptr) {
      break;
    }
    unsigned long v = strtoul(val, nullptr, 10);
    if (strcmp(tok, "--limit-steps") == 0) {
      opt->limits.steps = (uint32_t)v;
    } else if (strcmp(tok, "--limit-depth") == 0) {
      opt->limits.depth = (uint32_t)v;
    } else if (strcmp(tok, "--limit-nodes") == 0) {
      opt->limits.nodes = (uint32_t)v;
    } else if (strcmp(tok, "--limit-input") == 0) {
      opt->limits.input_bytes = (uint32_t)v;
    } else if (strcmp(tok, "--param") == 0) {
      size_t used = strlen(params);
      snprintf(params + used, cap - used, "%s%s", used > 0 ? ";" : "", val);
      opt->params = params;
    }
  }
}

int main(int argc, char **argv) {
  unsigned long files = 0, points = 0, failures = 0;
  for (int i = 1; i < argc; i++) {
    size_t len = 0;
    char *src = slurp(argv[i], &len);
    if (src == nullptr) {
      fprintf(stderr, "cannot read %s\n", argv[i]);
      failures++;
      continue;
    }
    files++;
    counter cnt = {0};
    ozp_allocator a = {count_alloc, count_release, &cnt};
    char params[512] = "";
    ozp_options opt = {.allocator = &a, .limits = ozp_default_limits()};
    apply_args(src, &opt, params, sizeof params);
    char *base = ozp_expand(src, len, &opt);
    unsigned long total = cnt.allocs;
    if (base == nullptr || (ozp_free_string(&opt, base), cnt.live != 0)) {
      fprintf(stderr, "%s: baseline failed\n", argv[i]);
      failures++;
      free(src);
      continue;
    }
    cnt = (counter){0};
    char *again = ozp_expand(src, len, &opt);
    for (unsigned long at = 1; at <= total + 1; at++) {
      cnt = (counter){.fail_at = at};
      char *out = ozp_expand(src, len, &opt);
      bool ok;
      if (at <= total) {
        ok = out == nullptr || strstr(out, " MAC006 outOfMemory ") != nullptr;
      } else {
        ok = out != nullptr && again != nullptr && strcmp(out, again) == 0;
      }
      ozp_free_string(&opt, out);
      if (!ok || cnt.live != 0) {
        fprintf(stderr, "%s: failing allocation %lu of %lu: %s\n", argv[i], at, total,
                cnt.live != 0 ? "leak" : "unexpected output");
        failures++;
      }
    }
    points += total;
    cnt = (counter){0};
    ozp_free_string(&opt, again);
    free(src);
  }
  printf("oom: %lu files, %lu points, %lu failures\n", files, points, failures);
  return failures == 0 ? 0 : 1;
}
