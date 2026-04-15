## Long-term project state

> **Read this section first if you are starting a new session.** Corbomite is in the middle of a multi-cluster Obsidian-compatibility roadmap. State persists across sessions in dedicated files; do not infer status from conversation context.

**Single source of truth for "where we are":** [`docs/PROJECT-STATE.md`](docs/PROJECT-STATE.md). Read it at session start. It tracks current focus, in-flight cluster work, recent decisions, and open questions.

**Operational rituals (how to start a session, mark phase done, mark cluster done):** [`docs/CONTRIBUTING-OPS.md`](docs/CONTRIBUTING-OPS.md). Three checklists. Follow them — they are not advice.

**Cluster plans (one per work cluster A–P) + parallel long-term internal refactors:** [`docs/superpowers/plans/INDEX.md`](docs/superpowers/plans/INDEX.md). 5 full plans + 3 stubs + 3 scouting docs + the qutepart-corbomite-fork plan as of 2026-04-15.

**Parallel internal refactors** (not cluster-numbered, run alongside parity work): the **Qutepart-Corbomite fork** at [`docs/superpowers/plans/2026-04-15-qutepart-corbomite-fork.md`](docs/superpowers/plans/2026-04-15-qutepart-corbomite-fork.md) (8-phase shaping of vendored `qutepart-cpp` into our permanent Source-mode widget at `libs/qutepart-corbomite/`; Phase 1 unblocks Cluster E). See PROJECT-STATE §"Parallel long-term internal refactors" for status.

**Reverse-engineered Obsidian audit (canonical reference, read-only except via addenda):** [`docs/obsidian-audit/`](docs/obsidian-audit/). Pass 1 taxonomy + 15 Pass 2 domain docs + 5 Pass 3 synthesis docs (`FEATURE-MATRIX.md`, `VAULT-FORMAT.md`, `GAP-ANALYSIS.md`, `PLUGIN-API-SKETCH.md`, `SHARED-SYMBOLS.md`). ~94k words of distilled spec. New facts discovered during implementation go in `docs/obsidian-audit/addenda/`, never as edits to the audit docs.

**Local KDE source (do not clone from invent.kde.org):** `~/src/kde/src/<repo>` is checked out for kate, kdevelop, kio, kconfig, kparts, kxmlgui, kwidgetsaddons, ktexteditor, krunner, baloo, okular, poppler, qtkeychain, sonnet (and more). Cluster plans reference these by absolute local path.

**Session-start ritual (TL;DR — full version in `CONTRIBUTING-OPS.md`):**
1. Read this CLAUDE.md.
2. Read `docs/PROJECT-STATE.md` top-to-bottom.
3. Read the cluster plan(s) for the current focus.
4. Read audit-doc sections cited in the plan.
5. Glance at `git log --oneline -10`.
6. State the situation back: "Per PROJECT-STATE, current focus is X, last touched Y; next step is Z. Confirm or redirect?" — wait for confirmation before working.

---

## Building

Configure and build with the dev build flag:

```bash
cmake -B build -DCORBOMITE_DEV_BUILD=ON
cmake --build build
```

Run:
```bash
./build/Corbomite
```

Run tests:
```bash
cd build && ctest --output-on-failure
```

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
