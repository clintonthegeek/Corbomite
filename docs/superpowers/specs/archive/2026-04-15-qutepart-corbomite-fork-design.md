# Qutepart-Corbomite Fork — Design Spec

## Overview

Vendor and fork [`qutepart-cpp`](https://github.com/diegoiast/qutepart-cpp) (MIT, Qt6, QPlainTextEdit-based) into `libs/qutepart-corbomite/` as Corbomite's permanent Source-mode widget. The fork shapes the upstream into our perfect widget: narrower scope (markdown + our needs only), unified highlight engine (KSyntaxHighlighting replaces the bundled Kate-XML parser), public find/replace API, visual-line float scroll, wiki-link awareness, and complete decoupling from upstream's release cadence.

This is a **long-term internal refactor** with its own multi-phase plan. The first phase (Phase 1 below — vendor + build-integration + smoke test) unblocks Cluster E Phase 0. The remaining phases happen asynchronously over subsequent sessions, with each phase independently shippable.

**Provenance at fork point:** commit `eec2e9ae5b50b591f017296ee743ee2860a280e4` (2026-04-12) — "Completion: make more context for text".

**Library location (proposed working name):** `libs/qutepart-corbomite/` with CMake target `Corbomite::QutepartSource`. The final name may change once our divergence from upstream passes a certain threshold — revisit during Phase 8 (rename / rebrand).

---

## Motivation

**Why fork, not depend.** Upstream qutepart-cpp is single-author (Diego Iastrubni), MIT-licensed, actively developed but bus-factor-1. Rather than take a runtime dependency on ongoing upstream maintenance, we take a permanent source-level snapshot and commit to maintaining it as our own. The MIT license explicitly permits this; we honour the copyright and SPDX attribution in every inherited file.

**Why reshape, not adopt as-is.** Upstream is a general-purpose Qt code editor. It carries features, languages, indent engines, and a syntax engine that duplicate infrastructure we already have (KSyntaxHighlighting) or serve use-cases we'll never have (indent algorithms for C / Python / Ruby / Lisp / Scheme / XML — we are a markdown editor). Shaping the fork to our exact needs:

- reduces the binary we ship (estimated 6–10 MB of syntax XML + unused indent engines removed)
- unifies syntax highlighting with the rest of the app (one Kate engine, not two)
- lets us add Obsidian-specific features (wiki-link token recognition, section-fold driven by markdown heading hierarchy) directly in the lexer/fold layer
- gives us a clean API surface (`Corbomite::SourceEditor`) without the upstream feature cruft

**Why now.** Cluster E Phase 0 needs a Source-mode widget. Vendoring is the unblock. The shaping work is multi-phase and can land incrementally — each phase is independently valuable and testable.

---

## Goals

1. Corbomite's Source-mode widget is **our code**, in our tree, under our CMake, on our release cadence.
2. Source-mode's syntax highlighting comes from **KSyntaxHighlighting** (same engine as Markoff, same engine as every other highlighted surface in the app).
3. Source-mode's scroll position is **visual-line float**, Obsidian-compatible.
4. Source-mode knows about **markdown-specific constructs**: wiki-links (`[[target]]`), tags (`#tag`), frontmatter block boundary, heading hierarchy (for fold-by-section).
5. The fork is **narrower and smaller** than upstream — only the features Corbomite uses are kept.
6. Our `libs/qutepart-corbomite/` builds standalone like `libs/markoff/`, with its own `CLAUDE.md`, `docs/`, and `tests/`, matching our encapsulation pattern.

## Non-goals

- **Merging back to upstream.** We're not maintaining a bidirectional sync. Upstream fixes that apply to shared core (fold logic, QPlainTextEdit interactions) may be cherry-picked manually; we don't commit to tracking upstream's roadmap.
- **Supporting non-markdown languages out of the box.** KSyntaxHighlighting can highlight anything, but the Source-mode widget is specialised for markdown — indent algorithms, brackets, auto-complete hooks are markdown-tuned. If we ever need a code editor elsewhere (settings JSON, custom regex editors), we'll expose a lower-level class.
- **Vim mode.** Upstream doesn't have it; we don't plan to build one. Users who need modal editing can use the Source-mode widget with external input methods.
- **Multi-language fold engine.** Fold is driven by markdown heading hierarchy + indentation, not by the generic Kate-XML `<fold>` rules. Simpler, more correct for our domain.

---

## Architecture

### Top-level shape

```
libs/qutepart-corbomite/
├── CLAUDE.md                # encapsulation doc (match markoff pattern)
├── CMakeLists.txt           # builds libqutepart-corbomite.a
├── LICENSE                  # MIT (inherited) + Corbomite GPLv3 header for new files
├── PROVENANCE.md            # upstream commit hash, fork date, diverge log
├── docs/
│   ├── architecture.md
│   ├── plans/               # fork-shaping phase plans
│   └── specs/
├── include/
│   └── corbomite/qutepart/  # public headers (renamed from upstream)
├── src/                     # impl
├── syntax/                  # removed in Phase 4 (replaced by KSyntaxHighlighting)
├── themes/                  # removed in Phase 6 (replaced by KColorScheme)
└── tests/
```

### Corbomite-facing API

A thin shim `Corbomite::SourceEditor` lives in `src/editor/SourceEditor.{h,cpp}` (outside the library, inside the app), wrapping the forked widget with the app-facing contract:

```cpp
namespace Corbomite {

class SourceEditor : public QWidget {
    Q_OBJECT
public:
    explicit SourceEditor(QWidget *parent = nullptr);

    // Content
    void setPlainText(const QString &);
    QString toPlainText() const;

    // Cursor
    struct CursorPos { int line; int column; };
    CursorPos cursorPosition() const;
    void setCursorPosition(CursorPos);

    // Scroll — visual-line float
    float scrollPosition() const;
    void setScrollPosition(float visualLine);

    // Fold — section-level (markdown heading-driven) + ad-hoc indent fold
    QVector<int> foldedHeadings() const;   // source-line indices of folded headings
    void setFoldedHeadings(const QVector<int> &);

    // Find / replace
    bool find(const QString &, FindFlags = {});
    int replaceAll(const QString &find, const QString &replace, FindFlags = {});

    // Integration
    void setVaultResourceProvider(VaultResourceProvider *);  // for wiki-link rendering
    void setReadOnly(bool);

Q_SIGNALS:
    void textChanged();
    void cursorPositionChanged(CursorPos);
    void scrollPositionChanged(float);
    void wikiLinkActivated(const QString &target);
};

} // namespace Corbomite
```

This is exactly the shape `NoteEditorWidget`'s stacked-widget container needs in Cluster E. The forked library provides the primitives; the shim adapts to our specific API.

### What stays from upstream, what goes

| Upstream layer | Keep? | Notes |
|---|---|---|
| `src/qutepart.cpp` (main widget, QPlainTextEdit subclass) | **Keep** | Core value — 4,950 LOC of QPlainTextEdit-based editor infrastructure |
| `src/hl/` (Kate-XML syntax engine) | **Delete in Phase 4** | Replaced by KSyntaxHighlighting bridge |
| `syntax/*.xml` (385 Kate syntax files) | **Delete in Phase 4** | KSyntaxHighlighting ships these at system level |
| `themes/*.theme` (15 JSON themes) | **Delete in Phase 6** | Replaced by KColorScheme / KF6::ColorScheme |
| `src/indent/` (8 indent algorithms) | **Trim in Phase 5** | Keep: normal + markdown-aware. Delete: C-style, Python, Ruby, Lisp, Scheme, XML |
| Fold logic (currently fused with syntax engine) | **Extract in Phase 4** | Becomes a `FoldCalculator` driven by KSyntaxHighlighting token stream + markdown-heading hierarchy |
| Multi-cursor support | **Keep** | Bonus feature, zero maintenance cost |
| Bookmarks, minimap, line annotations | **Keep** (evaluate per-feature) | Decide during Phase 5 cleanup. Minimap is heavy but user-valuable. |
| Completion hook (`setCompletionCallback`) | **Keep** | Corbomite's EditorSuggestManager feeds it. |
| Find / replace engine (internal, no public API) | **Expose in Phase 3** | Build public API on top of existing engine |
| Kate XML loader (`src/hl/loader.cpp`, `regenerate-language-db.py`) | **Delete** with Phase 4 | No longer needed |

Estimated final size: ~4,000 LOC (widget) + ~800 LOC (our KSyntaxHighlighting bridge) + ~500 LOC (markdown-specific extensions) = ~5,300 LOC, down from upstream's ~12,000 LOC + 8 MB of bundled XML/themes.

### Relationship to KSyntaxHighlighting

KF6::SyntaxHighlighting is already a hard dep of the Corbomite app and of `libs/markoff/`. Having `libs/qutepart-corbomite/` also link it is natural. The bridge design:

- `QutepartKSyntaxHighlighter` : `QSyntaxHighlighter` — replaces upstream's `SyntaxHighlighter` class. Drives highlighting from a `KSyntaxHighlighting::Repository::definitionForName("Markdown")` instance. Maps `KSyntaxHighlighting::Theme` colors into Qt's highlight-block format ranges.
- `FoldCalculator` — pure fn that walks KSyntaxHighlighting's token stream + a markdown AST (from `libs/markoff-parser`) and produces `TextBlockUserData::FoldingData` per block. Decouples fold from syntax tokenization.
- `MarkdownExtensions` — recognises wiki-links `[[target]]`, tags `#tag`, frontmatter-block boundaries, which KSyntaxHighlighting's markdown grammar doesn't expose at the fidelity we need. Feeds these as additional format ranges on top of the KSyntaxHighlighting base.

### Relationship to `libs/markoff-parser`

Source-mode doesn't *need* markoff-parser for highlighting (KSyntaxHighlighting handles that). But it *does* want the AST for:

- Section-fold (heading hierarchy)
- Frontmatter boundary detection (for fold behaviour around YAML)
- Wiki-link span detection (for hover/activation)

So `libs/qutepart-corbomite/` depends on `libs/markoff-parser/` as a peer. This is consistent with `libs/markoff/` which also depends on markoff-parser.

---

## Phased shaping plan

High level. Each phase is expanded in the companion plan file `docs/superpowers/plans/2026-04-15-qutepart-corbomite-fork.md`.

| Phase | Title | Blocks | Effort |
|---|---|---|---|
| 1 | Vendor + CMake integration + smoke test | Cluster E Phase 0 | 1 session |
| 2 | Corbomite-facing API shim (`Corbomite::SourceEditor`) + visual-line float scroll adapter | Cluster E Phase 1 | 1 session |
| 3 | Public find/replace API | Cluster E find/replace polish | 0.5 session |
| 4 | Replace Kate-XML syntax engine with KSyntaxHighlighting bridge; extract FoldCalculator | Binary size reduction; unified HL across app | 2–3 sessions |
| 5 | Trim indent engines (keep normal + markdown-aware only) | Code size reduction | 0.5 session |
| 6 | Remove bundled themes; drive colors from KColorScheme | Theme unification with KDE | 0.5 session |
| 7 | Markdown-specific features: wiki-link tokens, frontmatter boundary, heading-hierarchy fold | Obsidian UX parity | 1 session |
| 8 | Rename / rebrand — final namespace + CMake target + public-header path | — | 0.5 session |

Phases 2–7 can interleave with other cluster work — they're not a single-session push. The plan file details dispatchable units for each.

---

## Licensing + provenance

**Upstream license:** MIT (© 2024 Diego Iastrubni).

**Fork license:** Inherited MIT for upstream-origin files; new Corbomite files use GPL-3.0-or-later (project default). Dual-licensed files (those we modify) retain the upstream MIT header **and** add our GPL-3.0-or-later SPDX line — GPL is the "effectively more restrictive" license, so the combined work is distributable under GPL-3.0-or-later.

**Every inherited file** gets a header block like:

```cpp
// SPDX-License-Identifier: MIT AND GPL-3.0-or-later
//
// Originally from qutepart-cpp (https://github.com/diegoiast/qutepart-cpp)
// © 2024 Diego Iastrubni, MIT-licensed.
// Fork point: commit eec2e9ae5b50b591f017296ee743ee2860a280e4, 2026-04-12.
//
// Modifications © 2026 Corbomite contributors, GPL-3.0-or-later.
```

**Bundled Kate XML files** (if any survive into Phase 4): their originating licenses (LGPL-2.0-or-later, typically) are preserved; a `PROVENANCE.md` at library root catalogues per-file origins. Phase 4's deletion of the XML engine makes this concern mostly vanish — KSyntaxHighlighting ships the XML at system level, not inside our tree.

**`PROVENANCE.md`** at library root tracks:
- Upstream commit hash at fork point
- Date of fork
- List of files inherited verbatim from upstream
- List of files we've modified (with divergence summary per file)
- List of files we've added
- List of upstream commits we've cherry-picked post-fork (if any)

---

## Out-of-scope for this spec

- The Reading-mode widget (separate library, `libs/readingview/`, separate cluster work).
- Markoff changes (none — Markoff stays as-is).
- `NoteEditorWidget` stacked-widget integration (Cluster E Phase 7).
- `EphemeralState` / `WorkspaceState` persistence shape (Cluster E Phase 1).

This spec covers only the Source-mode widget itself, its shape, and the forking/shaping plan.

---

## Risks

- **Divergence debt.** Once we fork, we lose upstream bugfixes unless we actively monitor. Mitigation: document one-screen `UPSTREAM-WATCH.md` with checklist — check upstream monthly for the first 6 months, quarterly after. After Phase 4 (KSyntaxHighlighting replacement), upstream drift is limited to the core QPlainTextEdit-subclass widget code, which changes rarely.
- **Phase 4 scope.** Extracting the fold calculator from the fused syntax engine is the largest engineering risk. Mitigation: the FoldCalculator can first run *alongside* the existing engine, produce the same output, and only then can we flip the switch to KSyntaxHighlighting-driven tokens. Parallel runs catch divergence before we delete the old engine.
- **Binary size gain not materialising.** We estimate 6–10 MB reduction; actual gain depends on how KSyntaxHighlighting's XML is already-or-not already loaded by the app. If KSyntaxHighlighting is always resident, our delete is pure win. If it's lazy-loaded, we might eagerly-load it by being an additional consumer. Measure in Phase 4.
- **Single-author bus factor was upstream's problem; now it's our fork's problem.** We accept this. Corbomite contributors maintain the fork. If no contributor emerges to maintain Source-mode specifically, the whole Source-mode feature regresses equally, same as any other internal library.

---

## Success criteria

- `libs/qutepart-corbomite/` builds standalone and as part of the Corbomite app.
- `Corbomite::SourceEditor` exposes the public API above and is used by `NoteEditorWidget` as the Source-mode widget.
- All Source-mode tests green. Round-trip of cursor/scroll/fold state through `EphemeralState` + `workspace.json` works.
- No `KSyntaxHighlighting` **and** bundled Kate-XML engine simultaneously — the fork ships with exactly one syntax engine, and it's KSyntaxHighlighting.
- Repository contents (post-Phase 6): ≤ 6,000 LOC, ≤ 1 MB of non-source-code assets.
- `PROVENANCE.md` accurately reflects fork state.
- Upstream `qutepart-cpp` remains unmodified; our fork lives only in Corbomite's tree.
