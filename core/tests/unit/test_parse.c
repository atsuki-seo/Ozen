/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#include <string.h>

#include "ast.h"
#include "check.h"
#include "text.h"

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

static long walk(const ozni_ast *ast, ozni_index i, ozni_span parent, size_t len) {
  long bad = 0;
  for (ozni_index c = ozni_node_get(ast, i)->first; c != 0; c = ozni_node_get(ast, c)->next) {
    const ozni_node *n = ozni_node_get(ast, c);
    if (n->span.start > n->span.end || n->span.end > len || n->span.start < parent.start ||
        n->span.end > parent.end) {
      fprintf(stderr, "span %u-%u of %s outside %u-%u\n", n->span.start, n->span.end,
              ozni_node_kind_name(n->kind), parent.start, parent.end);
      bad++;
    }
    bad += walk(ast, c, n->span, len);
  }
  return bad;
}

void test_span_nesting(void) {
  CHECK(ozn_test_argc > 0);
  long nodes = 0;
  for (int k = 0; k < ozn_test_argc; k++) {
    size_t len = 0;
    char *text = slurp(ozn_test_argv[k], &len);
    CHECK(text != nullptr);
    size_t bad_at = 0;
    if (text == nullptr || !ozni_check_utf8(text, len, &bad_at)) {
      free(text);
      continue;
    }
    test_alloc t = {0};
    ozni_allocator a = test_allocator(&t);
    ozni_limits lim = ozni_default_limits();
    ozni_source src = {ozn_test_argv[k], text, len};
    ozni_ctx c;
    CHECK(ozni_ctx_init(&c, &a, &lim, &src, 1));
    ozni_ast ast = {0};
    CHECK(ozni_parse(&c, 0, &ast));
    const ozni_node *root = ozni_node_get(&ast, ast.root);
    CHECK(root != nullptr && root->span.start == 0 && root->span.end == len);
    long bad = root != nullptr ? walk(&ast, ast.root, root->span, len) : 1;
    if (bad != 0) {
      fprintf(stderr, "%s: %ld spans outside their parent\n", ozn_test_argv[k], bad);
    }
    CHECK(bad == 0);
    nodes += ozni_table_count(ast.nodes);
    ozni_ctx_free(&c);
    CHECK(t.live == 0);
    free(text);
  }
  CHECK(nodes > 0);
}
