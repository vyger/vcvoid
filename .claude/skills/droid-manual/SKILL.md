---
name: droid-manual
description: Answer DROID manual questions from the structured docs under manual/, falling back to the source PDF only when the structured docs aren't enough. Use for "show me the manual pages for <circuit>", "what does the manual say about <topic>", or when you need the original page renders (diagrams, tables, worked figures).
---

# droid-manual

Answer manual questions from the structured Markdown reference under `manual/`
first; go to the source PDF only when those docs leave the question unclear.

## 1. Primary source: the structured manual

Start at `manual/README.md` — it maps the whole reference:

- `manual/circuits/index.md` — entry point for the 76 circuits (alphabetical
  table, tag search, by-function grouping); each `circuits/<name>.md` has
  frontmatter, prose, worked `droid` examples, and full Inputs/Outputs tables.
- `manual/basics.md` — install, patching fundamentals, the Forge, advanced
  patching, patch generators, `droid.ini` syntax + error codes (ch. 1–5).
- `manual/hardware.md` — controllers, G8/X7 expanders, MASTER18, internals,
  specs (ch. 6–14).
- `manual/scales.md` — all 108 scales (ch. 15).

These docs answer the large majority of questions and are the spec this repo
implements against. Only fall back to the PDF (steps 2–3) when the structured
docs are genuinely not enough: you need a diagram or figure, the exact original
wording or a table as printed, or the md prose is ambiguous/silent on the
detail in question.

## 2. Fallback: find the PDF page numbers

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

## 3. Fallback: read those pages from the PDF

The source PDF lives at the repo root as `droid-manual-blue-7.pdf`. It is
gitignored (large binary, user-fetched), so **it may not exist on disk — check
before reading, and if it's missing do NOT fail silently or quietly answer
from the md docs alone.** Tell the user clearly, in your final response, that:

1. the lookup needed the source PDF, which is **not on disk**;
2. it belongs at the repo root as exactly `droid-manual-blue-7.pdf`, downloaded
   from Der Mann mit der Maschine
   (shop.dermannmitdermaschine.de/pages/downloads, or directly
   `https://dermannmitdermaschine.de/download/droid-manual-blue-7.pdf` —
   verify that link still resolves before relying on it, since firmware
   versions roll forward);
3. **offer to download it for them** (e.g. `curl -L -o
   droid-manual-blue-7.pdf <url>`) — but never download without the user
   agreeing first.

With the PDF present, use the Read tool directly on it with the `pages`
parameter set to the page numbers found in step 2 (e.g. `pages: "267-272"` or
`pages: "125-126"`). Read handles PDFs natively — no separate extraction step
is needed. Keep requests to 20 pages or fewer per call (Read's PDF limit);
split a `manual_pages` list wider than that into multiple calls.

## 4. Last resort: pre-rendered page images

If the PDF isn't available locally (and the user hasn't opted to download it),
the `manual/images/` directory holds full-page renders (`page-<NNN>` PNGs) for
at least the pages the structured docs reference — these are untracked
(local-only, git-ignored) so they may or may not be present. Read the specific
page files matching the page numbers from step 2 with the Read tool (it reads
images natively too). This is a fallback only: it covers just the pages some
doc already cited, not the whole manual, so it won't help for pages nothing
currently references.
