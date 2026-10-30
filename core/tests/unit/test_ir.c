/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#include <string.h>

#include "check.h"
#include "sema.h"

static const char recipe_text[] = "recipe gohanAndOnion {\n"
                                  "  ingredients {\n"
                                  "    onion: 1 pcs\n"
                                  "    rice: 300 g\n"
                                  "  }\n"
                                  "  let peeled = peel(onion)\n"
                                  "  let soaked = soak(rice)\n"
                                  "  let cooked = cookRice(soaked)\n"
                                  "}\n";

typedef struct built {
  test_alloc alloc;
  ozni_allocator a;
  ozni_ctx c;
  ozni_project pr;
  ozni_ir ir;
  bool ok;
} built;

static void build(built *b, const ozni_source *src) {
  b->a = test_allocator(&b->alloc);
  ozni_limits lim = ozni_default_limits();
  b->ok = ozni_ctx_init(&b->c, &b->a, &lim, src, 1) && ozni_project_load(&b->c, src, 1, &b->pr) &&
          ozni_sema_run(&b->pr, OZNI_OUT_NONE, nullptr, &b->ir) && b->ir.nodes != nullptr;
}

static bool same_table(const ozni_table *x, const ozni_table *y, size_t size) {
  if (ozni_table_count(x) != ozni_table_count(y)) {
    return false;
  }
  for (ozni_index k = 0; k < ozni_table_count(x); k++) {
    if (memcmp(ozni_table_get(x, k), ozni_table_get(y, k), size) != 0) {
      return false;
    }
  }
  return true;
}

void test_ir_flat_ids(void) {
  static_assert(sizeof(ozni_ir_node) == 96, "IR nodes hold only integers without padding");
  static_assert(sizeof(ozni_ir_variant) == 48, "IR variants hold only integers without padding");
  ozni_source src = {"ir.ozen", recipe_text, sizeof recipe_text - 1};
  built *x = calloc(1, sizeof *x), *y = calloc(1, sizeof *y);
  CHECK(x != nullptr && y != nullptr);
  if (x == nullptr || y == nullptr) {
    free(x);
    free(y);
    return;
  }
  build(x, &src);
  build(y, &src);
  CHECK(x->ok && y->ok);
  if (x->ok && y->ok) {
    CHECK(ozni_table_count(x->ir.nodes) == 5);
    CHECK(same_table(x->ir.nodes, y->ir.nodes, sizeof(ozni_ir_node)));
    CHECK(same_table(x->ir.inputs, y->ir.inputs, sizeof(ozni_ir_input)));
    CHECK(same_table(x->ir.reqs, y->ir.reqs, sizeof(ozni_ir_req)));
    CHECK(same_table(x->ir.variants, y->ir.variants, sizeof(ozni_ir_variant)));
    CHECK(same_table(x->ir.afters, y->ir.afters, sizeof(ozni_index)));
    for (ozni_index k = 0; k < ozni_table_count(x->ir.nodes); k++) {
      const ozni_ir_node *n = ozni_table_get(x->ir.nodes, k);
      CHECK(n->first_input + n->ninputs <= ozni_table_count(x->ir.inputs));
      CHECK(n->first_req + n->nreqs <= ozni_table_count(x->ir.reqs));
      CHECK(n->first_variant + n->nvariants <= ozni_table_count(x->ir.variants));
    }
    for (ozni_index k = 0; k < ozni_table_count(x->ir.inputs); k++) {
      const ozni_ir_input *in = ozni_table_get(x->ir.inputs, k);
      CHECK(in->node >= 1 && in->node <= ozni_table_count(x->ir.nodes));
    }
  }
  ozni_ctx_free(&x->c);
  ozni_ctx_free(&y->c);
  CHECK(x->alloc.live == 0 && y->alloc.live == 0);
  free(x);
  free(y);
}
