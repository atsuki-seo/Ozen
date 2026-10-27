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

test('each task and hold is drawn in the row of every unit it uses', () => {
  const res = result(join(root, 'samples', 'T3b'), ['--entry', 'curryRice', '--kitchen', 'withDishwasher']);
  const sched = entrySchedule(res);
  const svg = renderGantt(new Phrases([stdPhrases()], { lang: 'ja' }), sched);
  const rects = [...svg.matchAll(/data-row="([^"]+)" data-start="([\d.]+)" data-end="([\d.]+)"/g)]
    .map((m) => ({ row: m[1], start: Number(m[2]), end: Number(m[3]) }));
  let checked = 0;
  for (const t of scheduledTasks(sched)) {
    for (const u of t.uses || []) {
      assert.ok(rects.some((r) => r.row === u && r.start === seconds(t.start) && r.end === seconds(t.endHi)), `task ${t.id} in ${u}`);
      checked++;
    }
  }
  for (const h of sched.holds || []) {
    assert.ok(rects.some((r) => r.row === h.resource && r.start === seconds(h.start) && r.end === seconds(h.end)), `hold on ${h.resource}`);
    checked++;
  }
  assert.ok(checked > 0);
});
