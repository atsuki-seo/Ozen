/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#include <string.h>

#include "../../include/ozen.h"
#include "check.h"

static const char recipe_text[] = "recipe gohan {\n"
                                  "  ingredients {\n"
                                  "    rice: 300 g\n"
                                  "  }\n"
                                  "  let washed = wash(rice)\n"
                                  "}\n";
static const char kitchen_text[] = "kitchen home {\n"
                                   "  places {\n"
                                   "    stove: IHStove\n"
                                   "  }\n"
                                   "}\n";
static const char options_text[] = "kitchen today : Options {\n"
                                   "  servings: 2\n"
                                   "}\n";
static const char merged_text[] = "kitchen first : Options {\n"
                                  "  cooks: 1\n"
                                  "  servings: 2\n"
                                  "}\n"
                                  "kitchen second : Options {\n"
                                  "  cooks: 2\n"
                                  "}\n";

static recipe_config config(test_alloc *t) {
  ozni_allocator a = test_allocator(t);
  recipe_config cfg = recipe_default_config();
  cfg.alloc = a.alloc;
  cfg.release = a.release;
  cfg.alloc_ctx = a.ctx;
  return cfg;
}

static char *build(const recipe_config *cfg, const char *kitchen, const char *options) {
  recipe_source src = {"r.ozen", recipe_text, sizeof recipe_text - 1};
  return recipe_build(cfg, &src, 1, kitchen, kitchen != nullptr ? strlen(kitchen) : 0, options,
                      options != nullptr ? strlen(options) : 0);
}

void test_api_levels_by_null(void) {
  test_alloc t = {0};
  recipe_config cfg = config(&t);
  char *none = build(&cfg, nullptr, nullptr);
  char *opts = build(&cfg, nullptr, options_text);
  char *both = build(&cfg, kitchen_text, options_text);
  CHECK(none != nullptr && strstr(none, "\"level\":0,") != nullptr);
  CHECK(opts != nullptr && strstr(opts, "\"level\":1,") != nullptr);
  CHECK(both != nullptr && strstr(both, "\"level\":2,") != nullptr);
  CHECK(none != nullptr && strstr(none, "\"status\":\"ok\"") != nullptr);
  recipe_free_string(&cfg, none);
  recipe_free_string(&cfg, opts);
  recipe_free_string(&cfg, both);
}

void test_api_free_balance(void) {
  test_alloc t = {0};
  recipe_config cfg = config(&t);
  char *out = build(&cfg, kitchen_text, options_text);
  CHECK(out != nullptr);
  CHECK(t.live == 1);
  recipe_free_string(&cfg, out);
  CHECK(t.live == 0);
  recipe_config bad = recipe_default_config();
  CHECK(build(&bad, nullptr, nullptr) == nullptr);
}

void test_api_options_merge(void) {
  test_alloc t = {0};
  recipe_config cfg = config(&t);
  cfg.emit = RECIPE_EMIT_SCHEDULE;
  char *out = build(&cfg, nullptr, merged_text);
  CHECK(out != nullptr && strstr(out, "\"cooks\":2,") != nullptr);
  CHECK(out != nullptr && strstr(out, "\"level\":1,") != nullptr);
  recipe_free_string(&cfg, out);
  CHECK(t.live == 0);
}
