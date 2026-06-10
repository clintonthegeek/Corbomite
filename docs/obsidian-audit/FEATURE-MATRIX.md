# FEATURE-MATRIX — Obsidian feature inventory with Corbomite status

> **⚠ FROZEN AT 2026-04-14 — Corbomite-status columns are obsolete.** A 2026-06-10
> code-verification audit found this doc stale in *both* directions: shipped features
> listed as Missing (live preview, Bases UI, search DSL, undo-close-tab, fuzzy
> switcher…) and implementation pointers citing classes deleted by the 2026-05-25
> foundation port (`libs/markoff/Editor`, `ReadingView`, `EditorViewManager`,
> `src/sidebar/SearchPanel`…). The **Obsidian-side feature inventory remains valid**
> as a checklist of what exists to clone. For current Corbomite status, use
> [`../PARITY-MATRIX.md`](../PARITY-MATRIX.md) (living, code-verified).

Every user-visible Obsidian feature, indexed by functional area (not by Obsidian file layout). Each entry cites the Pass 2 domain doc(s) where the feature is specified and records Corbomite's current implementation state.

## How to use this document

Each feature has four fields:

- **User-visible behaviour** — one sentence, phrased as what the user does or sees.
- **Primary Obsidian domain(s)** — which Pass 2 doc(s) define the feature.
- **Corbomite status** — `Have` (works, mostly compat), `Partial` (works but diverges), `Missing` (no code).
- **Corbomite implementation** — file paths when Have/Partial, `—` when Missing.
- **Notes** — compat gotchas, follow-ups, cross-refs to GAP-ANALYSIS.

Features are grouped by functional area (Vault & file management, Navigation, Editor, …). Within each area, related features are nested under a single heading when they share a registry or contract.

For tallies and a project-planning summary, see the closing section "Summary counts".

---

## 1. Vault & file management

### Open vault

- **Behaviour:** User picks a folder via the vault-chooser window; Obsidian opens it and initialises `Vault`, `MetadataCache`, `Workspace`.
- **Domain:** `vault.md §1, §3`; `core.md §1 initializeWithAdapter`.
- **Status:** Have.
- **Implementation:** `src/app/VaultService`, `src/dialogs/OpenVaultDialog`, `libs/storage/FileSystemAdapter`.
- **Notes:** Corbomite opens a vault in-process rather than spawning a fresh window.

### Close vault / switch vault

- **Behaviour:** "Open another vault" closes the current window and respawns the chooser.
- **Domain:** `core.md §1 openVaultChooser` ("process-death on desktop").
- **Status:** Partial (crashes today — see `memory/project_vault_switching.md`).
- **Implementation:** `src/app/MainWindow::closeVault`, `src/app/VaultService::closeVault`.
- **Notes:** **P1 compat issue.** Obsidian's model is destroy-the-window-and-respawn; Corbomite reuses `MainWindow` and per-vault services leak stale refs. See GAP-ANALYSIS §P1 "Vault-switch process-death". Kate session pattern is the recommended fix.

### Create note

- **Behaviour:** Command `file-explorer:new-file` (Mod+N) / `new-file-in-current-tab` / `new-file-in-new-pane` (Mod+Shift+N); respects `newFileLocation` (`root`/`current`/`folder`); `newFileFolderPath` vault-absolute destination; defaults to localised "Untitled" with collision ` 2`, ` 3`, …
- **Domain:** `core.md §6`; `vault.md §1 FileManager.createAndOpenMarkdownFile`.
- **Status:** Partial.
- **Implementation:** `src/app/VaultService`, `libs/storage/VaultScanner`.
- **Notes:** Hotkeys match; `newFileLocation` config key not honoured. See GAP-ANALYSIS §P2 "New-file placement policy".

### Rename note (with link refactor)

- **Behaviour:** User renames a `.md` file; every `[[link]]` / `[md](url)` referencing it is rewritten vault-wide. `alwaysUpdateLinks` config gates the prompt.
- **Domain:** `vault.md §1 FileManager.renameFile`, `§8 "Vault.rename does not rewrite links — that's FileManager"`; `metadata.md §1 updateInternalLinks`.
- **Status:** Partial.
- **Implementation:** rename-only in `libs/storage/FileSystemAdapter`; link refactoring in `libs/storage/SQLiteIndex::renameFile`.
- **Notes:** **P0 correctness.** Corbomite's rename lacks the "snapshot references → mutate → replay updates" pattern of `runAsyncLinkUpdate`. Atomic guarantee missing. See GAP-ANALYSIS §P2 "Rename-with-link-refactor".

### Delete note (trash-policy aware)

- **Behaviour:** `trashOption` config chooses `system` (OS trash) / `local` (`.trash/<name><suffix>.<ext>`) / `none` (permanent). `promptDelete` gates confirm dialog; `deleteUnlinkedAttachments` policy handles orphan attachments.
- **Domain:** `vault.md §1 FileManager.promptForDeletion`, `trashFile`, `§9`.
- **Status:** Partial.
- **Implementation:** `src/app/VaultService::deleteNote`, `libs/storage/FileSystemAdapter::moveToTrash`.
- **Notes:** System trash only (no `.trash/` local fallback). Backlink-count warning missing. Orphan-attachment cleanup missing.

### Move file / folder

- **Behaviour:** Drag-and-drop, File Explorer context menu, or `file-explorer:move-file` command. Rename-with-refactor semantics preserved (see Rename note).
- **Domain:** `core.md §6 file-explorer:move-file`; `vault.md §1 FileManager.renameFile`.
- **Status:** Partial.
- **Implementation:** `src/sidebar/FileTreePanel`, `libs/storage/FileSystemAdapter::rename`.

### Attachments folder policy

- **Behaviour:** `attachmentFolderPath` config governs paste-attachment destination: `"."` = same folder; `"./sub"` = subfolder; else vault-absolute.
- **Domain:** `vault.md §1 getAvailablePathForAttachments`; `VAULT-FORMAT.md §2`.
- **Status:** Missing.
- **Implementation:** —
- **Notes:** Corbomite has no paste-attachment path yet. `stripHeadingForLink` + 250-byte truncation also missing. See GAP-ANALYSIS §P2.

### Download attachments for note

- **Behaviour:** Command `editor:download-attachments` scans the note for remote image URLs, prompts, downloads via `requestUrl`, rewrites links.
- **Domain:** `vault.md §1 downloadAttachmentsForNote`; `workspace.md §6`.
- **Status:** Missing.
- **Implementation:** —

### Bulk undo toast

- **Behaviour:** Any `FileManager.notifyForBulkUndo` caller can offer 30-s "Undo" toast that restores content + mtime of all affected files.
- **Domain:** `vault.md §1 notifyForBulkUndo`.
- **Status:** Missing.

### File-ignore filters

- **Behaviour:** `userIgnoreFilters` config (regex or plain-prefix patterns) excludes paths from the file explorer, metadata cache, and `fileMap`.
- **Domain:** `vault.md §2 AppConfig`; `metadata.md §3`.
- **Status:** Missing.
- **Notes:** Must be wired into `VaultScanner` and `SQLiteIndex`. See GAP-ANALYSIS §P2.

### External-edit live refresh

- **Behaviour:** Changes made in the OS file manager or another editor appear in Obsidian within ~500 ms. `MetadataCache` re-parses; `TextFileView` does three-way merge preserving unsaved edits.
- **Domain:** `vault.md §4 raw/create/modify/delete/rename`; `views.md §1 TextFileView.loadFileInternal` ("three-way merge via FX").
- **Status:** Partial.
- **Notes:** Watch wired via `QFileSystemWatcher`; three-way merge on external-modify missing — Corbomite reloads disk version, clobbering unsaved edits. See GAP-ANALYSIS §P2 "Three-way merge".

### Save-failure content backup

- **Behaviour:** On `vault.modify` throw, `TextFileView.save` calls `fileManager.storeTextFileBackup(file.path, content)` — snapshots into the `file-recovery` internal plugin's data store.
- **Domain:** `views.md §1 TextFileView.save` ("On throw").
- **Status:** Missing.
- **Notes:** Corbomite surfaces the error but loses unsaved content on write-fail. See GAP-ANALYSIS §P2 "Save-failure backup".

### Case-preserving rename on case-insensitive FS

- **Behaviour:** `Foo.md` → `foo.md` works on macOS/Windows without collision.
- **Domain:** `vault.md §3 "Case sensitivity"`, `§9`.
- **Status:** Missing.
- **Notes:** Needs the `.OBSIDIANTEST` probe + `adapter.insensitive` flag.

---

## 2. Navigation

### File Explorer (internal plugin)

- **Behaviour:** Left-sidebar file tree; expand/collapse folders; right-click context menu; drag-to-move.
- **Domain:** Internal plugin (out of Pass 2 scope); registered for "file-explorer" in `core.md §7`.
- **Status:** Have.
- **Implementation:** `src/sidebar/FileTreePanel`, `libs/models/VaultModel`.

### Quick Switcher

- **Behaviour:** `Mod+O` opens a fuzzy-search picker over all note filenames and aliases.
- **Domain:** Internal plugin "switcher"; `ui-bundle.md §1 FuzzySuggestModal`.
- **Status:** Partial.
- **Implementation:** `src/dialogs/QuickSwitcher`.
- **Notes:** Uses `QRegularExpression` / substring — not the Obsidian fuzzy matcher. Ranking diverges. See GAP-ANALYSIS §P2 "Port `prepareQuery`/`fuzzySearch`/`sortSearchResults`".

### Command Palette

- **Behaviour:** `Mod+P` shows every registered command; fuzzy search + keyboard nav.
- **Domain:** Internal plugin "command-palette"; `core.md §7`.
- **Status:** Missing.
- **Notes:** Needs the shared fuzzy matcher. See GAP-ANALYSIS §P2.

### Global search panel

- **Behaviour:** Left sidebar search; supports DSL: `tag:`, `path:`, `file:`, `line:`, `block:`, `section:`, `/regex/`, `"quoted"`, `-exclusion`, `match-case`, `whole-word`, AND/OR/NOT.
- **Domain:** Internal plugin "global-search" (out-of-tree, DSL in `_internal.js`); `search.md §1 QueryController` (the DSL is NOT in `QueryController`).
- **Status:** Partial.
- **Implementation:** `src/sidebar/SearchPanel.cpp:71 // TODO: Support Obsidian search operators`.
- **Notes:** **P2 feature parity.** Corbomite uses plain FTS5 `MATCH` with no operator parsing. DSL parser source not in Pass 2 scope; reverse-engineer from user docs. See GAP-ANALYSIS §P2 "Search DSL".

### Follow link under cursor

- **Behaviour:** `Alt+Enter` follows link; `Mod+Enter` / `Mod+Alt+Enter` / `Mod+Alt+Shift+Enter` open in new tab/split/window.
- **Domain:** `workspace.md §6` commands.
- **Status:** Partial.
- **Implementation:** Wikilink click handlers in `libs/markoff/`.
- **Notes:** `Keymap.isModEvent` modifier semantics not matched verbatim. See GAP-ANALYSIS §P2.

### Back / forward (per-leaf history)

- **Behaviour:** Per-leaf back/forward stacks; toolbar arrows; `Mod+Alt+Left/Right`; `Ctrl+Tab` cycle; file-rename rewrites history entries.
- **Domain:** `workspace.md §1 WorkspaceLeaf.history` (`qD`), `§6`.
- **Status:** Partial.
- **Implementation:** QTextDocument undo only.
- **Notes:** Cap 20 entries per leaf; file-rename rewrite required.

### Recent files tracker

- **Behaviour:** `app.vault` tracks most-recently-opened files; persisted in `workspace.json` `lastOpenFiles`.
- **Domain:** `workspace.md §2, §13 Q1 B0`.
- **Status:** Missing.
- **Notes:** Requires a `RecentFileTracker` object mirroring `B0`'s API.

### Hover-link preview (Page Preview)

- **Behaviour:** Hover any wikilink for 300 ms → popover with target note content. Mod-key pins (Page-Preview plugin setting `defaultMod`). Child popovers chain.
- **Domain:** `ui-bundle.md §1 HoverPopover`; `workspace.md §7 hoverLinkSources`; `editor-markdown.md §9`.
- **Status:** Missing.
- **Notes:** Needs three pieces: hover-link-source registry + `hover-link` event emission + `HoverPopover` widget with mod-pin + child-chain. See GAP-ANALYSIS §P2 "Hover-link preview infrastructure". The 300 ms hover delay (not 500 ms — that's the poll interval) is **Pass 1 correction 3**.

### Random note (internal plugin)

- **Behaviour:** Opens a random note.
- **Domain:** Internal plugin "random-note"; `core.md §7`.
- **Status:** Missing.

---

## 3. Editor

### Source mode (raw source editor)

- **Behaviour:** Markdown source visible with syntax highlighting; `**bold**` rendered as `**bold**` with styled asterisks.
- **Domain:** `editor-markdown.md §1 MarkdownView`, `§8 item 2 `{mode: "source", source: true}``.
- **Status:** Have.
- **Implementation:** `libs/markoff/Editor` + `MarkdownHighlighter`.

### Live-preview mode

- **Behaviour:** Syntax hidden; widgets rendered in-line; cursor entering a block reveals source per-**block** (not per-line).
- **Domain:** `editor-markdown.md §1, §8 item 2 `{mode: "source", source: false}``; `editor.md §1 editorLivePreviewField`.
- **Status:** Missing.
- **Implementation:** — (`docs/superpowers/plans/2026-04-03-editor-three-modes.md` is the planning doc).
- **Notes:** **P2 feature parity** — the editor's biggest gap. Corbomite has 2 modes (Editing, Reading); live-preview is the missing third. Implementation pivots to QGraphicsView (see `memory/project_markoff_graphicsview.md`). See GAP-ANALYSIS §P2 "Live-preview mode".

### Reading view (rendered preview)

- **Behaviour:** Fully-rendered markdown; no source visible; checkboxes clickable; code blocks syntax-highlighted; callouts foldable.
- **Domain:** `editor-markdown.md §1 MarkdownPreviewView`; `rendering.md §1`.
- **Status:** Have.
- **Implementation:** `libs/markoff/ReadingView`, `libs/core/MarkdownRenderEngine`.

### Three-mode toggle

- **Behaviour:** `Ctrl+E` toggles reading ↔ editing view; tab menu "Toggle source mode" toggles raw source ↔ live-preview within editing view.
- **Domain:** `editor-markdown.md §1, §6`.
- **Status:** Partial (two modes, not three).
- **Implementation:** `src/app/MainWindow.cpp:272,280` actions `view_editing_mode` / `view_reading_mode`.
- **Notes:** Mode-switch must `await save()` before leaving `"source"` — **save-on-leave invariant** (editor-markdown.md §8 item 4).

### Three-mode encoding in workspace.json

- **Behaviour:** `ViewState.state.mode ∈ {"source", "preview"}`; `state.source ∈ {true, false}` further splits `"source"` into raw vs live-preview. `{mode: "source", source: false}` = live-preview.
- **Domain:** `editor-markdown.md §2, §8 item 2`.
- **Status:** Missing.
- **Notes:** **P1 compat blocker for workspace.json round-trip.** See GAP-ANALYSIS §P1.

### Markdown syntax highlighting (source view)

- **Behaviour:** Source view colours `**bold**`, `*italic*`, `# heading`, `- list`, `` `code` ``, `[[link]]`, `#tag`, `> [!note]`, frontmatter keys, etc.
- **Domain:** `editor-markdown.md §1` (via CodeMirror extensions).
- **Status:** Have.
- **Implementation:** `libs/markoff/MarkdownHighlighter`.

### Wikilink autocomplete

- **Behaviour:** Typing `[[` opens a suggester with every note + alias; arrow keys navigate; Enter commits; `Mod+click` open target.
- **Domain:** `editor.md §1 EditorSuggest`; `metadata.md §1 getLinkSuggestions`.
- **Status:** Partial.
- **Implementation:** `libs/markoff/CompletionPopup`.
- **Notes:** Trigger detection hardcoded (no `MarkoffSuggestRegistry`). Aliases not included. See GAP-ANALYSIS §P2.

### Tag autocomplete

- **Behaviour:** Typing `#` opens a tag suggester; results from inline + frontmatter tags.
- **Domain:** `metadata.md §1 getTags`; `editor.md §1 EditorSuggest`.
- **Status:** Partial.
- **Implementation:** `libs/markoff/CompletionPopup`.
- **Notes:** Frontmatter-tag merge missing (metadata.md §11).

### Heading autocomplete after `#`

- **Behaviour:** Typing `[[Note#` opens a heading suggester for the target note.
- **Domain:** Internal `EditorSuggest` in `views/ViewRegistry.js:238`.
- **Status:** Missing.

### IME / CJK input

- **Behaviour:** Full-width brackets autocorrect: `【【` → `[[`, `】】` → `]]`, `！【【` → `![[` (editor.md §1).
- **Domain:** `editor.md §1 Editor.expandText`.
- **Status:** Missing.
- **Notes:** Three regexes; trivial port via `inputMethodEvent`.

### Multi-cursor / rectangular selection

- **Behaviour:** Mac-Option-drag for rectangular selection; `Ctrl+D` select-next-occurrence; plugins iterate `listSelections()`.
- **Domain:** `editor.md §12` (confirmed missing in Markoff).
- **Status:** Missing.
- **Notes:** QTextDocument single-cursor foundation. Document as explicit plugin-shim incompatibility.

### Triple-click line-extend selection

- **Behaviour:** 3 rapid clicks anchor a line range; drag extends one line at a time.
- **Domain:** `editor.md §1 K$`.
- **Status:** Unverified.

### Auto-continue list markers on Enter

- **Behaviour:** Enter inside `- foo` creates `- `; numbered lists increment; checkboxes preserved; blank trailing line strips the marker.
- **Domain:** `editor.md §1 Editor.newlineAndIndentContinueMarkdownList`.
- **Status:** Unverified.
- **Implementation:** `libs/markoff/Editor::keyPressEvent` (likely; needs confirmation).

### Tab/Shift-Tab indent/outdent list item

- **Behaviour:** Tab indents the whole list item; Shift+Tab outdents; bidi-aware.
- **Domain:** `editor.md §1`.
- **Status:** Partial (internal handling; no command surface).

### Fold headings / list items in source

- **Behaviour:** Gutter triangle collapses a heading or list item. Fold state persisted via `app.foldManager` (keyed on line numbers — loses on line-count change).
- **Domain:** `editor-markdown.md §1, §3, §8 item 11`.
- **Status:** Missing.

### Fold headings in reading view

- **Behaviour:** Click heading triangle; all sections with `level > current.level` hide. Scroll-to-subpath auto-expands ancestors.
- **Domain:** `editor-markdown.md §1 MarkdownPreviewSection.headingCollapsed`, `§8 "showSection walks backwards un-collapsing"`.
- **Status:** Missing.

### In-document find/replace

- **Behaviour:** Ctrl+F find; Ctrl+Shift+F replace (editing mode only); match highlights overlay.
- **Domain:** `editor-markdown.md §1 currentMode.showSearch`, `§9`.
- **Status:** Partial.
- **Implementation:** `QTextDocument::find`.
- **Notes:** No regex/case/whole-word UI; no replace.

### Checkbox round-trip in preview

- **Behaviour:** Click a rendered checkbox; source updates `- [ ]` ↔ `- [x]`; custom markers `[/]`, `[?]`, etc. preserved as `data-task="?"`.
- **Domain:** `editor-markdown.md §1 MarkdownRenderer.onCheckboxClick`, §8 item 12.
- **Status:** Partial.
- **Implementation:** `libs/markoff/CheckboxTextObject`.
- **Notes:** Exact regex in VAULT-FORMAT.md §4.6. Disk write on every click.

### Per-keystroke auto-save (debounced 2 s)

- **Behaviour:** Typing triggers a debounced save 2 s after last keystroke.
- **Domain:** `views.md §1 TextFileView.requestSave`, `§8`.
- **Status:** Partial.
- **Implementation:** `NoteEditorWidget` auto-save.
- **Notes:** Debounce timing may differ.

### Mode-switch saves before leaving source

- **Behaviour:** `setMode` always `await save()` before leaving `"source"` (disk write on dirty).
- **Domain:** `editor-markdown.md §8 item 4`.
- **Status:** Partial.

### Quick-preview cross-pane sync

- **Behaviour:** Two panes on same file (source + preview) — preview re-renders per keystroke without waiting for save.
- **Domain:** `workspace.md §4 quick-preview`; `editor-markdown.md §8 item 13`.
- **Status:** Missing.

### Ctrl+Wheel base-font zoom

- **Behaviour:** Ctrl+Wheel scales base font 10–30 px, gated on `baseFontSizeAction` config; 500 ms debounce write to `vault.setConfig("baseFontSize", n)`.
- **Domain:** `editor-markdown.md §3 (4), §9`.
- **Status:** Missing.

### Properties editor (frontmatter UI)

- **Behaviour:** Editable YAML frontmatter rows at top of document; widget per inferred property type; toggled by `propertiesInDocument` config (`visible` / `hidden` / `source`).
- **Domain:** `editor-markdown.md §1, §9`; `metadata.md §1 getAllPropertyInfos`.
- **Status:** Missing.
- **Notes:** **P2.** Requires YAML parser + `MetadataTypeManager` equivalent. See GAP-ANALYSIS §P2 "Properties panel".

### Inline-title rename

- **Behaviour:** Click the title row → contenteditable filename; Enter saves, Esc reverts, Tab saves-then-focus-next, ArrowDown blurs into editor.
- **Domain:** `views.md §1 EditableFileView`.
- **Status:** Missing.

### Print / Export to PDF

- **Behaviour:** `workspace:export-pdf` opens a dialog (page size, landscape, margin, downscale); renders via `ReadingView` → PDF. Strips `href` from `.internal-link` before printing.
- **Domain:** `editor-markdown.md §3 pdfExportSettings, §9`; `workspace.md §6`.
- **Status:** Missing.

### Ctrl+C copies full source in reading view

- **Behaviour:** `Mod+C` with empty selection copies the entire markdown source. `Mod+A` shows a Notice redirecting to `Mod+C`.
- **Domain:** `editor-markdown.md §9`.
- **Status:** Missing (low priority).

---

## 4. Rendering & preview

### Markdown render pipeline (static API)

- **Behaviour:** `MarkdownRenderer.render(app, text, el, sourcePath, component)` converts markdown to DOM with full post-processor chain.
- **Domain:** `editor-markdown.md §1 MarkdownRenderer`.
- **Status:** Partial.
- **Implementation:** `libs/core/MarkdownRenderEngine`.
- **Notes:** No post-processor chain; no embed resolution; no link-callback. Component-less call warning missing.

### Progressive section rendering with recycling

- **Behaviour:** Notes > 10240 bytes parse in a worker; DOM built per-section; section recycle key = exact HTML string equality; 5 ms / 10-section time budget; scrollbar preserved via `pusherEl + sizerEl.minHeight`.
- **Domain:** `editor-markdown.md §1 MarkdownPreviewRenderer, §8, §12 "Progressive section render + recycle"`.
- **Status:** Missing.
- **Notes:** **P2 major architectural gap.** Corbomite renders whole-document. Performance cliff on large notes. See GAP-ANALYSIS §P2 "Progressive section pipeline".

### MathJax inline + display math

- **Behaviour:** `$...$` inline, `$$...$$` display; typeset via MathJax; 100 ms debounce batched via `finishRenderMath`.
- **Domain:** `rendering.md §1 renderMath, finishRenderMath, loadMathJax`.
- **Status:** Partial.
- **Implementation:** `libs/markoff/MathRenderer` (JKQTMathText).
- **Notes:** Per-call synchronous rendering; no batching. Plugin-compat shim must expose `finishRenderMath` as no-op resolved future.

### Mermaid diagrams

- **Behaviour:** ```` ```mermaid ```` code blocks rendered as SVG; Mermaid lazy-loaded on first use.
- **Domain:** `rendering.md §1 loadMermaid`.
- **Status:** Partial.
- **Implementation:** `libs/mmdr/` (Rust FFI bridge).
- **Notes:** Theme integration, error-pane for invalid diagrams, version parity — all unconfirmed.

### PDF embeds and PDF view

- **Behaviour:** `.pdf` files open in PDFView (internal); `![[file.pdf]]` embeds render PDF.js inline.
- **Domain:** `rendering.md §1 loadPdfJs`; `views.md §7 "pdf" built-in`.
- **Status:** Missing.
- **Notes:** **P2.** No PDF capability today. Recommendation: Poppler-Qt6 (lighter) or Okular KPart (richer). See GAP-ANALYSIS §P2.

### Prism syntax highlighting in code blocks

- **Behaviour:** Fenced code with language tag syntax-highlighted (Prism.js, ~300 languages).
- **Domain:** `rendering.md §1 loadPrism`.
- **Status:** Partial.
- **Implementation:** KDE `KSyntaxHighlighting` (coverage acceptable).
- **Notes:** Language-name mapping differs (`py` vs `python`); shim needs translation.

### Callouts

- **Behaviour:** `> [!note]` / `[!warning]` / etc. render with icon + title + foldable content chrome. Optional `+`/`-` suffix enables fold state.
- **Domain:** `rendering.md §11 "Callout chrome"`; `editor-markdown.md §12`.
- **Status:** Partial.
- **Implementation:** `libs/markoff-parser/` emits AST.
- **Notes:** Reading-view DOM-class parity unconfirmed.

### File embeds `![[Note]]`

- **Behaviour:** Inline embed renders the target note inside the current one. Per-embed `MarkdownPreviewView`. Subpath resolution via `resolveSubpath`. Depth guard caps recursion via `JZ` walk.
- **Domain:** `editor-markdown.md §1 MarkdownPreviewView, §12 "![[Note]] inline embed"`.
- **Status:** Missing.
- **Notes:** **P2.** Needs `EmbedRegistry` (`aJ` / `sJ`) — see `02-extension-surfaces.md`.

### Image / audio / video embeds

- **Behaviour:** `![[image.png]]` / `![[audio.mp3]]` / `![[video.mp4]]` render inline via EmbedRegistry's built-in handlers.
- **Domain:** `views.md §7` built-ins; `core.md §2 embedRegistry`.
- **Status:** Missing.
- **Notes:** Extension-dispatch via `aJ`. Image cache-bust via `app.vault.getResourcePath(file)` → `app://local/<path>?<mtime>`.

### HTML sanitisation (plugin-supplied HTML)

- **Behaviour:** `sanitizeHTMLToDom(html)` runs DOMPurify over untrusted HTML, `importNode`'d into current document.
- **Domain:** `rendering.md §1 sanitizeHTMLToDom`.
- **Status:** Missing.
- **Notes:** **P1 security-boundary.** DOMPurify `SL` allowlist is a follow-up (GAP-ANALYSIS §P4 follow-up).

### HTML → Markdown (paste from browser)

- **Behaviour:** Paste HTML (from a browser); converted to Markdown via Turndown before insertion.
- **Domain:** `rendering.md §1 htmlToMarkdown`.
- **Status:** Missing.
- **Notes:** Turndown rule set (`hP`) is a follow-up.

### Hover tooltips on icons/buttons

- **Behaviour:** `displayTooltip(el, text, {placement, delay})`; single-slot state; sweep across multiple icons skips delay.
- **Domain:** `rendering.md §1 displayTooltip`; `ui-bundle.md §1`.
- **Status:** Have.
- **Implementation:** `QToolTip`.
- **Notes:** Sweep-skip-delay behaviour diverges; minor UX.

### Search result highlight spans

- **Behaviour:** `renderResults(el, text, {matches}, offset?)` wraps matched substrings in `.suggestion-highlight` spans. Used by search panel, quick switcher, command palette, every suggester.
- **Domain:** `rendering.md §1 renderResults`; `search.md §12`.
- **Status:** Missing.
- **Notes:** Shared chokepoint — Corbomite needs `Markoff::renderHighlightedRuns(...)` as the one primitive every search-ish widget renders through. See GAP-ANALYSIS §P2.

### Internal-link / external-link / tag inline renderer (RenderContext)

- **Behaviour:** `RenderContext.renderFileLink` / `renderExternalLink` / `renderTag` produces DOM with click + context-menu + drag + hover-link wiring. Used by Bases cells, search rows, every suggester.
- **Domain:** `rendering.md §1 RenderContext`.
- **Status:** Missing.
- **Notes:** **P2.** Corbomite needs `Markoff::LinkRenderer` extracted early to avoid every consumer re-implementing. `renderFileLink` hard-codes `source: "bases"` on hover-link emissions; Page-Preview must have `"bases"` registered.

### Bidi/RTL isolate per-inline

- **Behaviour:** CodeMirror `mW.rtl/.ltr/.auto` decorations wrap RTL/LTR runs in `<span class="cm-iso" dir="...">` with CM6 `bidiIsolate` Direction.
- **Domain:** `rendering.md §1 mW`; `editor.md §12`.
- **Status:** Missing.
- **Notes:** Qt provides per-paragraph direction only. Per-inline isolates are a Markoff-migration follow-up.

### `%%comment%%` body syntax

- **Behaviour:** `%%...%%` text is not rendered.
- **Domain:** `parsing.md §1 stripHeadingForLink` PT regex.
- **Status:** Unverified.

### Ctrl+Wheel font zoom (preview)

- **Behaviour:** Ctrl+Wheel scales preview font; clamp 10–30 px; gated on config.
- **Domain:** `editor-markdown.md §9`.
- **Status:** Missing.

### Touch double-tap toggles mode (mobile)

- **Behaviour:** 300 ms / 30 px threshold double-tap in preview toggles mode.
- **Domain:** `editor-markdown.md §12`.
- **Status:** N/A (Corbomite is desktop-only).

### Pinch-zoom image viewer (mobile)

- **Behaviour:** Pinch to zoom embedded images in preview.
- **Domain:** `editor-markdown.md §9`.
- **Status:** N/A.

---

## 5. Metadata & link graph

### Per-file cache `CachedMetadata`

- **Behaviour:** Every `.md` file has a `CachedMetadata` record: `links[]`, `embeds[]`, `tags[]`, `headings[]`, `sections[]`, `listItems[]`, `footnoteRefs[]`, `footnotes[]`, `blocks{}`, `frontmatter{}`, `frontmatterLinks[]`, `frontmatterPosition`.
- **Domain:** `metadata.md §2`.
- **Status:** Missing.
- **Implementation:** `libs/storage/SQLiteIndex` has tables for links/tags only.
- **Notes:** **P2.** Required for backlinks pane / graph view / properties panel / embed rendering / hover preview. See GAP-ANALYSIS §P2 "NoteMetadata struct".

### Resolved / unresolved link graph

- **Behaviour:** `resolvedLinks[src][dst] = count` outgoing link map; `unresolvedLinks[src][normalisedLinkpath] = count` unresolved map. Readable from plugins.
- **Domain:** `metadata.md §2 ResolvedLinkMap, UnresolvedLinkMap, §8`.
- **Status:** Partial.
- **Implementation:** `libs/storage/SQLiteIndex::allLinks, backlinksFor, outlinksFor, orphanLinks`.
- **Notes:** Query-only; no in-memory maps. Cache-by-path + link-resolution shape must match for plugin compat.

### Backlinks pane

- **Behaviour:** Bottom panel in MarkdownView (toggled per-leaf); shows every note linking to the current one.
- **Domain:** `metadata.md §1 getBacklinksForFile`; internal plugin "backlink".
- **Status:** Partial.
- **Implementation:** `libs/storage/SQLiteIndex::backlinksFor`.
- **Notes:** No position offsets in returned `LinkInfo`.

### Outgoing links pane (internal plugin)

- **Behaviour:** Right-sidebar pane listing every link out of the current note.
- **Domain:** Internal plugin "outgoing-link"; `core.md §7`.
- **Status:** Missing.

### Tags pane

- **Behaviour:** Right-sidebar tag tree; subtag counts; click to `openGlobalSearch("tag:" + name)`.
- **Domain:** Internal plugin "tag-pane"; `metadata.md §1 getTags`.
- **Status:** Partial.
- **Implementation:** `libs/storage/SQLiteIndex::allTags`.
- **Notes:** Inline-only tag extraction; no frontmatter `tags:` merge; no subtag-prefix counting.

### Headings / subpath anchor resolution

- **Behaviour:** `[[Note#Heading]]` and `[[Note#^block-id]]` scroll to target section/block; `resolveSubpath` dispatches on `^` / `[^` / heading prefix.
- **Domain:** `metadata.md §9`; `parsing.md §1 resolveSubpath`; `VAULT-FORMAT.md §4.4`.
- **Status:** Missing.
- **Notes:** **P0 + P1.** `SQLiteIndex` wikilink regex doesn't strip `#subpath` (P0); `Markoff::Document::extractSubpath` implementation unverified (P1).

### Wikilink aliases (`[[Target|display]]`)

- **Behaviour:** `|`-delimited alias overrides display text; `displayText` stored in `CachedMetadata.links[].displayText`.
- **Domain:** `metadata.md §2 LinkCache`; `parsing.md §1 parseLinktext`.
- **Status:** Partial.
- **Implementation:** `libs/storage/SQLiteIndex`.

### Frontmatter aliases

- **Behaviour:** `aliases:` frontmatter surfaces the note under additional names in the Quick Switcher.
- **Domain:** `parsing.md §1 parseFrontMatterAliases`; `metadata.md §1 getLinkSuggestions`.
- **Status:** Missing.

### Frontmatter tags

- **Behaviour:** `tags:` frontmatter merged with inline `#tag` occurrences; tag pane shows both.
- **Domain:** `parsing.md §1 parseFrontMatterTags`; `metadata.md §1 getTags`.
- **Status:** Missing.
- **Notes:** Requires YAML parser. See GAP-ANALYSIS §P1 "YAML library".

### Block IDs `^id`

- **Behaviour:** `paragraph text ^abc` attaches a block id; `[[Note#^abc]]` resolves to that block via `cache.blocks`.
- **Domain:** `metadata.md §2 CachedMetadata.blocks`.
- **Status:** Missing.

### Footnotes

- **Behaviour:** `[^id]` references + `[^id]: definition` blocks; click-follow navigates; mouseover fires hover-link with `linktext = "#[^" + id + "]"`.
- **Domain:** `metadata.md §2 footnoteRefs/footnotes`; `editor-markdown.md §12`.
- **Status:** Missing.

### Link rename refactor (rename-with-refactor)

- **Behaviour:** Renaming a file rewrites every `[[Foo]]` / `[md](Foo.md)` across the vault. `alwaysUpdateLinks` config gates the prompt.
- **Domain:** `vault.md §1 FileManager.renameFile, runAsyncLinkUpdate`; `metadata.md §1 updateInternalLinks`.
- **Status:** Partial.
- **Implementation:** `libs/storage/SQLiteIndex::updateLinksForRename`.
- **Notes:** Atomic snapshot-mutate-replay pattern missing. See GAP-ANALYSIS §P2.

### Shortest-path-wins link resolution

- **Behaviour:** `[[Foo]]` resolves to the shortest-path `Foo.md` in the vault, with same-folder preference for ambiguity.
- **Domain:** `metadata.md §8`; `VAULT-FORMAT.md §7`.
- **Status:** Missing.
- **Notes:** **P0 correctness bug.** `SQLiteIndex::resolveWikilink` does flat name lookup. See GAP-ANALYSIS §P0.

### Index-complete / changed / resolved events

- **Behaviour:** `MetadataCache` emits `changed`, `deleted`, `resolve` (per-file), `resolved` (batch complete), `finished` (initial scan done).
- **Domain:** `metadata.md §4`.
- **Status:** Partial.
- **Implementation:** `SQLiteIndex::indexReady` (one signal).
- **Notes:** **P2 plugin-compat.** Five distinct signals required. See GAP-ANALYSIS §P2.

### User-ignore-filters applied

- **Behaviour:** `userIgnoreFilters` config excludes paths from metadata index + file explorer.
- **Domain:** `metadata.md §3`.
- **Status:** Missing.

### Zero-latency startup (IndexedDB preload)

- **Behaviour:** Cache hydrates from IndexedDB on boot; resolved links appear immediately.
- **Domain:** `metadata.md §3, §9`.
- **Status:** Partial.
- **Implementation:** `libs/storage/SQLiteIndex` (SQLite, always-persisted).

---

## 6. Bases (database view feature)

### Open a `.base` file

- **Behaviour:** Double-click `.base` → opens `BasesView` (a `TextFileView` subclass).
- **Domain:** `bases.md §1, §3`.
- **Status:** Missing.
- **Notes:** **P2 biggest single gap.** 8–10 weeks MVP estimated. See GAP-ANALYSIS §P2 "Bases".

### Table of notes matching filter

- **Behaviour:** Each matching note = one row; each visible property = one column. Built-in view type `"table"`.
- **Domain:** `bases.md §1, §9`.
- **Status:** Missing.

### Add columns for frontmatter / file / formula

- **Behaviour:** Column by `note.<key>`, `file.<field>`, or `formula.<name>`.
- **Domain:** `bases.md §1 PropertyId`, `VAULT-FORMAT.md §6`.
- **Status:** Missing.

### Inline edit a cell

- **Behaviour:** Click a cell → widget matching the inferred type; writes back via `FileManager.processFrontMatter`.
- **Domain:** `bases.md §1, §9`.
- **Status:** Missing.
- **Notes:** Requires `MetadataTypeManager` widget registry.

### Sort columns

- **Behaviour:** Click header cycles ASC → DESC → unsorted; multi-key via toolbar.
- **Domain:** `bases.md §1, §9`.
- **Status:** Missing.

### Group rows

- **Behaviour:** `groupBy` property with optional summary cell (sum/count/mean).
- **Domain:** `bases.md §1 BasesEntryGroup, §9`.
- **Status:** Missing.

### Filter (global + per-view, AND-merged)

- **Behaviour:** `filters:` + per-view `filters:` combine. Atomic rules are formula expressions parsed by `DK`.
- **Domain:** `bases.md §1, §3, §13 Q1`.
- **Status:** Missing.
- **Notes:** **Filter DSL parser out-of-scope** (bases.md §13 Q1). Follow-up to extract.

### Multiple named views

- **Behaviour:** One `.base` contains multiple `views:` with different filter/columns/sort/type; toolbar switches.
- **Domain:** `bases.md §1, §9`.
- **Status:** Missing.

### Formulas

- **Behaviour:** Named formulas reference `note.*`, `file.*`, `formula.*`, `this.*`. Aggregate funcs over `ListValue`.
- **Domain:** `bases.md §1`.
- **Status:** Missing.

### `+ New` button

- **Behaviour:** Creates note with filter-satisfying frontmatter pre-populated, optionally from `newItemTemplate`.
- **Domain:** `bases.md §9`.
- **Status:** Missing.

### Export to CSV / TSV / Markdown / HTML

- **Behaviour:** Toolbar action copies results in multiple clipboard formats.
- **Domain:** `bases.md §9`.
- **Status:** Missing.

### Rename a view auto-rewrites `[[basefile#viewname]]`

- **Behaviour:** View rename updates every wikilink referencing the view via `MetadataCache.updateInternalLinks`.
- **Domain:** `bases.md §9`.
- **Status:** Missing.

### `![[my.base]]` embed in markdown

- **Behaviour:** Embed renders the first view inline (likely read-only).
- **Domain:** `bases.md §9, §13 Q6`.
- **Status:** Missing.

### Per-`BasesView` undo/redo

- **Behaviour:** Separate undo stack for inline-cell edits.
- **Domain:** `bases.md §9`.
- **Status:** Missing.

---

## 7. Canvas (infinite canvas)

### Open a `.canvas` file

- **Behaviour:** Opens `CanvasView` (internal plugin); infinite pan/zoom canvas with draggable cards.
- **Domain:** Internal plugin "canvas" (out of Pass 2 scope); `views.md §7 built-in "canvas"`.
- **Status:** Have.
- **Implementation:** `libs/canvas/`, `src/canvas/`.

### Note card, text card, file card, group

- **Behaviour:** Drag notes/text/files/group-rectangles onto canvas; arrow-tool draws edges.
- **Domain:** JsonCanvas spec (<https://jsoncanvas.org/>).
- **Status:** Have.

### `.canvas` file schema compat

- **Behaviour:** JsonCanvas 1.0 schema (nodes + edges).
- **Domain:** Out-of-scope for this audit.
- **Status:** Unverified.
- **Notes:** Validate `libs/canvas/` against <https://jsoncanvas.org/>. See GAP-ANALYSIS §P2.

---

## 8. Workspace & layout

### Split pane (horizontal / vertical)

- **Behaviour:** `workspace:split-vertical` (right) / `split-horizontal` (down); drag tab-header to pane edge for visual split.
- **Domain:** `workspace.md §1 splitActiveLeaf, §6`.
- **Status:** Have.
- **Implementation:** `src/editor/EditorViewManager`.

### Tabs (per-pane tab group)

- **Behaviour:** Each pane is a `WorkspaceTabs`; drag-to-reorder; Ctrl+Tab cycle; Mod+1..8 / Mod+9 jump.
- **Domain:** `workspace.md §1 WorkspaceTabs, §6`.
- **Status:** Have.
- **Implementation:** `EditorViewSpace`, `EditorViewManager`.

### Stacked tabs mode

- **Behaviour:** `workspace:toggle-stacked-tabs` — all tabs visible side-by-side with scrolling.
- **Domain:** `workspace.md §1 setStacked, §6`.
- **Status:** Missing.

### Pin tab

- **Behaviour:** `workspace:toggle-pin` — pinned tabs never recycle for `openLinkText`; `setPinned` propagates to linked-pane group members.
- **Domain:** `workspace.md §1, §6, §8`.
- **Status:** Missing.

### Linked-pane group (same file, different modes)

- **Behaviour:** Group tabs so navigating one navigates all; `setGroupMember` / `setGroup`.
- **Domain:** `workspace.md §1, §8`.
- **Status:** Missing.

### Side docks (left/right)

- **Behaviour:** Collapsible sidedocks, `setSize(px)` + `collapsed`; animated 140 ms; snap-collapse below 50 px; max 80% workspace width.
- **Domain:** `workspace.md §1 WorkspaceSidedock, §9`.
- **Status:** Partial.
- **Implementation:** `src/sidebar/` panels.

### Popout windows

- **Behaviour:** `workspace:open-in-new-window` / `move-to-new-window` — a new OS window hosting a detached subtree; cross-window drag-drop.
- **Domain:** `workspace.md §1 WorkspaceWindow, §6`.
- **Status:** Missing.

### Floating tab group

- **Behaviour:** `WorkspaceFloating` container; only popout windows inside.
- **Domain:** `workspace.md §1 WorkspaceFloating`.
- **Status:** Missing.

### Activity ribbon (left edge icons)

- **Behaviour:** Per-side icon strip; drag-reorder; right-click to hide; plugin-registered via `Plugin.addRibbonIcon`.
- **Domain:** `workspace.md §1 WorkspaceRibbon, §7 Ribbon.items`.
- **Status:** Partial.
- **Implementation:** toolbar in MainWindow.
- **Notes:** No plugin slot, reorder, hide-per-item.

### Layout autosave / restore

- **Behaviour:** Layout written to `.obsidian/workspace.json` (debounced 1 s); restored on vault open.
- **Domain:** `workspace.md §1 getLayout/saveLayout, §3`; `VAULT-FORMAT.md §3`.
- **Status:** Partial.
- **Implementation:** `src/app/SessionManager` (wrong directory + wrong schema).
- **Notes:** **P1 compat blocker.**

### Named workspaces (save / load)

- **Behaviour:** Save current layout under a name; restore later. Persisted in `.obsidian/workspaces.json`.
- **Domain:** Internal plugin "workspaces"; `workspace.md §3 "workspaces.json"`.
- **Status:** Missing.

### `workspace:undo-close-pane` (cap 10)

- **Behaviour:** `Mod+Shift+T` restores the most-recently-closed tab (up to 10); restores view-state + per-leaf history + original parent.
- **Domain:** `workspace.md §1 undoHistory, §6, §7`.
- **Status:** Missing.

### Command: `app:open-vault-chooser` / `switch-vault` / `open-another-vault`

- **Behaviour:** Vault switcher modal; spawns new window for picked vault.
- **Domain:** `core.md §6`.
- **Status:** Partial.

### Window commands (zoom, always-on-top)

- **Behaviour:** `window:toggle-always-on-top`, `window:zoom-in/out/reset-zoom`.
- **Domain:** `core.md §6`.
- **Status:** Have (Qt native).

### Mobile swipe gestures

- **Behaviour:** Pull-down → "mobile pull action"; two-finger trackpad → back/forward; swipe-to-open sidedock on mobile drawer.
- **Domain:** `workspace.md §4 swipe`.
- **Status:** N/A.

---

## 9. UI primitives

### Command palette pattern (SuggestModal / FuzzySuggestModal)

- **Behaviour:** Text input + scrolling suggestion list; arrow-nav + Ctrl+P/N; Enter commits; Esc cancels. Fuzzy match + highlight spans.
- **Domain:** `ui-bundle.md §1 SuggestModal, FuzzySuggestModal, §9`.
- **Status:** Partial.
- **Implementation:** `src/dialogs/QuickSwitcher` shadows `FuzzySuggestModal`.

### Input-anchored suggester (`AbstractInputSuggest`)

- **Behaviour:** Popup anchored to a form input; used by "new-file location", folder pickers, template-name input.
- **Domain:** `ui-bundle.md §1 AbstractInputSuggest`.
- **Status:** Missing.

### Modal dialog (`Modal`)

- **Behaviour:** Dim-bg overlay; Esc-closes; click-outside closes; title+content+Esc scope; restores focus + `QTextCursor` range on close.
- **Domain:** `ui-bundle.md §1 Modal`.
- **Status:** Partial.
- **Implementation:** `QDialog` subclasses.

### Confirm/prompt dialog

- **Behaviour:** `KMessageBox::questionYesNo` equivalent; optional `addCheckbox` ("Don't show again"); promise-based.
- **Domain:** `ui-bundle.md §1 nb/ib`.
- **Status:** Have.

### Notice (toast)

- **Behaviour:** `new Notice(text, durationMs)`; top-right stacked; hover pauses auto-hide; 1 s leave-grace; chainable `.addButton(text, cb)`.
- **Domain:** `ui-bundle.md §1 Notice`.
- **Status:** Partial.
- **Implementation:** Corbomite notice widget.

### Hover popover (`HoverPopover`)

- **Behaviour:** 300 ms hover delay; 500 ms global poll; anchor to mouse if target > 300 px; `setIsFocused(true)` pin-to-keep-open; `childHovers` chain; `watchResize` cap 10.
- **Domain:** `ui-bundle.md §1 HoverPopover, §12`.
- **Status:** Missing.
- **Notes:** See GAP-ANALYSIS §P2 "Hover-link preview infrastructure". The 300 ms hover delay is **Pass 1 correction 3**.

### Context menu (`Menu`)

- **Behaviour:** `Menu` with sections; arrow-nav; submenu on hover/arrow; `setSection(id)` buckets items; built-in section order.
- **Domain:** `ui-bundle.md §1 Menu, MenuItem`; `workspace.md §10 "Menu addSections ordering"`.
- **Status:** Partial.
- **Implementation:** `QMenu` direct.
- **Notes:** **P2.** Section ordering protocol needs `Corbomite::MenuSectionRouter`. See GAP-ANALYSIS §P2.

### Menu mid-construction plugin hook

- **Behaviour:** `workspace.trigger("file-menu" / "url-menu" / "editor-menu" / "files-menu" / "leaf-menu" / "tab-group-menu" / "markdown-viewport-menu", menu, ...ctx)` fires after built-ins add their items; plugins inject via `menu.addItem`.
- **Domain:** `workspace.md §4, §10`; `ui-bundle.md §10`.
- **Status:** Missing.
- **Notes:** **P3 plugin-ready.** See GAP-ANALYSIS §P3 "Menu section registry".

### Ribbon button

- **Behaviour:** `Plugin.addRibbonIcon(icon, title, cb)` adds to left activity bar.
- **Domain:** `workspace.md §7 WorkspaceRibbon`; `plugin.md §10`.
- **Status:** Partial.

### Status bar item

- **Behaviour:** `Plugin.addStatusBarItem()` returns a mutable `HTMLElement` in the bottom status bar; auto-classed `plugin-<sanitised-id>`.
- **Domain:** `plugin.md §10`.
- **Status:** Missing.
- **Notes:** **Plugin-id sanitiser bug** (regex without `g` flag — only first illegal char rewritten). Must preserve for compat (plugin.md §2).

### Setting builder (`Setting`)

- **Behaviour:** Fluent builder `new Setting(el).setName(...).setDesc(...).addToggle(t => t.setValue(...).onChange(...))`.
- **Domain:** `settings.md §1 Setting`; `ui-bundle.md §1`.
- **Status:** Partial.
- **Implementation:** `QFormLayout`-based.
- **Notes:** Not a fluent builder; plugin-compat requires the builder pattern.

### Icon registry

- **Behaviour:** `setIcon(el, name)` renders a Lucide SVG; `addIcon(name, svg)` registers custom; `getIconIds()` lists all.
- **Domain:** `ui-bundle.md §1 icons, §11 "Icon translation table"`.
- **Status:** Partial.
- **Implementation:** `QIcon::fromTheme`.
- **Notes:** ~70% Freedesktop coverage; 30% bundled SVG fallback. See GAP-ANALYSIS §P2 "Lucide icon shim".

### MomentFormatComponent live preview

- **Behaviour:** Input field for Moment.js format strings; sample element shows live current-date preview.
- **Domain:** `ui-bundle.md §1 MomentFormatComponent, §2`.
- **Status:** Missing.
- **Notes:** Requires Moment-format-string parity. `QDateTime::toString` tokens differ — needs adapter. See GAP-ANALYSIS §P1 "Moment format compat".

---

## 10. Settings

### Settings modal (`app.setting`)

- **Behaviour:** KPageDialog-shaped; sidebar tab picker; core tabs + plugin tabs.
- **Domain:** `settings.md §1 SettingTab, PluginSettingTab`.
- **Status:** Partial.
- **Implementation:** `src/dialogs/SettingsDialog` (KPageDialog).

### Core settings tabs

- **Behaviour:** Editor, Files & Links, Appearance, Hotkeys, Core Plugins, Community Plugins, Keychain, About, etc.
- **Domain:** `settings.md §3 per-tab table`.
- **Status:** Partial.
- **Implementation:** `SettingsDialog::setupEditorPage`, `setupFilesPage`, `setupAppearancePage`.
- **Notes:** Missing: Hotkeys, Core Plugins, Community Plugins, About.

### Plugin settings tab registration

- **Behaviour:** `Plugin.addSettingTab(tab)`; `display()` override builds rows.
- **Domain:** `settings.md §1 PluginSettingTab, §10`.
- **Status:** Missing.

### Hotkeys editor

- **Behaviour:** Hotkeys tab lets user assign/clear custom hotkeys; persists user-only overrides in `.obsidian/hotkeys.json`.
- **Domain:** `settings.md §3 hotkeys.json, §9`.
- **Status:** Missing.
- **Notes:** Corbomite uses `KConfigGroup("Shortcuts")` (different format).

### Core plugins toggle

- **Behaviour:** Core Plugins tab toggles each of the 31 internal plugins on/off; persisted in `.obsidian/core-plugins.json`.
- **Domain:** `settings.md §3 core-plugins.json`; `core.md §7`.
- **Status:** Missing.

### Community plugins install / enable / disable

- **Behaviour:** Community Plugins tab; browse, install from the registry, enable/disable.
- **Domain:** `settings.md §3 community-plugins.json, §9`.
- **Status:** N/A (Corbomite won't host community plugin store).

### Appearance settings (theme, font, accent)

- **Behaviour:** `theme`/`cssTheme`/`accentColor`/`baseFontSize`/`textFontFamily` etc.; changes apply immediately via `css-change` event.
- **Domain:** `settings.md §3`; `core.md §1 update{Theme,AccentColor,FontFamily,FontSize,…}`; `VAULT-FORMAT.md §3 AppearanceConfig`.
- **Status:** Partial.
- **Implementation:** Qt/KDE theme via system.

### Live reload on external config edit

- **Behaviour:** External `.obsidian/*.json` edit (git pull, direct edit) → `config-changed` per diff; UI refreshes live.
- **Domain:** `vault.md §4 config-changed`.
- **Status:** Missing.

---

## 11. Internal plugins (one row each, from core.md §7)

The 31 built-in "internal plugins" are where most user-visible features live. IDs confirmed in-tree: `file-explorer`, `daily-notes`, `sync`, `properties`, `bases`, `global-search`. Others inferred from canonical order (published `core-plugins.json` shape): 24 more listed below.

### file-explorer

- **Behaviour:** Left sidebar file tree. Covered above.
- **Status:** Have.

### global-search

- **Behaviour:** Left sidebar search panel with DSL. Covered above.
- **Status:** Partial.

### switcher (Quick Switcher)

- **Behaviour:** `Mod+O` fuzzy note picker.
- **Status:** Partial.

### command-palette

- **Behaviour:** `Mod+P` command picker.
- **Status:** Missing.

### properties

- **Behaviour:** In-document frontmatter editor.
- **Status:** Missing.

### bases

- **Behaviour:** `.base` database view.
- **Status:** Missing.

### canvas

- **Behaviour:** `.canvas` infinite-canvas view.
- **Status:** Have.

### backlink

- **Behaviour:** Backlinks pane below note.
- **Status:** Partial.

### outgoing-link

- **Behaviour:** Right-sidebar outgoing links.
- **Status:** Missing.

### graph

- **Behaviour:** Interactive force-directed vault graph.
- **Status:** Partial.
- **Implementation:** `src/graph/`, `libs/forcegraph/`.
- **Notes:** Compat with `.obsidian/graph.json` persistence unconfirmed.

### outline

- **Behaviour:** Right-sidebar heading outline.
- **Status:** Missing.

### tag-pane

- **Behaviour:** Right-sidebar tag tree.
- **Status:** Partial.

### bookmarks

- **Behaviour:** Bookmark files/searches/blocks. Persisted in `.obsidian/bookmarks.json`.
- **Status:** Missing.

### page-preview

- **Behaviour:** Hover-link popover plugin. Consumes `hover-link` event + `hoverLinkSources` registry.
- **Status:** Missing.

### templates

- **Behaviour:** Insert template via command; `{{date:…}}`, `{{time:…}}`, `{{title}}` substitutions (Moment.js).
- **Status:** Missing.
- **Notes:** **P2.** Requires Moment.js format-string parity.

### daily-notes

- **Behaviour:** Open today's daily note (creates if missing); filename from Moment.js format.
- **Status:** Missing.
- **Notes:** **P2.** Same Moment dependency.

### zk-prefixer

- **Behaviour:** Zettelkasten-style unique-note prefix generator.
- **Status:** Missing.

### unique-note

- **Behaviour:** Similar to zk-prefixer with different ID scheme.
- **Status:** Missing.

### workspaces

- **Behaviour:** Named workspaces plugin. Writes `.obsidian/workspaces.json`.
- **Status:** Missing.

### note-composer

- **Behaviour:** Extract selection to new note / merge two notes.
- **Status:** Missing.

### slash-command

- **Behaviour:** Type `/` in the editor to open a command picker.
- **Status:** Missing.

### random-note

- **Behaviour:** Command to open a random note.
- **Status:** Missing.

### word-count

- **Behaviour:** Status-bar word/char count.
- **Status:** Missing.

### editor-status

- **Behaviour:** Status-bar cursor position + modification indicator.
- **Status:** Missing.

### audio-recorder

- **Behaviour:** Record audio into the vault.
- **Status:** Missing.

### markdown-importer

- **Behaviour:** Import Evernote / Notion / etc.
- **Status:** Missing.

### slides

- **Behaviour:** Render a note as a slideshow.
- **Status:** Missing.

### file-recovery

- **Behaviour:** Periodic snapshots of note content; recovery modal. Storage target of `TextFileView.save` error path.
- **Status:** Missing.
- **Notes:** Cross-refs to "Save-failure content backup" above.

### sync

- **Behaviour:** Obsidian Sync (commercial). Out of scope for Corbomite.
- **Status:** N/A.

### publish

- **Behaviour:** Obsidian Publish (commercial). Out of scope for Corbomite.
- **Status:** N/A.

---

## 12. Community plugins

### Plugin manifest parsing

- **Behaviour:** Load `.obsidian/plugins/<id>/manifest.json` → `PluginManifest` object; inject `dir`.
- **Domain:** `plugin.md §2 PluginManifest`; `VAULT-FORMAT.md §3 manifest.json`.
- **Status:** Missing.
- **Notes:** **P3.** Phase 1 of future Corbomite plugin loader.

### Plugin lifecycle (load / enable / disable / unload / user-disable)

- **Behaviour:** `onload`/`onunload` overrides; `_userDisabled` distinguishes user-toggle-off (detaches leaves of registered view-types) from reload (leaves left alone).
- **Domain:** `plugin.md §1, §8, §9`.
- **Status:** Missing.

### Plugin data.json persistence

- **Behaviour:** `loadData` / `saveData` read/write `.obsidian/plugins/<id>/data.json`; self-edit suppression via mtime hint.
- **Domain:** `plugin.md §3`.
- **Status:** Missing.

### Plugin external-settings change hook

- **Behaviour:** `onExternalSettingsChange()` fires when `data.json` mtime advances past last-written.
- **Domain:** `plugin.md §10`.
- **Status:** Missing.

(Plugin-extension-point features are enumerated in `PLUGIN-API-SKETCH.md` rather than here — they are API surfaces, not user features.)

---

## 13. Sync / Publish (out of scope)

Obsidian Sync and Obsidian Publish are commercial services. Corbomite will not reimplement them. The `SecretStorage` (keychain wrapper) API is the only footprint — useful for Corbomite's own plugin secrets. See `leaf-utilities.md §1 SecretStorage`.

---

## 14. Cross-cutting features

### Events bus (universal `Events` mixin)

- **Behaviour:** Every major object (`Vault`, `MetadataCache`, `Workspace`, `WorkspaceLeaf`, `ViewRegistry`) mixes in `Events`: `.on/off/offref/trigger/tryTrigger`. `tryTrigger` re-throws exceptions asynchronously.
- **Domain:** `core.md §1 Events`.
- **Status:** Partial.
- **Notes:** Qt signals are typed; compat requires a `QVariantList` dispatch layer or explicit signal-per-name. Async re-throw semantics must be emulated. See GAP-ANALYSIS §P3.

### App-level events → `app.workspace` (Pass 1 correction)

- **Behaviour:** `app.on(...)` is a **no-op**; events like `css-change` and `quit` fire on `app.workspace`.
- **Domain:** `core.md §1, §4, §8`.
- **Status:** N/A (informational).
- **Notes:** **Pass 1 correction 2.** Document in plugin API spec so Corbomite doesn't expose a plausible-but-inert `app.on`.

### Scope (hotkey stack)

- **Behaviour:** `Scope` tree rooted at `Keymap.global`; `register(modifiers, key, fn)` handlers; parent-chain fallback for catch-alls.
- **Domain:** `core.md §1 Scope`.
- **Status:** Missing.
- **Notes:** Qt has no stack-scoped hotkeys. See GAP-ANALYSIS §P3.

### Component lifecycle

- **Behaviour:** Universal `load`/`unload` with child registration + cleanup thunks (`registerEvent`, `registerDomEvent`, `registerInterval`, `addChild`). LIFO cleanup.
- **Domain:** `ui-bundle.md §1 Component, §8`.
- **Status:** Missing.
- **Notes:** **P3.** First-order plugin-API primitive. See GAP-ANALYSIS §P3.

### Obsidian-protocol (`obsidian://`) URL scheme

- **Behaviour:** `obsidian://<action>?...` URLs dispatched to registered handlers. 11 built-ins: `open`, `new`, `search`, `show-plugin`, `show-theme`, `show-release-notes`, `debug-info`, `publish-sites`, `sync-setup`, `vault-setup`, `hook-get-address`.
- **Domain:** `workspace.md §7 protocolHandlers`; `core.md §6`.
- **Status:** Missing.
- **Notes:** **P3 plugin-ready.** See GAP-ANALYSIS §P3.

### Moment.js date formatting

- **Behaviour:** Global `window.moment`; used by `MomentFormatComponent`, templates, daily notes, zk-prefixer.
- **Domain:** `leaf-utilities.md §1 moment`.
- **Status:** Missing.
- **Notes:** **P1.** `QDateTime::toString` tokens differ. See GAP-ANALYSIS §P1.

### Platform capability flags

- **Behaviour:** `Platform.canExportPdf`, `.canPopoutWindow`, `.canStackTabs`, `.isMacOS`, `.isLinux`, etc. gate UI features.
- **Domain:** `leaf-utilities.md §1 Platform`.
- **Status:** Partial.
- **Notes:** Corbomite is desktop-only; most flags constant. Plugin-API shim needs stub struct.

### Drag manager

- **Behaviour:** `app.dragManager` coordinates drag of links, files, folders, tab headers across panes + popout windows.
- **Domain:** `core.md §2 dragManager`; `workspace.md §5 onDragLeaf`.
- **Status:** Missing.

### `vault.process(file, mut)` atomic read-modify-write

- **Behaviour:** Per-file serialised mutation primitive; callback receives current text, returns new text.
- **Domain:** `vault.md §1`.
- **Status:** Missing.

### `FileManager.processFrontMatter(file, mut)`

- **Behaviour:** Atomic frontmatter mutation; parse → mutate JS object → serialise. Preserves key order.
- **Domain:** `vault.md §1, §10`; `parsing.md §1`.
- **Status:** Missing.
- **Notes:** **P2 plugin-API surface.** Requires YAML library.

---

## Summary counts

Counting every distinct feature row in §1–§11 (excluding §13 out-of-scope Sync/Publish and `N/A` mobile-only entries):

| Status | Count |
|---|---|
| Have | ~15 |
| Partial | ~35 |
| Missing | ~70 |

(Exact counts depend on how sub-features are bucketed; the distribution gives the shape.)

**Headline reading:**

- ~15 features work and match Obsidian closely.
- ~35 features work but diverge (different persistence, missing nuance, subset of functionality).
- ~70 features are missing entirely.

Of the missing features, the highest-leverage clusters are:

1. **Vault-format compat** (reading/writing `.obsidian/*.json`): unblocks `Partial` → `Have` for Appearance, Hotkeys, Layout restore, Core Plugins toggle.
2. **YAML library + frontmatter helpers**: unblocks Properties panel, tags pane, aliases, Templates, Daily Notes.
3. **Bases** (8–10 weeks): the single biggest missing feature.
4. **Markoff three-mode pivot**: unblocks Live-preview, proper workspace.json round-trip.
5. **MetadataCache equivalence** (five events, `CachedMetadata` shape, shortest-path resolver): unblocks plugin compat once Corbomite builds a plugin API.

See `GAP-ANALYSIS.md` for detailed priority, clustering, and proposed work.
