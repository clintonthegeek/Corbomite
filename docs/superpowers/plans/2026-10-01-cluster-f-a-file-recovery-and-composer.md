# Cluster F-a — File Recovery & Note Composer

**Date:** 2026-10-01
**Type:** Full plan (split out of the Cluster F stub, items 7 + 8)
**Status:** **Plan written 2026-10-01 — ready to dispatch. Not started.**
**Sizing:** ~5–8 days of implementation across four phases (P0 ~1d, P1 ~2d,
P2 ~1d, P3 ~1–3d). Self-contained: **no substrate dependency, no cross-repo
(Markoff) dependency, no other cluster blocked.**

**Why this exists:** items 7 and 8 were named only inside the never-expanded
Cluster F stub
([`2026-04-26-cluster-f-internal-plugin-gap-fill.md`](2026-04-26-cluster-f-internal-plugin-gap-fill.md),
now split). The second-sweep appraisal filed them as a single punch-list item
(`[plugin][P2]`, punch-list 2026-10-01 §F) with the note that they are "core
Obsidian expectations" and "~5–8 days combined, self-contained". That is this
plan. Items 9 + 10 go to
[F-b](2026-10-01-cluster-f-b-workspaces-and-sidedock.md).

---

## 1. Goal

Two core Obsidian behaviours that Corbomite does not have:

1. **File Recovery** — unsaved work survives a crash or force-kill. Today a
   force-killed session loses everything typed since the last successful save,
   because the only recovery write fires *after a save has already failed*.
2. **Note Composer** — "Merge entire file with…", a two-stage modal that
   moves a note's whole body into another note (append or prepend) and trashes
   the source. Today this does not exist at all.

Both are independent of each other and of every other open work item; both
land on existing, already-built seams.

---

## 2. Current-state audit (verified 2026-10-01 against the tree)

### 2.1 File Recovery — the destination is right, the *trigger* is missing

| Fact | Evidence |
|---|---|
| `TextFileView::writeBackup` is private and called from exactly **one** place | `libs/core/src/TextFileView.h:42` (declaration), `libs/core/src/TextFileView.cpp:137` (definition) |
| That one call site is inside `if (!success)` — the save-**failure** branch | `libs/core/src/TextFileView.cpp:67-71` — `if (!success) { m_lastSavedData = previousLastSaved; writeBackup(currentData); Q_EMIT saveError(...); }` |
| There is therefore **no** snapshot of a dirty-but-not-yet-failing document | the same function's success branch only sets `m_dirty = false` and emits `saved()` (`:72-75`) |
| Debounced auto-save is a **2 s** timer writing straight to the vault | `libs/core/src/TextFileView.h:52` — `static constexpr int SaveDebounceMs = 2000;` |
| Backup destination is deliberately **outside the vault**, keyed per vault | `libs/core/src/TextFileView.cpp:137-165` — `QStandardPaths::AppLocalDataLocation` + `/file-recovery/` + `PathUtils::vaultId(m_vaultRoot)` + `<basename>-<ISO ts>.md` |
| That out-of-vault decision is a closed, deliberate call, not an accident | punch-list:276 (`[x]`, 2026-04-27 P2 sweep) and `decisions-archive.md` 2026-04-27 — writing into `<vault>/.obsidian/file-recovery/` made recovery copies show up in the tree, search, and graph, and re-trigger `Vault::modified` for the leaf that just failed |
| A **disabled** "Version history" menu slot already exists and is the exact activation seam | `libs/core/src/EditableFileView.cpp:117,136` (`i18n("Version history")`, `setEnabled(false)`); asserted by `tests/core/tst_editable_file_view_menu.cpp:243-264`, which today **requires** it to be disabled |
| Its section is `view.linked`, per Obsidian's menu-order protocol | `docs/superpowers/plans/archive/2026-04-19-cluster-r-view-header-menus.md:2516-2518` |
| Obsidian's own behaviour is fully specified, including the on-disk shape | [`docs/obsidian-audit/addenda/2026-04-19-file-recovery-plugin.md`](../../obsidian-audit/addenda/2026-04-19-file-recovery-plugin.md) — 60-minute default interval, Settings → File Recovery, `maxSnapshotAge` 7 d, `maxSnapshotsPerFile` 50, Version History modal with preview + restore, save-failure entries flagged `{"failure": true}` and **exempt** from pruning |
| Cluster R deliberately parked this as a disabled placeholder pending "Cluster T" | `docs/superpowers/plans/archive/2026-04-19-cluster-r-view-header-menus.md:3768` — that letter was never assigned; this cluster is its successor |

**⚠ One addendum claim must NOT be copied verbatim.** The addendum puts the
backup store at `<vault>/.obsidian/file-recovery.json`. Corbomite's
`.obsidian/` is inside the watched/indexed vault, which is precisely why the
2026-04-27 sweep moved backups out. **Decision for the user (Q1 below):
keep the existing out-of-vault location, or move to a single
`file-recovery.json` store outside the vault?** The addendum's *semantics*
(interval, pruning, flags, modal) are still normative; only the path is in
question.

**And Obsidian's own path is not agreed on by the audit corpus**, which is
worth knowing before treating any of it as a format contract:
`views.md §187-188` says the store is `.obsidian/plugins/file-recovery/` with
`data.json` "or similar", the 2026-04-19 addendum says
`.obsidian/file-recovery.json`, and `audit-2026-04-26/core-and-addenda.md`
item 15 says per-file `.md` files under `.obsidian/file-recovery/`. Three
documents, three shapes. Treat the *location* as unknown and the *behaviour*
as the spec.

### 2.2 Note Composer — absent

| Fact | Evidence |
|---|---|
| Zero code | `grep -rni 'composer\|mergeFile' src/ libs/` (excluding `libs/markoff-family/.worktrees/`) returns no Corbomite hits |
| The required primitive partly exists: `FileManager::createNewFile` is generic and generic-content-capable | `libs/vault/include/corbomite/vault/FileManager.h:71` |
| A read-modify-write helper with trash policy is available | `FileManager` (`createMarkdownNote`, `promptForFileRename`, trash option plumbing already used by `TextFileView`/`Workspace`) |
| A reusable fuzzy file picker exists and is the right shape for stage 1 | `src/dialogs/QuickSwitcher.{h,cpp}` (fuzzy proxy + "no match → create" path at `:180`) |
| A two-field confirm-dialog precedent exists | `src/dialogs/TemplatePicker.{h,cpp}` (used by `MainWindow::insertTemplate`) |
| Full Obsidian spec (both modal stages, frontmatter-stripping rule, append/prepend, trash-after, open-target, link-rewrite caveat) | [`docs/obsidian-audit/addenda/2026-04-19-merge-file-modal.md`](../../obsidian-audit/addenda/2026-04-19-merge-file-modal.md) |

**Note the two addenda disagree on one point worth flagging:** the
merge-modal addendum says source frontmatter is **stripped** in all cases
(§3.2), which it calls "arguably a bug but it's the shipped UX". Corbomite's
own frontmatter utilities are D2/roundyaml-based, so the strip must go through
Corbomite's writer, not a regex — flag as Q3.

### 2.3 What is explicitly *not* missing

Items 1–6 of the Cluster F stub (command palette, quick switcher, daily notes,
templates, page preview, word count) were audited on 2026-10-01 and are
**already shipped as host features**. Their per-item evidence is the table in
the [Cluster F stub banner](2026-04-26-cluster-f-internal-plugin-gap-fill.md).
Three residual gaps are recorded there but are **not** F-a's business and are
deliberately not expanded here:

- Quick switcher has no frontmatter-alias search (`src/dialogs/QuickSwitcherModel.h:36`
  says so in-code). → **punch-list candidate**, not a cluster phase.
- Daily notes have no open-on-startup setting (no key in `src/app/corbomite.kcfg`).
  → one-line punch-list candidate.
- Word count has no character count and is a host label, not a
  `StatusBarRegistry` item. → punch-list candidate.

---

## 3. Scope

### In scope

- Periodic dirty-state snapshotting of open markdown leaves (P0).
- Retention/pruning policy + a Settings page for the interval and retention
  (P1).
- Version History modal: per-file snapshot list, preview pane, restore
  (P1).
- Activating the existing disabled "Version history" menu slot (P1).
- Note Composer: the two-stage modal and its merge pipeline, plus a KAction
  and the menu entry (P2–P3).
- Tests named in §6.

### Out of scope

- **Undelete.** A file trashed by Note Composer is gone from the picker; no
  undelete UI in this cluster.
- **Link rewriting on merge.** Obsidian does not rewrite `[[A]]` links after a
  merge and neither will we (addendum §4 — a documented rough edge). Filing
  the follow-up (suggest an `Update links on rename`-style manual step in the
  confirm dialog's text) is in scope as *copy*, not as an implementation.
- **Sync/backup providers.** Local-only.
- **Snapshotting canvas or `.base` leaves.** Text-only leaves only.
- Anything in the Cluster F items 9–10 territory → [F-b](2026-10-01-cluster-f-b-workspaces-and-sidedock.md).

---

## 4. Open questions (answer before dispatch)

- **Q1 — backup store location.** Keep the existing per-file `.md` files
  outside the vault (`AppLocalDataLocation/file-recovery/<vault-id>/`), or move
  to a single `file-recovery.json` index there, mirroring Obsidian's shape?
  Recommendation: **keep out-of-vault, and keep one file per snapshot** — the
  existing test (`tst_textfileview::saveFailureWritesBackup`) already exercises
  that path through the `MemoryAdapter`, and per-file snapshots avoid rewriting
  a whole-vault JSON on every tick. Obsidian's single-JSON store is an
  implementation detail of its storage layer, not a format contract.
- **Q2 — snapshot cadence trigger.** (a) A host-level `QTimer` that walks
  dirty `TextFileView`s; (b) a timer per leaf. Recommendation: **(a)** — one
  timer, one place to make it vault-aware, and it composes with the existing
  debounce rather than racing it.
- **Q3 — frontmatter strip on merge.** Strip via Corbomite's frontmatter
  reader/writer, or reject merging notes that have frontmatter? The addendum
  says strip. Recommendation: strip, but *tell the user* in the confirm dialog
  when frontmatter will be discarded.
- **Q4 — restore semantics.** Restoring a snapshot writes the file on disk and
  reloads the leaf. Should the restore itself be undoable through the existing
  three-way-merge path (`TextFileView::onExternalModify`), or should it be a
  hard overwrite with a "a snapshot was just written to disk" safety copy?
  Recommendation: hard overwrite **plus** a snapshot of the pre-restore content,
  so restore is itself recoverable.

---

## 5. Work breakdown

### Phase P0 — Periodic dirty snapshot (the actual data-loss fix)

- [ ] **P0.1** Host-level snapshot timer. A `QObject` owned by `MainWindow`
      (or a small `RecoveryService` in `libs/core` if that is where the seams
      land), interval from settings, default 60 min (addendum §1). Started on
      vault open, stopped on vault close. **Never** write a vault the user is
      not inside.
- [ ] **P0.2** Dirty-leaf walk. Query the workspace for live `TextFileView`
      leaves, take `getViewData()` from the **dirty** ones only, and skip any
      leaf whose content equals what was last snapshotted. Add a
      `TextFileView::isDirty()` accessor if `m_dirty` is not already
      reachable (`libs/core/include/corbomite/core/TextFileView.h:45`).
- [ ] **P0.3** Reuse `writeBackup` rather than adding a second writer. It
      already does the out-of-vault + per-vault-id path
      (`libs/core/src/TextFileView.cpp:137-165`); promoting it from private to
      a snapshot entry point is the smallest change that preserves the
      2026-04-27 decision. Confirm the timestamp granularity is enough to keep
      multiple snapshots per minute (it is: `Qt::ISODate`).
- [ ] **P0.4** Mark periodic snapshots distinctly from save-failure backups so
      retention pruning (§P1.2) can tell them apart. Obsidian's discriminator is
      a `{"failure": true}` flag; the file-per-snapshot equivalent is a naming
      convention — decide in Q1.
- [ ] **P0.5** Corrupt/partial-write safety: write to a temp name then rename,
      so a crash *during* a snapshot cannot leave a truncated recovery copy.
- [ ] **P0.6** Guard against the failure mode that motivated the 2026-04-27
      decision: assert in a test that no snapshot path is inside the vault
      root. This is the invariant the previous bug violated.
- [ ] **P0.7** `tst_recovery_snapshot` — new test binary under `tests/core/`.

### Phase P1 — Retention, settings, Version History modal

- [ ] **P1.1** Settings page (File Recovery section): interval, max age,
      max snapshots per file. Keys into `src/app/corbomite.kcfg` (that is the
      live settings schema file; `corbomitesettings.kcfgc` holds the page
      labels).
- [ ] **P1.2** Pruning pass, run on snapshot write: drop snapshots past the age
      or count ceiling, **never** dropping failure-flagged ones (addendum §2).
- [ ] **P1.3** Prune a file's snapshots when the file leaves the vault (deleted
      or renamed) — Obsidian removes the entry immediately (addendum §2).
- [ ] **P1.4** Enumerate snapshots for one file, newest-first, with a
      human-readable timestamp.
- [ ] **P1.5** Version History modal: list + preview pane + "Restore this
      version". Use `StyledRenderEngine` for the preview (it renders headlessly;
      see `libs/core/include/corbomite/core/StyledRenderEngine.h`) so the modal
      matches the hover popover.
- [ ] **P1.6** Restore action per Q4, including the pre-restore safety copy.
- [ ] **P1.7** **Activate the disabled menu slot.** Replace the
      `setEnabled(false)` "Version history" action at
      `libs/core/src/EditableFileView.cpp:136` with a real handler, and
      **update `tests/core/tst_editable_file_view_menu.cpp:243-264`**, which
      today asserts the slot is disabled. (Tests define expected behaviour —
      this one encodes a placeholder state that is deliberately ending.)
- [ ] **P1.8** Live-eyeball requirement (see §7) for the modal and restore.
- [ ] **P1.9** `tst_version_history` — snapshot listing order, pruning,
      failure-entry exemption, restore round trip.

### Phase P2 — Note Composer: the modal

- [ ] **P2.1** `MergeFileDialog` (two stages in one dialog class, or two
      widgets — the addendum draws two): stage 1 fuzzy file picker listing all
      `.md` files **except the source**, stage 2 confirm with append/prepend
      radio buttons and the destructive-consequence text.
- [ ] **P2.2** Reuse rather than reinvent: the fuzzy proxy/filter machinery
      from `src/dialogs/QuickSwitcher.cpp` and the confirm-dialog shape from
      `src/dialogs/TemplatePicker.cpp`. Do **not** enable QuickSwitcher's
      create-on-no-match behaviour here.
- [ ] **P2.3** i18n all strings; `QIcon::fromTheme` for icons; KDE button
      placement (`QDialogButtonBox` with a `Merge` accept role).
- [ ] **P2.4** `tst_merge_file_dialog` — stage-1 list excludes the source,
      filter works, append/prepend toggle round-trips, Cancel leaves both
      files untouched.

### Phase P3 — Note Composer: the pipeline + surface

- [ ] **P3.1** Merge pipeline, in addendum §3 order: read source → strip
      frontmatter per Q3 → RMW into target → trash source → open target in the
      active leaf. Do it atomically (target write first, trash second) so a
      crash between the two leaves a duplicated body rather than data loss.
- [ ] **P3.2** Respect the existing trash-option setting rather than always
      trashing.
- [ ] **P3.3** KAction + menu entry in the markdown hamburger's `action`
      section (addendum §1: section `action`, not `view.linked` — confirm
      against `docs/obsidian-audit/domains/ui-bundle.md §4` section-order
      before wiring).
- [ ] **P3.4** Widen through Cluster O's provider mechanism rather than adding
      a `MainWindow` branch: the action belongs to the markdown view provider
      (`MarkdownViewActions`), consistent with Cluster O doctrine §D1–D7. No
      new leaf-typed dispatch in `MainWindow`.
- [ ] **P3.5** The link-rewrite caveat as **copy** in the confirm dialog.
- [ ] **P3.6** `tst_merge_file_pipeline` — append and prepend byte-exact,
      frontmatter stripped, source trashed, target opened, target untouched on
      cancel, and no action when the target is the source.
- [ ] **P3.7** Live-eyeball requirement (see §7).

---

## 6. Named tests

| Test | Binary | Covers |
|---|---|---|
| `tst_recovery_snapshot` | new, `tests/core/` | dirty-leaf walk, dedup-against-last-snapshot, no-write-when-clean, out-of-vault path assertion (P0.6), temp-file+rename |
| `tst_recovery_pruning` | new, `tests/core/` | age + count ceilings, failure-entry exemption, vault-exit purge |
| `tst_version_history` | new, `tests/core/` | listing order, preview render, restore round trip, pre-restore safety copy |
| `tst_editable_file_view_menu` | **existing**, `tests/core/` | updated: the "Version history" slot is no longer expected to be disabled |
| `tst_textfileview` | **existing**, `tests/core/` | unchanged — `saveFailureWritesBackup` must keep passing; it is the regression guard for the 2026-04-27 decision |
| `tst_merge_file_dialog` | new, `tests/ui/` (or `tests/dialogs/`) | both modal stages, filtering, destructive-copy |
| `tst_merge_file_pipeline` | new, `tests/integration/` | append/prepend byte-exact, frontmatter strip, trash-then-open ordering, cancel/no-op paths |

Falsifiability note (the lesson from the 2026-10-01 batches): each of these
must be **seen red** against the unfixed code before it is trusted. P0.6 in
particular is a guard on a *decision*, not on behaviour — if it passes before
P0 lands, the guard is not testing what it claims.

---

## 7. Definition of done

- [ ] Force-kill the app mid-edit; on restart the unsaved content of every
      open leaf is recoverable from the Version History modal, with no manual
      file hunting.
- [ ] Snapshot path is verifiably outside the vault root (P0.6 green, and
      confirmed by eye in a real vault: no recovery files appear in the file
      tree, search results, or the graph).
- [ ] Retention behaves: a vault left open for a day holds a bounded number of
      snapshots; save-failure backups are never pruned.
- [ ] "Version history" is enabled, opens the modal from the hamburger, and
      restores.
- [ ] "Merge entire file with…" completes a real merge: body moves, source is
      in the trash, target opens. Cancel is a true no-op.
- [ ] No new leaf-typed dispatch in `MainWindow` (Cluster O doctrine); no
      `grep` hit for a new `qobject_cast` on the markdown view.
- [ ] `cd build-dev && QT_QPA_PLATFORM=offscreen ctest -E "benchmark|perf" --output-on-failure -j 10`
      green, and every new test has been seen red once.
- [ ] **Live eyeball pass done by a human** for P1.8 and P3.7 — modal
      rendering, restore, and the destructive merge cannot be validated
      offscreen. This is standing project policy for UI/interaction work (same
      gate Cluster L Phase L4 D1/D4 passed), not a formality.
- [ ] Punch list updated: the `[plugin][P2]` File-Recovery/Composer item and the
      older `periodic crash-recovery snapshot` item (`punch-list.md:393`) both
      marked `[x]` with commit shas.

---

## 8. Audit references

- [`docs/obsidian-audit/addenda/2026-04-19-file-recovery-plugin.md`](../../obsidian-audit/addenda/2026-04-19-file-recovery-plugin.md) — full File Recovery spec (read §1–§3 before P0).
- [`docs/obsidian-audit/addenda/2026-04-19-merge-file-modal.md`](../../obsidian-audit/addenda/2026-04-19-merge-file-modal.md) — full Note Composer spec.
- [`docs/obsidian-audit/domains/views.md`](../../obsidian-audit/domains/views.md) §187-188 — the `file-recovery` plugin named as the save-failure backup destination.
- [`docs/audit-2026-04-26/core-and-addenda.md`](../../audit-2026-04-26/core-and-addenda.md) §"File Recovery" — the original finding.
- [`docs/audit-2026-04-26/plugin.md`](../../audit-2026-04-26/plugin.md) §"Internal-plugin inventory" — `file-recovery` and `note-composer` both MISSING; still true.
- [`docs/audit-2026-04-26/views.md`](../../audit-2026-04-26/views.md) §"Top suspected bugs" — the `writeBackup`-inside-the-vault finding that P0.6 now guards against.
- [`docs/cluster-retros/cluster-r.md`](../../cluster-retros/cluster-r.md) + the archived [Cluster R plan](archive/2026-04-19-cluster-r-view-header-menus.md) — the disabled "Version history" slot P1.7 activates.

## 9. Blocks / enables

- **Enables:** nothing blocked. Safe to pick up cold.
- **Does not touch:** the editor substrate, the workspace tree, the plugin
  ABI. Nothing in Cluster E, F-b, K, L, N, or O is a prerequisite or a
  consumer.
- **Precedent worth reusing:** Cluster O's per-view-type action providers (so
  P3.4 does not reopen the flat-`MainWindow` mechanism Cluster O deleted).
