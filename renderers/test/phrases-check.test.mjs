// SPDX-License-Identifier: MIT OR Apache-2.0
import test from 'node:test';
import assert from 'node:assert/strict';
import { mkdtempSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { check } from '../tools/phrases-check.mjs';

const defs = {
  definitions: [{ kind: 'action', name: 'fry', params: [{ name: 'x' }, { name: 'time' }], members: [] }],
  diagnostics: [],
};

function dir(ja) {
  const d = mkdtempSync(join(tmpdir(), 'phrases-'));
  writeFileSync(join(d, 'ja.ftl'), ja);
  writeFileSync(join(d, 'en.ftl'), 'action-fry = Fry { $x } for { $time }\n');
  return d;
}

test('variables outside the signature are reported', () => {
  assert.equal(check(defs, dir('action-fry = { $x }を{ $time }炒める\n')).unknown, 0);
  assert.equal(check(defs, dir('action-fry = { $x }を{ $heat }で炒める\n')).unknown, 1);
});

test('the kind of a food argument is allowed', () => {
  assert.equal(check(defs, dir('action-fry = { $x-kind -> [grain] 炒る *[other] { $x }を炒める }\n')).unknown, 0);
});
