# Second-sweep codebase appraisal

**Date:** 2026-10-01 · **Status:** Report / investigation. No code written by the
audit itself. Feeds the punch list, the addenda backlog, and the remediation
sequence in §9.
**Trigger:** ~6-week gap since the last commit (`459c7e7c`, 2026-08-21, v0.2.0),
prompted a full re-inspection of code + docs.
**Method:** two independent exhaustive subagent passes (core libs; app layer) plus
two more (punch-list verification; docs/roadmap consistency), with every
high-severity claim re-verified directly against the tree after the passes
disagreed. See §7 for the corrections that resulted.

---

## 0. Executive summary

**The code is in better shape than the documentation, and better than the test
suite's greenness suggests it is.**

Three findings drive everything else:

1. **The suite's greenness partly certifies nothing.** ~12 subsystems have **zero
   production callers** while carrying **250+ test references** between them. They
   report as covered and healthy in a 329/330 run while shipping no user-visible
   behaviour. A previous, cheaper sweep of the same question reported "one dead
   class"; that was wrong, because it counted header mentions rather than
   production call sites.

2. **`ThemeService` is a `#if 0` stub, and two `[x]` punch-list closures are false
   because of it.** Its `themeChanged` signal is emitted from inside the dead
   block, so the entire theme-propagation chain never fires — including the
   Settings theme dropdown, which renders **empty**, and the `appearance.json`
   read path, which no-ops. Opening an Obsidian-authored dark vault does not
   switch Corbomite to dark.

3. **The largest body of unfinished work is on no list at all.** The
   `2026-06-10-release-hygiene.md` plan — 163 lines, ~30 tasks across Phases A–E —
   is **~7% complete**, has **zero checkboxes**, is not on the punch list, is not a
   cluster, and is referenced only as a "companion doc" from INDEX. It contains the
   legal/licensing work for a project that has tagged two releases with `.deb` CI
   (Phase A) and the AppStream metadata that would make those `.deb`s appear in
   software centres (E.1).

Baseline at audit time: **329/330 offscreen** (`tst_canvas_perf_formatted` red only
under `-j10`; passes standalone — a perf-test-under-load flake, not a defect).

**Structural health is genuinely good** and should not be discounted by the above:
acyclic library layering with a structural guard test, zero TODOs in
save/serialization/workspace/undo paths, a completed editor-substrate retirement,
nine real plugins, and a clean Graffodil rebase.

---

## 1. What is sound

| Area | Evidence |
|---|---|
| **Library layering** | Acyclic DAG: `Core` (leaf: Qt/KF6/KDDW) → `Storage` → `Vault` → `Models`; `Search` a pure leaf; `Bases`/`canvas` above. The `core`↔`storage` cycle cut in Cluster P2 is guarded by `tst_no_library_cycles` (demonstrated red pre-fix). |
| **Retired-substrate retirement** | All four retired substrates are comment-only residue: `Markoff::Live`/QML (2 comments), `Markoff::Reading` (1 fwd-decl + the `MarkdownRenderer` corpse), `qutepart` (a member *name* only), `CanvasLivePreview` (gone). No live code depends on a retired API. |
| **TODO hygiene** | 68 TODOs in Corbomite-owned code; **zero** in save/serialization/workspace/undo paths. Overwhelmingly `TODO(port-foundation-exploration)` retirement receipts. |
| **Plugins** | All 9 are real: `KPluginFactory` registered, `.so` built, permission-gated (`createView()` null-checks rather than half-constructing), menu- or KAction-reachable, each with its own test target. **Zero stubs.** |
| **Canvas rebase (Cluster M)** | Cleanest large refactor in the tree. `CanvasScene.cpp` has a coherent 4-band split (tool assembly → document bridge → intent→`Cmd*` translation → canvas UX), three *documented and justified* divergences from stock Graffodil, and **zero** leftover residue. `libs/bases`: zero dead classes. |
| **Private helpers** | Brace-matched sweep of every anonymous-namespace and file-local type across the five core libs found **zero** unreferenced. |
| **MDI provenance** | `CorbomiteMDI.{h,cpp}` carry three SPDX copyright lines, explicit Kate adaptation note, and a second attribution (ktoolbarhandler.cpp). Release-hygiene D.1's provenance half is satisfied. |

---

## 2. The test-only islands (highest-value finding)

Counted by **production call sites** (not header mentions), with `#if 0` bodies
stripped and `packaging/arch/src/corbomite-0.1.0/` (a 225 MB vendored copy of the
tree — it silently doubles any naive `rg`) excluded.

| Symbol | Location | Prod callers | Test refs |
|---|---|---|---|
| `WorkspaceState` | `libs/storage/{include/corbomite/storage,src}/WorkspaceState.h:34` | **0** | 76 |
| `RegexRenderEngine` | `libs/core/{include/corbomite/core,src}/RegexRenderEngine.h:9` | **0** | 32 |
| `TabModel` | `libs/models/{include/corbomite/models,src}/TabModel.h:20` | **0** | 30 |
| `MarkdownRenderChild` | `libs/core/include/corbomite/core/MarkdownRenderChild.h:24` | **0** | 28 |
| `Hotkey` / `HotkeyFile` | `libs/core/{include/corbomite/core,src}/Hotkey.h:27,47` | **0** | 28 |
| `VaultScanner` + `IgnoreFilter` | `libs/vault/.../VaultScanner.h:16`, `libs/storage/.../IgnoreFilter.h:25` | **0** | 28 |
| `EmbedDepthGuard` | `libs/core/include/corbomite/core/EmbedDepthGuard.h:22` | **0** | 20 |
| `SourceEditor` (+`CursorPos`) | `src/editor/SourceEditor.{h,cpp}` | **0** | 0 |
| `PluginInstance` | `libs/core/include/corbomite/core/PluginInstance.h:28` | **0** | 0 |
| `MarkoffRenderEngine` | `libs/core/include/corbomite/core/MarkoffRenderEngine.h:9` | **0** | 0 |
| `SpatialGrid` | `libs/forcegraph/include/corbomite/forcegraph/SpatialGrid.h` | **0** | 0 |
| `SearchDSL::ParseResult` | `libs/search/include/corbomite/search/SearchDSL.h:9` | **0** | 0 |
| Kate session subtree | `src/mdi/CorbomiteMDI.cpp:1351-1458,1800-1885` | **0** | 0 |

**Consequences worth naming explicitly:**

- **`Hotkey`/`HotkeyFile`** *are* the `hotkeys.json` implementation. The punch-list
  P2 item "hotkeys.json parsed but never applied" is therefore more than a missing
  call site: the entire subsystem is a tested island.
- **`VaultScanner` + `IgnoreFilter`** make `VaultConfig::userIgnoreFilters()`
  (`libs/storage/src/VaultConfig.cpp:278`) unreachable, so the whole
  user-ignore-filters chain is unwired end to end.
- **`RegexRenderEngine`** is doubly dead: its only content source is
  `MarkdownRenderer::renderToHtml`, whose sole out-of-`#if 0` definition is
  `return {};` (`libs/core/src/MarkdownRenderer.cpp:601`).
- **`SourceEditor`** is a fully orphaned qutepart-era shim. Its
  `foldedHeadings()`/`setFoldedHeadings()` round-trip is **faked** — it stores to
  `m_pendingFoldedHeadings` and never folds (`src/editor/SourceEditor.cpp:94-109`).
  Production Source mode is `Markoff::Source::Editor`.
- **The Kate session subtree** (`startRestore`/`finishRestore`/`saveSession` +
  `Sidebar::{startRestoreSession,restoreSession,saveSession}` + 12 dead
  `Corbomite-MDI-*` config keys) is ~110 lines superseded by Cluster L2/L4's
  `SessionManager`. It carries live latent-bug surface nothing exercises.

Release-hygiene C.1–C.5 already name some of these (`MarkdownRenderer`,
`MarkoffAdapters`, `SystemThemeBuilder`, `MarkoffRenderEngine`, `EmbedDepthGuard`,
`TabModel`, `SourceEditor`). The rest are newly surfaced.

---

## 3. Duplication

1. **The tested resolver is dead; the naive one ships.**
   `Corbomite::resolveSubpath` (`libs/core/src/LinkUtils.cpp:83`) is the
   Obsidian-parity implementation — AST-based, handles `[[^fn]]` footnotes and
   heading hierarchy, returns a `SubpathResolution`, **15 tests** — and has
   **zero production callers**. What actually ships is
   `Corbomite::extractMarkdownSubpath` (`libs/core/src/SubpathExtract.cpp:9`), a
   regex string-scrubber, used by all three render engines. Root cause of the
   unwiring is a real design gap, not an oversight: `resolveSubpath` takes a
   `Markoff::Document`, which the render engines do not construct.

2. **Three classes named `VaultResourceProvider`.**
   `Corbomite::Core::VaultResourceProvider` (abstract,
   `libs/core/include/corbomite/core/VaultResourceProvider.h:20`),
   `Corbomite::VaultResourceProvider` (concrete, `src/editor/VaultResourceProvider.h:10`,
   which adds a `resolveTarget()` not on the Markoff interface), and
   `VaultScopedResources` (`src/app/MainWindow.cpp:129`). Both `Corbomite::`
   names are in scope in `MainWindow.cpp`. Release-hygiene D.3 tracks the
   two-class form; the third adapter is untracked.

3. **Public headers exporting private storage internals.**
   `MetadataParser`, `MetadataWorker`, `CachedMetadataStore`, `WorkspaceState`,
   `IgnoreFilter` are all public `include/corbomite/storage/*.h` with 0 external
   consumers. `MetadataCache` is the intended public face. The leak crosses
   libraries: `VaultScanner.h:5` includes a storage header.

---

## 4. `ThemeService` — the single worst item

`libs/core/src/ThemeService.cpp` is `#if 0` from line 7 to 109. The only
`emit themeChanged(...)` — line **104** — is inside that dead block. What links is
the stub at lines 118–137:

```cpp
QString      ThemeService::activeThemeName()  const { return {}; }
QStringList  ThemeService::availableThemeNames() const { return {}; }
void         ThemeService::setActiveThemeByName(const QString &) {}
void         ThemeService::addUserTheme(const Markoff::Theme &) {}
void         ThemeService::refreshSystemTheme() {}
```

Live consequences, all verified:

| # | Consequence | Evidence |
|---|---|---|
| 1 | **Settings "Editor theme" dropdown renders empty and is inert** | `src/dialogs/SettingsDialog.cpp:149` `addItems(availableThemeNames())` on an empty list; `:155` `setActiveThemeByName` is a no-op |
| 2 | **Opening an Obsidian-authored dark vault does not switch Corbomite to dark** | `src/app/MainWindow.cpp:2457-2478` reads `appearance.json`, calls the no-op `setActiveThemeByName("Light"/"Dark")` |
| 3 | **`css-change` plugin event is dead** | punch-list:272 marks it `[x] RESOLVED`; the only trigger is `MainWindow.cpp:1641` bound to a signal that never fires. No other `emitCssChange()` exists. The test passes because it calls the workspace directly. |
| 4 | **`appearance.json` write hook is dead** | punch-list:333 marks it `[x] RESOLVED` on that hook. *Partial mitigation:* a different live writer exists — `applyVaultPortableSettings` (`MainWindow.cpp:2768-2783`) writes `theme` from KConfig. So the write direction is covered; the **read** direction is not. |
| 5 | **Makes the open canvas dark-mode punch-list item not actionable** | that item proposes "update live on `themeChanged`" — there is no signal to subscribe to |

`NoteEditorWidget::applyThemeToAllLeaves()` still works, but only because it calls
`currentTheme()` once eagerly (`NoteEditorWidget.cpp:230`); the live
`currentTheme()` derives light/dark from the app palette, which is a
stopgap, not the theme port.

**Recommendation:** file as **one** P1 item; re-annotate punch-list:272 and :333 as
*partially* closed (wiring correct, source dead) rather than leaving clean `[x]`.

---

## 5. Obsidian RE corpus — sound method, silent for two months

The corpus is good work: 16 domain docs, an append-only addenda discipline with an
explicit "what is **not** an addendum" rule, and a 2026-06-10 verification pass
that **refuted ~5% of its own claims** rather than editing them. Corrections that
were written down **are** implemented correctly — verified: `getAvailablePath`
" 1" suffix (`FileManager.cpp:597` + case-insensitive check at `:594`), flat
window-node bounds (`WorkspaceSerializer.cpp:192-195,932-935`), `group` as a
sibling of `state` (`:215`).

**Zero addenda since 2026-06-10** — across Clusters K, L, M, N, O, P.

Ranked documentation debt:

1. **`domains/workspace.md` §2 — split-direction convention is known WRONG.**
   Cluster L5 root-caused (and live-verified against a real vault, over two round
   trips) that Obsidian's `"vertical"`/`"horizontal"` split-`direction` names the
   **divider line's orientation, not the stacking axis**. The fix is correctly
   commented in code (`WorkspaceSerializer.cpp:333-336,469-472`) but no addendum
   was written. `addenda/README.md` explicitly tells implementers to check this
   exact section before implementing — so a future session will re-introduce the
   P0 nested-split-scrambling bug the project already paid for.
2. **UTF-16-unit offsets — unverified and untracked.** The addendum calls it
   "load-bearing for all C++ offset arithmetic." `MetadataParser` operates on
   `QString` (so the shape is right), but there is **no test** asserting a
   non-ASCII offset and **no tracker item** anywhere.
3. **`pinned` written at one of two required sites.** The 2026-06-10 workspace
   addendum requires `pinned` at both leaf-node and ViewState level.
   `WorkspaceSerializer.cpp:214` writes it at node level; the nested `state` object
   (`:208-213`) does not re-write it.
4. **`domains/editor.md` §14** is structurally unusable (mislocates the
   live-preview/table/vim/DnD/find-replace engine); already flagged, never split.

---

## 6. Tracking-system state

### 6.1 Punch list — 87% accurate, 77% correctly dispositioned

The **failure mode is stale line numbers, not stale verdicts.** ~20 of 51 open items
cite code that has since moved; in two cases the new home is a *different file*, so
following the citation lands in unrelated code. Header still says "Last refreshed:
2026-06-10 / 129 items" — 4.5 months and 30+ items stale.

**Already fixed, still open:** About page (`MainWindow.cpp:1324`),
`apiLevel: 1` ABI doc (`docs/plugin-development/API-STABILITY.md:120-140`),
sidebar amnesia (Cluster L4 shipped it — the item now *contradicts* the banner
above it in the same file), workspace.json ids/dimension (Cluster L2 overtook it),
`tst_forcegraph_benchmark` (has both `TIMEOUT 120` and `LABELS "benchmark"`).

**Bucket ②** — the first block a new session reads — has 6 of 13 pointers already
closed and does not say so.

**Re-rank:** P1 `eState.scroll` is filed P4 as "Corbomite-internal, no interop
break." Cluster L2's L0 doctrine ("schema-at-rest fidelity to Obsidian is the only
contract") invalidated that rationale: Corbomite now writes `0.42` into an
Obsidian-shared `eState.scroll` that Obsidian reads as a line number.

**Escalate:** P1 frontmatter closing-fence is filed "Markoff-owned; steer
upstream… Awaiting upstream fix + re-pin." The re-pin happened long ago
(submodule now `5d885036`) and the bug is **still open at the cited line**
(`libs/markoff-family/libs/markoff-parser/src/Document.cpp:42`). Worse, the same
bug is **duplicated in Corbomite's own code** at
`src/editor/MarkdownView.cpp:208` — so it is partly Corbomite-fixable today and
"Markoff-owned" is the wrong disposition.

### 6.2 PARITY-MATRIX — ~11% of rows actively wrong

This file is the roadmap's named "current truth," so its errors propagate into
planning. Worst row: **"Replace ❌ — no UI; hamburger Find…/Replace… actions
connected to nothing."** Find/Replace shipped 2026-06-12 (`38022056`).

Also wrong: canvas edge creation (⭕ "never instantiated" — shipped as Cluster
M3.2/M3.3), canvas edge color / z-order / zoom-to-selection (❌ — all shipped),
pin tabs (🟡 — shipped L4 D3), stacked tabs toggle (⭕ — shipped), popout
persistence (🟡 — shipped L2), sidebars (❌ persistence — shipped L2+L4), collision
naming (⚠ — fixed `942411bc`; **the row's own citation is a correction already
applied**).

Stale prose: the "Submodule pin note" cites `ddf5e9a8` (now `5d8850369`); four
rows describe the **retired QML Live leaf** as the LivePreview engine.

**The "8 holes a dogfooder hits in hour one" section (§150-159) is 100% obsolete**
— 8/8 are closed or accepted, yet it reads as a to-do list.

No optimistic over-claim was found to have failed; every ✅ spot-checked held up.

### 6.3 PROJECT-STATE — violates its own slim rule

Self-declares "~30 lines — read in 30 seconds at session start." Actual: **145**.

| Section | Lines | Share |
|---|---|---|
| Orientation (header, two tracks, cluster table) | 1–40 | **13.8%** |
| Recent decisions (23 entries, 2026-04-28→08-21) | 41–66 | 49.6% |
| Last touched (35 entries, 2026-06-11→08-21) | 73–145 | ~34% |

84% is two append-only journals that `decisions-archive.md` already holds in full,
newest-first, and current. "Last touched" is an **undeclared second journal** —
CLAUDE.md bans the `**Previously:**` cascade and caps Recent-decisions
contributions but says nothing about it. There is **no "Current focus" section at
all**, so the rule is written against a section the file lacks. The banned cascade
is genuinely absent (verified 0).

### 6.4 INDEX.md — most-wrong doc per unit of text

Self-declares PROJECT-STATE authoritative, so these are its bugs: Cluster M row
says "Next: M1" (4 phases stale), Cluster L says "L4 NOT LIVE-VERIFIED" (verified
2026-08-18) and "L5 not started" (L5 ran, found a P0, fixed it, added
`fixture16_nestedSplitFirstChild_matchesObsidianGeometry`, then paused — the wording
erases a real bug fix), Cluster O says "Next: O4" (done). Clusters D and E rows
are accurate.

### 6.5 Release-hygiene — orphaned, ~7% complete

163 lines, ~30 tasks, **zero checkboxes**, on no punch list, not a cluster.
**2 done · 4 partial · 21 not started.** Its own overall DoD: 0 of 7.

| Phase | State | Notable |
|---|---|---|
| **A — Legal** | **1/5** | `LICENSE.txt` ✅ (674 lines). LGPL-2.1 text for vendored `jkqtmathtext` **partial** (readme notice, no canonical text, no provenance). SPDX→REUSE **not started**: 4/427 files carry `SPDX-FileCopyrightText`, no `REUSE.toml`, no `LICENSES/`. README third-party table **partial** (missing mmdr + KDDockWidgets). `DiffMatchPatch` rename not started. |
| **B — mmdr blob** | **0/3** | 34.8 MB `libmermaid_rs_renderer.a` still committed. README partially rewritten but **missing exact commit + rustc/cargo versions**, and its rebuild instructions point at `~/dev/mermaidclones/mermaid-rs-renderer`, which **does not exist on this machine** — so the DoD "a stranger can reproduce the `.a`" fails today. Knock-on: punch-list:210 (mermaid dark theme) is now blocked on an unreachable checkout. |
| **C — Dead code** | **0/10** | Every named file still exists. C.2 partial (moved by P2, not deleted). C.7 effectively done (`Notice::post`). C.6's stated reason is now obsolete — completion revival shipped 2026-06-11 with a new `CompletionController`, so `CompletionPopup`/`CompletionDelegate` are genuinely dead. |
| **D — Naming** | **0/6** | `CorbomiteMDI` still defines a **second `MainWindow` class**. `MarkdownView` still collides with `Markoff::MarkdownView`. Both `VaultResourceProvider`s still collide. `src/sidebar/` + `src/reactors/` still orphan single-class dirs. Jargon sweep far from met: **142** `Cluster [A-Z]` hits in `src/`, **17** `TODO(port-foundation-exploration)`. |
| **E — Packaging** | **1/5** | Icon ✅. **AppStream metainfo ❌ — none exists**, so a project shipping `.deb`s has no Software Center listing. E.2 ✅ (via P2). Three test-gating conventions coexist (`CORBOMITE_PORT_BUILD_TESTS`, `BUILD_TESTING`, `PROJECT_IS_TOP_LEVEL`). |

### 6.6 Orphaned work — on no list

**Tier 1 (needed for a credible Obsidian replacement):**

| Item | State | Only mentioned in |
|---|---|---|
| **Tags pane** | zero code (`grep TagPane\|TagsView` → 0) | roadmap `:135` + release criterion `:166`; no checkbox, no cluster, no punch item |
| **File Recovery / version history** | `writeBackup` fires **only on save failure**, no periodic snapshot | Cluster F item 7 (stub, no checkboxes) + punch-list:329 |
| **Note Composer** | absent entirely | Cluster F item 8 only |
| **Named Workspaces** | absent (`workspaces.json` → 0 hits) | Cluster F item 9; blocked by next |
| **Sidedock-as-workspace-tree** | `WorkspaceSidedock` deleted in L3 → **no home class remains** | Cluster F item 10 |
| **ThemeService revival** | §4 | roadmap Phase 5; no checkbox anywhere |

**Tier 2 (vault-compat credibility):** release-hygiene A + E.1 (§6.5);
`registerEmbed`/`EmbedRegistry` advertised in the public plugin API with **no
`EmbedFactory` registered for any extension** and the canvas leaf never calling
`dispatch()` — an API-honesty defect filed only as "PDF view + embed."

**Tracked but list-shaped-only:** Cluster D.5 (Bases plugin API — named by all three
status docs, zero checkboxes, no scope, no spec), Cluster L5 soak (6 named
scenarios, zero checkboxes, all three docs describe it wrongly).

**Cluster E** should be formally retired: its scope is entirely qutepart-era
line/column APIs; item 3 was completed then obsoleted by contract-v2; item 4's
`Qutepart::extraCursors` is moot (qutepart deleted `da9a0a2c`); item 6 shipped via
Cluster N; item 8 sequences against obsoleted G/H. ~15–30 min to archive.

**Cluster F** sizing: F-a (File Recovery + Note Composer) ~5–8d, self-contained;
F-b (Workspaces + sidedock substrate) ~6–9d, needs its own spec. Items 1/2/5/6
(command palette, quick switcher, page preview, word count) already largely exist
and need ~1 day of audit, not building.

---

## 7. Corrections this audit made to its own inputs

Recorded because the subagent passes **disagreed** on three high-severity claims.
All three were re-verified directly; two agent claims were wrong.

| Claim | Verdict | Resolution |
|---|---|---|
| "`go_back`/`go_forward` are permanently disabled and never re-enabled — a shipped-UX defect" | **FALSE** | `ActionContextController::updateBackForwardActions()` (`src/app/ActionContextController.cpp:325-331`) calls `setEnabled("go_back", canBack)`. The `setEnabled(false)` at `MainWindow.cpp:1458,1469` is only initial state. Already closed at punch-list:222. **Do not file.** |
| "`NoteDocument::setMarkdown` uses `Origin::TestFixture` on the production load path, wiping the CRDT buffer and undo stack on every note open" | **TRUE but narrower** | `TestFixture` shares a switch case with `FirstOpen` (`MarkoffDocument.cpp:707-712`), so load paths are correct. But that case *does* replace the CRDT buffer and clear the undo stack, and `MarkdownView::addFrontmatterRow` (`src/editor/MarkdownView.cpp:204,213,222`) routes **user-initiated Properties edits** through it. Real defect, untracked: adding a property row is non-undoable and destroys edit history. Same function duplicates the frontmatter-fence bug at `:208`. |
| "The vault save path has zero test coverage" | **FALSE** | `ctest` does not register the two disabled lib tests, but `tests/integration/tst_editor_save.cpp` does real disk round-trips: `vault.saveDocument(doc)` → `QFile::readAll()` → byte compare, 3 cases. The disabled tests concern `undoStack`/`resetContent` API porting, a different concern. |

---

## 8. Test coverage shape

- **12 `if(FALSE)` gates holding 13 `add_test` calls**: `tests/core` 6, `tests/app` 3,
  `libs/vault` 2, `tests/e2e` 1, `tests/editor` 1. The `libs/vault` pair is
  `tst_vault_save_reload` + `tst_vault_save_refuses_orc` — the punch list mentions
  only `tst_notedocument`.
- **5 test files report PASS while asserting nothing** (QSKIP in `initTestCase`):
  `tst_renderengine` (5 slots), `tst_markdownrenderer` (16 slots),
  `tst_proxy_secrets_process`, `tst_plugin_type_identity`, `tst_e2e_gui` (1 slot).
- **Alive public classes with no test at all**: `SearchResultsModel` (0 tests;
  `tests/models` has 4 files and misses its one Qt-model class), `EmptyView`,
  `PreparedQuery` (4 production consumers, 0 tests), `Decoration`, `RenderProfile`.
- **`examples/` is built by no preset** (`CORBOMITE_BUILD_EXAMPLES=OFF` in
  `CMakeLists.txt:5`; neither `dev` nor `release` sets it). The two example plugins
  the punch list itself fixed in 2026-06-10 have therefore never been compiled
  since. Static read says both are still API-current and their permission manifests
  are correct for the accessors they call — but that is unverified.
- **`appstreamtest` is registered twice** in ctest (#1 and #173) — ECM's
  `KDECMakeSettings` adds it at both top level and `src/`. Harmless; 2/330 wasted.

---

## 9. Remediation sequence

Ordered by value-per-risk. Items 1–4 are documentation; 5+ touch code.

| # | Action | Size | Why first |
|---|---|---|---|
| 1 | **Release-hygiene Phase A + E.1 → punch list P1** | ~0.5 d | Closes a live compliance exposure on a project already shipping `.deb`s. Largest defensible criticism. |
| 2 | **Write the L5 split-direction addendum** (+ link ritual) | ~0.5 h | Stops a frozen doc teaching a convention that already cost a P0. |
| 3 | **Close the 5 stale punch-list items; re-rank `eState.scroll` P4→P1; escalate the frontmatter-fence item** | ~1 h | Removes actively misleading task source. |
| 4 | **PARITY-MATRIX reconciliation** (~11 rows + obsolete §summary) | ~2 h | Restores the project's parity oracle. |
| 5 | **File the untracked defects** (addFrontmatterRow, ThemeService, test-only islands, 17 disabled KActions, `resolveSubpath`, 3× `VaultResourceProvider`, `examples/`) | ~1 h | Stops the blind spot that let them accumulate. |
| 6 | **Fix `addFrontmatterRow`** (undo destruction + duplicated fence bug) | ~0.5 d | Only *newly discovered user-visible defect* in the sweep. |
| 7 | **Delete the test-only islands**, clean ones first (`MarkoffAdapters`+`Adapters.h` is the single cleanest: 216 lines, zero live refs, base classes no longer exist in the dependency, and an already-wrong include path) | ~1–2 d | ~400–600 lines; converts fake coverage to honest absence. |
| 8 | **INDEX 3 stale rows; collapse PROJECT-STATE to ~50 lines** | ~1 h | Restores the session-start contract. |
| 9 | **ThemeService revival** (minimum viable: populate the combo, apply `appearance.json` light/dark/accent) | ~1–2 d | Unblocks 5 live defects + the canvas dark-mode item + release criterion "no `#if 0` corpse files". |
| 10 | **Cluster O Phase O5** (`BasesViewActions`/`GraphViewActions`) | ~1–2 d | Already the declared next step. |
| 11 | **Cluster M M5/M6; resume paused L5 soak** | multi-day | L5 already found one P0. |
| 12 | **Split Cluster F** into F-a / F-b; file Tags pane as a real cluster; retire Cluster E | ~0.5 d planning | Unlocks the largest unstarted user-facing surface. |
| 13 | **release-hygiene B (mmdr provenance/reproducibility), C, D, E** | multi-day | Remaining hygiene; A+E.1 first. |

---

## 10. Method notes for future audits

- **Exclude `packaging/arch/src/corbomite-0.1.0/`** (225 MB vendored copy of the
  tree), `build-*/`, and `libs/markoff-family/.worktrees/` from every code search.
  Naive `rg` over the repo double-counts every reference.
- **Count production call sites, not header mentions.** Forward declarations inflate
  the latter; a cheap sweep of the dead-code question returned "one dead class"
  where the real answer was twelve.
- **Strip `#if 0` bodies before counting**, or corpse code inflates apparent liveness.
- **Cross-check every high-severity subagent claim against the file directly.** Two
  of three contested claims in this audit were wrong (§7), and one of them
  ("go_back never re-enabled") would have produced a filed bug for a feature that
  works.
