# 人と AI が Ozen を書きやすくするための候補の洗い出し

- 作成日: 2026-11-02
- 種類: **洗い出し**（実装計画ではない）。ここで候補と順番を決め、決まったものから `verifiable-plan` で実装計画（`plans/ozen-<slug>.md`、`G-*` のゴールつき）を別に作る。
- 要求: 人と AI がレシピを書く・直す（AI に任せて書かせる「VibeCoding」を含む）ときに、効率よく、ストレスなく進められるよう、Ozen が提供すべきものを計画したい（2026-11-02、ユーザーがチャットで依頼）。
- 確かめた事実: 2026-11-02 に `18e8e43` のファイルを読み、`build/core-gcc-release/ozen` で間違いを入れたレシピを検査して確かめた（§2・§3）。2026-11-03 に `47292bc`（利用者のフィードバックの計画を実装した後）からビルドし直し、§3 の例と、二重消費・構文エラー・`--lang ja`・`--help` の出力が変わっていないことを確かめた。
- 番号: 候補は **C1** から、決めることは **Q1** から付ける。既存の計画の D・S の番号とは別で、実装計画にするときに D・S の番号を振り直す。

---

## 1. 目的

レシピを書くときの「書く → 検査 → 直す」の往復を、短く、迷わないものにする。

- **人**: エラーを読んですぐ直せる。使える動作と、その動作に要る状態を、書く前に調べられる。
- **AI**: 少ない往復で検査に通るレシピを書ける。診断を機械で読んで、そのまま直せる。直した結果が意図と合っているかを、文章に戻して確かめられる。

人と AI に要るものはほとんど同じで、違いは「読む形」（文章か JSON か）だけ。だから、診断や動作の一覧は 1 つの源から両方の形を出す。

## 2. 今あるもの

| 領域 | 今あるもの | 場所 |
| --- | --- | --- |
| 診断の形 | コード・名前・重さ・span・引数・関連する位置・直し方の候補。`--format json` と `--format sarif` | `ozen check`、`schemas/result.schema.json` |
| 直し方の候補 | 一部の診断に文字列で付く（例 `peel(onion)`）。テキストでは `--fixes` で表示 | `core/src/sema.c` ほか |
| 診断の文面 | `phrases/ja.ftl`・`en.ftl` に `diag-*` が約 110 件。レンダラーの `diagText` が使う | `renderers/lib/names.mjs` |
| 重さの設定 | `--allow`・`--warn`・`--deny`・`--strict`、`@allow`・`@expect` など | concept §6.1 |
| 文章に戻す | `ozen render --kind recipe\|schedule\|shopping\|html` | `renderers/` |
| 中を見る | `ozen debug ast\|definitions\|steps\|ir\|schedule\|embedded-std` ほか | `cli/src/main.c` |
| 構文エラーの回復 | 1 回の検査で複数の構文エラーを出す | `golden/syntax/recover-multiple` |
| 出力の安定 | 同じ入力なら同じ出力（golden で比較） | `tests/` |
| エディタ連携 | なし。LSP・構文ハイライトは concept §12 で後回し | — |

## 3. 今のつまずき（実際に試した例）

次のレシピには間違いが 2 つある（玉ねぎの皮をむいていない、`simmer` を `simer` と打ち間違えた）。

```ozen
recipe soup(servings: 2) {
  ingredients {
    water: 200 ml per serving
    onion: 0.5 pcs per serving
  }
  let onionCut = onion |> cut(slice, 3 mm)
  let soup = simer([water, onionCut], 15 min)
}
```

```text
warning LIN005 unusedValue a.ozen:3:5-3:10 args=["water", "Water"]: unused value
error TYP006 stateRequired a.ozen:6:18-6:23 args=["cut", "x", "onion"]: state required
error NAM001 undefined a.ozen:7:14-7:19 args=["simer"]: undefined
result error errors=2 warnings=1 infos=0
```

ここから見えるつまずき:

1. **文面が短く英語だけ**: 「state required」「undefined」だけで、何をすればよいかが書いていない。`phrases/ja.ftl` には「cut の x に、onion は今の状態では渡せません」があるのに、`ozen check` は使わない。`--lang ja` も効かない。
2. **似た名前を出さない**: `simer` に「`simmer` ではありませんか」と言わない。
3. **余計な診断が連鎖する**: `simer` が分からないせいで、`water` に「使われない値」の警告が出る。直すべきは 1 か所なのに 3 件に見える。
4. **直し方の候補がどこをどう直すかを言わない**: 候補は `peel(onion)` という文字列だけで、置き換える範囲がない。しかも書き方が `onion |> peel` ではなく関数呼び出しの形で、サンプルの書き方と合わない。
5. **関連する位置が空**: 同じ水を 2 回使う（`LIN001 doubleUse`）と、2 回目の位置だけが出て、1 回目の位置が出ない。
6. **構文エラーが期待した記号を言わない**: `boil(water 3 min)` は「unexpected token 3」で、「`,` が要ります」とは言わない。
7. **テキスト出力にソースの抜粋がない**: 行と列の数字だけで、どこを指すかを目で追えない。
8. **`--help` がない**: `ozen --help` は「unknown option」になる。
9. **書く前に調べる手段が少ない**: `std` の動作と、それぞれに要る状態・できる状態を一覧で引くコマンドがない（`debug embedded-std` は `std` のソースそのもの）。
10. **エディタの助けがない**: 補完・ホバー・その場の診断がない。

## 4. 候補

大きさは目安（小: 数日、中: 1〜2 週間、大: それ以上）。優先は §5 の案。

### A. 診断を読みやすくする

| ID | 内容 | 例 | 大きさ |
| --- | --- | --- | --- |
| C1 | 似た名前の候補を出す（名前・動作・状態・単位） | `simer` → 「`simmer` ではありませんか」 | 小 |
| C2 | 連鎖の診断を抑える。分からない名前に渡した値には「使われない値」を出さない | §3 の `water` の警告が消える | 小 |
| C3 | 関連する位置を埋める（二重消費の 1 回目、状態が変わった場所） | 「1 回目はここ（5 行目）」 | 小〜中 |
| C4 | `ozen check` の文面に `phrases/` を使い、`--lang ja\|en` で選べるようにする | 「onion は皮をむいてから切ってください」 | 中 |
| C5 | テキスト出力にソースの行と印（`^~~~`）を出す | Rust・Elm の形 | 小 |
| C6 | 構文エラーで、期待した記号や書き方を言う | 「`water` と `3 min` の間に `,` が要ります」 | 中 |
| C7 | `ozen explain <コード\|名前>`: 診断ごとに、意味・悪い例・直した例を出す。例は golden で検査して古くならないようにする | `ozen explain stateRequired` | 中 |

### B. 直しを機械で当てられるようにする

| ID | 内容 | 例 | 大きさ |
| --- | --- | --- | --- |
| C8 | 直し方の候補を「範囲 + 置き換える文字列」にする（SARIF の `fixes` と LSP の `TextEdit` にそのまま写せる形） | 6 行目の `onion` を `onion \|> peel` に | 中 |
| C9 | 候補を当てるコマンド（`ozen fix`、または `check --apply-fixes`）。安全（`SAF`）に関わる直しは自動では当てない | AI も人も 1 手で直せる | 中 |
| C10 | 候補の書き方を、材料を先に書く書き方（`\|>`）にそろえる | `peel(onion)` → `onion \|> peel` | 小 |

### C. 書く前に調べられるようにする

| ID | 内容 | 例 | 大きさ |
| --- | --- | --- | --- |
| C11 | `ozen --help` と、コマンドごとの `--help` | — | 小 |
| C12 | `ozen doc [名前]`: `std` の動作・食材・道具を引く。引数、要る状態、できる状態、例。`--format json` も | `ozen doc cut` → 「皮をむいた物を切る。`cut(how, size)`」 | 中 |
| C13 | AI 向けの手引きを 1 ファイルにまとめる: 文法の要約、動作の一覧、よくある誤りと直し方。C12・C7 と同じ源から生成し、手で書かない | 文書と開発環境の洗い出し（`plans/ozen-dev-environment.md`）へ移した | 中 |
| C14 | 書きかけの穴（`_` など）を許し、そこで使える動作と、要る状態を返す | `onion \|> _` → 「`peel`・`cut`（皮をむいた後）…」 | 大 |

### D. 書いた結果を確かめられるようにする

| ID | 内容 | 例 | 大きさ |
| --- | --- | --- | --- |
| C15 | 値ごとの型と状態を一覧で出す（ホバーの代わり） | `onionCut: Onion<peeled, cut: slice 3 mm>` | 小〜中 |
| C16 | 2 つの版のレシピの違いを、文章の差で見せる（AI の変更が手順をどう変えたか） | 「手順 3: 15 分 → 20 分」 | 中 |
| C17 | ファイルを保存するたびに検査する（`--watch`） | — | 小 |

### E. 書き方を 1 つにそろえる

| ID | 内容 | 例 | 大きさ |
| --- | --- | --- | --- |
| C18 | 整形 `ozen fmt`。書き方を 1 通りに決め、AI の出力の差が整形だけで揺れないようにする | 字下げ、並び、`\|>` の改行 | 中〜大 |
| C19 | 書き方の揺れを知らせる（関数呼び出しの形を `\|>` の形に） | `cut(onion, slice, 3 mm)` に情報を出す | 小 |
| C20 | ひな形を作る `ozen new recipe\|menu\|kitchen <名前>` | 料理本のフォルダは `ozen init`（`plans/ozen-dev-setup.md` の D128）が作る。ここに残るのはレシピ・献立・台所の 1 ファイルのひな形だけ | 小 |

### F. エディタと AI の道具

| ID | 内容 | 例 | 大きさ |
| --- | --- | --- | --- |
| C21 | LSP: その場の診断、ホバー（C15）、補完（C12）、直しの提案（C8） | VS Code などで使える | 大 |
| C22 | 構文ハイライト（tree-sitter または TextMate の文法） | GitHub やエディタで色が付く | 中 |
| C23 | Claude Code などの AI に渡す手順書（skill や `CLAUDE.md` のひな形）: 「書いたら `ozen check --format json` を回し、直し方の候補を当て、`ozen render` で文章に戻して確かめる」 | 文書と開発環境の洗い出し（`plans/ozen-dev-environment.md`）へ移した | 小 |
| C24 | MCP サーバー: 検査・文章化・`doc` を AI の道具として出す | — | 中 |
| C25 | 互換の約束を文書にする: 診断のコードと名前、JSON のスキーマ、出力の安定 | AI が頼れる約束 | 小 |

## 5. 優先順位の案

AI との往復は、すでに JSON の診断で回る。足りないのは診断の中身なので、そこから始める。

1. **第 1 弾（小さくて効く）**: C1・C2・C3・C5・C10・C11
   §3 のつまずき 2・3・4・5・7・8 が消える。どれも今の診断の仕組みに足すだけ。
2. **第 2 弾（直しと調べ物）**: C4・C6・C7・C8・C9・C12（C13・C23 は `plans/ozen-dev-environment.md` で扱う）
   人は日本語で読めて、AI は 1 手で直せる。書く前に調べられる。
3. **第 3 弾（書き方とエディタ）**: C15・C18・C19・C22・C21
   C21 の LSP は、C8・C12・C15 が揃うと薄く作れる。
4. **様子を見る**: C14・C16・C17・C20・C24・C25

## 6. 決めること

| ID | 問い | 推奨 | 決定（2026-11-02） |
| --- | --- | --- | --- |
| Q1 | concept §12 で後回しにした LSP・構文ハイライトを、範囲に戻すか | 戻す。ただし第 3 弾にする | 推奨どおり |
| Q2 | `ozen check` の文面の既定の言語 | 英語のまま。`--lang ja` で日本語 | 推奨どおり |
| Q3 | 直し方の候補を自動で当てる範囲 | 1 つに決まる候補だけ。`SAF` の直しは当てない | 推奨どおり |
| Q4 | AI 向けの手引き（C13）の置き場所と形 | `ozen doc --all` で生成し、`docs/` に生成物として置く | `plans/ozen-dev-environment.md` へ移す（EQ3〜EQ5）。2026-11-03 に、手引きは英語だけに狭めた（`plans/ozen-dev-setup.md` の §1） |
| Q5 | MCP サーバー（C24）を作るか | 作らない。CLI と JSON で足りるかを、`ozen init` が書き出す `AGENTS.md`（`plans/ozen-dev-setup.md` の D128）で試してから決める | 未決 |
| Q6 | 整形（C18）と候補（C10）の書き方 | 材料を先に書く書き方。`plans/ozen-cooking-feedback.md` の D106・D107 と同じにする | 未決 |
| Q7 | 実装の順番。利用者のフィードバックの計画は実装済み（2026-11-03）なので、`plans/ozen-dev-setup.md` との順番を決める | `plans/ozen-dev-setup.md` の後。同じ `main` で範囲が重なると、互いの範囲の除外が増えるため | 未決 |

## 7. 他の計画との関係

- `plans/ozen-cooking-feedback.md` は実装済み（2026-11-03）。診断を 3 つ足し（D108）、サンプルと紹介ページを材料が先の形にそろえ（D106）、`.` を正式な書き方にしなかった（D107）。本計画の C10・C18・C19 は、この決定に従う。第 1 弾の診断の候補（C1・C2・C3・C5）は、足した 3 つの診断にも効く。
- `plans/ozen-dev-setup.md`（2026-11-03、英語）は、書き方の参照 `reference/` とその生成器 `tools/refgen/`、`ozen init`、AI への指示の正本 `.rulesync/` を作る。`ozen doc`（C12）と `ozen explain`（C7）は同計画の範囲外で、本計画で決める。生成器と同じ源（`ozen debug definitions`・`core/src/diag_codes.def`・`phrases/en.ftl`。同計画の D126）を読むので、C7・C12 は生成器の読み方に合わせる。
- `plans/ozen-dev-environment.md` の EQ13 により、本ファイルは日本語のまま残すが、本計画から作る実装計画は英語で書く。
- `--format json` の形を変える候補（C3・C8）は、`schemas/result.schema.json` と既存の golden に効く。実装計画では、既存のゴールを前提ゴールとして引用する。

## 8. 次の一歩

1. §6 の Q5〜Q7 を決める。文書と開発環境の構成は `plans/ozen-dev-environment.md` で決める。
2. 第 1 弾を `verifiable-plan` で実装計画にする（`plans/ozen-diagnostics-clarity.md` など）。
