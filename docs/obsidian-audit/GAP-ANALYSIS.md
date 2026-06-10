# GAP-ANALYSIS — Prioritised Corbomite compat gaps against Obsidian

> **⚠ FROZEN AT 2026-04-14 — priorities here are obsolete.** Verified 2026-06-10:
> P0.1/P0.2/P0.3/P0.5/P0.6, P1.1/P1.2/P1.3 are FIXED; P2.13 (Bases) and P2.19
> (search DSL) substantially delivered; P2.5 prescribes a QGraphicsView migration
> superseded by the QML foundation port. Do not plan from this file. Current gaps:
> [`../PARITY-MATRIX.md`](../PARITY-MATRIX.md) + `docs/punch-list.md` + the roadmap
> `docs/superpowers/plans/2026-06-10-road-to-dogfood.md`.

This document ranks what Corbomite is missing, in-order, against the Obsidian behaviour specified in the Pass 2 domain docs. Each entry cites the domain doc that sources the Obsidian behaviour and the Corbomite file or library that would receive the fix. "Proposed work" is what a Corbomite engineer would implement; "Cluster" names the bundle of gaps that share a dependency.

## Prioritisation rubric

- **P0 — Correctness bugs.** Corbomite is wrong today. Small scope; fixable in days. Must ship before any compat work relies on them.
- **P1 — Compat blockers.** Corbomite would work but not interoperate with Obsidian-managed vaults. Fixing these is the difference between "Corbomite is a Markdown app" and "Corbomite is an Obsidian-compatible Markdown app".
- **P2 — Feature parity.** User-visible features present in Obsidian that Corbomite lacks. Not blockers for interop; blockers for user adoption by Obsidian users.
- **P3 — Plugin-ready prep.** Primitives Corbomite's future plugin API will expose. Not needed for Corbomite 1.0 but expensive to retrofit.
- **P4 — Nice-to-have / deferred.** Features with small user impact or with simpler alternatives Corbomite can adopt.

---

## P0 — Correctness bugs (small scope, must-fix)

### P0.1 — Link resolver returns wrong file under ambiguity

- **Gap:** `[[Foo]]` with two `Foo.md` files in the vault resolves to whichever was indexed last, not the shortest-path-same-folder-winner.
- **Obsidian behaviour:** 10-step algorithm in `metadata.md §8 "Link resolution algorithm"` and `VAULT-FORMAT.md §7`. Shortest-path-wins with same-folder preference; relative `./../` support; rooted `/…` disables short-name disambiguation.
- **Corbomite today:** `libs/storage/src/SQLiteIndex.cpp:592-609` `resolveWikilink` is a flat `m_nameToPath` lookup with no tiebreak, no same-folder bias, no relative path support.
- **Proposed work:** Port the full 10-step `getLinkpathDest` to `SQLiteIndex::resolveWikilink`. Also port `fileToLinktext(file, sourcePath, NewLinkFormat)` inverse so link-insertion in editors and canvas produces the same minimal form as Obsidian.
- **Cluster:** A — link/frontmatter correctness bundle.

### P0.2 — Frontmatter detection breaks on EOF-closed `---`

- **Gap:** A note ending in bare `---` (no trailing newline) is not recognised as having frontmatter.
- **Obsidian behaviour:** `parsing.md §1 getFrontMatterInfo` — closing regex `Qx = /---(\r?\n|$)/g` is **EOF-tolerant**.
- **Corbomite today:** `Markoff::Document::fromMarkdown` uses `indexOf("\n---", 3)` which requires a preceding `\n` (parsing.md §11 entry "Frontmatter delimiter regex").
- **Proposed work:** Replace `indexOf` with a regex matching `/---(\r?\n|$)/` and require a preceding `\n`. Implementation goes in `libs/markoff-parser/` or `libs/core/FrontMatter.cpp`.
- **Cluster:** A.

### P0.3 — Wikilink `#subpath` not stripped before indexing

- **Gap:** A link `[[Note#Heading]]` stored in SQLite has `target = "Note#Heading"` (instead of `"Note"` with a separate `subpath` column).
- **Obsidian behaviour:** `parseLinktext(linktext) → {path, subpath}` — `vault.md §1`; `VAULT-FORMAT.md §4.4`.
- **Corbomite today:** `SQLiteIndex.cpp:462-464` regex `[^\]|]+` captures everything before `|` but does not split at `#` (parsing.md "Pass 2 additions" entry "parseLinktext subpath not extracted in SQLiteIndex"; 01-markoff-gaps.md §parsing).
- **Proposed work:** Apply `parseLinktext` semantics at index time. Add `subpath` column to the links table or a separate normalised-linkpath index so scroll-to-heading and subpath-aware resolution both work.
- **Cluster:** A.

### P0.4 — `SearchMatch` lacks byte-range array

- **Gap:** Every search consumer (search panel, quick switcher, command palette, every suggester) needs byte-range match offsets to render highlight spans. Corbomite's `SearchMatch` has only `{notePath, snippet, score}`.
- **Obsidian behaviour:** Every matcher returns `{score, matches: [[s,e],...]}`. `rendering/renderResults.js` consumes this shape directly. (`search.md §2`, `rendering.md §1 renderResults`.)
- **Corbomite today:** `libs/storage/include/corbomite/storage/SQLiteIndex.h:14-18`:
  ```cpp
  struct SearchMatch { QString notePath; QString snippet; double score; };
  ```
- **Proposed work:** Add `QVector<std::pair<int,int>> matches;` to `SearchMatch`. Populate by parsing FTS5 `snippet()`/`highlight()` output back to offsets, or re-score FTS candidates through a ported `fuzzySearch`.
- **Cluster:** D — search parity bundle.

### P0.5 — Unsplit wikilinks in `SQLiteIndex`

- **Gap:** Corbomite's wikilink regex does not split `[[target|display]]` → `{target, display}` or `[[target#subpath]]` → `{target, subpath}`. The full `[[text]]` body hits the index target column.
- **Obsidian behaviour:** Markdown parser splits on `|` before passing to `parseLinktext`, which splits on first `#`.
- **Corbomite today:** `SQLiteIndex.cpp:462-464`.
- **Proposed work:** Merged with P0.3. The same regex overhaul should split on `|` (producing `displayText`) and `#` (producing `subpath`).
- **Cluster:** A.

### P0.6 — `.obsidian/workspace.json` schema mismatch

- **Gap:** Corbomite writes session state to `~/.local/share/corbomite[-dev]/<vault>/session.json` with its own schema. Obsidian writes to `<vaultRoot>/.obsidian/workspace.json` with the `LayoutJson` schema. Zero interop.
- **Obsidian behaviour:** `VAULT-FORMAT.md §3 "workspace.json"`; `workspace.md §2, §3`.
- **Corbomite today:** `src/app/SessionManager::buildSplitLayoutJson` — wrong directory + wrong field names (`split`+`sizes`+`orientation` vs `split`/`tabs`/`leaf`+`direction`+`dimension`+`children`).
- **Proposed work:** See P1.2 (this is a compat blocker, not strictly a correctness bug). Listed here so it's not lost.
- **Cluster:** B — vault-config I/O bundle.

### P0.7 — Vault-switch process-death pattern not implemented

- **Gap:** Opening a new vault while one is open **crashes** (stale refs across service reconstruction).
- **Obsidian behaviour:** `core.md §1 openVaultChooser, §2 "Vault-switch behaviour"` — Obsidian destroys the window and spawns a new one. No in-process swap.
- **Corbomite today:** `src/app/MainWindow::closeVault` does in-process teardown; service graph not topologically rebuilt.
- **Proposed work:** Mirror Obsidian. Destroy `MainWindow` (and everything below it) on vault switch; `CorbomiteApp` constructs a fresh one. Kate-session pattern is the reference (`memory/reference_kate_sessions.md`).
- **Cluster:** C — lifecycle bundle.

---

## P1 — Compat blockers (interop, not feature)

### P1.1 — `.obsidian/*.json` round-trip (preserve unknown keys)

- **Gap:** Corbomite does not read or write `.obsidian/app.json`, `.obsidian/appearance.json`, `.obsidian/core-plugins.json`, `.obsidian/community-plugins.json`, `.obsidian/hotkeys.json`, or `.obsidian/plugins/<id>/data.json`.
- **Obsidian behaviour:** All 6 JSON files are vault-format-critical; unknown-key preservation is the universal invariant (`VAULT-FORMAT.md §1, §3`).
- **Corbomite today:** Settings live outside the vault in `~/.config/corbomiterc`. No vault-scoped config at all. (`vault.md §11 entry`; `settings.md §11`.)
- **Proposed work:** New `libs/core/VaultConfig` class: read/write all six files; `QObject::objectNameChanged` or custom signal for `config-changed(key)`; preserve unknown keys via a `QJsonObject unknownKeys` field merged at write time. Wire into `VaultService`. Keep `corbomiterc` for Corbomite-only settings.
- **Cluster:** B.

### P1.2 — `.obsidian/workspace.json` compat

- **Gap:** Corbomite cannot restore the layout state of an Obsidian-managed vault, and vice versa. See P0.6.
- **Obsidian behaviour:** `VAULT-FORMAT.md §3 "workspace.json"`; full schema in `workspace.md §2`.
- **Corbomite today:** `SessionManager` writes wrong shape to wrong directory.
- **Proposed work:** Migrate `SessionManager` to a `.obsidian/workspace.json` writer matching `LayoutJson` shape. Per-leaf `id` via `QUuid`. Sidedock `width`+`collapsed`. Tab `currentTab`/`stacked`. Ribbon `hiddenItems` key-order-as-runtime-order invariant. Preserve unknown node types (don't drop, render as empty placeholder).
- **Cluster:** B.

### P1.3 — Three-mode encoding for workspace.json round-trip

- **Gap:** `ViewState.state.mode = "source"` + `source: true|false` encoding not supported. Corbomite has two modes only (`Editing`, `Reading`).
- **Obsidian behaviour:** `editor-markdown.md §2, §8 item 2` — `{mode: "source", source: false}` is live-preview.
- **Corbomite today:** `NoteEditorWidget::ViewMode {Editing, Reading}` (`NoteEditorWidget.h:21`).
- **Proposed work:** Introduce a third mode in `ViewMode`. Serialise as `{mode, source}`. This gates feature parity (live-preview) *and* compat (can't round-trip workspace.json without the encoding).
- **Cluster:** E — editor three-mode pivot.

### P1.4 — Visual-line float scroll persistence

- **Gap:** `workspace.json` stores `state.scroll` as a float (`42.73` = line 42, 73% through). Corbomite stores pixel scroll. Values don't round-trip correctly on font-size/zoom change.
- **Obsidian behaviour:** `editor-markdown.md §8 item 14, §12 "Scroll is a visual-line float"`.
- **Corbomite today:** Unverified; likely pixel.
- **Proposed work:** `Markoff::ReadingView::getScroll()` returns visual-line float; `applyScroll` interpolates per-section heights.
- **Cluster:** E.

### P1.5 — Moment.js format compat (Daily Notes / Templates)

- **Gap:** `{{date:YYYY-MM-DD}}` template substitution produces wrong output because `QDateTime::toString` uses different tokens (`yyyy` vs `YYYY`, `d` vs `D`, `AP` vs `A`).
- **Obsidian behaviour:** `leaf-utilities.md §1 moment, §11`; `ui-bundle.md §1 MomentFormatComponent, §2`.
- **Corbomite today:** No Moment support.
- **Proposed work:** `libs/core/MomentFormat.{h,cpp}` — translator from Moment.js tokens to `QDateTime::toString` tokens, handling `[escape]` brackets, locale-dependent tokens, ordinals (`Do`, `Qo`). Alternative: vendor moment.js (but adds ~50 KB JS that C++ code can't call). Translator preferred.
- **Cluster:** F — template / daily-notes bundle.

### P1.6 — YAML library + frontmatter helpers

- **Gap:** Corbomite's `Markoff::Document` stores frontmatter as a raw `QString`; does not parse it. Tags from `tags:`, aliases from `aliases:`, any key access — all unavailable.
- **Obsidian behaviour:** `parsing.md §1 parseYaml, stringifyYaml, parseFrontMatterTags, parseFrontMatterAliases, parseFrontMatterEntry, parseFrontMatterStringArray` — YAML 1.2, eemeli/yaml v2, `nullStr: ""`, `lineWidth: 0`, `aliasDuplicateObjects: false`.
- **Corbomite today:** No YAML library. (`parsing.md §11`.)
- **Proposed work:** Link `yaml-cpp` (or vendor `libyaml`). Write `libs/core/FrontMatter.{h,cpp}` exposing the four `parseFrontMatter*` helpers + `parseYaml`/`stringifyYaml`. YAML 1.2 core schema. Configure emitter to match Obsidian's options exactly: `null` → empty string; no line-wrap; no anchors. Comments are not preserved (document this).
- **Cluster:** A.

### P1.7 — `parseLinktext` + `stripHeading` + `stripHeadingForLink` + `resolveSubpath`

- **Gap:** Four functions that together define Obsidian's wikilink grammar. All missing in Corbomite.
- **Obsidian behaviour:** `parsing.md §1, §11`; `leaf-utilities.md §1, §14`; `VAULT-FORMAT.md §4.4, §7`.
- **Corbomite today:** Implicit inline logic in `SQLiteIndex.cpp`; no dedicated helpers.
- **Proposed work:** New `libs/core/LinkUtils.{h,cpp}` — pure functions, port regex verbatim (`AT` for `stripHeading`, `PT` for `stripHeadingForLink`), implement `parseLinktext` as first-`#`-split, port `resolveSubpath` three-way dispatch including nested heading paths.
- **Cluster:** A.

### P1.8 — `processFrontMatter` atomic mutator

- **Gap:** No atomic `fileManager.processFrontMatter(file, mut)` entry point — the plugin-API contract for safe frontmatter edits.
- **Obsidian behaviour:** `vault.md §1, §10`; `parsing.md §1 processFrontMatter`.
- **Corbomite today:** No equivalent.
- **Proposed work:** New `libs/filemanager/FileManager::processFrontMatter(NoteMeta, std::function<void(QVariantMap&)>)`. Inside: `vault.process(file, text → ...)` reads → parses → `mut(fm)` → re-serialises → splices back. Preserve key insertion order.
- **Cluster:** A.

### P1.9 — `Vault.process` atomic read-modify-write

- **Gap:** No per-file serialised mutator; concurrent frontmatter edits can race.
- **Obsidian behaviour:** `vault.md §1 Vault.process`.
- **Corbomite today:** No equivalent.
- **Proposed work:** `VaultService::processNote(NoteMeta, std::function<QString(const QString&)>)`. Per-file queue so concurrent edits serialise.
- **Cluster:** B.

### P1.10 — `DataAdapter` missing ops

- **Gap:** Corbomite's `FileSystemAdapter` lacks `stat`, `list`, `readBinary`, `writeBinary`, `append*`, `process`, `copy`, `rmdir`, `getResourcePath`, `watch(handler)` with structured events, `watchHiddenRecursive`, `applyWriteOptions({ctime, mtime, immediate})`.
- **Obsidian behaviour:** `vault.md §1 FileSystemAdapter`.
- **Corbomite today:** `libs/storage/FileSystemAdapter` has basic read/write/rename/remove/trash/exists/mkpath.
- **Proposed work:** Fill out the API. `immediate` hook is important for `Vault.modify` cache atomicity.
- **Cluster:** B.

### P1.11 — Case-sensitivity probe

- **Gap:** No `.OBSIDIANTEST` probe; Corbomite behaves wrong on case-insensitive FS (macOS/Windows).
- **Obsidian behaviour:** `vault.md §3 "Case sensitivity", §8`.
- **Corbomite today:** Missing.
- **Proposed work:** `FileSystemAdapter::probeCaseSensitivity()` at vault open; cache `insensitive` flag. `getAvailablePath` uses insensitive lookup to avoid clobber.
- **Cluster:** B.

### P1.12 — `.trash/` local-trash mode

- **Gap:** Corbomite only supports system trash; `trashOption: 'local'` config path (write to `.trash/<basename> 2.ext`) missing.
- **Obsidian behaviour:** `vault.md §3 ".trash/", §1 FileSystemAdapter.trashLocal`.
- **Corbomite today:** `moveToTrash` uses system only.
- **Proposed work:** Implement `trashLocal` with collision suffix scheme. Route via `FileManager::trashFile` which branches on `trashOption` config key.
- **Cluster:** B.

---

## P2 — Feature parity (user-visible)

### P2.1 — `ViewRegistry` refactor

- **Gap:** No extension → view-type registry; `EditorViewSpace.cpp:119` branches on `endsWith(".canvas")`.
- **Obsidian behaviour:** `views.md §1 ViewRegistry, §7 "Built-in registrations"`. Six built-in viewType ↔ extension bindings (`md`, image, audio, video, `pdf`, `release-notes`) + Canvas + Bases.
- **Proposed work:** `Corbomite::ViewRegistry` (`libs/workspace/` or `libs/views/`). Qt signals `viewRegistered/Unregistered/extensionsUpdated`. `registerView(type, factory)` throws on duplicate; `registerExtensions(exts, type)` atomic across array. Use it from `EditorViewSpace` for file-type dispatch.
- **Cluster:** G — views hierarchy.

### P2.2 — `View` / `ItemView` / `FileView` / `TextFileView` hierarchy

- **Gap:** No subclass hierarchy; `NoteEditorWidget` is flat.
- **Obsidian behaviour:** `views.md §1`. Required overrides per class.
- **Proposed work:** `Corbomite::View` (abstract, inherits `QObject` + `Component` lifecycle). `ItemView` adds header+actions. `FileView` binds to `TFile`-equivalent + breadcrumbs + rename/delete reactions. `TextFileView` adds 2 s debounced save + `getViewData`/`setViewData`/`clear` + three-way merge.
- **Cluster:** G.

### P2.3 — Three-way merge on external modify

- **Gap:** Corbomite reloads disk version, clobbering unsaved edits.
- **Obsidian behaviour:** `views.md §1 TextFileView.loadFileInternal` — `FX(previousLastSaved, currentViewData, freshDiskData)` diff-match-patch merge.
- **Proposed work:** Port `FX` (diff-match-patch) into `libs/core/TextMerge.{h,cpp}`. Wire into `NoteEditorWidget::onExternalModify`. Bases sets `isPlaintext = false` to skip merge.
- **Cluster:** G.

### P2.4 — Save-failure backup

- **Gap:** Unsaved content lost on write failure.
- **Obsidian behaviour:** `views.md §1 TextFileView.save, "On throw"`; `01-markoff-gaps.md §views`.
- **Proposed work:** `libs/storage/RecoveryStore` keyed by note path + timestamp; triggered by `NoteEditorWidget::onSaveFailed`.
- **Cluster:** G.

### P2.5 — Markoff three-mode live-preview semantics

- **Gap:** Live-preview (`{mode: "source", source: false}`) missing. Per-block cursor-reveal missing.
- **Obsidian behaviour:** `editor-markdown.md §1, §8 item 2, §12`; `editor.md §1 editorLivePreviewField`.
- **Proposed work:** QGraphicsView migration per `docs/superpowers/specs/2026-04-03-markoff-migration-design.md`. Per-block `MarkdownTextItem` that swaps between source text and rendered sub-widget based on cursor-in-block state.
- **Cluster:** E.

### P2.6 — Progressive section rendering + recycling

- **Gap:** Corbomite renders whole-document; no worker parse, no section recycle, no virtual scroll. Performance cliff on > 10 KB notes.
- **Obsidian behaviour:** `editor-markdown.md §1 MarkdownPreviewRenderer, §8, §12`.
- **Proposed work:** `libs/markoff-parser/` emits section array; `SceneCoordinator` recycles `MarkdownTextItem`s keyed on HTML-string equality (or source-hash + frontmatter-hash); 5 ms / 10-section budget; selection preservation across virtualisation.
- **Cluster:** E.

### P2.7 — Hover-link preview infrastructure

- **Gap:** No hover preview on wikilinks.
- **Obsidian behaviour:** `ui-bundle.md §1 HoverPopover, §12`; `workspace.md §7 hoverLinkSources`; `editor-markdown.md §12`. **300 ms hover delay** (not 500 ms — that's the global poll; Pass 1 correction 3).
- **Proposed work:** Three pieces — (1) `Workspace::registerHoverLinkSource(id, HoverLinkSource)` + `hoverLinkSources` map; (2) `Markoff::Editor`/`ReadingView` emit `hover-link` signal with `{event, source, hoverParent, targetEl, linktext, sourcePath?}`; (3) `Corbomite::HoverPopover` — frameless `QWidget` hosting `Markoff::ReadingView`; 300 ms delay per-popover; 500 ms global poll tracking `elementFromPoint`-equivalent; `setIsFocused(true)` pin; `childHovers` chain; `watchResize` 10-call cap; `staticPos` anchor-to-mouse when target > 300 px tall.
- **Cluster:** H — hover / suggester parity.

### P2.8 — Five MetadataCache event signals

- **Gap:** `SQLiteIndex::indexReady` is one signal; Obsidian has five: `changed`, `deleted`, `resolve`, `resolved`, `finished`.
- **Obsidian behaviour:** `metadata.md §4`.
- **Proposed work:** Split into `noteMetadataChanged(path)`, `noteDeleted(path)`, `linkResolvedFor(path)`, `linksDrained()`, `initialIndexFinished()`. Respect Obsidian's ordering guarantee (`changed` → async tick → `resolve` → queue drained → `resolved` → 10 ms debounce → `finished`).
- **Cluster:** I — MetadataCache parity.

### P2.9 — `CachedMetadata` shape exposure

- **Gap:** No `NoteMetadata` struct matching `CachedMetadata`'s keys. Backlinks pane / graph view / properties / embed rendering all need it.
- **Obsidian behaviour:** `metadata.md §2`; `VAULT-FORMAT.md §10`.
- **Proposed work:** `libs/core/NoteMetadata.h` with all fields (`links`, `embeds`, `tags`, `headings`, `sections`, `listItems`, `footnoteRefs`, `footnotes`, `blocks`, `frontmatter`, `frontmatterLinks`, `frontmatterPosition`). Populated by Markoff parser; mirrored into SQLite for persistence.
- **Cluster:** I.

### P2.10 — Headings / sections / blocks / footnotes cache

- **Gap:** Subpath anchor resolution (`[[Note#Heading]]`, `[[Note#^block]]`, `[[Note#[^fn]]]`) requires `headings[]`, `sections[]`, `blocks{}`, `footnotes[]` in the cache.
- **Obsidian behaviour:** `metadata.md §2, §9`; `parsing.md §1 resolveSubpath`.
- **Proposed work:** Extend `libs/storage/SQLiteIndex` with `headings`, `sections`, `blocks`, `footnotes` tables. Or just store the full `NoteMetadata` serialised per-note.
- **Cluster:** I.

### P2.11 — `userIgnoreFilters` wired

- **Gap:** Config key not honoured; ignored paths still appear in file explorer + metadata cache.
- **Obsidian behaviour:** `metadata.md §3`; `vault.md §2`.
- **Proposed work:** `QList<QRegularExpression>` from config into `VaultScanner` + `SQLiteIndex`. Invalid regex logs warning, no crash.
- **Cluster:** B.

### P2.12 — Embed rendering (`![[Note]]`)

- **Gap:** Inline file embeds not rendered.
- **Obsidian behaviour:** `editor-markdown.md §1 MarkdownPreviewView, §12`; `core.md §2 embedRegistry (aJ)`.
- **Proposed work:** `Corbomite::EmbedRegistry` (libs/core/): extension → embed factory. Built-in handlers for image/audio/video/pdf/markdown. Per-embed mini `MarkdownPreviewView`. Subpath resolution via `resolveSubpath`. Depth guard (`JZ`) walking ancestors; default cap ~8.
- **Cluster:** J — embed / rendering primitives.

### P2.13 — Bases (8–10 weeks)

- **Gap:** No `.base` support.
- **Obsidian behaviour:** `bases.md` entire document; `VAULT-FORMAT.md §6`.
- **Proposed work:** Largest single work item. New `libs/bases/` library: `Value` hierarchy (18 classes) via `std::variant<...>` + `IValue` interface; `BasesQuery`/`BasesViewConfig`/`BasesEntry`/`BasesQueryResult` plumbing; YAML round-trip with `unrecognizedData` preservation at every level; filter/formula parser (source out-of-scope — follow-up from bases.md §13 Q1; Obsidian docs at <https://help.obsidian.md/bases/functions>); `BasesView` widget + Table/Cards/List layouts; cell renderers + inline-edit via `MetadataTypeManager` widget factory; `+ New` integration with `FileManager`; CSV/TSV/MD/HTML export; view-rename link rewrite. Estimate: ~3-4 weeks formula engine, ~2 weeks Value hierarchy + config plumbing, ~3 weeks Qt table widget + inline-edit. Blocks on P1.6 (YAML) + P1.7 (link helpers) + P2.9 (NoteMetadata).
- **Cluster:** K — Bases complete (stand-alone).

### P2.14 — Properties panel (in-document frontmatter editor)

- **Gap:** Users can't edit frontmatter inline; `propertiesInDocument` config not honoured.
- **Obsidian behaviour:** `editor-markdown.md §1, §9`; `metadata.md §1 getAllPropertyInfos`.
- **Proposed work:** `Corbomite::PropertiesPanel` widget attached to top of `NoteEditorWidget`. `MetadataTypeManager`-equivalent widget registry with per-type widgets (`text`, `number`, `checkbox`, `date`, `datetime`, `tags`, `aliases`, `multitext`, `list`). Writes through `FileManager::processFrontMatter`. Blocks on P1.6 (YAML) + P1.8 (processFrontMatter).
- **Cluster:** L — properties / templates.

### P2.15 — Daily Notes

- **Gap:** Internal plugin missing.
- **Obsidian behaviour:** `core.md §7` "daily-notes"; `VAULT-FORMAT.md §3 app.json` (date-format key).
- **Proposed work:** Command `daily-notes:open-today` → open-or-create with Moment-formatted filename in configured folder, with configured template. Blocks on P1.5 (Moment compat).
- **Cluster:** F.

### P2.16 — Templates

- **Gap:** Internal plugin missing.
- **Obsidian behaviour:** `core.md §7` "templates".
- **Proposed work:** Template file picker + `{{date:FMT}}`, `{{time:FMT}}`, `{{title}}` substitution. Blocks on P1.5.
- **Cluster:** F.

### P2.17 — Graph view `.obsidian/graph.json` compat

- **Gap:** Corbomite has a graph view (`src/graph/`, `libs/forcegraph/`); persistence schema against Obsidian's `graph.json` unconfirmed.
- **Obsidian behaviour:** `VAULT-FORMAT.md §3 graph.json` (schema not extracted).
- **Proposed work:** Extract schema from the graph internal plugin (follow-up); validate Corbomite's persistence against it.
- **Cluster:** M — internal-plugin-feature audits.

### P2.18 — Canvas `.canvas` schema compat

- **Gap:** Canvas works in Corbomite (`libs/canvas/`, `src/canvas/`); byte-compat against Obsidian's `.canvas` schema unconfirmed.
- **Obsidian behaviour:** JsonCanvas 1.0 spec at <https://jsoncanvas.org/>.
- **Proposed work:** Validate `libs/canvas/` against the public spec; add round-trip test.
- **Cluster:** M.

### P2.19 — Search DSL parser

- **Gap:** `SearchPanel.cpp:71 // TODO: Support Obsidian search operators`. Plain FTS5 `MATCH` only.
- **Obsidian behaviour:** `search.md §13 Q1` — DSL is `tag:`, `path:`, `file:`, `line:`, `block:`, `section:`, `/regex/`, `"quoted"`, `-exclusion`, `match-case`, `whole-word`, AND/OR/NOT. Source in `plugin/internal-plugins/global-search/`, not extracted.
- **Proposed work:** Reverse-engineer DSL from Obsidian user documentation. Write `libs/search/QueryParser.{h,cpp}` producing a query tree. Execute against `SQLiteIndex` using a combination of FTS5 candidate fetch + per-operator post-filter.
- **Cluster:** D.

### P2.20 — Fuzzy matcher (shared across Quick Switcher / Command Palette / every Suggester)

- **Gap:** Quick Switcher uses `QRegularExpression`; ranking diverges from Obsidian.
- **Obsidian behaviour:** `search.md §1 prepareQuery/fuzzySearch/prepareFuzzySearch/prepareSimpleSearch/sortSearchResults, §2 "Scoring formula"`.
- **Proposed work:** Port the two-pass (word-token → char-fuzzy) algorithm and five-term scoring formula (contiguity + case + span + start + length) verbatim. `libs/search/Fuzzy.{h,cpp}` exposing `prepareQuery`, `fuzzySearch`, `prepareFuzzySearch`, `prepareSimpleSearch`, `sortSearchResults`. CJK per-codepoint tokenisation via `Ey` regex.
- **Cluster:** D.

### P2.21 — Highlight-span rendering

- **Gap:** No shared renderer; every search/suggester consumer re-implements.
- **Obsidian behaviour:** `rendering.md §1 renderResults`.
- **Proposed work:** `Markoff::renderHighlightedRuns(QTextCursor, QString text, QList<QPair<int,int>> matches, int offset=0)` — single chokepoint. Consumed by search panel, quick switcher, command palette, every suggester.
- **Cluster:** D.

### P2.22 — `RenderContext` inline-primitive renderer

- **Gap:** `renderFileLink`/`renderExternalLink`/`renderTag` missing as a shared primitive.
- **Obsidian behaviour:** `rendering.md §1`.
- **Proposed work:** `Markoff::LinkRenderer` (or `libs/core/InlineRenderer`): emit wikilink / external link / tag DOM-equivalent (Qt widget or text fragment) with click + context-menu-sections + drag + hover-link wiring. Required by Bases, Backlinks pane, Outline, preview.
- **Cluster:** J.

### P2.23 — `aJ` EmbedRegistry

- **Gap:** No embed registry.
- **Obsidian behaviour:** `core.md §2 App.embedRegistry = new aJ()`; `02-extension-surfaces.md §registry patterns`.
- **Proposed work:** See P2.12.
- **Cluster:** J.

### P2.24 — Menu section-ordering protocol

- **Gap:** No `addSections(ids)` + `setSection(id)` + sort-with-separators behaviour.
- **Obsidian behaviour:** `ui-bundle.md §2 "Canonical section orders"`; `workspace.md §10`.
- **Proposed work:** `Corbomite::MenuSectionRouter` helper. Buffer `(section, QAction*)` pairs; `apply(QMenu*)` sorts by declared order, inserts separators between non-empty sections, wraps sections with `submenuConfig` in child menus. Called from every menu-signal emitter.
- **Cluster:** H.

### P2.25 — Mid-construction menu signals

- **Gap:** No `file-menu`, `url-menu`, `editor-menu`, `files-menu`, `leaf-menu`, `tab-group-menu`, `markdown-viewport-menu` Qt signals.
- **Obsidian behaviour:** `workspace.md §4, §10`.
- **Proposed work:** `Workspace::fileMenuRequested(QMenu*, NoteMeta*, QString source, WorkspaceLeaf*)` (and analogous for the other six). Emit after built-in items added, before menu exec. Plugins connect and call `menu->addAction(...)` on their own.
- **Cluster:** H.

### P2.26 — Leaf-close undo (cap 10)

- **Gap:** `Mod+Shift+T` unsupported.
- **Obsidian behaviour:** `workspace.md §1 undoHistory, §7`.
- **Proposed work:** `WorkspaceImpl::undoHistory` (10-entry deque). `pushUndoHistory(leaf, parentId, rootId)` from leaf detach; `undoClosePane()` command reconstructs in original container.
- **Cluster:** G.

### P2.27 — Per-leaf history (cap 20)

- **Gap:** Per-pane back/forward stack missing.
- **Obsidian behaviour:** `workspace.md §1 qD`.
- **Proposed work:** `Corbomite::LeafHistory { backHistory, forwardHistory }` (cap 20), `back/forward/go/updateState/serialize/deserialize`. On vault rename, rewrite every entry's `state.state.file`.
- **Cluster:** G.

### P2.28 — Popout windows

- **Gap:** No `workspace:open-in-new-window` / `move-to-new-window`.
- **Obsidian behaviour:** `workspace.md §1 WorkspaceWindow`.
- **Proposed work:** `QMainWindow` subclass hosting a detached `EditorViewManager`. Cross-window drag-drop. Per-window persisted `geometry`/`maximize`/`zoom`. `Platform.canPopoutWindow` gate.
- **Cluster:** G.

### P2.29 — Stacked tabs mode

- **Gap:** `workspace:toggle-stacked-tabs` unsupported.
- **Obsidian behaviour:** `workspace.md §1 setStacked`.
- **Proposed work:** Alternative layout mode for `WorkspaceTabs`. Persistence via `stacked: true` in workspace.json.
- **Cluster:** G.

### P2.30 — Tab pin + linked-pane group

- **Gap:** No tab pin; no linked-pane group.
- **Obsidian behaviour:** `workspace.md §1 setPinned, setGroup, §8`.
- **Proposed work:** Per-tab `pinned: bool` (skip in `getLeaf` recycle). Linked-pane group id propagates on navigate; pin-on-one pins all members.
- **Cluster:** G.

### P2.31 — Ribbon plugin slot

- **Gap:** No `Workspace::addRibbonIcon(id, icon, title, cb)` surface.
- **Obsidian behaviour:** `workspace.md §7 WorkspaceRibbon.items, §10`.
- **Proposed work:** Extend the left toolbar with a plugin-ribbon API. Drag-reorder. Right-click context-menu to hide individual items. Persist as `['left-ribbon'].hiddenItems` in workspace.json (key order = item order).
- **Cluster:** H.

### P2.32 — Command registry with variant callbacks

- **Gap:** `KActionCollection` doesn't support Obsidian's `checkCallback`/`editorCallback`/`editorCheckCallback` variants.
- **Obsidian behaviour:** `core.md §7 Command shape`.
- **Proposed work:** `Corbomite::CommandRegistry` wrapping `KActionCollection` with `QAction::isEnabled()` gated on `checkCallback`-equivalent. `editorCallback` routes to current `MarkdownView`.
- **Cluster:** C.

### P2.33 — Hotkeys plugin-id-prefixed ID convention

- **Gap:** Command IDs not namespaced `"<plugin-id>:<cmd-id>"`. Required for `.obsidian/hotkeys.json` compat.
- **Obsidian behaviour:** `plugin.md §10 addCommand, §8 "manifest.id-prefix is mandatory"`.
- **Proposed work:** Command registry auto-namespaces IDs. Built-in commands use `"workspace:"`, `"editor:"`, `"app:"` namespaces matching Obsidian (see `workspace.md §6`, `core.md §6`).
- **Cluster:** C.

### P2.34 — Obsidian protocol handler registry

- **Gap:** `obsidian://<action>?...` URLs unsupported.
- **Obsidian behaviour:** `workspace.md §7 protocolHandlers, §1 registerObsidianProtocolHandler`.
- **Proposed work:** `KDBusService(Unique)` for single-instance + custom URL scheme handler. `Workspace::registerObsidianProtocolHandler(action, handler)` throws on duplicate. Map all 11 built-in actions (`open`, `new`, `search`, `show-plugin`, `show-theme`, `show-release-notes`, `debug-info`, `publish-sites`, `sync-setup`, `vault-setup`, `hook-get-address`).
- **Cluster:** N — plugin-ready surfaces.

---

## P3 — Plugin-ready prep (primitives for future plugin API)

### P3.1 — `Component` lifecycle

- **Gap:** No universal `load/unload` + child tree + cleanup-thunk queue.
- **Obsidian behaviour:** `ui-bundle.md §1 Component, §8`.
- **Proposed work:** `libs/core/Component.{h,cpp}`. `QObject` subclass with `registerCleanup(lambda)` LIFO queue. Methods: `load()`, `unload()`, `onload()`, `onunload()`, `addChild(c)`, `removeChild(c)`, `register(fn)`, `registerEvent(QMetaObject::Connection)`, `registerDomEvent(...)` (= Qt signal-slot), `registerInterval(QTimer*)`.
- **Cluster:** C.

### P3.2 — `Events` mixin facade over Qt signals

- **Gap:** Qt signals are typed; plugins want `emitter.on("name", cb)` dynamic subscription.
- **Obsidian behaviour:** `core.md §1 Events`; async re-throw semantics (`tryTrigger`).
- **Proposed work:** `libs/core/Events` — mixin (CRTP or multiple inheritance) exposing `on(name, cb, ctx)` → `EventRef`, `off(name, cb)` identity match, `offref(ref)`, `trigger(name, args...)`, `tryTrigger`. Per-listener `try/catch` + `QTimer::singleShot(0, [] { throw; })` for async rethrow. Backed by `QVariantList` for dynamic args.
- **Cluster:** C.

### P3.3 — `Scope` hierarchical hotkey stack

- **Gap:** Qt has no stack-scoped hotkeys; `KActionCollection` is per-action-context.
- **Obsidian behaviour:** `core.md §1 Scope, §11`.
- **Proposed work:** `libs/core/Scope.{h,cpp}` via `QApplication::installEventFilter`. Each scope owns `QList<KeyHandler>`; `handleKey` walks keys; falls back to parent for catch-all. `Modal`, `Menu`, `EditorSuggest`, `HoverPopover` all allocate child scopes.
- **Cluster:** C.

### P3.4 — `ViewRegistry` as registry-of-registries

- **Gap:** Central factory table missing. (Also see P2.1.)
- **Obsidian behaviour:** `views.md §7`.
- **Proposed work:** Done as P2.1.
- **Cluster:** G.

### P3.5 — Command registry with all variants

- **Gap:** See P2.32.
- **Proposed work:** See P2.32.
- **Cluster:** C.

### P3.6 — SettingTab registration API

- **Gap:** `SettingsDialog::addSettingsPage(id, name, icon, factory)` missing; plugin settings tab registration has no entry point.
- **Obsidian behaviour:** `settings.md §1 PluginSettingTab, §10`.
- **Proposed work:** `SettingsDialog::addPluginPage(plugin-id, name, icon, QWidget*)` + auto-removal on plugin unload. `display()`-on-open contract: rebuild UI on every tab switch.
- **Cluster:** N.

### P3.7 — `Setting` fluent builder

- **Gap:** Corbomite uses `QFormLayout` directly; plugins want `new Setting(el).setName(...).addToggle(t => t.setValue(...).onChange(...))`.
- **Obsidian behaviour:** `settings.md §1 Setting`.
- **Proposed work:** `Corbomite::Setting` thin wrapper over `QFormLayout` row with chainable builder returning `*this`. Twelve `add*` methods mapping to Qt widgets. Optional (not compat-blocking).
- **Cluster:** N.

### P3.8 — Plugin manifest parser + data.json watcher

- **Gap:** `.obsidian/plugins/<id>/manifest.json` + `data.json` reader + self-edit suppression + `onExternalSettingsChange` not wired.
- **Obsidian behaviour:** `plugin.md §1, §3`.
- **Proposed work:** `libs/pluginhost/PluginLoader`. Parse manifest; construct `Plugin`; inject `dir`. `data.json` via `QSaveFile` atomic write (improving on Obsidian's non-atomic). `QFileSystemWatcher` on `data.json`; debounce 50 ms; compare against last-written mtime.
- **Cluster:** N.

### P3.9 — MarkdownPostProcessor / CodeBlockProcessor registries

- **Gap:** No post-processor or code-block-processor registry.
- **Obsidian behaviour:** `editor-markdown.md §7, §10`; `plugin.md §10`.
- **Proposed work:** `MarkoffRenderEngine::registerPostProcessor(fn, sortOrder)` + `registerCodeBlockProcessor(lang, fn)`. Post-processors walk rendered `QGraphicsScene`/`QTextDocument`. Code-block processor wraps with `ctx.replaceCode(newSrc)` back-channel — requires source-position tracking in the scene graph. Both fire `post-processor-change` signal on register/unregister so live ReadingViews re-render.
- **Cluster:** J.

### P3.10 — EditorSuggest registry (insertion-order iteration)

- **Gap:** Trigger detection hardcoded.
- **Obsidian behaviour:** `editor.md §1 EditorSuggest, §12`.
- **Proposed work:** `Markoff::SuggestRegistry` — insertion order, first non-null `onTrigger` wins (so plugin overrides of `[[`/`#` are shadowed, not prioritised — compat!). `EditorSuggest<T>` abstract base with `onTrigger/getSuggestions/renderSuggestion/selectSuggestion`. Async `getSuggestions` re-checks `editor.hasFocus()` before showing.
- **Cluster:** H.

### P3.11 — HoverLinkSource registry

- **Gap:** See P2.7.
- **Proposed work:** See P2.7 (part 1).
- **Cluster:** H.

### P3.12 — OperatorFuncConfigs (Bases)

- **Gap:** No `workspace.registerOperatorFuncConfigs` analogue.
- **Obsidian behaviour:** `workspace.md §7`; `bases.md §10`.
- **Proposed work:** Part of Bases (P2.13) + `libs/formula/` parser. Plugin-added operators via `registerOperatorFuncConfigs(id, config)`.
- **Cluster:** K.

### P3.13 — `registerFileParentCreator`

- **Gap:** No plugin hook for "where does a new file of extension X get created?"
- **Obsidian behaviour:** `vault.md §10 FileManager.registerFileParentCreator`.
- **Proposed work:** `FileManager::registerFileParentCreator(ext, fn)` + `unregisterFileCreator(ext)` + `canCreateFileWithExt(ext)`. Internal plugins canvas/bases register their own.
- **Cluster:** N.

### P3.14 — `CachedMetadata` exposure (plugin-visible metadata contract)

- **Gap:** See P2.9.
- **Proposed work:** See P2.9, exposed through a compat shim `app.metadataCache.getFileCache(file)` returning an object with the exact shape from `VAULT-FORMAT.md §10`.
- **Cluster:** I.

---

## P4 — Nice-to-have / deferred

### P4.1 — Deferred-load view stubs (`eD`)

- **Gap:** Large workspaces load every tab eagerly.
- **Obsidian behaviour:** `views.md §1 eD`.
- **Proposed work:** Stub view that paints cached icon+title and defers real view construction until first interaction. Performance optimisation, not compat-blocking.

### P4.2 — Turndown shim (`htmlToMarkdown`)

- **Gap:** Paste-from-browser inserts HTML literally.
- **Obsidian behaviour:** `rendering.md §1 htmlToMarkdown`.
- **Proposed work:** `Markoff::htmlToMarkdown` function. Use vendored Turndown (JS) or a C++ port. Allowlist of supported tags a follow-up (Turndown rule list `hP` not extracted).

### P4.3 — DOMPurify shim (`sanitizeHTMLToDom`)

- **Gap:** No sanitiser for plugin-supplied HTML.
- **Obsidian behaviour:** `rendering.md §1 sanitizeHTMLToDom`.
- **Proposed work:** `Markoff::sanitizeHtml(QString, QTextDocument* target)`. Allowlist `SL` not extracted — follow-up before implementation (security-critical).

### P4.4 — PDF.js equivalent

- **Gap:** No PDF view, no PDF embeds.
- **Obsidian behaviour:** `rendering.md §1 loadPdfJs`; `views.md §7 "pdf" built-in`.
- **Proposed work:** Poppler-Qt6 (lighter) or Okular KPart (richer, KDE-native). Register for `.pdf` extension in `ViewRegistry`. Embed rendering via `EmbedRegistry`.

### P4.5 — Batch math typeset

- **Gap:** `finishRenderMath` debounce-batches typeset; Corbomite renders per-equation.
- **Obsidian behaviour:** `rendering.md §1 finishRenderMath`.
- **Proposed work:** Low priority — JKQT is per-call fast. Plugin-API shim exposes `finishRenderMath` as no-op resolved future.

### P4.6 — `MomentFormatComponent` widget

- **Gap:** No live-preview format-string editor.
- **Obsidian behaviour:** `ui-bundle.md §1 MomentFormatComponent`.
- **Proposed work:** `Corbomite::MomentFormatLineEdit` + live-sample-binding. Blocks on P1.5 (Moment compat).
- **Cluster:** F.

### P4.7 — Lucide icon shim

- **Gap:** Plugin icons using `lucide-*` names must be satisfied.
- **Obsidian behaviour:** `ui-bundle.md §11 "Icon translation table"`.
- **Proposed work:** `libs/core/IconMap.h` with 125 Lucide IDs → Freedesktop names (~70% coverage) + bundled SVGs under `resources/icons/obsidian-lucide-shim/` (30% fallback).

### P4.8 — `loadMathJax`/`loadMermaid`/`loadPrism` plugin-compat shims

- **Gap:** Plugins call these lazy-loaders. Need no-op resolved-future shims.
- **Obsidian behaviour:** `rendering.md §1 load*`.
- **Proposed work:** Trivial `QFuture<void>::makeReady()` returners in the plugin-API surface.

### P4.9 — Touch gestures

- **Gap:** Double-tap to toggle mode, pinch-zoom image viewer, swipe-to-open sidedock.
- **Obsidian behaviour:** mobile-only.
- **Proposed work:** N/A (Corbomite is desktop-only).

### P4.10 — macOS native menus

- **Gap:** `Menu.useNativeMenu` on macOS.
- **Obsidian behaviour:** `ui-bundle.md §1 Menu.useNativeMenu`.
- **Proposed work:** `QMenu` already uses platform chrome on macOS. Probably no work needed.

---

## Recommended implementation clusters

These clusters bundle gaps that share dependencies and should ship together.

### Cluster A — Link/frontmatter correctness (P0.1–P0.3, P0.5, P1.6, P1.7, P1.8)

**Goal.** Correct link resolution, correct frontmatter I/O, correct wikilink grammar. The foundation for everything else.

**Contents.**
- Shortest-path link resolver (P0.1).
- EOF-tolerant frontmatter (P0.2).
- Wikilink `#subpath` + `|alias` indexing (P0.3, P0.5).
- YAML library + frontmatter helpers (P1.6).
- `parseLinktext`/`stripHeading`/`stripHeadingForLink`/`resolveSubpath` (P1.7).
- `processFrontMatter` atomic mutator (P1.8).

**Shared dependency.** `libs/core/LinkUtils` + `libs/core/FrontMatter` (YAML) both live here. Once shipped, every subsequent feature that reads frontmatter or resolves subpaths is unblocked.

**Estimate.** 2–3 weeks.

### Cluster B — Vault I/O (P0.6, P1.1, P1.2, P1.9, P1.10, P1.11, P1.12, P2.11)

**Goal.** Vault-format compat — read/write every `.obsidian/*.json` file with Obsidian's schema. Round-trip the layout. Case-sensitivity probe. Local trash. User ignore filters.

**Contents.**
- `VaultConfig` for all 6 `.obsidian/*.json` files (P1.1).
- `workspace.json` read/write with correct schema (P1.2, P0.6).
- `vault.process` atomic RMW (P1.9).
- Filled-out `DataAdapter` interface (P1.10).
- Case-sensitivity probe (P1.11).
- `.trash/` local mode (P1.12).
- `userIgnoreFilters` wiring (P2.11).

**Estimate.** 3–4 weeks.

### Cluster C — Lifecycle / plugin primitives (P0.7, P2.32, P2.33, P3.1, P3.2, P3.3, P3.5)

**Goal.** Per-vault service-graph teardown/rebuild; plugin-API lifecycle primitives.

**Contents.**
- Vault-switch process-death pattern (P0.7).
- `Component` lifecycle (P3.1).
- `Events` mixin (P3.2).
- `Scope` hotkey stack (P3.3).
- Command registry variants (P2.32, P2.33, P3.5).

**Rationale.** These are the Corbomite-internal primitives every other plugin-ready work depends on. Building them before Bases / Properties / plugin API avoids retrofit.

**Estimate.** 2 weeks.

### Cluster D — Search / suggester parity (P0.4, P2.19, P2.20, P2.21)

**Goal.** Match Obsidian's search ranking and DSL everywhere (search panel, quick switcher, command palette, every suggester).

**Contents.**
- `SearchMatch` byte-range array (P0.4).
- Search DSL parser (P2.19).
- Ported fuzzy matcher (P2.20).
- Shared highlight-span renderer (P2.21).

**Estimate.** 1.5–2 weeks (without the DSL reverse-engineering). DSL parser adds 1–2 weeks if we do a full port; shorter if we adopt a subset and document the diffs.

### Cluster E — Markoff three-mode pivot (P1.3, P1.4, P2.5, P2.6)

**Goal.** Live-preview mode + workspace.json round-trip + progressive rendering.

**Contents.**
- Three-mode encoding (P1.3).
- Visual-line-float scroll (P1.4).
- Live-preview per-block semantics (P2.5).
- Progressive section rendering (P2.6).

**Rationale.** The editor's biggest architectural pivot. See `docs/superpowers/specs/2026-04-03-markoff-migration-design.md` + `docs/superpowers/plans/2026-04-03-editor-three-modes.md`.

**Estimate.** 6–8 weeks (ongoing per those specs).

### Cluster F — Templates / Daily Notes / Moment (P1.5, P2.15, P2.16, P4.6)

**Goal.** User-visible templating features that depend on Moment-format-string parity.

**Contents.**
- Moment format translator (P1.5).
- `MomentFormatComponent` widget (P4.6).
- Templates internal plugin (P2.16).
- Daily Notes internal plugin (P2.15).

**Estimate.** 1.5 weeks.

### Cluster G — Views hierarchy + TextFileView contract (P2.1–P2.4, P2.26–P2.30)

**Goal.** Mount Obsidian-style views with the full lifecycle contract. Unblocks plugin views.

**Contents.**
- ViewRegistry (P2.1).
- View/ItemView/FileView/TextFileView (P2.2).
- Three-way merge on external modify (P2.3).
- Save-failure backup (P2.4).
- Leaf-close undo (P2.26).
- Per-leaf history (P2.27).
- Popout windows (P2.28).
- Stacked tabs (P2.29).
- Tab pin + linked-pane group (P2.30).

**Estimate.** 3–4 weeks.

### Cluster H — Menus / hover / suggester UI (P2.7, P2.24, P2.25, P2.31, P3.10, P3.11)

**Goal.** The UI-primitives layer plugins hook into.

**Contents.**
- Hover-link preview infrastructure (P2.7 / P3.11).
- Menu section router (P2.24).
- Mid-construction menu signals (P2.25).
- Ribbon plugin slot (P2.31).
- Editor suggest registry (P3.10).

**Estimate.** 2–3 weeks.

### Cluster I — MetadataCache parity (P2.8, P2.9, P2.10, P3.14)

**Goal.** Match Obsidian's metadata model for plugin compat.

**Contents.**
- Five event signals (P2.8).
- `NoteMetadata` struct (P2.9).
- Headings/sections/blocks/footnotes cache (P2.10).
- Plugin-visible metadata contract shim (P3.14).

**Estimate.** 2 weeks.

### Cluster J — Embed / rendering primitives (P2.12, P2.22, P2.23, P3.9)

**Goal.** Share rendering primitives across every non-editor consumer (Bases, Backlinks, Outline, preview, hover).

**Contents.**
- EmbedRegistry + `![[Note]]` embeds (P2.12, P2.23).
- RenderContext-equivalent (`LinkRenderer`) (P2.22).
- MarkdownPostProcessor + CodeBlockProcessor registries (P3.9).

**Estimate.** 2–3 weeks.

### Cluster K — Bases (P2.13, P3.12)

**Goal.** Full `.base` support.

**Estimate.** 8–10 weeks. Blocks on Cluster A + Cluster I + Cluster J.

### Cluster L — Properties / Panel (P2.14)

**Goal.** In-document frontmatter editor.

**Estimate.** 1 week after Cluster A + Cluster I land.

### Cluster M — Internal-plugin-feature audits (P2.17, P2.18)

**Goal.** Validate existing Corbomite features (canvas, graph) against Obsidian schemas.

**Estimate.** 0.5 week per plugin.

### Cluster N — Plugin-ready surfaces (P2.34, P3.6, P3.7, P3.8, P3.13)

**Goal.** Extension points plugins register into.

**Contents.**
- Obsidian protocol handlers (P2.34).
- Plugin settings tab API (P3.6).
- Setting fluent builder (P3.7).
- Plugin manifest + data.json (P3.8).
- `registerFileParentCreator` (P3.13).

**Estimate.** 2 weeks. Deferred until after Cluster C + Cluster G land (Component lifecycle is the prerequisite).

---

## Sequencing recommendation

1. **Cluster A** (link/frontmatter correctness) — 2–3 weeks. Unblocks everything else that reads frontmatter or resolves subpaths.
2. **Cluster B** (vault I/O) — 3–4 weeks. Unblocks workspace.json round-trip, Settings UI, and all internal plugins that persist config.
3. **Cluster C** (lifecycle) — 2 weeks. Unblocks Cluster G and Cluster N.
4. **Cluster D** (search) — 2–3 weeks. Independent of A/B/C for the matcher; DSL parser depends on Cluster I.
5. **Cluster I** (MetadataCache) — 2 weeks. Builds on Cluster A.
6. **Cluster G** (views) — 3–4 weeks. Builds on Cluster B + Cluster C.
7. **Cluster J** (rendering primitives) — 2–3 weeks. Builds on Cluster I.
8. **Cluster E** (Markoff three-mode) — ongoing 6–8 weeks, can parallel with A–D–I–G–J.
9. **Cluster H** (menus / hover / suggesters) — 2–3 weeks. Depends on Cluster C + Cluster J.
10. **Cluster F** (templates / daily notes) — 1.5 weeks. Depends on Cluster A.
11. **Cluster L** (properties panel) — 1 week.
12. **Cluster N** (plugin surfaces) — 2 weeks.
13. **Cluster K** (Bases) — 8–10 weeks. Depends on A + I + J.
14. **Cluster M** (canvas/graph audits) — 1 week.

**Total critical path** to Obsidian-vault-compat Corbomite (without Bases or plugin API): roughly **16–22 weeks** single-developer. With Bases and plugin API: **28–38 weeks**.

## Unresolved follow-ups (controller-side)

Items that were flagged for extraction outside the Pass 2 scope (`_internal.js` / broader bundle):

1. DOMPurify allowlist (`SL`) — security-critical for P4.3.
2. Turndown rule set (`hP`) — web-clipper-compat for P4.2.
3. Bases filter/formula DSL (`DK` parser) — blocks Cluster K (but <https://help.obsidian.md/bases/functions> is a good user-docs starting point).
4. Search-panel DSL — blocks P2.19 (same situation; reverse-engineer from user docs).
5. `AC` allow-list + `PC` defaults table — blocks P1.1.
6. Illegal-filename regex sets `UT`/`WT`/`GT`/`KT` — blocks P1.11 (exact character sets).
7. Internal-plugin manifest IDs (25 remaining) — low priority; doesn't block any gap.
8. `.obsidian/workspaces.json` / `graph.json` / `bookmarks.json` schemas — follow-up for P2.17 and similar.

Recommended controller action for each: grep the full `app.js` / `_internal.js` bundle for the short-symbol assignment (`SL =`, `new TurndownService`, etc.) and file a `docs/obsidian-audit/followups/<name>.md` per item. The items with public docs (Bases functions, Obsidian user help) can proceed without extraction by matching user-observed behaviour.
