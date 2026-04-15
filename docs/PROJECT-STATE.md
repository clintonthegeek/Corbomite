# PROJECT-STATE

> **Living document.** Single source of truth for "where we are right now" on the Obsidian-compatibility roadmap. Keep under 200 lines. Update at the end of every meaningful work session per the Ritual 2 / Ritual 3 checklists in `docs/CONTRIBUTING-OPS.md`. Older "Recent decisions" entries archive to `docs/decisions-archive.md` quarterly.

**Last updated:** 2026-04-14 — added Cluster H full plan + G/K scouting docs; system stand-up still current focus.

---

## Current focus

**Idle.** Pass 3 audit synthesis complete; 5 full + 3 stub cluster plans written. No cluster in flight. See **Roadmap** below to choose next; recommended starting points are Cluster A (link/frontmatter correctness) and/or Cluster B (vault I/O) — both unblock the bulk of downstream work.

---

## Roadmap (14 clusters)

Status legend: `Not started` · `Plan-needed` (no cluster plan yet) · `Stub plan` (sketch exists, expand before dispatch) · `In progress (phase N)` · `Blocked — waiting on X` · `Done` · `Deferred`.

| Cluster | Title | Plan | Status | Blocks / Notes |
|---|---|---|---|---|
| A | Link / frontmatter correctness | [full](superpowers/plans/2026-04-14-cluster-a-link-frontmatter-correctness.md) | Not started | Keystone — blocks D, F, I, J, K, L |
| B | Vault I/O | [full](superpowers/plans/2026-04-14-cluster-b-vault-io.md) | Not started | Blocks C, E, G, H, N |
| C | Lifecycle / plugin primitives | [full](superpowers/plans/2026-04-14-cluster-c-lifecycle-plugin-primitives.md) | Not started | Blocked — waiting on B Phase 1–2 (DataAdapter + VaultConfig). Also resolves vault-switch crash |
| D | Search / suggester parity | [full](superpowers/plans/2026-04-14-cluster-d-search-suggester-parity.md) | Not started | Weakly blocked on A (LinkUtils for heading-match search) |
| E | Markoff three-mode pivot | [full](superpowers/plans/2026-04-14-cluster-e-markoff-three-mode-pivot.md) | Not started | Blocked — waiting on B Phase 3 (WorkspaceState) and A Phase 1 (frontmatter parsing) |
| F | Templates / Daily Notes / Moment | [stub](superpowers/plans/2026-04-14-cluster-f-templates-daily-notes-moment-STUB.md) | Stub plan | Expand after A + I land |
| G | Views hierarchy + TextFileView contract | [scouting](superpowers/plans/2026-04-14-cluster-g-views-hierarchy-SCOUTING.md) | Scouting doc | Expand to full plan when Cluster C Phase 1 lands |
| H | Menus / hover / suggester UI | [full](superpowers/plans/2026-04-14-cluster-h-menus-hover-suggester-ui.md) | Not started | Parallelisable with A/B/D; blocked softly on C Phase 1 (Component base) |
| I | MetadataCache parity | [stub](superpowers/plans/2026-04-14-cluster-i-metadatacache-parity-STUB.md) | Stub plan | Expand after A lands and C is in flight |
| J | Embed / rendering primitives | [stub](superpowers/plans/2026-04-14-cluster-j-embed-rendering-primitives-STUB.md) | Stub plan | Expand after E lands and I is in flight |
| K | Bases | [scouting](superpowers/plans/2026-04-14-cluster-k-bases-SCOUTING.md) | Scouting doc | Blocked — expand when Bases DSL extraction addendum lands (follow-up #3 below) |
| L | Properties panel | — | Deferred | Treat as normal implementation task when A/B/I/C are done |
| M | Internal-plugin feature audits (Graph, Canvas) | — | Deferred | Treat as two normal tasks; no cluster plan needed |
| N | Plugin-ready surfaces | — | Deferred | Builds incrementally on top of B + C; no upfront plan |

---

## In-flight work items

*(none — nothing currently in progress)*

When work begins, each in-flight cluster gets a row here:

```
### Cluster X — <title>
- **Phase:** N of M
- **Last completed step:** <one line, with date>
- **Next expected step:** <one line>
- **Owner:** <human / agent name>
- **Date last touched:** YYYY-MM-DD
- **Open sub-questions:** <list, or "none">
```

Move the row to "Recent decisions" or a cluster retro on completion.

---

## Recent decisions

Append-only. Most recent on top. Archive entries older than ~3 months to `docs/decisions-archive.md` (quarterly).

- **2026-04-14 — Cluster H full plan written; Clusters G and K get scouting docs.** H is parallelisable with A/B/D and its prior-art targets (KDevelop hover-tooltips, KDevelop completion popup, KMessageWidget) are ripe for exploration now. G and K defer to full plans because G depends on C Phase 1 signatures and K depends on the Bases DSL extraction. Scouting docs capture prior-art breadcrumbs + architectural questions + rough phasing so full-plan expansion is ~90–240 min instead of green-field. A new convention lands: `*-SCOUTING.md` filenames for "pre-plan notes not ready to dispatch."
- **2026-04-14 — Long-term-state machine adopted.** Standing up `PROJECT-STATE.md` + `CONTRIBUTING-OPS.md` + `plans/INDEX.md` + `obsidian-audit/addenda/` as the four-file persistence system. Reason: audit produced ~94k words of reference; we need a stable cursor to navigate it across sessions. CLAUDE.md is the single entry point. See `docs/CONTRIBUTING-OPS.md` for rituals.
- **2026-04-14 — Cluster F/I/J kept as stub plans.** Won't expand until their dependencies (A–E) are at least in flight. Reason: full plans written now would be re-edited once A–E reveal Corbomite-side surface details.
- **2026-04-14 — Local KDE source convention adopted.** All cluster plans require agents to grep `~/src/kde/src/<repo>` and forbid cloning from `invent.kde.org`. Reason: every KDE repo we cited is present locally; cloning wastes time and risks version drift.
- **2026-04-14 — Cluster C and plugin moved to Wave 3 of the audit.** Originally Wave 1; pushed last so they could cite completed sibling docs. Reason: aggregator domains produce sharper docs against finished consumers. Worked.
- **2026-04-14 — Pass 1 corrections applied silently in Pass 3 synthesis.** Four corrections: `QueryController` is Bases (not search); `App.prototype.on` is no-op (events fire on `app.workspace`); `HoverPopover` hover delay is 300ms (poll interval is 500ms); `PopoverState.js` was mis-extracted (enum reconstructed from use-sites). Synthesis docs reflect corrected facts; Pass 1 file is left as-is for historical record.

---

## Open questions blocking progress

Questions that need human input before the linked work can proceed.

- **None right now.** Roadmap items A and B are unblocked and ready to dispatch.

When questions arise, format:
```
### <one-line question>
- **Blocks:** <cluster ID + phase, or "general planning">
- **Context:** <one paragraph — why we need this answer>
- **Asked:** YYYY-MM-DD by <session/agent>
- **Resolved:** YYYY-MM-DD with answer "<one-line answer>" (then move to Recent decisions)
```

---

## Controller-side follow-ups (out-of-tree extractions)

Outstanding items the audit flagged as needing `_internal.js` grep work outside the audited `obsidian/` tree. None block A–E; F/G/H/K/L progress varies with these.

1. **DOMPurify `SL` allowlist.** Security-critical for plugin HTML sanitisation. Grep `_internal.js` for `ALLOWED_TAGS` / `ALLOWED_ATTR`. *Blocks: full plan for H.*
2. **Turndown `hP` rule set.** Web-clipper paste-from-browser compat. Grep `_internal.js` for `new TurndownService` / `.addRule(`. *Blocks: nothing critical; nice-to-have.*
3. **Bases `DK`/`RK`/`JK`/`PX` formula/filter DSL parser.** Grammar lives outside `bases/`. Authoritative user-facing grammar at `help.obsidian.md/bases/functions`. *Blocks: full plan for K.*
4. **25 unnamed internal-plugin manifest IDs.** Extract from `core/App.js:611-641`. *Blocks: completing FEATURE-MATRIX.md feature catalog.*
5. **`AC` (appearance-keys allow-list) + `PC` (vault-config defaults).** *Blocks: `.obsidian/app.json` + `appearance.json` round-trip in Cluster B.*
6. **Search-panel DSL grammar.** Source is in the out-of-tree `global-search` internal plugin. Reverse-engineer from Obsidian user docs + grep `openGlobalSearch("..."` call sites. *Blocks: Cluster D Phase 4.*

---

## Pointers

- **Audit reference (canonical):** `docs/obsidian-audit/` — taxonomy + 15 domain docs + 5 synthesis docs + 2 running lists. Treat as read-only except for *additions* via `addenda/`.
- **Cluster plans:** `docs/superpowers/plans/` — one file per cluster, named `2026-MM-DD-cluster-<letter>-<title>.md`. Index at `docs/superpowers/plans/INDEX.md`.
- **Rituals (how to update this file and others):** `docs/CONTRIBUTING-OPS.md`.
- **Cluster retrospectives:** `docs/cluster-retros/cluster-<letter>.md` — written when a cluster lands fully.
