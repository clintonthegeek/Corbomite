# Cluster A — Link / frontmatter correctness

> **Living-status note:** This file is the *plan*. Live status (Not started / In progress / Done / Blocked) is in [`docs/PROJECT-STATE.md`](../../PROJECT-STATE.md) Roadmap. Update PROJECT-STATE per the rituals in [`docs/CONTRIBUTING-OPS.md`](../../CONTRIBUTING-OPS.md), not this file. Edit this file only when the plan itself changes (work breakdown, target classes, references).

**Plan written:** 2026-04-14. Derived from `docs/obsidian-audit/GAP-ANALYSIS.md` §Cluster A.

**Covers:** P0.1 (link resolver ambiguity), P0.2 (frontmatter EOF close), P0.3 (subpath not stripped before indexing), P0.5 (unsplit wikilinks in SQLiteIndex), P1.6 (YAML library + frontmatter helpers), P1.7 (`parseLinktext` / `stripHeading` / `resolveSubpath`), P1.8 (`processFrontMatter` atomic mutator).

## Goal

Make Corbomite's link-resolution and frontmatter-parsing pipeline **behaviourally identical** to Obsidian's. These are the load-bearing correctness bugs that silently corrupt vaults today: wikilinks resolve to the wrong file under basename ambiguity, frontmatter ending in `---` at EOF is rejected, and wikilinks with `#heading` or `#^block` subpaths are stored and resolved as a single string, which breaks scroll-to-heading entirely.

Single cohesive cluster because every gap here reads the *same* Obsidian files and writes the *same* Corbomite libraries. Splitting the work across sprints would produce merge hell.

## Audit references

- **Link resolver algorithm (6-step shortest-path-wins):** `domains/metadata.md §8` (invariant list) + §11 (Corbomite mapping) — cites `libs/storage/src/SQLiteIndex.cpp:592-609` (`resolveWikilink`) as the site that must change.
- **Frontmatter delimiter regexes:** `domains/parsing.md §8` — opening `Yx = /^---(\r?\n)/g`, closing `Qx = /---(\r?\n|$)/g` (**EOF-tolerant** — current Corbomite misses this).
- **YAML library options:** `domains/parsing.md §2` — `eemeli/yaml` v2, YAML 1.2 strict, `nullStr: ""`, `lineWidth: 0`, `aliasDuplicateObjects: false`, no YAML 1.1 coercions.
- **`parseLinktext` location:** `domains/vault.md §1` (lives in `vault/`, not `parsing/` despite the name — two-line `#`-split returning `{path, subpath}`).
- **Strip-heading regexes:** `domains/leaf-utilities.md §15` (`AT` wide-strip for display, `PT` narrow-strip for wikilink generation).
- **`resolveSubpath` algorithm:** `domains/leaf-utilities.md §1` — dispatches on `^block`, `[^footnote]`, or heading; case-insensitive heading match.
- **`processFrontMatter` atomic contract:** `domains/vault.md §1` (cites `FileManager.processFrontMatter`) — loads, parses, mutates, stringifies, writes; **silently drops YAML comments** (compat limitation, preserved).
- **`VAULT-FORMAT.md §4`** — canonical frontmatter + link syntax for cross-reference.

## Target classes

| Class | File | Notes |
|---|---|---|
| `Corbomite::FrontMatter` | `libs/core/src/FrontMatter.{h,cpp}` | Wraps `yaml-cpp` (or `eemeli/yaml-port`) with Obsidian-compatible options |
| `Corbomite::LinkUtils` | `libs/core/src/LinkUtils.{h,cpp}` | `parseLinktext`, `resolveSubpath`, `stripHeading`, `stripHeadingForLink` |
| `Corbomite::LinkResolver` | `libs/storage/src/LinkResolver.{h,cpp}` | 6-step shortest-path-wins + same-folder preference; replaces `SQLiteIndex::resolveWikilink` |
| `Corbomite::FrontMatterWriter` | `libs/core/src/FrontMatterWriter.{h,cpp}` | Atomic read-modify-write for `processFrontMatter` semantics |

`SQLiteIndex` schema also changes — `wikilinks` table gains `subpath` column; `resolveWikilink` returns `{path, subpath}`. Migration step included.

## KDE / GPL3-compatible prior art

**Local KDE source convention:** the KDE source tree is checked out locally at `~/src/kde/src/<repo>`. **Always grep there first; never clone from `invent.kde.org` unless a repo is genuinely missing locally.** Verified-present locally: `kate`, `kdevelop`, `kio`, `kconfig`, `kconfigwidgets`, `kparts`, `kxmlgui`, `kwidgetsaddons`, `ktexteditor`, `krunner`, `baloo`, `okular`, `poppler`, `qtkeychain`, `sonnet`.

| Target | Local path | What we're looking for |
|---|---|---|
| YAML library | external — `yaml-cpp` upstream + `eemeli/yaml` reference | Exact option defaults; whether yaml-cpp can be configured to match |
| Path resolution | `~/src/kde/src/ktexteditor/` (`KateDocument::url()` handling), `~/src/kde/src/kio/` (`KFileItem`, path normalisation) | Relative-path shortcuts; case-handling patterns |
| Atomic file mutation | `QSaveFile` (Qt6 native — no clone needed), `~/src/kde/src/ktexteditor/src/document/katedocument.cpp` (`save()`) | fsync + rename-on-top-of pattern |
| Heading-text normalisation | `~/src/kde/src/kdevelop/` — grep `kdevplatform/language/duchain/` for `qualifiedIdentifier` slugification | Prior art on the strip-for-matching vs strip-for-display distinction |

## Work breakdown

**Phase 1 — Libraries (no integration yet):**
1. Add `yaml-cpp` (or chosen alternative) to CMake dep list; confirm null handling + no YAML 1.1 coercions are configurable.
2. Implement `Corbomite::FrontMatter::parse(QString) → optional<YamlNode>` and `Corbomite::FrontMatter::stringify(YamlNode) → QString` with the Obsidian option set. Unit test against the audit's exact regexes (incl. EOF-close case).
3. Implement `Corbomite::LinkUtils::parseLinktext(QString) → {path, subpath}` — two-line-ish implementation, full unit-test suite including edge cases (`[[#heading]]`, `[[#^block]]`, `[[path with spaces#h]]`).
4. Implement `Corbomite::LinkUtils::stripHeading` + `stripHeadingForLink` with the two distinct regex tables from `leaf-utilities.md §15`.
5. Implement `Corbomite::LinkUtils::resolveSubpath` — dispatch on `^`, `^footnote`, heading; case-insensitive heading match.

**Phase 2 — LinkResolver:**
6. Write 6-step algorithm in `Corbomite::LinkResolver`. Signature: `resolve(sourcePath: QString, target: QString) → optional<QString>`. Steps per `metadata.md §8`: exact path → same-folder relative → … → shortest-path-wins on basename match.
7. Comprehensive test vault covering every ambiguity case. Use a synthetic mini-vault in `tests/fixtures/`.
8. Replace `SQLiteIndex::resolveWikilink` (line 592) with delegation to `LinkResolver`. Migrate `wikilinks` table schema to store `subpath` separately (add column with default `""`).

**Phase 3 — Atomic mutator + integration:**
9. Implement `Corbomite::FrontMatterWriter::process(filePath, mutator: function<void(YamlNode&)>)`. Uses `QSaveFile`. Document the compat limitation (comments dropped) and write regression test.
10. Wire existing call sites that currently do ad-hoc frontmatter edits through `FrontMatterWriter`.
11. End-to-end test: open a vault with ambiguous-basename notes + heading subpaths + EOF-closed frontmatter + YAML comments in frontmatter. Verify all behaviours match documented Obsidian contract.

## Explore-agent dispatch prompts

**Prompt 1 — YAML library decision:**
> Explore GPLv3-compatible YAML libraries available as CMake dependencies for a Qt6/C++ project. Primary candidates: `yaml-cpp`, `RapidYAML`, `libfyaml`. Evaluate each for: YAML 1.2 strict mode availability, configurable null stringification (`null` → `""`), configurable line-width-0 (no wrapping), comment-preservation (if available), round-trip fidelity (key order preservation). Compare against Obsidian's `eemeli/yaml` v2 option set documented in `docs/obsidian-audit/domains/parsing.md §2`. Under 500 words. Recommendation + second-choice fallback.

**Prompt 2 — SearchMatch schema change feasibility:**
> Read `libs/storage/src/SQLiteIndex.{h,cpp}` and `libs/storage/include/corbomite/storage/SQLiteIndex.h`. Enumerate existing callers of `resolveWikilink`. Identify all schema-migration surfaces (existing `wikilinks` table rows) and recommend migration approach for adding a `subpath TEXT NOT NULL DEFAULT ''` column. Report the list of code changes needed to propagate `subpath` from insert through resolve. Under 500 words.

**Prompt 3 — Prior art for shortest-path-wins:**
> Search KDevelop locally at `~/src/kde/src/kdevelop/kdevplatform/language/duchain/` for any symbol resolution that disambiguates by shortest-path to a source file. Report whether the pattern is directly transferable to Corbomite's link resolution or whether it's too specific to C++ AST navigation. Do NOT clone from upstream — the local source is current. Under 400 words.

## Definition of done

- All P0.1–P0.3 + P0.5 bugs fixed; tests demonstrate ambiguity-resolution, EOF-close, and heading-subpath behaviour matches audit-documented Obsidian.
- `libs/core/` exposes `FrontMatter`, `LinkUtils`, `FrontMatterWriter` as a clean public API.
- `libs/storage/LinkResolver` is the sole wikilink-resolution entry point; old `SQLiteIndex::resolveWikilink` deleted.
- `yaml-cpp` (or chosen lib) committed to CMake deps; option defaults documented in `libs/core/README.md` or equivalent.
- Frontmatter round-trip preserves key order and unknown keys (preserves-unknown-keys principle from `VAULT-FORMAT.md §1`).
- Zero regressions in existing tests; new tests land under `tests/core/` + `tests/storage/`.

## Blocks / enables

- **Blocks:** Cluster B (vault I/O unknown-key round-trip), Cluster D (search match surface depends on link shape), Cluster I (MetadataCache needs parsed subpaths), Cluster K (Bases reads frontmatter).
- **Enables:** any P2 feature that follows wikilinks (backlinks, graph, hover-preview, embed, three-mode preview).
- **Estimated effort:** 2–3 weeks one engineer; 1.5 weeks with a focused two-engineer pair.
