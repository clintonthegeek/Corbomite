> **State of the world (refreshed 2026-06-10).**
>
> The 2026-05-25 foundation port is history: Markoff's QML/D2 rebuild merged at
> `v0.7.0-freeze`, the old four-leaf QGraphicsView editor is gone, and Corbomite
> hosts the **three Markoff leaves** (Canvas LivePreview, Source, Styled) via
> `NoteEditorWidget`. **Reading mode is a read-only `Markoff::Styled::Editor`
> leaf** (decided + shipped 2026-05-29; the earlier "read-only Live" steer is
> retired). `StyledRenderEngine` (2026-05-30) renders canvas cards headlessly.
>
> **Contract-v2 adoption COMPLETE (2026-06-10, road-to-dogfood Phase 1):**
> submodule pinned at `af91a936`; find-in-Reading, undo unification, theme
> propagation, Ln/Col, goToLine, ephemeral state, format-verb dispatch and
> zoom all run through the `Markoff::MarkdownView` base (MainWindow is
> grep-gated leaf-agnostic). **Next major workfront:** road-to-dogfood
> Phase 2 (completion revival first) —
> [`docs/superpowers/plans/2026-06-10-road-to-dogfood.md`](docs/superpowers/plans/2026-06-10-road-to-dogfood.md).
>
> **Dogfood loop started 2026-06-10 (Phase 6).** First real run surfaced a
> first-run SIGSEGV (use-after-free): double-clicking a recent vault on the
> welcome screen double-fired `vaultRequested`, so a second vault open tore the
> vault down while a live editor was attached, and the live binding held the
> freed `MarkoffDocument` by a never-detaching raw pointer. Fixed both ends —
> `WelcomeScreen` single-activation + markoff-family retire-on-destroy (re-pin
> `8112833f` → `af91a936`). Other first-run noise still open (see punch list):
> repeated `kf.xmlgui: Index 18 is not within range (0-16)`, plugin Id-in-
> metadata warnings, `qt.qml Invalid QML element name "Theme"`, portal app-id.
>
> **Cluster K — Markoff canvas leaf adoption, CLOSED (2026-08-18 Phase 5).**
> LivePreview is solely `Markoff::Canvas::EditorWidget`; the QML
> `markoff_live` leaf is unlinked (`MARKOFF_BUILD_LIVE=OFF`) and the
> `CanvasLivePreview` settings toggle is removed. Callouts remain frozen
> on Markoff E3 (not a canvas gap). Plan:
> [plan](docs/superpowers/plans/2026-08-15-cluster-k-markoff-canvas-adoption.md).
>
> **Cluster L — workspace/KDDW stabilization & nativization, L0-L4
> landed+closed, L5 (soak & closeout) in progress (2026-08-17/18).**
> Full re-evaluation of the tab/dock layer found a teardown-UAF crash
> class, persistence split-brain (production save dropping
> `floating`/`lastOpenFiles`, Corbomite-private keys leaking into the
> Obsidian-shared `workspace.json`), and Obsidian-literalism residue. L0
> set the compat doctrine (schema-at-rest fidelity to Obsidian is the
> only contract; in-memory shape is free to be idiomatic Qt/KDE) —
> [spec](docs/superpowers/specs/2026-08-17-workspace-compat-boundary.md).
> L1 fixed the crash class (`Workspace::destroyLeaves`, ASAN-clean).
> L2 made `workspace.json` persistence
> full-fidelity and split Corbomite-native state into the doctrine's
> three tiers. L3 removed dead Obsidian-shape shells, consolidated KDDW
> init, and fixed router/resize perf. L4 (native UX polish — KDDW chrome,
> back/forward completion, tab commands, sidebar-width persistence)
> **live-verified and closed 2026-08-18**. **L5's Obsidian round-trip
> soak pass found and fixed a P0 bug**: a real Obsidian-authored vault's
> nested split layout landed scrambled on open, root-caused to an
> inverted divider-direction convention plus a materialization
> anchor-ordering bug in `WorkspaceSerializer.cpp` — fixed and
> live-verified across two round trips against the real vault (punch-list
> `[cluster-l]`, `decisions-archive.md`). **L5 soak paused, not closed** —
> superseded same-day by the v0.1.0 release push below; remaining soak
> items (drag between groups, split, popout, close-undo, vault switch)
> are still open —
> [plan](docs/superpowers/plans/2026-08-17-cluster-l-workspace-stabilization.md).
>
> **v0.1.0 tagged and released (2026-08-18), canonical host moved to
> GitHub.** Same day as the Cluster K Phase 5 closeout: `origin` retargeted
> from Codeberg to `git@github.com:clintonthegeek/Corbomite.git` (the
> `codeberg` remote still exists but is no longer canonical — **the
> `~/dev/CLAUDE.md` ownership registry has not been updated to match and
> now conflicts with reality**); `libs/markoff-family` submodule repointed
> to GitHub Markoff; app identity rebranded to
> `com.concernednetizen.Corbomite`. Packaging added: AppImage build script,
> Arch `PKGBUILD`, and a GitHub Actions Ubuntu 25.10 `.deb` CI building on
> every `v*` tag — plus post-tag fixups (`libtree-sitter-dev` for the `.deb`
> configure step, Qt6 private-dev packages for KDDockWidgets, a
> markoff-family bump for a Qt 6.9 `qHash` fix, and a `Qt >= 6.10`
> version-gate on `endFilterChange`). README rewritten with install/build
> instructions and screenshots. One stale artifact from the migration: a
> code comment in `MainWindow.cpp`'s Help-menu wiring still says "homepage
> + Codeberg" even though `KAboutData` only sets the concernednetizen.com
> homepage. No docs (`PROJECT-STATE.md`, `decisions-archive.md`) were
> updated for any of this until now — see the 2026-08-18 decisions-archive
> entry for full detail. **Next: resume Cluster L5 soak**, or continue
> hardening the release (multi-distro packaging, CI matrix).
>
> **Cluster N — rich clipboard (copy-as + smart paste), OPENED 2026-08-20.**
> Parallel to Cluster M. **Do not implement on `master`.** Branch/worktree
> `feature/rich-clipboard` at `.worktrees/rich-clipboard`. Plan (on that
> branch): `docs/superpowers/plans/2026-08-20-cluster-n-rich-clipboard.md`.
> Next: N3 Source/Styled intercept, then N5 live eyeball. Post-reset N —
> not legacy Cluster N.
>
> **Cluster O — context-sensitive menu/toolbar/sidebar, AUDITED + PLANNED
> 2026-08-20, ready to dispatch at Phase O1.** Audit report:
> [`docs/audit-2026-08-20-context-sensitive-ui.md`](docs/audit-2026-08-20-context-sensitive-ui.md);
> [plan](docs/superpowers/plans/2026-08-20-cluster-o-context-sensitive-ui.md)
> (doctrine §D1-D7 is normative; all 12 open questions answered by the user).
> Headline: of ~79 KActions **zero** belong to canvas/bases/graph, 17 are
> permanently disabled, 7 silently no-op off-markdown. Phase O0 (docs
> bookkeeping) is done; **start at O1** — correctness only, no new mechanism.
> Closed **Cluster I** by absorption in the same pass
> ([retro](docs/cluster-retros/cluster-i-ui-surfacing.md) — note
> `cluster-retros/cluster-i.md` is the *pre-reset* Cluster I, a different
> cluster). Cluster M↔O boundary is recorded in the M plan's Phase M5 banner.
>
> Current parity status: [`docs/PARITY-MATRIX.md`](docs/PARITY-MATRIX.md).
> Port history: [`docs/port-foundation-exploration.md`](docs/port-foundation-exploration.md) (historical).

## Long-term project state

> **Read this section first if you are starting a new session.** Corbomite tracks work across **two parallel tracks**: a flat severity-ranked **punch list** of small fixes, and **strategic clusters** for multi-phase coordinated initiatives. State persists across sessions in dedicated files; do not infer status from conversation context.
>
> **Tracking system was reset 2026-04-26** after a comprehensive audit. Pre-reset cluster lettering (legacy A–Y) lives in `docs/superpowers/plans/archive/` + `docs/decisions-archive.md`. New post-reset lettering started fresh at A.

**Single source of truth for "where we are":** [`docs/PROJECT-STATE.md`](docs/PROJECT-STATE.md). ~30 lines — read in 30 seconds at session start. Names current focus across both tracks.

**Punch list (small fixes, severity P0–P6):** [`docs/punch-list.md`](docs/punch-list.md). Flat single file. Top of file is P0; pick from top. Mark `[x]` when committed; do not delete. **P0/P1 items are mostly silent vault-format-corruption risks — drain before strategic-cluster work unless explicitly redirected.**

**Strategic cluster plans:** [`docs/superpowers/plans/INDEX.md`](docs/superpowers/plans/INDEX.md). Table of contents over the clusters (10 at the 2026-04-26 reset; most since closed or obsoleted — see INDEX for live status). Closed/obsolete plans live under `plans/archive/` and are linked from INDEX.

**Audit (canonical task source — derived 2026-04-26):** [`docs/audit-2026-04-26/`](docs/audit-2026-04-26/). 14 per-domain sub-reports + synthesis README + 58-item priority list. Punch-list and audit-derived clusters all trace back here. Frozen snapshot — re-run audit cycle to refresh.

**Operational rituals (session start, phase done, cluster done):** [`docs/CONTRIBUTING-OPS.md`](docs/CONTRIBUTING-OPS.md). Checklists, not advice.

**Decisions archive (journal):** [`docs/decisions-archive.md`](docs/decisions-archive.md). Append-only closeout summaries + rolled-off decisions. Consult for *why* a prior call was made — not at session start.

**Archive directories are frozen.** `docs/archive/`, `docs/archive-2026-04-26/`, `docs/superpowers/plans/archive/`, and `docs/superpowers/specs/archive/` contain closed/pre-reset work. Don't follow links into them for live tasks.

**Reverse-engineered Obsidian audit (canonical reference, read-only except via addenda):** [`docs/obsidian-audit/`](docs/obsidian-audit/). Pass 1 taxonomy + 15 Pass 2 domain docs + 5 Pass 3 synthesis docs. ~94k words of distilled spec. New facts discovered during implementation go in `docs/obsidian-audit/addenda/`, never as edits to the audit docs. **Check `addenda/README.md` § Corrections before implementing from a domain doc** — a 2026-06-10 verification pass confirmed the corpus is ~95% accurate but refuted specific claims (vault naming/casing, workspace window-node shape, editor timing, taxonomy's QueryController). The Pass 3 synthesis docs `FEATURE-MATRIX.md`/`GAP-ANALYSIS.md` are frozen at 2026-04-14 and badly stale on the Corbomite side — use [`docs/PARITY-MATRIX.md`](docs/PARITY-MATRIX.md) instead.

**Local KDE source (do not clone from invent.kde.org):** `~/src/kde/src/<repo>` was checked out for kate, kdevelop, kio, kconfig, kparts, kxmlgui, kwidgetsaddons, ktexteditor, krunner, baloo, okular, poppler, qtkeychain, sonnet (and more), and older cluster plans reference these by absolute local path. **⚠ Verified absent 2026-08-20** — `~/src` currently holds only `codemirror`, `OrgModeParser`, `qtbase`. Re-clone if a plan needs it; in the meantime KF6 API questions can be settled against the installed headers under `/usr/include/KF6/<Component>/` and the installed `.rc` files under `/usr/share/kxmlgui5/`.

**Do not regrow `PROJECT-STATE.md`.** Slim is non-negotiable. When a cluster or phase closes, write at most 3 sentences in PROJECT-STATE §Current focus (replacing the previous top entry) and the full closeout paragraph into `decisions-archive.md` under a new dated H2 header. The `**Previously:** …` cascade pattern is banned.

**Session-start ritual (TL;DR — full version in `CONTRIBUTING-OPS.md`):**
1. Read `docs/PROJECT-STATE.md` top-to-bottom (~30 lines).
2. Skim `docs/punch-list.md` P0 section. Anything flagged urgent? If yes, drain those before resuming cluster work.
3. If continuing strategic-cluster work: read the cluster plan(s) for the current focus.
4. Read audit-doc sections cited in the plan or punch-list item.
5. Glance at `git log --oneline -10`.
6. State the situation back: "Per PROJECT-STATE, current focus is X (track: punch-list/cluster), last touched Y; next step is Z. Confirm or redirect?" — wait for confirmation before working.

---

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

Run tests — **two things are mandatory**: tests only exist if the build was
configured with `-DCORBOMITE_PORT_BUILD_TESTS=ON` (default **OFF**; the
checked-in `build-dev/` cache has it ON), and `QT_QPA_PLATFORM=offscreen` is
required or ~24 GUI tests abort trying to reach the display:

```bash
cd build-dev && QT_QPA_PLATFORM=offscreen ctest --output-on-failure -j 10
```

Baseline (2026-06-10, post-Phase 1): **259/259 pass** (excl.
`tst_benchmark_layout`, label `benchmark`, which times out by design). No
known-red tests; `tst_metadataparser` went green with the Phase 1 re-pin
(embed image-node fix landed upstream as `9a6a6b74`).

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
