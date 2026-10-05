> **State of the world (refreshed 2026-10-04).**
>
> Markoff **contract-v2 is adopted**: find-in-Reading, undo, theme propagation, Ln/Col,
> goToLine, ephemeral state, format-verb dispatch and zoom all run through the
> `Markoff::MarkdownView` base, and `MainWindow` is grep-gated leaf-agnostic. Corbomite
> hosts three Markoff leaves — LivePreview, Source, Styled — via `NoteEditorWidget`;
> **Reading mode is a read-only `Styled::Editor` leaf**, and `StyledRenderEngine` renders
> canvas cards headlessly. The old four-leaf QGraphicsView editor is gone.
>
> **Closed:** Cluster O (context-sensitive menu/toolbar/sidebar — doctrine §D1-D7 is
> normative), Cluster M Phase M5 (viewport persistence, read-only lock, convert-to-file,
> jump-to-group), Clusters K/N/P/D/F/I (archived). **v0.1.0** shipped 2026-08-18;
> `origin` is GitHub, not Codeberg. **Cluster L5 soak is paused** and is the gate on
> further `workspace.json` work.
>
> **The Obsidian audit is source-verified, not prose-verified.** The original corpus was
> destroyed, so all 16 domains were re-verified against Obsidian 1.13.7 in 2026-10-03:
> **47 addenda** in [`docs/obsidian-audit/addenda/`](docs/obsidian-audit/addenda/).
> The corpus is reproducible from the `.deb` via [`RE/extract-obsidian.sh`](RE/extract-obsidian.sh)
> (corpus gitignored for licensing; tooling tracked). **Check the corrections gate at the
> top of `addenda/README.md` before implementing from any domain doc** — the frozen docs
> are wrong in ~45 places, and 9 claims in that batch were refuted by the agents citing
> them, including one of our own prior addenda.
>
> Current queue: [`docs/punch-list.md`](docs/punch-list.md) `Added 2026-10-04` section.
> Parity: [`docs/PARITY-MATRIX.md`](docs/PARITY-MATRIX.md).
> Upstream asks: [`docs/UPSTREAM-ASKS.md`](docs/UPSTREAM-ASKS.md) + [`docs/handoff/`](docs/handoff/).
> Mermaid is **stock upstream 11.13.0** — see [`RE/README.md`](RE/README.md#mermaid--better-than-expected).
>
> Resolved history (why Reading mode is Styled, the Cluster M↔O boundary, refuted audit
> claims, v0.1.0 release notes) is archived at
> [`docs/archive/2026-10-04/pre-consolidation-state-banner.md`](docs/archive/2026-10-04/pre-consolidation-state-banner.md).

## Long-term project state

> **Read this section first if you are starting a new session.** Corbomite tracks work across **two parallel tracks**: a flat severity-ranked **punch list** of small fixes, and **strategic clusters** for multi-phase coordinated initiatives. State persists across sessions in dedicated files; do not infer status from conversation context.
>
> **Tracking system was reset 2026-04-26** after a comprehensive audit. Pre-reset cluster lettering (legacy A–Y) lives in `docs/superpowers/plans/archive/` + `docs/decisions-archive.md`. New post-reset lettering started fresh at A.

**Single source of truth for "where we are":** [`docs/PROJECT-STATE.md`](docs/PROJECT-STATE.md). ~30 lines — read in 30 seconds at session start. Names current focus across both tracks.

**Punch list (small fixes, severity P0–P6):** [`docs/punch-list.md`](docs/punch-list.md). Flat single file. **Start at the `Added 2026-10-04` section** — it is the ranked queue; everything below it is the historical record. Top of file is P0; pick from top. Mark `[x]` when committed; do not delete. **P0/P1 items are mostly silent vault-format-corruption risks — drain before strategic-cluster work unless explicitly redirected.** Checkboxes: `[x]` done, `[~]` partially done, `[ ]` open; the count is in the header with a verifying grep.

**Strategic cluster plans:** [`docs/superpowers/plans/INDEX.md`](docs/superpowers/plans/INDEX.md). Table of contents over the clusters (7 active; most others closed or obsoleted — see INDEX for live status). Closed plans live under `plans/archive/` and are linked from INDEX.

**April 2026 audit (provenance only — NOT a live task source):** [`docs/audit-2026-04-26/`](docs/audit-2026-04-26/). 14 per-domain sub-reports + a 58-item priority list that every early cluster traces back to. Frozen snapshot, triaged and then superseded three times (2026-10-01, 10-03, 10-04). Read it only to understand why a decision was made, never to find current work — the punch list is the tracker.

**Operational rituals (session start, phase done, cluster done):** [`docs/CONTRIBUTING-OPS.md`](docs/CONTRIBUTING-OPS.md). Checklists, not advice.

**Decisions archive (journal):** [`docs/decisions-archive.md`](docs/decisions-archive.md). Append-only closeout summaries + rolled-off decisions. Consult for *why* a prior call was made — not at session start. It is large; read only its top section.

**Reverse-engineering corpus (untracked, reproducible):** [`RE/README.md`](RE/README.md). Obsidian 1.13.7 extracted to readable/indexed/partially-renamed JS, queryable via `RE/tools/q.js`. Regenerate with `RE/extract-obsidian.sh`. **Never commit the corpus** — Obsidian is `UNLICENSED` and Corbomite is GPLv3. Do not delete `RE/tools/`: it is the only reason the corpus is reproducible, and losing it is exactly how the previous corpus was lost.

**New facts about Obsidian go in** [`docs/obsidian-audit/addenda/`](docs/obsidian-audit/addenda/), **never as edits to the audit docs.** Per `addenda/README.md`. Addenda are exclusively *new or corrected facts about Obsidian's behaviour* — Corbomite implementation choices go in cluster plans or `docs/cluster-retros/`.

**Upstream asks:** [`docs/UPSTREAM-ASKS.md`](docs/UPSTREAM-ASKS.md) is the index; the sendable documents are in [`docs/handoff/`](docs/handoff/).

**Archives are frozen.** `docs/archive/`, `docs/archive-2026-04-26/`, `docs/superpowers/{plans,specs}/archive/` contain closed and superseded work, including the pre-2026-10-04 `CLAUDE.md` banner. They have rotted relative links by design — **do not follow links into them for live tasks, and do not fix them.**

**Do not regrow `PROJECT-STATE.md`.** Slim is non-negotiable. When a cluster or phase closes, write at most 3 sentences in PROJECT-STATE §Current focus (replacing the previous top entry) and the full closeout paragraph into `decisions-archive.md` under a new dated H2 header. The `**Previously:** …` cascade pattern is banned.

**Session-start ritual (TL;DR — full version in `CONTRIBUTING-OPS.md`):**
1. Read `docs/PROJECT-STATE.md` top-to-bottom (~30 lines).
2. Skim `docs/punch-list.md`'s `Added 2026-10-04` section — that is the queue. Anything P0 there? If yes, drain those before resuming cluster work.
3. If continuing strategic-cluster work: read the cluster plan for the current focus.
4. Read audit-doc sections cited in the plan or punch-list item.
5. Glance at `git log --oneline -10`.
6. State the situation back: "Per PROJECT-STATE, current focus is X (track: punch-list/cluster), last touched Y; next step is Z. Confirm or redirect?" — wait for confirmation before working.

## Building

Two CMake presets are defined in `CMakePresets.json`:

- `dev` → `build-dev/`, `Debug`, `CORBOMITE_DEV_BUILD=ON` (isolated config/data, `[Dev]` window title)
- `release` → `build-release/`, `Release`, `CMAKE_INSTALL_PREFIX=/usr/local` (for dogfooding via `sudo cmake --install`)

Configure and build the dev preset:

```bash
cmake --preset dev
cmake --build --preset dev -j 10
```

Always pass `-j 10` to `cmake --build` — the default serial build is slow on this tree. Use the same `-j 10` for incremental rebuilds.

Run:
```bash
./build-dev/bin/Corbomite
```

Run tests — **three things are mandatory**: tests only exist if the build was
configured with `CORBOMITE_PORT_BUILD_TESTS=ON` (default **OFF**; the `dev`
preset sets it **ON** and the `release`/`appimage` presets set it **OFF** — it
is the single canonical test-gate for the whole tree), `QT_QPA_PLATFORM=offscreen`
is required or ~24 GUI tests abort trying to reach the display, and the
wall-clock timing tests must be excluded (see below):

```bash
cd build-dev && QT_QPA_PLATFORM=offscreen ctest -E "benchmark|perf" --output-on-failure -j 10
```

**Why `-E "benchmark|perf"` and not just `-E benchmark`** (2026-10-01): four
tests measure wall-clock time and flake red under `-j10` from machine load
alone, all passing standalone. `tst_benchmark_layout` (libs/forcegraph) carries
a native `LABELS "benchmark"`, but `tst_canvas_perf_500` and
`tst_canvas_perf_formatted` come from the markoff-family submodule with **no
label at all**, so `-E benchmark` does not reach them. A parent-directory
`set_tests_properties` cannot fix this either — CTest test properties are
directory-scoped — and patching the submodule would be lost on the next
re-pin. The real fix is a `LABELS "benchmark"` in markoff-canvas; until then
keep the regex. Without it, "the suite is green" is not a reliable signal.

Baseline (2026-10-04): **354/354 pass** with the exclusion above.

### Load-sensitive flakes — "the suite is green" is not yet a reliable signal

**Four** tests fail intermittently under `-j6`/load while passing standalone. None is
caused by the code they cover; all are timing- or count-sensitive:

- `tst_forcelayout` (`libs/forcegraph`) — pure timing.
- `tst_cachedmetadatastore::testRoundTripMultipleFiles` — asserts a watcher signal
  **count** (2 vs 1); races with the coalescing drain timer.
- `tst_hover_popover_pinning` — timing-sensitive.
- `tst_metadatacache_worker_integration` — observed at **30.5 s** under `-j6` versus
  **0.26 s** standalone, a ~100x spread. That ratio is large enough to be worth a look
  if it ever appears in CI; so far it has only been seen under parallel load.

So a single red run is **not** evidence of a regression: re-run the failing test alone
before investigating. Conversely, do not accept a green full run as proof a timing fix
landed — pin the specific value in a test that asserts it, which is what the
2026-10-03 audit-fix pass did throughout (every fix is pinned by an exact-value
assertion, and was verified red by reverting the fix).

Build and install the release preset (system-wide, separate config/data dirs from dev):

```bash
cmake --preset release
cmake --build --preset release -j 10
sudo cmake --install build-release
```

### Dependencies

Corbomite requires these system libraries at build time:

- Qt 6.8+ (`Core Widgets DBus Sql Svg PrintSupport`)
- ECM 6.0+ (`extra-cmake-modules`)
- KDE Frameworks 6 (`CoreAddons I18n XmlGui WidgetsAddons IconThemes Config ConfigWidgets ColorScheme DBusAddons SyntaxHighlighting`)
- **KDDockWidgets 2.0+** (`kddockwidgets-qt6` on Arch/Manjaro; provides tab-drag, split, and floating-window substrate)
- tree-sitter (vendored inside the `libs/markoff-family/` submodule, under `libs/markoff-parser/`)
- jkqtmathtext (vendored at `libs/jkqtmathtext/`)
- Optional: `qt6keychain` for persistent SecretStorage (auto-disables if absent)

### Dev Build Isolation

Always configure with `-DCORBOMITE_DEV_BUILD=ON` so dev builds use isolated config/data directories and don't interfere with any installed release version.

| | Release | Dev (`-DCORBOMITE_DEV_BUILD=ON`) |
|---|---|---|
| Config | `~/.config/corbomiterc` | `~/.config/corbomite-devrc` |
| Data | `~/.local/share/corbomite/` | `~/.local/share/corbomite-dev/` |
| Window title | "Corbomite" | "Corbomite [Dev]" |

**⚠ Dev-build isolation is per release-vs-dev, not per worktree/branch.** All
dev builds share the same component name (`corbomite-dev`), and therefore the
same KXMLGUI menu/toolbar cache at
`~/.local/share/kxmlgui5/corbomite-dev/corbomite-devui.rc` — including dev
builds run from other worktrees (e.g. `.worktrees/rich-clipboard`). If a
session's `corbomiteui.rc.in` structural changes don't appear to take effect
(menus/toolbars looking stale or wrong after a rebuild), suspect this cache
before suspecting the code: `rm ~/.local/share/kxmlgui5/corbomite-dev/corbomite-devui.rc`
and relaunch. Bumping the `.rc` file's `version=` attribute normally
self-invalidates the cache, but **not** if a different worktree's dev build
independently reached the same version number first (found + fixed during
Cluster O Phase O3, 2026-08-20 — see `decisions-archive.md`).

## Library Structure

| Library | Target | Purpose |
|---------|--------|---------|
| `libs/core` | `Corbomite::Core` | Workspace/docking (KDDockWidgets), view hierarchy, render engines, plugin proxy API + registries, themes, NoteMeta/NoteDocument |
| `libs/vault` | `Corbomite::Vault` (SHARED) | Vault/TFile/TFolder model, file watcher, FileManager (link rewriting, trash), PluginManager + permissions |
| `libs/storage` | `Corbomite::Storage` | MetadataCache/-Parser/-Worker, SQLiteIndex, LinkResolver, VaultConfig (`.obsidian/*.json` I/O), FileSystemAdapter |
| `libs/models` | `Corbomite::Models` | NotesTreeModel, SearchResultsModel, DailyNoteService, TemplateService, property types |
| `libs/search` | `Corbomite::Search` | SearchDSL parser, FuzzyMatcher (Obsidian-parity scoring) |
| `libs/bases` | `Corbomite::Bases` | `.base` files: YAML schema, formula lexer/parser/evaluator, table view + toolbar panels (largest lib) |
| `libs/canvas` | `canvas` | `.canvas` files: JSON round-trip, scene/items/tools, undo commands |
| `libs/forcegraph` | `forcegraph` | Force-directed graph layout (Barnes-Hut, multilevel); zero Corbomite deps |
| `libs/markoff-family` | (submodule) | The Markoff editor family: markoff-core (D2 CRDT model), markoff-live (QML), markoff-source, markoff-styled, markoff-parser (tree-sitter), collabtext, rapidyaml |
| `libs/mmdr` | `mmdr` (IMPORTED) | Pre-built Rust Mermaid renderer (`libmermaid_rs_renderer.a` + C FFI header) |
| `libs/jkqtmathtext` | `jkqtmathtext` | Vendored LaTeX math rendering (LGPL) |

## Testing

Tests define **expected behavior**. When a test fails, fix the code, not the test.

Run a single test:
```bash
cd build-dev && QT_QPA_PLATFORM=offscreen ctest -R tst_notemeta --output-on-failure
```

## Code Conventions

- C++20, Qt6/KDE Frameworks 6
- Use `i18n()` for all user-visible strings
- Use `QIcon::fromTheme()` for all icons
- Use `KStandardAction` where applicable
- GPLv3 license
