// SPDX-License-Identifier: MIT OR Apache-2.0
import { execFileSync } from 'node:child_process';
import { mkdtempSync, rmSync, writeFileSync } from 'node:fs';
import { createRequire } from 'node:module';
import { tmpdir } from 'node:os';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { inputs } from './inputs.mjs';

const root = resolve(dirname(fileURLToPath(import.meta.url)), '../..');
const require = createRequire(import.meta.url);
const OzenProto = require(join(root, 'build/wasm/ozenproto_bridge.js'));
const native = join(root, 'build/gcc-release/ozenproto');

export function expand(mod, text) {
  const bytes = new TextEncoder().encode(text);
  const ptr = mod._malloc(bytes.length + 1);
  mod.HEAPU8.set(bytes, ptr);
  const out = mod._ozpw_expand(ptr, bytes.length);
  mod._free(ptr);
  if (out === 0) {
    return null;
  }
  const result = mod.UTF8ToString(out);
  mod._ozpw_free(out);
  return result;
}

const mod = await OzenProto();
const dir = mkdtempSync(join(tmpdir(), 'ozen-bridge-'));
let mismatches = 0;
try {
  inputs.forEach((text, i) => {
    const file = join(dir, `in${i}.ozen`);
    writeFileSync(file, text);
    let want;
    try {
      want = execFileSync(native, [file], { encoding: 'utf8' });
    } catch (e) {
      want = e.stdout;
    }
    const got = expand(mod, text);
    if (got !== want) {
      mismatches++;
      console.error(`mismatch for input ${i}:\n${got}\n--- native ---\n${want}`);
    }
  });
} finally {
  rmSync(dir, { recursive: true, force: true });
}
console.log(`bridge: ${inputs.length} inputs, ${mismatches} mismatches`);
process.exit(mismatches === 0 ? 0 : 1);
