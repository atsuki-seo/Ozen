/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ozni.h"

static void *heap_alloc(void *ctx, size_t size) {
  (void)ctx;
  return malloc(size);
}

static void heap_release(void *ctx, void *ptr, size_t size) {
  (void)ctx;
  (void)size;
  free(ptr);
}

static int usage(void) {
  fputs("usage: ozen check FILE\n"
        "       ozen debug ast [--no-spans] [--rules] FILE\n",
        stderr);
  return 2;
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
    if (got == 0) {
      break;
    }
    if (n == cap) {
      char *bigger = realloc(buf, cap * 2);
      if (bigger == nullptr) {
        free(buf);
        buf = nullptr;
        break;
      }
      buf = bigger;
      cap *= 2;
    }
  }
  int failed = ferror(f);
  fclose(f);
  if (failed && buf != nullptr) {
    free(buf);
    return nullptr;
  }
  *len = n;
  return buf;
}

static int emit(const ozni_allocator *a, char *text, int has_errors) {
  if (text == nullptr) {
    fputs("ozen: out of memory\n", stderr);
    return 2;
  }
  fputs(text, stdout);
  ozni_free_text(a, text);
  return has_errors ? 1 : 0;
}

int main(int argc, char **argv) {
  ozni_allocator a = {heap_alloc, heap_release, nullptr};
  ozni_limits lim = ozni_default_limits();
  if (argc < 2) {
    return usage();
  }
  const char *path = nullptr;
  int ast = 0;
  unsigned flags = OZNI_AST_SPANS;
  if (strcmp(argv[1], "check") == 0 && argc == 3) {
    path = argv[2];
  } else if (strcmp(argv[1], "debug") == 0 && argc >= 4 && strcmp(argv[2], "ast") == 0) {
    ast = 1;
    for (int i = 3; i < argc; i++) {
      if (strcmp(argv[i], "--no-spans") == 0) {
        flags &= ~(unsigned)OZNI_AST_SPANS;
      } else if (strcmp(argv[i], "--rules") == 0) {
        flags |= OZNI_AST_RULES;
      } else if (argv[i][0] == '-' || path != nullptr) {
        return usage();
      } else {
        path = argv[i];
      }
    }
    if (path == nullptr) {
      return usage();
    }
  } else {
    return usage();
  }
  size_t len = 0;
  char *text = read_file(path, &len);
  if (text == nullptr) {
    fprintf(stderr, "ozen: cannot read %s\n", path);
    return 2;
  }
  ozni_source src = {path, text, len};
  int has_errors = 0;
  char *out = ast ? ozni_debug_ast_text(&a, &lim, &src, flags, &has_errors)
                  : ozni_check_text(&a, &lim, &src, 1, &has_errors);
  free(text);
  return emit(&a, out, has_errors);
}
