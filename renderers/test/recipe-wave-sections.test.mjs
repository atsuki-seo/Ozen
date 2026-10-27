// SPDX-License-Identifier: MIT OR Apache-2.0
import test from 'node:test';
import assert from 'node:assert/strict';
import { join } from 'node:path';
import { Phrases } from '../lib/phrases.mjs';
import { renderRecipe } from '../lib/recipe.mjs';
import { root, result, stdPhrases, projectPhrases } from './helpers.mjs';

test('a recipe without times is divided by the names of the waves', () => {
  const dir = join(root, 'samples', 'menuD');
  const ph = new Phrases([stdPhrases(), projectPhrases(dir, 'menuD')], { lang: 'ja' });
  const text = renderRecipe(result(dir, ['--entry', 'dinner', '--options', 'family']), ph);
  const main = text.indexOf('\n### メイン\n');
  const dessert = text.indexOf('\n### デザート\n');
  assert.ok(main > 0 && dessert > main, text);
  assert.ok(text.indexOf('組み立て') > dessert || text.indexOf('重ねる') > dessert, text);
});
