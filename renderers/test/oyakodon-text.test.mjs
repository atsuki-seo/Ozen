// SPDX-License-Identifier: MIT OR Apache-2.0
import test from 'node:test';
import assert from 'node:assert/strict';
import { join } from 'node:path';
import { Phrases } from '../lib/phrases.mjs';
import { renderRecipe } from '../lib/recipe.mjs';
import { root, result, stdPhrases, projectPhrases } from './helpers.mjs';

test('the oyakodon cracks and beats the egg and adds it until it is lightly cooked', () => {
  const dir = join(root, 'samples', 'T2');
  const ph = new Phrases([stdPhrases(), projectPhrases(dir, 'T2')], { lang: 'ja' });
  const text = renderRecipe(result(dir, ['--entry', 'oyakodon']), ph);
  assert.match(text, /^4\. 卵をボウルに割り入れ、溶きほぐす。$/m);
  assert.match(text, /^5\. 3 に溶き卵を加え、卵が半熟になるまで弱火で煮る（約 1〜2 分）。$/m);
});
