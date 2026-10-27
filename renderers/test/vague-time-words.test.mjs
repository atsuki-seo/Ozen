// SPDX-License-Identifier: MIT OR Apache-2.0
import test from 'node:test';
import assert from 'node:assert/strict';
import { join } from 'node:path';
import { Phrases } from '../lib/phrases.mjs';
import { renderRecipe } from '../lib/recipe.mjs';
import { root, result, stdPhrases } from './helpers.mjs';

test('serving properties are shown in words', () => {
  const ph = new Phrases([stdPhrases()], { lang: 'ja' });
  const text = renderRecipe(result(join(root, 'renderers', 'test', 'inputs'), ['--entry', 'lunch', 'small.ozen']), ph);
  assert.match(text, /を盛り付ける（温かいうちに出す）。$/m);
  assert.match(text, /^\d+\. 牛乳を盛り付ける（5 分以内に出す）。$/m);
});
