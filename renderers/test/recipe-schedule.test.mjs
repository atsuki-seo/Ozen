// SPDX-License-Identifier: MIT OR Apache-2.0
import test from 'node:test';
import assert from 'node:assert/strict';
import { join } from 'node:path';
import { Phrases } from '../lib/phrases.mjs';
import { renderSchedule } from '../lib/schedule.mjs';
import { renderHtml } from '../lib/html.mjs';
import { root, result, stdPhrases, projectPhrases } from './helpers.mjs';

test('a recipe without waves keeps its work out of the cleanup section', () => {
  const dir = join(root, 'samples', 'menuC');
  const res = result(dir, ['--entry', 'stirFry', '--kitchen', 'home']);
  const text = renderSchedule(res, new Phrases([stdPhrases(), projectPhrases(dir, 'menuC')], { lang: 'ja' }));
  assert.doesNotMatch(text, /## 片付け\n0:00/);
  assert.match(text, /## (下ごしらえ|調理)\n0:00〜/);
});

test('the schedule table shows the oven preheating', () => {
  const res = result(join(root, 'tests', 'golden', 'sched'), ['--kitchen', 'home', '--options', 'today', 'preheat-json.ozen']);
  const html = renderHtml(res, new Phrases([stdPhrases()], { lang: 'ja' }));
  assert.match(html, /オーブンを220℃に予熱する/);
});
