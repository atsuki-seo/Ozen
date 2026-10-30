# Ozen の記法の改善・不具合の修正・テストの書き直し・試作の削除の実装計画

- 作成日: 2026-10-28
- 参照元: なし（コードの参照元は持たない）。設計の根拠は `docs/concept.md` と、2026-10-27〜28 の会話でユーザーと決めた方針（パイプ演算子を足す、再束縛は採らない、引数の無いレシピの `()` を省く、本番のテストを材料と量まで実際の料理に近づける、計画を先に書く、試作を消す）
- 実装先: `/home/atsuki/ドキュメント/Ozen`、`main`（D40）。着手点は `09e05de`（ロードマップの P16-4 の記録まで。push していない 1 コミットがある）
- 前提ルール: `CLAUDE.md`（`main` だけで作業する、push の前に `G-REPO-NOREPLY` を回す、`plans/` は日本語、コミットする成果物は英語）、`~/.claude/rules/plan-language.md`、`~/.claude/rules/repo-language.md`、`~/.claude/rules/git-commit.md`、`~/.claude/rules/comment-style.md`。既存の計画 `plans/ozen-roadmap.md`（以下「ロードマップ」）の §5 のゴールを前提ゴールとして引用する（§5）

## 0. この計画の読み方

- 本計画は、ロードマップ（§13-1〜§13-12、P16-4 まで完了）の後に続く作業の計画で、ロードマップの番号を引き継ぐ。設計判断は **D40** から、仕様項目は **S479** から付ける（ロードマップの最後は D39・S478）。ロードマップの番号を指すときは「ロードマップ D12」「ロードマップ S85」と書く。
- タスクの完了条件は、すべて **コマンドで合否が決まる形** で書く。人による確認はタスクにせず、§8 の引き渡しリストに回す。
- 完了条件は、§5 で列挙する前提ゴールと、本計画 §5 で定義する `G-RF-*` で参照する。判定コマンドは §5 のコードブロックが正。表には書かない（表の中では `|` が `\|` になるため）。
- §2 の設計判断 D40〜D56 は、2026-10-30 にユーザーに 1 件ずつ確認して決めた（17 件すべて個別確認。D51・D54・D55 は代替を選んだ）。各節の先頭に「決定:」の行があり、推奨と代替の比較は経緯として残す（P0-1）。
- 実装先の事実は、2026-10-28 に `09e05de` のソースを読み、`build/core-gcc-release/ozen` で実行して確かめた（§3）。書き終えた後に独立したサブエージェントの監査を 1 回受け、指摘を取り込んだ（§9）。
- §4 の確かめられる主張には仕様項目 ID を付けてある。§5.3 の網羅表が、S 項目ごとにテストか §8 への引き渡し（理由付き）を示す。
- 期待値の作り方はロードマップ D20 に従う。期待値は §4 から手で書き、実装より先のコミットに置く。段取りは手で組み、`tools/schedcheck/` で違反が 0 であることを確かめてから期待値にする。実装の出力を写さない。

---

## 1. 目的とスコープ

**目的**: Ozen のレシピを、実際の料理の手順に近い形で短く書けるようにする。合わせて、書き方を調べる中で見つかった不具合を直す。本番のテストの入力は、今は `recipe r() { … let w = wash(rice) … }` のような記号的な形をしている。これを実際の料理に近い形に書き直し、テストを読めばそのまま言語の使い方が分かるようにする。役目を終えた試作 `proto/` は消す。

書き方の変化の例（今の書き方 → 本計画の後の書き方）:

```ozen
// 今
recipe r() {
  ingredients {
    rice: 300 g
    onion: 1 pcs
  }
  let w = wash(rice)
  let s = soak(w)
  let c = cookRice(s)
  let p = peel(onion)
  let o = cut(p, style: slice)
  let f = fry(o, 5 min)
}

// 本計画の後
recipe gohanToTamanegi(servings: 2) {
  ingredients {
    rice:  75 g per serving
    onion: 1 pcs
  }
  let gohan = rice |> wash |> soak |> cookRice
  let fried = onion |> peel |> cut(style: slice) |> fry(5 min)
  serve main {
    plate perPerson on Bowl { gohan }
    plate shared on FlatPlate { fried }
  }
}
```

### スコープ内

- パイプ演算子 `|>`（D42・D43）
- 引数の無い `recipe`・`menu` の `()` を省く書き方を正とすること、`make 名前` を `make 名前()` と同じにすること（D44）
- 1 人あたりの量 `per serving`（D46）
- 不具合の修正: 人数などの Count 型の値の次元（D45）、材料の量の次元の検査、`make` で渡した人数、`--serve-at` のときの `@serveWithin`（§3.4）
- 使われない値の診断 `LIN005 unusedValue`（D47）
- 道具の予熱（D48）
- 作ってから使うまでの期限 `@useWithin`（D49）
- 本番のテストの書き直し（`tests/`・`core/tests/`・`bindings/inputs/`。D50・D51）
- サンプル（`samples/`）のパイプと `per serving` での書き直し（D54。IR・段取り・描画の出力は変えない）
- 試作 `proto/` の削除（D52）
- concept・README・`CLAUDE.md` の更新（D53）。版は `0.1.0` のまま（D55）

### スコープ外（理由）

- **同じ名前での再束縛（`let onion = peel(onion)`）**: concept §4.1・§4.4 の「`let` は 1 回だけ束縛（SSA）」の決定事項に反する。名前を考える手間はパイプで減らす。
- **お好みの材料（`?`）・割合の量（`1 % of pork`）・同じ鍋のまま続けること**: 会話で候補に挙げたが、要求に入れたのは予熱・`@useWithin`・1 人あたりの量だけ。
- **`std` に `bake` などのオーブンの動作を足すこと**: `std` の範囲は concept §12.2 の一覧と基準の献立で決まり、`G-STD-LISTS` が照合している。予熱はテストの中で定義した動作で確かめる。
- **台所への要求の検査の穴（`:`・`has` を検査しない、台所に無い項目を満たしたことにする。§3.4 の B7）**: 予熱の `heated` だけを本計画で扱う。台所に `maxTemp` が無いときの扱いは今の `req_met` に合わせる（S581）。ほかは別の計画にする（§9）。
- **ロードマップの見出しの番号の重複（D33・D34 が 2 つずつ）と、文法の規則の数の食い違い（130 と 131）**: ロードマップは書き換えない（D41）。ユーザーへの報告だけにする（§9）。

---

## 2. 設計判断

### D40. 作業の場所と範囲の基点 【決定済み】

- **決定:** 推奨のとおり、`main` で作業し、P0-2 で打つタグ `refresh-start` から差分を数える。（2026-10-30、ユーザー確認）
- **推奨**: `main` で作業する（`CLAUDE.md`・ロードマップ D2）。P0-2 で `main` にタグ `refresh-start` を打ち、本計画の差分の範囲を `RF=refresh-start..HEAD` で数える。
  - push は、2026-10-30 にユーザーが「基本プッシュしていけ」と指示したので、そのつど確認を取らずに行う。push の前には毎回 `G-REPO-NOREPLY` を回す。着手点の `09e05de` も本ファイルと一緒に push する。
  - 別のセッションが同じ `main` にコミットしていた（会話の開始後の 196 コミット）。各フェーズの最初に `git log` で他の作業が入っていないかを見る（R6）。
- 代替: 作業ブランチ `syntax-refresh` を切り、最後に `main` へマージする。並行の作業とぶつかりにくいが、`CLAUDE.md` の「`main` だけ」に反する。

### D41. 計画ファイルとゴールの置き場所 【決定済み】

- **決定:** 推奨のとおり、本ファイルを新しい計画として置き、ロードマップは書き換えない。ゴールは `goals.sh` に `G-RF-*` として足し、`CLAUDE.md` の Plan の節に本ファイルを足す。（2026-10-30、ユーザー確認）
- **推奨**: 本ファイルを新しい計画として置く。ロードマップは完了した記録として残し、書き換えない。例外は、試作の削除に伴う `scripts/retired-tests.txt` への追記だけ（ロードマップの外のファイル）。ゴールの関数は `scripts/goals.sh` に `G-RF-*` として足し、本ファイルを指す変数 `PLAN_RF` を足す。版の行を見る `G-REPO-VERSION` は、比べる先をロードマップのままにする（D55 で版を据え置くので、ロードマップの「版: 0.1.0」の行がそのまま使える）。`CLAUDE.md` の「Plan」の節に本ファイルを足す。
- 代替: ロードマップに Phase 17 以降として書き足す。ゴールと網羅表を 1 つにまとめられるが、3,400 行を超えるロードマップがさらに長くなり、完了した計画と進行中の計画の区別がつかなくなる。

### D42. パイプの記号と意味 【決定済み】

- **決定:** 推奨のとおり、記号は `|>` で、左の値を最初の位置の引数にする。右辺は名前か名前の呼び出しに限り、優先順位は式の中で最も低い。（2026-10-30、ユーザー確認）
- **推奨**: 記号は `|>`。`a |> f` は `f(a)`、`a |> f(x, k: v)` は `f(a, x, k: v)` と読む（左の値を最初の位置の引数にする）。右辺は名前か、名前の呼び出し（動作・マクロ・組み込み）に限る。優先順位は式の中で最も低い（`expr := pipe ; pipe := link { '|>' callee }`）。
  - 理由: std の動作は、どれも食材を最初の引数に取る。調理の順に左から右へ読める。F#・Elixir と同じ形で、学ぶ人が迷いにくい。
  - `.` はフィールドの参照（`kitchen.floor`）と、`->` は戻り値の型と見分けがつかなくなる。`|>` は今の文法では正しいコードに現れない（`|` の直後には型か名前が来る）。
- 代替 A: 語 `then` を使う（`rice then wash then soak`）。予約語が増え、`then` を名前に使えなくなる。
- 代替 B: 左の値を `_` の位置に入れる（`a |> cut(_, style: slice)`）。どの引数にも入れられるが、std の動作は最初の引数に食材を取るので利点が小さく、書く量が増える。

### D43. パイプの構文木 【決定済み】

- **決定:** 推奨のとおり、構文解析の段階で呼び出し（`OZNI_N_Call`）に組み替え、文法の規則 `pipe` を `mark` で記録する。（2026-10-30、ユーザー確認）
- **推奨**: 構文解析の段階で呼び出し（`OZNI_N_Call`）に組み替える。右辺が呼び出しなら、その引数の先頭に左辺を差し込む。右辺が名前なら、引数 1 つの呼び出しを作る（ノードが 1 つ増え、`--max-nodes` の数に効く）。呼び出しの span は左辺の始まりから右辺の終わりまでにする。文法の規則 `pipe` は `mark` で記録し、`G-CORE-GRAMMAR` で網羅を確かめる。
  - 理由: 名前の解決・評価・燃料の数え方・注釈の検査（`check_targets`）・マクロの引数の結び付けは、どれも呼び出しを「最初の子が呼ばれる側、残りが引数」として扱っている（§3.2）。組み替えれば、これらを変えずに済む。
- 代替: 専用のノード `Pipe` を作る。`debug ast` に書いたとおりの形が出るが、評価・燃料・`check_targets`・マクロの扱いなど 5 か所以上に分岐を足すことになる。

### D44. `()` の省略と `make` 【決定済み】

- **決定:** 推奨のとおり、引数の無い `recipe`・`menu` は `()` を省く形を正とし、`make 名前` を `make 名前()` と同じに評価する。書き手のソースから空の `()` を消す。（2026-10-30、ユーザー確認）
- **推奨**: 引数の無い `recipe`・`menu` は `()` を省く書き方を正とする（文法は既に省略を許している）。書き手のソース（`tests/` の `.ozen` と heredoc、`core/tests/unit/` の C の文字列、`bindings/inputs/`、`renderers/test/inputs/`、README、concept の例）から空の `()` を消す。`()` の有無を比べるテストだけは、例外のリストに理由と一緒に書いて残す。`make 名前`（括弧なし）は `make 名前()` と同じに評価する（今は何も作らず、診断も出さない。§3.4 の B3）。IR の表示（`%1 = make steamedRice() : …`）はツールの出力なので変えない。
  - 理由: `kitchen myHome {`・`type Options {` など、引数を取らないほかの宣言と揃う。
- 代替: `recipe`・`menu` の `()` を必須にし、`make 名前` を構文の誤りにする。呼び出しと見た目が揃うが、文法を変え、ほかの宣言と書き方が分かれる。

### D45. 人数などの数の次元 【決定済み】

- **決定:** 推奨のとおり、型が `Count` の引数とレコードの項目の値は無次元の数として持ち、Count の次元（`pcs`）は材料の量だけに使う。（2026-10-30、ユーザー確認）
- **推奨**: 型が `Count` の引数と、レコードの項目（ビルド時の条件の `servings`・`cooks`、台所の `count`・`burners` など）の値は、**無次元の数** として持つ。`pcs` を付けて書いても無次元の数にする。Count の次元（単位 `pcs`）は材料の量だけに使う。
  - 効果: `150 g * servings` が `g*pcs` ではなく `g` になる（B1）。`servings > 4` が `TYP001` にならない（B2）。
  - 理由: 「4 人分」の 4 は個数ではなく倍率として使われる。材料の `pcs` は、`to_mass` が 1 個の重さを掛けるときに必要な次元なので残す。
- 代替: Count の次元を残し、掛け算・割り算・比較では Count を無次元と同じに扱う規則を足す。値の形は変わらないが、`2 pcs + 3` のように個数と数を混ぜた式が黙って通る。

### D46. 1 人あたりの量 【決定済み】

- **決定:** 推奨のとおり、材料の宣言に `per serving` を書いた量だけを人数に比例させ、印の無い量は総量のままにする。人数が決まらなければ `TYP020 servingsUnknown`。（2026-10-30、ユーザー確認）
- **推奨**: 材料の宣言に `per serving` を書いた量だけを人数に比例させる（`rice: 75 g per serving`）。印の無い量は今と同じく、人数で変わらない総量。人数は、その料理の引数 `servings`、無ければビルド時の条件の `servings` から取る。どちらも無ければ新しいエラー `TYP020 servingsUnknown` にする（`TYP018`・`TYP019` は使用済み）。`per` と `serving` は予約語にせず、`on` と同じく文脈の語として読む。
  - 理由: 既存のサンプルとテストの量（総量）をそのまま保てる。人数で変わる量と変わらない量を、書いた人が区別できる。
- 代替 A: Cooklang のように、印の無い量を人数に比例させ、変わらない量にだけ印を付ける。家庭のレシピの多くの材料は比例するので書く量は減るが、既存のサンプルとテストの量がすべて意味を変える。
- 代替 B: 記法を足さず、`75 g * servings` と書く（D45 で次元は正しくなる）。言語は変わらないが、読み手には総量か 1 人分かが式を読まないと分からない。

### D47. 使われない値の診断 【決定済み】

- **決定:** 推奨のとおり、使われない食材の値と宣言しただけの材料を `LIN005 unusedValue`（警告）で報告する。部位・`store`・`discard`・最後の値・`make` の中・合成した動作の中には出さない。（2026-10-30、ユーザー確認）
- **推奨**: 料理の評価の終わりに、次のすべてに当てはまる食材の値を `LIN005 unusedValue`（**警告**）で報告する。消費されていない、最後の値（とその組）でない、出されていない、部位（split・捌き・組の出力。これは `LIN003` の対象）でない、割り算で一部を取り出した途中でない。宣言しただけで使わない材料も含める。`store(x)`・`discard(x)` を通した値は消費されたことになるので出ない。`make` で呼ばれた料理の評価の中と、合成した動作の `steps` の中の値には出さない。
  - 理由: 炊いたご飯を丸ごと捨てるのは、ほぼ書き間違い。部位の残り（わざと残すことが多い）は今どおり情報の `LIN003` にする（concept §4.1）。
- 代替 A: 重さを情報にする。部位の残りと揃うが、`warnings=0` の期待値を見ても気づきにくい。
- 代替 B: `LIN003 leftoverPart` の対象を広げる。コードが増えないが、名前が「部位」と合わず、重さを部位の残りと別に変えられない。

### D48. 道具の予熱 【決定済み】

- **決定:** 推奨のとおり、`uses` の `HeatSource<oven, heated: t>` から、スケジューラが同じ個体に予熱を先読みで入れる。`maxTemp` が低ければ `KIT002`、無ければ満たしたことにする。予熱は IR に入れない。（2026-10-30、ユーザー確認）
- **推奨**: 動作の `uses` に、温まった能力を `HeatSource<oven, heated: 200 degC>` のように書く（温度は動作の引数の式でもよい）。設備の種類は props に `preheatTime`（範囲、`@source(provisional)`）を持つ。スケジューラは、その作業の前に、同じ個体に `preheat` の作業（無人、個体を占有）を入れる。
  - 予熱の終わりを作業の始まりに合わせる（先読み）。前の作業に依存する作業では、予熱を前の作業と並行に進める。`@serveWithin` と同じ release の反復で組み直す。今の洗いは消費する作業が ready になった時刻に始まり、先読みしない（§3.6）ので、同じ形にはしない。
  - 続けて同じ個体を同じかそれより低い温度で使う作業には、予熱を入れ直さない。
  - 台所の個体の `maxTemp` が要る温度より低ければ `KIT002 requiresUnmet`。`maxTemp` が無い個体は、今の `req_met` と同じく満たしたことにする。
  - 予熱は IR に入れない（洗いと同じく段取りの段階で入れる。ロードマップ D31 の不変条件）。
  - 理由: レシピは台所を名指ししない（concept §4.2）。段取りの段階で入れれば、台所や人数を変えても IR が変わらない。
- 代替: レシピに `preheat(oven, 200 degC)` の作業を書かせる。実装は小さいが、どの作業のための予熱かがつながらず、予熱から使うまでに冷めることや、別の温度の作業が割り込むことを検査できない。

### D49. 作ってから使うまでの期限 `@useWithin` 【決定済み】

- **決定:** 推奨のとおり、値を作った作業の終わりから、その値を入力に取るそれぞれの作業の始まりまでを t 以内にし、守れなければ `SCH011 useWithinMissed`（警告）。材料そのものに付けたら `TYP017`。（2026-10-30、ユーザー確認）
- **推奨**: `let` か式の文に `@useWithin(t)` を付けると、その値を作った作業の終わり（`endHi`）から、その値を入力に取る**それぞれの**作業の始まりまでを t 以内にする。`@serveWithin` と同じ release の反復で組み、守れなければ `SCH011 useWithinMissed`（**警告**。引数は値の名前・待ち・上限、位置は注釈）。値を作る作業が無い（材料そのものに付けた）ときは `TYP017 annotationTarget`。消費する作業が無い値に付けても、段取りの制約も診断も無い（使われないことは LIN005 が知らせる）。段取りの JSON に `limits` の欄を足し、`schedcheck` が独立に検査する。
  - 理由: 天ぷらの衣・切ったりんごなど、途中の値の鮮度は料理の失敗に直結する。`@serveWithin` の仕組み（release・採点・警告）をそのまま使える。
- 代替 A: 最初の消費の作業だけを見る。実装は小さいが、2 回に分けて使う値（衣を 2 回に分けて揚げる）で後の回を守れない。
- 代替 B: 守れなければエラーにする。厳しいが、`--prefer` や人数で守れないこともある段取りの制約を、ほかの出すタイミングの制約（警告）と別の重さにすることになる。

### D50. テストの書き直しの規則 【決定済み】

- **決定:** 推奨のとおり、ファイル名と動作の並びを保ち、料理名・束縛名・材料・量を実際の料理に寄せる。料理として成り立たないときだけ動作を足す。期待値は手で書き直し、上限の組は数え直す。（2026-10-30、ユーザー確認）
- **推奨**: 次の規則で書き直す。
  - テストのファイルの道筋（名前）は変えない。網羅表の名前・`bindings/inputs.txt`・`goals.sh` の名前・red-first の検査をそのまま保てる（§3.3）。
  - 料理名・束縛の名前・材料・量を、実際の料理に寄せる。1 文字の名前を使わない。引数の無い宣言の `()` は省く（D44）。
  - **動作の並び（`debug steps` の動作の名前の列）は保つ**。料理として成り立たない手順（切っただけで終わる、など）に限り、動作を足してよい。足したテストは D51 のとおりわざと壊す確認をやり直す。
  - パイプと `per serving` は自然に使える所で使う。`let` そのものを確かめるテスト（再束縛・範囲・マクロの衛生など）は `let` のまま残す。
  - 期待値は §4 とロードマップ §4 の書式から手で書き直す（位置の行と列、時間と量の計算を含む）。段取りは手で組み、`schedcheck` で違反 0 を確かめる。
  - 上限・燃料・バイト数・確保のテスト（`limits/*`・`eval/fuel-*`・`limits-shared/*`・`diag/out-of-memory`・`diag/depth-exceeded`）は、「ちょうど」と「1 つ超え」の組を保つように数え直す。「ちょうど」が本当にちょうどであることは `G-RF-LIMIT-PAIRS` で確かめる。
- 代替: 動作の並びも自由に変える。より実際の料理に近づくが、ほぼすべてのテストでわざと壊す確認をやり直すことになる（ロードマップの網羅表で約 260 の名前）。

### D51. 書き直しの確かめ方 【決定済み】

- **決定:** 代替 A のとおり、書き直したテスト（golden・CLI のテスト・単体テスト）を指すロードマップ §5.3 の**すべての**行で、わざと壊す確認をやり直し、本計画の §9 に `- 書き直しのわざと壊す確認: ` の行で記録する（2026-10-28 の数えで約 242 行）。推奨の機械の比較（`check-rewrite-equivalence.py`）も、やり直す前の安価な検査として残す。（2026-10-30、ユーザー確認）
- **推奨**: 新しい検査スクリプト `scripts/check-rewrite-equivalence.py` で、書き直す前（タグ `rewrite-start`）と後（Phase 5 の途中は HEAD、P5-9 からはタグ `rewrite-end`）の golden を比べる。それぞれの時点のコミットを一時の worktree に取り出してビルドし、その時点の実行ファイルで走らせる（後の Phase の LIN005 などの影響を受けない）。比べるのは、終了コード、診断のコードの並び、結果の行の数（errors・warnings・infos）、動作の名前の列（`debug steps` か `debug ir` の op の名前）。動作の名前の列が変わったテストの一覧を出す。そのテストに当たるロードマップ §5.3 の行の「わざと壊す方法」をやり直し、本計画の §9 に `- 書き直しのわざと壊す確認: ` の行で記録する（網羅表の `わざと壊す確認` の記録と数え方を分ける）。
  - 理由: 診断と動作の並びが同じなら、そのテストが確かめている性質（どの診断が出るか、どの作業ができるか）は保たれている。やり直す確認を、構造が変わったテストに絞れる。
- 代替 A: ロードマップの網羅表のうち、書き直したテストを指すすべての行で、わざと壊す確認をやり直す。最も確かだが、数百回の手作業になる。
- 代替 B: スクリプトの比較だけにする。速いが、動作を足したテストが元の性質を確かめ続けているかは分からない。

### D52. 試作 `proto/` の削除 【決定済み】

- **決定:** 推奨のとおり、使っている検査スクリプト 5 つを自己テストごと `scripts/` に移し、タグ `proto-final` を打ってから `proto/` と `ios-proto.yml` を消す。`G-PROTO-*` を外し、試作のテストの名前を `retired-tests.txt` に書き、報告書は残す。（2026-10-30、ユーザー確認）
- **推奨**: 次の順で消す。
  1. コアのゴールが使っている検査スクリプト（`check-flags.py`・`check-pure.sh`・`check-noglobal.py`・`check-diag-registry.sh`）と README の例の検査（`check-readme-example.sh`）を、自己テストと一緒に `scripts/` に移す。根の求め方（`root=$(cd "$(dirname "$0")/../.." …)`）と、`check-diag-registry.sh` の既定の道筋（`proto/src/diag_codes.def` など）を直す。README の例の検査は、`run:` の無い塊の既定を `ozen` にする。
  2. `proto/` を参照する残りのスクリプトを直す: `goals.sh`、`selftests.txt`、`check-selftests.sh` の対象のフォルダ、`check-spdx.sh` の対象、`check-plan-tests.sh` の `golden`・`sh` の解決、`test-check-plan-tests.sh`・`test-check-spdx.sh`。`check-red-first.sh` の、履歴のコミットを取り出す分岐は残す（過去のコミットの判定に要る）。
  3. 削除の直前のコミットにタグ `proto-final` を打つ。
  4. `proto/` と `.github/workflows/ios-proto.yml` を消す。手元の試作のビルド（`build/gcc-*`・`build/clang-*`）も消す（`check-plan-tests.sh` の `c/*` が古いビルドで解決してしまうため）。
  5. `G-PROTO-*` を `goals.sh` から外す。`G-PROTO-README-EXAMPLE` は `G-RF-README-EXAMPLE` に置き換える。
  6. ロードマップ §5.3 の試作のテストの名前（`golden/*` 109 件・`c/*` 4 件・試作だけにある `sh/*`）を、理由と一緒に `scripts/retired-tests.txt` に書く。
  7. README の「Evaluator prototype」の節と、ライセンスの表の `proto/` を消す。`G-REPO-README-LICENSE` の比べる文字列も合わせる。
  8. `docs/prototype-report.md` は残し、冒頭に「試作は削除した。タグ `proto-final` で読める」と書く。
  - 理由: コアはコードもテストも `proto/` を参照していない（§3.5）。報告書とタグがあれば、試作から学んだことは辿れる。
- 代替 A: 消さずに残す。何も壊れないが、使わない 574 ファイルがゴールの回る時間と CI（`ios-proto`）を使い続ける。
- 代替 B: ロードマップ D28 と同じく、ゴールだけを退役させ、ファイルは残す。先例と揃うが、要求（消す）と合わない。

### D53. concept の更新 【決定済み】

- **決定:** 推奨のとおり、concept の §4.1・§4.2・§4.4・§4.5・§6.1 に本計画の新しい仕組みを書き足し、例の空の `()` を消す。（2026-10-30、ユーザー確認）
- **推奨**: concept の該当の節に、本計画で足す言語の仕組みを書き足す。§4.4 の構文（パイプ・`()` の省略・`per serving`）、§4.1 の使い切り（使われない値）、§4.2 の資源（予熱）、§4.5 の出すタイミング（`@useWithin`）、§6.1 の診断（`LIN005`・`TYP020`・`SCH011`）。書き足す内容は本計画 §4 と同じにする。例の空の `()` も消す。
  - 理由: ロードマップは「挙動の正本は concept」とした。正本に無い仕組みが増えると、次の計画で食い違いを探す手間が増える。
- 代替: concept は変えず、本計画 §4 を新しい仕組みの正本にする。concept の記述は保てるが、読み手は 2 つの文書を見比べることになる。

### D54. サンプルを新しい書き方に直すか 【決定済み】

- **決定:** 代替のとおり、サンプル（`samples/`）もパイプと `per serving` で書き直す。書き直しは構文の置き換えに限り、各サンプルの IR・段取り・レンダラーの出力・`expect.yaml` が書き直す前と同じであることを機械で確かめる（S616〜S620、P5-8）。（2026-10-30、ユーザー確認）
- **推奨**: 直さない（スコープ外）。サンプルで LIN005 が出ないこと（S567）だけを確かめる。
  - 理由: 要求はテストの書き直し。サンプルは既に実際の料理で、書き直すと `expect.yaml` と、レンダラーのテストが使う出力も変わる。
- 代替: サンプルもパイプと `per serving` で書き直す。README から見える例が新しい書き方になるが、作業と期待値の更新が増える。

### D55. 版 【決定済み】

- **決定:** 代替のとおり、版は `0.1.0` のまま据え置く。`VERSION`・`stdlib/package.ozen`・版を書いた期待値・`requires` の golden は変えない。（2026-10-30、ユーザー確認）
- **推奨**: 本計画の最後で版を `0.2.0` にする。ロードマップ D30 の決めた 0.x の互換（`"0.1"` は 0.1.0 以上 1.0.0 未満）により、既存の `requires "0.1"` は通る。変えるものは次のとおり。
  - `VERSION`、`stdlib/package.ozen` の `version`。
  - 版の文字列を直接書いた `.out`（`"ozenVersion":"0.1.0"` など 13 個）と、README の束ねる例の出力（`std: "0.1.0"`）。
  - 「新しい版を要る」ことを確かめる golden（`packages/requires-newer-minor` の `requires "0.2"`、`bundle/std-version-check` の `requires "0.2.0"`）は、0.2.0 で要求を満たしてしまうので、入力を `0.3` に変えて意味を保つ。
  - `G-REPO-VERSION` の版の行の比べる先を本ファイルにし、本ファイル §9 に「版: 0.2.0」の行を置く（D41）。
  - 理由: 新しい構文と、次元の扱いの変更（D45）がある。
- 代替: `0.1.0` のまま据え置く。上の変更が要らず、公開していない版なので実害は小さいが、構文が変わったことが版に出ない。

### D56. 作業の順番 【決定済み】

- **決定:** 推奨のとおり、試作の削除 → 不具合の修正 → 記法 → `per serving` → テストとサンプルの書き直し → LIN005 → 予熱 → `@useWithin` → 文書と仕上げ の順にする。（2026-10-30、ユーザー確認）
- **推奨**: 試作の削除 → 不具合の修正 → 記法（パイプ・`()`）→ `per serving` → テストの書き直し → LIN005 → 予熱 → `@useWithin` → 文書と仕上げ。
  - 理由: 書き直しは新しい書き方を使うので、記法の後に置く。LIN005 は既存の golden の約 130 個の期待値を変える（§3.2）。書き直しで実際の料理にすれば使われない値の多くが消えるので、書き直しの後に置けば期待値の更新が少なくて済む。試作の削除は独立していて、ゴールの回る時間が減るので最初に置く。
- 代替: LIN005 を書き直しの前に置く。書き直しのときに使われない値に気づけるが、約 130 個の期待値を 2 回直すことになる。

---

## 3. 現状の確認結果（2026-10-28、`09e05de`）

### 3.1 ロードマップの状態

- P16-4 の最終確認は 2026-10-27 に終わっている（ロードマップ §9.1）。§6 に残りのタスクは無い。
- D の最後は D39、S の最後は S478。見出しの D33 と D34 が 2 つずつある（ロードマップ 124・325 行目と 135・334 行目）。
- `G-CORE-GRAMMAR` の文法の規則は実測で 131。ロードマップ §4.20.1 は 130 と書いている。
- golden の入口は 363 件（単一ファイル 329・フォルダ 34）、`.ozen` は 436 個。
- `scripts/goals.sh --list` は 93 のゴール（ロードマップ D28 で退役した 6 つと `G-PROTO-*` 24 個を含む）。

### 3.2 構文と評価（`core/src/`）

- 字句: 記号の表 `puncts[]`（`lex.c:237-244`）を先頭から照合する。2〜3 文字の記号を表の先頭に置くことで最長一致にしている。`|>` は `{"|", OZNI_T_PIPE}` より前に置く。トークンの種類を足すと、`-Wswitch-enum -Werror` のため `parse.c` の `primary`（470-635）と `sum_start`（724-768）の switch を直す必要がある。
- 式: `expr`（`parse.c:847-864`）が link を兼ね、`or_expr` → … → `postfix`（638-662）→ `primary` と下る。呼び出しは `OZNI_N_Call` で、子は「呼ばれる側、続いて引数」。名前付きの引数の後に位置の引数が来ると `SYN017`（`parse.c:307-309`）。
- 呼び出しを「最初の子が呼ばれる側」として扱う所: `eval_inner`（`sema.c:4734` から。呼び出しの分岐は 4819-4866）、`call_action`（3963 から）、`eval_make`（6002 から）、`action_call`（4951-4960）、`resolve_macro_call`（`resolve.c:1157-1194`）、`levels.c:133-137`。
- 単体テスト `span_nesting`（`core/tests/unit/test_parse.c:26-74`）は、子の span が親の span の内側にあることを golden の全ノードで確かめる。
- 文法の網羅: `scripts/check-grammar-coverage.py` が、`core/grammar.ebnf` の規則名と、golden の全 `.ozen` の `debug ast --rules` を突き合わせる。規則名は `core/src/rules.def` から作る。規則を足すときは、文法・`rules.def`・`mark` を同時に変え、使う golden を足す。
- `recipe`・`menu` の `()`: `paren_params(p, n, false)`（`parse.c:1684`）で省略できる。書き手のソースに省略した宣言は 0 件。空の `()` は `tests` に 454 行（うち `.out` の IR の表示が 20 ファイル）、`renderers/test/inputs` に 11、README に 6、`core/tests` に 2、`docs/concept.md` に 2、`bindings/inputs` に 1。`samples/` は 0。
- `make 名前`: 子が Name で Call にならず、`eval_make` が診断なしで `unknown()` を返す（`sema.c:6005-6010`）。
- 線形: `consume()`（`sema.c:1743-1761`）。最後の値は `s->last`・`s->last_node`（`sema.c:132-133`）。部位の残りは `finish_dish`（5780-5806）の `LIN003`。`store`・`discard`・`plate` は std の普通の動作で、呼ぶと引数を消費する（`stdlib/prelude.ozen:720, 732-737`）。料理の全食材を集めた表は無い。
- `let x =` の名前が後ろで使われない golden は、粗い見積もりで 436 個中 131 個。`warnings=0` を含む `.out` は 282 個。
- 量: `ozni_qty`（`sema.h:13-17`）は有理数の区間と次元のベクトル。`*` と `/` は次元を単に足し引きする（`sema.c:4341-4348`）。
- 引数の値: `eval_param`（`sema.c:5894-5938`）は、型があれば `typed_qty`（2550-2561）で、型が `Count` で値が無次元なら Count の次元を付ける。型が無ければ無次元のまま。
- 材料の量: `eval_ingredients`（`sema.c:4870-4932`）は、量の次元が食材に合うかを検査しない。
- 人数: 材料の量は人数で変わらない。`s->servings` は盛り付けの容量・器の枚数・`ASM005` にだけ使う。`make r(servings: 4)` では、料理の中の `s->servings` が設定されない（`sema.c:6084, 5898-5900`）。
- 診断のコード: `TYP018 optionLevel`・`TYP019 outputMismatch`・`SCH010 serveWithinMissed` が使用済み。`LIN005`・`TYP020`・`SCH011` は空いている（`core/src/diag_codes.def`）。

### 3.3 テスト基盤

- golden: `scripts/golden-entries.sh` が入口を集め、`scripts/run-golden.sh` が標準出力と `exit <n>` を `.out` と比べる。1 つの golden は `// args:` の 1 行で、コマンドを 1 つだけ走らせる。
- テストの名前の参照: ロードマップ §5.3 の網羅表（`scripts/check-plan-tests.sh` が解決する）、`bindings/inputs.txt`（golden 11 件）、`scripts/goals.sh:688`（`bundle/order-by-name`）、`tests/props/bundle_roundtrip.py`（`samples` と `tests/golden/bundle/*`）。道筋を変えなければ、どれも壊れない。
- red-first（`scripts/check-red-first.sh`）は、期待値のファイルを **追加** したコミットだけを数える。既存の `.out` を変えるだけのコミットは対象外。道筋を変えると追加として数えられ、「not red」になる。
- `scripts/check-plan-tests.sh --mutations` は、§9 の `- わざと壊す確認: ` の行の数が網羅表の「わざと壊す方法」を持つ行の数と**等しい**ことを求める。
- テストの入力の中身に依存するゴール: `G-CORE-GRAMMAR`、`G-CORE-DIAG-REGISTRY`（103 のコードがどれかの `.out` に出る）、`G-CORE-UNIT`（`span_nesting`）、`G-CORE-OOM`、`G-CORE-PROPS`（`reindent`・IR の 4 つ・`monotonic`・`bundle_roundtrip`）、`G-SCHED-CHECK`、`G-CORE-DETERMINISM`・`G-PLAT-*` の digest。
- 書き直しの量: `tests/golden` は 30 のフォルダ・816 ファイル。宣言を持つ `.ozen` は 290 個で、そのうち 1 文字の名前が 180 個。`let` の 1 文字の名前も多い（`s` 101・`c` 64・`w` 46 など）。`tests/cli` の heredoc に 19 個、`core/tests/unit` の C の文字列に 2 個、`bindings/inputs/utf8.ozen` に 1 個。
- サンプルの検査 `tests/samples/check.py:119-131` は、`warnings` の数を `expect.yaml` に書いたときだけ比べる。11 個のサンプルのどれにも `warnings:` は書かれていない。

### 3.4 調べる中で見つかった不具合と穴

| ID | 挙動 | 再現 | 本計画 |
| --- | --- | --- | --- |
| B1 | `servings: Count = 4` の料理で `150 g * servings` が `600 g*pcs`（`-s 2` で `300 g*pcs`）になり、`wash`・`cookRice`・盛り付けを診断なしで通る | 2026-10-28 に `ozen debug steps -s 2` で確認 | D45・S492〜S494 |
| B2 | 同じ料理で `if servings > 4` が `TYP001 dimension`（`Count` と `Dimensionless`） | 2026-10-28 に `ozen check` で確認 | D45・S495 |
| B3 | 献立の `let b = make teishoku`（括弧なし）が何の手順も作らず、診断も出ない | 2026-10-28 に `ozen debug steps --entry dinner` で確認 | D44・S500 |
| B4 | `--serve-at` のとき `@serveWithin` が効かない。`serve/serve-within` に `-t "60 min"` を付けると、1680 s に仕上がって 3600 s に出し、`SCH010` が出る | 2026-10-28 に調査役が実行して確認。release を前向きの時刻のまま反転した時計に写している（`sched.c:800-804`） | S502・S503 |
| B5 | 材料の量の次元を検査しない（`rice: 3 min` も通る） | `sema.c:4910-4915` を読んで確認 | S497〜S499 |
| B6 | `make r(servings: 4)` で渡した人数が、器・盛り付けの検査に使われない | `sema.c:6084, 5898-5900` を読んで確認 | S501 |
| B7 | 台所への要求の `:`・`has`、台所に無い項目は、検査されずに満たしたことになる（`req_met`、`sema.c:3163-3171`）。`HeatSource` の `modes` の綴りも検査しない | 読んで確認 | `heated` だけ D48 で扱い、ほかはスコープ外（§9） |
| B8 | 使われない値と、宣言しただけの材料に診断が出ない | 2026-10-28 に確認 | D47 |

### 3.5 試作 `proto/`

- 574 ファイル（`tests` 527・`scripts` 24・`src` 12 など）。`core/`・`cli/` はコードもテストも参照しない。
- 参照している所:
  - `scripts/goals.sh`: `G-PROTO-*` 24 個と、コアの 4 つのゴール（`G-CORE-FLAGS` 417 行・`G-CORE-PURE` 453・`G-CORE-NOGLOBAL` 458・`G-CORE-DIAG-REGISTRY` 480）が使う `proto/scripts/` の検査。
  - `scripts/selftests.txt` の 10 行、`scripts/check-selftests.sh:26` の対象のフォルダ、`scripts/check-spdx.sh:36`。
  - `scripts/check-plan-tests.sh:121, 138`（`golden`・`sh` の解決）、`scripts/test-check-plan-tests.sh:67, 92`、`scripts/test-check-spdx.sh:40, 71, 74`。
  - `scripts/check-red-first.sh:34, 48, 63-65`（履歴のコミットを取り出す分岐。残しても害は無い）。
  - 移すスクリプトの中: `check-readme-example.sh` の根の求め方と既定の実行ファイル、`check-diag-registry.sh:7-9` の既定の道筋。
  - `.github/workflows/ios-proto.yml`、README の 42-91 行と 292 行のライセンスの表、`docs/prototype-report.md`。
- `check-readme-example.sh` の使い方は `[--readme FILE] [--bin OZENPROTO] [--ozen OZEN]` で、出力は `readme: N examples, M mismatches`。
- ロードマップ §5.3 の試作のテストの名前: `golden/*` 109 件、`c/*` 4 件（`ctest --test-dir build/gcc-debug-san` で解決する。手元に古いビルドが残っている）、試作だけにある `sh/*` 7 件。
- 退役の先例（ロードマップ D28）はファイルを消していない。削除は先例が無い。

### 3.6 段取り（`core/src/sched.c`・`sema.c`）

- 洗いの挿入: `fit`（`sched.c:102-174`）が個体ごとに洗いの要否を見積もり、`run()`（292 から。洗いの呼び出しは 430-445）が `clean_unit`（184-262）を呼ぶ。洗いは消費する作業が ready になった時刻に始まり、先読みはしない。挿入した作業の種類は `OZNI_EXTRA_*`（`sched.h:14`）。
- `@serveWithin`: `timing_of`（`sema.c:5335-5373`）が注釈を読み、`schedule_dish` の反復（8259-8354、最大 8 回）が仕上がりの作業に release を付けて組み直す。`score_schedule`（7644-7658）が守れない段取りを `feasible=false` にする。`SCH010` は 8445-8461 で出す。
- 予熱に使える前例: 保温で個体を占有する `hold_until`（`sched.c:465-467`、golden `serve/keep-warm-occupies`）。挿入した作業を kind で分けていない所が 6 つある（`sched.c:569-583`・833-843、`sema.c:7460`・7671・7278・kind の名前の配列 7075/7428/7764）。
- 台所への要求の検査 `req_met`（`sema.c:3132-3178`）は `>=`・`<=` だけを見る。能力の照合 `kind_has`（2652-2703）は状態の引数を無視する。std の `Oven`・`OvenRange`（`stdlib/prelude.ozen:143, 158`）が `HeatSource<oven>` を持ち、`OvenSpec { maxTemp: Temperature? }`（756-758）。
- 段取りの JSON の作業の `kind` は `op`・`plate`・`wash`・`rinse`・`soak`・`wipe`・`run`（`schemas/result.schema.json:1281-1290`）。`tools/schedcheck/schedcheck.py` は JSON だけを読む独立の検査器（自己テストは `scripts/test-schedcheck.sh`）。
- std にオーブンの動作は無い。golden は毎回その場で `bake` を定義している（`sched/calibration.ozen:2-4` など）。

---

## 4. 挙動仕様

確かめられる主張は 1 項目 1 主張で、先頭に `**S<n>**` を付ける。例の出力の書式はロードマップ §4.20.1・§4.21.1・§4.22.1 に従う。

### 4.1 試作の削除（D52）

- **S479** リポジトリに `proto/` が無い。
- **S480** リポジトリに `.github/workflows/ios-proto.yml` が無い。
- **S481** タグ `proto-final` は、`proto/` を消したコミットの親を指す。
- **S482** `goals.sh` の `G-CORE-FLAGS`・`G-CORE-PURE`・`G-CORE-NOGLOBAL`・`G-CORE-DIAG-REGISTRY` の関数は、`scripts/` の検査スクリプトを呼ぶ。
- **S483** `proto/` を消した後も、`G-CORE-FLAGS`・`G-CORE-PURE`・`G-CORE-NOGLOBAL`・`G-CORE-DIAG-REGISTRY` が合格する。
- **S484** 移した 5 つの検査スクリプトには `scripts/` に自己テストがあり、`scripts/selftests.txt` に載っている。
- **S485** README に「Evaluator prototype」の節が無い。
- **S486** README のライセンスの表のどの行にも `proto/` が無い。
- **S487** ロードマップ §5.3 の試作のテストの名前は `scripts/retired-tests.txt` に理由と一緒に載り、手元の試作のビルドが無い状態で `G-PLAN-TESTS-EXIST` が合格する。
- **S488** `docs/prototype-report.md` の冒頭 10 行に、タグ `proto-final` で試作を読めることが書いてある。
- **S489** `scripts/check-readme-example.sh` は、`run:` の無い塊を `--ozen` の実行ファイルで走らせる。
- **S490** `scripts/` と `.github/` のどのファイルも、`proto/` の道筋を使わない（`check-red-first.sh` の履歴の分岐を除く）。
- **S491** `scripts/goals.sh --list` に `G-PROTO-` で始まる ID が無い。

### 4.2 不具合の修正（D44・D45）

- **S492** 型が `Count` の引数の値は無次元の数になる。`recipe gohan(servings: Count = 4) { ingredients { rice: 150 g * servings } … }` の `rice` は `600 g`。
- **S493** ビルド時の条件の `servings`（`-s 2`）で上書きした値も無次元の数になる。S492 の料理を `-s 2` でビルドすると `rice` は `300 g`。
- **S494** 台所の型が `Count` の項目（`burners` など）の値も無次元の数になる。
- **S495** 型が `Count` の値を数と比べられる。`servings: Count = 4` の料理の `if servings > 4 { … }` は `TYP001` にならない。
- **S496** 型が `Count` の引数・項目に `pcs` を付けて書いた値（`servings: 2 pcs`）も、無次元の数になる。
- **S497** 材料の量の次元が質量・体積・個数のどれでもなければ `TYP001 dimension`。`rice: 3 min` はエラー。
- **S498** S497 の診断の引数は得た次元と `Mass`、位置は量の式。
- **S499** 質量・体積・個数の材料の量（`rice: 300 g`・`milk: 200 ml`・`onion: 2 pcs`）は通る。
- **S500** `make 名前`（括弧なし）は `make 名前()` と同じ手順・同じ出力の値になる。
- **S501** 献立の `make r(servings: 4)` で呼んだ料理の器の枚数の検査は、4 人分で行う。
- **S502** `--serve-at`（`serveAt`）を指定したときも、`@serveWithin(t)` を守る配置があれば守る。`serve/serve-within` に `serveAt: 60 min` を足した入力で、汁物の仕上がりから出すまでの待ちは 300 s 以下。
- **S503** S502 の入力で `SCH010` は出ない。

### 4.3 パイプ（D42・D43）

- **S504** `a |> f` は `f(a)` と同じ IR になる。
- **S505** `a |> f(x, k: v)` は `f(a, x, k: v)` と同じ IR になる。
- **S506** パイプは左から順につながる。`a |> f |> g(x)` は `g(f(a), x)` と同じ IR になる。
- **S507** パイプの優先順位は式の中で最も低い。`x + y |> f` は `f(x + y)` と同じ IR になる。
- **S508** 右辺が数か量なら `SYN003 unexpectedToken`（位置は右辺の最初のトークン）。
- **S509** 右辺が並びか括弧の式なら `SYN003 unexpectedToken`（位置は右辺の最初のトークン）。
- **S510** 右辺が `make` なら `SYN003 unexpectedToken`（位置は `make`）。
- **S511** 右辺がマクロの呼び出しでもよく、左の値はマクロの最初の引数になる。
- **S512** 名前付きの引数だけの呼び出し `a |> f(k: v)` は、`SYN017 argumentOrder` にならず `f(a, k: v)` と同じ IR になる。
- **S513** パイプの式の文に付けた `@unattended` は、最後の段の呼び出しに付く。最後の段が動作なら通る。
- **S514** 最後の段が動作・マクロの呼び出しでなければ（組み込みの `ceil` など）、`@unattended` は `TYP017 annotationTarget`。
- **S515** パイプを足す前後（タグ `refresh-start` と `rewrite-start`）で、`tests/golden/typeargs/*` と `tests/golden/decl/alias.out` の期待値は変わらない（型の `|` の読み方が変わらない）。
- **S516** `ozen debug ast --rules` は、パイプを使った入力で規則 `pipe` を出す。
- **S517** `core/grammar.ebnf` に規則 `pipe` がある。
- **S518** パイプで組み替えた呼び出しの span は、左辺の始まりから右辺の終わりまで。
- **S519** パイプを使った入力でも、すべてのノードの span は親の span の内側にある（`span_nesting`）。
- **S520** `a |> f` は `f(a)` と同じ数のノードを数える（`--max-nodes`）。
- **S521** ファズの辞書 `core/fuzz/ozen.dict` に `"|>"` がある。

### 4.4 `()` の省略（D44）

- **S522** 引数の無い `recipe`・`menu` は、`()` を書いても書かなくても同じ IR になる。
- **S523** 書き手のソース（`tests/` の `.ozen` と `tests/cli/*.sh`、`core/tests/unit/*.c`、`bindings/inputs/`、`renderers/test/inputs/`、README の `ozen` の塊）に、空の `()` を持つ `recipe`・`menu` の宣言と `make` が無い。例外は `scripts/empty-parens-exceptions.txt` に理由付きで書いたものだけ。
- **S524** concept の Ozen の例に、空の `()` を持つ `recipe`・`menu` の宣言と `make` が無い。

### 4.5 1 人あたりの量（D46）

- **S525** 材料の `rice: 75 g per serving` の量は、人数 n のとき `75 g × n` になる。
- **S526** `per serving` の無い材料の量は、人数を変えても変わらない（`-s 2` と `-s 4` で同じ量）。
- **S527** 人数は、料理の引数 `servings` から取る。
- **S528** 料理の引数に `servings` が無ければ、ビルド時の条件の `servings` から取る。
- **S529** 人数が決まらないとき（引数にも条件にも `servings` が無い）、`TYP020 servingsUnknown`。
- **S530** `TYP020` の重さはエラーで、位置は `per serving` の量の式。
- **S531** `per serving` は材料の宣言の中にだけ書ける。ほかの場所（引数の既定値など）では `SYN003`。
- **S532** 献立の `make r(servings: 2)` で呼んだ料理の `per serving` の量は 2 人分になる。
- **S533** IR と JSON の材料の量は、人数を掛けた総量。
- **S534** 人数を掛けた量の単位の次元は、元の量と同じ。
- **S535** 登録簿の末尾に `TYP020`、`core/tests/diag-names.txt` に `TYP:servingsUnknown`、`phrases/ja.ftl`・`en.ftl` に `diag-servings-unknown` がある。
- **S536** ファズの辞書に `"per"` と `"serving"` がある。

### 4.6 テストの書き直し（D50・D51）

- **S537** タグ `rewrite-start` から `rewrite-end` の間に、`tests/golden` のファイルの追加・削除・名前の変更が無い。
- **S538** 書き直した golden は、`rewrite-start` の時点と `rewrite-end` の時点（それぞれの時点の実行ファイル）で比べて、終了コード・診断のコードの並び・結果の行の数が同じ。
- **S539** 書き直したテスト（golden・CLI のテスト・単体テスト）を指すロードマップ §5.3 の行のうち、「わざと壊す方法」を持つすべての行に、本計画 §9 の `- 書き直しのわざと壊す確認: ` の記録がある（D51）。
- **S540** `tests/`・`core/tests/`・`bindings/inputs/` のレシピ・献立の宣言と `let` の束縛に、1 文字の名前が無い。
- **S541** 1 文字の名前の例外は、`scripts/test-names-exceptions.txt` に理由付きで書いたものだけ。
- **S542** golden の期待値を変えたコミットは、実装（`core/src/`・`core/include/`・`cli/src/`・`stdlib/`・`phrases/`・`schemas/`・`tools/schedcheck/`・`renderers/lib/`）を変えない。
- **S543** 上限の golden の「ちょうど」の入力（`limits/*-exact`・`limits-shared/*-exact`・`eval/fuel-exact`）は、上限を 1 下げて走らせると上限の診断になる。
- **S544** 書き直しの後も、文法の全規則を golden が使う（`G-CORE-GRAMMAR`）。
- **S545** 書き直しの後も、登録簿の全診断のコードがどれかの golden に出る（`G-CORE-DIAG-REGISTRY`）。
- **S546** 書き直しの後も、全 golden が 4 構成で合格する（`G-CORE-GOLDEN`）。
- **S547** 書き直しの後も、単体テスト（`span_nesting` を含む）が合格する（`G-CORE-UNIT`）。
- **S548** 書き直しの後も、確保の失敗の注入が合格する（`G-CORE-OOM`）。
- **S549** 書き直しの後も、性質のテストが合格する（`G-CORE-PROPS`）。
- **S550** 書き直しの後も、段取りを出す golden がすべて `schedcheck` を通る（`G-SCHED-CHECK`）。
- **S551** 書き直しの後も、橋渡しのテストが合格する（`G-PLAT-LINUX`・`G-PLAT-WASM`）。
- **S552** golden の入口の数は、着手点の N0 に本計画で足した数を加えたもの。

### 4.6.1 サンプルの書き直し（D54）

- 書き直しは構文の置き換えに限る。続けて当てる動作の `let` の連鎖をパイプにし、人数に比例する材料を `per serving` にする（料理の引数 `servings` の既定値で、元の総量と同じ量になる値にする）。`phrases` が名前で参照する束縛（ロードマップの `0ded0a3` で足した局所の値の名前）は、`let` のまま残す。
- **S616** 各サンプル（`samples/*`）の Ozen のソースに、パイプ `|>` が 1 つ以上ある。例外は `scripts/samples-pipe-exceptions.txt` に理由付きで書いたものだけ。
- **S617** 各サンプルの `args` で走らせた IR（`--emit ir`）は、`rewrite-start` の時点と同じ。
- **S618** 各サンプルの `args` で走らせた段取り（`--emit schedule`）は、`rewrite-start` の時点と同じ。
- **S619** 書き直しの間に、`samples/*/expect.yaml` と `renderers/test/` の期待値は変わらない。
- **S620** `renderers/tools/render-all.mjs` の全サンプルの出力（文章・HTML）は、`rewrite-start` の時点と同じ。

### 4.7 使われない値（D47）

- **S553** 料理の終わりに、消費されず、最後の値でなく、出されていない、束縛した食材の値は `LIN005 unusedValue`。
- **S554** `LIN005` の重さは警告。
- **S555** 束縛した値の `LIN005` の位置は束縛した名前、引数は名前と型。
- **S556** 束縛していない式の文の値が使われなければ `LIN005`。位置は式、引数は式の原文と型。
- **S557** 宣言しただけで使わない材料は `LIN005`（位置は材料の名前）。
- **S558** `store(x)`・`discard(x)` に渡した値には出さない。
- **S559** 部位（split・捌き・組の出力）には出さない（`LIN003` だけ）。
- **S560** 最後の値と、最後の組の値には出さない。
- **S561** `make` で呼ばれた料理の評価の中では出さない。
- **S562** S561 の料理を単体で評価すれば出す。
- **S563** 合成した動作の `steps` の中の値には出さない。
- **S564** 割り算で一部を取り出した食材には出さない（ロードマップ §4.21.1 の「n 回取り出すと使い切る」の途中）。
- **S565** `@allow(unusedValue)` で隠せる。
- **S566** 登録簿の末尾に `LIN005`、`core/tests/diag-names.txt` に `LIN:unusedValue`、`phrases/ja.ftl`・`en.ftl` に `diag-unused-value` がある。
- **S567** サンプル（`samples/*`）の評価で `LIN005` は出ない。

### 4.8 予熱（D48）

- **S568** `uses` に `HeatSource<oven, heated: t>` を持つ作業の前に、`preheat` の作業が入る。
- **S569** 予熱は、その作業が使う個体と同じ個体に入る。
- **S570** 予熱の終わりは、その作業の始まり以前。
- **S571** 前の作業に依存する、温度を要る作業では、予熱は前の作業と並行に進み、温度を要る作業の始まりの時刻に終わる。
- **S572** 予熱の所要時間は、設備の種類の props の `preheatTime`。
- **S573** 予熱は人を使わない（予熱の間に人は別の作業をする）。
- **S574** 予熱の始まりから、その温度を要る作業の終わりまで、その個体をほかの作業が使わない。
- **S575** 同じ個体を、続けて同じかそれより低い温度で使う作業には、予熱を入れ直さない。
- **S576** 続けてより高い温度で使う作業の前には、予熱を入れ直す。
- **S577** 要る温度は、動作の引数の式でもよい（`heated: temp`）。
- **S578** `--serve-at`（後ろ向き）でも、S570 と S571 を守る。
- **S579** 台所の個体の `maxTemp` が要る温度より低ければ `KIT002 requiresUnmet`。
- **S580** `maxTemp` が要る温度と等しければ通る。
- **S581** `maxTemp` を書いていない個体は、要る温度を満たしたことにする。
- **S582** 予熱は IR に入らない（台所を注入した `debug ir` に予熱の作業が無い）。
- **S583** 段取りのテキストに `<始まり>..<終わりの上限> (<終わりの下限>) preheat <個体> to <温度> unattended [<個体>]` の行が出る。
- **S584** 段取りの JSON で、予熱の作業は `kind: "preheat"` と `heats: {resource, temp}` を持つ。
- **S585** 段取りの JSON で、温度を要る作業は `needs: [{resource, temp}]` を持つ。
- **S586** `schemas/result.schema.json` が S584・S585 の形を許す。
- **S587** `schedcheck` は、温度を要る作業の前に同じ個体に足りる温度の予熱があり、その間に別の作業が個体を使わないことを、JSON だけから検査する。
- **S588** レンダラーは予熱の作業を `phrases` の文で書く（`ja.ftl`・`en.ftl` に予熱のキー）。
- **S589** レンダラーの段取り表（HTML）に予熱の行が出る。
- **S590** std の `HeatSource<oven>` を持つ設備の種類は、すべて `preheatTime` を持つ。
- **S591** `preheatTime` は暫定の範囲で、出どころ（`@source(provisional)`）と調べた記録がある。

### 4.9 作ってから使うまでの期限（D49）

- **S592** `let v = e @useWithin(t)` の料理では、守れる配置があれば、v を作った作業の終わり（上限）から、v を入力に取る作業の始まりまでを t 以内にする。
- **S593** S592 は、v を入力に取る 2 つ目以降の作業にも当てはまる。
- **S594** 守れないとき `SCH011 useWithinMissed`。
- **S595** `SCH011` の重さは警告で、引数は値の名前・待ち・上限、位置は注釈。
- **S596** `--serve-at`（後ろ向き）でも S592 を守る。
- **S597** 値を作る作業が無い（材料そのものに付けた）`@useWithin` は `TYP017 annotationTarget`。
- **S598** 束縛していない式の文に付けた `@useWithin` も、その値を入力に取る作業に効く。
- **S599** 入力に取る作業が無い値に付けた `@useWithin` は、段取りの制約も診断も生まない。
- **S600** 段取りの JSON に `limits: [{producer, consumer, within, missed}]` がある。
- **S601** `schedcheck` は `limits` を JSON だけから検査する。
- **S602** `@useWithin` は IR を変えない。
- **S603** 登録簿の末尾に `SCH011`、`core/tests/diag-names.txt` に `SCH:useWithinMissed`、`phrases/ja.ftl`・`en.ftl` に `diag-use-within-missed` がある。

### 4.10 文書・設定・版（D41・D53・D55）

- **S604** concept の §4.4 に、パイプ・`()` の省略・`per serving` が書いてある。
- **S605** concept の §4.1 に、使われない値が書いてある。
- **S606** concept の §4.2 に、予熱が書いてある。
- **S607** concept の §4.5 に、`@useWithin` が書いてある。
- **S608** concept の §6.1 に、`unusedValue`・`servingsUnknown`・`useWithinMissed` が書いてある。
- **S609** README にパイプを使った例があり、README の例の検査が通る。
- **S610** 版は `0.1.0` のままで、`ozen --version`・`stdlib/package.ozen` の `version`・ロードマップの「版: 0.1.0」の行が一致し続ける（`G-REPO-VERSION`、D55）。
- **S613** `CLAUDE.md` の Plan の節が本ファイルを名指しする。
- **S614** パイプと `per serving` を使ったソースは、`ozen bundle` と `ozen unbundle` を通しても同じ IR になる（`prop/bundle_roundtrip`）。
- **S615** タグ `refresh-start`・`proto-final`・`rewrite-start`・`rewrite-end` がある。

### 4.11 足すもの・変えるものの一覧

| 種類 | ID・パス |
| --- | --- |
| 字句・文法 | トークン `\|>`、文法の規則 `pipe`（`core/grammar.ebnf`・`core/src/rules.def`）、材料の項目の `per serving`、ファズの辞書 `core/fuzz/ozen.dict` の `"\|>"`・`"per"`・`"serving"` |
| 診断 | `LIN005 unusedValue`（警告）、`TYP020 servingsUnknown`（エラー）、`SCH011 useWithinMissed`（警告）。登録簿の末尾に追記し、`diag-names.txt`・`phrases/*.ftl`・`tests/golden/api/local-not-listed.out` に足す |
| std | `HeatSource<oven>` を持つ設備の種類（`Oven`・`OvenRange`）の props `preheatTime`（暫定の範囲、`@source(provisional)`）。`api/local-not-listed.out` の定義の一覧も変わる |
| 段取りの JSON | 作業の `kind: "preheat"`、`heats`、`needs`、トップレベルの `limits` |
| スクリプト | 新設: `scripts/check-rewrite-equivalence.py`（`--samples` を含む）・`scripts/check-test-names.py`（`--empty-parens`・`--samples-pipe` を含む）・`scripts/check-limit-pairs.py`・`scripts/check-std-preheat.py`。例外のリスト `scripts/samples-pipe-exceptions.txt`。移す: `check-flags.py`・`check-pure.sh`・`check-noglobal.py`・`check-diag-registry.sh`・`check-readme-example.sh`。それぞれの自己テスト。例外のリスト `scripts/test-names-exceptions.txt`・`scripts/empty-parens-exceptions.txt` |
| タグ | `refresh-start`・`proto-final`・`rewrite-start`・`rewrite-end` |
| 消すもの | `proto/`、`.github/workflows/ios-proto.yml`、README の試作の節、`scripts/goals.sh` の `G-PROTO-*` |

### 4.12 コードの配置

| 場所 | 置くもの |
| --- | --- |
| `core/src/lex.c`・`lex.h` | `\|>` のトークン（名前は実装で決める） |
| `core/src/parse.c` | `pipe` の解析と呼び出しへの組み替え、`per serving` の解析 |
| `core/src/sema.c` | Count の無次元化（`typed_qty`）、材料の次元の検査（`eval_ingredients`）、`make` の括弧なし（`eval_make`）と人数（`eval_param`）、`per serving` の評価、使われない値の追跡（`finish_dish`）、予熱と `@useWithin` の段取りへの受け渡し |
| `core/src/sched.c`・`sched.h` | 予熱の作業（`OZNI_EXTRA_PREHEAT`）、後ろ向きの release の写し方 |
| `schemas/result.schema.json`・`tools/schedcheck/` | 予熱と `limits` の形と検査 |
| `renderers/lib/`・`phrases/` | 予熱の文、診断の文 |

---

## 5. 機械検証ゴールの定義

**前提ゴール**は、ロードマップ §5 のゴールのうち次のもの（各タスクの完了条件に暗黙に含む）。

- 手元で回すもの: `G-API-EXPORTS`・`G-API-HEADER`・`G-API-SARIF`・`G-API-SCHEMA`・`G-BUNDLE-STABLE`・`G-CLI-TESTS`・`G-CORE-ARENA-ACCESS`・`G-CORE-BUILD`・`G-CORE-DETERMINISM`・`G-CORE-DIAG-APPEND`・`G-CORE-DIAG-REGISTRY`・`G-CORE-FLAGS`・`G-CORE-FUZZ`・`G-CORE-GOLDEN`・`G-CORE-GRAMMAR`・`G-CORE-NOFLOAT`・`G-CORE-NOGLOBAL`・`G-CORE-NOINT128`・`G-CORE-OOM`・`G-CORE-PROPS`・`G-CORE-PURE`・`G-CORE-STD-EMBED`・`G-CORE-UNIT`・`G-PLAN-COVERAGE`・`G-PLAN-TESTS-EXIST`・`G-PLAT-LINUX`・`G-PLAT-WASM`・`G-REND-BROWSER-SAFE`・`G-REND-HTML`・`G-REND-PHRASES-RENDER`・`G-REND-SELFCONTAINED`・`G-REND-TESTS`・`G-REND-VENDOR`・`G-REPO-CLAUDEMD`・`G-REPO-LICENSE-TEXT`・`G-REPO-NOTICE`・`G-REPO-README-LICENSE`・`G-REPO-RED-FIRST`・`G-REPO-SELFTESTS`・`G-REPO-SPDX`・`G-REPO-TOOLS`・`G-REPO-VERSION`・`G-SAMPLE-CHECK`・`G-SAMPLE-STD-ONLY`・`G-SCHED-CHECK`・`G-STD-LISTS`・`G-STD-PHRASES`・`G-STD-PROVENANCE`・`G-STD-SOURCES`・`G-STD-STATIC`・`G-TYPE-SEVERITY-TABLE`・`G-VOCAB-FCT`・`G-VOCAB-FCT-STABLE`・`G-VOCAB-LISTS`・`G-VOCAB-LISTS-COPY`・`G-VOCAB-PRIORWORK`・`G-VOCAB-SOURCES`
- push の後の CI で決まるもの: `G-CORE-C23-SUBSET`（Xcode の行）・`G-PLAT-IOS`・`G-PLAT-ANDROID`・`G-PLAT-CI`・`G-REPO-TOOLS-CI`
- push の前に毎回: `G-REPO-NOREPLY`
- 回さないもの: ロードマップ D28 で退役した `G-VOCAB-SCHEMA`・`G-VOCAB-CHECK`・`G-VOCAB-SAMPLE`・`G-VOCAB-TESTS`・`G-VOCAB-READONLY`・`G-VOCAB-EXPECT-INDEPENDENT`。P1-2 の前は `G-PROTO-*` も前提ゴールに含め、P1-2 で消す。

失敗したときは、どの部分（どのゴール、どのビルドの構成、どのテスト、どの環境）が失敗したかを報告する。

- 本計画の差分の範囲は `RF=refresh-start..HEAD`。
- `PLAN_RF=plans/ozen-syntax-refresh.md`。
- 0 を期待する数え上げは、終了コードでなく出力の数で判定する。
- P0-3 で、下のコードブロックを `scripts/goals.sh` の関数 `goal_G_RF_*` にする。新設のスクリプトを使うゴールの中身は、そのスクリプトを作るタスクで入れる。

### 5.1 構造・データ

| ID | 何を判定するか | 合格条件 |
| --- | --- | --- |
| `G-RF-DECISIONS` | D40〜D56 が確定しているか | 決定済み 17、未決 0、決定の行 17 |
| `G-RF-COVERAGE` | S 項目すべてが網羅表に行を持ち、§8 の各項目に理由がある | 差分が空、理由の無い項目 0 |
| `G-RF-PLAN-TESTS` | 本計画の網羅表のテストが実在し、わざと壊す確認の記録が行の数だけある | `plan-tests: <n> names, 0 missing` と `mutations: <m> rows, <m> recorded` |
| `G-RF-TAGS` | S615 | 4 つのタグがすべてある |
| `G-RF-PROTO-GONE` | S479〜S482・S488・S490 | 追跡するファイル 0、タグが削除の親、`goals.sh` の 4 つの関数の `proto/` の参照 0、報告書の冒頭 1 以上、`scripts`・`.github` の参照 0 |
| `G-RF-NO-PROTO-GOALS` | S491 | 0 |
| `G-RF-README-NO-PROTO` | S485 | 0 |
| `G-RF-README-EXAMPLE` | S489・S609 の後半 | `readme: <n> examples, 0 mismatches`、n ≥ 1 |
| `G-RF-README-PIPE` | S609 の前半 | 1 以上 |
| `G-RF-NO-EMPTY-PARENS` | S523 | `empty-parens: <n> sources, 0 found, 0 unlisted exceptions` |
| `G-RF-CONCEPT-PARENS` | S524 | 0 |
| `G-RF-FUZZ-DICT` | S521・S536 | 3 つがすべて 1 |
| `G-RF-STD-PREHEAT` | S590 | `HeatSource<oven>` を持つ種類の数と `preheatTime` を持つ種類の数が等しく、1 以上 |
| `G-RF-SAMPLES-UNUSED` | S567 | 0 |
| `G-RF-NAMES` | S540・S541 | `names: <n> declarations, <b> bindings, 0 one-letter, 0 unlisted exceptions` |
| `G-RF-PATHS` | S537 | 0 |
| `G-RF-TYPEARGS-STABLE` | S515 | 差分なし |
| `G-RF-SEPARATION` | S542 | `separation: <c> commits, 0 mixed`、c ≥ 1 |
| `G-RF-EQUIVALENCE` | S538・S539 | `equivalence: <n> rewritten, 0 changed results, <k> changed steps; records: <r> roadmap rows, 0 missing`（r は、書き直したテストを指すロードマップ §5.3 の行のうち「わざと壊す方法」を持つ行の数） |
| `G-RF-SAMPLES-PIPE` | S616 | `samples-pipe: <n> samples, 0 without pipe, 0 unlisted exceptions` |
| `G-RF-SAMPLES-SAME` | S617・S618・S620 | `samples-same: <n> samples, 0 changed ir, 0 changed schedules, 0 changed renders` |
| `G-RF-SAMPLES-EXPECT` | S619 | 差分なし |
| `G-RF-LIMIT-PAIRS` | S543 | `limit-pairs: <p> pairs, 0 not exact` |
| `G-RF-GOLDEN-COUNT` | S552 | N0 + 66 |
| `G-RF-CONCEPT` | S604〜S608 | 5 つの数がすべて 1 以上 |
| `G-RF-CLAUDEMD` | S613 | 1 以上 |
| `G-RF-CI-NO-PROTO` | S480 の CI の側 | push した HEAD で `ios-proto` の実行が 0 |
| `G-RF-AUDIT` | P9-4 の監査の記録 | 1 以上 |

```bash
# G-RF-DECISIONS
grep -c '^### D[0-9]*\. .*【決定済み】' $PLAN_RF   # → 17
grep -c '^### D[0-9]*\. .*【未決】' $PLAN_RF       # → 0
grep -c '^- \*\*決定:' $PLAN_RF                    # → 17
# G-RF-COVERAGE
diff <(sed -n '/^## 4\./,/^## 5\./p' $PLAN_RF | grep -oE '\*\*S[0-9]+\*\*' | tr -d '*' | sort -u) \
     <(sed -n '/^### 5\.3/,/^## 6\./p' $PLAN_RF | grep -oE '^\| S[0-9]+' | tr -d '| ' | sort -u)   # → 空
sed -n '/^## 8\./,/^## 9\./p' $PLAN_RF | grep '^- ' | grep -v '機械で決められない理由:' | wc -l   # → 0
# G-RF-PLAN-TESTS
scripts/check-plan-tests.sh $PLAN_RF               # → plan-tests: <n> names, 0 missing
scripts/check-plan-tests.sh --mutations $PLAN_RF   # → mutations: <m> rows, <m> recorded
# G-RF-TAGS
for t in refresh-start proto-final rewrite-start rewrite-end; do git rev-parse -q --verify "refs/tags/$t" >/dev/null && echo ok; done | wc -l   # → 4
# G-RF-PROTO-GONE
git ls-files proto .github/workflows/ios-proto.yml | wc -l                                   # → 0
[ "$(git rev-parse proto-final)" = "$(git rev-parse "$(git log --diff-filter=D --format=%H -1 -- proto)^")" ] && echo same   # → same
sed -n '/^goal_G_CORE_\(FLAGS\|PURE\|NOGLOBAL\|DIAG_REGISTRY\)()/,/^}/p' scripts/goals.sh | grep -c 'prot[o]/'   # → 0
sed -n '1,10p' docs/prototype-report.md | grep -c 'proto-final'                              # → 1 以上
git grep -nE '(^|[^-a-z])prot[o]/' -- scripts .github ':!scripts/check-red-first.sh' | wc -l    # → 0
# G-RF-NO-PROTO-GOALS
scripts/goals.sh --list | grep -c '^G-PROTO-'      # → 0
# G-RF-README-NO-PROTO
grep -c '^## Evaluator prototype' README.md        # → 0
# G-RF-README-EXAMPLE
scripts/check-readme-example.sh --readme README.md --ozen build/core-gcc-release/ozen   # → readme: <n> examples, 0 mismatches
# G-RF-README-PIPE
sed -n '/^```ozen/,/^```/p' README.md | grep -c '|>'   # → 1 以上
# G-RF-NO-EMPTY-PARENS（P3-2 で新設。P5-7 から 0 を求める）
python3 scripts/check-test-names.py --empty-parens --exceptions scripts/empty-parens-exceptions.txt tests core/tests/unit bindings/inputs renderers/test/inputs README.md
# G-RF-CONCEPT-PARENS（P9-1 から）
sed -n '/^```/,/^```/p' docs/concept.md | grep -cE '\b(recipe|menu) +[A-Za-z_][A-Za-z0-9_]* *\( *\)|\bmake +[A-Za-z_.]+ *\( *\)'   # → 0
# G-RF-FUZZ-DICT
for w in '"|>"' '"per"' '"serving"'; do grep -cxF "$w" core/fuzz/ozen.dict; done   # → 1 1 1
# G-RF-STD-PREHEAT（P7-1 で新設）
python3 scripts/check-std-preheat.py --ozen build/core-gcc-release/ozen   # → oven kinds: <k>, with preheatTime: <k>（k ≥ 1）
# G-RF-SAMPLES-UNUSED（P6-3 から）
for s in samples/*/; do build/core-gcc-release/ozen check --format json "$s" 2>/dev/null; done | grep -o '"name":"unusedValue"' | wc -l   # → 0
# G-RF-NAMES（P5-0 で新設）
python3 scripts/check-test-names.py --exceptions scripts/test-names-exceptions.txt tests core/tests/unit bindings/inputs
# G-RF-PATHS（P5-9 から）
git diff --name-status --no-renames rewrite-start..rewrite-end -- tests/golden | grep -v '^M' | wc -l   # → 0
# G-RF-TYPEARGS-STABLE（P5-0 から）
git diff --exit-code refresh-start rewrite-start -- tests/golden/typeargs tests/golden/decl/alias.out   # → 差分なし
# G-RF-SEPARATION
scripts/check-commit-separation.sh "$RF" '^tests/golden/.*\.out$' '^(core/src|core/include|cli/src|stdlib|phrases|schemas|tools/schedcheck|renderers/lib)/'
# G-RF-EQUIVALENCE（P5-0 で新設。Phase 5 の途中は --head HEAD、P5-9 からは --head rewrite-end）
python3 scripts/check-rewrite-equivalence.py --base rewrite-start --head rewrite-end --records $PLAN_RF tests/golden
# G-RF-SAMPLES-PIPE（P5-8 で新設）
python3 scripts/check-test-names.py --samples-pipe --exceptions scripts/samples-pipe-exceptions.txt samples
# G-RF-SAMPLES-SAME（P5-8 で新設。2 つの時点を一時の worktree でビルドし、各サンプルの args と render-all で比べる）
python3 scripts/check-rewrite-equivalence.py --base rewrite-start --head rewrite-end --samples samples
# G-RF-SAMPLES-EXPECT（P5-9 から）
git diff --exit-code rewrite-start rewrite-end -- 'samples/*/expect.yaml' renderers/test   # → 差分なし
# G-RF-LIMIT-PAIRS（P5-2 で新設）
python3 scripts/check-limit-pairs.py --ozen build/core-gcc-release/ozen tests/golden   # → limit-pairs: <p> pairs, 0 not exact
# G-RF-GOLDEN-COUNT
scripts/golden-entries.sh tests/golden | wc -l     # → N0 + 66（N0 は §9）
# G-RF-CONCEPT（P9-1 から）
sed -n '/^### 4\.4/,/^### 4\.5/p' docs/concept.md | grep -cE '\|>|per serving'
sed -n '/^### 4\.1/,/^### 4\.2/p' docs/concept.md | grep -c 'unusedValue'
sed -n '/^### 4\.2/,/^### 4\.3/p' docs/concept.md | grep -c 'preheat'
sed -n '/^### 4\.5/,/^### 4\.6/p' docs/concept.md | grep -c 'useWithin'
sed -n '/^### 6\.1/,/^## 7\./p' docs/concept.md | grep -cE 'servingsUnknown|useWithinMissed'   # → すべて 1 以上
# G-RF-CLAUDEMD
grep -c 'plans/ozen-syntax-refresh.md' CLAUDE.md   # → 1 以上
# G-RF-CI-NO-PROTO（push の後）
gh run list --workflow ios-proto.yml --commit "$(git rev-parse HEAD)" --json databaseId --jq length   # → 0
# G-RF-AUDIT（P9-4）
grep -cE '^- 網羅の監査: 指摘 [0-9]+ 件、取り込み [0-9]+ 件' $PLAN_RF   # → 1 以上
```

新設するスクリプト（`check-rewrite-equivalence.py`・`check-test-names.py`・`check-limit-pairs.py`・`check-std-preheat.py`）には、わざと壊した入力で失敗することを確かめる自己テスト `scripts/test-check-rewrite-equivalence.sh`・`scripts/test-check-test-names.sh`・`scripts/test-check-limit-pairs.sh`・`scripts/test-check-std-preheat.sh` を添え、`scripts/selftests.txt` に載せる（`G-REPO-SELFTESTS`）。

### 5.2 テスト

共通の作り方:

- golden はロードマップ §4.20.1 の書式で、期待値を §4 から手で書き、実装より先のコミットに置く（ロードマップ D20、`G-REPO-RED-FIRST`）。新しい性質を確かめる golden は、既存のファイルに行を足さず、新しいファイルにする（red-first は追加したファイルだけを数えるため）。
- 段取りは手で組み、`schedcheck` で違反 0 を確かめてから期待値にする。量と時間の計算は有理数で手計算し、`fractions.Fraction` と突き合わせる。
- 新しい性質だけのフォルダとして `tests/golden/pipe/`・`tests/golden/quantity/` を作る。

| テスト | 何を判定するか | 主なタスク |
| --- | --- | --- |
| `cgolden/quantity/*`（14 個） | Count の無次元化、材料の次元、`per serving` | P2-1・P2-2・P4-1 |
| `cgolden/menus/make-bare`・`make-servings`・`make-servings-per-serving` | `make` の括弧なしと人数 | P2-3・P2-4・P4-1 |
| `cgolden/serve/serve-within-backward` | `--serve-at` での `@serveWithin` | P2-5 |
| `cgolden/pipe/*`（15 個） | パイプの意味・優先順位・誤り・注釈・span・ノードの数 | P3-1 |
| `cgolden/syntax/omit-parens` | `()` の省略で同じ IR | P3-2 |
| `cgolden/linear/unused-*`（11 個） | `LIN005` | P6-1 |
| `cgolden/sched/preheat-*`（13 個） | 予熱 | P7-1 |
| `cgolden/serve/use-within-*`（9 個） | `@useWithin` | P8-1 |
| `cgolden/bundle/new-syntax` | 束ねてもパイプと `per serving` が保たれる | P4-1 |
| `sh/test-check-rewrite-equivalence.sh`・`sh/test-check-test-names.sh`・`sh/test-check-limit-pairs.sh`・`sh/test-check-std-preheat.sh` | 新しい検査スクリプトの自己テスト | P3-2・P5-0・P5-2・P7-1 |
| `sh/test-check-flags.sh`・`sh/test-check-pure.sh`・`sh/test-check-noglobal.sh`・`sh/test-check-diag-registry.sh`・`sh/test-check-readme-example.sh` | 移した検査スクリプトの自己テスト | P1-1 |
| `sh/test-schedcheck.sh` に足す例と、`tools/schedcheck/fixtures/bad-preheat-*.json`・`bad-use-within.json` | 予熱と期限の独立の検査 | P7-3・P8-3 |
| `rtest/schedule-text`・`rtest/recipe-schedule` に足す例 | 予熱の文と段取り表 | P7-4 |

golden の入口は、着手点の N0 = 363（§9）から 66 増える（`quantity` 14・`menus` 3・`serve` 10・`pipe` 13・`syntax` 1・`linear` 11・`sched` 13・`bundle` 1）。
落ちたテストは緩めずに原因を調べる。仕様との食い違いはユーザーに報告する。

機械で押さえる範囲: 構文・評価・段取り・JSON・文書の語・テストの名前と道筋・書き直しの前後の等価・上限の組・CI の digest。§8 に回すのは、書き直したレシピが料理として自然か、予熱の時間の値が妥当か、パイプの読みやすさ、警告の煩さ、concept の文体だけ。

### 5.3 網羅表

§4 の S 項目と §4.11 の一覧を、観点ごとにテストか §8 に割り当てる。「実際の経路」は CLI（`ozen`）を通すことで満たす（golden はすべて CLI を通る）。「プラットフォームの経路」は `G-PLAT-LINUX`・`G-PLAT-WASM` と、push の後の CI の `G-PLAT-IOS`・`G-PLAT-ANDROID`（golden の digest）で満たす。

| S 項目 | 観点 | テストまたはゴール | わざと壊す方法 | 主なタスク |
| --- | --- | --- | --- | --- |
| S479 | 正常系 | `G-RF-PROTO-GONE` | `proto/` のファイルを 1 つ戻す | P1-2 |
| S480 | 正常系 | `G-RF-PROTO-GONE` | `ios-proto.yml` を戻す | P1-2 |
| S480 | プラットフォームの経路 | `G-RF-CI-NO-PROTO` | — | P1-3 |
| S481 | 永続化 | `G-RF-PROTO-GONE` | タグを 1 つ前のコミットに打つ | P1-2 |
| S482 | データの完全性 | `G-RF-PROTO-GONE`（`goals.sh` の 4 つの関数） | `G-CORE-FLAGS` の道筋を `proto/scripts/` に戻す | P1-1 |
| S483 | 正常系 | `G-CORE-FLAGS` | 移した `check-flags.py` の根の求め方を戻す | P1-2 |
| S483 | 正常系 | `G-CORE-PURE` | 移した `check-pure.sh` の根の求め方を戻す | P1-2 |
| S483 | 正常系 | `G-CORE-NOGLOBAL` | 移した `check-noglobal.py` の根の求め方を戻す | P1-2 |
| S483 | 正常系 | `G-CORE-DIAG-REGISTRY` | `check-diag-registry.sh` の既定の道筋を `proto/src/` に戻す | P1-2 |
| S484 | データの完全性 | `G-REPO-SELFTESTS`、`sh/test-check-flags.sh`・`sh/test-check-pure.sh`・`sh/test-check-noglobal.sh`・`sh/test-check-diag-registry.sh`・`sh/test-check-readme-example.sh` | `selftests.txt` から移した 1 行を消す | P1-1 |
| S485 | 文書との一致 | `G-RF-README-NO-PROTO` | 節の見出しを戻す | P1-2 |
| S486 | 文書との一致 | `G-REPO-README-LICENSE` | ライセンスの表の行に `proto/` を戻す | P1-2 |
| S487 | データの完全性 | `G-PLAN-TESTS-EXIST` | `retired-tests.txt` から `golden/*` の 1 行を消す | P1-2 |
| S488 | 文書との一致 | `G-RF-PROTO-GONE`（報告書の冒頭） | 冒頭の注記を消す | P1-2 |
| S489 | 正常系 | `sh/test-check-readme-example.sh`（`--bin` と `--ozen` に別のスクリプトを渡し、`run:` の無い塊が `--ozen` で走る場合を足す） | 既定の実行ファイルを `--bin` にする | P1-1 |
| S490 | 効かない条件 | `G-RF-PROTO-GONE` | `check-selftests.sh` の対象に `proto/scripts` を戻す | P1-2 |
| S491 | データの完全性 | `G-RF-NO-PROTO-GOALS` | `goal_G_PROTO_BUILD` を戻す | P1-2 |
| S492 | 正常系 | `cgolden/quantity/count-times-mass` | `typed_qty` で Count の次元を付けたままにする | P2-1 |
| S493 | 実際の経路 | `cgolden/quantity/count-options`（`-s 2`） | 条件の値にだけ Count の次元を付ける | P2-1 |
| S494 | 正常系 | `cgolden/quantity/count-kitchen` | 台所の項目にだけ Count の次元を付ける | P2-1 |
| S495 | 正常系 | `cgolden/quantity/count-compare` | 比較の評価のときだけ、Count の型の値に Count の次元を付け直す | P2-1 |
| S496 | 境界・上限 | `cgolden/quantity/count-with-pcs` | `pcs` 付きの値だけ Count の次元を残す | P2-1 |
| S497 | 正常系 | `cgolden/quantity/ingredient-dimension` | 次元の検査を外す | P2-2 |
| S498 | データの完全性 | `cgolden/quantity/ingredient-dimension` | 位置を材料の名前にする | P2-2 |
| S499 | 効かない条件 | `cgolden/quantity/ingredient-dimension-ok` | 体積をエラーにする | P2-2 |
| S500 | 正常系 | `cgolden/menus/make-bare` | 名前だけの `make` を `unknown()` に戻す | P2-3 |
| S501 | 正常系 | `cgolden/menus/make-servings` | `eval_make` で人数を戻したままにする | P2-4 |
| S502 | 時間と順序 | `cgolden/serve/serve-within-backward` | 後ろ向きで release を写さない（今の挙動） | P2-5 |
| S503 | 効かない条件 | `cgolden/serve/serve-within-backward` | 守れていても `SCH010` を出す | P2-5 |
| S504 | 正常系 | `cgolden/pipe/bare-name` | 右辺が名前のとき引数を付けない | P3-1 |
| S505 | 正常系 | `cgolden/pipe/first-arg` | 左辺を最後の位置の引数に入れる | P3-1 |
| S506 | 時間と順序 | `cgolden/pipe/chain` | 右から結合する | P3-1 |
| S507 | 境界・上限 | `cgolden/pipe/precedence` | `pipe` を `sum` より強くする | P3-1 |
| S508 | 効かない条件 | `cgolden/pipe/bad-target-quantity` | 右辺に任意の式を許す | P3-1 |
| S509 | 効かない条件 | `cgolden/pipe/bad-target-list` | 右辺に任意の式を許す | P3-1 |
| S510 | 効かない条件 | `cgolden/pipe/bad-target-make` | 右辺に `make` を許す | P3-1 |
| S511 | 正常系 | `cgolden/pipe/macro` | マクロの呼び出しで左辺を渡さない | P3-1 |
| S512 | 境界・上限 | `cgolden/pipe/named-only` | 左辺を名前付きの引数の後ろに入れる | P3-1 |
| S513 | 正常系 | `cgolden/pipe/unattended` | 注釈を最初の段に付ける | P3-1 |
| S514 | 効かない条件 | `cgolden/pipe/unattended-builtin` | 組み替えた呼び出しでは `check_targets` を呼ばない | P3-1 |
| S515 | 前提の固定 | `G-RF-TYPEARGS-STABLE` | `\|` の直後の空白を読み飛ばして `>` まで 1 つのトークンにする | P3-1 |
| S516 | データの完全性 | `cgolden/pipe/ast-rules` | `mark(OZNI_R_pipe)` を外す | P3-1 |
| S517 | データの完全性 | `G-CORE-GRAMMAR` | `grammar.ebnf` から規則 `pipe` を消す | P3-1 |
| S518 | 境界・上限 | `cgolden/pipe/ast` | 組み替えた呼び出しの span を右辺の始まりからにする | P3-1 |
| S519 | 前提の固定 | `cunit/span_nesting` | 組み替えた呼び出しの span の終わりを左辺の終わりにする | P3-1 |
| S520 | 副作用とコスト | `cgolden/pipe/nodes`（1 つ足りない）・`cgolden/pipe/nodes-exact`（ちょうど） | 右辺が名前のとき既存の Name のノードを Call に書き換え、ノードを増やさない | P3-1 |
| S521 | データの完全性 | `G-RF-FUZZ-DICT` | 辞書から `"\|>"` を消す | P3-1 |
| S522 | 正常系 | `cgolden/syntax/omit-parens` | `()` の無い宣言では引数の既定値を束縛しない | P3-2 |
| S523 | データの完全性 | `G-RF-NO-EMPTY-PARENS`、`sh/test-check-test-names.sh` | `tests/` に `recipe salad() {` を 1 つ戻す | P3-2・P5-7 |
| S524 | 文書との一致 | `G-RF-CONCEPT-PARENS` | concept の例に `make cookedRice()` を戻す | P9-1 |
| S525 | 正常系 | `cgolden/quantity/per-serving` | 人数を掛けない | P4-1 |
| S526 | 効かない条件 | `cgolden/quantity/fixed-amount` | 印の無い量にも人数を掛ける | P4-1 |
| S527 | 正常系 | `cgolden/quantity/per-serving-param` | 料理の引数を見ない | P4-1 |
| S528 | 正常系 | `cgolden/quantity/per-serving-options` | 条件の値を見ない | P4-1 |
| S529 | 効かない条件 | `cgolden/quantity/servings-unknown` | 人数が無いとき 1 を使う | P4-1 |
| S530 | データの完全性 | `cgolden/quantity/servings-unknown` | 重さを警告にする | P4-1 |
| S531 | 効かない条件 | `cgolden/quantity/per-serving-misplaced` | 引数の既定値の中でも `per serving` を読む | P4-1 |
| S532 | 正常系 | `cgolden/menus/make-servings-per-serving` | `make` の人数を `per serving` に渡さない | P4-1 |
| S533 | データの完全性 | `cgolden/quantity/per-serving-json`（`--format json --emit ir`） | JSON に人数を掛ける前の量を書く | P4-1 |
| S534 | データの完全性 | `cgolden/quantity/per-serving-json` | 人数の値に Count の次元を残して掛ける | P4-1 |
| S535 | データの完全性 | `G-CORE-DIAG-REGISTRY`・`G-CORE-DIAG-APPEND`・`G-STD-PHRASES` | `phrases/ja.ftl` から `diag-servings-unknown` を消す | P4-1 |
| S536 | データの完全性 | `G-RF-FUZZ-DICT` | 辞書から `"serving"` を消す | P4-1 |
| S537 | データの完全性 | `G-RF-PATHS` | 書き直しの中で golden を 1 つ改名する | P5-9 |
| S538 | 正常系 | `G-RF-EQUIVALENCE`、`sh/test-check-rewrite-equivalence.sh` | 書き直した 1 つの golden で診断を 1 つ消す | P5-0〜P5-7 |
| S538 | 前提の固定 | `G-RF-EQUIVALENCE`（2 つのタグの時点の実行ファイルで比べる） | 比べる実行ファイルを HEAD のものにする（Phase 6 の後に落ちる） | P5-9 |
| S539 | データの完全性 | `G-RF-EQUIVALENCE`（records） | 書き直した golden を指すロードマップの行の記録を 1 つ §9 から消す | P5-0〜P5-7 |
| S540 | データの完全性 | `G-RF-NAMES`、`sh/test-check-test-names.sh` | `recipe r` を 1 つ戻す | P5-0〜P5-7 |
| S541 | 効かない条件 | `G-RF-NAMES`（`unlisted exceptions`） | 例外のリストから理由を消す | P5-0 |
| S542 | 副作用とコスト | `G-RF-SEPARATION` | 期待値と `sema.c` を 1 つのコミットにする | P9-5 |
| S543 | 境界・上限 | `G-RF-LIMIT-PAIRS`、`sh/test-check-limit-pairs.sh` | `limits/nodes-exact` の入力のノードを 1 つ減らす | P5-2 |
| S544 | 前提の固定 | `G-CORE-GRAMMAR` | 規則 `tuple_binding` を使う唯一の golden からその構文を消す | P5-9 |
| S545 | 前提の固定 | `G-CORE-DIAG-REGISTRY` | `LIN004` を出す唯一の golden からその診断を消す | P5-9 |
| S546 | 前提の固定 | `G-CORE-GOLDEN` | 書き直した 1 つの `.out` の位置を 1 列ずらす | P5-1〜P5-7 |
| S547 | 前提の固定 | `G-CORE-UNIT` | `core/tests/unit` の C の文字列の宣言の名前と、期待する名前を食い違わせる | P5-7 |
| S548 | 前提の固定 | `G-CORE-OOM` | `diag/out-of-memory` の確保の番号を 1 増やす | P5-2 |
| S549 | 前提の固定 | `G-CORE-PROPS` | 書き直した golden に `propsOptions` という名前の宣言を足す（`monotonic` とぶつかる） | P5-9 |
| S550 | 前提の固定 | `G-SCHED-CHECK` | 書き直した段取りの golden で、洗いの行を 1 つ消す | P5-5 |
| S551 | プラットフォームの経路 | `G-PLAT-LINUX`・`G-PLAT-WASM` | `bindings/inputs.txt` の 1 件の期待を変える | P5-7 |
| S552 | データの完全性 | `G-RF-GOLDEN-COUNT` | 新しい golden を 1 つ消す | P9-5 |
| S616 | 正常系 | `G-RF-SAMPLES-PIPE`、`sh/test-check-test-names.sh`（`--samples-pipe` の場合） | `samples/T2/` のパイプを `let` の連鎖に戻す | P5-8 |
| S617 | 前提の固定 | `G-RF-SAMPLES-SAME`、`sh/test-check-rewrite-equivalence.sh`（`--samples` の場合） | `curryRice` の `per serving` の量を 1 g ずらす | P5-8 |
| S618 | 前提の固定 | `G-RF-SAMPLES-SAME` | `menuB` の 1 つの動作の時間の引数を変える | P5-8 |
| S619 | 前提の固定 | `G-RF-SAMPLES-EXPECT` | `samples/T1/expect.yaml` の 1 行を書き換える | P5-9 |
| S620 | 実際の経路 | `G-RF-SAMPLES-SAME`（render-all） | `phrases` が名前で参照する束縛をパイプに畳む | P5-8 |
| S553 | 正常系 | `cgolden/linear/unused-let` | 判定を外す | P6-2 |
| S554 | データの完全性 | `cgolden/linear/unused-let` | 登録簿の重さを情報にする | P6-2 |
| S555 | データの完全性 | `cgolden/linear/unused-let` | 位置を式にする | P6-2 |
| S556 | 正常系 | `cgolden/linear/unused-expr` | 束縛の無い値を飛ばす | P6-2 |
| S557 | 正常系 | `cgolden/linear/unused-ingredient` | 材料を追跡に入れない | P6-2 |
| S558 | 効かない条件 | `cgolden/linear/unused-store-discard` | `store`・`discard` の後も報告する | P6-2 |
| S559 | 効かない条件 | `cgolden/linear/unused-part` | 部位も報告する | P6-2 |
| S560 | 効かない条件 | `cgolden/linear/unused-last-tuple` | 最後の組の値を報告する | P6-2 |
| S561 | 効かない条件 | `cgolden/linear/unused-in-make` | `quiet` でも報告する | P6-2 |
| S562 | 正常系 | `cgolden/linear/unused-in-make-alone` | 献立から呼ばれる料理を単体でも報告しない | P6-2 |
| S563 | 効かない条件 | `cgolden/linear/unused-composite` | `steps` の中の値も追跡する | P6-2 |
| S564 | 効かない条件 | `cgolden/linear/unused-divided` | 割り算の残りを報告する | P6-2 |
| S565 | 正常系 | `cgolden/linear/unused-allow` | 重さの指定を見ない | P6-2 |
| S566 | データの完全性 | `G-CORE-DIAG-REGISTRY`・`G-CORE-DIAG-APPEND`・`G-STD-PHRASES` | `phrases/en.ftl` から `diag-unused-value` を消す | P6-3 |
| S567 | 前提の固定 | `G-RF-SAMPLES-UNUSED` | `LIN005` を最後の値にも出す | P6-3 |
| S568 | 正常系 | `cgolden/sched/preheat-basic` | 予熱を入れない | P7-2 |
| S569 | 正常系 | `cgolden/sched/preheat-basic`（オーブンが 2 台ある台所） | 予熱を最初の個体に入れる | P7-2 |
| S570 | 時間と順序 | `cgolden/sched/preheat-basic` | 予熱を作業の始まりから置く | P7-2 |
| S571 | 時間と順序 | `cgolden/sched/preheat-lookahead` | 予熱を作業が ready になった時刻に始める | P7-2 |
| S572 | 正常系 | `cgolden/sched/preheat-basic` | 所要時間を固定の 10 分にする | P7-2 |
| S573 | 副作用とコスト | `cgolden/sched/preheat-unattended` | 予熱に人を割り当てる | P7-2 |
| S574 | 副作用とコスト | `cgolden/sched/preheat-occupies` | 予熱と使う作業の間に個体を空ける | P7-2 |
| S575 | 効かない条件 | `cgolden/sched/preheat-reuse` | 毎回予熱を入れる | P7-2 |
| S576 | 境界・上限 | `cgolden/sched/preheat-hotter` | 高い温度でも入れ直さない | P7-2 |
| S577 | 正常系 | `cgolden/sched/preheat-arg-temp` | 温度を式でなく数のリテラルだけ読む | P7-2 |
| S578 | 時間と順序 | `cgolden/sched/preheat-backward` | 後ろ向きで予熱の release を写さない | P7-2 |
| S579 | 効かない条件 | `cgolden/sched/preheat-max-temp` | `heated` を `req_met` で見ない | P7-2 |
| S580 | 境界・上限 | `cgolden/sched/preheat-max-temp-equal` | 比較を `<` でなく `<=` にする | P7-2 |
| S581 | 効かない条件 | `cgolden/sched/preheat-no-max-temp` | `maxTemp` が無いとき満たさないことにする | P7-2 |
| S582 | 前提の固定 | `cgolden/sched/preheat-ir`（台所を注入した `debug ir`） | 予熱を IR のノードにする | P7-2 |
| S583 | データの完全性 | `cgolden/sched/preheat-basic` | 行の語を `wash` にする | P7-2 |
| S584 | データの完全性 | `cgolden/sched/preheat-json` | `heats` を出さない | P7-3 |
| S585 | データの完全性 | `cgolden/sched/preheat-json` | `needs` を出さない | P7-3 |
| S586 | データの完全性 | `G-API-SCHEMA` | スキーマの作業の `kind` の列挙から `preheat` を消す | P7-3 |
| S587 | 前提の固定 | `G-SCHED-CHECK`、`sh/test-schedcheck.sh`（`bad-preheat-*` の fixture） | `schedcheck` の予熱の検査を外す | P7-3 |
| S588 | 文書との一致 | `rtest/schedule-text`、`G-STD-PHRASES` | `ja.ftl` から予熱のキーを消す | P7-4 |
| S589 | 実際の経路 | `rtest/recipe-schedule`（`cgolden/sched/preheat-json` の出力を入力にする） | 段取り表で `preheat` の作業を飛ばす | P7-4 |
| S590 | データの完全性 | `G-RF-STD-PREHEAT`、`sh/test-check-std-preheat.sh` | `OvenRange` から `preheatTime` を消す | P7-1 |
| S591 | データの完全性 | `G-STD-PROVENANCE`・`G-STD-SOURCES` | `preheatTime` から `@source(provisional)` を消す | P7-1 |
| S592 | 時間と順序 | `cgolden/serve/use-within-basic` | release を付けない | P8-2 |
| S593 | 境界・上限 | `cgolden/serve/use-within-two-consumers` | 最初の消費の作業だけを見る | P8-2 |
| S594 | 正常系 | `cgolden/serve/use-within-missed` | 警告を出さない | P8-2 |
| S595 | データの完全性 | `cgolden/serve/use-within-missed` | 位置を値の名前にする | P8-2 |
| S596 | 時間と順序 | `cgolden/serve/use-within-backward` | 後ろ向きで release を写さない | P8-2 |
| S597 | 効かない条件 | `cgolden/serve/use-within-on-ingredient` | 材料に付けても通す | P8-2 |
| S598 | 正常系 | `cgolden/serve/use-within-expr` | 式の文の注釈を読まない | P8-2 |
| S599 | 効かない条件 | `cgolden/serve/use-within-no-consumer` | 消費の作業が無いとき `SCH011` を出す | P8-2 |
| S600 | データの完全性 | `cgolden/serve/use-within-json` | `limits` を出さない | P8-3 |
| S601 | 前提の固定 | `G-SCHED-CHECK`、`sh/test-schedcheck.sh`（`bad-use-within` の fixture） | `schedcheck` の `limits` の検査を外す | P8-3 |
| S602 | 前提の固定 | `cgolden/serve/use-within-ir` | 注釈を IR に入れる | P8-2 |
| S603 | データの完全性 | `G-CORE-DIAG-REGISTRY`・`G-CORE-DIAG-APPEND`・`G-STD-PHRASES`・`G-TYPE-SEVERITY-TABLE` | `diag-names.txt` から `SCH:useWithinMissed` を消す | P8-2 |
| S604 | 文書との一致 | `G-RF-CONCEPT` | concept §4.4 から `per serving` と `\|>` を消す | P9-1 |
| S605 | 文書との一致 | `G-RF-CONCEPT` | concept §4.1 から `unusedValue` を消す | P9-1 |
| S606 | 文書との一致 | `G-RF-CONCEPT` | concept §4.2 から `preheat` を消す | P9-1 |
| S607 | 文書との一致 | `G-RF-CONCEPT` | concept §4.5 から `useWithin` を消す | P9-1 |
| S608 | 文書との一致 | `G-RF-CONCEPT` | concept §6.1 から `useWithinMissed` を消す | P9-1 |
| S609 | 文書との一致 | `G-RF-README-PIPE`・`G-RF-README-EXAMPLE` | README のパイプの例の期待の出力を 1 文字変える | P9-2 |
| S610 | 前提の固定 | `G-REPO-VERSION` | `VERSION` だけを `0.2.0` にする | P9-3 |
| S613 | 文書との一致 | `G-RF-CLAUDEMD` | `CLAUDE.md` から本ファイルの名前を消す | P0-3 |
| S614 | 永続化 | `prop/bundle_roundtrip`（`cgolden/bundle/new-syntax` を含む） | `ozen bundle` でパイプの式を書き写すときに `\|>` を落とす | P4-1 |
| S615 | 永続化 | `G-RF-TAGS` | タグ `rewrite-end` を消す | P5-9 |

網羅の検査は `G-RF-COVERAGE`（P0-1 の後と P9-4 で実行する）。

独立した監査（サブエージェント）の指摘のうち、取り込まなかったものと理由:

- §8 の「書き直したレシピが料理として自然か」のうち、「最後の作業が加熱か盛り付けか」を機械で確かめる案 — D50 で「動作の並びを保つ」としたので、元のテストが切っただけで終わる場合は、そのまま保つことがある。自然かどうかの判断は §8 に残す。

---

## 6. フェーズとタスク

表記:

- **完了条件** は §5 のゴール ID か、合否が一意なコマンド。§5 の「手元で回す」前提ゴールは各タスクに暗黙で含まれる。CI の前提ゴールは、push するゲートの完了条件にだけ含める。
- 状態の報告は環境ごとに書く（「Linux: 4 構成で build と golden 合格。WASM: 合格。iOS・Android: push の後の CI で未確認」）。
- コミットは論理単位。期待値のコミットと実装のコミットは分ける（`G-RF-SEPARATION`）。push は確認を取らずに行う（2026-10-30 のユーザーの指示）が、その前に毎回 `G-REPO-NOREPLY` を回し、push した範囲を報告する。
- 各タスクの「わざと壊す確認」は、網羅表の方法で壊すとそのテストが落ち、戻すと合格することを確かめ、§9 に `- わざと壊す確認: ` の行で記録する。
- red-first の確認は `scripts/check-red-first.sh "$RF"` の出力が `0 not red` であること（`G-REPO-RED-FIRST` を本計画の範囲で回したもの）。

### Phase 0 — 決定と準備（機能のコードの変更なし）

| ID | 内容 | 完了条件 |
| --- | --- | --- |
| P0-1 | §2 の D40〜D56 をユーザーと確定し、本ファイルに書き込む | `G-RF-DECISIONS`、`G-RF-COVERAGE` |
| P0-2 | 着手点の `09e05de` と本ファイルを push する。`main` にタグ `refresh-start` を打つ。基準値（golden の入口 N0、文法の規則の数、手元の前提ゴールの合否）を §9 に記録する | `git rev-parse --verify refresh-start` が成功し、§5 の手元の前提ゴールがすべて合格 |
| P0-3 | `scripts/goals.sh` に `PLAN_RF` と `goal_G_RF_*` を足し。`CLAUDE.md` の Plan の節に本ファイルを足す | `scripts/goals.sh --list` の `G-RF-` で始まる ID が 28、`G-RF-CLAUDEMD`、`G-RF-COVERAGE`、`G-REPO-CLAUDEMD`。S613 のわざと壊す確認 |

### Phase 1 — 試作の削除（D52）

| ID | 内容 | 完了条件 |
| --- | --- | --- |
| P1-1 | 4 つの検査スクリプトと README の例の検査、その自己テストを `scripts/` に移す。根の求め方と既定の道筋を直す。`goals.sh` の 4 つの関数と `selftests.txt` を直す。README の例の検査の既定を `--ozen` にし、自己テストに `run:` の無い塊の場合を足す | `G-CORE-FLAGS`・`G-CORE-PURE`・`G-CORE-NOGLOBAL`・`G-CORE-DIAG-REGISTRY`・`G-REPO-SELFTESTS`・`G-RF-README-EXAMPLE`。S482・S484・S489 のわざと壊す確認 |
| P1-2 | `check-selftests.sh`・`check-spdx.sh`・`check-plan-tests.sh`・`test-check-plan-tests.sh`・`test-check-spdx.sh` から `proto/` を外す。タグ `proto-final` を打ち、`proto/` と `ios-proto.yml` と手元の試作のビルドを消す。`G-PROTO-*` を `goals.sh` から外し、`retired-tests.txt` に試作のテストの名前を足す。README の節と表、`G-REPO-README-LICENSE` の文字列、報告書の冒頭を直す | `G-RF-PROTO-GONE`・`G-RF-NO-PROTO-GOALS`・`G-RF-README-NO-PROTO`・`G-PLAN-TESTS-EXIST`・`G-REPO-README-LICENSE`・`G-REPO-SELFTESTS`・`G-REPO-SPDX`。S479〜S481・S483・S485〜S488・S490・S491 のわざと壊す確認 |
| P1-3 | ゲート。push する | 手元の前提ゴールと Phase 1 のゴール、`G-PLAT-CI`、`G-RF-CI-NO-PROTO` |

### Phase 2 — 不具合の修正（D44・D45）

| ID | 内容 | 完了条件 |
| --- | --- | --- |
| P2-1 | 先に `cgolden/quantity/count-*`（5 個）を書く。Count 型の値を無次元にする。既存の golden で Count の値の表示が変わるものの期待値を、別のコミットで手で直す（数を §9 に記録） | `G-CORE-GOLDEN`、red-first の確認。S492〜S496 のわざと壊す確認 |
| P2-2 | 先に `cgolden/quantity/ingredient-dimension*` を書く。材料の量の次元を検査する | `G-CORE-GOLDEN`、`G-SAMPLE-CHECK`、red-first の確認。S497〜S499 のわざと壊す確認 |
| P2-3 | 先に `cgolden/menus/make-bare` を書く。`make 名前` を呼び出しとして評価する | `G-CORE-GOLDEN`、red-first の確認。S500 のわざと壊す確認 |
| P2-4 | 先に `cgolden/menus/make-servings` を書く。`make` で渡した人数を料理の人数にする | `G-CORE-GOLDEN`、red-first の確認。S501 のわざと壊す確認 |
| P2-5 | 先に `cgolden/serve/serve-within-backward` を書く。後ろ向きの release を反転した時計に写す | `G-CORE-GOLDEN`、`G-SCHED-CHECK`、red-first の確認。S502・S503 のわざと壊す確認 |
| P2-6 | ゲート | 手元の前提ゴールと Phase 2 のゴール |

### Phase 3 — 記法（D42〜D44）

| ID | 内容 | 完了条件 |
| --- | --- | --- |
| P3-1 | 先に `cgolden/pipe/*`（15 個）を書く。字句・構文・`grammar.ebnf`・`rules.def`・ファズの辞書 | `G-CORE-GOLDEN`、`G-CORE-GRAMMAR`、`G-CORE-UNIT`、`G-CORE-FUZZ`、`G-RF-FUZZ-DICT`、red-first の確認。S504〜S514・S516〜S521 のわざと壊す確認（S515 は P5-0 で `G-RF-TYPEARGS-STABLE` が回せるようになってから行う） |
| P3-2 | 先に `cgolden/syntax/omit-parens` を書く。`scripts/check-test-names.py` の `--empty-parens` と自己テスト、`scripts/empty-parens-exceptions.txt` を作る。`renderers/test/inputs/`・`bindings/inputs/`・README の空の `()` を消す（`tests/`・`core/tests/` は Phase 5 で消す） | `G-CORE-GOLDEN`、`G-REND-TESTS`、`G-PLAT-LINUX`、`G-RF-README-EXAMPLE`、`python3 scripts/check-test-names.py --empty-parens --exceptions scripts/empty-parens-exceptions.txt bindings/inputs renderers/test/inputs README.md` が `0 found`。S522 のわざと壊す確認 |
| P3-3 | ゲート | 手元の前提ゴールと Phase 3 のゴール |

### Phase 4 — 1 人あたりの量（D46）

| ID | 内容 | 完了条件 |
| --- | --- | --- |
| P4-1 | 先に `cgolden/quantity/per-serving*`・`fixed-amount`・`servings-unknown`、`cgolden/menus/make-servings-per-serving`、`cgolden/bundle/new-syntax` を書く。構文・評価・`TYP020` の登録・`phrases`・ファズの辞書 | `G-CORE-GOLDEN`、`G-CORE-GRAMMAR`、`G-CORE-DIAG-REGISTRY`、`G-CORE-DIAG-APPEND`、`G-STD-PHRASES`、`G-API-SCHEMA`、`G-CORE-PROPS`、`G-RF-FUZZ-DICT`、red-first の確認。S525〜S536・S614 のわざと壊す確認 |
| P4-2 | ゲート | 手元の前提ゴールと Phase 4 のゴール |

### Phase 5 — テストの書き直し（D50・D51）

| ID | 内容 | 完了条件 |
| --- | --- | --- |
| P5-0 | `main` にタグ `rewrite-start` を打つ。`scripts/check-rewrite-equivalence.py`（2 つの時点を一時の worktree でビルドして比べ、書き直したテストを指すロードマップ §5.3 の行の記録が §9 にあるかを数える）と、`check-test-names.py` の名前の検査、それぞれの自己テスト、`scripts/test-names-exceptions.txt` を作り、`goals.sh` の関数に中身を入れる | `G-REPO-SELFTESTS`、`sh/test-check-rewrite-equivalence.sh`・`sh/test-check-test-names.sh` が合格、`G-RF-TYPEARGS-STABLE`。S515・S538・S541 のわざと壊す確認 |
| P5-1 | 書き直し: `syntax`・`decl`・`typeargs`・`names`・`modules`・`packages`・`std`（宣言・名前・モジュール） | 下の「書き直しの完了条件」（対象: この 7 フォルダ） |
| P5-2 | 書き直し: `dim`・`eval`・`macro`・`limits`・`limits-shared`・`diag`（値・評価・上限）。上限・燃料・確保は数え直す。`scripts/check-limit-pairs.py` と自己テストを作る | 同（6 フォルダ）、`G-RF-LIMIT-PAIRS`、`G-CORE-OOM`。S543・S548 のわざと壊す確認 |
| P5-3 | 書き直し: `state`・`mix`・`parts`・`linear`・`safety`・`actions`（食材の状態と線形） | 同（6 フォルダ） |
| P5-4 | 書き直し: `ir`・`levels`・`kitchen`・`vessels`・`serve`・`menus`（IR・台所・献立）。本計画の Phase 2〜4 で足した golden は書き直さない | 同（6 フォルダ） |
| P5-5 | 書き直し: `sched`・`wash`・`cleanup`（段取り） | 同（3 フォルダ）、`G-SCHED-CHECK`。S550 のわざと壊す確認 |
| P5-6 | 書き直し: `api`・`bundle`（API と束ねるもの） | 同（2 フォルダ）、`G-BUNDLE-STABLE`、`G-API-SCHEMA` |
| P5-7 | 書き直し: `tests/cli`（heredoc と `lib.sh` の雛形）、`core/tests/unit` の C の文字列、`bindings/inputs/`、`tests/samples/test-check.sh` | `G-CLI-TESTS`、`G-CORE-UNIT`、`G-PLAT-LINUX`、`G-PLAT-WASM`、`G-RF-NAMES`、`G-RF-NO-EMPTY-PARENS`。S523・S547・S551 のわざと壊す確認 |
| P5-8 | サンプルの書き直し（D54）: `samples/*` の `let` の連鎖をパイプに、人数に比例する材料を `per serving` にする。`check-rewrite-equivalence.py` の `--samples` と `check-test-names.py` の `--samples-pipe`、その自己テスト、`scripts/samples-pipe-exceptions.txt` を作る | `G-RF-SAMPLES-PIPE`、`G-RF-SAMPLES-SAME`（`--head HEAD`）、`G-SAMPLE-CHECK`、`G-SAMPLE-STD-ONLY`、`G-REND-TESTS`、`G-REND-HTML`、`G-REND-PHRASES-RENDER`、`G-REPO-SPDX`、`G-CORE-PROPS`（`bundle_roundtrip`）。S616〜S618・S620 のわざと壊す確認 |
| P5-9 | `main` にタグ `rewrite-end` を打つ。ゲート | `G-RF-PATHS`、`G-RF-EQUIVALENCE`（`--head rewrite-end`）、`G-RF-SAMPLES-SAME`（`--head rewrite-end`）、`G-RF-SAMPLES-EXPECT`、`G-RF-NAMES`、`G-RF-NO-EMPTY-PARENS`、`G-RF-SEPARATION`、S544〜S551 のゴール、`refresh-start`・`proto-final`・`rewrite-start`・`rewrite-end` の 4 つのタグがあること（`G-RF-TAGS`）。S537・S538（前提の固定）・S544・S545・S549・S615・S619 のわざと壊す確認 |

書き直しの完了条件（P5-1〜P5-6）: 対象のフォルダで `G-RF-EQUIVALENCE`（`--head HEAD`）が `0 changed results` で、対象のフォルダのテストを指すロードマップ §5.3 の行（「わざと壊す方法」を持つもの）すべてについて、わざと壊す確認をやり直した `- 書き直しのわざと壊す確認: ` の記録が §9 にある（D51）。`G-RF-NAMES` が対象のフォルダで `0 one-letter`。`G-CORE-GOLDEN`（4 構成）・`G-CORE-GRAMMAR`・`G-CORE-DIAG-REGISTRY`・`G-CORE-UNIT`・`G-CORE-OOM`・`G-CORE-PROPS` が合格。`G-RF-SEPARATION` が `0 mixed`。S539・S540・S546 のわざと壊す確認を、最初に行う P5-1 で 1 回ずつ記録する。

### Phase 6 — 使われない値（D47）

| ID | 内容 | 完了条件 |
| --- | --- | --- |
| P6-1 | 先に `cgolden/linear/unused-*`（11 個）を書く。既存の golden のうち `LIN005` が出るものの期待値を、別のコミットで手で直す（数を §9 に記録） | red-first の確認 |
| P6-2 | 追跡の表と `finish_dish` の判定を実装する | `G-CORE-GOLDEN`、`G-CORE-OOM`、`G-CORE-PROPS`。S553〜S565 のわざと壊す確認 |
| P6-3 | 登録簿・`diag-names.txt`・`phrases`・`api/local-not-listed` の期待値、レンダラーの引数の種類。サンプルで `LIN005` が出ないことを確かめる（出たら、サンプルの誤りか規則の誤りかをユーザーに報告する。§9 にサンプルと書き直した golden の `LIN005` の数を記録する） | `G-CORE-DIAG-REGISTRY`、`G-CORE-DIAG-APPEND`、`G-STD-PHRASES`、`G-SAMPLE-CHECK`、`G-REND-TESTS`、`G-RF-SAMPLES-UNUSED`。S566・S567 のわざと壊す確認 |
| P6-4 | ゲート | 手元の前提ゴールと Phase 6 のゴール |

### Phase 7 — 予熱（D48）

| ID | 内容 | 完了条件 |
| --- | --- | --- |
| P7-1 | 先に `schedcheck` の予熱の検査と `bad-preheat-*` の fixture を足す。`cgolden/sched/preheat-*`（13 個）を書く（段取りを手で組み、`schedcheck` で違反 0 を確かめる）。std の `Oven`・`OvenRange` に `preheatTime` と出どころ、調べた記録（`spec/sources/search-log.yaml`）を足し、`api/local-not-listed` の期待値を直す。`scripts/check-std-preheat.py` と自己テストを作る | red-first の確認、`G-RF-STD-PREHEAT`、`G-STD-PROVENANCE`、`G-STD-SOURCES`。S590・S591 のわざと壊す確認 |
| P7-2 | `sched.c`・`sched.h`・`sema.c` の予熱（先読み・後ろ向き・`maxTemp`）。挿入した作業を kind で分けていない 6 か所を直す | `G-CORE-GOLDEN`、`G-SCHED-CHECK`、`G-CORE-PROPS`。S568〜S583 のわざと壊す確認 |
| P7-3 | 段取りの JSON・スキーマ・`schedcheck` | `G-API-SCHEMA`、`G-SCHED-CHECK`、`G-REPO-SELFTESTS`。S584〜S587 のわざと壊す確認 |
| P7-4 | レンダラーと `phrases`。`rtest/recipe-schedule` に予熱を含む JSON の入力を足す | `G-REND-TESTS`、`G-REND-HTML`、`G-STD-PHRASES`。S588・S589 のわざと壊す確認 |
| P7-5 | ゲート | 手元の前提ゴールと Phase 7 のゴール |

### Phase 8 — 作ってから使うまでの期限（D49）

| ID | 内容 | 完了条件 |
| --- | --- | --- |
| P8-1 | 先に `schedcheck` の `limits` の検査と `bad-use-within` の fixture を足す。`cgolden/serve/use-within-*`（9 個）を書く（段取りは手で組む） | red-first の確認 |
| P8-2 | 注釈の解釈、release の反復、`SCH011` の登録と重さの表、後ろ向き | `G-CORE-GOLDEN`、`G-SCHED-CHECK`、`G-CORE-DIAG-REGISTRY`、`G-CORE-DIAG-APPEND`、`G-TYPE-SEVERITY-TABLE`、`G-CORE-PROPS`。S592〜S599・S602・S603 のわざと壊す確認 |
| P8-3 | 段取りの JSON の `limits`・スキーマ・`schedcheck`・`phrases` | `G-API-SCHEMA`、`G-SCHED-CHECK`、`G-STD-PHRASES`。S600・S601 のわざと壊す確認 |
| P8-4 | ゲート | 手元の前提ゴールと Phase 8 のゴール |

### Phase 9 — 文書と仕上げ

| ID | 内容 | 完了条件 |
| --- | --- | --- |
| P9-1 | concept の §4.1・§4.2・§4.4・§4.5・§6.1 を書き足し、例の空の `()` を消す（D53） | `G-RF-CONCEPT`、`G-RF-CONCEPT-PARENS`。S524・S604〜S608 のわざと壊す確認 |
| P9-2 | README にパイプと `per serving` の例を足す | `G-RF-README-PIPE`、`G-RF-README-EXAMPLE`。S609 のわざと壊す確認 |
| P9-3 | D55 のとおり版を `0.1.0` のまま据え置いていることを確かめる | `G-REPO-VERSION`。S610 のわざと壊す確認 |
| P9-4 | 網羅の監査: §9 で増えた事実を §4 と網羅表に足す。§8 の各項目を実装後の知識で「コマンドで決められるか」もう一度判定し、決められるものはテストにする。独立したサブエージェントに、§1・§4・§8・網羅表と、追加・書き直したテストのソースを読ませ、テストの無い主張と、§8 にあるがコマンドで決められる項目を根拠付きで列挙させる。指摘は足すか、§5.3 の末尾に足さない理由を書く。§9 に `- 網羅の監査: 指摘 <k> 件、取り込み <m> 件` を書く | `G-RF-COVERAGE`、`G-RF-PLAN-TESTS`、`G-RF-AUDIT` |
| P9-5 | 最終確認。`main` で手元の前提ゴールと `G-RF-*` を回し、push し、CI の前提ゴールを回す。§8 をユーザーに渡す | 手元と CI の前提ゴール（`G-PLAT-IOS`・`G-PLAT-ANDROID` の digest を含む）と `G-RF-*` のすべてが合格。S542・S552 のわざと壊す確認。報告は環境ごと |

---

## 7. フェーズ間の依存

- Phase 0 はすべての前提。P0-1 が終わるまで P0-2 以降に進まない。
- **Phase 1（試作の削除）は Phase 2〜4 と独立**。触るファイル（`proto/`・`scripts/`・README・`.github/`）が、Phase 2〜4 の触る `core/`・`tests/golden/` と重ならない（README は P3-2 でも触るので、P3-2 は P1-2 の後）。別のセッションで並行してもよい。Phase 5 の前には終える。
- Phase 2 の P2-1〜P2-5 は互いに独立。
- Phase 3 は Phase 2 の後（同じ `sema.c` の近い所を触る）。
- Phase 4 は Phase 2（D45 の Count）と Phase 3（構文の変更が同じ `parse.c` に入る）の後。
- **Phase 5 は Phase 1〜4 の後**。書き直しはパイプ・`()` の省略・`per serving` と、直した次元を使う。P5-1〜P5-8 は互いに独立で、触るフォルダが重ならない。別のセッションに分けてよい（同じ `main` に入れるので、コミットの前に `git log` と `git status` を見る。R6）。P5-0 が先、P5-9 が最後。P5-8（サンプル）は、`samples/` を入力に使う `bundle_roundtrip` とレンダラーのテストに効くので、ほかの書き直しと同じ日に push しない（ゲートでまとめて確かめる）。
- Phase 6 は Phase 5 の後（D56）。
- Phase 7 と Phase 8 は互いに独立だが、どちらも `schedule_dish` の release の反復と `schedule_json` を触るので、続けて行う（並行しない）。
- Phase 9 は全フェーズの後。

---

## 8. ユーザーに引き渡す確認

Claude は機械的なゴールまでを担当する。以下はユーザーが確認する。各項目には、コマンドで決められない理由を付ける。理由が書けない項目は §5.3 でテストにする。

- 書き直したテストのレシピが、料理として自然か（材料の組み合わせ・量・手順の順番）。`tests/golden` のフォルダごとに数件ずつ見る — 機械で決められない理由: 料理として自然かどうかの正解は、仕様にも外部の資料にも無い
- パイプで書いたレシピが、`let` で書いたものより読みやすいか。README とテストの例を見て判断する — 機械で決められない理由: 読みやすさは人の判断
- 予熱の時間の暫定値（`preheatTime`）の値そのものが、家庭のオーブンとして妥当か（範囲で持つことと出どころの記録は `G-STD-PROVENANCE`・`G-STD-SOURCES` で確かめる） — 機械で決められない理由: 一次情報の調査で値が見つからないときは暫定の範囲で持ち、その幅が妥当かは人が判断する
- `LIN005` の重さ（警告）が、実際にレシピを書いたときに煩くないか（出た数は §9 に記録する） — 機械で決められない理由: 誤りとわざとの区別は書き手の意図による
- 書き直したサンプルで、`per serving` にした材料と総量のままにした材料の分け方が料理として自然か（量そのものが変わらないことは `G-RF-SAMPLES-SAME` で確かめる） — 機械で決められない理由: どの材料が人数に比例するかは料理の知識による判断
- concept に書き足した文章が、既存の文体と設計の考え方に合っているか（どの節に何が書いてあるかは `G-RF-CONCEPT` で確かめる） — 機械で決められない理由: 文体と考え方の一貫は人の判断

---

## 9. 着手時に確認して記録する事項

（各タスクで確認した結果を、ここに日付付きで追記する）

- 着手点の基準値（P0-2）: golden の入口 N0（2026-10-28 の調査では 363）、文法の規則の数（131）、手元の前提ゴールの合否
  - 2026-10-30、`ef81dec` で計測: N0 は 363（`scripts/golden-entries.sh tests/golden | wc -l`）。文法の規則は 131（`G-CORE-GRAMMAR` が「131 rules, 0 not covered」）
  - 同日、§5 の手元の前提ゴール 57 個と、`G-PROTO-IOS` を除く `G-PROTO-*` 23 個をまとめて回した。合格 79、不合格 1（`G-PROTO-REPORT`）
  - `G-PROTO-REPORT` の不合格の原因: 同じ実行で先に回った `G-PROTO-FUZZ` がファズを回し直し、手元の `build/fuzz/summary.txt` を「628402 execs」で上書きした。`docs/prototype-report.md` 78 行目は前回の「746382 execs」を引いているので、照合が合わない。実行回数は回すたびに変わるので、試作の不具合ではなく 2 つのゴールを続けて回す順番の問題。試作は P1 で消すので直さない。P1-1 で残す `docs/prototype-report.md` の数値はそのままにする
- push の扱い（2026-10-30）: ユーザーが「基本プッシュしていけ」と指示。確認を取らずに push し、その前に毎回 `G-REPO-NOREPLY` を回す
- わざと壊す確認の記録（各タスク）: 網羅表の「わざと壊す方法」を持つ行ごとに 1 行、行頭を「わざと壊す確認」に続けてコロンにした箇条書きで、テスト・壊し方・落ちた理由・戻して合格したことを書く
- 書き直しのわざと壊す確認の記録（P5-1〜P5-7）: 書き直したテストを指すロードマップ §5.3 の行（「わざと壊す方法」を持つもの）ごとに 1 行、行頭を「書き直しのわざと壊す確認」に続けてコロンにした箇条書きで、ロードマップの S 項目と観点・テスト・壊し方・落ちた理由を書く（`check-rewrite-equivalence.py --records` が読む。D51）
- Count の表示が変わった既存の golden の数（P2-1）
- LIN005 が出る既存の golden の数（P6-1。書き直しの前の見積もりは 131）と、サンプルと書き直した golden の LIN005 の数（P6-3）
- 網羅の監査（P9-4）: 行頭を「網羅の監査」に続けてコロンにした箇条書きで、指摘の件数と取り込んだ件数を書く
- 計画を書いた直後の監査（2026-10-28）: 独立したサブエージェントの指摘 61 件（ゴールが落ちる・判定できないもの 9、壊しても落ちないもの 12、網羅表に無い主張 13、分けるべき項目 22、判定が一意でない完了条件 6、§8 から移せる項目 5、事実の食い違い 4。分類の重なりを含む）。60 件を取り込み、1 件は §5.3 の末尾に取り込まない理由を書いた
- `G-RF-PROTO-GONE` の grep（P0-3、2026-10-30）: `proto/` をそのまま書くと、`goals.sh` に置いたこのゴール自身の行が数えられて 0 にならない。§5.1 の 2 つの grep を `prot[o]/` に直した。一致する文字列は同じ
- わざと壊す確認: S613・`G-RF-CLAUDEMD`（P0-3、2026-10-30）。`CLAUDE.md` の `plans/ozen-syntax-refresh.md` を別の名前に置き換えると、数が 0 になり「plan named in CLAUDE.md: got '0'」で落ちた。戻すと合格した
- わざと壊す確認: S482・`G-RF-PROTO-GONE`（`goals.sh` の 4 つの関数。P1-1、2026-10-30）。`goal_G_CORE_FLAGS` の道筋を `proto/scripts/check-flags.py` に戻すと「proto/ in moved goals: got '1'」で落ちた。戻すと 0 で合格した
- わざと壊す確認: S484・`G-REPO-SELFTESTS`（P1-1、2026-10-30）。`selftests.txt` から `scripts/check-pure.sh` の行を消すと「1 without test」で落ちた。戻すと合格した
- わざと壊す確認: S489・`sh/test-check-readme-example.sh`（P1-1、2026-10-30）。`run:` の無い塊の既定の実行ファイルを `--bin` に戻すと、`default-not-bin` などで「9 cases, 4 failed」になった。戻すと合格した
- README の例の検査の相対パス（P1-1、2026-10-30）: 検査は例ごとに一時のフォルダで実行ファイルを呼ぶので、`G-RF-README-EXAMPLE` の `--ozen build/core-gcc-release/ozen` が見つからなかった。区切りを含む値を絶対パスに直し、自己テストに `relative-paths` を足した。直す前のスクリプトでは、この場合が落ちることを確かめた
- README の試作の例（P1-1、2026-10-30）: 既定を `ozen` にしたので、試作の節の例に `<!-- run: ozenproto example0.ozen -->` を足した。節は P1-2 で消す。`--bin` と `ozenproto` の対応も、節と一緒に P1-2 で外す
- わざと壊す確認: S479・`G-RF-PROTO-GONE`（P1-2、2026-10-30）。`proto/defects.tsv` を `proto-final` から戻して追跡すると「tracked prototype files: got '1'」で落ちた。消すと合格した
- わざと壊す確認: S480・`G-RF-PROTO-GONE`（P1-2、2026-10-30）。`ios-proto.yml` を戻すと、追跡するファイル 1 と `scripts`・`.github` の参照 4 で落ちた。消すと合格した
- わざと壊す確認: S481・`G-RF-PROTO-GONE`（P1-2、2026-10-30）。タグ `proto-final` を `HEAD~2` に向けると「proto-final is the parent of the deletion」で落ちた。元のタグのオブジェクトに戻すと合格した
- わざと壊す確認: S483・`G-CORE-FLAGS`（P1-2、2026-10-30）。`goal_G_CORE_FLAGS` の道筋を `proto/scripts/check-flags.py` に戻すと、4 つの構成すべてで落ちた。戻すと合格した
- わざと壊す確認: S483・`G-CORE-PURE`（P1-2、2026-10-30）。`goal_G_CORE_PURE` の道筋を `proto/scripts/check-pure.sh` に戻すと落ちた。戻すと合格した
- わざと壊す確認: S483・`G-CORE-NOGLOBAL`（P1-2、2026-10-30）。`goal_G_CORE_NOGLOBAL` の道筋を `proto/scripts/check-noglobal.py` に戻すと落ちた。戻すと合格した
- わざと壊す確認: S483・`G-CORE-DIAG-REGISTRY`（P1-2、2026-10-30）。`check-diag-registry.sh` の既定の登録簿を `proto/src/diag_codes.def` に戻すと、登録簿が無く落ちた。戻すと合格した
- わざと壊す確認: S485・`G-RF-README-NO-PROTO`（P1-2、2026-10-30）。README に `## Evaluator prototype` の見出しを足すと落ちた。消すと合格した
- わざと壊す確認: S486・`G-REPO-README-LICENSE`（P1-2、2026-10-30）。ライセンスの表の行に `proto/` を戻すと「code row: got '0'」で落ちた。戻すと合格した
- わざと壊す確認: S487・`G-PLAN-TESTS-EXIST`（P1-2、2026-10-30）。`retired-tests.txt` から `golden/*` の最初の 1 行を消すと「367 names, 1 missing」で落ちた。戻すと合格した
- わざと壊す確認: S488・`G-RF-PROTO-GONE`（P1-2、2026-10-30）。報告書の冒頭の注記を消すと「proto-final in report header: got '0'」で落ちた。戻すと合格した
- わざと壊す確認: S490・`G-RF-PROTO-GONE`（P1-2、2026-10-30）。`check-selftests.sh` の対象に `proto/scripts` を戻すと、参照 1 で落ちた。戻すと合格した
- わざと壊す確認: S491・`G-RF-NO-PROTO-GOALS`（P1-2、2026-10-30）。`goal_G_PROTO_BUILD` を足すと「G-PROTO goals: got '1'」で落ちた。消すと合格した
- S483 の壊し方（P1-2、2026-10-30）: 網羅表は「移した `check-flags.py`・`check-pure.sh`・`check-noglobal.py` の根の求め方を戻す」としたが、この 3 つは根を求めず、引数の道筋だけを読む。そこで、ゴールの関数の道筋を `proto/scripts/` に戻す壊し方で確かめた。`check-diag-registry.sh` は網羅表のとおり既定の道筋で確かめた。そのために P1-1 で、`G-CORE-DIAG-REGISTRY` を既定の道筋で呼ぶようにした
- README の例の検査の `--bin`（P1-2、2026-10-30）: 試作の節と一緒に、`--bin` と `ozenproto` の対応を外した。S489 は、`run:` の無い塊を `--ozen` で走らせる `match` などの場合で確かめ続ける
- 試作の削除（P1-2、2026-10-30）: `proto/` のファイルは 565（574 から `scripts/` に移した 10 を引き、`ios-proto.yml` の 1 を足した数）。手元の試作のビルド `build/gcc-*`・`build/clang-*` の 4 つは、`CMakeCache.txt` の元が `proto` であることを確かめてから消した。`build/wasm`・`build/wasm-web`・`build/fuzz` も試作のものだが、D52 は消すものに挙げていないので残した
- `G-RF-PROTO-GONE` の見出しの語（P1-2、2026-10-30）: 判定の見出しの文字列「proto/ in …」も grep に数えられたので、「prototype paths in …」に直した
- Count の表示が変わった既存の golden（P2-1、2026-10-30）: 0。`typed_qty` で Count の型の値を無次元にし、`check_value` が Count の型に無次元を求めるようにした後、既存の 363 本はすべてそのまま合格した。Count の型の値を表示する既存の golden は無く、期待値の書き直しのコミットは要らなかった（R7 は起きなかった）
- わざと壊す確認: S492・`cgolden/quantity/count-times-mass`（P2-1、2026-10-30）。`typed_qty` で無次元の値に Count の次元を付ける元の処理に戻すと、`count-times-mass` が `rice: 150 g * servings` の次元の食い違いで落ちた（`check_value` は無次元を求めるので、Count の型の引数を持つ既存の golden 79 本も一緒に落ちた）。戻すと合格した
- わざと壊す確認: S493・`cgolden/quantity/count-options`（P2-1、2026-10-30）。`eval_param` で、条件（`-s 2`）から来た無次元の値にだけ Count の次元を付けると、`count-options` だけが落ちた。戻すと合格した
- わざと壊す確認: S494・`cgolden/quantity/count-kitchen`（P2-1、2026-10-30）。`entry_field` で台所の項目の無次元の値にだけ Count の次元を付けると、`count-kitchen` だけが落ちた。戻すと合格した
- わざと壊す確認: S495・`cgolden/quantity/count-compare`（P2-1、2026-10-30）。比較の評価で、左辺が名前の無次元の値に Count の次元を付け直すと、`count-compare` だけが落ちた。戻すと合格した
- わざと壊す確認: S496・`cgolden/quantity/count-with-pcs`（P2-1、2026-10-30）。`typed_qty` で `pcs` の次元を外す処理を消すと、`count-with-pcs` だけが落ちた。戻すと合格した
- わざと壊す確認: S497・`cgolden/quantity/ingredient-dimension`（P2-2、2026-10-30）。材料の量の次元の検査を外すと、`rice: 3 min` が通り「result ok」になって落ちた。戻すと合格した
- わざと壊す確認: S498・`cgolden/quantity/ingredient-dimension`（P2-2、2026-10-30）。診断の位置を材料の名前から始まる項目の節にすると、位置が量の式と合わなくなって落ちた。戻すと合格した
- わざと壊す確認: S499・`cgolden/quantity/ingredient-dimension-ok`（P2-2、2026-10-30）。受け付ける次元から体積を外すと、`milk: 200 ml` が `TYP001` になって落ちた（体積の材料を持つ既存の golden 51 本も一緒に落ちた）。戻すと合格した
- `ingredient-dimension-ok` の red-first（P2-2、2026-10-30）: 検査の前は材料の量を何も検査しないので、通る量の golden は直す前から合格する。エラーの golden と別のコミットにし、`scripts/red-first-exceptions.txt` に理由を書いた
- わざと壊す確認: S500・`cgolden/menus/make-bare`（P2-3、2026-10-30）。`eval_make` で、呼び出しでない `make` の子を見ずに `unknown()` を返す元の処理に戻すと、手順の行が出ず落ちた。戻すと合格した
- `make-servings` の観察の仕方（P2-4、2026-10-30）: 先に書いた golden は `ozen check` で `KIT003`（器 3 枚に 4 人分）を期待したが、`make` の中で評価する料理の診断は重複を避けるために抑える作り（`s->quiet`）なので、人数を渡しても `KIT003` は出ない。人数が効くことは段取りで盛り付けに使う器の数に表れるので、golden を `debug schedule` で器 5 枚の台所にし、4 枚を使うことを確かめる形に直した（修正前は人数が設定されず 1 枚）。`make` の中の料理で器が足りなくても黙って足りる分だけで盛る穴は、本計画では直さず、ユーザーに報告する
- わざと壊す確認: S501・`cgolden/menus/make-servings`（P2-4、2026-10-30）。`eval_param` で、`make` の引数で束縛済みの `servings` を料理の人数にしない元の処理に戻すと、盛り付けの器が `[bowl#1]` の 1 枚になって落ちた。戻すと合格した
- 後ろ向きの release の写し方（P2-5、2026-10-30）: 前向きの release（この時刻より前に始めない）は、反転した時計では「この時刻までに終える」締め切りになり、release としては写せない。反転した段取りでは release を外し、release を持つ作業と、反転した時計でそれより先に済ませる作業の優先度を、すべての作業の長さの和より大きく上げて先に並べる。優先度は `run()` と同じ最長経路の値に足す（`rank` があればそれに足す）。P7-2・P8-2 の後ろ向きも同じ仕組みに乗る
- `serve-within-backward` の手計算の写し間違い（P2-5、2026-10-30）: `ASM007`（盛り付けの時間が仮の値）は表示の順で最初の盛り付けに 1 回だけ付く。後ろ向きでは煮込みの盛り付け（22 行目）が先に来るのに、前向きの golden の 21 行目を写していた。期待値を別のコミットで直した。段取りの行は手計算のとおりだった
- わざと壊す確認: S502・`cgolden/serve/serve-within-backward`（P2-5、2026-10-30）。`sched.c` を release をそのまま反転した時計に写す元の処理に戻すと、汁物が 1680 s に仕上がり `SCH010` が出て落ちた。戻すと合格した
- わざと壊す確認: S503・`cgolden/serve/serve-within-backward`（P2-5、2026-10-30）。`SCH010` を待ちが期限を超えなくても出すようにすると落ちた（前向きの `serve-within` など 5 本も一緒に落ちた）。戻すと合格した
- S520 の数え違い（P3-1、2026-10-30）: `ceil(4)` は `Call`・`Name ceil`・`Number 4` の 3 ノードで、D43 の組み替えでも `4 |> ceil` は `Number`・`Name`・新しい `Call` の 3 ノードになる。計画を書いたときは組み替えの `Call` だけを数えて「1 つ多い」としていた。`ozen check --max-nodes` で `property batches = ceil(5 / 2)` が 7 でちょうど通り、6 で止まることを確かめ、S520 を「同じ数」に直した。網羅表の壊し方（Name を Call に書き換えて数を減らす）は 1 つ足りない `nodes` で、数を増やす誤りはちょうどの `nodes-exact` で捉える
- `pipe/ast` と `pipe/ast-rules`（P3-1、2026-10-30）: `ozen debug ast --rules` は規則の一覧だけを出し、構文木と span を出さない。golden は 1 つのコマンドしか走らせないので、S518（span）を `ast`、S516（規則 `pipe`）を `ast-rules` に分けた。`cgolden/pipe/*` は `nodes-exact` と合わせて 15 個
- わざと壊す確認: S504・`cgolden/pipe/bare-name`（P3-1、2026-10-30）。右辺が名前のとき左辺を引数に足さないと、`wash()` が引数なしになり、`bare-name` が落ちた（名前の段を持つ `chain`・`macro`・`precedence` なども一緒に落ちた）。戻すと合格した
- わざと壊す確認: S505・`cgolden/pipe/first-arg`（P3-1、2026-10-30）。左辺を最後の位置の引数（名前付きの引数の前）に入れると、`cut(slice, peeled, …)` になり `first-arg` が落ちた。戻すと合格した
- わざと壊す確認: S506・`cgolden/pipe/chain`（P3-1、2026-10-30）。右辺を残りのパイプごと読んでから左辺を差し込む右結合にすると、`chain` が落ちた。戻すと合格した
- わざと壊す確認: S507・`cgolden/pipe/precedence`（P3-1、2026-10-30）。パイプを単項の式の直後で読むようにして `sum` より強くすると、`precedence` だけが落ちた。戻すと合格した
- わざと壊す確認: S508・`cgolden/pipe/bad-target-quantity`（P3-1、2026-10-30）。右辺の名前の検査を外し、任意の式を左辺との呼び出しに包むと、`bad-target-quantity` が落ちた（`bad-target-list`・`bad-target-make` も一緒に落ちた）。戻すと合格した
- わざと壊す確認: S509・`cgolden/pipe/bad-target-list`（P3-1、2026-10-30）。S508 と同じ壊し方で、`bad-target-list` が落ちた。戻すと合格した
- わざと壊す確認: S510・`cgolden/pipe/bad-target-make`（P3-1、2026-10-30）。右辺の先頭に `make` を許して包むと、`bad-target-make` だけが落ちた。戻すと合格した
- わざと壊す確認: S511・`cgolden/pipe/macro`（P3-1、2026-10-30）。`eval_macro` で、パイプで作った呼び出しの最初の引数を飛ばすと、`macro` だけが落ちた。戻すと合格した
- わざと壊す確認: S512・`cgolden/pipe/named-only`（P3-1、2026-10-30）。左辺を引数の最後（名前付きの引数の後ろ）に入れると、`named-only` が落ちた（`first-arg`・`chain` なども一緒に落ちた）。戻すと合格した
- わざと壊す確認: S513・`cgolden/pipe/unattended`（P3-1、2026-10-30）。`check_targets` で、パイプで作った呼び出しを最初の段の左辺（`rice`）までたどって確かめると、`unattended` だけが落ちた。戻すと合格した
- わざと壊す確認: S514・`cgolden/pipe/unattended-builtin`（P3-1、2026-10-30）。`check_targets` で、パイプで作った呼び出しを確かめないようにすると、`unattended-builtin` だけが落ちた。戻すと合格した
- わざと壊す確認: S516・`cgolden/pipe/ast-rules`（P3-1、2026-10-30）。`mark(p, OZNI_R_pipe)` を外すと、規則の一覧から `pipe` が消えて `ast-rules` だけが落ちた。戻すと合格した
- わざと壊す確認: S517・`G-CORE-GRAMMAR`（P3-1、2026-10-30）。`grammar.ebnf` から規則 `pipe` を消すと、直した検査（後述）で「131 rules, 0 not covered, 1 unknown」になって落ちた。戻すと合格した
- わざと壊す確認: S518・`cgolden/pipe/ast`（P3-1、2026-10-30）。組み替えた呼び出しの span を右辺の始まりからにすると、`ast` が落ちた（`nodes` の `MAC003` の位置も変わって落ちた）。戻すと合格した
- わざと壊す確認: S519・`cunit/span_nesting`（P3-1、2026-10-30）。組み替えた呼び出しの span の終わりを左辺の終わりにすると、`span_nesting` が「span 103-107 of Name outside 87-91」などで落ちた。戻すと合格した
- わざと壊す確認: S520・`cgolden/pipe/nodes`（P3-1、2026-10-30）。右辺の `Name` のノードを `Call` に書き換えてノードを増やさないと、`nodes` が落ちた。戻すと合格した
- わざと壊す確認: S521・`G-RF-FUZZ-DICT`（P3-1、2026-10-30）。辞書から `"|>"` を消すと「fuzz dict "|>": got '0'」で落ちた。戻すと合格した
- 文法の網羅の検査の穴（P3-1、2026-10-30）: `check-grammar-coverage.py` は、`ozen debug ast --rules` が出したのに文法に無い規則を「unknown rule」として終了コード 1 にするが、ゴールが読む最後の行は「0 not covered」のままだった。そのため S517 の壊し方では `G-CORE-GRAMMAR` が落ちなかった。文法に無い規則があるときだけ、最後の行に「, <n> unknown」を足すように直し、自己テストの `unknown-rule` の期待を先に変えた。無いときの行はロードマップの書式のまま
- `G-RF-FUZZ-DICT` の P3-1 での扱い（2026-10-30）: このゴールは `"|>"`・`"per"`・`"serving"` の 3 つを見る。P3-1 で足すのは `"|>"` だけで、残りの 2 つは P4-1 で足すので、それまでこのゴールは 2 つの確かめで落ちる。P3-1 では `"|>"` の確かめが合格することを見た
- ユーザーに報告するだけで本計画では直さないもの:
  - ロードマップの見出しの D33・D34 の重複（124・325 行目、135・334 行目）
  - ロードマップ §4.20.1 の文法の規則の数（130 と書いてあるが、実測は 131）
  - 台所への要求の `:`・`has`・知らない項目が満たしたことになる穴と、`HeatSource` の `modes` の綴りを検査しないこと（B7）

---

## 10. リスク

- **R1. 書き直しの量**: `tests/golden` は 816 ファイルで、期待値を手で書き直す。位置（行と列）を含む `.out` が 196、段取りを手で組む golden が約 60 ある。さらに D51 で、ロードマップ §5.3 の約 242 行のわざと壊す確認をやり直す。フォルダごとのタスク（P5-1〜P5-7）に分け、別のセッションで並行できるようにする。各タスクのゲートで `G-RF-EQUIVALENCE` を回し、性質を落としていないことを機械で確かめてから、壊す確認に進む。
- **R2. 手計算の誤り**: 期待値を手で書くので、誤りが入りうる。段取りは `schedcheck`、量は `Fraction` と突き合わせる。誤りと分かったら、ロードマップ §5.2 の規則どおり、実装と別のコミットで直し、`red-first-exceptions.txt` と §9 に理由を書く。
- **R3. LIN005 がサンプルに出る**: サンプルの検査（`tests/samples/check.py`）は警告の数を見ないので、出ても `G-SAMPLE-CHECK` は落ちない。`G-RF-SAMPLES-UNUSED` で数え、出たらユーザーに報告して、サンプルを直すか規則を直すかを決める。
- **R4. 予熱と期限の段取りが収束しない**: release の反復は最大 8 回で、守れなければ警告で終わる。手で組んだ golden に、守れる配置が 1 つしか無い例を入れ、収束しなければ golden が落ちるようにする。
- **R5. 試作の削除で CI が落ちる**: `ios-proto.yml` が `proto/` を使う。P1-2 でワークフローも消し、P1-3 で `G-PLAT-CI` と `G-RF-CI-NO-PROTO` を確かめる。
- **R6. 別のセッションと同じ `main` に入れる**: 会話の開始後に 196 コミットが入った前例がある。各フェーズの最初とコミットの前に `git log` と `git status` を見て、触るファイルが重なる作業が入っていれば、そのセッションと調整するかユーザーに確認する。
- **R7. D45 で既存の期待値が変わる**: Count の値を出す golden（器の枚数・台所の数など）の表示が `4 pcs` から `4` に変わりうる。P2-1 で該当の golden を数え、期待値を別のコミットで直す。
