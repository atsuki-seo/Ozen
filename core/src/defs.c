/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#include <string.h>

#include "resolve.h"

typedef struct jw {
  const ozni_project *pr;
  ozni_buf *b;
  ozni_index file;
} jw;

static const ozni_node *nd(const jw *w, ozni_index i) { return ozni_node_get(&w->pr->asts[w->file], i); }

static void jstr(ozni_buf *b, const char *s, size_t n) {
  ozni_buf_puts(b, "\"");
  size_t start = 0;
  for (size_t i = 0; i < n; i++) {
    unsigned char ch = (unsigned char)s[i];
    const char *rep = ch == '"' ? "\\\"" : ch == '\\' ? "\\\\" : ch == '\n' ? "\\n" : ch == '\t' ? "\\t" : nullptr;
    char hex[8];
    if (rep == nullptr && ch < 0x20) {
      static const char digits[] = "0123456789abcdef";
      hex[0] = '\\';
      hex[1] = 'u';
      hex[2] = '0';
      hex[3] = '0';
      hex[4] = digits[ch >> 4];
      hex[5] = digits[ch & 15];
      hex[6] = '\0';
      rep = hex;
    }
    if (rep != nullptr) {
      ozni_buf_put(b, s + start, i - start);
      ozni_buf_puts(b, rep);
      start = i + 1;
    }
  }
  ozni_buf_put(b, s + start, n - start);
  ozni_buf_puts(b, "\"");
}

static void jid(const jw *w, ozni_index id) {
  ozni_str s = ozni_intern_get(w->pr->c->names, id);
  jstr(w->b, s.p, s.len);
}

static void jtext(const jw *w, const ozni_node *n) {
  const ozni_source *src = &w->pr->c->srcs[w->file];
  jstr(w->b, src->text + n->span.start, n->span.end - n->span.start);
}

static void key(const jw *w, const char *k, bool *first) {
  ozni_buf_puts(w->b, *first ? "\"" : ",\"");
  ozni_buf_puts(w->b, k);
  ozni_buf_puts(w->b, "\":");
  *first = false;
}

static void annotations(const jw *w, ozni_index parent) {
  ozni_buf_puts(w->b, "[");
  bool first = true;
  for (ozni_index c = nd(w, parent)->first; c != 0; c = nd(w, c)->next) {
    const ozni_node *a = nd(w, c);
    if (a->kind != OZNI_N_Annotation) {
      continue;
    }
    ozni_buf_puts(w->b, first ? "{\"name\":" : ",{\"name\":");
    first = false;
    jid(w, a->text);
    ozni_buf_puts(w->b, ",\"args\":[");
    bool f2 = true;
    for (ozni_index x = a->first; x != 0; x = nd(w, x)->next) {
      ozni_buf_puts(w->b, f2 ? "" : ",");
      f2 = false;
      jtext(w, nd(w, x));
    }
    ozni_buf_puts(w->b, "]}");
  }
  ozni_buf_puts(w->b, "]");
}

static void expr(const jw *w, ozni_index i) {
  const ozni_node *n = nd(w, i);
  ozni_buf_puts(w->b, "{\"text\":");
  const ozni_source *src = &w->pr->c->srcs[w->file];
  uint32_t end = n->span.end;
  for (ozni_index c = n->first; c != 0; c = nd(w, c)->next) {
    if (nd(w, c)->kind == OZNI_N_Annotation && nd(w, c)->span.start < end) {
      end = nd(w, c)->span.start;
      while (end > n->span.start && (src->text[end - 1] == ' ' || src->text[end - 1] == '\t')) {
        end--;
      }
      break;
    }
  }
  jstr(w->b, src->text + n->span.start, end - n->span.start);
  ozni_buf_puts(w->b, ",\"annotations\":");
  annotations(w, i);
  ozni_buf_puts(w->b, "}");
}

static void type(const jw *w, ozni_index i) {
  const ozni_node *n = nd(w, i);
  bool first = true;
  ozni_buf_puts(w->b, "{");
  key(w, "text", &first);
  jtext(w, n);
  if (n->kind != OZNI_N_TypeRef) {
    key(w, "form", &first);
    jstr(w->b, ozni_node_kind_name(n->kind), strlen(ozni_node_kind_name(n->kind)));
    key(w, "items", &first);
    ozni_buf_puts(w->b, "[");
    bool f2 = true;
    for (ozni_index c = n->first; c != 0; c = nd(w, c)->next) {
      ozni_buf_puts(w->b, f2 ? "" : ",");
      f2 = false;
      type(w, c);
    }
    ozni_buf_puts(w->b, "]}");
    return;
  }
  key(w, "name", &first);
  jid(w, n->text);
  for (ozni_index c = n->first; c != 0; c = nd(w, c)->next) {
    const ozni_node *cn = nd(w, c);
    if (cn->kind == OZNI_N_CallArgs) {
      key(w, "callArgs", &first);
      ozni_buf_puts(w->b, "[");
      bool f2 = true;
      for (ozni_index x = cn->first; x != 0; x = nd(w, x)->next) {
        ozni_buf_puts(w->b, f2 ? "" : ",");
        f2 = false;
        jtext(w, nd(w, x));
      }
      ozni_buf_puts(w->b, "]");
    } else if (cn->kind == OZNI_N_TypeArgs) {
      key(w, "alts", &first);
      ozni_buf_puts(w->b, "[");
      bool fa = true;
      for (ozni_index alt = cn->first; alt != 0; alt = nd(w, alt)->next) {
        ozni_buf_puts(w->b, fa ? "[" : ",[");
        fa = false;
        bool fx = true;
        for (ozni_index a = nd(w, alt)->first; a != 0; a = nd(w, a)->next) {
          const ozni_node *an = nd(w, a);
          ozni_buf_puts(w->b, fx ? "" : ",");
          fx = false;
          if (an->kind == OZNI_N_BareArg) {
            ozni_buf_puts(w->b, "{\"bare\":");
            jid(w, an->text);
            ozni_buf_puts(w->b, "}");
          } else {
            ozni_buf_puts(w->b, "{\"axis\":");
            jid(w, an->text);
            ozni_buf_puts(w->b, ",\"op\":");
            jid(w, an->text2);
            ozni_buf_puts(w->b, ",\"value\":");
            if (an->first != 0) {
              jtext(w, nd(w, an->first));
            } else {
              ozni_buf_puts(w->b, "null");
            }
            ozni_buf_puts(w->b, "}");
          }
        }
        ozni_buf_puts(w->b, "]");
      }
      ozni_buf_puts(w->b, "]");
    }
  }
  ozni_buf_puts(w->b, "}");
}

static void members(const jw *w, ozni_index parent);

static void member(const jw *w, ozni_index i, bool *first_out) {
  const ozni_node *n = nd(w, i);
  bool first = true;
  if (n->kind == OZNI_N_Field) {
    ozni_buf_puts(w->b, *first_out ? "{" : ",{");
    key(w, "field", &first);
    jid(w, n->text);
    key(w, "values", &first);
    ozni_buf_puts(w->b, "[");
    bool f2 = true;
    for (ozni_index c = n->first; c != 0; c = nd(w, c)->next) {
      if (nd(w, c)->kind == OZNI_N_Annotation) {
        continue;
      }
      ozni_buf_puts(w->b, f2 ? "" : ",");
      f2 = false;
      expr(w, c);
    }
    ozni_buf_puts(w->b, "]");
    key(w, "annotations", &first);
    annotations(w, i);
  } else if (n->kind == OZNI_N_Section || n->kind == OZNI_N_Variant) {
    ozni_buf_puts(w->b, *first_out ? "{" : ",{");
    key(w, n->kind == OZNI_N_Section ? "section" : "variant", &first);
    jid(w, n->text);
    key(w, "members", &first);
    members(w, i);
    key(w, "annotations", &first);
    annotations(w, i);
  } else if (n->kind == OZNI_N_Marker) {
    ozni_buf_puts(w->b, *first_out ? "{" : ",{");
    key(w, "marker", &first);
    jid(w, n->text);
  } else if (n->kind == OZNI_N_Uses) {
    ozni_buf_puts(w->b, *first_out ? "{" : ",{");
    key(w, "uses", &first);
    ozni_buf_puts(w->b, "[");
    bool f2 = true;
    ozni_index for_node = 0;
    for (ozni_index c = n->first; c != 0; c = nd(w, c)->next) {
      if (nd(w, c)->kind == OZNI_N_TypeRef) {
        ozni_buf_puts(w->b, f2 ? "" : ",");
        f2 = false;
        type(w, c);
      } else if (nd(w, c)->kind == OZNI_N_For) {
        for_node = c;
      }
    }
    ozni_buf_puts(w->b, "]");
    key(w, "for", &first);
    if (for_node != 0 && nd(w, for_node)->first != 0) {
      expr(w, nd(w, for_node)->first);
    } else {
      ozni_buf_puts(w->b, "null");
    }
    key(w, "annotations", &first);
    annotations(w, i);
  } else if (n->kind == OZNI_N_Takes || n->kind == OZNI_N_Steps) {
    ozni_buf_puts(w->b, *first_out ? "{" : ",{");
    key(w, n->kind == OZNI_N_Takes ? "takes" : "steps", &first);
    if (n->first != 0) {
      jtext(w, nd(w, n->first));
    } else {
      ozni_buf_puts(w->b, "null");
    }
  } else {
    return;
  }
  ozni_buf_puts(w->b, "}");
  *first_out = false;
}

static void members(const jw *w, ozni_index parent) {
  ozni_buf_puts(w->b, "[");
  bool first = true;
  for (ozni_index c = nd(w, parent)->first; c != 0; c = nd(w, c)->next) {
    member(w, c, &first);
  }
  ozni_buf_puts(w->b, "]");
}

static const char *kind_word(ozni_node_kind k) {
  static const struct {
    ozni_node_kind k;
    const char *w;
  } words[] = {
      {OZNI_N_Unit, "unit"},       {OZNI_N_Axis, "axis"},         {OZNI_N_Enum, "enum"},         {OZNI_N_Trait, "trait"},
      {OZNI_N_Ingredient, "ingredient"}, {OZNI_N_Kind, "kind"},   {OZNI_N_Vessel, "vessel"},     {OZNI_N_Dirt, "dirt"},
      {OZNI_N_Resource, "resource"}, {OZNI_N_Tool, "tool"},       {OZNI_N_Alias, "alias"},       {OZNI_N_Property, "property"},
      {OZNI_N_Source, "source"},   {OZNI_N_Action, "action"},     {OZNI_N_Recipe, "recipe"},     {OZNI_N_Menu, "menu"},
      {OZNI_N_Kitchen, "kitchen"}, {OZNI_N_Type, "type"},         {OZNI_N_Macro, "macro"},       {OZNI_N_Rule, "rule"},
      {OZNI_N_Name, "dimension"},
  };
  for (size_t i = 0; i < sizeof words / sizeof words[0]; i++) {
    if (words[i].k == k) {
      return words[i].w;
    }
  }
  return "other";
}

static void definition(const jw *w, const ozni_module *m, const ozni_def *d) {
  const ozni_node *n = nd(w, d->node);
  ozni_buf *b = w->b;
  bool first = true;
  ozni_buf_puts(b, "{");
  key(w, "kind", &first);
  const char *kw = kind_word(n->kind);
  jstr(b, kw, strlen(kw));
  key(w, "name", &first);
  jid(w, d->name);
  key(w, "module", &first);
  jid(w, m->name);
  key(w, "file", &first);
  ozni_buf_putu(b, m->file);
  key(w, "local", &first);
  ozni_buf_puts(b, d->local ? "true" : "false");
  if (n->text2 != 0 && (n->kind == OZNI_N_Axis || n->kind == OZNI_N_Enum)) {
    key(w, "ordering", &first);
    jid(w, n->text2);
  }
  key(w, "supers", &first);
  ozni_buf_puts(b, "[");
  bool fs = true;
  ozni_index output = 0, value = 0, rule_after = 0;
  for (ozni_index c = n->first; c != 0; c = nd(w, c)->next) {
    const ozni_node *cn = nd(w, c);
    if (cn->kind == OZNI_N_TypeRef && n->kind != OZNI_N_Unit && n->kind != OZNI_N_Rule && n->kind != OZNI_N_Alias) {
      ozni_buf_puts(b, fs ? "" : ",");
      fs = false;
      type(w, c);
    } else if (cn->kind == OZNI_N_Output) {
      output = cn->first;
    } else if (n->kind == OZNI_N_Rule && cn->kind == OZNI_N_TypeRef && rule_after == 0) {
      rule_after = c;
    } else if ((n->kind == OZNI_N_Unit || n->kind == OZNI_N_Alias || n->kind == OZNI_N_Property) &&
               cn->kind != OZNI_N_Annotation && cn->kind != OZNI_N_TypeRef) {
      value = c;
    } else if (n->kind == OZNI_N_Alias && cn->kind != OZNI_N_Annotation) {
      value = c;
    }
  }
  ozni_buf_puts(b, "]");
  if (n->kind == OZNI_N_Unit) {
    for (ozni_index c = n->first; c != 0; c = nd(w, c)->next) {
      if (nd(w, c)->kind == OZNI_N_TypeRef) {
        key(w, "dimension", &first);
        jid(w, nd(w, c)->text);
      }
    }
  }
  if (value != 0) {
    key(w, "value", &first);
    if (n->kind == OZNI_N_Alias) {
      type(w, value);
    } else {
      expr(w, value);
    }
  }
  if (output != 0) {
    key(w, "output", &first);
    type(w, output);
  }
  if (rule_after != 0) {
    key(w, "after", &first);
    jid(w, nd(w, rule_after)->text);
    key(w, "cases", &first);
    ozni_buf_puts(b, "[");
    bool fc = true;
    for (ozni_index c = n->first; c != 0; c = nd(w, c)->next) {
      const ozni_node *rc = nd(w, c);
      if (rc->kind != OZNI_N_RuleCase) {
        continue;
      }
      ozni_buf_puts(b, fc ? "{\"level\":" : ",{\"level\":");
      fc = false;
      jid(w, rc->text);
      ozni_buf_puts(b, ",\"when\":");
      if (rc->first == 0) {
        ozni_buf_puts(b, "null");
      } else if (nd(w, rc->first)->kind == OZNI_N_AnyUse) {
        ozni_buf_puts(b, "\"any use\"");
      } else {
        jid(w, nd(w, rc->first)->text);
      }
      ozni_buf_puts(b, "}");
    }
    ozni_buf_puts(b, "]");
  }
  key(w, "params", &first);
  ozni_buf_puts(b, "[");
  bool fp = true;
  for (ozni_index c = n->first; c != 0; c = nd(w, c)->next) {
    const ozni_node *pn = nd(w, c);
    if (pn->kind != OZNI_N_Param) {
      continue;
    }
    ozni_buf_puts(b, fp ? "{\"name\":" : ",{\"name\":");
    fp = false;
    jid(w, pn->text);
    ozni_index t = 0, dflt = 0;
    for (ozni_index x = pn->first; x != 0; x = nd(w, x)->next) {
      if (nd(w, x)->kind == OZNI_N_Default) {
        dflt = nd(w, x)->first;
      } else {
        t = x;
      }
    }
    ozni_buf_puts(b, ",\"type\":");
    if (t != 0) {
      type(w, t);
    } else {
      ozni_buf_puts(b, "null");
    }
    ozni_buf_puts(b, ",\"default\":");
    if (dflt != 0) {
      expr(w, dflt);
    } else {
      ozni_buf_puts(b, "null");
    }
    ozni_buf_puts(b, "}");
  }
  ozni_buf_puts(b, "]");
  key(w, "values", &first);
  ozni_buf_puts(b, "[");
  bool fv = true;
  for (ozni_index c = n->first; c != 0; c = nd(w, c)->next) {
    if (nd(w, c)->kind == OZNI_N_Value) {
      ozni_buf_puts(b, fv ? "" : ",");
      fv = false;
      jid(w, nd(w, c)->text);
    }
  }
  ozni_buf_puts(b, "]");
  key(w, "finer", &first);
  ozni_buf_puts(b, "[");
  bool ff = true;
  for (ozni_index c = n->first; c != 0; c = nd(w, c)->next) {
    const ozni_node *fn = nd(w, c);
    if (fn->kind != OZNI_N_Finer) {
      continue;
    }
    ozni_buf_puts(b, ff ? "{\"from\":" : ",{\"from\":");
    ff = false;
    jid(w, fn->text);
    ozni_buf_puts(b, ",\"to\":[");
    bool ft = true;
    for (ozni_index x = fn->first; x != 0; x = nd(w, x)->next) {
      ozni_buf_puts(b, ft ? "" : ",");
      ft = false;
      jid(w, nd(w, x)->text);
    }
    ozni_buf_puts(b, "]}");
  }
  ozni_buf_puts(b, "]");
  key(w, "members", &first);
  members(w, d->node);
  key(w, "annotations", &first);
  annotations(w, d->node);
  ozni_buf_puts(b, "}");
}

bool ozni_render_definitions(const ozni_project *pr, ozni_buf *b) {
  ozni_index nm = ozni_table_count(pr->modules);
  ozni_buf_puts(b, "{\"schemaVersion\":1,\"files\":[");
  for (size_t f = 0; f < pr->c->nsrcs; f++) {
    ozni_buf_puts(b, f == 0 ? "" : ",");
    jstr(b, pr->c->srcs[f].path, strlen(pr->c->srcs[f].path));
  }
  ozni_buf_puts(b, "],\"definitions\":[");
  bool first = true;
  for (ozni_index mi = 0; mi < nm; mi++) {
    const ozni_module *m = ozni_project_module(pr, mi);
    jw w = {pr, b, m->file};
    for (ozni_index d = 0; d < ozni_table_count(m->defs); d++) {
      ozni_buf_puts(b, first ? "\n" : ",\n");
      first = false;
      definition(&w, m, ozni_table_get(m->defs, d));
    }
  }
  ozni_buf_puts(b, "\n],\"diagnostics\":[");
  for (int id = 0; id < OZNI_D_COUNT; id++) {
    const char *code = ozni_diag_code((ozni_diag_id)id), *name = ozni_diag_name((ozni_diag_id)id);
    ozni_buf_puts(b, id == 0 ? "\n{\"code\":" : ",\n{\"code\":");
    jstr(b, code, strlen(code));
    ozni_buf_puts(b, ",\"name\":");
    jstr(b, name, strlen(name));
    ozni_buf_puts(b, "}");
  }
  ozni_buf_puts(b, "\n]}\n");
  return !b->failed;
}
