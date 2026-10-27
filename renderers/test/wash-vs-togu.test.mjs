// SPDX-License-Identifier: MIT OR Apache-2.0
import test from 'node:test';
import assert from 'node:assert/strict';
import { Phrases } from '../lib/phrases.mjs';
import { actionText } from '../lib/names.mjs';
import { stdPhrases, definitions } from './helpers.mjs';

test('rice is polished, vegetables are washed', () => {
  const ph = new Phrases([stdPhrases()], { lang: 'ja' });
  const defs = definitions();
  assert.equal(actionText(ph, defs, 'wash', [{ name: 'x', food: { type: 'Rice' } }]), '米を研ぐ');
  assert.equal(actionText(ph, defs, 'wash', [{ name: 'x', food: { type: 'Cabbage' } }]), 'キャベツを洗う');
});
