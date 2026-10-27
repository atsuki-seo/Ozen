// SPDX-License-Identifier: MIT OR Apache-2.0
import test from 'node:test';
import assert from 'node:assert/strict';
import { join } from 'node:path';
import { Phrases } from '../lib/phrases.mjs';
import { renderRecipe } from '../lib/recipe.mjs';
import { root, result, stdPhrases } from './helpers.mjs';

test('ingredients are listed in the order they are written', () => {
  const ph = new Phrases([stdPhrases()], { lang: 'ja' });
  const text = renderRecipe(result(join(root, 'renderers', 'test', 'inputs'), ['--entry', 'soup', 'small.ozen']), ph);
  const names = [...text.matchAll(/^- (\S+) /gm)].map((m) => m[1]);
  assert.deepEqual(names, ['水', 'バナナ', '玉ねぎ', 'にんじん']);
});
