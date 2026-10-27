// SPDX-License-Identifier: MIT OR Apache-2.0
import test from 'node:test';
import assert from 'node:assert/strict';
import { Phrases } from '../lib/phrases.mjs';
import { diagText } from '../lib/names.mjs';
import { stdPhrases } from './helpers.mjs';

const washInserted = { code: 'SCH006', name: 'washInserted', args: ['board#1', 'rawMeat'] };

test('diagnostic text comes from the diag- phrase with its arguments', () => {
  const ja = new Phrases([stdPhrases()], { lang: 'ja' });
  assert.equal(diagText(ja, washInserted, { unit: () => 'まな板' }), 'まな板を洗う作業を入れました（生肉の汚れ）');
  const en = new Phrases([stdPhrases()], { lang: 'en' });
  assert.equal(diagText(en, washInserted, { unit: () => 'cutting board' }), 'A wash of cutting board was inserted (dirt: raw meat)');
});

test('identifiers in diagnostic arguments are looked up as words', () => {
  const ja = new Phrases([stdPhrases()], { lang: 'ja' });
  const variant = { code: 'SCH005', name: 'variantChosen', args: ['cookRice', 'pot', 'cooker', 'earlier'] };
  assert.equal(diagText(ja, variant), '「ご飯を炊く」は鍋で行います（炊飯器より早く終わるため）');
  assert.equal(diagText(ja, { code: 'ASM007', name: 'provisionalValue', args: ['cut'] }), '「切る」の時間は暫定の値です');
  const before = ja.stats.fallbacks;
  diagText(ja, { code: 'ASM007', name: 'provisionalValue', args: ['noSuchAction'] });
  assert.equal(ja.stats.fallbacks, before + 1);
});

test('a diagnostic without a phrase falls back to its words', () => {
  const ph = new Phrases([{ name: 'std', files: {} }], { lang: 'ja' });
  assert.equal(diagText(ph, washInserted), 'wash inserted');
});
