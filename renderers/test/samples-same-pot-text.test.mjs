// SPDX-License-Identifier: MIT OR Apache-2.0
import test from 'node:test';
import assert from 'node:assert/strict';
import { join } from 'node:path';
import { Phrases } from '../lib/phrases.mjs';
import { renderRecipe } from '../lib/recipe.mjs';
import { root, result, stdPhrases, projectPhrases } from './helpers.mjs';

const dir = join(root, 'samples', 'menuC');
const recipe = (entry) =>
  renderRecipe(result(dir, ['--entry', entry]), new Phrases([stdPhrases(), projectPhrases(dir, 'menuC')], { lang: 'ja' }));

test('the miso soup goes on in the pot where the water boiled', () => {
  assert.match(recipe('misoSoup'), /^3\. 2 にだし、キャベツを加え、弱火で5 分煮込む。$/m);
});

test('the stir fry goes on in the pan where the pork was fried', () => {
  assert.match(recipe('stirFry'), /^4\. 2 にキャベツを加え、強火で3 分炒める。$/m);
});
