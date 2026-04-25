# Cluster V.2 — Editor/Workspace Debt Cleanup (SCOUTING)

**Type:** Scouting doc. Expand to full plan after Cluster V lands.

**Parent cluster:** [Cluster V](2026-04-20-cluster-v-editor-workspace-ui-surfacing-SCOUTING.md) + [V spec](../specs/2026-04-20-cluster-v-editor-workspace-ui-surfacing-design.md).

**Motivation.** Cluster V was scoped *surface-first* — user-visible wins first, non-visible plumbing deferred. V.2 holds everything V deferred so nothing slips through the cracks. User confirmed 2026-04-20: "happy to pick (a) so long as nothing slips through the cracks for when we get around to what's relegated to later clusters."

**Estimate:** 4–5 days once expanded.

---

## Scope (rough phasing — revisit on expansion)

### V.2 Phase 1 — Fold gutter click-to-fold

`libs/markoff-family/libs/markoff/src/FoldGutter.cpp:70` — `paint()` returns early with TODO about coordinator wiring. Mouse-event dispatch is already there (`mousePressEvent` lines 118–144). The gap is **Markoff-internal**: the coordinator that connects heading positions to paint-target cells isn't wired from `Editor` into `FoldGutter` at mount time.

Cluster V's scouting doc incorrectly claimed this blocks on Qutepart fork Phase 6. It does not — fork Phase 6 is about removing bundled themes. Fold gutter is entirely a Markoff-internal question.

Steps sketched:
- Trace `Editor::setupFoldGutter` (or equivalent) to find where `FoldCoordinator` (or the heading-index source) should be handed to `FoldGutter`.
- Complete `FoldGutter::paint()` to iterate headings via coordinator and paint arrows.
- End-to-end test: synth mouse press on gutter arrow → assert fold state toggles.

### V.2 Phase 2 — VaultConfig writer routing

`libs/storage/include/corbomite/storage/VaultConfig.h` declares 6 writers, none called:

- `writeAppJson` ↔ `readAppJson` (`.obsidian/app.json`)
- `writeAppearanceJson` ↔ `readAppearanceJson` (`.obsidian/appearance.json`)
- `writeCommunityPlugins` ↔ reader (`.obsidian/community-plugins.json`)
- `writeHotkeys` ↔ reader (`.obsidian/hotkeys.json`)
- `writeDailyNotesJson` ↔ reader (`.obsidian/daily-notes.json`)
- `writeTemplatesJson` ↔ reader (`.obsidian/templates.json`)

Each needs to be routed from its matching `SettingsDialog` page's apply-handler. Must use **merge-unknown-keys** pattern (reuse approach from Cluster S Bookmarks `.obsidian/bookmarks.json` round-trip) so user-authored-in-Obsidian keys aren't clobbered.

Steps sketched:
- Formalise the merge-unknown pattern as a small helper in `VaultConfig` (takes known-key list + updates).
- For each writer: identify its SettingsDialog page, add the apply-handler connect.
- Test round-trips with unknown-key preservation for each file.

### V.2 Phase 3 — Persisted metadata cache loader

`libs/storage/include/corbomite/storage/CachedMetadataStore.h` — `loadInto(MetadataCache&)` + `saveFrom(MetadataCache&)` are implemented, have zero callers.

Hookup:
- `MainWindow::openVault` → `CachedMetadataStore::open(dbPath)` → `loadInto(cache)` before `MetadataWorker::scanVault`.
- `MainWindow::closeVault` (or equivalent) → `saveFrom(cache)` → close DB.
- Handle stale-cache case: scan still runs but uses cached timestamps to skip unchanged files.

Steps sketched:
- Measure cold-start scan time on a ≥1000-note vault before/after wiring.
- Implement startup/shutdown hooks.
- Cache-invalidation strategy: compare vault's `.obsidian/app.json` mtime or a vault fingerprint.

### V.2 Phase 4 — Autosave delay setting

`src/reactors/AutosaveReactor.cpp` — `setDelayMs()` implemented, never called. Hardcoded 2000ms default.

Steps sketched:
- Add a spinbox to SettingsDialog → Editor page (reuse `Editor/AutoSaveDelayMs` kcfg key already defined).
- Hook into `MainWindow::onSettingsApplied()` (added in Cluster V) so the `applyAutosaveDelay()` helper reads the kcfg value and calls `setDelayMs()`.
- Default 2000ms; range 500–30000ms.

### V.2 Phase 5 — LRU reopen upgrade (optional)

Current `TabModel::reopenLastClosed()` is single-LIFO. Obsidian behavior is multi-entry recent-close list. Low priority — most users reopen one tab.

Decision point at expansion: ship the upgrade, or keep single-LIFO and close the item. Depends on user demand.

### V.2 Phase 6 — Dead-code audit pass

After V + V.2 Phases 1–5 land:

- Re-run a focused dead-code grep over `src/app/`, `libs/markoff-family/`, `libs/readingview/`, `libs/core/`, `libs/storage/`, `libs/models/`.
- Delete remaining fully-dead public methods.
- Prune `corbomite.kcfg` keys that no Settings page surfaces *and* no code reads.
- Update SHARED-SYMBOLS.md if any public API is removed.

Corresponds to Phase 8 of the original Cluster V scouting doc.

---

## Blockers / prerequisites

- **Cluster V must land first.** V.2 builds on infrastructure V introduces (`MainWindow::onSettingsApplied`, dispatcher pattern).
- **Merge-unknown-keys helper pattern** — Cluster S shipped the first instance (bookmarks.json round-trip). Before Phase 2, extract to `VaultConfig` helper.
- **Cluster U File Explorer** — may touch VaultConfig's file/folder state writers; coordinate ordering if U is in flight simultaneously.

---

## Out of scope (permanently deferred — do not regrow)

- Plugin-API dead methods (`VaultProxy::modifyBinary/append/create/createFolder/trash/remove/on/off`, advanced `FileManagerProxy`, `SearchProxy` link/tag queries, `secrets()/process()/network()` proxies). Per user direction 2026-04-20.
- Bases discoverability — belongs to Cluster M audits.
- Canvas/graph affordances — Cluster W.

---

## Expansion triggers

Expand V.2 to a full plan when:

1. Cluster V is landed and retro is written.
2. User confirms V.2 is the next cluster (vs Cluster S Bookmarks expansion, Cluster U File Explorer expansion, Cluster M audits, or a new incoming priority).
3. Merge-unknown-keys helper pattern is decided (extract from Cluster S precedent).
