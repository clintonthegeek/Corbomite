# Cluster D — Bases UI completion

> **⚠ 2026-06-10:** 90% executed via D.1–D.4c + formula editor + filter builder (see [INDEX](INDEX.md)). Only D.5 (Bases plugin API) remains — and it is absent from the scope list below. Treat this file as historical; D.5 needs a fresh brainstorm+spec.

> **Created 2026-04-26 from audit reset.** Stub plan; needs brainstorm + full plan expansion before dispatch. Cluster K (legacy) shipped the Bases runtime + data model + Pratt parser as MVP. The UI is skeletal: no formula editor, no group rendering, no properties drawer, no export, no drag, no hover, no undo, no multi-key sort UI. This cluster builds out those surfaces.

## Goal

Bases reaches feature-parity-comparable UX for vault users editing `.base` files. A user opens a `.base` file, sees a fully-functional table view with grouping, can filter via a structured UI (not just YAML), can edit cells round-tripping back to source `.md`, can drag rows, can sort multi-column, can export.

## Audit references

- [audit-2026-04-26/bases.md](../../audit-2026-04-26/bases.md) — entire doc; especially §"Missing (prioritized: structural vs cosmetic)"
- [audit-2026-04-26/bases.md](../../audit-2026-04-26/bases.md) §"On-disk `.base` format compatibility" — overlap with Cluster A (key order)
- [obsidian-audit/addenda/2026-04-17-bases-formula-dsl.md](../../obsidian-audit/addenda/2026-04-17-bases-formula-dsl.md) — formula DSL reference

## Scope (in scope)

1. Formula editor (with parser-backed validation + autocomplete)
2. Filter UI (structured builder, not raw YAML editing)
3. Group rendering (currently group-by parses but groups don't render)
4. Properties drawer (per-row frontmatter editor)
5. Export (CSV / JSON / clipboard)
6. Drag (row reorder, drag-out-as-link)
7. Hover (rich preview popover on row hover)
8. Undo stack integration (cell edits go through QUndoStack)
9. Multi-key sort UI
10. Vault-bound function plumbing — `file()`, `LinkValue::asFile`, `looseEquals`, `linksTo` (currently stubs)
11. `TagListValue` for `file.tags.contains("#parent")` matching subtag semantics
12. `ListValue::sort()` null-comparator bug fix (small, could go to punch list — keep here for cluster coherence)

## Out of scope

- `.base` YAML key-order preservation → **Cluster A** (shares root cause with frontmatter writer)
- Other view kinds (cards, list, calendar) → defer to a future cluster

## Phases

TBD — brainstorm. Likely 4–5 phases: (1) formula editor + autocomplete, (2) filter UI + structured builder, (3) group rendering + properties drawer, (4) interactivity (drag, hover, undo, multi-key sort), (5) Vault-bound function plumbing + cleanup.

## Status

**Plan-needed** (stub).
