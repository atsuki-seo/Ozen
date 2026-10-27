// SPDX-License-Identifier: MIT OR Apache-2.0
import test from 'node:test';
import assert from 'node:assert/strict';
import { join } from 'node:path';
import { Phrases } from '../lib/phrases.mjs';
import { renderSchedule } from '../lib/schedule.mjs';
import { root, result, stdPhrases, projectPhrases } from './helpers.mjs';

test('a recipe without waves keeps its work out of the cleanup section', () => {
  const dir = join(root, 'samples', 'menuC');
  const res = result(dir, ['--entry', 'stirFry', '--kitchen', 'home']);
  const text = renderSchedule(res, new Phrases([stdPhrases(), projectPhrases(dir, 'menuC')], { lang: 'ja' }));
  assert.doesNotMatch(text, /## 片付け\n0:00/);
  assert.match(text, /## (下ごしらえ|調理)\n0:00〜/);
});
