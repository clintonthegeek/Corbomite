# Cluster O — Advanced Query Layer (SCOUTING)

> **Living-status note:** This is a *scouting document*, not a plan. Captures the strategic rationale + prior-art breadcrumbs for a post-Obsidian-parity capability: using Corbomite's native-C++ substrate to expose a structured-query layer (graph + SQL) over the markdown vault. Live status is in [`docs/PROJECT-STATE.md`](../../PROJECT-STATE.md). When expanded to a full plan, rename to drop `-SCOUTING` suffix and update `INDEX.md`.

**Scouting written:** 2026-04-14.
**Expand to full plan when:** Clusters A, B, I, and K have landed (authoritative source of truth for links, vault I/O, MetadataCache, and Bases' incremental-refresh pattern). Also benefits from observing real power-user friction — if nobody actually asks for graph queries, don't build it.

**Covers (pre-planning):** a new-by-invention category of capability not present in Obsidian vanilla — an advanced query layer combining a graph DB (for link traversal, 2+-hop queries) with enriched SQLite FTS (for PageRank-weighted search) over an existing markdown vault.

## Strategic rationale

Obsidian stores notes in plain markdown — deliberately, as a portability/longevity guarantee. It *does* maintain databases internally (MetadataCache is IndexedDB; Bases provides tabular-query; FTS search is built in), but these are strictly derived from the markdown and are positioned as implementation details, not first-class surfaces.

This leaves a real capability gap that a faction of Obsidian's power-user community tries to fill via plugins:

- **Dataview** (most popular): a JavaScript-based query language over the metadata cache. Supports JS-like queries and inline code in notes. Widely used; widely hits JS performance ceilings on vaults >10k notes; hard to compose; notorious for slow-rendering result tables.
- **Datacore** (successor in development): IndexedDB-backed, structurally more ambitious, but fundamentally constrained by the in-browser JS runtime.
- **JuggL** / **Breadcrumbs**: graph-traversal plugins that reimplement parts of a graph DB in JavaScript. Scale cliffs around a few hundred nodes.
- **Obsidian Bases** (shipped feature): table-view over frontmatter via in-memory Value types and a formula DSL. Powerful but *flat* — no traversal, no multi-hop.

These plugins reveal the *shape* of the unmet demand: users want to ask **relational and graph-shaped questions about their notes**, and the JS-in-a-browser substrate Obsidian ships on can't serve them at scale.

**Corbomite's native C++ / Qt6 substrate has a structural advantage here.** Embedded graph databases (KuzuDB is the obvious candidate — Apache 2, Rust+C++, mature, embeddable) run in-process with zero RPC overhead, handle millions of nodes on a laptop, and don't share a single-threaded event loop with the UI. SQLite FTS5 in native code outperforms its in-browser equivalents. A Bases-like query layer grafted onto a graph index gives us capabilities Dataview users would need a rewrite to match.

**The polemical "Obsidian-as-memory is bad" critique (Nov 2025 Substack) generalises this:** it's right that AI-assistant substrates benefit massively from structured databases, and right that people misuse notes apps as databases. But the generalisation to "notes apps should become databases" is wrong — plain-text markdown has won across 50 years of computing for durable reasons: portability, longevity, tool-ecosystem interop. Corbomite's defining value is *being Obsidian-vault-compatible*; replacing markdown with a DB vault (call it "Option 3") would destroy that.

**Cluster O is Option 2:** an *additive, optional, opt-in* query/graph layer built *over* the unchanged markdown vault. The indexes are derived, regenerable, deletable, and never authoritative. Markdown remains the source of truth. If you blow away `.obsidian/corbomite-indexes/`, Corbomite rebuilds the layer on next open and loses nothing. If you give your vault to a friend who uses Obsidian, it's still a perfectly normal Obsidian vault — they never see the indexes.

## What Cluster O would include (when expanded)

Sketch only — details owed to the full plan, written after A/B/I/K land.

1. **Embedded graph database over link structure.**
   - KuzuDB as primary candidate. Alternatives: roll-our-own in-memory graph (lighter, easier to embed, sufficient for vaults <100k notes), or LMDB-backed adjacency lists.
   - Schema: `Note` nodes with frontmatter-sourced properties; `LINKS_TO` / `EMBEDS` / `TAGGED_AS` / `CREATED_AFTER` edges; optional user-defined relations declared via frontmatter conventions.
   - Queryable via a small Corbomite DSL (Cypher-lite, or reuse Bases' DSL once extracted) — **not** exposed via in-note JS executions (the Dataview failure mode).
   - Populated at vault-open from MetadataCache (Cluster I); incrementally updated via MetadataCache's 5 fine-grained events.
2. **PageRank-weighted FTS.**
   - SQLite FTS5 scored with a separate pass weighting results by PageRank of the containing note over the link graph.
   - "Find notes about X, prioritised by how well-connected they are to my most-written-about clusters."
   - Regenerable; ~seconds for a vault of 10k notes.
3. **Vault-mutation transaction log (for multi-agent safety).**
   - When multiple AI agents or external tools edit the vault, writes go through a vault-mutation API that serialises through a SQLite transaction log, replaying to markdown with conflict resolution.
   - Markdown is still the source of truth; the log is the concurrency layer.
   - Obsidian doesn't have this. For AI-assistant use cases this is a differentiator.
4. **"AI companion export" (harmless middle ground).**
   - One-shot `corbomite vault export-sqlite` command that snapshots the vault into a single queryable SQLite file for external consumption.
   - NOT the vault. NOT authoritative. Just a "here's my notes, as a DB, right now" artefact.
   - Solves the Substack-polemic use case (AI-assistant memory wants a DB) without compromising Corbomite's core.
   - Could ship in ~2 weeks. Earlier than the rest of Cluster O.
5. **Power-user query UI.**
   - Tab/panel showing query editor + result table.
   - Saved queries (`.corbomite-query` files? `.base`-like?).
   - Query-result-as-view (cards, table, graph visualisation reusing `libs/forcegraph`).
6. **Plugin extension surface.**
   - Plugins can register custom relation types (`TAGGED_AS`, `MENTIONS`, `REFERENCES`) and read/write the graph.
   - Plugins can author saved queries shipped as presets.

## Design principles (the north star)

1. **Additive, never replacing.** Markdown files are the source of truth, always. Graph-DB and SQLite indexes live under `.obsidian/corbomite-indexes/` (or similar); regenerable; deletable without data loss. If the user opens the vault in Obsidian, everything works.
2. **Opt-in at first.** A toggle in settings enables the advanced query layer. Don't force database-backed indexing on users with a 50-note vault who just want to write.
3. **Don't expose it as "your vault is a database."** The metaphor stays "notes and links." The DB is how we answer interesting questions about those notes; it's not the thing.
4. **Compat break forbidden.** Any behaviour Cluster O enables that would make a vault *unopenable in vanilla Obsidian* is a bug, not a feature.
5. **Indexes are regenerable.** Every index must be rebuildable from markdown + `.obsidian/` config in <5 minutes for a 10k-note vault. If an index format rot happens, the user deletes it and we rebuild.

## Prior-art breadcrumbs (local paths — do NOT clone)

| Target | Local path | Note |
|---|---|---|
| **KDE metadata-search engine** | `~/src/kde/src/baloo/` (present locally) | **Strongest KDE prior art.** Baloo indexes file metadata + full-text across the user's whole filesystem, exposes a structured query API, handles concurrent access. Architecturally parallel to what Cluster O would build, just scoped to a vault instead of `$HOME`. Study its SQLite schema choices, its FTS integration, its change-notification reactor |
| **Baloo term parser for DSL patterns** | `~/src/kde/src/baloo/src/lib/term.cpp` | Concrete prior art for writing a structured query DSL parser that translates to SQL |
| **KDevelop DUChain (semantic-database for C++)** | `~/src/kde/src/kdevelop/kdevplatform/language/duchain/` | Embedded graph-shaped database over symbols + relationships; persistent, incremental, queryable. Different domain (code vs notes) but same shape of problem |
| **Embedded graph DB selection** | External — KuzuDB (Apache 2, Rust+C++; primary candidate), LMDB (alternative backing store for a rolled index), or a from-scratch in-memory adjacency-list index (sufficient for small vaults) | The primary build-vs-buy decision of Cluster O |
| **Corbomite already has force-directed graph layout** | `libs/forcegraph/` | Visualisation side is solved; we're not starting from nothing on graph rendering |

## What to learn from Obsidian plugins (Dataview, Datacore, Breadcrumbs, JuggL)

A dedicated Explore-agent prompt when Cluster O expands should survey these plugins' design (not code — they're AGPL/MIT, mixed). Topics to extract:

1. **Which query patterns users actually want.** Dataview's most-used queries are heavy tells.
2. **Performance cliffs users hit.** 10k-note barrier in Dataview, specific queries that choke Datacore, etc.
3. **The UX of saved queries.** Do users embed queries in notes, keep them in a sidebar, or build a dedicated page? Divergent in the plugin space.
4. **Frontmatter conventions** Dataview popularised (`rating::`, `status::`, inline metadata syntax). Corbomite honouring the same conventions = seamless Dataview-user migration.
5. **Plugins' handling of the Obsidian-upgrades-MetadataCache-shape-and-breaks-us pain.** Informs our own compat commitment.

## Expansion triggers & sequencing

- **Primary trigger:** A, B, I, K all landed. At that point we have correct links (A), clean vault I/O (B), fine-grained metadata events (I), and a learned example of an incremental-refresh engine (K's QueryController).
- **Secondary trigger / demand signal:** power users asking. If nobody asks for graph queries after we ship A–K, don't build it. This is a capability-for-power-users; don't impose it top-down.
- **Partial advance trigger:** the "AI companion export" feature (item 4 in the scope sketch) can ship after A + I alone. ~2 weeks of work. Gauges user interest cheaply.

## Do NOT do

- **Full Option 3 — vault as DB.** Breaks Obsidian compat. Corbomite's defining value disappears.
- **Require the query layer for core features.** Search, backlinks, outlinks, graph view must all work without it (they do today via SQLiteIndex + VaultModel). Cluster O is a power-user overlay.
- **Use the graph DB as the write path for notes.** Notes still land in `.md` files; the graph DB reads them. Agents writing through the graph DB is a *separate* subfeature (item 3, "vault-mutation transaction log") with its own design carefully constrained to preserve markdown-as-authoritative.

## Open questions when expanding

1. **KuzuDB vs rolled index?** KuzuDB is ~12MB embedded, adds build-time + binary-size cost. A rolled in-memory adjacency-list index is ~200 lines of C++ and handles vaults <100k notes fine. Benchmark vs library choice.
2. **Where do advanced query results live?** A dedicated tab type (new ViewRegistry entry — Cluster G dependency)? An overlay on the existing search panel? Both?
3. **Does the query layer cross into Bases' territory?** Bases already does tabular queries over frontmatter. Cluster O's FTS + graph extends this. Possible consolidation — or possible maintain-two-systems-forever. Cluster K's retro should inform.
4. **Frontmatter conventions:** do we adopt Dataview's inline metadata syntax (`rating:: 5`) as a first-class parse target? Nonstandard YAML; but millions of notes use it.

## Rough phasing (for planning, not prescriptive)

- Phase 1: AI companion export (SQLite snapshot). Cheap, gauges demand.
- Phase 2: Graph DB integration (KuzuDB-or-rolled). Populate from MetadataCache.
- Phase 3: Query DSL parser + executor. Initially minimal: `MATCH (n:Note)-[:LINKS_TO*1..3]->(m:Note) WHERE n.tag="x" RETURN m`.
- Phase 4: PageRank-weighted FTS.
- Phase 5: Query UI (tab/panel, saved queries).
- Phase 6: Vault-mutation transaction log (multi-agent safety).
- Phase 7: Plugin extension surface (register custom relation types, register saved queries).
