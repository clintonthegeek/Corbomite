# Cluster D — Search / suggester parity

> **Living-status note:** This file is the *plan*. Live status (Not started / In progress / Done / Blocked) is in [`docs/PROJECT-STATE.md`](../../PROJECT-STATE.md) Roadmap. Update PROJECT-STATE per the rituals in [`docs/CONTRIBUTING-OPS.md`](../../CONTRIBUTING-OPS.md), not this file. Edit this file only when the plan itself changes (work breakdown, target classes, references).

**Plan written:** 2026-04-14. Derived from `docs/obsidian-audit/GAP-ANALYSIS.md` §Cluster D.

**Covers:** P0.4 (`SearchMatch` lacks byte-range array — highlight spans cannot render), P2.19 (search DSL parser — `tag:`, `path:`, `file:`, quoted, AND/OR/NOT), P2.20 (shared fuzzy matcher across Quick Switcher / Command Palette / every Suggester), P2.21 (highlight-span rendering).

## Goal

Make Corbomite's search and suggester stack behave like Obsidian's — same fuzzy-ranking feel (so users keep muscle-memory), same search-panel DSL, and same **highlight-span rendering** so result snippets underline the matched portions. The current `libs/storage/include/corbomite/storage/SQLiteIndex.h`'s `struct SearchMatch` lacks the `matches: [[start,end]...]` field entirely, which means **highlight spans literally cannot be drawn today** — the panel can find results but can't show *where* in the line the hit occurred.

Cluster because every suggester in Obsidian shares the same matcher (QuickSwitcher, CommandPalette, FuzzySuggestModal, AbstractInputSuggest, PopoverSuggest, SuggestModal, SecretComponent, SettingTab, Notice, ViewRegistry footnote suggester — `search.md §15`). Divergence here leaks everywhere.

## Audit references

- **Fuzzy matcher algorithm:** `domains/search.md §1-§2` + `§8` — two-pass (word-token then char-fuzzy with CJK-codepoint split). Scoring at `_internal.js:83133-83143`: `contiguity-run-count / case-penalty/10 / span-cost/100 / start-bias/1000 / length-bias/10000`.
- **`PreparedQuery` / `FuzzyMatch` shapes:** `domains/search.md §2` — `{query, tokens, fuzzy}` and `{score, matches: [[start, end]...]}` respectively. Matches are merge-sorted non-overlapping ranges.
- **Public matcher helpers:** `prepareQuery`, `fuzzySearch`, `prepareFuzzySearch`, `prepareSimpleSearch`, `sortSearchResults` — all in `search/` per `domains/search.md §1`.
- **Search DSL actually lives outside the audited tree:** `domains/search.md` (Pass 1 correction) — `QueryController` is Bases, not search-panel. The real parser is in the internal `global-search` plugin inside `_internal.js`. **Extraction is a controller-side follow-up** noted in `Pass 3 report`. Grammar specs must be sourced from Obsidian user docs (`help.obsidian.md/plugins/search`) and cross-referenced with `openGlobalSearch("tag:" + ...)` call sites.
- **Highlight-span renderer:** `domains/rendering.md §1` — `rendering/renderResults.js` takes match offsets + source text, produces DOM with `<span class="search-result-file-matched-text">` wraps. Uses the same `{matches: [[start,end]...]}` shape.
- **Corbomite gap cite:** `domains/search.md §11` — `libs/storage/include/corbomite/storage/SQLiteIndex.h:14-18` `struct SearchMatch` missing `matches` field; `src/sidebar/SearchPanel.cpp:71` has the `// TODO: Support Obsidian search operators` that's "larger than it looks".

## Target classes

| Class | File | Notes |
|---|---|---|
| `Corbomite::PreparedQuery` | `libs/search/src/PreparedQuery.{h,cpp}` | Struct `{query, tokens, fuzzy}`. Built via factory `prepareQuery(QString)` |
| `Corbomite::FuzzyMatch` | `libs/search/src/FuzzyMatch.h` | Struct `{double score; QVector<QPair<int,int>> matches;}`. Merge-sorted non-overlap invariant |
| `Corbomite::FuzzyMatcher` | `libs/search/src/FuzzyMatcher.{h,cpp}` | `fuzzySearch(PreparedQuery, QString haystack) → optional<FuzzyMatch>`. Two-pass algorithm |
| `Corbomite::SearchMatch` (refactor) | `libs/storage/include/corbomite/storage/SQLiteIndex.h` | Add `QVector<QPair<int,int>> matches` field |
| `Corbomite::SearchDSL` | `libs/search/src/SearchDSL.{h,cpp}` | Parse `tag:#foo`, `path:dir/`, `file:name`, `line:`, `block:`, `section:`, quoted phrases, exclusion (`-word`), AND/OR/NOT |
| `Corbomite::SearchPlan` | `libs/search/src/SearchPlan.{h,cpp}` | Output of DSL parse; drives SQLiteIndex FTS5 MATCH construction + in-memory fuzzy post-ranking |
| `Corbomite::ResultHighlighter` | `libs/search/src/ResultHighlighter.{h,cpp}` | Produces `QTextDocument` fragment with highlight spans from `{text, matches}` |
| New lib | `libs/search/` | New library — search is a concern separate from storage (currently lives inside `libs/storage`) |

Refactor `src/sidebar/SearchPanel.{h,cpp}` to consume `libs/search/` and render highlight spans.

## KDE / GPL3-compatible prior art

**Local KDE source convention:** the KDE source tree is checked out locally at `~/src/kde/src/<repo>`. **Always grep there first; never clone from `invent.kde.org` unless a repo is genuinely missing locally.** Verified-present locally: `kate`, `kdevelop`, `kio`, `kconfig`, `kconfigwidgets`, `kparts`, `kxmlgui`, `kwidgetsaddons`, `ktexteditor`, `krunner`, `baloo`, `okular`, `poppler`, `qtkeychain`, `sonnet`.

| Target | Local path | What we're looking for |
|---|---|---|
| Fuzzy scoring | external — `fzf` source (MIT, reference only — not directly linkable without translation) | Scoring algorithm with contiguity/start/length biases |
| KDE command-palette ranking | `~/src/kde/src/krunner/` (KRunner engine), `~/src/kde/src/kwidgetsaddons/` (KCommandBar in-use ranking) | Existing fuzzy-rank code we may already partially leverage |
| Search DSL parser | `~/src/kde/src/baloo/src/lib/term.cpp` (`Baloo::Term` parser) | Structured query parsing patterns |
| Highlight span rendering | `~/src/kde/src/ktexteditor/` (`KTextEditor::Range` + KateSearch highlight bar) | Concrete QTextDocument span highlighting |
| Match-range data structures | `~/src/kde/src/sonnet/` (KDE spellcheck — match-range lists similarly used) | Validation for QVector<QPair<int,int>> choice |

## Work breakdown

**Phase 1 — `libs/search/` scaffold + SearchMatch refactor:**
1. Create `libs/search/` CMake library. Stub `FuzzyMatcher`, `PreparedQuery`, `FuzzyMatch`.
2. Add `matches: QVector<QPair<int,int>>` to `SearchMatch` struct in `libs/storage/include/corbomite/storage/SQLiteIndex.h`. Propagate through `SQLiteIndex::search` return path; default to empty vector for now.
3. Verify `src/sidebar/SearchPanel.cpp` compiles with empty matches and renders results (no highlights yet) — regression test.

**Phase 2 — FuzzyMatcher:**
4. Implement `prepareQuery(QString) → PreparedQuery` — tokenise on whitespace + punctuation, preserving CJK-codepoint splits per `search.md §1`.
5. Implement `fuzzySearch(PreparedQuery, haystack) → optional<FuzzyMatch>` — two-pass:
   - Pass 1: word-token match — tokens must all appear in haystack (order-free)
   - Pass 2: char-fuzzy per-token — for each token's position, find best-scoring character sequence using the 5-term formula
6. Implement `prepareSimpleSearch` (non-fuzzy, literal substring match) for command palette use.
7. Implement `sortSearchResults(QVector<FuzzyMatch>)` — stable descending-score sort.
8. Port exact scoring formula from `search.md §2` + `_internal.js:83133-83143` citations. Write unit tests with characteristic inputs (e.g. "fs" matching "FileSystemAdapter" at high score due to start-bias + acronym).

**Phase 3 — Shared suggester consumption:**
9. Refactor `CompletionPopup` (`src/editor/CompletionPopup.{h,cpp}`) + `QuickSwitcher` + `KCommandBar` palette backing to use `FuzzyMatcher` and `ResultHighlighter`.
10. Every suggester exposes `FuzzyMatch.matches` to its delegate for highlight rendering.
11. `ResultHighlighter::toDocument(QString text, QVector<QPair<int,int>> matches) → QTextDocument*` with bold-span formatting applied to match ranges. Reusable across all UI.

**Phase 4 — Search DSL:**
12. Research phase: grep `_internal.js` for `openGlobalSearch("tag:"` and `openGlobalSearch("path:"` call sites; cross-reference with `help.obsidian.md/plugins/search` documented grammar. Produce `docs/search-dsl-spec.md` before implementation.
13. Hand-write a recursive-descent parser for the grammar in `SearchDSL.parse(QString) → optional<SearchPlan>`.
14. Implement `SearchPlan::toFTS5(vault) → QString` — translate compatible predicates to SQLite FTS5 MATCH syntax; run unmatchable predicates (regex, fuzzy) as post-filter over FTS5 result set.
15. Wire into `SearchPanel` — replace current query-as-literal path with DSL-parsed path; old behaviour falls through as one-token literal search when parser returns no structural operators.

**Phase 5 — End-to-end:**
16. Integration test: open a vault with notes tagged `#project`, search `tag:#project foo`, assert results filtered by tag AND fuzzy-ranked by `foo`. Verify highlight spans render on `foo`.
17. UX regression: quick switcher still opens on `Ctrl+O`, command palette on `Ctrl+P`, both use the new matcher.

## Explore-agent dispatch prompts

**Prompt 1 — DSL grammar extraction:**
> Reverse-engineer Obsidian's search DSL grammar. Read Obsidian's published help at `help.obsidian.md/plugins/search` (use WebFetch on the root + "Search operators" page; do not re-fetch the whole doc tree). Cross-reference with every `openGlobalSearch(` call site in `/home/clinton/bin/ObsidianRAW/audit/renamed/obsidian/tree/_internal.js` and `obsidian/*.js` to find the literal DSL strings Obsidian emits itself. Produce `docs/search-dsl-spec.md` with: (a) full grammar in EBNF, (b) examples of every operator, (c) how AND/OR/NOT/grouping/quoting/negation compose. Under 1500 words. This doc blocks Phase 4 of Cluster D.

**Prompt 2 — FTS5 query translation feasibility:**
> Given the search DSL spec (when available) or the documented subset (`tag:`, `path:`, `file:`, quoted phrase, `-exclusion`, AND/OR), evaluate which predicates translate cleanly to SQLite FTS5 MATCH syntax and which require post-filtering. Read `libs/storage/src/SQLiteIndex.cpp` for the existing FTS5 table schema. Report: (a) schema changes needed (e.g. dedicated `tags` column if not already), (b) FTS5-native query shape for each DSL operator, (c) recommended post-filter implementation for non-FTS5 predicates (regex, fuzzy). Under 700 words.

**Prompt 3 — fzf algorithm cross-check:**
> Read `fzf`'s `src/algo/algo.go` (upstream github.com/junegunn/fzf). Compare its scoring model with the 5-term formula documented in `docs/obsidian-audit/domains/search.md §2`. Report: (a) which terms are shared, (b) which differ, (c) whether fzf's implementation is a sound port target or whether Obsidian's specifics require a hand-write. Under 600 words.

## Definition of done

- `libs/search/` library built and integrated; `FuzzyMatcher` passes unit tests with characteristic inputs from `search.md §2` examples.
- `SearchMatch` struct includes `matches: QVector<QPair<int,int>>`; all existing call sites updated.
- `SearchPanel` renders highlight spans on result snippets (P0.4 closed).
- Quick switcher, command palette, CompletionPopup all use the shared matcher.
- Search DSL parser handles `tag:`, `path:`, `file:`, `-word`, `"quoted"`, and AND/OR; more operators tracked as follow-ups.
- Benchmark: fuzzy match of 10k filenames under 100ms wall-time on single core.

## Blocks / enables

- **Depends on:** (weakly) Cluster A for `LinkUtils::stripHeading` normalisation on heading-match search.
- **Blocks:** Cluster H (hover-link preview often triggered from search results), Cluster I (fuzzy match used to rank MetadataCache resolveLink ambiguities).
- **Enables:** every suggester in the app; Obsidian-muscle-memory search feel.
- **Estimated effort:** 2–3 weeks, parallel-safe with Clusters A and B. Phase 4 (DSL) is gated on Prompt 1's explore output.

## Preserved Obsidian compat quirks

- Matcher always returns `matches` merge-sorted non-overlapping — even when the raw positions overlapped, they're coalesced before return.
- Empty query returns empty match list (not "all match").
- `FuzzyMatch.score` higher = better; callers sort descending.
- Score is unbounded above (term-bias dominates for short strings).
