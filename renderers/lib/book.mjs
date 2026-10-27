// SPDX-License-Identifier: MIT OR Apache-2.0
import { escapeXml } from './xml.mjs';
import { dishSections, page } from './html.mjs';
import { dishTitle } from './recipe.mjs';

export function renderBook(index, results, ph, opts = {}) {
  const title = opts.title ?? ph.format('heading-contents');
  const entries = index.entries.filter((e) => results.has(`${e.module}:${e.entry}`));
  const toc = entries.map((e, i) => {
    const name = dishTitle(ph, e.entry);
    return `<li><a href="#entry-${i + 1}">${escapeXml(name)}</a></li>`;
  });
  const chapters = entries.map((e, i) => {
    const result = results.get(`${e.module}:${e.entry}`);
    return `<section class="chapter" id="entry-${i + 1}"><h1>${escapeXml(dishTitle(ph, e.entry))}</h1>${dishSections(result, ph, opts)}</section>`;
  });
  return page(title, ph.lang, `<h1>${escapeXml(title)}</h1><nav><h2>${escapeXml(ph.format('heading-contents'))}</h2><ol>${toc.join('')}</ol></nav>${chapters.join('')}`);
}
