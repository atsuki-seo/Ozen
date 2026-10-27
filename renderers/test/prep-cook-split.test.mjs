// SPDX-License-Identifier: MIT OR Apache-2.0
import test from 'node:test';
import assert from 'node:assert/strict';
import { join } from 'node:path';
import { Phrases } from '../lib/phrases.mjs';
import { renderSchedule, entrySchedule, scheduledTasks, seconds, clock } from '../lib/schedule.mjs';
import { root, result, stdPhrases, projectPhrases } from './helpers.mjs';

test('work ending 30 minutes before the first serving is preparation', () => {
  const dir = join(root, 'samples', 'menuD');
  const res = result(dir, ['--entry', 'dinner', '--kitchen', 'home', '--options', 'family']);
  const ph = new Phrases([stdPhrases(), projectPhrases(dir, 'menuD')], { lang: 'ja' });
  const text = renderSchedule(res, ph);
  const sched = entrySchedule(res);
  const first = Math.min(...sched.waves.map((w) => seconds(w.serveAt)));
  const prep = text.slice(text.indexOf('## 下ごしらえ'), text.indexOf('## 調理'));
  const cook = text.slice(text.indexOf('## 調理'));
  for (const t of scheduledTasks(sched).filter((t) => t.kind === 'op' && seconds(t.endHi) <= first)) {
    const at = `${clock(seconds(t.start))}〜`;
    if (seconds(t.endHi) <= first - 1800) {
      assert.ok(prep.includes(at), `${at} in preparation`);
    } else {
      assert.ok(cook.includes(at), `${at} in cooking`);
    }
  }
  assert.doesNotMatch(renderSchedule(res, ph, { prepBefore: 0 }), /## 調理/);
  const wide = renderSchedule(res, ph, { prepBefore: 3600 });
  const widePrep = wide.slice(wide.indexOf('## 下ごしらえ'), wide.indexOf('## 調理'));
  for (const t of scheduledTasks(sched).filter((t) => t.kind === 'op' && t.cook !== null && seconds(t.endHi) <= first)) {
    const at = `${clock(seconds(t.start))}〜`;
    assert.equal(widePrep.includes(at), seconds(t.endHi) <= first - 3600, `${at} with prepBefore 3600`);
  }
});

test('work ending exactly at the boundary is preparation', () => {
  const dir = join(root, 'samples', 'menuD');
  const res = result(dir, ['--entry', 'dinner', '--kitchen', 'home', '--options', 'family']);
  const ph = new Phrases([stdPhrases(), projectPhrases(dir, 'menuD')], { lang: 'ja' });
  const sched = entrySchedule(res);
  const first = Math.min(...sched.waves.map((w) => seconds(w.serveAt)));
  const t = scheduledTasks(sched).filter((x) => x.kind === 'op' && x.cook !== null && seconds(x.endHi) <= first)
    .sort((a, b) => seconds(b.endHi) - seconds(a.endHi))[0];
  const length = seconds(t.endHi) - seconds(t.start);
  const at = first - 1800;
  for (const task of sched.tasks.filter((x) => x.id === t.id)) {
    task.start = { num: at - length, den: 1 };
    task.endLo = { num: at, den: 1 };
    task.endHi = { num: at, den: 1 };
  }
  const text = renderSchedule(res, ph);
  const prep = text.slice(text.indexOf('## 下ごしらえ'), text.indexOf('## 調理'));
  assert.ok(prep.includes(`${clock(at - length)}〜`), 'the task ending at the boundary is preparation');
});
