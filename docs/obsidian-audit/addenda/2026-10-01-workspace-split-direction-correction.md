# Workspace split `direction` names the divider line, not the stacking axis

**Date:** 2026-10-01
**Discovered during:** Cluster L Phase L5 (soak dogfood), originally 2026-08-18;
written up 2026-10-01 during the second-sweep appraisal
([`audit-2026-10-01-second-sweep-appraisal.md`](../../audit-2026-10-01-second-sweep-appraisal.md) §5).
**Supersedes / extends:** `domains/workspace.md` §2 (line 79) and the related
claims at lines 192 and 394. **This addendum REFUTES those claims.**
**Relevant cluster plans:** `superpowers/plans/2026-08-17-cluster-l-workspace-stabilization.md`
(Phase L5).

## Finding

`domains/workspace.md:79` glosses Obsidian's split `direction` field as:

> direction `"horizontal"` (top-to-bottom) or `"vertical"` (left-to-right)

That is **inverted**. `direction` names the **orientation of the divider line**,
not the stacking axis of the children:

| `direction` | divider line | sibling arrangement |
|---|---|---|
| `"vertical"` | vertical | **left ↔ right**, side by side |
| `"horizontal"` | horizontal | **top ↔ bottom**, stacked |

The audit's own other statements corroborate the corrected reading once you apply
it. `workspace.md:192` and `:394` both record that a restored root/window container
is **always `"vertical"`** — which, under the corrected reading, means a
side-by-side pair. That *is* Obsidian's default two-column layout. Under the
audit's inverted reading, a root `"vertical"` would mean top/bottom panes, which
is not what Obsidian restores by default. The audit stated both facts and never
reconciled them, because each was individually plausible.

**Two independent confirmations of the corrected reading:**

1. **Live round-trip against a real Obsidian-authored `workspace.json`** (Cluster
   L5). A three-pane vault written by Obsidian had every pane except the anchor
   land in the wrong quadrant under the audit's mapping. Inverting the mapping
   reproduced Obsidian's geometry exactly, confirmed by the user across two round
   trips.
2. The root-always-`"vertical"` fact above, which only makes sense under the
   corrected reading.

## Why noticed now

It surfaced as a P0 defect, not as a documentation question. A real
Obsidian-authored 3-pane vault opened in Corbomite with every pane but the anchor
in the wrong quadrant — a silent workspace-layout corruption on open, in the
serializer that governs the Obsidian-shared `workspace.json`.

Root cause was **two compounding bugs** in `libs/core/src/WorkspaceSerializer.cpp`,
not one:

1. **This inversion.** `directionToKddwLocation()` mapped the two values backwards.
2. **A materialization anchor-ordering bug** in `materializeSplit()`, which
   threaded a nested child's internal structure into place *before* its outer
   siblings existed, carving up the wrong KDDW frame. Fixing only the inversion
   still produced a different wrong layout; it took a second live round trip.

Both are now fixed (`WorkspaceSerializer.cpp:333-336` and `:469-472` carry the
corrected comments). A geometry-asserting regression test was added —
`fixture16_nestedSplitFirstChild_matchesObsidianGeometry` — because the prior
nested-split test only checked JSON shape and therefore **passed throughout the
bug's entire lifetime**.

## Action taken

- Code comments corrected in place at `WorkspaceSerializer.cpp:333-336` and
  `:469-472`, explicitly noting the divider-line-vs-stacking-axis distinction.
- This addendum written (2026-10-01) — the code fix landed 2026-08-18, the
  documentation follow-up was missed for ~6 weeks across Clusters L5 → M → N → O
  → P. During that window no addendum was written at all, and
  `addenda/README.md` continued to direct implementers to `workspace.md §2` for
  split-direction guidance.
- Linked from `00-taxonomy.md` §Addenda (per the linkage ritual) and cited in the
  Cluster L plan's audit references.
- **Not** recorded as a punch-list item: the defect is fixed. The documentation
  debt was the gap, and this file closes it.

## Note for future implementers

`addenda/README.md` lists `workspace.md §2` among the sections to check before
implementing from. That advice is now backed by a real refutation rather than a
routine correction. The general lesson, worth applying to the other listed
sections (`vault.md` §1/§8, `metadata.md` §2, `views.md` ViewState,
`00-taxonomy.md` QueryController, `editor.md` expandText/`lP`): **the audit's
prose glosses are less reliable than the field-level facts it records.** Where an
addendum contradicts a *gloss* but not a *field name*, trust the field facts and
verify the mapping empirically against a real Obsidian-authored file before
building on it. The inverted `direction` gloss is a perfect example — the audit
recorded `direction`, the valid values, and the root-always-`"vertical"`
invariant correctly, and still produced a wrong reading of what the value means.
