# The `ozen` command

`cli/` is the `ozen` command. It reads the files, calls the compiler in
[`core/`](../core/README.md), and prints or writes the result. `ozen render` passes the result to
the [renderer](../renderers/README.md) through node.

## Commands

```
ozen init [DIR] [--name NAME]           # a new cookbook: package.ozen and AGENTS.md
ozen check [OPTIONS] [ENTRY|FILE]       # diagnostics (--format text|json|sarif)
ozen build [OPTIONS] [ENTRY|FILE...]    # every entry into out/
ozen render [OPTIONS] [ENTRY|FILE]      # text or HTML through the renderer
ozen bundle [ENTRY|DIR] [-o FILE]       # one shareable file
ozen unbundle FILE [-o DIR]             # back to module files
ozen debug ast|modules|definitions|steps|requires|ir|schedule|entry|options
```

`ozen init` starts a cookbook in `DIR` (created if missing) or in the current folder. The
package name is the folder's name, or `NAME`. It writes `package.ozen` and an `AGENTS.md`
that points AI agents to the [guide](../reference/ai-guide.md) of the installed version, and
it never overwrites either file.

## Check levels

A kitchen spec (`-k`) and today's conditions (`--options`, or `-s`, `-c`, `-t`) deepen
the check from a recipe alone (level 0) to equipment (level 1) and a schedule (level 2).
For example, with these two files:

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

menu lunch(people: 2) {
  let soupPot = make soup
  serve main {
    plate perPerson on SoupBowl { soupPot }
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

`ozen debug schedule --entry lunch -k kitchen.ozen soup.ozen` prints the schedule:

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
info ASM007 provisionalValue soup.ozen:7:18-7:51 args=["cut"]: provisional value
info ASM007 provisionalValue soup.ozen:7:18-7:31 args=["peel"]: provisional value
info ASM007 provisionalValue soup.ozen:15:5-15:44 args=["plate"]: provisional value
result ok errors=0 warnings=0 infos=3
```

Times are in seconds. Each step line shows start..end, the latest start in
parentheses, the step, the cook (or `unattended`), and the equipment.

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
