// SPDX-License-Identifier: MIT OR Apache-2.0
import test from 'node:test';
import assert from 'node:assert/strict';
import { mkdtempSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { Phrases } from '../lib/phrases.mjs';
import { settingsSpec, renderSettings, settingsText } from '../lib/settings.mjs';
import { root, ozen, stdPhrases, typesResult } from './helpers.mjs';

test('text written from the form values passes the core type check', () => {
  const types = typesResult();
  const spec = settingsSpec(types.types, types.definitions);
  const html = renderSettings(spec, new Phrases([stdPhrases()], { lang: 'ja' }));
  const values = {
    'k-IHStove-count': '1', 'k-IHStove-burners': '2', 'k-CuttingBoard-count': '1', 'k-Knife-count': '1',
    'k-Peeler-count': '1', 'k-Pot-count': '1', 'k-Pot-capacity': '3000', 'k-FryingPan-count': '1',
    'k-FryingPan-diameter': '24', 'k-MixingBowl-count': '1', 'k-SoupBowl-count': '2', 'k-Glass-count': '2',
    'o-cooks': '2', 'o-cleanup': 'afterMeal', 'o-prefer': ['fewerWashes'], 'o-serveAt': '90',
  };
  for (const name of Object.keys(values)) {
    assert.match(html, new RegExp(`name="${name}"`), name);
  }
  const text = settingsText(spec, values);
  assert.match(text.kitchen, /diameter: 24 cm/);
  assert.match(text.options, /serveAt: 90 min/);
  const dir = mkdtempSync(join(tmpdir(), 'ozen-settings-'));
  writeFileSync(join(dir, 'kitchen.ozen'), text.kitchen);
  writeFileSync(join(dir, 'options.ozen'), text.options);
  const out = ozen(['check', '--format', 'json', '-k', join(dir, 'kitchen.ozen'), '--options', join(dir, 'options.ozen'),
    'small.ozen'], join(root, 'renderers', 'test', 'inputs'));
  const res = JSON.parse(out.stdout);
  assert.deepEqual(res.diagnostics.filter((d) => d.severity === 'error'), [], text.kitchen + text.options);
  assert.equal(res.level, 2);
});
