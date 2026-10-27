// SPDX-License-Identifier: MIT OR Apache-2.0
import test from 'node:test';
import assert from 'node:assert/strict';
import { join } from 'node:path';
import { Phrases } from '../lib/phrases.mjs';
import { renderRecipe } from '../lib/recipe.mjs';
import { root, result, stdPhrases, projectPhrases, expected } from './helpers.mjs';

const dir = join(root, 'samples', 'curryRice');

for (const lang of ['ja', 'en']) {
  test(`the curry and rice recipe in ${lang} lists steps in dependency order`, () => {
    const ph = new Phrases([stdPhrases(), projectPhrases(dir, 'curryRice')], { lang });
    assert.equal(renderRecipe(result(dir, ['--entry', 'curryRice']), ph), expected(`recipe-text.${lang}.txt`));
    assert.deepEqual(ph.stats, { fallbacks: 0, errors: 0, unresolved: 0 });
  });
}
