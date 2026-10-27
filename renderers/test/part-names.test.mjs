// SPDX-License-Identifier: MIT OR Apache-2.0
import test from 'node:test';
import assert from 'node:assert/strict';
import { Phrases } from '../lib/phrases.mjs';
import { foodName } from '../lib/names.mjs';
import { stdPhrases } from './helpers.mjs';

test('a combined key wins over the ingredient and the part', () => {
  const ph = new Phrases([stdPhrases()], { lang: 'ja' });
  assert.equal(foodName(ph, { type: 'Buri', part: 'collar' }), 'ブリカマ');
  assert.equal(foodName(ph, { type: 'Pork', part: 'belly' }), '豚バラ');
});

test('without a combined key the name is built from the ingredient and the part', () => {
  const ja = new Phrases([stdPhrases()], { lang: 'ja' });
  assert.equal(foodName(ja, { type: 'Beef', part: 'sirloin' }), '牛肉サーロイン');
  const en = new Phrases([stdPhrases()], { lang: 'en' });
  assert.equal(foodName(en, { type: 'Beef', part: 'sirloin' }), 'beef sirloin');
});
