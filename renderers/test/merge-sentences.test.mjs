// SPDX-License-Identifier: MIT OR Apache-2.0
import test from 'node:test';
import assert from 'node:assert/strict';
import { join } from 'node:path';
import { Phrases } from '../lib/phrases.mjs';
import { renderRecipe } from '../lib/recipe.mjs';
import { root, result, stdPhrases } from './helpers.mjs';

test('work done in a row on one food is one sentence', () => {
  const ph = new Phrases([stdPhrases()], { lang: 'ja' });
  const text = renderRecipe(result(join(root, 'renderers', 'test', 'inputs'), ['--entry', 'soup', 'small.ozen']), ph);
  assert.match(text, /^\d+\. にんじんの皮をむく、乱切りにする（20 mm）。$/m);
});

test('work is not merged when the next step takes more than that food', () => {
  const ph = new Phrases([stdPhrases()], { lang: 'ja' });
  const text = renderRecipe(result(join(root, 'renderers', 'test', 'inputs'), ['--entry', 'soup', 'small.ozen']), ph);
  assert.doesNotMatch(text, /乱切りにする（20 mm）、.*煮込む/);
  assert.match(text, /^\d+\. 水、玉ねぎ、にんじんを弱火で15 分煮込む。$/m);
});
