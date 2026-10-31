# README を短くし、説明を各フォルダの文書へ移す計画

- 作成日: 2026-11-01
- 参照元: なし（コードの参照元は持たない）。構成は、言語やライブラリの README の通例（rust-lang/rust・golang/go・gleam・serde・tokio など。説明 1 段落、短い例、ビルド、文書へのリンク、ライセンスに絞り、詳しい説明は別のファイルに置く）に倣う
- 実装先: `/home/atsuki/ドキュメント/Ozen`、`main`（D60）。着手点は `7e3e8a5`（記法の改善の計画の最後のわざと壊す確認の記録まで）
- 前提ルール: `CLAUDE.md`（`main` だけで作業する、push の前に `G-REPO-NOREPLY` を回す、`plans/` は日本語、それ以外のコミットする成果物は英語、`docs/` は日本語の設計資料）、`~/.claude/rules/plan-language.md`、`~/.claude/rules/repo-language.md`、`~/.claude/rules/git-commit.md`。既存の計画 `plans/ozen-roadmap.md`（以下「ロードマップ」）と `plans/ozen-syntax-refresh.md`（以下「記法の計画」）の §5 のゴールを前提ゴールとして引用する（§5）

## 0. この計画の読み方

- 本計画は記法の計画の後に続く作業の計画で、番号を引き継ぐ。設計判断は **D57** から、仕様項目は **S637** から付ける（記法の計画の最後は D56・S636）。
- タスクの完了条件は、すべて **コマンドで合否が決まる形** で書く。人による確認はタスクにせず、§8 の引き渡しリストに回す。
- 完了条件は、§5 で列挙する前提ゴールと、本計画 §5 で定義する `G-RD-*` で参照する。判定コマンドは §5 のコードブロックが正。表には書かない（表の中では `|` が `\|` になるため）。
- §2 の D57〜D59 は、計画を書く前の 2026-11-01 にユーザーが決めた。D60〜D66 は、計画を書いた後の 2026-11-01 にユーザーに 1 件ずつ確認して決めた（7 件すべて個別確認。D64 は代替を選んだ）。D67 は、別の作業が README に足した節の扱いとして、同じ日に P0-2 の前に決めた。各節の先頭に「決定:」の行があり、推奨と代替の比較は経緯として残す（P0-1）。
- 実装先の事実は、2026-11-01 に `7e3e8a5` のファイルを読み、`build/core-gcc-release/ozen` で実行して確かめた（§3）。書き終えた後に独立したサブエージェントの監査を 1 回受け、指摘を取り込んだ（§9）。
- §4 の確かめられる主張には仕様項目 ID を付けてある。§5.3 の網羅表が、S 項目ごとにテストか §8 への引き渡し（理由付き）を示す。

---

## 1. 目的とスコープ

**目的**: README（今は 264 行・10 節）を、言語やライブラリの README の通例に合わせて、説明・例 1 つ・ビルド・文書へのリンク・ライセンスだけに絞る。README から外す説明は消さずに、その説明が扱うフォルダの README（英語）と `CONTRIBUTING.md` に移し、README からはリンクだけを張る。設計資料（`docs/concept.md`）と計画（`plans/ozen-roadmap.md`）へのリンクと、それを紹介する文は README から消す。

縮めた後の README の形（例と行数は目安）:

~~~~
# Ozen

Ozen is a language and compiler for cooking procedures. ...（2〜3 文）

<!-- file: soup.ozen -->
```ozen
recipe soup(servings: 2) { ... onion |> peel |> cut(slice, 3 mm) ... }
```

<!-- run: ozen render --kind recipe --lang en soup.ozen -->
```text
# soup
## Ingredients ...
## Steps ...
```

## Build          … scripts/build-core.sh --only core-gcc-release、toolchain.txt へのリンク
## Documentation  … 8 つの文書と紹介ページへのリンク
## License        … ライセンスの表と出力の権利の一文、NOTICE へのリンク
~~~~

### スコープ内

- README の書き直し（D57・D59・D62〜D65）
- 移し先の文書 8 つの新設: `cli/README.md`・`core/README.md`・`stdlib/README.md`・`renderers/README.md`・`bindings/README.md`・`spec/README.md`・`site/README.md`・`CONTRIBUTING.md`（D58・D67）
- 移した例を、移し先でも README の例の検査（`scripts/check-readme-example.sh`）で確かめ続けること
- `scripts/goals.sh` への `G-RD-*` の追加と、`CLAUDE.md` の Plan の節への本計画の追加（D61）

### スコープ外（理由）

- **`docs/concept.md`・`docs/prototype-report.md` の書き換え**: 要求は README の整理だけ。`docs/` は日本語の設計資料で、利用者向けの英語の説明の置き場にはしない（D58）。
- **ロードマップと記法の計画の書き換え**: どちらも完了した記録として残す。README を縛る既存のゴール（`G-REPO-README-LICENSE`・`G-RF-README-EXAMPLE`・`G-RF-README-PIPE`・`G-RF-NO-EMPTY-PARENS`・`G-RF-README-NO-PROTO`）は、そのまま満たす。移し先の文書の検査は `G-RD-*` として新しく足す。
- **`NOTICE`・ライセンスの本文の書き換え**: Credits の節を消しても、同じ内容が既に `NOTICE` にある（§3.3）。
- **README の例の検査スクリプトの改造**: `--readme` で任意のファイルを渡せるので、移し先の文書もそのまま検査できる（§3.2）。
- **コード・テスト・期待値・サンプルの変更**: 文書だけの作業にする（S682）。
- **文章が伝わるか・自然か、GitHub の画面での配置が読みやすいかの確認**: §8 で引き渡す（見出しの数・注釈が見えないこと・フォルダの README の解決は、GitHub の API で機械で確かめる）。

---

## 2. 設計判断

### D57. README に残すコード例 【決定済み】

- **決定:** soup のレシピ 1 つ（パイプを使う）と、`ozen render` で出したレシピの文章を README に残す。段取りの例と束ねる例は `cli/README.md` に、描画の説明は `renderers/README.md` に移す。（2026-11-01、ユーザー確認）
- **推奨**: 上のとおり。約 30 行で、読み手が「プログラムからレシピの文章ができる」ことを一目で分かる。
- 代替: 今の段取りの例（soup・台所・`ozen debug schedule`）を残す。Ozen ならではの機能を見せられるが、台所の定義が要るので約 60 行になる。
- 代替: レシピと `ozen check` の結果だけ。最も短いが、出力が `result ok …` の 1 行で、何ができるかが伝わらない。

### D58. 説明の移し先 【決定済み】

- **決定:** 説明が扱うフォルダの README（`cli/`・`core/`・`stdlib/`・`renderers/`・`bindings/`・`spec/`）と、リポジトリの根の `CONTRIBUTING.md` に、英語で移す。（2026-11-01、ユーザー確認）
- **推奨**: 上のとおり。GitHub でそのフォルダを開くと README が表示され、説明とコードが同じ場所にある。英語の成果物なので `CLAUDE.md` の言語の規則を変えずに済む。
- 代替: `GUIDE.md` と `CONTRIBUTING.md` の 2 つにまとめる。ファイルは少ないが、1 つが長くなり、README を短くした効果が薄れる。
- 代替: `docs/en/` に置く。`CLAUDE.md` の「`docs/` は日本語の設計資料」の規則を書き換える必要がある。

### D59. Ozendate への言及 【決定済み】

- **決定:** README から消す。（2026-11-01、ユーザー確認）
- **推奨**: 消す。Ozendate はこのリポジトリに無いアプリで、README の読み手に要らない。
- 代替: 説明の段落に 1 文だけ残す。

### D60. 作業の場所と範囲の基点 【決定済み】

- **決定:** 推奨のとおり、`main` で作業し、P0-2 で打つタグ `readme-start` から差分を数える。（2026-11-01、ユーザー確認）
- **推奨**: `main` で作業する（`CLAUDE.md`）。P0-2 で `main` にタグ `readme-start` を打ち、本計画の差分の範囲を `RD=readme-start..HEAD` で数える。範囲は、文書だけを変えたことを確かめる `G-RD-SCOPE` が使う。push は、記憶にあるユーザーの指示（Ozen では push の確認は不要）に従い、そのつど確認を取らずに行う。push の前には毎回 `G-REPO-NOREPLY` を回す。
- 代替: タグを打たず、着手点のコミット `7e3e8a5` を直接書く。タグが 1 つ減るが、ロードマップと記法の計画の書き方（`plan-start`・`refresh-start`）と揃わない。

### D61. 計画ファイルとゴールの置き場所 【決定済み】

- **決定:** 推奨のとおり、本ファイルを新しい計画として置き、`scripts/goals.sh` に `G-RD-*` を足し、`CLAUDE.md` の Plan の節に 1 行足す。ロードマップと記法の計画は書き換えない。（2026-11-01、ユーザー確認）
- **推奨**: 本ファイルを新しい計画として置き、ロードマップと記法の計画は書き換えない。`scripts/goals.sh` に `PLAN_RD`・`RD`・`DOCS_RD` と `goal_G_RD_*` を足す。`CLAUDE.md` の Plan の節に、本計画の名前・ゴールの接頭辞・範囲のタグを 1 行で足す（記法の計画の行と同じ形）。
- 代替: 記法の計画に Phase 10 として書き足す。ゴールは 1 か所にまとまるが、完了した計画と進行中の作業の区別がつかなくなる。

### D62. Credits の節 【決定済み】

- **決定:** 推奨のとおり、Credits の節を消し、License の節から `NOTICE` にリンクする。（2026-11-01、ユーザー確認）
- **推奨**: 消す。License の節から `NOTICE` にリンクする。成分表・Fluent・SARIF のスキーマの出典は `NOTICE` に同じ内容があり（§3.3）、README と二重に持つ理由が無い。成分表の出典の文言（日本語）も `NOTICE` と `data/mext-fct-2023/README.md` に残り、`G-REPO-NOTICE` が確かめ続ける。
- 代替: 3 行のまま残す。README を開いただけで出典が見えるが、`NOTICE` と食い違うおそれが残り、README に日本語の文字が残る。

### D63. License の節 【決定済み】

- **決定:** 推奨のとおり、ライセンスの表と出力の権利の一文を残し、2 つのライセンスの説明を、各ライセンスのファイルと `NOTICE` へのリンクを含む 2〜3 文に縮める。（2026-11-01、ユーザー確認）
- **推奨**: ライセンスの表と「Recipes you write in Ozen …」の一文は残し、その後の 2 段落（2 つのライセンスの説明）を、各ライセンスのファイルと `NOTICE` へのリンクを含む 2〜3 文に縮める。表と一文は `G-REPO-README-LICENSE`（ロードマップ S6）が README に求めていて、残せば完了した計画のゴールを書き換えずに済む。フォルダごとにライセンスが違うことは、利用者が最初に知るべき情報でもある。
- 代替: 表を `LICENSING.md` などに移し、README からはリンクだけにする。README はさらに約 10 行短くなるが、`G-REPO-README-LICENSE` とロードマップ S6 を書き換える必要がある。

### D64. README の行数の上限 【決定済み】

- **決定:** 代替のとおり、行数の上限を置かない。README の形は、見出しが 3 つだけであること（`G-RD-HEADINGS`）と各節の中身のゴールで確かめる。これに合わせて、行数の仕様項目（旧 S638）と `G-RD-LENGTH` を外した。（2026-11-01、ユーザー確認）
- **推奨**: 80 行以下（`G-RD-LENGTH`）。見込みは、説明 5 行、例 30 行、Build 8 行、Documentation 10 行、License 20 行で約 75 行。上限を置くと、後で説明を書き足すときに、移し先の文書に書く方へ誘導できる。
- 代替: 上限を置かず、見出しが 3 つだけであること（`G-RD-HEADINGS`）だけを確かめる。後から節の中が長くなっても検出できない。

### D65. Status の節 【決定済み】

- **決定:** 推奨のとおり、Status の節を消し、暫定の値の説明（`provisionalValue`）を `stdlib/README.md` に移す。（2026-11-01、ユーザー確認）
- **推奨**: 消す。版は `VERSION` が持ち、「多くの値は暫定の範囲」という説明は `stdlib/README.md` に移す（`provisionalValue` の語も移す）。「アプリはこのリポジトリに無い」という文は D59 で不要になる。
- 代替: 説明の段落の最後に「Ozen is at version 0.1.0 and still experimental.」の 1 文を残す。読み手に成熟度が伝わるが、版を書き換えるたびに README も直す必要がある。

### D66. 版 【決定済み】

- **決定:** 推奨のとおり、版は `0.1.0` のまま据え置く。（2026-11-01、ユーザー確認）
- **推奨**: `0.1.0` のまま据え置く。文書だけの変更で、言語・コンパイラ・出力が変わらない。
- 代替: `0.1.1` に上げる。版の文字列を持つ期待値（記法の計画 §3 の 13 個の `.out` など）を直す作業が増え、文書だけの変更とする範囲（S682）から外れる。

### D67. Introduction page の節 【決定済み】

- **決定:** Introduction page の節（`site/index.html` と GitHub Pages への公開の説明）は `site/README.md` に移す。README には、紹介ページの URL を Documentation の節に 1 行だけ残す。（2026-11-01、ユーザー確認。計画を書いた後に別の作業がこの節を足したので、P0-2 の前に足した判断）
- **推奨**: 上のとおり。D58 の「説明はそれが扱うフォルダの README に置く」と揃い、`site/` を開くと説明が表示される。紹介ページそのものは利用者向けの入口なので、README からリンクする。
- 代替: README に節のまま残す。見出しが 4 つになり、紹介ページの公開の仕組み（ワークフロー・フォントの読み込み）という開発者向けの説明が README に残る。

---

## 3. 現状の確認結果（2026-11-01、`7e3e8a5`）

### 3.1 README の中身

- 264 行（2026-11-01、`4eeccf9`）。見出しは `Status`・`Vocabulary table`・`Core and CLI`・`Standard library`・`Platforms`・`Renderer`・`Bundles`・`Introduction page`・`Credits`・`License` の 10 。冒頭の段落の後に、`docs/concept.md`・`plans/ozen-roadmap.md`・紹介ページ（`<https://atsuki-seo.github.io/Ozen/>`）へのリンクが 3 行ある。
- 計画を書いた後（`7e3e8a5` の後）に、別の作業の 9 コミット（`08a0c6b`〜`4eeccf9`）が、紹介ページ `site/index.html`（日本語）と公開のワークフロー `.github/workflows/pages.yml` を足した。README には、紹介ページへのリンクの行、`Introduction page` の節（ビルドの段が無いこと、DotGothic16 のフォントだけを Google Fonts から読むこと、`main` への push で版とコミットをフッターに書いて GitHub Pages に公開すること、例と出力は手で写すこと）、ライセンスの表の `site/` が加わった。`CLAUDE.md` には「`site/index.html` は日本語」の行が加わった（D67）。
- 例は 3 つ（`readme: 3 examples`）: 段取り（`soup.ozen`・`kitchen.ozen`・`ozen debug schedule --entry lunch …`）、描画（前の例のファイルを使い回す `ozen render --kind recipe --lang en --entry lunch soup.ozen`）、束ねる（`package.ozen`・`soup.ozen`・`ozen bundle --entry soup`）。
- `Credits` の節にだけ日本語の文字（成分表の出典の文言）がある。

### 3.2 README を縛る検査

| 検査 | 場所 | 確認した事実 |
| --- | --- | --- |
| `G-REPO-README-LICENSE` | `scripts/goals.sh:94` | ライセンスの表の 2 行（コードと表の元、`docs/`・`plans/`・`spec/samples/`）と出力の権利の一文を `grep -cF` で探す |
| `G-RF-README-EXAMPLE` | `scripts/goals.sh:588` | `check-readme-example.sh --readme README.md` の最後の行が `readme: <n> examples, 0 mismatches`（n ≥ 1） |
| `G-RF-README-PIPE` | `scripts/goals.sh:593` | README の ```` ```ozen ```` の塊に `\|>` が 1 つ以上 |
| `G-RF-NO-EMPTY-PARENS` | `scripts/goals.sh:597` | 対象に `README.md` を名指しで含む |
| `G-RF-README-NO-PROTO` | `scripts/goals.sh:584` | `## Evaluator prototype` の見出しが 0 |
| `check-readme-example.sh` | `scripts/check-readme-example.sh` | `--readme FILE` で任意のファイルを検査できる。`<!-- file: 名前 -->` の後の ```` ```ozen ```` の塊をファイルにし、```` ```text ```` の塊を、直前の `<!-- run: … -->` のコマンドの標準出力と比べる。```` ```text ```` の後に来た最初の ```` ```ozen ```` で、ファイルの組を新しくする。```` ```text ```` 以外の言語名の塊（```` ```bash ```` など）と言語名の無い塊は無視する |
| `check-test-names.py --empty-parens` | `scripts/check-test-names.py:11・30` | フォルダを渡すと `.ozen`・`.sh`・`.c` だけを集める。`.md` は名指しで渡したときだけ、```` ```ozen ```` の塊を検査する。よって新しい文書は名指しで渡す必要がある |

### 3.3 Credits と NOTICE

- `NOTICE` に、成分表（日本語の出典の文言を含む）・Fluent（`@fluent/bundle` 0.19.1、Apache-2.0）・SARIF 2.1.0 のスキーマ（OASIS）の 3 項目と、出力の権利の一文が既にある。README の Credits の 3 行は、この写し。

### 3.4 新しい文書を置いても壊れないこと

- `scripts/check-spdx.sh` は `.md` に SPDX の行を求めない。
- `core/gen/embed-std.cmake`・`core/CMakeLists.txt` は `stdlib/` の `*.ozen` だけを埋め込む。`stdlib/README.md` はコンパイラに入らない。
- `scripts/check-selftests.sh` は `check-*`・`run-*` などの名前のファイルだけを集める。`README.md`・`CONTRIBUTING.md` は対象外。
- `renderers/tools/bundle.mjs` が読むのは `renderers/vendor/README.md` で、`renderers/README.md` とは別のファイル。

### 3.5 README に残す例の出力

`build/core-gcc-release/ozen` で、今の README の soup のレシピ（`menu lunch` を除いたもの）に対して `ozen render --kind recipe --lang en soup.ozen` を実行した。`--entry` を省いても `--entry soup` と同じ出力になる（入口が 1 つのため）。

```text
# soup

## Ingredients
- water, 400 ml
- onion, 1
- carrot, 1

## Steps
1. Peel onion, then cut into slices (3 mm).
2. Peel carrot, then cut into rolling wedges (20 mm).
3. Simmer water, onion, carrot over low heat for 15 min.
```

### 3.6 ビルドの手順

- `scripts/build-core.sh --only core-gcc-release` が `build/core-gcc-release/ozen` を作る（`cmake -S core -B build/<構成>`）。`ozen render` は node で `renderers/cli.mjs` を呼ぶ（`cli/src/main.c:1154`）。道具の版は `toolchain.txt` にある。
- `ozen` には `--help` が無い（`cli: unknown option: --help`）。コマンドの一覧は README から `cli/README.md` に移す。

### 3.7 作業ツリーの状態

- 2026-11-01 の時点で、別の作業による未コミットの変更がある: `spec/sources/search-log.yaml`・`stdlib/prelude.ozen`・`stdlib/sources.ozen` と `tests/golden/` の `.out` 7 つ。本計画はこれらに触らない。`ozen` は作業ツリーからビルドするので、例の出力がこの変更に左右されうる（R3）。
- GitHub の API（`gh api -X POST /markdown`、`gh api repos/atsuki-seo/Ozen/readme/<フォルダ>`）は、この環境で使える。旧 README を GFM で描画すると `<h2` が 9 つ、`&lt;!--` が 0 だった。`data/mext-fct-2023` のフォルダの README は `data/mext-fct-2023/README.md` と返った。

---

## 4. 挙動仕様

確かめられる主張は 1 項目 1 主張で、先頭に `**S<n>**` を付ける。以下で「9 つの文書」は `README.md` と §4.2 の 8 つの文書を、「旧 README」はタグ `readme-start` の時点の `README.md` を指す。「〜の節」は、コードの塊の外にある `## 〜` の見出しから次の `## ` の見出しまでを指す。

### 4.1 README（D57・D59・D62〜D65）

- **S637** README のコードの塊の外にある `## ` の見出しは、上から `Build`・`Documentation`・`License` の 3 つだけ（例の出力の塊の `## Ingredients`・`## Steps` は数えない）。
- **S639** README に、`docs/` と `plans/` の文書へのリンク（相対のリンクと `https://github.com/atsuki-seo/Ozen/blob/…` の URL の両方）が無い。
- **S640** README に、`docs/concept.md`・`docs/prototype-report.md`・`plans/ozen-*.md` のファイル名が、リンクでない文字としても無い。
- **S641** 9 つの文書に `Ozendate` の語が無い（D59。Status の節の移し先に紛れ込まないことも含む）。
- **S642** README の冒頭の段落（`# Ozen` の次の段落）は、Ozen を `a language and compiler for cooking procedures` と説明する。
- **S643** 冒頭の段落は、型の 3 つの柱（`dimension types`・`state types`・`linear types`）を挙げる。
- **S644** 冒頭の段落は、コンパイラが IR と段取り（`schedule`）を出すことを述べる。
- **S645** 冒頭の段落は 2〜3 文。
- **S646** README の ```` ```ozen ```` の塊は 1 つだけで、`recipe soup` を宣言する（D57）。
- **S647** 例は `## Build` の見出しより前にある。
- **S648** README の例は、すべての材料の下ごしらえでパイプ `|>` を使う。
- **S649** README の例の出力の塊は、`ozen render --kind recipe --lang en soup.ozen` を実行した出力と一致する（§3.5）。
- **S650** README の例の出力は、旧 README の描画の例（`--entry lunch`）の出力から、見出しの行（`# lunch`）と盛り付けの行（`4. Plate …`）を除き、見出しを `# soup` にしたものと同じ。
- **S651** Build の節は、`scripts/build-core.sh --only core-gcc-release` でビルドする手順を示す。
- **S652** そのコマンドを実行すると、`core-build: 1 configs, 0 failed` で終わる。
- **S653** Build の節は、道具の版の一覧として `toolchain.txt` にリンクする。
- **S654** Build の節は、`ozen render` に `node` が要ることを述べる。
- **S655** Documentation の節は、§4.2 の 8 つの文書すべてにリンクする。
- **S689** Documentation の節は、紹介ページ `https://atsuki-seo.github.io/Ozen/` にリンクする（D67）。
- **S656** License の節に、旧 README のライセンスの表の 6 行が一字一句そのままある（`G-REPO-README-LICENSE` の 2 行を含む。D63）。
- **S657** License の節に、出力の権利の一文（`Recipes you write in Ozen …`）がある。
- **S658** License の節は、`LICENSE-MIT`・`LICENSE-APACHE`・`LICENSE-CC-BY-4.0` にリンクする。
- **S659** License の節は `NOTICE` にリンクする（D62）。
- **S660** `NOTICE` に、Fluent と SARIF のスキーマの項目が残る（README の Credits を消しても出典が失われない。成分表は `G-REPO-NOTICE` が確かめる）。
- **S661** GitHub の Markdown の描画（`gh api /markdown`、GFM）で、README の `<h2>` は 3 つで、`<!-- file: … -->` などの注釈が文字として見えない。

### 4.2 移し先の文書（D58）

内容は旧 README の各節を移したもので、文の言い回しは整えてよいが、事実（コマンド・引数・パス・版）は変えない。

- **S662** 8 つの文書は、どれも 1 行目が `# ` で始まる見出し。
- **S663** `cli/README.md` に、旧 README のコマンドの書式の 6 行（`ozen check [OPTIONS] [ENTRY|FILE] …` から `ozen debug ast|modules|…` まで）が一字一句そのままある。
- **S664** `spec/README.md` に、旧 README の語彙の表の検査のコマンド 5 行（`python3 tools/vocabcheck/vocabcheck.py schema …` から `python3 tools/vocabcheck/sources.py …` まで）が一字一句そのままある。
- **S665** `cli/README.md` に、台所の定義（`-k`）と今日の条件（`--options`、`-s`・`-c`・`-t`）で検査が level 0・level 1・level 2 と深まる説明がある。
- **S666** `cli/README.md` の段取りの例（`soup.ozen`・`kitchen.ozen`・`ozen debug schedule --entry lunch -k kitchen.ozen soup.ozen`）の出力の塊は、実行した出力と一致する。
- **S667** `cli/README.md` の束ねる例（`package.ozen`・`soup.ozen`・`ozen bundle --entry soup`）の出力の塊は、実行した出力と一致する。
- **S668** `cli/README.md` に、段取りの出力の読み方（開始..終了、括弧の中の最も遅い開始 `latest start`、担当、器具）の説明がある。
- **S669** `cli/README.md` に、束ねる仕組み（`std` を除く、完全な名前の inline `module`、ヘッダーの `std` version、`ozen unbundle` は never overwrites）の説明がある。
- **S670** `core/README.md` に、C23 で書いた入出力を持たない本体、公開 API の `core/include/ozen.h` と `recipe_build`、JSON のスキーマ（`schemas/`、draft 2020-12）、`SARIF 2.1.0` の説明がある。
- **S671** `stdlib/README.md` に、パッケージ `std` がコンパイラに embedded であること、`stdlib/units/`、`Kitchen`・`Options` の型、`ozen debug definitions`、`stdlib/sources.ozen`、`phrases/ja.ftl`・`phrases/en.ftl`、暫定の値（`provisionalValue`）の説明がある（D65）。
- **S672** `renderers/README.md` の描画の例の出力の塊は、実行した出力と一致する。
- **S673** `renderers/README.md` に、出力の種類（レシピ、段取りと Gantt 図、shopping list、print の書式を持つ HTML、book、settings form）、no build step で node とブラウザで動くこと、WebView 向けの 1 ファイル（`renderers/tools/bundle.mjs`）、Fluent（`renderers/vendor/fluent-bundle/`）、言い回しの重ね方（`phrases/<lang>.ftl`）と、足りないときに falls back to English の順の説明がある。
- **S674** `bindings/README.md` に、4 つのプラットフォーム（Linux・WebAssembly・iOS・Android）、それぞれのパス（`bindings/linux/`・`bindings/wasm/`・`bindings/swift/`・`bindings/android/`）、Swift・Web Worker・XCFramework・JNI・`scripts/build-wasm.sh`、結果の digest をネイティブのビルドと比べることの説明がある。
- **S675** `spec/README.md` に、`spec/vocab/`、`tools/vocabcheck/`、成分表のデータ（`data/mext-fct-2023/`）と取り出し直し（`tools/fct/extract.py --check --xlsx`）、調査の記録（`spec/sources/search-log.yaml`）、先行研究の対応（`spec/notes/prior-work.md`、four papers）の説明がある。
- **S676** `CONTRIBUTING.md` に、ビルドの 4 構成（`scripts/build-core.sh`、ASan・UBSan）、golden の実行（`scripts/run-golden.sh build/core-gcc-release/ozen tests/golden`）、ゴールの回し方（`scripts/goals.sh`）、CI（`.github/workflows/ci.yml`）の説明がある。
- **S677** `CONTRIBUTING.md` は、`ci/jobs.txt` の CI のジョブ（`linux`・`fuzz`・`wasm`・`ios`・`android`）をすべて挙げる。
- **S688** `site/README.md` に、紹介ページ（`site/index.html`、日本語）、no build step、DotGothic16 のフォント、`.github/workflows/pages.yml` による GitHub Pages への公開と版・コミットのフッター、例と出力を by hand で写すこと、公開先の `https://atsuki-seo.github.io/Ozen/` の説明がある（D67）。

### 4.3 不変条件

- **S678** 旧 README にある用語の一覧（§5 の `G-RD-TERMS` の 33 語）は、すべて 9 つの文書のどれかにある（説明が消えていない）。
- **S679** 9 つの文書の相対リンクの先は、すべて実在し、git が無視するファイル（`build/` など）でない。
- **S680** 9 つの文書の ```` ```ozen ```` の塊に、空の `()` を持つ `recipe`・`menu` の宣言と `make` が無い。
- **S681** 9 つの文書に日本語の文字（漢字・ひらがな・カタカナ）が無い（`CLAUDE.md` の言語の規則）。
- **S682** 本計画の範囲（`RD`）でコミットされたファイルは、9 つの文書・`CLAUDE.md`・`scripts/goals.sh`・本ファイルだけ。
- **S683** タグ `readme-start` がある（D60。無いと S682 の範囲が空になり、何も確かめずに合格するため）。
- **S684** 本計画の範囲で、`scripts/goals.sh` と `CLAUDE.md` から消えた行が無い（足すだけ。既存のゴールを緩めない）。
- **S685** 版は `0.1.0` のまま（D66）。
- **S686** `CLAUDE.md` の Plan の節に、本ファイル・`G-RD-*`・`readme-start` を挙げる行がある（D61）。
- **S687** push の後、GitHub の API が、7 つのフォルダ（`cli`・`core`・`stdlib`・`renderers`・`bindings`・`spec`・`site`）の README としてそれぞれの `README.md` を返す。

### 4.4 足すもの・変えるものの一覧

| 種類 | パス |
| --- | --- |
| 書き直す文書 | `README.md` |
| 新しい文書 | `cli/README.md`・`core/README.md`・`stdlib/README.md`・`renderers/README.md`・`bindings/README.md`・`spec/README.md`・`site/README.md`・`CONTRIBUTING.md` |
| ゴール | `scripts/goals.sh`（`PLAN_RD`・`RD`・`DOCS_RD`・`OZ` と `goal_G_RD_*`、足すだけ） |
| 規則 | `CLAUDE.md`（Plan の節に 1 行、足すだけ） |

### 4.5 旧 README の節の移し先

| 旧 README の節 | 移し先 |
| --- | --- |
| 冒頭の段落 | README（2〜3 文に縮める。型の 3 つの柱と IR・段取りは残し、Ozendate の文を消す） |
| concept・ロードマップへのリンク | 消す |
| 紹介ページへのリンク | README の Documentation の節に URL を 1 行（D67） |
| Status | 消す。暫定の値の説明は `stdlib/README.md`（D65） |
| Vocabulary table | `spec/README.md`（コマンドの塊はそのまま） |
| Core and CLI | コマンドの書式（塊はそのまま）・レベル・段取りの例と読み方は `cli/README.md`。本体の説明・公開 API・スキーマ・SARIF は `core/README.md`。ビルドの 4 構成と golden は `CONTRIBUTING.md` |
| Standard library | `stdlib/README.md` |
| Platforms | `bindings/README.md`。CI のジョブは `CONTRIBUTING.md` |
| Renderer | `renderers/README.md`（描画の例を、`menu` を除いた soup で作り直す） |
| Bundles | `cli/README.md` の `ozen bundle` の節 |
| Introduction page | `site/README.md`（D67） |
| Credits | 消す（D62） |
| License | README に残す（D63） |

---

## 5. 機械検証ゴールの定義

**前提ゴール**（各タスクの完了条件に暗黙に含む）:

- 記法の計画 §5 の「手元で回すもの」すべてと、記法の計画の `G-RF-*` のうち次のもの: `G-RF-README-NO-PROTO`・`G-RF-README-EXAMPLE`・`G-RF-README-PIPE`・`G-RF-NO-EMPTY-PARENS`・`G-RF-CLAUDEMD`。とくに `G-REPO-README-LICENSE`・`G-REPO-NOTICE`・`G-REPO-SPDX`・`G-REPO-CLAUDEMD`・`G-REPO-VERSION`・`G-REPO-SELFTESTS`・`G-CORE-BUILD` は本計画で触るファイルに関わる
- push の前に毎回: `G-REPO-NOREPLY`
- push の後: `G-PLAT-CI`（文書だけの変更なので、CI が落ちないことだけを見る）と、本計画の `G-RD-GH-READMES`

失敗したときは、どのゴールのどの検査が落ちたかを報告する。

- `RD=readme-start..HEAD`、`PLAN_RD=plans/ozen-readme-compact.md`、`OZ=build/core-gcc-release/ozen`。
- `DOCS_RD="README.md CONTRIBUTING.md cli/README.md core/README.md stdlib/README.md renderers/README.md bindings/README.md spec/README.md site/README.md"`。
- `OLD="git show readme-start:README.md"`（旧 README）。
- 0 を期待する数え上げは、終了コードでなく出力の数で判定する。`goals.sh` は `set -o pipefail` なので、文書がまだ無いときに `cat` の失敗でパイプ全体が誤った結果を出さないよう、ファイルを読むのは `grep -s` に直接渡す。
- 判定は GNU grep（`/usr/bin/grep`）の動きを前提にする。複数のファイルを渡した `grep -q` は、どれかで見つかれば、無いファイルがあっても 0 で終わる（2026-11-01 に確かめた）。
- `G-RD-GFM` は README の本文を GitHub の Markdown の API に送る。README は公開する文書なので、送ってよい。
- P0-3 で、下のコードブロックを `scripts/goals.sh` の関数 `goal_G_RD_*` にする。節を切り出す `sec`・コードの塊の外の見出しを並べる処理は、`goals.sh` の中の小さな関数にしてよい。

### 5.1 構造・データ

| ID | 何を判定するか | 合格条件 |
| --- | --- | --- |
| `G-RD-DECISIONS` | D57〜D67 が確定しているか | 決定済み 11、未決 0、決定の行 11 |
| `G-RD-COVERAGE` | S 項目すべてが網羅表に行を持ち、§8 の各項目に理由がある | 差分が空、理由の無い項目 0 |
| `G-RD-PLAN-TESTS` | わざと壊す確認の記録が網羅表の行の数だけある | `mutations: <m> rows, <m> recorded` |
| `G-RD-HEADINGS` | S637 | 見出しの並びが `Build Documentation License ` |
| `G-RD-NO-PLAN-LINKS` | S639・S640 | 2 つの数がどちらも 0 |
| `G-RD-NO-OZENDATE` | S641 | 0 |
| `G-RD-INTRO` | S642〜S645 | 5 つの語の数がすべて 1 以上、文の数が 2〜3 |
| `G-RD-EXAMPLE` | S646・S647・S649 | 塊 1、`recipe soup` 1、`run:` の行 1、`before`、`readme: 1 examples, 0 mismatches` |
| `G-RD-PIPE-ALL` | S648 | パイプの無い `let` の行 0、パイプの数 2 以上 |
| `G-RD-RENDER-SAME` | S650 | 差分が空、出力の塊の行 5 以上 |
| `G-RD-BUILD` | S651・S653・S654 | 4 つの数がすべて 1 以上 |
| `G-RD-BUILD-RUN` | S652 | `core-build: 1 configs, 0 failed` |
| `G-RD-DOC-LINKS` | S655・S689 | リンクの無い文書 0、紹介ページへのリンク 1 以上 |
| `G-RD-LICENSE` | S656〜S659 | 表の行の欠け 0、一文 1、ライセンスのファイルへのリンクの欠け 0、`NOTICE` へのリンク 1 以上 |
| `G-RD-NOTICE` | S660 | 2 つの数がどちらも 1 以上 |
| `G-RD-GFM` | S661 | `<h2` が 3、`&lt;!--` が 0 |
| `G-RD-DOC-TITLES` | S662 | 見出しで始まらない文書 0 |
| `G-RD-FACTS` | S663・S664 | 旧 README の言語名の無い塊の行が 11、欠け 0 |
| `G-RD-DOC-EXAMPLES` | S666・S667・S672 | `cli/README.md` が `readme: 2 examples, 0 mismatches`、`renderers/README.md` が `readme: 1 examples, 0 mismatches` |
| `G-RD-DOC-CONTENT` | S665・S668〜S671・S673〜S676・S688 | 見つからない組 0 |
| `G-RD-CI-JOBS` | S677 | 挙げていないジョブ 0 |
| `G-RD-TERMS` | S678 | 見つからない語 0 |
| `G-RD-LINKS` | S679 | 先の無いリンク 0 |
| `G-RD-NO-EMPTY-PARENS` | S680 | `empty-parens: <n> sources, 0 found, 0 unlisted exceptions` |
| `G-RD-ENGLISH` | S681 | 0 |
| `G-RD-SCOPE` | S682・S683 | `ok`、範囲の外のファイル 0 |
| `G-RD-ADD-ONLY` | S684 | 0 |
| `G-RD-VERSION` | S685 | `0.1.0` |
| `G-RD-CLAUDEMD` | S686 | 1 以上 |
| `G-RD-GH-READMES` | S687（push の後） | 食い違い 0 |
| `G-RD-AUDIT` | P3-1 の監査の記録 | 1 以上 |

```bash
sec() { awk -v h="## $1" '/^```/{f=!f} !f&&$0==h{s=1;next} s&&!f&&/^## /{exit} s' README.md; }   # README の節の中身
# G-RD-DECISIONS
grep -c '^### D[0-9]*\. .*【決定済み】' $PLAN_RD   # → 11
grep -c '^### D[0-9]*\. .*【未決】' $PLAN_RD       # → 0
grep -c '^- \*\*決定:' $PLAN_RD                    # → 11
# G-RD-COVERAGE
diff <(sed -n '/^## 4\./,/^## 5\./p' $PLAN_RD | grep -oE '\*\*S[0-9]+\*\*' | tr -d '*' | sort -u) \
     <(sed -n '/^### 5\.3/,/^## 6\./p' $PLAN_RD | grep -oE '^\| S[0-9]+' | tr -d '| ' | sort -u)   # → 空
sed -n '/^## 8\./,/^## 9\./p' $PLAN_RD | grep '^- ' | grep -v '機械で決められない理由:' | wc -l   # → 0
# G-RD-PLAN-TESTS
scripts/check-plan-tests.sh --mutations $PLAN_RD   # → mutations: <m> rows, <m> recorded
# G-RD-HEADINGS（コードの塊の中の行は数えない）
awk '/^```/{f=!f;next} !f&&/^## /{sub(/^## /,"");printf "%s ",$0}' README.md   # → "Build Documentation License "
# G-RD-NO-PLAN-LINKS
grep -cE '\]\((https://github\.com/atsuki-seo/Ozen/(blob|tree)/[^/)]+/|\./|\.\./)?(docs|plans)/' README.md   # → 0
grep -cE 'docs/concept\.md|docs/prototype-report\.md|plans/ozen-[a-z-]+\.md' README.md                      # → 0
# G-RD-NO-OZENDATE
grep -sh 'Ozendate' $DOCS_RD | wc -l               # → 0
# G-RD-INTRO（冒頭の段落 = 1 行目の後の最初の段落）
intro=$(awk 'NR==1{next} /^$/{if(p)exit; next} {p=1; print}' README.md | tr '\n' ' ')
for t in 'a language and compiler for cooking procedures' 'dimension types' 'state types' 'linear types' 'schedule'; do
  grep -cF -- "$t" <<< "$intro"
done                                               # → 5 行とも 1 以上
grep -oE '[.!?]( |$)' <<< "$intro" | wc -l         # → 2〜3
# G-RD-EXAMPLE
grep -c '^```ozen$' README.md                      # → 1
grep -c '^recipe soup' README.md                   # → 1
grep -cxF '<!-- run: ozen render --kind recipe --lang en soup.ozen -->' README.md   # → 1
[ "$(grep -n '^```ozen$' README.md | head -n 1 | cut -d: -f1)" -lt "$(grep -n '^## Build$' README.md | cut -d: -f1)" ] && echo before   # → before
scripts/check-readme-example.sh --readme README.md --ozen $OZ | tail -n 1   # → readme: 1 examples, 0 mismatches
# G-RD-PIPE-ALL（例の中で、材料から始まる let の行はすべてパイプで書く。simmer などの関数呼び出しの行は除く）
awk '/^```ozen$/{f=1;next} f&&/^```$/{f=0} f' README.md | grep -E '^ *let [A-Za-z]+ = [a-z]+ ' | grep -v '= make ' | grep -vc '|>'   # → 0
awk '/^```ozen$/{f=1;next} f&&/^```$/{f=0} f' README.md | grep -o '|>' | wc -l   # → 2 以上
# G-RD-RENDER-SAME
blk() { awk -v r="$1" '$0==r{s=1;next} s&&/^```text$/{t=1;next} t&&/^```$/{exit} t'; }
diff <($OLD | blk '<!-- run: ozen render --kind recipe --lang en --entry lunch soup.ozen -->' | grep -vE '^(# |4\. )') \
     <(blk '<!-- run: ozen render --kind recipe --lang en soup.ozen -->' < README.md | grep -v '^# ')   # → 空
blk '<!-- run: ozen render --kind recipe --lang en soup.ozen -->' < README.md | wc -l   # → 5 以上
# G-RD-BUILD
sec Build | grep -cF 'scripts/build-core.sh --only core-gcc-release'   # → 1 以上
grep -cF 'core-gcc-release' scripts/build-core.sh                      # → 1 以上（構成の名前が実在する）
sec Build | grep -cF '](toolchain.txt)'                                # → 1 以上
sec Build | grep -cw 'node'                                            # → 1 以上
# G-RD-BUILD-RUN
scripts/build-core.sh --only core-gcc-release | tail -n 1   # → core-build: 1 configs, 0 failed
# G-RD-DOC-LINKS
for p in CONTRIBUTING.md cli/README.md core/README.md stdlib/README.md renderers/README.md bindings/README.md spec/README.md site/README.md; do
  sec Documentation | grep -qF "]($p)" || echo "$p"
done | wc -l                                       # → 0
sec Documentation | grep -cF '](https://atsuki-seo.github.io/Ozen/)'   # → 1 以上
# G-RD-LICENSE
$OLD | grep '^| ' | while IFS= read -r l; do sec License | grep -qxF -- "$l" || echo "$l"; done | wc -l   # → 0（表の見出しと区切りを含む 8 行）
sec License | grep -cF 'Recipes you write in Ozen and the schedules and IR the compiler produces are yours; the Ozen licenses do not apply to them.'   # → 1
for p in LICENSE-MIT LICENSE-APACHE LICENSE-CC-BY-4.0; do sec License | grep -qF "]($p)" || echo "$p"; done | wc -l   # → 0
sec License | grep -cF '](NOTICE)'                 # → 1 以上
# G-RD-NOTICE
grep -c 'Fluent' NOTICE                            # → 1 以上
grep -c 'SARIF' NOTICE                             # → 1 以上
# G-RD-GFM
gh api -X POST /markdown -f mode=gfm -f context=atsuki-seo/Ozen -F text=@README.md > "$tmp/readme.html"
grep -o '<h2' "$tmp/readme.html" | wc -l           # → 3
grep -c '&lt;!--' "$tmp/readme.html"               # → 0（tmp=$(mktemp -d)）
# G-RD-DOC-TITLES
for f in CONTRIBUTING.md cli/README.md core/README.md stdlib/README.md renderers/README.md bindings/README.md spec/README.md site/README.md; do
  head -n 1 "$f" 2>/dev/null | grep -q '^# ' || echo "$f"
done | wc -l                                       # → 0
# G-RD-FACTS（旧 README の言語名の無い塊。1 つ目は spec、2 つ目は cli へ一字一句そのまま）
facts=$($OLD | awk '/^```/{ if(!f){f=1; n++; plain=($0=="```")} else f=0; next} f&&plain{print n"\t"$0}')
wc -l <<< "$facts"                                 # → 11
while IFS=$'\t' read -r n l; do
  f=$([ "$n" = 1 ] && echo spec/README.md || echo cli/README.md)
  grep -sqxF -- "$l" "$f" || echo "$f: $l"
done <<< "$facts" | wc -l                          # → 0
# G-RD-DOC-EXAMPLES
scripts/check-readme-example.sh --readme cli/README.md --ozen $OZ | tail -n 1         # → readme: 2 examples, 0 mismatches
scripts/check-readme-example.sh --readme renderers/README.md --ozen $OZ | tail -n 1   # → readme: 1 examples, 0 mismatches
# G-RD-DOC-CONTENT（ファイルと語の組。見つからない組を数える。P1 の各タスクでは自分のファイルの組だけを数える）
while IFS='|' read -r f t; do grep -sqF -- "$t" "$f" || echo "$f: $t"; done <<'EOF' | wc -l   # → 0
cli/README.md|`-k`
cli/README.md|--options
cli/README.md|`-s`, `-c`, `-t`
cli/README.md|level 0
cli/README.md|level 1
cli/README.md|level 2
cli/README.md|latest start
cli/README.md|inline `module`
cli/README.md|`std` version
cli/README.md|never overwrites
core/README.md|C23
core/README.md|core/include/ozen.h
core/README.md|recipe_build
core/README.md|schemas/
core/README.md|2020-12
core/README.md|SARIF 2.1.0
stdlib/README.md|`std`
stdlib/README.md|embedded
stdlib/README.md|stdlib/units/
stdlib/README.md|`Kitchen`
stdlib/README.md|`Options`
stdlib/README.md|ozen debug definitions
stdlib/README.md|stdlib/sources.ozen
stdlib/README.md|phrases/ja.ftl
stdlib/README.md|phrases/en.ftl
stdlib/README.md|provisionalValue
renderers/README.md|Gantt
renderers/README.md|shopping list
renderers/README.md|print
renderers/README.md|book
renderers/README.md|settings form
renderers/README.md|no build step
renderers/README.md|WebView
renderers/README.md|renderers/tools/bundle.mjs
renderers/README.md|renderers/vendor/fluent-bundle/
renderers/README.md|phrases/<lang>.ftl
renderers/README.md|falls back to English
bindings/README.md|Linux
bindings/README.md|WebAssembly
bindings/README.md|iOS
bindings/README.md|Android
bindings/README.md|bindings/linux/
bindings/README.md|bindings/wasm/
bindings/README.md|bindings/swift/
bindings/README.md|bindings/android/
bindings/README.md|Swift
bindings/README.md|Web Worker
bindings/README.md|XCFramework
bindings/README.md|JNI
bindings/README.md|scripts/build-wasm.sh
bindings/README.md|digest
spec/README.md|spec/vocab/
spec/README.md|tools/vocabcheck/
spec/README.md|data/mext-fct-2023/
spec/README.md|tools/fct/extract.py --check --xlsx
spec/README.md|spec/sources/search-log.yaml
spec/README.md|spec/notes/prior-work.md
spec/README.md|four papers
CONTRIBUTING.md|scripts/build-core.sh
CONTRIBUTING.md|ASan
CONTRIBUTING.md|UBSan
CONTRIBUTING.md|scripts/run-golden.sh build/core-gcc-release/ozen tests/golden
CONTRIBUTING.md|scripts/goals.sh
CONTRIBUTING.md|.github/workflows/ci.yml
site/README.md|site/index.html
site/README.md|no build step
site/README.md|DotGothic16
site/README.md|.github/workflows/pages.yml
site/README.md|GitHub Pages
site/README.md|footer
site/README.md|by hand
site/README.md|https://atsuki-seo.github.io/Ozen/
EOF
# G-RD-CI-JOBS
while IFS= read -r j; do grep -sqw -- "$j" CONTRIBUTING.md || echo "$j"; done < ci/jobs.txt | wc -l   # → 0
# G-RD-TERMS（旧 README の用語。9 つの文書のどれかにあればよい）
for t in 'ozen check' 'ozen build' 'ozen render' 'ozen bundle' 'ozen unbundle' 'ozen debug' '--format' 'sarif' \
         'core/include/ozen.h' 'recipe_build' 'schemas/' 'SARIF 2.1.0' 'scripts/build-core.sh' 'scripts/run-golden.sh' \
         'scripts/build-wasm.sh' 'bindings/wasm/ozen.mjs' 'bindings/wasm/worker.js' 'XCFramework' 'JNI' \
         '.github/workflows/ci.yml' 'renderers/vendor/fluent-bundle/' 'renderers/tools/bundle.mjs' 'phrases/<lang>.ftl' \
         'ozen debug definitions' 'stdlib/sources.ozen' 'provisionalValue' 'spec/sources/search-log.yaml' \
         'spec/notes/prior-work.md' 'tools/fct/extract.py' 'data/mext-fct-2023/' \
         'site/index.html' '.github/workflows/pages.yml' 'https://atsuki-seo.github.io/Ozen/'; do
  grep -sqF -- "$t" $DOCS_RD || echo "$t"
done | wc -l                                       # → 0
# G-RD-LINKS（「](先 "題")」の題は除く。git が無視するファイルへのリンクも数える）
for f in $DOCS_RD; do
  grep -soE '\]\([^)]+\)' "$f" | sed -E 's/^\]\(//; s/\)$//; s/ .*$//; s/#.*$//' | grep -vE '^(https?:|mailto:|$)' |
    while IFS= read -r p; do
      x=$(realpath -m --relative-to=. "$(dirname "$f")/$p")
      { [ -e "$x" ] && ! git check-ignore -q "$x"; } || echo "$f: $p"
    done
done | wc -l                                       # → 0
# G-RD-NO-EMPTY-PARENS
python3 -B scripts/check-test-names.py --empty-parens --exceptions scripts/empty-parens-exceptions.txt $DOCS_RD | tail -n 1
                                                   # → empty-parens: <n> sources, 0 found, 0 unlisted exceptions
# G-RD-ENGLISH
grep -shP '[\p{Han}\p{Hiragana}\p{Katakana}]' $DOCS_RD | wc -l   # → 0
# G-RD-SCOPE（タグが無いと git diff が何も出さずに合格するので、先にタグを確かめる）
git rev-parse -q --verify refs/tags/readme-start >/dev/null && echo ok   # → ok
git diff --no-renames --name-only $RD | grep -vxF -e README.md -e CONTRIBUTING.md -e cli/README.md -e core/README.md \
  -e stdlib/README.md -e renderers/README.md -e bindings/README.md -e spec/README.md -e site/README.md \
  -e CLAUDE.md -e scripts/goals.sh -e plans/ozen-readme-compact.md | wc -l   # → 0
# G-RD-ADD-ONLY
git diff --no-renames $RD -- scripts/goals.sh CLAUDE.md | grep '^-' | grep -vc '^---'   # → 0
# G-RD-VERSION
cat VERSION                                        # → 0.1.0
# G-RD-CLAUDEMD
grep -F 'plans/ozen-readme-compact.md' CLAUDE.md | grep -c 'G-RD-.*readme-start'   # → 1 以上
# G-RD-GH-READMES（push の後）
for d in cli core stdlib renderers bindings spec site; do
  [ "$(gh api "repos/atsuki-seo/Ozen/readme/$d" --jq .path 2>/dev/null)" = "$d/README.md" ] || echo "$d"
done | wc -l                                       # → 0
# G-RD-AUDIT（P3-1）
grep -cE '^- 網羅の監査: 指摘 [0-9]+ 件、取り込み [0-9]+ 件' $PLAN_RD   # → 1 以上
```

`G-RD-SCOPE` は、本ファイルを着手点のタグより後にコミットする場合に備えて本ファイルを許す。`git diff A..B` はコミットだけを比べるので、別の作業の未コミットの変更（§3.7）は数えない。`G-RD-TERMS` の語は、旧 README の各節から、説明が消えたら困る固有のコマンド・パス・名前を拾ったもの。`G-RD-DOC-CONTENT` は、その語がどの文書にあるべきかまで決める。

### 5.2 テスト

本計画のテストは §5.1 のゴールそのもので、新しいテストのスクリプトは作らない。使う検査スクリプト（`check-readme-example.sh`・`check-test-names.py`・`check-plan-tests.sh`）には既に自己テストがある（`G-REPO-SELFTESTS`）。

- 例の期待の出力は、旧 README の期待の出力（記法の計画で手で書いたもの）から写す。新しく作る描画の例（README と `renderers/README.md`）の期待の出力は、旧 README の描画の例から見出しと盛り付けの行を除いて作り、`G-RD-RENDER-SAME` でその作り方を機械で確かめる。実装（`ozen`）の出力を写さない。
- 落ちたゴールは緩めずに原因を調べる。期待の出力と実行の出力が食い違ったら、文書の誤りか実装の変化かを調べ、実装の変化ならユーザーに報告する。

機械で押さえるのは、README の形（見出し・リンク・語・節の中身・GitHub での描画）、例の出力の一致、移し先の文書に事実が一字一句残っていること、文書だけを足すように変えたこと。文章が伝わるか・自然かは §8 に回す。

### 5.3 網羅表

観点のうち「時間と順序」「永続化」は、文書だけの変更なので該当しない。「実際の経路」は、例を `ozen` で実際に実行して比べること（`G-RD-EXAMPLE`・`G-RD-DOC-EXAMPLES`）、README のビルドの手順を実際に実行すること（`G-RD-BUILD-RUN`）、GitHub の API で描画と README の解決を確かめること（`G-RD-GFM`・`G-RD-GH-READMES`）で満たす。

| S 項目 | 観点 | テストまたはゴール | わざと壊す方法 | 主なタスク |
| --- | --- | --- | --- | --- |
| S637 | 正常系 | `G-RD-HEADINGS` | README に `## Credits` の見出しを戻す | P2-1 |
| S637 | 境界・上限 | `G-RD-HEADINGS` | 見出しの判定をコードの塊の中まで数える形（`grep '^## '`）に変え、出力の塊の `## Steps` で落ちることを確かめる | P2-1 |
| S639 | 効かない条件 | `G-RD-NO-PLAN-LINKS` | README に `[docs/concept.md](docs/concept.md)` を戻す | P2-1 |
| S639 | 効かない条件 | `G-RD-NO-PLAN-LINKS` | README に `https://github.com/atsuki-seo/Ozen/tree/main/plans/` へのリンクを足す | P2-1 |
| S640 | 効かない条件 | `G-RD-NO-PLAN-LINKS` | README に、リンクにしない文字で `Design concept: docs/concept.md` を足す | P2-1 |
| S641 | 効かない条件 | `G-RD-NO-OZENDATE` | `stdlib/README.md` に Ozendate の文を足す | P2-1 |
| S642 | 正常系 | `G-RD-INTRO` | 冒頭の段落の `cooking procedures` を `recipes` に変える | P2-1 |
| S643 | 正常系 | `G-RD-INTRO` | 冒頭の段落から `linear types` を消す | P2-1 |
| S644 | 正常系 | `G-RD-INTRO` | 冒頭の段落から段取りの文を消す | P2-1 |
| S645 | 境界・上限 | `G-RD-INTRO` | 冒頭の段落に 4 つ目の文を足す | P2-1 |
| S646 | 正常系 | `G-RD-EXAMPLE` | README に `kitchen.ozen` の ```` ```ozen ```` の塊を足す | P2-1 |
| S647 | 正常系 | `G-RD-EXAMPLE` | 例を `## Build` の節の後ろに動かす | P2-1 |
| S648 | 文書との一致 | `G-RD-PIPE-ALL` | carrot の行だけをパイプを使わない `cut(peel(carrot), rollCut, 20 mm)` の形にする | P2-1 |
| S648 | 文書との一致 | `G-RF-README-PIPE` | 例のすべてのパイプを関数呼び出しの形に変える | P2-1 |
| S649 | 文書との一致 | `G-RD-EXAMPLE` | 期待の出力の `(3 mm)` を `(4 mm)` にする | P2-1 |
| S649 | 文書との一致 | `G-RF-README-EXAMPLE` | 期待の出力から 3 つ目の手順の行を消す | P2-1 |
| S650 | 文書との一致 | `G-RD-RENDER-SAME` | 期待の出力と例の両方の `slice` を `dice` 系に変えて実行と一致させたまま、旧 README との差を作る | P2-1 |
| S651 | 正常系 | `G-RD-BUILD` | Build の節のコマンドを `scripts/build-core.sh` だけにする | P2-1 |
| S652 | 実際の経路 | `G-RD-BUILD-RUN` | `build-core.sh` の構成の一覧から `core-gcc-release` を一時的に外す（確かめた後に戻す） | P2-1 |
| S653 | 正常系 | `G-RD-BUILD` | `toolchain.txt` へのリンクを Documentation の節に動かす | P2-1 |
| S654 | 正常系 | `G-RD-BUILD` | Build の節から node の文を消す | P2-1 |
| S655 | データの完全性 | `G-RD-DOC-LINKS` | Documentation の節から `spec/README.md` へのリンクを消す | P2-1 |
| S689 | 正常系 | `G-RD-DOC-LINKS` | Documentation の節から紹介ページのリンクを消す | P2-1 |
| S656 | データの完全性 | `G-RD-LICENSE` | ライセンスの表から `*/vendor/` の行を消す | P2-1 |
| S656 | データの完全性 | `G-REPO-README-LICENSE` | ライセンスの表から `docs/` の行を消す | P2-1 |
| S657 | 正常系 | `G-RD-LICENSE` | 出力の権利の一文を Build の節に動かす | P2-1 |
| S658 | データの完全性 | `G-RD-LICENSE` | `LICENSE-CC-BY-4.0` へのリンクを消す | P2-1 |
| S659 | 正常系 | `G-RD-LICENSE` | License の節の `NOTICE` へのリンクを消す | P2-1 |
| S660 | 副作用とコスト | `G-RD-NOTICE` | `NOTICE` の Fluent の項目を一時的に消す（確かめた後に戻す） | P2-1 |
| S661 | 実際の経路 | `G-RD-GFM` | 例の前の `<!-- file: soup.ozen -->` の行を、前後に空行の無い段落の中に移して文字として描画させる | P2-1 |
| S662 | データの完全性 | `G-RD-DOC-TITLES` | `core/README.md` の 1 行目の見出しを消す | P1-2 |
| S663 | 文書との一致 | `G-RD-FACTS` | `cli/README.md` の `ozen unbundle FILE [-o DIR]` の行を `ozen unbundle FILE` にする | P1-1 |
| S664 | 文書との一致 | `G-RD-FACTS` | `spec/README.md` の `sample` のコマンドの引数を消す | P1-4 |
| S665 | 正常系 | `G-RD-DOC-CONTENT` | `cli/README.md` のレベルの説明から `level 1` を消す | P1-1 |
| S666 | 文書との一致 | `G-RD-DOC-EXAMPLES` | 段取りの期待の出力の `total 1560` を `total 1561` にする | P1-1 |
| S667 | 文書との一致 | `G-RD-DOC-EXAMPLES` | 束ねる例の期待の出力から `from: kitchenBook "0.1.0"` の行を消す | P1-1 |
| S668 | 正常系 | `G-RD-DOC-CONTENT` | `cli/README.md` から段取りの出力の読み方の文を消す | P1-1 |
| S669 | 正常系 | `G-RD-DOC-CONTENT` | `cli/README.md` から `never overwrites` の文を消す | P1-1 |
| S670 | 正常系 | `G-RD-DOC-CONTENT` | `core/README.md` から `2020-12` を消す | P1-2 |
| S671 | 正常系 | `G-RD-DOC-CONTENT` | `stdlib/README.md` から `phrases/ja.ftl` を消す | P1-2 |
| S672 | 文書との一致 | `G-RD-DOC-EXAMPLES` | 描画の期待の出力の `rolling wedges` を `wedges` にする | P1-3 |
| S673 | 正常系 | `G-RD-DOC-CONTENT` | `renderers/README.md` から `falls back to English` の文を消す | P1-3 |
| S674 | 正常系 | `G-RD-DOC-CONTENT` | `bindings/README.md` から digest の文を消す | P1-4 |
| S675 | 正常系 | `G-RD-DOC-CONTENT` | `spec/README.md` から `four papers` の文を消す | P1-4 |
| S676 | 正常系 | `G-RD-DOC-CONTENT` | `CONTRIBUTING.md` の golden のコマンドから引数を消す | P1-6 |
| S677 | データの完全性 | `G-RD-CI-JOBS` | `CONTRIBUTING.md` から `fuzz` のジョブを消す | P1-6 |
| S688 | 正常系 | `G-RD-DOC-CONTENT` | `site/README.md` から `.github/workflows/pages.yml` の文を消す | P1-5 |
| S678 | データの完全性 | `G-RD-TERMS` | 9 つの文書から `bindings/wasm/worker.js` を消す | P2-1 |
| S679 | データの完全性 | `G-RD-LINKS` | `cli/README.md` に `[x](missing.md)` を足す | P2-1 |
| S679 | 効かない条件 | `G-RD-LINKS` | `CONTRIBUTING.md` に `[build](build/core-gcc-release/ozen)` を足す（実在するが git が無視する先） | P2-1 |
| S680 | 効かない条件 | `G-RD-NO-EMPTY-PARENS` | `cli/README.md` の束ねる例の `recipe tea` を `recipe tea()` にする | P2-1 |
| S681 | 効かない条件 | `G-RD-ENGLISH` | `spec/README.md` に `日本食品標準成分表` を足す | P2-1 |
| S682 | 副作用とコスト | `G-RD-SCOPE` | 作業用のコミットで `samples/` のファイルを 1 つ変える（確かめた後に取り消す） | P3-2 |
| S683 | 前提の固定 | `G-RD-SCOPE` | タグ `readme-start` を一時的に別名に付け替える（確かめた後に戻す） | P3-2 |
| S684 | 副作用とコスト | `G-RD-ADD-ONLY` | 作業用のコミットで `goal_G_REPO_README_LICENSE` の 1 行を消す（確かめた後に取り消す） | P3-2 |
| S685 | 副作用とコスト | `G-RD-VERSION` | `VERSION` を `0.1.1` にする（確かめた後に戻す） | P3-2 |
| S686 | 正常系 | `G-RD-CLAUDEMD` | `CLAUDE.md` の本計画の行から `readme-start` を消す | P0-3 |
| S687 | 実際の経路 | `G-RD-GH-READMES` | — | P3-2 |

S650 の壊し方の補足: 例の `cut(slice, 3 mm)` を `cut(dice, 3 mm)` などに変え、期待の出力も実行の出力に合わせて直すと、`G-RD-EXAMPLE` は合格のまま、旧 README との比較だけが落ちる。

S687 は push した後の GitHub の状態を見るので、わざと壊す確認をしない（壊すには壊した文書を push する必要がある）。

網羅の検査（P0-3 の後と P3-1 で実行する。`G-RD-COVERAGE`）。

独立した監査（サブエージェント）の指摘のうち、取り込まなかったもの・一部だけ取り込んだものと理由:

- 文書の中のリンクのアンカー（`#節`）が実在するかの検査 — 移し先の文書はアンカーへのリンクをほとんど持たない見込みで、見出しから GitHub のアンカーを作る規則を再現する手間に見合わない。アンカーを使ったら P3-1 で検査を足す
- §3.4 の事実（`stdlib/README.md` がコンパイラに埋め込まれないことなど）を固定するゴール — 崩れたら `G-CORE-BUILD`・`G-CORE-GOLDEN`・`G-REPO-SPDX` が落ちるので、間接に検出できる
- 参照形式のリンク（`[x]: docs/…`）と `<https://…>` の形のリンクの検査 — ファイル名を文字として探す S640 の検査が、リンクの形によらず検出する

---

## 6. フェーズとタスク

表記:

- **完了条件** は §5 のゴール ID か、合否が一意なコマンド。§5 の「手元で回す」前提ゴールは各タスクに暗黙で含まれる。push の後の前提ゴールは、push するゲートの完了条件にだけ含める。
- 状態の報告の形: 「README・移し先の文書: `G-RD-*` 合格。前提ゴール: 合格。GitHub: push の後に `G-PLAT-CI`・`G-RD-GH-READMES` を確認」。
- コミットは論理単位（文書 1 つ、またはゴールの追加 1 回）。push は確認を取らずに行う（D60）が、その前に毎回 `G-REPO-NOREPLY` を回し、push した範囲を報告する。
- 各タスクの「わざと壊す確認」は、網羅表の方法で壊すとそのゴールが落ち、戻すと合格することを確かめ、§9 に `- わざと壊す確認: ` の行で記録する。
- 移し先の文書を先に作り（Phase 1）、README を縮めるのは後（Phase 2）。途中で説明がどこにも無い状態を作らない。
- 別の作業の未コミットの変更（§3.7）はコミットに含めない。`git add` はファイルを名指しする。

### Phase 0 — 決定と準備（文書の変更なし）

| ID | 内容 | 完了条件 |
| --- | --- | --- |
| P0-1 | §2 の D60〜D66 をユーザーと確定し、本ファイルに書き込む | `G-RD-DECISIONS` |
| P0-2 | 別の作業の未コミットの変更が片付いていることを確かめる。片付いていなければユーザーに確認する（§3.7・R3）。本ファイルをコミットして push し、`main` にタグ `readme-start` を打つ（D60）。基準値（前提ゴールの合否、`$OLD \| wc -l`、`check-readme-example.sh --readme README.md` の例の数）を §9 に記録する | `git status --porcelain -- stdlib tests core cli renderers spec \| wc -l` が 0、`git rev-parse --verify refs/tags/readme-start` が成功、§5 の手元の前提ゴールがすべて合格 |
| P0-3 | `scripts/goals.sh` に `PLAN_RD`・`RD`・`DOCS_RD`・`OZ`・`OLD` と `goal_G_RD_*` を足す。`CLAUDE.md` の Plan の節に本ファイルの行を足す（D61） | `scripts/goals.sh --list \| grep -c '^G-RD-'` が 31、`G-RD-CLAUDEMD`・`G-RD-COVERAGE`・`G-RD-ADD-ONLY`・`G-REPO-CLAUDEMD`・`G-REPO-SELFTESTS` が合格。S686 のわざと壊す確認 |

P0-3 の時点では文書をまだ変えていないので、README の形と移し先の文書を見るゴールの多くが落ちる（`G-RD-TERMS` は語が旧 README にあるので合格する）。§9 に、P0-3 の時点で落ちたゴールの一覧を記録する（ゴールが何かを確かめている証拠になる）。

### Phase 1 — 移し先の文書を作る（D58）

各タスクの `G-RD-DOC-CONTENT` は、§5 のコマンドを自分のファイルの組だけに絞った数が 0 であることを指す。

| ID | 内容 | 完了条件 |
| --- | --- | --- |
| P1-1 | `cli/README.md`: コマンドの書式（旧 README の塊をそのまま）、台所と今日の条件によるレベル、段取りの例と読み方、束ねる仕組みと例 | `G-RD-FACTS` の cli の行、`G-RD-DOC-EXAMPLES` の 1 行目、`G-RD-DOC-CONTENT` の cli の組、`G-RD-DOC-TITLES` の cli、`G-RD-LINKS`・`G-RD-ENGLISH`・`G-RD-NO-EMPTY-PARENS`。S663・S665〜S669 のわざと壊す確認 |
| P1-2 | `core/README.md`（本体・公開 API・スキーマ・SARIF）と `stdlib/README.md`（`std` の中身・定義の一覧・出どころ・言い回し・暫定の値） | `G-RD-DOC-CONTENT` の 2 つのファイルの組、`G-RD-DOC-TITLES` の 2 つ、`G-RD-LINKS`・`G-RD-ENGLISH`。S662・S670・S671 のわざと壊す確認 |
| P1-3 | `renderers/README.md`: 出力の種類、動く場所、`bundle.mjs`、Fluent、言い回しの重ね方、`menu` を除いた soup の描画の例 | `G-RD-DOC-EXAMPLES` の 2 行目、`G-RD-DOC-CONTENT` の renderers の組、`G-RD-DOC-TITLES` の renderers、`G-RD-LINKS`・`G-RD-ENGLISH`・`G-RD-NO-EMPTY-PARENS`。S672・S673 のわざと壊す確認 |
| P1-4 | `bindings/README.md`（4 つのプラットフォーム）と `spec/README.md`（語彙の表の検査の塊をそのまま、成分表、調査の記録、先行研究の対応） | `G-RD-FACTS` の spec の行、`G-RD-DOC-CONTENT` の 2 つのファイルの組、`G-RD-DOC-TITLES` の 2 つ、`G-RD-LINKS`・`G-RD-ENGLISH`。S664・S674・S675 のわざと壊す確認 |
| P1-5 | `site/README.md`: 紹介ページ、ビルドの段が無いこと、フォント、GitHub Pages への公開、例を手で写すこと（D67） | `G-RD-DOC-CONTENT` の site の組、`G-RD-DOC-TITLES` の site、`G-RD-LINKS`・`G-RD-ENGLISH`。S688 のわざと壊す確認 |
| P1-6 | `CONTRIBUTING.md`（ビルドの 4 構成、golden、ゴール、CI のジョブ） | `G-RD-DOC-CONTENT`（全組）、`G-RD-DOC-TITLES`（全文書）、`G-RD-FACTS`・`G-RD-CI-JOBS`・`G-RD-LINKS`・`G-RD-ENGLISH`。S676・S677 のわざと壊す確認 |

### Phase 2 — README を縮める（D57・D59・D62〜D65）

| ID | 内容 | 完了条件 |
| --- | --- | --- |
| P2-1 | README を §1 の形に書き直す | `G-RD-HEADINGS`・`G-RD-NO-PLAN-LINKS`・`G-RD-NO-OZENDATE`・`G-RD-INTRO`・`G-RD-EXAMPLE`・`G-RD-PIPE-ALL`・`G-RD-RENDER-SAME`・`G-RD-BUILD`・`G-RD-BUILD-RUN`・`G-RD-DOC-LINKS`・`G-RD-LICENSE`・`G-RD-NOTICE`・`G-RD-GFM`・`G-RD-TERMS`・`G-RD-LINKS`・`G-RD-NO-EMPTY-PARENS`・`G-RD-ENGLISH`、前提の `G-REPO-README-LICENSE`・`G-RF-README-EXAMPLE`・`G-RF-README-PIPE`・`G-RF-README-NO-PROTO`・`G-RF-NO-EMPTY-PARENS`。S637・S639〜S661・S678〜S681・S689 のわざと壊す確認 |

### Phase 3 — 仕上げ

| ID | 内容 | 完了条件 |
| --- | --- | --- |
| P3-1 | 網羅の監査: §9 で増えた事実を網羅表に足す。§8 の各項目を、書いた後の知識でもう一度「コマンドで決められるか」で判定する。独立したサブエージェントに、§1・§4・§8・網羅表と 9 つの文書と旧 README を読ませ、ゴールの無い主張と、旧 README から消えた説明を列挙させる。指摘は足すか、§5.3 の末尾に足さない理由を書く | `G-RD-COVERAGE`・`G-RD-PLAN-TESTS`・`G-RD-AUDIT` |
| P3-2 | 最終ゲート。S682〜S685 のわざと壊す確認をする。`G-REPO-NOREPLY` を回して push し、CI と GitHub の README の解決を確かめる。§8 をユーザーに渡す | `G-RD-SCOPE`・`G-RD-ADD-ONLY`・`G-RD-VERSION` を含む §5 の全ゴールと手元の前提ゴール、push の後の `G-PLAT-CI`・`G-RD-GH-READMES` |

---

## 7. フェーズ間の依存

- P0-1 → P0-2 → P0-3 の順。P0-3 のゴールが無いと、Phase 1 の完了条件を判定できない。P0-2 は、別の作業の未コミットの変更が片付くのを待つことがある。
- P1-1〜P1-6 は互いに独立（触るファイルが重ならない）。ただし `scripts/goals.sh` は P0-3 で書き終えておき、Phase 1 では触らない。P1-6 の完了条件は全文書を見るので、P1-6 は最後に回す。
- P2-1 は Phase 1 のすべての後（`G-RD-TERMS` は、README から消した語が移し先にあることで合格する）。
- P3-1 → P3-2 の順。

---

## 8. ユーザーに引き渡す確認

Claude は機械的なゴールまでを担当する。以下はユーザーが確認する。各項目には、コマンドで決められない理由を付ける。理由が書けない項目は §5.3 でテストにする。

- README の冒頭の 2〜3 文が、初めて見た人に Ozen が何かを伝えるか（文の数と、型の 3 つの柱・段取りの語があることは `G-RD-INTRO` で確かめる） — 機械で決められない理由: 説明が伝わるかは読み手の判断
- 移し先の 8 つの文書の英語の文章が自然か（事実が一字一句残っていることは `G-RD-FACTS`・`G-RD-DOC-CONTENT`・`G-RD-TERMS` で確かめる） — 機械で決められない理由: 文章の自然さは人の判断
- GitHub のリポジトリの画面で、README の例・表・リンクの見た目の配置が読みやすいか（見出しの数・注釈が見えないこと・フォルダの README の解決は `G-RD-GFM`・`G-RD-GH-READMES` で確かめる） — 機械で決められない理由: 読みやすい配置かは人の判断

---

## 9. 着手時に確認して記録する事項

（各タスクで確認した結果を、ここに日付付きで追記する）

- 計画を書いた後の独立した監査（2026-11-01）: 指摘 28 件。26 件をそのまま、2 件を一部取り込んだ（取り込まなかった部分と理由は §5.3 の末尾）。主な変更は、見出しの判定をコードの塊の外に限ったこと、`G-RD-SCOPE` にタグの確認を足したこと、`pipefail` で誤る `cat … | grep` をやめたこと、旧 README のコマンドの塊とライセンスの表を一字一句比べる `G-RD-FACTS`・`G-RD-LICENSE` を足したこと、GitHub の API で描画とフォルダの README を確かめる `G-RD-GFM`・`G-RD-GH-READMES` を §8 から移したこと、移し先の語の一覧を広げたこと
- 着手点の基準値（P0-2）: 手元の前提ゴールの合否、旧 README の行数（2026-11-01 の調査では 254）と例の数（3）
- P0-3 の時点で落ちた `G-RD-*` の一覧
- わざと壊す確認の記録（各タスク）: `- わざと壊す確認: S<n>・<ゴール>（<タスク>、<日付>）。<壊し方> で落ちた。戻すと合格した` の形で書く
- 網羅の監査（P3-1）: `- 網羅の監査: 指摘 <k> 件、取り込み <m> 件` の形で書く

---

## 10. リスク

- **R1. 説明が移し先に移らずに消える**: README を縮めるときに、どの文書にも無い説明が出うる。`G-RD-TERMS`・`G-RD-DOC-CONTENT` で旧 README の固有の語を、`G-RD-FACTS`・`G-RD-LICENSE` でコマンドの塊と表の行を一字一句確かめ、P3-1 の監査で語の一覧に無い説明も探させる。
- **R2. 例の出力が実装の変化で変わる**: 今後 `std` や描画が変わると、移し先の例も落ちる。`G-RD-EXAMPLE`・`G-RD-DOC-EXAMPLES` で検出する。直すのは文書の側か実装の側かを、そのときの計画で決める。
- **R3. 別の作業と同じ `main`・同じ作業ツリーを使う**: 2026-11-01 の時点で、`stdlib/`・`tests/golden/`・`spec/` に別の作業の未コミットの変更がある（§3.7）。`ozen` はこの作業ツリーからビルドするので、例の出力がその変更に左右されうる。P0-2 で片付いていることを確かめ、片付いていなければユーザーに確認する。各フェーズの最初とコミットの前に `git log` と `git status` を見る。`G-RD-SCOPE` は範囲に別の作業のコミットが入ると落ちるので、落ちたら原因が自分の変更かを確かめてからユーザーに報告する。
- **R4. README の節へのリンクが外で使われている**: 旧 README の見出し（`#bundles` など）を指すリンクが、リポジトリの外にあるかもしれない。リポジトリの中には無い（`git grep 'README.md#'` が 0 件、2026-11-01）。外のリンクは確かめられないが、公開して間もないリポジトリで、影響は小さいと見込む。
- **R5. GitHub の API に頼るゴール**: `G-RD-GFM`・`G-RD-GH-READMES` は `gh` の認証とネットワークが要る。使えないときは落ちたとして扱い、理由を報告する（合格にしない）。
