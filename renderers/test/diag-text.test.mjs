// SPDX-License-Identifier: MIT OR Apache-2.0
import test from 'node:test';
import assert from 'node:assert/strict';
import { Phrases } from '../lib/phrases.mjs';
import { diagText } from '../lib/names.mjs';
import { stdPhrases } from './helpers.mjs';

const washInserted = { code: 'SCH006', name: 'washInserted', args: ['board#1', 'rawMeat'] };

test('diagnostic text comes from the diag- phrase with its arguments', () => {
  const ja = new Phrases([stdPhrases()], { lang: 'ja' });
  assert.equal(diagText(ja, washInserted), 'board#1 を洗う作業を入れました（rawMeat の汚れ）');
  const en = new Phrases([stdPhrases()], { lang: 'en' });
  assert.equal(diagText(en, washInserted), 'A wash of board#1 was inserted (dirt: rawMeat)');
});

test('a diagnostic without a phrase falls back to its words', () => {
  const ph = new Phrases([{ name: 'std', files: {} }], { lang: 'ja' });
  assert.equal(diagText(ph, washInserted), 'wash inserted');
});
