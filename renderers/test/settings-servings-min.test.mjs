// SPDX-License-Identifier: MIT OR Apache-2.0
import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { join } from 'node:path';
import { settingsSpec } from '../lib/settings.mjs';
import { root } from './helpers.mjs';

test('the servings input starts at one', () => {
  const out = readFileSync(join(root, 'tests', 'golden', 'api', 'types-json.out'), 'utf8');
  const { types } = JSON.parse(out.slice(0, out.lastIndexOf('\nexit')));
  const servings = settingsSpec(types, null).options.find((f) => f.name === 'servings');
  assert.equal(servings.min, '1');
});
