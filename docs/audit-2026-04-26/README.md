# Corbomite vs Obsidian: Comprehensive Audit (2026-04-26)

> Synthesis of 14 parallel domain audits comparing the Corbomite codebase
> against its reverse-engineered Obsidian audit specs. The translation
> principle: Corbomite is a *translation* of Obsidian, not a clone — Qt
> widgets replace HTML/CSS, KDE primitives replace Obsidian primitives,
> but the **on-disk vault format must be byte-compatible**.

---

## Table of contents

1. [Executive summary](#executive-summary)
2. [Methodology](#methodology)
3. [Status by domain](#status-by-domain)
4. [Vault-format compatibility risks (CRITICAL)](#vault-format-compatibility-risks-critical)
5. [Translation successes](#translation-successes)
6. [Architecture: where it fits, where it's strained](#architecture-where-it-fits-where-its-strained)
7. [Implemented (parity-equivalent)](#implemented-parity-equivalent)
8. [Partial / divergent](#partial--divergent)
9. [Missing](#missing)
10. [Suspected bugs (concrete, file-cited)](#suspected-bugs-concrete-file-cited)
11. [Cross-cutting themes](#cross-cutting-themes)
12. [Recommended priority order](#recommended-priority-order)
13. [Per-domain detailed findings](#per-domain-detailed-findings)

---

## Executive summary

Corbomite has accomplished a remarkably faithful skeletal translation of Obsidian onto Qt6/KDE Frameworks 6. The architectural shapes — `Vault`/`FileManager` aggregate, `MetadataCache` with hash-keyed dedup, `Workspace`+`WorkspaceLeaf` hierarchy, `View`→`ItemView`→`FileView`→`EditableFileView`→`TextFileView` chain, `Component` lifecycle, `Events` mixin, KPluginFactory-based plugin host with end-to-end permission gating — all map directly onto their Obsidian counterparts and are wired correctly. Cluster I (metadata), Cluster J (embeds + post-processors), Cluster K (Bases MVP), Cluster G (Workspace), Cluster Q.0 (Vault canonicalization), Cluster Q (plugin host), and Cluster N (plugin-ready surfaces) have collectively delivered the load-bearing structure. The fuzzy matcher, Pratt parser for the Bases formula DSL, KSyntaxHighlighting code blocks, JKQTMathText math, mmdr Rust mermaid bridge, and HoverPopover-with-real-ReadingView are all genuine translation successes.

That said, the **on-disk vault format compatibility story is alarming**. Five distinct round-trip risks would cause silent data churn or loss when a vault is shared between Corbomite and Obsidian: (1) `processFrontMatter` reorders YAML keys alphabetically on every save because the mutator surface is `QVariantMap` which is a `QMap` (sorted); (2) `Workspace::serialize()` flattens nested splits and emits one global `currentTab`, losing per-group tab selections and pane sizes; (3) the YAML emitter for `.base` files alphabetises keys; (4) the `Vault::writeConfigJson` / `VaultProxy` plugin path uses 4-space indent instead of Obsidian's 2-space, no-trailing-newline format, so plugin-written `.obsidian/*.json` files diverge byte-for-byte; (5) `resolveSubpath` block-id lookup is case-sensitive where Obsidian's is case-insensitive, and the heading walker matches only the trailing segment of `[[Note#H1/H2/H3]]` rather than enforcing nesting. None of these are conceptually hard to fix; all four are silent — no exception, no warning, no diff signal until the user opens the vault in the other tool.

The second-largest concern bucket is **plugin extensibility surface drift**. Six host-side registries exist with no plugin proxy: `HoverLinkSourceRegistry`, `EditorSuggestManager`, `PostProcessorRegistry`, `RibbonToolBar`, `EmbedRegistry`, `CodeBlockProcessorRegistry`. Six more verbs are entirely missing: `addStatusBarItem`, `registerObsidianProtocolHandler`, `registerEditorExtension` (Markoff has no `EditorExtension` type), `registerMarkdownCodeBlockProcessor` plugin facade, `MarkdownRenderer.render()` static API, and `addIcon`/`removeIcon` (Lucide icon registry). Plugins authored against Obsidian's documented surface cannot today reach into hover-link decorating, post-processor decoration, or custom-icon rendering — even though the host has half the plumbing. Cluster N "plugin-ready surfaces" closed but did not close the proxy facades.

The third bucket is **lifecycle and per-vault isolation drift**. `MainWindow::onVaultClosed` correctly destroys most per-vault services but leaves three dangling adapter pointers (`m_linkResolverAdapter`, `m_metadataCacheAdapter`, `m_metadataParserImpl`) holding raw pointers to deleted services. `EmbedRegistry` factories survive vault switches with stale captures (latent until a plugin registers an embed). Plugin `disablePlugin` does not detach leaves of the plugin's view-types, orphaning open tabs. `onLoad` throws leak state. There are no `appId` (per-vault namespace), no `dragManager`, no `quit` collector for plugin async-cleanup, and no `css-change` event bridge from `ThemeService`.

Top recommended next foci, ranked: **(1) frontmatter key-order preservation in `processFrontMatter`** (silent data churn on every metadata edit, in both directions); **(2) consolidate the two `.obsidian/*.json` writers** so `Vault::writeConfigJson` routes through `VaultConfig::serializeObsidianStyle` (Bookmarks plugin currently writes 4-space indent); **(3) fix `resolveSubpath` block-id case sensitivity** (link resolution divergence breaks `[[Note#^MyBlock]]`); **(4) folder-rename descendant rekeying** (`m_fileMap` desyncs); **(5) the missing six plugin proxies** (`registerHoverLinkSource`, `registerEditorSuggest`, `registerMarkdownPostProcessor`, `addRibbonIcon`, `MarkdownRenderer.render` static, icon registry); **(6) HoverPopover mod-key pinning + child-popover chains** (the popover renders correctly but the interaction model is half-built); **(7) callouts** (`> [!note]`) — the data model exists in `Theme` but the rendering pipeline ignores them; **(8) footnotes** are numbered but non-navigating, and definitions are silently dropped.

---

## Methodology

Fourteen parallel sub-agents were dispatched, each comparing one Obsidian audit-domain document under `docs/obsidian-audit/domains/` against the corresponding Corbomite source tree. Each sub-report covers architecture fit, implemented features (parity-equivalent), partial/divergent behaviours, missing features, notable translation successes, suspected bugs (with `path:line` citations), and any domain-specific risk section (vault-format compat, plugin extensibility, etc.).

Sub-reports synthesised:

| Domain | Spec | Sub-report |
|---|---|---|
| vault | `domains/vault.md` | `./vault.md` |
| metadata | `domains/metadata.md` | `metadata.md` |
| workspace + leaf-utilities | `domains/workspace.md` + `domains/leaf-utilities.md` | `workspace.md` |
| views | `domains/views.md` | `views.md` |
| editor | `domains/editor.md` | `editor.md` |
| editor-markdown | `domains/editor-markdown.md` | `editor-markdown.md` |
| rendering | `domains/rendering.md` | `rendering.md` |
| parsing | `domains/parsing.md` | `parsing.md` |
| bases | `domains/bases.md` + DSL addendum | `bases.md` |
| search | `domains/search.md` | `search.md` |
| settings | `domains/settings.md` | `settings.md` |
| ui-bundle | `domains/ui-bundle.md` + 7 dialog addenda | `ui-bundle.md` |
| plugin | `domains/plugin.md` + `PLUGIN-API-SKETCH.md` | `plugin.md` |
| core + addenda | `domains/core.md` + 5 addenda | `core-and-addenda.md` |

The full text of every sub-report is reproduced under [Appendix: full sub-reports](#appendix-full-sub-reports) so this synthesis is self-contained. All `path:line` citations propagate from the sub-reports.

---

## Status by domain

| Domain | Posture | Headline finding |
|---|---|---|
| Vault | **Strong** | ~70% of the Obsidian surface area; gaps in `raw`/`config-changed` events, folder-rename recursion, BOM strip, and link-rewrite fidelity (markdown links, full-path forms). |
| Metadata | **Strong** | Cluster I shipped a sound architecture; `LinkResolver` faithfully ports the 6-step algorithm; gaps are downstream (no `unresolvedLinks` reverse map, no subtag-prefix tag counts, no `getFirstLinkpathDest` exposure to plugins). |
| Parsing | **Mostly there with critical bugs** | rapidyaml is the right shape; tree-sitter grammar covers the Obsidian extensions; **but** `processFrontMatter` rewrites keys alphabetically on every save (P0 vault-compat). |
| Bases | **Mostly there at runtime, skeletal in UI** | All 19 Value types present; Pratt parser + hard-cased `if`/`map`/`filter`/`reduce` correct; YAML emitter alphabetises keys, `unrecognizedData` truncates non-scalars, no formula editor, no group rendering, no export. |
| Workspace | **Half-built** | KDDW substrate is the right call but the serializer is round-trip-lossy in 5+ ways; two parallel serializers; `m_tabGroupOf` lags drag operations; popout windows can leak; `workspace.json` writes can degrade an Obsidian vault on first save. |
| Views | **Strong** | Five-class hierarchy ports cleanly; `ViewRegistry` is well-designed; gaps are inline-rename, breadcrumbs, ephemeral-state plumbing, vault rename/delete reactions per-view. |
| Editor | **Half-built** | Live/Source/Reading split is sound; `EditorSuggest` ports 1:1; **but** no `getLine`/`replaceRange`/`getSelection`/`setSelection` public API on `Markoff::Editor` — no plugin-API parity for any meaningful editor extension. |
| Editor-markdown | **Half-built** | Reading mode's progressive section pipeline matches Obsidian's frame-budget contract exactly; live preview lacks per-block reveal granularity; checkbox round-trip broken; no `quick-preview`/`markdown-scroll` events; no `MarkdownRenderer.render` static API. |
| Rendering | **Half-built** | Math/mermaid/syntax-highlighting work; HoverPopover-with-ReadingView is a translation success; **but** callouts aren't rendered, footnotes are functionally dead, no PDF view, no `htmlToMarkdown` paste handler, no `RenderContext`-equivalent for cross-surface link consistency. |
| Search | **Mostly there with two correctness bugs** | Fuzzy matcher is line-for-line accurate; FTS5 backend handles the DSL; **bare `/regex/` query and top-level `-foo` both return zero results** because of bad guard logic and invalid FTS5 emission. |
| Settings | **Skeletal on UI; partial on disk** | KConfig schema covers 14 keys vs Obsidian's ~70; `app.json`/`appearance.json` are read-mostly (only `userIgnoreFilters`); hotkeys parser exists but is dead code; PluginsPage does not wire `core-plugins.json`/`community-plugins.json` to PluginManager. |
| UI bundle | **Mixed** | `Component`, `MenuSectionHelper`, `MenuEventEmitter`, `Notice`, `CompletionPopup`, `EditorSuggest` ported well; **HoverPopover lacks Mod-key pinning + child-popover chains**; `Notice(text, 0)` immediately closes; FileExplorer context menu is 4 items vs ~13+ and has no plugin emission. |
| Plugin | **Strong host, gappy registration** | KPluginFactory + permission gating end-to-end; **only 6 of 13 Obsidian registration verbs have plugin proxies**; no JS shim (every plugin must be C++); 9 of ~25 internal plugins shipped. |
| Core/addenda | **Mixed** | Service split (`CorbomiteApp` vs `MainWindow`) is sound; `Events`/`Scope` ported faithfully; **`resolveSubpath` block-id case-sensitivity bug**; per-vault isolation has dangling adapter pointers; `appId`/`dragManager`/`quit`/`css-change` all absent. |

---

## Vault-format compatibility risks (CRITICAL)

This is the section the project owner most needs to act on. Every entry below is a *silent* data divergence — no exception, no warning, no diff signal until the user opens the vault in the other tool.

### 1. Frontmatter key reordering on every save (HIGH)

`FileManager::processFrontMatter` (`libs/vault/src/FileManager.cpp:138-143`) round-trips through a `QVariantMap`. `QVariantMap` is `QMap<QString, QVariant>` — sorted. Therefore *every* call rewrites the file with frontmatter keys in alphabetical order, regardless of source order. A user who edits a note's `tags` through Corbomite's properties UI sees the entire frontmatter reshuffle on save; opening the same file in Obsidian re-shuffles it back. **Diff churn on every edit, in both directions.**

This violates Obsidian's documented invariant ("JS object property insertion order — stable for existing keys if not deleted/re-added"). Per `parsing.md` §"Frontmatter round-trip risks" point 1: *"This is the highest-priority finding."*

**Fix sketch:** Replace the `QVariantMap` mutator surface with an order-preserving wrapper, or mutate the `Markoff::YamlValue` tree in place (it preserves key order via ryml).

### 2. Bases `.base` YAML emitter alphabetises keys (HIGH)

`BasesQuery::toString`'s hand-rolled `emitMap` (`libs/bases/src/BasesQuery.cpp:108-127`) iterates `QVariantMap::constBegin()` which is alphabetised. **Same root cause as #1.** An Obsidian-authored `.base` opened, edited (or even just saved unchanged), and written back will have all keys reshuffled. Highly visible diff churn for VCS users.

**Fix sketch:** Use an ordered map type or a fixed canonical emit order. Add a round-trip test against an Obsidian-authored `.base` golden file.

### 3. Two `.obsidian/*.json` writers with different formatters (HIGH)

There are two parallel writers:

- `Corbomite::VaultConfig::writeJson` (`libs/storage/src/VaultConfig.cpp:95-100`) calls `serializeObsidianStyle` → 2-space indent, no trailing newline. **Matches Obsidian.**
- `Corbomite::Vault::writeConfigJson` (`libs/vault/src/Vault.cpp:448-461`) calls `doc.toJson(QJsonDocument::Indented)` → 4-space indent, trailing newline. **Does not match.** This is the entry point re-exposed to plugins via `VaultProxy::writeConfigJson` (`libs/vault/src/proxies/VaultProxy.cpp:269-285`), which is what the Bookmarks plugin actually calls (`src/plugins/bookmarks/BookmarksPlugin.cpp:82`).

Result: `bookmarks.json` (and any future plugin-written config) is written with 4-space indent — `git diff` against an Obsidian-written file will show every non-blank line as changed even if no value changed.

**Fix sketch:** Route `Vault::writeConfigJson` through `VaultConfig::serializeObsidianStyle`. Single function-pointer change.

### 4. `resolveSubpath` block-id case sensitivity (MEDIUM)

Obsidian lowercases both sides for `^blockid` lookup. Corbomite (`libs/core/src/LinkUtils.cpp:121-155`) scans the source for the literal marker string. `[[Note#^MyBlock]]` against a `^myblock` definition resolves in Obsidian and silently fails in Corbomite.

This is also one of two `resolveSubpath` implementations in the tree — `Markoff::Document::extractSubpath` (`libs/markoff-family/libs/markoff-parser/src/Document.cpp:274-374`) uses hyphen-to-space + lowercase rather than `stripHeading`, and is called from different code paths than `LinkUtils::resolveSubpath`. **Two resolvers with different rules.**

**Fix sketch:** Lowercase both sides of the block-id comparison; consolidate the two `resolveSubpath`/`extractSubpath` implementations.

### 5. `resolveSubpath` multi-segment heading walk (MEDIUM)

Per `core-and-addenda.md` concern #2: `[[Note#H1/H2/H3]]` resolves on the first matching `H3` regardless of whether it nests under `H1 > H2`. Obsidian requires each intermediate path component to match a properly-nested ancestor heading. Self-noted in the source comment at `LinkUtils.cpp:167-168`. Vault-portability divergence; low impact in practice (multi-segment subpaths are rare).

### 6. Folder rename loses descendants (HIGH)

`Vault::rename` (`libs/vault/src/Vault.cpp:337-373`) takes the renamed node out of `m_fileMap`, updates its single key, and re-inserts. For a `TFolder` with children, the descendants' `m_fileMap` keys still point to the old prefix; their `path` strings are stale. A subsequent `getFileByPath("oldFolder/child.md")` returns `nullptr` and `getFileByPath("newFolder/child.md")` also returns `nullptr` until the watcher catches up.

**Fix sketch:** Recursive rekey with cache invalidation per descendant; emit `renamed` per descendant (Obsidian's adapter does this).

### 7. `FileManager::renameFile` link-rewrite is wikilink-only and basename-only (HIGH)

`FileManager.cpp:148-216` rewrites `[[oldBase]]`, `[[oldBase|`, `[[oldBase#`. **Misses:**

- Markdown-style `[label](Note.md)` and percent-encoded variants
- `[[folder/Note]]` (full-path forms)
- `[[oldBase \| alias]]` with whitespace
- References inside frontmatter (`note: "[[oldBase]]"`)

Vaults with `useMarkdownLinks=true` get **no** rename refactoring at all. Soft data-loss bug: links break, no notice.

### 8. Workspace.json round-trip is lossy in 5+ dimensions (HIGH for vault interop)

Per `workspace.md` "Layout JSON compatibility risks":

| Field | Status |
|---|---|
| `main` root with nested splits | flattened to one split + N flat tabs |
| `dimension` flex-grow per child | dropped on read, never written |
| `tabs.currentTab` per group | only active-leaf's group writes nonzero |
| `tabs.stacked` | persisted via sidecar but UI behaviour absent |
| `left`/`right` (sidedock subtrees) | passed through `m_unknownRoot` blindly — on-disk shape doesn't reflect actual sidebar state |
| `floating` wrapper | `Workspace::serialize` doesn't emit `floating` |
| `window.zoom` | never read or written |
| `['left-ribbon'].hiddenItems` | order from `iconIdsInOrder()` not from key order |
| Unknown leaf keys | preserved by `WorkspaceSerializer`, *discarded* by `Workspace::serialize` |

Net: **Corbomite is currently safe to *read* an Obsidian workspace.json, but unsafe to *write back*.** A user who pointed Corbomite at an in-use Obsidian vault, opened a single file, and exited would re-save a structurally simpler workspace.json.

### 9. BOM not stripped on read (LOW; cosmetic)

`Vault::read` (`libs/vault/src/Vault.cpp:151-156`) returns raw bytes; no leading-`U+FEFF` strip. A vault file authored on Windows with a UTF-8 BOM displays the BOM as the first character in the editor.

### 10. `tag:` in frontmatter (MEDIUM divergence)

`MetadataParser` (`libs/storage/src/MetadataParser.cpp:451-460`) accepts a singular `tag:` key Obsidian's `parseFrontMatterTags` does **not**. A user with `tag: foo` in frontmatter sees `#foo` indexed in Corbomite but not in Obsidian. Also, a string-typed `tags: alpha, beta` is read as one bogus tag including the comma; Obsidian drops it because it contains a space.

### 11. `withFrontmatter` cannot strip an emptied block (LOW)

`Document::withFrontmatter` always emits the `---/---` fence (`libs/markoff-family/libs/markoff-parser/src/Document.cpp:407`). A mutator that empties the map leaves a `---\n\n---\n` shell where Obsidian deletes the block.

### 12. `core-plugins.json` / `community-plugins.json` not wired (MEDIUM)

The full I/O layer exists (`VaultConfig.cpp:209-261` for core-plugins migration), but `PluginManager::enablePlugin`/`disablePlugin` does not consult or mutate either file. Toggling a plugin in Corbomite has zero effect on what Obsidian sees, and vice versa.

### 13. `appearance.json` theme value vocabulary mismatch (MEDIUM)

Corbomite writes `"theme": "system"`/`"light"`/`"dark"` (`MainWindow.cpp:2440-2453`). Obsidian uses `"obsidian"`/`"moonstone"` (or absence for system). Round-trip silently flips the user's theme each time the vault is opened in the other tool.

### 14. PluginDataStore writes 4-space indent (LOW; cosmetic)

`PluginDataStore::save` (`libs/vault/src/PluginDataStore.cpp:35`) writes `QJsonDocument::Indented`. Obsidian uses 2-space + no trailing newline. Cosmetic round-trip diff vs Obsidian.

### 15. Plugin command IDs use Corbomite prefix instead of Obsidian-canonical (MEDIUM)

`corbomite-graph-view:copy-screenshot` instead of `graph:copy-screenshot`. Canvas export has no command id at all. Vault `hotkeys.json` round-trip would not fire these. The Bookmarks plugin handles this correctly via `addCommandRaw` with the canonical `bookmarks:` prefix; Graph and Canvas don't. **Fix sketch:** standardise on `addCommandRaw` for vault-shared command ids.

---

## Translation successes

These are places where Corbomite either genuinely improves on Obsidian or faithfully preserves the contract while modernizing the implementation.

- **HoverPopover-with-real-ReadingView.** `src/editor/HoverPopover.cpp:60-69` embeds a `Markoff::Reading::ReadingView` *as the popover content*. Math, mermaid, syntax-highlighting, embeds, and recursive `![[]]` all render via the same pipeline as the full reading mode. Obsidian achieves the same via a sub-`MarkdownPreviewView`; Corbomite gets there via composition.

- **Markoff family architecture.** `MarkoffDocument` (`libs/markoff-family/libs/markoff-core/include/markoff/MarkoffDocument.h:28`) is the single canonical buffer with a `QUndoStack` of `MarkdownDelta` commands. Every leaf view subscribes to `contentsChanged`/`documentReloaded` and rebases its display. **Structurally cleaner** than CM6 for the multi-pane case.

- **Atomic per-plugin `data.json`.** `PluginDataStore::save` (`libs/vault/src/PluginDataStore.cpp:32-37`) uses `QSaveFile` for atomic write — strictly better than Obsidian's open-write-close pattern.

- **`KPluginFactory`-based, permission-gated plugin host.** Cluster Q's permission model (declare → grant → KConfig persist → proxy enforcement) is a non-trivial original addition that closes a real Obsidian security gap. Origin-based trust override (`PluginMetaData::trusted()` returns false for Origin::User regardless of JSON) is the right belt-and-braces design.

- **Pratt parser for Bases formula DSL** at ~200 lines (`libs/bases/src/Parser.cpp`). Diffs cleanly against the addendum precedence table. Replaces a 1.4 KB serialised Lezer state machine with reviewable code. Hard-cased `if`/`map`/`filter`/`reduce` lambdas (`Evaluator.cpp:367-462`) preserve correct shadowing semantics.

- **Faithful `FuzzyMatcher` port** (`libs/search/src/FuzzyMatcher.cpp`). Two-pass strategy (word-token then strict char-fuzzy with word-boundary preference), five-term scoring formula with identical denominators, CJK per-codepoint tokenisation, punctuation singletons, match-range merging — all line-for-line.

- **`Component` lifecycle port** (`libs/core/src/Component.cpp`). LIFO-children + cleanups, `addChild` auto-load, `registerInterval`/`registerQObjectConnection`/`registerCleanup`. The deliberate "Component is *not* a QObject" decision (`Component.h:32-33`) avoids MOC + MI traps.

- **`Events` mixin port** (`libs/core/src/Events.cpp:30-95`). Snapshots `byName[name]` before iterating (so self-`offref` from inside a listener doesn't shift iteration). `tryTrigger` runs each listener in `try/catch` and reschedules the exception via `QTimer::singleShot(0, ...)` — exactly Obsidian's `setTimeout(…, 0)` semantics.

- **`MenuSectionHelper`** (`libs/core/src/MenuSectionHelper.cpp`). Bucket-by-section + insert-separators-between-non-empty pattern is preserved exactly. More declarative than Obsidian's `addSections([...]) + setSection(id) + sort()` dance.

- **Per-vault NoteDocument cache + save echo-suppression** (`libs/vault/src/Vault.cpp:589-669, 779-794`). The mtime-ledger + byte-equal-disk-compare pair gives defence-in-depth against the "self-write fires watcher fires reload" loop. Better than Obsidian's `file.saving` flag because it survives cross-process editor races.

- **Frame-budget constants externalised as named header values.** `kAsyncParseThresholdBytes`, `kFrameBudgetMs`, `kFrameBudgetSections` in `ReadingViewConstants.h:15,19,24` — with `tst_frame_budget_constants` pinning them. The header docstring spells the contract out: *"These numbers are not hints; they are the documented wire contract."* Better than Obsidian's hardcoded literals.

- **Section recycling pool keyed on shape, not HTML string.** `SectionRecyclePool` keys on a `QByteArray` AST shape, avoiding the spec's "whitespace-only edit invalidates the whole paragraph" trap.

- **Tab select / tab close re-emission indirection** (`libs/core/src/Workspace.cpp:241-263`). KDDW's `isCurrentTabChanged`/`isOpenChanged` signals fire from substrate guts at unpredictable times. Workspace re-emits them as its own signals with an identity gate. Decouples KDDW's eager signaling from Workspace's identity-gated state machine.

- **Markdown-cache content hashing dedup.** `MetadataCache` (`libs/storage/src/MetadataCache.cpp:267-274`) uses SHA-256 hash-keyed `m_hashToCache` with refcount GC — preventing the "two duplicate notes deleted in sequence leaves a dangling cache row" failure mode.

- **`LinkResolver` 6-step algorithm** (`libs/storage/src/LinkResolver.cpp:100-198`) is a faithful translation of spec §8 — rooted-`/` exact match, `./..` relative, basename retry, single-candidate-with-extension shortcut, short-name disambiguation with same-folder bias.

- **Cluster N's two-axis API gating** (`X-Corbomite-MinVersion` semver + `X-Corbomite-ApiLevel` integer). Strictly more capable than Obsidian's single-axis `apiVersion`. Discovery-time gate state recorded eagerly so the Plugins page can render compat status without instantiating plugins.

- **`Platform::showInFolder` Linux DBus path** (`libs/core/src/Platform.cpp:46-61`) — direct DBus call to `org.freedesktop.FileManager1.ShowItems` succeeds on Dolphin/Nautilus/Nemo/PCManFM-Qt. Better than Electron's round-trip.

- **`Vault`'s mtime-ledger echo suppression and U+FFFC terminal guard** (`Vault.cpp:627-634`). The latter refuses to write the canonical buffer if it contains `QChar::ObjectReplacementCharacter`. Catches presentation-layer-leaking-into-source bugs Obsidian doesn't have to worry about.

- **`SQLiteIndex` as derived view, not the writer.** Cluster I's pivot: `SQLiteIndex` subscribes to `MetadataCache::cacheChanged` and derives FTS/links/note_tags rows. Eliminates the prior "two parsers, two truths" problem.

- **`MetadataCache` worker offloading.** `MetadataWorker` runs parses on a dedicated `QThread`; results delivered main-thread via `Qt::QueuedConnection`. Sequential FIFO matching Obsidian's "Work queue must be sequential!" invariant.

- **`CompletionPopup` as child widget** (`src/editor/CompletionPopup.h:21-27`) parented to the editor viewport with `Qt::NoFocus + WA_ShowWithoutActivating`. Avoids focus-stealing; modelled after KateCompletionWidget. The completion accept routes through `MarkoffDocument::undoStack()` via `MarkdownDelta` (`NoteEditorWidget.cpp:614-619`) — preserves undo history.

- **Markoff parse pool ownership** (`Vault.h:193`, `Vault.cpp:50, :595, :69-70`). Explicit `qDeleteAll(m_docs)` in dtor before the pool is destroyed prevents the UAF that Qt parent-child ordering would otherwise produce.

---

## Architecture: where it fits, where it's strained

### Markoff family (live/source/reading)

**Sound.** The `MarkoffDocument` canonical buffer with per-leaf observers replaces CM6's per-`EditorView` `EditorState`. The mode-swap pattern (`NoteEditorWidget::setViewMode`, `src/editor/NoteEditorWidget.cpp:337-371`) detaches→swaps→reattaches without round-tripping content through the leaves. Cleaner than Obsidian's `set(data, false)` re-push.

**Strained:** No public per-line accessors (`getLine`, `lineCount`), no `replaceRange`, no `getSelection`/`setSelection`, no `posAtCoords`, no editor-extension surface. Plugin parity for any meaningful editor extension is blocked. Live vs Source cursor convention also disagrees (1-based vs 0-based — `Editor.cpp:1590-1595` vs `SourceEditor.cpp:257`).

### KDDockWidgets vs hand-rolled splits

**Sound for the substrate.** KDDW gives drag-tab, drag-to-edge-to-split, floating windows, restore-from-binary-blob, and tab reorder for free. Worth months of hand-rolled work.

**Leaky** for two reasons. First, KDDW exposes no public Group enumeration API, so `m_tabGroupOf` lags user drags (`Workspace.h:267-271` openly admits this). Second, the substrate doesn't natively produce `workspace.json` shape, leading to two parallel serializers (`Workspace::serialize` vs `WorkspaceSerializer::toJson`) producing different shapes from different sources of truth. The Phase 5/6 "tree walker that round-trips KDDW's nested split structure" (per `Workspace.cpp:651-658`) hasn't landed.

### KPluginFactory vs Obsidian's `eval()`-on-`main.js`

**Sound for KDE integration.** First-class KPluginMetaData, KPluginInstaller-ready, KConfigGroup persistence, multi-arch packaging. Permission-gated proxies are end-to-end correct.

**Strained for Obsidian compat.** Compile-once C++ ABI rather than load-anywhere JavaScript. Every Obsidian plugin would need a C++ rewrite + recompile. The compat ceiling is *Obsidian-shaped API* not *Obsidian-binary-compat*. `PLUGIN-API-SKETCH.md` §1 actually recommended a JS sandbox ("never achieve C++ ABI parity"); the team chose differently. That's defensible but locks out the existing plugin ecosystem.

### JKQTMathText vs MathJax

**Partially leaky.** Static-linked Qt-native LaTeX typesetter is the right shape — no JS heap, no async load. Process-wide cache keyed on `(latex, displayMode, fontSize, dpr)` is idiomatic.

**Coverage is below MathJax v3** for AMS extensions (`\begin{align}`, `\binom`), `\newcommand`/`\renewcommand`, color, font stack switching, and there's no `loadMathJax` plugin-API shim. Math-dense notes will render visibly different. Display-mode also only adjusts font size (`MathRenderer.cpp:56-58`); MathJax actually switches the parser into display mode (different spacing rules).

### mmdr Rust bridge vs mermaid.js

**Functional but unthemed and uncached.** The Rust archive gives us a 100% offline, no-Node mermaid implementation. But: no theme integration (`MermaidRenderer::renderSvg(source)` takes only the source string — no `theme: 'dark'` parameter), so dark-mode users see near-illegible text against the SVG's default light backdrop. No process-wide cache; every recycle re-runs Rust SVG generation. Diagram-type coverage of mmdr vs current mermaid.js is unknown but almost certainly partial.

### KConfig vs `Setting` fluent builder

**Sound choice for KDE.** KCfg/KConfigXT singleton (`src/app/corbomite.kcfg`) gives strongly-typed accessors and standard XDG persistence. The Settings dialog uses `KPageDialog` with five hardcoded pages — structurally superior to Obsidian's hand-rolled tabbed modal (keyboard nav, native window management, per-page icons all free).

**Strained on coverage and bidirectional sync.** Only 14 of Obsidian's ~70 settings keys are in the schema. Crucially, `app.json` is read-mostly (only `userIgnoreFilters` consumed; nothing written back) and `appearance.json` writes `theme` with the wrong value vocabulary. There is no inbound read at vault open that pulls `app.json`/`appearance.json` into KConfig. `hotkeys.json` parser exists but is dead code; no bridge to `KSharedConfig`'s `Shortcuts` group.

### Two ambient pub-sub systems (Qt signals vs `Events`)

**Strained.** `Corbomite::Events` is a faithful port (used by `MetadataCache`'s plugin event-bus), but `Workspace`, `Vault`, `ViewRegistry`, `MetadataCache`-the-typed-side, all use Qt signals. Plugin code reaching `app.workspace.on("layout-change")` would need a translation layer that maps Obsidian event names onto QObject + Qt signal names. As of 2026-04-26, no such mapping exists.

### Two `.obsidian/*.json` writers

**Actively wrong.** Covered in vault-format risks #3. `VaultConfig::writeJson` (Obsidian-format) and `Vault::writeConfigJson` (4-space, plugin-facing) need to share a single formatter.

### Two `resolveSubpath` implementations

**Actively wrong.** Covered in vault-format risks #4. `Markoff::Document::extractSubpath` and `Corbomite::resolveSubpath` use different rules and are called from different code paths.

### Two parallel workspace serializers

**Actively wrong** (in-flight transition). Covered in vault-format risks #8. Until consolidated, every code path needs to be checked for which writer it routes through.

### Sidebar plugin panels are not `ItemView` subclasses

**Strained for plugin compat.** Backlinks, Outline, Properties, Outlinks, Search, LocalGraph, Bookmarks all mount bare `QWidget`s into KDDW dock areas. They get no `addAction` chrome, no "…" menu, no `setViewState`/`getViewState`, no `leaf-menu`/`file-menu` events, no command-dispatch hook. Off-the-shelf Obsidian sidebar panels (`extends ItemView`) cannot be ported as-is.

---

## Implemented (parity-equivalent)

Grouped by domain, with citations.

### Vault domain

- TFile/TFolder hierarchy with NFC paths (`libs/vault/src/{TFile,TFolder,TAbstractFile}.cpp`).
- Path normalisation applied at every Vault entry point (`libs/vault/src/PathNormalization.cpp:6-18`).
- Read API (`read`/`readBinary`/`readRaw`/`cachedRead`, `Vault.cpp:151-179`).
- Write API (`modify`/`modifyBinary`/`append`/`process`/`create`/`createBinary`/`createFolder`/`rename`/`remove`/`copy`/`trash`, `Vault.cpp:181-530`).
- `.trash/` writer with collision suffixing (`Vault.cpp:495-528`).
- System-trash via `QFile::moveToTrash` with fallback to `.trash/` (`Vault.cpp:492`, `FileSystemAdapter.cpp:124`).
- Echo suppression via mtime ledger (`Vault.cpp:779-794`) + byte-equal-disk-compare (`Vault.cpp:721-730`).
- External-edit conflict UX via `externalReloadConflict` signal + `Markoff::Origin::ExternalReloadClean` auto-reload (`Vault.cpp:759`, `:770`).
- VaultConfig schema-aware reads/writes for app/appearance/community-plugins/hotkeys/daily-notes/templates/core-plugins; legacy core-plugins.json array→object migration matches Obsidian (`VaultConfig.cpp:209-256`).
- `parseLinktext` byte-exact port (`LinkTextParser.cpp:6-16`).
- `generateMarkdownLink` (wikilink path, `FileManager.cpp:306-324`).
- `validateFileName` cross-platform reserved-name set (`FileNameValidator.cpp:34-81`).
- `VaultProxy`/`FileManagerProxy` with permission-token gates.

### Metadata domain

- `CachedMetadata` with all 13 spec-§2 fields including in-memory ↔ on-disk `frontmatterPosition`/`frontmatterPos` rename via `toPersistedJson`/`fromPersistedJson` (`CachedMetadata.h:163-169`).
- Two-layer hash-keyed dedup with refcount GC (`MetadataCache.cpp:267-274, 301-314`).
- Stat short-circuit on mtime+size match (`MetadataCache.cpp:96-102`).
- Five-event lifecycle in correct order: `cacheChanged` → `linksResolvedFor` → `allLinksResolved` → 10ms-debounced `indexFinished`; `cacheDeleted` independent (`MetadataCache.cpp:316-427`, debounce constant matches Obsidian's `didFinish = debounce(…, 10, leading=true)`).
- Plugin-facing event-bus mirror with Obsidian-named string keys (`MetadataCache.cpp:336-339, 396-409`).
- `MetadataWorker` `QThread`-resident parser with sequential FIFO.
- Persistence + destructive `PRAGMA user_version` migration mirroring Obsidian's IndexedDB v19 wipe (`CachedMetadataStore.cpp:92-100`).
- `LinkResolver` 6-step algorithm including same-folder bias (`LinkResolver.cpp:100-199`).
- Unsupported-files-in-fileCache distinguished from absent (`MetadataCache.cpp:199-217`).
- `prevCache` capture before delete (`MetadataCache.cpp:181-193`).
- Tag aggregation in-cache (frontmatter `tags`/`tag` plus inline `#tag`).
- `userIgnoreFilters` regex/prefix parsing (`IgnoreFilter.cpp:6-43`).
- All five vault signals routed to MetadataCache.
- `SQLiteIndex` as derived view subscribed to `cacheChanged`.

### Workspace + leaves

- `Workspace` aggregate with vault-scoped lifetime (`libs/core/src/Workspace.cpp`).
- `WorkspaceLeaf` API surface (id/getViewState/setViewState/setPinned/setGroup/setDeferred/loadIfDeferred/getEphemeralState/setEphemeralState).
- `pinnedChanged`/`groupChanged` signals 1:1 with Obsidian.
- Per-leaf history (`LeafHistory`, cap 20) + leaf-close undo stack (cap 10).
- `Workspace::getLeaf(LeafMode, LeafDirection)` factory mirroring Obsidian's `getLeaf(false|'tab'|'split'|'window'|true)`.
- `openLinkText` parsing first-of-`#`-or-`^` correctly.
- `layoutReady` gate as feedback-loop circuit-breaker.
- `active-leaf-change` signal with identity gate (no re-fire on identical leaf).
- Pin propagation through linked groups.
- Tab navigation primitives (`nextLeafInActiveGroup`/`previousLeafInActiveGroup`/etc.).
- Leaf id format (16 lowercase hex).
- `HoverLinkSourceRegistry` with built-ins (editor/search/backlinks/outlinks/graph/bases).
- `MenuEventEmitter` exposing all 7 mid-construction menu signals.
- `WorkspaceController` plugin facade (opaque string leaf ids over the wire).
- Sidebar substrate (Kate-derived `CorbomiteMDI::Sidebar`/`MultiTabBar`/`ToolView`).
- Vault-scoped DockRegistry namespacing (`uniqueNameFor(vaultId, leafId)`, `Workspace.cpp:31-36`).

### Views

- Five-class hierarchy (`View`→`ItemView`→`FileView`→`EditableFileView`→`TextFileView`).
- `Component` owned via `unique_ptr` rather than C++ MI.
- Lifecycle hooks (`open`/`close`/`onOpen`/`onClose`).
- `getViewType`/`getDisplayText`/`getIcon` abstract on `View`.
- `addAction` (icon, title, callback) on `ItemView`.
- `MenuSectionHelper` canonical pipeline.
- `onTabMenu` default close items.
- Back/forward navigation buttons wired to `LeafHistory`.
- `FileView` file-binding lifecycle (loadFile / unload current / null / try / revert).
- `TextFileView::save()` with `m_saving` re-entry guard, `m_saveAgain`, `m_lastSavedData` short-circuit, `previousLastSaved` rollback, immediate-mode flush, backup-on-failure.
- Three-way merge on external modify (`TextFileView.cpp:112-134`, `DiffMatchPatch::threeWayMerge`).
- ViewRegistry semantics (throw on duplicate, atomic-validation invariant, three signals).
- Empty-state pane.
- Unknown-view-type fallback.
- Deferred-load stub.
- Plugin view registration with auto-unregister via `ViewRegistrar` (RAII per-plugin scope-guard).

### Editor

- Live/Source/Reading mode swap with capture/detach/swap/attach/restore.
- Cursor get/set on Live and Source.
- Plain-text I/O.
- `coordsAtPos` analogue (`cursorScreenRect()`).
- `EditorSuggest` base class + `EditorSuggestManager` with insertion-order first-non-null-`onTrigger` wins.
- Built-in suggesters: `WikiLinkSuggest` (for `[[`), `TagSuggest` (for `#`).
- `CompletionPopup` non-focus-stealing widget.
- CJK full-width-bracket autocorrect (three patterns, longest-match-first).
- IME composition path (TextControl harvested from qtbase, `Qt::ImCursorRectangle`/`ImCursorPosition` handled).
- Find/replace cross-item with one wrap pass.

### Editor-markdown

- Three-mode encoding with on-disk shape `{mode: "preview"}` vs `{mode: "source", source: bool}` matching Obsidian byte-for-byte.
- Mode-toggle idempotence.
- Lazy mode-widget construction.
- Ephemeral-state round-trip across mode switch.
- Progressive section pipeline in Reading mode with frame-budget constants (10240-byte async threshold, 5ms frame budget, 10 sections per yield) — exact matches for Obsidian's numbers.
- Section recycling by AST shape key.
- Frontmatter-diff cascade (force re-render of any section with `usesFrontMatter()`).
- Async parse via `ReadingParseWorker` `QThread` + request-id coalescing.
- Heading collapse with descendant propagation.
- Visual-line float scroll position.
- Capability-probe-based feature gating (`hasCursor`/`hasEditing`/`hasFold`).
- `EmbedRegistry` with depth cap and placeholder fallback.
- `MarkdownRenderChild` lifecycle wrapper type (thin but in place).
- `CodeBlockProcessorRegistry` with built-in mermaid/math/latex.
- Find/Replace contract via per-leaf `SearchAdapter`.
- Hover-link wiring with shared `HoverPopover::scheduleShow`.
- Ctrl+E reading toggle.

### Rendering

- Inline math `$...$` and display math `$$...$$` via `JKQTMathText`.
- Display-math fenced code blocks routed through `MathRenderer`.
- Mermaid fenced blocks via Rust mmdr FFI.
- Code-block syntax highlighting via `KSyntaxHighlighting` (~300 languages).
- Wikilinks (clickable + hoverable) with `WikiLinkTargetProperty`.
- Hover popover with embedded `Markoff::Reading::ReadingView`.
- Embedded markdown `![[Note]]`/`![[Note#heading]]`/`![[Note#^block]]` with depth cap.
- Embedded images (registered factories for png/jpg/jpeg/gif/svg/webp).
- Search-result highlighting via `ResultHighlighter::drawHighlighted`.
- Footnote definitions extraction + numbering (in parser; rendering is incomplete — see Missing).
- Plugin-extensible code-block processors (`CodeBlockProcessorRegistry`).
- Plugin-extensible embeds (`EmbedRegistry`).

### Parsing

- `getFrontMatterInfo` shape via `Document::frontmatterSpan()` with CRLF tolerance.
- `frontmatterEofClose` detection (closing `---` at EOF without trailing newline).
- `parseYaml` via `YamlValue::parse` with manually re-implemented YAML 1.2 strictness (`yes/no/on/off/y/n` kept as strings).
- `parseLinktext` byte-exact 4-line implementation.
- `stripHeading`/`stripHeadingForLink` regex character-class byte-exact ports.
- `parsePropertyId` Bases column-key parse with unknown-prefix tolerance.
- `tags`/`tag` frontmatter merge in `MetadataParser` (more permissive than Obsidian — see Partial).
- Tree-sitter wikilink/embed extraction.

### Bases

- All 19 spec-named Value types as classes (`NullValue`, `BooleanValue`, `NumberValue`, `StringValue`, `ListValue`, `ObjectValue`, `RegExpValue`, `DateValue`, `RelativeDateValue`, `DurationValue`, `IconValue`, `FileValue`, `UrlValue`, `LinkValue`, `ImageValue`, `HTMLValue`, `MarkdownValue`, `FormulaErrorValue`, `TagValue`).
- Pratt parser (~200 lines) with binding-power table matching addendum precedence.
- Tree-walking evaluator with null-propagation, Date/Duration coercion, hard-cased `if`/`map`/`filter`/`reduce` (preserves shadowing semantics).
- All 15 global builtins except `file()` (which is a stub).
- Per-type method tables (Value/String/Number/Date/List/Object/RegExp/Link/File).
- `ObjectValue::fromFrontMatter` lazy coercion (Wikilink → URL → Date → fallback).
- `BasesEntry` identifier-dispatch with case-insensitive frontmatter fallback.
- Per-entry formula memoisation + cycle detection.
- `BasesQueryResult` sort+limit, group-by, properties union.
- `BasesView` with view selector, search, error banner, sortable/movable headers.
- `QueryController` 50ms-debounced recompute on `cacheChanged`/`cacheDeleted`.
- 13 test executables, ~2000 lines, ~411 assertions.

### Search

- `FuzzyMatcher` complete pipeline (prepareQuery / prepareSimpleSearch / fuzzySearch / sortSearchResults).
- DSL operators: `tag:`, `content:`, `path:` (FTS5 column-qualified), `file:` (mapped to `title:`), phrase, parens, `OR`, `-`/`NOT`, `match-case:` (literals only), `ignore-case:`.
- Exclusive-nesting validation with canonical error messages.
- `tag:` with `#`-prefix tolerance.
- `SearchMatch.matches` populated from FTS5 `snippet()` markup.
- Cmd-O quick switcher uses ported fuzzy matcher.
- Cmd-Shift-F global-search-panel hotkey.

### Settings

- KConfig schema with 14 keys across 5 groups.
- 5-page `KPageDialog` Settings dialog.
- VaultConfig library with generic JSON I/O + `mergeJson` for unknown-key preservation.
- Obsidian-style serialiser (2-space indent, no trailing newline, insertion-order keys) via `serializeObsidianStyle`.
- `core-plugins.json` legacy-array→object migration.
- `Hotkey.h`/`Hotkey.cpp` schema-faithful parser+serialiser.
- `PluginDataStore` for `<pluginDir>/data.json` atomic write.
- Cluster Q `PluginsPage` with permission grants, compat-state surfacing.
- Daily Notes / Templates per-vault override on top of KConfig defaults.
- Round-trip integration test asserting unknown-key preservation.

### UI bundle

- `Component` lifecycle base (with one cleanup-queue ordering divergence).
- `MenuSectionHelper` + canonical section ordering.
- `MenuEventEmitter` + `MenuInjector` plugin connection sugar.
- `HoverLinkSourceRegistry` + built-in source registration.
- `Notice` toast (frame/timer/stack-reflow correct; missing hover-pause + click-dismiss + duration=0 sentinel).
- `QuickSwitcher` with fuzzy filter + match highlighting + basic keyboard nav.
- `CompletionPopup` PopoverSuggest-equivalent for in-editor completion.
- `HoverPopover` with full ReadingView content fidelity (interaction model gaps below).
- `MomentFormatPreview` with live ticking 1s timer.
- `EditorSuggest` abstract + `WikiLinkSuggest` + `TagSuggest`.
- `Platform::openWithDefaultApp` + `Platform::showInFolder` (Linux DBus fast-path).
- `DeleteConfirmDialog`, `RenameDialog`, `MoveFileDialog` modals.
- `MarkdownView::insertFrontmatterProperty` (Add file property menu entry).

### Plugin

- KPluginFactory-based plugin host with full `PluginManager` + `Plugin` + `Component` lifecycle.
- Permission system end-to-end (declare → grant dialog → KConfig persist → proxy enforcement).
- Origin-based trust override (Origin::User cannot self-grant).
- Atomic `data.json` via `QSaveFile`.
- Per-plugin command namespacing with Obsidian "mutate cmd.id in place" quirk preserved.
- Per-plugin session-state persistence (Corbomite-original).
- `corbomite_add_plugin` CMake helper with TRUSTED gate.
- `X-Corbomite-MinVersion` + `X-Corbomite-ApiLevel` two-axis gating.
- Permission grant dialog with "DECLARES it needs" UX language.
- 9 in-tree built-in plugins.
- Permission-gated proxies for: Vault (R/W/Events), FileManager, Metadata (read), Search (FTS), Workspace, CommandRegistry, ViewRegistry, Menus, SecretStorage, ProcessSpawner, Network.

### Core

- `CorbomiteApp` (process-wide signaller) + `MainWindow` (vault-scoped registry) split.
- `Events` mixin port (snapshot-before-iterate, `tryTrigger` with `singleShot(0, …)` for exception scheduling).
- `Scope` + `ScopeManager` (singleton, global event filter, LIFO stack, top-down dispatch).
- `MomentFormatter` three-pass parser with bracket-escape preservation.
- `SecretStorage` proxy with QtKeychain backing, in-process fallback, permission gating.

---

## Partial / divergent

Ordered by user impact.

- **Frontmatter key reordering** — vault format risk #1. (`FileManager.cpp:138-143`)
- **`.base` YAML emitter alphabetises keys** — vault format risk #2. (`BasesQuery.cpp:108-127`)
- **Two `.obsidian/*.json` writers** — vault format risk #3. (`Vault.cpp:454-457` vs `VaultConfig.cpp:19-48`)
- **Workspace.json round-trip is lossy in 5+ ways** — vault format risk #8. (`Workspace.cpp:649-713`)
- **`FileManager::renameFile` link rewriting** is wikilink-only and basename-only. Does not handle markdown-style links, full-path forms, alias whitespace, frontmatter-string-embedded refs. (`FileManager.cpp:148-216`)
- **`MetadataCache::drainOnePath` only re-resolves `cache.links`** — embeds and frontmatterLinks stay in pre-resolve raw-target state until the source file is itself re-parsed. (`MetadataCache.cpp:368-393`)
- **`SQLiteIndex` does not write `frontmatterLinks` rows** — SQL backlinks miss every link declared in `related: "[[Foo]]"` etc. Two call paths (`MetadataCacheReader::backlinksFor` proxy walks `frontmatterLinks`; `SQLiteIndex` does not) return divergent results.
- **No `unresolvedLinks`/`resolvedLinks` reverse maps** — backlinks computed via O(N) walk in proxy or O(1) SQL, but no map for italicising broken links live or auto-relinking on target creation.
- **Tag aggregation lacks subtag-prefix counts** — `getTags()` returns flat list, not `Record<tag, count>` with subtag decomposition. Tag autocomplete under-reports.
- **`processFrontMatter` cannot strip the frontmatter block** — empty mutator leaves a `---/---` shell where Obsidian deletes the block.
- **Footnote def offset shift** — `Document::fromMarkdown` strips `[^N]:` definitions before tree-sitter parse but `MetadataParser` only compensates for the frontmatter shift.
- **`extractSubpath` heading match doesn't call `stripHeading`** — `#bold` won't match `## **bold**`.
- **`extractSubpath` block-id mode does substring matching** — false positives on `^id` text inside code spans.
- **YAML 1.2 strictness in ryml-emitted output not byte-pinned** — no test asserts byte equality of emit; ryml version bump could silently change wire format.
- **`addAction` orders left-to-right (append) vs Obsidian's right-to-left (prepend)** — plugins crossing ecosystems get reversed visual action-bar order.
- **`onPaneMenu(menu, source)` source argument underused** — default forwards to zero-arg overload.
- **`getEphemeralState`/`setEphemeralState` are stubs** on `MarkdownView` — blocks `openLinkText` scroll-to-heading and rename-on-create.
- **Inline rename via `contentEditable` not implemented** — `m_titleEdit` is declared on `EditableFileView` but never instantiated; `startRename()` delegates to host modal.
- **`renderBreadcrumbs()` not on `FileView`** — no clickable parent-folder spans.
- **Tab context menu pinned-tab exclusion missing** — silently closes pinned tabs.
- **`syncState`/`receiveSyncState` (stacked-group sync) absent** — linked-tabs feature non-functional.
- **`canAcceptExtension` reuse path is dead code** — every file open is close-and-reopen.
- **Built-in view set: 4 of Obsidian's 6+2** — pdf/image/audio/video missing.
- **Live editor `cursorColumn()` is 1-based; Source `CursorPos.column` is 0-based** — silent off-by-one for cross-leaf comparisons (`Editor.cpp:1590-1595` vs `SourceEditor.cpp:257`).
- **`EditorSuggest::getSuggestions` is synchronous-only** — no async (`QFuture`/`QPromise`) form.
- **`EditorSuggest.updatePosition` uses cursor only, not trigger range** — popup never reflows; bidi RTL gets wrong direction.
- **Mode-swap save** — does not `await save()` before swapping mode (in-memory shared buffer saves the data; cross-pane `quick-preview` semantics differ).
- **Reading mode `setCursorLine` is a hard-coded no-op** — subpath link navigation into Reading mode silently fails.
- **Live-preview lacks per-block cursor-reveal granularity** — substitutions are unconditional, applied at parse-time by `MarkdownHighlighter`, no leave-block-replaces-text-with-rendered-sub-widget gating.
- **Checkbox round-trip in Reading missing entirely; in Live `toggleCheckbox()` is broken by U+FFFC substitution layer**.
- **Search adapters are stubs in Reading** — `ReadingSearchAdapter::cursorSourceOffset` returns 0; `highlightMatches`/`clearMatchHighlight`/`scrollMatchIntoView` all comments noting "Phase A stub".
- **Heading-collapse does not detach hidden sections** — `recomputeFoldVisibility` sets `setHidden(true)` but `QGraphicsItem`s remain in scene.
- **`setFoldedHeadingLines` does not invalidate folds when line count has changed** — restored against wrong target after external edit adds lines.
- **JKQTMathText AMS coverage below MathJax** — `\begin{align}`, `\binom`, `\substack`, `\boxed`, `\xrightarrow`, `\newcommand`/`\renewcommand`, font stack switching all partial-or-missing.
- **Mermaid renders as light theme on dark themes** — `MermaidRenderer::renderSvg(source)` takes only the source string.
- **`CodeBlockHighlighter` overrides not applied** — Markoff theme `CodeKeyword`/`String`/`Comment` element styles silently ignored.
- **`SearchView` sort/filter/collapse-state/multi-snippets all missing** — fixed BM25, hard-`expandAll()`, one snippet per file.
- **`SearchView` ranking is BM25 not fuzzy `xy`** — operator-free queries don't match Obsidian's ordering.
- **Click-to-jump opens file but doesn't seek to snippet position**.
- **`["property name"]:value` parses to a no-op without `unsupported` notice** — silently misleading.
- **`whole-word:` operator missing from operator table**.
- **`line:`/`block:`/`section:`/`task:` parsed as `unsupported`** — no markdown-AST post-filter.
- **Quick-switcher missing alias-from-frontmatter matching** — only basenames match.
- **Quick-switcher no non-markdown file results** — markdown-only.
- **Command palette uses KCommandBar not the ported fuzzy matcher** — different ranking than every other surface.
- **`KCommandBar` action grouping ad-hoc by `objectName()` prefix** — misses Edit/Format/Tab/Window groupings.
- **No recent-command bias or search history in palette**.
- **`appearance.json` `theme` value vocabulary mismatch** — vault format risk #13.
- **`templates.json` `date_format`/`time_format` not written back** — only `folder` round-trips.
- **`daily-notes.json` `autorun` preserved but unconsumed**.
- **`PluginDataStore` 4-space indent + trailing newline** — vault format risk #14.
- **PluginsPage toggling does not write `core-plugins.json`/`community-plugins.json`** — vault format risk #12.
- **Settings dialog pages constructed once, not rebuilt on tab switch** — Obsidian's `display()`-on-open contract violated.
- **`Component` cleanup queue ordering** — three separate vectors (intervals/connections/cleanups) fire in fixed order vs Obsidian's single LIFO queue.
- **`MenuSectionHelper::canonicalSectionOrder()` missing `title`/`action-primary`/`order`/`ribbon`** — items set on these sections silently land in unset bucket.
- **`Notice(text, 0)` immediately closes** instead of disabling auto-hide.
- **Notice hover-pause + 1s grace + click-anywhere-dismiss** not wired.
- **HoverPopover Mod-key pinning + child-popover chains + `elementFromPoint` poll + anchor-to-mouse for tall targets** all not implemented.
- **`HoverLinkSourceRegistry` is write-only** — `defaultMod` ignored when deciding whether to fire popover.
- **`SuggestModal<T>` abstract base missing** — every picker re-implements scaffolding.
- **`SuggestModal` keyboard nav** — Up/Down/Enter/Esc only, no PgUp/PgDn/Home/End/Ctrl-P/Ctrl-N.
- **`AbstractInputSuggest<T>` input-anchored suggester missing** — only editor-anchored CompletionPopup exists.
- **`Modal` base class missing** — Esc/Enter scope, dim-bg, builder facade.
- **`Plugin::createView` returns `QObject*`** — should be `QWidget*` (currently does `reinterpret_cast<QWidget*>(mainWindow)` in plugins).
- **Six host-side registries with no plugin proxy** — `RibbonToolBar`, `HoverLinkSourceRegistry`, `EditorSuggestManager`, `PostProcessorRegistry`, `EmbedRegistry`, `CodeBlockProcessorRegistry`.
- **`Command::hotkeys` field missing** — `addCommand` cannot carry default keybindings.
- **`SecretStorage::listSecrets` only enumerates in-process fallback** — keyring backend not enumerated.
- **No `SecretStorage::isAvailable()`**.
- **Missing internal plugins (8)** — command-palette, switcher, daily-notes, templates, page-preview, word-count, file-recovery, note-composer.
- **`LinkValue::asFile()` and `file()` global return null** unconditionally — Vault binding never plumbed through to evaluator.
- **`LinkValue.looseEquals` and `linksTo` use string equality** instead of TFile resolution.
- **`MarkdownValue` unreachable** — no `markdown(...)` constructor builtin.
- **`HTMLValue.renderTo` missing** — currently safe-by-omission since cell delegate doesn't render HTML.
- **`ListValue::sort()` puts nulls first not last** (`ListValue.cpp:108-109`).
- **`TagValue::tagMatches` is bidirectional** vs audit one-direction spec.
- **`file.tags.contains()` uses string-strict equality** — `file.tags.contains("#parent")` won't match `#parent/child`.
- **`unrecognizedData` truncates non-scalars** in `BasesQuery::fromString` — forward-compat invariant violated for nested YAML.
- **`PropertyConfig.unrecognizedData` declared but unused**.
- **`limit: 0` dropped on save** even though spec invariant says `limit: 0 == unlimited`.
- **Bases sort cycling is single-key** — UI replaces sort, no shift-click multi-key (data model supports it).
- **Bases group-by has no UI rendering** — data model computes groups, rows render flat.
- **Bases search scans only `m_cfg->order`** — non-visible columns missed.
- **Bases sidebar `this` binding never updates** — `QueryController::setCurrentFile` is plumbed but `BasesView` has no workspace `file-open` subscription.
- **Cluster G save-failure backup writes `.md` files in `.obsidian/file-recovery/`** instead of Obsidian's single-file `file-recovery.json` schema.
- **Daily Notes `autorun` ignored**.
- **5 of 7 Bookmarks commands are palette stubs** with `checkCallback → false` — `WorkspaceController` lacks accessors.
- **MomentFormatter missing tokens** — `Y`, `Q`, `gg`/`gggg`, `e`/`E`, `k`/`kk`, `Z`/`ZZ`, locale shortcuts (`L`/`LL`/`LT`/etc.).
- **No Platform feature-flag accessors** — `isDesktop`/`canExportPdf`/`canPopoutWindow`/`canStackTabs`/`supportsIndexedDb`.

---

## Missing

Grouped, ordered by user impact. **Structurally missing** = would require a new module; **stub present, behavior absent** = registry exists, plugin facade or behavior missing.

### Structurally missing

**Vault**
- `raw` event (plugin-data external-edit observation impossible)
- `config-changed` event
- `Vault::getConfig`/`setConfig` (no in-memory config object on Vault)
- `Vault::getResourcePath` (`app://local/<path>?<mtime>` URL generator)
- `Vault::getAvailablePath`/`getAvailablePathForAttachments`
- `Vault::getAbstractFileByPathInsensitive`
- `FileManager::registerFileParentCreator`
- `FileManager::storeTextFileBackup`/`notifyForBulkUndo`
- `FileManager::downloadAttachmentsForNote` (remote URL localisation)
- BOM strip on read
- `Vault.process` no-op-when-text-equal optimisation

**Metadata**
- `getFirstLinkpathDest`/`getLinkpathDest` as public method on `MetadataCache` (the algorithm exists in `LinkResolver::resolve` but no `MetadataCache` exposure for plugins)
- `getLinkSuggestions`
- `fileToLinktext`
- `getAllPropertyInfos`/`getFrontmatterPropertyValuesForKey`
- `BacklinkSet` shape with positions
- `iterateRefs`/`iterateCacheRefs` helpers
- `onCleanCache(fn)` settle-then-snapshot primitive
- `isUnresolved(linktext, sourcePath)` query
- `aliases` extraction by `MetadataParser` — search/Quick-Switcher integration blind to alias frontmatter

**Workspace**
- `Workspace.activeEditor` getter/setter
- `fileOpened`/`file-open` event with diff-from-lastActiveFile filter
- `quick-preview` event
- `swipe` event
- `registerObsidianProtocolHandler` (`obsidian://`/`corbomite://` URL routing)
- `registerEditorExtension`
- `Workspace.editorExtensions` flat list
- `WorkspaceWindow` popout containing nested splits
- `WorkspaceLeaf.openFile(file, opts?)` with viewType resolution + `openWithDefaultApp` fallback
- `Workspace.handleLinkContextMenu`/`handleExternalLinkContextMenu`
- `Workspace.duplicateLeaf` with mode parameter
- Linked-pane group propagation on history navigation
- Vault-rename → leaf-history rewriting
- `ViewRegistry.on("view-registered")` → leaf rebuild
- `Mod+1..8`/`Mod+9` jump-to-Nth/last tab commands
- `workspace:show-trash`, `workspace:export-pdf`, `workspace:toggle-stacked-tabs` commands
- `apiVersion`/`requireApiVersion`
- `debounce(fn, delay, immediate)` central util
- `stripHeading`/`stripHeadingForLink`/`resolveSubpath` shared library helpers (per leaf-utilities audit; partial — `LinkUtils.cpp` has them but only used in some places)
- `Keymap.isModEvent` analogue

**Views**
- `renderBreadcrumbs()` on `FileView`
- Inline `contentEditable` title-bar rename on `EditableFileView`
- `setEphemeralState({rename, focus, focusMetadata, scroll, line, match, subpath})` consumers
- Ephemeral-state plumbing through `Workspace::openLinkText`
- `onHeaderMenu` hook
- `canDropAnywhere` flag + `handleDrop` delegation
- `handleCut`/`handleCopy`/`handlePaste` hooks
- Pinned-tab exclusion in default tab-menu close items
- `syncState`/`receiveSyncState` for stacked-group linked tabs
- Per-view `vault.on('rename')`/`on('delete')` subscriptions
- `SetStateResult` out-param
- Distinct unknown-view-type pane (folded into EmptyView)
- Built-in `image`/`audio`/`video`/`pdf` view types

**Editor**
- `getLine(n)`, `lineCount()`, `lastLine()`, `firstLine()`
- `getRange(from, to)`, `replaceRange(text, from, to?)`
- `setLine(n, text)`
- `getSelection()`, `replaceSelection(text)`
- `listSelections()`, `setSelection(...)`, `somethingSelected()`
- `posAtCoords(x, y)`, `posAtMouse(MouseEvent)`
- `wordAt(pos)`
- `exec(commandName: string)`
- `Editor.transaction({changes, selection, userEvent})`
- `processLines(matcher, mutator)` helper
- `Editor.insertText(text)` end-of-doc-append IME fast-path
- `expandText()` general-purpose post-IME-commit hook
- `Editor.newlineAndIndentContinueMarkdownList` — partial only; full list-aware smart-Enter state machine not audited
- Triple-click line-extend (Obsidian's `K$` mouseStyle)
- `editorEditorField`/`editorInfoField`/`editorViewField` plugin handles
- `editorLivePreviewField`-equivalent boolean signal
- CodeMirror-style extension surface (StateField/ViewPlugin/Decoration/Facet/keymap)
- Spell-checking (no Sonnet integration)
- Vim mode
- Paste-as-Markdown (HTML→MD via Turndown)
- ~120 of CM6's ~150 commands

**Editor-markdown / Rendering**
- Mode-button on action bar with Mod+click split
- `Mod+A` notice + `Mod+C` copy-all-source in Reading mode
- Touch double-tap toggle
- `Ctrl+Wheel` zoom of base font with persisted `baseFontSize`
- `strictLineBreaks` global config
- Live preview progressive section pipeline (only Reading uses it; Live rebuilds full scene on every parseUpdated)
- Section recycling pool integration with live editor scene
- Selection-preserved-across-virtualisation
- `MarkdownRenderer.render(app, markdown, el, sourcePath, component)` static API — **highest-leverage missing item**
- `MarkdownRenderChild` framework around the type (parent registry, addChild, mount/unmount signals)
- `docId = cc(16)` per-render plugin-state key
- Unload-on-fold-of-ancestor-heading
- Checkbox-click-to-toggle markdown round-trip
- Footnote-link click → scroll-to-target with flash highlight
- Scroll-sync between linked panes
- Cross-pane `quick-preview` event
- Inline-title rename at top of document
- `markdown-viewport-menu` event
- Print-to-PDF link `href` strip
- Fold-info invalidation on line-count change

**Rendering specifically**
- PDF view + `![[file.pdf]]` embed render — **biggest single rendering-domain capability gap**
- `htmlToMarkdown` (Turndown) for paste-from-browser
- `sanitizeHTMLToDom` (DOMPurify) for plugin-supplied HTML
- `displayTooltip` rich-tooltip primitive
- **Callouts** (`> [!note]` / `[!warning]` / etc.) — parser doesn't split blockquotes by callout-type prefix; `Theme::calloutColor` exists but no rendering pipeline consumes it. **Major visible gap.**
- **Footnote hover tooltip + jump-link** — superscripts go nowhere; definitions silently dropped
- Centralised inline-primitive renderer (`RenderContext` analogue)
- Bidi inline isolates (per-inline isolate spans)
- Pre-warming of math/mermaid on cold start

**Parsing**
- `parseFrontMatterEntry`/`parseFrontMatterStringArray`/`parseFrontMatterTags`/`parseFrontMatterAliases` standalone helpers (each consumer rebuilds the logic, with divergences)

**Bases**
- `file()` global and `LinkValue.asFile()` need Vault binding to FunctionRegistry
- Plugin-side `registerGlobalFunc`/`registerInstanceFunc`
- Source-position-bearing AST for formula errors
- Custom summaries via `summaries:` formula references (parsed but not evaluated)
- 6 of 15 default summary formulas (Range/Earliest/Latest/Checked/Unchecked/Empty/Filled)
- Properties / sort+group / views / results menus (the entire BasesView toolbar suite)
- Group-rendering in the table
- Multi-key sort cycle in header click handler
- Export (CSV/TSV/Markdown/`obsidian/table` MIME)
- "+ New" button (`newItemFolder`/`newItemTemplate` round-trip but unused)
- Drag, context menu, hover-link, per-view undo/redo on cells
- View-rename auto-rewrite of `[[basefile#viewname]]` references
- Rich rendering for `Image`/`HTML`/`Markdown`/`Icon` cells
- Plugin-API surfaces (`registerGlobalFunc`, `registerInstanceFunc`, `BasesPluginInstance.registerView`)
- Round-trip test against an Obsidian-authored `.base` file
- Fuzz/property test on the parser

**Search**
- `whole-word:` operator
- `line:`/`block:`/`section:`/`task:` semantics (parser surfaces but no AST post-filter)
- Frontmatter property queries (`["property name"]:value`)
- Streaming results to panel
- Scope filters in panel UX
- Sort menu (relevance/file name/modified/created)
- Collapsible per-file groups with persisted state
- Multiple snippets per file
- In-title vs in-content toggle
- Search history persistence
- Quick-switcher alias matching
- Quick-switcher non-markdown file results
- Command palette using ported fuzzy matcher
- `renderMatches`/`renderResults`-equivalent shared chokepoint for plugins

**Settings**
- `app.json` write-side entirely
- `app.json` read-side except `userIgnoreFilters`
- `appearance.json` read-side entirely
- Hotkeys round-trip to `hotkeys.json` (parser/serialiser exist but dead code; `KSharedConfig` is the only live path)
- Plugin enable-state round-trip to `core-plugins.json`/`community-plugins.json`
- About page
- Hotkeys page (no `KShortcutsDialog` invoke)
- Plugin runtime settings-tab API (plugins cannot contribute a settings page through `Plugin::configPages` because PluginsPage doesn't surface them)
- `graph.json`, `canvas.json`, `zk-prefixer.json`, `file-recovery.json`, `workspaces.json`
- External-edit watchers for `hotkeys.json` and per-plugin `data.json`

**UI bundle**
- Lucide icon registry / translation map (`Plugin.addIcon(name, svg)`); persisted `lucide-file` icon strings render blank
- MergeFile modal (per addendum, deferred)
- `SecretComponent` widget (keychain backing exists; picker UI doesn't)
- `Modal` base class
- `PopoverState` enum
- `addItem`-after-load Modal handling
- `MenuItem.setWarning` red-text styling
- SliderComponent dynamic-tooltip
- HoverLinkSourceRegistry consumption (currently write-only)
- Per-OS label adaptation ("Show in Finder/Explorer/system explorer")
- Notice-on-`openWithDefaultApp` failure

**Plugin**
- JS shim — every plugin must be C++; existing Obsidian plugin ecosystem cannot load
- Status bar items (`addStatusBarItem`)
- Markdown code-block processors (`registerMarkdownCodeBlockProcessor` plugin facade — registry exists)
- Editor extensions (Markoff equivalent of `registerEditorExtension`)
- Protocol handlers (`obsidian://`/`corbomite://`)
- `onUserEnable()` lifecycle hook
- `onExternalSettingsChange()` + `data.json` mtime watcher
- `addRibbonIcon` plugin proxy (host-side exists)
- `registerHoverLinkSource` plugin proxy
- `registerEditorSuggest` plugin proxy
- `registerMarkdownPostProcessor` plugin proxy
- Plugin-side `Events` mixin (plugins use Qt signals direct)
- `MarkdownRenderChild` post-processor lifecycle wrapper
- Modal/SuggestModal/FuzzySuggestModal exposed to plugins
- Plugin-installable icon registry
- `requireApiVersion()`/`apiVersion` runtime exports
- Community plugin browser/installer UI (no KNewStuff integration)
- Per-vault plugin install dir (system-wide install only)
- Auto-unload on `onLoad` throw
- Detach-leaves-of-type on plugin disable
- Internal plugins: command-palette, switcher, daily-notes, templates, word-count, file-recovery, page-preview, note-composer

**Core**
- `dragManager` (plugins cannot coordinate drag MIME types)
- `customCss`
- `renderContext`
- `appId` (per-vault process namespace; downstream consequences for QtKeychain scoping)
- `quit` event with `Eb`-equivalent collector for plugin async cleanup
- `css-change` event (no bridge from `ThemeService::themeChanged` to workspace event)
- File Recovery (`.obsidian/file-recovery.json` JSON store with retention policy)
- `Scope.setTabFocusContainerEl` (modal tab-trap)
- `off(name, fn)` (intentional fence — `offref` only)

---

## Suspected bugs (concrete, file-cited)

Grouped by domain. Each entry: symptom + path:line of root cause + fix sketch.

### Vault

1. **Folder rename loses descendants.** `Vault::rename` (`libs/vault/src/Vault.cpp:337-373`) doesn't recurse — descendant `m_fileMap` keys go stale. **Fix:** recursive rekey + emit `renamed` per descendant.

2. **`processFrontMatter` re-sorts keys alphabetically.** `FileManager::processFrontMatter` (`libs/vault/src/FileManager.cpp:138-143`) round-trips through `QVariantMap` (= sorted `QMap`). **Fix:** order-preserving wrapper or direct YamlValue mutation.

3. **`Vault::create` does not collision-check case-insensitively.** `Vault.cpp:256` exact-match only — on macOS/Windows `Note.md` and `note.md` collide on disk. `CaseSensitivityProbe` exists at `CaseSensitivityProbe.h:19` but is dead code. **Fix:** case-insensitive scan or case-fold key on insert.

4. **`Vault::onExternalCreated` parent-folder inference races.** `Vault.cpp:679-683` parents to root if parent doesn't yet exist in `m_fileMap`. **Fix:** build intermediate folders eagerly.

5. **`FileManager::trashFile` ignores user `[Files]/TrashOption`.** `FileManager.cpp:325` always calls `m_vault->trash(f, false)`. The dialog-routed `promptForDeletion` reads the same setting. **Fix:** route both through same KConfig read.

6. **Watcher excludes `.obsidian/` from observation.** `Watcher.cpp:28-39` filters out `.obsidian/`, `.corbomite/`, `.trash/`, `.git/`. Prevents `config-changed` reload + `raw` event for plugin data. **Fix:** at minimum, watch `.obsidian/`.

7. **`writeConfigJson` rejects primitive top-level JSON.** `Vault.cpp:454-457` returns false for `true`, `42`, `"string"`. **Fix:** accept any JSON-stringifiable value.

8. **Read does not strip BOM.** `Vault::read` returns raw bytes (`Vault.cpp:151-156`). **Fix:** strip leading `\xef\xbb\xbf` on read.

### Metadata

1. **`SQLiteIndex` does not write `frontmatterLinks` rows.** `writeRowsFromCache` indexes only `cache.links` and `cache.embeds` (`SQLiteIndex.cpp:256-294`). SQL backlinks miss every link in `related: "[[Foo]]"`. **Fix:** iterate `cache.frontmatterLinks` in row writer.

2. **`SQLiteIndex::writeRowsFromCache` re-reads file from disk on every cache-change.** `SQLiteIndex.cpp:209-216` reads bytes for FTS `content` column even though `MetadataWorker::parsed` already carries them. **Fix:** thread bytes through `cacheChanged` signal.

3. **`drainOnePath` only re-resolves `cache.links`.** `MetadataCache.cpp:368-393` doesn't touch embeds or frontmatterLinks. **Fix:** include all link kinds in drain.

4. **`collectFrontmatterLinks` grabs only first wikilink/md-link per string leaf.** `MetadataParser.cpp:213-249` calls `wikiRe.match(s)` and returns on first hit. **Fix:** iterate all matches.

5. **`LinkResolver` step 4 dot-relative path with extension is wrong.** `LinkResolver.cpp:139-153` does `if (!resolved.contains('.'))` — fails for folders named `2026.04`. **Fix:** check the original linktext for extension, not the resolved-folder concatenation.

6. **`LinkResolver` rooted-`/` path with extension same bug** (`LinkResolver.cpp:120-130`).

7. **Footnote definitions regex misses indented continuation.** `MetadataParser.cpp:485-500` captures only first line; multi-line definition bodies lost.

8. **Block-anchor regex matches anywhere `^id` appears at end-of-line** (`MetadataParser.cpp:548-550`). Doesn't exclude code blocks or YAML strings.

9. **`MetadataCache::drainOnePath` resolved cache rewrite is destructive.** Mutates `*cacheIt` directly; `m_hashToCache` is keyed by content hash and shared across paths via dedup. Two paths sharing hash but different folder contexts: resolution depends on which path drains last.

10. **`MainWindow.cpp:2033-2035` connects `indexFinished` after every vault load without disconnect** — multiple vault-open cycles show status message multiple times per index-finish.

### Workspace

1. **`m_tabGroupOf` lags user drags** — KDDW exposes no Group enumeration API. `nextLeafInActiveGroup`/`closeOtherLeavesInGroupOf`/`serialize()` all see stale grouping after drag.

2. **Two competing serializers** — `Workspace::serialize/deserialize` (`Workspace.cpp:649-852`) and `WorkspaceSerializer::toJson/fromJson` (`WorkspaceSerializer.cpp:407-477`) walk different sources of truth and emit different defaults. Phase 5/6 transition unfinished. **Fix:** declare one authoritative; delete or merge the other.

3. **`SessionManager` blind write-through of Obsidian's `left`/`right`/`floating` subtrees** — `SessionManager.cpp:85-91` re-serialises blindly while Corbomite sidebars are managed by `CorbomiteMDI::Sidebar` outside the persisted subtree. Subtle data-loss.

4. **`undoCloseLeaf` does not restore container/parent** (`Workspace.cpp:329-346`). `parentId`/`rootId` populated but never consumed. Closing in right pane → undo on left pane.

5. **`undoCloseLeaf` does not restore `leafHistory` or `eState`** — captured in `UndoEntry` but discarded.

6. **Popout window cleanup leak.** `popoutLeaf` (`Workspace.cpp:501-505`) connects `&QObject::destroyed` to emit `windowFrameChange`, but never removes from `m_windows`/`m_floating` on X-close. Permanent overstatement of popout count.

7. **`Workspace::deserialize` deletes leaves while KDDW MainWindow is alive** (`Workspace.cpp:758`) without `releaseDockWidget()` first — sporadic crash risk on vault reload.

8. **`Workspace::resize()` not debounced** — every Resize event fires every connected slot; continuous drag = jank.

9. **`activeLeafChanged` not debounced** — N events on flicker close where Obsidian emits 1.

10. **`WorkspaceLeaf::setViewState` emits `viewChanged` twice** (once with empty viewState, once after apply). Consumers reading title/icon on first emission see stale values.

### Views

1. **`writeBackup` writes plain markdown inside vault** (`TextFileView.cpp:140` → `<vaultRoot>/.obsidian/file-recovery/<name>-<ts>.md`). File shows up in tree, search, graph, triggers another `Vault::modified` for the failing leaf. **Fix:** write outside vault or migrate to JSON store.

2. **Title not refreshed on external rename.** No path re-emits `displayTextChanged` from `Vault::renamed`.

3. **Open file deleted externally — leaf becomes orphaned.** No reaction to `Vault::deletedFile` for open file; subsequent save silently re-creates.

4. **`FileView::setState` swallows missing-file silently** (`FileView.cpp:60, 67`). No `n.close = true` equivalent. Restored session leaves zombie tabs.

5. **`addAction` appends rather than prepends** (`ItemView.cpp:87`). Plugins crossing ecosystems get reversed action-bar order.

### Editor

1. **CJK autocorrect cursor desync on focused-item edits.** Documented TODO at `TextControl.cpp:1683-1700` (`qCWarning markoff.live.text_control.cursor_drift`). Real bug.

2. **`detectCompletionTriggers` runs in parallel with `EditorSuggestManager`.** Two trigger paths (`Editor.cpp:1681-1707` emits `wikiLinkTrigger`/`tagTrigger` to nowhere; `NoteEditorWidget` dispatches through manager). **Fix:** delete `detectCompletionTriggers` and its signals.

3. **`completionDismissHint` signal exists but emitted from nowhere I could find via grep** (`Editor.h:322` wired at `NoteEditorWidget.cpp:71`).

4. **`absoluteCursorPos()` does O(N) line-scan on every cursor change** (`NoteEditorWidget.cpp:533-545`). **Fix:** add `Editor::absoluteCursorPosition()` accessor.

5. **`Markoff::Editor::insertAtCursor` does not call `MarkdownDelta`** (`Editor.cpp:1276-1281`). Mutates `QTextDocument` directly; reaches canonical buffer only via SceneCoordinator listener. Brittle: missed listener wiring → silent edit loss on canonical undo stack.

### Editor-markdown

1. **Reading mode `setCursorLine` is hard-coded false** (`NoteEditorWidget.cpp:259-261`). Subpath link navigation into Reading mode silently fails.

2. **Live-preview off-by-one on column round-trip.** `NoteEditorWidget.cpp:289-292` subtracts 1 from line and column on save; `:323-324` adds 1 back to line only.

3. **`MarkdownView::getViewData` reads `noteDocument()->markdown()` directly** (`MarkdownView.cpp:54-59`). For Source mode in-progress IME composition that hasn't propagated, returns stale data.

4. **`setViewData(text, true)` drops the `clear` flag** — `Q_UNUSED` at `MarkdownView.cpp:66`. Should null scroll, doesn't.

5. **`getEphemeralState` is a TODO** — `Corbomite::MarkdownView::getEphemeralState` returns `{}`. ItemView-layer eState round-trip broken.

6. **`recomputeFoldVisibility` does not detach hidden sections from scene** (`ReadingView.cpp:638-671`). Fragile invariant — direct mutation of `headingCollapsed` bypassing `setFoldedHeadingLines`/`toggleFold` leaves detached-but-visible items.

7. **`setFoldedHeadingLines` does not invalidate folds when line count changes.**

8. **No `markdown-scroll`/`quick-preview` workspace events.** Cross-pane scroll-sync absent.

### Rendering

1. **Footnotes are functionally dead** — numbered superscripts go nowhere; definitions silently dropped (`Document.cpp:94-95`).

2. **Mermaid renders as light theme on dark themes** — `MermaidRenderer::renderSvg` takes only source string, no theme parameter.

3. **Math always wraps in `$...$`** (`MathRenderer.cpp:56-58`). Display-mode gets only font-size boost, not parser semantics.

4. **mmdr is uncached + sync** — every paint of mermaid block re-runs Rust SVG generation if section recycled out and back.

5. **Paste-HTML drops all formatting silently** — `TextControl::insertFromMimeData` checks `hasText()` only.

6. **`registerBuiltinCodeBlockProcessors` registers a `default` lang stub** (`ReadingView.cpp:256-264`) — body just constructs `CodeBlockHighlighter` and does nothing. `hasLanguage("default")` returns true, surprising plugins.

7. **`Theme::calloutColor` exists in JSON, no callout consumer** — silent dead-letter for theme authors.

8. **`linkHovered` 300ms delay hard-coded as two independent constants** (`ReadingView.cpp:125`, `HoverPopover.cpp:19`).

### Parsing

1. **`processFrontMatter` reorders keys alphabetically** — vault format risk #1 + this is the symptom.

2. **`processFrontMatter` cannot remove emptied frontmatter block** (`Document.cpp:397-409`).

3. **Two different subpath resolvers with different rules.** `Markoff::Document::extractSubpath` (hyphen-to-space + lowercase) vs `Corbomite::resolveSubpath` (`stripHeading` + lowercase). Inconsistent results between hovering an embed and clicking a backlink.

4. **`MetadataParser` accepts singular `tag:`** Obsidian doesn't (`MetadataParser.cpp:451-460`).

5. **`MetadataParser` interprets `tags: alpha, beta` as one bogus tag** (`MetadataParser.cpp:445-449`).

6. **No `aliases` extraction by `MetadataParser`.** Search/Quick-Switcher integration blind.

7. **`extractSubpath` block-id mode does substring matching** (`Document.cpp:292`) — false positives in code spans.

8. **Footnote-def offset shift latent bug** — addendum 2026-04-15.

9. **`YamlValue::stringify` byte format not pinned by any test** — ryml version bump could silently change wire format.

### Bases

1. **YAML key-order alphabetisation on write** (`BasesQuery.cpp:111-127`).

2. **`unrecognizedData` truncates non-scalars** (`BasesQuery.cpp:209-218`).

3. **`ListValue::sort()` puts nulls first not last** (`ListValue.cpp:108-109`).

4. **`Formula` copy constructor re-parses unnecessarily** (`Formula.cpp:21-32`) — `m_ast` is `shared_ptr` precisely to avoid this.

5. **`TagValue::tagMatches` is bidirectional** vs audit one-direction spec.

6. **`file.tags.contains()` is string-strict** — no `TagListValue` analog.

7. **`addToDate` order of operations** for negative durations may drift at month boundaries.

8. **`BasesView` lacks workspace `file-open`/`layout-change` subscriptions** — `QueryController::setCurrentFile` plumbed but never called. `this` always resolves to `.base` itself.

9. **`NumberValue::toString` returns `∞` for both `+∞` and `-∞`** — sign not distinguished (`NumberValue.cpp:18`).

### Search

1. **Bare `/regex/` query returns no results.** `SQLiteIndex.cpp:397` early-return checks `fts5Query.isEmpty() && requiredTags.isEmpty() && excludedTags.isEmpty()` *not* `postFilter`. **Fix:** add `&& !postFilter` or set `fts5Query = "*"`.

2. **Top-level `-foo` produces invalid FTS5.** `emitFts5` for bare `Not` at root (`SearchDSL.cpp:542-545`) emits `NOT foo` — FTS5 has no prefix `NOT`. `q.exec()` fails silently. **Fix:** rewrite as `* NOT joined-not-parts` or surface as unsupported.

3. **`["property"]:value` silently parses to nothing** without `unsupported` notice (`SearchDSL.cpp:371-384`).

4. **`FuzzyMatch::score` documentation misleading** — header says "higher = better" but actual scores are negative (`FuzzyMatch.h:13`).

5. **`FuzzyFilterProxyModel::lessThan` calls `fuzzySearch` per pairwise comparison** (`QuickSwitcher.cpp:58-59`). O(n log n · |query|·|name|) per keystroke. **Fix:** cache scores in model role.

6. **`QuickSwitcherDelegate::paint` re-runs `fuzzySearch` inside paint loop** (`QuickSwitcherDelegate.cpp:38-39`). Same caching argument.

7. **`tag:` operand keeps `#` in AST node** (`SearchDSL.cpp:482`) even after stripping during compile. Cosmetic.

8. **Streaming/incremental gap with no query-id guard** — second debounce restarts but in-flight query may return stale results that overwrite newer.

9. **`SearchView` does not persist last query**.

10. **`SearchProxy` permission gate returns empty on denied** with only a `qCDebug` log; plugin sees "no results" with no error signal.

### Settings

1. **Two `.obsidian/*.json` writers with different formatters** — vault format risk #3.

2. **`appearance.json` theme value vocabulary mismatch** — vault format risk #13.

3. **`MainWindow::applyVaultPortableSettings` only writes "outbound" keys** — no inbound read at vault open.

4. **`PluginsPage` toggling does not write `core-plugins.json`/`community-plugins.json`** — vault format risk #12.

5. **`HotkeyFile::serialise` open-bracket whitespace inconsistency** — empty-bindings case writes `"id": []` while non-empty case prepends newline before closing `]`.

6. **`PluginDataStore::load` returns empty object on missing file** — Obsidian returns `null`. Plugins porting from Obsidian need to adapt first-run detection.

7. **`SettingsDialog` not modal-singleton-managed** — twice-quickly invocations stack two dialogs.

### UI bundle

1. **`Notice(text, 0)` immediately closes** — `Notice.cpp:48-50` starts a 0-ms timer instead of disabling. **Fix:** `if (durationMs > 0) m_dismissTimer.start(durationMs)`.

2. **`FileExplorerView` bypasses proper rename/delete modals.** F2 → `QInputDialog` instead of `RenameDialog` (`FileExplorerView.cpp:152`); Delete → `QMessageBox::question` instead of `DeleteConfirmDialog` (`:138-145`). "Don't ask again" + trash-option awareness + live validation all silently disabled in file-explorer surface.

3. **`FileExplorerView` context menu has no plugin emission and no section helper** (`FileExplorerView.cpp:166-199`). Plugins cannot inject items.

4. **`MenuSectionHelper::canonicalSectionOrder()` missing `title`/`action-primary`/`order`/`ribbon`** — items silently fall into wrong bucket.

5. **`HoverLinkSourceRegistry` is write-only** — `defaultMod` ignored.

6. **`MarkdownView` and `GraphViewTab` build menus with raw `addAction` calls** — substrate exists, migration incomplete.

7. **`Component` cleanup ordering divergence** — three separate vectors vs single LIFO queue.

8. **`MoveFileDialog` substring match** when `FuzzyMatcher` is in tree.

9. **No icon registry at all** — `lucide-file` etc. render blank.

10. **HoverPopover lacks Mod-key pinning, child-popover chains, `elementFromPoint` poll, anchor-to-mouse** — every other gap is paint; these are *the* hover-link UX.

### Plugin

1. **`onLoad` throw leaks state** (`Plugin.cpp:19-22`). Exception propagates out of `Component::load` leaving `_loaded = true` and cleanups dangling. **Fix:** try/catch + `Component::unload()`.

2. **`disablePlugin` does not detach leaves of plugin-registered view-types.** Open tabs orphaned (factory gone, view alive).

3. **`HoverLinkSourceRegistry`/`PostProcessorRegistry`/`EditorSuggestManager` exist with no plugin facade** — header docstrings say "when plugins land". Plugins have landed; facades haven't.

4. **`Command::hotkeys` field missing.** Plugins porting commands with default keybindings cannot.

5. **`SecretStorage::listSecrets` returns only session-observed keys** (Qt Keychain limitation; documented at `SecretStorage.h:42-49`).

6. **No `Events` mixin on plugin-facing classes** — Obsidian-shape `vault.on("create", cb)` plugin code won't work.

7. **`PluginContext` accessor list incomplete** — no accessors for hover-link/editor-suggest/post-processor registries that exist host-side.

8. **`Plugin::createView` returns `QObject*`** — `reinterpret_cast<QWidget*>(mainWindow)` at `BacklinksPlugin.cpp:54` is a code smell.

9. **No `data.json` watcher** — plugins editing data.json from sister tool don't pick up the change.

10. **Permission tokens are string constants in a `.cpp`** (`PluginContext.cpp:21-32`). Wrong location. **Fix:** export as `PluginPermissions.h`.

### Core

1. **`resolveSubpath` block-id case-sensitivity bug** (`LinkUtils.cpp:121-155`). Vault-format compat. Highest-impact.

2. **`resolveSubpath` multi-segment heading walk not implemented** (`LinkUtils.cpp:170-189`). Self-noted.

3. **Dangling adapter pointers in `onVaultClosed`** (`MainWindow.cpp:2190-2263`). `m_linkResolverAdapter`/`m_metadataCacheAdapter`/`m_metadataParserImpl` not `.reset()`-ed before underlying services deleted. Use-after-free window. **Fix:** add `.reset()` calls before `delete m_metadataCache` / `delete m_linkResolver`.

4. **`css-change` and `quit` events not emitted.** No bridge from `m_themeService::themeChanged` onto workspace event; `closeEvent` runs cleanup directly without `Eb` collector for plugin async cleanup.

5. **Embed registry leaks plugin factories across vault switches.** `m_embedRegistry` built once at MainWindow construction; plugin embed factories registered at `onLoad` survive vault switches with stale captures (latent — no plugin registers embeds today).

6. **Zero actual `Scope` users** — `ScopeManager::pushScope` called from zero call sites outside `ScopeManager.cpp` itself + `main.cpp`. Modal hotkey containment infrastructure exists but nothing uses it.

---

## Cross-cutting themes

The following root causes appear across multiple sub-reports.

### Plugin extension surfaces: registries exist host-side but lack plugin proxies

| Registry | Host class | Plugin proxy | Sub-reports flagging |
|---|---|---|---|
| Hover-link sources | `HoverLinkSourceRegistry` | **MISSING** | workspace, ui-bundle, plugin |
| Editor suggesters | `EditorSuggestManager` | **MISSING** | editor, plugin, ui-bundle |
| Markdown post-processors | `PostProcessorRegistry` | **MISSING** | editor-markdown, plugin, rendering |
| Code-block processors | `CodeBlockProcessorRegistry` | **MISSING** | editor-markdown, plugin, rendering |
| Embeds | `EmbedRegistry` | **MISSING** | core, plugin |
| Ribbon icons | `RibbonToolBar::addRibbonIcon` | **MISSING** | workspace, plugin |

This is **the single most impactful Cluster-N follow-up**. The Cluster N memory entry says "plugin-ready surfaces shipped" — interpret that strictly: surfaces are *shapeable* by plugins, not *reachable*. Six concrete proxy facades + `PluginContext` accessor additions would close this in one pass.

### Round-trip serializer divergences (frontmatter + JSON + .base + workspace.json)

Three file types with key-order or formatting bugs:

| Surface | Bug | Severity |
|---|---|---|
| Frontmatter (`processFrontMatter`) | `QVariantMap` re-sorts keys alphabetically | HIGH |
| `.base` files | hand-rolled emitter `QVariantMap::constBegin()` alphabetises | HIGH |
| `.obsidian/*.json` (plugin-written) | `Vault::writeConfigJson` 4-space indent vs Obsidian's 2-space | HIGH |
| `workspace.json` | `m_unknownRoot` blind write-through, two parallel serializers, nested splits flattened | HIGH |
| `.obsidian/appearance.json` `theme` | wrong value vocabulary (`system`/`light`/`dark` vs `obsidian`/`moonstone`) | MEDIUM |
| Plugin `data.json` | 4-space indent + trailing newline | LOW |
| `core-plugins.json`/`community-plugins.json` | parser exists, PluginManager doesn't write to it | MEDIUM |
| `hotkeys.json` | parser exists, KSharedConfig is the only live write path | MEDIUM |

Pattern: many of these are "the I/O layer is built; nothing wires the read/write to runtime". Two related causes — using `QVariantMap` (sorted) where insertion-order is required, and having parallel writers with one being correct and one being wrong.

### "Internal-only" vs "plugin-facing" doubled APIs

Multiple cases of two parallel implementations or surfaces:

- **Two `.obsidian/*.json` writers** (`VaultConfig::writeJson` vs `Vault::writeConfigJson`).
- **Two workspace serializers** (`Workspace::serialize` vs `WorkspaceSerializer::toJson`).
- **Two `resolveSubpath` implementations** (`Markoff::Document::extractSubpath` vs `Corbomite::resolveSubpath`).
- **Two completion-trigger paths** (`Markoff::Editor::detectCompletionTriggers` emits dead `wikiLinkTrigger` signals while `EditorSuggestManager` actually dispatches).
- **Two backlink computation paths** — `MetadataCacheReader::backlinksFor` walks `frontmatterLinks`; `SQLiteIndex::backlinksFor` does not. Same query returns different results.
- **Two ambient pub-sub systems** (Qt signals + `Corbomite::Events`).
- **Two parsing paths for tags / footnotes / blocks** in `MetadataParser` (regex over body) and tree-sitter (`Document::tags`) — could diverge on edge cases.

Pattern: in-flight transitions or Cluster Q.0 / Cluster G refactors that didn't fully consolidate. Each pair needs one to be declared authoritative and the other deleted or merged.

### Per-vault state isolation

- **Dangling adapter pointers** in `MainWindow::onVaultClosed` (use-after-free window).
- **Embed registry factories** survive vault switches with stale captures.
- **No `appId`** for per-vault QtKeychain scoping.
- **`m_embedRegistry`/`m_themeService`/`m_hoverPopover`/`m_wikiSuggest`/`m_tagSuggest`/`m_suggestManager`/`m_ribbonToolBar`** all built once at MainWindow construction; explicit `setVault(nullptr)` resets present in some but not all.
- **`m_fsAdapter`** never reset (vault-agnostic so OK in theory).
- **No vault-rename → leaf-history rewriting** — Obsidian rewrites `state.state.file` in every leaf history + undo entry on `vault.on("rename")`; Corbomite has no listener.

### Lifecycle / Component-cleanup ordering

- **`Component` cleanup queue** uses three separate vectors (intervals/connections/cleanups) firing in fixed order vs Obsidian's single LIFO queue.
- **`Plugin::onLoad` throws leak state** — no try/catch.
- **`disablePlugin` does not detach leaves of plugin-registered view-types** — orphaned tabs.
- **No `onUserEnable()` lifecycle hook**.
- **No `onExternalSettingsChange()` + `data.json` watcher**.
- **No `quit` event with `Eb` collector** for plugin async cleanup.

### Same gap appears in 3+ sub-reports

- **No Lucide icon registry** — flagged in ui-bundle, plugin, and (implicitly) workspace.
- **No `RenderContext` analogue** — flagged in rendering, editor-markdown, ui-bundle. Needed for cross-surface link-affordance consistency.
- **No `MarkdownRenderer.render()` static API** — flagged in editor-markdown, rendering, plugin. Highest-leverage missing item per editor-markdown audit.
- **No `MarkdownRenderChild` framework around the type** — flagged in editor-markdown, plugin, rendering.
- **No status bar items / no plugin proxies for half the registries** — flagged across plugin, ui-bundle, workspace, editor-markdown.
- **Frontmatter key reordering** — flagged in vault, parsing, settings, bases.
- **`processFrontMatter` cannot strip emptied block** — flagged in parsing, vault.
- **Two parallel writers / serializers / resolvers** — flagged in vault, settings, workspace, parsing, plugin.
- **HoverPopover interaction model gaps** — flagged in ui-bundle, editor-markdown.
- **Reading mode `setCursorLine` no-op** — flagged in editor-markdown, views, workspace.

---

## Recommended priority order

Justification: vault-format risks first because they cause silent data corruption that cross-tool users won't notice until they git-diff. High-traffic UX next. Then extension surfaces, then features.

### P0 — Vault-format silent-corruption fixes

1. **Frontmatter key-order preservation** in `processFrontMatter` (`FileManager.cpp:138-143`). Every metadata edit reshuffles all keys alphabetically.
2. **Consolidate the two `.obsidian/*.json` writers** so `Vault::writeConfigJson` routes through `VaultConfig::serializeObsidianStyle`. Bookmarks plugin currently writes 4-space indent.
3. **`.base` YAML emitter** — same root cause as #1 (sorted `QVariantMap`).
4. **`resolveSubpath` block-id case-sensitivity** (`LinkUtils.cpp:121-155`). Lowercase both sides.
5. **Folder rename descendant rekeying** (`Vault.cpp:337-373`). Recursive walk + per-descendant `renamed` emission.
6. **`FileManager::renameFile` rewrites markdown-style links and full-path forms** (`FileManager.cpp:148-216`). Use the snapshot from `MetadataCache` as source-of-truth (it identifies the *what*; the rewrite is shallower).
7. **`appearance.json` `theme` value vocabulary mismatch** — write `"obsidian"`/`"moonstone"` not `"system"`/`"light"`/`"dark"`.
8. **Wire `core-plugins.json` and `community-plugins.json` to `PluginManager`** so toggle state actually transfers between Corbomite and Obsidian.

### P1 — Workspace.json round-trip fixes

9. **Consolidate `Workspace::serialize` and `WorkspaceSerializer::toJson`** into one writer.
10. **Implement nested-split round-trip** so opening + saving an Obsidian-authored layout doesn't degrade.
11. **Per-group `currentTab`** instead of conflated global active-leaf-index.
12. **Stop blind write-through of `left`/`right` subtree** in `SessionManager` — either match Obsidian's shape or omit entirely.
13. **Popout window cleanup leak** — add `m_windows.removeOne(window)` to the X-close path.

### P2 — High-traffic UX correctness

14. **Reading mode `setCursorLine`** — at minimum scroll to section containing line N; ideally implement subpath nav with ancestor un-fold.
15. **Footnotes** — render definitions, hover-popover for refs, scroll-to-definition on click.
16. **Callouts** — parser+renderer for `> [!note]` / `[!warning]` / etc. The data model exists in `Theme`; just no consumer.
17. **HoverPopover Mod-key pinning + child-popover chains** — the popover renders correctly but the interaction model is half-built. This is *the* hover-link UX.
18. **`Notice(text, 0)` should not auto-close** — one-line fix in `Notice.cpp:48-50`.
19. **FileExplorer modals** — F2/Delete should route through `RenameDialog`/`DeleteConfirmDialog`, not `QInputDialog`/`QMessageBox::question`.
20. **Title not refreshed on external rename** — re-emit `displayTextChanged` from `Vault::renamed`.
21. **Bare `/regex/` and top-level `-foo` queries** — both return zero results due to broken guards in `SearchView::executeSearch` + `SearchDSL::emitFts5`.
22. **Mermaid theme integration** — pass `theme: 'dark'` parameter; dark-mode renders are near-illegible today.
23. **Live-preview off-by-one column round-trip** (`NoteEditorWidget.cpp:289-292` vs `:323-324`).

### P3 — Plugin extension surface

24. **`MarkdownRenderer.render(...)` static API** — highest-leverage missing item per editor-markdown audit. Hover popovers, search snippets, plugin tooltips all need it.
25. **`MarkdownRenderChild` framework** — parent registry, addChild, mount/unmount signals.
26. **Plugin proxies for `HoverLinkSourceRegistry`, `EditorSuggestManager`, `PostProcessorRegistry`, `RibbonToolBar`, `EmbedRegistry`, `CodeBlockProcessorRegistry`** — six proxies in one pass.
27. **`addStatusBarItem`** — required for any plugin that wants persistent UI.
28. **Lucide icon registry + `Plugin.addIcon(name, svg)`** — `lucide-*` strings render blank today.
29. **Per-line accessors on `Markoff::Editor`** (`getLine`, `lineCount`, `replaceRange`, `getSelection`/`setSelection`, `posAtCoords`). Plugin-API blocker for any meaningful editor extension.
30. **Plugin-side `Events` mixin** — Obsidian-shape `vault.on("create", cb)` won't work today.
31. **`registerObsidianProtocolHandler`** — `obsidian://`/`corbomite://` URL routing.
32. **`MarkdownRenderer.render` + `RenderContext`** for cross-surface link-affordance consistency (search rows, future Bases cells, plugin tooltips).

### P4 — Lifecycle / per-vault isolation

33. **Dangling adapter pointers in `onVaultClosed`** (`MainWindow.cpp:2190-2263`). Three `.reset()` calls.
34. **`onLoad` throw auto-unload** — try/catch + `Component::unload()`.
35. **Detach-leaves-of-type on plugin disable**.
36. **`onExternalSettingsChange()` + `data.json` `QFileSystemWatcher`**.
37. **`appId` for per-vault QtKeychain scoping**.
38. **`quit` event with `Eb` collector** for plugin async cleanup.
39. **`css-change` event** — bridge from `ThemeService::themeChanged` to Workspace.

### P5 — Missing internal plugins / completeness

40. **Daily Notes, Templates, Word Count plugins** — common-need features. `DailyNoteService`/`TemplateService` already exist as libs/models services; lift into KPluginFactory `.so`s.
41. **Page Preview plugin** — registry is built; tiny plugin-shaped popover orchestrator closes the loop.
42. **Command Palette + Switcher as plugins** — uniformity with Obsidian's "everything is a plugin" model.
43. **File Recovery plugin** with `.obsidian/file-recovery.json` schema (Cluster T not scheduled).
44. **PDF view + `![[file.pdf]]` embed** — biggest single rendering-domain capability gap.
45. **Paste-from-HTML → Markdown** (`htmlToMarkdown` analogue / Turndown port).
46. **Bases Phase 2** — UI for properties drawer, formula editor, group rendering, multi-key sort, export. Wire `QueryController::setCurrentFile` to workspace `file-open` so `this` actually works.
47. **Hotkeys page** — invoke `KShortcutsDialog` from `SettingsDialog`. (Optional: bridge `hotkeys.json` ⇄ `KSharedConfig` `Shortcuts` group with care to avoid emitting defaults.)
48. **About page**.

### P6 — API stability prep

49. **Move permission tokens from `PluginContext.cpp:21-32` to `corbomite/core/PluginPermissions.h`** with `inline constexpr auto` constants.
50. **Document the `apiLevel: 1` ABI** before first third-party plugin release.
51. **Document the `X-Corbomite-DockArea`/`DockIcon`/`DockTitle` extension** to manifest format.
52. **MomentFormatter missing tokens** (`Y`, `Q`, `gg`/`gggg`, `e`/`E`, `k`/`kk`, `Z`/`ZZ`, locale shortcuts). Vault templates using these render literal characters.

---

## Per-domain detailed findings

The full sub-report for each domain is reproduced verbatim under [Appendix: full sub-reports](#appendix-full-sub-reports). The summaries below distil the headline architecture / top-3 gaps / top suspected bugs per domain.

### Vault

`libs/vault/` is the most thoroughly built parity surface in Corbomite. `Vault` + `FileManager` + `VaultConfig` + `DataAdapter` + `TFile`/`TFolder` + `VaultProxy`/`FileManagerProxy` cover ~70% of Obsidian's surface area. Echo-suppression via mtime ledger + byte-equal-disk-compare is defence-in-depth; the U+FFFC terminal guard in `saveDocument` is a smart Corbomite-specific addition. Per-plugin permission tokens (`vault.read`, `vault.write`, `vault.events`) are end-to-end correct.

**Top gaps:** `raw` and `config-changed` events absent; `Vault::getConfig`/`setConfig` not modelled (each caller goes through `VaultConfig` directly with no debounce/partition); `getResourcePath` absent; `.obsidian/` excluded from watcher (prevents `config-changed` reload and `raw` for plugin data).

**Top suspected bugs:** folder rename loses descendants (`Vault::rename` doesn't recurse); `processFrontMatter` re-sorts keys alphabetically; `Vault::create` doesn't case-insensitively collision-check; `CaseSensitivityProbe` is dead code (no callers).

See `./vault.md` for full citations.

### Metadata

Cluster I's pivot — `SQLiteIndex` subscribing to `MetadataCache::cacheChanged` rather than re-parsing markdown — eliminated the prior "two parsers, two truths" problem. `LinkResolver` faithfully ports the 6-step algorithm. Hash-keyed dedup with refcount GC prevents cache leaks.

**Top gaps:** no `unresolvedLinks`/`resolvedLinks` reverse maps (degrades wiki-link popup, italic-broken-links, live-relink); no `getFirstLinkpathDest` exposed to plugins; tag aggregation lacks subtag-prefix counts; `aliases` not extracted from frontmatter.

**Top suspected bugs:** `SQLiteIndex` doesn't write `frontmatterLinks` rows (SQL backlinks miss every link declared in `related: "[[Foo]]"`); `drainOnePath` only re-resolves `cache.links` (embeds + frontmatterLinks stay raw); `LinkResolver` step 4 dot-relative path with extension fails for folders named `2026.04`; `collectFrontmatterLinks` grabs only first wikilink per string leaf.

See `./metadata.md` for full citations.

### Workspace + leaf-utilities

Right architectural skeleton — single Qt-signal-driven `Workspace` aggregate, undo stack, layout-ready gate, deferred-loading, plugin facade. KDDW substrate covers tab/split/float at a level that would have taken months to hand-roll. But the substrate translation has leaky seams.

**Top gaps:** nested-split round-trip loses pane sizes + per-group tab selections + popouts (write-lossy); `m_unknownRoot` blind write-through silently freezes Obsidian's `left`/`right` JSON; `m_tabGroupOf` lags user drags; `undoCloseLeaf` loses original parent + leafHistory + eState; popout window leak on X-close; `obsidian://` protocol handlers absent.

**Top suspected bugs:** two competing serializers (`Workspace::serialize` vs `WorkspaceSerializer::toJson`); `MenuEventEmitter::fileMenu` lacks source discriminator; `addAction` appends rather than prepends.

See `./workspace.md` for full citations.

### Views

Five-class hierarchy is a precise port — `View`→`ItemView`→`FileView`→`EditableFileView`→`TextFileView`. `ViewRegistry` fully matches Obsidian's atomic-validation invariant. `MenuSectionHelper`, `MenuEventEmitter`, `ViewRegistrar` ported well. Empty-state pane, deferred-load stub, displayTextChanged-as-Qt-idiomatic-leaf.updateHeader all correct.

**Top gaps:** `renderBreadcrumbs()` absent; inline `contentEditable` rename not implemented (delegates to host modal); `setEphemeralState` consumers (rename-on-create, scroll-to-heading, focus) all stubs; no per-view vault rename/delete subscriptions (centralised handling covers metadata but not per-view UX); pinned-tab exclusion in default tab close items missing; `syncState`/`receiveSyncState` for stacked-group linked tabs absent.

**Top suspected bugs:** `writeBackup` writes plain `.md` inside vault (shows up in tree, search, graph; triggers another `Vault::modified`); title not refreshed on external rename; open-file-deleted-externally orphans leaf; `FileView::setState` swallows missing-file silently.

See `./views.md` for full citations.

### Editor

Three-mode encoding with on-disk shape preserved verbatim. `EditorSuggest` ports cleanly. CJK autocorrect three-pattern table works. IME composition path is substantially closer to CM6-quality than a naïve QGraphicsView would give.

**Top gaps:** No `getLine`/`replaceRange`/`getSelection`/`setSelection`/`posAtCoords` public API on `Markoff::Editor` — plugin-API parity blocker. CodeMirror-style extension surface absent. Live cursor 1-based vs Source 0-based silent inconsistency. `EditorSuggest` synchronous-only (no async). No spell-checking, no vim mode, no paste-as-Markdown.

**Top suspected bugs:** CJK autocorrect cursor desync (TODO at `TextControl.cpp:1683-1700`); two trigger paths (`detectCompletionTriggers` emits dead signals while manager dispatches); `Markoff::Editor::insertAtCursor` doesn't push `MarkdownDelta` directly (relies on SceneCoordinator listener — brittle).

See `./editor.md` for full citations.

### Editor-markdown

Reading-mode progressive section pipeline is best-in-class — frame-budget constants exact-match Obsidian (10240-byte threshold, 5ms budget, 10 sections per yield), section recycling by AST shape, async parse with request-id coalescing, frontmatter-diff cascade. Embed framework with depth cap + placeholder fallback.

**Top gaps:** No `MarkdownRenderer.render(...)` static API (the load-bearing missing piece for hover popovers, search snippets, plugin tooltips); `MarkdownRenderChild` framework absent (no parent registry, no addChild, no mount/unmount); checkbox-click-to-toggle missing in Reading and broken in Live; cross-pane scroll-sync absent; live preview lacks per-block reveal granularity; live preview doesn't progressively section-render (Reading does; Live rebuilds full scene).

**Top suspected bugs:** Reading mode `setCursorLine` is hard-coded false (subpath link nav silently fails); `setFoldedHeadingLines` doesn't invalidate when line count changes (folds against wrong target); off-by-one column round-trip in Live.

See `./editor-markdown.md` for full citations.

### Rendering

Math (JKQTMathText), Mermaid (mmdr Rust FFI), syntax highlighting (KSyntaxHighlighting), embed registry, code-block processor registry, hover popover with real ReadingView all in good shape. HoverPopover is genuinely better than Obsidian's because it shares the recycling pool.

**Top gaps:** Callouts (data model exists in `Theme`, no renderer); footnotes are functionally dead (numbered but non-navigating, definitions silently dropped); PDF view entirely absent; paste-from-HTML drops all formatting (no `htmlToMarkdown`); no centralised `RenderContext` analogue for cross-surface inline-link consistency.

**Top suspected bugs:** Mermaid renders as light theme on dark themes (`MermaidRenderer::renderSvg` takes only source string, no theme parameter); math always wraps in `$...$` (display mode gets only font-size boost, not parser semantics); mmdr is uncached + sync; `default` lang stub registers a do-nothing processor that surprises plugins.

See `./rendering.md` for full citations.

### Parsing

rapidyaml (ryml) is the right shape — high-fidelity Tree DOM that preserves key order. Tree-sitter grammar covers Obsidian's extensions (wiki_link, highlight, obsidian_comment, latex_block, minus_metadata frontmatter). YAML 1.2 strictness manually re-implemented (`yes/no/on/off/y/n` kept as strings — critical Obsidian compat). `parseLinktext` byte-exact.

**Top gaps:** No `parseFrontMatterEntry`/`parseFrontMatterStringArray`/`parseFrontMatterTags`/`parseFrontMatterAliases` standalone helpers — each consumer rebuilds the logic, with divergences. `aliases` extraction missing entirely. Callouts not parsed structurally (just "is callout" boolean).

**Top suspected bugs:** `processFrontMatter` reorders keys alphabetically (`QVariantMap` = sorted `QMap`); `processFrontMatter` cannot strip emptied block; two different subpath resolvers with different rules; `MetadataParser` accepts singular `tag:` (more permissive than Obsidian); `MetadataParser` interprets string `tags: alpha, beta` as one bogus tag.

See `./parsing.md` for full citations.

### Bases

Pratt parser at ~200 lines covers the full DSL grammar. All 19 spec-named Value types as classes. Hard-cased `if`/`map`/`filter`/`reduce` with correct shadowing semantics. `MetadataCache::cacheChanged` integration closes the spec's open Q3 about `QueryController` provenance. Cell delegate dispatches by `ValueTypeRole`. 13 test executables, ~411 assertions.

**Top gaps:** YAML emitter alphabetises keys (single biggest user-visible round-trip regression); `unrecognizedData` truncates non-scalars (forward-compat invariant violated); `file()` global and `LinkValue::asFile()` return null (Vault binding not plumbed to evaluator); whole UI toolbar suite absent (no formula editor, no properties drawer, no group rendering, no multi-key sort cycle, no export); `BasesView` lacks workspace `file-open` subscription so `this` always resolves to `.base` itself.

**Top suspected bugs:** `ListValue::sort()` puts nulls first not last; `Formula` copy constructor re-parses unnecessarily; `file.tags.contains("#parent")` doesn't match `#parent/child` (no `TagListValue` analog).

See `./bases.md` for full citations.

### Search

`FuzzyMatcher` is line-for-line accurate — two-pass strategy, five-term scoring formula, CJK tokenisation, punctuation singletons, match-range merging. DSL covers `tag:`/`content:`/`path:`/`file:`/phrase/parens/`OR`/`-NOT`/`match-case:`/`ignore-case:`. SearchProxy plugin permission gate.

**Top gaps:** Bare `/regex/` query and top-level `-foo` both return zero results (concrete confirmed bugs); Search panel ranking uses BM25 not fuzzy `xy`; Click-to-jump doesn't seek to snippet position; one snippet per file (not per match); no streaming, no scope filters, no sort menu, no collapsible per-file groups; no search history; quick-switcher missing alias matching; command palette uses `KCommandBar` not the ported fuzzy matcher.

**Top suspected bugs:** Bare regex returns nothing because `SQLiteIndex.cpp:397` early-return ignores `postFilter`; top-level `-foo` produces invalid FTS5 (`SearchDSL.cpp:542-545`); `["property"]:value` parses to a no-op without `unsupported` notice; `FuzzyFilterProxyModel::lessThan` and `QuickSwitcherDelegate::paint` re-run `fuzzySearch` per pairwise comparison / per paint.

See `./search.md` for full citations.

### Settings

`KConfig`-based per-user persistence is the right KDE choice. `VaultConfig` library handles `.obsidian/*.json` with 2-space-indent unknown-key-preserving Obsidian-style serialisation. `core-plugins.json` legacy migration matches Obsidian. `PluginsPage` with permission integration and compat-state surfacing is more sophisticated than Obsidian's binary on/off.

**Top gaps:** `app.json` is read-mostly (only `userIgnoreFilters`); `appearance.json` write `theme` value vocabulary mismatch; Hotkeys parser exists but is dead code; PluginsPage doesn't wire `core-plugins.json`/`community-plugins.json` to PluginManager; no Hotkeys page (KShortcutsDialog not invoked); no plugin runtime settings-tab API; no graph/canvas/zk-prefixer/file-recovery/workspaces JSON files.

**Top suspected bugs:** Two `.obsidian/*.json` writers with different formatters (Bookmarks plugin writes 4-space indent); `MainWindow::applyVaultPortableSettings` only writes outbound — no inbound read at vault open.

See `./settings.md` for full citations.

### UI bundle

`Component` lifecycle, `MenuSectionHelper`, `MenuEventEmitter`, `Notice`, `CompletionPopup`, `EditorSuggest`, `MomentFormatPreview`, `Platform::showInFolder` all ported well. `DeleteConfirmDialog` is high-fidelity. `RenameDialog` has live validation. `MoveFileDialog` has folder-picker layout. `MarkdownView::insertFrontmatterProperty` wired for "Add file property" menu.

**Top gaps:** No Lucide icon registry (`lucide-*` names render blank); HoverPopover lacks Mod-key pinning + child-popover chains + anchor-to-mouse for tall targets + `elementFromPoint` poll (the popover renders correctly but the *interaction model* is half-built); FileExplorer context menu is 4 items vs ~13+, no `MenuSectionHelper`, no `fileMenu` emission, no plugin extensibility; no `Modal` base class, no `SuggestModal<T>` abstract, no `AbstractInputSuggest<T>`.

**Top suspected bugs:** `Notice(text, 0)` immediately closes (`Notice.cpp:48-50`); FileExplorer F2 → `QInputDialog` instead of `RenameDialog`, Delete → `QMessageBox::question` instead of `DeleteConfirmDialog`; `MenuSectionHelper::canonicalSectionOrder()` missing `title`/`action-primary`/`order`/`ribbon` (items silently fall into wrong bucket); `HoverLinkSourceRegistry` is write-only (no consumer reads `defaultMod`); `Component` cleanup ordering divergence (three vectors vs single LIFO queue).

See `./ui-bundle.md` for full citations.

### Plugin

KPluginFactory + `.so` modules with permission-gated proxies end-to-end. Permission model is the standout (declare → grant → KConfig persist → enforce; Origin-based trust override). Atomic `data.json` via `QSaveFile` improves on Obsidian. Two-axis API gating (`X-Corbomite-MinVersion` + `X-Corbomite-ApiLevel`) is strictly more capable than Obsidian's single-axis. 9 in-tree built-ins shipped via Cluster Q.

**Top gaps:** Only 6 of 13 Obsidian registration verbs have plugin proxies; six host-side registries lack plugin facades; missing `addStatusBarItem`, `registerObsidianProtocolHandler`, `registerEditorExtension` (Markoff equivalent), `registerMarkdownCodeBlockProcessor` plugin facade; no `onUserEnable()`, no `onExternalSettingsChange()` + watcher, no detach-leaves-of-type on plugin disable; missing 8 internal plugins (command-palette, switcher, daily-notes, templates, page-preview, word-count, file-recovery, note-composer).

**Top suspected bugs:** `onLoad` throw leaks state; permission tokens are string constants in a `.cpp` (wrong location for public surface); `Plugin::createView` returns `QObject*` (`reinterpret_cast<QWidget*>(mainWindow)` smell); no `data.json` watcher.

See `./plugin.md` for full citations.

### Core + addenda

Service split (`CorbomiteApp` for process-wide, `MainWindow` for vault-scoped) is sound and matches Pass 2's "do not clone the god object" recommendation. `Events` is a faithful port (snapshot-before-iterate, exception scheduling via `singleShot(0, …)`). `Scope`/`ScopeManager` infrastructure exists with global event filter, LIFO stack, top-down dispatch. `MomentFormatter` handles the common Daily-Notes / Templates token set.

**Top gaps:** No `dragManager`; no `customCss`; no `renderContext`; no `appId` (downstream consequence: secrets cannot be vault-scoped); no `quit` event with `Eb`-equivalent collector for plugin async cleanup; no `css-change` event bridge; no Scope users (infrastructure exists but nothing pushes onto it); MomentFormatter missing `Y`, `Q`, `gg`/`gggg`, `e`/`E`, `k`/`kk`, `Z`/`ZZ`, locale shortcuts; no Platform feature-flag accessors.

**Top suspected bugs:** `resolveSubpath` block-id case-sensitivity (`LinkUtils.cpp:121-155`) — vault-format compat; `resolveSubpath` multi-segment heading walk not implemented (self-noted); dangling adapter pointers in `onVaultClosed` (use-after-free window); embed registry leaks plugin factories across vault switches (latent); `secrets` permission gate double-checked but no vault scoping (downstream of missing `appId`); File Recovery is per-file `.md` files (Cluster G save-failure backups), not Obsidian's JSON store with retention policy.

**Addendum coverage:** Bookmarks plugin compatible on the core path (read+write `bookmarks.json` faithfully, debounced 500ms save) but 5 of 7 commands are palette stubs (`WorkspaceController` lacks accessors). Canvas export-as-image implemented per addendum spec (PNG/SVG, 2× density, transparent + show-edges) but missing 20px edge padding and uses Corbomite-prefixed command id (`canvas:export-as-image` not registered as a Command, only reachable via menu). Graph screenshot implemented (`QWidget::grab()`) but uses `corbomite-graph-view:copy-screenshot` not `graph:copy-screenshot` (vault hotkeys.json round-trip wouldn't fire). Daily Notes / Templates schemas read+write 3 of 4 spec keys; `autorun` is consumed nowhere.

See `./core-and-addenda.md` for full citations.

---
