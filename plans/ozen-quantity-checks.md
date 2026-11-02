# 動作の引数の単位と、0 以下の量を検査する計画

- 作成日: 2026-11-02
- 参照元: なし（コードの参照元は持たない）。要求は GitHub Issue [atsuki-seo/Ozen#1](https://github.com/atsuki-seo/Ozen/issues/1)（動作の引数の単位が検査されない）と [atsuki-seo/Ozen#2](https://github.com/atsuki-seo/Ozen/issues/2)（マイナスや 0 の時間・量がエラーにならない）。どちらも報告者は sin-45、報告時のコミットは `8db17e6`
- 実装先: `/home/atsuki/ドキュメント/Ozen`、`main`（D81）。着手点は `8db17e6`（紹介ページの OGP の追加まで）
- 前提ルール: `CLAUDE.md`（`main` だけで作業する、push の前に `G-REPO-NOREPLY` を回す、`plans/` と `docs/` は日本語、それ以外のコミットする成果物は英語）、`~/.claude/rules/plan-language.md`、`~/.claude/rules/repo-language.md`、`~/.claude/rules/git-commit.md`、`~/.claude/rules/comment-style.md`、`~/.claude/rules/github.md`。既存の計画 `plans/ozen-roadmap.md`（以下「ロードマップ」）、`plans/ozen-syntax-refresh.md`（以下「記法の計画」）、`plans/ozen-readme-compact.md`（以下「README の計画」）の §5 のゴールを前提ゴールとして引用する（§5）

## 0. この計画の読み方

- 本計画は README の計画の後に続く作業の計画で、番号を引き継ぐ。設計判断は **D69** から、仕様項目は **S695** から付ける（README の計画の最後は D68・S694）。
- タスクの完了条件は、すべて **コマンドで合否が決まる形** で書く。人による確認はタスクにせず、§8 の引き渡しリストに回す。
- 完了条件は、§5 で列挙する前提ゴールと、本計画 §5 で定義する `G-QC-*` で参照する。判定コマンドは §5 のコードブロックが正。表には書かない（表の中では `|` が `\|` になるため）。
- §2 の D69〜D72 は、計画を書く前の 2026-11-02 にユーザーが決めた（Issue の確認の後、方針を 3 つの質問で確認した）。D73〜D87 は、計画を書いた後の 2026-11-02 にユーザーに 1 件ずつ確認して決めた（15 件すべて個別確認。範囲に関わる D85・D86・D78 を先に、その後は番号順。すべて推奨案）。D76 は、確認の途中で示した例（人数が範囲外でも `0 g per serving` は `TYP021`）に合わせて推奨の文を正確にしてから書き込み、S785 を足した。各節の先頭に「決定:」の行があり、推奨と代替の比較は経緯として残す（P0-1）。
- 実装先の事実は、2026-11-02 に `8db17e6` のファイルを読み、`build/core-gcc-release/ozen`（`8db17e6` の core と同じソースからのビルド。全 golden 448 個が合格）で実行して確かめた（§3）。書き終えた後に独立したサブエージェントの監査を 1 回受け、指摘を取り込んだ（§5.3 の末尾と §9）。
- §4 の確かめられる主張には仕様項目 ID を付けてある。§5.3 の網羅表が、S 項目ごとにテストか §8 への引き渡し（理由付き）を示す。

---

## 1. 目的とスコープ

**目的**: 動作に渡す量が、動作の宣言した次元（時間・長さなど）と合わないときにエラーにする（#1）。あわせて、材料の量・動作の時間と大きさ・人数（`servings`）に 0 や負の値を書いたときにエラーにする（#2）。どちらも、今は誤った値がそのまま段取りやレシピの文章に出る（「卵を中火で3 ml炒める」「水を-300 sゆでる」）。同じ誤りが別の書き方（両端に単位を書いた範囲 `3 ml..5 ml`、献立の `make` の引数、`-s` と条件）から通らないようにする。

直した後の診断の例:

```text
$ ozen check case1.ozen          # let x = fry(egg, 3 ml)
error TYP001 dimension case1.ozen:5:20-5:24 args=["Volume", "Time"]: dimension

$ ozen check case2.ozen          # water: -100 ml / boil(water, -300 s)
error TYP021 nonPositiveAmount case2.ozen:3:12-3:19 args=["water", "-100 ml"]: non positive amount
error TYP012 rangeViolation case2.ozen:5:23-5:29 args=["time", "-300 s", "0 s<.."]: range violation
```

（位置と引数の並びは §4 の仕様から書いた見込み。正しい値は各 golden の期待として P1・P2・P3 で手で書く）

### スコープ内

- 動作の呼び出しで、量の引数を宣言の型と照合する（D69）
- 両端に単位を書いた範囲（`3 ml..5 ml`）を区間として評価し、照合と量の検査に掛かるようにする（D85）
- 献立の `make` で料理に渡す量の引数を、料理の引数の宣言と照合する（D86）
- 型の範囲に、下端を含まない書き方を足す（D70・D73・D74・D79）
- 標準ライブラリの動作の時間・大きさに、0 より大きい範囲を付ける（D70）
- 材料の量が 0 以下のときの新しい診断（D71・D75・D76）
- 人数（`servings`）を、レシピと献立の見出し・`make` の引数・`-s`・ビルド時の条件のすべてで 1 以上に制限し、同じ値に診断を 1 つだけ出す（D72・D77）
- `docs/concept.md`・`core/grammar.ebnf`・`core/src/rules.def`・`core/fuzz/ozen.dict`・`schemas/result.schema.json` の更新
- `scripts/goals.sh` への `G-QC-*` の追加と、`CLAUDE.md` の Plan の節への本計画の追加（D82）
- 完了した計画の範囲のゴールを、後の作業で壊れない形に直す（D80・D87）

### スコープ外（理由）

- **量でない値と量の取り違え（`cut(slice, dice)`、`cut(3 mm)`、`fry(egg, 3 min, 3 ml)`）**: 列挙の値を次元の引数に、量を列挙の引数に渡す誤りで、Issue の「単位」とは別の照合（列挙と量の取り違え）が要る。今も通ることは監査で確かめた（§3.7）。最終ゲートの後に別の作業として提案する。
- **状態の型の中の引数（`T<shape: cut(slice, 0 mm)>`、`soaked(time: Time)` の軸の引数）**: 照合の経路（状態の型の照合）が別。同じ穴があるかは P1-2 で確かめて §9 に書き、あれば別の作業として提案する。
- **動作の引数の既定値の検査**: D78 の推奨どおりなら外す（決定で変わる）。
- **`-1..2 g` の読み方（`-(1..2 g)` と読まれる）**: 演算子の優先順位の問題で、Issue に無い。本計画では、この量の下端が 0 以下としてエラーになることだけを確かめる（S731）。
- **ループ（`repeat`）の中で同じ診断が回数分出ること**: 既存の診断（`TYP006` など）と同じ振る舞いで、本計画の診断もそれに従う。まとめるかは診断全体の設計の問題。
- **人数を式で書いた見出し（`servings: Count = 1 - 1`）**: 見出しの既定値の式は今も評価されない（`value_qty` が読めない）。Issue に無い。
- **`servings` の引数を持たない料理への `make gohan(servings: 0)`**: 名前の合わない引数が黙って捨てられる別の挙動（§3.7）。最終ゲートの後に別の作業として提案する。
- **範囲に `..` の無い単独の値（`Time in 0 s`）が黙って無視されること、数だけの範囲（`1..20`）が型の一覧の JSON に出ないこと**: 既存の挙動で、Issue に無い（§3.7）。
- **`servings` の小数（`1.5`）**: 範囲 `1..` は小数を許す今の閉区間の判定のまま。
- **`buy Naan 0 pcs` のような買う量、分ける量**: Issue は材料の量だけを挙げている。同じ穴があるかは P3-2 で確かめて §9 に書く。
- **温度の引数の範囲**: 負の温度（冷凍庫の -18 °C）が正しい値なので、範囲を付けない（D70）。
- **語彙の表（`spec/vocab/actions.yaml`）の更新**: 表は引数の次元だけを持ち、範囲を持たない。表から標準ライブラリを作る `tools/std-from-vocab/convert.py` はどのゴールからも呼ばれていない（R7）。
- **Issue へのコメントと閉じる操作の文面**: D84 で決め、最終ゲートの後にユーザーの確認を取ってから行う。
- **人による確認**: §8 で引き渡す。

---

## 2. 設計判断

### D69. 動作の量の引数の検査（#1） 【決定済み】

- **決定:** 動作の呼び出しで、宣言が次元の型の引数に渡した既知の量を宣言と照合し、次元が違えば既存の `TYP001 dimension` を出す。単位の無い数も次元の無い量としてエラーにする。（2026-11-02、ユーザー確認）
- 経緯: 型と値を照合する `check_value`（`core/src/sema.c:2640`）はあるが、レシピの引数・台所の設定・ビルド時の条件でしか呼ばれていない。動作を呼ぶ `call_action`（`core/src/sema.c:4046`）は、食材の引数の状態だけを照合している（§3.1）。

### D70. 0 と負の時間・大きさを弾く方法 【決定済み】

- **決定:** 型の範囲に下端を含まない書き方を足し、標準ライブラリの動作の `time`・`size` に付けて、0 と負の値を `TYP012 rangeViolation` にする。温度の引数には付けない。綴りは `0 s<..` を第一候補とし、パーサーとの衝突を調べて決める（D73）。（2026-11-02、ユーザー確認）
- 代替だった案: 文法を変えずに `in 0 s..` と書き、負の値だけを弾く。`simmer(x, 0 min)` が通ったままになる。

### D71. 材料の量の診断 【決定済み】

- **決定:** 材料の量が 0 以下なら、新しい診断（エラー）を出す。範囲の量は下端で判定し、`[any amount]` は対象外。（2026-11-02、ユーザー確認）
- 代替だった案: `TYP012 rangeViolation` を使い回し、範囲の欄に `0<..` などを入れる。宣言の無い範囲を表示することになる。

### D72. 人数（servings）の範囲 【決定済み】

- **決定:** レシピの見出しの `servings` と、ビルド時の条件の `servings` の両方を `Count in 1..` で検査する。（2026-11-02、ユーザー確認）
- 代替だった案: ビルド時の条件（`Options.servings`）だけ。レシピの見出しの `servings: 0` が通ったままになる。

### D73. 下端を含まない範囲の綴り 【決定済み】

- **決定:** 推奨のとおり、綴りは `a<..`（上端もあるときは `a<..b`）。`<` と `..` は空白を挟まず続けて書く。（2026-11-02、ユーザー確認）
- **推奨**: `a<..`（上端もあるときは `a<..b`）。`<` と `..` は空白を挟まず続けて書く。読みは「a < 値 ..」で、`<` が「a より大きい」を表す。
  - 衝突の調査（§3.2）: 今のパーサーでは、型の `in` の後の `0 s<..` は構文エラー（`SYN003 args=["<"]`）になるので、今の正しいプログラムの読み方は変わらない。量の後の `<` は単位の一部として読まれない（単位は名前と `/名前` だけ）。式の中の `3 min<..5 min` は今は比較（`3 min < ..5 min`）と読まれるが、D74 の推奨どおり型の範囲だけで読むなら式の読み方は変わらない。
  - 型の引数の `<`（`T<shape: cut>`）とは、`<` の前が量か数であることと、直後が `..` であることで区別がつく。
- 代替: `a^..`（Raku の `^..` に倣う）。他の言語に前例があるが、`^` は今の字句に無い記号で、新しいトークンが要り、読み手に意味が伝わりにくい。
- 代替: `>a`（`Time in >0 s`）。読みやすいが、上端との組み合わせ（`>0 s..10 min`）の形が範囲の文法から外れ、規則が 2 つに分かれる。

### D74. 下端を含まない書き方を使える場所 【決定済み】

- **決定:** 推奨のとおり、下端を含まない書き方は型の `in` の後の範囲だけで使え、新しい文法規則 `type_range` を立てる。式の中の範囲と量の範囲のリテラルには足さない。（2026-11-02、ユーザー確認）
- **推奨**: 型の `in` の後の範囲だけ（台所の設定・ビルド時の条件・レシピの引数・動作の引数の型）。式の中の範囲・量の範囲のリテラル（`15..20 min`）には足さない。文法には新しい規則 `type_range` を立て（`core/grammar.ebnf` と `core/src/rules.def`）、式と共用の `range` は変えない。
  - 理由: 範囲の制約は型に持つ、という今の仕様（`docs/concept.md` §4.2）の中に収まり、式の評価（区間の計算）に「端を含まない」を持ち込まずに済む。式の中の `<` の読み方も変えない。
- 代替: 式の中の範囲にも足す。`let t = 0 s<..10 min` のような値が書けるが、区間の計算・比較・表示のすべてに開いた端の扱いが要り、Issue の解決に必要ない。

### D75. 新しい診断のコードと名前 【決定済み】

- **決定:** 推奨のとおり、`TYP021 nonPositiveAmount`（エラー）を `diag_codes.def` の末尾に足す。引数は `[材料の名前, 書いた量の文字]`、言い回しのキーは `diag-non-positive-amount`、引数の種類は `['name']`。（2026-11-02、ユーザー確認）
- **推奨**: `TYP021 nonPositiveAmount`（エラー）。`core/src/diag_codes.def` の末尾に追記する（既存の番号を変えない。`G-CORE-DIAG-APPEND`）。引数は 2 つで、`[材料の名前, 書いた量の文字]`（例 `["water", "-100 ml"]`）。言い回しのキーは `diag-non-positive-amount`、`renderers/lib/names.mjs` の引数の種類は `['name']`（`unusedValue` と同じ）。
  - 日本語の文（案）: `{ $arg1 }の量 { $arg2 } は 0 より大きくしてください`。英語の文（案）: `The amount { $arg2 } of { $arg1 } must be more than zero`。
- 代替: 名前を `amountNotPositive` にする。並びは自然だが、既存の名前（`vagueAmount`・`servingsUnknown`）は「形容 + 名詞」の順で、推奨のほうが揃う。
- 代替: 分類を `LIN`（量の不足 `insufficient` と同じ）にする。`LIN` は消費の検査（二重消費・不足）の分類で、書いた値そのものの誤りは `TYP`（`vagueAmount` と同じ）が合う。

### D76. 材料の量を判定する値と、人数の誤りとの重なり 【決定済み】

- **決定:** 推奨のとおり、材料の量は書いた値（`per serving` は人数を掛ける前）で判定する。人数が範囲外のときは、式に `servings` を含む量に `TYP021` を、`per serving` の総量に `TYP020` を足さない。`0 g per serving` は人数が範囲外でも `TYP021`。（2026-11-02、ユーザー確認）
- **推奨**: 書いた式を評価した値（`per serving` なら人数を掛ける前）で判定する。下端が 0 以下ならエラー。人数が決まらない（`TYP020`）ときも、書いた値だけで判定できるので出す。`per serving` の量は人数を掛ける前に判定するので、人数の誤りに左右されない（人数が範囲外でも `0 g per serving` は `TYP021`、`100 g per serving` は診断なし）。人数が範囲外（D77 の `TYP012` を出した）ときは、人数を未知として扱い、式に `servings` を含む量（`150 ml * servings`）には `TYP021` を、`per serving` の総量には `TYP020` を足さない。
  - 理由: 誤りは書いた量にあり、診断の引数にも書いた量を出せる。人数の誤りが原因で 0 になった量にまで診断を出すと、同じ原因の診断が 2 つ以上出る（監査の指摘。`-s 0` で `150 ml * servings` が `0 ml` になることを確かめた）。
- 代替: 人数を掛けた後の総量で判定する。`150 g * servings` と同じ値を見られるが、人数の誤りと量の誤りの診断が重なり、人数が決まらないときに判定できない。

### D77. 人数の範囲外の診断と、同じ値に 1 つだけ出す規則 【決定済み】

- **決定:** 推奨のとおり、人数の範囲外は既存の `TYP012` で、同じ値には 1 つだけ出す。条件の側で誤りなら料理の見出しの側では検査しない。見出しに自分で範囲を宣言していればその範囲だけで検査する。（2026-11-02、ユーザー確認）
- **推奨**: 既存の `TYP012 rangeViolation` を使う。引数は `["servings", 書いた値, 範囲の文字]`。同じ値には診断を 1 つだけ出す。具体的には次のとおり。
  - 条件（`-s`・`--options`）から来た人数は、条件の側（`Options.servings` の型 `Count? in 1..`、S741）で検査する。条件の側で誤りなら、料理の見出しの側ではもう検査しない。条件の側で通れば、見出しに自分で宣言した範囲（`servings: Count in 1..20`）でも検査する。
  - 見出しの既定値（条件が無いとき）と `make` の引数は、見出しに自分で範囲を宣言していればその範囲だけで、宣言していなければ `1..` で検査する。
  - 今は `-s 3ml` で同じ `TYP001` が 2 行出る（§3.7）。この規則で、次元の誤りも 1 行になる。
  - 理由: 人数には `Options.servings` の型という宣言があり、範囲の違反として読める。新しい診断を足すより、言い回し・登録の追加が少ない。
- 代替: 人数専用の診断（例 `TYP022 servingsNotPositive`）を足す。文を「人数は 1 人以上にしてください」と具体的にできるが、登録の手間が 1 つ増え、重なりの規則は同じく要る。
- 代替: 重なりを許す（条件と見出しの両方で出す）。実装は簡単だが、今の `TYP001` の重複と同じく、1 つの誤りに 2 行出る。

### D78. 動作の引数の既定値を検査するか 【決定済み】

- **決定:** 推奨のとおり、動作の引数の既定値は本計画では検査しない（スコープ外）。（2026-11-02、ユーザー確認）
- **推奨**: 本計画では検査しない（スコープ外）。既定値は動作の定義の誤りで、呼び出しの誤りではない。呼び出しのたびに検査すると、同じ定義の誤りが呼び出しの数だけ出る。標準ライブラリの既定値（`soak` の `30 min`、`cut` の `3 mm`）が範囲に入ることは、`bounds/soak-default` の golden で確かめる。
- 代替: 既定値を使った呼び出しでも同じ検査をし、診断を既定値の位置に出す。利用者が定義した動作の誤った既定値（`time: Time = 3 ml`）も見つかるが、同じ位置の診断が重複する。
- 代替: 動作の定義を読むときに 1 回だけ検査する。重複は無いが、定義の検査（レベル 0）に新しい経路を足す作業が増える。

### D79. 型の一覧の JSON での、下端を含まない範囲の表し方 【決定済み】

- **決定:** 推奨のとおり、型の一覧の JSON で下端を含まないときだけ `"minExclusive":true` を足し、スキーマに任意の真偽値として足す。`min` は下端の文字のまま。（2026-11-02、ユーザー確認）
- **推奨**: 型の一覧の JSON（`ozen check --format json --emit types` の型のフィールド）で、下端を含まないときだけ `"minExclusive":true` を足す。含むときはキーを出さない。`schemas/result.schema.json` に任意の真偽値として足す。`min` の文字は下端の値（`0 s`）のまま。
  - 理由: 今の出力（golden `api/types-json` など）が、`Options.servings` の `min`（S741）を除いて変わらない。`min` を使う設定画面（`renderers/lib/settings.mjs`）は HTML の `min` 属性に渡すだけで、開いた端を表せないので、今のまま下端を渡す。
- 代替: すべての型の欄に `"minExclusive":false` を出す。読む側が分岐しなくて済むが、型の欄を含む golden がすべて変わる。
- 代替: JSON には出さない。情報が落ち、`min` だけを見る道具が 0 を許すと誤解する。

### D80. 記法の計画の `G-RF-GOLDEN-COUNT` の扱い 【決定済み】

- **決定:** 推奨のとおり、`G-RF-GOLDEN-COUNT` を「`readme-start` にあった golden が今もすべてある」と「`readme-start` の時点の数が N0 + 85」の形に直し、記法の計画の §9 に理由を 1 行足す。（2026-11-02、ユーザー確認）
- **推奨**: `G-RF-GOLDEN-COUNT` を「タグ `readme-start` にあった golden が、今もすべてある（消えた golden が 0）」と「`readme-start` の時点の数が N0 + 85」の 2 つに直す。前者が本来の検査で、後者はタグの中身なので変わらない記録として残す。記法の計画の §9 にも直した理由を 1 行足す（記法の計画が P8 で増える数を直した前例と同じ形）。
  - 理由: 今の形（今の数がちょうど N0 + 85）は、後の計画が golden を 1 つ足すだけで落ちる。本計画は golden を 72 個足す（§5.2）。記法の計画が確かめたかったこと（85 個増やした golden が消えていないこと）を保ったまま、後の計画の golden に左右されない。
- 代替: 「今の数が N0 + 85 以上」にする。直す量は最小だが、記法の計画の golden が消えて別の golden が足された場合に気づけない。
- 代替: 「N0 + 85 + 本計画で足した数」にする。正確だが、後の計画のたびに直す必要が残る。

### D81. 作業の場所と範囲の基点 【決定済み】

- **決定:** 推奨のとおり、`main` で作業し、P0-2 でタグ `qc-start` を打って範囲 `QC=qc-start..HEAD` を数える。push は確認なしで行い、その前に毎回 `G-REPO-NOREPLY` を回す。わざと壊す確認の一時的なコミットは一時的な worktree で作って消し、push しない。（2026-11-02、ユーザー確認）
- **推奨**: `main` で作業する（`CLAUDE.md`）。P0-2 で `main` にタグ `qc-start` を打ち、本計画の差分の範囲を `QC=qc-start..HEAD` で数える。push は、記憶にあるユーザーの指示（Ozen では push の確認は不要）に従い、そのつど確認を取らずに行う。push の前には毎回 `G-REPO-NOREPLY` を回す。範囲に別の作業のコミットが入ったときは、README の計画の D68 と同じく §9 に名指しで書いて除く。わざと壊す確認のための一時的なコミットは、一時的な worktree の中で作って `git reset --hard` で消し、`main` には入れない（push しない）。
- 代替: 作業ブランチ `quantity-checks` を切り、最終ゲートの後に `main` へ `--no-ff` でマージする。別の作業と分けられるが、`CLAUDE.md` の「`main` だけで作業する」に反し、既存のゴールの範囲（`plan-start..HEAD`）の考え方とも揃わない。

### D82. 計画ファイルとゴールの置き場所 【決定済み】

- **決定:** 推奨のとおり、本ファイルを新しい計画として置き、`scripts/goals.sh` に `PLAN_QC`・`QC` と `goal_G_QC_*` を足し、`CLAUDE.md` の Plan の節に 1 行足す。既存の計画は D80・D87 の直しを除いて書き換えない。（2026-11-02、ユーザー確認）
- **推奨**: 本ファイルを新しい計画として置く。`scripts/goals.sh` に `PLAN_QC`・`QC` と `goal_G_QC_*` を足す（`OZ` は既にある）。`CLAUDE.md` の Plan の節に、本計画の名前・ゴールの接頭辞・範囲のタグを 1 行で足す（README の計画の行と同じ形）。既存の計画は D80・D87 の直しを除いて書き換えない。
- 代替: README の計画に Phase を書き足す。ゴールは 1 か所にまとまるが、完了した計画と進行中の作業の区別がつかなくなる。

### D83. 版 【決定済み】

- **決定:** 推奨のとおり、版は `0.1.0` のまま据え置く。（2026-11-02、ユーザー確認）
- **推奨**: `0.1.0` のまま据え置く。まだ公開した版（タグ・配布物）が無く、誤りを直す変更で、正しく書かれたレシピの出力は変わらない。
- 代替: `0.2.0` に上げる。以前は通っていたレシピがエラーになる（入力の受け付けが狭まる）ことを版で示せるが、版の文字列を持つ期待（JSON の `ozenVersion` を含む golden など）を直す作業が増える。

### D84. Issue の閉じ方 【決定済み】

- **決定:** 推奨のとおり、最終ゲートと push の後に、各 Issue へ修正の SHA と直った後の `ozen check` の出力をコメントして閉じる。文面は投稿前にユーザーに見せて確認を取る。#2 では再現コードの貼り間違いにも触れる。（2026-11-02、ユーザー確認）
- **推奨**: 最終ゲートと push の後、`gh issue comment` で各 Issue に、直したコミットの SHA と、直した後の `ozen check` の出力（再現コードのもの）を短く書き、`gh issue close` で閉じる。コメントの文面は、送る前にユーザーに見せて確認を取る（外部への投稿のため）。#2 のコメントでは、報告の再現コードが #1 と同じものになっていたことに触れ、出力から読み取った再現を `bounds/issue2` に置いたことを書く。
- 代替: 修正のコミットのメッセージに `Fixes #1` を書き、push で自動で閉じる。手間は少ないが、push のたびに閉じるので、最終ゲートの前に閉じてしまう。報告者に直した後の挙動が伝わりにくい。
- 代替: 閉じずにユーザーに任せる。

### D85. 両端に単位を書いた範囲の評価 【決定済み】

- **決定:** 推奨のとおり、両端に単位を書いた範囲を、両端が既知の量で次元が同じなら区間として評価し、次元が違えば `TYP001` を出す。本計画で直す。（2026-11-02、ユーザー確認）
- **推奨**: 両端に単位を書いた範囲（`3 ml..5 ml`、`-5 min..-1 min`）を、両端が既知の量で次元が同じなら区間として評価する。次元が違えば（`3 ml..5 g`）`TYP001` を出す。これで #1 の照合と #2 の範囲・量の検査が、この書き方にも掛かる。
  - 理由: 今は式の評価（`eval_inner`）に範囲の節の評価が無く、`fry(egg, 3 ml..5 ml)` も `simmer(x, -5 min..-1 min)` も未知の値になって、計画の照合（未知の値は検査しない、S704）をすり抜ける（§3.7）。台所の設定では `10 cm..40 cm` のように両端に単位を書くのが普通で、利用者が書く見込みが高い。材料に `0.5 ml..2 ml` と書くと量が付かず「適量」と表示される今の挙動も、同じ原因で直る。
- 代替: 本計画では扱わず、別の作業にする。変更は小さく済むが、Issue の症状（単位の違う時間、負の時間）が別の書き方で残る。

### D86. 献立の `make` の量の引数 【決定済み】

- **決定:** 推奨のとおり、献立の `make` で料理に渡す量の引数（位置・名前のどちらも）を、料理の見出しの引数の宣言と照合する（次元は `TYP001`、範囲は `TYP012`）。（2026-11-02、ユーザー確認）
- **推奨**: `make soup(t: 3 ml)` のように、献立から料理に渡す量の引数（位置・名前のどちらも）を、料理の見出しの引数の宣言と照合する。次元は `TYP001`、範囲は `TYP012`。動作の引数と同じ関数（§4.12）を使う。人数（`servings`）もこの経路で検査する（D77）。
  - 理由: 今は `make` の食材でない引数を型と照合しない（`eval_make` が値を束縛するだけで、`eval_param` は束縛済みの値を検査せずに戻る。§3.7）。#1 と同じ種類の誤りで、人数の `make soup(servings: 0)` もこの経路を通る。
- 代替: 人数だけを検査し、ほかの量の引数は別の作業にする。変更は小さいが、`make soup(t: 3 ml)` が通ったまま残る。

### D87. 完了した計画の範囲のゴール 【決定済み】

- **決定:** 推奨のとおり、README の計画の最後のコミット `52a6ef1` にタグ `readme-end` を打ち、`RD` を `readme-start..readme-end` に閉じる。README の計画の §9 に理由を 1 行足す。`G-RF-SEPARATION` は本計画でも前提ゴールとして守る。（2026-11-02、ユーザー確認）
- **推奨**: README の計画の範囲（`RD=readme-start..HEAD`）を、README の計画の最後のコミット `52a6ef1` に打つタグ `readme-end` で閉じ（`readme-start..readme-end`）、`G-RD-SCOPE`・`G-RD-ADD-ONLY` がその範囲だけを見るようにする。README の計画の §9 にも理由を 1 行足す。記法の計画の `G-RF-SEPARATION`（期待 `.out` と実装を同じコミットにしない）は、本計画でも守る規則として前提ゴールに入れる。
  - 理由: `G-RD-SCOPE` は、README の計画の後の別の作業（OGP のコミット `2332ca6`・`8db17e6`）で既に落ちている（2026-11-02、`files outside scope: got '2'`）。`G-RD-ADD-ONLY` は、本計画が D80 で `goals.sh` の行を書き換えると落ちる。どちらも、計画が終わった後のコミットを数え続けるためで、README の計画が確かめたかったことは範囲を閉じても変わらない。
- 代替: 完了した計画の範囲のゴールを前提ゴールから外し、P0-2 で合否を記録するだけにする。直す量は無いが、落ちたゴールが一覧に残り続け、本当の失敗と見分けにくい。

---

## 3. 現状の確認結果（2026-11-02、`8db17e6`）

### 3.1 動作の呼び出しと、型と値の照合

- 動作の呼び出しは `call_action`（`core/src/sema.c:4046`）。引数を位置・名前で割り当て、渡されていない引数に既定値を評価する。その後、食材の引数について、状態の照合（`TYP006 stateRequired`・`TYP011 aliasCandidates`）と `where` の照合（`TYP010`）をする。量の引数（`V_QTY`）を宣言の型と照合する処理は無い。
- 型と値の照合は `check_value`（`core/src/sema.c:2640`）。宣言が次元の型なら、`typed_qty` で値を読み、次元が違えば `TYP001`（引数は `[値の次元, 宣言の型の名前]`）、範囲（`base_type` が返す `in` の後）を外れれば `TYP012`（引数は `[名前, 値の文字, 範囲の文字]`）を出す。範囲の判定は閉区間で、値の区間の一部でも外れれば外とする（`v.lo < lo` または `v.hi > hi`）。
- `check_value` は、ソースの節（値を書いた場所）を受け取って読み直す作りで、評価済みの値（変数で渡した量など）を受け取れない。呼ばれるのは、レシピの引数（`sema.c:6133`）、台所の設定のフィールド（`sema.c:2847`）、ビルド時の条件のフィールド（`sema.c:3163`）。`sema.c:2669` は `check_value` の中の再帰。
- 宣言が `Count` のとき、`typed_qty` は `2 pcs`（Count の次元を持つ量）を次元の無い量に直してから比べる。単位の無い数の次元の名前は `Dimensionless`（golden `kitchen/unitless-capacity.out`）。
- 診断の位置は、状態の照合と同じく `arg_site` で引数を書いた場所に向けられる。マクロの差し込み（`$t`）の引数は呼び出し元の位置と trail になり、マクロの本文に書いた量は本文の位置で展開履歴（trail）付きになる（golden `macro/typecheck-after-expand.out`）。
- `base_type`（`sema.c:2605`）は、省略可能（`OptionalType`）を外してから範囲（`RangedType`）を見る。文法では `Count? in 1..` は `RangedType(OptionalType(Count), …)` の入れ子になるので、範囲を外した後の `OptionalType` が型の名前として扱われず、`check_value` は何も検査せずに戻る（S742 で直す）。
- パイプ（`x |> f(a)`）はパーサーの `pipe_into`（`core/src/parse.c:871` 付近）で、左辺を第 1 引数に差し込んだ普通の呼び出しに直される。
- 合成の動作（`steps` を持つ動作）は `call_composite`（`sema.c:3998`）が中の呼び出しを評価する。中の診断を抑える仕組み（`quiet`）は上げない。

### 3.2 範囲の文法とトークン

- 型の範囲は `type := type_union [ 'in' range ]`、`range := sum [ '..' [ sum ] ] | '..' sum`（`core/grammar.ebnf:98`・`141`）。パーサーは `type()`（`core/src/parse.c:1110` 付近）で `in` の後を `range()`（`parse.c:771`）で読み、`Range`・`RangeFrom`・`RangeTo` の節を作る。数だけの範囲（`1..20`）は `NumberRange` の節になる。
- トークンは `<`（`OZNI_T_LT`）と `..`（`OZNI_T_DOTDOT`）が別にある（`core/src/lex.c:239`）。`<..` という一続きのトークンは無い。
- 型の `in` の後の `0 s<..` は、`range()` が `0 s` を範囲でない値として返し、呼び出し側が `<` で `SYN003 args=["<"]` を出す（監査で実行して確かめた）。式の中の `3 min<..5 min` は `Binary <`（`3 min` と `RangeTo`）と読まれ、`check` は `result ok`。
- 範囲を出力するのは、型の一覧の JSON（`ozen check --format json --emit types`。`core/src/defs.c:737` の `types` が `Kitchen`・`Options` と設備・器の種類の設定の型を並べ、各フィールドを `defs.c:597` の `put_field` が `min`・`max` に下端・上端の文字で出す。`Range`・`RangeFrom`・`RangeTo` だけを扱い、`NumberRange` は出さない）と、動作の引数の型の文字（`--emit signatures` などの `params` の `type`。`Time in 0 s<..` のように範囲ごと出る）。`min`・`max` は `renderers/lib/settings.mjs:26` が設定画面の数の入力の `min`・`max` 属性に使う。
- 文法の規則は `core/src/rules.def`（`OZNI_RULE(range)` など）にあり、`G-CORE-GRAMMAR`（`scripts/check-grammar-coverage.py`）が、`grammar.ebnf` の規則名がすべて golden で通ることを確かめている。

### 3.3 標準ライブラリの動作の量の引数

`stdlib/prelude.ozen` で量を取る引数は次の 8 つ（2026-11-02）。温度を取る動作の引数は無い（温度は `OvenSpec.maxTemp: Temperature?` だけ）。

| 動作 | 引数 | 今の型 |
| --- | --- | --- |
| `soak` | `time` | `Time = 30 min` |
| `cut` | `size` | `Length = 3 mm` |
| `fry` | `time` | `Time` |
| `simmer` | `time` | `Time` |
| `boil` | `time` | `Time` |
| `parboil` | `time` | `Time` |
| `chill` | `time` | `Time` |
| `rest` | `time` | `Time` |

### 3.4 再現（2026-11-02、`build/core-gcc-release/ozen check`）

| 入力 | 今の結果 |
| --- | --- |
| `fry(egg, 3 ml)`（#1 の再現コード） | `result ok`、レシピの文章は「卵を中火で3 ml炒める。」 |
| `potato \|> peel \|> cut(slice, 3 g)` | `result ok` |
| `fry(egg, 3)` | `result ok` |
| `water: -100 ml`、`potato: Potato 0 pcs`、`boil(water, -300 s)`、`cut(slice, -3 mm)`、`simmer([x, y], 0 min)`、`recipe a(servings: 0)` | `result ok`。文章は「水 -100 ml」「じゃがいも 0個」「水を-300 sゆでる。」「薄切りにする（-3 mm）」「弱火で0 時間煮込む」 |
| `recipe soup(servings: Count = 0)` | `result ok` |
| `make soup(servings: 0)` | `result ok` |
| `ozen check -s 0`、`-s -1` | `result ok` |
| `recipe a(servings: -2)` | `SYN003 unexpectedToken`（見出しの既定値に負の数を書けない） |
| `recipe soup(servings: Count in 1..20 = 0)` | `TYP012 args=["servings", "0", "1..20"]`（今も出る） |
| `salt: -1..2 g` | `result ok`、文章は「塩 -2〜-1 g」（`-(1..2 g)` と読まれる） |

#2 の報告の「再現コード」は #1 と同じもので、「実際の結果」（水 -100 ml、-300 s ゆでる）と合わない。貼り間違いと見て、出力から再現を組み立てた（`bounds/issue2`、S750）。

### 3.5 診断を足すときに触るもの

`LIN005 unusedValue` を足したコミット（`738a452`・`5e54d49`・`631e097`）から読み取った一覧:

- `core/src/diag_codes.def`（末尾に追記。`G-CORE-DIAG-APPEND` が削除の行 0 を確かめる）
- `core/tests/diag-names.txt`（`G-CORE-DIAG-REGISTRY`）
- `phrases/en.ftl`・`phrases/ja.ftl` の `diag-<名前の kebab>`（`G-STD-PHRASES`・`G-REND-PHRASES-RENDER`）
- `renderers/lib/names.mjs` の `ARG_KINDS`（引数の種類。`unusedValue: ['name']` と同じ形）
- `docs/concept.md` §6.1 の表と `core/tests/severity-examples.txt`（`G-TYPE-SEVERITY-TABLE`）
- 診断の一覧を出す golden（`api/local-not-listed`・`api/sarif-mapping` など）の期待

### 3.6 テストとゴールの仕組み

- golden は `tests/golden/<分類>/<名前>.ozen` と `.out` の組。`.ozen` の 1 行目の `// args:` がコマンドの引数で、`.out` は診断・`result`・`exit` の行。`scripts/run-golden.sh <ozen> <フォルダ>` が `golden: <n> passed, 0 failed, 0 timed out` を出す（フォルダ単位でも回せる。2026-11-02 に `tests/golden/kitchen` で確かめた）。ビルド時の条件は `kitchen today : Options { … }` と書き、`--options today` で選ぶ（golden `kitchen/range-open-ended`）。`-s` は CLI が `kitchen cli : Options { servings: … }` を作って渡す（`cli/src/main.c:599` 付近）。
- 期待を足すコミットは、そのコミットの時点で期待が落ちていること（赤）を `G-REPO-RED-FIRST`（範囲 `plan-start..HEAD`）が確かめる。最初から合格する golden は、別のコミットにして `scripts/red-first-exceptions.txt` に理由を書く前例がある（記法の計画の `ingredient-dimension-ok` など）。
- 期待 `.out` と実装（`core/src`・`stdlib` など）を同じコミットにしないことを `G-RF-SEPARATION`（範囲 `refresh-start..HEAD`）が確かめる。
- `G-RF-GOLDEN-COUNT` は今の golden の数がちょうど `RF_N0 + 85`（= 448）であることを確かめる。2026-11-02 の数は 448（D80）。
- `G-RD-SCOPE` は 2026-11-02 の時点で既に落ちている（D87）。
- 計画の網羅表のテスト名は `cgolden/<分類>/<名前>`・`rtest/<名前>` の形で書くと、`scripts/check-plan-tests.sh` が実在を確かめ、`--mutations` がわざと壊す確認の記録の数を数える。

### 3.7 監査で確かめた、ほかの経路（2026-11-02）

- 両端に単位を書いた範囲は評価されない: `fry(egg, 3 ml..5 ml)` は `debug steps` で `fry(egg, ?)`・`result ok`。材料の `sugar: -5 ml..-1 ml` は量が付かない。数だけの範囲に単位を付けた `0..2 min` は `0..120 s` と評価される（D85）。
- `make` の食材でない引数は型と照合されない: `make soup(t: 3 ml)`（`t: Time`）も位置で渡した `make soup(0)` も `result ok`（D86）。
- 条件の人数は 2 回検査される: 見出しに `servings: Count in 1..20` がある料理に `-s 3ml` を渡すと、同じ `error TYP001 dimension <options>:2:13-2:16 args=["Volume", "Count"]` が 2 行出る（条件のフィールドの検査と、見出しの引数の検査が同じ節を読む。D77）。
- 人数を使う式の量: `-s 0` で `dashi: 150 ml * servings` は `0 ml` になる（D76）。
- 量でない値と量の取り違え（`cut(slice, dice)`、`cut(3 mm)`、`fry(egg, 3 min, 3 ml)`）はどれも `result ok`（スコープ外）。
- `servings` を持たない料理への `make gohan(servings: 0)` は、名前の合わない引数が捨てられて `TYP020` になる（スコープ外）。
- `repeat 3 { peel(water) }` は同じ位置の `TYP006` を 3 行出す（スコープ外）。
- 範囲に `..` の無い `Time in 0 s` は `RangedType` として受け付けられ、範囲は無視される（スコープ外）。

---

## 4. 挙動仕様

確かめられる主張は 1 項目 1 主張で、先頭に `**S<n>**` を付ける。

### 4.1 動作の量の引数（#1、D69）

- **S695** 動作の引数の宣言の型が次元（`Mass`・`Volume`・`Length`・`Time`・`Temperature`・`Count`、または `dimension` で足した次元）で、渡した値が既知の量のとき、値の次元が宣言と違えば `TYP001 dimension` のエラーを出す。引数は `[値の次元, 宣言の型の名前]`（例 `fry(egg, 3 ml)` は `["Volume", "Time"]`）。
- **S696** 単位の無い数（`fry(egg, 3)`）は次元の無い量で、`Time` の宣言に対して S695 のエラーになる（引数は `["Dimensionless", "Time"]`）。
- **S697** 宣言が `Count` のとき、`2 pcs` と `2` はどちらも合格し、`2 g` はエラーになる。
- **S698** 名前で渡した引数（`fry(egg, time: 3 ml)`）も同じ検査をする。
- **S699** パイプで渡した呼び出し（`egg |> fry(3 ml)`）も同じ検査をし、診断の位置は `3 ml`。
- **S700** 診断の位置は値を書いた引数。変数で渡したとき（`let t = 3 ml` の後の `fry(egg, t)`）は、引数 `t` の位置に出る。
- **S701** マクロの本文に書いた量（本文の `fry($x, 3 ml)`）の診断は、本文の位置に展開履歴（trail）付きで出る。
- **S758** マクロに差し込んだ量（本文の `fry($x, $t)` に `3 ml` を渡す）の診断は、呼び出し元で `3 ml` を書いた位置に出る（最上位の呼び出しなら trail は無い）。
- **S702** 合成の動作（`steps` を持つ動作）を呼ぶときも、その動作の宣言に対して検査する。
- **S759** 合成の動作 `quick(x: T, t: Time) { steps { fry(x, t) } }` を `quick(egg, 3 ml)` と呼ぶと、`TYP001` は呼び出し元の `3 ml` の位置に 1 つだけ出る（中の `fry(x, t)` の照合で重ねない）。
- **S703** `dimension` で足した次元の引数（`by: Angle`）にも同じ検査をする。
- **S704** 値が評価の時点で決まらない引数（未知の量）は検査しない。
- **S705** 引数を省いて既定値を使ったときは、既定値を検査しない（D78）。利用者の動作の誤った既定値（`time: Time = 3 ml`）でも診断は出ない。
- **S706** 次元の誤りがあっても、評価は続き、同じレシピの後の誤り（皮をむかずに切る `TYP006` など）も出る。
- **S707** 正しい次元の値（`3 min`、`1 h 30 min`、`3..5 min`、`2 * 90 s`、`time: 3 min`、`heat: high`、`cut(slice, 3 mm)`）は新しい診断を出さない（量でない引数 `heat`・`style` に新しい診断は出ない）。
- **S708** Issue #1 の再現コード（`fry(egg, 3 ml)`）は `TYP001` を 1 つ出し、`ozen check` の終了コードは 1 になる。

### 4.2 両端に単位を書いた範囲（D85）

- **S760** 両端に同じ次元の単位を書いた範囲（`3 min..5 min`）は区間として評価され、`debug steps` に区間（`180..300 s`）で出る。
- **S761** `fry(egg, 3 ml..5 ml)` は `TYP001`（引数 `["Volume", "Time"]`）。
- **S762** 両端の次元が違う範囲（`3 ml..5 g`）は `TYP001`。
- **S763** 材料の `sugar: 0.5 ml..2 ml` は量 `0.5..2 ml` を持ち、レシピの文章は「適量」でなく範囲になる。

### 4.3 献立の `make` の量の引数（D86）

- **S764** `make soup(t: 3 ml)`（料理の見出しが `t: Time`）は、`3 ml` の位置に `TYP001`。
- **S765** `make soup(t: 0 s)`（料理の見出しが `t: Time in 0 s<..`）は、`0 s` の位置に `TYP012`。

### 4.4 下端を含まない範囲（D70・D73・D74・D79）

- **S709** 型の `in` の後に `a<..` と書くと、a より大きい値だけを許す。a ちょうどは範囲外。
- **S710** `a<..b` と書くと、a より大きく b 以下の値を許す（上端は含む）。
- **S711** `<` と `..` の間に空白を入れる（`0 s< ..`）と構文エラー（`SYN003`）になる。
- **S712** 範囲外の値は既存の `TYP012 rangeViolation` で、引数の 3 つ目は書いた範囲の文字（`0 s<..`）。
- **S713** 区間の値（`0..2 min`）は、下端が範囲の下端以下なら範囲外（今の閉区間と同じく、区間の一部でも外れたら外）。
- **S714** 下端を含む今の範囲（`0 s..`）の判定は変わらない（`0 s` は範囲内）。
- **S715** 型の範囲の外（式の中）の `3 min<..5 min` の読み方は変わらない（`debug ast` が今と同じ）。
- **S766** 下端を含まない範囲は、利用者の設備の設定の型のフィールド（`type ScaleSpec : EquipmentSpec { maxLoad: Mass in 0 g<.. }`）にも効き、台所の `maxLoad: 0 g` は `TYP012`。
- **S767** 数だけの下端（`Count in 0<..`）も同じ判定（`0` は範囲外、`1` は範囲内）。
- **S768** 空の範囲 `1<..1` には入る値が無く、`1` は `TYP012`（範囲そのものの誤りの診断は足さない）。
- **S716** `ozen check --format json --emit types` の型のフィールドで、下端を含まない範囲には `"minExclusive":true` が付き、`min` は下端の文字（`0 g`）になる。下端を含む範囲と範囲の無いフィールドには `minExclusive` のキーが無い。
- **S717** `schemas/result.schema.json` は、型のフィールドの `minExclusive` を任意の真偽値として許す。
- **S769** `--emit signatures` の標準の動作の引数の型の文字は、範囲ごと（`Time in 0 s<..`）出る。
- **S718** 文法の規則 `type_range` が `core/grammar.ebnf` と `core/src/rules.def` にあり、golden で通る（`G-CORE-GRAMMAR`）。
- **S719** ファジングの辞書（`core/fuzz/ozen.dict`）に `"<.."` がある。

### 4.5 標準の動作の範囲（D70）

- **S720** `soak`・`fry`・`simmer`・`boil`・`parboil`・`chill`・`rest` の `time` の型は `Time in 0 s<..`（`soak` の既定値 `30 min` はそのまま）。
- **S721** `cut` の `size` の型は `Length in 0 mm<..`（既定値 `3 mm` はそのまま）。
- **S722** 0 の時間（`simmer(x, 0 min)`）は `TYP012`。
- **S723** 負の時間（`boil(water, -300 s)`）は `TYP012`。
- **S770** 両端に単位を書いた負の時間（`simmer(x, -5 min..-1 min)`）は `TYP012`。
- **S724** 0 と負の大きさ（`cut(slice, 0 mm)`、`cut(slice, -3 mm)`）は `TYP012`。
- **S725** 正の小さい値（`fry(egg, 1 s)`、`cut(slice, 0.5 mm)`）と既定値（`soak(rice)`、`cut(slice)`）は診断を出さない。
- **S726** 温度の引数には範囲を付けない。利用者の動作 `freeze(x: T, temp: Temperature)` に `-18 degC` を渡しても診断は無い。標準ライブラリに温度の範囲（`Temperature in`・`Temperature? in`）は無い。
- **S727** 次元の違う値（`fry(egg, -3 ml)`）は `TYP001` だけを出し、`TYP012` を重ねない。
- **S771** 合成の動作 `quick(x: T, t: Time) { steps { fry(x, t) } }`（外の `t` に範囲が無い）を `quick(egg, 0 s)` と呼ぶと、中の `fry` の範囲で `TYP012` が 1 つだけ、呼び出し元の `0 s` の位置に出る。

### 4.6 材料の量（D71・D75・D76）

- **S728** 材料の量の下端が 0 以下なら `TYP021 nonPositiveAmount`（エラー）を出す。引数は `[材料の名前, 書いた量の文字]`、位置は量を書いた場所。
- **S729** 0 ちょうど（`egg: Egg 0 pcs`）もエラー。
- **S730** 負の量（`water: -100 ml`）はエラー。
- **S731** 範囲の量は下端で判定する。`0..2 g`、`-1..2 g`（`-(1..2 g)` と読まれ、下端は -2）、`-0.5..0.8 g`（`-(0.5..0.8 g)` と読まれ、下端は -0.8）はエラー、`0.5..2 g` は合格。
- **S772** 両端に単位を書いた範囲の量（`sugar: -5 ml..-1 ml`）もエラー（D85 で評価されるようになる）。
- **S732** `per serving` の量は書いた値で判定する（人数 2 の料理の `0 g per serving` はエラー、`100 g per serving` は合格）。
- **S733** 人数が決まらない `per serving` の量（`TYP020`）でも、書いた値が 0 以下なら `TYP021` も出す。
- **S773** 人数が範囲外の料理（`servings: 0`）では、`100 g per serving` と `150 ml * servings` に `TYP020`・`TYP021` を出さない。
- **S785** 人数が範囲外の料理でも、`0 g per serving` には `TYP021` を出す（書いた値の誤りで、人数の誤りと別の原因）。
- **S734** `[any amount]` は対象外（`TYP003` の警告だけ）。
- **S735** 量を書かない材料は対象外。
- **S736** 量の次元が誤っているとき（`-1 min`）は `TYP001` だけを出し、`TYP021` を重ねない。
- **S737** 式の量（`100 g - 100 g`）は評価した値で判定する。
- **S738** 正の量（`0.5 g`、`1 pcs`、`0.5..2 g`）は診断を出さない。
- **S739** `TYP021` は登録され（`diag_codes.def` の `SCH011` より後、`diag-names.txt`）、日本語と英語の言い回し（`diag-non-positive-amount`。どちらも `{ $arg1 }` と `{ $arg2 }` を使う）と、引数の種類（`names.mjs` の `['name']`）を持つ。
- **S740** `docs/concept.md` §6.1 のエラーの例と `core/tests/severity-examples.txt` に「0 以下の量」がある。

### 4.7 人数（D72・D77）

- **S741** `Options.servings` の型は `Count? in 1..`。`--emit types` の `Options` の `servings` は `min` が `"1"` になる。
- **S774** 設定画面（`renderers/lib/settings.mjs` の `settingsSpec`）は、`--emit types` の `Options.servings` から、人数の入力に `min` 1 を付ける。
- **S742** 省略可能で範囲のある型（`Count? in 1..`）を `check_value` が検査する。利用者の設定の型の `n: Count? in 1..` に `0` を渡すと `TYP012`。
- **S743** レシピの見出しの型の無い `servings: 0` は `TYP012`（引数 `["servings", "0", "1.."]`）。
- **S744** 型だけの `servings: Count = 0` も `TYP012`（同じ引数）。
- **S745** 自分で範囲を宣言した `servings: Count in 1..20 = 0` は、宣言の範囲で `TYP012` を 1 つだけ出す（引数の 3 つ目は `1..20`）。
- **S746** 献立の `make soup(servings: 0)` は、`0` の位置に `TYP012` を 1 つ。
- **S775** 位置で渡した `make soup(0)`（料理の最初の引数が `servings`）も `TYP012` を 1 つ。
- **S776** 献立の見出しの `menu dinner(servings: 0)` も `TYP012`。
- **S747** `servings` を持たない料理に `ozen check -s 0` と `-s -1` を渡すと、条件の側で `TYP012`（範囲の文字 `1..`）が 1 つ出る。
- **S777** 見出しに `servings: Count in 1..20 = 4` がある料理に `-s 0` を渡すと、`TYP012` は 1 つだけ（条件の側の `1..`）。
- **S778** 同じ料理に `-s 25` を渡すと、`TYP012` は 1 つだけ（見出しの側の `1..20`）。
- **S779** 同じ料理に `-s 3ml` を渡すと、`TYP001` は 1 つだけ（今は 2 行）。
- **S748** `servings` を持たない料理で、ビルド時の条件 `kitchen today : Options { servings: 0 }` を `--options today` で選ぶと、`TYP012` が 1 つ出る。
- **S749** 見出しの人数 1（`servings: 1`）は診断を出さない。
- **S780** `-s 1` は診断を出さない。

### 4.8 Issue の再現

- **S750** Issue #2 の出力から組み立てた再現（`water: -100 ml`、`boil(water, -300 s)`）は、`TYP021` と `TYP012` を 1 つずつ出し、`ozen check` の終了コードは 1 になる。

### 4.9 不変条件

- **S751** 本計画の範囲で期待を直した既存の golden・描画のテストは、すべて §9 に理由付きで名指しされている（足した golden は数えない）。
- **S752** 既存のサンプル（`samples/`）はすべて今までどおり通る（`G-SAMPLE-CHECK`）。
- **S753** 本計画のコミットが触るのは §4.11 の一覧のファイルだけ（§9 で除いた別の作業のコミットは数えない）。
- **S754** 版（`VERSION`）は D83 のとおり。
- **S755** `G-RF-GOLDEN-COUNT` は D80 の形に直り、本計画の golden を足した後も合格する。
- **S781** `G-RD-SCOPE`・`G-RD-ADD-ONLY` は D87 の形（`readme-start..readme-end`）に直り、合格する。
- **S782** 期待 `.out` を直すコミットと、実装を変えるコミットは分かれている（`G-RF-SEPARATION`）。

### 4.10 文書

- **S756** `docs/concept.md` §4.2 の範囲制約の説明に、下端を含まない書き方（`a<..`・`a<..b`）と例（`0 s<..`）がある。
- **S783** `docs/concept.md` §4.2 の「範囲外は必ずエラー」の対象に、動作の引数がある。
- **S784** `docs/concept.md` の `type Options` の例の `servings` が `Count? in 1..` になっている。
- **S757** `CLAUDE.md` の Plan の節に、本計画のファイル名・`G-QC-*`・タグ `qc-start` の行がある。

### 4.11 足すもの・変えるものの一覧

| 種類 | ID・パス |
| --- | --- |
| 診断 | `TYP021 nonPositiveAmount`（D75） |
| 文法 | 規則 `type_range`（型の範囲の下端を含まない形 `a<..`・`a<..b`）。`core/grammar.ebnf`、`core/src/rules.def`、`core/src/parse.c`、`core/src/ast.h` |
| 標準ライブラリ | `stdlib/prelude.ozen` の 8 つの引数（§3.3）と `Options.servings` |
| 言い回し | `phrases/en.ftl`・`phrases/ja.ftl` の `diag-non-positive-amount` |
| 描画 | `renderers/lib/names.mjs` の `ARG_KINDS` に `nonPositiveAmount`、`renderers/test/settings-servings-min.test.mjs`（S774） |
| スキーマ | `schemas/result.schema.json` の型のフィールドの `minExclusive` |
| ファジング | `core/fuzz/ozen.dict` の `"<.."` |
| コア | `core/src/sema.c`（量の照合、範囲の評価、範囲の開いた下端、材料の量、人数、`make` の引数）、`core/src/defs.c`（`minExclusive`） |
| テスト | `tests/golden/args/`（21 個）、`tests/golden/bounds/`（51 個）、期待を直す既存の golden（S751） |
| 文書 | `docs/concept.md` §4.2・§6.1・`type Options` の例、`core/tests/diag-names.txt`、`core/tests/severity-examples.txt` |
| 計画とゴール | 本ファイル、`scripts/goals.sh`、`CLAUDE.md`、`plans/ozen-syntax-refresh.md` §9 の 1 行（D80）、`plans/ozen-readme-compact.md` §9 の 1 行（D87）、`scripts/red-first-exceptions.txt`（対照の golden のコミット、§5.2） |

### 4.12 コードの配置

| 場所 | 置くもの |
| --- | --- |
| `core/src/sema.c` | `check_value` の次元と範囲の判定を、評価済みの量（`ozni_qty`）と型の節を受け取る関数に切り出す（名前は実装で決める）。`check_value` はそれを呼ぶ。`call_action` は引数を割り当てた後、既定値でない量の引数ごとにそれを呼ぶ。合成の動作の中の呼び出しの照合は、外で照合済みの値を重ねず、位置を呼び出し元に向ける（S759・S771）。`eval_inner` で両端に単位を書いた範囲を評価する。`base_type` は範囲と省略可能の入れ子のどちらの順でも外す。範囲の下端を含まない判定。`eval_ingredients` で `TYP021`。`eval_make`・`eval_param` で `make` の引数と人数を照合し、条件の人数の重複を除く |
| `core/src/parse.c` | `type()` の `in` の後だけで `<..` を読む（`range()` は式と共用なので変えない） |
| `core/src/ast.h` | 下端を含まない範囲を表す節の種類か印（実装で決め、§9 に書く） |
| `core/src/defs.c` | `put_field` の `minExclusive` |

---

## 5. 機械検証ゴールの定義

**前提ゴール**（各タスクの完了条件に暗黙に含む）:

- 記法の計画 §5 の「手元で回すもの」すべて。とくに `G-CORE-GOLDEN`・`G-CORE-GRAMMAR`・`G-CORE-DIAG-APPEND`・`G-CORE-DIAG-REGISTRY`・`G-TYPE-SEVERITY-TABLE`・`G-STD-PHRASES`・`G-REND-PHRASES-RENDER`・`G-REND-TESTS`・`G-API-SCHEMA`・`G-API-SARIF`・`G-SAMPLE-CHECK`・`G-CORE-FUZZ`・`G-REPO-RED-FIRST`・`G-PLAT-WASM` は本計画で触るファイルに関わる
- 記法の計画の `G-RF-GOLDEN-COUNT`（D80 で直した形）・`G-RF-FUZZ-DICT`・`G-RF-SEPARATION`・`G-RF-CONCEPT`
- README の計画の `G-RD-SCOPE`・`G-RD-ADD-ONLY`（D87 で直した形）
- push の前に毎回: `G-REPO-NOREPLY`
- push の後: `G-PLAT-CI`

失敗したときは、どのゴールのどの検査が落ちたかを報告する。

- `QC=qc-start..HEAD`、`PLAN_QC=plans/ozen-quantity-checks.md`、`OZ=build/core-gcc-release/ozen`（`OZ` は既に `goals.sh` にある）。
- 0 を期待する数え上げは、終了コードでなく出力の数で判定する。範囲を使う数え上げは、先にタグがあることを確かめる（無いと `git diff` が何も出さずに合格するため）。
- P0-3 で、下のコードブロックを `scripts/goals.sh` の関数 `goal_G_QC_*` にする。

### 5.1 構造・データ

| ID | 何を判定するか | 合格条件 |
| --- | --- | --- |
| `G-QC-DECISIONS` | D69〜D87 が確定しているか | 決定済み 19、未決 0、決定の行 19 |
| `G-QC-COVERAGE` | S 項目すべてが網羅表に行を持ち、§8 の各項目に理由がある | 差分が空、理由の無い項目 0 |
| `G-QC-PLAN-NAMES` | 網羅表のテスト名が実在する | `plan-tests: <n> names, 0 missing` |
| `G-QC-PLAN-TESTS` | わざと壊す確認の記録が網羅表の行の数だけある | `mutations: <m> rows, <m> recorded` |
| `G-QC-GOLDEN-ARGS` | §4.1〜§4.3 の golden | 21 個、`golden: 21 passed, 0 failed, 0 timed out` |
| `G-QC-GOLDEN-BOUNDS` | §4.4〜§4.8 の golden | 51 個、`golden: 51 passed, 0 failed, 0 timed out` |
| `G-QC-GOLDEN-CHANGED` | S751 | `ok`、名指しされていない直した期待 0 |
| `G-QC-PRELUDE` | S720・S721・S726・S741 | `time` の行 7、`size` の行 1、温度の範囲 0、`servings` の行 1 |
| `G-QC-DIAG` | S739・S740 | 7 つの数がすべて 1、`TYP021` の行が `SCH011` の行より後 |
| `G-QC-SCHEMA` | S716・S717 | `minExclusive` の数が `min` の数と同じ、`json: 1 files, 0 invalid` |
| `G-QC-SYNTAX` | S718・S719・S756・S783・S784・S740 | 7 つの数がすべて 1 以上 |
| `G-QC-SCOPE` | S753 | `ok`、範囲に無い除外のコミット 0、範囲の外のファイル 0 |
| `G-QC-VERSION` | S754 | D83 の値 |
| `G-QC-CLAUDEMD` | S757 | 1 以上 |
| `G-QC-RF-COUNT` | S755 | `G-RF-GOLDEN-COUNT` の関数に `readme-start` があり、関数が合格する |
| `G-QC-RD-RANGE` | S781 | `readme-end` が `52a6ef1`、`goals.sh` の `RD=` が `readme-start..readme-end`、2 つのゴールが合格 |
| `G-QC-ISSUES` | D84（push の後、ユーザーの確認の後） | 2 つの Issue が `CLOSED`、コメントに診断のコードがある |
| `G-QC-AUDIT` | P4-2 の監査の記録 | 1 以上 |

```bash
# G-QC-DECISIONS
grep -c '^### D[0-9]*\. .*【決定済み】' $PLAN_QC       # → 19
grep -c '^### D[0-9]*\. .*【未決】' $PLAN_QC           # → 0
grep -c '^- \*\*決定:' $PLAN_QC                         # → 19

# G-QC-COVERAGE（出力が空と 0）
diff <(sed -n '/^## 4\./,/^## 5\./p' $PLAN_QC | grep -oE '\*\*S[0-9]+\*\*' | tr -d '*' | sort -u) \
     <(sed -n '/^### 5\.3/,/^## 6\./p' $PLAN_QC | grep -oE '^\| S[0-9]+' | tr -d '| ' | sort -u)
sed -n '/^## 8\./,/^## 9\./p' $PLAN_QC | grep '^- ' | grep -vc '機械で決められない理由:'

# G-QC-PLAN-NAMES
scripts/check-plan-tests.sh $PLAN_QC | tail -n 1                  # → plan-tests: <n> names, 0 missing

# G-QC-PLAN-TESTS
scripts/check-plan-tests.sh --mutations $PLAN_QC | tail -n 1      # → mutations: <m> rows, <m> recorded

# G-QC-GOLDEN-ARGS
scripts/golden-entries.sh tests/golden/args | wc -l               # → 21
scripts/run-golden.sh $OZ tests/golden/args | tail -n 1           # → golden: 21 passed, 0 failed, 0 timed out

# G-QC-GOLDEN-BOUNDS
scripts/golden-entries.sh tests/golden/bounds | wc -l             # → 51
scripts/run-golden.sh $OZ tests/golden/bounds | tail -n 1         # → golden: 51 passed, 0 failed, 0 timed out

# G-QC-GOLDEN-CHANGED（範囲で直した（足したのではない）期待のうち、§9 に名指しされていないもの）
git rev-parse -q --verify refs/tags/qc-start >/dev/null && echo ok           # → ok
git diff --name-only --diff-filter=M qc-start HEAD -- 'tests/golden/**/*.out' 'renderers/test/**' 'samples/*/expect.yaml' \
  | while IFS= read -r f; do grep -qF -- "- 期待を直した golden: \`$f\`" $PLAN_QC || echo "$f"; done | wc -l   # → 0

# G-QC-PRELUDE
grep -cE '^action (soak|fry|simmer|boil|parboil|chill|rest)\(.*\btime: Time in 0 s<\.\.' stdlib/prelude.ozen   # → 7
grep -cE '^action cut\(.*\bsize: Length in 0 mm<\.\. = 3 mm\)' stdlib/prelude.ozen                              # → 1
grep -cE 'Temperature\??[[:space:]]+in\b' stdlib/prelude.ozen                                                  # → 0
grep -cE '^  servings: Count\? in 1\.\.$' stdlib/prelude.ozen                                                   # → 1

# G-QC-DIAG
grep -cx 'OZNI_DIAG(TYP021, TYP, nonPositiveAmount, ERROR)' core/src/diag_codes.def                  # → 1
[ "$(grep -n 'TYP021' core/src/diag_codes.def | cut -d: -f1)" -gt "$(grep -n 'SCH011' core/src/diag_codes.def | cut -d: -f1)" ] && echo after   # → after
grep -cxF 'TYP:nonPositiveAmount' core/tests/diag-names.txt                                          # → 1
grep -E '^diag-non-positive-amount = ' phrases/en.ftl | grep -F '{ $arg1 }' | grep -cF '{ $arg2 }'   # → 1
grep -E '^diag-non-positive-amount = ' phrases/ja.ftl | grep -F '{ $arg1 }' | grep -cF '{ $arg2 }'   # → 1
grep -cE "^  nonPositiveAmount: \['name'\]," renderers/lib/names.mjs                                  # → 1
grep -cP '\tTYP:nonPositiveAmount\terror\t' core/tests/severity-examples.txt                         # → 1

# G-QC-SCHEMA
grep -c '"minExclusive"' schemas/result.schema.json    # → grep -c '"min": {' schemas/result.schema.json と同じ数
mkdir -p build/results/qc-types && rm -f build/results/qc-types/*.json
$OZ check --format json --emit types tests/golden/bounds/types-json-exclusive.ozen > build/results/qc-types/exclusive.result.json
python3 -B tools/jsoncheck/validate.py schemas build/results/qc-types | tail -n 1   # → json: 1 files, 0 invalid

# G-QC-SYNTAX
grep -cE '^type_range[[:space:]]*:=' core/grammar.ebnf                                          # → 1
grep -cx 'OZNI_RULE(type_range)' core/src/rules.def                                              # → 1
grep -cxF '"<.."' core/fuzz/ozen.dict                                                            # → 1
sed -n '/^### 4\.2/,/^### 4\.3/p' docs/concept.md | grep -cF '0 s<..'                           # → 1 以上
sed -n '/^### 4\.2/,/^### 4\.3/p' docs/concept.md | grep -F '範囲外は必ずエラー' | grep -cF '動作の引数'   # → 1 以上
grep -cE '^  servings: +Count\? in 1\.\.' docs/concept.md                                        # → 1 以上
sed -n '/^### 6\.1/,/^## 7\./p' docs/concept.md | grep -cF '0 以下の量'                          # → 1 以上

# G-QC-SCOPE（README の計画の goal_G_RD_SCOPE と同じ作り。パターンは bash の case で照合する）
git rev-parse -q --verify refs/tags/qc-start >/dev/null && echo ok           # → ok
# §9 の「範囲に入った別の作業のコミット: <SHA>」の SHA のうち、範囲に無いもの → 0
# 除いたコミットを除く範囲のコミットが触ったファイルのうち、次に当たらないもの → 0
#   core/src/*.c core/src/*.h core/src/*.def core/grammar.ebnf core/fuzz/ozen.dict core/tests/diag-names.txt
#   core/tests/severity-examples.txt stdlib/prelude.ozen phrases/en.ftl phrases/ja.ftl renderers/lib/names.mjs
#   renderers/test/* schemas/result.schema.json tests/golden/* docs/concept.md scripts/goals.sh CLAUDE.md
#   scripts/red-first-exceptions.txt plans/ozen-quantity-checks.md plans/ozen-syntax-refresh.md plans/ozen-readme-compact.md

# G-QC-VERSION
cat VERSION                                            # → 0.1.0（D83）

# G-QC-CLAUDEMD
grep -c 'ozen-quantity-checks.md.*G-QC-.*qc-start' CLAUDE.md                 # → 1 以上

# G-QC-RF-COUNT
sed -n '/^goal_G_RF_GOLDEN_COUNT()/,/^}/p' scripts/goals.sh | grep -c 'readme-start'   # → 1 以上
scripts/goals.sh G-RF-GOLDEN-COUNT                                                     # → 合格

# G-QC-RD-RANGE
git rev-parse --short=7 readme-end                                           # → 52a6ef1
grep -cx 'RD=readme-start..readme-end' scripts/goals.sh                       # → 1
scripts/goals.sh G-RD-SCOPE G-RD-ADD-ONLY                                     # → どちらも合格

# G-QC-ISSUES（push の後）
gh issue view 1 --repo atsuki-seo/Ozen --json state -q .state                 # → CLOSED
gh issue view 2 --repo atsuki-seo/Ozen --json state -q .state                 # → CLOSED
gh issue view 1 --repo atsuki-seo/Ozen --json comments -q '.comments[].body' | grep -c 'TYP001'   # → 1 以上
gh issue view 2 --repo atsuki-seo/Ozen --json comments -q '.comments[].body' | grep -c 'TYP021'   # → 1 以上

# G-QC-AUDIT（P4-2）
grep -cE '^- 網羅の監査: 指摘 [0-9]+ 件、取り込み [0-9]+ 件' $PLAN_QC        # → 1 以上
```

`G-QC-SCOPE` は、本ファイルを着手点のタグより後にコミットする場合に備えて本ファイルを許す。`plans/ozen-syntax-refresh.md`・`plans/ozen-readme-compact.md` は D80・D87 の 1 行ずつのためだけに許す。`G-QC-GOLDEN-ARGS`・`G-QC-GOLDEN-BOUNDS` の数の完全一致は、本計画のフォルダ（`args/`・`bounds/`）だけを数えるので、後の計画が別のフォルダに golden を足しても落ちない。`G-QC-DIAG` は、後の計画が診断を足しても落ちないよう、末尾の行ではなく `SCH011` より後にあることを見る。

### 5.2 テスト

新しい golden は 72 個（`tests/golden/args/` に 21 個、`tests/golden/bounds/` に 51 個）と、描画のテスト 1 つ（`renderers/test/settings-servings-min.test.mjs`）。golden の数は着手点の N0（§9。2026-11-02 は 448）から 72 増える。

- 期待（`.out`）は §4 の仕様と、既存の golden の診断の書式（`kitchen/range-open-ended.out` の `TYP012`、`kitchen/unitless-capacity.out` の `TYP001` の引数の形）から手で書く。実装（`ozen`）の出力を写さない。位置（行・桁）はソースから数える。
- 例外は `bounds/expression-unchanged`（S715）だけで、`debug ast` の期待は着手点のビルド（`qc-start` の `ozen`）の出力から作る。読み方が変わらないことを確かめるテストなので、変える前の実装がオラクルになる。§9 にそう書く。
- `settings-servings-min.test.mjs`（S774）は、golden `api/types-json` の期待（手で直した `Options.servings` の `min` `"1"`）を入力に `settingsSpec` を呼び、人数の入力の `min` が 1 であることを確かめる。
- 期待を足すコミットは実装より前に置き、そのコミットの時点のビルドで期待が落ちること（赤。`G-REPO-RED-FIRST`）を確かめる。診断が出ないことを確かめる対照の golden（`args/ok-units` など）と、前の Phase の実装や今の実装で既に通る golden（`bounds/dimension-before-range`、`bounds/servings-own-range` など）は、足す時点で合格してしまう。記法の計画の前例（`ingredient-dimension-ok`）に倣い、赤の golden と別のコミットにし、`scripts/red-first-exceptions.txt` にそのコミットと理由（どのわざと壊す確認で落ちるか）を書く。
- 既存の golden の期待が変わったときは、変わった理由が本計画の仕様（診断の一覧が 1 つ増えた、`Options.servings` の型の文字が変わった、`std` の動作の引数の型の文字が変わった、範囲の量が評価されるようになった、など）で説明できるものだけを直し、§9 に `- 期待を直した golden: \`<パス>\` — <理由>` の形で 1 行ずつ書く（S751）。期待を直すコミットは、実装を変えるコミットと分ける（`G-RF-SEPARATION`、S782）。説明できない変化は直さずに調べる。
- 落ちた golden は緩めずに原因を調べる。期待と実装が食い違ったら、§4 と照らしてどちらの誤りかを決め、§4 の誤りならユーザーに報告する。

| テスト | 何を判定するか | 主なタスク |
| --- | --- | --- |
| `cgolden/args/*`（21 個） | §4.1〜§4.3 の各 S 項目。名前は §5.3 | P1-1 |
| `cgolden/bounds/*`（51 個） | §4.4〜§4.8 の各 S 項目。名前は §5.3 | P2-1・P3-1 |
| `rtest/settings-servings-min` | S774 | P3-1 |

機械で押さえるのは、診断の有無・数・コード・位置・引数・終了コード（golden）、文法の規則と辞書と文書の語（grep）、型の一覧の JSON とスキーマ（検証の道具）、設定画面の部品（描画のテスト）、標準ライブラリの宣言（grep）、変えたファイルの範囲、Issue の状態とコメントの中身（`gh`）。診断の文と説明の文が読み手に伝わるか、Issue への返答の言い方は §8 に回す。

### 5.3 網羅表

観点のうち「時間と順序」「永続化」は、コンパイル時の検査の変更なので該当しない（段取りの時間の計算は変えない）。「プラットフォームの経路」は、コアが C の単一の実装で、Linux・WebAssembly・iOS・Android の結線を変えないので、前提ゴールの `G-PLAT-LINUX`・`G-PLAT-WASM`（golden を wasm でも回す）で足りるとして行を立てない。「実際の経路」は、`ozen check` のコマンドを通す golden（すべて）と、パイプ・名前付き引数・マクロ・合成の動作・`make`・献立の見出し・`-s`・`--options` の golden で満たす。

| S 項目 | 観点 | テストまたはゴール | わざと壊す方法 | 主なタスク |
| --- | --- | --- | --- | --- |
| S695 | 正常系 | `cgolden/args/volume-for-time` | `call_action` の照合の呼び出しを消す | P1-2 |
| S695 | 正常系 | `cgolden/args/mass-for-length` | 照合を `Time` の宣言にだけ効くように狭める | P1-2 |
| S696 | 境界・上限 | `cgolden/args/unitless-time` | 次元の無い量を照合の前に読み飛ばす | P1-2 |
| S697 | 効かない条件 | `cgolden/args/count-param` | 照合で `pcs` を次元の無い量に直す処理を外す | P1-2 |
| S698 | 実際の経路 | `cgolden/args/named-arg` | 照合を位置で渡した引数だけにする | P1-2 |
| S699 | 実際の経路 | `cgolden/args/pipe-arg` | `pipe_into` で左辺を最後の引数に付ける（`3 ml` が `x` に当たり、診断の位置と引数が変わる） | P1-2 |
| S700 | 正常系 | `cgolden/args/variable-arg` | 診断の位置を呼び出し全体にする | P1-2 |
| S701 | 実際の経路 | `cgolden/args/macro-body` | 照合の診断の前に `arg_site` の trail を設定しない（本文の位置の trail が消える） | P1-2 |
| S758 | 実際の経路 | `cgolden/args/macro-splice` | 照合の診断の位置を `arg_site` を通さず引数の節そのもの（マクロの本文の `$t`）にする | P1-2 |
| S702 | 実際の経路 | `cgolden/args/composite` | 合成の動作のときは外の照合を飛ばす | P1-2 |
| S759 | 副作用とコスト | `cgolden/args/composite` | 合成の動作の中の呼び出しでも照合の診断を出す（2 つになる） | P1-2 |
| S703 | 正常系 | `cgolden/args/user-dimension` | 照合を標準の 6 つの次元の名前に限る | P1-2 |
| S704 | 効かない条件 | `cgolden/args/unknown-value` | 未知の量も照合し、次元の無い量として扱う | P1-2 |
| S705 | 効かない条件 | `cgolden/args/default-unchecked` | 既定値を評価した後にも照合を呼ぶ（`time: Time = 3 ml` で落ちる） | P1-2 |
| S706 | 副作用とコスト | `cgolden/args/continues-after` | 次元の誤りで `call_action` から未知の値を返す | P1-2 |
| S707 | 効かない条件 | `cgolden/args/ok-units` | 区間の値（`3..5 min`）をエラーにする | P1-2 |
| S707 | 効かない条件 | `cgolden/args/ok-units` | 列挙の引数（`heat: high`）も照合し、量でない値を次元の無い量として扱う | P1-2 |
| S708 | 実際の経路 | `cgolden/args/issue1` | 照合の診断の重大度を警告にする（終了コードが 0 になる） | P1-2 |
| S760 | 正常系 | `cgolden/args/range-units-steps` | `eval_inner` の範囲の評価を消す（`?` に戻る） | P1-2 |
| S761 | 実際の経路 | `cgolden/args/range-units-dimension` | 範囲の評価で、結果の次元を記録しない（次元の無い区間にする。`["Dimensionless", "Time"]` になる） | P1-2 |
| S762 | 境界・上限 | `cgolden/args/range-units-mixed` | 両端の次元を比べず、左の次元を使う | P1-2 |
| S763 | データの完全性 | `cgolden/args/range-units-ingredient` | 材料の量の評価で範囲の節を未知にする | P1-2 |
| S764 | 実際の経路 | `cgolden/args/make-arg-dimension` | `make` の量の引数の照合を消す | P1-2 |
| S765 | 実際の経路 | `cgolden/bounds/make-arg-range` | `make` の照合で範囲の判定を飛ばす | P2-2 |
| S709 | 境界・上限 | `cgolden/bounds/exclusive-from` | 下端を含まない判定を `<` から `<=` にする（下端ちょうどが通る） | P2-2 |
| S710 | 境界・上限 | `cgolden/bounds/exclusive-upper-inclusive` | 上端の判定を `>=` にする（上端ちょうどが落ちる） | P2-2 |
| S710 | 境界・上限 | `cgolden/bounds/exclusive-upper-over` | 上端のある `a<..b` で上端を読み捨てる | P2-2 |
| S711 | 効かない条件 | `cgolden/bounds/exclusive-space` | `<` と `..` の間の空白を許す | P2-2 |
| S712 | 正常系 | `cgolden/bounds/exclusive-from` | 診断の 3 つ目の引数を下端の文字だけにする | P2-2 |
| S713 | 境界・上限 | `cgolden/bounds/exclusive-interval` | 区間の値の判定に上端（`hi`）を使う | P2-2 |
| S714 | 効かない条件 | `cgolden/bounds/closed-zero-ok` | 閉じた範囲も下端を含まない判定にする | P2-2 |
| S715 | 効かない条件 | `cgolden/bounds/expression-unchanged` | `range()`（式と共用）で `<..` を読む（`debug ast` の木が変わる） | P2-2 |
| S766 | 実際の経路 | `cgolden/bounds/exclusive-kitchen` | 台所の設定のフィールドでは下端を含まない印を見ない | P2-2 |
| S767 | 境界・上限 | `cgolden/bounds/exclusive-count` | 数だけの下端（`0<..`）で印を読み捨てる | P2-2 |
| S768 | 境界・上限 | `cgolden/bounds/exclusive-empty` | 下端と上端が等しい範囲では下端の判定を閉区間にする | P2-2 |
| S716 | データの完全性 | `cgolden/bounds/types-json-exclusive` | `minExclusive` を出さない | P2-2 |
| S716 | 効かない条件 | `cgolden/api/types-json` | 下端を含む範囲にも `"minExclusive":false` を出す | P2-2 |
| S717 | データの完全性 | `G-QC-SCHEMA` | スキーマの型のフィールドから `minExclusive` を消す（`additionalProperties: false` で検証が落ちる） | P2-2 |
| S769 | データの完全性 | `cgolden/api/signatures-json` | 動作の引数の型の文字を、範囲を除いた型の名前だけにする | P2-3 |
| S718 | データの完全性 | `G-CORE-GRAMMAR` | `type()` の `mark(p, OZNI_R_type_range)` を消す（規則が golden で通らない） | P2-2 |
| S718 | 文書との一致 | `G-QC-SYNTAX` | `core/grammar.ebnf` から `type_range` の規則を消す | P2-2 |
| S719 | データの完全性 | `G-QC-SYNTAX` | `core/fuzz/ozen.dict` から `"<.."` を消す | P2-2 |
| S720 | データの完全性 | `G-QC-PRELUDE` | `chill` の `time` の範囲を消す | P2-3 |
| S721 | データの完全性 | `G-QC-PRELUDE` | `cut` の `size` の範囲を `0 mm..` にする | P2-3 |
| S722 | 境界・上限 | `cgolden/bounds/time-zero` | `simmer` の範囲を `0 s..` にする | P2-3 |
| S723 | 正常系 | `cgolden/bounds/time-negative` | `boil` の範囲を消す | P2-3 |
| S770 | 実際の経路 | `cgolden/bounds/range-units-negative-time` | `simmer` の範囲を消す | P2-3 |
| S724 | 境界・上限 | `cgolden/bounds/size-zero` | `cut` の範囲を `0 mm..` にする | P2-3 |
| S724 | 正常系 | `cgolden/bounds/size-negative` | `cut` の範囲を消す | P2-3 |
| S725 | 効かない条件 | `cgolden/bounds/small-positive` | 範囲を `1 min<..` にする | P2-3 |
| S725 | 効かない条件 | `cgolden/bounds/soak-default` | 既定値を使った呼び出しも照合する形にしたうえで、`soak` の範囲を `30 min<..` にする（既定値の 30 min が範囲外になる） | P2-3 |
| S726 | 効かない条件 | `cgolden/bounds/temperature-negative` | 次元の型の引数すべてに `0<..` の判定を当てる | P2-3 |
| S726 | データの完全性 | `G-QC-PRELUDE` | `OvenSpec.maxTemp` を `Temperature? in 0 degC..` にする | P2-3 |
| S727 | 副作用とコスト | `cgolden/bounds/dimension-before-range` | 次元が違っても範囲の判定を続ける | P2-3 |
| S771 | 副作用とコスト | `cgolden/bounds/composite-range` | 合成の動作の中の照合の位置を中の呼び出しのまま出す（位置が定義の中になる） | P2-3 |
| S728 | 正常系 | `cgolden/bounds/amount-negative` | `TYP021` を出す処理を消す | P3-2 |
| S729 | 境界・上限 | `cgolden/bounds/amount-zero` | 判定を `lo < 0` にする（0 が通る） | P3-2 |
| S730 | 正常系 | `cgolden/bounds/amount-negative` | 診断の引数の 2 つ目を材料の型の名前にする | P3-2 |
| S731 | 境界・上限 | `cgolden/bounds/amount-range-zero` | 判定に区間の上端を使う | P3-2 |
| S731 | 境界・上限 | `cgolden/bounds/amount-range-negative` | 判定を `lo < -1` にする（`-0.5..0.8 g` の下端 -0.8 が通り、`TYP021` が 1 つ減る） | P3-2 |
| S731 | 効かない条件 | `cgolden/bounds/amount-positive` | 判定を `lo <= 1` にする | P3-2 |
| S772 | 実際の経路 | `cgolden/bounds/amount-range-units` | `TYP021` の判定を量のリテラルと数の範囲の節にだけ当てる | P3-2 |
| S732 | 正常系 | `cgolden/bounds/amount-per-serving-zero` | `per serving` の量では判定を飛ばす | P3-2 |
| S733 | 副作用とコスト | `cgolden/bounds/amount-per-serving-unknown` | `TYP020` のときは判定を飛ばす | P3-2 |
| S773 | 副作用とコスト | `cgolden/bounds/servings-invalid-amounts` | 判定を人数を掛けた後に動かす（`servings: 0` で `100 g per serving` が 0 g になり `TYP021` が出る） | P3-3 |
| S785 | 効かない条件 | `cgolden/bounds/servings-invalid-amounts` | 人数が範囲外のときは `per serving` の量の判定も飛ばす（`0 g per serving` の `TYP021` が消える） | P3-3 |
| S734 | 効かない条件 | `cgolden/bounds/amount-any` | `[any amount]` を 0 として判定する | P3-2 |
| S735 | 効かない条件 | `cgolden/bounds/amount-none` | 量の無い材料を 0 として判定する | P3-2 |
| S736 | 副作用とコスト | `cgolden/bounds/amount-wrong-dim` | 次元の誤りの後にも判定を続ける | P3-2 |
| S737 | 正常系 | `cgolden/bounds/amount-expression` | 判定を量のリテラルの節にだけ当てる | P3-2 |
| S738 | 効かない条件 | `cgolden/bounds/amount-positive` | 判定を `lo <= 0.5 g` 相当にする（`0.5 g` が落ちる） | P3-2 |
| S739 | データの完全性 | `G-QC-DIAG` | `phrases/ja.ftl` の `diag-non-positive-amount` から `{ $arg2 }` を消す | P3-2 |
| S739 | データの完全性 | `G-CORE-DIAG-REGISTRY` | `core/tests/diag-names.txt` の `TYP:nonPositiveAmount` を消す | P3-2 |
| S739 | 前提の固定 | `G-CORE-DIAG-APPEND` | 一時的な worktree の作業用のコミットで `TYP021` を既存の行の間に入れ直す（削除の行が出る。`git reset --hard` で消し、push しない） | P3-2 |
| S740 | 文書との一致 | `G-TYPE-SEVERITY-TABLE` | `severity-examples.txt` の重大度を `warning` にする | P4-1 |
| S740 | 文書との一致 | `G-QC-SYNTAX` | concept §6.1 から「0 以下の量」を消す | P4-1 |
| S741 | データの完全性 | `cgolden/api/types-json` | `Options.servings` の範囲を消す | P3-3 |
| S774 | 実際の経路 | `rtest/settings-servings-min` | テストの入力の `min` を `null` に戻す（期待の 1 と合わない） | P3-3 |
| S742 | 境界・上限 | `cgolden/bounds/optional-ranged` | `base_type` の入れ子の外し方を元に戻す | P3-3 |
| S743 | 正常系 | `cgolden/bounds/servings-header-zero` | 型の無い `servings` の検査を消す | P3-3 |
| S744 | 正常系 | `cgolden/bounds/servings-typed-zero` | 型だけの `servings` では検査しない | P3-3 |
| S745 | 副作用とコスト | `cgolden/bounds/servings-own-range` | 宣言に範囲があっても `1..` を重ねる（`0` に 2 つ出る） | P3-3 |
| S746 | 実際の経路 | `cgolden/bounds/servings-make-zero` | `make` の引数の人数を検査しない | P3-3 |
| S775 | 実際の経路 | `cgolden/bounds/servings-make-positional` | `make` の照合を名前で渡した引数だけにする | P3-3 |
| S776 | 実際の経路 | `cgolden/bounds/servings-menu-header` | 人数の検査をレシピの見出しだけにする | P3-3 |
| S747 | 実際の経路 | `cgolden/bounds/servings-flag-zero` | 条件のフィールドの `check_value`（`sema.c:3163` の経路）を飛ばす | P3-3 |
| S747 | 境界・上限 | `cgolden/bounds/servings-flag-negative` | 判定を `== 0` にする | P3-3 |
| S777 | 副作用とコスト | `cgolden/bounds/servings-flag-header-range` | 条件の側で誤りでも見出しの側で検査する（2 つになる） | P3-3 |
| S778 | 実際の経路 | `cgolden/bounds/servings-flag-over-header` | 条件から来た人数では見出しの宣言の範囲を見ない | P3-3 |
| S779 | 副作用とコスト | `cgolden/bounds/servings-flag-dimension` | 重複を除く処理を消す（今と同じ 2 行に戻る） | P3-3 |
| S748 | 実際の経路 | `cgolden/bounds/servings-options-zero` | 条件のフィールドの `check_value`（`sema.c:3163` の経路）を飛ばす | P3-3 |
| S749 | 境界・上限 | `cgolden/bounds/servings-one-ok` | 範囲を `1<..` にする | P3-3 |
| S780 | 境界・上限 | `cgolden/bounds/servings-flag-one-ok` | `Options.servings` の範囲を `1<..` にする | P3-3 |
| S750 | 実際の経路 | `cgolden/bounds/issue2` | `boil` の範囲を消す | P3-3 |
| S751 | 副作用とコスト | `G-QC-GOLDEN-CHANGED` | §9 の「期待を直した golden」の行を 1 つ消す | P4-3 |
| S752 | 副作用とコスト | `G-SAMPLE-CHECK` | 一時的に `stdlib/prelude.ozen` の `rest` の範囲を `1 h<..` にする（サンプルの `rest(10 min)` が落ちる） | P4-3 |
| S753 | 副作用とコスト | `G-QC-SCOPE` | 一時的な worktree の作業用のコミットで `samples/` のファイルを 1 つ変える（`git reset --hard` で消し、push しない） | P4-3 |
| S753 | 前提の固定 | `G-QC-SCOPE` | タグ `qc-start` を手元で一時的に別名に付け替える（確かめた後に戻す） | P4-3 |
| S754 | 副作用とコスト | `G-QC-VERSION` | `VERSION` を `0.1.1` にする（確かめた後に戻す） | P4-3 |
| S755 | 前提の固定 | `G-QC-RF-COUNT` | `goal_G_RF_GOLDEN_COUNT` を今の形（今の数と比べる）に戻す（`readme-start` の行が無くなる） | P0-3 |
| S755 | 副作用とコスト | `G-RF-GOLDEN-COUNT` | 一時的な worktree で `readme-start` にある golden を 1 つ消す（消えた golden が 1 になる） | P0-3 |
| S781 | 前提の固定 | `G-QC-RD-RANGE` | `goals.sh` の `RD=` を `readme-start..HEAD` に戻す（`G-RD-SCOPE` が `got '2'` で落ちる） | P0-3 |
| S782 | 副作用とコスト | `G-RF-SEPARATION` | 一時的な worktree の作業用のコミットで、期待の `.out` と `core/src` を同じコミットにする（`git reset --hard` で消し、push しない） | P4-3 |
| S756 | 文書との一致 | `G-QC-SYNTAX` | concept §4.2 から `0 s<..` の例を消す | P4-1 |
| S783 | 文書との一致 | `G-QC-SYNTAX` | concept §4.2 の「範囲外は必ずエラー」の文から動作の引数を消す | P4-1 |
| S784 | 文書との一致 | `G-QC-SYNTAX` | concept の `type Options` の例の `servings` を `Count` に戻す | P4-1 |
| S757 | 正常系 | `G-QC-CLAUDEMD` | `CLAUDE.md` の本計画の行から `qc-start` を消す | P0-3 |

S704 の golden（`args/unknown-value`）は、評価の時点で決まらない量を引数に渡す書き方が要る。既存の golden `eval/runtime-value-in-if` の書き方（実行時まで決まらない条件）を使う見込みで、P1-1 で書けることを確かめる。書けなければ §9 に理由を書き、ユーザーに報告する（テストを §8 に回さない）。

網羅の検査（P0-3 の後と P4-2 で実行する。`G-QC-COVERAGE`）。

独立した監査（サブエージェント、2026-11-02、計画を書いた後）の指摘のうち、取り込まなかったもの・一部だけ取り込んだものと理由:

- `G-QC-PLAN-TESTS` が S 項目ごとの記録の対応を見ない（記録の行の総数だけを比べる） — 既存の計画（記法の計画・README の計画）と同じ検査で、記録の行に S 項目とゴールを書く形（§9）で人が辿れる。網羅表の行ごとに対応を取る検査にすると、`check-plan-tests.sh` の作りを変える別の作業になる
- 量でない値と量の取り違え（`cut(slice, dice)` など）をエラーにする — Issue の「単位」とは別の照合で、スコープ外にし（§1）、最終ゲートの後に別の作業として提案する
- `repeat` の中で同じ診断が回数分出る — 既存の診断と同じ振る舞いに従い、スコープ外にした（§1）
- `servings` を持たない料理への `make gohan(servings: 0)`、人数を式で書いた見出し、`Time in 0 s` の無視、数だけの範囲が JSON に出ないこと — どれも既存の別の挙動で、スコープ外にした（§1）。最初のものは別の作業として提案する
- §8 の文の伝わり方を grep で確かめる案 — 引数を使うことは `G-QC-DIAG` に足した。文が伝わるかは引き続き §8

---

## 6. フェーズとタスク

表記:

- **完了条件** は §5 のゴール ID か、合否が一意なコマンド。§5 の「手元で回す」前提ゴールは各タスクに暗黙で含まれる。push の後の前提ゴールは、push するゲートの完了条件にだけ含める。
- 状態の報告の形: 「core: `G-QC-*` と前提ゴール合格（4 構成）。wasm: `G-PLAT-WASM` 合格。GitHub: push の後に `G-PLAT-CI` を確認」。
- コミットは論理単位（golden の追加、実装の 1 つの変更、期待の直し、言い回しの追加、ゴールの追加）。golden を足すコミットは、その golden を通す実装のコミットより前に置く（`G-REPO-RED-FIRST`）。最初から合格する golden は別のコミットにし、`scripts/red-first-exceptions.txt` に理由を書く（§5.2）。既存の期待を直すコミットは、実装のコミットと分ける（`G-RF-SEPARATION`）。push は確認を取らずに行う（D81）が、その前に毎回 `G-REPO-NOREPLY` を回し、push した範囲を報告する。
- 各タスクの「わざと壊す確認」は、網羅表の方法で壊すとそのテストかゴールが落ち、戻すと合格することを確かめ、§9 に `- わざと壊す確認: ` の行で記録する。作業用のコミットが要る壊し方は、一時的な worktree で行い、`git reset --hard` で消し、push しない（D81）。
- 別の作業の未コミットの変更はコミットに含めない。`git add` はファイルを名指しする。

### Phase 0 — 決定と準備（機能のコードの変更なし）

| ID | 内容 | 完了条件 |
| --- | --- | --- |
| P0-1 | §2 の D73〜D87 をユーザーと確定し、本ファイルに書き込む | `grep -c '^### D[0-9]*\. .*【決定済み】' plans/ozen-quantity-checks.md` が 19、`grep -c '^### D[0-9]*\. .*【未決】' plans/ozen-quantity-checks.md` が 0、`grep -c '^- \*\*決定:' plans/ozen-quantity-checks.md` が 19（P0-3 の後は `G-QC-DECISIONS`） |
| P0-2 | 作業ツリーに別の作業の未コミットの変更が無いことを確かめる（あればユーザーに確認する）。本ファイルをコミットして push し、`main` にタグ `qc-start` を打つ（D81）。基準値（手元の前提ゴールと `G-RD-*`・`G-RF-*` の合否、golden の数 N0、`bounds/expression-unchanged` の期待を作る着手点のビルド）を §9 に記録する | `git rev-parse -q --verify refs/tags/qc-start` が成功し、§9 に基準値の行がある |
| P0-3 | `scripts/goals.sh` に `PLAN_QC`・`QC` と `goal_G_QC_*` を足す。`CLAUDE.md` の Plan の節に本ファイルの行を足す（D82）。`goal_G_RF_GOLDEN_COUNT` を D80 の形に直し、記法の計画の §9 に 1 行足す。`52a6ef1` にタグ `readme-end` を打ち、`RD` を `readme-start..readme-end` に直し、README の計画の §9 に 1 行足す（D87） | `scripts/goals.sh --list \| grep -c '^G-QC-'` が 18、`G-QC-CLAUDEMD`・`G-QC-COVERAGE`・`G-QC-RF-COUNT`・`G-QC-RD-RANGE`・`G-RF-GOLDEN-COUNT`・`G-REPO-CLAUDEMD`・`G-REPO-SELFTESTS` が合格。S755・S757・S781 のわざと壊す確認 |

P0-3 の時点では実装をまだ変えていないので、`G-QC-GOLDEN-*`・`G-QC-PRELUDE`・`G-QC-DIAG`・`G-QC-SCHEMA`・`G-QC-SYNTAX` は落ちる。§9 に、P0-3 の時点で落ちたゴールの一覧を記録する（ゴールが何かを確かめている証拠になる）。

### Phase 1 — 量の照合（#1、D69・D85・D86）

| ID | 内容 | 完了条件 |
| --- | --- | --- |
| P1-1 | `tests/golden/args/` の 21 個の golden を §4.1〜§4.3 から手で書き、実装より前のコミットにする（§4.1 の 16 個: `volume-for-time`・`mass-for-length`・`unitless-time`・`count-param`・`named-arg`・`pipe-arg`・`variable-arg`・`macro-body`・`macro-splice`・`composite`・`user-dimension`・`unknown-value`・`default-unchecked`・`continues-after`・`ok-units`・`issue1`。§4.2 の 4 個: `range-units-steps`・`range-units-dimension`・`range-units-mixed`・`range-units-ingredient`。§4.3 の 1 個: `make-arg-dimension`）。そのコミットの時点で合格する golden（見込みは `ok-units`・`default-unchecked`・`unknown-value`）は別のコミットにし、`scripts/red-first-exceptions.txt` に理由を書く（§5.2） | `scripts/golden-entries.sh tests/golden/args \| wc -l` が 21、前提の `G-REPO-RED-FIRST`。赤と対照の golden の名前を §9 に記録 |
| P1-2 | `check_value` の次元と範囲の判定を切り出し、`call_action`（合成の動作の重複の除き方を含む）と `make` の引数から呼ぶ。`eval_inner` で両端に単位を書いた範囲を評価する。既存の golden の期待が変わったら、S751 の手順で直すか調べ、直すなら別のコミットにする。状態の型の中の引数に同じ穴があるかを確かめて §9 に書く | `G-QC-GOLDEN-ARGS`・`G-QC-GOLDEN-CHANGED`、前提の `G-CORE-GOLDEN`（4 構成）・`G-SAMPLE-CHECK`・`G-CORE-FUZZ`・`G-RF-SEPARATION`。S695〜S708・S758〜S764 のわざと壊す確認 |

### Phase 2 — 下端を含まない範囲と、標準の動作の範囲（#2、D70・D73・D74・D79）

| ID | 内容 | 完了条件 |
| --- | --- | --- |
| P2-1 | `tests/golden/bounds/` のうち、範囲の書き方と標準の動作の範囲の 22 個を書き、実装より前のコミットにする（`exclusive-*` の 8 個: `from`・`upper-inclusive`・`upper-over`・`space`・`interval`・`kitchen`・`count`・`empty`。ほかの 14 個: `closed-zero-ok`・`expression-unchanged`・`types-json-exclusive`・`make-arg-range`・`time-zero`・`time-negative`・`range-units-negative-time`・`size-zero`・`size-negative`・`small-positive`・`soak-default`・`temperature-negative`・`dimension-before-range`・`composite-range`）。`expression-unchanged` の期待は着手点のビルドの `debug ast` の出力から作る（§5.2）。そのコミットの時点で合格する golden（見込みは `closed-zero-ok`・`expression-unchanged`・`small-positive`・`soak-default`・`temperature-negative`・`dimension-before-range`。`exclusive-space` は今も `SYN003` なので、位置が同じなら合格する）は別のコミットにし、例外に理由を書く。利用者の設定の型が `--emit types` に出るかを確かめる | `scripts/golden-entries.sh tests/golden/bounds \| wc -l` が 22、前提の `G-REPO-RED-FIRST`。赤と対照の golden の名前を §9 に記録 |
| P2-2 | パーサー・AST・`grammar.ebnf`・`rules.def`・`ozen.dict` に下端を含まない範囲（規則 `type_range`）を足す。範囲の判定、`base_type`、`defs.c` の `minExclusive`、スキーマ。期待を直すなら別のコミットにする | `exclusive-*`・`closed-zero-ok`・`expression-unchanged`・`types-json-exclusive`・`make-arg-range` と `api/types-json` が合格、`G-QC-SCHEMA`・`G-QC-SYNTAX` の 1〜3 行目、前提の `G-CORE-GRAMMAR`・`G-RF-FUZZ-DICT`・`G-API-SCHEMA`・`G-CORE-GOLDEN`・`G-RF-SEPARATION`。S709〜S719・S765〜S768 のわざと壊す確認 |
| P2-3 | `stdlib/prelude.ozen` の 8 つの引数に範囲を付ける。合成の動作の中の範囲の診断の位置を呼び出し元に向ける。既存の golden の期待の変化（`api/signatures-json` など）を S751 の手順で扱う | `time-*`・`range-units-negative-time`・`size-*`・`small-positive`・`soak-default`・`temperature-negative`・`dimension-before-range`・`composite-range`・`api/signatures-json` が合格、`G-QC-PRELUDE` の 1〜3 行目、前提の `G-CORE-GOLDEN`・`G-SAMPLE-CHECK`・`G-STD-*`・`G-RF-SEPARATION`。S720〜S727・S769〜S771 のわざと壊す確認 |

### Phase 3 — 材料の量と人数（#2、D71・D72・D75〜D77）

| ID | 内容 | 完了条件 |
| --- | --- | --- |
| P3-1 | `tests/golden/bounds/` の残りの 29 個と `renderers/test/settings-servings-min.test.mjs` を書き、実装より前のコミットにする（`amount-*` の 12 個: `zero`・`negative`・`range-zero`・`range-negative`・`range-units`・`per-serving-zero`・`per-serving-unknown`・`any`・`none`・`wrong-dim`・`expression`・`positive`。`servings-*` の 15 個: `header-zero`・`typed-zero`・`own-range`・`make-zero`・`make-positional`・`menu-header`・`flag-zero`・`flag-negative`・`flag-header-range`・`flag-over-header`・`flag-dimension`・`options-zero`・`one-ok`・`flag-one-ok`・`invalid-amounts`。ほかの 2 個: `optional-ranged`・`issue2`）。そのコミットの時点で合格するもの（見込みは `amount-any`・`amount-none`・`amount-positive`・`amount-wrong-dim`・`servings-own-range`・`servings-one-ok`・`servings-flag-one-ok`・`servings-flag-over-header`）は別のコミットにし、例外に理由を書く | `scripts/golden-entries.sh tests/golden/bounds \| wc -l` が 51、前提の `G-REPO-RED-FIRST`。赤と対照の golden の名前を §9 に記録 |
| P3-2 | `TYP021` を登録し（`diag_codes.def` の末尾、`diag-names.txt`、`phrases/*.ftl`、`names.mjs`）、`eval_ingredients` で出す。買う量・分ける量に同じ穴があるかを確かめて §9 に書く | `amount-*` が合格、`G-QC-DIAG` の 1〜6 行目、前提の `G-CORE-DIAG-REGISTRY`・`G-CORE-DIAG-APPEND`・`G-STD-PHRASES`・`G-REND-PHRASES-RENDER`・`G-REND-TESTS`・`G-API-SARIF`・`G-CORE-GOLDEN`・`G-RF-SEPARATION`。S728〜S739・S772 のわざと壊す確認 |
| P3-3 | `Options.servings` を `Count? in 1..` にし、人数の経路（見出し・`make`・献立の見出し・`-s`・`--options`）を検査し、同じ値の重複を除く（D77）。人数が範囲外のときの量の診断を抑える（D76） | `servings-*`・`optional-ranged`・`issue2`・`api/types-json`・`settings-servings-min` が合格、`G-QC-GOLDEN-BOUNDS`・`G-QC-PRELUDE`（全行）、前提の `G-CORE-GOLDEN`・`G-SAMPLE-CHECK`・`G-REND-TESTS`・`G-RF-SEPARATION`。S741〜S750・S773〜S780・S785 のわざと壊す確認 |

### Phase 4 — 文書と仕上げ

| ID | 内容 | 完了条件 |
| --- | --- | --- |
| P4-1 | `docs/concept.md` §4.2 に下端を含まない範囲と、範囲外がエラーになる対象に動作の引数を足す。`type Options` の例の `servings` を直す。§6.1 のエラーの例に「0 以下の量」を足す。`severity-examples.txt` に行を足す | `G-QC-SYNTAX`（全行）、`G-QC-DIAG`（全行）、前提の `G-TYPE-SEVERITY-TABLE`・`G-RF-CONCEPT`。S740・S756・S783・S784 のわざと壊す確認 |
| P4-2 | 網羅の監査: §9 で増えた事実を網羅表に足す。§8 の各項目を、実装後の知識でもう一度「コマンドで決められるか」で判定する。独立したサブエージェントに、§1・§4・§8・網羅表と、足した golden と `sema.c` の差分を読ませ、テストの無い主張と、§8 にあるがコマンドで決められる項目を列挙させる。指摘は足すか、§5.3 の末尾に足さない理由を書く | `G-QC-COVERAGE`・`G-QC-PLAN-NAMES`・`G-QC-AUDIT`。§9 に監査の指摘の件数と対応 |
| P4-3 | 最終ゲート。S751〜S754・S782 のわざと壊す確認をする。`G-REPO-NOREPLY` を回して push し、CI を確かめる | `G-QC-PLAN-TESTS`・`G-QC-SCOPE`・`G-QC-VERSION`・`G-QC-GOLDEN-CHANGED` を含む §5 の全ゴール（`G-QC-ISSUES` を除く）と手元の前提ゴール、push の後の `G-PLAT-CI` |
| P4-4 | D84 のとおり Issue に返答して閉じる。文面は送る前にユーザーに見せる。§8 をユーザーに渡し、スコープ外にした別の作業（§1）を提案する | `G-QC-ISSUES` |

---

## 7. フェーズ間の依存

- P0-1 → P0-2 → P0-3 の順。P0-3 のゴールが無いと、後のタスクの完了条件を判定できない。
- Phase 1 → Phase 2 → Phase 3 の順。Phase 2 の標準の動作の範囲（P2-3）と `make` の範囲（P2-2）は、Phase 1 の照合が無いと効かない。Phase 3 の `Options.servings` の範囲（P3-3）は、P2-2 の `base_type` の直しが要る。`amount-range-units`（P3-1）は P1-2 の範囲の評価が要る。
- 各 Phase の中では、golden を書くタスク（P1-1・P2-1・P3-1）が実装より先。P2-2 と P2-3、P3-2 と P3-3 は触るファイルが一部重なる（`sema.c`）ので、並行せず順に行う。
- P4-1 は Phase 3 の後（§6.1 の例は `TYP021` の登録の後でないと `G-TYPE-SEVERITY-TABLE` が落ちる）。P4-2 → P4-3 → P4-4 の順。

---

## 8. ユーザーに引き渡す確認

Claude は機械的なゴールまでを担当する。以下はユーザーが確認する。各項目には、コマンドで決められない理由を付ける。理由が書けない項目は §5.3 でテストにする。

- `TYP021` の日本語と英語の文（`diag-non-positive-amount`）が、レシピを書く人に何を直せばよいか伝わるか（言い回しがあり 2 つの引数を使うことは `G-QC-DIAG`、描画できることは `G-REND-PHRASES-RENDER` で確かめる） — 機械で決められない理由: 文が伝わるかは読み手の判断
- `docs/concept.md` §4.2 の下端を含まない範囲の説明が、`<..` の意味を誤解なく伝えるか（例と語があることは `G-QC-SYNTAX` で確かめる） — 機械で決められない理由: 説明が伝わるかは読み手の判断
- Issue #1・#2 への返答の言い方（D84。送る前に見せる。診断のコードが載ることは `G-QC-ISSUES` で確かめる） — 機械で決められない理由: 外部への投稿の言い方は人の判断

---

## 9. 着手時に確認して記録する事項

（各タスクで確認した結果を、ここに日付付きで追記する）

- 計画を書いた後の独立した監査（2026-11-02）: 指摘 30 件。25 件を取り込み、5 件は取り込まずスコープ外か既存の検査の限界とした（理由は §5.3 の末尾）。主な変更は、両端に単位を書いた範囲の評価（D85）・`make` の量の引数（D86）・完了した計画の範囲のゴール（D87）の設計判断を足したこと、条件の人数の重複と人数の誤りによる量の診断の重複を除く規則（D76・D77）、合成の動作とマクロの差し込みの診断の位置、効かなかったわざと壊す方法 8 件の直し、`G-QC-PRELUDE`（温度の範囲の見逃し）・`G-QC-GOLDEN-CHANGED`（タグの確認）・`G-QC-SYNTAX`（concept の節を §4.2 に、規則名 `type_range`）の直し、`G-RF-SEPARATION` を前提ゴールに足したこと、ビルド時の条件の書き方（`kitchen today : Options`）の誤りの直し
- 着手点の基準値（P0-2、2026-11-02）: タグ `qc-start` は `0b4b4eb`（本計画をコミットした点）。golden の数 N0 は **464**。前提ゴール 115 個と `G-CORE-BUILD` の計 116 個を回し、114 個が合格、2 個が落ちた。落ちたのはどちらも本計画が直す予定のもの:
  - `G-RF-GOLDEN-COUNT`: golden が 448 ではなく 464。別の作業が golden を足したため。D80 で判定の形を変える（P0-3）。
  - `G-RD-SCOPE`: `readme-start..HEAD` の範囲の外のファイルが 57。README の計画の後の別の作業が範囲に入ったため。D87 で範囲を `readme-end` で閉じる（P0-3）。
  - 1 回目の実行では `G-PLAT-WASM` と `G-REPO-SELFTESTS` も落ちた。原因は手元の `build/core-wasm/` が 2026-11-01 のビルドのまま古かったこと（golden 17 件・worker 10 件の不一致はどちらも新しい診断の分）。`scripts/build-wasm.sh` で作り直すと 2 つとも合格した。コードの問題ではない。
- 計画を書いた点 `8db17e6` から `qc-start` までに、別の作業のコミットが 24 件ある（`0b7db9e`〜`d181933`。`NAM019`〜`NAM021` の追加、golden 16 件の追加など）。§3 の行番号は `8db17e6` の時点のもので、`qc-start` ではおよそ 25 行後ろにずれる（例 `check_value` は 2665 行、`call_action` は 4074 行）。`TYP021` は `NAM021` の後に足す。
- `bounds/expression-unchanged` のオラクルは、`qc-start` でビルドした `build/core-gcc-release/ozen` の写し `build/qc-start-ozen`（`build/` は git の管理外。消えたら `qc-start` を worktree に出してビルドし直す）。
- P0-3 の時点の `G-QC-*` の合否（実装を変える前に落ちるゴールの一覧）
- 赤の確認（P1-1・P2-1・P3-1）: 足したコミットの時点のビルドで落ちた golden（赤）と、合格した golden（対照。`scripts/red-first-exceptions.txt` に載せたコミット）の名前
- `<..` を型の範囲で読んだときの既存の読み方との衝突の確認（P2-2。§3.2 の見込みを実際のパーサーで確かめる）と、AST の表し方（新しい節の種類か印か）
- 状態の型の中の引数（`T<shape: cut(slice, 0 mm)>`）に同じ穴があるか（P1-2）
- 買う量・分ける量に 0 以下の量が通る穴があるか（P3-2）
- 利用者が定義した設備の設定の型の範囲付きのフィールドが `--emit types` の出力に出るか（P2-1。出なければ `types-json-exclusive` を、出る型で書き直し、理由を書く）
- 期待を直した golden（各タスク）: `- 期待を直した golden: \`<パス>\` — <理由>` の形で 1 行ずつ書く
- 範囲に入った別の作業のコミット（あれば）: `- 範囲に入った別の作業のコミット: <SHA>（<内容>）` の形で書く
- わざと壊す確認の記録（各タスク）: `- わざと壊す確認: S<n>・<テストかゴール>（<タスク>、<日付>）。<壊し方> で落ちた。戻すと合格した` の形で書く
- 網羅の監査（P4-2）: `- 網羅の監査: 指摘 <k> 件、取り込み <m> 件` の形で書く
- P0-3 の時点の `G-QC-*` の合否（2026-11-02、`goals.sh` に 18 個を足した直後）: 合格は `G-QC-DECISIONS`・`G-QC-COVERAGE`・`G-QC-GOLDEN-CHANGED`・`G-QC-SCOPE`・`G-QC-VERSION`・`G-QC-CLAUDEMD`・`G-QC-RF-COUNT`・`G-QC-RD-RANGE` の 8 個。落ちたのは `G-QC-PLAN-NAMES`（`75 names, 73 missing`）・`G-QC-PLAN-TESTS`（`107 rows, 0 recorded`）・`G-QC-GOLDEN-ARGS`・`G-QC-GOLDEN-BOUNDS`（golden が無い）・`G-QC-PRELUDE`（4 つのうち 3 つ）・`G-QC-DIAG`（7 つすべて）・`G-QC-SCHEMA`・`G-QC-SYNTAX`（7 つすべて）・`G-QC-ISSUES`・`G-QC-AUDIT` の 10 個。`G-RF-GOLDEN-COUNT`・`G-RD-SCOPE`・`G-RD-ADD-ONLY`・`G-REPO-CLAUDEMD`・`G-REPO-SELFTESTS` は合格
- わざと壊す確認: S755・前提の固定・`G-QC-RF-COUNT`（P0-3、2026-11-02）。`goal_G_RF_GOLDEN_COUNT` を今の数と比べる元の形に戻すと「readme-start in G-RF-GOLDEN-COUNT: got '0'」と「G-RF-GOLDEN-COUNT: exit 1」で落ちた。戻すと合格した
- わざと壊す確認: S755・副作用とコスト・`G-RF-GOLDEN-COUNT`（P0-3、2026-11-02）。一時的な worktree で `readme-start` にある golden `linear/unused-alias` を消すと「readme-start goldens missing now: got '1'」で落ちた。worktree を消すと合格した
- わざと壊す確認: S781・前提の固定・`G-QC-RD-RANGE`（P0-3、2026-11-02）。`goals.sh` の `RD=` を `readme-start..HEAD` に戻すと「RD range: got '0'」で落ち、`G-RD-SCOPE` も「files outside scope: got '57'」で落ちた（計画を書いた時点の 2 から、後の作業でさらに増えていた）。戻すと合格した
- わざと壊す確認: S757・正常系・`G-QC-CLAUDEMD`（P0-3、2026-11-02）。`CLAUDE.md` の本計画の行の `qc-start` を `qc` にすると「plan line in CLAUDE.md: got '0'」で落ちた。戻すと合格した
- 赤の確認（P1-1、2026-11-02）: `9426fff` で足した 18 個（`volume-for-time`・`mass-for-length`・`unitless-time`・`count-param`・`named-arg`・`pipe-arg`・`variable-arg`・`macro-body`・`macro-splice`・`composite`・`user-dimension`・`continues-after`・`issue1`・`range-units-steps`・`range-units-dimension`・`range-units-mixed`・`range-units-ingredient`・`make-arg-dimension`）は、着手点のビルドですべて落ちた（赤）。`144bba6` で足した対照の 3 個（`ok-units`・`unknown-value`・`default-unchecked`）は合格し、`scripts/red-first-exceptions.txt` に載せた
- `args/unknown-value` の書き方（P1-1、2026-11-02）: §5.3 の見込み（`eval/runtime-value-in-if` の書き方）ではなく、既定値の無いレシピの引数 `recipe tamagoyaki(t: Time)` を動作に渡す形にした。単独で検査するとき `t` は値を持たず（`debug steps` に `fry(egg, t)` と出る）、比較の `TYP015` のような別の診断が混ざらない
- `args/composite` の形（P1-1、2026-11-02）: 中で引数を使わない `quickA` と、中の `fry(x, t)` に引数を渡す `quickB` の 2 つの合成の動作を並べた。`quickB` だけでは、外の照合を飛ばしても中の照合が同じ位置に同じ診断を出すので、S702 のわざと壊す確認が効かないため
- 利用者の動作の golden（`count-param`・`user-dimension`・`default-unchecked`）は、卵（`Hazard`）だと加熱しない動作の結果が `SAF001` になるので、豆腐（`RawEdible`）を使った
- 状態の型の中の引数（P1-2、2026-11-02）: 同じ穴がある。利用者の動作の引数の型 `T<shape: cut(slice, 3 ml)>` も `T<shape: cut(slice, 0 mm)>` も、材料の型 `Cucumber<shape: cut(slice, 3 ml)>` も `result ok` になる（`3 mm` に切った値を `3 ml` の型の引数に渡しても通る）。照合の経路が別なので本計画では直さず、最終ゲートの後に別の作業として提案する
- `args/unknown-value` の直し（P1-2、2026-11-02）: S704 のわざと壊す確認（未知の値を次元の無い量として照合する）が効かなかった。既定値の無いレシピの引数 `t` は、束縛も定義も無い名前として列挙の値（`V_ENUM`）に評価され、未知の値（`V_UNKNOWN`）にならないため。引数を `t + 1 min`（列挙の値と量の和で、評価の結果が未知の値になる）に直すと、同じ壊し方で落ちた。期待（`.out`）は `result ok` のまま変えていない
- S701 のわざと壊す方法の差し替え（P1-2、2026-11-02）: 網羅表の「`arg_site` の trail を設定しない」は効かない。`arg_site` は差し込みでない節には評価中の trail（マクロの本文を評価している間は展開履歴そのもの）を返すので、設定しなくても同じ trail になる。代わりに「照合の診断の trail を空にする」で確かめた
- わざと壊す確認: S695・正常系・`cgolden/args/volume-for-time`（P1-2、2026-11-02）。`call_action` の照合の呼び出しを消すと `volume-for-time` を含む 14 本が落ちた。戻すと合格した
- わざと壊す確認: S695・正常系・`cgolden/args/mass-for-length`（P1-2、2026-11-02）。照合を `Time` の宣言にだけ効くように狭めると `mass-for-length`・`count-param`・`user-dimension` が落ちた。戻すと合格した
- わざと壊す確認: S696・境界・上限・`cgolden/args/unitless-time`（P1-2、2026-11-02）。次元の無い量を照合の前に読み飛ばすと `unitless-time` だけが落ちた。戻すと合格した
- わざと壊す確認: S697・効かない条件・`cgolden/args/count-param`（P1-2、2026-11-02）。照合で `pcs` を次元の無い量に直す処理を外すと `count-param` だけが落ちた。戻すと合格した
- わざと壊す確認: S698・実際の経路・`cgolden/args/named-arg`（P1-2、2026-11-02）。照合を位置で渡した引数だけにすると `named-arg` だけが落ちた。戻すと合格した
- わざと壊す確認: S699・実際の経路・`cgolden/args/pipe-arg`（P1-2、2026-11-02）。`pipe_into` で左辺を最後の引数に付けると `pipe-arg`・`mass-for-length`・`ok-units` が落ちた。戻すと合格した
- わざと壊す確認: S700・正常系・`cgolden/args/variable-arg`（P1-2、2026-11-02）。診断の位置を呼び出し全体にすると `variable-arg` を含む 14 本が落ちた。戻すと合格した
- わざと壊す確認: S701・実際の経路・`cgolden/args/macro-body`（P1-2、2026-11-02）。照合の診断の trail を空にすると `macro-body` だけが落ちた。戻すと合格した
- わざと壊す確認: S758・実際の経路・`cgolden/args/macro-splice`（P1-2、2026-11-02）。照合の診断の位置を `arg_site` を通さず引数の節そのもの（マクロの本文の `$t`）にすると `macro-splice` だけが落ちた。戻すと合格した
- わざと壊す確認: S702・実際の経路・`cgolden/args/composite`（P1-2、2026-11-02）。合成の動作のときは外の照合を飛ばすと `composite` だけが落ちた。戻すと合格した
- わざと壊す確認: S759・副作用とコスト・`cgolden/args/composite`（P1-2、2026-11-02）。外で照合に落ちた引数でも中の呼び出しで照合の診断を出すと `composite` だけが落ちた（`5 g` に 2 つ出る）。戻すと合格した
- わざと壊す確認: S703・正常系・`cgolden/args/user-dimension`（P1-2、2026-11-02）。照合を標準の 6 つの次元に限ると `user-dimension` だけが落ちた。戻すと合格した
- わざと壊す確認: S704・効かない条件・`cgolden/args/unknown-value`（P1-2、2026-11-02）。未知の量も照合し、次元の無い量として扱うと `unknown-value`・`range-units-mixed` が落ちた。戻すと合格した
- わざと壊す確認: S705・効かない条件・`cgolden/args/default-unchecked`（P1-2、2026-11-02）。既定値を評価した後にも照合を呼ぶと `default-unchecked` だけが落ちた。戻すと合格した
- わざと壊す確認: S706・副作用とコスト・`cgolden/args/continues-after`（P1-2、2026-11-02）。次元の誤りで `call_action` から未知の値を返すと `continues-after` を含む 12 本が落ちた。戻すと合格した
- わざと壊す確認: S707・効かない条件・`cgolden/args/ok-units`（P1-2、2026-11-02）。区間の値をエラーにすると `ok-units`・`range-units-steps` が落ちた。戻すと合格した
- わざと壊す確認: S707・効かない条件・`cgolden/args/ok-units`（P1-2、2026-11-02）。列挙の値も次元の無い量として照合し、次元でない型の引数にも次元の誤りを出すと `ok-units`・`mass-for-length`・`unknown-value` が落ちた。戻すと合格した
- わざと壊す確認: S708・実際の経路・`cgolden/args/issue1`（P1-2、2026-11-02）。`TYP001` の重大度を警告にすると `issue1` を含む 16 本が落ちた。戻すと合格した
- わざと壊す確認: S760・正常系・`cgolden/args/range-units-steps`（P1-2、2026-11-02）。`eval_inner` の範囲の評価を消すと `range-units-steps` を含む 4 本が落ちた。戻すと合格した
- わざと壊す確認: S761・実際の経路・`cgolden/args/range-units-dimension`（P1-2、2026-11-02）。範囲の評価で結果の次元を記録しないと `range-units-dimension`・`range-units-steps`・`range-units-ingredient` が落ちた。戻すと合格した
- わざと壊す確認: S762・境界・上限・`cgolden/args/range-units-mixed`（P1-2、2026-11-02）。両端の次元を比べず左の次元を使うと `range-units-mixed` だけが落ちた。戻すと合格した
- わざと壊す確認: S763・データの完全性・`cgolden/args/range-units-ingredient`（P1-2、2026-11-02）。材料の量の評価で範囲の節を未知にすると `range-units-ingredient` だけが落ちた。戻すと合格した
- わざと壊す確認: S764・実際の経路・`cgolden/args/make-arg-dimension`（P1-2、2026-11-02）。`make` の量の引数の照合を消すと `make-arg-dimension` だけが落ちた。戻すと合格した
- 赤の確認（P2-1、2026-11-02）: `b894674` で足した 15 個（`exclusive-from`・`exclusive-upper-inclusive`・`exclusive-upper-over`・`exclusive-interval`・`exclusive-kitchen`・`exclusive-count`・`exclusive-empty`・`types-json-exclusive`・`make-arg-range`・`time-zero`・`time-negative`・`range-units-negative-time`・`size-zero`・`size-negative`・`composite-range`）は、足した時点のビルド（P1 の実装の後）ですべて落ちた（赤）。`095a7c8` で足した対照の 7 個（`closed-zero-ok`・`expression-unchanged`・`small-positive`・`soak-default`・`temperature-negative`・`dimension-before-range`・`exclusive-space`）は合格し、`scripts/red-first-exceptions.txt` に載せた
- 利用者の設備の設定の型（P2-1、2026-11-02）: `type ScaleSpec : EquipmentSpec { maxLoad: Mass in 0 g.. }` の範囲付きのフィールドは、`--emit types` の `equipment` の `Scale` に `"min":"0 g"` として出る。`types-json-exclusive` はこの型で書いた
- `bounds/types-json-exclusive` の期待の作り方（P2-1、2026-11-02）: 型の一覧の JSON は標準の設備と器の型をすべて含み、手で書くには長い。そこで、同じ名前のファイルで範囲を閉じた形（`0 g..`）にしたものを着手点のビルド `build/qc-start-ozen` に掛けた出力を土台にし、`maxLoad` のフィールドだけを手で `"min":"0 g","minExclusive":true,"max":null` に直した。ほかの部分は本計画で変わらない（D79）ので、変える前の実装がオラクルになる。P3 で `Options.servings` の `min` が `"1"` になるので、そのとき期待を直す
- `bounds/expression-unchanged` の期待（P2-1、2026-11-02）: §5.2 のとおり、`build/qc-start-ozen debug ast` の出力から作った（`3 min<..5 min` が `Binary <` と `RangeTo` に読まれる）
- `<..` の読み方と AST の表し方（P2-2、2026-11-02）: 新しい規則 `type_range` を `type()` の `in` の後だけで読む関数にした。`range()` で読んだ値の直後に `<` があり、その次のトークンが空白を挟まない `..` のときだけ、新しい節の種類 `RangeAbove`（子は下端と、あれば上端）を作る。印ではなく節の種類にしたのは、範囲を読む 3 か所（`check_qty`・`put_field`・診断の範囲の文字）がどれも節の種類で分岐しているため。`RangeAbove` と規則 `type_range` は、既存の番号を変えないよう `ast.h` の節の種類と `rules.def` の末尾に足した。既存の golden はすべて変わらず合格し（`T<shape: cut>` の型の引数を含む）、式の中の `3 min<..5 min` は `bounds/expression-unchanged` のとおり今までと同じ木になる。§3.2 の見込みどおり衝突は無い
- `schemas/result.schema.json` の `"min": {` は 4 か所（設備・器・台所・条件の型のフィールド）あり、4 か所すべてに `minExclusive` を足した（`G-QC-SCHEMA` は数が同じことを見る）
- S718 のわざと壊す方法の場所（P2-2、2026-11-02）: `mark(p, OZNI_R_type_range)` は `type()` ではなく、`type()` から呼ぶ `type_range()` の先頭に置いた。わざと壊す確認はそこを消して行った
- わざと壊す確認: S765・実際の経路・`cgolden/bounds/make-arg-range`（P2-2、2026-11-02）。`make` の照合に範囲を外した型を渡すと `make-arg-range` が落ちた。戻すと合格した
- わざと壊す確認: S709・境界・上限・`cgolden/bounds/exclusive-from`（P2-2、2026-11-02）。下端を含まない判定を閉区間の判定にする（下端ちょうどを通す）と `exclusive-from` を含む 6 本が落ちた。戻すと合格した
- わざと壊す確認: S710・境界・上限・`cgolden/bounds/exclusive-upper-inclusive`（P2-2、2026-11-02）。上端の判定を `>=` にすると `exclusive-upper-inclusive` が落ちた。戻すと合格した
- わざと壊す確認: S710・境界・上限・`cgolden/bounds/exclusive-upper-over`（P2-2、2026-11-02）。`type_range()` で `a<..b` の上端を読み捨てると `exclusive-upper-over` が落ちた。戻すと合格した
- わざと壊す確認: S711・効かない条件・`cgolden/bounds/exclusive-space`（P2-2、2026-11-02）。`<` と `..` の間の空白を許すと `exclusive-space` が落ちた。戻すと合格した
- わざと壊す確認: S712・正常系・`cgolden/bounds/exclusive-from`（P2-2、2026-11-02）。診断の 3 つ目の引数を下端の文字だけにすると `exclusive-from` を含む 7 本が落ちた。戻すと合格した
- わざと壊す確認: S713・境界・上限・`cgolden/bounds/exclusive-interval`（P2-2、2026-11-02）。区間の値の下端の判定に値の上端を使うと `exclusive-interval` が落ちた。戻すと合格した
- わざと壊す確認: S714・効かない条件・`cgolden/bounds/closed-zero-ok`（P2-2、2026-11-02）。閉じた範囲も下端を含まない判定にすると `closed-zero-ok` が落ちた。戻すと合格した
- わざと壊す確認: S715・効かない条件・`cgolden/bounds/expression-unchanged`（P2-2、2026-11-02）。`range()`（式と共用）で `<..` を読むと `expression-unchanged`・`exclusive-space` が落ちた。戻すと合格した
- わざと壊す確認: S766・実際の経路・`cgolden/bounds/exclusive-kitchen`（P2-2、2026-11-02）。`check_value`（台所の設定の経路）では下端を含まない範囲を見ないと `exclusive-kitchen` が落ちた。戻すと合格した
- わざと壊す確認: S767・境界・上限・`cgolden/bounds/exclusive-count`（P2-2、2026-11-02）。下端が数だけのときは下端を含まない印を読み捨てると `exclusive-count`・`exclusive-empty` が落ちた。戻すと合格した
- わざと壊す確認: S768・境界・上限・`cgolden/bounds/exclusive-empty`（P2-2、2026-11-02）。下端と上端が等しい範囲では下端の判定を閉区間にすると `exclusive-empty` が落ちた。戻すと合格した
- わざと壊す確認: S716・データの完全性・`cgolden/bounds/types-json-exclusive`（P2-2、2026-11-02）。`minExclusive` を出さないと `types-json-exclusive` が落ちた。戻すと合格した
- わざと壊す確認: S716・効かない条件・`cgolden/api/types-json`（P2-2、2026-11-02）。下端のある範囲にはすべて `minExclusive` を出し、含む範囲では `false` にすると `api/types-json` が落ちた。戻すと合格した
- わざと壊す確認: S717・データの完全性・`G-QC-SCHEMA`（P2-2、2026-11-02）。スキーマの型のフィールドから `minExclusive` を消すと「minExclusive beside min: got '0'」と「schema: got 'json: 1 files, 1 invalid'」で落ちた。戻すと合格した
- わざと壊す確認: S718・データの完全性・`G-CORE-GRAMMAR`（P2-2、2026-11-02）。`type_range()` の `mark(p, OZNI_R_type_range)` を消すと「grammar: 134 rules, 1 not covered」で落ちた。戻すと合格した
- わざと壊す確認: S718・文書との一致・`G-QC-SYNTAX`（P2-2、2026-11-02）。`core/grammar.ebnf` から `type_range` の規則を消すと「grammar type_range: got '0'」で落ちた。戻すと合格した
- わざと壊す確認: S719・データの完全性・`G-QC-SYNTAX`（P2-2、2026-11-02）。`core/fuzz/ozen.dict` から `"<.."` を消すと「fuzz dict: got '0'」で落ちた。戻すと合格した

---

## 10. リスク

- **R1. 既存の golden・サンプルが、単位の無い引数や 0 の値や両端に単位を書いた範囲を使っている**: 2026-11-02 に `git grep` で動作の呼び出しの単位の無い数・0 以下の材料の量を探し、見つからなかった（`tests/golden/kitchen/negative-count.ozen` の `count: -1` は台所の設定で、既にエラーを期待している）。範囲の評価（D85）は、今「適量」や `?` と出ている出力を変えうる。漏れがあれば P1-2・P2-3・P3-2 で `G-CORE-GOLDEN`・`G-SAMPLE-CHECK` が落ちる。期待を直すか（S751）、直すべきでない変化かを個別に調べる。
- **R2. `<..` の読み方が、型の範囲の外の書き方と衝突する**: 型の `in` の後だけで読むことで避ける（D74）。`bounds/expression-unchanged` と既存の golden（`G-CORE-GOLDEN`）、`G-CORE-FUZZ` で検出する。
- **R3. 人数の検査が、人数を式で使う量（`150 g * servings`）や `per serving` の計算を変える**: 人数が範囲内なら値は変わらない。既存の `quantity/per-serving*`・`quantity/count-options`・`menus/make-servings` の golden が変わらないことで確かめる（`G-QC-GOLDEN-CHANGED` で直した期待を名指しさせる）。
- **R4. 別の作業と同じ `main`・同じ作業ツリーを使う**: README の計画の R3 と同じ。範囲に入った別の作業のコミットは §9 に名指しして `G-QC-SCOPE` から除く（D81）。
- **R5. `G-REPO-RED-FIRST` が遅い**: 期待を足したコミットごとに worktree でビルドし直すので、golden を足すコミットを Phase ごとに赤と対照の 2 つ（計 6 つ）にまとめて回数を抑える。
- **R6. `G-QC-ISSUES` は `gh` の認証とネットワークが要る**: 使えないときは落ちたとして扱い、理由を報告する（合格にしない）。
- **R7. 語彙の表から標準ライブラリを作り直すと範囲が消える**: `tools/std-from-vocab/convert.py` は表の次元だけを写すので、もし誰かが標準ライブラリを表から作り直すと、本計画の範囲が消える。今はどのゴールからも呼ばれていない。消えたら `G-QC-PRELUDE` が落ちる。
