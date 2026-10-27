// SPDX-License-Identifier: MIT OR Apache-2.0
import { escapeXml } from './xml.mjs';
import { foodName, quantityText, diagText } from './names.mjs';
import { entryIr, dishTitle, steps } from './recipe.mjs';
import { entrySchedule, scheduleSections, taskText, clock, seconds, waveName } from './schedule.mjs';
import { shoppingItems } from './shopping.mjs';
import { renderGantt } from './gantt.mjs';
import { keyFor } from './phrases.mjs';
import { words } from './names.mjs';

export const STYLE = [
  'body{font-family:sans-serif;margin:2em;line-height:1.6;color:#222}',
  'h1{font-size:1.6em}h2{font-size:1.25em;border-bottom:1px solid #ccc}',
  'table{border-collapse:collapse}td,th{border:1px solid #ccc;padding:2px 6px}',
  'svg .task{fill:#7aa6d8;stroke:#335}',
  'nav ol{columns:2}',
  'fieldset.item{display:inline-block;vertical-align:top;margin:4px}.field{margin:2px 0}input[type=number]{width:6em}',
  '@media print{body{margin:0;font-size:10pt}nav{display:none}section{break-inside:avoid}h2{break-after:avoid}}',
].join('');

function list(items, tag = 'ul') {
  return `<${tag}>${items.map((i) => `<li>${escapeXml(i)}</li>`).join('')}</${tag}>`;
}

export function dishSections(result, ph, opts = {}) {
  const dish = entryIr(result);
  const defs = result.definitions ?? { definitions: [] };
  const nodes = new Map(dish.nodes.map((n) => [n.id, n]));
  const out = [];
  const ingredients = (dish.ingredients || []).map((ing) => {
    const node = ing.node !== null ? nodes.get(ing.node) : null;
    const name = node ? foodName(ph, { type: node.type, part: node.part }, opts) : ing.label;
    return ph.format('ingredient-line', { name, amount: quantityText(ph, node?.quantity ?? null, node?.type) });
  });
  out.push(`<section class="ingredients"><h2>${escapeXml(ph.format('heading-ingredients'))}</h2>${list(ingredients)}</section>`);
  out.push(`<section class="steps"><h2>${escapeXml(ph.format('heading-steps'))}</h2>${list(steps(ph, defs, dish, opts).map((s) => s.text), 'ol')}</section>`);
  const sched = entrySchedule(result);
  if (sched && result.level >= 1) {
    const { sections, after } = scheduleSections(sched);
    const rows = [];
    const row = (t) => rows.push(`<tr><td>${clock(seconds(t.start))}</td><td>${escapeXml(taskText(ph, defs, nodes, sched, t, opts))}</td>` +
      `<td>${escapeXml(t.cook !== null ? ph.format('cook-name', { n: t.cook }) : '')}</td></tr>`);
    for (const s of sections) {
      if (s.meal.length > 0) {
        rows.push(`<tr><th colspan="3">${escapeXml(ph.format('heading-meal'))}</th></tr>`);
        s.meal.forEach(row);
      }
      s.before.forEach(row);
      const name = waveName(result, s.wave.id);
      const wave = ph.format(keyFor('wave', name), {}, words(name));
      rows.push(`<tr><th colspan="3">${escapeXml(ph.format('serve-chapter', { wave, time: clock(seconds(s.wave.serveAt)) }))}</th></tr>`);
    }
    if (after.length > 0) {
      rows.push(`<tr><th colspan="3">${escapeXml(ph.format('heading-cleanup'))}</th></tr>`);
      after.forEach(row);
    }
    out.push(`<section class="schedule"><h2>${escapeXml(ph.format('heading-schedule'))}</h2><table>${rows.join('')}</table></section>`);
    out.push(`<section class="gantt"><h2>${escapeXml(ph.format('heading-gantt'))}</h2>${renderGantt(ph, sched)}</section>`);
  }
  const shopping = shoppingItems(result).map((item) =>
    ph.format('ingredient-line', { name: foodName(ph, item, opts), amount: quantityText(ph, item.quantity, item.type) }));
  out.push(`<section class="shopping"><h2>${escapeXml(ph.format('heading-shopping'))}</h2>${list(shopping)}</section>`);
  const notes = (result.diagnostics || []).filter((d) => d.severity !== 'error' || true).map((d) => diagText(ph, d));
  if (notes.length > 0) {
    out.push(`<section class="notes"><h2>${escapeXml(ph.format('heading-notes'))}</h2>${list(notes)}</section>`);
  }
  return out.join('');
}

export function page(title, lang, body) {
  return '<!DOCTYPE html>\n' +
    `<html xmlns="http://www.w3.org/1999/xhtml" lang="${lang}"><head><meta charset="utf-8"/>` +
    `<meta name="viewport" content="width=device-width, initial-scale=1"/><title>${escapeXml(title)}</title>` +
    `<style>${STYLE}</style></head><body>${body}</body></html>\n`;
}

export function renderHtml(result, ph, opts = {}) {
  const title = dishTitle(ph, entryIr(result).name);
  return page(title, ph.lang, `<h1>${escapeXml(title)}</h1>${dishSections(result, ph, opts)}`);
}
