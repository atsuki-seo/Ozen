# Ozen

Ozen is a language and compiler for cooking procedures. Recipes are written as
programs with dimension types (quantities), state types (ingredient states), and
linear types (consumed resources). The compiler type-checks a recipe or a whole
meal, injects a kitchen spec and today's conditions, and produces an IR and a
resource-constrained schedule. The app built on it is called Ozendate.

- Design concept (Japanese): [docs/concept.md](docs/concept.md)
- Implementation plan (Japanese): [plans/ozen-roadmap.md](plans/ozen-roadmap.md)

## Status

Early work. Nothing is usable yet; each section below is filled in as its part
of the plan is finished.

## Vocabulary table

Not yet available.

## Evaluator prototype

Not yet available.

## Core and CLI

Not yet available.

## Standard library

Not yet available.

## Platforms

Not yet available.

## Renderer

Not yet available.

## Bundles

Not yet available.

## License

| Paths | License |
| --- | --- |
| `proto/`, `tools/`, `scripts/`, `spec/vocab/`, `spec/schema/`, `spec/coverage/`, `spec/fixtures/` | MIT OR Apache-2.0 |
| `core/`, `cli/`, `stdlib/`, `bindings/`, `renderers/`, `schemas/`, `tests/`, `fuzz/` | MIT OR Apache-2.0 |
| `docs/`, `plans/`, `spec/samples/` | CC-BY-4.0 |
| `samples/`, `phrases/` | CC-BY-4.0 |
| `data/mext-fct-2023/` | See its README (source data from MEXT, used under MEXT's terms) |
| `*/vendor/` | The license of each vendored component, kept with it |

Code is dual-licensed: you may use it under either the MIT License
([LICENSE-MIT](LICENSE-MIT)) or the Apache License, Version 2.0
([LICENSE-APACHE](LICENSE-APACHE)). Documentation, sample recipes, and phrase
files are licensed under CC BY 4.0 ([LICENSE-CC-BY-4.0](LICENSE-CC-BY-4.0)).
See [NOTICE](NOTICE) for third-party data and components.

Recipes you write in Ozen and the schedules and IR the compiler produces are yours; the Ozen licenses do not apply to them.
