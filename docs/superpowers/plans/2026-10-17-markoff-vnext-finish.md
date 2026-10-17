# Finish the Markoff vNext integration

**Date:** 2026-10-17. **Status:** hard seam landed in the working tree, not committed. This file is the rest of the job.

Read this file before editing. It is PROJECT-STATE trajectory step 1. The punch-list section `## Now — Markoff vNext` is the checkbox form of the same work. Cluster M6.2 and M6.3 stay in the Cluster M plan and start only after punch-list §Next. This is not a new cluster and it is not M6.

## Already done — do not redo

Markoff submodule `libs/markoff-family` is detached at `dca64686` (local `master`, 21 commits ahead of GitHub `origin/master` `28e9a734`). CollabText is the installed package `0.2.0-alpha.5` (`e8a8e7d1`), found via `COLLABTEXT_ROOT` or the sibling prefix `../Markoff/build-collabtext-alpha5/install`. Root `CMakeLists.txt` forces `MARKOFF_BUILD_LIVE=OFF` and `MARKOFF_BUILD_COLLABTEXT_VNEXT_TESTS=OFF` before `add_subdirectory(libs/markoff-family)`.

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

### 1. Leave the card editor until Markoff ships it

Handoff: `docs/handoff/2026-10-17-to-markoff-canvas-cards-and-session.md` item 1. The widget must be a bounded `MarkdownView` (or `Canvas::EditorWidget` with a card flag): host-controlled size, no toolbar, embeddable in a `QGraphicsProxyWidget`. When that widget exists, and not before:

1. In `CanvasScene::beginInlineEdit`, put that widget in `m_editProxy` instead of `QTextEdit`. Open the card with `openImportedDocument(text.toUtf8(), canvasTextCardKeyHint(...))` when the hint is non-empty, and keep the `OpenDocument` alive until `finishInlineEdit`. An empty hint (unit tests) may keep a throwaway document.
2. `finishInlineEdit` must store `serializeForSave()` into `CmdEditText`, then paint with `openForRender` and the same hint. Do not store `QTextEdit::toPlainText()`.
3. File-card edit (`beginFileCardEdit` / `finishFileCardEdit`) still writes through `m_fileSaver`. When the card editor exists, write `serializeForSave()` of the open `NoteDocument` when one is open for `noteDocumentKeyHint(vaultRoot, file)`, otherwise of a document opened with `openImportedDocument` on the resolver bytes. Do not write a widget string that differs from that projection.
4. Keep `CmdEditText`'s undo contract: old text, new text, one command. The new text is the projection bytes.

Until item 1 exists, steps 1–3 are blocked. Do not start them.

### 2. Re-apply the renderer one-liner on every re-pin

`libs/markoff-family/libs/markoff-styled/src/DocumentRenderer.cpp`, byte `renderInto`: after `importInitialMarkdown`, return without painting only when the state is neither `committed` nor `committedHistoryPending`. The current comment in that function is the reason. Host paint no longer uses this overload (`StyledRenderEngine` uses the pointer overload). Hover and any future byte-path caller still need it. If a re-pin drops the hunk, put it back. Do not send it upstream by editing `/home/clinton/dev/Markoff` unless the user asks.

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

1. Embeddable card editor. Gates section 1 above.
2. A durable provider may keep many scopes open. Ephemeral stays one session per provider.
3. Keep `renderInto(QTextDocument*, const MarkoffDocument*)` public and cheap.
4. Optional publication that adopts external bytes without an undo step. Do not change `replaceMarkdown` to drop history. Clean reload undo restoring the pre-reload text is intentional.
5. `pasteMarkdownAt` should splice and keep leading spaces. Gates deletion of `insertAtFlatOffset`'s workaround.

The "what a canvas card is today" paragraph in that handoff now matches the pointer paint path. The `QTextEdit` editor is still what item 1 replaces.
