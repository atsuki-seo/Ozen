<h1 align="center"><img src="site/favicon.svg" width="48" height="48" alt=""><br>Ozen</h1>

Ozen is a language and compiler for cooking procedures. Recipes are written as
programs with dimension types (quantities), state types (ingredient states), and
linear types (consumed resources). The compiler type-checks a recipe or a whole meal,
injects a kitchen spec and today's conditions, and produces an IR and a
resource-constrained schedule.

<!-- file: soup.ozen -->
```ozen
recipe soup(servings: 2) {
  ingredients {
    water: 200 ml per serving
    onion: 0.5 pcs per serving
    carrot: 0.5 pcs per serving
  }
  let onionCut = onion |> peel |> cut(slice, 3 mm)
  let carrotCut = carrot |> peel |> cut(rollCut, 20 mm)
  let soup = simmer([water, onionCut, carrotCut], 15 min)
}
```

`ozen render --kind recipe --lang en soup.ozen` turns it into a recipe text:

<!-- run: ozen render --kind recipe --lang en soup.ozen -->
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

## Build

```bash
scripts/build-core.sh --only core-gcc-release
```

This builds `build/core-gcc-release/ozen`. `ozen render` also needs `node`. The
versions of the tools are listed in [toolchain.txt](toolchain.txt).

## Documentation

- [cli/README.md](cli/README.md): the `ozen` command, check levels, schedules, and bundles
- [core/README.md](core/README.md): the compiler core and its public API
- [stdlib/README.md](stdlib/README.md): the standard library `std`
- [renderers/README.md](renderers/README.md): recipe texts, schedules, HTML, and phrases
- [bindings/README.md](bindings/README.md): Linux, WebAssembly, iOS, and Android
- [spec/README.md](spec/README.md): the vocabulary table and its sources
- [site/README.md](site/README.md): the introduction page
- [CONTRIBUTING.md](CONTRIBUTING.md): builds, tests, goals, and CI
- [Introduction page (Japanese)](https://atsuki-seo.github.io/Ozen/)

## License

| Paths | License |
| --- | --- |
| `tools/`, `scripts/`, `spec/vocab/`, `spec/schema/`, `spec/coverage/`, `spec/fixtures/` | MIT OR Apache-2.0 |
| `core/`, `cli/`, `stdlib/`, `bindings/`, `renderers/`, `schemas/`, `tests/`, `fuzz/` | MIT OR Apache-2.0 |
| `docs/`, `plans/`, `spec/samples/` | CC-BY-4.0 |
| `samples/`, `phrases/`, `site/` | CC-BY-4.0 |
| `data/mext-fct-2023/` | See its README (source data from MEXT, used under MEXT's terms) |
| `*/vendor/` | The license of each vendored component, kept with it |

Code may be used under either the MIT License ([LICENSE-MIT](LICENSE-MIT)) or the
Apache License, Version 2.0 ([LICENSE-APACHE](LICENSE-APACHE)). The CC-BY-4.0 paths are
licensed under CC BY 4.0 ([LICENSE-CC-BY-4.0](LICENSE-CC-BY-4.0)). See [NOTICE](NOTICE)
for third-party data and components.

Recipes you write in Ozen and the schedules and IR the compiler produces are yours; the Ozen licenses do not apply to them.
