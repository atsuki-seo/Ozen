# The introduction page

[`site/index.html`](index.html) is a one-page introduction to Ozen in Japanese,
published at <https://atsuki-seo.github.io/Ozen/>.

- It has no build step and loads only the DotGothic16 font from Google Fonts.
- On every push to `main` that changes `site/`, `VERSION`, or the workflow,
  [`.github/workflows/pages.yml`](../.github/workflows/pages.yml) writes the version and
  the commit into the footer and deploys the page to GitHub Pages.
- The code examples and outputs on the page are copied from runs of
  `build/core-gcc-release/ozen`; update them by hand when the language changes.
- [`og.png`](og.png) is the 1200×630 link preview card named by the OGP tags. The tags
  use absolute URLs, so update them if the page moves.
