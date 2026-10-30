# Notes for Claude Code

## Language of committed files

- Design documents under `docs/` are written in Japanese. This includes `docs/concept.md` (kept as is, not translated) and the prototype report `docs/prototype-report.md`.
- Plans under `plans/` are written in Japanese, because the user reviews them.
- Everything else committed to this repository is written in English: source code, comments, developer-facing strings, commit messages, README, YAML comments, scripts, and build files.
- Exceptions: the MEXT attribution wording is written in Japanese as MEXT requires; values in `phrases/ja.ftl` are Japanese and values in `phrases/en.ftl` are English.

## Plan

- The implementation plan is `plans/ozen-roadmap.md`. Task completion conditions are the goals defined in its §5, and the judgment commands in the §5 code blocks are authoritative.
- Work happens on `main` only; ranges for history checks start at the tag `plan-start`.
- The follow-up plan for the syntax refresh, the test rewrite and the prototype removal is `plans/ozen-syntax-refresh.md`. Its goals are the `G-RF-*` goals in its §5, its history range starts at the tag `refresh-start`, and the roadmap goals it lists in §5 remain prerequisites.

## Publishing

- The repository is public (`atsuki-seo/Ozen`). Never publish the user's personal email address: commits must use the GitHub noreply address `146819854+atsuki-seo@users.noreply.github.com` (set as this repository's `user.email`), and tracked files must not contain the personal address.
- Run the plan's `G-REPO-NOREPLY` goal before every push, and ask the user before pushing.
