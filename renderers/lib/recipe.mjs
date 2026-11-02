// SPDX-License-Identifier: MIT OR Apache-2.0
import { keyFor } from './phrases.mjs';
import { foodName, quantityText, durationText, joinList, kindOf, words } from './names.mjs';

const STRUCTURAL = new Set(['combine']);

export function entryIr(result) {
  return result.ir.find((d) => d.name === result.entry) ?? result.ir[result.ir.length - 1];
}

export function dishTitle(ph, name) {
  return ph.format(keyFor('dish', name), {}, words(name));
}

export function valueName(ph, nodes, id, opts) {
  const node = nodes.get(id);
  if (!node) {
    return '';
  }
  if (node.kind === 'in' || node.kind === 'buy') {
    return foodName(ph, { type: node.type, part: node.part }, opts);
  }
  if (node.type && ph.has(keyFor('dish', node.type))) {
    return foodName(ph, { type: node.type }, opts);
  }
  if (node.kind === 'op' && node.label === 'beat' && node.inputs.length > 0) {
    return ph.format('value-beaten', { x: valueName(ph, nodes, node.inputs[0].node, opts) });
  }
  if (node.kind === 'op' && (STRUCTURAL.has(node.label) || node.label === 'assemble')) {
    return joinList(ph, node.inputs.map((i) => valueName(ph, nodes, i.node, opts)));
  }
  return node.inputs.length > 0 ? valueName(ph, nodes, node.inputs[0].node, opts) : foodName(ph, { type: node.type }, opts);
}

function argValue(ph, arg) {
  if (/ s$/.test(arg.value)) {
    return durationText(ph, arg.value);
  }
  const temp = /^(\d+(?:\.\d+)?) degC$/.exec(arg.value);
  if (temp) {
    return ph.format('temperature', { n: temp[1] });
  }
  if (/^[a-z][A-Za-z0-9]*$/.test(arg.value) && ph.has(keyFor('value', arg.value))) {
    return ph.format(keyFor('value', arg.value));
  }
  return arg.value;
}

function stepValues(ph, defs, nodes, node, opts) {
  const values = {};
  const shown = [];
  const action = (defs.definitions || []).find((d) => d.kind === 'action' && d.name === node.label);
  const named = new Set(node.args.map((a) => a.name));
  const foodParams = action ? action.params.map((p) => p.name).filter((n) => !named.has(n)) : ['x'];
  node.inputs.forEach((input, i) => {
    const param = foodParams[i] ?? `x${i + 1}`;
    const food = nodes.get(input.node);
    values[param] = valueName(ph, nodes, input.node, opts);
    values[`${param}-kind`] = food ? kindOf(defs, food.type ?? '') : 'other';
    shown.push(values[param]);
  });
  for (const arg of node.args) {
    values[arg.name] = argValue(ph, arg);
    shown.push(values[arg.name]);
  }
  return { values, mechanical: `${node.label}(${shown.join(', ')})` };
}

function noteText(ph, note) {
  const key = keyFor('note', note.name);
  if (note.name === 'serveWithin') {
    const m = /^(\d+) (s|min|h)$/.exec(note.args[0] ?? '');
    const time = m ? ph.format('duration', { n: m[1], unit: m[2] }) : note.args[0];
    return ph.format(key, { time });
  }
  if (ph.has(key)) {
    return ph.format(key);
  }
  return ph.format(keyFor('property', note.name), {}, words(note.name));
}

export function stepText(ph, defs, nodes, node, opts) {
  if (node.kind === 'plate') {
    const items = joinList(ph, node.inputs.map((i) => valueName(ph, nodes, i.node, opts)));
    const text = ph.format('action-plate', { x: items }, `plate(${items})`);
    const notes = (node.notes || []).map((n) => noteText(ph, n));
    return notes.length > 0 ? ph.format('step-with-notes', { text, notes: joinList(ph, notes) }) : text;
  }
  if (node.label === 'assemble') {
    const items = joinList(ph, node.inputs.map((i) => valueName(ph, nodes, i.node, opts)));
    return ph.format('action-assemble', { x: items }, `assemble(${items})`);
  }
  const { values, mechanical } = stepValues(ph, defs, nodes, node, opts);
  const until = node.until ? untilText(ph, nodes, node, values, opts) : null;
  return until ?? ph.format(keyFor('action', node.label), values, mechanical);
}

function rangeText(range) {
  const n = (r) => r.num / r.den;
  return n(range.lo) === n(range.hi) ? `${n(range.lo)} s` : `${n(range.lo)}..${n(range.hi)} s`;
}

function untilText(ph, nodes, node, values, opts, single = false) {
  const act = ph.formatAttr(keyFor('action', node.label), 'until', values);
  if (act === null || !node.until.axis) {
    return null;
  }
  const input = node.inputs.length > 0 ? nodes.get(node.inputs[0].node) : null;
  const mixed = !single && input && input.kind === 'op' && STRUCTURAL.has(input.label);
  const subject = node.until.food ? foodName(ph, { type: node.until.food }, opts) : values.x;
  const stage = ph.format(keyFor(`until-${node.until.axis}`, node.until.value), {}, words(node.until.value));
  const args = { x: values.x, subject, stage, act, time: durationText(ph, rangeText(node.time)) };
  return ph.format(mixed ? 'step-until-mixed' : 'step-until', args);
}

function reaches(nodes, id, target) {
  for (let cur = id, guard = 0; cur !== undefined && guard < 1000; guard++) {
    if (cur === target) {
      return true;
    }
    const n = nodes.get(cur);
    cur = n && n.kind === 'op' && n.inputs.length > 0 ? n.inputs[0].node : undefined;
  }
  return false;
}

function continuedText(ph, defs, nodes, head, numberOf, opts) {
  const input = head.inputs.length > 0 ? nodes.get(head.inputs[0].node) : null;
  const items = [];
  let held = head.inputs[0]?.node;
  if (input && input.kind === 'op' && STRUCTURAL.has(input.label)) {
    held = undefined;
    for (const i of input.inputs) {
      if (held === undefined && reaches(nodes, i.node, head.continues)) {
        held = i.node;
      } else {
        items.push(valueName(ph, nodes, i.node, opts));
      }
    }
  }
  const { values, mechanical } = stepValues(ph, defs, nodes, head, opts);
  const plain = { ...values, x: ph.format('continued-object') };
  const full = (head.until ? untilText(ph, nodes, head, plain, opts, true) : null) ??
    ph.format(keyFor('action', head.label), plain, mechanical);
  const next = head.until ? full : ph.formatAttr(keyFor('action', head.label), 'next', values) ?? full;
  if (items.length === 0) {
    return ph.format('step-as-is', { next, full });
  }
  return ph.format('step-add-to', { step: String(numberOf(held)), items: joinList(ph, items), next, full });
}

function nextText(ph, defs, nodes, node, opts) {
  if (node.until) {
    return null;
  }
  const { values } = stepValues(ph, defs, nodes, node, opts);
  return ph.formatAttr(keyFor('action', node.label), 'next', values);
}

export function steps(ph, defs, dish, opts = {}) {
  const nodes = new Map(dish.nodes.map((n) => [n.id, n]));
  const users = new Map();
  for (const n of dish.nodes) {
    for (const i of n.inputs) {
      users.set(i.node, [...(users.get(i.node) || []), n.id]);
    }
  }
  const work = dish.nodes.filter((n) => (n.kind === 'op' && !STRUCTURAL.has(n.label)) || n.kind === 'choose' || n.kind === 'plate');
  const merged = new Map();
  const absorbed = new Set();
  for (const n of work) {
    if (n.kind === 'plate' || absorbed.has(n.id)) {
      continue;
    }
    let chain = [n];
    let cur = n;
    for (;;) {
      const next = users.get(cur.id) || [];
      const b = next.length === 1 ? nodes.get(next[0]) : null;
      if (!b || b.kind === 'plate' || STRUCTURAL.has(b.label) || b.inputs.length !== 1 || b.group !== n.group) {
        break;
      }
      const text = nextText(ph, defs, nodes, b, opts);
      if (text === null) {
        break;
      }
      chain.push(b);
      absorbed.add(b.id);
      cur = b;
    }
    merged.set(n.id, chain);
  }
  const out = [];
  for (const n of work) {
    if (absorbed.has(n.id)) {
      continue;
    }
    const chain = merged.get(n.id) ?? [n];
    let text = stepText(ph, defs, nodes, chain[0], opts);
    if (chain.length > 1) {
      text = ph.formatAttr(keyFor('action', chain[0].label), 'cont', stepValues(ph, defs, nodes, chain[0], opts).values) ?? text;
    }
    for (const b of chain.slice(1)) {
      text = ph.format('steps-joined', { first: text, then: nextText(ph, defs, nodes, b, opts) });
    }
    out.push({ id: n.id, ids: chain.map((c) => c.id), group: n.group, wave: n.wave, text: ph.format('sentence', { text }) });
  }
  return out;
}

function waveOf(dish, list) {
  const nodes = new Map(dish.nodes.map((n) => [n.id, n]));
  const owner = new Map();
  const plates = dish.nodes.filter((n) => n.kind === 'plate').sort((a, b) => (a.wave ?? 0) - (b.wave ?? 0));
  for (const p of plates) {
    const stack = [p.id];
    while (stack.length > 0) {
      const id = stack.pop();
      if (owner.has(id)) {
        continue;
      }
      owner.set(id, p.wave);
      for (const i of nodes.get(id)?.inputs || []) {
        stack.push(i.node);
      }
    }
  }
  return (step) => owner.get(step.id) ?? plates[0]?.wave ?? null;
}

export function renderRecipe(result, ph, opts = {}) {
  const dish = entryIr(result);
  const defs = result.definitions ?? { definitions: [] };
  const nodes = new Map(dish.nodes.map((n) => [n.id, n]));
  const lines = [`# ${dishTitle(ph, dish.name)}`, '', `## ${ph.format('heading-ingredients')}`];
  const listed = new Set();
  for (const ing of dish.ingredients || []) {
    const node = ing.node !== null ? nodes.get(ing.node) : null;
    const food = node ?? ing;
    const name = food.type ? foodName(ph, { type: food.type, part: food.part }, opts) : ing.label;
    const amount = quantityText(ph, food.quantity ?? null, food.type);
    lines.push(`- ${ph.format('ingredient-line', { name, amount })}`);
    listed.add(ing.node);
  }
  for (const node of dish.nodes.filter((n) => n.kind === 'buy')) {
    const name = foodName(ph, { type: node.type, part: node.part }, opts);
    lines.push(`- ${ph.format('ingredient-line', { name, amount: quantityText(ph, node.quantity, node.type) })}`);
  }
  lines.push('', `## ${ph.format('heading-steps')}`);
  const list = steps(ph, defs, dish, opts);
  const nodeMap = new Map(dish.nodes.map((n) => [n.id, n]));
  const numberOf = (id) => list.findIndex((st) => st.ids.includes(id)) + 1;
  for (const step of list) {
    const head = nodeMap.get(step.id);
    if (!head || head.continues === undefined) {
      continue;
    }
    let text = continuedText(ph, defs, nodeMap, head, numberOf, opts);
    for (const id of step.ids.slice(1)) {
      text = ph.format('steps-joined', { first: text, then: nextText(ph, defs, nodeMap, nodeMap.get(id), opts) });
    }
    step.text = ph.format('sentence', { text });
  }
  const waves = dish.waves || [];
  const wave = waveOf(dish, list);
  let number = 0;
  let group = null;
  let currentWave;
  for (const step of list) {
    const w = waves.length > 1 ? wave(step) : null;
    if (waves.length > 1 && w !== currentWave) {
      currentWave = w;
      const name = waves.find((x) => x.id === w)?.name ?? '';
      lines.push('', `### ${ph.format(keyFor('wave', name), {}, words(name))}`);
      group = null;
    }
    if (step.group !== group) {
      group = step.group;
      lines.push('');
      if (group) {
        lines.push(`#### ${ph.format(keyFor('group', group), {}, words(group))}`);
      }
    }
    number++;
    lines.push(`${number}. ${step.text}`);
  }
  return `${lines.join('\n')}\n`;
}
