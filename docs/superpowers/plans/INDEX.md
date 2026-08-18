# Cluster Plans — Index

> **Living document.** Table of contents over active strategic clusters. Status mirrors [`docs/PROJECT-STATE.md`](../../PROJECT-STATE.md) — when those diverge, **PROJECT-STATE is authoritative**. Update this file per Ritual 3 (cluster done) or Ritual 2 (mid-cluster status change).
>
> **Reset 2026-04-26.** Lettering restarted at A after the audit (`docs/audit-2026-04-26/`). Pre-reset legacy clusters live in `archive/` and are referenced as "legacy cluster &lt;letter&gt;" in `decisions-archive.md` to avoid collision.

**Last updated:** 2026-08-17 — Cluster L: Phases L0-L2 landed (doctrine spec accepted, teardown-UAF crash class fixed, production writer full-fidelity + three-tier storage split). Prior same-day: Cluster L opened. Prior: 2026-06-10 — archive sweep: 20 executed/obsolete plans + 17 executed/obsolete specs moved to `archive/` per audit; cluster banners added (obsolete G/H/J disposition, kept-stub D/E/I corrections).

**Master plan (read this before picking cluster work):** [`2026-06-10-road-to-dogfood.md`](2026-06-10-road-to-dogfood.md) — sequences everything (data safety → Markoff contract-v2 adoption → editor/workspace polish → view gaps → architecture/release hygiene → dogfood loop). Companion docs: [`2026-06-10-release-hygiene.md`](2026-06-10-release-hygiene.md), [`../specs/2026-06-10-mainwindow-decomposition-design.md`](../specs/2026-06-10-mainwindow-decomposition-design.md), [`../../PARITY-MATRIX.md`](../../PARITY-MATRIX.md).

## Active clusters

| Cluster | Title | Plan file | Type | Status | Notes |
|---|---|---|---|---|---|
| D | Bases UI completion | [stub](2026-04-26-cluster-d-bases-ui-completion.md) | Stub | **Only D.5 remains (needs spec)** | **Unaffected by port.** D.1/D.2/D.3 shipped 2026-05-25, D.4a/D.4b 2026-05-26, D.4c + formula editor 2026-05-27, filter builder 2026-05-28. Sub-plan/spec history now lives in [`archive/`](archive/) + [`../specs/archive/`](../specs/archive/) and [`docs/decisions-archive.md`](../../decisions-archive.md). **D.5 (Bases plugin API)** is absent from the stub's scope list — needs a fresh brainstorm+spec. Hover-preview + Markdown cells re-targetable at StyledRenderEngine (landed 2026-05-30). |
| E | Markoff Editor API parity | [stub](2026-04-26-cluster-e-markoff-editor-api-parity.md) | Stub | **Re-scope vs D2 model** | Plugin shim — was line/column-based (`getLine`/`replaceRange`/`posAtCoords`). The new foundation's D2 block model invalidates that surface; **stub body obsolete — see banner.** Re-scope when plugin-editor-API pressure arrives (likely post-port-merge). Dependency on J Phases 1–2 dissolved. |
| F | Internal-plugin gap fill | [stub](2026-04-26-cluster-f-internal-plugin-gap-fill.md) | Stub | Plan-needed | **Mostly unaffected.** 8 missing internal plugins + Workspaces plugin + sidedock-as-tree substrate (pulled from C). Plugins touching the editor API wait on E's rework; the rest are independent. Word-count's Cluster B gate cleared 2026-04-28 (`StatusBarRegistry` exists). |
| I | Editor & Workspace UI surfacing | [full](2026-04-26-cluster-i-editor-workspace-ui-surfacing.md) | Full | **Needs rewrite — header was false; Phases 1–4 shipped as legacy Cluster V; live remnant is Phase 5 tail + Phase 6** | Was: legacy Cluster V — Phases 1–4 shipped 2026-04-20 ([retro](../../cluster-retros/cluster-v.md); commits `c0e63f44..127530f4`); Phase 4 substrate (`libs/readingview`) since deleted. Live remnant: Phase 5 tail (move-to-new-window / link-with-active-pane UI) + Phase 6 (search regex/match-case toggles, `Notice::post`). The earlier "being redone on `port/foundation-exploration`" note is stale — that branch merged + was deleted 2026-05-25. Rewrite before dispatch. |
| K | Markoff canvas leaf adoption | [full](2026-08-15-cluster-k-markoff-canvas-adoption.md) | Full | **Phase 3 closed 2026-08-17 — 10/14 dogfood findings fixed + live-confirmed; Phase 4 (dogfood-as-default) not started, unblocked by Cluster L Phase L1** | Markoff's canvas production arc (D5 part 1) closed 2026-08-15 at 315/315 tests, pending its own G2 (Corbomite adoption) gate — this cluster is that adoption. Goal: swap `Markoff::Live::EditorWidget` (QML) for `Markoff::Canvas::EditorWidget` behind a settings toggle, verify parity, dogfood, then retire the QML leaf (mirrors Markoff's G2/G3). `NoteEditorWidget`'s leaf-agnostic base dispatch (contract-v2, closed 2026-06-10) means most call sites need zero changes. Remaining 4 findings are low-severity polish, not blockers — see punch-list `[cluster-k]` tag. Phase 4 (dogfood-as-default) + Phase 5 (QML retirement) still open. |
| L | Workspace/KDDW stabilization & nativization | [full](2026-08-17-cluster-l-workspace-stabilization.md) | Full | **L0-L3 landed 2026-08-17; L4-L5 not started** | Full re-evaluation of the tab/dock layer (2026-08-17) found three disease clusters — teardown fragility, persistence split-brain, Obsidian-literalism residue. L0 (compat-boundary doctrine, [spec](../specs/2026-08-17-workspace-compat-boundary.md)) signed off; L1 (crash-safety: `destroyLeaves` primitive, findings A1/A3/A4) landed ASAN-clean, unblocking K Phase 4; L2 (one writer, full fidelity: B1/C2/denylist/B4 fixed, B3 id-assignment fixed with a dimension-round-trip gap punch-listed `[cluster-l]`) landed; L3 (cruft removal: C1/C3/C4 fixed, C5 partial, C6 + `eState.scroll` deliberately skipped as out-of-proportion for the phase) landed. Next: L4 (native UX polish — needs live eyeball) or L5 (soak & closeout). Detail: `decisions-archive.md` (2026-08-17 entries). |

## Closed in this scheme

| Cluster | Title | Closed | Disposition |
|---|---|---|---|
| A | Vault-format compatibility sweep | 2026-04-27 | Drained inline via P0 sweep + BOM-strip closeout. Plan: [closed](archive/2026-04-26-cluster-a-vault-format-compat.md). raw/config-changed events reassigned to B. |
| B | Plugin API surface completion | 2026-04-28 | 16 items shipped across 4 phases — 11 mechanical / new-substrate proxies (Hover, Suggest, PostProcessor, Ribbon, Embed, CodeBlock, StatusBar, LucideIcon, MarkdownRenderer, DecorationProvider, ProtocolHandler), `Vault::raw` + `configChanged` events, expanded `.obsidian/` watcher, `Plugin::onExternalSettingsChange`, permission tokens public header + reference docs. 5 new permission tokens. Plan: [executed](archive/2026-04-28-cluster-b-plugin-api-surface.md). Spec: [`specs/archive/2026-04-28-cluster-b-plugin-api-surface-design.md`](../specs/archive/2026-04-28-cluster-b-plugin-api-surface-design.md). |
| C | Workspace serializer fidelity rebuild | 2026-04-27 | Drained inline via P1 sweep + serializer-consolidation work-unit. Plan: [closed](archive/2026-04-26-cluster-c-workspace-serializer.md). Sidedock-as-tree + named-workspaces reassigned to F. |

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
- New cluster letters (A onwards) refer to the **post-reset** scheme. Legacy letters (anything in `archive/` or `decisions-archive.md` predating 2026-04-26) refer to the **pre-reset** scheme. Always qualify if ambiguous: "legacy Cluster Y" vs "Cluster Y (post-reset)".

## Reading order

If you have to start somewhere blind: read PROJECT-STATE first, then this INDEX, then the punch list (top P0s), then the cluster plan(s) for the focus PROJECT-STATE names, then the audit-doc sections those plans cite.
