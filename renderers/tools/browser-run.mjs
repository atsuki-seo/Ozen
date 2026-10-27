// SPDX-License-Identifier: MIT OR Apache-2.0
import { readFileSync, writeFileSync } from 'node:fs';
import * as api from '../lib/render.mjs';
import { runCases } from './browser-cases.mjs';

const [inputs, out] = process.argv.slice(2);
writeFileSync(out, JSON.stringify(runCases(api, JSON.parse(readFileSync(inputs, 'utf8')))));
