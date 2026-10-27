// SPDX-License-Identifier: MIT OR Apache-2.0
import { foodName, quantityText } from './names.mjs';
import { entryIr } from './recipe.mjs';

function add(a, b) {
  const den = a.den * b.den;
  const num = a.num * b.den + b.num * a.den;
  const g = gcd(Math.abs(num), den);
  return { num: num / g, den: den / g };
}

function gcd(a, b) {
  return b === 0 ? a || 1 : gcd(b, a % b);
}

function sum(a, b) {
  if (!a.lo && !b.lo) {
    return add(a, b);
  }
  const lo = (v) => (v.lo ? v.lo : v);
  const hi = (v) => (v.hi ? v.hi : v);
  return { lo: add(lo(a), lo(b)), hi: add(hi(a), hi(b)) };
}

export function shoppingItems(result) {
  const dish = entryIr(result);
  const items = new Map();
  for (const node of dish.nodes.filter((n) => n.kind === 'in' || n.kind === 'buy')) {
    const unit = node.quantity?.unit ?? null;
    const key = `${node.type}|${node.part ?? ''}|${unit}`;
    const prev = items.get(key);
    if (prev && prev.quantity && node.quantity) {
      prev.quantity = { value: sum(prev.quantity.value, node.quantity.value), unit };
    } else if (!prev) {
      items.set(key, { type: node.type, part: node.part, quantity: node.quantity, bought: node.kind === 'buy' });
    }
  }
  return [...items.values()];
}

export function renderShopping(result, ph, opts = {}) {
  const lines = [`# ${ph.format('heading-shopping')}`];
  for (const item of shoppingItems(result)) {
    const name = foodName(ph, item, opts);
    lines.push(`- ${ph.format('ingredient-line', { name, amount: quantityText(ph, item.quantity, item.type) })}`);
  }
  return `${lines.join('\n')}\n`;
}
