// SPDX-License-Identifier: MIT OR Apache-2.0
importScripts('ozen_bridge.js');

const ready = OzenModule();

self.onmessage = async (event) => {
  const mod = await ready;
  const { build } = await import('./ozen.mjs');
  self.postMessage(event.data.map((input) => build(mod, { sources: [input], emit: 1 | 2 | 4 | 16 })));
};
