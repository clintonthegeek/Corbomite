# Cluster J — Qutepart-Corbomite Fork

> **Re-lettered 2026-04-26.** Was the standalone "Parallel long-term internal refactor" tracked outside the cluster scheme. Folded into the new A-onwards lettering as **Cluster J**. Phases 1+2 done (2026-04-15); Phase 3 (public find/replace API) is next.


> **Living-status note:** This file is the *plan*. Live status in [`docs/PROJECT-STATE.md`](../../PROJECT-STATE.md). Update PROJECT-STATE per [`docs/CONTRIBUTING-OPS.md`](../../CONTRIBUTING-OPS.md). Edit this file only when the plan itself changes.

**Plan written:** 2026-04-15. Derived from [`docs/superpowers/specs/2026-04-15-qutepart-corbomite-fork-design.md`](../specs/2026-04-15-qutepart-corbomite-fork-design.md).

**Covers:** multi-phase shaping of a vendored `qutepart-cpp` fork into Corbomite's permanent Source-mode widget library at `libs/qutepart-corbomite/`.

**Type:** long-term internal refactor (parallel to parity roadmap). Phases are independently shippable; they do **not** have to land in one session.

**Relationship to Cluster E:** Phase 1 here unblocks Cluster E Phase 0 (Source-mode widget bootstrap). Phase 2 here unblocks Cluster E Phase 1 (ViewMode encoding, which needs a real Source-mode widget with a scroll API). Phases 3–8 run asynchronously after Cluster E is well underway.

## Goal

Vendor `qutepart-cpp` (upstream commit `eec2e9ae5b50b591f017296ee743ee2860a280e4`, 2026-04-12) at `libs/qutepart-corbomite/` and shape it over 8 phases into a narrower, markdown-specialised, KSyntaxHighlighting-powered Source-mode widget that we own and maintain internally. See the companion spec for full architectural rationale.

## Target state (post-Phase 8)

- `libs/qutepart-corbomite/` builds standalone, ~5–6 KLOC, no bundled Kate XML, no bundled themes.
- Uses KSyntaxHighlighting for all highlighting (single engine across the Corbomite app).
- `Corbomite::SourceEditor` shim exposes visual-line float scroll, fold state serialization, find/replace public API, markdown-specific wiki-link/tag/frontmatter awareness.
- MIT + GPL-3.0-or-later dual-header on inherited files; GPL-3.0-or-later on new files; `PROVENANCE.md` tracks all divergences.
- No dependency on upstream qutepart-cpp's release cadence. Monthly manual check of upstream commits for cherry-pick candidates (first 6 months), then quarterly.

## Phases

### Phase 1 — Vendor + CMake integration + smoke test

**Goal:** Source tree contains `libs/qutepart-corbomite/` at upstream HEAD; builds as part of top-level Corbomite; one smoke test passes.

**Steps:**

1. Copy `~/src/qutepart-cpp/` into `libs/qutepart-corbomite/` verbatim. Exclude upstream's `.git/`, CI workflows, upstream `README.md` (will be replaced with our own), upstream tests that reference its standalone structure.
2. Add `PROVENANCE.md` at library root with:
   - Upstream commit hash (`eec2e9ae5b50b591f017296ee743ee2860a280e4`)
   - Fork date (2026-04-15)
   - Upstream URL
   - Fork rationale (one paragraph, cite the design spec)
   - Empty "Modified files" and "Added files" sections (populated in later phases)
3. Add SPDX dual-header block to every inherited `.cpp` / `.h` file:
   ```
   // SPDX-License-Identifier: MIT AND GPL-3.0-or-later
   // Originally from qutepart-cpp © 2024 Diego Iastrubni, MIT.
   // Fork point: commit eec2e9a, 2026-04-12.
   // Modifications © 2026 Corbomite contributors, GPL-3.0-or-later.
   ```
   Automate with a shell one-liner; manual-check a few afterward to verify formatting.
4. Write `libs/qutepart-corbomite/CLAUDE.md` matching the `libs/markoff/CLAUDE.md` encapsulation pattern. Agents working in the library should stay within it.
5. Write a new `libs/qutepart-corbomite/README.md` replacing upstream's. Briefly note: "Fork of qutepart-cpp. See `PROVENANCE.md` for upstream details; see `docs/` for architecture."
6. Replace upstream's `CMakeLists.txt` with a Corbomite-style one:
   - Target name: `qutepart-corbomite` (static lib).
   - Namespace alias: `Corbomite::QutepartSource`.
   - Links: Qt6::Core, Qt6::Widgets. (KSyntaxHighlighting added in Phase 4.)
   - `target_include_directories(... PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)`.
   - No install rules (internal library).
7. Adjust the public header layout. Upstream's public headers live at `include/qutepart/`. Keep that for Phase 1 (minimize diff); rename to `include/corbomite/qutepart/` in Phase 8.
8. Add `add_subdirectory(libs/qutepart-corbomite)` to the top-level `CMakeLists.txt`.
9. Verify build: `cmake -B build -DCORBOMITE_DEV_BUILD=ON && cmake --build build --target qutepart-corbomite`.
10. Port **one** upstream test (smoke test — instantiate widget, set plain text, read it back, verify) into `libs/qutepart-corbomite/tests/`. Wire it into CTest. Delete the rest of upstream's tests — we'll write our own as we shape.
11. Smoke-run the test executable under `QT_QPA_PLATFORM=offscreen`.
12. Commit: "vendor: fork qutepart-cpp into libs/qutepart-corbomite/ at eec2e9a".

**Definition of done:** Library builds; smoke test green; PROVENANCE.md + CLAUDE.md + README.md in place; SPDX headers on all inherited files.

**Unblocks:** Cluster E Phase 0b.

### Phase 2 — Corbomite-facing API shim + visual-line float scroll

**Goal:** `Corbomite::SourceEditor` shim in `src/editor/SourceEditor.{h,cpp}` with the full app-facing API; scroll is visual-line float; cursor is (line, column); fold state is `QVector<int>` of folded line indices.

**Steps:**

1. Write `Corbomite::SourceEditor` as a `QWidget` composing a private `Qutepart *` child. Public API per the spec §"Corbomite-facing API".
2. Implement `scrollPosition() → float`:
   ```
   firstBlockNumber = firstVisibleBlock().blockNumber();
   fractionalOffset = - blockBoundingGeometry(firstVisibleBlock()).top() / blockBoundingRect(firstVisibleBlock()).height();
   return firstBlockNumber + fractionalOffset;
   ```
3. Implement `setScrollPosition(float visualLine)`:
   - Find block at floor(visualLine).
   - Pixel-scroll to block-top + fraction * block-height.
4. Tests:
   - Round-trip a cursor position through `EphemeralState` fixture.
   - Round-trip a scroll position at integer and fractional values.
   - Reflow (change widget width, force re-layout) and verify `setScrollPosition(x); y = scrollPosition();` restores within ±0.5 visual-line tolerance.
5. Wire `Corbomite::SourceEditor` into `NoteEditorWidget` as the third stacked widget (Cluster E Phase 7 will finalise the mode-switch glue; for now just prove the widget mounts and responds).
6. Commit: "feat(source): Corbomite::SourceEditor shim with visual-line float scroll".

**Definition of done:** `Corbomite::SourceEditor` exposes the full public API; scroll tests pass; widget mounts inside NoteEditorWidget.

**Unblocks:** Cluster E Phase 1 and Phase 2.

### Phase 3 — Public find/replace API

**Goal:** Upstream's internal find/replace engine is exposed through the `Corbomite::SourceEditor` public API. Supports literal + regex + case-sensitive + word-boundary + wrap.

**Steps:**

1. Survey upstream's internal find/replace code (engine exists, no public API per the fitness report). Identify the entry points.
2. Add minimal public methods on the forked `Qutepart` class: `findNext(QString, FindFlags) → bool`, `findPrevious(...)`, `replaceCurrent(QString)`, `replaceAll(...)`.
3. Expose these through `Corbomite::SourceEditor::find` / `replaceAll`.
4. Unit tests for each flag combination.
5. Commit: "feat(source): expose public find/replace API".

**Definition of done:** `Corbomite::SourceEditor::find` works for all flag combinations; tests pass.

### Phase 4 — KSyntaxHighlighting bridge + FoldCalculator extraction (the big one)

**Goal:** The Kate-XML syntax engine in `src/hl/` is deleted. Highlighting is driven by `KF6::SyntaxHighlighting`. Fold is driven by a new `FoldCalculator` that consumes KSyntaxHighlighting tokens + markdown heading hierarchy from `libs/markoff-parser`.

**This phase is 2–3 sessions. Strongly recommend running it with the full attention-span of its own plan expansion.**

**Steps:**

1. **Survey the fusion.** Read `src/hl/language.cpp` carefully. Document which code paths emit highlighting-formats vs. which emit fold metadata (`TextBlockUserData::FoldingData`). Produce a "fusion map" as a short doc.
2. **Build `FoldCalculator` in parallel.** New class, greenfield. Input: a `QTextBlock` and the tokens emitted by KSyntaxHighlighting for that block, plus (optionally) a reference to a `markoff-parser` `Document`. Output: `FoldingData { level, folded }`. Initially runs *alongside* the existing engine — both compute fold data, we log divergence.
3. **Build `QutepartKSyntaxHighlighter : QSyntaxHighlighter`.** Drives off `KSyntaxHighlighting::SyntaxHighlighter` for format ranges; delegates to `FoldCalculator` for fold metadata. Initially runs *alongside* the existing `SyntaxHighlighter`.
4. **Parallel-run test.** For a fixture set of 20 markdown files, run both engines, assert fold metadata matches and format ranges match (within the tolerance that KSyntaxHighlighting's markdown styles != upstream's bundled markdown XML styles; this is expected and fine).
5. **Flip the default.** `Qutepart::setHighlighter(...)` starts routing through `QutepartKSyntaxHighlighter` for `languageId == "markdown"`. Other languages fail-loud — we don't support them.
6. **Delete old engine.** Remove `src/hl/` (loader, language, rules, context, etc.), `syntax/` directory, `syntax-files.qrc`, `regenerate-language-db.py`, `generate-php.pl`. Update CMakeLists.txt. Update resources.qrc.
7. **Add KSyntaxHighlighting link.** `target_link_libraries(qutepart-corbomite PRIVATE KF6::SyntaxHighlighting)`.
8. **Measure binary-size delta.** Record in `PROVENANCE.md`.
9. **Update PROVENANCE.md "Modified files" and "Deleted files" sections.**
10. Commit: "refactor(source): replace bundled Kate-XML engine with KSyntaxHighlighting bridge".

**Definition of done:** No files in `src/hl/` or `syntax/` remain in `libs/qutepart-corbomite/`. KSyntaxHighlighting is a hard dep. Fold metadata for markdown matches (or improves upon) the pre-swap output for the fixture set. Binary size reduced as measured.

**Unblocks:** Unified syntax engine across Corbomite app. Enables Phase 5.

### Phase 5 — Trim indent engines

**Goal:** Only markdown-relevant indent algorithms remain.

**Steps:**

1. Survey `src/indent/`. Upstream has ~8 algorithms: normal, C-style, Python, Ruby, Lisp, Scheme, XML, none.
2. Keep: **normal** (whitespace-respecting), and build **markdown-aware** (continues list markers, handles blockquote `>`, respects code-fence boundaries).
3. Delete: C-style, Python, Ruby, Lisp, Scheme, XML indent engines.
4. Update the `IndentAlg` enum and related API. Bump version in PROVENANCE.md.
5. Adjust any tests that referenced deleted algorithms.
6. Commit: "refactor(source): trim indent engines to markdown-only".

**Definition of done:** ~2,500 LOC deleted. Markdown-aware indent works on list markers + blockquotes + code fences (tests included).

### Phase 6 — Remove bundled themes; drive from KColorScheme

**Goal:** `themes/` directory deleted. Colors come from `QPalette` + KF6::ColorScheme at runtime. Dark/light mode responds to KDE theme change.

**Steps:**

1. Delete `themes/` directory and `qutepart-theme-data.qrc`.
2. Remove `Theme` class, `setTheme()` method. Replace with direct reads of `QApplication::palette()` + `KColorScheme` at paint time.
3. Wire up `QApplication::paletteChanged` signal to trigger re-highlight (KSyntaxHighlighting's theme is already palette-driven when configured right — verify).
4. Add a manual-test note to the library's `docs/`: run the app with KDE's system settings switched light ↔ dark, verify editor repaints without restart.
5. Commit: "refactor(source): remove bundled themes, drive from KColorScheme".

**Definition of done:** No theme JSON files in the tree. Dark/light mode switches at runtime.

### Phase 7 — Markdown-specific features

**Goal:** Wiki-links, tags, frontmatter boundary, and section-fold (driven by markdown heading hierarchy) are recognised by the widget.

**Steps:**

1. **Wiki-link rendering.** Scan each block for `[[target]]` / `[[target|display]]` using a regex post-pass on KSyntaxHighlighting output. Apply a distinct format range + emit `wikiLinkHovered` / `wikiLinkActivated` signals via `Corbomite::SourceEditor`.
2. **Tag rendering.** Same pattern for `#tag`. Use the tag syntax from Obsidian audit (`domains/editor-markdown.md` + audit §tags).
3. **Frontmatter boundary.** Detect `---\n…\n---` at document start via markoff-parser. Adjust fold behaviour: the frontmatter block folds as a unit (one level-0 fold region). Heading-fold for the body starts after.
4. **Section-fold.** Extend `FoldCalculator` (from Phase 4) to derive fold regions from the heading hierarchy reported by markoff-parser. A level-N heading's fold region extends until the next heading at level ≤ N. Persist in `EphemeralState.foldedHeadings` by source-line index of the heading.
5. **Wiki-link resolution.** `Corbomite::SourceEditor::setVaultResourceProvider(VaultResourceProvider *)` — when set, Ctrl+click on a wiki-link resolves the target and emits `wikiLinkActivated`.
6. Tests: wiki-link regex coverage, frontmatter fold boundary, section-fold mirrors heading hierarchy for a fixture with 5 heading levels.
7. Commit: "feat(source): markdown-specific wiki-link, tag, frontmatter, section-fold".

**Definition of done:** All four features live in the shim's tests. Manual smoke: open a real vault note with wiki-links + tags + frontmatter + multi-level headings, verify all surface correctly.

### Phase 8 — Rename / rebrand

**Goal:** Final namespace, CMake target, and public-header path. We are no longer a "qutepart fork"; we are "Corbomite's Source editor".

**Steps:**

1. Decide final names. Candidates:
   - Namespace: `Corbomite::SourceEdit::` / `Corbomite::Source::` / keep `Qutepart::` for file-level types. (Recommendation: `Corbomite::SourceEdit::` for new types; migrate incrementally from `Qutepart::` to avoid a flag-day.)
   - CMake target: `qutepart-corbomite` → `corbomite-source` / `sourceedit`. (Recommendation: `corbomite-source`, alias `Corbomite::Source`.)
   - Library directory: `libs/qutepart-corbomite/` → `libs/source-edit/` or keep. (Recommendation: keep — renaming affects every `add_subdirectory` and `#include` in the tree.)
   - Public header path: `include/qutepart/` → `include/corbomite/source/`.
2. Execute the rename. All at once, via a scripted global find/replace, in one commit.
3. Update every consumer (`Corbomite::SourceEditor` shim, tests, NoteEditorWidget).
4. Update CLAUDE.md, README.md, PROVENANCE.md.
5. Commit: "refactor(source): finalise naming as Corbomite::Source".

**Definition of done:** No `Qutepart::` namespace remains in new code (only inside the library's internal files, which we never expose). Header paths are `corbomite/source/*.h`.

---

## Phase dependency graph

```
Phase 1 ─┬─► Cluster E Phase 0b
         ├─► Phase 2 ─┬─► Cluster E Phase 1/2
         │            ├─► Phase 3
         │            └─► Phase 4 ─► Phase 5
         │                        └─► Phase 6
         │                        └─► Phase 7 ─► Phase 8
```

Phase 1 is a prerequisite for everything. Phases 2 and 3 can run in either order. Phase 4 must precede 5, 6, 7, 8. Phase 7 depends on 4 (shared FoldCalculator). Phase 8 is the last.

## Definition of done (whole project)

- `libs/qutepart-corbomite/` at ≤ 6,000 LOC, ≤ 1 MB non-code assets.
- Single syntax engine in the app (KSyntaxHighlighting), shared with Markoff and any other highlighted surface.
- `Corbomite::SourceEditor` is the Source-mode widget in `NoteEditorWidget`, with full visual-line float scroll persistence.
- Markdown-specific wiki-links, tags, frontmatter, and section-fold all work.
- All phases committed with clean provenance; `PROVENANCE.md` reflects every divergence from upstream `eec2e9a`.
- Upstream monitoring schedule (monthly for 6 months, then quarterly) documented.

## Blocks / enables

- **Blocks Cluster E.** Phases 1–2 are the unblock. Phases 3+ are parallel improvements.
- **Enables future:** cleaner path for adding a second editor surface (e.g. regex-editor in search panel) if we ever want one — the shaped fork becomes a reusable markdown-specialised primitive.

## Risks + mitigations

- **Upstream drift during fork.** Checklist-driven monthly upstream review for 6 months. Commits worth cherry-picking: bugfixes to `QPlainTextEdit`-derived behaviour, fold-logic bugs, cursor/scroll edge cases. Commits not worth cherry-picking: new language support, new themes, anything in `src/hl/` (we delete the whole directory in Phase 4).
- **Phase 4 fold-calc divergence.** Parallel-run mode (both engines producing fold data, assert equal) catches this before we delete. Keep parallel-run code available behind a compile flag for one release post-Phase 4.
- **Bundled Kate XML license drift.** Moot after Phase 4 — we delete all XML. Until Phase 4, individual file SPDX headers are preserved verbatim.
- **Naming churn in Phase 8.** Mitigated by doing the whole rename in one commit via scripted global replace.
