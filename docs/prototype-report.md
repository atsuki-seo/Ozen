# 評価器の試作の報告

concept §9 の「試作で確認すること」の 3 項目について、`proto/` の試作（C23 の部分集合、評価器のレベル 3 の部分: `if`・`for`・`repeat`・`macro` と `quote`・燃料制）で測った結果をまとめる。数値は `build/results/`・`build/fuzz/summary.txt`・`proto/defects.tsv` から書き、`proto/scripts/check-report.py` が照合する。

試作の範囲: 字句解析と、誤りから立ち直るパーサー、評価器（`let`・`if`・`for`・`repeat`・名前付き引数・注釈・入口と `params`）、展開の印による衛生的なマクロ、燃料・深さ・生成ノードの上限、自前の 128 bit 整数による有理数と区間、31 種の診断（展開履歴つき）。本番のコア `core/` は D12 のとおり D で作り直し、この試作からは下の §4 に挙げる部品をテストごと移す。

## 1. 書き心地とバグ量

### 行数

| ファイルの組 | ファイル | 行数 |
| --- | --- | --- |
| コア | `proto/src/*.c` | 3,082 |
| コアの内部ヘッダーと診断の登録簿 | `proto/src/*.h` `proto/src/*.def` | 301 |
| 公開ヘッダー | `proto/include/*.h` | 38 |
| ドライバー | `proto/driver/*.c` | 175 |
| 単体テスト | `proto/tests/unit/*` | 345 |
| 確保の失敗のテスト | `proto/tests/oom/*.c` | 147 |
| 有理数のオラクル | `proto/tests/oracle/*.py` | 159 |
| golden の入力 | `proto/tests/golden/**/*.ozen` | 2,589 |
| golden の期待値 | `proto/tests/golden/**/*.out` | 443 |
| ファズの入口 | `proto/fuzz/*.c` | 28 |
| WASM の橋渡しと Worker | `proto/wasm/*` | 133 |
| 検査とビルドのスクリプト | `proto/scripts/*` | 1,283 |
| ビルドの設定 | `proto/CMakeLists.txt` | 89 |

コアの内訳はパーサー（`parse.c`）1,391 行、評価器（`eval.c`）1,084 行で、この 2 つで 8 割を占める。128 bit の整数（`u128.c`）と有理数・区間（`rational.c`）は合わせて 208 行。

### 欠陥

`proto/defects.tsv` に、見つけた欠陥を検出した手段・直したコミットとともに記録した。

| 検出した手段 | 件数 |
| --- | --- |
| unit | 1 |
| golden | 3 |
| measurement | 1 |
| goal | 1 |
| 合計 | 6 |

- unit の 1 件はテストの期待値の誤り（`(2^63-2)/2 + 1/2` を整数と思い込んでいた）で、コードの誤りではない
- golden の 3 件は、診断の引数が空になる誤り、`NAM:rebind` の後に新しい値を束縛しない誤り、確保に一度失敗するとアリーナがその後の要求をすべて断り、`MAC:outOfMemory` を報告できない誤り
- measurement の 1 件は、範囲ごとに束縛をアリーナに確保し直していたため、`repeat 1000000` で 420 MB を使っていたもの（束縛と引数のスタックを使い回して 1.5 MB にした）
- goal の 1 件は、コアが `strlen` を呼び、許可した C ライブラリの記号の外に出ていたもの（`G-PROTO-PURE` が検出）

このほか、golden の期待値の手計算の誤りが 4 件あった（入れ子の深さの入力の括弧の数、診断の位置、範囲の終わり）。期待値を実装より先にコミットする運用（D20）のため、どれも実装と食い違った時点で見つかった。

### 書き心地

- アリーナ 1 つと添字の参照、タグ付きの union と `switch`、X マクロの診断の登録簿（`diag_codes.def`）は、規律として守りやすかった。登録簿と golden・名前の一覧の照合（`check-diag-registry.sh`）で、診断の追加の漏れは機械で捕まえられた
- 書きにくかったのは、出力の組み立て（`ok = ok && ozpi_buf_puts(...)` の連鎖が `api.c` だけで 15 か所）と、`__int128` を使わない 128 bit の演算（D15）。後者は有理数のオラクル（`fractions.Fraction` との 20,000 件の照合）が無ければ、正しさを確かめにくい
- コンパイラが慣用の書き方を C ライブラリの呼び出しに置き換える（gcc の `strlen`、clang の `bcmp`）ため、純粋性の検査の許可一覧に 2 つの記号を足した。コアのソースからは呼ばない
- ASan・UBSan の構成は、わざと壊す確認で範囲外の読み出しとスタックの溢れを報告し、単体テスト・golden・確保の失敗のテストはすべて通った

## 2. 環境ごとの結果

golden は 109 件。digest は golden と、ファズのコーパス 300 件の出力（終了コードを含む）をまとめた sha256。

| 環境 | golden | digest |
| --- | --- | --- |
| `gcc-debug-san` | 109/109 | — |
| `gcc-release` | 109/109 | — |
| `clang-debug-san` | 109/109 | — |
| `clang-release` | 109/109 | — |
| `native` | — | `9a6479fb12a33b3a666fd137aba318daefe5b05fa3b80034dac4f40843a84de6` |
| `wasm` | 109/109 | `9a6479fb12a33b3a666fd137aba318daefe5b05fa3b80034dac4f40843a84de6` |
| `ios` | 109/109 | `9a6479fb12a33b3a666fd137aba318daefe5b05fa3b80034dac4f40843a84de6` |

- `native` は gcc の Release のドライバーで取った digest。4 つの構成（gcc・clang × Debug・Release）の digest が 2 回ずつ一致することは `G-PROTO-DETERMINISM` で確かめた
- WASM は Emscripten 6.0.13 で作り node で実行した。有理数のオラクル 2,000 件も不一致 0
- JS の橋渡し（`bridge-test.mjs`）とブラウザの Web Worker（ヘッドレスの Chrome 154）で、日本語と絵文字を含む 4 つの入力の結果がネイティブと一致した
- iOS は GitHub Actions の macOS 26 のランナー（Xcode 26.6）で、端末用とシミュレーター用の静的ライブラリを `OzenProto.xcframework` にまとめ、arm64 のシミュレーター（iPhone 17 Pro）で golden と digest を取った（ランナーの image は `macos26 20261013.0424.1`、1 回の実行はおよそ 9 分）。CMake は iOS 向けの実行ファイルを既定で `.app` にするので `CMAKE_MACOSX_BUNDLE` を切り、`simctl spawn` の起動に 10 秒を超えることがあるので 1 回の制限を 120 秒にした

## 3. ファズ

libFuzzer（clang 17、ASan・UBSan 付き、1 入力 5 秒、入力の長さ 65,536 バイトまで、構文の辞書つき）で 600 秒回した結果:

`fuzz: 746382 execs in 601 s, 0 crashes, 0 leaks, 0 timeouts`

- ゲート C（P6-4）のときの実行で、毎秒およそ 1,241 回、カバレッジ 1,184（P5-1 の最初の実行は 653,862 回・毎秒 1,087 回・カバレッジ 1,177）。ライブラリを `-fsanitize=fuzzer-no-link` で作り直す前は、カバレッジが 3 から増えなかった
- P5-1 で育ったコーパス 1,170 件を 300 件に絞って `proto/tests/corpus/` に置き、digest の入力にした
- 文字列の終わりの検査を外すと最初の入力で落ちることを確かめた（わざと壊す確認）
- 600 秒で見つかった問題は 0 件で、管理できる範囲に収まっている。ただし 600 秒は短く、本番のコアではパーサーと展開器を対象に CI で回し続ける（concept §10）

## 4. ユーザーが決めること

### concept §9 の 3 項目の判断

1. 書き心地とバグ量: コア 3,082 行で欠陥 6 件（うち 1 件はテストの誤り）。どれも golden・単体テスト・ゴールの検査・測定で見つかり、ファズとサニタイザーが見つけたものは無い
2. iOS と WASM で同じ結果が出るか: ネイティブ・WASM・iOS シミュレーターの digest が一致し、golden はどの環境でも 109 件すべて通った
3. ファズで出る問題が管理できる範囲か: 600 秒で 0 件

これを受けて、コアを C23 の部分集合（D13）のまま D に進めてよいかを決める。

### 本番に持ち込む部品（D12）

持ち込むときは、同じテストごと `core/` に移す（P7-0）。

- 128 bit の整数と有理数・区間（`u128.c`・`rational.c`）と、単体テスト `c/u128_*`・`c/rational_*`、有理数のオラクル
- UTF-8 の検査（`ozpi_check_utf8`）と、golden の `syntax/utf8-*`
- ASan の毒の余白つきのアリーナ（`arena.c`）と `c/arena_*`
- X マクロの診断の登録簿と `check-diag-registry.sh`（コードは追記だけ）
- ドライバーの終了コードの決まり（0・1・2）と `test-driver.sh`
- golden のランナーと digest（`run-golden.sh`・`digest.sh`、`OZP_RUN_TIMEOUT`）、確保の失敗の注入（`ozp_oom_test`）
- 純粋性・大域変数・警告の検査（`check-pure.sh`・`check-noglobal.py`・`check-flags.py`・`check-compilers.sh`）
- ファズの入口と辞書・コーパス、WASM の橋渡しと Worker の検査、iOS のワークフロー

### 評価器の方式で直すべき点

- 束縛と引数は、最初からコンテキストが持つ使い回しのスタックに置く（範囲ごとの確保はしない）
- 確保の失敗は「その要求が失敗した」ことと「アリーナが使えない」ことを分け、出力の組み立てに必要な分を失敗の後も確保できるようにする
- 出力のバッファーに失敗を覚えさせ、最後に 1 度だけ確かめる形にして、`ok = ok && ...` の連鎖をなくす
- 名前を文字列のまま比べている（`eval.c` に比較が 11 か所）。concept §10 のとおり文字列をインターンして整数で比べる
- 展開の印による衛生は golden の範囲で期待どおりに動いたので、本番でも同じ方式にする
- 数値の golden の期待値は、手計算ではなくオラクル（`fractions.Fraction`）で作るか照合する
