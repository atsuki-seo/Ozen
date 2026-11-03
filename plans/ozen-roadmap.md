# Implementation plan for the Ozen roadmap §13-1 to §13-12 (vocabulary table, evaluator prototype, core, 4 platforms, CLI, renderer, bundle)

- Created: 2026-10-25 (rewritten after the D1 decision widened the scope from §13-1–§13-3 to §13-1–§13-12)
- Reference source: none (there is no code to port from). The design basis is `docs/concept.md` (1654 lines, as of 2026-10-25). "concept §n" in the text means a section of this file
- Implementation target: `/home/atsuki/ドキュメント/Ozen`. As of 2026-10-25 it is not a git repository, and it holds only `docs/concept.md` (initialized in D2)
- Rules this plan builds on: `~/.claude/rules/plan-language.md` (this file is in Japanese), `~/.claude/rules/repo-language.md` (committed artifacts default to English. D3), `~/.claude/rules/git-commit.md` (commit step by step in logical units, and push only after asking), `~/.claude/rules/github.md` (GitHub operations use `gh`). The project has no `CLAUDE.md`, `.claude/rules/` or existing `plans/`. There are no prerequisite goals from earlier plans either, so this plan also defines the build and test goals

## 0. How to read this plan

- The scope is items 1 to 12 of the roadmap in concept §13 (D1). Item 13, external integration, and the block editor and LSP are out of scope, because concept §12 decided to postpone them.
  - **A** action signature table (§13-1), **B** evaluator prototype (§13-2), **C** review of the prototype results (§13-3), **D** base of parser, arena and diagnostics (§13-4), **E** type check (§13-5), **F** IR, scheduler and text IR (§13-6), **G** public API, JSON and CLI (§13-7, concept §5.2), **H** builds and CI for the 4 platforms (§13-8), **I** minimal sample (§13-9), **J** standard library, reference menus and feature tests (§13-10), **K** renderer and settings screen (§13-11), **L** `ozen bundle` (§13-12).
- **The concept is the source of truth for behavior.** §4 splits the checkable claims of the concept into spec items `S1`…, one claim each, and names the concept section as (concept §n). Rules the concept does not decide and this plan fills in are marked **(added)**, and the user confirms in §8 whether they are sound.
- Every task's completion condition is written **so that a command decides pass or fail**. Human checks are not tasks; they go to the hand-over list in §8.
- Completion conditions refer to the goal IDs defined in §5 (`G-REPO-*`, `G-VOCAB-*`, `G-PROTO-*`, `G-CORE-*`, `G-TYPE-*`, `G-SCHED-*`, `G-API-*`, `G-CLI-*`, `G-PLAT-*`, `G-SAMPLE-*`, `G-STD-*`, `G-REND-*`, `G-BUNDLE-*`, `G-PLAN-*`). The code blocks in §5 are authoritative for the judgment commands. They are not written in tables (in a Markdown table `|` becomes `\|`, and copying it breaks regular expressions and pipes).
- Every design decision D1–D33 in §2 was decided by asking the user one at a time (D1–D31 and D33 on 2026-10-25, D32 on 2026-10-26. D33 was added from the user's question inside the D9 question, and D31 and D32 from the second audit). Each [decided] design decision in §2 starts with a "Decision:" line. The line has the form `- **Decision:** <結論>。（YYYY-MM-DD、ユーザー確認）`. The comparison of the recommendation and the alternatives is kept as history (P0-1).
- Tools and external sources are based on facts checked with local commands and public pages as of 2026-10-25 (§3).
- The coverage table in §5.3 shows, for each S item, a test or a hand-over to §8 (with a reason).
- For later phases (D onward), the first task of each phase (`P<n>-0`) adds the format details of that phase to §4 (output formats, field names and so on. All (added)), adds rows to the coverage table, confirms that `G-PLAN-COVERAGE` passes, and then reports to the user. Anything where judgment can differ is added to §2 there as a new D, and work continues only after confirmation.
- All work happens on `main`, with no work branches (D2). Diff ranges are counted from the tag `plan-start` set in P0-7.

---

## 1. Purpose and scope

**Purpose**: Build the implementation of the concept's language Ozen, from the vocabulary table to `ozen bundle`. Run a single core written in C23 (a restricted feature set, D13) so that it gives the same results on Linux, WASM, iOS and Android, and together with the standard library (`std`), the CLI (`ozen`) and the JS renderer, pass the 5 reference menus (concept §12.1) and the 4 feature tests (concept §12.4) on every platform. Each stage is checked against the concept's claims split into S items, using expected values derived by a path separate from the implementation.

### In scope

- **Preparation**: initialize the repository (D2), licenses (D4), README skeleton, language rules in `CLAUDE.md` (D3), shared check scripts
- **A (§13-1)**: vocabulary table `spec/vocab/*.yaml` and JSON Schema, check script `tools/vocabcheck/`, type check of the minimal sample on the table, values from the food composition table, research into primary sources from manufacturers, public bodies and papers, a mapping table to the 4 prior works that could be obtained (D5–D11)
- **B (§13-2)**: evaluator prototype `proto/` (a subset of the syntax, values and operations, `let`, `if`, `for`, `repeat`, hygienic macros, fuel, diagnostics and expansion history), gcc, clang, ASan, UBSan, libFuzzer, WASM, iOS simulator (D12, D14–D20)
- **C (§13-3)**: prototype report `docs/prototype-report.md` (measurements for the 3 items in concept §9)
- **D (§13-4)**: the base of the core `core/` (arena, string interning, spans and expansion history, diagnostic arrays and registry), a parser for the full syntax, resolution of modules, `import` and packages, embedding `std`
- **E (§13-5)**: type check (dimensions, states, linearity, resource conflicts, safety and allergens, parts and aliases, `variant`, kitchen and condition types, `check`, vessels, menus), the diagnostic severity mechanism, verification levels and assumptions
- **F (§13-6)**: IR, text IR dump, resource-constrained scheduler (waves, serving time, choice of `variant`, inserting washing and wiping, cleanup, keeping warm and refrigeration, plating, sharing, improvement, tie-breaking, `--prefer`) (D26, D27)
- **G (§13-7, concept §5.2)**: public API and headers, JSON output and schemas, SARIF for diagnostics, JSON for the list of action signatures and the type definitions, CLI `ozen` (`check`, `build`, `render`, `bundle`, `unbundle`, how to point at the entry point, short options, project-level builds and `out/`) (D21, D25, D30)
- **H (§13-8)**: builds and call checks for Linux (shared library and CLI), WASM (node and Web Worker), iOS and macOS (xcframework and Swift), Android (`.so` per ABI and JNI), CI (D18, D19, D29)
- **I (§13-9)**: make the minimal sample "curry and rice" give the same result on every platform
- **J (§13-10)**: standard library (`stdlib/`, converted from the table, D28), phrases (`phrases/ja.ftl`, `en.ftl`), pass reference menus A–E and feature tests T1–T4 on every platform
- **K (§13-11)**: renderer `renderers/` (recipe text, schedule table, HTML, Gantt chart, shopping list, Fluent phrases, name choice, drift check) and generating the settings screen from the type definitions (D22–D24)
- **L (§13-12)**: `ozen bundle`, `ozen unbundle`

### Out of scope (reasons)

- **External integration (concept §13-13)**: everything robot-related, sensor conditions, CBOR, FlatBuffers and so on. concept §12 decided to postpone them
- **LSP, syntax highlighting, block editor, deep OS integration (File Provider and so on)**: postponed in concept §12
- **The Ozendate app itself (iOS and Android screens, store distribution)**: not on the concept §13 roadmap. For the settings screen, the work goes as far as the form generated from the type definitions (D24)
- **Publishing releases (publishing artifacts to GitHub Releases)**: this counts as publishing to the outside. The scope goes as far as building the artifacts; the user decides on publishing
- **Non-goals in concept §12** (compiling C on the device, features that need a network, becoming a general-purpose language, drying, positions and movement inside the kitchen, plans over many days, nested courses, the look of plating, integration with Cooklang)
- **Windows**: not among the supported platforms in concept §8
- **PDF output**: not among the renderer items in the "in scope" list of concept §12. Printing is handled by print CSS for HTML (S449)
- **Web app (including offline support with a Service Worker)**: this is the Ozendate app itself (the note on Web distribution in concept §8). The renderer's HTML is self-contained (S381)
- **Values measured during cooking (sensors and so on)**: postponed as external integration in concept §12
- **Values from numerical heat-conduction calculation (the second source in concept §12.3)**: D7. Values with no primary source found are entered as provisional ranges, with a record of the search
- **Mapping table to the prior works that could not be obtained** (Matsushima et al. 2011, Kayahara et al. 2017): D8
- **In the prototype: checking state types and linear types and scheduling, declarations of units, ingredients, actions and enum types, field access on kitchen values, computation inside macro bodies, type checking of `variant` steps (at the table stage)**: not needed for the purposes of B and A (checking the evaluator's approach, and types seen from outside the table). The production core (D–F) handles them
- **Correctness of the cooking knowledge, how it feels to write, intent of the added rules**: the user checks these (§8)

---

## 2. Design decisions

### D1. Scope of the plan [decided]

- **Decision:** Make all of §13-1 to §13-12 of the concept §13 roadmap the scope of this plan. The implementation language of the core is decided at this stage of the plan, without waiting for the prototype results (D13). External integration in §13-13 and the block editor stay out of scope, because concept §12 decided to postpone them. §1 and §4–§10 are rewritten to match this decision. (2026-10-25, confirmed by the user. The answer "All of it. So the premise is that the language is decided at this point." was interpreted as above)
- The option that was recommended: up to §13-1–§13-3. Alternatives: only §13-1 / only §13-2 and §13-3 / up to §13-9.

### D2. Repository initialization and work branches [decided]

- **Decision:** Initialize with `git init -b main` and work only on `main`, with no work branches. The first commit holds only `docs/concept.md`, this plan and `.gitignore`. Diff ranges (the check that expected values are written first, the check that registries are append-only and so on) are counted from the tag `plan-start` set in P0-7. A remote is created only if D19 uses GitHub, and only after confirmation. (2026-10-25, confirmed by the user)
- **Recommendation**: `git init -b main`. The first commit holds only `docs/concept.md`, this plan and `.gitignore`. Shared files (licenses, NOTICE, README skeleton, the shared scripts in `scripts/`, the record of tool versions) also go on `main`.
  - For each phase, cut a work branch from `main`, set a tag `base-<ブランチ>` at the branch point, and merge into `main` with `--no-ff` once that phase's gate passes. Branches: `vocab-table` (A) and `eval-prototype` (B, C) in parallel. After that, in order: `core-base` (D), `core-types` (E), `core-schedule` (F), `core-api` (G), `platforms` (H), `samples` (I, J), `renderer` (K), `bundle` (L).
  - Diff ranges are counted from the tags (the range does not become empty after merging into `main`). Each branch touches only its own phase's folder, its own heading in the README and its own subsection in §9 of this plan (the option included a goal to check the ranges).
  - A remote is created only if D19 uses GitHub, and only after the user confirms.
- Alternative: work only on `main`. Simple, but in-between states before the goals pass get mixed into `main`.
- Alternative: work on one work branch to the end. The unit of merging becomes large, and in-between states are hard to roll back.

### D3. Language of documents and comments [decided]

- **Decision:** Design documents and the report under `docs/` (`docs/prototype-report.md`) are in Japanese; everything else (code, comments, commit messages, README, YAML comments) is in English. `docs/concept.md` stays in Japanese. This rule is written into `CLAUDE.md` in P0-3 (S409). The attribution wording for the food composition table is in Japanese, values in `phrases/ja.ftl` are in Japanese, and values in `en.ftl` are in English. (2026-10-25, confirmed by the user)
- **Recommendation**: As the global rule (`~/.claude/rules/repo-language.md`) says, code, comments, commit messages, README, YAML comments and the report are in English. `docs/concept.md` stays in Japanese (not translated). The attribution wording for the food composition table is written in Japanese, as MEXT specifies. Values in the phrases file (`phrases/ja.ftl`) are in Japanese, and values in `en.ftl` are in English.
- Alternative: design documents and the report under `docs/` are in Japanese (to match concept.md), everything else in English. As a project rule, the language rule is written into `CLAUDE.md` in P0-3.

### D4. License files [decided]

- **Decision:** As recommended, add `LICENSE-MIT`, `LICENSE-APACHE`, `LICENSE-CC-BY-4.0` and `NOTICE` in P0-3, with an SPDX line in each file. The MIT author line is `Copyright (c) 2026 atsuki.seo`. Ask for confirmation before fetching the texts. (2026-10-25, confirmed by the user)
- **Recommendation**: In P0-3, add `LICENSE-MIT`, `LICENSE-APACHE`, `LICENSE-CC-BY-4.0` and `NOTICE`, with an SPDX line in each file (S1–S6). The MIT author line is `Copyright (c) 2026 atsuki.seo` (git's `user.name`). The texts are fetched from the official distributors (this is a download, so before fetching, show the file name, source and size and ask for confirmation). Reason: the license split is decided in concept §3, and before code is written there is no need to add them to every file later.
- Alternative: add them before publishing.

### D5. Table format [decided]

- **Decision:** As recommended, YAML (loaded so that duplicate keys are rejected, shape checked with JSON Schema). The table is converted into `.ozen` for `std` in P7-4 of D (D28). (2026-10-25, confirmed by the user)
- **Recommendation**: YAML. Read it with PyYAML 6.0.1 (loading that rejects duplicate keys) and check its shape with JSON Schema (jsonschema 4.10.3, draft 2020-12). Comments are possible, and it is easy to read and write by hand. Quantities are written as strings in the same form as Ozen quantity literals, such as `"15 ml"` and `"4..6 min"`. YAML's implicit type conversion is caught by the schema (S12). The table is converted into `.ozen` for `std` in J (D28).
- Alternative: JSON. It can be read with the standard library alone, but comments are not possible.
- Alternative: TOML (`tomllib`). Deeply nested action signatures are hard to write.

### D6. Scope of samples passed on the table [decided]

- **Decision:** As recommended, in A only curry and rice is type checked on the table, and the other menus are checked for coverage by matching against the list. The 5 reference menus and T1–T4 are passed with the production compiler in J. (2026-10-25, confirmed by the user)
- **Recommendation**: In A (the table stage), as the concept §16 criteria say, only curry and rice is type checked on the table, and the other menus are checked for coverage by matching against the list. The 5 reference menus and T1–T4 are passed with the production compiler in J (S363–S374).
- Alternative: type check all 5 menus on the table in A. Gaps in the table are found earlier, but the menu syntax (`choose`, `assemble`, waves, `buy`) would also have to be written into the check script, duplicating the production compiler in J.

### D7. How to collect attribute values [decided]

- **Decision:** Take alternative A. In addition to the food composition table values (S58–S61), research primary sources from the official information of appliance and cookware manufacturers (instruction manuals, technical documents), documents from public bodies, and research papers, and enter the values (P3-5, P3-6). Values from primary sources have a provenance registry entry with the URL, document name, retrieval date, location and a transcription of the number, and a command checks that each value matches its transcription (S465). Values that could not be found are entered as provisional ranges, with a record of the search (S466). Source document files are not put in the repository (S467). Numerical heat-conduction calculation (the second item in concept §12.3) is not done in this plan. (2026-10-25, confirmed by the user)
- **Recommendation**: Extract as primary sources only the values that can be taken mechanically from the food composition table (refuse rate; weight change rate, if it is in the Excel file), and put them in `data/mext-fct-2023/` (S58–S61). Other values (density, typical weight, heating time, hardness, action durations) are entered as "provisional" ranges, with their provenance recorded (the third priority in concept §12.3). Schedules (F, J) are built with the provisional values, and the list of assumptions says "provisional values were used" (S277). Fetching the Excel file is a download, so ask for confirmation when starting P3-1.
- Alternative A: also research and enter primary sources such as appliance instruction manuals and papers in this plan. The quality of schedule times goes up, but the amount of research is hard to predict.
- Alternative B: enter no values and build only the shape. Schedules get no times.

### D8. Handling of prior work [decided]

- **Decision:** Following the user's instruction "Download them yourself, and look only at the ones you can get", the mapping table is made only for the documents that could be obtained on 2026-10-25. They are 4 works: Nakabe et al. (2021, Algorithms 14(2) 67, obtained from mdpi-res.com), Tasse and Smith (2008, MILK technical report CMU-LTI-08-005), Paulius et al. (FOON, arXiv:1902.01537) and Kumbhakern et al. (arXiv:2509.04159). Matsushima et al. (2011, cannot connect to the IAENG server) and Kayahara et al. (2017, IEICE members only), which could not be obtained, are out of scope. arXiv:2206.13900 and 1806.09912 are used as primary sources on heating for D7. Source document files are not put in the repository (S467). (2026-10-25, confirmed by the user)
- **Recommendation**: In this plan, do not make a mapping table to the vocabulary of the papers; treat the lists in concept §12.2 and §12.3 as the source of truth. The lists are already worked backward from the reference menus. The body of Nakabe et al. (2021, MDPI) could not be fetched with WebFetch (HTTP 403).
- Alternative: the user provides the papers as PDFs, and a mapping table between their classification of cooking steps and the actions in this table is made in `spec/notes/prior-work.md`.

### D9. Shape of the cutting action [decided]

- **Decision:** As recommended, cutting with a knife has a single action `cut(x, style, size)`. Chopping finely by machine is a separate action `blend`, and how to handle it is decided in D33. (2026-10-25, confirmed by the user)
- Background: concept §12.2 makes "cut (the cutting style is an argument)" a single action. On the other hand, the examples in concept §4.2, §4.4, §6 and §7.1 are written with an action per cutting style, such as `slice(x, 3 mm)`.
- **Recommendation**: Have a single `cut(x, style, size)` (as in concept §12.2; concept §14.1 decided that the vocabulary follows §12). Samples are written as `cut(peeled, style: slice, size: 3 mm)`. The relation of how fine the cutting styles are (S35, S199) stays inside one action.
- Alternative A: an action per cutting style (`slice`, `dice`, …). It matches the concept §4 examples, but adds 6 actions and writes the same requirement 6 times.
- Alternative B: make `cut` the basis, and add `slice` and the others to std as composite actions that call `cut`. Both ways of writing work.

### D33. Handling of chopping finely by machine (blender, food processor) [decided]

- **Decision:** As recommended, chopping finely by machine is `blend` (hands and the capability `Blending`). Applied to a list, it fills in `combine` and gives a new value; applied to a single ingredient, it gives the new shape value `pureed` (finer than any cutting style). Mincing with a food processor is also written as `blend`. (2026-10-25, confirmed by the user)
- Background: in the answer to the D9 question, the user asked "I understand up to mincing, but what about work that needs a blender?" (2026-10-25). concept §12.2 puts "put in the blender (`blend`)" among the mixing actions, and menu E (mixed juice) uses it. The concept has no way to write the shape when only one ingredient is chopped finely by machine (onion paste and so on).
- **Recommendation**: Chopping finely by machine is the action `blend`, separate from cutting with a knife, `cut` (it uses hands and the capability `Blending`, which blenders and food processors have. The soiling is `Sticky`).
  - Applied to a list (2 or more ingredients), it fills in `combine` from D11 and becomes a new value (`Mixture`) (mixed juice).
  - Applied to one ingredient, it becomes the new value `pureed` on the shape axis (finer than any cutting style. The "becomes finer" relation is `mince` → `pureed`). (added)
  - Mincing with a food processor is not distinguished from the knife's `cut(style: mince)`; when a machine is used, it is written as `blend`.
- Alternative A: putting something in a machine always gives a new value, even for one ingredient (no shape axis is added). Simple to write, but the state check of the original ingredient cannot continue, as in "fry the onion paste".
- Alternative B: express mincing with a food processor as a `variant` of `cut(style: mince)` (knife and cutting board / food processor), and let the schedule choose the faster one if the kitchen has it. This rides on the concept's `variant` mechanism, but needs a mechanism to switch `variant` by argument (the cutting style).

### D34. How the kitchen and build-time conditions are injected (brought forward in P8-5) [decided]

- **Decision:** As recommended, both the kitchen and the conditions are written as `kitchen` declarations in the project source, and are injected only when selected by name with `--kitchen 名前` or `--options 名前`. Conditions are written with a type annotation, as `kitchen <名前> : Options { … }`. (2026-10-26, confirmed by the user)

- Background: the golden tests of P8-5 (type check of the kitchen, narrowing of `variant`, `check`, vessel capacity) need the kitchen and build-time conditions to be injected, but the Ozen text form of the conditions was planned to be decided in P10-0, and the CLI's `--kitchen` and `--options` are work for P10-4. concept §4.6 says "a kitchen can be written too, but it is not injected automatically. It is used only when selected by name, as in `--kitchen myHome`", and the grammar's `kitchen` declaration can take a type annotation (`: Kitchen` can be omitted).
- **Recommendation (the form implemented in P8-5)**: both the kitchen and the conditions are written as `kitchen` declarations in the project source, and are injected only when a name is selected with `--kitchen 名前` or `--options 名前`. Conditions are written with a type annotation, as `kitchen today : Options { cooks: 2, cleanup: afterMeal }` (the grammar is not changed). The core's input (`ozni_settings`) holds the names. In P10, when the `kitchen` and `options` texts of D25's `recipe_build` are received, those texts are read as declarations of the same form. For `--options FILE` in S355, P10-4 reads the file and passes it as a declaration of the same form.
- Alternative A: add a dedicated declaration for conditions, `options { … }`, to the grammar. Easy to read, but it changes the grammar rules (`G-CORE-GRAMMAR`) and the parser.
- Alternative B: pass conditions only as files (`--options FILE`) and do not select them by name. This matches S355, but each golden test of P8-5 needs its own file (they become folder golden tests).

### D10. "Or" in state requirements [decided]

- **Decision:** As recommended, allow a requirement to be written as a list of alternatives (`anyOf` in the table; in the language, separated by `|` inside the type's `< >`. Example: `T<shape: peeled | SkinEdible, shape: whole>`). (2026-10-25, confirmed by the user)
- Background: "An onion cannot be cut unless it is peeled; a carrot can be cut with its skin on" (concept §4.1). To pass both through `cut` while keeping it a single action (there is no overloading, concept §4.3), the requirement needs an "or".
- **Recommendation**: Allow a requirement to be written as a list of alternatives (`anyOf` in the table; in the language, alternatives separated by `|` inside the type's `< >`). Each alternative is a pair of axis conditions and trait conditions. `cut` is "`shape: peeled`, or `SkinEdible` with `shape: whole`".
- Alternative A: traits reinterpret axis values (for `SkinEdible` ingredients, `whole` is treated as `peeled`).
- Alternative B: split the action (`cut` and `cutWithSkin`).

### D11. Handling of lists (mixing into a new value) [decided]

- **Decision:** As recommended, have a structure-only action `combine`, and make it sugar: writing a list for an argument that takes one ingredient fills in `combine([...])`. (2026-10-25, confirmed by the user)
- Background: the concept §4.4 sample writes `simmer([fried, water, roux], 20 min)`, but the type of `simmer` in concept §4.1 takes one ingredient, and there is no overloading (concept §4.3).
- **Recommendation**: Put a structure-only action `combine` in std (it uses no hands, equipment or time), and treat writing a list `[a, b, c]` for an argument that takes one ingredient as if `combine([a, b, c])` were filled in (sugar). `combine` consumes all the ingredients in the list and returns a new value (`Mixture`) holding the total quantity. It is a separate action from "mix" (`mix`, which uses hands and a bowl).
- Alternative A: add a list (`[Food]`) to the argument types of actions, and write for each action how it handles one item and a list.
- Alternative B: have no sugar and always make the author write `combine`.

### D12. Role of the prototype [decided]

- **Decision:** As recommended, the prototype goes in `proto/`, and the production core `core/` is rebuilt in D. Parts that can be carried over are listed in the report and moved together with their tests. (2026-10-25, confirmed by the user)
- Background: since D13 decided the language, the question became whether to carry the prototype's code into production.
- **Recommendation**: The prototype goes in `proto/`, and the production core `core/` is rebuilt in D (§13-4), assuming the full syntax, spans on every node and an arena. Parts that may be carried over from the prototype (128-bit integers, rationals, UTF-8 checks and so on) are listed in the report (C), and when carried over they are moved into `core/` together with the same tests (P7-0). The prototype tries the approach on a subset, and its role is to hand what was learned to the production design.
- Alternative: build the prototype directly in `core/` and grow it as is into the first stage of the production core. Less rebuilding work, but code made for the subset tends to stay in production.

### D13. Implementation language of the core [decided]

- **Decision:** Use C23 with a restricted set of features (as in the recommendation below. clang uses `-std=c23`, gcc 13 uses `-std=c2x`). C++ is dropped from the candidates. (2026-10-25, confirmed by the user. Confirmed after researching Xcode and the NDK)
- Background: concept §14.2 said the core's language would be "among the C-family languages, checked in the §9 prototype", but D1 decided to choose it in this plan without waiting for the prototype. The evaluator prototype (B) is done in the chosen language, to check the evaluator's implementation approach (hygiene, fuel, expansion history). C++ was dropped from the candidates on the user's instruction (2026-10-25).
- Facts found (the "C23 support" table in §3.2): the C compilers of the target platforms are Apple clang 21 in Xcode 27.1 (LLVM 21.1 line), Apple clang 17 in Xcode 26.0–26.3 (LLVM 19.1 line), clang in NDK r30 and r29 (LLVM 21.0), and Emscripten 6.0.13 (LLVM 23 line), all clang. The local clang (LLVM 19.1.5 line) passes all 15 features with `-std=c23`; the local gcc 13.3 does not know `-std=c23`, and with `-std=c2x` it passes 12 (`_BitInt`, `#embed` and `<stdckdint.h>` do not pass).
- The option that was recommended: use C23 with a restricted set of features. Allowed are only the 12 features that both the local clang's `-std=c23` and gcc 13's `-std=c2x` pass (`nullptr`, the keywords `bool`, `true` and `false`, `typeof` and `typeof_unqual`, `constexpr` for objects, attributes such as `[[nodiscard]]` and `[[maybe_unused]]`, single-argument `static_assert`, `enum` with a specified underlying type, type-inferring `auto`, the empty initializer `{}`, `unreachable()`, binary literals, digit separators) (the 2026-10-25 audit fixed `auto` missing from the list. The count of 12 was the same at the time of the decision). `#embed`, `_BitInt`, `<stdckdint.h>` (overflow checks use `__builtin_*_overflow`) and the standard library functions added in C23 (`<stdbit.h>`, `%b` in `printf` and so on) are not used. The list of allowed features is pinned in P4-1 as small probe compilations on each toolchain (Xcode is checked through the D19 path).
- The option that was the alternative: C17. It surely behaves the same on every toolchain, but `[[nodiscard]]`, `enum` with a specified underlying type, `constexpr` and `nullptr` are not available.

### D14. Input of the prototype [decided]

- **Decision:** As recommended, parse a subset of the Ozen syntax (§4.8) with a hand-written recursive descent parser. (2026-10-25, confirmed by the user)
- **Recommendation**: Parse a subset of the Ozen syntax (§4.8) with a hand-written recursive descent parser. Tests can be written readably in Ozen syntax, and fuzzing hits the parser too (concept §10 "focus on the expander and parser"). It also becomes a draft of the production parser in D.
- Alternative: write the AST directly as S-expressions. The parser gets smaller, but how it feels to write in Ozen syntax cannot be checked.

### D15. Representation of rationals [decided]

- **Decision:** As recommended, hold the numerator and denominator as int64, and compute intermediate values with a home-made 128-bit integer (`__int128` is not used). If the result does not fit, `MAC:overflow`. (2026-10-25, confirmed by the user)
- **Recommendation**: Hold the numerator and denominator as int64, and compute intermediate values of operations with a home-made 128-bit integer (the same in the prototype and the core). `__int128` is not used because it is not available on 32-bit ARM (Android's armeabi-v7a and so on). If the reduced result does not fit in int64, `MAC:overflow` (S77). The range is enough for home quantities, and the result is the same on every platform.
- Alternative A: arbitrary-precision (multi-precision) rationals. No overflow, but more implementation and allocation.
- Alternative B: fixed point (for example, units of 10⁻⁶). Fast, but ⅓ cannot be represented exactly.

### D16. Kitchen values in the prototype [decided]

- **Decision:** As recommended, in the prototype they are passed as recipe arguments, keeping the value types at 6 kinds. (2026-10-25, confirmed by the user)
- **Recommendation**: In the prototype (B), pass them as recipe arguments (`recipe fryMeat(panCapacity: 250 g)`). The value types stay at 6 kinds (§4.9). In the production core, kitchen injection (verification level 2) allows writing `pan.capacity` (S236).
- Alternative: add a record type to the prototype too, so that `pan.capacity` can be written. The value types become 7 kinds.

### D17. Build system [decided]

- **Decision:** As recommended, CMake. (2026-10-25, confirmed by the user)
- **Recommendation**: CMake (3.28.3 is installed). Emscripten (`emcmake`), iOS (`CMAKE_SYSTEM_NAME=iOS`) and the Android NDK (the bundled toolchain file) can all be handled with CMake.
- Alternative: GNU Make. No dependency, but the cross-build settings for iOS and Android would have to be written by hand.

### D18. WASM toolchain [decided]

- **Decision:** As recommended, Emscripten (install emsdk into `~/.local/share/emsdk` and pin the version. Ask for confirmation before installing). (2026-10-25, confirmed by the user)
- **Recommendation**: Emscripten. Install emsdk into `~/.local/share/emsdk` and pin the version. This follows the Web policy in concept §8 (Emscripten + a few dozen lines of JS). Run it with node v20 and headless Chrome. Installing is a download, so ask for confirmation in P0-4.
- Alternative A: wasi-sdk and `node:wasi`. Smaller, but it differs from the concept's policy, and the JS bridge for the Web would have to be built separately.
- Alternative B: the local clang's `--target=wasm32-unknown-unknown` (no download needed). There is no libc, so the core would have to be written without the C standard library.

### D19. CI, and the path to run on iOS and Android [decided]

- **Decision:** Take the recommended path (GitHub Actions) with a public repository (the user's answer: "A public repository is OK"). Use only the standard runners hosted by GitHub, not larger runners (in public repositories the standard runners are free; larger runners are billed even in public ones. On the Free plan, concurrency is 20 overall and 5 for macOS, and a job runs at most 6 hours. Checked in GitHub's documentation on 2026-10-25). Creating the repository and the first push count as publishing to the outside, so confirm again when starting P5-3, and for each later push, show the range and ask for confirmation. (2026-10-25, confirmed by the user)
- Background: concept §10 says "put cross-builds for the 4 platforms (+ WASM) into CI from the start" and "run ASan, UBSan and fuzzing in CI all the time". The local machine is Linux, with no Xcode and no Android SDK (§3.2). KVM is available.
- **Recommendation**: Create a private repository on GitHub, push to it, and run GitHub Actions.
  - Linux job: builds and tests for 4 configurations, fuzzing for 60 seconds per target, WASM (node and Chrome).
  - macOS job (arm64): static libraries for device and simulator, `.xcframework`, tests called from Swift, golden tests and digests on the simulator.
  - Android job (a Linux runner with KVM): NDK builds (4 ABIs), golden tests and digests on the emulator, JNI tests.
  - Locally too, run the JNI tests on a Linux JVM and the module map tests with Swift on Linux (catch bridge errors locally before the device platforms).
  - On the user's instruction, the repository was created on 2026-10-26, before P5-3, as `atsuki-seo/Ozen` (public), and `main` was pushed. Before each push, `G-REPO-NOREPLY` is run (S475). After that too, for pushes in tasks that read CI results (P5-3, P11-3–P11-5, P12-2, P13-5, P16-4), show the range to push each time and ask for confirmation (`~/.claude/rules/git-commit.md`).
- Alternative A: do not use CI; run locally. Android on a local emulator (downloading the SDK and system images); for iOS, the user runs one script on their Mac and pastes the resulting digest (a machine does the comparison). This does not meet "in CI all the time" in concept §10.
- Alternative B: do not check running on iOS and Android devices; check only the builds.

### D20. Test oracles [decided]

- **Decision:** As recommended, expected values are written by hand from the spec and committed before the implementation (`G-REPO-RED-FIRST`); rationals use `Fraction`; schedules use a checker separate from the implementation, `tools/schedcheck/`; agreement across platforms is checked with digests. No reference implementation is built. (2026-10-25, confirmed by the user)
- **Recommendation**: Derive expected values by a path separate from the implementation.
  1. Expected values of golden tests (`.out`) and of fixtures are written by hand from §4 and committed before the implementation (`G-REPO-RED-FIRST`, `G-*-INDEPENDENT`).
  2. Rational arithmetic is compared against Python's `fractions.Fraction`.
  3. Fuel, repeat counts and depth are hand-calculated from the counting rules in §4.12.
  4. Food composition table values are re-extracted from the Excel file, plus 3 entries read by hand.
  5. Lists and relations are copies from the concept (§4.7, §4.7.1).
  6. Schedules use a checker written in Python separately from the implementation, `tools/schedcheck/` (it checks dependency order, resource capacity, the allowed serving time window, hands during the meal, washing compatibility and the number of vessels), plus expected schedules built by hand for small examples.
  7. Agreement across platforms is checked with digests.
- Alternative: in addition to the above, write a reference compiler in Python (evaluator and type check) and diff against it with randomly generated programs. Strong, but it means maintaining a second implementation.

### D21. Language of the CLI driver [decided]

- **Decision:** As recommended, the CLI is written natively in C23 (the same restricted features as the core) and links the core directly. `ozen render` calls `node renderers/cli.mjs`. CLI diagnostics are the code, name, location and arguments, plus an English display made mechanically from the identifier. (2026-10-25, confirmed by the user)
- Background: the CLI (`ozen`) walks folders to collect entry points, calls the core in order and writes out the output (concept §5.2). `ozen render` calls the renderer (JS) (concept §7.1).
- **Recommendation**: Write a native CLI in C23 (the same restricted features as the core) and link the core directly (concept §8, Linux "direct link"). The CLI has file I/O, but the core does not. `ozen render` calls `node renderers/cli.mjs`. The text of diagnostics the CLI prints does not use the phrases files; it is the code, name, location and arguments, plus an English display made mechanically from the identifier (the last alternative in concept §7.1). The renderer makes the phrase text.
- Alternative: make the CLI in node and call the WASM core. The renderer and phrases can be shared, but the CLI needs node, and the Linux "direct link" path cannot be checked with the CLI.

### D22. Language of the renderer [decided]

- **Decision:** As recommended, JavaScript that needs no build step (ES2022 modules, types in JSDoc). For WebViews, the main code is bundled into one file and handed over. (2026-10-25, confirmed by the user)
- **Recommendation**: JavaScript that needs no build step (ES2022 modules, `.mjs`). Types are written in JSDoc comments. No dependencies, and it works fully offline (concept §8). Tests use node's built-in `node --test`. Some WebViews (such as the Chrome-based Android WebView) refuse to load ES modules from `file://`, so write a small script (no dependencies) that bundles the main code into one file, and hand only that one file to the WebView (details decided in P14-0).
- Alternative: TypeScript. Stronger type checking, but it adds installing `tsc` (a download from npm) and a build step.

### D23. Fluent implementation [decided]

- **Decision:** As recommended, pin a version of Mozilla's `@fluent/bundle` (Apache-2.0), put it in `renderers/vendor/`, and add the license text and its provenance. Ask for confirmation before fetching it. (2026-10-25, confirmed by the user)
- Background: phrases use Mozilla's Fluent (concept §7.1). They use selectors (`{ $stage -> … }`), variables and default variants.
- **Recommendation**: Pin a version of Mozilla's `@fluent/bundle` (Apache-2.0), put it in `renderers/vendor/`, and add the license text and its provenance. Fetching is a download, so ask for confirmation when starting P14-1. There is no need to write a spec-conforming interpreter ourselves.
- Alternative: implement only the parts the concept's examples use (messages, variables, selectors, default variants, nested selectors) ourselves. No dependency, but any mismatch with the Fluent spec is ours to carry.

### D24. Scope of generating the settings screen [decided]

- **Decision:** As recommended, from the core's JSON of type definitions (S335), the renderer generates a self-contained HTML form, and the work goes as far as the kitchen and condition texts made from the form values passing the core's type check. Native screens are out of scope. (2026-10-25, confirmed by the user)
- Background: concept §4.2 "Ozendate's input screens for 'my kitchen' and 'today's conditions' can be generated from the type definitions (screen generation is outside the core)".
- **Recommendation**: The core outputs a JSON of type definitions (`types.json`, S335. The fields of `Kitchen` and `Options`, and the kinds of equipment and vessels that can be placed in a kitchen, with the types, ranges and defaults of their attributes), and the renderer generates a self-contained HTML form from it. Form values can be written out as Ozen kitchen spec and condition texts, and passing those to the core passes the type check. Native iOS and Android screens are out of scope (§1).
- Alternative: stop at a description of the form (JSON), with no HTML.

### D25. Input shape of the public API [decided]

- **Decision:** As recommended, pass a list of sources `recipe_source {path, text, len}` (including `package.ozen` and dependencies). The kitchen and conditions are texts in Ozen syntax, and the choice of entry point and outputs goes in `cfg`. The return value is JSON text. (2026-10-25, confirmed by the user)
- Background: concept §8 has `recipe_build(src_utf8, kitchen_utf8, options_utf8 /* ... */)`. The core does not read files (concept §5.2), so for multi-file projects the caller has to pass them. Diagnostic spans (S154) must map back to the original files, and the package declaration (`package.ozen`) also has to be passed.
- **Recommendation**: Pass a list of sources. `recipe_build(const recipe_config *cfg, const recipe_source *sources, size_t n_sources, const char *kitchen, size_t kitchen_len, const char *options, size_t options_len)`. `recipe_source` is `{const char *path; const char *text; size_t len;}`, and `path` is the path from the project root (including `package.ozen`; dependency packages are `deps/<名前>/…`). The file number in a span is the index in the list, and the output JSON carries a table of paths. `std` is embedded in the core (S179). The choice of entry point and outputs (IR, schedule, list of definitions and so on) goes in `cfg`, the kitchen spec in `kitchen`, and build-time conditions (servings, cooks, serveAt, cleanup, prefer, per-person attributes, severity settings) in `options`, written in Ozen syntax (`--serve-at` is a condition, not code, so S251 does not apply). The CLI, Swift, Java and JS bridges only assemble this list. The return value is JSON text (§4.23).
- Alternative A: bundle everything into one text, as inline `module a.b @source("path") { … }` (the same form as `ozen bundle`). The arguments are closer to the concept's shape, but annotations for mapping spans back to the original files, and how to bundle package declarations, would have to be decided separately.
- Alternative B: make the input a single JSON (`{sources: [...], entry, kitchen, options}`). The bridges are simple, but embedding sources inside JSON strings needs escaping.

### D26. Scheduler priority, and the rule for waiting or washing [decided]

- **Decision:** As recommended, priority is the longest path to serving (counted with upper bounds), assignment is the choice that finishes earliest, and ties go by candidate order → the one without washing → resource declaration order. With `--serve-at`, the same is used with the direction of time reversed. Improvement repacks backward and forward at most 8 round trips. (2026-10-25, confirmed by the user)
- Background: concept §4.2 says "assign in order of priority", "improve by repacking forward and backward", "ties go by IR node number", "`variant` takes the candidate that finishes earliest, and on a tie the candidate order", and "if there is no clean compatible tool, choose between waiting and inserting washing", but does not decide what the priority is or how to choose between waiting and washing.
- **Recommendation**:
  - When packing forward (without `--serve-at`): priority is the length of the longest path from that task to serving (counted with upper bounds of durations; longer goes first). Ties go by smaller node number. Assignment is "the choice that makes the task finish earliest" (including `variant`, resources, and waiting or washing). Ties go by candidate order (as in concept §4.2), then the one without washing, then resource declaration order.
  - When packing backward (with `--serve-at`): use the above with the direction of time reversed. Priority is the length of the longest path from the start to that task. Assignment is "the choice that makes the task start latest". The tie order is the same as when packing forward.
  - Improvement repacks the finished schedule backward and forward (forward-backward improvement) until the total time stops shrinking, at most 8 round trips.
- Alternative A: make the priority the earliest "latest time it must start" (latest start time).
- Alternative B: build with several priority rules and pick the best one by the order of goals (S291) (deterministic).

### D27. Handling of range durations [decided]

- **Decision:** As recommended, assignment and backward calculation use upper bounds, and the output gives the start and the end by the upper and lower bound. Serving-timing constraints are checked with the wait when the dish is done at the lower bound. (2026-10-25, confirmed by the user)
- Background: concept §4.1 "Schedules are built with ranges and settled during cooking", "schedules are built on the safe side".
- **Recommendation**: Assignment uses the upper bound of durations (it does not lean toward undercooking). Each task in the output has a start time, an end time counted with the upper bound and an end time counted with the lower bound. Backward calculation from `--serve-at` also uses upper bounds. However, serving-timing constraints (`@serveWithin(t)`, the max of `@keepWarm`) are checked with the time waited until serving when the dish is done at the lower bound (the earlier it is done the longer it waits, so this is the safe side).
- Alternative: assign with the middle value and output the upper and lower bounds as a range. This is not on the safe side.

### D28. Source of truth for the standard library [decided]

- **Decision:** As recommended, convert the table into `stdlib/` only once in P7-4, and from then on `stdlib/` is the source of truth. The table and the type check on the table are retired, and the matching of lists and relations and the static checks needed for `std` continue against the core's list of all definitions. (2026-10-25, confirmed by the user)
- Background: the vocabulary table of A (YAML) and the `.ozen` of `std` hold the same vocabulary. Making both the source of truth leads to disagreement.
- **Recommendation**: In P7-4 of D (after the parser exists), run a script that converts the table into `.ozen` in `stdlib/` only once, and from then on `stdlib/` is the source of truth.
  - What is retired: `spec/vocab/`, `spec/fixtures/`, `spec/samples/` and the type check of `tools/vocabcheck/` (they stay in git history). The goals that checked them (`G-VOCAB-SCHEMA`, `G-VOCAB-CHECK`, `G-VOCAB-SAMPLE`, `G-VOCAB-TESTS`, `G-VOCAB-READONLY`, `G-VOCAB-EXPECT-INDEPENDENT`) are not run from P7-4 on, and the names of those tests are written into `scripts/retired-tests.txt` with reasons and removed from the targets of `G-PLAN-TESTS-EXIST`.
  - What is carried over: the matching of lists and relations (`spec/coverage/`, S29, S138, S139) and the static checks on the table that `std` also needs (naming conventions, provisional values are ranges, no food composition table values written, equipment with the capability exists. S459–S462) are moved into `tools/stdcheck/` and run against the list of all definitions the core outputs (`ozen debug definitions`, S463. The public JSON from P10-2 on). A part with the same name as a trait becomes a core error (S421).
- Alternative: keep the YAML as the source of truth and generate `.ozen` on every build. The table check scripts can stay, but the generated `.ozen` is hard for people to read and write.

### D29. Android bridge [decided]

- **Decision:** As recommended, write the JNI wrapper in C and call it from the Java class `ozen.Ozen` (no Kotlin). Also try it on a Linux JVM. The `javac --release` version is decided in P11-0. (2026-10-25, confirmed by the user)
- Background: concept §8 "A JNI wrapper (a few dozen lines of C) or JNA. Strings are passed as UTF-8 `ByteArray`".
- **Recommendation**: Write the JNI wrapper in C (`bindings/android/jni/`) and call it from the Java class `ozen.Ozen`. No Kotlin (there is no local `kotlinc`. Java 25 is there). The same wrapper is also built as an x86_64 `.so` and tried on a Linux JVM. The Java class is compiled with `javac --release <n>` to match the class file version Android's d8 accepts (n is decided in P11-0 and written in §9.8).
- Alternative: JNA. No C wrapper needed, but it depends on the JNA jar.

### D30. Versioning [decided]

- **Decision:** As recommended, the root `VERSION` is the source of truth, it becomes `0.1.0` in P7-4, and it is not raised within this plan. Versions in golden tests are written as `{{STD_VERSION}}`. The prototype and the table have no version. (2026-10-25, confirmed by the user)
- Background: concept §4.7 says "`std` has the same version as the compiler", "`"1.3"` means 1.3 or later with the same major version", and "a minor is a change that does not break existing recipes". How 0.x versions are handled is not decided. Matching `package.ozen` versions against `requires` (S174, S176) is needed in P7-4 of D.
- **Recommendation**: The root `VERSION` is the source of truth for versions, and the compiler, `std` (`stdlib/package.ozen`) and libraries take their version from it. It becomes `0.1.0` in P7-4 and is not raised within this plan (the prototype `proto/` and the table `spec/` have no version). For 0.x compatibility, the concept's rule "same major version" is applied as is (`"0.1"` means 0.1.0 or later and below 1.0.0). Where a version appears in golden expected values, it is written as `{{STD_VERSION}}`, and the test runner script replaces it with the value of `VERSION` (raising the version does not require rewriting expected values).
- Alternative A: raise the minor version at each phase gate. Progress shows in the version, but version-matching golden tests need more handling.
- Alternative B: keep `0.0.0` to the end of this plan, and make it `1.0.0` at the end.

### D31. Compile-time values that depend on the kitchen or cooks, and the IR invariant [decided]

- **Decision:** As recommended, narrow the invariant to "in programs that do not use compile-time values referring to the kitchen or cooks, changing cooks and the kitchen leaves the IR the same". When they are referred to, report the dependent values and their locations as info (S463). (2026-10-25, confirmed by the user)
- Background: concept §4.2 says "changing cooks or the kitchen does not change the IR (DAG)", while it also says "servings and cooks are both compile-time values, so they can be used in `if` / `repeat`", "the number of splits, as in `ceil(meat / pan.capacity)`, is decided by the household's equipment", and "they can be used for branches like `if kitchen.has(Oven)`". Using compile-time values that refer to the kitchen or cooks changes the IR, so both cannot hold at the same time (§3.4. Found in the 2026-10-25 audit).
- **Recommendation**: Narrow the invariant to "in programs that do not use compile-time values referring to the kitchen or cooks, changing cooks and the kitchen leaves the IR the same" (S281). For programs that refer to them, accept that the IR may change, and report as info which values depended on the kitchen or cooks (S464). `prop/ir_invariant` targets only samples that do not refer to them.
- Alternative A: forbid using the kitchen and cooks as compile-time values (`pan.capacity`, `kitchen.has` and `cooks` are type errors), and decide the number of splits and the like at the schedule stage. The invariant holds for every program, but the examples in concept §4.2 and §4.3 can no longer be written.
- Alternative B: split the IR into 2 stages, "the part that does not depend on the kitchen or cooks" and "the part decided by the kitchen or cooks", and impose the invariant only on the first stage.

### D32. Diagnostic codes for `check` [decided]

- **Decision:** As recommended, the code is written in the `check` declaration (`check SCH101 tooManyCooks: … else warn`). Codes in `std` are checked for duplicates in the same space as the C registry, and the author's packages may use only numbers 900–999. The output is `パッケージ名:コード` (omitted for `std`). (2026-10-26, confirmed by the user)
- Background: concept §6.1 says "every diagnostic has a code and a name" and "a code once used is never reused", and concept §4.2 also outputs `check` written in type definitions (`check tooManyCooks: cooks <= kitchen.floor.capacity else warn`) as diagnostics. How to assign codes for `check` in `std` or by authors, which are not in the C registry (`diag_codes.def`), is not decided.
- **Recommendation**: The code is written in the `check` declaration (`check SCH101 tooManyCooks: … else warn`). Codes of `check` in `std` are checked for duplicates in the same space as the C registry, and are also listed in `core/tests/diag-names.txt`. `check` in the author's packages may use only numbers `900`–`999`, and duplicates are checked within the package (added). In the output, they are told apart as `パッケージ名:コード` (omitted for `std`).
- Alternative: assign `check` codes automatically by a fixed rule from the package name and the name (the author does not write them). No effort to write, but renaming changes the code, which makes "never reused" hard to keep.

### D33. What to add to the result JSON (allergens and the breakdown of choices) [decided]

- **Decision:** As recommended, add `allergens` to IR nodes and `choices` to the per-dish entries of the IR, and do not change the text IR. (2026-10-26, confirmed by the user)
- Background: S367 requires menu D's "breakdown of choices shows in the schedule", S368 requires menu E's "the milk allergen shows", and S370 requires T2's per-person differences. Sample properties are matched against the result JSON (§4.27), but the current JSON has neither the allergens of IR values nor the breakdown of `choose perPerson` (both appear only in the `debug steps` text). The renderer's (phase K) shopping list and allergen display need the same information. Found in P13-0 on 2026-10-26.
- **Recommendation**: Add `allergens` to IR nodes (the names of the allergens the value has, in name order. Empty if none), and add `choices` to the per-dish entries of the IR (next to `nodes` and `waves`) (`[{"mode": "perPerson", "options": [{"node": <候補の出力の節。無ければ null>, "label": <make のレシピの名前か buy の品名>, "count": <人数>}]}]`. Menu choices have no IR node, so no node is written for the choice itself). Do not change the text IR or `debug ir` (to keep the IR invariant and the golden tests). Also add them to `schemas/result.schema.json`.
  - ((added), P13-3) For the same reason, a list of holds `holds` (`[{"start", "end", "item": <品のソースの文字列>, "node": <品の IR の節>, "resource": <個体>}]`. Refrigeration, freezing, keeping warm) was added to the schedule JSON. It is the same as the `hold` lines in the text schedule
- Alternative A: add choices as IR nodes (`kind: "choose"`, `label: "perPerson"`). The breakdown is visible in the DAG, but the text IR and the expected values of existing golden tests change.
- Alternative B: do not change the JSON; the sample checks read the `debug steps` text. The renderer would need another way to get the same information.

### D34. How to count the serving-timing wait, and how to report when it cannot be kept [decided]

- **Decision:** As recommended, count the serving-timing wait from the upper-bound end of the finishing task, and make `schedcheck` do the same. Dishes that still cannot keep it are reported with `SCH010 serveWithinMissed` (warning). (2026-10-26, confirmed by the user)
- Background: §4.22.1 counts the wait as "serving time − lower-bound end of the finishing task", and when it exceeds the upper limit of `serveWithin`, delays the finishing task and rebuilds ((6) of `schedcheck` is the same). Plating comes after the upper-bound end of the finishing task, so the wait is at least "the width of the task's time range + the plating time". The parfait of menu D (`@justBeforeServe` = 2 minutes. Assembly is `layer` at 1–3 minutes, plating is 18–60 seconds) waits at least 3 minutes, so it cannot be kept however it is built. Currently the rebuilding does not converge, a candidate that does not keep the serving timing is silently chosen, and `schedcheck` reports a violation. Found in P13-3 on 2026-10-26.
- **Recommendation**: Count the wait as "serving time − upper-bound end of the finishing task" (the finish planned for assignment, the same upper bound as D27), and make (6) of `schedcheck` the same. Dishes that still cannot keep it are reported with a new diagnostic `SCH010 serveWithinMissed` (warning; arguments are the dish served, the wait in seconds and the limit in seconds; location is the dish served).
- Alternative A: keep counting the wait with the lower bound, and only output `SCH010 serveWithinMissed` for dishes that cannot keep it. For tasks with a wide time range like the parfait, `@justBeforeServe` is always a warning.
- Alternative B: keep counting the wait with the lower bound, and leave the width of the assembling task's time out of the serving-timing calculation (treat only `assemble` specially).

### D35. Vocabulary T1 and T2 need (filleting action and egg) [decided]

- **Decision:** As recommended, add the round-fish filleting action `fillet` and the ingredient `Egg` to `std`, and keep S456 as is. (2026-10-26, confirmed by the user)
- Background: T1 (S369) requires "multiple outputs of a filleting action", and T2 (S370) requires "a person with an egg allergy and oyakodon". `std` has the parts of a round fish (`head`, `collar`, `back`, `belly`, `frame`, `skin`, `guts` and `scales` of `RoundFish`. concept §12.3 says they are for "filleting and using everything (T1)") and the allergen `egg`, but neither a filleting action nor an egg ingredient (the list in §4.7 follows concept §12.2, worked backward from the 5 reference menus, and neither is in it). Meanwhile, S456 forbids vocabulary declarations (`ingredient`, `action` and so on) in the samples in `samples/`. Found in P13-4 on 2026-10-26.
- **Recommendation**: Add to `std` the round-fish filleting action `fillet(x: T<RoundFish>) -> (T<head>, T<collar>, T<back>, T<belly>, T<frame>, T<skin>, T<guts>, T<scales>)` (hands, cutting board, knife; soiling is `rawFish`) and the ingredient `Egg` (`Allergen(egg)`, `Hazard`; soiling is `rawEgg`) (added). Values such as part yields and times are added with provenance (a primary source if there is one, otherwise provisional with a record of the search). Phrase keys are added too. S456 is kept as is.
- Alternative A: put only T1–T4 outside S456, and declare the filleting action and egg inside the test recipes. `std` does not change, but the check that "samples can be written with `std` vocabulary alone" then covers only the reference menus.
- Alternative B: for T1, prepare parts cut by hand with `buy` (`buy Buri<collar>` and so on), and for T2, test the same feature with another allergen in `std` (milk) instead of egg. No vocabulary is added, but it departs from the content of concept §12.4 (filleting, egg allergy).

### D36. Add the display information the renderer needs to the result JSON [decided]

- **Decision:** As recommended, add `type` (and `part`) and `group` to IR nodes, and `ingredients` (in written order) per dish, and do not change the text IR. (2026-10-26, confirmed by the user)
- Background: the renderer (phase K) makes text from the result JSON alone. Phrase keys (`ingredient-onion`) and names with parts (S388) need the value's type and part, the ingredient list (S450) needs the order written in `ingredients`, and the grouped display (S378) needs the range of `@group`, but IR nodes in the current JSON hold only a name (the author's name, such as `roux`) and a quantity, and none of these appear (`@group` and the written order are AST metadata, and the agreement is not to put them in the IR, concept §6). Found in P14-0 on 2026-10-26.
- **Recommendation**: Add to IR nodes `type` (the name of the value's type; also `part` if it has a part) and `group` (the key if it is inside the range of `@group(キー)`, otherwise null), and add to the per-dish entries of the IR `ingredients` (`[{"label", "node"}]` in the order written in `ingredients`). All of these are display-only information and do not change the text IR or the IR invariant (S281). Also add them to `schemas/result.schema.json`.
- Alternative: the renderer reads the Ozen source itself. This departs from the agreement that drawing works from JSON alone (S330, S334), and keeps two parsers.

---

### D37. Unit and fine shape of the bundled file's content [decided]

- **Decision:** As recommended, include only the definitions reachable from the entry point, as their original text, and read the round trip as "the restored declarations are a subset of the original declarations, each one the same as the original, and the built IR is the same as the original". Make everything, including the entry module, an inline `module`, record dependency versions by repeating `from`, and gather phrases into one `phrases` per language, separated per package with `###` comments. (2026-10-27, confirmed by the user)
- Background: S399 (unused definitions are not included) and S401 (the declarations of files restored by `unbundle` are the same as the original source) cannot both hold when an original module has unused definitions. The concept §4.7 example puts the entry `menu` at the top level of the file and has only one `from`, but with that the entry module's path and the dependency versions (checked by S407) cannot be restored.
- **Recommendation**: Drop at the unit of definitions, and match the round trip as a subset. The shape is as in the decision above.
- Alternative: include whole modules, and make the restored files match the originals exactly (reading S399 as "unused modules"). As in the example, put the entry at the top level and have only one `from`.

---

### D38. How diagnostic arguments are said in text [decided]

- **Decision:** As recommended, actions, soilings, candidates, reasons, equipment instances and dish names are turned into words by looking up phrase keys per argument kind, and are counted as fallbacks when they cannot be looked up. `render-all.mjs` also counts identifiers left in the body text and requires 0. (2026-10-27, confirmed by the user)
- Background: the P16-3 audit found that diagnostic arguments appeared as raw identifiers in note sentences (`diag-*`) (155 places in the Japanese HTML, such as "cookRice is done in pot (cooker is earlier)"). S451 counted only falls to fallbacks and did not detect this.
- **Recommendation**: As in the decision above.
- Alternative: output them as raw identifiers, read S451 as counting only phrase fallbacks, and send the naturalness of note sentences to §8.

### D39. How to pass the stage of a fish that changes its name as it grows [decided]

- **Decision:** As recommended, the stage is passed through the drawing settings (`render(…, {stage})`, `ozen render --stage`) the same way as the dialect, and a real-path test is added. Choosing the name from the value's attributes or the dish's context is sent to §8. (2026-10-27, confirmed by the user)
- Background: the P16-3 audit found that `std` has no stage attribute, so the stage cannot be obtained from the IR, and "choose by attribute" in S387 did not work on the real path (always the adult fish), and that there were no phrases using `$context`.
- **Recommendation**: As in the decision above.
- Alternative: add a stage axis to the buri in `std`, and look it up from the attributes of IR values (a change to the core and the vocabulary).

---

## 3. Findings on the current state (2026-10-25)

### 3.1 Base of the implementation target

- `/home/atsuki/ドキュメント/Ozen` holds only `docs/concept.md`. It is not a git repository (`git status` says `not a git repository`). There is no `CLAUDE.md`, `.claude/` or `plans/`.
- There is no existing mechanism to build on. All build, test and check mechanisms are made in this plan.

### 3.2 Tools used (checked with local commands)

| Tool | Location | Facts checked |
| --- | --- | --- |
| gcc 13.3.0 | `/usr/bin/gcc` | Builds and runs with `-std=c17`, ASan and UBSan. With `-mgeneral-regs-only`, code that uses floating point becomes an error (`SSE register return with SSE disabled` for a function returning `double`) |
| clang 17.0.0 | `~/.local/share/swiftly/bin/clang` (bundled with the Swift 6.2.3 toolchain) | Links and runs with `-fsanitize=fuzzer,address,undefined` (`DONE` with `-runs=10`). At link time ld.gold warns (`Cannot export local symbol '__asan_...'`), but this does not affect running. It can output wasm with `--target=wasm32-unknown-unknown -nostdlib`, and can also compile `__int128`. `--target=wasm32-wasi` fails because there is no sysroot. `-mgeneral-regs-only` does not detect floating point |
| wasm-ld (LLD 17) | Same as above | Present |
| cmake 3.28.3, GNU Make 4.3 | | Present. ninja and meson are missing |
| node v20.20.2 | | Present |
| Python 3.12.3 | | PyYAML 6.0.1, jsonschema 4.10.3 (with `Draft202012Validator`), openpyxl 3.1.5 and `tomllib` are present. pytest and hypothesis are missing (use `unittest`) |
| jq 1.7 | | Present |
| google-chrome 154.0.8037.97, firefox 157.0 | `/usr/bin` | Present. Headless Chrome is used for the Web Worker check (S150) |
| objdump (GNU Binutils 2.42), `timeout` (coreutils 9.4) | | Present. awk is mawk 1.3.4, so gawk extensions (`strtonum` and so on) are not available |
| gh 2.45.0 | | Logged in to github.com as atsuki-seo (git operations use ssh) |
| docker 29.8.2 | | The CLI is present, but cannot connect to the daemon (Docker Desktop is not running) |
| Missing | | emcc (Emscripten), wasmtime, wasi-sdk, Android NDK, Xcode (because this is Linux), valgrind, clang-tidy, clang-format, shellcheck, qemu |
| Java 25.0.4.1 (`java`, `javac`) | | Present. The JNI wrapper can be tried on a Linux JVM (D29). `kotlinc`, `gradle` and the Android SDK (`adb`, `sdkmanager`, `emulator`) are missing |
| Swift 6.2.3 (Linux) | `~/.local/share/swiftly/bin/swift` | Present. A test that imports a C header through a module map and calls it can be tried on Linux (S355) |
| KVM | `/dev/kvm` | The ACL gives atsuki read and write permission. The Android emulator can run locally (downloading the SDK and system images is needed) |
| Resources | | 16 cores, 30 GB memory, 486 GB free in home |

#### C23 support (researched on 2026-10-25. D13)

| Platform | Current version | C compiler | Facts checked |
| --- | --- | --- | --- |
| Xcode 27.1 (2026-10-15) | Apple clang 21.0.0 (2100.3.34.2) | The Swift 6.4 branch of `swiftlang/llvm-project` is LLVM 21.1 | data.json on xcodereleases.com, and `LLVMVersion.cmake` in `swift/release/6.4.0` of `swiftlang/llvm-project`. Xcode 26.4 and later are also Apple clang 21. `-std=c23` on real hardware is unconfirmed (checked through the D19 path) |
| Xcode 26.0–26.3 | Apple clang 17.0.0 | The Swift 6.2 branch is LLVM 19.1.5 (the same commit line as the local swiftly clang) | With the local clang, all 15 features below passed with `-std=c23 -pedantic-errors -Werror` |
| Android NDK r30 (2026-09-08) | clang-r574158c | LLVM 21.0.0 | `gh release view r30 -R android/ndk`, and `LLVMVersion.cmake` in `mirror-goog-llvm-r574158-release` of AOSP's `toolchain/llvm-project`. clang-r563880c in NDK r29 is also LLVM 21.0.0 |
| Emscripten 6.0.13 (2026-10-19) | — | LLVM 23 line | `ChangeLog.md` of `emscripten-core/emscripten` (the in-development 6.0.14 updates compiler-rt to LLVM 23.1.3) |
| Upstream clang | — | — | The C support table on clang.llvm.org: `-std=c23` from clang 18. `constexpr` from 19, `enum` with a specified underlying type from 20, `#embed` from 19, `<stdckdint.h>` from 18, `auto` from 18, `nullptr` from 17 |
| Local gcc 13.3 | — | — | Does not accept `-std=c23`. With `-std=c2x`, 12 of the 15 features pass, and `_BitInt`, `#embed` and `<stdckdint.h>` do not pass |

The 15 features tried: `nullptr`, the keyword `bool`, `typeof` and `typeof_unqual`, `constexpr` for objects, `[[nodiscard]]` and `[[maybe_unused]]`, single-argument `static_assert`, `enum` with a specified underlying type, `#embed`, `auto`, digit separators, binary literals, `<stdckdint.h>`, `_BitInt(128)`, the empty initializer `{}`, `unreachable()`.

### 3.3 External sources (checked on public pages)

- **Standard Tables of Food Composition in Japan (Eighth Revised Edition), Supplement 2023**: https://www.mext.go.jp/a_menu/syokuhinseibun/mext_00001.html lists the chapter 2 data (Excel) `/content/20260327-mxt_kagsei-mext-000029402_02.xlsx` and the errata dated 2026-03-27 (Excel) `/content/20260327-mxt_kagsei-mext-000029402_16.xlsx`. This page itself says nothing about terms of use.
- The terms of use come from the guidance at https://www.mext.go.jp/a_menu/syokuhinseibun/index.htm: the tables are public and free to use, and secondary use in papers, books, applications and so on must state "日本食品標準成分表（八訂）増補2023年から引用（又は出典）" ("quoted from (or source:) Standard Tables of Food Composition in Japan (Eighth Revised Edition), Supplement 2023") (the same as stated in concept §3).
- Which file and column hold the refuse rate and the weight change rate from cooking is unconfirmed (to be checked in §9.2 when starting P3-1).
- The body of Nakabe et al. (2021, Algorithms 14(2) 67, MDPI) returned HTTP 403 with WebFetch and with the PDF on `www.mdpi.com`, but could be obtained from the distribution URL on `mdpi-res.com` (18 pages). A cooking task has {ingredients, quantity, content, required resources, held resources, cooking time, preparation time, cooking type, holding type, start condition, holding condition, preceding task}; resources are people and tools; washing happens "when the type attached to a resource differs from the type of the next task"; and the objective is to minimize the total time including washing and cleanup (solved as an RCPSP). Elements Ozen does not have: held resources (after a task, the food occupies a vessel), matching the end of the preceding task with the start of the next, and preparation time that needs no person (handled in the D8 mapping table).
- Cost of GitHub Actions (2026-10-25, docs.github.com): in public repositories, the standard runners hosted by GitHub are free. Larger runners are billed even in public ones. On the Free plan, concurrency is 20 overall and 5 for macOS, and a job runs at most 6 hours (D19).
- Documents obtained (2026-10-25, kept in the scratchpad and not put in the repository): Nakabe et al. 2021, Tasse and Smith 2008 (MILK, 9 pages), FOON (arXiv:1902.01537, 8 pages), arXiv:2509.04159 (6 pages), arXiv:2206.13900 (20 pages), arXiv:1806.09912 (13 pages). Not obtained: Matsushima et al. 2011 (cannot connect to the IAENG server), Kayahara et al. 2017 (IEICE members only)

### 3.4 Inconsistencies in the concept and how this plan handles them

| Statement in the concept | Inconsistency | This plan |
| --- | --- | --- |
| The §4.4 sample `cook(soaked, cooker, 40 min)` | The recipe names a kitchen tool. This conflicts with §4.2 "recipes are not aware of the kitchen" and rule 4 of §4.6 "referring to the kitchen by name from a dish or menu is an error" | Samples are written with `cookRice(x)` from §4.2 (an action that has `variant`) |
| §4.4 `simmer([fried, water, roux], 20 min)` and §4.1 `simmer(x: T<shape: cut>, ...)` | The argument types do not match | D11 |
| §4.1 "a carrot can be cut with its skin on" | How to write it with one action is not decided | D10 |
| §12.2 "cut (the cutting style is an argument)" and `slice(x, 3 mm)` in §4 | The shape of the action differs | D9 |
| §4.4 `onion: 2 pcs` | `pcs` is not in the list of units in §12.2 | Use `pcs` as the unit name for the count dimension (display counters are in the phrases file, §4.1) |
| §4.2 `requires Oven<maxTemp >= 250 °C>` | `°C` is not ASCII. This conflicts with §4.4 "identifiers are ASCII only" | The unit name is `degC` |
| The tool "bowl" in §12.3 and the vessel class "bowl (`Bowl`)" | The English names collide | The tool is `MixingBowl`, the vessel is `Bowl` |
| `uses hands, Counter, CuttingSurface` of `slice` in §4.2 | `Counter` is an equipment kind, and the standard capabilities in §12.3 have no work-surface capability | Add the capability `WorkSurface` (added), which `Counter` has |
| The value types in §4.3, "about 5 kinds: quantity, string, list, ingredient reference, syntax tree" | There is no type for `if` conditions | The prototype adds booleans, making 6 kinds (§4.9) |
| §4.2 "changing cooks or the kitchen does not change the IR", versus `ceil(meat / pan.capacity)`, `if kitchen.has(Oven)` and "cooks can be used in `if` / `repeat`" in §4.2 and §4.3 | Using compile-time values that refer to the kitchen or cooks changes the IR | D31 |
| The phrase key `ingredient-buri` in §7.1 (an example of how keys are formed) and `ing-buri`, `ing-buri-kama` (examples of names) | There are 2 prefixes for ingredients | Follow the example of the key-forming rule (a prefix per kind + kebab-case) and use `ingredient-` |

When a new inconsistency is found during implementation, do not decide it silently; record it in §9 and report it to the user (R2).

---

## 4. Behavior spec

Each checkable claim is one item with one claim, and starts with `**S<n>**`. Items that add a rule not in the concept are marked (added).

### 4.1 Repository and licenses (D2, D4)

- **S1** Code files in `proto/`, `tools/`, `scripts/` and `.github/` (extensions `.c` `.h` `.def` `.py` `.sh` `.mjs` `.cmake` `.yml`, and `CMakeLists.txt`) have `SPDX-License-Identifier: MIT OR Apache-2.0` within their first 5 lines.
- **S2** Files in `spec/vocab/`, `spec/schema/`, `spec/coverage/` and `spec/fixtures/` have `SPDX-License-Identifier: MIT OR Apache-2.0` (because they are the source of the standard library, concept §3). In JSON it is written in `"$comment"`.
- **S3** The sample recipes in `spec/samples/` have `SPDX-License-Identifier: CC-BY-4.0`.
- **S4** The root has `LICENSE-MIT`, `LICENSE-APACHE` and `LICENSE-CC-BY-4.0`, each with its official text (containing the lines `Permission is hereby granted, free of charge`, `Version 2.0, January 2004` and `Attribution 4.0 International`). The MIT text keeps none of the template's `<year>` and `<copyright holders>`, and has the line `Copyright (c) 2026 atsuki.seo`.
- **S5** `NOTICE` and `data/mext-fct-2023/README.md` contain the wording that the data is quoted from the Standard Tables of Food Composition in Japan (Eighth Revised Edition), Supplement 2023. The README writes the source URL, the retrieval date, the original file name and the sha256, each on a line starting with `- Source URL:`, `- Retrieved:`, `- File:` and `- SHA-256:`.
- **S6** `README.md` has a table of path groups and license classes (code and the source of the tables: `MIT OR Apache-2.0`; `docs/`, `plans/`, `spec/samples/`: `CC-BY-4.0`; `data/mext-fct-2023/`: the terms in its README), and this sentence: `Recipes you write in Ozen and the schedules and IR the compiler produces are yours; the Ozen licenses do not apply to them.`
- **S409** `CLAUDE.md` states the language rule of D3 (`docs/` and the report in Japanese, everything else in English).
- **S475** The author and committer emails of published commits are only the GitHub noreply address, and the user's personal email address does not appear in tracked files (the user's policy "do not publish the email", 2026-10-26). This check runs before every push.

### 4.2 Format of the tables (D5)

The files go in `spec/vocab/`: `dimensions.yaml`, `units.yaml`, `axes.yaml` (axes, the enumeration of cut styles and their fineness relation, the enumeration of heat levels), `traits.yaml` (ingredient traits and dirt classes in one namespace), `ingredients.yaml` (including parts), `aliases.yaml`, `actions.yaml`, `capabilities.yaml` (capabilities and kinds of equipment), `dirt.yaml` (kinds of dirt, washing methods, compatibility rules), `vessels.yaml` (vessels and their substitution order), `sources.yaml` (the registry of sources).

An attribute value is written `{value: "4..6 min", source: provisional}`, and a value from the food composition table is written `{source: mext-fct-2023, food: "<食品番号>", field: refuse}`.

- **S7** Every YAML file in `spec/vocab/` matches `spec/schema/vocab.schema.json`. A file that does not match is error `FMT:schema`.
- **S137** The files in `spec/vocab/` match exactly the 11 names above.
- **S8** A quantity string is accepted only in two forms: "number unit" and "number..number unit". A number is an integer, a decimal, or `p/q` (denominator not 0). A unit is a unit name or `単位/単位` (such as `g/ml` for density). Any other form (`200g`, `1e3 g`, `1/0 g`, `g/ml/s`, `5.. g`) is an error, and does not crash with an exception.
- **S9** A quantity string with no unit is an error.
- **S10** A quantity whose range lower bound is greater than its upper bound is an error. One whose lower and upper bounds are equal is treated as a single value.
- **S11** A quantity with a unit not in the tables is an error.
- **S12** A value that stopped being a string by YAML's implicit type conversion (`no` to `false`, `1:30` to a number, and so on) is `FMT:schema` at a position that requires a string.

### 4.3 Static checks of the tables

The namespaces are: types (ingredients, traits, kinds of equipment, vessels, capabilities, aliases), actions, axes, values per axis, units, kinds of dirt, parts per ingredient, and sources.

- **S13** Every reference to a name (trait, axis, axis value, capability, kind of equipment, kind of dirt, vessel, unit, dimension, part, source, food number in the food composition table) has a definition. Each reference without one is one error `NAM:undefined` (with file name and YAML path); the same undefined name in two places gives two errors.
- **S14** Two definitions of the same name in the same namespace (even across files, or as duplicate keys in the same mapping of the same file) are error `NAM:duplicate`. YAML is read with a loader that rejects duplicate keys (inheriting `SafeLoader` and detecting them in `construct_mapping`). The same name in different namespaces (the washing method `wash` and the action `wash`) is not an error.
- **S15** A trait with the same name as a part is error `NAM:partTraitClash` (concept §4.4).
- **S16** A cycle in unit definitions is error `TYP:unitCycle` (concept §4.3).
- **S17** A unit definition whose two sides have different dimensions is error `TYP:unitDimension` (defining `tbsp` as `15 g`, for example).
- **S18** Each dimension has exactly one base unit. None, or two or more, is error `TYP:baseUnit`.
- **S19** A duplicate among the values of an ordered axis is error `TYP:axisDuplicate`.
- **S20** A cycle in the "finer than" relation of cut styles is error `TYP:cutCycle`.
- **S21** Names of types, ingredients, traits, kinds of equipment, vessels, capabilities and aliases are camel case starting with an upper case letter; names of actions, axes, axis values, parts, kinds of dirt, arguments and compatibility rules are camel case starting with a lower case letter; both are ASCII only (concept §4.4). A name that breaks this is error `NAM:naming`. Unit names are exempt from the case convention (because of `L`) but must be ASCII only (`°C` is an error).
- **S22** Every numeric attribute has a source (`source`), and that source is in `sources.yaml`. If not, error `NAM:provenance`.
- **S23** A value whose source is `provisional` is held as a range (lower bound < upper bound). A single value is error `TYP:provisionalPoint`. (added)
- **S24** An attribute whose source is `mext-fct-2023` holds no value, only a food number (values live only in `data/mext-fct-2023/`, concept §3). Holding a value is error `TYP:fctInlineValue`.
- **S25** For every capability, the tables have at least one kind of equipment that provides it. If not, error `NAM:capabilityUnprovided`.
- **S26** The compatibility rules after `Hazard` and `Allergen` are `fixed: true` (settings cannot relax them, concept §12.3). If not, error `SAF:ruleNotFixed`.
- **S27** Axis values that appear in an action's requirements (including each option of `anyOf`) and effects are within that axis's values. If not, error `TYP:axisValue`.
- **S28** The check script does not rewrite the tables, samples or fixtures, and creates no files in the repository (it does not write Python bytecode either: `sys.dont_write_bytecode = True` is set before importing its own modules).

### 4.4 Matching against the lists (D6)

- **S29** Every item of the lists in §4.7 (a copy of concept §12.2 and §12.3) is defined in the tables as a (kind, owner, identifier) tuple (pork `belly` and beef `belly` are separate items). The tables having more definitions than the lists is not an error (the standard library may be larger than the lists).
- **S138** Items for which the lists in §4.7 give values (unit sizes, the order of an ordered axis's values, the order of a vessel's substitution order, the range of `pinch`) match the values and orders in the tables.
- **S139** The relations in §4.7.1 (which the concept states explicitly: dirt classes, vessel properties, equipment capabilities, washing methods of compatibility rules, action requirements and effects) match the tables.
- **S140** The set of identifiers in `spec/coverage/concept-lists.yaml` matches the set of identifiers written as `` `…` `` in the table of §4.7 (the copy is correct).

### 4.5 Type check of the sample

Sample format (`spec/schema/sample.schema.json`):

```yaml
# SPDX-License-Identifier: CC-BY-4.0
recipes:
  cookedRice:
    output: {type: CookedRice}
    ingredients:
      rice: {type: Rice, amount: "600 g"}     # 部位を持つ食材は part: belly のように書ける
    steps:
      - {let: washed, do: wash, args: [rice]}
      - {let: soaked, do: soak, args: [washed], named: {time: "30 min"}}
      - {let: cooked, do: cookRice, args: [soaked]}
    result: cooked
  curry:
    output: {type: Curry, traits: [Dish, Liquid]}
    ingredients:
      onion: {type: Onion, amount: "2 pcs"}
      roux:  {type: CurryRoux, amount: "100 g"}
      water: {type: Water, amount: "800 ml"}
    steps:
      - {let: peeled, do: peel, args: [onion]}
      - {let: sliced, do: cut, args: [peeled], named: {style: slice, size: "3 mm"}}
      - {let: fried, do: fry, args: [sliced], named: {time: "10 min", heat: medium}}
      - {let: curry, do: simmer, args: [[fried, water, roux]], named: {time: "20 min"}}
    result: curry
menus:
  curryRice:
    make: {rice: cookedRice, curry: curry}
    serve:
      main:
        - {plate: perPerson, on: [DeepPlate, Donburi], items: [rice, curry]}
expect:
  errors: []
  infos: [...]        # P2-3 で手で書く
  requires: [...]     # P2-3 で表の uses から手で書く
  states: {...}       # 盛り付ける値の型・状態・トレイト
  steps: {...}        # 手順ごとの値の型・状態（fried、curry など）
```

A value has a type, a part, a state per axis, traits, and an amount (a dimension and a range, or "unknown").

- **S30** An ingredient's initial state is, for each axis it uses: the first value for an ordered axis, `whole` for shape, and the empty set for prep. The ingredient's definition can override it with `initial`. (added)
- **S31** If an ingredient passed as an action argument does not meet that argument's requirements (trait and axis conditions), error `TYP:stateRequirement` (cutting an onion that was not peeled, for example).
- **S32** A requirement with `anyOf` passes when any one of its options is met (D10). (added)
- **S33** A requirement on an axis the ingredient does not use is not met (cutting water is an error). (added)
- **S34** The effect on an ordered axis gives the larger of the current value and the effect's value (values do not go back, concept §4.1).
- **S35** An ingredient whose shape is `cut(s0)` can become `cut(s1)` only when s1 is finer than s0. The same or a coarser cut is error `TYP:cutCoarser` (concept §4.1).
- **S36** A prep effect adds an item to the set. If the same item is already there, its arguments become the newer ones. (added)
- **S37** Axes not named in an action's effects do not change (concept §4.1).
- **S38** If the dimension of a quantity argument differs from the dimension in the argument's declaration, error `TYP:dimension`.
- **S39** If the value of an enumeration argument (`style`, `heat`) is not in the enumeration, error `TYP:enumValue`.
- **S40** An action, ingredient type or part not in the tables is error `NAM:undefined`.
- **S41** A missing argument with no default, an extra argument, or an unknown named argument is error `NAM:arity`. Arguments with defaults can be left out.
- **S42** Binding the same name twice with `let` in the same recipe is error `NAM:rebind`.
- **S43** Consuming one value twice is error `LIN:doubleUse` (plating also consumes).
- **S44** Values and ingredients left unconsumed become info `LIN:leftover`, not an error (concept §4.1).
- **S45** Writing a list `[a, b, c]` for an argument that takes one ingredient is treated as if `combine([a, b, c])` were filled in (D11). (added)
- **S46** The result of `combine` is of type `Mixture`; its `doneness` is the minimum of the inputs, and its traits `Hazard` and `Allergen(k)` are the union of the inputs (the mixed ground meat rule in concept §4.1). The only axes `Mixture` has are `doneness` and `prep` (starting from the empty set); other axes are not carried over. (added)
- **S47** The amount of `combine` is the total mass if every input can be converted to mass (using density or the typical weight per piece). If any input cannot be converted, it is "unknown", and info `LIN:amountUnknown` is reported. (added)
- **S48** The result of an action with an `output` type (`cookRice` gives `CookedRice`) is that type's initial state with the effects applied, and it carries over the inputs' `Allergen(k)`. (added)
- **S49** For an action with `variant`, the requirements use the first candidate's `uses`, and info `ASM:firstVariant` lists the names of the other candidates (concept §4.2, level 0 with no kitchen).
- **S50** If a recipe's result is `Mixture`, it is replaced by the recipe's `output` type (keeping the state and traits, and adding the traits written in `output`). (added)
- **S51** If a recipe's result is not `Mixture` and differs from the `output` type, error `TYP:output`. (added)
- **S52** A menu's `make` binds the recipe's output value (type and final state).
- **S53** If a plated value has `Hazard`, does not have `RawEdible`, and its `doneness` is below `done`, error `SAF:undercooked` (concept §4.1).
- **S54** Plating a value with `Liquid` on a vessel without `HoldsLiquid` (any one of those listed in `on`) is error `TYP:vesselLiquid` (concept §4.2).
- **S55** The sample's requirements (`requires`) are output as the union of the capabilities in the `uses` of the actions used (the level-0 requirements, with no kitchen).
- **S56** A diagnostic has a name (such as `TYP:stateRequirement`), a severity (error, warning, info), a location (recipe name and step number, or menu name and serve number), and arguments (names of leftover values, names of the other candidates, and so on), and diagnostics are ordered as written in the sample.
- **S471** Passing one ingredient to `blend` gives a result whose shape is `pureed` (D33). (added)
- **S472** Passing a list to `blend` gives a new value (`Mixture`) as if `combine` were filled in (D11, D33). (added)
- **S473** `pureed` is finer than every cut style, and doing `cut` on a `pureed` ingredient is `TYP:cutCoarser` (D33). (added)
- **S57** The curry and rice sample passes with 0 errors, and the output requirements, infos, plated values, and the types, states and traits of each step's values match `expect`. The number of checked items k is the number of leaves of `expect`, and is written in §9.2 in P2-3.

How to use the check script (all subcommands can change the table location with `--vocab DIR`, and give machine-readable output with `--json`; the exit code is 1 if there is an error or an expectation mismatch, otherwise 0):

```
python3 tools/vocabcheck/vocabcheck.py schema          # schema: <n> files, <e> errors
python3 tools/vocabcheck/vocabcheck.py check           # check: <e> errors, <w> warnings, <i> infos
python3 tools/vocabcheck/vocabcheck.py lists FILE      # lists: <n> listed, <m> missing
python3 tools/vocabcheck/vocabcheck.py sample FILE     # sample: <e> errors; expectations: <k> checked, <f> failed
```

### 4.6 Values from the food composition table (D7)

- What is extracted: for the rows of the food numbers that ingredients in the tables refer to, the food number, food name and refuse rate (%). The weight change rate is included only if it is in the Excel file (§9.2).
- Where it goes: `data/mext-fct-2023/refuse.csv` (UTF-8, with a header, in food number order) and `README.md`. The original Excel file goes in `data/mext-fct-2023/source/` and is kept out of git (`.gitignore`).
- **S58** Each value in each row of the extracted data matches the value in the same column of the row with the same food number in the original Excel file.
- **S59** Every food number in the tables' `mext-fct-2023` references is in the extracted data.
- **S60** Extracting twice from the same Excel file gives no diff in the extracted data.
- **S61** The sha256 of the original Excel file matches the value written in `data/mext-fct-2023/README.md`.
- **S465** A value whose source is primary information (`primary`) has, in its `sources.yaml` entry, a URL, the document title, the retrieval date, the location in the document (page, table, section) and a transcription of the number (`quote`: the number and unit as written in the document), and the value matches the transcribed number (a range value matches the transcribed range).
- **S466** A value whose source is provisional (`provisional`) has a record of the search for primary information (`spec/sources/search-log.yaml`: the documents checked, the search terms, the date, and why nothing was found).
- **S467** Primary-information document files (PDF and so on) are not put in the repository (`git ls-files` has no `.pdf`; the registry holds only the URL and the sha256 at retrieval).
- **S469** `spec/notes/prior-work-elements.yaml` lists, for each of the 4 documents of D8, the elements of their model of cooking procedures (task attributes, resources, states, washing, goals, operations, and so on), with page numbers in the document.
- **S470** The mapping table in `spec/notes/prior-work.md` gives, for every element in `prior-work-elements.yaml`, either the Ozen counterpart (vocabulary, type or schedule rule, and the S item of this plan or the concept section) or "no counterpart" with a reason. Those among "no counterpart" that are candidates to add to Ozen are written in §9.2 and reported to the user.
- **S141** The set of food numbers in `refuse.csv` matches the set of food numbers the tables refer to, and the rows are in food number order.
- The text of the header cells of the columns read, and 3 values read by hand, are written in §9.2 in P3-1, and `test_extract` checks them against the real Excel file (to catch reading the wrong column by a path separate from the implementation; S58).

### 4.7 Lists (a copy of concept §12.2 and §12.3)

In P1-6 this table is copied to `spec/coverage/concept-lists.yaml` and used for the check of S29. The identifiers are the ones this plan assigned. Rows that change with the answers to D9 and D11 are fixed when the answers come.

| Kind | Count | Identifiers (concept terms) |
| --- | --- | --- |
| Dimension | 6 | `Mass` `Volume` `Length` `Time` `Temperature` `Count` |
| Unit | 18 | `g` `kg` `ml` `L` `tbsp` (tablespoon, 15 ml) `tsp` (teaspoon, 5 ml) `gou` (gō, 180 ml) `std.units.jp.cup` (200 ml) `std.units.us.cup` (240 ml) `mm` `cm` `s` `min` `h` `degC` `pcs` (count) `pinch` (a pinch) `dash` (a little) |
| Axis | 5 | `doneness` `browning` `shape` `prep` `cleaning` |
| Axis value | 21 | doneness: `raw` `surface` `par` `done` / browning: `none` `light` `golden` `dark` `charred` / shape: `whole` `peeled` `cut` `pureed` (added, D33) / prep: `washed` `soaked` `salted` `marinated` / cleaning: `none` `rinse` `wash` `soakThenWash` |
| Cut style | 7 | `slice` (thin slices) `dice` (cubes) `rollCut` (roll cut) `roughChop` (rough chop) `julienne` (julienne) `biteSize` (bite size) `mince` (mince) |
| Heat level | 3 | `low` `medium` `high` |
| Trait | 30 | Kind: `Vegetable` `Root` `LeafyVegetable` `Meat` `Fish` `Grain` `Noodle` `Dairy` `Fruit` `Seasoning` / Handling: `Peelable` `SkinEdible` `RawEdible` `Hazard` / Storage: `Refrigerated` `Frozen` / `Allergen` (takes the kind as an argument) / Dirt class: `Smelly` `Soiled` `Greasy` `Sticky` `Staining` `Burnt` `Light` / Other: `Fatty` `RoundFish` `Dish` `Liquid` (added) `Sauce` `HoldsLiquid` |
| Ingredient | 37 | Staples: `Rice` `SteamedNoodles` `Naan` / Meat: `Pork` `Chicken` `Beef` / Fish: `Buri` / Vegetables: `Onion` `Carrot` `Potato` `Cabbage` `BeanSprouts` `Lettuce` `Tomato` `Cucumber` / Soup: `Miso` `Dashi` `Tofu` `Wakame` / Seasonings: `Salt` `Pepper` `Oil` `CurryRoux` `WorcesterSauce` (sauce) `Dressing` / Dessert: `VanillaIce` `Cream` `Sugar` `Strawberry` `Banana` `Cornflakes` `ChocolateSauce` / Drinks: `Milk` `Ice` / For the sample and the structure: `Water` `CookedRice` `Mixture` |
| Part | 26 | `Pork`: `shoulder` `shoulderLoin` `loin` `tenderloin` `belly` `leg` / `Chicken`: `thigh` `breast` `tender` `wingTip` `wingStick` / `Beef`: `shoulder` `shoulderLoin` `ribLoin` `sirloin` `tenderloin` `belly` `leg` / `RoundFish`: `head` `collar` `back` `belly` `frame` `skin` `guts` `scales` |
| Alias | 2 | `Komagire` (small cuts of meat) `Karubi` (kalbi, short rib) |
| Action | 28 | Prep: `wash` `soak` `peel` `loosen` `drain` / Cutting: `cut` `tear` / Heating: `cookRice` `fry` (stir-fry) `simmer` `boil` `boilWater` `dissolve` `warm` / Mixing: `mix` `toss` `whip` `season` `blend` / Waiting: `chill` `rest` / Finishing: `plate` `layer` (stack) / Cleanup and leftovers: `clean` `wipe` `store` `discard` / Structure: `combine` (D11) |
| Capability | 10 | `HeatSource` `CuttingSurface` `Container` `RiceCooking` `Blending` `Chilling` `Freezing` `Washing` `Vessel` `WorkSurface` (added) |
| Capability argument | 4 | `stovetop` `oven` `microwave` `toaster` of `HeatSource` |
| Kind of equipment | 26 | Places: `Floor` `Counter` `Sink` `Stove` `GasStove` `IHStove` `Oven` `Microwave` `Toaster` `OvenRange` `Fridge` `Freezer` `Dishwasher` / Tools: `CuttingBoard` `Knife` `Peeler` `FryingPan` `Pot` `MixingBowl` `Colander` `CookingChopsticks` `Ladle` `Whisk` `MeasuringSpoon` `RiceCooker` `Blender` |
| Vessel | 11 | `FlatPlate` `LongPlate` `DeepPlate` `Donburi` `RiceBowl` `SoupBowl` `Bowl` `SmallBowl` `SmallPlate` `Platter` `Glass` |
| Kind of dirt | 11 | `rawMeat` `rawFish` `rawEgg` `soil` `oil` `starch` `odor` `stain` `scorch` `light` `allergen` |
| Compatibility rule | 9 | `afterHazard` `afterAllergen` `afterSoiled` `afterSmelly` `afterGreasy` `afterSticky` `afterBurnt` `afterStaining` `afterLight` |
| Vessel substitution order | 4 | `deepPlate` (deep plate → donburi → bowl) `riceBowl` (rice bowl → donburi) `flatPlate` (flat plate → platter) `smallBowl` (small bowl → small plate, only for food without liquid) |
| Kind of allergen | 9 | Arguments of `Allergen` (the specified raw materials of the Consumer Affairs Agency, as of April 2026 (Reiwa 8); confirmed in P1-4): `shrimp` (shrimp) `cashew` (cashew nuts) `crab` (crab) `walnut` (walnuts) `wheat` (wheat) `buckwheat` (buckwheat, soba) `egg` (egg) `milk` (milk) `peanut` (peanuts) |
| Ingredient (eggs, `plans/ozen-cooking-feedback.md` D110) | 1 | `Egg` |
| Action (eggs, `plans/ozen-cooking-feedback.md` D110) | 2 | `crack` (crack) `beat` (beat) |
| Action (deep frying, `plans/ozen-cooking-feedback.md` D103, D110) | 1 | `deepFry` (deep-fry) |
| Total | 271 | |

#### 4.7.1 Relations the concept states explicitly

In P1-6 this table is copied to `spec/coverage/concept-relations.yaml` and used for the check of S139. Values and relations the concept does not decide (provisional ranges, fineness relations of cut styles other than item 7 below, and so on) are not listed; they are checked in §8.

| # | Relation | concept |
| --- | --- | --- |
| 1 | Classes of the kinds of dirt: `rawMeat` is `Hazard`, `rawFish` is `Hazard` and `Smelly`, `rawEgg` is `Hazard`, `soil` is `Soiled`, `oil` is `Greasy`, `starch` is `Sticky`, `odor` is `Smelly`, `stain` is `Staining`, `scorch` is `Burnt`, `light` is `Light`, `allergen` is `Allergen` | The dirt table in §12.3 |
| 2 | The vessels with `HoldsLiquid` are the 6 vessels `DeepPlate` `Donburi` `SoupBowl` `Bowl` `SmallBowl` `Glass`; `FlatPlate` `LongPlate` `RiceBowl` `SmallPlate` `Platter` do not have it. `LongPlate` inherits from `FlatPlate` | The vessel classes in §12.3 |
| 3 | Equipment capabilities: `IHStove` and `GasStove` have `Stove` and `HeatSource<stovetop>`, `Microwave` has `HeatSource<microwave>`, `OvenRange` has `HeatSource<oven>` and `HeatSource<microwave>`, `Oven` has `HeatSource<oven>`, `Toaster` has `HeatSource<toaster>`, `Pot` and `MixingBowl` have `Container`, `CuttingBoard` has `CuttingSurface`, `RiceCooker` has `RiceCooking`, `Blender` has `Blending`, `Fridge` has `Chilling`, `Freezer` has `Freezing`, `Sink` and `Dishwasher` have `Washing`, `Counter` has `WorkSurface` (added) | §4.2, the capability table in §12.3 |
| 4 | Washing methods of the compatibility rules: `afterHazard` is `wash` (not needed when continuing with the same ingredient in the same state), `afterAllergen` is `wash` (before that person's portion), `afterSoiled` is `wash` before `RawEdible` and `rinse` otherwise, `afterSmelly` is `rinse` before `RawEdible` and desserts and `none` otherwise, `afterGreasy` is `wash` (`none` when the frying pan is used again for a stir-fry), `afterSticky` and `afterBurnt` are `soakThenWash`, `afterStaining` and `afterLight` are `none` | The compatibility rules in §12.3 |
| 5 | `cookRice` requires `prep has soaked`, its output is `CookedRice`, and its candidates are `cooker` `pot` `microwave` in that order | §4.2 |
| 6 | `peel` turns `whole` into `peeled`. The effect of `fry` does not change `shape` | §4.1 |
| 7 | The "finer than" relation of cut styles has `dice` → `mince`, and does not have `mince` → `dice` | §4.1 |
| 8 | `Pork` and `Chicken` have `Hazard`. `Onion` has `Peelable`; `Carrot` has `Root`, `Peelable` and `SkinEdible` | §4.1, §4.4 |

### 4.8 Syntax of the prototype input (D14)

The input is one UTF-8 text. The syntax is a subset of Ozen and follows the decisions in concept §4.4 (brace blocks, statements separated by newlines or `;`, single binding with `let`, named arguments after positional arguments, annotations at the end, quantity literals, `//` and `/* */`).

```
file       := { decl }
decl       := action | macro | recipe
action     := 'action' IDENT '(' [ IDENT { ',' IDENT } ] ')'
macro      := 'macro' IDENT '(' [ IDENT { ',' IDENT } ] ')' '{' quote '}'
recipe     := 'recipe' IDENT '(' [ param { ',' param } ] ')' block
param      := IDENT ':' expr                       // 既定値（コンパイル時の値）
block      := '{' { stmt } '}'
stmt       := ( ingredients | let | if | for | repeat | expr ) { annotation }
ingredients:= 'ingredients' '{' { IDENT ':' [ IDENT ] expr } '}'
let        := 'let' ( IDENT | '$' IDENT ) '=' expr
if         := 'if' expr block [ 'else' ( block | if ) ]
for        := 'for' IDENT 'in' expr block
repeat     := 'repeat' expr block
annotation := '@' IDENT [ '(' args ')' ]
expr       := or ;  or := and { 'or' and } ;  and := not { 'and' not }
not        := 'not' not | cmp ;  cmp := sum [ ( '==' | '!=' | '<' | '<=' | '>' | '>=' ) sum ]
sum        := prod { ( '+' | '-' ) prod } ;  prod := unary { ( '*' | '/' ) unary }
unary      := '-' unary | postfix ;  postfix := primary [ '(' args ')' ]
primary    := quantity { quantity } | NUMBER | STRING | 'true' | 'false' | IDENT
            | '$' IDENT | '[' [ expr { ',' expr } ] ']' | '(' expr ')' | quote
quantity   := NUMBER [ '..' NUMBER ] IDENT          // 数の直後の名前は単位
quote      := 'quote' block
args       := [ expr { ',' expr } ] [ IDENT ':' expr { ',' IDENT ':' expr } ]
```

- Quantities written in a row (`1 h 30 min`) become an addition tree from the left (`Binary +`).
- The only string escapes are `\"`, `\\` and `\n` (S142).
- Built-in units: mass `g` (base) and `kg`; volume `ml` (base), `L`, `tbsp` and `tsp`; length `mm` (base) and `cm`; time `s` (base), `min` and `h`; count `pcs` (base).
- Reserved words: `action` `macro` `recipe` `ingredients` `let` `if` `else` `for` `in` `repeat` `quote` `and` `or` `not` `true` `false`.

- **S62** Invalid UTF-8 input is error `SYN:invalidUtf8`, and is not parsed.
- **S63** An identifier containing non-ASCII characters is error `SYN:nonAsciiIdent` (concept §4.4).
- **S64** UTF-8 (Japanese, emoji) inside strings and comments passes as is, and strings appear in the output as the same bytes (concept §3).
- **S65** On a syntax error (`SYN:unexpectedToken`), the parser skips to the next statement separator (newline, `;`, or the matching `}`) and continues, reporting several errors in one run.
- **S66** If the input size is over the limit (default 1,048,576 bytes), error `SYN:inputTooLarge` without parsing. Input exactly at the limit is accepted.
- **S67** If the parse nesting depth is over the limit (the same value as the depth limit, default 256), `SYN:nestingTooDeep`. Depth is counted per level of recursive descent (parentheses, brackets and blocks, and also unary `-`, `not`, and chains of `else if`), so the C stack does not overflow.
- **S68** `quote` outside a macro body is error `SYN:quotePlacement`. `quote` inside `quote` is also error `SYN:quotePlacement` (there are no macros that make macros, concept §4.3).
- **S69** `$x` outside `quote` is error `SYN:spliceOutsideQuote`.
- **S70** Quantities written in a row, `1 h 30 min`, are read as addition (concept §4.4). Writing quantities of different dimensions in a row is error `TYP:dimension`.
- **S71** If a number literal does not fit in an int64 numerator and denominator (a decimal uses a power of 10 as the denominator), error `SYN:numberTooLarge`.
- **S142** A string escape other than `\"`, `\\` and `\n` is error `SYN:badEscape`.
- **S143** A newline inside parentheses or brackets is not a statement separator.
- **S144** Reserved words cannot be used as identifiers (names of actions, macros, recipes, arguments, `let`); doing so is error `SYN:reservedWord`.

### 4.9 Value types and operations (D15, D16)

There are 6 value types: boolean, quantity, string, list, ingredient reference, and syntax tree.

- Quantity: a single rational value or a range `[lo, hi]`, and a dimension (one of mass, volume, length, time and count, or dimensionless). A rational has a numerator from −(2⁶³−1) to 2⁶³−1 and a denominator from 1 to 2⁶³−1, and is in lowest terms. Internally it is held in the base unit.
- Ingredient reference: an ingredient declared in `ingredients` or the result of an action step (`%n`), plus the share fraction (default 1). Declared ingredients have an amount; step results do not.
- Syntax tree: held only by macro arguments and bodies (it cannot be used other than by splicing with `$`).

Built-in functions:

| Function | Arguments | Result |
| --- | --- | --- |
| `ceil(q)` | A single-value quantity | The value rounded up to an integer, same dimension |
| `floor(q)` | A single-value quantity | The value rounded down to an integer, same dimension |
| `min(a, b)` / `max(a, b)` | Two single-value quantities of the same dimension | The smaller / the larger |
| `len(xs)` | A list | The number of elements (dimensionless) |

- **S72** The value types are only the 6 above, and a quantity's dimension is one base dimension or dimensionless.
- **S73** Decimal literals become rationals with no error (`0.3` is 3/10, and `0.1 + 0.2 == 0.3` is true).
- **S74** Quantities can be added and subtracted only with the same dimension. Different dimensions are error `TYP:dimension`.
- **S75** Dividing quantities of the same dimension gives a dimensionless value. Multiplying or dividing a quantity by a dimensionless value keeps its dimension. Multiplying two quantities that have dimensions, and dividing a dimensionless value by a quantity with a dimension, are error `TYP:unsupportedDimension`.
- **S76** Adding and subtracting ranges uses interval arithmetic (`[a,b] + [c,d] = [a+c, b+d]`, `[a,b] − [c,d] = [a−d, b−c]`). A range can be multiplied or divided only by a single value, and the ends are swapped if it is negative. Multiplying or dividing two ranges, dividing a single value by a range, comparing ranges, and `ceil`, `floor`, `min`, `max` of a range are error `TYP:range`.
- **S77** The result of an operation is exact if its numerator and denominator in lowest terms fit in the range above; otherwise error `MAC:overflow`. Intermediate values are computed in 128-bit integers and never overflow midway.
- **S78** Division by 0 is error `MAC:divisionByZero`.
- **S79** Comparisons (`==` `!=` `<` `<=` `>` `>=`) return a boolean for two single-value quantities of the same dimension, or for two booleans or two strings (only `==` `!=`). Anything else is error `TYP:type` (`TYP:dimension` if the dimensions differ).
- **S80** `and` and `or` short-circuit. When the right side is not evaluated, no fuel is spent on it and errors in it are not reported.
- **S81** Dividing an ingredient reference by a dimensionless positive single value k gives a share with the fraction multiplied by 1/k (`meat / 3` is `meat*1/3`). Dividing a declared ingredient by a quantity of the same dimension as its amount gives a dimensionless ratio (declared amount ÷ quantity). Dividing a step result, which has no amount, by a quantity is error `TYP:amountUnknown`.
- **S82** Built-in functions behave as in the table above; if the arguments' types, dimensions or count do not fit, it is an error (`TYP:type`, `TYP:dimension`, `NAM:arity`).
- **S83** A name right after a number is resolved as a unit; if it is not a built-in unit, error `NAM:unknownUnit`.

### 4.10 Evaluation

For the entry recipe, the default values of its arguments (or the override values) are evaluated in the order written, and then the body block is evaluated. The result is a "list of steps", numbered `%1`, `%2`, … in the order the action calls were evaluated.

- **S84** Statements are evaluated in the order written, and arguments from left to right (named arguments also in the order written). For nested calls the inner call becomes a step first (`cut(peel(onion), 3 mm)` gives `%1 = peel(onion)`, `%2 = cut(%1, 3 mm)`).
- **S85** `let` binds only once. Binding again, in the same recipe, a name in the current scope or an outer scope (with the same expansion mark) is error `NAM:rebind`. Hiding a top-level name (action, macro, built-in) with a local name is allowed.
- **S86** If the condition of `if` is not a boolean, error `TYP:type`. If the condition is false and there is no `else`, nothing happens. `else if` can be chained.
- **S87** The n of `repeat n` is a single value that is a count or dimensionless and a non-negative integer. If 0, the body is not evaluated. Anything else (a fraction, negative, a range, another dimension) is error `TYP:count`.
- **S88** `for x in xs` evaluates the body for each element of the list, binding x fresh. For an empty list the body is not evaluated. Anything that is not a list is error `TYP:type`.
- **S89** A `let` in the body of a loop (`repeat`, `for`) enters a new scope on each pass, and does not clash with the binding from the previous pass.
- **S90** In an action call, if the total number of positional and named arguments differs from the number of declared arguments, if there is an unknown named argument, or if the same argument is passed twice, error `NAM:arity`.
- **S91** An annotation after a statement attaches to the step of the outermost action call of the statement's expression (`let cooked = cook(soaked) @unattended`). If the outermost is not an action call, error `TYP:annotationTarget`.
- **S92** An evaluation error makes the value of that expression an "error" and evaluation continues. An expression that uses an error reports no new diagnostic, and an action call with an error argument emits no step. Statements unrelated to the error are evaluated as usual.
- **S93** On a limit error (fuel, depth, generated nodes, allocation failure), evaluation stops there, and the steps and diagnostics produced so far are output.
- **S94** Entry: if the file has one recipe it is used; if it has several, one is chosen by the `entry` name. If there are several and none is given, error `NAM:ambiguousEntry` listing the candidate names in declaration order (rule 3 of concept §4.6).
- **S95** `params` (`name=expr;name=expr`) replaces the default values of the entry recipe's arguments. Override expressions also count as evaluation steps. An argument name not in the recipe is error `NAM:undefined`.

### 4.11 Macros and hygiene (concept §4.3)

- A macro call `m(e1, e2)` does not evaluate its arguments; it receives them as syntax trees (with the caller's mark and location). It copies the contents of the body's `quote`, replaces `$v` with a copy of the argument's syntax tree, gives the copy a new expansion mark (1, 2, 3, … in evaluation order), and evaluates it at the call site.
- Name resolution: a name with mark m first looks for a binding made by `let` with the same mark m; if there is none, it is resolved where the macro was defined (the top level of the file: actions, macros, built-ins). Names in spliced arguments keep the caller's mark, so they are resolved in the caller's scope.

- **S96** Macro arguments are not evaluated at the call. They are evaluated at each place they are spliced (twice if spliced twice, zero times if not spliced).
- **S97** Names the macro itself wrote inside `quote` and bound with `let` (such as `tmp`) do not clash with the same name in the caller. Expanding the same macro twice in the same scope does not give `NAM:rebind`.
- **S98** Free names the macro itself wrote inside `quote` are resolved where the macro was defined, and are not captured by local bindings in the caller.
- **S99** Names inside an argument spliced with `$x` are resolved in the caller's scope.
- **S100** With `let $x = ...`, the macro can bind to a name passed by the caller, and the caller can use that name after the expansion (the form `marinate(meat, into: marinated)`, concept §4.3).
- **S101** If the argument spliced into `let $x` is not a single name, error `TYP:spliceNotName`.
- **S102** If a name written without `$` in a macro body is none of a `let` in the body, a top-level action, a macro, a built-in or a unit, it is error `NAM:undefinedInMacro` at definition time, even if the macro is never called (including argument names written without `$`).
- **S103** If the number of arguments in a macro call differs from the definition, error `NAM:arity`.
- **S104** The diagnostic for an error in a node that came from a macro body points to the location in the macro definition, and has the expansion history (call sites from inner to outer).
- **S105** The diagnostic for an error inside a spliced argument points to the location in the caller's source.
- **S106** Macros can call macros (including recursion on themselves). Expansion that does not end stops at one of the fuel, depth or generated node limits.
- **S145** If a macro body is not a single `quote { … }`, error `SYN:macroBody`.

### 4.12 Fuel and limits (concept §4.3, §10)

| Limit | How it is counted | Default | When exceeded |
| --- | --- | --- | --- |
| Evaluation steps | 1 each time one node in the table below is evaluated | 1,000,000 | `MAC:fuelExhausted` |
| Depth | The nesting count of nodes being evaluated (the same value is used for parse nesting) | 256 | `MAC:depthExceeded` (`SYN:nestingTooDeep` in parsing) |
| Generated nodes | The number of AST nodes copied by macro expansion plus the number of steps output | 100,000 | `MAC:nodeLimit` |
| Input size | Bytes | 1,048,576 | `SYN:inputTooLarge` |

| Node | Steps | Children evaluated next |
| --- | --- | --- |
| Block `{ … }` | 1 | The statements in order |
| `ingredients` | 1 | 1 per item, and the quantity expression |
| `let` | 1 | The value expression |
| `if` | 1 | The condition, the block of the chosen branch (`else if` is an `if` on the else side) |
| `for` | 1 | The list expression, the body block for each element |
| `repeat` | 1 | The count expression, the body block that many times |
| Call (action, built-in) | 1 | The arguments from the left |
| Call (macro) | 1 | Arguments are not evaluated. The expanded block |
| Name | 1 | — |
| Literal (number, quantity, range, string, boolean) | 1 | — (`15..20 min` is one; `1 h 30 min` is `Binary +` and 2 literals, so 3) |
| List `[…]` | 1 | The elements from the left |
| Binary operation | 1 | Left, then right unless short-circuited |
| Unary operation (`-`, `not`) | 1 | The operand |
| Annotation `@name(…)` | 1 | The arguments from the left |

Worked examples (how the golden expected values are made):

```
action a()
recipe r() {
  repeat 3 {
    a()
  }
}
```

Steps = block 1 + `repeat` 1 + literal `3` 1 + 3 × (block 1 + call 1) = 9. Generated nodes = 3 steps. Depth = block → `repeat` → block → call, 4. With a limit of 8 it stops at the 9th node (the 3rd `a()`), outputs `%1` and `%2`, and then reports `MAC:fuelExhausted`.

```
action a(x)
macro m(v) { quote { a($v) } }
recipe r() { m(1 g) }
```

Steps = block 1 + macro call 1 + expanded block 1 + call of `a` 1 + literal `1 g` 1 = 5. Generated nodes = 3 copies (block, call, spliced literal) + 1 step = 4. Depth = 5.

- **S107** Evaluation steps are counted as in the table above, and the node that exceeds the limit gives error `MAC:fuelExhausted`.
- **S108** Depth is the nesting count of nodes being evaluated, and the node that exceeds the limit gives error `MAC:depthExceeded`.
- **S109** Generated nodes are the nodes copied by expansion plus the steps output, and exceeding the limit gives error `MAC:nodeLimit` at that point.
- **S110** A limit can be used up exactly. Input that uses exactly K passes with limit K, and stops with limit K−1 (for each of steps, depth and generated nodes).
- **S111** The limits can be changed with `ozp_limits` in the API, and `ozp_default_limits()` returns the defaults above.
- Schedule fuel (added, 2026-11-01): building the schedule also uses the evaluation step fuel. Each time the clock advances by one, "the number of tasks + the square of the number of tasks that can be started" is added. When it runs out, `MAC:fuelExhausted` (argument: the limit) is reported at the location of the dish's name, and no schedule is output. Example: a `repeat` that grills 500 times in the same frying pan took several minutes to schedule before the fix (the timeout in `G-CORE-FUZZ`). Now it stops at once. The fuel used by the schedules of golden and `samples/` is at most about 50,000 (`cunit/sched_fuel_bound`)

### 4.13 Diagnostics and output format

A diagnostic code is a 3-letter class + 3 digits (concept §6.1). The prototype's registry is `proto/src/diag_codes.def` (an X macro), and the classes used are `SYN`, `NAM`, `TYP` and `MAC`. In the prototype every severity is error.

The string `ozp_expand` returns (lines end with `\n`):

```
%1 = peel(onion)
%2 = cut(%1, 3 mm) @unattended
error TYP001 dimension 4:12-4:20 args=["Mass", "Time"] via 9:3-9:15, 12:1-12:8
result error steps=2 diagnostics=1
stats steps=17/1000000 depth=6/256 nodes=2/100000
```

- Step line: `%<n> = <動作>(<引数>)`. Arguments are separated by `, `, with named arguments after positional ones as `name: value` in the order written. Annotations are appended as ` @name` or ` @name(<引数>)` in the order attached.
- How values are written: booleans `true`/`false`. Strings as `"…"` (`"`, `\` and newline written back as `\"`, `\\` and `\n`; other bytes as is). Lists `[a, b]`. An ingredient reference is the declared name or `%n`, and a share is `<参照>*<p>/<q>` (omitted when the fraction is 1). A quantity is in the base unit (`g` `ml` `mm` `s` `pcs`), as an integer or `p/q` in lowest terms, with a leading `-` if negative; a range is `<lo>..<hi> <単位>`; a dimensionless value is the number only.
- Diagnostic line: `<重さ> <コード> <名前> <行>:<列>-<行>:<列>`. Columns are byte positions in the line (starting at 1), and the end is the position after the last byte (exclusive). If there are arguments, ` args=[…]` (`"…"` separated by `, `); if there is an expansion history, ` via <位置>, <位置>` (inner to outer).
- Last line: `result ok|error steps=<手順の数> diagnostics=<診断の数>`. If `stats` is true, a `stats` line follows.

- **S112** A diagnostic has a code, name, severity, location, arguments and expansion history, and diagnostics are output in the format above in the order they occur.
- **S113** Codes and names in the registry are each unique, and a registered line is never rewritten or deleted (concept §6.1).
- **S146** The set of names in the registry matches the set of prototype diagnostic names that appear in §4.8 to §4.14 (copied to `proto/tests/diag-names.txt` in P4-3); every code has the form `^[A-Z]{3}[0-9]{3}$`, and its class is one of `SYN`, `NAM`, `TYP` and `MAC`.
- **S147** Every diagnostic in the registry appears at least once somewhere in the golden expected output or the output of the allocation failure tests.
- **S114** The output consists of step lines, diagnostic lines and the `result` line in the format above, and has a `stats` line only when `stats` is requested.
- **S115** Quantities are written in the dimension's base unit, as a fraction in lowest terms or an integer (`1 h 30 min` is `5400 s`, `0.3 g` is `3/10 g`).

### 4.14 Public API and purity (concept §8)

```c
/* SPDX-License-Identifier: MIT OR Apache-2.0 */
typedef struct ozp_allocator {
  void *(*alloc)(void *ctx, size_t size);              /* max_align_t に揃えた領域。失敗は NULL */
  void (*release)(void *ctx, void *ptr, size_t size);
  void *ctx;
} ozp_allocator;
typedef struct ozp_limits { uint32_t steps, depth, nodes, input_bytes; } ozp_limits;
typedef struct ozp_options {
  const ozp_allocator *allocator;   /* 必須 */
  ozp_limits limits;
  const char *entry;                /* NULL ならレシピが 1 つのときだけ通る */
  const char *params;               /* NULL か "name=expr;name=expr" */
  int stats;                        /* 0 以外なら stats の行を付ける */
} ozp_options;
ozp_limits ozp_default_limits(void);
char *ozp_expand(const char *src, size_t src_len, const ozp_options *opt);  /* 結果の確保に失敗したら NULL */
void ozp_free_string(const ozp_options *opt, char *s);
```

The driver `ozenproto` (`proto/driver/`, outside the core): `ozenproto [--entry NAME] [--param name=expr]... [--limit-steps N] [--limit-depth N] [--limit-nodes N] [--limit-input N] [--stats] [--count-allocs] [--fail-alloc-at N] FILE`. `--count-allocs` prints the number of allocations to standard error, and `--fail-alloc-at N` makes the Nth allocation fail (for tests). It reads the file, calls `ozp_expand` with an allocator that uses `malloc`, and writes the result to standard output. If the first line of a golden input is `// args: …`, the test runner script passes those arguments.

- **S116** The only symbols the core library exposes are the 3 functions `ozp_default_limits`, `ozp_expand` and `ozp_free_string`, and internal symbols starting with `ozpi_`.
- **S117** The core (`proto/src/`) does not use I/O, time, random numbers, environment variables, threads, dynamic loading, the `malloc` family, `exit`, `abort` or `assert`. The only headers the sources include are `stddef.h` `stdint.h` `stdbool.h` `string.h` `limits.h` and its own, and the library's undefined symbols are only those in the allowed list (`memcpy` `memmove` `memset` `memcmp` `__stack_chk_fail`).
- **S118** The core has no writable global variables (no global state, concept §8).
- **S119** The core does not use floating point (concept §6).
- **S120** Every allocation goes through the given allocator, and after `ozp_free_string` nothing is left unfreed.
- **S121** When the i-th allocation is made to fail (for every i from 1 to the total number of allocations), it returns a result containing `MAC:outOfMemory` or NULL, does not crash, and after `ozp_free_string` nothing is left unfreed. When i is the total number of allocations + 1, the result is the same as a run with no failure.
- **S148** In the ASan configuration, the arena places a poisoned gap (`ASAN_POISON_MEMORY_REGION`) after each allocation, and ASan reports reads outside the bounds of an arena array.
- **S149** The core does not use `__int128`, `__uint128_t` or `_BitInt` (D15).
- **S122** The same input and the same settings give the same output (not one byte differs between 2 runs in the same environment, between gcc and clang, or between Debug and Release).
- **S123** The driver's exit code is 0 for `result ok`, 1 for `result error`, and 2 for a usage error, an unreadable file, or `ozp_expand` returning NULL (when `--fail-alloc-at` makes the result allocation fail).

### 4.15 Environments (D17 to D19)

- **S124** It builds with gcc 13 and clang 17 with no warnings under `-Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wstrict-prototypes -Werror` and the C23 standard (`-std=c23` for clang, `-std=c2x` for gcc 13; D13) (Debug and Release). In the `compile_commands.json` of the 4 configurations, every `.c` in `proto/src/` has these 8 flags (not an extended variant such as `-std=gnu2x`).
- **S125** In builds with ASan and UBSan (`-fno-sanitize-recover=all`), the unit tests, golden and allocation failure tests all pass.
- **S126** Running libFuzzer (with ASan and UBSan) for 600 seconds gives 0 crashes, leaks and timeouts (5 seconds per input).
- **S127** In WASM (Emscripten, run with node) all golden tests pass, and the output digest matches native.
- **S128** The bridge that calls `ozp_expand` from JS (`proto/wasm/bridge-test.mjs`, with input containing Japanese and emoji strings) gives results that match native (the "a few dozen lines of JS" in concept §8).
- **S129** On the iOS simulator (arm64) all golden tests pass, and the digest matches native (D19).
- **S130** The static library can be made into an `.xcframework` that bundles the device and simulator builds, containing `ios-arm64/` and `ios-arm64-simulator/` (D19, the table in concept §9).
- **S131** Builds and tests use the tool versions recorded in `toolchain.txt`. If a version differs, the check fails. The compiler each configuration actually used (`CMAKE_C_COMPILER_ID` and `_VERSION` in `CMakeCCompiler.cmake`) and `xcodebuild -version` in the iOS workflow are also checked.
- **S150** The result of calling `ozp_expand` inside a browser Web Worker (the same input as `bridge-test.mjs`) matches native (headless Chrome, a temporary profile, served on `127.0.0.1`; the "Web Worker recommended" in concept §8).

### 4.16 Report and documents

Sections of the report `docs/prototype-report.md`: (1) how it feels to write and the amount of bugs, (2) results per environment, (3) fuzzing, (4) what the user decides. The same order as the check items in concept §9.

- **S132** The report has the 4 headings above.
- **S133** The report's table of line counts (per group of files) matches the counts from `wc -l`.
- **S134** The report's table of results per environment (golden pass counts and digests) matches the results of running in each environment.
- **S135** The report's fuzzing numbers (runs, time, number of problems found) and defect counts (per means of detection) match `build/fuzz/summary.txt` and `proto/defects.tsv`. The commit in each row of `defects.tsv` is an ancestor of `main`, and every commit in `plan-start..HEAD` whose subject starts with `fix` and which changes `proto/src/` is in `defects.tsv`.
- **S136** The prototype usage examples in the README match output from actually running them.

### 4.17 Summary of invariants

- The table check script rewrites nothing (S28).
- The prototype core is pure (S117 to S119), deterministic (S122), and does not crash even on allocation failure (S121).
- Golden expected values are not changed in the same commit as the implementation (D20, `G-PROTO-GOLDEN-INDEPENDENT`).
- The diagnostic registry is append-only (S113).

### 4.18 List of things to add or change

| Kind | ID / path |
| --- | --- |
| Common | `.gitignore`, `LICENSE-MIT`, `LICENSE-APACHE`, `LICENSE-CC-BY-4.0`, `NOTICE`, `README.md`, `toolchain.txt`, `scripts/check-spdx.sh`, `scripts/check-tools.sh`, `scripts/check-plan-tests.sh`, `scripts/check-commit-separation.sh`, `scripts/check-red-first.sh` (and the list of exceptions `scripts/red-first-exceptions.txt`), `scripts/check-selftests.sh`, `scripts/check-lists-copy.py`, `scripts/goals.sh` (the code blocks of §5.1 turned into functions), and each `scripts/test-*.sh` |
| Tables (A) | `spec/vocab/*.yaml` (11 files), `spec/schema/vocab.schema.json`, `spec/schema/sample.schema.json`, `spec/coverage/concept-lists.yaml`, `spec/samples/curryRice.yaml`, `spec/fixtures/{vocab-ng,vocab-ok,sample-ng,sample-ok}/` |
| Table tools (A) | `tools/__init__.py`, `tools/vocabcheck/{__init__.py,vocabcheck.py,qty.py,test_qty.py,test_vocabcheck.py}`, `tools/fct/{__init__.py,extract.py,test_extract.py}`, `spec/coverage/concept-relations.yaml` |
| External data (A) | `data/mext-fct-2023/{README.md,refuse.csv}` (the original Excel file goes in `source/`, outside git) |
| Prototype (B) | `proto/CMakeLists.txt`, `proto/include/ozenproto.h`, `proto/src/*.c`, `*.h`, `diag_codes.def`, `proto/driver/main.c`, `proto/tests/{unit,golden,oracle,oom,corpus}/`, `proto/tests/diag-names.txt`, `proto/fuzz/{fuzz_expand.c,ozen.dict}`, `proto/wasm/{bridge-test.mjs,worker-test.html,worker.js}`, `proto/scripts/*`, `proto/defects.tsv` |
| CI (B, D19) | `.github/workflows/ios-proto.yml` |
| Report (C) | `docs/prototype-report.md` |
| Public API symbols | `ozp_default_limits`, `ozp_expand`, `ozp_free_string` |
| Diagnostic codes | Prototype: `proto/src/diag_codes.def` (`SYN`, `NAM`, `TYP`, `MAC`). Table checks: names `NAM:` `TYP:` `LIN:` `SAF:` `ASM:` (no numbers) |

### 4.19 Code layout

| Location | What goes there |
| --- | --- |
| `tools/vocabcheck/qty.py` | Lexing of quantity strings and conversion to rationals (`fractions.Fraction`) |
| `tools/vocabcheck/vocabcheck.py` | Loading the tables, the subcommands `schema`, `check`, `lists`, `sample`, and diagnostic output |
| `tools/fct/extract.py` | Extracts the target rows from the food composition table's Excel file. `--check` re-extracts and compares |
| `proto/src/alloc.c`, `arena.c` | Funneling allocator calls, the arena (linked chunks), how allocation failures are passed on |
| `proto/src/u128.c`, `rational.c` | 128-bit signed integers (multiplication, division, gcd), rational and interval arithmetic |
| `proto/src/utf8.c`, `lex.c`, `parse.c` | UTF-8 validation, lexing, the recursive descent parser, the AST (arrays in the arena, referenced by index) |
| `proto/src/value.c`, `eval.c` | Values, name scopes, evaluation, counting fuel and limits |
| `proto/src/macro.c` | Expansion, marks, splicing, expansion history |
| `proto/src/diag.c`, `diag_codes.def` | The diagnostic array, the registry |
| `proto/src/render.c`, `api.c` | The output string, the public API |
| `proto/driver/main.c` | CLI (reading the file, the `malloc` allocator, the exit code) |
| `proto/tests/unit/` | C unit tests (registered in ctest) |
| `proto/tests/oom/oom_test.c` | Allocation failure injection |
| `proto/fuzz/fuzz_expand.c` | The libFuzzer entry point |
| `proto/scripts/` | `build-all.sh`, `run-golden.sh`, `digest.sh`, `fuzz.sh`, `worker-test.sh`, `check-pure.sh`, `check-noglobal.py`, `check-flags.py`, `check-compilers.sh`, `check-diag-registry.sh`, `check-report.py`, `check-readme-example.sh`, `test-driver.sh` and each `test-*.sh` (the commit separation check is the common `scripts/check-commit-separation.sh`) |


### 4.20 D: Core foundation (§13-4)

The core goes in `core/`. It is written with a narrowed set of C23 features (D13). Tests are golden through the CLI (§4.25) (`tests/golden/<分類>/<名前>.ozen` and `.out`), and C unit tests (`core/tests/unit/`). The golden output formats (diagnostic lines, text IR, schedule text) are added to §4 in P7-0 and P9-0 (added). Diagnostic names belong to the classes of concept §6.1 (`SYN`, `NAM`, `TYP`, `LIN`, `SAF`, `KIT`, `SCH`, `MAC`, `PKG`, `ASM`), and are added to `core/tests/diag-names.txt` and this section in the first task of each phase.

- **S151** The sources in `core/` do not use anything outside the 12 C23 features D13 allowed (`#embed`, `_BitInt`, `<stdckdint.h>`, `<stdbit.h>`, standard library functions added in C23).
- **S152** The AST, IR and values live in one arena per context, and nodes refer to each other by 32-bit indexes (the size of the index type is fixed by `static_assert`). The arena's array types are defined only inside `core/src/arena.c` and are incomplete types to other files, so getting an element from an index goes only through the functions in `arena.h` (concept §10).
- **S153** Identifiers and strings are interned into integer IDs, and the same spelling gets the same ID (concept §10).
- **S154** Every node the parser makes has a span (file number, start and end byte positions), and a child's span is inside its parent's span (concept §10).
- **S155** Nodes made by expansion have the expansion mark and the chain of call sites (expansion history) (concept §4.3, §10).
- **S156** Diagnostics are collected in an array and returned at the end. Without stopping midway, one run returns several errors (concept §10).
- **S157** The parser accepts every rule of `core/grammar.ebnf` (the grammar written out from concept §4 in P7-0). When all golden inputs are parsed with `ozen debug ast --rules`, the set of rules used covers all rules of the grammar.
- **S158** It parses the declarations `dimension`, `unit`, `axis`, `trait`, `ingredient`, `alias`, `kind`, `vessel`, `dirt`, `rule`, `resource`, `tool`, `action` (including `variant` and `extend action`), `recipe`, `menu`, `kitchen`, `enum`, `type` (including `check`), `property`, `macro`, `package`, `module`, `bundle`, `phrases`, `import` and `local` (concept §4).
- **S159** It parses the forms inside a type's `< >` (a bare part, a part's trait, `軸: 値`, `>=` and `<=`, `has`, combinations, the options of D10) (concept §4.4).
- **S160** Indentation has no meaning: changing spaces and tabs without changing the newlines that separate statements gives the same AST dump (concept §4.4).
- **S161** Name resolution does not depend on case. Names that break the naming convention are still resolved (concept §4.4).
- **S410** Invalid UTF-8 input is a `SYN` error, and is not parsed (concept §3).
- **S411** An identifier containing non-ASCII characters is a `SYN` error. UTF-8 inside strings and comments passes as is (concept §3, §4.4).
- **S412** It parses multi-line strings `"""…"""` and keeps their contents as is (concept §4.4, §4.7).
- **S413** Binding a `let` name again in the same scope is a `NAM` error (concept §4.4).
- **S414** In an action or recipe call, a missing argument with no default, an unknown named argument, or passing the same argument twice is a `NAM` error (concept §4.4).
- **S415** An annotation is written after the statement or declaration it belongs to, and attaches to the outermost action of the statement's expression. An annotation with nothing to attach to is a `TYP` error (concept §4.4).
- **S416** In an ingredient declaration `名前: 型 量`, the type can be left out if the name is the type name with its first letter in lower case (`rice: 600 g` is `Rice`) (concept §4.4).
- **S162** One file is one module, and the module name comes from the package name and the folder path (`dashi/pantry/fish.ozen` is `dashi.pantry.fish`) (concept §4.7).
- **S163** Inside the same package the package name is left out; other packages are written starting with the package name. If a top-level folder name of one's own package is the same as a dependency's package name, it is an ambiguity error at load time (concept §4.7).
- **S164** It accepts `import a.b` (used qualified), `import a.b.{x, y}` and `import a.b as c`; there is no form that imports everything with `*` (concept §4.7).
- **S165** `std.prelude` is imported implicitly (concept §4.7).
- **S166** If the same name comes from two `import`s, it is an ambiguity error where it is used. Just importing is not an error (concept §4.7).
- **S167** Definitions in the file take precedence over `import`, and the override is reported as info (concept §4.7).
- **S168** A `local` definition can be used only inside its file, and trying to `import` it is an error (concept §4.7).
- **S169** A dish that does `import` of a menu, or files that import each other in a cycle, is an error (concept §4.6).
- **S170** Referring to a kitchen by name from a dish or a menu is an error (concept §4.6).
- **S171** It does not depend on declaration order (forward references are allowed) (concept §4.6).
- **S172** Entry: if the file has one menu, that one; if several, chosen by name; if there is no menu and only dishes, a single dish is treated as a menu of one dish (concept §4.6).
- **S173** Moving a dish written in one file out to another file and adding an `import` gives text IR that does not differ by one byte before and after (concept §4.6).
- **S174** The fields of `package.ozen` are only name, version, dependencies (`requires`) and license; any other field is an error (concept §4.7).
- **S175** `std` is a reserved name; no other package, module, or module inside a bundled file can take it (concept §4.7, §10).
- **S176** A version string in `requires` means "that version or later, compatible" (the compatibility range is D30). Requiring a version newer than the local `std`, or an incompatible version, is a `PKG` error (concept §4.7).
- **S177** Two versions of the same package mixed in one project is an error (concept §4.7).
- **S178** Diagnostics of imported packages are hidden by default; `SAF` errors are always shown, even from dependencies (concept §6.1).
- **S424** Candidates added by `extend action` appear in the list of assumptions, and the first candidate is always the one from the original definition (concept §4.2).
- **S425** `local` definitions do not appear in the action signature list or in the JSON of the definition list (concept §4.7).
- **S426** There is no range notation (`>=` and so on) for versions in `requires`; writing one is a `PKG` error (concept §4.7).
- **S427** The version of a dependency copied into the project is the version in that copy's `package.ozen` (concept §4.7).
- **S428** Using a `std` definition marked deprecated gives warning `deprecated` (concept §4.7).
- **S179** The `std` source embedded in the core library does not differ by one byte from the source in `stdlib/` (concept §4.7, §8). Embedding is done by generating a C array at build time (not with `#embed`).
- **S180** The limits on input size, recursion depth, expansion node count and evaluation step count can be changed by settings; exceeding one gives a diagnostic and stops, and does not crash (concept §10).
- **S181** The core (`core/src/`) does not use I/O, time, random numbers, environment variables, threads, dynamic loading, the `malloc` family, `exit`, `abort` or `assert`, and depends on nothing other than the C standard library (concept §8).
- **S182** The core has no writable global variables (concept §8).
- **S183** The core does not use floating point (concept §6).
- **S184** The allocator can be replaced, and making an allocation fail at any point does not crash and leaves nothing unfreed (concept §8).
- **S185** The same input gives the same output regardless of configuration and environment (concept §6, §11.2).

#### 4.20.1 Grammar and the golden output format (decided in P7-0; all (added))

- The grammar is `core/grammar.ebnf` (130 rules). It was written out from the syntax sketch and decisions in concept §4, and the points the sketch alone does not settle were decided as follows
  - The temperature unit is `degC` (identifiers are ASCII only, so `°C` cannot be written; the same as `units.yaml` in the tables)
  - `/` inside a unit is written with no spaces around it (`0.6 g/ml`). With spaces it is division. Fractions such as `1/4` also have no spaces
  - In the expression right after `if`, `for … in` and `repeat`, `名前 { … }` (a configuration value) is not read (`{` starts the block)
  - Items in a declaration body are separated by newlines or `,`. `名前: 値` (values can be listed with `,`: `axes: doneness, shape`), `名前 { … }` (a section: `props { … }`), `名前` alone (a marker: a list of parts and so on), `uses`, `takes`, `variant`, `steps`. The kitchen's `floor: Floor { capacity: 2 }` and the part's `shoulderLoin: loin { fat: high }` are read as `名前: 構成の値`
  - Item names (`名前:`) and named arguments can also be reserved words (`kind: primary`)
  - An action argument's type is `型 [= 既定値]`; recipe and menu arguments may have only a default value, like `servings: 4` (the type comes from the value)
  - `where` has the form `where T: Vegetable`, one per type variable
  - A compatibility rule is `rule after 分類: 洗い方 [before 対象]`. The target `any use` means "before anything". A rule that cannot be relaxed gets the annotation `@fixed`, and a rule that only reports info gets `@info`
  - The fineness relation of cut styles is written inside an enum type or an axis's value list as `finer slice -> julienne, mince`
  - Sources such as primary information are declared with `source 名前 { … }` (`stdlib/sources.ozen`), and values get the annotation `@source(名前)`
  - `from:` in a bundled file is `from: パッケージ名 "版"`, and phrases are `phrases 言語 """…"""`
  - Receiving several outputs is `let (a, b, c) = …`
  - `check` is `check コード 名前: 式 else 重さ` as in D32 (the code can be left out)
- Output of `ozen check FILE`: diagnostics one per line, then a result line at the end. The exit code is 0 if there are no errors, 1 if there are errors, and 2 for a usage error or an unreadable file
  - Diagnostic line: `<重さ> <コード> <名前> <道筋>:<行>:<列>-<行>:<列>[ args=["…", …]][ via <道筋>:<行>:<列>-<行>:<列>, …]: <英語の表示>`. The severity is `error`, `warning` or `info`. Lines and columns count from 1, and columns count bytes. `via` is the expansion history (from the inner one). The English message is the name's camel case split into lower case words (`peelMissing` is `peel missing`, D21). The code of a `check` in the author's package is `パッケージ名:コード` (D32)
  - The path is the path as passed to the CLI (the golden runner moves to the folder of the input and passes only the file name). For a folder golden (a project) it is the path from the root. A `std` file is `std/<stdlib/ からの道筋>`
  - Result line: `result ok errors=<e> warnings=<w> infos=<i>` (`result error …` if there are errors)
  - Diagnostics are ordered by file number → start position → code → arguments
- `ozen debug ast FILE`: prints the AST one node per line, indented by 2 spaces per depth level, as `<ノードの種類>[ <名前か値>] <行>:<列>-<行>:<列>`. `--no-spans` leaves out the locations (used by `prop/reindent`). `--rules` prints the names of the grammar rules used in parsing, one per line, without duplicates, in name order
- The names and default severities of the diagnostics of D are as follows (also written in `core/tests/diag-names.txt` in the same order; the registry codes are assigned in this order). Those without a severity are errors
  - `SYN`: `invalidUtf8`, `nonAsciiIdent`, `unexpectedToken`, `unexpectedCharacter`, `unterminatedString`, `unterminatedComment`, `badEscape`, `numberTooLarge`, `reservedWord`, `inputTooLarge`, `nestingTooDeep`, `quotePlacement`, `spliceOutsideQuote`, `macroBody`, `importStar` (`import a.*`), `memberPlacement` (an item that cannot be written in that declaration), `argumentOrder` (a positional argument after a named argument)
  - `NAM`: `undefined`, `ambiguous` (a name that came from two `import`s was used), `rebind`, `duplicateDefinition` (a definition of the same name in the same module), `importLocal`, `localOverride` (info: a definition in the file overrode an `import`), `unknownModule`, `kitchenReference` (a dish or menu referred to a kitchen by name), `ambiguousEntry`, `noEntry`
  - `PKG`: `importCycle`, `recipeImportsMenu`, `topFolderClash`, `unknownPackageField`, `stdReserved`, `requiresNewer`, `requiresIncompatible`, `requiresRange`, `twoVersions`, `badVersion`, `missingPackageField`
  - `MAC`: `fuelExhausted`, `depthExceeded`, `nodeLimit`, `outOfMemory`

### 4.21 E: Type check (§13-5)

Dimensions:

- **S186** Adding quantities of different dimensions is a `TYP` error (`200 g + 2 tbsp`) (concept §4.1).
- **S187** Converting between `g` and `ml` is possible only when the ingredient declares a density. Converting an ingredient without a density is a `TYP` error (concept §4.1).
- **S188** `[any amount]` is normally the warning `vagueAmount`, and an error under `--strict` (same as `--deny strict`) (concept §4.1).
- **S189** A quantity in a unit with a range (`pinch` and so on) gives no diagnostic, even in strict mode (concept §4.1).
- **S190** Putting `@deny(strict)` on a package or recipe makes the `strict` group an error in that scope (concept §4.1).
- **S191** The content of the diagnostic group `strict` is defined in `std`, and authors can add to it (concept §4.1).
- **S192** Assumptions (`ASM`) stay information even in strict mode (concept §4.1).
- **S193** Count is one dimension. It converts to weight through the rough weight per piece, and fractions such as ¼ are kept as rationals (concept §4.1).
- **S194** A cycle in unit definitions is an error, and the base unit cannot be changed later (concept §4.3).
- **S195** `cup` is decided by which of `std.units.jp` (200 ml) and `std.units.us` (240 ml) was imported with `import` (concept §4.3, §12.2).

States:

- **S196** Not meeting the state requirement on an action's input is an error (cutting after forgetting `peel`) (concept §4.1).
- **S197** A state is a combination of independent axes, so reverse-order cooking (boil then peel, grill then cut) passes (concept §4.1).
- **S198** An ordered axis moves in one direction only, and a requirement can be written with `>=` (concept §4.1).
- **S199** The cut can only move toward finer cuts (concept §4.1).
- **S200** The set of things done only grows, and each item can carry arguments (concept §4.1).
- **S201** When an author adds an axis with `axis`, it is checked the same way as the standard axes (concept §4.1).
- **S202** Each ingredient can leave out axes and stages it does not use (concept §4.1).
- **S203** Heating (`doneness`) and browning (`browning`) progress independently (concept §4.1).
- **S204** An axis that is not written is not asked about in an input type, and is carried over from the input in an output type (concept §4.4).
- **S205** State axes defined by an author are checked at the point of definition (unreachable values, conflicting transitions) (concept §4.3).
- **S474** In the type check of the production core too, `blend` works by the same rules as S471 to S473 (D33).

Safety and allergens:

- **S206** An ingredient with `Hazard` is a `SAF` error unless it is at `doneness >= done` before it is served (concept §4.1).
- **S207** An ingredient meant to be eaten raw can be made an exception in that ingredient's definition (concept §4.1).
- **S208** A `SAF` diagnostic cannot be lowered from anywhere. Putting `@allow` on it adds the information "this diagnostic cannot be lowered" (concept §6.1).
- **S209** Using an ingredient with an allergen in the portion of a person allergic to it, or using a tool soiled with that allergen without washing, is a `SAF` error (concept §12.3, T2).

Mixing and linearity:

- **S210** Mixing makes a new value, and the states of the individual ingredients are no longer tracked (concept §4.1).
- **S211** Linear quantities are conserved as a total (concept §4.1).
- **S212** The safety and allergens of a `Mixture` include everything in it (concept §4.1).
- **S213** A consumed ingredient cannot be used again, and consuming the same name twice is an error (concept §4.1).
- **S214** Not having enough quantity is an error (concept §4.1).
- **S215** Parts that were not used up are reported with the information `leftoverPart`, and do not disappear silently (concept §4.1, §6.1).
- **S216** Writing `store` or `discard` gives no diagnostic, and the step appears in the schedule table (concept §4.1).
- **S217** Two parallel branches both consuming the same ingredient is an error (concept §4.2).

Parts and aliases:

- **S218** A part is a type argument. Parent and child are expressed by inheritance, and properties by traits (concept §4.1).
- **S219** A union alias allows only what holds for all candidates: a trait requirement passes if all candidates have it, an attribute is the widest range combining the candidates' ranges, and `Hazard` is present if even one candidate has it (concept §4.1).
- **S220** An alias is expanded at the type level, and the alias's name is used in display and diagnostics (concept §4.1).
- **S221** `|` means "one of", and "both", as in mixed ground meat, is expressed with the `contains` of a `Mixture` (the ratios may be ranges) (concept §4.1).
- **S222** A butchering action has several outputs, and the total quantity is conserved within the range of the yield (concept §4.1).
- **S417** Writing a part on an ingredient without parts (`Onion<belly>`) is a `TYP` error (concept §4.4).
- **S418** Passing an ingredient that does not meet an action's type constraint `where T: トレイト` is a `TYP` error (concept §4.1).
- **S419** A part can be defined once on a trait (`RoundFish`), and every ingredient with that trait has that part (concept §12.3).
- **S420** The discarded parts among a butchering action's outputs (guts, scales) are treated as `discard`, and do not appear in the leftover information (concept §12.3).
- **S421** If a part and a trait have the same name, it is a `NAM` error at the point of definition (concept §4.4).
- **S223** Season (`meta`) is not used for the type check or the schedule (changing `meta` leaves the IR and the schedule the same) (concept §4.1).

Actions, `uses` and `variant`:

- **S224** An action's `uses` is written as capabilities (traits), and the equipment kinds (`kind`) have the capabilities (concept §4.2).
- **S225** The requirement is decided from the quantity (`Container<capacity >= volume(x) * 1.2>` asks for a pot of 2.4 L or more for 2 L of curry) (concept §4.2).
- **S226** The type check runs on every `variant`, so errors inside unused candidates are also found without a kitchen (concept §4.2).
- **S227** Candidates added with `extend action` come after the candidates of the original definition, in order of package name (concept §4.2).
- **S228** A recipe's requirements are decided from the `uses` of the actions it uses, and the author's `requires` is an extra condition on top (concept §4.2).
- **S229** A second definition with the same name (overloading a name) is an error (concept §4.3).

Kitchen and option types:

- **S230** The kitchen spec and the build-time options are checked with the `std` types `Kitchen` and `Options`. A unitless `capacity: 500`, a `count: -1` and a misspelled `cleanup: afterDinner` are `TYP` errors with a span (concept §4.2).
- **S231** Range constraints are intervals only (`a..b`, `a..`, `..b`), and a value outside the range is a `TYP` error (concept §4.2).
- **S232** A type's `check` has a name and a default severity, and the severity can be changed by the mechanism of §6.1 (concept §4.2).
- **S233** A `check` that refers to the kitchen is evaluated only when a kitchen is injected (concept §4.2).
- **S234** When a kitchen is injected, each `variant` whose `uses` cannot be met is removed. If all are removed, it is a `KIT` error listing the candidates (concept §4.2).
- **S235** The kitchen notation is "name: kind { attributes }". Leaving out `count` means 1, and the type annotation `: Kitchen` can be left out (concept §4.2).
- **S236** Kitchen capacities are compile-time values and can be used for the number of batches, as in `ceil(meat / pan.capacity)` (concept §4.2).
- **S237** An option that does not fit the level (`--cleanup` without a kitchen, and so on) gives a warning and is ignored (concept §5.1).
- **S429** If the kitchen does not meet a recipe's `requires`, it is a `KIT` error before the schedule is built (concept §4.2, §6.1).
- **S430** Places left out of the kitchen take default values (1 sink, 1 work area and so on; the defaults of the `std` type `Kitchen`) (concept §4.2).
- **S431** The fuel limit also applies to `check` expressions (concept §4.2).
- **S432** A branch on a kitchen value, such as `if kitchen.has(種類)`, is evaluated at compile time (concept §4.2).

Vessels:

- **S238** Plating a soup (a value that needs `HoldsLiquid`) in a vessel that cannot hold liquid is an error (concept §4.2).
- **S239** `on A or B` is a fallback order: if A is short, B is used (concept §4.2).
- **S240** The number of vessels is checked (servings × vessels per person ≤ number of vessels), and if even the fallback order is short, it is an error (concept §4.2).
- **S241** Plating divides the quantity by servings and puts it in each vessel, and exceeding a vessel's capacity gives a warning (concept §4.2).

Linking menus and dishes:

- **S242** A recipe's output type is the type written, or, if none is written, a type named after the recipe with its first letter in upper case (concept §4.5).
- **S243** `make レシピ(…)` and `buy 品名` can be swapped if their types are the same (concept §4.5).
- **S244** The properties of an output are decided from the ingredients (allergens are the union over the ingredients, safety is the last doneness, the state is the result of the last action) (concept §4.5).
- **S245** The breakdown of `choose perPerson from { … }` is decided from each person's attributes. If left out, it is split evenly and reported as an assumption (concept §4.5).
- **S246** The order inside `assemble { … }` is the layering order (concept §4.5).
- **S247** Something made once can be divided with `split` and passed to several recipes as arguments (concept §4.5).
- **S248** A single recipe with a default vessel (`@vessel`) is given plating and a wave as a menu of one dish. Without a default vessel, plating is left out, the schedule runs "until it is done", and this is reported as an assumption (concept §4.5, §5.2).
- **S249** When a recipe that takes ingredients as arguments is built on its own, the arguments are assumed to be "ingredients that come from outside", and this is reported as an assumption (concept §5.2).
- **S250** There is one wave by default, and from the second wave on each has `after 前の波 + 間隔` (concept §4.5).
- **S251** The serving time cannot be written in code (only `--serve-at`) (concept §4.5).
- **S252** Vague time properties (`whileWarm` and so on) are defined in `std` as named properties with ranges, and are not treated as vague in strict mode (concept §4.5).
- **S422** A recipe can have several outputs (`-> (Buri<fillet>, Buri<kama>, Buri<ara>)`) (concept §4.5).
- **S423** The defaults of `Dish` (vessel, serving timing) can be overridden by the menu's `plate`, and the menu side wins (concept §4.5).

Compile-time evaluation (level 3):

- **S253** Compile-time values (servings, ingredient quantities, capacities, lists) can be used in `if` and `for` (concept §4.3). Cooking-time values (sensors and so on) are external integration and out of scope (§1).
- **S254** Macros are hygienic, with a mark per expansion, resolution at the definition site, and resolution of `$x` at the call site (concept §4.3; the same rules as S96 to S100 of B, in the production core).
- **S255** At the point of a macro definition, only the number of arguments and undefined names are checked (concept §4.3).
- **S256** The type check runs on the AST after expansion (concept §4.3).
- **S257** The location of an error inside a macro carries the expansion history (concept §4.3).
- **S258** Evaluation is pure and always stops at the fuel limit (concept §4.3).
- **S259** There are no macros that make macros, and no escape hatch for capturing names on purpose (concept §4.3).

The diagnostic mechanism (concept §6.1):

- **S260** Every diagnostic has a code of a 3-letter category and 3 digits (`TYP012` and so on) and a name (`peelMissing` and so on), and both appear in the output.
- **S261** The severity is not part of the code, and the default per code can be changed in settings.
- **S262** `--allow`, `--warn` and `--deny` can be given a name, a category prefix (`LIN`) or a named group (`strict`).
- **S263** `@allow`, `@warn`, `@deny` and `@expect` change only the scope of the declaration or expression they are attached to.
- **S264** `@expect` suppresses a diagnostic, and warns when that diagnostic no longer appears.
- **S265** The severity priority is: annotation in code > build-time setting > default.
- **S266** A code and name used once are never reused, even after being retired (the registry is append-only).
- **S267** A diagnostic can carry fix suggestions (in a machine-readable form).
- **S268** The diagnostic categories are the 10 categories `SYN`, `NAM`, `TYP`, `LIN`, `SAF`, `KIT`, `SCH`, `MAC`, `PKG` and `ASM`.
- **S269** The examples in the "severity criteria" table of concept §6.1 (7 error, 5 warning and 5 information examples) match the default severity of each diagnostic.
- **S457** The examples in the category table of concept §6.1 (bracket matching is `SYN`, ambiguous names and `local` imports are `NAM`, short quantity and leftover parts are `LIN`, undercooking and allergens are `SAF`, missing equipment and too few vessels are `KIT`, vessel capacity exceeded and more people than can stand are `SCH`, out of fuel is `MAC`, mixed versions and dependency cycles are `PKG`, the unlimited-equipment assumption is `ASM`) match the category of that diagnostic.
- **S464** A `check` diagnostic has the code written in its declaration. `std` codes do not overlap the C registry, and the codes of an author's package are in 900 to 999 and do not overlap within the package (D32). (added)

Verification levels and assumptions (concept §5.1):

- **S270** The verification level is decided by the inputs given (0 for a recipe only, 1 when servings or cooks are added, 2 when a kitchen is added).
- **S271** `check` stops at the type check and does not build a schedule.
- **S272** The list of assumptions is included in the result, and assumptions are returned in the same form as "information" diagnostics (category `ASM`).
- **S273** Under the same settings, adding inputs never reduces the diagnostics (level 0 diagnostics always appear at levels 1 and 2 too).
- **S274** The level 0 output is the IR and the shortest time needed (the critical path).
- **S275** When there is no kitchen (levels 0 and 1), the first candidate of a `variant` is used, and the other candidates are reported as assumptions (concept §4.2).
- **S276** When small tools (cooking chopsticks, bowls and so on) are left out of the kitchen, "there are enough of them" is reported as an assumption (concept §12.3).
- **S277** A schedule that uses provisional values (D7) shows them in the list of assumptions (concept §12.3).

#### 4.21.1 E's diagnostics, severity table and fuel (decided in P8-0; all (added))

- Names and default severities of E's diagnostics (registry codes are appended in this order by each P8 task that writes golden files; entries without a severity are errors)
  - `NAM`: `arity` (an argument without a default is missing), `unknownArgument`, `duplicateArgument`, `partTraitClash`, `undefinedInMacro`, `unknownUnit`, `unknownDiagnostic` (an unknown diagnostic name in `@allow` and so on), `expectUnfulfilled` (warning; the diagnostic of `@expect` did not appear)
  - `TYP`: `dimension`, `densityMissing`, `vagueAmount` (warning; the `strict` group), `unitCycle`, `baseUnitChanged`, `type`, `stateRequired`, `stateRegress` (moving an ordered axis back), `cutCoarser`, `axisValue` (a value not in the axis or enum), `axisDefinition` (unreachable value, conflicting transitions), `partless` (a part on an ingredient without parts), `whereUnsatisfied`, `aliasCandidates` (only some of the union's candidates meet the requirement), `rangeViolation`, `count`, `annotationTarget`, `spliceNotName`, `vesselLiquid` (a vessel that cannot hold liquid), `outputMismatch` (the recipe's output type differs from the type of the result), `optionLevel` (warning; an option that does not fit the level)
  - `LIN`: `doubleUse`, `insufficient`, `leftoverPart` (information), `parallelConsume`
  - `SAF`: `undercooked`, `allergen`, `fixedSeverity` (information; `SAF` cannot be lowered)
  - `KIT`: `noVariant`, `requiresUnmet`, `vesselShortage`
  - `SCH`: `vesselOverflow` (warning)
  - `ASM` (all information): `unlimitedEquipment`, `defaultServings`, `firstVariant`, `smallTools`, `provisionalValue`, `evenChoice`, `noPlating`, `externalIngredient`
  - `MAC`: `overflow`, `divisionByZero`
  - Diagnostics of the `std` `check` (D32): `SCH101 tooManyCooks` (warning; the `Options` type)
- The 17 examples of the "severity criteria" and the examples of the category table in concept §6.1 are kept in `core/tests/severity-examples.txt`, and `scripts/check-severity-table.py` checks them against the registry and the `std` `check`. Examples decided by F's scheduling (using a tool unwashed after raw meat `SAF:unwashedHazard`, a soft ingredient before a hard one `SCH:softBeforeHard`, an inserted wash `SCH:washInserted`, a vessel used as a substitute `KIT:vesselSubstituted`, the shortest time `SCH:criticalPath`) have their phase set to F and are not counted with `--phase E` at E's gate
- `ozen debug steps [FILE]`: evaluates the recipes and menus of the user's modules in definition order. After a `recipe <名前>` line (`menu <名前>` for a menu), it prints ingredients as `  <名前> : <型> <量>` and action calls as `  %<番号> = <動作>(<引数>) : <型> <量>` (for an action without output, `: …` is left out; numbers start at 1 per recipe), and at the end prints the same diagnostics and result line as `ozen check`. Arguments are: the name for an ingredient, `%<番号>` for the result of a step, the value itself for a quantity or an enum value, and `名前: 値` for a named argument. A type is the name, with the part and the axes changed from the initial state attached in `<…>` (a set axis as `prep has washed`, a value with arguments as `cut(slice, 3 mm)`). A quantity is in the base unit of its dimension and kept as a rational (a decimal if the denominator has only 2 and 5 as factors, otherwise `a/b`), and a range is `lo..hi`. Passing a list `[a, b]` to a single-ingredient argument first prints a `combine` step (D11)
- Traits of mixed values (added): a mixed value inherits `Hazard` and `Allergen(...)` from what is in it, and if everything in it is `RawEdible`, it also inherits `RawEdible` (things eaten raw together can be served raw; added in P13-4)
- Quantity conversion (added): the quantity of a mixed value (`combine`) is the sum of all quantities converted to mass (volume by `density`, count by `unitWeight`). When an ingredient cannot be converted, the quantities are added in their dimension if they all have the same dimension, and otherwise it is `TYP:densityMissing`. In strict mode (`--strict`), the `strict` group (only `vagueAmount` until P8-8) becomes an error
- Parts, aliases and butchering actions (added, P8-4): parts are the items of an ingredient's `parts` (a mark if empty, a section if not) and the items of `parts` on the ingredient's traits. A part's `extends: 親` is inheritance, `traits:` are its properties, `discard: true` marks a discarded part, and `props` are the part's attributes (if missing, the parent part's, then the ingredient's). If the name in `型<名前>` is a part, that part is the candidate; if it is a trait, every part with that trait is a candidate. On an ingredient without parts it is `TYP:partless` (arguments are the type and the name), and an unknown name on an ingredient with parts is `NAM:undefined`. Requirements on a candidate value (`alias A = I<a | B>`, `alias A = I | J`, `I<トレイト>`) are judged on all candidates (only `Hazard` needs just one), and attributes are the union of the candidates' ranges (the widest range). When a requirement holds for only some of the candidates, it is `TYP:aliasCandidates` (arguments are the action, the argument name, the expression and the first candidate for which it does not hold). An alias value uses the alias's name in display and diagnostics, and a requirement by type name passes if all candidates are of that type. If an argument's type name is a trait, values with that trait pass
- `contains` (added, P8-4): a value of `ingredient X : Mixture { contains: { A: 割合, … } }` has the `Hazard` and `Allergen(…)` of what is in it (the ` with …` at the end of a `debug steps` line), and its attributes are the widest range possible within the ranges of the ratios (the lower bound starts from the lower bounds of the ratios and hands out the rest from the smallest values up to their upper bounds; the upper bound hands out from the largest). If the sum of the lower bounds of the ratios exceeds 100 % or the sum of the upper bounds does not reach 100 %, there are no attributes
- Butchering actions (added, P8-4): an action whose output is a tuple type `(T<部位>, …)` makes a value per part from the value of the argument of the type variable. If a part has a `yield` (ratio), its quantity is input × yield. Parts without a yield share what is left after subtracting the sum of the parts with a yield from the input (if there is one such part, it gets the rest; if two or more, each gets 0 to the rest). `debug steps` prints `%n = 動作(引数) : (型 量, …)`, and each value is referred to by `%n.1`…. Parts with `discard: true` are not shown in the leftover information (the discarding step in the schedule is F's)
- Number of arguments (added, P8-4): a missing argument without a default is `NAM:arity` (arguments are the action and the argument name, the location is the call), too many positional arguments is `NAM:arity` (the argument is the action, the location is the extra argument), an unknown named argument is `NAM:unknownArgument`, and the same argument twice is `NAM:duplicateArgument` (arguments are the action and the argument name, the location is the named argument). The second value is not used. A value that does not meet `where T: トレイト` is `TYP:whereUnsatisfied` (arguments are the action, the argument name, the expression and the trait)
- Checks at definition time (added, P8-4): for actions in the user's files and `extend action`, `uses … for 時間` checks the time dimension on every candidate (`TYP:dimension`, arguments are the actual dimension and `Time`). If a part's name resolves as a trait, it is `NAM:partTraitClash` (the location is the part's name)
- `ozen debug requires [FILE]` (added, P8-4): for each dish in the user's modules, after a `recipe <名前>` line, prints a `  requires A, B` line with the `uses` of the actions used (excluding `resource`) and the recipe's `requires`, without duplicates and in string order (not printed if empty), and for each action with a `variant`, in call order, a `  variants <動作>: <候補> | …` line (the candidates of the original definition, then the candidates of `extend action` in order of package name), and at the end prints the same diagnostics and result line as `ozen check`. `debug steps` does not print requirement lines
- Kitchen and option types (added, P8-5): the `std` prelude has `enum Cleanup`, `enum Preference`, `type Kitchen` (the `places` defaults `floor`, `counter`, `sink`, and `tools`, `tableware`), `type Options` (`servings`, `cooks: Count in 1.. = 1`, `cleanup`, `serveAt`, `prefer` and `check SCH101 tooManyCooks: cooks <= kitchen.floor.capacity else warn`) and the types of equipment items (`count: Count in 0.. = 1` of `EquipmentSpec`, and `FloorSpec`, `CounterSpec`, `StoveSpec`, `OvenSpec`, `ContainerSpec`, `FryingPanSpec`, `RiceCookingSpec`, `VesselSpec`) (the conversion script appends them after the vocabulary). The attribute type of a kitchen item `名前: 種類 { 属性 }` is the type named by adding `Spec` to the kind, to the kinds above it, or to the trait names (the first one found; if none, `VesselSpec` for a vessel and `EquipmentSpec` otherwise). The `places` defaults are added by name. A unitless number is accepted only for `Count` items. A dimension mismatch is `TYP:dimension`, a value outside the interval is `TYP:rangeViolation` (arguments are the item name, the value expression and the interval expression; intervals include their ends), and a value not in the enum is `TYP:axisValue` (arguments are the enum and the value). Only the selected kitchen and options are checked. A kitchen or options without a name is `NAM:undefined` (the location is the top of the file). An item not in the type in the attributes of a kitchen item, an options item, or a record value (`Person { … }`) is `NAM:unknownArgument` (arguments are the kind or type name written and the item name, the location is that item; added in P12-1)
- Matching against the kitchen (added, P8-5): a requirement (the types of `uses` and `requires`) is met if, among the kitchen items whose count is not 0, there is an item whose kind name equals the requirement's name, or whose list of kind, kinds above it and trait types contains the requirement's name (the requirement's bare arguments, as in `HeatSource<stovetop>`, must all be among the arguments in that list), and which meets the requirement's state arguments (`Container<capacity >= volume(x) * 1.2>`). The expression of a state argument is evaluated with the action's arguments bound (the built-in `volume(x)` converts a quantity to volume), and if the item has no value, it is taken as met. `resource` (`hands`) is not matched. With a kitchen, an unmet requirement of an action without `variant` and an unmet `requires` of a recipe are `KIT:requiresUnmet` (the argument is the requirement's expression, the location is the call or the `requires` type), and if every `variant` is removed it is `KIT:noVariant` (arguments are the action and the candidate names joined with `, `, the location is the call). The `variants` line of `debug requires` shows only candidates usable in the kitchen. A type's `check` that refers to the kitchen is evaluated only when there is a kitchen, and if it does not hold, a diagnostic with the `check`'s code (severity is the `warn`, `info` or `error` of `else`; error if none) is put at the location of the name of the options declaration
- Dish arguments and vessels (added, P8-5): a dish's arguments are bound to their defaults (or to the value of an options item with the same name, if any) and checked against the argument types (`servings` is used for the vessel check). For the items of `plate` in `serve`, plating a liquid (something with `Liquid`, or a value mixed with a liquid) in a vessel without `HoldsLiquid` gives `TYP:vesselLiquid` for each vessel in `on` (arguments are the item's expression and the vessel). With a kitchen, if the kitchen item of the first vessel in `on` has a capacity, and one portion (the quantity divided by `servings`) exceeds it, it is `SCH:vesselOverflow` (warning; arguments are the vessel, the portion and the capacity, the location is the item)
- Menus (added, P8-6): `make レシピ(引数)` binds the arguments to the recipe's arguments (ingredient values are consumed on the menu side and a copy is passed; types are checked against the argument types), evaluates the recipe body without printing diagnostics or display, and makes the last value the output. The output type is T if there is `-> T` (the new name is not treated as undefined by name resolution), otherwise a name made from the recipe name with its first letter in upper case. The value is a copy of the last value (carrying over state axes, quantity, `Hazard` and `Allergen`). A tuple output returns copies of the last tuple's values as `%n.1`…, and they enter the leftover check as parts. `buy 型 [量]` makes a value of the type. For the breakdown of `choose 名前 from { … }`, if the options have `people` (a list of `Person { picks: [型の名前] }`), each person is counted toward the first candidate whose output type name is in that person's `picks` (or the first candidate if none); otherwise the menu argument `people` (1 if missing) is split evenly among the candidates (the remainder goes to the earlier candidates). Added to the `std` `Options`: `people: [Person] = []`, `type Person { picks: [Dish] = [] }`, and `property whileWarm`, `piping`, `beforeMelts`, `justBeforeServe` (`serveWithin(範囲)`). `debug steps` prints `%n = make r(引数) : 型 量`, `%n = buy 型 量 : 型 量`, `%n = choose 名前 { %a: 人数, … }`, `serve 名前[ after 波 + 間隔]` and `    plate 名前 on 器[ or 器] { 参照, … }`
- Building alone and assumptions (added, P8-6): assumptions (`ASM`) are given only when evaluating the dish selected with `--entry 名前` (the dish built on its own). The ingredient arguments of the selected recipe are bound to newly made values of their types, with `ASM:externalIngredient` (arguments are the argument name and the type expression, the location is the argument name). If there is `@vessel(器 or 器)`, `serve main` and `plate perPerson on 器 { 最後の値 }` are added to `debug steps`; if not, `ASM:noPlating` (the argument is the recipe name, the location is the recipe name). An even `choose` in the selected menu gives `ASM:evenChoice` (arguments are the `choose` name and the number of people, the location is the `choose` expression)
- Compile-time evaluation (added, P8-7): the condition of `if` is a boolean. A comparison of range quantities whose truth is not decided (`15..20 min > 18 min`) is a value decided at cooking time and is `TYP:type` (arguments are `Bool` and `Range`), and a non-boolean value is also `TYP:type` (`Quantity`, `Food`, `String`, `List`). The count of `repeat` is an integer of 0 or more (otherwise `TYP:type`, the argument is `Count`), and `for` takes a list. A `let` inside `if`, `for` or `repeat` does not leak outside. Built-ins are `ceil`, `floor`, `min`, `max` and `len`. A dish reads kitchen values with `kitchen.<項目>.<属性>` and `kitchen.has(種類)` (without a kitchen, `has` is true and items are unknown), and `kitchen` is a built-in name. Dividing an ingredient by a quantity of the same dimension gives a count, and dividing by a count gives a 1/n part (shown as `pork/3`); taking it out n times uses up the original ingredient
- Details of fuel counting (added, P8-7): in addition to name resolution (D), each evaluation of a node in the user's files counts 1: block, `let`, `if`, `for`, `repeat`, `ingredients` and its items, `parallel`, `branch`, `serve`, `plate`, annotation, name, quantity, number, string, boolean, list, record, binary operation, unary operation, member access, call (the called name is not counted), `make`, `buy`, `choose`. A splice `$x` counts the expression of the spliced argument. The expressions of `check` on options types are also counted (the `std` `check` is not). The depth is the nesting count of evaluation. For each macro expansion, the number of nodes inside `quote` is added to the generated nodes. The limit diagnostic is also given inside the evaluation of a called dish
- Macros (added, P8-7): a macro call evaluates the statements inside `quote` in a new scope per expansion (the caller's local bindings are not visible), in the module where the macro is defined. `$x` is evaluated in the caller's scope (if the caller is inside an expansion, in that expansion), and `let $x = …` binds the name passed by the caller in the caller's scope (if what is passed is not a name, `TYP:spliceNotName`). Name resolution treats the argument names that correspond to `let $x` in a macro call as bindings of the caller. A mismatch in the number of arguments is `NAM:arity`. A diagnostic inside an expansion has the location inside the expansion and ` via …` listing the call locations from the inside out, and a diagnostic inside a spliced argument has the caller's location and the caller's expansion history. At definition time, a name written without `$` inside `quote` that is none of a `let` in the body, a top-level definition, a built-in, or an enum or axis value is `NAM:undefinedInMacro` (types are checked after expansion)
- The severity mechanism (added, P8-8): a diagnostic's severity is decided by taking the severity at creation (the registry default; for `check`, its `else`), then applying `--strict` (`--deny strict`) and `--allow`, `--warn`, `--deny` in the order written, then the annotation among `@allow`, `@warn`, `@deny`, `@expect` with the narrowest scope containing the diagnostic's location (the span of the annotated declaration or statement). A setting names a diagnostic name, a category prefix (3 letters) or a group name, and an unknown name is `NAM:unknownDiagnostic` (for an annotation, at the name's location; for a build-time setting, at the top of the file). A group is `property 名前 = diagnostics(名前, …)`, and an author's `property` with the same name is added to the `std` `strict = diagnostics(vagueAmount)` (no information about overriding is given). `@allow` and `@expect` hide. If `@expect` matches nothing, it is `NAM:expectUnfulfilled` (warning, the location is the annotation). A `SAF` error stays an error whatever is done, and an attempt to lower it with an annotation gives `SAF:fixedSeverity` (information) at the annotation's location. Diagnostics in files of dependency packages are hidden, except `SAF` errors
- Annotation targets and deprecation (added, P8-8): `@unattended` on a statement is `TYP:annotationTarget` (the location is the annotation) unless the outermost expression of the statement is an action call, a macro call or `make`. Severity annotations only change a scope and do not care what they are attached to. With `@deprecated` on a `std` definition, using it as a quantity unit in an author's file gives `PKG:deprecated` (warning, the argument is the unit). The `std` `cc` (1 ml) was deprecated
- Fix suggestions and `check` codes (added, P8-8): `TYP:stateRequired` carries a suggestion `動作(式)` when the only argument without a default is a single ingredient: the first action (in the `std` prelude, then in the author's modules) that accepts that ingredient's requirements and whose output effect meets the requirement. In text output, ` fixes=["…"]` is printed after the arguments and the expansion history only with `--fixes` (JSON is in G). For the codes of `check` on an author's type, a duplicate with `std` (the C registry) or within the package is `PKG:checkCodeDuplicate`, and a code not of the form `XXX9nn` is `PKG:checkCodeRange` (the location is the `check`). A diagnostic of an author's `check` is given with its code (`パッケージ名:コード` if there is a root package) and the `check`'s name
- Verification levels and assumptions (added, P8-9): the level is 2 if a kitchen is selected, 1 if the options have `servings` or `cooks`, and 0 if neither. The dish selected with `--entry` gives `ASM:unlimitedEquipment` below level 2 (the location is the dish name), `ASM:defaultServings` if the argument `servings` is not overridden by the options (the argument is the default's expression), and, without a kitchen, `ASM:firstVariant` for each call of an action with a `variant` (arguments are the action, the first candidate and the other candidates joined with `, `, the location is the call; added candidates are listed too). The annotation `@needs(kitchen)` on an item of the `std` type `Options` marks an item that assumes a kitchen (currently `cleanup`), and writing it without a kitchen is `TYP:optionLevel` (warning, the argument is the item name, the location is the value). `check` stops at the type check and does not build a schedule. For monotonicity, `tests/props/monotonic.py` adds options and a full kitchen to each single-file `check` golden and confirms that the diagnostics, excluding assumptions, satisfy level 0 ⊆ 1 ⊆ 2
- Counting fuel (evaluation steps): 1 each time name resolution looks up a name (same as D), and 1 each time evaluation evaluates one of the following nodes: block, `let`, `if` (`else if` is an `if` on the else side), `for`, `repeat`, call (action, recipe, built-in; arguments from the left), macro call (the arguments are not evaluated; the expanded block is), name, quantity (`1 h 30 min` is also one), number, string, boolean, list, record, binary operation (the right side too if not short-circuited), unary operation, member access, annotation, `make`, `buy`, `choose`, `assemble`, `ingredients` (plus 1 per item), `parallel`, `branch`, `serve`, `plate`. Generated nodes are D's count plus the number of nodes copied by macro expansion. The depth is the nesting count of evaluation (the same limit as D's resolution depth)

### 4.22 F: IR and scheduling (§13-6)

IR:

- **S278** The IR holds the DAG of actions (SSA), resource requirements and state transitions as flat tables on an arena with integer IDs (concept §5, §6).
- **S279** Display information (ingredient order, headings, wave names, `@group`) is not mixed into the IR. Changing only display information leaves the text IR the same (concept §6).
- **S280** Every `variant` goes into the IR as a "choose one of these" branch, and the IR is the same regardless of the kitchen (concept §4.2).
- **S281** In a program that uses no compile-time values referring to the kitchen or cooks, changing only cooks and the kitchen leaves the text IR the same and changes only the schedule (concept §4.2, §16, D31).
- **S282** Changing servings changes the quantities in the IR and the count of `repeat` (concept §4.2).
- **S283** Waves go into the IR, and wave names are kept on the metadata side (concept §4.5).
- **S284** Washes are inserted at the scheduling stage and are not put into the IR (concept §4.2).
- **S285** The text IR dump has the form of the example in concept §6 (`%n = 動作 引数 : 入力の型 -> 出力の型 [資源 時間]`), and the details are added in P9-0 (added).
- **S286** Numbers in the IR are kept as rationals (concept §6).
- **S463** When the IR changes because of a compile-time value referring to the kitchen or cooks, the value referred to and its location are given as information (D31). (added)

Schedule (scheduler):

- **S287** In a schedule, every task starts after the tasks it depends on have finished, and at no time is the capacity of a resource (people, places, tools, vessels) exceeded (concept §4.2; confirmed with `tools/schedcheck/`).
- **S288** Tasks whose dependencies have finished are assigned to free resources in priority order. The priority follows the rules of D26.
- **S289** Tasks with the same priority go in order of IR node number (concept §4.2).
- **S290** Without `--serve-at`, the schedule is packed forward (start as early as possible and serve early); with it, the schedule is packed backward from the serving time (start as late as possible) (concept §4.2).
- **S291** The order of goals is: (1) keep the allowed serving window and the wave constraints (required), (2) shorten the total time (with `--serve-at`, make the start time later), (3) what is given in `--prefer` (in the order given) (concept §4.2).
- **S292** `fewerWashes`, `lessWaiting`, `fewerTools` and `steadyPace` of `--prefer` reduce, respectively, the number of washes, the cooks' idle time, the number of tools used, and the time spent with several tasks going at once, without changing (1) and (2) (concept §4.2).
- **S293** A finished schedule is improved by repacking it backward and forward (up to the iteration limit of D26; deterministic) (concept §4.2).
- **S294** `@unattended` tasks do not use hands (concept §4.2).
- **S295** When one unit has several capabilities, the whole unit is occupied while it is used for any one of them (concept §4.2).
- **S296** For a `variant`, the candidate that finishes earliest at assignment time is chosen, and ties go by candidate order (concept §4.2).
- **S297** The chosen `variant` is given as information (the material for "There is no rice cooker, so the rice is cooked in a pot": the action, the chosen candidate and the reason) (concept §4.2).
- **S298** A kitchen calibration (`calibration { oven: 1.1 }`) is multiplied into the time needed for that cooking method (concept §4.2).
- **S299** Heating time is "(thickness ÷ reference thickness)ⁿ × reference time + size-independent time", and if n is a range, the time is a range too (concept §4.1).
- **S300** Putting a soft ingredient into the pot before a hard one gives a warning (concept §4.1).
- **S301** When exceeding a resource's capacity cannot be avoided (there are fewer pieces of equipment than required), it is an error (concept §4.2).
- **S302** When `cooks` exceeds `floor.capacity`, the warning `tooManyCooks` is given (concept §4.2).
- **S303** Per-person attributes (whether they can use a knife, and so on) can restrict the tasks assigned to that person (concept §4.2).
- **S304** Range durations are handled as in D27 (assignment uses the upper bound; the output has the upper and lower ends).
- **S433** `parallel { branch … }` is fork-join: inside each branch tasks go in order, and the tasks of the next statement do not start until all branches have finished (concept §4.2).
- **S434** A new action made by combining actions (level 2 composition) is expanded into the DAG with its arguments applied when it is used, and the expanded tasks enter the schedule (concept §4.3).
- **S436** A schedule at verification level 1 is built with people only, treating equipment as unlimited, and "equipment is unlimited" is reported as an assumption (concept §5.1).
- **S437** `store` and `discard` appear in the schedule table as tasks (concept §4.1).
- **S438** The "thickness" in the heating time formula is decided by the cut (the thickness for slices, the side for cubes, the rough size for rolling cuts and rough chops) (concept §4.1).
- **S439** The heating times for microwaves and for ingredients with irregular shapes (leafy vegetables) are held directly as attributes, without the formula (concept §4.1).

Washing up and cleanup:

- **S305** Tools, vessels and places have the states `clean` and `dirty(汚れの種類)`, and the dirt is decided by the ingredient's `leaves` and the action's `leaves` (concept §4.2).
- **S306** The washing needed before the next use is decided by the compatibility rules in `std` (concept §12.3).
- **S307** If there is no compatible clean tool, whether to wait for another tool or insert a wash is chosen by the rules of D26 (concept §4.2).
- **S308** An inserted wash or wipe carries the tool and the reason, the previous dirt (the material for "cutting board 1, after raw meat") (concept §4.2).
- **S309** A wash uses "sink + hands + the tool's wash time", and soaking is a task that does not use hands (concept §4.2).
- **S310** The compatibility rules after `Hazard` and `Allergen` cannot be relaxed by settings (concept §12.3).
- **S311** A tool can be used without washing when the same ingredient in the same state is handled in a row (concept §12.3).
- **S312** A washed tool can be used right away (drying is not handled) (concept §4.2).
- **S440** Compatibility rules other than those after `Hazard` and `Allergen` can be overridden by a pantry (concept §12.3).
- **S441** `Staining` does not block later use; it is only given as information (concept §12.3).
- **S442** With one cutting board, and tasks to cut raw vegetables and raw meat with no dependency between them, the schedule cuts the vegetables first (one wash fewer, concept §4.2). (added)
- **S313** `--cleanup none` does only the washes needed during cooking, `beforeMeal` cleans the tools and places finished with by serving time (not tableware; per wave), and `afterMeal` cleans everything, including tableware, after the last meal (concept §4.2).
- **S314** Wiping places (counter, stove) is included in everything except `none` (concept §4.2).
- **S315** Putting things kept with `store` into the fridge is part of cleanup (concept §4.2).
- **S316** During a meal hands are busy, and only tasks that do not use hands progress during a meal (concept §4.2, §4.5).
- **S317** With `afterMeal`, if the dishwasher can run during the meal, the schedule does that (concept §4.2).

Serving timing and plating:

- **S318** A dish with `@serveWithin(t)` is served within t after it is done (concept §4.5).
- **S319** A dish with `@keepWarm(on, max)` occupies the equipment used for keeping warm while it waits (concept §4.5).
- **S320** A dish with `@chilled` or `@frozen` uses the fridge or freezer while it waits. The default capacity of fridge and freezer is unlimited, and contention is checked only when it is written (concept §4.5, §12.3).
- **S321** `@justBeforeServe` is an alias of `@serveWithin(2 min)` (concept §4.5).
- **S322** A dish with nothing written has no constraint, and one that waits more than 30 minutes between being done and being served gives information (concept §4.5).
- **S323** Plating uses hands and takes time, and is included in the backward calculation from the serving time (concept §4.2).
- **S324** A plated vessel is in use until the meal of its wave ends (concept §4.2).
- **S325** Tasks that cut the same ingredient in the same way can be merged without any notation (concept §4.5).
- **S326** The time between waves is meal time, and the length of the meal is taken from the gap (concept §4.5).
- **S435** Plating `shared` (shared serving) puts the dish on one large plate, and the total plated quantity equals the dish's quantity (plating too little or too much is a `LIN` diagnostic, concept §4.2).

#### 4.22.1 Formats and checks for the IR and schedule (decided in P9-0; all (added))

- Unit of IR: one IR per dish selected with `--entry` (if none, per dish and menu of the user's modules, in definition order). A menu's `make` expands the recipe's tasks into the menu's IR. IR nodes are of 6 kinds: ingredient `in` (made when the ingredient first becomes the input of a task; the order of ingredient declarations is display information and does not affect the IR), bought item `buy`, task `op` (one action call; `store`, `discard`, `split` and `combine` are tasks too), candidate branch `choose` (an action with `variant`), plating `plate`, and wave `wave`. They are numbered in the order made (from `%1`). Values are task outputs, and a task holds the numbers of its input values (SSA). A tuple output is `%n.k`, and the part of an ingredient divided by a count is `%n/k`
- Task resources: the requirement listing the types of `uses` (`hands` is a person) and the duration (the range of `for`; if none, the value of the argument `time`; if neither, 0 s; the heating time formula (S299) is P9-4), and `@unattended` (on `uses` or on the action). Each `variant` candidate has its own resources and duration
- `ozen debug ir [FILE]` (takes `--kitchen`, `--options`, `--entry`): for each dish, after an `ir <名前>` line, prints one node per line.
  - `  %n = in <名前> : <型> <量>`, `  %n = buy <型> : <型> <量>`
  - `  %n = op <動作> <入力>, …[ <名前>=<値> …] : <入力の型>, … -> <出力の型> <量> [<要求>, …; <時間>[; unattended]]` (inputs are the ingredient arguments in declaration order; `<名前>=<値>` lists all non-ingredient arguments, defaults included, in declaration order; types are written as in `debug steps`, without quantities. Without requirements it is `[<時間>]`, without output ` -> …` is left out, and a tuple output is `(<型> <量>, …)`)
  - `  %n = choose <動作> <入力>, …[ <名前>=<値> …] : <入力の型>, … -> <出力の型> <量> { <候補> [<要求>, …; <時間>[; unattended]] | … }` (candidate order is the order of §4.21.1; candidates not usable in the kitchen are included too)
  - `  %n = plate <名前> <入力>, … on <器>[ or <器>] [<要求>, …; <時間>]` (resources and time come from the `std` action `plate`)
  - `  wave <番号>[ after <番号> + <間隔>]` (a wave's name is display information, so only its number is printed in the IR. It is printed at the position of `serve`, followed by that wave's `plate`s)
  - At the end, the same diagnostics and result line as `ozen check`. With `--entry`, only the IR of that dish is printed
- Information on kitchen values (P9-1): each time a dish reads a kitchen value (`kitchen.<項目>.<属性>`, `kitchen.has(…)`), `SCH:kitchenDependent` is given (information; the argument is the expression read, the location is that expression) (D31; given even without a kitchen)
- IR tables (P9-1): the IR is flat tables of integers only (nodes, inputs, requirements, candidates). A node has its number, dish, label, body (the rest of the text IR line), the ranges of its inputs, requirements and candidates, its wave, the range of its duration, and whether it is unattended. Structs are arrays without padding, and `cunit/ir_flat_ids` confirms that IRs built in separate contexts match byte for byte
- `ozen debug schedule [FILE]` (takes `--kitchen`, `--options`, `--entry`): after a `schedule <名前> level=<レベル> cooks=<人数>` line, prints one task per line in order of start time (ties in order of IR number).
  - `  <開始>..<上限の終わり> (<下限の終わり>) <作業> by <人>|unattended [<資源の個体>, …]` (times are rationals in seconds from the start. A task is `%n <動作>`, an inserted wash is `wash <道具> after <汚れ>`, a wipe is `wipe <場所> after <汚れ>`, a chosen candidate is `%n <動作>/<候補>`. A resource unit is the kitchen item's name and number, `board#2`; at level 1, only people)
  - `  serve <波> at <時刻>`, and at the end `total <上限の終わり>`, then the diagnostics and the result line
- Schedule JSON (the input of `schedcheck`; becomes part of G's JSON): `{"schemaVersion":1,"cooks":n,"resources":[{"id","kind","capacity"}],"tasks":[{"id","op","deps":[…],"start","endHi","endLo","cook"|null,"uses":[資源の id…],"unattended","wave"|null}],"waves":[{"id","serveAt","after"|null,"gap"}],"dishes":[{"id","serveWithin"|null,"ready"}]}`. Times are `{num, den}`
- Checks of `tools/schedcheck/` (a path separate from the implementation, reading only the schedule JSON): (1) dependencies: every task starts at or after the upper end of the tasks it depends on, (2) resources: at every time, for each resource, the number of tasks using it is at most its capacity (intervals are `[開始, 上限の終わり)`), (3) people: tasks that use a person do not overlap per person, the number of people is at most `cooks`, and `unattended` tasks use no person, (4) one unit with several capabilities: no two tasks use the same unit at the same time, (5) waves: a wave's plating finishes by that wave's serving time, and from the second wave on, the serving time is at or after the previous wave's serving time + the gap, (6) serving timing: for a dish with `serveWithin`, the time from being done (by the upper end) to serving is at most the window (D34). However, a `missed` dish may exceed the window, and being within the window while `missed` is a violation, (7) washing: for a tool that needs washing because of dirt compatibility, there is a wash task for that tool before its next use, (8) cleanup: at the end, tools and places are clean as `--cleanup` specifies. (9) meals: from the second wave on, no task using a person falls within "previous wave's serving time + gap" (during the meal) (added in P13-5). The check prints `schedcheck: <段取りの数> schedules, <違反の数> violations`. For the washing and cleanup checks, tasks have `leaves` (`[{resource, dirt, sameFoodAs}]`) and `cleans` (`{resource, level}`), the schedule has `rules` (the washing needed per dirt) and `cleanup`, and resources have `category` (`tool`, `place`, `vessel`) and, for units with several capabilities, `capabilities` (P9-2)
- Details of dependencies and scheduling (P9-3): a task depends on the tasks that made its input values (data dependencies), plus, inside a branch of `parallel`, the previous task, and, after `parallel`, every task inside the branches (S433). In the IR line these are printed as ` after %a, …` after the inputs and value arguments. The priority is the longest path from that task to the end (counted by upper bounds, including the task's own upper bound). From time 0, tasks whose dependencies have all finished are taken in order of priority, highest first (ties in order of IR number). A task that uses a person (with `hands` in its requirements and not unattended) goes to the free person with the lowest number, and other requirements go to the first free kitchen unit that meets them (there are as many units as the item's `count`, multiplied by `burners` for a stove). A task that cannot be assigned waits until the next time something finishes. The end is the upper bound, and the lower end is start + lower bound (D27). If the options have `serveAt`, a schedule with reversed dependencies is built by the same rules, and the times are turned back by subtracting from `serveAt`. Level 0 counts neither people nor equipment (the shortest time), level 1 counts only people (`cooks`, 1 if missing), and level 2 counts people and kitchen units. For a candidate branch, the first candidate usable in the kitchen (in P9-4 this becomes the candidate that finishes earliest). A task line of `debug schedule` is `<開始>..<上限の終わり> (<下限の終わり>) %n <動作>[/<候補>]`, followed at levels 1 and 2 by ` by cook<番号>` or ` unattended`, and at level 2 by the units used, ` [<項目>#<番号>, …]`. `--format json` prints the schedule JSON on one line (`scripts/collect-schedules.sh` collects the schedules of the golden files and `G-SCHED-CHECK` checks them). In the schedule of an `--entry` dish, an action that used a provisional value (a time with `@source(provisional)`) gives `ASM:provisionalValue` at the location of its first call (the argument is the action)
- Details of equipment, candidates and heating (P9-4): for a candidate branch, the candidates usable in the kitchen (all of them if none) are laid out with their own durations, units and people, and the one that finishes earliest at assignment time is chosen (ties by candidate order). When the first candidate is not chosen, it is `SCH:variantChosen` (information; arguments are the action, the chosen candidate, the first candidate and the reason `unavailable` or `earlier`, the location is the call). An item with several capabilities in one unit is also a single unit (S295). The kitchen's `calibration { <加熱の種類>: <倍率> }` is multiplied into the durations of tasks that require `HeatSource<種類>`. A task needing the same requirement more times than there are kitchen units is `SCH:capacityShortage` (arguments are the requirement, the number needed and the number available, the location is the call), and the schedule is built with the missing requirements removed. `Person { avoids: [<種類>…] }` in the options' `people` keeps tasks that require those kinds away from that person (cook1, cook2… in list order). The duration is taken from `for`, then the action's `takes` expression, then the argument `time`. The built-in `heating(食材, 調理法, 指数)` gives the time of `direct` if the ingredient's `props.heating.<調理法>` has it, and otherwise `heatUp × (厚さ ÷ at)^指数 + soften` (if the exponent is a range, so is the time; non-integer ends are bounded on the safe side by the integers below and above). The thickness is the cut size (for `roughChop`, `rollCut` and `biteSize`, the rough size of the `std` `nominalCutSize`; if not cut, `at`). When a list is put into a pot and a cooked soft ingredient (`hardness: soft`) and a raw hard ingredient (`hardness: hard`) are in the same list, it is `SCH:softBeforeHard` (warning; arguments are the types of the soft and hard ingredients, the location is the call). An action with `steps { … }` (a composed action) binds its arguments and evaluates its steps when called, and the last value is the result (the steps' tasks go into the IR and the schedule). If the result does not meet the output type, it is `TYP:outputMismatch` (arguments are the action and the result type)
- Details of washing and cleanup (P9-5): washes and wipes are handled only at level 2, which counts kitchen units (at levels 0 and 1 tools are unlimited, so a clean tool is always available).
  - Dirt: for each item of an action's (candidate's) `leaves:`, `<汚れ> on <能力>` puts that dirt, and `<引数> on <能力>` puts the dirt of the `leaves` of that argument's ingredient, on the unit the task uses for that capability. `Hazard` dirt is put only while the ingredient's `doneness` is at its initial value (raw). `allergen` becomes the dirt `allergen(k)` for each `Allergen(k)` of the ingredient. The ingredient key is "type name + the value of `doneness`"
  - Compatibility rules: the `rule`s of `std` and of the user's modules are read, and a rule for the same `after` trait is replaced by the one from the later module (the user's) (the same goes for an unnamed `rule after X: …`). If the rule being replaced is `@fixed` (`Hazard` and `Allergen`), it is not replaced, and it is `SAF:allergen` for `Allergen` and `SAF:unwashedHazard` otherwise (error; the argument is the trait, the location is the user's rule). When task T uses a unit after the dirt that an earlier task P put on it, the washing needed is decided by each dirt trait's rule: the first of the cases with a target that applies to T; if none, the case without a target (including `any use`); if none of those, `none`; and the maximum over traits and dirt. The target `sameFoodSameState` means P and T have the same ingredient key, `sameTool` means T also puts the same dirt on the same unit, `allergicPerson` means T is for the portion of a person allergic to the dirt's allergen k, and any other name means T's input ingredient has the trait of that name (including the name with its first letter in upper case). The dirt of every task that used the unit after its last wash remains, and a wash (`rinse` or stronger) makes the unit clean
  - Inserting washes: when assigning a task to a unit, the scheduler estimates for each unit "the time it becomes free (if a wash is needed, the later of that and the time a person is free, plus the wash time)", and chooses the unit that can start earliest (ties go to the one that needs no wash, then to kitchen declaration order; D26). If the chosen unit needs a wash, the wash task is placed at the time the sink (a unit of `Sink`) and a person are free, and the unit is reserved for the task (the task itself is assigned after the wash). A wash takes "sink + hands + tool" for the tool kind's `washTime`; `rinse` takes the same time; `soakThenWash` is a soak that uses no hands (the tool only, `std` `soakTime`) followed by a wash. The lines are `wash <個体> after <汚れ>`, `rinse <個体> after <汚れ>` and `soak <個体> after <汚れ>` (the dirt is the first one that decided the washing needed). Each time a wash is inserted, `SCH:washInserted` is given at the call location of the task that needed it (information; arguments are the unit and the dirt). At the location of the task that first put `Staining` dirt on a unit, `SCH:stainLeft` is given (information; arguments are the unit and the dirt)
  - Between tasks with the same priority and no dependency between them, if the dirt one task leaves would require a wash before the other task that uses the same capability, and the reverse order would not, the other task goes first (S442)
  - Cleanup (the options' `cleanup`): `none` adds no tasks. `beforeMeal`, after all tasks finish, washes the tools used (excluding tableware) (with the washing of the case without a target; `wash` if weaker than `wash`; `after use` if there is no dirt as the reason), and wipes the places used whose kind has a `wipeTime` (`wipe <個体> after use`, hands + place). Cleanup tasks are placed in the order the units become free (ties in kitchen declaration order), with the earliest free person and sink. The serving time is the end of all tasks. With `serveAt`, cleanup is added after the schedule is built, and the whole schedule is shifted earlier so that cleanup ends at `serveAt`. `afterMeal` cleans up after the serving time (the end of the dish's tasks), including tableware (vessels used for plating; `std` `tablewareWashTime`) (meal time is added in P9-6). `store` tasks are treated as cleanup tasks, and with `afterMeal` they are placed after all the other tasks of the dish have finished. In the schedule JSON, tasks have `kind` (`op`, `wash`, `rinse`, `soak`, `wipe`), `leaves` (`[{resource, dirt}]`), `food`, `targets` and `cleans` (`{resource, level}`), and `rules` has the list of cases per dirt (`{dirt, cases: [{rule, level, when}]}`); `schedcheck` works out by itself the washing needed from the earlier task's dirt and the next task
  - Per-person allergies: `Person { allergies: [<AllergenKind>…] }` in the options. A `make` task that a person chose through `choose perPerson` is that person's portion. If the candidate chosen by a person with an allergy contains that allergen (the ingredient's `Allergen(k)`; for `make`, the `Allergen(k)` of that recipe's ingredients), it is `SAF:allergen` (error; arguments are the allergen and the candidate, the location is the `choose`)
  - Small tools: when a task requires a kind in the `std` `smallTools` (peeler, bowl, colander, cooking chopsticks, ladle, whisk, measuring spoons) and the kitchen does not have it, `ASM:smallToolsAssumed` is given instead of `KIT:requiresUnmet` (information; the argument is the kind, the location is the dish name, once per dish and kind), and the schedule does not count that requirement
- Details of serving timing, vessels and waves (P9-6):
  - Number of people: the number of vessels for `perPerson` is the menu's or recipe's `people` (when an integer); if none, the dish's `servings`, then the options' `servings`; if none of these, 1. `shared` is one plate
  - Vessel assignment (when the kitchen has at least one `tableware`): per wave, in the order `plate`s are written, in the order of the candidates of `on` (a property name of `vesselOrder` is expanded into its list), the kitchen items of that vessel kind (and its subkinds) are looked at in declaration order, and free units are taken in order of number, lowest first. If there are not enough, it is `KIT:vesselShortage` (error; arguments are how `on` was written, the number needed and the number available, the location is the `plate`). If a vessel other than the first candidate is used, `KIT:vesselSubstituted` is given per kind (information; arguments are the vessel kind and the count). A plated vessel is in use until that wave's meal ends and is not given to another `plate` of the same wave (it is free again in the next wave). A `plate` without `on` uses the `@vessel(…)` of the plated dish's recipe (S423; the `on` of `plate` takes priority). Plating tasks in the schedule use the assigned units
  - Waves and meals: the serving time S_k of wave k is the later of the end of wave k's plating tasks and S_{k-1} + gap. Meal k runs from S_k to S_k + the next wave's gap (the last wave's meal is 0). No task using a person is placed during a meal (unattended tasks progress). The schedule is built and rebuilt until the serving times and meals stop changing (at most 8 times). `debug schedule` prints `  serve <波の名前> at <S_k>` after the task lines, in wave order. `serveAt` is used as the last wave's serving time for backward scheduling, and meals between waves are not handled
  - Serving timing: for each plated dish, the task that made it (the finishing task) is looked at. The annotations of `plate` (`@serveWithin(t)`, a named property that calls `serveWithin`, `@keepWarm(on: <能力>, max: t)`, `@chilled`, `@frozen`) are used, or if none, the annotations of the dish's recipe (the defaults of `Dish`). `@chilled` and `@frozen` put on an item inside `plate` or `assemble` (`buy VanillaIce @frozen` in concept §4.5) apply only to that item: an item of `plate` uses the fridge or freezer until the serving time, and an item of `assemble` until assembly starts (added in P13-3). If the wait (S_k − the upper end of the finishing task; D34) exceeds the limit of `serveWithin` (`max` for `keepWarm`), the start of the finishing task is delayed by the excess and the schedule is rebuilt. A dish that still exceeds it after rebuilding gives `SCH:serveWithinMissed` (warning; arguments are the plated dish, the wait in seconds and the limit in seconds, the location is the plated dish), and `"missed": true` is written on the dish (`dishes`) in the schedule JSON (P13-3, D34). `keepWarm` keeps the units used by the finishing task that meet `on` in use until S_k. `@chilled` and `@frozen` use the first unit of `Chilling` or `Freezing` while waiting (the line is `  <仕上がり>..<S_k> hold <盛る料理> [<個体>]`, written the same way for `keepWarm`). The capacity of the fridge and freezer is the `capacity` of the `std` `FridgeSpec` and `FreezerSpec`, and only when it is written, more dishes stored at once than the capacity gives `SCH:capacityShortage`. When the wait of a dish without annotations (counted from the lower end of the finishing task here) exceeds the `std` `longWait` (30 minutes), `SCH:longWait` is given (information; arguments are the plated dish and the wait in seconds, the location is the plated dish)
  - `assemble { … }`: the items are evaluated in the order written and become the IR task `assemble` (inputs in the order written; requirements and time are the same as the `std` `layer`). The result type is `Assembly`
  - Shared cutting tasks: tasks of actions that require `CuttingSurface`, with no task dependencies between them and with the same action, the same input ingredient types and states, and the same named arguments, are merged into the first task (the time of one; the successors of the later tasks depend on the first task). The line is `%a+%b <動作>`
  - Dishwasher: the dishwasher is used only for runs that wash tools, and is not assigned to tasks of a dish that require `Washing` (such as `wash` for food) (added in P13-4). With `afterMeal`, if the kitchen has a `Dishwasher`, tools are not washed by hand but in one run that uses no hands, `run <個体> after use` (the `runTime` of `Dishwasher`). It starts at the latest of the first wave's serving time, the last use of the tools, and the time the dishwasher is free (it may be during a meal). Vessels and places are cleaned by hand after the last meal
- Details of goals, `--prefer` and improvement (P9-7): a schedule is chosen by building candidates in a fixed order and comparing them. The candidates are: (0) the one rebuilt to keep serving timing (P9-6), (1) the one that takes tasks of the same priority in order of IR number, highest first, (2) the one that, among kitchen units free at the same time, takes a unit already used, (3) both (1) and (2), (4) (0) repacked backward and forward (D26's forward-backward improvement: build backward in order of the previous schedule's end times, latest first, then build forward in order of those start times, earliest first; repeat until the total time stops shrinking, at most 8 round trips; not made when there are two or more waves or there are serving timing constraints), (5) the one without the rebuild for serving timing. They are compared by: (1) keeping serving timing (`serveWithin` and the `max` of `keepWarm`), (2) a shorter total time (with `serveAt`, a later start), (3) in the order of the options' `prefer` list, smaller values of: for `fewerWashes`, the number of washes, rinses and soaks inserted during cooking; for `lessWaiting`, the total idle time of each person from the start of their first task to the end of their last; for `fewerTools`, the number of tool units used; for `steadyPace`, the total time during which two or more tasks are in progress at once, (4) a lower candidate number. A level 0 schedule gives `SCH:criticalPath` at the location of the dish name (information; the argument is the total time in seconds)
- The procedure for building the expected schedule of a small example by hand is in §9.6

### 4.23 G: Public API and JSON (§13-7)

- **S327** The public functions are the 3 functions `recipe_build`, `recipe_free_string` and `recipe_default_config`, and every other symbol with external linkage starts with the internal prefix (`ozni_`) (concept §8).
- **S328** The input of `recipe_build` has the form of D25, and if `kitchen` and `options` are NULL, they are not injected. The inputs given decide the verification level (concept §5.1, §8).
- **S329** The caller frees the returned string with `recipe_free_string` (concept §8).
- **S330** The result JSON has `schemaVersion`. The DAG is an array of nodes with references by integer ID, the node kind is `"op"`, quantities are `{value, unit}`, and rationals are `{num, den}` (concept §6).
- **S331** The result JSON matches the JSON Schema in `schemas/` (written in P10-0; added).
- **S332** The diagnostic JSON has the code, name, severity (default and after settings), category, span, arguments, related locations (including the expansion history) and fix suggestions (concept §6.1).
- **S333** The output of `--format sarif` matches the JSON Schema of SARIF 2.1.0, and the diagnostic content maps to ruleId, level (`note` for information), locations, relatedLocations and fixes (concept §6.1).
- **S334** The core outputs structured data that is the material for prose (each step's action, arguments, ingredients, duration, resources and the reasons for inserted washes) and a JSON list of action signatures (concept §7.1).
- **S335** The core outputs the JSON of type definitions (`types.json`: the items of `Kitchen` and `Options`, and the kinds of equipment and vessels that can be put in a kitchen, with the types, ranges and defaults of their attributes) (concept §4.2, D24).
- **S462** The core outputs a JSON list of all definitions of `std` and the author (dimensions, units, axes, traits, ingredients, parts, aliases, actions, capabilities, equipment kinds, vessels, dirt, compatibility rules, fallback orders, properties, diagnostic registrations) (`ozen debug definitions`; a public output from P10-2 on) (D28).
- **S336** The public header `core/include/ozen.h` compiles as both C17 and C23 (`-std=c17 -pedantic-errors`) and uses no C23 features (so that Swift, the NDK and Emscripten can read it; added).
- **S337** The core output does not include diagnostic messages (only codes and arguments) (concept §6).
- **S476** If the `options` text has several `Options` declarations, the value in a later declaration overrides the value in an earlier one, item by item (to combine the CLI's `--options FILE` with its options; added).

#### 4.23.1 The shape of the API and JSON (decided in P10-0; all (added))

- Public header `core/include/ozen.h` (readable as C17 too; does not use `bool`, `nullptr` or `[[…]]`):
  - `typedef struct recipe_source { const char *path; const char *text; size_t len; } recipe_source;`
  - `enum { RECIPE_EMIT_IR = 1, RECIPE_EMIT_SCHEDULE = 2, RECIPE_EMIT_STEPS = 4, RECIPE_EMIT_SIGNATURES = 8, RECIPE_EMIT_DEFINITIONS = 16, RECIPE_EMIT_TYPES = 32 };`
  - `typedef struct recipe_config { const char *entry; unsigned emit; unsigned long max_steps, max_depth, max_nodes, max_input_bytes; void *(*alloc)(void *ctx, size_t size); void (*release)(void *ctx, void *ptr, size_t size); void *alloc_ctx; } recipe_config;` (the core does not call the standard library's allocation (S181), so the caller passes `alloc` and `release`; if they are NULL, `recipe_build` returns NULL. A limit of 0 means the default)
  - `recipe_config recipe_default_config(void);`, `char *recipe_build(const recipe_config *cfg, const recipe_source *sources, size_t n_sources, const char *kitchen, size_t kitchen_len, const char *options, size_t options_len);`, `void recipe_free_string(const recipe_config *cfg, char *s);` (the return value is allocated with the allocation function of `cfg` and freed with the same `cfg`. NULL if allocation fails)
- The `kitchen` text is Ozen text with one `kitchen <名前> { … }` declaration, and the `options` text is Ozen text with one or more `kitchen <名前> : <型> { … }` declarations (the type is `Options`, or an author's type that has it as its type annotation). The core reads each as `<kitchen>` and `<options>` in the file table, and uses the declarations in them for the kitchen and options (no declaration is the error `NAM:unknownKitchen`). Severity settings are passed through `strict: StrictMode = off` (`enum StrictMode { off, on }`), `allow`, `warn` and `deny: [DiagnosticName] = []` of `Options` (strings of diagnostic or group names; `type DiagnosticName {}`)
- Result JSON (`schemas/result.schema.json`): `{"schemaVersion": 1, "ozenVersion", "level": 0|1|2, "entry": 名前|null, "files": [道筋…], "outcome": {"status": "ok"|"error", "errors", "warnings", "infos"}, "diagnostics": [診断…]}`, plus `ir`, `schedules`, `steps`, `signatures`, `definitions` and `types` as selected by `emit`. A rational is `{"num","den"}` (integers; `den` is positive), a range is `{"lo","hi"}`, and a quantity is `{"value": 有理数|範囲, "unit": 単位の名前}`
  - Diagnostic: `{"code", "name", "category", "severity", "defaultSeverity", "span": {"file": 添字, "start": {"line","column"}, "end": {"line","column"}}, "args": [文字列…], "related": [{"kind": "via", "span"}…], "fixes": [文字列…]}` (no message field, S337)
  - `ir`: per dish, `{"name", "nodes": [{"id", "kind": "in"|"buy"|"op"|"choose"|"plate", "label", "inputs": [{"node", "part": 整数|null}…], "after": [id…], "requires": [文字列…], "time": 範囲, "unattended", "wave": 整数|null, "variants": [{"name","requires","time","unattended"}…], "outputs": [数量|null…]}…], "waves": [{"id", "name", "after": 整数|null, "gap": 有理数}…]}` (an output without a quantity is null)
  - `schedules`: per dish, `{"name", "schedule": 段取りの JSON}` (the schedule JSON of §4.22.1, with fields such as `tasks` added)
  - `steps` (material for prose, S334): per dish, `{"name", "steps": [{"start", "end", "kind": "op"|"plate"|"wash"|"rinse"|"soak"|"wipe"|"run"|"hold", "action", "args": [{"name","value"}…], "foods": [文字列…], "resources": [文字列…], "reason": 汚れ|null, "cook": 整数|null}…]}` (in schedule time order; times are rationals)
  - `signatures`: per action that is not `local`, `{"name", "module", "params": [{"name","type","default": 文字列|null}…], "output": 文字列|null, "requires": [文字列…], "variants": [名前…], "provisional"}`
  - `definitions`: the same shape as `ozen debug definitions` (`schemas/definitions.schema.json`). `local` ones are not output (S425)
  - `types` (S335): `{"kitchen": [{"name","type","default"}…], "options": [項目…], "equipment": [{"kind","category": "place"|"tool","fields": [{"name","type","min","max","default"}…]}…], "vessels": [{"kind","fields": […]}…]}`
- The JSON Schemas in `schemas/` are written in a subset of draft 2020-12 (`type`, `properties`, `required`, `additionalProperties: false`, `items`, `enum`, `const`, `$ref`/`$defs`, `oneOf`, `minimum`), and every object is closed. `tools/jsoncheck/validate.py` checks this subset without external libraries (the schema is chosen by the file name ending `.result.json`, `.definitions.json` or `.types.json`)

### 4.24 H: The 4 environments, bridges and CI (§13-8)

- **S338** For iOS, an `.xcframework` combining static libraries for device and simulator is built, and Swift can call it directly through the header and module map (concept §8).
- **S339** For Android, an `.so` per ABI (`arm64-v8a`, `armeabi-v7a`, `x86_64`, `x86`) is built, and Java can call it through a JNI wrapper (concept §8, D29).
- **S340** The JNI bridge passes strings as UTF-8 `ByteArray`s, and Japanese and emoji are not broken (concept §8).
- **S341** For Linux, a shared library (`libozen.so`) and the CLI are built (concept §8).
- **S342** For the Web, a `.wasm` and a JS bridge (a few dozen lines) are built, and they run in node and in a Web Worker (concept §8).
- **S455** The `.xcframework` also has a macOS slice, and Swift can call it on macOS (concept §3, §8 "macOS is supported in the same line").
- **S343** The JSON string returned by calling `recipe_build` from Swift (Linux, macOS, iOS simulator), Java (the Linux JVM and the Android emulator) and JS (node and a Chrome Web Worker) does not differ by a single byte from the result of passing the same input natively.
- **S344** CI runs the builds and tests of the 4 environments and WASM, ASan and UBSan, and fuzzing on every push (concept §10, D19).

#### 4.24.1 Details of artifacts, bridges and CI (decided in P11-0; all (added))

- Artifact names:
  - Linux: `build/core-<構成>/libozen.so` (the only dynamic symbols are the 3 symbols `recipe_build`, `recipe_default_config` and `recipe_free_string`; the others are hidden by the linker version script `core/ozen.map`) and the CLI `build/core-<構成>/ozen`
  - iOS and macOS: `Ozen.xcframework` (slices `ios-arm64`, `ios-arm64-simulator`, `macos-arm64_x86_64`; contents are the static library `libozen.a`, `core/include/ozen.h` and the module map). CI artifact `xcframework` (`Ozen.xcframework.zip`)
  - Android: `libozen.so` per ABI and the JNI `libozen_jni.so` (`arm64-v8a`, `armeabi-v7a`, `x86_64`, `x86`). CI artifact `android-libs`
  - WASM: `build/core-wasm/ozen.js` and `ozen.wasm` (a CLI that runs in node; runs the golden files) and `build/core-wasm/ozen_bridge.js` and `ozen_bridge.wasm` (a module that exports `recipe_build` and the others; used from a Web Worker and node)
- Bridge APIs (each calls `recipe_build` once and returns the returned string as is):
  - Swift: the module map module `COzen` (`bindings/swift/module.modulemap`) and `public enum Ozen { public static func build(sources: [(path: String, text: String)], kitchen: String?, options: String?, entry: String?, emit: UInt32) -> String? }` in `bindings/swift/Ozen.swift`
  - Java: `public static native byte[] buildUtf8(byte[][] paths, byte[][] texts, byte[] kitchen, byte[] options, byte[] entry, int emit)` in `bindings/android/java/ozen/Ozen.java`, and `public static String build(String[] paths, String[] texts, String kitchen, String options, String entry, int emit)`, which converts with UTF-8. The JNI wrapper is `bindings/android/jni/ozen_jni.c`. `javac --release 17` (D29; the class file version that Android's d8 accepts)
  - JS: `build(module, {sources: [{path, text}], kitchen, options, entry, emit})` in `bindings/wasm/ozen.mjs`. Strings use UTF-8 via `TextEncoder` and `TextDecoder`
- Bridge agreement (S343): for each file in the input list `bindings/inputs.txt` (part of the golden files, and `bindings/inputs/utf8.ozen`, which has strings with Japanese and emoji), the result string of a call with the path as the file name only, no kitchen or options, no entry, and `emit` of `ir|schedule|steps|definitions` is compared byte for byte with the output of the native `ozen check --format json --emit ir,schedule,steps,definitions <ファイル名>` (run in the file's folder). The output is `<道具>: <入力の数> inputs, <違いの数> mismatches`
- CI (`.github/workflows/ci.yml`; the list of job names is `ci/jobs.txt`): `linux` (builds of the 4 configurations, golden files, unit tests, allocation-failure injection, CLI tests, the Swift and JNI bridges), `fuzz` (60 seconds per target), `wasm` (node golden files, Web Worker, digest), `ios` (xcframework, Swift on the iOS simulator and macOS, golden files and digest on the simulator), `android` (the 4 ABIs with NDK r30, golden files and digest on the emulator, JNI). Every job writes its tool versions to `tools.txt` and makes it the artifact `tools-<ジョブ>`

### 4.25 G: The CLI `ozen` (concept §5.2)

- **S345** The subcommands are the 5 subcommands `check`, `build`, `render`, `bundle` and `unbundle`, plus `debug` for development (prints the AST, the grammar rules used, the list of definitions, and the embedded `std`).
- **S346** An entry can be given in 4 ways: a module name, a module name + an entry name, a file path, and `--entry`. It is resolved as a module from the front, and the rest is read as the entry name (concept §5.2).
- **S347** The form `file:name` is not accepted (concept §5.2).
- **S348** If there are several entries and none is specified, it is an error listing the candidates (concept §5.2).
- **S349** `-k home` searches the project's kitchen declarations by name, also accepts a file path, and if the same name is found more than once, it is an ambiguity error (concept §5.2).
- **S350** There is no default mechanism for the kitchen and the like; leaving it out checks at the level without a kitchen (concept §5.2).
- **S351** The only short forms are the 4 forms `-k`, `-c`, `-s` and `-t`; everything else has only the long form. Abbreviating a long form partway is not accepted (concept §5.2).
- **S352** A project build treats every entry in the given scope as follows: a menu as is, a dish as a menu of one dish, and a definitions-only module is only checked. Without an entry, the whole project (concept §5.2).
- **S353** Each entry is built independently, in series in order of module name. If one fails, the rest continue. At the end "how many of how many passed" is printed, and if there is even one error, the exit code is failure (concept §5.2).
- **S354** The output is `schedule.json`, `ir.txt` and `diagnostics.json` (SARIF too with `--format sarif`) in `out/<モジュールの道筋>/`, and the list of all entries and their results in `out/index.json` (concept §5.2).
- **S355** `--servings`, `--cooks`, `--serve-at`, `--cleanup`, `--prefer`, `--strict`, `--allow`, `--warn` and `--deny` are converted into the Ozen text of `options`, and `--options FILE` (a file of options, including per-person attributes, written in Ozen syntax) is combined with it. `--kitchen` goes to the `kitchen` argument, and `--entry`, `--emit` and `--format` go to `cfg` (D25, concept §4.2, §5.2, §6.1).
- **S356** The diagnostics the CLI prints are the code, name, location and arguments, plus an English display made mechanically from the identifier (D21).
- **S443** Building again with the same input leaves the contents of `out/` the same down to the byte, and building after removing an entry removes that entry's output from `out/` and `index.json`. (added)

#### 4.25.1 Details of the CLI (decided in P10-0; all (added))

- The output of `--format json` and `--format sarif` and of `build` is made through the core's public API (`recipe_build`). The text display (the default `--format text`) and `debug` use the core's internal text output so that they print the same lines as the golden files. `check` and `build` pass all the `.ozen` files of the project (the folder with `package.ozen`; if none, just the one file given)
- A diagnostic is displayed (S356) on one line as `<重さ> <コード> <名前> <道筋>:<行>:<列>-<行>:<列>[ args=[…]][ via …][ fixes=[…]]: <英語の言葉>` (the English words are the camel-case name split into words in lower case, D21). At the end, `result <ok|error> errors=<n> warnings=<n> infos=<n>`
- Converting options (S355): `--servings N` (`-s`), `--cooks N` (`-c`), `--serve-at T` (`-t`), `--cleanup X`, `--prefer A,B`, `--strict`, `--allow NAME`, `--warn NAME` and `--deny NAME` are written into `kitchen cli : Options { … }`, appended after the contents of `--options FILE` (if `FILE` does not exist, the text of the options declaration with that name), and passed as `options` (by S476, the later one wins). When there is only `--options NAME` and no option for the options, the name is passed as is to keep the declaration's location. `ozen debug options [オプション]` prints the `options` text it made
- `-k NAME|PATH` (S349): if `PATH` is an existing file, the kitchen declaration (one) in it is used. Otherwise, `kitchen NAME { … }` (one that is not `Options`) is searched for in the project's `.ozen` files. If there are 0, `cli: kitchen not found: NAME`; if 2 or more, `cli: kitchen is ambiguous: NAME (<道筋>, <道筋>)` (exit code 2)
- Entries (S346 to S348): if a positional argument is an existing file, that file's module is used; otherwise it is resolved as a module from the front of its `.`-separated parts (the part before the first that is a module is the module), and the rest is the entry name. If it is neither a module nor the part before one, every module under the folder of that name (`build menus`). `--entry` names a further name. Entries (menus and dishes) are collected from `recipe <名前>` and `menu <名前>` at the start of lines in each file. An argument containing `:` gives `cli: use a module path, not file:name` (exit code 2). If `build` points at one module that has several entries and no name is given, `cli: several entries; choose one of: <名前>, …` (exit code 2). `check` checks all the dishes of the file or module
- `build` (S352 to S354): so that each entry's result is not affected by errors in other dishes, when an entry is passed to the public API, only that dish and the dishes reachable from it through `make` are evaluated (definitions are checked in all modules; fixed in P12-1 so that errors inside recipes called with `make` are not dropped). The menus and dishes in the given scope (the whole project if none) are built one by one in order of module name and entry name, and `schedule.json`, `ir.txt` and `diagnostics.json` (`diagnostics.sarif` with `--format sarif`) are written to `out/<モジュールの道筋>/` (`out/<モジュールの道筋>/<入口>/` if the module has several entries). `out/index.json` is `{"schemaVersion": 1, "entries": [{"module", "entry", "status", "errors", "warnings", "infos", "dir"}…]}`. At the end, `build: <n> entries, <m> passed`. `schedule.json` is the schedule only, `diagnostics.json` is the result JSON with no output selected, and `ir.txt` is the same text as `debug ir`. The contents of `out/` are removed before writing (S443; the output of entries from the previous run does not remain)
- The contents of `render`, `bundle` and `unbundle` are made in K and L. In G, they print `cli: <名前> is not available yet` and exit with code 2

### 4.26 J: Standard library and phrases (§13-10)

- **S357** `std` is `stdlib/package.ozen` and the `.ozen` files under `stdlib/`, and its package name is `std` (concept §16).
- **S358** `std` has every item in the list of §4.7 and every relation of §4.7.1 (matched against the JSON list of action signatures that the core outputs, D28).
- **S359** Numeric attributes in `std` carry their source (primary source, calculation, provisional) (concept §12.3).
- **S360** `std` is treated the same as an author's definitions: copying a `std` definition into a user package with the same syntax gives the same text IR (concept §11.2).
- **S361** `std.prelude` contains the basic vocabulary (common units and actions) (concept §4.7).
- **S362** `phrases/ja.ftl` and `phrases/en.ftl` have the keys for every action, ingredient, part, vessel, property, alias and diagnostic of `std` (0 findings in the drift check, concept §7.1).
- **S458** Names in `std` follow the naming conventions (concept §4.4).
- **S459** Provisional values in `std` are held as ranges (lower < upper) (the same rule as S23).
- **S468** Primary-source values and provisional values in `std` also carry the same records as S465 to S467 (carried over from the table in D28).
- **S460** `std` does not write values from the food composition table as numbers, but refers to them by food number (the same rule as S24, concept §3).
- **S461** Every capability in `std` has an equipment kind that has it (the same rule as S25).

#### 4.26.1 Details of phrase keys (decided in P13-2; all (added))

- A key is a kind prefix plus the name in kebab case (split before upper-case letters and lower-cased; `cookRice` → `cook-rice`): action `action-`, ingredient `ingredient-`, part `part-` (one per part name across ingredients), vessel `vessel-`, property `property-`, alias `alias-`, diagnostic `diag-`
- Allowed variables: for an action, its argument names (`action-cut` has `$x`, `$style`, `$size`); for a diagnostic, `$arg1` to `$arg9` in argument order; for ingredients, parts, vessels, properties and aliases, `$stage`, `$dialect`, `$context` and `$count`, which choose the name used
- The drift check `renderers/tools/phrases-check.mjs DEFINITIONS PHRASES_DIR` builds the expected keys from the JSON of `ozen debug definitions`, and counts keys missing from `ja.ftl` and `en.ftl` and variables outside the allowed ones (S389). It reads only `キー =` at the start of a line and indented continuation lines of `.ftl` (reading Fluent proper is phase K)

### 4.27 I, J: Samples (§13-9, §13-10)

Samples are kept in `samples/` (CC-BY-4.0). Expectations are written as a list of properties on the output JSON (`samples/<名前>/expect.yaml`), and `tests/samples/check.py` matches them.

- **S363** The minimal sample "curry and rice" (menu composition) passes with 0 errors without a kitchen, with a kitchen, with `--cooks 1` and with `--cooks 2`, and gives the same result in all environments (concept §13-9).
- **S364** Menu A (yakisoba) passes as a single-dish menu, with plating and a wave added (concept §12.1).
- **S365** Menu B (curry rice), as a composition, connects the outputs of 2 recipes into one DAG (concept §12.1).
- **S366** In menu C (stir-fry, miso soup, rice), the tasks of cutting cabbage are merged, a wash of the cutting board after raw meat is inserted, and the miso soup is plated in a soup bowl (concept §12.1).
- **S367** In menu D (curry (rice or store-bought naan), salad, parfait), the breakdown of choices, the parfait's assembly order, the `buy` of the naan, 2 waves and the salad's chilling appear in the schedule (concept §12.1).
- **S368** In menu E (mixed juice), contention for the blender, the soak-then-wash (`Sticky`), the freshly-made serving constraint, freezing the ice, and the milk allergen appear (concept §12.1).
- **S369** In T1 (one whole yellowtail), parts, yield, multiple outputs of a butchering action, hand-over across menus, leftover information, the raw-eating exception, and the cutting board's raw-fish dirt appear (concept §12.4).
- **S370** In T2 (4 people including one with an egg allergy), contamination through a shared bowl or cooking chopsticks is detected as `SAF`, and that person's portion becomes an egg-free chicken rice bowl (concept §12.4).
- **S371** In T3 (different kitchens), in a kitchen without a rice cooker the candidate of cooking rice in a pot is chosen, and in a kitchen with a dishwasher and `--cleanup afterMeal`, the dishwasher runs during the meal (concept §12.4).
- **S372** In T4 (beef kalbi rice bowl with kalbi), a union type whose part is not fixed and the `Fatty` requirement pass (concept §12.4).
- **S373** The schedules of all samples pass the checks of `tools/schedcheck/` (D20).
- **S374** All samples give the same digest in all environments (concept §13-10).
- **S456** The samples in `samples/` can be written with only the vocabulary of `std`, and there is not a single vocabulary declaration (`ingredient`, `action`, `unit`, `kind`, `vessel`, `dirt`, `trait`) in the samples (confirming that the list of §4.7 is enough for the reference menus).

#### 4.27.1 Details of sample expectations (decided in P13-0; all (added))

- `expect.yaml` has `entry` (the entry; if left out, the CLI default) and `runs` (a list of runs). A run has `name`, and `entry` (an entry for that run only), `kitchen`, `options`, `cooks`, `args` (arguments added to the CLI) and `expect` (properties). `tests/samples/check.py` runs `ozen check --format json --emit ir,schedule,steps …` in the sample's folder and matches the result. All schedules in the result also go through the checks of `tools/schedcheck/` (S373). One property (one item of a list) counts as one expectation
- Vocabulary of properties:
  - Overall: `errors`, `warnings` (counts), `level`, `diagnostics.include` (a name, or `{name, args}` matching the arguments from the front), `diagnostics.exclude` (a name)
  - IR (the IR of the entry dish): `labels` (contains a node name), `reaches` (`{to, from}`: tracing inputs back from the `to` node (kind or name) reaches the name `from`), `kinds` (`{種類: 最小の数}`), `inputs` (`{名前: [入力の名前の順]}`; assembly order), `allergens` (`{名前: [アレルゲン]}`, D33), `choices` (`{選択の候補の名前: 人数}`, D33)
  - Schedule (the schedule of the entry dish): `tasks` (contains a task's action or kind), `taskCount` (`{動作: 数}`), `order` (`[a, b]`: every upper end of a is before every start of b), `cooks`, `cooksUsed`, `waves` (count), `wave` (`{作業: 波の番号}`), `usesKind` (`[作業, 種類]`: the kind of unit that task uses), `holds` (`[食べ物, 種類]`: keeping warm, chilling or freezing uses a unit of that kind), `serveWithin` (the number of dishes with a limit on the time until serving)
- Properties per sample (the kitchen is each sample's `kitchen.ozen`; per-person attributes are in `options.ozen`):

| Sample | Runs | Main properties (S) |
| --- | --- | --- |
| `menuA` (yakisoba, single dish) | no kitchen, with kitchen | 0 errors, `kinds: {plate: 1}` (plating is added from `@vessel`), `waves: 1`, no `noPlating` (S364) |
| `menuB` (curry rice, composition) | no kitchen, with kitchen, `--cooks 2` | 0 errors, `reaches` shows the ingredients of 2 recipes reaching one plating (S365) |
| `menuC` (stir-fry, miso soup, rice) | with kitchen, `--cooks 2` | 0 errors, the cabbage `cut` merges into one in `taskCount`, `washInserted` (cutting board and raw-meat dirt), the miso soup is plated in a soup bowl (`usesKind`) (S366) |
| `menuD` (curry, salad, parfait) | with kitchen (with per-person choices), with kitchen (no choices) | 0 errors, `choices` (number of people who chose; with no choices, even split as an `ASM` assumption), assembly order in `inputs`, `kinds: {buy: 1}` (naan), `waves: 2`, `holds` for the salad (fridge) (S367) |
| `menuE` (mixed juice) | with kitchen, `--cooks 2` | 0 errors, `blend` uses the same blender in turn, soaking and washing for `Sticky`, serving time for freshly made (a dish with `serveWithin`), `holds` for the ice (freezer), milk in `allergens` (S368) |
| `T1` (one whole yellowtail) | with kitchen | 0 errors, multiple outputs and parts of the butchering action, leftover information, no `SAF:unwashedHazard` and the like on the sashimi, washing for raw-fish dirt (S369) |
| `T2` (4 people including an egg allergy) | options where the chosen dishes are correct, options where the person with the egg allergy gets oyakodon (the kitchen has one bowl) | the correct options give 0 errors and one egg-free chicken rice bowl in `choices`, and the schedule check (washing compatibility) passes even with a shared bowl. The options where the person with the egg allergy gets oyakodon give `SAF:allergen` (S370) |
| `T3a`, `T3b` (different kitchens) | kitchen without a rice cooker, dishwasher with `--cleanup afterMeal` | 0 errors, `variantChosen` (cook rice in a pot), dishwasher run during the meal (S371) |
| `T4` (beef kalbi rice bowl) | no kitchen, with kitchen | 0 errors, the union of `Karubi` and the `Fatty` requirement pass (no `aliasCandidates`) (S372) |


### 4.28 K: Renderers and the settings screen (§13-11)

Renderers are kept in `renderers/` (D22). Their inputs are the core's JSON (S330, S334, S335) and the phrase files. Tests are `node --test` and golden files of the output HTML and text.

- **S375** The recipe text is made from the verification level 0 output: a procedure that does not depend on the kitchen, in dependency order (concept §7.1).
- **S376** The schedule table is made from the level 2 output: steps with times and cooks (concept §7.1).
- **S377** Tasks done in a row on the same ingredient (peel → cut) are merged into one sentence (concept §7.1).
- **S378** A range marked with `@group(キー)` is displayed as one block, and `@group` does not go into the IR (concept §7.1).
- **S379** The schedule table has a chapter per serving wave, with a "during the meal" column between waves that lists the tasks that do not use hands (concept §7.1).
- **S380** The split between "prep" and "cooking" is made by the renderer from the distance to the first wave's serving time (concept §7.1).
- **S381** The HTML is self-contained, with inline CSS and JS, and loads no external files, CDNs or web fonts (concept §8).
- **S382** The Gantt chart (SVG) draws the schedule's tasks at their time positions in rows per cook and per resource (concept §7).
- **S383** The shopping list totals the menu's ingredients per ingredient and includes the `buy` items (concept §5, §7). (added: concept says the location "may be either"; this plan puts it in the renderer)
- **S384** Phrases are in Fluent, and keys are a kind prefix plus the name in kebab case (concept §7.1).
- **S385** Phrases are layered in the order `std` → dependency packages (in name order) → one's own project, and later ones override earlier ones (concept §7.1).
- **S386** When a phrase is missing, the fallbacks are, in order, the user's language → English → a display made mechanically from the identifier, so something can always be displayed (concept §7.1).
- **S387** The name used is chosen by attribute (the stage of a fish that changes names as it grows), dish context and region (`$dialect`) (concept §7.1).
- **S388** For a name with a part, a key just for that combination is used if there is one; otherwise it is built as "kind + part" (concept §7.1).
- **S389** The drift-check tool warns about actions without phrases and arguments not in the signature (concept §7.1).
- **S390** Counter words (ko, hon, mai, tama) are attached by the phrases (concept §4.1).
- **S391** Vague time properties are displayed as words ("while warm") (concept §4.5).
- **S392** The chosen `variant`, the reasons for inserted washes, and the list of assumptions are shown in the text (concept §4.2, §5.1).
- **S393** The settings screen is generated as an HTML form from the JSON of type definitions, and the kitchen and options text made from the form's values passes the core's type check (concept §4.2, D24).
- **S394** Adding an item to a type definition adds an item to the form without changing the renderer's code (concept §4.2).
- **S395** It can be called from the CLI as `ozen render` (concept §7.1).
- **S396** The renderer body (`renderers/lib/`) uses no node-specific APIs (`fs`, `process` and so on) and also runs in the browser (shared by the WebViews of all environments, concept §7, §7.1).
- **S445** Recipe text without times is divided by the names of the serving waves ("main", "dessert") (concept §7.1).
- **S446** Diagnostic messages are made from the `diag-*` keys of the phrases (concept §7.1).
- **S447** Whether rice is "polished" or "washed" is chosen by the phrases from the ingredient's trait (grain) (concept §12.2).
- **S448** From `index.json` and each entry's result, a book-shaped HTML with a table of contents and chapters is made (concept §5.2).
- **S449** The HTML has CSS for printing (`@media print`) (concept §7).
- **S450** The ingredient list is in the order written in the recipe's `ingredients` (AST metadata, concept §6).
- **S451** When all samples are rendered in `ja` and `en`, there are 0 words that fell back to the mechanical display from identifiers, 0 Fluent errors, 0 unresolved variables, and 0 identifiers left in the body text (including diagnostic arguments; D38).
- **S452** In the Gantt chart, tasks in the same row do not overlap, and every task fits within the drawing area.
- **S453** Every input of the settings screen's form has a visible name (label), and quantity items have units.
- **S454** The output HTML can also be read as well-formed XML (`xml.etree` gives no error). (added)

#### 4.28.1 Details of the renderers (decided in P14-0; all (added))

- Locations: the body is `renderers/lib/*.mjs` (`phrases.mjs` phrase layering, fallbacks and names used; `recipe.mjs` recipe text; `schedule.mjs` schedule table; `gantt.mjs` Gantt chart; `shopping.mjs` shopping list; `html.mjs` HTML; `book.mjs` book shape; `settings.mjs` settings screen; `render.mjs` entry point). Fluent is `renderers/vendor/fluent-bundle/` (the ES module `esm/` of `@fluent/bundle` 0.19.1 and the license text; the source, version and sha256 are in `renderers/vendor/README.md`). Tests are `renderers/test/<rtest の名前>.test.mjs` (one-to-one with `rtest/*` in the coverage table). Tools are in `renderers/tools/` (`phrases-check.mjs`, `render-all.mjs`, `bundle.mjs`, `browser-test.sh`)
- Inputs: the result JSON (`--emit ir,schedule,steps,definitions`) and the list of phrase files (`std` → dependency packages (in name order) → one's own project, S385). The entry point is `render(result, {phrases, lang, dialect, kind})` (`kind` is `recipe`, `schedule`, `shopping`, `html` or `book`), which returns a string. The body uses no node APIs (S396). Reading and writing files is done by the tools and the CLI (`ozen render`)
- Names: a value's display name comes from the IR node's `type` (D36) as `ingredient-<型>`. For a part, the combination key `ingredient-<型>-<部位>` is used if it exists; otherwise it is built with `name-with-part` (`{ $ingredient }{ $part }`) (S388). The selectors for the name used receive `$stage`, `$dialect` (rendering settings) and `$context` (dish context) (S387). Counter words attach `counter-<型>` (`counter-default` if missing) to count quantities (S390). Vague time properties are shown as the words of `property-<名前>` (S391)
- Action sentences: the action's arguments are passed by name to `action-<名前>`, and the food argument `x` additionally gets `$x-kind` (the type's kind trait in lower case; `grain` for `Grain`). Rice's "polish" and "wash" are told apart by the `$x-kind` selector of `action-wash` (S447; the phrase check allows action arguments and their `-kind`). When a phrase is missing, the fallbacks are, in order, the user's language → English → a display made from the identifier (`slice(onion, 3 mm)`) (S386)
- Recipe text (level 0, S375): after the ingredient list (in the order written, S450), the steps are listed in IR dependency order. Tasks done in a row on the same value (exactly one input, and only the next task uses that value) are merged into one sentence (S377), and a `@group` range becomes one block with a heading (S378). With two or more waves, the text is divided by the wave names (the phrase `wave-<名前>`, or the name if missing) (S445)
- Schedule table (level 2, S376): rows of time (in the form `0:00` from the start; `serveAt` is a time from the start and has no clock time, so this form was decided in P16-3), step and cook. A schedule without waves (an entry that is only a recipe) is split into prep and cooking using the end of the last task as the reference. Each serving wave is a chapter, with a "during the meal" column between waves listing unattended tasks (S379). Tasks that end more than 30 minutes before the first wave's serving time are "prep", and the rest are "cooking" (the split distance is the rendering setting `prepBefore`, default 30 minutes; S380). The chosen candidates, the reasons for inserted washes and the assumptions (the diagnostics `variantChosen`, `washInserted` and `ASM`) are shown in the text with the `diag-*` messages (S392, S446)
- Gantt chart (S382): SVG. The rows are the cooks (`cook1`…) and the resource units the tasks use. One minute is a fixed width, and each task's rectangle has `data-row`, `data-start` and `data-end` (seconds) (`tools/htmlcheck/htmlcheck.py` checks overlaps and the drawing area, S452)
- Shopping list (S383): the quantities of the IR's `in` and `buy` nodes are added up per type (only quantities of the same dimension; a different dimension goes on a separate line). `buy` items are included too
- HTML (S381, S449, S454): one file, with inline CSS and JS and no external references. It has `@media print` CSS. Elements are written so they can be read as well-formed XML (empty elements are `<br/>`). The contents are the title, ingredients, recipe text, schedule table, Gantt chart, shopping list and notes
- Book shape (S448): one HTML with a table of contents and a chapter per entry, made from `out/index.json` of `ozen build` and each entry's result
- Details added in the P16-3 audit: the name stage is passed as the rendering setting `stage` (`ozen render --stage`) (D39). `ozen render` also layers the dependencies' `deps/<名前>/phrases/`, decides the entry by the same rules as `ozen build` (with only a module name, when that module has one entry), and for a result with errors prints the diagnostics and exits with code 1. Diagnostic arguments look up phrases by the kind decided per diagnostic (an action's `.name`, candidate `variant-*`, reason `reason-*`, dirt `dirt-*`, equipment unit, dish `dish-*` and local name `name-*`, part, allergen, value, time) (D38). `type`, `part` and `quantity` were added to `ingredients` in the result JSON (because unused ingredients have no IR node either; a supplement to D36). `render-all.mjs` also writes a book shape per sample (`book.<サンプル>.<言語>.html`)
- How the golden files are made: the expected values of `rtest` are written first in `renderers/test/expected/<名前>.<txt|html>`, and the tests have `build/core-gcc-release/ozen` make the result JSON inside the test (from a sample or a small input). `render-all.mjs` renders all the JSON in `build/results/json/samples/` (which `tests/samples/check.py` writes for each run's result) in both `ja` and `en`, writes it to `build/results/html/`, and counts words that fell back, Fluent errors and unresolved variables (S451)

### 4.29 L: `ozen bundle` (§13-12)

- **S397** `ozen bundle` puts the dependencies other than `std` into one file, and the parts put in keep their namespace as inline modules `module パッケージ.道筋 { … }` (concept §4.7).
- **S398** A bundled file has `bundle { std, entry, from, license }`, recording the `std` version at the time it was made and the original package (concept §4.7).
- **S399** What goes in is the entry, the definitions and dishes imported from it with `import` (only the parts used, for dependency packages), and the phrases (Fluent text in `phrases`). What does not go in is the kitchen spec, `std` and unused definitions (concept §4.7).
- **S400** The order inside a bundled file is module-name order, and the same input always gives the same file (concept §4.7).
- **S401** `ozen unbundle` can turn it back into files and folders per module. The declarations of the restored files (including `local`) are the same as the original source, and the IR built from them is the same as the original (concept §4.7).
- **S444** `ozen unbundle` does not overwrite existing files (if any exist, it writes nothing and gives an error). (added)
- **S402** A bundled file can be read as an ordinary `.ozen`, and building it alone uses the modules inside (concept §4.7).
- **S403** A module inside a bundled file that claims to be `std` is an error (concept §4.7).
- **S404** The `std` version recorded in a bundled file is also checked by the compatibility rules (concept §4.7).
- **S405** Annotations inside a bundled file (`@allow` and so on) apply only inside their module (concept §6.1).
- **S406** `local` definitions stay closed inside their module even when bundled (concept §4.7).
- **S407** When a bundled file is brought into a project, it is an error if another version of the same package is there (concept §4.7).
- **S408** Shared files (bundled files, kitchen specs) are treated as untrusted input; the limits apply, and they are fuzzed (concept §4.2, §4.6, §10).
- **S477** A module inside a bundled file belongs to the `from` package whose name equals the first segment of the module name, and an `import` inside it also resolves with a path inside the same package (`import recipes.curry`). (added, D37)
- **S478** `ozen unbundle` splits `phrases` at the `###` separators per package and puts each back into that package's `phrases/<言語>.ftl`. (added, D37)

#### 4.29.1 Details of bundled files (decided in P15-0; all (added), D37)

- Shape: the top level of the file is only `bundle { … }`, the inline `module 名前 { … }` in module-name order, and `phrases <言語> """ … """` in order of language name. The items of `bundle` are, in order, `std` (the `std` version at the time it was made; `主.副.修正` as in `VERSION`), `entry` (the name of the entry definition), `from` (the name and version of the original package, followed by the dependency packages put in, one each, in name order) and `license` (if the original `package.ozen` has one)
- Contents: starting from the entry definition, the names that appear in definition bodies are matched against the definitions of the same module and of the modules that module imports with `import`, and the reachable definitions are collected (if a name matches both, both go in; putting in more is the safe side). `extend`, `rule` and `check` affect everything wherever they are written, so those of every module go in (the P16-3 audit found a bug where dropping `extend` from unused modules changed the entry's candidates, and fixed it). Kitchen specs (`kitchen` declarations) and `std` do not go in. Definitions inside a module are in their original order, and the text of each definition's range in the original source (including the comment just before it) is copied as is. Only `import`s that point to modules that go in are kept. A module with no definitions going in is not put in
- Module names: the original module name (package name + path). Dependency packages are the modules under `deps/<名前>/` (S397)
- Phrases: the original package's `phrases/<言語>.ftl` and the included dependencies' `deps/<名前>/phrases/<言語>.ftl` are joined in the order dependencies (in name order) → original package, each with a `### <パッケージ名>` line at its start (the same order as the renderer's layering, S385). If the text contains `"""`, it is an error
- Building alone (S402): a module inside a file with `bundle` belongs to the `from` package whose name equals the first segment of the module name (S477). Without a given entry, the `entry` of `bundle` is the entry. The recorded `std` version is checked by the same rules as `requires` (S404; `PKG006`, `PKG007`, `PKG010`)
- Bringing in (S407): if the `from` packages of a bundled file placed in the project and the packages in `deps/` include one with the same name and a different version, it is `PKG009`
- Restoring (S401, S444, S478): `ozen unbundle FILE [-o DIR]` (the default is the current folder). The original package's modules are written to `<道筋>.ozen` and the dependencies to `deps/<名前>/<道筋>.ozen`, and `package.ozen` (the name and version of `from`, `license`, `requires { std: "<版>" }`) and the dependencies' `deps/<名前>/package.ozen` are written too. If even one file exists at a destination, nothing is written and it is an error (exit code 2)
- CLI: `ozen bundle [ENTRY] [-o FILE]` (the default is standard output). The contents are made by an internal function of the core (which does not read or write files); the CLI reads the phrase files, passes them in, and writes the result

### 4.30 What D to L add, and where the code goes

| Location | What goes there | Phase |
| --- | --- | --- |
| `core/include/ozen.h` | Public API (S327, S336) | G |
| `core/src/` | Arena, interning, spans, diagnostics, lexer, parser, name resolution, packages, evaluation and macros, type check, IR, scheduler, JSON, text IR | D to G |
| `core/src/diag_codes.def` | The diagnostic registry (X macro; append-only) | D on |
| `core/grammar.ebnf` | The grammar of the whole syntax (S157) | D |
| `core/gen/` | CMake scripts that make the C arrays embedding `std` (S179) | D |
| `core/tests/unit/`, `core/tests/diag-names.txt` | C unit tests, a copy of the diagnostic names | D on |
| `cli/` | The CLI `ozen` (D21) | D to G |
| `stdlib/` | The `std` package (converted from the table in D28) | E to J |
| `phrases/` | Phrases of `std` (`ja.ftl`, `en.ftl`) | J, K |
| `tests/golden/`, `tests/samples/` | Golden files through the CLI, matching of sample properties | D to J |
| `tools/schedcheck/` | The schedule checker (Python, a path separate from the implementation; D20) | F |
| `tools/std-from-vocab/` | The one-time conversion from the table to `std` (D28) | E |
| `schemas/` | JSON Schemas of the output (S331) | G |
| `bindings/ios/`, `bindings/android/`, `bindings/wasm/`, `bindings/linux/` | Module map and Swift tests, JNI and Java classes, JS bridge and Worker, shared library | H |
| `.github/workflows/` | CI (D19) | B, H |
| `samples/` | The minimal sample, reference menus A to E, T1 to T4 (CC-BY-4.0) | I, J |
| `renderers/lib/`, `renderers/cli.mjs`, `renderers/vendor/`, `renderers/test/` | The renderer body, the entry from the CLI, the Fluent implementation (D23), tests | K |
| `fuzz/` | Fuzz entry points of the core (parser, expander, type check, scheduler, bundled files) | D to L |

---

## 5. Definition of the machine-checked goals

There are no prerequisite goals (there is no earlier plan and no build system). All goals for the build, the tests and the checks are defined in this section.

When a goal fails, report which part failed (which check subcommand, which build configuration, which test, which environment).

- The diff range is counted as `R=plan-start..HEAD`, from the tag `plan-start` that P0-7 puts on `main` (D2).
- `<cfg>` is one of the 4 build configurations `gcc-debug-san`, `clang-debug-san`, `gcc-release`, `clang-release` (`proto/scripts/build-all.sh` creates them under `build/<cfg>/`).
- A count that is expected to be 0 is judged by the count in the output, not by the exit code (`grep -c` returns exit code 1 when it finds nothing, so we count with `wc -l`).
- In P0-5, we make `scripts/goals.sh`, which turns the code blocks below into functions, so that `scripts/goals.sh <ID>` runs one goal (its content is the same as these code blocks).

### 5.1 Structure and data

| ID | What it judges | Pass condition |
| --- | --- | --- |
| `G-REPO-SPDX` | The SPDX lines of S1 to S3 | `spdx: <n> files checked, 0 missing, 0 wrong`. n equals the number of lines of `--list` |
| `G-REPO-LICENSE-TEXT` | The license texts of S4 | The 4 body lines are each 1 or more, the template placeholders are 0 |
| `G-REPO-README-LICENSE` | The category table and the sentence of S6 | All 3 lines are 1 or more |
| `G-REPO-NOTICE` | The source wording of S5 and the record in the README (from P3-2 on) | All 6 counts are 1 or more |
| `G-REPO-TOOLS` | The tool versions of S131 | `tools: <n> checked, 0 mismatched`. n is the number of lines of `toolchain.txt` |
| `G-REPO-SELFTESTS` | Whether the new scripts and checkers have self-tests, and whether they pass | `selftests: <n> scripts, 0 without test, 0 failed`. The targets are the check scripts and checkers in `scripts/`, `proto/scripts/`, `tools/`, `tests/samples/`, `tests/props/`, `renderers/tools/`, `bindings/` (`check-*`, `run-*`, `digest.sh`, `goals.sh`, `*-test.sh`, `schedcheck`, `stdcheck`, `jsoncheck`, `htmlcheck`, `vendorcheck` and so on. The list is `scripts/selftests.txt`). `build-all.sh`, `build-core.sh`, `fuzz.sh`, `fuzz-core.sh` are excluded, because the output of the goals that use them confirms them |
| `G-REPO-RED-FIRST` | Whether the test failed at the commit that added its expectation (D20) | `red-first: <c> commits, 0 not red`, c ≥ 1. The only exceptions are the lines of `scripts/red-first-exceptions.txt`, and the reason for each line is in §9 |
| `G-VOCAB-SOURCES` | S465 to S467 | `sources: <p> primary, <q> provisional, 0 missing fields, 0 quote mismatches, 0 provisional without search, 0 pdf in repo` |
| `G-VOCAB-PRIORWORK` | S469, S470 | `prior-work: <n> elements from 4 sources, 0 unmapped, 0 without page` |
| `G-VOCAB-SCHEMA` | S7, S137 | `schema: 11 files, 0 errors`, and an exact match of the list of file names |
| `G-VOCAB-CHECK` | S13 to S27 with the real tables | A line that starts with `check: 0 errors`, exit code 0 |
| `G-VOCAB-LISTS` | S29, S138, S139 | `lists: <n> listed, 0 missing; values: <v> checked, 0 mismatched; relations: <r> listed, 0 mismatched`. n is 258 + a (§4.7) |
| `G-VOCAB-LISTS-COPY` | S140 | `copy: <n> identifiers, 0 differences` |
| `G-VOCAB-SAMPLE` | S57 | `sample: 0 errors; expectations: <k> checked, 0 failed`, k is the number written in §9.2 |
| `G-VOCAB-TESTS` | Whether all the Python tests ran | `Ran <N> tests` and `OK`. N is the sum of the `fx` and `py` counts that `check-plan-tests.sh` prints |
| `G-VOCAB-READONLY` | S28 | The before and after comparison matches |
| `G-VOCAB-FCT` | S58, S61, S141 | `fct: <n> rows, 0 mismatches; sha256 ok; order ok; refs ok; headers ok`. n is the number of data rows of `refuse.csv` |
| `G-VOCAB-FCT-STABLE` | S60 | Exit code 0 |
| `G-VOCAB-EXPECT-INDEPENDENT` | Expectations and the check script code are not changed in the same commit (D20) | `separation: <c> commits, 0 mixed`, c ≥ 1 |
| `G-PROTO-BUILD` | S124 | `build: 4 configs, 0 failed` (each configuration with `-Werror`) |
| `G-PROTO-FLAGS` | Whether the flags of S124 are really applied | All 4 give `flags: <n> files, 0 missing`, n is the number of `proto/src/*.c` |
| `G-PROTO-COMPILERS` | The compiler of each configuration of S131 | `compilers: 4 configs, 0 mismatched` |
| `G-PROTO-PURE` | S117 | `pure: 0 disallowed symbols, 0 disallowed includes` |
| `G-PROTO-NOGLOBAL` | S118 | `noglobal: <m> objects, 0 writable sections` (sections that are `.data`, `.bss`, `.tdata`, `.tbss` or their subsections, with a nonzero size. Sections that start with `.data.rel.ro` are excluded) |
| `G-PROTO-NOFLOAT` | S119 | The loop succeeds and the count is 0 |
| `G-PROTO-NOINT128` | S149 | 0 |
| `G-PROTO-EXPORTS` | S116 | Exact match with `ozp_default_limits ozp_expand ozp_free_string ` |
| `G-PROTO-DIAG-REGISTRY` | S113, S146, S147 | `diag: <n> codes, 0 duplicate codes, 0 duplicate names, 0 bad codes, 0 unlisted, 0 unused`. n is the number of lines of `proto/tests/diag-names.txt` |
| `G-PROTO-DIAG-APPEND` | Only appends for S113 | 0 deleted lines, 1 or more target commits |
| `G-PROTO-GOLDEN-INDEPENDENT` | The golden expectations and the implementation (including the driver and `run-golden.sh`) are not changed in the same commit (D20) | `separation: <c> commits, 0 mixed`, c ≥ 1 |
| `G-PROTO-VERSION` | D30 (no version for the prototype) | All 3 are 0 |
| `G-PROTO-REPORT` | S132 to S135 | `report: <k> checks, 0 failed`. k is at least the number of files in `build/results/` + 4 |
| `G-PROTO-README-EXAMPLE` | S136 | `readme: <k> examples, 0 mismatches`, k ≥ 1 |
| `G-REPO-VERSION` | The version of D30 | `ozen --version`, `stdlib/package.ozen` and the library version are the same as `VERSION`, and `VERSION` matches the version written in §9.1 |
| `G-CORE-BUILD` | The build of the core and the CLI in the 4 configurations (with `-Werror -Wswitch-enum`) | `core-build: 4 configs, 0 failed` |
| `G-CORE-FLAGS` | Whether the core flags are really applied | All 4 give `flags: <n> files, 0 missing` |
| `G-CORE-C23-SUBSET` | S151 | `c23: 12 allowed probes ok on <t> toolchains, 0 banned uses`. t is 2 in P7-1 (the local gcc and clang), 3 in P11-2 (+ emcc), and 5 after P11-3 and P11-4 (+ the CI Xcode and NDK. The output of the CI jobs is read with `gh`) |
| `G-CORE-UNIT` | The core unit tests | `100% tests passed, 0 tests failed out of <N>` in the 4 configurations, N is the same as `Total Tests` of `ctest -N` |
| `G-CORE-GOLDEN` | Golden through the CLI (D to L) | `golden: <N> passed, 0 failed, 0 timed out` in the 4 configurations, N is the same as the number of entries of `tests/golden` (`.ozen` files and golden folders) |
| `G-CORE-GRAMMAR` | S157 | `grammar: <r> rules, 0 not covered` (collects the rules actually used with `ozen debug ast --rules`) |
| `G-CORE-PURE`, `G-CORE-NOGLOBAL`, `G-CORE-NOFLOAT`, `G-CORE-NOINT128` | S181 to S183, D15 | The same pass conditions as the goals of the same names in B, for the core library and `core/src` |
| `G-CORE-ARENA-ACCESS` | S152 | 0 arena struct definitions outside `arena.c`, 1 `static_assert` on the index type |
| `G-CORE-OOM` | S184 | `oom: <f> files, <p> points, 0 failures` |
| `G-CORE-DIAG-REGISTRY`, `G-CORE-DIAG-APPEND` | S260, S266 | The same pass conditions as the goals of the same names in B, for `core/src/diag_codes.def` and `core/tests/diag-names.txt` |
| `G-CORE-STD-EMBED` | S179 | Exit code 0 (the embedded copy matches `stdlib/`) |
| `G-CORE-PROPS` | The property tests of S160, S173, S223, S273, S279, S281, S360, S401 | Each gives `<名前>: <n> cases, 0 differences` (for monotonicity, `0 decreases`) |
| `G-CORE-DETERMINISM` | S185 | The 8 digests are the same |
| `G-CORE-FUZZ` | S180, S408 | For each target, `fuzz-<対象>: <e> execs in <t> s, 0 crashes, 0 leaks, 0 timeouts` (t ≥ 600, e ≥ 1) |
| `G-TYPE-SEVERITY-TABLE` | S269, S457 | `severity: <e> examples, 0 mismatched; categories: <c> examples, 0 mismatched`. At the gate of E, only the examples of the diagnostics of E (`--phase E`); from P9-7 on, all 17 examples |
| `G-STD-LISTS` | S358 (matching the lists and relations after D28) | `lists: <n> listed, 0 missing; values: <v> checked, 0 mismatched; relations: <r> listed, 0 mismatched` (the input is the list of all definitions of the core, S462) |
| `G-STD-SOURCES` | S468 | The same pass condition as `G-VOCAB-SOURCES`, for the list of all definitions of `std` |
| `G-STD-PROVENANCE` | S359 | `provenance: <n> values, 0 without source` |
| `G-STD-PHRASES` | S362, S389 | `phrases: <n> keys, 0 missing in ja, 0 missing in en, 0 unknown args` |
| `G-SCHED-CHECK` | S287 and others, the schedule constraints (item 6 of D20) | `schedcheck: <n> schedules, 0 violations`, n is the number of files in `build/results/schedules/`, and the same as the number of golden and sample entries that output a schedule |
| `G-API-EXPORTS` | S327 | Exact match with `recipe_build recipe_default_config recipe_free_string ` |
| `G-API-HEADER` | S336 | All 4 combinations (gcc, clang × `-std=c17`, `-std=c23`) succeed |
| `G-API-SCHEMA` | S331, S337 | `json: <n> files, 0 invalid`, n is the number of golden and sample entries × the number of output kinds (`run-golden.sh` and `check.py` write `build/results/json/`) |
| `G-API-SARIF` | S333 | `sarif: <n> files, 0 invalid` (n is the number of golden files with diagnostics), and `cgolden/api/sarif-mapping` passes |
| `G-CLI-TESTS` | S345 to S356 | `cli: <n> cases, 0 failed` |
| `G-PLAT-LINUX` | S341, S343 (Swift and the JVM on Linux) | The list of dynamic symbols exactly matches `recipe_build recipe_default_config recipe_free_string `, `swift: <k> inputs, 0 mismatches`, `jni: <k> inputs, 0 mismatches` |
| `G-PLAT-WASM` | S342, S343 | `golden: <N> passed, 0 failed, 0 timed out`, `worker: <k> inputs, 0 mismatches`, and the digests match |
| `G-PLAT-IOS` | S338, S343, S455 (D19) | The run is `success` and its headSha is the current HEAD, the digests match, the xcframework artifact has 3 slices (`ios-arm64`, `ios-arm64-simulator`, `macos-arm64_x86_64`), and the Swift tests on macOS pass |
| `G-PLAT-ANDROID` | S339, S340, S343 (D19) | The run is `success` and its headSha is the current HEAD, the digests match, and the artifacts have `libozen.so` for 4 ABIs (4 files) |
| `G-PLAT-CI` | S344 | All CI jobs for the current HEAD are `success`, and the list of job names exactly matches `ci/jobs.txt` (written in P11-5) |
| `G-SAMPLE-CHECK` | S363 to S373 | `samples: <n> samples, <k> expectations, 0 failed` |
| `G-REND-TESTS` | S375 to S396 | `# fail 0`, and n of `# pass <n>` is at least the `rtest` count that `check-plan-tests.sh` prints |
| `G-REND-SELFCONTAINED` | S381 | `selfcontained: <n> files, 0 external references`, n is the number of files in `build/results/html/`, and the same as the number of rendered samples × the number of output kinds |
| `G-REND-BROWSER-SAFE` | S396 | 0 references to node built-in modules, and the match with the browser gives `browser: <k> inputs, 0 mismatches` |
| `G-REND-VENDOR` | Imported external files (Fluent of D23, the SARIF schema of P10-3) | The license texts exist, and the versions and sha256 in `renderers/vendor/README.md` and `schemas/vendor/README.md` match the files |
| `G-BUNDLE-STABLE` | S400 | The 2 outputs do not differ by a single byte, and `cgolden/bundle/order-by-name` passes |
| `G-REPO-CLAUDEMD` | S409 | The 2 sentences are each 1 or more |
| `G-REPO-NOREPLY` | S475 (before every push) | The emails of all commits are only `146819854+atsuki-seo@users.noreply.github.com`, this repository's `user.email` is the same, and tracked files contain the personal email 0 times |
| `G-STD-STATIC` | S458 to S461 | `std-static: <n> definitions, 0 naming, 0 provisional-point, 0 fct-inline, 0 capability-unprovided` |
| `G-REND-HTML` | S452 to S454 | `html: <n> files, 0 not well-formed; gantt: <g> charts, 0 overlaps, 0 out of bounds; form: <f> inputs, 0 without label, 0 quantity without unit` |
| `G-REND-PHRASES-RENDER` | S451 | `render-phrases: <n> samples × 2 languages, 0 fallbacks, 0 fluent errors, 0 unresolved, 0 identifiers` (identifiers are Latin-letter words left in the text (Japanese), and camel case and equipment unit numbers (English). D38) |
| `G-SAMPLE-STD-ONLY` | S456 | 0 |
| `G-REPO-TOOLS-CI` | S131 for the CI jobs too | `ci-tools: <n> jobs, 0 mismatched` (each job writes its tool versions to an artifact, which is compared with `toolchain.txt`) |
| `G-PLAN-COVERAGE` | The coverage table of this plan | The output of the 2 commands at the end of §5.3 is empty and 0 |
| `G-PLAN-TESTS-EXIST` | Whether the test names in the coverage table exist in the repository | `plan-tests: <n> names, 0 missing` |

Judgment commands (the content of `scripts/goals.sh`. The text to the right of `→` in a comment is the passing output):

```bash
R=plan-start..HEAD
CFGS="gcc-debug-san clang-debug-san gcc-release clang-release"
SRC=data/mext-fct-2023/source/<元のファイル名>     # decided in P3-1

# G-REPO-SPDX
scripts/check-spdx.sh                              # → spdx: <n> files checked, 0 missing, 0 wrong
scripts/check-spdx.sh --list | wc -l               # → n
# G-REPO-LICENSE-TEXT
grep -c 'Permission is hereby granted, free of charge' LICENSE-MIT    # → 1 or more
grep -c 'Copyright (c) 2026 atsuki.seo' LICENSE-MIT                   # → 1 or more
grep -E '<year>|<copyright holders>' LICENSE-MIT | wc -l              # → 0
grep -c 'Version 2.0, January 2004' LICENSE-APACHE                    # → 1 or more
grep -c 'Attribution 4.0 International' LICENSE-CC-BY-4.0             # → 1 or more
# G-REPO-README-LICENSE
grep -cF '| `proto/`, `tools/`, `scripts/`, `spec/vocab/`, `spec/schema/`, `spec/coverage/`, `spec/fixtures/` | MIT OR Apache-2.0 |' README.md
grep -cF '| `docs/`, `plans/`, `spec/samples/` | CC-BY-4.0 |' README.md
grep -cF 'Recipes you write in Ozen and the schedules and IR the compiler produces are yours; the Ozen licenses do not apply to them.' README.md
# G-REPO-NOTICE (from P3-2 on)
grep -c '日本食品標準成分表（八訂）増補2023年から引用' NOTICE
grep -c '日本食品標準成分表（八訂）増補2023年から引用' data/mext-fct-2023/README.md
grep -cE '^- Source URL: https://www\.mext\.go\.jp/' data/mext-fct-2023/README.md
grep -cE '^- Retrieved: 20[0-9]{2}-[0-9]{2}-[0-9]{2}$' data/mext-fct-2023/README.md
grep -cE '^- File: [^ ]+\.xlsx$' data/mext-fct-2023/README.md
grep -cE '^- SHA-256: [0-9a-f]{64}$' data/mext-fct-2023/README.md
# G-REPO-TOOLS
scripts/check-tools.sh                             # → tools: <n> checked, 0 mismatched
# G-REPO-SELFTESTS
scripts/check-selftests.sh                         # → selftests: <n> scripts, 0 without test, 0 failed
# G-REPO-RED-FIRST (check out the tree of that time into a temporary worktree and run only the tests of the added expectations)
scripts/check-red-first.sh "$R"                    # → red-first: <c> commits, 0 not red

# G-VOCAB-SOURCES
python3 tools/vocabcheck/sources.py spec/vocab spec/sources/search-log.yaml   # → sources: <p> primary, <q> provisional, 0 missing fields, 0 quote mismatches, 0 provisional without search
git ls-files '*.pdf' | wc -l                                                    # → 0
# G-VOCAB-PRIORWORK
python3 tools/vocabcheck/priorwork.py spec/notes/prior-work-elements.yaml spec/notes/prior-work.md   # → prior-work: <n> elements from 4 sources, 0 unmapped, 0 without page
# G-VOCAB-SCHEMA
python3 tools/vocabcheck/vocabcheck.py schema      # → schema: 11 files, 0 errors
ls spec/vocab | tr '\n' ' '                        # → actions.yaml aliases.yaml axes.yaml capabilities.yaml dimensions.yaml dirt.yaml ingredients.yaml sources.yaml traits.yaml units.yaml vessels.yaml
# G-VOCAB-CHECK
python3 tools/vocabcheck/vocabcheck.py check       # → check: 0 errors, <w> warnings, <i> infos
# G-VOCAB-LISTS
python3 tools/vocabcheck/vocabcheck.py lists spec/coverage/concept-lists.yaml spec/coverage/concept-relations.yaml
# G-VOCAB-LISTS-COPY
python3 scripts/check-lists-copy.py plans/ozen-roadmap.md spec/coverage/concept-lists.yaml   # → copy: <n> identifiers, 0 differences
# G-VOCAB-SAMPLE
python3 tools/vocabcheck/vocabcheck.py sample spec/samples/curryRice.yaml
# G-VOCAB-TESTS
python3 -B -m unittest discover -s tools -t . -p 'test_*.py' -v 2>&1 | tail -3     # → Ran <N> tests / OK
scripts/check-plan-tests.sh plans/ozen-roadmap.md                     # → … fx=<b> py=<c> … (N = b + c)
# G-VOCAB-READONLY (the folders of A have no changes before the run)
test -z "$(git status --porcelain -- spec tools data)"
s0=$(git status --porcelain --ignored -- spec tools data); h0=$(git ls-files -z | xargs -0 sha256sum | sha256sum)
for c in schema check; do python3 tools/vocabcheck/vocabcheck.py $c >/dev/null; done
python3 tools/vocabcheck/vocabcheck.py lists spec/coverage/concept-lists.yaml spec/coverage/concept-relations.yaml >/dev/null
python3 tools/vocabcheck/vocabcheck.py sample spec/samples/curryRice.yaml >/dev/null
[ "$s0" = "$(git status --porcelain --ignored -- spec tools data)" ] && [ "$h0" = "$(git ls-files -z | xargs -0 sha256sum | sha256sum)" ]   # → exit code 0
# G-VOCAB-FCT
python3 tools/fct/extract.py --check --xlsx "$SRC"
# G-VOCAB-FCT-STABLE
python3 tools/fct/extract.py --xlsx "$SRC" >/dev/null && test -z "$(git status --porcelain -- data/mext-fct-2023)"   # → exit code 0
# G-VOCAB-EXPECT-INDEPENDENT
scripts/check-commit-separation.sh "$R" '^spec/(fixtures|samples)/' '^tools/vocabcheck/'

# G-PROTO-BUILD
proto/scripts/build-all.sh                         # → build: 4 configs, 0 failed
# G-PROTO-FLAGS
for c in $CFGS; do python3 proto/scripts/check-flags.py --std-gcc c2x --std-clang c23 build/$c/compile_commands.json; done   # → flags: <n> files, 0 missing
ls proto/src/*.c | wc -l                           # → n
# G-PROTO-COMPILERS
proto/scripts/check-compilers.sh                   # → compilers: 4 configs, 0 mismatched
# G-PROTO-PURE
proto/scripts/check-pure.sh proto/src build/gcc-release/libozenproto.a build/clang-release/libozenproto.a
# G-PROTO-NOGLOBAL
python3 proto/scripts/check-noglobal.py build/gcc-release/libozenproto.a build/clang-release/libozenproto.a
# G-PROTO-NOFLOAT
( for f in proto/src/*.c; do gcc -std=c2x -O2 -mgeneral-regs-only -Iproto/include -c "$f" -o /dev/null || exit 1; done )   # → exit code 0
for f in proto/src/*.c proto/src/*.h proto/include/*.h; do gcc -x c -fpreprocessed -dD -E -P "$f"; done | grep -wE 'float|double' | wc -l   # → 0 (count after dropping comments)
# G-PROTO-NOINT128
git grep -nwE '__int128|__uint128_t|_BitInt' -- proto/src proto/include | wc -l   # → 0
# G-PROTO-EXPORTS
nm -g --defined-only build/gcc-release/libozenproto.a | awk 'NF==3{print $3}' | grep -v '^ozpi_' | sort -u | tr '\n' ' '   # → ozp_default_limits ozp_expand ozp_free_string
# G-PROTO-DIAG-REGISTRY
proto/scripts/check-diag-registry.sh
# G-PROTO-DIAG-APPEND
git log --format= -p "$R" -- proto/src/diag_codes.def | grep -E '^-[^-]' | wc -l   # → 0
git rev-list --count "$R" -- proto/src/diag_codes.def                                 # → 1 or more
# G-PROTO-GOLDEN-INDEPENDENT
scripts/check-commit-separation.sh "$R" '^proto/tests/golden/.*\.out$' '^proto/(src|driver)/|^proto/scripts/run-golden\.sh$'
# G-PROTO-VERSION
tr '\n' ' ' < proto/CMakeLists.txt | grep -E 'project\([^)]*VERSION' | wc -l                   # → 0
grep -v 'cmake_minimum_required' proto/CMakeLists.txt | grep -wE 'VERSION|SOVERSION|PROJECT_VERSION' | wc -l   # → 0
git grep -niE '^\s*version\s*:' -- spec | wc -l                                               # → 0
# G-PROTO-REPORT
python3 proto/scripts/check-report.py docs/prototype-report.md build/results proto/defects.tsv
# G-PROTO-README-EXAMPLE
proto/scripts/check-readme-example.sh
# G-PLAN-TESTS-EXIST
scripts/check-plan-tests.sh plans/ozen-roadmap.md   # → plan-tests: <n> names, 0 missing
```

Judgment commands for D to L (added to `scripts/goals.sh`. `<cfg>` is a core configuration: `core-gcc-debug-san`, `core-clang-debug-san`, `core-gcc-release`, `core-clang-release`):

```bash
CCFGS="core-gcc-debug-san core-clang-debug-san core-gcc-release core-clang-release"
R=plan-start..HEAD

# G-REPO-VERSION
v=$(cat VERSION)
[ "$(build/core-gcc-release/ozen --version)" = "ozen $v" ] && grep -cE "^ *version: *\"$v\"" stdlib/package.ozen && grep -cF "Version: $v" plans/ozen-roadmap.md   # → exit code 0, and the 2 counts are each 1 or more

# G-CORE-BUILD
scripts/build-core.sh                               # → core-build: 4 configs, 0 failed
# G-CORE-FLAGS
for c in $CCFGS; do python3 proto/scripts/check-flags.py --std-gcc c2x --std-clang c23 --extra -Wswitch-enum build/$c/compile_commands.json; done
# G-CORE-C23-SUBSET (compile 12 probes with the local gcc and clang, Emscripten, the NDK and the CI Xcode, and count uses of banned features)
scripts/check-c23-subset.sh core/src cli/src        # → c23: 12 allowed probes ok on <t> toolchains, 0 banned uses
# G-CORE-UNIT
for c in $CCFGS; do ctest --test-dir build/$c --output-on-failure | grep -E '^[0-9]+% tests passed'; done   # → 100% tests passed, 0 tests failed out of <N>
ctest --test-dir build/core-gcc-debug-san -N | grep -E '^Total Tests:'                                        # → Total Tests: <N>
# G-CORE-GOLDEN
for c in $CCFGS; do scripts/run-golden.sh "build/$c/ozen" tests/golden; done   # → golden: <N> passed, 0 failed, 0 timed out
# G-CORE-GRAMMAR
python3 scripts/check-grammar-coverage.py --ozen build/core-gcc-release/ozen core/grammar.ebnf tests/golden   # → grammar: <r> rules, 0 not covered (collects the output of ozen debug ast --rules)
# G-CORE-PURE, NOGLOBAL, NOFLOAT, NOINT128
proto/scripts/check-pure.sh core/src build/core-gcc-release/libozen.a build/core-clang-release/libozen.a
python3 proto/scripts/check-noglobal.py build/core-gcc-release/libozen.a build/core-clang-release/libozen.a
( for f in core/src/*.c; do gcc -std=c2x -O2 -mgeneral-regs-only -Icore/include -Icore/src -c "$f" -o /dev/null || exit 1; done )
for f in core/src/*.c core/src/*.h core/include/*.h; do gcc -x c -fpreprocessed -dD -E -P "$f"; done | grep -wE 'float|double' | wc -l   # → 0
git grep -nwE '__int128|__uint128_t|_BitInt' -- core cli | wc -l                                     # → 0
# G-CORE-ARENA-ACCESS (do not index the arena arrays directly outside arena.h)
git grep -nE 'struct ozni_arena[a-z_]*[[:space:]]*\{' -- core ':!core/src/arena.c' | wc -l   # → 0 (the arena structs are defined only inside arena.c)
git grep -cF 'static_assert(sizeof(ozni_index) == 4' -- core/src/arena.h                        # → 1
# G-CORE-OOM
build/core-gcc-debug-san/ozen_oom_test tests/golden/*/*.ozen
# G-CORE-DIAG-REGISTRY, APPEND
proto/scripts/check-diag-registry.sh --registry core/src/diag_codes.def --names core/tests/diag-names.txt --outputs tests/golden
git log --format= -p "$R" -- core/src/diag_codes.def | grep -E '^-[^-]' | wc -l  # → 0
git rev-list --count "$R" -- core/src/diag_codes.def                              # → 1 or more
# G-CORE-STD-EMBED
cmp <(build/core-gcc-release/ozen debug embedded-std) <(scripts/concat-std.sh stdlib)                # → exit code 0
# G-CORE-PROPS
for t in reindent split_merge meta_ignored monotonic display_only ir_invariant std_copy bundle_roundtrip; do python3 tests/props/$t.py --ozen build/core-gcc-release/ozen; done
# G-CORE-DETERMINISM
for c in $CCFGS; do scripts/digest.sh "build/$c/ozen" tests/golden samples; scripts/digest.sh "build/$c/ozen" tests/golden samples; done | sort -u | wc -l   # → 1
# G-CORE-FUZZ (targets: parse expand typecheck schedule bundle kitchen)
scripts/fuzz-core.sh 600                            # → per target, fuzz-<target>: … 0 crashes, 0 leaks, 0 timeouts
ls build/fuzz-core/artifacts | wc -l                # → 0

# G-TYPE-SEVERITY-TABLE
python3 scripts/check-severity-table.py core/tests/severity-examples.txt core/src/diag_codes.def stdlib   # → severity: 17 examples, 0 mismatched; categories: <c> examples, 0 mismatched (--phase E at the gate of E)
# G-STD-LISTS (after D28)
build/core-gcc-release/ozen debug definitions > build/definitions.json     # S462 (from P10-2 on, the same as ozen build --emit definitions)
python3 tools/stdcheck/lists.py spec/coverage/concept-lists.yaml spec/coverage/concept-relations.yaml build/definitions.json
# G-STD-STATIC
python3 tools/stdcheck/static.py build/definitions.json   # → std-static: <n> definitions, 0 naming, 0 provisional-point, 0 fct-inline, 0 capability-unprovided
# G-STD-PROVENANCE
python3 tools/stdcheck/provenance.py build/definitions.json   # → provenance: <n> values, 0 without source
# G-STD-SOURCES
python3 tools/stdcheck/sources.py build/definitions.json stdlib/sources.ozen spec/sources/search-log.yaml   # → sources: … 0 missing fields, 0 quote mismatches, 0 provisional without search
# G-STD-PHRASES
node renderers/tools/phrases-check.mjs build/definitions.json phrases   # → phrases: <n> keys, 0 missing in ja, 0 missing in en, 0 unknown args
# G-SCHED-CHECK
python3 tools/schedcheck/schedcheck.py build/results/schedules/*.json   # → schedcheck: <n> schedules, 0 violations

# G-API-EXPORTS
nm -g --defined-only build/core-gcc-release/libozen.a | awk 'NF==3{print $3}' | grep -v '^ozni_' | sort -u | tr '\n' ' '   # → recipe_build recipe_default_config recipe_free_string
# G-API-HEADER
for cc in gcc clang; do for std in c17 c23; do [ $cc = gcc ] && [ $std = c23 ] && std=c2x; $cc -std=$std -pedantic-errors -Werror -fsyntax-only -x c core/include/ozen.h || exit 1; done; done
# G-API-SCHEMA
python3 tools/jsoncheck/validate.py schemas build/results/json   # → json: <n> files, 0 invalid
# G-API-SARIF (the SARIF 2.1.0 schema is fetched in P10-3 and placed in schemas/vendor/)
python3 tools/jsoncheck/validate.py --schema schemas/vendor/sarif-2.1.0.json build/results/sarif   # → sarif: <n> files, 0 invalid
# G-CLI-TESTS
scripts/run-cli-tests.sh build/core-gcc-release/ozen tests/cli   # → cli: <n> cases, 0 failed

# G-PLAT-LINUX
nm -D --defined-only build/core-gcc-release/libozen.so | awk 'NF==3{print $3}' | grep -vE '^(_init|_fini)$' | sort | tr '\n' ' '   # → recipe_build recipe_default_config recipe_free_string
bindings/linux/swift-test.sh build/core-gcc-release   # → swift: <k> inputs, 0 mismatches
bindings/android/jvm-test.sh build/core-gcc-release   # → jni: <k> inputs, 0 mismatches
# G-PLAT-WASM
scripts/run-golden.sh 'node build/core-wasm/ozen.js' tests/golden
bindings/wasm/worker-test.sh                          # → worker: <k> inputs, 0 mismatches
[ "$(scripts/digest.sh 'node build/core-wasm/ozen.js' tests/golden samples)" = "$(scripts/digest.sh build/core-gcc-release/ozen tests/golden samples)" ]
# G-PLAT-IOS, G-PLAT-ANDROID (D19. The jobs ios and android of the workflow ci.yml)
# ci.yml does not run on a push that touches only paths-ignore paths (site/ and so on). Walk back from HEAD and use the run of the newest commit that has a ci.yml run
ignored=($(awk '/^    paths-ignore:/ { on = 1; next } on && /^      - / { print ":(exclude,glob)" $2; next } !/^ *#/ { on = 0 }' .github/workflows/ci.yml))
base=$(git log -1 --format=%H HEAD -- . "${ignored[@]}")
for sha in $(git rev-list "$base~1..HEAD"); do id=$(gh run list --workflow ci.yml --commit "$sha" --json databaseId -q '.[0].databaseId'); [ -n "$id" ] && break; done
gh run view "$id" --json conclusion -q .conclusion      # → success
git diff --name-only "$(gh run view "$id" --json headSha -q .headSha)" HEAD -- . "${ignored[@]}"   # → empty output (the run's commit and HEAD differ only in ignored paths)
rm -rf build/ci && gh run download "$id" -D build/ci
for e in ios android; do [ "$(cat build/ci/digest-$e/digest.txt)" = "$(scripts/digest.sh build/core-gcc-release/ozen tests/golden samples)" ] || exit 1; done
unzip -l build/ci/xcframework/Ozen.xcframework.zip | grep -oE '(ios-arm64-simulator|ios-arm64|macos-arm64_x86_64)/' | sort -u | wc -l   # → 3
cat build/ci/swift-macos/result.txt   # → swift: <k> inputs, 0 mismatches
find build/ci/android-libs -name libozen.so | sed 's#.*/android-libs/##; s#/libozen.so##' | sort | tr '\n' ' '   # → arm64-v8a armeabi-v7a x86 x86_64
# G-PLAT-CI
diff <(gh run view "$id" --json jobs -q '.jobs[] | .name + " " + .conclusion' | sort) <(sed 's/$/ success/' ci/jobs.txt | sort)   # → empty output
# G-REPO-TOOLS-CI
python3 scripts/check-ci-tools.py toolchain.txt build/ci/tools-*/tools.txt   # → ci-tools: <n> jobs, 0 mismatched

# G-SAMPLE-CHECK
python3 tests/samples/check.py --ozen build/core-gcc-release/ozen --save build/results/json/samples samples   # → samples: <n> samples, <k> expectations, 0 failed (writes the JSON of each run for rendering)

# G-REND-TESTS
node --test renderers/test/ 2>&1 | grep -E '^# (pass|fail)'      # → # pass <n> / # fail 0
# G-REND-SELFCONTAINED
python3 tools/htmlcheck/selfcontained.py build/results/html       # → selfcontained: <n> files, 0 external references
# G-REND-BROWSER-SAFE
git grep -nE "from ['\"](node:[^'\"]*|fs|path|process|child_process|os)(/[^'\"]*)?['\"]|import\\(['\"](node:|fs|path|os)|require\\(|\\bprocess\\." -- renderers/lib | wc -l   # → 0 (browser-test.sh, which loads the code outside node, is authoritative)
renderers/tools/browser-test.sh                                    # → browser: <k> inputs, 0 mismatches
# G-REND-VENDOR
test -f renderers/vendor/fluent-bundle/LICENSE && python3 tools/vendorcheck.py renderers/vendor/README.md schemas/vendor/README.md   # → vendor: <n> files, 0 mismatched
# G-REND-HTML
python3 tools/htmlcheck/htmlcheck.py build/results/html   # → html: … 0 not well-formed; gantt: … 0 overlaps, 0 out of bounds; form: … 0 without label, 0 quantity without unit
# G-REND-PHRASES-RENDER
node renderers/tools/render-all.mjs --langs ja,en build/results/json   # → render-phrases: <n> samples × 2 languages, 0 fallbacks, 0 fluent errors, 0 unresolved, 0 identifiers
# G-SAMPLE-STD-ONLY
git grep -nE '^[[:space:]]*(local[[:space:]]+)?(ingredient|action|unit|kind|vessel|dirt|trait)[[:space:]]' -- samples | wc -l   # → 0
# G-REPO-NOREPLY (before every push. The personal email is not written in this plan; it is read from the global git config)
git log --all --format='%ae%n%ce' | sort -u | tr '\n' ' '                 # → 146819854+atsuki-seo@users.noreply.github.com
git config --local user.email                                             # → 146819854+atsuki-seo@users.noreply.github.com
git grep -nF "$(git config --global user.email)" -- . | wc -l               # → 0
# G-REPO-CLAUDEMD
grep -c 'docs/' CLAUDE.md; grep -ciE 'japanese|日本語' CLAUDE.md   # → each 1 or more
# G-BUNDLE-STABLE
build/core-gcc-release/ozen bundle samples/menuD --output build/b1.ozen && build/core-gcc-release/ozen bundle samples/menuD --output build/b2.ozen && cmp build/b1.ozen build/b2.ozen   # → exit code 0
```

Each new script comes with a self-test that confirms it fails on deliberately broken input (`scripts/test-<名前>.sh`, `proto/scripts/test-<名前>.sh`) (`G-REPO-SELFTESTS`).

### 5.2 Tests

How the tests are made in general:

- Python uses only the standard `unittest` (pytest is not installed). Run with `python3 -B`. One test method is generated per fixture.
- The `expect` of a fixture is written as a multiset of diagnostics (name, file, YAML path or step position, arguments), and is compared for an exact match with the multiset of the diagnostics that come out. Sample fixtures also compare the state of `states` and `steps`.
- C unit tests are written with a small home-made assert macro, and each test is registered in `ctest` on its own.
- A golden is `proto/tests/golden/<分類>/<名前>.ozen` and the expected output `<名前>.out` (which holds the exit code in its last line `exit <n>`). If the first line of the input is `// args: …`, those arguments are passed. `proto/scripts/run-golden.sh '<実行のコマンド>'` runs them in the same way on native, WASM (`node build/wasm/ozenproto.js`) or the iOS simulator (`xcrun simctl spawn <udid> <実行ファイル>`). Each case runs under `timeout 10`, and a timeout counts as a failure (`golden: <N> passed, 0 failed, 0 timed out`). The results are written to `build/results/golden-<環境>.txt`.
- Expectations are found by a path separate from the implementation (D20): the expectations of golden files and fixtures are written by hand from §4 and committed before the implementation (`G-REPO-RED-FIRST`). Rationals use Python's `fractions.Fraction`, fuel uses the hand calculation of §4.12, the food composition table uses a re-extraction from Excel and 3 entries read by hand, and the lists and relations are a copy of the concept.
- The scripts that run golden files and samples write their output to `build/results/` (`json/`, `schedules/`, `sarif/`, `html/`, one per entry). For a goal that takes `build/results/` as input, n must match this number of entries (an empty folder does not pass).
- `{{STD_VERSION}}` in a golden expectation is replaced by the running script with the value of `VERSION` (D30).
- The match of the bridges (Swift, Java, JS) compares the JSON strings that `recipe_build` returns (the CLI format is not used, S343).
- For the digest, `proto/scripts/digest.sh '<実行のコマンド>' [--save <環境>]` runs all golden inputs and the inputs in `proto/tests/corpus/` (up to 300 chosen from the fuzz corpus in P5-1) in name order, and prints on one line the sha256 of the concatenation of "name, output, exit code". With `--save`, it also writes it to `build/results/digest-<環境>.txt`.
- How the test names in the coverage table map to paths in the repository (used by `scripts/check-plan-tests.sh`):
  - `golden/<分類>/<名前>` → `proto/tests/golden/<分類>/<名前>.ozen` and `.out`
  - `fx/<種類>/<名前>` → `spec/fixtures/<種類>/<名前>.yaml`, or the folder `spec/fixtures/<種類>/<名前>/`
  - `py/<モジュール>.<関数>` → `def <関数>` in `<モジュール>.py` under `tools/`
  - `c/<名前>` → the name of a test shown by `ctest --test-dir build/gcc-debug-san -N`
  - `sh/<ファイル>` → `scripts/<ファイル>` or `proto/scripts/<ファイル>`
  - `cgolden/<分類>/<名前>` → `tests/golden/<分類>/<名前>.ozen` and `.out`, or, for cases that need several files (packages, dependencies, kitchens, conditions), the folder `tests/golden/<分類>/<名前>/` (with `args`, the input files and `expected.out` inside) (through the core CLI. From D on. `run-golden.sh` and `ozen_oom_test` accept both forms)
  - `cunit/<名前>` → the name of a test shown by `ctest --test-dir build/core-gcc-debug-san -N`
  - `prop/<名前>` → `tests/props/<名前>.py`
  - `cli/<名前>` → `tests/cli/<名前>.sh`
  - `sample/<名前>` → `samples/<名前>/expect.yaml`
  - `rtest/<名前>` → `renderers/test/<名前>.test.mjs`
  - A name that contains `*` has "n cases" in the parentheses right after it, and matches exactly n entries (`check-plan-tests.sh` reports an error for a name with `*` and no "n cases")
  - Names written in `scripts/retired-tests.txt` (tests retired by D28) are excluded from the existence check

| Test | What it judges | Main tasks |
| --- | --- | --- |
| `py/test_qty.*` | Accepting and rejecting quantity strings. The expectations are a hand-written table of `Fraction` | P1-1 |
| `fx/vocab-ng/*`, `fx/vocab-ok/*` | Pass small tables with `--vocab`; the multiset of diagnostics exactly matches `expect` | P1-2, P1-3 |
| `fx/sample-ng/*`, `fx/sample-ok/*` | Check samples with the real tables; the multiset of diagnostics and the states exactly match `expect` | P2-1, P2-2 |
| `py/test_vocabcheck.*` | Matching the lists and relations, the number of schema targets, the order and position of diagnostics | P1-2, P1-6, P2-1 |
| `py/test_extract.*` | Extraction from a small xlsx (made with `openpyxl` inside the test), and 3 entries of the real Excel file | P3-2 |
| `c/*` | 128-bit integers, rationals, intervals, UTF-8, the arena (poisoned padding), default limits, balanced allocation | P4-1, P4-2, P4-6, P4-8 |
| `golden/syntax/*` | Syntax, UTF-8, recovery, limits on input and nesting, where `quote` and `$` may go, number size, escapes, reserved words | P4-3 |
| `golden/values/*` | Values and operations, dimensions, ranges, built-in functions, built-in units, how quantities are written | P4-4 |
| `golden/control/*` | Evaluation order, `let`, `if`, `repeat`, `for`, argument counts, annotations, how errors propagate, entry points, overriding arguments | P4-4 |
| `golden/macro/*` | Expansion, hygiene, checks at definition time, expansion history, recursion, body shapes | P4-5 |
| `golden/fuel/*` | The step, depth and generated-node limits exactly and one below, how each node kind is counted, the output when stopped | P4-6 |
| `golden/diag/*` | The format of diagnostics and of the `result` and `stats` lines, byte positions of columns | P4-3 |
| `G-PROTO-RATIONAL-ORACLE` | Turn random expressions (including ones near 2⁶² and division by 0) into `show(<式>)` lines, run them, and compare with the value found with `Fraction` or with `overflow`, `divisionByZero` | P4-4 |
| `G-PROTO-UNIT` | Unit tests | P4-1 to P4-8 |
| `G-PROTO-GOLDEN` | Golden in the 4 configurations | P4-3 to P4-6 |
| `G-PROTO-OOM` | Inject an allocation failure at every position | P4-8 |
| `G-PROTO-DETERMINISM` | Digests for 4 configurations × 2 runs | P4-8 |
| `G-PROTO-FUZZ` | libFuzzer for 600 seconds | P5-1 |
| `G-PROTO-WASM`, `G-PROTO-WASM-BRIDGE`, `G-PROTO-WASM-WORKER` | WASM golden, oracle and digest, the JS bridge, Web Worker | P5-2 |
| `G-PROTO-IOS` | Golden and digest on the iOS simulator, xcframework | P5-3 |
| `sh/test-driver.sh` | The driver's exit codes (0, 1, 2) and usage errors | P4-3 |
| `cgolden/*` | Through the core CLI: syntax, name resolution, packages, type checks, evaluation and macros, diagnostics, check levels, IR, schedules, bundled files. Expectations come from §4 and the counting of §4.12; schedules come from hand-made tables | P7 to P15 |
| `cunit/*` | Arena, interning, span, the flat tables of the IR, freeing in the public API | P7, P9, P10 |
| `prop/*` | Property tests (indentation, extraction, `meta`, monotonicity, display-only information, IR unchanged by cooks and the kitchen, copying `std`, bundling and unbundling). Change the input from outside the implementation and check that the output matches or is contained | P7 to P15 |
| `cli/*` | CLI subcommands, entry points, how the kitchen is found, short options, building a project, `out/` | P10, P14 |
| `sample/*` | Properties of the JSON output of samples (`expect.yaml`). Schedules also go through `tools/schedcheck/` | P12, P13 |
| `rtest/*` | Renderer text, schedule tables, HTML, Gantt charts, shopping lists, phrases, the settings screen | P14 |

Test goals (the text to the right of `→` in a comment is the passing output):

```bash
# G-PROTO-UNIT
for c in $CFGS; do ctest --test-dir build/$c --output-on-failure | grep -E '^[0-9]+% tests passed'; done   # → 100% tests passed, 0 tests failed out of <N>
ctest --test-dir build/gcc-debug-san -N | grep -E '^Total Tests:'                                        # → Total Tests: <N>
# G-PROTO-GOLDEN
for c in $CFGS; do proto/scripts/run-golden.sh build/$c/ozenproto; done            # → golden: <N> passed, 0 failed, 0 timed out
find proto/tests/golden -name '*.ozen' | wc -l                                     # → N
# G-PROTO-RATIONAL-ORACLE
for s in 1 2 3; do python3 proto/tests/oracle/rational_oracle.py --runner build/gcc-debug-san/ozenproto --cases 20000 --seed $s; done   # → oracle: 20000 cases, 0 mismatches
# G-PROTO-OOM (reads the // args: of each input. p is the sum of the allocations counted in runs without injected failures, f is the number of golden files)
build/gcc-debug-san/ozp_oom_test proto/tests/golden/*/*.ozen    # → oom: <f> files, <p> points, 0 failures (no ASan reports)
# G-PROTO-DETERMINISM
for c in $CFGS; do proto/scripts/digest.sh build/$c/ozenproto; proto/scripts/digest.sh build/$c/ozenproto; done | sort -u | wc -l   # → 1
# G-PROTO-FUZZ (-max_total_time=600 -timeout=5 -max_len=65536. Reads libFuzzer's last "Done <e> runs in <t> second(s)")
proto/scripts/fuzz.sh 600                           # → fuzz: <e> execs in <t> s, 0 crashes, 0 leaks, 0 timeouts (t ≥ 600, e ≥ 1)
ls build/fuzz/artifacts | wc -l                     # → 0
# G-PROTO-WASM
proto/scripts/run-golden.sh 'node build/wasm/ozenproto.js'                                        # → golden: <N> passed, 0 failed, 0 timed out
python3 proto/tests/oracle/rational_oracle.py --runner 'node build/wasm/ozenproto.js' --cases 2000 --seed 1   # → oracle: 2000 cases, 0 mismatches
[ "$(proto/scripts/digest.sh 'node build/wasm/ozenproto.js' --save wasm)" = "$(proto/scripts/digest.sh build/gcc-release/ozenproto --save native)" ]   # → exit code 0
# G-PROTO-WASM-BRIDGE
node proto/wasm/bridge-test.mjs                     # → bridge: <k> inputs, 0 mismatches (k ≥ 3, including Japanese and emoji)
# G-PROTO-WASM-WORKER (served with python3 -m http.server --bind 127.0.0.1 and read from the result of google-chrome --headless=new --user-data-dir=<temporary folder> --dump-dom)
proto/scripts/worker-test.sh                        # → worker: <k> inputs, 0 mismatches
# G-PROTO-IOS (when the recommendation of D19 is chosen)
id=$(gh run list --workflow ios-proto.yml --commit "$(git rev-parse HEAD)" --json databaseId -q '.[0].databaseId')
gh run view "$id" --json conclusion,headSha -q '.conclusion + " " + .headSha'   # → success <sha of HEAD>
gh api "repos/{owner}/{repo}/actions/runs/$id/artifacts" -q '.artifacts[].name' | sort | tr '\n' ' '   # → ios-digest xcframework
rm -rf build/ios && gh run download "$id" -n ios-digest -D build/ios && cp build/ios/digest.txt build/results/digest-ios.txt
[ "$(cat build/results/digest-ios.txt)" = "$(cat build/results/digest-native.txt)" ]                 # → exit code 0
gh run download "$id" -n xcframework -D build/ios && unzip -l build/ios/OzenProto.xcframework.zip | grep -oE 'ios-arm64(-simulator)?/' | sort -u | wc -l   # → 2
```

The number of tests grows from N0 = 0 at the starting point (§9.1). `scripts/check-plan-tests.sh` prints the growth per category (`golden= fx= py= c= sh= cgolden= cunit= prop= cli= sample= rtest=`).
A failing test is not loosened; we look for the cause. A mismatch with the spec of §4 is reported to the user, without silently fixing either the test or the implementation. Only when it turns out to be a mistake in the hand calculation of an expectation, we fix the expectation in a commit separate from the implementation code, add that commit to `scripts/red-first-exceptions.txt`, and write the reason in §9.

What the machine covers: the shape of the tables, references, coverage of the lists and of the relations the concept states explicitly, how the type rules work, how the samples pass, the match of the food composition table values, the prototype's syntax, evaluation, hygiene, fuel, diagnostic format, purity, allocation failures and determinism, the match on Linux (gcc, clang), WASM (node and a Chrome Web Worker) and the iOS simulator, fuzzing, and the numbers in the report. What goes to §8: the culinary correctness of vocabulary and values that the concept does not decide, whether the (added) rules fit the design intent, judging whether the writing feel, the amount of bugs and the fuzz findings are acceptable, running on a real iPhone, and the decision on the implementation language of the core.

### 5.3 Coverage table

The S items of §4 and the lists of §4.7 and §4.18 are assigned to a test or to §8 for each viewpoint (normal, boundary and limits, no-effect condition, side effects and cost, time and order, persistence, real path, data completeness, platform path, docs match, fixed premise).
"Persistence" covers, in A and B, the extracted data of the food composition table (S60, S61), and from D on, writing `out/` (S443), the output of `ozen bundle` (S400) and what `ozen unbundle` writes (S401, S444).
"Real path" means that all the table checks are confirmed through the CLI (`vocabcheck.py`), and the prototype through the CLI driver, the JS bridge and the iOS simulator (the command of each goal).

| S item | Viewpoint | Test or goal | How to break it | Main tasks |
| --- | --- | --- | --- | --- |
| S1 | Normal | `G-REPO-SPDX` | Delete the SPDX line from one target file | P0-3 |
| S1 | Boundary | `sh/test-check-spdx.sh` (a file with SPDX only on line 6, a wrong identifier, `$comment` in JSON, an extension out of scope) | grep the whole file instead of the first 5 lines | P0-3 |
| S2 | Normal | `G-REPO-SPDX` | Change the SPDX of `spec/vocab/units.yaml` to `CC-BY-4.0` | P1-4 |
| S3 | Normal | `G-REPO-SPDX` | Change the SPDX of the sample to `MIT OR Apache-2.0` | P2-3 |
| S4 | Normal | `G-REPO-LICENSE-TEXT` | Delete the version line from `LICENSE-APACHE` | P0-3 |
| S4 | Boundary | `G-REPO-LICENSE-TEXT` (no template placeholders remain) | Put the MIT copyright line back to the template `<year> <copyright holders>` | P0-3 |
| S5 | Docs match | `G-REPO-NOTICE` | Delete the source wording from `NOTICE` | P3-2 |
| S5 | Fixed premise | `G-VOCAB-FCT` (checks the sha256 in the README) | Change one character of the sha256 in the README | P3-2 |
| S5 | Data completeness | `G-REPO-NOTICE` (the URL, retrieval date, file name and sha256 lines) | Delete the `- Retrieved:` line from the README | P3-2 |
| S6 | Docs match | `G-REPO-README-LICENSE` | Delete the sentence about how output is treated from the README | P0-3 |
| S6 | Data completeness | `G-REPO-README-LICENSE` (the rows of the category table) | Move `spec/samples/` in the table to the MIT row | P0-3 |
| S7 | Normal | `G-VOCAB-SCHEMA` | Add a misspelled key to `ingredients.yaml` | P1-2 |
| S7 | Data completeness | `py/test_vocabcheck.test_schema_covers_all_files` (the number of files in `spec/vocab/` equals the number checked) | Skip reading one file | P1-2 |
| S7 | No-effect condition | `fx/vocab-ng/schema-unknown-key`, `fx/vocab-ng/schema-wrong-type` | Remove `additionalProperties: false` from the schema | P1-2 |
| S137 | Data completeness | `G-VOCAB-SCHEMA` (exact match of the list of file names) | Rename `vessels.yaml` to `vessel.yaml` | P1-4 |
| S8 | No-effect condition | `py/test_qty.test_rejects_malformed` (`200g`, `1e3 g`, `1/0 g`, `g/ml/s`, `5.. g` become error values and do not crash with an exception), `py/test_qty.test_non_string_is_error` (a value that is not a string) | Let a zero denominator raise `ZeroDivisionError` as is | P1-1 |
| S8 | Normal | `py/test_qty.test_accepts` (compare `200 g`, `0.3..0.5 g`, `1/4 pcs`, `0.6 g/ml`, `15 ml` with `Fraction` values found by hand) | Read decimals as float | P1-1 |
| S9 | No-effect condition | `py/test_qty.test_rejects_unitless` | Let a value without a unit pass as dimensionless | P1-1 |
| S10 | Boundary | `py/test_qty.test_range_bounds` (`5..3 g` is an error, `5..5 g` is a single point) | Change the comparison to `>=` | P1-1 |
| S11 | No-effect condition | `py/test_qty.test_rejects_unknown_unit` | Make an unknown unit dimensionless | P1-1 |
| S12 | Boundary | `fx/vocab-ng/yaml-implicit-bool`, `fx/vocab-ng/yaml-implicit-sexagesimal` | Remove `type: string` from the schema | P1-2 |
| S13 | No-effect condition | `fx/vocab-ng/undefined-*` (per reference kind, 13 cases. Includes `undefined-twice` below) | Skip resolving trait references | P1-3 |
| S13 | Boundary | `fx/vocab-ng/undefined-twice` (the same undefined name in 2 places. Expects 2 findings with different files and paths) | Merge undefined uses of the same name into 1 finding | P1-3 |
| S13 | Data completeness | `G-VOCAB-CHECK` (0 findings with the real tables) | Misspell one capability name in `actions.yaml` | P1-5 |
| S14 | No-effect condition | `fx/vocab-ng/duplicate-across-files` | Check duplicates per file | P1-3 |
| S14 | Boundary | `fx/vocab-ng/duplicate-same-file` (duplicate keys in the same mapping) | Remove the loader that rejects duplicate keys (the last one wins) | P1-3 |
| S14 | Normal | `fx/vocab-ok/same-name-other-namespace` | Merge the namespaces into one | P1-3 |
| S15 | No-effect condition | `fx/vocab-ng/part-trait-clash` | Remove the check | P1-3 |
| S16 | Boundary | `fx/vocab-ng/unit-cycle` (3 levels), `fx/vocab-ng/unit-self-cycle` | Detect cycles only up to 2 levels | P1-3 |
| S17 | No-effect condition | `fx/vocab-ng/unit-dimension-mismatch` | Remove the dimension comparison | P1-3 |
| S18 | Boundary | `fx/vocab-ng/base-unit-missing`, `fx/vocab-ng/base-unit-twice` | Judge by "1 or more" | P1-3 |
| S19 | No-effect condition | `fx/vocab-ng/ordered-axis-duplicate` | Remove the duplicate check | P1-3 |
| S20 | No-effect condition | `fx/vocab-ng/cut-refinement-cycle` | Remove the cycle detection | P1-3 |
| S21 | No-effect condition | `fx/vocab-ng/naming-*` (15 cases. One for each of the 13 name kinds, plus `naming-non-ascii` and `naming-unit-non-ascii`) | Check the convention only for type names | P1-3 |
| S21 | Boundary | `fx/vocab-ok/unit-uppercase` (`L` passes) | Apply the convention to units too | P1-3 |
| S22 | No-effect condition | `fx/vocab-ng/provenance-missing`, `fx/vocab-ng/provenance-unknown-source` | Remove the provenance check | P1-3 |
| S22 | Data completeness | `G-VOCAB-CHECK` (all numeric attributes of the real tables) | Delete `source` from one attribute of the real tables | P1-5 |
| S23 | Boundary | `fx/vocab-ng/provisional-point` (lower bound = upper bound) | Change the comparison to `<=` | P1-3 |
| S23 | Docs match | §8 (Reason a command cannot decide: the rule that holds provisional values as ranges was added by this plan, and the concept has no right answer for whether it fits the intent) | — | — |
| S24 | No-effect condition | `fx/vocab-ng/fct-inline-value` | Remove the ban on `value` from the schema | P1-3 |
| S25 | No-effect condition | `fx/vocab-ng/capability-without-kind` | Remove the check | P1-3 |
| S26 | No-effect condition | `fx/vocab-ng/hazard-rule-not-fixed`, `fx/vocab-ng/allergen-rule-not-fixed` | Exclude `Allergen` from the check | P1-3 |
| S27 | No-effect condition | `fx/vocab-ng/effect-value-out-of-axis` | Remove the check of the axis value range | P1-3 |
| S27 | Boundary | `fx/vocab-ng/requirement-value-out-of-axis` (placed in the second choice of `anyOf`) | Check only the first choice of `anyOf` for requirements | P1-3 |
| S28 | Side effects | `G-VOCAB-READONLY` | Remove `sys.dont_write_bytecode` (`__pycache__` is created) | P1-7 |
| S29 | Data completeness | `G-VOCAB-LISTS` | Delete `tear` from the table | P1-6 |
| S29 | No-effect condition | `py/test_vocabcheck.test_lists_reports_missing` (deleting 1 entry from a temporary copy gives missing 1, adding 1 definition that is not in the list still gives missing 0, and deleting 1 entry while adding 1 unlisted definition of the same kind gives missing 1) | Match by the difference in counts per kind | P1-6 |
| S29 | Boundary | `py/test_vocabcheck.test_lists_owner` (deleting only the beef `belly` gives missing 1) | Match by identifier only, without looking at the owner | P1-6 |
| S138 | Data completeness | `G-VOCAB-LISTS` (values) | Change `tbsp` in `units.yaml` to `16 ml` | P1-6 |
| S138 | Time and order | `py/test_vocabcheck.test_lists_values` (swapping `surface` and `par` of `doneness`, or swapping 2 entries in an alternative order, gives a mismatch) | Compare the order as a set | P1-6 |
| S139 | Data completeness | `G-VOCAB-LISTS` (relations) | Delete `Smelly` from `rawFish` in `dirt.yaml` | P1-6 |
| S139 | No-effect condition | `py/test_vocabcheck.test_relations_mismatch` (add an extra capability to the table, delete one relation) | Compare relations in one direction only | P1-6 |
| S140 | Docs match | `G-VOCAB-LISTS-COPY`, `sh/test-check-lists-copy.sh` | Change `julienne` in `concept-lists.yaml` to `julliene` (it fails even if the table uses the same spelling) | P1-6 |
| S29 | Normal (enough for cooking) | §8 (Reason a command cannot decide: only the concept has the right answer for whether the lists themselves are enough for cooking, and the lists are a copy of the concept, so there is nothing to match against) | — | — |
| S30 | Normal | `fx/sample-ok/initial-state` | Make the initial value of an ordered axis its last value | P2-1 |
| S30 | Boundary | `fx/sample-ok/initial-override` (`initial` of `CurryRoux`) | Ignore `initial` | P2-1 |
| S30 | Docs match | §8 (Reason a command cannot decide: an (added) rule, and the concept has no right answer for whether it fits the intent) | — | — |
| S31 | No-effect condition | `fx/sample-ng/peel-missing` | Make the requirement check always true | P2-1 |
| S32 | Normal | `fx/sample-ok/skin-edible-cut-whole` | Judge `anyOf` as "all" | P2-1 |
| S32 | Docs match | §8 (Reason a command cannot decide: an (added) rule. The answer to D10 stands in for the intent, but a person judges how it feels to write) | — | — |
| S33 | No-effect condition | `fx/sample-ng/requirement-on-unused-axis` | Treat an unused axis as satisfied | P2-1 |
| S33 | Docs match | §8 (Reason a command cannot decide: an (added) rule, and the concept has no right answer for whether it fits the intent) | — | — |
| S34 | Time and order | `fx/sample-ok/ordered-no-regress` (an effect that sets `par` after `done`) | Assign instead of taking the maximum | P2-1 |
| S35 | Normal | `fx/sample-ok/cut-finer` | Follow the relation in the reverse direction | P2-1 |
| S35 | Boundary | `fx/sample-ng/cut-coarser`, `fx/sample-ng/cut-same-style` | Let the same cut pass | P2-1 |
| S36 | Normal | `fx/sample-ok/prep-add` | Replace the set | P2-1 |
| S36 | Boundary | `fx/sample-ok/prep-update-args` (soaking twice) | Keep the old arguments | P2-1 |
| S36 | Docs match | §8 (Reason a command cannot decide: an (added) rule, and the concept has no right answer for whether it fits the intent) | — | — |
| S37 | Side effects | `fx/sample-ok/unmentioned-axes` | Reset axes without an effect to their initial values | P2-1 |
| S38 | No-effect condition | `fx/sample-ng/dimension-mismatch` | Remove the dimension check | P2-1 |
| S39 | No-effect condition | `fx/sample-ng/enum-value` | Remove the enum check | P2-1 |
| S40 | No-effect condition | `fx/sample-ng/undefined-action`, `fx/sample-ng/undefined-type`, `fx/sample-ng/undefined-part` | Ignore undefined names | P2-1 |
| S41 | Boundary | `fx/sample-ng/arity-missing`, `fx/sample-ng/arity-extra`, `fx/sample-ng/unknown-named` | Ignore extra arguments | P2-1 |
| S41 | Normal | `fx/sample-ok/default-param` (omit `size` of `cut`) | Make arguments with a default value required too | P2-1 |
| S42 | No-effect condition | `fx/sample-ng/rebind` | Overwrite the binding | P2-1 |
| S43 | No-effect condition | `fx/sample-ng/double-use` | Remove the record of consumption | P2-1 |
| S43 | Real path | `fx/sample-ng/plate-twice` (twice in plating) | Do not count plating as consumption | P2-2 |
| S44 | Normal | `fx/sample-ok/leftover-info` (1 info, 0 errors) | Make leftovers an error | P2-1 |
| S45 | Normal | `fx/sample-ok/list-sugar` (`simmer([a, b])` and `simmer(combine([a, b]))` give the same result) | Use only the first element of the list | P2-2 |
| S45 | Docs match | §8 (Reason a command cannot decide: an (added) rule. The answer to D11 stands in for the intent, but a person judges how it feels to write) | — | — |
| S46 | Normal | `fx/sample-ok/combine-doneness-min`, `fx/sample-ok/combine-allergen-union` | Change the minimum to the maximum, remove the union of traits | P2-2 |
| S46 | Real path | `fx/sample-ng/mixture-hazard` (mix raw pork and plate it without heating) | Remove the union of `Hazard` | P2-2 |
| S46 | Side effects | `fx/sample-ok/mixture-axes` (`Mixture` has only `doneness` and `prep`) | Carry over `browning` | P2-2 |
| S47 | Normal | `fx/sample-ok/combine-mass-sum` (onion `2 pcs` + roux `100 g` + water `800 ml`. The expectation is a range found by hand from the table's typical weight and density range) | Add only the numbers without converting (gives `902`) | P2-2 |
| S47 | Boundary | `fx/sample-ok/combine-amount-unknown` | Treat what cannot be converted as 0 g | P2-2 |
| S47 | Docs match | §8 (Reason a command cannot decide: an (added) rule, and the concept has no right answer for whether it fits the intent) | — | — |
| S48 | Normal | `fx/sample-ok/output-type-action`, `fx/sample-ok/output-allergen-inherit` | Keep the input type | P2-2 |
| S48 | Docs match | §8 (Reason a command cannot decide: an (added) rule, and the concept has no right answer for whether it fits the intent) | — | — |
| S49 | Normal | `fx/sample-ok/variant-first` | Take the union of `uses` of all candidates | P2-2 |
| S50 | Normal | `fx/sample-ok/recipe-retype` | Do not replace it | P2-2 |
| S50 | Docs match | §8 (Reason a command cannot decide: an (added) rule, and the concept has no right answer for whether it fits the intent) | — | — |
| S51 | No-effect condition | `fx/sample-ng/recipe-output-mismatch` | Remove the type comparison | P2-2 |
| S51 | Docs match | §8 (Reason a command cannot decide: an (added) rule, and the concept has no right answer for whether it fits the intent) | — | — |
| S52 | Normal | `fx/sample-ok/menu-make` | Bind the initial state | P2-2 |
| S53 | No-effect condition | `fx/sample-ng/undercooked-pork` | Remove the safety check | P2-2 |
| S53 | Boundary | `fx/sample-ng/undercooked-par` (`par` is an error), `fx/sample-ok/cooked-pork` (`done` passes) | Change the comparison to `>` | P2-2 |
| S53 | No-effect condition | `fx/sample-ok/raw-edible-buri` (`RawEdible` passes raw) | Remove the `RawEdible` exception | P2-2 |
| S54 | No-effect condition | `fx/sample-ng/liquid-flat-plate`, `fx/sample-ok/dry-flat-plate` | Remove the check | P2-2 |
| S54 | Boundary | `fx/sample-ng/liquid-alt-flat` (`on: [DeepPlate, FlatPlate]`) | Check only the first vessel | P2-2 |
| S55 | Normal | `fx/sample-ok/requires-union` | Output only `uses` of the last action | P2-2 |
| S56 | Time and order | `py/test_vocabcheck.test_diag_order_and_location` (errors at steps 2 and 4 come out in the order and position of the text. The name of the step 2 diagnostic comes later in name order than that of step 4) | Sort diagnostics by name | P2-1 |
| S57 | Normal | `G-VOCAB-SAMPLE` | Delete the `peel` step from the sample | P2-3 |
| S57 | Data completeness | `G-VOCAB-SAMPLE` (checks all of the requirements, infos, plated values and per-step states and traits in `expect`. k is the number in §9.2) | Delete the `doneness` effect of `simmer` in `actions.yaml` | P2-3 |
| S57 | Time and order | `G-VOCAB-SAMPLE` (`browning` of `fried` in `expect.steps`) | Delete the `browning` effect of `fry` in `actions.yaml` | P2-3 |
| S57 | Normal (correct as cooking) | §8 (Reason a command cannot decide: there is no oracle that judges whether the requirements and effects of the actions in the tables are correct as cooking) | — | — |
| S58 | Normal | `G-VOCAB-FCT` | Change one value in `refuse.csv` | P3-2 |
| S58 | Boundary | `py/test_extract.test_reads_fixture_xlsx` (a small xlsx made inside the test. Includes blanks, `Tr` and values in parentheses) | Make values in parentheses empty | P3-2 |
| S58 | Fixed premise | `py/test_extract.test_matches_real_xlsx` (the header strings written in §9.2 and 3 entries read by hand. Prints skip if the Excel file is absent), `py/test_extract.test_rejects_wrong_headers` | Shift the read columns by one | P3-2 |
| S59 | No-effect condition | `fx/vocab-ng/fct-unknown-food` | Remove the food number lookup | P3-3 |
| S60 | Side effects | `G-VOCAB-FCT-STABLE` | Write in the iteration order of a `set` (it changes per run with `PYTHONHASHSEED`) | P3-2 |
| S141 | Data completeness | `G-VOCAB-FCT` (order ok, refs ok), `py/test_extract.test_writes_sorted_csv_and_checks` | Also write rows for food numbers that the tables do not reference | P3-3 |
| S61 | Fixed premise | `G-VOCAB-FCT` | Change the sha256 in the README | P3-2 |
| S465 | Data completeness | `G-VOCAB-SOURCES`, `fx/vocab-ng/primary-missing-locator`, `fx/vocab-ng/primary-quote-mismatch` | Write a value that differs from the transcription | P3-6 |
| S466 | Data completeness | `G-VOCAB-SOURCES`, `fx/vocab-ng/provisional-without-search` | Delete the search record | P3-6 |
| S467 | No-effect condition | `G-VOCAB-SOURCES` (pdf in repo) | Commit the PDF of a source document | P3-6 |
| S468 | Data completeness | `G-STD-SOURCES` | Delete the transcription from one value of `std` | P7-4 |
| S465 | Normal (whether the transcription matches the source) | §8 (Reason a command cannot decide: the machine confirms that the transcription and the value match, but whether the transcription matches the source document is confirmed by a person who opens the document, because the documents are not put in the repository) | — | — |
| S469 | Data completeness | `G-VOCAB-PRIORWORK` (elements from all 4 sources, pages present) | Delete the elements of one source | P3-7 |
| S469 | Normal (whether the extracted elements match the source) | §8 (Reason a command cannot decide: whether the extracted elements match the source document is confirmed by a person who opens the document, because the documents are not put in the repository) | — | — |
| S470 | Data completeness | `G-VOCAB-PRIORWORK` (unmapped), `sh/test-priorwork.sh` | Delete one row from the mapping table | P3-7 |
| S471 | Normal | `fx/sample-ok/blend-single-pureed` | Use the new value for even a single item | P2-2 |
| S472 | Normal | `fx/sample-ok/blend-list-mixture` | Use only the first element of the list | P2-2 |
| S473 | No-effect condition | `fx/sample-ng/cut-after-pureed` | Leave `pureed` out of the fineness relation | P2-2 |
| S471 | Docs match | §8 (Reason a command cannot decide: the rule that adds `pureed` was added by this plan, and a person judges whether it is reasonable as cooking) | — | — |
| S474 | Normal | `cgolden/state/blend-*` (3 cases. A single item, a list, `cut` after `pureed`) | Treat `blend` the same as `cut` in the core | P8-2 |
| S475 | No-effect condition | `G-REPO-NOREPLY` | In a throwaway clone, add a commit whose author is the personal email | P0-3 |
| S476 | Normal | `cunit/api_options_merge` | Use only the first declaration | P10-1 |
| S62 | No-effect condition | `golden/syntax/utf8-invalid` | Remove the UTF-8 check | P4-3 |
| S63 | No-effect condition | `golden/syntax/ident-nonascii` | Allow UTF-8 in identifiers | P4-3 |
| S64 | Normal | `golden/syntax/string-utf8-passthrough` | Rewrite non-ASCII bytes | P4-3 |
| S64 | Platform path | `G-PROTO-WASM-BRIDGE` (pass a JS string as UTF-8) | Pass the UTF-16 length in the bridge | P5-2 |
| S65 | Normal | `golden/syntax/recover-multiple` (3 errors) | Stop at the first error | P4-3 |
| S142 | No-effect condition | `golden/syntax/bad-escape` | Pass unknown escapes through as characters | P4-3 |
| S143 | Normal | `golden/syntax/newline-in-parens` | Treat a newline inside parentheses as a separator | P4-3 |
| S144 | No-effect condition | `golden/syntax/reserved-words` (16 cases. In one file, using recovery) | Accept `repeat` as an identifier | P4-3 |
| S66 | Boundary | `golden/syntax/input-too-large`, `golden/syntax/input-exact-limit` (`--limit-input`) | Change the comparison to `>=` | P4-3 |
| S67 | Boundary | `golden/syntax/nesting-too-deep`, `golden/syntax/nesting-exact` (`--limit-depth 8`) | Do not count the parser depth | P4-3 |
| S67 | Side effects | `golden/syntax/deep-unary` (with the default limits: 1,000 levels of `-`, 1,000 levels of `not`, 300 levels of `else if`) | Do not count unary recursion in the depth (the stack overflows and ASan reports it) | P4-3 |
| S68 | No-effect condition | `golden/syntax/quote-outside-macro`, `golden/syntax/nested-quote` | Remove the check of where `quote` may go | P4-3 |
| S69 | No-effect condition | `golden/syntax/splice-outside-quote` | Remove the check | P4-3 |
| S70 | Normal | `golden/syntax/juxtaposed-sum` | Drop the second of the juxtaposed quantities | P4-4 |
| S70 | No-effect condition | `golden/syntax/juxtaposed-mismatch` (`1 h 30 g`) | Remove the dimension check | P4-4 |
| S71 | Boundary | `golden/syntax/number-too-large`, `golden/syntax/number-int64-max`, `golden/syntax/decimal-den-overflow` | Shift the limit comparison by 1 | P4-3 |
| S72 | Normal | `golden/values/kinds` (write the 6 kinds of values as arguments. Syntax trees cannot be written) | Allow syntax trees to appear as arguments | P4-4 |
| S73 | Normal | `golden/values/decimal-exact` | Read decimals as binary approximations | P4-4 |
| S74 | Normal | `golden/values/add-same-dimension` (`1 kg + 500 g` is `1500 g`) | Do not multiply by the unit factor | P4-4 |
| S74 | No-effect condition | `golden/values/add-dimension-mismatch` | Remove the dimension check | P4-4 |
| S75 | Normal | `golden/values/div-same-dimension` (`600 g / 250 g` is `12/5`) | Give a division the dimension of its left side | P4-4 |
| S75 | No-effect condition | `golden/values/mul-dimensioned`, `golden/values/div-by-dimensioned` (`3 / 2 g`) | Let the multiplication of dimensions pass | P4-4 |
| S75 | Normal | `golden/values/mul-dimensionless` (`2 * 3 g` and `3 g / 2` stay in `g`) | Make the result dimensionless when multiplying by a dimensionless value | P4-4 |
| S76 | Normal | `golden/values/range-add`, `golden/values/range-sub`, `golden/values/range-scale-negative` | Make subtraction `[a−c, b−d]` | P4-4 |
| S76 | No-effect condition | `golden/values/range-mul-range`, `golden/values/range-point-div`, `golden/values/range-compare`, `golden/values/range-ceil`, `golden/values/range-minmax` | Compute with one end of the range | P4-4 |
| S77 | Boundary | `G-PROTO-RATIONAL-ORACLE` (random expressions including values near 2⁶²) | Compute intermediate values in int64 | P4-4 |
| S77 | Boundary | `c/rational_overflow_bounds` (exactly ±(2⁶³−1) passes, 2⁶³ and −2⁶³ give `overflow`) | Allow the negative side down to INT64_MIN | P4-2 |
| S78 | No-effect condition | `golden/values/div-zero`, `G-PROTO-RATIONAL-ORACLE` (includes expressions that divide by 0) | Remove the zero check | P4-4 |
| S79 | Normal | `golden/values/compare` | Mix up `<` and `<=` | P4-4 |
| S79 | No-effect condition | `golden/values/compare-mismatch` | Let a comparison of different dimensions pass | P4-4 |
| S80 | Side effects | `golden/values/short-circuit` (check the fuel with `--stats`; the error on the right side does not appear) | Evaluate both sides | P4-4 |
| S81 | Normal | `golden/values/ref-portion`, `golden/values/ref-ratio` | Add the portion ratio instead of multiplying by it | P4-4 |
| S81 | No-effect condition | `golden/values/ref-ratio-unknown` | Make a reference without a quantity 0 | P4-4 |
| S82 | Normal | `golden/values/builtins` (5 functions. Includes `ceil` and `floor` of negative values) | Make `floor` truncate toward 0 | P4-4 |
| S82 | No-effect condition | `golden/values/builtin-errors` | Remove the argument checks | P4-4 |
| S83 | No-effect condition | `golden/values/unknown-unit` | Make an unknown unit dimensionless | P4-4 |
| S84 | Time and order | `golden/control/order-nested-calls` (`f(peel(a), peel(b))`, and the order among named arguments) | Evaluate arguments from the right | P4-4 |
| S85 | No-effect condition | `golden/control/let-rebind`, `golden/control/let-inner-rebind` | Do not search the outer scope | P4-4 |
| S85 | Normal | `golden/control/shadow-global` (hide an action name with a local name) | Forbid hiding top-level names too | P4-4 |
| S86 | Normal | `golden/control/if-else-chain` | Ignore `else if` | P4-4 |
| S86 | No-effect condition | `golden/control/if-nonbool`, `golden/control/if-false-noelse` | Treat a non-boolean condition as true | P4-4 |
| S87 | Boundary | `golden/control/repeat-zero`, `golden/control/repeat-count-unit` (`3 pcs`) | Evaluate once when the count is 0 | P4-4 |
| S87 | No-effect condition | `golden/control/repeat-nonint`, `golden/control/repeat-negative`, `golden/control/repeat-range` | Truncate decimals and let them pass | P4-4 |
| S88 | Normal | `golden/control/for-list`, `golden/control/for-empty` | Skip the last element | P4-4 |
| S88 | No-effect condition | `golden/control/for-nonlist` | Treat a non-list as a list of 1 element | P4-4 |
| S89 | Time and order | `golden/control/loop-scope-fresh` | Do not create a scope per iteration | P4-4 |
| S90 | No-effect condition | `golden/control/action-arity`, `golden/control/named-arg-unknown`, `golden/control/named-arg-duplicate` | Do not count named arguments | P4-4 |
| S91 | Normal | `golden/control/annotation-attach` | Attach the annotation to the last step that came out | P4-4 |
| S91 | No-effect condition | `golden/control/annotation-non-call`, `golden/control/annotation-macro-outer` (`m(x) @unattended`), `golden/control/annotation-outer-error` (the outermost call is an error) | Attach the annotation to the last step that came out | P4-4 |
| S92 | Side effects | `golden/control/error-poison` (1 error. Dependent calls do not come out, independent statements do) | Report diagnostics even for expressions that use the error | P4-4 |
| S93 | Time and order | `golden/fuel/stop-keeps-prior-output` | Discard the steps when stopped | P4-6 |
| S94 | Normal | `golden/control/entry-single`, `golden/control/entry-select` | Use the first recipe | P4-4 |
| S94 | No-effect condition | `golden/control/entry-ambiguous` (the candidates are in declaration order. The declaration order differs from name order) | Pick the first when ambiguous, sort the candidates by name | P4-4 |
| S95 | Normal | `golden/control/param-override` | Ignore the override | P4-4 |
| S95 | No-effect condition | `golden/control/param-unknown` | Ignore unknown names | P4-4 |
| S95 | Side effects | `golden/fuel/param-steps` (check the steps of the override expression with `--stats`) | Do not count the override expression | P4-6 |
| S96 | Side effects | `golden/macro/args-unevaluated` (splicing twice gives 2 steps, 0 times gives 0) | Evaluate arguments at call time | P4-5 |
| S96 | Normal | `golden/macro/expand-basic` | Evaluate the expansion at the definition site | P4-5 |
| S97 | Normal | `golden/macro/hygiene-tmp`, `golden/macro/hygiene-two-expansions` | Do not add marks | P4-5 |
| S98 | No-effect condition | `golden/macro/hygiene-free-name` (not captured by the caller's local `peel`) | Resolve free names at the caller | P4-5 |
| S99 | Normal | `golden/macro/splice-caller-scope` | Add the expansion mark to spliced names | P4-5 |
| S100 | Normal | `golden/macro/bind-caller-name` | Add the expansion mark to the name of `let $x` | P4-5 |
| S101 | No-effect condition | `golden/macro/bind-non-name` | Remove the check | P4-5 |
| S102 | No-effect condition | `golden/macro/undefined-in-macro-uncalled`, `golden/macro/param-without-dollar` | Remove the check at definition time | P4-5 |
| S103 | No-effect condition | `golden/macro/macro-arity` | Ignore extra arguments | P4-5 |
| S104 | Normal | `golden/macro/diag-trail-nested` (2 levels of expansion) | List the expansion history from the outside | P4-5 |
| S105 | Normal | `golden/macro/diag-in-spliced-arg` | Put the position of a spliced node inside the macro | P4-5 |
| S106 | Normal | `golden/macro/recursive-terminating` | Forbid recursive calls | P4-5 |
| S106 | Boundary | `golden/macro/recursive-infinite` (stops at a small limit) | Remove the limit check (it fails by the 10-second timeout of `run-golden.sh`) | P4-6 |
| S145 | No-effect condition | `golden/macro/body-not-quote` | Remove the check of the body shape | P4-5 |
| S107 | Boundary | `golden/fuel/steps-exact`, `golden/fuel/steps-short` | Do not count literals | P4-6 |
| S107 | Data completeness | `golden/fuel/count-each-node` (`--stats` of an input that contains all 14 node kinds of §4.12) | Do not count the items of `ingredients` | P4-6 |
| S107 | Normal | `golden/fuel/steps-default` (`repeat 1000000 { let x = 1 }` stops with the default limit at the `let` of the 1,000,001st node) | Change the default value | P4-6 |
| S108 | Boundary | `golden/fuel/depth-exact`, `golden/fuel/depth-short` | Do not count macro calls in the depth | P4-6 |
| S109 | Boundary | `golden/fuel/nodes-exact`, `golden/fuel/nodes-short` | Do not count steps as generated nodes | P4-6 |
| S110 | Boundary | `golden/fuel/*-exact` (3 cases) and `golden/fuel/*-short` (3 cases) | Change the limit comparison to `>=` | P4-6 |
| S111 | Normal | `c/limits_defaults` | Make the default depth 128 | P4-6 |
| S112 | Normal | `golden/diag/format` (a diagnostic with arguments and expansion history) | Make columns start at 0 | P4-3 |
| S112 | Boundary | `golden/diag/column-bytes` (`"玉ねぎ🧅"` before the error) | Count columns in characters | P4-3 |
| S113 | Data completeness | `G-PROTO-DIAG-REGISTRY` | Write the same name twice in the registry | P4-1 |
| S146 | Data completeness | `G-PROTO-DIAG-REGISTRY` (unlisted, bad codes), `sh/test-check-diag-registry.sh` | Delete `SYN:badEscape` from the registry, change a code to `SY001` | P4-3 |
| S147 | Data completeness | `G-PROTO-DIAG-REGISTRY` (unused) | Delete `golden/control/annotation-non-call` (`TYP:annotationTarget` no longer appears anywhere) | P4-6 |
| S113 | Time and order | `G-PROTO-DIAG-APPEND` | Rewrite an existing line of the registry and commit it | P4-1 |
| S114 | Normal | `golden/diag/result-line`, `golden/diag/stats-line` | Miscount the number of steps | P4-3 |
| S115 | Normal | `golden/values/render-quantities` | Do not reduce fractions | P4-4 |
| S115 | Data completeness | `golden/values/builtin-units` (write each of the 12 built-in units as `1 <単位>`; they come out in the base unit) | Make the factor of `tsp` 15 | P4-4 |
| S116 | No-effect condition | `G-PROTO-EXPORTS` | Remove the prefix of one internal function | P4-1 |
| S117 | No-effect condition | `G-PROTO-PURE`, `sh/test-check-pure.sh` (fails with a small object that calls `fopen` and with a source that includes `<assert.h>`) | Put one `printf` in the core, put one `assert` in it | P4-8 |
| S118 | No-effect condition | `G-PROTO-NOGLOBAL`, `sh/test-check-noglobal.sh` (an object with only `.data.rel.ro` passes, one with a read-write variable fails) | Add a read-write `static int n;` (`return ++n;`) | P4-8 |
| S119 | No-effect condition | `G-PROTO-NOFLOAT` | Compute ratios with `double` | P4-8 |
| S120 | Side effects | `c/alloc_balance` (0 unfreed for all golden inputs) | Do not free the diagnostics array | P4-8 |
| S121 | Boundary | `G-PROTO-OOM` (fail at every allocation position; succeed at the total count + 1) | Dereference NULL after an allocation failure | P4-8 |
| S148 | Side effects | `c/arena_poison_detects` (`WILL_FAIL` in ctest. Only in the ASan configurations) | Do not poison the arena padding | P4-1 |
| S149 | No-effect condition | `G-PROTO-NOINT128` | Write the 128-bit multiplication with `__int128` | P4-2 |
| S122 | Platform path | `G-PROTO-DETERMINISM` (gcc, clang × Debug, Release × 2 runs) | Walk a hash table seeded with `&local` to output candidate names | P4-8 |
| S123 | Real path | `sh/test-driver.sh` (exit codes 0, 1, 2, an unknown option, an unreadable file, 2 when the result allocation fails with `--fail-alloc-at`) | Return 0 even for `result error`, return 1 for NULL | P4-3 |
| S124 | Normal | `G-PROTO-BUILD` | Add code that warns about a sign conversion | P4-1 |
| S124 | Fixed premise | `G-PROTO-FLAGS`, `sh/test-check-flags.sh` | Turn `CMAKE_C_EXTENSIONS` ON (it becomes `-std=gnu17`) | P4-1 |
| S124 | Normal (writing feel) | §8 (Reason a command cannot decide: the standard for whether the writing feel and the amount of bugs are acceptable is subjective, and the line counts and defect counts in the report are only material for the judgment) | — | — |
| S125 | Side effects | `G-PROTO-UNIT`, `G-PROTO-GOLDEN`, `G-PROTO-OOM` (the ASan and UBSan configurations) | Shift the range check of the arena arrays by 1 (ASan reports it through the poisoned padding of S148) | P4-9 |
| S126 | Boundary | `G-PROTO-FUZZ` | Remove the check for the end of a string | P5-1 |
| S126 | Normal (manageable or not) | §8 (Reason a command cannot decide: the standard for "manageable" is subjective, and the counts in the report are only material for the judgment) | — | — |
| S127 | Platform path | `G-PROTO-WASM` | Change the quantity formatting branch only in the WASM configuration | P5-2 |
| S127 | Boundary | `G-PROTO-WASM` (the rational oracle on WASM with 1 seed) | Assume `long` is 64 bits in 128-bit arithmetic (it is 32 bits on wasm32) | P5-2 |
| S128 | Platform path | `G-PROTO-WASM-BRIDGE` | Pass `str.length` as the length in the bridge | P5-2 |
| S150 | Platform path | `G-PROTO-WASM-WORKER`, `sh/test-worker-test.sh` | Pass `str.length` as the length inside the Worker | P5-2 |
| S129 | Platform path | `G-PROTO-IOS` | Change the quantity formatting branch only in the simulator configuration | P5-3 |
| S129 | Platform path (real device) | §8 (Reason a command cannot decide: GitHub Actions runners have no real iPhone, and a real device cannot be connected from the local Linux machine) | — | — |
| S130 | Normal | `G-PROTO-IOS` (the list of artifacts, and `ios-arm64/` and `ios-arm64-simulator/` inside the xcframework) | Make the xcframework without the static library for devices | P5-3 |
| S131 | Fixed premise | `G-REPO-TOOLS`, `sh/test-check-tools.sh` (fails with a `toolchain.txt` whose versions were rewritten) | Compare only the major version | P0-4 |
| S131 | Real path | `G-PROTO-COMPILERS`, `sh/test-check-compilers.sh` | Build the clang configuration with `/usr/bin/cc` (gcc) | P4-1 |
| S131 | Platform path | `G-PROTO-IOS` (the workflow compares `xcodebuild -version` with `toolchain.txt` and fails if they differ) | Remove the Xcode version setting from the workflow | P5-3 |
| S132 | Docs match | `G-PROTO-REPORT` | Delete one heading | P6-2 |
| S133 | Docs match | `G-PROTO-REPORT` | Change one value in the line count table | P6-2 |
| S134 | Docs match | `G-PROTO-REPORT` | Change one character of the digest | P6-2 |
| S135 | Docs match | `G-PROTO-REPORT`, `sh/test-check-report.sh` | Change the defect count by one | P6-2 |
| S135 | Data completeness | `G-PROTO-REPORT` (the commit of each row in `defects.tsv` is an ancestor, and all `fix` commits exist) | Delete one row from `defects.tsv` | P6-2 |
| S136 | Docs match | `G-PROTO-README-EXAMPLE` | Change one line of the example output in the README | P6-1 |
| S151 | No-effect condition | `G-CORE-C23-SUBSET`, `sh/test-check-c23-subset.sh` (fails with a source that contains `#embed`) | Include `<stdckdint.h>` in the core | P7-1 |
| S151 | Platform path | `G-CORE-C23-SUBSET` (probes with the local gcc and clang, Emscripten, the NDK and the CI Xcode) | Remove `constexpr` from the probes (it is no longer confirmed that Xcode supports it) | P11-3 |
| S152 | No-effect condition | `G-CORE-ARENA-ACCESS`, `cunit/arena_poison_detects`, `cunit/index_is_32bit` | Copy the arena struct definition into `parse.c` and index it directly | P7-2 |
| S153 | Normal | `cunit/intern_same_id` | Give a new ID every time, even for the same spelling | P7-2 |
| S154 | Data completeness | `cunit/span_nesting` (parse all golden inputs; the span of every node is inside the input and inside the parent's span) | Make the span of a binary operation only the left child | P7-3 |
| S155 | Normal | `cgolden/macro/trail-nested` | Do not attach the expansion history | P8-7 |
| S156 | Normal | `cgolden/syntax/recover-multiple` | Stop at the first error | P7-3 |
| S157 | Data completeness | `G-CORE-GRAMMAR` | Add one grammar rule without adding a golden | P7-3 |
| S158 | Normal | `cgolden/decl/*` (per declaration kind, 27 cases. The 26 kinds of S158 and `source` of §4.20.1) | Skip reading the `rule` declaration | P7-3 |
| S159 | Normal | `cgolden/typeargs/*` (per writing form, 7 cases) | Read `has` as `:` | P7-3 |
| S160 | Normal | `G-CORE-PROPS` (`prop/reindent`) | Separate statements by indentation | P7-3 |
| S161 | No-effect condition | `cgolden/names/case-insensitive-free` | Look up capitalized names as types first | P7-4 |
| S162 | Normal | `cgolden/modules/name-from-path` | Do not attach the package name | P7-4 |
| S163 | No-effect condition | `cgolden/modules/top-folder-clash` | Remove the ambiguity check | P7-4 |
| S164 | Normal | `cgolden/modules/import-forms` (3 forms) | Ignore `as` | P7-4 |
| S164 | No-effect condition | `cgolden/modules/import-star` | Accept `*` | P7-4 |
| S165 | Normal | `cgolden/modules/prelude-implicit` | Do not import the prelude | P7-4 |
| S166 | Boundary | `cgolden/modules/ambiguous-on-use`, `cgolden/modules/ambiguous-unused-ok` | Make it an error at import time | P7-4 |
| S167 | Normal | `cgolden/modules/local-overrides-import` | Do not report the override info | P7-4 |
| S168 | No-effect condition | `cgolden/modules/import-local` | Ignore `local` | P7-4 |
| S169 | No-effect condition | `cgolden/modules/recipe-imports-menu`, `cgolden/modules/cycle` | Remove the cycle detection | P7-4 |
| S170 | No-effect condition | `cgolden/modules/recipe-names-kitchen` | Let the kitchen reference pass | P7-4 |
| S171 | Normal | `cgolden/modules/forward-reference` | Resolve declarations from top to bottom | P7-4 |
| S172 | Normal | `cgolden/modules/entry-*` (3 cases. One menu, several, dishes only) | Treat the case with dishes only as having no entry point | P7-4 |
| S173 | Normal | `G-CORE-PROPS` (`prop/split_merge`) | Mix the name of the extracted module into the seed of the IR IDs | P9-1 |
| S174 | No-effect condition | `cgolden/packages/unknown-field` | Ignore unknown fields | P7-4 |
| S175 | No-effect condition | `cgolden/packages/std-reserved` (package, module, bundled file) | Check `std` only for packages | P7-4 |
| S176 | Boundary | `cgolden/packages/requires-newer-minor`, `cgolden/packages/requires-other-major`, `cgolden/packages/requires-ok` | Do not compare the minor version | P7-4 |
| S177 | No-effect condition | `cgolden/packages/two-versions` | Remove the check | P7-4 |
| S178 | No-effect condition | `cgolden/packages/dep-diag-suppressed`, `cgolden/packages/dep-saf-shown` | Suppress `SAF` of dependencies too | P8-8 |
| S179 | Fixed premise | `G-CORE-STD-EMBED` | Change `stdlib/` without regenerating | P7-4 |
| S180 | Boundary | `cgolden/limits/*` (8 cases. Exactly at and 1 over each of the 4 limits), `G-CORE-FUZZ` | Change the limit comparison to `>=` | P7-5 |
| S181 | No-effect condition | `G-CORE-PURE` | Put `fopen` in the core | P7-5 |
| S182 | No-effect condition | `G-CORE-NOGLOBAL` | Add a read-write `static int` | P7-5 |
| S183 | No-effect condition | `G-CORE-NOFLOAT` | Compute ratios with `double` | P7-5 |
| S184 | Boundary | `G-CORE-OOM` | Dereference NULL after an allocation failure | P7-5 |
| S185 | Platform path | `G-CORE-DETERMINISM`, the digests of `G-PLAT-*` | Walk the name table in the order of address hashes (it changes per run with ASLR) | P7-5 |
| S186 | No-effect condition | `cgolden/dim/add-mismatch` | Remove the dimension check | P8-1 |
| S187 | Boundary | `cgolden/dim/density-convert`, `cgolden/dim/density-missing` | Assume 1 g/ml when there is no density | P8-1 |
| S188 | Boundary | `cgolden/dim/any-amount-warn`, `cgolden/dim/any-amount-strict` | Ignore `--strict` | P8-1 |
| S189 | No-effect condition | `cgolden/dim/pinch-strict-ok` | Treat units with a range as vague too | P8-1 |
| S190 | Normal | `cgolden/dim/deny-strict-annotation` | Ignore the scope of the annotation | P8-8 |
| S191 | Normal | `cgolden/dim/strict-group-extended` | Fix the contents of the group | P8-8 |
| S192 | No-effect condition | `cgolden/dim/asm-strict-info` | Raise assumptions to errors too | P8-8 |
| S193 | Normal | `cgolden/dim/count-unit-weight`, `cgolden/dim/rational-quarter` | Round ¼ to a decimal | P8-1 |
| S194 | No-effect condition | `cgolden/dim/unit-cycle`, `cgolden/dim/base-unit-redefine` | Remove the cycle detection | P8-1 |
| S195 | Normal | `cgolden/dim/cup-jp`, `cgolden/dim/cup-us` | Make `cup` a single unit | P8-1 |
| S196 | No-effect condition | `cgolden/state/peel-missing` | Remove the state requirement check | P8-2 |
| S197 | Normal | `cgolden/state/boil-then-peel`, `cgolden/state/grill-then-cut` | Hold the state as a single line of transitions | P8-2 |
| S198 | Boundary | `cgolden/state/ordered-ge`, `cgolden/state/ordered-no-regress` | Assign the value | P8-2 |
| S199 | Boundary | `cgolden/state/cut-finer`, `cgolden/state/cut-coarser` | Follow the fineness relation in reverse | P8-2 |
| S200 | Normal | `cgolden/state/prep-set-args` | Replace the set | P8-2 |
| S201 | Normal | `cgolden/state/user-axis` | Do not check the writer's axes | P8-2 |
| S202 | Normal | `cgolden/state/omit-axis` | Make unused axes required | P8-2 |
| S203 | Normal | `cgolden/state/doneness-browning-independent` | Also advance doneness with the browning effect | P8-2 |
| S204 | Normal | `cgolden/state/unwritten-axis-inherit` | Reset unwritten axes to their initial values | P8-2 |
| S205 | No-effect condition | `cgolden/state/axis-unreachable`, `cgolden/state/axis-contradiction` | Remove the check at definition time | P8-2 |
| S206 | No-effect condition | `cgolden/safety/undercooked-pork` | Remove the `SAF` check | P8-3 |
| S207 | No-effect condition | `cgolden/safety/raw-edible-exception` | Remove the exception | P8-3 |
| S208 | No-effect condition | `cgolden/safety/allow-saf-stays-error`, `cgolden/safety/cli-allow-saf` (`--allow SAF`, `--warn SAF`) | Lower `SAF` with `@allow`, lower it with a build-time option | P8-8 |
| S209 | Real path | `cgolden/safety/allergen-bowl`, `sample/T2` | Do not track allergens from the dirt on tools | P9-5 |
| S210 | Normal | `cgolden/mix/new-value` | Keep tracking the states of the original ingredients after mixing | P8-3 |
| S211 | Normal | `cgolden/mix/amount-sum` | Do not add the amounts | P8-3 |
| S212 | Normal | `cgolden/mix/mixture-hazard-allergen` | Remove the union of traits | P8-3 |
| S213 | No-effect condition | `cgolden/linear/double-use` | Remove the record of consumption | P8-3 |
| S214 | Boundary | `cgolden/linear/shortage`, `cgolden/linear/exact-amount` | Change the amount comparison to `>` | P8-3 |
| S215 | Normal | `cgolden/linear/leftover-part` | Do not report leftovers | P8-3 |
| S216 | No-effect condition | `cgolden/linear/store-discard-silent` | Report info even after `store` | P8-3 |
| S217 | No-effect condition | `cgolden/linear/parallel-double-use` | Do not count consumption across branches | P8-3 |
| S218 | Normal | `cgolden/parts/inherit-trait` | Remove part inheritance | P8-4 |
| S219 | Boundary | `cgolden/parts/alias-all-fatty`, `cgolden/parts/alias-one-not-fatty`, `cgolden/parts/alias-widest-range`, `cgolden/parts/alias-hazard-any` | Judge by the first candidate only | P8-4 |
| S220 | Normal | `cgolden/parts/alias-name-in-diag` | Show the expanded name in diagnostics | P8-4 |
| S221 | Normal | `cgolden/parts/aibiki-contains` | Make the range of `contains` a single point | P8-4 |
| S222 | Normal | `cgolden/parts/fillet-yield` | Do not keep the yield amount | P8-4 |
| S223 | Side effects | `G-CORE-PROPS` (`prop/meta_ignored`) | Read `meta` as attributes | P9-1 |
| S224 | Normal | `cgolden/actions/uses-capability` | Match by kind instead of capability | P8-5 |
| S225 | Boundary | `cgolden/actions/capacity-from-amount` (2.4 L in 2 L passes exactly, 2.3 L fails) | Do not multiply by the factor 1.2 | P8-5 |
| S226 | No-effect condition | `cgolden/actions/error-in-unused-variant` | Check only the first candidate | P8-4 |
| S227 | Time and order | `cgolden/actions/extend-order` | Put added candidates in front | P8-4 |
| S228 | Normal | `cgolden/actions/requires-auto` | Limit it to what has `requires` written | P8-4 |
| S229 | No-effect condition | `cgolden/actions/no-overload` | Overwrite with the second definition | P8-4 |
| S230 | No-effect condition | `cgolden/kitchen/unitless-capacity`, `cgolden/kitchen/negative-count`, `cgolden/kitchen/misspelled-cleanup` | Do not type-check the kitchen | P8-5 |
| S231 | Boundary | `cgolden/kitchen/range-bounds` (the ends of `10 cm..40 cm`), `cgolden/kitchen/range-open-ended` (`a..`, `..b`, a numeric interval), `cgolden/kitchen/recipe-arg-range` (30 for `servings: Count in 1..20`) | Exclude the ends of the interval, ignore open intervals | P8-5 |
| S232 | Normal | `cgolden/kitchen/check-severity` | Fix the severity of `check` | P8-5 |
| S233 | No-effect condition | `cgolden/kitchen/check-without-kitchen` | Evaluate even without a kitchen | P8-5 |
| S234 | Normal | `cgolden/kitchen/variant-filtered`, `cgolden/kitchen/no-variant-left` | Keep candidates that cannot be used | P8-5 |
| S235 | Normal | `cgolden/kitchen/notation-defaults` | Make the default of `count` 0 | P8-5 |
| S236 | Normal | `cgolden/kitchen/capacity-compile-time` | Treat kitchen values as cook-time values | P8-7 |
| S237 | No-effect condition | `cgolden/kitchen/cleanup-without-kitchen` | Use it without a warning | P8-9 |
| S238 | No-effect condition | `cgolden/vessels/soup-flat-plate` | Remove the required constraint | P8-5 |
| S239 | Normal | `cgolden/vessels/fallback-order` | Reverse the alternative order | P9-6 |
| S240 | Boundary | `cgolden/vessels/count-exact`, `cgolden/vessels/count-short` | Shift the count comparison by 1 | P9-6 |
| S241 | Boundary | `cgolden/vessels/capacity-over-warn` | Remove the capacity check | P8-5 |
| S242 | Normal | `cgolden/menus/output-type-default`, `cgolden/menus/output-type-explicit` | Do not attach the output type | P8-6 |
| S243 | Normal | `cgolden/menus/make-buy-swap` | Give `buy` a different type | P8-6 |
| S244 | Normal | `cgolden/menus/output-properties` | Do not carry over allergens | P8-6 |
| S245 | Normal | `cgolden/menus/choose-breakdown`, `cgolden/menus/choose-default-even` | Ignore the breakdown | P8-6 |
| S246 | Time and order | `cgolden/menus/assemble-order` | Reverse the order | P9-6 |
| S247 | Normal | `cgolden/menus/split-pass` | Do not count `split` as consumption | P8-6 |
| S248 | Normal | `cgolden/menus/single-recipe-wrap`, `cgolden/menus/no-vessel-until-done` | Do not fill in plating | P8-6 |
| S249 | Normal | `cgolden/menus/recipe-arg-external` | Do not report the assumption | P8-6 |
| S250 | Normal | `cgolden/menus/waves` | Ignore the interval of the second wave | P8-6 |
| S251 | No-effect condition | `cgolden/menus/serve-time-in-code` | Accept the clock-time notation | P8-6 |
| S252 | No-effect condition | `cgolden/menus/while-warm-strict-ok` | Treat the property as vague | P8-6 |
| S253 | No-effect condition | `cgolden/eval/runtime-value-in-if` | Do not check the timing | P8-7 |
| S254 | Normal | `cgolden/macro/hygiene-*` (5 cases. The same cases as S96 to S100 of B) | Do not add marks | P8-7 |
| S255 | Normal | `cgolden/macro/definition-checks` | Also check types at definition time | P8-7 |
| S256 | Normal | `cgolden/macro/typecheck-after-expand` | Type-check before expansion | P8-7 |
| S257 | Normal | `cgolden/macro/trail-nested` | List the expansion history from the outside | P8-7 |
| S258 | Boundary | `cgolden/eval/fuel-*` (2 cases. Exactly at and one below) | Do not count fuel | P8-7 |
| S259 | No-effect condition | `cgolden/macro/no-macro-defining-macro`, `cgolden/macro/no-capture` | Accept nested `quote` | P8-7 |
| S260 | Normal | `cgolden/diag/code-and-name` | Do not show the name | P7-3 |
| S261 | Normal | `cgolden/diag/severity-override` | Fix the severity by code | P8-8 |
| S262 | Normal | `cgolden/diag/select-by-name-prefix-group` | Ignore selection by prefix | P8-8 |
| S263 | Boundary | `cgolden/diag/annotation-scope` | Let the annotation work outside the declaration too | P8-8 |
| S264 | Normal | `cgolden/diag/expect-unfulfilled`, `cgolden/diag/expect-suppresses` | Make `@expect` the same as `@allow` | P8-8 |
| S265 | Time and order | `cgolden/diag/precedence`, `cgolden/diag/nested-annotations` (the narrower scope wins) | Let the build-time option win, let the outer annotation win | P8-8 |
| S266 | Time and order | `G-CORE-DIAG-APPEND` | Rewrite a line of the registry | P7-1 |
| S267 | Normal | `cgolden/diag/fix-suggestion` | Do not show fix suggestions | P8-8 |
| S268 | Data completeness | `G-CORE-DIAG-REGISTRY` (the category is one of the 10) | Register a diagnostic of category `XYZ` | P7-1 |
| S269 | Data completeness | `G-TYPE-SEVERITY-TABLE` | Make the default for vessel overcapacity an error | P8-8 |
| S270 | Normal | `cgolden/levels/level-0`, `cgolden/levels/level-1`, `cgolden/levels/level-2` | Always use level 2 | P8-9 |
| S271 | Side effects | `cgolden/levels/check-stops` | Build the schedule even for `check` | P8-9 |
| S272 | Normal | `cgolden/levels/assumptions-as-info` | Return assumptions in another form | P8-9 |
| S273 | Boundary | `G-CORE-PROPS` (`prop/monotonic`) | Do not report `vagueAmount` when there is a kitchen | P8-9 |
| S274 | Normal | `cgolden/levels/critical-path` | Count the shortest time as a sum | P9-3 |
| S275 | Normal | `cgolden/levels/first-variant-assumed` | Use the last candidate | P8-9 |
| S276 | Normal | `cgolden/levels/small-tools-assumed` | Do not report the assumption | P9-5 |
| S277 | Normal | `cgolden/levels/provisional-assumed` | Do not report the assumption of provisional values | P9-3 |
| S278 | Data completeness | `cunit/ir_flat_ids` | Put pointers in the IR | P9-1 |
| S279 | Side effects | `G-CORE-PROPS` (`prop/display_only`) | Put the wave names in the IR | P9-1 |
| S280 | Normal | `cgolden/ir/variants-as-choice` | Put only the candidate chosen by the kitchen in the IR | P9-1 |
| S281 | Platform path | `G-CORE-PROPS` (`prop/ir_invariant`. Samples that do not refer to the kitchen or cooks × cooks 1 to 3 × 3 kitchens) | Put washing in the IR | P9-1 |
| S282 | Normal | `cgolden/ir/servings-changes-ir` | Let servings affect only the schedule | P9-1 |
| S283 | Normal | `cgolden/ir/waves-in-ir` | Remove the waves from the IR | P9-1 |
| S284 | Side effects | `G-CORE-PROPS` (`prop/ir_invariant`) | Put washing in the IR | P9-5 |
| S285 | Normal | `cgolden/ir/text-dump-format` | Omit the resource field | P9-1 |
| S286 | Normal | `cgolden/ir/rational-amounts` | Write amounts as decimals | P9-1 |
| S287 | Data completeness | `G-SCHED-CHECK` (all schedules of golden files and samples) | Remove the wait for dependencies | P9-3 |
| S288 | Normal | `cgolden/sched/priority-longest-path` (an expected schedule built by hand) | Reverse the priority | P9-3 |
| S289 | Boundary | `cgolden/sched/tie-node-order` | Break ties in reverse order | P9-3 |
| S290 | Normal | `cgolden/sched/forward`, `cgolden/sched/backward-serve-at` | Pack from the front even when scheduling backward | P9-3 |
| S291 | Time and order | `cgolden/sched/objective-order` | Put (2) before (1) | P9-7 |
| S292 | Normal | `cgolden/sched/prefer-*` (6 cases. The 4 kinds and the order of 2 combinations) | Ignore the given order | P9-7 |
| S293 | Normal | `cgolden/sched/improvement` (an example that shrinks with the improvement) | Skip the improvement | P9-7 |
| S294 | Normal | `cgolden/sched/unattended-frees-hands` | Use hands even when unattended | P9-3 |
| S295 | Boundary | `cgolden/sched/oven-range-one-unit` | Count it as a separate unit per capability | P9-4 |
| S296 | Normal | `cgolden/sched/variant-earliest`, `cgolden/sched/variant-tie-order` | Always pick the first candidate | P9-4 |
| S297 | Normal | `cgolden/sched/variant-info` | Do not report the info | P9-4 |
| S298 | Normal | `cgolden/sched/calibration` | Do not apply the calibration | P9-4 |
| S299 | Boundary | `cgolden/sched/heat-model` (exactly the reference thickness, twice it, an exponent given as a range) | Do not apply the exponent | P9-4 |
| S300 | No-effect condition | `cgolden/sched/hardness-order-warn` | Remove the hardness check | P9-4 |
| S301 | No-effect condition | `cgolden/sched/capacity-impossible` | Allow exceeding the capacity | P9-4 |
| S302 | Boundary | `cgolden/sched/too-many-cooks` (no warning at exactly the number, a warning with one person more) | Change the comparison to `>=` | P9-4 |
| S303 | Normal | `cgolden/sched/person-attributes` | Ignore per-person restrictions | P9-4 |
| S304 | Normal | `cgolden/sched/range-duration-upper` | Allocate by the lower bound | P9-3 |
| S305 | Normal | `cgolden/wash/dirt-from-leaves` | Do not add the dirt of actions | P9-5 |
| S306 | Normal | `cgolden/wash/compat-rules` (9 compatibility rules) | Shift the rule table by one row | P9-5 |
| S307 | Normal | `cgolden/wash/wait-or-wash` | Always wash | P9-5 |
| S308 | Normal | `cgolden/wash/reason` | Do not attach the reason | P9-5 |
| S309 | Side effects | `cgolden/wash/uses-sink-hands` | Do not use the sink for washing | P9-5 |
| S310 | No-effect condition | `cgolden/wash/hazard-not-relaxed`, `cgolden/wash/allergen-not-relaxed` | Allow the `Hazard` and `Allergen` rules to be overridden | P9-5 |
| S311 | No-effect condition | `cgolden/wash/same-food-same-state` | Wash even for the same ingredient | P9-5 |
| S312 | Time and order | `cgolden/wash/no-drying` | Insert a wait after washing | P9-5 |
| S313 | Normal | `cgolden/cleanup/none`, `cgolden/cleanup/before-meal`, `cgolden/cleanup/after-meal` | Include tableware in `beforeMeal` | P9-5 |
| S314 | Normal | `cgolden/cleanup/wipe-places` | Do not include wiping | P9-5 |
| S315 | Normal | `cgolden/cleanup/store-to-fridge` | Do not include putting things away | P9-5 |
| S316 | Time and order | `cgolden/sched/meal-blocks-hands` | Put hand work during the meal too | P9-6 |
| S317 | Normal | `cgolden/cleanup/dishwasher-during-meal` | Run the dishwasher after the meal | P9-6 |
| S318 | Boundary | `cgolden/serve/serve-within` (exactly t passes, and no placement goes beyond t) | Remove the constraint | P9-6 |
| S319 | Side effects | `cgolden/serve/keep-warm-occupies` | Do not occupy equipment for keeping warm | P9-6 |
| S320 | Normal | `cgolden/serve/chilled-frozen`, `cgolden/serve/fridge-capacity-when-written` | Make the default capacity 1 | P9-6 |
| S321 | Normal | `cgolden/serve/just-before-serve` | Make the alias 5 minutes | P9-6 |
| S322 | Boundary | `cgolden/serve/long-wait-info` (no info at exactly 30 minutes, info beyond it) | Change the comparison to `>=` | P9-6 |
| S323 | Side effects | `cgolden/serve/plating-time` | Make plating 0 minutes | P9-6 |
| S324 | Time and order | `cgolden/serve/vessel-busy-until-meal-end` | Return the vessel right after plating | P9-6 |
| S325 | Normal | `cgolden/sched/shared-cutting` | Do not combine them | P9-6 |
| S326 | Time and order | `cgolden/sched/meal-duration-from-interval` | Make the meal length 0 | P9-6 |
| S327 | No-effect condition | `G-API-EXPORTS` | Remove the prefix of an internal function | P10-1 |
| S328 | Normal | `cunit/api_levels_by_null` | Treat NULL as having a kitchen | P10-1 |
| S329 | Side effects | `cunit/api_free_balance` | Leave the returned string in the arena | P10-1 |
| S330 | Normal | `G-API-SCHEMA` | Write rationals as strings | P10-2 |
| S331 | Data completeness | `G-API-SCHEMA`, `sh/test-validate.sh` | Add a field that is not in the schema | P10-2 |
| S332 | Data completeness | `cgolden/api/diag-json-fields` (match the JSON of a diagnostic with expansion history down to its fields) | Do not output related locations | P10-2 |
| S333 | Docs match | `G-API-SARIF`, `cgolden/api/sarif-mapping` (ruleId, level, relatedLocations and fixes of diagnostics of each severity) | Make info `warning` | P10-3 |
| S334 | Normal | `cgolden/api/signatures-json`, `cgolden/api/text-material` | Do not output the argument types | P10-2 |
| S335 | Normal | `cgolden/api/types-json` | Do not output range constraints | P10-2 |
| S336 | Platform path | `G-API-HEADER` | Use `nullptr` in the header | P10-1 |
| S337 | No-effect condition | `G-API-SCHEMA` (diagnostics have no message text field) | Add an English sentence to diagnostics | P10-2 |
| S338 | Platform path | `G-PLAT-IOS` | Remove the device slice from the xcframework | P11-3 |
| S339 | Platform path | `G-PLAT-ANDROID` | Remove one ABI | P11-4 |
| S340 | Boundary | `G-PLAT-LINUX` (`jni`, input with emoji and Japanese), `G-PLAT-ANDROID` | Pass strings with `NewStringUTF` | P11-1 |
| S341 | Normal | `G-PLAT-LINUX` (`linux-so`) | Expose internal symbols | P11-1 |
| S342 | Platform path | `G-PLAT-WASM` | Count the length in UTF-16 inside the Worker | P11-2 |
| S343 | Platform path | `G-PLAT-LINUX`, `G-PLAT-WASM`, `G-PLAT-IOS`, `G-PLAT-ANDROID` | Drop the trailing newline of the output in one of the bridges | P11-1 to P11-4 |
| S344 | Data completeness | `G-PLAT-CI` | Delete the fuzz job from the workflow | P11-5 |
| S345 | Normal | `cli/subcommands` | Remove `unbundle` | P10-4 |
| S346 | Normal | `cli/entry-forms` (4 forms) | Resolve the path from the end | P10-4 |
| S347 | No-effect condition | `cli/entry-colon` | Accept `file:name` | P10-4 |
| S348 | No-effect condition | `cli/entry-ambiguous` | Pick the first | P10-4 |
| S349 | Boundary | `cli/kitchen-lookup` (by name, by path, 2 found) | Use the first one found | P10-4 |
| S350 | No-effect condition | `cli/no-default-kitchen` | Make one in `kitchens/` the default | P10-4 |
| S351 | Boundary | `cli/short-options`, `cli/no-prefix-abbrev` | Accept `--kit` | P10-4 |
| S352 | Normal | `cli/project-scope` | Also build modules that only have definitions | P10-4 |
| S353 | Time and order | `cli/project-order-continue` | Stop at the first failure | P10-4 |
| S354 | Data completeness | `cli/out-layout` (match the tree of `out/` with `index.json`) | Do not write `index.json` | P10-4 |
| S355 | Normal | `cli/options-to-ozen` | Drop `--prefer` | P10-4 |
| S356 | Normal | `cli/diag-display` | Do not show the display text from the identifier | P10-4 |
| S357 | Normal | `G-CORE-STD-EMBED`, `cgolden/std/package` | Change the name in `stdlib/package.ozen` | P8-0 |
| S358 | Data completeness | `G-STD-LISTS` | Delete `tear` from `stdlib/` | P13-1 |
| S359 | Data completeness | `G-STD-PROVENANCE` | Delete the provenance from one attribute | P13-1 |
| S360 | Normal | `G-CORE-PROPS` (`prop/std_copy`) | Add special handling only for the definitions of `std` | P13-1 |
| S361 | Normal | `cgolden/std/prelude-contents` | Remove `g` from the prelude | P13-1 |
| S362 | Data completeness | `G-STD-PHRASES` | Delete one from `en.ftl` | P13-2 |
| S363 | Platform path | `sample/curryRice`, the digests of `G-PLAT-*` | Delete `peel` from the sample | P12-1 |
| S364 | Normal | `sample/menuA` | Do not fill in plating for a single dish | P13-3 |
| S365 | Normal | `sample/menuB` | Do not connect the outputs of the recipes | P13-3 |
| S366 | Real path | `sample/menuC` | Do not combine the shared cutting work | P13-3 |
| S367 | Real path | `sample/menuD` | Ignore the second wave | P13-3 |
| S368 | Real path | `sample/menuE` | Do not soak `Sticky` | P13-3 |
| S369 | Real path | `sample/T1` | Make the output of the butchering action a single one | P13-4 |
| S370 | Real path | `sample/T2` | Do not look at per-person allergies | P13-4 |
| S371 | Real path | `sample/T3*` (2 cases. `T3a` for kitchen (a) and `T3b` for (b), each with 2 runs for menus B and C) | Fix the candidate choice to the first candidate | P13-4 |
| S372 | Real path | `sample/T4` | Do not expand the alias | P13-4 |
| S373 | Data completeness | `G-SCHED-CHECK` (all schedules of the samples) | Put hand work during the meal | P13-5 |
| S374 | Platform path | The digests of `G-PLAT-WASM`, `G-PLAT-IOS`, `G-PLAT-ANDROID` (including the samples) | Cut the end of the input in the bridge of one environment | P13-5 |
| S375 | Normal | `rtest/recipe-text` | List the steps in reverse order (`let` refers to earlier things, so the written order is always one of the dependency orders, and does not work as a way to break it) | P14-2 |
| S376 | Normal | `rtest/schedule-text` | Do not show the cook | P14-3 |
| S377 | Normal | `rtest/merge-sentences` | Do not combine them | P14-2 |
| S378 | Side effects | `rtest/group-annotation`, `G-CORE-PROPS` (`prop/display_only`) | Put `@group` in the IR | P14-2 |
| S379 | Normal | `rtest/waves-chapters` | Do not show the during-the-meal section | P14-3 |
| S380 | Boundary | `rtest/prep-cook-split` | Make the split a fixed time | P14-3 |
| S381 | No-effect condition | `G-REND-SELFCONTAINED` | Load CSS from a CDN | P14-3 |
| S382 | Normal | `rtest/gantt-svg` (match the positions of the rows and tasks) | Shift the time position by 1 minute | P14-3 |
| S383 | Normal | `rtest/shopping-list` | Drop things that are `buy` | P14-3 |
| S384 | Normal | `rtest/fluent-keys` | Make the keys camel case | P14-1 |
| S385 | Time and order | `rtest/phrase-layering` | Reverse the layering order | P14-1 |
| S386 | Boundary | `rtest/phrase-fallback` (missing in ja, missing in en too) | Skip the fallback to English | P14-1 |
| S387 | Normal | `rtest/name-by-stage-dialect` | Do not pass `$dialect` | P14-1 |
| S388 | Normal | `rtest/part-names` | Do not use the specific keys | P14-1 |
| S389 | No-effect condition | `G-STD-PHRASES` (`unknown args`), `rtest/phrases-check` | Remove the argument check | P14-1 |
| S390 | Normal | `rtest/counters` | Do not attach counter words | P14-2 |
| S391 | Normal | `rtest/vague-time-words` | Show the range numbers | P14-2 |
| S392 | Normal | `rtest/variant-wash-assumption-text` | Do not show the reason for washing | P14-3 |
| S393 | Real path | `rtest/settings-form-roundtrip` (`check` the text made from the form values with the CLI) | Write out without units | P14-4 |
| S394 | Normal | `rtest/settings-form-new-field` | List the fields by hand | P14-4 |
| S395 | Real path | `cli/render` | Remove `render` | P14-5 |
| S396 | Platform path | `G-REND-BROWSER-SAFE` | Load `fs` in the main code | P14-5 |
| S397 | Normal | `cgolden/bundle/inline-modules` | Remove the namespaces | P15-1 |
| S398 | Normal | `cgolden/bundle/header`, `cgolden/bundle/header-license-from` | Do not write the version of `std`, do not carry over `license` | P15-1 |
| S399 | Data completeness | `cgolden/bundle/contents` (the kitchen, `std` and unused definitions are not included), `cgolden/bundle/phrases-included`, `cgolden/bundle/dep-partial` | Include unused definitions too, do not include phrases | P15-1 |
| S400 | Side effects | `G-BUNDLE-STABLE` (`cgolden/bundle/order-by-name`. Modules whose name order, order traced from the entry, and path order all differ) | Use the reading order | P15-1 |
| S401 | Normal | `G-CORE-PROPS` (`prop/bundle_roundtrip`. Match the declarations of the restored files with the original source, and match the IR) | Restore modules with different names in `unbundle`, remove `local` | P15-2 |
| S402 | Normal | `cgolden/bundle/build-standalone` | Ignore the modules inside the bundled file | P15-1 |
| S403 | No-effect condition | `cgolden/bundle/std-module-rejected` | Remove the check | P15-1 |
| S404 | Boundary | `cgolden/bundle/std-version-check` | Do not compare versions | P15-1 |
| S405 | Boundary | `cgolden/bundle/annotation-scope` | Let the annotation work on the whole bundled file | P15-1 |
| S406 | No-effect condition | `cgolden/bundle/local-closed` | Remove `local` when bundling | P15-1 |
| S407 | No-effect condition | `cgolden/bundle/import-two-versions` | Remove the check | P15-2 |
| S408 | Boundary | `G-CORE-FUZZ` (the `bundle` and `kitchen` targets), `cgolden/limits-shared/*` (4 cases. Exactly at and 1 over the size limits of bundled files and kitchen specs) | Do not apply the input size limit to the kitchen argument | P15-3 |
| S285 | Docs match | §8 (Reason a command cannot decide: the details of the text IR format were added by this plan, and the concept has no right answer for readability) | — | — |
| S288 | Normal (natural as cooking) | §8 (Reason a command cannot decide: there is no right answer for whether a schedule is natural as cooking (whether the order of tasks lets you move around the kitchen without strain). That the constraints are kept is confirmed by `G-SCHED-CHECK`) | — | — |
| S331 | Docs match | §8 (Reason a command cannot decide: the names and shapes of the JSON fields were added by this plan, and the concept has no right answer for ease of use) | — | — |
| S336 | Docs match | §8 (Reason a command cannot decide: the rule that the public header can also be read as C17 was added by this plan, and a person judges whether it fits the intent) | — | — |
| S338 | Platform path (real device) | §8 (Reason a command cannot decide: CI runners have no real iPhone, and a real device cannot be connected from the local Linux machine) | — | — |
| S339 | Platform path (real device) | §8 (Reason a command cannot decide: the CI runners and the local emulator are not real Android devices, and no real device is connected locally) | — | — |
| S381 | Normal (appearance) | §8 (Reason a command cannot decide: the standard for the look and readability of the HTML is subjective; the machine confirms only that it is self-contained) | — | — |
| S382 | Normal (appearance) | §8 (Reason a command cannot decide: the standard for how easy the Gantt chart is to read is subjective; the machine confirms only that the task positions are correct) | — | — |
| S383 | Docs match | §8 (Reason a command cannot decide: the choice to put the shopping list in the renderer was added by this plan, and a person judges whether it fits the intent) | — | — |
| S384 | Normal (natural as Japanese) | §8 (Reason a command cannot decide: there is no right answer for whether the Japanese and English phrases are natural. Key coverage and argument matching are confirmed by `G-STD-PHRASES`) | — | — |
| D30 | Negative | `G-PROTO-VERSION` | Write `project(ozenproto VERSION 0.0.1 LANGUAGES C)` | P6-3 |
| S409 | Docs match | `G-REPO-CLAUDEMD` | Delete the language rules from `CLAUDE.md` | P0-3 |
| S410 | No-effect condition | `cgolden/syntax/utf8-invalid` | Remove the UTF-8 check | P7-3 |
| S411 | No-effect condition | `cgolden/syntax/ident-nonascii`, `cgolden/syntax/string-utf8-passthrough` | Allow UTF-8 in identifiers | P7-3 |
| S412 | Normal | `cgolden/syntax/multiline-string` | Read newlines as separators | P7-3 |
| S413 | No-effect condition | `cgolden/names/let-rebind` | Overwrite the binding | P7-4 |
| S414 | Boundary | `cgolden/names/arity-*` (3 cases. Missing, unknown name, twice) | Do not count named arguments | P8-4 |
| S415 | No-effect condition | `cgolden/syntax/annotation-target` | Ignore annotations with nothing to attach to | P8-8 |
| S416 | Normal | `cgolden/names/ingredient-type-omitted` | Make the type `Unknown` when it is omitted | P7-4 |
| S417 | No-effect condition | `cgolden/parts/part-on-partless` | Remove the part check | P8-4 |
| S418 | No-effect condition | `cgolden/actions/where-constraint` | Ignore `where` | P8-4 |
| S419 | Normal | `cgolden/parts/trait-defined-parts` | Do not inherit the parts of a trait | P8-4 |
| S420 | Normal | `cgolden/parts/auto-discard` | Report the innards as leftovers | P8-4 |
| S421 | No-effect condition | `cgolden/parts/part-trait-clash` | Remove the same-name check | P8-4 |
| S422 | Normal | `cgolden/menus/multiple-outputs` | Drop the second output | P8-6 |
| S423 | Time and order | `cgolden/menus/plate-overrides-dish` | Let the default of `Dish` win | P9-6 |
| S424 | Normal | `cgolden/actions/extend-in-assumptions` | Do not show added candidates in the assumptions | P8-9 |
| S425 | No-effect condition | `cgolden/api/local-not-listed` | List `local` too | P10-2 |
| S426 | No-effect condition | `cgolden/packages/requires-range-syntax` | Accept `>=` | P7-4 |
| S427 | Normal | `cgolden/packages/copied-dep-version` | Do not look at the version of the copy | P7-4 |
| S428 | Normal | `cgolden/packages/deprecated-warning` | Do not show the warning | P8-8 |
| S429 | No-effect condition | `cgolden/kitchen/requires-unmet` | Check after building the schedule | P8-5 |
| S430 | Normal | `cgolden/kitchen/default-places` | Make omitted places 0 | P8-5 |
| S431 | Boundary | `cgolden/kitchen/check-fuel` | Do not count fuel in the `check` expression | P8-7 |
| S432 | Normal | `cgolden/kitchen/kitchen-has-branch` | Defer the branch to cook time | P8-7 |
| S433 | Time and order | `cgolden/sched/parallel-join` (the next statement, even without a data dependency, starts after all branches) | Remove the join | P9-3 |
| S434 | Normal | `cgolden/actions/composite-*` (3 cases. Expansion, types, schedule) | Keep a composite action as a single task | P9-4 |
| S435 | Boundary | `cgolden/vessels/shared-platter`, `cgolden/vessels/shared-leftover` (food left unplated) | Do not count the plated amount | P9-6 |
| S436 | Normal | `cgolden/levels/level-1-human-only` | Count equipment even at level 1 | P9-3 |
| S437 | Normal | `cgolden/sched/store-discard-in-schedule` | Do not show `store` in the schedule | P9-5 |
| S438 | Normal | `cgolden/sched/heat-thickness-by-cut` (3 cuts) | Count by thickness even for cubes | P9-4 |
| S439 | Normal | `cgolden/sched/heat-direct-attribute` | Use the formula for the microwave too | P9-4 |
| S440 | Normal | `cgolden/wash/rule-override` | Ignore the pantry's override | P9-5 |
| S441 | Normal | `cgolden/wash/staining-info` | Add washing for `Staining` | P9-5 |
| S442 | Normal | `cgolden/sched/veg-before-meat` | Choose without counting washing | P9-5 |
| S443 | Persistence | `cli/out-rebuild-identical`, `cli/out-removed-entry` | Do not delete old output | P10-4 |
| S444 | Persistence | `cli/unbundle-no-overwrite` | Overwrite | P15-2 |
| S477 | Normal | `cgolden/bundle/relative-import` | Do not make the modules inside the bundled file belong to a package | P15-1 |
| S478 | Normal | `cli/unbundle-phrases` | Put all phrases back into the original package | P15-2 |
| S395 | Real path | `cli/render-entry` (giving a module name, the default when the target is omitted, an error result, an ambiguous entry point) | Do not decide the entry point when only a module is given | P16-3 |
| S385 | Real path | `cli/render-dep-phrases` | Do not pass the phrases of dependencies | P16-3 |
| S387 | Real path | `cli/render-stage` (D39. The stage and dialect of the rendering settings) | Do not pass the stage setting | P16-3 |
| S451 | Data completeness | `G-REND-PHRASES-RENDER` (identifiers, D38) | Output diagnostic arguments as raw identifiers | P16-3 |
| S453 | Data completeness | `sh/test-htmlcheck.sh` (unitless. Whether something is a quantity is decided by the dimension of its type) | Decide whether something is a quantity only by `data-quantity` | P16-3 |
| S452 | Boundary | `G-REND-HTML` (gantt positions. Match positions and widths derived from `data-label-width` and `data-per-minute`) | Shift the position of a rectangle by 1 minute | P16-3 |
| S452 | Boundary | `G-REND-HTML` (page width. A chart wider than 720 needs `svg{max-width:100%}`) | Remove `max-width` from the SVG | P16-3 |
| S380 | Boundary | `rtest/prep-cook-split` (exactly at the split, `prepBefore` of 0 and 3600) | Change the split comparison to `<` | P16-3 |
| S382 | Normal | `rtest/gantt-svg` (resource rows and holding) | Do not draw resource rows | P16-3 |
| S393 | Real path | `rtest/settings-form-roundtrip` (run the script embedded in the page with a fake form) | Call a function outside the module from the embedded function | P16-3 |
| S377 | No-effect condition | `rtest/merge-sentences` (do not combine when the next task takes 2 inputs) | Combine without looking at the number of inputs | P16-3 |
| S383 | Data completeness | `rtest/shopping-list` (different dimensions go on separate lines) | Add without looking at the dimension | P16-3 |
| S454 | Data completeness | `G-REND-HTML` (the book form. `render-all.mjs` writes a book per sample) | Output an element that is not closed in the book form | P16-3 |
| S379 | Normal | `rtest/recipe-schedule` (a schedule without waves) | Put the tasks of a schedule without waves in cleanup | P16-3 |
| S406 | No-effect condition | `cgolden/bundle/local-import-rejected` | Allow importing `local` for inline modules | P16-3 |
| S404 | Boundary | `cgolden/bundle/std-version-incompatible` | Do not compare the major version | P16-3 |
| S404 | Boundary | `cgolden/bundle/std-version-bad` | Do not check the version format | P16-3 |
| S401 | Normal | `prop/bundle_roundtrip` (an entry that cannot be bundled is skipped only for an entry error) | Refuse to bundle because of an error in a dish unrelated to the entry | P16-3 |
| S399 | Data completeness | `cgolden/bundle/global-effects` (`extend`, `rule`) | Trace declarations with global effect by name | P16-3 |
| S399 | Normal | `cgolden/bundle/same-name` | Include only one of the candidates with the same name | P16-3 |
| S399 | Normal | `cgolden/bundle/import-pruned` | Keep imports of unused names | P16-3 |
| S399 | No-effect condition | `cli/bundle-phrases-quote` | Do not check `"""` in phrases | P16-3 |
| S444 | Persistence | `cli/unbundle-manifest-taken` (conflicts at `package.ozen` and under dependencies) | Check only the module files | P16-3 |
| S445 | Normal | `rtest/recipe-wave-sections` | Do not split by wave name | P14-2 |
| S446 | Normal | `rtest/diag-text` | Make diagnostic messages fixed English sentences | P14-1 |
| S447 | Normal | `rtest/wash-vs-togu` | Use the general verb for "wash" for rice too | P14-1 |
| S448 | Normal | `rtest/book-from-index` | Do not make a table of contents | P14-3 |
| S449 | Normal | `rtest/print-css` | Remove `@media print` | P14-3 |
| S450 | Time and order | `rtest/ingredient-order` | Sort ingredients by name | P14-2 |
| S451 | Data completeness | `G-REND-PHRASES-RENDER` | Delete one from `ja.ftl` | P14-3 |
| S452 | Boundary | `G-REND-HTML` (gantt) | Draw tasks of the same row overlapping | P14-3 |
| S453 | Data completeness | `G-REND-HTML` (form) | Do not show the unit of a quantity field | P14-4 |
| S454 | Data completeness | `G-REND-HTML` (html), `sh/test-htmlcheck.sh` | Output an unclosed `<br>` | P14-3 |
| S455 | Platform path | `G-PLAT-IOS` (the macOS slice and the Swift tests) | Remove the macOS slice from the xcframework | P11-3 |
| S456 | Data completeness | `G-SAMPLE-STD-ONLY` | Add one `ingredient` to a sample | P13-3 |
| S457 | Data completeness | `G-TYPE-SEVERITY-TABLE` (categories) | Make vessel overcapacity `KIT` | P9-7 |
| S458 | No-effect condition | `G-STD-STATIC` (naming), `sh/test-stdcheck.sh` | Start the name of a `std` action with a capital letter | P7-4 |
| S459 | Boundary | `G-STD-STATIC` (provisional-point) | Make a provisional value a single point | P7-4 |
| S460 | No-effect condition | `G-STD-STATIC` (fct-inline) | Write the refuse rate as a number | P7-4 |
| S461 | No-effect condition | `G-STD-STATIC` (capability-unprovided) | Remove `Blending` from `Blender` | P7-4 |
| S462 | Data completeness | `G-STD-LISTS` (the input is the list of all definitions) | Drop vessels from the list of definitions | P7-4 |
| S463 | Normal | `cgolden/ir/kitchen-dependent-info` | Do not report the info | P9-1 |
| S464 | Boundary | `cgolden/diag/check-codes` (`std` and the writer, the ends of 900 to 999, duplicates) | Do not check the range of the writer's numbers | P8-8 |
| D20 | Negative | `G-PROTO-GOLDEN-INDEPENDENT`, `G-VOCAB-EXPECT-INDEPENDENT`, `sh/test-check-commit-separation.sh` | Change the implementation and the expectations in the same commit | P0-6, P4-3, P2-1 |
| D20 | Time and order | `G-REPO-RED-FIRST`, `sh/test-check-red-first.sh` | Put in the implementation first and add the expectations in the next commit (the test passes at that commit) | P0-6, P1-3, P4-3 |
| D15 | Negative | `G-PROTO-NOINT128` | Use `__int128` | P4-2 |
| Plan | Data completeness | `G-PLAN-COVERAGE`, `G-PLAN-TESTS-EXIST`, `sh/test-check-plan-tests.sh` | Delete one file named by a test in the coverage table | P0-5, P16-3 |
| Plan | Data completeness | `G-REPO-SELFTESTS` | Delete one self-test | P0-6, P16-3 |
| D28 | Negative | The completion condition of P8-0 (running the conversion again without changing `spec/vocab/` gives no diff in `stdlib/`) | The conversion script changes the order on each run | P8-0 |
| D30 | Time and order | `G-REPO-VERSION` | Raise only the version in `stdlib/package.ozen` | P10-5, P16-2 |

Coverage check (run after P0-1, at `P<n>-0` of each phase and at P16-3. The output is empty and 0):

```bash
diff <(sed -n '/^## 4\./,/^## 5\./p' plans/ozen-roadmap.md | grep -oE '\*\*S[0-9]+\*\*' | tr -d '*' | sort -u) \
     <(sed -n '/^### 5\.3/,/^## 6\./p' plans/ozen-roadmap.md | grep -oE '^\| S[0-9]+' | tr -d '| ' | sort -u)
sed -n '/^## 8\./,/^## 9\./p' plans/ozen-roadmap.md | grep '^- ' | grep -v 'Reason a command cannot decide:' | wc -l
```

Findings of the independent audits (subagents) that were not taken, and why:

- Of the 19 findings of the coverage audit of P16-3 (2026-10-27), 17 were taken as tests or fixes (§9.1). Not taken: (1) The detail of S376, "with `--serve-at`, clock times": `serveAt` is a time from the start (`Time`) and holds no clock time, so §4.28.1 was changed to "times are always `H:MM` from the start" (the test stays `rtest/schedule-text`). (2) "choose the name by the value's attributes and the cooking context" of S387: `std` has no stage attribute and no phrase uses `$context`, so, as D39 says, the form passed through the rendering settings was tested, and attributes and context went to §8
- All 72 findings of the second audit (2026-10-25, the part for D to L) were also taken. The main ones: `ctest | tail -1` does not pick up the passing line (changed to take the line with `grep`); the contradiction between the concept's "changing cooks and the kitchen does not change the IR" and compile-time values that depend on the kitchen (D31); the wrong order of embedding `std` before `stdlib/` exists (the conversion was moved earlier, to P7-4); the JSON of the list of all definitions (S462); the branch range (D2 limited work to `main`, so it is counted from `plan-start`); confirming before push (D19); missing claims of the concept (fork-join, composite actions, portions, `requires` of the kitchen, multiple outputs and so on. S410 to S464); fixes to the recommendations of D25 to D27 and D30; the codes of `check` (D32).
- About the first audit: none. All 54 findings of the audit at plan creation (2026-10-25) were taken into §4, §5, §6 and §8. The main ones: `\|` inside tables breaks the judgment commands (moved to the code block of §5.1); the range becomes empty on `main` (fixed to the tags `base-vocab` and `base-proto`); rows that do not fail even when deliberately broken (the ways to break and the tests of S47, S57, S60, S91, S118, S122, S125 were fixed); S items without diagnostic names (names were given to S15 to S27 and S65); matching the relations the concept states explicitly (§4.7.1, S139); the browser Web Worker check (moved from §8 to S150).

---

## 6. Phases and tasks

Notation:

- The **completion condition** is a goal ID from §5, or a command with a single pass/fail result. The gate of each phase runs all goals up to that point
- Status reports are written per part and per environment ("Table: schema, check, lists, sample and tests passed. Validity as cooking not checked", "Prototype: Linux gcc: build and tests passed. Linux clang: passed. WASM: passed. iOS simulator: passed. Real iPhone and browser not checked")
- A commit is one task, or a smaller logical unit. Test expectations (golden `.out`, fixtures, the sample `expect`) go in a commit separate from the implementation code (D20). Do not push (only P5-3 of D19, after the user confirms)
- A "mutation check" breaks the implementation (or the target file) the way the "How to break it" column of §5.3 says, confirms that the test fails and that it passes after restoring, and records one line per check in §9. Never commit the broken state

### Phase 0 — Decisions and preparation (no change to feature code, `main`)

| ID | Content | Completion condition |
| --- | --- | --- |
| P0-1 | Settle D1 to D33 of §2 with the user and write them into this file. Fix §4 to §8 where the answers change them | `grep -c '^### D[0-9]*\. .*\[decided\]' plans/ozen-roadmap.md` is 33, `grep -c '^### D[0-9]*\. .*\[open\]' plans/ozen-roadmap.md` is 0, `grep -c '^- \*\*Decision:' plans/ozen-roadmap.md` is 33. `G-PLAN-COVERAGE` |
| P0-2 | `git init -b main`. Create `.gitignore` (`build/`, `__pycache__/`, `data/mext-fct-2023/source/`), and make `docs/concept.md`, this plan and `.gitignore` the first commit (confirm with the user before committing) | `git rev-parse --verify main` succeeds, `git rev-list --count main` is 1, `git show --name-only --format= main \| sort \| tr '\n' ' '` is `.gitignore docs/concept.md plans/ozen-roadmap.md ` |
| P0-3 | License texts (D4. Before fetching, show the file name, source and size and confirm. Write the sha256 of the fetched texts in §9.1), `NOTICE`, the README skeleton (the table and the one sentence on license classes, empty headings for each part), `CLAUDE.md` (the language rules of D3), `scripts/check-spdx.sh` and its self-test | `G-REPO-SPDX`, `G-REPO-LICENSE-TEXT`, `G-REPO-README-LICENSE`, `G-REPO-CLAUDEMD`, `G-REPO-NOREPLY`, `scripts/test-check-spdx.sh` prints `test-check-spdx: <k> cases, 0 failed`. Mutation checks for S1, S409 and S475 (recorded in §9.1) |
| P0-4 | `toolchain.txt` (versions of gcc, clang, cmake, node, python3, PyYAML, jsonschema, openpyxl, emcc), `scripts/check-tools.sh` and its self-test. If the Recommendation of D18 is taken, install emsdk and pin its version (confirm before downloading) | `G-REPO-TOOLS`, `scripts/test-check-tools.sh` prints `0 failed`, the first line of `emcc --version` contains the emcc version in `toolchain.txt` |
| P0-5 | `scripts/goals.sh` (the code blocks of §5 turned into functions; added to per phase), `scripts/check-plan-tests.sh` (checks that test names exist using the mapping rules of §5.2 and prints counts per category. Counts the "n cases" of names that contain `*`, and leaves out names in `scripts/retired-tests.txt`. With `--mutations`, prints the number of coverage table rows that have a "How to break it" entry and the number of mutation check records in §9), `scripts/check-lists-copy.py`, `scripts/selftests.txt`, and a self-test for each | The 3 self-tests print `0 failed`. `scripts/check-plan-tests.sh plans/ozen-roadmap.md` prints counts per category (only the count of missing is recorded at this point). `scripts/goals.sh G-PLAN-COVERAGE` passes |
| P0-6 | `scripts/check-commit-separation.sh`, `scripts/check-red-first.sh` (create the exception list `scripts/red-first-exceptions.txt` empty), `scripts/check-selftests.sh`, and a self-test for each | The 3 self-tests print `0 failed`, `G-REPO-SELFTESTS`. Mutation checks for the D2 and D20 rows (done in a throwaway repository) |
| P0-7 | Put the tag `plan-start` on `main` (the starting point of later diff ranges, D2). Record the baseline N0 at the starting point (0 tests) in §9.1 | `git rev-parse --verify plan-start` succeeds, and `git rev-parse plan-start` equals `git rev-parse main` |

### Phase 1 — A: Table shape and static checks

| ID | Content | Completion condition |
| --- | --- | --- |
| P1-1 | `tools/vocabcheck/qty.py` and `test_qty.py` (S8 to S11) | `python3 -m unittest tools.vocabcheck.test_qty -v` prints `OK`. Mutation checks for the S8 to S11 rows of §5.3 |
| P1-2 | `spec/schema/vocab.schema.json`, `sample.schema.json`, loading that rejects duplicate keys, the `schema` subcommand, `fx/vocab-ng/schema-*` and `yaml-implicit-*` (S7, S12, S137) | `py/test_vocabcheck.test_schema_covers_all_files` and the fixture tests pass. Mutation checks for S7 and S12 |
| P1-3 | The `check` subcommand (the checks of S13 to S28). Write `fx/vocab-ng/*` and `fx/vocab-ok/*` first, in a separate commit | All tests of the matching fixtures pass, `G-VOCAB-EXPECT-INDEPENDENT`. Mutation checks for S13 to S27 |
| P1-4 | Table data 1: `dimensions`, `units`, `axes`, `traits`, `capabilities`, `dirt`, `vessels`, `sources`. Check the allergen kinds against the current list of the Consumer Affairs Agency, and write them in the "allergen kinds" row of §4.7 and in §9.2 (changes to §4.7 are made on `main`) | `G-VOCAB-SCHEMA` (the files at this point), `G-VOCAB-CHECK`, `G-REPO-SPDX`. Mutation check for S2 |
| P1-5 | Table data 2: `ingredients` (including parts), `aliases`, `actions` (following the answers to D9 to D11) | `G-VOCAB-SCHEMA`, `G-VOCAB-CHECK`. Mutation checks for the "Data completeness" rows of S13 and S22 |
| P1-6 | Copy §4.7 into `spec/coverage/concept-lists.yaml` as (kind, owner, identifier, value) tuples, and §4.7.1 into `spec/coverage/concept-relations.yaml`. The `lists` subcommand, `test_lists_*` and `test_relations_mismatch` | `G-VOCAB-LISTS`, `G-VOCAB-LISTS-COPY`. Mutation checks for S29 and S138 to S140 |
| P1-7 | Gate A1 | `G-VOCAB-SCHEMA`, `G-VOCAB-CHECK`, `G-VOCAB-LISTS`, `G-VOCAB-LISTS-COPY`, `G-VOCAB-TESTS`, `G-VOCAB-READONLY`, `G-VOCAB-EXPECT-INDEPENDENT`, `G-REPO-SPDX`, `G-REPO-SELFTESTS`, `G-REPO-RED-FIRST`. Mutation check for S28 |

### Phase 2 — A: Type check of the sample

| ID | Content | Completion condition |
| --- | --- | --- |
| P2-1 | Rules for state, dimension, linearity, names and diagnostic order (S30 to S44, S56). Write `fx/sample-*` first, in a separate commit | The matching fixtures and `test_diag_order_and_location` pass, `G-VOCAB-EXPECT-INDEPENDENT`. Mutation checks for S30 to S44 and S56 |
| P2-2 | Lists and `combine`, `blend` (D33), `output` types, `variant`, recipe output, `make` of menus, plating, safety, requirements (S45 to S55, S471 to S473) | The matching fixtures pass, `G-VOCAB-EXPECT-INDEPENDENT`. Mutation checks for S43 (plating), S45 to S55 and S471 to S473 |
| P2-3 | `spec/samples/curryRice.yaml`. Write the `expect` (requirements, info, plated values, state after each step) by hand from the table and §4.5, in a commit separate from the check script code. Write the number of leaves k in §9.2 | `G-VOCAB-SAMPLE`, `G-REPO-SPDX`. Mutation checks for S3 and S57 |
| P2-4 | Gate A2 | The goals of P1-7 and `G-VOCAB-SAMPLE` |

### Phase 3 — A: Values from the food composition table

| ID | Content | Completion condition |
| --- | --- | --- |
| P3-1 | On the MEXT page, check where the refuse rate and weight change rate are, and whether the errata of 2026-03-27 are applied. Before fetching the Excel file, show the file name, source and size and get the user's confirmation, then place it in `data/mext-fct-2023/source/`. Write the results (including the text of the header cells of the columns to read, and 3 food numbers with values read by hand) in §9.2 | `sha256sum data/mext-fct-2023/source/*.xlsx` prints 1 or more lines, and the values are in §9.2. `git check-ignore data/mext-fct-2023/source/<元のファイル名>` succeeds |
| P3-2 | `tools/fct/extract.py` and `test_extract.py`, `refuse.csv`, `data/mext-fct-2023/README.md`, the attribution line in `NOTICE` | `G-VOCAB-FCT`, `G-VOCAB-FCT-STABLE`, `G-REPO-NOTICE`, `python3 -m unittest tools.fct.test_extract -v` prints `OK`. Mutation checks for S5, S58, S60 and S61 |
| P3-3 | Make the yield properties of the table refer to `mext-fct-2023`. `fx/vocab-ng/fct-unknown-food` | `G-VOCAB-CHECK`, `G-VOCAB-FCT`, the matching fixture passes. Mutation checks for S59 and S141 |
| P3-5 | Primary source research: for the numeric properties of the table (time of actions, heating time, soaking and rice cooking time, density, typical weight per piece, hardness, and yields not in the food composition table), search official information from makers of home appliances and cooking tools, documents of public bodies, and research papers. Before fetching any document (downloading a PDF and so on), show the document name, source and size each time and get confirmation. Values that are found go in as `primary` together with a transcription; values that are not found stay provisional, and the search is recorded in `spec/sources/search-log.yaml` | `G-VOCAB-SOURCES`, `G-VOCAB-CHECK`. The number of values researched and the number not found are in §9.2 |
| P3-6 | The provenance check `tools/vocabcheck/sources.py` and its self-test, `fx/vocab-ng/primary-*` and `provisional-without-search` | `G-VOCAB-SOURCES`, the matching fixture tests pass. Mutation checks for S465 to S467 |
| P3-7 | Mapping to prior work (D8): fetch the 4 documents again (place them in the scratchpad; get confirmation before fetching), extract the model elements into `spec/notes/prior-work-elements.yaml`, and write the mapping to Ozen in `spec/notes/prior-work.md`. Of the "no match" elements, write the candidates to add to Ozen (held resources, continuity with the previous task, preparation time and so on) in §9.2 and report them to the user. `tools/vocabcheck/priorwork.py` and its self-test | `G-VOCAB-PRIORWORK`. Mutation checks for S469 and S470 |
| P3-4 | Gate A (run after P3-5 to P3-7). The `## Vocabulary table` section of the README (usage commands) | The goals of P2-4, `G-VOCAB-FCT`, `G-VOCAB-FCT-STABLE`, `G-VOCAB-SOURCES`, `G-VOCAB-PRIORWORK`, `G-REPO-NOTICE`, `G-REPO-RED-FIRST` |

### Phase 4 — B: Prototype base and evaluator

| ID | Content | Completion condition |
| --- | --- | --- |
| P4-1 | `proto/CMakeLists.txt` (4 configurations, warning flags, `CMAKE_C_EXTENSIONS OFF`, `CMAKE_EXPORT_COMPILE_COMMANDS ON`, the `ozpi_` rule), `build-all.sh`, `check-flags.py` and `check-compilers.sh` with self-tests, `include/ozenproto.h`, allocation functions and the arena (poison padding in the ASan configuration, S148), the diagnostic registry and `check-diag-registry.sh` | `G-PROTO-BUILD`, `G-PROTO-FLAGS`, `G-PROTO-COMPILERS`, `G-PROTO-EXPORTS`, `G-PROTO-DIAG-APPEND`, `c/arena_*` (including `c/arena_poison_detects`) pass in the 4 configurations. Mutation checks for S113, S116, S124, S131 (compiler row) and S148 |
| P4-2 | 128-bit integers (without `__int128`), rationals and intervals, with their unit tests | `c/rational_*`, `c/u128_*` and `c/rational_overflow_bounds` pass in the 4 configurations, `G-PROTO-NOINT128`. Mutation checks for S77 (unit test row) and S149 |
| P4-3 | First write `proto/tests/diag-names.txt` (a copy of the diagnostic names in §4.8 to §4.14) and the expectations of `golden/syntax/*` and `golden/diag/*` in a separate commit. Lexer, parser, diagnostics, output format, driver (including `--count-allocs` and `--fail-alloc-at`) | `G-PROTO-GOLDEN` (the golden files at this point), `sh/test-driver.sh`, `G-PROTO-GOLDEN-INDEPENDENT`, `G-REPO-RED-FIRST` (B). Mutation checks for S62 to S69, S71, S112, S114, S123, S142 to S144 and S146 |
| P4-4 | First write `golden/values/*` and `golden/control/*`. Values and operations, evaluation, entry points, argument overrides. The rational oracle | `G-PROTO-GOLDEN`, `G-PROTO-RATIONAL-ORACLE`, `G-PROTO-GOLDEN-INDEPENDENT`. Mutation checks for S70, S72 to S92, S94, S95 and S115 |
| P4-5 | First write `golden/macro/*`. Macros, marks, splicing, checks at definition time, expansion history | `G-PROTO-GOLDEN`, `G-PROTO-GOLDEN-INDEPENDENT`. Mutation checks for S96 to S106 and S145 |
| P4-6 | First write `golden/fuel/*` (hand calculations of §4.12). Limits on fuel, depth and generated nodes, output when stopped, `c/limits_defaults` | `G-PROTO-GOLDEN`, `G-PROTO-UNIT`, `G-PROTO-GOLDEN-INDEPENDENT`, `G-PROTO-DIAG-REGISTRY`. Mutation checks for S93, S95 (fuel row), S106 (limit row), S107 to S111 and S147 |
| P4-7 | `proto/scripts/digest.sh` and its self-test (changing one input changes the digest) | `proto/scripts/test-digest.sh` prints `0 failed` |
| P4-8 | Allocation failure injection (`ozp_oom_test`), `c/alloc_balance`, `check-pure.sh` and `check-noglobal.py` with self-tests, checks for floating point, global variables and determinism | `G-PROTO-OOM`, `G-PROTO-PURE`, `sh/test-check-pure.sh`, `G-PROTO-NOGLOBAL`, `G-PROTO-NOFLOAT`, `G-PROTO-DETERMINISM`, `G-PROTO-UNIT`. Mutation checks for S117 to S122 |
| P4-9 | Gate B1 | `G-PROTO-BUILD`, `G-PROTO-UNIT`, `G-PROTO-GOLDEN` (4 configurations), `G-PROTO-RATIONAL-ORACLE`, `G-PROTO-OOM`, `G-PROTO-PURE`, `G-PROTO-NOGLOBAL`, `G-PROTO-NOFLOAT`, `G-PROTO-EXPORTS`, `G-PROTO-DETERMINISM`, `G-PROTO-DIAG-REGISTRY`, `G-PROTO-DIAG-APPEND`, `G-PROTO-GOLDEN-INDEPENDENT`, `G-PROTO-FLAGS`, `G-PROTO-COMPILERS`, `G-PROTO-NOINT128`, `G-REPO-SPDX`, `G-REPO-TOOLS`, `G-REPO-SELFTESTS`, `G-REPO-RED-FIRST`. Mutation check for S125 |

### Phase 5 — B: Fuzzing and environments

| ID | Content | Completion condition |
| --- | --- | --- |
| P5-1 | `proto/fuzz/fuzz_expand.c` (calls `ozp_expand` with small limits), `ozen.dict`, `fuzz.sh` (`-max_total_time=600 -timeout=5 -max_len=65536`; writes results to `build/fuzz/summary.txt`). Use the golden inputs as the seed corpus and run for 600 seconds. Fix any problems found, record them in `proto/defects.tsv`, and run another 600 seconds after fixing. Shrink the corpus with `-merge=1` and place at most 300 entries in `proto/tests/corpus/` | `G-PROTO-FUZZ`, `ls proto/tests/corpus \| wc -l` is 1 to 300, `G-PROTO-DETERMINISM` (including the corpus). Mutation check for S126 |
| P5-2 | Emscripten configurations (for node: `emcmake`, `NODERAWFS`. For the browser: `-sENVIRONMENT=web,worker` into `build/wasm-web/`), `bridge-test.mjs`, `worker-test.html`, `worker.js` and `worker-test.sh` with a self-test | `G-PROTO-WASM`, `G-PROTO-WASM-BRIDGE`, `G-PROTO-WASM-WORKER`. Mutation checks for S64 (bridge row), S127, S128 and S150 |
| P5-3 | As in D19 (the repository `atsuki-seo/Ozen` was created on 2026-10-26): confirm with the user before pushing, and run `G-REPO-NOREPLY`. `.github/workflows/ios-proto.yml` (pin the runner image and the Xcode version, and compare `xcodebuild -version` with `toolchain.txt`. Static libraries for device and simulator, `xcodebuild -create-xcframework`, golden and digest on the simulator, artifacts `ios-digest` (`digest.txt`) and `xcframework` (`OzenProto.xcframework.zip`)) | `G-PROTO-IOS`. Mutation checks for S129, S130 and S131 (iOS row) (push the broken state on a throwaway branch and delete it afterwards; confirm before deleting) |
| P5-4 | Gate B2 | The goals of P4-9, `G-PROTO-FUZZ`, `G-PROTO-WASM`, `G-PROTO-WASM-BRIDGE`, `G-PROTO-WASM-WORKER`, `G-PROTO-IOS` (when the Recommendation of D19 is taken) |

### Phase 6 — C: Prototype report

| ID | Content | Completion condition |
| --- | --- | --- |
| P6-1 | The `## Evaluator prototype` section of the README (how to build, usage examples and output), `check-readme-example.sh` | `G-PROTO-README-EXAMPLE`. Mutation check for S136 |
| P6-2 | The report `docs/prototype-report.md` (the 4 sections of §4.16. Numbers are written from `build/results/`, `build/fuzz/summary.txt` and `proto/defects.tsv`. Under "decisions for the user", list the parts to carry into production as D12 says, and the points to fix in the evaluator's approach), `check-report.py` and its self-test | `G-PROTO-REPORT`, `proto/scripts/test-check-report.sh` prints `0 failed`. Mutation checks for S132 to S135 |
| P6-3 | Confirm that the prototype has no version (D30) | `G-PROTO-VERSION` |
| P6-4 | Gate C | The goals of P5-4 and `G-PROTO-README-EXAMPLE`, `G-PROTO-REPORT`, `G-PROTO-VERSION` |

### Phase 7 — D: Core base (§13-4)

| ID | Content | Completion condition |
| --- | --- | --- |
| P7-0 | Details: write `core/grammar.ebnf` from concept §4. Add the golden output format (diagnostic lines) and the diagnostic names of D to §4.20 and `core/tests/diag-names.txt`. Write in §9 the plan for moving the parts to carry over from the parts list of the report (C) into `core/` together with their tests. Add rows to the coverage table | `G-PLAN-COVERAGE`. The number of rules in `core/grammar.ebnf` is in §9 |
| P7-1 | `core/CMakeLists.txt` (4 configurations, `-Wswitch-enum`, `CMAKE_C_EXTENSIONS OFF`, `compile_commands.json`, the `ozni_` rule), `scripts/build-core.sh`, `scripts/check-c23-subset.sh` (12 probes and forbidden features), the diagnostic registry, the CLI skeleton (`ozen check FILE`, `ozen debug …`), `scripts/run-golden.sh` (`.ozen` golden files and folder golden files, replacing `{{STD_VERSION}}`, `timeout 10` per case, writing to `build/results/`), and `scripts/digest.sh` and `scripts/check-grammar-coverage.py`. Adapt the B scripts for the core (`--std-gcc`, `--std-clang` and `--extra` of `check-flags.py`; 10 categories, `--registry`, `--names` and `--outputs` for `check-diag-registry.sh`). A self-test for each | `G-CORE-BUILD`, `G-CORE-FLAGS`, `G-CORE-C23-SUBSET` (t = 2), `G-CORE-DIAG-REGISTRY`, `G-CORE-DIAG-APPEND`, `G-REPO-SELFTESTS`. Mutation checks for S151, S266 and S268 |
| P7-2 | Arena (with poison padding), interning, spans, the diagnostic array. Move the parts carried over from B (128-bit, rationals, UTF-8) together with their tests | `cunit/*` (the P7-2 share) pass in the 4 configurations, `G-CORE-ARENA-ACCESS`, `G-CORE-NOINT128`. Mutation checks for S152 and S153 |
| P7-3 | First write the expectations of `cgolden/syntax/*`, `cgolden/decl/*`, `cgolden/typeargs/*` and `cgolden/diag/code-and-name`. Lexer and parser for the full syntax, AST dump (`ozen debug ast`, and the rules used with `--rules`), `prop/reindent` | `G-CORE-GOLDEN`, `G-CORE-GRAMMAR`, `G-CORE-PROPS` (`reindent`), `G-REPO-RED-FIRST`. Mutation checks for S154, S156 to S160, S260 and S410 to S412 |
| P7-4 | First write `cgolden/names/*`, `cgolden/modules/*` and `cgolden/packages/*`. Modules, `import`, `local`, forward references, entry points, packages and versions. As D28 says, convert the table into `stdlib/` exactly once (`tools/std-from-vocab/`), set `VERSION` to `0.1.0` (D30), and embed `std` (generate a C array at build time, `scripts/concat-std.sh`). `ozen debug definitions` (S462) and `tools/stdcheck/` (`lists.py`, `static.py`, `provenance.py`) with a self-test. Write the retired tests in `scripts/retired-tests.txt` | `G-CORE-GOLDEN`, `G-CORE-STD-EMBED`, `G-STD-LISTS`, `G-STD-STATIC`, `G-STD-SOURCES`, `G-REPO-VERSION`. Running the conversion again without changing `spec/vocab/` gives no diff in `stdlib/`. Mutation checks for S161 to S172, S174 to S177, S179, S413, S416, S426, S427, S458 to S462 and S468 |
| P7-5 | Limits, allocation failure, purity, global variables, floating point, determinism, fuzzing (`parse`; `scripts/fuzz-core.sh`). `cgolden/limits/*` | `G-CORE-OOM`, `G-CORE-PURE`, `G-CORE-NOGLOBAL`, `G-CORE-NOFLOAT`, `G-CORE-DETERMINISM`, `G-CORE-FUZZ` (`parse`). Mutation checks for S180 to S185 |
| P7-6 | Gate D | The goals of P7-1 to P7-5, `G-CORE-UNIT`, `G-REPO-SPDX`, `G-REPO-SELFTESTS`, `G-REPO-RED-FIRST`, `G-REPO-TOOLS` |

### Phase 8 — E: Type check (§13-5)

| ID | Content | Completion condition |
| --- | --- | --- |
| P8-0 | Details: add to §4.21 the diagnostic names of E, a mapping from the 17 examples of the "severity criteria" table and the examples of the category table in concept §6.1 to diagnostic names (`core/tests/severity-examples.txt`, with columns for severity, category and phase), and the table of how fuel is counted for core nodes (the prototype table of §4.12 extended to core nodes). `scripts/check-severity-table.py` and its self-test. Add rows to the coverage table | `G-PLAN-COVERAGE`, `cgolden/std/package` passes |
| P8-1 | First write `cgolden/dim/*`. Dimensions, units, conversion, vague amounts, numbers and fractions, `cup` | `G-CORE-GOLDEN`. Mutation checks for S186 to S189 and S193 to S195 |
| P8-2 | First write `cgolden/state/*`. State axes, requirements and effects, reverse order, author-defined axes, checks at definition time | `G-CORE-GOLDEN`. Mutation checks for S196 to S205 and S474 |
| P8-3 | First write `cgolden/safety/*` (except `allergen-bowl`), `cgolden/mix/*` and `cgolden/linear/*`. Safety, mixing, linearity | `G-CORE-GOLDEN`. Mutation checks for S206, S207 and S210 to S217 |
| P8-4 | First write `cgolden/parts/*`, `cgolden/actions/*` (except capacity and composition) and `cgolden/names/arity-*`. Parts, aliases, the makeup of `Mixture`, breaking down (fish and meat), `uses`, `variant`, `extend action`, `requires`, the ban on overloading, `where`, argument counts | `G-CORE-GOLDEN`. Mutation checks for S218 to S222, S226 to S229, S414 and S417 to S421 (S224 needs kitchen injection, so it goes in P8-5) |
| P8-5 | First write `cgolden/kitchen/*` (except `cleanup-without-kitchen`, `check-fuel` and `kitchen-has-branch`), `cgolden/vessels/*` (required and capacity), `cgolden/actions/capacity-from-amount` and `cgolden/actions/uses-capability`. Kitchen and condition types, intervals, `check`, narrowing of `variant`, `requires` and the kitchen, default locations, the required constraint and capacity of vessels | `G-CORE-GOLDEN`. Mutation checks for S224, S225, S230 to S235, S238, S241, S429 and S430 (S236 needs compile-time evaluation, so it goes in P8-7) |
| P8-6 | First write `cgolden/menus/*` (except the assembly order and `plate-overrides-dish`). Links between menus and dishes, multiple outputs | `G-CORE-GOLDEN`. Mutation checks for S242 to S245, S247 to S252 and S422 |
| P8-7 | First write `cgolden/eval/*`, `cgolden/macro/*`, `cgolden/kitchen/check-fuel`, `cgolden/kitchen/kitchen-has-branch` and `cgolden/kitchen/capacity-compile-time`. Compile-time evaluation and macros (the B approach on the production AST) | `G-CORE-GOLDEN`, `G-CORE-FUZZ` (`expand`). Mutation checks for S155, S236, S253 to S259, S431 and S432 |
| P8-8 | First write `cgolden/diag/*` (except the P7-3 share), `cgolden/dim/deny-strict-annotation`, `cgolden/safety/*-saf*`, `cgolden/syntax/annotation-target`, `cgolden/packages/deprecated-warning` and so on. The severity mechanism, annotations, `@expect`, priority, suggested fixes, suppression in dependencies, fixed `SAF`, `check` codes (D32) | `G-CORE-GOLDEN`, `G-TYPE-SEVERITY-TABLE` (`--phase E`). Mutation checks for S178, S190 to S192, S208, S261 to S265, S267, S415, S428 and S464 |
| P8-9 | First write `cgolden/levels/*` (except the P9 share) and `cgolden/actions/extend-in-assumptions`. Verification levels, `check`, assumptions, monotonicity, `prop/monotonic` | `G-CORE-GOLDEN`, `G-CORE-PROPS` (`monotonic`), `G-CORE-FUZZ` (`typecheck`). Mutation checks for S237, S270 to S273, S275 and S424 |
| P8-10 | Gate E | The goals of P7-6 and the goals of P8 |

### Phase 9 — F: IR and schedule (§13-6)

| ID | Content | Completion condition |
| --- | --- | --- |
| P9-0 | Details: add to §4.22 the format of the text IR dump, the text and JSON formats of the schedule, and the list of checks in `tools/schedcheck/`. Write in §9 the procedure for building the expected schedule of small examples by hand. Add rows to the coverage table | `G-PLAN-COVERAGE` |
| P9-1 | First write `cgolden/ir/*`. IR, text IR dump, `prop/display_only`, `prop/ir_invariant`, `prop/split_merge`, `prop/meta_ignored` (compared on the text IR) | `G-CORE-GOLDEN`, `G-CORE-PROPS` (`display_only`, `ir_invariant`, `split_merge`, `meta_ignored`), `cunit/ir_flat_ids`. Mutation checks for S173, S223, S278 to S283, S285, S286 and S463 |
| P9-2 | `tools/schedcheck/` (a schedule checker written on a path separate from the implementation) and its self-test (fails on schedules that break one constraint at a time) | `sh/test-schedcheck.sh` prints `0 failed` |
| P9-3 | First write `cgolden/sched/*` (the basic share), `cgolden/sched/parallel-join`, `cgolden/levels/level-1-human-only` and the expected schedules built by hand. Resource-constrained list scheduling, priority, ties, forward and backward, unattended time, duration ranges, shortest time, fork-join, level 1 | `G-CORE-GOLDEN`, `G-SCHED-CHECK`. Mutation checks for S274, S277, S287 to S290, S294, S304, S433 and S436 |
| P9-4 | First write `cgolden/sched/*` (the share for equipment, candidates, adjustments, heating, hardness, capacity and cooks) and `cgolden/actions/composite-*`. Expansion of composite actions | `G-CORE-GOLDEN`, `G-SCHED-CHECK`. Mutation checks for S295 to S303, S434, S438 and S439 |
| P9-5 | First write `cgolden/wash/*`, `cgolden/cleanup/*` (except during the meal), `cgolden/safety/allergen-bowl`, `cgolden/levels/small-tools-assumed`, `cgolden/sched/store-discard-in-schedule` and `cgolden/sched/veg-before-meat`. Washing, wiping, compatibility rules, cleanup, allergens through tools | `G-CORE-GOLDEN`, `G-SCHED-CHECK`, `G-CORE-PROPS` (`ir_invariant`). Mutation checks for S209, S276, S284, S305 to S315, S437 and S440 to S442 |
| P9-6 | First write `cgolden/serve/*`, `cgolden/vessels/*` (count, alternative order, sharing out), `cgolden/menus/assemble-order`, `cgolden/menus/plate-overrides-dish`, `cgolden/sched/meal-*`, `cgolden/sched/shared-cutting` and `cgolden/cleanup/dishwasher-during-meal`. Serving times, plating, vessel assignment, waves and during the meal, sharing | `G-CORE-GOLDEN`, `G-SCHED-CHECK`. Mutation checks for S239, S240, S246, S316 to S326, S423 and S435 |
| P9-7 | First write `cgolden/sched/objective-order`, `prefer-*` and `improvement`. Order of objectives, `--prefer`, improvement | `G-CORE-GOLDEN`, `G-SCHED-CHECK`, `G-CORE-FUZZ` (`schedule`), `G-TYPE-SEVERITY-TABLE` (all 17 examples). Mutation checks for S291 to S293 and S457 |
| P9-8 | Gate F | The goals of P8-10 and the goals of P9 |

### Phase 10 — G: Public API, JSON, CLI (§13-7)

| ID | Content | Completion condition |
| --- | --- | --- |
| P10-0 | Details: add to §4.23 and §4.25 the JSON Schema of `schemas/` (every object closed with `additionalProperties: false`), the Ozen text form of `options`, and the CLI's display form for diagnostics. Add rows to the coverage table | `G-PLAN-COVERAGE` |
| P10-1 | Public API and header (D25), `cunit/api_*` | `G-API-EXPORTS`, `G-API-HEADER`, `cunit/api_*` pass. Mutation checks for S327 to S329 and S336 |
| P10-2 | JSON output (results, diagnostics, the list of action signatures, the list of all definitions, ingredients for the text, type definitions) and `schemas/`, `tools/jsoncheck/` and its self-test | `G-API-SCHEMA`, `G-STD-LISTS` (on the public output). Mutation checks for S330 to S332, S334, S335, S337 and S425 |
| P10-3 | Conversion to SARIF (CLI). Before fetching the SARIF 2.1.0 schema, show the file name, source and size and get confirmation; place it in `schemas/vendor/` and write the version and sha256 in `schemas/vendor/README.md` | `G-API-SARIF`, `G-REND-VENDOR` (the SARIF share). Mutation check for S333 |
| P10-4 | First write `cli/*`. CLI subcommands, entry points, how the kitchen is found, short options, `--options`, project builds, `out/`, `index.json`, option conversion, display of diagnostics. `scripts/run-cli-tests.sh` and its self-test | `G-CLI-TESTS`. Mutation checks for S345 to S356 and S443 |
| P10-5 | Confirm that the versions of D30 match (`VERSION`, CLI, `std`, library) | `G-REPO-VERSION` |
| P10-6 | Gate G | The goals of P9-8 and the goals of P10 |

### Phase 11 — H: 4 environments and CI (§13-8)

| ID | Content | Completion condition |
| --- | --- | --- |
| P11-0 | Details: add to §4.24 the names of the artifacts for each environment, the form of the bridge APIs (Swift, Java, JS), that bridge agreement is compared on the JSON string of `recipe_build`, the `javac --release` version (D29), and the list of CI job names. Add rows to the coverage table | `G-PLAN-COVERAGE` |
| P11-1 | Linux shared library, a test that calls it from Swift on Linux through a module map, and a test that tries the JNI wrapper and Java class on a Linux JVM | `G-PLAT-LINUX`. Mutation checks for S340 (Linux row), S341 and S343 (Linux row) |
| P11-2 | Core WASM (for node and for Web Workers) and the JS bridge | `G-PLAT-WASM`. Mutation check for S342 |
| P11-3 | When the Recommendation of D19 is taken: the `ios` job of the workflow `ci.yml` (an xcframework with 3 slices: iOS device, iOS simulator and macOS; Swift tests on the iOS simulator and macOS; golden and sample digest on the simulator; the 12 probes in Xcode). Before pushing, show the range to push and get confirmation | `G-PLAT-IOS`, `G-CORE-C23-SUBSET` (including Xcode). Mutation checks for S151 (Xcode row), S338 and S455 |
| P11-4 | When the Recommendation of D19 is taken: the `android` job (4 ABIs with NDK r30, golden and sample digest on the emulator, JNI tests). Get confirmation before pushing. If the NDK is used locally, get confirmation before fetching it | `G-PLAT-ANDROID`, `G-CORE-C23-SUBSET` (t = 5). Mutation checks for S339 and S340 (Android row) |
| P11-5 | The whole CI (4 Linux configurations, ASan and UBSan, 60 seconds of fuzzing per target, WASM, iOS and macOS, Android, each job printing its tool versions). Write the list of job names in `ci/jobs.txt`, `scripts/check-ci-tools.py` and its self-test. Get confirmation before pushing | `G-PLAT-CI`, `G-REPO-TOOLS-CI`. Mutation check for S344 |
| P11-6 | Gate H | The goals of P10-6 and the goals of P11 |

### Phase 12 — I: Minimal sample (§13-9)

| ID | Content | Completion condition |
| --- | --- | --- |
| P12-1 | First write `expect.yaml` of `samples/curryRice/` (the sample of concept §4.4, fixed as §3.4 says). `tests/samples/check.py` and its self-test | `G-SAMPLE-CHECK` (`curryRice`), `G-SCHED-CHECK`. Mutation check for S363 |
| P12-2 | Run the minimal sample in all environments and compare digests (get confirmation before pushing for CI) | `G-PLAT-LINUX`, `G-PLAT-WASM`, `G-PLAT-IOS`, `G-PLAT-ANDROID` (digest including the sample) |

### Phase 13 — J: Standard library, reference menus, per-feature tests (§13-10)

| ID | Content | Completion condition |
| --- | --- | --- |
| P13-0 | Details: add to §4.27 the list of properties to write in the `expect.yaml` of the reference menus and T1 to T4. Add rows to the coverage table | `G-PLAN-COVERAGE` |
| P13-1 | Coverage of `std` (lists and relations, provenance, prelude, the same IR as the transcription) | `G-STD-LISTS`, `G-STD-PROVENANCE`, `G-CORE-PROPS` (`std_copy`). Mutation checks for S358 to S361 |
| P13-2 | `phrases/ja.ftl` and `en.ftl` (all keys of `std`) and the drift check tool `renderers/tools/phrases-check.mjs` | `G-STD-PHRASES`. Mutation check for S362 |
| P13-3 | First write the `expect.yaml` of `samples/menuA` to `menuE`. Write the menus (with the `std` vocabulary only) | `G-SAMPLE-CHECK`, `G-SCHED-CHECK`, `G-SAMPLE-STD-ONLY`. Mutation checks for S364 to S368 and S456 |
| P13-4 | First write the `expect.yaml` of `samples/T1` to `T4` (T3 has a and b). Write the test recipes | `G-SAMPLE-CHECK`, `G-SCHED-CHECK`. Mutation checks for S369 to S372 |
| P13-5 | Run all samples in all environments (get confirmation before pushing for CI) | `G-SCHED-CHECK`, the digest of `G-PLAT-*` (including all samples). Mutation checks for S373 and S374 |
| P13-6 | Gates I and J | The goals of P11-6 and the goals of P12 and P13 |

### Phase 14 — K: Renderer and settings screen (§13-11)

| ID | Content | Completion condition |
| --- | --- | --- |
| P14-0 | Details: add to §4.28 the output forms of the text, the schedule table, HTML, the Gantt chart, the shopping list and the settings screen, and how the golden files are made. Add rows to the coverage table | `G-PLAN-COVERAGE` |
| P14-1 | Bringing in Fluent (D23; get confirmation before fetching), layering of phrases, fallbacks, choice of names, part names, drift check, diagnostic messages, different wording for rinsing (rice) and washing | `G-REND-TESTS` (the P14-1 share), `G-REND-VENDOR`, `G-STD-PHRASES`. Mutation checks for S384 to S389, S446 and S447 |
| P14-2 | First write the expectations of `rtest/recipe-text` and others. Recipe text, merging sentences, `@group`, counter words, words for vague times, separation by wave name, ingredient order | `G-REND-TESTS`. Mutation checks for S375, S377, S378, S390, S391, S445 and S450 |
| P14-3 | Schedule table, chapters by wave, the split between prep and cooking, HTML (self-contained, CSS for printing, well-formed), Gantt chart, shopping list, text for candidates, washing and assumptions, the book form. `tools/htmlcheck/` and `renderers/tools/render-all.mjs` with self-tests | `G-REND-TESTS`, `G-REND-SELFCONTAINED`, `G-REND-HTML`, `G-REND-PHRASES-RENDER`. Mutation checks for S376, S379 to S383, S392, S448, S449, S451, S452 and S454 |
| P14-4 | The settings screen (an HTML form) from type definitions, and kitchen and condition text from the form values | `G-REND-TESTS` (`settings-form-*`), `G-REND-HTML` (form). Mutation checks for S393, S394 and S453 |
| P14-5 | `ozen render`, a script that bundles the renderer into one file (D22), checks of the bundle in a browser (headless Chrome, `renderers/tools/browser-test.sh` and its self-test) | `G-CLI-TESTS` (`cli/render`), `G-REND-BROWSER-SAFE`. Mutation checks for S395 and S396 |
| P14-6 | Gate K | The goals of P13-6 and the goals of P14 |

### Phase 15 — L: `ozen bundle` (§13-12)

| ID | Content | Completion condition |
| --- | --- | --- |
| P15-0 | Details: add to §4.29 the detailed form of bundled files. Add rows to the coverage table | `G-PLAN-COVERAGE` |
| P15-1 | First write `cgolden/bundle/*`. `ozen bundle` | `G-CORE-GOLDEN`, `G-BUNDLE-STABLE`. Mutation checks for S397 to S400 and S402 to S406 |
| P15-2 | `ozen unbundle` (does not overwrite existing files), `prop/bundle_roundtrip`, mixed versions on import | `G-CORE-PROPS` (`bundle_roundtrip`), `G-CORE-GOLDEN`, `G-CLI-TESTS` (`cli/unbundle-no-overwrite`). Mutation checks for S401, S407 and S444 |
| P15-3 | Fuzzing of untrusted input (the `bundle` and `kitchen` targets), and size limits for bundled files and kitchen specs (`cgolden/limits-shared/*`) | `G-CORE-FUZZ` (`bundle`, `kitchen`), `G-CORE-GOLDEN`. Mutation check for S408 |
| P15-4 | Gate L | The goals of P14-6 and the goals of P15 |

### Phase 16 — Finishing

| ID | Content | Completion condition |
| --- | --- | --- |
| P16-1 | The whole README (usage of each part, license classes, credits: the food composition table, Fluent, the SARIF schema) | `G-REPO-README-LICENSE`, `G-PROTO-README-EXAMPLE`, running the examples in each README section (`check-readme-example.sh` extended to all sections) |
| P16-2 | As D30 says, confirm that the version is still 0.1.0 in `VERSION` and matches everywhere | `G-REPO-VERSION` |
| P16-3 | Coverage audit: add to §4 and the coverage table the facts that grew in §9 (new registered items, diagnostics, mismatches). Judge again with post-implementation knowledge whether each item of §8 "can be decided by a command", and turn those that can into tests. Have an independent subagent read §1, §4, §8, the coverage table and the sources in `tools/`, `tests/`, `proto/tests/`, `spec/`, `samples/` and `renderers/test/`, and list, with evidence, claims without tests and items in §8 that a command can decide. Add each finding, or write at the end of §5.3 why it is not added. Tests added afterwards go in `scripts/red-first-exceptions.txt` with a reason | `G-PLAN-COVERAGE`, `G-PLAN-TESTS-EXIST`, `scripts/check-plan-tests.sh --mutations` prints `mutations: <n> rows, <n> recorded`. The number of audit findings and how they were handled are in §9.1 |
| P16-4 | Final check. Run all goals on `main` (range `plan-start..HEAD`; goals retired by D28 are left out). Get confirmation before pushing for CI. Hand §8 to the user | All goals of §5 (except `G-VOCAB-SCHEMA`, `CHECK`, `SAMPLE`, `TESTS`, `READONLY` and `EXPECT-INDEPENDENT`) pass on `main`. Report per part and per environment |

---

## 7. Dependencies between phases

- Phase 0 is the prerequisite of everything. The answers in P0-1 change §4 and later, so do not go on to P0-2 and later until P0-1 is done. The order is P0-1 → P0-2 → P0-3 to P0-6 (independent of each other, all on `main`) → P0-7.
- **A (Phase 1 to 3) and B (Phase 4 to 5) are independent** and can proceed in parallel (they touch no common folders, D2). They may be split into separate sessions.
- Within A: P1-1 → P1-2 → P1-3 → P1-4 → P1-5 → P1-6 → P1-7 → P2-1 → P2-2 → P2-3 → P2-4. P3-1 (checking where the data is and fetching it) can be done any time after P0; P3-2 comes after P3-1, and P3-3 after P1-5 and P3-2. P3-5 (primary source research) comes after P1-5, P3-6 after P3-5, P3-7 (mapping to prior work) after P1-5, and P3-4 (the gate) after P3-3, P3-6 and P3-7.
- Within B: P4-1 → P4-2 → P4-3 → P4-4 → P4-5 → P4-6 → P4-7 → P4-8 → P4-9 → P5-1 → P5-2 and P5-3 (independent of each other; both use the P5-1 corpus for the digest) → P5-4 → P6-1 and P6-2 (independent of each other) → P6-3 → P6-4.
- **From D on, phases go in order**: P7 comes after both the A gate (P3-4) and the C gate (P6-4) (P7-0 uses the parts list of the C report, and P8-0 converts the A table). P8 comes after P7, P9 after P8, and so on: each phase starts after the gate of the previous phase.
- Within a phase, `P<n>-0` (details) comes first and the gate comes last. The rest follow the order of the table.
- iOS and Android in H (P11) (P11-3 and P11-4) are independent of each other. Both come after P11-1.
- K (P14) uses the output of the phrases (P13-2) and the samples (P13-3, P13-4) as input, so it comes after J. L (P15) comes after the G CLI (P10-4) and the J samples.

---

## 8. Checks handed over to the user

Claude is responsible up to the mechanical goals. The user checks the items below. Each item gives the reason a command cannot decide it. An item for which no reason can be written becomes a test in §5.3.

Check first (parts that run on a different path in each environment, which the common tests do not prove. The browser Web Worker is checked by machine in S150 and S342):

- On a real iPhone, whether running the golden files and samples, with a small app that includes `Ozen.xcframework` or with the same steps as the simulator, gives the same digest as native (only if a real device is available) — Reason a command cannot decide: the CI runners have no real iPhone, and a real device cannot be connected from the local Linux machine
- On a real Android device, whether running the JNI tests, golden files and samples gives the same digest as native (only if a real device is available) — Reason a command cannot decide: CI and the local emulator are not real devices, and no real device is connected locally

Others:

- Whether the vocabulary and values that concept does not decide are correct as cooking (ranges of provisional values, fineness relations between cuts other than item 7 of §4.7.1, how `Hazard` and `RawEdible` are given other than item 8 of §4.7.1, requirements and effects of actions not in §4.7.1) — Reason a command cannot decide: there is no oracle to judge cooking knowledge that concept does not write down. Relations that concept states explicitly are checked by machine in S139 and S358
- Whether the (added) rules of §4 (S23, S30, S32, S33, S36, S45 to S48, S50, S51, S471 to S473, S285, S331, S336, S383, S442, S443, S444, S454, S463, S464, the trait `Liquid`, the capability `WorkSurface`, and the details added in `P<n>-0` of each phase) match the design intent — Reason a command cannot decide: the intent is not written in concept
- Whether to fix the mismatches of §3.4 on the concept side — Reason a command cannot decide: revising the design document is the author's judgment
- Whether the schedules are natural as cooking (read the schedule tables of the reference menus and T1 to T4, and judge whether one can move in the kitchen without strain) — Reason a command cannot decide: keeping the constraints (`G-SCHED-CHECK`) and the order concept writes (vegetables first on the same cutting board, S442) are checked by machine, but there is no right answer for naturalness beyond that
- Whether the Japanese and English phrases are natural — Reason a command cannot decide: there is no right answer for naturalness. Key coverage and matching arguments (`G-STD-PHRASES`), and zero fallback displays, Fluent errors and identifiers left in the text when rendering all samples (`G-REND-PHRASES-RENDER`, D38), are checked by machine
- The look and readability of the HTML, the Gantt chart and the settings screen — Reason a command cannot decide: the standard is subjective. Self-containment, well-formedness, overlap, range and position in the Gantt chart, fitting in the page width, and form names and units (`G-REND-SELFCONTAINED`, `G-REND-HTML`) are checked by machine
- Choosing the name of a fish that changes names as it grows from value properties or cooking context (the rest of S387) — Reason a command cannot decide: `std` has no growth-stage property, and which name to use in which context is a judgment of cooking custom with no rule in concept (D39 only made the way to pass it through render settings)
- Whether the extraction of prior work elements (`spec/notes/prior-work-elements.yaml`) matches the documents, and deciding which "no match" elements to add to Ozen — Reason a command cannot decide: the documents are not put in the repository, so a person must open them to compare, and whether to add something is a design judgment
- Whether the transcriptions (`quote`) of primary sources match the documents (a few, by sampling) — Reason a command cannot decide: the documents are not put in the repository, so a person must open them to compare. Agreement between the transcription and the value is checked by `G-VOCAB-SOURCES`
- Whether the writing experience and amount of bugs in the prototype, and the amount of problems found by fuzzing, are acceptable (items (1) and (3) of the report), and whether to change how D and later are built as a result — Reason a command cannot decide: the standard of acceptability is subjective, and line counts and defect counts are only material for the judgment

---
- Whether `schemas/vendor/sarif-2.1.0.json` may be redistributed (the OASIS IPR policy and the conditions in the Notices of the specification; whether the text in `schemas/vendor/LICENSE-OASIS.txt` is enough) — Reason a command cannot decide: interpreting license conditions is a legal judgment that a check cannot decide

## 9. Items to check and record when starting

(Add the results checked in each task here, with dates, in a subsection per part)

### 9.1 Common

- Baseline N0 at the starting point (P0-7, 2026-10-26): of the coverage table tests, those in the repository are `sh=5` (self-tests of the common scripts of P0-3 to P0-6), and the other categories are 0 (`golden=0 fx=0 py=0 c=0 sh=5 cgolden=0 cunit=0 prop=0 cli=0 sample=0 rtest=0`). The number of feature tests is 0. The sha of the tag `plan-start` is written in the first record commit after the tag is made
- Tool versions (P0-4): gcc 13.3.0, clang 17.0.0 (swiftly, Swift 6.2.3, LLVM 19.1.5 line), cmake 3.28.3, node v20.20.2, Python 3.12.3, PyYAML 6.0.1, jsonschema 4.10.3, openpyxl 3.1.5, Java 25.0.4.1, google-chrome 154.0.8037.97 (as of 2026-10-25). The versions of emcc, NDK and Xcode are written here
- emsdk (P0-4, installed on 2026-10-26 after the user confirmed): cloned github.com/emscripten-core/emsdk into `~/.local/share/emsdk`, checked out the tag `6.0.13`, and ran `./emsdk install 6.0.13` and `activate 6.0.13`. Fetched `wasm-binaries.tar.xz` (311,791,988 bytes, release hash `eb132757325583b787fe9414b09afea9442b72ad`) and node 24.19.0, which emsdk bundles. Size after install: 1.7 GB. The first line of `emcc --version` is `emcc (Emscripten gcc/clang-like replacement + linker emulating GNU ld) 6.0.13 (e13ee988ce3d87a738dd8f3661d02223308c412a)`. `emcc` is not put on PATH; `check-tools.sh` uses `upstream/emscripten/emcc` under `$EMSDK` (default `~/.local/share/emsdk`)
- P0-4 results (2026-10-26): `toolchain.txt` has 9 lines: gcc 13.3.0, clang 17.0.0, cmake 3.28.3, node 20.20.2, python3 3.12.3, pyyaml 6.0.1, jsonschema 4.10.3, openpyxl 3.1.5, emcc 6.0.13. `G-REPO-TOOLS` (`tools: 9 checked, 0 mismatched`), `test-check-tools: 4 cases, 0 failed`
  - Mutation check: `sh/test-check-tools.sh` (S131, Fixed premise) — failed when the version comparison looked only at the major version (`FAIL patch-differs`, `4 cases, 2 failed`). Passed after restoring
- License texts (P0-3, fetched on 2026-10-26 after the user confirmed):
  - `LICENSE-MIT`: `https://raw.githubusercontent.com/spdx/license-list-data/main/text/MIT.txt` (1,078 bytes, sha256 `b05785f9f18e6716bab63424b11454513b9943a222595b70411009202fc592b5`). sha256 after replacing the copyright line with `Copyright (c) 2026 atsuki.seo`: `8bcf9c408a5d1fd01f810c5ac9f843d6e07e29c063fcd8bb39720ec019347b1c`
  - `LICENSE-APACHE`: `https://www.apache.org/licenses/LICENSE-2.0.txt` (11,358 bytes, sha256 `cfc7749b96f63bd31c3c42b5c471bf756814053e847c10f3eb003417bc523d30`, text unchanged)
  - `LICENSE-CC-BY-4.0`: `https://creativecommons.org/licenses/by/4.0/legalcode.txt` (18,657 bytes, sha256 `9ba9550ad48438d0836ddab3da480b3b69ffa0aac7b7878b5a0039e7ab429411`, text unchanged)
- GitHub repository (2026-10-26, before P5-3 at the user's request): `https://github.com/atsuki-seo/Ozen` (public, default branch `main`, remote `origin` over ssh). Before pushing, following the user's policy "do not publish the email address", rewrote the author and committer email of the 7 commits so far to the GitHub noreply address `146819854+atsuki-seo@users.noreply.github.com` with `git filter-branch --env-filter` (dates unchanged; old objects removed with `reflog expire` and `gc --prune=now`), and set this repository's `user.email` to the noreply address too. The rewrite changed the commit IDs (the first commit changed from `9b1be45`)
- P0-3 goals (2026-10-26): `G-REPO-SPDX` (`spdx: 2 files checked, 0 missing, 0 wrong`, and `--list` also gives 2 files), `G-REPO-LICENSE-TEXT` (1, 1, 0, 1, 3), `G-REPO-README-LICENSE` (1, 1, 1), `G-REPO-CLAUDEMD` (1, 3) and `scripts/test-check-spdx.sh` (`9 cases, 0 failed`) all passed
- Following the user's comment policy (`~/.claude/rules/comment-style.md`), script comments were limited to SPDX lines and comments that involve both an outside constraint and an unusual implementation (the reason `vendor/` is left out). The separator comments in `.gitignore` were also removed (2026-10-26)
- Mutation check records (P0-3, 2026-10-26. In every case `cmp` confirmed that the restored file was byte-for-byte identical to the original):
  - Mutation check: `G-REPO-SPDX` (S1, Normal) — failed when the SPDX line was removed from `scripts/test-check-spdx.sh` (`spdx: 2 files checked, 1 missing, 0 wrong`, exit code 1). Passed after restoring
  - Mutation check: `sh/test-check-spdx.sh` (S1, Boundary) — failed when the check covered the whole file instead of the first 5 lines (`FAIL line-six`, `9 cases, 1 failed`). Passed after restoring
  - Mutation check: `G-REPO-LICENSE-TEXT` (S4, Normal) — failed when the version line was removed from `LICENSE-APACHE` (0 version lines). Passed after restoring
  - Mutation check: `G-REPO-LICENSE-TEXT` (S4, Boundary) — failed when the MIT copyright line was set back to the template (0 copyright lines, 1 template). Passed after restoring
  - Mutation check: `G-REPO-README-LICENSE` (S6, Docs match) — failed when the sentence about how output is treated was removed from the README (0 such sentences). Passed after restoring
  - Mutation check: `G-REPO-README-LICENSE` (S6, Data completeness) — failed when `spec/samples/` in the table was moved to the MIT row (both row counts were 0). Passed after restoring
  - Mutation check: `G-REPO-CLAUDEMD` (S409, Docs match) — failed when the language rules section was removed from `CLAUDE.md` (`docs/` 0, Japanese mention 0). Passed after restoring
  - Mutation check: `G-REPO-NOREPLY` (S475, No-effect condition) — in a throwaway copy, failed after adding an empty commit with a personal email as author (2 kinds of email). The copy was deleted (2026-10-26)
- P0-5 results (2026-10-26): `scripts/check-plan-tests.sh plans/ozen-roadmap.md` printed `golden=0 fx=0 py=0 c=0 sh=5 cgolden=0 cunit=0 prop=0 cli=0 sample=0 rtest=0` and `plan-tests: 544 names, 539 missing` (only the count of missing is recorded at this point). `--mutations` printed `mutations: 550 rows, 12 recorded`. Self-tests: `test-check-plan-tests: 10 cases`, `test-check-lists-copy: 5 cases`, `test-goals: 7 cases`, all `0 failed`. `scripts/goals.sh G-PLAN-COVERAGE` passed
- P0-6 results (2026-10-26): `test-check-commit-separation: 4 cases`, `test-check-red-first: 5 cases`, `test-check-selftests: 5 cases`, all `0 failed`. `G-REPO-SELFTESTS` printed `selftests: 7 scripts, 0 without test, 0 failed`. Test names for fixture expectations are `test_fx_<種類>_<名前>` (`-` becomes `_`), and `check-red-first.sh` runs `unittest -k` with that name (P1-2 generates the tests with this name)
- Mutation check records (P0-5, P0-6, 2026-10-26):
  - Mutation check: `G-REPO-SELFTESTS` (plan, Data completeness) — failed when `scripts/test-check-lists-copy.sh` was moved away (`selftests: 7 scripts, 1 without test, 0 failed`, exit code 1). Passed after restoring
  - Mutation check: `G-PLAN-TESTS-EXIST` (plan, Data completeness) — when `scripts/test-check-spdx.sh` was moved away, missing rose from 539 to 540 and `sh` fell from 5 to 4. Back to the original counts after restoring
  - Mutation check: `sh/test-check-commit-separation.sh` (D20, negative) — in a throwaway repository, failed when a fixture and `tools/vocabcheck/` were changed in the same commit (`separation: 1 commits, 1 mixed`, exit code 1). The repository was deleted
  - Mutation check: `sh/test-check-red-first.sh` (D20, Time and order) — in a throwaway repository, failed when the implementation went in first and `renderers/test/a.test.mjs` was added in the next commit (`red-first: 1 commits, 1 not red`, exit code 1). The repository was deleted
- List of CI job names (P11-5)
- Version: 0.1.0 (D30. Written in `VERSION` in P7-4, and not raised within this plan)
- P16-1 (2026-10-27): wrote the README sections "Core and CLI", "Standard library", "Platforms", "Renderer" and "Bundles" and "Credits" (the food composition table, Fluent, the SARIF schema), and updated "Status" to the 0.1.0 state. Extended `proto/scripts/check-readme-example.sh` to the examples in all sections (`<!-- file: NAME -->` gives the file name of an ozen block, `<!-- run: CMD -->` gives the command that produces an output block (`ozen` is the core CLI, `ozenproto` is the prototype), and a file carries over to later output examples until a new ozen block comes. Self-tests went from 3 to 6). Added Fluent and the SARIF schema to NOTICE. `G-PROTO-README-EXAMPLE` (`readme: 4 examples, 0 mismatches`), `G-REPO-README-LICENSE`, `G-REPO-NOTICE`, `G-REPO-SELFTESTS`, `G-REPO-LICENSE-TEXT` and `G-REPO-CLAUDEMD` passed
- P16-2 (2026-10-27): the versions in `VERSION`, `ozen --version`, `stdlib/package.ozen` and this plan are all still 0.1.0. `G-REPO-VERSION` passed
- Mutation checks found in P16-3 that had no record (2026-10-27. Those that `check-plan-tests.sh --mutations` counted as having fewer records than coverage table rows were done on the spot):
  - Mutation check: `G-TYPE-SEVERITY-TABLE` (S269, Data completeness) — failed when the default for exceeding a vessel's capacity (`vesselOverflow`) was made an error (`severity: 17 examples, 1 mismatched`). Passed after restoring
  - Mutation check: `G-PROTO-NOINT128` (D15, negative) — failed when an `unsigned __int128` variable was added to the prototype source (`int128: got '1'`). Passed after restoring
  - Mutation check: `G-PROTO-VERSION` (D30, negative) — failed when `project(ozenproto VERSION 0.0.1 LANGUAGES C)` was written (`project VERSION` and `VERSION words` were 1). Passed after restoring
  - Mutation check: `G-REPO-VERSION` (D30, Time and order) — failed when only the version in `stdlib/package.ozen` was raised to 0.1.1 (`stdlib version: got '0'`). Passed after restoring
- Mutation checks of the tests added in the P16-3 audit (2026-10-27. For the import pruning of `S399`, removing the module-level check was not detected at first because pruning still happened per name, so it was broken again in a way that keeps the names. For `S377`, the first negative test stopped on a different condition (a structural task) and could not detect the break, so it was changed to an example of a task that takes 2 inputs. For `S401`, the decision that it may skip was not detected by the text check (which evaluates all dishes), so it was changed to the JSON check (only dishes reachable from the entry point)):
  - Mutation check: `cli/render-entry` (module name given, default with no target, error results, ambiguous entry point) (S395, Real path) — failed when the entry point was not resolved for a module-only target. Passed after restoring
  - Mutation check: `cli/render-dep-phrases` (S385, Real path) — failed when the phrases of dependencies were not passed. Passed after restoring
  - Mutation check: `cli/render-stage` (D39. Growth stage and dialect in render settings) (S387, Real path) — failed when the stage setting was not passed. Passed after restoring
  - Mutation check: `G-REND-PHRASES-RENDER` (identifiers, D38) (S451, Data completeness) — failed when diagnostic arguments were output as identifiers. Passed after restoring
  - Mutation check: `sh/test-htmlcheck.sh` (unitless. Whether something is an amount is decided by the type's dimension) (S453, Data completeness) — failed when whether something is an amount was decided by `data-quantity` alone. Passed after restoring
  - Mutation check: `G-REND-HTML` (Gantt position. Checking position and width from `data-label-width` and `data-per-minute`) (S452, Boundary) — failed when a rectangle's position was shifted by 1 minute. Passed after restoring
  - Mutation check: `G-REND-HTML` (page width. A chart wider than 720 needs `svg{max-width:100%}`) (S452, Boundary) — failed when the SVG `max-width` was removed. Passed after restoring
  - Mutation check: `rtest/prep-cook-split` (exactly at the split, `prepBefore` of 0 and 3600) (S380, Boundary) — failed when the split comparison was changed to `<`. Passed after restoring
  - Mutation check: `rtest/gantt-svg` (resource rows and holding) (S382, Normal) — failed when resource rows were not drawn. Passed after restoring
  - Mutation check: `rtest/settings-form-roundtrip` (running the script embedded in the page against a fake form) (S393, Real path) — failed when the embedded function called a function outside the module. Passed after restoring
  - Mutation check: `rtest/merge-sentences` (do not merge when the next task takes 2 inputs) (S377, No-effect condition) — failed when sentences were merged without looking at the number of inputs. Passed after restoring
  - Mutation check: `rtest/shopping-list` (different dimensions go on separate lines) (S383, Data completeness) — failed when amounts were added without looking at dimensions. Passed after restoring
  - Mutation check: `G-REND-HTML` (book form. `render-all.mjs` writes a book per sample) (S454, Data completeness) — failed when the book form output an unclosed element. Passed after restoring
  - Mutation check: `rtest/recipe-schedule` (a schedule without waves) (S379, Normal) — failed when the tasks of a schedule without waves were put into cleanup. Passed after restoring
  - Mutation check: `cgolden/bundle/local-import-rejected` (S406, No-effect condition) — failed when a `local` import was allowed in inline modules. Passed after restoring
  - Mutation check: `cgolden/bundle/std-version-incompatible` (S404, Boundary) — failed when the major version was not compared. Passed after restoring
  - Mutation check: `cgolden/bundle/std-version-bad` (S404, Boundary) — failed when the version format was not checked. Passed after restoring
  - Mutation check: `prop/bundle_roundtrip` (an entry point that cannot be bundled is skipped only when the error is in the entry point) (S401, Normal) — failed when bundling was refused because of an error in a dish unrelated to the entry point. Passed after restoring
  - Mutation check: `cgolden/bundle/global-effects` (`extend`, `rule`) (S399, Data completeness) — failed when declarations with global effect were traced from names. Passed after restoring
  - Mutation check: `cgolden/bundle/same-name` (S399, Normal) — failed when only one of the candidates with the same name was included. Passed after restoring
  - Mutation check: `cgolden/bundle/import-pruned` (S399, Normal) — failed when imports of unused names were kept. Passed after restoring
  - Mutation check: `cli/bundle-phrases-quote` (S399, No-effect condition) — failed when `"""` in the phrases was not checked. Passed after restoring
  - Mutation check: `cli/unbundle-manifest-taken` (clashes under `package.ozen` and dependencies) (S444, Persistence) — failed when only the module files were checked. Passed after restoring
- Coverage audit (P16-3, 2026-10-27): 19 findings, 17 taken (the 2 not taken and the reasons are at the end of §5.3). 5 real bugs were found; tests were added first and then they were fixed: `ozen render` did not resolve the entry point and rendered a different dish when only a module name was given or the target was omitted; the renderer crashed with an exception on results with errors; `ozen render` did not pass the phrases of dependencies; all tasks of a recipe-only schedule went into "cleanup"; `ozen bundle` dropped `extend` and `rule` of unused modules, which changed the entry point candidates. In addition, the following were fixed by adding tests and checks: diagnostic arguments appeared as identifiers in note sentences (D38); the growth stage of fish was not passed on the real path (D39); the S453 check judged amounts by whether a unit was present and could not detect the break; the Gantt position and page width were not checked; and details with no tests (the no-merge case, different dimensions, the book form check, and 7 items of the bundle mechanism)
- P16-4 final check (2026-10-27, up to `2af02fa` on `main`; local machine is Linux): of the 87 goals excluding the 6 retired by D28, ran the 82 that can be decided locally (80 in `scripts/goals.sh`, plus `G-CORE-FUZZ` and `G-PROTO-FUZZ`). On the first run 3 failed. For `G-PROTO-README-EXAMPLE`, `soup` in the README bundle example was read as a module name, so it was changed to `--entry soup`. For `G-PROTO-REPORT`, the line count of the prototype scripts in the report was out of date, so it was changed to 1,283. For `G-REND-HTML`, it was reading old book-form HTML left from a mutation check, so the HTML was rebuilt with `render-all.mjs`. All passed after fixing, and all 82 passed. Numbers per part:
  - A (table): the 6 `G-VOCAB-*` goals (FCT, FCT-STABLE, LISTS, LISTS-COPY, PRIORWORK, SOURCES) and `G-STD-*` passed
  - B and C (prototype): golden, unit, OOM (109 files), determinism, WASM and Worker, `G-PROTO-COMPILERS` and fuzzing (`fuzz: 778144 execs in 601 s, 0 crashes, 0 leaks, 0 timeouts`) passed
  - D to G (core, API, CLI): 363 golden cases, OOM 329 files / 4398 points, SARIF 193 files, JSON 361 files, CLI 23 cases, and property tests (including 24 cases of `bundle_roundtrip`) passed. Fuzzing ran 600 seconds on each of 6 targets, all `0 crashes, 0 leaks, 0 timeouts` (bundle 20740, expand 30946, kitchen 38534, parse 71124, schedule 36109, typecheck 25422 execs)
  - H (environments): Linux (`G-PLAT-LINUX`) and WASM (`G-PLAT-WASM`; `build/core-wasm` was rebuilt first with `scripts/build-wasm.sh`) passed locally. The iOS, Android and CI goals (`G-PLAT-IOS`, `G-PLAT-ANDROID`, `G-PLAT-CI`, `G-REPO-TOOLS-CI`, `G-PROTO-IOS`) all 5 passed in CI after `643c00c` was pushed with the user's confirmation (`ci` run 40496811079 and `ios-proto` run 40496811209, both successful). With this, all 87 goals passed
  - I and J (samples): 236 checks over 11 samples and 65 schedule checks passed
  - K (renderer): 26 browser-vs-node comparisons, the rendered phrases check (`0 unresolved, 0 identifiers`), and the self-containment and HTML checks passed
  - L (bundle): `G-BUNDLE-STABLE` passed
- Independent audit when the plan was made (2026-10-25, on the version with §13-1 to §13-3): 54 findings, 54 taken (contents at the end of §5.3)
- Commits added to `scripts/red-first-exceptions.txt` and their reasons (fixes to hand-calculation errors in expectations, tests added afterwards in P16-3)
- Independent audit after the plan was rewritten (2026-10-25, on the D to L part of the version with §13-1 to §13-12): 72 findings, 72 taken (the main ones at the end of §5.3)

### 9.2 A (table)

- Allergen kinds (P1-4): check the current list of specified ingredients of the Consumer Affairs Agency and write it here. If that is not possible (if the list cannot be fetched), use the "items with a labeling duty in Japan" of concept §12.3 as provisional, and say so
  - Result (2026-10-26): the Consumer Affairs Agency page "Information on food allergy labeling" (`https://www.caa.go.jp/policies/policy/food_labeling/food_sanitation/allergy/`) says that cashew nuts were added as a specified ingredient (in force from April 1, Reiwa 8). The table in the document "What is allergy labeling" on the same page (`assets/food_labeling_cms204_260401_01.pdf`, sha256 of the fetched PDF `d5fab03732bc309221abb3ca37eb9f67f898c577a818b862a68ea0cbbc346d7b`, not put in the repository) lists 9 specified ingredients: shrimp, cashew nuts, crab, walnuts, wheat, buckwheat, egg, milk, peanuts (as of April, Reiwa 8). The identifiers are `shrimp` `cashew` `crab` `walnut` `wheat` `buckwheat` `egg` `milk` `peanut`, and the §4.7 row was changed to a = 9, total 267. Labeling of cashew nuts has a transition period until March 31, Reiwa 10 (according to news reports and notices from industry groups), but it is included in the vocabulary
- P1-4 and P1-5 (2026-10-26): wrote the 11 files of `spec/vocab/`. `G-VOCAB-SCHEMA` (`schema: 11 files, 0 errors` and the 11 names match), `G-VOCAB-CHECK` (`check: 0 errors, 0 warnings, 0 infos`) and `G-REPO-SPDX` passed. 28 actions and 37 ingredients (as in §4.7). All table values are `provisional` (to be replaced with primary sources in P3-5)
  - Decisions about how the table is written (all (added); the user checks them in §8): ingredients without skin (meat, cabbage, tomato and so on) have `SkinEdible`, so they can be cut with the "`whole` with `SkinEdible`" option of `cut`. `Beef` also has `Hazard`. The relations for finer cuts are `slice → julienne, mince`, `julienne → mince`, `dice → mince`, `roughChop → biteSize, dice`, `biteSize → dice`, `rollCut → biteSize` (including `dice → mince` of item 7 of §4.7.1). The checker treats `pureed` as a shape value finer than any cut. `dash` is `0.1..0.3 g`. The compatibility rules "the same ingredient in the same state in a row", "that person's portion", "dessert" and "using the same frying pan for stir-frying in a row" are expressed with fixed words in `cases[].when`
  - In YAML 1.1, bare keys such as `on` and `yes` become booleans, so the check script always reads mapping keys as strings (implicit type conversion of values stays as S12 says). This is for `leaves[].on` of actions and `on` of `serve` in the sample
  - Mutation check: `G-REPO-SPDX` (S2, Normal) — failed when the SPDX of `spec/vocab/units.yaml` was set to `CC-BY-4.0` (`0 missing, 1 wrong`). Passed after restoring
  - Mutation check: `G-VOCAB-SCHEMA` (S137, Data completeness) — failed when `vessels.yaml` was renamed to `vessel.yaml` (`FMT:unknownFile` and a mismatch in the name list). Passed after restoring
  - Mutation check: `G-VOCAB-SCHEMA` (S7, Normal) — failed when a misspelled key `lefaves` was added to `Onion` in `ingredients.yaml` (`schema: 11 files, 1 errors`). Passed after restoring
  - Mutation check: `py/test_vocabcheck.test_schema_covers_all_files` (S7, Data completeness) — failed when `schema` was made to skip 1 file (`10 != 11`). Passed after restoring
  - Mutation check: `G-VOCAB-CHECK` (S13, Data completeness) — failed when the capability `WorkSurface` of `peel` in `actions.yaml` was changed to `WorkSurfase` (`check: 1 errors`). Passed after restoring
  - Mutation check: `G-VOCAB-CHECK` (S22, Data completeness) — failed when `source` was removed from `washTime` of `Blender` in `capabilities.yaml` (`check: 1 errors`). Passed after restoring
- Food composition table (P3-1): the text and position of the header cell of the refuse rate column, 3 food numbers and values read by hand, whether the weight change rate is in the Excel file (if not, leave it out of the extraction and set the weight change of yields back to provisional), whether the errata are applied, the original file name, sha256, date fetched
  - Result (P3-1, 2026-10-26. Confirmed with the user before fetching): from the MEXT page "Standard Tables of Food Composition in Japan (Eighth Revised Edition), 2023 Supplement" (`https://www.mext.go.jp/a_menu/syokuhinseibun/mext_00001.html`), placed the "Chapter 2 (data)" Excel file `20260327-mxt_kagsei-mext-000029402_02.xlsx` (1,973,402 bytes, Last-Modified 2026-03-27, sha256 `0d5a77077dd6cd91cbc2e6e317b8b218a38728c409eed452f1c10635a0d3099c`) and the errata `20260327-mxt_kagsei-mext-000029402_16.xlsx` (68,693 bytes, sha256 `fb61037c7f66af0db1fb0729977913a629bc17097bc3ff7bf75217f9110acabb`) in `data/mext-fct-2023/source/` (confirmed untracked with `git check-ignore`). Date fetched 2026-10-26
  - Columns read: in the sheet `表全体`, B2 `食　品　番　号`, D3 `食　品　名`, E3 `廃　棄　率`, and the component identifier `REFUSE` in E12 (matched as the string including the full-width spaces). Data starts at row 13
  - 3 read by hand: `06153` (onion, bulb, raw) 6, `07107` (banana, raw) 40, `06061` (cabbage, head leaves, raw) 15
  - The weight change rate is not in this Excel file (the main table of Chapter 2). It was left out of the extraction, and weight changes from heating stay as provisional values
  - The corrections in the errata (March 27, Reiwa 8) have no refuse rate items, and the file of the main table was published with the same date in its name. No effect on the extracted values
- P3-2 and P3-3 (2026-10-26): `tools/fct/extract.py` (`--check` extracts again and compares) and `test_extract.py` (`Ran 4 tests`, `OK`, including 3 cases from the real Excel file). From the 13 references in the table (10 ingredients, 3 chicken parts; for carrot, `06214` without skin is used as the value after peeling), made the 13 rows of `data/mext-fct-2023/refuse.csv`. `G-VOCAB-FCT` (`fct: 13 rows, 0 mismatches; sha256 ok; order ok; refs ok; headers ok`), `G-VOCAB-FCT-STABLE` and `G-REPO-NOTICE` passed. The table check matches food numbers against the data extracted with `--fct` (S59. Fixtures can have their own `refuse.csv`)
  - Mutation check: `G-REPO-NOTICE` (S5, Docs match) — failed when the attribution wording was removed from `NOTICE`. Passed after restoring
  - Mutation check: `G-VOCAB-FCT` (S5, Fixed premise) — failed when one character of the sha256 in the README was changed (`sha256 failed`). Passed after restoring
  - Mutation check: `G-REPO-NOTICE` (S5, Data completeness) — failed when the `- Retrieved:` line was removed from the README. Passed after restoring
  - Mutation check: `G-VOCAB-FCT` (S58, Normal) — failed when the banana value in `refuse.csv` was set to 41 (`1 mismatches`). Passed after restoring
  - Mutation check: `py/test_extract.test_reads_fixture_xlsx` (S58, Boundary) — failed when values in parentheses were made empty. Passed after restoring
  - Mutation check: `py/test_extract.test_matches_real_xlsx` (S58, Fixed premise) — failed when the column read was shifted by one (reading energy in column F) (`'139' != '6'`). Passed after restoring
  - Mutation check: `fx/vocab-ng/fct-unknown-food` (S59, No-effect condition) — failed when the food number matching was removed. Passed after restoring
  - Mutation check: `G-VOCAB-FCT-STABLE` (S60, Side effects) — when output followed the iteration order of a `set`, `refuse.csv` showed a diff all 3 times and it failed. Passed after restoring
  - Mutation check: `G-VOCAB-FCT` (S141, Data completeness) — failed when a row for food number `01001`, which the table does not refer to, was also written (`refs failed`). Passed after restoring
  - Mutation check: `G-VOCAB-FCT` (S61, Fixed premise) — failed when the sha256 in the README was changed (the same break as the S5 Fixed premise, `sha256 failed`). Passed after restoring
- Leaves of the sample `expect`, k (P2-3): 63
- Mapping to prior work (P3-7): number of elements, number of "no match", and the list of candidates to add to Ozen (date reported to the user)
  - Result (P3-7, 2026-10-26. Confirmed with the user before fetching): fetched 4 PDFs into the scratchpad (Nakabe et al. 4,534,771 bytes, FOON 3,881,659 bytes, Kumbhakern et al. 569,752 bytes, Tasse and Smith 119,692 bytes; sha256 recorded in `spec/notes/prior-work-elements.yaml`), and a subagent extracted elements from the per-page text (element descriptions were translated into English and put in the repository). 84 elements (Nakabe et al. 23, Tasse and Smith 21, FOON 20, Kumbhakern et al. 20), 24 "no match". `G-VOCAB-PRIORWORK` printed `prior-work: 84 elements from 4 sources, 0 unmapped, 0 without page`, and `scripts/test-priorwork.sh` printed `6 cases, 0 failed`
  - Candidates to add to Ozen (end of `spec/notes/prior-work.md`; reported to the user on 2026-10-26): held resources (a tool holding a product stays occupied after a task until the next task), preparation time before a task, continuity with no gap after the previous task, tracking which ingredients are in a container, tasks inserted in the middle of a running task (relative times)
  - Learned while extracting: the bottom of page 11 of the Nakabe et al. PDF had text from an older version mixed in that duplicated page 12, so those 2 pages were not used for elements. The title of the Tasse and Smith document is "SOUR CREAM: Toward Semantic Processing of Recipes", and MILK is the name of a section in it
  - Mutation check: `G-VOCAB-PRIORWORK` (S469, Data completeness) — failed when all FOON elements were removed (`64 elements from 3 sources`). Passed after restoring
  - Mutation check: `G-VOCAB-PRIORWORK` (S470, Data completeness) — failed when the `foon-edge` row was removed from the mapping (`1 unmapped`). Passed after restoring
- Primary source research (P3-5): number of values researched, number of values with and without a primary source found, list of documents fetched (date confirmation was given for fetching)
  - Result (P3-5 and P3-6, 2026-10-26): researched 83 numeric properties of the table (excluding references to the food composition table). A primary source was found for 1 (`cooker` time `42..54 min` of `cookRice`: the white rice course in the "cooking time" of the official spec table of the Panasonic SR-PW109. It is an HTML page; no document file was fetched), and not found for 82 (covered by field by the 9 records in `spec/sources/search-log.yaml`: time of manual actions 16, rice cooking in a pot and in a microwave 2, warming naan 3, boiling water 1, time to wash tools and wipe places 19, density 17, density of milk 1, weight per piece 12, heating time 11). Documents fetched: the text of the Ministerial Ordinance on Milk and Milk Products (XML from the e-Gov law search API; placed in the scratchpad, not put in the repository). No PDFs were fetched (except the Consumer Affairs Agency PDF of P1-4)
  - Found but not used as values: Hitachi IH water boiling (1.0 L at maximum heat in about 2 min 19 s, a specific model), and the specific gravity of milk in the Ministerial Ordinance on Milk and Milk Products (`一・〇二八以上`, only a lower bound; the lower end of the provisional range was changed to 1.028)
  - The provenance check `tools/vocabcheck/sources.py`: a primary source entry has `url`, `title`, `retrieved`, `location` and `quote`, and the numbers for the lower and upper bounds of the value must be among the numbers in the transcription (read after NFKC normalization). A search record has `date`, `queries`, `consulted`, `reason` and `covers`, a list of the paths of the properties it covers. `G-VOCAB-SOURCES` printed `sources: 1 primary, 82 provisional, 0 missing fields, 0 quote mismatches, 0 provisional without search, 0 pdf in repo`
  - Mutation check: `G-VOCAB-SOURCES` (S465, Data completeness) — failed when the rice cooker time was set to `42..55 min`, which differs from the transcription (`1 quote mismatches`). With the transcription matching removed, `fx/vocab-ng/primary-quote-mismatch` also failed. Passed after restoring
  - Mutation check: `G-VOCAB-SOURCES` (S466, Data completeness) — failed when `ingredients.Onion.props.density` was removed from the search record (`1 provisional without search`). Passed after restoring
  - Mutation check: `G-VOCAB-SOURCES` (S467, No-effect condition) — in a throwaway copy, failed when a PDF was committed (`1 pdf in repo`). The copy was deleted
- Mutation check records (each task): <test> — failed when broken by <how> (<message>). Passed after restoring
- P1-1 (2026-10-26): `python3 -m unittest tools.vocabcheck.test_qty -v` printed `Ran 6 tests` and `OK`
  - Mutation check: `py/test_qty.test_rejects_malformed` (S8, No-effect condition) — failed when the check for a zero denominator was removed (`ZeroDivisionError: Fraction(1, 0)`, `errors=2`). Passed after restoring
  - Mutation check: `py/test_qty.test_accepts` (S8, Normal) — failed when decimals were read with `Fraction(float(text))` (`Fraction(5404319552844595, 9007199254740992)` did not equal `Fraction(3, 5)`). Passed after restoring
  - Mutation check: `py/test_qty.test_rejects_unitless` (S9, No-effect condition) — failed when an amount with no unit was let through as an amount with unit `""` (`failures=3`). Passed after restoring
  - Mutation check: `py/test_qty.test_range_bounds` (S10, Boundary) — failed when the comparison was changed to `lo >= hi` (`5..5 g` became an error). Passed after restoring
  - Mutation check: `py/test_qty.test_rejects_unknown_unit` (S11, No-effect condition) — failed when the unit check was removed (`5 oz/ml` got through). Passed after restoring
- P1-2 and P1-3 (2026-10-26): wrote 53 fixtures (51 `vocab-ng`, 2 `vocab-ok`) in one commit (`4bb0a1c`) before the check script, and `G-REPO-RED-FIRST` printed `red-first: 1 commits, 0 not red`. `python3 -B -m unittest tools.vocabcheck.test_vocabcheck` printed `Ran 54 tests` and `OK`. `G-VOCAB-EXPECT-INDEPENDENT` printed `separation: 1 commits, 0 mixed`
- Details of the table and fixture formats decided by this plan (P1-2 and P1-3, all (added)):
  - A fixture `fx/vocab-*/<名前>/` has 11 table files (small tables) in `vocab/`, and `expect.yaml`, the multiset of expected diagnostics (each item of `diagnostics:` has `severity`, `name`, `file`, `path`, `args`). The test name is `test_fx_<種類>_<名前>` (`-` becomes `_`)
  - A diagnostic path joins YAML keys with `.` and writes list indexes as `[i]` (`ingredients.Onion.traits[1]`). The file name is the name relative to the table folder (`ingredients.yaml`)
  - The argument of `FMT:schema` is the one JSON Schema keyword that failed (`additionalProperties` gives one diagnostic per extra key, with the path up to that key). The argument of `NAM:undefined`, `NAM:duplicate`, `NAM:naming`, `NAM:partTraitClash` and `NAM:capabilityUnprovided` is the name; for `NAM:provenance` it is the source name (the property name when there is no `source`); for `TYP:unitCycle` and `TYP:cutCycle` it is the cycle traced from the element that appears first in table order (one diagnostic per cycle); for `TYP:unitDimension` it is (declared dimension, dimension of the right side of the definition); for `TYP:baseUnit` it is (dimension, number of base units); for `TYP:axisValue` it is (axis, value); for `SAF:ruleNotFixed` it is the `after` trait
  - `NAM:duplicate` is reported on the definition that appears later in the order of the table files (the order of §4.2). For duplicate keys in the same file, the later key is dropped and the earlier one kept
  - Namespaces: types (dimensions, enums, traits, ingredients, aliases, capabilities, equipment kinds, vessels), actions, axes, values per axis, units, dirt kinds, compatibility rules, vessel alternative orders, sources, and parts per ingredient and trait
  - Among `FMT:schema` errors, `TYP:fctInlineValue` (`value` on a food composition table property) and `NAM:provenance` (missing `source`) are renamed through `x-diag` and `x-diag-required` in the schema; if there is even one other `FMT:schema`, the semantic checks (S13 to S27) are not run
  - In `cases[].when` of a compatibility rule, a name starting with an uppercase letter is a trait, and one starting with a lowercase letter is a fixed word (`allergicPerson`, `dessert`, `sameFoodSameState`, `sameTool`)
  - Kinds of references for S13 (12 kinds of fixtures `undefined-*`): trait, axis, axis value, capability, equipment kind, dirt kind, vessel, unit, dimension, part, enum, ingredient. Undefined sources are handled by `NAM:provenance` of S22, and food numbers of the food composition table by S59
  - Kinds of names for S21 (13 kinds of fixtures `naming-*`): ingredient, trait, equipment kind, vessel, capability, alias, action, axis, axis value, part, dirt kind, argument, compatibility rule. Dimensions and enums (uppercase), and enum values, allergen kinds, capability arguments and vessel alternative orders (lowercase) are checked with the same rules
  - Mutation check: `fx/vocab-ng/schema-unknown-key` (S7, No-effect condition) — failed when `additionalProperties: false` was removed from the ingredient schema (the expected `FMT:schema` did not appear). Passed after restoring
  - Mutation check: `fx/vocab-ng/yaml-implicit-*` (S12, Boundary) — both failed when `type: string` was removed from `qty` in the schema. Passed after restoring
  - Mutation check: `fx/vocab-ng/undefined-*` (S13, No-effect condition) — `undefined-trait` and `undefined-twice` failed when resolving trait references was skipped. Passed after restoring
  - Mutation check: `fx/vocab-ng/undefined-twice` (S13, Boundary) — failed when `NAM:undefined` for the same name was merged into one (the one in `vessels.yaml` was missing). Passed after restoring
  - Mutation check: `fx/vocab-ng/duplicate-across-files` (S14, No-effect condition) — failed when the duplicate check was limited to within one file. Passed after restoring
  - Mutation check: `fx/vocab-ng/duplicate-same-file` (S14, Boundary) — failed when the loading that rejects duplicate keys was removed (the expected `NAM:duplicate` did not appear). Passed after restoring
  - Mutation check: `fx/vocab-ok/same-name-other-namespace` (S14, Normal) — failed when duplicates were counted across namespaces (`NAM:duplicate` on the action `wash`). Passed after restoring
  - Mutation check: `fx/vocab-ng/part-trait-clash` (S15, No-effect condition) — failed when the check was removed. Passed after restoring
  - Mutation check: `fx/vocab-ng/unit-cycle` (S16, Boundary) — the 3-step cycle failed when cycle detection was limited to 2 steps. Passed after restoring
  - Mutation check: `fx/vocab-ng/unit-dimension-mismatch` (S17, No-effect condition) — failed when the dimension comparison was removed. Passed after restoring
  - Mutation check: `fx/vocab-ng/base-unit-twice` (S18, Boundary) — failed when the condition was "1 or more". Passed after restoring
  - Mutation check: `fx/vocab-ng/ordered-axis-duplicate` (S19, No-effect condition) — failed when the duplicate check was removed. Passed after restoring
  - Mutation check: `fx/vocab-ng/cut-refinement-cycle` (S20, No-effect condition) — failed when cycle detection was removed. Passed after restoring
  - Mutation check: `fx/vocab-ng/naming-*` (S21, No-effect condition) — when the convention check was limited to uppercase names, 8 failed: the 7 lowercase kinds and `naming-unit-non-ascii`. Passed after restoring
  - Mutation check: `fx/vocab-ok/unit-uppercase` (S21, Boundary) — failed when units were also required to be lower camel case (`NAM:naming` on `L`). Passed after restoring
  - Mutation check: `fx/vocab-ng/provenance-*` (S22, No-effect condition) — `provenance-unknown-source` failed when matching against source entries was removed, and `provenance-missing` failed when renaming for a missing `source` was removed. Passed after restoring
  - Mutation check: `fx/vocab-ng/provisional-point` (S23, Boundary) — failed when the condition was changed to `lo <= hi` (`TYP:provisionalPoint` also on range values). Passed after restoring
  - Mutation check: `fx/vocab-ng/fct-inline-value` (S24, No-effect condition) — failed when the ban on `value` in the schema was removed (it became `FMT:schema`). Passed after restoring
  - Mutation check: `fx/vocab-ng/capability-without-kind` (S25, No-effect condition) — failed when the check was removed. Passed after restoring
  - Mutation check: `fx/vocab-ng/allergen-rule-not-fixed` (S26, No-effect condition) — failed when `Allergen` was taken out of the targets. Passed after restoring
  - Mutation check: `fx/vocab-ng/effect-value-out-of-axis` (S27, No-effect condition) — failed when the check of axis values in effects was removed. Passed after restoring
  - Mutation check: `fx/vocab-ng/requirement-value-out-of-axis` (S27, Boundary) — failed when only the first option of a requirement's `anyOf` was checked. Passed after restoring
- P1-6 (2026-10-26): copied §4.7 into `spec/coverage/concept-lists.yaml` (267 items; 13 items have values: 6 units, 3 ordered axes, 4 vessel alternative orders) and §4.7.1 into `spec/coverage/concept-relations.yaml` (47 items). `G-VOCAB-LISTS` (`lists: 267 listed, 0 missing; values: 13 checked, 0 mismatched; relations: 47 listed, 0 mismatched`) and `G-VOCAB-LISTS-COPY` (`copy: 257 identifiers, 0 differences`; the set of identifiers in §4.7 is 257 without duplicates) passed. Relations are matched as follows: dirt categories, equipment capabilities and compatibility rules by set equality; ingredient traits by "has" (the table may have more); `HoldsLiquid` of vessels by the same split over all vessels
  - Mutation check: `G-VOCAB-LISTS` (S29, Data completeness) — failed when `tear` was removed from the table (`1 missing`). Passed after restoring
  - Mutation check: `py/test_vocabcheck.test_lists_reports_missing` (S29, No-effect condition) — failed when matching of actions was changed to the difference in counts per kind (removing 1 and adding 1 gave `0 != 1`). Passed after restoring
  - Mutation check: `py/test_vocabcheck.test_lists_owner` (S29, Boundary) — failed when parts were matched by identifier alone without looking at the owner (removing beef `belly` still gave `0 != 1`). Passed after restoring
  - Mutation check: `G-VOCAB-LISTS` (S138, Data completeness) — failed when `tbsp` in `units.yaml` was set to `16 ml` (`1 mismatched`). Passed after restoring
  - Mutation check: `py/test_vocabcheck.test_lists_values` (S138, Time and order) — failed when lists were compared as sets (swapping in `doneness` gave `0 != 1`). Passed after restoring
  - Mutation check: `G-VOCAB-LISTS` (S139, Data completeness) — failed when `Smelly` was removed from `rawFish` in `dirt.yaml` (`relations: 47 listed, 1 mismatched`). Passed after restoring
  - Mutation check: `py/test_vocabcheck.test_relations_mismatch` (S139, No-effect condition) — failed when equipment capabilities were compared only one way, "the table contains the relation" (adding a capability to `Pot` still gave `0 != 1`). Passed after restoring
  - Mutation check: `G-VOCAB-LISTS-COPY` (S140, Docs match) — failed when `julienne` in `concept-lists.yaml` was changed to `julliene` (`2 differences`). Passed after restoring
- Gate A1 (P1-7, 2026-10-26): `G-VOCAB-SCHEMA`, `G-VOCAB-CHECK`, `G-VOCAB-LISTS`, `G-VOCAB-LISTS-COPY`, `G-VOCAB-TESTS`, `G-VOCAB-READONLY`, `G-VOCAB-EXPECT-INDEPENDENT`, `G-REPO-SPDX`, `G-REPO-SELFTESTS` and `G-REPO-RED-FIRST` all passed
  - This session's environment had `PYTHONDONTWRITEBYTECODE=1`, so the check of `G-VOCAB-READONLY` was run with it removed (`env -u PYTHONDONTWRITEBYTECODE`). `G-VOCAB-READONLY` in `scripts/goals.sh` calls `python3` without `-B`, as the §5 command says
  - Mutation check: `G-VOCAB-READONLY` (S28, Side effects) — in a throwaway copy, removed `sys.dont_write_bytecode = True` from `vocabcheck.py`, committed it, and ran with `PYTHONDONTWRITEBYTECODE` removed; it failed (`tools/vocabcheck/__pycache__` was created). Passed after restoring. The copy was deleted
- P2-1 to P2-3 (2026-10-26): committed 49 sample fixtures (27 `sample-ok`, 22 `sample-ng`) before the check script (`G-REPO-RED-FIRST` was red for all 3 commits), and also committed `spec/samples/curryRice.yaml` (expected leaves k = 63) before the `sample` implementation. `python3 -B -m unittest tools.vocabcheck.test_vocabcheck` printed `Ran 108 tests` and `OK`, and `G-VOCAB-SAMPLE` printed `sample: 0 errors; expectations: 63 checked, 0 failed`
- Details decided for the sample check (P2-1 to P2-3, all (added)):
  - How states are written: an ordered axis is the value name, an axis with arguments is `cut(style: slice, size: 3 mm)` (arguments in the order of the axis declaration), and a set axis is `[washed, soaked(time: 30 min)]` (in the order of the axis values). The `state` in `expect` lists every axis the value uses and is compared for exact equality. `traits` is the set of the traits declared on the ingredient, the traits of the part, and added traits (`Hazard`, `Allergen(k)`, traits of a recipe's `output`); the closure over inheritance is not included
  - Diagnostic locations are `recipes.<レシピ>.ingredients.<材料>`, `recipes.<レシピ>.steps[<i>]` (0-based), `recipes.<レシピ>.result`, `menus.<献立>.make.<名前>` and `menus.<献立>.serve.<波>[<i>]`. The order follows this description (recipes → menus, ingredients → steps → result). `LIN:leftover` is reported where the value was made
  - The arguments of the argument error `NAM:arity` are (action, `missing`, `extra`, `unknown` or `duplicate`, the argument name or the extra position). `TYP:stateRequirement` is (action, argument, value name), `TYP:cutCoarser` is (current cut or `pureed`, new cut), `TYP:dimension` is (argument, declared dimension, given dimension), `TYP:enumValue` is (argument, value), `TYP:output` is (type of `output`, type of the result), `TYP:vesselLiquid` is (value, vessel; one per vessel that does not fit), and `ASM:firstVariant` is the list of names of the other candidates
  - Effects are applied and the check continues even for arguments that do not meet the requirements (cutting `pureed` gives 2 diagnostics: `TYP:stateRequirement` and `TYP:cutCoarser`). Effects on axes the value does not use are skipped (the shape effect of `blend` does not apply to `Mixture`). Arguments of unknown actions are treated as consumed. A `let` with the same name reports `NAM:rebind` and overwrites with the new value
  - The amount of the result of an action with an output type takes over the amount of the first ingredient input. The amount of `combine` is the sum of masses (volume converted with `density`, counts with `unitWeight`)
  - Requirements (`requires`) are the union of the `uses` items other than `hands` of the actions used (the first candidate for `variant`), sorted by name (both capabilities and equipment kinds)
  - Added to the table: the parboiling action `parboil` (`doneness: par`; to check S34 and S53), the output type `WhippedCream` of `whip` (it declares no allergen and takes over `Allergen(milk)` from the input; to check S48), and `Hazard` on `Buri` (to check the exception for `RawEdible`; from concept §4.1, "ingredients meant to be eaten raw are made an exception in that ingredient's definition")
  - Mutation check: `fx/sample-ok/initial-state` (S30, Normal) — failed when the initial value of an ordered axis was made the last value. Passed after restoring
  - Mutation check: `fx/sample-ok/initial-override` (S30, Boundary) — failed when `initial` was ignored. Passed after restoring
  - Mutation check: `fx/sample-ng/peel-missing` (S31, No-effect condition) — failed when the requirement check was made always true. Passed after restoring
  - Mutation check: `fx/sample-ok/skin-edible-cut-whole` (S32, Normal) — failed when `anyOf` was judged as "all". Passed after restoring
  - Mutation check: `fx/sample-ng/requirement-on-unused-axis` (S33, No-effect condition) — failed when requirements on unused axes were treated as met. Passed after restoring
  - Mutation check: `fx/sample-ok/ordered-no-regress` (S34, Time and order) — failed when the effect on an ordered axis was assignment instead of maximum. Passed after restoring
  - Mutation check: `fx/sample-ok/cut-finer` (S35, Normal) — failed when the fineness relation was traced backwards. Passed after restoring
  - Mutation check: `fx/sample-ng/cut-coarser`, `fx/sample-ng/cut-same-style` (S35, Boundary) — counted the same cut as "finer" (`cut-same-style` failed). Passed after restoring
  - Mutation check: `fx/sample-ok/prep-add` (S36, Normal) — failed when the set of prep steps was replaced. Passed after restoring
  - Mutation check: `fx/sample-ok/prep-update-args` (S36, Boundary) — failed when old arguments of the same item were kept. Passed after restoring
  - Mutation check: `fx/sample-ok/unmentioned-axes` (S37, Side effects) — failed when axes with no effect were reset to initial values. Passed after restoring
  - Mutation check: `fx/sample-ng/dimension-mismatch` (S38, No-effect condition) — failed when the dimension check was removed. Passed after restoring
  - Mutation check: `fx/sample-ng/enum-value` (S39, No-effect condition) — failed when the enum check was removed. Passed after restoring
  - Mutation check: `fx/sample-ng/undefined-action`, `undefined-type`, `undefined-part` (S40, No-effect condition) — did not report `NAM:undefined` (all 3 failed). Passed after restoring
  - Mutation check: `fx/sample-ng/arity-extra` (S41, Boundary) — ignored extra arguments (`arity-extra` failed). Passed after restoring
  - Mutation check: `fx/sample-ok/default-param` (S41, Normal) — failed when arguments with default values were also made required. Passed after restoring
  - Mutation check: `fx/sample-ng/rebind` (S42, No-effect condition) — failed when bindings were silently overwritten. Passed after restoring
  - Mutation check: `fx/sample-ng/double-use` (S43, No-effect condition) — failed when the consumption record was removed. Passed after restoring
  - Mutation check: `fx/sample-ng/plate-twice` (S43, Real path) — failed when plating was not counted as consumption. Passed after restoring
  - Mutation check: `fx/sample-ok/leftover-info` (S44, Normal) — failed when leftovers were made an error. Passed after restoring
  - Mutation check: `fx/sample-ok/list-sugar` (S45, Normal) — failed when only the head of the list was used. Passed after restoring
  - Mutation check: `fx/sample-ok/combine-doneness-min`, `combine-allergen-union` (S46, Normal) — `combine-doneness-min` failed when minimum was changed to maximum, and `combine-allergen-union` failed when the union of traits was removed. Passed after restoring
  - Mutation check: `fx/sample-ng/mixture-hazard` (S46, Real path) — failed when the union of `Hazard` was removed. Passed after restoring
  - Mutation check: `fx/sample-ok/mixture-axes` (S46, Side effects) — failed when `browning` was carried over to `Mixture`. Passed after restoring
  - Mutation check: `fx/sample-ok/combine-mass-sum` (S47, Normal) — failed when numbers were just added without conversion. Passed after restoring
  - Mutation check: `fx/sample-ok/combine-amount-unknown` (S47, Boundary) — failed when things that cannot be converted were made 0 g. Passed after restoring
  - Mutation check: `fx/sample-ok/output-type-action`, `output-allergen-inherit` (S48, Normal) — kept the input type for an action with an output type (both failed). Passed after restoring
  - Mutation check: `fx/sample-ok/variant-first` (S49, Normal) — failed when requirements were the union of the `uses` of all candidates. Passed after restoring
  - Mutation check: `fx/sample-ok/recipe-retype` (S50, Normal) — failed when `Mixture` was not replaced. Passed after restoring
  - Mutation check: `fx/sample-ng/recipe-output-mismatch` (S51, No-effect condition) — failed when the type comparison was removed. Passed after restoring
  - Mutation check: `fx/sample-ok/menu-make` (S52, Normal) — failed when `make` of a menu was bound to the initial state of the type. Passed after restoring
  - Mutation check: `fx/sample-ng/undercooked-pork` (S53, No-effect condition) — failed when the safety check was removed. Passed after restoring
  - Mutation check: `fx/sample-ng/undercooked-par`, `fx/sample-ok/cooked-pork` (S53, Boundary) — shifted the comparison by one step so `par` passed (`undercooked-par` failed). Passed after restoring
  - Mutation check: `fx/sample-ok/raw-edible-buri` (S53, No-effect condition) — failed when the `RawEdible` exception was removed. Passed after restoring
  - Mutation check: `fx/sample-ng/liquid-flat-plate`, `fx/sample-ok/dry-flat-plate` (S54, No-effect condition) — removed the check (`liquid-flat-plate` failed). Passed after restoring
  - Mutation check: `fx/sample-ng/liquid-alt-flat` (S54, Boundary) — failed when only the first vessel was checked. Passed after restoring
  - Mutation check: `fx/sample-ok/requires-union` (S55, Normal) — failed when only the `uses` of the last action was output. Passed after restoring
  - Mutation check: `py/test_vocabcheck.test_diag_order_and_location` (S56, Time and order) — failed when diagnostics were sorted by name. Passed after restoring
  - Mutation check: `fx/sample-ok/blend-single-pureed` (S471, Normal) — failed when even a single ingredient was made a new value (`Mixture`). Passed after restoring
  - Mutation check: `fx/sample-ok/blend-list-mixture` (S472, Normal) — failed when only the head of the list was used. Passed after restoring
  - Mutation check: `fx/sample-ng/cut-after-pureed` (S473, No-effect condition) — failed when `pureed` was not put in the fineness relation. Passed after restoring
  - Mutation check: `G-REPO-SPDX` (S3, Normal) — failed when the SPDX of the sample was set to `MIT OR Apache-2.0` (`1 wrong`). Passed after restoring
  - Mutation check: `G-VOCAB-SAMPLE` (S57, Normal) — failed when the `peel` step was removed from the sample (`sample: 1 errors; expectations: 63 checked, 9 failed`). Passed after restoring
  - Mutation check: `G-VOCAB-SAMPLE` (S57, Data completeness) — failed when the `doneness` effect of `simmer` in `actions.yaml` was removed (`4 failed`). Passed after restoring
  - Mutation check: `G-VOCAB-SAMPLE` (S57, Time and order) — failed when the `browning` effect of `fry` in `actions.yaml` was removed (`4 failed`). Passed after restoring
- Expectation fix (2026-10-26): in P3-5 the provisional range of milk density was changed from `1.02..1.04` to `1.028..1.040 g/ml`, so the expected amount of `fx/sample-ok/blend-list-mixture` was changed from `304..358 g` to `305.6..358 g` (banana `100..150 g` + milk `200 ml × 1.028..1.040`). This fix comes from a change in table values, not a hand-calculation error, and is not an addition, so it is outside `G-REPO-RED-FIRST`
- Replacement of provisional values (2026-11-01, continuing P3-5; concept §12.3 says "replace as soon as a primary source is found"): researched the 95 provisional values of `std` again by field, and replaced 6 with primary sources (registered in `stdlib/sources.ozen`. All are HTML pages or paper texts, and the transcriptions were compared with the fetched text. No PDFs were fetched)
  - Water density `0.99..1.00` → `0.99565..0.99997 g/ml` (the 5 °C and 30 °C rows of the 1 atm table in the NIST Chemistry WebBook), oil density `0.90..0.93` → `0.91..0.92 g/ml` (Nisshin OilliO Q&A, which gives the specific gravity of vegetable oil as about 0.91 to 0.92), bulk density of rice `0.80..0.86` → `0.77..0.87 g/ml` (Nádvorníková et al. 2018, 8 varieties of polished rice; not Japanese varieties), pork density `1.00..1.10` → `1.0052..1.0622 g/ml` (Molecules 25(7):1736, 75 samples by hydrostatic weighing), ice density `0.91..0.92` → `0.92 g/ml` (freshwater ice at 0 °C in the FAO "Ice in Fisheries"; a single point), dishwasher run time `60..90` → `85..120 min` (washing to drying of 9 models in the Panasonic comparison table)
  - For values not found, the documents consulted and the reasons were added to `spec/sources/search-log.yaml` as 13 records dated 2026-11-01. The weight of an M-size hen's egg was confirmed in a Ministry of Agriculture, Forestry and Fisheries notice (Appended Table 1, "58 grams or more and less than 64 grams"), but the numbers are written in kanji numerals and do not pass the transcription matching, so it stays provisional. Steamed noodles and naan had only a single product's value, and tofu only an industry group's explanation, so they were not used
  - Expectation fixes: fixed 7 golden cases whose amounts and times change with the values (`api/ir-display`, `api/local-not-listed`, `cleanup/dishwasher-during-meal`, `dim/density-convert`, `mix/amount-sum`, `serve/use-within-ir`, `std/prelude-contents`). Amounts were calculated by hand to confirm agreement (for example: egg `58..64 g` + water `200 ml × 0.99565..0.99997` = `257.13..263.994 g`; the dishwasher is `120 s` + `85..120 min`, ending at `5220..7320 s`). These fixes come from value changes and are not additions, so they are outside `G-REPO-RED-FIRST`
  - The 2 rewrites in `scripts/test-stdcheck.sh` (making a provisional value a single point, removing a search record) targeted the rice density, so they were moved to the sugar density, which is still provisional
  - `G-STD-SOURCES` printed `sources: 7 primary, 89 provisional, 0 missing fields, 0 quote mismatches, 0 provisional without search, 0 pdf in repo`
- Replacement using PDF documents (2026-11-01, confirmed with the user before fetching): fetched 9 PDFs from 4 bodies into the scratchpad (the yield survey results table of the Tokyo Metropolitan Central Wholesale Market, 290,216 bytes; the weight guide table of the National Institute of Health and Nutrition, 661,978 bytes; chapter 3 of the MEXT cooking technique manual, 1,541,711 bytes; the Ministry of Health, Labour and Welfare document on milk specific gravity, 220,889 bytes; Yamanashi Prefecture standard shipping specifications for fruit and vegetables for tomato, cucumber, cabbage, lettuce and strawberry, 28,276 to 379,887 bytes). They were not put in the repository; the sha256 of documents used is written in `location` in `stdlib/sources.ozen`, and the sha256 of documents not used in `spec/sources/search-log.yaml`. 6 values were replaced
  - From the measuring cup and spoon table of the National Institute of Health and Nutrition (weight of one 200 ml measuring cup and one 15 ml tablespoon): sugar `0.60..0.85 g/ml` → `130..180 g/cup` (soft white sugar and granulated sugar), Worcestershire sauce `1.15..1.25 g/ml` → `240 g/cup`, fresh cream `1.00..1.03 g/ml` → `210 g/cup`, dressing `0.95..1.10 g/ml` → `15..17 g/tbsp` (non-oil and separated types; the table has no measuring cup value). To use the document's numbers as they are, the densities are written in measuring cup and tablespoon units, and `std.prelude` does `import` of `std.units.jp.{cup}` (200 ml)
  - Milk density `1.028..1.040` → `1.027..1.035 g/ml` (the MHLW document says the specific gravity of normal raw milk is 1.027 to 1.035, average 1.032 at 15 °C. It is a value for raw milk, not milk, and the lower end is below the milk lower bound of 1.028 in the Ministerial Ordinance on Milk and Milk Products), cucumber weight `90..110` → `70..200 g/pcs` (from 2S, "70 g or more", to 2L, "150 to less than 200 g", in the Yamanashi Prefecture 5 kg cardboard box specification)
  - Expectation fixes: 8 golden cases (`api/ir-allergens-choices`, `api/local-not-listed`, `menus/output-properties`, `menus/output-type-explicit`, `mix/explicit-combine`, `mix/mixture-hazard-allergen`, `state/blend-list`, `std/prelude-contents`). Amounts were calculated by hand to confirm agreement (for example: milk `200 ml × 1.027..1.035` = `205.4..207 g`; dressing `30 ml` = 2 tablespoons × `15..17 g` = `30..34 g`; sugar `1 tsp` = `5 ml × 130..180 / 200` = `3.25..4.5 g`). These fixes come from value changes and are not additions, so they are outside `G-REPO-RED-FIRST`
  - Not used: yellowtail yield (only the net of three-piece filleting, with no ratio per part), cabbage (the transcription matching reads the thousands-separator commas in "1,300 to less than 1,700 g" as separate numbers), strawberry (no upper bound for L), large tomatoes and lettuce (graded by count), the rough-chop width "3 to 4 cm" (`nominalCutSize` has one source for 3 cuts, and `@source` cannot be written per field of a record). Polished rice at 160 g (one measuring cup, equivalent to 0.80 g/ml) in the same table falls within the range `0.77..0.87` of the paper already in, so the paper's range, which spans varieties, was kept. Oil at 180 g (equivalent to 0.90 g/ml) is slightly below the `0.91..0.92` the maker gives as specific gravity, but the specific gravity statement was preferred over the weight guide, and it was not replaced
  - The targets of the 2 rewrites in `scripts/test-stdcheck.sh` were moved to the chocolate sauce density, which is still provisional, because sugar now has a primary source too
  - `G-STD-SOURCES` printed `sources: 13 primary, 83 provisional, 0 missing fields, 0 quote mismatches, 0 provisional without search, 0 pdf in repo`
- Count check of `G-VOCAB-TESTS` (2026-10-26): `check-plan-tests.sh` counted `fx/vocab-ng/undefined-twice` twice, once with `undefined-*`, so it was fixed to count per category without duplicate files. 3 self-written tests that were not in the coverage table (`py/test_qty.test_non_string_is_error`, `py/test_extract.test_rejects_wrong_headers`, `py/test_extract.test_writes_sorted_csv_and_checks`) were added to the S8, S58 and S141 rows. `Ran 122 tests` = `fx=106` + `py=16`
- Concept mismatches found during implementation (R2)

### 9.3 B, C (prototype)

- The emsdk version (is it the same as the P0-4 record?)
- The libFuzzer speed (exec/s) and the number of runs in 600 seconds (P5-1)
- Symbols added to the allow list of `check-pure.sh` (ones the compiler generates) and the reasons (P4-8)
- The image of the GitHub Actions macOS runner and the Xcode version (also written in `toolchain.txt`), and the run time (P5-3)
- The headless Chrome version, and how the temporary profile used for the Worker check was handled (P5-2)
- Defects are recorded in `proto/defects.tsv` (date, means of detection, category, summary, fixing commit)
- Record of mutation checks (each task): <test> — failed with <how it was broken> (<message>). Restored and passed
- Expectations corrected because of an error in the hand calculation (the reason and the fixing commit)
  - 2026-10-26: 4 golden files of P4-3 were fixed in commit `de17d51` (the inputs of `syntax/nesting-too-deep` and `syntax/nesting-exact` had one bracket fewer than the design, the position of `macro/body-not-quote` was taken at the `m` of `macro`, and the range of `values/range-mul-range` left out the closing bracket). For `fuel/nodes-short` of P4-6, the test could not detect a break that does not count steps, so it was rewritten as an input that goes over the limit by steps. `values/overflow` and `diag/out-of-memory` were added after the implementation, for S147 (every diagnostic appears in a golden). All of these were written in `scripts/red-first-exceptions.txt`
- P4-1 (2026-10-26): `proto/CMakeLists.txt` (`CMAKE_C_STANDARD 23`, `CMAKE_C_EXTENSIONS OFF`. CMake 3.28 turns C23 for clang 17 into `-std=c2x`, so `CMAKE_C23_STANDARD_COMPILE_OPTION` was set to `-std=c23`), `build-all.sh` (`--only` for one configuration), `check-flags.py`, `check-compilers.sh` and `check-diag-registry.sh` with self-tests (5, 3 and 6 cases, all `0 failed`), `include/ozenproto.h`, the arena (in the ASan configuration, a 16-byte poisoned margin for each allocation. `<sanitizer/asan_interface.h>` is not in the S117 allow list, so the functions were declared by hand), the registry `diag_codes.def` (31 codes registered at once. Codes are in the order they appear in §4.8 to §4.14, per category), and the driver. `G-PROTO-BUILD`, `G-PROTO-FLAGS`, `G-PROTO-COMPILERS`, `G-PROTO-EXPORTS`, `G-PROTO-DIAG-APPEND` and `c/arena_*` (5 tests. `arena_poison_detects` is `WILL_FAIL` only in the ASan configuration) passed in 4 configurations
  - Mutation check: `G-PROTO-EXPORTS` (S116, No-effect condition) — failed when the internal function `diag_code` was exported without a prefix. Restored and passed
  - Mutation check: `G-PROTO-BUILD` (S124, Normal) — adding code that warns on a sign conversion made all 4 configurations fail. Restored and passed
  - Mutation check: `G-PROTO-FLAGS` (S124, Fixed premise) — failed when `CMAKE_C_EXTENSIONS` was set to ON (it became `-std=gnu2x` and all 4 configurations gave `3 missing`). Restored and passed
  - Mutation check: `G-PROTO-COMPILERS` (S131, Real path) — failed when the clang configuration was built with `/usr/bin/cc` (gcc) (`2 mismatched`). Restored and passed
  - Mutation check: `c/arena_poison_detects` (S148, Side effects) — without poisoning, the `WILL_FAIL` test failed (`0% tests passed`). Restored and passed
  - Mutation check: `G-PROTO-DIAG-APPEND` (S113, Time and order) — in a throwaway copy, rewriting the `SYN009` line of the registry and committing made it fail (1 deleted line). The copy was removed
- P4-2 (2026-10-26): our own 128-bit integer (`u128.c`), rationals and ranges (`rational.c`), and unit tests `c/u128_*` (3) and `c/rational_*` (5, including `rational_overflow_bounds`). Passed in 4 configurations, and `G-PROTO-NOINT128` passed
- P4-3 to P4-8 (2026-10-26): the golden expectations (syntax and diagnostics 22; values, control, macros and fuel 85) were committed before the implementation, and then we wrote the lexer and parser (`parse.c`), evaluation and macros (`eval.c`), output (`api.c`), the runner (`run-golden.sh`), the digest (`digest.sh`), allocation failure injection (`ozp_oom_test`), and the purity and global variable checks. The first run passed 102 of 107 cases. Of the other 5, 4 were wrong expectations, and there was one implementation error of each kind (an empty argument in the diagnostic for a non-ASCII identifier, and the binding after `NAM:rebind`)
  - Prototype details this plan decided (all (added)): if there is even one syntax error, nothing is evaluated. The position of diagnostics for `params` and the entry is `0:0-0:0`. After `NAM:rebind`, the name is bound to the new value. Macro expansion adds bindings to the caller's scope (it does not make a new scope). Diagnostic arguments: `SYN:unexpectedToken` takes the token text (a line break is `newline`, the end is `end of input`), `SYN:inputTooLarge` takes (size, limit), `SYN:nestingTooDeep`, `MAC:fuelExhausted`, `MAC:depthExceeded` and `MAC:nodeLimit` take the limit, `NAM:arity` takes (callee, declared count, passed count) or (callee, argument name), `TYP:dimension` and `TYP:unsupportedDimension` take (left dimension, right dimension; no dimension is `Dimensionless`), `TYP:type` takes (required kind, passed kind), `TYP:range` takes the operator or function name, `TYP:count` takes how the value was written, `TYP:annotationTarget` takes the annotation name, and `TYP:amountUnknown` takes how the reference was written. If the call that an annotation attaches to is an error, the annotation is silently dropped
  - Values of bindings and arguments are kept on a reused stack (at first they were allocated in the arena for each scope, and the peak memory of `fuel/steps-default` was 420 MB. After the fix it was 1.5 MB)
  - Symbols added to the allow list of `check-pure.sh`: `bcmp` (clang replaces a `memcmp` that only checks equality with `bcmp`), and `strlen` (gcc at `-O2` and above replaces a loop in the core that counts a length with a call to `strlen`). The core does not call `strlen` itself
  - `c/arena_poison_detects` is also registered in the non-ASan configurations so that the 4 configurations have the same number of tests; there it prints "skipped" and passes (`WILL_FAIL` is only in the ASan configuration)
  - The rational oracle: 20,000 expressions (including values near 2⁶², near `INT64_MAX`, decimals and 0) were run in batches of 2,000. With seed 1, 9,767 normal results, 11,298 `MAC:overflow` and 1,214 `MAC:divisionByZero` were compared with the `fractions.Fraction` calculation, with 0 mismatches
- Gate B1 (P4-9, 2026-10-26): `G-PROTO-BUILD`, `UNIT` (15 tests × 4 configurations), `GOLDEN` (109 files × 4 configurations), `RATIONAL-ORACLE` (seeds 1 to 3), `OOM` (`oom: 109 files, 244 points, 0 failures`), `PURE`, `NOGLOBAL`, `NOFLOAT`, `EXPORTS`, `DETERMINISM`, `DIAG-REGISTRY`, `DIAG-APPEND`, `GOLDEN-INDEPENDENT`, `FLAGS`, `COMPILERS`, `NOINT128`, and `G-REPO-SPDX`, `TOOLS`, `SELFTESTS` and `RED-FIRST` all passed
  - Mutation check: `c/rational_overflow_bounds` (S77, Boundary) — allowed the negative side down to INT64_MIN (`-INT64_MAX - 1` and `INT64_MIN` got through). Restored and passed
  - Mutation check: `G-PROTO-NOINT128` (S149, No-effect condition) — used `unsigned __int128` for the 128-bit multiplication (after committing, `git grep` counted 1 hit). Restored and passed
  - Mutation check: `golden/syntax/utf8-invalid` (S62, No-effect condition) — failed when the UTF-8 check was removed. Restored and passed
  - Mutation check: `golden/syntax/ident-nonascii` (S63, No-effect condition) — failed when the non-ASCII check on identifiers was removed. Restored and passed
  - Mutation check: `golden/syntax/string-utf8-passthrough` (S64, Normal) — failed when non-ASCII bytes in strings were rewritten to `?`. Restored and passed
  - Mutation check: `golden/syntax/recover-multiple` (S65, Normal) — failed when parsing stopped at the first syntax error. Restored and passed
  - Mutation check: `golden/syntax/bad-escape` (S142, No-effect condition) — failed when unknown escapes were passed through as characters. Restored and passed
  - Mutation check: `golden/syntax/newline-in-parens` (S143, Normal) — failed when line breaks inside brackets were also treated as separators. Restored and passed
  - Mutation check: `golden/syntax/reserved-words` (S144, No-effect condition) — failed when `repeat` was removed from the reserved words. Restored and passed
  - Mutation check: `golden/syntax/input-exact-limit` (S66, Boundary) — failed when the size comparison was changed to `>=`. Restored and passed
  - Mutation check: `golden/syntax/nesting-too-deep` (S67, Boundary) — failed when the parser did not count depth. Restored and passed
  - Mutation check: `golden/syntax/deep-unary` (S67, Side effects) — did not count unary recursion toward the depth (in the ASan configuration the stack overflowed and it failed). Restored and passed
  - Mutation check: `golden/syntax/quote-outside-macro` (S68, No-effect condition) — removed the check on where `quote` may appear (`nested-quote` also failed). Restored and passed
  - Mutation check: `golden/syntax/splice-outside-quote` (S69, No-effect condition) — failed when the check on `$x` was removed. Restored and passed
  - Mutation check: `golden/syntax/juxtaposed-sum` (S70, Normal) — failed when the second of two juxtaposed quantities was dropped. Restored and passed
  - Mutation check: `golden/syntax/juxtaposed-mismatch` (S70, No-effect condition) — failed when the dimension check for addition was removed. Restored and passed
  - Mutation check: `golden/syntax/number-int64-max` (S71, Boundary) — failed when the limit comparison was changed to `>=` (off by one). Restored and passed
  - Mutation check: `golden/syntax/quote-outside-macro` (S72, Normal) — accepted `quote` in an expression position (so a syntax tree could be written as a value). `golden/values/kinds` checks the 5 of the 6 value kinds that can be written, and the syntax tree is covered by `quote` not being allowed in an expression; it failed. Restored and passed
  - Mutation check: `golden/values/decimal-exact` (S73, Normal) — failed when decimals were read as a binary approximation with denominator 1024. Restored and passed
  - Mutation check: `golden/values/add-same-dimension` (S74, Normal) — failed when the unit factor was not applied. Restored and passed
  - Mutation check: `golden/values/add-dimension-mismatch` (S74, No-effect condition) — failed when the dimension check was removed. Restored and passed
  - Mutation check: `golden/values/div-same-dimension` (S75, Normal) — failed when the dimension of a division was set to the left side's. Restored and passed
  - Mutation check: `golden/values/mul-dimensioned` (S75, No-effect condition) — failed when multiplying two quantities with dimensions was allowed. Restored and passed
  - Mutation check: `golden/values/mul-dimensionless` (S75, Normal) — failed when multiplying by a dimensionless value made the result dimensionless. Restored and passed
  - Mutation check: `golden/values/range-sub` (S76, Normal) — failed when range subtraction was computed as `[a−c, b−d]`. Restored and passed
  - Mutation check: `golden/values/range-mul-range` (S76, No-effect condition) — failed when the check on multiplying two ranges was removed (computing with one end only). Restored and passed
  - Mutation check: `G-PROTO-RATIONAL-ORACLE` (S77, Boundary) — computed additions with small denominators in 64-bit integers (the oracle reported mismatches). Restored and passed
  - Mutation check: `golden/values/div-zero` (S78, No-effect condition) — failed when the zero check was removed. Restored and passed
  - Mutation check: `golden/values/compare` (S79, Normal) — failed when `<` and `<=` were swapped. Restored and passed
  - Mutation check: `golden/values/compare-mismatch` (S79, No-effect condition) — failed when comparing different dimensions was allowed. Restored and passed
  - Mutation check: `golden/values/short-circuit` (S80, Side effects) — evaluated the right side even when short-circuiting (the fuel count changed). Restored and passed
  - Mutation check: `golden/values/ref-portion` (S81, Normal) — failed when the portion was added without multiplying by its ratio. Restored and passed
  - Mutation check: `golden/values/ref-ratio-unknown` (S81, No-effect condition) — failed when a reference with no amount was treated as 0. Restored and passed
  - Mutation check: `golden/values/builtins` (S82, Normal) — failed when `floor` was changed to truncation toward 0. Restored and passed
  - Mutation check: `golden/values/builtin-errors` (S82, No-effect condition) — failed when the argument count check of built-in functions was removed. Restored and passed
  - Mutation check: `golden/values/unknown-unit` (S83, No-effect condition) — failed when unknown units were treated as dimensionless. Restored and passed
  - Mutation check: `golden/control/order-nested-calls` (S84, Time and order) — failed when arguments were evaluated from the right. Restored and passed
  - Mutation check: `golden/control/let-inner-rebind` (S85, No-effect condition) — failed when overlapping bindings were searched only inside the current block. Restored and passed
  - Mutation check: `golden/control/shadow-global` (S85, Normal) — failed when a `let` hiding a top-level name was also made `NAM:rebind`. Restored and passed
  - Mutation check: `golden/control/if-else-chain` (S86, Normal) — failed when the `else` branch was ignored. Restored and passed
  - Mutation check: `golden/control/if-nonbool` (S86, No-effect condition) — failed when a non-boolean condition was treated as true. Restored and passed
  - Mutation check: `golden/control/repeat-zero` (S87, Boundary) — failed when 0 times evaluated once. Restored and passed
  - Mutation check: `golden/control/repeat-nonint` (S87, No-effect condition) — failed when decimals were truncated and allowed. Restored and passed
  - Mutation check: `golden/control/for-list` (S88, Normal) — failed when the last element was skipped. Restored and passed
  - Mutation check: `golden/control/for-nonlist` (S88, No-effect condition) — failed when a non-list was treated as a one-element list. Restored and passed
  - Mutation check: `golden/control/loop-scope-fresh` (S89, Time and order) — failed when bindings were kept after leaving the block (not making a scope per iteration). Restored and passed
  - Mutation check: `golden/control/named-arg-unknown` (S90, No-effect condition) — did not count named arguments and also removed the name check (`named-arg-duplicate` also failed). Restored and passed
  - Mutation check: `golden/control/annotation-attach` (S91, Normal) — failed when annotations were attached to the last step that appeared. Restored and passed
  - Mutation check: `golden/control/annotation-non-call` (S91, No-effect condition) — attached annotations to the last step that appeared (`annotation-macro-outer` and `annotation-outer-error` also failed). Restored and passed
  - Mutation check: `golden/control/error-poison` (S92, Side effects) — failed when binary operations that use an error also emitted a diagnostic. Restored and passed
  - Mutation check: `golden/fuel/stop-keeps-prior-output` (S93, Time and order) — failed when no steps were output on a stop. Restored and passed
  - Mutation check: `golden/control/entry-select` (S94, Normal) — failed when `entry` was ignored and the first recipe was used. Restored and passed
  - Mutation check: `golden/control/entry-ambiguous` (S94, No-effect condition) — failed when the first one was chosen in an ambiguous case. Restored and passed
  - Mutation check: `golden/control/param-override` (S95, Normal) — failed when overrides were ignored. Restored and passed
  - Mutation check: `golden/control/param-unknown` (S95, No-effect condition) — failed when unknown names were ignored. Restored and passed
  - Mutation check: `golden/fuel/param-steps` (S95, Side effects) — failed when the steps of override expressions were not counted. Restored and passed
  - Mutation check: `golden/macro/args-unevaluated` (S96, Side effects) — failed when arguments were evaluated at the time of the call. Restored and passed
  - Mutation check: `golden/macro/expand-basic` (S96, Normal) — failed when the expansion was not evaluated at the call position (the expanded steps did not appear). Restored and passed
  - Mutation check: `golden/macro/hygiene-tmp` (S97, Normal) — did not mark expansions (`hygiene-two-expansions` also failed). Restored and passed
  - Mutation check: `golden/macro/hygiene-free-name` (S98, No-effect condition) — failed when names of calls were looked up with the caller's mark. Restored and passed
  - Mutation check: `golden/macro/splice-caller-scope` (S99, Normal) — failed when spliced arguments were also given the expansion mark. Restored and passed
  - Mutation check: `golden/macro/bind-caller-name` (S100, Normal) — failed when the name in `let $x` was given the expansion mark. Restored and passed
  - Mutation check: `golden/macro/bind-non-name` (S101, No-effect condition) — failed when the check on splices that are not names was removed. Restored and passed
  - Mutation check: `golden/macro/undefined-in-macro-uncalled` (S102, No-effect condition) — removed the check at definition time (`param-without-dollar` also failed). Restored and passed
  - Mutation check: `golden/macro/macro-arity` (S103, No-effect condition) — failed when extra arguments were ignored. Restored and passed
  - Mutation check: `golden/macro/diag-trail-nested` (S104, Normal) — failed when the expansion history was listed from the outside in. Restored and passed
  - Mutation check: `golden/macro/diag-in-spliced-arg` (S105, Normal) — failed when the position and mark of a spliced node were set to those inside the macro. Restored and passed
  - Mutation check: `golden/macro/recursive-terminating` (S106, Normal) — failed when calls to macros from inside a macro were stopped (recursion was forbidden). Restored and passed
  - Mutation check: `golden/macro/recursive-infinite` (S106, Boundary) — removed the checks on fuel, depth and generated nodes (in the ASan configuration the stack overflowed and it failed). Restored and passed
  - Mutation check: `golden/macro/body-not-quote` (S145, No-effect condition) — failed when the check on the shape of the body was removed. Restored and passed
  - Mutation check: `golden/fuel/steps-exact` (S107, Boundary) — did not count literals (`steps-short` also failed). Restored and passed
  - Mutation check: `golden/fuel/count-each-node` (S107, Data completeness) — failed when the items of `ingredients` were not counted. Restored and passed
  - Mutation check: `golden/fuel/steps-default` (S107, Normal) — failed when the default fuel was set to 999,999. Restored and passed
  - Mutation check: `golden/fuel/depth-exact` (S108, Boundary) — did not count macro expansion toward the depth (`depth-short` also failed). Restored and passed
  - Mutation check: `golden/fuel/nodes-short` (S109, Boundary) — did not count steps as generated nodes (the first `nodes-short` reached the limit with macro copies alone, so it could not detect this; it was changed to an input that goes over the limit by steps). Restored and passed
  - Mutation check: `golden/fuel/steps-exact` (S110, Boundary) — made all 3 limit comparisons stop one early (`depth-exact` and `nodes-exact` also failed). Restored and passed
  - Mutation check: `c/limits_defaults` (S111, Normal) — failed when the default depth was set to 128. Restored and passed
  - Mutation check: `golden/diag/format` (S112, Normal) — failed when columns started at 0. Restored and passed
  - Mutation check: `golden/diag/column-bytes` (S112, Boundary) — failed when columns were counted in characters. Restored and passed
  - Mutation check: `G-PROTO-DIAG-REGISTRY` (S113, Data completeness) — failed when the same name was written twice in the registry (`1 duplicate names`). Restored and passed
  - Mutation check: `G-PROTO-DIAG-REGISTRY` (S146, Data completeness) — removing `SYN:badEscape` from the registry gave `1 unlisted`, and changing a code to `SY001` gave `1 bad codes`; both failed. Restored and passed
  - Mutation check: `G-PROTO-DIAG-REGISTRY` (S147, Data completeness) — moving away `golden/control/annotation-non-call` and `annotation-macro-outer`, which emits the same diagnostic, made `TYP007 annotationTarget` `1 unused` (the plan's break only names the former, but the latter emits the same diagnostic, so both were moved away). Restored and passed
  - Mutation check: `golden/diag/result-line` (S114, Normal) — output a step count one too high (`stats-line` also failed). Restored and passed
  - Mutation check: `golden/values/render-quantities` (S115, Normal) — failed when fractions were not reduced. Restored and passed
  - Mutation check: `golden/values/builtin-units` (S115, Data completeness) — failed when the factor of `tsp` was set to 15. Restored and passed
  - Mutation check: `G-PROTO-PURE` (S117, No-effect condition) — adding one `printf` to the core got its symbol counted, and adding `assert` got `<assert.h>` counted. Restored and passed
  - Mutation check: `G-PROTO-NOGLOBAL` (S118, No-effect condition) — failed when a read-write `static int n;` (`return ++n;`) was added. Restored and passed
  - Mutation check: `G-PROTO-NOFLOAT` (S119, No-effect condition) — failed when a function computing a ratio with `double` was added. Restored and passed
  - Mutation check: `c/alloc_balance` (S120, Side effects) — failed when one allocated block was not freed. Restored and passed
  - Mutation check: `G-PROTO-OOM` (S121, Boundary) — dereferenced NULL after an allocation failure (it failed in the ASan configuration). Restored and passed
  - Mutation check: `G-PROTO-DETERMINISM` (S122, Platform path) — changed the order of candidate names by the address of a local variable (the digest came out in 2 variants). Restored and passed
  - Mutation check: `sh/test-driver.sh` (S123, Real path) — failed when it returned 0 even on `result error`. Restored and passed
  - Mutation check: `G-PROTO-UNIT` (S125, Side effects) — shifted the range check of the arena array by one (reported in the golden of the ASan configuration). Restored and passed
- P5-1 (2026-10-26): `proto/fuzz/fuzz_expand.c`, the dictionary `ozen.dict`, and `fuzz.sh`. Ran for 600 seconds with clang 17's libFuzzer: `fuzz: 653862 execs in 601 s, 0 crashes, 0 leaks, 0 timeouts` (1,087 exec/s, coverage 1,177). Until the library was rebuilt with `-fsanitize=fuzzer-no-link`, the coverage stopped at 3. The corpus of 1,170 inputs was reduced with `-merge=1` to 300 (smallest first), committed to `proto/tests/corpus/`, and included in the digest input. The digest matched across the 4 configurations × 2 runs and WASM (`9a6479fb…`)
  - Mutation check: `G-PROTO-FUZZ` (S126, Boundary) — removing the check on the end of a string crashed on the first input (`fuzz: 0 execs in 0 s, 1 crashes`). Restored and passed
- P5-2 (2026-10-26): the Emscripten version is 6.0.13, the same as P0-4. We built one for node (`NODERAWFS`, `build/wasm/`) and one for the browser (`-sENVIRONMENT=web,worker`, `MODULARIZE`, `build/wasm-web/`), with an 8 MiB stack (`-sSTACK_SIZE=8388608`. With the default 64 KiB, the golden with deep nesting overflows). `G-PROTO-WASM` (109 golden files, 2,000 oracle cases, matching digest), `G-PROTO-WASM-BRIDGE` (`bridge: 4 inputs, 0 mismatches`), `G-PROTO-WASM-WORKER` (`worker: 4 inputs, 0 mismatches`) and `test-worker-test: 3 cases, 0 failed` passed
  - Headless Chrome was 154.0.8037.97. `--dump-dom` outputs the DOM without waiting for the Worker's result, so, unlike the note in §5, the page POSTs the result to a check server on `127.0.0.1`, and `worker-test.sh` reads it (served by `ThreadingHTTPServer`). The profile is placed in a temporary folder from `mktemp -d`; Chrome is started with `setsid`, and at the end the whole process group is stopped before the folder is removed (when child processes remained, the folder could not be removed)
  - Mutation check: `G-PROTO-WASM` (S127, Platform path) — changing the branch for how quantities are written only in the WASM configuration (changing the dimension check only under `__EMSCRIPTEN__`) gave `golden: 76 passed, 33 failed`, and the digest did not match either. Restored and passed
  - Mutation check: `G-PROTO-WASM` (S127, Boundary) — putting the partial products of the 128-bit multiplication in `unsigned long` (32 bits on wasm32) made the WASM oracle fail with `2000 cases, 2000 mismatches` (native is 64 bits, so 0 mismatches). Restored and passed
  - Mutation check: `G-PROTO-WASM-BRIDGE` (S64, Platform path) — passing the UTF-16 length (`text.length`) in the bridge made inputs with Japanese and emoji fail with `bridge: 4 inputs, 4 mismatches`. Restored and passed
  - Mutation check: `G-PROTO-WASM-BRIDGE` (S128, Platform path) — passing `str.length` as the length in the bridge failed with `bridge: 4 inputs, 4 mismatches` (the same break as S64). Restored and passed
  - Mutation check: `G-PROTO-WASM-WORKER` (S150, Platform path) — passing `str.length` as the length inside the Worker failed with `worker: 4 inputs, 4 mismatches`. Restored and passed
- P5-3 (2026-10-26): `.github/workflows/ios-proto.yml`. The runner is `macos-26` (image `macos26 20261013.0424.1` and `20261021.0445.8`), Xcode 26.6 (`xcode 26.6 macos` in `toolchain.txt`), and one run takes about 9 minutes. The static libraries for device and simulator were combined into `OzenProto.xcframework`; on the arm64 simulator (iPhone 17 Pro) all 109 golden files passed, and the digest matched native and WASM (`9a6479fb…`). Fixes made in the first 3 runs: CMake makes iOS executables `.app` by default, so `CMAKE_MACOSX_BUNDLE=OFF` was set (`simctl spawn` gave `exit 111`); boot completion is awaited with `simctl bootstatus -b`; and `simctl spawn` can take more than 10 seconds, so `OZP_RUN_TIMEOUT` (the limit for one run of golden and digest) was added and set to 120 seconds on iOS. The simulator's golden summary is also put in the artifact `ios-digest`, and `G-PROTO-IOS` copies it to `build/results/golden-ios.txt`. `G-PROTO-IOS` passed at the last pushed commit `ae60feb`
  - Mutation check: `G-PROTO-IOS` (S129, Platform path) — on the throwaway branch `mut/s129`, dropping the space before a quantity's unit only under `__APPLE_EMBEDDED_SIMULATOR__` made 33 simulator golden files fail, and the run failed. The branch was deleted after confirming
  - Mutation check: `G-PROTO-IOS` (S130, Normal) — on the throwaway branch `mut/s130`, building the xcframework without the device static library let the run succeed, but the xcframework contained only `ios-arm64-simulator/`, and it fails with `xcframework slices: got 1, want 2`. The branch was deleted after confirming
  - Mutation check: `G-PROTO-IOS` (S131, Platform path) — on the throwaway branch `mut/s131`, removing the Xcode version setting from the workflow was not detected, because the runner's default Xcode is 26.6. A variant choosing an Xcode other than the pinned version picked the alias `Xcode_26.6.0.app` and was not detected either. A variant choosing while excluding aliases picked Xcode 26.5, and the workflow's comparison failed with `Xcode is 'Xcode 26.5', toolchain.txt pins 26.6` (the comparison works. Just removing the setting only fails once the runner's default version changes). The branch was deleted after confirming
- Gate B2 (P5-4, 2026-10-26): the goals of P4-9 (from `G-PROTO-BUILD` to `G-REPO-RED-FIRST`), `G-PROTO-FUZZ` (`fuzz: 746382 execs in 601 s, 0 crashes, 0 leaks, 0 timeouts`, coverage 1,184), `G-PROTO-WASM`, `WASM-BRIDGE`, `WASM-WORKER`, and `G-PROTO-IOS` (`ae60feb`) all passed
- P6-1 (2026-10-26): added a prototype section to the README (examples of build, run and diagnostics), and `check-readme-example.sh` runs the README example and compares. `readme: 1 examples, 0 mismatches`
  - Mutation check: `G-PROTO-README-EXAMPLE` (S136, Docs match) — failed when one line of the README example's output was changed. Restored and passed
- P6-2 (2026-10-26): the report `docs/prototype-report.md` (the 4 sections of §4.16), `check-report.py` (headings, the line count table, the per-environment result table, the fuzz line, the number of defects per means of detection, whether the commits in `defects.tsv` are ancestors, and whether every commit whose subject starts with `fix` and that changed `proto/src/` is in `defects.tsv`) and its self-test (`test-check-report: 10 cases, 0 failed`). `G-PROTO-REPORT` also checks that the number of checks is at least the number of result files + 4. `plan-start..HEAD` has no commit whose subject starts with `fix` (the subjects of the fixing commits were written as "Avoid …", "Reuse …" and so on), so the last check was confirmed with the self-test `fix-row-removed`
  - Mutation check: `G-PROTO-REPORT` (S132, Docs match) — removing the heading "3. Fuzzing" made the heading check and the fuzz check fail (`2 failed`). Restored and passed
  - Mutation check: `G-PROTO-REPORT` (S133, Docs match) — failed when the driver in the line count table was set to 176 (`wc -l gives 175`). Restored and passed
  - Mutation check: `G-PROTO-REPORT` (S134, Docs match) — failed when one character of the `native` digest was changed. Restored and passed
  - Mutation check: `G-PROTO-REPORT` (S135, Docs match) — failed when the number of defects found by golden was set to 4. Restored and passed
  - Mutation check: `G-PROTO-REPORT` (S135, Data completeness) — removing the last line of `defects.tsv` made the defect count check fail (the `goal` line was gone and the total was 5). Restored and passed
- P6-3 (2026-10-26): `G-PROTO-VERSION` passed
- Gate C (P6-4, 2026-10-26): the goals of gate B2 and `G-PROTO-README-EXAMPLE`, `G-PROTO-REPORT` and `G-PROTO-VERSION` all passed. The judgment was made after fixing the report's line count table (in P7-1 the self-tests in `proto/scripts/` grew by 8 lines) and the fuzz numbers (matched to the gate's run)

### 9.4 D (core foundation)

- The number of rules in `core/grammar.ebnf` (P7-0)
- Parts brought over from the prototype, and the tests moved with them (P7-0, P7-2)
- The result of converting the tables to `stdlib/` (number of files, items that could not be converted and why), and the retired tests written in `scripts/retired-tests.txt` (P7-4)
- Records of mutation checks and of defects (in `core/defects.tsv`, in the same form)
- P7-0 (2026-10-26): `core/grammar.ebnf` has 130 rules. §4.20.1 records the details decided by the grammar, the golden format, and the 42 diagnostic names of D (`SYN` 17, `NAM` 10, `PKG` 11, `MAC` 4), which were placed in `core/tests/diag-names.txt` in the same order. No new S was added (every detail is checked by tests of existing S items)
- Parts brought over from the prototype, and the tests moved with them (from the list in report §4, the ones D brings over. When bringing them over, the files in `proto/` are not changed; they are copied to `core/` and the prefix is changed from `ozpi_` to `ozni_`)
  - P7-1: the X macro of the diagnostic registry (adding a default severity column to the `diag_codes.def` form) and `check-diag-registry.sh` (10 categories, `--registry`, `--names`, `--outputs`). `check-flags.py` (`--std-gcc`, `--std-clang`, `--extra`), `check-compilers.sh`, how `build-all.sh` makes configurations, and `run-golden.sh` and `digest.sh` (including `OZP_RUN_TIMEOUT`) are adapted for the core in `scripts/`. Their self-tests are moved too
  - P7-2: the arena (`arena.c`, the ASan poisoned margin) and `c/arena_*` move to `cunit/arena_*`; the 128-bit integer and rationals and ranges (`u128.c`, `rational.c`) and `c/u128_*` and `c/rational_*` move to `cunit/`; the UTF-8 check (`ozpi_check_utf8`) moves to `core/src/utf8.c`. The arena is changed, as S152 says, so that the array type is kept inside `arena.c`
  - P7-3: the inputs of the golden files `syntax/utf8-invalid`, `syntax/ident-nonascii`, `syntax/string-utf8-passthrough` and `syntax/recover-multiple` are copied to `cgolden/syntax/`, and the expectations are rewritten in the §4.20.1 format
  - P7-5: allocation failure injection (`ozp_oom_test`), the purity, global variable and floating point checks (`check-pure.sh`, `check-noglobal.py`), and the fuzz entry and dictionary (`fuzz_expand.c`, `ozen.dict`) are moved and adapted for `parse`
  - Not brought over: the prototype's parser, evaluator and output (`parse.c`, `eval.c`, `api.c`). The "points to fix in the evaluator's approach" from report §4 (the binding stack, handling of allocation failure, the output buffer, name interning) go into the production design
- P7-1 (2026-10-26): `core/CMakeLists.txt` (8 flags with `-Wswitch-enum` added, the library `libozen.a`, and the CLI built from `cli/` into `build/<構成>/ozen`. The `ozni_` rule is checked with `nm` by the ctest `symbols_prefixed`), `scripts/build-core.sh`, `scripts/check-c23-subset.sh` (12 probes in `scripts/c23-probes/`, plus a grep for banned features and functions added in C23), the registry `core/src/diag_codes.def` (42 codes, with the default severity column), the CLI skeleton (`ozen check FILE`, `ozen debug ast [--no-spans] [--rules] FILE`; their contents come from P7-3), `scripts/run-golden.sh` and `scripts/digest.sh` (the list of entries is `scripts/golden-entries.sh`. A folder golden is a folder that has `expected.out`, its arguments are the folder's `args`, and `--std-version` changes the value of `{{STD_VERSION}}`), and `scripts/check-grammar-coverage.py`. `check-diag-registry.sh` now has 10 categories and reads the 4th column (severity) of `OZNI_DIAG`. `check-spdx.sh` also looks at `core/`, `cli/`, `tests/` and `.ebnf`. Each self-test gives `0 failed`. `G-CORE-BUILD`, `G-CORE-FLAGS` (all 4 give `flags: 3 files, 0 missing`), `G-CORE-C23-SUBSET` (`12 allowed probes ok on 2 toolchains, 0 banned uses`), `G-CORE-DIAG-APPEND` and `G-REPO-SELFTESTS` passed
  - Difference from the plan: `G-CORE-DIAG-REGISTRY` is a completion condition of P7-1, but P7-0 wrote all 42 diagnostic names of D in `diag-names.txt`, so the registry also got 42 codes. `unused` counts diagnostics that appear in golden expectations, so it does not reach 0 until P7-3 to P7-5 write the golden files (at P7-1 it was `42 unused`, the others 0). Gate D (P7-6) confirms 0
  - Mutation check: `G-CORE-C23-SUBSET` (S151, No-effect condition) — adding `#include <stdckdint.h>` to `core/src/check.c` failed with `1 banned uses`. Restored and passed
  - Mutation check: `G-CORE-DIAG-APPEND` (S266, Time and order) — in a throwaway copy, rewriting the name on the `SYN004` line of the registry and committing made it fail (1 deleted line). The copy was removed
  - Mutation check: `G-CORE-DIAG-REGISTRY` (S268, Data completeness) — registering a diagnostic in category `XYZ` failed with `1 bad codes`. Restored and passed
- P7-2 (2026-10-26): the arena (`arena.c`. In the ASan configuration, a poisoned margin for each allocation. An allocation failure is only counted; later requests are left to the allocation function), the index-addressed table (`ozni_table`. The type `struct ozni_arena_table` is defined only inside `arena.c` and is an incomplete type elsewhere. It adds blocks that double in size, starting from 64 elements, so element addresses do not move. Out of range gives `nullptr`), interning (`intern.c`. FNV-1a with open addressing. IDs start at 1), the UTF-8 check (`utf8.c`. The error position is returned as a byte position), spans (file number and start and end byte positions) and the diagnostic array (`ctx.c`. Arguments are interned strings). `u128.c`, `rational.c` and `num.h` were moved from the prototype with only the prefix changed, and 8 unit tests were moved too. The 18 `cunit/*` tests (and `symbols_prefixed`) passed in 4 configurations, and `G-CORE-ARENA-ACCESS`, `G-CORE-NOINT128` and `G-CORE-UNIT` passed. `symbols_prefixed` was changed to exclude `___asan_globals_registered`, which clang's ASan adds
  - Mutation check: `G-CORE-ARENA-ACCESS` (S152, No-effect condition) — failed when the definition of `struct ozni_arena_table` was copied into `ctx.c` (`arena structs outside arena.c: got '2'`). Restored and passed
  - Mutation check: `cunit/intern_same_id` (S153, Normal) — failed when a new ID was given every time, even for the same spelling. Restored and passed
- P7-3 (2026-10-26): 53 golden expectations (declarations 27, type arguments 7, syntax 18, diagnostics 1) were committed before the implementation, and then we wrote the lexer (`lex.c`. The reserved words are only the statement and expression keywords; declaration words are recognized by spelling in item position. Line breaks inside `(` and `[` are not separators), a recursive descent parser for the whole syntax (`parse.c`. On an error it skips to the next separator, per statement, item, or body item. Depth is counted for each bracket, block, type argument, unary `-`, `not` and `else if`), and the AST dump and diagnostic output (`render.c`. Output is written to `buf.c`, a buffer that remembers allocation failures). In the first run all 51 passed; there was no golden that used the `paren` rule, so `syntax/paren-newline` and `syntax/spans` were added. `G-CORE-GOLDEN` (53 files × 4 configurations), `G-CORE-GRAMMAR` (`grammar: 130 rules, 0 not covered`), `G-CORE-UNIT` (20 tests including `cunit/span_nesting`) and `prop/reindent` (`reindent: 152 cases, 0 differences`) passed
  - Grammar details added: a `param` default can also be a quantity (`t: 3 mm`). `golden_entries` in `goals.sh` now counts with `golden-entries.sh`, the same as the golden runner (the first version did not count `tests/golden/<分類>/<名前>.ozen`)
  - Mutation check: `cunit/span_nesting` (S154, Data completeness) — failed when the span of a binary operation ended at the end of the left child. Restored and passed
  - Mutation check: `cgolden/syntax/recover-multiple` (S156, Normal) — failed when everything after the first error was skipped to the end of input (`errors=2`). Restored and passed
  - Mutation check: `G-CORE-GRAMMAR` (S157, Data completeness) — adding a rule `extra` to the grammar without adding a golden failed with `grammar: 131 rules, 1 not covered`. Restored and passed
  - Mutation check: `cgolden/decl/rule` (S158, Normal) — failed when `rule` declarations were skipped. Restored and passed
  - Mutation check: `cgolden/typeargs/state-has` (S159, Normal) — failed when `has` was read as `:`. Restored and passed
  - Mutation check: `G-CORE-PROPS` (S160, Normal) — making a tab at the start of a line a statement separator failed with `reindent: 152 cases, 2 differences`. Restored and passed
  - Mutation check: `cgolden/diag/code-and-name` (S260, Normal) — failed when the name was removed from the diagnostic line. Restored and passed
  - Mutation check: `cgolden/syntax/utf8-invalid` (S410, No-effect condition) — failed when the UTF-8 check was removed. Restored and passed
  - Mutation check: `cgolden/syntax/ident-nonascii` (S411, No-effect condition) — failed when the non-ASCII check on identifiers was removed (allowing UTF-8). Restored and passed
  - Mutation check: `cgolden/syntax/multiline-string` (S412, Normal) — failed when multi-line strings were ended at a line break. Restored and passed
- P7-4 (2026-10-26): 32 golden files (names 4, modules 17, packages 10, other 1) were committed before the implementation (the 4 single-file cases with no errors pass with parsing alone, so they went in a separate commit and were written as red-first exceptions). `VERSION` was set to `0.1.0`, and the conversion from the tables to `stdlib/` (`tools/std-from-vocab/convert.py`; 730 lines of `stdlib/package.ozen`, `prelude.ozen`, `units/jp.ozen`, `units/us.ozen` and `sources.ozen`) was run once. Running it again without changing `spec/vocab/` gives no diff. At build time `core/gen/embed-std.cmake` turns `std` into a C array and embeds it, and `ozen debug embedded-std` matches `scripts/concat-std.sh stdlib`. Resolution (`resolve.c`) handles packages (`package.ozen`, `deps/<名前>/package.ozen`, the embedded `std`), modules (package name + folder path; inline `module`), definitions, the 3 forms of `import`, override information, cycles, `import` of a menu from a recipe, clashes between top-level folders and dependencies, `requires` versions, and names in bodies (local binding → definition in the file → `import` → `std.prelude` → built-in function → enum and axis values). Names inside `std` are resolved by the same rules, with 0 diagnostics. We wrote `ozen debug definitions` (JSON of all definitions, S462) and `tools/stdcheck/` (`lists.py`, `static.py`, `provenance.py`, `sources.py`; self-test `test-stdcheck: 12 cases, 0 failed`), and wrote the 81 type check tests of the tables in `scripts/retired-tests.txt`. `G-CORE-GOLDEN` (85 files × 4 configurations), `G-CORE-STD-EMBED`, `G-STD-LISTS` (`lists: 267 listed, 0 missing; values: 13 checked, 0 mismatched; relations: 47 listed, 0 mismatched`), `G-STD-STATIC` (211 definitions, all 0), `G-STD-PROVENANCE` (96 values), `G-STD-SOURCES` (1 primary source, 82 provisional, the same as with the tables) and `G-REPO-VERSION` passed
  - Details this plan decided (all (added)): diagnostic positions are the name for `rebind` and `undefined` (when an ingredient's type is omitted), the whole declaration for `duplicateDefinition` and `localOverride`, the relevant item or declaration in `package.ozen` for package diagnostics, and the start of the file (`1:1-1:1`) for entry diagnostics. A type variable is an undefined one-letter type name inside an action declaration. Hiding a `std.prelude` name with a definition of the same name emits `localOverride` (information). If there is even one syntax error, nothing is resolved. If no FILE is given, `ozen check` and `ozen debug modules` read everything from the current folder's `package.ozen` down as one project (with no `package.ozen`, it is a usage error). For development we added `ozen debug modules` (module names and paths) and `ozen debug entry [--entry 名前]` (the chosen entry)
  - Grammar fixes: `rule` now has the form `rule [名前] after 分類: 洗い方 [before 対象], …` (because the table comparison `ruleLevel` looks up compatibility rules by name and has cases. The golden `decl/rule` was fixed first). `plate` became a word recognized by spelling only inside `serve` (because `std` defines an action `plate`). There was 1 error in a hand-calculated expectation (the end column of `modules/top-folder-clash`), fixed before the implementation
  - Mutation check: `cgolden/names/case-insensitive-free` (S161, No-effect condition) — failed when type names starting with a lowercase letter were treated as not found. Restored and passed
  - Mutation check: `cgolden/modules/name-from-path` (S162, Normal) — failed when the package name was not added to the module name. Restored and passed
  - Mutation check: `cgolden/modules/top-folder-clash` (S163, No-effect condition) — failed when the check for name clashes between top-level folders and dependencies was removed. Restored and passed
  - Mutation check: `cgolden/modules/import-forms` (S164, Normal) — failed when `as` was ignored. Restored and passed
  - Mutation check: `cgolden/modules/import-star` (S164, No-effect condition) — failed when `*` was accepted with no diagnostic (it changed to `unknownModule`). Restored and passed
  - Mutation check: `cgolden/modules/prelude-implicit` (S165, Normal) — failed when `std.prelude` was not searched. Restored and passed
  - Mutation check: `cgolden/modules/ambiguous-unused-ok` (S166, Boundary) — failed when the ambiguity error was raised at the second `import`. Restored and passed
  - Mutation check: `cgolden/modules/local-overrides-import` (S167, Normal) — failed when override information was not emitted. Restored and passed
  - Mutation check: `cgolden/modules/import-local` (S168, No-effect condition) — failed when `local` was ignored. Restored and passed
  - Mutation check: `cgolden/modules/cycle` (S169, No-effect condition) — failed when cycle detection was removed. Restored and passed
  - Mutation check: `cgolden/modules/recipe-names-kitchen` (S170, No-effect condition) — failed when references to a kitchen were allowed. Restored and passed
  - Mutation check: `cgolden/modules/forward-reference` (S171, Normal) — failed when definitions further down were treated as not found (forward references inside `std` also became `undefined`). Restored and passed
  - Mutation check: `cgolden/modules/entry-recipe-only` (S172, Normal) — failed when there was said to be no entry when there are only recipes. Restored and passed
  - Mutation check: `cgolden/packages/unknown-field` (S174, No-effect condition) — failed when unknown items were ignored. Restored and passed
  - Mutation check: `cgolden/packages/std-reserved` (S175, No-effect condition) — failed when the `std` check applied only to packages (not looking at module names). Restored and passed
  - Mutation check: `cgolden/packages/requires-newer-minor` (S176, Boundary) — failed when only the major version was compared and not the minor version. Restored and passed
  - Mutation check: `cgolden/packages/two-versions` (S177, No-effect condition) — failed when the check was removed. Restored and passed
  - Mutation check: `G-CORE-STD-EMBED` (S179, Fixed premise) — failed when `stdlib/units/jp.ozen` was changed without regenerating. Restored and passed
  - Mutation check: `cgolden/names/let-rebind` (S413, No-effect condition) — failed when the binding was overwritten. Restored and passed
  - Mutation check: `cgolden/names/ingredient-type-omitted` (S416, Normal) — failed when the type of an ingredient with an omitted type was not looked up (made `Unknown`). Restored and passed
  - Mutation check: `cgolden/packages/requires-range-syntax` (S426, No-effect condition) — failed when `>=` was not treated as range notation (it changed to `badVersion`). Restored and passed
  - Mutation check: `cgolden/packages/copied-dep-version` (S427, Normal) — failed when the version of a copied dependency was not checked (only `std` was compared). Restored and passed
  - Mutation check: `G-STD-STATIC` (S458, No-effect condition) — changing the `std` action `wash` to `Wash` failed with `1 naming`. `static-naming` in `sh/test-stdcheck.sh` also checks the same break. Restored and passed
  - Mutation check: `G-STD-STATIC` (S459, Boundary) — making the provisional density of rice a single point `0.83 g/ml` failed with `1 provisional-point`. Restored and passed
  - Mutation check: `G-STD-STATIC` (S460, No-effect condition) — writing the waste ratio of a chicken cut as the number `10..20 %` failed with `1 fct-inline`. Restored and passed
  - Mutation check: `G-STD-STATIC` (S461, No-effect condition) — removing `Blending` from `Blender` failed with `1 capability-unprovided`. Restored and passed
  - Mutation check: `G-STD-LISTS` (S462, Data completeness) — dropping the vessel `Platter` from the definitions failed with `1 missing` and a mismatch in the relation `vesselTrait`. Restored and passed
  - Mutation check: `G-STD-SOURCES` (S468, Data completeness) — removing the transcription (`quote`) from a primary source in `stdlib/sources.ozen` failed with `1 missing fields, 1 quote mismatches`. Changing the rice cooker time to `42..55 min`, which differs from the transcription, also failed with `1 quote mismatches`. Restored and passed
- P7-5 (2026-10-26): 8 limit golden files were committed first, and `--max-input-bytes`, `--max-depth`, `--max-nodes`, `--max-steps` and `--fail-alloc-at` were added to the CLI (all (added)). How D counts limits (added): input size per user file; depth as syntactic nesting (P7-3's counting); generated nodes as the total AST nodes of the user's files; steps as the number of name lookups in the user's modules (E adds evaluation steps). The embedded `std` is outside the limits. When a limit is reached, a diagnostic is emitted, the remaining files are not read, and nothing is resolved. A chain like `a + b + c` makes the AST deeper without increasing syntactic depth, so a depth limit was put on resolution recursion (`MAC:depthExceeded`, `diag/depth-exceeded`) and a recursion limit on the AST dump. Allocation failure injection is `ozen_oom_test` (`oom: 76 files, 913 points, 0 failures`). `_FORTIFY_SOURCE` replaces `memcpy` into local arrays with `__memcpy_chk`, so copies into local arrays and `strcmp` and the like were made into our own functions in `str.c` (the purity allow list is the same as the prototype's). The fuzz target `parse` uses the `debug ast` path (parsing and resolving `std` each time only runs about 90 times per second). `G-CORE-OOM`, `G-CORE-PURE`, `G-CORE-NOGLOBAL`, `G-CORE-NOFLOAT`, `G-CORE-DETERMINISM` and `G-CORE-FUZZ` (`fuzz-parse: 87077 execs in 601 s, 0 crashes, 0 leaks, 0 timeouts`, coverage 1,293) passed. `diag/out-of-memory` was added after the implementation because the allocation number is decided by the implementation, and it was written as a red-first exception
  - Mutation check: `cgolden/limits/*` (S180, Boundary) — changing the generated node comparison to `>=` made `nodes-exact` and `nodes-over` fail (`6 passed, 2 failed`). Restored and passed
  - Mutation check: `G-CORE-PURE` (S181, No-effect condition) — adding `fopen` to the core failed with `2 disallowed symbols, 1 disallowed includes`. Restored and passed
  - Mutation check: `G-CORE-NOGLOBAL` (S182, No-effect condition) — adding a read-write `static int` failed with `2 writable sections`. Restored and passed
  - Mutation check: `G-CORE-NOFLOAT` (S183, No-effect condition) — adding a function computing a ratio with `double` failed both in the `-mgeneral-regs-only` compile and in the token count. Restored and passed
  - Mutation check: `G-CORE-OOM` (S184, Boundary) — copying in interning without checking for allocation failure was stopped by UBSan with `null pointer passed as argument 1`. Restored and passed
  - Mutation check: `G-CORE-DETERMINISM` (S185, Platform path) — ordering `debug modules` by a value made from a stack address made the digest come out in 2 variants. Restored and passed
- Gate D (P7-6, 2026-10-26): the goals of P7-1 to P7-5 (`G-CORE-BUILD`, `FLAGS`, `C23-SUBSET`, `DIAG-REGISTRY` (`42 codes … 0 unused`), `DIAG-APPEND`, `UNIT`, `ARENA-ACCESS`, `NOINT128`, `GOLDEN` (95 files × 4 configurations), `GRAMMAR` (131 rules), `STD-EMBED`, `G-STD-LISTS`, `STATIC`, `SOURCES`, `G-REPO-VERSION`, `G-CORE-OOM`, `PURE`, `NOGLOBAL`, `NOFLOAT`, `DETERMINISM`, `FUZZ` (`86279 execs in 601 s`)), `prop/reindent` (`416 cases, 0 differences`), and `G-REPO-SPDX`, `SELFTESTS`, `RED-FIRST`, `TOOLS` and `G-PLAN-COVERAGE` all passed. Red-first passed after adding `modules/import-star` of P7-4 to the exceptions (the P7-3 parser already emitted `importStar`)

### 9.5 E (type check)

- The mapping between the 17 examples in the severity table of concept §6.1 and diagnostic names (P8-0)
- P8-0 (2026-10-26): §4.21.1 records the diagnostic names of E (`NAM` 8, `TYP` 21, `LIN` 4, `SAF` 3, `KIT` 3, `SCH` 1, `ASM` 8, `MAC` 2, and `SCH101 tooManyCooks` of `std`'s `check`), the mapping of the 17 examples and the category examples, and how fuel is counted. The 17 examples and the 15 examples of the category table were placed in `core/tests/severity-examples.txt` (with a phase column; the 5 examples decided in F have phase F), and `scripts/check-severity-table.py` and its self-test (`4 cases, 0 failed`) were written. Codes are added to the registry in each P8 task that writes golden files, so at P8-0 `--phase E` gave `12 examples, 12 mismatched`, and gate E confirms 0. No new S was added. `cgolden/std/package` (a golden relying on the name and version of `std`; it passes with the current resolution, so it was written as a red-first exception) passed. Fuel adds evaluation nodes to D's name lookups, so P8-7 is expected to fix the expectations of `limits/steps-*`
  - Mutation check: `cgolden/std/package` (S357, Normal) — failed when the name in `stdlib/package.ozen` was changed to `stdx`. Restored and passed
  - Mutation check: the completion condition of P8-0 (D28, negative) — when the conversion script changed the order of foods on each run, running it again gave a diff in `stdlib/prelude.ozen` (`184 insertions(+), 184 deletions(-)`). Restored, no diff
- P8-1 (2026-10-26): 13 golden files were committed first, and semantic analysis (`sema.c`) was added. The unit table (dimensions held as a list of dimension names; the factor to the base unit is a rational with a range; cycles and a second base unit are checked), quantity evaluation (sums of juxtaposed quantities, the unit path `g/ml`, `%`), food values (type, axis state, amount), evaluation of recipe bodies (`ingredients`, `let`, action calls, list sugar, binary operations), `ozen debug steps`, and the CLI's `--strict`. `TYP001 dimension` to `TYP005 baseUnitChanged` and `NAM011 unknownUnit` were added to the registry. In the first run all 109 passed. There were places in `sema` that missed arena allocation failures (3 cases in the allocation failure injection), so the arena's failure count is now checked at the end of the run
  - Mutation check: `cgolden/dim/add-mismatch` (S186, No-effect condition) — failed when the dimension check for addition was removed. Restored and passed
  - Mutation check: `cgolden/dim/density-missing` (S187, Boundary) — failed when a missing density was taken as 1 g/ml. Restored and passed
  - Mutation check: `cgolden/dim/any-amount-strict` (S188, Boundary) — failed when `--strict` was ignored. Restored and passed
  - Mutation check: `cgolden/dim/pinch-strict-ok` (S189, No-effect condition) — failed when quantities with a range were also treated as vague in strict mode. Restored and passed
  - Mutation check: `cgolden/dim/rational-quarter` (S193, Normal) — failed when rationals that do not divide evenly were output rounded to 2 decimal places. Restored and passed
  - Mutation check: `cgolden/dim/unit-cycle` (S194, No-effect condition) — failed when cycle detection was removed. Restored and passed
  - Mutation check: `cgolden/dim/cup-us` (S195, Normal) — failed when `cup` was fixed to the first definition (`std.units.jp`) regardless of `import`. Restored and passed
- P8-2 (2026-10-26): 17 golden files were committed first, and we added type requirements on action arguments (whether one of the type argument options is met: a bare argument is a trait or part, `:` is a value, `>=` and `<=` are order, `has` is a set), the type effects of outputs (ordered axes do not go back, sets update the item's arguments, values with arguments show the argument values), cut fineness (following the `finer` relation of the enum. Going back to `cut` from a later value on the axis (`pureed`) also counts as coarser), and checks on axis definitions written by the user (unreachable values, outputs that go back before a required value). `TYP006 stateRequired`, `TYP007 cutCoarser` and `TYP008 axisDefinition` were added to the registry. The position and arguments of `stateRequired` are that argument's expression (the 3rd argument is the text of that expression). The effect is applied even if the requirement is not met, so `cut` after `pureed` gives 2 diagnostics, `cutCoarser` and `stateRequired` (the same as `cut-after-pureed` in the table stage). In the first run all 126 passed
  - Mutation check: `cgolden/state/peel-missing` (S196, Normal) — failed when the state requirement check was removed. Restored and passed
  - Mutation check: `cgolden/state/grill-then-cut` (S197, Normal) — failed when an action's output was made a new value that does not carry over the input's state (a single transition). Restored and passed
  - Mutation check: `cgolden/state/ordered-no-regress` (S198, Time and order) — failed when the effect on an ordered axis was made an assignment. Restored and passed
  - Mutation check: `cgolden/state/cut-coarser` (S199, No-effect condition) — failed when the fineness relation was followed backwards. Restored and passed
  - Mutation check: `cgolden/state/prep-set-args` (S200, Normal) — failed when a set effect replaced the set. Restored and passed
  - Mutation check: `cgolden/state/user-axis` (S201, No-effect condition) — failed when axes in the user's file were not read (not checked). Restored and passed
  - Mutation check: `cgolden/state/omit-axis` (S202, Normal) — failed when an effect on an axis the food does not use was made an error (making unused axes required). Restored and passed
  - Mutation check: `cgolden/state/doneness-browning-independent` (S203, Normal) — failed when the browning effect also advanced doneness to `done`. Restored and passed
  - Mutation check: `cgolden/state/unwritten-axis-inherit` (S204, Normal) — failed when set axes not written in the output were reset to empty. Restored and passed
  - Mutation check: `cgolden/state/axis-unreachable` and `axis-contradiction` (S205, No-effect condition) — failed when the check at definition time was removed. Restored and passed
  - Mutation check: `cgolden/state/blend-*` (S474, Normal) — treating `blend` the same as `cut` made `blend-after-pureed` fail. Restored and passed
- P8-3 (2026-10-26): 11 golden files were committed first, and safety, mixing and linearity were added. A mixed value inherits `Hazard` and `Allergen(...)` as traits from what it contains (` with ...` at the end of the `debug steps` line), and its doneness is the lowest one. Consumption is recorded per value; a second use is `LIN001 doubleUse`, and a second use in another branch of the same `parallel` is `LIN004 parallelConsume`. The built-in `split(食材, 量)` compares the amount in the food's dimension (mass if they differ); if there is not enough it gives `LIN002 insufficient` (arguments: the food expression, the required amount, the amount on hand), and if there is enough it returns 2 parts (`%n.1`, `%n.2`). At the end of a dish, parts that were not used up and are not the served value are reported as `LIN003 leftoverPart` (position: the bound name), and if the served value is `Hazard`, not `RawEdible`, and its `doneness` is before `done`, it gives `SAF001 undercooked` (position: the expression of the last statement). `SAF001` and `LIN001` to `LIN004` were added to the registry. `split` was added to the built-in names. Mixed values inherit allergens, so ` with Allergen(milk)` was added to the expectation of the existing `cgolden/state/blend-list` (an expectation-only commit). In the first run all 137 passed, and the 4 configurations and allocation failure injection (`G-CORE-OOM`) also passed
  - Mutation check: `cgolden/safety/undercooked-pork` (S206, No-effect condition) — failed when the `SAF` check was removed. Restored and passed
  - Mutation check: `cgolden/safety/raw-edible-exception` (S207, No-effect condition) — failed when the `RawEdible` exception was removed. Restored and passed
  - Mutation check: `cgolden/mix/new-value` (S210, Normal) — failed when the mixed value was made a copy of the first food (carrying over its state). Restored and passed
  - Mutation check: `cgolden/mix/amount-sum` (S211, Normal) — failed when the masses were not summed (keeping the first amount). Restored and passed
  - Mutation check: `cgolden/mix/mixture-hazard-allergen` (S212, Normal) — failed when trait inheritance was removed. Restored and passed
  - Mutation check: `cgolden/linear/double-use` (S213, No-effect condition) — failed when consumption recording was removed. Restored and passed
  - Mutation check: `cgolden/linear/shortage` and `exact-amount` (S214, Boundary) — treating it as enough only when the amount on hand is more than required (making the comparison `>=`) made `exact-amount` fail (`shortage` still passed). Restored and passed
  - Mutation check: `cgolden/linear/leftover-part` (S215, Normal) — failed when leftover parts were not reported. Restored and passed
  - Mutation check: `cgolden/linear/store-discard-silent` (S216, No-effect condition) — failed when consumed parts were also reported as leftovers. Restored and passed
  - Mutation check: `cgolden/linear/parallel-double-use` (S217, No-effect condition) — failed when a second consumption across branches was not reported. Restored and passed
- P8-4 (2026-10-26): 20 golden files were committed first, and we added parts (inheritance, properties, trait parts, discarded parts), aliases and candidates (requirements on all candidates, `Hazard` if even one, attributes as the widest range, display with the alias name), `contains` of `Mixture` (safety and allergens of what it contains, attributes from the range of ratios), butchering actions (tuple outputs, yield, distribution of the remainder), `where`, argument counts, the `uses … for` check on all candidates, same names for parts and traits, and `ozen debug requires` (requirements and the order of candidates). `TYP009 partless`, `TYP010 whereUnsatisfied`, `TYP011 aliasCandidates`, `NAM012 arity`, `NAM013 unknownArgument`, `NAM014 duplicateArgument` and `NAM015 partTraitClash` were added to the registry. Details are in the (added) parts of §4.21.1. Putting requirement lines in `debug steps` would add lines to every existing golden, so they were split into `debug requires`, and the expectations of `actions/requires-auto` and `extend-order` were moved there (an expectation-only commit). S224 (`uses-capability`) needs kitchen injection, so it was moved to P8-5. `actions/no-overload` passes with P7's duplicate check, so it was written as a red-first exception. Also, 10 of P8-2's `cgolden/state/*` and 3 of P8-3's passed before the implementation (positive golden files) but were not written as exceptions, so we confirmed which mutation check makes each one fail and wrote them as exceptions (the 10 of P8-2 fail with one of S197 to S204 or S474). In the first run all 157 passed, and the 4 configurations and allocation failure injection also passed
  - Mutation check: `cgolden/parts/inherit-trait` (S218, Normal) — failed when `extends` of parts was not followed. Restored and passed
  - Mutation check: `cgolden/parts/alias-*` (S219, Boundary) — judging by the first candidate only made `alias-one-not-fatty`, `alias-widest-range` and `alias-hazard-any` fail (`alias-all-fatty` still passed because all its candidates are fatty parts). Restored and passed
  - Mutation check: `cgolden/parts/alias-name-in-diag` (S220, Normal) — failed when the expanded type name was used for alias values. Restored and passed
  - Mutation check: `cgolden/parts/aibiki-contains` (S221, Normal) — failed when the ratio was made the single point of its lower bound. Restored and passed
  - Mutation check: `cgolden/parts/fillet-yield` (S222, Normal) — failed when parts with no yield kept the input amount as is. Restored and passed
  - Mutation check: `cgolden/actions/error-in-unused-variant` (S226, No-effect condition) — failed when only the first candidate was checked. Restored and passed
  - Mutation check: `cgolden/actions/extend-order` (S227, Time and order) — failed when added candidates were placed before the original candidates. Restored and passed
  - Mutation check: `cgolden/actions/requires-auto` (S228, Normal) — failed when the actions' `uses` were not collected (only the written `requires`). Restored and passed
  - Mutation check: `cgolden/actions/no-overload` (S229, No-effect condition) — failed when the second definition overwrote the first. Restored and passed
  - Mutation check: `cgolden/names/arity-*` (S414, Boundary) — not reporting unknown named arguments made `arity-unknown` fail, not reporting a second one made `arity-duplicate` fail, and not reporting missing arguments made `arity-missing` fail. Restored and passed
  - Mutation check: `cgolden/parts/part-on-partless` (S417, No-effect condition) — failed when the part check was removed. Restored and passed
  - Mutation check: `cgolden/actions/where-constraint` (S418, No-effect condition) — failed when `where` was ignored. Restored and passed
  - Mutation check: `cgolden/parts/trait-defined-parts` (S419, Normal) — failed when trait parts were not looked up. Restored and passed
  - Mutation check: `cgolden/parts/auto-discard` (S420, Normal) — failed when discarded parts were also reported as leftovers. Restored and passed
  - Mutation check: `cgolden/parts/part-trait-clash` (S421, No-effect condition) — failed when the same-name check was removed. Restored and passed
- P8-5 (2026-10-26): 17 golden files were committed first, and we added kitchen and options injection (`--kitchen 名前`, `--options 名前`; waiting for confirmation in D34), `std`'s `Kitchen` and `Options` and the types of equipment items, type checks on values (dimension, range, enum), location defaults, matching requirements against the kitchen (capabilities, bare arguments, capacity from amounts), narrowing by `variant` and `KIT:noVariant`, `KIT:requiresUnmet`, type `check` (`SCH101 tooManyCooks`. Ones that refer to the kitchen run only when there is a kitchen), binding of dish arguments, and checks on vessel liquid and capacity. `TYP012 rangeViolation`, `TYP013 axisValue`, `TYP014 vesselLiquid`, `KIT001 noVariant`, `KIT002 requiresUnmet`, `SCH001 vesselOverflow` and `SCH101 tooManyCooks` were added to the registry (`SCH101` is the code that `std`'s `check` uses). Details are in the (added) parts of §4.21.1. Only one kitchen can be chosen, so `capacity-from-amount` checks "for 2 L, a 2.4 L pot fits exactly and a 2.3 L pot fails" as "in a 2.4 L pot, 2 L fits exactly and 2.1 L fails", with 2 recipes (the same boundary and factor). S236 (`capacity-compile-time`) needs compile-time evaluation, so it was moved to P8-7. Allocation failure injection found an error where the axis count stayed when allocating the axes of a food copy failed, and it was fixed. In the first run all 174 passed, and the 4 configurations and allocation failure injection also passed. The `--phase E` severity table gave `12 examples, 2 mismatched`
  - Mutation check: `cgolden/actions/uses-capability` (S224, Normal) — failed when matching was by kind name only. Restored and passed
  - Mutation check: `cgolden/actions/capacity-from-amount` (S225, Boundary) — failed when the factor 1.2 was not applied (evaluating only the left side of the multiplication), and also failed when the capacity comparison was changed to "greater than". Restored and passed
  - Mutation check: `cgolden/kitchen/unitless-capacity`, `negative-count` and `misspelled-cleanup` (S230, No-effect condition) — removing the type check on values made all 3 fail. Restored and passed
  - Mutation check: `cgolden/kitchen/range-*` and `recipe-arg-range` (S231, Boundary) — not including the ends of a range made `range-bounds` fail, and ignoring open ranges made `range-open-ended` fail. Restored and passed
  - Mutation check: `cgolden/kitchen/check-severity` (S232, Normal) — failed when the severity of `check` was fixed to error regardless of `else`. Restored and passed
  - Mutation check: `cgolden/kitchen/check-without-kitchen` (S233, No-effect condition) — failed when `check` was evaluated with default locations even with no kitchen. Restored and passed
  - Mutation check: `cgolden/kitchen/variant-filtered` and `no-variant-left` (S234, Normal) — keeping unusable candidates made both fail. Restored and passed
  - Mutation check: `cgolden/kitchen/notation-defaults` (S235, Normal) — failed when the default of `std`'s `count` was set to 0. Restored and passed
  - Mutation check: `cgolden/vessels/soup-flat-plate` (S238, No-effect condition) — failed when the required constraint between liquid and vessel was removed. Restored and passed
  - Mutation check: `cgolden/vessels/capacity-over-warn` (S241, Boundary) — failed when the capacity check was removed. Restored and passed
  - Mutation check: `cgolden/kitchen/requires-unmet` (S429, No-effect condition) — failed when the recipe's `requires` was not matched at the type check stage (scheduling is in F, so in E this was checked by removing the matching). Restored and passed
  - Mutation check: `cgolden/kitchen/default-places` (S430, Normal) — failed when omitted locations were not added. Restored and passed
- P8-6 (2026-10-26): 14 golden files were committed first, and we added menu evaluation (`make`, `buy`, `choose`, tuple outputs, display of waves), how output types are decided, assumptions for a dish chosen with `--entry` (registering `ASM001 evenChoice`, `ASM002 noPlating` and `ASM003 externalIngredient`), and `std`'s `Person` and the property of serving timing. Details are in the (added) parts of §4.21.1. `multiple-outputs` wrote `ingredients` in a menu (which D's rules do not allow), so it was changed to buy the fish with `buy` (an expectation fix commit). `serve-time-in-code` (parsing already rejects times) and `while-warm-strict-ok` (nothing treats named properties as vague) pass before the implementation, so they were written as red-first exceptions. In the first run all 188 passed, and the 4 configurations and allocation failure injection also passed
  - Mutation check: `cgolden/menus/output-type-*` (S242, Normal) — not giving the output type (keeping the type of the last value) made both fail. Restored and passed
  - Mutation check: `cgolden/menus/make-buy-swap` (S243, Normal) — failed when the type of `buy` was given a different name. Restored and passed
  - Mutation check: `cgolden/menus/output-properties` (S244, Normal) — failed when allergens were not carried over to the output. Restored and passed
  - Mutation check: `cgolden/menus/choose-*` (S245, Normal) — ignoring the per-person breakdown (always even) made `choose-breakdown` fail (`choose-default-even` stays even, so it passed). Restored and passed
  - Mutation check: `cgolden/menus/split-pass` (S247, Normal) — failed when `split` was not counted as consumption. Restored and passed
  - Mutation check: `cgolden/menus/single-recipe-wrap` and `no-vessel-until-done` (S248, Normal) — not adding plating even when there is a vessel default made `single-recipe-wrap` fail. Restored and passed
  - Mutation check: `cgolden/menus/recipe-arg-external` (S249, Normal) — failed when the assumption about ingredients coming from outside was not emitted. Restored and passed
  - Mutation check: `cgolden/menus/waves` (S250, Normal) — failed when the interval of the second wave was ignored. Restored and passed
  - Mutation check: `cgolden/menus/serve-time-in-code` (S251, No-effect condition) — failed when parsing accepted `at` and a time after a wave name. Restored and passed
  - Mutation check: `cgolden/menus/while-warm-strict-ok` (S252, No-effect condition) — failed when annotations of named properties were reported as vague amounts (an error in strict mode). Restored and passed
  - Mutation check: `cgolden/menus/multiple-outputs` (S422, Normal) — failed when the second and later outputs of a tuple were dropped. Restored and passed
- P8-7 (2026-10-26): 16 golden files were committed first (`limits/steps-exact` was changed to `--max-steps 9` with the evaluation fuel added), and we added compile-time evaluation (`if`, `for`, `repeat`, comparison, logic, built-ins, kitchen values, division of foods), evaluation fuel and depth, hygienic macros (a scope per expansion, splicing, `let $x`, expansion history, generated nodes), checks at macro definition time, `check` of the user's option types, storing the expansion history and drawing ` via`, and the fuzz target `expand`. `TYP015 type`, `NAM016 undefinedInMacro` and `TYP016 spliceNotName` were added to the registry, and `macro/splice-not-name`, which uses `spliceNotName`, was added before the implementation (the registry check counts codes that appear in no golden). Details are in the (added) parts of §4.21.1. The macro name `prep` overrode the name of a `std` axis and emitted information, so the macro names in 3 golden files were changed (an expectation fix commit). `fuel-exact` (passes even without counting fuel) and `no-macro-defining-macro` (parsing already rejects nested `quote`) were written as red-first exceptions. In the first run all 205 passed, and the 4 configurations and allocation failure injection also passed. 60 seconds of fuzzing `expand` found no failures (the 600 seconds run at gate E)
  - Mutation check: `cgolden/macro/trail-nested` (S155, Normal) — failed when no expansion history was attached. Restored and passed
  - Mutation check: `cgolden/macro/trail-nested` (S257, Normal) — failed when the expansion history was listed from the outside in. Restored and passed
  - Mutation check: `cgolden/eval/runtime-value-in-if` (S253, No-effect condition) — failed when a comparison whose truth is not decided was taken as true. Restored and passed
  - Mutation check: `cgolden/macro/hygiene-*` (S254, Normal) — not making a new scope for an expansion (not marking) made `hygiene-tmp` and `hygiene-splice` fail (the other 3 are properties that do not depend on scope, so they still passed). Restored and passed
  - Mutation check: `cgolden/macro/definition-checks` (S255, Normal) — failed when the inside of `quote` was evaluated at definition time (also type checked). Restored and passed
  - Mutation check: `cgolden/macro/typecheck-after-expand` (S256, Normal) — failed when the body was evaluated before splicing (with `$x` still unknown). Restored and passed
  - Mutation check: `cgolden/eval/fuel-*` (S258, Boundary) — not counting evaluation steps made `fuel-short` fail. Restored and passed
  - Mutation check: `cgolden/macro/no-macro-defining-macro` and `no-capture` (S259, No-effect condition) — accepting nested `quote` made `no-macro-defining-macro` fail, and removing the name check at definition time made `no-capture` fail. Restored and passed
  - Mutation check: `cgolden/kitchen/capacity-compile-time` (S236, Normal) — failed when kitchen values were treated as unknown (values at cooking time). Restored and passed
  - Mutation check: `cgolden/kitchen/check-fuel` (S431, Boundary) — failed when fuel was not counted in `check` expressions. Restored and passed
  - Mutation check: `cgolden/kitchen/kitchen-has-branch` (S432, Normal) — failed when the result of `kitchen.has` was made a value decided at cooking time. Restored and passed
- P8-8 (2026-10-26): 18 golden files were committed first, and we added the severity system (build-time settings, annotations, priority of narrower scopes, `@expect`, groups, fixed `SAF`, suppression for dependencies), what annotations attach to, deprecated units, fix suggestions, checks on `check` codes, and diagnostics of the user's `check`. `NAM017 unknownDiagnostic`, `NAM018 expectUnfulfilled`, `TYP017 annotationTarget`, `SAF002 fixedSeverity`, `PKG012 deprecated`, `PKG013 checkCodeRange` and `PKG014 checkCodeDuplicate` were added to the registry, and `diag/unknown-diagnostic`, which uses `unknownDiagnostic`, was added before the implementation. Details are in the (added) parts of §4.21.1. We found and fixed an error (from P8-3) where an annotated tuple `let` mistook the annotation for the expression. `asm-strict-info`, `expect-suppresses` (because of that error) and `dep-saf-shown` pass before the implementation, so they were written as red-first exceptions. In the first run all 224 passed, and the 4 configurations and allocation failure injection also passed. The `--phase E` severity table gave `12 examples, 2 mismatched; categories: 15 examples, 3 mismatched` (`ASM:unlimitedEquipment` and `TYP:optionLevel` are handled in P8-9, `SAF:allergen` and `KIT:vesselShortage` at gate E)
  - Mutation check: `cgolden/packages/dep-*` (S178, No-effect condition) — also hiding `SAF` of dependencies made `dep-saf-shown` fail, and not hiding dependencies made `dep-diag-suppressed` fail. Restored and passed
  - Mutation check: `cgolden/dim/deny-strict-annotation` (S190, Normal) — failed when the scope of annotations was ignored (`diag/annotation-scope` also failed). Restored and passed
  - Mutation check: `cgolden/dim/strict-group-extended` (S191, Normal) — failed when the contents of a group were limited to those of `std`. Restored and passed
  - Mutation check: `cgolden/dim/asm-strict-info` (S192, No-effect condition) — failed when assumptions were also made errors in strict mode. Restored and passed
  - Mutation check: `cgolden/safety/allow-saf-stays-error` and `cli-allow-saf` (S208, No-effect condition) — removing the fixing of `SAF` made both fail. Restored and passed
  - Mutation check: `cgolden/diag/severity-override` (S261, Normal) — failed when build-time settings were ignored (severity fixed in code). Restored and passed
  - Mutation check: `cgolden/diag/select-by-name-prefix-group` (S262, Normal) — failed when settings by category prefix were ignored. Restored and passed
  - Mutation check: `cgolden/diag/annotation-scope` (S263, Boundary) — failed when annotations also took effect outside the declaration. Restored and passed
  - Mutation check: `cgolden/diag/expect-*` (S264, Normal) — making `@expect` the same as `@allow` (no warning when it does not hit) made `expect-unfulfilled` fail, and ignoring `@expect` made `expect-suppresses` fail. Restored and passed
  - Mutation check: `cgolden/diag/precedence` and `nested-annotations` (S265, Time and order) — giving build-time settings priority over annotations made `precedence` fail, and giving outer annotations priority made `nested-annotations` fail. Restored and passed
  - Mutation check: `cgolden/diag/fix-suggestion` (S267, Normal) — failed when fix suggestions were not emitted. Restored and passed
  - Mutation check: `cgolden/syntax/annotation-target` (S415, No-effect condition) — failed when annotations with nothing to attach to were ignored. Restored and passed
  - Mutation check: `cgolden/packages/deprecated-warning` (S428, Normal) — failed when the warning was not emitted. Restored and passed
  - Mutation check: `cgolden/diag/check-codes` (S464, Boundary) — failed when the range of the user's numbers was not checked. Restored and passed
- P8-9 (2026-10-26): 8 golden files were committed first, and we added verification levels, level assumptions (`ASM004 unlimitedEquipment`, `ASM005 defaultServings`, `ASM006 firstVariant`), option items that need a kitchen (`TYP018 optionLevel`, `@needs(kitchen)` on `std`'s `cleanup`), the property test `prop/monotonic`, and the fuzz target `typecheck`. Details are in the (added) parts of §4.21.1. Because there are more assumptions, 6 existing golden files that use `--entry` got an `ASM004` line, and `misspelled-cleanup` got an `optionLevel` line (an expectation-only commit; the diff is only those lines and the counts). `check-stops` and `level-2` pass before the implementation, so they were written as red-first exceptions, and `prop/monotonic` is a property that holds before the implementation, so it was written as an exception too. In the first run all 232 passed, `monotonic: 56 cases, 0 decreases`, and 60 seconds of fuzzing `typecheck` found no failures
  - Mutation check: `cgolden/kitchen/cleanup-without-kitchen` (S237, No-effect condition) — failed when it was used without a warning. Restored and passed
  - Mutation check: `cgolden/levels/level-*` (S270, Normal) — always making the level 2 made `level-0` and `level-1` fail. Restored and passed
  - Mutation check: `cgolden/levels/check-stops` (S271, Side effects) — failed when steps were also output for `check`. Restored and passed
  - Mutation check: `cgolden/levels/assumptions-as-info` (S272, Normal) — failed when assumptions were returned as warnings. Restored and passed
  - Mutation check: `prop/monotonic` (S273, Boundary) — not emitting `vagueAmount` when there is a kitchen gave `56 cases, 2 decreases`. Restored: `0 decreases`
  - Mutation check: `cgolden/levels/first-variant-assumed` (S275, Normal) — failed when the last candidate was used. Restored and passed
  - Mutation check: `cgolden/actions/extend-in-assumptions` (S424, Normal) — failed when added candidates were not put in the list of candidates. Restored and passed
- Gate E (P8-10, 2026-10-26): the goals of P7-6 (`G-CORE-BUILD`, `FLAGS`, `C23-SUBSET`, `DIAG-REGISTRY` (`87 codes … 0 unused`), `DIAG-APPEND`, `UNIT`, `ARENA-ACCESS`, `NOINT128`, `GOLDEN` (232 files × 4 configurations), `GRAMMAR` (131 rules), `STD-EMBED`, `G-STD-LISTS`, `STATIC`, `SOURCES`, `PROVENANCE`, `G-REPO-VERSION`, `G-CORE-OOM` (`212 files, 2716 points, 0 failures`), `PURE`, `NOGLOBAL`, `NOFLOAT`, `DETERMINISM`, `prop/reindent` (`984 cases, 0 differences`), `G-REPO-SPDX`, `SELFTESTS`, `RED-FIRST`, `TOOLS`, `G-PLAN-COVERAGE`) and the goals of P8 (`G-TYPE-SEVERITY-TABLE` (`--phase E`, `12 examples, 0 mismatched; categories: 13 examples, 0 mismatched`), `prop/monotonic` (`56 cases, 0 decreases`), `G-CORE-FUZZ` (`parse` `71283 execs in 602 s`, `expand` `41114 execs in 601 s`, `typecheck` `46557 execs in 601 s`, all 0 crashes, 0 leaks, 0 timeouts)) all passed. Difference from the plan: for `SAF:allergen` (`safety/allergen-bowl` is in P9-5) and `KIT:vesselShortage` (`vessels/count-short` is in P9-6) in the category table, the plan's table puts the golden in F's work, so their stage in `core/tests/severity-examples.txt` was changed to F (confirmed at gate F by `G-TYPE-SEVERITY-TABLE` (all 17 examples)). The remaining properties of `G-CORE-PROPS` (`split_merge` and others) and `G-STD-PHRASES` (renderer tooling) are added in the work from F on
- Record of mutation checks

### 9.6 F (IR and schedule)

- How the hand-built expected schedules were made, and the list of those examples (P9-0)
- How to build by hand (P9-0, 2026-10-26): the examples are small, with 3 to 6 tasks and 1 to 3 kinds of resources. (1) Write the IR on paper in the `debug ir` format, with the upper and lower bounds of each task's duration and its requirements. (2) Compute the priority (D26) as the length of the longest path from the task to the time it is served (if there is no `serve`, the end of the dish's last task), counted with upper bounds; if equal, order by IR number. (3) From time 0, take tasks whose dependencies are done in priority order, choose, among the free people and resource units, the assignment that can finish earliest (ties: candidate order → the one with no wash in between → resource declaration order), and set the start time. If a wash is needed, put the washing task (washing time at the sink, of hands, of tools) before it. (4) Write the start, upper end and lower end of every task, and if there is `--serve-at`, reverse the direction of time and do the same. (5) If repacking back and forth (D26, at most 8 round trips) changes it, make the schedule after the change the expectation and record the number of round trips. (6) Turn the expected schedule into schedule JSON and confirm with `tools/schedcheck/` that there are 0 violations, then make it the golden expectation. The hand-built examples are listed by name in this section, per golden
- Measured number of improvement round trips (did any example reach D26's limit of 8?)
- P9-0 (2026-10-26): §4.22.1 records the IR units and node kinds, how tasks' resources and durations are decided, the text formats of `ozen debug ir` and `ozen debug schedule`, the schedule JSON (the input of `schedcheck`), and the 8 checks of `tools/schedcheck/`; §9.6 records how to build expected schedules by hand. No new S was added, so the coverage table has no new rows. There is no new D (it was decided on top of the rules of D26 and D27)
- P9-1 (2026-10-26): 6 golden files (and `SCH002 kitchenDependent` in the expectations of 2 existing golden files that read kitchen values) were committed first, and we added `ozen debug ir`, the IR built while evaluating (ingredients when first used, expansion of `make`, branches for candidates, waves as numbers only, plating), information on kitchen values, the flat table of the IR, `cunit/ir_flat_ids`, and the property tests `display_only` (changing the order of ingredients and the names of waves), `ir_invariant` (cooks 1 to 3 × 3 kitchens), `split_merge` (cutting the last recipe out into another file and `import`ing it) and `meta_ignored` (adding `meta` to foods). `SCH002 kitchenDependent` was added to the registry. With `--entry`, only the IR of the entry dish is output (added to §4.22.1). In the first run all 238 passed, and `display_only: 29 cases`, `ir_invariant: 945 cases`, `split_merge: 95 cases` and `meta_ignored: 4 cases` all had 0 differences. The IR property tests match even without an IR, so they were written as red-first exceptions
  - Mutation check: `prop/split_merge` (S173, Normal) — mixing module numbers into IR numbers gave `95 cases, 8 differences`. Restored: 0
  - Mutation check: `prop/meta_ignored` (S223, Side effects) — reading `meta` as an attribute gave `4 cases, 1 differences`. Restored: 0
  - Mutation check: `cunit/ir_flat_ids` (S278, Data completeness) — adding a pointer to IR nodes stopped the test build at the size `static_assert`. Restored and passed
  - Mutation check: `prop/display_only` (S279, Side effects) — putting wave names in the IR gave `29 cases, 5 differences`. Restored: 0
  - Mutation check: `cgolden/ir/variants-as-choice` (S280, Normal) — failed when only the candidates usable in the kitchen were put in the IR. Restored and passed
  - Mutation check: `prop/ir_invariant` (S281, Platform path) — adding wash nodes to the IR when there is a kitchen gave `945 cases, 702 differences`. Restored: 0
  - Mutation check: `cgolden/ir/servings-changes-ir` (S282, Normal) — failed when the option `servings` was not used (only affecting the schedule). Restored and passed
  - Mutation check: `cgolden/ir/waves-in-ir` (S283, Normal) — failed when waves were removed from the IR. Restored and passed
  - Mutation check: `cgolden/ir/text-dump-format` (S285, Normal) — failed when the resource field was left out. Restored and passed
  - Mutation check: `cgolden/ir/rational-amounts` (S286, Normal) — failed when amounts that do not divide evenly were written as decimals. Restored and passed
  - Mutation check: `cgolden/ir/kitchen-dependent-info` (S463, Normal) — failed when information on kitchen values was not emitted. Restored and passed
- P9-2 (2026-10-26): we wrote `tools/schedcheck/schedcheck.py` (reads only the schedule JSON; rationals are `Fraction`) and its self-test `scripts/test-schedcheck.sh` (registered in `scripts/selftests.txt`). The checks are 9: the 8 of §4.22.1 plus a check on the order of start, lower end and upper end. To check washing and cleanup, `leaves`, `cleans`, `rules`, `cleanup`, `category` and `capabilities` were added to the schedule JSON (added to §4.22.1). The output was made `schedcheck: <段取りの数> schedules, <違反の数> violations` to match `G-SCHED-CHECK`. The self-test gives `test-schedcheck: 10 cases, 0 failed`
  - How the self-test was made: the 9 schedules each break one constraint of a correct schedule (dependency, resource, person, multiple capabilities of one unit, wave, serving timing, washing, cleanup, order of times), and we confirmed each one fails only with a violation of that check
- P9-3 (2026-10-26): 10 golden files with expected schedules built by hand with the §9.6 procedure (`sched/forward`, `priority-longest-path`, `tie-node-order`, `backward-serve-at`, `unattended-frees-hands`, `range-duration-upper`, `parallel-join`, `levels/critical-path`, `level-1-human-only`, `provisional-assumed`) were committed first, and we added the scheduler (`core/src/sched.c`; forward and backward list scheduling with resource constraints), control dependencies, call positions and provisional marks in the IR, `ozen debug schedule` (text and `--format json`), `ASM007 provisionalValue`, and `scripts/collect-schedules.sh` (with a self-test), which collects schedules. Details are in the (added) parts of §4.22.1. In the first run all 248 passed, and `G-SCHED-CHECK` gave `10 schedules, 0 violations`. The hand-built expected schedules needed no repacking (improvement is P9-7)
  - Mutation check: `cgolden/levels/critical-path` (S274, Normal) — failed when the shortest time at level 0 was counted as the sum of task times. Restored and passed
  - Mutation check: `cgolden/levels/provisional-assumed` (S277, Normal) — failed when the assumption for provisional values was not emitted. Restored and passed
  - Mutation check: `G-SCHED-CHECK` (S287, Data completeness) — removing the wait for dependencies gave `10 schedules, 4 violations`. Restored: 0
  - Mutation check: `cgolden/sched/priority-longest-path` (S288, Normal) — failed when the priority was reversed. Restored and passed
  - Mutation check: `cgolden/sched/tie-node-order` (S289, Boundary) — failed when ties were put in reverse order. Restored and passed
  - Mutation check: `cgolden/sched/forward` and `backward-serve-at` (S290, Normal) — packing from the front even with `serveAt` made `backward-serve-at` fail. Restored and passed
  - Mutation check: `cgolden/sched/unattended-frees-hands` (S294, Normal) — failed when every task was made to use a person (an unattended task that does not require `hands` using a person). Restored and passed
  - Mutation check: `cgolden/sched/range-duration-upper` (S304, Normal) — failed when assignment used lower bounds. Restored and passed
  - Mutation check: `cgolden/sched/parallel-join` (S433, Time and order) — failed when the join was removed. Restored and passed
  - Mutation check: `cgolden/levels/level-1-human-only` (S436, Normal) — failed when one piece of equipment per requirement was counted even at level 1. Restored and passed
- P9-4 (2026-10-26): 15 golden files with hand-built expectations (`sched/oven-range-one-unit`, `variant-earliest`, `variant-tie-order`, `variant-info`, `calibration`, `heat-model`, `heat-thickness-by-cut`, `heat-direct-attribute`, `hardness-order-warn`, `capacity-impossible`, `too-many-cooks`, `too-many-cooks-exact`, `person-attributes`, `actions/composite-expand`, `composite-schedule`, `composite-types`. The exact case of `too-many-cooks` was split into `too-many-cooks-exact`) were committed first, and we added candidate selection, calibration, capacity shortage, per-person limits, the heating time formula, hardness order, and composite actions. `SCH003 variantChosen`, `SCH004 softBeforeHard`, `SCH005 capacityShortage` and `TYP019 outputMismatch` were added to the registry, and `nominalCutSize` was added to `std`. Details are in the (added) parts of §4.22.1. `oven-range-one-unit`, `variant-tie-order` and `too-many-cooks` (2 files) pass before the implementation, so they were written as red-first exceptions. In the first run all 264 passed, and `G-SCHED-CHECK` also passed
  - Mutation check: `cgolden/sched/oven-range-one-unit` (S295, Boundary) — failed when a unit with multiple capabilities was counted as a separate unit per capability. Restored and passed
  - Mutation check: `cgolden/sched/variant-*` (S296, Normal) — always choosing the first candidate made `variant-earliest` fail, and choosing the later candidate on a tie made `variant-tie-order` fail. Restored and passed
  - Mutation check: `cgolden/sched/variant-info` (S297, Normal) — failed when the information about the chosen candidate was not emitted. Restored and passed
  - Mutation check: `cgolden/sched/calibration` (S298, Normal) — failed when calibration was not applied. Restored and passed
  - Mutation check: `cgolden/sched/heat-model` (S299, Boundary) — failed when the exponent was not applied (exponent set to 1). Restored and passed
  - Mutation check: `cgolden/sched/hardness-order-warn` (S300, No-effect condition) — failed when the hardness check was removed. Restored and passed
  - Mutation check: `cgolden/sched/capacity-impossible` (S301, No-effect condition) — failed when exceeding capacity was allowed. Restored and passed
  - Mutation check: `cgolden/sched/too-many-cooks*` (S302, Boundary) — changing the comparison in `std`'s `check` to `<` made `too-many-cooks-exact` fail. Restored and passed
  - Mutation check: `cgolden/sched/person-attributes` (S303, Normal) — failed when per-person limits were ignored. Restored and passed
  - Mutation check: `cgolden/actions/composite-*` (S434, Normal) — keeping composite actions as one task made all 3 fail. Restored and passed
  - Mutation check: `cgolden/sched/heat-thickness-by-cut` (S438, Normal) — failed when uneven cuts were also counted with the cut size. Restored and passed
  - Mutation check: `cgolden/sched/heat-direct-attribute` (S439, Normal) — failed when the formula was used instead of `direct`. Restored and passed
- P9-5 (2026-10-26): 21 golden files with expectations built by hand with the §4.22.1 procedure (`wash/dirt-from-leaves`, `compat-rules`, `wait-or-wash`, `reason`, `uses-sink-hands`, `hazard-not-relaxed`, `allergen-not-relaxed`, `same-food-same-state`, `no-drying`, `rule-override`, `staining-info`; `cleanup/none`, `before-meal`, `after-meal`, `wipe-places`, `store-to-fridge`; `safety/allergen-bowl`, `allergen-in-portion` (allergens through a food. Not in the plan's table, but added to check the first half of S209); `levels/small-tools-assumed`; `sched/store-discard-in-schedule`, `veg-before-meat`) were committed first, and we added dirt records, loading compatibility rules and checking the fixed rules, inserting washing and soaking (wait or wash), fewer washes first at equal priority, cleanup and wiping, treating `store` as cleanup, per-person allergies, the small tools assumption, and the plating vessel units. `SAF003 unwashedHazard`, `SAF004 allergen`, `SCH006 washInserted`, `SCH007 stainLeft` and `ASM008 smallToolsAssumed` were added to the registry, and `allergies` and `avoids` of `Person`, `smallTools`, `soakTime` and `tablewareWashTime` to `std`. `schedcheck` was changed to work out on its own the list of cases per dirt and the washing needed from the food keys and targets of tasks, and the fixed examples were also changed to the new format. Details are in the (added) parts of §4.22.1. Of the expectation golden files, 2 had errors made when building by hand (a missing `branch` name, the output type of a softening action) and 1 miscounted the order at the same time; they were fixed before the implementation and included in the golden commit. `rule-override` passes before the implementation, so it was written as a red-first exception. We found and fixed an error (from P9-3) where time did not advance after a task taking 0 seconds and the schedule became empty (`store-discard-in-schedule`), and allocation failure injection found an error (from earlier) that dereferenced a failed allocation of list arguments, which was fixed. In the first run all 285 passed, and the 4 configurations, allocation failure injection (`265 files, 3474 points, 0 failures`), `G-SCHED-CHECK` (`37 schedules, 0 violations`) and `ir_invariant` (`1008 cases, 0 differences`) also passed
  - Mutation check: `cgolden/safety/allergen-bowl` (S209, Real path) — failed when allergen dirt on tools was not recorded. Restored and passed
  - Mutation check: `cgolden/levels/small-tools-assumed` (S276, Normal) — failed when `KIT:requiresUnmet` was emitted even for small tools. Restored and passed
  - Mutation check: `G-CORE-PROPS` (`prop/ir_invariant`, S284, Side effects) — adding washing to IR task lines when there is a kitchen gave `765 differences`. Restored and passed
  - Mutation check: `cgolden/wash/dirt-from-leaves` (S305, Normal) — failed when the dirt of an action's `leaves` (`oil on FryingPan`) was not attached. Restored and passed
  - Mutation check: `cgolden/wash/compat-rules` (S306, Normal) — failed when the rule case one after the right one was used for the dirt trait (shifting the table by one row). Restored and passed
  - Mutation check: `cgolden/wash/wait-or-wash` (S307, Normal) — failed when the unit that needs washing was always chosen. Restored and passed
  - Mutation check: `cgolden/wash/reason` (S308, Normal) — failed when the reason (`after <汚れ>`) was removed from the wash line. Restored and passed
  - Mutation check: `cgolden/wash/uses-sink-hands` (S309, Side effects) — failed when washing did not use the sink. Restored and passed
  - Mutation check: `cgolden/wash/hazard-not-relaxed` and `allergen-not-relaxed` (S310, No-effect condition) — allowing the fixed rules to be replaced made both fail. Restored and passed
  - Mutation check: `cgolden/wash/same-food-same-state` (S311, No-effect condition) — failed when the same-food, same-state case was not applied. Restored and passed
  - Mutation check: `cgolden/wash/no-drying` (S312, Time and order) — failed when a 60-second wait was put after washing. Restored and passed
  - Mutation check: `cgolden/cleanup/before-meal` (S313, Normal) — failed when tableware was included in `beforeMeal`. Restored and passed
  - Mutation check: `cgolden/cleanup/wipe-places` (S314, Normal) — failed when wiping of locations was not added (`before-meal` also failed). Restored and passed
  - Mutation check: `cgolden/cleanup/store-to-fridge` (S315, Normal) — failed when `store` was not treated as cleanup (not moved to after the meal). Restored and passed
  - Mutation check: `cgolden/sched/store-discard-in-schedule` (S437, Normal) — failed when `store` tasks were removed from the schedule. Restored and passed
  - Mutation check: `cgolden/wash/rule-override` (S440, Normal) — failed when the user's rules did not replace the defaults. Restored and passed
  - Mutation check: `cgolden/wash/staining-info` (S441, Normal) — failed when `Staining` dirt was made to need washing. Restored and passed
  - Mutation check: `cgolden/sched/veg-before-meat` (S442, Normal) — failed when washes were not counted in ordering tasks of equal priority. Restored and passed
- P9-6 (2026-10-26): 19 golden files with expectations built by hand with the §4.22.1 procedure (`serve/serve-within`, `keep-warm-occupies`, `chilled-frozen`, `fridge-capacity-when-written`, `just-before-serve`, `long-wait-info`, `plating-time`, `vessel-busy-until-meal-end`; `vessels/fallback-order`, `count-exact`, `count-short`, `shared-platter`, `shared-leftover`; `menus/assemble-order`, `plate-overrides-dish`; `sched/meal-blocks-hands`, `meal-duration-from-interval`, `shared-cutting`; `cleanup/dishwasher-during-meal`), and the expectations of `cleanup/before-meal` and `after-meal` with a serving time line added, were committed first, and we added vessel counts and fallback order, portioning, consumption and leftovers from plating, the default vessel of `Dish`, serving timing (delay and rebuild), keeping warm and keeping chilled or frozen, information on dishes with long waits, waves and meals (no people used during a meal), `assemble`, shared cutting tasks, and running the dishwasher. `KIT003 vesselShortage`, `KIT004 vesselSubstituted` and `SCH008 longWait` were added to the registry, and `FridgeSpec`, `FreezerSpec`, `runTime` of `Dishwasher` (with a record of research) and `longWait` to `std`. Waves, dishes with serving timing, plating waves, and the list of tools the dishwasher washes were added to the schedule JSON, and `schedcheck` now accepts one task washing several resources. Details are in the (added) parts of §4.22.1. In the first run all 304 passed, and the 4 configurations, allocation failure injection (`284 files, 3755 points, 0 failures`), `G-SCHED-CHECK` (`52 schedules, 0 violations`) and the property tests (0 differences) also passed. The `on` of `keepWarm` cannot take type arguments in an annotation, so it is written with a capability or kind name (such as `Stove`)
  - Mutation check: `cgolden/vessels/fallback-order` (S239, Normal) — failed when the fallback order was reversed. Restored and passed
  - Mutation check: `cgolden/vessels/count-short` and `count-exact` (S240, Boundary) — loosening the shortage test by one plate made `count-short` fail, and tightening it by one made `count-exact` fail. Restored and passed
  - Mutation check: `cgolden/menus/assemble-order` (S246, Time and order) — failed when the order of `assemble` inputs was reversed. Restored and passed
  - Mutation check: `cgolden/sched/meal-blocks-hands` (S316, Time and order) — failed when tasks using people were placed during a meal. Restored and passed
  - Mutation check: `cgolden/cleanup/dishwasher-during-meal` (S317, Normal) — failed when the dishwasher was moved to after the last meal. Restored and passed
  - Mutation check: `cgolden/serve/serve-within` (S318, Boundary) — failed when the serving timing constraint was removed (a placement that passes with a wait of exactly 5 minutes). Restored and passed
  - Mutation check: `cgolden/serve/keep-warm-occupies` (S319, Side effects) — failed when keeping warm did not occupy a burner. Restored and passed
  - Mutation check: `cgolden/serve/chilled-frozen` (S320, Normal) — failed when the fridge capacity was taken as 1 even when not written. Restored and passed
  - Mutation check: `cgolden/serve/just-before-serve` (S321, Normal) — failed when `std`'s `justBeforeServe` was set to 5 minutes. Restored and passed
  - Mutation check: `cgolden/serve/long-wait-info` (S322, Boundary) — failed when the comparison was changed to `>=` (information also appears for a dish of exactly 30 minutes). Restored and passed
  - Mutation check: `cgolden/serve/plating-time` (S323, Side effects) — failed when plating was set to 0 minutes. Restored and passed
  - Mutation check: `cgolden/serve/vessel-busy-until-meal-end` (S324, Time and order) — failed when vessels were returned after each plating. Restored and passed
  - Mutation check: `cgolden/sched/shared-cutting` (S325, Normal) — failed when cutting tasks were not combined. Restored and passed
  - Mutation check: `cgolden/sched/meal-duration-from-interval` (S326, Time and order) — failed when the meal length was set to 0. Restored and passed
  - Mutation check: `cgolden/menus/plate-overrides-dish` (S423, Time and order) — failed when the recipe's `@vessel` was given priority over the menu's `plate`. Restored and passed
  - Mutation check: `cgolden/vessels/shared-platter` and `shared-leftover` (S435, Boundary) — not consuming the dish when plating made `shared-leftover` fail, and counting a vessel per person even with `shared` made `shared-platter` fail. Restored and passed
- P9-7 (2026-10-26): 8 golden files (`sched/objective-order`, `improvement`, `prefer-fewer-washes`, `prefer-less-waiting`, `prefer-fewer-tools`, `prefer-steady-pace`, `prefer-tools-then-pace`, `prefer-pace-then-tools`), and the expectations of `levels/critical-path` and `provisional-assumed` with the level 0 shortest time information added, were committed first, and we added building candidate schedules and choosing by the order of objectives, repacking back and forth, the 4 measures of `prefer`, `SCH:criticalPath`, and schedule fuzzing (`fuzz_schedule`, added to the default targets of `scripts/fuzz-core.sh`). `SCH009 criticalPath` was added to the registry. Details are in the (added) parts of §4.22.1. The inputs of `prefer-*`, `improvement` and `objective-order` were picked with a tool that searched small randomly generated recipes for ones whose result changes with the comparison mechanism, and the correctness of the schedules (dependencies, resources, washing, measure values) was checked by hand before making them expectations. `objective-order` and `prefer-tools-then-pace` give the same schedule as before the implementation, so they were written as red-first exceptions. Comparing candidates changed none of the existing golden schedules. In the first run all 312 passed, and the 4 configurations, allocation failure injection (`292 files, 3865 points, 0 failures`), `G-SCHED-CHECK` (`60 schedules, 0 violations`), the property tests (0 differences) and `G-TYPE-SEVERITY-TABLE` (all 17 examples, `17 examples, 0 mismatched; categories: 15 examples, 0 mismatched`) also passed
  - Mutation check: `cgolden/sched/objective-order` (S291, Time and order) — failed when the total time was compared before the serving timing. Restored and passed
  - Mutation check: `cgolden/sched/prefer-*` (S292, Normal) — sorting the `prefer` list into a fixed order made `prefer-pace-then-tools` fail, and not comparing by the measures made 5 fail. Restored and passed
  - Mutation check: `cgolden/sched/improvement` (S293, Normal) — failed when no repacking candidates were made. Restored and passed
  - Mutation check: `G-TYPE-SEVERITY-TABLE` (categories, S457, Data completeness) — changing the category of `vesselOverflow` in the registry to `KIT` gave `categories: 15 examples, 1 mismatched`. Restored and passed
- Gate F (P9-8, 2026-10-26): the goals of P8-10 (`G-CORE-BUILD`, `FLAGS`, `C23-SUBSET`, `DIAG-REGISTRY` (`102 codes`, unused 0), `DIAG-APPEND`, `UNIT`, `ARENA-ACCESS`, `NOINT128`, `GOLDEN` (312 files × 4 configurations), `GRAMMAR`, `STD-EMBED`, `G-STD-LISTS`, `STATIC`, `SOURCES`, `PROVENANCE`, `G-REPO-VERSION`, `G-CORE-OOM` (`292 files, 3865 points, 0 failures`), `PURE`, `NOGLOBAL`, `NOFLOAT`, `DETERMINISM`, `G-REPO-SPDX`, `SELFTESTS`, `RED-FIRST`, `TOOLS`, `G-PLAN-COVERAGE`, `G-TYPE-SEVERITY-TABLE` (all 17 examples, `17 examples, 0 mismatched; categories: 15 examples, 0 mismatched`)) and the goals of P9 (`G-SCHED-CHECK` (`60 schedules, 0 violations`), `G-CORE-FUZZ` (`parse` `77163 execs in 601 s`, `expand` `38526 execs in 601 s`, `typecheck` `41991 execs in 601 s`, `schedule` `43180 execs in 601 s`, all 0 crashes, 0 leaks, 0 timeouts), and, of `G-CORE-PROPS`, `reindent` (`1304 cases`), `split_merge` (`103 cases`), `meta_ignored` (`5 cases`), `monotonic` (`60 cases, 0 decreases`), `display_only` (`36 cases`) and `ir_invariant` (`1017 cases`), all with 0 differences) passed. Difference from the plan: `std_copy` (P13-1) and `bundle_roundtrip` (P15-2) of `G-CORE-PROPS`, and `G-STD-PHRASES` (renderer tooling, phase K) do not pass yet, because their tests and tools are made in those tasks (treated the same as at gate E)
- Record of mutation checks

### 9.7 G (API, JSON, CLI)

- The list of `schemas/`: `result.schema.json` (the result JSON; P10-2), `vendor/sarif-2.1.0.json` (SARIF 2.1.0 Errata 01. Source https://docs.oasis-open.org/sarif/sarif/v2.1.0/errata01/os/schemas/sarif-schema-2.1.0.json, fetched 2026-10-26, 112768 bytes, sha256 `c3b4bb2d6093897483348925aaa73af03b3e3f4bd4ca38cef26dcb4212a2682e`, fetched after the user confirmed. The license text is in `vendor/LICENSE-OASIS.txt`) (P10-3)
- Record of mutation checks
- P10-0 (2026-10-26): §4.23.1 records the public header, the `kitchen`/`options` text, the result JSON and its contents, and how `schemas/` is written; §4.25.1 records the CLI details. S476 (merging `options` declarations) was added, with a row in the coverage table. There is no new D. The core cannot call the standard library's allocation (S181), so the caller must always pass the allocation functions
- P10-1 (2026-10-26): we added `core/include/ozen.h`, `core/src/api.c` (`recipe_default_config`, `recipe_build`, `recipe_free_string`), the head of the result JSON (version, level, entry, file table, result, diagnostics) and the `RECIPE_EMIT_SCHEDULE` schedule, reading the `kitchen`/`options` text as `<kitchen>` and `<options>` files, merging `options` declarations, `strict`, `allow`, `warn` and `deny` of `Options` (`StrictMode` and `DiagnosticName` were added to `std`), and `cunit/api_levels_by_null`, `api_free_balance` and `api_options_merge`. `G-API-EXPORTS`, `G-API-HEADER` (4 combinations), `G-CORE-UNIT`, `PURE`, `NOGLOBAL`, `C23-SUBSET` and the 312 golden files passed
  - Mutation check: `G-API-EXPORTS` (S327, No-effect condition) — failed when an internal function was made an external symbol with no prefix. Restored and passed
  - Mutation check: `cunit/api_levels_by_null` (S328, Normal) — failed when a NULL `kitchen` was treated as having a kitchen. Restored and passed
  - Mutation check: `cunit/api_free_balance` (S329, Side effects) — failed when `recipe_free_string` did not free (an AddressSanitizer leak). Restored and passed
  - Mutation check: `G-API-HEADER` (S336, Platform path) — using `nullptr` in the header made the 2 `-std=c17` combinations fail. Restored and passed
  - Mutation check: `cunit/api_options_merge` (S476, Normal) — failed when the value of the first declaration was used. Restored and passed
- P10-2 (2026-10-26): 5 golden files (`api/diag-json-fields`, `signatures-json`, `text-material`, `types-json`, `local-not-listed`) were committed first, and `ir` (with quantities and arguments), `schedules`, `steps` (material for the text), `signatures`, `definitions` (excluding `local`) and `types` were added to the result JSON. We added `schemas/result.schema.json` (every object closed), `tools/jsoncheck/validate.py` with no external library and its self-test `scripts/test-validate.sh` (`test-validate: 10 cases, 0 failed`), `ozen check --format json --emit …` (goes through the public API. It finds the kitchen and options declarations by name, cuts out their text, and pads the front with spaces so lines and columns match the original file), and `run-golden.sh --json OUT` (writes the result JSON for each golden). We found and fixed an error where the definition list gave empty output on a syntax error, and an error where the CLI did not treat options declarations with a user-given type as options. `G-API-SCHEMA` (`json: 317 files, 0 invalid`), `G-STD-LISTS` and the 317 golden files × 4 configurations passed. `api/local-not-listed`, `types-json` and `signatures-json` include `std` definitions, so when `std` changes their expectations are fixed too
  - Mutation check: `G-API-SCHEMA` (S330, Normal) — writing rationals as strings produced invalid files (those that stop on a value with denominator 0 timed out with no output). Restored and passed
  - Mutation check: `G-API-SCHEMA` (S331, Data completeness) — adding an item not in the schema to diagnostics gave `242 invalid` (`bad-additional` in `scripts/test-validate.sh` is the same check). Restored and passed
  - Mutation check: `cgolden/api/diag-json-fields` (S332, Data completeness) — failed when related locations (expansion history) were not output. Restored and passed
  - Mutation check: `cgolden/api/signatures-json` and `text-material` (S334, Normal) — not outputting argument types made `signatures-json` fail, and not outputting the foods of the text material made `text-material` fail. Restored and passed
  - Mutation check: `cgolden/api/types-json` (S335, Normal) — failed when range constraints were not output. Restored and passed
  - Mutation check: `G-API-SCHEMA` (S337, No-effect condition) — adding an item with an English sentence to diagnostics gave `242 invalid`. Restored and passed
  - Mutation check: `cgolden/api/local-not-listed` (S425, No-effect condition) — failed when `local` was also listed. Restored and passed
- P10-3 (2026-10-26): the golden `api/sarif-mapping` (error, warning, information, expansion history, fix suggestions) was committed first, the SARIF 2.1.0 schema was brought in (after the user confirmed), and we added `ozen check --format sarif` (the CLI reads the result JSON and maps it to rules, artifacts and results. Information is `note`, expansion history is `relatedLocations`, fix suggestions are `fixes` that replace the diagnostic's range, and the message text is English made from the name), the draft-04 keyword mapping in `validate.py`, `run-golden.sh --sarif OUT`, and `tools/vendorcheck.py` with a self-test. `G-API-SARIF` (`sarif: 168 files, 0 invalid`), `tools/vendorcheck.py schemas/vendor/README.md` (`vendor: 1 files, 0 mismatched`. The rest of `G-REND-VENDOR` is Fluent in phase K) and the 318 golden files passed. The full text of the OASIS notice was not brought in, so `LICENSE-OASIS.txt` gives the copyright notice and where the IPR policy and notice are (checked by a person in §8)
  - Mutation check: `cgolden/api/sarif-mapping` (S333, Docs match) — failed when information was made `warning`. Restored and passed
- P10-4 (2026-10-26): 15 CLI tests (`tests/cli/*.sh`) and `scripts/run-cli-tests.sh` (self-test `scripts/test-run-cli-tests.sh`) were committed first, and the CLI was rewritten (subcommands `check`, `build`, `render`/`bundle`/`unbundle` (not usable yet) and `debug`, 4 ways to name the entry, checks on `-k` names, paths and ambiguity, the 4 short forms and rejection of abbreviations, `--options` and options text built from options (`ozen debug options`), project builds with `out/`, `index.json` and `--format sarif`, and removing the previous output). So that a per-entry build does not fail because of errors in other dishes, when an entry is passed to the public API only that dish is evaluated (added to §4.25.1). An error made when writing the tests (a menu used a recipe without `import`) was fixed. `G-CLI-TESTS` (`cli: 15 cases, 0 failed`) and the 318 golden files passed
  - Mutation check: `cli/subcommands` (S345, Normal) — failed when `unbundle` was removed. Restored and passed
  - Mutation check: `cli/entry-forms` (S346, Normal) — failed when paths were resolved from the end (`side.lunch` resolves to `side/lunch.ozen`). Restored and passed
  - Mutation check: `cli/entry-colon` (S347, No-effect condition) — failed when the `:` check was removed. Restored and passed
  - Mutation check: `cli/entry-ambiguous` (S348, No-effect condition) — failed when the candidate check was removed and the first one (all of them) was built. Restored and passed
  - Mutation check: `cli/kitchen-lookup` (S349, Boundary) — failed when the first kitchen found was used. Restored and passed
  - Mutation check: `cli/no-default-kitchen` (S350, No-effect condition) — failed when `kitchens/home.ozen` was made the default. Restored and passed
  - Mutation check: `cli/no-prefix-abbrev` (S351, Boundary) — failed when a prefix of a long form was accepted. Restored and passed
  - Mutation check: `cli/project-scope` (S352, Normal) — failed when modules with only definitions were also made entries. Restored and passed
  - Mutation check: `cli/project-order-continue` (S353, Time and order) — failed when it stopped at the first failure. Restored and passed
  - Mutation check: `cli/out-layout` (S354, Data completeness) — failed when `index.json` was not written. Restored and passed
  - Mutation check: `cli/options-to-ozen` (S355, Normal) — failed when `--prefer` was dropped. Restored and passed
  - Mutation check: `cli/diag-display` (S356, Normal) — failed when the English display made from the identifier was not output. Restored and passed
  - Mutation check: `cli/out-removed-entry` (S443, Persistence) — failed when the previous output was not removed (`out-rebuild-identical` checks that the same input gives not a single byte of difference). Restored and passed
- P10-5 (2026-10-26): `VERSION`, `ozen --version`, `stdlib/package.ozen` and `ozenVersion` in the library's result JSON (embedded from `VERSION` at build time) are all `0.1.0`. `G-REPO-VERSION` passed
- Gate G (P10-6, 2026-10-26): the goals of P9-8 (`G-CORE-BUILD`, `FLAGS`, `C23-SUBSET`, `DIAG-REGISTRY`, `DIAG-APPEND`, `UNIT`, `ARENA-ACCESS`, `NOINT128`, `GOLDEN` (318 files × 4 configurations), `GRAMMAR`, `STD-EMBED`, `G-STD-LISTS`, `STATIC`, `SOURCES`, `PROVENANCE`, `G-REPO-VERSION`, `G-CORE-OOM`, `PURE`, `NOGLOBAL`, `NOFLOAT`, `DETERMINISM`, `G-REPO-SPDX`, `SELFTESTS`, `RED-FIRST`, `TOOLS`, `G-PLAN-COVERAGE`, `G-TYPE-SEVERITY-TABLE`, `G-SCHED-CHECK`, `G-CORE-FUZZ` (`parse` `72694 execs in 602 s`, `expand` `37395 execs in 601 s`, `typecheck` `38277 execs in 601 s`, `schedule` `39318 execs in 601 s`, all 0 crashes, 0 leaks, 0 timeouts)) and the goals of P10 (`G-API-EXPORTS`, `G-API-HEADER`, `G-API-SCHEMA` (`json: 318 files, 0 invalid`), `G-API-SARIF` (`sarif: 168 files, 0 invalid`), `G-CLI-TESTS` (`cli: 15 cases, 0 failed`), `vendorcheck` (the SARIF part)) passed. Difference from the plan: `std_copy` (P13-1) and `bundle_roundtrip` (P15-2) of `G-CORE-PROPS` and the Fluent license of `G-REND-VENDOR` (phase K) do not pass yet, because they are made in those tasks (treated the same as at gate F)

### 9.8 H (4 environments)

- P11-0 (2026-10-26): §4.24.1 records the artifact names, the Swift, Java and JS bridge APIs, how bridge matching is compared (byte by byte on the `recipe_build` JSON string for each file in the input list), `javac --release 17`, and the CI job names (`linux`, `fuzz`, `wasm`, `ios`, `android`). There are no new S or D
- P11-1 (2026-10-26): we added `libozen.so` (the version script `core/ozen.map` exposes only the 3 public symbols), the Swift module map (`COzen`) with `Ozen.swift` and `bindings/linux/swift-test.sh`, the JNI wrapper `ozen_jni.c` with `ozen.Ozen` and `bindings/android/jvm-test.sh`, the input list `bindings/inputs.txt` (12 inputs, including `bindings/inputs/utf8.ozen` with Japanese and emoji), and the self-test `scripts/test-bridge-tests.sh`. The Swift bridge takes a string, so the invalid UTF-8 input (`syntax/utf8-invalid`) is not in the list. `G-PLAT-LINUX` (`swift: 12 inputs, 0 mismatches`, `jni: 12 inputs, 0 mismatches`, 3 dynamic symbols) passed
  - Mutation check: `G-PLAT-LINUX` (`jni`, S340, Boundary) — reading the result on the Java side as ASCII instead of UTF-8 gave `10 mismatches` (Japanese and emoji break, the same as with the modified UTF-8 of `NewStringUTF`). Restored and passed
  - Mutation check: `G-PLAT-LINUX` (dynamic symbols, S341, Normal) — removing the version script exposed internal symbols and it failed. Restored and passed
  - Mutation check: `G-PLAT-LINUX` (`swift`, the Linux row of S343, Platform path) — dropping the trailing line break of the output in the Swift bridge gave `12 mismatches`. Restored and passed
- P11-2 (2026-10-26): we added `scripts/build-wasm.sh`, which uses Emscripten 6.0.13 to build into `build/core-wasm/` a node CLI (`ozen.js`, `NODERAWFS`) and `ozen_bridge.js` from the C bridge `bindings/wasm/bridge.c` (`-sENVIRONMENT=web,worker,node`, `MODULARIZE`, `OzenModule`); the JS bridge `bindings/wasm/ozen.mjs` (`build(mod, {sources, kitchen, options, entry, emit})`, with lengths in UTF-8 bytes); `bindings/wasm/worker-test.sh`, which runs the input list through both node and a Web Worker and compares with the local CLI's JSON (as in P5-2, the page POSTs the result to a check server. Chrome 154.0.8037.97, node v20.20.2); and the self-test `scripts/test-worker-test.sh`. Mismatches are counted per input (1 if either node or the Worker differs). `G-PLAT-WASM` (318 golden files, `worker: 12 inputs, 0 mismatches`, matching digest) passed
  - Mutation check: `G-PLAT-WASM` (`worker`, S342, Platform path) — passing `s.text.length` (the UTF-16 count) as the length in the bridge gave `worker: 12 inputs, 1 mismatches` (`utf8.ozen` with Japanese and emoji differs in both node and the Worker). Restored and passed. The same break was also put in the self-test
  - emcc (`-std=c23`) was added to `scripts/check-c23-subset.sh`, making t = 3, and it now reads `build/ci/c23-*/c23.txt` written by the CI jobs and adds them to the toolchain count (t = 5 after P11-3 and P11-4)
- P11-3 (2026-10-26): `ios` in `.github/workflows/ci.yml` (`macos-26`, Xcode 26.6). The static libraries for device, simulator and macOS (`arm64;x86_64`) are combined into `Ozen.xcframework` (headers: `core/include/ozen.h` and `bindings/apple/module.modulemap`); `bindings/linux/swift-test.sh` (the compiler, run prefix and module folder can now be changed with environment variables) is run on the macOS slice and the simulator slice; golden and digest run on the simulator; and the 12 probes go through Xcode's clang. The artifacts are `digest-ios`, `xcframework`, `swift-macos`, `swift-ios`, `c23-ios` and `tools-ios`. Fixes: `simctl spawn` starts the child in `/` and does not inherit the caller's folder, so the CLI is run through `bindings/apple/sim-chdir.c` (`chdir`, then `execv`) and `bindings/apple/sim-run.sh` for the simulator. `scripts/golden-entries.sh` used GNU `find -printf`, so 20 folder golden files were missing on macOS; it was changed to `dirname`. The device golden step succeeds only when all cases pass. `G-PLAT-IOS` (matching digest `c668e8a2…`, 3 slices, `swift: 12 inputs, 0 mismatches`) passed
  - Mutation check: `G-PLAT-IOS` (S338, Platform path) — removing `ios-arm64` from the CI artifact `Ozen.xcframework.zip` and passing it to the judgment command gave 2 slices and it failed (checked on a copy of the artifact)
  - Mutation check: `G-PLAT-IOS` (S455, Platform path) — likewise, removing `macos-arm64_x86_64` gave 2 slices and it failed (in CI, the Swift macOS step also fails)
  - Mutation check: `G-CORE-C23-SUBSET` (the Xcode row of S151, Platform path) — removing `scripts/c23-probes/04-constexpr.c` gave `c23: 11 allowed probes ok on 5 toolchains` and it failed. Restored and passed
  - The iOS row of S343: the macOS and simulator Swift bridges are compared with the same `bindings/swift/Ozen.swift` and `swift-test.sh` as Linux, so they fail the same way with P11-1's break (dropping the trailing line break)
- P11-4 (2026-10-26): `android` (`ubuntu-24.04`). `sdkmanager` installs NDK 30.0.16248370 (`ndk 30.0.16248370 ci` in `toolchain.txt`; not fetched locally), emulator 37.2.12.0 and `system-images;android-34;default;x86_64` (API 34), and builds `libozen.so` for 4 ABIs (`ozen_shared` is now built on Android too), `libozen_jni.so`, and the x86_64 CLI. On the emulator, `bindings/android/emulator-test.sh` (dexes with `d8`, runs `JvmTest` with `dalvikvm`, and compares with the local CLI's JSON. The self-test is `scripts/test-emulator-test.sh`, which uses fake `adb`, `d8` and `dalvikvm`) and golden and digest through `bindings/android/adb-run.sh` (copies the host folder to the same place under `/data/local/tmp/ozen/repo/` on the device and runs the CLI). Fixes: the NDK deprecates `strtok`, so the CLI uses `strtok_r`; the AVD folders of `avdmanager` and the emulator were aligned with `ANDROID_AVD_HOME`; `adb shell` sent standard input to the device and swallowed the golden list, so `/dev/null` is passed; and the boot wait got a 600-second limit. `G-PLAT-ANDROID` (matching digest, 4 ABIs) and `jni-android: 12 inputs, 0 mismatches` passed
  - Mutation check: `G-PLAT-ANDROID` (S339, Platform path) — removing `x86` from the artifact `android-libs` made the ABI list `arm64-v8a armeabi-v7a x86_64` and it failed (checked on a copy of the artifact)
  - The Android rows of S340 and S343: the JNI wrapper and the Java classes are the same as in the Linux JVM test, so they fail the same way with P11-1's breaks (reading as ASCII, dropping the trailing line break)
- P11-5 (2026-10-26): we added `linux` (clang 17.0.0 from the Swift 6.2.3 distribution and CMake 3.28.3 from `pipx`, the same versions as locally; `G-CORE-BUILD`, `UNIT`, `GOLDEN`, `OOM`, `G-CLI-TESTS` and `G-PLAT-LINUX` from `scripts/goals.sh`), `fuzz` (`scripts/fuzz-core.sh 60`) and `wasm` (emsdk 6.0.13, node 20.20.2, `G-PLAT-WASM`), and wrote the job names in `ci/jobs.txt`. Every job writes the versions of the tools it uses to `tools.txt`, which `scripts/check-ci-tools.py` compares (giving priority to the `ci` rows of `toolchain.txt`. The self-test is `scripts/test-check-ci-tools.sh`). Fixes: with the Makefile generator, the 2 libraries sometimes wrote the same `std_embedded.c` in parallel and the gcc-release build failed, so generation was combined into one target, `ozen_std_gen`. `build-core.sh` also prints the error lines on failure. `bindings/` and `.swift`, `.java`, `.js` and `.html` were added to the SPDX check. `G-PLAT-CI`, `G-REPO-TOOLS-CI` (`ci-tools: 5 jobs, 0 mismatched`) and `G-CORE-C23-SUBSET` (`12 allowed probes ok on 5 toolchains`) passed
  - Mutation check: `G-PLAT-CI` (S344, Data completeness) — removing `fuzz` from the run's job list and comparing with `ci/jobs.txt` gave a `> fuzz success` difference and it failed (instead of pushing a run with the job removed from the workflow, this was checked on the input of the judgment command)
- Gate H (P11-6, 2026-10-26): the goals of P10-6 (`G-CORE-BUILD`, `FLAGS`, `C23-SUBSET` (`12 allowed probes ok on 5 toolchains`), `DIAG-REGISTRY` (`102 codes`), `DIAG-APPEND`, `UNIT`, `ARENA-ACCESS`, `NOINT128`, `GOLDEN` (318 files × 4 configurations), `GRAMMAR` (131 rules), `STD-EMBED`, `G-STD-LISTS`, `STATIC`, `SOURCES`, `PROVENANCE`, `G-REPO-VERSION`, `G-CORE-OOM` (`298 files, 3951 points, 0 failures`), `PURE`, `NOGLOBAL`, `NOFLOAT`, `DETERMINISM`, `G-REPO-SPDX`, `SELFTESTS`, `RED-FIRST`, `TOOLS`, `G-PLAN-COVERAGE`, `G-TYPE-SEVERITY-TABLE`, `G-SCHED-CHECK` (`60 schedules, 0 violations`), `G-CORE-FUZZ` (`parse` `74577 execs in 601 s`, `expand` `36424 execs in 601 s`, `typecheck` `34169 execs in 601 s`, `schedule` `39404 execs in 601 s`, all 0 crashes, 0 leaks, 0 timeouts), `G-API-EXPORTS`, `G-API-HEADER`, `G-API-SCHEMA` (`json: 318 files, 0 invalid`), `G-API-SARIF` (`sarif: 168 files, 0 invalid`), `G-CLI-TESTS` (`cli: 15 cases, 0 failed`)) and the goals of P11 (`G-PLAT-LINUX`, `G-PLAT-WASM`, `G-PLAT-IOS`, `G-PLAT-ANDROID`, `G-PLAT-CI`, `G-REPO-TOOLS-CI`. In CI, the run for commit `8476dcb` had all 5 jobs `success`) passed. Difference from the plan: `std_copy` (P13-1) and `bundle_roundtrip` (P15-2) of `G-CORE-PROPS` and the Fluent license of `G-REND-VENDOR` (phase K) do not pass yet, because they are made in those tasks (treated the same as at gate G). The CI goals read the run for the current HEAD, so CI is checked again after this gate's record is pushed

- The NDK version (P11-4): 30.0.16248370 (r30). Installed by `sdkmanager` in CI; not fetched locally
- The `javac --release` version (D29, P11-0): 17 (the class file version that d8 of Android Gradle Plugin 8.x accepts. Built with `javac --release 17` of the local JDK 25)
- The Xcode version and the result of the 12 probes (P11-3): Xcode 26.6 (image `20261013.0424.1` of `macos-26`). All 12 probes passed
- The emulator system image and API level (P11-4): `system-images;android-34;default;x86_64` (API 34), emulator 37.2.12.0
- CI run times (R4): in the run where all jobs passed (`9dcbbf0`), `ios` 21.5 minutes, `fuzz` 4.8 minutes, `android` 2.8 minutes, `linux` 2.2 minutes, `wasm` 1.7 minutes (GitHub's record). One run was cancelled because no macOS runner was assigned, and only the job was rerun
- Record of mutation checks

### 9.9 I, J (samples)

- P12-1 (2026-10-26): first `samples/curryRice/expect.yaml` was committed (5 runs: no kitchen, kitchen `home`, `--cooks 1`, `--cooks 2`, kitchen and `--cooks 2`. 0 errors, the level, IR names and the links from ingredients to plating, schedule tasks and order, and the number of cooks used), and then we added the check `tests/samples/check.py` (for each of the `runs` in `expect.yaml`, it runs `ozen check --format json --emit ir,schedule,steps` and matches the properties, and also passes every schedule in the result through the `tools/schedcheck/` check. The self-test is `tests/samples/test-check.sh`) and the sample (`cookedRice`, `curry`, the menu `curryRice`, the kitchen `home`. As in §3.4, cooking rice is a candidate of `cookRice`). A folder that has `expect.yaml` counts as one entry in the list of golden entries, and the digest runs it with the arguments in `args` (so the samples are in the digest of every environment). CC-BY-4.0 for `samples/` was added to the SPDX check. `G-SAMPLE-CHECK` (`samples: 1 samples, 62 expectations, 0 failed`), `G-SCHED-CHECK` and `G-PLAT-WASM` (matching digest including the sample) passed
  - Things found and fixed while writing the sample (in both cases the golden was committed first): items not in the type of a kitchen item, option or record value were silently ignored (`kitchen/unknown-field`. Made `NAM:unknownArgument`; added to the kitchen and option type row of §4.21). When an entry was passed to the public API, errors inside recipes that the entry menu called with `make` were dropped from the JSON (`api/entry-made-errors`. They appeared in the text output)
  - Mutation check: `sample/curryRice` (S363, Platform path) — removing `peel` from `curry.ozen` made the 3 expectations `ir label peel`, `task peel` and `peel before cut` fail, and the digest changed too (before the fix, errors past `make` did not appear in the JSON, so `errors` stayed 0. After the fix, `TYP:stateRequired` also appears). Restored and passed
  - Behavior of kitchen matching: the rice cooker candidate takes 42 to 54 minutes and the pot candidate 25 to 35 minutes, so in a kitchen with 2 pots, `cookRice` uses the pot candidate (the candidate that finishes earliest). In a kitchen with 1 pot, the pot candidate makes the curry's `simmer` wait for the pot and the whole thing gets longer (because candidate choice is a local decision per task). The sample kitchen has 2 pots
- P12-2 (2026-10-26): after confirming, we pushed up to commit `14fde8f`, and CI run `40413793325` had all 5 jobs `success`. The digest including the sample is the same `9329767…` on the local CLI (the 320 golden files at that time and the sample), the iOS simulator and the Android emulator. WASM matched in the local `G-PLAT-WASM` (`G-PLAT-*` is checked again at the HEAD of the next push)
- P13-0 (2026-10-26): §4.27.1 records the form of `expect.yaml` and the property vocabulary, the reference menus, and the runs and properties of T1 to T4. We found that the result JSON had no allergens or choice breakdown, and got confirmation as D33 (`allergens` in the IR section, `choices` per dish). There is no new S
- P13-1 (2026-10-26): the lists, relations, provenance and prelude of `std` had gone in during P7 to P9, and `G-STD-LISTS` (`267 listed`) and `G-STD-PROVENANCE` (`97 values`) passed. We added the golden `std/prelude-contents` (`gou`, `kg`, `pcs`, `L`, `pinch`, `tbsp`, `tsp`, `min`, `h` and the basic actions can be used without `import`) and the property test `tests/props/std_copy.py` (copies the definitions of the `std` actions that golden dishes call into the user's file with the same syntax, and compares the text IR). `std_copy: 105 cases, 0 differences`. Both pass before the change, so they were written as red-first exceptions
  - Mutation check: `G-STD-LISTS` (S358, Data completeness) — removing `tear` from `stdlib/prelude.ozen` failed with `1 missing`. Restored and passed
  - Mutation check: `G-STD-PROVENANCE` (S359, Data completeness) — removing `@source(provisional)` from `unitWeight` of `Onion` failed with `1 without source`. Restored and passed
  - Mutation check: `prop/std_copy` (S360, Normal) — marking only the IR lines of `std` actions gave `104 cases, 103 differences`. Restored and passed
  - Mutation check: `cgolden/std/prelude-contents` (S361, Normal) — failed when `unit g` was removed from the prelude. Restored and passed
- P13-2 (2026-10-26): we added `phrases/ja.ftl` and `phrases/en.ftl` (209 keys: 29 `std` actions, 38 foods, 13 parts, 2 aliases, 11 vessels, 14 properties, 102 diagnostics. CC-BY-4.0) and the drift check `renderers/tools/phrases-check.mjs` (self-test `renderers/tools/test-phrases-check.sh`). How keys are named is in §4.26.1. `renderers/` and `phrases/` were added to the SPDX check. `G-STD-PHRASES` (`phrases: 209 keys, 0 missing in ja, 0 missing in en, 0 unknown args`) passed
  - Mutation check: `G-STD-PHRASES` (S362, Data completeness) — removing `action-tear` from `en.ftl` failed with `1 missing in en`. Restored and passed
- P13-3 (2026-10-26): first the `expect.yaml` files of `samples/menuA` to `menuE` (the properties of §4.27.1, 11 runs, 100 expectations) were committed, and then the menus were written (A, yakisoba, is a single dish with `@vessel(FlatPlate)`; B is the same composition as "curry and rice"; C is a stir-fry with pork marinated and left to rest, miso soup and rice; D is curry (rice or naan per person), salad, and a parfait in the dessert wave; E is a mixed juice that does not fit in the blender and is split into 2 batches. All use only the `std` vocabulary). `serveWithin` (the number of dishes with a limit on the time until serving) was added to the check vocabulary. `G-SAMPLE-CHECK` (`samples: 6 samples, 162 expectations, 0 failed`), `G-SCHED-CHECK` and `G-SAMPLE-STD-ONLY` passed
  - Things found and fixed while writing the menus (in every case the golden was committed first): plating and waves were added to a single `@vessel` recipe only in the `debug steps` display, not in the IR and schedule (S248. `menus/single-recipe-ir`, `single-recipe-schedule`; the expectation of `levels/provisional-assumed` was also fixed). The vessel list of a property (`deepPlate`) was not expanded in the liquid check, so even a deep plate gave `TYP:vesselLiquid`, and `dryOnly` of `smallBowl` did not take effect either (`vessels/liquid-vessel-order`). An explicit `combine([…])` dropped inputs (`mix/explicit-combine`). `@chilled` and `@frozen` on items inside `plate` and `assemble` did not take effect (`serve/item-frozen`, added to §4.22.1). Holds were not in the schedule JSON (`serve/holds-json`, added to D33). `washInserted`, `variantChosen`, `stainLeft` and `provisionalValue` appeared only in the schedule text, not in the JSON result (`api/schedule-diags-json`). The position of `assemble` tasks pointed at the call just before. How waits for serving timing are counted (D34. `serve/serve-within-missed`, `dessert-just-before`, `missed-json`, and `SCH010 serveWithinMissed` in the registry)
  - Mutation check: `sample/menuA` (S364, Normal) — not adding plating to a single dish made 6 expectations fail, including `waves`, `plate uses FlatPlate` and `season before plate`. Restored and passed
  - Mutation check: `sample/menuB` (S365, Normal) — not linking the output of `make` into the IR made 6 expectations on the links from ingredients to plating and on order fail. Restored and passed
  - Mutation check: `sample/menuC` (S366, Real path) — not combining shared cutting tasks made 2 `task count cut@Cabbage: 2` expectations fail. Restored and passed
  - Mutation check: `sample/menuD` (S367, Real path) — giving the second wave the same number as the first made `waves` and `wave of plate` fail. Restored and passed
  - Mutation check: `sample/menuE` (S368, Real path) — changing `std`'s `afterSticky` to `wash` made `task soak` fail. Restored and passed
  - Mutation check: `G-SAMPLE-STD-ONLY` (S456, Data completeness) — adding one `ingredient` to `samples/menuA/yakisoba.ozen` failed with `1`. Restored and passed
  - Record of schedule behavior: in menu E, the first batch of juice waits 30 minutes for the blender to soak (`afterSticky` soaks even for the same food) and is then served together with the second (`serveWithin` is counted at the task after they are combined). Menu C packs from the front when `--serve-at` is not given, so the stir-fry and miso soup wait 40 minutes or more for the rice (`SCH:longWait`)
- P13-4 (2026-10-26): as D35 says, we added to `std` the action `fillet` for butchering a round fish (outputs 8 parts; hands, cutting board and knife; dirt `rawFish`), provisional yields for the parts of `RoundFish` (head 15–20%, collar 5–8%, back 25–30%, belly 15–20%, bones 10–15%, skin 3–5%, guts 8–12%, scales 1–2%), and the food `Egg` (`Allergen(egg)`, `Hazard`, `rawEgg`, 58–64 g each), with research records (no primary source was found) and phrase keys (the golden `std/round-fish-fillet` and the expectations of existing golden files whose amounts are now decided were committed first). First the `expect.yaml` files of `samples/T1` to `T4` (T3 has a and b; both run menus B and C per kitchen) were committed, and then the test recipes were written (T1 butchers one yellowtail into sashimi, grilled collar and simmered bones; T2 is oyakodon for 3 and an egg-free chicken bowl for 1; T3a is a kitchen with no rice cooker; T3b has a dishwasher and `--cleanup afterMeal`; T4 is a beef kalbi bowl with kalbi and raw kalbi). A per-run `entry` was added to the check, and `reaches` now passes if it reaches any plating. `G-SAMPLE-CHECK` (`samples: 11 samples, 236 expectations, 0 failed`), `G-SCHED-CHECK` and `G-SAMPLE-STD-ONLY` passed
  - Things found and fixed while writing the test recipes (in every case the golden was committed first): a value mixing things that can be eaten raw did not inherit `RawEdible`, so the sashimi garnish gave `SAF:undercooked` (`safety/raw-edible-mixture`, added to §4.21). `wash` of foods that require `Washing` was assigned to the dishwasher (`cleanup/food-wash-not-dishwasher`. `std`'s `Dishwasher` keeps `Washing` as concept §12.3 says, and a rule not to assign the dishwasher to food tasks was added to §4.22.1. An expectation removing the capability was committed once and then reverted). Raw meat bought with `buy` in a menu and plated as is did not give `SAF:undercooked` (`safety/plate-raw-buy`. Dishes made with `make` are checked on the recipe side, so plating does not look at them). The phrases check did not count trait parts (`RoundFish`)
  - Mutation check: `sample/T1` (S369, Real path) — making the output of `std`'s `fillet` a single `T<back>` made 7 expectations fail. Restored and passed
  - Mutation check: `sample/T2` (S370, Real path) — not looking at per-person allergies made `diagnostic allergen ['egg']` fail. Restored and passed
  - Mutation check: `sample/T3a` and `T3b` (S371, Real path) — fixing the candidate to the first one (rice cooker) made 2 expectations of T3a (`variantChosen … unavailable`) fail (T3b is a kitchen with a rice cooker, so it does not change). Restored and passed
  - Mutation check: `sample/T4` (S372, Real path) — not expanding aliases at first made no expectation fail (even without expansion, the argument check of `Beef<Fatty>` does not fail). We added a run that checks that `Hazard` passes through the alias (plating raw kalbi gives `SAF:undercooked`), and along the way found and fixed the raw meat bug with `buy` above. After the fix, `diagnostic undercooked ['Karubi']` failed. Restored and passed
- P13-5 (2026-10-26): after confirming, we pushed up to commit `c2399aa`, and CI run `40437608127` had all 5 jobs `success`. The digest including all samples (11) is the same `2024f97a…` on the local CLI, WASM, the iOS simulator and the Android emulator (`G-PLAT-LINUX`, `G-PLAT-WASM`, `G-PLAT-IOS`, `G-PLAT-ANDROID`, `G-PLAT-CI`, `G-REPO-TOOLS-CI` and `G-CORE-C23-SUBSET` passed). The schedules of all samples pass the `tools/schedcheck/` check inside `tests/samples/check.py` (S373)
  - Mutation check: `G-SCHED-CHECK` (all sample schedules, S373, Data completeness) — `tools/schedcheck/` had no check for people's tasks during a meal ((9)), so at first it was not detected. The check and the example `bad-meal.json` were added (the wiping during a meal in hand-made `ok.json` and others was moved to after the meal). Then stopping the meal time slot in the schedule put the assembly of menu D's parfait inside the meal, and 2 cases failed with `meal: task 39 …`. Restored and passed
  - Mutation check: the digest of `G-PLAT-WASM` (S374, Platform path) — when only the WASM CLI missed the last byte of the input file, the digest became `aeb6dc86…` and differed from the local one. Restored: they match
- Gate I, J (P13-6, 2026-10-26): the goals of P11-6 (`G-CORE-BUILD`, `FLAGS`, `C23-SUBSET` (`12 allowed probes ok on 5 toolchains`), `DIAG-REGISTRY` (`103 codes`), `DIAG-APPEND`, `UNIT`, `ARENA-ACCESS`, `NOINT128`, `GOLDEN` (336 files × 4 configurations), `GRAMMAR` (131 rules), `STD-EMBED`, `G-STD-LISTS` (`267 listed`), `STATIC`, `SOURCES`, `PROVENANCE`, `G-REPO-VERSION`, `G-CORE-OOM` (`316 files, 4210 points, 0 failures`), `PURE`, `NOGLOBAL`, `NOFLOAT`, `DETERMINISM`, `G-REPO-SPDX`, `SELFTESTS`, `RED-FIRST`, `TOOLS`, `G-PLAN-COVERAGE`, `G-TYPE-SEVERITY-TABLE`, `G-SCHED-CHECK` (`65 schedules, 0 violations`), `G-CORE-FUZZ` (`parse` `71891 execs in 601 s`, `expand` `34824 execs in 601 s`, `typecheck` `30631 execs in 602 s`, `schedule` `36986 execs in 601 s`, all 0 crashes, 0 leaks, 0 timeouts), `G-API-EXPORTS`, `G-API-HEADER`, `G-API-SCHEMA` (`json: 336 files, 0 invalid`), `G-API-SARIF` (`sarif: 179 files, 0 invalid`), `G-CLI-TESTS`, `G-PLAT-LINUX`, `G-PLAT-WASM`, `G-PLAT-IOS`, `G-PLAT-ANDROID`, `G-PLAT-CI`, `G-REPO-TOOLS-CI` (CI is the run for commit `c2399aa`; the core and CLI have not changed since)) and the goals of P12 and P13 (`G-SAMPLE-CHECK` (`samples: 11 samples, 236 expectations, 0 failed`), `G-SAMPLE-STD-ONLY`, `G-STD-PHRASES` (`219 keys`), `std_copy` of `G-CORE-PROPS` (`110 cases, 0 differences`)) passed. Difference from the plan: `bundle_roundtrip` (P15-2) of `G-CORE-PROPS` and the Fluent license of `G-REND-VENDOR` (phase K) do not pass yet, because they are made in those tasks (treated the same as at gate H). The CI goals read the run for the current HEAD, so CI is checked again after this gate's record is pushed

- The number of properties in each sample's `expect.yaml`
- The list of schedules built with provisional values (S277)
- Record of mutation checks

### 9.10 K (renderer)

- P14-0 (2026-10-26): §4.28.1 records where the renderer lives, its input, names and phrases, recipe text, the schedule table, the Gantt chart, the shopping list, HTML, the book form, and how its golden files are made. We found that the result JSON had no display information (value type, written order, `@group`), and got confirmation as D36. Fetching Fluent (`@fluent/bundle` 0.19.1) was also confirmed. There is no new S
- Supplement to P14-0: as D36 says, `type`, `part` and `group` were added to IR sections, and `ingredients` per dish (in written order; `node` is null for ingredients not yet used) (the expectations of `api/ir-display` and `api/ir-allergens-choices` were committed first). `group` is attached to tasks created within the scope of the annotation (`op`, `choose`)
- Supplement to P14-0 (2): for the vague time words of S391, `notes` (the annotation name and argument strings, `api/plate-notes`) was added to IR plating sections, as display information like that of D36
- P14-1 (2026-10-27): after confirming, `@fluent/bundle` 0.19.1 was brought in (record below), and `G-REND-VENDOR` passed. First 8 `rtest` tests (`fluent-keys`, `phrase-layering`, `phrase-fallback`, `name-by-stage-dialect`, `part-names`, `phrases-check`, `diag-text`, `wash-vs-togu`) were committed, and then we wrote `renderers/lib/phrases.mjs` (keys, layering, fallback from the user's language → English → identifier, and counting of fallbacks, Fluent errors and unresolved variables. `useIsolating: false`) and `renderers/lib/names.mjs` (value names, keys for part combinations and `name-with-part`, action sentences and `$x-kind`, diagnostic messages). Selectors for the yellowtail's stages and regions, `ingredient-buri-collar`, `ingredient-pork-belly`, `name-with-part`, and "togu" (rinsing) for grains were added to the phrases, and the phrases check now allows `-kind` for action arguments. `G-REND-TESTS` (`# pass 13`, `# fail 0`) and `G-STD-PHRASES` passed
  - Mutation check: `rtest/fluent-keys` (S384, Normal) — failed when keys were made camelCase. Restored and passed
  - Mutation check: `rtest/phrase-layering` (S385, Time and order) — failed when the layering order was reversed. Restored and passed
  - Mutation check: `rtest/phrase-fallback` (not in ja, not in en either) (S386, Boundary) — failed when the fallback to English was skipped. Restored and passed
  - Mutation check: `rtest/name-by-stage-dialect` (S387, Normal) — failed when `$dialect` was not passed. Restored and passed
  - Mutation check: `rtest/part-names` (S388, Normal) — failed when the specific keys were not used. Restored and passed
  - Mutation check: `G-STD-PHRASES` (`unknown args`), `rtest/phrases-check` (S389, No-effect condition) — failed when the argument matching was removed. Restored and passed
  - Mutation check: `rtest/diag-text` (S446, Normal) — failed when diagnostic messages were made a fixed English sentence. Restored and passed
  - Mutation check: `rtest/wash-vs-togu` (S447, Normal) — failed when "arau" (wash) was also used for rice. Restored and passed
- P14-2 (2026-10-27): first 7 `rtest` tests (`recipe-text` (`renderers/test/expected/recipe-text.{ja,en}.txt`), `merge-sentences`, `group-annotation`, `counters`, `vague-time-words`, `recipe-wave-sections`, `ingredient-order`) and the input `renderers/test/inputs/small.ozen` were committed, and then we wrote `renderers/lib/recipe.mjs` (the ingredient list and the steps in IR dependency order. Tasks done in a row are merged into one sentence with the phrase attribute `.next`, `@group` becomes a headed block, if there are 2 or more waves they are divided by wave name, and plating notes are given in words). Headings, enum values, counters, notes, wave names, the continuing form and `action-assemble` were added to the phrases, and dish name phrases (`samples/curryRice/phrases/`) were added to the sample `curryRice`. The `type` of IR sections for values made with `make` was set to the recipe's output type (the expectations of `api/plate-notes` and `api/ir-allergens-choices` were fixed first). A change that removes `@group` and the golden `ir/group-display-only` were added to the property test `display_only` (both pass before the change, so they are red-first exceptions). `G-REND-TESTS` (`# pass 21`) passed
  - Mutation check: `rtest/recipe-text` (S375, Normal) — failed when steps were put in written order (the JSON has no written-order information, so the reverse order was used). Restored and passed
  - Mutation check: `rtest/merge-sentences` (S377, Normal) — failed when sentences were not merged. Restored and passed
  - Mutation check: `rtest/group-annotation`, `G-CORE-PROPS` (`prop/display_only`) (S378, Side effects) — failed when `@group` was put in the IR (`ir/group-display-only` and `prop/display_only` (`1 differences`) also failed). Restored and passed
  - Mutation check: `rtest/counters` (S390, Normal) — failed when counters were not added. Restored and passed
  - Mutation check: `rtest/vague-time-words` (S391, Normal) — failed when the range numbers were output. Restored and passed
  - Mutation check: `rtest/recipe-wave-sections` (S445, Normal) — failed when the text was not divided by wave name. Restored and passed
  - Mutation check: `rtest/ingredient-order` (S450, Time and order) — failed when ingredients were sorted by name. Restored and passed
- P14-3 (2026-10-27): first 8 `rtest` tests (`schedule-text`, `waves-chapters` (input `renderers/test/inputs/waves.ozen`), `prep-cook-split`, `gantt-svg`, `shopping-list`, `variant-wash-assumption-text`, `book-from-index`, `print-css`) were committed, and then we wrote in `renderers/lib/` the schedule table (`schedule.mjs`. It looks up the IR section by the task number in the schedule JSON to make the step sentence, and outputs wave chapters, the during-meal column, prep and cooking, cleanup, and notes), the Gantt chart (`gantt.mjs`), the shopping list (`shopping.mjs`), HTML (`html.mjs`), the book form (`book.mjs`) and the entry (`render.mjs`). Equipment names and schedule table parts were added to the phrases, and dish name phrases to every sample. `tests/samples/check.py --save` writes the JSON of each run to `build/results/json/samples/`, and `renderers/tools/render-all.mjs` renders it in `ja` and `en` and writes to `build/results/html/`. We added the HTML checks `tools/htmlcheck/htmlcheck.py` (well-formedness, Gantt chart overlaps and bounds, form names and units) and `selfcontained.py` (external references), with the self-test `scripts/test-htmlcheck.sh`. Fallbacks of phrases to English are now also counted as "fallbacks" (in the mutation check of S451, removing only the Japanese fell back to English and was not counted. The expectation of `phrase-fallback` was fixed first). `G-REND-TESTS` (`# pass 30`), `G-REND-PHRASES-RENDER` (`26 samples × 2 languages, 0 fallbacks, 0 fluent errors, 0 unresolved`), `G-REND-SELFCONTAINED` (`52 files, 0 external references`) and `G-REND-HTML` (`52 files, 0 not well-formed; gantt: 42 charts, 0 overlaps, 0 out of bounds`) passed
  - Mutation check: `rtest/schedule-text` (S376, Normal) — failed when the cook was not output. Restored and passed
  - Mutation check: `rtest/waves-chapters` (S379, Normal) — failed when the during-meal column was not output. Restored and passed
  - Mutation check: `rtest/prep-cook-split` (S380, Boundary) — failed when the split was put at a fixed time. Restored and passed
  - Mutation check: `G-REND-SELFCONTAINED` (S381, No-effect condition) — failed when CSS was loaded from a CDN (`selfcontained: 52 files, 52 external references`). Restored and passed
  - Mutation check: `rtest/gantt-svg` (matching row and task positions) (S382, Normal) — failed when time positions were shifted by 1 minute. Restored and passed
  - Mutation check: `rtest/shopping-list` (S383, Normal) — failed when `buy` items were dropped. Restored and passed
  - Mutation check: `rtest/variant-wash-assumption-text` (S392, Normal) — failed when the reason for washing was not output. Restored and passed
  - Mutation check: `rtest/book-from-index` (S448, Normal) — failed when no table of contents was made. Restored and passed
  - Mutation check: `rtest/print-css` (S449, Normal) — failed when `@media print` was removed. Restored and passed
  - Mutation check: `G-REND-PHRASES-RENDER` (S451, Data completeness) — failed when one entry was removed from `ja.ftl` (`render-phrases: 26 samples × 2 languages, 394 fallbacks` (at first fallbacks to English were not counted and it was not detected, so it was detected after fixing the counting)). Restored and passed
  - Mutation check: `G-REND-HTML` (gantt) (S452, Boundary) — failed when tasks on the same row were drawn overlapping (`560 overlaps`). Restored and passed
  - Mutation check: `G-REND-HTML` (html), `sh/test-htmlcheck.sh` (S454, Data completeness) — failed when an unclosed `<br>` was output (`52 not well-formed`). Restored and passed
- P14-4 (2026-10-27): first 2 `rtest` tests (`settings-form-roundtrip`, `settings-form-new-field`) were committed, and then we wrote `renderers/lib/settings.mjs`. From the equipment, vessel and option items in the type definition JSON (`--emit types`) and the enum values in the list of all definitions, it decides the input form for each item (`Count` is a number; a quantity is a number and a unit (the unit of the default or range, otherwise the display unit for the type: `g`, `ml`, `cm`, `min`, `degC`); an enum is a select; a list of enums is checkboxes; anything else stays as Ozen text), and makes a self-contained HTML form. The functions that make the kitchen text (`kitchen home { places/tools/tableware }`; equipment with a count of 0 is not written) and the options text (`kitchen today : Options { … }`; values equal to the default are not written) from the form values are also embedded in the form, and the text is rewritten on every input (confirmed to work in a browser). Settings screen headings, items (`field-*`), choices (`value-*`) and vessel names were added to the phrases. `types` was added to the sample JSON, and `render-all.mjs` also writes the settings screen `settings.<言語>.html`. `G-REND-TESTS` (`# pass 32`), `G-REND-HTML` (`54 files, … form: 144 inputs, 0 without label, 0 quantity without unit`), `G-REND-PHRASES-RENDER` (`0 fallbacks`), `G-REND-SELFCONTAINED`, `G-SAMPLE-CHECK` and `G-STD-PHRASES` passed
  - Mutation check: `rtest/settings-form-roundtrip` (the text made from the form values is run through the CLI's `check`) (S393, Real path) — failed when values were written out without units (even with the text comparison removed, the CLI's `check` emits `TYP001` and it fails). Restored and passed
  - Mutation check: `rtest/settings-form-new-field` (S394, Normal) — failed when items were listed by hand. Restored and passed
  - Mutation check: `G-REND-HTML` (form) (S453, Data completeness) — failed when units of quantity items were not output (`36 quantity without unit`). Restored and passed
- P14-5 (2026-10-27): first the CLI test `cli/render` (`tests/cli/render.sh`. Own project phrases in the HTML title, `std` phrases in the body, `--kind recipe -o`, the `--kind settings` form, and exit code 2 for an unknown `--kind` and a missing renderer) and the change to the `subcommands` expectation (removing `render` from "not usable yet") were committed, and then we wrote `ozen render [OPTIONS] [ENTRY|FILE]` (`--kind recipe|schedule|shopping|html|settings`, `--lang`, `--dialect`, `-o/--output`). The CLI makes the result JSON (`ir,schedule,steps,definitions,types`) with the same entry resolution as `check`, and passes it to the standard input of `node renderers/cli.mjs` (D21). The renderer is the environment variable `OZEN_RENDERER`; otherwise it is searched for from the executable's real location (`build/<構成>/ozen`) at `../../renderers/cli.mjs`, then `../renderers/cli.mjs`; the `std` phrases are in `phrases/` next to it, and the own project's phrases are in `phrases/` of the current folder. node is the environment variable `OZEN_NODE`, otherwise `node`. `settings` was added to `kind` in `render.mjs`. `renderers/tools/bundle.mjs` bundles the main code and Fluent into one plain script, each wrapped in a function in dependency order (it defines `globalThis.OzenRenderer`; the default output is `build/renderer/ozen-renderer.js`), and non-relative imports throw an exception when loaded (import cycles are an error). `renderers/tools/browser-test.sh` passes the sample result JSON (`build/results/json/samples/`) with the phrases to headless Chrome, and compares the results of rendering 2 languages × 5 kinds with the bundled file against the results of reading `renderers/lib/` directly in node (the shared steps are in `browser-cases.mjs`). The self-test `scripts/test-browser-test.sh` (5 cases: same, a bundle using node APIs, a bundle with changed output, 0 inputs) was registered in `scripts/selftests.txt`. `G-CLI-TESTS` (`16 cases, 0 failed`), `G-REND-BROWSER-SAFE` (0 node references, `browser: 26 inputs, 0 mismatches`), `G-CORE-BUILD`, `G-CORE-FLAGS` and `G-CORE-C23-SUBSET` passed
  - Mutation check: `cli/render` (S395, Real path) — failed when `render` was removed (removing the function made the build fail because it was unused, and the old executable was measured, so we broke it again by changing the subcommand name matching: `html status: got '2'`). Restored and passed
  - Mutation check: `G-REND-BROWSER-SAFE` (S396, Platform path) — failed when the main code loaded `fs` (adding `import { readFileSync } from 'node:fs'` to `xml.mjs` gave both node references `1` and `browser: 26 inputs, 26 mismatches`). Restored and passed
- Gate K (P14-6, 2026-10-27): the goals of P13-6 (`G-CORE-BUILD`, `FLAGS`, `C23-SUBSET` (`12 allowed probes ok on 5 toolchains`), `DIAG-REGISTRY` (`103 codes`), `DIAG-APPEND`, `UNIT` (24 tests × 4 configurations), `ARENA-ACCESS`, `NOINT128`, `GOLDEN` (339 files × 4 configurations), `GRAMMAR` (131 rules), `STD-EMBED`, `G-STD-LISTS` (`267 listed`), `STATIC`, `SOURCES`, `PROVENANCE`, `G-REPO-VERSION`, `G-CORE-OOM` (`319 files, 4256 points, 0 failures`), `PURE`, `NOGLOBAL`, `NOFLOAT`, `DETERMINISM`, `G-REPO-SPDX`, `SELFTESTS`, `RED-FIRST`, `TOOLS`, `G-PLAN-COVERAGE`, `G-TYPE-SEVERITY-TABLE`, `G-SCHED-CHECK` (`65 schedules, 0 violations`), `G-CORE-FUZZ` (`parse` `71130 execs in 601 s`, `expand` `32306 execs in 601 s`, `typecheck` `27292 execs in 606 s`, `schedule` `35826 execs in 601 s`, all 0 crashes, 0 leaks, 0 timeouts), `G-API-EXPORTS`, `G-API-HEADER`, `G-API-SCHEMA` (`json: 339 files, 0 invalid`), `G-API-SARIF` (`sarif: 181 files, 0 invalid`), `G-CLI-TESTS` (`16 cases`), `G-PLAT-LINUX`, `G-PLAT-WASM`, `G-SAMPLE-CHECK` (`11 samples, 236 expectations, 0 failed`), `G-SAMPLE-STD-ONLY`, `G-STD-PHRASES` (`219 keys`), `std_copy` of `G-CORE-PROPS` (`111 cases, 0 differences`), `G-REPO-NOREPLY`) and the goals of P14 (`G-REND-TESTS` (`# pass 32`), `G-REND-VENDOR`, `G-REND-PHRASES-RENDER` (`26 samples × 2 languages, 0 fallbacks, 0 fluent errors, 0 unresolved`), `G-REND-SELFCONTAINED` (`54 files, 0 external references`), `G-REND-HTML` (`54 files, … 42 charts, 0 overlaps, 0 out of bounds; form: 144 inputs, 0 without label, 0 quantity without unit`), `G-REND-BROWSER-SAFE` (`browser: 26 inputs, 0 mismatches`)) passed. `G-PLAT-WASM` failed at first because `build/core-wasm` was stale (the goal does not rebuild wasm), so it was rebuilt with `scripts/build-wasm.sh` and passed (the `popen` of the CLI's `ozen render` also built with emcc). Difference from the plan: `bundle_roundtrip` of `G-CORE-PROPS` is made in P15-2, so it does not pass yet (treated the same as at gates H to J). `G-PLAT-IOS`, `G-PLAT-ANDROID`, `G-PLAT-CI` and `G-REPO-TOOLS-CI` were checked after the push and passed (CI is run 40465944412 for commit `e9e1d08`, all 5 jobs succeeded)
- The version, source and sha256 of `@fluent/bundle` (P14-1): 0.19.1 (Apache-2.0). The 10 files in `package/esm/` and `package.json` from `https://registry.npmjs.org/@fluent/bundle/-/bundle-0.19.1.tgz` (33,539 bytes, SHA-1 `d9931a0b…`). The archive has no license file, so `renderers/vendor/fluent-bundle/LICENSE` is the text of the repository's `LICENSE-APACHE`. The sha256 of each file is in `renderers/vendor/README.md`
- Record of mutation checks

### 9.11 L (bundle)

- P15-0 (2026-10-27): we looked at how the core currently handles modules (inline `module` works up to parsing and name registration, and names are as written. `bundle` and `phrases` are parse only. `mod_of_file` in `sema.c`, `module_of` in `defs.c` and `dependency_file` in `levels.c` assume one module per file, and annotation collection is doubled for inline modules). We found that S399 and S401 conflict, and got confirmation as D37. §4.29.1 records the form, contents, phrases, standalone build, import, unbundling and CLI, and S477 and S478 were added with rows in the coverage table
- P15-1 (2026-10-27): first 13 golden files were committed (`bundle/inline-modules`, `header`, `header-license-from`, `contents`, `phrases-included`, `dep-partial`, `order-by-name`, `local-closed` (these are `ozen bundle` output) and `build-standalone`, `std-module-rejected`, `std-version-check`, `annotation-scope`, `relative-import` (standalone builds and checks of bundled files)). While writing `annotation-scope`, we found a bug where `@expect` inside an inline module wrongly became `NAM018` (annotations were collected in both the file's module and the inner module). In a standalone build, the `entry` of `bundle` becomes the entry and the information that comes with an entry (`ASM002`, `ASM004`) appears, so 4 expectations were fixed before the implementation (`std-module-rejected` already had `PKG005`, and at first it passed before the fix, so it was recorded as an exception). The core now resolves names and annotations in the innermost module enclosing the node (`mod_at_node` in `sema.c`, annotation collection and dependency checks in `levels.c`), makes the `from` of a bundled file a provisional package that the inline modules belong to (S477), checks the `std` version with the same rules as `requires` (S404), and uses `entry` as the entry in a standalone build. `ozni_bundle_text` in `core/src/bundle.c` follows names in definition bodies from the entry by matching them to candidate definitions (`ozni_project_candidates`), adds `extend` and `check`, copies the original text (including the comment just before), and rewrites `import` with only what is included. The CLI's `ozen bundle [ENTRY|DIR] [-o FILE]` reads the phrase files (`phrases/*.ftl`, `deps/*/phrases/*.ftl`) and passes them. Only dishes reachable from the entry are evaluated (the same as the public API; it does not refuse because of errors in dishes unrelated to the entry). For all samples (11), the IR of building the bundled file standalone matched the IR of the same entry in the original project. The change to the `subcommands` expectation passes both before and after, so it was recorded as an exception. `G-CORE-GOLDEN` (352 files × 4 configurations), `G-BUNDLE-STABLE`, `G-CLI-TESTS`, `G-CORE-BUILD`, `FLAGS`, `UNIT`, `PURE`, `NOGLOBAL`, `NOFLOAT` and `C23-SUBSET` passed (in the sanitizer configuration, passing null to `qsort` when there were 0 phrases was fixed)
  - Mutation check: `cgolden/bundle/inline-modules` (S397, Normal) — failed when namespaces were removed. Restored and passed
  - Mutation check: `cgolden/bundle/header`, `cgolden/bundle/header-license-from` (S398, Normal) — failed when the `std` version was not written or `license` was not carried over (for not writing the `std` version, the break did not build because of an unused warning, so we broke it again by keeping the code used but making it have no effect. Not carrying over `license` made `header-license-from` fail). Restored and passed
  - Mutation check: `cgolden/bundle/contents` (the kitchen, `std` and unused definitions are not included), `cgolden/bundle/phrases-included`, `cgolden/bundle/dep-partial` (S399, Data completeness) — failed when unused definitions were also included or phrases were not included (the break did not build because of an unused warning, so we broke it again by keeping the code used but making it have no effect). Restored and passed
  - Mutation check: `G-BUNDLE-STABLE` (`cgolden/bundle/order-by-name`; modules whose creation order is the reverse of name order) (S400, Side effects) — failed when the order was made the load order. Restored and passed
  - Mutation check: `cgolden/bundle/build-standalone` (S402, Normal) — failed when modules inside the bundled file were ignored. Restored and passed
  - Mutation check: `cgolden/bundle/std-module-rejected` (S403, No-effect condition) — failed when the check was removed. Restored and passed
  - Mutation check: `cgolden/bundle/std-version-check` (S404, Boundary) — failed when versions were not compared (the break did not build because of an unused warning, so we broke it again by keeping the code used but making it have no effect). Restored and passed
  - Mutation check: `cgolden/bundle/annotation-scope` (S405, Boundary) — failed when annotations took effect on the whole bundled file (broken by also applying annotations to inner modules on top). Restored and passed
  - Mutation check: `cgolden/bundle/local-closed` (S406, No-effect condition) — failed when bundling removed `local`. Restored and passed
  - Mutation check: `cgolden/bundle/relative-import` (S477, Normal) — failed when modules inside the bundled file were not made to belong to the package (the break did not build because of an unused warning, so we broke it again by keeping the code used but making it have no effect). Restored and passed
- P15-2 (2026-10-27): first we committed the golden `bundle/import-two-versions` (P15-1 had already added the comparison between the `from` version and the `deps/` version, so it passed from the start and was recorded as an exception), the CLI tests `cli/unbundle-no-overwrite` and `cli/unbundle-phrases`, the change to the `subcommands` expectation (passes both before and after, so recorded as an exception), and the property test `prop/bundle_roundtrip` (bundles and unbundles the projects of the 13 sample entries and the 8 `ozen bundle` output golden files, and checks that the declarations of the unbundled files (the AST of top-level items other than `import`) are a subset of the declarations of the original files, and that the IR of the same entry is the same as the original). The core's `ozni_unbundle_text` parses only the bundled file, writes the modules of the first package in `from` to `<道筋>.ozen` and the others to `deps/<パッケージ>/<道筋>.ozen`, creates `package.ozen` (with `std` and dependency versions in `requires`) and the dependencies' `package.ozen`, and splits `phrases` back out by the `### <パッケージ>` lines. The CLI's `ozen unbundle FILE [-o DIR]` checks every destination first, and if even one exists it writes nothing and exits with code 2. Dependency folder names become package names (a bundled file does not keep folder names; `deps/aaa` of `order-by-name` comes back as `deps/zeta`), so the property test's matching was changed to look up the original dependency folder from the package name. `bundle_roundtrip` of `G-CORE-PROPS` (`21 cases, 0 differences`), `G-CORE-GOLDEN` (353 files × 4 configurations) and `G-CLI-TESTS` (`18 cases`, 4 configurations) passed
  - Mutation check: `G-CORE-PROPS` (`prop/bundle_roundtrip`; matching the declarations of the unbundled files against the original source, and matching IR) (S401, Normal) — failed when `unbundle` changed module names or removed `local` (changing `.` in paths to `_` gave `6 differences`, removing `local` gave `1 differences`). Restored and passed
  - Mutation check: `cgolden/bundle/import-two-versions` (S407, No-effect condition) — failed when the check was removed. Restored and passed
  - Mutation check: `cli/unbundle-no-overwrite` (S444, Persistence) — failed when it overwrote (`overwrite status: got '0'`). Restored and passed
  - Mutation check: `cli/unbundle-phrases` (S478, Normal) — failed when phrases were put back all together into the original package (`deps/dashi/phrases/ja.ftl differs`). Restored and passed
- P15-3 (2026-10-27): first the golden files `limits-shared/bundle-exact` and `bundle-over` (bundled file size exactly at and one over the limit, 207 bytes), and `kitchen-exact` and `kitchen-over` (the `-k` kitchen spec exactly at and one over, 116 bytes) were committed. The input size limit already applied to all user sources including the kitchen spec (`<kitchen>`), so all 4 passed from the start and were recorded as exceptions. We added the fuzz targets `fuzz_bundle` (passes the input as a shared file through `check`, `unbundle` and `bundle`) and `fuzz_kitchen` (passes the input as a kitchen spec to a fixed menu and builds up to the schedule), and made the default targets of `scripts/fuzz-core.sh` 6 (the CI fuzz job also runs the 6 for 60 seconds each). `G-CORE-GOLDEN` (357 files) passed. The 600-second fuzzing runs at gate L
  - Mutation check: S408 (not applying the input size limit to the kitchen argument) made `limits-shared/kitchen-over` fail. Restored and passed
- Gate L (P15-4, 2026-10-27): the goals of P14-6 (`G-CORE-BUILD`, `FLAGS`, `C23-SUBSET` (`12 allowed probes ok on 5 toolchains`), `DIAG-REGISTRY` (`103 codes`), `DIAG-APPEND`, `UNIT` (24 tests × 4 configurations), `ARENA-ACCESS`, `NOINT128`, `GOLDEN` (357 files × 4 configurations), `GRAMMAR` (131 rules), `STD-EMBED`, `G-STD-LISTS` (`267 listed`), `STATIC`, `SOURCES`, `PROVENANCE`, `G-REPO-VERSION`, `G-CORE-OOM` (`326 files, 4356 points, 0 failures`), `PURE`, `NOGLOBAL`, `NOFLOAT`, `DETERMINISM`, `G-REPO-SPDX`, `SELFTESTS`, `RED-FIRST`, `TOOLS`, `G-PLAN-COVERAGE`, `G-TYPE-SEVERITY-TABLE`, `G-SCHED-CHECK` (`65 schedules, 0 violations`), `G-API-EXPORTS`, `G-API-HEADER`, `G-API-SCHEMA` (`json: 355 files, 0 invalid`), `G-API-SARIF` (`sarif: 190 files, 0 invalid`), `G-CLI-TESTS` (`18 cases`), `G-PLAT-LINUX`, `G-PLAT-WASM` (after rebuilding wasm), `G-SAMPLE-CHECK` (`11 samples, 236 expectations, 0 failed`), `G-SAMPLE-STD-ONLY`, `G-STD-PHRASES`, `G-REND-TESTS`, `G-REND-VENDOR`, `G-REND-PHRASES-RENDER`, `G-REND-SELFCONTAINED`, `G-REND-HTML`, `G-REND-BROWSER-SAFE` (`browser: 26 inputs, 0 mismatches`), `G-REPO-NOREPLY`) and the goals of P15 (`G-BUNDLE-STABLE`, `G-CORE-PROPS` (`reindent` 1608, `split_merge` 113, `meta_ignored` 5, `monotonic` 64, `display_only` 46, `ir_invariant` 1107, `std_copy` 116, `bundle_roundtrip` 21 cases, all 0 differences / 0 decreases), `G-CORE-FUZZ` (`parse` `68046 execs in 601 s`, `expand` `32067`, `typecheck` `25535`, `schedule` `35175`, `bundle` `22663`, `kitchen` `41550 execs in 601 s`, all 0 crashes, 0 leaks, 0 timeouts)) passed. With this, every item of `G-CORE-PROPS` passed (including `bundle_roundtrip`, which had remained through gates H to K). `G-PLAT-IOS`, `G-PLAT-ANDROID`, `G-PLAT-CI` and `G-REPO-TOOLS-CI` are checked after the push
- Record of mutation checks

---

## 10. Risks

- **R1. Filling in too many rules drifts from the concept's intent**: the table check scripts and the details of each phase fill in rules that the concept does not have. Filled-in rules are marked (added) and counted (§8), and those where judgment could differ get confirmation as a D in `P<n>-0`
- **R2. New inconsistencies in the concept are found during implementation**: record them in §9 and report to the user; until decided, the item is treated as provisional
- **R3. Introducing Emscripten is large, or fails**: fall back to the alternative of D18 (wasi-sdk and `node:wasi`) and write the reason in §9.3
- **R4. GitHub Actions concurrency limits and run time** (D19 made the repository public, so there is no cost, but the Free plan allows 5 concurrent macOS jobs, and a job runs at most 6 hours): split the jobs and record the run times in §9.8. If the limits stop things from running, ask the user whether to change how the jobs are split
- **R21. Making the repository public publishes the concept, plan and code as of creation**: in P5-3, confirm before creating it and before the first push. The source PDFs and the original Excel of the food composition table are not put in the repository (S467, `.gitignore`)
- **R5. Implicit type conversion in YAML**: positions where the schema requires a string are set to `type: string`, and the fixtures (S12) confirm detection
- **R6. Errors in our own 128-bit arithmetic**: detected by the `Fraction` oracle (including values near 2⁶²) and boundary unit tests. When brought into the core, the tests move with it (D12)
- **R7. Problems not found in the fuzzing time remain**: the report states the time and the number of runs, and distinguishes "not found" from "does not exist"
- **R8. Errors in hand calculation of fuel and schedules**: both exactly K and K−1 are golden files, and schedules also go through a checker on a separate path, so if they drift one of them fails. When one fails, review the spec and the hand calculation, and do not change the expectation to match the implementation (change it only when it is known to be a hand calculation error, in a separate commit)
- **R9. clang 17 comes with Swift from swiftly, and its version may change when Swift is updated**: `G-REPO-TOOLS` and `G-PROTO-COMPILERS` fail on a version difference
- **R10. The format of the food composition table Excel differs from what is expected**: check the columns in P3-1, and put the same shape in the small xlsx of `test_extract`
- **R12. Exceptions to the "write expectations first" check (`G-REPO-RED-FIRST`) grow and the check becomes a formality**: exceptions are limited to fixes of hand calculation errors and tests added later in P16-3, each with a reason on its own line in §9
- **R13. Headless Chrome touches the user's profile**: pass a temporary folder to `--user-data-dir` and remove it at the end. Serve only on `127.0.0.1`
- **R14. The check machinery grows, and errors in it make the goals a formality**: every check gets a self-test (`G-REPO-SELFTESTS`), and the judgment commands in the §5 code blocks are authoritative
- **R15. The plan is large (408 S items, about 80 goals), and the coverage table breaks each time details are added**: run `G-PLAN-COVERAGE` and `G-PLAN-TESTS-EXIST` in each phase's `P<n>-0` and in P16-3, and do not move to the next task if it breaks
- **R16. The C compilers of the 4 environments differ in details of C23 handling**: limit the features used to 12 (D13) and compile the probes on each toolchain (`G-CORE-C23-SUBSET`). Results are compared by digest
- **R17. Fetching the Android SDK, NDK and system images is large** (about 780 MB for the NDK alone): confirm before fetching, and make CI the main place (D19)
- **R20. The amount of primary source research is unpredictable** (D7): in P3-5, research per kind of value, and record the range researched and why values were not found. Values not found go in with provisional ranges so work can move on; if the amount turns out too large partway, ask the user whether to leave the rest provisional
- **R18. After converting the tables (A) to `std`, A's checks are gone and errors in `std` are missed**: of the checks being retired, those also needed for `std` move to `tools/stdcheck/` and core diagnostics (D28, S421, S458 to S462)
- **R19. Every time CI results are read, a push needs confirmation, which adds work**: pushes are limited to tasks that read CI results (the D19 list in §2), and each time the range is shown and confirmation obtained
