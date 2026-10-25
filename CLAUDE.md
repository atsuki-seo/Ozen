# Notes for Claude Code

## Language of committed files

- Design documents under `docs/` are written in Japanese. This includes `docs/concept.md` (kept as is, not translated) and the prototype report `docs/prototype-report.md`.
- Plans under `plans/` are written in Japanese, because the user reviews them.
- Everything else committed to this repository is written in English: source code, comments, developer-facing strings, commit messages, README, YAML comments, scripts, and build files.
- Exceptions: the MEXT attribution wording is written in Japanese as MEXT requires; values in `phrases/ja.ftl` are Japanese and values in `phrases/en.ftl` are English.

## Plan

- The implementation plan is `plans/ozen-roadmap.md`. Task completion conditions are the goals defined in its §5, and the judgment commands in the §5 code blocks are authoritative.
- Work happens on `main` only; ranges for history checks start at the tag `plan-start`.
