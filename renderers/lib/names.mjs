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
  return ph.format(keyFor('ingredient', food.type), args, mechanical);
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

export function diagText(ph, diag) {
  const values = {};
  (diag.args || []).forEach((a, i) => {
    values[`arg${i + 1}`] = a;
  });
  return ph.format(keyFor('diag', diag.name), values, words(diag.name));
}
