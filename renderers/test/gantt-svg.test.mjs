// SPDX-License-Identifier: MIT OR Apache-2.0
import test from 'node:test';
import assert from 'node:assert/strict';
import { join } from 'node:path';
import { Phrases } from '../lib/phrases.mjs';
import { renderGantt } from '../lib/gantt.mjs';
import { entrySchedule, scheduledTasks, seconds } from '../lib/schedule.mjs';
import { root, result, stdPhrases } from './helpers.mjs';

test('each task is drawn in its cook row at its time', () => {
  const res = result(join(root, 'samples', 'menuC'), ['--entry', 'dinner', '--kitchen', 'home', '--cooks', '2']);
  const sched = entrySchedule(res);
  const svg = renderGantt(new Phrases([stdPhrases()], { lang: 'ja' }), sched);
  const rects = [...svg.matchAll(/<rect x="([\d.]+)"[^>]*data-row="([^"]+)" data-start="([\d.]+)" data-end="([\d.]+)"/g)]
    .map((m) => ({ x: Number(m[1]), row: m[2], start: Number(m[3]), end: Number(m[4]) }));
  for (const t of scheduledTasks(sched).filter((t) => t.cook !== null)) {
    const r = rects.find((r) => r.row === `cook${t.cook}` && r.start === seconds(t.start));
    assert.ok(r, `task ${t.id} in cook${t.cook}`);
    assert.equal(r.end, seconds(t.endHi));
    assert.ok(Math.abs(r.x - (140 + (seconds(t.start) / 60) * 6)) < 0.11);
  }
});
