# Architecture Audit — 2026-05-29

> One-off architecture/correctness audit of the Corbomite-owned tree (~73k LOC:
> `libs/*` excluding the `markoff-family` submodule + vendored `jkqtmathtext`, plus all
> of `src/*`). Method: 8 parallel read-only audit agents, one per domain, every finding
> cited to `file:line`; the highest-stakes and most surprising claims were then
> independently re-verified (two agent claims were **corrected** — see below). Comments
> and tracking docs were treated as suspect until the audit was complete, then
> cross-checked against the code.
>
> This is a **frozen snapshot**. New facts discovered during follow-up work go in the
> punch-list / decisions-archive, not as edits here.

## Verdict

The **architecture is fundamentally sound**: dependency layering is almost entirely
clean and acyclic, encapsulation is well respected (zero private-header reach-ins
across the whole tree), the plugin system is genuinely well engineered, and
`libs/bases` is excellent. The dominant problem is that the codebase is **mid-port and
it shows**: ~1,100 lines of `#if 0` plus ~40 stubbed methods (all from the retired
QGraphicsView→QML/D2 editor migration) are left *wired into the live UI*, so a large
fraction of the editor *looks* functional but silently does nothing. Separately, the
audit found **two genuine save-path correctness bugs** not in any tracking doc. The
tracking docs are unusually disciplined and honest, but have systematic blind spots
around save-path correctness and architectural/layering debt.

Two corrections to the agents' raw output, both caught on re-verification:

1. **KDDockWidgets is NOT a "phantom dependency."** It is genuinely used — `Workspace`
   (in `libs/core`) wraps it and is instantiated at `MainWindow.cpp:1698`. The real
   finding is subtler: **two docking substrates coexist** (vendored KateMDI for
   sidebars + KDDockWidgets for the central document area).
2. **The dirty `.base` file is NOT a Corbomite save regression.** `columnSize` never
   appears in Corbomite's serializer, and Corbomite always writes `note.`-prefixed
   names. The dirty `Films.base` has bare names *and* a `columnSize` field — that is
   **Obsidian's** serialization, written during dogfooding. It demonstrates benign
   format drift between the two apps, not corruption by Corbomite.

---

## THE GOOD

- **Encapsulation is clean.** No lib reaches into another lib's private `src/` headers;
  no boundary-escaping relative includes. Every cross-lib include goes through the
  public `include/corbomite/X/` surface.
- **Layering is mostly correct and acyclic:** `vault → {core,storage}`,
  `models → {vault,core,storage}`, `bases → {vault,storage,core}`, `canvas → core` are
  all properly directed.
- **`libs/bases` (14k LOC, the largest lib) is the healthiest code in the tree.** Clean
  lexer→parser→evaluator→UI separation; sane `shared_ptr` Value semantics with no
  cycles; a complete Pratt parser with uniform error recovery; strong behavioral tests;
  **no dead code, no stubs**. The just-shipped Filter Builder is well integrated —
  `FilterSpec` (mutable UI mirror) ↔ immutable `FilterNode` backend via two pure
  converters, no duplication of eval/filter logic.
- **The plugin system is well engineered.** All 9 plugins follow one uniform shape via
  the `corbomite_add_plugin` helper; vault/file/search access goes **only** through
  permission-gated proxies — no plugin reaches into core/vault/storage internals.
  `graph-view` and `local-graph` *share* the compiled `GraphDataBuilder` (not
  copy-pasted). Host owns plugin-view lifecycle (QPointer tracking, flush-on-unload).
- **`forcegraph` and `search` linking zero Corbomite libs is good isolation, not dead
  code.** Both have real consumers (Barnes-Hut layout w/ QuadTree + multilevel
  coarsening; a search-DSL/scoring layer that *complements* the storage FTS index).
  `mmdr` (Rust Mermaid FFI) is real, called, with correct alloc/free pairing.
- **Echo-suppression + metadata threading are genuinely well built.** The save→watcher
  echo guard is real defense-in-depth (mtime ledger + byte-equality fallback +
  rollback-on-failure); the `MetadataWorker` thread is correctly synchronized
  (CV queue, parse-outside-lock, queued connections, clean shutdown);
  `SQLiteIndex` deriving from `cacheChanged` is real, not stubbed.
- **CMake hygiene is good:** consistent C++20, no hardcoded absolute paths,
  `BUILD_INTERFACE` wrapping of *submodule* deps is the correct technique.

---

## THE BAD — architecture & layering

- **`libs/core` is not a "domain types" library — it's a grab-bag spanning four
  layers.** Beyond `NoteMeta`/`NoteDocument` it contains the entire **Workspace/docking
  substrate** (`Workspace`, `WorkspaceLeaf`, …) and the **View hierarchy**
  (`View`/`FileView`/`TextFileView`/…). *This* is why "Core" links
  `KDAB::kddockwidgets` + `Qt6::Widgets`, and those deps then transitively burden every
  downstream lib. `KDDockWidgets::QtWidgets::MainWindow*` leaks through **public**
  headers (`Workspace.h:15,217`; `WorkspaceLeaf.h:12,106`). The Workspace/Views
  subsystem belongs in a separate `libs/workspace` (or `ui`) lib above models.
- **One header-level cycle: `core ⇄ storage`.** `libs/core/src/{MarkoffAdapters,
  TextFileView}.cpp` include `corbomite/storage/*.h`, while
  `libs/storage/include/.../MetadataCache.h:4` includes `corbomite/core/Events.h`.
  Intended layering is `core ← storage`; reality is bidirectional. Fix by moving the
  storage-consuming code out of core (it's the mislayered Workspace/Views code again).
- **`libs/models` public header leaks a DB type:**
  `SearchResultsModel.h:6` includes `corbomite/storage/SQLiteIndex.h`, so every model
  consumer transitively pulls the SQLite API. Depend on a narrower interface.
- **CMake "install-time correctness" hazard:** `storage`/`models` link
  `Corbomite::Core`/`Storage` only under `if(NOT PROJECT_IS_TOP_LEVEL)`, and `bases`
  wraps its *first-party* Core/Storage/Vault deps in `BUILD_INTERFACE` — yet all three
  expose those deps through public headers. A third party doing
  `find_package(Corbomite)` against an install gets headers that `#include` core/storage
  with no transitive link/include path.
- **`libs/core` links `Qt6::Network` but never uses it** (only "network" hit is a
  permission *string constant*). Pure transitive bloat; trivially removable. `vault`
  likely copy-pasted the same unused link.
- **Two docking substrates coexist:** vendored KateMDI (`src/mdi/CorbomiteMDI`, the
  `MainWindow` base) for the sidebar tool-views, *and* KDDockWidgets (via `Workspace` in
  libs/core) for the central document area. KDDockWidgets is genuinely required; the
  dual-substrate design is undocumented and a long-term simplification target.

---

## THE UGLY — port residue (largest single issue)

The retired-editor → QML/D2 port left a large dead/stubbed surface **still wired into
the running UI**:

- **~1,100 lines of `#if 0`** across 5 files in `libs/core` alone:
  `MarkdownRenderer.cpp` (588 dead lines, still a build source), `MarkoffAdapters.cpp`
  (147), `SystemThemeBuilder.cpp`, `ThemeService.cpp`,
  `markoff_adapters/Adapters.h`. **Theming is wired into MainWindow/SettingsDialog but
  is a no-op** (`ThemeService` returns a default `Markoff::Theme{}`).
- **Three overlapping markdown render engines**, all currently broken/empty:
  `MarkdownRenderer` (stub), `RegexRenderEngine` (wraps the stub → empty docs),
  `MarkoffRenderEngine` (dead, returns plain text). `MarkdownRenderer::render`'s body is
  inside `#if 0` while declared in the header — a declared-but-undefined public API that
  would fail to link if called.
- **The canvas card-render pipeline is implemented + unit-tested but disconnected at
  runtime.** `CanvasFileView::setRenderEngine` has **no caller in the app** (only
  canvas-internal + tests), so `m_renderEngine` stays null → embedded file cards render
  blank; text cards limp on a regex fallback. (Distinct from the documented "Markoff
  card renderer" cliff — even the *existing* engine is one missing call from working for
  text cards.) Secondary: `CanvasScene::setRenderEngine` doesn't re-render existing
  cards.
- **Reading mode is a confusing live dead-end, not a clean disable.** The no-op stub is
  exposed via a menu action, the Ctrl+E 3-way cycle, and a hamburger action — selecting
  it yields a read-only LivePreview that looks *identical* to LivePreview.
- **~670 LOC of orphaned editor widgets** compiled + tested but unreachable:
  `SourceEditor`, `Notice` (toast), `HoverPopover` (doubly dead — `m_view` hardcoded
  null *and* zero callers), and the entire in-editor **completion + suggest subsystem**
  (`m_completionPopup` never non-null; `m_suggestManager` write-only).
  `WikiLinkSuggest`/`TagSuggest` are `nullptr`-parented and never deleted → **likely
  leak**.
- **Silent-failure UX** (looks wired, does nothing): the status bar never updates
  (`cursorInfoChanged` has no emitter); zoom in/out/reset are empty stubs behind live
  actions; **Insert Table / Insert Callout open a dialog then `(void)` the result**;
  ephemeral-state (scroll/cursor/fold) persistence is a no-op end to end.
- **Dead code unrelated to the port:** `VaultScanner` has **zero production callers**
  (`Vault::buildTree` duplicates its walk) — kept alive only by its own tests. Several
  dead signal connections (e.g. `linkActivated` connected but never emitted).

---

## DATA-INTEGRITY DEFECTS (highest priority — not previously tracked)

1. **`Vault::saveDocument` writes via raw `QFile(WriteOnly|Truncate)`**
   (`Vault.cpp:755`), bypassing the `DataAdapter`'s atomic temp-file-rename that every
   other write uses. A crash mid-write truncates the note. **P0-class data-loss risk**,
   ironically sitting right below a U+FFFC guard whose whole purpose is preventing
   corruption.
2. **`Vault::modify()` doesn't reconcile an open `NoteDocument`** (`Vault.cpp:213`) —
   and because it stamps a self-write, the watcher echo is suppressed, so a
   frontmatter/process write to a file open in the editor leaves the in-memory buffer
   silently stale. No test covers this path.

### Adjacent (verified NOT Corbomite bugs, kept for the record)

- **Markdown save blank-line drift** (working tree: `Start Here.md` 68→33 lines). The
  save path routes through `doc->markoff()->serializeForSave()` —
  **Markoff's** D2 serializer (submodule), a known data-loss class already tracked via
  the `handoff/2026-05-21-save-path-data-loss.md` cross-repo steer. Surfaced through
  Corbomite; not Corbomite-fixable locally. Needs confirmation the working-tree files
  were Corbomite-written (the pattern matches an AST round-trip, not Obsidian).
- **`.base` format drift** (dirty `Films.base`): Obsidian's serialization
  (`columnSize`, bare names, unquoted), not Corbomite's. Benign cross-app format
  difference. Actionable bit: confirm Corbomite's `unrecognizedData` mechanism
  (`1be8a577`) preserves Obsidian-written fields like `columnSize` across a Corbomite
  round-trip.

---

## HYGIENE

- **Tracked machine-specific broken symlink:** `qmarkdowntextedit →
  /mnt/oldhome/clinton/src/QOwnNotes/...` — unreferenced by any code/CMake; `git rm` it.
- **Stale empty `build/` dir** (active build is `build-dev/`) — delete to avoid
  confusion with the CLAUDE.md "legacy build/" convention.
- **Dogfood pollution:** `testvaults/films-vault/Untitled.md` (empty, untracked) +
  modified test-vault files (Obsidian/Corbomite round-trip churn — revert unless an
  intentional fixture refresh).
- **142 `TODO`/`XXX` markers**, ~70 tagged `TODO(port-foundation-exploration)` — one
  coherent class marking the unfinished editor port.

---

## Plugin / metadata nits (low severity)

- `bookmarks/BookmarksPlugin.cpp:14` dead `#include "corbomite/vault/Vault.h"` (+ unused
  `class Vault` fwd-decl) — Vault type never used; access is all via `VaultProxy`.
- `X-Corbomite-ApiLevel` present in only 1 of 9 plugins (defaults to 1, so harmless
  today, but defeats the "consistent across all" intent for a future ABI bump).
- `local-graph` lacks `X-Obsidian-Id` (arguably justified — no Obsidian counterpart).
- `reinterpret_cast<QWidget*>(mainWindow)` in 8 view-creating plugins (works by
  primary-base layout coincidence; host re-casts properly so low real risk). Replace
  with a `QWidget*` param or `asWidget()` accessor.
- `search` plugin needs a post-helper `OUTPUT_NAME` patch — fold into
  `corbomite_add_plugin`.

---

## Tracking-doc cross-check

**Excellent:** `PROJECT-STATE.md` is slim and current; the two-track model is sensible;
port-debt is acknowledged honestly and in detail (`#if 0` walls, Reading-mode freeze,
canvas cliff, Markoff E-arc deps all named); Cluster D's history matches the commits;
clusters map cleanly onto "feature-match Obsidian."

**Blind spots:**

1. **No save-path correctness items.** The punch-list's stated purpose is "silent
   vault-format corruption risks," yet the two data-integrity defects above appear
   nowhere. Highest-value gap. (Added to P0 this session.)
2. **No architecture/layering track at all.** The `core↔storage` cycle, core-hosts-
   Workspace mislayering, `Qt6::Network` dead link, and `SearchResultsModel.h` leak are
   untracked. Worth a small "structural debt" cluster.
3. **The docs under-communicate the aggregate dead-UI surface.** Each item is
   individually honest, but read top-down you wouldn't realize the editor currently has
   non-functional status bar, zoom, theming, completion, hover, insert-table/callout and
   ephemeral-state, plus ~670 LOC of orphaned widgets. A single "port-residue inventory"
   would make the cliff legible.
4. **Minor drift:** stale "Phase 10 will delete VaultModel" comment at
   `MainWindow.h:182` (VaultModel is gone); "8 plugins + note-stats" is stale (9
   built-ins now; note-stats is an `examples/` plugin); J's obsolescence still listed
   "to confirm."

**Is the plan sensible toward the goal?** Yes. The risk is purely that the doc system
tracks *feature clusters* but has no lane for *correctness/architecture debt*, so the
genuinely scary bugs (save path) are invisible to the session-start ritual meant to
surface "silent corruption risks first."

---

## Recommended actions (severity-ordered)

1. Fix `saveDocument` to route through `m_adapter->writeBinary` (atomic). **P0.**
2. Fix / test `Vault::modify()` open-document reconciliation. **P1.**
3. Add a "save-path integrity" P0 block to the punch-list (done this session).
4. Decide Reading mode: gray out its 3 entry points or implement read-only Live.
5. Wire the missing `setRenderEngine` call + make `CanvasScene::setRenderEngine`
   re-render existing cards (lights up text cards immediately).
6. Sweep orphans: `VaultScanner`, `MarkoffRenderEngine`, `SourceEditor`, `Notice`, dead
   `HoverPopover`/completion/suggest, `qmarkdowntextedit` symlink, stale `build/`.
7. Structural-debt cluster: extract Workspace/Views out of `core`; drop `Qt6::Network`;
   fix the `NOT PROJECT_IS_TOP_LEVEL` / `BUILD_INTERFACE` first-party-dep hiding; narrow
   `SearchResultsModel.h`.
8. Confirm `unrecognizedData` preserves Obsidian's `.base` `columnSize` round-trip.
9. Plugin nits: dead bookmarks include; `X-Corbomite-ApiLevel` on all 9;
   `reinterpret_cast` → typed accessor; `OUTPUT_NAME` into the plugin helper.

---

## Disposition update — 2026-06-10

> The findings above are a frozen snapshot of 2026-05-29 and have not been edited.
> This appended section records what has changed since, verified against code/git
> on 2026-06-10.

**Overtaken since the audit:**

- **Rec 4 (Reading-mode dead-end)** — resolved 2026-05-29 as a read-only Styled
  leaf (`775fa54e`).
- **Rec 5 (canvas `setRenderEngine` never called)** — wired 2026-05-30
  (`dee26c2f`, `aaca39b7`, `658bbee8`).
- **Zoom stubs** — fixed (`d813fd21`).
- **"Three render engines, all broken"** — a fourth, working `StyledRenderEngine`
  landed (`e7a40ae2`).
- **Retired-renderer tests** — QSKIP-gated (`a6a664d5`).

**Still open as of 2026-06-10 (verified):**

- Non-atomic `Vault::saveDocument` (`libs/vault/src/Vault.cpp:755`) — **P0**.
- `Vault::modify()` doesn't reconcile open `NoteDocument`s.
- `#if 0` walls: `MarkdownRenderer` / `MarkoffAdapters` / `SystemThemeBuilder` /
  `ThemeService`.
- Tracked `qmarkdowntextedit` symlink (removal in progress 2026-06-10).
- Stale empty `build/` dir (removal in progress).
- `VaultScanner` zero production callers.
- Ephemeral-state persistence no-op.
- Status bar dead (no `cursorInfoChanged` emitter).

Newer findings: see `docs/PARITY-MATRIX.md` (2026-06-10) and the punch-list
refresh of the same date.
