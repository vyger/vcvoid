# DROID manual (firmware blue-7) — structured reference

Machine-navigable translation of the DROID eurorack manual, broken into focused
Markdown documents for efficient reference by agents and humans.

## Documents

| Doc | Manual chapters | Contents |
|-----|-----------------|----------|
| [basics.md](basics.md) | 1–5 | Installation, patching fundamentals, the Forge, advanced patching (one-knob-multiple-functions, presets, tap tempo), patch generators (MFPS & Megasequencer), and the patch-file text syntax (incl. the error-code table). |
| [hardware.md](hardware.md) | 6–14 | Controllers (P2B8, P4B2, P10, S10, P8S8, B32, E4, M4, DB8E), the G8 & X7 expanders, the MASTER18, the R2M/R2C bridge, internals, firmware upgrades, calibration/maintenance, and hardware specs. |
| [scales.md](scales.md) | 15 | All 108 musical scales (numbered 0–107) with their notes and scale-degree fills. |
| [circuits/](circuits/) | 16 | **The core reference:** one file per circuit (76 total). Start at [circuits/index.md](circuits/index.md). |

## The circuit reference

[`circuits/index.md`](circuits/index.md) is the entry point for finding circuits:

- **Jack type legend** — what each `Type` token (`CV`, `pitch`, `0..1`, `gate`,
  `trigger`, `integer`, `stepped`, `text`, …) means. The manual prints these as
  glyphs; they are translated to text tokens throughout the circuit docs.
- **Alphabetical master table** — every circuit with its function, category,
  RAM cost, manual page range, and searchable keywords.
- **By-function grouping** and an **obsolete-circuits** list.

Each `circuits/<name>.md` has YAML frontmatter (`circuit`, `title`, `obsolete`,
`ram_bytes`, `manual_pages`, `category`, `tags`, `see_also`) followed by prose,
worked `droid` patch examples, and complete **Inputs**/**Outputs** tables.

## Images

The `images/` directory holds full-page renders (150 dpi) of every manual page
that carries a figure — panel drawings, timing/waveform diagrams, wiring
diagrams, and Forge/DB8E screenshots, named by source PDF page number. They are
kept on disk (git-ignored, not tracked) for later processing (e.g. cropping
individual figures); docs keep their `(page NNN)` prose references but no
longer link the renders inline.

## Conventions

- DROID code is fenced as ```` ```droid ````; register/jack/cable names are in
  `backticks` (`I1`, `O3`, `G1`, `B1.1`, `L1.1`, `_MYCABLE`).
- Cross-references are relative Markdown links between these docs.
- Source: *DROID manual for firmware blue-7* (413 pages). Text is faithful to the
  source; the manual's own wording/quirks are generally preserved.
