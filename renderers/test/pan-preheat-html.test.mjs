// SPDX-License-Identifier: MIT OR Apache-2.0
import test from 'node:test';
import assert from 'node:assert/strict';
import { join } from 'node:path';
import { Phrases } from '../lib/phrases.mjs';
import { renderHtml } from '../lib/html.mjs';
import { root, result, stdPhrases } from './helpers.mjs';

test('the schedule table has the row that heats the frying pan', () => {
  const res = result(join(root, 'tests', 'golden', 'pan'), ['--kitchen', 'home', '--options', 'today', 'preheat-before-fry.ozen']);
  const html = renderHtml(res, new Phrases([stdPhrases()], { lang: 'ja' }));
  assert.match(html, /フライパンを中火で温める/);
});
