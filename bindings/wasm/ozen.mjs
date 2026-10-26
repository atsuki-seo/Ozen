// SPDX-License-Identifier: MIT OR Apache-2.0
function copyBytes(mod, bytes) {
  const ptr = mod._malloc(bytes.length + 1);
  mod.HEAPU8.set(bytes, ptr);
  mod.HEAPU8[ptr + bytes.length] = 0;
  return ptr;
}

export function build(mod, { sources, kitchen = null, options = null, entry = null, emit = 0 }) {
  const encoder = new TextEncoder();
  const owned = [];
  const keep = (bytes) => {
    const ptr = copyBytes(mod, bytes);
    owned.push(ptr);
    return ptr;
  };
  const n = sources.length;
  const table = mod._malloc(4 * (3 * n + 1));
  owned.push(table);
  sources.forEach((s, i) => {
    const text = encoder.encode(s.text);
    mod.HEAPU32[(table >> 2) + i] = keep(encoder.encode(s.path));
    mod.HEAPU32[(table >> 2) + n + i] = keep(text);
    mod.HEAPU32[(table >> 2) + 2 * n + i] = text.length;
  });
  const optional = (s) => (s === null ? [0, 0] : (() => {
    const bytes = encoder.encode(s);
    return [keep(bytes), bytes.length];
  })());
  const [k, kl] = optional(kitchen);
  const [o, ol] = optional(options);
  const [e] = optional(entry);
  const out = mod._ozw_build(table, table + 4 * n, table + 8 * n, n, k, kl, o, ol, e, emit);
  owned.forEach((p) => mod._free(p));
  if (out === 0) {
    return null;
  }
  let end = out;
  while (mod.HEAPU8[end] !== 0) {
    end++;
  }
  const result = new TextDecoder().decode(mod.HEAPU8.subarray(out, end));
  mod._ozw_free(out);
  return result;
}
