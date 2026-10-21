# Finish the Markoff vNext integration

**Date:** 2026-10-17. **Status:** hard seam landed in the working tree, not committed. This file is the rest of the job. **Updated 2026-10-20:** §1 landed on pin `e0b96bf8`; §2's hunk survived that re-pin.

Read this file before editing. It is PROJECT-STATE trajectory step 1. The punch-list section `## Now — Markoff vNext` is the checkbox form of the same work. Cluster M6.2 and M6.3 stay in the Cluster M plan and start only after punch-list §Next. This is not a new cluster and it is not M6.

## Already done — do not redo

Markoff submodule `libs/markoff-family` is detached at **`2c58088a`** (the `v0.7.1` release tag; was `1c9b9415`, the tag's pre-push commit, and `e0b96bf8` and `dca64686` before that — the 2026-10-20/21 re-pins took upstream's card mode `aae3727d`, its strict frontmatter fence `5c0e5c6a`, and the v0.7.1 release's canvas rendering fixes). CollabText is the **installed package** `0.2.0-alpha.5` (manifest v17), found via `COLLABTEXT_ROOT` or the sibling prefix `../Markoff/build-collabtext-alpha5/install`; as of 2026-10-21 that prefix holds the `collabtext-perf-checkpoint-2026-10-21` engine (`16cc48f`), which Markoff's `v0.7.1` tag pins, built `Release` per the recipe in collabtext's `docs/USING_VNEXT.md`. Root `CMakeLists.txt` forces `MARKOFF_BUILD_LIVE=OFF` and `MARKOFF_BUILD_COLLABTEXT_VNEXT_TESTS=OFF` before `add_subdirectory(libs/markoff-family)`.

`EditingSessionSource` is the plugin seam (`libs/core/include/corbomite/core/EditingSession.h`). The default source mints one ephemeral provider per `bindingFor` call. `setEditingSessionSource(nullptr)` restores it. The pointer is not owned.

Key strings, only from these two functions:

- `noteDocumentKeyHint(vaultRoot, relativePath)` = `vaultRoot + '\n' + relativePath`
- `canvasTextCardKeyHint(canvasRelativePath, nodeId)` = `canvas:<path>#<nodeId>`

`NoteDocument` opens with the note hint. First `publishMarkdown` is `importInitialMarkdown`. Later calls are `replaceMarkdown`. Save uses `snapshotForExport`, writes those bytes, then best-effort `acknowledgeExport`. A failed snapshot fails the save. A missing file is not imported. `hostEditSeq` plus `d2EditSequence()` is the dirty watermark, because a pristine import and a whole-document replace do not move `d2EditSequence()`. `serializeForSave()` on a pristine import returns the original bytes and does not add a trailing newline.

`openImportedDocument` always imports. `openForRender` imports only when `providerState()` is `awaitingInitialImport` or `initialImportAuthorized`. `ready` sets `imported` and ignores the byte argument. `recoveryRequired` does not import and does not paint. `OpenDocument`'s destructor is out of line so a caller can destroy it without including the full `MarkoffDocument` type. `StyledRenderEngine::render` calls `openForRender` and then `DocumentRenderer::renderInto(QTextDocument*, const MarkoffDocument*)`.

`CanvasScene::setSessionContext` stores the vault root and the canvas path. `CanvasViewTab` sets it from `canvasRelativePath()` (the path half of `localDataKey()`, without the `canvas-` prefix). Text-card paints pass `canvasTextCardKeyHint` when that path is non-empty. File cards pass `noteDocumentKeyHint` plus `RenderOptions.subpath`. Unit tests that never call `setSessionContext` keep an empty hint and still paint.

`publicationCommitted` is true for `committed` and `committedHistoryPending`. `CompletionController::accept` and both replace slots in `NoteEditorWidget` return without moving the caret or the match when it is false.

The byte `renderInto` in `libs/markoff-family/libs/markoff-styled/src/DocumentRenderer.cpp` accepts the same two states. That file is dirty against `dca64686`. Do not commit the submodule for it.

Offscreen `ctest -E "benchmark|perf"` on 2026-10-17: **267/267**. Markoff's own tests are absent because both Markoff test options are forced off.

## Do not

- Do not build a durable journal, a `.corbomite/collaboration/` tree, replica ids, leases, presence, or network UI. Markdown files stay the on-disk contract. The Oct 12 host-persistence note is a later plugin, not this work.
- Do not call `makeReferenceDocumentStorageProvider`. It is Markoff's test store. A failed durable open has no fallback.
- Do not share one ephemeral provider across documents. The default source already allocates one per call. Leave that.
- Do not call `importInitialMarkdown` on a document whose `providerState()` is `ready`. A file-card subpath slice would wipe the live note. Stock paints still import, because each paint gets a new ephemeral provider.
- Do not "fix" a ready file card that has a subpath by importing the slice. Stock paints the slice (new provider). A plugin that returns the live note paints the whole projection. A later subpath-of-a-live-document paint needs a new Markoff API or a slice of `serializeForSave()` that is never imported. Do not invent it here.
- Do not treat `ActionPublicationState::committed` as the only success. Use `publicationCommitted`.
- Do not call `pasteMarkdownAt` for a flat insert. It trims, so `" more"` becomes a paragraph `"more"`, and `"Hello"` plus `"X\n\nY"` at byte 3 becomes `"Hel\n\nX\n\nYlo"`. `NoteDocument::insertAtFlatOffset` is the workaround: no blank line uses `applyBlockEdit`; a payload that contains `\n\n` splices prefix + payload + suffix and calls `pasteMarkdownReplacingRange` over the whole block. Delete that workaround only after Markoff ships handoff item 5.
- Do not replace the canvas `QTextEdit` until handoff item 1 has landed. Do not write a second editor.
- Do not include `EditingSession.h` from `tests/editor/tst_findbar.cpp` or `tests/editor/tst_line_resolve.cpp`. They do not link `corbomite-core`. They already have a local ephemeral open helper.
- Do not `add_subdirectory` the untracked `libs/markoff-family/libs/collabtext/` tree, and do not touch `/home/clinton/dev/collabtext`.
- Do not bump the Markoff pin, and do not commit, unless the user asks. Parent dirty files that are not this integration stay unstaged.
- Do not follow links into `docs/archive/`.

## Remaining work, in order

### 1. The card editor — landed 2026-10-20

Handoff: `docs/handoff/2026-10-17-to-markoff-canvas-cards-and-session.md` item 1. Markoff shipped the widget: upstream `aae3727d` added the card (embedded) mode to `Markoff::Canvas::EditorWidget` — the host owns the geometry, the scrolling and the chrome; the card arrives read-only and the host promotes it. Pin `e0b96bf8`.

What landed, against the four steps below:

1. `CanvasScene::beginInlineEdit` puts a `Markoff::Canvas::EditorWidget` (embedded mode) in `m_editProxy` instead of the `QTextEdit`. The card is opened with `openImportedDocument(text.toUtf8(), canvasTextCardKeyHint(...))` and the `OpenDocument` is held by the scene until `finishInlineEdit`. An empty hint (unit tests) gets a throwaway document.
2. `finishInlineEdit` stores `serializeForSave()` into `CmdEditText`, then paints with `openForRender` and the same hint (the paint path was already that). It never stores a widget string.
3. File-card edit still writes through `m_fileSaver`, and writes the projection of the note-hint document. The open is `openForRender(resolverBytes, noteDocumentKeyHint(...))`: it returns the plugin's live note when the provider is `ready` and imports the resolver bytes only while the document is empty, so a live note is never imported over (the "do not" list forbids that, and `openImportedDocument` would have done it).
4. `CmdEditText`'s undo contract is unchanged: old text, new text, one command.

Two host decisions came out of it, both recorded in [`decisions-archive.md`](../../decisions-archive.md) (`2026-10-20 — The card view is integrated`):

- **An edit whose document did not open must not start.** A document that never took the import projects an empty text, and that projection is what the card would store — so the edit is refused instead of erasing the card.
- **The edit ends on the scene's click, not on the application's focus.** A `QGraphicsProxyWidget` keeps the embedded leaf a top-level widget with `WA_DontShowOnScreen`; its window never becomes the application's active window, so `QApplication::focusChanged` never fires for it. The scene's `mousePressEvent` (a press outside the editor proxy) finishes the edit; the focus watcher only catches a focus that leaves the editor's subtree. Filed as a Markoff gap in `docs/obsidian-audit/01-markoff-gaps.md`.

Pinned by `tst_canvas_card_editor` (7 cases) and verified live under Xvfb. The re-pin also brought upstream's strict frontmatter fence (`5c0e5c6a`), which is a separate change — see [`2026-10-20-strict-frontmatter-fence-and-the-writers-refusal.md`](../../obsidian-audit/addenda/2026-10-20-strict-frontmatter-fence-and-the-writers-refusal.md).

### 2. Re-apply the renderer one-liner on every re-pin

`libs/markoff-family/libs/markoff-styled/src/DocumentRenderer.cpp`, byte `renderInto`: after `importInitialMarkdown`, return without painting only when the state is neither `committed` nor `committedHistoryPending`. The current comment in that function is the reason. Host paint no longer uses this overload (`StyledRenderEngine` uses the pointer overload). Hover and any future byte-path caller still need it. If a re-pin drops the hunk, put it back. Do not send it upstream by editing `/home/clinton/dev/Markoff` unless the user asks.

**Checked on the 2026-10-20 re-pin to `e0b96bf8`, on the 2026-10-20 re-pin to `1c9b9415`, and again on 2026-10-21 on the `v0.7.1` tag target `2c58088a`: the hunk is still there** (the byte path accepts `committedHistoryPending` as well as `committed`). Each time it survived the checkout intact — upstream's `DocumentRenderer.cpp` is unchanged since `e0b96bf8`, so the move carried the local modification rather than conflicting — upstream's renderer still accepts only `committed`, and `importInitialMarkdown` still returns `committedHistoryPending` for a successful-but-pending publication, which is the §2 condition the hunk depends on. The submodule's working tree carries it; the submodule is not committed.

### 3. Comments that still name removed APIs

`applyFlatEdit`, `resetContent`, and `documentReloaded` are gone from the public document API. Rewrite only these host comments so the next reader does not call them. Do not edit the submodule, `docs/decisions-archive.md`, old punch-list entries, or plans under `archive/`.

- `src/editor/NoteEditorWidget.cpp` (insert-at-cursor comment)
- `src/editor/LineResolve.h` and `LineResolve.cpp`
- `src/editor/MarkdownViewActions.cpp` (the disabled-reason strings and the comment near them). Those `i18n()` strings are user-visible. Say the command needs a real block-kind change. Do not name `applyFlatEdit`.
- `tests/editor/tst_line_resolve.cpp`
- `tests/core/tst_notedocument_dirty.cpp`

Behaviour stays. Run `tst_line_resolve`, `tst_notedocument_dirty`, and `tst_note_editor_widget_insert_at_cursor` after the comment pass.

### 4. Verify

```bash
cmake --build --preset dev -j 10
cd build-dev && QT_QPA_PLATFORM=offscreen ctest -E "benchmark|perf" --output-on-failure -j 10
```

A single red timing test is re-run alone before it is called a regression. `tst_metadatacache_worker_integration` is not a flake: `indexFinished` must wait for worker parses (punch-list §Next). Update the `CLAUDE.md` baseline count only when this command is green, and say that Markoff's own tests were not in the run.

## Handoff that is still waiting on Markoff

`docs/handoff/2026-10-17-to-markoff-canvas-cards-and-session.md`, indexed from `docs/UPSTREAM-ASKS.md`:

1. Embeddable card editor. **Answered 2026-10-20** — upstream `aae3727d` shipped the card (embedded) mode; §1 is landed. The follow-up that came out of it: the mode documents the host's duties for geometry, scrolling and chrome, but not for focus, and a `QGraphicsProxyWidget` host cannot use application-level focus to notice that the user left the card. Filed in `docs/obsidian-audit/01-markoff-gaps.md`.
2. A durable provider may keep many scopes open. Ephemeral stays one session per provider.
3. Keep `renderInto(QTextDocument*, const MarkoffDocument*)` public and cheap.
4. Optional publication that adopts external bytes without an undo step. Do not change `replaceMarkdown` to drop history. Clean reload undo restoring the pre-reload text is intentional.
5. `pasteMarkdownAt` should splice and keep leading spaces. Gates deletion of `insertAtFlatOffset`'s workaround.

The "what a canvas card is today" paragraph in that handoff now matches the pointer paint path. The `QTextEdit` editor it describes was replaced on 2026-10-20 (§1).
