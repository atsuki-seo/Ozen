// SPDX-License-Identifier: MIT OR Apache-2.0
import { escapeXml } from './xml.mjs';
import { seconds, scheduledTasks, unitName } from './schedule.mjs';

const LABEL = 140;
const ROW = 24;
const PER_MINUTE = 6;

export function ganttRows(sched) {
  const tasks = scheduledTasks(sched);
  const rows = [];
  for (let c = 1; c <= (sched.cooks || 0); c++) {
    if (tasks.some((t) => t.cook === c)) {
      rows.push({ id: `cook${c}`, cook: c });
    }
  }
  const units = [];
  for (const t of [...tasks].sort((a, b) => seconds(a.start) - seconds(b.start) || a.id - b.id)) {
    for (const u of t.uses || []) {
      if (!units.includes(u)) {
        units.push(u);
      }
    }
  }
  for (const h of sched.holds || []) {
    if (!units.includes(h.resource)) {
      units.push(h.resource);
    }
  }
  for (const u of units) {
    rows.push({ id: u, unit: u });
  }
  return rows;
}

export function renderGantt(ph, sched) {
  const rows = ganttRows(sched);
  const tasks = scheduledTasks(sched);
  const end = Math.max(60, ...tasks.map((t) => seconds(t.endHi)), ...(sched.holds || []).map((h) => seconds(h.end)));
  const width = LABEL + Math.ceil(end / 60) * PER_MINUTE + 10;
  const height = rows.length * ROW + 10;
  const parts = [`<svg xmlns="http://www.w3.org/2000/svg" data-gantt="true" width="${width}" height="${height}" viewBox="0 0 ${width} ${height}">`];
  rows.forEach((row, i) => {
    const label = row.cook ? ph.format('cook-name', { n: row.cook }) : unitName(ph, sched, row.unit);
    parts.push(`<text x="4" y="${i * ROW + 17}" font-size="12">${escapeXml(label)}</text>`);
    const items = [
      ...tasks.filter((t) => (row.cook ? t.cook === row.cook : (t.uses || []).includes(row.unit)))
        .map((t) => ({ start: seconds(t.start), end: seconds(t.endHi), title: t.op })),
      ...(row.unit ? (sched.holds || []).filter((h) => h.resource === row.unit)
        .map((h) => ({ start: seconds(h.start), end: seconds(h.end), title: 'hold' })) : []),
    ];
    for (const it of items) {
      const x = LABEL + (it.start / 60) * PER_MINUTE;
      const w = Math.max(1, ((it.end - it.start) / 60) * PER_MINUTE);
      parts.push(`<rect x="${x.toFixed(1)}" y="${i * ROW + 4}" width="${w.toFixed(1)}" height="${ROW - 8}" ` +
        `data-row="${escapeXml(row.id)}" data-start="${it.start}" data-end="${it.end}" class="task"><title>${escapeXml(it.title)}</title></rect>`);
    }
  });
  parts.push('</svg>');
  return parts.join('');
}
