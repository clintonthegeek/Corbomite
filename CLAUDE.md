> **Orientation is [`AGENTS.md`](AGENTS.md).** Status lives only in [`docs/PROJECT-STATE.md`](docs/PROJECT-STATE.md). This file is build, test, and library layout. When this file and PROJECT-STATE disagree, PROJECT-STATE wins.

## Building

Two CMake presets are defined in `CMakePresets.json`:

- `dev` → `build-dev/`, `Debug`, `CORBOMITE_DEV_BUILD=ON` (isolated config/data, `[Dev]` window title)
- `release` → `build-release/`, `Release`, `CMAKE_INSTALL_PREFIX=/usr/local` (for dogfooding via `sudo cmake --install`)

Configure and build the dev preset:

```bash
cmake --preset dev
cmake --build --preset dev -j 3
```

Use `-j 3` for `cmake --build` and `ctest`. This machine has **4 cores and other
users**, so `-j 3` is the ceiling — the old `-j 10` was set for a different
development machine. (Serial is needlessly slow; higher than `-j 3` starves the
box.) Use the same `-j 3` for incremental rebuilds.

Run:
```bash
./build-dev/bin/Corbomite
```

Run tests — **three things are mandatory**: tests only exist if the build was
configured with `CORBOMITE_BUILD_TESTS=ON` (default **OFF**; the `dev`
preset sets it **ON** and the `release`/`appimage` presets set it **OFF** — it
is the single canonical test-gate for the whole tree), `QT_QPA_PLATFORM=offscreen`
is required or ~24 GUI tests abort trying to reach the display, and the
wall-clock timing tests must be excluded (see below):

```bash
cd build-dev && QT_QPA_PLATFORM=offscreen ctest -E "benchmark|perf" --output-on-failure -j 3
```

**Why `-E "benchmark|perf"` and not just `-E benchmark`** (2026-10-01): the wall-clock tests flake red under parallel load alone, and pass standalone. `tst_forcegraph_benchmark` (libs/forcegraph) carries a native `LABELS "benchmark"`, but `tst_canvas_perf_500` and `tst_canvas_perf_formatted` come from the markoff-family submodule with **no label at all**, so `-E benchmark` does not reach them. A parent-directory
`set_tests_properties` cannot fix this either — CTest test properties are
directory-scoped — and patching the submodule would be lost on the next
re-pin. The real fix is a `LABELS "benchmark"` in markoff-canvas; until then
keep the regex. Without it, "the suite is green" is not a reliable signal.

Baseline (2026-10-20): **263/263 pass** with the exclusion above, on the local Markoff pin `e0b96bf8`. Markoff's own tests are excluded because `MARKOFF_BUILD_TESTS` and `MARKOFF_BUILD_COLLABTEXT_VNEXT_TESTS` are forced OFF. The 2026-10-17 count of 267 (and the 274 recorded in `docs/PROJECT-STATE.md`) belong to the previous pin: two test files were retired with their features since (`tst_vaultscanner`, `tst_notice`), the card editor added one, and the release-hygiene pass deleted two more with the dead classes they tested (`tst_embeddepthguard`, `tst_tabmodel`). The 2026-10-04 count of 354 belongs to an earlier pin still.

### Load-sensitive flakes — "the suite is green" is not yet a reliable signal

**Three** tests fail intermittently under load while passing standalone. None is
caused by the code they cover; all are timing- or count-sensitive:

- `tst_forcegraph_layout` (`libs/forcegraph`) — pure timing.
- `tst_cachedmetadatastore::testRoundTripMultipleFiles` — asserts a watcher signal
  **count** (2 vs 1); races with the coalescing drain timer.
- `tst_hover_popover_pinning` — timing-sensitive.

`tst_metadatacache_worker_integration` is **not** on this list. Its 2026-10-17 diagnosis is on the punch list: `indexFinished` was arming while worker parses were still outstanding. A fast pass is not what closed it.

So a single red run is **not** evidence of a regression: re-run the failing test alone
before investigating. Conversely, do not accept a green full run as proof a timing fix
landed — pin the specific value in a test that asserts it, which is what the
2026-10-03 audit-fix pass did throughout (every fix is pinned by an exact-value
assertion, and was verified red by reverting the fix).

Build and install the release preset (system-wide, separate config/data dirs from dev):

```bash
cmake --preset release
cmake --build --preset release -j 3
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

Local KDE Frameworks checkouts, when you need to read upstream, are under `~/src/kde/src/<repo>`. Do not clone them from invent.kde.org.

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
| `libs/canvas` | `Corbomite::Canvas` | `.canvas` files: JSON round-trip, scene/items/tools, undo commands |
| `libs/forcegraph` | `Corbomite::ForceGraph` | Force-directed graph layout (Barnes-Hut, multilevel); zero Corbomite deps |
| `libs/markoff-family` | (submodule) | Markoff editor family: markoff-core, markoff-source, markoff-styled, markoff-parser (tree-sitter), rapidyaml. CollabText is the installed package, not the untracked `libs/markoff-family/libs/collabtext/` tree. Do not `add_subdirectory` that tree. `MARKOFF_BUILD_LIVE` stays OFF. |
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
