# Release Hygiene — Public-Facing Codebase Cleanup Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the codebase presentable for the public release cut: legal/licensing completeness (GPL text, LGPL text, REUSE compliance, honest third-party attribution), resolve the 34.8 MB `libs/mmdr` binary blob, purge ~1,000+ lines of verified-dead code, fix misleading names, and land packaging polish. Everything here is "how the repo looks to a stranger on release day" — no feature work.

**Architecture:** Five independent phases of small, mechanical, independently landable tasks. All items were verified by the 2026-06-10 audit; re-verify any cited line number before editing (files drift). One commit per task, message prefix `release-hygiene <task-id>:`.

**Verification baseline:** every task ends with `cmake --build --preset dev -j 10` clean and `cd build-dev && QT_QPA_PLATFORM=offscreen ctest --output-on-failure -j 10` showing **no new failures vs the 2026-06-10 baseline** (250/251; known failure: `tst_metadataparser`, gated on a Markoff re-pin). "Build+test green" below means exactly this.

**Dependencies:** Phases A, B, and E are mutually independent and can land in any order. Phase C/D tasks that touch roadmap revival targets (ThemeService, CompletionPopup, KateMDI session code, Notice, hamburger Find/Replace) are gated — each such task says so and must check `docs/PROJECT-STATE.md` + the dogfoodable-release roadmap before acting. D.2 must coordinate with the MainWindow-decomposition spec. C.5 retires `SourceEditor::m_qutepart`, satisfying that D-item.

---

## Phase A — Legal / licensing (hard release blockers)

### Task A.1: Add GPL-3.0 license text at repo root
- [ ] README promises "GPLv3-or-later" but no license text exists anywhere in the repo. Fetch the canonical GPL-3.0 text (`https://www.gnu.org/licenses/gpl-3.0.txt`) verbatim into `LICENSE` at repo root. Add a `LICENSES/GPL-3.0-or-later.txt` copy if adopting REUSE layout (pairs with A.3).
- [ ] **DoD:** `LICENSE` exists, byte-identical to the canonical text; README License section links to it.

### Task A.2: Add LGPL-2.1 text for vendored jkqtmathtext
- [ ] `libs/jkqtmathtext/` (LGPL-2.1-or-later, vendored) ships no license text. Add `libs/jkqtmathtext/LICENSE` (canonical LGPL-2.1) and a short provenance note (upstream JKQtPlotter repo + commit/version vendored) in or beside `jkqtmathtext.readme`.
- [ ] **DoD:** LGPL-2.1 text present under `libs/jkqtmathtext/`; provenance note states upstream URL and version.

### Task A.3: SPDX-FileCopyrightText sweep → REUSE compliance
- [ ] `SPDX-License-Identifier` coverage in `libs/` (excl. `markoff-family/`) is ~100%, but only 2 files carry `SPDX-FileCopyrightText`. Confirm the copyright-holder string with the user (existing precedent: `2026 Corbomite contributors` in `libs/core/include/corbomite/core/VaultResourceProvider.h`), then script-insert the line above each existing `SPDX-License-Identifier` in `libs/` and `src/`. Do not touch `libs/markoff-family/` (submodule) or vendored third-party files (those keep upstream notices).
- [ ] **DoD:** `reuse lint` passes (or, if `reuse` is unavailable, a grep audit shows every first-party `.cpp/.h` has both SPDX lines); build+test green.

### Task A.4: Complete the README third-party table
- [ ] README's table omits **mermaid-rs-renderer** (`libs/mmdr`; upstream `github.com/1jehuang/mermaid-rs-renderer`, MIT) and **KDDockWidgets** (GPL-2.0/3.0 or commercial; system dep). Add both rows. Audit `libs/` and `cmake/` for any other dep missing a row while in there.
- [ ] **DoD:** table covers every vendored lib and every non-KDE/Qt linked dep, with correct license IDs.

### Task A.5: Rename `DiffMatchPatch` → `ThreeWayMerge`
- [ ] `libs/core/include/corbomite/core/DiffMatchPatch.h` + `libs/core/src/DiffMatchPatch.cpp` are a homegrown LCS three-way merge, **not** Google's diff-match-patch — the name implies foreign code under the wrong license. Rename files, class, includes, CMake entries, and any test references to `ThreeWayMerge`.
- [ ] **DoD:** no occurrence of `DiffMatchPatch` remains in the repo (excl. archives); build+test green.

---

## Phase B — The mmdr binary blob

### Task B.1: Document mmdr provenance
- [ ] `libs/mmdr/` contains a 34.8 MB pre-built Rust static lib (`libmermaid_rs_renderer.a`) committed to git, built out-of-tree from `github.com/1jehuang/mermaid-rs-renderer` (MIT). Expand `libs/mmdr/README.md` to state: upstream URL, license, exact upstream commit built, rustc/cargo versions, and the build command that produced the `.a`. Add the MIT license text to `libs/mmdr/`.
- [ ] **DoD:** a stranger can reproduce the `.a` from the README alone; MIT text present.

### Task B.2: Replace the committed `.a` with a reproducible mechanism
- [ ] Pick one (present trade-offs to the user; recommend the first that fits the toolchain): (a) build-from-source via [corrosion](https://github.com/corrosion-rs/corrosion) + a cargo submodule/vendor of mermaid-rs-renderer; (b) optional system/external dep (`-DCORBOMITE_MMDR_LIB=/path/to/libmermaid_rs_renderer.a`, feature auto-disables when absent, like qt6keychain); (c) a fetch script that downloads a release artifact and verifies a pinned SHA-256. Implement the chosen option; keep `mmdr_ffi.h` + `libs/mmdr/CMakeLists.txt` IMPORTED-target shape working; delete the `.a` from the working tree.
- [ ] **DoD:** fresh clone configures and builds with Mermaid either functional or cleanly disabled; `git status` shows no `.a` tracked; build+test green.

### Task B.3: Decision point — git-history rewrite
- [ ] The `.a` dominates the ~86 MiB pack. Removing it from history (`git filter-repo`) shrinks clones dramatically **but changes every SHA** — all commit hashes cited in `docs/` (decisions-archive, retros, INDEX) would dangle. Write up the trade-off and put the decision to the user. **Recommendation:** if rewriting at all, do it exactly at the public-release cut (fresh public remote), never before; the private Codeberg history stays as-is.
- [ ] **DoD:** decision recorded in `docs/decisions-archive.md` under a dated header; no rewrite executed as part of this plan.

---

## Phase C — Dead code purge (~1,000+ lines, all audit-verified dead)

### Task C.1: Delete `MarkdownRenderer.cpp` corpse + CMake dedupe
- [ ] `libs/core/src/MarkdownRenderer.cpp`: 588 of 602 lines are inside `#if 0`. Delete the dead block; if the surviving ~14 lines are trivial, fold them into the header and delete the file. Also: the file is listed **twice** in `libs/core/CMakeLists.txt` SOURCES (lines 16 and 138) — dedupe regardless.
- [ ] **DoD:** no `#if 0` in the file (or file gone); single CMake entry; build+test green.

### Task C.2: Delete `MarkoffAdapters` corpse
- [ ] `libs/core/src/MarkoffAdapters.cpp` body is entirely dead. Delete the .cpp, its header (`Adapters.h`), the `#include` in `src/app/MainWindow.cpp` (~line 47), and the 3 commented-out members in `src/app/MainWindow.h:239-241`. Remove CMake entry.
- [ ] **DoD:** no `MarkoffAdapters` references outside archives; build+test green.

### Task C.3: Strip `SystemThemeBuilder.cpp` dead bulk
- [ ] ~95% of `libs/core/src/SystemThemeBuilder.cpp` (219 lines) is dead. Delete the dead portions; keep the live remainder.
- [ ] **DoD:** only reachable code remains; build+test green.

### Task C.4: `ThemeService.cpp` — roadmap-gated
- [ ] ~85% of `libs/core/src/ThemeService.cpp` is dead, **but theme revival is on the roadmap** (MarkdownView-contract-v2 adoption includes theme propagation). **Check `docs/PROJECT-STATE.md` and the roadmap first.** If theme work is scheduled to land before release, skip this task (mark N/A). Otherwise purge dead portions only, preserving the live surface.
- [ ] **DoD:** explicit purge-or-skip decision recorded in the commit message or punch list; build+test green.

### Task C.5: Delete dead classes — MarkoffRenderEngine, EmbedDepthGuard, TabModel, SourceEditor
- [ ] `libs/core/src/MarkoffRenderEngine.cpp` ("DEPRECATED stub"), `libs/core/src/EmbedDepthGuard.cpp` (zero refs), `libs/models` `TabModel` (only disabled tests), `src/editor/SourceEditor.{h,cpp}` (Qutepart remnant, compiled but never instantiated — its test goes too, and `SourceEditor::m_qutepart` dies with it). Re-grep each for callers before deleting; remove headers, CMake/test registrations, and `docs/SHARED-SYMBOLS.md` entries.
- [ ] **DoD:** zero references to the four classes outside archives; build+test green.

### Task C.6: CompletionPopup / CompletionDelegate — roadmap-gated, default keep
- [ ] `src/editor/CompletionPopup.{h,cpp}` + `CompletionDelegate.{h,cpp}` are never instantiated, **but completion revival is on the roadmap**. Default: **keep** until that roadmap item decides delete-vs-revive. Action here is only to add a punch-list entry cross-referencing this plan, so the dangling state is tracked.
- [ ] **DoD:** punch-list entry exists naming both files and the gating roadmap item.

### Task C.7: `src/dialogs/Notice` — park or wire (roadmap-gated)
- [ ] `Notice.{h,cpp}` has only a test consumer, but Obsidian parity wants `Notice::post` eventually (Cluster I Phase 6 remnant). **Check Cluster I status first.** Do not delete; either leave with a one-line comment pointing at Cluster I, or wire a single trivial production call if one is already specced.
- [ ] **DoD:** Notice's status (parked-for-Cluster-I or wired) is stated in code comment + punch list.

### Task C.8: KateMDI session machinery — conditional, cross-reference only
- [ ] `src/mdi/CorbomiteMDI.cpp` session code (~150 lines: `Sidebar::startRestoreSession`/`saveSession` at 1351–1430, `MainWindow::startRestore`/`finishRestore`/`saveSession` at 1800–1890) is currently uncalled, **but the roadmap wants sidebar persistence by CALLING this code. Do not delete.** Only if the roadmap explicitly chooses a different persistence path may this be removed — re-check at execution time; otherwise mark N/A.
- [ ] **DoD:** decision trail (kept-for-roadmap or deleted-after-roadmap-decision) recorded; build+test green if deleted.

### Task C.9: MainWindow / MarkdownView micro-corpses
- [ ] Delete `MainWindow::triggerEditorAction` (`src/app/MainWindow.cpp:499-513`, pure no-op) and its call sites/declaration; delete the dead `expandedFolders` fetch (`:2403-2405`, `Q_UNUSED`). The unconnected hamburger Find/Replace actions (`src/editor/MarkdownView.cpp:298-312`) are **roadmap-gated** (find-UI port): wire if the contract-v2 adoption has landed `showFind`, else remove the menu items.
- [ ] **DoD:** no no-op paths remain; Find/Replace either functional or absent from the menu; build+test green.

### Task C.10: libs/search include-only stubs → header-only
- [ ] `libs/search/src/PreparedQuery.cpp` and `FuzzyMatch.cpp` are 2-line include-only stubs. Delete both, remove from CMake, confirm the headers are genuinely header-only.
- [ ] **DoD:** files gone; `Corbomite::Search` builds; build+test green.

---

## Phase D — Naming & semantics

### Task D.1: Rename `CorbomiteMDI` + document Kate provenance
- [ ] `src/mdi/CorbomiteMDI.{h,cpp}` is a KateMDI fork (sidebars/tool-views) — not MDI — and defines a second `MainWindow` class in the same app. Rename class+files+dir to `ToolViewHost` (or `CorbomiteSidebars`; confirm with user), update all includes/CMake. Add a README "Forked code" note: derived from Kate's katemdi (LGPL-2.0 headers, Cullmann/Wenninger copyrights) — keep the original copyright headers intact.
- [ ] **DoD:** no `MDI`-named class/dir; exactly one class named `MainWindow` in `src/`; provenance in README; build+test green.

### Task D.2: Rename `Corbomite::MarkdownView` → `MarkdownFileView`
- [ ] `Corbomite::MarkdownView` (`src/editor/MarkdownView.{h,cpp}`) collides with `Markoff::MarkdownView` in the same TUs. Rename to `MarkdownFileView` (files, class, includes, registrations). **Coordinate first** with the MainWindow-decomposition spec, which also notes this rename — whichever lands first does it.
- [ ] **DoD:** one `MarkdownView` simple name remains (Markoff's); build+test green.

### Task D.3: De-collide the two `VaultResourceProvider`s
- [ ] `Corbomite::VaultResourceProvider` (`src/editor/`, concrete Markoff resource impl) vs `Corbomite::Core::VaultResourceProvider` (`libs/core/`, abstract popover-resource contract). Rename one — suggest the libs/core contract → `ResourceResolver` (or the src/editor impl → `EditorResourceProvider`); pick whichever has fewer call sites.
- [ ] **DoD:** the two classes have distinct simple names; build+test green.

### Task D.4: Normalize canvas/forcegraph targets + vault SHARED
- [ ] `libs/canvas`/`libs/forcegraph` use bare `canvas`/`forcegraph` targets, `include/canvas/...` prefixes, and `namespace Canvas`/`ForceGraph`, while every other lib is `corbomite-*` / `corbomite/...` / `Corbomite::*`. Normalize both to the house pattern (targets, include dirs, namespaces, all consumers). Separately: `libs/vault/CMakeLists.txt:14` is the only `SHARED` lib — make it `STATIC` unless a load-time reason surfaces; record the answer either way.
- [ ] **DoD:** all first-party libs follow one convention; vault linkage decision documented; build+test green.

### Task D.5: Fold orphan single-class dirs
- [ ] `src/sidebar/` holds only `PropertyEditorWidget` + `PropertyRow`, consumed solely by `src/plugins/properties` via `../../` relative includes — move both into that plugin dir and fix includes. `src/reactors/` holds one class (`AutosaveReactor`) — fold it into `src/app/` (or wherever its only consumer lives).
- [ ] **DoD:** `src/sidebar/` and `src/reactors/` no longer exist; no `../../` cross-dir includes for these files; build+test green.

### Task D.6: Comment-jargon sweep + TODO triage
- [ ] Sweep `src/` (and `libs/` where found) for internal-process jargon: "Cluster B/Q.0 Phase 7/C2 Task 13"-style comments and commit-hash references — rewrite each to state the actual constraint, or delete if it carries none. Prune the archaeology narration in `src/app/MainWindow.h:10-11,57-59,182-205`. Triage all 72 `TODO` markers in `src/`: each becomes a punch-list item, gets fixed inline, or is deleted; none may read `TODO(port-foundation-exploration)` at release.
- [ ] **DoD:** `grep -rn "Cluster [A-Z]\|Phase [0-9]\|port-foundation-exploration" src/` returns only comments stating real constraints (target: zero); every surviving TODO maps to a punch-list line; build+test green.

---

## Phase E — Packaging / polish

### Task E.1: AppStream metainfo + icon
- [ ] `data/` holds only `org.corbomite.Corbomite.desktop`. Add `data/org.corbomite.Corbomite.metainfo.xml` (AppStream: name, summary, description, GPL-3.0-or-later, screenshots placeholder, releases stanza) and an app icon (scalable SVG minimum); install both via CMake.
- [ ] **DoD:** `appstreamcli validate` passes on the metainfo; icon installs to hicolor; desktop file references it.

### Task E.2: Break the core↔storage include cycle
- [ ] `libs/storage/include/corbomite/storage/MetadataCache.h:4` includes `corbomite/core/Events.h` while `Corbomite::Core` links `Corbomite::Storage`. Move `Events.h` down into storage, or extract it into a tiny header-only base target both link.
- [ ] **DoD:** no `corbomite/core/` include anywhere under `libs/storage/`; build+test green.

### Task E.3: Fold WorkspaceSerializer sidecar hashes into the object
- [ ] `libs/core/src/WorkspaceSerializer.cpp` keeps file-static sidecar `QHash`es ("Phase 3 workaround", lines ~36, ~59). Move them into the serializer object as members.
- [ ] **DoD:** zero file-static mutable state in the file; build+test green.

### Task E.4: SecretStorage nested event loops
- [ ] `libs/core/src/proxies/SecretStorage.cpp` runs triple-nested `QEventLoop::exec()`. Refactor to async (signal/callback) if the proxy contract allows; otherwise add a prominent re-entrancy-hazard comment and a punch-list item for the async rework.
- [ ] **DoD:** either no nested `exec()`, or the hazard is documented in code + punch list; build+test green.

### Task E.5: Unify test-gating conventions
- [ ] Three conventions coexist: `CORBOMITE_PORT_BUILD_TESTS` (top-level, default OFF), `BUILD_TESTING`, and `PROJECT_IS_TOP_LEVEL` checks across `libs/*/CMakeLists.txt` and `src/plugins/*/CMakeLists.txt`. Pick one option name (recommend `CORBOMITE_BUILD_TESTS`, default **ON for the dev preset**, OFF for release), convert all gates, update `CMakePresets.json` + CLAUDE.md build docs.
- [ ] **DoD:** one gating variable repo-wide; `cmake --preset dev` builds tests without extra flags; build+test green.

---

## Closeout (Ritual 3)

- [ ] Update `docs/superpowers/plans/INDEX.md` (register/close this plan), `docs/PROJECT-STATE.md` §Current focus (≤3 sentences), append closeout paragraph to `docs/decisions-archive.md`, and move resolved punch-list items to `[x]`.

## Definition of done (overall)

1. `LICENSE` (GPL-3.0), jkqtmathtext LGPL text, and mmdr MIT text all present; REUSE-clean first-party sources.
2. README third-party table is complete and honest; KateMDI provenance documented; no class name implies foreign code it isn't (`DiffMatchPatch` gone).
3. No committed binary blob, or an explicit user-approved decision to defer with provenance fully documented; history-rewrite decision recorded, not executed.
4. All audit-verified dead code deleted **except** the four roadmap-gated items (ThemeService, CompletionPopup, KateMDI session, Notice), each with a recorded keep/delete decision.
5. One naming convention across libs; no duplicate simple class names in shared TUs; no single-class orphan dirs; no process-jargon comments or unowned TODOs in `src/`.
6. AppStream metainfo + icon installed; core↔storage cycle broken; one test-gating variable.
7. Full suite: no new failures vs the 2026-06-10 baseline (250/251).
