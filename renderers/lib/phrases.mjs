// SPDX-License-Identifier: MIT OR Apache-2.0
import { FluentBundle, FluentResource } from '../vendor/fluent-bundle/index.js';

export function kebab(name) {
  return name.replace(/([a-z0-9])([A-Z])/g, '$1-$2').replace(/([A-Z])([A-Z][a-z])/g, '$1-$2').toLowerCase();
}

export function keyFor(kind, name) {
  return `${kind}-${kebab(name)}`;
}

export function layerOrder({ std = null, deps = [], project = null }) {
  const sorted = [...deps].sort((a, b) => (a.name < b.name ? -1 : a.name > b.name ? 1 : 0));
  return [std, ...sorted, project].filter(Boolean);
}

export class Phrases {
  constructor(sources, { lang = 'ja' } = {}) {
    this.lang = lang;
    this.stats = { fallbacks: 0, errors: 0, unresolved: 0 };
    this.missing = new Set();
    this.bundles = new Map();
    for (const l of new Set([lang, 'en'])) {
      const bundle = new FluentBundle(l, { useIsolating: false });
      for (const source of sources) {
        const text = source.files?.[l];
        if (text) {
          this.stats.errors += bundle.addResource(new FluentResource(text), { allowOverrides: true }).length;
        }
      }
      this.bundles.set(l, bundle);
    }
  }

  has(key) {
    return [...this.bundles.values()].some((b) => b.getMessage(key)?.value);
  }

  formatAttr(key, attr, args = {}) {
    for (const l of [this.lang, 'en']) {
      const bundle = this.bundles.get(l);
      const pattern = bundle?.getMessage(key)?.attributes?.[attr];
      if (pattern) {
        return this.resolve(bundle, pattern, args);
      }
    }
    return null;
  }

  resolve(bundle, pattern, args) {
    const errors = [];
    const text = bundle.formatPattern(pattern, args, errors);
    for (const e of errors) {
      if (e instanceof ReferenceError && /Unknown variable/.test(e.message)) {
        this.stats.unresolved++;
      } else {
        this.stats.errors++;
      }
    }
    return text;
  }

  format(key, args = {}, fallback = null) {
    for (const l of [this.lang, 'en']) {
      const bundle = this.bundles.get(l);
      const message = bundle?.getMessage(key);
      if (!message?.value) {
        continue;
      }
      if (l !== this.lang) {
        this.stats.fallbacks++;
        this.missing.add(`${this.lang}: ${key}`);
      }
      return this.resolve(bundle, message.value, args);
    }
    this.stats.fallbacks++;
    this.missing.add(`${this.lang}: ${key}`);
    return fallback ?? key;
  }
}
