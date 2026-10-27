// SPDX-License-Identifier: MIT OR Apache-2.0
import test from 'node:test';
import assert from 'node:assert/strict';
import { join } from 'node:path';
import { Phrases } from '../lib/phrases.mjs';
import { renderRecipe } from '../lib/recipe.mjs';
import { root, result, stdPhrases } from './helpers.mjs';

test('steps under @group form one block with a heading', () => {
  const ph = new Phrases([stdPhrases()], { lang: 'ja' });
  const text = renderRecipe(result(join(root, 'renderers', 'test', 'inputs'), ['--entry', 'soup', 'small.ozen']), ph);
  assert.match(text, /\n#### onion prep\n1\. 玉ねぎの皮をむく、薄切りにする（3 mm）。\n\n2\. にんじんの/);
});
