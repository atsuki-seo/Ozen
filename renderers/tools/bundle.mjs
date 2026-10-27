// SPDX-License-Identifier: MIT OR Apache-2.0
import { mkdirSync, readFileSync, writeFileSync } from 'node:fs';
import { dirname, join, relative, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const renderers = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const root = resolve(renderers, '..');

function names(list) {
  return list.split(',').map((s) => s.trim()).filter(Boolean).map((s) => {
    const [from, as] = s.split(/\s+as\s+/);
    return { from, as: as ?? from };
  });
}

function parse(file) {
  const key = relative(renderers, file).split('\\').join('/');
  const imports = [];
  const exports = [];
  const requires = (spec, list) => {
    const target = spec.startsWith('.') ? resolve(dirname(file), spec) : null;
    imports.push({ spec, target, names: names(list) });
    return target;
  };
  let code = readFileSync(file, 'utf8');
  code = code.replace(/^import\s*\{([^}]*)\}\s*from\s*['"]([^'"]+)['"];?/gm, (_, list, spec) => {
    requires(spec, list);
    return '';
  });
  code = code.replace(/^export\s*\{([^}]*)\}\s*from\s*['"]([^'"]+)['"];?/gm, (_, list, spec) => {
    requires(spec, list);
    exports.push(...names(list).map((n) => ({ from: n.as, as: n.as })));
    return '';
  });
  code = code.replace(/^export\s*\{([^}]*)\};?/gm, (_, list) => {
    exports.push(...names(list));
    return '';
  });
  code = code.replace(/^export\s+(async\s+function\*?|function\*?|class|const|let|var)\s+([A-Za-z_$][\w$]*)/gm, (_, kind, name) => {
    exports.push({ from: name, as: name });
    return `${kind} ${name}`;
  });
  const left = /^\s*(import|export)\b.*$/m.exec(code);
  if (left) {
    throw new Error(`${key}: cannot bundle: ${left[0].trim()}`);
  }
  return { key, file, imports, exports, code };
}

export function bundle(entry = join(renderers, 'lib', 'render.mjs')) {
  const modules = new Map();
  const order = [];
  const visit = (file, stack) => {
    if (stack.includes(file)) {
      throw new Error(`import cycle: ${[...stack, file].map((f) => relative(renderers, f)).join(' -> ')}`);
    }
    if (modules.has(file)) {
      return;
    }
    const mod = parse(file);
    modules.set(file, mod);
    for (const imp of mod.imports) {
      if (imp.target) {
        visit(imp.target, [...stack, file]);
      }
    }
    order.push(mod);
  };
  visit(resolve(entry), []);
  const keyOf = (file) => modules.get(file).key;
  const body = order.map((mod) => {
    const head = mod.imports.map((imp) => {
      const source = imp.target ? `modules[${JSON.stringify(keyOf(imp.target))}]` : `external(${JSON.stringify(imp.spec)})`;
      const fields = imp.names.map((n) => (n.from === n.as ? n.from : `${n.from}: ${n.as}`)).join(', ');
      return `  const { ${fields} } = ${source};\n`;
    }).join('');
    const fields = mod.exports.map((e) => (e.from === e.as ? e.from : `${e.as}: ${e.from}`)).join(', ');
    return `modules[${JSON.stringify(mod.key)}] = (function () {\n  'use strict';\n${head}${mod.code}\n  return Object.freeze({ ${fields} });\n})();\n`;
  }).join('');
  const license = readFileSync(join(renderers, 'vendor', 'README.md'), 'utf8').split('\n').find((l) => /fluent/i.test(l)) ?? '';
  return '/* Ozen renderer, bundled from renderers/lib by renderers/tools/bundle.mjs. Licensed MIT OR Apache-2.0.\n' +
    ` * Includes @fluent/bundle (Apache-2.0, renderers/vendor/fluent-bundle/LICENSE). ${license.replace(/\*\//g, '')}\n */\n` +
    '(function (global) {\n' +
    'const modules = {};\n' +
    "const external = (name) => { throw new Error('the renderer imports ' + name + ', which a browser does not have'); };\n" +
    body +
    `global.OzenRenderer = modules[${JSON.stringify(keyOf(resolve(entry)))}];\n` +
    "})(typeof globalThis !== 'undefined' ? globalThis : this);\n";
}

if (process.argv[1] && fileURLToPath(import.meta.url) === resolve(process.argv[1])) {
  const out = process.argv[2] ?? join(root, 'build', 'renderer', 'ozen-renderer.js');
  mkdirSync(dirname(out), { recursive: true });
  const text = bundle();
  writeFileSync(out, text);
  console.log(`bundle: ${out} (${text.length} bytes)`);
}
