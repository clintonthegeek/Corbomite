# Pass 2 fan-out plan

Pilot on `vault` produced `domains/vault.md` (6014 words) and surfaced six concrete template revisions. All revisions are now in `TEMPLATE.md`. Pilot verdict: **GO**.

## Remaining domains: 20 directories → 13 agents

After bundling, 13 agents cover the remaining 20 in-scope directories:

### Individual agents (10 — Opus)

| # | Domain | Approx. files | Why Opus | Expected words |
|---|---|---|---|---|
| 1 | `workspace` | 12 | layout state machine; split-pane + side-dock geometry; event payloads critical | 4000–6000 |
| 2 | `metadata` | 4 | incremental parser + resolved/unresolved link graph; invariants are subtle | 3000–5000 |
| 3 | `views` | 6 | View / FileView / TextFileView / ViewRegistry — the subclass hierarchy everything hooks | 3000–5000 |
| 4 | `editor` | 6 | CM6 wrapper + EditorSuggest; IME/bidi/autocorrect; heavy Markoff-gap input | 3500–5000 |
| 5 | `editor/markdown` | 6 | MarkdownView + preview pipeline; largest editor-domain file; three-mode semantics | 4500–6000 |
| 6 | `rendering` | 11 | Markdown-to-HTML + math/mermaid/pdf/prism loaders; huge Markoff-gap input | 4000–6000 |
| 7 | `bases` | 25 | biggest Corbomite gap; `.base` schema + `Value` hierarchy; own spec doc essentially | 5000–6000 |
| 8 | `core` | 3 | App/Events/Scope; tiny in LOC but architecturally load-bearing | 2500–4000 |
| 9 | `plugin` | 1 | full enumeration of `register*`/`add*`; extension-surface bible for our future plugin system | 3000–4500 |
| 10 | `search` | 6 | fuzzy matcher + query grammar; self-contained but subtle for compat | 2500–4000 |

### Individual agents (2 — Sonnet, small and precise)

| # | Domain | Approx. files | Why Sonnet | Expected words |
|---|---|---|---|---|
| 11 | `parsing` | 8 | frontmatter/YAML/link-text; contract-precise but small | 2000–3500 |
| 12 | `settings` | 4 | Setting/SettingTab/PluginSettingTab; four classes | 2000–3000 |

### Bundled agents (2 — Opus for UI, Sonnet for leaf)

| # | Bundle | Dirs | Why bundled | Expected words |
|---|---|---|---|---|
| 13 | UI primitives | `ui/components`, `ui/icons`, `ui/menu`, `ui/popups` | coherent client-side UI layer; avoids 4 separate weak docs | 4500–6000 |
| — | *(skipping — Sonnet handles 14)* | | | |

Wait — counting: 10 Opus + 2 Sonnet + 1 Opus bundle = 13.

Plus the leaf-utility bundle:

| 14 | Leaf utilities | `utils`, `platform`, `secrets`, `network` | support libraries; avoid four thin docs | 3000–4500 |

Total: **14 agents** (11 Opus, 3 Sonnet). Running in 3 waves to respect rate limits and surface problems before they multiply.

## Wave structure

### Wave 1 — Skeleton (3 Opus agents, parallel)

- `workspace`, `metadata`, `views`

Rationale: everything else cross-references these three. `core` and `plugin` were originally here but were moved to Wave 3 because they are *aggregators* — they will produce sharper docs if the consumer domains they cite are already complete. `Events`/`Scope` in `core` are ~70 LOC; waiting for Wave 3 costs nothing.

**After Wave 1:** read results, confirm no systemic template failures, adjust if needed.

### Wave 2 — Editor & rendering (4 Opus agents, parallel)

- `editor`, `editor/markdown`, `rendering`, `bases`

Rationale: the three editor/rendering domains are interlocked; doing them together lets cross-references self-check. `bases` piggybacks because it is large and has independent dependencies — waiting another wave wastes time.

### Wave 3 — Aggregators + leaves (7 agents, parallel; mixed models)

- `core` (Opus), `plugin` (Opus), `search` (Opus), UI-primitives bundle (Opus), `parsing` (Sonnet), `settings` (Sonnet), leaf-utilities bundle (Sonnet)

Rationale: `core` and `plugin` are dependency-informed — every `App.<field>` and every `register*` will cite into completed Wave 1/2 docs. Smaller domains (parsing/settings/search/bundles) trail because their cross-references also resolve best after the big docs exist.

## Per-wave monitoring

After each wave:
1. Spot-check one output against the template (word count, section completeness, de-minifier note, Section 15 populated)
2. Glance at `01-markoff-gaps.md` and `02-extension-surfaces.md` for new entries (sign of good opportunistic capture)
3. If a systemic issue appears (e.g. agents consistently misreading a template instruction), pause and revise before next wave

## Cost estimate (rough order of magnitude)

At current token prices, assume ~50k input tokens per Opus agent for reading its domain + templates, and ~20k output. 11 Opus runs ≈ 600k input + 220k output ≈ ballpark cost; 3 Sonnet runs add negligibly. Well within scope for a once-per-project reverse-engineering pass.

## Pass 3 plan (after all 14 docs land)

Single capable agent (Opus, probably need larger context) reads the ~15 Pass-2 domain docs plus the two running lists and produces:

1. `FEATURE-MATRIX.md` — every user-visible Obsidian feature, indexed by feature (not by Obsidian file). Columns: feature, Obsidian domain(s), Corbomite status (Have/Partial/Missing), Corbomite file/library.
2. `VAULT-FORMAT.md` — the canonical on-disk compatibility spec, compiled from every Section 3 across the Pass 2 docs. Every file and schema in `.obsidian/`, plus `.md`/`.canvas`/`.base` body conventions, plus frontmatter conventions.
3. `GAP-ANALYSIS.md` — prioritised gap list against current Corbomite code. Feeds Corbomite roadmap planning.
4. `PLUGIN-API-SKETCH.md` — synthesised from `02-extension-surfaces.md` and the `plugin` domain doc; outlines the extension-point shape Corbomite's future plugin API should mirror.
5. `SHARED-SYMBOLS.md` — reconciliation of short-name cross-references across domains.

Pass 3 deliberately does **not** re-open Obsidian JS. If any Pass 2 doc has the wrong information, Pass 3 produces the wrong synthesis — that's by design; the per-domain docs are the source of truth.
