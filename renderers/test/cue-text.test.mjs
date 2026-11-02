// SPDX-License-Identifier: MIT OR Apache-2.0
import test from 'node:test';
import assert from 'node:assert/strict';
import { join } from 'node:path';
import { Phrases } from '../lib/phrases.mjs';
import { renderRecipe } from '../lib/recipe.mjs';
import { root, result, stdPhrases } from './helpers.mjs';

const dir = join(root, 'tests', 'golden', 'cue');
const recipe = (lang) => renderRecipe(result(dir, ['fragrant.ozen']), new Phrases([stdPhrases()], { lang }));

test('a cue is written as what the cook waits for, with the time', () => {
  assert.match(recipe('ja'), /^2\. 玉ねぎを香りが立つまで中火で炒める（約 1 分）。$/m);
  assert.match(recipe('en'), /^2\. Fry onion over medium heat until fragrant \(about 1 min\)\.$/m);
});
