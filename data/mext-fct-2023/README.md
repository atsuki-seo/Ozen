# Data extracted from the Standard Tables of Food Composition in Japan

出典: 日本食品標準成分表（八訂）増補2023年から引用（文部科学省）

`refuse.csv` holds the refuse rate (廃棄率, percent of the purchased weight that is
discarded) of the foods that the vocabulary table refers to. It is extracted from the
MEXT Excel file below by `tools/fct/extract.py`, and `tools/fct/extract.py --check`
re-extracts it and compares. The Excel file itself is not committed; put it in
`source/` to run the extractor.

- Source URL: https://www.mext.go.jp/a_menu/syokuhinseibun/mext_00001.html
- Retrieved: 2026-10-26
- File: 20260327-mxt_kagsei-mext-000029402_02.xlsx
- SHA-256: 0d5a77077dd6cd91cbc2e6e317b8b218a38728c409eed452f1c10635a0d3099c

The file is the data of chapter 2 of the 2023 supplement as published on 2026-03-27,
together with the errata of that date (20260327-mxt_kagsei-mext-000029402_16.xlsx,
SHA-256 fb61037c7f66af0db1fb0729977913a629bc17097bc3ff7bf75217f9110acabb). The errata
change no refuse rate.

Columns of `refuse.csv`: `food` (food number), `name` (food name as in the table),
`refuse` (refuse rate in percent, as written in the table). The values are read from
the sheet `表全体`, columns B (食品番号), D (食品名), and E (廃棄率, component
identifier `REFUSE`), from row 13.

These values are used under MEXT's terms for the Standard Tables of Food Composition;
the Ozen licenses do not apply to them.
