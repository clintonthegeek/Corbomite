# Road to Dogfood — master sequencing plan

**Date:** 2026-06-10 · **Status:** Active master plan · **Type:** Sequencing/orchestration (phases dispatch to their own specs/plans/punch-list items — this file does not duplicate their detail)

**Goal:** a Corbomite the user can run as his daily Obsidian replacement on a real
vault — *stable* (no data loss), *sound* (no known correctness rot), and
*respectable in public* (docs in order, no god classes, no dead weight) — focused
on **core Obsidian features**; plugins are explicitly post-first-release.

**Inputs:** [`docs/PARITY-MATRIX.md`](../../PARITY-MATRIX.md) (current truth),
[`docs/punch-list.md`](../../punch-list.md) (re-triaged 2026-06-10),
the Markoff adoption brief
(`/home/clinton/dev/Markoff/docs/handoff/2026-06-09-corbomite-api-adoption-brief.md`),
[`specs/2026-06-10-mainwindow-decomposition-design.md`](../specs/2026-06-10-mainwindow-decomposition-design.md),
[`plans/2026-06-10-release-hygiene.md`](2026-06-10-release-hygiene.md).

**Gate discipline:** each phase ends green (250/251 offscreen baseline or better),
committed, with PARITY-MATRIX rows updated. Phases 0–2 are strictly ordered;
3–5 can interleave; 6 runs continuously once 0–1 land.

---

## Phase 0 — Data safety (BLOCKS DOGFOODING; days, not weeks)

**Status: COMPLETE (2026-06-10, branch `feature/phase0-data-safety`)** — 256/257 offscreen; lone red `tst_metadataparser` gated on the Phase 1 re-pin. Note: the blank-line-collapse interop decision (item 0.6) was **RESOLVED 2026-06-10 — accept + document** (user decision): Markoff's blank-line-run normalization is intentional (B1 §2) and is now treated as an expected, documented interop diff rather than a defect; release criterion below amended accordingly. See [`docs/handoff/2026-06-10-blank-line-collapse-triage.md`](../../handoff/2026-06-10-blank-line-collapse-triage.md).

Nobody dogfoods an editor that can eat notes. All items are punch-listed with
file:line evidence; most are <20-line fixes.

| # | Item | Source |
|---|---|---|
| 0.1 | Atomic `Vault::saveDocument` — route through `m_adapter->writeBinary` | punch-list P0 (`Vault.cpp:755`) |
| 0.2 | Wire `LinkService` → link clicks navigate (Live + Reading) | punch-list P0 [audit-2026-06-10] |
| 0.3 | LinkResolver freshness: connect created/renamed/deleted → add/removeVaultPath; feed non-`.md` too | punch-list P1 |
| 0.4 | `modify()` ↔ open-NoteDocument reconciliation (decide policy: refresh-if-clean, signal-if-dirty) | punch-list P1 |
| 0.5 | BOM strip in `MetadataCache::rebuildVault` | punch-list P1 |
| 0.6 | Steer upstream to Markoff: frontmatter closing-fence false-match; blank-line-collapse triage (NT item — confirm writer, then fix or steer) | punch-list P1/NT |
| 0.7 | Move `index.sqlite`/`metadata-cache.db` out of `.obsidian/` → AppLocalDataLocation | punch-list P1 (was P4) |

**Exit:** a crash mid-save cannot truncate a note; clicking a wikilink opens it;
a note created this session resolves immediately; the vault dir stays clean.

## Phase 1 — Markoff re-pin + contract-v2 adoption (~1 week)

**Status: COMPLETE (2026-06-10, master)** — final re-pin at `8112833f`; adoption
brief §2 fully consumed; **259/259** offscreen (excl. `benchmark`); MainWindow
leaf-agnosticism grep gate enforced (zero concrete-leaf mentions). **Three
roadmap deviations landed upstream in Markoff this phase** (plan anticipated
zero-to-one): ① the `![[…]]` embed image-node fix was NOT in Markoff master at
planning time — implemented upstream as `9a6a6b74` (Task 1); ② `markoff-source`/
`markoff-styled` never emitted the base `cursorPositionChanged` — fixed as
`23c36aac` (Task 8, unplanned, user-approved); ③ the Live leaf lost
`setCursorPosition`/`setScrollPositionVisualLine` issued in the document-attach
window (QML initial-focus seed clobbered the pending caret; scroll fraction
dropped at zero contentHeight) — fixed as `8112833f` (Task 9, unplanned,
user-approved; falsifiable suite `tst_view_contract_live_attach_window`).
Execution record: [`2026-06-10-phase1-markoff-repin-contract-v2.md`](2026-06-10-phase1-markoff-repin-contract-v2.md);
in-progress handoff (kept for the attach-window investigation record):
[`../../handoff/2026-06-10-phase1-contract-v2-progress.md`](../../handoff/2026-06-10-phase1-contract-v2-progress.md).

The single highest-leverage work item: one re-pin + mechanical edits unstub
eight dead surfaces.

1. Re-pin `libs/markoff-family` to Markoff master at/after the Task-13 commit
   (**never into `8c13c5d..079ac1f`** — styled-table SIGSEGV window). Verify the
   `![[…]]` embed image-node fix is in the pin (steered 2026-06-04); if so,
   `tst_metadataparser` goes green → **251/251**.
2. Execute the adoption brief section by section (it cites exact Corbomite
   lines verified against `b6ae2c0f`): find-attach via base virtual (Reading
   find arrives free) · undo/redo via base (**fixes the Source dual-stack
   divergence**, INVARIANTS §3) · theme propagation · format verbs on the base
   pointer + delete `addEditorActionForwarded` · `contextChanged` → toolbar
   enable-state + heading radio · ephemeral state capture/restore · goToLine
   all modes · Ln/Col statusbar.
3. Free riders to verify after re-pin: styled tables in Reading; source
   find-highlight drift fix; source fontScale (unify zoom across leaves).
4. Update PARITY-MATRIX editor rows; close the matching punch-list items.

**Exit:** Reading mode is no longer a downgrade; undo is correct everywhere;
the brief's §2 migration table is fully consumed.

## Phase 2 — Editor experience completion (~2 weeks)

What a writer needs hourly. Order within phase is suggested, not binding.

- **Completion revival** — decide CompletionPopup-revive vs rewrite against the
  current suggester API; wire `maybeActivateSuggester`; `[[` + `#` end-to-end.
  (Biggest single UX win after links.)
- **Status bar honesty** — word count (connect `NoteDocument::textChanged` →
  existing `wordCount()` cache) + Ln/Col (arrives via Phase 1).
- **Hover preview** — ✅ DONE 2026-06-11 (pending live eyeball). `HoverPopover::renderTarget`
  re-targeted at `StyledRenderEngine` + a `VaultResourceProvider` resolver into a
  `QTextBrowser`; trigger wired through the shared `LinkService` (`linkHovered`/
  `linkHoverLeft`), covering Live + Reading at once — no upstream Markoff work or
  re-pin needed. Subpath (`#heading`/`#^block`) slicing + modifier-gated hover deferred.
  Spec/plan: `specs/2026-06-11-hover-preview-relight-design.md`,
  `plans/2026-06-11-hover-preview-relight.md`.
- **Placebo removal** — Insert Table/Callout dialogs + hamburger Find/Replace:
  wire or hide (punch-list P3). Disabled-stub menu actions: hide from menus
  until functional (keep registered for discovery).
- **Template-at-cursor** (punch-list P3) · **checkbox toggle in Reading**
  (styled-leaf interaction; punch list re-aimed).
- Defer to post-1.0 unless trivially unlocked upstream: callouts, footnotes,
  mermaid-in-Live, embed transclusion rendering, paste-image, replace UI,
  vim/spellcheck/RTL.

## Phase 3 — Workspace & interop polish (~1–2 weeks, interleaves with 2)

- Back/forward: route opens through `leaf->navigate()`, add actions +
  Ctrl+Alt+←/→ + mouse buttons 4/5 (engine already built+tested).
- Sidebar persistence: call the dormant KateMDI session save/restore from
  `saveSessionState`/`onVaultOpened` (code exists; do NOT let release-hygiene
  Phase C delete it).
- workspace.json fidelity: persist full `serialize()` (floating +
  lastOpenFiles); assign split/tabs node ids; round-trip `dimension`.
- `hotkeys.json` application at vault open (map Obsidian command ids → KActions).
- `app.json` consumption: trashOption, alwaysUpdateLinks, newFileLocation,
  useMarkdownLinks/newLinkFormat in `generateMarkdownLink`.
- Quick switcher: path + alias matching, visible create-row, Shift/Ctrl+Enter.
- Untitled-note flow: inline "Untitled N" (Obsidian counts from 1 — see
  addendum 2026-06-10) instead of the modal prompt.
- Pin-tab + move-to-new-window commands (one-call wrappers).

## Phase 4 — View-type gaps (~2 weeks, parallelizable per view)

- **Canvas:** instantiate CreateEdgeTool/CreateCardTool + minimal tool strip;
  vault-root file-card resolution; order-preserving node/edge containers;
  image + link nodes; edge color.
- **Graph:** persist settings to `.obsidian/graph.json`; local-graph depth
  control; tag/attachment filters; color groups (stretch).
- **Search:** multi-snippet per file (the `snippet()` one-row limit); match-case
  toggle UI; history; copy-results.
- **Bases:** the 7 missing built-in summaries; (stretch) cards view + embedded
  ` ```base ` blocks.
- **Tags pane** — the one wholly-missing core panel.

## Phase 5 — Architecture & public respectability (continuous; finishes last)

Dispatches to the two dedicated docs:

- [`specs/2026-06-10-mainwindow-decomposition-design.md`](../specs/2026-06-10-mainwindow-decomposition-design.md)
  — VaultSessionController → EditorActionController (after Phase 1) →
  PluginHostController → theme-into-ThemeService. Sequenced so Phase 1's
  dispatch collapse lands first.
- [`plans/2026-06-10-release-hygiene.md`](2026-06-10-release-hygiene.md) —
  LICENSE/REUSE/LGPL text · mmdr blob replacement (history-rewrite decision at
  release cut) · dead-code purge (~1,000+ lines; four items gated on this
  roadmap's revival choices) · renames (CorbomiteMDI→ToolViewHost,
  MarkdownFileView, VaultResourceProvider de-collision, canvas/forcegraph
  normalization) · jargon/TODO sweep · AppStream metadata · core↔storage cycle.
- **ThemeService revival** (currently `#if 0`): minimum viable = populate the
  Settings theme combo + apply `appearance.json` light/dark/accent; CSS theme
  import stays post-1.0.

## Phase 6 — Dogfood loop (starts after Phase 1; permanent)

1. **One consolidated eyeball session** clearing the 8-package verification
   backlog (PROJECT-STATE § Open questions).
2. Daily-drive a real vault (start with a copy, graduate to live after a week
   of clean diffs); file everything into the punch list with severity.
3. Weekly: re-run the interop check — open the vault in Obsidian, confirm
   clean `git diff` / sync behavior after a Corbomite session.

## Release criteria (first public cut)

- [ ] Phases 0–3 complete; Phase 4 canvas-edges + graph-persistence + tags pane done
- [ ] Phase 5 legal items + dead-code purge + renames done; no `#if 0` corpse files; no `TODO(port-…)` markers
- [ ] 2+ weeks of live-vault dogfooding without a data-loss or interop incident
- [ ] Obsidian round-trip: a week of alternating sessions produces no *unexplained* diffs (blank-line-run normalization is an **expected, documented** diff — Markoff B1 §2; see [`docs/handoff/2026-06-10-blank-line-collapse-triage.md`](../../handoff/2026-06-10-blank-line-collapse-triage.md))
- [ ] Docs: CLAUDE.md/PARITY-MATRIX current; README accurate (deps, third-party, build); LICENSE present
- [ ] Eyeball backlog empty; 251/251 tests; zero disabled-but-visible menu actions
