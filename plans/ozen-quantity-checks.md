# Plan to check the units of action arguments and amounts of zero or less

- Created: 2026-11-02
- Reference source: none (there is no code reference source). The requests are GitHub Issue [atsuki-seo/Ozen#1](https://github.com/atsuki-seo/Ozen/issues/1) (the units of action arguments are not checked) and [atsuki-seo/Ozen#2](https://github.com/atsuki-seo/Ozen/issues/2) (negative or zero times and amounts are not errors). Both were reported by sin-45, at commit `8db17e6`
- Implementation target: `/home/atsuki/ドキュメント/Ozen`, `main` (D81). The starting point is `8db17e6` (up to the addition of OGP to the introduction page)
- Prerequisite rules: `CLAUDE.md` (work only on `main`, run `G-REPO-NOREPLY` before a push, `plans/` and `docs/` are in Japanese, other committed artifacts are in English), `~/.claude/rules/plan-language.md`, `~/.claude/rules/repo-language.md`, `~/.claude/rules/git-commit.md`, `~/.claude/rules/comment-style.md`, `~/.claude/rules/github.md`. The §5 goals of the existing plans `plans/ozen-roadmap.md` (below, "the roadmap"), `plans/ozen-syntax-refresh.md` (below, "the syntax plan") and `plans/ozen-readme-compact.md` (below, "the README plan") are cited as prerequisite goals (§5)

## 0. How to read this plan

- This plan is for the work that follows the README plan, and it continues the numbering. Design decisions start at **D69** and spec items start at **S695** (the README plan ended at D68 and S694).
- Every task completion condition is written **so that a command decides pass or fail**. Human checks are not tasks; they go to the hand-over list in §8.
- Completion conditions refer to the prerequisite goals listed in §5 and to the `G-QC-*` goals defined in §5 of this plan. The judgment commands in the §5 code blocks are authoritative. They are not written in tables (because `|` becomes `\|` in a table).
- In §2, D69 to D72 were decided by the user on 2026-11-02, before the plan was written (after reading the Issues, the direction was confirmed with 3 questions). D73 to D87 were decided on 2026-11-02, after the plan was written, by asking the user one by one (all 15 confirmed individually; D85, D86 and D78, which affect the scope, came first, then the rest in number order; all took the recommendation). For D76, the recommendation text was made exact to match an example shown during the confirmation (`0 g per serving` is `TYP021` even when the servings are out of range) before it was written in, and S785 was added. Each section starts with a "Decision:" line, and the comparison of the recommendation and alternatives is kept as history (P0-1).
- Facts about the implementation target were checked on 2026-11-02 by reading the files at `8db17e6` and running `build/core-gcc-release/ozen` (built from the same source as the core at `8db17e6`; all 448 goldens pass) (§3). After writing, the plan received one audit by an independent subagent, and its findings were taken in (end of §5.3 and §9).
- Checkable claims in §4 carry spec item IDs. The coverage table in §5.3 shows, for each S item, a test or a hand-over to §8 (with a reason).

---

## 1. Purpose and scope

**Purpose**: Make it an error when an amount passed to an action does not match the dimension the action declares (time, length, and so on) (#1). Also make it an error to write zero or a negative value for an ingredient amount, an action's time or size, or the servings (`servings`) (#2). In both cases, the wrong value now goes straight into the schedule and the recipe text ("fry the egg over medium heat 3 ml", "boil the water -300 s"). Make sure the same mistakes do not pass through other ways of writing them (a range with units on both ends `3 ml..5 ml`, the arguments of `make` in a menu, `-s` and the options).

Example diagnostics after the fix:

```text
$ ozen check case1.ozen          # let x = fry(egg, 3 ml)
error TYP001 dimension case1.ozen:5:20-5:24 args=["Volume", "Time"]: dimension

$ ozen check case2.ozen          # water: -100 ml / boil(water, -300 s)
error TYP021 nonPositiveAmount case2.ozen:3:12-3:19 args=["water", "-100 ml"]: non positive amount
error TYP012 rangeViolation case2.ozen:5:23-5:29 args=["time", "-300 s", "0 s<.."]: range violation
```

(The positions and the order of arguments are expected values written from the spec in §4. The correct values are written by hand as each golden's expectation in P1, P2 and P3)

### In scope

- In an action call, check quantity arguments against the declared type (D69)
- Evaluate a range with units on both ends (`3 ml..5 ml`) as an interval, so that the check and the amount checks apply to it (D85)
- Check the quantity arguments that `make` in a menu passes to a dish against the dish's argument declarations (D86)
- Add a way to write a type range whose lower end is not included (D70, D73, D74, D79)
- Give the time and size of standard library actions a range greater than 0 (D70)
- A new diagnostic for ingredient amounts of zero or less (D71, D75, D76)
- Limit the servings (`servings`) to 1 or more in every place: the recipe and menu headers, the arguments of `make`, `-s`, and the build-time options; give only one diagnostic for the same value (D72, D77)
- Update `docs/concept.md`, `core/grammar.ebnf`, `core/src/rules.def`, `core/fuzz/ozen.dict` and `schemas/result.schema.json`
- Add `G-QC-*` to `scripts/goals.sh`, and add this plan to the Plan section of `CLAUDE.md` (D82)
- Fix the range goals of completed plans so that later work does not break them (D80, D87)

### Out of scope (reasons)

- **Mixing up non-quantity values and quantities (`cut(slice, dice)`, `cut(3 mm)`, `fry(egg, 3 min, 3 ml)`)**: These pass an enum value to a dimension argument, or a quantity to an enum argument. They need a different check (enum vs. quantity) from the "units" in the Issue. The audit confirmed they still pass (§3.7). Propose this as separate work after the final gate.
- **Arguments inside state types (`T<shape: cut(slice, 0 mm)>`, the axis argument of `soaked(time: Time)`)**: The check path (checking state types) is different. Whether the same hole exists is checked in P1-2 and written in §9; if it does, propose it as separate work.
- **Checking the default values of action arguments**: Excluded if D78 follows the recommendation (it changes with the decision).
- **How `-1..2 g` is read (as `-(1..2 g)`)**: This is an operator precedence issue and is not in the Issues. This plan only confirms that this amount's lower end is zero or less and so is an error (S731).
- **The same diagnostic appearing once per iteration inside a loop (`repeat`)**: This is the same behavior as existing diagnostics (`TYP006` and others), and the diagnostics in this plan follow it. Whether to merge them is a question for the design of diagnostics as a whole.
- **Headers that write the servings as an expression (`servings: Count = 1 - 1`)**: Default value expressions in headers are still not evaluated (`value_qty` cannot read them). Not in the Issues.
- **`make gohan(servings: 0)` for a dish that has no `servings` argument**: This is a different behavior, where arguments with unmatched names are silently dropped (§3.7). Propose it as separate work after the final gate.
- **A single value with no `..` in a range (`Time in 0 s`) being silently ignored, and number-only ranges (`1..20`) not appearing in the JSON type list**: Existing behavior, not in the Issues (§3.7).
- **Fractional `servings` (`1.5`)**: The range `1..` keeps the current closed-interval check, which allows fractions.
- **Amounts to buy such as `buy Naan 0 pcs`, and amounts to split**: The Issue only names ingredient amounts. Whether the same hole exists is checked in P3-2 and written in §9.
- **Ranges for temperature arguments**: Negative temperatures (a freezer at -18 °C) are correct values, so no range is added (D70).
- **Updating the vocabulary table (`spec/vocab/actions.yaml`)**: The table only holds the dimensions of arguments, not ranges. `tools/std-from-vocab/convert.py`, which builds the standard library from the table, is not called by any goal (R7).
- **The wording of the comments on the Issues and closing them**: Decided in D84; done after the final gate, after confirming with the user.
- **Human checks**: Handed over in §8.

---

## 2. Design decisions

### D69. Checking quantity arguments of actions (#1) [decided]

- **Decision:** In an action call, check a known quantity passed to an argument whose declared type is a dimension against the declaration, and if the dimension differs, give the existing `TYP001 dimension`. A number without a unit is also an error, as a dimensionless quantity. (2026-11-02, confirmed by the user)
- History: `check_value` (`core/src/sema.c:2640`), which checks a value against a type, exists, but it is only called for recipe arguments, kitchen settings and build-time options. `call_action` (`core/src/sema.c:4046`), which calls an action, only checks the states of ingredient arguments (§3.1).

### D70. How to reject zero and negative times and sizes [decided]

- **Decision:** Add a way to write a type range whose lower end is not included, put it on the `time` and `size` of standard library actions, and make zero and negative values `TYP012 rangeViolation`. Do not put it on temperature arguments. The spelling `0 s<..` is the first candidate, decided after checking for conflicts with the parser (D73). (2026-11-02, confirmed by the user)
- The alternative was: write `in 0 s..` without changing the grammar, and reject only negative values. `simmer(x, 0 min)` would still pass.

### D71. Diagnostic for ingredient amounts [decided]

- **Decision:** If an ingredient amount is zero or less, give a new diagnostic (an error). A range amount is judged by its lower end, and `[any amount]` is not covered. (2026-11-02, confirmed by the user)
- The alternative was: reuse `TYP012 rangeViolation` and put `0<..` or similar in the range field. That would show a range that was never declared.

### D72. The range of the servings (servings) [decided]

- **Decision:** Check both the `servings` in the recipe header and the `servings` in the build-time options with `Count in 1..`. (2026-11-02, confirmed by the user)
- The alternative was: only the build-time options (`Options.servings`). `servings: 0` in a recipe header would still pass.

### D73. Spelling of a range whose lower end is not included [decided]

- **Decision:** As recommended, the spelling is `a<..` (`a<..b` when there is also an upper end). `<` and `..` are written together with no space between them. (2026-11-02, confirmed by the user)
- **Recommendation**: `a<..` (`a<..b` when there is also an upper end). `<` and `..` are written together with no space between them. It reads as "a < value ..", where `<` means "greater than a".
  - Conflict check (§3.2): With the current parser, `0 s<..` after a type's `in` is a syntax error (`SYN003 args=["<"]`), so the reading of current valid programs does not change. A `<` after a quantity is not read as part of the unit (a unit is only a name and `/名前`, a slash and a name). `3 min<..5 min` inside an expression is now read as a comparison (`3 min < ..5 min`), but if D74's recommendation is taken and it is only read in type ranges, the reading of expressions does not change.
  - It can be told apart from the `<` of type arguments (`T<shape: cut>`) because what comes before `<` is a quantity or number, and what comes right after is `..`.
- Alternative: `a^..` (following Raku's `^..`). There is a precedent in another language, but `^` is a symbol not in the current lexer, needs a new token, and its meaning is hard for readers to guess.
- Alternative: `>a` (`Time in >0 s`). Easy to read, but the form combined with an upper end (`>0 s..10 min`) falls outside the range grammar, and the rules split in two.

### D74. Where the lower-end-excluded form can be used [decided]

- **Decision:** As recommended, the lower-end-excluded form can only be used in the range after a type's `in`, and a new grammar rule `type_range` is set up. It is not added to ranges in expressions or to quantity range literals. (2026-11-02, confirmed by the user)
- **Recommendation**: Only in the range after a type's `in` (kitchen settings, build-time options, recipe arguments, types of action arguments). Not added to ranges in expressions or quantity range literals (`15..20 min`). Set up a new grammar rule `type_range` (`core/grammar.ebnf` and `core/src/rules.def`), and do not change `range`, which is shared with expressions.
  - Reason: It fits inside the current spec (`docs/concept.md` §4.2) that range constraints belong to types, and avoids bringing "end not included" into expression evaluation (interval arithmetic). It also does not change how `<` is read in expressions.
- Alternative: Add it to ranges in expressions too. Values like `let t = 0 s<..10 min` could be written, but interval arithmetic, comparison and display would all need to handle open ends, and this is not needed to solve the Issues.

### D75. Code and name of the new diagnostic [decided]

- **Decision:** As recommended, add `TYP021 nonPositiveAmount` (error) at the end of `diag_codes.def`. The arguments are `[材料の名前, 書いた量の文字]` (ingredient name, text of the written amount), the phrase key is `diag-non-positive-amount`, and the argument kinds are `['name']`. (2026-11-02, confirmed by the user)
- **Recommendation**: `TYP021 nonPositiveAmount` (error). Append it to the end of `core/src/diag_codes.def` (existing numbers do not change; `G-CORE-DIAG-APPEND`). There are 2 arguments, `[材料の名前, 書いた量の文字]` (ingredient name, text of the written amount; for example `["water", "-100 ml"]`). The phrase key is `diag-non-positive-amount`, and the argument kinds in `renderers/lib/names.mjs` are `['name']` (same as `unusedValue`).
  - Japanese text (draft): `{ $arg1 }の量 { $arg2 } は 0 より大きくしてください`. English text (draft): `The amount { $arg2 } of { $arg1 } must be more than zero`.
- Alternative: Name it `amountNotPositive`. The word order is natural, but existing names (`vagueAmount`, `servingsUnknown`) use "adjective + noun" order, and the recommendation matches them better.
- Alternative: Use the category `LIN` (same as the shortage `insufficient`). `LIN` is the category for consumption checks (double use, shortage); an error in the written value itself fits `TYP` (same as `vagueAmount`).

### D76. The value used to judge ingredient amounts, and overlap with servings errors [decided]

- **Decision:** As recommended, judge an ingredient amount by the written value (for `per serving`, before multiplying by the servings). When the servings are out of range, do not add `TYP021` to amounts whose expression contains `servings`, nor `TYP020` to the totals of `per serving`. `0 g per serving` is `TYP021` even when the servings are out of range. (2026-11-02, confirmed by the user)
- **Recommendation**: Judge by the value of the written expression (for `per serving`, before multiplying by the servings). If the lower end is zero or less, it is an error. Give it even when the servings are not decided (`TYP020`), because the written value alone is enough to judge. A `per serving` amount is judged before multiplying by the servings, so it does not depend on servings errors (even when the servings are out of range, `0 g per serving` is `TYP021` and `100 g per serving` gets no diagnostic). When the servings are out of range (D77's `TYP012` was given), treat the servings as unknown, and do not add `TYP021` to amounts whose expression contains `servings` (`150 ml * servings`), nor `TYP020` to the totals of `per serving`.
  - Reason: The error is in the written amount, and the diagnostic can show the written amount as its argument. Giving a diagnostic for an amount that became 0 because of a servings error would give 2 or more diagnostics for the same cause (an audit finding; it was confirmed that `-s 0` makes `150 ml * servings` into `0 ml`).
- Alternative: Judge by the total after multiplying by the servings. This sees the same value as `150 g * servings`, but the servings error and the amount error diagnostics overlap, and it cannot judge when the servings are not decided.

### D77. Diagnostic for out-of-range servings, and the rule of one diagnostic per value [decided]

- **Decision:** As recommended, out-of-range servings use the existing `TYP012`, and the same value gets only one. If the options side is wrong, the dish header side is not checked. If the header declares its own range, check only with that range. (2026-11-02, confirmed by the user)
- **Recommendation**: Use the existing `TYP012 rangeViolation`. The arguments are `["servings", 書いた値, 範囲の文字]` (written value, range text). The same value gets only one diagnostic. Specifically:
  - Servings that come from the options (`-s`, `--options`) are checked on the options side (the type of `Options.servings`, `Count? in 1..`, S741). If the options side is wrong, the dish header side is not checked again. If the options side passes, it is also checked against a range the header declares itself (`servings: Count in 1..20`).
  - The header's default value (when there are no options) and the arguments of `make` are checked only against the header's own range if it declares one, and against `1..` if it does not.
  - Now `-s 3ml` gives the same `TYP001` on 2 lines (§3.7). With this rule, the dimension error also becomes 1 line.
  - Reason: The servings have a declaration, the type of `Options.servings`, so this can be read as a range violation. It needs fewer phrase and registration additions than a new diagnostic.
- Alternative: Add a diagnostic just for servings (for example `TYP022 servingsNotPositive`). The text could be specific, like "the servings must be 1 or more", but it adds one more registration, and the same overlap rule is still needed.
- Alternative: Allow the overlap (give it on both the options and the header). Simple to implement, but like the current duplicate `TYP001`, one error gives 2 lines.

### D78. Whether to check the default values of action arguments [decided]

- **Decision:** As recommended, this plan does not check the default values of action arguments (out of scope). (2026-11-02, confirmed by the user)
- **Recommendation**: Not checked in this plan (out of scope). A default value is an error in the action definition, not in the call. Checking it at each call would give the same definition error once per call. That the standard library defaults (`30 min` for `soak`, `3 mm` for `cut`) are in range is checked with the golden `bounds/soak-default`.
- Alternative: Do the same check on calls that use the default, and put the diagnostic at the default value's position. This also finds wrong defaults in user-defined actions (`time: Time = 3 ml`), but diagnostics at the same position repeat.
- Alternative: Check once when reading the action definition. No repetition, but it adds the work of a new path in the definition check (level 0).

### D79. How the JSON type list shows a range whose lower end is not included [decided]

- **Decision:** As recommended, in the JSON type list add `"minExclusive":true` only when the lower end is not included, and add it to the schema as an optional boolean. `min` stays the text of the lower end. (2026-11-02, confirmed by the user)
- **Recommendation**: In the JSON type list (the type fields of `ozen check --format json --emit types`), add `"minExclusive":true` only when the lower end is not included. When it is included, do not output the key. Add it to `schemas/result.schema.json` as an optional boolean. The text of `min` stays the lower end's value (`0 s`).
  - Reason: The current output (golden `api/types-json` and others) does not change, except for the `min` of `Options.servings` (S741). The settings screen that uses `min` (`renderers/lib/settings.mjs`) only passes it to the HTML `min` attribute, which cannot express an open end, so it keeps passing the lower end as now.
- Alternative: Output `"minExclusive":false` on every type field. Readers would not need to branch, but every golden that contains type fields would change.
- Alternative: Do not output it in JSON. Information is lost, and tools that only look at `min` would wrongly think 0 is allowed.

### D80. Handling of `G-RF-GOLDEN-COUNT` in the syntax plan [decided]

- **Decision:** As recommended, change `G-RF-GOLDEN-COUNT` into the form "all goldens that were at `readme-start` are still there" and "the count at `readme-start` is N0 + 85", and add one line with the reason to §9 of the syntax plan. (2026-11-02, confirmed by the user)
- **Recommendation**: Change `G-RF-GOLDEN-COUNT` into two checks: "all goldens that were at the tag `readme-start` are still there (0 goldens removed)" and "the count at `readme-start` is N0 + 85". The first is the real check; the second is kept as a record that does not change, because it is the content of the tag. Also add one line with the reason for the change to §9 of the syntax plan (the same form as the precedent where the syntax plan corrected the count added in P8).
  - Reason: The current form (the current count is exactly N0 + 85) fails as soon as a later plan adds a single golden. This plan adds 72 goldens (§5.2). It keeps what the syntax plan wanted to check (that the 85 added goldens have not disappeared), and does not depend on the goldens of later plans.
- Alternative: Make it "the current count is N0 + 85 or more". The smallest change, but it cannot notice when the syntax plan's goldens disappear and other goldens are added.
- Alternative: Make it "N0 + 85 + the number added by this plan". Exact, but it would still need fixing for every later plan.

### D81. Where work happens and the base of the range [decided]

- **Decision:** As recommended, work on `main`, put the tag `qc-start` in P0-2, and count the range `QC=qc-start..HEAD`. Push without asking, and run `G-REPO-NOREPLY` every time before that. Temporary commits for mutation checks are made in a temporary worktree and removed, and are not pushed. (2026-11-02, confirmed by the user)
- **Recommendation**: Work on `main` (`CLAUDE.md`). In P0-2, put the tag `qc-start` on `main`, and count this plan's diff range with `QC=qc-start..HEAD`. Following the user's instruction in memory (no push confirmation is needed in Ozen), push without asking each time. Run `G-REPO-NOREPLY` every time before a push. When another work's commit enters the range, name it in §9 and exclude it, as in D68 of the README plan. Temporary commits for mutation checks are made inside a temporary worktree and removed with `git reset --hard`; they do not go into `main` (they are not pushed).
- Alternative: Create a work branch `quantity-checks` and merge it into `main` with `--no-ff` after the final gate. This separates it from other work, but goes against "work only on `main`" in `CLAUDE.md`, and does not match the idea of existing goal ranges (`plan-start..HEAD`).

### D82. Where the plan file and the goals go [decided]

- **Decision:** As recommended, put this file as a new plan, add `PLAN_QC`, `QC` and `goal_G_QC_*` to `scripts/goals.sh`, and add one line to the Plan section of `CLAUDE.md`. Existing plans are not rewritten, except for the fixes in D80 and D87. (2026-11-02, confirmed by the user)
- **Recommendation**: Put this file as a new plan. Add `PLAN_QC`, `QC` and `goal_G_QC_*` to `scripts/goals.sh` (`OZ` already exists). Add one line to the Plan section of `CLAUDE.md` with this plan's name, goal prefix and range tag (the same form as the line for the README plan). Existing plans are not rewritten, except for the fixes in D80 and D87.
- Alternative: Add a Phase to the README plan. The goals would be in one place, but completed plans and ongoing work could no longer be told apart.

### D83. Version [decided]

- **Decision:** As recommended, the version stays at `0.1.0`. (2026-11-02, confirmed by the user)
- **Recommendation**: Keep it at `0.1.0`. There is no published version yet (tags, distributions), this is a change that fixes errors, and the output of correctly written recipes does not change.
- Alternative: Raise it to `0.2.0`. The version would show that recipes that used to pass now give errors (input acceptance becomes narrower), but it adds the work of fixing expectations that contain the version string (goldens that contain `ozenVersion` in JSON, and others).

### D84. How to close the Issues [decided]

- **Decision:** As recommended, after the final gate and the push, comment on each Issue with the SHA of the fix and the `ozen check` output after the fix, and close it. Show the wording to the user and get confirmation before posting. In #2, also mention the wrongly pasted reproduction code. (2026-11-02, confirmed by the user)
- **Recommendation**: After the final gate and the push, use `gh issue comment` to write briefly on each Issue the SHA of the fixing commit and the `ozen check` output after the fix (for the reproduction code), and close it with `gh issue close`. Show the comment wording to the user and get confirmation before sending (because it is an external post). In the comment on #2, mention that the reported reproduction code was the same as #1's, and that a reproduction built from the output was put in `bounds/issue2`.
- Alternative: Write `Fixes #1` in the fixing commit's message and let the push close it automatically. Less work, but it closes on the push, so it would close before the final gate. It also tells the reporter less about the behavior after the fix.
- Alternative: Do not close them and leave it to the user.

### D85. Evaluating a range with units on both ends [decided]

- **Decision:** As recommended, evaluate a range with units on both ends as an interval when both ends are known quantities with the same dimension, and give `TYP001` if the dimensions differ. Fixed in this plan. (2026-11-02, confirmed by the user)
- **Recommendation**: Evaluate a range with units on both ends (`3 ml..5 ml`, `-5 min..-1 min`) as an interval when both ends are known quantities with the same dimension. If the dimensions differ (`3 ml..5 g`), give `TYP001`. With this, the #1 check and the #2 range and amount checks also apply to this way of writing.
  - Reason: Now expression evaluation (`eval_inner`) has no evaluation for range nodes, so both `fry(egg, 3 ml..5 ml)` and `simmer(x, -5 min..-1 min)` become unknown values and slip past the plan's check (unknown values are not checked, S704) (§3.7). In kitchen settings, writing units on both ends like `10 cm..40 cm` is common, so users are likely to write it. The current behavior where writing `0.5 ml..2 ml` for an ingredient gives no amount and shows "as needed" is also fixed, since it has the same cause.
- Alternative: Do not handle it in this plan and make it separate work. The change would be small, but the symptoms in the Issues (a time with the wrong unit, a negative time) would remain through another way of writing.

### D86. Quantity arguments of `make` in a menu [decided]

- **Decision:** As recommended, check the quantity arguments (both positional and named) that `make` in a menu passes to a dish against the argument declarations in the dish's header (dimension with `TYP001`, range with `TYP012`). (2026-11-02, confirmed by the user)
- **Recommendation**: Check the quantity arguments (both positional and named) that a menu passes to a dish, as in `make soup(t: 3 ml)`, against the argument declarations in the dish's header. Dimension with `TYP001`, range with `TYP012`. Use the same function as for action arguments (§4.12). The servings (`servings`) are also checked on this path (D77).
  - Reason: Now non-ingredient arguments of `make` are not checked against types (`eval_make` only binds the value, and `eval_param` returns without checking an already bound value; §3.7). It is the same kind of error as #1, and the servings in `make soup(servings: 0)` also go through this path.
- Alternative: Check only the servings, and make the other quantity arguments separate work. A smaller change, but `make soup(t: 3 ml)` would still pass.

### D87. Range goals of completed plans [decided]

- **Decision:** As recommended, put the tag `readme-end` on the README plan's last commit `52a6ef1`, and close `RD` as `readme-start..readme-end`. Add one line with the reason to §9 of the README plan. `G-RF-SEPARATION` is also kept as a prerequisite goal in this plan. (2026-11-02, confirmed by the user)
- **Recommendation**: Close the README plan's range (`RD=readme-start..HEAD`) with the tag `readme-end` put on the README plan's last commit `52a6ef1` (`readme-start..readme-end`), so that `G-RD-SCOPE` and `G-RD-ADD-ONLY` only look at that range. Also add one line with the reason to §9 of the README plan. The syntax plan's `G-RF-SEPARATION` (do not put expectation `.out` files and the implementation in the same commit) is included in the prerequisite goals as a rule this plan also keeps.
  - Reason: `G-RD-SCOPE` already fails because of other work after the README plan (the OGP commits `2332ca6` and `8db17e6`) (2026-11-02, `files outside scope: got '2'`). `G-RD-ADD-ONLY` would fail when this plan rewrites lines of `goals.sh` in D80. Both happen because they keep counting commits after the plan ended; what the README plan wanted to check does not change when the range is closed.
- Alternative: Remove the range goals of completed plans from the prerequisite goals, and only record their pass or fail in P0-2. Nothing to fix, but failed goals would stay in the list and be hard to tell apart from real failures.

---

## 3. Results of checking the current state (2026-11-02, `8db17e6`)

### 3.1 Action calls, and checking values against types

- Actions are called in `call_action` (`core/src/sema.c:4046`). It assigns arguments by position and name, and evaluates default values for arguments not passed. After that, for ingredient arguments, it checks states (`TYP006 stateRequired`, `TYP011 aliasCandidates`) and `where` (`TYP010`). There is no step that checks quantity arguments (`V_QTY`) against the declared type.
- Values are checked against types in `check_value` (`core/src/sema.c:2640`). If the declaration is a dimension type, it reads the value with `typed_qty`, gives `TYP001` if the dimension differs (arguments `[値の次元, 宣言の型の名前]`: the value's dimension and the name of the declared type), and gives `TYP012` if it is outside the range (the part after `in` returned by `base_type`) (arguments `[名前, 値の文字, 範囲の文字]`: name, value text, range text). The range check is a closed interval, and a value is outside if any part of its interval is outside (`v.lo < lo` or `v.hi > hi`).
- `check_value` is built to receive a source node (where the value was written) and read it again; it cannot receive an evaluated value (such as a quantity passed through a variable). It is called for recipe arguments (`sema.c:6133`), kitchen setting fields (`sema.c:2847`) and build-time option fields (`sema.c:3163`). `sema.c:2669` is a recursion inside `check_value`.
- When the declaration is `Count`, `typed_qty` turns `2 pcs` (a quantity with the Count dimension) into a dimensionless quantity before comparing. The dimension name of a number without a unit is `Dimensionless` (golden `kitchen/unitless-capacity.out`).
- As with state checks, the diagnostic position is pointed at where the argument was written, through `arg_site`. For an argument inserted into a macro (`$t`), it is the caller's position with a trail; for a quantity written in the macro body, it is the body's position with the expansion history (trail) (golden `macro/typecheck-after-expand.out`).
- `base_type` (`sema.c:2605`) removes the optional (`OptionalType`) and then looks at the range (`RangedType`). In the grammar, `Count? in 1..` becomes the nesting `RangedType(OptionalType(Count), …)`, so after removing the range the `OptionalType` is not treated as a type name, and `check_value` returns without checking anything (fixed in S742).
- A pipe (`x |> f(a)`) is turned by the parser's `pipe_into` (around `core/src/parse.c:871`) into an ordinary call with the left side inserted as the first argument.
- For composite actions (actions with `steps`), `call_composite` (`sema.c:3998`) evaluates the inner calls. It does not raise the mechanism that suppresses inner diagnostics (`quiet`).

### 3.2 Range grammar and tokens

- A type range is `type := type_union [ 'in' range ]`, `range := sum [ '..' [ sum ] ] | '..' sum` (`core/grammar.ebnf:98`, `141`). The parser reads the part after `in` in `type()` (around `core/src/parse.c:1110`) with `range()` (`parse.c:771`), and builds `Range`, `RangeFrom` and `RangeTo` nodes. A number-only range (`1..20`) becomes a `NumberRange` node.
- `<` (`OZNI_T_LT`) and `..` (`OZNI_T_DOTDOT`) are separate tokens (`core/src/lex.c:239`). There is no single token `<..`.
- For `0 s<..` after a type's `in`, `range()` returns `0 s` as a non-range value, and the caller gives `SYN003 args=["<"]` at `<` (confirmed by running it in the audit). `3 min<..5 min` inside an expression is read as `Binary <` (`3 min` and `RangeTo`), and `check` gives `result ok`.
- Ranges are output in the JSON type list (`ozen check --format json --emit types`: `types` at `core/src/defs.c:737` lists the setting types of `Kitchen`, `Options` and the kinds of equipment and vessels, and `put_field` at `defs.c:597` outputs each field's lower and upper end text as `min` and `max`; it only handles `Range`, `RangeFrom` and `RangeTo`, and does not output `NumberRange`), and in the type text of action arguments (the `type` of `params` in `--emit signatures` and others; the whole range is output, like `Time in 0 s<..`). `min` and `max` are used by `renderers/lib/settings.mjs:26` as the `min` and `max` attributes of the number inputs in the settings screen.
- Grammar rules are in `core/src/rules.def` (`OZNI_RULE(range)` and others), and `G-CORE-GRAMMAR` (`scripts/check-grammar-coverage.py`) confirms that every rule name in `grammar.ebnf` is passed through by a golden.

### 3.3 Quantity arguments of standard library actions

In `stdlib/prelude.ozen`, the arguments that take quantities are the following 8 (2026-11-02). No action argument takes a temperature (temperature is only `OvenSpec.maxTemp: Temperature?`).

| Action | Argument | Current type |
| --- | --- | --- |
| `soak` | `time` | `Time = 30 min` |
| `cut` | `size` | `Length = 3 mm` |
| `fry` | `time` | `Time` |
| `simmer` | `time` | `Time` |
| `boil` | `time` | `Time` |
| `parboil` | `time` | `Time` |
| `chill` | `time` | `Time` |
| `rest` | `time` | `Time` |

### 3.4 Reproduction (2026-11-02, `build/core-gcc-release/ozen check`)

| Input | Current result |
| --- | --- |
| `fry(egg, 3 ml)` (the reproduction code of #1) | `result ok`; the recipe text says "Fry the egg over medium heat 3 ml." |
| `potato \|> peel \|> cut(slice, 3 g)` | `result ok` |
| `fry(egg, 3)` | `result ok` |
| `water: -100 ml`, `potato: Potato 0 pcs`, `boil(water, -300 s)`, `cut(slice, -3 mm)`, `simmer([x, y], 0 min)`, `recipe a(servings: 0)` | `result ok`. The text says "water -100 ml", "potatoes 0 pieces", "Boil the water -300 s.", "Slice (-3 mm)", "Simmer over low heat for 0 hours" |
| `recipe soup(servings: Count = 0)` | `result ok` |
| `make soup(servings: 0)` | `result ok` |
| `ozen check -s 0`, `-s -1` | `result ok` |
| `recipe a(servings: -2)` | `SYN003 unexpectedToken` (a negative number cannot be written as a header default value) |
| `recipe soup(servings: Count in 1..20 = 0)` | `TYP012 args=["servings", "0", "1..20"]` (still given now) |
| `salt: -1..2 g` | `result ok`; the text says "salt -2 to -1 g" (read as `-(1..2 g)`) |

The "reproduction code" in the #2 report is the same as #1's, and does not match the "actual result" (water -100 ml, boil -300 s). We took it as a pasting mistake and built a reproduction from the output (`bounds/issue2`, S750).

### 3.5 What to touch when adding a diagnostic

The list read from the commits that added `LIN005 unusedValue` (`738a452`, `5e54d49`, `631e097`):

- `core/src/diag_codes.def` (append at the end; `G-CORE-DIAG-APPEND` confirms 0 deleted lines)
- `core/tests/diag-names.txt` (`G-CORE-DIAG-REGISTRY`)
- `diag-<名前の kebab>` (the name in kebab case) in `phrases/en.ftl` and `phrases/ja.ftl` (`G-STD-PHRASES`, `G-REND-PHRASES-RENDER`)
- `ARG_KINDS` in `renderers/lib/names.mjs` (argument kinds; same form as `unusedValue: ['name']`)
- The table in `docs/concept.md` §6.1 and `core/tests/severity-examples.txt` (`G-TYPE-SEVERITY-TABLE`)
- The expectations of goldens that output the list of diagnostics (`api/local-not-listed`, `api/sarif-mapping` and others)

### 3.6 How tests and goals work

- A golden is a pair of `tests/golden/<分類>/<名前>.ozen` and `.out` (category and name). The `// args:` on the first line of `.ozen` holds the command arguments, and `.out` holds the diagnostic, `result` and `exit` lines. `scripts/run-golden.sh <ozen> <フォルダ>` (a folder) prints `golden: <n> passed, 0 failed, 0 timed out` (it can also run one folder; confirmed with `tests/golden/kitchen` on 2026-11-02). Build-time options are written as `kitchen today : Options { … }` and chosen with `--options today` (golden `kitchen/range-open-ended`). For `-s`, the CLI builds `kitchen cli : Options { servings: … }` and passes it (around `cli/src/main.c:599`).
- For a commit that adds expectations, `G-REPO-RED-FIRST` (range `plan-start..HEAD`) confirms that the expectation fails (red) at that commit. For goldens that pass from the start, there is a precedent of putting them in a separate commit and writing the reason in `scripts/red-first-exceptions.txt` (such as `ingredient-dimension-ok` in the syntax plan).
- `G-RF-SEPARATION` (range `refresh-start..HEAD`) confirms that expectation `.out` files and the implementation (`core/src`, `stdlib` and others) are not in the same commit.
- `G-RF-GOLDEN-COUNT` confirms that the current number of goldens is exactly `RF_N0 + 85` (= 448). The count on 2026-11-02 is 448 (D80).
- `G-RD-SCOPE` already fails as of 2026-11-02 (D87).
- If test names in the plan's coverage table are written in the form `cgolden/<分類>/<名前>` or `rtest/<名前>` (category and name), `scripts/check-plan-tests.sh` confirms they exist, and `--mutations` counts the records of mutation checks.

### 3.7 Other paths confirmed in the audit (2026-11-02)

- Ranges with units on both ends are not evaluated: `fry(egg, 3 ml..5 ml)` is `fry(egg, ?)` in `debug steps`, with `result ok`. The ingredient `sugar: -5 ml..-1 ml` gets no amount. `0..2 min`, a number-only range with a unit, is evaluated as `0..120 s` (D85).
- Non-ingredient arguments of `make` are not checked against types: both `make soup(t: 3 ml)` (`t: Time`) and the positional `make soup(0)` give `result ok` (D86).
- The servings from the options are checked twice: passing `-s 3ml` to a dish whose header has `servings: Count in 1..20` gives the same `error TYP001 dimension <options>:2:13-2:16 args=["Volume", "Count"]` on 2 lines (the check of the options field and the check of the header argument read the same node; D77).
- Amounts in expressions that use the servings: with `-s 0`, `dashi: 150 ml * servings` becomes `0 ml` (D76).
- Mixing up non-quantity values and quantities (`cut(slice, dice)`, `cut(3 mm)`, `fry(egg, 3 min, 3 ml)`) all give `result ok` (out of scope).
- `make gohan(servings: 0)` for a dish that has no `servings` drops the argument with the unmatched name and becomes `TYP020` (out of scope).
- `repeat 3 { peel(water) }` gives `TYP006` at the same position on 3 lines (out of scope).
- `Time in 0 s`, a range with no `..`, is accepted as a `RangedType`, and the range is ignored (out of scope).

---

## 4. Behavior spec

Each checkable claim is one item with one claim, and starts with `**S<n>**`.

### 4.1 Quantity arguments of actions (#1, D69)

- **S695** When the declared type of an action argument is a dimension (`Mass`, `Volume`, `Length`, `Time`, `Temperature`, `Count`, or a dimension added with `dimension`) and the passed value is a known quantity, give a `TYP001 dimension` error if the value's dimension differs from the declaration. The arguments are `[値の次元, 宣言の型の名前]` (the value's dimension and the name of the declared type; for example `fry(egg, 3 ml)` gives `["Volume", "Time"]`).
- **S696** A number without a unit (`fry(egg, 3)`) is a dimensionless quantity, and against a `Time` declaration it is the S695 error (arguments `["Dimensionless", "Time"]`).
- **S697** When the declaration is `Count`, both `2 pcs` and `2` pass, and `2 g` is an error.
- **S698** Arguments passed by name (`fry(egg, time: 3 ml)`) get the same check.
- **S699** Calls passed through a pipe (`egg |> fry(3 ml)`) get the same check, and the diagnostic position is `3 ml`.
- **S700** The diagnostic position is the argument where the value was written. When passed through a variable (`fry(egg, t)` after `let t = 3 ml`), it appears at the position of the argument `t`.
- **S701** The diagnostic for a quantity written in a macro body (`fry($x, 3 ml)` in the body) appears at the body's position, with the expansion history (trail).
- **S758** The diagnostic for a quantity inserted into a macro (passing `3 ml` to `fry($x, $t)` in the body) appears where the caller wrote `3 ml` (no trail if it is a top-level call).
- **S702** Calling a composite action (an action with `steps`) is also checked against that action's declaration.
- **S759** Calling the composite action `quick(x: T, t: Time) { steps { fry(x, t) } }` as `quick(egg, 3 ml)` gives only one `TYP001`, at the caller's `3 ml` (the check of the inner `fry(x, t)` does not add another).
- **S703** Arguments of a dimension added with `dimension` (`by: Angle`) get the same check.
- **S704** Arguments whose value is not decided at evaluation time (unknown quantities) are not checked.
- **S705** When an argument is left out and the default value is used, the default value is not checked (D78). Even a wrong default in a user action (`time: Time = 3 ml`) gives no diagnostic.
- **S706** Even with a dimension error, evaluation continues, and later errors in the same recipe (such as `TYP006` for cutting without peeling) are also given.
- **S707** Values with the correct dimension (`3 min`, `1 h 30 min`, `3..5 min`, `2 * 90 s`, `time: 3 min`, `heat: high`, `cut(slice, 3 mm)`) give no new diagnostic (non-quantity arguments `heat` and `style` get no new diagnostic).
- **S708** The reproduction code of Issue #1 (`fry(egg, 3 ml)`) gives one `TYP001`, and the exit code of `ozen check` is 1.
- **S788** Default values of composite action arguments are not checked in inner calls either (D78). Calling `chill(x, time)` twice inside `quickChill(x: T, time: Time = 3 ml)` gives no diagnostic (added in the P4-2 audit).
- **S789** When a recipe argument's default value does not fit the type (`boil(dashi, t)` inside `recipe soup(t: Time = 3 ml)`), the only diagnostic is one `TYP001` at the header's default value position (an argument that failed the type is not bound; added in the P4-2 audit).

### 4.2 Ranges with units on both ends (D85)

- **S760** A range with units of the same dimension on both ends (`3 min..5 min`) is evaluated as an interval, and appears as an interval (`180..300 s`) in `debug steps`.
- **S761** `fry(egg, 3 ml..5 ml)` is `TYP001` (arguments `["Volume", "Time"]`).
- **S762** A range whose ends have different dimensions (`3 ml..5 g`) is `TYP001`.
- **S763** The ingredient `sugar: 0.5 ml..2 ml` has the amount `0.5..2 ml`, and the recipe text shows a range, not "as needed".
- **S786** When the lower end of a range with units on both ends is larger than the upper end (`5 min..3 min`), it becomes an interval with the ends swapped (`180..300 s`), the same as a number-only range (`5..3 min`) (added in the P4-2 audit).

### 4.3 Quantity arguments of `make` in a menu (D86)

- **S764** `make soup(t: 3 ml)` (the dish header has `t: Time`) gives `TYP001` at `3 ml`.
- **S765** `make soup(t: 0 s)` (the dish header has `t: Time in 0 s<..`) gives `TYP012` at `0 s`.

### 4.4 Ranges whose lower end is not included (D70, D73, D74, D79)

- **S709** Writing `a<..` after a type's `in` allows only values greater than a. Exactly a is out of range.
- **S710** Writing `a<..b` allows values greater than a and up to b (the upper end is included).
- **S711** Putting a space between `<` and `..` (`0 s< ..`) is a syntax error (`SYN003`).
- **S712** An out-of-range value is the existing `TYP012 rangeViolation`, and the third argument is the text of the written range (`0 s<..`).
- **S713** An interval value (`0..2 min`) is out of range if its lower end is at or below the range's lower end (as with the current closed interval, it is outside if any part is outside).
- **S714** The check for the current ranges that include the lower end (`0 s..`) does not change (`0 s` is in range).
- **S715** Outside type ranges (in expressions), how `3 min<..5 min` is read does not change (`debug ast` is the same as now).
- **S766** A range whose lower end is not included also works on fields of user equipment setting types (`type ScaleSpec : EquipmentSpec { maxLoad: Mass in 0 g<.. }`), and `maxLoad: 0 g` in the kitchen is `TYP012`.
- **S767** A number-only lower end (`Count in 0<..`) is judged the same way (`0` is out of range, `1` is in range).
- **S768** The empty range `1<..1` has no values in it, and `1` is `TYP012` (no diagnostic is added for errors in the range itself).
- **S790** Writing a range as the lower end of a type range (`Time in 0..1 min<..`) gives `SYN003` at `<` (added in the P4-2 audit).
- **S716** In the type fields of `ozen check --format json --emit types`, a range whose lower end is not included gets `"minExclusive":true`, and `min` is the lower end's text (`0 g`). Ranges that include the lower end and fields without a range have no `minExclusive` key.
- **S717** `schemas/result.schema.json` allows `minExclusive` on type fields as an optional boolean.
- **S769** In `--emit signatures`, the type text of standard action arguments is output with the whole range (`Time in 0 s<..`).
- **S718** The grammar rule `type_range` is in `core/grammar.ebnf` and `core/src/rules.def`, and is passed through by a golden (`G-CORE-GRAMMAR`).
- **S719** The fuzzing dictionary (`core/fuzz/ozen.dict`) has `"<.."`.
- **S794** In the type fields of `--emit types`, the upper end of `a<..b` is output as `max` (`minLoad: Mass in 0 g<..5 kg` gives `"min":"0 g","minExclusive":true,"max":"5 kg"`; added in the P4-2 audit).

### 4.5 Ranges of standard actions (D70)

- **S720** The type of `time` in `soak`, `fry`, `simmer`, `boil`, `parboil`, `chill` and `rest` is `Time in 0 s<..` (the default `30 min` of `soak` stays as is).
- **S721** The type of `size` in `cut` is `Length in 0 mm<..` (the default `3 mm` stays as is).
- **S722** A zero time (`simmer(x, 0 min)`) is `TYP012`.
- **S723** A negative time (`boil(water, -300 s)`) is `TYP012`.
- **S770** A negative time with units on both ends (`simmer(x, -5 min..-1 min)`) is `TYP012`.
- **S724** Zero and negative sizes (`cut(slice, 0 mm)`, `cut(slice, -3 mm)`) are `TYP012`.
- **S725** Small positive values (`fry(egg, 1 s)`, `cut(slice, 0.5 mm)`) and default values (`soak(rice)`, `cut(slice)`) give no diagnostic.
- **S726** No range is put on temperature arguments. Passing `-18 degC` to the user action `freeze(x: T, temp: Temperature)` gives no diagnostic. The standard library has no temperature range (`Temperature in`, `Temperature? in`).
- **S727** A value with a different dimension (`fry(egg, -3 ml)`) gives only `TYP001`, with no `TYP012` on top.
- **S771** Calling the composite action `quick(x: T, t: Time) { steps { fry(x, t) } }` (the outer `t` has no range) as `quick(egg, 0 s)` gives only one `TYP012` from the inner `fry`'s range, at the caller's `0 s`.
- **S787** Even when the same argument is used twice inside a composite action (`fry(x, t)` and `rest(fried, t)` inside `twice(x: T, t: Time)`), `twice(done, 0 s)` gives only one `TYP012`, at the caller's `0 s` (added in the P4-2 audit).

### 4.6 Ingredient amounts (D71, D75, D76)

- **S728** If the lower end of an ingredient amount is zero or less, give `TYP021 nonPositiveAmount` (error). The arguments are `[材料の名前, 書いた量の文字]` (ingredient name, text of the written amount), and the position is where the amount was written.
- **S729** Exactly zero (`egg: Egg 0 pcs`) is also an error.
- **S730** A negative amount (`water: -100 ml`) is an error.
- **S731** A range amount is judged by its lower end. `0..2 g`, `-1..2 g` (read as `-(1..2 g)`, lower end -2) and `-0.5..0.8 g` (read as `-(0.5..0.8 g)`, lower end -0.8) are errors, and `0.5..2 g` passes.
- **S772** A range amount with units on both ends (`sugar: -5 ml..-1 ml`) is also an error (it is evaluated after D85).
- **S732** A `per serving` amount is judged by the written value (in a dish for 2, `0 g per serving` is an error and `100 g per serving` passes).
- **S733** Even for a `per serving` amount whose servings are not decided (`TYP020`), `TYP021` is also given if the written value is zero or less.
- **S773** In a dish whose servings are out of range (`servings: 0`), `100 g per serving` and `150 ml * servings` get no `TYP020` or `TYP021`.
- **S785** Even in a dish whose servings are out of range, `0 g per serving` gets `TYP021` (it is an error in the written value, a different cause from the servings error).
- **S734** `[any amount]` is not covered (only the `TYP003` warning).
- **S735** Ingredients with no written amount are not covered.
- **S736** When the dimension of the amount is wrong (`-1 min`), only `TYP001` is given, with no `TYP021` on top.
- **S737** An amount written as an expression (`100 g - 100 g`) is judged by its evaluated value.
- **S738** Positive amounts (`0.5 g`, `1 pcs`, `0.5..2 g`) give no diagnostic.
- **S739** `TYP021` is registered (after `SCH011` in `diag_codes.def`, and in `diag-names.txt`), and has Japanese and English phrases (`diag-non-positive-amount`; both use `{ $arg1 }` and `{ $arg2 }`) and argument kinds (`['name']` in `names.mjs`).
- **S740** The error examples in `docs/concept.md` §6.1 and `core/tests/severity-examples.txt` include "amount of zero or less".

### 4.7 Servings (D72, D77)

- **S741** The type of `Options.servings` is `Count? in 1..`. In `--emit types`, `servings` of `Options` has `min` of `"1"`.
- **S774** The settings screen (`settingsSpec` in `renderers/lib/settings.mjs`) puts `min` 1 on the servings input, from `Options.servings` in `--emit types`.
- **S742** `check_value` checks an optional type with a range (`Count? in 1..`). Passing `0` to `n: Count? in 1..` in a user setting type gives `TYP012`.
- **S743** An untyped `servings: 0` in a recipe header is `TYP012` (arguments `["servings", "0", "1.."]`).
- **S744** A typed-only `servings: Count = 0` is also `TYP012` (the same arguments).
- **S745** `servings: Count in 1..20 = 0`, which declares its own range, gives only one `TYP012` with the declared range (the third argument is `1..20`).
- **S746** `make soup(servings: 0)` in a menu gives one `TYP012` at `0`.
- **S775** The positional `make soup(0)` (the dish's first argument is `servings`) also gives one `TYP012`.
- **S776** `menu dinner(servings: 0)` in a menu header is also `TYP012`.
- **S747** Passing `ozen check -s 0` and `-s -1` to a dish without `servings` gives one `TYP012` on the options side (range text `1..`).
- **S777** Passing `-s 0` to a dish whose header has `servings: Count in 1..20 = 4` gives only one `TYP012` (`1..` on the options side).
- **S778** Passing `-s 25` to the same dish gives only one `TYP012` (`1..20` on the header side).
- **S779** Passing `-s 3ml` to the same dish gives only one `TYP001` (now 2 lines).
- **S748** In a dish without `servings`, choosing the build-time options `kitchen today : Options { servings: 0 }` with `--options today` gives one `TYP012`.
- **S749** Header servings of 1 (`servings: 1`) give no diagnostic.
- **S780** `-s 1` gives no diagnostic.
- **S791** Passing `-s 0` to a dish whose header has `servings: Count = 2` gives only the one `TYP012` on the options side, and `1 g per serving` and `150 ml * servings` get no `TYP020` or `TYP021` (D76; added in the P4-2 audit).
- **S792** The dish in `make soup(servings: 0)` in a menu is evaluated with unknown servings, and the `make` line in `debug steps` has no amount (D76; added in the P4-2 audit).
- **S793** `make soup(servings: 1)` for a dish whose header declares its own range (`servings: Count in 2..20`) gives `TYP012` with the declared range (the third argument is `2..20`; added in the P4-2 audit).

### 4.8 Issue reproductions

- **S750** The reproduction built from the output of Issue #2 (`water: -100 ml`, `boil(water, -300 s)`) gives one `TYP021` and one `TYP012`, and the exit code of `ozen check` is 1.

### 4.9 Invariants

- **S751** Every existing golden or rendering test whose expectation was corrected within this plan's range is named in §9 with a reason (added goldens are not counted).
- **S752** All existing samples (`samples/`) still pass as before (`G-SAMPLE-CHECK`).
- **S753** This plan's commits only touch the files in the list in §4.11 (commits of other work excluded in §9 are not counted).
- **S754** The version (`VERSION`) follows D83.
- **S755** `G-RF-GOLDEN-COUNT` is changed to the form in D80, and passes after this plan's goldens are added.
- **S781** `G-RD-SCOPE` and `G-RD-ADD-ONLY` are changed to the form in D87 (`readme-start..readme-end`) and pass.
- **S782** Commits that correct expectation `.out` files and commits that change the implementation are separate (`G-RF-SEPARATION`).

### 4.10 Documents

- **S756** The explanation of range constraints in `docs/concept.md` §4.2 has the lower-end-excluded forms (`a<..`, `a<..b`) and an example (`0 s<..`).
- **S783** In `docs/concept.md` §4.2, the targets of "out of range is always an error" include action arguments.
- **S784** `servings` in the `type Options` example in `docs/concept.md` is `Count? in 1..`.
- **S795** `docs/concept.md` §4.2 has the form `a<..b` and says that the upper end is then included ("the upper end is included") (added in the P4-2 audit).
- **S757** The Plan section of `CLAUDE.md` has a line with this plan's file name, `G-QC-*` and the tag `qc-start`.

### 4.11 List of things to add or change

| Kind | ID / path |
| --- | --- |
| Diagnostic | `TYP021 nonPositiveAmount` (D75) |
| Grammar | Rule `type_range` (the lower-end-excluded forms of type ranges, `a<..` and `a<..b`). `core/grammar.ebnf`, `core/src/rules.def`, `core/src/parse.c`, `core/src/ast.h` |
| Standard library | The 8 arguments in `stdlib/prelude.ozen` (§3.3) and `Options.servings` |
| Phrases | `diag-non-positive-amount` in `phrases/en.ftl` and `phrases/ja.ftl` |
| Rendering | `nonPositiveAmount` in `ARG_KINDS` of `renderers/lib/names.mjs`, `renderers/test/settings-servings-min.test.mjs` (S774) |
| Schema | `minExclusive` on type fields in `schemas/result.schema.json` |
| Fuzzing | `"<.."` in `core/fuzz/ozen.dict` |
| Core | `core/src/sema.c` (quantity checks, range evaluation, open lower ends of ranges, ingredient amounts, servings, arguments of `make`), `core/src/defs.c` (`minExclusive`) |
| Tests | `tests/golden/args/` (22 cases), `tests/golden/bounds/` (55 cases), existing goldens whose expectations are corrected (S751) |
| Documents | `docs/concept.md` §4.2, §6.1 and the `type Options` example, `core/tests/diag-names.txt`, `core/tests/severity-examples.txt` |
| Plan and goals | This file, `scripts/goals.sh`, `CLAUDE.md`, one line in `plans/ozen-syntax-refresh.md` §9 (D80), one line in `plans/ozen-readme-compact.md` §9 (D87), `scripts/red-first-exceptions.txt` (the commit of the control goldens, §5.2), `scripts/test-names-exceptions.txt` (the one-letter names of the Issue reproductions, added in P4-2) |

### 4.12 Where the code goes

| Place | What goes there |
| --- | --- |
| `core/src/sema.c` | Split the dimension and range checks of `check_value` out into a function that receives an evaluated quantity (`ozni_qty`) and a type node (the name is decided during implementation). `check_value` calls it. `call_action` calls it for each non-default quantity argument after assigning the arguments. Checks of calls inside composite actions do not repeat values already checked outside, and point the position at the caller (S759, S771). Evaluate ranges with units on both ends in `eval_inner`. `base_type` removes the nesting of range and optional in either order. The check for a range whose lower end is not included. `TYP021` in `eval_ingredients`. In `eval_make` and `eval_param`, check the arguments of `make` and the servings, and remove the duplicate check of servings from the options |
| `core/src/parse.c` | Read `<..` only after `in` in `type()` (`range()` is shared with expressions, so it does not change) |
| `core/src/ast.h` | A node kind or a mark for a range whose lower end is not included (decided during implementation and written in §9) |
| `core/src/defs.c` | `minExclusive` in `put_field` |

---
## 5. Definition of machine-verifiable goals

**Prerequisite goals** (implicitly part of every task's completion condition):

- All goals "run locally" in §5 of the syntax plan. In particular, `G-CORE-GOLDEN`, `G-CORE-GRAMMAR`, `G-CORE-DIAG-APPEND`, `G-CORE-DIAG-REGISTRY`, `G-TYPE-SEVERITY-TABLE`, `G-STD-PHRASES`, `G-REND-PHRASES-RENDER`, `G-REND-TESTS`, `G-API-SCHEMA`, `G-API-SARIF`, `G-SAMPLE-CHECK`, `G-CORE-FUZZ`, `G-REPO-RED-FIRST` and `G-PLAT-WASM` concern files this plan touches
- From the syntax plan: `G-RF-GOLDEN-COUNT` (in the form fixed by D80), `G-RF-FUZZ-DICT`, `G-RF-SEPARATION`, `G-RF-CONCEPT`
- From the README plan: `G-RD-SCOPE`, `G-RD-ADD-ONLY` (in the form fixed by D87)
- Before every push: `G-REPO-NOREPLY`
- After a push: `G-PLAT-CI`

When something fails, report which check of which goal failed.

- `QC=qc-start..HEAD`, `PLAN_QC=plans/ozen-quantity-checks.md`, `OZ=build/core-gcc-release/ozen` (`OZ` is already in `goals.sh`).
- Counts that expect 0 are judged by the number in the output, not by the exit code. Counts that use a range first make sure the tag exists (without it, `git diff` prints nothing and the check passes).
- In P0-3, the code block below becomes the functions `goal_G_QC_*` in `scripts/goals.sh`.

### 5.1 Structure and data

| ID | What it checks | Pass condition |
| --- | --- | --- |
| `G-QC-DECISIONS` | Whether D69 to D87 are settled | 19 decided, 0 open, 19 decision lines |
| `G-QC-COVERAGE` | Every S item has a row in the coverage table, and every item in §8 has a reason | Empty diff, 0 items without a reason |
| `G-QC-PLAN-NAMES` | The test names in the coverage table exist | `plan-tests: <n> names, 0 missing` |
| `G-QC-PLAN-TESTS` | There are as many mutation check records as rows in the coverage table | `mutations: <m> rows, <m> recorded` |
| `G-QC-GOLDEN-ARGS` | The goldens of §4.1 to §4.3 | 22 goldens, `golden: 22 passed, 0 failed, 0 timed out` |
| `G-QC-GOLDEN-BOUNDS` | The goldens of §4.4 to §4.8 | 55 goldens, `golden: 55 passed, 0 failed, 0 timed out` |
| `G-QC-GOLDEN-CHANGED` | S751 | `ok`, 0 corrected expectations that are not named |
| `G-QC-PRELUDE` | S720, S721, S726, S741 | 7 `time` lines, 1 `size` line, 0 temperature ranges, 1 `servings` line |
| `G-QC-DIAG` | S739, S740 | All 9 counts are 1, and the `TYP021` line comes after the `SCH011` line |
| `G-QC-SCHEMA` | S716, S717 | The count of `minExclusive` equals the count of `min`, `json: 1 files, 0 invalid` |
| `G-QC-SYNTAX` | S718, S719, S756, S783, S784, S740, S795 | All 9 counts are 1 or more |
| `G-QC-SCOPE` | S753 | `ok`, 0 excluded commits not in the range, 0 files outside the scope |
| `G-QC-VERSION` | S754 | The value from D83 |
| `G-QC-CLAUDEMD` | S757 | 1 or more |
| `G-QC-RF-COUNT` | S755 | The function of `G-RF-GOLDEN-COUNT` contains `readme-start`, and the function passes |
| `G-QC-RD-RANGE` | S781 | `readme-end` is `52a6ef1`, `RD=` in `goals.sh` is `readme-start..readme-end`, and both goals pass |
| `G-QC-ISSUES` | D84 (after the push, after the user's confirmation) | Both Issues are `CLOSED`; the comments contain the diagnostic codes, the SHA of a commit in the range and the output lines after the fix; the comment on #2 contains `bounds/issue2` |
| `G-QC-AUDIT` | The record of the P4-2 audit | 1 or more |

```bash
# G-QC-DECISIONS
grep -c '^### D[0-9]*\. .*\[decided\]' $PLAN_QC       # → 19
grep -c '^### D[0-9]*\. .*\[open\]' $PLAN_QC           # → 0
grep -c '^- \*\*Decision:' $PLAN_QC                         # → 19

# G-QC-COVERAGE (empty output and 0)
diff <(sed -n '/^## 4\./,/^## 5\./p' $PLAN_QC | grep -oE '\*\*S[0-9]+\*\*' | tr -d '*' | sort -u) \
     <(sed -n '/^### 5\.3/,/^## 6\./p' $PLAN_QC | grep -oE '^\| S[0-9]+' | tr -d '| ' | sort -u)
sed -n '/^## 8\./,/^## 9\./p' $PLAN_QC | grep '^- ' | grep -vc 'Reason a command cannot decide:'

# G-QC-PLAN-NAMES
scripts/check-plan-tests.sh $PLAN_QC | tail -n 1                  # → plan-tests: <n> names, 0 missing

# G-QC-PLAN-TESTS
scripts/check-plan-tests.sh --mutations $PLAN_QC | tail -n 1      # → mutations: <m> rows, <m> recorded

# G-QC-GOLDEN-ARGS
scripts/golden-entries.sh tests/golden/args | wc -l               # → 22
scripts/run-golden.sh $OZ tests/golden/args | tail -n 1           # → golden: 22 passed, 0 failed, 0 timed out

# G-QC-GOLDEN-BOUNDS
scripts/golden-entries.sh tests/golden/bounds | wc -l             # → 55
scripts/run-golden.sh $OZ tests/golden/bounds | tail -n 1         # → golden: 55 passed, 0 failed, 0 timed out

# G-QC-GOLDEN-CHANGED (expectations corrected (not added) in the range that §9 does not name)
git rev-parse -q --verify refs/tags/qc-start >/dev/null && echo ok           # → ok
git diff --name-only --diff-filter=M qc-start HEAD -- 'tests/golden/**/*.out' 'renderers/test/**' 'samples/*/expect.yaml' \
  | while IFS= read -r f; do grep -qF -- "- Golden with a corrected expectation: \`$f\`" $PLAN_QC || echo "$f"; done | wc -l   # → 0

# G-QC-PRELUDE
grep -cE '^action (soak|fry|simmer|boil|parboil|chill|rest)\(.*\btime: Time in 0 s<\.\.' stdlib/prelude.ozen   # → 7
grep -cE '^action cut\(.*\bsize: Length in 0 mm<\.\. = 3 mm\)' stdlib/prelude.ozen                              # → 1
grep -cE 'Temperature\??[[:space:]]+in\b' stdlib/prelude.ozen                                                  # → 0
grep -cE '^  servings: Count\? in 1\.\.$' stdlib/prelude.ozen                                                   # → 1

# G-QC-DIAG
grep -cx 'OZNI_DIAG(TYP021, TYP, nonPositiveAmount, ERROR)' core/src/diag_codes.def                  # → 1
[ "$(grep -n 'TYP021' core/src/diag_codes.def | cut -d: -f1)" -gt "$(grep -n 'SCH011' core/src/diag_codes.def | cut -d: -f1)" ] && echo after   # → after
grep -cxF 'TYP:nonPositiveAmount' core/tests/diag-names.txt                                          # → 1
grep -E '^diag-non-positive-amount = ' phrases/en.ftl | grep -F '{ $arg1 }' | grep -cF '{ $arg2 }'   # → 1
grep -E '^diag-non-positive-amount = ' phrases/ja.ftl | grep -F '{ $arg1 }' | grep -cF '{ $arg2 }'   # → 1
grep -cxF 'diag-non-positive-amount = The amount { $arg2 } of { $arg1 } must be more than zero' phrases/en.ftl   # → 1 (the wording from D75)
grep -cxF 'diag-non-positive-amount = { $arg1 }の量 { $arg2 } は 0 より大きくしてください' phrases/ja.ftl          # → 1 (the wording from D75)
grep -cE "^  nonPositiveAmount: \['name'\]," renderers/lib/names.mjs                                  # → 1
grep -cP '\tTYP:nonPositiveAmount\terror\t' core/tests/severity-examples.txt                         # → 1

# G-QC-SCHEMA
grep -c '"minExclusive"' schemas/result.schema.json    # → the same count as grep -c '"min": {' schemas/result.schema.json
mkdir -p build/results/qc-types && rm -f build/results/qc-types/*.json
$OZ check --format json --emit types tests/golden/bounds/types-json-exclusive.ozen > build/results/qc-types/exclusive.result.json
python3 -B tools/jsoncheck/validate.py schemas build/results/qc-types | tail -n 1   # → json: 1 files, 0 invalid

# G-QC-SYNTAX
grep -cE '^type_range[[:space:]]*:=' core/grammar.ebnf                                          # → 1
grep -cx 'OZNI_RULE(type_range)' core/src/rules.def                                              # → 1
grep -cxF '"<.."' core/fuzz/ozen.dict                                                            # → 1
sed -n '/^### 4\.2/,/^### 4\.3/p' docs/concept.md | grep -cF '0 s<..'                           # → 1 or more
sed -n '/^### 4\.2/,/^### 4\.3/p' docs/concept.md | grep -cF 'a<..b'                            # → 1 or more
sed -n '/^### 4\.2/,/^### 4\.3/p' docs/concept.md | grep -cF '上端は含む'                       # → 1 or more
sed -n '/^### 4\.2/,/^### 4\.3/p' docs/concept.md | grep -F '範囲外は必ずエラー' | grep -cF '動作の引数'   # → 1 or more
grep -cE '^  servings: +Count\? in 1\.\.' docs/concept.md                                        # → 1 or more
sed -n '/^### 6\.1/,/^## 7\./p' docs/concept.md | grep -cF '0 以下の量'                          # → 1 or more

# G-QC-SCOPE (built the same way as goal_G_RD_SCOPE of the README plan; patterns are matched with a bash case)
git rev-parse -q --verify refs/tags/qc-start >/dev/null && echo ok           # → ok
# SHAs in the "Other work's commit in range: <SHA>" lines of §9 that are not in the range → 0
# Files touched by the commits in the range, minus the excluded commits, that match none of the following → 0
#   core/src/*.c core/src/*.h core/src/*.def core/grammar.ebnf core/fuzz/ozen.dict core/tests/diag-names.txt
#   core/tests/severity-examples.txt stdlib/prelude.ozen phrases/en.ftl phrases/ja.ftl renderers/lib/names.mjs
#   renderers/test/* schemas/result.schema.json tests/golden/* docs/concept.md scripts/goals.sh CLAUDE.md
#   scripts/red-first-exceptions.txt scripts/test-names-exceptions.txt plans/ozen-quantity-checks.md
#   plans/ozen-syntax-refresh.md plans/ozen-readme-compact.md

# G-QC-VERSION
cat VERSION                                            # → 0.1.0 (D83)

# G-QC-CLAUDEMD
grep -c 'ozen-quantity-checks.md.*G-QC-.*qc-start' CLAUDE.md                 # → 1 or more

# G-QC-RF-COUNT
sed -n '/^goal_G_RF_GOLDEN_COUNT()/,/^}/p' scripts/goals.sh | grep -c 'readme-start'   # → 1 or more
scripts/goals.sh G-RF-GOLDEN-COUNT                                                     # → pass

# G-QC-RD-RANGE
git rev-parse --short=7 readme-end                                           # → 52a6ef1
grep -cx 'RD=readme-start..readme-end' scripts/goals.sh                       # → 1
scripts/goals.sh G-RD-SCOPE G-RD-ADD-ONLY                                     # → both pass

# G-QC-ISSUES (after the push)
gh issue view 1 --repo atsuki-seo/Ozen --json state -q .state                 # → CLOSED
gh issue view 2 --repo atsuki-seo/Ozen --json state -q .state                 # → CLOSED
gh issue view 1 --repo atsuki-seo/Ozen --json comments -q '.comments[].body' | grep -c 'TYP001'   # → 1 or more
gh issue view 2 --repo atsuki-seo/Ozen --json comments -q '.comments[].body' | grep -c 'TYP021'   # → 1 or more
gh issue view 2 --repo atsuki-seo/Ozen --json comments -q '.comments[].body' | grep -c 'TYP012'   # → 1 or more
gh issue view 1 --repo atsuki-seo/Ozen --json comments -q '.comments[].body' | grep -cF 'case1.ozen:5:20-5:24'   # → 1 or more
gh issue view 2 --repo atsuki-seo/Ozen --json comments -q '.comments[].body' | grep -cF 'case2.ozen:3:12-3:19'   # → 1 or more
gh issue view 2 --repo atsuki-seo/Ozen --json comments -q '.comments[].body' | grep -cF 'bounds/issue2'          # → 1 or more
# Among the 7- to 40-digit hex strings in each Issue's comments, those that point to a commit in the range qc-start..HEAD → 1 or more

# G-QC-AUDIT (P4-2)
grep -cE '^- Coverage audit: [0-9]+ findings, [0-9]+ taken' $PLAN_QC        # → 1 or more
```

`G-QC-SCOPE` allows this file, in case this file is committed after the starting-point tag. `plans/ozen-syntax-refresh.md` and `plans/ozen-readme-compact.md` are allowed only for the one line each from D80 and D87. The exact counts in `G-QC-GOLDEN-ARGS` and `G-QC-GOLDEN-BOUNDS` count only this plan's folders (`args/`, `bounds/`), so they do not fail when a later plan adds goldens to another folder. `G-QC-DIAG` checks that the line comes after `SCH011`, not that it is the last line, so it does not fail when a later plan adds diagnostics.

### 5.2 Tests

There are 77 new goldens (22 in `tests/golden/args/` and 55 in `tests/golden/bounds/`) and one renderer test (`renderers/test/settings-servings-min.test.mjs`). The number of goldens grows by 77 from N0 at the starting point (§9; 464 on 2026-11-02). To the 72 goldens at the time the plan was written (21 and 51), the P4-2 audit added 5 (`args/recipe-default-once`, `bounds/exclusive-range-lower`, `bounds/servings-flag-invalid-amounts`, `bounds/servings-make-invalid`, `bounds/servings-make-own-range`).

- The expectations (`.out`) are written by hand from the spec in §4 and from the diagnostic format of existing goldens (`TYP012` in `kitchen/range-open-ended.out`, and the argument shape of `TYP001` in `kitchen/unitless-capacity.out`). Do not copy the output of the implementation (`ozen`). Count the positions (line and column) from the source.
- The only exception is `bounds/expression-unchanged` (S715): its `debug ast` expectation is made from the output of the starting-point build (the `ozen` of `qc-start`). This test checks that parsing does not change, so the implementation before the change is the oracle. Say so in §9.
- `settings-servings-min.test.mjs` (S774) calls `settingsSpec` with the expectation of the golden `api/types-json` as input (with the `min` `"1"` of `Options.servings` fixed by hand), and checks that the `min` of the servings input is 1.
- Commits that add expectations come before the implementation, and we check that the expectations fail with the build at that commit (red; `G-REPO-RED-FIRST`). Control goldens that check that no diagnostic appears (such as `args/ok-units`), and goldens that already pass with the implementation of an earlier phase or the current implementation (such as `bounds/dimension-before-range` and `bounds/servings-own-range`), pass at the time they are added. Following the precedent of the syntax plan (`ingredient-dimension-ok`), put them in a separate commit from the red goldens, and write that commit and the reason (which mutation check makes it fail) in `scripts/red-first-exceptions.txt`.
- When the expectation of an existing golden changes, correct it only if the reason for the change can be explained by this plan's spec (the list of diagnostics grew by one, the type text of `Options.servings` changed, the type text of arguments of `std` actions changed, range quantities are now evaluated, and so on), and write one line per golden in §9 in the form `- Golden with a corrected expectation: \`<path>\` — <理由>` (S751). Commits that correct expectations are kept apart from commits that change the implementation (`G-RF-SEPARATION`, S782). Do not correct changes that cannot be explained; investigate them.
- Do not loosen a failing golden; investigate the cause. When the expectation and the implementation disagree, compare them with §4 to decide which one is wrong, and if §4 is wrong, report it to the user.

| Test | What it checks | Main task |
| --- | --- | --- |
| `cgolden/args/*` (22 cases) | Each S item of §4.1 to §4.3. Names are in §5.3 | P1-1, P4-2 |
| `cgolden/bounds/*` (55 cases) | Each S item of §4.4 to §4.8. Names are in §5.3 | P2-1, P3-1, P4-2 |
| `rtest/settings-servings-min` | S774 | P3-1 |

What machines cover: whether diagnostics appear, their count, code, position and arguments, and the exit code (goldens); grammar rules, the dictionary and words in the documents (grep); the JSON of the type list and the schema (the validation tool); the parts of the settings screen (renderer test); the declarations in the standard library (grep); the scope of changed files; and the state and content of the Issue comments (`gh`). Whether the diagnostic wording and the explanation get through to readers, and how the replies to the Issues are worded, go to §8.

### 5.3 Coverage table

Of the aspects, "Time and order" and "Persistence" do not apply, because this is a change to compile-time checks (the schedule's time calculation does not change). For "Platform path", the core is a single implementation in C and the wiring for Linux, WebAssembly, iOS and Android does not change, so the prerequisite goals `G-PLAT-LINUX` and `G-PLAT-WASM` (which also run the goldens on wasm) are enough, and no rows are added. "Real path" is covered by the goldens that go through the `ozen check` command (all of them), and by the goldens for pipes, named arguments, macros, composite actions, `make`, menu headers, `-s` and `--options`.

| S item | Aspect | Test or goal | How to break it | Main task |
| --- | --- | --- | --- | --- |
| S695 | Normal | `cgolden/args/volume-for-time` | Remove the call to the check in `call_action` | P1-2 |
| S695 | Normal | `cgolden/args/mass-for-length` | Narrow the check so it only works on `Time` declarations | P1-2 |
| S696 | Boundary | `cgolden/args/unitless-time` | Skip dimensionless quantities before the check | P1-2 |
| S697 | No-effect condition | `cgolden/args/count-param` | Remove the step in the check that turns `pcs` into a dimensionless quantity | P1-2 |
| S698 | Real path | `cgolden/args/named-arg` | Limit the check to arguments passed by position | P1-2 |
| S699 | Real path | `cgolden/args/pipe-arg` | In `pipe_into`, attach the left side as the last argument (`3 ml` lands on `x`, and the position and arguments of the diagnostic change) | P1-2 |
| S700 | Normal | `cgolden/args/variable-arg` | Put the position of the diagnostic on the whole call | P1-2 |
| S701 | Real path | `cgolden/args/macro-body` | Make the trail of the check's diagnostic empty (the trail for the position in the body disappears; the reason for this replacement is in §9) | P1-2 |
| S758 | Real path | `cgolden/args/macro-splice` | Set the position of the check's diagnostic to the argument node itself (the `$t` in the macro body), without going through `arg_site` | P1-2 |
| S702 | Real path | `cgolden/args/composite` | Skip the outer check for composite actions | P1-2 |
| S759 | Side effects | `cgolden/args/composite` | Also report the check's diagnostic for calls inside a composite action (there are 2) | P1-2 |
| S703 | Normal | `cgolden/args/user-dimension` | Limit the check to the names of the 6 standard dimensions | P1-2 |
| S704 | No-effect condition | `cgolden/args/unknown-value` | Also check unknown quantities, treating them as dimensionless quantities | P1-2 |
| S705 | No-effect condition | `cgolden/args/default-unchecked` | Also call the check after evaluating default values (fails on `time: Time = 3 ml`) | P1-2 |
| S706 | Side effects | `cgolden/args/continues-after` | Return an unknown value from `call_action` on a dimension error | P1-2 |
| S707 | No-effect condition | `cgolden/args/ok-units` | Make interval values (`3..5 min`) an error | P1-2 |
| S707 | No-effect condition | `cgolden/args/ok-units` | Also check enum arguments (`heat: high`), treating values that are not quantities as dimensionless quantities | P1-2 |
| S708 | Real path | `cgolden/args/issue1` | Make the severity of the check's diagnostic a warning (the exit code becomes 0) | P1-2 |
| S788 | No-effect condition | `cgolden/args/default-unchecked` | In `qty_site`, do not skip the frame of a composite action's default values (the inner `chill` checks the default `3 ml`) | P4-2 |
| S789 | Side effects | `cgolden/args/recipe-default-once` | Also bind recipe arguments that failed their type (`TYP001` also appears at the `boil` position) | P4-2 |
| S760 | Normal | `cgolden/args/range-units-steps` | Remove the evaluation of ranges in `eval_inner` (goes back to `?`) | P1-2 |
| S761 | Real path | `cgolden/args/range-units-dimension` | In the range evaluation, do not record the dimension of the result (make it a dimensionless interval; it becomes `["Dimensionless", "Time"]`) | P1-2 |
| S762 | Boundary | `cgolden/args/range-units-mixed` | Do not compare the dimensions of the two ends; use the dimension of the left one | P1-2 |
| S763 | Data completeness | `cgolden/args/range-units-ingredient` | Make range nodes unknown when evaluating ingredient amounts | P1-2 |
| S786 | Boundary | `cgolden/args/range-units-steps` | Make a range whose lower end is larger than its upper end an unknown value | P4-2 |
| S764 | Real path | `cgolden/args/make-arg-dimension` | Remove the check of quantity arguments of `make` | P1-2 |
| S765 | Real path | `cgolden/bounds/make-arg-range` | Skip the range check in the check of `make` | P2-2 |
| S709 | Boundary | `cgolden/bounds/exclusive-from` | Change the exclusive lower-end check from `<` to `<=` (exactly the lower end passes) | P2-2 |
| S710 | Boundary | `cgolden/bounds/exclusive-upper-inclusive` | Make the upper-end check `>=` (exactly the upper end fails) | P2-2 |
| S710 | Boundary | `cgolden/bounds/exclusive-upper-over` | Drop the upper end of an `a<..b` that has one | P2-2 |
| S711 | No-effect condition | `cgolden/bounds/exclusive-space` | Allow spaces between `<` and `..` | P2-2 |
| S712 | Normal | `cgolden/bounds/exclusive-from` | Make the third argument of the diagnostic only the text of the lower end | P2-2 |
| S713 | Boundary | `cgolden/bounds/exclusive-interval` | Use the upper end (`hi`) when checking interval values | P2-2 |
| S714 | No-effect condition | `cgolden/bounds/closed-zero-ok` | Use the exclusive lower-end check for closed ranges too | P2-2 |
| S715 | No-effect condition | `cgolden/bounds/expression-unchanged` | Read `<..` in `range()` (shared with expressions) (the `debug ast` tree changes) | P2-2 |
| S766 | Real path | `cgolden/bounds/exclusive-kitchen` | Ignore the exclusive lower-end mark in kitchen settings fields | P2-2 |
| S767 | Boundary | `cgolden/bounds/exclusive-count` | Drop the mark on a number-only lower end (`0<..`) | P2-2 |
| S768 | Boundary | `cgolden/bounds/exclusive-empty` | For a range whose lower and upper ends are equal, make the lower-end check closed | P2-2 |
| S790 | No-effect condition | `cgolden/bounds/exclusive-range-lower` | In `type_range()`, accept a range of numbers with units as the lower end | P4-2 |
| S716 | Data completeness | `cgolden/bounds/types-json-exclusive` | Do not output `minExclusive` | P2-2 |
| S716 | No-effect condition | `cgolden/api/types-json` | Also output `"minExclusive":false` for ranges that include the lower end | P2-2 |
| S717 | Data completeness | `G-QC-SCHEMA` | Remove `minExclusive` from the type fields of the schema (validation fails because of `additionalProperties: false`) | P2-2 |
| S769 | Data completeness | `cgolden/api/signatures-json` | Make the type text of action arguments only the type name, without the range | P2-3 |
| S718 | Data completeness | `G-CORE-GRAMMAR` | Remove `mark(p, OZNI_R_type_range)` from `type_range()` (the rule is not covered by the goldens) | P2-2 |
| S718 | Docs match | `G-QC-SYNTAX` | Remove the `type_range` rule from `core/grammar.ebnf` | P2-2 |
| S719 | Data completeness | `G-QC-SYNTAX` | Remove `"<.."` from `core/fuzz/ozen.dict` | P2-2 |
| S794 | Data completeness | `cgolden/bounds/types-json-exclusive` | In `put_field`, do not output the upper end of `RangeAbove` as `max` | P4-2 |
| S720 | Data completeness | `G-QC-PRELUDE` | Remove the range of `time` in `chill` | P2-3 |
| S721 | Data completeness | `G-QC-PRELUDE` | Make the range of `size` in `cut` `0 mm..` | P2-3 |
| S722 | Boundary | `cgolden/bounds/time-zero` | Make the range of `simmer` `0 s..` | P2-3 |
| S723 | Normal | `cgolden/bounds/time-negative` | Remove the range of `boil` | P2-3 |
| S770 | Real path | `cgolden/bounds/range-units-negative-time` | Remove the range of `simmer` | P2-3 |
| S724 | Boundary | `cgolden/bounds/size-zero` | Make the range of `cut` `0 mm..` | P2-3 |
| S724 | Normal | `cgolden/bounds/size-negative` | Remove the range of `cut` | P2-3 |
| S725 | No-effect condition | `cgolden/bounds/small-positive` | Make the range `1 min<..` | P2-3 |
| S725 | No-effect condition | `cgolden/bounds/soak-default` | Make calls that use the default value checked too, then make the range of `soak` `30 min<..` (the default 30 min is out of range) | P2-3 |
| S726 | No-effect condition | `cgolden/bounds/temperature-negative` | Apply the `0<..` check to all arguments of dimension types | P2-3 |
| S726 | Data completeness | `G-QC-PRELUDE` | Make `OvenSpec.maxTemp` `Temperature? in 0 degC..` | P2-3 |
| S727 | Side effects | `cgolden/bounds/dimension-before-range` | Continue the range check even when the dimension differs | P2-3 |
| S771 | Side effects | `cgolden/bounds/composite-range` | Report the position of a check inside a composite action at the inner call as it is (the position falls inside the definition) | P2-3 |
| S787 | Side effects | `cgolden/bounds/composite-range` | Do not record in the composite action's frame that the inner check failed (the diagnostic is also reported for the second use) | P4-2 |
| S728 | Normal | `cgolden/bounds/amount-negative` | Remove the code that reports `TYP021` | P3-2 |
| S729 | Boundary | `cgolden/bounds/amount-zero` | Make the check `lo < 0` (0 passes) | P3-2 |
| S730 | Normal | `cgolden/bounds/amount-negative` | Make the second argument of the diagnostic the type name of the ingredient | P3-2 |
| S731 | Boundary | `cgolden/bounds/amount-range-zero` | Use the upper end of the interval in the check | P3-2 |
| S731 | Boundary | `cgolden/bounds/amount-range-negative` | Make the check `lo < -1` (the lower end -0.8 of `-0.5..0.8 g` passes, and there is one fewer `TYP021`) | P3-2 |
| S731 | No-effect condition | `cgolden/bounds/amount-positive` | Make the check `lo <= 1` | P3-2 |
| S772 | Real path | `cgolden/bounds/amount-range-units` | Apply the `TYP021` check only to quantity literals and numeric range nodes | P3-2 |
| S732 | Normal | `cgolden/bounds/amount-per-serving-zero` | Skip the check for `per serving` amounts | P3-2 |
| S733 | Side effects | `cgolden/bounds/amount-per-serving-unknown` | Skip the check when there is a `TYP020` | P3-2 |
| S773 | Side effects | `cgolden/bounds/servings-invalid-amounts` | Use out-of-range servings as they are instead of making them unknown (`150 ml * servings` becomes 0 ml and `TYP021` appears; the reason for this replacement is in §9) | P3-3 |
| S785 | No-effect condition | `cgolden/bounds/servings-invalid-amounts` | When the servings are out of range, also skip the check of `per serving` amounts (the `TYP021` for `0 g per serving` disappears) | P3-3 |
| S734 | No-effect condition | `cgolden/bounds/amount-any` | Treat `[any amount]` as 0 in the check | P3-2 |
| S735 | No-effect condition | `cgolden/bounds/amount-none` | Treat ingredients without an amount as 0 in the check | P3-2 |
| S736 | Side effects | `cgolden/bounds/amount-wrong-dim` | Continue the check after a dimension error | P3-2 |
| S737 | Normal | `cgolden/bounds/amount-expression` | Apply the check only to quantity literal nodes | P3-2 |
| S738 | No-effect condition | `cgolden/bounds/amount-positive` | Make the check the same as `lo <= 0.5 g` (`0.5 g` fails) | P3-2 |
| S739 | Data completeness | `G-QC-DIAG` | Remove `{ $arg2 }` from `diag-non-positive-amount` in `phrases/ja.ftl` | P3-2 |
| S739 | Data completeness | `G-CORE-DIAG-REGISTRY` | Remove `TYP:nonPositiveAmount` from `core/tests/diag-names.txt` | P3-2 |
| S739 | Fixed premise | `G-CORE-DIAG-APPEND` | In a work commit in a temporary worktree, move `TYP021` in between existing lines (a deleted line shows up; remove it with `git reset --hard` and do not push) | P3-2 |
| S740 | Docs match | `G-TYPE-SEVERITY-TABLE` | Make the severity in `severity-examples.txt` `warning` | P4-1 |
| S740 | Docs match | `G-QC-SYNTAX` | Remove "amounts of 0 or less" from concept §6.1 | P4-1 |
| S741 | Data completeness | `cgolden/api/types-json` | Remove the range of `Options.servings` | P3-3 |
| S774 | Real path | `rtest/settings-servings-min` | Make `settingsSpec` drop the `min` of optional types (`Count?`) (the P4-2 audit replaced the original method, which rewrote the test input) | P3-3 |
| S742 | Boundary | `cgolden/bounds/optional-ranged` | Revert how `base_type` unwraps nesting | P3-3 |
| S743 | Normal | `cgolden/bounds/servings-header-zero` | Remove the check of untyped `servings` | P3-3 |
| S744 | Normal | `cgolden/bounds/servings-typed-zero` | Do not check `servings` that has only a type | P3-3 |
| S745 | Side effects | `cgolden/bounds/servings-own-range` | Add `1..` on top even when the declaration has a range (`0` gets 2) | P3-3 |
| S746 | Real path | `cgolden/bounds/servings-make-zero` | Do not check the servings in `make` arguments | P3-3 |
| S775 | Real path | `cgolden/bounds/servings-make-positional` | Limit the check of `make` to arguments passed by name | P3-3 |
| S776 | Real path | `cgolden/bounds/servings-menu-header` | Check servings only in recipe headers | P3-3 |
| S747 | Real path | `cgolden/bounds/servings-flag-zero` | Skip `check_value` for option fields (the `sema.c:3163` path) | P3-3 |
| S747 | Boundary | `cgolden/bounds/servings-flag-negative` | Make the check `== 0` | P3-3 |
| S777 | Side effects | `cgolden/bounds/servings-flag-header-range` | Check on the header side even when there is an error on the options side (there are 2) | P3-3 |
| S778 | Real path | `cgolden/bounds/servings-flag-over-header` | Ignore the range declared in the header for servings that come from options | P3-3 |
| S779 | Side effects | `cgolden/bounds/servings-flag-dimension` | Remove the deduplication (goes back to the same 2 lines as now) | P3-3 |
| S748 | Real path | `cgolden/bounds/servings-options-zero` | Skip `check_value` for option fields (the `sema.c:3163` path) | P3-3 |
| S749 | Boundary | `cgolden/bounds/servings-one-ok` | Make the range `1<..` | P3-3 |
| S780 | Boundary | `cgolden/bounds/servings-flag-one-ok` | Make the range of `Options.servings` `1<..` | P3-3 |
| S791 | Side effects | `cgolden/bounds/servings-flag-invalid-amounts` | Bind the servings on the header side even when there is an error on the options side (`150 ml * servings` becomes 0 ml and `TYP021` appears) | P4-2 |
| S792 | Data completeness | `cgolden/bounds/servings-make-invalid` | Pass an argument that failed the `make` check to the dish as it is (the amount becomes `0 ml`) | P4-2 |
| S793 | Real path | `cgolden/bounds/servings-make-own-range` | Check the servings of `make` with the type of `Options.servings`, ignoring the header's range | P4-2 |
| S750 | Real path | `cgolden/bounds/issue2` | Remove the range of `boil` | P3-3 |
| S751 | Side effects | `G-QC-GOLDEN-CHANGED` | Remove one "Golden with a corrected expectation" line from §9 | P4-3 |
| S752 | Side effects | `G-SAMPLE-CHECK` | Temporarily make the range of `rest` in `stdlib/prelude.ozen` `1 h<..` (the sample's `rest(10 min)` fails) | P4-3 |
| S753 | Side effects | `G-QC-SCOPE` | In a work commit in a temporary worktree, change one file under `samples/` (remove it with `git reset --hard` and do not push) | P4-3 |
| S753 | Fixed premise | `G-QC-SCOPE` | Temporarily move the tag `qc-start` to another name locally (put it back after checking) | P4-3 |
| S754 | Side effects | `G-QC-VERSION` | Make `VERSION` `0.1.1` (put it back after checking) | P4-3 |
| S755 | Fixed premise | `G-QC-RF-COUNT` | Revert `goal_G_RF_GOLDEN_COUNT` to its current form (comparing with the current count) (the `readme-start` line disappears) | P0-3 |
| S755 | Side effects | `G-RF-GOLDEN-COUNT` | In a temporary worktree, remove one golden that exists at `readme-start` (the missing goldens become 1) | P0-3 |
| S781 | Fixed premise | `G-QC-RD-RANGE` | Revert `RD=` in `goals.sh` to `readme-start..HEAD` (`G-RD-SCOPE` fails with `got '2'`) | P0-3 |
| S782 | Side effects | `G-RF-SEPARATION` | In a work commit in a temporary worktree, put an expectation `.out` and `core/src` in the same commit (remove it with `git reset --hard` and do not push) | P4-3 |
| S756 | Docs match | `G-QC-SYNTAX` | Remove the `0 s<..` example from concept §4.2 | P4-1 |
| S783 | Docs match | `G-QC-SYNTAX` | Remove action arguments from the sentence "out of range is always an error" in concept §4.2 | P4-1 |
| S784 | Docs match | `G-QC-SYNTAX` | Revert `servings` in the `type Options` example of concept to `Count` | P4-1 |
| S795 | Docs match | `G-QC-SYNTAX` | Remove "the upper end is included" from concept §4.2 | P4-2 |
| S757 | Normal | `G-QC-CLAUDEMD` | Remove `qc-start` from this plan's line in `CLAUDE.md` | P0-3 |

The golden for S704 (`args/unknown-value`) needs a way to pass as an argument a quantity that is not known at evaluation time. The plan is to use the style of the existing golden `eval/runtime-value-in-if` (a condition not known until run time), and to confirm in P1-1 that it can be written. If it cannot, write the reason in §9 and report it to the user (do not move the test to §8).

The coverage check (run after P0-3 and in P4-2; `G-QC-COVERAGE`).

Findings of the independent audit (subagent, 2026-11-02, after the plan was written) that were not taken, or only partly taken, with reasons:

- `G-QC-PLAN-TESTS` does not check the match between records and each S item (it only compares the total number of record lines) — this is the same check as in the existing plans (the syntax plan and the README plan), and a person can trace them because each record line names the S item and the goal (§9). A check that matches each row of the coverage table would be separate work that changes how `check-plan-tests.sh` is built
- Make mixing up non-quantity values and quantities (such as `cut(slice, dice)`) an error — this is a different check from the "units" in the Issue, so it is out of scope (§1), and will be proposed as separate work after the final gate
- The same diagnostic appears once per repetition inside `repeat` — this follows the behavior of existing diagnostics and was put out of scope (§1)
- `make gohan(servings: 0)` for a dish without `servings`, headers that write the servings as an expression, ignoring `Time in 0 s`, and number-only ranges not appearing in the JSON — all are separate existing behaviors and were put out of scope (§1). The first one will be proposed as separate work
- The idea of checking with grep whether the wording in §8 gets through — the use of the arguments was added to `G-QC-DIAG`. Whether the wording gets through stays in §8

Of the 23 findings of the coverage audit (P4-2, 2026-11-02, independent subagent), the 8 that were not taken and the reasons:

- 1-1 S763 is not checked in the rendered text — the text is made from the ingredient amount (the value shown by `debug steps`) through the existing rendering path, and existing renderer tests check the text for ingredients with amounts. What this plan changed is whether the amount is attached, and `args/range-units-ingredient` checks that
- 1-3 The trail of a quantity spliced in by a nested macro — the position and the trail are decided by the existing `arg_site`, which is shared with the state check. Existing goldens (such as `macro/trail-nested`) check nested trails
- 1-5 There is no test for a temperature dimension mismatch — the dimension check does not depend on the dimension's name (it compares by the value of `dim_index`), so checking representatives of the 6 standard dimensions and user dimensions (`args/user-dimension`) is enough
- 2-4 An error in a composite action's definition (passing the outer `t: Time` to the `Length` of the inner `cut`) appears at the caller's position — where and how many times to report errors in a definition needs the same kind of design decision as D78 (default values). It is outside the Issue's scope (errors in calls), so it will be proposed as separate work after the final gate
- 2-5 The position and trail when an argument is passed as an expression inside a composite action — diagnostics inside composite actions (such as the state check) already appear at the position inside the definition with no trail, which is existing behavior, and the quantity check follows it. To be proposed together with 2-4
- 2-11 Allow a space before `<` (`0 s <..`) — D73 decided only about the space between `<` and `..`; before `<`, allowing a space is natural, as with comparisons in expressions (`3 min < 5 min`). §4 is not changed
- 2-13 The argument shape of `TYP001` for a range with a unit on only one side (`1 ml..2`) — it has the same `[左の次元, 右の次元]` shape (left dimension, right dimension) as a dimension mismatch in a binary operation (`$amount + 1 min`), and `args/range-units-mixed` checks the same shape
- 2-14 Rebinding an argument's name with `let` inside a composite action — rebinding is already a `NAM003` error, and we do not decide the shape of secondary diagnostics for wrong programs

---

## 6. Phases and tasks

Notation:

- A **completion condition** is a goal ID from §5, or a command with a clear pass or fail. The "run locally" prerequisite goals of §5 are implicitly part of every task. Prerequisite goals for after a push are only part of the completion condition of the gate that pushes.
- Status report format: "core: `G-QC-*` and the prerequisite goals pass (4 configurations). wasm: `G-PLAT-WASM` passes. GitHub: check `G-PLAT-CI` after the push".
- Commits are logical units (adding goldens, one implementation change, correcting expectations, adding phrases, adding goals). A commit that adds goldens comes before the implementation commit that makes them pass (`G-REPO-RED-FIRST`). Goldens that pass from the start go in a separate commit, with the reason in `scripts/red-first-exceptions.txt` (§5.2). Commits that correct existing expectations are kept apart from implementation commits (`G-RF-SEPARATION`). Pushes happen without asking (D81), but every time before a push, run `G-REPO-NOREPLY`, and report the pushed range.
- The "mutation check" of each task confirms that breaking things the way the coverage table says makes that test or goal fail, and that restoring makes it pass; record it in §9 with a `- Mutation check: ` line. Ways of breaking that need a work commit are done in a temporary worktree, removed with `git reset --hard`, and not pushed (D81).
- Do not include uncommitted changes from other work in commits. Name the files in `git add`.

### Phase 0 — Decisions and preparation (no change to feature code)

| ID | Content | Completion condition |
| --- | --- | --- |
| P0-1 | Settle D73 to D87 in §2 with the user and write them into this file | `grep -c '^### D[0-9]*\. .*\[decided\]' plans/ozen-quantity-checks.md` is 19, `grep -c '^### D[0-9]*\. .*\[open\]' plans/ozen-quantity-checks.md` is 0, `grep -c '^- \*\*Decision:' plans/ozen-quantity-checks.md` is 19 (after P0-3, `G-QC-DECISIONS`) |
| P0-2 | Check that the working tree has no uncommitted changes from other work (if there are, ask the user). Commit and push this file, and put the tag `qc-start` on `main` (D81). Record the baseline in §9 (pass or fail of the local prerequisite goals and of `G-RD-*` and `G-RF-*`, the golden count N0, and the starting-point build used to make the expectation of `bounds/expression-unchanged`) | `git rev-parse -q --verify refs/tags/qc-start` succeeds, and §9 has the baseline lines |
| P0-3 | Add `PLAN_QC`, `QC` and `goal_G_QC_*` to `scripts/goals.sh`. Add a line for this file to the Plan section of `CLAUDE.md` (D82). Change `goal_G_RF_GOLDEN_COUNT` to the form of D80, and add a line to §9 of the syntax plan. Put the tag `readme-end` on `52a6ef1`, change `RD` to `readme-start..readme-end`, and add a line to §9 of the README plan (D87) | `scripts/goals.sh --list \| grep -c '^G-QC-'` is 18, and `G-QC-CLAUDEMD`, `G-QC-COVERAGE`, `G-QC-RF-COUNT`, `G-QC-RD-RANGE`, `G-RF-GOLDEN-COUNT`, `G-REPO-CLAUDEMD`, `G-REPO-SELFTESTS` pass. Mutation checks for S755, S757, S781 |

At P0-3 the implementation has not changed yet, so `G-QC-GOLDEN-*`, `G-QC-PRELUDE`, `G-QC-DIAG`, `G-QC-SCHEMA` and `G-QC-SYNTAX` fail. Record in §9 the list of goals that failed at P0-3 (this is evidence that the goals check something).

### Phase 1 — Checking quantities (#1, D69, D85, D86)

| ID | Content | Completion condition |
| --- | --- | --- |
| P1-1 | Write the 21 goldens in `tests/golden/args/` by hand from §4.1 to §4.3, and commit them before the implementation (16 from §4.1: `volume-for-time`, `mass-for-length`, `unitless-time`, `count-param`, `named-arg`, `pipe-arg`, `variable-arg`, `macro-body`, `macro-splice`, `composite`, `user-dimension`, `unknown-value`, `default-unchecked`, `continues-after`, `ok-units`, `issue1`. 4 from §4.2: `range-units-steps`, `range-units-dimension`, `range-units-mixed`, `range-units-ingredient`. 1 from §4.3: `make-arg-dimension`). Goldens that pass at that commit (expected: `ok-units`, `default-unchecked`, `unknown-value`) go in a separate commit, with the reason in `scripts/red-first-exceptions.txt` (§5.2) | `scripts/golden-entries.sh tests/golden/args \| wc -l` is 21, and the prerequisite `G-REPO-RED-FIRST`. Record the names of the red and control goldens in §9 |
| P1-2 | Extract the dimension and range checks of `check_value`, and call them from `call_action` (including the deduplication for composite actions) and from `make` arguments. Evaluate ranges with units on both ends in `eval_inner`. If the expectation of an existing golden changes, correct it or investigate it following S751, and put any correction in a separate commit. Check whether arguments inside state types have the same hole, and write it in §9 | `G-QC-GOLDEN-ARGS`, `G-QC-GOLDEN-CHANGED`, and the prerequisites `G-CORE-GOLDEN` (4 configurations), `G-SAMPLE-CHECK`, `G-CORE-FUZZ`, `G-RF-SEPARATION`. Mutation checks for S695 to S708 and S758 to S764 |

### Phase 2 — Ranges that exclude the lower end, and ranges for standard actions (#2, D70, D73, D74, D79)

| ID | Content | Completion condition |
| --- | --- | --- |
| P2-1 | Of `tests/golden/bounds/`, write the 22 goldens for range syntax and for ranges of standard actions, and commit them before the implementation (8 `exclusive-*`: `from`, `upper-inclusive`, `upper-over`, `space`, `interval`, `kitchen`, `count`, `empty`. 14 others: `closed-zero-ok`, `expression-unchanged`, `types-json-exclusive`, `make-arg-range`, `time-zero`, `time-negative`, `range-units-negative-time`, `size-zero`, `size-negative`, `small-positive`, `soak-default`, `temperature-negative`, `dimension-before-range`, `composite-range`). Make the expectation of `expression-unchanged` from the `debug ast` output of the starting-point build (§5.2). Goldens that pass at that commit (expected: `closed-zero-ok`, `expression-unchanged`, `small-positive`, `soak-default`, `temperature-negative`, `dimension-before-range`; `exclusive-space` is already `SYN003` today, so it passes if the position is the same) go in a separate commit, with the reason in the exceptions. Check whether user settings types appear in `--emit types` | `scripts/golden-entries.sh tests/golden/bounds \| wc -l` is 22, and the prerequisite `G-REPO-RED-FIRST`. Record the names of the red and control goldens in §9 |
| P2-2 | Add ranges that exclude the lower end (rule `type_range`) to the parser, the AST, `grammar.ebnf`, `rules.def` and `ozen.dict`. The range check, `base_type`, `minExclusive` in `defs.c`, and the schema. Put any expectation corrections in a separate commit | `exclusive-*`, `closed-zero-ok`, `expression-unchanged`, `types-json-exclusive`, `make-arg-range` and `api/types-json` pass, lines 1 to 3 of `G-QC-SCHEMA` and `G-QC-SYNTAX`, and the prerequisites `G-CORE-GRAMMAR`, `G-RF-FUZZ-DICT`, `G-API-SCHEMA`, `G-CORE-GOLDEN`, `G-RF-SEPARATION`. Mutation checks for S709 to S719 and S765 to S768 |
| P2-3 | Add ranges to 8 arguments in `stdlib/prelude.ozen`. Point the position of range diagnostics inside composite actions to the caller. Handle changes in the expectations of existing goldens (such as `api/signatures-json`) following S751 | `time-*`, `range-units-negative-time`, `size-*`, `small-positive`, `soak-default`, `temperature-negative`, `dimension-before-range`, `composite-range`, `api/signatures-json` pass, lines 1 to 3 of `G-QC-PRELUDE`, and the prerequisites `G-CORE-GOLDEN`, `G-SAMPLE-CHECK`, `G-STD-*`, `G-RF-SEPARATION`. Mutation checks for S720 to S727 and S769 to S771 |

### Phase 3 — Ingredient amounts and servings (#2, D71, D72, D75 to D77)

| ID | Content | Completion condition |
| --- | --- | --- |
| P3-1 | Write the remaining 29 goldens of `tests/golden/bounds/` and `renderers/test/settings-servings-min.test.mjs`, and commit them before the implementation (12 `amount-*`: `zero`, `negative`, `range-zero`, `range-negative`, `range-units`, `per-serving-zero`, `per-serving-unknown`, `any`, `none`, `wrong-dim`, `expression`, `positive`. 15 `servings-*`: `header-zero`, `typed-zero`, `own-range`, `make-zero`, `make-positional`, `menu-header`, `flag-zero`, `flag-negative`, `flag-header-range`, `flag-over-header`, `flag-dimension`, `options-zero`, `one-ok`, `flag-one-ok`, `invalid-amounts`. 2 others: `optional-ranged`, `issue2`). Those that pass at that commit (expected: `amount-any`, `amount-none`, `amount-positive`, `amount-wrong-dim`, `servings-own-range`, `servings-one-ok`, `servings-flag-one-ok`, `servings-flag-over-header`) go in a separate commit, with the reason in the exceptions | `scripts/golden-entries.sh tests/golden/bounds \| wc -l` is 51, and the prerequisite `G-REPO-RED-FIRST`. Record the names of the red and control goldens in §9 |
| P3-2 | Register `TYP021` (at the end of `diag_codes.def`, `diag-names.txt`, `phrases/*.ftl`, `names.mjs`) and report it in `eval_ingredients`. Check whether bought amounts and split amounts have the same hole, and write it in §9 | `amount-*` pass, lines 1 to 6 of `G-QC-DIAG`, and the prerequisites `G-CORE-DIAG-REGISTRY`, `G-CORE-DIAG-APPEND`, `G-STD-PHRASES`, `G-REND-PHRASES-RENDER`, `G-REND-TESTS`, `G-API-SARIF`, `G-CORE-GOLDEN`, `G-RF-SEPARATION`. Mutation checks for S728 to S739 and S772 |
| P3-3 | Make `Options.servings` `Count? in 1..`, check the servings paths (headers, `make`, menu headers, `-s`, `--options`), and remove duplicates of the same value (D77). Suppress amount diagnostics when the servings are out of range (D76) | `servings-*`, `optional-ranged`, `issue2`, `api/types-json`, `settings-servings-min` pass, `G-QC-GOLDEN-BOUNDS`, `G-QC-PRELUDE` (all lines), and the prerequisites `G-CORE-GOLDEN`, `G-SAMPLE-CHECK`, `G-REND-TESTS`, `G-RF-SEPARATION`. Mutation checks for S741 to S750, S773 to S780 and S785 |

### Phase 4 — Documents and finishing

| ID | Content | Completion condition |
| --- | --- | --- |
| P4-1 | In `docs/concept.md` §4.2, add ranges that exclude the lower end, and add action arguments to the things for which out of range is an error. Fix `servings` in the `type Options` example. Add "amounts of 0 or less" to the error examples in §6.1. Add a line to `severity-examples.txt` | `G-QC-SYNTAX` (all lines), `G-QC-DIAG` (all lines), and the prerequisites `G-TYPE-SEVERITY-TABLE`, `G-RF-CONCEPT`. Mutation checks for S740, S756, S783, S784 |
| P4-2 | Coverage audit: add the facts that grew in §9 to the coverage table. Judge each item of §8 again, with the knowledge gained from the implementation, by "can a command decide it?". Have an independent subagent read §1, §4, §8, the coverage table, the added goldens and the diff of `sema.c`, and list claims without tests and items in §8 that a command can decide. For each finding, add it, or write the reason for not adding it at the end of §5.3 | `G-QC-COVERAGE`, `G-QC-PLAN-NAMES`, `G-QC-AUDIT`. The number of audit findings and how they were handled, in §9 |
| P4-3 | Final gate. Do the mutation checks for S751 to S754 and S782. Run `G-REPO-NOREPLY`, push, and check CI | All goals of §5 (except `G-QC-ISSUES`), including `G-QC-PLAN-TESTS`, `G-QC-SCOPE`, `G-QC-VERSION`, `G-QC-GOLDEN-CHANGED`, the local prerequisite goals, and `G-PLAT-CI` after the push |
| P4-4 | Reply to the Issues and close them as D84 says. Show the text to the user before sending. Hand §8 to the user, and propose the separate work that was put out of scope (§1) | `G-QC-ISSUES` |

---

## 7. Dependencies between phases

- P0-1 → P0-2 → P0-3, in this order. Without the goals of P0-3, the completion conditions of later tasks cannot be judged.
- Phase 1 → Phase 2 → Phase 3, in this order. The ranges of standard actions in Phase 2 (P2-3) and the range of `make` (P2-2) have no effect without the check from Phase 1. The range of `Options.servings` in Phase 3 (P3-3) needs the fix to `base_type` from P2-2. `amount-range-units` (P3-1) needs the range evaluation from P1-2.
- Within each phase, the tasks that write goldens (P1-1, P2-1, P3-1) come before the implementation. P2-2 and P2-3, and P3-2 and P3-3, partly touch the same file (`sema.c`), so do them in order, not in parallel.
- P4-1 comes after Phase 3 (the example in §6.1 makes `G-TYPE-SEVERITY-TABLE` fail unless `TYP021` is registered first). P4-2 → P4-3 → P4-4, in this order.

---

## 8. Checks handed over to the user

Claude is responsible up to the machine-checkable goals. The user checks the items below. Each item has the reason a command cannot decide it. Items for which no reason can be written become tests in §5.3.

- Whether the Japanese and English wording of `TYP021` (`diag-non-positive-amount`) tells recipe writers what to fix (that the phrases exist and use both arguments is checked by `G-QC-DIAG`, and that they can be rendered by `G-REND-PHRASES-RENDER`) — Reason a command cannot decide: whether wording gets through is up to the reader
- Whether the explanation in `docs/concept.md` §4.2 of ranges that exclude the lower end conveys the meaning of `<..` without misunderstanding (that the examples and words exist is checked by `G-QC-SYNTAX`) — Reason a command cannot decide: whether an explanation gets through is up to the reader
- The wording of the replies to Issues #1 and #2 (D84; shown before sending. That they contain the diagnostic codes, the SHA of a commit in the range, the output lines after the fix and the name `bounds/issue2` is checked by `G-QC-ISSUES`) — Reason a command cannot decide: the wording of a public post is a human judgment

---

## 9. Items to check and record when starting

(Append the results checked in each task here, with dates.)

- Independent audit after the plan was written (2026-11-02): 30 findings. 25 were taken, and 5 were not taken, being treated as out of scope or as limits of existing checks (the reasons are at the end of §5.3). The main changes: added the design decisions for evaluating ranges with units on both ends (D85), quantity arguments of `make` (D86) and goals for the ranges of completed plans (D87); the rules that remove duplicate servings from options and duplicate amount diagnostics caused by wrong servings (D76, D77); the positions of diagnostics in composite actions and macro splices; fixes to 8 ways of breaking that did not work; fixes to `G-QC-PRELUDE` (missed temperature ranges), `G-QC-GOLDEN-CHANGED` (tag check) and `G-QC-SYNTAX` (the concept section moved to §4.2, rule name `type_range`); adding `G-RF-SEPARATION` to the prerequisite goals; and fixing a mistake in how build-time options are written (`kitchen today : Options`)
- Baseline at the starting point (P0-2, 2026-11-02): the tag `qc-start` is `0b4b4eb` (the commit of this plan). The golden count N0 is **464**. We ran 116 goals in total, the 115 prerequisite goals and `G-CORE-BUILD`; 114 passed and 2 failed. Both failures are things this plan is going to fix:
  - `G-RF-GOLDEN-COUNT`: there are 464 goldens, not 448, because other work added goldens. D80 changes the form of the check (P0-3).
  - `G-RD-SCOPE`: 57 files outside the scope of the range `readme-start..HEAD`, because other work after the README plan entered the range. D87 closes the range with `readme-end` (P0-3).
  - In the first run, `G-PLAT-WASM` and `G-REPO-SELFTESTS` also failed. The cause was that the local `build/core-wasm/` was an old build from 2026-11-01 (the 17 golden mismatches and 10 worker mismatches were all for new diagnostics). After rebuilding with `scripts/build-wasm.sh`, both passed. It was not a code problem.
- Between `8db17e6`, where the plan was written, and `qc-start`, there are 24 commits from other work (`0b7db9e` to `d181933`; adding `NAM019` to `NAM021`, adding 16 goldens, and so on). The line numbers in §3 are as of `8db17e6`; at `qc-start` they shift about 25 lines down (for example, `check_value` is at line 2665 and `call_action` at line 4074). `TYP021` is added after `NAM021`.
- The oracle for `bounds/expression-unchanged` is `build/qc-start-ozen`, a copy of `build/core-gcc-release/ozen` built at `qc-start` (`build/` is not tracked by git; if it is lost, check out `qc-start` in a worktree and rebuild).
- Pass or fail of `G-QC-*` at P0-3 (the list of goals that fail before the implementation changes)
- Red check (P1-1, P2-1, P3-1): the names of the goldens that failed with the build at the commit that added them (red), and of those that passed (control; the commits listed in `scripts/red-first-exceptions.txt`)
- Whether reading `<..` in type ranges conflicts with existing parsing (P2-2; check the expectation of §3.2 with the real parser), and how it is represented in the AST (a new node kind or a mark)
- Whether arguments inside state types (`T<shape: cut(slice, 0 mm)>`) have the same hole (P1-2)
- Whether bought amounts and split amounts have a hole that lets amounts of 0 or less through (P3-2)
- Whether ranged fields of user-defined equipment settings types appear in the output of `--emit types` (P2-1; if not, rewrite `types-json-exclusive` with a type that does appear, and write the reason)
- Goldens with corrected expectations (each task): write one line each in the form `- Golden with a corrected expectation: \`<path>\` — <理由>`
- Other work's commits in the range (if any): write them in the form `- Other work's commit in range: <SHA>（<内容>）`
- Mutation check records (each task): write them in the form `- Mutation check: S<n>・<テストかゴール>（<タスク>、<日付>）。<壊し方> で落ちた。戻すと合格した`
- Coverage audit (P4-2): write it in the form `- Coverage audit: <k> findings, <m> taken`
- Pass or fail of `G-QC-*` at P0-3 (2026-11-02, right after adding the 18 goals to `goals.sh`): 8 passed: `G-QC-DECISIONS`, `G-QC-COVERAGE`, `G-QC-GOLDEN-CHANGED`, `G-QC-SCOPE`, `G-QC-VERSION`, `G-QC-CLAUDEMD`, `G-QC-RF-COUNT`, `G-QC-RD-RANGE`. 10 failed: `G-QC-PLAN-NAMES` (`75 names, 73 missing`), `G-QC-PLAN-TESTS` (`107 rows, 0 recorded`), `G-QC-GOLDEN-ARGS`, `G-QC-GOLDEN-BOUNDS` (no goldens), `G-QC-PRELUDE` (3 of 4), `G-QC-DIAG` (all 7), `G-QC-SCHEMA`, `G-QC-SYNTAX` (all 7), `G-QC-ISSUES`, `G-QC-AUDIT`. `G-RF-GOLDEN-COUNT`, `G-RD-SCOPE`, `G-RD-ADD-ONLY`, `G-REPO-CLAUDEMD` and `G-REPO-SELFTESTS` passed
- Mutation check: S755, Fixed premise, `G-QC-RF-COUNT` (P0-3, 2026-11-02). Reverting `goal_G_RF_GOLDEN_COUNT` to its original form, which compares with the current count, made it fail with "readme-start in G-RF-GOLDEN-COUNT: got '0'" and "G-RF-GOLDEN-COUNT: exit 1". Restoring it made it pass
- Mutation check: S755, Side effects, `G-RF-GOLDEN-COUNT` (P0-3, 2026-11-02). Removing the golden `linear/unused-alias`, which exists at `readme-start`, in a temporary worktree made it fail with "readme-start goldens missing now: got '1'". Removing the worktree made it pass
- Mutation check: S781, Fixed premise, `G-QC-RD-RANGE` (P0-3, 2026-11-02). Reverting `RD=` in `goals.sh` to `readme-start..HEAD` made it fail with "RD range: got '0'", and `G-RD-SCOPE` also failed with "files outside scope: got '57'" (it had grown further through later work from the 2 at the time the plan was written). Restoring it made it pass
- Mutation check: S757, Normal, `G-QC-CLAUDEMD` (P0-3, 2026-11-02). Changing `qc-start` to `qc` in this plan's line in `CLAUDE.md` made it fail with "plan line in CLAUDE.md: got '0'". Restoring it made it pass
- Red check (P1-1, 2026-11-02): the 18 goldens added in `9426fff` (`volume-for-time`, `mass-for-length`, `unitless-time`, `count-param`, `named-arg`, `pipe-arg`, `variable-arg`, `macro-body`, `macro-splice`, `composite`, `user-dimension`, `continues-after`, `issue1`, `range-units-steps`, `range-units-dimension`, `range-units-mixed`, `range-units-ingredient`, `make-arg-dimension`) all failed with the starting-point build (red). The 3 control goldens added in `144bba6` (`ok-units`, `unknown-value`, `default-unchecked`) passed and were listed in `scripts/red-first-exceptions.txt`
- How `args/unknown-value` is written (P1-1, 2026-11-02): instead of the expectation in §5.3 (the style of `eval/runtime-value-in-if`), it passes a recipe argument without a default value, `recipe tamagoyaki(t: Time)`, to an action. When checked on its own, `t` has no value (`debug steps` shows `fry(egg, t)`), and no other diagnostic, such as the `TYP015` for comparisons, gets mixed in
- The shape of `args/composite` (P1-1, 2026-11-02): it puts two composite actions side by side: `quickA`, which does not use the argument inside, and `quickB`, which passes the argument to the inner `fry(x, t)`. With `quickB` alone, even if the outer check is skipped, the inner check reports the same diagnostic at the same position, so the mutation check for S702 would have no effect
- The goldens for user actions (`count-param`, `user-dimension`, `default-unchecked`) use tofu (`RawEdible`), because with egg (`Hazard`) the result of an action that does not heat becomes `SAF001`
- Arguments inside state types (P1-2, 2026-11-02): the same hole exists. For a user action's argument type, both `T<shape: cut(slice, 3 ml)>` and `T<shape: cut(slice, 0 mm)>`, and for the ingredient type `Cucumber<shape: cut(slice, 3 ml)>`, give `result ok` (a value cut to `3 mm` passes when given to an argument of type `3 ml`). The check path is different, so this plan does not fix it; it will be proposed as separate work after the final gate
- Fix to `args/unknown-value` (P1-2, 2026-11-02): the mutation check for S704 (checking unknown values as dimensionless quantities) had no effect. The recipe argument `t` without a default value evaluates to an enum value (`V_ENUM`) as a name with no binding and no definition, and does not become an unknown value (`V_UNKNOWN`). After changing the argument to `t + 1 min` (the sum of an enum value and a quantity, which evaluates to an unknown value), the same way of breaking made it fail. The expectation (`.out`) stays `result ok`, unchanged
- Replacing the way of breaking for S701 (P1-2, 2026-11-02): "do not set the trail of `arg_site`" from the coverage table has no effect. For nodes that are not splices, `arg_site` returns the trail being evaluated (while the macro body is evaluated, that is the expansion history itself), so the trail is the same even without setting it. Instead, we checked with "make the trail of the check's diagnostic empty"
- Mutation check: S695, Normal, `cgolden/args/volume-for-time` (P1-2, 2026-11-02). Removing the call to the check in `call_action` made 14 goldens fail, including `volume-for-time`. Restoring it made them pass
- Mutation check: S695, Normal, `cgolden/args/mass-for-length` (P1-2, 2026-11-02). Narrowing the check so it only works on `Time` declarations made `mass-for-length`, `count-param` and `user-dimension` fail. Restoring it made them pass
- Mutation check: S696, Boundary, `cgolden/args/unitless-time` (P1-2, 2026-11-02). Skipping dimensionless quantities before the check made only `unitless-time` fail. Restoring it made it pass
- Mutation check: S697, No-effect condition, `cgolden/args/count-param` (P1-2, 2026-11-02). Removing the step in the check that turns `pcs` into a dimensionless quantity made only `count-param` fail. Restoring it made it pass
- Mutation check: S698, Real path, `cgolden/args/named-arg` (P1-2, 2026-11-02). Limiting the check to arguments passed by position made only `named-arg` fail. Restoring it made it pass
- Mutation check: S699, Real path, `cgolden/args/pipe-arg` (P1-2, 2026-11-02). Attaching the left side as the last argument in `pipe_into` made `pipe-arg`, `mass-for-length` and `ok-units` fail. Restoring it made them pass
- Mutation check: S700, Normal, `cgolden/args/variable-arg` (P1-2, 2026-11-02). Putting the position of the diagnostic on the whole call made 14 goldens fail, including `variable-arg`. Restoring it made them pass
- Mutation check: S701, Real path, `cgolden/args/macro-body` (P1-2, 2026-11-02). Making the trail of the check's diagnostic empty made only `macro-body` fail. Restoring it made it pass
- Mutation check: S758, Real path, `cgolden/args/macro-splice` (P1-2, 2026-11-02). Setting the position of the check's diagnostic to the argument node itself (the `$t` in the macro body), without going through `arg_site`, made only `macro-splice` fail. Restoring it made it pass
- Mutation check: S702, Real path, `cgolden/args/composite` (P1-2, 2026-11-02). Skipping the outer check for composite actions made only `composite` fail. Restoring it made it pass
- Mutation check: S759, Side effects, `cgolden/args/composite` (P1-2, 2026-11-02). Reporting the check's diagnostic in the inner call even for arguments that failed the outer check made only `composite` fail (`5 g` got 2). Restoring it made it pass
- Mutation check: S703, Normal, `cgolden/args/user-dimension` (P1-2, 2026-11-02). Limiting the check to the 6 standard dimensions made only `user-dimension` fail. Restoring it made it pass
- Mutation check: S704, No-effect condition, `cgolden/args/unknown-value` (P1-2, 2026-11-02). Also checking unknown quantities, treating them as dimensionless quantities, made `unknown-value` and `range-units-mixed` fail. Restoring it made them pass
- Mutation check: S705, No-effect condition, `cgolden/args/default-unchecked` (P1-2, 2026-11-02). Also calling the check after evaluating default values made only `default-unchecked` fail. Restoring it made it pass
- Mutation check: S706, Side effects, `cgolden/args/continues-after` (P1-2, 2026-11-02). Returning an unknown value from `call_action` on a dimension error made 12 goldens fail, including `continues-after`. Restoring it made them pass
- Mutation check: S707, No-effect condition, `cgolden/args/ok-units` (P1-2, 2026-11-02). Making interval values an error made `ok-units` and `range-units-steps` fail. Restoring it made them pass
- Mutation check: S707, No-effect condition, `cgolden/args/ok-units` (P1-2, 2026-11-02). Also checking enum values as dimensionless quantities, and reporting dimension errors for arguments of non-dimension types too, made `ok-units`, `mass-for-length` and `unknown-value` fail. Restoring it made them pass
- Mutation check: S708, Real path, `cgolden/args/issue1` (P1-2, 2026-11-02). Making the severity of `TYP001` a warning made 16 goldens fail, including `issue1`. Restoring it made them pass
- Mutation check: S760, Normal, `cgolden/args/range-units-steps` (P1-2, 2026-11-02). Removing the evaluation of ranges in `eval_inner` made 4 goldens fail, including `range-units-steps`. Restoring it made them pass
- Mutation check: S761, Real path, `cgolden/args/range-units-dimension` (P1-2, 2026-11-02). Not recording the dimension of the result in the range evaluation made `range-units-dimension`, `range-units-steps` and `range-units-ingredient` fail. Restoring it made them pass
- Mutation check: S762, Boundary, `cgolden/args/range-units-mixed` (P1-2, 2026-11-02). Using the dimension of the left end without comparing the dimensions of the two ends made only `range-units-mixed` fail. Restoring it made it pass
- Mutation check: S763, Data completeness, `cgolden/args/range-units-ingredient` (P1-2, 2026-11-02). Making range nodes unknown when evaluating ingredient amounts made only `range-units-ingredient` fail. Restoring it made it pass
- Mutation check: S764, Real path, `cgolden/args/make-arg-dimension` (P1-2, 2026-11-02). Removing the check of quantity arguments of `make` made only `make-arg-dimension` fail. Restoring it made it pass
- Red check (P2-1, 2026-11-02): the 15 goldens added in `b894674` (`exclusive-from`, `exclusive-upper-inclusive`, `exclusive-upper-over`, `exclusive-interval`, `exclusive-kitchen`, `exclusive-count`, `exclusive-empty`, `types-json-exclusive`, `make-arg-range`, `time-zero`, `time-negative`, `range-units-negative-time`, `size-zero`, `size-negative`, `composite-range`) all failed with the build at the time they were added (after the P1 implementation) (red). The 7 control goldens added in `095a7c8` (`closed-zero-ok`, `expression-unchanged`, `small-positive`, `soak-default`, `temperature-negative`, `dimension-before-range`, `exclusive-space`) passed and were listed in `scripts/red-first-exceptions.txt`
- User equipment settings types (P2-1, 2026-11-02): the ranged field of `type ScaleSpec : EquipmentSpec { maxLoad: Mass in 0 g.. }` appears in `--emit types` under `Scale` in `equipment` as `"min":"0 g"`. `types-json-exclusive` was written with this type
- How the expectation of `bounds/types-json-exclusive` was made (P2-1, 2026-11-02): the JSON of the type list contains all standard equipment and vessel types, and is long to write by hand. So we took as a base the output of the starting-point build `build/qc-start-ozen` on a file of the same name with the range closed (`0 g..`), and changed only the `maxLoad` field by hand to `"min":"0 g","minExclusive":true,"max":null`. The other parts do not change in this plan (D79), so the implementation before the change is the oracle. In P3 the `min` of `Options.servings` becomes `"1"`, so the expectation is corrected then
- The expectation of `bounds/expression-unchanged` (P2-1, 2026-11-02): as §5.2 says, it was made from the output of `build/qc-start-ozen debug ast` (`3 min<..5 min` is read as `Binary <` and `RangeTo`)
- How `<..` is read and represented in the AST (P2-2, 2026-11-02): the new rule `type_range` is a function that reads only after `in` in `type()`. Only when a `<` follows right after the value read by `range()`, and the next token is `..` with no space in between, does it create the new node kind `RangeAbove` (its children are the lower end and, if any, the upper end). It is a node kind, not a mark, because the 3 places that read ranges (`check_qty`, `put_field`, and the range text in diagnostics) all branch on the node kind. `RangeAbove` and the rule `type_range` were added at the end of the node kinds in `ast.h` and at the end of `rules.def`, so that existing numbers do not change. All existing goldens still pass unchanged (including type arguments such as `T<shape: cut>`), and `3 min<..5 min` in an expression gives the same tree as before, as `bounds/expression-unchanged` shows. As expected in §3.2, there is no conflict
- `"min": {` appears in 4 places in `schemas/result.schema.json` (the fields of the equipment, vessel, kitchen and options types), and `minExclusive` was added to all 4 (`G-QC-SCHEMA` checks that the counts are equal)
- Where the way of breaking for S718 is (P2-2, 2026-11-02): `mark(p, OZNI_R_type_range)` was put not in `type()` but at the start of `type_range()`, which `type()` calls. The mutation check was done by removing it there
- Mutation check: S765, Real path, `cgolden/bounds/make-arg-range` (P2-2, 2026-11-02). Passing the type without its range to the `make` check made `make-arg-range` fail. Restoring it made it pass
- Mutation check: S709, Boundary, `cgolden/bounds/exclusive-from` (P2-2, 2026-11-02). Making the exclusive lower-end check a closed check (letting exactly the lower end through) made 6 goldens fail, including `exclusive-from`. Restoring it made them pass
- Mutation check: S710, Boundary, `cgolden/bounds/exclusive-upper-inclusive` (P2-2, 2026-11-02). Making the upper-end check `>=` made `exclusive-upper-inclusive` fail. Restoring it made it pass
- Mutation check: S710, Boundary, `cgolden/bounds/exclusive-upper-over` (P2-2, 2026-11-02). Dropping the upper end of `a<..b` in `type_range()` made `exclusive-upper-over` fail. Restoring it made it pass
- Mutation check: S711, No-effect condition, `cgolden/bounds/exclusive-space` (P2-2, 2026-11-02). Allowing spaces between `<` and `..` made `exclusive-space` fail. Restoring it made it pass
- Mutation check: S712, Normal, `cgolden/bounds/exclusive-from` (P2-2, 2026-11-02). Making the third argument of the diagnostic only the text of the lower end made 7 goldens fail, including `exclusive-from`. Restoring it made them pass
- Mutation check: S713, Boundary, `cgolden/bounds/exclusive-interval` (P2-2, 2026-11-02). Using the value's upper end in the lower-end check of interval values made `exclusive-interval` fail. Restoring it made it pass
- Mutation check: S714, No-effect condition, `cgolden/bounds/closed-zero-ok` (P2-2, 2026-11-02). Using the exclusive lower-end check for closed ranges too made `closed-zero-ok` fail. Restoring it made it pass
- Mutation check: S715, No-effect condition, `cgolden/bounds/expression-unchanged` (P2-2, 2026-11-02). Reading `<..` in `range()` (shared with expressions) made `expression-unchanged` and `exclusive-space` fail. Restoring it made them pass
- Mutation check: S766, Real path, `cgolden/bounds/exclusive-kitchen` (P2-2, 2026-11-02). Ignoring exclusive ranges in `check_value` (the kitchen settings path) made `exclusive-kitchen` fail. Restoring it made it pass
- Mutation check: S767, Boundary, `cgolden/bounds/exclusive-count` (P2-2, 2026-11-02). Dropping the exclusive mark when the lower end is only a number made `exclusive-count` and `exclusive-empty` fail. Restoring it made them pass
- Mutation check: S768, Boundary, `cgolden/bounds/exclusive-empty` (P2-2, 2026-11-02). Making the lower-end check closed for ranges whose lower and upper ends are equal made `exclusive-empty` fail. Restoring it made it pass
- Mutation check: S716, Data completeness, `cgolden/bounds/types-json-exclusive` (P2-2, 2026-11-02). Not outputting `minExclusive` made `types-json-exclusive` fail. Restoring it made it pass
- Mutation check: S716, No-effect condition, `cgolden/api/types-json` (P2-2, 2026-11-02). Outputting `minExclusive` for every range with a lower end, set to `false` for inclusive ranges, made `api/types-json` fail. Restoring it made it pass
- Mutation check: S717, Data completeness, `G-QC-SCHEMA` (P2-2, 2026-11-02). Removing `minExclusive` from the type fields of the schema made it fail with "minExclusive beside min: got '0'" and "schema: got 'json: 1 files, 1 invalid'". Restoring it made it pass
- Mutation check: S718, Data completeness, `G-CORE-GRAMMAR` (P2-2, 2026-11-02). Removing `mark(p, OZNI_R_type_range)` from `type_range()` made it fail with "grammar: 134 rules, 1 not covered". Restoring it made it pass
- Mutation check: S718, Docs match, `G-QC-SYNTAX` (P2-2, 2026-11-02). Removing the `type_range` rule from `core/grammar.ebnf` made it fail with "grammar type_range: got '0'". Restoring it made it pass
- Mutation check: S719, Data completeness, `G-QC-SYNTAX` (P2-2, 2026-11-02). Removing `"<.."` from `core/fuzz/ozen.dict` made it fail with "fuzz dict: got '0'". Restoring it made it pass
- Golden with a corrected expectation: `tests/golden/api/signatures-json.out` — the type text of 8 standard arguments (`time` of `soak`, `fry`, `simmer`, `boil`, `parboil`, `chill`, `rest`, and `size` of `cut`) now includes the range (`Time in 0 s<..`, `Length in 0 mm<..`) (S769, P2-3). The expectation was made by rewriting the 8 matching places of the old expectation by hand, and we confirmed that it matches the implementation's output. The `time: Time` of the user action `press` does not change
- Golden with a corrected expectation: `tests/golden/api/local-not-listed.out` — in the JSON of the list of definitions, the types of the same 8 arguments changed from `TypeRef` to `RangedType` (with the type name and `RangeAbove` as children) (P2-3). We read the diff and confirmed that nothing changed besides the type trees in these 8 places before correcting it
- Mutation check: S720, Data completeness, `G-QC-PRELUDE` (P2-3, 2026-11-02). Removing the range of `time` in `chill` made it fail with "time ranges: got '6'". Restoring it made it pass
- Mutation check: S721, Data completeness, `G-QC-PRELUDE` (P2-3, 2026-11-02). Making the range of `size` in `cut` `0 mm..` made it fail with "size range: got '0'". Restoring it made it pass
- Mutation check: S722, Boundary, `cgolden/bounds/time-zero` (P2-3, 2026-11-02). Making the range of `simmer` `0 s..` made `time-zero` and `range-units-negative-time` fail. Restoring it made them pass
- Mutation check: S723, Normal, `cgolden/bounds/time-negative` (P2-3, 2026-11-02). Removing the range of `boil` made `time-negative` fail. Restoring it made it pass
- Mutation check: S770, Real path, `cgolden/bounds/range-units-negative-time` (P2-3, 2026-11-02). Removing the range of `simmer` made `range-units-negative-time` and `time-zero` fail. Restoring it made them pass
- Mutation check: S724, Boundary, `cgolden/bounds/size-zero` (P2-3, 2026-11-02). Making the range of `cut` `0 mm..` made `size-zero` and `size-negative` fail (for `-3 mm` too, the range text in `TYP012` changes). Restoring it made them pass
- Mutation check: S724, Normal, `cgolden/bounds/size-negative` (P2-3, 2026-11-02). Removing the range of `cut` made `size-negative` and `size-zero` fail. Restoring it made them pass
- Mutation check: S725, No-effect condition, `cgolden/bounds/small-positive` (P2-3, 2026-11-02). Making the range of `fry` `1 min<..` made `small-positive` and `composite-range` fail. Restoring it made them pass
- Mutation check: S725, No-effect condition, `cgolden/bounds/soak-default` (P2-3, 2026-11-02). Making calls that use the default value checked too, and then making the range of `soak` `30 min<..`, made `soak-default` fail. Restoring it made it pass
- Mutation check: S726, No-effect condition, `cgolden/bounds/temperature-negative` (P2-3, 2026-11-02). Applying a check that treats 0 or less as out of range to all arguments of dimension types without a range made `temperature-negative` and `composite-range` fail. Restoring it made them pass
- Mutation check: S726, Data completeness, `G-QC-PRELUDE` (P2-3, 2026-11-02). Making `OvenSpec.maxTemp` `Temperature? in 0 degC..` made it fail with "temperature ranges: got '1'". Restoring it made it pass
- Mutation check: S727, Side effects, `cgolden/bounds/dimension-before-range` (P2-3, 2026-11-02). Continuing the range check even when the dimension differs made `dimension-before-range` fail. Restoring it made it pass
- Mutation check: S771, Side effects, `cgolden/bounds/composite-range` (P2-3, 2026-11-02). Reporting the position of the check inside a composite action at the inner call as it is, instead of pointing it to the caller, made `composite-range` fail. Restoring it made it pass
- Mutation check: S769, Data completeness, `cgolden/api/signatures-json` (P2-3, 2026-11-02). Making the type text of arguments in `--emit signatures` only the type name without the range made `signatures-json` fail. Restoring it made it pass
- Bought amounts and split amounts (P3-2, 2026-11-02): the same hole exists. Both `let naan = buy Naan 0 pcs` and `split(water, -50 ml)` (`debug steps` shows `(Water -50 ml, Water 250 ml)`) give `result ok`. The Issue only mentions ingredient amounts, so this plan does not fix it; it will be proposed as separate work after the final gate
- The order in the expectation of `bounds/amount-per-serving-unknown` (P3-2, 2026-11-02): the hand-written expectation had the order `TYP021`, `TYP020`, but diagnostics are output sorted by position and then diagnostic code (`cmp_diag` in `core/src/render.c`), so at the same position `TYP020` comes first. This is not a mistake in the spec (S733) but a mistake in the order, so the 2 lines of the expectation were swapped (in a separate commit from the implementation, following `G-RF-SEPARATION`)
- Golden with a corrected expectation: `tests/golden/api/local-not-listed.out` — the list of diagnostics gained one entry, `TYP021 nonPositiveAmount` (P3-2)
- Mutation check: S728, Normal, `cgolden/bounds/amount-negative` (P3-2, 2026-11-02). Removing the code that reports `TYP021` made 9 goldens fail, including `amount-negative`. Restoring it made them pass
- Mutation check: S729, Boundary, `cgolden/bounds/amount-zero` (P3-2, 2026-11-02). Making the check `lo < 0` made 5 goldens fail, including `amount-zero`. Restoring it made them pass
- Mutation check: S730, Normal, `cgolden/bounds/amount-negative` (P3-2, 2026-11-02). Making the second argument of the diagnostic the type name of the ingredient made 9 goldens fail, including `amount-negative`. Restoring it made them pass
- Mutation check: S731, Boundary, `cgolden/bounds/amount-range-zero` (P3-2, 2026-11-02). Using the upper end of the interval in the check made `amount-range-zero` fail. Restoring it made it pass
- Mutation check: S731, Boundary, `cgolden/bounds/amount-range-negative` (P3-2, 2026-11-02). Making the check `lo < -1` made 6 goldens fail, including `amount-range-negative` (the `TYP021` for `-0.5..0.8 g` disappears). Restoring it made them pass
- Mutation check: S731, No-effect condition, `cgolden/bounds/amount-positive` (P3-2, 2026-11-02). Making the check `lo <= 1` made many goldens fail, including `amount-positive` (goldens that use ingredients of `1 pcs`). Restoring it made them pass
- Mutation check: S772, Real path, `cgolden/bounds/amount-range-units` (P3-2, 2026-11-02). Applying the `TYP021` check only to quantity literal, sign, number and numeric range nodes made `amount-range-units` and `amount-expression` fail. Restoring it made them pass
- Mutation check: S732, Normal, `cgolden/bounds/amount-per-serving-zero` (P3-2, 2026-11-02). Skipping the check for `per serving` amounts made `amount-per-serving-zero` and `amount-per-serving-unknown` fail. Restoring it made them pass
- Mutation check: S733, Side effects, `cgolden/bounds/amount-per-serving-unknown` (P3-2, 2026-11-02). Skipping the check for `per serving` amounts whose servings are not known made `amount-per-serving-unknown` fail. Restoring it made it pass
- Mutation check: S734, No-effect condition, `cgolden/bounds/amount-any` (P3-2, 2026-11-02). Treating `[any amount]` as 0 and reporting `TYP021` made `amount-any` fail. Restoring it made it pass
- Mutation check: S735, No-effect condition, `cgolden/bounds/amount-none` (P3-2, 2026-11-02). Treating ingredients whose amount is not decided as 0 in the check made `amount-none` fail. Restoring it made it pass
- Mutation check: S736, Side effects, `cgolden/bounds/amount-wrong-dim` (P3-2, 2026-11-02). Continuing the check after a dimension error made `amount-wrong-dim` fail. Restoring it made it pass
- Mutation check: S737, Normal, `cgolden/bounds/amount-expression` (P3-2, 2026-11-02). Applying the check only to amounts that are not expressions (binary operations) made `amount-expression` fail. Restoring it made it pass
- Mutation check: S738, No-effect condition, `cgolden/bounds/amount-positive` (P3-2, 2026-11-02). Making the check the same as `lo <= 0.5 g` made `amount-positive` fail. Restoring it made it pass
- Mutation check: S739, Data completeness, `G-QC-DIAG` (P3-2, 2026-11-02). Removing `{ $arg2 }` from `diag-non-positive-amount` in `phrases/ja.ftl` made it fail with "ja phrase: got '0'". Restoring it made it pass
- Mutation check: S739, Data completeness, `G-CORE-DIAG-REGISTRY` (P3-2, 2026-11-02). Removing `TYP:nonPositiveAmount` from `core/tests/diag-names.txt` made it fail with "1 unlisted". Restoring it made it pass
- Mutation check: S739, Fixed premise, `G-CORE-DIAG-APPEND` (P3-2, 2026-11-02). Moving `TYP021` to the line right after `TYP020` in a work commit in a temporary worktree made it fail with "deleted registry lines: got '1'". It was removed with `git reset --hard`, and the worktree was removed (not pushed)
- How the servings paths are built (P3-3, 2026-11-02): servings for which the header does not declare its own range (untyped `servings: 0`, `servings: Count = 0`, menu headers, `make` arguments) are checked using the type of `Options.servings` in the standard library (`Count? in 1..`) as their type. The range text `1..` comes from there. Servings from options (`-s`, `--options`) are marked when they fail the check on the options side; they are not checked on the header side, and the servings are treated as unknown. In a dish whose servings became unknown because they were out of range or had the wrong dimension, no `TYP020` is reported for the total of `per serving` amounts, and amounts in expressions that use `servings` cannot be evaluated and become unknown (no `TYP021` either)
- Replacing the way of breaking for S773 (P3-3, 2026-11-02): "move the check to after multiplying by the servings" from the coverage table has no effect. Out-of-range servings are treated as unknown, so the multiplication itself does not happen and `100 g per serving` does not become 0 g. Instead, we checked with two ways: "use out-of-range servings as they are, without making them unknown" (`150 ml * servings` becomes `0 ml` and `TYP021` appears), and "report `TYP020` even when the servings are out of range"
- Golden with a corrected expectation: `tests/golden/api/types-json.out` — the `min` of `Options.servings` changed from `null` to `"1"` (S741; corrected by hand in P3-1)
- Mutation check: S773, Side effects, `cgolden/bounds/servings-invalid-amounts` (P3-3, 2026-11-02). Using out-of-range servings as they are, without making them unknown, made `servings-invalid-amounts` fail (`TYP021` on `150 ml * servings`). Reporting `TYP020` even when the servings are out of range also made it fail. Restoring it made it pass
- Mutation check: S785, No-effect condition, `cgolden/bounds/servings-invalid-amounts` (P3-3, 2026-11-02). Also skipping the check of `per serving` amounts when the servings are out of range made `servings-invalid-amounts` fail. Restoring it made it pass
- Mutation check: S741, Data completeness, `cgolden/api/types-json` (P3-3, 2026-11-02). Removing the range of `Options.servings` made `types-json` fail. Restoring it made it pass
- Mutation check: S774, Real path, `rtest/settings-servings-min` (P3-3, P4-2, 2026-11-02). Making `settingsSpec` drop the `min` of optional types (`Count?`) made `settings-servings-min` fail. Restoring it made it pass (in P3-3 we checked by rewriting the test input, but as audit finding 4-1 points out, that does not break the implementation, so in P4-2 it was replaced with a way of breaking on the implementation side)
- Mutation check: S742, Boundary, `cgolden/bounds/optional-ranged` (P3-3, 2026-11-02). Reverting how `base_type` unwraps nesting made 11 goldens fail, including `optional-ranged`. Restoring it made them pass
- Mutation check: S743, Normal, `cgolden/bounds/servings-header-zero` (P3-3, 2026-11-02). Removing the check of untyped `servings` made `servings-header-zero`, `servings-menu-header` and `servings-invalid-amounts` fail. Restoring it made them pass
- Mutation check: S744, Normal, `cgolden/bounds/servings-typed-zero` (P3-3, 2026-11-02). Not checking `servings` that has only a type made `servings-typed-zero` fail. Restoring it made it pass
- Mutation check: S745, Side effects, `cgolden/bounds/servings-own-range` (P3-3, 2026-11-02). Also checking with `1..` on top even when the declaration has a range made `servings-own-range` fail (`0` got 2). Restoring it made it pass
- Mutation check: S746, Real path, `cgolden/bounds/servings-make-zero` (P3-3, 2026-11-02). Not checking the servings in `make` arguments made `servings-make-zero` and `servings-make-positional` fail. Restoring it made them pass
- Mutation check: S775, Real path, `cgolden/bounds/servings-make-positional` (P3-3, 2026-11-02). Limiting the check of `make` to arguments passed by name made `servings-make-positional` fail. Restoring it made it pass
- Mutation check: S776, Real path, `cgolden/bounds/servings-menu-header` (P3-3, 2026-11-02). Checking servings only in recipe headers (not evaluating `servings` in menu headers) made `servings-menu-header` fail. Restoring it made it pass
- Mutation check: S747, Real path, `cgolden/bounds/servings-flag-zero` (P3-3, 2026-11-02). Skipping `check_value` for option fields made 4 goldens fail, including `servings-flag-zero` and `servings-options-zero`. Restoring it made them pass
- Mutation check: S747, Boundary, `cgolden/bounds/servings-flag-negative` (P3-3, 2026-11-02). Making the servings check only for exactly 0 (letting negative values through) made `servings-flag-negative` fail. Restoring it made it pass
- Mutation check: S777, Side effects, `cgolden/bounds/servings-flag-header-range` (P3-3, 2026-11-02). Checking on the header side even when there is an error on the options side made `servings-flag-header-range` and `servings-flag-dimension` fail. Restoring it made them pass
- Mutation check: S778, Real path, `cgolden/bounds/servings-flag-over-header` (P3-3, 2026-11-02). Ignoring the range declared in the header for servings that come from options made `servings-flag-over-header` fail. Restoring it made it pass
- Mutation check: S779, Side effects, `cgolden/bounds/servings-flag-dimension` (P3-3, 2026-11-02). Removing the deduplication (also checking with the header's type regardless of an error on the options side) made `servings-flag-dimension` (back to the same 2 lines as now) and `servings-flag-header-range` fail. Restoring it made them pass
- Mutation check: S748, Real path, `cgolden/bounds/servings-options-zero` (P3-3, 2026-11-02). Skipping `check_value` for option fields made `servings-options-zero` fail (the same way of breaking as S747). Restoring it made it pass
- Mutation check: S749, Boundary, `cgolden/bounds/servings-one-ok` (P3-3, 2026-11-02). Making the range of `Options.servings` (also used to check the servings of untyped headers) `1<..` made `servings-one-ok` fail. Restoring it made it pass
- Mutation check: S780, Boundary, `cgolden/bounds/servings-flag-one-ok` (P3-3, 2026-11-02). Making the range of `Options.servings` `1<..` made `servings-flag-one-ok` fail. Restoring it made it pass
- Mutation check: S750, Real path, `cgolden/bounds/issue2` (P3-3, 2026-11-02). Removing the range of `boil` made `issue2` and `time-negative` fail. Restoring it made them pass
- Mutation check: S740, Docs match, `G-TYPE-SEVERITY-TABLE` (P4-1, 2026-11-02). Making the severity of "amounts of 0 or less" in `severity-examples.txt` `warning` made it fail with "severity: 18 examples, 1 mismatched". Restoring it made it pass
- Mutation check: S740, Docs match, `G-QC-SYNTAX` (P4-1, 2026-11-02). Removing "amounts of 0 or less" from concept §6.1 made it fail with "concept 6.1 amount: got '0'". Restoring it made it pass
- Mutation check: S756, Docs match, `G-QC-SYNTAX` (P4-1, 2026-11-02). Removing the `0 s<..` example (the sentence and the line of the `boil` example) from concept §4.2 made it fail with "concept 4.2 example: got '0'". Restoring it made it pass
- Mutation check: S783, Docs match, `G-QC-SYNTAX` (P4-1, 2026-11-02). Removing action arguments from the sentence "out of range is always an error" in concept §4.2 made it fail with "concept 4.2 action arguments: got '0'". Restoring it made it pass
- Mutation check: S784, Docs match, `G-QC-SYNTAX` (P4-1, 2026-11-02). Reverting `servings` in the `type Options` example of concept to `Count` made it fail with "concept Options servings: got '0'". Restoring it made it pass
- Coverage audit: 23 findings, 15 taken (P4-2, 2026-11-02). An independent subagent read §1, §4, §8, the coverage table, the added goldens and the implementation diff, and checked by actually running things. Taken: the 5 findings where wrong output actually appeared (2-1 a reversed range becomes unknown, 2-2 using the same argument twice in a composite action gives 2 lines, 2-3 a composite action's default value is checked inside, 2-6 a wrong default value of a recipe argument is reported in 2 places, 2-8 a dish is evaluated as 0 servings with out-of-range servings in `make`), the fix for 2-10 (accepting `0..1 min<..`), goldens for paths that had no test (1-4, 2-7, 2-9, 2-12), the description of the upper end in the documents and `G-QC-SYNTAX` (1-2, 3-2), the added checks in `G-QC-ISSUES` and `G-QC-DIAG` (3-1, 3-3), and replacing the way of breaking for S774 (4-1). S786 to S795 were added to §4, and 5 goldens were added and 3 corrected (§5.2). The reasons for the 8 not taken are at the end of §5.3
- Commits of the fixes after the audit (P4-2, 2026-11-02): goldens were added in `0310942` (red; with the build before the fixes, `args/default-unchecked`, `args/range-units-steps`, `args/recipe-default-once`, `bounds/composite-range`, `bounds/exclusive-range-lower` and `bounds/servings-make-invalid` failed) and `f6f06a3` (control; listed in `scripts/red-first-exceptions.txt`), and the fixes are `f9dbcf9` (reordering the two ends of a range), `f9c4efb` (rejecting `0..1 min<..`), `4e8e19e` (report a composite action's argument only once, and do not check default values) and `08059da` (make recipe arguments that failed their type, and `make` arguments, unknown)
- One-letter names (P4-2, 2026-11-02): the prerequisite goal `G-RF-NAMES` failed on one-letter binding names in this plan's goldens (`let s`, `let t`, `recipe a`, and so on). `4bfd2d3` changed them to `dish`, `time`, `limit`, and only `args/issue1` and `bounds/issue2`, which copy the Issues' reproductions as they are, were listed with reasons in `scripts/test-names-exceptions.txt`. The expectation of `bounds/expression-unchanged` was remade from the starting-point build with the renamed input. The expectation of `bounds/types-json-exclusive` was also remade the same way (the output of the starting-point build, with `minExclusive` and the `min` of `Options.servings` added by hand), with an input that adds a field with an upper end
- Mutation check: S786, Boundary, `cgolden/args/range-units-steps` (P4-2, 2026-11-02). Making a range whose lower end is larger than its upper end an unknown value made `range-units-steps` fail. Restoring it made it pass
- Mutation check: S787, Side effects, `cgolden/bounds/composite-range` (P4-2, 2026-11-02). Not recording in the composite action's frame that the inner check failed made `composite-range` fail (the `0 s` of `twice` got 2). Restoring it made it pass
- Mutation check: S788, No-effect condition, `cgolden/args/default-unchecked` (P4-2, 2026-11-02). Not skipping the frame of a composite action's default values in `qty_site` made `default-unchecked` fail. Restoring it made it pass
- Mutation check: S789, Side effects, `cgolden/args/recipe-default-once` (P4-2, 2026-11-02). Also binding recipe arguments that failed their type made `recipe-default-once` fail (`TYP001` also at the `boil` position). Restoring it made it pass
- Mutation check: S790, No-effect condition, `cgolden/bounds/exclusive-range-lower` (P4-2, 2026-11-02). Accepting a range of numbers with units as the lower end in `type_range()` made `exclusive-range-lower` fail. Restoring it made it pass
- Mutation check: S791, Side effects, `cgolden/bounds/servings-flag-invalid-amounts` (P4-2, 2026-11-02). Binding the servings on the header side even when there is an error on the options side made `servings-flag-invalid-amounts`, `servings-flag-header-range` and `servings-flag-dimension` fail. Restoring it made them pass
- Mutation check: S792, Data completeness, `cgolden/bounds/servings-make-invalid` (P4-2, 2026-11-02). Passing an argument that failed the `make` check to the dish as it is made `servings-make-invalid` fail. Restoring it made it pass
- Mutation check: S793, Real path, `cgolden/bounds/servings-make-own-range` (P4-2, 2026-11-02). Checking the servings of `make` with the type of `Options.servings`, ignoring the header's range, made `servings-make-own-range` fail. Restoring it made it pass
- Mutation check: S794, Data completeness, `cgolden/bounds/types-json-exclusive` (P4-2, 2026-11-02). Not outputting the upper end of `RangeAbove` as `max` in `put_field` made `types-json-exclusive` fail. Restoring it made it pass
- Mutation check: S795, Docs match, `G-QC-SYNTAX` (P4-2, 2026-11-02). Removing "the upper end is included" from concept §4.2 made it fail with "concept 4.2 upper end included: got '0'". Restoring it made it pass
- Other work's commit in range: b215857 (another session added the plan file `plans/ozen-authoring-support.md`; it entered `main` at 16:04 on 2026-11-02)
- Added `scripts/test-names-exceptions.txt` to the files `G-QC-SCOPE` allows (P4-3, 2026-11-02), because in P4-2 the one-letter names of the Issues' reproductions were listed as exceptions (3 places: the table in §4.11, the check command in §5.1, and `goals.sh`)
- Mutation check: S751, Side effects, `G-QC-GOLDEN-CHANGED` (P4-3, 2026-11-02). Removing the line for `tests/golden/api/types-json.out` from the "Golden with a corrected expectation" lines in §9 made it fail with "changed expectations not named in the plan: got '1'". Restoring it made it pass
- Mutation check: S752, Side effects, `G-SAMPLE-CHECK` (P4-3, 2026-11-02). Making the range of `rest` in `stdlib/prelude.ozen` `1 h<..` and rebuilding made it fail with "samples: 11 samples, 236 expectations, 4 failed". Restoring it and rebuilding made it pass
- Mutation check: S753, Side effects, `G-QC-SCOPE` (P4-3, 2026-11-02). Changing `samples/T1/araNi.ozen` in a work commit in a temporary worktree made it fail with "files outside scope: got '3'" (the other 2 outside the scope were `b215857`, which had no exclusion line at the time, and `scripts/test-names-exceptions.txt`, which was not in the allowed list at the time; this check found them and they were fixed). It was removed with `git reset --hard`, and the worktree was removed (not pushed). After the fix it passed
- Mutation check: S753, Fixed premise, `G-QC-SCOPE` (P4-3, 2026-11-02). Moving the tag `qc-start` locally to `qc-start-moved` made it fail with "tag qc-start: got ''". `qc-start` was put back on `0b4b4eb`
- Mutation check: S754, Side effects, `G-QC-VERSION` (P4-3, 2026-11-02). Making `VERSION` `0.1.1` made it fail with "VERSION: got '0.1.1'". Restoring it made it pass
- Mutation check: S782, Side effects, `G-RF-SEPARATION` (P4-3, 2026-11-02). Putting `tests/golden/args/issue1.out` and `core/src/sema.c` in the same commit in a work commit in a temporary worktree made it fail with "separation: 73 commits, 1 mixed". It was removed with `git reset --hard`, and the worktree was removed (not pushed)
- Other work's commit in range: 139d786 (another session added the plan file `plans/ozen-cooking-feedback.md`)
- Other work's commit in range: bb545b8 (another session recorded decisions in `plans/ozen-authoring-support.md`)
- Final gate (P4-3, 2026-11-02): at `e98a8a9`, we ran 133 goals: the same prerequisite goals as the P0-2 baseline (114 excluding `G-CORE-FUZZ`, plus `G-CORE-BUILD`), plus `G-QC-*` (17, excluding `G-QC-ISSUES`), `G-RF-GOLDEN-COUNT`, `G-RF-FUZZ-DICT`, `G-RF-SEPARATION`, `G-RF-CONCEPT`, `G-RD-SCOPE`, `G-RD-ADD-ONLY` and `G-REPO-NOREPLY`; 132 passed. `G-QC-SCOPE` failed because of commits from another session that came in during the run (`139d786`, `bb545b8`); after adding them to the exclusion lines as D81 says, it passed. `G-CORE-FUZZ` was run in a temporary worktree at `08059da`, the last commit that changed the core, and passed (600 seconds for each of the 6 targets, 0 crashes, leaks and timeouts, 0 artifacts; the core and standard library have not changed since). For wasm, we rebuilt with `scripts/build-wasm.sh` before running `G-PLAT-WASM`
- Other work's commit in range: 1ea30d3 (another session added the plan file `plans/ozen-dev-environment.md`)
- Closed the range (2026-11-02): following D111 of the later plan `plans/ozen-cooking-feedback.md`, put the tag `qc-end` on `e350725`, the last commit of this plan, and changed `QC=` in `scripts/goals.sh` to `qc-start..qc-end` and the end of the comparison in `G-QC-GOLDEN-CHANGED` to `qc-end`. This keeps commits of later plans out of the ranges of `G-QC-SCOPE` and `G-QC-GOLDEN-CHANGED`

---

## 10. Risks

- **R1. Existing goldens and samples use arguments without units, values of 0, or ranges with units on both ends**: on 2026-11-02 we searched with `git grep` for numbers without units in action calls and for ingredient amounts of 0 or less, and found none (`count: -1` in `tests/golden/kitchen/negative-count.ozen` is a kitchen setting, and an error is already expected). Range evaluation (D85) can change output that now shows "to taste" or `?`. If anything was missed, `G-CORE-GOLDEN` and `G-SAMPLE-CHECK` fail in P1-2, P2-3 and P3-2. Investigate each case: correct the expectation (S751), or decide it is a change that should not happen.
- **R2. Reading `<..` conflicts with syntax outside type ranges**: avoided by reading it only after `in` in types (D74). Detected by `bounds/expression-unchanged`, the existing goldens (`G-CORE-GOLDEN`) and `G-CORE-FUZZ`.
- **R3. The servings check changes amounts that use the servings in expressions (`150 g * servings`) or `per serving` calculations**: if the servings are in range, the values do not change. Confirmed by the existing goldens `quantity/per-serving*`, `quantity/count-options` and `menus/make-servings` not changing (`G-QC-GOLDEN-CHANGED` makes us name corrected expectations).
- **R4. Using the same `main` and the same working tree as other work**: the same as R3 of the README plan. Other work's commits in the range are named in §9 and excluded from `G-QC-SCOPE` (D81).
- **R5. `G-REPO-RED-FIRST` is slow**: it rebuilds in a worktree for each commit that adds expectations, so the commits that add goldens are grouped into 2 per phase, red and control (6 in total), to keep the count down.
- **R6. `G-QC-ISSUES` needs `gh` authentication and the network**: when they are not available, treat it as failed and report the reason (do not count it as a pass).
- **R7. Rebuilding the standard library from the vocabulary table removes the ranges**: `tools/std-from-vocab/convert.py` copies only the dimensions from the table, so if someone rebuilds the standard library from the table, this plan's ranges disappear. No goal calls it today. If the ranges disappear, `G-QC-PRELUDE` fails.
