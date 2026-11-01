/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#include <string.h>

#include "check.h"

static const char batches_text[] = "kitchen home {\n"
                                   "  places {\n"
                                   "    stove: IHStove\n"
                                   "  }\n"
                                   "  tools {\n"
                                   "    pan: FryingPan { capacity: %s }\n"
                                   "  }\n"
                                   "}\n"
                                   "recipe porkBatches {\n"
                                   "  ingredients { pork: 500 g }\n"
                                   "  let batches = ceil(pork / kitchen.pan.capacity)\n"
                                   "  repeat batches {\n"
                                   "    let fried = fry(pork / batches, 8 min)\n"
                                   "  }\n"
                                   "}\n";

static char *schedule(test_alloc *t, const char *capacity, uint64_t steps, int *has_errors) {
  char text[sizeof batches_text + 16];
  int len = snprintf(text, sizeof text, batches_text, capacity);
  ozni_allocator a = test_allocator(t);
  ozni_limits lim = ozni_default_limits();
  if (steps != 0) {
    lim.steps = steps;
  }
  ozni_settings set = {.kitchen = "home", .entry = "porkBatches"};
  ozni_source src = {"batches.ozen", text, (size_t)len};
  return ozni_run_text(&a, &lim, &set, &src, 1, OZNI_MODE_SCHEDULE, nullptr, has_errors);
}

void test_sched_fuel_bound(void) {
  test_alloc t = {0};
  ozni_allocator a = test_allocator(&t);
  int errors = 0;
  char *few = schedule(&t, "250 g", 0, &errors);
  CHECK(few != nullptr && errors == 0);
  CHECK(few != nullptr && strstr(few, " fry") != nullptr && strstr(few, "MAC001") == nullptr);
  ozni_free_text(&a, few);
  char *many = schedule(&t, "1 g", 0, &errors);
  CHECK(many != nullptr && errors != 0);
  CHECK(many != nullptr && strstr(many, "MAC001 fuelExhausted") != nullptr &&
        strstr(many, "args=[\"1000000\"]") != nullptr);
  ozni_free_text(&a, many);
  char *small = schedule(&t, "1 g", 20000, &errors);
  CHECK(small != nullptr && strstr(small, "args=[\"20000\"]") != nullptr);
  ozni_free_text(&a, small);
  CHECK(t.live == 0);
}
