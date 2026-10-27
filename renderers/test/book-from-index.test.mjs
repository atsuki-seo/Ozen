// SPDX-License-Identifier: MIT OR Apache-2.0
import test from 'node:test';
import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { cpSync, mkdtempSync, readFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { Phrases } from '../lib/phrases.mjs';
import { renderBook } from '../lib/book.mjs';
import { root, result, stdPhrases, projectPhrases } from './helpers.mjs';

test('the book has a table of contents and a chapter per entry', () => {
  const dir = mkdtempSync(join(tmpdir(), 'book-'));
  cpSync(join(root, 'samples', 'T3a'), dir, { recursive: true });
  spawnSync(join(root, 'build', 'core-gcc-release', 'ozen'), ['build'], { cwd: dir });
  const index = JSON.parse(readFileSync(join(dir, 'out', 'index.json'), 'utf8'));
  const results = new Map(index.entries.map((e) => [`${e.module}:${e.entry}`, result(dir, ['--entry', e.entry])]));
  const ph = new Phrases([stdPhrases(), projectPhrases(dir, 'T3a')], { lang: 'ja' });
  const html = renderBook(index, results, ph, { title: '台所の違い' });
  const toc = [...html.matchAll(/<li><a href="#entry-(\d+)">([^<]+)<\/a><\/li>/g)];
  assert.equal(toc.length, index.entries.length);
  for (const [, n] of toc) {
    assert.match(html, new RegExp(`<section class="chapter" id="entry-${n}">`));
  }
  assert.ok(toc.some((m) => m[2] === 'カレーとごはん'));
});
