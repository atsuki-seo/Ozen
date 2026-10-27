// SPDX-License-Identifier: MIT OR Apache-2.0
export const KINDS = ['recipe', 'schedule', 'shopping', 'html', 'settings'];
export const LANGS = ['ja', 'en'];

export function runCases(api, inputs) {
  return inputs.map((input) => {
    const out = {};
    for (const lang of LANGS) {
      for (const kind of KINDS) {
        try {
          out[`${lang}/${kind}`] = api.render(input.result, { phrases: input.phrases, lang, kind });
        } catch (e) {
          out[`${lang}/${kind}`] = `error: ${e.message}`;
        }
      }
    }
    return out;
  });
}
