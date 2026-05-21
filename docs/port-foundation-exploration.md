# Corbomite port: `port/foundation-exploration` branch

**Created:** 2026-05-20.
**Purpose:** Port Corbomite from Markoff `master` (v0.6.x line) to Markoff `exploration/new-foundation` (the D-arc + E-arc rebuild that retired the old four leaves).
**Status:** In flight. Compiles, launches, renders documents in Live mode. Many features degraded; each comes back as its feature port lands.

This branch is the active port effort. **Corbomite `master` remains untouched** and continues to track Markoff `master`. The two diverge fully here.

## Cross-repo branch map

| Repo | Branch | State |
|------|--------|-------|
| Markoff (`/home/clinton/dev/Markoff`) | `master` | v0.6.x line. Frozen — foundation-exploration will eventually merge here. |
| Markoff | `exploration/new-foundation` | **Active dev.** D-arc + E-arc rebuild. ~1000 commits ahead of master. |
| Corbomite (`/home/clinton/dev/Corbomite`) | `master` | Pinned to Markoff `master` tip (`2b7b3e7`). No port work touches it. |
| Corbomite | `port/foundation-exploration` | **This branch.** Pinned to Markoff `exploration/new-foundation` HEAD. |

The Markoff side is worked from `.worktrees/foundation-exploration/`. The Corbomite side is the main checkout (no worktree — different branch on the same tree).

## Eventual merge plan back to masters

1. **Complete the port on this branch** — one feature at a time as evidence-driven micro-specs against Markoff `exploration/new-foundation`. Find UI is feature #1.
2. **When the port is functionally complete** (find, source-widget swap, embeds/tags/callouts, table editor minimum, theme port), draft a small evidence-driven `markoff-core` freeze spec based on real port pressure (NOT the speculative draft preserved at Markoff's `docs/specs/2026-05-20-markoff-core-freeze-shape-design.md`).
3. **Tag Markoff** at that point — probably `v0.7.0-freeze`.
4. **Merge Markoff `exploration/new-foundation` → Markoff `master`.** Large merge: retires old leaves wholesale; new layout becomes canonical.
5. **Merge Corbomite `port/foundation-exploration` → Corbomite `master`.** Submodule pin tracks new Markoff master tip.

**Markoff merges first.** Reversing the order leaves Corbomite master with a submodule pin nobody else can resolve.

## What's done

### Build infrastructure

- Submodule pin moved to Markoff foundation-exploration tip.
- Mechanical CMake target renames: `MarkoffParser::MarkoffParser` → `Markoff::Parser`; `Markoff::Live` → bare `markoff_live` (no alias exists in the new layout); static QML module plugin linked as `markoff_liveplugin` + `markoff_liveplugin_init`.
- Mechanical include-path renames: `markoff/X.h` → `markoff/core/X.h` (Theme, MarkdownView, MarkoffDocument, EditorContext, CodeBlockProcessorRegistry); `markoff-parser/X.h` → `markoff/parser/X.h`; `markoff/source/SourceEditor.h` → `markoff/source/Editor.h` (also a type rename: `Markoff::Source::SourceEditor` → `Markoff::Source::Editor`).
- `Markoff::Reading::*` references stubbed with TODO markers (Reading leaf retired; no replacement yet).
- Top-level `CMAKE_POSITION_INDEPENDENT_CODE=ON` so Corbomite's shared libs can link Markoff's static libs.
- Tests gated behind `CORBOMITE_PORT_BUILD_TESTS=OFF` (default). Most reference retired types and would need their own port pass. Re-enable in chunks as feature ports come online.

### Editor hosting

- `Corbomite::NoteEditorWidget` ported to host Live via `Markoff::Live::EditorWidget` (the new QQuickWidget wrapper added in Markoff bc8216d).
- Most `Markoff::Editor` (the retired live class) signal connections + method calls stubbed with TODOs.
- Theme service (`SystemThemeBuilder` + `ThemeService`) wholly `#if 0`-disabled with minimal stubs for linkage.
- `MarkoffAdapters` (LinkResolverAdapter, MetadataCacheAdapter, MetadataParserImpl) `#if 0`-disabled pending `Markoff::Vault::*` concretes restoration.
- `Corbomite::SourceEditor` (the qutepart shim) stubbed to plain `QPlainTextEdit` pending the source-widget swap port.
- HoverPopover's ReadingView dependency stubbed; popover renders nothing.

### Document loading

- `Vault::openDocument` switched from `MarkoffDocument::resetContent` (legacy buffer only) to `loadFromMarkdown` (D2-aware parse + materialize blocks). This is the fix that made the editor actually render content.

## What works

- App launches, opens vaults, opens documents.
- Live mode renders documents (headings, paragraphs, lists, etc.).
- Tabs work for switching between open docs.

## Known degradations

| # | Symptom | Probable cause | Next move |
|---|---------|---------------|-----------|
| 1 | Editing causes content to repeat at end of doc | Multiple `LiveListModelBindings` share one `MarkoffDocument` across tabs; each EditorWidget creates its own Session; edits fire on the shared doc → all bindings respond. | Corbomite-side restructuring. Either one binding per doc shared by views, or one doc per leaf with state replication. Brainstorm needed. (Task #8) |
| 2 | Source mode renders empty | `SourceTextDocumentBinding` needs its own population trigger equivalent to EditorWidget's `flushPendingD2Changed`. | Probably a Markoff-side fix. Investigate when source port begins. (Task #9) |
| 3 | Most toolbar actions stubbed | `Markoff::ActionId` enum restructured; Corbomite's editor-action registration block disabled wholesale. | Each comes back per feature port. Find UI port covers find-related ones. |
| 4 | Sidebars don't show on vault open | Unknown — possibly plugin loading regression or sidebar-construction issue in MainWindow. Unrelated to editor port. | Investigate separately. Not editor-port-blocking. |
| 5 | Reading mode = no-op fallback to Live | `Markoff::Reading::ReadingView` retired. | Restore reading leaf later OR rewire against read-only Live (`Capabilities::Editable`). |
| 6 | MermaidRenderer is no-op | Abstract retired with old leaves (E5 work). | E5 Markoff phase. |
| 7 | Embeds non-functional | Abstract restored, no concrete factories registered. | E3 Markoff phase. |
| 8 | HoverPopover renders nothing | Used Reading::ReadingView. | Depends on Reading restoration OR Live-with-editing-disabled. |
| 9 | Theme import from QOwnNotes .ini disabled | `Markoff::Theme::importFromQOwnNotesIni` retired. | Theme port. |
| 10 | Word count not updated | `wordCountChanged` signal retired; no equivalent yet. | Small Markoff-side add (wordCount on MarkoffDocument). |
| 11 | Undo/redo non-functional | `Markoff::Editor::undo/redo` retired; new path is `MarkoffDocument::d2UndoLog`. | Wire to d2UndoLog in a separate port pass. |
| 12 | Ephemeral state round-trip non-functional | Line/column cursor model doesn't map directly to TextAnchor/BlockAnchor. | Revisit when EphemeralState pulls. |

## Next session — priority order

1. **Find UI port** (the original port-first target). Build a Corbomite-owned `FindBar` QWidget; instantiate `Markoff::FindController` per document; attach/detach on leaf swap; wire Ctrl+F + FindNext + FindPrevious. Should produce zero or one Markoff-side micro-spec.
2. **Doc-sharing doubling.** Quality bug — needs its own brainstorm + micro-spec.
3. **Source mode empty.** Probably Markoff-side fix.
4. **`MarkoffDocument::resetContent` builds D2.** Markoff-side cleanup of the workaround we did in Vault.

## Commit ledger (this branch only)

Run `git log --oneline master..port/foundation-exploration` for the live list. Highlights:

- `f4ad88a4` — submodule pin bump to Markoff foundation-exploration tip.
- `3f19703f` — initial CMake renames + stubs.
- `00d8c455` — EmbedRegistry uptake + theme files disabled + tests gated + PIC + CodeBlockRegistrar migrated.
- `76bcf7b3` — **the big one** — NoteEditorWidget hosts Live via EditorWidget; CorbomiteApp builds clean.
- `0c9e35e8` — submodule re-bump for EditorWidget resource path fix.
- `040d5654` — link `markoff_liveplugin` for static-QML resolution.
- `875f852c` — Vault switches to `loadFromMarkdown`; Corbomite RENDERS docs at this point.

Plus paired submodule bumps as Markoff fixes landed.

## Markoff-side commits this branch depends on

All on Markoff `exploration/new-foundation`. Listed for traceability:

| Markoff commit | What it did for the port |
|----------------|--------------------------|
| `af45aa5` | `MARKOFF_BUILD_APPS=OFF` for submodule consumers |
| `47f62c4` | Restore EmbedRegistry + MarkdownRenderChild + EmbedDepthGuard + Vault::ResourceProvider |
| `e8986f8` | EmbedRegistry hasExtension/unregisterExtension |
| `bc8216d` | **NEW: `Markoff::Live::EditorWidget`** — QQuickWidget wrapper that hosts the live editor in a QWidget host |
| `d4b117a` | Fix EditorContent.qml resource path |
| `d5d210e` | EditorWidget flushes pending d2 changes after setDocument |
| `2291c99` | Remove debug instrumentation |

Plus this branch's own `876f...` (or wherever the handoff doc lands).

## How to resume

Fresh agent landing on this branch:

```bash
cd /home/clinton/dev/Corbomite
git status                                  # confirm port/foundation-exploration is current branch
git log --oneline master..HEAD              # see what port commits exist
cd libs/markoff-family
git status                                  # detached HEAD on a Markoff commit
git log --oneline -3                        # confirm pinned commit matches what's expected
cd /home/clinton/dev/Corbomite
cmake --build build-dev -j 8                # should build clean
./build-dev/bin/Corbomite                   # should launch
```

For the typical port-first iteration:

1. Identify a Corbomite-side feature gap (look at TODO(port-foundation-exploration) markers).
2. If the gap needs a Markoff API change: brainstorm → micro-spec → implement in Markoff worktree → push → re-bump submodule pin here.
3. If the gap is Corbomite-side only: port the consumer code directly on this branch.
4. Commit on this branch; build; run; verify.

Full session recap in Markoff: `libs/markoff-family/docs/handoff/2026-05-20-port-first-session-recap.md`.
