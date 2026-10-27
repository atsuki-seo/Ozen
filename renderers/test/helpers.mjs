// SPDX-License-Identifier: MIT OR Apache-2.0
import { spawnSync } from 'node:child_process';
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

export function result(dir, args) {
  const out = spawnSync(join(root, 'build', 'core-gcc-release', 'ozen'),
    ['check', '--format', 'json', '--emit', 'ir,schedule,steps,definitions', ...args], { cwd: dir, encoding: 'utf8' });
  return JSON.parse(out.stdout);
}

export function projectPhrases(dir, name) {
  const read = (lang) => {
    try {
      return readFileSync(join(dir, 'phrases', `${lang}.ftl`), 'utf8');
    } catch {
      return null;
    }
  };
  return { name, files: { ja: read('ja'), en: read('en') } };
}

export function expected(name) {
  return readFileSync(join(root, 'renderers', 'test', 'expected', name), 'utf8');
}
