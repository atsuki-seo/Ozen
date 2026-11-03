# Plan for agent rules, development setup, English documents, the reference and releases

- Created: 2026-11-03
- Source: none (no upstream code). The requirements are the decisions EQ1–EQ13 in `plans/ozen-dev-environment.md` (the list of candidates for documents and development environments, decided with the user one by one on 2026-11-02). That list took over C13, C23 and Q4 from `plans/ozen-authoring-support.md`. On 2026-11-03 the user narrowed EQ4 and EQ11: move to English as far as possible and add no new Japanese text, so the reference, the AI guide and the `AGENTS.md` template are English only.
- Target: `/home/atsuki/ドキュメント/Ozen`, `main` (D116). The facts in §3 were read at `8f419ad` and checked again at `f4f77c4`.
- Rules: `CLAUDE.md` (work on `main` only, run `G-REPO-NOREPLY` before every push, the language of committed files), `~/.claude/rules/repo-language.md`, `~/.claude/rules/git-commit.md`, `~/.claude/rules/comment-style.md`, `~/.claude/rules/github.md`. This plan is written in English because of EQ13 (committed documents that people review are written in English from now on), which overrides `~/.claude/rules/plan-language.md` for this repository. The goals in §5 of `plans/ozen-roadmap.md` (the roadmap), `plans/ozen-syntax-refresh.md` (the syntax plan), `plans/ozen-readme-compact.md` (the README plan), `plans/ozen-quantity-checks.md` (the quantity plan) and `plans/ozen-cooking-feedback.md` (the feedback plan) are prerequisite goals (§5).

## 0. How to read this plan

- This plan follows the feedback plan and continues its numbers. Design decisions start at **D116** and spec items at **S945** (the feedback plan ends at D115 and S944).
- Every completion condition is a command whose result decides pass or fail. Checks by people are not tasks; they go to the hand-over list in §8.
- Completion conditions refer to the prerequisite goals listed in §5 and to the `G-DS-*` goals defined in §5. The code blocks in §5 are the authoritative commands. Tables do not repeat them, because a `|` in a table cell becomes `\|`.
- The decisions D116–D136 in §2 were decided with the user one by one on 2026-11-03 (all 21 asked individually: D118 first, because the order decides the rest, then the others by number). D126–D129 and D135 were rewritten before they were asked, after the user chose English only for the new files. D128 was rewritten after the user asked how other tools behave. D131 and D134 follow the user's answer `v0.1.0`. Each section starts with a `- **Decision:**` line, and the comparison of the recommendation and the alternatives stays as history (P0-1).
- This is the first plan written in English. Its labels are the English labels of §4.5: `[decided]`, `- **Decision:**`, the columns `Test or goal` and `How to break it`, `- Mutation check:` and `Reason a command cannot decide:`. P0-3 teaches the plan checkers to read them next to the Japanese labels.
- Facts about the target were checked on 2026-11-02 and 2026-11-03. The checks read the files at `8f419ad` and `f4f77c4`, ran `build/core-gcc-release/ozen` built from the same sources, and read the published `rulesync` 43.4.2 package (§3).
- After writing, the plan had one independent audit by a subagent: 45 findings, all taken (§5.3, §9).
- Each checkable claim in §4 has a spec item ID. The coverage table in §5.3 assigns each S item a test, a goal, or a hand-over in §8 with its reason.

---

## 1. Purpose and scope

**Purpose**: Make the repository easy to start and to work in for people and AI agents, and make Ozen easy to install and to write with an AI agent. The work has six parts:

1. One source for the instructions to AI agents (`.rulesync/`). It is generated for Claude Code, `AGENTS.md`, Codex CLI, Cursor and GitHub Copilot.
2. A preparation script, a git pre-push hook for the publishing check, and the same check in CI.
3. The existing design documents and the finished plans in English, with every existing goal still giving the same result.
4. A reference for writing Ozen, in English (`reference/`). It is mostly generated, and it includes a one-file guide for AI agents.
5. `ozen init`, which starts a cookbook folder with `package.ozen` and a short `AGENTS.md`.
6. Official releases of `ozen` for Linux, macOS and WebAssembly on GitHub Releases when a version tag is pushed.

A cookbook started with `ozen init` looks like this. The wording comes from §4.8; the exact text is fixed by the template in P3-4.

```text
$ ozen init ourCookbook
created ourCookbook/package.ozen
created ourCookbook/AGENTS.md
If this folder already has a CLAUDE.md, add the line @AGENTS.md to it.
```

### In scope

- `.rulesync/`, `rulesync.jsonc`, the generated agent files, `scripts/rulesync.sh`, `scripts/agent-allow.txt`, `scripts/agent-outputs.txt`, and the `rulesync` and `node` lines of `toolchain.txt` (D119, D120, D121)
- The rewritten language rule (EQ12, EQ13), the push rule (D136) and the plan list. They move from `CLAUDE.md` to `.rulesync/rules/overview.md`.
- `.githooks/pre-push`, the changes to `G-REPO-NOREPLY` for CI (D122), and the workflow `.github/workflows/agents.yml`
- `scripts/setup.sh`
- Removing §16 of `docs/concept.md`, the entry sections of `CONTRIBUTING.md`, and the documentation list of `README.md`
- Plan labels in two languages for the plan checkers (D123), and closing the feedback plan's range at `fb-end` (D118)
- Translating `docs/concept.md`, `docs/prototype-report.md` and the four finished plans into English. This includes a translation checker (D124) and rewriting every goal that reads their text to the English text (D125).
- `reference/` and the generator `tools/refgen/` (D126, D127)
- `ozen init` (D128, D129)
- Version strings in golden expectations, `scripts/bump-version.sh`, and the version goals of finished plans (D132, D133)
- `scripts/check-release-tag.sh`, `scripts/make-release.sh`, `.github/workflows/release.yml`, and the first release (D130, D131, D134)
- Links from the introduction page to the reference and the releases (D135)

### Out of scope (reasons)

- **Japanese versions of the reference, the AI guide and the `AGENTS.md` template**: the user asked on 2026-11-03 to move to English and add no new Japanese text.
- **People who embed Ozen**: library packages for npm, the XCFramework and Android, and the C API reference. EQ1 left them for later.
- **Translating the plans that are not finished** (`plans/ozen-cooking-feedback.md`, `plans/ozen-authoring-support.md`, `plans/ozen-dev-environment.md`): the user said they stay Japanese (EQ13).
- **`ozen doc` and `ozen explain`**: candidates C12 and C7 of `plans/ozen-authoring-support.md`, decided there. The generator of this plan reads the same sources, so those commands can reuse it later.
- **devcontainer, Nix, mise**: not chosen (EQ8).
- **Nightly builds**: only official releases (EQ10).
- **Skills, subagents, MCP and agent hooks in `.rulesync/`**: not chosen (EQ7). The publishing check is a git hook.
- **The verifiable-plan skill of the user** (`~/.claude/skills/`), which writes Japanese labels. It is outside the repository, and changing it is the user's choice. This plan's checkers accept both label sets, so nothing here depends on it.
- **The red-first check for CLI tests**: `scripts/check-red-first.sh` passes `--only` to `scripts/run-cli-tests.sh`, which accepts exactly two arguments, so CLI expectations always count as red (§3.8). Fixing it is a separate change; §10 R8 records it.
- **Checks by people**: §8.

---

## 2. Design decisions

### D116. Where to work, the range base, and pushing [decided]

- **Decision:** As recommended: work on `main`, count from `ds-start`, push without asking each time after `G-REPO-NOREPLY`, and leave other work's commits out through `ds_excluded`. (2026-11-03, confirmed by the user)
- **Recommendation**: Work on `main` (`CLAUDE.md`). P0-2 tags `ds-start` on `main`, and the range of this plan is `DS=ds-start..HEAD`. Push without asking each time (the user's instruction for Ozen), and run `G-REPO-NOREPLY` before every push. Commits of other work that fall in the range are named in §9 as `- Other work's commit in range: <sha>`. The goals that use the range leave them out, through a function `ds_excluded` written like the feedback plan's `fb_excluded`. Temporary commits for mutation checks are made in a temporary worktree and removed; they are never pushed.
- Alternative: a branch `dev-setup` merged with `--no-ff` at the end. It keeps the work apart, but it goes against `CLAUDE.md` and the range style of the earlier plans.

### D117. Where the plan and its goals live [decided]

- **Decision:** As recommended: this file is a new plan with the `G-DS-*` goals in `scripts/goals.sh`, one line in the plan list of the agent rules, and one pointer line in `plans/ozen-dev-environment.md`. (2026-11-03, confirmed by the user)
- **Recommendation**: This file is a new plan. `scripts/goals.sh` gets `PLAN_DS`, `DS` and the `goal_G_DS_*` functions. The plan list in the agent rules gets one line for this plan, naming the plan file, the goal prefix and the tag in one line, like the other plans. The rules live first in `CLAUDE.md`, then (from P1-2) in the source `.rulesync/rules/overview.md`. `plans/ozen-dev-environment.md` gets one line pointing to this plan.
- Alternative: write the tasks into `plans/ozen-dev-environment.md`. There would be one file fewer, but that file is a Japanese list of candidates without goals, and EQ13 says new plans are English.

### D118. Order relative to the feedback plan [decided]

- **Decision:** As recommended: start this plan only after the feedback plan's final gate (`fb-end`), and close `FB` at `fb-end` in P0-3. (2026-11-03, confirmed by the user)
- **Recommendation**: Start this plan only after the feedback plan's final gate. P0-2 checks for a tag `fb-end` on the feedback plan's last commit; if that plan has not set it, P0-2 sets it and records it in §9. P0-3 then closes the feedback plan's range (`FB=fb-start..fb-end`), as the quantity plan was closed at `qc-end`.
  - Reason: the feedback plan is being carried out now (commits up to `f4f77c4`). Run at the same time, the two plans collide in five ways (§3.9):
    - Its open range makes `G-FB-SCOPE` fail on every commit of this plan.
    - Its scope includes files this plan edits: `scripts/goals.sh`, `CLAUDE.md`, `site/index.html`, `tests/golden/*`, `tests/cli/*`, `scripts/selftests.txt`, `scripts/check-rewrite-equivalence.py`, and two of the plans to translate.
    - Its goals read Japanese text in `docs/concept.md`.
    - It keeps editing the files this plan translates.
    - Once `CLAUDE.md` is generated, a direct edit of `CLAUDE.md` by the other session would break `generate --check`.
- Alternative: run now, in parallel. Each commit of this plan would be recorded in the feedback plan's §9 (so `G-FB-SCOPE` excludes it), and the edits to the shared files would be ordered by hand between two sessions. The translation would still have to wait for the feedback plan's end.

### D119. The Node.js version for rulesync [decided]

- **Decision:** As recommended: raise `node` to the newest Node.js 24 LTS release (chosen and recorded at P0-2), install it locally under the home directory after the user's yes, and run the renderer and WebAssembly goals on it first. (2026-11-03, confirmed by the user)
- **Recommendation**: `rulesync` 43.4.2 needs Node.js 22.22.0 or newer, and `toolchain.txt` pins Node.js 20.20.2 (§3.1). Raise the `node` line to the newest Node.js 24 LTS release at P0-2 and record it in §9. CI reads the version from `toolchain.txt`, so the CI jobs follow. The local machine also needs that Node.js for `G-REPO-TOOLS`. P1-1 installs it under the user's home directory from nodejs.org, after the user says yes in chat. Run `G-REND-TESTS`, `G-PLAT-WASM` and the renderer goals on the new version before anything else.
- Alternative: Node.js 22 (22.22.0 or newer). The same work, and its support ends earlier (April 2027).
- Alternative: keep Node.js 20 and use rulesync's single-binary installer. No Node.js change, but the installer is `curl ... | bash` from the latest release, which pins nothing and runs a downloaded script.

### D120. rulesync source layout and outputs [decided]

- **Decision:** As recommended: one root rule `.rulesync/rules/overview.md`, the features `rules` and `permissions` for the five targets, a wrapper that refuses `--delete`, the outputs listed in `scripts/agent-outputs.txt`, and `npx` in the third column of the `rulesync` line. (2026-11-03, confirmed by the user)
- **Recommendation**:
  - `rulesync.jsonc` names the targets `claudecode`, `agentsmd`, `codexcli`, `cursor` and `copilot`, and the features `rules` and `permissions`.
  - One root rule, `.rulesync/rules/overview.md`, holds the current `CLAUDE.md` content, rewritten (§4.1).
  - `scripts/rulesync.sh` runs `npx --yes rulesync@<version>` with the version from `toolchain.txt`. It refuses `--delete`, because that option empties whole output folders, and the Copilot target writes into `.github/`, next to the workflows.
  - The generated files are committed (EQ6) and listed in `scripts/agent-outputs.txt`.
  - The `rulesync` line of `toolchain.txt` has the third column `npx`, so `scripts/check-tools.sh` and `G-REPO-TOOLS` do not probe it as an installed tool.
- Alternative: several rule files (language, plans, publishing). They are easier to read one at a time, but the root file of each target then holds only part of the rules, and each target joins non-root rules differently.

### D121. The permission allow list [decided]

- **Decision:** As recommended: allow only the commands that read or build into `build/`, as listed in the recommendation, kept in `scripts/agent-allow.txt`. (2026-11-03, confirmed by the user)
- **Recommendation**: Allow, without a prompt, the commands that only read or that build into `build/`:
  - `scripts/goals.sh`, `scripts/run-golden.sh`, `scripts/run-cli-tests.sh`, `scripts/check-*.sh`, `scripts/build-core.sh`
  - `scripts/setup.sh`, `scripts/rulesync.sh generate --check`
  - `build/*/ozen check`, `build/*/ozen debug`, `build/*/ozen render`
  - `git status`, `git diff`, `git log`, `git show`

  The list is kept in `scripts/agent-allow.txt`. Nothing on it writes outside `build/`, commits, tags, resets, removes or pushes.
- Alternative: no allow list. EQ7 chose to set one.

### D122. `G-REPO-NOREPLY` in CI [decided]

- **Decision:** As recommended: check 3 prints `skip` when the global address is empty or the noreply address, check 2 prints `skip` only under `GITHUB_ACTIONS`, and `agents.yml` checks out with `fetch-depth: 0`. (2026-11-03, confirmed by the user)
- **Recommendation**: The goal has three checks (§3.3), and two of them cannot pass in CI as written. CI has no global address, so `git grep -F ""` matches every line. CI also has no local `user.email`. A third problem: the default checkout of GitHub Actions has depth 1, so `git log --all` sees one commit. The changes:
  - Check 3 (tracked files) prints `skip` when the global address is empty or equal to the noreply address. Other contributors' own addresses are still found, because the value is theirs.
  - Check 2 (local address) prints `skip` only when `GITHUB_ACTIONS` is `true`. Everywhere else it still requires the noreply address.
  - `agents.yml` checks out with `fetch-depth: 0`.
- Alternative: set placeholder addresses in CI. The CI job would then check made-up values, which proves nothing.

### D123. Plan labels in two languages [decided]

- **Decision:** As recommended: the checkers read both label sets from `scripts/plan-labels.tsv`. (2026-11-03, confirmed by the user)
- **Recommendation**: `scripts/plan-labels.tsv` lists each Japanese label that a checker reads, with its English counterpart (§4.5). `scripts/check-plan-tests.sh`, `scripts/check-rewrite-equivalence.py`, `scripts/check-lists-copy.py` and the label greps in `scripts/goals.sh` take the labels from this file instead of writing them inline, and accept both. The feedback plan stays Japanese and keeps passing. The translated plans and this plan use English.
- Alternative: English labels only, with the feedback plan's goals reading Japanese through a separate copy of the checkers. Two copies of every checker would drift.

### D124. How to verify a translation [decided]

- **Decision:** As recommended: `scripts/check-translation.py` compares the structure with `ds-start`, and each translation task compares the goal results before and after its commit. (2026-11-03, confirmed by the user)
- **Recommendation**: Use two checks.
  - A new checker, `scripts/check-translation.py --base REV FILE...`, compares each file with its version at `REV` (`ds-start`). It requires:
    - the same IDs
    - the same heading numbers
    - the same number of table rows in each section
    - the same inline code and code blocks after the label map (only comments in shell blocks may change)
    - no Japanese characters in prose or shell comments, except the strings listed in `scripts/translation-allow.txt`
  - Each translation task compares the results of the goals before and after its commit. The compared goals are all goals except those in `scripts/ds-goals-skip.txt`: the `G-DS-*` goals, goals that need the network (`G-PLAT-CI`, `G-REPO-TOOLS-CI`, `G-RD-GH-*`), and goals that rebuild every commit (`G-REPO-RED-FIRST`, `G-CORE-BUILD`). The results are recorded in §9.
- Alternative: review by people only. Nothing would then catch a dropped table row or a changed command in about 9,000 lines.

### D125. Historical references in the translated plans [decided]

- **Decision:** As recommended: keep every reference of the finished plans as it was, including the three references to concept §16. (2026-11-03, confirmed by the user)
- **Recommendation**: Translate the finished plans as records of what was done. Keep every reference as it was, including the three references of the roadmap to concept §16, which P1-9 removes. Section numbers, IDs and commit hashes do not change.
- Alternative: rewrite references that point to removed or renamed parts. That needs judgement in a history document, and the translation checker could no longer compare the files mechanically.

### D126. How the reference is made [decided]

- **Decision:** As recommended, in English only: `reference/language.md` by hand with checked examples; `std.md`, `diagnostics.md` and `ai-guide.md` generated and committed. (2026-11-03, confirmed by the user)
- **Recommendation**: `reference/language.md` is written by hand in English: the syntax, with examples. The examples use the `<!-- file: -->` and `<!-- run: -->` markers that `scripts/check-readme-example.sh` already runs, so they cannot go stale. `reference/std.md`, `reference/diagnostics.md` and `reference/ai-guide.md` are generated by `tools/refgen/refgen.py` from `ozen debug definitions`, `core/src/diag_codes.def` and `phrases/en.ftl`, and committed (EQ5).
- Alternative: generate `language.md` from `core/grammar.ebnf`. Fully generated, but a grammar is not a guide; people and agents learn the syntax from examples.

### D127. Content and size of the AI guide [decided]

- **Decision:** As recommended: one generated English file with the quick syntax, the compact action and diagnostics tables and the work loop, at most 64 KiB. (2026-11-03, confirmed by the user)
- **Recommendation**: `reference/ai-guide.md` is generated as one file from the reference. It holds:
  - the marked quick-syntax part of `language.md`
  - a compact table of the `std` actions (name, parameters, required state)
  - a compact table of the diagnostics (code, name, one line of text)
  - the work loop: write, run `ozen check --format json`, fix every error, run `ozen render --kind recipe`

  It stays at or under 64 KiB, so that an agent can read it whole.
- Alternative: no size limit. The guide could grow until agents read only part of it.

### D128. Behaviour of `ozen init` [decided]

- **Decision:** `ozen init [DIR] [--name NAME]` like `git init`, `uv init` and `deno init`: the current folder by default, the given folder (created if missing) otherwise; the package name is the folder's base name unless `--name` is given; never overwrites. The user asked how other tools behave, and chose this after the comparison. (2026-11-03, confirmed by the user)
- Comparison (common behaviour of other tools, from memory, not re-checked on 2026-11-03): `git init [dir]`, `uv init [path]` and `deno init [dir]` work in the current folder, or in the given folder (created if missing). `cargo` splits `cargo init [path]` (current or given folder) from `cargo new <name>` (a new folder). `go mod init <module>` works in the current folder and needs the module name. `cargo`, `go`, `uv` and `deno` refuse when the project file already exists. So `init` usually means "here, or the given folder", and a separate `new` makes a new folder.
- **Recommendation**: `ozen init [DIR] [--name NAME]`, like `git init`, `uv init` and `deno init`.
  - Without `DIR`, it works in the current folder. With `DIR`, it works in that folder and creates it if missing.
  - The package name is the base name of the folder, or `NAME` when `--name` is given. It must be an ASCII identifier other than `std`; otherwise it writes nothing and exits 2.
  - It writes `package.ozen` and `AGENTS.md`. It never overwrites: it checks both files before writing anything, and if either exists it writes nothing and exits 1.
  - The text of `AGENTS.md` comes from `reference/agents-template.md` (English), built into the binary, so the file in the repository is the only source.
- Alternative: two commands, `ozen new NAME` (a new folder) and `ozen init` (the current folder), like `cargo` and `poetry`. Clear, but two commands for one small job.
- Alternative: `ozen init NAME` always makes a new folder (the first draft). It behaves like `cargo new` under the name `init`, which differs from what `init` does in most tools.

### D129. Where `AGENTS.md` points [decided]

- **Decision:** As recommended: link the AI guide at the tag of the installed version, `https://github.com/atsuki-seo/Ozen/blob/v<VERSION>/reference/ai-guide.md`. (2026-11-03, confirmed by the user)
- **Recommendation**: To the AI guide of the installed version on GitHub: `https://github.com/atsuki-seo/Ozen/blob/v<VERSION>/reference/ai-guide.md`. The guide then always matches the installed `ozen`. A build of a version that has no tag yet points to a page that does not exist until the release (§10 R6).
- Alternative: point to `main`. That page always exists, but it can describe a newer language than the installed one.

### D130. Release contents [decided]

- **Decision:** As recommended: Linux, macOS (universal) and WebAssembly archives with the renderer, phrases, reference, licenses and README, plus `SHA256SUMS`, each run after packing in the release workflow. (2026-11-03, confirmed by the user)
- **Recommendation**: Three archives and a checksum file:
  - `ozen-<v>-linux-x86_64.tar.gz` and `ozen-<v>-macos-universal.tar.gz` hold `ozen-<v>/bin/ozen`, `renderers/` (without tests), `phrases/`, `reference/`, the license files, `NOTICE` and `README.md`. `bin/ozen render` finds `../renderers/cli.mjs` by itself (§3.5).
  - `ozen-<v>-wasm.zip` holds `bindings/wasm/ozen.mjs`, `worker.js`, `ozen_bridge.js` and the files they load.
  - `SHA256SUMS`.

  `std` is built into the binary. The release workflow runs each archive after packing it: `--version`, a render, and a render with `bin/` on `PATH`. On macOS it also checks both architectures with `lipo -archs`.
- Alternative: the binary alone. Smaller, but `ozen render` would not work without a checkout.

### D131. Release tag name [decided]

- **Decision:** Tags have the form `v<VERSION>`; the user answered with the example `v0.1.0`, which D134 takes up for the first release. (2026-11-03, confirmed by the user)
- **Recommendation**: `v<VERSION>` (for example `v0.2.0`). It is the common form on GitHub, and it does not collide with the plan tags (`plan-start`, `qc-end`, and so on).
- Alternative: `<VERSION>` without `v`. Also common, but the release workflow then has to tell version tags from plan tags by a pattern on digits.

### D132. Version strings in golden expectations [decided]

- **Decision:** As recommended: write `{{STD_VERSION}}` for `ozenVersion` and the SARIF tool version, keep package versions literal, and make `std-version-check` require `99.0.0`. (2026-11-03, confirmed by the user)
- **Recommendation**: `scripts/run-golden.sh` already replaces `{{STD_VERSION}}` in expectations with the content of `VERSION` (16 expectation files use it, §3.6). Write `{{STD_VERSION}}` for the remaining literal versions that a bump changes: `"ozenVersion":"0.1.0"` in 19 JSON expectations and the SARIF tool `"version":"0.1.0"` in one. Package versions in inputs and outputs (`version: "0.1.0"` of sample packages, `std: "0.1.0"` requirements) stay literal. `tests/golden/bundle/std-version-check` requires `std: "0.2.0"` to test "requires newer"; change its requirement to `99.0.0` so that the test keeps its meaning after any bump.
- Alternative: `scripts/bump-version.sh` rewrites the expectations at each bump. No change to the expectations now, but every bump changes about 20 expectations in the same commit as `VERSION`. `std-version-check` would also need a new number at each bump.

### D133. Version goals of the finished plans [decided]

- **Decision:** As recommended: `G-RD-VERSION`, `G-QC-VERSION` and `G-FB-VERSION` read `VERSION` at their end tags, and `G-REPO-VERSION` stops reading the roadmap. (2026-11-03, confirmed by the user)
- **Recommendation**:
  - `G-REPO-VERSION` checks `VERSION` against `ozen --version` and `stdlib/package.ozen` only, and stops reading `版: <v>` in the roadmap.
  - `G-RD-VERSION`, `G-QC-VERSION` and `G-FB-VERSION` check `VERSION` as it was at their end tags (`git show readme-end:VERSION`, `qc-end`, `fb-end`). That is what they meant: those plans did not change the version.
- Alternative: keep them and never bump. Then there are no releases (EQ10).
- Alternative: leave them until the next bump. They pass while the version stays `0.1.0` (D134), but the next bump would fail three finished plans' goals.

### D134. The first release [decided]

- **Decision:** Release the current version as `v0.1.0` without a bump (the user's choice; `0.1.0` has never been published). The tag is pushed only after the user's yes in chat (P4-6). (2026-11-03, confirmed by the user)
- **Recommendation**: At the end of this plan, bump to `0.2.0` (a new command, `ozen init`, and a new reference: a minor step). Then tag `v0.2.0` and push the tag, after the user says yes in chat (P4-6). `G-DS-RELEASE-PUBLISHED` then checks the published release.
- Alternative: release the current version as `v0.1.0` without a bump. `0.1.0` has never been published, so the first public release can carry it, including what this plan adds.
- Alternative: prepare everything but release later. Nothing public changes now, but the release workflow is then first run on a later, unrelated change.

### D135. Links from the introduction page [decided]

- **Decision:** As recommended: add the two links `Reference` and `Releases` to `site/index.html`, with no new Japanese sentence. (2026-11-03, confirmed by the user)
- **Recommendation**: `site/index.html` (Japanese, the one exception to English) gets two links: the reference (`https://github.com/atsuki-seo/Ozen/tree/main/reference`) and the releases page. The link texts are the two words `Reference` and `Releases`, so no new Japanese sentence is added.
- Alternative: no change to the page. Its readers would not find the reference or the downloads.

### D136. The push rule in the agent rules [decided]

- **Decision:** As recommended: keep "run `G-REPO-NOREPLY` before every push" and drop "ask the user before pushing". (2026-11-03, confirmed by the user)
- **Recommendation**: The rewritten rules keep "run `G-REPO-NOREPLY` before every push" and drop "ask the user before pushing". Whether to ask before a push is each person's own preference (the owner has said not to ask for Ozen). The pre-push hook enforces the part that protects the repository.
- Alternative: keep "ask the user before pushing" for every agent. Safer for new contributors, but it contradicts the owner's standing instruction in every session.

---

## 3. Facts checked (2026-11-02 and 2026-11-03)

### 3.1 Toolchain and rulesync

- `toolchain.txt` pins `node 20.20.2`, `cmake 3.28.3`, `emcc 6.0.13` and others. A third column names a host (`xcode 26.6 macos`, `ndk ... ci`). `ci.yml` reads the `cmake`, `emcc`, `node`, `xcode` and `ndk` versions from it with `awk`. `scripts/check-tools.sh` compares the local tools with it and has no probe for unknown tools. `G-REPO-TOOLS` counts the rows whose third column is empty or `linux`, and requires 0 mismatches. The local `node --version` is v20.20.2.
- `npm view rulesync` (2026-11-03): version 43.4.2, license MIT, `engines.node` `>=22.22.0`.
- In the 43.4.2 package:
  - Commands: `rulesync generate --targets ... --features ...`, `generate --check` ("exits with code 1 if changes needed"), `generate --dry-run`, and `generate --delete` ("Delete all existing files in output directories before generating").
  - Source tree: `.rulesync/` (`rules`, `permissions`, `hooks`, `mcp`, `skills`, `subagents`, `commands`), with the config file `rulesync.jsonc`.
  - Target names include `claudecode`, `agentsmd`, `codexcli`, `cursor` and `copilot`.
  - The support table marks `rules` and `permissions` for Claude Code, Codex CLI, GitHub Copilot and Cursor, and `rules` for `AGENTS.md`.
  - The package names output paths such as `CLAUDE.md`, `AGENTS.md`, `.claude/settings.json`, `.claude/rules`, `.codex/config.toml`, `.cursor/rules` and `copilot-instructions.md`. The exact set written for this config is recorded at P1-2 (§9).
- Claude Code reads `AGENTS.md` when no `CLAUDE.md` is present, and reads only `CLAUDE.md` when both are present (official documentation, <https://code.claude.com/docs/en/memory>, read 2026-11-02).

### 3.2 Agent files today

- `CLAUDE.md` has three sections: language of committed files, plans, publishing. The publishing section says "ask the user before pushing". `.claude/` is empty and untracked. There is no `AGENTS.md`.
- Goals that read `CLAUDE.md`: `G-REPO-CLAUDEMD` (the word Japanese), `G-RD-CLAUDEMD`, the syntax plan's check for its plan line, `G-QC-CLAUDEMD`, and the feedback plan's check for its plan line. Each requires a plan line to match `<plan>.md.*G-<XX>-.*<tag>` on one line.

### 3.3 The publishing check

- `goal_G_REPO_NOREPLY` (`scripts/goals.sh`) has three checks:
  1. Every author and committer address in `git log --all` is the noreply address.
  2. The local `user.email` is the noreply address.
  3. `git grep -nF "$(git config --global user.email)"` finds 0 lines.

  It takes about 0.01 s.
- `ci.yml` ignores pushes that change only `site/**`, `plans/**`, `docs/**` or `pages.yml`, and checks out with the default depth.
- `scripts/goals.sh` exits 0 when every named goal passes and non-zero otherwise. Its last line names only the last goal.

### 3.4 Text that goals read in Japanese

- Plan labels:
  - In `scripts/goals.sh`: `【決定済み】`, `【未決】`, `- **決定:`, `機械で決められない理由:`, `- 網羅の監査: 指摘 N 件、取り込み M 件`, `- 範囲に入った別の作業のコミット:`, `- 期待を直した golden:`, ``- サンプルの `expect` の葉の数 k（P2-3）:`` and `版: <v>`.
  - In `scripts/check-plan-tests.sh`: the columns `わざと壊す方法` and `テストまたはゴール`, the record `- わざと壊す確認:`, and the count `（N 個）`.
  - In `scripts/check-rewrite-equivalence.py`: `- 書き直しのわざと壊す確認:`.
  - In `scripts/check-lists-copy.py`: the row `合計`.
- Concept text, read by goals selected by content (each function that reads `docs/concept.md`):
  - `G-RF-CONCEPT` and `G-RF-CONCEPT-PARENS`: `()` を省く, and the rows `| 警告` and `| エラー` of §6.1.
  - `G-QC-SYNTAX`: 上端は含む, 範囲外は必ずエラー with 動作の引数, and 0 以下の量.
  - `G-FB-CONCEPT`: 9 Japanese greps.
  - `G-FB-DIAG`.
- Feedback plan labels: `見直しの対象`, `書き直したテスト`.

### 3.5 Building and running `ozen`

- `cli/CMakeLists.txt` and `core/CMakeLists.txt` read `VERSION` into `OZEN_VERSION`. `stdlib/package.ozen` has `version: "0.1.0"`. `std` is built into the binary (`ozen debug embedded-std`).
- `ozen render` finds the renderer through `OZEN_RENDERER`, or through `../../renderers/cli.mjs` or `../renderers/cli.mjs` relative to the binary (`cli/src/main.c`). It runs the renderer with `node`; `OZEN_NODE` overrides that.
- The `ios` job of `ci.yml` already builds a universal macOS binary, `build/macos/ozen` (`arm64;x86_64`). The `wasm` job runs `scripts/build-wasm.sh`. `bindings/wasm/worker.js` loads `ozen_bridge.js` and `./ozen.mjs`.
- `ozen debug definitions FILE` prints every definition, including `std`, as JSON (365 lines for a one-recipe file), with `kind`, `name`, `module`, `local`, `params` and other fields. The 32 `std` actions are all `local: false`.
- `core/src/diag_codes.def` lists each diagnostic as `OZNI_DIAG(CODE, CATEGORY, name, SEVERITY)`. `phrases/{en,ja}.ftl` hold `diag-*` texts and action texts such as `action-peel`.
- The commands are `check`, `build`, `render`, `bundle`, `unbundle` and `debug`; there is no `init`. CLI tests are `tests/cli/*.sh`, run by `scripts/run-cli-tests.sh`, whose last line is `cli: N cases, N failed`.
- `scripts/check-readme-example.sh --readme FILE --ozen OZEN` runs the `<!-- file: -->` and `<!-- run: -->` examples of a Markdown file. Its last line is `readme: N examples, 0 mismatches`. The self-tests of the checkers end with `<name>: N cases, 0 failed`.
- `scripts/selftests.txt` pairs each checker with one self-test (`scripts/goals.sh scripts/test-goals.sh`).

### 3.6 Version strings

- `scripts/run-golden.sh` reads `VERSION` (or `--std-version V`) and replaces `{{STD_VERSION}}` in expectations; 16 golden files use it.
- At `8f419ad`, 40 files under `tests/` and `samples/` contain `0.1.0`:
  - 19 JSON expectations with `"ozenVersion":"0.1.0"` (19 at `f4f77c4`)
  - one SARIF expectation with `"version":"0.1.0"` (`tests/golden/api/sarif-mapping.out`)
  - package versions in inputs and samples
- `tests/golden/bundle/std-version-check` requires `std: "0.2.0"` and expects `PKG006 requiresNewer`.
- `G-REPO-VERSION` checks `ozen --version`, `stdlib/package.ozen` and `版: <v>` in the roadmap. `G-RD-VERSION`, `G-QC-VERSION` and `G-FB-VERSION` require `VERSION` to be `0.1.0`.

### 3.7 Sizes and references

- At `f4f77c4`: `docs/concept.md` 1663 lines, `docs/prototype-report.md` 117, `plans/ozen-roadmap.md` 3738, `plans/ozen-syntax-refresh.md` 1476, `plans/ozen-readme-compact.md` 880, `plans/ozen-quantity-checks.md` 1019.
- The roadmap refers to concept §16 three times (lines 100, 1216 and 1409 at `f4f77c4`).
- `scripts/goals.sh` defines 177 goals at `8f419ad`.

### 3.8 Open ranges of earlier plans

- `R=plan-start..HEAD`: `G-REPO-RED-FIRST` requires each new golden or CLI expectation to fail on the commit before its implementation. For CLI tests the check is nominal: `scripts/check-red-first.sh` passes `--only` to `scripts/run-cli-tests.sh`, which exits 2 unless it gets exactly two arguments.
- `RF=refresh-start..HEAD`: `G-RF-SEPARATION` forbids a commit that changes golden expectations together with the implementation (`core/`, `stdlib/`, `cli/src/`).
- `FB=fb-start..HEAD`: `G-FB-SCOPE` fails on any commit that changes a file outside the feedback plan's scope, unless that plan's §9 names it.

### 3.9 Files shared with the feedback plan

- The scope of the feedback plan (`fb_in_scope` in `scripts/goals.sh`) includes `scripts/goals.sh`, `CLAUDE.md`, `site/index.html`, `tests/golden/*`, `tests/cli/*`, `scripts/selftests.txt`, `scripts/check-rewrite-equivalence.py`, `docs/concept.md`, `plans/ozen-roadmap.md` and `plans/ozen-quantity-checks.md`. This plan edits all of them.

---

## 4. Behaviour spec

Each checkable claim is one item with an ID `**S<n>**`.

### 4.1 Agent rules (D119, D120, D121, D136)

- **S945** `toolchain.txt` has a line `rulesync <version> npx`, with the version chosen at P0-2 (43.4.2 unless §9 records a newer one).
- **S946** The `node` line of `toolchain.txt` is the version chosen by D119, and it is 22.22.0 or newer.
- **S947** `G-REPO-TOOLS` passes with the new `toolchain.txt` on a machine that has the pinned tools.
- **S948** `scripts/rulesync.sh ARGS...` runs `npx --yes rulesync@<version from toolchain.txt> ARGS...` from the repository root.
- **S949** `scripts/rulesync.sh` exits 2 with a message naming `toolchain.txt` when the file has no `rulesync` line.
- **S950** `scripts/rulesync.sh` exits 2 and runs nothing when an argument is `--delete`.
- **S951** The targets of `rulesync.jsonc` are exactly `claudecode`, `agentsmd`, `codexcli`, `cursor`, `copilot`, and its features are exactly `rules`, `permissions`.
- **S952** `.rulesync/rules/` holds one file, `overview.md`, and its front matter marks it as the root rule.
- **S953** After `scripts/rulesync.sh generate`, `git status --porcelain --untracked-files=no` is empty.
- **S954** `scripts/rulesync.sh generate --check` exits 0 on the committed tree.
- **S955** After an edit of `.rulesync/rules/overview.md` without generating, `scripts/rulesync.sh generate --check` exits 1.
- **S956** Each path in `scripts/agent-outputs.txt` is tracked by git, and the files written by `scripts/rulesync.sh generate` are exactly those paths.
- **S957** `CLAUDE.md` and `AGENTS.md` exist at the root and contain the noreply address.
- **S958** For each plan (the five earlier plans and this one), `CLAUDE.md` and `AGENTS.md` each have one line that names the plan file, its goal prefix and its tag.
- **S959** The Cursor rule file has `alwaysApply: true` in its front matter, and the Copilot rule file is `.github/copilot-instructions.md`.
- **S960** The Claude Code allow list in the generated `.claude/settings.json` equals `scripts/agent-allow.txt`.
- **S961** No entry of `scripts/agent-allow.txt` pushes, commits, tags, resets, removes files or redirects output.
- **S962** The language rule says that committed files are written in English.
- **S963** The language rule names the exceptions: `site/`, the MEXT attribution wording, and the values of `phrases/ja.ftl`.
- **S964** The language rule names the three Japanese plans that stay Japanese.
- **S965** The language rule says that the language of chat and of uncommitted work follows each person's own settings.
- **S966** No sentence of the generated agent files says that `docs/` or `plans/` as a whole are written in Japanese.
- **S967** The generated agent files keep "run `G-REPO-NOREPLY` before every push" and no longer say "ask the user before pushing" (D136).
- **S968** `G-REPO-CLAUDEMD` and the plan-line goals of the earlier plans pass on the generated `CLAUDE.md`.

### 4.2 Publishing check (D122)

- **S969** `.githooks/pre-push` is executable and runs `scripts/goals.sh G-REPO-NOREPLY`.
- **S970** In a throwaway clone with `core.hooksPath` set to `.githooks`, a push of a commit authored with an address other than the noreply address is refused, and the remote branch does not move.
- **S971** In the same clone, a push of a commit with the noreply address succeeds.
- **S972** When `git config --global user.email` is empty, check 3 of `G-REPO-NOREPLY` prints `skip`, and the goal passes on a clean tree.
- **S973** When the global address equals the noreply address, check 3 prints `skip`.
- **S974** When the global address is another address and that address appears in a tracked file, `G-REPO-NOREPLY` fails.
- **S975** When `GITHUB_ACTIONS` is `true` and no local `user.email` is set, check 2 prints `skip`. Without `GITHUB_ACTIONS`, a missing local address still fails.
- **S976** `.github/workflows/agents.yml` runs on every push to `main` without a path filter, checks out with `fetch-depth: 0`, sets up Node.js from `toolchain.txt`, and runs `G-REPO-NOREPLY` and `scripts/rulesync.sh generate --check`.
- **S977** On GitHub, a push whose commit changes only `plans/` runs `agents.yml`, and the run succeeds.

### 4.3 Preparation script (EQ8)

- **S978** `scripts/setup.sh` prints one line per row of `toolchain.txt`, taken from the probes of `scripts/check-tools.sh`: `ok <tool> <version>`, `missing <tool> want <version>`, `differs <tool> want <version> got <version>` or `skip <tool>`.
- **S979** A row whose third column names another host, `ci` or `npx` prints `skip`.
- **S980** `scripts/setup.sh` sets `git config --local core.hooksPath .githooks` when it is unset or already `.githooks`.
- **S981** When `core.hooksPath` has another value, `scripts/setup.sh` keeps it and prints `hooksPath: kept <value>`.
- **S982** `scripts/setup.sh` runs `scripts/rulesync.sh generate` and prints `agent files: unchanged` or `agent files: updated`.
- **S983** When `scripts/rulesync.sh` fails (for example offline), `scripts/setup.sh` prints `agent files: skipped (rulesync failed)`, and the exit status does not change because of it.
- **S984** A second run of `scripts/setup.sh` changes no file and prints the same lines.
- **S985** `scripts/setup.sh` exits 0 when the required tools are `ok`, and 1 otherwise. The required tools are `gcc`, `cmake`, `python3` and `node` on Linux, and `cmake`, `python3` and `node` on other hosts.
- **S986** The other tools do not change the exit status.
- **S987** `scripts/setup.sh` installs nothing into the system or the global environment: it contains no `sudo`, `apt`, `brew install`, `pip install` or `npm install` (the npm cache that `npx` fills is the only download).

### 4.4 Entry documents and concept §16

- **S988** `docs/concept.md` has no section 16 (no heading `## 16.`), and its last numbered section is 15.
- **S989** `CONTRIBUTING.md` has a section `## Where things are` with a table that has rows for `docs/`, `plans/`, `reference/`, `site/`, the folder READMEs, `.rulesync/`, and `CLAUDE.md` with `AGENTS.md`.
- **S990** `CONTRIBUTING.md` has a section `## Getting started` that names `scripts/setup.sh` and `scripts/goals.sh` and says that `CLAUDE.md` and `AGENTS.md` are generated from `.rulesync/`.
- **S991** The documentation list of `README.md` links `reference/`.
- **S992** `plans/ozen-dev-environment.md` has one line that points to `plans/ozen-dev-setup.md`.

### 4.5 Plan labels and the translation (D118, D123, D124, D125)

The label map (`scripts/plan-labels.tsv`, with the Japanese and English labels separated by a tab):

| Japanese | English |
| --- | --- |
| `【決定済み】` | `[decided]` |
| `【未決】` | `[open]` |
| `- **決定:**` | `- **Decision:**` |
| `機械で決められない理由:` | `Reason a command cannot decide:` |
| `わざと壊す方法` | `How to break it` |
| `テストまたはゴール` | `Test or goal` |
| `- わざと壊す確認: ` | `- Mutation check: ` |
| `- 書き直しのわざと壊す確認: ` | `- Rewrite mutation check: ` |
| `（N 個）` | `(N cases)` |
| `合計` | `Total` |
| `- 網羅の監査: 指摘 N 件、取り込み M 件` | `- Coverage audit: N findings, M taken` |
| `- 範囲に入った別の作業のコミット: ` | `- Other work's commit in range: ` |
| `- 期待を直した golden: ` | `- Golden with a corrected expectation: ` |
| ``- サンプルの `expect` の葉の数 k（P2-3）: `` | ``- Leaves of the sample `expect`, k (P2-3): `` |
| `版: ` | `Version: ` |

- **S993** `scripts/plan-labels.tsv` has exactly the rows of the table above.
- **S994** `scripts/check-plan-tests.sh`, `scripts/check-rewrite-equivalence.py` and `scripts/check-lists-copy.py` take their labels from `scripts/plan-labels.tsv` and contain none of the Japanese labels themselves.
- **S995** `scripts/check-plan-tests.sh` finds the test column and the mutation column under either label, and counts mutation records under either label.
- **S996** `scripts/check-plan-tests.sh` reads a case count written as `（N 個）` or `(N cases)`.
- **S997** `scripts/check-rewrite-equivalence.py` counts rewrite mutation records under either label.
- **S998** `scripts/check-lists-copy.py` skips a row whose first cell is `合計` or `Total`.
- **S999** Every label grep of `scripts/goals.sh` that reads a translated plan reads its English label, and every one that reads the feedback plan reads its Japanese label.
- **S1000** The six translated files (`docs/concept.md`, `docs/prototype-report.md` and the four finished plans) have no Japanese character in prose, outside inline code and code blocks, except the strings in `scripts/translation-allow.txt`.
- **S1001** In each translated file, comments in shell code blocks have no Japanese character.
- **S1002** In each translated file, the set of IDs (`D<n>`, `S<n>`, `P<n>-<m>`, `R<n>`, `G-<NAME>`) equals the set at `ds-start`.
- **S1003** In each translated file, the sequence of heading numbers equals the one at `ds-start`, except section 16 of `docs/concept.md` (S988).
- **S1004** In each translated file, inline code and code blocks equal those at `ds-start` after the label map, except comments in shell blocks.
- **S1005** In each translated file, each section has the same number of table rows as at `ds-start`.
- **S1006** Each entry of `scripts/translation-allow.txt` has a reason after a tab.
- **S1007** Each entry of `scripts/translation-allow.txt` occurs in at least one translated file.
- **S1008** For each translation task, every goal that is not in `scripts/ds-goals-skip.txt` has the same result before and after the task's commit, and §9 records both summaries.
- **S1009** No goal function that reads `docs/concept.md` contains a Japanese character after the translation, and each of them passes.
- **S1010** The commits of this plan do not change `plans/ozen-cooking-feedback.md` or `plans/ozen-authoring-support.md`.
- **S1011** The three references of the translated roadmap to concept §16 stay as they were (D125).
- **S1012** `scripts/goals.sh --summary ID...` prints one line `<ID> pass` or `<ID> fail` per goal and runs every goal even when one fails.
- **S1013** `scripts/goals.sh` defines `FB=fb-start..fb-end`, and `G-FB-SCOPE` passes.

### 4.6 This plan's own labels

- **S1014** Outside §3.4, the label table of §4.5, the code block of §5.1 and the coverage table of §5.3, this plan has none of the Japanese labels.

### 4.7 Reference (D126, D127)

- **S1015** `reference/` holds exactly `README.md`, `language.md`, `std.md`, `diagnostics.md`, `ai-guide.md` and `agents-template.md`, all in English.
- **S1017** The examples of `reference/language.md` pass `scripts/check-readme-example.sh`.
- **S1018** `language.md` marks its quick-syntax part with `<!-- ai-guide:begin -->` and `<!-- ai-guide:end -->`.
- **S1019** `std.md` has one entry for each definition of kind `action` from `ozen debug definitions` whose module starts with `std.` and that is not `local`.
- **S1020** Each action entry of `std.md` lists its parameters, with their types or dimensions, and the states it requires, as given in the definitions JSON.
- **S1021** `std.md` uses the action text of `phrases/en.ftl` for each action that has one.
- **S1022** `diagnostics.md` has one entry for each line of `core/src/diag_codes.def`, with its code, name, category and default severity.
- **S1023** Each entry of `diagnostics.md` has the `diag-<name>` text of `phrases/en.ftl` when that phrase exists, and the words of the name otherwise.
- **S1024** `ai-guide.md` holds, in this order: the quick-syntax part of `language.md`, the compact action table, the compact diagnostics table and the work loop.
- **S1025** `ai-guide.md` is at most 65536 bytes.
- **S1026** Each generated file starts with a line saying that it is generated by `tools/refgen/refgen.py` and must not be edited.
- **S1027** `python3 -B tools/refgen/refgen.py --check` exits 0 on the committed tree and prints `refgen: 3 files, 0 differ`.
- **S1028** Two runs of `tools/refgen/refgen.py` write the same bytes.
- **S1029** When a new action is added to a copy of `std`, `refgen.py --check` names the changed files and exits 1.

### 4.8 `ozen init` (D128, D129)

- **S1030** `ozen init DIR` creates `DIR` if it is missing and writes `DIR/package.ozen` with the text `package <base name of DIR> {\n  version: "0.1.0"\n}\n`.
- **S1074** `ozen init` without `DIR` writes into the current folder, and the package name is the base name of the current folder.
- **S1075** `ozen init DIR --name NAME` uses `NAME` as the package name.
- **S1031** `ozen init DIR` creates `DIR/AGENTS.md` from `reference/agents-template.md`, with `@VERSION@` replaced by the version of the binary.
- **S1033** `ozen init DIR` prints `created DIR/package.ozen`, `created DIR/AGENTS.md` and the line about `@AGENTS.md`, and exits 0.
- **S1034** When `package.ozen` or `AGENTS.md` exists in the target folder, `ozen init` writes neither file and exits 1.
- **S1035** When the package name (the base name of the folder, or `--name`) is not an ASCII identifier or is `std`, `ozen init` writes nothing, creates no folder, and exits 2.
- **S1036** `ozen check DIR/package.ozen` on a new folder reports 0 errors.
- **S1037** The `AGENTS.md` written by `ozen init` contains `https://github.com/atsuki-seo/Ozen/blob/v<VERSION>/reference/ai-guide.md`.
- **S1038** The template contains `ozen check --format json` and `ozen render --kind recipe`.
- **S1039** The usage text of `ozen` and the command list of `cli/README.md` name `ozen init`.
- **S1040** After the template is edited and the binary rebuilt, `ozen init` writes the edited text.

### 4.9 Version (D132, D133)

- **S1041** The golden expectations hold `{{STD_VERSION}}` where they print the version of Ozen: `"ozenVersion":"{{STD_VERSION}}"` and the SARIF tool version.
- **S1042** No golden expectation contains `"ozenVersion":"` followed by a literal version.
- **S1043** Package versions and `std` requirements in golden inputs and samples (`version: "0.1.0"`, `std: "0.1.0"`) stay literal.
- **S1044** `tests/golden/bundle/std-version-check` requires `std: "99.0.0"` and still expects `PKG006 requiresNewer`.
- **S1045** `scripts/bump-version.sh X.Y.Z` writes `X.Y.Z` to `VERSION` and to the `version:` line of `stdlib/package.ozen`, and changes no other file.
- **S1046** `scripts/bump-version.sh` exits 2 for an argument that is not `<digits>.<digits>.<digits>`.
- **S1047** In a throwaway worktree bumped to `9.9.9` and rebuilt, `G-CORE-GOLDEN`, `G-CLI-TESTS`, `G-REPO-VERSION`, `G-RD-VERSION`, `G-QC-VERSION` and `G-FB-VERSION` pass.
- **S1048** `G-REPO-VERSION` no longer reads the roadmap.
- **S1049** `G-RD-VERSION`, `G-QC-VERSION` and `G-FB-VERSION` read `VERSION` at `readme-end`, `qc-end` and `fb-end`.

### 4.10 Releases (D130, D131, D134)

- **S1050** `.github/workflows/release.yml` runs only on pushed tags matching `v*`. It has no `branches`, `pull_request`, `workflow_dispatch` or `schedule` trigger.
- **S1051** The first step after the checkout in each job of the release workflow runs `scripts/check-release-tag.sh "$GITHUB_REF_NAME"`.
- **S1052** `scripts/check-release-tag.sh TAG` exits 0 when `TAG` is `v` followed by the content of `VERSION`, and 1 otherwise.
- **S1053** The release workflow has `permissions: contents: write`, uses no secret other than `GITHUB_TOKEN`, and creates the release with `gh release create`.
- **S1054** `scripts/make-release.sh linux|macos|wasm` writes `dist/ozen-<v>-linux-x86_64.tar.gz`, `dist/ozen-<v>-macos-universal.tar.gz` or `dist/ozen-<v>-wasm.zip`.
- **S1055** A native archive holds `ozen-<v>/bin/ozen`, `ozen-<v>/renderers/cli.mjs`, `ozen-<v>/phrases/`, `ozen-<v>/reference/`, the three license files, `NOTICE` and `README.md`, and no `renderers/test/`.
- **S1056** The `README.md` in the archive says that `ozen render` needs `node`.
- **S1057** In an extracted Linux archive, `bin/ozen --version` prints `ozen <v>`.
- **S1058** In an extracted Linux archive, `bin/ozen render --kind recipe` on the soup example of `README.md` prints the same text that `README.md` shows.
- **S1059** With the extracted `bin/` on `PATH`, `ozen render --kind recipe` from another folder prints the same text. This is checked locally on Linux and in the release workflow on macOS.
- **S1060** In the release workflow, `lipo -archs` of the macOS binary prints both `x86_64` and `arm64`.
- **S1061** `SHA256SUMS` has one line for each archive, and `sha256sum -c SHA256SUMS` passes.
- **S1062** The WebAssembly archive holds `ozen.mjs`, `worker.js`, `ozen_bridge.js` and every file they import or fetch. A node run of the extracted `ozen.mjs` on the soup example succeeds.
- **S1063** After the first release, `gh release view v<v>` lists the three archives and `SHA256SUMS`.
- **S1064** The Linux archive downloaded from the release passes `sha256sum -c` and prints `ozen <v>` for `--version`.
- **S1065** The release workflow run for the tag's commit succeeds.
- **S1066** After the release, `reference/ai-guide.md` exists at the tag on GitHub.

### 4.11 Introduction page (D135)

- **S1067** `site/index.html` links `https://github.com/atsuki-seo/Ozen/tree/main/reference` and `https://github.com/atsuki-seo/Ozen/releases`, with the link texts `Reference` and `Releases`.
- **S1068** After the push, `reference` exists on GitHub at `main`, and the Pages run for that commit succeeds.

### 4.12 Invariants

- **S1069** In the range `DS` (without the commits named in §9), no golden expectation changes, except `{{STD_VERSION}}` in place of the version (S1041) and the requirement of `std-version-check` (S1044).
- **S1070** The files changed in the range `DS` (without the commits named in §9) match the patterns of `scripts/ds-scope.txt`.
- **S1071** `scripts/ds-scope.txt` has no empty line and lists exactly the paths of §4.13.
- **S1072** `scripts/selftests.txt` pairs each new script with its self-test: `rulesync.sh`, `setup.sh`, `check-translation.py`, `bump-version.sh`, `check-release-tag.sh` and `make-release.sh`. The new cases of `G-REPO-NOREPLY` go into `scripts/test-goals.sh`.
- **S1073** `.gitignore` lists `dist/`.

### 4.13 Files added or changed

| Kind | Paths |
| --- | --- |
| Agent rules | `.rulesync/rules/overview.md`, `.rulesync/permissions*`, `rulesync.jsonc`, `scripts/agent-allow.txt`, `scripts/agent-outputs.txt`, and the generated `CLAUDE.md`, `AGENTS.md`, `.claude/settings.json` and Copilot, Cursor and Codex files listed in `scripts/agent-outputs.txt` |
| Scripts | `scripts/rulesync.sh`, `scripts/setup.sh`, `scripts/plan-labels.tsv`, `scripts/check-translation.py`, `scripts/translation-allow.txt`, `scripts/ds-goals-skip.txt`, `scripts/ds-scope.txt`, `scripts/bump-version.sh`, `scripts/check-release-tag.sh`, `scripts/make-release.sh`, `scripts/test-rulesync.sh`, `scripts/test-setup.sh`, `scripts/test-pre-push.sh`, `scripts/test-check-translation.sh`, `scripts/test-bump-version.sh`, `scripts/test-check-release-tag.sh`, `scripts/test-make-release.sh`, `scripts/test-refgen.sh`, and changes to `scripts/goals.sh`, `scripts/test-goals.sh`, `scripts/check-plan-tests.sh`, `scripts/test-check-plan-tests.sh`, `scripts/check-rewrite-equivalence.py`, `scripts/test-check-rewrite-equivalence.sh`, `scripts/check-lists-copy.py`, `scripts/test-check-lists-copy.sh`, `scripts/check-tools.sh`, `scripts/selftests.txt` |
| Hooks and CI | `.githooks/pre-push`, `.github/workflows/agents.yml`, `.github/workflows/release.yml` |
| Repository files | `toolchain.txt`, `.gitignore` |
| Documents | `docs/concept.md`, `docs/prototype-report.md`, `plans/ozen-roadmap.md`, `plans/ozen-syntax-refresh.md`, `plans/ozen-readme-compact.md`, `plans/ozen-quantity-checks.md`, `plans/ozen-dev-environment.md` (one line), `plans/ozen-dev-setup.md`, `CONTRIBUTING.md`, `README.md`, `cli/README.md`, `site/index.html`, `site/README.md` |
| Reference | `reference/*.md`, `tools/refgen/*` |
| `ozen init` | `cli/src/*`, `cli/CMakeLists.txt`, `tests/cli/init-*.sh` |
| Version | `tests/golden/**/*.out` (only S1041), `tests/golden/bundle/std-version-check.*` (only S1044), and `VERSION` and `stdlib/package.ozen` at P4-6 |

### 4.14 Where code goes

| Place | What |
| --- | --- |
| `scripts/` | Shell and Python helpers and checkers, each with a self-test listed in `scripts/selftests.txt` |
| `tools/refgen/` | The reference generator, in Python with only the packages that `toolchain.txt` lists; its test is `scripts/test-refgen.sh` |
| `cli/src/` | `ozen init`; the template is embedded at build time, and it is a build dependency of the binary |

---

## 5. Machine-checked goals

**Prerequisite goals** (implied in every completion condition):

- All goals that run locally in §5 of the syntax plan, the README plan, the quantity plan and the feedback plan. P0-2 records which of them pass at `ds-start`.
- Before every push: `G-REPO-NOREPLY`. After a push: `G-PLAT-CI`.

When a goal fails, report which goal and which check failed.

- `DS=ds-start..HEAD`, `PLAN_DS=plans/ozen-dev-setup.md`, `OZ=build/core-gcc-release/ozen`.
- `ds_excluded` lists the commits named in §9 as `- Other work's commit in range: <sha>`. `ds_commits` lists `git rev-list --no-merges $DS` without them. Range-based checks first check that the tag exists, and look at the files of `ds_commits` only (`git diff-tree --no-commit-id --name-only -r <c>` per commit).
- Counts that should be 0 are judged by the printed count. A group of goals is judged by the exit status of `scripts/goals.sh`, because its last line names only the last goal.
- P0-3 turns the code block below into `goal_G_DS_*` functions in `scripts/goals.sh`.

### 5.1 Structure and data

| ID | What it decides | Pass condition |
| --- | --- | --- |
| `G-DS-DECISIONS` | D116–D136 decided | decided 21, open 0, decision lines 21 |
| `G-DS-COVERAGE` | every S item has a row; every §8 item has a reason | empty diff, then 0 |
| `G-DS-PLAN-NAMES` | test names in the coverage table exist | `plan-tests: <n> names, 0 missing` |
| `G-DS-PLAN-TESTS` | one mutation record per row with a way to break it | `mutations: <m> rows, <m> recorded` |
| `G-DS-OWN-LABELS` | S1014 | 0 |
| `G-DS-LABELS` | S993–S999, S1012, S1013 | empty diff; 0 labels in checkers; self-tests end with `0 failed`; the goals exit 0 |
| `G-DS-TOOLCHAIN` | S945–S947 | a version and `npx`; node check prints 1; `G-REPO-TOOLS` exits 0 |
| `G-DS-RULESYNC` | S948–S956 | self-test ends with `0 failed`; `exit 0`; 0 changes; set comparison as written; 1 rule; 0 missing outputs |
| `G-DS-AGENT-FILES` | S957–S968 | the counts in the code block |
| `G-DS-HOOK` | S969–S971 | `pre-push: 2 cases, 0 failed` |
| `G-DS-NOREPLY` | S972–S975 | `test-goals.sh` ends with `0 failed`; its `noreply` cases are 4 |
| `G-DS-CI-AGENTS` | S976 | the workflow counts in the code block |
| `G-DS-CI-AGENTS-RUN` | S977 (after a push) | 0 files outside `plans/`; `success` |
| `G-DS-SETUP` | S978–S987 | `setup: <k> cases, 0 failed`; 0 install commands |
| `G-DS-CONCEPT16` | S988 | 0, then 15 |
| `G-DS-ENTRY-DOCS` | S989–S992 | each count at least 1 |
| `G-DS-TRANSLATION` | S1000–S1007, S1011 | `translation: 6 files, 0 problems`; self-test ends with `0 failed` |
| `G-DS-GOALS-SAME` | S1008 | 6 records in §9; each pair has an empty diff |
| `G-DS-CONCEPT-GOALS` | S1009 | 0 Japanese characters; the goals exit 0 |
| `G-DS-JA-PLANS` | S1010 | 0 |
| `G-DS-REFERENCE` | S1015, S1017, S1018 | 6 files; `0 mismatches`; markers 2; 0 Japanese characters |
| `G-DS-REFGEN` | S1019–S1029 | `refgen: 3 files, 0 differ`; self-test ends with `0 failed`; the guide at most 65536 bytes |
| `G-DS-INIT` | S1030, S1031, S1033–S1040, S1074, S1075 | `cli: <k> cases, 0 failed` with 9 `init-*` cases; usage and README counts at least 1 |
| `G-DS-VERSION` | S1041–S1049 | 0 literal versions; 1 placeholder in SARIF; package versions unchanged; self-test ends with `0 failed`; 0 roadmap reads; 3 end-tag reads; the goals exit 0 |
| `G-DS-RELEASE-LOCAL` | S1052, S1054–S1059, S1061, S1062 | `release: <k> cases, 0 failed`; `release-tag: <k> cases, 0 failed` |
| `G-DS-RELEASE-WORKFLOW` | S1050, S1051, S1053, S1060 | the workflow checks in the code block |
| `G-DS-RELEASE-PUBLISHED` | S1063–S1066 (after the user's yes, P4-6) | 4 assets; `OK` lines 3 and the version; `success`; the path |
| `G-DS-SITE` | S1067, S1068 | both links; a positive count; `success` |
| `G-DS-GOLDEN-SAME` | S1069 | 0 |
| `G-DS-SCOPE` | S1070, S1071 | 0, 0, empty diff |
| `G-DS-SELFTESTS` | S1072, S1073 | each count 1; `G-REPO-SELFTESTS` exits 0; `.gitignore` count 1 |
| `G-DS-AUDIT` | P5-3 | at least 1 audit record |

```bash
# G-DS-DECISIONS (21, 0, 21)
grep -c '^### D[0-9]*\. .*\[decided\]' $PLAN_DS
grep -c '^### D[0-9]*\. .*\[open\]' $PLAN_DS
grep -c '^- \*\*Decision:\*\*' $PLAN_DS

# G-DS-COVERAGE (empty output, then 0)
diff <(sed -n '/^## 4\./,/^## 5\./p' $PLAN_DS | grep -oE '\*\*S[0-9]+\*\*' | tr -d '*' | sort -u) \
     <(sed -n '/^### 5\.3/,/^## 6\./p' $PLAN_DS | grep -oE '^\| S[0-9]+' | tr -d '| ' | sort -u)
sed -n '/^## 8\./,/^## 9\./p' $PLAN_DS | grep '^- ' | grep -v 'Reason a command cannot decide:' | wc -l

# G-DS-PLAN-NAMES, G-DS-PLAN-TESTS
scripts/check-plan-tests.sh $PLAN_DS | tail -n 1                # plan-tests: <n> names, 0 missing
scripts/check-plan-tests.sh --mutations $PLAN_DS | tail -n 1    # mutations: <m> rows, <m> recorded

# G-DS-OWN-LABELS (0)
sed '/^### 3\.4/,/^### 3\.5/d; /^| Japanese | English |/,/^- \*\*S993\*\*/d; /^### 5\.1/,/^### 5\.2/d; /^### 5\.3/,/^## 6\./d' $PLAN_DS |
  grep -cE '【決定済み】|【未決】|決定:|機械で決められない理由|わざと壊す|テストまたはゴール|網羅の監査|範囲に入った別の作業|期待を直した'

# G-DS-LABELS (15, 0, three "0 failed", exit 0, then the closed range)
wc -l < scripts/plan-labels.tsv
cut -f1 scripts/plan-labels.tsv | grep -cFf - scripts/check-plan-tests.sh scripts/check-rewrite-equivalence.py scripts/check-lists-copy.py | awk -F: '{s+=$2} END {print s}'
bash scripts/test-check-plan-tests.sh | tail -n 1
bash scripts/test-check-rewrite-equivalence.sh | tail -n 1
bash scripts/test-check-lists-copy.sh | tail -n 1
scripts/goals.sh G-FB-DECISIONS G-FB-COVERAGE G-FB-PLAN-TESTS G-FB-SCOPE > /dev/null 2>&1; echo "exit $?"
grep -E '^FB=' scripts/goals.sh                                  # FB=fb-start..fb-end

# G-DS-TOOLCHAIN (a version and npx, 1, exit 0)
awk '$1 == "rulesync" { print $2, $3 }' toolchain.txt
awk '$1 == "node" { split($2, v, "."); print (v[1] > 22 || (v[1] == 22 && v[2] >= 22)) }' toolchain.txt
scripts/goals.sh G-REPO-TOOLS > /dev/null 2>&1; echo "exit $?"

# G-DS-RULESYNC
bash scripts/test-rulesync.sh | tail -n 1                       # rulesync: <k> cases, 0 failed
scripts/rulesync.sh generate --check > /dev/null; echo "exit $?"   # exit 0
scripts/rulesync.sh generate > /dev/null && git status --porcelain --untracked-files=no | wc -l   # 0
python3 -c 'import json,re; c=json.loads(re.sub(r"(?m)^\s*//.*$","",open("rulesync.jsonc").read())); print(sorted(c["targets"]), sorted(c["features"]))'
                                                                # ['agentsmd', 'claudecode', 'codexcli', 'copilot', 'cursor'] ['permissions', 'rules']
ls .rulesync/rules | wc -l; grep -c '^root: true' .rulesync/rules/overview.md   # 1, 1
while read -r p; do git ls-files --error-unmatch "$p" > /dev/null 2>&1 || echo "$p"; done < scripts/agent-outputs.txt | wc -l   # 0

# G-DS-AGENT-FILES
for f in CLAUDE.md AGENTS.md; do grep -cF '146819854+atsuki-seo@users.noreply.github.com' $f; done   # 1 or more each
for p in 'ozen-roadmap.md.*G-.*plan-start' 'ozen-syntax-refresh.md.*G-RF-.*refresh-start' 'ozen-readme-compact.md.*G-RD-.*readme-start' \
         'ozen-quantity-checks.md.*G-QC-.*qc-start' 'ozen-cooking-feedback.md.*G-FB-.*fb-start' 'ozen-dev-setup.md.*G-DS-.*ds-start'; do
  grep -cE "$p" CLAUDE.md; grep -cE "$p" AGENTS.md; done                                            # 1 each
grep -c 'alwaysApply: true' $(grep '^\.cursor/' scripts/agent-outputs.txt)                          # 1
grep -cx '.github/copilot-instructions.md' scripts/agent-outputs.txt                                 # 1
diff <(python3 -c 'import json; print("\n".join(sorted(json.load(open(".claude/settings.json"))["permissions"]["allow"])))') \
     <(sort scripts/agent-allow.txt)                                                                 # empty
grep -cE 'git (push|commit|tag|reset)|(^| )rm |>' scripts/agent-allow.txt                            # 0
R=.rulesync/rules/overview.md
grep -ciE 'committed .*(are|is) (written )?in English' $R                                            # 1 or more
for s in 'site/' 'MEXT' 'phrases/ja.ftl' 'ozen-cooking-feedback.md' 'ozen-authoring-support.md' 'ozen-dev-environment.md' 'own settings'; do
  grep -cF "$s" $R; done                                                                             # 1 or more each
grep -ciE '(under|in) `?(docs|plans)/`? (are|is) written in Japanese' CLAUDE.md AGENTS.md            # 0 each
grep -cF 'ask the user before pushing' CLAUDE.md AGENTS.md                                           # 0 each
grep -c 'G-REPO-NOREPLY' $R                                                                          # 1 or more
scripts/goals.sh G-REPO-CLAUDEMD G-RD-CLAUDEMD G-QC-CLAUDEMD > /dev/null 2>&1; echo "exit $?"         # exit 0

# G-DS-HOOK, G-DS-NOREPLY
bash scripts/test-pre-push.sh | tail -n 1                        # pre-push: 2 cases, 0 failed
bash scripts/test-goals.sh | tail -n 1                           # goals: <k> cases, 0 failed
grep -c '^noreply_case ' scripts/test-goals.sh                   # 4

# G-DS-CI-AGENTS (1, 0, 1, 1 or more, 1, 1)
A=.github/workflows/agents.yml
grep -c '^  push:' $A
grep -cE 'paths(-ignore)?:' $A
grep -c 'fetch-depth: 0' $A
grep -c '$1 == "node"' $A
grep -c 'goals.sh G-REPO-NOREPLY' $A
grep -c 'rulesync.sh generate --check' $A

# G-DS-CI-AGENTS-RUN (0, success)
c=$(git log --format=%H -1 -- plans/); git diff-tree --no-commit-id --name-only -r $c | grep -vc '^plans/'
gh run list --workflow agents.yml --commit $c --json conclusion --jq '.[0].conclusion'

# G-DS-SETUP
bash scripts/test-setup.sh | tail -n 1                            # setup: <k> cases, 0 failed
grep -cE '\b(sudo|apt(-get)?|brew install|pip install|npm install)\b' scripts/setup.sh   # 0

# G-DS-CONCEPT16 (0, then 15)
grep -c '^## 16\.' docs/concept.md
grep -oE '^## [0-9]+\.' docs/concept.md | tail -n 1 | tr -dc '0-9'

# G-DS-ENTRY-DOCS (each at least 1)
W=$(sed -n '/^## Where things are/,/^## [^W]/p' CONTRIBUTING.md)
for p in docs/ plans/ reference/ site/ README .rulesync/ CLAUDE.md AGENTS.md; do grep '^| ' <<< "$W" | grep -cF "$p"; done
G=$(sed -n '/^## Getting started/,/^## [^G]/p' CONTRIBUTING.md)
for p in scripts/setup.sh scripts/goals.sh .rulesync/; do grep -cF "$p" <<< "$G"; done
grep -cF 'reference/' README.md
grep -cF 'plans/ozen-dev-setup.md' plans/ozen-dev-environment.md

# G-DS-TRANSLATION
python3 -B scripts/check-translation.py --base ds-start docs/concept.md docs/prototype-report.md \
  plans/ozen-roadmap.md plans/ozen-syntax-refresh.md plans/ozen-readme-compact.md plans/ozen-quantity-checks.md | tail -n 1
                                                                 # translation: 6 files, 0 problems
bash scripts/test-check-translation.sh | tail -n 1               # check-translation: <k> cases, 0 failed

# G-DS-GOALS-SAME (6, then 0)
grep -cE '^- Goals before and after P2-[2-7]: `build/ds-goals-P2-[2-7]-before\.txt` and `-after\.txt`, same$' $PLAN_DS
for t in 2 3 4 5 6 7; do diff build/ds-goals-P2-$t-before.txt build/ds-goals-P2-$t-after.txt > /dev/null 2>&1 || echo "P2-$t"; done | wc -l

# G-DS-CONCEPT-GOALS (0, exit 0)
awk '/^[a-z_A-Z0-9]+\(\) \{/ {n=$1; b=""} {b=b "\n" $0} /^}/ {if (n ~ /^goal_G_/ && b ~ /docs\/concept\.md/) print b}' scripts/goals.sh |
  grep -cP '[\x{3040}-\x{30ff}\x{4e00}-\x{9fff}]'
ids=$(awk '/^[a-z_A-Z0-9]+\(\) \{/ {n=$1; b=""} {b=b "\n" $0} /^}/ {if (n ~ /^goal_G_/ && b ~ /docs\/concept\.md/) print n}' scripts/goals.sh |
  sed 's/^goal_//; s/()$//; s/_/-/g')
scripts/goals.sh $ids > /dev/null 2>&1; echo "exit $?"

# G-DS-JA-PLANS (0)
for c in $(ds_commits); do git diff-tree --no-commit-id --name-only -r $c; done |
  grep -cxE 'plans/ozen-(cooking-feedback|authoring-support)\.md'

# G-DS-REFERENCE (6, 0 mismatches, 2, 0)
ls reference | grep -c '\.md$'
scripts/check-readme-example.sh --readme reference/language.md --ozen $OZ | tail -n 1
grep -cE '^<!-- ai-guide:(begin|end) -->$' reference/language.md
cat reference/*.md | grep -cP '[\x{3040}-\x{30ff}\x{4e00}-\x{9fff}]'

# G-DS-REFGEN
python3 -B tools/refgen/refgen.py --check | tail -n 1           # refgen: 3 files, 0 differ
bash scripts/test-refgen.sh | tail -n 1                          # refgen: <k> cases, 0 failed
[ "$(wc -c < reference/ai-guide.md)" -le 65536 ] && echo ok                      # ok

# G-DS-INIT
scripts/run-cli-tests.sh $OZ tests/cli | tail -n 1               # cli: <k> cases, 0 failed
ls tests/cli/init-*.sh | wc -l                                   # 9
$OZ 2>&1 | grep -c 'ozen init'; grep -c 'ozen init' cli/README.md

# G-DS-VERSION
grep -rlE '"ozenVersion":"[0-9]' tests/golden | wc -l            # 0
grep -c '{{STD_VERSION}}' tests/golden/api/sarif-mapping.out     # 1
git diff ds-start -- samples tests ':!tests/golden/bundle/std-version-check.*' | grep -E '^[-+][^-+]' | grep -cE '(version|std): "[0-9]'   # 0
bash scripts/test-bump-version.sh | tail -n 1                    # bump-version: <k> cases, 0 failed
sed -n '/^goal_G_REPO_VERSION/,/^}/p' scripts/goals.sh | grep -c 'PLAN'                      # 0
sed -n '/^goal_G_\(RD\|QC\|FB\)_VERSION/,/^}/p' scripts/goals.sh | grep -cE 'git show (readme|qc|fb)-end:VERSION'   # 3
scripts/goals.sh G-REPO-VERSION G-RD-VERSION G-QC-VERSION G-FB-VERSION G-CORE-GOLDEN > /dev/null 2>&1; echo "exit $?"   # exit 0

# G-DS-RELEASE-LOCAL
bash scripts/test-check-release-tag.sh | tail -n 1               # release-tag: <k> cases, 0 failed
bash scripts/test-make-release.sh | tail -n 1                    # release: <k> cases, 0 failed

# G-DS-RELEASE-WORKFLOW (1, 0, one "ok" line per job and no "bad", 1, 0, 1, 1)
F=.github/workflows/release.yml
grep -cE "tags: *\[ *'?v\*'? *\]" $F
grep -cE '^\s*(branches|pull_request|workflow_dispatch|schedule):' $F
scripts/check-release-tag.sh --workflow-first-step $F
grep -cE '^\s*contents: write' $F
grep -oE 'secrets\.[A-Z_]+' $F | grep -vc '^secrets\.GITHUB_TOKEN$'
grep -c 'gh release create' $F
grep -c 'lipo -archs' $F

# G-DS-RELEASE-PUBLISHED (4; 3 and the version; success; the path)
v=$(cat VERSION); t="v$v"
gh release view "$t" --json assets --jq '.assets[].name' |
  grep -cxE "ozen-$v-(linux-x86_64\.tar\.gz|macos-universal\.tar\.gz|wasm\.zip)|SHA256SUMS"
d=build/ds-release; rm -rf $d; mkdir -p $d; gh release download "$t" -D $d
(cd $d && sha256sum -c SHA256SUMS | grep -c ': OK$' && tar xzf ozen-$v-linux-x86_64.tar.gz && ozen-$v/bin/ozen --version)
gh run list --workflow release.yml --commit "$(git rev-list -n 1 "$t")" --json conclusion --jq '.[0].conclusion'
gh api "repos/atsuki-seo/Ozen/contents/reference/ai-guide.md?ref=$t" --jq .path

# G-DS-SITE (1, 1, a positive count, success)
grep -cF 'https://github.com/atsuki-seo/Ozen/tree/main/reference"' site/index.html
grep -cF 'https://github.com/atsuki-seo/Ozen/releases' site/index.html
gh api 'repos/atsuki-seo/Ozen/contents/reference?ref=main' --jq length
gh run list --workflow pages.yml --commit "$(git log --format=%H -1 -- site/index.html)" --json conclusion --jq '.[0].conclusion'

# G-DS-GOLDEN-SAME (0)
norm() { sed -E 's/"ozenVersion":"[^"]*"/"ozenVersion":"V"/g; s/\{\{STD_VERSION\}\}/V/g'; }
for c in $(ds_commits); do git diff-tree --no-commit-id --name-only -r $c; done | sort -u |
  grep -E '^tests/golden/.*\.out$' | grep -v '^tests/golden/bundle/std-version-check' |
  while read -r f; do cmp -s <(git show ds-start:"$f" 2>/dev/null | norm) <(norm < "$f") || echo "$f"; done | wc -l

# G-DS-SCOPE (0, 0, empty)
for c in $(ds_commits); do git diff-tree --no-commit-id --name-only -r $c; done | sort -u |
  grep -vxEf <(cut -d' ' -f1 scripts/ds-scope.txt) | wc -l
grep -c '^$' scripts/ds-scope.txt
diff <(sed -n '/^### 4\.13/,/^### 4\.14/p' $PLAN_DS | grep -oE '`[^`]+`' | tr -d '`' | sort -u) <(cut -d' ' -f2- scripts/ds-scope.txt | sort -u)

# G-DS-SELFTESTS (1 each, exit 0, 1)
for s in rulesync.sh setup.sh check-translation.py bump-version.sh check-release-tag.sh make-release.sh; do grep -c "^scripts/$s " scripts/selftests.txt; done
scripts/goals.sh G-REPO-SELFTESTS > /dev/null 2>&1; echo "exit $?"
grep -cx 'dist/' .gitignore

# G-DS-AUDIT (at least 1)
grep -cE '^- Coverage audit: [0-9]+ findings, [0-9]+ taken' $PLAN_DS
```

`scripts/ds-scope.txt` holds one line per path of §4.13: a regular expression for `grep -xE`, a space, and the path as written in §4.13. `scripts/check-release-tag.sh --workflow-first-step FILE` reads the workflow and prints `ok <job>` for each job whose first step after the checkout runs the tag check, or `bad <job>` otherwise, and exits 1 if any job is `bad`. New checkers come with a self-test that feeds a broken input and expects a failure.

### 5.2 Tests

- Tests that need a remote or a clone use a throwaway directory under `build/` and a local bare repository; they never touch `origin`.
- Tests that change `HOME`, the git configuration or `PATH` change them for that test only, using throwaway directories and fake tools.
- Expected values come from the files themselves (the TSV, `diag_codes.def`, the definitions JSON, `README.md`, the imports of `bindings/wasm/`), not from the code under test.
- Commit order follows the open ranges of §3.8. A new CLI test is committed before the implementation that makes it pass. Golden expectations are never changed in the same commit as `core/`, `stdlib/` or `cli/src/`.

| Test | What it decides | Main task |
| --- | --- | --- |
| `sh/test-rulesync.sh` | S948–S950, S955 with a fake `npx` on `PATH`, and S956 with a fake generator that writes one extra file | P1-2 |
| `sh/test-goals.sh` | S972–S975 (four `noreply_case` cases with a throwaway `HOME` and repository), and S1012 | P1-5, P0-3 |
| `sh/test-pre-push.sh` | S970, S971 | P1-6 |
| `sh/test-setup.sh` | S978–S986 with a fake `toolchain.txt`, fake tools, a fake `uname`, a fake failing `npx`, and a preset `core.hooksPath` | P1-7 |
| `sh/test-check-plan-tests.sh` | S995, S996 (cases added for English labels) | P0-3 |
| `sh/test-check-rewrite-equivalence.sh` | S997 (case added) | P0-3 |
| `sh/test-check-lists-copy.sh` | S998 (case added) | P0-3 |
| `sh/test-check-translation.sh` | S1000–S1007 | P2-1 |
| `sh/test-refgen.sh` | S1019–S1023, S1026, S1028, S1029 against the definitions JSON, `diag_codes.def` and a fixture `std` with a `local` action | P3-2 |
| `cli/init-*` (9 cases) | S1030, S1031, S1033–S1038, S1040, S1074, S1075 | P3-4 |
| `sh/test-bump-version.sh` | S1044–S1047, S1049 | P4-2 |
| `sh/test-check-release-tag.sh` | S1051, S1052 | P4-5 |
| `sh/test-make-release.sh` | S1054–S1059, S1061, S1062 for `linux` and `wasm` (macOS runs in the release workflow) | P4-4 |

A failing test is not loosened. Its cause is found, and a difference from the spec is reported to the user.

Commands decide everything above, including what runs on GitHub, through `gh`. Only three things go to §8: how each agent follows the generated rules, how the archives behave on a Mac outside CI, and the wording of the translation and of the hand-written reference.

### 5.3 Coverage table

Aspects: Normal, Boundary, No-effect condition, Side effects, Time and order, Persistence, Real path, Data completeness, Platform path, Docs match, Fixed premise.

| S item | Aspect | Test or goal | How to break it | Main task |
| --- | --- | --- | --- | --- |
| S945 | Data completeness | `G-DS-TOOLCHAIN` | drop the `npx` column | P1-1 |
| S946 | Fixed premise | `G-DS-TOOLCHAIN` | set node to 22.21.0 | P1-1 |
| S947 | Fixed premise | `G-DS-TOOLCHAIN` | give the `rulesync` row an empty third column | P1-1 |
| S948 | Normal | `sh/test-rulesync.sh` | drop `--yes` from the wrapper | P1-2 |
| S949 | No-effect condition | `sh/test-rulesync.sh` | exit 0 when the line is missing | P1-2 |
| S950 | Side effects | `sh/test-rulesync.sh` | pass `--delete` through | P1-2 |
| S951 | Data completeness | `G-DS-RULESYNC` | remove `cursor` from the targets | P1-2 |
| S952 | Data completeness | `G-DS-RULESYNC` | add a second rule file | P1-2 |
| S953 | Persistence | `G-DS-RULESYNC` | edit `AGENTS.md` by hand | P1-2 |
| S954 | Normal | `G-DS-RULESYNC` | commit a source edit without generating | P1-2 |
| S955 | No-effect condition | `sh/test-rulesync.sh` | make the wrapper drop `--check` | P1-2 |
| S956 | Data completeness | `sh/test-rulesync.sh` | leave one output out of `scripts/agent-outputs.txt` | P1-2 |
| S956 | Persistence | `G-DS-RULESYNC` | list a path that is not tracked | P1-2 |
| S957 | Data completeness | `G-DS-AGENT-FILES` | drop the noreply address from the source | P1-2 |
| S958 | Data completeness | `G-DS-AGENT-FILES` | split this plan's line into two lines | P1-2 |
| S959 | Platform path | `G-DS-AGENT-FILES` | set `alwaysApply: false` | P1-2 |
| S960 | Data completeness | `G-DS-AGENT-FILES` | add a line to `scripts/agent-allow.txt` without generating | P1-3 |
| S961 | Side effects | `G-DS-AGENT-FILES` | add `git push` to the allow list | P1-3 |
| S962 | Docs match | `G-DS-AGENT-FILES` | drop the English sentence | P1-4 |
| S963 | Docs match | `G-DS-AGENT-FILES` | drop `phrases/ja.ftl` from the exceptions | P1-4 |
| S964 | Docs match | `G-DS-AGENT-FILES` | drop `ozen-dev-environment.md` | P1-4 |
| S965 | Docs match | `G-DS-AGENT-FILES` | drop the sentence about own settings | P1-4 |
| S966 | Docs match | `G-DS-AGENT-FILES` | keep the old sentence about `plans/` | P1-4 |
| S967 | Docs match | `G-DS-AGENT-FILES` | keep "ask the user before pushing" | P1-4 |
| S968 | Fixed premise | `G-DS-AGENT-FILES` | drop the word Japanese from the rule | P1-4 |
| S969 | Data completeness | `sh/test-pre-push.sh` | remove the execute bit of the hook | P1-6 |
| S970 | Real path | `sh/test-pre-push.sh` | make the hook `exit 0` | P1-6 |
| S971 | Normal | `sh/test-pre-push.sh` | make the hook `exit 1` | P1-6 |
| S972 | Boundary | `sh/test-goals.sh` | remove the empty-address branch | P1-5 |
| S973 | Boundary | `sh/test-goals.sh` | compare with a wrong noreply address | P1-5 |
| S974 | No-effect condition | `sh/test-goals.sh` | skip whenever the address is set | P1-5 |
| S975 | Platform path | `sh/test-goals.sh` | skip check 2 whenever the local address is missing | P1-5 |
| S976 | Real path | `G-DS-CI-AGENTS` | add `paths-ignore: [plans/**]` | P1-8 |
| S976 | Platform path | `G-DS-CI-AGENTS` | drop `fetch-depth: 0` | P1-8 |
| S977 | Platform path | `G-DS-CI-AGENTS-RUN` | run `agents.yml` only for `scripts/**` | P1-8 |
| S978 | Normal | `sh/test-setup.sh` | print `ok` for a different version | P1-7 |
| S979 | Platform path | `sh/test-setup.sh` | probe the `xcode` row on Linux | P1-7 |
| S980 | Side effects | `sh/test-setup.sh` | drop the `core.hooksPath` line | P1-7 |
| S981 | No-effect condition | `sh/test-setup.sh` | overwrite an existing `core.hooksPath` | P1-7 |
| S982 | Normal | `sh/test-setup.sh` | skip the generate step | P1-7 |
| S983 | Boundary | `sh/test-setup.sh` | exit 1 when `npx` fails | P1-7 |
| S984 | Time and order | `sh/test-setup.sh` | append a timestamp line to a file | P1-7 |
| S985 | Boundary | `sh/test-setup.sh` | exit 0 when `cmake` differs | P1-7 |
| S985 | Platform path | `sh/test-setup.sh` | require `gcc` on a fake macOS | P1-7 |
| S986 | No-effect condition | `sh/test-setup.sh` | exit 1 when `emcc` is missing | P1-7 |
| S987 | Side effects | `G-DS-SETUP` | add `sudo apt-get install` | P1-7 |
| S988 | Data completeness | `G-DS-CONCEPT16` | restore section 16 | P1-9 |
| S989 | Docs match | `G-DS-ENTRY-DOCS` | drop the `reference/` row | P1-10 |
| S990 | Docs match | `G-DS-ENTRY-DOCS` | drop the `scripts/setup.sh` line | P1-10 |
| S991 | Docs match | `G-DS-ENTRY-DOCS` | drop the `reference/` link | P3-5 |
| S992 | Docs match | `G-DS-ENTRY-DOCS` | drop the pointer line | P0-3 |
| S993 | Data completeness | `G-DS-LABELS` | delete the `Total` row of the TSV | P0-3 |
| S994 | Fixed premise | `G-DS-LABELS` | write the Japanese mutation column name inline in the checker | P0-3 |
| S995 | Normal | `sh/test-check-plan-tests.sh` | read only the Japanese column name | P0-3 |
| S996 | Boundary | `sh/test-check-plan-tests.sh` | read only the Japanese count form | P0-3 |
| S997 | Normal | `sh/test-check-rewrite-equivalence.sh` | read only the Japanese record | P0-3 |
| S998 | Normal | `sh/test-check-lists-copy.sh` | skip only the Japanese total row | P0-3 |
| S999 | Real path | `G-DS-LABELS` | make the feedback plan's decision goal read English | P0-3 |
| S1000 | Normal | `sh/test-check-translation.sh` | leave one Japanese sentence in a test file | P2-1 |
| S1000 | Real path | `G-DS-TRANSLATION` | leave one Japanese sentence in `docs/concept.md` | P2-2 |
| S1001 | Normal | `sh/test-check-translation.sh` | leave a Japanese shell comment | P2-1 |
| S1002 | Data completeness | `sh/test-check-translation.sh` | drop an S item | P2-1 |
| S1003 | Data completeness | `sh/test-check-translation.sh` | renumber a heading | P2-1 |
| S1004 | Data completeness | `sh/test-check-translation.sh` | change a flag in a command | P2-1 |
| S1005 | Data completeness | `sh/test-check-translation.sh` | delete a table row | P2-1 |
| S1006 | Data completeness | `sh/test-check-translation.sh` | add an allow entry without a reason | P2-1 |
| S1007 | Data completeness | `sh/test-check-translation.sh` | add an allow entry that no file uses | P2-1 |
| S1008 | Real path | `G-DS-GOALS-SAME` | translate a label a goal reads without changing the goal | P2-4 |
| S1009 | Real path | `G-DS-CONCEPT-GOALS` | leave the concept grep of `G-QC-SYNTAX` in Japanese | P2-2 |
| S1010 | No-effect condition | `G-DS-JA-PLANS` | translate a line of the feedback plan | P2-8 |
| S1011 | Docs match | `G-DS-TRANSLATION` | rewrite one concept §16 reference in the roadmap | P2-4 |
| S1012 | Normal | `sh/test-goals.sh` | stop `--summary` at the first failure | P0-3 |
| S1013 | Fixed premise | `G-DS-LABELS` | leave `FB=fb-start..HEAD` | P0-3 |
| S1014 | Data completeness | `G-DS-OWN-LABELS` | write the Japanese decided label in a heading of this plan | P0-3 |
| S1015 | Data completeness | `G-DS-REFERENCE` | delete `reference/std.md` | P3-2 |
| S1015 | Docs match | `G-DS-REFERENCE` | write one Japanese sentence in `reference/language.md` | P3-1 |
| S1017 | Docs match | `G-DS-REFERENCE` | change an expected output line | P3-1 |
| S1018 | Data completeness | `G-DS-REFERENCE` | remove the end marker | P3-1 |
| S1019 | Data completeness | `sh/test-refgen.sh` | include `local` actions | P3-2 |
| S1020 | Normal | `sh/test-refgen.sh` | omit required states | P3-2 |
| S1021 | Normal | `sh/test-refgen.sh` | print the action name instead of its text | P3-2 |
| S1022 | Data completeness | `sh/test-refgen.sh` | skip the last line of `diag_codes.def` | P3-2 |
| S1023 | Boundary | `sh/test-refgen.sh` | print nothing when a phrase is missing | P3-2 |
| S1024 | Normal | `G-DS-REFGEN` | leave out the work loop | P3-3 |
| S1025 | Boundary | `G-DS-REFGEN` | write the full `std.md` into the guide | P3-3 |
| S1026 | Data completeness | `sh/test-refgen.sh` | drop the generated-file line | P3-2 |
| S1027 | Persistence | `G-DS-REFGEN` | edit `reference/std.md` by hand | P3-2 |
| S1028 | Time and order | `sh/test-refgen.sh` | iterate a set without sorting | P3-2 |
| S1029 | Real path | `sh/test-refgen.sh` | make `--check` exit 0 always | P3-2 |
| S1030 | Normal | `cli/init-basic` | write `version: "0.0.1"` | P3-4 |
| S1031 | Normal | `cli/init-basic` | leave `@VERSION@` in place | P3-4 |
| S1033 | Docs match | `cli/init-basic` | drop the `@AGENTS.md` line | P3-4 |
| S1034 | No-effect condition | `cli/init-exists` | write `package.ozen` before checking `AGENTS.md` | P3-4 |
| S1035 | Boundary | `cli/init-bad-name` | accept `std` | P3-4 |
| S1035 | Boundary | `cli/init-path-name` | accept a folder named `my-book` as the package name | P3-4 |
| S1074 | Normal | `cli/init-here` | write into `./new` when no folder is given | P3-4 |
| S1075 | Normal | `cli/init-name` | ignore `--name` | P3-4 |
| S1036 | Real path | `cli/init-check` | write the package without a version | P3-4 |
| S1037 | Normal | `cli/init-link` | link `blob/main` | P3-4 |
| S1038 | Docs match | `cli/init-template` | remove `--format json` from the template | P3-4 |
| S1039 | Docs match | `G-DS-INIT` | leave `init` out of the usage text | P3-4 |
| S1040 | Time and order | `cli/init-rebuild` | drop the template from the build dependencies | P3-4 |
| S1041 | Normal | `G-DS-VERSION` | leave a literal version in the SARIF expectation | P4-1 |
| S1042 | Data completeness | `G-DS-VERSION` | restore one `"ozenVersion":"0.1.0"` | P4-1 |
| S1043 | Boundary | `G-DS-VERSION` | replace a sample package version too | P4-1 |
| S1044 | Fixed premise | `sh/test-bump-version.sh` | keep `std: "0.2.0"` in `std-version-check` | P4-1 |
| S1045 | Side effects | `sh/test-bump-version.sh` | also rewrite `README.md` | P4-2 |
| S1046 | Boundary | `sh/test-bump-version.sh` | accept `1.2` | P4-2 |
| S1047 | Real path | `sh/test-bump-version.sh` | keep `stdlib/package.ozen` unchanged | P4-2 |
| S1048 | Fixed premise | `G-DS-VERSION` | read `PLAN` again in `G-REPO-VERSION` | P4-3 |
| S1049 | Fixed premise | `G-DS-VERSION` | read `VERSION` at `HEAD` in `G-FB-VERSION` | P4-3 |
| S1049 | Real path | `sh/test-bump-version.sh` | read `VERSION` at `HEAD` in `G-QC-VERSION` | P4-3 |
| S1050 | No-effect condition | `G-DS-RELEASE-WORKFLOW` | add `branches: [main]` | P4-5 |
| S1051 | Time and order | `G-DS-RELEASE-WORKFLOW` | move the tag check after the build | P4-5 |
| S1052 | Boundary | `sh/test-check-release-tag.sh` | accept `0.1.0` without `v` | P4-5 |
| S1053 | Side effects | `G-DS-RELEASE-WORKFLOW` | use `secrets.PAT` | P4-5 |
| S1054 | Normal | `sh/test-make-release.sh` | name the archive without the version | P4-4 |
| S1055 | Data completeness | `sh/test-make-release.sh` | include `renderers/test/` | P4-4 |
| S1056 | Docs match | `sh/test-make-release.sh` | pack a README without the `node` sentence | P4-4 |
| S1057 | Real path | `sh/test-make-release.sh` | build without `VERSION` | P4-4 |
| S1058 | Real path | `sh/test-make-release.sh` | leave out `renderers/` | P4-4 |
| S1059 | Platform path | `sh/test-make-release.sh` | look for the renderer only next to the current folder | P4-4 |
| S1060 | Platform path | `G-DS-RELEASE-WORKFLOW` | build the macOS binary for `arm64` only | P4-5 |
| S1061 | Data completeness | `sh/test-make-release.sh` | write the sums before the last archive | P4-4 |
| S1062 | Platform path | `sh/test-make-release.sh` | leave out `ozen_bridge.js` | P4-4 |
| S1063 | Platform path | `G-DS-RELEASE-PUBLISHED` | upload without `SHA256SUMS` | P4-6 |
| S1064 | Real path | `G-DS-RELEASE-PUBLISHED` | upload an archive built before the bump | P4-6 |
| S1065 | Platform path | `G-DS-RELEASE-PUBLISHED` | make the macOS job fail | P4-6 |
| S1066 | Docs match | `G-DS-RELEASE-PUBLISHED` | tag a commit without `reference/` | P4-6 |
| S1067 | Docs match | `G-DS-SITE` | drop the releases link | P3-5 |
| S1068 | Platform path | `G-DS-SITE` | push `site/` before `reference/` | P3-5 |
| S1069 | Side effects | `G-DS-GOLDEN-SAME` | change a non-version part of an expectation | P5-4 |
| S1070 | Side effects | `G-DS-SCOPE` | change a file outside §4.13 | P5-4 |
| S1071 | Data completeness | `G-DS-SCOPE` | add an empty line to `scripts/ds-scope.txt` | P5-2 |
| S1072 | Data completeness | `G-DS-SELFTESTS` | leave `setup.sh` out of `scripts/selftests.txt` | P5-2 |
| S1073 | Side effects | `G-DS-SELFTESTS` | remove `dist/` from `.gitignore` | P4-4 |

Coverage check (run after P0-3 and in P5-3; empty output, then 0): the `G-DS-COVERAGE` block in §5.1.

Findings of the independent audit that were not taken, with reasons:

- None. All 45 findings of the audit on 2026-11-03 were taken (§9).

---

## 6. Phases and tasks

Notation:

- **Completion conditions** are goal IDs from §5 or commands with one result. The prerequisite goals are implied in every task.
- Status is reported per area. Example: "agent rules: goals pass. translation: not started. reference: goals pass. releases: local tests pass, workflow not yet run on GitHub."
- Commits are per task or smaller logical units, in the order that §5.2 requires. Pushes need no question (D116); `G-REPO-NOREPLY` runs before each.
- Each task's completion condition includes the mutation checks of the coverage rows it owns. Break the implementation as the row says, see the test fail, restore it, see it pass, and record the result in §9.

### Phase 0 — Decisions and preparation (no change to features)

| ID | Task | Completion condition |
| --- | --- | --- |
| P0-1 | Decide D116–D136 with the user and write the decisions into this file | `G-DS-DECISIONS` |
| P0-2 | Check that the feedback plan's final gate has passed and that `fb-end` exists (set it on that plan's last commit if missing). Tag `ds-start`. Record in §9: the summary of all goals, the newest Node.js 24 LTS and rulesync versions, and any other work in progress | `git rev-parse --verify fb-end` and `git rev-parse --verify ds-start` succeed; §9 has the four records |
| P0-3 | Add `scripts/plan-labels.tsv` and make the plan checkers read both labels (D123). Add `--summary` to `scripts/goals.sh`. Close `FB` at `fb-end`. Add `PLAN_DS`, `DS`, `ds_excluded`, `ds_commits` and the `goal_G_DS_*` functions. Add this plan's line to the plan list and the pointer line to `plans/ozen-dev-environment.md` | `G-DS-LABELS`, `G-DS-OWN-LABELS`, `G-DS-COVERAGE`, `G-DS-PLAN-NAMES`, `G-DS-ENTRY-DOCS` (pointer line only) |

### Phase 1 — Agent rules and setup (A)

| ID | Task | Completion condition |
| --- | --- | --- |
| P1-1 | Raise `node` in `toolchain.txt` (D119) and add `rulesync <v> npx`. After the user's yes, install that Node.js under the home directory. Run the renderer and WebAssembly goals on it | `G-DS-TOOLCHAIN`, `G-REND-TESTS`, `G-PLAT-WASM` |
| P1-2 | `scripts/rulesync.sh` and its test, `rulesync.jsonc`, and `.rulesync/rules/overview.md` from the current `CLAUDE.md`. Generate and commit the outputs, and list them in `scripts/agent-outputs.txt` | `G-DS-RULESYNC`; the noreply, plan-line and Cursor/Copilot lines of `G-DS-AGENT-FILES`; every prerequisite goal that reads `CLAUDE.md` |
| P1-3 | `scripts/agent-allow.txt` and `.rulesync/permissions` (D121) | `G-DS-AGENT-FILES` (allow-list lines) |
| P1-4 | Rewrite the language rule (EQ12, EQ13) and the push rule (D136) in `.rulesync/rules/overview.md`, and generate | `G-DS-AGENT-FILES` |
| P1-5 | `G-REPO-NOREPLY` changes (D122) and the `noreply_case` cases in `scripts/test-goals.sh` | `G-DS-NOREPLY`, `G-REPO-NOREPLY` |
| P1-6 | `.githooks/pre-push` and `scripts/test-pre-push.sh` | `G-DS-HOOK` |
| P1-7 | `scripts/setup.sh` and `scripts/test-setup.sh` | `G-DS-SETUP` |
| P1-8 | `.github/workflows/agents.yml`; push it, then push a commit that changes only `plans/` (the §9 records of this phase) | `G-DS-CI-AGENTS`, `G-DS-CI-AGENTS-RUN` |
| P1-9 | Delete concept §16 | `G-DS-CONCEPT16`, every prerequisite goal that reads `docs/concept.md` |
| P1-10 | `CONTRIBUTING.md`: where things are, and getting started | `G-DS-ENTRY-DOCS` (except the README lines) |
| P1-11 | Phase gate | every prerequisite goal and every goal of Phases 0 and 1 |

### Phase 2 — English documents (B)

Each translation task writes `build/ds-goals-P2-<n>-before.txt` before its commit and `build/ds-goals-P2-<n>-after.txt` after it (`scripts/goals.sh --summary` over every goal not in `scripts/ds-goals-skip.txt`). It records them in §9 as ``- Goals before and after P2-<n>: `build/ds-goals-P2-<n>-before.txt` and `-after.txt`, same``.

| ID | Task | Completion condition |
| --- | --- | --- |
| P2-1 | `scripts/check-translation.py`, `scripts/translation-allow.txt`, `scripts/ds-goals-skip.txt`, and `scripts/test-check-translation.sh` | `sh/test-check-translation.sh` ends with `0 failed` |
| P2-2 | Translate `docs/concept.md`, and in the same commit rewrite every concept grep found by content (`G-RF-CONCEPT`, `G-RF-CONCEPT-PARENS`, `G-QC-SYNTAX`, `G-FB-CONCEPT`, `G-FB-DIAG`) | `G-DS-TRANSLATION` reports no problem for this file; `G-DS-CONCEPT-GOALS`; the P2-2 summaries are equal |
| P2-3 | Translate `docs/prototype-report.md` | the same, for this file and P2-3 |
| P2-4 | Translate `plans/ozen-roadmap.md` and switch its label greps to English | the same, for this file and P2-4 |
| P2-5 | Translate `plans/ozen-syntax-refresh.md` and its label greps | the same, for this file and P2-5 |
| P2-6 | Translate `plans/ozen-readme-compact.md` and its label greps | the same, for this file and P2-6 |
| P2-7 | Translate `plans/ozen-quantity-checks.md` and its label greps | the same, for this file and P2-7 |
| P2-8 | Phase gate | `G-DS-TRANSLATION`, `G-DS-GOALS-SAME`, `G-DS-JA-PLANS`, every prerequisite goal |

### Phase 3 — Reference and `ozen init` (C)

| ID | Task | Completion condition |
| --- | --- | --- |
| P3-1 | `reference/README.md` and `reference/language.md`, with checked examples and the guide markers | `G-DS-REFERENCE` (the hand-written files) |
| P3-2 | `tools/refgen/refgen.py` for `std.md` and `diagnostics.md`, with `scripts/test-refgen.sh` | `G-DS-REFGEN` (except the guide), `G-DS-REFERENCE` |
| P3-3 | `ai-guide.md` in `refgen.py` (D127) | `G-DS-REFGEN` |
| P3-4 | `reference/agents-template.md`; the CLI tests `tests/cli/init-*.sh`, committed first; then `ozen init` (D128, D129) and `cli/README.md` | `G-DS-INIT`, `G-CLI-TESTS`, `G-REPO-RED-FIRST` |
| P3-5 | Links in `README.md` and `site/index.html` (D135); push | `G-DS-ENTRY-DOCS`, `G-DS-SITE` |
| P3-6 | Phase gate | every prerequisite goal and every goal of Phases 0, 1 and 3 |

### Phase 4 — Version and releases (D)

| ID | Task | Completion condition |
| --- | --- | --- |
| P4-1 | `{{STD_VERSION}}` in the version spots of the expectations, and `std: "99.0.0"` in `std-version-check` (D132), in a commit that changes no implementation | `G-DS-VERSION` (first three lines), `G-CORE-GOLDEN`, `G-RF-SEPARATION` |
| P4-2 | `scripts/bump-version.sh` and its test | `G-DS-VERSION` |
| P4-3 | `G-REPO-VERSION`, `G-RD-VERSION`, `G-QC-VERSION` and `G-FB-VERSION` (D133) | `G-DS-VERSION` |
| P4-4 | `scripts/make-release.sh` and its test; `dist/` in `.gitignore` | `G-DS-RELEASE-LOCAL` (release cases) |
| P4-5 | `scripts/check-release-tag.sh` with its test, and `.github/workflows/release.yml` | `G-DS-RELEASE-LOCAL`, `G-DS-RELEASE-WORKFLOW` |
| P4-6 | Ask the user in chat. On yes: tag `v0.1.0` on `main` without a bump and push the tag (D134) | `G-DS-RELEASE-PUBLISHED`, `G-REPO-VERSION` |

### Phase 5 — Documents and finish

| ID | Task | Completion condition |
| --- | --- | --- |
| P5-1 | `cli/README.md`, `CONTRIBUTING.md` and `site/README.md` mention what this plan added | `G-DS-ENTRY-DOCS`, `G-DS-INIT` |
| P5-2 | `scripts/ds-scope.txt` from §4.13, and the `scripts/selftests.txt` entries | `G-DS-SCOPE`, `G-DS-SELFTESTS` |
| P5-3 | Coverage audit. Add the facts found in §9 to the coverage table, and check each §8 item again. Ask an independent subagent to list claims without a test and §8 items that a command can decide; give it §1, §4, §8, the coverage table and the test sources. Take each finding, or write why not at the end of §5.3 | `G-DS-COVERAGE`, `G-DS-PLAN-TESTS`, `G-DS-AUDIT` |
| P5-4 | Final gate; hand §8 to the user | every prerequisite goal and every `G-DS-*` goal, including `G-DS-GOLDEN-SAME` and `G-DS-SCOPE` |

---

## 7. Dependencies between phases

- The feedback plan's final gate comes before P0-2 (D118).
- Phase 0 comes first. P0-3 must come before any `G-DS-PLAN-*` goal is run, because the checkers learn the English labels there.
- P1-1 (Node.js) comes before P1-2 (rulesync needs it). P1-9 comes before P2-2 (concept §16 is already gone when it is translated).
- Phase 2, Phase 3 and Phase 4 are independent of each other after Phase 1, except for three files. `scripts/goals.sh` is edited in P2-2 to P2-7 and in P4-3. `README.md` and `cli/README.md` are edited in P3-4, P3-5 and P5-1. These three files are edited by one task at a time, never in parallel.
- P3-4 needs P3-3, because the template links the guide. P4-4 needs P3-3, because the archive holds `reference/`.
- P4-6 needs every other task of Phases 1, 3 and 4, and the user's yes.

---

## 8. Checks handed to the user

Claude covers the machine-checked goals. The user checks these:

- Open the repository in Claude Code, Codex CLI, Cursor and Copilot, and see that each one follows the language rule and the publishing rule — Reason a command cannot decide: the files are checked for place and form (`G-DS-AGENT-FILES`), but whether an agent follows a rule depends on the agent.
- Run the macOS archive on your own Mac (download, extract, `bin/ozen render`) — Reason a command cannot decide: the release workflow runs it on a GitHub macOS runner, and a Mac outside CI is not reachable from the goals.
- Read the translated documents and the hand-written reference for natural English and Japanese — Reason a command cannot decide: the checker proves that nothing was dropped or changed, not that the wording reads well.

---

## 9. Facts to record during the work

(Append results here with dates.)

- Independent audit of this plan (2026-11-03): 45 findings, all taken. The main changes:
  - D118 now waits for the feedback plan's end.
  - D132 reuses `{{STD_VERSION}}`.
  - D122 covers check 2 and the checkout depth.
  - D136 was added.
  - The commands of `G-DS-OWN-LABELS`, `G-DS-GOALS-SAME`, `G-DS-SCOPE`, `G-DS-GOLDEN-SAME`, `G-DS-RULESYNC`, `G-DS-CONCEPT-GOALS` and the release goals were fixed.
  - Three §8 items became goals.
- `fb-end` and the summary of all goals at `ds-start` (P0-2)
- Node.js and rulesync versions chosen (P0-2)
- Output paths written by rulesync for this config (P1-2)
- Goal summaries before and after each translation task (P2-2 to P2-7)
- Mutation checks (each task): `- Mutation check: <test> — <how it was broken>: failed (<message>). Restored: pass`
- Coverage audit (P5-3): `- Coverage audit: <k> findings, <m> taken`
- Other work's commits in the range: `- Other work's commit in range: <sha>`
- `fb-end` (P0-2, 2026-11-03): the feedback plan's final gate is recorded in `4cc5906` (P9-4 of that plan, run at `667fa46`). That plan had not set `fb-end`, so P0-2 set it on `4cc5906`, the plan's last commit. `ds-start` is `2d2f971`.
- Goals at `ds-start` (P0-2, 2026-11-03): `build/ds-goals-start.txt`, 171 goals run, 169 pass. Not run: `G-CORE-FUZZ`, `G-CORE-BUILD`, `G-REPO-RED-FIRST`, `G-PLAT-CI`, `G-REPO-TOOLS-CI` and `G-RD-GH-*` (the network and rebuild goals of D124). The two failures are known: `G-FB-SCOPE` counts the two plan commits after `fb-end` (P0-3 closes `FB`), and `G-VOCAB-TESTS` was retired by roadmap D28 and fails since before the feedback plan (that plan's §9 records it). The prerequisite goals of this plan are therefore every goal except the six retired `G-VOCAB-SCHEMA`, `G-VOCAB-CHECK`, `G-VOCAB-SAMPLE`, `G-VOCAB-TESTS`, `G-VOCAB-READONLY` and `G-VOCAB-EXPECT-INDEPENDENT`.
- Versions chosen (P0-2, 2026-11-03): Node.js `24.22.2`, the newest Node.js 24 release on <https://nodejs.org/dist/index.json>; rulesync `43.4.2` (`npm view rulesync version`), `engines.node` `>=22.22.0`.
- Other work in progress at `ds-start` (P0-2, 2026-11-03): none. The working tree was clean, and no other plan has an open range besides `R` and `RF`, which stay open by design.
- P0-3 (2026-11-03): `scripts/plan-labels.tsv` was written from the table of §4.5. Two commands of §5.1 were corrected while writing the goals. (1) `G-DS-CONCEPT-GOALS`: the awk started a body only at `goal_G_`, so a helper function after a goal (`fb_in_scope` names `docs/concept.md`) was counted as part of that goal; the awk now starts a body at every function and prints only goals. (2) `G-DS-VERSION`: the line for unchanged package versions also counted the `std` requirement of `std-version-check`, which S1044 changes on purpose; the diff now leaves out `tests/golden/bundle/std-version-check.*`, which `G-DS-GOLDEN-SAME` also leaves out. At P0-3, `G-DS-PLAN-NAMES` reports `plan-tests: 22 names, 18 missing`: the missing names are the tests that later tasks add.
- Mutation check: `G-DS-ENTRY-DOCS` — S992, renamed the pointer to `plans/x.md`: failed (`pointer line: got '0'`). Restored: pass
- Mutation check: `G-DS-LABELS` — S993, deleted the `Total` row of the TSV: failed (`label rows: got '14', want '15'`). Restored: pass
- Mutation check: `G-DS-LABELS` — S994, wrote the Japanese mutation column name inline in `check-plan-tests.sh`: failed (`Japanese labels in checkers: got '1', want '0'`). Restored: pass
- Mutation check: `sh/test-check-plan-tests.sh` — S995, read only the Japanese test column name: failed (`16 cases, 2 failed`, `english-names`). Restored: pass
- Mutation check: `sh/test-check-plan-tests.sh` — S996, read only the Japanese count form: failed (`16 cases, 2 failed`, `english-count-short`). Restored: pass
- Mutation check: `sh/test-check-rewrite-equivalence.sh` — S997, read only the Japanese rewrite record: failed (`17 cases, 1 failed`, `english-record`). Restored: pass
- Mutation check: `sh/test-check-lists-copy.sh` — S998, skipped only the Japanese total row: failed (`6 cases, 1 failed`, `english-total`). The first version of the case put no identifier in the `Total` row and missed the break, so the row now holds one. Restored: pass
- Mutation check: `G-DS-LABELS` — S999, made `G-FB-DECISIONS` read `[decided]`: failed (`feedback plan goals: exit 1`). Restored: pass
- Mutation check: `sh/test-goals.sh` — S1012, stopped `--summary` at the first failure: failed (`8 cases, 1 failed`, `summary`). Restored: pass
- Mutation check: `G-DS-LABELS` — S1013, left `FB=fb-start..HEAD`: failed (`FB range: got 'FB=fb-start..HEAD'`, `feedback plan goals: exit 1`). Restored: pass
- Mutation check: `G-DS-OWN-LABELS` — S1014, wrote the Japanese decided label in the heading of D116: failed (`Japanese labels: got '1'`). Restored: pass
- Node.js 24.22.2 (P1-1, 2026-11-03): installed under `~/.local/node-v24.22.2-linux-x64` after the user's yes, from nodejs.org with its `SHASUMS256.txt` checked. `G-PLAT-WASM`, `G-REND-HTML`, `G-REND-PHRASES-RENDER`, `G-REND-BROWSER-SAFE`, `G-REND-SELFCONTAINED`, `G-REND-VENDOR` and `G-REPO-TOOLS` passed on it. `G-REND-TESTS` failed with `renderer failures: got ''`: all 60 renderer tests pass, but since Node.js 22 `node --test DIR` no longer runs a folder, and since Node.js 23 the default reporter is not TAP. Following R2, the user was asked and chose to fix the call: `goal_G_REND_TESTS` now runs `node --test --test-reporter=tap 'renderers/test/*.test.mjs'` and passes. The roadmap's code block keeps the old command, because the translation must keep code blocks as they were (S1004); the goal function is the command that runs.
- Mutation check: `G-DS-TOOLCHAIN` — S945, dropped the `npx` column: failed (`rulesync line: got '43.4.2'`, `G-REPO-TOOLS: exit 1`). Restored: pass
- Mutation check: `G-DS-TOOLCHAIN` — S946, set node to 22.21.0: failed (`node >= 22.22: got '0'`). Restored: pass
- Mutation check: `G-DS-TOOLCHAIN` — S947, gave the `rulesync` row an empty third column: failed (`G-REPO-TOOLS: exit 1`). Restored: pass
- Output paths written by rulesync 43.4.2 for this config (P1-2, 2026-11-03): rules go to `CLAUDE.md`, `AGENTS.md` (written by both `agentsmd` and `codexcli`), `.cursor/rules/overview.mdc` and `.github/copilot-instructions.md`. `agentsmd` has no `permissions` feature and is skipped for it. A rule file that is unchanged is not listed as written. `scripts/rulesync.sh generate` reads the paths that rulesync prints and fails when one is not in `scripts/agent-outputs.txt`, or when a listed path does not exist (S956).
- Mutation check: `sh/test-rulesync.sh` — S948, dropped `--yes` from the wrapper: failed (`runs-pinned`). The first try left a second copy of the command line without `--yes` untouched and the test passed, so the wrapper now builds the command once. Restored: pass
- Mutation check: `sh/test-rulesync.sh` — S949, exited 0 when the `rulesync` line is missing: failed (`missing-line`). Restored: pass
- Mutation check: `sh/test-rulesync.sh` — S950, passed `--delete` through: failed (`refuses-delete`). Restored: pass
- Mutation check: `G-DS-RULESYNC` — S951, removed `cursor` from the targets: failed (`targets and features`, `changes after generate: got '1'`). Restored: pass
- Mutation check: `G-DS-RULESYNC` — S952, added a second rule file: failed (`rule files: got '2'`). The generated `.cursor/rules/second.mdc` was removed afterwards. Restored: pass
- Mutation check: `G-DS-RULESYNC` — S953, edited `AGENTS.md` by hand: failed (`generate --check: exit 1`). Restored: pass
- Mutation check: `G-DS-RULESYNC` — S954, changed the source heading without generating: failed (`generate --check: exit 1`, `changes after generate: got '5'`). Restored: pass
- Mutation check: `sh/test-rulesync.sh` — S955, made the wrapper drop `--check`: failed (`stale-check`). Restored: pass
- Mutation check: `sh/test-rulesync.sh` — S956, accepted an output that `scripts/agent-outputs.txt` does not list: failed (`extra-output`). Restored: pass
- Mutation check: `G-DS-RULESYNC` — S956, listed the untracked path `.codex/none.md`: failed (`untracked outputs: got '1'`). Restored: pass
- Mutation check: `G-DS-AGENT-FILES` — S957, dropped the noreply address from the source and generated: failed (`noreply in CLAUDE.md: got '0'`). Restored: pass for that check
- Mutation check: `G-DS-AGENT-FILES` — S958, split this plan's line into two lines and generated: failed (`plan line ozen-dev-setup.md.*G-DS-.*ds-start in CLAUDE.md: got '0'`). Restored: pass for that check
- Mutation check: `G-DS-AGENT-FILES` — S959, set `alwaysApply: false`: failed (`cursor alwaysApply: got '0'`). Restored: pass for that check
- Permission outputs (P1-3, 2026-11-03): the `permissions` feature writes `.claude/settings.json`, `.codex/config.toml`, `.codex/rules/rulesync.rules`, `.cursor/cli.json` and `.vscode/settings.json` (Copilot's terminal auto-approve list). Codex CLI `prefix_rule` patterns are literal words, so rulesync drops the four entries with an inner `*` (`scripts/check-*.sh *` and `build/*/ozen ... *`) for Codex and warns; the other targets keep all 14 entries. `scripts/agent-allow.txt` holds the 14 entries in the Claude Code form that `.claude/settings.json` uses.
- Mutation check: `G-DS-AGENT-FILES` — S960, added a line to `scripts/agent-allow.txt` without generating: failed (`allow list`). Restored: pass
- Mutation check: `G-DS-AGENT-FILES` — S961, added `Bash(git push *)` to the allow list: failed (`writing commands in the allow list: got '1'`). Restored: pass
- Mutation check: `G-DS-AGENT-FILES` — S962, dropped the English sentence: failed (`English rule: got '0'`). Restored: pass
- Mutation check: `G-DS-AGENT-FILES` — S963, dropped `phrases/ja.ftl` from the exceptions: failed (`rule names phrases/ja.ftl: got '0'`). Restored: pass
- Mutation check: `G-DS-AGENT-FILES` — S964, dropped `ozen-dev-environment.md`: failed (`rule names ozen-dev-environment.md: got '0'`). Restored: pass
- Mutation check: `G-DS-AGENT-FILES` — S965, dropped the sentence about own settings: failed (`rule names own settings: got '0'`). Restored: pass
- Mutation check: `G-DS-AGENT-FILES` — S966, kept the old sentence about `plans/`: failed (`Japanese folders in CLAUDE.md: got '1'`). Restored: pass
- Mutation check: `G-DS-AGENT-FILES` — S967, kept "ask the user before pushing": failed (`ask before pushing in CLAUDE.md: got '1'`). Restored: pass
- Mutation check: `G-DS-AGENT-FILES` — S968, dropped the word Japanese from the rule and generated: failed (`CLAUDE.md goals: exit 1`, from `G-REPO-CLAUDEMD` `Japanese rule: got '0'`). Restored: pass
- Mutation check: `sh/test-goals.sh` — S972, removed the empty-address branch: failed (`noreply-no-global`, `noreply-actions-no-local`). Restored: pass
- Mutation check: `sh/test-goals.sh` — S973, compared with a wrong noreply address: failed (`noreply-global-is-noreply`). Restored: pass
- Mutation check: `sh/test-goals.sh` — S974, skipped whenever the address is set: failed (`noreply-personal-found`). Restored: pass
- Mutation check: `sh/test-goals.sh` — S975, skipped check 2 whenever the local address is missing: failed (`noreply-local-missing`). Restored: pass
- Mutation check: `sh/test-pre-push.sh` — S969, removed the execute bit of the hook: failed (`other-address-push`; git ignores a hook that is not executable). Restored: pass
- Mutation check: `sh/test-pre-push.sh` — S970, made the hook `exit 0`: failed (`other-address-push`). Restored: pass
- Mutation check: `sh/test-pre-push.sh` — S971, made the hook `exit 1`: failed (`noreply-push`). Restored: pass
- Mutation check: `sh/test-setup.sh` — S978, printed `ok` for a different version: failed (`cmake-differs`). Restored: pass
- Mutation check: `sh/test-setup.sh` — S979, probed the `xcode` row on Linux: failed (`linux-skips`, `missing xcode want 26.6`). Restored: pass
- Mutation check: `sh/test-setup.sh` — S980, dropped the `core.hooksPath` line: failed (`hooks-set`). Restored: pass
- Mutation check: `sh/test-setup.sh` — S981, overwrote an existing `core.hooksPath`: failed (`hooks-kept`). Restored: pass
- Mutation check: `sh/test-setup.sh` — S982, skipped the generate step: failed (`generated`, `rulesync-fails`). Restored: pass
- Mutation check: `sh/test-setup.sh` — S983, exited 1 when rulesync fails: failed (`rulesync-fails`). Restored: pass
- Mutation check: `sh/test-setup.sh` — S984, appended a timestamp line to a file: failed (`second-run`). Restored: pass
- Mutation check: `sh/test-setup.sh` — S985, exited 0 when `cmake` differs: failed (`cmake-differs`). Restored: pass
- Mutation check: `sh/test-setup.sh` — S985, required `gcc` on a fake macOS: failed (`macos`). Restored: pass
- Mutation check: `sh/test-setup.sh` — S986, exited 1 when `emcc` is missing: failed (`emcc-missing-ok`, `rulesync-fails`). Restored: pass
- Mutation check: `G-DS-SETUP` — S987, added `sudo apt-get install`: failed (`install commands: got '1'`). Restored: pass
- Mutation check: `G-DS-CI-AGENTS` — S976, added `paths-ignore: [plans/**]`: failed (`path filters: got '1'`). Restored: pass
- Mutation check: `G-DS-CI-AGENTS` — S976, dropped `fetch-depth: 0`: failed (`full history: got '0'`). Restored: pass

---

## 10. Risks

- **R1. rulesync changes quickly** (major version 43): the version is pinned in `toolchain.txt`, and `G-DS-RULESYNC` and the `agents` workflow fail when generated files drift. Raise the version on purpose only.
- **R2. The Node.js raise breaks the renderer or the WebAssembly comparison**: P1-1 runs `G-REND-TESTS` and `G-PLAT-WASM` first. If they fail, record it in §9 and stop for the user's decision.
- **R3. The translation changes a goal's meaning**: each translation task compares the goal results before and after (`G-DS-GOALS-SAME`), and `G-DS-TRANSLATION` compares the structure.
- **R4. The feedback plan does not finish soon**: this plan waits (D118). The alternative in D118 is the way out if waiting is not acceptable.
- **R5. A generated agent file is edited by hand, or another session edits `CLAUDE.md` directly**: `generate --check` in the `agents` workflow and in `G-DS-RULESYNC` catches it. The agent rules themselves say that `CLAUDE.md` is generated.
- **R6. `AGENTS.md` links a tag that does not exist yet**: only builds of an unreleased version are affected. Releases are made from tags, so a released `ozen` always links an existing tag.
- **R7. Other contributors' commits fail check 1 of `G-REPO-NOREPLY`** (every commit address must be the owner's noreply address): today the repository has one author. When that changes, the check must be revisited (not in this plan).
- **R8. The red-first check is nominal for CLI tests** (§3.8): this plan still commits CLI tests before their implementation, but `G-REPO-RED-FIRST` cannot prove it. Fixing `scripts/check-red-first.sh` is a separate change.
- **R9. `npx` needs the network**: `scripts/setup.sh` reports a failure and goes on (S983). The goals that run `scripts/rulesync.sh` need the network or a warm npm cache.
