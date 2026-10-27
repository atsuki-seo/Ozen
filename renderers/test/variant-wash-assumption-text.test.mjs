// SPDX-License-Identifier: MIT OR Apache-2.0
import test from 'node:test';
import assert from 'node:assert/strict';
import { join } from 'node:path';
import { Phrases } from '../lib/phrases.mjs';
import { renderSchedule } from '../lib/schedule.mjs';
import { root, result, stdPhrases } from './helpers.mjs';

test('chosen variants, inserted washes and assumptions are written as notes', () => {
  const ph = new Phrases([stdPhrases()], { lang: 'ja' });
  const noCooker = renderSchedule(result(join(root, 'samples', 'T3a'), ['--entry', 'curryRice', '--kitchen', 'noCooker']), ph);
  assert.match(noCooker, /^- cookRice は pot で行います（cooker は unavailable）$/m);
  const washes = renderSchedule(result(join(root, 'samples', 'menuC'), ['--entry', 'dinner', '--kitchen', 'home']), ph);
  assert.match(washes, /^- board#1 を洗う作業を入れました（rawMeat の汚れ）$/m);
  const level1 = renderSchedule(result(join(root, 'samples', 'menuC'), ['--entry', 'dinner', '--cooks', '1']), ph);
  assert.match(level1, /^- 台所が無いので、設備と人の数は無制限として段取りしました$/m);
});
