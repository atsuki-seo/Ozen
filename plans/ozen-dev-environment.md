# 読む人ごとの開発環境と文書の構成の洗い出し

- 作成日: 2026-11-02
- 種類: **洗い出し**（実装計画ではない）。ここで候補と問いを決め、決まったものから `verifiable-plan` で実装計画（`plans/` 配下、`G-*` のゴールつき）を作る。
- 要求: 人と AI が Ozen を使って開発するときの、文書の形と構成、それぞれの開発環境の作り方の設計が弱い。AI 向けの文書のルールも刷新したい（2026-11-02、ユーザーがチャットで依頼）。
- 引き継ぎ: `plans/ozen-authoring-support.md`（以下「書きやすさの洗い出し」）から、C13（AI 向けの手引き）・C23（レシピを書く AI への手順書）・Q4（手引きの置き場と形）を移した。Q4 は「手引きは英日の 2 つにする」まで決まっている。
- 確かめた事実: 2026-11-02 に `bb545b8` のファイルを読んで確かめた（§3）。外部の事実は §3 に出典を付けた。
- 番号: 候補は **E1** から、決めることは **EQ1** から付ける。書きやすさの洗い出しの C・Q、既存の計画の D・S とは別。

---

## 1. 目的

Ozen にかかわる人と AI が、迷わずに始められ、同じことを 2 か所に書かずに済む構成にする。

- **入れ方**: 何を入れれば動くか。版が合っているかを確かめられる。
- **書き方の参照**: 1 か所で引ける。古くならない。
- **AI への指示**: 1 つの正本から、使う AI に届く。
- **確かめ方**: 手元で何を回せば正しいと言えるか。

## 2. 読む人と、それぞれに要るもの

| 読む人 | 入れ方 | 書き方の参照 | AI への指示 | 確かめ方 |
| --- | --- | --- | --- | --- |
| **Ozen 本体を開発する人と AI** | `toolchain.txt`（版の一覧）と `scripts/build-core.sh`。準備の手順や環境の定義はない | 各フォルダの README（英語）、`docs/concept.md`（日本語の設計資料）、`plans/` | `CLAUDE.md` だけ。`.claude/` は空 | `scripts/run-golden.sh`、`scripts/goals.sh`。CI と同じものを手元で回す |
| **レシピを書く人と AI** | 配布物がない。自分でビルドするしかない（GitHub Releases なし） | ない。文法は `core/grammar.ebnf`、動作は `stdlib/prelude.ozen`、例は README、意味は `docs/concept.md` に散らばる | ない（旧 C23） | `ozen check`・`ozen render` |
| **Ozen を組み込む人** | 配布物がない。`bindings/` のソースからビルドする | `core/include/ozen.h`、`bindings/README.md`、`schemas/` | ない | 各プラットフォームの golden と digest |

## 3. 今の事実

1. `docs/` には `concept.md`（1661 行）と `prototype-report.md`（117 行）だけがある。どちらも日本語の設計資料と経緯で、`CLAUDE.md` の言語ルールもそう決めている。
2. `docs/concept.md` は、仕様・根拠・未決事項・ロードマップ・AI への依頼（§16）が 1 ファイルに入っている。§16 は「このドキュメントを `docs/concept.md` などに置く」「まず動作シグネチャ表を書き出す」など、もう済んだ指示のまま残っている。
3. `docs/concept.md` は `scripts/goals.sh` で 10 か所、`plans/` で約 75 か所参照されている。動かすと既存のゴールの判定コマンドが変わる。
4. `toolchain.txt` は CI の版の正本として使われている（`ci.yml` が `awk` で `cmake`・`emcc` の版を読む）。`scripts/check-tools.sh` が手元の版と照合する。
5. CI は Linux（ubuntu-24.04）・macOS（macos-26）で動く。iOS は macOS、Android はエミュレータが要るので、1 つのコンテナにはまとまらない。
6. 紹介ページ `site/index.html` は GitHub Pages で公開している。コード例は手で写している（`site/README.md`）。
7. **Claude Code は `AGENTS.md` を読む**: `CLAUDE.md` がなければ `AGENTS.md` を読み、`CLAUDE.md` から `@AGENTS.md` で取り込むこともできる（[公式ドキュメント](https://code.claude.com/docs/en/memory)、2026-11-02 に確認）。
8. **rulesync** は、`.rulesync/` に書いたルールから、Claude Code・Codex CLI・Cursor・Copilot・`AGENTS.md` などの設定を生成する Node.js の道具。ルールのほか、MCP・コマンド・サブエージェント・skill も生成する（[dyoshikawa/rulesync](https://github.com/dyoshikawa/rulesync)、2026-11-02 に確認）。
9. 公開前の検査（`G-REPO-NOREPLY`）は、`CLAUDE.md` の文とメモリの指示で守っている。hook のような仕組みでは止めていない。

## 4. 論点の整理

### 4.1 言語のリポジトリに文書を束ねるべきか

よく知られた言語は、**コンパイラと一緒に変わり、一緒に検査できるもの**をリポジトリに置き、**自分の更新の周期を持つもの**を別に置いている（例: Go は言語仕様を本体のリポジトリに、Web サイトを別のリポジトリに置く。Rust はエラーコードの説明を本体に置き、`rustc --explain` で引ける）。

Ozen に当てはめると:

- **リポジトリに置く**: 書き方の参照、std の一覧、診断の説明、AI への手順書、環境の定義。どれも `core/`・`stdlib/`・`phrases/` と同じ版で変わり、ゴールで古くなっていないかを検査できる。
- **別にしてよい**: 入門書のような読み物、大きくなった Web サイト。今は要らない。

### 4.2 文書の役割

| 役割 | 読む人 | 言語 | 今の置き場 |
| --- | --- | --- | --- |
| 設計と経緯（なぜこう決めたか） | 本体の開発者 | 日本語 | `docs/` |
| 計画 | 本体の開発者（ユーザーがレビュー） | 日本語 | `plans/` |
| 各部分の使い方（ビルド・API・CLI） | 本体の開発者・組み込む人 | 英語 | 各フォルダの README |
| 書き方の参照（言語・std・診断） | レシピを書く人と AI | 英日 | なし |
| AI への指示 | 本体の AI・レシピを書く AI | 英語（＋日本語） | `CLAUDE.md` だけ |
| 紹介 | はじめて見る人 | 日本語 | `site/` |

足りないのは「書き方の参照」と「レシピを書く AI への指示」。`docs/` に足すか、別の置き場にするかを決める（EQ3）。

### 4.3 AI への指示の正本

| 方式 | 良い点 | 弱い点 |
| --- | --- | --- |
| `AGENTS.md` を正本にし、`CLAUDE.md` は `@AGENTS.md` と Claude 固有の分だけ | 多くの AI が読む形。足す道具がない | Claude 固有の skill・hook は別に書く |
| rulesync | skill・サブエージェント・MCP・権限まで、複数の AI にそろえて出せる | Node.js の道具と生成の手順が増える。今は開発者 1 人・道具 1 つ |
| `CLAUDE.md` のまま | 変更がない | Claude 以外の AI が読まない |

## 5. 候補

大きさは目安（小: 数日、中: 1〜2 週間、大: それ以上）。

### A. 文書の置き場と構成

| ID | 内容 | 大きさ |
| --- | --- | --- |
| E1 | 文書の役割（§4.2）と置き場を決め、`CONTRIBUTING.md` か README に「どこに何があるか」を 1 表で書く | 小 |
| E2 | 書き方の参照（英日）: 言語・std・診断。`core/grammar.ebnf`・`stdlib/`・`phrases/`・`core/src/diag_codes.def` から生成し、例は golden で検査する（書きやすさの洗い出しの C7・C12 と同じ源） | 中 |
| E3 | AI 向けの手引き（旧 C13、英日の 2 つ）: 文法の要約、動作の一覧、よくある誤りと直し方を 1 ファイルずつにまとめる。E2 と同じ源から生成する | 中 |
| E4 | 生成した参照と手引きをコミットし、「生成し直しても差がない」をゴールで検査する | 小 |
| E5 | `docs/concept.md` §16 の古い AI への依頼を片付ける | 小 |
| E6 | `CLAUDE.md` の言語ルールを、新しい置き場に合わせて書き直す | 小 |

### B. 本体を開発する人と AI

| ID | 内容 | 大きさ |
| --- | --- | --- |
| E7 | AI への指示の正本を決めて移す（§4.3） | 小 |
| E8 | `.claude/settings.json` の hook: `git push` の前に `G-REPO-NOREPLY` を回し、落ちたら止める | 小 |
| E9 | `.claude/` のプロジェクト用 skill・rules: 例「ゴールの回し方」「golden の足し方」 | 中 |
| E10 | 準備スクリプト（`scripts/setup.sh` など）: `toolchain.txt` の版を入れるか、足りないものを示す。`scripts/check-tools.sh` で確かめる | 中 |
| E11 | 環境の定義を足す（devcontainer・Nix・mise のいずれか）。Linux の分だけになる（§3-5） | 中 |
| E12 | `CONTRIBUTING.md` を、人と AI の作業の入口にする（読む順、回す検査） | 小 |

### C. レシピを書く人と AI

| ID | 内容 | 大きさ |
| --- | --- | --- |
| E13 | GitHub Releases で `ozen` を配る（Linux x86_64・macOS・WebAssembly）。`std` を同梱し、版は `VERSION` | 中 |
| E14 | レシピを書く AI への手順書（旧 C23、英日）: `AGENTS.md` の形のひな形。「書いたら `ozen check --format json` を回し、直し方の候補を当て、`ozen render` で文章に戻して確かめる」 | 小 |
| E15 | E14 を配る手段: `ozen init` がレシピのフォルダに `AGENTS.md` を書き出す、または参照から写す | 小〜中 |
| E16 | 紹介ページから参照と Releases へつなぐ | 小 |

### D. Ozen を組み込む人

| ID | 内容 | 大きさ |
| --- | --- | --- |
| E17 | ライブラリの配布物（WebAssembly の npm パッケージ、XCFramework、Android のライブラリ） | 大 |
| E18 | C API の参照を `core/include/ozen.h` から生成する | 中 |

## 6. 決めること

| ID | 問い | 推奨 | 決定 |
| --- | --- | --- | --- |
| EQ1 | 今回の範囲にする読む人 | 本体を開発する人と、レシピを書く人。組み込む人（E17・E18）は後回し | 未決 |
| EQ2 | リポジトリに置くものと、別にするもの | §4.1 のとおり。参照・手引き・手順書・環境の定義はリポジトリに置き、入門書や大きな Web サイトは今は作らない | 未決 |
| EQ3 | 書き方の参照と AI 向けの手引きの置き場 | `docs/reference/{en,ja}/`。`docs/` は「設計（日本語）」と「参照（英日）」の 2 つの役割を持つ | 未決 |
| EQ4 | 参照と AI 向けの手引きの関係 | 参照を正本にし、手引きは参照から作る 1 ファイルの要約にする。同じことを手で 2 か所に書かない | 未決 |
| EQ5 | 生成した参照をコミットするか | コミットする。GitHub や clone で読め、ゴールで「生成し直しても差がない」を確かめる | 未決 |
| EQ6 | AI への指示の正本 | `AGENTS.md` を正本にし、`CLAUDE.md` は `@AGENTS.md` と Claude 固有の分だけにする。rulesync は入れない | 未決 |
| EQ7 | `.claude/` に何を置くか | `git push` の前の `G-REPO-NOREPLY` の hook（E8）。skill（E9）は様子を見る | 未決 |
| EQ8 | 環境の定義の方式 | `toolchain.txt` を正本にした準備スクリプト（E10）。devcontainer・Nix・mise（E11）は足さない | 未決 |
| EQ9 | `docs/concept.md` §16 の扱い | 本文を消し、「作業の入口は `AGENTS.md` と `plans/`」の 1 行に置き換える | 未決 |
| EQ10 | GitHub Releases での配布を範囲に入れるか | 入れる。Linux x86_64 と WebAssembly から始め、macOS は CI の macOS で作れるなら足す | 未決 |
| EQ11 | レシピを書く AI への手順書の配り方 | `ozen init` が `AGENTS.md` を書き出し、原本は参照に置く | 未決 |
| EQ12 | `CLAUDE.md` の言語ルール | EQ3・EQ6 の決定に合わせて書き直す（参照・手引き・手順書は英日、`AGENTS.md` は英語） | 未決 |

## 7. 他の計画との関係

- 書きやすさの洗い出しの C7（`ozen explain`）・C12（`ozen doc`）は、E2・E3 と同じ源を使う。源の形はどちらか先に作る方で決め、もう一方はそれに従う。
- `plans/ozen-cooking-feedback.md` は紹介ページの例を照合する道具（`scripts/check-site-examples.py`）を足す。E16 で紹介ページを変えるときは、その道具に従う。
- `docs/concept.md` を動かす案は取らない（§3-3）。

## 8. 次の一歩

1. §6 の EQ1〜EQ12 を 1 件ずつ決める。
2. 決まったものから `verifiable-plan` で実装計画を作る。
