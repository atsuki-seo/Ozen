// SPDX-License-Identifier: MIT OR Apache-2.0
import { readFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

export const root = join(dirname(fileURLToPath(import.meta.url)), '..', '..');

export function stdPhrases() {
  const read = (lang) => readFileSync(join(root, 'phrases', `${lang}.ftl`), 'utf8');
  return { name: 'std', files: { ja: read('ja'), en: read('en') } };
}

export function definitions() {
  return JSON.parse(readFileSync(join(root, 'build', 'definitions.json'), 'utf8'));
}
