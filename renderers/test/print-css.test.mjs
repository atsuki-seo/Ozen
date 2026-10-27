// SPDX-License-Identifier: MIT OR Apache-2.0
import test from 'node:test';
import assert from 'node:assert/strict';
import { join } from 'node:path';
import { Phrases } from '../lib/phrases.mjs';
import { renderHtml } from '../lib/html.mjs';
import { root, result, stdPhrases } from './helpers.mjs';

test('the HTML carries print styles inline', () => {
  const ph = new Phrases([stdPhrases()], { lang: 'ja' });
  const html = renderHtml(result(join(root, 'samples', 'menuA'), ['--entry', 'yakisoba', '--kitchen', 'home']), ph);
  assert.match(html, /<style>[^<]*@media print\{[^<]*<\/style>/);
});
