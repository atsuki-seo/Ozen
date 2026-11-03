# Notes for AI agents

This folder is an Ozen cookbook: recipes, menus and kitchens written in `.ozen` files.
`package.ozen` names the package. It was started with `ozen` @VERSION@.

## Before writing

Read the guide for this version of Ozen, which holds the syntax, every standard action and
every diagnostic in one file:

https://github.com/atsuki-seo/Ozen/blob/v@VERSION@/reference/ai-guide.md

## After every change

1. Run `ozen check --format json FILE` and read every diagnostic.
2. Fix every error and run the check again, until it reports 0 errors.
3. Run `ozen render --kind recipe FILE` and read the recipe text to see that it says what you meant.

Do not guess the names of actions, ingredients or units: take them from the guide, or run
`ozen debug definitions` to list them.
