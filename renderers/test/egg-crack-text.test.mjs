// SPDX-License-Identifier: MIT OR Apache-2.0
import test from 'node:test';
import assert from 'node:assert/strict';
import { join } from 'node:path';
import { Phrases } from '../lib/phrases.mjs';
import { actionText } from '../lib/names.mjs';
import { renderRecipe } from '../lib/recipe.mjs';
import { root, result, stdPhrases, definitions } from './helpers.mjs';

const egg = [{ name: 'x', food: { type: 'Egg' } }];
const onion = [{ name: 'x', food: { type: 'Onion' } }];

test('an egg is cracked into a bowl, beaten and shelled', () => {
  const ph = new Phrases([stdPhrases()], { lang: 'ja' });
  const defs = definitions();
  assert.equal(actionText(ph, defs, 'crack', egg), '卵をボウルに割り入れる');
  assert.equal(actionText(ph, defs, 'beat', egg), '卵を溶きほぐす');
  assert.equal(actionText(ph, defs, 'peel', egg), '卵の殻をむく');
  assert.equal(actionText(ph, defs, 'peel', onion), '玉ねぎの皮をむく');
});

test('the English sentences crack, beat and shell the egg', () => {
  const ph = new Phrases([stdPhrases()], { lang: 'en' });
  const defs = definitions();
  assert.equal(actionText(ph, defs, 'crack', egg), 'Crack egg into a bowl');
  assert.equal(actionText(ph, defs, 'beat', egg), 'Beat egg');
  assert.equal(actionText(ph, defs, 'peel', egg), 'Shell egg');
  assert.equal(actionText(ph, defs, 'peel', onion), 'Peel onion');
});

test('cracking and beating an egg is one sentence', () => {
  const ph = new Phrases([stdPhrases()], { lang: 'ja' });
  const inputs = join(root, 'renderers', 'test', 'inputs');
  const omelet = renderRecipe(result(inputs, ['--entry', 'omelet', 'eggs.ozen']), ph);
  assert.match(omelet, /^1\. 卵をボウルに割り入れ、溶きほぐす。$/m);
  const boiled = renderRecipe(result(inputs, ['--entry', 'boiledEgg', 'eggs.ozen']), ph);
  assert.match(boiled, /^1\. 卵を10 分ゆでる。$/m);
  assert.match(boiled, /^2\. 卵の殻をむく。$/m);
});
