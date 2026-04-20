# Program State Report — 2026-04-15

> **One-off retrospective.** Zoom-out across the cluster roadmap after an unprecedented multi-cluster landing session on 2026-04-15. Complements PROJECT-STATE.md (which is forward-looking operational cursor) by stepping back to ask: given what we now know, do we need to redirect planning? What residuals / surprises / hurdles remain? Is the docs/code state-machine still coherent?

Scope: read all living state docs + 5 cluster retros + code structure verification + gap/addenda/test-coverage scans.

---

## 1. Executive summary

The project is healthy. On 2026-04-15 alone, **six clusters landed** (D, E, F, H, I, L) plus qutepart-corbomite fork Phases 1+2 as a parallel internal refactor. Roughly 40+ commits, one calendar day. Eight of the sixteen roadmap clusters are now **Done** (A, B, C, D, E, F, H, I, L); one is partial (C — primitives done, Phase 4b-d deferred until consumers demand); **seven remain** (G, J, K, M, N, O, P).

Codebase-vs-docs alignment verified end-to-end: every artifact the PROJECT-STATE and retros claim exists actually exists at the claimed path. No drift between prose and reality.

The biggest emergent picture: **the Obsidian-parity substrate is essentially complete** (metadata cache, vault I/O, lifecycle primitives, three-mode editor, frontmatter correctness, search DSL, templates/daily-notes, properties panel, menus/hover/suggesters). Remaining clusters fall into three buckets: user-facing pull (J, K, L-already-done), deferred-until-consumer (M, N), and post-parity (O, P). The "shape" of remaining work has shifted from infrastructure to surfaces.

Major hurdles remaining are concentrated in three places: **(1) out-of-tree extractions** (6 still open — they bottleneck K in particular), **(2) plugin surfaces** (N has no upfront plan; it's the "plugin loader that isn't JS" problem nobody's designed yet), and **(3) coverage depth** (test matrix L3/L4/L7/L8 lifecycle dimensions are largely blank).

Recommendation: **pause and re-triage before starting Cluster J.** The pre-2026-04-15 plan assumed sequential A-through-E work with careful inter-cluster handoffs. The observed velocity was 6× faster, and several stub/scouting plans (G, J, K) were written before their upstream context existed. Before executing stubs as-written, one cluster-replanning session could save a round of rewrite-mid-implementation.

---

## 2. Where each cluster actually brought us

### Landed (8 clusters + 2 fork phases)

| Cluster | Landed | Artifacts shipped | Emergent takeaway |
|---|---|---|---|
| A — Link / frontmatter correctness | 2026-04-14 | `LinkUtils` + `LinkResolver` (6-step shortest-path + `links.subpath` PK column) + `FrontMatterWriter` (QSaveFile atomic `processFrontMatter`). | Keystone held: no downstream cluster has reopened A's contracts. `QString()` / NULL / NOT NULL SQLite gotcha caught in integration — keep the defensive `.isNull() ? ""` guard. |
| B — Vault I/O | 2026-04-14 | `DataAdapter` + `FileSystemAdapter` + `VaultConfig` + `WorkspaceState` + `CaseSensitivityProbe` + `VaultTrash` + `IgnoreFilter` + `VaultProcess` + (Phase 3b) `PaneLayout` harvested from KDevelop Sublime. | Unknown-key preservation is the load-bearing contract other clusters lean on (E's `eState` rode it without schema change). Decision to write only cache to `.corbomite/` and everything else to `.obsidian/` has held. |
| C — Lifecycle / plugin primitives | 2026-04-15 (partial) | `Component`, `Events`, `Scope` + `ScopeManager`, `Command` + `CommandRegistry`, `Hotkey` + `HotkeyFile`, `PluginInstance` stub. Phase 4a app wiring landed. | The vault-switching-crash memory turned out stale — fix already landed pre-Cluster-C via `MainWindow::onVaultClosed::closeAllDocuments`. Phase 4 `SessionDestroyer` downgraded from urgent-bugfix to forward-looking-refactor. |
| D — Search / suggester parity | 2026-04-15 | `SearchDSL::compile()` free-function + `CompiledPlan`, `ResultHighlighter::drawHighlighted`, `FuzzyMatcher`, `SQLiteIndex::searchCompiled`. | "Mid-word retry" mis-reading of the audit cost one test round. `match-case` compound-operator tokenizer edge case was the real surprise — the hyphen rule required a greedy-merge workaround. 7 follow-ups deferred (see §5). |
| E — Three-mode pivot | 2026-04-15 | `libs/qutepart-corbomite/` vendored fork, `libs/readingview/` greenfield, `src/editor/SourceEditor` shim, `NoteEditorWidget` as `QStackedWidget`. 18 commits, 7 phases + 2 fork-prereq phases. | 6 design deviations from the plan (Penelope rejected as base, qutepart vendored-not-adopted, ±0.5 fractional scroll accepted, section-splitting hand-rolled, renderedShape moved pre-layout, `VaultResourceProvider` defined fresh). All reviewed and retained. |
| F — Templates / Daily Notes / Moment | 2026-04-15 | `MomentFormatter` + `VaultConfig` typed accessors + `TemplateService` + `DailyNoteService` + `MomentFormatPreview`. | Moment's tokenised-English-letters gotcha (`"hello"` → `"2ello"` because `h` is a token) worth memorising. Parallel P3/P4 dispatch produced a MainWindow race — handled, but noted pattern to avoid. |
| H — Menus / hover / suggester UI | 2026-04-15 | `HoverPopover` + `HoverLinkSourceRegistry` + `RibbonSlot` (docked in MainWindow as of f70afa1) + `MenuSectionHelper` + `MenuEventEmitter` + `EditorSuggest` + `EditorSuggestManager` + `CompletionPopup` + `Notice`. | FileExplorerPanel migrated to MenuSectionHelper as exemplar; **5 other call-sites remain un-migrated** (EditorViewSpace tab bar, CanvasScene, Markoff Editor, TextControl, CorbomiteMDI Sidebar). SuggestPopup kept as CompletionPopup (deferred proper delegate). |
| I — MetadataCache parity | 2026-04-15 | `CachedMetadata` + `MetadataParser` + `MetadataCache` + `MetadataWorker` + `CachedMetadataStore`; SQLiteIndex refactored to subscribe to `cacheChanged` instead of owning indexing. | Sync-vs-async signal boundary was the hidden reef; Phase 4 had to rename a test after Phase 5 moved parse to a worker thread. Two BUG IDs filed + fixed during Cycle 1 test enrichment (BUG-20260415-000 schema-bump links drop, BUG-20260415-001 stale reap). |
| L — Properties panel | 2026-04-15 | 6 editor widget types (Text / Number / Checkbox / Date / DateTime / List) + factory dispatch via `inferPropertyType` + 500ms debounced `FrontMatterWriter` writeback + reactive `cacheChanged` subscription. | Shipped as a "normal task" without a cluster plan. Validates that the Cluster A + I substrate was strong enough for a downstream feature to land in one phase. |
| Fork Phases 1 + 2 | 2026-04-15 | Vendored qutepart-cpp at `eec2e9a` into `libs/qutepart-corbomite/` with SPDX dual-headers on 66 files + `Corbomite::SourceEditor` shim + visual-line float scroll accessors. | Ran in parallel with Cluster E as documented. Phase 2 extended the vendored `Qutepart` public API (logged in `PROVENANCE.md`). |

### Not landed (7 clusters + fork Phases 3-8)

| Cluster | Plan state | Status | What's holding |
|---|---|---|---|
| G — Views hierarchy / TextFileView contract | Scouting doc | Ready to expand → full plan | C Phase 1 signatures now exist; the documented expansion trigger has fired. Has not been expanded. |
| J — Embed / rendering primitives | Stub plan | Ready to expand → full plan | E + I both done (the stub's unblocker conditions). Has not been expanded. |
| K — Bases | Scouting doc | **Blocked** | Requires Bases DSL grammar extraction from Obsidian `_internal.js` (controller-side follow-up #3). No progress on extraction. |
| M — Internal-plugin feature audits (Graph, Canvas) | None | Deferred as two normal tasks | Audit explicitly said no cluster plan needed; can dispatch anytime. |
| N — Plugin-ready surfaces | None | Deferred, builds on B + C | No upfront plan — the "non-JS plugin loader" shape isn't decided. **This is the biggest design unknown on the roadmap.** |
| O — Advanced query layer (post-parity) | Scouting doc | Post-parity hold | Expansion trigger is "A/B/I/K landed + user demand visible" — A, B, I all done; K is the gate. |
| P — Graffodil adoption (internal refactor) | Scouting doc | Await Graffodil API stability | Parallelisable with parity roadmap; trigger was "2–3 wk Graffodil API stability observation" starting 2026-04-14 → still within that window. |
| Fork Phases 3–8 | Full plan | Phases 3-8 pending | Phase 3 (find/replace API) parallelisable now. Phases 4-8 asynchronous (KSyntaxHighlighting replacement, trim indent engines, bundled-themes removal, markdown-specific features, rebrand). |

---

## 3. Codebase-vs-docs alignment check

Spot-checked every artifact the PROJECT-STATE claims across 8 landed clusters. **All claimed artifacts present.** No drift.

Specifically verified:
- `libs/core/` contains LinkUtils, FrontMatterWriter, MomentFormatter, Component, Events, Scope, Command, Hotkey, PluginInstance, PaneLayout, HoverPopover, MenuSectionHelper, MenuEventEmitter, EditorSuggest, EditorSuggestManager.
- `libs/storage/` contains LinkResolver, DataAdapter, FileSystemAdapter, VaultConfig, WorkspaceState, CaseSensitivityProbe, VaultTrash, IgnoreFilter, VaultProcess, CachedMetadata, MetadataParser, MetadataCache, MetadataWorker, CachedMetadataStore, SQLiteIndex (refactored).
- `libs/search/` — SearchDSL, ResultHighlighter, FuzzyMatcher.
- `libs/models/` — TemplateService, DailyNoteService, PropertyType, PropertyTypeInference.
- `libs/qutepart-corbomite/` and `libs/readingview/` — both present; readingview contains ReadingPipeline, ReadingSection, SectionLayout, SectionRecyclePool, ReadingParseWorker, VirtualScrollController.
- `src/editor/` — SourceEditor shim + NoteEditorWidget QStackedWidget host.
- `src/sidebar/` — PropertiesPanel + PropertyEditorWidget.
- `src/dialogs/MomentFormatPreview.h`.

Git log verified: recent 40 commits match phase-by-phase landings described in retros. Commit message convention (`<type>(<area>): <subject>\n\nCluster X phase N: <context>`) is followed consistently.

Conclusion: the state machine (CLAUDE.md → PROJECT-STATE.md → CONTRIBUTING-OPS.md → plans/INDEX.md + cluster-retros + audit) is coherent. The only minor inconsistency is **test flakiness count** — PROJECT-STATE says "5 known-flaky failures" in one place, retros say "4" elsewhere. Worth reconciling (or enumerating) next session.

---

## 4. Surprises — things we didn't see coming

Distilled from cluster retros, with commentary on whether they should affect planning.

1. **Obsidian-parity substrate completed ~6× faster than the plan's implicit pace assumption.** Original cluster pacing seems to have assumed ~1 cluster / 2-3 days. Actual: 6 clusters in one 2026-04-15 session (plus fork Phases 1+2). Parallel sub-agent dispatch + the state-machine rituals (which force status updates every phase) combined to eliminate nearly all coordination overhead. **Implication:** stub/scouting plans (G, J, K) should get a freshness check before execution — context they were written against may already be superseded.

2. **MarkoffParser's frontmatter-stripping shifts offsets in ways the audit didn't flag.** Cluster I Phase 2 discovered that footnote-def lines are also removed before tree-sitter parse — so content after them has offset shifts that `frontmatterOffsetShift` doesn't compensate for. Latent bug; no impact yet. Not in any addendum or gap list. **Action:** file this as a GAP-ANALYSIS addendum or `01-markoff-gaps.md` entry before it's forgotten.

3. **Penelope's StyleManager was NOT ThemeManager-coupled.** Cluster E plan warned the transplant would be messy. Reality: clean. **Implication:** when the plan flags "expected-messy integration" and it comes out clean, retain the forward-looking enum even when unused — E's Theme enum is doing exactly that.

4. **Obsidian's `HoverPopover` hover delay is 300ms, not 500ms** (audit Pass 1 caught this; Cluster H held the line). Several third-party plugin authors and Obsidian's own docs get this wrong. **Implication:** when in doubt, trust the audit's corrections list in §Pass-3-synthesis over published Obsidian docs.

5. **Fractional scroll quantization limit is Qt-imposed, not Obsidian-imposed.** `QPlainTextEdit::scrollContentsBy` rounds to integer visual-line (verified against `~/src/qtbase`). Accepting ±0.5 precision for now; true fix lives in fork Phase 4. **Implication:** don't promise perfect scroll fidelity in demos until fork Phase 4 lands.

6. **Markoff-fromMarkdown strips footnote defs before tree-sitter** — combined with #2, implies any future offset-sensitive consumer of Markoff AST must account for *two* coordinate-system shifts. **Action:** this belongs in Markoff's public API docs as a contract, not just a comment.

7. **`Q_DECLARE_METATYPE` on structs with `std::optional<QVector<T>>` + `QJsonObject` "just worked"** — no workarounds. **Implication:** design MetadataCache-shaped structs with full Obsidian-parity field shapes from the start; don't simplify under the assumption that Qt meta-system will balk.

8. **Clangd noise is ambient and not a signal.** Every sub-agent return produced stale `.moc missing` / `undeclared identifier` diagnostics; `cmake --build` always passed. **Implication:** this is worth promoting from retro-lesson to a CLAUDE.md note or a `.clangd` config tweak.

9. **Parallel-agent file-commit races are survivable.** Cluster F P3+P4 both touched MainWindow.cpp; P4 absorbed P3's pending edits into its commit. Zero code loss. **Implication:** the pattern works, but the attribution is misleading — better to commit P3 first or bundle the shared edits.

10. **The vault-switch-crash memory was stale.** Already fixed pre-Cluster-C. `project_vault_switching` auto-memory outdated. **Action:** update or retire that memory entry.

---

## 5. Residuals and leftovers

### Un-landed follow-ups from landed clusters

**Cluster C (3 follow-ups, all consumer-gated):** SessionDestroyer for per-vault lifecycle, `.obsidian/hotkeys.json` I/O, Modal/Menu Scope push/pop. All three remain because no consumer exists yet.

**Cluster D (7 follow-ups):**
1. `line:` / `block:` / `section:` / `task*:` operators (parser accepts; compile rejects).
2. `[key]` / `[key:val]` property-call — coordinated with Cluster I (now unblocked — *Cluster I's retro explicitly flagged this as "thawed"*).
3. Regex post-filter.
4. True `match-case` semantics.
5. KCommandBar palette through `CommandRegistry`.
6. Quick-Switcher mode-switching (`#` / `^` / `[[`) — belongs to Cluster H territory.
7. Snippet-text rich rendering in SearchResultsModel.

**Cluster H (6 follow-ups):** RibbonSlot docking **landed** post-retro (commit f70afa1); remaining are 5 menu-construction-site migrations, HoverPopover anchor on hovered-link rect, SuggestPopup dedicated widget, Multi-Notice stacking, plugin-facing wrappers (Cluster N territory).

**Cluster E (8 follow-ups):** Source↔LivePreview cursor-column preservation (needs Markoff::Editor API extension), Reading-mode scroll restore during setViewMode, Source-mode fractional scroll precision (fork Phase 4), gutter fold arrow UX, overlapping-sections-on-nested-headings cleanup, native-QPA 100k-line benchmark, shared `Corbomite::Core::VaultResourceProvider`, fork Phases 3-8.

**Cluster F (2 soft follow-ups):** MomentFormatter comparison harness vs Node.js moment.min.js for locale edge cases; SettingsDialog Templates tab.

### Controller-side out-of-tree extractions (all 6 still open)

1. DOMPurify `SL` allowlist (blocks H full plan — H landed anyway via KDE defaults; not a hard blocker in practice).
2. Turndown `hP` rule set (nice-to-have).
3. **Bases DSL grammar** (blocks K full-plan expansion — **this is the biggest single external dependency**).
4. 25 unnamed internal-plugin manifest IDs (blocks FEATURE-MATRIX.md completion).
5. `AC` / `PC` allow-lists (nominally blocked B — B landed anyway via unknown-key preservation).
6. Search-panel DSL grammar (blocks D Phase 4 — D landed anyway; the gap is the *panel-side* DSL, not the matcher).

Pattern: 4 of 6 were flagged as blockers for clusters that landed anyway. The extractions remain needed for **completeness / Obsidian bug-for-bug parity**, not for functional parity.

### Inline TODOs

102 TODO/FIXME/XXX across `libs/`. Five most load-bearing:
- `libs/storage/src/MetadataParser.cpp:478` — footnote position via regex; move to tree-sitter exposure.
- `libs/storage/src/SQLiteIndex.cpp:561` — direct UPDATE against links table (Cluster I follow-up).
- `libs/storage/src/MetadataCache.cpp:388` — embeds + frontmatterLinks resolution deferred.
- `libs/markoff/src/FoldGutter.cpp:70` — heading-fold gutter painting blocked on coordinator wiring.
- `libs/core/include/corbomite/core/MarkdownRenderer.h:12` — "Replace regex renderer with cmark-gfm or other proper markdown." **This is a separate legacy renderer from ReadingView** — check whether it still has callers; if not, delete.

### Test coverage gaps

From `docs/test-coverage-matrix.md`: L3 (external edits), L4 (schema bumps), L7 (external mutations), L8 (crash recovery) are largely blank across most seams. Cycle 1 added L5/L6 integration coverage; L4/L7/L8 remain. **Action:** schedule Cycle 2 after Cluster J lands.

---

## 6. Major hurdles remaining

Ranked by difficulty × blast radius.

1. **Cluster N (Plugin-ready surfaces).** No upfront plan; the audit didn't spec a non-JS plugin runtime. This is a *design* problem, not an implementation one. Options: (a) C++ dlopen-style with a stable ABI, (b) embed QJSEngine and ship the Obsidian `api.d.ts` shim surface, (c) something else entirely (Lua / Python / WASM). Each has major tradeoffs. Before N can even have a stub plan, a design spike is needed. **Recommendation:** hold a scoped brainstorming session before expanding N.

2. **Cluster K (Bases).** Blocked on Obsidian `_internal.js` DSL extraction (controller follow-up #3). Without that, K can't be a full plan. The grammar is substantial (formulas + filters, `DK`/`RK`/`JK`/`PX` functions). **Recommendation:** fund a focused extraction sub-task (maybe dispatch a dedicated agent against `_internal.js` + `help.obsidian.md/bases/functions`) rather than letting K languish.

3. **Cluster J (Embed / rendering primitives).** Stub plan, now unblocked (E + I done). Primary unknown: whether `![[Note#heading]]` renders via section-level ReadingView mount or a separate light path. The retro for E flagged "hover-link preview at section granularity" as newly-practical — J should inherit that. **Recommendation:** expand the stub to full plan as the next planning task.

4. **Cluster G (Views hierarchy).** Scouting doc; C Phase 1 is now done → expansion trigger fired. The contract between view registration, TextFileView, and the editor/reading-view swap is well-understood post-Cluster-E. **Recommendation:** expand to full plan; execution should be relatively cheap.

5. **Fork Phase 4 (KSyntaxHighlighting replacement).** The largest remaining fork effort. Ties to: fractional-scroll fidelity, source-mode performance, future theme unification. Parallelisable with parity work but large.

6. **Coverage L4/L7/L8.** Schema bump, external mutation, crash recovery — these are the lifecycles that bite in production but are cheap to skip in unit tests. Needs deliberate Cycle 2 scheduling.

7. **Moment.js locale fidelity.** Cluster F shipped English-first. German / Japanese / French ordinals, weekday abbreviations, day-of-year semantics — we're guessing. Comparison harness is cheap; deferred is fine, but flag as a known-gap.

8. **Plugin-facing wrappers for Cluster H registries.** HoverLinkSourceRegistry, EditorSuggestManager, RibbonSlot, MenuEventEmitter — all have built-ins registered, none have plugin-facing surfaces. This is really a sub-task of Cluster N.

---

## 7. Should we redirect planning?

### What I'd keep

- The state-machine ritual system has worked. PROJECT-STATE + CONTRIBUTING-OPS + plans/INDEX + cluster-retros + audit-as-read-only has survived 8 landings with zero coherence failures. Do not restructure.
- Parallel sub-agent dispatch for disjoint-directory work. Cluster I, E, F all used it.
- The "plan-written-during-a-prior-session, executed-in-a-later-session" cadence. Plans written 2026-04-14 landed 2026-04-15 without mid-execution rewrite pressure.
- The `*-SCOUTING.md` / `*-STUB.md` / full-plan three-tier naming.

### What I'd redirect

1. **Expand J and G's stubs/scouting docs to full plans now, before executing.** Both had their unblocker conditions fire on 2026-04-15. Their plans were written against assumed shapes of E and I; now that E and I are real, plan-freshness is worth a pass.

2. **Schedule a design spike for Cluster N.** Plugin surface shape is unknown. One 2-hour brainstorming session with three candidate architectures (dlopen, QJSEngine, WASM) would produce a scouting doc and unblock the full plan.

3. **Schedule a targeted extraction sub-task for Bases DSL.** The "controller-side follow-ups" section has had 6 items for months. Four of them turned out to be softer blockers than thought (H, D, B all landed without their nominal controller-side prerequisites). Only Bases DSL (K) looks like a hard one. Invest specifically there.

4. **Rename or enumerate the "known-flaky" tests.** Both numbers (4 and 5) appear in retros / PROJECT-STATE. Document which tests, why, and whether they're fixable. If some are environmental (offscreen QPA vs native), that's a note; if some are real flakes, file bugs.

5. **Write an addendum for the MarkoffParser footnote-offset-shift discovery** (§4 surprise #2). One paragraph in `docs/obsidian-audit/01-markoff-gaps.md` under a new `## Implementation additions — 2026-04` heading.

6. **Audit `libs/core/include/corbomite/core/MarkdownRenderer.h`** — if it's unused, delete it. If it's in use, the TODO about swapping in cmark-gfm is years old and worth a decision.

### What I'd NOT change

- Don't pre-write plans for M, N, O. They'll be fresher if written against the actual state-of-plugin-framework when that question is live.
- Don't rush Fork Phases 3-8. They're asynchronous and not blocking anything user-facing. Phase 3 (find/replace) can land when someone needs find/replace in Source mode.
- Don't restructure the cluster numbering. Letters A-P carry load-bearing cross-references in plans and addenda.

---

## 8. Minor docs maintenance to pick up

Small items surfaced during this audit, none urgent:

- PROJECT-STATE.md says "5 known-flaky"; retros say "4". Pick one and propagate.
- `project_vault_switching` auto-memory is stale — fix landed pre-Cluster-C. Update or retire.
- Addenda directory has exactly 1 addendum (`2026-04-15-daily-notes-templates-schemas.md`) plus README. Given implementations have produced at least 2 Obsidian-behavior discoveries not yet captured (§4 #2 footnote offsets, §4 #10 vault-switch crash lineage), addenda are under-used.
- `docs/test-coverage-matrix.md` hasn't been refreshed after E's 50 new tests landed — L1/L2 row for NoteEditorWidget / ReadingView needs filling.
- INDEX.md "Last updated" line is up to date (2026-04-15, Cluster E landed), good.

---

## 9. Next-step recommendation

One-sentence: **pause for one planning session before dispatching Cluster J** — expand J + G stubs to full plans against the post-E/I reality, hold a N design spike, and file the one Bases-DSL extraction sub-task that's gating K. Then Cluster J executes cleanly, and G + N + K all have a runway.

If the human wants to skip the pause: dispatching Cluster J as-stubbed is a reasonable risk — the retros show scope-drift is caught and corrected mid-dispatch. Cost of the pause is ~1 hour of planning; cost of skipping is ~1 round of plan-rewrite-mid-implementation in J.
