# Ozen (cooking language project): concept and direction

> This document collects what the design discussions decided and what is still open.
> It is the starting point for detailed design and prototyping in Claude Code.
> The language is named **Ozen** (from the Japanese word "ozendate", getting everything ready), the app is **Ozendate**, and the file extension is **`.ozen`** (§4.6). The text also calls the language "this language".

---

## 1. In one line

**A language and a processor that describe cooking steps as types, concurrency and resource constraints, and compile them into "step data with a schedule (IR)".**
The core of the processor is written once in a C-family language and runs on Web / iOS / Android / Linux (/ macOS).

## 2. Purpose and users

- **Main users**: people who love cooking and have some programming background.
- **Development**: open source. Basically one person (the author) drives it.
- **Goals**
  - Write recipes as "programs that can be checked", not as "prose".
  - Build the schedule of concurrent work automatically, based on servings and resources (number of burners, number of hands, tools).
  - Let users extend units, ingredients, actions and steps inside the language (metaprogramming).
  - In the end, output a recipe page (HTML, etc.).
- **External integration (later)**: output machine-readable IR or control code for robot control.

## 3. Settled policies (decisions)

| Item | Decision |
|---|---|
| Name | The language and compiler are **Ozen**; the app is **Ozendate** |
| Extension | Only **`.ozen`**. File kinds are not told apart by extension (the kind is decided from the content, and the location is shown by folder conventions). Everything can be written in one file (§4.6) |
| Syntax | **English-based** (keywords are ASCII). Ingredient names and comments pass through as UTF-8 |
| Users | People who love cooking + have programming background |
| Types | **Dimension types** for quantities, **state types** for ingredients, **linear types** for consumed resources. Kitchen specs and build-time conditions are also type-checked |
| Concurrency | The writer only declares "logically independent". Whether things really run in parallel is **decided by the scheduler from resource constraints** |
| Extensibility | Units, ingredients, actions, resources and steps can be defined and extended inside the language. **Metaprogramming is planned up to level 3** (see below) |
| Compilation | No machine code. **Build an unambiguous, type-checked DAG (IR) and output it per target** |
| Implementation | The core is written once in a **C-family language** (see §9) |
| Platforms | **Web (WASM), iOS, Android, Linux** (macOS is supported in the same line) |
| How it runs | On each platform, "**the processor that compiles recipes** runs". No C compiler on the device |
| Network | **Fully offline**. Extensions and libraries are shared as source files |
| Bridges | Minimal. The public API is limited to a few "text in → result out" calls |
| Robots | Later, as external integration (only leave room to add attributes to the IR) |
| License | Code is dual MIT and Apache-2.0; docs and samples are CC BY 4.0 (below) |

### License

| Target | License | Reason |
|---|---|---|
| Core, CLI, platform bridges, renderers | Dual license, MIT and Apache-2.0 | Does not block embedding in apps. Apache-2.0 also states how patents are handled |
| Standard library (source in this language) | Same as above | So that a writer's package that includes the standard library gets no constraints from it |
| Docs, sample recipes, phrase files | CC BY 4.0 | Easy to reuse as text and as works |
| External data (values from the food composition table, etc.) | Follow the source's terms | Keep it in a separate folder and state the source and terms. MEXT allows free use of the "Standard Tables of Food Composition in Japan"; for secondary use in apps and the like, write "日本食品標準成分表（八訂）増補2023年から引用（又は出典）" (in the NOTICE and in the app's "About this app") |

- **Compiler output belongs to the user**: state clearly that Ozen's license does not extend to recipes written in Ozen, or to the schedule tables and IR that a build produces.
- **No copyleft (GPL, etc.)**: it is said to fit badly with embedding in iOS and Android apps and distributing them.

### Note: do not confuse the two kinds of "compilation"

| | Who | What | Where |
|---|---|---|---|
| (1) Building the core | Developers / CI | Turns the processor source (C-family language) into machine code | Dev machine, CI |
| (2) Compiling a recipe | Users | Type-checks the recipe and outputs the schedule and IR | Device (the app just calls core functions) |

iOS and others do not allow JIT or dynamic code generation on the device, but this language **outputs only data (IR), not machine code**, so it does not hit that restriction.

## 4. Language design

### 4.1 Type system

1. **Dimension types (quantities)**
   - Mass, volume, length, time, temperature and so on are types. `200 g + 2 tbsp` is a unit error.
   - `g ↔ ml` conversion is possible only when the ingredient's density is declared.
   - An amount that cannot even be given a range, such as "as needed", is written `[any amount]`. It is a warning (`vagueAmount`) in normal mode and an error in strict mode. "A pinch" and "a dash" are defined as units with a range, so they are fine even in strict mode (§4.4).
   - **Strict mode = turn the diagnostic group `strict` into errors**: `ozen build --strict` is the same as `--deny strict`. In code, `@deny(strict)` can be put on a package or a recipe. Right now `strict` contains only `vagueAmount`; the group's contents are defined in the standard library, and writers can add to it. Assumptions (`ASM`) are about the environment, so they stay info even in strict mode (§6.1).
   - **Counts share one dimension (count)**: counter words (the Japanese words for pieces, sticks, sheets, heads and so on) are a display matter, so phrase files add them (§7.1). Each ingredient holds a typical weight per piece as an attribute ("1 head of cabbage ≈ 1 kg"), so counts convert to weight. Fractions such as ¼ head are kept as rationals as they are.
2. **State types (ingredient state)**
   - An action's signature declares the input state and the output state. An action declares only "which axes it changes and how", and does not touch the other axes (e.g. `fry` advances doneness and does not change shape).
   - Example: `peel: Onion<shape: whole> -> Onion<shape: peeled>` / `slice: Onion<shape: peeled> -> Onion<shape: cut(slice, 3 mm)>`
   - Calling `slice` after forgetting `peel` is a type error.
   - **State is a combination of independent axes** (not one single chain of transitions). With one chain, the number of states grows with every combination, and reverse-order cooking such as "boil, then peel (potatoes, eggs)" or "grill, then cut (steak)" cannot be written.
   - **Three kinds of axis types**:

     | Axis type | Example | Behavior |
     |---|---|---|
     | Ordered enum | Inner doneness: raw < surface only < parboiled / half-cooked < cooked through<br>Surface browning: none < light < golden < dark < charred | Moves in one direction only. Requirements can be written with a comparison (`>=`) |
     | Enum with arguments | Shape: whole / peeled / cut (cut style: slice, dice, julienne..., size) | The cut can only move toward finer (dice → mince is allowed, the reverse is not) |
     | Set of done items | Prep: washed, soaked (30 min), salted, marinated (sauce) | Only grows. Each item can have arguments |

   - **Four standard axes**: inner doneness (`doneness`), surface browning (`browning`), shape (`shape`) and prep (`prep`). Each ingredient can leave out axes and stages it does not use. **Writers can add axes the same way.**

     ```
     axis doneness : ordered { raw, surface, par, done }
     axis browning : ordered { none, light, golden, dark, charred }
     axis shape    : { whole, peeled, cut(style: CutStyle, size: Length) }
     axis prep     : set { washed, soaked(time: Time), salted, marinated(with: Sauce) }
     ```

   - **Heating and browning are independent**: cooking that browns only the surface and cooks the inside at low heat (roast beef, etc.) is easy to write.
   - **Food safety check**: raw pork, chicken and the like (`Hazard`) are an error unless `doneness >= done` before serving (§6.1). Ingredients meant to be eaten raw, such as sashimi, are exempted in that ingredient's definition.
   - **Mixing makes a new value**: once onion, meat and roux are simmered, they become a new value, "curry", and from then on the states of the individual ingredients are not tracked (only the state as curry is kept). Linear-type amounts are kept as a total.
     - However, for the undercooking check, a mixture remembers doneness per ingredient. When a heating action raises the mixture's doneness, each ingredient becomes the larger of "the ingredient's current value" and "the mixture's new value", and serving checks each ingredient (so in an oyakodon topping, made by combining chicken cooked earlier with half-cooked egg, the chicken is not left undercooked).
   - **Eggs start in the shell**: the initial shape of std `Egg` is `inShell` (in the shell). Actions that need the contents (`mix`, `fry`, etc.) require `T<shape != inShell>`, so mixing or frying without writing `egg |> crack` (crack) or `beat` (beat) is a type error. The shell is not a value; it is kept as a discarded part (waste ratio). Peeling a boiled egg is `peel` (`shape: inShell, doneness >= done`). A half-cooked (`par`) egg is not an undercooking error; it gives the warning `SAF005 lightlyCookedEgg` (`props { safeDoneness: par }`).
   - **"Not" (`!=`)**: a state requirement can be written `T<shape != inShell>`. An ingredient without the axis (water) counts as satisfying it, and on a set-of-done-items axis it means "does not contain that item".
   - **Lists are matched per element**: when a list is passed as an action's ingredient argument (`mix([egg, milk])`), the requirement is matched per element (each element must satisfy one of the alternatives, traits and axes together), not against the mixture.
   - Per-ingredient constraints (an onion cannot be cut until it is peeled; a carrot can be cut with its skin on) are expressed by action requirements and traits (`SkinEdible`, etc.).
3. **Linear types (consumed resources)**
   - Once an ingredient is consumed, it cannot be reused. The total amount is preserved.
   - Consuming the same name twice, or not having enough, is an error.
   - `let` binds only once (SSA). This fits well with linear types.

**Ingredient representation: what becomes a type and what becomes an attribute**

Making everything a type makes writing painful, so only "things that should be errors when wrong" become types, and "things used in calculations as estimates or amounts" are kept as attributes (data). An ingredient is represented in the following 5 layers.

| Layer | Example | Representation | Use |
|---|---|---|---|
| Kind | onion, carrot | Name (type) | Decides what an action applies to |
| Category | peelable, root vegetable, leafy vegetable | Trait (set of properties) | Avoids writing actions per ingredient |
| State | Combination of the doneness, browning, shape and prep axes | State type | Makes missing steps, wrong order and undercooking type errors |
| Properties | Typical heating time, hardness, density, water content | Attribute values (`props`) | Time calculation, unit conversion, warnings |
| Things known only while cooking | Actual heating time, doneness | Range (`15..20 min`) + value at cooking time | The schedule is built with ranges and fixed while cooking (waiting for sensors is external integration) |

- **Categories are traits**: `peel` accepts any `Peelable` ingredient. Actions are not defined per vegetable.
- **Cuts = the action's type + state arguments**: actions such as `slice` / `dice` change the state, and the size is kept in the state. Heating time is computed at compile time by the "heating time model" below (level 3 compile-time evaluation).
- **Heating time model: a sum of two parts**
  - **Time to heat through to the center**: proportional to (thickness ÷ reference thickness)ⁿ. The exponent n has a default per cooking method and can be a range (if the exponent is a range, the time is a range too, and the schedule is built on the safe side). The provisional values are below; they will be replaced following the source priority in §12.3.

    | Cooking method | Provisional exponent n | Basis |
    |---|---|---|
    | Simmer, boil, steam | 2 | Heat conduction |
    | Grill in a frying pan or on a griddle | 2 | Contact with the hot surface puts heat in well (a heat-conduction analysis of steak used 900 on the heated side and 60 W/m²°C on the air side. arXiv:2206.13900) |
    | Stir-fry | 1.5–2 | A mix of contact and air (because the food is tossed while heating) |
    | Oven, toaster oven | 1–2 | Heat transfer from the air is weak. No source (provisional) |
    | Microwave | Not used | It heats from the inside, so the thickness formula does not apply. The time is kept directly as an attribute |
  - **Time independent of size**: for example, the time for vegetables to soften (pectin breakdown, a chemical reaction that proceeds after the heat has arrived). Kept as an attribute per ingredient and cooking method.
  - How "thickness" is taken is decided per cut (slices: the thickness; dice: the side length; rolling cut and rough chop: the typical size). For things with irregular shapes, such as leafy vegetables, the formula is not used and the time is kept directly as an attribute.
  - Both are kept as ranges and can be overridden per ingredient.
  - Basis: the time for heat to reach the center is proportional to the square of the thickness (heat conduction, Fourier number). But when heat enters the surface poorly (small Biot number, grilling in air, etc.), the power is smaller than 2, and vegetable softening proceeds as a first-order reaction independent of size. References are in §11.4.
- **Hardness is an attribute, not a type**: it is used for the "put harder things in first" check (e.g. a warning if cabbage goes into the pot before carrots).
- **Heating time is kept as a range**: the actual time depends on the pot and the heat, so it is not fixed at compile time. This follows the "separate the timing of values" policy in §4.3.

Syntax sketch (details not settled):

```
trait Peelable
trait Root

ingredient Carrot : Root, Peelable, SkinEdible {
  axes: doneness, shape, prep        // 使う軸（焼き色は使わない、など省ける）
  props {
    hardness: hard
    density:  0.6 g/ml
    simmer {
      heatUp: 4..6 min at 20 mm   // 中まで温まる時間（基準の厚さで）
      soften:  8..12 min           // 柔らかくなる時間（大きさによらない）
    }
  }
}

action simmer(x: T<shape: cut>, ...) -> T<doneness: done> where T: Vegetable {
  takes heatUp(T.simmer, x.size, exponent: simmer.n) + T.simmer.soften
}
```

**Meat and fish cuts, names, and using everything up**

The criterion is "**does the difference matter for cooking**". Differences that matter for cooking (fat content, hardness, bones or not, heating time) are kept in types and attributes; differences only in name (names of fish that change as they grow, regional names, names that depend on the dish) are kept in phrase files (§7.1).

| Difference | Example | Representation |
|---|---|---|
| Cut | loin, shoulder loin, belly, collar, fish belly, bones and trimmings | Type argument (`Pork<belly>`). Parent-child relations are inheritance, properties are traits |
| Growth stage | hamachi → buri | Attribute (`stage`). The phrase file picks the name from the attribute |
| Peak season | buri in winter, bonito in spring and autumn | Neither a type nor an attribute. Kept as meta information (`meta`) (below) |
| Regional name | inada (Kanto) / hamachi (Kansai) | Phrase files only (`$dialect`). Unrelated to types |
| Store name | karubi (usually belly, but some stores include other cuts) | A type alias that is the union of the candidate cuts (`alias`) |
| A different kind that can substitute | kanpachi and buri (hamachi) | Define them as separate ingredients with a shared trait. Express the differences as attributes |

```
ingredient Pork : Meat {
  parts {
    loin          { fat: medium, tender: high }
    shoulderLoin : loin { fat: high }    // ロースの一種として継承
    belly : Fatty { simmerTime: 60..90 min }
  }
}

alias Karubi = Beef<belly | Fatty>      // 売り場の名前（部位はゆるい）

ingredient Buri     : Amberjack { props { fat: high } }
ingredient Kanpachi : Amberjack { props { fat: medium, firmness: high } }

let fish = teriyaki(x: Amberjack<fillet>, ...)   // どちらを買っても通る
```

- **Checks work even when the cut is not fixed (meaning of a union type alias)**: a union type such as `Karubi` means "one of the candidates". At compile time it is unknown which one, so **only what holds for all candidates** is accepted.
  - Types: if an action requires `Fatty`, it passes when all candidates are `Fatty`, and it is an error if even one is not. A recipe can write a requirement such as "any fatty cut" (`Pork<Fatty>`).
  - Attributes (heating time, etc.): use the widest range, combining the candidates' ranges (the schedule is built on the safe side).
  - Safety: if any candidate is `Hazard`, it is treated as `Hazard`.
  - Names: an alias is expanded in types, but display and diagnostics use the alias's name ("karubi"). Phrase files can hold a name per alias.
- **Tell "one of" from "a mix of"**: `|` (union) is used only for "one of pork **or** beef". Something like mixed ground meat, which "contains **both** pork **and** beef", is defined as an ingredient whose makeup is an attribute (`Mixture`), following "mixing makes a new value" in §4.1.

  ```
  ingredient Aibiki : Mixture {
    contains: { Beef: 0..100 %, Pork: 0..100 % }   // 割合は分からない
  }
  ingredient Aibiki73 : Mixture {
    contains: { Beef: 30 %, Pork: 70 % }           // 店が割合を示しているなら書ける
  }
  ```

  - Makeup ratios can be ranges, because the ratio of store-bought mixed ground meat is often unknown. Attributes use the widest (safe-side) value over the whole range.
  - Even if the ratio is unknown, what is inside is known. **Safety checks and allergens treat it as having everything that is inside** (if either one is `Hazard`, the mixed ground meat is `Hazard`; if either one has an allergen, the mixed ground meat has it too).
- **The language has no notion of seasons. Peak season is meta information**: type checks and schedules do not change with the season (a recipe can be made in any season, and seasonal differences such as fat content are included in attribute ranges). Peak season is meta information that can be written in an ingredient's `meta`, and is not used by type checks or scheduling. Renderers and search ("dishes in season now", etc.) use it.

  ```
  ingredient Buri : RoundFish {
    meta { inSeason: [winter] }    // 型検査・段取りには使わない
    ...
  }
  ```
- **Butchering actions have several outputs**: filleting a whole fish into three pieces yields "fillets, collar, fish belly, bones and trimmings" plus discarded parts such as innards. The yield is kept as a range attribute of the fish, and the total amount is preserved. Cutting up a block of meat uses the same mechanism.
- **Tracking use (linear types)**: unused cuts do not silently disappear. Without a declaration, "300 g of fish bones and trimmings remain" is reported as **info** (often they are left on purpose, so it is not a warning). If the intent is written with `store(ara)` (keep) or `discard(...)` (throw away), it only appears in the schedule table and gives no diagnostic. On a day that aims to "use everything up", a setting can raise it to a warning (§6.1).
- **Unused values (`unusedValue`)**: an ingredient value that is made but never used (a bound value, the value of an expression statement that is not bound, an ingredient that is only declared) is reported as the **warning** `LIN005 unusedValue` at the end of evaluating the dish, because throwing away a whole batch of cooked rice is almost always a writing mistake. It is not given for the last value (the dish's output) and its tuple, served values, cuts (leftover cuts are the info `LIN003` above), intermediate ingredients from which a part was taken by division, or values passed to `store(x)` / `discard(x)`. Dishes called with `make` and the `steps` of composed actions are also excluded (it appears when that dish is evaluated on its own). `@allow(unusedValue)` hides it.
- **Passing outputs around within a menu**: sashimi uses the fillet, grilled collar uses the collar, and buri daikon uses the bones and trimmings; the outputs of butchering are passed as ingredients to other recipes. If the whole menu is compiled as one, types track which cut is used where.

### 4.2 Concurrency, parallelism and resource constraints

- `parallel { branch ... }` is **fork-join**. Each `branch` runs in order inside, and nothing goes on until all branches finish.
- **"Concurrent (independent)" and "parallel (actually running at the same time)" are different**. The writer declares only concurrency, and the scheduler decides parallelism from resources (one cook: nearly serial; two cooks: parallel).
- **Effect annotations**:
  - `@unattended` (left alone; frees the hands; soaking, cooking rice, simmering, etc.)
  - Uses hands (the default)
  - Waiting for sensors (later, as external integration)
- **Resources**: kinds such as stove, work spot, sink, cutting board, pot and rice cooker are declared as `resource`. The capacity (number of burners, number of units, etc.) comes from the **kitchen spec**, not from the recipe. The number of cooks comes from the **build-time conditions** (below).
- **Data dependencies are implicit join points** (`plate ... { rice, curry }` waits for both outputs).
- **How the serving time is set (there is no `goal`)**: serving things together is expressed by serving waves (§4.5). What remains is only "when to serve", which is today's circumstance, so it is a build-time condition.
  - With `--serve-at 19:00`, the schedule is worked back from that time and starts as late as possible.
  - Without it, start as early as possible and serve early.
  - Finer preferences (fewer washes, less waiting, etc.) are also passed as build-time conditions (`--prefer fewerWashes`, etc.).
  - This leaves only "what to make" in the recipe and menu code.
- **What is detected at compile time**: concurrent branches that both consume the same ingredient, a resource capacity overflow that cannot be avoided, etc. Diagnostics point at the source location before expansion.
- **Scheduler implementation**: no real threads are used; start and end times are computed by discrete-event simulation.
  - **The base is resource-constrained list scheduling**: work whose dependencies are done is assigned, in order of priority, to free resources (people, places, tools). Deterministic and fast.
  - **The direction switches on the serving time**: without `--serve-at`, pack forward (start as early as possible and serve early). With it, pack backward from the serving time (start as late as possible).
  - **Washing is inserted during assignment** (setup time; see "Washing up and cleanup" below).
  - **Preheating is also inserted during assignment (`preheat`)**: an action's `uses` writes the heated capability like `HeatSource<oven, heated: 200 degC>` (the temperature can also be an expression of the action's arguments). The scheduler puts an unattended preheat task on the same unit that the task uses. Its duration comes from the equipment kind's attribute `preheatTime`. Preheating is placed ahead of time so that it ends when the task starts, and it runs in parallel with earlier tasks. From the start of preheating to the end of the task, that unit is not given to other tasks. When the next use is at the same or a lower temperature, it is not preheated again; at a higher temperature it is. If the kitchen unit's `maxTemp` is lower than the needed temperature, the kitchen does not meet the requirement (`KIT002`). Preheating is not put in the IR (like washing, it is inserted at the scheduling stage, where the kitchen is known).
    - **Preheating frying pans and oil**: `fry` has `uses ..., FryingPan<heated: heat>` and preheats by heat level rather than by temperature. The heat level is the ordered enum `enum Heat : ordered { low, medium, high }`; at the same or a lower heat level next it is not reheated, and at a higher one it is. Deep frying, `deepFry`, heats the oil in the pot with `Pot<heated: temp>` (default 170 °C). Preheating a tool that is heated on the stove also occupies, from the start of preheating, the same burner as the task that uses it. The durations are the `preheatTime` of `FryingPan` and `Pot` (provisional). The schedule text and table show "heat the frying pan over medium heat" and "heat the oil to 170 °C"; the recipe text does not.
  - **Continue in the same pot**: the output of an action that uses a tool that holds contents (capability `Holding`; in std, `Pot`, `FryingPan`, `MixingBowl` and `Blender`) remembers the tool it is in (it stays in after an action that uses no `Holding` tool, such as `season`). The next action that uses the same kind of tool continues in the unit that holds its input (for a list, the first element in written order), inserts no washing or preheating, and lets no other task use the unit between the end of the previous task and the start of the next. With a different kind of tool, the contents are moved. `@sameVessel` checks that it continues (`TYP017` if it cannot), and `@newVessel` moves to another unit. The recipe text adds to the previous step, like "Add the beaten egg to 3, ...".
  - **Improve the finished schedule by repacking it forward and backward**: a light improvement procedure often used in resource-constrained scheduling. It does not guarantee the optimum, but it is deterministic and finishes instantly at the size of a home menu.
  - **Fix the tie-break**: tasks with the same priority go in IR node number order. Every platform gets the same schedule (§6).
  - **Exact optimization (mathematical programming, etc.) is not in the core**: its computation time is unpredictable and determinism is hard to keep.
  - **Order of aims**: (1) keep the allowed serving window and the wave constraints (required) → (2) shorten the total time (with `--serve-at`, start as late as possible) → (3) what `--prefer` names (in the given order).
  - **Kinds of `--prefer`**: `fewerWashes` (fewer washes; prefers orders that use the same tool in a row), `lessWaiting` (less idle waiting for the cooks), `fewerTools` (fewer tools used) and `steadyPace` (less time handling several tasks at once).

**Split the input in three (requirements, equipment, today's conditions)**

This is the pillar of the parallelism design. If equipment or the number of cooks is written into a recipe, the recipe only works correctly in the writer's kitchen on that day. So **the recipe writes only "what is needed"; "what is available" comes from the kitchen spec and "what to do today" from the build-time conditions, both passed in from outside**.

| Input | What it says | Example | How often it changes |
|---|---|---|---|
| Recipe (code) | What to make, abstract requirements | One heat source, something that can cook rice, needs hands | Per recipe |
| Kitchen spec | The home's places and tools | 3-burner IH, 1 counter, 1 sink, 2 cutting boards | Rarely |
| Build-time conditions | Today's circumstances | How many cooks, how many servings (override), when to serve, when cleanup must be done | Every time |

Compilation is "recipe × kitchen × today's conditions → schedule table". The same curry comes out closer to serial with 1 cook + 2 burners, and closer to parallel with 2 cooks + 3 burners.

**Kitchen spec: split into places and tools**

The boundary: **a place is a spot you go to and work at, and it is wiped where it is when dirty (`wipe`). A tool is used in the work, and it is washed at the sink when dirty (`clean`)**. This criterion matches the washing-up mechanism (later in §4.2): a rice cooker (its inner pot is washed) is a tool, and a microwave (wiped in place) is a place. The standard vocabulary is in §12.3.

```
kitchen myHome {
  places {
    floor:     Floor     { capacity: 2 }     // 同時に立てる人数
    counter:   Counter   { workSpots: 1 }   // まな板を広げられる場所
    sink:      Sink
    stove:     IHStove   { burners: 3 }
    microwave: Microwave
    fridge:    Fridge                        // 容量の既定は「十分に入る」
  }
  tools {
    board:  CuttingBoard { count: 2 }
    pan:    FryingPan    { diameter: 26 cm }
    pot:    Pot          { count: 2 }
    cooker: RiceCooker   { capacity: 900 ml }
  }
  tableware {
    deepPlate: DeepPlate { count: 4, capacity: 500 ml }
    soupBowl:  SoupBowl  { count: 4 }
  }
}
```

- **The notation is "name: kind { attributes }"**: the kind is a type, so an IH stove is `IHStove` and a gas stove is `GasStove`, each a kind of `Stove`. Leaving out `count` means 1. Tableware is a kind of tool, but there is a lot of it and it hurts readability, so it is written separately under `tableware`. Without the `: Kitchen` type annotation, it is still `Kitchen`.
- **Actions require "place + tool + hands"**: this is declared with `uses` in the action signature. `fry` needs one burner + frying pan + hands; `slice` needs a counter + cutting board + hands.

  ```
  action slice(x: T<shape: peeled>, t: Length = 3 mm) -> T<shape: cut(slice, t)> {
    uses hands, Counter, CuttingSurface for 2 min
  }
  ```

- **An action's `uses` is written in capabilities (traits)**: it asks for capabilities, not for specific equipment kinds. Kitchen equipment gets its capabilities from the definition of its kind (`kind`).

  ```
  // パントリー: 設備の種類が能力を持つ
  kind IHStove   : Stove, HeatSource<stovetop>
  kind Microwave : HeatSource<microwave>
  kind OvenRange : HeatSource<oven>, HeatSource<microwave>   // オーブンレンジ
  kind Pot       : Container

  // 動作: 能力で要求する
  action simmer(x: T, ...) -> T<doneness: done> {
    uses hands, HeatSource<stovetop>, Container<capacity >= volume(x) * 1.2>
  }
  ```

  - **The amount sets the requirement**: like "pot capacity at least 1.2 times the contents", the needed size is computed automatically from the amount known through linear types. For 2 L of curry, the check requires a pot that holds it.
  - **One unit can have several capabilities**: a combination oven-microwave works as an oven and as a microwave. But while it is used for either one, the whole unit is taken. Capabilities are "different views of the same equipment", and contention is counted per unit of equipment.
  - **New appliances are added by declaring capabilities**: an appliance such as an automatic cooking pot works with the existing actions as soon as a writer defines its kind and capabilities.
  - The standard capabilities are in §12.3.
- **Alternative methods (`variant`): fallbacks when equipment is missing**: one action lists alternatives that differ only in the equipment and steps they use. The input and output types are the same (this is not name overloading; §4.3).

  ```
  action cookRice(x: Rice<prep has soaked>) -> CookedRice {
    variant cooker {                       // 炊飯器
      uses RiceCooking for 40 min @unattended
    }
    variant pot {                          // 鍋で炊く
      uses hands, HeatSource<stovetop>, Pot
      steps { boil(...); simmer(..., 12 min); rest(..., 10 min) @unattended }
    }
    variant microwave { ... }              // 電子レンジ
  }

  extend action cookRice {                // 他のモジュールから候補を足す
    variant autoCooker { uses AutoCooking for 50 min @unattended }
  }
  ```

  - **Recipes do not care about the kitchen**: a recipe just writes `cookRice` and works in any kitchen (requirements are separate from equipment).
  - **Type checks run on all alternatives**: errors inside alternatives that are not used are found even without a kitchen.
  - **The IR holds all alternatives as a "pick one" branch**: which one is used is decided at the scheduling stage, so the IR is the same for every kitchen.
  - **Alternatives the kitchen cannot use are dropped**: once a kitchen is injected (level 2), alternatives whose `uses` cannot be met are dropped. If all of them are dropped, it is an error that lists the alternatives (`KIT`).
  - **The remaining alternatives are chosen during scheduling**: at assignment time, the alternative that finishes earliest is chosen (heat it in the microwave if the stove is full, etc.). On a tie, the alternatives' order decides. Research calls this a multi-mode problem: the scheduler chooses "which way" as well as "when".
  - **The chosen alternative is reported as info**: it appears in the schedule table, like "There is no rice cooker, so the rice is cooked in a pot".
  - **Without a kitchen (levels 0 and 1), the first alternative is used**: equipment is assumed to be unlimited, and the author's standard method (the first alternative written) gives the estimate. The assumption list says something like "Cooking rice assumes the first alternative (rice cooker). There are also pot and microwave alternatives".
  - **Order of added alternatives**: alternatives added with `extend action` go after the original definition's alternatives, in package name order. The first alternative is always one from the original definition. The assumption list shows which alternatives were added.
- **Requirements (`requires`) are basically derived from the actions automatically**: a recipe's requirements are known by collecting the `uses` of the actions it uses. A writer writes `requires` only for extra conditions that the actions do not reveal. The syntax is the same as the type `< >` (§4.4).

  ```
  recipe roastBeef(servings: 4)
    requires Oven<maxTemp >= 250 °C>
  { ... }
  ```
- **Only counts (capacities)**: positions, area, movement paths and distances are left out (the precision gained does not justify the burden of defining them).
- **Defaults when omitted**: 1 sink, 1 work spot, etc. A kitchen spec can be short, and only those who care write the details.
- **Corrections per cooking method can be written**: differences per kitchen, such as "my oven is slower than its display says", cannot be absorbed by the standard values, so the user writes a correction in the kitchen spec (`calibration { oven: 1.1 }`, etc.) and adjusts it from experience after cooking a few times. Corrections are part of the kitchen, so they are not in shared recipes (§4.6).
- **Usable as compile-time values**: kitchen capacities are compile-time values (§4.3). A split count such as `ceil(meat / pan.capacity)` is decided by the home's equipment.
- **When equipment is missing**: a usable one is picked from the action's alternative methods (`variant`) (e.g. without a rice cooker, cook rice in a pot on the stove). If none is usable, a compile error says so (see "alternative methods" above).
- **Planning a menu (several recipes) together**: several recipes can be poured into one kitchen and scheduled, including contention for the stove and the sink. This works because the kitchen is a separate input.
- **Handled like any other source**: a kitchen spec is also text source and can be shared offline. Shared ones are treated as untrusted input (§10). In Ozendate, it is registered as "my kitchen" and passed in every time.

**Build-time conditions: "how many servings" and "how many cooks" are different things**

```
ozen build recipes.curry --servings 4            # 何を作るか（レシピの既定値を上書き）
                             --kitchen myHome   # どこで作るか
                             --cooks 2           # 誰が作るか
                             --serve-at 19:00    # 何時に出すか（省略時はなるべく早く）
                             --cleanup beforeMeal # 片付けをいつまでに（後述）
```

| | How many servings (servings) | How many cooks (cooks) |
|---|---|---|
| What changes | The dish itself (ingredient amounts, number of batches, needed pot size) | Only the schedule |
| Stage it affects | Expansion, type check | Scheduler only |
| Where it lives | A value of the recipe (or menu). Can be overridden at build time | Build-time conditions |

- **Invariant: changing cooks or the kitchen does not change the IR (DAG). Only the schedule changes.** This is because washing insertion and the choice of alternative methods both happen at the scheduling stage. Changing servings can change the IR (amounts, `repeat` counts). A test checks that "changing only cooks or the kitchen gives an identical text IR dump and only a different schedule".
- To split work between people (e.g. two people cutting 4 onions), write independent tasks in the recipe and leave the assignment of who does what to the scheduler.
- "How many people can stand at once" is a property of the kitchen, so it is on `floor`. It is a different constraint from "how many people cook today".
- Per-person attributes (whether they can use a knife, etc.) belong to the build-time conditions.
- servings and cooks are both compile-time values, so they can be used in `if` / `repeat`.

**Washing up and cleanup (part of the design from the start)**

Cooking is impossible without thinking about washing up, so **the mechanism and the vocabulary (kinds of dirt, rules) are designed from the start**. This has the same structure as "sequence-dependent setup time" in production scheduling, and can be handled as an extension of list scheduling.

- **Tools and places have state**: with the same mechanism as ingredient state types, they have `clean` and `dirty(汚れの種類)`. Tools and tableware go back to `clean` with `clean(t, level)` (`level` is the washing method), and places (counter, stove, etc.) with `wipe`. The name differs from `wash`, which washes ingredients (§4.3).
- **Dirt is decided by ingredients and actions**: it is decided automatically from ingredient attributes (`leaves`) and actions (`fry` leaves oil in the pan). The recipe writer does not need to think about dirt.
- **Dirt is a kind + a category (trait)**: the washing method and time are decided by the category. Food safety rules (`Hazard`) are constraints that cannot be relaxed.
- **Compatibility rules are defined in the pantry**: "after raw meat, washing is needed before any use", "vegetables after vegetables can go as is", etc. The standard dirt kinds and compatibility rules are in §12.3.
- **Washing methods are an ordered enum**: nothing < rinse < wash with detergent < soak, then wash. Soaking is `@unattended` work.
- **Washing is just work too**: it uses "sink + hands + time". The duration comes from the tool's attribute. Soaking is expressed as `@unattended` work.

```
dirt rawMeat : Hazard          // 衛生上、必ず洗う
dirt rawFish : Hazard, Smelly
dirt oil      : Greasy          // 洗剤が要る
dirt starch   : Sticky          // 浸け置きが効く
dirt plain    : Light           // そのまま使える

axis cleaning : ordered { none, rinse, wash, soakThenWash }   // 洗い方

tool cuttingBoard {
  states: clean <-> dirty(kind)
  props { washTime: 2 min }
}

ingredient Chicken { props { leaves: rawMeat } }
action fry(...) { leaves: oil on pan }

rule after Hazard: wash before any use
rule after Smelly: rinse before RawEdible
rule after Light:  none
```

- **Washing is inserted at the scheduling stage**: whether washing is needed depends on "the order in which tools are used", so it is not in the IR before scheduling (this keeps the cooks invariant). When assigning a tool, if no compatible clean tool is available, the scheduler chooses between "wait for another tool to become free" and "insert a wash". Priority tweaks also produce orders such as "on the same cutting board, vegetables first and meat later".
- **Inserted tasks carry a reason**: like "cutting board 1, after raw meat", so that the schedule table can explain them.
- **Definition of cleanup**: cleanup specifies "what state the kitchen (tools and places) should be in at the end". When cleanup must be done by is a build-time condition.

| `--cleanup` | Meaning | With several waves | Relation to the serving time |
|---|---|---|---|
| `none` (default) | Only the washing needed during cooking. It may be dirty at the end | Same | No effect |
| `beforeMeal` | At serving time, the tools and places finished with by then are clean. Tableware is not included | Per wave, clean "what was finished with before serving that wave" | On the path to the serving time |
| `afterMeal` | After the last meal ends, clean everything, tableware included | After the last wave's meal | Not on it |

- Wiping places (counter, stove) is included in every option except `none`.
- Storing leftovers (things kept with `store`; putting them in the fridge) is included in cleanup and goes into the schedule.
- Hands are busy during the meal (§4.5), so the only cleanup that goes on during the meal is unattended work (running the dishwasher, soaking, etc.). Even with `afterMeal`, if the dishwasher can run during the meal, the scheduler plans it that way.

- **Drying is not handled**: washed tools are treated as usable at once. Drying time, dish rack capacity and drying with a towel are outside this concept (§12 non-goals).

**Plating and tableware**

Tableware is also expressed with the existing mechanisms (traits, kitchen spec tools, linear types, washing up). Pairings such as curry in a deep plate and miso soup in a soup bowl can be checked and assigned.

| Where | What |
|---|---|
| Pantry | Kinds and categories of tableware (deep plate, flat plate, soup bowl, rice bowl, donburi, small bowl, platter, etc.) |
| Kitchen spec | How many pieces of tableware the home has and their capacity (as the tool `tableware`) |
| Recipe | Which category of tableware to plate in (abstract requirement). The compiler decides which actual piece to use |

```
// パントリー
vessel DeepPlate : HoldsLiquid
vessel SoupBowl  : HoldsLiquid
vessel Donburi   : HoldsLiquid
vessel FlatPlate

// 台所スペック
tableware {
  deepPlate: DeepPlate { count: 4, capacity: 500 ml }
  soupBowl:  SoupBowl  { count: 4, capacity: 300 ml }
  donburi:    Donburi   { count: 2, capacity: 800 ml }
}

// 献立（§4.5）
plate perPerson on DeepPlate or Donburi {
  rice, curry
}
```

- **Separate "required" from "preference order"**: "soup must go in tableware that holds liquid" is a required constraint derived from the dish's state (liquid). "Curry is best in a deep plate, but a donburi is fine" is a **tableware preference order**, written `on DeepPlate or Donburi` (deep plates first, donburi if they run short). If tableware runs short, the preference order is used first; if that still does not work, it is a compile error.
- **Count check**: plated tableware is in use until the meal ends (a linear resource). The check is "servings × pieces per person ≤ count", and it gives assignments and diagnostics such as "3 deep plates for 4 servings, so 1 donburi is used".
- **Several dishes on one plate / sharing**: curry rice puts the rice and the curry on one plate. Side dishes may also go together on one platter. Two kinds express this: `perPerson` (one per person) and `shared` (shared from one dish).
- **Dividing amounts**: plating is an action that "divides the total amount by servings into each piece of tableware". Linear types keep the total, so leftovers and over-plating can be checked, and going over a piece's capacity gives a warning.
- **Link to washing up**: if small bowls or mixing bowls are used in prep, washing them before plating is handled by the existing washing insertion. Washing the tableware after the meal is covered by `--cleanup afterMeal`.
- **Plating takes time too**: plating for everyone uses hands. It is included when working back from the serving time.
- **Not handled**: warming tableware, visual arrangement (the beauty of plating), and matching the formality of tableware and dish (§12 non-goals).

**Types of kitchen specs and conditions**

Injected settings (kitchen specs, build-time conditions) are handled by the same language and the same type checker as recipes. The kinds and counts of the tableware and tools a kitchen has are also defined following types.

| Layer | Where | Content |
|---|---|---|
| Definition | Pantry | Declare the kinds of tableware, tools and places, and the "shape" of kitchen specs and conditions, as types |
| Value | Kitchen spec, build-time conditions | Concrete settings written following the types |
| Requirement | Recipe | Write "this kind of kitchen is needed" as a type constraint (`requires`) |

```
// パントリー: 形を定義する
enum Cleanup { none, beforeMeal, afterMeal }

type Options {
  servings: Count? in 1..   // 省略時はレシピの既定値
  cooks:    Count in 1..  = 1
  cleanup:  Cleanup       = none
  serveAt: Time?                // 省略時はなるべく早く出す
  prefer:   [Preference]  = []  // 洗い物を減らす、待ち時間を減らす など
}

type Kitchen {
  places:    { floor: Floor, counter: Counter, sink: Sink, stove: Stove }
  tools:     [Tool]
  tableware: [Vessel]
}

// 台所スペック: 値を書く（記法は上の「台所スペック: 場所と道具で切る」）
kitchen myHome {
  tableware {
    deepPlate: DeepPlate { count: 4, capacity: 500 ml }
  }
  ...
}

// レシピ: 要求は使っている動作の uses から自動で決まる。
// 動作から分からない追加の条件だけ requires で書く
recipe roastBeef(servings: 4)
  requires Oven<maxTemp >= 250 °C>
{ ... }
```

- **Setting mistakes are compile errors too**: a unitless `capacity: 500`, `count: -1`, and misspellings such as `cleanup: afterDinner` are diagnosed with a span. Dimension types work on kitchen specs as they are.
- **Recipe-kitchen fit is checked first**: "an oven is needed, but this kitchen has none" is reported as a type mismatch before scheduling. If there is an alternative action, it is used instead.
- **Range constraints keep only intervals in types**: the intervals `a..b`, `a..` and `..b` can be written for numbers and for quantities with units. When the lower end is excluded, write `a<..` (with an upper end too, write `a<..b`, and the upper end is included). Write `<` and `..` together with no space between them; this means "greater than a". For example, `Time in 0 s<..` excludes 0 seconds and allows only longer times. Arbitrary condition expressions (such as "must be even") are not kept in types (so that type checking always terminates and diagnostics stay easy to understand). Kitchen specs, build-time conditions, recipe arguments and action arguments are all compile-time values, so out of range is always an error (`TYP`) and can be detected.

  ```
  recipe curry(servings: Count in 1..20 = 4) { ... }
  type FryingPan : Tool {
    diameter: Length in 10 cm..40 cm
  }
  action boil(x: T, time: Time in 0 s<..) -> T<doneness: done> { ... }
  ```

- **Checks across fields are written as named `check`s in the type definition**: these are rules over several values, such as "the number of cooks exceeds the number of people who can stand at once". Each `check` has a diagnostic name and a default severity, and the severity can be changed with the mechanism in §6.1. The writer decides the default severity (the standard library follows the criterion in §6.1: "a warning if it can continue, an error if it does not work as a dish").

  ```
  type Options {
    ...
    check tooManyCooks: cooks <= kitchen.floor.capacity  else warn
  }
  ```

  - A `check` that refers to the kitchen is evaluated only when a kitchen is injected (verification level 2). When none is injected, it is not evaluated and gives no diagnostic (this fits the monotonicity in §5.1).
  - `check` expressions are written in a subset of compile-time evaluation, so the fuel limit applies too.
- **Extensions use the same syntax**: a new kind of tableware or appliance only needs one more type in the pantry.
- **Settings screens can be built from types**: Ozendate's "my kitchen" and "today's conditions" input screens can be generated from the type definitions. Adding a field needs no change to the app code (screen generation is outside the core).
- **Timing**: kitchen specs and conditions are all compile-time values. They can be used in branches such as `if kitchen.has(Oven)` (within the "separate the timing of values" policy in §4.3).

### 4.3 Extensibility (metaprogramming)

Units, ingredients, actions, resources and steps can be defined with the same syntax as the standard library. The standard library itself ships as source in this language.

| Level | Content | Status |
|---|---|---|
| 1. Declarations | Definitions of dimensions, units, ingredients (state transition graphs), actions and resources | In scope |
| 2. Composition | New actions that combine actions (with arguments, expanded into the DAG where used) | In scope |
| 3. Compile-time evaluation, macros | Evaluate `if` / `for` / `repeat` on compile-time values; generate steps with `macro` + `quote` | In scope (the implementation approach is decided by the evaluator prototype) |

**Level 3 design policy (agreed)**

- Compile-time code is written in **a subset of the same language as the recipe body**. No separate language is made.
- Separate the "timing" of values: **compile-time values** (servings, ingredient amounts, resource capacities, lists) can be used in `if`/`for`. **Cooking-time values** (temperature sensors, etc.) cannot (type error).
- Macros **work on syntax trees**, not on string substitution (built with `quote` / `$`). They are **hygienic**, to avoid name clashes.
- **Implementing hygiene: mark each expansion**
  - Each time a macro is expanded, it gets a unique number (the expansion mark).
  - Names that the macro itself writes inside `quote` get that mark and are resolved in the namespace where the macro was defined. Argument names received through `$x` keep the caller's mark. Temporary names inside the macro (`tmp`, etc.) do not clash with the caller.
  - **No macros that make macros** (the more general approach [scope sets] is needed mainly in this case, and generating cooking steps does not need it).
  - **No escape hatch for capturing names on purpose**: to show a name to the caller, pass that name as an argument (e.g. `marinate(meat, into: marinated)`). A shared macro cannot silently overwrite the caller's names.
  - The expansion marks are also used as they are in the expansion history of diagnostics (§10).
- Evaluation is **pure and always terminates**: no files, time, random numbers or network. With a **fuel** system, there are limits on evaluation steps, recursion depth and the number of generated nodes. This is required because untrusted input (shared recipes) is handled.
- Type checks run on the AST **after expansion**. When a macro is defined, only the minimum (number of arguments, undefined names) is checked.
- Error locations carry the **expansion history** (an error inside a macro also points at the caller's line).
- Value types are kept few: about 5 kinds, namely quantities, strings, lists, ingredient references and syntax trees.

**No name overloading**

- The language does not have overloading (several definitions under one name, one per type). To group similar actions across ingredients, use traits (anything `Peelable` can take `peel`).
- Reason: even if types can tell them apart, diagnostic text, schedule tables and phrase file keys would all have to tell "which definition", so the names end up being split somewhere anyway.
- Example: `wash` for washing ingredients, `clean` for washing tools and tableware, and `wipe` for wiping places are separate names.

**Pitfalls of extension**

- User-defined state machines are checked at definition time (unreachable states, conflicting transitions).
- Detecting cycles in unit definitions (`unit a = 2 b`, `unit b = 3 a`). The base unit cannot be changed later.
- Namespaces and `import`. Clashes such as `cup` (US / Japanese) are settled by choosing whether to import `std.units.jp` or `std.units.us` (§4.7).
- Extensions are **source distribution only**. Native code is not loaded dynamically (this also fits the iOS signing and review rules).

### 4.4 Syntax

**Decisions**

- **Blocks**: braces `{ }`. Indentation has no meaning (so the meaning does not change when sharing or copy-paste breaks the spaces).
- **Statement separator**: a newline. Use `;` only when you want several on one line.
- **Binding**: `let name = ...`. A name is bound only once (SSA) and cannot be reassigned (§4.1).
- **Arguments**: positional arguments first, then named arguments (`name: value`). Arguments with a default value can be left out. Example: `fry(sliced, 10 min, heat: medium)`
- **Annotations**: put `@name` or `@name(引数)` **after** the expression or declaration it applies to. Examples: `let cooked = cook(soaked, cooker, 40 min) @unattended`, `serve main { ... } @serveWithin(5 min)`
- **Pipe `|>`**: `x |> f(a)` is the same as `f(x, a)`. It puts the value on the left into the first argument of the call on the right. Only a name or a call can be on the right (a qualified name such as `p.cut` is also allowed), and `x |> f` is `f(x)`. It has the lowest precedence, so you can chain it as in `rice |> wash |> soak(30 min)`. An annotation attaches to the last step. The syntax tree rewrites it into calls, so the IR is the same as for nested calls. There is no way to call an action by putting `.` after a value (`negi.cut(slice, 2 mm)`); it is `NAM019 methodCall` (error), which points to `|>` as in `negi |> cut`. Only actions and macros can be called; calling an ingredient or a dish (`Onion(onion)`, `gohan()`) is `NAM020 notCallable` (error). A dish is made with `make gohan`. Values have no fields, and `sliced.size` is `NAM021 valueField` (error).
  - **`until:` written as an end state**: a heating action (an action that declares an ordered axis on its output) can give an end state instead of a time, as in `until: doneness >= par` (`simmer([simmered, beaten], until: doneness >= par)`). That axis of the output takes that value, and the task gets the cook. When the time is left out, it is estimated from the ingredient's `heating { <動作> { <値>: <範囲> } }` (for a mixture, it follows back to the ingredients and takes the widest range), and with `done` and no such field it uses the heating model. If neither exists and there is no `time` either, it is `TYP022 untilTime` (error). An undeclared axis, a value earlier than or equal to the input, or a value that is neither a comparison nor a cue is `TYP023 untilTarget` (error). A cue that a state axis cannot express is written as a value of `enum Cue { fragrant, translucent, boiling, thickened }`, as in `until: fragrant`; it does not change the state and needs a `time`. The text takes the form "Simmer over low heat until the egg is soft-set (about 1–2 min)" or "Stir-fry over medium heat until fragrant (about 1 min)".
  - **Write the ingredient first**: even for a call with only one step, the basic style is to write the ingredient first, as in `chicken |> cut(biteSize, 30 mm)` / `soaked |> cookRice`. This is because a cook thinks from the ingredient: "cut the chicken". Calls that take a list (`simmer([a, b], 10 min)`) and `make` are written as they are. The samples (`samples/`) and the examples on the introduction page also follow this style.
- **Recipes and menus without arguments omit `()`**: `recipe gohan { ... }`, `make cookedRice`. Writing `()` means the same thing.
- **Amount per person `per serving`**: adding `per serving` to an ingredient's amount makes it scale with the number of people (`rice: 75 g per serving`). An amount without the mark is a total that does not change with the number of people. The number of people comes from the dish's argument `servings`, or else from the build condition `servings`; if neither exists, it is `TYP020 servingsUnknown` (error). `per` and `serving` are not reserved words; they are read as contextual words. The number of people (a `Count` value) has no dimension, so `150 g * servings` stays a mass.
- **Quantity literals**: a number and a unit side by side with a space (`200 g`). A range is `15..20 min`, a fraction is `1/4` (kept as a rational number). Quantities of the same dimension written in a row, as in `1 h 30 min`, are read as a sum.
- **Resolving unit names**: a name placed right after a number is resolved as a unit. Units are also names brought in by `import` (`std.units.jp` and so on; §4.7). To use one qualified, write it without spaces: `2 jp.cup`.
- **Comments**: `//` (to the end of the line) and `/* */` (block).
- **Naming (camel case)**: values, actions, recipes, menus, fields, enum values, diagnostic names, modules (file names) and package names use camel case starting with a lowercase letter (`cookRice`, `perPerson`, `leftoverPart`, `katsuCurryNight`). Types, ingredients, traits, kinds of equipment and aliases use camel case starting with an uppercase letter (`Onion`, `Pork`, `DeepPlate`, `HeatSource`, `IHStove`). Cuts and axis values are values, so they start with a lowercase letter (`Pork<belly>`, `shape: peeled`). The language does not enforce this (name resolution does not rely on case), and the standard library follows this convention.
  - Exceptions: CLI options use kebab case (`--serve-at`), keys in phrase files (Fluent) use kebab case (`action-slice`), and the C public API uses snake case (`recipe_build`). Each follows the customs of its own world.
- **Ingredient declarations**: `名前: 型 量` (`roux: CurryRoux 100 g`). If the name is the type name with its first letter lowercased, the type can be left out (`rice: 600 g` is `Rice`).
- **Strings**: `"..."` (UTF-8). A multi-line string is `"""..."""` (for phrases in a bundled file and so on; §4.7). Identifiers are ASCII only, and the display names of ingredients are kept in phrase files or strings (§3).

**What goes inside a type's `< >`**

```
Pork<belly>                    // 部位
Pork<Fatty>                    // 部位のトレイト（脂の多い部位なら何でも）
Onion<shape: peeled>           // 状態（ちょうどその値）
Onion<shape: cut>              // 状態（切ってあれば、切り方は問わない）
T<doneness >= done>            // 順序つきの軸の比較
Rice<prep has washed>          // 実施済みの集合に含まれる
Pork<belly, doneness: raw, shape: cut(slice, 3 mm)>   // 組み合わせ
```

- **A cut is written bare, at the front**: something with only a name and no `:` or comparison is a cut (or a cut trait). It cannot be written for an ingredient that has no cuts.
- **A state is "axis + operator + value"**: `:` means exactly that value (if the arguments are left out, as in `cut`, any arguments match), `>=` / `<=` compare on an ordered axis, and `has` tests whether something is in the set of done steps.
- **Cuts and traits are told apart by name resolution**: cuts are declared inside the ingredient's definition and traits in a namespace, so the definitions tell them apart. If a cut and a trait have the same name, it is an error at definition time. There is a convention that cuts start lowercase and traits start uppercase (naming in §4.4), but the language does not rely on case.
- **An axis you did not write matches anything**: in an input type it means "anything is fine"; in an action's output type it means "carried over from the input". If `fry`'s output only names `doneness`, the shape and the cut stay as they were.

**Literals: the forms are fixed, the vocabulary can be extended**

- **The forms are fixed**: literal forms such as "number + unit", "range", "fraction" and "string" are only those the language defines. There is no way for authors to add new forms to the parser (nothing like reader macros).
  - Reason: macros work on the syntax tree (§4.3). If even the forms could change, the parser, error positions and editor support would become unstable. A shared file must not be able to change the behavior of the reader's parser (§10).
  - A form with the unit first, like the Japanese "1 tablespoon" written as "tablespoon 1", is not needed (identifiers are ASCII; the display word order is decided by the phrase files; §7.1).
- **The vocabulary can be extended**:
  - Units: define `unit gou : Volume = 180 ml` and you can write `2 gou`.
  - Units with a range: vague amounts such as "a pinch" or "a little" can also be defined as units with a range, as in `unit pinch : Mass = 0.3..0.5 g`. Something whose range cannot be fixed either, like "to taste", is `[any amount]` (§4.1).
  - Enum values: values such as `heat: medium` can be added by defining an `enum`.

**Syntax sketch**

```
dimension Mass, Volume, Length, Time

unit g    : Mass   = base
unit kg   : Mass   = 1000 g
unit ml   : Volume = base
unit tbsp          = 15 ml

ingredient Onion : Peelable {
  axes: doneness, browning, shape, prep
}

resource stove   // 種類だけ宣言。容量は台所スペック（§4.2）で与える

action slice(x: Onion<shape: peeled>, t: Length = 3 mm) -> Onion<shape: cut(slice, t)> {
  uses hands, Counter, CuttingSurface for 2 min
}

recipe curryRice(servings: 4) {
  ingredients {
    rice:  600 g        // 名前が型名の先頭を小文字にしたものと同じなら、型（Rice）は省略できる
    onion: 2 pcs
    roux:  CurryRoux 100 g
    water: 800 ml
  }

  parallel {
    branch riceLine {
      let soaked = soak(wash(rice), 30 min)      @unattended
      let cooked = cook(soaked, cooker, 40 min)  @unattended
    }
    branch curryLine {
      let sliced = slice(peel(onion), 3 mm)
      let fried  = fry(sliced, 10 min, heat: medium)
      let curry  = simmer([fried, water, roux], 20 min)
    }
  }

  serve main {
    plate perPerson on DeepPlate or Donburi { cooked, curry }
  }
}
```

A picture of level 3:

```
let batches = ceil(meat / pan.capacity)     // コンパイル時に計算
repeat batches {
  fry(meat / batches, 8 min)
}

macro marinate(x, minutes) {
  quote { mix($x, marinade); rest($x, $minutes) @unattended }
}
```

### 4.5 Menus (one meal) and how dishes work together

**The menu is at the center of the compiler.** Competition for the kitchen, washing up, the number of vessels and using things up matter most when several dishes are made together. A single recipe is treated as "a menu with only one dish", with no special handling. The scope is **one meal** only (meal plans over several days are outside scheduling).

**Kinds of links between dishes**

| Example | Kind of link | What the compiler does |
|---|---|---|
| Katsu curry | **Composition**: combine the outputs of separate dishes into one dish | Calls recipes as functions and passes the outputs along. Joins the DAGs into one |
| Parfait | **Assembly**: layer parts made in advance just before serving | Makes and stores the parts first, and places the assembly just before serving. Keeps the layering order |
| Curry and salad | **Side by side**: no data passes between them; they are served at the same meal | Adjusts only competition for the kitchen and serving timing (hot / cold) |
| Rice or naan | **Choice**: one or the other, or different per person | Computes amounts from each person's choice (rice for 2, naan for 2, and so on) |
| Ramen and fried rice | **Sharing**: they have common ingredients or prep | Splits the common ingredients (chashu, green onion) and adds up the amounts. Cutting tasks can be combined |

**Design pillars**
- **A recipe is a function with a typed output**: it returns the recipe's result as a value, which becomes an ingredient of another recipe or a menu. Passing along the parts of a butchered fish or animal (§4.1) uses the same mechanism.
  - **The output has the same type as an ingredient**: there is no special type like `Dish<...>`. By §4.1's "mixing makes a new value", curry becomes a value of the ingredient "curry".
  - **The output type is explicit if written, otherwise the recipe name's type**: if `-> Curry` is left out, a type named after the recipe with its first letter uppercased (`Curry` for `curry`) is created automatically. A recipe can also have several outputs.

    ```
    recipe curry(servings: 4) -> Curry { ... }              // 出力の型を明示
    recipe homemadeNaan(pieces: 4) -> Naan { ... }          // 既存の食材の型（市販のナン）と同じ
    recipe filletBuri(fish: Buri) -> (Buri<fillet>, Buri<kama>, Buri<ara>)   // 複数の出力
    ```

  - **Swapping make and buy works because the types are the same**: `make homemadeNaan` and `buy Naan` return the same `Naan` type, so they can be swapped in a menu.
  - **The output's properties are decided automatically from the ingredients**: allergens are the union of the ingredients, safety (`Hazard`) is judged from the last doneness, and the state axes hold the result of the last action. Authors can also add traits.
  - **`Dish` is a trait**: it means "something that can be plated and served". It can hold defaults for the vessel (`@vessel`) and for serving-timing properties (`@serveWithin` and so on). It is not put on intermediate ingredients such as dashi.
- **Make or buy can be swapped**: katsu, ice cream, naan and roux can be made from a recipe or bought ready-made. "Anything that satisfies the type is the same, whether made or bought" (this fits the strength of extensibility).
- **Serving timing is a property of the dish**: "serve right away (ramen, freshly fried katsu)", "can wait while kept warm (curry, though it keeps one burner busy)", "keep chilled (salad)", "assemble just before (parfait)" and so on. The scheduler builds the schedule so that each dish's "window in which it may be served" is met. Serving freshly fried items one after another is also expressed with each dish's `@serveWithin`, not with courses.
  - **Vocabulary**:

    | Property | Meaning | Example | Effect on the scheduler |
    |---|---|---|---|
    | `@serveWithin(t)` | Serve within t after it is finished | Ramen, freshly fried katsu, juice | Places the finish just before the serving time |
    | `@keepWarm(on, max)` | Can wait up to max if kept warm | Curry (burner on low), rice (rice cooker's keep-warm) | While waiting, occupies the equipment used to keep it warm |
    | `@chilled` | Kept chilled in the fridge until served | Salad, chilled vessels | Uses the fridge while waiting |
    | `@frozen` | Kept in the freezer until served (assembled) | Ice cream, ice | Uses the freezer while waiting |
    | `@justBeforeServe` | Plate or assemble just before serving | Parfait | Defined in the standard library as an alias of `@serveWithin(2 min)` |

  - **Where to write it**: as a default on the dish's type (the `Dish` trait); a menu's `plate` can override it.
  - **When nothing is written**: there is no constraint, and the dish is treated as able to wait at room temperature. A dish that waits a long time between being finished and served (say, more than 30 minutes) is reported as information ("The curry waits 40 minutes").
  - **Keeping warm uses equipment**: while curry is kept warm, one burner is busy; while rice is kept warm, the rice cooker is busy. Competition for burners is computed correctly.
  - **Deadline from making to using, `@useWithin(t)`**: intermediate values also have freshness, like tempura batter or cut apples. Writing `let batter = whisk([egg, water]) @useWithin(10 min)` keeps the time from the end of the task that made the value to the start of **each** task that takes the value as input within t (as with `@serveWithin`, the making task is delayed and the schedule rebuilt). If this cannot be kept, it is the warning `SCH011 useWithinMissed`. Putting it on an ingredient itself is `TYP017`. Putting it on a value with no task that uses it gives no constraint and no diagnostic (`unusedValue` reports that it is unused). It also works on values made with `make`, and does not change the IR.
  - **Vague time expressions are defined in the standard library as named properties with a range**: expressions such as "while it is warm" or "piping hot" are allowed.

    ```
    property whileWarm   = serveWithin(5..15 min)    // 温かいうちに
    property piping      = serveWithin(0..3 min)     // 熱々で
    property beforeMelts = serveWithin(0..5 min)     // 溶けないうちに

    plate perPerson on DeepPlate { rice, curry } @whileWarm
    ```

    - Because it is a range, the scheduler can build the schedule within the range. Strict mode does not treat it as vague either (the same as units with a range in §4.1).
    - It is shown in words: the schedule table and the recipe say "while it is warm" (the phrase files hold a name for each property; §7.1).
    - The range can be overridden per dish (narrowed for thin things that cool quickly, and so on). Authors can define their household's own expressions in the same form.
    - "Once it has cooled a bit" is not a serving timing but a waiting task. Write the cooling action (`cool`) in the steps with `@unattended`, as "until lukewarm (an estimated range)".

**Serving waves (courses)**

Courses within one meal are kept lightly, as "**waves of when to serve**". The scope is the structure of waves (notation, IR, JSON) and the scheduling of several waves (intervals, and how hands are occupied during the meal). Waves are also used for the chapters of the schedule table.

- A wave holds only "a group of things served together" and "the interval from the previous wave". The default is one wave (everything served together). An ordinary meal, such as one soup and three dishes, needs no waves written.
- **Between waves is "during the meal"**: the cook is eating too, so their hands are occupied. Only unattended tasks (`@unattended`: baking in the oven, chilling to set, and so on) can progress during the meal. The length of the meal comes from the interval.
- **Waves go into the IR**: they are scheduling constraints that decide serving times and how hands are handled during the meal. The wave names ("main", "dessert") are for display, so they go on the AST metadata side (§6).
- **Not included**: nested courses, pacing per plate (eating speed is a value at cooking time and cannot be decided at compile time), and dishes cooked continuously at the table (for yakiniku, hot pot and hand-rolled sushi, the scope is "until it is brought to the table").

```
menu katsuCurryNight(people: 4) {
  let rice  = make cookedRice(servings: 4)
  let curry = make curry(servings: 4)
  let katsu = make tonkatsu(pieces: 4)        // buy Tonkatsu にもできる
  let salad = make greenSalad(servings: 4)

  serve main {
    plate perPerson on DeepPlate or Donburi { rice, curry, katsu }  @serveWithin(5 min)
    plate perPerson on SmallBowl            { salad }               @chilled
  }
  serve dessert after main + 40 min {
    plate perPerson on Glass {
      assemble { flakes, buy VanillaIce @frozen, cream, fruit }
    } @justBeforeServe
  }
}
```

**Menu notation (decisions)**

- **`plate` is an action, `serve` is a wave**: `plate` is the action of putting a dish on a vessel (it uses hands and vessels and takes time; it has a split, `perPerson` / `shared`, and a vessel requirement). `serve` is a wave of when to serve (it groups what was plated and holds the constraint on the serving time). `plate` statements go inside `serve`.
- **Vessel fallback order**: `on A or B` means "prefer A, and use B if there are not enough". It uses a different symbol from an unordered union (`|` in `alias`).
- **Make / buy**: `make レシピ(引数)` and `buy 品名`. If the types are the same, they can be swapped.
- **Choice**: `choose perPerson from { make cookedRice, buy Naan }`. The breakdown of who chooses what is given as a build condition (per-person attributes). If left out, it is split evenly and shown in the list of assumptions (§5.1).
- **Assembly**: the order inside `assemble { ... }` is the layering order as is.
- **Waves**: `serve 名前 { ... }`. The second and later waves get `after 前の波 + 間隔`. If no `serve` is written, everything is served in one wave.
- **Serving time**: not written in the code. It is given as the build condition `--serve-at` (§4.2).
- **A single recipe**: if the recipe has a default vessel (such as `@vessel(DeepPlate or Donburi)`), plating and a wave are filled in automatically as a one-dish menu. When used from a menu, the menu's `plate` takes priority.
- **Sharing**: tasks that cut the same ingredient the same way are combined automatically by the scheduler, with no notation. When something made once is divided (chashu and so on), make it in the menu, divide it with `split`, and pass each part to a recipe as an argument. Recipes can take ingredients as arguments (`recipe ramen(chashu: Chashu<shape: cut>)`).

### 4.6 File units and extension

**Extension: only `.ozen`**

| Location (convention) | Example | What it usually holds |
|---|---|---|
| `pantry/` | `pantry/fish.ozen` | Ingredients, actions, vessels, dirt, traits, aliases |
| `recipes/` | `recipes/curry.ozen` | One dish per file |
| `menus/` | `menus/katsuCurryNight.ozen` | A menu for one meal |
| `kitchens/` | `kitchens/myHome.ozen` | Kitchen spec |
| Anywhere | `katsuCurry.ozen` | Anything may be mixed (a single file to share with others, and so on) |

- **One OS file association, `.ozen`**: iOS also needs only one type identifier (UTType), and it does not clash with other tools (`.recipe` is used by AutoPkg, `.menu` by Cooklang).
- **File kinds are not told apart by extension**: no double extensions with `.recipe` and the like inside.
  - **The kind is decided from the content**: Ozendate calls the core to parse the file anyway, so it can tell exactly from the content that "it has a dish" or "it has a kitchen". Icons and categories are set this way (it can also show things grouped by folder).
  - **The folder is already a marker**: from `recipes/curry.ozen`, a person can guess the content too. Saying the same thing with the extension would be duplication.
  - The problem of the marker and the content disagreeing never happens at all.
  - In the OS file list, everything gets the same `.ozen` icon. We accept this.
- **`.oz` is not used**: an existing language (Mozart/Oz) uses it.

**File unit: one file = one module (namespace). Files are connected with `import`**

Recommended layout (a convention; the language does not enforce it):

```
myKitchen/
  pantry/      fish.ozen
  recipes/     curry.ozen、tonkatsu.ozen、cookedRice.ozen
  menus/       katsuCurryNight.ozen
  kitchens/    myHome.ozen
  phrases/     ja.ftl
```

```
// menus/katsuCurryNight.ozen
import recipes.curry
import recipes.tonkatsu
import recipes.cookedRice

menu katsuCurryNight(people: 4) {
  let curry = make curry(servings: 4)
  ...
}
```

- **Dependencies go one way**: menu → dish → definitions. It is an error if a dish does an `import` of a menu, or if files form a cycle.
- **Nobody does an `import` of a kitchen**: neither menus nor dishes know the kitchen; it is injected at build time (§4.2). The direction of dependencies itself keeps "requirements separate from equipment".
- **Dishes can be reused across menus**: the same `curry` is used from both the katsu curry menu and the curry rice menu. Arguments such as the number of people are passed from the menu.

**Rules that let you write everything in one file**

1. **Any declaration can go in any file**: definitions, dishes, menus and kitchens may be mixed in one file. The kind of file does not limit which declarations can be written (there are many cases where you want to write a small action just for that dish in the dish's file).
2. **Declaration order does not matter** (forward references are allowed): you can write in a readable order, such as the menu at the top with the dishes and definitions it uses below.
3. **The entry is decided automatically, and chosen by name if there are several**: if a file has one menu, that is the entry. If there are several, choose by module name inside a project (`ozen build menus.dinners.katsuCurryNight`), or with `--entry` when handling a single file on its own (§5.2). If there is no menu and only dishes, it is treated as a one-dish menu (so a half-written dish can be run through `ozen check` on its own).
4. **Kitchens can be written too, but are not injected automatically**: even if a shared file contains the author's kitchen, it must not be used for the receiver's schedule without them knowing. It is used only when chosen by name, as in `--kitchen myHome`. Referring to a kitchen by name from a dish or a menu is an error.
5. **Splitting or combining does not change the meaning**: references are resolved by name, and `import` only brings names in. This makes it natural to start writing in one file, and when it grows, move dishes out into separate files and add `import`. Tests confirm that the IR is the same before and after moving them out.

```
// katsuCurry.ozen（1 ファイルに全部）
menu katsuCurryNight(people: 4) {
  let curry = make curry(servings: 4)
  let katsu = make tonkatsu(pieces: 4)
  serve main { ... }
}

recipe curry(servings: 4) { ... }
recipe tonkatsu(pieces: 4) { ... }

action marinate(...) { ... }        // この献立専用の小さな定義
```

A shared file is untrusted input, so even if one file grows large, the limits on input size and fuel (§10) still apply.

### 4.7 Packages, namespaces and import

**Package = a folder with a declaration file**

- Put `package.ozen` directly in the folder and write the name and version **in this language**. No separate format for configuration files (TOML and so on) is brought in.
- The standard library is also a package (named `std`). `std` is a reserved name, and no other package or module can use it.


```
// ourCookbook/package.ozen
package ourCookbook {
  version:  "0.3.0"
  license:  "CC-BY-4.0"               // 共有するときのため。省略可
  requires: {
    std:   "1.3"                       // 1.3 以上 2.0 未満と互換
    dashi: "1.2.0"                     // プロジェクト内にコピーして置いたもの
  }
}
```

- The only fields are the name, the version, the dependencies (`requires`) and the license (optional). There are no fields for defaults (§5.2).

**Module name = package name + folder path**

- `dashi/pantry/fish.ozen` is `dashi.pantry.fish`.
- **Within the same package, the package name is left out**: inside your own package, write only the path from the root (`import recipes.curry`); for other packages, start with the package name (`import dashi.pantry.fish`). If a top-level folder name in your package is the same as the name of a package you depend on, it is an ambiguity error at load time.

**Public and private (access control between modules)**

- **Public by default, hidden with `local`**: a definition marked `local` can be used only inside that file. Trying to `import` a `local` name is an error.

  ```
  local action marinateBase(...) { ... }   // このファイルの中だけで使う
  ```

- **Purpose**: even after a pack is distributed, the author can rename or remove internal helper actions without breaking the dishes that use the pack. Internal helpers do not show up in editor completion or in a renderer's list of actions.
- When bundled (`ozen bundle`) too, `local` stays closed inside its module.

**How to write import**

```
import std.units                  // units.cup のように修飾して使う
import std.units.{cup, tbsp}      // 個別に取り込む
import dashi.pantry.fish as fish  // 別名
```

- The basic vocabulary (common units and actions) is **brought in implicitly** as `std.prelude`. Everything else is explicit.
- There is no form that brings in everything with `*` (it causes clashes).
- `import` only brings names in. Splitting or combining does not change the meaning (§4.6).

**Name clashes**

- If the same name comes from two `import`s, it is an "ambiguous" error **at the place where it is used**. Only bringing it in is not an error.
- Definitions in the file take priority over `import`, and the override is reported as information (§6.1).
- Vocabulary whose meaning differs by region is split into modules, and the importing side chooses (for example, `cup` is in `std.units.jp` and `std.units.us`). This is what "overriding per scope" means.

**Versioning and compatibility**

- **Semantic versioning**: `主.副.修正` (`1.3.0`). Patch is only for bug fixes, minor is for changes that do not break existing recipes (adding vocabulary and so on), and major is for changes that may make existing recipes fail.
- **`std` ships with the compiler and has the same version number**: `std` is never replaced on its own, so we do not manage two versions.
- **A plain version string in `requires` means "compatible"**: `"1.3"` means "1.3 or later, with the same major version". There is no notation for detailed ranges (`>=` and so on). A dependency copied into the project is exactly the version of that copy (below).
- **A mismatch is an error (`PKG`)**: if a newer minor version than the local `std` is required (reading `"1.5"` with 1.3), it says "Please update Ozen". A different major version is also an error. The `std` version recorded in a bundled file is checked by the same rule.
- **Deprecation steps**: to remove vocabulary from `std`, first give a deprecation warning (`deprecated`) in a minor version, then remove it in the next major version.

**Versions and offline use**

- Packages you depend on are copied into the project (the copy that was brought in is itself the version pin). Nothing is fetched from the network.
- It is an error if two versions of the same package are mixed in one project (this is enough at the scale of one developer).

**Bundling into one file to share (`ozen bundle`)**

- All dependencies except `std` are put into one file. The parts put in stay as inline modules such as `module dashi.pantry.fish { ... }`, keeping their namespaces.
- The bundled file records the `std` version used to make it.
- When a bundled file is built on its own, the modules inside it are used. When it is brought into a project, it is an error if there is another version of the same package.
- A module in a bundled file that calls itself `std` is an error (this prevents swapping out the standard library; §10).
- **Format**: a bundled file can also be read as an ordinary `.ozen` file.

  ```
  // katsuCurry.ozen（ozen bundle で作ったもの）
  bundle {
    std:     "1.3"                       // 作ったときの std の版
    entry:   katsuCurryNight             // 入口
    from:    ourCookbook "0.3.0"         // 束ねた元のパッケージ
    license: "CC-BY-4.0"                 // 元の package.ozen にあれば引き継ぐ
  }

  module ourCookbook.recipes.curry { ... }
  module ourCookbook.pantry.house { ... }
  module dashi.pantry.fish { ... }       // 依存しているパッケージの、使っている部分だけ

  phrases ja """
  ingredient-buri = ブリ
  ...
  """

  menu katsuCurryNight(people: 4) { ... }
  ```

  - **What goes in**: the entry and the definitions and dishes that it brings in with `import` (for dependency packages too, only the parts in use), and the phrases (the Fluent text is put as is into a `phrases` block).
  - **What does not go in**: the kitchen spec, `std` (the receiver's local one is used), and unused definitions.
  - **The order is fixed**: by module name. The same input always gives the same file, so diffs work.
  - **It can be undone**: `ozen unbundle` turns it back into files and folders per module (the reverse of §4.6's "splitting or combining does not change the meaning").
  - **Where it stands**: the package declaration (`package.ozen`) is a manifest written in the language itself, like SPM's `Package.swift`. Making one file for distribution is close to SQLite's amalgamation (but it keeps the module boundaries, so it can be undone).

## 5. Compile pipeline

```
DSL テキスト
 → AST                     構文のみ。編集の正本
 → import 解決
 → 展開・評価              if / for / macro を実行し、純粋な手順の列にする
 → 型検査（展開後）         次元・状態・線形（量と資源）
 → IR                      動作 DAG + 資源要求 + 状態遷移（SSA、平らな構造体配列）
 → スケジュール             資源制約つき。波と出す時刻（--serve-at）に従って解く
 ┌──────────────────────────────── ここまでがコア ────────────────────────────────┐
 → ビュー / レンダラー       HTML・ガント図・買い物リスト・テキスト IR ダンプ・JSON
 → （外部連携）ロボット向け   CBOR / FlatBuffers / 機種別コード（コア外のプラグイン）
```

### 5.1 Check levels (the depth changes with the inputs given)

**The checks go as deep as the inputs given, and what is not given is stated explicitly as an "assumption".** One pipeline covers everything from checking just a recipe to a complete schedule table.

| Level | What is given | What is checked | What is assumed | Output |
|---|---|---|---|---|
| 0: Recipe only | Recipe | Syntax, types for dimensions, states and linearity, how requirements (`requires`) are written | servings is the recipe's default. Unlimited people and equipment | IR and the shortest possible time (critical path) |
| 1: With conditions | + servings, cooks | Re-expanding amounts, competition for hands | Unlimited equipment | A schedule that considers only hands |
| 2: With kitchen | + kitchen spec | Matching requirements against the kitchen, numbers of equipment and vessels, inserting washing up, cleanup | None | Complete schedule table |

```
ozen check recipes.curry                                 # レベル 0（スケジュールを組まず型検査で止める）
ozen build recipes.curry --servings 6 --cooks 2          # レベル 1
ozen build recipes.curry --kitchen myHome --cooks 2     # レベル 2
```

- **The level is decided by the inputs**: the command does not ask you to choose a level; the inputs you pass decide it. Only `check` is separate, as a fast version that stops at the type check, used for checking on every keystroke in an editor (LSP is an external integration).
- **Assumptions are included in the output**: the result has a list such as "No kitchen given, so equipment is assumed unlimited". Ozendate can see this and show "this time is the shortest estimate". Assumptions are returned in the same form as diagnostic "information" (category: assumption) (§6.1).
- **Invariant: adding information only adds constraints and diagnostics (never removes them)**. An error found at level 0 is always found at level 2 too. Tests confirm this.
- **Options that do not fit the level are diagnosed**: for example, `--cleanup` means nothing without knowing the sink and the number of tools, so if it is given without a kitchen, it warns and ignores it. Which option assumes which input is written in the type definition of `Options` (§4.2).
- **The API stays the same function**: passing NULL to `kitchen_utf8` / `options_utf8` of `recipe_build` means "not injected" (§8). The public API does not grow.

### 5.2 Build units and commands

**Build units**

| What you do | Smallest unit | Reason |
|---|---|---|
| Type check (level 0) | Definitions (module) | Unit cycles and state contradictions can be checked with definitions alone. There is no schedule |
| Build (through the schedule) | Dish (recipe) | Automatically wrapped as a one-dish menu (the default vessel and one wave; §4.5) |
| Unit you actually cook | Menu (one meal) | Competition for the kitchen, washing up, vessels and using things up matter at the menu level |

- **When building a dish on its own**: for a recipe that takes ingredients as arguments, the arguments are assumed to be "ingredients from outside (treated as bought)". For a recipe without a default vessel, plating is left out and the schedule goes "until it is finished". Both are shown in the list of assumptions (§5.1).
- **The compile unit is the menu**: dishes are not compiled separately and then joined in the menu (separate compilation). Even the same dish gets a different number of people and arguments, and so a different IR, depending on the menu. The dishes each menu needs are compiled together. If speed becomes necessary, it will be added as a cache of results per set of arguments.

**How to point to the entry**

```
ozen build menus.katsuCurryNight            # モジュールの入口が 1 つならこれで決まる
ozen build menus.dinners.katsuCurryNight    # モジュール内に複数あれば、名前まで続ける
ozen build menus/katsuCurryNight.ozen       # ファイルの道筋も使える
ozen build katsuCurry.ozen --entry katsuCurryNight   # 1 ファイルを単体で扱うとき
```

- It is resolved as a module from the start, and the rest is read as the entry name.
- The form `file:name` is not used (some OSes allow `:` in file names, so it cannot be told apart).
- If there are several entries and none is given, it is an error that lists the candidates.

**Injecting the kitchen and so on**

- `-k home` searches the kitchen declarations in the project by name (putting them in `kitchens/` is a convention, not enforced). A file path is also accepted. If the same name is found more than once, it is an ambiguity error.
- **There is no mechanism for defaults**: the kitchen and the like are injected every time. If left out, the check just runs at the level without a kitchen (§5.1).

**Short forms of options**

| Long form | Short form |
|---|---|
| `--kitchen` | `-k` |
| `--cooks` | `-c` |
| `--servings` | `-s` |
| `--serve-at` | `-t` |

- Options not used every time, such as `--entry`, `--cleanup` and `--prefer`, have only the long form.
- Abbreviating a long form partway (`--kit` for `--kitchen`) is not supported (when options are added later, earlier abbreviations would become ambiguous and break).

**Building a whole project**

```
ozen check                 # プロジェクト全体を検査
ozen build -k home         # プロジェクト全体をビルド
ozen build menus           # 献立フォルダ（モジュール）以下だけ
ozen build recipes -k home # 料理フォルダ以下だけ
ozen build menus.curryNight menus.yakisobaLunch -k home   # 複数を指定
```

- **Targets**: all entries in the given scope. Menus are taken as they are, and dishes are treated as one-dish menus. Modules with only definitions (pantry) are only checked. If no entry is given, the whole project.
- **Each is built independently**: one schedule table comes out per menu, and competition for the kitchen between menus is not considered (a menu is one meal; §4.5).
- **Order**: run one after another in module name order. The order of the results is the same every time, which makes diffs easy.
- **Do not stop on failure**: even if one has an error, the rest continue. At the end, a list of "how many passed out of how many" is shown, and if there is even one error, the exit code is failure (for use in CI).
- **Output**: for each entry, the IR, schedule table and diagnostics are written to a folder that follows the module path.

  ```
  out/
    menus/katsuCurryNight/   schedule.json, ir.txt, diagnostics.json（--format sarif で SARIF も）
    recipes/curry/             ...
    index.json                 全入口の一覧と結果（レンダラーが使う）
  ```

**How responsibilities are split**

- **Core**: only takes one entry and returns one result. It is a pure function that does not read files (§8), so it has no code that walks a project.
- **Driver (CLI, Ozendate)**: walks folders to collect entries, calls the core in order, and writes out the output. Things like the parsed standard library may be reused within one build.
- **Renderer**: reads `index.json` and each entry's result and turns them into a book or pages. "How it looks as a book", such as the table of contents and chapters, is the renderer's job (§7.1).

## 6. Principles of the IR and data representation

> **The core holds only forms that are easy for machines to handle, and forms shown to people are pushed to the edge (outside the core).**

- **The source of truth is the IR as arrays of structs on an arena** (references by integer ID, tagged unions, SSA). JSON and the text IR are only **views** written out on the spot.
- **The AST and the IR are different things**: the AST is the source of truth for editing (close to how the syntax looks), and the IR is the product after checking (normalized, with a dependency graph and resource requirements).
- **Text IR (in the style of LLVM IR)**: for debugging, test expectations and checking diffs. Implementing it first as one of the core's outputs makes it easier to write tests for the type checker and the scheduler. It is not made the source of truth.
  ```
  %o1 = slice  %onion : Onion<shape: peeled> -> Onion<shape: cut(slice, 3 mm)>  [hands, Counter, CuttingSurface 2m]
  %o2 = fry    %o1    : Onion<shape: cut> -> Onion<doneness: done>             [hands, HeatSource<stovetop>, FryingPan 10m]
  ```
- **JSON**: a view for handing data to the UI and other programs. The DAG is a node array + references by integer ID (+ an edge table if needed). The node kind is the `"op"` tag. Quantities are `{value, unit}` (rational numbers are `{num, den}`). It includes `schemaVersion`.
- **For devices (external integration)**: added when work starts on integration with robots and cooking appliances. Until then, only the text IR dump and JSON.
  - The first choice is CBOR (RFC 8949): it can turn the same data shape as JSON directly into binary, with no schema compilation. The same structure as the JSON output can be written out almost as is.
  - FlatBuffers is added only if a device needs it (it has the advantage of no copying when reading, but adds schema management).
  - What to keep now: keep the IR as flat tables that refer to each other by integer IDs (so writing out to any format can be done mechanically). Leave room to add attributes to the IR (§3).
- **Diagnostics**: the core returns only **an error code + span + arguments**, and the Japanese/English text is made in the display layer (this also helps localization).
- **No display information mixed into the IR** (ingredient order, headings and so on belong to the AST / metadata side). Example: serving waves go into the IR (they are scheduling constraints), but wave names go on the metadata side (§4.5).
- **Numbers**: avoid floating point and keep **rational or fixed-point** numbers (so results match across environments).

### 6.1 Diagnostic design (errors, warnings, information)

Diagnostics come from every stage (syntax, type check, matching against the kitchen, scheduling), so the severity criteria are made consistent.

| Severity | Criterion | Example |
|---|---|---|
| Error | As it is, it does not work as cooking, or it is not safe | Missing state (cutting without peeling), not enough of an amount, consuming twice, unit mismatch, using something after raw meat without washing, undercooked raw pork or chicken, the kitchen does not meet the requirements, an amount per person when the number of people is not decided (`servingsUnknown`), an amount of 0 or less (`nonPositiveAmount`), the time for `until:` is unknown (`untilTime`), a state that `until:` cannot advance to (`untilTarget`) |
| Warning | Probably a mistake, but it can go on | "To taste" (normal mode), over a vessel's capacity, adding a soft ingredient before a hard one, more cooks than people who can stand in the kitchen, an option that does not fit the level, an unused value (`unusedValue`), a deadline from making to using that cannot be kept (`useWithinMissed`), serving a soft-boiled egg (`lightlyCookedEgg`; it tells the assumption that fresh eggs safe to eat raw are used) |
| Information | Not a problem, but a useful fact to know | Leftover parts (fish trimmings 300 g), inserted washing, a vessel used instead (one donburi bowl), assumptions (unlimited equipment), the shortest time |

The rule when unsure: **would the cook be in trouble if they went on as is?** If so, it is an error or a warning; if not, it is information.

**Rules**
- **Severity is a default per diagnostic code and can be changed by settings** (the same idea as a compiler's warning flags). Example: in strict mode, "to taste" becomes an error, and on a day aiming at "using everything up", leftover parts are raised to warnings. Settings are passed as build conditions (typed, §4.2) or as annotations in the recipe (`@allow(leftoverPart)` and so on) (see "Severity settings" below).
- **Safety errors cannot be lowered**: errors about food hygiene (`Hazard`) and allergens (category `SAF`) cannot be turned into warnings by settings.
- **If you write the intent, it need not be reported**: if `store(ara)` is written, the rest just appears in the schedule table as "store", and no diagnostic is given.
- **The guarantee of linear types is kept**: leftovers never disappear silently; they are always reported as information, even without a declaration.
- **Monotonicity (§5.1) holds under the same settings**: adding inputs never reduces diagnostics. This does not hold when the severity settings are changed.

**What a diagnostic contains**
- Diagnostic code, severity (the default and the value after settings change it), category (assumption and so on)
- Span and arguments (the text is made in the display layer)
- Related locations (macro expansion history, other related lines)
- Candidate fixes (in a machine-readable form): "add `peel`", "use a donburi bowl" and so on. Used for fix suggestions in editors and for buttons in Ozendate.

**Diagnostic codes: category + number, plus a name**

| Category | What it covers | Example |
|---|---|---|
| `SYN` | Syntax | Matching brackets |
| `NAM` | Name resolution, import | Ambiguous names, import of a `local` |
| `TYP` | Types (dimensions, states) | Unit mismatch, cutting without peeling |
| `LIN` | Linearity (amounts) | Not enough of an amount, consuming twice, leftover parts |
| `SAF` | Safety | Undercooking, using something after raw meat without washing, allergens |
| `KIT` | Matching against the kitchen | No equipment, not enough vessels |
| `SCH` | Schedule | Over a vessel's capacity, more than the number of people who can stand |
| `MAC` | Evaluation, macros | Out of fuel, expansion limit |
| `PKG` | Packages, files | Mixed versions, dependency cycles |
| `ASM` | Assumptions | Equipment assumed unlimited |

- Every diagnostic has a code such as `TYP012` (a category prefix + a 3-digit number, the same form as Flake8 and Ruff) and a name such as `peelMissing`. Both are shown in the output, and settings use the readable name.
- The severity is not part of the code (because the severity changes with settings).
- A code and name once used are never reused, even after being retired.

**Severity settings**

```
ozen build -k home --deny vagueAmount --warn leftoverPart
ozen build -k home --warn LIN          # 分類の接頭辞でまとめて指定
```

```
let ara = ... @allow(leftoverPart)          // この式だけ
recipe tonjiru(...) @warn(staining) { ... }   // このレシピ全体
let ara = ... @expect(leftoverPart)         // 出るはず。出なくなったら警告
```

- **At build time** (`--allow` / `--warn` / `--deny`): changes the defaults for that whole build. Strict mode (`--strict`) is `--deny` on the diagnostic group `strict` (§4.1). Besides names, you can also set several at once by category prefix (`LIN` and so on) or by a named group (`strict` and so on).
- **In code** (`@allow` / `@warn` / `@deny` / `@expect`): changes only the scope of the declaration or expression it is put on.
- **`@expect`**: suppresses the diagnostic, and warns if that diagnostic stops appearing (this prevents an old annotation from staying after the dish was fixed and no trimmings are left any more).
- **Priority**: the narrower scope wins. Code annotations > build-time settings > standard defaults (because the intent at the place it is written is the most specific).
- **`SAF` diagnostics cannot be lowered from anywhere**: even with `@allow`, they stay errors, and an extra piece of information "This diagnostic cannot be lowered" is given (the same idea as Rust's `forbid`).
- **Diagnostics from packages brought in are hidden by default**: only the diagnostics of your own code are shown (the same idea as Cargo passing `--cap-lints allow` to dependencies). However, `SAF` errors are always shown, even from dependencies. Annotations inside a bundled shared file work only inside that module.

**Output format**

- The standard is this language's own `diagnostics.json`. With `--format sarif`, it can also be written as SARIF 2.1.0 (OASIS's exchange format for static analysis results). The contents of a diagnostic (code, severity, span, related locations, candidate fixes) map almost one to one to SARIF's ruleId, level, locations, relatedLocations and fixes (the severity "information" is SARIF's `note`).

## 7. Where the core's responsibilities end

| Item | Where it lives | Reason |
|---|---|---|
| Parsing, AST, expansion and evaluation, type check, scheduling, IR | Core | The body of the language |
| Diagnostics (code + span) | Core | Each UI draws them |
| Text IR dump, JSON output | Core (view) | For tests and integration |
| HTML, Gantt chart, PDF | **Outside the core** | A matter of appearance. Written in JS/TS or similar, and shared by the WebView or similar on every platform |
| Shopping list totals | Either | In the core if it leans toward calculation |
| Code for robots | Outside the core (plugin) | Depends on the machine. Deferred as an external integration |
| Structured data that feeds the text, list of action signatures (JSON) | Core (view) | Input to the renderer and to the consistency check |
| Walking the project to collect entry points, building them in order, and writing the output | **Outside the core** (driver: CLI / Ozendate) | The core is a pure function that reads no files (§5.2) |
| Generating recipe text and the schedule table, phrases (many languages) | **Outside the core** | A matter of appearance and language. Not kept in the code (§7.1) |

### 7.1 Generating text (the renderer) and keeping phrases separate

The recipe text is made by **a later app (the renderer)**. The core knows nothing about phrases. **The code holds only language-independent identifiers, and the phrases go in separate files** (the same split as app localization).

**What the core outputs**
- Structured data that feeds the text (a kind of JSON view): for each step, the action, arguments, display names of ingredients, time needed, resources used, reasons for inserted washing, and so on.
- A list of action signatures (JSON): used to check consistency with the phrases.

**What the renderer holds**
- **Two kinds of text**:
  - Recipe: a step list in dependency order that does not depend on the kitchen. Made from the output of verification level 0 (§5.1).
  - Schedule table: timed steps fitted to the kitchen and the number of people, such as "18:00 Cut the onion (you)". Made from the output of level 2.
- **How steps are grouped**: deciding to merge small actions into one sentence, such as "Peel the onion and slice it thin".
  - By default the renderer infers this from the DAG (work done in a row on the same ingredient [peel → cut] becomes one sentence).
  - Only when the writer wants to be explicit, they add `@group(見出しのキー)`. That range is shown as one group. It is display-only information: it stays in the AST metadata and does not go into the IR (§6).

    ```
    let sliced = slice(peel(onion), 3 mm) @group(onion-prep)
    ```
- **Use serving waves as chapters** (§4.5): in the schedule table, "Serve ..." becomes a heading, and the work in between is grouped under it. Between waves there is a "During the meal" row that lists work that proceeds while left alone. In a recipe without times, the split is "Main" and "Dessert". The split into "Prep" and "Cooking" is made by the renderer from how far the work before the first wave is from the serving time (the source does not have to say it).

```
17:30〜 下ごしらえ
  ・米を洗って浸水（放置 30分）
  ・玉ねぎを切る
18:00〜 調理
  ・カツを揚げる
18:30 メインを出す: カツカレー、サラダ
  食事中: パフェ用の器を冷やす（放置）
19:10 デザートを出す: パフェ（直前に組み立て 5分）
```
- Headings, the order of the ingredient list, and the layout for HTML and print.
- Where it lives: held as a feature of Ozendate, and also callable from the CLI as `ozen render` or similar. Written in JS/TS and shared by the WebView on every platform.

**Phrase files**

```
dashiPack/
  pantry/dashi.ozen # 型と動作の定義（言語に依存しない）
  phrases/
    ja.ftl          # 日本語の言い回し
    en.ftl          # 英語の言い回し
```

```
# phrases/ja.ftl
action-slice = { $x }を{ $t }の厚さに切る
action-simmer = { $x }を{ $time }煮込む
```

- **Format**: Mozilla's Fluent (`.ftl`). Word order, particles, plurals and so on can be absorbed on the translation side, and it has a JS implementation, so it suits the renderer.
- **How keys are named**: a prefix per kind + the name in kebab case. Diagnostic messages also go here (the core returns only the code and the arguments; §6).

  ```
  ingredient-buri = ブリ
  ingredient-buri-kama = ブリカマ
  action-slice = { $x }を{ $t }の厚さに切る
  property-while-warm = 温かいうちに
  alias-karubi = カルビ
  diag-peel-missing = { $x }の皮をむいてから切ってください
  ```

- **Layering**: read in the order `std` → packages depended on (in package-name order) → your own project, and later ones override earlier ones. A household name (such as "our house curry") takes effect if you write it in your own project's `phrases/`.
- **Unit of sharing for extensions (pack)**: when you share your own actions, distribute the definition files (`.ozen` in `pantry/`) together with `phrases/`.
- **Translation-only contributions are possible**: you can add only phrase files without touching the code.
- **Drift check**: provide a tool that compares the list of action signatures with the phrases and warns about "actions without phrases" and "arguments not in the signature".
- **Fallback when there is no phrase**: the user's language → English → a display made mechanically from the identifiers (such as `slice(onion, 3 mm)`), in that order. Even without writing any phrases, the recipe can always be shown.
- **Choose names by property, context and region**: the name is chosen by the growth stage of a fish that changes names as it grows (property), by whether it is for yakiniku or for simmering (the dish context), and by Kanto or Kansai (`$dialect`, a renderer setting).

```
# phrases/ja.ftl
ing-buri = { $stage ->
    [young] { $dialect ->
        [kansai] ハマチ
       *[kanto]  イナダ
    }
    [middle] { $dialect ->
        [kansai] メジロ
       *[kanto]  ワラサ
    }
   *[adult] ブリ
}
```

- **For names with a cut, prefer the proper name**: proper names such as "nakaochi" (meat scraped off the backbone) or "otoro" (fattiest tuna belly) are defined with a key just for that combination (such as `ing-buri-kama`). If there is none, the name is built as "kind + cut" ("pork belly", "yellowtail collar").

## 8. Platform strategy

**One core + thin bridges**. The public API is narrowed to "text in → result out".

| Platform | How it ships | Bridge |
|---|---|---|
| iOS / macOS | Static library / `.xcframework` | Header + module map (called directly from Swift) |
| Android | `.so` per ABI (NDK) | JNI wrapper (a few dozen lines of C) or JNA. **Strings are passed as UTF-8 in a `ByteArray`** (because JNI's modified UTF-8 breaks emoji) |
| Linux | Shared library / CLI | Linked directly |
| Web | `.wasm` | Emscripten + a few dozen lines of JS. Web Worker recommended, offline support with a Service Worker |

**API policy (ownership example)**

```c
// 返された文字列は呼び出し側が recipe_free_string で解放する
// kitchen_utf8 は台所スペック、options_utf8 はビルド時の条件（cooks・servings・cleanup など。§4.2）
// どちらも NULL なら未注入。渡した入力で検証レベルが決まる（§5.1）
char *recipe_build(const char *src_utf8, const char *kitchen_utf8,
                   const char *options_utf8 /* ... */);
void  recipe_free_string(char *s);
```

- Do not make a fine-grained API that handles AST nodes one at a time (the wrappers for each language would grow large).
- Hold no global state; pass a context struct.
- Make it a set of pure functions with no I/O (they run as is in WASM and WebView).
- Make the memory allocation functions replaceable.
- Inside the core, depend on nothing other than the C standard library (no threads, `dlopen`, or platform-specific APIs).

**Notes on iOS limits**: JIT, fork, and running downloaded native binaries are not allowed. This design "only outputs data (IR)", so it does not run into these. Extensions are also shared as source (text). File association is done with one type identifier for `.ozen` (§4.6).

**Notes on working offline**: bundled data (unit conversion, density table, standard library) ships in the bundle. Recipe HTML on the Web is self-contained (CSS/JS inline, no CDN or web fonts). Watch out for files that iCloud or similar has not yet downloaded (put the project inside the sandbox if needed).

## 9. Core implementation language: a C-family language

The motivation is "**to provide an environment that runs cross-platform with the thinnest translation layer**". A C-family language (one that can expose a C ABI and is easy to build with each OS's toolchain) meets this condition directly.

| Aspect | What is wanted |
|---|---|
| Build dependencies | Few (buildable with each OS's toolchain) |
| Language stability | High (one person can keep up) |
| iOS integration | Can be a static library / `.xcframework` |
| Bridge | Can expose a C ABI |
| Existing C libraries | Usable as is |
| Safety | Discipline + ASan/UBSan + fuzzing |

**Weaknesses and countermeasures**: the evaluator and the AST tend to be hard to write (no exhaustiveness check, manual memory management). Make up for this with one arena + index references, the discipline of tagged unions and `switch`, and ASan / UBSan and fuzzing.

**Distribution**: for outside developers, **ship prebuilt artifacts (`.a` / `.so` / `.wasm`) in releases**. Users (people who write recipes) do not need to build anything.

**Escape hatch**: because the public boundary is a C ABI of "text in → result out", **only the core implementation can later be swapped for another language**. The code on each OS does not change.

**What the prototype checks**: build **a small prototype of the evaluator (the level 3 part)** (`if` / `for` / `quote` / fuel) and check the following.
1. Whether the feel of writing it and the number of bugs are acceptable
2. Whether iOS (static library) and WASM give the same results
3. Whether the problems found by fuzzing are within a manageable range

## 10. Safety and quality (preparing for untrusted input)

Shared recipes and extension libraries (via AirDrop or file sharing) are treated as untrusted input.

- Put limits (fuel) on input size, recursion depth, number of expanded nodes, and number of evaluation steps.
- One arena + node references by index (range checks gathered in one place); strings are interned (turned into integer IDs).
- Run ASan / UBSan + fuzzing (focused on the expander and the parser) **always in CI**.
- Put cross builds for the 4 platforms (+ WASM) into CI **from the start** (the later it is done, the more painful it gets).
- Diagnostics are collected in an array and returned at the end (no abort midway; several errors are reported at once).
- Give every node a span (position information) from the start. Expansion history is carried along too.
- `std` is a reserved name. A shared file or package cannot call itself `std`, or silently swap out an installed package (§4.7).

## 11. Relation to existing work (analysis of novelty)

> A survey as of October 2026. **Looked at part by part, almost everything has prior examples**. In particular, scheduling several dishes with the kitchen's equipment and number of people in mind (including washing up) has been studied in Japanese research for more than 10 years. What is new in this language is not the parts but **the combination: putting them into a statically type-checked language and allowing step-by-step verification by injecting the kitchen and conditions**.

### 11.1 Comparison with existing work

| Category | Examples | Where it overlaps | How this language differs |
|---|---|---|---|
| Recipe markup | Cooklang | Describing ingredients, equipment and time, scaling by servings, shopping list, recipe references, menus (`.menu`), LSP | Annotations on text. No type check, resources or scheduling (an idea for a type checker is mentioned on the official blog) |
| "Finish at the same time" apps | Chef's Multi-Dish Timer, Cooking Time Planner, MealTime, and many more | Working back from the finish time, sharing the oven | Do not handle competition for hands, burners and the sink, or the internal structure of recipes |
| Cooking scheduling research | Matsushima, Funabiki et al. (2011–2015), Nakabe et al. (NAIST, 2021), Yamabuki et al., Kayahara et al. (2017) | A kitchen model of cooks, pots, cutting boards, burners and the sink; cooking steps that include "wash"; tool state transitions; taking washing into account; formulation as RCPSP; dynamic rescheduling; finishing temperature | The kitchen is input data for an algorithm. No language, type check, or user extension |
| Formal representation and robots | MILK (2008), FOON, action graph DSL (2025), Cook2LTL | Typed predicates, graphs of objects and actions, state, representing the environment and concurrency | The aims are extracting structure from text, ontologies, and robot planning. A static-checking compiler for home use is not the subject (for the 2025 DSL only the abstract was checked; the full text still needs checking) |
| Guided cooking appliances | Thermomix (Cookidoo) | Treats recipes as step data a machine can run (overlaps with the future picture of an IR for robots) | Linear steps for one device. A closed ecosystem |
| Others | schema.org/Recipe, Chef (esoteric language), Bwyd (cooking-notes DSL), Corel (graduation project on a cooking DSL) | Structuring, the form of a DSL | No type check, resources or scheduling, or small in scale |

### 11.2 Points that seem novel

**The biggest strength: writers can define and extend types and steps, vocabulary and all.** The type system, injecting the kitchen and so on are positioned as things built on top of this extensible mechanism.

1. **Writers can grow the vocabulary itself**: every prior example has a fixed vocabulary (in the research the kinds of cooking steps are decided in advance; FOON and MILK annotate with a vocabulary chosen by the researchers; Thermomix can only be written within the functions the maker provides; Cooklang also has a fixed notation and leaves meaning to each tool). This language writes its standard library in the same language, so there is no privileged built-in vocabulary. An `ingredient` or `action` defined by a writer is type-checked the same way as the standard ones.
   - Writers can add Japanese dashi and fermentation, regional units (the Japanese `cup`), appliances only their home has, and even new kinds of dirt and dishes.
   - Definitions can be shared as source (passed on like sharing food with neighbors), and knowledge spreads from person to person.
   - Safety devices for extension are already designed: checks at definition time (contradictory state transitions, circular units, unreachable states), compile-time evaluation that always stops (fuel), and diagnostics with expansion history (§4.3, §10).
2. **A combination of static type systems for cooking**: the compiler checks dimension types, state types and linear types, and makes missing steps, too little quantity and double use errors. As far as we looked, this combination does not exist elsewhere. The same checks apply to extended vocabulary.
3. **Separating requirements, equipment and today's conditions, and matching them by type**: in research, the kitchen was input data for an algorithm. A language design where a recipe declares its needs with `requires` and the kitchen spec is matched by type is new (§4.2). Writers can also extend the types of kitchen specs and conditions.
4. **Verification levels and stating assumptions**: verification gets deeper with the amount of input, and diagnostics never decrease (monotonicity). This brings the thinking of compilers and build systems into cooking (§5.1).
5. **A single core that runs offline and deterministically**: an engineering difference rather than novelty. A strength research prototypes do not have (§8).

**Positioning (a phrasing that does not exaggerate)**: the results of cooking scheduling research turned into a typed language and compiler whose vocabulary writers can extend.

### 11.3 Cautions

- **Do not claim scheduling itself as novel**. The models of earlier research (classification of cooking steps, handling of washing, tool state transitions) are used as a starting point for the action signature table in §13.
- **Term collision**: Cooklang uses `pantry.conf` to mean "the stock of ingredients currently in the kitchen". This differs from this language's "pantry" (definitions and libraries). The file extension is only `.ozen`, and `pantry` is just a conventional folder name, so there is no collision in file names (§4.6). Take care not to mix them up when explaining terms.
- **Balancing the effort of writing against the value**: Cooklang's strength is the ease of "adding marks little by little to ordinary text". The first samples need to show clearly that the value (checks and scheduling) is worth the effort of writing with types.
- **Vocabulary split**: if everyone defines their own `onion`, recipes can no longer be mixed. Grow the standard library as a common base, and recommend that custom definitions extend it (add traits, override properties). This is supported by the design of namespaces and `import` (§4.3).
- **No integration with Cooklang**: no importer is made. Cooklang lacks the strictness of state, equipment and quantity, so a conversion would only give a skeleton, and rewriting would take as much effort as writing from scratch. Keeping up with its spec would also be a large burden, and the intended users (§2) can write in this language from the start. Reconsider if demand clearly appears (§12 non-goals).

### 11.4 References (including some that need rechecking)

- Cooklang: https://cooklang.org/ , spec https://cooklang.org/docs/spec/ , conventions (`.menu`, pantry) https://cooklang.org/docs/conventions/
- Matsushima, Funabiki, Nakanishi, "A Proposal of Cooking Model and Cooking Step Scheduling Algorithm for Multiple Dishes", IMECS 2011
- Nakabe, Mizumoto, Suwa, Yasumoto, "Optimal Cooking Procedure Presentation System for Multiple Recipes and Investigating Its Effect", Algorithms 14(2), 2021 https://www.mdpi.com/1999-4893/14/2/67
- Kayahara, Kuwabara, Kunieda, "Dynamic Scheduling Method of Cooking Process Considering The Finishing Temperature of Dishes", IEICE Tech. Rep. MVE2017-21
- Tasse, Smith, MILK / CURD corpus (2008)
- Paulius, Sun et al., FOON (Functional Object-Oriented Network)
- Kumbhakern et al., "Towards an Action-Centric Ontology for Cooking Procedures Using Temporal Graphs", arXiv:2509.04159 (2025)
- Thermomix Guided Cooking (Cookidoo)
- Example heat transfer coefficients for cooking by searing: heat conduction analysis of how to cook steaks and patties, arXiv:2206.13900 (heated surface 900, air side 60 W/m²℃)
- Basis for the heating time model: Alan Davidson's (1980) rule for cooking time (square of the thickness); "Boiling, steaming or rinsing? (physics of the Chinese cuisine)" arXiv:1806.09912 (time to heat a slab ∝ L², discussion of estimation errors); "Kinetic analysis of the softening of daikon radish and the degradation of pectin by heating" (Hiroshima University Institutional Repository https://ir.lib.hiroshima-u.ac.jp/00030155 . Softening follows a first-order rate equation. Authors and journal need checking)
- References for the design of diagnostics: rustc lint levels (allow / warn / deny / forbid / expect, `--cap-lints`) https://doc.rust-lang.org/rustc/lints/levels.html , Ruff's rule selection and suppression (selection by prefix, unused-noqa) https://docs.astral.sh/ruff/linter/ , SARIF 2.1.0 https://docs.oasis-open.org/sarif/sarif/v2.1.0/sarif-v2.1.0.html
- Sources for yield: MEXT, "Standard Tables of Food Composition in Japan (Eighth Revised Edition), Supplement 2023" (refuse rate and refuse parts, weight change rate from cooking); "Comparison of measured food refuse rates in food service practice with the values of the Standard Tables of Food Composition in Japan (Report 1)" (measured values can be higher than the table values)

## 12. Scope

Not split into stages. The whole of the language, the processor and the app outputs is in scope; only external integrations and the block editor are deferred. The order of building is set by the roadmap in §13.

### In scope
- Parser (hand-written recursive descent, brace syntax), AST, a span on every node
- Extension: levels 1–3 (declarations, composition, compile-time evaluation and macros `macro` / `quote`)
- Type check (dimension, state, linear, resource conflicts)
- Scheduler with resource constraints (waves and serving times, fine-grained priority settings; §4.2)
- Kitchen spec (places and tools) and build-time conditions (cooks, servings, cleanup, per-person properties)
- Type check of the kitchen spec and conditions, and matching against the recipe's needs (`requires`)
- Verification levels (0: recipe only / 1: with conditions / 2: with kitchen) and output of the list of assumptions
- Washing up: state of tools and places, classification of dirt, compatibility rules, inserting washing and wiping, competition for the sink, cleanup settings, dishwasher, check for allergen cross-contact
- Plating and dishes: classification and count of dishes, required constraints and the fallback order of dishes, count check, `perPerson` / `shared`, dividing quantities
- Meat and fish: cuts (type arguments, inheritance, traits), store names (`alias`), multiple outputs from the butchering action, tracking that everything is used up with `store` / `discard`
- Menu (one meal): the menu as the top-level unit, typed outputs of recipes, composition, assembly, side by side, choice, sharing, swapping between making and buying, serving-timing properties (`@serveWithin` and so on), hand-over across menus and use-up per menu
- Serving waves: structure (notation, IR, JSON) and scheduling several waves (intervals, how work during the meal ties up hands)
- Units of files: modules and `import`, dependency direction and cycle detection, forward references, choosing the entry point, being able to write everything in one file, the `.ozen` extension
- Units of building and commands: building a single dish, compiling per menu, naming the entry point by module name, short forms of options, building and output per project (§5.2)
- Packages and import: `package.ozen`, how module names are decided, `std.prelude`, importing single items and aliases, errors for ambiguous names, detecting mixed versions, `ozen bundle` (inline modules)
- IR, text IR dump, JSON output, diagnostics (code + name + span, three levels of error, warning and info, severity settings and `@expect`, suggested fixes, SARIF output; §6.1)
- Standard library (shipped as language source). Its range is set by the reference menus (§12.1)
- Renderer (outside the core): text (recipe and schedule table), HTML, Gantt chart, phrase files (`phrases/`), JSON list of action signatures and the consistency check, choosing names by property, context and region
- Generating settings screens from type definitions (Ozendate; outside the core)
- Builds for the 4 platforms

### Deferred (external integrations and the block editor)
- Everything for robots (tolerances, sensor conditions `until`, safety information, capability matching), formats for devices (CBOR / FlatBuffers, etc.)
- Deep OS integration (File Provider, etc.)
- Editor integration such as LSP and syntax highlighting (tree-sitter grammar)
- Block editor (it can be added later because the AST is the source of truth; deferred because the intended users have some programming background)

### 12.1 Reference menus

The menus that set the range of the standard library and serve as the reference for testing the whole. Together they include at least one of each kind of menu link (§4.5) and of serving waves.

| Menu | Patterns included |
|---|---|
| A. Yakisoba | Single item (a menu of one dish) |
| B. Curry rice | Composition (the minimal sample "curry and rice" in §13) |
| C. Stir-fried pork and cabbage + cabbage miso soup + rice | Side by side, sharing (cabbage used in both, with the cutting done together), cutting board dirtied by raw meat, soup and its bowl |
| D. Curry (each person chooses rice or store-bought naan) + salad, with a parfait after the meal | Choice, assembly, make or buy, two waves (main and dessert), cold dishes |
| E. Mixed juice (banana, strawberry, milk) | Drink, competition for and washing of an appliance (blender) (`Sticky`), served freshly made (`@serveWithin` because it separates and discolors), ice (must be frozen), milk allergen |

### 12.2 Range of the standard library

Worked back from the 5 reference menus (§12.1).

- **Actions (about 25)**
  - Prep: wash rice, soak, wash (vegetables), peel, loosen (noodles), drain, crack the egg (`crack`), beat the egg (`beat`). Peeling the shell off a boiled egg is `peel`
  - Cutting: cut (the cut style is an argument: thin slices, cubes, rolling cut, rough chop, julienne, bite-size), tear
  - Heating: cook rice, stir-fry, simmer, boil, boil water, dissolve in (roux, miso), warm (naan), deep-fry (`deepFry`; it takes the oil as an ingredient, and preheating the oil in the pot to temperature goes into the schedule)
  - Mixing: mix, dress, whip, season, blend in a blender (`blend`)
  - Waiting: chill, let stand
  - Finishing: serve onto dishes, layer (assembly)
  - Cleanup and leftovers: wash tools and dishes (`clean`), wipe (`wipe`), `store`, `discard`
  - Naming: washing an ingredient is `wash` ("washed" is added to the prep axis). Whether rice is "polished" or "washed" in the text is chosen by the phrase file from the ingredient's trait (grain) (§7.1)
  - Heat level (low, medium, high) is not a unit but an argument (an enum) of the heating actions
- **Ingredients (about 30)**
  - Staples: rice, steamed Chinese noodles, naan (store-bought)
  - Meat: pork (komagire and belly are expressed with cuts and store names)
  - Eggs: egg (bought in the shell; cracked, beaten, or boiled and peeled)
  - Vegetables: onion, carrot, potato, cabbage, bean sprouts, lettuce, tomato, cucumber
  - Soup: miso, dashi, tofu, wakame
  - Seasonings: salt, pepper, oil, curry roux, sauce, dressing
  - Dessert: vanilla ice cream (store-bought), fresh cream, sugar, strawberry, banana, cornflakes, chocolate sauce
  - Drinks: milk, ice
- **Units**
  - Quantity: g, kg, ml, L, tablespoon (15 ml), teaspoon (5 ml), go (180 ml). The cup is split into `std.units.jp` (200 ml) and `std.units.us` (240 ml)
  - Length, time, temperature: mm, cm, seconds, minutes, hours, ℃
  - Count: one dimension. Counter words are added by the phrase file (§4.1)

### 12.3 Standard traits and properties

- **Traits (classification)**
  - Kind of food: vegetable, root vegetable, leafy vegetable, meat, fish, grain, noodles, dairy, fruit, seasoning
  - Handling: can be peeled (`Peelable`), skin can be eaten (`SkinEdible`), can be eaten raw (`RawEdible`), needs hygiene care (`Hazard`)
  - Storage: must be refrigerated, must be frozen (this affects the schedule, e.g. ice cream stays in the freezer until just before serving)
  - Allergens: a trait that takes the kind as an argument (such as `Allergen(卵)`). The standard kinds start from the items Japan requires on labels
- **Where the standard library's data comes from (priority order)**: values such as heating time, yield and density get their source in the following order. Measuring ourselves is not assumed (there is no setup for it).
  1. **Primary sources**: official information from makers of appliances and cookware (manuals, technical documents, etc.), documents from public bodies (the "Standard Tables of Food Composition in Japan", etc.), and data from research and papers
  2. **Calculation**: when there is no primary source, numerical heat conduction calculations using parameters with a source (thermal properties of ingredients, surface heat transfer coefficients per cooking method, etc.). The calculation conditions and sources are recorded with the value
  3. **Provisional**: a tentative value when neither exists (with a wide range)
  - Web calculators and summary articles with unclear sources are not used.
  - Each value records its source (primary, calculation or provisional), and a schedule that uses a "provisional" value is listed in the list of assumptions (§5.1). Calculated and provisional values are replaced as soon as a primary source is found.
- **Standard items of properties (`props`)**
  - Quantity conversion: density, approximate weight per piece
  - Heating guide: per cooking method, ranges for "time to heat through to the center (at a reference thickness)" and "time that does not depend on size" (the heating time model in §4.1)
  - Qualities: hardness (enum), fat content (per cut)
  - Yield: the share lost to peeling and so on (a range). The eggshell is also held as a discarded part, with the refuse rate (14 %) of `12004 鶏卵 全卵 生` in the same table. The central value comes from the refuse rate in the "Standard Tables of Food Composition in Japan", and the weight change from heating comes from that table's weight change rate. Measured values can be higher than the table, so ranges are used. The share of each part when butchering a fish cannot be known at the table's level of detail, so it is decided by the source priority in §12.3 (primary sources such as papers → calculation → provisional) or by the writer's input
  - Dirt: the kind of dirt left on tools when cutting or touching (`leaves`)
- **Kinds and classes of dirt**

  | Dirt | Class | Main sources |
  |---|---|---|
  | Raw meat | `Hazard` | Cutting or touching pork, beef, chicken |
  | Raw fish | `Hazard`, `Smelly` | Butchering or cutting fish |
  | Raw egg | `Hazard` | Cracking or beating eggs |
  | Mud | `Soiled` | Root vegetables before washing (potato, carrot) |
  | Oil | `Greasy` | Stir-frying, deep-frying, fresh cream |
  | Starch and sugar | `Sticky` | Rice, potato, noodles, chocolate sauce |
  | Smell | `Smelly` | Onion, garlic, curry |
  | Color transfer | `Staining` | Curry, tomato |
  | Scorching | `Burnt` | Food burnt onto pots or frying pans |
  | Light dirt | `Light` | Mild-smelling vegetables, water |
  | Allergen | `Allergen(種類)` | Tools that handled egg, milk, wheat, etc. (carried over from the ingredient's trait) |

- **Ways of washing**: nothing < rinse < wash with detergent < soak and then wash (an ordered enum)
- **Compatibility rules (what the tool is used for next decides how it must be washed)**
  - After `Hazard`: wash with detergent whatever it is used for next. But if the same ingredient in the same state is handled again in a row (cutting raw pork again), it can be used as is
  - After `Allergen(k)`: wash with detergent before using it for the portion of a person with that allergy. No condition otherwise
  - After `Soiled`: wash before using it for something eaten raw (`RawEdible`). Rinse if the next thing is cooked
  - After `Smelly`: rinse before using it for something eaten raw or for dessert. It can be used as is for a cooked dish (carrot after onion, for example)
  - After `Greasy`: wash cutting boards and dishes with detergent. A frying pan used again for stir-frying can be used as is
  - After `Sticky` or `Burnt`: soak and then wash
  - After `Staining`: does not prevent use. Only reported as information (color stays on a plastic cutting board, for example)
  - After `Light`: can be used as is
  - The `Hazard` and `Allergen` rules cannot be relaxed by settings (§6.1). The others are standard defaults and can be overridden in the pantry

- **Standard capabilities (mapping to equipment; §4.2)**

  | Capability | Example equipment that provides it |
  |---|---|
  | `HeatSource<stovetop / oven / microwave / toaster>` | Stove, oven, microwave, toaster |
  | `CuttingSurface` | Cutting board |
  | `Container<capacity>` | Pot, bowl |
  | `RiceCooking` | Rice cooker |
  | `Blending` | Blender |
  | `Chilling` / `Freezing` | Refrigerator / freezer |
  | `Washing` | Sink, dishwasher |
  | `Vessel` (class of dish) | Dish |

- **Vocabulary of the kitchen spec (places and tools)**

  | Group | Item | How capacity is expressed |
  |---|---|---|
  | Place | Standing space (`floor`) | Number of people who can stand at once |
  | Place | Counter (`counter`) | Number of cutting boards that can be laid out |
  | Place | Sink (`sink`) | Count |
  | Place | Stove (`stove`; gas / IH) | Number of burners |
  | Place | Oven, microwave, toaster | Number of units |
  | Place | Refrigerator, freezer | Default is "room for everything" |
  | Place | Dishwasher | Amount that fits at once |
  | Tool | Cutting board, knife, peeler | Count |
  | Tool | Frying pan, pot, bowl, colander | Count and size |
  | Tool | Cooking chopsticks, ladle, whisk, measuring spoons | Count |
  | Tool | Rice cooker (wash the inner pot), blender (wash the jar) | Count and capacity |
  | Tool | Dishes (see "Classes of dishes" below) | Count and size |

  - The boundary: a place is where you go to work and is wiped on the spot. A tool is used and then washed at the sink (§4.2)
  - Defaults for small tools: when cooking chopsticks or bowls are left out, "there are enough" is assumed and shown in the list of assumptions (§5.1). Where the count matters, such as the allergen check in T2, it is checked if written
  - The refrigerator and freezer are where chilling and keeping things frozen while waiting (ice cream, ice) happen. Capacity is unlimited by default, and competition is checked only when it is written
- **Vocabulary of cuts**

  | Ingredient | Cuts | Used in |
  |---|---|---|
  | Pork | Shoulder, shoulder loin, loin, tenderloin, belly, leg | Stir-fry, yakisoba, curry |
  | Chicken | Thigh, breast, tenderloin, wing tip, drumette | Oyakodon (T2) |
  | Beef | Shoulder, chuck roll, rib loin, sirloin, tenderloin, belly, round | Beef kalbi bowl (T4) |
  | Whole fish (yellowtail, etc.) | Head, collar, back meat, belly meat, backbone, skin, innards, scales | Butchering and using it up (T1) |

  - Store names are expressed with `alias`: "komagire" is not a cut but "small pieces from any cut", so it is an alias that combines a union of cuts with a shape. "Kalbi" is an alias centered on beef belly (T4)
  - Fish share structure through types: the cuts of a whole fish are defined once in the `RoundFish` trait, and yellowtail, horse mackerel, mackerel and so on have it. Innards and scales are discarded parts and are treated as `discard` in the output of the butchering action
- **Classes of dishes**

  | Dish | Class | Dishes served in it (menus, tests) |
  |---|---|---|
  | Flat plate | `FlatPlate` | Yakisoba, stir-fry, naan |
  | Long plate | `LongPlate : FlatPlate` | Grilled collar (T1) |
  | Deep plate | `DeepPlate`, `HoldsLiquid` | Curry |
  | Donburi bowl | `Donburi`, `HoldsLiquid` | Oyakodon (T2), beef kalbi bowl (T4) |
  | Rice bowl | `RiceBowl` | Rice |
  | Soup bowl | `SoupBowl`, `HoldsLiquid` | Miso soup |
  | Bowl | `Bowl`, `HoldsLiquid` | Simmered fish trimmings (T1), salad |
  | Small bowl | `SmallBowl`, `HoldsLiquid` | Small side dishes |
  | Small plate | `SmallPlate` | Soy sauce (served with sashimi), plates for sharing out |
  | Platter | `Platter` | Dishes shared out (`shared`) |
  | Glass | `Glass`, `HoldsLiquid` | Parfait, juice |

  - Size is not a class but a property (`diameter`, `capacity`). The capacity check (§4.5) also uses these properties
  - Special dishes such as a sashimi plate can be added by writers through inheritance, like the long plate
  - Whether a dish can go in a microwave or oven is not in the standard; it is a property writers can add
  - Standard fallback order of dishes: deep plate → donburi bowl → bowl (curry, etc.), rice bowl → donburi bowl, flat plate → larger flat plate → platter, small bowl → small plate (only for things without liquid). Holding liquid (`HoldsLiquid`) is a required constraint, so soup never ends up on a small plate

### 12.4 Test recipes per feature

The reference menus cover the menu links, but they do not include the following features. These are tried with test recipes per feature, separate from the menus.

| Test | Content | Features tried |
|---|---|---|
| T1. Butchering a fish and using it up | Butcher one whole yellowtail and make a menu of sashimi, grilled collar and simmered trimmings | Cuts, yield, multiple outputs from the butchering action, hand-over across menus, showing information about leftovers, the exception for ingredients eaten raw, cutting board dirtied by raw fish |
| T2. Allergens and per-person properties | A menu for 4 people including one with an egg allergy. Oyakodon (for that one person, a chicken bowl without egg) and miso soup | Check for cross-contact through the same bowl or cooking chopsticks, per-person differences |
| T3. Different kitchens | Run reference menus B and C through (a) a kitchen without a rice cooker and (b) a kitchen with a dishwasher and `--cleanup afterMeal` set | Fallback when equipment is missing (cooking rice in a pot), dishwasher, cleanup after the meal |
| T4. Store names | Beef kalbi bowl made with kalbi (`alias`) | Union types where the cut is not fixed, needs expressed with a trait (`Fatty`) |

### Non-goals
- C compilation or machine code generation on the device
- Features that require a network (dynamic download of packages, etc.)
- Becoming a general-purpose programming language (the aim is only describing cooking steps)
- Drying (drying time, capacity of the dish rack, drying with a towel). Washed tools are treated as ready to use at once
- Modeling positions, paths of movement and travel distances inside the kitchen
- Scheduling meal plans for many days, nested courses, pacing per plate, scheduling dishes cooked continuously at the table (yakiniku, hot pot, etc.)
- Warming dishes, the look of plating, matching the formality of dishes and food
- Integration with Cooklang (importer, export). Reconsider if demand clearly appears (§11.3)

## 13. Development roadmap (draft)

1. **Action signature table** (the actions needed for the reference menus; §12.1) and type definitions for quantity, state and resources. Also include definitions of tools, places, dirt, dishes and cuts. Use the models of cooking scheduling research (§11.3) as a starting point. Property values are collected by the source priority in §12.3 (primary source > calculation > provisional). This does not depend on the choice of language, so it can be written first.
2. **Prototype of the evaluator** (C-family language): `if` / `for` / `quote` / fuel. A prototype to decide how to implement it, not to cut the scope.
3. **Review the prototype results** (the check items in §9).
4. Parser + arena + diagnostics foundation.
5. Type check (dimension → state → linear and resource conflicts).
6. IR + scheduler (including inserting washing and wiping) + text IR dump.
7. Public API and header, JSON output.
8. Builds and call checks for the 4 platforms (iOS / Android / Linux / WASM) (setting up CI).
9. Make the minimal sample "curry and rice" (the smallest example of menu "composition") give the same result on every platform.
10. Pass the 5 reference menus (§12.1) and the 4 test recipes per feature (§12.4) on every platform.
11. Renderer (text, HTML, Gantt chart, phrase files) and generating settings screens from type definitions.
12. `ozen bundle`.
13. External integrations (deferred; §12).

## 14. Open questions

Sorted by when they must be decided (before which stage of the roadmap in §13). The higher an item, the sooner it must be decided.

### 14.1 Before the action signature table (§13-1): standard vocabulary

- (All decided. The decisions are in §4.1 and §12.1–§12.4)

### 14.2 Around the evaluator prototype (§13-2, 3): implementation foundation

- The concrete implementation language of the core (within the C family; checked by the prototype in §9. It cannot be decided in conversation, so it waits for the prototype)

### 14.3 Before the parser (§13-4): notation

- (All decided)

### 14.4 Before the type check (§13-5): checking rules

- (All decided)

### 14.5 Before the scheduler (§13-6): scheduling rules

- (All decided)

### 14.6 Before publishing and distribution (§13-7, 8)

- (All decided)

### 14.7 Before the renderer and the bundling mechanism (§13-11, 12)

- (All decided)

### 14.8 External integrations (deferred)

- (All decided. Formats for devices are in §6, and integration with Cooklang is in the §12 non-goals)

## 15. Risks and countermeasures

| Risk | Countermeasure |
|---|---|
| The level 3 evaluator becomes a breeding ground for bugs in a C-family language | Build a small prototype first. Write it with an arena and discipline, and run ASan / UBSan and fuzzing from the start. If needed, swap only the core for another language (the boundary is the C ABI) |
| Macro expansion makes errors hard to understand | Build in the design that gives spans an expansion history from the start |
| State types are too fine-grained and make writing unpleasant | Make state a combination of independent axes to avoid a combinatorial explosion. Treat a mixture as a new value and do not track the state of each ingredient in it (§14.1) |
| Floating point gives different results on different platforms | Rationals / fixed point |
| Shared recipes or extensions become malicious input | Fuel, limits, sanitizing, fuzzing |
| Mistaken novelty (overlap with research and existing work) | Following the analysis in §11, present the combination as a language, not scheduling itself, as the novelty. Check the full text of close research such as the 2025 action graph DSL |
| Vocabulary split (everyone defines their own, and recipes can no longer be mixed) | Grow the standard library as a common base. Recommend writing custom definitions as extensions of the standard. Manage collisions with namespaces, `import`, and overrides per scope |
| Inserting washing up makes the scheduler complex | Treat it as setup (changeover) time. Design the mechanism and the vocabulary together from the start, to avoid rebuilding when it is added later |
