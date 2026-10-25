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

The vocabulary table (`spec/vocab/*.yaml`) holds the standard vocabulary of Ozen before
the core exists: dimensions, units, state axes, traits, ingredients with parts, aliases,
actions with their requirements and effects, capabilities and equipment kinds, dirt and
washing rules, vessels, and the provenance of every value. It is checked by
`tools/vocabcheck/` (Python 3 with PyYAML and jsonschema):

```
python3 tools/vocabcheck/vocabcheck.py schema       # shape of the 11 files (JSON Schema)
python3 tools/vocabcheck/vocabcheck.py check        # references, names, units, rules
python3 tools/vocabcheck/vocabcheck.py lists spec/coverage/concept-lists.yaml spec/coverage/concept-relations.yaml
python3 tools/vocabcheck/vocabcheck.py sample spec/samples/curryRice.yaml
python3 tools/vocabcheck/sources.py spec/vocab spec/sources/search-log.yaml
```

`sample` type-checks the curry-and-rice sample on the table (states, linear use, safety,
vessels) and compares the result with the expectations in the sample. Refuse rates come
from the Standard Tables of Food Composition in Japan (`data/mext-fct-2023/`,
re-extracted with `tools/fct/extract.py --check --xlsx <file>`). Values without a primary
source are provisional ranges, and `spec/sources/search-log.yaml` records the search for
each of them. `spec/notes/prior-work.md` maps the models of four papers to Ozen.

## Evaluator prototype

`proto/` is a prototype of the compile-time evaluator in C23 (D13): a subset of the
syntax, rational quantities with dimensions, `let`, `if`, `for`, `repeat`, hygienic
macros with `quote` and `$`, fuel and size limits, and diagnostics with expansion
trails. It has no type checker or scheduler; it prints the steps that a recipe expands
to. Build the four configurations (gcc and clang, Debug with ASan and UBSan, and Release)
and run the tests with:

```
proto/scripts/build-all.sh
proto/scripts/run-golden.sh build/gcc-release/ozenproto
ctest --test-dir build/gcc-debug-san
```

`build/<config>/ozenproto [--entry NAME] [--param name=expr] [--stats] FILE` expands a
file. For example, this recipe:

```ozen
action peel(x)
action cut(x, size)
macro prep(v) { quote { cut(peel($v), 3 mm) } }
recipe salad() {
  ingredients {
    onion: Onion 1 pcs
    carrot: Carrot 1 pcs
  }
  prep(onion)
  repeat 2 {
    cut(carrot / 2, 1 cm)
  }
  cut(carrot, 1 cm + 30 s)
}
```

prints:

```text
%1 = peel(onion)
%2 = cut(%1, 3 mm)
%3 = cut(carrot*1/2, 10 mm)
%4 = cut(carrot*1/2, 10 mm)
error TYP001 dimension 13:15-13:26 args=["Length", "Time"]
result error steps=4 diagnostics=1
```

`proto/scripts/build-wasm.sh` builds the WebAssembly versions with Emscripten
(`node build/wasm/ozenproto.js FILE` and a module for browsers), and
`proto/scripts/fuzz.sh SECONDS` runs libFuzzer. The results of the prototype are in
[docs/prototype-report.md](docs/prototype-report.md).

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
