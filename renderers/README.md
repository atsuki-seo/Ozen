# The Ozen renderer

`renderers/` turns the JSON result into a recipe text, a schedule with a Gantt chart,
a shopping list, a self-contained HTML page with print styles, a book of all entries,
and a settings form generated from the type definitions.

It is JavaScript with no build step, uses Mozilla's Fluent
([`renderers/vendor/fluent-bundle/`](vendor/fluent-bundle/)) for the phrases, and runs
in node and in browsers. `node renderers/tools/bundle.mjs` makes one file for WebViews.

## Example

`ozen render` calls the renderer through node. With this recipe:

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

## Phrases

Phrases are layered as `std`, then the dependencies, then the project's own
`phrases/<lang>.ftl`; a missing phrase falls back to English and then to a name made
from the identifier. The phrases of `std` are in [`phrases/`](../phrases/).
