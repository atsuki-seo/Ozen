// SPDX-License-Identifier: MIT OR Apache-2.0
import { existsSync, mkdirSync, readdirSync, readFileSync, rmSync, writeFileSync } from 'node:fs';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { Phrases, layerOrder, renderHtml, renderRecipe, renderSchedule, renderShopping, renderSettings, settingsSpec } from '../lib/render.mjs';

const root = resolve(dirname(fileURLToPath(import.meta.url)), '..', '..');

function phrasesIn(dir, name) {
  const read = (lang) => (existsSync(join(dir, `${lang}.ftl`)) ? readFileSync(join(dir, `${lang}.ftl`), 'utf8') : null);
  return { name, files: { ja: read('ja'), en: read('en') } };
}

const UNITS = new Set(['g', 'kg', 'mg', 'ml', 'l', 'mm', 'cm', 'm', 'min', 's', 'h', 'pcs', 'IH']);

export function identifiers(text, lang) {
  const plain = text.replace(/<(style|script|svg)[^>]*>[\s\S]*?<\/\1>/g, ' ').replace(/<[^>]+>/g, ' ');
  const words = lang === 'ja' ? plain.match(/[A-Za-z][A-Za-z0-9_]*(?:#\d+)?/g) || []
    : plain.match(/\b[a-z]+[A-Z]\w*\b|\b\w+#\d+\b/g) || [];
  return words.filter((w) => !UNITS.has(w));
}

export function renderAll(jsonDir, langs, outDir, sampleRoot = join(root, 'samples')) {
  const dir = join(jsonDir, 'samples');
  const files = existsSync(dir) ? readdirSync(dir).filter((f) => f.endsWith('.json')).sort() : [];
  rmSync(outDir, { recursive: true, force: true });
  mkdirSync(outDir, { recursive: true });
  const total = { fallbacks: 0, errors: 0, unresolved: 0 };
  const missing = new Set();
  const found = new Set();
  let idents = 0;
  const scan = (text, lang) => {
    for (const w of identifiers(text, lang)) {
      idents++;
      found.add(`${lang}: ${w}`);
    }
  };
  const count = (ph) => {
    for (const key of ph.missing) {
      missing.add(key);
    }
    for (const k of Object.keys(total)) {
      total[k] += ph.stats[k];
    }
  };
  let settings = null;
  for (const file of files) {
    const result = JSON.parse(readFileSync(join(dir, file), 'utf8'));
    const sample = file.split('--')[0];
    settings = settings ?? (result.types ? settingsSpec(result.types, result.definitions) : null);
    const depDir = join(sampleRoot, sample, 'deps');
    const deps = existsSync(depDir) ? readdirSync(depDir).map((d) => phrasesIn(join(depDir, d, 'phrases'), d)) : [];
    const sources = layerOrder({ std: phrasesIn(join(root, 'phrases'), 'std'), deps, project: phrasesIn(join(sampleRoot, sample, 'phrases'), sample) });
    for (const lang of langs) {
      const ph = new Phrases(sources, { lang });
      const html = renderHtml(result, ph);
      for (const text of [renderRecipe(result, ph), renderSchedule(result, ph), renderShopping(result, ph), html]) {
        scan(text, lang);
      }
      writeFileSync(join(outDir, `${file.replace(/\.json$/, '')}.${lang}.html`), html);
      count(ph);
    }
  }
  for (const lang of settings ? langs : []) {
    const ph = new Phrases([phrasesIn(join(root, 'phrases'), 'std')], { lang });
    writeFileSync(join(outDir, `settings.${lang}.html`), renderSettings(settings, ph));
    count(ph);
  }
  return { samples: files.length, ...total, identifiers: idents, missing: [...missing].sort(), found: [...found].sort() };
}

if (process.argv[1] && fileURLToPath(import.meta.url) === resolve(process.argv[1])) {
  const args = process.argv.slice(2);
  const at = args.indexOf('--langs');
  const langs = at >= 0 ? args.splice(at, 2)[1].split(',') : ['ja', 'en'];
  const out = args.indexOf('--out') >= 0 ? args.splice(args.indexOf('--out'), 2)[1] : join(root, 'build', 'results', 'html');
  if (args.length !== 1) {
    console.error('usage: render-all.mjs [--langs ja,en] [--out DIR] JSON_DIR');
    process.exit(2);
  }
  const r = renderAll(args[0], langs, out);
  for (const key of r.missing) {
    console.error(`fallback: ${key}`);
  }
  for (const word of r.found) {
    console.error(`identifier: ${word}`);
  }
  console.log(`render-phrases: ${r.samples} samples × ${langs.length} languages, ${r.fallbacks} fallbacks, ${r.errors} fluent errors, ` +
    `${r.unresolved} unresolved, ${r.identifiers} identifiers`);
  process.exit(r.samples > 0 && r.fallbacks + r.errors + r.unresolved + r.identifiers === 0 ? 0 : 1);
}
