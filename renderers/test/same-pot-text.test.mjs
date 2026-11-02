// SPDX-License-Identifier: MIT OR Apache-2.0
import test from 'node:test';
import assert from 'node:assert/strict';
import { join } from 'node:path';
import { Phrases } from '../lib/phrases.mjs';
import { renderRecipe } from '../lib/recipe.mjs';
import { root, result, stdPhrases } from './helpers.mjs';

const inputs = join(root, 'renderers', 'test', 'inputs');
const recipe = (entry, lang = 'ja') =>
  renderRecipe(result(inputs, ['--entry', entry, 'same-pot.ozen']), new Phrases([stdPhrases()], { lang }));

test('a step that goes on in the same pot adds the new ingredients to the earlier step', () => {
  const ja = recipe('porkSoup');
  assert.match(ja, /^3\. 1 に玉ねぎを加え、弱火で5 分煮込む。$/m);
  assert.doesNotMatch(ja, /豚肉、玉ねぎを/);
  assert.match(recipe('porkSoup', 'en'), /^3\. Add onion to step 1\. Simmer it over low heat for 5 min\.$/m);
});

test('a step that goes on with nothing new is written as going on', () => {
  assert.match(recipe('goldenPork'), /^2\. そのまま豚肉がきつね色になるまで中火で炒める（約 2 分）。$/m);
});

test('a step moved to a new pot is written as before', () => {
  assert.match(recipe('newPot'), /^3\. 豚肉、玉ねぎを弱火で5 分煮込む。$/m);
});
