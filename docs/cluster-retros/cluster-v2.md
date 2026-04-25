# Cluster V.2 — Editor/Workspace debt cleanup (retro)

**Closed:** 2026-04-25
**Plan:** [`superpowers/plans/archive/2026-04-25-cluster-v2-debt-cleanup.md`](../superpowers/plans/archive/2026-04-25-cluster-v2-debt-cleanup.md) + [`scouting`](../superpowers/plans/archive/2026-04-20-cluster-v2-debt-cleanup-SCOUTING.md)
**Companion to:** [Cluster V](cluster-v.md)

## Outcome

V.2 closed the non-user-visible debt that Cluster V deferred under the surface-first framing — `VaultConfig` writer routing, persisted MetadataCache verification, autosave delay applier, and post-V dead-code audit. Five phases shipped across 7 commits (`6f737933..bb12fbbd`); two scope items were retired without code (Phase 5b kcfg orphan sweep found nothing, Phase 5c SHARED-SYMBOLS sweep found zero references). The session's single biggest surprise was Phase 3: the V.2 scouting doc claimed `CachedMetadataStore::loadInto`/`saveFrom` had zero callers, but inspection found them already wired through `MetadataCache::open(dbPath)` invoked from `MainWindow::onVaultOpened` — likely landed silently during Cluster Y absorption. Phase 3 reduced from a wiring task to an end-to-end verification test.

## Phase ledger

| Phase | SHA | Subject |
|---|---|---|
| 1 | `6f737933` | `cluster-v2 phase 1: VaultConfig::mergeJson preserves unknown keys` |
| 2 | `775738b5` | `cluster-v2 phase 2: persist 3 SettingsDialog pages to .obsidian/*.json` |
| 2 fix | `a872dfc6` | `cluster-v2 phase 2 fix: log mergeJson failures in applyVaultPortableSettings` |
| 3 | `b9b3f2a6` | `cluster-v2 phase 3: end-to-end CachedMetadataStore round-trip test` |
| 3 fix | `66c9802e` | `cluster-v2 phase 3 fix: comment typo (Phase 6 → Phase 3)` |
| 4 | `8b317a19` | `cluster-v2 phase 4: wire autosave delay through onSettingsApplied` |
| 5a | `bb12fbbd` | `cluster-v2 phase 5a: delete WorkspaceWindow standalone facade` |

What landed:

- **Phase 1.** `VaultConfig::mergeJson(fileName, updates) const` — generic helper that round-trips an `.obsidian/*.json` file while preserving keys we don't recognise. 3 unit tests for round-trip + create-if-absent + overwrite-known.
- **Phase 2.** `MainWindow::applyVaultPortableSettings()` — when SettingsDialog applies, persist Appearance / Daily Notes / Templates kcfg keys to `.obsidian/{appearance,daily-notes,templates}.json`. Each section guards on non-empty values; bails early if no vault is loaded or if `ensureConfigDir` fails. The follow-up fix added `qWarning` on `mergeJson` failure (toasts deferred to V.future). 2 persistence-layer integration tests.
- **Phase 3.** `tst_cachedmetadatastore_e2e` — proves `MetadataCache::open(dbPath)` + `close()` round-trip survives a real-vault open/populate(via `rebuildVault`)/close/reopen cycle. The wiring itself was already in place; the test closes the verification item.
- **Phase 4.** `MainWindow::applyAutosaveDelay()` — 4-line applier reading `CorbomiteSettings::self()->autoSaveDelayMs()` and forwarding to `m_autosave->setDelayMs(ms)`. Hooked into the `onSettingsApplied()` dispatcher after `applyVaultPortableSettings()`. The dispatcher is now `applyTheme(); applyVaultPortableSettings(); applyAutosaveDelay();` — the future-appliers comment was retired. Test: kcfg-round-trip in `tst_mainwindow_settings_apply`.
- **Phase 5a.** Deleted dead `Corbomite::WorkspaceWindow` standalone QWidget facade — 6 named methods (`widget`, `setWindowGeometry`, `showWindow`, `closeWindow`, `setMaximized`, `serialize`) plus orphan getter (`maximized`), `eventFilter` override, and members (`m_widget`, geometry ints, `m_maximized`). Class shrinks to identity token (`id()`/`setId()`) sufficient for `popoutLeaf` contract. `tests/core/tst_workspace_window.cpp` deleted entirely (option b in the backlog entry); coverage already in `tst_workspace_containers.cpp` + `tst_workspace_popout.cpp`. Test count 291 → 290.
- **Phase 5b skipped.** kcfg orphan sweep found no orphans. The implementer flagged 4 SettingsDialog-only kcfg keys (`LineNumbers`, `LineWrap`, `PromptDelete`, `TabSize`) that aren't orphan but are currently no-op (toggling them has no behavioural effect because no code outside SettingsDialog reads them). Carry-forward to backlog.
- **Phase 5c skipped.** `docs/obsidian-audit/SHARED-SYMBOLS.md` had zero references to any deleted facade method.

## Surprises

- **Phase 3 was already done.** The V.2 scouting doc claimed `CachedMetadataStore::loadInto`/`saveFrom` had zero callers and needed wiring. The audit found `MetadataCache::open(dbPath)` already invokes them via `MainWindow::onVaultOpened` at line 2027 (and symmetrically on close). This wiring landed sometime between the scouting doc and V.2 dispatch (probably during Cluster Y absorption — no specific commit located, but unimportant). V.2 reduced Phase 3 to a verification e2e test rather than a wiring task.
- **Phase 2 had only 3 realistic targets, not 6.** The scouting doc named 6 `VaultConfig` writers. The audit confirmed only 3 (appearance, daily-notes, templates) had matching SettingsDialog pages today. The other 3 (`writeAppJson`, `writeCommunityPlugins`, `writeHotkeys`) lack UI surfaces — wiring them would be premature.

## Carry-forwards

1. **Fold-gutter click-to-fold** — Markoff-internal; deferred for the Markoff QA cycle. Was the original V.2 Phase 1 in the scouting doc; dropped per user request 2026-04-25 because Markoff is fragile and needs user testing.
2. **LRU multi-entry reopen** — kept open in V.2 scouting doc until user testing reveals demand. Per user direction 2026-04-25.
3. **3 unwired `VaultConfig` writers** — `writeAppJson`, `writeCommunityPlugins`, `writeHotkeys`. Each blocked on its UI page existing in SettingsDialog. Will be wired by whichever future cluster adds the matching UI.
4. **Vault-level cache fingerprint** — `.obsidian/app.json` mtime gate as a future cold-start optimisation. File-level mtime checks already short-circuit re-parses, so this is optimisation not correctness debt.
5. **No-op settings keys** — `LineNumbers`, `LineWrap`, `PromptDelete`, `TabSize`: SettingsDialog reads/writes them but no code outside SettingsDialog consumes them. Either wire them or remove from kcfg + UI. Identified during Phase 5b sweep.
6. **`WorkspaceWindow` identity-token review** — Reviewer flagged that post-Phase-5a, the class is a thin wrapper around `QString m_id` that could plausibly be replaced by routing the id directly through `popoutLeaf` and a separate map. Out of scope for V.2; Cluster Z (linked views + active-leaf) is the natural place to re-evaluate.

## Patterns harvested

- **`VaultConfig::mergeJson` is now the canonical primitive for unknown-key-preservation when round-tripping vault config.** Cluster S's bookmarks.json round-trip used a per-item `unknownKeys` map; SessionManager uses a root-level stash; both predate the helper. Future writers should use `mergeJson` directly rather than reinventing the merge.
- **The `MainWindow::onSettingsApplied()` dispatcher pattern (Cluster V) scaled to 3 appliers without strain.** New appliers slot in as one line. The dispatcher is the right home for any future kcfg→runtime applier; resist the urge to wire kcfg keys directly into their consumers.
