// SPDX-License-Identifier: MIT OR Apache-2.0
import test from 'node:test';
import assert from 'node:assert/strict';
import { Phrases } from '../lib/phrases.mjs';

test('the user language, then English, then the mechanical display', () => {
  const ph = new Phrases([{ name: 'std', files: { ja: 'both = 両方', en: 'both = both\nonlyEn = english' } }], { lang: 'ja' });
  assert.equal(ph.format('both'), '両方');
  assert.equal(ph.stats.fallbacks, 0);
  assert.equal(ph.format('onlyEn'), 'english');
  assert.equal(ph.stats.fallbacks, 1);
  assert.equal(ph.format('neither', {}, 'slice(onion, 3 mm)'), 'slice(onion, 3 mm)');
  assert.equal(ph.stats.fallbacks, 2);
  assert.deepEqual([...ph.missing].sort(), ['ja: neither', 'ja: onlyEn']);
});
