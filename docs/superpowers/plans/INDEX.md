# Cluster Plans — Index

> **Living document.** Table of contents over active strategic clusters. Status mirrors [`docs/PROJECT-STATE.md`](../../PROJECT-STATE.md) — when those diverge, **PROJECT-STATE is authoritative**. Update this file per Ritual 3 (cluster done) or Ritual 2 (mid-cluster status change).
>
> **Reset 2026-04-26.** Lettering restarted at A after the audit (`docs/audit-2026-04-26/`). Pre-reset legacy clusters live in `archive/` and are referenced as "legacy cluster &lt;letter&gt;" in `decisions-archive.md` to avoid collision.

**Last updated:** 2026-04-26 — tracking reset; 6 audit-derived clusters created (A–F, all stub plans), 4 in-flight clusters re-lettered (G–J), 8 plans archived.

## Active clusters

| Cluster | Title | Plan file | Type | Status | Notes |
|---|---|---|---|---|---|
| A | Vault-format compatibility sweep | [stub](2026-04-26-cluster-a-vault-format-compat.md) | Stub | Plan-needed | Aggregates P0 vault-format corruption from audit. Silent data churn risk if deferred. |
| B | Plugin API surface completion | [stub](2026-04-26-cluster-b-plugin-api-surface.md) | Stub | Plan-needed | 6 missing plugin proxies + 6 missing verbs. Blocks third-party plugin authoring. |
| C | Workspace serializer fidelity rebuild | [stub](2026-04-26-cluster-c-workspace-serializer.md) | Stub | Plan-needed | 5+ orthogonal `workspace.json` lossiness issues; cross-tool degradation on first save. |
| D | Bases UI completion | [stub](2026-04-26-cluster-d-bases-ui-completion.md) | Stub | Plan-needed | Runtime exists (legacy K). Builds the missing UI: formula editor, group rendering, properties drawer, export, drag, hover, undo, multi-key sort. |
| E | Markoff Editor API parity | [stub](2026-04-26-cluster-e-markoff-editor-api-parity.md) | Stub | Plan-needed | Plugin shim — `getLine`/`replaceRange`/`posAtCoords`/etc. Multi-cursor in Live mode. `EditorExtension` type. Should land before Cluster B's `registerEditorExtension`. |
| F | Internal-plugin gap fill | [stub](2026-04-26-cluster-f-internal-plugin-gap-fill.md) | Stub | Plan-needed | 8 missing internal plugins (daily-notes, templates, page-preview, word-count, file-recovery, note-composer, switcher, command-palette). Some gated on B. |
| G | Markoff Phase C8 (inline-ORC coherence) | [full](2026-04-26-cluster-g-markoff-phase-c8.md) | Full | In-flight | Markoff-side work-unit C8. Was: `2026-04-23-phase-c8-inline-orc-canonical-coherence.md`. |
| H | Block-substitution widgets | [scouting](2026-04-26-cluster-h-block-substitution-widgets.md) | Scouting | Blocked on G | Promotes block math + mermaid out of QTextDocument substitution into peer `QGraphicsItem`s. Was: legacy Cluster X. |
| I | Editor & Workspace UI surfacing | [full](2026-04-26-cluster-i-editor-workspace-ui-surfacing.md) | Full | In-flight | Was: legacy Cluster V. Plan written 2026-04-20; phases not yet executed. |
| J | Qutepart-Corbomite Fork | [full](2026-04-26-cluster-j-qutepart-fork.md) | Full | In-flight (Phases 1+2 done) | Long-term internal Markoff Source-mode refactor. Was: standalone "Parallel long-term internal refactor". Next: Phase 3 (public find/replace API). |

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
