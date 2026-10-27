// SPDX-License-Identifier: MIT OR Apache-2.0
import test from 'node:test';
import assert from 'node:assert/strict';
import { Phrases } from '../lib/phrases.mjs';
import { foodName } from '../lib/names.mjs';
import { stdPhrases } from './helpers.mjs';

test('the stage and the dialect choose the name', () => {
  const ph = new Phrases([stdPhrases()], { lang: 'ja' });
  assert.equal(foodName(ph, { type: 'Buri' }, { stage: 'young', dialect: 'kansai' }), 'ハマチ');
  assert.equal(foodName(ph, { type: 'Buri' }, { stage: 'young', dialect: 'kanto' }), 'イナダ');
  assert.equal(foodName(ph, { type: 'Buri' }, { stage: 'middle', dialect: 'kansai' }), 'メジロ');
  assert.equal(foodName(ph, { type: 'Buri' }, {}), 'ブリ');
});
