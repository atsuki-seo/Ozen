/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#define _POSIX_C_SOURCE 200809L
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "ozni.h"

#ifndef OZEN_VERSION
#define OZEN_VERSION "0.0.0"
#endif

typedef struct heap {
  unsigned long count;
  unsigned long fail_at;
} heap;

static void *heap_alloc(void *ctx, size_t size) {
  heap *h = ctx;
  if (++h->count == h->fail_at) {
    return nullptr;
  }
  return malloc(size);
}

static void heap_release(void *ctx, void *ptr, size_t size) {
  (void)ctx;
  (void)size;
  free(ptr);
}

static int usage(void) {
  fputs("usage: ozen check [OPTIONS] [FILE]\n"
        "       ozen debug ast [--no-spans] [--rules] FILE\n"
        "       ozen debug modules\n"
        "       ozen debug definitions [FILE]\n"
        "       ozen debug steps [FILE]\n"
        "       ozen debug requires [FILE]\n"
        "       ozen debug ir [FILE]\n"
        "       ozen debug entry [--entry NAME] [FILE]\n"
        "       ozen debug embedded-std\n"
        "       ozen --version\n"
        "options: --strict --allow NAME --warn NAME --deny NAME --fixes --kitchen NAME --options NAME\n"
        "         --entry NAME --max-input-bytes N --max-depth N --max-nodes N --max-steps N --fail-alloc-at N\n",
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

typedef struct file_list {
  char **paths;
  size_t n;
  size_t cap;
} file_list;

static int add_path(file_list *l, const char *path) {
  if (l->n == l->cap) {
    size_t cap = l->cap == 0 ? 64 : l->cap * 2;
    char **p = realloc(l->paths, cap * sizeof *p);
    if (p == nullptr) {
      return -1;
    }
    l->paths = p;
    l->cap = cap;
  }
  size_t n = strlen(path);
  l->paths[l->n] = malloc(n + 1);
  if (l->paths[l->n] == nullptr) {
    return -1;
  }
  memcpy(l->paths[l->n++], path, n + 1);
  return 0;
}

static int walk(file_list *l, const char *dir) {
  DIR *d = opendir(dir[0] == '\0' ? "." : dir);
  if (d == nullptr) {
    return -1;
  }
  struct dirent *e;
  int rc = 0;
  while (rc == 0 && (e = readdir(d)) != nullptr) {
    if (e->d_name[0] == '.' || (dir[0] == '\0' && strcmp(e->d_name, "out") == 0)) {
      continue;
    }
    char path[4096];
    int n = snprintf(path, sizeof path, "%s%s%s", dir, dir[0] == '\0' ? "" : "/", e->d_name);
    if (n < 0 || (size_t)n >= sizeof path) {
      rc = -1;
      break;
    }
    struct stat st;
    if (stat(path, &st) != 0) {
      continue;
    }
    if (S_ISDIR(st.st_mode)) {
      rc = walk(l, path);
    } else {
      size_t len = strlen(path);
      if (len > 5 && strcmp(path + len - 5, ".ozen") == 0) {
        rc = add_path(l, path);
      }
    }
  }
  closedir(d);
  return rc;
}

static bool stat_file(const char *path) {
  struct stat st;
  return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

static int cmp_path(const void *a, const void *b) { return strcmp(*(char *const *)a, *(char *const *)b); }

static int emit(const ozni_allocator *a, char *text, int has_errors) {
  if (text == nullptr) {
    fputs("ozen: out of memory\n", stderr);
    return 2;
  }
  fputs(text, stdout);
  ozni_free_text(a, text);
  return has_errors ? 1 : 0;
}

static ozni_limits limits;
static ozni_settings settings;

static int run(const ozni_allocator *a, ozni_mode mode, const char *file, const char *entry) {
  ozni_limits lim = limits;
  file_list l = {0};
  if (file != nullptr) {
    if (add_path(&l, file) != 0) {
      return 2;
    }
  } else if (stat_file("package.ozen")) {
    if (walk(&l, "") != 0) {
      fputs("ozen: cannot read the project folder\n", stderr);
      return 2;
    }
  } else if (mode == OZNI_MODE_CHECK || mode == OZNI_MODE_MODULES || mode == OZNI_MODE_STEPS ||
             mode == OZNI_MODE_REQUIRES || mode == OZNI_MODE_IR) {
    fputs("ozen: no FILE given and no package.ozen in this folder\n", stderr);
    return 2;
  }
  qsort(l.paths, l.n, sizeof *l.paths, cmp_path);
  ozni_source *srcs = calloc(l.n + 1, sizeof *srcs);
  int status = srcs == nullptr ? 2 : -1;
  for (size_t i = 0; i < l.n && status == -1; i++) {
    size_t len = 0;
    char *text = read_file(l.paths[i], &len);
    if (text == nullptr) {
      fprintf(stderr, "ozen: cannot read %s\n", l.paths[i]);
      status = 2;
      break;
    }
    srcs[i] = (ozni_source){l.paths[i], text, len};
  }
  if (status == -1) {
    int has_errors = 0;
    char *out = ozni_run_text(a, &lim, &settings, srcs, l.n, mode, entry, &has_errors);
    status = emit(a, out, has_errors);
  }
  for (size_t i = 0; i < l.n; i++) {
    if (srcs != nullptr) {
      free((void *)srcs[i].text);
    }
    free(l.paths[i]);
  }
  free(srcs);
  free(l.paths);
  return status;
}

static int debug_ast(const ozni_allocator *a, int argc, char **argv) {
  const char *path = nullptr;
  unsigned flags = OZNI_AST_SPANS;
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
  size_t len = 0;
  char *text = read_file(path, &len);
  if (text == nullptr) {
    fprintf(stderr, "ozen: cannot read %s\n", path);
    return 2;
  }
  ozni_limits lim = limits;
  ozni_source src = {path, text, len};
  int has_errors = 0;
  char *out = ozni_debug_ast_text(a, &lim, &src, flags, &has_errors);
  free(text);
  return emit(a, out, has_errors);
}

static bool parse_number(const char *s, unsigned long long *out) {
  if (*s == '\0') {
    return false;
  }
  unsigned long long v = 0;
  for (; *s != '\0'; s++) {
    if (*s < '0' || *s > '9' || v > 100000000000000ull) {
      return false;
    }
    v = v * 10 + (unsigned long long)(*s - '0');
  }
  *out = v;
  return true;
}

static const char *level_names[64];
static uint8_t level_kinds[64];
static size_t nlevels;

static int take_options(int argc, char **argv, heap *h) {
  int out = 1;
  for (int i = 1; i < argc; i++) {
    static const char *const names[] = {"--max-input-bytes", "--max-depth", "--max-nodes", "--max-steps",
                                        "--fail-alloc-at"};
    int which = -1;
    for (int k = 0; k < 5; k++) {
      if (strcmp(argv[i], names[k]) == 0) {
        which = k;
      }
    }
    unsigned long long v = 0;
    if (strcmp(argv[i], "--strict") == 0) {
      settings.strict = true;
      continue;
    }
    if (strcmp(argv[i], "--fixes") == 0) {
      settings.fixes = true;
      continue;
    }
    int level = strcmp(argv[i], "--allow") == 0  ? OZNI_LEVEL_ALLOW
                : strcmp(argv[i], "--warn") == 0 ? OZNI_LEVEL_WARN
                : strcmp(argv[i], "--deny") == 0 ? OZNI_LEVEL_DENY
                                                 : -1;
    if (level >= 0) {
      if (i + 1 >= argc || nlevels >= sizeof level_names / sizeof level_names[0]) {
        return -1;
      }
      level_names[nlevels] = argv[++i];
      level_kinds[nlevels++] = (uint8_t)level;
      settings.level_names = level_names;
      settings.level_kinds = level_kinds;
      settings.nlevels = nlevels;
      continue;
    }
    const char **text = strcmp(argv[i], "--kitchen") == 0   ? &settings.kitchen
                        : strcmp(argv[i], "--options") == 0 ? &settings.options
                        : strcmp(argv[i], "--entry") == 0   ? &settings.entry
                                                            : nullptr;
    if (text != nullptr) {
      if (i + 1 >= argc) {
        return -1;
      }
      *text = argv[++i];
      continue;
    }
    if (which < 0) {
      argv[out++] = argv[i];
      continue;
    }
    if (i + 1 >= argc || !parse_number(argv[i + 1], &v)) {
      return -1;
    }
    i++;
    if (which == 0) {
      limits.input_bytes = (size_t)v;
    } else if (which == 1) {
      limits.depth = (uint32_t)(v > UINT32_MAX ? UINT32_MAX : v);
    } else if (which == 2) {
      limits.nodes = (uint32_t)(v > UINT32_MAX ? UINT32_MAX : v);
    } else if (which == 3) {
      limits.steps = v;
    } else {
      h->fail_at = (unsigned long)v;
    }
  }
  return out;
}

int main(int argc, char **argv) {
  heap h = {0, 0};
  ozni_allocator a = {heap_alloc, heap_release, &h};
  limits = ozni_default_limits();
  argc = take_options(argc, argv, &h);
  if (argc < 0) {
    return usage();
  }
  if (argc < 2) {
    return usage();
  }
  if (strcmp(argv[1], "--version") == 0 && argc == 2) {
    puts("ozen " OZEN_VERSION);
    return 0;
  }
  if (strcmp(argv[1], "check") == 0 && argc <= 3) {
    return run(&a, OZNI_MODE_CHECK, argc == 3 ? argv[2] : nullptr, nullptr);
  }
  if (strcmp(argv[1], "debug") != 0 || argc < 3) {
    return usage();
  }
  if (strcmp(argv[2], "ast") == 0) {
    return debug_ast(&a, argc, argv);
  }
  if (strcmp(argv[2], "definitions") == 0 && argc <= 4) {
    return run(&a, OZNI_MODE_DEFINITIONS, argc == 4 ? argv[3] : nullptr, nullptr);
  }
  if (strcmp(argv[2], "steps") == 0 && argc <= 4) {
    return run(&a, OZNI_MODE_STEPS, argc == 4 ? argv[3] : nullptr, nullptr);
  }
  if (strcmp(argv[2], "requires") == 0 && argc <= 4) {
    return run(&a, OZNI_MODE_REQUIRES, argc == 4 ? argv[3] : nullptr, nullptr);
  }
  if (strcmp(argv[2], "ir") == 0 && argc <= 4) {
    return run(&a, OZNI_MODE_IR, argc == 4 ? argv[3] : nullptr, nullptr);
  }
  if (strcmp(argv[2], "modules") == 0 && argc == 3) {
    return run(&a, OZNI_MODE_MODULES, nullptr, nullptr);
  }
  if (strcmp(argv[2], "embedded-std") == 0 && argc == 3) {
    ozni_limits lim = ozni_default_limits();
    int has_errors = 0;
    return emit(&a, ozni_run_text(&a, &lim, &settings, nullptr, 0, OZNI_MODE_EMBEDDED_STD, nullptr, &has_errors),
                has_errors);
  }
  if (strcmp(argv[2], "entry") == 0) {
    const char *entry = settings.entry, *file = nullptr;
    for (int i = 3; i < argc; i++) {
      if (strcmp(argv[i], "--entry") == 0 && i + 1 < argc) {
        entry = argv[++i];
      } else if (argv[i][0] == '-' || file != nullptr) {
        return usage();
      } else {
        file = argv[i];
      }
    }
    return run(&a, OZNI_MODE_ENTRY, file, entry);
  }
  return usage();
}
