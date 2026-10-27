// SPDX-License-Identifier: MIT OR Apache-2.0
import { Phrases, layerOrder } from './phrases.mjs';
import { renderRecipe } from './recipe.mjs';
import { renderSchedule } from './schedule.mjs';
import { renderShopping } from './shopping.mjs';
import { renderHtml } from './html.mjs';
import { renderBook } from './book.mjs';
import { settingsSpec, settingsText, renderSettings } from './settings.mjs';

export { Phrases, layerOrder, renderRecipe, renderSchedule, renderShopping, renderHtml, renderBook, settingsSpec, settingsText, renderSettings };

const KINDS = { recipe: renderRecipe, schedule: renderSchedule, shopping: renderShopping, html: renderHtml };

export function render(result, { phrases, lang = 'ja', dialect, kind = 'html', prepBefore } = {}) {
  const ph = phrases instanceof Phrases ? phrases : new Phrases(layerOrder(phrases || {}), { lang });
  const fn = KINDS[kind];
  if (!fn) {
    throw new Error(`unknown kind: ${kind}`);
  }
  return fn(result, ph, { dialect, prepBefore });
}
