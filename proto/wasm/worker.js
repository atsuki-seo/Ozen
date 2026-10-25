// SPDX-License-Identifier: MIT OR Apache-2.0
importScripts('ozenproto_bridge.js');

const ready = OzenProto();

function expand(mod, text) {
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

self.onmessage = async (event) => {
  const mod = await ready;
  self.postMessage(event.data.map((text) => expand(mod, text)));
};
