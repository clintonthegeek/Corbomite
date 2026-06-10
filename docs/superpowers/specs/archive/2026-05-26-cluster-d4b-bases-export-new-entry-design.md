# Cluster D.4b — Bases export/copy + +New entry — Design

**Date:** 2026-05-26
**Cluster:** D (Bases UI completion), sub-project D.4b
**Status:** Shipped 2026-05-26
**Predecessors:** D.1 (backend correctness), D.2 (read-side rendering), D.3 (toolbar menus + properties drawer), D.4a (cell interactivity)

## Summary

Add the two remaining toolbar actions Obsidian's Bases offers for moving data out
of and new notes into a base view:

1. **Export / copy** — a "Results menu" toolbar button offering *Copy table*
   (clipboard, four MIME flavors: TSV, Markdown, HTML, `obsidian/table`) and
   *Export CSV…* (save to a file).
2. **+New entry** — a "+" toolbar button that creates a new note seeded so it
   satisfies the view's filter, opens it in a new tab, and prompts for a rename.

Both follow the D.x pattern established across this cluster: **pure, widget-free
helpers carrying their own unit tests**, then thin `BasesView` widget wiring that
reuses the service and callback seams already injected in earlier sub-projects.

## Reference

Obsidian behavior, from the reverse-engineered audit
([`docs/obsidian-audit/domains/bases.md`](../../obsidian-audit/domains/bases.md)):

- `:112` — `exportTable()` → 4-format exporter; `copyToClipboard()` sets
  TSV + Markdown + HTML + `obsidian/table`.
- `:124` — `NewItemMenu`: creates a note in `query.newItemFolder` (default via
  `FY(...)`), seeded from `query.newItemTemplate` or computed filter-satisfying
  defaults, then opens an inline `HoverEditor` popover for the title rename.
- `:136` — the "Results menu" hosts *Copy table* (4 MIME types) and *Export CSV*.

The `obsidian/table` clipboard payload format, confirmed against the real source
(`/home/clinton/bin/ObsidianRAW/audit/extracted/obsidian/app.js`): the editor
calls `setData("obsidian/table", JSON.stringify({rows, alignment}))` where `rows`
is a 2-D array of cell-text strings and `alignment` is a per-column array of
alignment strings. Left-alignment is the empty string `""`.

## Components

### 1. `TableExporter` (pure helper, `libs/bases`)

Widget-free serializer. No Qt widget dependency; depends only on the bases
value/result types and `QString`/`QStringList`.

**Input.** The current `BasesQueryResult` plus the active `BasesViewConfig`
(for the ordered list of *visible* properties and their display names). Cells
are rendered to their display string using the same value→text path the cell
delegate uses, so exported text matches what is on screen.

**Row set.** Flat, in the view's current sort order. Grouping is **ignored** —
this matches Obsidian, which exports the flat result, not the collapsed group
tree. No group-heading rows are emitted.

**Outputs.**

| Method | Format |
|---|---|
| `toCsv()` | RFC-4180: fields containing `,`, `"`, `\r`, or `\n` are double-quoted, embedded `"` doubled. Header row = display names. `\r\n` line endings. |
| `toTsv()` | Tab-separated; tabs/newlines inside a cell are stripped or space-substituted (TSV has no quoting). Header row = display names. |
| `toMarkdown()` | GFM pipe table with a header row and a `---` separator row; `|` inside cells escaped as `\|`. |
| `toHtml()` | `<table>` with a `<thead>`/`<tbody>`; cell text HTML-escaped. |
| `toObsidianTable()` | `QByteArray` of `JSON.stringify({rows, alignment})`. `rows` includes the header row as `rows[0]`; `alignment` is one `""` per column. |

**Tests** (`tst_table_exporter`): empty result; single column; multiple
columns/rows; a cell containing comma + quote + embedded newline (CSV quoting);
a cell containing a tab (TSV sanitization); a cell containing `|` (Markdown
escaping); a cell containing `<`/`&` (HTML escaping); `obsidian/table` JSON shape
(header in `rows[0]`, alignment length == column count).

### 2. `NewItemSeed` (pure helper, `libs/bases`)

Widget-free. Computes the frontmatter to seed into a new note so that it lands
inside the view's filter.

**Input.** The active filter tree (the same `FilterTree` the query evaluates)
and the optional `newItemTemplate` frontmatter.

**Algorithm.** Start from the template's key/value pairs (empty if no template).
Then walk the filter tree collecting **top-level, AND-context equality
constraints** of the shape `property == literal`:

- Descend through `AND` nodes only.
- An `OR` node, a negation, or any non-equality comparison (`!=`, `<`, `>`,
  `contains`, regex, function call, etc.) contributes **nothing** — that whole
  subtree is skipped. A note created under an OR filter may therefore not match;
  that is accepted (documented limitation, same pragmatic boundary chosen in the
  brainstorm).
- `property` must be a plain note frontmatter property (`note.*` / bare
  identifier). `file.*` and formula properties are skipped (not writable as
  frontmatter).

Equality-derived values **override** template values on key collision (a filter
constraint is a harder requirement than a template default).

**Output.** An ordered `{property → value}` map (insertion order: template keys
first, then any new equality keys), suitable for handing to a frontmatter writer.

**Tests** (`tst_new_item_seed`): no filter + no template → empty; single
`status == "active"` → `{status: active}`; AND chain of two equalities → both;
OR of two equalities → neither; negated equality → skipped; `!=`/`<` → skipped;
`file.name == …` → skipped; template-only (no matching filter) → template
verbatim; equality overrides a colliding template key.

### 3. `BasesView` widget wiring

Two new toolbar controls, added alongside the existing props/sort/views/drawer
buttons, using the same `QToolButton` idiom.

**"+" button** (`m_newBtn`):
1. Resolve the target folder: `query.newItemFolder` if set, else the vault's
   default new-file location.
2. Compute the seed map via `NewItemSeed`.
3. `FileManager::createMarkdownNote("Untitled", folder)` — the existing API
   auto-suffixes on collision and returns the new `TFile` (or null on failure).
4. Apply the seed frontmatter to the new file via the same `processFrontMatter`
   path the rest of Bases uses for inline edits.
5. `m_openInNewTab(path)` then `m_promptRename(path)` — both callbacks already
   injected in D.4a. This reproduces Obsidian's "create, open, rename" intent
   using the validating rename dialog instead of an inline HoverEditor popover.

If create fails (null return), surface a non-fatal error (reuse the existing
`m_errorBanner`) and do nothing else.

**Results-menu button** (`m_resultsBtn`): a `QToolButton` with a `QMenu`
containing:
- *Copy table* — build a single `QMimeData`, call
  `setText(toTsv())` plus `setData()` for `text/markdown`, `text/html`, and
  `obsidian/table`, then `QGuiApplication::clipboard()->setMimeData(...)`.
- *Export CSV…* — `QFileDialog::getSaveFileName` (default name from the base
  file stem + `.csv`); on accept write `toCsv()` (UTF-8). Report write failure
  via `m_errorBanner`.

The `TableExporter` is constructed on demand from `m_controller->result()` and
`m_activeView`; no exporter state is held across actions.

## Host wiring

Minimal. `m_fm` (FileManager) is already injected via `setServices`;
`m_openInNewTab` and `m_promptRename` are already wired by D.4a's MainWindow
host callbacks. Creating a file requires the `vault.write` permission — the same
context the D.4a rename/delete prompts already run under. BasesView is a built-in
view (Cluster K), so the file-create path is expected to be a direct
`FileManager` call rather than a proxy hop; this is confirmed during planning,
and if a proxy hop is required it is added to the existing MainWindow wiring
block, not invented anew.

## Testing & verification

- **Pure helpers** (`TableExporter`, `NewItemSeed`) are covered by full TDD with
  the test matrices above. These are the correctness core of D.4b.
- **Widget paths** (clipboard MIME assembly, file-save dialog, the +New
  create/open/rename flow) cannot be exercised in the offscreen Qt test
  environment. They join the existing D.2/D.3/D.4a **"pending user eyeball"**
  verification backlog, called out explicitly at close-out.
- All existing bases tests must remain green; clean build; launch smoke clean.

## Out of scope (deferred)

- **Filter-satisfying defaults across OR / negation / non-equality trees** —
  D.4b seeds AND-context equalities only.
- **Undo/redo** for the +New create (and for inline edits) — that is **D.4c**.
- **Per-layout export variants** (cards/list) — table export only; the table is
  the only layout currently rendered.
- **Inline HoverEditor rename popover** — superseded by the existing validating
  rename dialog; gated on Markoff's read-only-Live spec regardless.
