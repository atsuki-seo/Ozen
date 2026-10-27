// SPDX-License-Identifier: MIT OR Apache-2.0
import test from 'node:test';
import assert from 'node:assert/strict';
import { join } from 'node:path';
import { Phrases } from '../lib/phrases.mjs';
import { renderSchedule } from '../lib/schedule.mjs';
import { root, result, stdPhrases, projectPhrases } from './helpers.mjs';

const dir = join(root, 'samples', 'menuC');

test('schedule lines carry the time and the cook', () => {
  const ph = new Phrases([stdPhrases(), projectPhrases(dir, 'menuC')], { lang: 'ja' });
  const one = renderSchedule(result(dir, ['--entry', 'dinner', '--kitchen', 'home']), ph);
  assert.match(one, /^# 夕ごはんの段取り表$/m);
  assert.match(one, /^0:00〜 米を研ぐ。（1人目）$/m);
  assert.doesNotMatch(one, /（2人目）/);
  const two = renderSchedule(result(dir, ['--entry', 'dinner', '--kitchen', 'home', '--cooks', '2']), ph);
  assert.match(two, /（2人目）$/m);
});
