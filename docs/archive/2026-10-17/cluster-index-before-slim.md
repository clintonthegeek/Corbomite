# Frozen cluster index — 2026-10-17, before the slim

The long status notes and the "last updated" journal. Not the live index.
Live index: `docs/superpowers/plans/INDEX.md`.

# Cluster Plans — Index

> **Living document.** Table of contents over active strategic clusters. Status mirrors [`docs/PROJECT-STATE.md`](../../PROJECT-STATE.md) — when those diverge, **PROJECT-STATE is authoritative**. Update this file per Ritual 3 (cluster done) or Ritual 2 (mid-cluster status change).
>
> **Reset 2026-04-26.** Lettering restarted at A after the audit (`docs/audit-2026-04-26/`). Pre-reset legacy clusters live in `archive/` and are referenced as "legacy cluster &lt;letter&gt;" in `decisions-archive.md` to avoid collision.

**Last updated:** 2026-10-09 — Cluster M M6.1 landed (canvas references now enter
the vault metadata/link index); its next phase is rename rewriting. **Markoff vNext finish (2026-10-17):** [`2026-10-17-markoff-vnext-finish.md`](2026-10-17-markoff-vnext-finish.md) — not a cluster; M6.2 and M6.3 stay in Cluster M. The 2026-10-01
bookkeeping notes below remain historical context. Prior: 2026-10-01 — **cluster bookkeeping pass** (later the same day
as the second-sweep appraisal below, no code touched): Cluster **E retired**
to `archive/` with an item-by-item disposition banner; Cluster **F split** into
**F-a** (File Recovery + Note Composer) and **F-b** (named Workspaces + sidedock
substrate), with items 1–6 audited as already shipped and the stub marked
superseded; Cluster **Q opened** (the Tags pane — previously named only in two
roadmap bullets with no owner); Cluster **D** gains a **D.5 spec section**
(brainstorm, still not a dispatch); Cluster **L**'s Phase L5 gains the 6-scenario
checkbox list and its banner corrected from "not started" to "paused mid-flight
after finding a P0". Prior: 2026-10-01 — **second-sweep appraisal** (4 exhaustive
agent passes over code + docs, all 51 previously-open punch-list items
re-verified against the tree). 17 PARITY-MATRIX rows corrected, 3 stale cluster
rows in *this file* fixed, 5 punch-list items closed as already-fixed, 1
re-ranked P4→P1, 1 escalated, 15 new items filed, release-hygiene Phases A+E.1
promoted to P1, and the never-written Cluster L5 split-direction addendum
finally written. Report: [`../../audit-2026-10-01-second-sweep-appraisal.md`](../../audit-2026-10-01-second-sweep-appraisal.md).
Prior: 2026-08-21 — Cluster P CLOSED, all phases P0-P6 done. Prior: 2026-08-20 —
Cluster N merged; Cluster O O0-O3 closed. Prior: 2026-08-19 — Cluster M opened.
Prior: 2026-08-17 — Cluster L opened, Phases L0-L2 landed. Prior: 2026-06-10 —
archive sweep.

**Master plan (read this before picking cluster work):** [`2026-06-10-road-to-dogfood.md`](2026-06-10-road-to-dogfood.md) — sequences everything (data safety → Markoff contract-v2 adoption → editor/workspace polish → view gaps → architecture/release hygiene → dogfood loop). **⚠ Its Phases 2–6 are stale as of 2026-10-01** — Phases 0–1 are accurate and done, but Clusters L/M/N/O/P have substantially overtaken Phases 3–4 (canvas overshot, workspace overshot), Phase 5's `release-hygiene` dispatch was never executed, and its 7 release-criteria checkboxes are all unchecked while two releases have shipped. Treat it as a historical sequencing document and use the cluster table below + the punch list for current truth. Reconciling it is an open chore.

Companion docs: [`2026-06-10-release-hygiene.md`](2026-06-10-release-hygiene.md) (**~37% complete, 0 of its own boxes checked — re-verified against the tree 2026-10-02, and three of its premises are now factually wrong; read its banner before executing. DoD 2/7.** Phase A + E.1 done; the punch list is the authoritative tracker), [`../specs/2026-06-10-mainwindow-decomposition-design.md`](../specs/2026-06-10-mainwindow-decomposition-design.md), [`../../PARITY-MATRIX.md`](../../PARITY-MATRIX.md) (reconciled 2026-10-01), [`../../audit-2026-10-01-second-sweep-appraisal.md`](../../audit-2026-10-01-second-sweep-appraisal.md) (current health report).

## Active clusters

| Cluster | Title | Plan file | Type | Status | Notes |
|---|---|---|---|---|---|
| F-a | File Recovery & Note Composer | [full](2026-10-01-cluster-f-a-file-recovery-and-composer.md) | Full | **Plan written 2026-10-01 — ready to dispatch. Not started.** ~5–8d across P0–P3. | Split out of the Cluster F stub (items 7+8). Self-contained: no substrate, no cross-repo, no other cluster blocked. P0 = periodic dirty snapshot (the actual data-loss fix; today the only recovery write fires *after* a save has failed — `TextFileView.cpp:67-71`). P1 = retention + Settings + Version History modal, activating the disabled menu slot that already exists (`EditableFileView.cpp:136`). P2–P3 = the Note Composer modal + merge pipeline (addendum has the full spec). Five open questions, incl. **Q1: do not adopt Obsidian's in-vault backup path** — the audit corpus gives three different shapes for it, and Corbomite deliberately moved backups outside the vault in the 2026-04-27 sweep. |
| F-b | Named Workspaces & the sidedock substrate | [full](2026-10-01-cluster-f-b-workspaces-and-sidedock.md) | Full | **Plan written 2026-10-01 — NOT dispatchable. Needs its own spec first.** ~6–9d after that. | Split out of the Cluster F stub (items 9+10). Two blockers: (a) the `.obsidian/workspaces.json` schema was never extracted — `VAULT-FORMAT.md:362-368` marks it *inferred* and defers to `workspace.md §13 Q5`; (b) item 10 **lost its home class** when Cluster L3 deleted `WorkspaceSidedock`, so there is nothing to refactor — `Workspace.h:248-253` now hands the job to Cluster F explicitly. Correction to the old rationale: the "dirty-bit hack" is **gone**; `left`/`right` already round-trip via the unknown-root passthrough (`m_unknownRoot`, `Workspace.h:358-366`, tested at `tst_workspace_session.cpp:701`) — they are just not *modelled*. S0 (spec) is blocking; S1 (Corbomite-native named layouts) is independent of S2 (substrate). |
| Q | The Tags pane | [full](2026-10-01-cluster-q-tags-pane.md) | Full | **Plan written 2026-10-01 — ready to dispatch. Not started.** ~3–5d across Q0–Q2. | **New cluster 2026-10-01.** Post-reset letter Q (next free); *not* legacy Cluster Q (internal-plugin wrapping, closed 2026-04-17) — qualify as "Cluster Q (post-reset)". Closes the roadmap's "one wholly-missing core panel" (`road-to-dogfood.md:135`) and its last unowned release criterion (`:166`). `grep 'TagPane\|TagsPane\|TagsView'` over `src/`+`libs/` returns zero hits. Cheap because **five layers are already live**: `note_tags` table + `allTags()`/`notesWithTag()` (`SQLiteIndex.cpp:148-154,624-650`), the `tag:`/`-tag:` DSL operator, `TagSuggest` completion, frontmatter-tag merge (`MetadataParser.cpp:421-451`), and plugin right-sidebar mounting (`X-Corbomite-DockArea: "right"`). Missing: the pane, counts, subtag-prefix decomposition, and a plugin-side way to run a search. ⚠ The pane's **own** UI (sort/filter/nesting/context menu) has **no audit coverage at all** — `FEATURE-MATRIX.md:553` is three sentences of frozen Pass-3 text; Q2's polish items are stretch until an addendum exists. |
| M | Canvas authoring parity (Graffodil rebase first) | [full](2026-08-19-cluster-m-canvas-authoring-parity.md) | Full | **M0–M5 done; M6.1 canvas-link indexing landed 2026-10-09. Next: M6.2 rename rewriting, then M6.3 missing-file recovery.** | User decision: rebase `libs/canvas` onto Graffodil::Core (v0.2.2) before authoring features. M6.1 indexes file nodes, group backgrounds and text-card Markdown links/embeds; the focused SQLite index test passes. Remaining M6 phases are specified in the plan. |
| L | Workspace/KDDW stabilization & nativization | [full](2026-08-17-cluster-l-workspace-stabilization.md) | Full | **L0–L4 landed + closed (L4 live-verified 2026-08-18). L5 PAUSED mid-flight, not closed — now checkboxed.** | *(Rows corrected 2026-10-01 — it read "L4 NOT LIVE-VERIFIED; L5 not started", erasing both the L4 live verification and the P0 that L5 found. L5 ran, found and fixed a P0 nested-split-scrambling bug in the Obsidian round trip, added `fixture16_nestedSplitFirstChild_matchesObsidianGeometry`, then paused for the v0.1.0 release. **Later 2026-10-01:** the plan's own header banner was corrected from "L5 not started" and the 6 soak scenarios were given checkboxes **L5.1–L5.6** plus an L5.7 closeout gate, with the standing live-eyeball requirement stated in the plan — the round-trip P0 survived a fully green suite because the only tests were shape-only.)* | Full re-evaluation of the tab/dock layer (2026-08-17) found three disease clusters — teardown fragility, persistence split-brain, Obsidian-literalism residue. L0 (doctrine, [spec](../specs/2026-08-17-workspace-compat-boundary.md)) signed off; L1 (crash-safety, ASAN-clean) unblocked K Phase 4; L2 (full-fidelity writer + three-tier storage) landed; L3 (cruft removal, C6/`eState.scroll` deliberately skipped) landed. L4 (back/forward, tab commands, sidebar-width wiring) shipped and was live-verified 2026-08-18. Detail: `decisions-archive.md` (2026-08-17 entries). |
| O | Context-sensitive menu bar, toolbar & sidebar | [full](2026-08-20-cluster-o-context-sensitive-ui.md) | Full | **O0–O4 CLOSED 2026-08-21** (O4 `CanvasViewActions` live-eyeballed; cluster closed end to end, v0.2.0). **Next: Phase O5 (`BasesViewActions`/`GraphViewActions`).** *(Row corrected 2026-10-01 — it read "O0–O3 … Next: O4".)* | Audit report: [`audit-2026-08-20-context-sensitive-ui.md`](../../audit-2026-08-20-context-sensitive-ui.md) — of ~79 KActions, **zero** belong to canvas/bases/graph; 39 are markdown-only; 17 are permanently disabled stubs; 7 silently no-op off-markdown. Doctrine: three tiers (presence keyed on view type / enablement keyed on capability / check-state), hide-other-types + disable-unavailable, per-view-type `KXMLGUIClient` providers swapped on the factory. Phases: O1 correctness (7 bug fixes + `ActionContextController` extraction, **closed** `7c9faf91`/`c3fbf00d`/`bbe1c658`) → O2 capability contract (**closed** `d8c414ac`/`ade51e03`) → O3 provider mechanism, markdown (**closed** `e22ef7de`/`a7e04fb0`, clipboard actions ported in here post-Cluster-N-merge) → O4 canvas → O5 bases/graph/folded-I → O6 pane-local inspector → O7 stubs/soak. Absorbs Cluster I's remnant. Punch-list: `[ui-bundle][canvas][P2][cluster-o]`, `[graph][P2][cluster-o]`, `[editor][P3][verify-first][cluster-o]`. |

## Closed in this scheme

| Cluster | Title | Closed | Disposition |
|---|---|---|---|
| A | Vault-format compatibility sweep | 2026-04-27 | Drained inline via P0 sweep + BOM-strip closeout. Plan: [closed](archive/2026-04-26-cluster-a-vault-format-compat.md). raw/config-changed events reassigned to B. |
| B | Plugin API surface completion | 2026-04-28 | 16 items shipped across 4 phases — 11 mechanical / new-substrate proxies (Hover, Suggest, PostProcessor, Ribbon, Embed, CodeBlock, StatusBar, LucideIcon, MarkdownRenderer, DecorationProvider, ProtocolHandler), `Vault::raw` + `configChanged` events, expanded `.obsidian/` watcher, `Plugin::onExternalSettingsChange`, permission tokens public header + reference docs. 5 new permission tokens. Plan: [executed](archive/2026-04-28-cluster-b-plugin-api-surface.md). Spec: [`specs/archive/2026-04-28-cluster-b-plugin-api-surface-design.md`](../specs/archive/2026-04-28-cluster-b-plugin-api-surface-design.md). |
| C | Workspace serializer fidelity rebuild | 2026-04-27 | Drained inline via P1 sweep + serializer-consolidation work-unit. Plan: [closed](archive/2026-04-26-cluster-c-workspace-serializer.md). Sidedock-as-tree + named-workspaces reassigned to F. |
| E | Markoff Editor API parity | 2026-10-01 | **RETIRED, not executed** — scope was entirely qutepart-era line/column APIs (`getLine`/`setLine`/`getRange`/`replaceRange`/`posAtCoords`/`lineCount`). Item-by-item verification 2026-10-01: the only `Markoff::Editor` references left are two comment stubs; item 3 (unify Live/Source cursor base) was completed *then obsoleted* by contract-v2 Phase 1 Task 8, which moved Ln/Col onto the leaf-agnostic `Markoff::MarkdownView` base; item 4 (`Qutepart::extraCursors`) is moot — qutepart was deleted `da9a0a2c`; item 5 (`EditorExtension`) shipped in a different shape as `Corbomite::DecorationProvider` in Cluster B; item 6 (paste-HTML) shipped via Cluster N on top of `Markoff::ClipboardCodec`; item 7's cited `TextControl.cpp` does not exist in the tree; item 8 sequenced against G/H, both obsoleted by the 2026-05-25 foundation rewrite. Plan: [retired](archive/2026-04-26-cluster-e-markoff-editor-api-parity.md). **Residue:** re-derive the `Editor` surface against D2 + contract-v2 if/when plugin-author pressure arrives — that is a fresh brainstorm, not an expansion of this stub (contract-v2 deliberately defers plugin-facing editor surfaces). |
| P | STATIC → SHARED internal library refactor | 2026-08-21 | Plan [archived](archive/2026-08-20-cluster-p-shared-libraries.md); refactor report [kept](../../audit-2026-08-20-shared-libraries-refactor.md) |
| K | Markoff canvas leaf adoption | 2026-08-18 | Plan [archived](archive/2026-08-15-cluster-k-markoff-canvas-adoption.md); retro [kept](../../cluster-retros/cluster-k.md). Post-reset K, not legacy Cluster K |
| D | Bases UI completion | 2026-04-26 | Stub [archived](archive/2026-04-26-cluster-d-bases-ui-completion.md). Only D.5 remained and it got its own spec; the stub had no open work |
| F | Internal-plugin gap fill | 2026-10-01 | Stub [archived](archive/2026-04-26-cluster-f-internal-plugin-gap-fill.md), superseded by the F-a / F-b split below |
| I | Editor & Workspace UI surfacing (post-reset) | 2026-08-20 | Absorbed by Cluster O; plan [archived](archive/2026-04-26-cluster-i-editor-workspace-ui-surfacing.md), retro [kept](../../cluster-retros/cluster-i-ui-surfacing.md). Distinct from legacy Cluster I (closed 2026-04-17) |
| N | Rich clipboard (copy-as + smart paste) | 2026-08-20 | Plan [archived](archive/2026-08-20-cluster-n-rich-clipboard.md); N5 eyeball passed on `feature/rich-clipboard`; merged same day after a cleanup pass fixed an incomplete LO-table-paste fix and a BlockQuote/ListItem marker asymmetry (canvas + Styled), then ported the 8 clipboard actions onto Cluster O's `MarkdownViewActions` (O3.T6 had removed the flat `MainWindow` mechanism N was built on). Plan: [full](archive/2026-08-20-cluster-n-rich-clipboard.md). Detail: `decisions-archive.md`. Post-reset N — not legacy Cluster N (plugin-ready surfaces, closed 2026-04-17). |

> **Archived 2026-06-10:** all closed/executed plan files in this table — plus the executed D-series sub-plans (D.1–D.4c, formula editor, filter builder), properties-panel editing, find-UI port, workspace-serializer consolidation, and the styled-rendering plans — now live in [`archive/`](archive/); their design specs live in [`../specs/archive/`](../specs/archive/).

## Obsoleted by the foundation rewrite (2026-05-25)

These three clusters were built entirely on the old Markoff editor substrate (QGraphicsView scene + `QTextDocument` + `ObjectReplacementCharacter` substitution + qutepart Source mode). Markoff's `exploration/new-foundation` rebuild — a QML/QtQuick peer-delegate editor over a D2/CollabText block model, merged to Markoff `master` (`v0.7.0-freeze`) on 2026-05-25 — removes that substrate, so these clusters' problems no longer exist or are now the baseline. Plans archived 2026-06-10 with disposition banners; do not dispatch.

| Cluster | Title | Plan file | Why obsolete |
|---|---|---|---|
| G | Markoff Phase C8 (inline-ORC coherence) | [archived](archive/2026-04-26-cluster-g-markoff-phase-c8.md) | Guarded against `U+FFFC` corruption in `QTextDocument`-substituted glyphs. No QTextDocument, no ORC glyphs in the new model — the corruption class can't occur. E1 InlineHighlighter is the replacement. |
| H | Block-substitution widgets | [archived](archive/2026-04-26-cluster-h-block-substitution-widgets.md) | Goal was to promote math/mermaid *out of* `QTextDocument` *into* peer graphics items. That *is* the new baseline — every block is a peer QML delegate. Superseded by Markoff E5 (math/Mermaid Live parity). |
| J | Qutepart-Corbomite Fork | [archived](archive/2026-04-26-cluster-j-qutepart-fork.md) | Was going to vendor + own a Source-mode widget; qutepart-corbomite was deleted 2026-04-20 (`da9a0a2c`). The new foundation's `Markoff::Source::Editor` (block-aware d2 edits, format ops) superseded it. Confirmation of full intent-coverage (visual-line scroll, fold serialization, find/replace, markdown awareness) is tracked in PROJECT-STATE Open questions. |

## Where to find more

- **Punch list (small fixes, severity-ranked):** [`docs/punch-list.md`](../../punch-list.md) — 58 items at reset, drains continuously
- **Audit (canonical task source):** [`docs/audit-2026-04-26/`](../../audit-2026-04-26/) — 14 sub-reports + synthesis README
- **Decisions archive (history):** [`docs/decisions-archive.md`](../../decisions-archive.md) — append-only closeouts; legacy cluster letters live here
- **Closed plans (reference):** [`archive/`](archive/) — 75+ landed/retired plan files (post-reset closures archived 2026-06-10 alongside the pre-reset legacy set)
- **Pre-reset state (reference):** [`docs/archive-2026-04-26/`](../../archive-2026-04-26/) — frozen snapshots of `backlog.md` and `PROJECT-STATE.md` at reset time

## Conventions

- **Stub plans** are skeletons created by audit-driven cluster creation. They cite audit references and sketch scope; brainstorm + full plan expansion is required before dispatch.
- **Full plans** follow the structure: goal, audit references, target classes, KDE/Qt prior art, work breakdown phases, Explore-agent dispatch prompts, definition of done, blocks/enables.
- **Scouting docs** are pre-plan notes; not dispatchable.
- **Status values** must match PROJECT-STATE verbatim.
- New cluster letters (A onwards) refer to the **post-reset** scheme. Legacy letters (anything in `archive/` or `decisions-archive.md` predating 2026-04-26) refer to the **pre-reset** scheme. Always qualify if ambiguous: "legacy Cluster Y" vs "Cluster Y (post-reset)". **Cluster N (this scheme) is rich clipboard, 2026-08-20, closed and merged.** Legacy Cluster N was plugin-ready surfaces (closed 2026-04-17).

## Reading order

If you have to start somewhere blind: read PROJECT-STATE first, then this INDEX, then the punch list (top P0s), then the cluster plan(s) for the focus PROJECT-STATE names, then the audit-doc sections those plans cite.
