# Cluster Plans — Index

> **Living document.** Table of contents over active strategic clusters. Status mirrors [`docs/PROJECT-STATE.md`](../../PROJECT-STATE.md) — when those diverge, **PROJECT-STATE is authoritative**. Update this file per Ritual 3 (cluster done) or Ritual 2 (mid-cluster status change).
>
> **Reset 2026-04-26.** Lettering restarted at A after the audit (`docs/audit-2026-04-26/`). Pre-reset legacy clusters live in `archive/` and are referenced as "legacy cluster &lt;letter&gt;" in `decisions-archive.md` to avoid collision.

**Last updated:** 2026-05-25 — Foundation port reconciliation. Markoff's QML/D2 rebuild merged to Markoff `master` (`v0.7.0-freeze`); the rewrite obsoletes G/H/J and re-scopes E. D/F/I-Workspace unaffected. See PROJECT-STATE + the new "Obsoleted by the foundation rewrite" table below.

## Active clusters

| Cluster | Title | Plan file | Type | Status | Notes |
|---|---|---|---|---|---|
| D | Bases UI completion | [stub](2026-04-26-cluster-d-bases-ui-completion.md) | Stub | **In progress (D.1 done)** | **Unaffected by port.** Decomposed into sub-projects D.1–D.5. **D.1 (backend correctness) shipped 2026-05-25** — spec [`2026-05-25-cluster-d1-bases-backend-correctness-design.md`](../specs/2026-05-25-cluster-d1-bases-backend-correctness-design.md), plan [`2026-05-25-cluster-d1-bases-backend-correctness.md`](2026-05-25-cluster-d1-bases-backend-correctness.md): VaultResolver seam + `file()`/`asFile`/`linksTo`, one-directional tags, sort pinned. Remaining: D.2 read-side rendering (groups, multi-key sort, rich cells), D.3 editing UI (formula editor, filter builder, properties drawer), D.4 interactivity/export, D.5 plugin API. |
| E | Markoff Editor API parity | [stub](2026-04-26-cluster-e-markoff-editor-api-parity.md) | Stub | **Re-scope vs D2 model** | Plugin shim — was line/column-based (`getLine`/`replaceRange`/`posAtCoords`). The new foundation's D2 block model invalidates that surface; re-scope when plugin-editor-API pressure arrives (likely post-port-merge). Dependency on J Phases 1–2 dissolved. |
| F | Internal-plugin gap fill | [stub](2026-04-26-cluster-f-internal-plugin-gap-fill.md) | Stub | Plan-needed | **Mostly unaffected.** 8 missing internal plugins + Workspaces plugin + sidedock-as-tree substrate (pulled from C). Plugins touching the editor API wait on E's rework; the rest are independent. |
| I | Editor & Workspace UI surfacing | [full](2026-04-26-cluster-i-editor-workspace-ui-surfacing.md) | Full | **Partly absorbed into port** | Was: legacy Cluster V. Editor-surfacing half is being redone on `port/foundation-exploration` (heading actions, per-leaf format dispatch); reconcile against that rather than executing the 2026-04-20 plan. Workspace (KDDW) half is substrate-independent and survives. |

## Closed in this scheme

| Cluster | Title | Closed | Disposition |
|---|---|---|---|
| A | Vault-format compatibility sweep | 2026-04-27 | Drained inline via P0 sweep + BOM-strip closeout. Plan: [closed](2026-04-26-cluster-a-vault-format-compat.md). raw/config-changed events reassigned to B. |
| B | Plugin API surface completion | 2026-04-28 | 16 items shipped across 4 phases — 11 mechanical / new-substrate proxies (Hover, Suggest, PostProcessor, Ribbon, Embed, CodeBlock, StatusBar, LucideIcon, MarkdownRenderer, DecorationProvider, ProtocolHandler), `Vault::raw` + `configChanged` events, expanded `.obsidian/` watcher, `Plugin::onExternalSettingsChange`, permission tokens public header + reference docs. 5 new permission tokens. Plan: [executed](2026-04-28-cluster-b-plugin-api-surface.md). Spec: [`specs/2026-04-28-cluster-b-plugin-api-surface-design.md`](../specs/2026-04-28-cluster-b-plugin-api-surface-design.md). |
| C | Workspace serializer fidelity rebuild | 2026-04-27 | Drained inline via P1 sweep + serializer-consolidation work-unit. Plan: [closed](2026-04-26-cluster-c-workspace-serializer.md). Sidedock-as-tree + named-workspaces reassigned to F. |

## Obsoleted by the foundation rewrite (2026-05-25)

These three clusters were built entirely on the old Markoff editor substrate (QGraphicsView scene + `QTextDocument` + `ObjectReplacementCharacter` substitution + qutepart Source mode). Markoff's `exploration/new-foundation` rebuild — a QML/QtQuick peer-delegate editor over a D2/CollabText block model, merged to Markoff `master` (`v0.7.0-freeze`) on 2026-05-25 — removes that substrate, so these clusters' problems no longer exist or are now the baseline. Plans left in place for history; do not dispatch.

| Cluster | Title | Plan file | Why obsolete |
|---|---|---|---|
| G | Markoff Phase C8 (inline-ORC coherence) | [full](2026-04-26-cluster-g-markoff-phase-c8.md) | Guarded against `U+FFFC` corruption in `QTextDocument`-substituted glyphs. No QTextDocument, no ORC glyphs in the new model — the corruption class can't occur. E1 InlineHighlighter is the replacement. |
| H | Block-substitution widgets | [scouting](2026-04-26-cluster-h-block-substitution-widgets.md) | Goal was to promote math/mermaid *out of* `QTextDocument` *into* peer graphics items. That *is* the new baseline — every block is a peer QML delegate. Superseded by Markoff E5 (math/Mermaid Live parity). |
| J | Qutepart-Corbomite Fork | [full](2026-04-26-cluster-j-qutepart-fork.md) | Was going to vendor + own a Source-mode widget. New foundation ships `Markoff::Source::Editor` (block-aware d2 edits, format ops). **Likely** obsolete — confirm `Markoff::Source::Editor` covers the qutepart-fork intent (visual-line scroll, fold serialization, find/replace, markdown awareness) before formal closeout. |

## Where to find more

- **Punch list (small fixes, severity-ranked):** [`docs/punch-list.md`](../../punch-list.md) — 58 items at reset, drains continuously
- **Audit (canonical task source):** [`docs/audit-2026-04-26/`](../../audit-2026-04-26/) — 14 sub-reports + synthesis README
- **Decisions archive (history):** [`docs/decisions-archive.md`](../../decisions-archive.md) — append-only closeouts; legacy cluster letters live here
- **Closed plans (reference):** [`archive/`](archive/) — 50+ landed/retired plan files
- **Pre-reset state (reference):** [`docs/archive-2026-04-26/`](../../archive-2026-04-26/) — frozen snapshots of `backlog.md` and `PROJECT-STATE.md` at reset time

## Conventions

- **Stub plans** are skeletons created by audit-driven cluster creation. They cite audit references and sketch scope; brainstorm + full plan expansion is required before dispatch.
- **Full plans** follow the structure: goal, audit references, target classes, KDE/Qt prior art, work breakdown phases, Explore-agent dispatch prompts, definition of done, blocks/enables.
- **Scouting docs** are pre-plan notes; not dispatchable.
- **Status values** must match PROJECT-STATE verbatim.
- New cluster letters (A onwards) refer to the **post-reset** scheme. Legacy letters (anything in `archive/` or `decisions-archive.md` predating 2026-04-26) refer to the **pre-reset** scheme. Always qualify if ambiguous: "legacy Cluster Y" vs "Cluster Y (post-reset)".

## Reading order

If you have to start somewhere blind: read PROJECT-STATE first, then this INDEX, then the punch list (top P0s), then the cluster plan(s) for the focus PROJECT-STATE names, then the audit-doc sections those plans cite.
