// SPDX-License-Identifier: MIT OR Apache-2.0
import test from 'node:test';
import assert from 'node:assert/strict';
import { Phrases, layerOrder } from '../lib/phrases.mjs';

const src = (name, text) => ({ name, files: { ja: text, en: text } });

test('std, then dependencies by name, then the project; later layers win', () => {
  const sources = layerOrder({
    std: src('std', 'x = std\ny = std\nz = std'),
    deps: [src('zeta', 'x = zeta\ny = zeta'), src('alpha', 'x = alpha\ny = alpha')],
    project: src('book', 'x = mine'),
  });
  assert.deepEqual(sources.map((s) => s.name), ['std', 'alpha', 'zeta', 'book']);
  const ph = new Phrases(sources, { lang: 'ja' });
  assert.equal(ph.format('x'), 'mine');
  assert.equal(ph.format('y'), 'zeta');
  assert.equal(ph.format('z'), 'std');
});
