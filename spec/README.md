# The vocabulary table

The vocabulary table ([`spec/vocab/`](vocab/)`*.yaml`) holds the standard vocabulary of
Ozen before the core exists: dimensions, units, state axes, traits, ingredients with
parts, aliases, actions with their requirements and effects, capabilities and equipment
kinds, dirt and washing rules, vessels, and the provenance of every value.

## Checks

It is checked by [`tools/vocabcheck/`](../tools/vocabcheck/) (Python 3 with PyYAML and
jsonschema):

```
python3 tools/vocabcheck/vocabcheck.py schema       # shape of the 11 files (JSON Schema)
python3 tools/vocabcheck/vocabcheck.py check        # references, names, units, rules
python3 tools/vocabcheck/vocabcheck.py lists spec/coverage/concept-lists.yaml spec/coverage/concept-relations.yaml
python3 tools/vocabcheck/vocabcheck.py sample spec/samples/curryRice.yaml
python3 tools/vocabcheck/sources.py spec/vocab spec/sources/search-log.yaml
```

`sample` type-checks the curry-and-rice sample on the table (states, linear use, safety,
vessels) and compares the result with the expectations in the sample.

## Sources

Refuse rates come from the Standard Tables of Food Composition in Japan
([`data/mext-fct-2023/`](../data/mext-fct-2023/), re-extracted with
`tools/fct/extract.py --check --xlsx <file>`). Values without a primary source are
provisional ranges, and [`spec/sources/search-log.yaml`](sources/search-log.yaml)
records the search for each of them.

[`spec/notes/prior-work.md`](notes/prior-work.md) maps the models of four papers to
Ozen.
