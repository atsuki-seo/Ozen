/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#define _XOPEN_SOURCE 700
#include <dirent.h>
#include <errno.h>
#include <ftw.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include "../../core/include/ozen.h"
#include "ozni.h"
#include "sarif.h"

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
  fputs("usage: ozen check [OPTIONS] [ENTRY|FILE]\n"
        "       ozen build [OPTIONS] [ENTRY|FILE...]\n"
        "       ozen render [OPTIONS] [ENTRY|FILE]\n"
        "       ozen bundle [ENTRY] [-o FILE]\n"
        "       ozen unbundle [OPTIONS]\n"
        "       ozen debug ast [--no-spans] [--rules] FILE\n"
        "       ozen debug modules\n"
        "       ozen debug definitions [FILE]\n"
        "       ozen debug steps|requires|ir|schedule [FILE]\n"
        "       ozen debug entry [--entry NAME] [FILE]\n"
        "       ozen debug options [OPTIONS]\n"
        "       ozen debug embedded-std\n"
        "       ozen --version\n"
        "options: -k/--kitchen NAME|FILE --options NAME|FILE --entry NAME -s/--servings N -c/--cooks N\n"
        "         -t/--serve-at TIME --cleanup MODE --prefer A,B --strict --allow NAME --warn NAME --deny NAME\n"
        "         --format text|json|sarif --emit LIST --fixes --max-input-bytes N --max-depth N --max-nodes N\n"
        "         --max-steps N --fail-alloc-at N\n"
        "render:  --kind recipe|schedule|shopping|html|settings --lang ja|en --dialect NAME -o/--output FILE\n",
        stderr);
  return 2;
}

static char *read_file(const char *path, size_t *len) {
  FILE *f = fopen(path, "rb");
  if (f == nullptr) {
    return nullptr;
  }
  size_t cap = 4096, n = 0;
  char *buf = malloc(cap + 1);
  while (buf != nullptr) {
    size_t got = fread(buf + n, 1, cap - n, f);
    n += got;
    if (got == 0) {
      break;
    }
    if (n == cap) {
      char *bigger = realloc(buf, cap * 2 + 1);
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
  if (buf != nullptr) {
    buf[n] = '\0';
  }
  *len = n;
  return buf;
}

static char *dup_text(const char *s) {
  size_t n = strlen(s);
  char *out = malloc(n + 1);
  if (out != nullptr) {
    memcpy(out, s, n + 1);
  }
  return out;
}

typedef struct strings {
  char **items;
  size_t n;
  size_t cap;
} strings;

static bool push_string(strings *l, const char *s) {
  if (l->n == l->cap) {
    size_t cap = l->cap == 0 ? 16 : l->cap * 2;
    char **p = realloc(l->items, cap * sizeof *p);
    if (p == nullptr) {
      return false;
    }
    l->items = p;
    l->cap = cap;
  }
  l->items[l->n] = dup_text(s);
  return l->items[l->n++] != nullptr;
}

static void free_strings(strings *l) {
  for (size_t i = 0; i < l->n; i++) {
    free(l->items[i]);
  }
  free(l->items);
  *l = (strings){0};
}

static int walk(strings *l, const char *dir) {
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
      if (len > 5 && strcmp(path + len - 5, ".ozen") == 0 && !push_string(l, path)) {
        rc = -1;
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

/* Options */

typedef struct cli_options {
  const char *kitchen;
  const char *options;
  const char *entry;
  const char *servings;
  const char *cooks;
  const char *serve_at;
  const char *cleanup;
  const char *prefer;
  const char *format;
  const char *kind;
  const char *lang;
  const char *dialect;
  const char *output;
  bool strict;
  bool no_spans;
  bool rules;
  strings levels[3];
  unsigned emit;
} cli_options;

static ozni_limits limits;
static ozni_settings settings;
static cli_options opt;
static const char *level_names[64];
static uint8_t level_kinds[64];
static size_t nlevels;

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

static int take_options(int argc, char **argv, heap *h) {
  int out = 1;
  for (int i = 1; i < argc; i++) {
    const char *a = argv[i];
    if (a[0] != '-' || strcmp(a, "-") == 0 || strcmp(a, "--version") == 0) {
      argv[out++] = argv[i];
      continue;
    }
    static const struct {
      const char *name;
      const char *short_name;
      const char **slot;
    } with_value[] = {
        {"--kitchen", "-k", &opt.kitchen},   {"--options", nullptr, &opt.options}, {"--entry", nullptr, &opt.entry},
        {"--servings", "-s", &opt.servings}, {"--cooks", "-c", &opt.cooks},        {"--serve-at", "-t", &opt.serve_at},
        {"--cleanup", nullptr, &opt.cleanup}, {"--prefer", nullptr, &opt.prefer}, {"--format", nullptr, &opt.format},
        {"--kind", nullptr, &opt.kind},       {"--lang", nullptr, &opt.lang},     {"--dialect", nullptr, &opt.dialect},
        {"--output", "-o", &opt.output},
    };
    bool done = false;
    for (size_t k = 0; k < sizeof with_value / sizeof with_value[0] && !done; k++) {
      if (strcmp(a, with_value[k].name) == 0 ||
          (with_value[k].short_name != nullptr && strcmp(a, with_value[k].short_name) == 0)) {
        if (i + 1 >= argc) {
          return -1;
        }
        *with_value[k].slot = argv[++i];
        done = true;
      }
    }
    if (done) {
      continue;
    }
    if (strcmp(a, "--strict") == 0) {
      opt.strict = true;
      settings.strict = true;
      continue;
    }
    if (strcmp(a, "--fixes") == 0) {
      settings.fixes = true;
      continue;
    }
    if (strcmp(a, "--no-spans") == 0 || strcmp(a, "--rules") == 0) {
      opt.no_spans = opt.no_spans || a[2] == 'n';
      opt.rules = opt.rules || a[2] == 'r';
      continue;
    }
    int level = strcmp(a, "--allow") == 0  ? OZNI_LEVEL_ALLOW
                : strcmp(a, "--warn") == 0 ? OZNI_LEVEL_WARN
                : strcmp(a, "--deny") == 0 ? OZNI_LEVEL_DENY
                                           : -1;
    if (level >= 0) {
      if (i + 1 >= argc || nlevels >= sizeof level_names / sizeof level_names[0]) {
        return -1;
      }
      level_names[nlevels] = argv[++i];
      level_kinds[nlevels++] = (uint8_t)level;
      if (!push_string(&opt.levels[level], argv[i])) {
        return -1;
      }
      settings.level_names = level_names;
      settings.level_kinds = level_kinds;
      settings.nlevels = nlevels;
      continue;
    }
    if (strcmp(a, "--emit") == 0) {
      static const char *const kinds[] = {"ir", "schedule", "steps", "signatures", "definitions", "types"};
      if (i + 1 >= argc) {
        return -1;
      }
      char list[256];
      snprintf(list, sizeof list, "%s", argv[++i]);
      for (char *rest = nullptr, *tok = strtok_r(list, ",", &rest); tok != nullptr; tok = strtok_r(nullptr, ",", &rest)) {
        bool known = false;
        for (unsigned k = 0; k < 6; k++) {
          if (strcmp(tok, kinds[k]) == 0) {
            opt.emit |= 1u << k;
            known = true;
          }
        }
        if (!known) {
          return -1;
        }
      }
      continue;
    }
    static const char *const numbers[] = {"--max-input-bytes", "--max-depth", "--max-nodes", "--max-steps",
                                          "--fail-alloc-at"};
    int which = -1;
    for (int k = 0; k < 5; k++) {
      which = strcmp(a, numbers[k]) == 0 ? k : which;
    }
    unsigned long long v = 0;
    if (which < 0) {
      fprintf(stderr, "cli: unknown option: %s\n", a);
      return -2;
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
  if (opt.format != nullptr && strcmp(opt.format, "text") != 0 && strcmp(opt.format, "json") != 0 &&
      strcmp(opt.format, "sarif") != 0) {
    return -1;
  }
  settings.json = opt.format != nullptr && strcmp(opt.format, "json") == 0;
  settings.entry = opt.entry;
  return out;
}

static bool format_is(const char *f) { return opt.format != nullptr && strcmp(opt.format, f) == 0; }

static bool condition_flags(void) {
  return opt.servings != nullptr || opt.cooks != nullptr || opt.serve_at != nullptr || opt.cleanup != nullptr ||
         opt.prefer != nullptr;
}

/* Projects and declarations */

typedef struct project {
  strings paths;
  ozni_source *srcs;
  size_t n;
} project;

static void free_project(project *p) {
  for (size_t i = 0; p->srcs != nullptr && i < p->n; i++) {
    free((void *)p->srcs[i].text);
  }
  free(p->srcs);
  free_strings(&p->paths);
  *p = (project){0};
}

static int load_project(project *p, const char *file, bool allow_empty) {
  if (file == nullptr && allow_empty && !stat_file("package.ozen")) {
    p->srcs = calloc(3, sizeof *p->srcs);
    return p->srcs == nullptr ? 2 : 0;
  }
  if (file != nullptr && stat_file(file)) {
    if (!push_string(&p->paths, file)) {
      return 2;
    }
  } else if (stat_file("package.ozen")) {
    if (walk(&p->paths, "") != 0) {
      fputs("ozen: cannot read the project folder\n", stderr);
      return 2;
    }
  } else {
    fputs("ozen: no FILE given and no package.ozen in this folder\n", stderr);
    return 2;
  }
  qsort(p->paths.items, p->paths.n, sizeof *p->paths.items, cmp_path);
  p->srcs = calloc(p->paths.n + 3, sizeof *p->srcs);
  if (p->srcs == nullptr) {
    return 2;
  }
  for (size_t i = 0; i < p->paths.n; i++) {
    size_t len = 0;
    char *text = read_file(p->paths.items[i], &len);
    if (text == nullptr) {
      fprintf(stderr, "ozen: cannot read %s\n", p->paths.items[i]);
      return 2;
    }
    p->srcs[p->n++] = (ozni_source){p->paths.items[i], text, len};
  }
  return 0;
}

static size_t skip_space(const char *t, size_t n, size_t i) {
  while (i < n) {
    if (t[i] == ' ' || t[i] == '\t' || t[i] == '\r' || t[i] == '\n') {
      i++;
    } else if (i + 1 < n && t[i] == '/' && t[i + 1] == '/') {
      while (i < n && t[i] != '\n') {
        i++;
      }
    } else {
      break;
    }
  }
  return i;
}

static bool ident_char(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
}

static bool word_at(const char *t, size_t n, size_t i, const char *w) {
  size_t k = strlen(w);
  return i + k <= n && memcmp(t + i, w, k) == 0 && (i + k == n || !ident_char(t[i + k]));
}

typedef struct decl_slice {
  size_t file;
  size_t start;
  size_t end;
  int found;
  size_t other;
} decl_slice;

static void find_kitchen(const ozni_source *srcs, size_t n, const char *name, bool options, decl_slice *out) {
  for (size_t f = 0; f < n; f++) {
    const char *t = srcs[f].text;
    size_t len = srcs[f].len, i = 0, depth = 0;
    while (i < len) {
      if (t[i] == '"') {
        for (i++; i < len && t[i] != '"'; i++) {
          i += t[i] == '\\' ? 1 : 0;
        }
        i++;
        continue;
      }
      if (i + 1 < len && t[i] == '/' && t[i + 1] == '/') {
        while (i < len && t[i] != '\n') {
          i++;
        }
        continue;
      }
      if (t[i] == '{') {
        depth++;
      } else if (t[i] == '}' && depth > 0) {
        depth--;
      }
      bool line_start = i == 0 || t[i - 1] == '\n';
      if (depth != 0 || !line_start || !word_at(t, len, i, "kitchen")) {
        i++;
        continue;
      }
      size_t start = i, j = skip_space(t, len, i + 7);
      bool named = name == nullptr || word_at(t, len, j, name);
      while (j < len && ident_char(t[j])) {
        j++;
      }
      j = skip_space(t, len, j);
      bool typed = j < len && t[j] == ':';
      while (j < len && t[j] != '{') {
        j++;
      }
      size_t d = 0, k = j;
      for (; k < len; k++) {
        d += t[k] == '{' ? 1 : 0;
        if (t[k] == '}' && --d == 0) {
          break;
        }
      }
      if (named && typed == options && k < len) {
        out->found++;
        if (out->found == 1) {
          out->file = f;
          out->start = start;
          out->end = k + 1;
        } else if (out->found == 2) {
          out->other = f;
        }
      }
      i = k < len ? k + 1 : len;
    }
  }
}

static char *padded_slice(const ozni_source *src, const decl_slice *s, size_t *len) {
  size_t pad = 0;
  for (size_t i = 0; i < s->start; i++) {
    pad += src->text[i] == '\n' ? 1 : 0;
  }
  size_t col = 0;
  for (size_t i = s->start; i > 0 && src->text[i - 1] != '\n'; i--) {
    col++;
  }
  *len = pad + col + (s->end - s->start);
  char *out = malloc(*len + 1);
  if (out == nullptr) {
    return nullptr;
  }
  memset(out, '\n', pad);
  memset(out + pad, ' ', col);
  memcpy(out + pad + col, src->text + s->start, s->end - s->start);
  out[*len] = '\0';
  return out;
}

typedef struct config_text {
  char *text;
  size_t len;
  bool by_name;
} config_text;

static int resolve_config(const project *p, const char *value, bool options, config_text *out) {
  *out = (config_text){0};
  if (value == nullptr) {
    return 0;
  }
  if (stat_file(value)) {
    size_t len = 0;
    char *text = read_file(value, &len);
    if (text == nullptr) {
      fprintf(stderr, "ozen: cannot read %s\n", value);
      return 2;
    }
    if (options) {
      out->text = text;
      out->len = len;
      return 0;
    }
    ozni_source src = {value, text, len};
    decl_slice s = {0};
    find_kitchen(&src, 1, nullptr, false, &s);
    if (s.found != 1) {
      fprintf(stderr, "cli: kitchen not found in %s\n", value);
      free(text);
      return 2;
    }
    out->text = padded_slice(&src, &s, &out->len);
    free(text);
    return out->text == nullptr ? 2 : 0;
  }
  decl_slice s = {0};
  find_kitchen(p->srcs, p->n, value, options, &s);
  if (s.found == 0) {
    fprintf(stderr, options ? "cli: options not found: %s\n" : "cli: kitchen not found: %s\n", value);
    return 2;
  }
  if (s.found > 1) {
    fprintf(stderr, options ? "cli: options are ambiguous: %s (%s, %s)\n" : "cli: kitchen is ambiguous: %s (%s, %s)\n",
            value, p->srcs[s.file].path, p->srcs[s.other].path);
    return 2;
  }
  out->text = padded_slice(&p->srcs[s.file], &s, &out->len);
  out->by_name = true;
  return out->text == nullptr ? 2 : 0;
}

static void append(char **buf, size_t *len, size_t *cap, const char *s, size_t n) {
  if (*buf == nullptr || *len + n + 1 > *cap) {
    size_t c = (*cap + n + 1) * 2;
    char *b = realloc(*buf, c);
    if (b == nullptr) {
      return;
    }
    *buf = b;
    *cap = c;
  }
  memcpy(*buf + *len, s, n);
  *len += n;
  (*buf)[*len] = '\0';
}

static void appends(char **buf, size_t *len, size_t *cap, const char *s) { append(buf, len, cap, s, strlen(s)); }

static char *cli_options_text(const char *base, size_t base_len, bool levels, size_t *out_len) {
  char *buf = nullptr;
  size_t len = 0, cap = 0;
  appends(&buf, &len, &cap, "");
  if (base != nullptr && base_len > 0) {
    append(&buf, &len, &cap, base, base_len);
    if (buf != nullptr && len > 0 && buf[len - 1] != '\n') {
      appends(&buf, &len, &cap, "\n");
    }
  }
  bool any_level = levels && (opt.strict || opt.levels[0].n + opt.levels[1].n + opt.levels[2].n > 0);
  if (condition_flags() || any_level) {
    char line[512];
    appends(&buf, &len, &cap, "kitchen cli : Options {\n");
    const char *fields[4] = {opt.servings, opt.cooks, opt.serve_at, opt.cleanup};
    static const char *const names[4] = {"servings", "cooks", "serveAt", "cleanup"};
    for (int k = 0; k < 4; k++) {
      if (fields[k] != nullptr) {
        snprintf(line, sizeof line, "  %s: %s\n", names[k], fields[k]);
        appends(&buf, &len, &cap, line);
      }
    }
    if (opt.prefer != nullptr) {
      appends(&buf, &len, &cap, "  prefer: [");
      char list[256];
      snprintf(list, sizeof list, "%s", opt.prefer);
      bool first = true;
      for (char *rest = nullptr, *tok = strtok_r(list, ",", &rest); tok != nullptr; tok = strtok_r(nullptr, ",", &rest)) {
        appends(&buf, &len, &cap, first ? "" : ", ");
        appends(&buf, &len, &cap, tok);
        first = false;
      }
      appends(&buf, &len, &cap, "]\n");
    }
    if (levels && opt.strict) {
      appends(&buf, &len, &cap, "  strict: on\n");
    }
    static const char *const lnames[3] = {"allow", "warn", "deny"};
    for (int k = 0; levels && k < 3; k++) {
      if (opt.levels[k].n == 0) {
        continue;
      }
      snprintf(line, sizeof line, "  %s: [", lnames[k]);
      appends(&buf, &len, &cap, line);
      for (size_t i = 0; i < opt.levels[k].n; i++) {
        snprintf(line, sizeof line, "%s\"%s\"", i == 0 ? "" : ", ", opt.levels[k].items[i]);
        appends(&buf, &len, &cap, line);
      }
      appends(&buf, &len, &cap, "]\n");
    }
    appends(&buf, &len, &cap, "}\n");
  }
  *out_len = len;
  return buf;
}

/* Building through the public API */

static void *api_alloc(void *ctx, size_t size) {
  (void)ctx;
  return malloc(size);
}

static void api_release(void *ctx, void *ptr, size_t size) {
  (void)ctx;
  (void)size;
  free(ptr);
}

typedef struct api_inputs {
  config_text kitchen;
  char *options;
  size_t options_len;
} api_inputs;

static int api_prepare(const project *p, api_inputs *in) {
  *in = (api_inputs){0};
  config_text base = {0};
  int rc = resolve_config(p, opt.kitchen, false, &in->kitchen);
  if (rc == 0) {
    rc = resolve_config(p, opt.options, true, &base);
  }
  if (rc != 0) {
    free(in->kitchen.text);
    in->kitchen.text = nullptr;
    return rc;
  }
  in->options = cli_options_text(base.text, base.len, true, &in->options_len);
  if (in->options != nullptr && in->options_len == 0) {
    free(in->options);
    in->options = nullptr;
  }
  free(base.text);
  return 0;
}

static void api_free(api_inputs *in) {
  free(in->kitchen.text);
  free(in->options);
}

static char *api_build(const project *p, const api_inputs *in, const char *entry, unsigned emit, recipe_config *cfg) {
  recipe_source *rs = calloc(p->n + 1, sizeof *rs);
  *cfg = recipe_default_config();
  cfg->alloc = api_alloc;
  cfg->release = api_release;
  if (rs == nullptr) {
    return nullptr;
  }
  for (size_t i = 0; i < p->n; i++) {
    rs[i] = (recipe_source){p->srcs[i].path, p->srcs[i].text, p->srcs[i].len};
  }
  cfg->entry = entry;
  cfg->emit = emit;
  cfg->max_steps = (unsigned long)limits.steps;
  cfg->max_depth = limits.depth;
  cfg->max_nodes = limits.nodes;
  cfg->max_input_bytes = (unsigned long)limits.input_bytes;
  char *out = recipe_build(cfg, rs, p->n, in->kitchen.text, in->kitchen.len, in->options, in->options_len);
  free(rs);
  return out;
}

static bool result_failed(const char *json) { return strstr(json, "\"status\":\"error\"") != nullptr; }

/* Text output through the core */

static int emit_text(const ozni_allocator *a, char *text, int has_errors) {
  if (text == nullptr) {
    fputs("ozen: out of memory\n", stderr);
    return 2;
  }
  fputs(text, stdout);
  ozni_free_text(a, text);
  return has_errors ? 1 : 0;
}

static int text_run(const ozni_allocator *a, project *p, ozni_mode mode, const char *entry, char **text_out,
                    int *errors_out) {
  ozni_settings set = settings;
  set.entry = entry;
  config_text kitchen = {0}, base = {0};
  int rc = 0;
  size_t extra = 0;
  if (opt.kitchen != nullptr) {
    rc = resolve_config(p, opt.kitchen, false, &kitchen);
    if (rc == 0 && kitchen.by_name) {
      set.kitchen = opt.kitchen;
    } else if (rc == 0) {
      set.kitchen = "<kitchen>";
      p->srcs[p->n + extra++] = (ozni_source){"<kitchen>", kitchen.text, kitchen.len};
    }
  }
  char *options = nullptr;
  if (rc == 0 && opt.options != nullptr && !condition_flags() && !stat_file(opt.options)) {
    rc = resolve_config(p, opt.options, true, &base);
    set.options = opt.options;
  } else if (rc == 0 && (opt.options != nullptr || condition_flags())) {
    rc = resolve_config(p, opt.options, true, &base);
    size_t len = 0;
    options = rc == 0 ? cli_options_text(base.text, base.len, false, &len) : nullptr;
    if (options != nullptr) {
      set.options = "<options>";
      p->srcs[p->n + extra++] = (ozni_source){"<options>", options, len};
    }
  }
  int has_errors = 0;
  char *out = rc == 0 ? ozni_run_text(a, &limits, &set, p->srcs, p->n + extra, mode, entry, &has_errors) : nullptr;
  free(kitchen.text);
  free(base.text);
  free(options);
  if (rc != 0) {
    return rc;
  }
  *text_out = out;
  *errors_out = has_errors;
  return -1;
}

/* Entries */

typedef struct entry_ref {
  char *module;
  char *name;
  size_t count;
} entry_ref;

typedef struct entry_list {
  entry_ref *items;
  size_t n;
} entry_list;

static char *module_of_path(const char *path) {
  size_t n = strlen(path);
  if (n > 5 && strcmp(path + n - 5, ".ozen") == 0) {
    n -= 5;
  }
  char *m = malloc(n + 1);
  if (m == nullptr) {
    return nullptr;
  }
  for (size_t i = 0; i < n; i++) {
    m[i] = path[i] == '/' ? '.' : path[i];
  }
  m[n] = '\0';
  return m;
}

static bool is_manifest(const char *path) {
  size_t n = strlen(path);
  return n >= 12 && strcmp(path + n - 12, "package.ozen") == 0 && (n == 12 || path[n - 13] == '/');
}

static void collect_entries(const project *p, entry_list *out) {
  for (size_t f = 0; f < p->n; f++) {
    const char *t = p->srcs[f].text, *path = p->srcs[f].path;
    size_t len = p->srcs[f].len;
    if (is_manifest(path)) {
      continue;
    }
    for (size_t i = 0; i < len; i++) {
      if (i > 0 && t[i - 1] != '\n') {
        continue;
      }
      size_t j = word_at(t, len, i, "recipe") ? i + 6 : word_at(t, len, i, "menu") ? i + 4 : len;
      if (j >= len) {
        continue;
      }
      j = skip_space(t, len, j);
      size_t k = j;
      while (k < len && ident_char(t[k])) {
        k++;
      }
      if (k == j) {
        continue;
      }
      entry_ref *items = realloc(out->items, (out->n + 1) * sizeof *items);
      char *name = malloc(k - j + 1);
      if (items == nullptr || name == nullptr) {
        free(name);
        if (items != nullptr) {
          out->items = items;
        }
        return;
      }
      out->items = items;
      memcpy(name, t + j, k - j);
      name[k - j] = '\0';
      out->items[out->n++] = (entry_ref){module_of_path(path), name, 0};
    }
  }
  for (size_t i = 0; i < out->n; i++) {
    for (size_t k = 0; k < out->n; k++) {
      out->items[i].count += out->items[i].module != nullptr && out->items[k].module != nullptr &&
                                     strcmp(out->items[i].module, out->items[k].module) == 0
                                 ? 1
                                 : 0;
    }
  }
}

static void free_entries(entry_list *l) {
  for (size_t i = 0; i < l->n; i++) {
    free(l->items[i].module);
    free(l->items[i].name);
  }
  free(l->items);
  *l = (entry_list){0};
}

static bool module_exists(const project *p, const char *m) {
  for (size_t f = 0; f < p->n; f++) {
    char *name = module_of_path(p->srcs[f].path);
    bool same = name != nullptr && strcmp(name, m) == 0 && !is_manifest(p->srcs[f].path);
    free(name);
    if (same) {
      return true;
    }
  }
  return false;
}

static int cmp_entry(const void *a, const void *b) {
  const entry_ref *x = a, *y = b;
  int r = strcmp(x->module, y->module);
  return r != 0 ? r : strcmp(x->name, y->name);
}

static bool split_target(const project *p, const char *arg, char *module, size_t msize, char *name, size_t nsize) {
  size_t len = strlen(arg);
  for (size_t k = 1; k <= len; k++) {
    if (k < len && arg[k] != '.') {
      continue;
    }
    char prefix[1024];
    snprintf(prefix, sizeof prefix, "%.*s", (int)k, arg);
    if (module_exists(p, prefix)) {
      snprintf(module, msize, "%s", prefix);
      snprintf(name, nsize, "%s", k < len ? arg + k + 1 : "");
      return true;
    }
  }
  return false;
}

static bool in_target(const entry_ref *e, const char *module, bool folder) {
  size_t mlen = strlen(module);
  return folder ? strncmp(e->module, module, mlen) == 0 && e->module[mlen] == '.' : strcmp(e->module, module) == 0;
}

static int select_entries(const project *p, const entry_list *all, int nargs, char **args, bool *chosen) {
  for (size_t i = 0; i < all->n; i++) {
    chosen[i] = nargs == 0;
  }
  for (int a = 0; a < nargs; a++) {
    const char *arg = args[a];
    if (strchr(arg, ':') != nullptr) {
      fputs("cli: use a module path, not file:name\n", stderr);
      return 2;
    }
    char module[1024] = "", name[256] = "";
    bool folder = false;
    if (stat_file(arg)) {
      char *m = module_of_path(arg);
      snprintf(module, sizeof module, "%s", m != nullptr ? m : "");
      free(m);
    } else if (!split_target(p, arg, module, sizeof module, name, sizeof name)) {
      snprintf(module, sizeof module, "%s", arg);
      folder = true;
    }
    if (name[0] == '\0' && opt.entry != nullptr) {
      snprintf(name, sizeof name, "%s", opt.entry);
    }
    size_t matched = 0;
    for (size_t i = 0; i < all->n; i++) {
      matched += in_target(&all->items[i], module, folder) && (name[0] == '\0' || strcmp(all->items[i].name, name) == 0);
    }
    if (!folder && name[0] == '\0' && matched > 1) {
      fputs("cli: several entries; choose one of: ", stderr);
      bool first = true;
      for (size_t i = 0; i < all->n; i++) {
        if (strcmp(all->items[i].module, module) == 0) {
          fprintf(stderr, "%s%s", first ? "" : ", ", all->items[i].name);
          first = false;
        }
      }
      fputs("\n", stderr);
      return 2;
    }
    if (matched == 0) {
      fprintf(stderr, "cli: no entry matches %s\n", arg);
      return 2;
    }
    for (size_t i = 0; i < all->n; i++) {
      chosen[i] = chosen[i] ||
                  (in_target(&all->items[i], module, folder) && (name[0] == '\0' || strcmp(all->items[i].name, name) == 0));
    }
  }
  return 0;
}

static int remove_entry(const char *path, const struct stat *st, int flag, struct FTW *ftw) {
  (void)st;
  (void)flag;
  (void)ftw;
  return remove(path);
}

static bool make_dirs(const char *path) {
  char buf[4096];
  snprintf(buf, sizeof buf, "%s", path);
  for (char *s = buf + 1; *s != '\0'; s++) {
    if (*s == '/') {
      *s = '\0';
      if (mkdir(buf, 0777) != 0 && errno != EEXIST) {
        return false;
      }
      *s = '/';
    }
  }
  return mkdir(buf, 0777) == 0 || errno == EEXIST;
}

static bool write_text(const char *dir, const char *name, const char *text) {
  char path[8192];
  snprintf(path, sizeof path, "%s/%s", dir, name);
  FILE *f = fopen(path, "wb");
  if (f == nullptr) {
    return false;
  }
  fputs(text, f);
  return fclose(f) == 0;
}

static long long count_of(const char *json, const char *key) {
  char pat[64];
  snprintf(pat, sizeof pat, "\"%s\":", key);
  const char *at = strstr(json, pat);
  return at != nullptr ? strtoll(at + strlen(pat), nullptr, 10) : 0;
}

static void build_one(const ozni_allocator *a, project *p, const api_inputs *in, const entry_ref *e, FILE *index,
                      size_t *total, size_t *passed) {
  char dir[4096], rel[2048];
  snprintf(rel, sizeof rel, "%s", e->module);
  for (char *s = rel; *s != '\0'; s++) {
    *s = *s == '.' ? '/' : *s;
  }
  if (e->count > 1) {
    size_t n = strlen(rel);
    snprintf(rel + n, sizeof rel - n, "/%s", e->name);
  }
  snprintf(dir, sizeof dir, "out/%s", rel);
  make_dirs(dir);
  recipe_config cfg;
  char *diags = api_build(p, in, e->name, 0, &cfg);
  char *sched = api_build(p, in, e->name, RECIPE_EMIT_SCHEDULE, &cfg);
  char *ir = nullptr;
  int errors = 0;
  int trc = text_run(a, p, OZNI_MODE_IR, e->name, &ir, &errors);
  bool failed = diags == nullptr || result_failed(diags);
  if (diags != nullptr) {
    write_text(dir, "diagnostics.json", diags);
  }
  if (diags != nullptr && format_is("sarif")) {
    char path[8192];
    snprintf(path, sizeof path, "%s/diagnostics.sarif", dir);
    FILE *f = fopen(path, "wb");
    if (f != nullptr) {
      ozen_write_sarif(f, diags, strlen(diags));
      fclose(f);
    }
  }
  if (sched != nullptr) {
    write_text(dir, "schedule.json", sched);
  }
  if (trc == -1 && ir != nullptr) {
    write_text(dir, "ir.txt", ir);
    ozni_free_text(a, ir);
  }
  if (index != nullptr) {
    fprintf(index,
            "%s{\"module\":\"%s\",\"entry\":\"%s\",\"status\":\"%s\",\"errors\":%lld,\"warnings\":%lld,\"infos\":%lld,"
            "\"dir\":\"%s\"}",
            *total == 0 ? "" : ",", e->module, e->name, failed ? "error" : "ok",
            diags != nullptr ? count_of(diags, "errors") : 0, diags != nullptr ? count_of(diags, "warnings") : 0,
            diags != nullptr ? count_of(diags, "infos") : 0, rel);
  }
  (*total)++;
  *passed += failed ? 0 : 1;
  recipe_free_string(&cfg, diags);
  recipe_free_string(&cfg, sched);
}

static int build(const ozni_allocator *a, int nargs, char **args) {
  project p = {0};
  int rc = load_project(&p, nargs == 1 && stat_file(args[0]) && !stat_file("package.ozen") ? args[0] : nullptr, false);
  entry_list all = {0};
  bool *chosen = nullptr;
  api_inputs in = {0};
  if (rc == 0) {
    collect_entries(&p, &all);
    qsort(all.items, all.n, sizeof *all.items, cmp_entry);
    chosen = calloc(all.n + 1, sizeof *chosen);
    rc = chosen == nullptr ? 2 : select_entries(&p, &all, nargs, args, chosen);
  }
  if (rc == 0) {
    rc = api_prepare(&p, &in);
  }
  if (rc != 0) {
    free(chosen);
    free_entries(&all);
    free_project(&p);
    return rc;
  }
  nftw("out", remove_entry, 16, FTW_DEPTH | FTW_PHYS);
  make_dirs("out");
  FILE *index = fopen("out/index.json", "wb");
  size_t total = 0, passed = 0;
  if (index != nullptr) {
    fputs("{\"schemaVersion\":1,\"entries\":[", index);
  }
  for (size_t i = 0; i < all.n; i++) {
    if (chosen[i]) {
      build_one(a, &p, &in, &all.items[i], index, &total, &passed);
    }
  }
  if (index != nullptr) {
    fputs("]}", index);
    fclose(index);
  }
  printf("build: %zu entries, %zu passed\n", total, passed);
  api_free(&in);
  free(chosen);
  free_entries(&all);
  free_project(&p);
  return passed == total ? 0 : 1;
}

static void append_quoted(char **buf, size_t *len, size_t *cap, const char *s) {
  appends(buf, len, cap, " '");
  for (; *s != '\0'; s++) {
    if (*s == '\'') {
      appends(buf, len, cap, "'\\''");
    } else {
      append(buf, len, cap, s, 1);
    }
  }
  appends(buf, len, cap, "'");
}

static char *sibling(const char *path, const char *rel) {
  const char *slash = strrchr(path, '/');
  size_t dir = slash == nullptr ? 0 : (size_t)(slash - path) + 1;
  char *out = malloc(dir + strlen(rel) + 1);
  if (out != nullptr) {
    memcpy(out, path, dir);
    strcpy(out + dir, rel);
  }
  return out;
}

/* The renderer is JavaScript next to the source tree, so it is found from the executable's real path
   (build/<dir>/ozen); OZEN_RENDERER overrides that for installs elsewhere. */
static char *find_renderer(const char *argv0) {
  const char *env = getenv("OZEN_RENDERER");
  if (env != nullptr) {
    if (stat_file(env)) {
      return realpath(env, nullptr);
    }
    fprintf(stderr, "cli: renderer not found: %s\n", env);
    return nullptr;
  }
  static const char *const rels[] = {"../../renderers/cli.mjs", "../renderers/cli.mjs"};
  const char *exes[] = {"/proc/self/exe", argv0};
  for (size_t e = 0; e < 2; e++) {
    char *exe = realpath(exes[e], nullptr);
    for (size_t r = 0; exe != nullptr && r < 2; r++) {
      char *cand = sibling(exe, rels[r]);
      char *real = cand != nullptr ? realpath(cand, nullptr) : nullptr;
      free(cand);
      if (real != nullptr && stat_file(real)) {
        free(exe);
        return real;
      }
      free(real);
    }
    free(exe);
  }
  fputs("cli: renderer not found; set OZEN_RENDERER to renderers/cli.mjs\n", stderr);
  return nullptr;
}

static int render(int nargs, char **args, const char *argv0) {
  if (nargs > 1) {
    return usage();
  }
  char *renderer = find_renderer(argv0);
  if (renderer == nullptr) {
    return 2;
  }
  const char *target = nargs == 1 ? args[0] : nullptr;
  project p = {0};
  const char *entry = opt.entry;
  char module[1024] = "", name[256] = "";
  int rc = load_project(&p, target, false);
  if (rc == 0 && target != nullptr && !stat_file(target) &&
      split_target(&p, target, module, sizeof module, name, sizeof name) && name[0] != '\0') {
    entry = name;
  }
  api_inputs in = {0};
  if (rc == 0) {
    rc = api_prepare(&p, &in);
  }
  char *json = nullptr;
  recipe_config cfg;
  if (rc == 0) {
    static const unsigned emit = 1u << 0 | 1u << 1 | 1u << 2 | 1u << 4 | 1u << 5;
    json = api_build(&p, &in, entry, emit, &cfg);
    rc = json == nullptr ? 2 : 0;
  }
  char *cmd = nullptr;
  size_t len = 0, cap = 0;
  if (rc == 0) {
    const char *node = getenv("OZEN_NODE");
    char *std = sibling(renderer, "../phrases");
    struct stat st;
    appends(&cmd, &len, &cap, "exec");
    append_quoted(&cmd, &len, &cap, node != nullptr ? node : "node");
    append_quoted(&cmd, &len, &cap, renderer);
    const char *pairs[][2] = {{"--kind", opt.kind},
                              {"--lang", opt.lang},
                              {"--dialect", opt.dialect},
                              {"--output", opt.output},
                              {"--std", std},
                              {"--project", stat("phrases", &st) == 0 && S_ISDIR(st.st_mode) ? "phrases" : nullptr}};
    for (size_t k = 0; k < sizeof pairs / sizeof pairs[0]; k++) {
      if (pairs[k][1] != nullptr) {
        append_quoted(&cmd, &len, &cap, pairs[k][0]);
        append_quoted(&cmd, &len, &cap, pairs[k][1]);
      }
    }
    free(std);
  }
  if (rc == 0 && cmd != nullptr) {
    fflush(stdout);
    FILE *pipe = popen(cmd, "w");
    if (pipe == nullptr) {
      fputs("cli: cannot start the renderer\n", stderr);
      rc = 2;
    } else {
      fputs(json, pipe);
      int status = pclose(pipe);
      int code = status == -1 || !WIFEXITED(status) ? 2 : WEXITSTATUS(status);
      rc = code != 0 ? code : result_failed(json) ? 1 : 0;
    }
  } else if (rc == 0) {
    rc = 2;
  }
  free(cmd);
  if (json != nullptr) {
    recipe_free_string(&cfg, json);
  }
  api_free(&in);
  free_project(&p);
  free(renderer);
  return rc;
}

static int list_ftl(strings *l, const char *dir) {
  DIR *d = opendir(dir);
  if (d == nullptr) {
    return 0;
  }
  struct dirent *e;
  int rc = 0;
  while (rc == 0 && (e = readdir(d)) != nullptr) {
    size_t len = strlen(e->d_name);
    char path[4096];
    if (e->d_name[0] == '.' || len <= 4 || strcmp(e->d_name + len - 4, ".ftl") != 0) {
      continue;
    }
    int n = snprintf(path, sizeof path, "%s/%s", dir, e->d_name);
    rc = n < 0 || (size_t)n >= sizeof path || !push_string(l, path) ? -1 : 0;
  }
  closedir(d);
  return rc;
}

static int collect_phrases(strings *l) {
  int rc = list_ftl(l, "phrases");
  DIR *d = opendir("deps");
  if (d != nullptr) {
    struct dirent *e;
    while (rc == 0 && (e = readdir(d)) != nullptr) {
      char dir[4096];
      int n = snprintf(dir, sizeof dir, "deps/%s/phrases", e->d_name);
      if (e->d_name[0] != '.' && n > 0 && (size_t)n < sizeof dir) {
        rc = list_ftl(l, dir);
      }
    }
    closedir(d);
  }
  qsort(l->items, l->n, sizeof *l->items, cmp_path);
  return rc;
}

static int bundle(const ozni_allocator *a, int nargs, char **args) {
  if (nargs > 1) {
    return usage();
  }
  if (!stat_file("package.ozen")) {
    fputs("cli: bundle needs a package.ozen in this folder\n", stderr);
    return 2;
  }
  project p = {0};
  const char *entry = opt.entry;
  char module[1024] = "", name[256] = "";
  int rc = load_project(&p, nullptr, false);
  if (rc == 0 && nargs == 1) {
    const char *dot = strrchr(args[0], '.');
    bool split = split_target(&p, args[0], module, sizeof module, name, sizeof name);
    entry = split && name[0] != '\0' ? name : dot != nullptr ? dot + 1 : args[0];
  }
  strings files = {0};
  ozni_source *phrases = nullptr;
  if (rc == 0 && collect_phrases(&files) != 0) {
    fputs("ozen: cannot read the phrases folders\n", stderr);
    rc = 2;
  }
  if (rc == 0) {
    phrases = calloc(files.n + 1, sizeof *phrases);
    rc = phrases == nullptr ? 2 : 0;
  }
  for (size_t i = 0; rc == 0 && i < files.n; i++) {
    size_t len = 0;
    char *text = read_file(files.items[i], &len);
    if (text == nullptr) {
      fprintf(stderr, "ozen: cannot read %s\n", files.items[i]);
      rc = 2;
      break;
    }
    phrases[i] = (ozni_source){files.items[i], text, len};
    if (strstr(text, "\"\"\"") != nullptr) {
      fprintf(stderr, "cli: %s holds \"\"\", which a bundle cannot quote\n", files.items[i]);
      rc = 2;
    }
  }
  if (rc == 0) {
    ozni_settings set = settings;
    set.entry = entry;
    int has_errors = 0;
    char *text = ozni_bundle_text(a, &limits, &set, p.srcs, p.n, phrases, files.n, &has_errors);
    if (text != nullptr && !has_errors && opt.output != nullptr) {
      FILE *f = fopen(opt.output, "wb");
      rc = f == nullptr || fputs(text, f) < 0 ? 2 : 0;
      if (f != nullptr && fclose(f) != 0) {
        rc = 2;
      }
      if (rc != 0) {
        fprintf(stderr, "ozen: cannot write %s\n", opt.output);
      }
      ozni_free_text(a, text);
    } else {
      rc = emit_text(a, text, has_errors);
    }
  }
  for (size_t i = 0; phrases != nullptr && i < files.n; i++) {
    free((void *)phrases[i].text);
  }
  free(phrases);
  free_strings(&files);
  free_project(&p);
  return rc;
}

static int check(const ozni_allocator *a, int nargs, char **args, ozni_mode mode) {
  if (nargs > 1) {
    return usage();
  }
  const char *target = nargs == 1 ? args[0] : nullptr;
  if (target != nullptr && strchr(target, ':') != nullptr) {
    fputs("cli: use a module path, not file:name\n", stderr);
    return 2;
  }
  project p = {0};
  const char *entry = opt.entry;
  char module[1024] = "", name[256] = "";
  int rc = load_project(&p, target, mode == OZNI_MODE_DEFINITIONS || mode == OZNI_MODE_ENTRY);
  if (rc == 0 && target != nullptr && !stat_file(target) &&
      split_target(&p, target, module, sizeof module, name, sizeof name) && name[0] != '\0') {
    entry = name;
  }
  if (rc != 0) {
    free_project(&p);
    return rc;
  }
  if (mode == OZNI_MODE_CHECK && (format_is("json") || format_is("sarif"))) {
    api_inputs in;
    rc = api_prepare(&p, &in);
    if (rc == 0) {
      recipe_config cfg;
      char *out = api_build(&p, &in, entry, opt.emit, &cfg);
      if (out == nullptr) {
        fputs("ozen: out of memory\n", stderr);
        rc = 2;
      } else {
        if (format_is("sarif")) {
          ozen_write_sarif(stdout, out, strlen(out));
        } else {
          fputs(out, stdout);
        }
        rc = result_failed(out) ? 1 : 0;
        recipe_free_string(&cfg, out);
      }
      api_free(&in);
    }
    free_project(&p);
    return rc;
  }
  char *text = nullptr;
  int errors = 0;
  rc = text_run(a, &p, mode, entry, &text, &errors);
  if (rc == -1) {
    rc = emit_text(a, text, errors);
  }
  free_project(&p);
  return rc;
}

static int debug_ast(const ozni_allocator *a, int nargs, char **args) {
  if (nargs != 1) {
    return usage();
  }
  size_t len = 0;
  char *text = read_file(args[0], &len);
  if (text == nullptr) {
    fprintf(stderr, "ozen: cannot read %s\n", args[0]);
    return 2;
  }
  unsigned flags = (opt.no_spans ? 0u : (unsigned)OZNI_AST_SPANS) | (opt.rules ? (unsigned)OZNI_AST_RULES : 0u);
  ozni_source src = {args[0], text, len};
  int has_errors = 0;
  char *out = ozni_debug_ast_text(a, &limits, &src, flags, &has_errors);
  free(text);
  return emit_text(a, out, has_errors);
}

static int debug_options(void) {
  config_text base = {0};
  project p = {0};
  if (opt.options != nullptr) {
    int rc = resolve_config(&p, opt.options, true, &base);
    if (rc != 0) {
      return rc;
    }
  }
  size_t len = 0;
  char *text = cli_options_text(base.text, base.len, true, &len);
  if (text != nullptr) {
    fputs(text, stdout);
  }
  free(text);
  free(base.text);
  return 0;
}

int main(int argc, char **argv) {
  heap h = {0, 0};
  ozni_allocator a = {heap_alloc, heap_release, &h};
  limits = ozni_default_limits();
  int n = take_options(argc, argv, &h);
  if (n == -2) {
    return 2;
  }
  if (n < 2) {
    return usage();
  }
  const char *cmd = argv[1];
  if (strcmp(cmd, "--version") == 0 && n == 2) {
    puts("ozen " OZEN_VERSION);
    return 0;
  }
  if (strcmp(cmd, "check") == 0) {
    return check(&a, n - 2, argv + 2, OZNI_MODE_CHECK);
  }
  if (strcmp(cmd, "build") == 0) {
    return build(&a, n - 2, argv + 2);
  }
  if (strcmp(cmd, "render") == 0) {
    return render(n - 2, argv + 2, argv[0]);
  }
  if (strcmp(cmd, "bundle") == 0) {
    return bundle(&a, n - 2, argv + 2);
  }
  if (strcmp(cmd, "unbundle") == 0) {
    fprintf(stderr, "cli: %s is not available yet\n", cmd);
    return 2;
  }
  if (strcmp(cmd, "debug") != 0 || n < 3) {
    return usage();
  }
  const char *sub = argv[2];
  int nargs = n - 3;
  char **args = argv + 3;
  if (strcmp(sub, "ast") == 0) {
    return debug_ast(&a, nargs, args);
  }
  if (strcmp(sub, "options") == 0 && nargs == 0) {
    return debug_options();
  }
  static const struct {
    const char *name;
    ozni_mode mode;
  } modes[] = {{"definitions", OZNI_MODE_DEFINITIONS},
               {"steps", OZNI_MODE_STEPS},
               {"requires", OZNI_MODE_REQUIRES},
               {"ir", OZNI_MODE_IR},
               {"schedule", OZNI_MODE_SCHEDULE},
               {"entry", OZNI_MODE_ENTRY}};
  for (size_t k = 0; k < sizeof modes / sizeof modes[0]; k++) {
    if (strcmp(sub, modes[k].name) == 0 && nargs <= 1) {
      return check(&a, nargs, args, modes[k].mode);
    }
  }
  if (strcmp(sub, "modules") == 0 && nargs == 0) {
    return check(&a, 0, args, OZNI_MODE_MODULES);
  }
  if (strcmp(sub, "embedded-std") == 0 && nargs == 0) {
    int has_errors = 0;
    return emit_text(&a, ozni_run_text(&a, &limits, &settings, nullptr, 0, OZNI_MODE_EMBEDDED_STD, nullptr, &has_errors),
                     has_errors);
  }
  return usage();
}
