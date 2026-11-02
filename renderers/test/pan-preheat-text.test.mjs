// SPDX-License-Identifier: MIT OR Apache-2.0
import test from 'node:test';
import assert from 'node:assert/strict';
import { join } from 'node:path';
import { Phrases } from '../lib/phrases.mjs';
import { renderSchedule } from '../lib/schedule.mjs';
import { renderRecipe } from '../lib/recipe.mjs';
import { root, result, stdPhrases } from './helpers.mjs';

const pan = () => result(join(root, 'tests', 'golden', 'pan'), ['--kitchen', 'home', '--options', 'today', 'preheat-before-fry.ozen']);

test('the frying pan is heated over the heat of the frying', () => {
  const ja = renderSchedule(pan(), new Phrases([stdPhrases()], { lang: 'ja' }));
  assert.match(ja, /^0:00〜 フライパンを中火で温める。（放置）$/m);
  assert.match(ja, /^0:03〜 豚肉を中火で5 分炒める。（1人目）$/m);
  const en = renderSchedule(pan(), new Phrases([stdPhrases()], { lang: 'en' }));
  assert.match(en, /^0:00 Heat the frying pan over medium heat\. \(unattended\)$/m);
});

test('the recipe does not mention heating the pan', () => {
  const ja = renderRecipe(pan(), new Phrases([stdPhrases()], { lang: 'ja' }));
  assert.doesNotMatch(ja, /温める/);
  assert.match(ja, /^1\. 豚肉を中火で5 分炒める。$/m);
});
