// SPDX-License-Identifier: MIT OR Apache-2.0
import { readFileSync, writeFileSync } from 'node:fs';
import { createRequire } from 'node:module';
import { build } from './ozen.mjs';

const [bridge, inputsPath, outPath] = process.argv.slice(2);
const OzenModule = createRequire(import.meta.url)(bridge);
const mod = await OzenModule();
const inputs = JSON.parse(readFileSync(inputsPath, 'utf8'));
writeFileSync(outPath, JSON.stringify(inputs.map((input) => build(mod, { sources: [input], emit: 1 | 2 | 4 | 16 }))));
