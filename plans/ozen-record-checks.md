# Plan for using Ozen as a checkable record

- Created: 2026-11-24
- Source: none (no upstream code). The requirements are R1–R13 in `docs/record-requirements.md` (below, "the requirements"). That document is the authority; this plan decides the syntax, the option names and the output fields that it leaves open.
- Target: `/home/atsuki/Documents/Ozen`, `main` (D138). The facts in §3 were read and run at `eb7e24f`, with `build/manual-gcc-release/ozen` (version 0.1.0, built from the same sources; all 641 goldens pass).
- Rules: `CLAUDE.md` (work on `main`, committed files in English, `G-REPO-NOREPLY` before every push). The goals in §5 of `plans/ozen-roadmap.md` (the roadmap), `plans/ozen-syntax-refresh.md` (the syntax plan), `plans/ozen-readme-compact.md` (the README plan), `plans/ozen-quantity-checks.md` (the quantity plan), `plans/ozen-cooking-feedback.md` (the feedback plan) and `plans/ozen-dev-setup.md` (the setup plan) are prerequisite goals (§5).

## 0. How to read this plan

- This plan follows the setup plan and continues its numbers. Design decisions start at **D137** and spec items at **S1076** (the setup plan ends at D136 and S1075).
- Every completion condition is a command whose result decides pass or fail. Checks by people are not tasks; they go to the hand-over list in §8.
- Completion conditions refer to the prerequisite goals listed in §5 and to the `G-RC-*` goals defined in §5. The code block in §5 holds the authoritative commands.
- The decisions D137–D155 in §2 were decided on 2026-11-24 in one answer from the user, without a question for each: D138 takes its first alternative for pushing (push without asking, in batches at the phase gates), and every other decision is the recommendation. Each section has a `- **Decision:**` line at the top; the comparison of the recommendation and the alternatives stays as history (P0-1). The syntax in §4 is written with the recommended choices, which are the decisions.
- The requirements use the word "quantity argument" for time, temperature, heat, size and the end condition. D143 fixes which parameters that covers.
- After writing, the plan had one independent audit by a subagent: 32 findings, 28 taken in full and 4 in part (end of §5.3, §9).
- Facts about the target were checked on 2026-11-24 by reading the files at `eb7e24f` and by running the built `ozen` (§3). §3.2 repeats the measurements of §2 of the requirements.
- Each checkable claim in §4 has a spec item ID. The coverage table in §5.3 assigns each S item a test, a goal, or a hand-over in §8 with its reason.
- The examples in this plan and all test material are written new for this plan. No transcription of a third-party recipe enters the repository (`G-RC-NO-THIRD-PARTY`).

---

## 1. Purpose and scope

**Purpose**: Let a recipe that someone else wrote be kept in Ozen as a record that a machine can check. For every value in the record, the record says where the value came from: the source states it, a definition gives it, the transcriber supplied it, or the source does not state it. One switch turns on checks that find every place where such a mark is missing, so a transcriber cannot finish a record without marking each place. Without the switch, Ozen behaves as it does today.

A record after this plan (recommended syntax; a new example, not a transcription):

```
source homeNotes {
  kind: "notebook"
  title: "Example notebook"
}

recipe sweetOmelet(servings: [unstated])
  @source(homeNotes, location: "page 12", heading: 1)
  @lines(heading: 1, ingredient: 3, step: 4, note: 1)
  @notRecorded(note: 1, kind: advice, reason: "serving suggestion") {
  ingredients {
    egg: Egg 3 pcs                 @source(ingredient: 1)
    sugar: Sugar 2 tbsp 1 tsp      @source(ingredient: 2)
    salt: Salt [any amount]        @source(ingredient: 3)
    oil: Oil [unstated]            @supplied(ingredient, reason: "step 3 oils the pan; the list has no oil")
  }
  let cracked = egg |> crack       @supplied(impliedStep, reason: "the source beats the eggs without cracking them")
  let beaten = cracked |> beat     @source(step: 1)
  let mixed = mix([beaten, sugar, salt])  @source(step: 2)
  let fried = fry(season(mixed, with: oil), [unstated], heat: [any amount], until: doneness >= done)
    @source(step: 3..4)
}
```

```text
$ ozen check --record sweetOmelet.ozen
result ok errors=0 warnings=1 infos=0        # SCH012: the frying time is not known, so there is no schedule
$ ozen debug record sweetOmelet.ozen         # one line per slot, with its origin and positions
$ ozen check --record --format json --emit record sweetOmelet.ozen
```

The same file without the marks gives one diagnostic per unmarked place (`REC001`–`REC008`, §4.11).

### In scope

- A value that says "the source does not state this", for ingredient amounts, quantity arguments and servings (R2, R9, R13; D140, D142)
- A mark for what the transcriber supplied, with a kind and a reason (R3, R10; D141, D149)
- Checking and outputting sources and positions, for the heading, the ingredient list, the steps and the notes; a default source per recipe; diagnostics for an undefined source and an unknown annotation (R4, R12; D150)
- Declaring the lines of the source, declaring lines as not recorded with a kind, and reporting uncovered lines (R5, R11; D149, D150)
- One switch for the record checks; with it, a quantity argument that takes a default is a diagnostic, and an end condition without a time is not an error (R6, R8; D143, D145, D146, D147)
- Outputting the origin of every slot in a form a machine reads (R1; D144)
- The least change to the scheduler and the renderer so that an unstated value is not shown or scheduled as 0 (R2; D148)
- An end condition whose state no definition has (D153), and annotations on continuation lines (D151)
- R13 is taken up: `servings: [unstated]`. The cost is one more place where the unstated value is read.
- `reference/`, `docs/concept.md`, `cli/README.md`, the diagnostics list, `phrases` (`en`, `ja`), `core/grammar.ebnf`, `schemas/result.schema.json` (R7)
- `G-RC-*` in `scripts/goals.sh`, this plan's line in `.rulesync/rules/overview.md`, and closing the setup plan's open range (D137, D139)

### Out of scope (reasons)

- **Everything in §6 of the requirements**: approximate amounts, using one listed ingredient in several steps without splitting the line, separating an egg, alternatives and optional steps, a mixture that carries state axes, additions to the standard library vocabulary, schema.org export, display names, choosing definitions from outside the record, and scheduler changes beyond D148. A record declares these with `@notRecorded` (R11), so they are counted.
- **The words of a vague value** ("slowly", "to taste"): the record keeps that the value is stated and vague, not which word the source used. The requirements do not ask for the word.
- **Origins of derived quantities, one by one**: the amount of a mixture or a converted amount comes from definitions by construction (S1182). The output does not list which density or unit weight was used; choosing and naming definitions is out of scope in the requirements.
- **Record checks for menus and for `make`**: the requirements speak of a recipe transcribed from one source. Arguments of `make` are not slots.
- **Showing origins in rendered text** (marks for supplied lines, footnotes with positions): the requirements ask for the IR and the JSON output. The renderer changes only as far as D148 says.
- **A diagnostic for unknown fields in `package`**: today an unknown field is ignored (§3.4). Only the new field of D145 is checked.
- **Changing what `[any amount]` does as an action argument when the record checks are off**: it stays 0 s in the schedule, as today (D148). No golden or sample writes it, but it is default behavior.
- **A new sample under `samples/`**: test material lives in `tests/`. A sample is rendered by the introduction page goals, which this plan does not touch.
- **Checks by people**: handed over in §8.

---

## 2. Design decisions

### D137. Plan name, goal prefix and start tag [decided]

- **Decision:** As recommended: the file `plans/ozen-record-checks.md`, the goal prefix `G-RC-`, the start tag `rc-start`, the range `RC=rc-start..HEAD`. (2026-11-24, confirmed by the user)
- **Recommendation**: the file `plans/ozen-record-checks.md`, the goal prefix `G-RC-`, the start tag `rc-start`, the range `RC=rc-start..HEAD`, and tests under `tests/golden/record/` and `tests/cli/record-*.sh`.
  - Reason: "record" is the word the requirements use, and "checks" matches the quantity plan's name. `RC` is not used by any plan.
- Alternative: `plans/ozen-provenance.md`, `G-PV-`, `pv-start`. "Provenance" is exact, but the requirements and the diagnostics say "origin" and "record", so a second word would enter the documents.
- Alternative: `plans/ozen-record.md`, `G-REC-`, `rec-start`. Shorter, but `REC` is also the proposed diagnostic category (D146), and a goal `G-REC-*` would read like a diagnostic.

### D138. Where to work, the range base, and pushing [decided]

- **Decision:** The first alternative for pushing, the recommendation for the rest: work on `main` and tag `rc-start` in P0-2. Push without asking, and push in batches: one push at each phase gate, after `G-REPO-NOREPLY`, not after each commit. Throwaway commits for mutation checks stay in a temporary worktree and are never pushed. (2026-11-24, confirmed by the user)
- **Recommendation**: work on `main` (`CLAUDE.md`). P0-2 sets the tag `rc-start`. Commits are per task or smaller. After a group of commits, ask before pushing; run `G-REPO-NOREPLY` before each push. Throwaway commits for mutation checks are made in a temporary worktree and never pushed.
  - Reason: `CLAUDE.md` says work happens on `main` and each plan's range starts at its tag. The generated `CLAUDE.md` has no line that allows pushing without a question, so the user's general rule (ask before a push) applies.
- Alternative: push without asking, as D116 of the setup plan did. Faster for `G-PLAT-CI`, but it needs the user's standing yes.
- Alternative: a work branch merged at the end. It separates the work, but it goes against `CLAUDE.md`, and range goals of earlier plans count on `main`.

### D139. The setup plan's open range [decided]

- **Decision:** As recommended: tag `ds-end` on `f124e84` and close the setup plan's range as `DS=ds-start..ds-end`. (2026-11-24, confirmed by the user)
- **Recommendation**: tag `ds-end` on `f124e84` (the setup plan's last commit, "Record the final gate and the first release"), change `DS` to `ds-start..ds-end` in `scripts/goals.sh`, and add one line with the reason to §9 of the setup plan.
  - Reason: `G-DS-SCOPE` already fails at `eb7e24f` (`files outside scope: got '1'`), because `docs/record-requirements.md` was committed after the final gate. Every file of this plan would add to that. This is the precedent of D87 and D111.
- Alternative: name `eb7e24f` and every commit of this plan in §9 of the setup plan as other work. It keeps the range open, but the list grows with each commit.

### D140. How an unstated value is written [decided]

- **Decision:** As recommended: the value `[unstated]`, written where the slot's value goes. (2026-11-24, confirmed by the user)
The requirements ask that "the source does not state it" can be written as a value (R2), and that it differs from a vague amount.

- **Recommendation**: a new literal `[unstated]`, written where the value would be: `oil: Oil [unstated]`, `fry(x, [unstated])`, `heat: [unstated]`, `recipe a(servings: [unstated])`.
  - Reason: it sits in the slot it fills, next to `[any amount]`, so a reader sees at the place of the value that there is none. One spelling serves every slot.
  - Conflict check (§3.4): today `[unstated]` parses as a list with the name `unstated`, and is `NAM001 undefined` unless a file defines that name. The parser reads `[` `unstated` `]` as the literal only when the three tokens stand exactly so; a file that defines a value named `unstated` and writes a one-element list of it changes its meaning. No golden, sample or std file has that name.
- Alternative: `[none stated]`. Two words in brackets are a syntax error today (`SYN003`), so no valid file changes. It reads less well, and it is longer in argument lists.
- Alternative: an annotation and no value: `oil: Oil @unstated`, `fry(x) @unstated(time, heat)`. No grammar change at all (annotations with arguments parse today). But the mark is away from the slot, an omitted argument looks the same as a forgotten one until the annotation is read, and R2 asks for a value.

### D141. How a supplied mark is written [decided]

- **Decision:** As recommended: the trailing annotation `@supplied(kind, reason: "text")`. (2026-11-24, confirmed by the user)
- **Recommendation**: a trailing annotation `@supplied(kind, reason: "text")` on the ingredient line, the statement or the recipe header that holds the supplied thing. Example: `oil: Oil [unstated] @supplied(ingredient, reason: "...")`.
  - Reason: a supplied thing has an ordinary value (an ingredient, a number, a step); the mark says who added it. That is what trailing annotations are for (§4.4 of the concept), and `@supplied(...)` parses today with no grammar change (§3.4).
- Alternative: a value wrapper, `sugar: Sugar supplied(0.5 tbsp, "...")`. The mark is at the value, but an ingredient line or a step that is supplied as a whole has no single value to wrap, so a second form would be needed.
- Alternative: a block `supplied { ... }` around supplied lines. It groups them, but it moves ingredient lines out of the order of the source list, and each line still needs its own kind and reason.

### D142. Telling vague from unstated for an action argument [decided]

- **Decision:** As recommended: `[any amount]` for a vague argument and `[unstated]` for an unstated one. (2026-11-24, confirmed by the user)
- **Recommendation**: use the existing `[any amount]` for a stated, vague value in every slot, and `[unstated]` (D140) for an unstated one. `fry(x, [any amount], heat: [unstated])` reads "the source gives a word of degree for the time and says nothing about the heat".
  - Reason: `[any amount]` already parses as an argument and already means "a word of degree and no number" for ingredients (R9 says "as an ingredient amount can today"). No new spelling is needed, and the two brackets stand side by side in the rule book.
- Alternative: a new pair `[vague]` and `[unstated]`, with `[any amount]` kept as the ingredient spelling of `[vague]`. "Any amount" reads oddly for a heat or a size, and `[vague]` fixes that, but there would be two spellings for one meaning.
- Alternative: write the vague word, `time: vague("slowly")`. It keeps the source's word, which the requirements do not ask for, and it puts display text of one language into the record.

### D143. Which arguments count as quantity arguments [decided]

- **Decision:** As recommended: a quantity argument is a parameter whose type is a dimension or an enum, plus `until:`, in calls written in a recipe body. (2026-11-24, confirmed by the user)
R8 needs an exact rule for "quantity argument".

- **Recommendation**: a parameter of an action whose declared type is a dimension (`Time`, `Length`, `Temperature`, `Mass`, `Volume`, `Count`, or one added with `dimension`, with or without `in` and `?`) or an enum (`Heat`, or one added with `enum`), plus `until:`. Only calls written in the body of a recipe are slots. A call inside a composite action's `steps` or inside a macro body is part of a definition: its values have the origin `definition`, and its defaults are not reported.
  - Reason: this is every non-ingredient, non-tool parameter, so it covers "every quantity argument, not only time, temperature, heat and amount" without a list of names. A definition's inner calls are the definition author's choices, which the requirements count as `definition`.
- Alternative: only dimension-typed parameters. `heat: Heat = medium` would then pass unmarked, though the requirements name heat.
- Alternative: also report defaults inside macro bodies at the call that expands them. A macro is text of the record's own vocabulary, but the transcriber cannot fix the place the diagnostic points to without editing the definition.

### D144. Where origins are output, and the schema version [decided]

- **Decision:** As recommended: a new `record` section (`--emit record`) and `ozen debug record`; `ir` gets optional keys only and `schemaVersion` stays 1. The user accepted that R1's "in the IR and in the JSON output" is met by the `record` section. (2026-11-24, confirmed by the user)
- **Recommendation**: a new output section. `--emit record` adds a top-level `record` array to the result JSON (one entry per recipe: source, lines, slots with origins and positions, coverage), and `ozen debug record` prints the same as text. The `ir` section gets only optional keys that appear when a value is unstated or vague (`unstated`, `vague`, `timeUnknown`), which the renderer needs. `schemaVersion` stays `1`; `schemas/result.schema.json` gains the `record` property and the optional keys.
  - Reason: every existing output stays byte-identical, because nothing is added to an output that has no new syntax in its input. A flat list of slots is what a count needs ("how many slots are unstated"), without walking the IR. The change is additive, and the existing sections `types`, `signatures`, `steps` and `schedules` were added the same way under version 1.
- Alternative: put `origin`, `source` and `supplied` on every ingredient, node and argument of `ir`, and raise `schemaVersion` to `2`. One place holds everything, but every golden with `--emit ir` changes, which the requirements forbid (§7), and arguments would change from `{name, value}` strings to objects.
- Alternative: add those keys to `ir` only when the record checks are on. Existing goldens stay, but the shape of `ir` then depends on a switch, and readers need both shapes.

### D145. How the record checks are turned on [decided]

- **Decision:** As recommended: both the CLI option `--record` and the package field `record: on`. (2026-11-24, confirmed by the user)
- **Recommendation**: both a CLI option and a package setting. `--record` on `check`, `build`, `render` and `debug`; and `record: on` in the `package` declaration, which turns the checks on for that package's own recipes. Either one is enough.
  - Reason: a folder of records is a package, and "this is a record" is a fact about the files, so `ozen check` in that folder should check them as records without a flag. The option serves a single file and the tests. The package field is read by the core, so the WebAssembly build gets it too.
  - Not through `Options` (as `strict` is): a new field in `type Options` changes the output of `--emit types`, and so existing goldens (`api/types-json`).
- Alternative: the CLI option only. Smallest, but every caller must remember the flag, and a record checked without it passes silently.
- Alternative: a recipe annotation `@record`. It is per recipe, but each recipe of a folder repeats it, and a forgotten one passes silently.
- Alternative: a diagnostic group, `--deny record`. It reuses the severity settings, but R6 also changes meaning (an end condition without a time is valid), which a severity cannot express.

### D146. Category, codes and severity of the new diagnostics [decided]

- **Decision:** As recommended: the new category `REC` (REC001–REC008, errors, only with the record checks on), and `NAM022`, `TYP024`, `TYP025` and `SCH012`. (2026-11-24, confirmed by the user)
- **Recommendation**: a new category `REC` (record) with `REC001`–`REC008`, all errors and produced only when the record checks are on; plus `NAM022 unknownAnnotation` and `TYP025 recordMark` (errors, record checks only, because annotations with these names pass silently today), `TYP024 unstatedPlace` (error, always) and `SCH012 durationUnknown` (warning, always). An undefined source name is the existing `NAM001 undefined`. The list is §4.11.
  - Reason: `--allow REC` and `--warn REC` then address the slot and coverage checks as one group, the same way `LIN` does (`NAM022`, `TYP025` and the `NAM001` for a source name are outside the group and are set by name). Errors, because the purpose is that a record cannot be finished with an unmarked place. The two "always" diagnostics are about the new literal only, so no existing file gets them.
- Alternative: no new category; put the record checks in `TYP` and `NAM`. Fewer places to touch (`scripts/check-diag-registry.sh` lists the categories), but the record checks cannot be addressed as a group.
- Alternative: warnings by default. A record would then finish with unmarked places unless the caller adds `--deny REC`.

### D147. `vagueAmount` when the record checks are on [decided]

- **Decision:** As recommended: with the record checks on, `[any amount]` gives no `TYP003`. (2026-11-24, confirmed by the user)
- **Recommendation**: with the record checks on, `[any amount]` does not produce `TYP003 vagueAmount`. In a record it is a legitimate value: "stated, vague".
  - Reason: the trial gave 79 of these warnings in 48 records; they would hide the record diagnostics. `--strict` together with `--record` then has no effect on `[any amount]`.
- Alternative: keep `TYP003` and let the transcriber pass `--allow vagueAmount` or write `@allow(vagueAmount)` in the package. No special case in the core, but every record project needs the same extra setting.

### D148. Unstated values in the scheduler and the renderer [decided]

- **Decision:** As recommended: an unknown time uses the definition's estimate if there is one; otherwise the dish gets no schedule and `SCH012`. The renderer prints the "not stated" phrases. (2026-11-24, confirmed by the user)
- **Recommendation**:
  - Scheduler: a step whose time is not known has no duration. If the action's definition estimates the time (`uses ... for`, `takes`, or the estimate of `until:`), that estimate is used and its origin is `definition`. If not, the dish gets no schedule, and `SCH012 durationUnknown` (warning) names the step. A time is "not known" when it is `[unstated]` (in any mode), or `[any amount]` or an end condition without a time (with the record checks on).
  - Renderer: the recipe text writes "amount not stated" for an unstated amount (a new phrase next to "to taste"), writes no time for a vague time, and writes "time not stated" for an unstated time. Kinds that need a schedule print the diagnostic text instead of a table for that dish.
  - Reason: a schedule with a silent 0 s is wrong, and a schedule that guesses a time would put a number into the record that the source does not have. Refusing the schedule for that dish is the least change (§6 of the requirements).
- Alternative: schedule the step with 0 s and flag it (`"timeUnknown": true`, total marked as a lower bound). A schedule always exists, but every consumer must remember to read the flag, and the total is wrong for the ones that do not.
- Alternative: make `[any amount]` as a time unknown in default mode too. More consistent, and no golden covers it, but it changes default behavior, which the requirements forbid.

### D149. Kinds of supplied and of not recorded: how they are written and whether the sets are closed [decided]

- **Decision:** As recommended: names inside the annotations, from closed sets that include `other`; a reason is required for every `@supplied` and for `kind: other`. (2026-11-24, confirmed by the user)
- **Recommendation**: the kind is a bare name inside the annotation, from a closed set that the core knows, and each set has `other`. Supplied: `ingredient`, `amount`, `split`, `servings`, `impliedStep`, `other`. Not recorded: `toolPreparation`, `pictureCaption`, `purpose`, `advice`, `alternative`, `crossReference`, `parallelAction`, `other`. An unknown kind is `TYP025`. A reason text is required for every `@supplied` and for `@notRecorded(..., kind: other)`.
  - Reason: a closed set catches a misspelled kind, and the JSON schema can list the values. `other` keeps a record finishable when the source has something the set lacks (the trial found 26 kinds of loss, 17 of them once), and `other` is itself counted, so the set can grow from evidence.
- Alternative: closed sets without `other`. Strictest, but a transcriber who meets a new case must misuse a kind or stop.
- Alternative: open sets (any name is a kind). Nothing to maintain, but `advise` and `advice` are counted as two kinds without a diagnostic.
- Alternative: define the kinds as `enum` in the standard library, so writers add their own. It follows "the vocabulary can be extended", but it adds to the standard library, changes `--emit definitions` goldens, and the kinds then differ between records, which defeats counting across records.

### D150. How positions and the lines of the source are written [decided]

- **Decision:** As recommended: `@source(NAME, location: "text")`, `@lines(...)`, line-level `@source(step: 3..4)` and `@notRecorded(...)`. (2026-11-24, confirmed by the user)
- **Recommendation**: positions are named arguments of `@source`, by part and line number, and the lines are declared on the recipe header:
  - `@source(homeNotes, location: "page 12")` on the recipe header names the default source. `location` is free text and is output as written.
  - `@lines(heading: 1, ingredient: 3, step: 4, note: 1)` on the header declares how many lines each part of the source has. The parts are the closed set `heading`, `ingredient`, `step`, `note`.
  - `@source(step: 3)`, `@source(step: 3..4)`, `@source(ingredient: 2, step: 1)` on an ingredient line or a statement give its positions. A first positional name (`@source(otherNotes, step: 3)`) overrides the default source. Several lines may give the same position.
  - `@notRecorded(note: 1, kind: advice)` on the header declares lines that the record does not carry.
  - Reason: all of these parse today as annotations (§3.4), so the grammar does not change. Numbers and ranges are values the core can check against `@lines`, which a string such as `"c3-c4"` is not. The part names make the heading, the list and the notes first-class, as R12 asks.
- Alternative: keep one string, `location: "c3-c4"`, and have the core parse a fixed format (`h`, `i`, `c`, `n` plus numbers). It matches how the trial wrote positions, but it hides structure in a string and needs its own small syntax and error messages.
- Alternative: a block in the recipe body that lists the source lines (`sourceLines { step 1; step 2 ... }`), optionally with their text. It could hold quoted text, but it is new grammar, and quoting third-party text in records raises a licence question the requirements leave outside.

### D151. Annotations on continuation lines [decided]

- **Decision:** As recommended: a line that starts with `@` continues the annotations of the line before. (2026-11-24, confirmed by the user)
- **Recommendation**: a line that starts with `@` continues the annotations of the item before it. Newlines before an `@` are skipped wherever annotations are allowed (recipe and menu headers, ingredient lines, statements, plates).
  - Reason: a record puts two or three annotations on most lines, and a recipe header carries `@source`, `@lines` and several `@notRecorded`. Today a newline before `@` is `SYN003` (§3.4), so the header must be one long line. A line starting with `@` is a syntax error today, so no valid file changes.
- Alternative: no change; long lines. No grammar work, but record headers of 300 characters are hard to review, which is the purpose of a record.
- Alternative: allow it only on recipe headers. Smaller, but the rule then differs by place.

### D152. An amount written as a sum of two units [decided]

- **Decision:** As recommended: nothing is added; `2 tbsp 1 tsp` is fixed by a golden and described in the documents. (2026-11-24, confirmed by the user)
- **Recommendation**: do not add anything. `2 tbsp 1 tsp` already works (§3.4: it is 35 ml), because quantities of one dimension written in a row are a sum (§4.4 of the concept). This plan adds a golden that fixes it for an ingredient amount, and the reference says so. It is one amount slot with one position.
- Alternative: a dedicated form (`2 tbsp and 1 tsp`). It reads like the source, but it duplicates what exists.

### D153. An end condition whose state no definition has [decided]

- **Decision:** As recommended: an enum marked `@cue` adds end states. (2026-11-24, confirmed by the user)
Today `until:` takes a comparison on an ordered axis or a value of the standard `enum Cue` (`fragrant`, `translucent`, `boiling`, `thickened`). `until: sticky` is `NAM001` and `TYP023` (§3.2).

- **Recommendation**: let the vocabulary add cues. An enum declared with the annotation `@cue` (`enum TrialCue { sticky, set } @cue`) contributes its values as cues. A cue does not change the state, as today.
  - Reason: actions, ingredients, units and axes can be added by the writer, and the trial needed them all; a cue is the same kind of vocabulary. The value is a checked name, so `sticky` and `stikcy` differ by a diagnostic, and it can be counted and given a display phrase.
- Alternative: allow a string, `until: "sticky"`. Nothing to define, but it is unchecked text, in the language of the source.
- Alternative: add the missing cues to the standard `Cue`. Simplest for the transcriber, but additions to the standard library are out of scope in the requirements, and the next source needs another.

### D154. Version [decided]

- **Decision:** As recommended: raise `VERSION` to `0.2.0` in the last phase; a release is asked separately. (2026-11-24, confirmed by the user)
- **Recommendation**: raise `VERSION` to `0.2.0` with `scripts/bump-version.sh` in the last phase. Publishing a release (the tag `v0.2.0`) is not part of this plan; it is asked separately after the final gate.
  - Reason: `v0.1.0` is published, and this plan adds language syntax, a CLI option and an output section. Golden expectations hold placeholders for the version (D132 of the setup plan), so the bump changes no expectation.
- Alternative: stay at `0.1.0` until a release is wanted. Nothing to do now, but a build from `main` then calls itself 0.1.0 while accepting syntax that the published 0.1.0 rejects.

### D155. How the goals run on this machine [decided]

- **Decision:** As recommended: run locally with the hand-built gcc binary and node, and read the rest from CI after each push. (2026-11-24, confirmed by the user)
This machine has no `cmake`, and its compilers and Node.js differ from `toolchain.txt` (gcc 16.2.1, clang 23.1.5, node 22.23.5 under `~/.local/opt`; §3.6).

- **Recommendation**: two levels. Locally, build `build/core-gcc-release/ozen` by hand with gcc (P0-2 records the exact command in §9) and run the goals that need only that binary, Python and node: all `G-RC-*` goals and the prerequisite goals listed in §3.6. The goals that need the four build configurations, the pinned tool versions or the other platforms (`G-CORE-BUILD`, `G-CORE-GOLDEN` for clang and the sanitizer builds, `G-REPO-TOOLS`, `tests/cli/init-rebuild.sh`, `G-PLAT-*`) are decided by CI after a push (`G-PLAT-CI`). Each phase gate reports both lists.
  - Reason: every goal of this plan is written against `$OZ` and plain tools, so the work can be checked here without waiting for CI, and CI stays the authority for what this machine cannot build.
- Alternative: install the pinned `cmake` under the home directory (no `sudo`), as was done for node. `scripts/build-core.sh` then works for the release configurations, but the compiler versions still differ from `toolchain.txt`, so `G-REPO-TOOLS` still fails here.
- Alternative: run nothing locally and rely on CI. No setup, but each mutation check then needs a push.

---

## 3. Facts checked (2026-11-24, `eb7e24f`)

### 3.1 Where the current behavior comes from

- **Annotations** are parsed everywhere `annotations` appears in `core/grammar.ebnf` and are read by name where a feature needs them (`unattended`, `useWithin`, `group`, `vessel`, `sameVessel`, `newVessel`, `needs`, `deprecated`, `serveWithin`, and `allow`/`warn`/`deny`/`expect` in `core/src/levels.c:155`). An annotation that no code asks for is never looked at. `@source` is read in two places only, both for definitions: `core/src/sema.c:1804` and `core/src/defs.c:475` test for `@source(provisional)` on a property. A serving property such as `@whileWarm` is a name that resolves to a `property` definition (`plate_annotations`, `sema.c:6069`).
- **Sources**: `source NAME { ... }` is a declaration (`grammar.ebnf`, rule `source`), and `stdlib/sources.ozen` declares the sources of the standard library's property values.
- **Ingredient amounts**: `eval_ingredients` (`sema.c:5580`). `[any amount]` is the node `Vague`; it sets `vague` on the ingredient and gives `TYP003` (`sema.c:5618`). A line with no amount expression takes neither branch, so it has no amount and no diagnostic.
- **Action calls**: `call_action` (`sema.c:4486`) binds positional and named arguments, then evaluates the declared default for each parameter that was not set (`sema.c:4573`–`4590`). After that, a default and a written value are the same `value`; nothing records which it was. An argument written `[any amount]` is evaluated like any expression and becomes an unknown value.
- **`until:`**: `apply_until` (`sema.c:4374`). A cue is a value of the enum named `Cue` (`is_cue`, `sema.c:4361`); the enum is found by that name, so a writer cannot add one. Without a time, a cue is `TYP022` (`sema.c:4390`), and a comparison is `TYP022` when no estimate is found (`sema.c:4459`).
- **IR JSON**: `sema.c:7400`–`7625`. A node's `args` are `{name, value}` strings and hold defaults and written values alike. An ingredient's `quantity` is `null` both for `[any amount]` and for a missing amount (`sema.c:7619`). `schemaVersion` is the literal `1` in `core/src/render.c:247`.
- **Settings**: `--strict` and `--allow/--warn/--deny` reach the core as fields of a `kitchen cli : Options { ... }` that the CLI writes (`cli/src/main.c:598`–`623`), read by `options_levels` (`sema.c:3283`). `type Options` is in `stdlib/prelude.ozen:830`.
- **Renderer**: `renderers/lib/recipe.mjs:245` passes a `null` quantity to `quantityText`, which prints the phrase `amount-any` ("to taste"). A missing amount therefore reads "to taste".

### 3.2 §2 of the requirements, measured again

All seven rows hold. Inputs are in §9 form (new files, std vocabulary only).

| # | Input | Result on 2026-11-24 |
|---|---|---|
| 1 | `sugar: Sugar` (no amount), used in `mix` | `result ok errors=0 warnings=0 infos=0`; the JSON ingredient has `"quantity":null` |
| 2 | `cut(slice, [any amount])`, `fry([any amount])`, `simmer([f, salt], [any amount])`, and `salt: [any amount]` | one `TYP003`, for `salt` only. `debug ir` shows `fry ... [..; 0 s]` and `simmer ... [..; 0 s; unattended]`; `debug schedule` gives `300..300 (300) %4 fry` and a total of 300 s. The cut size is `?` and is left out of the node's `args` |
| 3 | `cut(slice)` | the JSON node has `{"name":"size","value":"3 mm"}`, the same as for a written `3 mm`; `fry(x, [any amount])` shows `heat=medium` |
| 4 | `@source(nosuch, location: "1")` on a recipe, `@source(nosuch2, ...)` on an ingredient line, `@foo(1)` on a statement | `result ok`, no diagnostic; the JSON has no source and no position |
| 5 | `steamIt(until: doneness >= done)` for an added ingredient without a heating property; `simmer(until: thickened)` | `TYP022 untilTime` (error) for both. With std `Onion`, `fry(until: doneness >= done)` passes, because the heating model gives an estimate |
| 6 | `recipe a { ... }` | `result ok`, no diagnostic |
| 7 | (a statement about writing, not a behavior) | an added ingredient is declared like a listed one; nothing can mark it |

Three more facts, not in the requirements' table:

- A missing amount and `[any amount]` are also the same in the JSON (`"quantity":null`) and in the recipe text ("to taste").
- `until: sticky` (a state no definition has) is `NAM001 undefined` plus `TYP023 untilTarget`; `until: "sticky"` is `TYP023`.
- `--strict` turns `TYP003` into an error, for ingredient amounts only.

### 3.3 What the trial wrote (48 records, read outside the repository)

Read as evidence for the requirements and as a source of cases for §5.3. Nothing from it is copied.

- Marks in comments: `NOTIME` 24, `UNTIL` 19 (4 written as `until: doneness >= done`; the others name a state that no definition has, such as "set", "dissolved", "when it boils", or "to the firmness you like"), `SUP ingredient` 15, `SUP amount` 7 (all are splits of one listed line), `SUP step` 1, `SUP servings` 1, `LOSS:<kind>` 90 in 26 kinds, `REF` 4, `INC:<kind>` 6.
- Positions: `location: "N/N"` on every recipe header (48), `"cN"` (200) and `"cN-cN"` (63) on statements. Ingredient lines carry no position. Some statements carry none (`discard(waterSoak)`).
- One statement often covers several source steps (`c1-c2`), and several statements share one step (`c7` twice in one record).
- `[any amount]` as an argument: 36 lines, for the time of `boil`, `fry`, `simmer`, `soak`, `chill` and of added actions, and once for a temperature (`bake(x, [any amount], temp: [any amount])`).
- Defaults taken silently: added actions declare `heat: Heat = low`; 16 calls write `heat:`, the others take the default. `soak` and `cut` have defaults in std.
- Servings: 47 with a number (one of them supplied, marked in a comment), one with a range `2..4`.

### 3.4 Syntax that parses today

Run on 2026-11-24. "Parses" means `ozen check` gives no `SYN` diagnostic.

| Input | Result today |
|---|---|
| `@source(step: 1..2)`, `@source(mextFct2023, ingredient: 3, note: [1, 3])`, `@supplied(amount, reason: "x")`, `@notRecorded(step: 4, kind: advice)`, `@lines(step: 3)` as trailing annotations on one line | parse; no diagnostic |
| a newline before `@` in a recipe header | `SYN003 unexpectedToken args=["newline"]`, then `SYN003 args=["@"]` |
| `cut(slice, [unstated])` | parses as a list; `NAM001 undefined args=["unstated"]` |
| `cut(slice, [none stated])` | `SYN003 args=["stated"]` |
| `fry(s, [not stated])` | parses as a list of `not stated`; `NAM001 args=["stated"]` |
| `recipe a(servings: [any amount])` | `SYN003 args=["amount"]` (a header parameter takes a quantity, a literal or a type) |
| `package p { version: "0.1.0"; record: on }` | `result ok`; an unknown field of `package` is ignored |
| `enum MyCue { sticky } @cue` | parses; the annotation is ignored |
| `soySauce: 2 tbsp 1 tsp` | 35 ml in `debug steps`; `1 tbsp + 1 tsp` is 20 ml |

### 3.5 What to touch when adding a diagnostic or a category

From §3.5 of the quantity plan, §3.4 of the feedback plan and the commit `5d996fa`:

- `core/src/diag_codes.def` (append; `G-CORE-DIAG-APPEND`), `core/tests/diag-names.txt` (`G-CORE-DIAG-REGISTRY`), `diag-<kebab>` in `phrases/en.ftl` and `phrases/ja.ftl` (`G-STD-PHRASES`), `ARG_KINDS` in `renderers/lib/names.mjs`, the table in `docs/concept.md` §6.1 and `core/tests/severity-examples.txt` (`G-TYPE-SEVERITY-TABLE`), `reference/diagnostics.md` and `reference/ai-guide.md` (generated; `G-DS-REFGEN`).
- A new category also needs the default list in `scripts/check-diag-registry.sh:10` (`categories="SYN NAM ... ASM"`) and the category table in `docs/concept.md` §6.1.
- One existing golden lists every diagnostic: `tests/golden/api/local-not-listed.out` (`check --format json --emit definitions`; one line per code, `TYP023` is line 373). It gains one line per new diagnostic. No other golden expectation contains a diagnostic that a file does not produce.

### 3.6 Running the goals on this machine

- `scripts/goals.sh` uses `OZ=build/core-gcc-release/ozen` and the four configurations of `scripts/build-core.sh`, which calls `cmake`. `cmake` is not installed. `build/manual-gcc-release/ozen` exists; `build/core-gcc-release/` holds only `configure.log`.
- With a link from `build/core-gcc-release/ozen` to that binary and `~/.local/opt/node-v22.23.5-linux-x64/bin` first in `PATH`, these pass: `G-CORE-GOLDEN` for `core-gcc-release` (641 passed), `G-REND-TESTS` (60 pass), `G-CORE-DIAG-REGISTRY` (113 codes), `G-STD-PHRASES` (232 keys), `G-TYPE-SEVERITY-TABLE`, `G-CORE-GRAMMAR` (134 rules), `G-DS-REFGEN`, `G-DS-REFERENCE`, `G-CORE-DIAG-APPEND`, `G-DS-PLAN-NAMES`, `G-DS-GOLDEN-SAME`, `G-DS-COVERAGE`, `G-RF-SEPARATION`, `G-REPO-NOREPLY`. `scripts/run-cli-tests.sh` gives `33 cases, 1 failed`; the one is `init-rebuild`, which calls `cmake`.
- These do not pass here: `G-CORE-GOLDEN` for the clang and sanitizer configurations (not built), `G-REND-PHRASES-RENDER` (it reads `build/results/json`, which another goal's build step writes), `G-DS-SCOPE` (D139; it fails on any machine).
- `G-REPO-RED-FIRST` replays the range from `plan-start` and did not finish in 10 minutes here; it was not measured.
- The link was removed after the run. The command that built `build/manual-gcc-release/ozen` is not recorded in the repository; P0-2 rebuilds it and records the command (D155).

---

## 4. Behaviour spec

Each checkable claim is one item with one claim, and starts with `**S<n>**`. "Record checks on" means D145's switch is on for the recipe being checked. Examples use the recommended syntax of §2.

### 4.1 Turning the record checks on (D145)

- **S1076** `--record` on `ozen check`, `ozen build`, `ozen render` and `ozen debug` turns the record checks on for every recipe of the entry's package.
- **S1077** `record: on` in a `package` declaration turns the record checks on for the recipes of that package, without the option.
- **S1078** `record: off`, or no `record` field, leaves them off unless `--record` is given.
- **S1079** A value of `record` other than `on` or `off` is `TYP025 recordMark` with the arguments `["record", "value"]`, in any mode.
- **S1080** With the record checks off, no `REC` diagnostic and no `NAM022` is produced, whatever the file contains.
- **S1081** Recipes of a dependency are not checked as records when the entry's package turns the checks on.
- **S1231** A dependency whose own package says `record: on` is not checked as a record when the entry's package has the checks off.
- **S1082** `--record` is listed in the usage text of the CLI.

### 4.2 The unstated value (D140)

- **S1083** `[unstated]` is a primary expression of its own (grammar rule `unstated_value`), and `ozen debug ast --rules` reports that rule for it.
- **S1084** `[unstated]` is accepted as an ingredient amount (`oil: Oil [unstated]`), in any mode.
- **S1085** `[unstated]` is accepted as a quantity argument (D143) of an action call written in a recipe body, positional or named, in any mode.
- **S1086** `[unstated]` is accepted as the `servings` parameter of a recipe header (`recipe a(servings: [unstated])`), in any mode.
- **S1087** Anywhere else, `[unstated]` is `TYP024 unstatedPlace` (error) at the literal: inside arithmetic (`2 * [unstated]`), as a list element, as the value of a `let`, as an ingredient or tool argument, with `per serving`, as an argument of `make`, as a field of a kitchen or of options, as a parameter's default, as the count of `repeat`, as an end of a range (`1..[unstated] min`), as a header parameter other than `servings`, as an item of a `plate`, and inside the `steps` of a composite action or a macro body.
- **S1088** An ingredient with an unstated amount has no amount: it gives neither `TYP003` nor `TYP021`, and it is not counted in shortage checks, the same as a vague amount today.
- **S1089** An ingredient with an unstated amount is still checked for use: unused is `LIN005`, used twice is `LIN001`.
- **S1090** An unstated argument is an unknown value: the dimension and range checks (`TYP001`, `TYP012`) are not applied to it.
- **S1091** An unstated argument of an enum type (`heat: [unstated]`) is accepted, and the call gives no diagnostic because of it.
- **S1092** A recipe with `servings: [unstated]` behaves as a recipe without a `servings` parameter does today: an amount `per serving` is `TYP020 servingsUnknown` unless the build gives the servings (`-s N`), and an amount without `per serving` is a total.
- **S1228** The `record` section is the same with and without `-s N`: origins do not depend on the build's servings.
- **S1093** `ozen debug ir` prints `[unstated]` for an unstated ingredient amount and `name=[unstated]` for an unstated argument.
- **S1236** `ozen debug steps` prints `[unstated]` for an unstated ingredient amount, and an unstated argument in the call form (`fry(%1, [unstated], heat: [unstated])`).
- **S1094** In the JSON `ir`, an ingredient with an unstated amount has `"quantity":null` and `"unstated":true`; the key `unstated` is absent otherwise.
- **S1095** In the JSON `ir`, a node with unstated arguments has `"unstated":[<parameter names>]` in the order of the parameters; the key is absent when there are none.
- **S1096** A file that defines a value named `unstated` can still use the name outside the exact form `[unstated]` (`[unstated, x]`, `(unstated)`).

### 4.3 Vague values for arguments (D142, D147)

- **S1097** `[any amount]` is accepted as a quantity argument in any mode, with no diagnostic, as today.
- **S1098** With the record checks on, a node with vague arguments has `"vague":[<parameter names>]` in the JSON `ir`; with the checks off the key is absent.
- **S1099** With the record checks on, `[any amount]` as an ingredient amount gives no `TYP003`.
- **S1100** With the record checks on and `--strict`, `[any amount]` still gives no `TYP003`.
- **S1101** With the record checks off, `[any amount]` as an ingredient amount gives `TYP003` as today.

### 4.4 Quantity arguments and defaults (D143, R8)

- **S1102** A quantity argument is a parameter of an action whose declared type is a dimension (with or without `in` and `?`) or an enum, and the named argument `until`.
- **S1103** With the record checks on, a call in a recipe body that leaves out a quantity argument which has a default is `REC002 defaultArgument` at the call, with the arguments `[action, parameter, <the default as written>]`.
- **S1104** One call that leaves out two such arguments gives two `REC002`.
- **S1105** Writing the value, `[any amount]` or `[unstated]` for that argument removes the diagnostic.
- **S1106** `REC002` applies to parameters of every dimension, including one added with `dimension`, and to parameters of an added `enum`.
- **S1107** A parameter that takes an ingredient, a tool or a list of ingredients is not a quantity argument, and leaving out its default is not `REC002`.
- **S1108** A call inside the `steps` of a composite action that takes a default is not `REC002`.
- **S1109** A call inside a macro body that takes a default is not `REC002`.
- **S1110** A value that the caller of a macro writes and the macro splices in (`$t`) has the origin of the caller's text (`stated`, `statedVague` or `unstated`), not `definition`.
- **S1111** With the record checks off, a default is taken silently as today.

### 4.5 End conditions (R6, D153)

- **S1112** With the record checks on, `until:` without a time gives no `TYP022`, for a comparison and for a cue.
- **S1113** With the record checks off, `until:` without a time and without an estimate is `TYP022` as today.
- **S1114** With the record checks on, a call with `until:` and no time has a time slot whose origin is `unstated`, although nothing is written for it.
- **S1115** An enum declared with `@cue` adds its values as cues: `until: sticky` is accepted for `enum TrialCue { sticky } @cue`, in any mode.
- **S1116** A cue from an `@cue` enum does not change the state of the output.
- **S1117** A value of an enum without `@cue` is still `TYP023 untilTarget`.
- **S1118** With the record checks off, a cue from an `@cue` enum needs a time (`TYP022`), the same as a standard cue.
- **S1119** In the JSON `ir`, a cue from an `@cue` enum is output as `"until":{"cue":"sticky"}`, the same form as a standard cue.
- **S1219** `until: [any amount]` is accepted in any mode: the end condition is stated and vague ("to the firmness you like"). It does not change the state, its slot has the origin `statedVague`, and with the record checks off it needs a time like a cue.
- **S1220** `until: [unstated]` is `TYP024`: an end condition that the source does not state is not written.
- **S1221** With the record checks on, a call that writes `until:` and leaves out a time that has a default is `REC002` for the time; the time slot is `unstated` only when the parameter has no default.

### 4.6 Supplied marks (D141, D149, R3, R10)

- **S1120** `@supplied(kind, reason: "text")` is read on an ingredient line, on a statement of a recipe body, and on a recipe header.
- **S1121** The kinds are `ingredient`, `amount`, `split`, `servings`, `impliedStep` and `other`; another name is `TYP025 recordMark` with the arguments `["supplied", "kind"]`.
- **S1122** A missing or empty `reason` is `TYP025`.
- **S1123** `ingredient`, `amount` and `split` belong on an ingredient line, `servings` on a recipe header, `impliedStep` on a statement that creates at least one action step, and `other` on any of the three; a kind on another target is `TYP025`.
- **S1124** Two `@supplied` on one line are `TYP025`.
- **S1125** `@supplied(amount)` and `@supplied(split)` need a written number or range as the amount; with `[unstated]`, `[any amount]` or no amount they are `TYP025`.
- **S1126** With the record checks on, a `split` line whose position no other ingredient line shares is `TYP025`.
- **S1127** `@supplied(impliedStep)` marks every action step that the statement creates.
- **S1128** With the record checks off, a malformed or misplaced `@supplied`, `@lines`, `@notRecorded`, `@source` or `@cue` gives no diagnostic, as today; its slot has the origin it would have without the mark.
- **S1129** A supplied ingredient line or step needs no position, and may have one.

### 4.7 Sources, lines and positions (D150, R4, R12)

- **S1130** `@source(NAME, location: "text")` on a recipe header sets the recipe's default source; `location` is kept as written.
- **S1131** With the record checks on, a source name that is not a `source` declaration in scope is `NAM001 undefined` at the name, on a header, an ingredient line or a statement.
- **S1132** `@lines(heading: H, ingredient: I, step: C, note: N)` on a recipe header declares the lines 1..n of each part; a part left out has 0 lines.
- **S1133** In `@lines`, a part name outside `heading`, `ingredient`, `step`, `note`, a value that is not a whole number of 0 or more, and a second `@lines` on the same header are each `TYP025`.
- **S1134** `@source(PART: n)` and `@source(PART: a..b)` on an ingredient line or a statement give that line the positions n, or a to b inclusive, in the default source.
- **S1135** One `@source` may name several parts, and one line may carry several `@source`; the line has all the positions.
- **S1136** A first positional name in a line's `@source` (`@source(otherNotes, step: 3)`) uses that source instead of the default for that annotation.
- **S1137** A range whose first number is larger than its second, a line number below 1, and an unknown part name in `@source` are each `TYP025`.
- **S1138** `location: "text"` on a line's `@source` is kept in the output and does not count as a position.
- **S1139** A statement's positions belong to every action step it creates (each step of a pipe).
- **S1140** On a recipe header, part arguments in `@source` (`@source(homeNotes, heading: 1)`) are positions of the header: they count for coverage, and the servings slot carries them when the servings are a number or a range.
- **S1141** With the record checks on, a recipe header without a default source or without `@lines` is `REC005 sourceMissing`, once per recipe, with the argument `[recipe]`.
- **S1142** With the record checks on, an ingredient line that has no position and is not `@supplied(ingredient)` or `@supplied(other)` is `REC004 positionMissing` with the argument `[label]`.
- **S1143** With the record checks on, a statement that creates at least one action step, has no position and is not `@supplied(impliedStep)` or `@supplied(other)` is `REC004` with the name of the first action that the statement calls.
- **S1144** With the record checks on, a recipe whose servings are a number or a range, with no position on the header and no `@supplied(servings)`, is `REC004` with the argument `["servings"]`.
- **S1145** A statement that creates no action step (`let t = 3 min`) needs no position.
- **S1146** A `plate` inside `serve` is a step for these rules: it takes `@source` and `@supplied`, and without them it is `REC004` with the argument `["plate"]`.
- **S1147** With the record checks on, a position outside the declared lines is `REC006 unknownPosition` with the arguments `[part, line, <declared count>]`.
- **S1148** With the record checks on, an annotation whose name is neither built in nor the name of a definition in scope is `NAM022 unknownAnnotation` with the argument `[name]`.
- **S1149** Every annotation that an existing golden, sample or standard library file uses is free of `NAM022` with the record checks on.
- **S1150** The built-in annotation names are those the core reads by name; §4.14 lists them, and P1-1 checks the list against the source.
- **S1225** A position in a source other than the default is output with its source name; it is not checked against `@lines` and does not count in the coverage.
- **S1227** With the record checks on, `@lines` or `@notRecorded` anywhere but a recipe header, `@supplied` on a menu, a plate item or a definition, and `@cue` on anything but an `enum` are each `TYP025` with the problem `target`.
- **S1230** A statement inside `if`, `for`, `repeat` or a `branch` of `parallel` is a statement of the recipe body for all these rules; its diagnostics appear once per evaluation, as other diagnostics do today.
- **S1235** The order of the annotations on a header does not matter (`@notRecorded` before `@lines`, `@lines` before `@source`).

### 4.8 Lines not recorded, and coverage (D149, D150, R5, R11)

- **S1151** `@notRecorded(PART: n, kind: K, reason: "text")` and `@notRecorded(PART: a..b, kind: K)` on a recipe header declare those lines as not recorded.
- **S1152** The kinds are `toolPreparation`, `pictureCaption`, `purpose`, `advice`, `alternative`, `crossReference`, `parallelAction` and `other`; another name, or no `kind`, is `TYP025`.
- **S1153** `reason` may be left out, except with `kind: other`, where a missing or empty reason is `TYP025`.
- **S1154** A header may carry several `@notRecorded`, also for the same line.
- **S1155** A line may be referred to by the record and also be declared not recorded (a recorded step that drops an alternative tool); this is not a diagnostic.
- **S1156** A `@notRecorded` line outside the declared lines is `REC006`.
- **S1157** The coverage report has one entry for every declared line, in the order heading, ingredient, step, note, then by line number.
- **S1158** An entry's `refs` is the number of slots' lines (ingredient lines, statements, the header) that give that position; two ingredient lines with the same position give 2.
- **S1159** An entry's status is `referred` when `refs` is 1 or more, else `notRecorded` when a `@notRecorded` names it, else `uncovered`.
- **S1160** An entry lists the kind and the reason of every `@notRecorded` that names it, whatever its status.
- **S1161** With the record checks on, each `uncovered` line is `REC007 uncoveredLine` at the recipe header, with the arguments `[part, line]`.
- **S1162** Lines of the ingredient list, the notes and the heading are covered by the same rules as steps.
- **S1234** A coverage entry has the keys `part`, `line`, `refs`, `status` and `notRecorded` (a list of `{kind, reason}`, empty when none; `reason` is `null` when left out).

### 4.9 Slot checks (R2, R6, R13)

- **S1163** With the record checks on, an ingredient line with no amount is `REC001 amountMissing` with the argument `[label]`.
- **S1164** Writing a number, `[any amount]` or `[unstated]` removes `REC001`.
- **S1165** With the record checks on, a recipe without a `servings` parameter is `REC003 servingsMissing` with the argument `[recipe]`.
- **S1166** `servings: [unstated]`, and a number with `@supplied(servings)`, both satisfy `REC003` and `REC004` for the servings.
- **S1167** A complete record (§1's example) checks with the record checks on with no error.
- **S1168** The same record with every mark removed gives exactly one diagnostic per unmarked place and no other error.
- **S1217** With the record checks on, a `buy` expression in a recipe body is `REC008 inlineIngredient` with the argument `[type]`: an ingredient of a record is declared in `ingredients`, where it has slots.

### 4.10 The record output (D144, R1)

- **S1169** `--emit record` adds a top-level `record` array to the result JSON, with one entry per recipe that was evaluated.
- **S1170** Without `--emit record`, the result JSON has no `record` key.
- **S1171** `ozen debug record FILE` prints the same content as text: one line per slot and one per coverage entry.
- **S1172** An entry has `dish`, `checks` (whether the record checks were on), `source` (`{name, location}` or `null`), `lines` (the four counts, or `null`), `slots` and `coverage`.
- **S1173** The slots are, in this order: one `servings` slot; for each ingredient line an `ingredient` slot and an `amount` slot; for each action step created by a statement of the recipe body, in the order of the IR, a `step` slot, one `argument` slot per quantity argument of the action (in parameter order, then `until` if written), and one `duration` slot.
- **S1174** A slot has `slot`, `origin`, `positions` (a list of `{source, part, from, to}`), and where they apply `label`, `node` (the IR node id), `name` (the parameter), `kind` and `reason` (for supplied), and `location`.
- **S1175** `origin` is one of `stated`, `statedVague`, `unstated`, `supplied`, `definition`, or `null`. `stated` and `statedVague` together are origin (a) of R1.
- **S1176** Servings: a number or range is `stated`, or `supplied` with `@supplied(servings)`; `[unstated]` is `unstated`; no `servings` parameter is `null`.
- **S1177** Ingredient slot: `supplied` with `@supplied(ingredient)` or `@supplied(other)`, else `stated`.
- **S1178** Amount slot: `[unstated]` is `unstated`; `[any amount]` is `statedVague`; a number or range is `supplied` when the line has `@supplied` with the kind `ingredient`, `amount`, `split` or `other`, else `stated`; no amount is `null`.
- **S1179** Step slot: `supplied` with `@supplied(impliedStep)` or `@supplied(other)`, else `stated`. Steps that the core inserts (`combine` for a list argument) and the inner steps of a composite action are not slots.
- **S1218** A call of a composite action written in a recipe body has one `step` slot (its `label` is the composite action's name, its `node` the last inner node), one `argument` slot per quantity parameter of the composite action, and one `duration` slot with the origin `definition`.
- **S1229** A `plate` has one `step` slot with the label `plate` and no `duration` slot; a step with variants (`cookRice`) has one `step` slot and one `duration` slot with the origin `definition`.
- **S1180** Argument slot: a written value is `stated`; `[any amount]` is `statedVague`; `[unstated]` is `unstated`; an omitted argument that takes its default is `definition`; an omitted time with `until:` is `unstated`; a value that comes from a macro body is `definition`.
- **S1181** Duration slot: `stated` when the step's time comes from a stated time argument; `definition` when the action's definition gives the time (`uses ... for`, `takes`, the estimate of `until:`); otherwise the origin of the time argument (`statedVague` or `unstated`). An action with no time argument and no time in its definition has the origin `definition`.
- **S1182** Quantities of intermediate values (the amount of a mixture, a converted amount) are not slots; they come from definitions, and the reference says so.
- **S1183** A slot of an ingredient line or a statement carries that line's positions; the servings slot carries the header's positions; a `duration` slot carries none.
- **S1184** With the record checks on and no `REC`, `NAM022`, `TYP025` or source-name `NAM001` diagnostic at any severity (allowed ones included), no slot has the origin `null`, and every slot whose origin is `stated` or `statedVague` and that is not a `duration` slot has at least one position.
- **S1185** The slots, origins, positions and coverage of the `record` section do not depend on the switch; only `checks` differs. With the checks off, a vague time argument is `statedVague` although the schedule uses 0 s, and a time left out with `until:` is `unstated` although `TYP022` may be given.
- **S1186** The result JSON with a `record` section validates against `schemas/result.schema.json`, and `schemaVersion` is `1`.
- **S1187** The schema lists the origins, the slot names, the parts, the coverage statuses and both sets of kinds as enumerations, equal to the lists of §4.14.
- **S1188** `--format sarif` output is unchanged by `--emit record`, except that the new diagnostics appear as rules when they are produced.

### 4.11 Diagnostics (D146)

| Code | Name | Severity | When | Arguments |
|---|---|---|---|---|
| `REC001` | `amountMissing` | error | record checks | `[label]` |
| `REC002` | `defaultArgument` | error | record checks | `[action, parameter, default]` |
| `REC003` | `servingsMissing` | error | record checks | `[recipe]` |
| `REC004` | `positionMissing` | error | record checks | `[label]` |
| `REC005` | `sourceMissing` | error | record checks | `[recipe]` |
| `REC006` | `unknownPosition` | error | record checks | `[part, line, count]` |
| `REC007` | `uncoveredLine` | error | record checks | `[part, line]` |
| `REC008` | `inlineIngredient` | error | record checks | `[type]` |
| `NAM022` | `unknownAnnotation` | error | record checks | `[name]` |
| `TYP024` | `unstatedPlace` | error | always | `[]` |
| `TYP025` | `recordMark` | error | record checks; always for the package field `record` | `[annotation or field, problem]` |
| `SCH012` | `durationUnknown` | warning | always | `[action]` |

- **S1189** The twelve diagnostics are appended to `core/src/diag_codes.def` in this order, and no existing line changes.
- **S1190** `REC` is a category: `--allow REC`, `--warn REC` and `@allow(amountMissing)` change the severity of the record diagnostics as for any category.
- **S1191** Every new diagnostic has a phrase in `phrases/en.ftl` and `phrases/ja.ftl`, and the renderer formats it with its arguments in both languages.
- **S1192** `docs/concept.md` §6.1 names the category `REC` and the twelve diagnostics, and `core/tests/severity-examples.txt` has an example for the new category.
- **S1226** The second argument of `TYP025` is one of the closed list `kind`, `reason`, `target`, `duplicate`, `amount`, `split`, `part`, `number`, `range`, `value`; the phrases choose their wording by it.

### 4.12 Scheduler and renderer (D148)

- **S1193** A step's time is unknown when its time argument is `[unstated]` (any mode), or `[any amount]` or left out with `until:` (record checks on), and the action's definition gives no time.
- **S1194** A dish with a step of unknown time has no schedule: `ozen debug schedule` prints no rows and no total for it, and the JSON `schedules` has no entry for it.
- **S1195** Each such step gives `SCH012 durationUnknown` (warning) with the action's name, and `SCH009 criticalPath` is not given for that dish.
- **S1196** A menu that makes such a dish has no schedule either.
- **S1197** When the definition gives the time (the heating estimate of `until:` for std `Onion`), the step is scheduled with it, and there is no `SCH012`.
- **S1198** In `debug ir`, a step of unknown time prints `?` where the time would be, never `0 s`; in the JSON `ir` its node has `"timeUnknown":true`, and the key is absent otherwise.
- **S1224** In the JSON `ir`, the `time` of a node of unknown time is `null`, never a range of 0; the schema allows `null` there.
- **S1199** With the record checks off, `[any amount]` as a time is scheduled as 0 s, as today.
- **S1200** The recipe text prints the phrase `amount-unstated` ("amount not stated", 「分量の記載なし」) for an unstated ingredient amount, in both languages.
- **S1201** The recipe text prints the phrase `time-unstated` ("time not stated", 「時間の記載なし」) for a step with an unstated time, and never a time of 0.
- **S1202** The recipe text prints no time for a step with a vague time (record checks on), and no size or heat words for an unstated or vague size or heat.
- **S1203** `ozen render --kind schedule` for a dish without a schedule prints the text of `SCH012` and no table for that dish.
- **S1222** `ozen render --kind html` and `--kind shopping` print the phrase `amount-unstated` for an unstated amount, never "to taste".
- **S1223** `ozen render --kind html` draws no time chart for a dish without a schedule and prints the text of `SCH012`.
- **S1232** Another dish of the same file whose times are known is scheduled as today, and no diagnostic that needs the schedule (`SCH005`, `SCH008`, `SCH010`, `SCH011`) is given for the dish without one.
- **S1233** The recipe text for a step with `until:` and an unknown time has no "(about ...)" part, for a comparison, a standard cue, an added cue (shown by the fallback name when no phrase exists) and `until: [any amount]`.

### 4.13 Syntax that stays or is added (D151, D152)

- **S1204** Newlines, blank lines and comment lines before an `@` are skipped wherever the grammar has `annotations`: recipe and menu headers, ingredient lines, statements, `serve`, plates and their items, and declarations (an `enum` followed by `@cue` on the next line).
- **S1205** An annotation on a continuation line attaches to the item before it, and the result is the same as on one line (`debug ast`).
- **S1206** An `@` with no item before it (the first token of a file or of a block) is `SYN003` as today.
- **S1207** `sugar: Sugar 2 tbsp 1 tsp` is one ingredient with 35 ml and one `amount` slot.

### 4.14 Things added or changed

| Kind | Items |
| --- | --- |
| CLI | the option `--record`; the subcommands `debug record` and `debug annotations`; the value `record` of `--emit` |
| Package field | `record: on` / `record: off` |
| Literal | `[unstated]` |
| Annotations read | `@supplied`, `@lines`, `@notRecorded`, `@cue`, and `@source` on recipe headers, ingredient lines and statements |
| Built-in annotation names (for `NAM022`) | `allow`, `warn`, `deny`, `expect`, `unattended`, `source`, `deprecated`, `needs`, `group`, `useWithin`, `serveWithin`, `vessel`, `sameVessel`, `newVessel`, `fixed`, `info`, `supplied`, `lines`, `notRecorded`, `cue` (P1-1 checks this list against `core/src`) |
| Supplied kinds | `ingredient`, `amount`, `split`, `servings`, `impliedStep`, `other` |
| Not-recorded kinds | `toolPreparation`, `pictureCaption`, `purpose`, `advice`, `alternative`, `crossReference`, `parallelAction`, `other` |
| Parts | `heading`, `ingredient`, `step`, `note` |
| Origins | `stated`, `statedVague`, `unstated`, `supplied`, `definition` |
| Coverage statuses | `referred`, `notRecorded`, `uncovered` |
| Problems of `TYP025` | `kind`, `reason`, `target`, `duplicate`, `amount`, `split`, `part`, `number`, `range`, `value` |
| Slots | `servings`, `ingredient`, `amount`, `step`, `argument`, `duration` |
| Diagnostics | the twelve of §4.11; category `REC` |
| Phrases | twelve `diag-*` keys, `amount-unstated`, `time-unstated`, and the forms of `step-until` and `step-until-cue` without a time (P5-2 names the keys), each in `en` and `ja` |
| Grammar | rule `unstated_value`; `annotations` skips newlines before `@`; `param` accepts `unstated_value` |
| JSON | `record` (top level); optional `unstated`, `vague`, `timeUnknown` in `ir`; `time` may be `null` |

- **S1208** `reference/language.md` has a section on records that lists every row of this table except the diagnostics, and its examples pass the reference's example check.
- **S1209** `reference/diagnostics.md` and `reference/ai-guide.md` are regenerated and contain the twelve diagnostics.
- **S1210** `docs/concept.md` §4.1 separates unstated from vague, §4.4 describes the record annotations, continuation lines and `@cue`, and §6 describes the `record` section.
- **S1211** `cli/README.md` describes `--record`, `debug record` and `--emit record`.
- **S1212** `core/grammar.ebnf` and `core/src/rules.def` name the same rules, and every rule is passed by a golden.

### 4.15 Invariants

- **S1213** Every golden expectation that exists at `rc-start` is byte-identical at the end, except `tests/golden/api/local-not-listed.out`, which only gains lines.
- **S1214** No file under `stdlib/` changes.
- **S1215** No test or document of this plan contains material from the trial transcriptions (names of its source, its recipes or its vocabulary file).
- **S1216** The version files hold the version that D154 decides.

### 4.16 Where code goes

| Place | What |
| --- | --- |
| `core/src/parse.c`, `core/src/lex.c` | `[unstated]`, newlines before `@`, the header parameter |
| `core/src/sema.c` | reading the marks, the slot table, the record checks, `@cue`, unknown time |
| `core/src/record.c` (new) | the `record` section as JSON and as text, and coverage |
| `core/src/sched.c` | no schedule for a dish with a step of unknown time |
| `cli/src/main.c` | `--record`, `debug record`, `--emit record` |
| `renderers/lib/recipe.mjs`, `renderers/lib/names.mjs`, `renderers/lib/schedule.mjs`, `renderers/lib/html.mjs`, `renderers/lib/shopping.mjs`, `renderers/lib/gantt.mjs` | the phrases, `ARG_KINDS`, the output without a schedule |
| `scripts/check-record-annotations.sh` (new) | S1149 over every existing `.ozen` file |
| `scripts/check-record-origins.py` (new) | S1184 over every JSON record output of the tests |

---

## 5. Machine-checked goals

**Prerequisite goals** (implied in every completion condition): all goals that run locally in §5 of the earlier plans, as far as D155 says they run on this machine; P0-2 records which pass at `rc-start`. Pushes happen at the phase gates, without a question (D138). Before every push: `G-REPO-NOREPLY`. After a push: `G-PLAT-CI`. `G-RF-SEPARATION` and `G-REPO-RED-FIRST` apply to this plan's commits: a new expectation is committed before the implementation that makes it pass, and never in the same commit as `core/`, `stdlib/` or `cli/src/`.

When a goal fails, report which goal and which check failed. A failing test is not loosened; its cause is found, and a difference from the spec is reported to the user.

- `RC=rc-start..HEAD`, `PLAN_RC=plans/ozen-record-checks.md`, `OZ=build/core-gcc-release/ozen`.
- P0-3 turns the code block below into `goal_G_RC_*` functions in `scripts/goals.sh`.

### 5.1 Structure and data

| ID | What it decides | Pass condition |
| --- | --- | --- |
| `G-RC-DECISIONS` | D137–D155 decided | decided 19, open 0, decision lines 19 |
| `G-RC-COVERAGE` | every S item has a row; every §8 item has a reason | empty diff, then 0 |
| `G-RC-PLAN-NAMES` | test names in the coverage table exist | `plan-tests: <n> names, 0 missing` |
| `G-RC-PLAN-TESTS` | one mutation record per row with a way to break it | `mutations: <m> rows, <m> recorded` |
| `G-RC-GOLDEN` | the record goldens pass | `golden: <n> passed, 0 failed, 0 timed out` with n at least the number of `cgolden/record/` names in §5.3 |
| `G-RC-GOLDEN-SAME` | S1213 | 0 existing test files changed or removed (goldens with their inputs, CLI cases, renderer tests); the one exception has 0 deleted lines |
| `G-RC-STD-SAME` | S1214 | empty diff |
| `G-RC-CLI` | the `record-*` CLI cases pass | `cli: 5 cases, 0 failed` |
| `G-RC-REND` | the renderer tests of §5.2 pass | `# fail 0` |
| `G-RC-DIAG` | S1189–S1192 | the 12 new lines after `TYP023` equal the expected list (code, category, name, severity, in order); 0 deleted lines; `G-CORE-DIAG-REGISTRY`, `G-STD-PHRASES`, `G-TYPE-SEVERITY-TABLE` exit 0; 12 `ARG_KINDS` entries |
| `G-RC-ORIGINS` | S1184, S1186, S1187 | `record-origins: <n> outputs, 0 problems, 0 enumeration values unseen` with n at least 20; the schema's enumerations equal §4.14 (empty output); self-test ends with `0 failed` |
| `G-RC-ANNOTATIONS` | S1149, S1150 | `record-annotations: <n> files, <n> checked, 0 with NAM022`, n equal to the number of tracked `.ozen` files outside `tests/golden/record/`; the list of §4.14 equals `ozen debug annotations`; self-test ends with `0 failed` |
| `G-RC-GRAMMAR` | S1212 | `G-CORE-GRAMMAR` exits 0; the fuzz dictionary has `unstated` |
| `G-RC-SYNTAX-LIST` | S1208 | every item of §4.14 (except the diagnostics, phrases and grammar rows) appears in backticks inside the records section of `reference/language.md`: 0 missing |
| `G-RC-DOCS` | S1209–S1211 | `G-DS-REFGEN` and `G-DS-REFERENCE` exit 0; each count at least 1 |
| `G-RC-NO-THIRD-PARTY` | S1215 | at least 20 patterns; 0 matches |
| `G-RC-SCOPE` | only files of §4.16, the documents of §4.14 and the tests change | 0 files outside `scripts/rc-scope.txt`; the file has no line that matches every path |
| `G-RC-SELFTESTS` | new checkers have self-tests | each count 1; `G-REPO-SELFTESTS` exits 0 |
| `G-RC-RANGES` | D139 | `ds-end` exists; `G-DS-SCOPE` exits 0 |
| `G-RC-VERSION` | S1216 | `G-REPO-VERSION` exits 0; `VERSION` holds D154's value |
| `G-RC-AUDIT` | P6-3 | at least 1 audit record |

```bash
# G-RC-DECISIONS (19, 0, 19)
grep -c '^### D[0-9]*\. .*\[decided\]' $PLAN_RC
grep -c '^### D[0-9]*\. .*\[open\]' $PLAN_RC
grep -c '^- \*\*Decision:\*\*' $PLAN_RC

# G-RC-COVERAGE (empty output, then 0)
diff <(sed -n '/^## 4\./,/^## 5\./p' $PLAN_RC | grep -oE '\*\*S[0-9]+\*\*' | tr -d '*' | sort -u) \
     <(sed -n '/^### 5\.3/,/^## 6\./p' $PLAN_RC | grep -oE '^\| S[0-9]+' | tr -d '| ' | sort -u)
sed -n '/^## 8\./,/^## 9\./p' $PLAN_RC | grep '^- ' | grep -vc 'Reason a command cannot decide:'

# G-RC-PLAN-NAMES, G-RC-PLAN-TESTS
scripts/check-plan-tests.sh $PLAN_RC
scripts/check-plan-tests.sh --mutations $PLAN_RC

# G-RC-GOLDEN
scripts/run-golden.sh $OZ tests/golden/record

# G-RC-GOLDEN-SAME (0, 0)
git diff --name-only --diff-filter=MDR rc-start -- tests/golden tests/cli renderers/test | grep -vx 'tests/golden/api/local-not-listed.out' | wc -l
git diff --numstat rc-start -- tests/golden/api/local-not-listed.out | awk '{d=$2} END{print d+0}'

# G-RC-STD-SAME (empty)
git diff --stat rc-start -- stdlib

# G-RC-CLI
d=$(mktemp -d); cp tests/cli/lib.sh tests/cli/record-*.sh "$d"; scripts/run-cli-tests.sh $OZ "$d" 2>&1 | tail -n 1; rm -rf "$d"

# G-RC-REND
node --test --test-reporter=tap 'renderers/test/record-*.test.mjs' 2>&1 | grep -E '^# (pass|fail)'

# G-RC-DIAG
diff <(sed -n '/TYP023/,$p' core/src/diag_codes.def | sed 1d | tr -d ' ') <(printf 'OZNI_DIAG(%s)\n' REC001,REC,amountMissing,ERROR REC002,REC,defaultArgument,ERROR REC003,REC,servingsMissing,ERROR REC004,REC,positionMissing,ERROR REC005,REC,sourceMissing,ERROR REC006,REC,unknownPosition,ERROR REC007,REC,uncoveredLine,ERROR REC008,REC,inlineIngredient,ERROR NAM022,NAM,unknownAnnotation,ERROR TYP024,TYP,unstatedPlace,ERROR TYP025,TYP,recordMark,ERROR SCH012,SCH,durationUnknown,WARNING)
git diff rc-start -- core/src/diag_codes.def | grep -c '^-OZNI_DIAG'
for g in G-CORE-DIAG-REGISTRY G-STD-PHRASES G-TYPE-SEVERITY-TABLE; do scripts/goals.sh $g > /dev/null 2>&1; echo "$g exit $?"; done
grep -cE '^  (amountMissing|defaultArgument|servingsMissing|positionMissing|sourceMissing|unknownPosition|uncoveredLine|inlineIngredient|unknownAnnotation|unstatedPlace|recordMark|durationUnknown):' renderers/lib/names.mjs

# G-RC-ORIGINS
scripts/check-record-origins.py --ozen $OZ --schema schemas/result.schema.json tests/golden/record   # runs each golden's file with check --record --format json --emit ir,record
scripts/check-record-origins.py --schema schemas/result.schema.json --lists $PLAN_RC                 # prints the enumeration values that differ from §4.14
scripts/test-check-record-origins.sh | tail -n 1

# G-RC-ANNOTATIONS
scripts/check-record-annotations.sh $OZ     # a file is "checked" when check --record prints a result line for it
git ls-files stdlib samples tests/golden | grep '\.ozen$' | grep -vc '^tests/golden/record/'
scripts/test-check-record-annotations.sh | tail -n 1
diff <(sed -n '/^| Built-in annotation names/p' $PLAN_RC | grep -oE '`[a-zA-Z]+`' | tr -d '`' | sort -u) \
     <($OZ debug annotations | sort -u)

# G-RC-GRAMMAR (exit 0, 1)
scripts/goals.sh G-CORE-GRAMMAR > /dev/null 2>&1; echo "exit $?"
grep -c '"unstated"' core/fuzz/ozen.dict

# G-RC-SYNTAX-LIST (0)
sed -n '/^### 4\.14/,/^### 4\.15/p' $PLAN_RC | grep '^| ' | grep -vE '^\| (Kind|---|Diagnostics|Phrases|Grammar|Built-in)' |
  grep -oE '`[^`]+`' | sort -u | while read -r w; do sed -n '/^## Records/,/^## /p' reference/language.md | grep -qF -- "$w" || echo "$w"; done | wc -l

# G-RC-DOCS
for g in G-DS-REFGEN G-DS-REFERENCE; do scripts/goals.sh $g > /dev/null 2>&1; echo "$g exit $?"; done
grep -c 'durationUnknown' reference/diagnostics.md
grep -cE '\-\-record' cli/README.md
grep -c 'debug record' cli/README.md
for w in '[unstated]' '`REC`' '@supplied' '@notRecorded' '@lines' '@cue' '--emit record' 'continuation'; do grep -cF -- "$w" docs/concept.md; done
grep -cF -- '--emit record' cli/README.md
grep -c 'REC008' reference/ai-guide.md

# G-RC-NO-THIRD-PARTY (at least 20, then 0)
grep -c . scripts/rc-third-party.txt
git grep -nEi -f scripts/rc-third-party.txt -- . ':!scripts/rc-third-party.txt' | wc -l

# G-RC-SCOPE (0, 0)
for c in $(git rev-list --no-merges $RC); do git diff-tree --no-commit-id --name-only -r $c; done | sort -u |
  grep -vxEf scripts/rc-scope.txt | wc -l
grep -cxE '\.\*|\.\+' scripts/rc-scope.txt

# G-RC-SELFTESTS
grep -cx 'scripts/check-record-origins.py scripts/test-check-record-origins.sh' scripts/selftests.txt
grep -cx 'scripts/check-record-annotations.sh scripts/test-check-record-annotations.sh' scripts/selftests.txt
scripts/goals.sh G-REPO-SELFTESTS > /dev/null 2>&1; echo "exit $?"

# G-RC-RANGES
git rev-parse --verify ds-end
scripts/goals.sh G-DS-SCOPE > /dev/null 2>&1; echo "exit $?"

# G-RC-VERSION
scripts/goals.sh G-REPO-VERSION > /dev/null 2>&1; echo "exit $?"
cat VERSION

# G-RC-AUDIT (at least 1)
grep -cE '^- Coverage audit: [0-9]+ findings, [0-9]+ taken' $PLAN_RC
```

`scripts/rc-third-party.txt` holds patterns that identify the trial material without quoting it: the name of the trial's source declaration, the identifiers of its recipes and the names of its added vocabulary that the standard library does not have. P0-3 writes it from the trial folder; the patterns are identifiers only. `ozen debug annotations` prints the built-in annotation names, one per line (S1150). New checkers come with a self-test that feeds a broken input and expects a failure.

### 5.2 Tests

- All test recipes are written new, with standard library vocabulary and small added definitions (an action with a default of each dimension, an enum with `@cue`). None is taken from a published recipe.
- Expected values come from the spec in §4, written by hand. `scripts/check-record-origins.py` reads the JSON output and the schema; it does not share code with the core.
- A golden is `tests/golden/record/<name>.ozen` with `// args:` on its first line, and `<name>.out`. Cases that need a `package` declaration or a dependency are CLI tests.

| Test | What it decides | Main task |
| --- | --- | --- |
| `cgolden/record/*` (the names in §5.3) | one behaviour each, by the diagnostics, `debug record`, `debug ir`, `debug schedule` or JSON output | P1–P4 |
| `cli/record-package` | S1077, S1078: `record: on` and `off` in `package`, with and without `--record` | P1-2 |
| `cli/record-dependency` | S1081, S1231: a dependency with unmarked recipes gives no `REC`, with the entry on, and with the dependency on and the entry off | P1-2 |
| `cli/record-modes` | S1076: `--record` with `build`, `render`, `debug ir` and `debug schedule` | P1-2 |
| `cli/record-usage` | S1082 | P1-2 |
| `cli/record-emit` | S1169, S1170, S1188: the JSON with and without `--emit record`, and SARIF | P4-2 |
| `rtest/record-text` | S1200–S1202, S1233, in `ja` and `en` | P5-2 |
| `rtest/record-no-schedule` | S1203 | P5-2 |
| `rtest/record-html` | S1222, S1223 | P5-2 |
| `rtest/record-shopping` | S1222 | P5-2 |
| `rtest/record-diag-text` | S1191: each of the twelve diagnostics formats with its arguments in `ja` and `en` | P5-1 |
| `sh/test-check-record-origins.sh` | the checker fails on an output with a `null` origin, with a stated slot without a position, with a schema violation, and with a schema whose enumeration lacks a value of §4.14 | P4-3 |
| `sh/test-check-record-annotations.sh` | the checker fails when a file gives `NAM022` and when a file prints no result line | P3-4 |

The goldens number about 130 (§5.3 names each; one golden has one command, so a behaviour seen through two outputs has two goldens). The CLI cases are 5, the renderer tests 5.

Commands decide everything in §4. §8 holds only how the text reads and whether the syntax serves a transcriber.

### 5.3 Coverage table

Aspects: Normal, Boundary, No-effect condition, Side effects, Time and order, Persistence, Real path, Data completeness, Platform path, Docs match, Fixed premise. "Persistence" does not apply to this plan (nothing is stored between runs). "Platform path": the core is one code base for every platform, and `G-PLAT-CI` runs the goldens on each; only the package field (read by the core, not the CLI) has a row.

| S item | Aspect | Test or goal | How to break it | Main task |
| --- | --- | --- | --- | --- |
| S1076 | Normal | `cgolden/record/switch-option` | ignore `--record` | P1-2 |
| S1076 | Real path | `cgolden/record/switch-debug` (`debug record --record`), `cli/record-modes` (`build`, `render`, `debug ir`, `debug schedule`) | read the option in `check` only | P1-2 |
| S1077 | Normal | `cli/record-package` | ignore the package field | P1-2 |
| S1077 | Platform path | `cli/record-package` (the field is read from the file by the core; no CLI flag) | read the field in `cli/src/main.c` only for `--record` | P1-2 |
| S1078 | No-effect condition | `cli/record-package` | treat `record: off` as on | P1-2 |
| S1079 | Boundary | `cgolden/record/switch-bad-value` | accept any value as on | P1-2 |
| S1080 | No-effect condition | `cgolden/record/switch-off-bare` (no marks: nothing for `REC001`–`REC003`, `REC005`, `REC008`) | produce `REC001` without the switch | P1-2 |
| S1080 | Boundary | `cgolden/record/switch-off-marked` (a position outside the lines, an uncovered line, an unknown annotation, an undefined source name, a line without a position: no diagnostic) | produce `REC007` without the switch | P3-3 |
| S1081 | No-effect condition | `cli/record-dependency` | apply the switch to every module | P1-2 |
| S1231 | No-effect condition | `cli/record-dependency` | check a dependency by its own field | P1-2 |
| S1082 | Docs match | `cli/record-usage` | drop the option from the usage text | P1-2 |
| S1083 | Normal | `cgolden/record/unstated-ast` | parse it as a list | P1-3 |
| S1084 | Normal | `cgolden/record/unstated-amount` | reject it in `ingredients` | P1-3 |
| S1085 | Normal | `cgolden/record/unstated-argument` | reject it as a positional argument | P1-3 |
| S1085 | Boundary | `cgolden/record/unstated-named-argument` | reject it as a named argument | P1-3 |
| S1086 | Normal | `cgolden/record/unstated-servings` | keep `param` as it is | P1-3 |
| S1087 | No-effect condition | `cgolden/record/unstated-place` (one line per place of the item, all fourteen), `cgolden/record/unstated-place-off` (the same with the checks off) | accept it in arithmetic | P1-3 |
| S1087 | Boundary | `cgolden/record/unstated-place-food` | accept it for an ingredient parameter | P1-3 |
| S1088 | Side effects | `cgolden/record/unstated-amount` | give `TYP003` for it | P1-3 |
| S1089 | Side effects | `cgolden/record/unstated-unused` | skip the use check for it | P1-3 |
| S1090 | No-effect condition | `cgolden/record/unstated-no-range` | apply `TYP012` to an unknown value | P1-3 |
| S1091 | Normal | `cgolden/record/unstated-enum` | give `TYP015` for an enum parameter | P1-3 |
| S1092 | Side effects | `cgolden/record/unstated-servings-per`, `cgolden/record/unstated-servings-scale` (`-s 4`) | treat unstated servings as 1 | P1-3 |
| S1228 | No-effect condition | `cgolden/record/origins-scale` (`debug record` with `-s 4` equals `origins-text`) | derive origins from scaled amounts | P4-1 |
| S1093 | Normal | `cgolden/record/unstated-ir-text` | print `?` for it | P1-3 |
| S1236 | Normal | `cgolden/record/unstated-steps-text` | print `?` in `debug steps` | P1-3 |
| S1094 | Data completeness | `cgolden/record/unstated-ir-json` | leave out the key | P1-3 |
| S1094 | No-effect condition | `G-RC-GOLDEN-SAME` | always print `"unstated":false` | P1-3 |
| S1095 | Data completeness | `cgolden/record/unstated-ir-json` | list the names in written order | P1-3 |
| S1096 | Boundary | `cgolden/record/unstated-name` | read `[unstated, x]` as the literal | P1-3 |
| S1097 | Normal | `cgolden/record/vague-argument-off` (a time, a size, a temperature and a heat) | give `TYP003` for an argument | P1-4 |
| S1098 | Data completeness | `cgolden/record/vague-argument` | print the key with the checks off | P1-4 |
| S1099 | No-effect condition | `cgolden/record/vague-amount-quiet` | keep `TYP003` in record mode | P1-4 |
| S1100 | Boundary | `cgolden/record/vague-amount-strict` | let `--strict` win | P1-4 |
| S1101 | Fixed premise | `G-RC-GOLDEN-SAME` (the existing `TYP003` goldens) | drop `TYP003` in every mode | P1-4 |
| S1102 | Data completeness | `cgolden/record/default-every-dimension` | test the parameter's name instead of its type | P2-1 |
| S1103 | Normal | `cgolden/record/default-argument` | take the default silently | P2-1 |
| S1104 | Boundary | `cgolden/record/default-two` | stop after the first | P2-1 |
| S1105 | No-effect condition | `cgolden/record/default-written` (value, vague, unstated) | report a written `[unstated]` | P2-1 |
| S1106 | Boundary | `cgolden/record/default-every-dimension` (Mass, Volume, Length, Time, Temperature, Count, an added dimension, an added enum) | handle `Time` and `Length` only | P2-1 |
| S1107 | No-effect condition | `cgolden/record/default-food` | report every parameter with a default | P2-1 |
| S1108 | No-effect condition | `cgolden/record/default-composite` | report inner calls | P2-1 |
| S1109 | No-effect condition | `cgolden/record/default-macro` | report calls in a macro body | P2-1 |
| S1110 | Normal | `cgolden/record/origin-macro` (a spliced number, `[any amount]` and `[unstated]`, and a value written in the body) | give a spliced value the origin `definition` | P4-1 |
| S1111 | Fixed premise | `cgolden/record/switch-off-bare` | report `REC002` without the switch | P2-1 |
| S1112 | Normal | `cgolden/record/until-no-time` (a comparison and a cue) | keep `TYP022` | P2-2 |
| S1219 | Normal | `cgolden/record/until-vague`, `cgolden/record/until-vague-off` | keep `TYP023` for it | P2-2 |
| S1220 | No-effect condition | `cgolden/record/until-unstated` | accept it as a cue | P2-2 |
| S1221 | Boundary | `cgolden/record/default-time-with-until` (a time with a default, and one without) | call the time unstated when it has a default | P2-2 |
| S1113 | Fixed premise | `G-RC-GOLDEN-SAME` (the `until/` goldens) | drop `TYP022` in every mode | P2-2 |
| S1114 | Data completeness | `cgolden/record/until-no-time-record` (`debug record`) | leave the time slot out | P4-1 |
| S1115 | Normal | `cgolden/record/cue-added` | look up `Cue` by name only | P2-3 |
| S1115 | Boundary | `cgolden/record/cue-collision` (a value named like a standard cue, the same value in two `@cue` enums, a value named like an axis value) | take the first enum found | P2-3 |
| S1116 | Side effects | `cgolden/record/cue-added-steps` (`debug steps`) | advance `doneness` for a cue | P2-3 |
| S1117 | No-effect condition | `cgolden/record/cue-not-marked` | accept any enum value | P2-3 |
| S1118 | Boundary | `cgolden/record/cue-added-off` | skip the time check for added cues | P2-3 |
| S1119 | Data completeness | `cgolden/record/cue-json` | print the cue as an axis | P2-3 |
| S1120 | Normal | `cgolden/record/supplied-kinds` (each kind on its target) | read it on ingredient lines only | P3-1 |
| S1121 | Boundary | `cgolden/record/supplied-bad-kind` | accept any name | P3-1 |
| S1122 | Boundary | `cgolden/record/supplied-no-reason` (missing, and `""`) | accept an empty reason | P3-1 |
| S1123 | No-effect condition | `cgolden/record/supplied-wrong-target` (every kind on each target it does not belong to) | accept `servings` on an ingredient | P3-1 |
| S1124 | Boundary | `cgolden/record/supplied-twice` | take the last one | P3-1 |
| S1125 | Boundary | `cgolden/record/supplied-amount-unstated` | accept `amount` with `[unstated]` | P3-1 |
| S1126 | Boundary | `cgolden/record/supplied-split-alone` | skip the shared-position check | P3-1 |
| S1127 | Normal | `cgolden/record/supplied-step-pipe` | mark the last step only | P3-1 |
| S1128 | No-effect condition | `cgolden/record/marks-off` (every malformed form of S1226 and S1227, checks off, no diagnostic) | produce `TYP025` in every mode | P3-1 |
| S1129 | No-effect condition | `cgolden/record/supplied-kinds` | ask a supplied line for a position | P3-1 |
| S1130 | Normal | `cgolden/record/source-default` | drop `location` | P3-2 |
| S1131 | Normal | `cgolden/record/source-undefined` (header, ingredient line, statement) | check the header only | P3-2 |
| S1131 | Boundary | `cgolden/record/source-not-a-source` (the name of an ingredient type) | accept any defined name | P3-2 |
| S1131 | No-effect condition | `cgolden/record/switch-off-bare` | report it without the switch | P3-2 |
| S1132 | Normal | `cgolden/record/lines` | treat a part left out as unlimited | P3-2 |
| S1133 | Boundary | `cgolden/record/lines-bad` (unknown part, `-1`, `1.5`, twice) | accept a fraction | P3-2 |
| S1134 | Normal | `cgolden/record/position-range` | read `a..b` as a only | P3-2 |
| S1134 | Boundary | `cgolden/record/position-range` (`3..3`, first and last line) | exclude the upper end | P3-2 |
| S1135 | Normal | `cgolden/record/position-several` | keep the last annotation only | P3-2 |
| S1136 | Normal | `cgolden/record/position-other-source` | use the default always | P3-2 |
| S1225 | No-effect condition | `cgolden/record/position-other-source-coverage` (line 9 of another source, 3 declared lines: no `REC006`, `refs` 0) | count it for the default source | P4-2 |
| S1137 | Boundary | `cgolden/record/position-bad` (`3..1`, `0`, unknown part) | swap a reversed range silently | P3-2 |
| S1138 | No-effect condition | `cgolden/record/position-location-text` | count `location` as a position | P3-2 |
| S1139 | Normal | `cgolden/record/position-pipe` | give positions to the last step only | P3-2 |
| S1140 | Normal | `cgolden/record/position-servings` | ignore parts on the header | P3-2 |
| S1141 | Normal | `cgolden/record/source-missing` (no source; no `@lines`) | require the source only | P3-3 |
| S1141 | Boundary | `cgolden/record/source-missing` (two recipes in one file, one diagnostic each) | report once per file | P3-3 |
| S1142 | Normal | `cgolden/record/position-missing` | skip ingredient lines | P3-3 |
| S1143 | Normal | `cgolden/record/position-missing` | skip statements | P3-3 |
| S1144 | Normal | `cgolden/record/position-missing-servings` | skip the servings | P3-3 |
| S1145 | No-effect condition | `cgolden/record/position-no-step` | ask every statement | P3-3 |
| S1146 | Normal | `cgolden/record/position-plate` | skip plates | P3-3 |
| S1227 | No-effect condition | `cgolden/record/marks-wrong-place` (each case of the item) | read `@lines` on a statement | P3-5 |
| S1230 | Normal | `cgolden/record/position-nested-blocks` (`if`, `for`, `repeat 2`, `parallel`) | skip statements inside blocks | P3-3 |
| S1235 | Time and order | `cgolden/record/header-annotation-order` (the same output as `coverage`) | read `@notRecorded` before `@lines` is known | P3-5 |
| S1147 | Boundary | `cgolden/record/position-outside` (count + 1; a part with 0 lines) | allow count + 1 | P3-3 |
| S1148 | Normal | `cgolden/record/annotation-unknown` | accept every name | P3-4 |
| S1148 | No-effect condition | `cgolden/record/annotation-defined` (a `property` used as `@whileWarm`) | check built-in names only | P3-4 |
| S1149 | Real path | `G-RC-ANNOTATIONS` | drop `useWithin` from the list | P3-4 |
| S1150 | Fixed premise | `G-RC-ANNOTATIONS` (the plan's list against `debug annotations`) | add a name to the core only | P3-4 |
| S1151 | Normal | `cgolden/record/not-recorded` | read single lines only | P3-5 |
| S1152 | Data completeness | `cgolden/record/not-recorded-kinds` (all eight) | drop `parallelAction` | P3-5 |
| S1152 | Boundary | `cgolden/record/not-recorded-bad` (unknown kind; no kind) | default a missing kind to `other` | P3-5 |
| S1153 | Boundary | `cgolden/record/not-recorded-bad` (`other` without a reason) | never ask for a reason | P3-5 |
| S1154 | Normal | `cgolden/record/not-recorded-twice` | keep one per line | P3-5 |
| S1155 | No-effect condition | `cgolden/record/not-recorded-and-referred` | report a conflict | P3-5 |
| S1156 | Boundary | `cgolden/record/not-recorded-outside` | skip the range check | P3-5 |
| S1157 | Time and order | `cgolden/record/coverage` | order by first reference | P4-2 |
| S1158 | Normal | `cgolden/record/coverage-shared` (two ingredient lines, one position; one statement over two steps; two statements on one step) | count a position once | P4-2 |
| S1159 | Normal | `cgolden/record/coverage` (one line of each status) | let `notRecorded` win over `referred` | P4-2 |
| S1160 | Data completeness | `cgolden/record/not-recorded-and-referred-record` (`debug record`) | list kinds for `notRecorded` status only | P4-2 |
| S1234 | Data completeness | `cgolden/record/coverage-json`, `G-RC-ORIGINS` (the schema) | leave out `refs` | P4-2 |
| S1161 | Normal | `cgolden/record/coverage-uncovered` | report the first uncovered line only | P4-2 |
| S1162 | Data completeness | `cgolden/record/coverage-parts` (an uncovered heading, ingredient line and note) | cover steps only | P4-2 |
| S1163 | Normal | `cgolden/record/amount-missing` | accept a missing amount | P2-1 |
| S1164 | No-effect condition | `cgolden/record/amount-missing` (the three ways to resolve it) | report `[unstated]` | P2-1 |
| S1165 | Normal | `cgolden/record/servings-missing` | accept a recipe without servings | P2-1 |
| S1166 | No-effect condition | `cgolden/record/servings-ways` | ask supplied servings for a position | P3-3 |
| S1167 | Real path | `cgolden/record/whole` | report `REC004` for a supplied step | P4-4 |
| S1168 | Real path | `cgolden/record/whole-bare` | report two diagnostics for one place | P4-4 |
| S1217 | Normal | `cgolden/record/buy-inline` (`with: buy Salt`, a `buy` in a plate) | accept `buy` in a record | P2-1 |
| S1217 | No-effect condition | `G-RC-GOLDEN-SAME` (the existing goldens with `buy`) | report it without the switch | P2-1 |
| S1169 | Normal | `cli/record-emit` | print the section for the first recipe only | P4-2 |
| S1170 | No-effect condition | `cli/record-emit`, `G-RC-GOLDEN-SAME` | always print `"record":[]` | P4-2 |
| S1171 | Normal | `cgolden/record/origins-text` | leave coverage out of the text | P4-1 |
| S1172 | Data completeness | `cgolden/record/origins-json` | leave out `lines` | P4-2 |
| S1173 | Time and order | `cgolden/record/origins-text` | print arguments in written order | P4-1 |
| S1174 | Data completeness | `cgolden/record/origins-json` | leave out `node` | P4-2 |
| S1175 | Data completeness | `G-RC-ORIGINS` (the schema enumeration) | print `vague` for `statedVague` | P4-3 |
| S1176 | Normal | `cgolden/record/origin-servings` (a number, a range `2..4`, supplied, unstated, none) | print `stated` for supplied servings | P4-1 |
| S1177 | Normal | `cgolden/record/origin-ingredient` | print `stated` for a supplied line | P4-1 |
| S1178 | Normal | `cgolden/record/origin-amount` (each row of the item) | print `stated` for a supplied split | P4-1 |
| S1179 | No-effect condition | `cgolden/record/origin-step` (a list argument and a composite action) | list `combine` as a slot | P4-1 |
| S1218 | Normal | `cgolden/record/origin-composite-call` (a default taken, a written value, a position) | give the call no slot | P4-1 |
| S1229 | Normal | `cgolden/record/origin-plate`, `cgolden/record/origin-choose` | give a plate a `duration` slot | P4-1 |
| S1229 | Real path | `cgolden/record/position-plate-coverage` (a plate's position counts in `refs`) | leave plates out of the coverage | P4-2 |
| S1180 | Normal | `cgolden/record/origin-argument` (each row of the item) | print `stated` for a default | P4-1 |
| S1181 | Normal | `cgolden/record/origin-duration` (a stated time; `peel` with `uses ... for`; the `until:` estimate; unstated without an estimate) | print `stated` for an estimate | P4-1 |
| S1182 | No-effect condition | `cgolden/record/origin-mixture` (a mixture has no amount slot) | add a slot for the mixture | P4-1 |
| S1183 | Data completeness | `cgolden/record/origins-json` | give `duration` the statement's positions | P4-2 |
| S1184 | Data completeness | `G-RC-ORIGINS` | skip `argument` slots in the checker | P4-3 |
| S1184 | Boundary | `cgolden/record/category-allow-record` (`--allow REC`: `result ok`, and `debug record` still shows the `null` origins) | fill `null` origins when a diagnostic is allowed | P4-1 |
| S1185 | No-effect condition | `cgolden/record/origins-off` (the file of `origins-text` plus a vague time and `until:` without a time; the same slots, `checks` off) | print `null` origins when off | P4-1 |
| S1186 | Fixed premise | `G-RC-ORIGINS` (every JSON record output against the schema) | add a key the schema lacks | P4-3 |
| S1187 | Data completeness | `G-RC-ORIGINS` (the lists against §4.14; every enumeration value appears in some output), `sh/test-check-record-origins.sh` | drop `parallelAction` from the schema | P4-3 |
| S1188 | No-effect condition | `cli/record-emit` | add the section to SARIF | P4-2 |
| S1189 | Data completeness | `G-RC-DIAG` | insert a code in the middle | P1-1 |
| S1190 | Normal | `cgolden/record/category-allow` (`--allow REC`, `--warn REC`, `@allow(amountMissing)`) | leave `REC` out of the category match | P2-1 |
| S1191 | Data completeness | `rtest/record-diag-text` (every diagnostic; `TYP025` with each problem of S1226), `G-RC-DIAG` | drop one `ja` phrase | P5-1 |
| S1226 | Data completeness | `cgolden/record/mark-problems` (one case per problem), `G-RC-ORIGINS` (the lists) | print free text as the problem | P3-1 |
| S1192 | Docs match | `G-RC-DIAG` (`G-TYPE-SEVERITY-TABLE`), `G-RC-DOCS` | leave `REC` out of the table | P5-3 |
| S1193 | Normal | `cgolden/record/time-unknown` (each of the three ways; diagnostics) | treat `[unstated]` as 0 s | P2-4 |
| S1193 | Boundary | `cgolden/record/time-unknown-off` (`[unstated]` with the checks off: `SCH012`) | apply it in record mode only | P2-4 |
| S1194 | Normal | `cgolden/record/schedule-none` (`debug schedule`) | print a total of 0 | P2-4 |
| S1194 | Data completeness | `cgolden/record/schedule-none-json` (JSON `schedules`) | keep an empty entry for the dish | P2-4 |
| S1195 | Normal | `cgolden/record/schedule-none` | give `SCH009` too | P2-4 |
| S1195 | Boundary | `cgolden/record/schedule-none-two` (two steps, two `SCH012`) | report once per dish | P2-4 |
| S1196 | Real path | `cgolden/record/schedule-menu` | schedule the menu with 0 s | P2-4 |
| S1197 | No-effect condition | `cgolden/record/schedule-estimate` | refuse whenever the time is not written | P2-4 |
| S1198 | Data completeness | `cgolden/record/time-unknown-ir` (`debug ir`), `cgolden/record/time-unknown-json` | print `0 s` | P2-4 |
| S1224 | Data completeness | `cgolden/record/time-unknown-json`, `G-RC-ORIGINS` (the schema) | keep `time` as a range of 0 | P2-4 |
| S1232 | Side effects | `cgolden/record/schedule-none-sibling` (two dishes, one scheduled), `cgolden/record/schedule-none-serve-within` | drop every schedule of the file | P2-4 |
| S1199 | Fixed premise | `cgolden/record/vague-argument-off-schedule` (`debug schedule`) | make a vague time unknown in every mode | P2-4 |
| S1200 | Normal | `rtest/record-text` | fall back to `amount-any` | P5-2 |
| S1201 | Normal | `rtest/record-text` | print the duration of 0 | P5-2 |
| S1202 | No-effect condition | `rtest/record-text` | print the size `?` | P5-2 |
| S1203 | Real path | `rtest/record-no-schedule` | print an empty table | P5-2 |
| S1222 | Real path | `rtest/record-html`, `rtest/record-shopping` | fall back to `amount-any` in `quantityText` | P5-2 |
| S1223 | Real path | `rtest/record-html` | draw an empty chart | P5-2 |
| S1233 | Real path | `rtest/record-text` (a comparison, a standard cue, an added cue, `until: [any amount]`) | keep "(about 0 s)" | P5-2 |
| S1204 | Normal | `cgolden/record/continuation` (recipe header, ingredient line, statement, serve, plate, plate item) | skip newlines on headers only | P1-5 |
| S1204 | Boundary | `cgolden/record/continuation-menu`, `cgolden/record/continuation-decl` (`enum` and `@cue`), `cgolden/record/continuation-blank-comment` | stop at a blank line | P1-5 |
| S1205 | Normal | `cgolden/record/continuation-ast` (the same `debug ast` as `continuation-one-line`) | attach to the next item | P1-5 |
| S1206 | No-effect condition | `cgolden/record/continuation-alone` (first in a file), `cgolden/record/continuation-block-first` | accept a leading `@` | P1-5 |
| S1207 | Fixed premise | `cgolden/record/sum-two-units` | read the second quantity as a new item | P2-1 |
| S1208 | Docs match | `G-RC-SYNTAX-LIST`, `G-RC-DOCS` (`G-DS-REFERENCE` runs the examples) | leave `@notRecorded` out of the reference | P5-3 |
| S1209 | Docs match | `G-RC-DOCS` (`G-DS-REFGEN`) | do not regenerate | P5-3 |
| S1210 | Docs match | `G-RC-DOCS` | leave `[unstated]` out of the concept | P5-3 |
| S1211 | Docs match | `G-RC-DOCS` | leave `debug record` out | P5-3 |
| S1212 | Data completeness | `G-RC-GRAMMAR` | add the rule to the EBNF only | P1-3 |
| S1213 | Fixed premise | `G-RC-GOLDEN-SAME`, `G-CORE-GOLDEN` | print `"unstated":false` on every ingredient | every phase gate |
| S1214 | Fixed premise | `G-RC-STD-SAME` | add a cue to `prelude.ozen` | every phase gate |
| S1215 | Data completeness | `G-RC-NO-THIRD-PARTY` | name a test recipe after a trial recipe | P0-3 |
| S1216 | Docs match | `G-RC-VERSION` | bump one of the version files only | P6-2 |

Coverage check (run after P0 and in P6-3; empty output, then 0): the two commands of `G-RC-COVERAGE`.

Findings of the independent audit that were not taken, with reasons:

- Derive the built-in annotation list from the places where the core reads an annotation, instead of the list that `ozen debug annotations` prints — not taken: the core reads annotations in about forty separate places with no table; `scripts/check-record-annotations.sh` runs the record checks over every existing file, which is what catches a missing name.
- A WebAssembly test for the package field — not taken as a new test: the bridge is expected to have no code of its own for package files (to confirm in P1-2, §9); if it has, a case is added there.
- Run the 48 trial records with `--record` and compare the diagnostics with the trial's comment marks, as a goal — not taken as a goal: the records are outside the repository and must stay there. It is noted in §9 as an optional check by hand after P4.
- Report `REC004` once per statement inside `repeat` instead of once per evaluation — not taken: existing diagnostics repeat per evaluation, and changing that belongs to the design of diagnostics as a whole (S1230 fixes the behaviour).

---

## 6. Phases and tasks

Notation:

- **Completion conditions** are goal IDs from §5 or commands with one result. The prerequisite goals are implied in every task.
- Status is reported per area and per level of D155. Example: "unstated value: goldens pass locally (gcc release). positions: not started. CI: last push passed."
- Commits are per task or smaller logical units. For each golden: first the `.ozen` and `.out` (failing), then the implementation, never together (`G-RF-SEPARATION`, `G-REPO-RED-FIRST`).
- Each task's completion condition includes the mutation checks of the coverage rows it owns. Break the implementation as the row says, see the test fail, restore it, see it pass, and record the result in §9 as `- Mutation check: <test> — <S item>, <how>: <what failed>. Restored: pass`.

### Phase 0 — Decisions and preparation (no change to features)

| ID | Task | Completion condition |
| --- | --- | --- |
| P0-1 | Decide D137–D155 with the user and write the decisions into this file. Rewrite the §4 items that a decision changes | `G-RC-DECISIONS`, `G-RC-COVERAGE` |
| P0-2 | Tag `ds-end` and close `DS` (D139). Tag `rc-start`. Build `build/core-gcc-release/ozen` (D155) and record in §9: the build command, the goals that pass and fail at `rc-start` on this machine, the golden count, and any other work in progress | `git rev-parse --verify rc-start` succeeds; `G-RC-RANGES`; §9 has the four records |
| P0-3 | Add `PLAN_RC`, `RC` and the `goal_G_RC_*` functions to `scripts/goals.sh`. Add this plan's line to `.rulesync/rules/overview.md` and generate. Write `scripts/rc-third-party.txt` and `scripts/rc-scope.txt` | `G-RC-COVERAGE`, `G-RC-NO-THIRD-PARTY`, `G-DS-RULESYNC`; `scripts/goals.sh --list` names every `G-RC-*` of §5.1 |

### Phase 1 — The switch and the values

| ID | Task | Completion condition |
| --- | --- | --- |
| P1-1 | Register the twelve diagnostics and the category `REC` (codes, names file, phrases in both languages, `ARG_KINDS`, the category list of the registry check). Add `ozen debug annotations` | `G-RC-DIAG` (except the concept table), `G-RC-ANNOTATIONS` (second line) |
| P1-2 | `--record`, the package field, and the scope of the switch | the `switch-option`, `switch-debug`, `switch-bad-value`, `switch-off-bare` goldens, `cli/record-package`, `cli/record-dependency`, `cli/record-modes`, `cli/record-usage` |
| P1-3 | `[unstated]`: grammar, the three places, `TYP024`, the text and JSON `ir` | the `unstated-*` goldens, `G-RC-GRAMMAR` |
| P1-4 | `[any amount]` as an argument under the record checks, and `TYP003` under them (D147) | the `vague-*` goldens |
| P1-5 | Annotations on continuation lines (D151) | the `continuation*` goldens, `G-RC-GRAMMAR` |
| P1-6 | Phase gate, then one push of the phase's commits (D138) | every prerequisite goal, `G-RC-GOLDEN`, `G-RC-GOLDEN-SAME`, `G-RC-STD-SAME`; `G-REPO-NOREPLY` before the push and `G-PLAT-CI` after it |

### Phase 2 — Slot checks, end conditions and unknown time

| ID | Task | Completion condition |
| --- | --- | --- |
| P2-1 | `REC001`, `REC002`, `REC003`, `REC008`, the category settings, and the golden for the sum of two units | the `amount-missing`, `default-*` (except `default-time-with-until`), `servings-missing`, `buy-inline`, `category-allow`, `sum-two-units` goldens |
| P2-2 | `until:` without a time under the record checks; `until: [any amount]` | the `until-no-time`, `until-vague*`, `until-unstated`, `default-time-with-until` goldens |
| P2-3 | `@cue` enums (D153) | the `cue-*` goldens |
| P2-4 | Unknown time: `SCH012`, no schedule, `?`, `timeUnknown` and a `null` time (D148) | the `time-unknown*`, `schedule-*`, `vague-argument-off-schedule` goldens |
| P2-5 | Phase gate | as P1-6 |

### Phase 3 — Marks, sources and positions

| ID | Task | Completion condition |
| --- | --- | --- |
| P3-1 | `@supplied` and `TYP025` with its problems | the `supplied-*`, `mark-problems`, `marks-off` goldens |
| P3-2 | `@source` on headers, ingredient lines and statements; `@lines`; positions | the `source-default`, `source-undefined`, `lines*`, `position-range`, `position-several`, `position-other-source`, `position-bad`, `position-location-text`, `position-pipe`, `position-servings` goldens |
| P3-3 | `REC004`, `REC005`, `REC006` | the `source-missing`, `source-not-a-source`, `position-missing*`, `position-no-step`, `position-plate`, `position-nested-blocks`, `position-outside`, `servings-ways`, `switch-off-marked` goldens |
| P3-4 | `NAM022` and the built-in list | `cgolden/record/annotation-unknown`, `cgolden/record/annotation-defined`, `sh/test-check-record-annotations.sh`, `G-RC-ANNOTATIONS` |
| P3-5 | `@notRecorded`, marks in the wrong place, the order of header annotations | the `not-recorded*` (except `-record`), `marks-wrong-place`, `header-annotation-order` goldens |
| P3-6 | Phase gate | as P1-6 |

### Phase 4 — The record output and coverage

| ID | Task | Completion condition |
| --- | --- | --- |
| P4-1 | The slot table and `ozen debug record` | the `origin-*`, `origins-text`, `origins-off`, `origins-scale`, `until-no-time-record`, `category-allow-record` goldens |
| P4-2 | Coverage, `REC007`, `--emit record` and the schema | the `coverage*`, `origins-json`, `not-recorded-and-referred-record`, `position-plate-coverage`, `position-other-source-coverage` goldens, `cli/record-emit` |
| P4-3 | `scripts/check-record-origins.py` and its self-test | `G-RC-ORIGINS`, `G-RC-SELFTESTS` |
| P4-4 | The whole record and its bare form | `cgolden/record/whole`, `cgolden/record/whole-bare` |
| P4-5 | Phase gate | as P1-6, and `G-API-SCHEMA` |

### Phase 5 — Renderer and documents

| ID | Task | Completion condition |
| --- | --- | --- |
| P5-1 | Renderer text for the twelve diagnostics | `rtest/record-diag-text`, `G-STD-PHRASES` |
| P5-2 | Recipe text and the kinds without a schedule (D148) | `rtest/record-text`, `rtest/record-no-schedule`, `rtest/record-html`, `rtest/record-shopping`, `G-RC-REND`, `G-REND-TESTS` |
| P5-3 | `reference/language.md`, the generated reference, `docs/concept.md`, `cli/README.md` | `G-RC-DOCS`, `G-RC-SYNTAX-LIST`, `G-RC-DIAG` |

### Phase 6 — Finish

| ID | Task | Completion condition |
| --- | --- | --- |
| P6-1 | Check `scripts/rc-scope.txt` against the files changed | `G-RC-SCOPE` |
| P6-2 | The version, as D154 decides | `G-RC-VERSION` |
| P6-3 | Coverage audit. Add the facts found in §9 to the coverage table, and check each §8 item again. Ask an independent subagent to list claims without a test and §8 items that a command can decide; give it §1, §4, §8, the coverage table and the test sources. Take each finding, or write why not at the end of §5.3 | `G-RC-COVERAGE`, `G-RC-PLAN-NAMES`, `G-RC-PLAN-TESTS`, `G-RC-AUDIT` |
| P6-4 | Final gate. Push (D138) and read CI. Hand §8 and the report of §7.1 to the user | every prerequisite goal and every `G-RC-*` goal; `G-PLAT-CI` after the push |

---

## 7. Dependencies between phases

- Phase 0 comes first. P0-3 must come before any `G-RC-*` goal is run.
- P1-1 comes before every task that produces a diagnostic. P1-2 comes before every task whose behavior depends on the switch.
- P1-3 comes before P1-4, P2-1 and P2-4 (they read the unstated value). P1-5 is independent of the rest of Phase 1.
- Phase 2 and Phase 3 are independent of each other after Phase 1. Both edit `core/src/sema.c`, so they are done one task at a time, not in parallel.
- Phase 4 needs Phases 2 and 3 (the slot table reads the marks and the checks' results).
- P5-1 needs P1-1 only. P5-2 needs P2-4. P5-3 needs Phase 4.

### 7.1 What the final report holds

- The list of the decided syntax (the marks, positions, kinds, and how the checks are turned on), as the table of §4.14 with one example each; it is copied into the transcription rule book later.
- For each requirement R1–R13, the goals and S items that satisfy it:

| Requirement | S items | Goals |
| --- | --- | --- |
| R1 Origin | S1169–S1188, S1218, S1229 | `G-RC-GOLDEN`, `G-RC-ORIGINS`, `G-RC-CLI` |
| R2 Unstated | S1083–S1096, S1193–S1203, S1222–S1224, S1232–S1233, S1236 | `G-RC-GOLDEN`, `G-RC-REND` |
| R3 Supplied | S1120–S1129, S1176–S1179 | `G-RC-GOLDEN` |
| R4 Source | S1130–S1131, S1134–S1150 | `G-RC-GOLDEN`, `G-RC-ANNOTATIONS` |
| R5 Coverage | S1132–S1133, S1157–S1162 | `G-RC-GOLDEN` |
| R6 Record checks | S1076–S1082, S1112–S1114, S1163–S1168, S1213, S1217, S1219–S1221, S1231 | `G-RC-GOLDEN`, `G-RC-CLI`, `G-RC-GOLDEN-SAME` |
| R7 Documents | S1189–S1192, S1208–S1212 | `G-RC-DOCS`, `G-RC-SYNTAX-LIST`, `G-RC-DIAG`, `G-RC-GRAMMAR` |
| R8 No defaults | S1102–S1111 | `G-RC-GOLDEN` |
| R9 Vague arguments | S1097–S1101, S1180 | `G-RC-GOLDEN` |
| R10 Kinds of supplied | S1121–S1126, S1187 | `G-RC-GOLDEN`, `G-RC-ORIGINS` |
| R11 Not recorded | S1151–S1156, S1159–S1160 | `G-RC-GOLDEN` |
| R12 Positions for every part | S1132–S1140, S1146, S1157–S1162 | `G-RC-GOLDEN` |
| R13 Unstated servings | S1086, S1092, S1165–S1166, S1176 | `G-RC-GOLDEN` |

- The requirements not taken up, with reasons: none of R1–R13 is left out. The limits are those of "Out of scope" in §1.
- One reading to confirm with D144: R1, R3 and R4 say "in the IR and in the JSON output". This plan keeps the marks in the core's tables and outputs them in the `record` section and `debug record`, not inside the `ir` section, so that existing `ir` outputs stay the same.

---

## 8. Checks handed to the user

Claude covers the machine-checked goals. The user checks these:

- Transcribe two or three source recipes of your own choice with the new syntax and `--record`, and see whether every place the check stops at is a place you would want to mark — Reason a command cannot decide: the tests prove that each rule fires as specified, not that the rules match how a transcriber reads a real source.
- Read the phrases of the twelve diagnostics and the two new recipe-text phrases in Japanese and English — Reason a command cannot decide: the tests check that each phrase exists and takes its arguments, not that the wording is natural.
- Read the new section of `reference/language.md` as the source for the transcription rule book — Reason a command cannot decide: `G-RC-SYNTAX-LIST` proves that every item is named, not that the explanation is enough to write a record from.
- When reviewing a record, compare its positions with the source text — Reason a command cannot decide: the core checks that a position is inside the declared lines, not that the line of the source says what the record says; the source text is not in the record.

---

## 9. Facts to record during the work

(Append results here with dates.)

- Base values at `rc-start` (P0-2): the build command, the goals that pass and fail on this machine, the golden count
- Inputs of §3.2 (2026-11-24), each checked with `ozen check` unless noted: row 1 `recipe a(servings: 2) { ingredients { sugar: Sugar; onion: 1 pcs } ... mix([s, sugar]) }`; row 2 `cut(slice, [any amount])`, `fry([any amount])`, `simmer([f, salt], [any amount])` with `salt: [any amount]` (`check`, `debug ir`, `debug schedule`); row 3 `onion |> peel |> cut(slice)` with `check --format json --emit ir`; row 4 the three annotations of the table; row 5 `action steamIt(x: T, time: Time in 0 s<.., heat: Heat = low) -> T<doneness: done>` on `ingredient Jelly { axes: doneness, prep }`, and `simmer(until: thickened)`; row 6 `recipe a { ingredients { onion: 1 pcs } ... }`
- To confirm in P1-3: what requirement a heating action records for an unknown `heat` (today `fry` shows `FryingPan<heated: heat>`). If an unknown enum value breaks the requirement, the requirement is left without the argument, and the result is written here
- To confirm in P2-4: where `core/src/sched.c` reads a node's time, and whether a dish can be left out of `schedules` without touching the menu path. If it cannot, D148's first alternative (0 s with a flag) is the fallback, and the user is asked first
- To confirm in P5-2: what `ozen render --kind schedule` prints today for a result without a schedule for a dish
- To confirm in P1-2: where the core reads `package` fields, and whether the WebAssembly bridge passes the package file unchanged
- Mutation checks (each task)
- Coverage audit (P6-3): `- Coverage audit: N findings, M taken`
- Plan audit before P0 (2026-11-24, an independent subagent): 32 findings; 28 taken in full, 4 in part (the parts not taken are at the end of §5.3)
- Optional check by hand after P4: run the trial records (outside the repository) with `--record` and compare the counts of diagnostics with the trial's comment marks

---

## 10. Risks

- **R1. `[unstated]` changes the reading of a file that defines the name `unstated`**: only the exact three tokens are the literal (S1096), and D140 names the alternative `[none stated]`, which is a syntax error today.
- **R2. A record check fires in default mode and changes an existing output**: `G-RC-GOLDEN-SAME` and `G-CORE-GOLDEN` run at every phase gate, and `switch-off-bare` holds a file full of unmarked places with the checks off.
- **R3. Leaving a dish out of the schedule is not a small change in `sched.c`**: P2-4 checks first (§9). The fallback is named there and needs the user's yes.
- **R4. The one golden that lists all diagnostics changes** (`api/local-not-listed.out`): this is an exception to "existing goldens do not change" that cannot be avoided when diagnostics are added. `G-RC-GOLDEN-SAME` allows added lines in that file only.
- **R5. This machine cannot run every goal** (D155): goals that need other compilers or platforms are read from CI after a push. Each phase gate ends with a push (D138), so they are read at every gate; a report made between gates lists them as "not run here".
- **R6. The closed sets of kinds are too small for real sources**: `other` with a reason keeps a record finishable, and its count shows when a kind should be added (D149).
- **R7. `NAM022` reports an annotation that a feature reads but the list lacks**: `G-RC-ANNOTATIONS` runs the record checks over every existing golden, sample and standard library file, and compares the plan's list with the core's.
- **R8. Positions are checked for form, not for truth**: the core can check that a position is inside the declared lines, not that line 3 of the source says what the record says. That stays with the transcriber and the reviewer (§8).
