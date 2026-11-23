# Requirements for using Ozen as a checkable record

> Status: requirements, not a plan. This document states what the language and the CLI must
> do so that a recipe written by someone else can be kept as a record that a machine can
> check. It is the input for one implementation plan under `plans/`. It does not decide
> syntax, option names or IR field names; the plan does.

## 1. Purpose

Ozen so far serves a person who writes a new recipe. A second use is to transcribe an
existing recipe (a published text with an ingredient list and numbered steps) into Ozen and
keep the result as a record. A record has to answer, for every value in it, where the value
came from:

- **stated**: the source text says it (with its position in the source);
- **definition**: it comes from a definition outside the record (a unit conversion, a
  density, the weight of one piece, the estimated duration of an action, a state change);
- **supplied**: the transcriber added it because the source does not have it, and says so;
- **unstated**: the source does not say it, and the record keeps that fact instead of a
  value.

The central requirement is the last one. A record must be able to keep "the source does not
say" as a value, and the check must find every place where a value is expected, so that a
transcriber cannot finish a record without marking each such place as stated, supplied or
unstated.

## 2. What the current release does (measured with v0.1.0)

A trial transcribed 48 published recipes with the current release. All 48 checked with
`result ok`, which showed the following gaps.

| # | Behavior today | Consequence for a record |
|---|---|---|
| 1 | An ingredient with a type and no amount (`sugar: Sugar`) passes without a diagnostic, and its amount is empty in the IR | An unstated amount cannot be told from a forgotten one |
| 2 | A time or temperature written as `[any amount]` passes without a diagnostic and becomes 0 s. `TYP003` is reported for ingredient amounts only | 43 heating or resting steps in the trial had no time in the source; only 4 produced a diagnostic |
| 3 | A default argument value (`cut` size `3 mm`, `fry` heat `medium`) cannot be told from a written value in the IR | A number that the source never states enters the record unmarked |
| 4 | A trailing `@source(name, location: "...")` parses and is then dropped. An undefined source name and an unknown annotation give no diagnostic. The IR has neither the source nor a position | The link from the record to the source text is lost |
| 5 | `until:` without a time is the error `TYP022` unless the ingredient has a heating-time property | "Until set", with no time, is how sources usually write; a record needs it to be valid |
| 6 | A recipe without `servings` checks without a diagnostic | A source that does not state servings cannot be recorded as such |
| 7 | An ingredient that a step uses but the source list lacks must be declared like any other | 15 ingredients in the trial were added by the transcriber and look the same as listed ones |

The same trial confirmed what already works and must keep working: an unused listed
ingredient is `LIN005`, an undeclared one `NAM001`, double use `LIN001`, a dimension
mismatch `TYP001`, an unknown unit `NAM011`, a step out of order `TYP006`, a non-positive
amount `TYP021`; and a definition added by the writer (unit, ingredient, action, axis) gets
the same checks with no extra rule.

## 3. Terms

A record is counted in **slots**. Every slot has exactly one origin.

| Slot | Example |
|---|---|
| Servings | 2 servings |
| Ingredient line (that it exists) | sugar |
| Ingredient amount | 75 g |
| Step action (that it exists) | strain |
| Quantity argument of an action | time, temperature, heat, size, end condition |

| Origin | When |
|---|---|
| stated, numeric | The source gives a number with a unit, or a range |
| stated, vague | The source gives a word of degree and no number ("to taste", "a little", "slowly", "to the size you like") |
| unstated | The slot is expected and the source gives neither a number nor a word of degree |
| supplied | The transcriber added it; a kind and a reason go with it |
| definition | An inner value that the source does not talk about: a conversion, a density, a unit weight, the estimated duration of an action, a state change |

Rules that the transcription follows, and that the language must make possible:

- A quantity argument never takes a definition's default value in a record. If the source
  has it, it is stated; if the source has a word of degree, it is stated and vague;
  otherwise it is unstated. This holds for every quantity argument, not only time,
  temperature, heat and amount.
- A step with an end condition and no time has a stated end condition and an unstated time.
- Only quantity arguments can be unstated. A tool that the source does not name is not
  counted; the action's definition decides the tool.
- A step that the source implies but does not write (cracking an egg before beating it) is
  written as a supplied step of the kind "implied step".
- One source line may be the position of several record lines, for example when one listed
  ingredient is used in two steps and so is written as two ingredient lines.
- Every source line is either referred to by the record or declared as not recorded, with
  a reason.

## 4. Requirements

R1 to R7 are the requirements the plan was first given. R8 to R13 were added after the
transcription rules above were fixed.

**R1 Origin.** In the IR and in the JSON output, every amount, time and argument value has
one of the origins: (a) stated in the recipe file, (b) from a definition, (c) supplied by
the transcriber, (d) unstated. The list of origins can be output in a form a machine reads.

**R2 Unstated.** An amount, time, heat or temperature that the source does not state can be
written as a value. It is a different thing from a vague amount.

**R3 Supplied.** An ingredient, amount, step or number of servings that the transcriber
added carries a mark that says so, with a reason. The mark is in the IR and in the JSON
output.

**R4 Source.** A source attached to a recipe, an ingredient line or a step (the source's
name and a position inside the source) is checked and output in the IR and in the JSON
output. An undefined source name is a diagnostic. An unknown annotation is a diagnostic. A
recipe can set a default source.

**R5 Coverage.** The lines of the source can be declared, and the lines that no part of the
record refers to can be reported.

**R6 Record checks.** There is one way to turn on all the checks above. Without it, existing
behavior does not change. With it, "an end condition and no time" is not an error; the time
is unstated.

**R7 Documents.** `reference/`, `docs/concept.md`, the list of diagnostics and `phrases`
(`en`, `ja`) reflect the change.

**R8 No defaults in a record.** With the record checks on, a quantity argument that takes
its definition's default value is a diagnostic. The transcriber resolves it by writing the
stated value, the vague value or unstated. (Extends R1: telling a default apart in the IR
is not enough.)

**R9 Vague values for arguments.** A quantity argument of an action (time, size, heat) can
be written as stated and vague, as an ingredient amount can today, and is told apart from
unstated. (Extends R2, which separates the two for ingredient amounts only.)

**R10 Kinds of supplied.** A supplied mark has a kind that a machine can count, besides the
free-text reason. The kinds needed so far: ingredient, amount, split of one listed
ingredient, servings, implied step. (Extends R3.)

**R11 Not recorded.** A source line can be declared as not recorded, with a kind of reason
that a machine can count, and such a line is told apart from an uncovered line in the
coverage report. The kinds needed so far: tool preparation, description of a picture,
statement of purpose, advice, alternative ingredient or tool, reference to another recipe,
action done in parallel with another step. (Extends R5.)

**R12 Positions for every part of the source.** Positions cover the ingredient list, the
steps, the notes and the heading, not only the steps. Several record lines may share one
position. The coverage report covers all of these parts. (Extends R4 and R5.)

**R13 Unstated servings.** A recipe whose source does not state servings can be recorded
with unstated servings. If this is not taken up, the plan says so, and the transcriber
supplies a number with a reason instead. (Extends R2.)

## 5. Points the plan has to decide

- How the unstated and supplied marks are written: as values or as annotations.
- How stated-and-vague and unstated are told apart for an action argument (R9).
- Whether origins are added to the existing IR schema or output separately, and the schema
  version.
- How the record checks are turned on: a CLI option, a package setting, or both.
- What the renderer and the scheduler do with a recipe that has unstated values. An unstated
  time must not enter the schedule as 0 s.
- How the kinds of R10 and R11 are written, and whether the sets of kinds are closed.
- How positions for the ingredient list, the notes and the heading are written (R12).
- Whether an amount written as a sum of two units ("2 tbsp and 1 tsp") is taken up. Without
  it, the transcriber writes two ingredient lines with the same position.
- How an end condition is written when no definition has that state ("until sticky").

## 6. Out of scope

- Approximate amounts ("about 250 g").
- Using one listed ingredient in several steps without splitting the line; separating an egg
  into white and yolk.
- Alternative ingredients and tools; optional steps.
- A mixture that carries state axes.
- Additions to the standard library (vocabulary, densities).
- Export to schema.org JSON-LD; display names for added vocabulary.
- Choosing a set of definitions from outside the record.
- Changes to the scheduler, except the least that keeps an unstated time out of the
  schedule as 0 s.

A transcription under these limits declares what it could not record (R11), so the limits
are counted rather than hidden.

## 7. Constraints on the plan

- Existing default behavior, existing goals and existing goldens do not change. The plan
  has a goal that checks this.
- Test material is written new. Transcriptions of third-party recipes are not added to this
  repository.
- Each requirement's current behavior is measured again when the plan is written, and any
  difference from §2 is reported.
