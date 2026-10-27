// SPDX-License-Identifier: MIT OR Apache-2.0
import { existsSync, readFileSync, writeFileSync } from 'node:fs';
import { join } from 'node:path';
import { render } from './lib/render.mjs';

function phrasesIn(dir, name) {
  const read = (lang) => (existsSync(join(dir, `${lang}.ftl`)) ? readFileSync(join(dir, `${lang}.ftl`), 'utf8') : null);
  return { name, files: { ja: read('ja'), en: read('en') } };
}

const args = process.argv.slice(2);
const opts = { kind: 'html', lang: 'ja', dialect: undefined, std: null, project: null, output: null };
for (let i = 0; i < args.length; i += 2) {
  const key = args[i].replace(/^--/, '');
  if (!(key in opts) || i + 1 >= args.length) {
    console.error('usage: cli.mjs [--kind K] [--lang L] [--dialect D] [--std DIR] [--project DIR] [--output FILE] < RESULT.json');
    process.exit(2);
  }
  opts[key] = args[i + 1];
}
const result = JSON.parse(readFileSync(0, 'utf8'));
if (result.outcome?.status === 'error') {
  for (const d of result.diagnostics || []) {
    console.error(`${d.severity} ${d.code} ${d.name}`);
  }
  process.exit(1);
}
const phrases = {
  std: opts.std ? phrasesIn(opts.std, 'std') : null,
  project: opts.project ? phrasesIn(opts.project, 'project') : null,
};
let text;
try {
  text = render(result, { phrases, lang: opts.lang, dialect: opts.dialect, kind: opts.kind });
} catch (e) {
  console.error(`render: ${e.message}`);
  process.exit(2);
}
if (opts.output) {
  writeFileSync(opts.output, text);
} else {
  process.stdout.write(text);
}
