# Cluster V — Editor & Workspace UI surfacing (retro)

**Closed:** 2026-04-20
**Plan:** [`superpowers/plans/2026-04-20-cluster-v-editor-workspace-ui-surfacing.md`](../superpowers/plans/2026-04-20-cluster-v-editor-workspace-ui-surfacing.md)
**Spec:** [`superpowers/specs/2026-04-20-cluster-v-editor-workspace-ui-surfacing-design.md`](../superpowers/specs/2026-04-20-cluster-v-editor-workspace-ui-surfacing-design.md)

## What shipped

- **Phase 1** (`c0e63f44` → `037d536e`): Find/Zoom/About wiring, `editor_toggle_mode` Ctrl+E cycle, `View::zoomIn/Out/Reset` default-no-op virtuals + `MarkdownView` delegation, `KColorSchemeManager` single-dispatcher via `MainWindow::onSettingsApplied`.
- **Phase 2+3** (Markoff submodule `f62be59`/`9c917a3`/`5b146c5` + outer `a5224b20`/`a076d5cc`/`aa028475`/`127530f4`): `Markoff::Editor::SetHeading1..6` actions (Ctrl+1..6), `cursorInTable()` accessor, widened `insertCallout(title)` + `insertTable(hasHeader)`, `currentHeadingLevel()`, `CalloutPickerDialog` + `InsertTableDialog`, ~40-entry KActionCollection (Edit/View/Format/Heading/Insert/Table/Fold submenus), Editor Mode radio submenu, `corbomiteui.rc.in` v10 menu tree, `refreshEditorActions` gated on `activeLeafChanged` + `cursorMoved`, `tst_mainwindow_action_wiring` introspection test.
- **Phase 4** — absorbed into Markoff Phase C5 (2026-04-20). Shipped at Markoff `v0.4.0` + Corbomite adapter `eef21e8e`. Unified `ReadingView::linkHovered(href, globalPos)`, new Reading-mode hover-popover wiring in `NoteEditorWidget`, click-to-fold regression guard. `codeBlockProcessorRegistry` routing + regular-URL hover forwarding deferred to C3/C4 (both required upstream refactors larger than a signal-unification work-unit).

## What went well

- The surface-first framing (chosen during brainstorming over the "shape-first" option) kept Phases 1-3 tight: every commit added something the user could click/see within days. No debt accumulated that blocked user-visible progress.
- Splitting debt out into **Cluster V.2** (fold-gutter coordinator, 6× VaultConfig writer routing, persistent metadata cache loader, autosave delay spinbox, LRU-reopen upgrade, post-V dead-code audit) prevented V from bloating into a multi-week refactor marathon.
- Phase 4's "absorbed by Markoff C5" resolution was clean because the handoff happened before any Corbomite-side code was written. Markoff became the natural owner of `ReadingView::linkHovered` widening and click-to-fold — both are leaf-internal concerns — and Corbomite only had to rewire the HoverPopover consumer. One adapter commit, no leakage.

## What we'd do differently

- **The phase-c-status C5 input prescription was written before code inspection.** Two of its four bullets turned out to be based on assumptions that didn't match production state:
  - "`codeBlockProcessorRegistry` routing — same as Markoff Live already does" — Live has zero registry consumers.
  - "LinkRenderer already emits for regular URLs" — the class is orphaned; nothing connects it.

  Both surfaced late (during C5 implementation, not during C5 brainstorming). The fix is procedural: when absorbing a Phase-C-style work-unit, inspect the claimed state-of-the-world with a grep-verified paragraph in the spec §1 before listing requirements. The C5 spec revision commit (`d445345`) and T3 deferral (`4b95f3d`) folded these corrections in — but ideally they'd have been caught during brainstorming.

- **`QCursor::pos()` synthesis was an anti-pattern worth naming.** The pre-C5 `NoteEditorWidget` code synthesized hover anchor positions from `QCursor::pos()` after the signal fired, instead of accepting them as signal arguments. This coupled the consumer to an assumption about signal timing (synchronous from mouse event handling) and made the anchor brittle on async/queued invocation. The C5 two-arg signal fixes this at both leaves; future Corbomite consumer signals should pass position args from the source, not synthesize at the sink.

## Follow-ups

- **Cluster V.2** remains open — fold-gutter coordinator + VaultConfig writer routing + persistent metadata cache loader + autosave delay spinbox + LRU-reopen upgrade + post-V dead-code audit. See `superpowers/plans/2026-04-20-cluster-v2-debt-cleanup-SCOUTING.md`.
- **Reading-mode regular-URL hover** — needs a `wikiLinkTargetAt`-equivalent URL-span hit-test or LinkRenderer pipeline integration. Tracked under Markoff C3/C4.
- **codeBlockProcessorRegistry routing redesign** — same destination (C3/C4). Unblocks math/latex fenced-block rendering + the four currently-gated `MARKOFF_READING_USE_REAL_COREDEPS` tests.
- **`tst_editorsuggest` QFATAL on cursor-past-end** — pre-existing bounds bug (`ASSERT: "n <= d.size - pos" in qstring.h:1226`) in `EditorSuggestManager::dispatch`. Unrelated to V but surfaced during C5 full-ctest. Add to backlog.
