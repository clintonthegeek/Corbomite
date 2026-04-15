# `obsidian/search` — fuzzy matcher, simple matcher, query-controller

**Source:** `/home/clinton/bin/ObsidianRAW/audit/renamed/obsidian/tree/obsidian/search/`
**File count:** 6
**Files:** `fuzzySearch.js`, `prepareFuzzySearch.js`, `prepareQuery.js`, `prepareSimpleSearch.js`, `QueryController.js`, `sortSearchResults.js`
**Pass 1 summary (verbatim from `00-taxonomy.md`):**
> Two layers. The lower (`fuzzySearch`/`prepareQuery`/`prepareFuzzySearch`/`prepareSimpleSearch`/`sortSearchResults`) is a generic fuzzy-match engine plugins reuse for any list-of-strings (commands, files, tags). The upper `QueryController` is the global-search engine: parses Obsidian's search-DSL (`tag:`, `path:`, `file:`, `line:`, `block:`, `section:`, regex, quoted-phrase, AND/OR/NOT, `match-case`/`whole-word` flags), runs it against vault content + `MetadataCache`, and streams `BasesQueryResult`-shaped results to the search panel.

**De-minifier artifact note:** md5sum is unique across all six files; no duplicate-extraction candidates. Five files are tiny (236–724 bytes, 5–23 lines) and self-contained. `QueryController.js` is the outlier at 31725 bytes / 819 lines, declared source `app.js:145043-145855`; its code is Bases-oriented (uses `BasesEntry`, `BasesQueryResult`, `FY`, `nY`, `iY`, `aq`) and the wrapping directory name "search" is misleading. **The Pass 1 taxonomy's claim that `QueryController` parses the search-DSL is not borne out by the source.** `QueryController` is the Bases view controller; its text-search method (`applySearchQuery`) calls `prepareQuery` only to tokenise a free-text "filter this Bases result set" string — it does NOT implement the search-panel DSL. The real DSL lives in the internal `global-search` plugin (not in `obsidian/`), reachable via `internalPlugins.getPluginById("global-search").openGlobalSearch(queryString)` — 21 call-sites in `_internal.js` use `"tag:"+name`. Audit is therefore constrained to what is actually in these six files; the DSL is logged in OPEN QUESTIONS as out-of-tree. STRUCTURE.md lists `obsidian/search` twice (lines 559 and 624) with identical contents; treated as one entry per assignment.

---

## 1. Public API surface

All six files carry explicit `// public API symbol:` banners. Five are plugin-reachable; `QueryController` is not on the public API list but is the Bases-internal orchestrator.

### `prepareQuery`

- **Kind:** function
- **Exported as:** `prepareQuery` (`search/prepareQuery.js:2`)
- **Signature:** `prepareQuery(e: string): { query: string; tokens: string[]; fuzzy: string[] }`
- **Purpose:** Tokenise the raw query into three forms. Walks the lowercased string once, dispatching on character class (regexes at `_internal.js:83109-83113`, declared `app.js:62778-62782`):
  - `Cy = /\s/` — whitespace flushes the current word token and skips.
  - `ky = /[\u2000-\u206F\u2E00-\u2E7F\\'!"#$%&()*+,\-.\/:;<=>?@\[\]^_\`{|}~]/` — ASCII + general-punctuation; each char is its own singleton token.
  - `Ey = /[\u0F00-\u0FFF\u3040-\u30ff\u3400-\u4dbf\u4e00-\u9fff\uf900-\ufaff\uff66-\uff9f]/` — Tibetan + Japanese + CJK Hanzi + half-width katakana; each codepoint is its own token (per-codepoint CJK match without a dictionary).
- **Return shape:**
  - `query` — original (case-preserved) user string.
  - `tokens` — lowercase word-tokens plus singleton punctuation/CJK chars, in source order.
  - `fuzzy` — the lowercased full string as a `char[]` with **spaces removed**. Used as the per-character fallback when word-token matching fails.
- **Purity:** deterministic, no I/O.

### `fuzzySearch`

- **Kind:** function
- **Exported as:** `fuzzySearch` (`search/fuzzySearch.js:2`)
- **Signature:** `fuzzySearch(e: PreparedQuery, t: string): { score: number; matches: [number, number][] } | null`
- **Purpose:** Score one candidate against a `prepared` query. Two-tier (`search/fuzzySearch.js:5-13`):
  1. Word-token match via `Ty(e.tokens, e.query, t, false)`.
  2. On `null`, char-fuzzy fallback via `Ty(e.fuzzy, e.query, t, true)`.
  The `true` flag switches `Ty` into **strict-adjacent** char-fuzzy mode — a char at non-consecutive position is re-sought later. Enables typing `mdf` to fuzzy-match inside `getMarkdownFiles`; typing `gms` hits `g`+`M`+`s` as three runs via the word-token path.
- **Empty query shortcut:** `e.query === ""` → `{score: 0, matches: []}`, every candidate matches. Load-bearing for "show everything on fresh popup open" (`FuzzySuggestModal.getSuggestions`, `ui/popups/FuzzySuggestModal.js:351-370`).

### `prepareFuzzySearch`

- **Kind:** function
- **Exported as:** `prepareFuzzySearch` (`search/prepareFuzzySearch.js:2`)
- **Signature:** `prepareFuzzySearch(e: string): (text: string) => { score, matches } | null`
- **Purpose:** Curry helper. Calls `prepareQuery(e)` once and returns a closure that calls `fuzzySearch(prepared, text)`. Avoids re-tokenising when scoring N candidates against one query. Exported for plugin use via the `obsidian` module.

### `prepareSimpleSearch`

- **Kind:** function
- **Exported as:** `prepareSimpleSearch` (`search/prepareSimpleSearch.js:2`)
- **Signature:** `prepareSimpleSearch(e: string): (text: string) => { score, matches } | null`
- **Purpose:** Substring-only variant for autocompleters. Tokenises on whitespace only (`e.toLowerCase().split(" ")`). Closure delegates to internal `Iy(tokens, query, candidate)` which runs `Ly` (all-tokens-must-appear substring finder, `_internal.js:83199-83212`) and scores via `xy`. No CJK codepoint split, no acronym fallback. Returns `null` if any token is absent.

### `sortSearchResults`

- **Kind:** function
- **Exported as:** `sortSearchResults` (`search/sortSearchResults.js:2`)
- **Signature:** `sortSearchResults(e: Array<{ match: { score: number } }>): void` (in-place)
- **Purpose:** Sort by `match.score` descending: `(e, t) => t.match.score - e.match.score`. Requires each element to be shaped `{match: {score}, ...}` (the `FuzzySuggestModal` record shape).

### `QueryController`

- **Kind:** class extending `Component` (`search/QueryController.js:23`, `:701`)
- **Exported as:** `QueryController` (Bases-view orchestrator — consumed by `bases/BasesView.js`, `bases/BasesEntryGroup.js`, `bases/BasesQueryResult.js`)
- **Constructor:** `new QueryController(app: App, plugin: BasesPluginInstance, containerEl: HTMLElement, currentFile: TFile | null = null)` (`search/QueryController.js:24-210`)
- **Purpose:** Orchestrates one Bases-query lifecycle: DOM chrome (toolbar menus — view/results/sort/filter/property/search/newItem), query execution against the Bases entry queue (`aq(app, this)` at `:203`), debounced view notification, drag-drop handling, `QueryController.applySearchQuery(results, order)` for free-text filtering of already-resolved results.
- **Key methods (grouped):**
  - *Query lifecycle:* `setQuery` / `setQueryAndView` / `update` / `runQuery` / `buildBasesContext` / `clear` / `onConfigChanged` / `updateCurrentFile`.
  - *Result management:* `addResult` / `removeResult` / `evaluateRelevantProperties` / `getProperties`.
  - *Text-filter over results:* `updateSearchQuery` / `getSearchQuery` / `applySearchQuery(entries, orderList): BasesEntry[]` — **the only site in `obsidian/search/` that actually uses `prepareQuery`** (`search/QueryController.js:678`). Runs `prepareQuery(this.searchQuery).tokens` and filters each entry by requiring every remaining token to appear as a substring of any field's string value across the ordered column list. Tokens are removed from the pending set as they match; entry passes iff the set empties. **No fuzzy, no scoring** — substring AND, short-circuit.
  - *View/chrome:* `selectView` / `promptForAddView` / `getViewConfig` / `notifyView` (debounced 50 ms) / `startLoader`/`stopLoader` / `displayError`/`clearError`.
  - *State:* `setEphemeralState` / `getEphemeralState` / `onResize` / `getCurrentFile` / `getWidgetForIdent` / `getEditorLanguageSupport`.
  - *Event plumbing:* `onload` subscribes to `metadataTypeManager.on("changed")` and `vault.on("config-changed")`.
- **Lifecycle:** Instantiated by `bases/BasesView` and by the `E$` embed class at the bottom of this file (`search/QueryController.js:702-817`, wrapping a `QueryController` inside an `interactive-child` div for `![[something.base]]` embeds). Owned by the parent `BasesView`/embed; `clear()` stops the child `queue: aq`. `requestNotifyView = debounce(notifyView, 50)`.
- **Mixes in:** `Component` (child registration + onload/unload). Instance field `events: new k$()` where `k$ extends Events` (`:6-22`) — exposes `controller.events.on("view-changed", cb)`; only event is `"view-changed"` (`:270,:285,:354,:385,:428`).

Plugin-reachable public API count: **5** (`prepareQuery`, `prepareFuzzySearch`, `fuzzySearch`, `prepareSimpleSearch`, `sortSearchResults`). `QueryController` is not documented in the official plugin API but is importable via the `obsidian` module; Bases-plugin callers treat it as internal.

---

## 2. Data structures

### `PreparedQuery`

```typescript
interface PreparedQuery {
  query: string;    // original, case preserved — kept so renderResults can display the user's exact text
  tokens: string[]; // lowercase whitespace-split words PLUS 1-char punctuation/CJK tokens
  fuzzy: string[];  // lowercase char-array, spaces filtered out — "no space fallback alphabet"
}
```

**Invariants:** `tokens.length` is **not** a word count — a hyphenated query like `foo-bar` yields `["foo", "-", "bar"]` (three tokens). `fuzzy.every(ch => ch !== " ") === true`.

### `FuzzyMatch`

```typescript
interface FuzzyMatch {
  score: number;          // typically negative; higher (closer to 0) is better
  matches: [number, number][]; // half-open [start, end) byte offsets into the CANDIDATE text
}
```

Emitted by `fuzzySearch`, `prepareFuzzySearch(q)(t)`, and `prepareSimpleSearch(q)(t)`. A `null` return means "no match".

**Offset contract:** offsets are UTF-16 code-unit indices (JS string length, not byte or grapheme). `matches` is already **merge-sorted and non-overlapping** — consecutive matches that touch (`a[1] === b[0]`) are merged into one range by `Ty` at `_internal.js:83175-83178`. Callers can assume `for (let i=0; i<matches.length-1; i++) matches[i][1] < matches[i+1][0]` strictly.

### Suggestion-item shape (consumer-owned)

Every consumer wraps fuzzy output into a record with the `match` field nested. Example from `FuzzySuggestModal`:

```typescript
interface SuggestionRecord<T> {
  item: T;           // the underlying domain object (TFile, Command, TagInfo, …)
  match: FuzzyMatch; // required by sortSearchResults
  // Plugin-specific extras allowed; sortSearchResults reads ONLY .match.score
}
```

`AbstractInputSuggest.getSuggestions` (`ui/popups/AbstractInputSuggest.js:369-398`) adds a score-penalty trick: when the primary `o.value` text misses but the auxiliary `o.display` text hits, the hit is recorded with `score: a.score - 10` (i.e. ranked below any value-hit). This is a convention plugins can copy.

### Scoring formula (`xy`, `_internal.js:83133-83143`)

```typescript
function xy(matches: [number, number][], queryLen: number, candidateLen: number, penalty: number): number {
  if (matches.length === 0) return 0;
  let r = 0;
  r -= Math.max(0, matches.length - 1);        // 1 point per GAP between runs
  r -= penalty / 10;                            // caller-supplied penalty (case-mismatch count from Ty)
  const first = matches[0][0];
  r -= (matches[matches.length-1][1] - first + 1 - queryLen) / 100; // span-cost
  r -= first / 1e3;                             // prefer earlier first-match
  r -= candidateLen / 1e4;                      // tie-break toward shorter candidates
  return r;                                     // <= 0 for any real match
}
```

Interpretation (weights descending):
- **Contiguity** (−1 per extra run). Dominates: a 3-run match scores −2 before any other penalty.
- **Case-fidelity penalty** `/10`. `Ty` increments `o` per character that matched without landing on a camelCase or word-boundary.
- **Span cost** `/100` — stretched-across-candidate penalty.
- **Start bias** `/1000` — rewards matches starting near position 0.
- **Length bias** `/10000` — tie-breaks toward shorter candidates.

Not Jaro-Winkler, Smith-Waterman, or BM25 — a hand-tuned run-count + span + position scorer. Corbomite's C++ port must reproduce *ordering* (comparator result) not absolute values.

### `PopoverSuggestItem<T>` (from `AbstractInputSuggest`)

```typescript
interface PopoverSuggestItem<T> {
  value: string;     // primary text (scored first)
  display?: string;  // fallback text (scored with -10 penalty if value misses)
  matches?: [number, number][];  // added by getSuggestions
  score?: number;                 // added by getSuggestions
}
```

---

## 3. On-disk contracts

**No on-disk contracts.** None of the six files in this domain read or write the vault, `.obsidian/*`, IndexedDB, or LocalStorage. `prepareQuery` et al. are pure functions; `QueryController` reads its inputs via `app.vault`, `app.metadataCache`, and `app.fileManager.processFrontMatter` but writes nothing directly to disk — the on-disk contract for a persisted Bases query is owned by `bases/` (the `.base` file schema), and the `obsidian/search/` directory borrows `QueryController` only for runtime orchestration.

**`QueryController` config reads:** `app.vault.on("config-changed", "userIgnoreFilters", …)` (`search/QueryController.js:228-230`) — a change to the user-ignore patterns invalidates the cached `queryState` and forces `update()`. The actual config key schema and default are documented in `domains/vault.md` §3.

---

## 4. Events emitted

### `QueryController.events` (the nested `k$` extending `Events`, `search/QueryController.js:6-22, :35`)

| Event name | Payload (inferred) | Triggered when | Typical consumers |
|---|---|---|---|
| `view-changed` | `()` — no-arg | Any of (a) `selectView(newName)` (`:270`), (b) `setQueryAndView(q, viewName)` when `oldName !== newName` (`:285`), (c) `update()` path that resolves a `null → actual` view name (`:354`), (d) `update()` path that swaps the `BasesView` instance for a different type (`:385`), (e) `clear()` (`:428`). | `BasesView`'s toolbar chrome — the view-picker menu, sort-menu, filter-menu re-render on this event. |

No other events emitted from this domain. The five pure functions do not emit.

**Note on `requestNotifyView`:** `notifyView` is NOT an event — it is a direct method call into `this.view.onDataUpdated()` and into each toolbar menu's `updateQuery()`. The 50 ms `debounce(notifyView, 50)` is a **rate-limit for UI refresh**, not an observer pattern. Plugin authors can't subscribe to it; a Bases plugin view gets `onDataUpdated` invoked on it directly.

---

## 5. Events consumed

| Listener file | Subscribes to | Why |
|---|---|---|
| `search/QueryController.js:214-218` | `app.metadataTypeManager.on("changed")` | Frontmatter property-widget types changed → may affect column widget choice → debounced re-notify. |
| `search/QueryController.js:220-225` | `app.vault.on("config-changed", onConfigChanged, this)` | `userIgnoreFilters` changed → invalidate `queryState`, re-run. Other keys are ignored (`:228-230` filters to `"userIgnoreFilters"` only). |
| `search/QueryController.js` (embed wrapper `E$`, `:744-770`) | `app.vault.on("modify", file)` | Guarded on `file === this.file`; reloads `.base` content and calls `controller.setQuery(newQuery)`. Backs the `![[something.base]]` embed use-case. |

No Workspace events, no MetadataCache `changed`/`resolved`/`finished` subscriptions. The domain is deliberately decoupled from MetadataCache — Bases uses `BasesEntry` which resolves frontmatter on demand via `metadataTypeManager`, not by listening to cache events.

---

## 6. Commands registered

**No commands registered here.** None of the six files calls `app.commands.addCommand`. The command-palette integration for the search-panel, quick-switcher, and tag-search is implemented by the internal `global-search`, `switcher`, and `command-palette` plugins (out of audit scope), which merely *use* this domain's fuzzy matcher.

---

## 7. Registries owned

**No registries.** `QueryController` reads registrations owned by other domains (`plugin.getRegistration(r.type)` → Bases-plugin's view-type registry, documented in `domains/bases.md` §10) but does not populate any registry of its own.

---

## 8. Invariants

- **`fuzzySearch(prepared, candidate)` returns non-null iff every token in `prepared.tokens` appears in order within `candidate`.** `Ty` tracks `a = lastMatchEnd` and each `r.indexOf(c, a)` must succeed. Tokens `["foo","bar"]` do **not** match `"barxxfoo"` — left-to-right progression.
- **Empty query (`""`) always matches** with `{score: 0, matches: []}`. Used by initial popup open to show full list.
- **`matches` is merge-sorted and non-overlapping** — `renderResults` iterates linearly.
- **`prepareQuery` is idempotent on whitespace.** Trailing/leading/repeated spaces produce empty segments filtered by the `i !== r` guard.
- **Tokens are lowercased; `.query` retains original case.** Score depends on case-fidelity in `Ty` via the `o` counter (see §2).
- **CJK matched codepoint-by-codepoint.** `Ey` splits each CJK char into its own token; "日本" is a two-token query, matched with contiguity bonus if adjacent. No dictionary/stemming/romaji.
- **Punctuation is a real token.** `my-file.md` tokenises as `["my","-","file",".","md"]`; each punctuation char must match the candidate. Means: no wildcard `*`; typing `file:` is an ordinary literal constraint.
- **Word-boundary bonus is not free** (`Ty`, `_internal.js:83158-83173`). Character landing mid-word (not at position 0, not after `ky`/`Ey` boundary) increments the case-mismatch penalty `o` — unless the transition is lowercase→uppercase (camelCase) or post-boundary. In char-fuzzy mode (`i` flag true), a mid-word mismatch forces `continue`/retry instead of penalty.
- **`QueryController.applySearchQuery` ignores any DSL** — pure substring AND across tokens and ordered columns. The Pass 1 "DSL" parsing is **not** here.
- **`QueryController` idempotent update.** `update()` short-circuits when `JSON.stringify({filter, formulas})` is unchanged (`:402-408`) — only calls `requestNotifyView()`. Corbomite Bases-equivalent must preserve this.
- **`notifyView` suppressed during `initialScan`** (`:462-463`). Corbomite's FTS-driven panel has no equivalent phase.

---

## 9. Observable user features

- The user types into a `FuzzySuggestModal` / `SuggestModal` / `AbstractInputSuggest` / `PopoverSuggest` and sees matches ranked by: contiguity > case-fidelity > compact span > leftmost start > shorter candidate.
- The user can type the CJK characters of a note/command title and get per-codepoint fuzzy match without a romaji keyboard.
- The user can type an acronym (e.g. `gmf` for `getMarkdownFiles`) and have the char-fuzzy second pass find it even if the word-token first pass misses.
- The user can type partial text, whitespace-separated, in any order within each token — `md file` matches `"Markdown file"` but NOT `"file Markdown"` (because left-to-right progression is enforced inside the candidate).
- The user sees highlighted spans in the suggestion rows — this is `renderResults` (defined in `rendering/renderResults.js`) wrapping the char-range arrays into DOM. The matcher owns the ranges; the renderer owns the DOM. See §12.
- **Empty query shows every item** (in every `FuzzySuggestModal` — commands palette on fresh open, quick switcher on fresh open, …).
- In Bases views, the user types into the search-row text input and sees the already-resolved Bases rows filtered substring-AND across all columns (`QueryController.applySearchQuery`).
- The user's `userIgnoreFilters` vault config change causes every open Bases view to drop its cached query state and re-run (`QueryController.onConfigChanged`).

**Features NOT powered by this domain** (contrary to Pass 1 taxonomy): the global search-panel DSL parser (`tag:foo`, `path:bar`, `-word`, `"quoted"`, `/regex/`, `line:`, `block:`, `section:`, `match-case`/`whole-word` flags, AND/OR/NOT). Those belong to the internal `global-search` plugin which the audit did not ingest.

---

## 10. Extension surfaces exposed

| Surface | Registration verb | Consumer call site | What plugins supply |
|---|---|---|---|
| `FuzzySuggestModal<T>` subclass | — (subclass) | `ui/popups/FuzzySuggestModal.js:342` | `getItems(): T[]`, `getItemText(item: T): string`, `onChooseItem(item, evt)`. Plugins use this for "pick from a finite list" pickers. |
| `AbstractInputSuggest<T>` subclass | — (subclass) | `ui/popups/AbstractInputSuggest.js:412` | `getItems(): {value, display?, ...}[]`, `renderSuggestion`, `selectSuggestion`. For attached-to-an-input autocomplete. |
| `prepareFuzzySearch` as exported helper | `require("obsidian").prepareFuzzySearch` | — | Plugins call this to get a reusable `(text) => match|null` closure. |
| `fuzzySearch` / `prepareQuery` / `prepareSimpleSearch` / `sortSearchResults` | `require("obsidian").fuzzySearch` etc. | — | Raw helpers for plugins implementing custom UIs. |

`QueryController` is **not a plugin-facing surface** (not part of the published `obsidian` typings; not used outside Bases-plugin code inside `obsidian/` tree).

---

## 11. Corbomite mapping

| Obsidian concept | Corbomite equivalent | Status | Notes |
|---|---|---|---|
| `prepareQuery` / `fuzzySearch` / `prepareFuzzySearch` / `prepareSimpleSearch` / `sortSearchResults` | — | Missing | Five-helper matcher. Must reproduce the two-pass (word-token → char-fuzzy) strategy and the five-term scoring formula to preserve ranking parity with Obsidian plugin expectations. Ship as free functions in a new `libs/search/` or as statics alongside `libs/storage/`. SQLite `LIKE` is not equivalent — doesn't return offsets. |
| `{score, matches: [[s,e],...]}` return shape | `struct SearchMatch { QString notePath; QString snippet; double score; }` (`libs/storage/include/corbomite/storage/SQLiteIndex.h:14-18`) | Partial | **Lacks the byte-range array.** Highlight-span rendering needs a `QVector<std::pair<int,int>> matches` field or equivalent. |
| Global-search DSL (`tag:`, `path:`, `file:`, `line:`, `block:`, `section:`, regex, quoted phrase, `-exclusion`, `match-case`/`whole-word`, AND/OR/NOT) | `src/sidebar/SearchPanel.cpp:71` (`// TODO`) | Missing | `SearchPanel::executeSearch` passes the raw query directly to `SQLiteIndex::search(query)` (plain FTS5 `MATCH`). No operator parsing. |
| In-memory incremental fuzzy over Quick Switcher / Command Palette | — | Missing | No command palette or quick switcher exists yet. When added, share one matcher across all surfaces. |
| `renderResults(el, text, {matches}, offset?)` highlight-span renderer | — | Missing | See `domains/rendering.md` Pass 2 additions. Corbomite needs `Markoff::renderHighlightedRuns(...)` as the shared chokepoint. |
| `debounce(notifyView, 50)` post-keystroke UI refresh | `QTimer(300 ms)` in `SearchPanel.cpp:35-39` | Have — longer interval | 300 ms is fine for SQLite FTS but feels laggier than Obsidian's 50 ms. |
| `SQLiteIndex::search(QString, int maxResults=100)` | `libs/storage/SQLiteIndex.h:44` | Have — but FTS-only | No fuzzy scoring, no operator DSL, no per-match byte offsets. |
| Tag search `openGlobalSearch("tag:" + name)` | `SQLiteIndex::notesWithTag(QString)` (`SQLiteIndex.h:54`) | Partial | Direct Qt call, not a query string. Acceptable divergence. |
| Per-file `MetadataCache` access for headings/sections/blocks/frontmatter search | `libs/storage/SQLiteIndex` tables for links/tags only | Partial | Obsidian's DSL reaches into `CachedMetadata.headings[].heading`, `.sections[].{type,id}`, `.blocks[id]` (see `domains/metadata.md` §2). Needs new heading/section tables to support `section:`/`block:`. |
| `getAllTags(cache)` tag list | `SQLiteIndex::allTags()` | Partial | Per `domains/metadata.md` §11 — no frontmatter `tags:` merge, no subtag-prefix counting. |
| `QueryController.applySearchQuery(entries, order)` token-AND filter | — | Missing (Bases not implemented) | Scope grows with Bases. |

**Key architectural decision for Corbomite.** Obsidian's model is SQLite-free: every Quick Switcher / Command Palette / SuggestModal filter is an in-memory fuzzy score across an in-RAM list (commands, file paths, tags). The matcher is O(|query| · |candidate|) on a few thousand items — fits a single event-loop tick. Corbomite decisions per surface: (1) **Quick Switcher** and **Command Palette** must use the ported fuzzy matcher over small in-RAM arrays — `QSortFilterProxyModel` with `QRegularExpression` is not equivalent. (2) **Search panel** can keep FTS5 for candidate `MATCH` retrieval, but needs byte-range `matches` for highlight spans — either rewrite FTS5 `snippet()` output into offsets, or re-score FTS candidates through the fuzzy matcher. Option 2 is closer to Obsidian. (3) Every **SuggestModal-equivalent** must use the fuzzy matcher, not `QCompleter` prefix match.

---

## 12. Markoff gap confirmations / discoveries

N/A — search is not an editor/rendering surface. The adjacent `rendering/renderResults.js` (`renderResults(el, text, {matches}, offset?)`) is owned by the `rendering/` domain (`docs/obsidian-audit/domains/rendering.md` Pass 2 additions already captures it as a gap). This domain *produces* the `{matches}` structure but does not render it.

---

## 13. Open questions

1. **The full search-DSL grammar (`tag:`, `path:`, `file:`, `line:`, `block:`, `section:`, `/regex/`, `"quoted phrase"`, `-exclusion`, `match-case`, `whole-word`, AND/OR/NOT).** Pass 2 focus #2 but **not** in `obsidian/search/`. 21 `openGlobalSearch("tag:" + e)` call sites in `_internal.js` (`:290927` on) prove the DSL exists in the internal `global-search` plugin — out of audit scope. For Corbomite planning the DSL must be reverse-engineered from Obsidian's user documentation.
2. **Where is `renderMatches` defined?** Every suggestion-renderer calls `renderMatches(el, text, matches, offset?)` (e.g. `ui/popups/PopoverSuggest.js:533`). `rendering/renderResults.js:5-7` re-exports it but the underlying function is not `function renderMatches(...)` anywhere in `_internal.js` — likely assigned inside a private IIFE the audit didn't reach. Cross-domain question for Pass 3.
3. **How does `global-search` cache / re-run per-keystroke?** Pass 2 focus #5 — belongs to the `plugin/internal-plugins/search/` audit. The lower matcher is stateless; caching is a consumer concern.
4. **`FuzzySuggestModal` result-count cap?** Base class (`ui/popups/FuzzySuggestModal.js:351-370`) never caps; subclasses vary (file picker caps at 100 per `_internal.js:279737`). Corbomite should adopt 100 as the default.
5. **`prepareSimpleSearch` has zero call sites in the audited tree.** Exported only for plugin use. Confirm this is intentional.
6. **Is the 50 ms `requestNotifyView` debounce trailing or leading edge?** `debounce` lives in `utils/` (not audited). The `request…` naming hints trailing-edge coalescing.

---

## 14. Recommended Pass 3 synthesis input

1. **`{score, matches: [[s,e],...]}` is the universal result shape** consumed by every highlight renderer. Promote to `FEATURE-MATRIX.md` as "Search result contract" — Corbomite's `SearchMatch` must gain a `QVector<std::pair<int,int>> matches` field. Flag the two-pass (word-token + char-fuzzy) algorithm as mandatory for Quick-Switcher/Command-Palette parity.
2. **Global-search DSL (`tag:`, `path:`, `file:`, `line:`, `block:`, `section:`, regex, quoted phrase, exclusion, AND/OR/NOT) is out-of-tree** — the parser lives in the internal `global-search` plugin, not in `obsidian/search/`. Promote to `GAP-ANALYSIS.md` as "DSL parser — source unavailable; reverse-engineer from user documentation." `src/sidebar/SearchPanel.cpp:71` has a `// TODO: Support Obsidian search operators` — the TODO is larger than it appears.
3. **Search and Suggest surfaces share one matcher.** Quick Switcher, Command Palette, FuzzySuggestModal, AbstractInputSuggest, PopoverSuggest — same `prepareQuery + fuzzySearch + sortSearchResults` pipeline. Non-negotiable for Corbomite: do not split into `QRegularExpression` / substring / FTS per-surface — plugin-compat breaks the instant a plugin calls `prepareFuzzySearch(q)` and observes ranking divergence.

---

## 15. Cross-domain references

| Other domain | Reference type | Brief description |
|---|---|---|
| `rendering` | consumer | `rendering/renderResults.js:5-7` wraps `renderMatches(el, text, n?.matches, offset)` — highlight-span renderer for the `{matches}` offsets produced here. `rendering/RenderContext.js` uses `prepareQuery` to filter link suggestions. |
| `metadata` | data-source | `QueryController` subscribes to `metadataTypeManager.on("changed")` (`:214`). Search surfaces filtering by tags/headings/frontmatter must pull from `MetadataCache` — see `domains/metadata.md` §2 for `CachedMetadata` (`headings[]`, `sections[]`, `blocks{}`, `tags[]`, `frontmatter{}`, `frontmatterLinks[]`) and §8 invariants (`tags[]` is inline only — frontmatter `tags:` merged via `getAllTags`). Corbomite's future DSL must hit equivalent data (today `SQLiteIndex::allTags`/`notesWithTag`; heading/section tables absent). |
| `vault` | data-source | `QueryController` reads `vault.on("config-changed")` and `vault.read(file)` (embed wrapper). Path queries use `Vault.getAllLoadedFiles`/`getFiles` — per `domains/vault.md` §1, `TFile.path` is NFC, `/`-separated, extension-suffixed. |
| `core` | consumer of `App` | `QueryController` reads `app.vault`, `.metadataCache`, `.metadataTypeManager`, `.fileManager`, `.dragManager`, `.importAttachments`, `.internalPlugins`. |
| `views` | consumer | `ui/popups/FuzzySuggestModal`, `AbstractInputSuggest`, `PopoverSuggest`, `SuggestModal` — all use this matcher. `views/ViewRegistry.js:193` uses `prepareQuery` for footnote suggester. `views/TextFileView.js:859,1897` are de-minifier IIFE-copy artefacts of the Bases-search methods. |
| `bases` | consumer | `QueryController` is the Bases orchestrator — used by `BasesEntry`, `BasesQueryResult`, `BasesView`. |
| `plugin` | consumer | Plugins import the five helpers via the `obsidian` module. Subclassing `FuzzySuggestModal<T>` / `AbstractInputSuggest<T>` is the canonical plugin path. |
| `ui/popups` | sibling | `SuggestModal`, `FuzzySuggestModal`, `AbstractInputSuggest`, `PopoverSuggest`, `Notice`, `SecretComponent`, `SettingTab` all call `prepareQuery` + `fuzzySearch`. `sortSearchResults` reads only `match.score`. |

**Short symbols from other domains referenced by name here:**

| Short symbol | Defined in | Used here for |
|---|---|---|
| `Ty`, `Iy`, `Ly`, `wy`, `xy` | `utils` (same IIFE at `_internal.js:83097-83224`, declared `app.js` lines 62769-62901) | Matcher internals: `Ty` two-mode word/char matcher; `Iy` simple substring-AND scorer; `Ly` tokens-all-present finder; `wy` range-merger; `xy` scoring formula. Not exported. |
| `Cy`, `ky`, `Ey` | `utils` (`_internal.js:83109-83113`) | Character-class regexes: whitespace / ASCII+General punctuation / CJK-Tibetan-Japanese. Consumed by `prepareQuery` and `Ty`. |
| `debounce` | `utils` | `requestNotifyView = debounce(notifyView, 50)` and embed-save `debounce(..., 1e3)`. |
| `BasesEntry`, `DY` (`iY`), `uY`, `aq`, `yI`, `FY` | `bases` | Bases data model and formula runner — `QueryController` drives these. |
| `Component`, `Events` | `core`/`ui/components` | Base classes for `QueryController` and its nested `k$`. |
| `TFile` | `vault` | Input type to `addResult(file, entry)`; drag-drop file-list reification. |
| `Notice` | `ui/popups` | `notifyView` surfaces `this.errors` via `new Notice(msg)`. |
| `gm.plugins.bases` (alias `w$`) | `plugin`/Bases i18n shim | Localised error messages (`msgErrorViewNotFound`, `msgErrorUnknownViewType`, `msgErrorUnableToParse`, `msgErrorFilterFailedToEvaluate`). |
| `ub`, `cb` | `utils` (Intl.Collator) | Result secondary sort by file path. |
