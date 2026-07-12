---
name: droid-manual
description: Look up the source manual pages for a DROID circuit or topic and read them from the PDF. Use for "show me the manual pages for <circuit>", "what does the manual say about <topic>", or when a circuit's structured doc frontmatter/prose isn't enough and you need the original page renders (diagrams, tables, worked figures).
---

# droid-manual

Map a circuit or topic to PDF page numbers, then read those exact pages from
the source manual — useful when the structured docs under `manual/` summarize
or restructure content and you need to check the original wording, a diagram,
or a table exactly as printed.

## 1. Find the page numbers

- **Circuit**: read `manual/circuits/<name>.md`'s YAML frontmatter. The
  `manual_pages` field is a flat list of PDF page numbers, e.g.:
  ```yaml
  manual_pages: [267, 268, 269, 270, 271, 272]
  ```
  It is not a `start-end` range string — treat it as the literal set of pages
  covering that circuit (may be non-contiguous).
- **Topic without its own circuit file** (hardware, patch syntax, scales,
  general concepts): grep `manual/basics.md`, `manual/hardware.md`, and
  `manual/scales.md` for the topic. These docs cite the source with inline
  `(page NNN)` references in prose — pull the page number(s) from the nearest
  such reference.
- If unsure which circuit/topic applies, check `manual/circuits/index.md`
  first (alphabetical table + tag search + by-function grouping).

## 2. Read those pages

The source PDF lives at the repo root as `droid-manual-blue-7.pdf`. It is
gitignored (large binary, user-fetched) — if it's missing, tell the user to
download the blue-7 DROID manual PDF from Der Mann mit der Maschine's site
(shop.dermannmitdermaschine.de/pages/downloads, or directly
`https://dermannmitdermaschine.de/download/droid-manual-blue-7.pdf` — verify
that link still resolves before relying on it, since firmware versions roll
forward) and save it at the repo root as exactly `droid-manual-blue-7.pdf`.

With the PDF present, use the Read tool directly on it with the `pages`
parameter set to the page numbers found in step 1 (e.g. `pages: "267-272"` or
`pages: "125-126"`). Read handles PDFs natively — no separate extraction step
is needed. Keep requests to 20 pages or fewer per call (Read's PDF limit);
split a `manual_pages` list wider than that into multiple calls.

## 3. Fallback: pre-rendered page images

If the PDF isn't available locally, the `manual/images/` directory holds
full-page renders (`page-<NNN>` PNGs) for at least the pages the structured
docs reference — these are untracked (local-only, git-ignored) so they may or
may not be present. Read the specific page files matching the page numbers
from step 1 with the Read tool (it reads images natively too). This is a
fallback only: it covers just the pages some doc already cited, not the
whole manual, so it won't help for pages nothing currently references.
