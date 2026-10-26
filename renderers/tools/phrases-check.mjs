// SPDX-License-Identifier: MIT OR Apache-2.0
import { readFileSync, existsSync } from 'node:fs';
import { join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

export function kebab(name) {
  return name.replace(/([a-z0-9])([A-Z])/g, '$1-$2').replace(/([A-Z])([A-Z][a-z])/g, '$1-$2').toLowerCase();
}

function partNames(members, out) {
  for (const m of members || []) {
    if (m.marker) out.add(m.marker);
    if (m.section && m.section !== 'props') {
      out.add(m.section);
      partNames(m.members, out);
    }
  }
}

const NAME_VARS = ['stage', 'dialect', 'context', 'count'];

export function expectedKeys(defs) {
  const keys = new Map();
  const parts = new Set();
  for (const d of defs.definitions) {
    if (d.kind === 'action') keys.set(`action-${kebab(d.name)}`, d.params.map((p) => p.name));
    if (d.kind === 'ingredient') {
      keys.set(`ingredient-${kebab(d.name)}`, NAME_VARS);
      for (const m of d.members) if (m.section === 'parts') partNames(m.members, parts);
    }
    if (d.kind === 'vessel') keys.set(`vessel-${kebab(d.name)}`, NAME_VARS);
    if (d.kind === 'property') keys.set(`property-${kebab(d.name)}`, NAME_VARS);
    if (d.kind === 'alias') keys.set(`alias-${kebab(d.name)}`, NAME_VARS);
  }
  for (const p of parts) keys.set(`part-${kebab(p)}`, NAME_VARS);
  for (const d of defs.diagnostics) {
    keys.set(`diag-${kebab(d.name)}`, Array.from({ length: 9 }, (_, i) => `arg${i + 1}`));
  }
  return keys;
}

export function parseFtl(text) {
  const messages = new Map();
  let current = null;
  for (const line of text.split('\n')) {
    const m = /^([a-z][a-z0-9-]*)\s*=(.*)$/.exec(line);
    if (m) {
      current = m[1];
      messages.set(current, m[2]);
    } else if (current !== null && /^\s+\S/.test(line)) {
      messages.set(current, `${messages.get(current)}\n${line}`);
    } else {
      current = null;
    }
  }
  return messages;
}

export function variables(body) {
  return [...body.matchAll(/\$([A-Za-z_][A-Za-z0-9_-]*)/g)].map((m) => m[1]);
}

export function check(defs, phrasesDir, langs = ['ja', 'en']) {
  const keys = expectedKeys(defs);
  const report = { keys: keys.size, missing: {}, unknown: 0, lines: [] };
  for (const lang of langs) {
    const path = join(phrasesDir, `${lang}.ftl`);
    const messages = existsSync(path) ? parseFtl(readFileSync(path, 'utf8')) : new Map();
    report.missing[lang] = 0;
    for (const [key, allowed] of keys) {
      if (!messages.has(key)) {
        report.missing[lang]++;
        report.lines.push(`missing in ${lang}: ${key}`);
        continue;
      }
      for (const v of new Set(variables(messages.get(key)))) {
        if (!allowed.includes(v)) {
          report.unknown++;
          report.lines.push(`unknown arg in ${lang}: ${key}: $${v}`);
        }
      }
    }
  }
  return report;
}

if (process.argv[1] && fileURLToPath(import.meta.url) === resolve(process.argv[1])) {
  const [defsPath, phrasesDir] = process.argv.slice(2);
  if (!defsPath || !phrasesDir) {
    console.error('usage: phrases-check.mjs DEFINITIONS.json PHRASES_DIR');
    process.exit(2);
  }
  const report = check(JSON.parse(readFileSync(defsPath, 'utf8')), phrasesDir);
  for (const line of report.lines) console.error(line);
  console.log(`phrases: ${report.keys} keys, ${report.missing.ja} missing in ja, ${report.missing.en} missing in en, ${report.unknown} unknown args`);
  process.exit(report.missing.ja + report.missing.en + report.unknown === 0 ? 0 : 1);
}
