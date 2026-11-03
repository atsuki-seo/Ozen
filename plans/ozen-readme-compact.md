# Plan to shorten the README and move its explanations into per-folder documents

- Created: 2026-11-01
- Reference: none (no code reference). The structure follows the usual README of languages and libraries (rust-lang/rust, golang/go, gleam, serde, tokio and others: limited to a one-paragraph description, a short example, the build, links to documents and the license, with detailed explanations in separate files)
- Implemented in: `/home/atsuki/ドキュメント/Ozen`, `main` (D60). The starting point is `7e3e8a5` (up to the record of the last mutation check of the syntax refresh plan)
- Rules assumed: `CLAUDE.md` (work only on `main`, run `G-REPO-NOREPLY` before each push, `plans/` in Japanese, other committed files in English, `docs/` as Japanese design documents), `~/.claude/rules/plan-language.md`, `~/.claude/rules/repo-language.md`, `~/.claude/rules/git-commit.md`. The §5 goals of the existing plans `plans/ozen-roadmap.md` (below, "the roadmap") and `plans/ozen-syntax-refresh.md` (below, "the syntax plan") are cited as prerequisite goals (§5)

## 0. How to read this plan

- This plan covers work that follows the syntax plan and continues its numbering. Design decisions start at **D57** and spec items at **S637** (the syntax plan ended at D56 and S636).
- Every task's completion condition is written **so that a command decides pass or fail**. Checks by a person are not tasks; they go to the hand-over list in §8.
- Completion conditions refer to the prerequisite goals listed in §5 and to the `G-RD-*` goals defined in §5 of this plan. The code blocks in §5 are authoritative for the judgment commands. They are not written in tables (inside a table, `|` becomes `\|`).
- In §2, D57 to D59 were decided by the user on 2026-11-01, before the plan was written. D60 to D66 were decided on 2026-11-01, after the plan was written, by asking the user one at a time (all 7 asked individually; D64 chose the alternative). D67 was decided on the same day, before P0-2, on how to handle a section that other work added to the README. D68 was decided on the same day, in the middle of P3-2, on how to handle other work's commits entering the range at the final gate. Each section starts with a "Decision:" line, and the comparison of Recommendation and Alternative is kept as history (P0-1).
- The facts about the target were confirmed on 2026-11-01 by reading the files at `7e3e8a5` and running `build/core-gcc-release/ozen` (§3). After writing, the plan went through one audit by an independent subagent and its findings were taken in (§9).
- The checkable claims in §4 carry spec item IDs. The coverage table in §5.3 shows, for each S item, a test or a hand-over to §8 (with a reason).

---

## 1. Purpose and scope

**Purpose**: Cut the README (now 264 lines, 10 sections) down to a description, one example, the build, links to documents and the license, in line with the usual README of languages and libraries. The explanations removed from the README are not deleted; they move to the README (in English) of the folder each one covers and to `CONTRIBUTING.md`, and the README only links to them. The links to the design document (`docs/concept.md`) and the plan (`plans/ozen-roadmap.md`), and the sentences introducing them, are removed from the README.

The shape of the README after shortening (the example and the line counts are rough):

~~~~
# Ozen

Ozen is a language and compiler for cooking procedures. ...（2〜3 文）

<!-- file: soup.ozen -->
```ozen
recipe soup(servings: 2) { ... onion |> peel |> cut(slice, 3 mm) ... }
```

<!-- run: ozen render --kind recipe --lang en soup.ozen -->
```text
# soup
## Ingredients ...
## Steps ...
```

## Build          … scripts/build-core.sh --only core-gcc-release、toolchain.txt へのリンク
## Documentation  … 8 つの文書と紹介ページへのリンク
## License        … ライセンスの表と出力の権利の一文、NOTICE へのリンク
~~~~

### In scope

- Rewriting the README (D57, D59, D62 to D65)
- Creating 8 target documents: `cli/README.md`, `core/README.md`, `stdlib/README.md`, `renderers/README.md`, `bindings/README.md`, `spec/README.md`, `site/README.md`, `CONTRIBUTING.md` (D58, D67)
- Keeping the moved examples checked in their new places by the README example check (`scripts/check-readme-example.sh`)
- Adding `G-RD-*` to `scripts/goals.sh`, and adding this plan to the Plan section of `CLAUDE.md` (D61)

### Out of scope (reasons)

- **Rewriting `docs/concept.md` and `docs/prototype-report.md`**: the request is only to tidy the README. `docs/` holds Japanese design documents and is not a place for English explanations for users (D58).
- **Rewriting the roadmap and the syntax plan**: both stay as records of finished work. The existing goals that bind the README (`G-REPO-README-LICENSE`, `G-RF-README-EXAMPLE`, `G-RF-README-PIPE`, `G-RF-NO-EMPTY-PARENS`, `G-RF-README-NO-PROTO`) are met as they are. Checks of the target documents are added new as `G-RD-*`.
- **Rewriting `NOTICE` or the license texts**: even if the Credits section is removed, the same content is already in `NOTICE` (§3.3).
- **Changing the README example check script**: `--readme` takes any file, so the target documents can be checked as they are (§3.2).
- **Changing code, tests, expectations or samples**: this is documentation-only work (S682).
- **Checking whether the writing gets across and reads naturally, and whether the layout on the GitHub page is easy to read**: handed over in §8 (the number of headings, hidden annotations, and the resolution of folder READMEs are checked by machine through the GitHub API).

---

## 2. Design decisions

### D57. Code example kept in the README [decided]

- **Decision:** Keep one soup recipe (using pipes) and the recipe text produced by `ozen render` in the README. Move the schedule example and the bundle example to `cli/README.md`, and the rendering explanation to `renderers/README.md`. (2026-11-01, confirmed by the user)
- **Recommendation**: As above. About 30 lines, and readers see at a glance that "a program turns into recipe text".
- Alternative: Keep the current schedule example (soup, kitchen, `ozen debug schedule`). It shows features unique to Ozen, but it needs the kitchen definition, so it grows to about 60 lines.
- Alternative: Only the recipe and the result of `ozen check`. Shortest, but the output is the single line `result ok …` and does not show what Ozen can do.

### D58. Where the explanations move [decided]

- **Decision:** Move them, in English, to the README of the folder each explanation covers (`cli/`, `core/`, `stdlib/`, `renderers/`, `bindings/`, `spec/`) and to `CONTRIBUTING.md` at the repository root. (2026-11-01, confirmed by the user)
- **Recommendation**: As above. Opening the folder on GitHub shows its README, so the explanation sits next to the code. These are English files, so the language rules in `CLAUDE.md` need no change.
- Alternative: Gather them into 2 files, `GUIDE.md` and `CONTRIBUTING.md`. Fewer files, but one of them gets long and the gain from shortening the README shrinks.
- Alternative: Put them in `docs/en/`. That needs a rewrite of the `CLAUDE.md` rule "`docs/` holds Japanese design documents".

### D59. Mention of Ozendate [decided]

- **Decision:** Remove it from the README. (2026-11-01, confirmed by the user)
- **Recommendation**: Remove it. Ozendate is an app not in this repository, and README readers do not need it.
- Alternative: Keep just one sentence in the description paragraph.

### D60. Place of work and base of the range [decided]

- **Decision:** As recommended, work on `main` and count the diff from the tag `readme-start` set in P0-2. (2026-11-01, confirmed by the user)
- **Recommendation**: Work on `main` (`CLAUDE.md`). In P0-2, set the tag `readme-start` on `main` and count this plan's diff as the range `RD=readme-start..HEAD`. The range is used by `G-RD-SCOPE`, which checks that only documents changed. Push follows the user's instruction in memory (in Ozen, no need to ask before pushing), so pushes happen without asking each time. `G-REPO-NOREPLY` runs before every push.
- Alternative: Do not set a tag and write the starting commit `7e3e8a5` directly. One tag fewer, but it does not match how the roadmap and the syntax plan do it (`plan-start`, `refresh-start`).

### D61. Where the plan file and the goals live [decided]

- **Decision:** As recommended, add this file as a new plan, add `G-RD-*` to `scripts/goals.sh`, and add one line to the Plan section of `CLAUDE.md`. The roadmap and the syntax plan are not rewritten. (2026-11-01, confirmed by the user)
- **Recommendation**: Add this file as a new plan and do not rewrite the roadmap or the syntax plan. Add `PLAN_RD`, `RD`, `DOCS_RD` and `goal_G_RD_*` to `scripts/goals.sh`. Add one line to the Plan section of `CLAUDE.md` with this plan's name, the goal prefix and the range tag (in the same form as the syntax plan's line).
- Alternative: Append it to the syntax plan as Phase 10. The goals stay in one place, but a finished plan and ongoing work can no longer be told apart.

### D62. The Credits section [decided]

- **Decision:** As recommended, remove the Credits section and link to `NOTICE` from the License section. (2026-11-01, confirmed by the user)
- **Recommendation**: Remove it. Link to `NOTICE` from the License section. The sources of the food composition table, Fluent and the SARIF schema have the same content in `NOTICE` (§3.3), so there is no reason to keep them twice with the README. The source wording of the food composition table (in Japanese) also stays in `NOTICE` and `data/mext-fct-2023/README.md`, and `G-REPO-NOTICE` keeps checking it.
- Alternative: Keep the 3 lines. The sources are visible just by opening the README, but they may drift from `NOTICE`, and Japanese characters stay in the README.

### D63. The License section [decided]

- **Decision:** As recommended, keep the license table and the sentence on output rights, and shorten the explanation of the 2 licenses to 2 or 3 sentences that include links to each license file and `NOTICE`. (2026-11-01, confirmed by the user)
- **Recommendation**: Keep the license table and the sentence "Recipes you write in Ozen …", and shorten the 2 paragraphs after them (the explanation of the 2 licenses) to 2 or 3 sentences that include links to each license file and `NOTICE`. `G-REPO-README-LICENSE` (roadmap S6) requires the table and the sentence in the README, so keeping them avoids rewriting a goal of a finished plan. That the license differs by folder is also the first thing users should know.
- Alternative: Move the table to `LICENSING.md` or similar and leave only a link in the README. The README gets about 10 lines shorter, but `G-REPO-README-LICENSE` and roadmap S6 must be rewritten.

### D64. Line limit for the README [decided]

- **Decision:** As in the alternative, set no line limit. The shape of the README is checked by having exactly 3 headings (`G-RD-HEADINGS`) and by the goals on each section's content. Accordingly, the line-count spec item (old S638) and `G-RD-LENGTH` were dropped. (2026-11-01, confirmed by the user)
- **Recommendation**: 80 lines or fewer (`G-RD-LENGTH`). The estimate is about 75 lines: 5 for the description, 30 for the example, 8 for Build, 10 for Documentation, 20 for License. With a limit, later additions are steered toward the target documents.
- Alternative: Set no limit and only check that there are exactly 3 headings (`G-RD-HEADINGS`). If a section grows longer later, it is not detected.

### D65. The Status section [decided]

- **Decision:** As recommended, remove the Status section and move the explanation of provisional values (`provisionalValue`) to `stdlib/README.md`. (2026-11-01, confirmed by the user)
- **Recommendation**: Remove it. The version lives in `VERSION`, and the explanation that "many values are provisional ranges" moves to `stdlib/README.md` (the word `provisionalValue` moves too). The sentence "the app is not in this repository" becomes unnecessary with D59.
- Alternative: Keep one sentence, "Ozen is at version 0.1.0 and still experimental.", at the end of the description paragraph. Readers learn its maturity, but the README must be edited every time the version changes.

### D66. Version [decided]

- **Decision:** As recommended, keep the version at `0.1.0`. (2026-11-01, confirmed by the user)
- **Recommendation**: Keep it at `0.1.0`. This is a documentation-only change; the language, the compiler and the output do not change.
- Alternative: Raise it to `0.1.1`. More work fixing expectations that contain the version string (such as the 13 `.out` files in §3 of the syntax plan), and it falls outside the documentation-only scope (S682).

### D67. The Introduction page section [decided]

- **Decision:** Move the Introduction page section (`site/index.html` and how it is published to GitHub Pages) to `site/README.md`. In the README, keep only one line with the introduction page URL in the Documentation section. (2026-11-01, confirmed by the user. Other work added this section after the plan was written, so this decision was added before P0-2)
- **Recommendation**: As above. It matches D58's "an explanation goes in the README of the folder it covers", and opening `site/` shows the explanation. The introduction page itself is an entry point for users, so the README links to it.
- Alternative: Keep it as a section in the README. There would be 4 headings, and a developer-facing explanation of how the introduction page is published (the workflow, the font loading) would stay in the README.

### D68. Other work's commits in the range [decided]

- **Decision:** As recommended, write other work's commits that enter the range `RD` on lines of the form `- Other work's commit in range: <SHA>` in §9, and `G-RD-SCOPE` does not count the files those commits changed. The core hang (`G-CORE-FUZZ`) is fixed in a separate session, and those commits are also written on such lines. (2026-11-01, confirmed by the user. Added when `83db656` entered the range at the final gate and `G-RD-SCOPE` failed)
- **Recommendation**: As above. This plan's work shares `main` with other work (R3). Moving the tag (P0-2) cannot be used, because this plan's commits are already in the range. The excluded commits are named in the plan, so what was excluded can be checked later. If a written SHA is not in the range or does not exist, `G-RD-SCOPE` fails.
- Alternative: Keep the goal as is and record the failure as a known failure. The completion condition stays unmet.

---

## 3. Findings on the current state (2026-11-01, `7e3e8a5`)

### 3.1 Contents of the README

- 264 lines (2026-11-01, `4eeccf9`). The 10 headings are `Status`, `Vocabulary table`, `Core and CLI`, `Standard library`, `Platforms`, `Renderer`, `Bundles`, `Introduction page`, `Credits`, `License`. After the opening paragraph there are 3 lines of links to `docs/concept.md`, `plans/ozen-roadmap.md` and the introduction page (`<https://atsuki-seo.github.io/Ozen/>`).
- After the plan was written (after `7e3e8a5`), 9 commits of other work (`08a0c6b` to `4eeccf9`) added the introduction page `site/index.html` (in Japanese) and the publishing workflow `.github/workflows/pages.yml`. The README gained the line linking to the introduction page, the `Introduction page` section (no build step; only the DotGothic16 font is loaded from Google Fonts; a push to `main` publishes to GitHub Pages with the version and commit in the footer; the examples and output are copied by hand), and `site/` in the license table. `CLAUDE.md` gained the line "`site/index.html` is in Japanese" (D67).
- There are 3 examples (`readme: 3 examples`): schedule (`soup.ozen`, `kitchen.ozen`, `ozen debug schedule --entry lunch …`), rendering (`ozen render --kind recipe --lang en --entry lunch soup.ozen`, reusing the files of the previous example), and bundle (`package.ozen`, `soup.ozen`, `ozen bundle --entry soup`).
- Only the `Credits` section contains Japanese characters (the source wording of the food composition table).

### 3.2 Checks that bind the README

| Check | Location | Confirmed fact |
| --- | --- | --- |
| `G-REPO-README-LICENSE` | `scripts/goals.sh:94` | Looks for 2 rows of the license table (code and table sources; `docs/`, `plans/`, `spec/samples/`) and the sentence on output rights with `grep -cF` |
| `G-RF-README-EXAMPLE` | `scripts/goals.sh:588` | The last line of `check-readme-example.sh --readme README.md` is `readme: <n> examples, 0 mismatches` (n ≥ 1) |
| `G-RF-README-PIPE` | `scripts/goals.sh:593` | At least 1 `\|>` in the README's ```` ```ozen ```` blocks |
| `G-RF-NO-EMPTY-PARENS` | `scripts/goals.sh:597` | Its targets name `README.md` explicitly |
| `G-RF-README-NO-PROTO` | `scripts/goals.sh:584` | 0 headings `## Evaluator prototype` |
| `check-readme-example.sh` | `scripts/check-readme-example.sh` | `--readme FILE` checks any file. It turns each ```` ```ozen ```` block after `<!-- file: 名前 -->` into a file, and compares each ```` ```text ```` block with the standard output of the command in the `<!-- run: … -->` just before it. The first ```` ```ozen ```` that comes after a ```` ```text ```` starts a new set of files. Blocks with a language name other than ```` ```text ```` (such as ```` ```bash ````) and blocks without a language name are ignored |
| `check-test-names.py --empty-parens` | `scripts/check-test-names.py:11・30` | Given a folder, it collects only `.ozen`, `.sh` and `.c`. A `.md` file has its ```` ```ozen ```` blocks checked only when named explicitly. So new documents must be named explicitly |

### 3.3 Credits and NOTICE

- `NOTICE` already has the 3 items, the food composition table (including the Japanese source wording), Fluent (`@fluent/bundle` 0.19.1, Apache-2.0) and the SARIF 2.1.0 schema (OASIS), and the sentence on output rights. The 3 Credits lines in the README are a copy of this.

### 3.4 New documents do not break anything

- `scripts/check-spdx.sh` does not require an SPDX line in `.md` files.
- `core/gen/embed-std.cmake` and `core/CMakeLists.txt` embed only the `*.ozen` files of `stdlib/`. `stdlib/README.md` does not go into the compiler.
- `scripts/check-selftests.sh` collects only files with names like `check-*` and `run-*`. `README.md` and `CONTRIBUTING.md` are not targets.
- `renderers/tools/bundle.mjs` reads `renderers/vendor/README.md`, which is a different file from `renderers/README.md`.

### 3.5 Output of the example kept in the README

With `build/core-gcc-release/ozen`, we ran `ozen render --kind recipe --lang en soup.ozen` on the soup recipe of the current README (with `menu lunch` removed). Leaving out `--entry` gives the same output as `--entry soup` (there is only one entry point).

```text
# soup

## Ingredients
- water, 400 ml
- onion, 1
- carrot, 1

## Steps
1. Peel onion, then cut into slices (3 mm).
2. Peel carrot, then cut into rolling wedges (20 mm).
3. Simmer water, onion, carrot over low heat for 15 min.
```

### 3.6 Build steps

- `scripts/build-core.sh --only core-gcc-release` makes `build/core-gcc-release/ozen` (`cmake -S core -B build/<構成>`). `ozen render` calls `renderers/cli.mjs` with node (`cli/src/main.c:1154`). The tool versions are in `toolchain.txt`.
- `ozen` has no `--help` (`cli: unknown option: --help`). The command list moves from the README to `cli/README.md`.

### 3.7 State of the working tree

- As of 2026-11-01 there are uncommitted changes from other work: `spec/sources/search-log.yaml`, `stdlib/prelude.ozen`, `stdlib/sources.ozen` and 7 `.out` files in `tests/golden/`. This plan does not touch them. `ozen` is built from the working tree, so the example output may be affected by those changes (R3).
- The GitHub API (`gh api -X POST /markdown`, `gh api repos/atsuki-seo/Ozen/readme/<フォルダ>`) works in this environment. Rendering the old README as GFM gave 9 `<h2` and 0 `&lt;!--`. For the folder `data/mext-fct-2023`, the README returned was `data/mext-fct-2023/README.md`.

---

## 4. Behavior spec

Each checkable claim is one item with one claim, starting with `**S<n>**`. Below, "the 9 documents" means `README.md` and the 8 documents of §4.2, and "the old README" means `README.md` at the tag `readme-start`. "The X section" means everything from a `## 〜` heading (X being the heading text) outside code blocks to the next `## ` heading.

### 4.1 README (D57, D59, D62 to D65)

- **S637** The `## ` headings outside code blocks in the README are exactly 3, from the top: `Build`, `Documentation`, `License` (the `## Ingredients` and `## Steps` in the example's output block are not counted).
- **S639** The README has no links to documents in `docs/` or `plans/` (neither relative links nor `https://github.com/atsuki-seo/Ozen/blob/…` URLs).
- **S640** The README does not contain the file names `docs/concept.md`, `docs/prototype-report.md` or `plans/ozen-*.md`, not even as plain text that is not a link.
- **S641** The 9 documents do not contain the word `Ozendate` (D59; this includes it not slipping into where the Status section moved).
- **S642** The opening paragraph of the README (the paragraph after `# Ozen`) describes Ozen as `a language and compiler for cooking procedures`.
- **S643** The opening paragraph names the 3 pillars of the types (`dimension types`, `state types`, `linear types`).
- **S644** The opening paragraph says the compiler produces an IR and a schedule (`schedule`).
- **S645** The opening paragraph is 2 to 3 sentences.
- **S646** The README has exactly one ```` ```ozen ```` block, and it declares `recipe soup` (D57).
- **S647** The example comes before the `## Build` heading.
- **S648** The README example uses the pipe `|>` for the prep of every ingredient.
- **S649** The README example's output block matches the output of running `ozen render --kind recipe --lang en soup.ozen` (§3.5).
- **S650** The README example's output is the same as the output of the old README's rendering example (`--entry lunch`) with the heading line (`# lunch`) and the plating line (`4. Plate …`) removed and the heading changed to `# soup`.
- **S651** The Build section shows the steps to build with `scripts/build-core.sh --only core-gcc-release`.
- **S652** Running that command ends with `core-build: 1 configs, 0 failed`.
- **S653** The Build section links to `toolchain.txt` as the list of tool versions.
- **S654** The Build section says that `ozen render` needs `node`.
- **S655** The Documentation section links to all 8 documents of §4.2.
- **S689** The Documentation section links to the introduction page `https://atsuki-seo.github.io/Ozen/` (D67).
- **S656** The License section contains the 6 rows of the old README's license table word for word (including the 2 rows of `G-REPO-README-LICENSE`; D63).
- **S657** The License section contains the sentence on output rights (`Recipes you write in Ozen …`).
- **S658** The License section links to `LICENSE-MIT`, `LICENSE-APACHE` and `LICENSE-CC-BY-4.0`.
- **S659** The License section links to `NOTICE` (D62).
- **S660** `NOTICE` keeps the items for Fluent and the SARIF schema (removing Credits from the README loses no source; the food composition table is checked by `G-REPO-NOTICE`).
- **S690** The README has no `### ` headings outside code blocks and no code blocks without a language name (removed explanation sections and blocks of command syntax do not come back to the README).
- **S691** The README does not contain the specific terms that moved to the target documents (`ozen debug`, `ozen bundle`, `ozen unbundle`, `provisionalValue`, `vocabcheck`, `XCFramework`, `DotGothic16`, `recipe_build`, `Gantt`, `run-golden`).
- **S661** In GitHub's Markdown rendering (`gh api /markdown`, GFM), the README has 3 `<h2>`, and annotations such as `<!-- file: … -->` are not visible as text.

### 4.2 Target documents (D58)

The content is moved from the sections of the old README. The wording may be tidied, but the facts (commands, arguments, paths, versions) do not change.

- **S662** Each of the 8 documents has a heading starting with `# ` on line 1.
- **S663** `cli/README.md` contains the 6 command-syntax lines of the old README (from `ozen check [OPTIONS] [ENTRY|FILE] …` to `ozen debug ast|modules|…`) word for word.
- **S664** `spec/README.md` contains the 5 commands of the old README for checking the vocabulary table (from `python3 tools/vocabcheck/vocabcheck.py schema …` to `python3 tools/vocabcheck/sources.py …`) word for word.
- **S665** `cli/README.md` explains how the check deepens through level 0, level 1 and level 2 with the kitchen definition (`-k`) and today's conditions (`--options`, `-s`, `-c`, `-t`).
- **S666** The output block of the schedule example in `cli/README.md` (`soup.ozen`, `kitchen.ozen`, `ozen debug schedule --entry lunch -k kitchen.ozen soup.ozen`) matches the output of running it.
- **S667** The output block of the bundle example in `cli/README.md` (`package.ozen`, `soup.ozen`, `ozen bundle --entry soup`) matches the output of running it.
- **S668** `cli/README.md` explains how to read the schedule output (start..end, the latest start in parentheses `latest start`, the cook, the equipment).
- **S669** `cli/README.md` explains how bundling works (`std` excluded, inline `module` with full names, the `std` version in the header, `ozen unbundle` never overwrites).
- **S670** `core/README.md` explains the core written in C23 with no I/O, the public API `core/include/ozen.h` and `recipe_build`, the JSON schemas (`schemas/`, draft 2020-12), and `SARIF 2.1.0`.
- **S671** `stdlib/README.md` explains that the package `std` is embedded in the compiler, `stdlib/units/`, the types `Kitchen` and `Options`, `ozen debug definitions`, `stdlib/sources.ozen`, `phrases/ja.ftl` and `phrases/en.ftl`, and provisional values (`provisionalValue`) (D65).
- **S672** The output block of the rendering example in `renderers/README.md` matches the output of running it.
- **S692** The recipe and the output block of the rendering example in `renderers/README.md` are the same as the README's example.
- **S693** The output blocks of the schedule and bundle examples in `cli/README.md` are the same as the output blocks of the same commands in the old README. Also, the `<!-- run: … -->` lines of the 3 examples (2 in cli, 1 in renderers) are fixed commands, and the same command is also written in the text.
- **S673** `renderers/README.md` explains the kinds of output (recipe, schedule and Gantt chart, shopping list, HTML with print styles, book, settings form), that it has no build step and runs in node and in browsers, the single file for WebView (`renderers/tools/bundle.mjs`), Fluent (`renderers/vendor/fluent-bundle/`), how phrases are layered (`phrases/<lang>.ftl`), and the order in which it falls back to English when something is missing.
- **S674** `bindings/README.md` explains the 4 platforms (Linux, WebAssembly, iOS, Android), their paths (`bindings/linux/`, `bindings/wasm/`, `bindings/swift/`, `bindings/android/`), Swift, Web Worker, XCFramework, JNI, `scripts/build-wasm.sh`, and comparing the digest of the results with the native build.
- **S675** `spec/README.md` explains `spec/vocab/`, `tools/vocabcheck/`, the food composition table data (`data/mext-fct-2023/`) and re-extracting it (`tools/fct/extract.py --check --xlsx`), the research log (`spec/sources/search-log.yaml`), and the mapping to prior work (`spec/notes/prior-work.md`, four papers).
- **S676** `CONTRIBUTING.md` explains the 4 build configurations (`scripts/build-core.sh`, ASan, UBSan), running golden (`scripts/run-golden.sh build/core-gcc-release/ozen tests/golden`), how to run the goals (`scripts/goals.sh`), and CI (`.github/workflows/ci.yml`). It also gives `scripts/build-core.sh --only core-gcc-release`, which builds just one configuration, and `scripts/goals.sh --list`, which lists the goals.
- **S677** `CONTRIBUTING.md` names every CI job in `ci/jobs.txt` (`linux`, `fuzz`, `wasm`, `ios`, `android`).
- **S688** `site/README.md` explains the introduction page (`site/index.html`, in Japanese), no build step, the DotGothic16 font, publishing to GitHub Pages through `.github/workflows/pages.yml` with the version and commit in the footer, copying the examples and output by hand, and the published address `https://atsuki-seo.github.io/Ozen/` (D67).

### 4.3 Invariants

- **S678** Every term in the list of terms from the old README (the 33 terms of `G-RD-TERMS` in §5) is in at least one of the 9 documents (no explanation has disappeared).
- **S679** The targets of relative links in the 9 documents all exist and are not files git ignores (such as `build/`).
- **S680** The ```` ```ozen ```` blocks of the 9 documents contain no `recipe` or `menu` declaration and no `make` with empty `()`.
- **S681** The 9 documents contain no Japanese characters (kanji, hiragana, katakana) (the language rules of `CLAUDE.md`).
- **S682** The files committed in this plan's range (`RD`) are only the 9 documents, `CLAUDE.md`, `scripts/goals.sh` and this file.
- **S694** Every commit written on a `- Other work's commit in range: ` line in §9 exists and is inside the range `RD`. S682 counts the commits of the range except those written on such lines (D68).
- **S683** The tag `readme-start` exists (D60; without it, the range of S682 is empty and the check passes without checking anything).
- **S684** In this plan's range, no lines were removed from `scripts/goals.sh` or `CLAUDE.md` (only additions; existing goals are not loosened).
- **S685** The version stays at `0.1.0` (D66).
- **S686** The Plan section of `CLAUDE.md` has a line naming this file, `G-RD-*` and `readme-start` (D61).
- **S687** After the push, the GitHub API returns each `README.md` as the README of the 7 folders (`cli`, `core`, `stdlib`, `renderers`, `bindings`, `spec`, `site`).

### 4.4 List of things added or changed

| Kind | Path |
| --- | --- |
| Rewritten document | `README.md` |
| New documents | `cli/README.md`, `core/README.md`, `stdlib/README.md`, `renderers/README.md`, `bindings/README.md`, `spec/README.md`, `site/README.md`, `CONTRIBUTING.md` |
| Goals | `scripts/goals.sh` (`PLAN_RD`, `RD`, `DOCS_RD`, `OZ` and `goal_G_RD_*`, additions only) |
| Rules | `CLAUDE.md` (one line in the Plan section, additions only) |

### 4.5 Where each section of the old README goes

| Section of the old README | Goes to |
| --- | --- |
| Opening paragraph | README (shortened to 2 to 3 sentences; keep the 3 pillars of the types, the IR and the schedule; remove the Ozendate sentence) |
| Links to the concept and the roadmap | Removed |
| Link to the introduction page | One line with the URL in the README's Documentation section (D67) |
| Status | Removed. The explanation of provisional values goes to `stdlib/README.md` (D65). The version statement and the sentence "what the plan covers is implemented and tested" are removed, not moved |
| Vocabulary table | `spec/README.md` (the command block as is) |
| Core and CLI | Command syntax (the block as is), levels, the schedule example and how to read it go to `cli/README.md`. The explanation of the core, the public API, the schemas and SARIF go to `core/README.md`. The 4 build configurations and golden go to `CONTRIBUTING.md` |
| Standard library | `stdlib/README.md` |
| Platforms | `bindings/README.md`. The CI jobs go to `CONTRIBUTING.md` |
| Renderer | `renderers/README.md` (the rendering example is redone with soup, without `menu`) |
| Bundles | The `ozen bundle` section of `cli/README.md` |
| Introduction page | `site/README.md` (D67) |
| Credits | Removed (D62) |
| License | Kept in the README (D63) |

---

## 5. Definitions of machine-checked goals

**Prerequisite goals** (implicitly part of every task's completion condition):

- Everything "run locally" in §5 of the syntax plan, and these `G-RF-*` goals of the syntax plan: `G-RF-README-NO-PROTO`, `G-RF-README-EXAMPLE`, `G-RF-README-PIPE`, `G-RF-NO-EMPTY-PARENS`, `G-RF-CLAUDEMD`. In particular, `G-REPO-README-LICENSE`, `G-REPO-NOTICE`, `G-REPO-SPDX`, `G-REPO-CLAUDEMD`, `G-REPO-VERSION`, `G-REPO-SELFTESTS` and `G-CORE-BUILD` concern the files this plan touches
- Before every push: `G-REPO-NOREPLY`
- After the push: `G-PLAT-CI` (a documentation-only change, so we only check that CI does not fail) and this plan's `G-RD-GH-READMES`

On failure, report which check of which goal failed.

- `RD=readme-start..HEAD`, `PLAN_RD=plans/ozen-readme-compact.md`, `OZ=build/core-gcc-release/ozen`.
- `DOCS_RD="README.md CONTRIBUTING.md cli/README.md core/README.md stdlib/README.md renderers/README.md bindings/README.md spec/README.md site/README.md"`.
- `OLD="git show readme-start:README.md"` (the old README).
- Counts expected to be 0 are judged by the number printed, not by the exit code. `goals.sh` uses `set -o pipefail`, so files are passed directly to `grep -s` to read them; this keeps a failing `cat` from making the whole pipe give a wrong result while a document does not exist yet.
- The judgments assume the behavior of GNU grep (`/usr/bin/grep`). `grep -q` given several files exits 0 if any of them matches, even if some files are missing (confirmed on 2026-11-01).
- `G-RD-GFM` sends the README text to GitHub's Markdown API. The README is a public document, so sending it is fine.
- In P0-3, the code block below becomes the functions `goal_G_RD_*` in `scripts/goals.sh`. The `sec` that cuts out a section and the step that lists headings outside code blocks may be small functions inside `goals.sh`.

### 5.1 Structure and data

| ID | What it decides | Pass condition |
| --- | --- | --- |
| `G-RD-DECISIONS` | Whether D57 to D68 are settled | 12 decided, 0 open, 12 Decision lines |
| `G-RD-COVERAGE` | Every S item has a row in the coverage table, and each item of §8 has a reason | Empty diff, 0 items without a reason |
| `G-RD-PLAN-TESTS` | There are as many mutation check records as coverage table rows | `mutations: <m> rows, <m> recorded` |
| `G-RD-HEADINGS` | S637 | The sequence of headings is `Build Documentation License ` |
| `G-RD-NO-MOVED` | S690, S691 | All 3 counts are 0 |
| `G-RD-NO-PLAN-LINKS` | S639, S640 | All 4 counts are 0 |
| `G-RD-NO-OZENDATE` | S641 | 0 |
| `G-RD-INTRO` | S642 to S645 | The counts of the 5 terms and of `IR` are all 1 or more, and the number of sentences is 2 to 3 |
| `G-RD-EXAMPLE` | S646, S647, S649 | 1 block, 1 `recipe soup`, 1 `run:` line, `before`, `readme: 1 examples, 0 mismatches` |
| `G-RD-PIPE-ALL` | S648 | 0 `let` lines without a pipe, 0 `let` lines with a call that takes a name as its first argument, 0 `let` lines other than `make` and `simmer(` that do not start with `= 名前 \|>` (a name followed by a pipe), and 2 or more pipes |
| `G-RD-RENDER-SAME` | S650, S692 | All 3 diffs empty, and 5 or more lines in the output block |
| `G-RD-BUILD` | S651, S653, S654 | All 4 counts are 1 or more |
| `G-RD-BUILD-RUN` | S652 | `core-build: 1 configs, 0 failed` |
| `G-RD-DOC-LINKS` | S655, S689 | 0 documents without a link, and 1 or more links to the introduction page |
| `G-RD-LICENSE` | S656 to S659 | 0 missing table rows, 1 sentence, 0 missing links to license files, and 1 or more links to `NOTICE` |
| `G-RD-NOTICE` | S660 | Both counts are 1 or more |
| `G-RD-GFM` | S661 | 3 `<h2`, 0 `&lt;!--` |
| `G-RD-DOC-TITLES` | S662 | 0 documents that do not start with a heading |
| `G-RD-FACTS` | S663, S664 | 11 lines in the old README's blocks without a language name, 0 missing |
| `G-RD-DOC-EXAMPLES` | S666, S667, S672, S693 | `cli/README.md` gives `readme: 2 examples, 0 mismatches`, `renderers/README.md` gives `readme: 1 examples, 0 mismatches`, 0 missing run lines, and empty diffs against the old README |
| `G-RD-DOC-CONTENT` | S665, S668 to S671, S673 to S676, S688 | 0 pairs not found |
| `G-RD-CI-JOBS` | S677 | 0 jobs not named |
| `G-RD-TERMS` | S678 | 0 terms not found |
| `G-RD-LINKS` | S679 | 0 links without a target |
| `G-RD-NO-EMPTY-PARENS` | S680 | `empty-parens: <n> sources, 0 found, 0 unlisted exceptions` |
| `G-RD-ENGLISH` | S681 | 0 |
| `G-RD-SCOPE` | S682, S683, S694 | `ok`, 0 excluded commits outside the range, 0 files outside the scope |
| `G-RD-ADD-ONLY` | S684 | 0 |
| `G-RD-VERSION` | S685 | `0.1.0` |
| `G-RD-CLAUDEMD` | S686 | 1 or more |
| `G-RD-GH-READMES` | S687 (after the push) | 0 mismatches |
| `G-RD-AUDIT` | Record of the P3-1 audit | 1 or more |

```bash
sec() { awk -v h="## $1" '/^```/{f=!f} !f&&$0==h{s=1;next} s&&!f&&/^## /{exit} s' README.md; }   # body of a README section
# G-RD-DECISIONS
grep -c '^### D[0-9]*\. .*\[decided\]' $PLAN_RD   # → 12
grep -c '^### D[0-9]*\. .*\[open\]' $PLAN_RD       # → 0
grep -c '^- \*\*Decision:' $PLAN_RD                    # → 12
# G-RD-COVERAGE
diff <(sed -n '/^## 4\./,/^## 5\./p' $PLAN_RD | grep -oE '\*\*S[0-9]+\*\*' | tr -d '*' | sort -u) \
     <(sed -n '/^### 5\.3/,/^## 6\./p' $PLAN_RD | grep -oE '^\| S[0-9]+' | tr -d '| ' | sort -u)   # → empty
sed -n '/^## 8\./,/^## 9\./p' $PLAN_RD | grep '^- ' | grep -v 'Reason a command cannot decide:' | wc -l   # → 0
# G-RD-PLAN-TESTS
scripts/check-plan-tests.sh --mutations $PLAN_RD   # → mutations: <m> rows, <m> recorded
# G-RD-HEADINGS (lines inside code blocks are not counted)
awk '/^```/{f=!f;next} !f&&/^## /{sub(/^## /,"");printf "%s ",$0}' README.md   # → "Build Documentation License "
# G-RD-NO-MOVED
awk '/^```/{f=!f;next} !f&&/^### /' README.md | wc -l                                      # → 0
awk '/^```/{ if(!f){f=1; if($0=="```") n++} else f=0 } END{print n+0}' README.md           # → 0
for t in 'ozen debug' 'ozen bundle' 'ozen unbundle' 'provisionalValue' 'vocabcheck' 'XCFramework' 'DotGothic16' \
         'recipe_build' 'Gantt' 'run-golden'; do grep -qF -- "$t" README.md && echo "$t"; done | wc -l   # → 0
# G-RD-NO-PLAN-LINKS
grep -cE '\]\((https://github\.com/atsuki-seo/Ozen/(blob|tree)/[^/)]+/|\./|\.\./|/)?(docs|plans)(/|\)|#)' README.md   # → 0
grep -cE 'docs/concept\.md|docs/prototype-report\.md|plans/ozen-[a-z-]+\.md' README.md                      # → 0
grep -cE 'atsuki-seo/Ozen/(blob|tree)/[^/ )>]+/(docs|plans)\b' README.md                                     # → 0 (URLs, whatever the link form)
grep -cE '^ *\[[^]]+\]: *(\./|/)?(docs|plans)(/| |$)' README.md                                              # → 0 (reference-style links)
# G-RD-NO-OZENDATE
grep -sh 'Ozendate' $DOCS_RD | wc -l               # → 0
# G-RD-INTRO (opening paragraph = the first paragraph after line 1)
intro=$(awk 'NR==1{next} /^$/{if(p)exit; next} {p=1; print}' README.md | tr '\n' ' ')
for t in 'a language and compiler for cooking procedures' 'dimension types' 'state types' 'linear types' 'schedule'; do
  grep -cF -- "$t" <<< "$intro"
done                                               # → 1 or more on all 5 lines
grep -cw 'IR' <<< "$intro"                         # → 1 or more
grep -oE '[.!?]( |$)' <<< "$intro" | wc -l         # → 2 to 3
# G-RD-EXAMPLE
grep -c '^```ozen$' README.md                      # → 1
grep -c '^recipe soup' README.md                   # → 1
grep -cxF '<!-- run: ozen render --kind recipe --lang en soup.ozen -->' README.md   # → 1
[ "$(grep -n '^```ozen$' README.md | head -n 1 | cut -d: -f1)" -lt "$(grep -n '^## Build$' README.md | cut -d: -f1)" ] && echo before   # → before
scripts/check-readme-example.sh --readme README.md --ozen $OZ | tail -n 1   # → readme: 1 examples, 0 mismatches
# G-RD-PIPE-ALL (in the example, every let line that starts from an ingredient uses a pipe; lines calling a function such as simmer are excluded)
awk '/^```ozen$/{f=1;next} f&&/^```$/{f=0} f' README.md | grep -E '^ *let [A-Za-z]+ = [a-z]+ ' | grep -v '= make ' | grep -vc '|>'   # → 0
awk '/^```ozen$/{f=1;next} f&&/^```$/{f=0} f' README.md | grep -cE '^ *let [A-Za-z]+ = [a-z]+\([A-Za-z]+[(,]'   # → 0 (lines like cut(peel(carrot), …) that take a name or a nested call as the first argument)
awk '/^```ozen$/{f=1;next} f&&/^```$/{f=0} f' README.md | grep -E '^ *let [A-Za-z0-9_]+ = ' | grep -vE '= (make |simmer\()' | grep -vEc '= [a-z][A-Za-z0-9_]* \|>'   # → 0
awk '/^```ozen$/{f=1;next} f&&/^```$/{f=0} f' README.md | grep -o '|>' | wc -l   # → 2 or more
# G-RD-RENDER-SAME
blk() { awk -v r="$1" '$0==r{s=1;next} s&&/^```text$/{t=1;next} t&&/^```$/{exit} t'; }
diff <($OLD | blk '<!-- run: ozen render --kind recipe --lang en --entry lunch soup.ozen -->' | grep -vE '^(# |4\. )') \
     <(blk '<!-- run: ozen render --kind recipe --lang en soup.ozen -->' < README.md | grep -v '^# ')   # → empty
blk '<!-- run: ozen render --kind recipe --lang en soup.ozen -->' < README.md | wc -l   # → 5 or more
diff <(blk '<!-- run: ozen render --kind recipe --lang en soup.ozen -->' < README.md) \
     <(blk '<!-- run: ozen render --kind recipe --lang en soup.ozen -->' < renderers/README.md)   # → empty (S692)
diff <(awk '/^```ozen$/{f=1;next} f&&/^```$/{f=0} f' README.md) \
     <(awk '/^```ozen$/{f=1;next} f&&/^```$/{f=0} f' renderers/README.md)                    # → empty (S692)
# G-RD-BUILD
sec Build | grep -cF 'scripts/build-core.sh --only core-gcc-release'   # → 1 or more
grep -cF 'core-gcc-release' scripts/build-core.sh                      # → 1 or more (the configuration name exists)
sec Build | grep -cF '](toolchain.txt)'                                # → 1 or more
sec Build | grep -cw 'node'                                            # → 1 or more
# G-RD-BUILD-RUN
scripts/build-core.sh --only core-gcc-release | tail -n 1   # → core-build: 1 configs, 0 failed
# G-RD-DOC-LINKS
for p in CONTRIBUTING.md cli/README.md core/README.md stdlib/README.md renderers/README.md bindings/README.md spec/README.md site/README.md; do
  sec Documentation | grep -qF "]($p)" || echo "$p"
done | wc -l                                       # → 0
sec Documentation | grep -cF '](https://atsuki-seo.github.io/Ozen/)'   # → 1 or more
# G-RD-LICENSE
$OLD | grep '^| ' | while IFS= read -r l; do sec License | grep -qxF -- "$l" || echo "$l"; done | wc -l   # → 0 (8 lines, including the table header and separator)
sec License | grep -cF 'Recipes you write in Ozen and the schedules and IR the compiler produces are yours; the Ozen licenses do not apply to them.'   # → 1
for p in LICENSE-MIT LICENSE-APACHE LICENSE-CC-BY-4.0; do sec License | grep -qF "]($p)" || echo "$p"; done | wc -l   # → 0
sec License | grep -cF '](NOTICE)'                 # → 1 or more
# G-RD-NOTICE
grep -c 'Fluent' NOTICE                            # → 1 or more
grep -c 'SARIF' NOTICE                             # → 1 or more
# G-RD-GFM
gh api -X POST /markdown -f mode=gfm -f context=atsuki-seo/Ozen -F text=@README.md > "$tmp/readme.html"
grep -o '<h2' "$tmp/readme.html" | wc -l           # → 3
grep -c '&lt;!--' "$tmp/readme.html"               # → 0 (tmp=$(mktemp -d))
# G-RD-DOC-TITLES
for f in CONTRIBUTING.md cli/README.md core/README.md stdlib/README.md renderers/README.md bindings/README.md spec/README.md site/README.md; do
  head -n 1 "$f" 2>/dev/null | grep -q '^# ' || echo "$f"
done | wc -l                                       # → 0
# G-RD-FACTS (the blocks without a language name in the old README; the first goes to spec, the second to cli, word for word)
facts=$($OLD | awk '/^```/{ if(!f){f=1; n++; plain=($0=="```")} else f=0; next} f&&plain{print n"\t"$0}')
wc -l <<< "$facts"                                 # → 11
while IFS=$'\t' read -r n l; do
  f=$([ "$n" = 1 ] && echo spec/README.md || echo cli/README.md)
  grep -sqxF -- "$l" "$f" || echo "$f: $l"
done <<< "$facts" | wc -l                          # → 0
# G-RD-DOC-EXAMPLES
scripts/check-readme-example.sh --readme cli/README.md --ozen $OZ | tail -n 1         # → readme: 2 examples, 0 mismatches
scripts/check-readme-example.sh --readme renderers/README.md --ozen $OZ | tail -n 1   # → readme: 1 examples, 0 mismatches
while IFS='|' read -r f r; do   # exactly 1 run line, and the same command in the text (S693)
  [ "$(grep -cxF "<!-- run: $r -->" "$f")" = 1 ] && grep -qF "\`$r\`" "$f" || echo "$f: $r"
done <<'EOF' | wc -l                               # → 0
cli/README.md|ozen debug schedule --entry lunch -k kitchen.ozen soup.ozen
cli/README.md|ozen bundle --entry soup
renderers/README.md|ozen render --kind recipe --lang en soup.ozen
EOF
for r in 'ozen debug schedule --entry lunch -k kitchen.ozen soup.ozen' 'ozen bundle --entry soup'; do
  diff <($OLD | blk "<!-- run: $r -->") <(blk "<!-- run: $r -->" < cli/README.md)
done                                               # → empty (S693)
# G-RD-DOC-CONTENT (pairs of file and term; counts the pairs not found; each P1 task counts only the pairs of its own files)
while IFS='|' read -r f t; do grep -sqF -- "$t" "$f" || echo "$f: $t"; done <<'EOF' | wc -l   # → 0
cli/README.md|`-k`
cli/README.md|--options
cli/README.md|`-s`, `-c`, `-t`
cli/README.md|level 0
cli/README.md|level 1
cli/README.md|level 2
cli/README.md|latest start
cli/README.md|inline `module`
cli/README.md|`std` version
cli/README.md|never overwrites
cli/README.md|in seconds
cli/README.md|start..end
cli/README.md|the cook
cli/README.md|the equipment
cli/README.md|except `std`
core/README.md|C23
core/README.md|no I/O
core/README.md|core/include/ozen.h
core/README.md|recipe_build
core/README.md|schemas/
core/README.md|2020-12
core/README.md|SARIF 2.1.0
stdlib/README.md|`std`
stdlib/README.md|embedded
stdlib/README.md|stdlib/units/
stdlib/README.md|`Kitchen`
stdlib/README.md|`Options`
stdlib/README.md|ozen debug definitions
stdlib/README.md|stdlib/sources.ozen
stdlib/README.md|phrases/ja.ftl
stdlib/README.md|phrases/en.ftl
stdlib/README.md|provisionalValue
renderers/README.md|Gantt
renderers/README.md|shopping list
renderers/README.md|print
renderers/README.md|book
renderers/README.md|settings form
renderers/README.md|no build step
renderers/README.md|WebView
renderers/README.md|renderers/tools/bundle.mjs
renderers/README.md|renderers/vendor/fluent-bundle/
renderers/README.md|phrases/<lang>.ftl
renderers/README.md|falls back to English
renderers/README.md|in node and in browsers
renderers/README.md|then the dependencies
renderers/README.md|name made from the identifier
bindings/README.md|Linux
bindings/README.md|WebAssembly
bindings/README.md|iOS
bindings/README.md|Android
bindings/README.md|bindings/linux/
bindings/README.md|bindings/wasm/
bindings/README.md|bindings/swift/
bindings/README.md|bindings/android/
bindings/README.md|Swift
bindings/README.md|Web Worker
bindings/README.md|XCFramework
bindings/README.md|JNI
bindings/README.md|scripts/build-wasm.sh
bindings/README.md|digest
spec/README.md|spec/vocab/
spec/README.md|tools/vocabcheck/
spec/README.md|data/mext-fct-2023/
spec/README.md|tools/fct/extract.py --check --xlsx
spec/README.md|spec/sources/search-log.yaml
spec/README.md|spec/notes/prior-work.md
spec/README.md|four papers
CONTRIBUTING.md|scripts/build-core.sh
CONTRIBUTING.md|ASan
CONTRIBUTING.md|UBSan
CONTRIBUTING.md|scripts/run-golden.sh build/core-gcc-release/ozen tests/golden
CONTRIBUTING.md|scripts/goals.sh
CONTRIBUTING.md|.github/workflows/ci.yml
CONTRIBUTING.md|four configurations
CONTRIBUTING.md|gcc and clang
CONTRIBUTING.md|Release
CONTRIBUTING.md|scripts/build-core.sh --only core-gcc-release
CONTRIBUTING.md|scripts/goals.sh --list
site/README.md|site/index.html
site/README.md|no build step
site/README.md|DotGothic16
site/README.md|.github/workflows/pages.yml
site/README.md|GitHub Pages
site/README.md|footer
site/README.md|by hand
site/README.md|https://atsuki-seo.github.io/Ozen/
EOF
# G-RD-CI-JOBS
while IFS= read -r j; do grep -sqw -- "$j" CONTRIBUTING.md || echo "$j"; done < ci/jobs.txt | wc -l   # → 0
# G-RD-TERMS (terms of the old README; each must be in at least one of the 9 documents)
for t in 'ozen check' 'ozen build' 'ozen render' 'ozen bundle' 'ozen unbundle' 'ozen debug' '--format' 'sarif' \
         'core/include/ozen.h' 'recipe_build' 'schemas/' 'SARIF 2.1.0' 'scripts/build-core.sh' 'scripts/run-golden.sh' \
         'scripts/build-wasm.sh' 'bindings/wasm/ozen.mjs' 'bindings/wasm/worker.js' 'XCFramework' 'JNI' \
         '.github/workflows/ci.yml' 'renderers/vendor/fluent-bundle/' 'renderers/tools/bundle.mjs' 'phrases/<lang>.ftl' \
         'ozen debug definitions' 'stdlib/sources.ozen' 'provisionalValue' 'spec/sources/search-log.yaml' \
         'spec/notes/prior-work.md' 'tools/fct/extract.py' 'data/mext-fct-2023/' \
         'site/index.html' '.github/workflows/pages.yml' 'https://atsuki-seo.github.io/Ozen/'; do
  grep -sqF -- "$t" $DOCS_RD || echo "$t"
done | wc -l                                       # → 0
# G-RD-LINKS (the title in "](target "title")" is dropped; links to files git ignores are counted too)
for f in $DOCS_RD; do
  grep -soE '\]\([^)]+\)' "$f" | sed -E 's/^\]\(//; s/\)$//; s/ .*$//; s/#.*$//' | grep -vE '^(https?:|mailto:|$)' |
    while IFS= read -r p; do
      x=$(realpath -m --relative-to=. "$(dirname "$f")/$p")
      { [ -e "$x" ] && ! git check-ignore -q "$x"; } || echo "$f: $p"
    done
done | wc -l                                       # → 0
# G-RD-NO-EMPTY-PARENS
python3 -B scripts/check-test-names.py --empty-parens --exceptions scripts/empty-parens-exceptions.txt $DOCS_RD | tail -n 1
                                                   # → empty-parens: <n> sources, 0 found, 0 unlisted exceptions
# G-RD-ENGLISH
grep -shP '[\p{Han}\p{Hiragana}\p{Katakana}]' $DOCS_RD | wc -l   # → 0
# G-RD-SCOPE (without the tag, git diff prints nothing and passes, so check the tag first)
git rev-parse -q --verify refs/tags/readme-start >/dev/null && echo ok   # → ok
ex=$(grep -oE '^- Other work's commit in range: [0-9a-f]{7,40}' $PLAN_RD | awk '{print $NF}' |
     while IFS= read -r c; do git rev-parse -q --verify "$c^{commit}" || echo "unknown:$c"; done)   # D68 exclusions
while IFS= read -r c; do [ -n "$c" ] || continue; git rev-list $RD | grep -qxF -- "$c" || echo "$c"; done <<< "$ex" | wc -l   # → 0
git rev-list --no-merges $RD | grep -vxF -e "${ex:-none}" | while IFS= read -r c; do
  git diff-tree --no-commit-id --no-renames --name-only -r "$c"
done | sort -u | grep -vxF -e README.md -e CONTRIBUTING.md -e cli/README.md -e core/README.md \
  -e stdlib/README.md -e renderers/README.md -e bindings/README.md -e spec/README.md -e site/README.md \
  -e CLAUDE.md -e scripts/goals.sh -e plans/ozen-readme-compact.md | wc -l   # → 0 (counts the files of each commit)
# G-RD-ADD-ONLY
git diff --no-renames $RD -- scripts/goals.sh CLAUDE.md | grep '^-' | grep -vc '^---'   # → 0
# G-RD-VERSION
cat VERSION                                        # → 0.1.0
# G-RD-CLAUDEMD
grep -F 'plans/ozen-readme-compact.md' CLAUDE.md | grep -c 'G-RD-.*readme-start'   # → 1 or more
# G-RD-GH-READMES (after the push)
for d in cli core stdlib renderers bindings spec site; do
  [ "$(gh api "repos/atsuki-seo/Ozen/readme/$d" --jq .path 2>/dev/null)" = "$d/README.md" ] || echo "$d"
done | wc -l                                       # → 0
# G-RD-AUDIT (P3-1)
grep -cE '^- Coverage audit: [0-9]+ findings, [0-9]+ taken' $PLAN_RD   # → 1 or more
```

`G-RD-SCOPE` allows this file in case it is committed after the starting tag. `git diff A..B` compares only commits, so uncommitted changes of other work (§3.7) are not counted. The terms of `G-RD-TERMS` are the specific commands, paths and names picked from each section of the old README whose explanation must not disappear. `G-RD-DOC-CONTENT` goes further and fixes which document each term must be in.

### 5.2 Tests

The tests of this plan are the §5.1 goals themselves; no new test scripts are written. The check scripts used (`check-readme-example.sh`, `check-test-names.py`, `check-plan-tests.sh`) already have self-tests (`G-REPO-SELFTESTS`).

- The expected output of the examples is copied from the expected output of the old README (written by hand in the syntax plan). The expected output of the new rendering example (the README and `renderers/README.md`) is made from the old README's rendering example by removing the heading and the plating line, and `G-RD-RENDER-SAME` checks by machine that it was made that way. The output of the implementation (`ozen`) is not copied.
- A failing goal is not loosened; the cause is investigated. If the expected output and the actual output disagree, find out whether the document is wrong or the implementation changed, and if the implementation changed, report it to the user.

By machine we cover the shape of the README (headings, links, terms, section contents, rendering on GitHub), the examples' outputs matching, the facts staying word for word in the target documents, and that the change only added documents. Whether the writing gets across and reads naturally goes to §8.

### 5.3 Coverage table

Of the aspects, "Time and order" and "Persistence" do not apply, since this is a documentation-only change. "Real path" is met by actually running the examples with `ozen` and comparing (`G-RD-EXAMPLE`, `G-RD-DOC-EXAMPLES`), actually running the README's build steps (`G-RD-BUILD-RUN`), and checking rendering and README resolution through the GitHub API (`G-RD-GFM`, `G-RD-GH-READMES`).

| S item | Aspect | Test or goal | How to break it | Main task |
| --- | --- | --- | --- | --- |
| S637 | Normal | `G-RD-HEADINGS` | Put the `## Credits` heading back in the README | P2-1 |
| S637 | Boundary | `G-RD-HEADINGS` | Change the heading judgment to also count inside code blocks (`grep '^## '`), and confirm it fails on `## Steps` in the output block | P2-1 |
| S639 | No-effect condition | `G-RD-NO-PLAN-LINKS` | Put `[docs/concept.md](docs/concept.md)` back in the README | P2-1 |
| S639 | No-effect condition | `G-RD-NO-PLAN-LINKS` | Add a link to `https://github.com/atsuki-seo/Ozen/tree/main/plans/` to the README | P2-1 |
| S640 | No-effect condition | `G-RD-NO-PLAN-LINKS` | Add `Design concept: docs/concept.md` to the README as plain text, not a link | P2-1 |
| S639 | No-effect condition | `G-RD-NO-PLAN-LINKS` | Add a relative link to a folder, `[Plans](plans)`, to the README | P3-1 |
| S639 | No-effect condition | `G-RD-NO-PLAN-LINKS` | Add `<https://github.com/atsuki-seo/Ozen/tree/main/docs/>` to the README | P3-1 |
| S690 | No-effect condition | `G-RD-NO-MOVED` | Add a `### Status` section under the README's Documentation section | P3-1 |
| S690 | No-effect condition | `G-RD-NO-MOVED` | Paste the old README's command-syntax block (a block without a language name) into the README's Build section | P3-1 |
| S691 | No-effect condition | `G-RD-NO-MOVED` | Add a `provisionalValue` sentence after the README's opening paragraph | P3-1 |
| S641 | No-effect condition | `G-RD-NO-OZENDATE` | Add an Ozendate sentence to `stdlib/README.md` | P2-1 |
| S642 | Normal | `G-RD-INTRO` | Change `cooking procedures` in the opening paragraph to `recipes` | P2-1 |
| S643 | Normal | `G-RD-INTRO` | Remove `linear types` from the opening paragraph | P2-1 |
| S644 | Normal | `G-RD-INTRO` | Remove the schedule sentence from the opening paragraph | P2-1 |
| S644 | Normal | `G-RD-INTRO` | Remove `an IR and` from the opening paragraph | P3-1 |
| S645 | Boundary | `G-RD-INTRO` | Add a 4th sentence to the opening paragraph | P2-1 |
| S646 | Normal | `G-RD-EXAMPLE` | Add a ```` ```ozen ```` block of `kitchen.ozen` to the README | P2-1 |
| S647 | Normal | `G-RD-EXAMPLE` | Move the example after the `## Build` section | P2-1 |
| S648 | Docs match | `G-RD-PIPE-ALL` | Rewrite only the carrot line without pipes, as `cut(peel(carrot), rollCut, 20 mm)` | P2-1 |
| S648 | Docs match | `G-RD-PIPE-ALL` | Rewrite the carrot line as `cut( peel(carrot), rollCut, 20 mm)`, with a space inside | P3-1 |
| S648 | Docs match | `G-RF-README-PIPE` | Turn every pipe in the example into function-call form | P2-1 |
| S649 | Docs match | `G-RD-EXAMPLE` | Change `(3 mm)` in the expected output to `(4 mm)` | P2-1 |
| S649 | Docs match | `G-RF-README-EXAMPLE` | Remove the 3rd step line from the expected output | P2-1 |
| S650 | Docs match | `G-RD-RENDER-SAME` | Change `slice` to something like `dice` in both the expected output and the example, so it still matches the run but differs from the old README | P2-1 |
| S651 | Normal | `G-RD-BUILD` | Make the Build section's command just `scripts/build-core.sh` | P2-1 |
| S652 | Real path | `G-RD-BUILD-RUN` | Narrow the `core-gcc-*` case in `build-core.sh` to `core-gcc-debug-*`, making `core-gcc-release` an unknown configuration (restore after checking) | P2-1 |
| S653 | Normal | `G-RD-BUILD` | Move the link to `toolchain.txt` to the Documentation section | P2-1 |
| S654 | Normal | `G-RD-BUILD` | Remove the node sentence from the Build section | P2-1 |
| S655 | Data completeness | `G-RD-DOC-LINKS` | Remove the link to `spec/README.md` from the Documentation section | P2-1 |
| S689 | Normal | `G-RD-DOC-LINKS` | Remove the introduction page link from the Documentation section | P2-1 |
| S656 | Data completeness | `G-RD-LICENSE` | Remove the `*/vendor/` row from the license table | P2-1 |
| S656 | Data completeness | `G-REPO-README-LICENSE` | Remove the `docs/` row from the license table | P2-1 |
| S657 | Normal | `G-RD-LICENSE` | Move the sentence on output rights to the Build section | P2-1 |
| S658 | Data completeness | `G-RD-LICENSE` | Remove the link to `LICENSE-CC-BY-4.0` | P2-1 |
| S659 | Normal | `G-RD-LICENSE` | Remove the link to `NOTICE` in the License section | P2-1 |
| S660 | Side effects | `G-RD-NOTICE` | Temporarily remove the Fluent item from `NOTICE` (restore after checking) | P2-1 |
| S661 | Real path | `G-RD-GFM` | Indent the `<!-- file: soup.ozen -->` line before the example by 4 spaces, so it is rendered as text in an indented code block | P2-1 |
| S662 | Data completeness | `G-RD-DOC-TITLES` | Remove the heading on line 1 of `core/README.md` | P1-2 |
| S663 | Docs match | `G-RD-FACTS` | Change the `ozen unbundle FILE [-o DIR]` line of `cli/README.md` to `ozen unbundle FILE` | P1-1 |
| S664 | Docs match | `G-RD-FACTS` | Remove the arguments of the `sample` command in `spec/README.md` | P1-4 |
| S665 | Normal | `G-RD-DOC-CONTENT` | Remove `level 1` from the level explanation in `cli/README.md` | P1-1 |
| S666 | Docs match | `G-RD-DOC-EXAMPLES` | Change `total 1560` in the schedule's expected output to `total 1561` | P1-1 |
| S667 | Docs match | `G-RD-DOC-EXAMPLES` | Remove the `from: kitchenBook "0.1.0"` line from the bundle example's expected output | P1-1 |
| S668 | Normal | `G-RD-DOC-CONTENT` | Remove the sentences on reading the schedule output from `cli/README.md` | P1-1 |
| S668 | Normal | `G-RD-DOC-CONTENT` | Remove only `Times are in seconds.` from `cli/README.md` | P3-1 |
| S669 | Normal | `G-RD-DOC-CONTENT` | Remove the `never overwrites` sentence from `cli/README.md` | P1-1 |
| S669 | Normal | `G-RD-DOC-CONTENT` | Remove `, except `std`,` from `cli/README.md` | P3-1 |
| S670 | Normal | `G-RD-DOC-CONTENT` | Remove `2020-12` from `core/README.md` | P1-2 |
| S670 | Normal | `G-RD-DOC-CONTENT` | Remove `with no I/O of its own` from `core/README.md` | P3-1 |
| S671 | Normal | `G-RD-DOC-CONTENT` | Remove `phrases/ja.ftl` from `stdlib/README.md` | P1-2 |
| S672 | Docs match | `G-RD-DOC-EXAMPLES` | Change `rolling wedges` in the rendering's expected output to `wedges` | P1-3 |
| S692 | Docs match | `G-RD-RENDER-SAME` | Change `slice` to `dice` in the example of `renderers/README.md`, and make the expected output match the actual output | P3-1 |
| S693 | Docs match | `G-RD-DOC-EXAMPLES` | Change the run line of the schedule example in `cli/README.md` to `<!-- run: cat kitchen.ozen -->`, and make the output block the contents of `kitchen.ozen` | P3-1 |
| S693 | Docs match | `G-RD-DOC-EXAMPLES` | Change both the version in `package.ozen` of `cli/README.md` and the `from:` version in the bundle example's expected output to `0.2.0` | P3-1 |
| S673 | Normal | `G-RD-DOC-CONTENT` | Remove the `falls back to English` sentence from `renderers/README.md` | P1-3 |
| S673 | Normal | `G-RD-DOC-CONTENT` | Remove `and runs in node and in browsers` from `renderers/README.md` | P3-1 |
| S674 | Normal | `G-RD-DOC-CONTENT` | Remove the digest sentence from `bindings/README.md` | P1-4 |
| S675 | Normal | `G-RD-DOC-CONTENT` | Remove the `four papers` sentence from `spec/README.md` | P1-4 |
| S676 | Normal | `G-RD-DOC-CONTENT` | Remove the arguments from the golden command in `CONTRIBUTING.md` | P1-6 |
| S676 | Normal | `G-RD-DOC-CONTENT` | Change the 4-configuration sentence in `CONTRIBUTING.md` to "builds the core with ASan and UBSan" | P3-1 |
| S676 | Normal | `G-RD-DOC-CONTENT` | Change `--only core-gcc-release` in `CONTRIBUTING.md` to `--only core-gcc-relase` | P3-1 |
| S677 | Data completeness | `G-RD-CI-JOBS` | Remove the `fuzz` job from `CONTRIBUTING.md` | P1-6 |
| S688 | Normal | `G-RD-DOC-CONTENT` | Remove the `.github/workflows/pages.yml` sentence from `site/README.md` | P1-5 |
| S678 | Data completeness | `G-RD-TERMS` | Remove `bindings/wasm/worker.js` from the 9 documents | P2-1 |
| S679 | Data completeness | `G-RD-LINKS` | Add `[x](missing.md)` to `cli/README.md` | P2-1 |
| S679 | No-effect condition | `G-RD-LINKS` | Add `[build](build/core-gcc-release/ozen)` to `CONTRIBUTING.md` (a target that exists but git ignores) | P2-1 |
| S680 | No-effect condition | `G-RD-NO-EMPTY-PARENS` | Change `recipe tea` in the bundle example of `cli/README.md` to `recipe tea()` | P2-1 |
| S681 | No-effect condition | `G-RD-ENGLISH` | Add `日本食品標準成分表` to `spec/README.md` | P2-1 |
| S682 | Side effects | `G-RD-SCOPE` | Change one file in `samples/` in a scratch commit (undo after checking) | P3-2 |
| S694 | Fixed premise | `G-RD-SCOPE` | Add a commit not in the range (`da0fb67`) to an exclusion line in §9 | P3-2 |
| S683 | Fixed premise | `G-RD-SCOPE` | Temporarily move the tag `readme-start` to a different name (restore after checking) | P3-2 |
| S684 | Side effects | `G-RD-ADD-ONLY` | Remove one line of `goal_G_REPO_README_LICENSE` in a scratch commit (undo after checking) | P3-2 |
| S685 | Side effects | `G-RD-VERSION` | Set `VERSION` to `0.1.1` (restore after checking) | P3-2 |
| S686 | Normal | `G-RD-CLAUDEMD` | Remove `readme-start` from this plan's line in `CLAUDE.md` | P0-3 |
| S687 | Real path | `G-RD-GH-READMES` | — | P3-2 |

Note on how to break S650: change `cut(slice, 3 mm)` in the example to `cut(dice, 3 mm)` or similar, and fix the expected output to match the actual output; then `G-RD-EXAMPLE` still passes and only the comparison with the old README fails.

S687 looks at the state of GitHub after the push, so it gets no mutation check (breaking it would require pushing a broken document).

The coverage check (run after P0-3 and in P3-1; `G-RD-COVERAGE`).

Findings of the independent audit (subagent) that were not taken in, or only partly, and why:

- A check that link anchors (`#節`, a section anchor) in the documents exist — the target documents are expected to have almost no links to anchors, and reproducing GitHub's rules for making anchors from headings is not worth the effort. If anchors are used, a check is added in P3-1
- A goal that fixes the facts of §3.4 (such as `stdlib/README.md` not being embedded in the compiler) — if they break, `G-CORE-BUILD`, `G-CORE-GOLDEN` and `G-REPO-SPDX` fail, so it is detected indirectly
- A check of reference-style links (`[x]: docs/…`) and links of the form `<https://…>` — the S640 check, which looks for the file names as text, detects them whatever the link form (the P3-1 audit found this does not hold for links to folders, so checks for URLs whatever the link form and for reference-style links were added to `G-RD-NO-PLAN-LINKS`)

---

## 6. Phases and tasks

Notation:

- A **completion condition** is a §5 goal ID or a command with a single pass/fail result. The "run locally" prerequisite goals of §5 are implicitly part of every task. The prerequisite goals for after the push are part of the completion condition only of the gate that pushes.
- Form of status reports: "README and target documents: `G-RD-*` pass. Prerequisite goals: pass. GitHub: check `G-PLAT-CI` and `G-RD-GH-READMES` after the push".
- Commits are logical units (one document, or one addition of goals). Pushes happen without asking (D60), but `G-REPO-NOREPLY` runs before each one, and the pushed range is reported.
- Each task's "mutation check" confirms that breaking things the coverage table's way makes that goal fail and that restoring makes it pass, and records it in §9 on a `- Mutation check: ` line.
- The target documents are made first (Phase 1), and the README is shortened later (Phase 2). At no point is an explanation left nowhere.
- Uncommitted changes of other work (§3.7) are not included in commits. `git add` names the files.

### Phase 0 — Decisions and preparation (no document changes)

| ID | Content | Completion condition |
| --- | --- | --- |
| P0-1 | Settle D60 to D66 of §2 with the user and write them into this file | `G-RD-DECISIONS` |
| P0-2 | Confirm that other work's uncommitted changes are cleared. If not, ask the user (§3.7, R3). Commit and push this file, and set the tag `readme-start` on `main` (D60). Record the baseline (pass/fail of the prerequisite goals, `$OLD \| wc -l`, and the number of examples from `check-readme-example.sh --readme README.md`) in §9 | `git status --porcelain -- stdlib tests core cli renderers spec \| wc -l` is 0, `git rev-parse --verify refs/tags/readme-start` succeeds, and all local prerequisite goals of §5 pass |
| P0-3 | Add `PLAN_RD`, `RD`, `DOCS_RD`, `OZ`, `OLD` and `goal_G_RD_*` to `scripts/goals.sh`. Add this file's line to the Plan section of `CLAUDE.md` (D61) | `scripts/goals.sh --list \| grep -c '^G-RD-'` is 31, and `G-RD-CLAUDEMD`, `G-RD-COVERAGE`, `G-RD-ADD-ONLY`, `G-REPO-CLAUDEMD`, `G-REPO-SELFTESTS` pass. Mutation check of S686 |

At P0-3 the documents have not changed yet, so many of the goals that look at the README's shape and the target documents fail (`G-RD-TERMS` passes because the terms are in the old README). Record in §9 the list of goals that failed at P0-3 (evidence that the goals check something).

### Phase 1 — Make the target documents (D58)

In each task, `G-RD-DOC-CONTENT` means that the §5 command, narrowed to the pairs of the task's own files, counts 0.

| ID | Content | Completion condition |
| --- | --- | --- |
| P1-1 | `cli/README.md`: command syntax (the old README's block as is), levels from the kitchen and today's conditions, the schedule example and how to read it, how bundling works and its example | The cli line of `G-RD-FACTS`, line 1 of `G-RD-DOC-EXAMPLES`, the cli pairs of `G-RD-DOC-CONTENT`, the cli part of `G-RD-DOC-TITLES`, and `G-RD-LINKS`, `G-RD-ENGLISH`, `G-RD-NO-EMPTY-PARENS`. Mutation checks of S663 and S665 to S669 |
| P1-2 | `core/README.md` (the core, the public API, schemas, SARIF) and `stdlib/README.md` (contents of `std`, the list of definitions, sources, phrases, provisional values) | The pairs of the 2 files in `G-RD-DOC-CONTENT`, the 2 files in `G-RD-DOC-TITLES`, and `G-RD-LINKS`, `G-RD-ENGLISH`. Mutation checks of S662, S670, S671 |
| P1-3 | `renderers/README.md`: kinds of output, where it runs, `bundle.mjs`, Fluent, how phrases are layered, and the rendering example of soup without `menu` | Line 2 of `G-RD-DOC-EXAMPLES`, the renderers pairs of `G-RD-DOC-CONTENT`, the renderers part of `G-RD-DOC-TITLES`, and `G-RD-LINKS`, `G-RD-ENGLISH`, `G-RD-NO-EMPTY-PARENS`. Mutation checks of S672 and S673 |
| P1-4 | `bindings/README.md` (the 4 platforms) and `spec/README.md` (the vocabulary table check block as is, the food composition table, the research log, the mapping to prior work) | The spec line of `G-RD-FACTS`, the pairs of the 2 files in `G-RD-DOC-CONTENT`, the 2 files in `G-RD-DOC-TITLES`, and `G-RD-LINKS`, `G-RD-ENGLISH`. Mutation checks of S664, S674, S675 |
| P1-5 | `site/README.md`: the introduction page, no build step, the font, publishing to GitHub Pages, copying the examples by hand (D67) | The site pairs of `G-RD-DOC-CONTENT`, the site part of `G-RD-DOC-TITLES`, and `G-RD-LINKS`, `G-RD-ENGLISH`. Mutation check of S688 |
| P1-6 | `CONTRIBUTING.md` (the 4 build configurations, golden, goals, CI jobs) | `G-RD-DOC-CONTENT` (all pairs), `G-RD-DOC-TITLES` (all documents), and `G-RD-FACTS`, `G-RD-CI-JOBS`, `G-RD-LINKS`, `G-RD-ENGLISH`. Mutation checks of S676 and S677 |

### Phase 2 — Shorten the README (D57, D59, D62 to D65)

| ID | Content | Completion condition |
| --- | --- | --- |
| P2-1 | Rewrite the README into the shape of §1 | `G-RD-HEADINGS`, `G-RD-NO-PLAN-LINKS`, `G-RD-NO-OZENDATE`, `G-RD-INTRO`, `G-RD-EXAMPLE`, `G-RD-PIPE-ALL`, `G-RD-RENDER-SAME`, `G-RD-BUILD`, `G-RD-BUILD-RUN`, `G-RD-DOC-LINKS`, `G-RD-LICENSE`, `G-RD-NOTICE`, `G-RD-GFM`, `G-RD-TERMS`, `G-RD-LINKS`, `G-RD-NO-EMPTY-PARENS`, `G-RD-ENGLISH`, and the prerequisites `G-REPO-README-LICENSE`, `G-RF-README-EXAMPLE`, `G-RF-README-PIPE`, `G-RF-README-NO-PROTO`, `G-RF-NO-EMPTY-PARENS`. Mutation checks of S637, S639 to S661, S678 to S681, S689 |

### Phase 3 — Finishing

| ID | Content | Completion condition |
| --- | --- | --- |
| P3-1 | Coverage audit: add the facts that grew in §9 to the coverage table. Judge each item of §8 again, with what we know after writing, on whether a command can decide it. Have an independent subagent read §1, §4, §8, the coverage table, the 9 documents and the old README, and list claims without a goal and explanations that vanished from the old README. Each finding is either added, or a reason for not adding it is written at the end of §5.3 | `G-RD-COVERAGE`, `G-RD-PLAN-TESTS`, `G-RD-AUDIT` |
| P3-2 | Final gate. Do the mutation checks of S682 to S685. Run `G-REPO-NOREPLY`, push, and check CI and GitHub's README resolution. Hand §8 to the user | All §5 goals including `G-RD-SCOPE`, `G-RD-ADD-ONLY`, `G-RD-VERSION`, the local prerequisite goals, and after the push `G-PLAT-CI` and `G-RD-GH-READMES` |

---

## 7. Dependencies between phases

- P0-1 → P0-2 → P0-3 in that order. Without the P0-3 goals, the completion conditions of Phase 1 cannot be judged. P0-2 may have to wait for other work's uncommitted changes to be cleared.
- P1-1 to P1-6 are independent of each other (the files they touch do not overlap). However, `scripts/goals.sh` is finished in P0-3 and not touched in Phase 1. The completion condition of P1-6 looks at all documents, so P1-6 goes last.
- P2-1 comes after all of Phase 1 (`G-RD-TERMS` passes because the terms removed from the README are in the target documents).
- P3-1 → P3-2 in that order.

---

## 8. Checks handed over to the user

Claude is responsible up to the machine-checked goals. The user checks the following. Each item gives the reason a command cannot decide it. An item for which no reason can be written becomes a test in §5.3.

- Whether the README's opening 2 to 3 sentences tell a first-time reader what Ozen is (the number of sentences and the presence of the 3 pillars of the types and the schedule term are checked by `G-RD-INTRO`) — Reason a command cannot decide: whether an explanation gets across is up to the reader
- Whether the English of the 8 target documents reads naturally (the facts staying word for word is checked by `G-RD-FACTS`, `G-RD-DOC-CONTENT`, `G-RD-TERMS`) — Reason a command cannot decide: natural writing is a human judgment
- Whether the layout of the README's example, tables and links on the GitHub repository page is easy to read (the number of headings, hidden annotations and the resolution of folder READMEs are checked by `G-RD-GFM` and `G-RD-GH-READMES`) — Reason a command cannot decide: whether a layout is easy to read is a human judgment

---

## 9. Items confirmed and recorded at the start

(The results confirmed in each task are appended here with dates)

- Independent audit after writing the plan (2026-11-01): 28 findings. 26 were taken in as they were and 2 partly (the parts not taken and why are at the end of §5.3). The main changes: limiting the heading judgment to outside code blocks; adding the tag check to `G-RD-SCOPE`; dropping `cat … | grep`, which gives wrong results under `pipefail`; adding `G-RD-FACTS` and `G-RD-LICENSE`, which compare the old README's command blocks and license table word for word; moving `G-RD-GFM` and `G-RD-GH-READMES`, which check rendering and folder READMEs through the GitHub API, out of §8; and widening the list of terms for the target documents
- Baseline at the starting point (P0-2, 2026-11-01): the tag `readme-start` is `da0fb67`. All 62 local prerequisite goals passed (`da0fb67`, clean working tree, about 67 minutes). The old README is 264 lines (`$OLD | wc -l`), with 3 examples (`check-readme-example.sh --readme README.md` gives `readme: 3 examples, 0 mismatches`)
- Moving the tag (P0-2, 2026-11-01): the first tag was set on `f0e894a`, the commit of this file. After that, other work put 17 commits into `stdlib/`, `tests/golden/`, `scripts/goals.sh`, `.github/workflows/ci.yml`, `plans/ozen-roadmap.md` and others (`911bb35` to `da0fb67`). With those in the range, `G-RD-SCOPE` and `G-RD-ADD-ONLY` would fail for reasons unrelated to this plan's work, so after asking the user we moved the tag to `da0fb67` and also overwrote the remote tag (other work had moved it to `626be31` in the meantime). The old README's content is the same as at `f0e894a`. The first run of the prerequisite goals (after `f0e894a`) was not used, because the working tree and `goals.sh` changed during the run; it was rerun at `da0fb67`. To avoid changes during the run, the rerun executed a copy from `git show HEAD:scripts/goals.sh` with `--root`
- `G-RD-*` at P0-3 (2026-11-01, before changing documents): of 31, 19 failed and 12 passed. Failed: `G-RD-PLAN-TESTS` (`mutations: 57 rows, 0 recorded`), `HEADINGS` (the old README's 10 sections), `NO-PLAN-LINKS` (2 and 2), `NO-OZENDATE` (2), `INTRO` (4 sentences), `EXAMPLE` (4 `ozen` blocks, 3 examples), `RENDER-SAME` (no new rendering example; on the old README's side, 10 lines were extracted), `BUILD` (no Build section), `DOC-LINKS`, `GFM` (10 `<h2`), `DOC-TITLES` (8), `FACTS` (11 missing; on the old README's side, 11 lines were extracted), `DOC-EXAMPLES` (no documents), `DOC-CONTENT` (72 missing), `CI-JOBS` (5), `NO-EMPTY-PARENS` (no documents, so the check prints nothing), `ENGLISH` (1 line of the README), `GH-READMES` (7), `AUDIT`. Passed: `DECISIONS`, `COVERAGE`, `BUILD-RUN`, `LICENSE` (the old README already meets it), `NOTICE`, `LINKS`, `PIPE-ALL` (the old README's examples also use pipes everywhere), `TERMS`, `SCOPE`, `ADD-ONLY`, `VERSION`, `CLAUDEMD`. As §6 expected (`TERMS` passes)
- Mutation check: S686, G-RD-CLAUDEMD (P0-3, 2026-11-01). Removing ", and its history range starts at the tag `readme-start`" from this plan's line in `CLAUDE.md` made it fail with `plan line in CLAUDE.md: got '0'`. Restoring it made it pass
- Mutation check: S637, G-RD-HEADINGS (P2-1, 2026-11-01). Putting the `## Credits` heading back before License in the README made it fail. Restoring it made it pass
- Mutation check: S637, G-RD-HEADINGS (P2-1, 2026-11-01). Changing the heading judgment to `grep '^## '` in a copy of `goals.sh` (it became `Ingredients Steps Build …`) made it fail. Restoring it made it pass
- Mutation check: S639, G-RD-NO-PLAN-LINKS (P2-1, 2026-11-01). Putting `[docs/concept.md](docs/concept.md)` back in the README made it fail. Restoring it made it pass
- Mutation check: S639, G-RD-NO-PLAN-LINKS (P2-1, 2026-11-01). Adding a link to `https://github.com/atsuki-seo/Ozen/tree/main/plans/` to the README made it fail. Restoring it made it pass
- Mutation check: S640, G-RD-NO-PLAN-LINKS (P2-1, 2026-11-01). Adding `Design concept: docs/concept.md`, not as a link, to the README made it fail. Restoring it made it pass
- Mutation check: S641, G-RD-NO-OZENDATE (P2-1, 2026-11-01). Adding an Ozendate sentence to `stdlib/README.md` made it fail. Restoring it made it pass
- Mutation check: S642, G-RD-INTRO (P2-1, 2026-11-01). Changing `cooking procedures` in the opening paragraph to `recipes` made it fail. Restoring it made it pass
- Mutation check: S643, G-RD-INTRO (P2-1, 2026-11-01). Removing `linear types` from the opening paragraph made it fail. Restoring it made it pass
- Mutation check: S644, G-RD-INTRO (P2-1, 2026-11-01). Removing the schedule sentence from the opening paragraph made it fail. Restoring it made it pass
- Mutation check: S645, G-RD-INTRO (P2-1, 2026-11-01). Adding a 4th sentence to the opening paragraph made it fail. Restoring it made it pass
- Mutation check: S646, G-RD-EXAMPLE (P2-1, 2026-11-01). Adding a `kitchen.ozen` block to the README made it fail. Restoring it made it pass
- Mutation check: S647, G-RD-EXAMPLE (P2-1, 2026-11-01). Moving the example after the Build section made it fail. Restoring it made it pass
- Mutation check: S648, G-RD-PIPE-ALL (P2-1, 2026-11-01). Changing the carrot line to `cut(peel(carrot), rollCut, 20 mm)` (at first the goal still passed, so this was after adding the check for nested calls to the goal) made it fail. Restoring it made it pass
- Mutation check: S648, G-RF-README-PIPE (P2-1, 2026-11-01). Turning every pipe in the example into function-call form made it fail. Restoring it made it pass
- Mutation check: S649, G-RD-EXAMPLE (P2-1, 2026-11-01). Changing `(3 mm)` in the expected output to `(4 mm)` made it fail. Restoring it made it pass
- Mutation check: S649, G-RF-README-EXAMPLE (P2-1, 2026-11-01). Removing the 3rd step line from the expected output made it fail. Restoring it made it pass
- Mutation check: S650, G-RD-RENDER-SAME (P2-1, 2026-11-01). Changing `slice` in the example to `dice` and making the expected output match the actual output made it fail. Restoring it made it pass
- Mutation check: S651, G-RD-BUILD (P2-1, 2026-11-01). Making the Build section's command just `scripts/build-core.sh` made it fail. Restoring it made it pass
- Mutation check: S652, G-RD-BUILD-RUN (P2-1, 2026-11-01). Narrowing the `core-gcc-*` case in `build-core.sh` to `core-gcc-debug-*` (`core-build: 1 configs, 1 failed`) made it fail. Restoring it made it pass
- Mutation check: S653, G-RD-BUILD (P2-1, 2026-11-01). Moving the link to `toolchain.txt` to the Documentation section made it fail. Restoring it made it pass
- Mutation check: S654, G-RD-BUILD (P2-1, 2026-11-01). Removing the node sentence from the Build section made it fail. Restoring it made it pass
- Mutation check: S655, G-RD-DOC-LINKS (P2-1, 2026-11-01). Removing the link to `spec/README.md` from the Documentation section made it fail. Restoring it made it pass
- Mutation check: S689, G-RD-DOC-LINKS (P2-1, 2026-11-01). Removing the introduction page link from the Documentation section made it fail. Restoring it made it pass
- Mutation check: S656, G-RD-LICENSE (P2-1, 2026-11-01). Removing the `*/vendor/` row from the license table made it fail. Restoring it made it pass
- Mutation check: S656, G-REPO-README-LICENSE (P2-1, 2026-11-01). Removing the `docs/` row from the license table made it fail. Restoring it made it pass
- Mutation check: S657, G-RD-LICENSE (P2-1, 2026-11-01). Moving the sentence on output rights after the Build section made it fail. Restoring it made it pass
- Mutation check: S658, G-RD-LICENSE (P2-1, 2026-11-01). Removing the link to `LICENSE-CC-BY-4.0` made it fail. Restoring it made it pass
- Mutation check: S659, G-RD-LICENSE (P2-1, 2026-11-01). Removing the link to `NOTICE` in the License section made it fail. Restoring it made it pass
- Mutation check: S660, G-RD-NOTICE (P2-1, 2026-11-01). Removing the Fluent line from `NOTICE` made it fail. Restoring it made it pass
- Mutation check: S661, G-RD-GFM (P2-1, 2026-11-01). Indenting the `<!-- file: soup.ozen -->` line by 4 spaces (1 `&lt;!--`) made it fail. Restoring it made it pass
- Mutation check: S678, G-RD-TERMS (P2-1, 2026-11-01). Removing `bindings/wasm/worker.js` from `bindings/README.md` made it fail. Restoring it made it pass
- Mutation check: S679, G-RD-LINKS (P2-1, 2026-11-01). Adding `[x](missing.md)` to `cli/README.md` made it fail. Restoring it made it pass
- Mutation check: S679, G-RD-LINKS (P2-1, 2026-11-01). Adding `[build](build/core-gcc-release/ozen)` to `CONTRIBUTING.md` made it fail. Restoring it made it pass
- Mutation check: S680, G-RD-NO-EMPTY-PARENS (P2-1, 2026-11-01). Changing `recipe tea` in `cli/README.md` to `recipe tea()` made it fail. Restoring it made it pass
- Mutation check: S681, G-RD-ENGLISH (P2-1, 2026-11-01). Adding the Japanese name of the food composition table to `spec/README.md` made it fail. Restoring it made it pass
- Mutation check: S662, G-RD-DOC-TITLES (P1-2, 2026-11-01). Removing the heading on line 1 of `core/README.md` made it fail. Restoring it made it pass
- Mutation check: S663, G-RD-FACTS (P1-1, 2026-11-01). Changing `ozen unbundle FILE [-o DIR]` in `cli/README.md` to `ozen unbundle FILE` made it fail. Restoring it made it pass
- Mutation check: S664, G-RD-FACTS (P1-4, 2026-11-01). Removing the arguments of the `sample` command in `spec/README.md` made it fail. Restoring it made it pass
- Mutation check: S665, G-RD-DOC-CONTENT (P1-1, 2026-11-01). Removing `level 1` from `cli/README.md` made it fail. Restoring it made it pass
- Mutation check: S666, G-RD-DOC-EXAMPLES (P1-1, 2026-11-01). Changing `total 1560` in the schedule's expected output to `total 1561` made it fail. Restoring it made it pass
- Mutation check: S667, G-RD-DOC-EXAMPLES (P1-1, 2026-11-01). Removing the `from: kitchenBook "0.1.0"` line from the bundle example's expected output made it fail. Restoring it made it pass
- Mutation check: S668, G-RD-DOC-CONTENT (P1-1, 2026-11-01). Removing the sentences on reading the schedule output from `cli/README.md` made it fail. Restoring it made it pass
- Mutation check: S669, G-RD-DOC-CONTENT (P1-1, 2026-11-01). Removing `never overwrites` from `cli/README.md` made it fail. Restoring it made it pass
- Mutation check: S670, G-RD-DOC-CONTENT (P1-2, 2026-11-01). Removing `2020-12` from `core/README.md` made it fail. Restoring it made it pass
- Mutation check: S671, G-RD-DOC-CONTENT (P1-2, 2026-11-01). Removing `phrases/ja.ftl` from `stdlib/README.md` made it fail. Restoring it made it pass
- Mutation check: S672, G-RD-DOC-EXAMPLES (P1-3, 2026-11-01). Changing `rolling wedges` in the rendering's expected output to `wedges` made it fail. Restoring it made it pass
- Mutation check: S673, G-RD-DOC-CONTENT (P1-3, 2026-11-01). Removing the `falls back to English` sentence from `renderers/README.md` made it fail. Restoring it made it pass
- Mutation check: S674, G-RD-DOC-CONTENT (P1-4, 2026-11-01). Removing the digest sentence from `bindings/README.md` made it fail. Restoring it made it pass
- Mutation check: S675, G-RD-DOC-CONTENT (P1-4, 2026-11-01). Removing the `four papers` sentence from `spec/README.md` made it fail. Restoring it made it pass
- Mutation check: S676, G-RD-DOC-CONTENT (P1-6, 2026-11-01). Removing the arguments from the golden command in `CONTRIBUTING.md` made it fail. Restoring it made it pass
- Mutation check: S677, G-RD-CI-JOBS (P1-6, 2026-11-01). Removing the `fuzz` job from `CONTRIBUTING.md` made it fail. Restoring it made it pass
- Mutation check: S688, G-RD-DOC-CONTENT (P1-5, 2026-11-01). Removing `.github/workflows/pages.yml` from `site/README.md` made it fail. Restoring it made it pass
- Mutation check: S682, G-RD-SCOPE (P3-2, 2026-11-01). Changing one file in `samples/` in a scratch commit (undone with `git reset --hard HEAD~1`) made it fail. Restoring it made it pass
- Mutation check: S683, G-RD-SCOPE (P3-2, 2026-11-01). Moving the tag `readme-start` to a different name locally (still `da0fb67` after restoring) made it fail. Restoring it made it pass
- Mutation check: S684, G-RD-ADD-ONLY (P3-2, 2026-11-01). Removing one line of `goal_G_REPO_README_LICENSE` in a scratch commit (undone) made it fail. Restoring it made it pass
- Mutation check: S685, G-RD-VERSION (P3-2, 2026-11-01). Setting `VERSION` to `0.1.1` made it fail. Restoring it made it pass
- Ways of breaking and fixes to goals (2026-11-01): 3 of the coverage table's methods did not cause a failure at first. For S648, `cut(peel(carrot), …)` is not caught by the regular expression of `G-RD-PIPE-ALL` (which requires a space after the name), so we added a check that counts `let` lines taking a name or a nested call as the first argument (the goal was weak). For S652, `build-core.sh --only` does not look at the default list, so removing it from the list broke nothing; we switched to narrowing the case. For S661, an annotation inside a paragraph stays hidden in GFM too, so we switched to making it an indented code block (both of these were mistakes in the way of breaking; the goals were not changed)
- Judgment for P1 and P2 (2026-11-01): after writing the 8 documents and the README, the 28 `G-RD-*` goals other than `G-RD-GH-READMES`, `G-RD-AUDIT` and `G-RD-PLAN-TESTS`, and `G-REPO-README-LICENSE`, `G-RF-README-EXAMPLE`, `G-RF-README-PIPE`, `G-RF-README-NO-PROTO`, `G-RF-NO-EMPTY-PARENS` all passed. The README is 77 lines
- Mutation check: S639, G-RD-NO-PLAN-LINKS (P3-1, 2026-11-01). Adding `[Plans](plans)` to the README made it fail. Restoring it made it pass
- Mutation check: S639, G-RD-NO-PLAN-LINKS (P3-1, 2026-11-01). Adding `<https://github.com/atsuki-seo/Ozen/tree/main/docs/>` to the README made it fail. Restoring it made it pass
- Mutation check: S690, G-RD-NO-MOVED (P3-1, 2026-11-01). Adding a `### Status` section to the README made it fail. Restoring it made it pass
- Mutation check: S690, G-RD-NO-MOVED (P3-1, 2026-11-01). Pasting the command-syntax block without a language name into the README's Build section made it fail. Restoring it made it pass
- Mutation check: S691, G-RD-NO-MOVED (P3-1, 2026-11-01). Adding a `provisionalValue` sentence to the README made it fail. Restoring it made it pass
- Mutation check: S644, G-RD-INTRO (P3-1, 2026-11-01). Removing `an IR and` from the opening paragraph made it fail. Restoring it made it pass
- Mutation check: S648, G-RD-PIPE-ALL (P3-1, 2026-11-01). Changing the carrot line to `cut( peel(carrot), rollCut, 20 mm)` made it fail. Restoring it made it pass
- Mutation check: S692, G-RD-RENDER-SAME (P3-1, 2026-11-01). Changing `slice` to `dice` in the example of `renderers/README.md` and making the expected output match the actual output made it fail. Restoring it made it pass
- Mutation check: S693, G-RD-DOC-EXAMPLES (P3-1, 2026-11-01). Changing the run line of the schedule example in `cli/README.md` to `cat kitchen.ozen` and making the output block the contents of `kitchen.ozen` made it fail. Restoring it made it pass
- Mutation check: S693, G-RD-DOC-EXAMPLES (P3-1, 2026-11-01). Changing the version of `package.ozen` in `cli/README.md` and the `from:` version in the expected output to `0.2.0` made it fail. Restoring it made it pass
- Mutation check: S668, G-RD-DOC-CONTENT (P3-1, 2026-11-01). Removing only `Times are in seconds.` from `cli/README.md` made it fail. Restoring it made it pass
- Mutation check: S669, G-RD-DOC-CONTENT (P3-1, 2026-11-01). Removing `, except `std`,` from `cli/README.md` made it fail. Restoring it made it pass
- Mutation check: S670, G-RD-DOC-CONTENT (P3-1, 2026-11-01). Removing `with no I/O of its own` from `core/README.md` made it fail. Restoring it made it pass
- Mutation check: S673, G-RD-DOC-CONTENT (P3-1, 2026-11-01). Removing `and runs in node and in browsers` from `renderers/README.md` made it fail. Restoring it made it pass
- Mutation check: S676, G-RD-DOC-CONTENT (P3-1, 2026-11-01). Changing the 4-configuration sentence in `CONTRIBUTING.md` to "builds the core with ASan and UBSan" made it fail. Restoring it made it pass
- Mutation check: S676, G-RD-DOC-CONTENT (P3-1, 2026-11-01). Changing `--only core-gcc-release` in `CONTRIBUTING.md` to `--only core-gcc-relase` made it fail. Restoring it made it pass
- Coverage audit: 15 findings, 15 taken (2026-11-01, P3-1, independent subagent). For the 12 cases where a goal was weaker than its claim, we strengthened `G-RD-NO-PLAN-LINKS` (links to folders, URLs whatever the link form, reference-style links), `G-RD-INTRO` (`IR`), `G-RD-PIPE-ALL` (nested calls with a space inside), `G-RD-DOC-EXAMPLES` (fixing the run lines and comparing with the old README), `G-RD-RENDER-SAME` (comparing with `renderers/README.md`) and `G-RD-DOC-CONTENT` (15 more pairs for cli, core, renderers and CONTRIBUTING), and added `G-RD-NO-MOVED` (S690, S691), which checks that removed explanations do not come back to the README. The 1 explanation that vanished from the old README (the "implemented and tested" sentence of the Status section) is removed on purpose together with the whole section by D65, so we stated it in §4.5. The 2 factual mismatches (the "called on other platforms" in `core/README.md`, and the "outputs" only explanation in `cli/README.md`) were fixed in the documents. The 2 points the audit raised for reference (the `fuzz/` of the license table not existing at the root, and the example check not running in CI) are existing matters inherited from the old README and were put outside this plan's scope
- Final gate (P3-2, 2026-11-01, after `ca0c4a0`, HEAD at `83db656`): of the 62 local prerequisite goals and 31 `G-RD-*` goals, 91 passed and 2 failed (about 65 minutes). `G-RD-SCOPE` failed because other work's commit `83db656` (splitting the examples of `site/index.html`) entered the range (R3); the only file outside the scope was `site/index.html`. `G-CORE-FUZZ` failed because `fuzz-schedule` hit the 5-second timeout while loading the corpus (artifact `build/fuzz-core/artifacts/schedule-timeout-bacbf149…`; an input with `capacity: 001 g` that becomes 500 rounds of `repeat`, and even run alone it is cut off at 30 seconds). No code of core, cli, stdlib or renderers changed in the range (only each folder's `README.md` changed); the slowness of the scheduler, there since before this work started, showed up with the grown corpus. It is outside this plan's scope (documents only, S682), so we did not fix it and report it to the user. The push had already been done by the other work together with `83db656`. `G-REPO-NOREPLY`, `G-RD-GH-READMES` and `G-PLAT-CI` (all ci jobs of `83db656` succeeded) passed
- Other work's commit in range: 83db656 (splits the introduction page examples of `site/index.html` into a recipe and a menu. D68)
- Other work's commit in range: 0222e12 (adds a unit test checking that the schedule of a dish made in many batches finishes in time. A separate session fixing the `G-CORE-FUZZ` hang, D68)
- Other work's commit in range: 3fa32b5 (counts the schedule computation against the evaluation fuel. A separate session fixing the `G-CORE-FUZZ` hang, D68)
- Other work's commit in range: e612365 (writes in the roadmap that the schedule uses fuel. A separate session fixing the `G-CORE-FUZZ` hang, D68)
- Other work's commit in range: 95c2855 (adds a line for the plating time to the schedule chart of `site/index.html`. D68)
- Mutation check: S694, G-RD-SCOPE (P3-2, 2026-11-01). Adding `da0fb67`, which is not in the range, to an exclusion line in §9 in a temporary worktree made it fail. Restoring it made it pass
- Rechecking S682 after changing `G-RD-SCOPE` to the D68 form (2026-11-01): changing one file in `samples/` in a scratch commit in a temporary worktree made it fail with `files outside scope: got '1'`, and undoing it made it pass (done in a worktree to keep the working tree clean, since the fixing session uses the same folder)
- Rerun of the final gate (P3-2, 2026-11-01): after another session fixed the core hang (`0222e12`, `3fa32b5`, `e612365`), we ran the 62 local prerequisite goals, the 32 `G-RD-*` goals and `G-REPO-NOREPLY` at `fa5284c`, and 94 including `G-CORE-FUZZ` passed (0 `fuzz-schedule` timeouts, 0 artifacts). The failing `G-RD-SCOPE` was due to other work's `95c2855` (`site/index.html`), which came in during the run, and following D68 it was added to an exclusion line. At the final `9610abd`, core had not changed, so we ran the 94 goals other than `G-CORE-FUZZ` (including `G-RD-GH-READMES` and `G-REPO-NOREPLY`), and all passed. `G-PLAT-CI` passed by looking at the ci of `fa5284c` (all jobs succeeded), since CI does not start for `site/**` and `plans/**`
- Mutation check records (each task): write them in the form `- Mutation check: S<n>・<ゴール>（<タスク>、<日付>）。<壊し方> で落ちた。戻すと合格した`
- Coverage audit (P3-1): write it in the form `- Coverage audit: <k> findings, <m> taken`
- Closing the range (D87 of the quantity checks plan `plans/ozen-quantity-checks.md`, 2026-11-02): `RD=readme-start..HEAD` kept counting commits after this plan ended, and `G-RD-SCOPE` was failing due to later work (`2332ca6`, `8db17e6` and others). We set the tag `readme-end` on this plan's last commit `52a6ef1` and closed `RD` as `readme-start..readme-end`. What `G-RD-SCOPE` and `G-RD-ADD-ONLY` are meant to check does not change

---

## 10. Risks

- **R1. Explanations disappear without reaching the target documents**: when shortening the README, an explanation may end up in no document. `G-RD-TERMS` and `G-RD-DOC-CONTENT` check the old README's specific terms, `G-RD-FACTS` and `G-RD-LICENSE` check the command blocks and the table rows word for word, and the P3-1 audit also looks for explanations not in the term list.
- **R2. Example output changes with the implementation**: if `std` or rendering changes later, the examples in the target documents will fail too. `G-RD-EXAMPLE` and `G-RD-DOC-EXAMPLES` detect it. Whether to fix the document or the implementation is decided by the plan at that time.
- **R3. Sharing the same `main` and the same working tree with other work**: as of 2026-11-01, `stdlib/`, `tests/golden/` and `spec/` have uncommitted changes from other work (§3.7). `ozen` is built from this working tree, so the example output may be affected by those changes. P0-2 confirms they are cleared, and if not, asks the user. Look at `git log` and `git status` at the start of each phase and before commits. `G-RD-SCOPE` fails when other work's commits enter the range, so when it fails, check whether the cause is our own change before reporting to the user.
- **R4. Links to README sections are used elsewhere**: links pointing to the old README's headings (such as `#bundles`) may exist outside the repository. There are none inside (`git grep 'README.md#'` gives 0 hits, 2026-11-01). Outside links cannot be checked, but the repository was published recently, so the impact is expected to be small.
- **R5. Goals that rely on the GitHub API**: `G-RD-GFM` and `G-RD-GH-READMES` need `gh` authentication and the network. When they are unavailable, treat the goal as failed and report the reason (do not treat it as passed).
