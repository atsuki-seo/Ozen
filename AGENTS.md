# Notes for AI agents

`CLAUDE.md`, `AGENTS.md` and the other agent files are generated from `.rulesync/` by `scripts/rulesync.sh generate`. Edit `.rulesync/rules/overview.md` or `.rulesync/permissions.jsonc`, then generate; never edit the generated files.

## Language

- Files committed to this repository are written in English: source code, comments, developer-facing strings, commit messages, README files, documents under `docs/`, plans under `plans/`, YAML comments, scripts and build files.
- Exceptions:
  - The introduction page under `site/` is written in Japanese for its readers; its CSS and script comments stay in English.
  - The MEXT attribution wording is written in Japanese as MEXT requires.
  - Values in `phrases/ja.ftl` are Japanese; values in `phrases/en.ftl` are English.
  - Three plans that were written before this rule and are not carried out yet stay Japanese: `plans/ozen-cooking-feedback.md`, `plans/ozen-authoring-support.md` and `plans/ozen-dev-environment.md`. New text added to them is written in English.
- The language of chat and of work that is not committed (notes, drafts, plan reviews in progress) follows each person's own settings.

## Plans

Each plan names its goals in its §5; the commands in the §5 code blocks are authoritative, and `scripts/goals.sh` runs them. Work happens on `main`. Each plan's history range starts at its tag.

- `plans/ozen-roadmap.md`: the implementation plan; its goals are the `G-*` goals of its §5, and its range starts at the tag `plan-start`.
- `plans/ozen-syntax-refresh.md`: the syntax refresh, the test rewrite and the prototype removal; goals `G-RF-*`, from the tag `refresh-start`. The roadmap goals it lists in its §5 remain prerequisites.
- `plans/ozen-readme-compact.md`: the compact README and the per-folder documents; goals `G-RD-*`, from the tag `readme-start`.
- `plans/ozen-quantity-checks.md`: checking quantity arguments and non-positive amounts; goals `G-QC-*`, from the tag `qc-start`.
- `plans/ozen-cooking-feedback.md`: the cooking feedback (cracking eggs, `until:` and subject-first calls); goals `G-FB-*`, from the tag `fb-start`.
- `plans/ozen-dev-setup.md`: agent rules, the development setup, English documents, the reference and releases; goals `G-DS-*`, from the tag `ds-start`.

## Publishing

- The repository is public (`atsuki-seo/Ozen`). Never publish a personal email address: commits must use the GitHub noreply address `146819854+atsuki-seo@users.noreply.github.com` (set as this repository's `user.email`), and tracked files must not contain a personal address.
- Run `scripts/goals.sh G-REPO-NOREPLY` before every push. The pre-push hook in `.githooks/` runs it too; `scripts/setup.sh` turns the hook on.
