// SPDX-License-Identifier: MIT OR Apache-2.0
import test from 'node:test';
import assert from 'node:assert/strict';
import { keyFor } from '../lib/phrases.mjs';
import { stdPhrases } from './helpers.mjs';

test('keys are the kind prefix and the kebab-case name', () => {
  assert.equal(keyFor('action', 'cookRice'), 'action-cook-rice');
  assert.equal(keyFor('ingredient', 'CurryRoux'), 'ingredient-curry-roux');
  assert.equal(keyFor('diag', 'invalidUtf8'), 'diag-invalid-utf8');
  assert.equal(keyFor('part', 'shoulderLoin'), 'part-shoulder-loin');
});

test('every std phrase key is kebab-case', () => {
  for (const text of Object.values(stdPhrases().files)) {
    for (const line of text.split('\n')) {
      const m = /^([^\s#=][^=]*?)\s*=/.exec(line);
      if (m) assert.match(m[1], /^[a-z][a-z0-9]*(-[a-z0-9]+)*$/, m[1]);
    }
  }
});
