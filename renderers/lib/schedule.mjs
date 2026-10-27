// SPDX-License-Identifier: MIT OR Apache-2.0
import { keyFor } from './phrases.mjs';
import { diagText, words } from './names.mjs';
import { entryIr, dishTitle, stepText } from './recipe.mjs';

const CLEANS = new Set(['wash', 'rinse', 'soak', 'wipe', 'run']);
const NOTE_DIAGS = new Set(['variantChosen', 'washInserted', 'stainLeft', 'longWait', 'serveWithinMissed',
  'evenChoice', 'unlimitedEquipment', 'defaultServings', 'firstVariant', 'smallToolsAssumed', 'vesselSubstituted',
  'externalIngredient', 'noPlating', 'leftoverPart']);

export function seconds(r) {
  return r.num / r.den;
}

export function clock(t) {
  const total = Math.round(t / 60);
  return `${Math.floor(total / 60)}:${String(total % 60).padStart(2, '0')}`;
}

export function entrySchedule(result) {
  const list = result.schedules || [];
  return (list.find((s) => s.name === result.entry) ?? list[list.length - 1])?.schedule ?? null;
}

export function unitName(ph, sched, id) {
  const res = (sched.resources || []).find((r) => r.id === id);
  const same = (sched.resources || []).filter((r) => r.kind === res?.kind);
  const kindKey = res && res.category === 'vessel' ? keyFor('vessel', res.kind) : keyFor('kind', res?.kind ?? '');
  const name = res ? ph.format(kindKey, {}, words(res.kind)) : id;
  const n = id.split('#')[1];
  return same.length > 1 ? ph.format('unit-numbered', { name, n }) : name;
}

export function scheduledTasks(sched) {
  return (sched.tasks || []).filter((t) => {
    if (t.kind === 'op' && t.op === 'combine') {
      return false;
    }
    return !(t.kind === 'op' && t.cook === null && (t.uses || []).length === 0 && t.unattended);
  });
}

export function taskText(ph, defs, nodes, sched, t, opts = {}) {
  if (CLEANS.has(t.kind)) {
    const target = Array.isArray(t.cleans) ? t.cleans.map((c) => c.resource) : t.cleans ? [t.cleans.resource] : t.uses.slice(-1);
    const tools = target.map((id) => unitName(ph, sched, id));
    const tool = tools.reduce((a, b) => (a === null ? b : ph.format('list-join', { first: a, rest: b })), null) ?? '';
    return ph.format(keyFor('clean', t.kind), { tool }, `${t.kind}(${tool})`);
  }
  const node = nodes.get(t.id);
  return node ? stepText(ph, defs, nodes, node, opts) : t.op;
}

function line(ph, defs, nodes, sched, t, opts) {
  const who = t.cook !== null ? ph.format('by-cook', { n: t.cook }) : ph.format('unattended-note');
  return `${ph.format('time-from', { time: clock(seconds(t.start)) })} ${ph.format('sentence', { text: taskText(ph, defs, nodes, sched, t, opts) })}${who}`;
}

export function waveName(result, id) {
  return (entryIr(result).waves || []).find((w) => w.id === id)?.name ?? 'main';
}

export function scheduleSections(sched) {
  const tasks = scheduledTasks(sched).sort((a, b) => seconds(a.start) - seconds(b.start) || a.id - b.id);
  const waves = [...(sched.waves || [])].sort((a, b) => seconds(a.serveAt) - seconds(b.serveAt));
  const sections = waves.map((w, i) => {
    const prev = waves[i - 1];
    const mealStart = prev ? seconds(prev.serveAt) : null;
    const mealEnd = prev ? seconds(prev.serveAt) + seconds(w.gap) : null;
    return { wave: w, mealStart, mealEnd, meal: [], before: [] };
  });
  const after = [];
  if (sections.length === 0 && tasks.length > 0) {
    const end = Math.max(...tasks.map((t) => seconds(t.endHi)));
    return { sections: [{ wave: null, end, mealStart: null, mealEnd: null, meal: [], before: tasks }], after };
  }
  for (const t of tasks) {
    const start = seconds(t.start);
    const end = seconds(t.endHi);
    let placed = false;
    for (const s of sections) {
      if (s.mealStart !== null && start < s.mealEnd && end > s.mealStart && t.cook === null) {
        s.meal.push(t);
        placed = true;
        break;
      }
      if (end <= seconds(s.wave.serveAt) + 1e-9) {
        s.before.push(t);
        placed = true;
        break;
      }
    }
    if (!placed) {
      after.push(t);
    }
  }
  return { sections, after };
}

export function renderSchedule(result, ph, opts = {}) {
  const dish = entryIr(result);
  const sched = entrySchedule(result);
  const defs = result.definitions ?? { definitions: [] };
  const nodes = new Map(dish.nodes.map((n) => [n.id, n]));
  const lines = [`# ${ph.format('schedule-title', { dish: dishTitle(ph, dish.name) })}`];
  if (!sched) {
    return `${lines.join('\n')}\n`;
  }
  const prepBefore = opts.prepBefore ?? 1800;
  const { sections, after } = scheduleSections(sched);
  sections.forEach((s, i) => {
    if (s.meal.length > 0) {
      lines.push('', `## ${ph.format('heading-meal')}`);
      for (const t of s.meal) {
        lines.push(line(ph, defs, nodes, sched, t, opts));
      }
    }
    if (i === 0) {
      const first = s.wave ? seconds(s.wave.serveAt) : s.end;
      const prep = s.before.filter((t) => seconds(t.endHi) <= first - prepBefore);
      const cook = s.before.filter((t) => seconds(t.endHi) > first - prepBefore);
      for (const [heading, list] of [['heading-prep', prep], ['heading-cook', cook]]) {
        if (list.length > 0) {
          lines.push('', `## ${ph.format(heading)}`);
          for (const t of list) {
            lines.push(line(ph, defs, nodes, sched, t, opts));
          }
        }
      }
    } else if (s.before.length > 0) {
      lines.push('');
      for (const t of s.before) {
        lines.push(line(ph, defs, nodes, sched, t, opts));
      }
    }
    if (s.wave) {
      const name = waveName(result, s.wave.id);
      const wave = ph.format(keyFor('wave', name), {}, words(name));
      lines.push('', `## ${ph.format('serve-chapter', { wave, time: clock(seconds(s.wave.serveAt)) })}`);
    }
  });
  if (after.length > 0) {
    lines.push('', `## ${ph.format('heading-cleanup')}`);
    for (const t of after) {
      lines.push(line(ph, defs, nodes, sched, t, opts));
    }
  }
  const notes = (result.diagnostics || []).filter((d) => NOTE_DIAGS.has(d.name));
  if (notes.length > 0) {
    lines.push('', `## ${ph.format('heading-notes')}`);
    for (const d of notes) {
      lines.push(`- ${diagText(ph, d)}`);
    }
  }
  return `${lines.join('\n')}\n`;
}
