/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#include "sarif.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum jkind { J_NULL, J_BOOL, J_NUM, J_STR, J_ARR, J_OBJ } jkind;

typedef struct jv {
  jkind kind;
  long long num;
  char *str;
  struct jv *items;
  char **keys;
  size_t n;
} jv;

typedef struct reader {
  const char *p;
  const char *end;
  bool failed;
} reader;

static void skip(reader *r) {
  while (r->p < r->end && (*r->p == ' ' || *r->p == '\n' || *r->p == '\r' || *r->p == '\t')) {
    r->p++;
  }
}

static char *read_string(reader *r) {
  size_t cap = 16, n = 0;
  char *out = malloc(cap);
  r->p++;
  while (out != nullptr && r->p < r->end && *r->p != '"') {
    char ch = *r->p++;
    if (ch == '\\' && r->p < r->end) {
      char e = *r->p++;
      ch = e == 'n' ? '\n' : e == 't' ? '\t' : e == 'r' ? '\r' : e == 'b' ? '\b' : e == 'f' ? '\f' : e;
      if (e == 'u' && r->end - r->p >= 4) {
        unsigned v = 0;
        for (int k = 0; k < 4; k++) {
          char h = *r->p++;
          v = v * 16 + (unsigned)(h >= 'a' ? h - 'a' + 10 : h >= 'A' ? h - 'A' + 10 : h - '0');
        }
        ch = (char)v;
      }
    }
    if (n + 2 > cap) {
      cap *= 2;
      char *bigger = realloc(out, cap);
      if (bigger == nullptr) {
        free(out);
        out = nullptr;
        break;
      }
      out = bigger;
    }
    out[n++] = ch;
  }
  if (out == nullptr || r->p >= r->end) {
    free(out);
    r->failed = true;
    return nullptr;
  }
  r->p++;
  out[n] = '\0';
  return out;
}

static bool push(jv *v, jv item, char *key) {
  jv *items = realloc(v->items, (v->n + 1) * sizeof *items);
  if (items == nullptr) {
    return false;
  }
  v->items = items;
  if (v->kind == J_OBJ) {
    char **keys = realloc(v->keys, (v->n + 1) * sizeof *keys);
    if (keys == nullptr) {
      return false;
    }
    v->keys = keys;
    v->keys[v->n] = key;
  }
  v->items[v->n++] = item;
  return true;
}

static jv parse(reader *r, unsigned depth) {
  jv v = {J_NULL, 0, nullptr, nullptr, nullptr, 0};
  skip(r);
  if (r->p >= r->end || depth > 200) {
    r->failed = true;
    return v;
  }
  char ch = *r->p;
  if (ch == '{' || ch == '[') {
    v.kind = ch == '{' ? J_OBJ : J_ARR;
    r->p++;
    skip(r);
    if (r->p < r->end && *r->p == (ch == '{' ? '}' : ']')) {
      r->p++;
      return v;
    }
    while (!r->failed) {
      char *key = nullptr;
      if (v.kind == J_OBJ) {
        skip(r);
        if (r->p >= r->end || *r->p != '"') {
          r->failed = true;
          break;
        }
        key = read_string(r);
        skip(r);
        if (key == nullptr || r->p >= r->end || *r->p != ':') {
          free(key);
          r->failed = true;
          break;
        }
        r->p++;
      }
      jv item = parse(r, depth + 1);
      if (!push(&v, item, key)) {
        r->failed = true;
      }
      skip(r);
      if (r->p < r->end && *r->p == ',') {
        r->p++;
        continue;
      }
      if (r->p < r->end && *r->p == (ch == '{' ? '}' : ']')) {
        r->p++;
        break;
      }
      r->failed = true;
    }
    return v;
  }
  if (ch == '"') {
    v.kind = J_STR;
    v.str = read_string(r);
    return v;
  }
  if (strncmp(r->p, "true", 4) == 0 || strncmp(r->p, "false", 5) == 0) {
    v.kind = J_BOOL;
    v.num = *r->p == 't';
    r->p += *r->p == 't' ? 4 : 5;
    return v;
  }
  if (strncmp(r->p, "null", 4) == 0) {
    r->p += 4;
    return v;
  }
  v.kind = J_NUM;
  bool neg = *r->p == '-';
  r->p += neg ? 1 : 0;
  while (r->p < r->end && *r->p >= '0' && *r->p <= '9') {
    v.num = v.num * 10 + (*r->p++ - '0');
  }
  v.num = neg ? -v.num : v.num;
  return v;
}

static void release(jv *v) {
  for (size_t i = 0; i < v->n; i++) {
    release(&v->items[i]);
    if (v->keys != nullptr) {
      free(v->keys[i]);
    }
  }
  free(v->items);
  free(v->keys);
  free(v->str);
}

static const jv *get(const jv *o, const char *key) {
  for (size_t i = 0; o != nullptr && o->kind == J_OBJ && i < o->n; i++) {
    if (strcmp(o->keys[i], key) == 0) {
      return &o->items[i];
    }
  }
  return nullptr;
}

static const char *str_of(const jv *v) { return v != nullptr && v->kind == J_STR ? v->str : ""; }

static long long num_of(const jv *v) { return v != nullptr && v->kind == J_NUM ? v->num : 0; }

static void put_str(FILE *f, const char *s) {
  fputc('"', f);
  for (; *s != '\0'; s++) {
    unsigned char ch = (unsigned char)*s;
    if (ch == '"' || ch == '\\') {
      fputc('\\', f);
      fputc(ch, f);
    } else if (ch < 0x20) {
      fprintf(f, "\\u%04x", ch);
    } else {
      fputc(ch, f);
    }
  }
  fputc('"', f);
}

static void put_english(FILE *f, const char *name) {
  fputc('"', f);
  for (const char *s = name; *s != '\0'; s++) {
    if (*s >= 'A' && *s <= 'Z') {
      fputc(' ', f);
      fputc(*s - 'A' + 'a', f);
    } else if (*s != '"' && *s != '\\') {
      fputc(*s, f);
    }
  }
  fputc('"', f);
}

static const char *level_of(const char *severity) {
  return strcmp(severity, "error") == 0 ? "error" : strcmp(severity, "warning") == 0 ? "warning" : "note";
}

static void put_region(FILE *f, const jv *span) {
  const jv *start = get(span, "start"), *end = get(span, "end");
  fprintf(f, "{\"startLine\":%lld,\"startColumn\":%lld,\"endLine\":%lld,\"endColumn\":%lld}", num_of(get(start, "line")),
          num_of(get(start, "column")), num_of(get(end, "line")), num_of(get(end, "column")));
}

static void put_artifact(FILE *f, const jv *files, const jv *span) {
  long long file = num_of(get(span, "file"));
  const char *uri = files != nullptr && file >= 0 && (size_t)file < files->n ? str_of(&files->items[file]) : "";
  fputs("{\"uri\":", f);
  put_str(f, uri);
  fprintf(f, ",\"index\":%lld}", file);
}

static void put_location(FILE *f, const jv *files, const jv *span) {
  fputs("{\"physicalLocation\":{\"artifactLocation\":", f);
  put_artifact(f, files, span);
  fputs(",\"region\":", f);
  put_region(f, span);
  fputs("}}", f);
}

bool ozen_write_sarif(FILE *f, const char *json, size_t len) {
  reader r = {json, json + len, false};
  jv root = parse(&r, 0);
  if (r.failed || root.kind != J_OBJ) {
    release(&root);
    return false;
  }
  const jv *diags = get(&root, "diagnostics"), *files = get(&root, "files");
  size_t nd = diags != nullptr && diags->kind == J_ARR ? diags->n : 0;
  const char **rules = calloc(nd + 1, sizeof *rules);
  size_t nrules = 0;
  for (size_t i = 0; rules != nullptr && i < nd; i++) {
    const char *code = str_of(get(&diags->items[i], "code"));
    bool seen = false;
    for (size_t k = 0; k < nrules; k++) {
      seen = seen || strcmp(rules[k], code) == 0;
    }
    if (!seen) {
      rules[nrules++] = code;
    }
  }
  fputs("{\"$schema\":\"https://docs.oasis-open.org/sarif/sarif/v2.1.0/errata01/os/schemas/sarif-schema-2.1.0.json\","
        "\"version\":\"2.1.0\",\"runs\":[{\"tool\":{\"driver\":{\"name\":\"ozen\",\"version\":",
        f);
  put_str(f, str_of(get(&root, "ozenVersion")));
  fputs(",\"informationUri\":\"https://github.com/atsuki-seo/Ozen\",\"rules\":[", f);
  for (size_t k = 0; rules != nullptr && k < nrules; k++) {
    const jv *d = nullptr;
    for (size_t i = 0; i < nd && d == nullptr; i++) {
      d = strcmp(str_of(get(&diags->items[i], "code")), rules[k]) == 0 ? &diags->items[i] : nullptr;
    }
    fputs(k == 0 ? "{\"id\":" : ",{\"id\":", f);
    put_str(f, rules[k]);
    fputs(",\"name\":", f);
    put_str(f, str_of(get(d, "name")));
    fputs(",\"shortDescription\":{\"text\":", f);
    put_english(f, str_of(get(d, "name")));
    fprintf(f, "},\"defaultConfiguration\":{\"level\":\"%s\"}}", level_of(str_of(get(d, "defaultSeverity"))));
  }
  fputs("]}},\"artifacts\":[", f);
  for (size_t i = 0; files != nullptr && files->kind == J_ARR && i < files->n; i++) {
    fputs(i == 0 ? "{\"location\":{\"uri\":" : ",{\"location\":{\"uri\":", f);
    put_str(f, str_of(&files->items[i]));
    fputs("}}", f);
  }
  fputs("],\"results\":[", f);
  for (size_t i = 0; i < nd; i++) {
    const jv *d = &diags->items[i];
    const char *code = str_of(get(d, "code"));
    size_t index = 0;
    for (size_t k = 0; rules != nullptr && k < nrules; k++) {
      index = strcmp(rules[k], code) == 0 ? k : index;
    }
    fputs(i == 0 ? "{\"ruleId\":" : ",{\"ruleId\":", f);
    put_str(f, code);
    fprintf(f, ",\"ruleIndex\":%zu,\"level\":\"%s\",\"message\":{\"text\":", index, level_of(str_of(get(d, "severity"))));
    put_english(f, str_of(get(d, "name")));
    fputs(",\"arguments\":[", f);
    const jv *args = get(d, "args");
    for (size_t a = 0; args != nullptr && args->kind == J_ARR && a < args->n; a++) {
      fputs(a == 0 ? "" : ",", f);
      put_str(f, str_of(&args->items[a]));
    }
    fputs("]},\"locations\":[", f);
    put_location(f, files, get(d, "span"));
    fputs("]", f);
    const jv *related = get(d, "related");
    if (related != nullptr && related->kind == J_ARR && related->n > 0) {
      fputs(",\"relatedLocations\":[", f);
      for (size_t k = 0; k < related->n; k++) {
        fprintf(f, "%s{\"id\":%zu,\"physicalLocation\":{\"artifactLocation\":", k == 0 ? "" : ",", k);
        put_artifact(f, files, get(&related->items[k], "span"));
        fputs(",\"region\":", f);
        put_region(f, get(&related->items[k], "span"));
        fputs("},\"message\":{\"text\":", f);
        put_str(f, str_of(get(&related->items[k], "kind")));
        fputs("}}", f);
      }
      fputs("]", f);
    }
    const jv *fixes = get(d, "fixes");
    if (fixes != nullptr && fixes->kind == J_ARR && fixes->n > 0) {
      fputs(",\"fixes\":[", f);
      for (size_t k = 0; k < fixes->n; k++) {
        fputs(k == 0 ? "{\"description\":{\"text\":" : ",{\"description\":{\"text\":", f);
        put_str(f, str_of(&fixes->items[k]));
        fputs("},\"artifactChanges\":[{\"artifactLocation\":", f);
        put_artifact(f, files, get(d, "span"));
        fputs(",\"replacements\":[{\"deletedRegion\":", f);
        put_region(f, get(d, "span"));
        fputs(",\"insertedContent\":{\"text\":", f);
        put_str(f, str_of(&fixes->items[k]));
        fputs("}}]}]}", f);
      }
      fputs("]", f);
    }
    fputs("}", f);
  }
  fputs("]}]}\n", f);
  free(rules);
  release(&root);
  return true;
}
