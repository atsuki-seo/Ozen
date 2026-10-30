# Ozen

Ozen is a language and compiler for cooking procedures. Recipes are written as
programs with dimension types (quantities), state types (ingredient states), and
linear types (consumed resources). The compiler type-checks a recipe or a whole
meal, injects a kitchen spec and today's conditions, and produces an IR and a
resource-constrained schedule. The app built on it is called Ozendate.

- Design concept (Japanese): [docs/concept.md](docs/concept.md)
- Implementation plan (Japanese): [plans/ozen-roadmap.md](plans/ozen-roadmap.md)

## Status

Version 0.1.0. The core, the CLI, the standard library, the platform bindings, the
renderer, and bundles described in the plan are implemented and tested. Many vocabulary
values are provisional ranges (the compiler reports them as `provisionalValue`), and the
app Ozendate itself is not part of this repository.

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

## Core and CLI

`core/` is the compiler in C23 with no I/O of its own: parsing, imports, compile-time
evaluation, type checking (dimensions, states, linear use of quantities and resources),
the IR, and the scheduler. `cli/` is the `ozen` command that reads files and calls it.
`scripts/build-core.sh` builds four configurations (gcc and clang, Debug with ASan and
UBSan, and Release), and `scripts/run-golden.sh build/core-gcc-release/ozen tests/golden`
runs the golden tests.

```
ozen check [OPTIONS] [ENTRY|FILE]       # diagnostics (--format text|json|sarif)
ozen build [OPTIONS] [ENTRY|FILE...]    # every entry into out/
ozen render [OPTIONS] [ENTRY|FILE]      # text or HTML through the renderer
ozen bundle [ENTRY|DIR] [-o FILE]       # one shareable file
ozen unbundle FILE [-o DIR]             # back to module files
ozen debug ast|modules|definitions|steps|requires|ir|schedule|entry|options
```

A kitchen spec (`-k`) and today's conditions (`--options`, or `-s`, `-c`, `-t`) deepen
the check from a recipe alone (level 0) to equipment (level 1) and a schedule (level 2).
For example, with these two files:

<!-- file: soup.ozen -->
```ozen
recipe soup {
  ingredients {
    water: 400 ml
    onion: 1 pcs
    carrot: 1 pcs
  }
  let onionCut = cut(peel(onion), slice, 3 mm)
  let carrotCut = cut(peel(carrot), rollCut, 20 mm)
  let soup = simmer([water, onionCut, carrotCut], 15 min)
}

menu lunch(people: 2) {
  let s = make soup
  serve main {
    plate perPerson on SoupBowl { s }
  }
}
```

<!-- file: kitchen.ozen -->
```ozen
kitchen home {
  places {
    stove: IHStove { burners: 2 }
  }
  tools {
    board: CuttingBoard
    knife: Knife
    peeler: Peeler
    pot: Pot
  }
  tableware {
    bowl: SoupBowl { count: 2 }
  }
}
```

`ozen debug schedule --entry lunch -k kitchen.ozen soup.ozen` prints the schedule in
seconds (start..end, the latest start in parentheses, the cook, and the equipment):

<!-- run: ozen debug schedule --entry lunch -k kitchen.ozen soup.ozen -->
```text
schedule lunch level=2 cooks=1
  0..120 (60) %2 peel by cook1 [counter#1]
  120..240 (180) %5 peel by cook1 [counter#1]
  240..420 (300) %3 cut by cook1 [counter#1, board#1, knife#1]
  420..600 (480) %6 cut by cook1 [counter#1, board#1, knife#1]
  600..600 (600) %8 combine unattended
  600..1500 (1500) %9 simmer unattended [stove#1, pot#1]
  1500..1560 (1518) %10 plate perPerson by cook1 [bowl#1, bowl#2]
  serve main at 1560
  total 1560
info ASM007 provisionalValue soup.ozen:7:18-7:47 args=["cut"]: provisional value
info ASM007 provisionalValue soup.ozen:7:22-7:33 args=["peel"]: provisional value
info ASM007 provisionalValue soup.ozen:15:5-15:38 args=["plate"]: provisional value
result ok errors=0 warnings=0 infos=3
```

The public API is `core/include/ozen.h` (`recipe_build` returns the result as JSON).
The JSON outputs follow the schemas in `schemas/` (draft 2020-12), and `--format sarif`
follows SARIF 2.1.0.

## Standard library

`stdlib/` is the package `std` written in Ozen and embedded in the compiler: dimensions
and units (`stdlib/units/`), state axes, traits, ingredients with parts, actions with
their requirements and effects, equipment kinds, vessels, dirt and washing rules, and the
`Kitchen` and `Options` types. `ozen debug definitions` lists it, and
`stdlib/sources.ozen` records where each value comes from. `phrases/ja.ftl` and
`phrases/en.ftl` hold its names and sentences for the renderer.

## Platforms

The same core runs on Linux (linked directly; `bindings/linux/` calls it from Swift),
WebAssembly (`scripts/build-wasm.sh`, then `bindings/wasm/ozen.mjs` in node or a Web
Worker through `bindings/wasm/worker.js`), iOS (an XCFramework with the Swift wrapper in
`bindings/swift/`), and Android (JNI in `bindings/android/`). Each runs the golden tests
and the samples and compares a digest of the results with the native build. CI
(`.github/workflows/ci.yml`) runs the Linux, fuzz, WebAssembly, iOS simulator, and
Android emulator jobs.

## Renderer

`renderers/` turns the JSON result into a recipe text, a schedule with a Gantt chart,
a shopping list, a self-contained HTML page with print styles, a book of all entries,
and a settings form generated from the type definitions. It is JavaScript with no build
step, uses Mozilla's Fluent (`renderers/vendor/fluent-bundle/`) for the phrases, and runs
in node and in browsers (`node renderers/tools/bundle.mjs` makes one file for WebViews).
`ozen render` calls it through node:

<!-- run: ozen render --kind recipe --lang en --entry lunch soup.ozen -->
```text
# lunch

## Ingredients
- water, 400 ml
- onion, 1
- carrot, 1

## Steps
1. Peel onion, then cut into slices (3 mm).
2. Peel carrot, then cut into rolling wedges (20 mm).
3. Simmer water, onion, carrot over low heat for 15 min.
4. Plate water, onion, carrot.
```

Phrases are layered as `std`, then the dependencies, then the project's own
`phrases/<lang>.ftl`; a missing phrase falls back to English and then to a name made
from the identifier.

## Bundles

`ozen bundle` puts the entry and everything it reaches, except `std`, into one file.
Each module stays an inline `module` with its full name, and the header records the
`std` version and the packages it came from. `ozen unbundle` writes the module files,
manifests, and phrases back and never overwrites a file. With this manifest next to
`soup.ozen`:

<!-- file: package.ozen -->
```ozen
package kitchenBook {
  version: "0.1.0"
}
```

<!-- file: soup.ozen -->
```ozen
recipe soup {
  ingredients {
    water: 400 ml
  }
  let soup = simmer(water, 15 min)
}

recipe tea {
  ingredients {
    water: 300 ml
  }
  let tea = boil(water, 3 min)
}
```

`ozen bundle --entry soup` leaves out `tea`, which the entry does not use:

<!-- run: ozen bundle --entry soup -->
```text
bundle {
  std: "0.1.0"
  entry: soup
  from: kitchenBook "0.1.0"
}

module kitchenBook.soup {
recipe soup {
  ingredients {
    water: 400 ml
  }
  let soup = simmer(water, 15 min)
}
}
```

## Credits

- Refuse rates: 日本食品標準成分表（八訂）増補2023年から引用 (Standard Tables of Food
  Composition in Japan 2020, 2023 Supplement, MEXT). See
  [data/mext-fct-2023/README.md](data/mext-fct-2023/README.md).
- [Fluent](https://projectfluent.org/): Mozilla's `@fluent/bundle` 0.19.1 (Apache-2.0),
  vendored in `renderers/vendor/fluent-bundle/`.
- SARIF 2.1.0 JSON Schema (OASIS), vendored in `schemas/vendor/` with its license.

## License

| Paths | License |
| --- | --- |
| `tools/`, `scripts/`, `spec/vocab/`, `spec/schema/`, `spec/coverage/`, `spec/fixtures/` | MIT OR Apache-2.0 |
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
