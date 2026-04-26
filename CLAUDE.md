## Long-term project state

> **Read this section first if you are starting a new session.** Corbomite tracks work across **two parallel tracks**: a flat severity-ranked **punch list** of small fixes, and **strategic clusters** for multi-phase coordinated initiatives. State persists across sessions in dedicated files; do not infer status from conversation context.
>
> **Tracking system was reset 2026-04-26** after a comprehensive audit. Pre-reset cluster lettering (legacy A–Y) lives in `docs/superpowers/plans/archive/` + `docs/decisions-archive.md`. New post-reset lettering started fresh at A.

**Single source of truth for "where we are":** [`docs/PROJECT-STATE.md`](docs/PROJECT-STATE.md). ~30 lines — read in 30 seconds at session start. Names current focus across both tracks.

**Punch list (small fixes, severity P0–P6):** [`docs/punch-list.md`](docs/punch-list.md). Flat single file. Top of file is P0; pick from top. Mark `[x]` when committed; do not delete. **P0/P1 items are mostly silent vault-format-corruption risks — drain before strategic-cluster work unless explicitly redirected.**

**Strategic cluster plans:** [`docs/superpowers/plans/INDEX.md`](docs/superpowers/plans/INDEX.md). Table of contents over the 10 active clusters (A–J at reset). Closed-cluster plans live under `plans/archive/` and are linked from INDEX.

**Audit (canonical task source — derived 2026-04-26):** [`docs/audit-2026-04-26/`](docs/audit-2026-04-26/). 14 per-domain sub-reports + synthesis README + 58-item priority list. Punch-list and audit-derived clusters all trace back here. Frozen snapshot — re-run audit cycle to refresh.

**Operational rituals (session start, phase done, cluster done):** [`docs/CONTRIBUTING-OPS.md`](docs/CONTRIBUTING-OPS.md). Checklists, not advice.

**Decisions archive (journal):** [`docs/decisions-archive.md`](docs/decisions-archive.md). Append-only closeout summaries + rolled-off decisions. Consult for *why* a prior call was made — not at session start.

**Archive directories are frozen.** `docs/archive/`, `docs/archive-2026-04-26/`, `docs/superpowers/plans/archive/`, and `docs/superpowers/specs/archive/` contain closed/pre-reset work. Don't follow links into them for live tasks.

**Reverse-engineered Obsidian audit (canonical reference, read-only except via addenda):** [`docs/obsidian-audit/`](docs/obsidian-audit/). Pass 1 taxonomy + 15 Pass 2 domain docs + 5 Pass 3 synthesis docs. ~94k words of distilled spec. New facts discovered during implementation go in `docs/obsidian-audit/addenda/`, never as edits to the audit docs.

**Local KDE source (do not clone from invent.kde.org):** `~/src/kde/src/<repo>` is checked out for kate, kdevelop, kio, kconfig, kparts, kxmlgui, kwidgetsaddons, ktexteditor, krunner, baloo, okular, poppler, qtkeychain, sonnet (and more). Cluster plans reference these by absolute local path.

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

Configure and build with the dev build flag:

```bash
cmake -B build -DCORBOMITE_DEV_BUILD=ON
cmake --build build -j 10
```

Always pass `-j 10` to `cmake --build` — the default serial build is slow on this tree. Use the same `-j 10` for incremental rebuilds.

Run:
```bash
./build/Corbomite
```

Run tests:
```bash
cd build && ctest --output-on-failure -j 10
```

### Dependencies

Corbomite requires these system libraries at build time:

- Qt 6.8+ (`Core Widgets DBus Sql Svg PrintSupport`)
- ECM 6.0+ (`extra-cmake-modules`)
- KDE Frameworks 6 (`CoreAddons I18n XmlGui WidgetsAddons IconThemes Config ConfigWidgets ColorScheme DBusAddons SyntaxHighlighting`)
- **KDDockWidgets 2.0+** (`kddockwidgets-qt6` on Arch/Manjaro; provides tab-drag, split, and floating-window substrate)
- tree-sitter (vendored in `libs/markoff-parser/`)
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
| `libs/core` | `Corbomite::Core` | Domain types: NoteMeta, NoteDocument |
| `libs/storage` | `Corbomite::Storage` | File I/O: FileSystemAdapter, VaultScanner |
| `libs/models` | `Corbomite::Models` | Qt item models: VaultModel, NotesTreeModel, TabModel |
| `libs/markoff` | `Markoff::Markoff` | QGraphicsView-based Markdown editor + ReadingView |
| `libs/markoff-parser` | `Markoff::Parser` | tree-sitter-based Markdown parser |
| `libs/mmdr` | `mmdr` | Rust Mermaid renderer bridge |
| `libs/canvas` | `Corbomite::Canvas` | Infinite-canvas (`.canvas` files) |
| `libs/forcegraph` | `Corbomite::ForceGraph` | Force-directed graph layout |
| `libs/jkqtmathtext` | `JKQTMathText` | LaTeX/MathJax-equivalent inline math rendering |

## Testing

Tests define **expected behavior**. When a test fails, fix the code, not the test.

Run a single test:
```bash
cd build && ctest -R tst_notemeta --output-on-failure
```

## Code Conventions

- C++20, Qt6/KDE Frameworks 6
- Use `i18n()` for all user-visible strings
- Use `QIcon::fromTheme()` for all icons
- Use `KStandardAction` where applicable
- GPLv3 license
