// SPDX-License-Identifier: MIT OR Apache-2.0
import test from 'node:test';
import assert from 'node:assert/strict';
import { join } from 'node:path';
import { Phrases } from '../lib/phrases.mjs';
import { renderRecipe } from '../lib/recipe.mjs';
import { root, result, stdPhrases } from './helpers.mjs';

const inputs = join(root, 'renderers', 'test', 'inputs');

function recipe(lang, entry) {
  return renderRecipe(result(inputs, ['--entry', entry, 'until.ozen']), new Phrases([stdPhrases()], { lang }));
}

test('until is written as the stage the food reaches, with the estimated time', () => {
  assert.match(recipe('ja', 'softEgg'), /^2\. 卵が半熟になるまで弱火で煮る（約 1〜2 分）。$/m);
  assert.match(recipe('ja', 'goldenOnion'), /^\d\. 玉ねぎがきつね色になるまで中火で炒める（約 5 分）。$/m);
});

test('the subject of until in a mixture is the ingredient that has the time', () => {
  assert.match(recipe('ja', 'eggSoup'), /^\d\. 水、卵を、卵が半熟になるまで弱火で煮る（約 1〜2 分）。$/m);
});

test('the English sentence says until the food reaches the stage', () => {
  assert.match(recipe('en', 'softEgg'), /^2\. Simmer egg over low heat until egg is lightly cooked \(about 1–2 min\)\.$/m);
  assert.match(recipe('en', 'goldenOnion'), /^\d\. Fry onion over medium heat until onion is golden brown \(about 5 min\)\.$/m);
});
