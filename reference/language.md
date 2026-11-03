# The Ozen language

Ozen describes a cooking procedure as a program. A recipe lists its ingredients and the
steps that turn them into a dish. The compiler checks the amounts, the state each step
needs, and that every ingredient is used once, and it can render the recipe as text or
plan a schedule for a kitchen.

Every example on this page is run by `scripts/check-readme-example.sh`, so the output shown
is the output of the current `ozen`.

<!-- ai-guide:begin -->
## Quick syntax

<!-- file: salad.ozen -->
```ozen
// One recipe: ingredients first, then one step per line.
recipe salad(servings: 2) {
  ingredients {
    cabbage: 0.25 pcs per serving
    carrot: 0.5 pcs per serving
    dressing: Dressing 1 tbsp per serving
  }
  let cabbageCut = cabbage |> wash |> cut(julienne, 2 mm)
  let carrotCut = carrot |> peel |> cut(julienne, 2 mm)
  let mixed = toss([cabbageCut, carrotCut])
  let dressed = season(mixed, with: dressing)
}
```

<!-- run: ozen render --kind recipe --lang en salad.ozen -->
```text
# salad

## Ingredients
- cabbage, 0.5
- carrot, 1
- dressing, 30 ml

## Steps
1. Wash cabbage, then cut into julienne (2 mm).
2. Peel carrot, then cut into julienne (2 mm).
3. Toss cabbage, carrot.
4. Season cabbage, carrot with dressing.
```

- A file holds declarations: `recipe`, `menu`, `kitchen`, `import`, and the declarations of
  a library (`action`, `ingredient`, `unit` and others).
- `recipe NAME(servings: N) { ... }` declares a recipe. Parameters are written `name: value`
  or `name: Type`.
- `ingredients { name: [Type] AMOUNT [per serving] }` lists what the recipe uses. Without a
  type, the name is looked up as an ingredient of `std` (`carrot` is `Carrot`).
- `let name = expression` names the result of a step. Every value must be used exactly once;
  an unused value is a warning.
- A call is `action(x, positional, named: value)`. `x |> action(args)` is the same as
  `action(x, args)`, so steps chain from left to right. A call without arguments can drop
  `()`: `x |> peel`.
- `[a, b]` passes several ingredients to one step; the step makes a mixture of them.
- Amounts have units: `200 ml`, `0.5 pcs`, `1 tbsp`, `3 mm`, `15 min`, `170 degC`. A range is
  written `1..3 min`.
- A type can name the state it needs: `T<shape: peeled>`. Calling a step on an ingredient in
  another state is an error, for example `beat` before `crack`.
- `menu NAME(people: N) { ... }` combines recipes with `make RECIPE(args)` and plates them
  with `serve` and `plate`.
- `kitchen NAME { places {...} tools {...} tableware {...} }` describes the equipment, and
  `ozen debug schedule --entry MENU -k KITCHEN-FILE FILE` plans the work.
- Comments are `// to the end of the line` and `/* ... */`.
<!-- ai-guide:end -->

## Files, packages and imports

A file is a module named after its path. A folder with a `package.ozen` is a package:

```
package kitchenBook {
  version: "0.1.0"
}
```

`import soup.{soup}` makes the recipe `soup` of the module `soup` visible; `import jp` with
`jp.cup` uses a unit from the module `jp`. The standard library `std` is always visible.

## Recipes and ingredients

A recipe has parameters, an ingredient list and steps. `servings` scales every amount
written `per serving`:

<!-- file: tea.ozen -->
```ozen
recipe greenTea(servings: 2) {
  ingredients {
    water: 150 ml per serving
  }
  let hot = boil(water, 3 min)
}
```

<!-- run: ozen render --kind recipe --lang en tea.ozen -->
```text
# green tea

## Ingredients
- water, 300 ml

## Steps
1. Boil water for 3 min.
```

An ingredient can name its type before the amount (`salt: Salt 1 pinch`); this is needed
when the name is not an ingredient of `std`. A recipe can also take an ingredient that a
menu passes to it, such as `recipe kamaYaki(kama: Buri<collar>, servings: 4)`.

## Steps and states

Each step is a call of an action. [std.md](std.md) lists the actions of `std`, with the
state each input must be in and the state of the result. Ozen tracks those states, so a
step that needs another state is an error:

<!-- file: egg.ozen -->
```ozen
recipe scrambled(servings: 1) {
  ingredients {
    egg: Egg 2 pcs per serving
  }
  let beaten = egg |> beat
  let cooked = fry(beaten, 2 min, heat: low)
}
```

<!-- run: ozen check egg.ozen -->
```text
error TYP006 stateRequired egg.ozen:5:16-5:19 args=["beat", "x", "egg"]: state required
result error errors=1 warnings=0 infos=0
```

`beat` needs `shape: cracked`, and an egg starts `inShell`. Cracking it first fixes the
error:

<!-- file: egg.ozen -->
```ozen
recipe scrambled(servings: 1) {
  ingredients {
    egg: Egg 2 pcs per serving
  }
  let beaten = egg |> crack |> beat
  let cooked = fry(beaten, 2 min, heat: low)
}
```

<!-- run: ozen render --kind recipe --lang en egg.ozen -->
```text
# scrambled

## Ingredients
- egg, 2

## Steps
1. Crack egg into a bowl, then beat, then fry over low heat for 2 min.
```

Every value is used exactly once. A value that no step uses is reported as `unusedValue`;
using a value twice is an error, because the first step already consumed it.

## Menus and serving

A menu makes recipes and plates the results. `serve` names a course, and each `plate`
says how the dish is shared and on which tableware:

<!-- file: lunch.ozen -->
```ozen
recipe soup(servings: 2) {
  ingredients {
    water: 200 ml per serving
    onion: 0.5 pcs per serving
  }
  let onionCut = onion |> peel |> cut(slice, 3 mm)
  let soup = simmer([water, onionCut], 15 min)
}

menu lunch(people: 2) {
  let soupPot = make soup
  serve main {
    plate perPerson on SoupBowl { soupPot }
  }
}
```

<!-- run: ozen check --entry lunch lunch.ozen -->
```text
info ASM004 unlimitedEquipment lunch.ozen:10:6-10:11: unlimited equipment
result ok errors=0 warnings=0 infos=1
```

The info `unlimitedEquipment` says that no kitchen was given, so the check assumed enough
equipment.

## Kitchens and schedules

A kitchen lists the places, tools and tableware. With a kitchen, `ozen` checks that the
equipment is enough and plans the steps for each cook; [cli/README.md](../cli/README.md)
shows a full schedule.

```
kitchen home {
  places {
    stove: IHStove { burners: 2 }
  }
  tools {
    board: CuttingBoard
    knife: Knife
    pot: Pot
  }
  tableware {
    bowl: SoupBowl { count: 2 }
  }
}
```

## Checking

`ozen check FILE` prints one line per diagnostic and a result line, and exits 1 when there
is an error. `ozen check --format json FILE` prints the same as JSON, which is easier for
tools and agents to read. [diagnostics.md](diagnostics.md) lists every diagnostic.
