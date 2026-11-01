# The standard library `std`

`stdlib/` is the package `std` written in Ozen and embedded in the compiler: dimensions
and units ([`stdlib/units/`](units/)), state axes, traits, ingredients with parts,
actions with their requirements and effects, equipment kinds, vessels, dirt and washing
rules, and the `Kitchen` and `Options` types.

- `ozen debug definitions` lists it.
- [`stdlib/sources.ozen`](sources.ozen) records where each value comes from.
- [`phrases/ja.ftl`](../phrases/ja.ftl) and [`phrases/en.ftl`](../phrases/en.ftl) hold
  its names and sentences for the [renderer](../renderers/README.md).

## Provisional values

Many vocabulary values are provisional ranges, and the compiler reports them as
`provisionalValue` (an info diagnostic). The search for a primary source of each value
is recorded with the vocabulary table (see [spec/README.md](../spec/README.md)).
