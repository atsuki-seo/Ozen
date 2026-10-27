// SPDX-License-Identifier: MIT OR Apache-2.0
import { escapeXml } from './xml.mjs';
import { keyFor } from './phrases.mjs';
import { words } from './names.mjs';
import { page } from './html.mjs';

const DISPLAY_UNITS = { Mass: 'g', Volume: 'ml', Length: 'cm', Time: 'min', Temperature: 'degC' };
const BLOCKS = { place: 'places', tool: 'tools', vessel: 'tableware' };

function amount(text) {
  const m = /^(-?[\d.]+)(?: (\S+))?$/.exec(text ?? '');
  return m ? { value: m[1], unit: m[2] ?? null } : null;
}

function fieldSpec(id, field, enums, dims) {
  const optional = field.type.endsWith('?');
  const base = optional ? field.type.slice(0, -1) : field.type;
  const out = { id, name: field.name, type: base, optional, default: field.default };
  const list = /^\[(\w+)\]$/.exec(base);
  if (list && enums.has(list[1])) {
    return { ...out, input: 'checks', values: enums.get(list[1]) };
  }
  if (enums.has(base)) {
    return { ...out, input: 'select', values: enums.get(base) };
  }
  const bounds = [field.default, field.min, field.max].map(amount);
  const unit = bounds.find((b) => b?.unit)?.unit ?? DISPLAY_UNITS[base] ?? dims.get(base) ?? null;
  if (base === 'Count' || dims.has(base)) {
    return { ...out, input: 'number', unit: base === 'Count' ? null : unit,
      min: bounds[1]?.value ?? null, max: bounds[2]?.value ?? null, initial: bounds[0]?.value ?? '' };
  }
  return { ...out, input: 'text' };
}

export function settingsSpec(types, definitions) {
  const all = definitions?.definitions ?? [];
  const enums = new Map(all.filter((d) => d.kind === 'enum').map((d) => [d.name, d.values]));
  const dims = new Map(all.filter((d) => d.kind === 'dimension' && d.name !== 'Count').map((d) => [d.name,
    all.find((u) => u.kind === 'unit' && u.dimension === d.name && u.value?.text === 'base')?.name ?? null]));
  const blocks = { places: [], tools: [], tableware: [] };
  const item = (e, category) => ({
    kind: e.kind,
    fields: e.fields.map((f) => fieldSpec(`k-${e.kind}-${f.name}`, f, enums, dims)),
    category,
  });
  for (const e of types.equipment) {
    blocks[BLOCKS[e.category]].push(item(e, e.category));
  }
  for (const v of types.vessels) {
    blocks.tableware.push(item(v, 'vessel'));
  }
  return {
    kitchen: Object.entries(blocks).map(([block, items]) => ({ block, items })),
    options: types.options.map((f) => fieldSpec(`o-${f.name}`, f, enums, dims)),
  };
}

export function settingsText(spec, values) {
  const one = (id) => {
    const v = values[id];
    return v === undefined || v === null ? '' : String(Array.isArray(v) ? v[0] ?? '' : v).trim();
  };
  const written = (f) => {
    if (f.input === 'checks') {
      const picked = [].concat(values[f.id] ?? []).filter((x) => x !== '');
      const text = `[${picked.join(', ')}]`;
      return text === (f.default ?? '[]') ? null : text;
    }
    const v = one(f.id);
    if (v === '' || v === f.default || (f.input === 'number' && v === f.initial)) {
      return null;
    }
    return f.unit ? `${v} ${f.unit}` : v;
  };
  const lower = (kind) => kind.charAt(0).toLowerCase() + kind.slice(1);
  let kitchen = 'kitchen home {\n';
  for (const group of spec.kitchen) {
    const lines = [];
    for (const item of group.items) {
      const count = Number(one(`k-${item.kind}-count`) || '0');
      if (!(count > 0)) {
        continue;
      }
      const parts = [];
      if (count !== 1) {
        parts.push(`count: ${count}`);
      }
      for (const f of item.fields) {
        const text = f.name === 'count' ? null : written(f);
        if (text !== null) {
          parts.push(`${f.name}: ${text}`);
        }
      }
      lines.push(`    ${lower(item.kind)}: ${item.kind}${parts.length > 0 ? ` { ${parts.join(', ')} }` : ''}\n`);
    }
    if (lines.length > 0) {
      kitchen += `  ${group.block} {\n${lines.join('')}  }\n`;
    }
  }
  kitchen += '}\n';
  let options = 'kitchen today : Options {\n';
  for (const f of spec.options) {
    const text = written(f);
    if (text !== null) {
      options += `  ${f.name}: ${text}\n`;
    }
  }
  options += '}\n';
  return { kitchen, options };
}

function collectValues(form) {
  const values = {};
  for (const el of form.elements) {
    if (!el.name) {
      continue;
    }
    if (el.type === 'checkbox') {
      values[el.name] = values[el.name] ?? [];
      if (el.checked) {
        values[el.name].push(el.value);
      }
    } else {
      values[el.name] = el.value;
    }
  }
  return values;
}

function initialValues(spec) {
  const values = {};
  const all = [...spec.kitchen.flatMap((g) => g.items.flatMap((i) => i.fields)), ...spec.options];
  for (const f of all) {
    if (f.input === 'checks') {
      values[f.id] = [];
    } else if (f.input === 'number') {
      values[f.id] = /-count$/.test(f.id) && f.id.startsWith('k-') ? '0' : f.initial;
    } else {
      values[f.id] = f.input === 'select' ? (f.default ?? '') : '';
    }
  }
  return values;
}

function label(ph, kind, name) {
  return escapeXml(ph.format(keyFor(kind, name), {}, words(name)));
}

function control(ph, f, value) {
  const named = `id="${f.id}" name="${f.id}" data-type="${escapeXml(f.type)}"`;
  const text = label(ph, 'field', f.name);
  if (f.input === 'checks') {
    const boxes = f.values.map((v) => `<span class="check"><input type="checkbox" id="${f.id}-${v}" name="${f.id}" value="${v}"/>` +
      `<label for="${f.id}-${v}">${label(ph, 'value', v)}</label></span>`);
    return `<fieldset class="field"><legend>${text}</legend>${boxes.join('')}</fieldset>`;
  }
  let input;
  if (f.input === 'select') {
    const choices = (f.optional ? [''] : []).concat(f.values).map((v) =>
      `<option value="${v}"${v === value ? ' selected="selected"' : ''}>${v === '' ? '' : label(ph, 'value', v)}</option>`);
    input = `<select ${named}>${choices.join('')}</select>`;
  } else if (f.input === 'number') {
    const range = (f.min !== null ? ` min="${f.min}"` : '') + (f.max !== null ? ` max="${f.max}"` : '');
    const quantity = f.unit ? ` data-quantity="true" data-unit="${escapeXml(f.unit)}"` : ' data-quantity="false"';
    input = `<input type="number" ${named} step="${f.unit ? 'any' : '1'}"${range} value="${escapeXml(value)}"${quantity}/>` +
      (f.unit ? `<span class="unit">${escapeXml(f.unit)}</span>` : '');
  } else {
    input = `<input type="text" ${named} value="" placeholder="${escapeXml(f.default ?? '')}"/>`;
  }
  return `<p class="field"><label for="${f.id}">${text}</label> ${input}</p>`;
}

function scriptLiteral(value) {
  return JSON.stringify(value).replace(/[<>&]/g, (c) => `\\u00${c.charCodeAt(0).toString(16)}`);
}

export function renderSettings(spec, ph) {
  const values = initialValues(spec);
  const groups = spec.kitchen.map((g) => {
    const items = g.items.map((i) => `<fieldset class="item"><legend>${label(ph, 'kind', i.kind)}</legend>` +
      i.fields.map((f) => control(ph, f, values[f.id])).join('') + '</fieldset>');
    return `<fieldset class="block"><legend>${label(ph, 'heading', g.block)}</legend>${items.join('')}</fieldset>`;
  });
  const options = spec.options.map((f) => control(ph, f, values[f.id])).join('');
  const text = settingsText(spec, values);
  const title = ph.format('heading-settings');
  const script = `const spec = ${scriptLiteral(spec)};\nconst settingsText = ${String(settingsText)};\n` +
    `const collectValues = ${String(collectValues)};\nconst form = document.getElementById('settings');\n` +
    'const update = () => { const t = settingsText(spec, collectValues(form));' +
    " document.getElementById('kitchen-text').textContent = t.kitchen;" +
    " document.getElementById('options-text').textContent = t.options; };\n" +
    "form.addEventListener('input', update);\nform.addEventListener('change', update);\n";
  const body = `<h1>${escapeXml(title)}</h1><form id="settings" action="#">` +
    `<fieldset><legend>${escapeXml(ph.format('heading-kitchen'))}</legend>${groups.join('')}</fieldset>` +
    `<fieldset><legend>${escapeXml(ph.format('heading-options'))}</legend>${options}</fieldset></form>` +
    `<section><h2>${escapeXml(ph.format('heading-kitchen-text'))}</h2><pre id="kitchen-text">${escapeXml(text.kitchen)}</pre></section>` +
    `<section><h2>${escapeXml(ph.format('heading-options-text'))}</h2><pre id="options-text">${escapeXml(text.options)}</pre></section>` +
    `<script>//<![CDATA[\n${script}//]]></script>`;
  return page(title, ph.lang, body);
}
