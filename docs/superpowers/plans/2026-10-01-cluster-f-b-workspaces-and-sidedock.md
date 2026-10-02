# Cluster F-b — Named Workspaces & the Sidedock Substrate

**Date:** 2026-10-01
**Type:** Full plan (split out of the Cluster F stub, items 9 + 10) — **but
needs its own spec before dispatch** (see §2)
**Status:** **Plan written 2026-10-01 — NOT dispatchable yet.** The two items
below are the least-specified surface left in the project: one has an
*inferred* on-disk schema that the audit itself flags as unresolved, and the
other has no home class left to refactor. §5 lists the questions that must be
answered first; the phase breakdown in §6 is a shape, not a dispatch.
**Sizing:** ~6–9 days *after* the spec, and the sidedock substrate is the part
that could double it (it is an architecture change, not a feature). The two
items are separable: Workspaces can ship without the substrate if the substrate
work is deferred.

**Why this exists:** items 9 and 10 came from Cluster C's closeout
(`decisions-archive.md` 2026-04-27, "Sidedock-as-tree + named-workspaces
reassigned to F") and then sat in the never-expanded F stub. Worse, item 10
**lost its home class**: Cluster L3 deleted `WorkspaceSidedock` as a nullptr
shell, so the work as originally written ("refactor `WorkspaceSidedock`") no
longer has a target. `libs/core/include/corbomite/core/Workspace.h:248-253`
now explicitly hands the job to Cluster F.

---

## 1. Goal

1. **Named Workspaces** — save the current tab/dock layout under a name,
   switch back to it later, and interoperate with Obsidian's
   `.obsidian/workspaces.json` so a vault's named workspaces survive a
   round-trip through either app.
2. **Sidedock as workspace tree** — model the left and right sidebars as real
   parts of the `Workspace` object graph instead of living outside the KDDW
   tree in `CorbomiteMDI`, so that (a) `workspace.leftSplit.collapse()`-shaped
   plugin calls have an honest target, and (b) `left`/`right` sub-trees in
   `workspace.json` are *understood* rather than merely passed through.

---

## 2. Why this is not dispatchable yet

Two blockers, both documented rather than guessed:

- **The `workspaces.json` schema was never extracted from Obsidian.** It is
  marked *inferred* — `docs/obsidian-audit/VAULT-FORMAT.md:362-368` says
  verbatim: *"Schema (inferred). `Record<workspaceName, LayoutJson>` — one
  snapshot per named workspace, in the same shape as `workspace.json`. Exact
  schema is a follow-up (workspace.md §13 Q5)."* Building a writer against an
  inferred schema produces exactly the class of bug Cluster L's P0 round-trip
  scramble was: plausible-looking JSON that is wrong in the other app. **Sizing
  a phase around an unverified schema is how that happened.**
- **There is no home class for the substrate work.** `WorkspaceSidedock` is
  gone, and the sidebar is a `QSplitter` owned by a Kate-era `MainWindow`
  subclass in `src/mdi/`. Refactoring that into the KDDW tree is an
  architectural change with a real teardown-crash surface — the same surface
  Cluster L Phase L1 spent a whole phase unifying.

**Recommended order:** (a) verify the schema against a real Obsidian vault, as
an addendum; (b) ship Workspaces scoped to *Corbomite's own* named layout
storage with Obsidian read-only support, deferring Obsidian write until the
schema is verified; (c) treat the sidedock substrate as its own cluster-level
spec, not a phase of this one.

---

## 3. Current-state audit (verified 2026-10-01 against the tree)

### 3.1 Named Workspaces — absent

| Fact | Evidence |
|---|---|
| Zero references to `workspaces.json` outside documentation | `grep -n 'workspaces\.json'` over `src/`, `libs/`, top-level `CMakeLists.txt` returns nothing |
| Zero references to any named-workspace concept | `grep -ni 'namedworkspace'` → nothing |
| The **only** named-workspace mention in the codebase's docs is the vault-format table | `docs/obsidian-audit/VAULT-FORMAT.md:41` — "`workspaces.json` (named-workspaces snapshots — owned by Workspaces internal plugin)" |
| Layout save/load *does* exist — it is just per-vault, not named | `Workspace::serialize()` (`libs/core/src/Workspace.cpp:895-911`) and `WorkspaceSerializer::toJson`; `Workspace::{read,write}WorkspaceJson` became the production path in Cluster L2 |
| Sidebar plugin/tool-view state already has a persistence tier | Cluster L2's three-tier storage (`.obsidian/workspace.json` for Obsidian-shaped keys, Corbomite-native keys split by tier) — `Workspace.h:358-366` (`m_unknownRoot`) is the tier boundary |
| Obsidian's switch mechanism is an internal-plugin method, not a workspace call | `VAULT-FORMAT.md:364-366` — `app.internalPlugins.getEnabledPluginById("workspaces").switchToSavedWorkspace(name)` → `workspace.changeLayout(json)` |
| Corbomite has **no** `changeLayout`-equivalent seam | `grep -n 'changeLayout\|applyLayout\|loadLayout'` over `src/`, `libs/core`, `libs/models` returns hits only in `libs/graffodil` (the force-graph layout library, unrelated). Named-layout application would be new work — S1.2 must find the closest existing seam and say which one it is |

### 3.2 Sidedock-as-workspace-tree — no home class

| Fact | Evidence |
|---|---|
| `WorkspaceSidedock` was deleted as a nullptr shell in Cluster L3 | `docs/superpowers/plans/2026-08-17-cluster-l-workspace-stabilization.md:142-147,283`; the residue in code is one test-file comment: `tests/core/tst_workspace_containers.cpp:5` |
| `Workspace.h` hands the job to Cluster F explicitly | `libs/core/include/corbomite/core/Workspace.h:248-253` — *"Left/right sidebars still live in `CorbomiteMDI`; when Cluster F migrates them into the Workspace tree, add honest accessors then."* |
| The sidebar is a `QSplitter` outside the KDDW tree | `src/mdi/CorbomiteMDI.h:212` — `class Sidebar : public QSplitter`, with `ToolView` (`:89`) and `KMultiTabBar` |
| `CorbomiteMDI` is itself a second `MainWindow` class (a naming collision already filed) | punch-list §E, release-hygiene D.1 — `src/mdi/CorbomiteMDI.h:77-89` |
| The "dirty-bit hack" the stub complained about is **already gone** — replaced by a blind unknown-root passthrough | `Workspace.h:358-366` (`m_unknownRoot`), `Workspace.cpp:895-911`, covered by `tests/core/tst_workspace_session.cpp:701-703` (`unknownRootKeysSurviveRoundTrip`) |
| **Which means `left`/`right` already round-trip** — they are simply not modelled | This is the important correction to the old item-10 rationale: data safety is done; what is missing is *understanding*, *rendering*, and *manipulation* |
| The plugin-API parity target is Obsidian's `workspace.leftSplit.collapse()` | Named in the stub's item 10 and in `docs/obsidian-audit/domains/workspace.md` |
| Obsidian's `left`/`right` are full split sub-trees in `workspace.json`, not a single width | Confirmed against a real Obsidian-authored vault during Cluster L5 (see the P0 closeout, punch-list:391) |

---

## 4. Scope

### In scope

- Verify the real `.obsidian/workspaces.json` schema against an Obsidian vault
  and write an **addendum** correcting `VAULT-FORMAT.md §.obsidian/workspaces.json`
  and `domains/workspace.md §13 Q5`.
- A `Workspaces` command surface: save current layout as `<name>`, load,
  delete, list (with a picker).
- Persistence with an explicit schema tier decision (see Q2).
- Left/right sidebar modelled as `Workspace` objects with honest accessors
  (`leftSplit()`/`rightSplit()` returning real containers, or an honest
  documented absence — **never** a `nullptr`-returning accessor pretending to
  be Obsidian-shaped, which is what Cluster L3 deleted).
- Whatever `workspace.json` handling of `left`/`right` follows from that
  modelling decision, replacing or extending the passthrough.

### Out of scope

- Full Obsidian `left`/`right` node fidelity if the spec review decides the
  cost is wrong — the passthrough already guarantees no data loss.
- Mobile drawer layouts (`workspace-mobile.json` — L's non-goals).
- Multi-vault-per-process (`vaultId` namespacing stays).
- Renaming `CorbomiteMDI` → that is release-hygiene D.1 and should be done
  *by that item*, not smuggled in here. If the substrate work makes the rename
  unavoidable, coordinate rather than absorb.

---

## 5. Open questions (must be answered before this becomes a dispatch)

- **Q1 — schema verification.** Who extracts the real `workspaces.json`
  schema, and from what? (The audit corpus moved to
  `/home/clinton/bin/ObsidianRAW/audit/renamed/` per
  `addenda/2026-06-10-taxonomy-and-bases-corrections.md` — confirm it still
  exists, since several audit paths in the 2026-10-01 appraisal pointed at
  checkouts that no longer do.) Until answered, Corbomite must not *write* the
  file.
- **Q2 — schema tier for Corbomite's own named layouts.** Obsidian's file is
  Obsidian's. Corbomite's named-layout store should probably be a
  **Corbomite-private key** (the tier-2/tier-3 split Cluster L2 built), not
  `.obsidian/workspaces.json`, *unless* Q1 proves the shapes match. Get this
  wrong and Corbomite destroys a user's Obsidian named workspaces.
- **Q3 — substrate or no substrate.** Model sidebars into the KDDW tree, or
  keep `CorbomiteMDI::Sidebar` and expose honest accessors that *adapt* it?
  The second is far cheaper and satisfies `workspace.leftSplit.collapse()` with
  a real (non-`nullptr`) object. Recommend **deciding this before writing any
  code**, and that the adapt route be seriously costed first.
- **Q4 — crash safety.** If Q3 lands as a migration, the teardown discipline
  from Cluster L Phase L1 (`Workspace::destroyLeaves`) is the pattern to copy,
  and ASAN must be clean before the phase closes. This is the single most
  likely way this work introduces a regression of the 2026-06-10 SIGSEGV class.
- **Q5 — plugin surface.** Does Workspaces need a `PluginContext` accessor
  (`workspaces()->save(name)`)? Obsidian's is an internal-plugin method, and
  Corbomite's plugin API has no equivalent. Cheap to add, but it is a public
  surface and belongs in the same review as Cluster D.5's.

---

## 6. Work breakdown (shape, not dispatch — contingent on §5)

### Phase S0 — Spec + schema verification (blocking, ~1–2d)

- [ ] **S0.1** Locate and verify the Obsidian source for `workspaces.json`
      (Q1). Record what was actually found, including "could not verify".
- [ ] **S0.2** Write `docs/obsidian-audit/addenda/2026-10-01-workspaces-json-schema.md`
      in the house format, and link it from `00-taxonomy.md` §Addenda per the
      addenda README's linkage ritual.
- [ ] **S0.3** Decide Q2 in writing, citing the L0 compat doctrine
      ([`../specs/2026-08-17-workspace-compat-boundary.md`](../specs/2026-08-17-workspace-compat-boundary.md)):
      compatibility lives at the file-format boundary, so *Corbomite's own
      named layouts* should live in Corbomite's own tier unless the schemas are
      proven identical.
- [ ] **S0.4** Answer Q3 in writing; if the adapt route wins, S2 disappears.
- [ ] **S0.5** Only after S0.1–S0.4: write the spec file for this cluster under
      `docs/superpowers/specs/`, and get it reviewed.

### Phase S1 — Corbomite-native named layouts (~2d, independent of S2)

- [ ] **S1.1** A `Workspaces` service: save / load / delete / list by name.
- [ ] **S1.2** Reuse the Cluster L2 writer rather than adding a second
      serializer (the split-brain disease L2 cured). Verify whether
      `Workspace::serialize()` can be pointed at a named target without
      touching the live-session path.
- [ ] **S1.3** Commands via Cluster O's command/provider mechanism, surfaced in
      the command palette (`KCommandBar` picks up `CommandRegistry` commands —
      `src/app/MainWindow.cpp:2073-2082`) and a picker dialog.
- [ ] **S1.4** Obsidian-`workspaces.json` **read-only** support, if S0.2 says the
      shape is legible. Never write it this phase.
- [ ] **S1.5** `tst_named_workspaces` — save/load/delete/list round trip,
      name collision policy, corrupt-entry tolerance.

### Phase S2 — Sidedock substrate (only if Q3 chose migration, ~4–7d)

- [ ] **S2.1** Inventory every live caller of `CorbomiteMDI::Sidebar` and
      `ToolView` before touching anything. Note that `MainWindow.cpp` passes
      the sidebar into plugin view mounting
      (`examples/note-stats-plugin/CMakeLists.txt` documents the `ViewRegistrar`
      right-sidebar mount path) — that is the migration's blast radius.
- [ ] **S2.2** Either a real container type with honest `leftSplit()`/
      `rightSplit()`, or documented absence. **A `nullptr`-returning accessor is
      banned** — that is precisely what L3 deleted.
- [ ] **S2.3** Teardown discipline copied from Cluster L Phase L1
      (`Workspace::destroyLeaves`), with ASAN clean as a phase gate (Q4).
- [ ] **S2.4** Decide the `workspace.json` `left`/`right` story: modelled
      (serializer owns it) vs passthrough (keep `m_unknownRoot`). Whatever is
      chosen, keep `unknownRootKeysSurviveRoundTrip`
      (`tests/core/tst_workspace_session.cpp:701`) green.
- [ ] **S2.5** Cluster L5's outstanding soak scenarios (**drag between groups,
      split, popout, close-undo, vault switch**) must be re-run if the tree
      moves underneath them — see
      [`2026-08-17-cluster-l-workspace-stabilization.md`](2026-08-17-cluster-l-workspace-stabilization.md) Phase L5.
- [ ] **S2.6** Live eyeball for anything touching layout, keyboard, or focus.

---

## 7. Named tests

| Test | Status | Covers |
|---|---|---|
| `tst_named_workspaces` | new | save/load/delete/list; collision policy; corrupt-entry tolerance; **no write to `.obsidian/workspaces.json`** until S0.2 says otherwise (assert the file is byte-identical after a Corbomite session) |
| `tst_workspace_session::unknownRootKeysSurviveRoundTrip` | **existing** | must stay green through S2 (`tests/core/tst_workspace_session.cpp:701`) |
| `tst_workspace_serializer` (fixture set) | **existing** | add fixtures only if S2 changes what the serializer owns |
| ASAN run of the workspace/dock teardown paths | gate, not a test binary | S2.3 — the Cluster L1 discipline |
| Cluster L5 soak scenarios | manual, human | S2.5 — checkbox list lives in the Cluster L plan |

---

## 8. Definition of done

- [ ] A named layout saved in Corbomite can be restored in a later session and
      after a vault switch.
- [ ] Either a real Obsidian-authored `workspaces.json` round-trips through
      Corbomite untouched, or Corbomite demonstrably never writes the file and
      says so in the UI.
- [ ] No accessor in the public surface returns `nullptr` while pretending to
      be Obsidian-shaped.
- [ ] ASAN clean; full suite green:
      `cd build-dev && QT_QPA_PLATFORM=offscreen ctest -E "benchmark|perf" --output-on-failure -j 10`
- [ ] **Human live eyeball** for any layout/tab/sidebar interaction touched
      here, and Cluster L5's scenarios re-run if S2 moved the tree.
- [ ] The spec from S0.5 exists and was reviewed before any of §6 was executed.

---

## 9. Audit references

- [`docs/obsidian-audit/VAULT-FORMAT.md`](../../obsidian-audit/VAULT-FORMAT.md) §`.obsidian/workspaces.json` (line 362) — **schema inferred, Q5 unresolved**. Read the caveat before anything else.
- [`docs/obsidian-audit/domains/workspace.md`](../../obsidian-audit/domains/workspace.md) §13 Q5 (schema), §2 (`left`/`right` split shape — **note the 2026-06-10 and 2026-10-01 corrections**), §3 (workspaces is an internal plugin, not the workspace domain).
- [`docs/obsidian-audit/addenda/2026-06-10-workspace-serialization-corrections.md`](../../obsidian-audit/addenda/2026-06-10-workspace-serialization-corrections.md) and [`2026-10-01-workspace-split-direction-correction.md`](../../obsidian-audit/addenda/2026-10-01-workspace-split-direction-correction.md) — read **before** touching the serializer. The second one exists because getting a frozen doc wrong cost a P0.
- [`docs/audit-2026-04-26/workspace.md`](../../audit-2026-04-26/workspace.md) §"Obsidian-shaped accessors" — the original `leftSplit()`/`rightSplit()` finding.
- [`docs/superpowers/specs/2026-08-17-workspace-compat-boundary.md`](../specs/2026-08-17-workspace-compat-boundary.md) — the L0 doctrine that decides Q2.
- [`superpowers/plans/INDEX.md`](INDEX.md) §"Closed in this scheme", Cluster C row — *"Sidedock-as-tree + named-workspaces reassigned to F"* (the 2026-04-27 decision; there is no `cluster-c.md` retro file, so the INDEX row is the citation of record).