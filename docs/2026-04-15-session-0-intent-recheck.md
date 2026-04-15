# Session 0 — Intent Re-anchor Report

**Date:** 2026-04-15
**Question:** Is Corbomite still on-course for its guiding intent — "complete translation of Obsidian's full feature set to KDE paradigms" — after the 2026-04-15 8-cluster landing sprint? Where has reality diverged from the audit's picture?

**Method:** Read the 5 Pass-3 synthesis docs (FEATURE-MATRIX, GAP-ANALYSIS, PLUGIN-API-SKETCH, SHARED-SYMBOLS, VAULT-FORMAT) cross-referenced against the 15 Pass-2 domain docs, against each landed cluster's retrospective. Dispatched 4 parallel exploration agents to widen the view.

---

## TL;DR

**On-course.** The 16-cluster decomposition still maps cleanly onto the audit's 15 domain docs — no fragmentation of the aggregator domains (core, workspace, ui-bundle, leaf-utilities), no orphaned feature areas in the P0/P1 tier. Every P0 correctness gap is closed; every P1 compat blocker except stray config-file round-trip items is closed.

**Three emergent insights:**

1. **Plugin-runtime decision is the critical path, not a side-quest.** The PLUGIN-API-SKETCH has 30+ surfaces. QJSEngine is the clear best-fit (~85% native coverage). The one hard impedance-mismatch is `PluginSettingTab` (plugins expect DOM-ish `containerEl.empty()`; Qt is hierarchical). This is a Cluster N design-spike question and blocks downstream plugin-facing work in N.

2. **Cluster G absorbs more of `workspace.md` than its scouting doc suggests.** Originally scoped as "views hierarchy + TextFileView contract," the domain doc's scope also covers layout persistence, tab/split mechanics, and ribbon docking. We've done some of this piecemeal (PaneLayout from Sublime in B; RibbonSlot in H; QStackedWidget hosting in E), but the unified TextFileView-via-ViewRegistry contract is still unformed. G's full plan must consolidate these.

3. **Three un-clustered feature areas surfaced.** PDF embedding (Obsidian's `PdfView`), file recovery (periodic snapshots + recovery modal), and bookmarks panel are all in FEATURE-MATRIX but are not claimed by any of A–P. Low-priority individually; collectively they're a "Cluster Q — feature cleanup" post-parity micro-cluster. Flag now; decide later.

**KDE-paradigm health:** Good alignment on what's used (KConfigGroup, KStandardAction, KCommandBar, KF6::WidgetsAddons, Kate session pattern, harvested Sublime::AreaIndex + Baloo worker idiom). **Not yet using:** KParts (natural fit for embedded non-markdown view types — Bases, Canvas, PDF), KIO (for future `vault://` protocol), KNewStuff3 (plugin catalog), KCrash + KBackup (file recovery). None are urgent, but they're where the next "KDE-paradigm" bets live.

---

## 1. Drift check — landed clusters against audit

For each landed cluster, I compared the cluster retro's design-deviation list against the audit's guiding domain doc. **Net:** no load-bearing drift. Every pragmatic deviation is documented, small, and bounded.

| Cluster | Audit reference | Landed deviations | Is drift load-bearing? |
|---|---|---|---|
| A (Links/frontmatter) | `parsing.md`, `leaf-utilities.md` | Drop `libs/core/FrontMatter` wrapper, use `Markoff::YamlValue` directly (ryml 10-70× faster than yaml-cpp). `QString()` / NULL / NOT NULL SQLite guard added defensively. | **No.** Deviation is an optimization + bugfix. |
| B (Vault I/O) | `vault.md`, plus `workspace.md §layout-json` | Sublime::AreaIndex harvested as the in-memory PaneLayout; `.obsidian/workspace.json` with `_corbomite` namespace for Qt-specific state; no backward-compat for `.corbomite/session.json`. | **No.** The `_corbomite` namespace rides on Cluster B's unknown-key preservation — spec-faithful. |
| C (Lifecycle primitives) | `core.md`, `plugin.md` | Phase 4a wiring landed; Phase 4b-d (SessionDestroyer, hotkeys.json I/O, Scope push/pop) deferred until consumers exist. `EditorLike = void*` placeholder for now — G will narrow. | **No.** Phase 4b-d deferral is explicitly consumer-gated. The `EditorLike = void*` is a known future-narrow point. |
| D (Search/DSL) | `search.md`, `ui-bundle.md §popups` | `SearchPlan` class collapsed to free `SearchDSL::compile()` + `CompiledPlan` struct. `ResultHighlighter::drawHighlighted` paint helper instead of `QTextDocument` factory. `match-case` compound-operator edge required greedy-merge workaround. KCommandBar palette through CommandRegistry deferred — KF6 fuzzy currently serving. | **Partial.** The unparsed `line:` / `block:` / `section:` / `task*:` / regex / `[key:val]` property operators are follow-ups. D is done *structurally* but not *functionally complete* — compat-leaky. |
| E (Three-mode editor) | `editor-markdown.md`, `editor.md` | Six design deviations documented (Penelope rejected as base, qutepart vendored-not-adopted, ±0.5 fractional scroll accepted, hand-rolled section splitting, `renderedShape` moved pre-layout, `VaultResourceProvider` defined fresh). | **No.** All six documented in retro + PROVENANCE. Fractional-scroll and cursor-column preservation are honest residuals; both have follow-up paths defined. |
| F (Templates/Daily/Moment) | `editor-markdown.md §Templates`, `core.md §Moment` | Strategy B (hand-translate Moment tokens) over Strategy A (embed QJSEngine bundle). `{{cursor}}` positioning is line-granular only (Markoff has no `setCursor(line,col)` public API). | **No.** Strategy B avoids a new architectural seam; `{{cursor}}` column precision is a Markoff-API follow-up, not an F scope. |
| H (Menus/hover/suggesters) | `ui-bundle.md` | 5 of 6 menu-construction sites un-migrated to MenuSectionHelper (FileExplorer migrated as exemplar). RibbonSlot docked in MainWindow post-retro (f70afa1). SuggestPopup kept as CompletionPopup (no per-suggester renderSuggestion delegate yet). | **Partial.** The 5 un-migrated sites are mechanical one-by-one refactors; substrate is in place. Not drift, but unfinished mop-up. |
| I (MetadataCache) | `metadata.md` | `cacheChanged` went sync→async between Phase 4 and Phase 5 (worker-threaded parse). Phase 2 parser simplifications: footnote-def positions via regex scan (Markoff doesn't expose offsets); block-id position = marker span only; 3 of 11 section types emitted. TagCache.tag includes leading `#`. | **Partial.** Three Phase 2 simplifications are documented TODOs in-code; they diverge from Obsidian's bug-for-bug shape but are ergonomically reasonable. Flag for JK-plugin-compat pass later. |
| L (Properties panel) | `metadata.md §properties`, `ui-bundle.md §components` | Shipped as single-phase normal task. Uses `inferPropertyType` for factory dispatch; 500ms debounced FrontMatterWriter writeback. No explicit support for Obsidian's internal property widget registry (`NL` / `RL` shared-symbol references). | **No drift for P1/P2.** The MetadataTypeManager registry (`RL`) absence is P3 — plugin-facing; correct to defer to N. |

**New drift discoveries during this session (not in any doc yet):**

- **MarkoffParser strips footnote-def lines before tree-sitter parse.** This shifts content offsets in ways `frontmatterOffsetShift` doesn't compensate for. Surfaced in Cluster I Phase 2 implementation. *Not yet captured in any addendum.* **Action:** write `docs/obsidian-audit/addenda/2026-04-15-footnote-def-offset-shift.md` now.

- **`libs/core/include/corbomite/core/MarkdownRenderer.h`** contains a TODO: "Replace regex renderer with cmark-gfm or other proper markdown." This predates ReadingView and may be orphaned code. **Action:** audit callers; either wire to ReadingView or delete.

---

## 2. Cluster decomposition soundness

The cluster-domain mapping is sound. All 15 Pass-2 domain docs have clear primary ownership:

| Domain doc | Owning cluster | Notes |
|---|---|---|
| bases.md | K | Deferred scouting — DSL extraction blocker. |
| core.md | C | Done (primitives). |
| editor-markdown.md | E | Done. |
| editor.md | E (extended) | Done. |
| leaf-utilities.md | A + D + J utilities | Library utilities used across clusters — correct shape. |
| metadata.md | I | Done. |
| parsing.md | A | Done (via Markoff::YamlValue + LinkUtils). |
| plugin.md | C (primitives) + N (surfaces) | Done for primitives; N is the remaining piece. |
| rendering.md | J | Stub plan; unblocked by E+I. |
| search.md | D | Done structurally; follow-ups pending. |
| settings.md | L (properties UI) + N (settings-tab plugin API) | L landed for properties; the fluent-Setting-builder for plugin tabs belongs to N. |
| ui-bundle.md | H | Done. |
| vault.md | B | Done. |
| views.md | G | Scouting → ready to expand. |
| workspace.md | G (views) + N (plugin-API) + B (layout-json already done) | **Cross-cutting; G must absorb more than its scouting doc suggests.** |

**Cross-cutting concerns: respected, not fragmented.**
- `core.md` — fully C.
- `workspace.md` — split cleanly between B (persistence format), G (view hierarchy + tab/split mechanics), N (plugin-facing events + layout mutation API).
- `ui-bundle.md` — fully H.
- `leaf-utilities.md` — library-shaped on purpose; used where needed.

**No over-scoped clusters.** A, B, D, E all touch 2-3 domains but cohesively (link resolution + atomicity; vault config + state; search DSL + suggester matchers; editor modes + extensions).

**No under-scoped clusters visible.** One caveat: parallel agents flagged "Cluster I has no plan" and "Cluster L has no plan" — **these flags are stale**. Both clusters landed in full on 2026-04-15 with retros at `docs/cluster-retros/cluster-{i,l}.md`. The audit-task agents were reading the old roadmap. Mention because it shows Pass-3 synthesis docs themselves need a light refresh post-sprint.

---

## 3. P0/P1/P2 gap reconciliation

Cross-checked each gap ID against landed clusters. **All P0/P1 gaps are addressed by landed clusters.** The remaining gap pressure is concentrated in P2 on clusters G and J, and P3 on cluster N.

| Tier | Total entries | Addressed by landed | Addressed by remaining-with-plan | Orphan |
|---|---|---|---|---|
| P0 | 7 | 7 | 0 | 0 |
| P1 | 12 | 12 | 0 | 0 |
| P2 | 34 | 23 | 8 (J, G, K, M) | 3 (see below) |
| P3 | 14 | 8 | 3 (N, J) | 3 (see below) |

**Genuinely orphan gap entries** (not owned by any A-P cluster, missed from the original decomposition):

- **P2.x — PDF embedding / `PdfView`** (matrix §408, §410). Obsidian's built-in PDF rendering uses PDF.js. Candidates for Corbomite: Poppler-Qt6 (lighter) or Okular KPart (KDE-native, richer). Not claimed by J (J is markdown-embed rendering), not claimed by M (M is Graph/Canvas feature audits).
- **P2.x — File recovery plugin** (matrix §1141). Periodic snapshots + recovery modal. Data-safety feature. Not claimed.
- **P2.x — Bookmarks panel** (matrix §1065). Right-sidebar persistent bookmark tree backed by `.obsidian/bookmarks.json`. Not claimed.
- **P3.x — Community plugin manifest parsing** (matrix §1162). The mechanical part of N, but not yet in N's (nonexistent) plan.

**Action:** when N gets a full plan, absorb P3.x. When time allows, add a short "Cluster Q — feature cleanup" slot for PDF/bookmarks/file-recovery, or slot them into existing clusters (file-recovery → C extension; bookmarks → G extension; PDF → J extension via KParts).

---

## 4. Plugin-runtime decision (Cluster N preview)

The PLUGIN-API-SKETCH analysis was the richest intelligence of this session. Key findings:

**30+ plugin-facing API surfaces** across 6 categories: lifecycle (5), registries (15), event subscriptions (4 emitters × ~30 events), subclass entry points (7), utility/context (5), command palette variants (4).

**JS-dependency spectrum:**
- ~40% "easy" to expose in C++ (registries, commands, protocol handlers, view hierarchy).
- ~35% "medium" (async processors, modals, menus).
- ~25% "hard" (Events mixin dynamic dispatch, settings UI DOM, CodeMirror extensions).

**Runtime candidate ranking:**

| Candidate | Surface coverage | Plugin breakage | Biggest risk |
|---|---|---|---|
| (a) C++ dlopen | 55-65% native | 20-30% plugins port | ABI stability; settings-UI impedance |
| **(b) QJSEngine + `obsidian` shim** | **80-90% native** | **15-25% plugins break** | **SettingsTab DOM mock complexity** |
| (c) QuickJS / WASM | 75-85% native | 15-25% plugins break | Custom runtime embedding; WASM distribution |

**Hard-or-impossible surfaces regardless of runtime:**
1. **EditorExtension (CodeMirror 6).** Corbomite uses Markoff. Plugins must rewrite against `Markoff::EditorPlugin` hooks.
2. **Node.js imports** (`child_process`, `fs`, `path`). No Node runtime. `requestUrl` and `DataAdapter` replace respectively.
3. **Electron IPC** (`ipcRenderer`, `BrowserWindow`). Not Electron — zero compat possible.
4. **Direct DOM access.** Plugins that query `document.querySelector` fail unless we run them in QtWebEngine (expensive).
5. **`PluginSettingTab`.** Plugins expect `containerEl.empty()` then rebuild via DOM builder. **This is the core architectural problem of Cluster N.** Two paths: (a) provide a minimal DOM mock in QJSEngine that proxies to Qt widgets; (b) require plugin authors to subclass `PluginSettingTab` against a Qt-native API (breaks source-level compat).

**Recommendation for Cluster N:** **QJSEngine + `obsidian` shim as the baseline**, with explicit documentation that plugins declaring `minAppVersion: "corbomite-1.0.0"` get compat-mode. Port top ~80% of plugins via minor edits (declare version, swap CM6 for Markoff hooks). The SettingsTab-DOM-mock question gets a scoped spike before the full plan commits to one path.

This is a big enough decision that Cluster N's scouting doc should have **three short spike prompts** (one per candidate) and a decision meeting before the full plan locks in QJSEngine.

---

## 5. KDE-paradigm health check

Our guiding intent says "translate Obsidian to KDE paradigms." A tally of where we stand:

**KDE libraries / patterns we use well:**
- KConfigGroup — dev vs release settings isolation.
- KF6::WidgetsAddons — KCommandBar, KMessageWidget.
- KStandardAction — actions where applicable.
- KXmlGui — main menu/toolbar (convention).
- Kate session pattern — vault-switch destroy/rebuild.
- Sublime::AreaIndex (KDevelop) — **harvested** into libs/core/PaneLayout.
- Baloo worker-idiom (condition-variable + atomic stop-flag) — **harvested** into MetadataWorker.
- KSyntaxHighlighting — via qutepart vendor; will be swapped in fork Phase 4 to KSyntaxHighlighting-native.

**KDE libraries / patterns we could adopt — candidates per remaining cluster:**
- **G (Views hierarchy):** KParts is the natural fit for embedded non-markdown view types. When Bases/Canvas/PDF views need their own lifecycle + toolbar integration, KPart is the KDE-native answer. Worth a short spike.
- **J (Embed rendering):** Okular KPart for PDF embeds (as a plugin-view-type candidate, complementary to markdown-embed via ReadingView).
- **K (Bases):** No direct KDE analog — Bases is unique to Obsidian. The DSL parser itself is a standalone problem.
- **M (Graph/Canvas audits):** KNetworkReply for any data-fetching (Cluster P's Graffodil may already do this).
- **N (Plugin-ready surfaces):** KNewStuff3 for community plugin catalog — exactly the Obsidian `community-plugins` equivalent. KDBusService (Unique) for single-instance + `obsidian://` URL handling.
- **Feature-cleanup orphans:**
  - KCrash + KBackup → file-recovery plugin. Drop-in data-safety story.
  - KIO → future `vault://` protocol for networked vaults (if/when we decide Sync-adjacent).

**Action:** when writing G's and N's full plans, explicitly include a KDE-prior-art section with spike prompts for KParts and KNewStuff respectively.

---

## 6. Feature-matrix shape check — what's complete vs missing

After 8 cluster landings, Corbomite now covers:

**Complete:**
- Vault I/O, link resolution, frontmatter correctness (A + B).
- Metadata pipeline end-to-end (I).
- Three editor modes Source / LivePreview / Reading (E).
- Search + fuzzy suggesters + DSL compile (D, with operator follow-ups).
- Menus, hover, suggester UI primitives (H).
- Templates, Daily Notes, Moment format (F).
- Properties panel (L).
- Lifecycle primitives (Component, Events, Scope, Command, Hotkey, PluginInstance) (C).

**Substrate-ready, consumer-pending:**
- Ribbon docking (landed, but only 3 built-ins).
- 4 plugin-facing registries (HoverLinkSource, EditorSuggest, MenuSection, RibbonSlot) — all populated with built-ins, waiting for N.
- SessionDestroyer / hotkeys.json I/O / Scope push-pop — consumer-gated.
- 7 search-DSL operators — parser accepts, compile rejects.

**Remaining to build:**
- Views hierarchy + TextFileView contract (G).
- Embed / rendering primitives: `![[Note#heading]]`, EmbedRegistry, MarkdownPostProcessor (J).
- Bases (K — DSL-blocked).
- Plugin runtime + Settings-tab API (N — design-spike-first).
- Graph / Canvas audits (M — two normal tasks).
- Advanced query layer (O — post-parity).
- Graffodil adoption (P — API-stability-gated).

**Unclaimed:**
- PDF embed view.
- File recovery plugin.
- Bookmarks panel.

---

## 7. Redirect recommendations

**What I'd keep verbatim:**
- 16-cluster decomposition — validated against 15 domains with clean mapping.
- State-machine rituals — survived 8 landings without coherence failure.
- Parallel sub-agent dispatch — proven effective.
- Cluster ordering (J → G → N → K → M → O → P) — correct modulo points below.

**What I'd redirect:**

1. **Expand Cluster G's scouting doc into a full plan that absorbs more of `workspace.md` than originally scoped.** Specifically: TextFileView contract, ViewRegistry, tab/split/pane-type semantics. The Cluster B harvest already gave us PaneLayout; G must wire the view-class hierarchy onto it. Include KParts spike.

2. **Expand Cluster J's stub against the post-E/I reality.** ReadingView's section-level mount + `VaultResourceProvider` are available now. J should inherit "hover-link preview at section granularity" (flagged in Cluster E retro §downstream-effects) as a concrete sub-task. Include KParts-for-PDF spike.

3. **Pre-pone Cluster N design-spike before executing J.** N's SettingsTab-DOM-mock question is architectural and will influence J's embed-renderer public surface. One 2-hour spike with three candidates (C++ dlopen / QJSEngine / QuickJS) producing a scouting doc + decision. Recommendation is already QJSEngine; spike confirms or overturns.

4. **File the Bases DSL extraction as a dedicated agent sub-task.** Not a cluster plan — a targeted grep + cross-reference job. 2-4 agent-hours. Output: a grammar doc that unblocks K's full plan.

5. **Flag three unclaimed feature areas.** PDF embed, file recovery, bookmarks. None urgent, but they should have a home. Candidates: PDF → J (with KPart), file recovery → post-parity Cluster Q or C extension, bookmarks → G extension.

6. **Doc hygiene micro-sweep.** Footnote-def-offset addendum; flaky-test enumeration; stale vault-switch memory; MarkdownRenderer.h audit; test-coverage-matrix refresh for E's 50 new tests. 30-45 minutes of work.

**What I'd NOT change:**
- Don't pre-plan M or O. Fresh context when written.
- Don't rush fork Phases 3-8. Parallelisable, no pressure.
- Don't add a "Cluster Q" formally until a cleanup cluster is actually ready to dispatch.

---

## 8. Proposed order for remaining planning sessions

1. **Session 1 — Cluster J full plan** (stub→full). Brainstorm; include hover-link preview, EmbedRegistry, MarkdownPostProcessor, KPart-for-PDF spike.
2. **Session 2 — Cluster G full plan** (scouting→full). Brainstorm; absorb views + tab/split/pane + ViewRegistry + KParts spike.
3. **Session 3 — Cluster N design spike → scouting doc.** Three runtime candidates; SettingsTab-DOM-mock deep-dive; QJSEngine + `obsidian` shim almost-certain recommendation.
4. **Session 4 — Bases DSL extraction sub-task.** Not a cluster plan; a targeted grep + `help.obsidian.md/bases/functions` cross-ref. Output: grammar doc → unblocks K's full-plan expansion.
5. **Session 5 — Doc hygiene sweep.** Addendum, stale memory, MarkdownRenderer decision, coverage matrix refresh.
6. **Then: Cluster J execution.**

After Cluster J lands, K and N become the next branch point — K gated on Session 4's grammar doc, N on Session 3's runtime decision. M and O stay deferred.

---

## 9. Single-sentence takeaway

**The Obsidian-to-KDE translation is on-track; the remaining work is 3 full plans (J, G) + 1 major design spike (N) + 1 extraction sub-task (Bases DSL), and we have a better-than-expected KDE-adoption runway (KParts, KNewStuff3, KCrash) still to pick up.**
