# Prior work and Ozen

Correspondence between the elements of the cooking-procedure models in four papers
(`prior-work-elements.yaml`, with page numbers) and Ozen. Each row gives the Ozen
counterpart and its basis (a section of `docs/concept.md`, a decision D in the plan, or a
spec item S in the plan), or "No counterpart" with the reason. The papers are not in the
repository; the YAML records where they were retrieved and their SHA-256.

## Nakabe et al. (2021)

| Element | Ozen | Basis |
| --- | --- | --- |
| `nakabe2021-task-graph` | The IR is a DAG of steps whose edges are data dependencies; durations come from action times | concept §4.2, §6 |
| `nakabe2021-cooking-task` | One action call is one step; the granularity is chosen by the recipe author | concept §4.1, §4.2 |
| `nakabe2021-quantity` | Quantities are dimension-typed values and change the IR and resource needs (servings, container capacity) | concept §4.1, §4.2; S74, S76 |
| `nakabe2021-required-resource` | `uses` of an action: hands, places, and tools requested by capability | concept §4.2; S55 |
| `nakabe2021-holding-resource` | No counterpart: Ozen does not reserve a tool after a step to hold its product; a candidate to add (holding resources). | — |
| `nakabe2021-cooking-time` | Action and attribute times are ranges in the standard library; scheduling uses the upper bound | concept §4.1, §12.3; D27 |
| `nakabe2021-preparing-time` | No counterpart: Actions have one duration; a separate preparation time before a step is not modeled. A candidate to add. | — |
| `nakabe2021-cooking-holding-type` | Dirt kinds left on tools and places, classified by traits; compatibility rules decide the washing level | concept §4.2, §12.3; S139 |
| `nakabe2021-starting-requirement` | Data dependencies between values are the precedence constraints (an implicit join) | concept §4.2 |
| `nakabe2021-holding-requirement` | No counterpart: Follows from holding resources, which Ozen does not have. | — |
| `nakabe2021-previous-task` | No counterpart: Ozen has no constraint that one step must start exactly when another ends; a candidate to add (no-gap continuation). | — |
| `nakabe2021-resource-vector` | The kitchen spec gives the count of each place and tool, and the build options give the number of cooks | concept §4.2 |
| `nakabe2021-cooking-procedure` | The schedule, including inserted washing and the cleanup chosen by `--cleanup` | concept §4.2 |
| `nakabe2021-washing-task` | Washing is inserted during assignment when no clean compatible tool is free, with a reason attached | concept §4.2; D26 |
| `nakabe2021-washing-as-overhead` | Washing is treated as sequence-dependent setup time decided at scheduling time, not in the IR | concept §4.2 |
| `nakabe2021-cleanup-task` | `--cleanup none | beforeMeal | afterMeal` defines the final state of the kitchen | concept §4.2 |
| `nakabe2021-objective` | Objective order: serving windows and waves, then total time (or latest start with `--serve-at`), then `--prefer` | concept §4.2; D26 |
| `nakabe2021-gap-time` | Waiting versus washing is chosen during assignment by the earliest finish; idle time of a cook is a `--prefer lessWaiting` criterion | concept §4.2; D26 |
| `nakabe2021-pdf-ihs` | No counterpart: Exact optimization is deliberately left out of the core for predictable time and determinism (concept §4.2). | — |
| `nakabe2021-allocation-resources` | Ties are broken by candidate order, then by not inserting a wash, then by resource declaration order | D26 |
| `nakabe2021-preprocessing` | Priority is the longest path to serving, counted with upper-bound durations | D26 |
| `nakabe2021-bounding` | No counterpart: Part of branch-and-bound search, which Ozen does not use (list scheduling with improvement, D26). | — |
| `nakabe2021-warm-dish-completion` | Serving waves and `@serveWithin` / `@keepWarm` constrain when dishes finish relative to serving | concept §4.5 |

## Tasse and Smith (2008)

| Element | Ozen | Basis |
| --- | --- | --- |
| `tasse2008-ingredient` | Ingredients are linear values: created by declarations and steps, consumed by actions and plating | concept §4.1 |
| `tasse2008-tool` | Tools are kitchen spec entries typed by kinds that provide capabilities; they are not values in the recipe | concept §4.2 |
| `tasse2008-temporal-order` | Statements run in written order but the schedule follows data dependencies, not the text order | concept §4.2, §4.4 |
| `tasse2008-state-tuple` | A value's state is a combination of independent axes (doneness, browning, shape, prep) | concept §4.1; S30 |
| `tasse2008-final-state` | Values not consumed are reported as `LIN:leftover` information; the kitchen end state is set by `--cleanup` | S44; concept §4.2 |
| `tasse2008-create` | The `ingredients` block declares ingredients with types and quantities | concept §4.4 |
| `tasse2008-quantity-in-description` | Quantities are typed literals with units and ranges, not text | concept §4.1, §4.4; S8 |
| `tasse2008-combine` | `combine` is a structural action that makes a `Mixture`; a list argument is sugar for it | D11; S45, S46 |
| `tasse2008-separate` | Butchering actions have several outputs, and the yields are attributes | concept §4.1 |
| `tasse2008-manner` | How an action is done is given by typed arguments such as `style` and `heat` (enums) | D9; S39 |
| `tasse2008-put` | No counterpart: Ozen does not track which container holds which food; containers are requested through `uses`. A candidate to consider. | — |
| `tasse2008-remove-error` | No counterpart: Follows from containment tracking, which Ozen does not have. | — |
| `tasse2008-cut` | `cut(x, style, size)` with the refinement relation between cut styles | D9; S35 |
| `tasse2008-mix` | `mix` (hands and bowl) is distinct from the structural `combine` | D11 |
| `tasse2008-cook` | Heating actions (`fry`, `simmer`, `boil`, ...) advance `doneness` and `browning` as declared effects | concept §4.1, §12.2; S34 |
| `tasse2008-do` | No counterpart: Every action must be declared with a signature; there is no untyped generic action (concept §4.3). | — |
| `tasse2008-serve` | Plating in the menu's `serve` consumes the values and assigns vessels | concept §4.5; S43, S52 |
| `tasse2008-set` | No counterpart: Appliance settings are not modeled; only kitchen requirements such as `Oven<maxTemp >= 250 °C>` are (concept §4.2). | — |
| `tasse2008-leave` | `rest` and `chill` are actions marked `@unattended` that free the hands | concept §4.2, §12.2 |
| `tasse2008-chefcheck` | No counterpart: Sensor and observation conditions (`until`) are external integration, deferred by concept §12. | — |
| `tasse2008-curd-line` | No counterpart: An annotated corpus, not part of a recipe model. | — |

## Paulius et al. (FOON)

| Element | Ozen | Basis |
| --- | --- | --- |
| `foon-bipartite-network` | The IR alternates steps (actions) and values (states of ingredients) | concept §6 |
| `foon-object-node` | A value is an ingredient type with a state on each axis and traits | concept §4.1 |
| `foon-state-change` | Actions declare state effects on ingredients; tools and places have clean or dirty states and dirt left by foods and actions | concept §4.1, §4.2 |
| `foon-motion-node` | Actions are declared once with signatures and may be called many times | concept §4.1, §4.4 |
| `foon-edge` | Arguments are the inputs of a step and its result is the output | concept §4.2, §6 |
| `foon-semi-acyclic` | Single assignment (`let` binds once) keeps the IR acyclic; an action without effects still yields a new value | concept §4.1, §4.4; S85 |
| `foon-functional-unit` | One action call (inputs, action, output) is one step in the IR | concept §4.2 |
| `foon-adjacency-matrix` | No counterpart: A storage format; the Ozen IR is an arena of nodes with indices. | — |
| `foon-subgraph-creation` | No counterpart: Building graphs from annotated videos is outside Ozen, whose recipes are written as programs. | — |
| `foon-merge` | No counterpart: Ozen does not merge recipe graphs into a universal knowledge graph. | — |
| `foon-parser` | Identifiers are fixed ASCII names; display names and regional names come from phrase files | concept §4.1, §7.1 |
| `foon-motion-harmonics` | No counterpart: Robot trajectories are external integration, deferred by concept §12. | — |
| `foon-centrality` | No counterpart: Graph statistics over a knowledge network are not part of Ozen. | — |
| `foon-motion-frequency` | No counterpart: Ozen does not predict actions from statistics. | — |
| `foon-kitchen-list` | The kitchen spec lists what is available, and variants whose `uses` cannot be met are removed | concept §4.2 |
| `foon-task-tree-retrieval` | No counterpart: Ozen does not plan recipes backward from a goal; the author writes the recipe. | — |
| `foon-unsolvable` | When every variant of an action is removed by the kitchen, compilation fails with a `KIT` error listing the candidates | concept §4.2 |
| `foon-motion-cost` | `--prefer` criteria (fewer washes, less waiting, fewer tools, steady pace) order otherwise equal schedules | concept §4.2; D26 |
| `foon-task-sequence` | The schedule is rendered as step-by-step text by the renderer | concept §7.1 |
| `foon-quantities-missing` | Quantities are first-class dimension types with ranges and linear conservation | concept §4.1 |

## Kumbhakern et al. (2025)

| Element | Ozen | Basis |
| --- | --- | --- |
| `kumbhakern-action-graph` | Recipes compile to an IR DAG; the scheduler picks one order among those the partial order allows | concept §4.2, §6 |
| `kumbhakern-process` | An action call with requirements and effects on state axes and typed arguments (time, heat) | concept §4.1 |
| `kumbhakern-transfer` | No counterpart: Locations and moving items between places are a non-goal (concept §12: positions and movement in the kitchen). | — |
| `kumbhakern-plate` | Plating and assembly in menus (`plate`, `assemble`, vessels and fallbacks) | concept §4.5 |
| `kumbhakern-ingredient-schema` | Ingredient declarations have a type, a quantity with unit, and optional part | concept §4.4; S8 |
| `kumbhakern-environment` | No counterpart: Positions and geometry in the kitchen are a non-goal (concept §12). | — |
| `kumbhakern-ppc` | Intermediate results are values bound by `let` or passed directly as nested calls | concept §4.4 |
| `kumbhakern-provenance` | Linear typing tracks every value back to its ingredients; a `Mixture` keeps `Hazard` and allergens of its inputs | concept §4.1; S46 |
| `kumbhakern-technique-lexicon` | The standard library's action table gives each action's arguments, defaults, requirements, and effects | concept §12.2; S27 |
| `kumbhakern-temperature` | Heat is an enum argument (`low`, `medium`, `high`) instead of a unit | concept §12.2; S39 |
| `kumbhakern-global-axioms` | The type checker enforces the IR invariants: dimensions, states, linear use, and safety | concept §4.1, §6.1 |
| `kumbhakern-concurrency` | `parallel { branch ... }` is fork-join; all branches finish before the next statement | concept §4.2 |
| `kumbhakern-relative-timing` | No counterpart: Ozen cannot interject a step inside a running step; a candidate to consider (relative timing within a step). | — |
| `kumbhakern-termination-condition` | No counterpart: Result-based termination with sensors is external integration, deferred by concept §12. | — |
| `kumbhakern-utensil` | `uses` requests tools by capability, and container capacity is computed from quantities | concept §4.2 |
| `kumbhakern-instance-tag` | Distinct values are distinguished by their `let` names and step numbers | concept §4.4 |
| `kumbhakern-environment-occupancy` | Tools and places are resources whose occupancy the scheduler checks; a tool is busy until its step ends and washing is done | concept §4.2 |
| `kumbhakern-plugin-recipe` | Recipe outputs are typed and can be used by other recipes in a menu | concept §4.5 |
| `kumbhakern-transfer-limitations` | Portions of a value are written as divisions (`meat / 3`), and linear typing conserves the total | concept §4.1; S81 |
| `kumbhakern-implicit-step-recovery` | No counterpart: Ozen does not add steps the author did not write; missing steps are type errors instead (concept §4.1). | — |

## Candidates to add to Ozen

Elements with no counterpart that could be added to Ozen (for the user to decide):

- Holding resources: a tool stays occupied after a step until the next step that takes its product (`nakabe2021-holding-resource`, `nakabe2021-holding-requirement`).
- Preparation time before a step, separate from its duration (`nakabe2021-preparing-time`).
- No-gap continuation: one step must start exactly when another ends (`nakabe2021-previous-task`).
- Containment: which container holds which food (`tasse2008-put`, `tasse2008-remove-error`).
- Relative timing: interjecting a step inside a running step (`kumbhakern-relative-timing`).
