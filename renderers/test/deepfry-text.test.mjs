// SPDX-License-Identifier: MIT OR Apache-2.0
import test from 'node:test';
import assert from 'node:assert/strict';
import { join } from 'node:path';
import { Phrases } from '../lib/phrases.mjs';
import { renderSchedule } from '../lib/schedule.mjs';
import { renderRecipe } from '../lib/recipe.mjs';
import { root, result, stdPhrases } from './helpers.mjs';

const fried = () => result(join(root, 'tests', 'golden', 'deepfry'), ['--kitchen', 'home', '--options', 'today', 'preheat-oil.ozen']);

test('the oil is heated before deep-frying', () => {
  const ja = renderSchedule(fried(), new Phrases([stdPhrases()], { lang: 'ja' }));
  assert.match(ja, /^0:00〜 油を170℃に熱する。（放置）$/m);
  const en = renderSchedule(fried(), new Phrases([stdPhrases()], { lang: 'en' }));
  assert.match(en, /^0:00 Heat the oil to 170 °C\. \(unattended\)$/m);
});

test('the recipe deep-fries in oil at the temperature', () => {
  const ja = renderRecipe(fried(), new Phrases([stdPhrases()], { lang: 'ja' }));
  assert.match(ja, /^1\. じゃがいもを170℃の油で5 分揚げる。$/m);
  const en = renderRecipe(fried(), new Phrases([stdPhrases()], { lang: 'en' }));
  assert.match(en, /^1\. Deep-fry potato in oil at 170 °C for 5 min\.$/m);
});
