/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#include <string.h>

#include "resolve.h"
#include "text.h"

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

static void definitions_body(const ozni_project *pr, ozni_buf *b, bool public) {
  ozni_index nm = ozni_table_count(pr->modules);
  ozni_buf_puts(b, "\"definitions\":[");
  bool first = true;
  for (ozni_index mi = 0; mi < nm; mi++) {
    const ozni_module *m = ozni_project_module(pr, mi);
    jw w = {pr, b, m->file};
    for (ozni_index d = 0; d < ozni_table_count(m->defs); d++) {
      const ozni_def *def = ozni_table_get(m->defs, d);
      if (public && def->local) {
        continue;
      }
      ozni_buf_puts(b, first ? "\n" : ",\n");
      first = false;
      definition(&w, m, def);
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
  ozni_buf_puts(b, "\n]");
}

static bool provisional_in(const jw *w, ozni_index holder) {
  for (ozni_index c = nd(w, holder)->first; c != 0; c = nd(w, c)->next) {
    const ozni_node *cn = nd(w, c);
    for (ozni_index a = cn->kind == OZNI_N_Uses ? cn->first : 0; a != 0; a = nd(w, a)->next) {
      const ozni_node *an = nd(w, a);
      ozni_str nm = ozni_intern_get(w->pr->c->names, an->text);
      ozni_str arg = an->first != 0 ? ozni_intern_get(w->pr->c->names, nd(w, an->first)->text) : (ozni_str){"", 0};
      if (an->kind == OZNI_N_Annotation && nm.len == 6 && memcmp(nm.p, "source", 6) == 0 && arg.len == 11 &&
          memcmp(arg.p, "provisional", 11) == 0) {
        return true;
      }
    }
    if (cn->kind == OZNI_N_Variant && provisional_in(w, c)) {
      return true;
    }
  }
  return false;
}

static void signatures(const ozni_project *pr, ozni_buf *b) {
  ozni_buf_puts(b, "\"signatures\":[");
  bool first = true;
  for (ozni_index mi = 0; mi < ozni_table_count(pr->modules); mi++) {
    const ozni_module *m = ozni_project_module(pr, mi);
    jw w = {pr, b, m->file};
    for (ozni_index d = 0; d < ozni_table_count(m->defs); d++) {
      const ozni_def *def = ozni_table_get(m->defs, d);
      const ozni_node *n = nd(&w, def->node);
      if (def->local || n->kind != OZNI_N_Action) {
        continue;
      }
      ozni_buf_puts(b, first ? "\n{\"name\":" : ",\n{\"name\":");
      first = false;
      jid(&w, def->name);
      ozni_buf_puts(b, ",\"module\":");
      jid(&w, m->name);
      ozni_buf_puts(b, ",\"params\":[");
      bool fp = true;
      ozni_index output = 0;
      for (ozni_index c = n->first; c != 0; c = nd(&w, c)->next) {
        const ozni_node *pn = nd(&w, c);
        if (pn->kind == OZNI_N_Output) {
          output = pn->first;
        }
        if (pn->kind != OZNI_N_Param) {
          continue;
        }
        ozni_buf_puts(b, fp ? "{\"name\":" : ",{\"name\":");
        fp = false;
        jid(&w, pn->text);
        ozni_index ty = 0, dflt = 0;
        for (ozni_index x = pn->first; x != 0; x = nd(&w, x)->next) {
          if (nd(&w, x)->kind == OZNI_N_Default) {
            dflt = nd(&w, x)->first;
          } else {
            ty = x;
          }
        }
        ozni_buf_puts(b, ",\"type\":");
        if (ty != 0) {
          jtext(&w, nd(&w, ty));
        } else {
          ozni_buf_puts(b, "null");
        }
        ozni_buf_puts(b, ",\"default\":");
        if (dflt != 0) {
          jtext(&w, nd(&w, dflt));
        } else {
          ozni_buf_puts(b, "null");
        }
        ozni_buf_puts(b, "}");
      }
      ozni_buf_puts(b, "],\"output\":");
      if (output != 0) {
        jtext(&w, nd(&w, output));
      } else {
        ozni_buf_puts(b, "null");
      }
      ozni_buf_puts(b, ",\"requires\":[");
      bool fr = true;
      for (ozni_index c = n->first; c != 0; c = nd(&w, c)->next) {
        for (ozni_index r = nd(&w, c)->kind == OZNI_N_Uses ? nd(&w, c)->first : 0; r != 0; r = nd(&w, r)->next) {
          if (nd(&w, r)->kind == OZNI_N_TypeRef) {
            ozni_buf_puts(b, fr ? "" : ",");
            fr = false;
            jtext(&w, nd(&w, r));
          }
        }
      }
      ozni_buf_puts(b, "],\"variants\":[");
      bool fv = true;
      for (ozni_index c = n->first; c != 0; c = nd(&w, c)->next) {
        if (nd(&w, c)->kind == OZNI_N_Variant) {
          ozni_buf_puts(b, fv ? "" : ",");
          fv = false;
          jid(&w, nd(&w, c)->text);
        }
      }
      ozni_buf_puts(b, provisional_in(&w, def->node) ? "],\"provisional\":true}" : "],\"provisional\":false}");
    }
  }
  ozni_buf_puts(b, "\n]");
}

static bool find_decl(const ozni_project *pr, ozni_index mod, const char *name, ozni_node_kind kind, ozni_index *file,
                      ozni_index *node) {
  ozni_index id;
  if (!ozni_intern_put(pr->c->names, name, strlen(name), &id)) {
    return false;
  }
  ozni_index m, d;
  if (!ozni_project_find(pr, mod, id, &m, &d)) {
    return false;
  }
  const ozni_module *mm = ozni_project_module(pr, m);
  *file = mm->file;
  *node = ((const ozni_def *)ozni_table_get(mm->defs, d))->node;
  return ozni_project_node(pr, *file, *node)->kind == kind;
}

static ozni_index module_of(const ozni_project *pr, ozni_index file) {
  for (ozni_index mi = 0; mi < ozni_table_count(pr->modules); mi++) {
    if (ozni_project_module(pr, mi)->file == file) {
      return mi;
    }
  }
  return pr->prelude;
}

static void put_field(const jw *w, ozni_index f, bool *first) {
  const ozni_node *fn = nd(w, f);
  ozni_index ty = 0, dflt = 0;
  for (ozni_index x = fn->first; x != 0; x = nd(w, x)->next) {
    if (nd(w, x)->kind == OZNI_N_Default) {
      dflt = nd(w, x)->first;
    } else if (nd(w, x)->kind != OZNI_N_Annotation && nd(w, x)->kind != OZNI_N_Check) {
      ty = x;
    }
  }
  ozni_buf_puts(w->b, *first ? "{\"name\":" : ",{\"name\":");
  *first = false;
  jid(w, fn->text);
  ozni_index base = ty, lo = 0, hi = 0;
  if (ty != 0 && nd(w, ty)->kind == OZNI_N_RangedType && nd(w, ty)->first != 0) {
    base = nd(w, ty)->first;
    ozni_index r = nd(w, base)->next;
    const ozni_node *rn = r != 0 ? nd(w, r) : nullptr;
    if (rn != nullptr && rn->kind == OZNI_N_Range) {
      lo = rn->first;
      hi = lo != 0 ? nd(w, lo)->next : 0;
    } else if (rn != nullptr && rn->kind == OZNI_N_RangeFrom) {
      lo = rn->first;
    } else if (rn != nullptr && rn->kind == OZNI_N_RangeTo) {
      hi = rn->first;
    }
  }
  ozni_buf_puts(w->b, ",\"type\":");
  if (base != 0) {
    jtext(w, nd(w, base));
  } else {
    ozni_buf_puts(w->b, "null");
  }
  const ozni_index ends[3] = {lo, hi, dflt};
  static const char *const names[3] = {",\"min\":", ",\"max\":", ",\"default\":"};
  for (int k = 0; k < 3; k++) {
    ozni_buf_puts(w->b, names[k]);
    if (ends[k] != 0) {
      jtext(w, nd(w, ends[k]));
    } else {
      ozni_buf_puts(w->b, "null");
    }
  }
  ozni_buf_puts(w->b, "}");
}

static void type_fields(const ozni_project *pr, ozni_buf *b, ozni_index file, ozni_index node, bool *first,
                        unsigned depth) {
  jw w = {pr, b, file};
  if (depth > 8) {
    return;
  }
  for (ozni_index c = nd(&w, node)->first; c != 0; c = nd(&w, c)->next) {
    ozni_index sf, sn;
    char name[128];
    ozni_str sup = ozni_intern_get(pr->c->names, nd(&w, c)->text);
    if (nd(&w, c)->kind != OZNI_N_TypeRef || sup.len >= sizeof name) {
      continue;
    }
    ozni_copy(name, sup.p, sup.len);
    name[sup.len] = '\0';
    if (find_decl(pr, module_of(pr, file), name, OZNI_N_Type, &sf, &sn)) {
      type_fields(pr, b, sf, sn, first, depth + 1);
    }
  }
  for (ozni_index c = nd(&w, node)->first; c != 0; c = nd(&w, c)->next) {
    if (nd(&w, c)->kind == OZNI_N_TypeField) {
      put_field(&w, c, first);
    }
  }
}

static bool kind_has_super(const ozni_project *pr, ozni_index file, ozni_index node, const char *want, unsigned depth) {
  jw w = {pr, nullptr, file};
  if (depth > 16) {
    return false;
  }
  for (ozni_index c = nd(&w, node)->first; c != 0; c = nd(&w, c)->next) {
    ozni_str sup = ozni_intern_get(pr->c->names, nd(&w, c)->text);
    char name[128];
    ozni_index sf, sn;
    if (nd(&w, c)->kind != OZNI_N_TypeRef || sup.len >= sizeof name) {
      continue;
    }
    if (sup.len == strlen(want) && memcmp(sup.p, want, sup.len) == 0) {
      return true;
    }
    ozni_copy(name, sup.p, sup.len);
    name[sup.len] = '\0';
    if ((find_decl(pr, module_of(pr, file), name, OZNI_N_Kind, &sf, &sn) ||
         find_decl(pr, module_of(pr, file), name, OZNI_N_Trait, &sf, &sn) ||
         find_decl(pr, module_of(pr, file), name, OZNI_N_Vessel, &sf, &sn)) &&
        kind_has_super(pr, sf, sn, want, depth + 1)) {
      return true;
    }
  }
  return false;
}

static bool spec_of(const ozni_project *pr, ozni_index file, ozni_index node, ozni_index *sf, ozni_index *sn,
                    unsigned depth) {
  jw w = {pr, nullptr, file};
  ozni_str nm = ozni_intern_get(pr->c->names, nd(&w, node)->text);
  char name[160];
  if (depth > 16 || nm.len + 5 > sizeof name) {
    return false;
  }
  ozni_copy(name, nm.p, nm.len);
  ozni_copy(name + nm.len, "Spec", 5);
  if (find_decl(pr, module_of(pr, file), name, OZNI_N_Type, sf, sn)) {
    return true;
  }
  for (ozni_index c = nd(&w, node)->first; c != 0; c = nd(&w, c)->next) {
    ozni_str sup = ozni_intern_get(pr->c->names, nd(&w, c)->text);
    ozni_index kf, kn;
    if (nd(&w, c)->kind != OZNI_N_TypeRef || sup.len >= sizeof name) {
      continue;
    }
    ozni_copy(name, sup.p, sup.len);
    name[sup.len] = '\0';
    if ((find_decl(pr, module_of(pr, file), name, OZNI_N_Kind, &kf, &kn) ||
         find_decl(pr, module_of(pr, file), name, OZNI_N_Trait, &kf, &kn) ||
         find_decl(pr, module_of(pr, file), name, OZNI_N_Vessel, &kf, &kn)) &&
        spec_of(pr, kf, kn, sf, sn, depth + 1)) {
      return true;
    }
  }
  return false;
}

static bool is_abstract(const jw *w, ozni_index node) {
  for (ozni_index c = nd(w, node)->first; c != 0; c = nd(w, c)->next) {
    const ozni_node *cn = nd(w, c);
    ozni_str nm = ozni_intern_get(w->pr->c->names, cn->text);
    if (cn->kind == OZNI_N_Field && nm.len == 8 && memcmp(nm.p, "abstract", 8) == 0) {
      return true;
    }
  }
  return false;
}

static void types(const ozni_project *pr, ozni_buf *b) {
  static const char *const tops[2] = {"Kitchen", "Options"};
  static const char *const keys[2] = {"\"types\":{\"kitchen\":[", "],\"options\":["};
  for (int k = 0; k < 2; k++) {
    ozni_buf_puts(b, keys[k]);
    ozni_index tf, tn;
    bool first = true;
    if (find_decl(pr, pr->prelude, tops[k], OZNI_N_Type, &tf, &tn)) {
      type_fields(pr, b, tf, tn, &first, 0);
    }
  }
  for (int vessel = 0; vessel < 2; vessel++) {
    ozni_buf_puts(b, vessel == 0 ? "],\"equipment\":[" : "],\"vessels\":[");
    bool first = true;
    for (ozni_index mi = 0; mi < ozni_table_count(pr->modules); mi++) {
      const ozni_module *m = ozni_project_module(pr, mi);
      jw w = {pr, b, m->file};
      for (ozni_index d = 0; d < ozni_table_count(m->defs); d++) {
        const ozni_def *def = ozni_table_get(m->defs, d);
        const ozni_node *n = nd(&w, def->node);
        if (def->local || n->kind != (vessel ? OZNI_N_Vessel : OZNI_N_Kind) || is_abstract(&w, def->node)) {
          continue;
        }
        ozni_buf_puts(b, first ? "{\"kind\":" : ",{\"kind\":");
        first = false;
        jid(&w, def->name);
        if (!vessel) {
          bool tool = kind_has_super(pr, m->file, def->node, "Tool", 0);
          ozni_buf_puts(b, tool ? ",\"category\":\"tool\"" : ",\"category\":\"place\"");
        }
        ozni_buf_puts(b, ",\"fields\":[");
        ozni_index sf, sn;
        bool ff = true;
        if (spec_of(pr, m->file, def->node, &sf, &sn, 0) ||
            find_decl(pr, pr->prelude, vessel ? "VesselSpec" : "EquipmentSpec", OZNI_N_Type, &sf, &sn)) {
          type_fields(pr, b, sf, sn, &ff, 0);
        }
        ozni_buf_puts(b, "]}");
      }
    }
  }
  ozni_buf_puts(b, "]}");
}

bool ozni_render_public(const ozni_project *pr, ozni_buf *b, unsigned emit) {
  if ((emit & 8u) != 0) {
    ozni_buf_puts(b, ",");
    signatures(pr, b);
  }
  if ((emit & 16u) != 0) {
    ozni_buf_puts(b, ",\"definitions\":{");
    definitions_body(pr, b, true);
    ozni_buf_puts(b, "}");
  }
  if ((emit & 32u) != 0) {
    ozni_buf_puts(b, ",");
    types(pr, b);
  }
  return !b->failed;
}

bool ozni_render_definitions(const ozni_project *pr, ozni_buf *b) {
  ozni_buf_puts(b, "{\"schemaVersion\":1,\"files\":[");
  for (size_t f = 0; f < pr->c->nsrcs; f++) {
    ozni_buf_puts(b, f == 0 ? "" : ",");
    jstr(b, pr->c->srcs[f].path, strlen(pr->c->srcs[f].path));
  }
  ozni_buf_puts(b, "],");
  definitions_body(pr, b, false);
  ozni_buf_puts(b, "}\n");
  return !b->failed;
}
