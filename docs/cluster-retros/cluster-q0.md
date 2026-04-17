# Cluster Q.0 — Vault architecture refactor (retrospective)

**Landed:** 2026-04-16 → 2026-04-17 across 11 phases. ~50 commits.

Collapses the three-way Vault split (`Corbomite::Vault` stub in libs/core
+ `VaultModel` in libs/models + `VaultService` in src/app) into a single
`libs/vault/` library with:

- `TAbstractFile` / `TFile` / `TFolder` value-handle types
- `Vault` aggregate (load/unload, tree queries, read/write API,
  config-dir JSON I/O, NoteDocument lifecycle, watcher + echo
  suppression, signals)
- `FileManager` (link-aware rename, frontmatter mutation, new-file
  placement, attachment placement, trash routing)
- `VaultProxy` + `FileManagerProxy` (permission-gated plugin facades)
- `Plugin` / `PluginContext` / `PluginManager` /
  `PluginPermissionGrantDialog` (moved from libs/core/ during P9)

Plan at `docs/superpowers/plans/2026-04-16-cluster-q0-vault-architecture.md`.
Spec at `docs/superpowers/specs/2026-04-16-vault-architecture-design.md`.

## Phase landing order

| Phase | Date | Scope | Commits (representative) |
|---|---|---|---|
| 1 | 2026-04-16 | libs/vault scaffold + TFile/TFolder + skeletal Vault; Task-7 path-only Vault stub deleted | `d89374a` · `e4b61ed` · `490ed6d` · `445a45a` · `c6e8562` · `526426c` · `132ae68` |
| 2 | 2026-04-16 | DataAdapter-driven buildTree; VaultScanner + FileWatchReactor folded into libs/vault as `detail::Watcher`; 5 signals + echo-suppression ledger | `43f4cba` · `e3cf73c` · `49fd3e5` · `c2cc907` · `42499e8` |
| 3 | 2026-04-16 | Vault mutation API (read/modify/process/create/rename/remove/copy/trash); absorbed VaultProcess + VaultTrash | `1abcf01` · `fb23e3b` · `c746997` · `c0790b3` |
| 4+5 | 2026-04-16 | Vault configDir/readConfigJson/writeConfigJson; complete FileManager (renameFile with link rewrite, processFrontMatter, new-file placement, attachments, link generation, trash) | `6e1b646` · `515bf88` · `241019b` |
| 6 | 2026-04-17 | Sidebar-panel migration wave 1 — OutlinksPanel + LocalGraphPanel (cascaded through GraphDataBuilder / GraphViewTab / GraphView) + PropertiesPanel | `93ade3e` · `070f24f` · `b2c8121` |
| 7 | 2026-04-17 | Consumer migration wave 2 — NoteEditorWidget / VaultResourceProvider / QuickSwitcher / NotesTreeModel / MetadataCache subscribes to Vault signals / TextFileView external-modify / DailyNoteService + TemplateService | `b3e4f70` · `453f074` · `bd0f82c` · `d2458aa` · `4f94f1a` |
| 8 | 2026-04-17 | App-level reshape — VaultService deleted, NoteService dissolved into VaultModel, RecentVaults helper extracted, CorbomiteApp owns vault lifecycle | `e52c89c` · `c1cc26f` · `cfe975a` · `22e4637` |
| 10 | 2026-04-17 | VaultModel + FrontMatterWriter deleted; NoteDocument lifecycle moves to Vault; path-based file ops move to FileManager; echo-suppression bug fixed | `e991d4f` · `23085f5` · `38eb065` · `83a271f` |
| 9 | 2026-04-17 | Plugin proxy layer rebuilt — VaultProxy + FileManagerProxy + PluginContext rewire + plugin-system co-location in libs/vault/ | `4487e40` · `646524b` · `f78d0ba` |
| 11 | 2026-04-17 | Close-out — Cluster Q plan retargeted onto new proxies (T11.1); this retro + PROJECT-STATE + memory update (T11.3) | `02f8898` + this commit |

Phase 10 landed before Phase 9 by design — moving proxies onto a
post-VaultModel canonical Vault was cleaner than building proxies
against a transitional tree.

## What changed vs the original plan

Mostly faithful. Seven notable deviations during execution:

1. **`m_fileMap` uses `std::unordered_map` not `QHash`** (Phase 1). Qt
   6's QHash requires copy-constructible values during rehash; the map
   holds `std::unique_ptr<TAbstractFile>`, which is move-only. Custom
   `qHash`-backed hasher bridges Qt and std.

2. **`FileStat` reused from libs/storage** rather than redefined in
   libs/vault (Phase 1). The existing type in `DataAdapter.h` had
   richer fields (exists/isDirectory/isFile + size + mtime + ctime);
   no reason to duplicate.

3. **`PathNormalization` namespace is `Corbomite::VaultPaths` not
   `Corbomite::Vault::Paths`** (Phase 1). The plan's name would have
   collided with the `Vault` class.

4. **`m_pendingDelete` is `std::vector` not `QVector`** (Phase 2). Same
   move-only-value-type constraint as `m_fileMap`.

5. **NoteService dissolved into VaultModel not FileManager** (Phase 8
   T8.4, reversed in Phase 10). The plan said "fold NoteService into
   FileManager" but `libs/vault` is downstream of `libs/models` in the
   build graph, so FileManager couldn't reach VaultModel's
   `NoteDocument` cache without inverting the dep direction. Co-located
   the note-ops on VaultModel temporarily; Phase 10 moved them to
   their real homes (NoteDocument lifecycle on Vault where the TFile
   tree lives, path-based file ops on FileManager where TFile*-shaped
   equivalents already existed).

6. **`MetadataCache::backlinksFor` doesn't exist** (Phase 5). The plan
   assumed a backlinks lookup method on MetadataCache; reality had
   only `allPaths()` + per-cache link/embed iteration. FileManager's
   rename link-rewrite walks the cache manually — O(N) per rename is
   acceptable at current vault sizes.

7. **Plugin-system co-location in Phase 9 grew to four types, not
   one.** The plan said "simplest: move PluginContext into
   libs/vault". The actual move needed `Plugin` + `PluginContext` +
   `PluginManager` + `PluginPermissionGrantDialog` because
   `PluginManager.cpp` calls `new PluginContext(...)` (requires the
   full definition) and splitting the class across libraries would
   still need `libs/core` to reach `libs/vault`. `PluginMetaData`
   stayed in libs/core/ — no libs/vault deps; cleanly reusable.

## What surprised

- **Phase 8's echo-suppression bug fixed itself in Phase 10.** When
  Phase 8 temporarily hosted `saveDocument` on VaultModel, saves wrote
  through the adapter without stamping the self-write ledger, so the
  watcher fed the save back into `TextFileView::onExternalModify`.
  Phase 10 moved `saveDocument` to Vault, where it naturally routes
  through `Vault::modify` → `stampSelfWrite`. The bug dissolved — no
  dedicated fix commit needed.

- **Phase 9 done after Phase 10.** The plan numbered Phase 9 before
  Phase 10 but we deliberately reordered because Phase 10 retired
  VaultModel entirely, giving Phase 9's proxy layer a cleaner
  substrate. No rework; the swap just made Phase 9 shorter.

- **The `VaultReader`/`VaultWriter` proxies from Cluster Q Tasks 1–6
  were never production-wired.** They shipped in Tasks 1–6 with stub
  forwarding, and Phase 1 of Q.0 deleted them along with the
  Task-7 `Corbomite::Vault` stub. The end state is cleaner than if the
  old proxies had accumulated consumer code that now needed
  rewriting.

- **Config dir flip from `.corbomite/` to `.obsidian/`.** Phase 10's
  `Vault::configDir()` default lines up Corbomite with Obsidian's vault
  format; this is the final pre-parity architectural detail on the
  vault layer.

## Downstream effects

- **Cluster Q (Tasks 7–12) unblocked** — proxy infrastructure is
  complete and documented; Task 7 becomes a no-op pointer to Q.0 P9.
  The Cluster Q plan at
  `docs/superpowers/plans/2026-04-16-cluster-q-internal-plugin-wrapping.md`
  has been retargeted per T11.1.

- **Every plugin-facing codepath imports from `corbomite/vault/`** for
  Plugin / PluginContext / PluginManager / PluginPermissionGrantDialog.
  `corbomite/core/Plugin*.h` and `VaultReader`/`VaultWriter` are gone;
  any lingering references outside `docs/` are bugs.

- **Vault/VaultModel tension resolved.** The open question at
  `PROJECT-STATE §"How should Corbomite::Vault (Cluster Q proxies)
  relate to Corbomite::VaultModel?"` is answered by the Q.0 spec:
  canonical `Corbomite::Vault` in libs/vault/ owns the whole vault
  layer; VaultModel is gone; `VaultProxy` is a permission-gated
  facade over it with signal/cache behaviour coming for free.

- **`src/reactors/` holds only AutosaveReactor now.** FileWatchReactor
  moved into `libs/vault/src/Watcher.cpp` as a private implementation
  detail; FrontMatterWriter deleted. The reactors folder survives
  narrowly but is no longer the external-filesystem integration
  pattern for the app.

## Lessons for the next cluster

- **Reordering phases to land the cleanup first pays off.** Phase 10
  shipped before Phase 9 because doing the cleanup first (retire
  VaultModel) produced a cleaner canonical substrate for the proxy
  work. Default assumption for future multi-phase clusters: if a later
  phase is a cleanup that removes transitional noise, consider
  landing it early — it makes the remaining phases smaller.

- **Library-stability-first remains right.** The
  `feedback_plugin_api_stability.md` rule held throughout Q.0 — we
  shaped `libs/vault` internally (Phases 1–8, 10) before freezing its
  plugin-facing surface (Phase 9). Had we built the proxies against a
  moving Vault target, every Phase 1–10 internal API change would
  have been a plugin-ABI break. Apply to every future
  plugin-API-adjacent cluster.

- **"Obvious" dep-graph assumptions need checking.** Phase 8 rewired
  NoteService onto VaultModel because libs/vault being downstream of
  libs/models broke the plan's assumption. Phase 10's larger move
  (NoteDocument onto Vault, path-based file ops onto FileManager)
  matched the graph again. Before dispatching a "fold X into Y"
  instruction, confirm the library directions support it.

- **Pure-doc close-out phases are worth the cost.** Phase 11's two
  doc commits (Cluster Q plan retarget + this retro/PROJECT-STATE
  update) took ~30 minutes total. The alternative — expecting a
  future Cluster Q execution session to rediscover why VaultReader
  references don't build — would be several hours of confusion.
  Default assumption: every multi-phase cluster should end with a
  close-out phase that either updates downstream plans or writes a
  retro.

## Open questions resolved

- ~~**How should `Corbomite::Vault` (Cluster Q proxies) relate to
  `Corbomite::VaultModel`?**~~ Resolved 2026-04-17 by the Q.0 spec and
  its 11-phase execution. Canonical Vault + FileManager live in
  `libs/vault/`; VaultModel is deleted; `VaultProxy` +
  `FileManagerProxy` facade it with permission gating. Plugin writes
  get signals + cache behaviour + echo-suppression for free.
