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
  const late = renderSchedule(res, ph, { prepBefore: 0 });
  assert.doesNotMatch(late, /## 調理\n0:00/);
});
