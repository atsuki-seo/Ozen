// SPDX-License-Identifier: MIT OR Apache-2.0
import test from 'node:test';
import assert from 'node:assert/strict';
import { join } from 'node:path';
import { Phrases } from '../lib/phrases.mjs';
import { renderRecipe } from '../lib/recipe.mjs';
import { root, result, stdPhrases } from './helpers.mjs';

test('counts take the counter of the ingredient', () => {
  const ph = new Phrases([stdPhrases()], { lang: 'ja' });
  const text = renderRecipe(result(join(root, 'renderers', 'test', 'inputs'), ['--entry', 'soup', 'small.ozen']), ph);
  assert.match(text, /^- 玉ねぎ 2個$/m);
  assert.match(text, /^- バナナ 1本$/m);
  assert.match(text, /^- にんじん 1本$/m);
  assert.match(text, /^- 水 400 ml$/m);
});
