# Audit addendum — taxonomy QueryController refutation, Bases `summaries` parse quirk, corpus-path housekeeping

**Corrects:** `00-taxonomy.md` (search section). **Extends:** `domains/bases.md` §3/§13. **Housekeeping:** provenance paths in `addenda/2026-04-17-bases-formula-dsl.md`.

**Date:** 2026-06-10
**Discovered during:** verification pass of audit claims against the decompiled source.
**Source:** decompiled Obsidian 1.12.7 corpus at `/home/clinton/bin/ObsidianRAW/audit/` (paths relative to `renamed/obsidian/`). All claims re-checked 2026-06-10.

## 1. 00-taxonomy.md's `QueryController` search-DSL claim is REFUTED — and the taxonomy carries no correction markers

**Wrong claim:** 00-taxonomy.md (search section) — "The upper `QueryController` is the global-search engine: parses Obsidian's search-DSL (`tag:`, `path:`, `file:`, `line:`, `block:`, `section:`, regex, quoted-phrase, AND/OR/NOT, …)" and "`QueryController` — owns DSL parsing, file iteration, result aggregation…".

**Verified reality:** `QueryController` is the **Bases view controller**, not a search-DSL engine. Its only text matching is `applySearchQuery` (`tree/obsidian/search/QueryController.js:670-698`): it calls `prepareQuery(this.searchQuery).tokens` and filters Bases entries by **lowercased substring containment with AND semantics** — every token must `includes()`-match somewhere in the entry's stringified property values; an entry passes when the token list empties. No `tag:`/`path:` operators, no regex, no boolean grammar. The real DSL lives in the **`global-search` internal plugin**, which is out-of-tree (not in the extracted corpus) and reached at runtime via `getEnabledPluginById("global-search")` (59 call sites in `src/_internal.js`).

search.md §0 (de-minifier artifact note) and §9 already state this refutation explicitly — the gap is that **00-taxonomy.md was never annotated**, so a reader landing on the taxonomy first gets the wrong picture with no pointer to the correction. Per project convention the taxonomy stays frozen; the operative guidance is:

> **Treat `00-taxonomy.md` as a routing index — which domain doc to read — not as a source of factual claims.** Where a Pass 2 domain doc contradicts the taxonomy, the domain doc wins; where this addenda directory contradicts both, the addendum wins.

**Implementation impact:** any plan citing the taxonomy for search scope must re-base on search.md §9/§13 — the search-panel DSL has **no extracted source** and must be reverse-engineered from user documentation.

## 2. Bases parse quirk: top-level `summaries:` is double-handled (parsed AND copied into `unrecognizedData`)

**Extends:** bases.md §3 (`.base` on-disk contract). Not previously documented.

**Verified reality:** the view-config parser reads `summaries` into `summaryFormulas` (`tree/obsidian/bases/BasesViewConfig.js:186`, `:250-269`), but the recognized-key switch that routes leftover keys into `unrecognizedData` **omits `summaries` from its case list**:

```js
// tree/obsidian/bases/BasesViewConfig.js:294-302
switch ((A = D[T])) {
  case "views": case "filters": case "display": case "properties":
  case "formulas": case "newItemFolder": case "newItemTemplate":
    continue;
  default:
    h.unrecognizedData[A] = x[A];
}
```

So a top-level `summaries:` block is *also* copied into `unrecognizedData`. At serialize time `getSerializable` starts from `Object.assign({}, this.unrecognizedData)` and then **overwrites** `e.summaries` from the live `summaryFormulas` (`:309`, `:324-333`) — net effect harmless for Obsidian itself. It is relevant to byte-level round-trip writers: a port that faithfully reproduces `unrecognizedData` passthrough but serialises `summaries` *before* (or instead of) the overwrite would emit the stale parsed-time copy. Replicate the order: unrecognized first, computed `summaries` last.

**Also stale:** bases.md §13 Open Q2 ("`QueryController` source not extracted… Pass 3 should grep app.js…") and the matching §14 #3 flag — `QueryController` **has since been extracted** to `tree/obsidian/search/QueryController.js` (819 lines) and audited in search.md. Q2 is answered; the incremental-refresh question (Q3) should be pursued against that file, not re-extracted.

## 3. Provenance note: bases-formula-DSL addendum cites a moved corpus

`addenda/2026-04-17-bases-formula-dsl.md` cites its sources under `~/src/obsidian-audit/` (`renamed/…`, `formatted/obsidian/app.js`). The corpus has **moved to `/home/clinton/bin/ObsidianRAW/audit/`** (same subdirectory structure: `renamed/`, `formatted/`, `unbundled/`, `extracted/`). Spot-checked 2026-06-10: the addendum's line citations still match at the new location — e.g. `DK` (formula wrapper) at `renamed/obsidian/src/_internal.js:387838` and `OK` (error-recovery walker) at `:388032`, exactly as cited. No re-verification of its content is implied or needed; only the path prefix is stale.
