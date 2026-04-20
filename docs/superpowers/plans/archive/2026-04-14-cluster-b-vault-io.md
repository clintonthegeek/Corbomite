# Cluster B — Vault I/O

> **Living-status note:** This file is the *plan*. Live status (Not started / In progress / Done / Blocked) is in [`docs/PROJECT-STATE.md`](../../PROJECT-STATE.md) Roadmap. Update PROJECT-STATE per the rituals in [`docs/CONTRIBUTING-OPS.md`](../../CONTRIBUTING-OPS.md), not this file. Edit this file only when the plan itself changes (work breakdown, target classes, references).

**Plan written:** 2026-04-14. Derived from `docs/obsidian-audit/GAP-ANALYSIS.md` §Cluster B.

**Covers:** P0.6 (`.obsidian/workspace.json` schema mismatch), P1.1 (`.obsidian/*.json` round-trip preserving unknown keys), P1.2 (full workspace.json compat), P1.9 (`Vault.process` atomic read-modify-write), P1.10 (`DataAdapter` missing ops — `exists`, `rmdir`, atomic rename, mtime-hint-aware write), P1.11 (case-sensitivity probe on vault open), P1.12 (`.trash/` local-trash mode), P2.11 (`userIgnoreFilters` wired to scans).

## Goal

Make Corbomite a **faithful read/write participant** in the `.obsidian/` config-directory contract. The single universal compat principle surfaced by synthesis is *round-trip preserves unknown keys* (`VAULT-FORMAT.md §1`) — if Corbomite opens a vault Obsidian wrote, edits anything, and writes back, no plugin-stashed or future-Obsidian key may be dropped. Additionally, `DataAdapter` needs a handful of ops that are currently missing but required by plugin data.json save suppression and atomic frontmatter mutation.

## Audit references

- **`.obsidian/` file roster + schemas:** `VAULT-FORMAT.md §3` — consolidated from `domains/vault.md §3` (Vault write sites), `domains/settings.md §3` (core-plugins.json / community-plugins.json / hotkeys.json), `domains/workspace.md §3` (workspace.json + workspace-mobile.json), `domains/plugin.md §3` (`.obsidian/plugins/<id>/data.json`).
- **Unknown-key preservation:** `domains/vault.md §8` (invariant), confirmed across all four domains above — `Object.assign({}, existing, incoming)` pattern.
- **`DataAdapter` ops:** `domains/vault.md §1` — `exists(path)`, `rmdir(path)`, atomic `rename(from, to)`, `write(path, data, {mtime, ctime}?)` with optional mtime-hint honoured by the external-edit watcher.
- **Case-sensitivity probe:** `domains/vault.md §1` — Obsidian writes/reads a probe file on vault open and caches the result; case-sensitive-vs-case-insensitive filesystem informs all path comparisons.
- **`.trash/` mode:** `domains/vault.md §1` — trash-to-vault-`.trash/` vs trash-to-system; configurable; compat with Obsidian's scheme matters for restore.
- **`userIgnoreFilters`:** `domains/vault.md §1` — glob-list Obsidian honours to exclude paths from scans; not currently wired in Corbomite.
- **`Vault.process`:** `domains/vault.md §1` — atomic read-modify-write function (different from `processFrontMatter`); takes a mutator over the whole note body.
- **Plugin data.json mtime-hint echo-suppression:** `domains/plugin.md §5` — write mtime passed as hint; watcher compares `freshMtime === _lastWriteMtime` and no-ops on match.
- **workspace.json schema:** `domains/workspace.md §3` (5 SplitNode variants discriminated by `type`) — the concrete target for P0.6.

## Target classes

| Class | File | Notes |
|---|---|---|
| `Corbomite::VaultConfig` | `libs/storage/src/VaultConfig.{h,cpp}` | Typed getters per `.obsidian/<file>.json`; transparent unknown-key preservation via opaque `QJsonObject` tail |
| `Corbomite::DataAdapter` | `libs/storage/include/corbomite/storage/DataAdapter.h` (new) | Interface: `exists`, `read`, `write(path, data, WriteHints{mtime?})`, `rename` (atomic), `remove`, `rmdir`, `list`, `stat` |
| `Corbomite::FileSystemAdapter` | already exists at `libs/storage/src/FileSystemAdapter.{h,cpp}` | Extend to implement new `DataAdapter` surface; add `QSaveFile` atomic rename |
| `Corbomite::WorkspaceState` | `libs/storage/src/WorkspaceState.{h,cpp}` | Parser/writer for `.obsidian/workspace.json` 5-variant SplitNode tree |
| `Corbomite::CaseSensitivityProbe` | `libs/storage/src/CaseSensitivityProbe.{h,cpp}` | One-shot vault-open probe; caches result |
| `Corbomite::VaultTrash` | `libs/storage/src/VaultTrash.{h,cpp}` | `.trash/`-mode implementation with Obsidian-compatible naming |
| `Corbomite::IgnoreFilter` | `libs/storage/src/IgnoreFilter.{h,cpp}` | Glob-matcher reading `userIgnoreFilters` from app.json |

Refactor to `VaultScanner` to consult `IgnoreFilter` before recursing.

## KDE / GPL3-compatible prior art

**Local KDE source convention:** the KDE source tree is checked out locally at `~/src/kde/src/<repo>`. **Always grep there first; never clone from `invent.kde.org` unless a repo is genuinely missing locally.** Verified-present locally: `kate`, `kdevelop`, `kio`, `kconfig`, `kconfigwidgets`, `kparts`, `kxmlgui`, `kwidgetsaddons`, `ktexteditor`, `krunner`, `baloo`, `okular`, `poppler`, `qtkeychain`, `sonnet`.

| Target | Local path | What we're looking for |
|---|---|---|
| Atomic write + mtime-hint | `QSaveFile` (Qt6 native, already usable) | Confirmed — native. Extend with `utime(file, mtime)` call post-commit |
| Unknown-key-preserving JSON | `~/src/kde/src/kconfig/` — `src/core/kcoreconfigskeleton.cpp` for typed+untyped mixed access | Partial — KConfig is INI-shaped; we need JSON preservation. Study skeleton for the pattern, then port |
| Case-sensitivity probe | `~/src/kde/src/kio/` — search for `caseSensitivity()` references in `KFileItem`/`KCoreDirLister` | Likely thin coverage; build from scratch: write `.obsidian/.case-probe-ABC`, read `.case-probe-abc`, compare inode |
| Glob ignore filter | `~/src/kde/src/kio/src/core/kdirlister.cpp` (`nameFilters`), Qt native `QDirIterator` | Per-directory filter; IgnoreFilter wraps QRegularExpression with QGlobMatcher |
| Trash file naming convention | `~/src/kde/src/kio/src/core/trash/` (KIO::trash freedesktop impl) | Only relevant for system-trash mode; local-`.trash/` uses Obsidian's scheme |
| JSON schema parsing + variant | Qt native `QJsonDocument` (no external lib needed) | Sufficient for 5-variant discriminated unions via `type:` field |

## Work breakdown

**Phase 1 — DataAdapter interface:**
1. Define `Corbomite::DataAdapter` abstract interface in `libs/storage/include/corbomite/storage/DataAdapter.h`. Include `WriteHints` struct with optional `qint64 mtime`.
2. Extend `FileSystemAdapter` to implement the full interface. Replace any direct `QFile` usage in callers with `DataAdapter`.
3. Implement `FileSystemAdapter::write` such that a `WriteHints{mtime}` argument both writes and then calls `utime()` to stamp the file. This is the mtime-hint contract that suppresses echo-reloads.
4. Unit test: write + read-back-stat + verify mtime matches hint within 1ms.

**Phase 2 — VaultConfig with unknown-key preservation:**
5. Implement `Corbomite::VaultConfig`. Each config file (`app.json`, `appearance.json`, `core-plugins.json`, `community-plugins.json`, `hotkeys.json`) has: a typed struct for known fields, plus `QJsonObject unknown` for anything unrecognised.
6. Load: parse JSON, extract known fields into typed struct, move residual keys into `unknown`.
7. Save: merge typed struct over `unknown` (typed wins on key conflict) and serialise. Round-trip test: write JSON with extra `_pluginXKey: {...}` field, load, save, assert key survives.
8. Migration handler for `core-plugins.json` array-format → object-format (`settings.md §3`).

**Phase 3 — WorkspaceState (5-variant SplitNode):**
9. Define `SplitNode` as `std::variant<SplitSplit, SplitTabs, SplitLeaf, SplitFloating, SplitWindow>` discriminated by `type:` field. Use `QJsonObject` tail per node for unknown keys.
10. Implement `WorkspaceState::load(path) → optional<WorkspaceState>` and `WorkspaceState::save(state, path)`. Compare output byte-equality against Obsidian-written sample `workspace.json` (fixture).
11. Bridge to existing `SessionManager`: SessionManager stops writing its current schema, delegates to WorkspaceState. Existing Corbomite sessions migrate on load (detect old schema via field-shape, convert).

**Phase 4 — Case-sensitivity probe + .trash + IgnoreFilter:**
12. `CaseSensitivityProbe::probe(vaultRoot)` — write `.obsidian/.case-probe-ABC`, attempt read of `.case-probe-abc`, report result. Cache on `VaultService`.
13. `VaultTrash::moveToTrash(file)` with `mode: {VaultTrash, SystemTrash}`. VaultTrash mode uses Obsidian-compatible file naming (timestamped).
14. `IgnoreFilter::fromConfig(VaultConfig::userIgnoreFilters)`. Wire into `VaultScanner` — scanner consults filter before recursing into a directory or emitting a note.

**Phase 5 — Vault.process atomic RMW:**
15. `Corbomite::VaultService::process(path, mutator: function<QString(QString)>)`. Uses `QSaveFile` + retry-on-conflict. Returns success/failure.
16. Integration test: simultaneous writers from two threads, final content is one mutator's output applied to the other's output (serial), never a lost-update.

**Phase 6 — End-to-end round-trip:**
17. Take a sample Obsidian-written vault (with plugin data, workspace.json, unknown app.json keys, community plugins list). Open in Corbomite, make no edits, close. Assert byte-equality of every `.obsidian/*.json` and `.obsidian/plugins/*/data.json`.

## Explore-agent dispatch prompts

**Prompt 1 — Sample vault procurement:**
> Check if `/home/clinton/dev/Corbomite/testvaults/` already contains an Obsidian-written vault with representative `.obsidian/` content (non-default `app.json`, a populated `workspace.json`, at least one plugin's `data.json`). If not, recommend a minimal setup procedure to create one from scratch using Obsidian itself. Report vault paths + file contents inventories. Under 400 words.

**Prompt 2 — `SessionManager` migration plan:**
> Read `src/editor/SessionManager.{h,cpp}` and `src/editor/EditorViewManager.{h,cpp}` (specifically `buildSessionState` and `restoreFromSession`). Compare the existing session-JSON shape against Obsidian's workspace.json schema documented in `docs/obsidian-audit/domains/workspace.md §3` and `docs/obsidian-audit/VAULT-FORMAT.md §3.workspace.json`. Report: (a) fields Corbomite has that Obsidian doesn't, (b) fields Obsidian has that Corbomite doesn't, (c) recommended migration strategy for existing Corbomite session files. Under 700 words.

**Prompt 3 — KConfig unknown-key investigation:**
> Investigate whether `KConfig` / `KConfigGroup` / `KCoreConfigSkeleton` from KDE Frameworks 6 can be used for JSON-shaped config files where unknown keys must round-trip. **Read the local KDE source at `~/src/kde/src/kconfig/src/core/`** (do NOT clone from upstream) — specifically `kcoreconfigskeleton.cpp` and `kconfiggroup.cpp`. If not directly applicable, what's the smallest wrapper needed? Compare against a from-scratch `QJsonObject`-based `VaultConfig` approach for this cluster's Phase 2 work. Under 400 words. Recommendation.

## Definition of done

- All `.obsidian/*.json` files and `.obsidian/plugins/*/data.json` round-trip byte-identically when no field is edited.
- `DataAdapter` is the sole file I/O entry point in `libs/storage/`; all operations are atomic via `QSaveFile`.
- `FileSystemAdapter::write` honours `WriteHints{mtime}`; external-edit watcher observes matching mtime and no-ops.
- `workspace.json` load/save matches Obsidian byte-for-byte on a fixture sample; `SessionManager` delegates to `WorkspaceState`.
- Case-sensitivity probe runs once per vault open; result available to `SQLiteIndex` + `LinkResolver` (from Cluster A).
- `IgnoreFilter` respects `userIgnoreFilters` from app.json; `VaultScanner` honours it.
- `.trash/` mode writes Obsidian-compatible file names (trash entries visible from Obsidian's Files-and-Links → Show deleted files UI).

## Blocks / enables

- **Depends on:** Cluster A (YAML + linkutils used by some config parsers).
- **Blocks:** Cluster C (plugin data.json needs DataAdapter mtime-hint), Cluster E (three-mode encoding must serialise into WorkspaceState), Cluster G (ViewRegistry's factory persistence needs VaultConfig), Cluster H (plugin-settable registries persist via DataAdapter).
- **Enables:** opening any Obsidian vault without data loss. Everything downstream.
- **Estimated effort:** 3–4 weeks one engineer; overlaps cleanly with Cluster A (Phase 1 here can run while Phase 1–2 of Cluster A are in flight).
