// SPDX-License-Identifier: MIT OR Apache-2.0
export const inputs = [
  'action say(text)\nrecipe r() {\n  say("玉ねぎ🧅を切る")\n}\n',
  'action a(x)\nrecipe r() {\n  // コメント🧅\n  a(1 kg + 500 g)\n  a("カレー🍛")\n}\n',
  'action a(x)\nrecipe r() {\n  let s = "🧅🥕🥔"\n  a(1 g + 2 min)\n}\n',
  'action a(x)\nmacro twice(v) { quote { a($v); a($v) } }\nrecipe r() {\n  twice("ごはん")\n}\n',
];
