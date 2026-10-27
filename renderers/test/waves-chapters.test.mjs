// SPDX-License-Identifier: MIT OR Apache-2.0
import test from 'node:test';
import assert from 'node:assert/strict';
import { join } from 'node:path';
import { Phrases } from '../lib/phrases.mjs';
import { renderSchedule } from '../lib/schedule.mjs';
import { root, result, stdPhrases } from './helpers.mjs';

test('waves are chapters with a meal section holding unattended work', () => {
  const ph = new Phrases([stdPhrases()], { lang: 'ja' });
  const text = renderSchedule(result(join(root, 'renderers', 'test', 'inputs'), ['--entry', 'dinner', '--kitchen', 'home', 'waves.ozen']), ph);
  const main = text.indexOf('\n## メインを出す（');
  const meal = text.indexOf('\n## 食事中\n');
  const dessert = text.indexOf('\n## デザートを出す（');
  assert.ok(main > 0 && meal > main && dessert > meal, text);
  assert.match(text.slice(meal, dessert), /冷やす。（放置）/);
});
