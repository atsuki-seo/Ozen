# Implementation plan for Ozen's syntax improvements, bug fixes, test rewrite and prototype removal

- Created: 2026-10-28
- Reference source: none (no code reference source). The design rests on `docs/concept.md` and on the direction the user and I set in the conversation of 2026-10-27 to 28 (add a pipe operator, do not adopt rebinding, omit `()` on recipes without arguments, bring the production tests close to real cooking down to ingredients and amounts, write the plan first, remove the prototype)
- Target: `/home/atsuki/ドキュメント/Ozen`, `main` (D40). The starting point is `09e05de` (up to the record of the roadmap's P16-4. There is 1 commit not yet pushed)
- Premise rules: `CLAUDE.md` (work on `main` only, run `G-REPO-NOREPLY` before pushing, `plans/` in Japanese, committed artifacts in English), `~/.claude/rules/plan-language.md`, `~/.claude/rules/repo-language.md`, `~/.claude/rules/git-commit.md`, `~/.claude/rules/comment-style.md`. The goals in §5 of the existing plan `plans/ozen-roadmap.md` (below, "the roadmap") are cited as prerequisite goals (§5)

## 0. How to read this plan

- This plan covers the work that follows the roadmap (§13-1 to §13-12, done through P16-4), and it continues the roadmap's numbering. Design decisions start at **D40** and spec items at **S479** (the roadmap ends at D39 and S478). To refer to a roadmap number, write "roadmap D12" or "roadmap S85".
- Every task's completion condition is written **so that a command decides pass or fail**. Checks by a person are not tasks; they go to the hand-over list in §8.
- Completion conditions refer to the prerequisite goals listed in §5 and to the `G-RF-*` goals this plan defines in §5. The code blocks in §5 are authoritative for the judgment commands. They are not written in tables (inside a table, `|` becomes `\|`).
- The design decisions D40 to D56 in §2 were decided on 2026-10-30 by confirming them with the user one by one (all 17 confirmed individually; for D51, D54 and D55 the alternative was chosen). Each section starts with a "Decision:" line, and the comparison of the recommendation and the alternatives is kept as history (P0-1).
- The facts about the target were confirmed on 2026-10-28 by reading the source at `09e05de` and running `build/core-gcc-release/ozen` (§3). After writing, the plan went through one audit by an independent subagent, and its findings were taken in (§9).
- Each checkable claim in §4 carries a spec item ID. The coverage table in §5.3 shows, for each S item, a test or a hand-over to §8 (with a reason).
- Expected values are made as in roadmap D20. Expected values are written by hand from §4 and placed in a commit before the implementation. Schedules are built by hand, and become expected values only after `tools/schedcheck/` confirms 0 violations. The implementation's output is not copied.

---

## 1. Purpose and scope

**Purpose**: Let Ozen recipes be written briefly, in a form close to real cooking steps. At the same time, fix the bugs found while looking into how recipes are written. The inputs of the production tests are now symbolic, like `recipe r() { … let w = wash(rice) … }`. Rewrite them in a form close to real cooking, so that reading the tests shows how to use the language as is. Remove the prototype `proto/`, which has done its job.

An example of how writing changes (the current way → the way after this plan):

```ozen
// 今
recipe r() {
  ingredients {
    rice: 300 g
    onion: 1 pcs
  }
  let w = wash(rice)
  let s = soak(w)
  let c = cookRice(s)
  let p = peel(onion)
  let o = cut(p, style: slice)
  let f = fry(o, 5 min)
}

// 本計画の後
recipe gohanToTamanegi(servings: 2) {
  ingredients {
    rice:  75 g per serving
    onion: 1 pcs
  }
  let gohan = rice |> wash |> soak |> cookRice
  let fried = onion |> peel |> cut(style: slice) |> fry(5 min)
  serve main {
    plate perPerson on Bowl { gohan }
    plate shared on FlatPlate { fried }
  }
}
```

### In scope

- The pipe operator `|>` (D42, D43)
- Making the form that omits `()` on `recipe` and `menu` without arguments the standard one, and making `make 名前` the same as `make 名前()` (D44)
- Amounts per person, `per serving` (D46)
- Bug fixes: the dimension of Count-typed values such as servings (D45), the dimension check of ingredient amounts, the servings passed with `make`, and `@serveWithin` under `--serve-at` (§3.4)
- The diagnostic for unused values, `LIN005 unusedValue` (D47)
- Preheating tools (D48)
- The time limit from making to using, `@useWithin` (D49)
- Rewriting the production tests (`tests/`, `core/tests/`, `bindings/inputs/`. D50, D51)
- Rewriting the samples (`samples/`) with pipes and `per serving` (D54. The IR, schedule and rendered outputs do not change)
- Removing the prototype `proto/` (D52)
- Updating the concept, the README and `CLAUDE.md` (D53). The version stays at `0.1.0` (D55)

### Out of scope (reasons)

- **Rebinding the same name (`let onion = peel(onion)`)**: it goes against the decision in concept §4.1 and §4.4 that "`let` binds only once (SSA)". Pipes reduce the effort of thinking up names.
- **Optional ingredients (`?`), proportional amounts (`1 % of pork`), and continuing in the same pot**: these came up as candidates in the conversation, but only preheating, `@useWithin` and per-person amounts went into the request.
- **Adding oven actions such as `bake` to `std`**: the range of `std` is set by the list in concept §12.2 and the reference menus, and `G-STD-LISTS` checks it. Preheating is checked with actions defined inside the tests.
- **Holes in the check of requirements on the kitchen (`:` and `has` are not checked, items missing from the kitchen count as met. B7 in §3.4)**: this plan handles only the `heated` of preheating. When the kitchen has no `maxTemp`, the handling follows the current `req_met` (S581). The rest goes to a separate plan (§9).
- **The duplicate heading numbers in the roadmap (two each of D33 and D34) and the mismatch in the number of grammar rules (130 vs 131)**: the roadmap is not rewritten (D41). These are only reported to the user (§9).

---

## 2. Design decisions

### D40. Where the work happens and the base of the range [decided]

- **Decision:** As recommended, work on `main`, and count differences from the tag `refresh-start` set in P0-2. (2026-10-30, confirmed by the user)
- **Recommendation**: Work on `main` (`CLAUDE.md`, roadmap D2). In P0-2, set the tag `refresh-start` on `main`, and count this plan's range of differences as `RF=refresh-start..HEAD`.
  - On 2026-10-30 the user said "basically keep pushing", so pushes happen without asking each time. Run `G-REPO-NOREPLY` before every push. The starting point `09e05de` is also pushed together with this file.
  - Another session had been committing to the same `main` (196 commits after the conversation began). At the start of each phase, use `git log` to see whether other work has come in (R6).
- Alternative: create a work branch `syntax-refresh` and merge it into `main` at the end. It collides less with parallel work, but goes against "`main` only" in `CLAUDE.md`.

### D41. Where the plan file and the goals live [decided]

- **Decision:** As recommended, place this file as a new plan and do not rewrite the roadmap. Add the goals to `goals.sh` as `G-RF-*`, and add this file to the Plan section of `CLAUDE.md`. (2026-10-30, confirmed by the user)
- **Recommendation**: Place this file as a new plan. Keep the roadmap as a finished record and do not rewrite it. The only exception is the addition to `scripts/retired-tests.txt` that comes with removing the prototype (a file outside the roadmap). Add the goal functions to `scripts/goals.sh` as `G-RF-*`, and add a variable `PLAN_RF` that points to this file. `G-REPO-VERSION`, which reads the version line, keeps comparing with the roadmap (since D55 keeps the version, the roadmap's "Version: 0.1.0" line can be used as is). Add this file to the "Plan" section of `CLAUDE.md`.
- Alternative: add to the roadmap as Phase 17 onward. The goals and the coverage table stay in one place, but the roadmap, already over 3,400 lines, gets even longer, and finished and ongoing plans can no longer be told apart.

### D42. The pipe's symbol and meaning [decided]

- **Decision:** As recommended, the symbol is `|>`, and the left value becomes the first positional argument. The right side is limited to a name or a call of a name, and the precedence is the lowest within an expression. (2026-10-30, confirmed by the user)
- **Recommendation**: The symbol is `|>`. Read `a |> f` as `f(a)`, and `a |> f(x, k: v)` as `f(a, x, k: v)` (the left value becomes the first positional argument). The right side is limited to a name, or a call of a name (an action, macro or built-in). The precedence is the lowest within an expression (`expr := pipe ; pipe := link { '|>' callee }`).
  - Reason: every std action takes the food as its first argument. It reads left to right in cooking order. It is the same form as in F# and Elixir, so learners are less likely to get lost.
  - `.` could not be told apart from field access (`kitchen.floor`), nor `->` from a return type. `|>` does not appear in valid code under the current grammar (a type or a name comes right after `|`).
- Alternative A: use the word `then` (`rice then wash then soak`). It adds a reserved word, and `then` can no longer be used as a name.
- Alternative B: put the left value at the position of `_` (`a |> cut(_, style: slice)`). It can go into any argument, but since std actions take the food as the first argument, the benefit is small and there is more to write.

### D43. The pipe's syntax tree [decided]

- **Decision:** As recommended, rewrite it into a call (`OZNI_N_Call`) at the parsing stage, and record the grammar rule `pipe` with `mark`. (2026-10-30, confirmed by the user)
- **Recommendation**: Rewrite it into a call (`OZNI_N_Call`) at the parsing stage. If the right side is a call, insert the left side at the head of its arguments. If the right side is a name, make a call with one argument (this adds one node and counts toward `--max-nodes`). The call's span runs from the start of the left side to the end of the right side. Record the grammar rule `pipe` with `mark`, and confirm coverage with `G-CORE-GRAMMAR`.
  - Reason: name resolution, evaluation, fuel counting, the annotation check (`check_targets`) and the binding of macro arguments all treat a call as "the first child is the callee, the rest are arguments" (§3.2). Rewriting means none of these need to change.
- Alternative: make a dedicated node `Pipe`. `debug ast` would show the form as written, but branches would have to be added in 5 or more places: evaluation, fuel, `check_targets`, macro handling and so on.

### D44. Omitting `()` and `make` [decided]

- **Decision:** As recommended, make the form that omits `()` the standard one for `recipe` and `menu` without arguments, and evaluate `make 名前` the same as `make 名前()`. Remove empty `()` from authored sources. (2026-10-30, confirmed by the user)
- **Recommendation**: Make the form that omits `()` the standard one for `recipe` and `menu` without arguments (the grammar already allows omitting it). Remove empty `()` from authored sources (the `.ozen` files and heredocs in `tests/`, the C strings in `core/tests/unit/`, `bindings/inputs/`, `renderers/test/inputs/`, the README, the concept's examples). Only tests that compare with and without `()` are kept, listed with a reason in an exceptions list. Evaluate `make 名前` (without parentheses) the same as `make 名前()` (now it makes nothing and gives no diagnostic. B3 in §3.4). The IR display (`%1 = make steamedRice() : …`) is tool output, so it does not change.
  - Reason: it matches other declarations that take no arguments, such as `kitchen myHome {` and `type Options {`.
- Alternative: make `()` required on `recipe` and `menu`, and make `make 名前` a syntax error. It looks the same as a call, but it changes the grammar and splits the writing style from other declarations.

### D45. The dimension of counts such as servings [decided]

- **Decision:** As recommended, hold the values of `Count`-typed arguments and record fields as dimensionless numbers, and use the Count dimension (`pcs`) only for ingredient amounts. (2026-10-30, confirmed by the user)
- **Recommendation**: Hold the values of `Count`-typed arguments and record fields (`servings` and `cooks` of the build-time conditions, `count` and `burners` of the kitchen, and so on) as **dimensionless numbers**. Even when written with `pcs`, they become dimensionless numbers. Use the Count dimension (unit `pcs`) only for ingredient amounts.
  - Effect: `150 g * servings` becomes `g`, not `g*pcs` (B1). `servings > 4` no longer gives `TYP001` (B2).
  - Reason: the 4 in "4 servings" is used as a multiplier, not as a number of pieces. Keep `pcs` on ingredients, because `to_mass` needs that dimension when it multiplies by the weight of one piece.
- Alternative: keep the Count dimension, and add a rule that treats Count the same as dimensionless in multiplication, division and comparison. The form of values does not change, but expressions that mix pieces and numbers, like `2 pcs + 3`, pass silently.

### D46. Amounts per person [decided]

- **Decision:** As recommended, only amounts written with `per serving` in the ingredient declaration scale with the servings; unmarked amounts stay totals. If the servings cannot be determined, `TYP020 servingsUnknown`. (2026-10-30, confirmed by the user)
- **Recommendation**: Only amounts written with `per serving` in the ingredient declaration scale with the servings (`rice: 75 g per serving`). Unmarked amounts are totals that do not change with the servings, as now. The servings come from the dish's argument `servings`, or, if there is none, from `servings` of the build-time conditions. If neither exists, give a new error `TYP020 servingsUnknown` (`TYP018` and `TYP019` are taken). `per` and `serving` are not reserved words; like `on`, they are read as contextual words.
  - Reason: the amounts (totals) of the existing samples and tests stay as they are. The author can distinguish amounts that change with the servings from those that do not.
- Alternative A: as in Cooklang, scale unmarked amounts with the servings and mark only the fixed amounts. Most ingredients in home recipes scale, so there is less to write, but every amount in the existing samples and tests changes meaning.
- Alternative B: add no notation and write `75 g * servings` (D45 makes the dimension correct). The language does not change, but a reader cannot tell whether it is a total or one person's share without reading the expression.

### D47. The diagnostic for unused values [decided]

- **Decision:** As recommended, report unused food values and ingredients that are only declared with `LIN005 unusedValue` (warning). Do not report parts, `store`, `discard`, the last value, anything inside `make`, or anything inside a composed action. (2026-10-30, confirmed by the user)
- **Recommendation**: At the end of evaluating a dish, report with `LIN005 unusedValue` (**warning**) every food value that meets all of the following: not consumed, not the last value (or part of its tuple), not served, not a part (outputs of split, butchering or tuples; these are covered by `LIN003`), and not in the middle of taking out portions by division. Include ingredients that are only declared and not used. Values passed through `store(x)` or `discard(x)` count as consumed, so they are not reported. Do not report values inside the evaluation of a dish called by `make`, or inside the `steps` of a composed action.
  - Reason: throwing away a whole pot of cooked rice is almost always a mistake. Leftover parts (often left on purpose) stay as the info `LIN003`, as now (concept §4.1).
- Alternative A: make the severity info. It matches leftover parts, but it is hard to notice when looking at expected values with `warnings=0`.
- Alternative B: widen what `LIN003 leftoverPart` covers. No code is added, but the name ("part") does not fit, and the severity cannot be set apart from leftover parts.

### D48. Preheating tools [decided]

- **Decision:** As recommended, from `HeatSource<oven, heated: t>` in `uses`, the scheduler puts a preheat, planned ahead, on the same unit. If `maxTemp` is lower, `KIT002`; if there is none, it counts as met. Preheating does not go into the IR. (2026-10-30, confirmed by the user)
- **Recommendation**: In an action's `uses`, write the heated capability like `HeatSource<oven, heated: 200 degC>` (the temperature may also be an expression of the action's arguments). Equipment kinds have `preheatTime` in their props (a range, `@source(provisional)`). Before that task, the scheduler puts a `preheat` task (unattended, occupying the unit) on the same unit.
  - Align the end of the preheat with the start of the task (planned ahead). For a task that depends on an earlier task, the preheat runs in parallel with the earlier task. Rebuild with the same release iteration as `@serveWithin`. The current washing starts at the time the consuming task becomes ready and does not plan ahead (§3.6), so it does not take the same form.
  - A following task that uses the same unit at the same or a lower temperature does not get a new preheat.
  - If the kitchen unit's `maxTemp` is lower than the required temperature, `KIT002 requiresUnmet`. A unit without `maxTemp` counts as meeting it, the same as the current `req_met`.
  - Preheating does not go into the IR (like washing, it is inserted at the scheduling stage. The invariant of roadmap D31).
  - Reason: a recipe does not name a kitchen (concept §4.2). Inserting it at the scheduling stage keeps the IR the same when the kitchen or the servings change.
- Alternative: have the recipe write a `preheat(oven, 200 degC)` task. The implementation is small, but there is no link to which task the preheat is for, so it cannot check that the oven cools between preheating and use, or that a task at another temperature cuts in.

### D49. The time limit from making to using, `@useWithin` [decided]

- **Decision:** As recommended, keep the time from the end of the task that makes a value to the start of each task that takes the value as input within t, and if this cannot be kept, `SCH011 useWithinMissed` (warning). Attached to an ingredient itself, `TYP017`. (2026-10-30, confirmed by the user)
- **Recommendation**: Attaching `@useWithin(t)` to a `let` or an expression statement keeps the time from the end (`endHi`) of the task that makes the value to the start of **each** task that takes the value as input within t. Build it with the same release iteration as `@serveWithin`, and if it cannot be kept, `SCH011 useWithinMissed` (**warning**. The arguments are the value's name, the wait and the limit; the position is the annotation). When there is no task that makes the value (attached to an ingredient itself), `TYP017 annotationTarget`. Attached to a value that no task consumes, it gives neither a schedule constraint nor a diagnostic (LIN005 reports that it is unused). Add a `limits` field to the schedule JSON, and `schedcheck` checks it independently.
  - Reason: the freshness of intermediate values, such as tempura batter or cut apples, leads directly to failed dishes. The mechanism of `@serveWithin` (release, scoring, warning) can be used as is.
- Alternative A: look only at the first consuming task. The implementation is small, but for a value used in two rounds (frying batter in two rounds), the later round is not protected.
- Alternative B: make it an error when it cannot be kept. It is stricter, but a schedule constraint that may not be keepable depending on `--prefer` or the servings would get a different severity from the other serving-time constraints (warnings).

### D50. Rules for rewriting the tests [decided]

- **Decision:** As recommended, keep the file names and the sequence of actions, and bring dish names, binding names, ingredients and amounts closer to real cooking. Add actions only when it would not hold up as cooking. Rewrite expected values by hand, and recount the pairs for limits. (2026-10-30, confirmed by the user)
- **Recommendation**: Rewrite by the following rules.
  - Do not change the paths (names) of test files. This keeps the names in the coverage table, `bindings/inputs.txt`, the names in `goals.sh` and the red-first check as they are (§3.3).
  - Bring dish names, binding names, ingredients and amounts closer to real cooking. Do not use one-letter names. Omit `()` on declarations without arguments (D44).
  - **Keep the sequence of actions (the list of action names in `debug steps`)**. Actions may be added only for steps that do not hold up as cooking (ending with just cutting, for example). For a test with added actions, redo the mutation check as in D51.
  - Use pipes and `per serving` where they fit naturally. Tests that check `let` itself (rebinding, scope, macro hygiene and so on) stay with `let`.
  - Rewrite expected values by hand from §4 and the formats of roadmap §4 (including line and column of positions and the calculation of time and amounts). Build schedules by hand and confirm 0 violations with `schedcheck`.
  - Recount the tests for limits, fuel, byte counts and allocation (`limits/*`, `eval/fuel-*`, `limits-shared/*`, `diag/out-of-memory`, `diag/depth-exceeded`) so that the "exact" and "one over" pairs are kept. `G-RF-LIMIT-PAIRS` confirms that "exact" is really exact.
- Alternative: change the sequence of actions freely too. It gets closer to real cooking, but the mutation check would have to be redone for almost every test (about 260 names in the roadmap's coverage table).

### D51. How to check the rewrite [decided]

- **Decision:** As in alternative A, redo the mutation check for **every** row of roadmap §5.3 that points to a rewritten test (golden, CLI test, unit test), and record it in §9 of this plan with a `- Rewrite mutation check: ` line (about 242 rows by the count of 2026-10-28). The machine comparison of the recommendation (`check-rewrite-equivalence.py`) is also kept, as a cheap check before redoing. (2026-10-30, confirmed by the user)
- **Recommendation**: With a new check script `scripts/check-rewrite-equivalence.py`, compare the golden tests before the rewrite (tag `rewrite-start`) and after it (HEAD during Phase 5, tag `rewrite-end` from P5-9). Check out the commit of each point into a temporary worktree, build it, and run with the executable of that point (so it is not affected by LIN005 and the like from later phases). Compare the exit code, the sequence of diagnostic codes, the counts of the result lines (errors, warnings, infos), and the list of action names (the op names of `debug steps` or `debug ir`). Output the list of tests whose list of action names changed. Redo the "How to break it" of the roadmap §5.3 rows for those tests, and record them in §9 of this plan with a `- Rewrite mutation check: ` line (counted separately from the coverage table's `Mutation check` records).
  - Reason: if the diagnostics and the sequence of actions are the same, the property the test checks (which diagnostics appear, which tasks are made) is kept. Redoing the check can be narrowed to tests whose structure changed.
- Alternative A: redo the mutation check for every row of the roadmap's coverage table that points to a rewritten test. It is the most certain, but means hundreds of rounds of manual work.
- Alternative B: only the script comparison. It is fast, but it cannot tell whether a test with added actions still checks the original property.

### D52. Removing the prototype `proto/` [decided]

- **Decision:** As recommended, move the 5 check scripts in use, together with their self-tests, to `scripts/`, set the tag `proto-final`, and then remove `proto/` and `ios-proto.yml`. Drop `G-PROTO-*`, write the names of the prototype tests in `retired-tests.txt`, and keep the report. (2026-10-30, confirmed by the user)
- **Recommendation**: Remove it in the following order.
  1. Move the check scripts the core goals use (`check-flags.py`, `check-pure.sh`, `check-noglobal.py`, `check-diag-registry.sh`) and the README example check (`check-readme-example.sh`) to `scripts/` together with their self-tests. Fix how the root is found (`root=$(cd "$(dirname "$0")/../.." …)`) and the default paths of `check-diag-registry.sh` (`proto/src/diag_codes.def` and others). In the README example check, make `ozen` the default for blocks without `run:`.
  2. Fix the remaining scripts that refer to `proto/`: `goals.sh`, `selftests.txt`, the target folders of `check-selftests.sh`, the targets of `check-spdx.sh`, the resolution of `golden` and `sh` in `check-plan-tests.sh`, `test-check-plan-tests.sh` and `test-check-spdx.sh`. Keep the branch of `check-red-first.sh` that checks out historical commits (it is needed to judge past commits).
  3. Set the tag `proto-final` on the commit just before the removal.
  4. Remove `proto/` and `.github/workflows/ios-proto.yml`. Also remove the local prototype builds (`build/gcc-*`, `build/clang-*`) (because the `c/*` of `check-plan-tests.sh` would resolve against old builds).
  5. Drop `G-PROTO-*` from `goals.sh`. Replace `G-PROTO-README-EXAMPLE` with `G-RF-README-EXAMPLE`.
  6. Write the names of the prototype tests in roadmap §5.3 (`golden/*` 109 entries, `c/*` 4 entries, and the `sh/*` only in the prototype) in `scripts/retired-tests.txt` with a reason.
  7. Remove the "Evaluator prototype" section of the README and `proto/` in the license table. Also adjust the string `G-REPO-README-LICENSE` compares.
  8. Keep `docs/prototype-report.md`, and write at its top "The prototype was removed. It can be read at the tag `proto-final`".
  - Reason: the core refers to `proto/` neither in code nor in tests (§3.5). With the report and the tag, what was learned from the prototype can be traced.
- Alternative A: do not remove it. Nothing breaks, but the 574 unused files keep using goal run time and CI (`ios-proto`).
- Alternative B: as in roadmap D28, retire only the goals and keep the files. It follows the precedent, but does not match the request (remove it).

### D53. Updating the concept [decided]

- **Decision:** As recommended, add this plan's new mechanisms to §4.1, §4.2, §4.4, §4.5 and §6.1 of the concept, and remove the empty `()` from the examples. (2026-10-30, confirmed by the user)
- **Recommendation**: Add the language mechanisms this plan introduces to the matching sections of the concept: the syntax in §4.4 (pipe, omitting `()`, `per serving`), using up in §4.1 (unused values), resources in §4.2 (preheating), serving time in §4.5 (`@useWithin`), and diagnostics in §6.1 (`LIN005`, `TYP020`, `SCH011`). The added content is the same as §4 of this plan. Also remove the empty `()` from the examples.
  - Reason: the roadmap made "the concept is the source of truth for behavior". If mechanisms not in the source of truth pile up, the next plan has more work finding mismatches.
- Alternative: leave the concept unchanged and make §4 of this plan the source of truth for the new mechanisms. The concept text stays as it is, but readers have to compare two documents.

### D54. Whether to rewrite the samples in the new style [decided]

- **Decision:** As in the alternative, rewrite the samples (`samples/`) with pipes and `per serving` too. The rewrite is limited to replacing syntax, and a machine confirms that each sample's IR, schedule, renderer output and `expect.yaml` are the same as before the rewrite (S616–S620, P5-8). (2026-10-30, confirmed by the user)
- **Recommendation**: Do not rewrite them (out of scope). Only confirm that LIN005 does not appear in the samples (S567).
  - Reason: the request is to rewrite the tests. The samples are already real cooking, and rewriting them also changes `expect.yaml` and the output the renderer tests use.
- Alternative: rewrite the samples with pipes and `per serving` too. The examples seen from the README take the new style, but the work and the expected-value updates grow.

### D55. Version [decided]

- **Decision:** As in the alternative, keep the version at `0.1.0`. Do not change `VERSION`, `stdlib/package.ozen`, expected values that contain the version, or the `requires` golden tests. (2026-10-30, confirmed by the user)
- **Recommendation**: At the end of this plan, make the version `0.2.0`. By the 0.x compatibility decided in roadmap D30 (`"0.1"` means 0.1.0 or later and before 1.0.0), the existing `requires "0.1"` passes. The changes are as follows.
  - `VERSION`, and `version` in `stdlib/package.ozen`.
  - The `.out` files that write the version string directly (`"ozenVersion":"0.1.0"` and others, 13 files), and the output of the README's bundle example (`std: "0.1.0"`).
  - The golden tests that check "a newer version is required" (`requires "0.2"` in `packages/requires-newer-minor`, `requires "0.2.0"` in `bundle/std-version-check`) would be satisfied by 0.2.0, so change their input to `0.3` to keep the meaning.
  - Make `G-REPO-VERSION` compare the version line with this file, and put a "Version: 0.2.0" line in §9 of this file (D41).
  - Reason: there is new syntax, and a change in how dimensions are handled (D45).
- Alternative: keep `0.1.0`. None of the changes above are needed, and since the version is not published the actual harm is small, but the syntax change does not show in the version.

### D56. Order of the work [decided]

- **Decision:** As recommended, the order is prototype removal → bug fixes → notation → `per serving` → rewrite of the tests and samples → LIN005 → preheating → `@useWithin` → documents and finishing. (2026-10-30, confirmed by the user)
- **Recommendation**: prototype removal → bug fixes → notation (pipe, `()`) → `per serving` → test rewrite → LIN005 → preheating → `@useWithin` → documents and finishing.
  - Reason: the rewrite uses the new style, so it comes after the notation. LIN005 changes about 130 expected values of existing golden tests (§3.2). Moving to real cooking in the rewrite removes many unused values, so placing it after the rewrite means fewer expected-value updates. Prototype removal is independent and cuts goal run time, so it comes first.
- Alternative: put LIN005 before the rewrite. Unused values would be noticed during the rewrite, but about 130 expected values would be fixed twice.

---

## 3. Results of checking the current state (2026-10-28, `09e05de`)

### 3.1 State of the roadmap

- The final check of P16-4 finished on 2026-10-27 (roadmap §9.1). No tasks remain in §6.
- The last D is D39 and the last S is S478. The headings D33 and D34 each appear twice (roadmap lines 124 and 325, and 135 and 334).
- `G-CORE-GRAMMAR` measures 131 grammar rules. Roadmap §4.20.1 says 130.
- There are 363 golden entries (329 single files, 34 folders) and 436 `.ozen` files.
- `scripts/goals.sh --list` has 93 goals (including the 6 retired in roadmap D28 and 24 `G-PROTO-*`).

### 3.2 Syntax and evaluation (`core/src/`)

- Lexing: the symbol table `puncts[]` (`lex.c:237-244`) is matched from the top. Longest match is achieved by putting the 2- and 3-character symbols at the top of the table. `|>` goes before `{"|", OZNI_T_PIPE}`. Adding a token kind requires fixing the switches in `primary` (470-635) and `sum_start` (724-768) of `parse.c`, because of `-Wswitch-enum -Werror`.
- Expressions: `expr` (`parse.c:847-864`) also serves as link, and descends `or_expr` → … → `postfix` (638-662) → `primary`. A call is `OZNI_N_Call`, with children "the callee, then the arguments". A positional argument after a named argument gives `SYN017` (`parse.c:307-309`).
- Places that treat a call as "the first child is the callee": `eval_inner` (from `sema.c:4734`; the call branch is 4819-4866), `call_action` (from 3963), `eval_make` (from 6002), `action_call` (4951-4960), `resolve_macro_call` (`resolve.c:1157-1194`), `levels.c:133-137`.
- The unit test `span_nesting` (`core/tests/unit/test_parse.c:26-74`) checks, for every node of the golden tests, that a child's span lies inside its parent's span.
- Grammar coverage: `scripts/check-grammar-coverage.py` matches the rule names of `core/grammar.ebnf` against `debug ast --rules` of every golden `.ozen`. The rule names come from `core/src/rules.def`. When adding a rule, change the grammar, `rules.def` and `mark` together, and add a golden test that uses it.
- `()` on `recipe` and `menu`: it can be omitted via `paren_params(p, n, false)` (`parse.c:1684`). Authored sources have 0 declarations that omit it. Empty `()` occur on 454 lines in `tests` (of which 20 files are IR displays in `.out`), 11 in `renderers/test/inputs`, 6 in the README, 2 in `core/tests`, 2 in `docs/concept.md`, and 1 in `bindings/inputs`. 0 in `samples/`.
- `make 名前`: the child is a Name, not a Call, and `eval_make` returns `unknown()` with no diagnostic (`sema.c:6005-6010`).
- Linearity: `consume()` (`sema.c:1743-1761`). The last value is `s->last` and `s->last_node` (`sema.c:132-133`). Leftover parts are `LIN003` in `finish_dish` (5780-5806). `store`, `discard` and `plate` are ordinary std actions, and calling them consumes their arguments (`stdlib/prelude.ozen:720, 732-737`). There is no table collecting all the food of a dish.
- Golden tests in which a name of `let x =` is not used later: 131 of 436 by a rough estimate. 282 `.out` files contain `warnings=0`.
- Amounts: `ozni_qty` (`sema.h:13-17`) is an interval of rationals plus a vector of dimensions. `*` and `/` simply add and subtract dimensions (`sema.c:4341-4348`).
- Argument values: `eval_param` (`sema.c:5894-5938`) uses `typed_qty` (2550-2561) when there is a type, and if the type is `Count` and the value is dimensionless, it attaches the Count dimension. Without a type, it stays dimensionless.
- Ingredient amounts: `eval_ingredients` (`sema.c:4870-4932`) does not check whether the amount's dimension fits the food.
- Servings: ingredient amounts do not change with the servings. `s->servings` is used only for plating capacity, the number of vessels and `ASM005`. With `make r(servings: 4)`, `s->servings` inside the dish is not set (`sema.c:6084, 5898-5900`).
- Diagnostic codes: `TYP018 optionLevel`, `TYP019 outputMismatch` and `SCH010 serveWithinMissed` are taken. `LIN005`, `TYP020` and `SCH011` are free (`core/src/diag_codes.def`).

### 3.3 Test infrastructure

- golden: `scripts/golden-entries.sh` collects the entries, and `scripts/run-golden.sh` compares standard output and `exit <n>` with `.out`. One golden test has one `// args:` line and runs exactly one command.
- References to test names: the coverage table of roadmap §5.3 (resolved by `scripts/check-plan-tests.sh`), `bindings/inputs.txt` (11 golden tests), `scripts/goals.sh:688` (`bundle/order-by-name`), `tests/props/bundle_roundtrip.py` (`samples` and `tests/golden/bundle/*`). If the paths do not change, none of these break.
- red-first (`scripts/check-red-first.sh`) counts only commits that **add** expected-value files. Commits that only change existing `.out` files are not covered. Changing a path counts as an addition and gives "not red".
- `scripts/check-plan-tests.sh --mutations` requires the number of `- Mutation check: ` lines in §9 to be **equal** to the number of rows in the coverage table that have a "How to break it".
- Goals that depend on the contents of test inputs: `G-CORE-GRAMMAR`, `G-CORE-DIAG-REGISTRY` (each of the 103 codes appears in some `.out`), `G-CORE-UNIT` (`span_nesting`), `G-CORE-OOM`, `G-CORE-PROPS` (`reindent`, the 4 IR ones, `monotonic`, `bundle_roundtrip`), `G-SCHED-CHECK`, and the digests of `G-CORE-DETERMINISM` and `G-PLAT-*`.
- Size of the rewrite: `tests/golden` has 30 folders and 816 files. 290 `.ozen` files have declarations, and 180 of them have one-letter names. One-letter `let` names are also common (`s` 101, `c` 64, `w` 46 and so on). 19 in heredocs of `tests/cli`, 2 in C strings of `core/tests/unit`, and 1 in `bindings/inputs/utf8.ozen`.
- The sample check `tests/samples/check.py:119-131` compares the number of `warnings` only when it is written in `expect.yaml`. None of the 11 samples has `warnings:` written.

### 3.4 Bugs and holes found while looking into it

| ID | Behavior | Reproduction | This plan |
| --- | --- | --- | --- |
| B1 | In a dish with `servings: Count = 4`, `150 g * servings` becomes `600 g*pcs` (`300 g*pcs` with `-s 2`), and passes `wash`, `cookRice` and plating with no diagnostic | Confirmed on 2026-10-28 with `ozen debug steps -s 2` | D45, S492–S494 |
| B2 | In the same dish, `if servings > 4` gives `TYP001 dimension` (`Count` vs `Dimensionless`) | Confirmed on 2026-10-28 with `ozen check` | D45, S495 |
| B3 | In a menu, `let b = make teishoku` (without parentheses) makes no steps and gives no diagnostic | Confirmed on 2026-10-28 with `ozen debug steps --entry dinner` | D44, S500 |
| B4 | With `--serve-at`, `@serveWithin` has no effect. Adding `-t "60 min"` to `serve/serve-within` finishes at 1680 s, serves at 3600 s, and gives `SCH010` | Confirmed on 2026-10-28 by an investigating agent's run. The release is mapped to the reversed clock while still a forward time (`sched.c:800-804`) | S502, S503 |
| B5 | The dimension of ingredient amounts is not checked (`rice: 3 min` passes too) | Confirmed by reading `sema.c:4910-4915` | S497–S499 |
| B6 | The servings passed with `make r(servings: 4)` are not used in the checks of vessels and plating | Confirmed by reading `sema.c:6084, 5898-5900` | S501 |
| B7 | `:` and `has` in requirements on the kitchen, and items missing from the kitchen, are not checked and count as met (`req_met`, `sema.c:3163-3171`). The spelling of `modes` of `HeatSource` is not checked either | Confirmed by reading | Only `heated` is handled, in D48; the rest is out of scope (§9) |
| B8 | No diagnostic for unused values or ingredients that are only declared | Confirmed on 2026-10-28 | D47 |

### 3.5 The prototype `proto/`

- 574 files (`tests` 527, `scripts` 24, `src` 12 and so on). `core/` and `cli/` refer to it neither in code nor in tests.
- Places that refer to it:
  - `scripts/goals.sh`: the 24 `G-PROTO-*`, and the checks in `proto/scripts/` used by 4 core goals (`G-CORE-FLAGS` line 417, `G-CORE-PURE` 453, `G-CORE-NOGLOBAL` 458, `G-CORE-DIAG-REGISTRY` 480).
  - 10 lines of `scripts/selftests.txt`, the target folders of `scripts/check-selftests.sh:26`, and `scripts/check-spdx.sh:36`.
  - `scripts/check-plan-tests.sh:121, 138` (resolution of `golden` and `sh`), `scripts/test-check-plan-tests.sh:67, 92`, `scripts/test-check-spdx.sh:40, 71, 74`.
  - `scripts/check-red-first.sh:34, 48, 63-65` (the branch that checks out historical commits. Keeping it does no harm).
  - Inside the scripts to move: how `check-readme-example.sh` finds the root and its default executable, and the default paths of `check-diag-registry.sh:7-9`.
  - `.github/workflows/ios-proto.yml`, README lines 42-91 and the license table at line 292, and `docs/prototype-report.md`.
- The usage of `check-readme-example.sh` is `[--readme FILE] [--bin OZENPROTO] [--ozen OZEN]`, and its output is `readme: N examples, M mismatches`.
- Names of the prototype tests in roadmap §5.3: `golden/*` 109 entries, `c/*` 4 entries (resolved by `ctest --test-dir build/gcc-debug-san`; an old build is left locally), and 7 `sh/*` entries only in the prototype.
- The retirement precedent (roadmap D28) did not remove files. There is no precedent for removal.

### 3.6 Scheduling (`core/src/sched.c`, `sema.c`)

- Inserting washing: `fit` (`sched.c:102-174`) estimates for each unit whether washing is needed, and `run()` (from 292; the washing call is at 430-445) calls `clean_unit` (184-262). Washing starts at the time the consuming task becomes ready, and does not plan ahead. The kinds of inserted tasks are `OZNI_EXTRA_*` (`sched.h:14`).
- `@serveWithin`: `timing_of` (`sema.c:5335-5373`) reads the annotation, and the iteration of `schedule_dish` (8259-8354, at most 8 times) attaches a release to the finishing task and rebuilds. `score_schedule` (7644-7658) marks schedules that cannot keep it as `feasible=false`. `SCH010` is given at 8445-8461.
- A precedent usable for preheating: `hold_until`, which occupies a unit for keeping warm (`sched.c:465-467`, golden `serve/keep-warm-occupies`). There are 6 places that do not tell inserted tasks apart by kind (`sched.c:569-583` and 833-843, `sema.c:7460`, 7671, 7278, and the arrays of kind names at 7075/7428/7764).
- The check of requirements on the kitchen, `req_met` (`sema.c:3132-3178`), looks only at `>=` and `<=`. The capability match `kind_has` (2652-2703) ignores state arguments. The std `Oven` and `OvenRange` (`stdlib/prelude.ozen:143, 158`) have `HeatSource<oven>`, and `OvenSpec { maxTemp: Temperature? }` (756-758).
- The `kind` of tasks in the schedule JSON is `op`, `plate`, `wash`, `rinse`, `soak`, `wipe` or `run` (`schemas/result.schema.json:1281-1290`). `tools/schedcheck/schedcheck.py` is an independent checker that reads only the JSON (its self-test is `scripts/test-schedcheck.sh`).
- std has no oven actions. The golden tests define `bake` on the spot each time (`sched/calibration.ozen:2-4` and others).

---

## 4. Behavior specification

Each checkable claim is one item with one claim, starting with `**S<n>**`. The format of example outputs follows roadmap §4.20.1, §4.21.1 and §4.22.1.

### 4.1 Removing the prototype (D52)

- **S479** The repository has no `proto/`.
- **S480** The repository has no `.github/workflows/ios-proto.yml`.
- **S481** The tag `proto-final` points to the parent of the commit that removed `proto/`.
- **S482** The functions of `G-CORE-FLAGS`, `G-CORE-PURE`, `G-CORE-NOGLOBAL` and `G-CORE-DIAG-REGISTRY` in `goals.sh` call the check scripts in `scripts/`.
- **S483** After `proto/` is removed, `G-CORE-FLAGS`, `G-CORE-PURE`, `G-CORE-NOGLOBAL` and `G-CORE-DIAG-REGISTRY` still pass.
- **S484** Each of the 5 moved check scripts has a self-test in `scripts/`, listed in `scripts/selftests.txt`.
- **S485** The README has no "Evaluator prototype" section.
- **S486** No row of the README's license table has `proto/`.
- **S487** The names of the prototype tests in roadmap §5.3 are listed in `scripts/retired-tests.txt` with a reason, and `G-PLAN-TESTS-EXIST` passes with no local prototype build.
- **S488** The first 10 lines of `docs/prototype-report.md` say that the prototype can be read at the tag `proto-final`.
- **S489** `scripts/check-readme-example.sh` runs blocks without `run:` with the executable given by `--ozen`.
- **S490** No file in `scripts/` or `.github/` uses a `proto/` path (except the history branch of `check-red-first.sh`).
- **S491** `scripts/goals.sh --list` has no ID starting with `G-PROTO-`.

### 4.2 Bug fixes (D44, D45)

- **S492** The value of a `Count`-typed argument is a dimensionless number. In `recipe gohan(servings: Count = 4) { ingredients { rice: 150 g * servings } … }`, `rice` is `600 g`.
- **S493** A value overridden by `servings` of the build-time conditions (`-s 2`) is also a dimensionless number. Building the dish of S492 with `-s 2` gives `rice` of `300 g`.
- **S494** The values of `Count`-typed kitchen fields (such as `burners`) are also dimensionless numbers.
- **S495** A `Count`-typed value can be compared with a number. In a dish with `servings: Count = 4`, `if servings > 4 { … }` does not give `TYP001`.
- **S496** A value written with `pcs` for a `Count`-typed argument or field (`servings: 2 pcs`) is also a dimensionless number.
- **S497** If the dimension of an ingredient amount is none of mass, volume or count, `TYP001 dimension`. `rice: 3 min` is an error.
- **S498** The arguments of the diagnostic of S497 are the dimension found and `Mass`; the position is the amount expression.
- **S499** Ingredient amounts of mass, volume or count (`rice: 300 g`, `milk: 200 ml`, `onion: 2 pcs`) pass.
- **S500** `make 名前` (without parentheses) gives the same steps and the same output value as `make 名前()`.
- **S501** A dish called with `make r(servings: 4)` in a menu uses vessels for 4 people in the schedule's plating (inside `make` no diagnostic on the number of vessels is given, so it is checked by the number of vessels. §9).
- **S502** Even when `--serve-at` (`serveAt`) is given, `@serveWithin(t)` is kept if an arrangement that keeps it exists. With the input of `serve/serve-within` plus `serveAt: 60 min`, the wait from finishing the soup to serving is 300 s or less.
- **S503** With the input of S502, `SCH010` does not appear.

### 4.3 Pipe (D42, D43)

- **S504** `a |> f` gives the same IR as `f(a)`.
- **S505** `a |> f(x, k: v)` gives the same IR as `f(a, x, k: v)`.
- **S506** Pipes chain from left to right. `a |> f |> g(x)` gives the same IR as `g(f(a), x)`.
- **S507** The pipe's precedence is the lowest within an expression. `x + y |> f` gives the same IR as `f(x + y)`.
- **S508** If the right side is a number or an amount, `SYN003 unexpectedToken` (the position is the first token of the right side).
- **S509** If the right side is a list or a parenthesized expression, `SYN003 unexpectedToken` (the position is the first token of the right side).
- **S510** If the right side is `make`, `SYN003 unexpectedToken` (the position is `make`).
- **S511** The right side may be a macro call, and the left value becomes the macro's first argument.
- **S512** A call with only named arguments, `a |> f(k: v)`, does not give `SYN017 argumentOrder` and gives the same IR as `f(a, k: v)`.
- **S513** `@unattended` attached to a pipe expression statement attaches to the call of the last stage. If the last stage is an action, it passes.
- **S514** If the last stage is not an action or macro call (such as the built-in `ceil`), `@unattended` gives `TYP017 annotationTarget`.
- **S515** Before and after adding the pipe (tags `refresh-start` and `rewrite-start`), the expected values of `tests/golden/typeargs/*` and `tests/golden/decl/alias.out` do not change (the reading of `|` in types does not change).
- **S516** `ozen debug ast --rules` outputs the rule `pipe` for input that uses a pipe.
- **S517** `core/grammar.ebnf` has the rule `pipe`.
- **S518** The span of a call rewritten from a pipe runs from the start of the left side to the end of the right side.
- **S519** Even for input that uses pipes, every node's span lies inside its parent's span (`span_nesting`).
- **S520** `a |> f` counts the same number of nodes as `f(a)` (`--max-nodes`).
- **S521** The fuzz dictionary `core/fuzz/ozen.dict` has `"|>"`.

### 4.4 Omitting `()` (D44)

- **S522** A `recipe` or `menu` without arguments gives the same IR whether or not `()` is written.
- **S523** Authored sources (the `.ozen` files of `tests/` and `tests/cli/*.sh`, `core/tests/unit/*.c`, `bindings/inputs/`, `renderers/test/inputs/`, the `ozen` blocks of the README) have no `recipe` or `menu` declaration or `make` with an empty `()`. The only exceptions are those written with a reason in `scripts/empty-parens-exceptions.txt`.
- **S524** The concept's Ozen examples have no `recipe` or `menu` declaration or `make` with an empty `()`.

### 4.5 Amounts per person (D46)

- **S525** The amount of the ingredient `rice: 75 g per serving` is `75 g × n` for n servings.
- **S526** The amount of an ingredient without `per serving` does not change with the servings (the same amount with `-s 2` and `-s 4`).
- **S527** The servings come from the dish's argument `servings`.
- **S528** If the dish has no argument `servings`, they come from `servings` of the build-time conditions.
- **S529** When the servings cannot be determined (no `servings` in either the arguments or the conditions), `TYP020 servingsUnknown`.
- **S530** The severity of `TYP020` is error, and the position is the amount expression with `per serving`.
- **S531** `per serving` can be written only inside an ingredient declaration. Anywhere else (such as an argument's default value), `SYN003`.
- **S532** The `per serving` amounts of a dish called with `make r(servings: 2)` in a menu are for 2 people.
- **S533** Ingredient amounts in the IR and the JSON are totals multiplied by the servings.
- **S534** The dimension of the unit of an amount multiplied by the servings is the same as the original amount.
- **S535** `TYP020` is at the end of the registry, `TYP:servingsUnknown` is in `core/tests/diag-names.txt`, and `diag-servings-unknown` is in `phrases/ja.ftl` and `en.ftl`.
- **S536** The fuzz dictionary has `"per"` and `"serving"`.

### 4.6 Test rewrite (D50, D51)

- **S537** Between the tags `rewrite-start` and `rewrite-end`, no files in `tests/golden` are added, removed or renamed.
- **S538** The rewritten golden tests, compared at `rewrite-start` and at `rewrite-end` (each with the executable of that point), have the same exit code, sequence of diagnostic codes and counts of result lines.
- **S539** Every row of roadmap §5.3 that points to a rewritten test (golden, CLI test, unit test) and has a "How to break it" has a `- Rewrite mutation check: ` record in §9 of this plan (D51).
- **S540** Recipe and menu declarations and `let` bindings in `tests/`, `core/tests/` and `bindings/inputs/` have no one-letter names.
- **S541** The only exceptions for one-letter names are those written with a reason in `scripts/test-names-exceptions.txt`.
- **S542** Commits that change golden expected values do not change the implementation (`core/src/`, `core/include/`, `cli/src/`, `stdlib/`, `phrases/`, `schemas/`, `tools/schedcheck/`, `renderers/lib/`).
- **S543** The "exact" inputs of the limit golden tests (`limits/*-exact`, `limits-shared/*-exact`, `eval/fuel-exact`) give the limit diagnostic when run with the limit lowered by 1.
- **S544** After the rewrite, golden tests still use every grammar rule (`G-CORE-GRAMMAR`).
- **S545** After the rewrite, every diagnostic code in the registry still appears in some golden test (`G-CORE-DIAG-REGISTRY`).
- **S546** After the rewrite, all golden tests still pass in the 4 configurations (`G-CORE-GOLDEN`).
- **S547** After the rewrite, the unit tests (including `span_nesting`) still pass (`G-CORE-UNIT`).
- **S548** After the rewrite, the allocation failure injection still passes (`G-CORE-OOM`).
- **S549** After the rewrite, the property tests still pass (`G-CORE-PROPS`).
- **S550** After the rewrite, every golden test that outputs a schedule still passes `schedcheck` (`G-SCHED-CHECK`).
- **S551** After the rewrite, the binding tests still pass (`G-PLAT-LINUX`, `G-PLAT-WASM`).
- **S552** The number of golden entries is the starting point's N0 plus the number this plan adds.

### 4.6.1 Rewriting the samples (D54)

- The rewrite is limited to replacing syntax. Chains of `let` for actions applied in a row become pipes, and ingredients that scale with the servings become `per serving` (with values that give the same amount as the original total at the default value of the dish's argument `servings`). Bindings that `phrases` refers to by name (the local value names added in the roadmap's `0ded0a3`) stay as `let`.
- **S616** The Ozen source of each sample (`samples/*`) has at least one pipe `|>`. The only exceptions are those written with a reason in `scripts/samples-pipe-exceptions.txt`.
- **S617** The IR (`--emit ir`) of each sample run with its `args` is the same as at `rewrite-start`.
- **S618** The schedule (`--emit schedule`) of each sample run with its `args` is the same as at `rewrite-start`.
- **S619** During the rewrite, `samples/*/expect.yaml` and the expected values of `renderers/test/` do not change.
- **S620** The outputs (text, HTML) of all samples from `renderers/tools/render-all.mjs` are the same as at `rewrite-start`.

### 4.7 Unused values (D47)

- **S553** At the end of a dish, a bound food value that is not consumed, not the last value and not served gives `LIN005 unusedValue`.
- **S554** The severity of `LIN005` is warning.
- **S555** The position of `LIN005` for a bound value is the bound name; the arguments are the name and the type.
- **S556** If the value of an unbound expression statement is not used, `LIN005`. The position is the expression; the arguments are the expression's source text and the type.
- **S557** An ingredient that is only declared and not used gives `LIN005` (the position is the ingredient's name).
- **S558** Not reported for values passed to `store(x)` or `discard(x)`.
- **S559** Not reported for parts (outputs of split, butchering or tuples) (only `LIN003`).
- **S560** Not reported for the last value. When the last value is a tuple, each of its values is a part, so S559 excludes it (checked by the judgment shared with `LIN003`).
- **S561** Not reported inside the evaluation of a dish called by `make`.
- **S562** Reported when the dish of S561 is evaluated on its own.
- **S563** Not reported for values inside the `steps` of a composed action.
- **S564** Not reported for food from which portions are taken out by division (in the middle of "taking out n times uses it up" of roadmap §4.21.1).
- **S565** Can be hidden with `@allow(unusedValue)`.
- **S566** `LIN005` is at the end of the registry, `LIN:unusedValue` is in `core/tests/diag-names.txt`, and `diag-unused-value` is in `phrases/ja.ftl` and `en.ftl`.
- **S567** `LIN005` does not appear when evaluating the samples (`samples/*`).
- **S621** Values made in the body of `repeat` or `for` are not tracked (the value of each round is part of the dish, and there is no way to write collecting them).
- **S622** When a dish ends with `parallel`, the values inside the branches do not become the last value, and are reported if unused.
- **S623** When a call is erroneous and not evaluated, its arguments are not consumed and are reported as unused values.
- **S624** A binding inside a macro expansion is reported at the position of the macro definition, with the expanded call attached as `via`.
- **S625** When the same food value is bound to two names, only the first name is reported.
- **S626** Menu values (results of `make` and `buy`) are covered too.
- **S627** Values bound in the body of `if` are covered too.
- **S628** Not reported for values that are not food (amounts, times, numbers).

### 4.8 Preheating (D48)

- **S568** A `preheat` task is inserted before a task that has `HeatSource<oven, heated: t>` in `uses`.
- **S569** The preheat goes on the same unit that the task uses.
- **S570** The preheat ends at or before the start of the task.
- **S571** For a task that needs a temperature and depends on an earlier task, the preheat runs in parallel with the earlier task and ends at the start time of the task that needs the temperature.
- **S572** The duration of the preheat is `preheatTime` in the props of the equipment kind.
- **S573** Preheating uses no person (during preheating, people do other tasks).
- **S574** From the start of the preheat to the end of the task that needs that temperature, no other task uses that unit.
- **S575** A following task that uses the same unit at the same or a lower temperature does not get a new preheat.
- **S576** A following task that uses it at a higher temperature gets a new preheat before it.
- **S577** The required temperature may be an expression of the action's arguments (`heated: temp`).
- **S578** Even with `--serve-at` (backward), S570 and S571 are kept.
- **S579** If the kitchen has no unit that meets the required temperature (`maxTemp` at or above the required temperature, or no `maxTemp`), `KIT002 requiresUnmet`.
- **S580** If `maxTemp` equals the required temperature, it passes.
- **S581** A unit with no `maxTemp` written counts as meeting the required temperature.
- **S582** Preheating does not go into the IR (the `debug ir` with the kitchen injected has no preheat task).
- **S583** The schedule text has a line `<始まり>..<終わりの上限> (<終わりの下限>) preheat <個体> to <温度> unattended [<個体>]`.
- **S584** In the schedule JSON, a preheat task has `kind: "preheat"` and `heats: {resource, temp}`.
- **S585** In the schedule JSON, a task that needs a temperature has `needs: [{resource, temp}]`.
- **S586** `schemas/result.schema.json` allows the forms of S584, S585 and S600.
- **S587** `schedcheck` checks, from the JSON alone, that before a task that needs a temperature there is a preheat of sufficient temperature on the same unit, and that no other task uses the unit in between.
- **S588** The renderer writes preheat tasks with sentences from `phrases` (preheat keys in `ja.ftl` and `en.ftl`).
- **S589** Preheat rows appear in the renderer's schedule table (HTML).
- **S590** Every std equipment kind that has `HeatSource<oven>` has `preheatTime`.
- **S591** `preheatTime` is a provisional range, with its source (`@source(provisional)`) and a record of the research.
- **S629** When a unit of a kind without `preheatTime` needs a temperature, a preheat of length 0 is inserted at the start of the task (to keep `needs` and `heats` matched).
- **S630** A unit whose `maxTemp` is lower than the required temperature is dropped from the candidates for that task.
- **S631** After a task that does not need a temperature uses a unit, a new preheat is inserted before the next task that needs a temperature. Time passing alone is treated as not cooling it.
- **S632** If the time a task that needs a temperature becomes ready is earlier than the upper bound of the preheat time, the preheat starts at 0 seconds (or at the time the unit becomes free), and the task waits until the preheat ends.

### 4.9 The time limit from making to using (D49)

- **S592** In a dish with `let v = e @useWithin(t)`, if an arrangement that keeps it exists, the time from the end (upper bound) of the task that makes v to the start of the task that takes v as input is kept within t.
- **S593** S592 also applies to the second and later tasks that take v as input.
- **S594** When it cannot be kept, `SCH011 useWithinMissed`.
- **S595** The severity of `SCH011` is warning; the arguments are the value's name, the wait and the limit, and the position is the annotation.
- **S596** Even with `--serve-at` (backward), S592 is kept.
- **S597** `@useWithin` with no task that makes the value (attached to an ingredient itself) gives `TYP017 annotationTarget`.
- **S598** `@useWithin` attached to an unbound expression statement also applies to the tasks that take that value as input.
- **S599** `@useWithin` attached to a value that no task takes as input produces neither a schedule constraint nor a diagnostic.
- **S600** The schedule JSON has `limits: [{producer, consumer, within, missed}]`.
- **S601** `schedcheck` checks `limits` from the JSON alone.
- **S602** `@useWithin` does not change the IR.
- **S603** `SCH011` is at the end of the registry, `SCH:useWithinMissed` is in `core/tests/diag-names.txt`, and `diag-use-within-missed` is in `phrases/ja.ftl` and `en.ftl`.
- **S633** If the argument of `@useWithin` is not a time, it gives `TYP001 dimension` and no limit is added.
- **S634** A schedule JSON with no limits does not output the `limits` field.
- **S635** A limit attached inside a dish called by `make` also applies to the menu's schedule.
- **S636** When a limit cannot be kept by any attempt, the schedule that does not delay the making task (the attempt that ignores limits) is taken.

### 4.10 Documents, settings and version (D41, D53, D55)

- **S604** §4.4 of the concept describes the pipe, omitting `()`, and `per serving`.
- **S605** §4.1 of the concept describes unused values.
- **S606** §4.2 of the concept describes preheating.
- **S607** §4.5 of the concept describes `@useWithin`.
- **S608** §6.1 of the concept describes `unusedValue`, `servingsUnknown` and `useWithinMissed`.
- **S609** The README has an example that uses a pipe, and the README example check passes.
- **S610** The version stays at `0.1.0`, and `ozen --version`, `version` in `stdlib/package.ozen` and the roadmap's "Version: 0.1.0" line keep matching (`G-REPO-VERSION`, D55).
- **S613** The Plan section of `CLAUDE.md` names this file.
- **S614** Source that uses pipes and `per serving` gives the same IR after going through `ozen bundle` and `ozen unbundle` (`prop/bundle_roundtrip`).
- **S615** The tags `refresh-start`, `proto-final`, `rewrite-start` and `rewrite-end` exist.

### 4.11 List of what is added or changed

| Kind | ID / path |
| --- | --- |
| Lexing and grammar | The token `\|>`, the grammar rule `pipe` (`core/grammar.ebnf`, `core/src/rules.def`), `per serving` in ingredient items, `"\|>"`, `"per"` and `"serving"` in the fuzz dictionary `core/fuzz/ozen.dict` |
| Diagnostics | `LIN005 unusedValue` (warning), `TYP020 servingsUnknown` (error), `SCH011 useWithinMissed` (warning). Appended to the end of the registry, and added to `diag-names.txt`, `phrases/*.ftl` and `tests/golden/api/local-not-listed.out` |
| std | The props `preheatTime` (a provisional range, `@source(provisional)`) of the equipment kinds that have `HeatSource<oven>` (`Oven`, `OvenRange`). The list of definitions in `api/local-not-listed.out` also changes |
| Schedule JSON | Task `kind: "preheat"`, `heats`, `needs`, and the top-level `limits` |
| Scripts | New: `scripts/check-rewrite-equivalence.py` (including `--samples`), `scripts/check-test-names.py` (including `--empty-parens` and `--samples-pipe`), `scripts/check-limit-pairs.py`, `scripts/check-std-preheat.py`. The exceptions list `scripts/samples-pipe-exceptions.txt`. Moved: `check-flags.py`, `check-pure.sh`, `check-noglobal.py`, `check-diag-registry.sh`, `check-readme-example.sh`. The self-test of each. The exceptions lists `scripts/test-names-exceptions.txt` and `scripts/empty-parens-exceptions.txt` |
| Tags | `refresh-start`, `proto-final`, `rewrite-start`, `rewrite-end` |
| Removed | `proto/`, `.github/workflows/ios-proto.yml`, the prototype section of the README, `G-PROTO-*` in `scripts/goals.sh` |

### 4.12 Where the code goes

| Place | What goes there |
| --- | --- |
| `core/src/lex.c`, `lex.h` | The `\|>` token (its name is decided in the implementation) |
| `core/src/parse.c` | Parsing `pipe` and rewriting it into a call, parsing `per serving` |
| `core/src/sema.c` | Making Count dimensionless (`typed_qty`), the dimension check of ingredients (`eval_ingredients`), `make` without parentheses (`eval_make`) and servings (`eval_param`), evaluating `per serving`, tracking unused values (`finish_dish`), passing preheating and `@useWithin` to scheduling |
| `core/src/sched.c`, `sched.h` | The preheat task (`OZNI_EXTRA_PREHEAT`), how the release is mapped backward |
| `schemas/result.schema.json`, `tools/schedcheck/` | The forms and checks of preheating and `limits` |
| `renderers/lib/`, `phrases/` | Preheat sentences, diagnostic sentences |

---

## 5. Definitions of machine-checked goals

**Prerequisite goals** are the following goals of roadmap §5 (they are implicitly part of each task's completion condition).

- Run locally: `G-API-EXPORTS`, `G-API-HEADER`, `G-API-SARIF`, `G-API-SCHEMA`, `G-BUNDLE-STABLE`, `G-CLI-TESTS`, `G-CORE-ARENA-ACCESS`, `G-CORE-BUILD`, `G-CORE-DETERMINISM`, `G-CORE-DIAG-APPEND`, `G-CORE-DIAG-REGISTRY`, `G-CORE-FLAGS`, `G-CORE-FUZZ`, `G-CORE-GOLDEN`, `G-CORE-GRAMMAR`, `G-CORE-NOFLOAT`, `G-CORE-NOGLOBAL`, `G-CORE-NOINT128`, `G-CORE-OOM`, `G-CORE-PROPS`, `G-CORE-PURE`, `G-CORE-STD-EMBED`, `G-CORE-UNIT`, `G-PLAN-COVERAGE`, `G-PLAN-TESTS-EXIST`, `G-PLAT-LINUX`, `G-PLAT-WASM`, `G-REND-BROWSER-SAFE`, `G-REND-HTML`, `G-REND-PHRASES-RENDER`, `G-REND-SELFCONTAINED`, `G-REND-TESTS`, `G-REND-VENDOR`, `G-REPO-CLAUDEMD`, `G-REPO-LICENSE-TEXT`, `G-REPO-NOTICE`, `G-REPO-README-LICENSE`, `G-REPO-RED-FIRST`, `G-REPO-SELFTESTS`, `G-REPO-SPDX`, `G-REPO-TOOLS`, `G-REPO-VERSION`, `G-SAMPLE-CHECK`, `G-SAMPLE-STD-ONLY`, `G-SCHED-CHECK`, `G-STD-LISTS`, `G-STD-PHRASES`, `G-STD-PROVENANCE`, `G-STD-SOURCES`, `G-STD-STATIC`, `G-TYPE-SEVERITY-TABLE`, `G-VOCAB-FCT`, `G-VOCAB-FCT-STABLE`, `G-VOCAB-LISTS`, `G-VOCAB-LISTS-COPY`, `G-VOCAB-PRIORWORK`, `G-VOCAB-SOURCES`
- Decided by CI after a push: `G-CORE-C23-SUBSET` (the Xcode row), `G-PLAT-IOS`, `G-PLAT-ANDROID`, `G-PLAT-CI`, `G-REPO-TOOLS-CI`
- Before every push: `G-REPO-NOREPLY`
- Not run: `G-VOCAB-SCHEMA`, `G-VOCAB-CHECK`, `G-VOCAB-SAMPLE`, `G-VOCAB-TESTS`, `G-VOCAB-READONLY`, `G-VOCAB-EXPECT-INDEPENDENT`, retired by roadmap D28. Before P1-2, `G-PROTO-*` are also prerequisite goals; P1-2 removes them.

On failure, report which part (which goal, which build configuration, which test, which environment) failed.

- The diff range of this plan is `RF=refresh-start..HEAD`.
- `PLAN_RF=plans/ozen-syntax-refresh.md`.
- A count that is expected to be 0 is judged by the number in the output, not by the exit code.
- In P0-3, the code block below becomes the functions `goal_G_RF_*` in `scripts/goals.sh`. For goals that use a new script, the task that creates that script fills in the body.

### 5.1 Structure and data

| ID | What it checks | Pass condition |
| --- | --- | --- |
| `G-RF-DECISIONS` | Whether D40 to D56 are decided | 17 decided, 0 open, 17 decision lines |
| `G-RF-COVERAGE` | Every S item has a row in the coverage table, and every item in §8 has a reason | Empty diff, 0 items without a reason |
| `G-RF-PLAN-TESTS` | The tests in this plan's coverage table exist, and there are as many mutation check records as rows | `plan-tests: <n> names, 0 missing` and `mutations: <m> rows, <m> recorded` |
| `G-RF-TAGS` | S615 | All 4 tags exist |
| `G-RF-PROTO-GONE` | S479 to S482, S488, S490 | 0 tracked files, the tag is the parent of the deletion, 0 references to `proto/` in the 4 functions of `goals.sh`, 1 or more at the top of the report, 0 references in `scripts` and `.github` |
| `G-RF-NO-PROTO-GOALS` | S491 | 0 |
| `G-RF-README-NO-PROTO` | S485 | 0 |
| `G-RF-README-EXAMPLE` | S489 and the second half of S609 | `readme: <n> examples, 0 mismatches`, n ≥ 1 |
| `G-RF-README-PIPE` | The first half of S609 | 1 or more |
| `G-RF-NO-EMPTY-PARENS` | S523 | `empty-parens: <n> sources, 0 found, 0 unlisted exceptions` |
| `G-RF-CONCEPT-PARENS` | S524 | 0 |
| `G-RF-FUZZ-DICT` | S521, S536 | All 3 are 1 |
| `G-RF-STD-PREHEAT` | S590 | The number of kinds with `HeatSource<oven>` equals the number of kinds with `preheatTime`, and is 1 or more |
| `G-RF-SAMPLES-UNUSED` | S567 | 0 |
| `G-RF-NAMES` | S540, S541 | `names: <n> declarations, <b> bindings, 0 one-letter, 0 unlisted exceptions` |
| `G-RF-PATHS` | S537 | 0 |
| `G-RF-TYPEARGS-STABLE` | S515 | No diff |
| `G-RF-SEPARATION` | S542 | `separation: <c> commits, 0 mixed`, c ≥ 1 |
| `G-RF-EQUIVALENCE` | S538, S539 | `equivalence: <n> rewritten, 0 changed results, <k> changed steps; records: <r> roadmap rows, 0 missing` (r is the number of rows in roadmap §5.3 that point to rewritten tests and have "How to break it") |
| `G-RF-SAMPLES-PIPE` | S616 | `samples-pipe: <n> samples, 0 without pipe, 0 unlisted exceptions` |
| `G-RF-SAMPLES-SAME` | S617, S618, S620 | `samples-same: <n> samples, 0 changed ir, 0 changed schedules, 0 changed renders` |
| `G-RF-SAMPLES-EXPECT` | S619 | No diff |
| `G-RF-LIMIT-PAIRS` | S543 | `limit-pairs: <p> pairs, 0 not exact` |
| `G-RF-GOLDEN-COUNT` | S552 | N0 + 85 |
| `G-RF-CONCEPT` | S604 to S608 | All 5 numbers are 1 or more |
| `G-RF-CLAUDEMD` | S613 | 1 or more |
| `G-RF-CI-NO-PROTO` | The CI side of S480 | 0 runs of `ios-proto` at the pushed HEAD |
| `G-RF-AUDIT` | The record of the P9-4 audit | 1 or more |

```bash
# G-RF-DECISIONS
grep -c '^### D[0-9]*\. .*\[decided\]' $PLAN_RF   # → 17
grep -c '^### D[0-9]*\. .*\[open\]' $PLAN_RF       # → 0
grep -c '^- \*\*Decision:' $PLAN_RF                    # → 17
# G-RF-COVERAGE
diff <(sed -n '/^## 4\./,/^## 5\./p' $PLAN_RF | grep -oE '\*\*S[0-9]+\*\*' | tr -d '*' | sort -u) \
     <(sed -n '/^### 5\.3/,/^## 6\./p' $PLAN_RF | grep -oE '^\| S[0-9]+' | tr -d '| ' | sort -u)   # → empty
sed -n '/^## 8\./,/^## 9\./p' $PLAN_RF | grep '^- ' | grep -v 'Reason a command cannot decide:' | wc -l   # → 0
# G-RF-PLAN-TESTS
scripts/check-plan-tests.sh $PLAN_RF               # → plan-tests: <n> names, 0 missing
scripts/check-plan-tests.sh --mutations $PLAN_RF   # → mutations: <m> rows, <m> recorded
# G-RF-TAGS
for t in refresh-start proto-final rewrite-start rewrite-end; do git rev-parse -q --verify "refs/tags/$t" >/dev/null && echo ok; done | wc -l   # → 4
# G-RF-PROTO-GONE
git ls-files proto .github/workflows/ios-proto.yml | wc -l                                   # → 0
[ "$(git rev-parse proto-final)" = "$(git rev-parse "$(git log --diff-filter=D --format=%H -1 -- proto)^")" ] && echo same   # → same
sed -n '/^goal_G_CORE_\(FLAGS\|PURE\|NOGLOBAL\|DIAG_REGISTRY\)()/,/^}/p' scripts/goals.sh | grep -c 'prot[o]/'   # → 0
sed -n '1,10p' docs/prototype-report.md | grep -c 'proto-final'                              # → 1 or more
git grep -nE '(^|[^-a-z])prot[o]/' -- scripts .github ':!scripts/check-red-first.sh' | wc -l    # → 0
# G-RF-NO-PROTO-GOALS
scripts/goals.sh --list | grep -c '^G-PROTO-'      # → 0
# G-RF-README-NO-PROTO
grep -c '^## Evaluator prototype' README.md        # → 0
# G-RF-README-EXAMPLE
scripts/check-readme-example.sh --readme README.md --ozen build/core-gcc-release/ozen   # → readme: <n> examples, 0 mismatches
# G-RF-README-PIPE
sed -n '/^```ozen/,/^```/p' README.md | grep -c '|>'   # → 1 or more
# G-RF-NO-EMPTY-PARENS (added in P3-2; requires 0 from P5-7)
python3 scripts/check-test-names.py --empty-parens --exceptions scripts/empty-parens-exceptions.txt tests core/tests/unit bindings/inputs renderers/test/inputs README.md
# G-RF-CONCEPT-PARENS (from P9-1)
sed -n '/^```/,/^```/p' docs/concept.md | grep -cE '\b(recipe|menu) +[A-Za-z_][A-Za-z0-9_]* *\( *\)|\bmake +[A-Za-z_.]+ *\( *\)'   # → 0
# G-RF-FUZZ-DICT
for w in '"|>"' '"per"' '"serving"'; do grep -cxF "$w" core/fuzz/ozen.dict; done   # → 1 1 1
# G-RF-STD-PREHEAT (added in P7-1)
python3 scripts/check-std-preheat.py --ozen build/core-gcc-release/ozen   # → oven kinds: <k>, with preheatTime: <k> (k ≥ 1)
# G-RF-SAMPLES-UNUSED (from P6-3)
for s in samples/*/; do (cd "$s" && ../../build/core-gcc-release/ozen check --format json 2>/dev/null); echo; done | grep -c '"schemaVersion"'   # → the number of samples
for s in samples/*/; do (cd "$s" && ../../build/core-gcc-release/ozen check --format json 2>/dev/null); echo; done | grep -o '"name":"unusedValue"' | wc -l   # → 0
# G-RF-NAMES (added in P5-0)
python3 scripts/check-test-names.py --exceptions scripts/test-names-exceptions.txt tests core/tests/unit bindings/inputs
# G-RF-PATHS (from P5-9)
git diff --name-status --no-renames rewrite-start..rewrite-end -- tests/golden | grep -v '^M' | wc -l   # → 0
# G-RF-TYPEARGS-STABLE (from P5-0)
git diff --exit-code refresh-start rewrite-start -- tests/golden/typeargs tests/golden/decl/alias.out   # → no diff
# G-RF-SEPARATION
scripts/check-commit-separation.sh "$RF" '^tests/golden/.*\.out$' '^(core/src|core/include|cli/src|stdlib|phrases|schemas|tools/schedcheck|renderers/lib)/'
# G-RF-EQUIVALENCE (added in P5-0; --head HEAD during Phase 5, --head rewrite-end from P5-9)
python3 scripts/check-rewrite-equivalence.py --base rewrite-start --head rewrite-end --records $PLAN_RF tests/golden
# G-RF-SAMPLES-PIPE (added in P5-8)
python3 scripts/check-test-names.py --samples-pipe --exceptions scripts/samples-pipe-exceptions.txt samples
# G-RF-SAMPLES-SAME (added in P5-8; builds the two points in temporary worktrees and compares each sample's args and render-all)
python3 scripts/check-rewrite-equivalence.py --base rewrite-start --head rewrite-end --samples samples
# G-RF-SAMPLES-EXPECT (from P5-9)
git diff --exit-code rewrite-start rewrite-end -- 'samples/*/expect.yaml' renderers/test   # → no diff
# G-RF-LIMIT-PAIRS (added in P5-2)
python3 scripts/check-limit-pairs.py --ozen build/core-gcc-release/ozen tests/golden   # → limit-pairs: <p> pairs, 0 not exact
# G-RF-GOLDEN-COUNT
scripts/golden-entries.sh tests/golden | wc -l     # → N0 + 85 (N0 is in §9)
# G-RF-CONCEPT (from P9-1)
sed -n '/^### 4\.4/,/^### 4\.5/p' docs/concept.md | grep -c '|>'
sed -n '/^### 4\.4/,/^### 4\.5/p' docs/concept.md | grep -c 'per serving'
sed -n '/^### 4\.4/,/^### 4\.5/p' docs/concept.md | grep -c '`()` を省く'
sed -n '/^### 4\.1/,/^### 4\.2/p' docs/concept.md | grep -c 'unusedValue'
sed -n '/^### 4\.2/,/^### 4\.3/p' docs/concept.md | grep -c 'preheat'
sed -n '/^### 4\.5/,/^### 4\.6/p' docs/concept.md | grep -c 'useWithin'   # → all of the above are 1 or more
sed -n '/^### 6\.1/,/^## 7\./p' docs/concept.md | grep '^| 警告' | grep -oE 'unusedValue|useWithinMissed' | sort -u | wc -l   # → 2
sed -n '/^### 6\.1/,/^## 7\./p' docs/concept.md | grep '^| エラー' | grep -c 'servingsUnknown'   # → 1
# G-RF-CLAUDEMD
grep -c 'plans/ozen-syntax-refresh.md' CLAUDE.md   # → 1 or more
# G-RF-CI-NO-PROTO (after the push)
gh run list --workflow ios-proto.yml --commit "$(git rev-parse HEAD)" --json databaseId --jq length   # → 0
# G-RF-AUDIT (P9-4)
grep -cE '^- Coverage audit: [0-9]+ findings, [0-9]+ taken' $PLAN_RF   # → 1 or more
```

The new scripts (`check-rewrite-equivalence.py`, `check-test-names.py`, `check-limit-pairs.py`, `check-std-preheat.py`) come with self-tests that confirm they fail on deliberately broken input, `scripts/test-check-rewrite-equivalence.sh`, `scripts/test-check-test-names.sh`, `scripts/test-check-limit-pairs.sh`, `scripts/test-check-std-preheat.sh`, and these are listed in `scripts/selftests.txt` (`G-REPO-SELFTESTS`).

### 5.2 Tests

Common method:

- Goldens use the format of roadmap §4.20.1. Their expected values are written by hand from §4 and placed in a commit before the implementation (roadmap D20, `G-REPO-RED-FIRST`). A golden that checks a new property goes into a new file, not new lines in an existing file (red-first counts only added files).
- Schedules are built by hand, and become expected values only after `schedcheck` confirms 0 violations. Amounts and times are calculated by hand as rational numbers and checked against `fractions.Fraction`.
- Create `tests/golden/pipe/` and `tests/golden/quantity/` as folders only for the new properties.

| Test | What it checks | Main tasks |
| --- | --- | --- |
| `cgolden/quantity/*` (14 cases) | Making Count dimensionless, ingredient dimensions, `per serving` | P2-1, P2-2, P4-1 |
| `cgolden/menus/make-bare`, `make-servings`, `make-servings-per-serving` | `make` without parentheses, and servings | P2-3, P2-4, P4-1 |
| `cgolden/serve/serve-within-backward` | `@serveWithin` with `--serve-at` | P2-5 |
| `cgolden/pipe/*` (15 cases) | Pipe meaning, precedence, errors, annotations, spans, node counts | P3-1 |
| `cgolden/syntax/omit-parens` | The same IR when `()` is omitted | P3-2 |
| `cgolden/linear/unused-*` (16 cases) | `LIN005` | P6-1, P9-4 |
| `cgolden/sched/preheat-*` (20 cases) | Preheating | P7-1, P9-4 |
| `cgolden/serve/use-within-*` (14 cases) | `@useWithin` | P8-1, P9-4 |
| `cgolden/bundle/new-syntax` | Bundling keeps pipes and `per serving` | P4-1 |
| `sh/test-check-rewrite-equivalence.sh`, `sh/test-check-test-names.sh`, `sh/test-check-limit-pairs.sh`, `sh/test-check-std-preheat.sh` | Self-tests of the new check scripts | P3-2, P5-0, P5-2, P7-1 |
| `sh/test-check-flags.sh`, `sh/test-check-pure.sh`, `sh/test-check-noglobal.sh`, `sh/test-check-diag-registry.sh`, `sh/test-check-readme-example.sh` | Self-tests of the moved check scripts | P1-1 |
| Examples added to `sh/test-schedcheck.sh`, and `tools/schedcheck/fixtures/bad-preheat-*.json`, `bad-use-within.json` | Independent checks of preheating and time limits | P7-3, P8-3 |
| Examples added to `rtest/schedule-text` and `rtest/recipe-schedule` | Preheat sentences and the schedule table | P7-4 |

Golden entries grow by 85 from N0 = 363 at the starting point (§9) (`quantity` 14, `menus` 3, `serve` 15, `pipe` 15, `syntax` 1, `linear` 16, `sched` 20, `bundle` 1). The coverage audit of P9-4 added 17 (§9).
A failing test is not loosened; find the cause. Report mismatches with the spec to the user.

What machines cover: syntax, evaluation, schedules, JSON, words in documents, test names and paths, equivalence before and after the rewrite, limit pairs, CI digests. Only these go to §8: whether the rewritten recipes are natural as cooking, whether the preheat time values are reasonable, how readable pipes are, how noisy the warnings are, and the style of the concept.

### 5.3 Coverage table

Each S item of §4 and the list in §4.11 is assigned, per aspect, to a test or to §8. "Real path" is met by going through the CLI (`ozen`) (all goldens go through the CLI). "Platform path" is met by `G-PLAT-LINUX`, `G-PLAT-WASM`, and the CI goals after a push, `G-PLAT-IOS` and `G-PLAT-ANDROID` (golden digests).

| S item | Aspect | Test or goal | How to break it | Main tasks |
| --- | --- | --- | --- | --- |
| S479 | Normal | `G-RF-PROTO-GONE` | Restore one file under `proto/` | P1-2 |
| S480 | Normal | `G-RF-PROTO-GONE` | Restore `ios-proto.yml` | P1-2 |
| S480 | Platform path | `G-RF-CI-NO-PROTO` | — | P1-3 |
| S481 | Persistence | `G-RF-PROTO-GONE` | Put the tag on the commit one before | P1-2 |
| S482 | Data completeness | `G-RF-PROTO-GONE` (the 4 functions in `goals.sh`) | Point the path in `G-CORE-FLAGS` back to `proto/scripts/` | P1-1 |
| S483 | Normal | `G-CORE-FLAGS` | Restore how the moved `check-flags.py` finds the root | P1-2 |
| S483 | Normal | `G-CORE-PURE` | Restore how the moved `check-pure.sh` finds the root | P1-2 |
| S483 | Normal | `G-CORE-NOGLOBAL` | Restore how the moved `check-noglobal.py` finds the root | P1-2 |
| S483 | Normal | `G-CORE-DIAG-REGISTRY` | Point the default path of `check-diag-registry.sh` back to `proto/src/` | P1-2 |
| S484 | Data completeness | `G-REPO-SELFTESTS`, `sh/test-check-flags.sh`, `sh/test-check-pure.sh`, `sh/test-check-noglobal.sh`, `sh/test-check-diag-registry.sh`, `sh/test-check-readme-example.sh` | Delete one moved line from `selftests.txt` | P1-1 |
| S485 | Docs match | `G-RF-README-NO-PROTO` | Restore the section heading | P1-2 |
| S486 | Docs match | `G-REPO-README-LICENSE` | Put `proto/` back into a row of the license table | P1-2 |
| S487 | Data completeness | `G-PLAN-TESTS-EXIST` | Delete one `golden/*` line from `retired-tests.txt` | P1-2 |
| S488 | Docs match | `G-RF-PROTO-GONE` (the top of the report) | Delete the note at the top | P1-2 |
| S489 | Normal | `sh/test-check-readme-example.sh` (pass different scripts to `--bin` and `--ozen`, and add a case where a block without `run:` runs with `--ozen`) | Make `--bin` the default executable | P1-1 |
| S490 | No-effect condition | `G-RF-PROTO-GONE` | Put `proto/scripts` back into the targets of `check-selftests.sh` | P1-2 |
| S491 | Data completeness | `G-RF-NO-PROTO-GOALS` | Restore `goal_G_PROTO_BUILD` | P1-2 |
| S492 | Normal | `cgolden/quantity/count-times-mass` | Keep the Count dimension in `typed_qty` | P2-1 |
| S493 | Real path | `cgolden/quantity/count-options` (`-s 2`) | Give the Count dimension only to option values | P2-1 |
| S494 | Normal | `cgolden/quantity/count-kitchen` | Give the Count dimension only to kitchen items | P2-1 |
| S495 | Normal | `cgolden/quantity/count-compare` | Put the Count dimension back on Count-typed values only when evaluating comparisons | P2-1 |
| S496 | Boundary | `cgolden/quantity/count-with-pcs` | Keep the Count dimension only on values with `pcs` | P2-1 |
| S497 | Normal | `cgolden/quantity/ingredient-dimension` | Remove the dimension check | P2-2 |
| S498 | Data completeness | `cgolden/quantity/ingredient-dimension` | Make the location the ingredient's name | P2-2 |
| S499 | No-effect condition | `cgolden/quantity/ingredient-dimension-ok` | Make volumes an error | P2-2 |
| S500 | Normal | `cgolden/menus/make-bare` | Turn a name-only `make` back into `unknown()` | P2-3 |
| S501 | Normal | `cgolden/menus/make-servings` | Leave `eval_make` putting the old servings back | P2-4 |
| S502 | Time and order | `cgolden/serve/serve-within-backward` | Do not map the release in backward scheduling (the current behavior) | P2-5 |
| S503 | No-effect condition | `cgolden/serve/serve-within-backward` | Emit `SCH010` even when the limit is met | P2-5 |
| S504 | Normal | `cgolden/pipe/bare-name` | Add no argument when the right side is a name | P3-1 |
| S505 | Normal | `cgolden/pipe/first-arg` | Put the left side into the last argument position | P3-1 |
| S506 | Time and order | `cgolden/pipe/chain` | Associate from the right | P3-1 |
| S507 | Boundary | `cgolden/pipe/precedence` | Make `pipe` bind tighter than `sum` | P3-1 |
| S508 | No-effect condition | `cgolden/pipe/bad-target-quantity` | Allow any expression on the right side | P3-1 |
| S509 | No-effect condition | `cgolden/pipe/bad-target-list` | Allow any expression on the right side | P3-1 |
| S510 | No-effect condition | `cgolden/pipe/bad-target-make` | Allow `make` on the right side | P3-1 |
| S511 | Normal | `cgolden/pipe/macro` | Do not pass the left side in a macro call | P3-1 |
| S512 | Boundary | `cgolden/pipe/named-only` | Put the left side after the named arguments | P3-1 |
| S513 | Normal | `cgolden/pipe/unattended` | Attach the annotation to the first stage | P3-1 |
| S514 | No-effect condition | `cgolden/pipe/unattended-builtin` | Do not call `check_targets` for rearranged calls | P3-1 |
| S515 | Fixed premise | `G-RF-TYPEARGS-STABLE` | Point `rewrite-start` at a commit that changed one line of `typeargs/alternatives.out` | P3-1 |
| S516 | Data completeness | `cgolden/pipe/ast-rules` | Remove `mark(OZNI_R_pipe)` | P3-1 |
| S517 | Data completeness | `G-CORE-GRAMMAR` | Delete the rule `pipe` from `grammar.ebnf` | P3-1 |
| S518 | Boundary | `cgolden/pipe/ast` | Start the span of a rearranged call at the start of the right side | P3-1 |
| S519 | Fixed premise | `cunit/span_nesting` | End the span of a rearranged call at the end of the left side | P3-1 |
| S520 | Side effects | `cgolden/pipe/nodes` (one short), `cgolden/pipe/nodes-exact` (exact) | When the right side is a name, rewrite the existing Name node into a Call and add no node | P3-1 |
| S521 | Data completeness | `G-RF-FUZZ-DICT` | Delete `"\|>"` from the dictionary | P3-1 |
| S522 | Normal | `cgolden/syntax/omit-parens` | Make `()` required for `recipe` and `menu` | P3-2 |
| S523 | Data completeness | `G-RF-NO-EMPTY-PARENS`, `sh/test-check-test-names.sh` | Put one `recipe salad() {` back into `tests/` | P3-2, P5-7 |
| S524 | Docs match | `G-RF-CONCEPT-PARENS` | Add `()` to `make curry` in a code example of the concept | P9-1 |
| S525 | Normal | `cgolden/quantity/per-serving` | Do not multiply by the servings | P4-1 |
| S526 | No-effect condition | `cgolden/quantity/fixed-amount` | Multiply unmarked amounts by the servings too | P4-1 |
| S527 | Normal | `cgolden/quantity/per-serving-param` | Ignore the dish's arguments | P4-1 |
| S528 | Normal | `cgolden/quantity/per-serving-options` | Ignore the option values | P4-1 |
| S529 | No-effect condition | `cgolden/quantity/servings-unknown` | Use 1 when there are no servings | P4-1 |
| S530 | Data completeness | `cgolden/quantity/servings-unknown` | Make the severity a warning | P4-1 |
| S531 | No-effect condition | `cgolden/quantity/per-serving-misplaced` | Read `per serving` inside argument default values too | P4-1 |
| S532 | Normal | `cgolden/menus/make-servings-per-serving` | Do not pass the servings of `make` to `per serving` | P4-1 |
| S533 | Data completeness | `cgolden/quantity/per-serving-json` (`--format json --emit ir`) | Write the amount before multiplying by the servings into the JSON | P4-1 |
| S534 | Data completeness | `cgolden/quantity/per-serving-json` | Multiply with the servings value still carrying the Count dimension | P4-1 |
| S535 | Data completeness | `G-CORE-DIAG-REGISTRY`, `G-CORE-DIAG-APPEND`, `G-STD-PHRASES` | Delete `diag-servings-unknown` from `phrases/ja.ftl` | P4-1 |
| S536 | Data completeness | `G-RF-FUZZ-DICT` | Delete `"serving"` from the dictionary | P4-1 |
| S537 | Data completeness | `G-RF-PATHS` | Rename one golden during the rewrite | P5-9 |
| S538 | Normal | `G-RF-EQUIVALENCE`, `sh/test-check-rewrite-equivalence.sh` | Remove one diagnostic from one rewritten golden | P5-0 to P5-7 |
| S538 | Fixed premise | `G-RF-EQUIVALENCE` (compares the executables built at the two tags) | Compare with the HEAD executable instead (fails after Phase 6) | P5-9 |
| S539 | Data completeness | `G-RF-EQUIVALENCE` (records) | Delete from §9 one record of a roadmap row that points to a rewritten golden | P5-0 to P5-7 |
| S540 | Data completeness | `G-RF-NAMES`, `sh/test-check-test-names.sh` | Put one `recipe r` back | P5-0 to P5-7 |
| S541 | No-effect condition | `G-RF-NAMES` (`unlisted exceptions`) | Delete a reason from the exception list | P5-0 |
| S542 | Side effects | `G-RF-SEPARATION` | Put an expected value and `sema.c` into one commit | P9-5 |
| S543 | Boundary | `G-RF-LIMIT-PAIRS`, `sh/test-check-limit-pairs.sh` | Remove one node from the input of `limits/nodes-exact` | P5-2 |
| S544 | Fixed premise | `G-CORE-GRAMMAR` | Remove the syntax from the only golden that uses the rule `for` (`decl/recipe`) | P5-9 |
| S545 | Fixed premise | `G-CORE-DIAG-REGISTRY` | Remove the diagnostic from the only golden that emits `LIN004` | P5-9 |
| S546 | Fixed premise | `G-CORE-GOLDEN` | Shift a location in one rewritten `.out` by one column | P5-1 to P5-7 |
| S547 | Fixed premise | `G-CORE-UNIT` | Put an undefined name into a recipe in a C string in `core/tests/unit` | P5-7 |
| S548 | Fixed premise | `G-CORE-OOM` | Do not emit `MAC004` when an allocation fails (`check.c`) | P5-2 |
| S549 | Fixed premise | `G-CORE-PROPS` | Add a declaration named `propsOptions` to a rewritten golden (it clashes with `monotonic`) | P5-9 |
| S550 | Fixed premise | `G-SCHED-CHECK` | Use a dirty tool without putting the washing into the schedule (`clean_unit` does not record the washing step) | P5-5 |
| S551 | Platform path | `G-PLAT-LINUX`, `G-PLAT-WASM` | Drop one `emit` on the bridge side (`bindings/linux/main.swift`) | P5-7 |
| S552 | Data completeness | `G-RF-GOLDEN-COUNT` | Delete one new golden | P9-5 |
| S616 | Normal | `G-RF-SAMPLES-PIPE`, `sh/test-check-test-names.sh` (the `--samples-pipe` case) | Turn the pipe in `samples/T2/` back into a chain of `let` | P5-8 |
| S617 | Fixed premise | `G-RF-SAMPLES-SAME`, `sh/test-check-rewrite-equivalence.sh` (the `--samples` case) | Shift a `per serving` amount of `curryRice` by 1 g | P5-8 |
| S618 | Fixed premise | `G-RF-SAMPLES-SAME` | Change the time argument of one action in `menuB` | P5-8 |
| S619 | Fixed premise | `G-RF-SAMPLES-EXPECT` | Rewrite one line of `samples/T1/expect.yaml` | P5-9 |
| S620 | Real path | `G-RF-SAMPLES-SAME` (render-all) | Fold a binding that `phrases` refers to by name into a pipe | P5-8 |
| S553 | Normal | `cgolden/linear/unused-let` | Remove the check | P6-2 |
| S554 | Data completeness | `cgolden/linear/unused-let` | Make the registry severity info | P6-2 |
| S555 | Data completeness | `cgolden/linear/unused-let` | Make the location the expression | P6-2 |
| S556 | Normal | `cgolden/linear/unused-expr` | Skip values without a binding | P6-2 |
| S557 | Normal | `cgolden/linear/unused-ingredient` | Do not track ingredients | P6-2 |
| S558 | No-effect condition | `cgolden/linear/unused-store-discard` | Report even after `store` or `discard` | P6-2 |
| S559 | No-effect condition | `cgolden/linear/unused-part` | Report parts too | P6-2 |
| S560 | No-effect condition | `cgolden/linear/unused-last-tuple` | Report the values of the last tuple | P6-2 |
| S561 | No-effect condition | `cgolden/linear/unused-in-make-only` | Report even with `quiet` | P9-4 |
| S561 | No-effect condition | `cgolden/linear/unused-in-make` | Report even with `quiet` | P6-2 |
| S562 | Normal | `cgolden/linear/unused-in-make-alone` | Do not report a dish called from a menu even when it is checked alone | P6-2 |
| S563 | No-effect condition | `cgolden/linear/unused-composite` | Track values inside `steps` too | P6-2 |
| S564 | No-effect condition | `cgolden/linear/unused-divided` | Report the remainder of a division | P6-2 |
| S565 | Normal | `cgolden/linear/unused-allow` | Ignore the severity setting | P6-2 |
| S566 | Data completeness | `G-CORE-DIAG-REGISTRY`, `G-CORE-DIAG-APPEND`, `G-STD-PHRASES` | Delete `diag-unused-value` from `phrases/en.ftl` | P6-3 |
| S567 | Fixed premise | `G-RF-SAMPLES-UNUSED` | Emit `LIN005` for the last value too | P6-3 |
| S568 | Normal | `cgolden/sched/preheat-basic` | Do not insert preheating | P7-2 |
| S569 | Normal | `cgolden/sched/preheat-basic` (a kitchen with two ovens) | Insert preheating into the first unit | P7-2 |
| S570 | Time and order | `cgolden/sched/preheat-basic` | Place preheating from the start of the step | P7-2 |
| S571 | Time and order | `cgolden/sched/preheat-lookahead` | Start preheating at the time the step becomes ready | P7-2 |
| S572 | Normal | `cgolden/sched/preheat-basic` | Fix the duration at 10 minutes | P7-2 |
| S573 | Side effects | `cgolden/sched/preheat-unattended` | Assign a person to preheating | P7-2 |
| S574 | Side effects | `cgolden/sched/preheat-occupies` | Do not bound the start of preheating by the time the unit becomes free | P7-2 |
| S575 | No-effect condition | `cgolden/sched/preheat-reuse` | Insert preheating every time | P7-2 |
| S575 | Boundary | `cgolden/sched/preheat-same-temp` | Change the temperature comparison from "the same or lower" to "lower" | P9-4 |
| S576 | Boundary | `cgolden/sched/preheat-hotter` | Do not preheat again even for a higher temperature | P7-2 |
| S577 | Normal | `cgolden/sched/preheat-arg-temp` | Read the temperature only as a number literal, not as an expression | P7-2 |
| S578 | Time and order | `cgolden/sched/preheat-backward` | In backward scheduling, place preheating the same way as forward | P7-2 |
| S578 | Time and order | `cgolden/sched/preheat-reuse-backward` | In backward scheduling, do not cancel the preheat reservation | P9-4 |
| S578 | Time and order | `cgolden/sched/preheat-hotter-backward` | In backward scheduling, cancel the preheat reservation without looking at the temperature | P9-4 |
| S579 | No-effect condition | `cgolden/sched/preheat-max-temp` | Do not check `heated` in `req_met` | P7-2 |
| S580 | Boundary | `cgolden/sched/preheat-max-temp-equal` | Use `<=` instead of `<` for the comparison | P7-2 |
| S581 | No-effect condition | `cgolden/sched/preheat-no-max-temp` | Treat a missing `maxTemp` as not met | P7-2 |
| S582 | Fixed premise | `cgolden/sched/preheat-ir` (`debug ir` with an injected kitchen) | Add preheating to the IR output | P7-2 |
| S583 | Data completeness | `cgolden/sched/preheat-basic` | Make the row's word `wash` | P7-2 |
| S584 | Data completeness | `cgolden/sched/preheat-json` | Do not emit `heats` | P7-3 |
| S585 | Data completeness | `cgolden/sched/preheat-json` | Do not emit `needs` | P7-3 |
| S586 | Data completeness | `G-API-SCHEMA` | Delete `preheat` from the `kind` enum of steps in the schema | P7-3 |
| S587 | Fixed premise | `G-SCHED-CHECK`, `sh/test-schedcheck.sh` (the `bad-preheat-*` fixtures) | Remove the preheat check from `schedcheck` | P7-3 |
| S588 | Docs match | `rtest/schedule-text`, `G-STD-PHRASES` | Delete the preheat keys from `ja.ftl` | P7-4 |
| S589 | Real path | `rtest/recipe-schedule` (takes the output of `cgolden/sched/preheat-json` as input) | Skip `preheat` steps in the schedule table | P7-4 |
| S590 | Data completeness | `G-RF-STD-PREHEAT`, `sh/test-check-std-preheat.sh` | Delete `preheatTime` from `OvenRange` | P7-1 |
| S591 | Data completeness | `G-STD-PROVENANCE`, `G-STD-SOURCES` | Remove `@source(provisional)` from `preheatTime` | P7-1 |
| S592 | Time and order | `cgolden/serve/use-within-basic` | Do not add a release | P8-2 |
| S592 | Boundary | `cgolden/serve/use-within-range` | Measure the wait from the lower bound of the end of the producing step | P9-4 |
| S593 | Boundary | `cgolden/serve/use-within-two-consumers` | Look only at the first consuming step | P8-2 |
| S594 | Normal | `cgolden/serve/use-within-missed` | Do not emit the warning | P8-2 |
| S595 | Data completeness | `cgolden/serve/use-within-missed` | Make the location the value's name | P8-2 |
| S596 | Time and order | `cgolden/serve/use-within-backward` | Do not map the release in backward scheduling | P8-2 |
| S597 | No-effect condition | `cgolden/serve/use-within-on-ingredient` | Accept it on an ingredient too | P8-2 |
| S597 | No-effect condition | `cgolden/serve/use-within-on-alias` | Treat a binding as an ingredient only when it has the same name as the ingredient | P9-4 |
| S598 | Normal | `cgolden/serve/use-within-expr` | Do not read annotations on expression statements | P8-2 |
| S599 | No-effect condition | `cgolden/serve/use-within-no-consumer` | Emit `SCH011` for every named limit without checking whether a consuming step exists | P8-2 |
| S600 | Data completeness | `cgolden/serve/use-within-json` | Do not emit `limits` | P8-3 |
| S600 | Data completeness | `cgolden/serve/use-within-missed-json` | Always set `missed` to `false` | P9-4 |
| S601 | Fixed premise | `G-SCHED-CHECK`, `sh/test-schedcheck.sh` (the `bad-use-within` fixture) | Remove the `limits` check from `schedcheck` | P8-3 |
| S601 | Fixed premise | `sh/test-schedcheck.sh` (the `bad-use-marked` and `bad-use-unknown` fixtures) | Remove the checks for a limit marked `missed` that is in fact in time, and for a reference to an unknown step | P9-4 |
| S602 | Fixed premise | `cgolden/serve/use-within-ir` | Make the annotation's name the IR name of the step that produces the value | P8-2 |
| S603 | Data completeness | `G-CORE-DIAG-REGISTRY`, `G-CORE-DIAG-APPEND`, `G-STD-PHRASES`, `G-TYPE-SEVERITY-TABLE` | Delete `SCH:useWithinMissed` from `diag-names.txt` | P8-2 |
| S604 | Docs match | `G-RF-CONCEPT` | Delete `per serving` and `\|>` from concept §4.4 | P9-1 |
| S605 | Docs match | `G-RF-CONCEPT` | Delete `unusedValue` from concept §4.1 | P9-1 |
| S606 | Docs match | `G-RF-CONCEPT` | Delete `preheat` from concept §4.2 | P9-1 |
| S607 | Docs match | `G-RF-CONCEPT` | Delete `useWithin` from concept §4.5 | P9-1 |
| S608 | Docs match | `G-RF-CONCEPT` | Delete `useWithinMissed` from concept §6.1 | P9-1 |
| S609 | Docs match | `G-RF-README-PIPE`, `G-RF-README-EXAMPLE` | Change one character of the expected output of the README pipe example | P9-2 |
| S610 | Fixed premise | `G-REPO-VERSION` | Set only `VERSION` to `0.2.0` | P9-3 |
| S613 | Docs match | `G-RF-CLAUDEMD` | Delete the name of this file from `CLAUDE.md` | P0-3 |
| S614 | Persistence | `prop/bundle_roundtrip` (includes `cgolden/bundle/new-syntax`) | Drop `\|>` when `ozen bundle` copies a pipe expression | P4-1 |
| S615 | Persistence | `G-RF-TAGS` | Delete the tag `rewrite-end` | P5-9 |
| S621 | No-effect condition | `cgolden/linear/unused-repeat` | Track values in the body of a loop too | P9-4 |
| S622 | Normal | `cgolden/wash/reason` | Make the last value of a branch the last value of the dish | P9-4 |
| S623 | Normal | `cgolden/eval/runtime-value-in-if` | Do not report in a dish that has errors | P9-4 |
| S624 | Data completeness | `cgolden/macro/hygiene-tmp` | Do not attach the expansion history to the report | P9-4 |
| S625 | No-effect condition | `cgolden/linear/unused-alias` | Push the same value twice | P9-4 |
| S626 | Normal | `cgolden/menus/split-pass` | Do not report in menus | P9-4 |
| S627 | Normal | `cgolden/linear/unused-if` | Do not track the body of `if` | P9-4 |
| S628 | No-effect condition | `cgolden/linear/unused-non-food` | Report bindings of amounts as unused values too | P9-4 |
| S629 | Data completeness | `cgolden/sched/preheat-no-preheat-time` | Do not insert preheating when there is no `preheatTime` | P9-4 |
| S630 | No-effect condition | `cgolden/sched/preheat-max-temp-mixed` | Do not drop units with a low `maxTemp` from the candidates | P9-4 |
| S631 | No-effect condition | `cgolden/sched/preheat-cooled` | Keep the unit hot even after a step that needs no temperature | P9-4 |
| S632 | Time and order | `cgolden/sched/preheat-short-prep` | Do not make the step wait for the end of preheating | P9-4 |
| S633 | No-effect condition | `cgolden/serve/use-within-bad-arg` | Ignore the dimension of the argument | P9-4 |
| S634 | Data completeness | `cgolden/sched/preheat-json` | Emit the `limits` field even when there is no limit | P9-4 |
| S635 | Real path | `cgolden/serve/use-within-expr` | Do not collect the limits inside `make` | P9-4 |
| S636 | Normal | `cgolden/serve/use-within-missed-no-delay` | Always take the first attempt that looked at the limits, without looking at the score | P9-4 |

The coverage check is `G-RF-COVERAGE` (run after P0-1 and in P9-4).

Findings of the independent audit (subagent) that were not taken, and why:

- From the §8 item "whether the rewritten recipes are natural as cooking", the idea of checking by machine "whether the last step is heating or plating" — D50 says "keep the order of the actions", so when the original test ended after just cutting, the rewrite may keep it that way. The judgment of naturalness stays in §8.
- Item 18 of the P9-4 audit (goldens that combine preheating with `@useWithin`, `@serveWithin` and `OvenRange`) — each mechanism is checked by its own golden, and `G-SCHED-CHECK` (which checks `needs`, `heats` and `limits` independently from the JSON) checks the correctness of combined schedules on all goldens. The combinations run during the audit also had 0 violations
- From item 20 of the P9-4 audit, `@useWithin` on the result of `buy` — there is no producing step, so the IR producer is not a step; the limit has no effect and no diagnostic is emitted. S597 is limited to "the ingredient itself (its declaration and names that rebind it)"
- Item 21 of the P9-4 audit (`@allow(unusedValue)` on ingredients and expression statements) — the severity setting is decided by a shared mechanism (`levels.c`) from the range of the annotations that contain the diagnostic's location, and `diag/annotation-scope` and `bundle/annotation-scope` check it on both ingredients and bindings
- Item 22 of the P9-4 audit (the elements of the preheat row in the schedule table) — `recipe-schedule` checks that the preheat sentence appears in the HTML. The existing schedule table tests check the row structure regardless of the step kind
- Item 23 of the P9-4 audit (deciding by machine whether the researched values fall within the `preheatTime` range) — the research record uses a format with no value field (`spec/sources/search-log.yaml`), and whether the width of the values is reasonable stays in §8
- Item 25 of the P9-4 audit (a goal that matches the number of LIN005 against §9) — the expected values of the goldens fix the number emitted, so no separate goal is added

---

## 6. Phases and tasks

Notation:

- **Completion condition** is a goal ID of §5 or a command whose pass or fail is unambiguous. The prerequisite goals of §5 that are "run locally" are implicitly part of every task. The CI prerequisite goals are part of the completion condition only for gates that push.
- Status reports are written per environment ("Linux: build and golden pass in 4 configurations. WASM: pass. iOS, Android: not yet confirmed by CI after the push").
- Commits are logical units. Commits of expected values and commits of the implementation are separate (`G-RF-SEPARATION`). Pushes happen without asking (the user's instruction of 2026-10-30), but `G-REPO-NOREPLY` is run before every push, and the pushed range is reported.
- Each task's "mutation check" confirms that breaking things the way the coverage table says makes that test fail, and that undoing it makes it pass. It is recorded in §9 as a `- Mutation check: ` line.
- The red-first check means the output of `scripts/check-red-first.sh "$RF"` is `0 not red` (`G-REPO-RED-FIRST` run over this plan's range).

### Phase 0 — Decisions and preparation (no change to feature code)

| ID | What | Completion condition |
| --- | --- | --- |
| P0-1 | Decide D40 to D56 of §2 with the user and write them into this file | `G-RF-DECISIONS`, `G-RF-COVERAGE` |
| P0-2 | Push the starting point `09e05de` and this file. Put the tag `refresh-start` on `main`. Record the baselines (golden entries N0, the number of grammar rules, pass or fail of the local prerequisite goals) in §9 | `git rev-parse --verify refresh-start` succeeds, and all local prerequisite goals of §5 pass |
| P0-3 | Add `PLAN_RF` and `goal_G_RF_*` to `scripts/goals.sh`. Add this file to the Plan section of `CLAUDE.md` | 28 IDs starting with `G-RF-` in `scripts/goals.sh --list`, `G-RF-CLAUDEMD`, `G-RF-COVERAGE`, `G-REPO-CLAUDEMD`. The mutation check of S613 |

### Phase 1 — Removing the prototype (D52)

| ID | What | Completion condition |
| --- | --- | --- |
| P1-1 | Move the 4 check scripts, the README example check, and their self-tests to `scripts/`. Fix how they find the root and their default paths. Fix the 4 functions in `goals.sh` and `selftests.txt`. Make `--ozen` the default of the README example check, and add the case of a block without `run:` to its self-test | `G-CORE-FLAGS`, `G-CORE-PURE`, `G-CORE-NOGLOBAL`, `G-CORE-DIAG-REGISTRY`, `G-REPO-SELFTESTS`, `G-RF-README-EXAMPLE`. The mutation checks of S482, S484, S489 |
| P1-2 | Remove `proto/` from `check-selftests.sh`, `check-spdx.sh`, `check-plan-tests.sh`, `test-check-plan-tests.sh`, `test-check-spdx.sh`. Put the tag `proto-final`, and delete `proto/`, `ios-proto.yml` and the local prototype build. Remove `G-PROTO-*` from `goals.sh`, and add the names of the prototype tests to `retired-tests.txt`. Fix the README section and table, the string in `G-REPO-README-LICENSE`, and the top of the report | `G-RF-PROTO-GONE`, `G-RF-NO-PROTO-GOALS`, `G-RF-README-NO-PROTO`, `G-PLAN-TESTS-EXIST`, `G-REPO-README-LICENSE`, `G-REPO-SELFTESTS`, `G-REPO-SPDX`. The mutation checks of S479 to S481, S483, S485 to S488, S490, S491 |
| P1-3 | Gate. Push | The local prerequisite goals and the Phase 1 goals, `G-PLAT-CI`, `G-RF-CI-NO-PROTO` |

### Phase 2 — Bug fixes (D44, D45)

| ID | What | Completion condition |
| --- | --- | --- |
| P2-1 | First write `cgolden/quantity/count-*` (5 cases). Make values of the Count type dimensionless. In a separate commit, fix by hand the expected values of existing goldens whose display of Count values changes (record the number in §9) | `G-CORE-GOLDEN`, the red-first check. The mutation checks of S492 to S496 |
| P2-2 | First write `cgolden/quantity/ingredient-dimension*`. Check the dimension of ingredient amounts | `G-CORE-GOLDEN`, `G-SAMPLE-CHECK`, the red-first check. The mutation checks of S497 to S499 |
| P2-3 | First write `cgolden/menus/make-bare`. Evaluate `make 名前` as a call | `G-CORE-GOLDEN`, the red-first check. The mutation check of S500 |
| P2-4 | First write `cgolden/menus/make-servings`. Use the servings passed by `make` as the dish's servings | `G-CORE-GOLDEN`, the red-first check. The mutation check of S501 |
| P2-5 | First write `cgolden/serve/serve-within-backward`. Map the backward release onto the reversed clock | `G-CORE-GOLDEN`, `G-SCHED-CHECK`, the red-first check. The mutation checks of S502, S503 |
| P2-6 | Gate | The local prerequisite goals and the Phase 2 goals |

### Phase 3 — Notation (D42 to D44)

| ID | What | Completion condition |
| --- | --- | --- |
| P3-1 | First write `cgolden/pipe/*` (15 cases). Lexer, parser, `grammar.ebnf`, `rules.def`, the fuzz dictionary | `G-CORE-GOLDEN`, `G-CORE-GRAMMAR`, `G-CORE-UNIT`, `G-CORE-FUZZ`, `G-RF-FUZZ-DICT`, the red-first check. The mutation checks of S504 to S514 and S516 to S521 (S515 is done in P5-0, once `G-RF-TYPEARGS-STABLE` can run) |
| P3-2 | First write `cgolden/syntax/omit-parens`. Create `--empty-parens` of `scripts/check-test-names.py`, its self-test, and `scripts/empty-parens-exceptions.txt`. Remove the empty `()` from `renderers/test/inputs/`, `bindings/inputs/` and README (`tests/` and `core/tests/` are cleaned in Phase 5) | `G-CORE-GOLDEN`, `G-REND-TESTS`, `G-PLAT-LINUX`, `G-RF-README-EXAMPLE`, `python3 scripts/check-test-names.py --empty-parens --exceptions scripts/empty-parens-exceptions.txt bindings/inputs renderers/test/inputs README.md` gives `0 found`. The mutation check of S522 |
| P3-3 | Gate | The local prerequisite goals and the Phase 3 goals |

### Phase 4 — Amounts per person (D46)

| ID | What | Completion condition |
| --- | --- | --- |
| P4-1 | First write `cgolden/quantity/per-serving*`, `fixed-amount`, `servings-unknown`, `cgolden/menus/make-servings-per-serving`, `cgolden/bundle/new-syntax`. Syntax, evaluation, registering `TYP020`, `phrases`, the fuzz dictionary | `G-CORE-GOLDEN`, `G-CORE-GRAMMAR`, `G-CORE-DIAG-REGISTRY`, `G-CORE-DIAG-APPEND`, `G-STD-PHRASES`, `G-API-SCHEMA`, `G-CORE-PROPS`, `G-RF-FUZZ-DICT`, the red-first check. The mutation checks of S525 to S536 and S614 |
| P4-2 | Gate | The local prerequisite goals and the Phase 4 goals |

### Phase 5 — Rewriting the tests (D50, D51)

| ID | What | Completion condition |
| --- | --- | --- |
| P5-0 | Put the tag `rewrite-start` on `main`. Create `scripts/check-rewrite-equivalence.py` (it builds the two points in temporary worktrees and compares them, and counts whether §9 has records for the roadmap §5.3 rows that point to rewritten tests), the name check of `check-test-names.py`, a self-test for each, and `scripts/test-names-exceptions.txt`, and fill in the functions in `goals.sh` | `G-REPO-SELFTESTS`, `sh/test-check-rewrite-equivalence.sh` and `sh/test-check-test-names.sh` pass, `G-RF-TYPEARGS-STABLE`. The mutation checks of S515, S538, S541 |
| P5-1 | Rewrite: `syntax`, `decl`, `typeargs`, `names`, `modules`, `packages`, `std` (declarations, names, modules) | The "completion condition of the rewrite" below (targets: these 7 folders) |
| P5-2 | Rewrite: `dim`, `eval`, `macro`, `limits`, `limits-shared`, `diag` (values, evaluation, limits). Recount limits, fuel and allocations. Create `scripts/check-limit-pairs.py` and its self-test | The same (6 folders), `G-RF-LIMIT-PAIRS`, `G-CORE-OOM`. The mutation checks of S543, S548 |
| P5-3 | Rewrite: `state`, `mix`, `parts`, `linear`, `safety`, `actions` (ingredient states and linearity) | The same (6 folders) |
| P5-4 | Rewrite: `ir`, `levels`, `kitchen`, `vessels`, `serve`, `menus` (IR, kitchen, menus). Goldens added in Phases 2 to 4 of this plan are not rewritten | The same (6 folders) |
| P5-5 | Rewrite: `sched`, `wash`, `cleanup` (schedules) | The same (3 folders), `G-SCHED-CHECK`. The mutation check of S550 |
| P5-6 | Rewrite: `api`, `bundle` (the API and bundles) | The same (2 folders), `G-BUNDLE-STABLE`, `G-API-SCHEMA` |
| P5-7 | Rewrite: `tests/cli` (heredocs and the templates in `lib.sh`), the C strings in `core/tests/unit`, `bindings/inputs/`, `tests/samples/test-check.sh` | `G-CLI-TESTS`, `G-CORE-UNIT`, `G-PLAT-LINUX`, `G-PLAT-WASM`, `G-RF-NAMES`, `G-RF-NO-EMPTY-PARENS`. The mutation checks of S523, S547, S551 |
| P5-8 | Rewrite the samples (D54): turn the `let` chains in `samples/*` into pipes, and make ingredients that scale with servings `per serving`. Create `--samples` of `check-rewrite-equivalence.py` and `--samples-pipe` of `check-test-names.py`, their self-tests, and `scripts/samples-pipe-exceptions.txt` | `G-RF-SAMPLES-PIPE`, `G-RF-SAMPLES-SAME` (`--head HEAD`), `G-SAMPLE-CHECK`, `G-SAMPLE-STD-ONLY`, `G-REND-TESTS`, `G-REND-HTML`, `G-REND-PHRASES-RENDER`, `G-REPO-SPDX`, `G-CORE-PROPS` (`bundle_roundtrip`). The mutation checks of S616 to S618 and S620 |
| P5-9 | Put the tag `rewrite-end` on `main`. Gate | `G-RF-PATHS`, `G-RF-EQUIVALENCE` (`--head rewrite-end`), `G-RF-SAMPLES-SAME` (`--head rewrite-end`), `G-RF-SAMPLES-EXPECT`, `G-RF-NAMES`, `G-RF-NO-EMPTY-PARENS`, `G-RF-SEPARATION`, the goals of S544 to S551, and the 4 tags `refresh-start`, `proto-final`, `rewrite-start`, `rewrite-end` exist (`G-RF-TAGS`). The mutation checks of S537, S538 (fixed premise), S544, S545, S549, S615, S619 |

Completion condition of the rewrite (P5-1 to P5-6): in the target folders, `G-RF-EQUIVALENCE` (`--head HEAD`) gives `0 changed results`, and for every roadmap §5.3 row (with "How to break it") that points to a test in the target folders, §9 has a `- Rewrite mutation check: ` record of the redone mutation check (D51). `G-RF-NAMES` gives `0 one-letter` in the target folders. `G-CORE-GOLDEN` (4 configurations), `G-CORE-GRAMMAR`, `G-CORE-DIAG-REGISTRY`, `G-CORE-UNIT`, `G-CORE-OOM` and `G-CORE-PROPS` pass. `G-RF-SEPARATION` gives `0 mixed`. The mutation checks of S539, S540 and S546 are recorded once each in P5-1, which is done first.

### Phase 6 — Unused values (D47)

| ID | What | Completion condition |
| --- | --- | --- |
| P6-1 | First write `cgolden/linear/unused-*` (11 cases). In a separate commit, fix by hand the expected values of existing goldens that emit `LIN005` (record the number in §9) | The red-first check |
| P6-2 | Implement the tracking table and the check in `finish_dish` | `G-CORE-GOLDEN`, `G-CORE-OOM`, `G-CORE-PROPS`. The mutation checks of S553 to S565 |
| P6-3 | The registry, `diag-names.txt`, `phrases`, the expected values of `api/local-not-listed`, the renderer's argument kinds. Confirm that the samples emit no `LIN005` (if they do, report to the user whether the sample or the rule is wrong. Record in §9 the number of `LIN005` in the samples and in the rewritten goldens) | `G-CORE-DIAG-REGISTRY`, `G-CORE-DIAG-APPEND`, `G-STD-PHRASES`, `G-SAMPLE-CHECK`, `G-REND-TESTS`, `G-RF-SAMPLES-UNUSED`. The mutation checks of S566, S567 |
| P6-4 | Gate | The local prerequisite goals and the Phase 6 goals |

### Phase 7 — Preheating (D48)

| ID | What | Completion condition |
| --- | --- | --- |
| P7-1 | First add the preheat check of `schedcheck` and the `bad-preheat-*` fixtures. Write `cgolden/sched/preheat-*` (13 cases) (build the schedules by hand and confirm 0 violations with `schedcheck`). Add `preheatTime` with its source and the research record (`spec/sources/search-log.yaml`) to the std `Oven` and `OvenRange`, and fix the expected values of `api/local-not-listed`. Create `scripts/check-std-preheat.py` and its self-test | The red-first check, `G-RF-STD-PREHEAT`, `G-STD-PROVENANCE`, `G-STD-SOURCES`. The mutation checks of S590, S591 |
| P7-2 | Preheating in `sched.c`, `sched.h`, `sema.c` (lookahead, backward, `maxTemp`). Fix the 6 places that do not tell inserted steps apart by kind | `G-CORE-GOLDEN`, `G-SCHED-CHECK`, `G-CORE-PROPS`. The mutation checks of S568 to S583 |
| P7-3 | The schedule JSON, the schema, `schedcheck` | `G-API-SCHEMA`, `G-SCHED-CHECK`, `G-REPO-SELFTESTS`. The mutation checks of S584 to S587 |
| P7-4 | The renderer and `phrases`. Add a JSON input that contains preheating to `rtest/recipe-schedule` | `G-REND-TESTS`, `G-REND-HTML`, `G-STD-PHRASES`. The mutation checks of S588, S589 |
| P7-5 | Gate | The local prerequisite goals and the Phase 7 goals |

### Phase 8 — Time limit from making to using (D49)

| ID | What | Completion condition |
| --- | --- | --- |
| P8-1 | First add the `limits` check of `schedcheck` and the `bad-use-within` fixture. Write `cgolden/serve/use-within-*` (9 cases) (the schedules are built by hand) | The red-first check |
| P8-2 | Reading the annotation, the release iteration, registering `SCH011` and the severity table, backward scheduling | `G-CORE-GOLDEN`, `G-SCHED-CHECK`, `G-CORE-DIAG-REGISTRY`, `G-CORE-DIAG-APPEND`, `G-TYPE-SEVERITY-TABLE`, `G-CORE-PROPS`. The mutation checks of S592 to S599, S602, S603 |
| P8-3 | `limits` in the schedule JSON, the schema, `schedcheck`, `phrases` | `G-API-SCHEMA`, `G-SCHED-CHECK`, `G-STD-PHRASES`. The mutation checks of S600, S601 |
| P8-4 | Gate | The local prerequisite goals and the Phase 8 goals |

### Phase 9 — Documents and finishing

| ID | What | Completion condition |
| --- | --- | --- |
| P9-1 | Add to concept §4.1, §4.2, §4.4, §4.5, §6.1, and remove the empty `()` from the examples (D53) | `G-RF-CONCEPT`, `G-RF-CONCEPT-PARENS`. The mutation checks of S524 and S604 to S608 |
| P9-2 | Add pipe and `per serving` examples to the README | `G-RF-README-PIPE`, `G-RF-README-EXAMPLE`. The mutation check of S609 |
| P9-3 | Confirm that the version stays at `0.1.0`, as D55 says | `G-REPO-VERSION`. The mutation check of S610 |
| P9-4 | Coverage audit: add the facts that grew in §9 to §4 and the coverage table. Judge each item of §8 again, with knowledge from after the implementation, on whether a command can decide it, and turn those that can into tests. Have an independent subagent read §1, §4, §8, the coverage table, and the source of the added and rewritten tests, and list, with evidence, claims without tests and items in §8 that a command can decide. Add each finding, or write at the end of §5.3 why it is not added. Write `- Coverage audit: <k> findings, <m> taken` in §9 | `G-RF-COVERAGE`, `G-RF-PLAN-TESTS`, `G-RF-AUDIT` |
| P9-5 | Final check. On `main`, run the local prerequisite goals and `G-RF-*`, push, and run the CI prerequisite goals. Hand §8 over to the user | All local and CI prerequisite goals (including the digests of `G-PLAT-IOS` and `G-PLAT-ANDROID`) and all of `G-RF-*` pass. The mutation checks of S542, S552. Report per environment |

---

## 7. Dependencies between phases

- Phase 0 is the prerequisite for everything. Do not start P0-2 or later until P0-1 is done.
- **Phase 1 (removing the prototype) is independent of Phases 2 to 4**. The files it touches (`proto/`, `scripts/`, README, `.github/`) do not overlap with `core/` and `tests/golden/`, which Phases 2 to 4 touch (README is also touched in P3-2, so P3-2 comes after P1-2). It may run in parallel in another session. Finish it before Phase 5.
- P2-1 to P2-5 of Phase 2 are independent of each other.
- Phase 3 comes after Phase 2 (they touch nearby parts of the same `sema.c`).
- Phase 4 comes after Phase 2 (the Count of D45) and Phase 3 (the syntax changes go into the same `parse.c`).
- **Phase 5 comes after Phases 1 to 4**. The rewrite uses pipes, omitted `()`, `per serving`, and the fixed dimensions. P5-1 to P5-8 are independent of each other and touch different folders. They may be split into separate sessions (they all go into the same `main`, so look at `git log` and `git status` before committing; R6). P5-0 comes first and P5-9 last. P5-8 (samples) affects `bundle_roundtrip` and the renderer tests, which use `samples/` as input, so do not push it on the same day as other rewrites (the gate checks them together).
- Phase 6 comes after Phase 5 (D56).
- Phases 7 and 8 are independent of each other, but both touch the release iteration of `schedule_dish` and `schedule_json`, so do them one after the other (not in parallel).
- Phase 9 comes after all phases.

---

## 8. Checks handed over to the user

Claude is responsible up to the machine-checked goals. The user checks the following. Each item has a reason a command cannot decide it. An item without such a reason becomes a test in §5.3.

- Whether the recipes in the rewritten tests are natural as cooking (ingredient combinations, amounts, order of steps). Look at a few per folder of `tests/golden` — Reason a command cannot decide: neither the spec nor outside sources give a right answer for what is natural as cooking
- Whether recipes written with pipes are easier to read than ones written with `let`. Judge from the examples in the README and the tests — Reason a command cannot decide: readability is a human judgment
- Whether the provisional preheat time values (`preheatTime`) themselves are reasonable for home ovens (that they are held as ranges and that their sources are recorded is checked by `G-STD-PROVENANCE` and `G-STD-SOURCES`) — Reason a command cannot decide: when research in primary sources finds no value, a provisional range is used, and whether its width is reasonable is a human judgment
- Whether the severity of `LIN005` (warning) is too noisy when actually writing recipes (the number emitted is recorded in §9) — Reason a command cannot decide: telling a mistake from something deliberate depends on the writer's intent
- Whether, in the rewritten samples, the split between ingredients made `per serving` and ingredients kept as totals is natural as cooking (that the amounts themselves do not change is checked by `G-RF-SAMPLES-SAME`) — Reason a command cannot decide: which ingredients scale with servings is a judgment of cooking knowledge
- Whether the text added to the concept fits the existing style and design ideas (which section says what is checked by `G-RF-CONCEPT`) — Reason a command cannot decide: consistency of style and ideas is a human judgment

---

## 9. Items to check and record at the start

(Add the result checked in each task here, with the date.)

- Baseline at the starting point (P0-2): the golden entries N0 (363 in the survey of 2026-10-28), the number of grammar rules (131), and whether the local prerequisite goals pass
  - Measured on 2026-10-30 at `ef81dec`: N0 is 363 (`scripts/golden-entries.sh tests/golden | wc -l`). The grammar has 131 rules (`G-CORE-GRAMMAR` reports "131 rules, 0 not covered")
  - On the same day, the 57 local prerequisite goals of §5 and the 23 `G-PROTO-*` goals other than `G-PROTO-IOS` were run together. 79 passed and 1 failed (`G-PROTO-REPORT`)
  - Cause of the `G-PROTO-REPORT` failure: `G-PROTO-FUZZ`, which ran earlier in the same run, ran the fuzzer again and overwrote the local `build/fuzz/summary.txt` with "628402 execs". Line 78 of `docs/prototype-report.md` quotes the previous "746382 execs", so the comparison does not match. The number of runs changes every time, so this is not a defect of the prototype but a problem of the order in which the two goals run one after the other. The prototype is removed in P1, so it was not fixed. The numbers in `docs/prototype-report.md`, which P1-1 keeps, stay as they are
- How to push (2026-10-30): the user said "push as a rule". Push without asking, and run `G-REPO-NOREPLY` before every push
- Records of the mutation checks (each task): one line for each row of the coverage table that has a "How to break it" entry, as a bullet that starts with "Mutation check" followed by a colon, giving the test, how it was broken, why it failed, and that it passed after the change was undone
- Records of the rewrite mutation checks (P5-1 to P5-7): one line for each row of roadmap §5.3 that points to a rewritten test (rows that have a "How to break it" entry), as a bullet that starts with "Rewrite mutation check" followed by a colon, giving the roadmap S item and aspect, the test, how it was broken, and why it failed (read by `check-rewrite-equivalence.py --records`. D51)
- The number of existing golden tests whose Count display changed (P2-1)
- The number of existing golden tests that report LIN005 (P6-1. The estimate before the rewrite is 131), and the number of LIN005 in the samples and the rewritten golden tests (P6-3)
- Coverage audit (P9-4): a bullet that starts with "Coverage audit" followed by a colon, giving the number of findings and the number taken
- Audit right after the plan was written (2026-10-28): an independent subagent made 61 findings (9 goals that fail or cannot decide, 12 that do not fail when broken, 13 claims missing from the coverage table, 22 items that should be split, 6 completion conditions with no single verdict, 5 items that could move from §8, 4 factual mismatches. The categories overlap). 60 were taken, and for 1 the reason for not taking it was written at the end of §5.3
- The grep of `G-RF-PROTO-GONE` (P0-3, 2026-10-30): writing `proto/` as is makes the goal count its own line in `goals.sh`, so it never reaches 0. The two greps in §5.1 were changed to `prot[o]/`. They match the same strings
- Mutation check: S613, `G-RF-CLAUDEMD` (P0-3, 2026-10-30). Replacing `plans/ozen-syntax-refresh.md` in `CLAUDE.md` with another name made the count 0, and the goal failed with "plan named in CLAUDE.md: got '0'". It passed after the change was undone
- Mutation check: S482, `G-RF-PROTO-GONE` (the 4 functions in `goals.sh`. P1-1, 2026-10-30). Changing the path in `goal_G_CORE_FLAGS` back to `proto/scripts/check-flags.py` made it fail with "proto/ in moved goals: got '1'". It passed with 0 after the change was undone
- Mutation check: S484, `G-REPO-SELFTESTS` (P1-1, 2026-10-30). Removing the line for `scripts/check-pure.sh` from `selftests.txt` made it fail with "1 without test". It passed after the change was undone
- Mutation check: S489, `sh/test-check-readme-example.sh` (P1-1, 2026-10-30). Changing the default executable for blocks without `run:` back to `--bin` gave "9 cases, 4 failed", with `default-not-bin` and others failing. It passed after the change was undone
- Relative paths in the README example check (P1-1, 2026-10-30): the check calls the executable in a temporary folder for each example, so `--ozen build/core-gcc-release/ozen` of `G-RF-README-EXAMPLE` was not found. Values that contain a separator are now made absolute, and the self-test `relative-paths` was added. It was confirmed that this case fails with the script before the fix
- The prototype example in the README (P1-1, 2026-10-30): the default became `ozen`, so `<!-- run: ozenproto example0.ozen -->` was added to the example in the prototype section. The section is removed in P1-2. The mapping between `--bin` and `ozenproto` is also removed in P1-2, together with the section
- Mutation check: S479, `G-RF-PROTO-GONE` (P1-2, 2026-10-30). Restoring `proto/defects.tsv` from `proto-final` and tracking it made the goal fail with "tracked prototype files: got '1'". It passed after the file was removed
- Mutation check: S480, `G-RF-PROTO-GONE` (P1-2, 2026-10-30). Restoring `ios-proto.yml` made it fail with 1 tracked file and 4 references in `scripts` and `.github`. It passed after the file was removed
- Mutation check: S481, `G-RF-PROTO-GONE` (P1-2, 2026-10-30). Pointing the tag `proto-final` at `HEAD~2` made it fail with "proto-final is the parent of the deletion". It passed after the tag was pointed back at the original object
- Mutation check: S483, `G-CORE-FLAGS` (P1-2, 2026-10-30). Changing the path in `goal_G_CORE_FLAGS` back to `proto/scripts/check-flags.py` made it fail in all 4 configurations. It passed after the change was undone
- Mutation check: S483, `G-CORE-PURE` (P1-2, 2026-10-30). Changing the path in `goal_G_CORE_PURE` back to `proto/scripts/check-pure.sh` made it fail. It passed after the change was undone
- Mutation check: S483, `G-CORE-NOGLOBAL` (P1-2, 2026-10-30). Changing the path in `goal_G_CORE_NOGLOBAL` back to `proto/scripts/check-noglobal.py` made it fail. It passed after the change was undone
- Mutation check: S483, `G-CORE-DIAG-REGISTRY` (P1-2, 2026-10-30). Changing the default registry of `check-diag-registry.sh` back to `proto/src/diag_codes.def` made it fail because the registry was missing. It passed after the change was undone
- Mutation check: S485, `G-RF-README-NO-PROTO` (P1-2, 2026-10-30). Adding the heading `## Evaluator prototype` to the README made it fail. It passed after the heading was removed
- Mutation check: S486, `G-REPO-README-LICENSE` (P1-2, 2026-10-30). Putting `proto/` back into a row of the license table made it fail with "code row: got '0'". It passed after the change was undone
- Mutation check: S487, `G-PLAN-TESTS-EXIST` (P1-2, 2026-10-30). Removing the first `golden/*` line from `retired-tests.txt` made it fail with "367 names, 1 missing". It passed after the change was undone
- Mutation check: S488, `G-RF-PROTO-GONE` (P1-2, 2026-10-30). Removing the note at the top of the report made it fail with "proto-final in report header: got '0'". It passed after the change was undone
- Mutation check: S490, `G-RF-PROTO-GONE` (P1-2, 2026-10-30). Putting `proto/scripts` back among the targets of `check-selftests.sh` made it fail with 1 reference. It passed after the change was undone
- Mutation check: S491, `G-RF-NO-PROTO-GOALS` (P1-2, 2026-10-30). Adding `goal_G_PROTO_BUILD` made it fail with "G-PROTO goals: got '1'". It passed after it was removed
- How S483 was broken (P1-2, 2026-10-30): the coverage table said "undo the way the moved `check-flags.py`, `check-pure.sh` and `check-noglobal.py` find the root", but these 3 do not find the root; they only read the path given as an argument. So the check broke the goals by changing the paths in the goal functions back to `proto/scripts/`. `check-diag-registry.sh` was checked with its default path, as the coverage table says. For that, P1-1 made `G-CORE-DIAG-REGISTRY` call it with the default path
- `--bin` in the README example check (P1-2, 2026-10-30): together with the prototype section, the mapping between `--bin` and `ozenproto` was removed. S489 is still checked with cases such as `match`, which run blocks without `run:` with `--ozen`
- Removal of the prototype (P1-2, 2026-10-30): `proto/` had 565 files (574, minus the 10 moved to `scripts/`, plus 1 for `ios-proto.yml`). The 4 local prototype builds `build/gcc-*` and `build/clang-*` were removed after confirming that the source in their `CMakeCache.txt` was `proto`. `build/wasm`, `build/wasm-web` and `build/fuzz` also belong to the prototype, but D52 does not list them for removal, so they were kept
- Wording of the headings in `G-RF-PROTO-GONE` (P1-2, 2026-10-30): the heading string "proto/ in …" of the verdict was also counted by the grep, so it was changed to "prototype paths in …"
- Existing golden tests whose Count display changed (P2-1, 2026-10-30): 0. After `typed_qty` made values of the Count type dimensionless and `check_value` required dimensionless values for the Count type, all 363 existing tests passed unchanged. No existing golden test shows a value of the Count type, so no commit to rewrite expected values was needed (R7 did not happen)
- Mutation check: S492, `cgolden/quantity/count-times-mass` (P2-1, 2026-10-30). Restoring the old code in `typed_qty` that gives dimensionless values the Count dimension made `count-times-mass` fail with a dimension mismatch in `rice: 150 g * servings` (`check_value` requires dimensionless values, so the 79 existing golden tests with an argument of the Count type failed too). It passed after the change was undone
- Mutation check: S493, `cgolden/quantity/count-options` (P2-1, 2026-10-30). In `eval_param`, giving the Count dimension only to dimensionless values that come from options (`-s 2`) made only `count-options` fail. It passed after the change was undone
- Mutation check: S494, `cgolden/quantity/count-kitchen` (P2-1, 2026-10-30). In `entry_field`, giving the Count dimension only to dimensionless values of kitchen entries made only `count-kitchen` fail. It passed after the change was undone
- Mutation check: S495, `cgolden/quantity/count-compare` (P2-1, 2026-10-30). In the evaluation of comparisons, giving the Count dimension back to a dimensionless value whose left side is a name made only `count-compare` fail. It passed after the change was undone
- Mutation check: S496, `cgolden/quantity/count-with-pcs` (P2-1, 2026-10-30). Removing the code in `typed_qty` that drops the `pcs` dimension made only `count-with-pcs` fail. It passed after the change was undone
- Mutation check: S497, `cgolden/quantity/ingredient-dimension` (P2-2, 2026-10-30). Removing the dimension check of ingredient amounts let `rice: 3 min` through with "result ok", and the test failed. It passed after the change was undone
- Mutation check: S498, `cgolden/quantity/ingredient-dimension` (P2-2, 2026-10-30). Making the position of the diagnostic the node of the entry that starts at the ingredient name made the position no longer match the amount expression, and the test failed. It passed after the change was undone
- Mutation check: S499, `cgolden/quantity/ingredient-dimension-ok` (P2-2, 2026-10-30). Removing volume from the accepted dimensions made `milk: 200 ml` report `TYP001`, and the test failed (the 51 existing golden tests with volume ingredients failed too). It passed after the change was undone
- Red-first for `ingredient-dimension-ok` (P2-2, 2026-10-30): before the check, ingredient amounts were not checked at all, so the golden test with a valid amount passes even before the fix. It was put in a different commit from the error golden test, and the reason was written in `scripts/red-first-exceptions.txt`
- Mutation check: S500, `cgolden/menus/make-bare` (P2-3, 2026-10-30). Restoring the old code in `eval_make` that returns `unknown()` without looking at a child of `make` that is not a call made the step lines disappear, and the test failed. It passed after the change was undone
- How `make-servings` is observed (P2-4, 2026-10-30): the golden test written first expected `KIT003` (4 servings for 3 bowls) from `ozen check`, but diagnostics of dishes evaluated inside `make` are suppressed to avoid duplicates (`s->quiet`), so `KIT003` does not appear even when the servings are passed. The servings show up in the number of vessels used for plating in the schedule, so the golden test was changed to use `debug schedule` with a kitchen of 5 bowls and to check that 4 are used (before the fix the servings were not set and 1 was used). The gap where a dish inside `make` quietly plates only what fits when vessels run short is not fixed in this plan; it is reported to the user
- Mutation check: S501, `cgolden/menus/make-servings` (P2-4, 2026-10-30). Restoring the old code in `eval_param` that does not use `servings`, already bound by the argument of `make`, as the servings of the dish made plating use only 1 vessel, `[bowl#1]`, and the test failed. It passed after the change was undone
- How a backward release is mapped (P2-5, 2026-10-30): a forward release (do not start before this time) becomes a deadline "finish by this time" on the reversed clock, so it cannot be mapped as a release. In the reversed schedule the release is dropped, and the priority of the task that has the release, and of the tasks that must finish before it on the reversed clock, is raised by more than the sum of the lengths of all tasks so that they are placed first. The priority is added to the same longest-path value as in `run()` (added to `rank` if there is one). The backward schedules of P7-2 and P8-2 use the same mechanism
- A copying mistake in the hand calculation of `serve-within-backward` (P2-5, 2026-10-30): `ASM007` (the plating time is a placeholder) is attached only once, to the first plating in display order. Backward, the plating of the stew (line 22) comes first, but line 21 of the forward golden test had been copied. The expected value was fixed in a separate commit. The schedule lines matched the hand calculation
- Mutation check: S502, `cgolden/serve/serve-within-backward` (P2-5, 2026-10-30). Restoring the old code in `sched.c` that maps the release as is onto the reversed clock made the soup finish at 1680 s and report `SCH010`, and the test failed. It passed after the change was undone
- Mutation check: S503, `cgolden/serve/serve-within-backward` (P2-5, 2026-10-30). Making `SCH010` appear even when the wait does not exceed the limit made the test fail (5 forward tests such as `serve-within` failed too). It passed after the change was undone
- A miscount in S520 (P3-1, 2026-10-30): `ceil(4)` is 3 nodes, `Call`, `Name ceil` and `Number 4`, and the regrouping of D43 also makes `4 |> ceil` 3 nodes: `Number`, `Name` and a new `Call`. When the plan was written, only the regrouped `Call` was counted, and it said "one more". `ozen check --max-nodes` confirmed that `property batches = ceil(5 / 2)` passes at exactly 7 and stops at 6, and S520 was changed to "the same number". The way of breaking it in the coverage table (rewrite a Name into a Call and lower the count) is caught by `nodes`, which is one short, and an error that raises the count is caught by `nodes-exact`, which is exact
- `pipe/ast` and `pipe/ast-rules` (P3-1, 2026-10-30): `ozen debug ast --rules` prints only the list of rules, not the syntax tree and spans. A golden test runs only one command, so S518 (spans) went to `ast` and S516 (the rule `pipe`) to `ast-rules`. `cgolden/pipe/*` has 15 tests, counting `nodes-exact`
- Mutation check: S504, `cgolden/pipe/bare-name` (P3-1, 2026-10-30). Not adding the left side as an argument when the right side is a name left `wash()` without arguments, and `bare-name` failed (`chain`, `macro`, `precedence` and others with a name stage failed too). It passed after the change was undone
- Mutation check: S505, `cgolden/pipe/first-arg` (P3-1, 2026-10-30). Putting the left side in the last positional argument slot (before the named arguments) gave `cut(slice, peeled, …)`, and `first-arg` failed. It passed after the change was undone
- Mutation check: S506, `cgolden/pipe/chain` (P3-1, 2026-10-30). Making it right-associative, reading the right side together with the rest of the pipe before inserting the left side, made `chain` fail. It passed after the change was undone
- Mutation check: S507, `cgolden/pipe/precedence` (P3-1, 2026-10-30). Reading the pipe right after a unary expression, so that it binds tighter than `sum`, made only `precedence` fail. It passed after the change was undone
- Mutation check: S508, `cgolden/pipe/bad-target-quantity` (P3-1, 2026-10-30). Removing the check that the right side is a name and wrapping any expression in a call with the left side made `bad-target-quantity` fail (`bad-target-list` and `bad-target-make` failed too). It passed after the change was undone
- Mutation check: S509, `cgolden/pipe/bad-target-list` (P3-1, 2026-10-30). The same break as S508 made `bad-target-list` fail. It passed after the change was undone
- Mutation check: S510, `cgolden/pipe/bad-target-make` (P3-1, 2026-10-30). Allowing `make` at the start of the right side and wrapping it made only `bad-target-make` fail. It passed after the change was undone
- Mutation check: S511, `cgolden/pipe/macro` (P3-1, 2026-10-30). Skipping the first argument of a call made by a pipe in `eval_macro` made only `macro` fail. It passed after the change was undone
- Mutation check: S512, `cgolden/pipe/named-only` (P3-1, 2026-10-30). Putting the left side at the end of the arguments (after the named arguments) made `named-only` fail (`first-arg`, `chain` and others failed too). It passed after the change was undone
- Mutation check: S513, `cgolden/pipe/unattended` (P3-1, 2026-10-30). In `check_targets`, following a call made by a pipe back to the left side of the first stage (`rice`) for the check made only `unattended` fail. It passed after the change was undone
- Mutation check: S514, `cgolden/pipe/unattended-builtin` (P3-1, 2026-10-30). Not checking calls made by a pipe in `check_targets` made only `unattended-builtin` fail. It passed after the change was undone
- Mutation check: S516, `cgolden/pipe/ast-rules` (P3-1, 2026-10-30). Removing `mark(p, OZNI_R_pipe)` dropped `pipe` from the list of rules, and only `ast-rules` failed. It passed after the change was undone
- Mutation check: S517, `G-CORE-GRAMMAR` (P3-1, 2026-10-30). Removing the rule `pipe` from `grammar.ebnf` gave "131 rules, 0 not covered, 1 unknown" with the fixed check (see below), and the goal failed. It passed after the change was undone
- Mutation check: S518, `cgolden/pipe/ast` (P3-1, 2026-10-30). Making the span of the regrouped call start at the start of the right side made `ast` fail (the position of `MAC003` in `nodes` also changed, and it failed). It passed after the change was undone
- Mutation check: S519, `cunit/span_nesting` (P3-1, 2026-10-30). Making the span of the regrouped call end at the end of the left side made `span_nesting` fail with "span 103-107 of Name outside 87-91" and others. It passed after the change was undone
- Mutation check: S520, `cgolden/pipe/nodes` (P3-1, 2026-10-30). Rewriting the `Name` node of the right side into a `Call` without adding a node made `nodes` fail. It passed after the change was undone
- Mutation check: S521, `G-RF-FUZZ-DICT` (P3-1, 2026-10-30). Removing `"|>"` from the dictionary made it fail with "fuzz dict "|>": got '0'". It passed after the change was undone
- A gap in the grammar coverage check (P3-1, 2026-10-30): `check-grammar-coverage.py` treats a rule that `ozen debug ast --rules` printed but the grammar does not have as an "unknown rule" and exits with code 1, but the last line, which the goal reads, still said "0 not covered". So the break of S517 did not make `G-CORE-GRAMMAR` fail. It was fixed to add ", <n> unknown" to the last line only when there are rules not in the grammar, and the expectation of the self-test `unknown-rule` was changed first. When there are none, the line keeps the format of the roadmap
- How `G-RF-FUZZ-DICT` is handled in P3-1 (2026-10-30): this goal looks at 3 entries: `"|>"`, `"per"` and `"serving"`. P3-1 adds only `"|>"`, and the other 2 are added in P4-1, so until then this goal fails on those 2 checks. In P3-1 it was seen that the check of `"|>"` passes
- How S522 was broken (P3-2, 2026-10-30): the coverage table said "do not bind the default values of the parameters in a declaration without `()`", but a declaration without `()` has no parameters, so this break cannot happen. `recipe x()` and `recipe x` already gave the same syntax tree (`paren_params(p, n, false)`), and `make x` became the same as `make x()` in P2-3. So `omit-parens` passed even before the fix, and the reason was written in `scripts/red-first-exceptions.txt`. The break was changed to "make `()` required for `recipe` and `menu`"
- Mutation check: S522, `cgolden/syntax/omit-parens` (P3-2, 2026-10-30). Changing `paren_params(p, n, false)` to `true` to make `()` required turned the declarations without `()` into syntax errors, and `omit-parens` failed. It passed after the change was undone
- The number of empty `()` (P3-2, 2026-10-30): `check-test-names.py --empty-parens` found 434 in `tests` and 2 in `core/tests/unit` (the estimate of 454 lines in §3.2 counted lines with 2 on one line and lines of `.out` files; this is the number of matches in `.ozen`, `.sh` and `.c` files. `syntax/omit-parens` is an exception). The 11 in `renderers/test/inputs`, the 1 in `bindings/inputs` and the 4 in the `ozen` blocks of the README were removed in P3-2, and `recipe soup()` in the output of the bundling example in the README was fixed with them. `G-RF-NO-EMPTY-PARENS` fails until P5-7
- Mutation check: S525, `cgolden/quantity/per-serving` (P4-1, 2026-10-30). Removing the multiplication by the servings left `rice` at 75 g, and `per-serving` failed (the other golden tests that use `per serving` failed too). It passed after the change was undone
- Mutation check: S526, `cgolden/quantity/fixed-amount` (P4-1, 2026-10-30). Multiplying amounts without the marker by the servings too made `miso` 80 g, and `fixed-amount` failed (the existing golden tests with a `servings` parameter failed too). It passed after the change was undone
- Mutation check: S527, `cgolden/quantity/per-serving-param` (P4-1, 2026-10-30). Not looking at the parameter of the dish (`s->servings`) in `serving_count` gave `TYP020`, and `per-serving-param` failed. It passed after the change was undone
- Mutation check: S528, `cgolden/quantity/per-serving-options` (P4-1, 2026-10-30). Not looking at `servings` in the options in `serving_count` made only `per-serving-options` fail. It passed after the change was undone
- Mutation check: S529, `cgolden/quantity/servings-unknown` (P4-1, 2026-10-30). Using 1 serving when the servings are not known left out `TYP020`, and only `servings-unknown` failed. It passed after the change was undone
- Mutation check: S530, `cgolden/quantity/servings-unknown` (P4-1, 2026-10-30). Setting the severity of `TYP020` in the registry to `WARNING` made only `servings-unknown` fail. It passed after the change was undone
- Mutation check: S531, `cgolden/quantity/per-serving-misplaced` (P4-1, 2026-10-30). Skipping `per` and the next word after a default value in `default_of` left out `SYN003`, and only `per-serving-misplaced` failed. It passed after the change was undone
- Mutation check: S532, `cgolden/menus/make-servings-per-serving` (P4-1, 2026-10-30). Not using `servings` from the arguments of `make` as the servings of the dish in `eval_param` (going back to before P2-4) made `make-servings-per-serving` and `make-servings` fail. It passed after the change was undone
- Mutation check: S533, `cgolden/quantity/per-serving-json` (P4-1, 2026-10-30). Writing the amounts of `ingredients` in the JSON divided by the servings made only `per-serving-json` fail. It passed after the change was undone
- Mutation check: S534, `cgolden/quantity/per-serving-json` (P4-1, 2026-10-30). Adding one Count dimension to the multiplied amount made `per-serving-json` fail (the other golden tests that use `per serving` failed too). It passed after the change was undone
- Mutation check: S535, `G-STD-PHRASES` (P4-1, 2026-10-30). Removing `diag-servings-unknown` from `phrases/ja.ftl` made it fail with "1 missing in ja". It passed after the change was undone
- Mutation check: S536, `G-RF-FUZZ-DICT` (P4-1, 2026-10-30). Removing `"serving"` from the dictionary made it fail with "fuzz dict "serving": got '0'". It passed after the change was undone
- Mutation check: S614, `prop/bundle_roundtrip` (P4-1, 2026-10-30). Dropping `|>` when `ozen bundle` copies a declaration gave 1 differences with "tests/golden/bundle/new-syntax: unbundle failed", and it failed. It passed after the change was undone
- How `per serving` is built (P4-1, 2026-10-30): the syntax is `ingredient_item := IDENT ':' [ type_ref ] expr [ per_serving ] annotations` and `per_serving := 'per' 'serving'`. The amount expression is wrapped in a new node `PerServing` (the node kind was added at the end, so the numbers of the other kinds do not change). `TYP020` has no arguments, and its position is the whole `75 g per serving`. The servings come from `s->servings` (the parameter of the dish, or the value passed by `make`), or, if there is none, from `servings` in the options, with `pcs` dropped if it is attached (D45)
- Changes to expected values of existing golden tests (P4-1, 2026-10-30): `api/local-not-listed` prints the list of diagnostics, so one line for `TYP020` was added by hand. It was a separate commit before the implementation
- Red-first for `per-serving-misplaced` (P4-1, 2026-10-30): before the fix, `per` is an unexpected token anywhere, so the test passes even before the fix. The reason was written in `scripts/red-first-exceptions.txt`
- How S515 was broken (P5-0, 2026-10-30): the coverage table said "skip the space right after `|` and make one token up to `>`", but no golden test writes `| >`, so this break makes nothing fail. `G-RF-TYPEARGS-STABLE` is a goal that looks at the difference in expected values between two tags, so the break was changed to "point `rewrite-start` at a commit that changes one line of `typeargs/alternatives.out`". How `|` is read in types is itself checked by `typeargs/*` in `G-CORE-GOLDEN`, without changing the expected values
- Mutation check: S515, `G-RF-TYPEARGS-STABLE` (P5-0, 2026-10-30). In a temporary worktree, a commit that added a space at the end of line 1 of `typeargs/alternatives.out` was made, and pointing `rewrite-start` at it made the goal fail with "typeargs unchanged: exit 1". It passed after the tag was pointed back at the original commit
- Mutation check: S541, `G-RF-NAMES` (`unlisted exceptions`) (P5-0, 2026-10-30). Adding a line without a reason to `scripts/test-names-exceptions.txt` gave "1 unlisted exceptions". Removing it brought it back to 0 (while one-letter names remain, this goal also fails for other reasons)
- Tools of P5-0 (2026-10-30): for each point in time it compares, `check-rewrite-equivalence.py` runs `scripts/build-core.sh --only core-gcc-release` in a temporary worktree and keeps the executable at `build/equivalence/<コミット>/ozen`. The rewritten tests are the golden tests whose `.ozen`, `.out` or folder contents changed between the two points. What it compares is the exit code, the sequence of diagnostic codes, the number of `result` lines and the sequence of step names. For the records, for each row of roadmap §5.3 that points to a rewritten test (`cgolden/`, `cli/`, `cunit/`) and has a "How to break it" entry, it looks for a `- Rewrite mutation check: ` line that contains the S item, the aspect and the test name. A `cunit/` name counts as rewritten if a changed C file of the unit tests contains that name. The tag `rewrite-start` was placed after the commit of the P5-0 tools. One-letter names numbered 705 at the start (out of 459 declarations and 709 bindings)
- Mutation check: S538, Normal, `G-RF-EQUIVALENCE` (P5-1, 2026-10-30). In a temporary worktree, a commit was made that removed the last error (`let seasoned = *`) of the rewritten `syntax/recover-multiple` and matched the expected values, and passing it to `--head` made the goal fail with "1 changed results" (`SYN003` went from 3 to 2). It passed with 0 changed results after going back to HEAD
- Rewrite mutation check: S156, Normal, `cgolden/syntax/recover-multiple` (P5-1, 2026-10-30). Leaving the block without synchronizing after a syntax error made `recover-multiple` fail (`reserved-word`, `diag/code-and-name` and others failed too). It passed after the change was undone
- Rewrite mutation check: S410, No-effect condition, `cgolden/syntax/utf8-invalid` (P5-1, 2026-10-30). Removing the UTF-8 check in `resolve.c` made only `utf8-invalid` fail. It passed after the change was undone
- Rewrite mutation check: S411, No-effect condition, `cgolden/syntax/ident-nonascii` (P5-1, 2026-10-30). Not reporting the diagnostic for non-ASCII identifiers in the lexer made only `ident-nonascii` fail. It passed after the change was undone
- Rewrite mutation check: S412, Normal, `cgolden/syntax/multiline-string` (P5-1, 2026-10-30). Stopping the reading of triple-quoted strings at a newline made `multiline-string` and `decl/phrases` fail. It passed after the change was undone
- Rewrite mutation check: S415, No-effect condition, `cgolden/syntax/annotation-target` (P5-1, 2026-10-30). Not reporting `TYP017` in `check_targets` made `annotation-target` and `pipe/unattended-builtin` fail. It passed after the change was undone
- How the expected values of the rewrite were found (P5-1, 2026-10-30): after the rewritten source was written, the new expected values were made by applying the name mapping table to the old expected values (a helper tool in the scratchpad; it does not run `ozen`). The position of a diagnostic was found by counting which occurrence the string at that position in the old source was, and looking for the same occurrence of the renamed string in the new source. Cases where the number of occurrences changed, and the spans of the syntax tree, were counted by hand. Lines whose amounts or times changed were calculated by hand. Then the golden tests were run, and each mismatch was traced to its cause one by one (in `syntax`, 1 escaping error in a hand-written replacement string)
- Rewrite mutation check: S158, Normal, `cgolden/decl/*` (P5-1, 2026-10-30). Skipping `rule` declarations in the parser did not make the rewritten `decl/menu` fail, but `decl/rule` failed (the 23 other tests that use `rule` failed too). In `decl`, only the empty `()` of `menu.ozen` was rewritten, and `decl/menu` changes neither its sequence of step names nor its results. It passed after the change was undone
- Rewrite mutation check: S161, No-effect condition, `cgolden/names/case-insensitive-free` (P5-1, 2026-10-30). Not looking at action definitions when looking up names that start with a capital letter made only `case-insensitive-free` fail. It passed after the change was undone
- Rewrite mutation check: S162, Normal, `cgolden/modules/name-from-path` (P5-1, 2026-10-30). Not putting the package name in front of module names made `name-from-path` fail (the names of the std modules changed too, and 329 tests failed). It passed after the change was undone
- Rewrite mutation check: S164, Normal, `cgolden/modules/import-forms` (P5-1, 2026-10-30). Not binding the alias of `import … as` made only `import-forms` fail. It passed after the change was undone
- Rewrite mutation check: S165, Normal, `cgolden/modules/prelude-implicit` (P5-1, 2026-10-30). Not importing the std prelude made `prelude-implicit` fail (329 tests failed). It passed after the change was undone
- Rewrite mutation check: S166, Boundary, `cgolden/modules/ambiguous-on-use` (P5-1, 2026-10-30). Reporting `NAM002` as soon as the same name is imported from 2 modules made `ambiguous-on-use` (which then had 2 errors) and `ambiguous-unused-ok` (`cgolden/modules/ambiguous-unused-ok`) fail. It passed after the change was undone
- Rewrite mutation check: S169, No-effect condition, `cgolden/modules/recipe-imports-menu` (P5-1, 2026-10-30). The row's break (remove cycle detection) made only `cycle` fail, not the rewritten `recipe-imports-menu`. Adding a break that stops `PKG002` from being reported made only `recipe-imports-menu` fail. Both passed after the changes were undone
- Rewrite mutation check: S170, No-effect condition, `cgolden/modules/recipe-names-kitchen` (P5-1, 2026-10-30). Not reporting `NAM008` when a dish refers to a kitchen made only `recipe-names-kitchen` fail. It passed after the change was undone
- Rewrite mutation check: S171, Normal, `cgolden/modules/forward-reference` (P5-1, 2026-10-30). Not finding definitions declared later in the same file made `forward-reference` fail (forward references in std broke too, and 329 tests failed). It passed after the change was undone
- Rewrite mutation check: S172, Normal, `cgolden/modules/entry-*` (P5-1, 2026-10-30). Not making a dish a candidate entry point when there is no menu made `entry-recipe-only` and 5 bundling golden tests fail. It passed after the change was undone
- Rewrite mutation check: S176, Boundary, `cgolden/packages/requires-ok` (P5-1, 2026-10-30). The row's break (do not compare minor versions, in 2 places: std and dependencies) made `requires-newer-minor` and others fail, but not `requires-ok`, the passing side. Adding a break that treats even the same version as asking for a newer one made `requires-ok` fail. Both passed after the changes were undone
- Rewrite mutation check: S178, No-effect condition, `cgolden/packages/dep-diag-suppressed` (P5-1, 2026-10-30). The row's break (also suppress `SAF` of dependencies) made only `dep-saf-shown` (`cgolden/packages/dep-saf-shown`) fail. Adding a break that does not suppress diagnostics of dependencies made only `dep-diag-suppressed` fail. Both passed after the changes were undone
- Rewrite mutation check: S357, Normal, `cgolden/std/package` (P5-1, 2026-10-30). Changing the package name in `stdlib/package.ozen` to `stdx` made `std/package` fail (329 tests failed). It passed after the change was undone
- Rewrite mutation check: S361, Normal, `cgolden/std/prelude-contents` (P5-1, 2026-10-30). Removing `unit g` from the prelude made `prelude-contents` fail (198 tests failed). It passed after the change was undone
- Rewrite mutation check: S413, No-effect condition, `cgolden/names/let-rebind` (P5-1, 2026-10-30). Letting a binding of the same name through without `NAM003` made only `let-rebind` fail. It passed after the change was undone
- Rewrite mutation check: S414, Boundary, `cgolden/names/arity-*` (P5-1, 2026-10-30). Not counting duplicated named arguments and unknown names made `arity-duplicate` and `arity-unknown` fail (`arity-missing` is about missing positional arguments, so it did not fail). It passed after the change was undone
- Rewrite mutation check: S416, Normal, `cgolden/names/ingredient-type-omitted` (P5-1, 2026-10-30). Making the type of an ingredient with its type left out `Unknown` made `ingredient-type-omitted` fail (245 tests failed). It passed after the change was undone
- Rewrite mutation check: S428, Normal, `cgolden/packages/deprecated-warning` (P5-1, 2026-10-30). Not reporting `PKG012` for a deprecated unit made `deprecated-warning` and `bundle/annotation-scope` fail. It passed after the change was undone
- Range of P5-1 (2026-10-30): 17 tests in `syntax`, 1 in `decl` (only the empty `()` of `menu`), 6 in `names`, 11 in `modules`, 4 in `packages` and 3 in `std` were rewritten. The golden tests of `typeargs` and of the other declarations in `decl` already declared real dish names and had no one-letter names or empty `()`, so they were not changed. The same goes for the golden tests of `modules` and `packages` that have no dishes (cycles, imports, versions). For the 42 rewritten tests, `check-rewrite-equivalence.py` found the exit code, diagnostics, number of results and sequence of step names all the same as before the rewrite
- Mutation check: S539, Data completeness, `G-RF-EQUIVALENCE` (P5-1, 2026-10-30). Removing one rewrite record line for `S413` (`names/let-rebind`) from §9 made it fail with "records: 23 roadmap rows, 1 missing". It passed with 0 missing after the change was undone
- Mutation check: S540, Data completeness, `G-RF-NAMES` (P5-1, 2026-10-30). Changing the dish name in `names/let-rebind.ozen` back to `recipe r` gave "1 one-letter" in the 7 folders of P5-1. Undoing it brought it back to 0 one-letter
- Mutation check: S546, Fixed premise, `G-CORE-GOLDEN` (P5-1, 2026-10-30). Shifting a position in the rewritten `names/let-rebind.out` by one column gave "397 passed, 1 failed" in all 4 configurations, and it failed. It passed after the change was undone
- Rewrite mutation check: S155, Normal, `cgolden/macro/trail-nested` (P5-2, 2026-10-30). Not attaching the expansion history in `diag` made `trail-nested`, `splice-not-name` and 2 API tests fail. It passed after the change was undone
- Rewrite mutation check: S180, Boundary, `cgolden/limits/*` (P5-2, 2026-10-30). Changing the comparisons for the limits on nodes, depth, steps and input size to `>=` made the 4 "exact" tests in `limits` and the "exact" tests for fuel and bundling fail. It passed after the change was undone
- Rewrite mutation check: S186, No-effect condition, `cgolden/dim/add-mismatch` (P5-2, 2026-10-30). Removing the dimension check of addition and subtraction made `add-mismatch`, `macro/trail-nested` and others fail. It passed after the change was undone
- Rewrite mutation check: S187, Boundary, `cgolden/dim/density-missing` (P5-2, 2026-10-30). Treating volume as mass at 1 g/ml when there is no density made `density-missing` fail. The passing side, `density-convert` (`cgolden/dim/density-convert`), failed with an added break that doubles the density of water. Both passed after the changes were undone
- Rewrite mutation check: S188, Boundary, `cgolden/dim/any-amount-strict` (P5-2, 2026-10-30). Ignoring `--strict` made `any-amount-strict` and `strict-group-extended` fail. Adding a break that is strict even without `--strict` made `any-amount-warn` (`cgolden/dim/any-amount-warn`) fail. Both passed after the changes were undone
- Rewrite mutation check: S189, No-effect condition, `cgolden/dim/pinch-strict-ok` (P5-2, 2026-10-30). Treating ranged amounts as vague amounts and reporting `TYP003` made `pinch-strict-ok` and `std/prelude-contents` fail. It passed after the change was undone
- Rewrite mutation check: S190, Normal, `cgolden/dim/deny-strict-annotation` (P5-2, 2026-10-30). Ignoring the severity annotation on a declaration made `deny-strict-annotation`, the annotation golden tests in `diag` and others fail. It passed after the change was undone
- Rewrite mutation check: S191, Normal, `cgolden/dim/strict-group-extended` (P5-2, 2026-10-30). Not letting `diagnostics(…)` in the author's source extend a group made only `strict-group-extended` fail. It passed after the change was undone
- Rewrite mutation check: S192, No-effect condition, `cgolden/dim/asm-strict-info` (P5-2, 2026-10-30). Raising `ASM` to errors too under `--strict` made only `asm-strict-info` fail. It passed after the change was undone
- Rewrite mutation check: S193, Normal, `cgolden/dim/rational-quarter` (P5-2, 2026-10-30). Rounding amounts that do not divide evenly to integers for display made `rational-quarter` and `ir/rational-amounts` fail. `count-unit-weight` (`cgolden/dim/count-unit-weight`) failed with an added break that changes the lower bound of the weight of one onion. Both passed after the changes were undone
- Rewrite mutation check: S195, Normal, `cgolden/dim/cup-us` (P5-2, 2026-10-30). Making the US cup 200 ml made `cup-us` fail. Adding a break that makes the Japanese cup 180 ml made `cup-jp` (`cgolden/dim/cup-jp`) fail. Both passed after the changes were undone
- Rewrite mutation check: S253, No-effect condition, `cgolden/eval/runtime-value-in-if` (P5-2, 2026-10-30). Removing the type check of the condition of `if` made only `runtime-value-in-if` fail. It passed after the change was undone
- Rewrite mutation check: S254, Normal, `cgolden/macro/hygiene-*` (P5-2, 2026-10-30). Evaluating the body of a macro in the caller's scope made `hygiene-splice` and `hygiene-tmp` fail. The other 3 each failed with added breaks: not binding to a spliced name (`hygiene-bind`), evaluating a splice only once (`hygiene-args`), and evaluating in the caller's scope while also looking at locals first for the called name (`hygiene-free-name`). All passed after the changes were undone
- Rewrite mutation check: S255, Normal, `cgolden/macro/definition-checks` (P5-2, 2026-10-30). Reporting `+` as a dimension error at the point of the macro definition made `definition-checks`, `trail-nested` and others fail. It passed after the change was undone
- Rewrite mutation check: S256, Normal, `cgolden/macro/typecheck-after-expand` (P5-2, 2026-10-30). Reporting the position of an error in a splice in the macro body instead of mapping it to the caller made only `typecheck-after-expand` fail. It passed after the change was undone
- Rewrite mutation check: S257, Normal, `cgolden/macro/trail-nested` (P5-2, 2026-10-30). Listing the expansion history from the outside made only `trail-nested` fail. It passed after the change was undone
- Rewrite mutation check: S258, Boundary, `cgolden/eval/fuel-*` (P5-2, 2026-10-30). Not counting evaluation steps made `fuel-short` and `kitchen/check-fuel` fail. Adding a break that counts steps by 2 made `fuel-exact` fail. Both passed after the changes were undone
- Rewrite mutation check: S259, No-effect condition, `cgolden/macro/no-macro-defining-macro` (P5-2, 2026-10-30). Accepting nested `quote` made `no-macro-defining-macro` and `syntax/quote-placement` fail. Adding a break that does not check free names in the macro body made `no-capture` (`cgolden/macro/no-capture`) fail. Both passed after the changes were undone
- Rewrite mutation check: S260, Normal, `cgolden/diag/code-and-name` (P5-2, 2026-10-30). Not printing the name on diagnostic lines made 208 tests, including `code-and-name`, fail. It passed after the change was undone
- Rewrite mutation check: S261, Normal, `cgolden/diag/severity-override` (P5-2, 2026-10-30). Ignoring severity settings given at build time made `severity-override`, `precedence` and `select-by-name-prefix-group` fail. It passed after the change was undone
- Rewrite mutation check: S262, Normal, `cgolden/diag/select-by-name-prefix-group` (P5-2, 2026-10-30). Ignoring settings by category prefix (`LIN`) made only `select-by-name-prefix-group` fail. It passed after the change was undone
- Rewrite mutation check: S263, Boundary, `cgolden/diag/annotation-scope` (P5-2, 2026-10-30). Not looking at the end of an annotation's scope made `annotation-scope`, `nested-annotations` and others fail. It passed after the change was undone
- Rewrite mutation check: S264, Normal, `cgolden/diag/expect-unfulfilled` (P5-2, 2026-10-30). Not reporting `NAM018` when `@expect` is not fulfilled made only `expect-unfulfilled` fail. Adding a break that does not suppress with `@expect` made `expect-suppresses` (`cgolden/diag/expect-suppresses`) fail. Both passed after the changes were undone
- Rewrite mutation check: S265, Time and order, `cgolden/diag/precedence` (P5-2, 2026-10-30). Ignoring annotations when there is a build-time setting made only `precedence` fail, and giving priority to the outer annotation made only `nested-annotations` (`cgolden/diag/nested-annotations`) fail. Both passed after the changes were undone
- Rewrite mutation check: S267, Normal, `cgolden/diag/fix-suggestion` (P5-2, 2026-10-30). Not offering fix suggestions made only `fix-suggestion` fail. It passed after the change was undone
- Rewrite mutation check: S408, Boundary, `cgolden/limits-shared/*` (P5-2, 2026-10-30). Not applying the input size limit to kitchen arguments made only `kitchen-over` fail. Adding a break that does not apply the limit to the first file (the bundled file) made `bundle-over` and `limits/input-over` fail. The 2 "exact" tests failed with the break of S180. All passed after the changes were undone
- Range of P5-2 (2026-10-30): 14 tests in `dim`, 3 in `eval`, 11 in `macro`, 8 in `limits`, 4 in `limits-shared` and 12 in `diag` were rewritten. For the limit golden tests the input size was counted again (54 for `limits/input-*`, 212 for `limits-shared/bundle-*`; counted by hand, and the "exact" cases confirmed with `check-limit-pairs.py`). Steps, nodes and depth do not change when names change, so the limit values stayed the same. The position of the allocation failure in `diag/out-of-memory` (the 3rd) does not change with the dish names either
- Mutation check: S543, Boundary, `G-RF-LIMIT-PAIRS` (P5-2, 2026-10-30). Changing the input of `limits/nodes-exact` to `property cups = 1` to lower the number of nodes gave "--max-nodes 4 gives no MAC003" and "8 pairs, 1 not exact", and it failed. It passed with 0 not exact after the change was undone
- Mutation check: S548, Fixed premise, `G-CORE-OOM` (P5-2, 2026-10-30). Not reporting `MAC004` when an allocation fails in `check.c` made it fail with "363 files, 4865 points, 3014 failures". It passed after the change was undone
- How S548 was broken (P5-2, 2026-10-30): the coverage table said "add 1 to the allocation number of `diag/out-of-memory`", but an allocation failure gives the same `MAC004` (position 1:1) at any number, and with `--fail-alloc-at 4` both the golden test and `G-CORE-OOM` passed. A break that misses an allocation failure in `bind` in `sema.c` did not fail either, because `check.c` counts arena failures separately. `G-CORE-OOM` checks that `MAC004` is reported for every allocation failure, so the break was changed to "do not report `MAC004` when an allocation fails"
- Rewrite mutation check: S474, Normal, `cgolden/state/blend-*` (P5-3, 2026-10-30). Making the output of std's `blend` `cut(slice, 3 mm)` made `blend-single` and `blend-after-pureed` fail. `blend-list` does not have the shape of a blended value, so it did not fail; it failed with the break of S212 (remove the union of traits of the blended items). It passed after the change was undone
- Rewrite mutation check: S196, No-effect condition, `cgolden/state/peel-missing` (P5-3, 2026-10-30). Removing the check of the state an argument requires made 10 tests, including `peel-missing`, fail. It passed after the change was undone
- Rewrite mutation check: S197, Normal, `cgolden/state/boil-then-peel` (P5-3, 2026-10-30). Clearing the values of the other axes every time an effect is applied, making a single chain of transitions, made 67 tests, including `boil-then-peel` and `grill-then-cut` (`cgolden/state/grill-then-cut`), fail. It passed after the change was undone
- Rewrite mutation check: S198, Boundary, `cgolden/state/ordered-no-regress` (P5-3, 2026-10-30). Always assigning the value of an ordered axis made only `ordered-no-regress` fail. `ordered-ge` (`cgolden/state/ordered-ge`) failed with an added break that reverses the `>=` comparison. It passed after the changes were undone
- Rewrite mutation check: S199, Boundary, `cgolden/state/cut-finer` (P5-3, 2026-10-30). Following the fineness relation in reverse made `cut-finer` and `cut-coarser` (`cgolden/state/cut-coarser`) fail. It passed after the change was undone
- Rewrite mutation check: S200, Normal, `cgolden/state/prep-set-args` (P5-3, 2026-10-30). Replacing the contents when adding to a set axis made 13 tests, including `prep-set-args`, fail. It passed after the change was undone
- Rewrite mutation check: S201, Normal, `cgolden/state/user-axis` (P5-3, 2026-10-30). Not checking requirements on axes defined in the author's source made only `user-axis` fail. It passed after the change was undone
- Rewrite mutation check: S202, Normal, `cgolden/state/omit-axis` (P5-3, 2026-10-30). Making an effect on an axis the food does not have `TYP006` made 10 tests, including `omit-axis`, fail. It passed after the change was undone
- Rewrite mutation check: S203, Normal, `cgolden/state/doneness-browning-independent` (P5-3, 2026-10-30). Making the browning effect also set the doneness to `done` made only `doneness-browning-independent` fail. It passed after the change was undone
- Rewrite mutation check: S204, Normal, `cgolden/state/unwritten-axis-inherit` (P5-3, 2026-10-30). Emptying set axes that were not written every time an effect is applied made `unwritten-axis-inherit` and 1 other test fail. It passed after the change was undone
- Rewrite mutation check: S206, No-effect condition, `cgolden/safety/undercooked-pork` (P5-3, 2026-10-30). Removing the check of `SAF001` made 8 tests, including `undercooked-pork`, fail. It passed after the change was undone
- Rewrite mutation check: S207, No-effect condition, `cgolden/safety/raw-edible-exception` (P5-3, 2026-10-30). Removing the exception for things that can be eaten raw made 6 tests, including `raw-edible-exception`, fail. It passed after the change was undone
- Rewrite mutation check: S208, No-effect condition, `cgolden/safety/allow-saf-stays-error` (P5-3, 2026-10-30). Letting `@allow` and build-time settings lower `SAF` made `allow-saf-stays-error` and `cli-allow-saf` (`cgolden/safety/cli-allow-saf`) fail. It passed after the change was undone
- Rewrite mutation check: S209, Real path, `cgolden/safety/allergen-bowl` (P5-3, 2026-10-30). Not recording allergen contamination on tools made only `allergen-bowl` fail. It passed after the change was undone
- Rewrite mutation check: S210, Normal, `cgolden/mix/new-value` (P5-3, 2026-10-30). Keeping the type of a mixture as the type of the first ingredient made 20 tests, including `new-value`, fail. It passed after the change was undone
- Rewrite mutation check: S211, Normal, `cgolden/mix/amount-sum` (P5-3, 2026-10-30). Not adding up the amounts of a mixture made 20 tests, including `amount-sum`, fail. It passed after the change was undone
- Rewrite mutation check: S212, Normal, `cgolden/mix/mixture-hazard-allergen` (P5-3, 2026-10-30). Not passing the traits of the ingredients on to a mixture made 11 tests, including `mixture-hazard-allergen` and `state/blend-list`, fail. It passed after the change was undone
- Rewrite mutation check: S213, No-effect condition, `cgolden/linear/double-use` (P5-3, 2026-10-30). Removing the record of consumption made 21 tests, including `double-use`, fail. It passed after the change was undone
- Rewrite mutation check: S214, Boundary, `cgolden/linear/exact-amount` (P5-3, 2026-10-30). Changing the amount comparison to `>=` made only `exact-amount` fail. `shortage` (`cgolden/linear/shortage`) failed with an added break that does not report a shortage. It passed after the changes were undone
- Rewrite mutation check: S215, Normal, `cgolden/linear/leftover-part` (P5-3, 2026-10-30). Not reporting `LIN003` for leftover parts made 10 tests, including `leftover-part`, fail. It passed after the change was undone
- Rewrite mutation check: S216, No-effect condition, `cgolden/linear/store-discard-silent` (P5-3, 2026-10-30). Reporting consumed parts as leftovers too made 18 tests, including `store-discard-silent`, fail. It passed after the change was undone
- Rewrite mutation check: S217, No-effect condition, `cgolden/linear/parallel-double-use` (P5-3, 2026-10-30). Not distinguishing consumption across branches made only `parallel-double-use` fail. It passed after the change was undone
- Rewrite mutation check: S218, Normal, `cgolden/parts/inherit-trait` (P5-3, 2026-10-30). Not following `extends` of parts made only `inherit-trait` fail. It passed after the change was undone
- Rewrite mutation check: S219, Boundary, `cgolden/parts/alias-one-not-fatty` (P5-3, 2026-10-30). Judging the candidates of an alias by the first one only made only `alias-one-not-fatty` fail. `alias-all-fatty` (`cgolden/parts/alias-all-fatty`), `alias-hazard-any` (`cgolden/parts/alias-hazard-any`) and `alias-widest-range` (`cgolden/parts/alias-widest-range`) each failed with an added break: treating the candidates as having no traits other than Hazard, requiring Hazard of all candidates too, and limiting the range of properties to the first candidate. It passed after the changes were undone
- Rewrite mutation check: S220, Normal, `cgolden/parts/alias-name-in-diag` (P5-3, 2026-10-30). Showing the type name with the alias expanded in diagnostics made `alias-name-in-diag` and 1 other test fail. It passed after the change was undone
- Rewrite mutation check: S221, Normal, `cgolden/parts/aibiki-contains` (P5-3, 2026-10-30). Making the ratio of `contains` a single point at the lower bound made only `aibiki-contains` fail. It passed after the change was undone
- Rewrite mutation check: S222, Normal, `cgolden/parts/fillet-yield` (P5-3, 2026-10-30). Not keeping the amount of a part found from the yield made 4 tests, including `fillet-yield`, fail. It passed after the change was undone
- Rewrite mutation check: S224, Normal, `cgolden/actions/uses-capability` (P5-3, 2026-10-30). Matching kitchen kinds only by name instead of capability made 49 tests, including `uses-capability`, fail. It passed after the change was undone
- Rewrite mutation check: S225, Boundary, `cgolden/actions/capacity-from-amount` (P5-3, 2026-10-30). Ignoring the number in a multiplication of an amount by a number (not multiplying by the factor 1.2) made 6 tests, including `capacity-from-amount`, fail. It passed after the change was undone
- Rewrite mutation check: S227, Time and order, `cgolden/actions/extend-order` (P5-3, 2026-10-30). Placing variants added by `extend` before the original variants made `extend-order` and 1 other test fail. It passed after the change was undone
- Rewrite mutation check: S228, Normal, `cgolden/actions/requires-auto` (P5-3, 2026-10-30). Not collecting requirements from the `uses` of actions made 3 tests, including `requires-auto`, fail. It passed after the change was undone
- Rewrite mutation check: S417, No-effect condition, `cgolden/parts/part-on-partless` (P5-3, 2026-10-30). Not reporting `TYP009` when a part is given to a food without parts made only `part-on-partless` fail. It passed after the change was undone
- Rewrite mutation check: S418, No-effect condition, `cgolden/actions/where-constraint` (P5-3, 2026-10-30). Removing the check of `where` made only `where-constraint` fail. It passed after the change was undone
- Rewrite mutation check: S419, Normal, `cgolden/parts/trait-defined-parts` (P5-3, 2026-10-30). Not inheriting parts defined by a trait made 4 tests, including `trait-defined-parts`, fail. It passed after the change was undone
- Rewrite mutation check: S420, Normal, `cgolden/parts/auto-discard` (P5-3, 2026-10-30). Not marking parts to throw away (guts, scales) made them reported as leftovers, and `auto-discard` and 1 other test failed. It passed after the change was undone
- Rewrite mutation check: S424, Normal, `cgolden/actions/extend-in-assumptions` (P5-3, 2026-10-30). Removing variants added by `extend` from the candidate list of `ASM006` made only `extend-in-assumptions` fail. It passed after the change was undone
- Rewrite mutation check: S434, Normal, `cgolden/actions/composite-*` (P5-3, 2026-10-30). Not expanding the `steps` of a composite action made `composite-expand`, `composite-schedule` and `composite-types` fail. It passed after the change was undone
- Range of P5-3 (2026-10-30): 15 tests in `state`, 4 in `mix`, 11 in `parts`, 6 in `linear`, 7 in `safety` and 9 in `actions` were rewritten. Actions applied in a row were turned into pipes (8 tests in `state`, plus `actions/requires-auto` and `uses-capability`). Only tests where no diagnostic position falls on the span of a call were turned into pipes. `state/axis-*`, `parts/part-trait-clash`, `actions/error-in-unused-variant`, `no-overload` and `safety/plate-raw-buy` have no dish names or one-letter names, so they were not changed
- Rewrite mutation check: S231, Boundary, `cgolden/kitchen/recipe-arg-range` (P5-4, 2026-10-30). The row's breaks (exclude the ends of a range, ignore open ranges) do not change the result of the rewritten `recipe-arg-range` (30 for the closed range `1..20`). With a break that removes the range check, 4 tests, including `recipe-arg-range`, failed. It passed after the change was undone
- Rewrite mutation check: S234, Normal, `cgolden/kitchen/variant-filtered` (P5-4, 2026-10-30). Keeping variants that the kitchen cannot use among the candidates made 3 tests, including `variant-filtered` and `no-variant-left` (`cgolden/kitchen/no-variant-left`), fail. It passed after the change was undone
- Rewrite mutation check: S235, Normal, `cgolden/kitchen/notation-defaults` (P5-4, 2026-10-30). Making the default of `count` in std's `EquipmentSpec` 0 made 55 tests, including `notation-defaults`, fail (a break in `req_met` that treats a missing `count` as 0 did not fail, because the default value of the type takes effect first). It passed after the change was undone
- Rewrite mutation check: S236, Normal, `cgolden/kitchen/capacity-compile-time` (P5-4, 2026-10-30). Treating expressions that read kitchen values as cooking-time values (unknown) made 3 tests, including `capacity-compile-time`, fail. It passed after the change was undone
- Rewrite mutation check: S238, No-effect condition, `cgolden/vessels/soup-flat-plate` (P5-4, 2026-10-30). Not reporting `TYP014` for plating a liquid on a flat plate made only `soup-flat-plate` fail. It passed after the change was undone
- Rewrite mutation check: S239, Normal, `cgolden/vessels/fallback-order` (P5-4, 2026-10-30). Reversing the fallback order of vessels made only `fallback-order` fail. It passed after the change was undone
- Rewrite mutation check: S240, Boundary, `cgolden/vessels/count-exact` (P5-4, 2026-10-30). Counting one more vessel than needed made 8 tests, including `count-exact` and `count-short` (`cgolden/vessels/count-short`), fail. It passed after the change was undone
- Rewrite mutation check: S241, Boundary, `cgolden/vessels/capacity-over-warn` (P5-4, 2026-10-30). Not reporting `SCH001` for vessel capacity made only `capacity-over-warn` fail. It passed after the change was undone
- Rewrite mutation check: S242, Normal, `cgolden/menus/output-type-default` (P5-4, 2026-10-30). Keeping the output type of `make` as the type of the last value made 13 tests, including `output-type-default` and `output-type-explicit` (`cgolden/menus/output-type-explicit`), fail. It passed after the change was undone
- Rewrite mutation check: S243, Normal, `cgolden/menus/make-buy-swap` (P5-4, 2026-10-30). Giving the value of `buy` a different type made 7 tests, including `make-buy-swap`, fail. It passed after the change was undone
- Rewrite mutation check: S244, Normal, `cgolden/menus/output-properties` (P5-4, 2026-10-30). Not passing the traits of the dish (including allergens) on to the output of `make` made `output-properties` and 1 other test fail (a break that removes `made_from` did not fail, because the displayed allergens come from the last value). It passed after the change was undone
- Rewrite mutation check: S245, Normal, `cgolden/menus/choose-breakdown` (P5-4, 2026-10-30). Ignoring the per-person breakdown of choices made 4 tests, including `choose-breakdown`, fail. `choose-default-even` (`cgolden/menus/choose-default-even`) failed with an added break that does not report `ASM001` when splitting evenly. It passed after the changes were undone
- Rewrite mutation check: S246, Time and order, `cgolden/menus/assemble-order` (P5-4, 2026-10-30). Evaluating the contents of `assemble` in reverse order made only `assemble-order` fail. It passed after the change was undone
- Rewrite mutation check: S247, Normal, `cgolden/menus/split-pass` (P5-4, 2026-10-30). Not consuming the original value in `split` made only `split-pass` fail. It passed after the change was undone
- Rewrite mutation check: S248, Normal, `cgolden/menus/single-recipe-wrap` (P5-4, 2026-10-30). Not adding plating to the entry dish made 9 tests, including `single-recipe-wrap`, fail. `no-vessel-until-done` (`cgolden/menus/no-vessel-until-done`) failed with an added break that does not report `ASM002` when there is no vessel. It passed after the changes were undone
- Rewrite mutation check: S249, Normal, `cgolden/menus/recipe-arg-external` (P5-4, 2026-10-30). Not reporting `ASM003` for ingredients passed as dish arguments made only `recipe-arg-external` fail. It passed after the change was undone
- Rewrite mutation check: S250, Normal, `cgolden/menus/waves` (P5-4, 2026-10-30). The row's break (make the interval of the second wave of the schedule 0) did not make `waves`, which prints only the steps, fail, but made 4 tests that print the schedule fail. Adding a break that does not show the interval in the step display made `waves` fail. It passed after the changes were undone
- Rewrite mutation check: S251, No-effect condition, `cgolden/menus/serve-time-in-code` (P5-4, 2026-10-30). Accepting `at` and a time after `serve` in the syntax made only `serve-time-in-code` fail. It passed after the change was undone
- Rewrite mutation check: S252, No-effect condition, `cgolden/menus/while-warm-strict-ok` (P5-4, 2026-10-30). Treating property annotations such as `@whileWarm` as vague amounts (`TYP003`) made them errors under `--strict`, and 4 tests, including `while-warm-strict-ok`, failed. It passed after the change was undone
- Rewrite mutation check: S270, Normal, `cgolden/levels/level-0` (P5-4, 2026-10-30). Always using level 2 made 61 tests, including `level-0` and `level-1` (`cgolden/levels/level-1`), fail. `level-2` (`cgolden/levels/level-2`) is already level 2, so it did not fail. It passed after the change was undone
- Rewrite mutation check: S271, Side effects, `cgolden/levels/check-stops` (P5-4, 2026-10-30). Building and printing the schedule in `ozen check` too made 105 tests, including `check-stops`, fail. It passed after the change was undone
- Rewrite mutation check: S272, Normal, `cgolden/levels/assumptions-as-info` (P5-4, 2026-10-30). Setting the severity of `ASM002` in the registry to warning made 13 tests, including `assumptions-as-info`, fail. It passed after the change was undone
- Rewrite mutation check: S274, Normal, `cgolden/levels/critical-path` (P5-4, 2026-10-30). Counting the shortest time as the sum of the lengths of all tasks made only `critical-path` fail. It passed after the change was undone
- Rewrite mutation check: S275, Normal, `cgolden/levels/first-variant-assumed` (P5-4, 2026-10-30). Assuming the last variant instead of the first made 5 tests, including `first-variant-assumed`, fail. It passed after the change was undone
- Rewrite mutation check: S276, Normal, `cgolden/levels/small-tools-assumed` (P5-4, 2026-10-30). Not reporting `ASM008` for small tools made only `small-tools-assumed` fail. It passed after the change was undone
- Rewrite mutation check: S277, Normal, `cgolden/levels/provisional-assumed` (P5-4, 2026-10-30). Not reporting `ASM007` for provisional values made 27 tests, including `provisional-assumed`, fail. It passed after the change was undone
- Rewrite mutation check: S280, Normal, `cgolden/ir/variants-as-choice` (P5-4, 2026-10-30). Putting only the variants the kitchen can use into the choice of the IR made `variants-as-choice` and 1 other test fail. It passed after the change was undone
- Rewrite mutation check: S282, Normal, `cgolden/ir/servings-changes-ir` (P5-4, 2026-10-30). Not overriding dish arguments with values from the options made 4 tests, including `servings-changes-ir`, fail. It passed after the change was undone
- Rewrite mutation check: S283, Normal, `cgolden/ir/waves-in-ir` (P5-4, 2026-10-30). Removing the wave lines from the IR display made 4 tests, including `waves-in-ir`, fail. It passed after the change was undone
- Rewrite mutation check: S285, Normal, `cgolden/ir/text-dump-format` (P5-4, 2026-10-30). Removing the names of the resource fields from the IR display made 16 tests, including `text-dump-format`, fail. It passed after the change was undone
- Rewrite mutation check: S286, Normal, `cgolden/ir/rational-amounts` (P5-4, 2026-10-30). Writing amounts that do not divide evenly rounded to integers made `rational-amounts` and `dim/rational-quarter` fail. It passed after the change was undone
- Rewrite mutation check: S318, Boundary, `cgolden/serve/serve-within` (P5-4, 2026-10-30). Not putting the `@serveWithin` constraint into the schedule made 5 tests, including `serve-within`, fail. It passed after the change was undone
- Rewrite mutation check: S319, Side effects, `cgolden/serve/keep-warm-occupies` (P5-4, 2026-10-30). Not occupying equipment while keeping food warm made only `keep-warm-occupies` fail. It passed after the change was undone
- Rewrite mutation check: S320, Normal, `cgolden/serve/chilled-frozen` (P5-4, 2026-10-30). Using 1 when the capacity of the fridge is not written made 3 tests, including `chilled-frozen`, fail. `fridge-capacity-when-written` (`cgolden/serve/fridge-capacity-when-written`) failed with an added break that ignores the written capacity. It passed after the changes were undone
- Rewrite mutation check: S321, Normal, `cgolden/serve/just-before-serve` (P5-4, 2026-10-30). Making std's `justBeforeServe` `serveWithin(5 min)` made `just-before-serve` and 1 other test fail. It passed after the change was undone
- Rewrite mutation check: S322, Boundary, `cgolden/serve/long-wait-info` (P5-4, 2026-10-30). Changing the comparison for long waits to `>=` made only `long-wait-info` fail. It passed after the change was undone
- Rewrite mutation check: S323, Side effects, `cgolden/serve/plating-time` (P5-4, 2026-10-30). Making the plating time in std 0 minutes made 32 tests, including `plating-time`, fail. It passed after the change was undone
- Rewrite mutation check: S422, Normal, `cgolden/menus/multiple-outputs` (P5-4, 2026-10-30). Dropping the second and later outputs of a dish that returns a tuple made only `multiple-outputs` fail. It passed after the change was undone
- Rewrite mutation check: S423, Time and order, `cgolden/menus/plate-overrides-dish` (P5-4, 2026-10-30). Giving the dish's `@vessel` priority over the vessel of the plating made 7 tests, including `plate-overrides-dish`, fail. It passed after the change was undone
- Rewrite mutation check: S429, No-effect condition, `cgolden/kitchen/requires-unmet` (P5-4, 2026-10-30). Removing the check of a dish's `requires` (done before scheduling) made only `requires-unmet` fail. It passed after the change was undone
- Rewrite mutation check: S430, Normal, `cgolden/kitchen/default-places` (P5-4, 2026-10-30). Removing the defaults of the places in std's `Kitchen` (making them `Floor?` and so on) made 26 tests, including `default-places`, fail. It passed after the change was undone
- Rewrite mutation check: S432, Normal, `cgolden/kitchen/kitchen-has-branch` (P5-4, 2026-10-30). Treating `kitchen.has(…)` as a cooking-time value (unknown) made only `kitchen-has-branch` fail. It passed after the change was undone
- Rewrite mutation check: S435, Boundary, `cgolden/vessels/shared-platter` (P5-4, 2026-10-30). Requiring one vessel per serving for `shared` plating too made only `shared-platter` fail. `shared-leftover` (`cgolden/vessels/shared-leftover`) failed with an added break that does not consume the value on plating. It passed after the changes were undone
- Rewrite mutation check: S436, Normal, `cgolden/levels/level-1-human-only` (P5-4, 2026-10-30). The row's break (count equipment even at level 1) changes nothing, because without a kitchen there is no equipment to count. With a break that uses level 2 even when there are only options, 24 tests, including `level-1-human-only`, failed. It passed after the change was undone
- Rewrite mutation check: S463, Normal, `cgolden/ir/kitchen-dependent-info` (P5-4, 2026-10-30). Not reporting `SCH002` for expressions that read kitchen values made 3 tests, including `kitchen-dependent-info`, fail. It passed after the change was undone
- Range of P5-4 (2026-10-30): 7 tests in `ir`, 10 in `levels`, 8 in `kitchen`, 8 in `vessels`, 12 in `serve` and 18 in `menus` were rewritten. Of the golden tests added in Phases 2 to 4, `serve/serve-within-backward` had inherited one-letter names and empty `()` from the original `serve-within`, so it was rewritten with the same mapping table (there is no roadmap row for it). Positions in the JSON expected values (`"line"`, `"column"`) were moved the same way with the helper tool
- Rewrite mutation check: S288, Normal, `cgolden/sched/priority-longest-path` (P5-5, 2026-10-30). Ordering ready tasks from the lowest priority made 22 tests, including `priority-longest-path`, fail. It passed after the change was undone
- Rewrite mutation check: S289, Boundary, `cgolden/sched/tie-node-order` (P5-5, 2026-10-30). Reversing the order of tied tasks made 13 tests, including `tie-node-order`, fail. It passed after the change was undone
- Rewrite mutation check: S290, Normal, `cgolden/sched/backward-serve-at` (P5-5, 2026-10-30). Packing from the front even in backward scheduling made 8 tests, including `backward-serve-at`, fail. `forward` (`cgolden/sched/forward`) is already forward, so it did not fail. It passed after the change was undone
- Rewrite mutation check: S291, Time and order, `cgolden/sched/objective-order` (P5-5, 2026-10-30). In comparing schedules, comparing the total time before keeping the serving time (whether it can be met) made only `objective-order` fail. It passed after the change was undone
- Rewrite mutation check: S292, Normal, `cgolden/sched/prefer-*` (P5-5, 2026-10-30). Ignoring the order given by `--prefer` and comparing in a fixed order made `prefer-fewer-tools`, `prefer-less-waiting`, `prefer-pace-then-tools` and `prefer-steady-pace` fail. `prefer-fewer-washes` and `prefer-tools-then-pace` each failed with an added break: not using the setting, and reversing the given order. It passed after the changes were undone
- Rewrite mutation check: S293, Normal, `cgolden/sched/improvement` (P5-5, 2026-10-30). Skipping the improvement loop made only `improvement` fail. It passed after the change was undone
- Rewrite mutation check: S294, Normal, `cgolden/sched/unattended-frees-hands` (P5-5, 2026-10-30). Assigning a cook to unattended tasks too made 28 tests, including `unattended-frees-hands`, fail. It passed after the change was undone
- Rewrite mutation check: S295, Boundary, `cgolden/sched/oven-range-one-unit` (P5-5, 2026-10-30). A break close to the row's break (count each capability as a separate unit), which allows simultaneous use without looking at when the equipment is free, made 10 tests, including `oven-range-one-unit`, fail (it is smaller than adding code that splits units by capability, and has the same effect of removing contention for the same unit). It passed after the change was undone
- Rewrite mutation check: S296, Normal, `cgolden/sched/variant-earliest` (P5-5, 2026-10-30). Always choosing the first candidate variant made only `variant-earliest` fail. `variant-tie-order` (`cgolden/sched/variant-tie-order`) failed with an added break that chooses the later candidate on a tie. It passed after the changes were undone
- Rewrite mutation check: S297, Normal, `cgolden/sched/variant-info` (P5-5, 2026-10-30). Not reporting `SCH003` for the chosen variant made `variant-info` and 1 other test fail. It passed after the change was undone
- Rewrite mutation check: S298, Normal, `cgolden/sched/calibration` (P5-5, 2026-10-30). Not applying the kitchen's correction factor made only `calibration` fail. It passed after the change was undone
- Rewrite mutation check: S299, Boundary, `cgolden/sched/heat-model` (P5-5, 2026-10-30). Not applying the exponent of the thickness ratio in the heating time formula made `heat-model` and 1 other test fail. It passed after the change was undone
- Rewrite mutation check: S300, No-effect condition, `cgolden/sched/hardness-order-warn` (P5-5, 2026-10-30). Not reporting `SCH004` when the soft item goes in first made only `hardness-order-warn` fail. It passed after the change was undone
- Rewrite mutation check: S301, No-effect condition, `cgolden/sched/capacity-impossible` (P5-5, 2026-10-30). Not reporting `SCH005` when there is not enough equipment made only `capacity-impossible` fail. It passed after the change was undone
- Rewrite mutation check: S303, Normal, `cgolden/sched/person-attributes` (P5-5, 2026-10-30). Ignoring the limits on tasks each person avoids made only `person-attributes` fail. It passed after the change was undone
- Rewrite mutation check: S304, Normal, `cgolden/sched/range-duration-upper` (P5-5, 2026-10-30). Assigning tasks with a ranged time at the length of the lower bound made 43 tests, including `range-duration-upper`, fail. It passed after the change was undone
- Rewrite mutation check: S305, Normal, `cgolden/wash/dirt-from-leaves` (P5-5, 2026-10-30). Not adding dirt from the `leaves` of actions made 24 tests, including `dirt-from-leaves`, fail. It passed after the change was undone
- Rewrite mutation check: S306, Normal, `cgolden/wash/compat-rules` (P5-5, 2026-10-30). A break close to the row's break (shift the rule table by one row), which uses the default value before the value of a matching rule, made 7 tests, including `compat-rules`, fail. It passed after the change was undone
- Rewrite mutation check: S307, Normal, `cgolden/wash/wait-or-wash` (P5-5, 2026-10-30). Always washing a used tool before its next use made 20 tests, including `wait-or-wash`, fail. It passed after the change was undone
- Rewrite mutation check: S308, Normal, `cgolden/wash/reason` (P5-5, 2026-10-30). Removing the reason for the dirt from the arguments of `SCH006` made 15 tests, including `reason`, fail. It passed after the change was undone
- Rewrite mutation check: S309, Side effects, `cgolden/wash/uses-sink-hands` (P5-5, 2026-10-30). Not using the sink for washing made 15 tests, including `uses-sink-hands`, fail. It passed after the change was undone
- Rewrite mutation check: S310, No-effect condition, `cgolden/wash/hazard-not-relaxed` (P5-5, 2026-10-30). Allowing the `@fixed` rules for `Hazard` and `Allergen` to be overridden made `hazard-not-relaxed` and `allergen-not-relaxed` (`cgolden/wash/allergen-not-relaxed`) fail. It passed after the change was undone
- Rewrite mutation check: S311, No-effect condition, `cgolden/wash/same-food-same-state` (P5-5, 2026-10-30). Washing even for the same food in the same state made only `same-food-same-state` fail. It passed after the change was undone
- Rewrite mutation check: S312, Time and order, `cgolden/wash/no-drying` (P5-5, 2026-10-30). Adding a 5-minute wait before washing made 15 tests, including `no-drying`, fail. It passed after the change was undone
- Rewrite mutation check: S313, Normal, `cgolden/cleanup/before-meal` (P5-5, 2026-10-30). Including tableware in the `beforeMeal` cleanup made only `before-meal` fail. `none` (`cgolden/cleanup/none`) and `after-meal` (`cgolden/cleanup/after-meal`) each failed with an added break: cleaning up even with no cleanup, and starting the after-meal cleanup before the meal. It passed after the changes were undone
- Rewrite mutation check: S314, Normal, `cgolden/cleanup/wipe-places` (P5-5, 2026-10-30). Not including wiping of places in the cleanup made 4 tests, including `wipe-places`, fail. It passed after the change was undone
- Rewrite mutation check: S315, Normal, `cgolden/cleanup/store-to-fridge` (P5-5, 2026-10-30). Not treating `store` as a cleanup task made only `store-to-fridge` fail. It passed after the change was undone
- Rewrite mutation check: S316, Time and order, `cgolden/sched/meal-blocks-hands` (P5-5, 2026-10-30). Not excluding the meal time from hand tasks made 4 tests, including `meal-blocks-hands`, fail. It passed after the change was undone
- Rewrite mutation check: S317, Normal, `cgolden/cleanup/dishwasher-during-meal` (P5-5, 2026-10-30). Running the dishwasher after the last wave made only `dishwasher-during-meal` fail. It passed after the change was undone
- Rewrite mutation check: S325, Normal, `cgolden/sched/shared-cutting` (P5-5, 2026-10-30). Not grouping tasks with the same cut made only `shared-cutting` fail. It passed after the change was undone
- Rewrite mutation check: S326, Time and order, `cgolden/sched/meal-duration-from-interval` (P5-5, 2026-10-30). Not making the meal time from the interval of the waves made 4 tests, including `meal-duration-from-interval`, fail. It passed after the change was undone
- Rewrite mutation check: S433, Time and order, `cgolden/sched/parallel-join` (P5-5, 2026-10-30). Not joining the task after `parallel` after all branches made only `parallel-join` fail. It passed after the change was undone
- Rewrite mutation check: S437, Normal, `cgolden/sched/store-discard-in-schedule` (P5-5, 2026-10-30). Removing `store` tasks from the schedule display made `store-discard-in-schedule` and 1 other test fail (a break that removes the cleanup mark did not change the display and did not fail). It passed after the change was undone
- Rewrite mutation check: S438, Normal, `cgolden/sched/heat-thickness-by-cut` (P5-5, 2026-10-30). Using the size as the thickness as is, instead of the nominal thickness for each cut, made only `heat-thickness-by-cut` fail. It passed after the change was undone
- Rewrite mutation check: S439, Normal, `cgolden/sched/heat-direct-attribute` (P5-5, 2026-10-30). Computing it with the formula instead of using the `direct` value of heating made only `heat-direct-attribute` fail. It passed after the change was undone
- Rewrite mutation check: S440, Normal, `cgolden/wash/rule-override` (P5-5, 2026-10-30). Not overriding std's rules with the author's rules made only `rule-override` fail. It passed after the change was undone
- Rewrite mutation check: S441, Normal, `cgolden/wash/staining-info` (P5-5, 2026-10-30). Changing std's `Staining` rule to `wash` made 3 tests, including `staining-info`, fail. It passed after the change was undone
- Rewrite mutation check: S442, Normal, `cgolden/sched/veg-before-meat` (P5-5, 2026-10-30). Stopping the postponement that avoids washing and also not running the improvement trials made 9 tests, including `veg-before-meat`, fail (stopping the postponement alone did not fail, because one of the trials, which orders ties in reverse, chooses the same schedule). It passed after the change was undone
- Mutation check: S550, Fixed premise, `G-SCHED-CHECK` (P5-5, 2026-10-30). The break in the coverage table (remove the washing lines from the `.out` of the rewritten golden tests) did not make this goal fail, because the goal has the implementation print the schedule and checks that. A break that uses tools without putting washing into the schedule (do not record washing tasks in `clean_unit`) made it fail with "67 schedules, 36 violations". It passed after the change was undone
- Range of P5-5 (2026-10-30): 32 tests in `sched`, 11 in `wash` and 7 in `cleanup` were rewritten. The name mapping followed the steps (the results of test actions such as `mark` and `touch` became `marked` and `touched`). The dish names `a`, `b` and `c` in `sched/objective-order` became `applePie`, `beefStew` and `carrotSalad`, which keep the same order, because the order may depend on the names
- How S550 was broken (P5-5, 2026-10-30): `G-SCHED-CHECK` does not read `.out`; it has the implementation print the schedule JSON with `collect-schedules.sh` and runs `schedcheck` on it. So the break in the coverage table does not make it fail. The break was changed to one where the implementation uses a dirty tool without adding washing
- Rewrite mutation check: S332, Data completeness, `cgolden/api/diag-json-fields` (P5-6, 2026-10-30). Not putting the positions of the expansion history in `related` of JSON diagnostics made `diag-json-fields` and 1 other test fail. It passed after the change was undone
- Rewrite mutation check: S333, Docs match, `cgolden/api/sarif-mapping` (P5-6, 2026-10-30). Mapping SARIF `info` to `warning` made `sarif-mapping` fail (`G-API-SARIF` checks the shape of SARIF, so it still passed). It passed after the change was undone
- Rewrite mutation check: S334, Normal, `cgolden/api/signatures-json` (P5-6, 2026-10-30). Not putting parameter types in the signature JSON made only `signatures-json` fail. `text-material` (`cgolden/api/text-material`) failed with an added break that does not put arguments in the step JSON. It passed after the changes were undone
- Rewrite mutation check: S335, Normal, `cgolden/api/types-json` (P5-6, 2026-10-30). Not putting the upper and lower bounds of ranges in the type JSON made only `types-json` fail. It passed after the change was undone
- Rewrite mutation check: S397, Normal, `cgolden/bundle/inline-modules` (P5-6, 2026-10-30). Not writing module namespaces (`module … {`) in the bundled file made 12 tests, including `inline-modules`, fail. It passed after the change was undone
- Rewrite mutation check: S398, Normal, `cgolden/bundle/header` (P5-6, 2026-10-30). Not writing the `std` version in the header made 12 tests, including `header` and `header-license-from` (`cgolden/bundle/header-license-from`), fail. A break that does not carry over `license` made 3 tests, including `header-license-from`, fail. It passed after the changes were undone
- Rewrite mutation check: S399, Data completeness, `cgolden/bundle/contents` (P5-6, 2026-10-30). Including all definitions other than kitchens made 10 tests, including `contents` and `dep-partial` (`cgolden/bundle/dep-partial`), fail. A break that leaves out phrases made `phrases-included` (`cgolden/bundle/phrases-included`) fail. It passed after the changes were undone
- Rewrite mutation check: S399, Data completeness, `cgolden/bundle/global-effects` (P5-6, 2026-10-30). Following `extend`, `rule` and `check` only from names made only `global-effects` fail. It passed after the change was undone
- Rewrite mutation check: S399, Normal, `cgolden/bundle/same-name` (P5-6, 2026-10-30). Including only the first of the candidates with the same name made only `same-name` fail. It passed after the change was undone
- Rewrite mutation check: S399, Normal, `cgolden/bundle/import-pruned` (P5-6, 2026-10-30). Keeping imports of unused names made only `import-pruned` fail. It passed after the change was undone
- Rewrite mutation check: S400, Side effects, `cgolden/bundle/order-by-name` (P5-6, 2026-10-30). Keeping modules in load order instead of sorting them by name made `G-BUNDLE-STABLE` fail with "order-by-name golden: got '0 passed, 1 failed'". It passed after the change was undone
- Rewrite mutation check: S402, Normal, `cgolden/bundle/build-standalone` (P5-6, 2026-10-30). Not registering the modules inside a bundled file made 10 tests, including `build-standalone`, fail. It passed after the change was undone
- Rewrite mutation check: S403, No-effect condition, `cgolden/bundle/std-module-rejected` (P5-6, 2026-10-30). Not reporting `PKG005` for an inline `std` module made `std-module-rejected` and 1 other test fail. It passed after the change was undone
- Rewrite mutation check: S404, Boundary, `cgolden/bundle/std-version-check` (P5-6, 2026-10-30). Not comparing the `std` version of a bundled file made `std-version-check` and 1 other test fail. It passed after the change was undone
- Rewrite mutation check: S404, Boundary, `cgolden/bundle/std-version-incompatible` (P5-6, 2026-10-30). Not comparing the major version made only `std-version-incompatible` fail. It passed after the change was undone
- Rewrite mutation check: S404, Boundary, `cgolden/bundle/std-version-bad` (P5-6, 2026-10-30). Not reporting `PKG010` when the form of a version is wrong made only `std-version-bad` fail. It passed after the change was undone
- Rewrite mutation check: S405, Boundary, `cgolden/bundle/annotation-scope` (P5-6, 2026-10-30). Applying an annotation to the whole rest of the file without looking at the end of its scope made 5 tests, including `annotation-scope`, fail. It passed after the change was undone
- Rewrite mutation check: S406, No-effect condition, `cgolden/bundle/local-closed` (P5-6, 2026-10-30). Dropping `local` when copying during bundling made only `local-closed` fail. It passed after the change was undone
- Rewrite mutation check: S406, No-effect condition, `cgolden/bundle/local-import-rejected` (P5-6, 2026-10-30). Allowing imports of `local` definitions made `local-import-rejected` and 1 other test fail. It passed after the change was undone
- Rewrite mutation check: S407, No-effect condition, `cgolden/bundle/import-two-versions` (P5-6, 2026-10-30). Not reporting `PKG009` for 2 versions of the same dependency made `import-two-versions` and 1 other test fail. It passed after the change was undone
- Rewrite mutation check: S425, No-effect condition, `cgolden/api/local-not-listed` (P5-6, 2026-10-30). Listing `local` definitions too in the list of definitions made only `local-not-listed` fail. It passed after the change was undone
- Rewrite mutation check: S477, Normal, `cgolden/bundle/relative-import` (P5-6, 2026-10-30). Not making the modules inside a bundled file belong to the package made only `relative-import` fail. It passed after the change was undone
- Range of P5-6 (2026-10-30): 11 tests in `api` and 21 in `bundle` (including the dependencies of 13 folder golden tests) were rewritten. The expected values of bundled files copy the original source, so besides applying the name mapping table, the empty `()` of the copied `recipe`, `menu` and `make` were removed too. Places in the JSON expected values where the mapping table had also replaced names of std definitions (the unit `s`, the action parameter `p` and so on), and the positions of `deletedRegion` in the SARIF fixes, were found by running the golden tests and fixed by hand. In the whole of `tests/golden`, one-letter names and empty `()` went to 0
- Rewrite mutation check: S345, Normal, `cli/subcommands` (P5-7, 2026-10-30). Removing the `unbundle` branch made `unbundle-manifest-taken` and `unbundle-no-overwrite` fail with the usage message (`subcommands` looks at the exit codes of `check` and `build` and the list in the usage message, so it did not fail with this break). Adding a break that removes the `build` branch made `subcommands` fail with "build: got '2', want '0'". Both passed after the changes were undone
- Rewrite mutation check: S346, Normal, `cli/entry-forms` (P5-7, 2026-10-30). Searching from the end when splitting the module name and the entry name made only `entry-forms` fail. It passed after the change was undone
- Rewrite mutation check: S348, No-effect condition, `cli/entry-ambiguous` (P5-7, 2026-10-30). Choosing the first one instead of reporting an error when there are several entries made only `entry-ambiguous` fail. It passed after the change was undone
- Rewrite mutation check: S349, Boundary, `cli/kitchen-lookup` (P5-7, 2026-10-30). Using the first kitchen instead of reporting an error when 2 kitchens are found made only `kitchen-lookup` fail. It passed after the change was undone
- Rewrite mutation check: S350, No-effect condition, `cli/no-default-kitchen` (P5-7, 2026-10-30). Making `home` in `kitchens/` the default kitchen when there is no `-k` made `no-default-kitchen` fail with "level without -k: got '"level":2'" (13 other tests without a kitchen failed too). It passed after the change was undone
- Rewrite mutation check: S351, Boundary, `cli/no-prefix-abbrev` (P5-7, 2026-10-30). Accepting prefix matches of long option names (`--kit`) made only `no-prefix-abbrev` fail. `short-options` (`cli/short-options`) failed with an added break that does not accept short names. It passed after the changes were undone
- Rewrite mutation check: S352, Normal, `cli/project-scope` (P5-7, 2026-10-30). Counting modules with only `action` as entries too made only `project-scope` fail. It passed after the change was undone
- Rewrite mutation check: S353, Time and order, `cli/project-order-continue` (P5-7, 2026-10-30). Stopping the build at the first failure made only `project-order-continue` fail. It passed after the change was undone
- Rewrite mutation check: S354, Data completeness, `cli/out-layout` (P5-7, 2026-10-30). Not writing `out/index.json` made `out-layout` fail. It passed after the change was undone
- Rewrite mutation check: S355, Normal, `cli/options-to-ozen` (P5-7, 2026-10-30). Dropping `--prefer` when writing the options out to Ozen made only `options-to-ozen` fail. It passed after the change was undone
- Rewrite mutation check: S356, Normal, `cli/diag-display` (P5-7, 2026-10-30). Not showing the display text from the name at the end of diagnostic lines made only `diag-display` fail. It passed after the change was undone
- Rewrite mutation check: S395, Real path, `cli/render` (P5-7, 2026-10-30). Removing the `render` branch made `render` and 3 rendering tests fail. It passed after the change was undone
- Rewrite mutation check: S395, Real path, `cli/render-entry` (P5-7, 2026-10-30). Not deciding the entry when only a module name is given (making it an error) made `render-entry` and 2 rendering tests fail. It passed after the change was undone
- Rewrite mutation check: S443, Persistence, `cli/out-removed-entry` (P5-7, 2026-10-30). Not removing old output in `out/` made `out-removed-entry` fail. `out-rebuild-identical` (`cli/out-rebuild-identical`) failed with an added break that appends without removing the old output. It passed after the changes were undone
- Rewrite mutation check: S444, Persistence, `cli/unbundle-no-overwrite` (P5-7, 2026-10-30). Removing the overwrite check of `unbundle` made `unbundle-no-overwrite` and `unbundle-manifest-taken` fail. It passed after the change was undone
- Rewrite mutation check: S444, Persistence, `cli/unbundle-manifest-taken` (P5-7, 2026-10-30). Not running the overwrite check for `package.ozen` and under `deps/` made only `unbundle-manifest-taken` fail. It passed after the change was undone
- Rewrite mutation check: S478, Normal, `cli/unbundle-phrases` (P5-7, 2026-10-30). Putting phrases back into the root package instead of their original package made only `unbundle-phrases` fail. It passed after the change was undone
- Rewrite mutation check: S385, Real path, `cli/render-dep-phrases` (P5-7, 2026-10-30). Not passing the phrases folders of dependencies to rendering made only `render-dep-phrases` fail. It passed after the change was undone
- Rewrite mutation check: S387, Real path, `cli/render-stage` (P5-7, 2026-10-30). Not passing `--stage` to rendering made only `render-stage` fail. It passed after the change was undone
- Mutation check: S523, Data completeness, `G-RF-NO-EMPTY-PARENS` (P5-7, 2026-10-30). Changing the dish in `syntax/annotation-target.ozen` back to `recipe salad() {` made it fail with "513 sources, 1 found". It passed after the change was undone (the `recipe-parens` case of `sh/test-check-test-names.sh` also checks the same form)
- Mutation check: S547, Fixed premise, `G-CORE-UNIT` (P5-7, 2026-10-30). Changing `wash(rice)` in a recipe written as a C string in `test_api.c` to `wash(nosuch)` made `api_levels_by_null` fail ("1 tests failed out of 24"). It passed after the change was undone
- Mutation check: S551, Platform path, `G-PLAT-LINUX` (P5-7, 2026-10-30). Dropping definitions (16) from `emit` on the bridge side (`bindings/linux/main.swift`) made it fail with "swift: 12 inputs, 10 mismatches". It passed after the change was undone
- Range of P5-7 (2026-10-30): 22 tests in `tests/cli` (including the template in `lib.sh`), `test_api.c` and `test_ir.c` in `core/tests/unit`, `bindings/inputs/utf8.ozen`, and `tests/samples/test-check.sh` were rewritten. The dish names made by the helper function `recipe FILE NAME` (`r`, `a`, `b`) are not caught by the regular expression of the name check, but they were rewritten too (`gohan`, `aemono`, `butaJiru`; names that keep the sort order). In `render-entry.sh`, a binding had been renamed but `{ s }` in the plating had not been fixed, and the test still passed because it looks only at the status code; this was found and fixed
- How S547 and S551 were broken (P5-7, 2026-10-30): the unit tests do not hold the names of the recipes in C strings as expectations, so the coverage table's "make the declared name and the expected name differ" does not apply. The break puts an undefined name into the recipe instead. In the bridge check, the CLI and the library go through the same entry point, so breaking the library changes both the same way and nothing fails. `bindings/inputs.txt` has no expectations, so the break drops `emit` on the bridge side instead
- Mutation check: S616, Normal, `G-RF-SAMPLES-PIPE` (P5-8, 2026-10-30). Changing the 4 pipes in `samples/T2/` back to chains of `let` and nested calls made it fail with "samples-pipe: 11 samples, 1 without pipe, 0 unlisted exceptions". It passed after the change was undone (the `--samples-pipe` case of `sh/test-check-test-names.sh` also checks that a sample without a pipe fails)
- Mutation check: S617, Fixed premise, `G-RF-SAMPLES-SAME` (P5-8, 2026-10-30). Shifting the roux of `curryRice` from `25 g per serving` to `26 g per serving` made it fail with "5 changed ir, 0 changed schedules, 12 changed renders". It passed after the change was undone (the `samples-ir` case of `sh/test-check-rewrite-equivalence.sh` also checks the same form)
- Mutation check: S618, Fixed premise, `G-RF-SAMPLES-SAME` (P5-8, 2026-10-30). Changing `fry(sliced, 10 min, …)` in `menuB` to `12 min` made it fail with "3 changed ir, 3 changed schedules, 8 changed renders". It passed after the change was undone
- Mutation check: S620, Real path, `G-RF-SAMPLES-SAME` (render-all) (P5-8, 2026-10-30). Folding `let topping = make karubiTopping(…)` in `samples/T4/karubiDon.ozen` into the plating left the IR and schedule unchanged, and it failed with "0 changed ir, 0 changed schedules, 6 changed renders". It passed after the change was undone
- How S620 was broken (P5-8, 2026-10-30): at first the last `let topping =` in `karubiTopping.ozen` was removed, but this binding is not referenced from the plating, and the phrase `name-topping` points to the binding in `karubiDon.ozen`, so the output did not change. The break was changed to fold a binding that a phrase refers to by name
- Mutation check: S537, Data completeness, `G-RF-PATHS` (P5-9, 2026-10-30). Temporarily moving `rewrite-end` to a commit that renamed `decl/enum.ozen` and `decl/enum.out` made it fail with "added, deleted or renamed goldens: got '4'". It passed after the tag was moved back
- Mutation check: S544, Fixed premise, `G-CORE-GRAMMAR` (P5-9, 2026-10-30). Changing `for dish in [rice, curry] { taste(dish) }` in `decl/recipe.ozen` to `taste(curry)` made it fail with "grammar: 133 rules, 1 not covered". It passed after the change was undone
- Mutation check: S545, Fixed premise, `G-CORE-DIAG-REGISTRY` (P5-9, 2026-10-30). Removing the `LIN004` line from `linear/parallel-double-use.out` made it fail with "1 unused". It passed after the change was undone
- Mutation check: S549, Fixed premise, `G-CORE-PROPS` (P5-9, 2026-10-30). Adding `kitchen propsOptions : Options {}` to `linear/parallel-double-use.ozen` made it fail with "monotonic: 75 cases, 1 decreases". It passed after the change was undone
- Mutation check: S615, Persistence, `G-RF-TAGS` (P5-9, 2026-10-30). Removing the tag `rewrite-end` made it fail with "tags: got '3', want '4'". It passed after the tag was placed again
- Mutation check: S619, Fixed premise, `G-RF-SAMPLES-EXPECT` (P5-9, 2026-10-30). Temporarily moving `rewrite-end` to a commit that changed line 1 of `samples/T1/expect.yaml` made it fail. It passed after the tag was moved back
- How S544 was broken (P5-9, 2026-10-30): the rewrite added golden tests that use destructuring bindings, and 22 golden tests now use `tuple_binding`. There are 11 rules used by only one golden test (`for`, `resource`, `trait` and others), and the coverage table row was switched to `for`
- How S545 was broken (P5-9, 2026-10-30): the registry check looks for "code name" pairs in `.out`, so changing the input of a golden test does not make it fail if `.out` does not change. The break removes a diagnostic line from `.out` instead (the list in `api/local-not-listed.out` is in JSON format, so it is not counted)
- The fixed premise of S538 (P5-9, 2026-10-30): a break that makes the compared executable the one at HEAD does not fail now, because the output does not change until LIN005 comes in Phase 6. It is checked and recorded at the gate of Phase 6
- Remaining one-letter names (P5-9, 2026-10-30): the loop variable `x` in `decl/recipe.ozen` was changed to `dish` (`rewrite-end` was placed again after this). The parameters `x` and `p` of actions defined inside golden tests (152 places) copy the style of the standard library (`stdlib/*.ozen`) as is, and are outside the range of S540 (declarations and `let` bindings), so they were kept
- Mutation check: S553, Normal, `cgolden/linear/unused-let` (P6-2, 2026-10-30). Removing the check (reporting no unused values at all) made 79 tests, including `unused-let`, fail. It passed after the change was undone
- Mutation check: S554, Data completeness, `cgolden/linear/unused-let` (P6-2, 2026-10-30). Setting the severity of `LIN005` in the registry to info made 78 tests, including `unused-let`, fail. It passed after the change was undone
- Mutation check: S555, Data completeness, `cgolden/linear/unused-let` (P6-2, 2026-10-30). Making the position of a `let` the expression instead of the bound name made 55 tests, including `unused-let`, fail. It passed after the change was undone
- Mutation check: S556, Normal, `cgolden/linear/unused-expr` (P6-2, 2026-10-30). Not tracking expression statements without a binding made only `unused-expr` fail. It passed after the change was undone
- Mutation check: S557, Normal, `cgolden/linear/unused-ingredient` (P6-2, 2026-10-30). Not tracking ingredients made 24 tests, including `unused-ingredient`, fail. It passed after the change was undone
- Mutation check: S558, No-effect condition, `cgolden/linear/unused-store-discard` (P6-2, 2026-10-30). Making `store` and `discard` not consume their arguments made 9 tests, including `unused-store-discard`, fail. It passed after the change was undone
- Mutation check: S559, No-effect condition, `cgolden/linear/unused-part` (P6-2, 2026-10-30). Tracking the parts of tuple bindings and removing the check that excludes parts made 13 tests, including `unused-part`, fail. It passed after the change was undone
- Mutation check: S560, No-effect condition, `cgolden/linear/unused-last-tuple` (P6-2, 2026-10-30). Not treating the last value as output when it is a tuple (a check shared with `LIN003`) made `unused-last-tuple`, `menus/multiple-outputs` and `sched/store-discard-in-schedule` fail. It passed after the change was undone
- Mutation check: S561, No-effect condition, `cgolden/linear/unused-in-make` (P6-2, 2026-10-30). Tracking inside `make` too and reporting with `quiet` removed at the end of evaluation made `unused-in-make`, `unused-in-make-alone` and `bundle/annotation-scope` fail. It passed after the change was undone
- Mutation check: S562, Normal, `cgolden/linear/unused-in-make-alone` (P6-2, 2026-10-30). Not reporting when a dish made from the menu (`cookedRice`) is evaluated alone made `unused-in-make-alone` and `unused-in-make` fail. It passed after the change was undone
- Mutation check: S563, No-effect condition, `cgolden/linear/unused-composite` (P6-2, 2026-10-30). Tracking inside the `steps` of composite actions too made only `unused-composite` fail. It passed after the change was undone
- Mutation check: S564, No-effect condition, `cgolden/linear/unused-divided` (P6-2, 2026-10-30). Removing the check that excludes foods of which a part was taken out by division made only `unused-divided` fail. It passed after the change was undone
- Mutation check: S565, Normal, `cgolden/linear/unused-allow` (P6-2, 2026-10-30). Not applying severity settings (the annotations in `levels.c`) to `LIN005` made only `unused-allow` fail. It passed after the change was undone
- Mutation check: S566, Data completeness, `G-STD-PHRASES` (P6-3, 2026-10-30). Removing `diag-unused-value` from `phrases/en.ftl` made it fail with "1 missing in en". `G-CORE-DIAG-REGISTRY` and `G-CORE-DIAG-APPEND` do not look at phrases, so they still passed. It passed after the change was undone
- Mutation check: S567, Fixed premise, `G-RF-SAMPLES-UNUSED` (P6-3, 2026-10-30). Reporting `LIN005` for the last value too made it fail with "unusedValue in samples: got '30'". It passed after the change was undone
- Mutation check: S538, Fixed premise, `G-RF-EQUIVALENCE` (P6-4, 2026-10-30). Running the golden tests of the later point with the HEAD executable (with LIN005) made it fail with "291 rewritten, 73 changed results". It passed after the change was undone
- Numbers of P6-1 (2026-10-30): 75 existing golden tests report `LIN005` (133 in 71 tests with text output; the 4 tests in `api/` have 6 in JSON and SARIF and 1 line in the registry list). This is close to the estimate before the rewrite (131); the rewrite that kept the sequence of actions (D50) removed almost no unused values. For the expected values, a throwaway check that finds unused bindings, ingredients and expression statements only from the text of the Ozen source, separately from the implementation, gave the positions and names, which were compared with the real output and then fixed. The 57 mismatches (21 found only by the implementation, 36 found only by the check) were read one by one and confirmed to be limits of the check (`store` and `discard` return no value, macro expansion, `debug ast` does not evaluate, diagnostics of dependency packages are hidden, and so on)
- Decisions made in the implementation of LIN005 (P6-2, 2026-10-30): (1) Values made in the body of `repeat` and `for` are not tracked. As in `repeat batches { let fried = fry(pork / batches, 8 min) }`, the value of each round is part of the dish, and the language has no way to collect them. (2) Values inside a branch of `parallel` do not become the last value (the current evaluation does not update the last value inside a branch). When a dish ends with `parallel`, all values of the branches are reported (many of `wash/*` and `sched/*`). (3) When a call is not evaluated because of an error (`NAM002` and others), its arguments are not consumed and are reported (`modules/ambiguous-on-use`, `eval/runtime-value-in-if`). (4) Bindings inside a macro expansion are reported at the position in the macro definition, with the expanded call attached as `via`. (5) When the value of the same food is bound to 2 names, only the first name is reported
- An error in the judgment command of `G-RF-SAMPLES-UNUSED` (P6-3, 2026-10-30): it ran `ozen check --format json samples/T4/` from the repository root, but in a folder without `package.ozen` this fails with "no FILE given", and standard error was thrown away, so the result was always 0. It was fixed to run inside each sample folder and also count the number of samples run (the command in §5 was fixed too)
- LIN005 in the samples (P6-3, 2026-10-30): the fixed check found 1 for `sugar` in `samples/T4/karubiTopping.ozen`. Since `refresh-start` it had only been listed as an ingredient and never used (an error in the sample). It was fixed by adding `|> season(sugar)`. `G-SAMPLE-CHECK` and the renderer tests still passed unchanged. LIN005 in the rewritten golden tests is included in the 133 above
- Mutation check: S568, Normal, `cgolden/sched/preheat-basic` (P7, 2026-10-30). Not adding the preheating task made 8 tests, including `preheat-basic`, fail. It passed after the change was undone
- Mutation check: S569, Normal, `cgolden/sched/preheat-basic` (P7, 2026-10-30). Always putting preheating on the first unit (`oven#1`) made only `preheat-basic` fail. It passed after the change was undone
- Mutation check: S570, Time and order, `cgolden/sched/preheat-basic` (P7, 2026-10-30). Placing preheating from the start of the task made 8 tests, including `preheat-basic`, fail. It passed after the change was undone
- Mutation check: S571, Time and order, `cgolden/sched/preheat-lookahead` (P7, 2026-10-30). Starting preheating at the time the task became ready made 8 tests, including `preheat-lookahead`, fail. It passed after the change was undone
- Mutation check: S572, Normal, `cgolden/sched/preheat-basic` (P7, 2026-10-30). Making the duration a fixed 10 minutes made 9 tests, including `preheat-basic`, fail. It passed after the change was undone
- Mutation check: S573, Side effects, `cgolden/sched/preheat-unattended` (P7, 2026-10-30). Assigning the first person to preheating made 8 tests, including `preheat-unattended`, fail. It passed after the change was undone
- Mutation check: S574, Side effects, `cgolden/sched/preheat-occupies` (P7, 2026-10-30). Not tying the start of preheating to the time the unit becomes free made `preheat-occupies` and `preheat-hotter` fail. It passed after the change was undone
- Mutation check: S575, No-effect condition, `cgolden/sched/preheat-reuse` (P7, 2026-10-30). Adding preheating every time, even for a unit that is already hot, made only `preheat-reuse` fail. It passed after the change was undone
- Mutation check: S576, Boundary, `cgolden/sched/preheat-hotter` (P7, 2026-10-30). Skipping preheating whenever the unit is hot, without looking at the temperature, made only `preheat-hotter` fail. It passed after the change was undone
- Mutation check: S577, Normal, `cgolden/sched/preheat-arg-temp` (P7, 2026-10-30). Reading the temperature only when it is a number literal made `preheat-arg-temp` and `preheat-json` fail. It passed after the change was undone
- Mutation check: S578, Time and order, `cgolden/sched/preheat-backward` (P7, 2026-10-30). Placing preheating in backward scheduling the same way as forward made only `preheat-backward` fail. It passed after the change was undone
- Mutation check: S579, No-effect condition, `cgolden/sched/preheat-max-temp` (P7, 2026-10-30). Not looking at `heated` in `req_met` made only `preheat-max-temp` fail. It passed after the change was undone
- Mutation check: S580, Boundary, `cgolden/sched/preheat-max-temp-equal` (P7, 2026-10-30). Making the comparison with `maxTemp` not satisfied when equal made only `preheat-max-temp-equal` fail. It passed after the change was undone
- Mutation check: S581, No-effect condition, `cgolden/sched/preheat-no-max-temp` (P7, 2026-10-30). Treating units without `maxTemp` as not satisfying it made 11 tests, including `preheat-no-max-temp`, fail. It passed after the change was undone
- Mutation check: S582, Fixed premise, `cgolden/sched/preheat-ir` (P7, 2026-10-30). Adding preheating to the IR display made only `preheat-ir` fail. It passed after the change was undone
- Mutation check: S583, Data completeness, `cgolden/sched/preheat-basic` (P7, 2026-10-30). Changing the word on the preheating line to `wash` made 8 tests, including `preheat-basic`, fail. It passed after the change was undone
- Mutation check: S584, Data completeness, `cgolden/sched/preheat-json` (P7, 2026-10-30). Not putting `heats` in the schedule JSON made only `preheat-json` fail. It passed after the change was undone
- Mutation check: S585, Data completeness, `cgolden/sched/preheat-json` (P7, 2026-10-30). Not putting `needs` in the schedule JSON made only `preheat-json` fail. It passed after the change was undone
- Mutation check: S586, Data completeness, `G-API-SCHEMA` (P7, 2026-10-30). Removing `preheat` from the enum of task `kind` in the schema made it fail with "420 files, 13 invalid". It passed after the change was undone
- Mutation check: S587, Fixed premise, `sh/test-schedcheck.sh` (P7, 2026-10-30). Removing the preheating check of `schedcheck` made the 3 `bad-preheat-*` cases fail (`G-SCHED-CHECK` still passed with 0 violations, because it only collects correct schedules). It passed after the change was undone
- Mutation check: S588, Docs match, `rtest/schedule-text` (P7, 2026-10-30). Removing the `preheat` key from `ja.ftl` made the preheating test of `schedule-text` fail (`G-STD-PHRASES` only looks at phrases of definitions, so it still passed). It passed after the change was undone
- Mutation check: S589, Real path, `rtest/recipe-schedule` (P7, 2026-10-30). Skipping `preheat` tasks in the schedule table made the preheating test of `recipe-schedule` fail. It passed after the change was undone
- Mutation check: S590, Data completeness, `G-RF-STD-PREHEAT` (P7, 2026-10-30). Removing `preheatTime` from `OvenRange` made it fail with "oven kinds with preheatTime: got '1', want '2'". It passed after the change was undone
- Mutation check: S591, Data completeness, `G-STD-PROVENANCE` (P7, 2026-10-30). Removing `@source(provisional)` from `preheatTime` of `Oven` made it fail with "1 without source". It passed after the change was undone
- How preheating is placed (P7-2, 2026-10-30): D48's "rebuild with the same release loop as `@serveWithin`" was not taken. The forward schedule, when it places a task that needs a temperature, places preheating back in the past (from the later of `now - preheatTime` and the time the unit becomes free). Preheating is unattended, so it can be placed without changing who is assigned. When the task has to wait until preheating ends, that time is added to the next events. In the backward schedule (built in reverse), right after a task that needs a temperature is placed, the preheating interval is reserved on the unit; if the next task that uses that unit needs the same or a lower temperature, the reservation is cancelled, and otherwise the preheating is confirmed. The breaks of S574 and S578 in the coverage table were rewritten to match this way of placing
- Rebuilding `preheat-reuse` and `preheat-hotter` (P7-1, 2026-10-30): they were first written as 2 foods baked separately, but the scheduler also tries plans with tied tasks swapped and takes the shorter one, so for `hotter` it chose a schedule that bakes at 200 °C first and reuses the oven at 180 °C, and the reheating could not be checked. They were rewritten as one food baked twice in a row (the order fixed by dependency)
- Preheating time (P7-1, 2026-10-30): `preheatTime: 10..20 min @source(provisional)` was added to std's `Oven` and `OvenRange`. Whirlpool's product support page "Long Pre-Heat Times for Oven" gives about 12 to 15 minutes to 350°F (about 20 minutes with a hidden heater), ±5 minutes, but this is a guide for one product line and one temperature, so the value was kept provisional and recorded in `spec/sources/search-log.yaml`
- Decisions (P7-2, 2026-10-30): a unit whose `maxTemp` is lower than the required temperature gets `KIT002`, but it is not removed from the schedule candidates (in a kitchen with 2 ovens where only one is too low, the lower one may be chosen). `ASM007 provisionalValue` is not reported for the provisional preheating time (the same treatment as the props of kinds, such as washing time)
- A note on the gate of Phase 6 (P6-4, 2026-10-30): `G-PLAT-WASM` fails when the wasm build is out of date (the goal does not build). It passed after rebuilding with `scripts/build-wasm.sh`. `G-REPO-SELFTESTS` also fails in `test-worker-test.sh` when the wasm build is out of date. After changing the core, rebuild with `build-wasm.sh` before running them
- Mutation check: S592, Time and order, `cgolden/serve/use-within-basic` (P8, 2026-10-30). Not adding the release for the time limit made 5 tests, including `use-within-basic`, fail. It passed after the change was undone
- Mutation check: S593, Boundary, `cgolden/serve/use-within-two-consumers` (P8, 2026-10-30). Looking only at the first task that takes the value as input made `use-within-two-consumers` and `use-within-missed` fail. It passed after the change was undone
- Mutation check: S594, Normal, `cgolden/serve/use-within-missed` (P8, 2026-10-30). Not reporting `SCH011` made only `use-within-missed` fail. It passed after the change was undone
- Mutation check: S595, Data completeness, `cgolden/serve/use-within-missed` (P8, 2026-10-30). Making the position of `SCH011` the `let` statement instead of the annotation made only `use-within-missed` fail. It passed after the change was undone
- Mutation check: S596, Time and order, `cgolden/serve/use-within-backward` (P8, 2026-10-30). Not adding the release for the time limit in backward scheduling made only `use-within-backward` fail. It passed after the change was undone
- Mutation check: S597, No-effect condition, `cgolden/serve/use-within-on-ingredient` (P8, 2026-10-30). Not reporting `TYP017` even when it is put on an ingredient itself made only `use-within-on-ingredient` fail. It passed after the change was undone
- Mutation check: S598, Normal, `cgolden/serve/use-within-expr` (P8, 2026-10-30). Not reading annotations on expression statements made only `use-within-expr` fail. It passed after the change was undone
- Mutation check: S599, No-effect condition, `cgolden/serve/use-within-no-consumer` (P8, 2026-10-30). Reporting `SCH011` for every named time limit without looking at whether there is a consuming task made 6 tests, including `use-within-no-consumer`, fail. It passed after the change was undone
- Mutation check: S600, Data completeness, `cgolden/serve/use-within-json` (P8, 2026-10-30). Not putting `limits` in the schedule JSON made only `use-within-json` fail. It passed after the change was undone
- Mutation check: S601, Fixed premise, `sh/test-schedcheck.sh` (P8, 2026-10-30). Removing the time limit check of `schedcheck` made the `bad-use-within` case fail (`G-SCHED-CHECK` still passed with 0 violations). It passed after the change was undone
- Mutation check: S602, Fixed premise, `cgolden/serve/use-within-ir` (P8, 2026-10-30). Making the name of the annotation the IR name of the task that makes the value made 8 tests, including `use-within-ir`, fail. It passed after the change was undone
- Mutation check: S603, Data completeness, `G-CORE-DIAG-REGISTRY` (P8, 2026-10-30). Removing `SCH:useWithinMissed` from `diag-names.txt` made it fail with "1 unlisted" (`G-CORE-DIAG-APPEND`, `G-STD-PHRASES` and `G-TYPE-SEVERITY-TABLE` do not look at the list of names, so they still passed). It passed after the change was undone
- Implementation of `@useWithin` (P8, 2026-10-30): annotations on bindings, expression statements and ingredients are read during evaluation, and the IR node that made the value and the limit are pushed onto a table (they are pushed inside `make` too, so they also apply to values used in a menu). In the schedule loop, if the difference between the start of each task that takes the value as input and the end of the task that made it exceeds the limit, a release is added to the task that made it. In backward scheduling, as with the current release, the task is raised in rank as an urgent task. A time limit that cannot be kept gives `feasible=false` in scoring, and if no attempt keeps it, the schedule without the delay is chosen. When there is no task that makes the value (an ingredient itself), `TYP017` is reported. `limits` in the JSON is printed only when there is a time limit (so that existing schedule JSON does not change; it is an optional field in the schema)
- The golden test of S598 (P8-1, 2026-10-30): the value of an expression statement that is not bound cannot be taken as input inside the same dish. So `@useWithin` was put on the last expression of a dish, and a menu uses that dish with `make` (`use-within-expr`). This also checks that time limits pushed inside `make` apply to the schedule of the menu
- Expected values of `use-within-*` (P8-1, 2026-10-30): the schedules were built by hand and checked against the difference from the schedule of the executable before annotations were read (only the release of the making task). `use-within-backward` uses 1 person, because with 2 the backward schedule already keeps the limit. In `use-within-two-consumers`, the first form kept the limit naturally through reordering of ties, so the cutting time was made 25 minutes
- `G-RF-GOLDEN-COUNT` (P8, 2026-10-30): P3-1 made 15 pipe golden tests (2 more than the planned 13), so the expected increase was changed from 66 to 68 (the table in §5, the judgment command and `goals.sh`)
- The gate of Phase 7 (P7-5, 2026-10-30): the 56 goals before fuzzing passed. Fuzzing rebuilds the core and takes an hour, during which Phase 8 cannot go into the main line, so it was stopped. P7-5 is run together with the gate of P8-4
- Mutation check: S524, Docs match, `G-RF-CONCEPT-PARENS` (P9-1, 2026-10-30). Adding `()` to `make curry` in a code example in the concept made it fail with "got '1'". It passed after the change was undone
- Mutation check: S604, Docs match, `G-RF-CONCEPT` (P9-1, 2026-10-30). Removing `|>` and `per serving` from concept §4.4 made it fail. It passed after the change was undone
- Mutation check: S605, Docs match, `G-RF-CONCEPT` (P9-1, 2026-10-30). Removing `unusedValue` from concept §4.1 made it fail. It passed after the change was undone
- Mutation check: S606, Docs match, `G-RF-CONCEPT` (P9-1, 2026-10-30). Removing `preheat` from concept §4.2 made it fail. It passed after the change was undone
- Mutation check: S607, Docs match, `G-RF-CONCEPT` (P9-1, 2026-10-30). Removing `useWithin` from concept §4.5 made it fail. It passed after the change was undone
- Mutation check: S608, Docs match, `G-RF-CONCEPT` (P9-1, 2026-10-30). Removing `useWithinMissed` and `servingsUnknown` from concept §6.1 made it fail. It passed after the change was undone
- Mutation check: S609, Docs match, `G-RF-README-EXAMPLE` (P9-2, 2026-10-30). Changing the plating line in the expected schedule output in the README from `(1518)` to `(1519)` made it fail with "1 mismatches". It passed after the change was undone
- Mutation check: S610, Fixed premise, `G-REPO-VERSION` (P9-3, 2026-10-30). Changing only `VERSION` to `0.2.0` made 3 checks fail. It passed after the change was undone
- The README example (P9-2, 2026-10-30): `let s = make soup` in the example menu was a one-letter name, so it became `soupPot`; the soup ingredients were changed to `per serving` (with the default value `servings: 2`), and the cutting steps to a pipe. The total amounts are the same as before. Diagnostic positions were counted again by hand, and the example check confirmed they match
- concept (P9-1, 2026-10-30): unused values were added to §4.1, preheating to §4.2, pipes, leaving out `()` and `per serving` to §4.4, `@useWithin` to §4.5, and `unusedValue`, `useWithinMissed` (warning) and `servingsUnknown` (error) to the severity table of §6.1. The empty `()` of `make homemadeNaan()` and `make cookedRice()` in the text were also removed
- Coverage audit: 25 findings, 20 taken (P9-4, 2026-10-30). An independent subagent read §1, §4, §8, the coverage table, and the sources of the added and rewritten tests. 3 gaps in the implementation (for kinds without `preheatTime`, no preheating is added and `schedcheck` reports a violation; units with a low `maxTemp` stay among the candidates; `@useWithin(10 g)` is treated as a time) were fixed and became S629, S630 and S633. The decisions made in §9 became S621 to S628, S631, S632 and S634 to S636, and 17 golden tests and 2 `schedcheck` fixtures were added. `G-RF-CONCEPT` was split per word and per row of the severity table. The reasons for the 5 findings not taken were written at the end of §5.3. The wording of S501, S560, S579 and S586 was fixed to match the implementation
- Order in `schedcheck` (P9-4, 2026-10-30): a preheating of length 0 falls at the same time as the task, so at the same start time preheating is now looked at first (the schedule text prints the task first, as in the current order)
- Mutation check: S561, No-effect condition, `cgolden/linear/unused-in-make-only` (P9-4, 2026-10-30). Tracking inside `make` too and reporting with `quiet` removed made 4 tests, including `unused-in-make-only`, fail. It passed after the change was undone
- Mutation check: S575, Boundary, `cgolden/sched/preheat-same-temp` (P9-4, 2026-10-30). Changing the temperature comparison from "the same or lower" to "lower" made only `preheat-same-temp` fail. It passed after the change was undone
- Mutation check: S578, Time and order, `cgolden/sched/preheat-reuse-backward` (P9-4, 2026-10-30). Not cancelling the preheating reservation in backward scheduling made only `preheat-reuse-backward` fail. It passed after the change was undone
- Mutation check: S578, Time and order, `cgolden/sched/preheat-hotter-backward` (P9-4, 2026-10-30). Cancelling the preheating reservation in backward scheduling without looking at the temperature made only `preheat-hotter-backward` fail. It passed after the change was undone
- Mutation check: S592, Boundary, `cgolden/serve/use-within-range` (P9-4, 2026-10-30). Measuring the wait from the lower bound of the end of the making task made only `use-within-range` fail. It passed after the change was undone
- Mutation check: S597, No-effect condition, `cgolden/serve/use-within-on-alias` (P9-4, 2026-10-30). Treating a binding as an ingredient only when it has the same name as the ingredient made only `use-within-on-alias` fail. It passed after the change was undone
- Mutation check: S600, Data completeness, `cgolden/serve/use-within-missed-json` (P9-4, 2026-10-30). Always setting `missed` to `false` made only `use-within-missed-json` fail. It passed after the change was undone
- Mutation check: S601, Fixed premise, `sh/test-schedcheck.sh` (P9-4, 2026-10-30). Removing, one at a time, the check for a case marked `missed` that is actually on time and the check for a case that points to an unknown task made the `bad-use-marked` and `bad-use-unknown` cases fail. It passed after the change was undone
- Mutation check: S621, No-effect condition, `cgolden/linear/unused-repeat` (P9-4, 2026-10-30). Tracking values in the body of loops too made 4 tests, including `unused-repeat`, fail. It passed after the change was undone
- Mutation check: S622, Normal, `cgolden/wash/reason` (P9-4, 2026-10-30). Making the last value of a branch the last value of the dish made 13 tests, including `wash/reason`, fail. It passed after the change was undone
- Mutation check: S623, Normal, `cgolden/eval/runtime-value-in-if` (P9-4, 2026-10-30). Not reporting in a dish that has even one diagnostic made 41 tests, including `runtime-value-in-if`, fail. It passed after the change was undone
- Mutation check: S624, Data completeness, `cgolden/macro/hygiene-tmp` (P9-4, 2026-10-30). Not attaching the expansion history (`via`) to the report made 3 tests, including `hygiene-tmp`, fail. It passed after the change was undone
- Mutation check: S625, No-effect condition, `cgolden/linear/unused-alias` (P9-4, 2026-10-30). Pushing the same value twice made only `unused-alias` fail. It passed after the change was undone
- Mutation check: S626, Normal, `cgolden/menus/split-pass` (P9-4, 2026-10-30). Not reporting in menus made `split-pass` and `sched/shared-cutting` fail. It passed after the change was undone
- Mutation check: S627, Normal, `cgolden/linear/unused-if` (P9-4, 2026-10-30). Not tracking the body of `if` made only `unused-if` fail. It passed after the change was undone
- Mutation check: S628, No-effect condition, `cgolden/linear/unused-non-food` (P9-4, 2026-10-30). Reporting bindings of amounts as unused values too made 12 tests, including `unused-non-food`, fail. It passed after the change was undone
- Mutation check: S629, Data completeness, `cgolden/sched/preheat-no-preheat-time` (P9-4, 2026-10-30). Not adding preheating when `preheatTime` is 0 made only `preheat-no-preheat-time` fail. It passed after the change was undone
- Mutation check: S630, No-effect condition, `cgolden/sched/preheat-max-temp-mixed` (P9-4, 2026-10-30). Not removing units with a low `maxTemp` from the candidates made only `preheat-max-temp-mixed` fail. It passed after the change was undone
- Mutation check: S631, No-effect condition, `cgolden/sched/preheat-cooled` (P9-4, 2026-10-30). Keeping the unit hot even after a task that needs no temperature made only `preheat-cooled` fail. It passed after the change was undone
- Mutation check: S632, Time and order, `cgolden/sched/preheat-short-prep` (P9-4, 2026-10-30). Not making the task wait until preheating ends made 11 tests, including `preheat-short-prep`, fail. It passed after the change was undone
- How S632 was broken (P9-4, 2026-10-30): the first break was "do not clamp the start of preheating at 0 seconds", but the start is the later of that and the time the unit becomes free (0 or more), so the result did not change and nothing failed. The break in the coverage table was changed to "do not make the task wait until preheating ends"
- Mutation check: S633, No-effect condition, `cgolden/serve/use-within-bad-arg` (P9-4, 2026-10-30). Not looking at the dimension of the argument of `@useWithin` made only `use-within-bad-arg` fail. It passed after the change was undone
- Mutation check: S634, Data completeness, `cgolden/sched/preheat-json` (P9-4, 2026-10-30). Printing the `limits` field even without a time limit made 4 tests, including `preheat-json`, fail. It passed after the change was undone
- Mutation check: S635, Real path, `cgolden/serve/use-within-expr` (P9-4, 2026-10-30). Not pushing time limits inside `make` made only `use-within-expr` fail. It passed after the change was undone
- Mutation check: S636, Normal, `cgolden/serve/use-within-missed-no-delay` (P9-4, 2026-10-30). Always taking the first attempt that looked at the time limit, without looking at the score, made 11 tests, including `use-within-missed-no-delay`, fail. It passed after the change was undone
- Expected values of the golden tests added by the audit (P9-4, 2026-10-30): the schedules were built by hand. Those that the implementation before the fix could produce were compared with its output, and the 3 that assume the fix (`preheat-no-preheat-time`, `preheat-max-temp-mixed`, `use-within-bad-arg`) were written by hand and committed first. Only the line numbers of `unused-if` were miscounted, and they were fixed
- Mutation check: S542, Side effects, `G-RF-SEPARATION` (P9-5, 2026-10-30). In another worktree, changing a golden `.out` and `sema.c` in one commit made it fail with "1 mixed". It passed after the commit was removed
- Mutation check: S552, Data completeness, `G-RF-GOLDEN-COUNT` (P9-5, 2026-10-30). In another worktree, removing `linear/unused-alias` made it fail with "got '447', want '448'". It passed after the change was undone
- Things only reported to the user and not fixed in this plan:
  - The duplicate D33 and D34 in the roadmap headings (lines 124 and 325, lines 135 and 334)
  - The number of grammar rules in roadmap §4.20.1 (it says 130, but the measured number is 131)
  - The gap where `:`, `has` and unknown entries in kitchen requirements count as satisfied, and the spelling of `modes` of `HeatSource` not being checked (B7)
- A fix to the form of `G-RF-GOLDEN-COUNT` (D80 of the quantity check plan `plans/ozen-quantity-checks.md`, 2026-11-02): the form that checks that the current number of golden tests is exactly N0 + 85 fails as soon as a later plan adds a single golden test (it was failing at 464 on 2026-11-02). To keep what this plan wanted to check (that the added golden tests have not disappeared), it was changed into 2 checks: "all golden tests that existed at the tag `readme-start` still exist" and "the number at `readme-start` is N0 + 85"

---

## 10. Risks

- **R1. The amount of rewriting**: `tests/golden` has 816 files, and the expected values are rewritten by hand. 196 `.out` files contain positions (line and column), and about 60 golden tests need schedules built by hand. On top of that, D51 redoes the mutation checks of about 242 rows of roadmap §5.3. The work is split into tasks per folder (P5-1 to P5-7) so that they can run in parallel in separate sessions. At the gate of each task, `G-RF-EQUIVALENCE` is run to confirm by machine that no property was lost, before moving on to the mutation checks.
- **R2. Errors in hand calculation**: the expected values are written by hand, so errors can creep in. Schedules are compared with `schedcheck`, and amounts with `Fraction`. When an error is found, it is fixed in a commit separate from the implementation, following the rule of roadmap §5.2, and the reason is written in `red-first-exceptions.txt` and §9.
- **R3. LIN005 appears in the samples**: the sample check (`tests/samples/check.py`) does not look at the number of warnings, so `G-SAMPLE-CHECK` does not fail even if it appears. It is counted by `G-RF-SAMPLES-UNUSED`, and if it appears, it is reported to the user, who decides whether to fix the samples or the rule.
- **R4. Schedules with preheating and time limits do not converge**: the release loop runs at most 8 times, and if a limit cannot be kept it ends with a warning. The hand-built golden tests include an example with only one placement that keeps the limit, so that a golden test fails if it does not converge.
- **R5. CI fails when the prototype is removed**: `ios-proto.yml` uses `proto/`. P1-2 removes the workflow too, and P1-3 checks `G-PLAT-CI` and `G-RF-CI-NO-PROTO`.
- **R6. Another session commits to the same `main`**: there is a precedent where 196 commits came in after the conversation started. At the start of each phase and before committing, look at `git log` and `git status`, and if work that touches the same files has come in, coordinate with that session or ask the user.
- **R7. D45 changes existing expected values**: the display of golden tests that print Count values (number of vessels, number of kitchens and so on) may change from `4 pcs` to `4`. P2-1 counts those golden tests, and their expected values are fixed in a separate commit.
