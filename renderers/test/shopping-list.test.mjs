// SPDX-License-Identifier: MIT OR Apache-2.0
import test from 'node:test';
import assert from 'node:assert/strict';
import { join } from 'node:path';
import { Phrases } from '../lib/phrases.mjs';
import { renderShopping } from '../lib/shopping.mjs';
import { root, result, stdPhrases } from './helpers.mjs';

test('the shopping list sums ingredients and includes bought items', () => {
  const ph = new Phrases([stdPhrases()], { lang: 'ja' });
  const text = renderShopping(result(join(root, 'samples', 'menuD'), ['--entry', 'dinner', '--options', 'family']), ph);
  assert.match(text, /^- ナン 2枚$/m);
  assert.match(text, /^- バニラアイス 100 g$/m);
  assert.match(text, /^- 玉ねぎ 2個$/m);
});

test('the same ingredient from two recipes is summed', () => {
  const ph = new Phrases([stdPhrases()], { lang: 'ja' });
  const text = renderShopping(result(join(root, 'samples', 'menuC'), ['--entry', 'dinner']), ph);
  assert.match(text, /^- キャベツ 400 g$/m);
});

test('amounts of different dimensions stay on separate lines', () => {
  const ph = new Phrases([stdPhrases()], { lang: 'ja' });
  const text = renderShopping(result(join(root, 'renderers', 'test', 'inputs'), ['--entry', 'sweets', 'dims.ozen']), ph);
  assert.equal(text.split('\n').filter((l) => l.startsWith('- 砂糖 ')).length, 2, text);
  assert.match(text, /^- 砂糖 10 g$/m);
  assert.match(text, /^- 水 150 ml$/m);
});
