# Search domain audit

Spec: `/home/clinton/dev/Corbomite/docs/obsidian-audit/domains/search.md`
Sources audited:

- `/home/clinton/dev/Corbomite/libs/search/` — fuzzy matcher + AST + DSL planner
- `/home/clinton/dev/Corbomite/src/plugins/search/SearchView.{h,cpp}` — global search panel
- `/home/clinton/dev/Corbomite/src/dialogs/QuickSwitcher{,Model,Delegate}.{h,cpp}` — Cmd-O fuzzy file switcher
- `/home/clinton/dev/Corbomite/src/app/MainWindow.cpp:1845-1915, 2282-2299` — Cmd-P palette + Cmd-Shift-F search-panel hotkey
- `/home/clinton/dev/Corbomite/libs/storage/include/corbomite/storage/SQLiteIndex.h` and `libs/storage/src/SQLiteIndex.cpp` — FTS5 backend with `searchCompiled()` accepting the planner output
- `/home/clinton/dev/Corbomite/libs/models/{include,src}/.../SearchResultsModel.{h,cpp}` — grouped tree model for the panel
- `/home/clinton/dev/Corbomite/libs/storage/src/proxies/SearchProxy.cpp` — plugin-permission gate over `SQLiteIndex`

## Architecture fit (FTS5 + Qt vs in-memory JS scan)

Obsidian's search domain is JS-stateless: every Quick Switcher / Command Palette / SuggestModal / global-search keystroke is an O(|query|·|candidate|) sweep over an in-RAM list (commands, file paths, tags, MetadataCache entries). The matcher (`prepareQuery` → `fuzzySearch`) returns `{score, matches:[[s,e]…]}` and the panel renders highlight ranges directly. The global-search DSL (`tag:`, `path:`, `file:`, `line:`, `block:`, `section:`, `/regex/`, `"phrase"`, `-NOT`, `OR`, `match-case`, `whole-word`) lives inside the internal `global-search` plugin, which the spec explicitly notes as *out-of-tree*; the matcher itself does not implement the DSL.

Corbomite's split is reasonable: the **fuzzy matcher** is ported in-process at `libs/search/src/FuzzyMatcher.cpp`, used directly for QuickSwitcher (`src/dialogs/QuickSwitcher.cpp:27,42,58`), wiki-link suggester (`src/editor/WikiLinkSuggest.cpp:50`), tag suggester (`src/editor/TagSuggest.cpp:51`), and the editor completion popup (`src/editor/CompletionPopup.cpp:23,36,47`). The **global search panel**, on the other hand, leans on SQLite FTS5 by way of `SQLiteIndex::searchCompiled` (`libs/storage/src/SQLiteIndex.cpp:388`); the DSL is parsed at `libs/search/src/SearchDSL.cpp` and compiled to an FTS5 `MATCH` fragment plus a tag-include/exclude list. Translation is acceptable provided the DSL surface is preserved — it largely is for `path:`/`file:`/`content:`/`tag:`/`"phrase"`/`OR`/`-NOT`/`/regex/`/`match-case`, and gapped for `line:`/`block:`/`section:`/`task*`/property syntax/`whole-word`, which the parser accepts but the planner emits to a `unsupported` list (`SearchDSL.cpp:501`). The two layers do not cross-pollinate: FTS5 ranking via `bm25()` (the `rank` column at `SQLiteIndex.cpp:358-360`) drives panel ordering, while the Obsidian-port `xy` scorer drives every other surface — so panel ranking is *not* parity-equivalent with Obsidian's even when the query is operator-free.

The bigger architectural gap is the panel's lack of streaming. Obsidian's `QueryController` debounces `notifyView` at 50 ms and re-issues per keystroke; Corbomite does the equivalent via `SearchView::m_debounceTimer` at 300 ms (`src/plugins/search/SearchView.cpp:65`). A single SQL transaction returns the whole batch; there is no progressive append. For modest vaults this is fine, but it diverges from Obsidian's perceived responsiveness — the spec flags this at line 262 ("300 ms is fine for SQLite FTS but feels laggier than Obsidian's 50 ms"). There is also no `MetadataCache::indexFinished` re-execution interaction beyond a one-shot re-run (`SearchView.cpp:73-76`); if the index updates during a search session the panel does not re-stream.

## Fuzzy matcher parity

`libs/search/src/FuzzyMatcher.cpp` is a near-line port of `prepareQuery` + `Ty` + `xy` + `wy`:

- **Tokeniser:** `tokenise()` (`FuzzyMatcher.cpp:144-188`) reproduces the three character classes (`isPunctuation` covers ASCII `\!"#$%&'()*+,-./:;<=>?@[\]^_\`{|}~` plus `U+2000-206F` general-punct and `U+2E00-2E7F` supplemental-punct; `isCJKLike` covers Tibetan, Hiragana+Katakana, CJK ext A, CJK unified, CJK compat, half-width katakana). Whitespace flushes the running word; punctuation/CJK chars are flushed *and* emitted as singleton tokens. `prepared.fuzzy` is built with spaces filtered out, matching the spec's `Ey`/`Cy`/`ky` partition. **Parity confirmed.**
- **Two-pass match:** `fuzzySearch` (`FuzzyMatcher.cpp:202-236`) attempts `ty(prepared.tokens, lower, orig, strict=false)` first (word-token mode), falls through to a per-char `ty(chars, lower, orig, strict=true)` second pass exactly as the spec describes. Empty-query short-circuit returns `{0.0, {}}` (line 206-208). **Parity confirmed.**
- **`ty` matcher:** `ty()` (`FuzzyMatcher.cpp:72-102`) reproduces the cursor-advancing left-to-right `indexOf` walk. `strict` mode (char-fuzzy) prefers a word-boundary occurrence by probing forward from the first hit and only falling back if no boundary occurrence exists. Word-token mode accepts the first hit and increments `penalty` if mid-word. The boundary predicate (`isWordBoundary`, lines 52-59) covers index-0, post-whitespace, post-punct, post-CJK, and lower-to-upper camelCase — matching the JS rules. **Parity confirmed.**
- **Range merger:** `mergeAdjacent` (`FuzzyMatcher.cpp:106-122`) coalesces touching ranges so consumers can iterate with strict gaps. Matches `wy` behaviour. **Parity confirmed.**
- **Scorer `xy`:** `scoreXy` (`FuzzyMatcher.cpp:127-142`) reproduces the five-term hand-tuned formula exactly: `−max(0,gaps) − penalty/10 − span/100 − firstStart/1000 − candLen/10000`. All five denominators match the spec §2 weights. **Parity confirmed.**
- **Sort:** `sortSearchResults` (`FuzzyMatcher.cpp:238-242`) does a `std::stable_sort` descending by `score`. Spec uses `Array.prototype.sort` (not stable historically, but V8 has been stable since ES2019). Stable-sort is strictly safer. **Parity confirmed.**
- **`prepareSimpleSearch`:** Implemented at `FuzzyMatcher.cpp:197-200` with a `simple=true` flag that suppresses CJK/punctuation singleton splitting (line 168-172) and skips the second char-fuzzy pass (line 223). Matches the spec §1 `prepareSimpleSearch` semantics — whitespace-only tokenisation, all-tokens-must-appear, no acronym fallback. **Parity confirmed.**

The one substantive divergence: the spec's `xy` returns *negative* scores ("higher = closer to 0 is better"), and JS `sortSearchResults` does `t.match.score - e.match.score`, i.e. descending. Corbomite keeps the same sign convention (lines 132-141 show `r -= …` accumulating negative offsets). `FuzzyMatch::score` is therefore typically negative, and `sortSearchResults` compares with `>`. The header comment at `FuzzyMatch.h:13` says "higher = better" which is technically true (less-negative is greater) but misleading — a reader could think Corbomite's `score` is the inverted signed magnitude. Not a bug, but a documentation snag.

There is no port of `renderMatches`/`renderResults` (the highlight-span renderer). Instead, `libs/search/src/ResultHighlighter.cpp` (declaration at `ResultHighlighter.h:23`) provides `drawHighlighted(painter, x, baseline, text, matches, baseFont, normalColor, highlightColor)` — a `QPainter`-level draw that bolds + recolours match ranges. This is Corbomite-native, not a JS port, but it satisfies the same callback contract and is what `QuickSwitcherDelegate` and `SearchResultsDelegate` paint with.

## Query DSL grammar coverage matrix (operator-by-operator)

`libs/search/src/SearchDSL.cpp` parses an Obsidian-compatible DSL into `SearchNode` (a Text/Phrase/Regex/And/Or/Not/Group/OpCall AST defined at `SearchAst.h:28-52`) and compiles it to a `CompiledPlan` (FTS5 fragment + tag include/exclude + regex/case-sensitive post-filter + `unsupported` list). Operator-by-operator coverage:

| Obsidian operator | Tokenized? | AST? | Compiled? | Effective? | Site |
|---|---|---|---|---|---|
| `tag:#foo` / `tag:foo` | yes | `OpCall("tag", Text)` | yes (`requiredTags`) | **yes** | `SearchDSL.cpp:480-487` strips leading `#`, intersects on `note_tags` table at `SQLiteIndex.cpp:433-437` |
| `path:folder/sub` | yes | `OpCall("path", Text/Phrase)` | yes (`fts5Query "path:…"` qualified col) | **partial** | `SearchDSL.cpp:462-470`. FTS5 column is path-as-text; *substring* match within the path column. No prefix anchoring — `path:foo` will not exclude `folder/foo/note.md` parents-first the way Obsidian's `path.startsWith(operand)` does. |
| `file:Name` | yes | `OpCall("file", Text/Phrase)` | yes (mapped to `title:` column) | **partial** | `SearchDSL.cpp:471-479`. Maps `file:` to the FTS5 `title` column. Obsidian's `file:` matches against the file *basename + alias list*; Corbomite's `title` column doesn't include aliases (no frontmatter-aliases in the FTS schema per `SQLiteIndex.cpp` table layout). |
| `content:foo` | yes | `OpCall("content", Text/Phrase)` | yes (`fts5Query "content:…"`) | **yes** | `SearchDSL.cpp:462-470` |
| `line:(text)` | yes (parses) | yes | no — emitted to `unsupported` | **no** | `SearchDSL.cpp:499-502`. SearchView shows "(unsupported: line)" in status. |
| `block:(text)` | yes | yes | no | **no** | same. |
| `section:(text)` | yes | yes (allows self-nesting) | no | **no** | Operator table marks `section` as `allowSelf=true` (`SearchDSL.cpp:32`) but planner emits to unsupported. |
| `task:` / `task-todo:` / `task-done:` | yes | yes | no | **no** | Recognised by the operator table (`SearchDSL.cpp:33-35`) but no FTS5 mapping. |
| `["quoted phrase"]` / phrase | yes | `Phrase` (via `Tok::Quote` at `:112-132`) | yes (FTS5-quoted) | **yes** | `SearchDSL.cpp:511-512` quotes via `fts5Quote`. Note: `fts5Quote` doubles internal quotes — not standard FTS5 (which uses two double-quotes inside a quoted token). The implementation at `SearchDSL.cpp:420-425` does exactly that, so it's correct. |
| `/pattern/` regex | yes (`Tok::Regex` at `:134-159`) | `Regex` node | yes (`regexPatterns` post-filter) | **partial** | `SearchDSL.cpp:513-517`. Regex is applied as content post-filter at `SQLiteIndex.cpp:461-462`. **Critical flaw:** the planner does not contribute a positive FTS5 fragment for a bare-regex query, so `searchCompiled` short-circuits at line 397 (`fts5Query empty AND tag lists empty → return empty results`). A query that is *only* a regex (`/foo/`) compiles to an empty FTS5 string, the panel hits the `searchCompiled(query, {}, {}, regex, {})` branch (`SearchView.cpp:118-122`) but `fts5Query.isEmpty()` triggers an empty `WHERE 1=1` SELECT (`SQLiteIndex.cpp:427-431`) which scans every note and post-filters — that's actually what you want, so the early-return at `:397` is bypassed by the `postFilter` being `true`. **Wait** — re-reading: the early-return at line 397 only triggers if `fts5Query.isEmpty() && requiredTags.isEmpty() && excludedTags.isEmpty()`, ignoring `postFilter`. So `/foo/` alone does in fact return zero results. That is a bug. |
| `AND` (implicit by adjacency) | yes | `And` | yes (` AND ` join) | **yes** | `SearchDSL.cpp:529-547` |
| `OR` (uppercase keyword) | yes (`Tok::Or` at `:183`) | `Or` | yes (`( … OR … )`) | **yes** | `SearchDSL.cpp:548-556`. Uppercase-only matches Obsidian. |
| `NOT` / `-word` | yes (`Tok::Not` at `:96`) | `Not` | yes (FTS5 `NOT` infix) | **partial** | `SearchDSL.cpp:520-528, 532-536`. FTS5 `NOT` is infix (`X NOT Y`); a top-level `-foo` with no positive sibling falls into `joined.isEmpty() ? n : …` (line 542-545) which emits a bare `NOT foo` — that is *not* a legal FTS5 MATCH expression and will return the SQL error path (silently empty results in `SQLiteIndex::searchCompiled` since `q.exec()` returns false at line 455). |
| Parentheses for grouping | yes (`Tok::ParenOpen/Close`) | `Group` | yes (transparent) | **yes** | `SearchDSL.cpp:284-289, 518-519` |
| `match-case:` flag | yes | `OpCall("match-case", …)` | yes (`caseSensitiveTerms` post-filter) | **yes** | `SearchDSL.cpp:495-498`. Collects literal terms via `collectLiteralTerms`, post-filters at `SQLiteIndex.cpp:464-470`. Limitation: only literal Text/Phrase terms are collected; `match-case:/regex/` does not flag the regex for case-sensitive evaluation. |
| `ignore-case:` flag | yes | `OpCall("ignore-case", …)` | yes (no-op) | **yes** | `SearchDSL.cpp:488-490` — recurses into operand without recording case-sensitive terms. Default behaviour anyway, so this is correct. |
| `whole-word:` flag | **no** | not even in operator table | n/a | **no** | Not present in `operatorTable()` at `SearchDSL.cpp:25-39`. Parser will reject `whole-word:foo` with "Operator … not recognized" (line 322-325). Spec calls this out as missing. |
| `["property name"]:value` (frontmatter property) | partial | parser silently swallows `[…]` body via `parsePropertyStub` at `:371-384` and returns `nullptr` | no | **no** | Bracket-property syntax is intentionally stubbed to a no-op (Phase 4b deferred). Means `["status"]:done` parses without error but contributes nothing — the user gets results for everything else, which is silently misleading. No `unsupported` entry is recorded. |

Operator scope rules: the `OperatorSpec.exclusive` flag and `m_exclusiveStack` (`SearchDSL.cpp:329-345`) reproduce Obsidian's "Operator X cannot be nested within Y" rule (`section:` self-nests via `allowSelf`). The `textOnly` rule for `tag:` rejects non-text operands at parse with the canonical error message format (`SearchDSL.cpp:353-365`). Both behaviours match the Obsidian global-search parser's published quirks even though the source for that parser is out-of-tree.

The `supportedOperators()` advertised list (`SearchDSL.cpp:403-414`) is correctly conservative: only `path/file/content/tag/match-case/ignore-case` are surfaced as supported; the SearchView help popover (`SearchView.cpp:147-157`) advertises `tag:`, `path:`, `file:`, `content:`, phrase, `-`, `OR`, grouping, and labels regex/line/block/section as "coming soon".

## Search panel UX parity

The panel is a single `QLineEdit` + help button + status label + grouped `QTreeView` (`SearchView.cpp:33-77`). Compared to Obsidian's panel:

- **Filter by file extension:** **missing.** Obsidian doesn't expose this either prominently; both rely on `path:.md` workarounds.
- **Sort options (relevance / modified / name):** **missing.** Obsidian has a sort menu (relevance, file name A→Z/Z→A, modified time, created time). Corbomite is fixed to FTS5 `bm25()` rank ordering.
- **Collapsible results-per-file:** **missing.** `SearchView::executeSearch` calls `m_resultView->expandAll()` at line 136, hard-expanding every file group on every query; the user can collapse manually but loses state on the next keystroke.
- **Show count:** **yes** (`SearchView.cpp:131-134`, "%1 matches in %2 files"). Mismatched semantics, though: the FTS5 backend returns at most one row per file (one `snippet()`), so "matches" equals "files" unless `searchCompiled` produces multiple rows per file — which `SQLiteIndex` does not, since `notes_fts` has one row per note. The grouped tree view will therefore always show one match per file group. Obsidian shows multiple snippets per file (one per match site).
- **"In note title" vs "in note content" toggle:** **missing.** Obsidian has separate toggles. Corbomite users have to manually type `file:foo` or `content:foo`.
- **Streaming:** **missing.** Single SQL transaction returns the entire result. No incremental notification.
- **Source-range citations / click-to-jump-to-position:** **partial.** `onResultClicked` (`SearchView.cpp:139-143`) calls `m_workspace->openFile(path)` — opens the file but does not seek to the snippet position. Match ranges *are* preserved on the snippet (`SearchResultsModel::MatchRangesRole`, `SearchResultsModel.cpp:91-92`), and `SearchResultsDelegate` paints them, but the click handler does not translate snippet-position to file-position. Obsidian opens at the matched line.
- **Status label for unsupported operators:** **yes** (`SearchView.cpp:127-129`). Reasonable surfacing of the planner's `unsupported` list.
- **Index-finished re-run:** **partial** (`SearchView.cpp:73-76`). When the index completes, the panel re-runs the current query *once*. There is no per-update incremental refresh.

## Quick switcher parity

Cmd-O bound at `MainWindow.cpp:1215` to `MainWindow::showQuickSwitcher`. Implementation at `src/dialogs/QuickSwitcher.cpp:68-151` builds a `QuickSwitcherModel` from `Vault::getMarkdownFiles()` and wraps it in a `FuzzyFilterProxyModel` that calls `FuzzyMatcher::prepareQuery` + `fuzzySearch` per row (lines 24-66). Behaviour:

- Same fuzzy algorithm as Obsidian (parity-checked above) — **good.**
- Recent files first when filter is empty (`QuickSwitcher.cpp:48-54`) — Obsidian's behaviour is "recent + opened tabs first," same shape.
- Empty filter shows full file list — matches Obsidian invariant ("empty query matches all").
- Enter-on-no-match emits `createNoteRequested(currentFilter)` (`QuickSwitcher.cpp:172-177`) — reproduces Obsidian's "no match → create new note" affordance.
- Highlight span rendering via `QuickSwitcherDelegate::paint` (calls `FuzzyMatcher::fuzzySearch` again per row at `QuickSwitcherDelegate.cpp:38-39` — re-running the matcher in the paint path is wasteful; should cache the per-row `matches` from the proxy or model).
- **Missing:** alias-from-frontmatter matching. Header `QuickSwitcherModel.h:30` flags `// Future: add setAliases(…)`; `QuickSwitcher.h:42` echoes the same TODO. Obsidian's switcher fuzzy-matches against alias list, not just the basename. This is a real parity gap.
- **Missing:** scoring penalty for non-name (alias) hits. The Obsidian convention (`AbstractInputSuggest`: `score - 10` for display-text-only hits) is irrelevant until aliases land.
- **Missing:** type filter (Obsidian shows attachments/PDFs in the same switcher with icons). Corbomite is markdown-only.

Note: `FuzzyFilterProxyModel::lessThan` calls `FuzzyMatcher::fuzzySearch` twice *per comparison* (lines 58-59) which is O(n log n · |query|·|candidate|) on the visible row count. For large vaults this will jank. Caching the score in the model's `Qt::UserRole` would fix it.

## Command palette parity

Cmd-P bound at `MainWindow.cpp:1221` to `MainWindow::showCommandPalette`. Implementation at `MainWindow.cpp:1845-1889` instantiates `KCommandBar` and feeds it three `KCommandBar::ActionGroup` buckets from `actionCollection()->actions()` plus a `Commands` group built from `m_commandRegistry->listAvailable()`.

- **Reuses the fuzzy matcher?** **No.** `KCommandBar` is a kate-derived widget that does its own substring matching internally; it does *not* call `Corbomite::FuzzyMatcher`. This is the single biggest parity gap in this audit: any plugin that calls `prepareFuzzySearch` and observes ranking will see different results than the command palette returns.
- **Action grouping:** ad-hoc by `objectName()` prefix (`file_*` → File, `view_*` → View, else → Other). Misses many useful groupings (Edit, Format, Tab, Window). Cf. Obsidian's command palette which shows the per-command source plugin id + nice category.
- **Recent-command bias:** none. Obsidian remembers the last selection per-query and ranks it higher.
- **Search history:** none — `KCommandBar` does not persist last query.

## Implemented (parity-equivalent)

- Fuzzy matcher pipeline (prepareQuery / prepareSimpleSearch / fuzzySearch / sortSearchResults) — line-for-line port at `libs/search/src/FuzzyMatcher.cpp`.
- Two-pass strategy (word-token then strict char-fuzzy with word-boundary preference).
- Five-term `xy` scoring formula with identical denominators.
- Empty-query short-circuit returning `{0, []}`.
- CJK per-codepoint tokenisation; punctuation singletons.
- Match-range merging.
- DSL operators: `tag:` (with `#` prefix tolerated), `content:`, `path:` (column-qualified FTS5), `file:` (mapped to `title:`), `"phrase"`, parenthesised grouping, `OR`, `-`/`NOT`, `match-case:` (literals only), `ignore-case:`.
- Exclusive-nesting validation with canonical error messages.
- Tag-only-text rule.
- `SearchMatch.matches` is populated from FTS5 `snippet()` markup translated back to char-range pairs (`SQLiteIndex.cpp:323-347`) — gives the panel real highlight spans.
- Cmd-O quick switcher uses the ported fuzzy matcher.
- Cmd-Shift-F bound to global search panel via `actionCollection()` (`MainWindow.cpp:1227`).

## Partial / divergent

- **`path:`** matches as FTS5 column substring rather than path prefix; will over-match on `path:foo` against `…/foo/…`.
- **`file:`** maps to FTS5 `title` column with no alias-list join (no aliases in the schema yet).
- **`/regex/`** post-filter works *only* when there is also a positive FTS5 fragment or tag filter; a bare regex query short-circuits at `SQLiteIndex.cpp:397` (see "Notable concerns" below).
- **`-foo` at top level / single-term NOT** emits an FTS5 `NOT` without a positive operand at `SearchDSL.cpp:542-545`, which is invalid FTS5; the SQL execute fails silently and returns no results.
- **Search panel ranking** uses BM25 rather than the Obsidian fuzzy `xy` score — parity is broken for any panel-DSL-free query.
- **Snippets** are SQLite FTS5 `snippet()` output, not markdown-aware. Obsidian's snippets respect block boundaries; FTS5's are token-window-based and may straddle markdown structure.
- **One snippet per file.** FTS5 returns one row per `notes_fts` row; the panel never renders multiple match sites within the same file.
- **Click-to-jump:** opens the file but doesn't position the caret at the snippet location.
- **`["property"]:value`** silently parses to a no-op without `unsupported` notice.
- **Help popover** lists `regex, line:, block:, section: coming soon` but doesn't mention `match-case:`/`ignore-case:` (which are working) or `["property"]:` / `whole-word:` (which are not).

## Missing

- **`whole-word:`** operator — not in the operator table at all.
- **`line:` / `block:` / `section:` / `task:` / `task-todo:` / `task-done:`** — parsed and reported as `unsupported`, but no markdown-AST post-filter exists. These need a heading/section/block side-table plus a markdown-AST walker over candidate notes.
- **Frontmatter property queries** (`["property name"]:value`).
- **Streaming results** to the panel.
- **Scope filters** in the panel UX (file extension, modified date range).
- **Sort menu** (relevance / file name / modified / created).
- **Collapsible per-file groups with persisted collapse state.**
- **Multiple snippets per file.**
- **In-title vs in-content toggle.**
- **Search history persistence** (`SearchView` keeps no history; nothing in `SearchPlugin.cpp` reads/writes a last-query setting; QtKeychain / data.json not consulted for search). Obsidian remembers the last query across sessions.
- **Quick-switcher alias matching.**
- **Quick-switcher non-markdown file results.**
- **Command palette using the ported fuzzy matcher** (currently delegates to `KCommandBar`).
- **`renderMatches`/`renderResults`-equivalent shared chokepoint** for plugin highlight rendering. `ResultHighlighter::drawHighlighted` is QPainter-only and Corbomite-private; plugins that want highlight spans can't call into it. The spec calls this out as a Markoff-side gap (`docs/obsidian-audit/domains/search.md:261`).

## Notable concerns / suspected bugs

1. **Bare `/regex/` query returns no results.** `SearchView::executeSearch` (`SearchView.cpp:101-126`) parses `/foo/`, gets a `CompiledPlan` with empty `fts5Query`, empty tag lists, and one regex pattern. It hits the `postFilter` branch at line 118 and calls `searchCompiled(QString{}, {}, {}, {"foo"}, {})`. Inside `SQLiteIndex::searchCompiled` the early-return at `SQLiteIndex.cpp:397` checks only `fts5Query.isEmpty() && requiredTags.isEmpty() && excludedTags.isEmpty()` — *not* `postFilter`. So it returns empty before the `WHERE 1=1` scan can run. Fix: add `&& !postFilter` to the early-return guard, or handle the bare-regex case by setting `fts5Query` to `*` (FTS5 wildcard match). **Confirmed bug.**

2. **Top-level `-foo` produces invalid FTS5.** `emitFts5` for a bare `Not` at the And or root level emits `NOT foo` (line 522-528, 542-545). FTS5 has no prefix `NOT`; only binary `X NOT Y`. `q.exec()` will fail and `searchCompiled` returns silent empty. The `And` path correctly defers `Not` children and combines them with positive siblings — but if there is *no* positive sibling, the fallback at line 543 is broken. Fix: when `parts.isEmpty()` and `notParts` is non-empty, either rewrite as `* NOT joined-not-parts` (FTS5 supports `*` to match anything but only on a column qualifier), or surface as `unsupported`. **Confirmed bug.**

3. **`["property"]:value` silently parses to nothing.** `parsePropertyStub` consumes the bracket pair and returns `nullptr` without recording an `unsupported` entry (`SearchDSL.cpp:371-384`). Any user typing this will get unrelated results without any indication that the bracket clause was ignored. The `match-case:` of an explicit literal at least makes it into `caseSensitiveTerms`; `["foo"]:bar` is just dropped on the floor. Fix: append `[…]` to `plan.unsupported` from the planner side (the parser doesn't know about the plan; needs a marker node).

4. **`FuzzyMatch::score` documentation is misleading.** Header comment says "higher = better" (`FuzzyMatch.h:13`) but actual scores are negative. A naive plugin author logging "score: 0.0" might think they have a perfect match when in fact 0.0 is reserved for the empty-query short-circuit and any real match is ≤ −length/10000. Tighten the comment.

5. **`FuzzyFilterProxyModel::lessThan` calls `fuzzySearch` per pairwise comparison** (`QuickSwitcher.cpp:58-59`). For a 10k-file vault this is O(n log n · |query|·|name|) on every keystroke. Cache scores in a model role.

6. **`QuickSwitcherDelegate::paint` re-runs `FuzzyMatcher::fuzzySearch` inside the paint loop** (`QuickSwitcherDelegate.cpp:38-39`). Same caching argument — paint is on the hot scroll path.

7. **`tag:` operand allows the leading `#` but does not strip it on the AST side.** `SearchDSL.cpp:482` does `if (tag.startsWith('#')) tag.remove(0, 1);` *during compilation* but the AST `Text` node retains the `#`. If anything walks the AST and reads `text` (e.g., for syntax highlighting in the input field, or a future test asserting node content) it will see the `#`. Cosmetic — only matters if AST is ever exposed.

8. **`SearchProxy` permission gate** (`SearchProxy.cpp:33`) returns empty on denied with a `qCDebug` log. No `Q_EMIT` of an error signal — plugins that get permission revoked mid-session see "no results" with no indication. Minor UX cost.

9. **Streaming/incremental gap:** in a vault of any size, the first keystroke triggers the 300 ms debounce, then a synchronous SQL query, then `expandAll()`. If the user keeps typing during the SQL query, the second debounce restarts but the in-flight query continues — potentially returning stale results that overwrite the newer query. There is no query-id / generation guard. Confirmed by code: `SearchView::executeSearch` does not capture a sequence number.

10. **`SearchView` does not persist last query.** No `KConfig` read/write of the search input. Reopening the panel starts blank. Obsidian restores the last query on workspace reload.

11. **`match-case` does not propagate into FTS5.** The FTS5 default tokeniser case-folds, so `match-case:Foo` returns the same candidate set as `Foo` and only filters at the post-filter stage. This is the right strategy (FTS5's `unicode61` tokeniser doesn't easily flip per-query) but the candidate-overfetch (`fetchLimit = max(maxResults*4, 100)` at `SQLiteIndex.cpp:449`) may still drop case-sensitive hits for very common case-folded base terms. No bug per se; a perf cliff.

12. **`SearchResultsModel` group-row `Qt::DisplayRole` includes the match count in the same string** (`%1 (%2)` at `SearchResultsModel.cpp:69-70`). The delegate then renders it as a single string; a custom `MatchCountRole` exists but the default tree painter ignores it. Minor presentation issue; unrelated to parity.
