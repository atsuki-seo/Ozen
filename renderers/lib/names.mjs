// SPDX-License-Identifier: MIT OR Apache-2.0
import { kebab, keyFor } from './phrases.mjs';

const KINDS = ['Noodle', 'Grain', 'Root', 'LeafyVegetable', 'Vegetable', 'Meat', 'Fish', 'Fruit', 'Dairy', 'Seasoning'];

function definitionMap(defs) {
  if (!defs.byName) {
    Object.defineProperty(defs, 'byName', { value: new Map(defs.definitions.map((d) => [d.name, d])) });
  }
  return defs.byName;
}

export function kindOf(defs, type) {
  const byName = definitionMap(defs);
  const seen = new Set();
  const queue = [type];
  while (queue.length > 0) {
    const name = queue.shift();
    if (seen.has(name)) {
      continue;
    }
    seen.add(name);
    const supers = (byName.get(name)?.supers || []).map((s) => s.name);
    const direct = KINDS.find((k) => supers.includes(k));
    if (direct) {
      return kebab(direct);
    }
    queue.push(...supers);
  }
  return 'other';
}

export function words(identifier) {
  return identifier.replace(/([a-z0-9])([A-Z])/g, '$1 $2').toLowerCase();
}

export function foodName(ph, food, opts = {}) {
  const args = { stage: opts.stage ?? 'adult', dialect: opts.dialect ?? 'kanto', context: opts.context ?? 'any' };
  const mechanical = food.part ? `${food.type}<${food.part}>` : food.type;
  if (food.part) {
    const combined = `ingredient-${kebab(food.type)}-${kebab(food.part)}`;
    if (ph.has(combined)) {
      return ph.format(combined, args);
    }
    if (ph.has(keyFor('ingredient', food.type)) && ph.has(keyFor('part', food.part))) {
      const ingredient = ph.format(keyFor('ingredient', food.type), args);
      const part = ph.format(keyFor('part', food.part), args);
      return ph.format('name-with-part', { ingredient, part }, `${ingredient} ${part}`);
    }
    return ph.format(combined, args, mechanical);
  }
  if (!ph.has(keyFor('ingredient', food.type))) {
    for (const kind of ['alias', 'dish']) {
      if (ph.has(keyFor(kind, food.type))) {
        return ph.format(keyFor(kind, food.type), args);
      }
    }
  }
  return ph.format(keyFor('ingredient', food.type), args, mechanical);
}

function number(r) {
  if (r.den === 1) {
    return String(r.num);
  }
  let den = r.den;
  for (const p of [2, 5]) {
    while (den % p === 0) {
      den /= p;
    }
  }
  return den === 1 ? String(r.num / r.den) : `${r.num}/${r.den}`;
}

export function numberText(ph, value) {
  if (value.lo) {
    return ph.format('range', { lo: number(value.lo), hi: number(value.hi) }, `${number(value.lo)}..${number(value.hi)}`);
  }
  return number(value);
}

export function quantityText(ph, quantity, type) {
  if (!quantity) {
    return ph.format('amount-any');
  }
  const n = numberText(ph, quantity.value);
  if (quantity.unit === 'pcs' || quantity.unit === '1') {
    const key = type && ph.has(keyFor('counter', type)) ? keyFor('counter', type) : 'counter-default';
    return ph.format('amount-count', { n, counter: ph.format(key) });
  }
  return ph.format('amount-unit', { n, unit: quantity.unit });
}

export function durationText(ph, text) {
  const m = /^(\d+(?:\.\d+)?)(?:\.\.(\d+(?:\.\d+)?))? s$/.exec(text);
  if (!m) {
    return text;
  }
  const lo = Number(m[1]);
  const hi = m[2] ? Number(m[2]) : lo;
  const unit = hi % 3600 === 0 && lo % 3600 === 0 ? 'h' : hi % 60 === 0 && lo % 60 === 0 ? 'min' : 's';
  const div = unit === 'h' ? 3600 : unit === 'min' ? 60 : 1;
  const n = lo === hi ? String(lo / div) : ph.format('range', { lo: String(lo / div), hi: String(hi / div) });
  return ph.format('duration', { n, unit });
}

export function joinList(ph, items) {
  return items.reduce((acc, item) => (acc === null ? item : ph.format('list-join', { first: acc, rest: item })), null) ?? '';
}

export function actionText(ph, defs, action, args, opts = {}) {
  const values = {};
  const shown = [];
  for (const arg of args) {
    if (arg.food) {
      values[arg.name] = arg.text ?? foodName(ph, arg.food, opts);
      values[`${arg.name}-kind`] = kindOf(defs, arg.food.type);
    } else {
      values[arg.name] = arg.text ?? String(arg.value);
    }
    shown.push(values[arg.name]);
  }
  return ph.format(keyFor('action', action), values, `${action}(${shown.join(', ')})`);
}

const ARG_KINDS = {
  provisionalValue: ['action'],
  variantChosen: ['action', 'variant', 'variant'],
  washInserted: ['unit', 'dirt'],
  stainLeft: ['unit', 'dirt'],
  longWait: ['name', 'duration'],
  serveWithinMissed: ['name', 'duration'],
  leftoverPart: ['part'],
  allergen: ['allergen'],
  smallToolsAssumed: ['kind'],
  undercooked: ['ingredient', 'value'],
  evenChoice: ['value'],
  criticalPath: ['duration'],
  noPlating: ['name'],
  unusedValue: ['name'],
};

export function termText(ph, kind, x, opts = {}) {
  if (kind === 'duration') {
    return durationText(ph, x);
  }
  if (kind === 'unit') {
    return opts.unit ? opts.unit(x) : ph.format(keyFor('unit', x.split('#')[0]), {}, x);
  }
  if (kind === 'ingredient') {
    return foodName(ph, { type: x }, opts);
  }
  if (kind === 'action') {
    return ph.formatAttr(keyFor('action', x), 'name') ?? ph.format(keyFor('term', x), {}, words(x));
  }
  if (kind === 'name') {
    const key = ph.has(keyFor('dish', x)) ? keyFor('dish', x) : keyFor('name', x);
    return ph.format(key, {}, words(x));
  }
  return ph.format(keyFor(kind, x), {}, words(x));
}

export function diagText(ph, diag, opts = {}) {
  const kinds = ARG_KINDS[diag.name] || [];
  const values = {};
  (diag.args || []).forEach((a, i) => {
    values[`arg${i + 1}`] = kinds[i] ? termText(ph, kinds[i], a, opts) : a;
  });
  if (diag.name === 'variantChosen') {
    values.reason = ph.format(keyFor('reason', diag.args[3]), { other: values.arg3 }, words(diag.args[3]));
  }
  return ph.format(keyFor('diag', diag.name), values, words(diag.name));
}
