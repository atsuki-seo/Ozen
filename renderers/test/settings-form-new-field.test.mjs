// SPDX-License-Identifier: MIT OR Apache-2.0
import test from 'node:test';
import assert from 'node:assert/strict';
import { Phrases } from '../lib/phrases.mjs';
import { settingsSpec, renderSettings, settingsText } from '../lib/settings.mjs';
import { stdPhrases, typesResult } from './helpers.mjs';

test('a field added to the type definitions appears in the form', () => {
  const { types, definitions } = typesResult();
  types.equipment.find((e) => e.kind === 'FryingPan').fields.push(
    { name: 'lidCount', type: 'Count', min: '0', max: null, default: '0' });
  types.equipment.push({ kind: 'Steamer', category: 'tool', fields: [{ name: 'count', type: 'Count', min: '0', max: null, default: '1' }] });
  types.options.push({ name: 'saltLimit', type: 'Mass?', min: null, max: null, default: null });
  const spec = settingsSpec(types, definitions);
  const html = renderSettings(spec, new Phrases([stdPhrases()], { lang: 'en' }));
  for (const id of ['k-FryingPan-lidCount', 'k-Steamer-count', 'o-saltLimit']) {
    assert.match(html, new RegExp(`<label for="${id}">`), id);
    assert.match(html, new RegExp(`id="${id}"`), id);
  }
  assert.match(html, /id="o-saltLimit"[^>]*data-unit="g"/);
  const text = settingsText(spec, { 'k-FryingPan-count': '1', 'k-FryingPan-lidCount': '1', 'k-Steamer-count': '1', 'o-saltLimit': '6' });
  assert.match(text.kitchen, /fryingPan: FryingPan \{ lidCount: 1 \}/);
  assert.match(text.kitchen, /steamer: Steamer/);
  assert.match(text.options, /saltLimit: 6 g/);
});
