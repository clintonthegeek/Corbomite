# Cluster M — Canvas authoring parity (Graffodil rebase first)

**Opened:** 2026-08-19. **Type:** Full plan. **Track:** strategic cluster.
**Decision of record:** user chose **"Graffodil rebase first"** over extending the
hand-rolled `CanvasScene` (2026-08-19 session) — the substrate migration is Phase M1,
and all authoring features build on Graffodil's tool/anchor/snap plumbing.

## Goal

Corbomite opens Obsidian-authored `.canvas` files faithfully (interop bundle shipped
2026-05-28; order-preservation + vault-root path fixes 2026-08-19) but gives the user
almost no way to **author** one. Close the authoring gap against Obsidian's canvas
feature catalogue (`docs/obsidian-audit/domains/canvas.md` §9) so a user can build
canvases like `business-model-canvas-for-obsidian` / `design-research-vault` from
scratch: create text/file/group nodes fluidly, draw and reconnect edges, snap and
align, and have canvas links participate in the vault graph.

**Explicit non-goals (deferred, decision-gated — see §Deferred):** per-card content
fidelity strategy (audit §11.3), link nodes (webview dependency), byte-exact
serializer parity.

## Where we are (verified against code 2026-08-19)

Working today (`libs/canvas`, ~3.8k LOC + `src/canvas/` app shell):
- `.canvas` round-trip is **lossless and diff-stable** (unknown-field passthrough,
  default omission, V5 side self-heal, integer rounding, node/edge order preserved).
- `SelectMoveTool`: click-select, shift-toggle, marquee, move, edge/corner resize
  (`resizeModeAtPos`), Delete (single compound undo), arrow nudge (1px/10px), Ctrl+A.
- Context menus: Edit / Duplicate / Delete / Color (6 presets) on cards; Edit Label /
  Reverse Direction / Delete on edges; New Text Card / New Group on empty space.
- Inline editing via `QGraphicsProxyWidget` (`QTextEdit`) for text cards, file cards,
  group labels; `QUndoStack` with 8 command classes; PNG **and** SVG export (ahead of
  Obsidian); middle-drag pan, wheel zoom, Home = zoom-to-fit.

Broken / missing (the authoring cliff):
1. **File cards are not selectable or movable.** `SelectMoveTool::mousePressEvent`
   dynamic_casts only `TextCardItem`/`GroupItem`; a click on a `FileCardItem` falls
   through to rubber-band. The user's reference canvases are ~all file cards — they
   literally cannot be rearranged. (Same per-type-cast disease in
   `CreateEdgeTool::findNearCard`, `CanvasTool.cpp:478`.)
2. **No edge-creation affordance.** `CreateEdgeTool` (press/drag/release + preview
   line + undo command) is complete but **dead code** — never instantiated
   (`CanvasScene.cpp` only creates `SelectMoveTool`). No hover connection points.
3. **No creation flows** beyond the two context-menu entries: no double-click-empty →
   text card, no drag-drop files → file nodes, no paste (text/URL/canvas-JSON/image),
   no Alt-drag duplicate, no card-menu toolbar, no "New file card" at all.
4. **No snapping** — grid is drawn but not snapped-to; no object alignment guides.
5. **No edge endpoint reconnect**, no direction submenu (only Reverse), drop-on-empty
   does nothing.
6. Group semantics diverge: center-test membership (Obsidian: full containment,
   recomputed at grab), no `zIndex = -area` stacking.
7. **No viewport persistence** (Obsidian: `{x, y, zoom(log2)}` in workspace leaf
   eState), no read-only lock, no `jump-to-group`/`convert-to-file` commands.
8. **Canvas links don't feed the metadata graph** — no backlinks/unresolved-links
   participation, no rename rewrite of `file`/`background`/`subpath`.
9. i18n convention violated throughout canvas menus (`QStringLiteral` for
   user-visible strings).

## Audit references

- `docs/obsidian-audit/domains/canvas.md` — §9.2–9.6 (chrome/edges/creation/
  selection/viewport: the feature spec for M2–M5), §8 invariants 2–5 & 10 (edge
  anchors, defaults, group semantics, V5-vs-A3 side algorithms), §7 + §9.9 (link
  index, M6), §11.2 (the Graffodil library/consumer boundary M1 executes), §13 open
  questions 1–2.
- `docs/superpowers/plans/archive/2026-04-14-cluster-p-graffodil-adoption-SCOUTING.md`
  (archived; use as background only — Graffodil has moved from v0.1.0 to v0.2.2 since).
- `~/dev/Graffodil/docs/ROADMAP.md` — phases 1–5 + 6c (edge labels) + 6f
  (graph-editor affordances) complete; **6d (CMake install/export) still open** and
  named as the consumer-migration gate.
- `~/dev/Graffodil/docs/corbomite-integration-feedback.md` — our own 2026-04-14
  requirements doc; 6f was built from it.

## Target classes

| Area | Classes |
|---|---|
| Rebase (M1) | `Canvas::CanvasScene` → subclass `Graffodil::GraphScene`; `TextCardItem`/`FileCardItem`/`GroupItem` implement `Graffodil::IGraphNode` (+`IRenderableCache`); `EdgeItem` → `Graffodil::GraphEdgeItem` + a `CanvasBezierStrategy : EdgePathStrategy`; tools → `Graffodil::{SelectMoveTool, CreateEdgeTool, PanZoomTool, DefaultGraphTool}`; `CanvasView` slims to a shell |
| Kept as-is | `CanvasDocument`/`CanvasTypes` (all of §3 disk contract), `CanvasCommands` (`QUndoStack` — now driven by Graffodil tool intent-signals), export paths, `CanvasViewTab`/`CanvasFileView` |
| App side (M5/M6) | `WorkspaceSerializer` (eState viewport), `SessionManager` tier-3 (read-only flag), `MetadataCache`/`SQLiteIndex`/`LinkResolver` + `FileManager` rename rewriting (M6), `MainWindow` commands |

## KDE/Qt prior art

- Graffodil itself is the prior art for scene/tools/anchors/minimap (built for this).
- KDE `kdiagram`/`calligra` flow layers for snap-guide rendering conventions;
  `QGraphicsItem::ItemIgnoresTransformations` for zoom-constant chrome (audit §11.3
  table); `QVariantAnimation` for eased camera.

## Work breakdown

### Phase M0 — Graffodil readiness gate (cross-repo, small) — **CLOSED 2026-08-19**

> **Resolution:** Graffodil replied same day
> (`~/dev/Graffodil/docs/handoff/2026-08-19-reply-to-corbomite-canvas-adoption.md`):
> submodule consumption blessed (6d not required; Graffodil's own `origin` IS
> GitHub, no mirror-lag risk); pin **`v0.2.3`** (`dd7667de`), tagged same day with
> the M3 gesture gap already fixed upstream — `CreateEdgeTool` supports
> press-drag-release (use `setDragOnly(true)` for Obsidian-strict cancel) and a new
> `edgeDroppedOnEmpty(source, anchorId, scenePos)` intent signal covers
> release-on-empty (tool resets before emitting; self-loop is a silent cancel, not
> a drop). `IAlignmentStrategy` confirmed consumer-side. No eased camera upstream —
> Corbomite drives `QVariantAnimation` over `applyZoom()` in M5 as planned.
> Executed: submodule added at `libs/graffodil` (GitHub URL, checked out
> `v0.2.3`), `add_subdirectory` wired after markoff-family (demo/tests auto-OFF
> via `PROJECT_IS_TOP_LEVEL`), dev preset builds all six `libgraffodil-*.a`,
> full offscreen suite **313/313**. Test-side note for M1+: spying on
> `edgeDroppedOnEmpty`/`edgeRequested` needs
> `qRegisterMetaType<IGraphNode*>("IGraphNode*")`.
- Write a handoff doc (`docs/handoff/2026-08-19-to-graffodil-for-canvas-adoption.md`)
  asking Graffodil to land **6d (CMake install/export)** or bless submodule
  `add_subdirectory` consumption; per `~/dev/CLAUDE.md` symlink rule the consumption
  mechanism is a **git submodule** (Codeberg-primary Graffodil; mirror question same
  as markoff-family precedent) unless 6d's installed-package route is preferred.
- Verify in Graffodil: `PanZoomTool` zooms about cursor + eases (audit §11.2 flag);
  anchor model supports exactly-4 face-midpoint compass anchors; `CreateEdgeTool`
  supports live nearest-face snap (A3 semantics) distinct from load-time V5.
  File Graffodil issues/specs for any gap — **Graffodil-side work happens in
  Graffodil's repo under its own roadmap**, not here.
- Exit: pinned Graffodil revision building inside Corbomite's dev preset.

### Phase M1 — Substrate rebase, feature-frozen

**Full migration contract (read it before touching code — every class mapping,
signal→command table, file disposition, and test name is specified there):**
[`../specs/2026-08-19-cluster-m1-graffodil-rebase-design.md`](../specs/2026-08-19-cluster-m1-graffodil-rebase-design.md).
Only three intended behavior changes (spec §7): file cards become
selectable/movable, wheel=pan + Ctrl+wheel=zoom, 24px edge hit zone.

Tasks (sequence matters; each ends with a green build + the tests named in spec §5):
- [x] **M1.0 Verification pass** — answer spec §6 V1–V4 by reading
      `libs/graffodil/src/core/src/{GraphScene,CompositeTool,DefaultGraphTool,SelectMoveTool}.cpp`;
      append answers to the spec under §6 before writing code. Answers landed
      as spec §6a (2026-08-19). V3 concluded `DefaultGraphTool` can't be used
      as-is (see divergence note below the checklist).
- [x] **M1.1 `CanvasNodeItem` base** (spec §3.1) — new class; rebase
      `TextCardItem`/`FileCardItem`/`GroupItem` onto it; delete
      `ConnectableItem.h`, the 3 duplicated `ResizeMode` enums and
      `resizeModeAtPos` bodies, and `connectionPoint(Side)`. Anchor identity:
      `sideToString()` strings == Graffodil compass anchor ids (spec §2).
- [x] **M1.2 `CanvasEdgeItem`** (spec §3.2) — rewrite `EdgeItem` as a
      `Graffodil::GraphEdgeItem` subclass: stock `BezierPathStrategy`
      (identical math to current: `min(dist*0.4, 80)`), `TriangleTerminus`/
      `NoTerminus` per `fromEnd`/`toEnd`, Graffodil 6c label,
      `setHitWidth(24)`, `edgeId()` override returning the document id.
      Test: `testEdgeIdPreserved`.
- [x] **M1.3 Scene rebase** (spec §3.4) — `CanvasScene : Graffodil::GraphScene`;
      delete item hashes + mouse/key tool dispatch (keep only the edit-proxy
      pre-check); keep every public method the app calls; i18n-sweep all
      user-visible menu strings.
- [x] **M1.4 Tools** (spec §3.5, §4.3) — delete `CanvasTool.{h,cpp}`;
      consumer `CanvasResizeTool` (verbatim port of the resize math,
      `kMinSize=40`, emits `resizeCommitted`) routed by resize-zone predicate
      ahead of select/move in a bespoke `Graffodil::CompositeTool` (see
      divergence note — `DefaultGraphTool` itself couldn't be used); slim
      `CanvasView` (spec §3.7).
- [x] **M1.5 Undo wiring** (spec §3.6 table) — `dragBegan/dragEnded` →
      `CmdMoveCards`; `deleteRequested` → compound remove; `resizeCommitted` →
      `CmdResizeCard`. Tools never touch `CanvasDocument`.
- [x] **M1.6 Tests** (spec §5) — port `tst_canvasscene` to new seams keeping
      every assertion; add the six named new tests (incl.
      `testFileCardSelectableAndMovable`, which must FAIL against pre-M1 code —
      prove it, then fix forward). All 19 slots green
      (13 ported + 6 new); full offscreen suite 313/313.
- [x] **M1.7 Live eyeball gate — CLOSED 2026-08-19.** User live-tested against
      the running dev build: select/move/resize every node kind, inline
      edits, export PNG+SVG, undo/redo all confirmed working. Found + fixed
      two bugs during the pass (both user-confirmed fixed after re-test):
      (1) app-wide Ctrl+Z/Redo KActions never checked for an active canvas
      tab, so global Undo/Redo silently no-op'd on canvas moves even though
      `CanvasView`'s own keyPressEvent handler was correct — added
      `MainWindow::activeCanvasView()` (`fb672574`); (2) PNG/SVG export
      background rendered black instead of matching the on-screen white —
      `CanvasScene` never set `backgroundBrush()` so it fell back to
      `QGraphicsScene`'s black default — fixed with an explicit
      `setBackgroundBrush(Qt::white)` (`2e06b966`). Canvas's total lack of
      dark-mode/theme awareness (neither path consults `ThemeService`) is
      punch-listed as a follow-up, not fixed here (out of M1's frozen
      scope). Byte-identical move+undo+save diff not explicitly re-checked
      but implied by the undo fix landing correctly + all offscreen
      round-trip tests green — flag if a real diff-check surfaces drift.

**M1 divergences from the literal spec text (all noted per the spec's own
"note, don't silently improvise" instruction; none change the class-mapping
intent, only the assembly mechanics):**

1. **§3.5's `Graffodil::DefaultGraphTool` is not used at all.** M1.0/V3 found
   `DefaultGraphTool`'s constructor pre-registers its own select/pan mouse
   routes via plain `addMouseRoute` (append-only; only `addAnchorRoute`
   prepends). A `CanvasResizeTool` route added afterward would always lose to
   `m_select`'s already-registered plain-left-button route. Per the spec's own
   fallback instruction ("build a bespoke CompositeTool from parts"),
   `CanvasScene` now owns its own `Graffodil::SelectMoveTool` +
   `Graffodil::PanZoomTool` + `Graffodil::CompositeTool`, with the resize
   route registered first. Same key/pan bindings as `DefaultGraphTool` ships,
   only the assembly differs.
2. **Wheel-zoom modifier is Ctrl (PanZoomTool's own default), not
   `DefaultGraphTool`'s `NoModifier`.** Spec §7 item 2 states intent as
   "bare wheel scrolls, Ctrl+wheel zooms" and attributes that to "Graffodil's
   default" — but `DefaultGraphTool`'s constructor actually sets
   `setZoomWheelModifier(Qt::NoModifier)` ("plain wheel zooms"), contradicting
   both its own inline comment and the spec's stated intent. Since M1 already
   builds a bespoke `CompositeTool` (divergence 1), this scene simply leaves
   `PanZoomTool`'s built-in default (`Qt::ControlModifier`) untouched, which
   matches the spec's actual intent rather than `DefaultGraphTool`'s literal
   behavior.
3. **Ctrl+A (select-all) and arrow-key nudge (1px / 10px+Shift) are
   preserved in `CanvasScene::keyPressEvent` as a consumer-side stopgap.**
   `Graffodil::SelectMoveTool` only owns Delete/Backspace/R — it has no
   select-all or arrow-nudge handling. These were both live pre-M1 features
   (plan "Working today" list) and the spec is silent on them for M1 (M4.2
   formally redesigns nudge with grid-snap stepping later). Rather than
   silently regressing them mid-migration, they're re-implemented directly
   in `CanvasScene::keyPressEvent`, guarded on `!focusItem()` so they never
   steal keys from an in-place label/text editor.
4. **`CanvasNodeItem::itemChange`/`setGeometry` call
   `scene()->adjustEdgesForNode()` directly** (the §3.1 conditional hook),
   confirmed necessary by V2: `GraphScene` does not self-subscribe to node
   position changes for edges (only groups get that treatment). This also
   required adding the same call inside `setGeometry()` for the width/height-
   only case (a right/bottom-edge resize that doesn't move `pos()` would
   otherwise leave edges stale — `itemChange(ItemPositionHasChanged)` alone
   doesn't fire for a size-only change).
5. **Class file naming:** `EdgeItem.h`/`EdgeItem.cpp` keep their filenames
   (per spec §8's disposition table) but the class is documented as
   `CanvasEdgeItem` via a `using CanvasEdgeItem = EdgeItem;` alias — the spec
   text alternates between calling it `EdgeItem` (§3.2 heading says "rename
   of `EdgeItem`") and `CanvasEdgeItem` elsewhere; resolved in favor of
   keeping the existing `Canvas::EdgeItem` symbol (zero call-site churn) with
   the alias available for spec-literal references.

**M1.0–M1.6 commit note:** landed as two commits rather than one-per-task —
the rebase (M1.0 spec-answers + M1.1–M1.5 code) is one deeply interdependent
change (CanvasScene's constructor alone reaches into every new class), so
splitting it into individually-green intermediate commits would have meant
building throwaway stub scaffolding with no lasting value; M1.6 (the new
tests) is the one piece that was genuinely separable and landed as its own
commit.

Exit: current feature set intact live, suite green (313 baseline + new tests),
export unchanged, `git rm`'d files match spec §8 disposition table.

### Phase M2 — Node creation flows (audit §9.4)

Every creation path goes through `CmdAddCard` on the scene's undo stack; new
nodes get `CanvasDocument::generateId()` (16 lowercase hex) and are selected
after creation. Defaults (Appendix A): **text 250×60, file 400×400**.

- [x] **M2.1 Double-click empty → text card** — landed `10175197`.
      `CanvasScene::mouseDoubleClickEventBackground()` override: `CmdAddCard`
      (text, 250×60, click point = card center) → select → `beginInlineEdit`.
      Test: `testDoubleClickEmptyCreatesTextCardInEditMode`. **Heap-corruption
      bug found during M1.7/M2 live review, root-caused and FIXED
      (`8bfefddc`):** destroying `CanvasScene` while an inline text/group-
      label edit was still open (e.g. closing a tab mid-edit) double-deleted
      a scene item — deleting the focused edit widget mid-teardown
      synchronously re-fires `QApplication::focusChanged`, which was still
      wired to `finishInlineEdit()`/`finishGroupLabelEdit()`, reentering them
      while `QGraphicsScene`'s own destructor was already deleting the same
      item. Fixed with an explicit `CanvasScene` destructor that finishes any
      open edit before base-class item cleanup starts. New regression test:
      `testSceneDestroyedWhileEditingDoesNotCrash`.
- [x] **M2.2 Context menu create** — landed `1a01a19c` + app-wiring
      follow-up `476bede4`. Menu entries renamed to i18n'd "New text card" /
      "New file card…" / "New group" (also fixed text-card default height
      100→60 to match Appendix A, a latent bug). New
      `Canvas::CanvasFilePickerDialog` (libs/canvas) fuzzy-filters/-ranks a
      flat candidate `QStringList` live via `Corbomite::FuzzyMatcher` (same
      matcher `CompletionPopup`/`QuickSwitcher` use). `CanvasScene` gains
      `FilePickerRequestor` (injectable callback) +
      `createFileCardViaPicker()`, pushing `CmdAddCard` for a 400x400 file
      node with the path stored vault-relative. Tests:
      `testContextMenuCreatesFileCard`,
      `testContextMenuFileCardCancelledPickerCreatesNothing` (both inject a
      fixed picker result, no real modal driven). **Divergence:** the
      candidate list is *not* sourced via `Vault::getMarkdownFiles()`/
      `getFiles()` as the plan suggested — `CanvasViewTab` (src/canvas/)
      only ever receives a `vaultRoot` path string, no `Vault*` is reachable
      at that layer today, so `CanvasViewTab` wires the requestor to a
      `QDirIterator` scan of the vault root for markdown + common
      attachment extensions instead. **Partial fix landed (`3a0fe634`):**
      dot-prefixed path segments (`.obsidian`, `.git`, `.trash`, …) are now
      skipped, so internal config/plugin JSON no longer clutters the picker.
      Still not backed by `Vault`'s real exclude patterns/in-memory file
      tree — punch-listed (`[canvas][cluster-m]`) as follow-up work needing
      a `Vault*` threaded down from `CanvasFileView`.
- [x] **M2.3 Drag-drop** — landed `98630a52`.
      `CanvasScene::{dragEnterEvent,dragMoveEvent,dropEvent}`:
      `text/uri-list` from FileExplorer/OS → one file node per file at drop
      point, multiple files laid out in a grid (Obsidian: grid of file nodes;
      spacing = one gridSpacing gap, 20px), paths made vault-relative when
      inside the vault, else rejected (M1 scope: no copy-into-vault; note in
      punch list if users hit it); `text/plain` drop → text card. One compound
      undo command per drop. Requires `setAcceptDrops` on view+scene wiring.
      Tests: `testDropSingleFileCreatesFileCard`, `testDropTextCreatesTextCard`.
- [x] **M2.4 Clipboard** — landed `86b11f57`. Copy (Ctrl+C): selection serialized as `.canvas`-
      shaped JSON `{nodes:[…],edges:[…]}` (edges only when both ends selected)
      to `QMimeData` `text/plain` — this is what Obsidian puts on the
      clipboard, enabling cross-app paste. Paste (Ctrl+V): if clipboard parses
      as canvas JSON → clone + **re-ID every node/edge** (16-hex, remap edge
      endpoints), offset +16px, single compound command; else plain text →
      text card at viewport center. Cut = Copy + delete-compound. Tests:
      `testCopyPasteRoundTripReIds`, `testPastePlainTextCreatesCard`.
- [x] **M2.5 Alt-drag duplicate** — landed `181c6997`. New
      `CanvasDuplicateDragTool` routed ahead of `SelectMoveTool` (predicate:
      Alt modifier + press on an already-selected node, mirroring
      `findResizeTarget`'s pattern as `findAltDragDuplicateTarget`). Clones
      created directly in the scene at press time (fresh 16-hex ids, cloned
      internal edges remapped) and dragged live; one compound
      `CmdAddCard`/`CmdAddEdge` undo step commits on release. Test:
      `testAltDragDuplicates`. **Missing drag-threshold gate found + fixed
      (`ddde36d5`):** a press+release with zero mouse movement was
      committing a duplicate at the original position. Added a
      `QApplication::startDragDistance()` gate — below threshold, the
      eagerly-created clones are removed and the original selection
      restored instead, so a bare Alt+click behaves like a plain click.
      Test: `testAltClickWithoutDragDoesNotDuplicate`.
- [x] **M2.6 New-canvas command** — landed `92cab583`. New
      `FileManager::createNewFile(parent, name, ext, content)` generalizes
      `createNewMarkdownFile` (now delegates to it), reusing the existing
      `collisionFreeName()` dedup rule; exposed via
      `FileManagerProxy::createNewFile`. `MainWindow::createNewCanvas()`
      replaces the pre-existing `file_new_canvas` action (which prompted for
      a name then saved straight to `<name>.canvas` with **no collision
      check at all** — a real bug this incidentally fixes) with
      create-`Untitled.canvas`-then-open-then-rename.
      `FileExplorerView::onNewCanvasIn()` mirrors it as "New Canvas Here" /
      "New Canvas" context-menu entries. **Divergence:** there is no
      existing inline-rename-on-creation flow to mirror —
      `createNewNote()`/`onNewNoteIn()` both prompt for a name via
      `QInputDialog` *before* creating the file, not after. Building a true
      inline-edit-on-creation mechanism was out of scope; this reuses
      `FileManager::promptForFileRename()` (the same modal F2/"Rename..."
      dialog) as the closest existing "start a rename" primitive. No named
      test in the plan for this task — covered by the full offscreen suite
      staying green.
- Exit: user can build the business-model canvas layout from an empty file
  without touching JSON — live-verified by actually rebuilding ~6 cards of it.
- **Phase M2 CLOSED 2026-08-19.** User live-tested all six flows against the
  running dev build and confirmed working: double-click-to-create, context-
  menu file picker, drag-drop, clipboard copy/paste (incl. the +16px offset
  on paste), Alt-drag duplicate, and the New Canvas command/dialogs. Three
  bugs surfaced and fixed during the pass are detailed inline above:
  the `~CanvasScene` heap-corruption double-delete (`8bfefddc`), the
  file-picker dot-path leak (`3a0fe634`), and the Alt-drag zero-movement
  duplicate (`ddde36d5`). Remaining known gaps, both punch-listed rather
  than fixed here (out of M2 scope): file-picker candidates aren't
  `Vault`-backed (`[canvas][cluster-m]`), and canvas has no dark-mode/theme
  awareness at all (`[canvas][cluster-m]`, found during M1.7).

### Phase M3 — Edge authoring (audit §9.3, §8 inv. 2/3/10)

Graffodil v0.2.3 supplies the gesture machinery: `CreateEdgeTool` does
press-drag-release AND click-click (use `setDragOnly(true)` for
Obsidian-strict cancel), snaps preview to nearest anchor with
`AnchorHighlight` dots, emits `edgeRequested(src, srcAnchor, tgt, tgtAnchor)`
and `edgeDroppedOnEmpty(src, srcAnchor, scenePos)` — the tool never mutates
the model. Reminder: `qRegisterMetaType<Graffodil::IGraphNode*>` in tests.

- [ ] **M3.1 Obsidian-exact bezier** — new `CanvasBezierStrategy :
      Graffodil::EdgePathStrategy` replacing M1's stock one: control offset
      `clamp(dist/2, 70, 150)` along each anchor's `outwardDirection`, path
      inset 7px from the face, straight `L` fill segment when that end has no
      arrowhead. Arrow terminus: Obsidian's triangle is 13×10.4 px
      (`polygon 0,0 / 6.5,10.4 / −6.5,10.4`), rotated per side, placed AT the
      face — implement as `CanvasArrowTerminus : TerminusStyle` if
      `TriangleTerminus` visibly differs at the M1 eyeball (else keep stock).
      Test: `testBezierMatchesObsidianConstants` (assert control points for a
      known geometry).
- [ ] **M3.2 Hover connection points + create gesture** — route a
      `CreateEdgeTool` (dragOnly=true, `setAnchorHoverRadius(12)`) into the
      composite tool with a predicate "press within N px of a face-midpoint
      anchor of a hovered/selected node" (N=12 to match hover radius; this is
      how the same left-button press serves both select-move and edge-create,
      Obsidian-style connection dots on the interaction layer). Handler for
      `edgeRequested`: build `CanvasEdge` (id=generateId, sides =
      `sideFromString(anchorId)`, defaults fromEnd=none/toEnd=arrow) →
      `CmdAddEdge`. Works on all three node kinds by construction. Tests:
      `testEdgeCreateDragBetweenFileCards`, `testEdgeCreateDefaultsToArrowHead`.
- [ ] **M3.3 Drop-on-empty → create-and-connect menu** — connect
      `edgeDroppedOnEmpty`: popup menu at `scenePos` (i18n: "New text card",
      "New file card…", "Cancel"); on choice, one compound command =
      `CmdAddCard` + `CmdAddEdge` (toSide = side facing the source, via the
      existing `pickSideToward()`). Obsidian parity: edge-drag to empty
      offers node creation. Test: `testEdgeDropOnEmptyCreatesConnectedCard`.
- [ ] **M3.4 Endpoint reconnect** — drag an existing edge's endpoint
      (hit-test: within 8px of the edge terminus point, before the edge-create
      route) → new `ReconnectEdgeTool` (consumer, ~100 LOC, mirrors
      CreateEdgeTool's preview) → on release over a node: new
      `CmdReconnectEdge` (add to `CanvasCommands`: stores old/new
      node+side for that end); over empty: `CmdRemoveEdge` (Obsidian
      semantics: dropping an endpoint on nothing deletes the edge). Tests:
      `testReconnectEdgeEndpoint`, `testEndpointDropOnEmptyDeletesEdge`.
- [ ] **M3.5 Direction menu** — edge context menu gains
      "Direction ▸ Nondirectional / Unidirectional / Bidirectional" mapped to
      (`fromEnd`,`toEnd`) = (none,none)/(none,arrow)/(arrow,arrow), via new
      `CmdSetEdgeEnds`; keep "Reverse direction" (swaps
      fromNode/fromSide↔toNode/toSide — `GraphEdgeItem::reverse()` handles the
      visual; the command updates the document). Wire
      `SelectMoveTool::reverseRequested` (R key) to the same command. Test:
      `testDirectionMenuWritesEndFields`.
- Exit: draw, label, redirect, reconnect, and delete edges entirely with the
  mouse; a save after each operation is Obsidian-clean (concrete sides,
  defaults omitted — assert via existing `tst_canvasdocument` invariants).

### Phase M4 — Move/snap/selection polish (audit §9.5, §9.2)

- [ ] **M4.1 `CanvasAlignmentStrategy : Graffodil::IAlignmentStrategy`** —
      installed via `SelectMoveTool::setAlignmentStrategy` (tool paints the
      returned guide lines itself). Two snap modes, both default ON:
      *snapToGrid* — snap the primary node's top-left to gridSpacing, where
      gridSpacing is zoom-dependent: 20/40/80/160 scene units (pick the step
      whose on-screen size ≥ ~10px: 20 at scale ≥ 1, 40 at ≥ 0.5, 80 at ≥ 0.25,
      else 160); *snapToObjects* — candidate lines = corners + centers (x and
      y independently) of all other nodes, snap within **15/scale** scene
      units, return the matched lines as guides. No-snap modifier: **Alt**
      (Obsidian: Ctrl on mac / Alt elsewhere; we ship Alt) — but Alt is taken
      by M2.5 duplicate-drag at *press* time; Alt during an already-running
      drag = no-snap (disambiguate on press vs. hold; document in code).
      Settings persisted as canvas view options (tier-3 app-data, alongside
      M5.4's read-only store; NOT in `.canvas`, NOT in workspace.json). Pure
      logic = unit-testable without a scene: `tst_canvas_alignment` (new
      file): `testGridSnapPitchByZoom`, `testObjectSnapCornerAndCenter`,
      `testToleranceScalesInverseZoom`, `testAltDisablesSnap`.
- [ ] **M4.2 Modifier polish** — Shift axis-lock on move (larger |Δ| axis
      wins); Shift aspect-lock on resize (in `CanvasResizeTool`); arrow-key
      nudge = current gridSpacing, ×5 with Shift (replaces today's 1px/10px —
      lives in the delete/nudge key handler, still one `CmdMoveCards` per
      keypress, merge-compress repeats via `QUndoCommand::mergeWith` id if
      churn is annoying). Edge auto-pan while dragging near the viewport edge
      (Obsidian: 60Hz when cursor nears wrapper edge) — QTimer-driven view
      scroll; keep simple.
- [ ] **M4.3 Group grab semantics** — replace `GroupItem`'s `itemChange`
      move-children (center-test, live) with Obsidian's model: membership
      computed **once at drag start** by full containment
      (`group.sceneRect.contains(node.sceneRect)`), captured into the drag
      set (hook `dragBegan`: if a group is in the drag set, union in its
      contained nodes). Group stacking: `setZValue(-width*height)` on every
      geometry change (bigger groups behind). Tests:
      `testGroupDragMovesFullyContainedOnly`, `testGroupZOrderByArea`.
- [ ] **M4.4 Resize/connection chrome** — visible 8 resize handles + 4
      connection dots on the selected/hovered node, zoom-constant
      (counter-scale by 1/viewScale or `ItemIgnoresTransformations` on a
      handle overlay item — one overlay retargeted to the active node,
      mirroring Obsidian's single interaction layer, NOT per-node children);
      cursor feedback per zone (SizeFDiag/SizeBDiag/SizeHor/SizeVer).
      Eyeball-gated.
- Exit: cards align as effortlessly as in Obsidian; a saved file's geometry
  stays integer + stable (no snap-induced ±1 churn on reopen).

### Phase M5 — Viewport, commands, persistence (audit §9.6, §6, §2-ephemeral)

- [ ] **M5.1 Camera** — zoom clamped to log2 ∈ [−4, 1] (scale 0.0625..2);
      zoom always about cursor (PanZoomTool already does); eased transitions
      for programmatic moves (zoom-to-fit/selection/jump): `QVariantAnimation`
      driving the view transform, ~500ms ease-out (Obsidian:
      `1 − 0.984^dt` rAF easing — an OutExpo-ish curve; eyeball-match, don't
      overthink). Keyboard/UI: Shift+1 = zoom-to-fit, Shift+2 =
      zoom-to-selection, Ctrl+= / Ctrl+− / Ctrl+0 = in/out/reset (mirror the
      editor's zoom keys, `View::keyPressEvent` precedent in markoff-canvas).
- [ ] **M5.2 On-canvas control cluster** — floating widget column,
      bottom-right of the view (plain QWidgets overlaid on the viewport, not
      scene items): lock toggle, zoom in/out/reset/fit, undo/redo, matching
      Obsidian's gear/lock/zoom/undo cluster. Actions reuse the existing
      QActions/undo stack.
- [ ] **M5.3 Viewport persistence (eState)** — save `{x, y, zoom}` (zoom =
      **log2 of scale**, x/y = scene point at viewport center) into the
      canvas leaf's ephemeral state so it round-trips through
      `workspace.json` exactly as Obsidian writes it (audit §2-ephemeral;
      grep `WorkspaceSerializer` for how markdown leaves persist
      `eState.scroll` — same channel; L0 doctrine: this is the shared
      Obsidian-schema file, field names must match). Restore on open.
      **Coordinate with Cluster L**: if L5 soak is still open when this lands,
      keep the eState diff minimal and run one Obsidian round-trip check.
      Test: `testCanvasEStateRoundTrip` (fixture with a real
      Obsidian-written canvas leaf eState).
- [ ] **M5.4 Read-only lock** — per-file, machine-local (Obsidian keeps it in
      localStorage `canvas-<path>` → for us: tier-3 app-data `session.json`-
      adjacent store, per L2's tier split; NEVER in `.canvas` or
      workspace.json). Gates: all tools except pan/zoom/select, context-menu
      mutations, inline edits, drops, paste. UI: lock button (M5.2) + padlock
      status. Test: `testReadOnlyBlocksMutations`.
- [ ] **M5.5 Commands** — palette: "Canvas: Jump to group" (fuzzy list of
      group labels → eased zoomToBbox), "Canvas: Convert text card to file"
      (exactly one text node selected → prompt filename → write `.md` with
      the card text via FileManager → replace node type/file in one compound
      command), plus M2.6's "Create new canvas" if not yet landed. Test:
      `testConvertToFileSwapsNodePreservingGeometry`.
- Exit: reopening a canvas restores the view; lock survives restart; both
  commands work from the palette; an Obsidian-written workspace.json with a
  canvas leaf opens at the right viewport (and vice versa).

### Phase M6 — Vault integration (audit §7, §9.9)

Obsidian's mechanism (audit §7): a `linkUpdaters["canvas"]` contract on the
metadata cache — canvas supplies reference iteration + rewrite; the cache
supplies rename events. Corbomite equivalent: teach the indexing pipeline
that `.canvas` is a link-bearing file type. **Run the M6 Explore prompt
(below) before starting M6.1 — the exact seams are pipeline-specific.**

- [ ] **M6.1 Index canvas references** — for each `.canvas` file, extract:
      file-node `file` paths (+`subpath`), group `background` paths, and
      `[[wikilinks]]`/`[](…)` inside text-node `text` (reuse the markdown
      link extraction the metadata parser already runs on note bodies).
      Feed them into `SQLiteIndex` as outgoing links from the canvas file →
      backlinks ("linked from My Canvas"), outgoing-links, and
      unresolved-links all light up. Test: `tst_canvas_link_index` (new):
      canvas fixture with one file node + one wikilink in a text card →
      both appear as links from the canvas.
- [ ] **M6.2 Rename rewrite** — when `FileManager` rewrites links on a vault
      rename, include canvases: update `file`/`background` fields (exact
      path match, and prefix match for folder renames), `subpath` heading/
      block renames if the existing rewrite handles them for notes, and
      wikilinks inside text-node `text`. Writes go through `CanvasDocument`
      load→mutate→save (NOT regex over raw JSON — preserves unknown fields
      and field order). If the canvas is open, the document instance must be
      the one mutated (single-writer; grep how open notes handle external
      rename reconciliation). Test: rename a note referenced by a fixture
      canvas → canvas file updated, unknown fields intact.
- [ ] **M6.3 Missing-file placeholder** — `FileCardItem` whose `file`
      doesn't resolve renders the existing empty state plus an i18n'd
      "File not found: <path>" + buttons Create / Locate… / Remove;
      auto-resolve: on vault create/rename events matching the path,
      re-render the card (subscribe via the vault watcher the scene's host
      already has — thread through `CanvasViewTab`). Test: fixture with a
      dangling file node → placeholder; create the file → card renders.
- Exit: a canvas behaves like a first-class vault citizen — its links appear
  in backlinks/outgoing/unresolved panels, renames never break it, and a
  dangling card is a recoverable state, not a blank.

## Explore-agent dispatch prompts

- *M0 / M1 mapping:* **superseded** — both were executed 2026-08-19; the answers
  ARE the design spec (`../specs/2026-08-19-cluster-m1-graffodil-rebase-design.md`).
  Only spec §6's V1–V4 verification reads remain (task M1.0, .cpp files, not headers).
- *M6:* "Trace how Corbomite indexes markdown wikilinks end-to-end (MetadataCache → MetadataParser/Worker → SQLiteIndex → LinkResolver → backlinks UI) and where FileManager rewrites links on rename (renameFileByPath and its link-rewrite pass). Identify the exact seams where a `.canvas` file type can (a) contribute outgoing links at index time and (b) receive path rewrites at rename time. Report seam functions by path:line and note whether the parser pipeline is extension-gated anywhere that would drop `.canvas` files before parsing."

## Definition of done — per task

Every checkbox task above lands as **one TDD commit** (test first where the
task names a test), builds with `cmake --build --preset dev -j 10`, and keeps
the full offscreen suite green (`cd build-dev && QT_QPA_PLATFORM=offscreen
ctest -E benchmark -j 10`). A phase closes only after its Exit line is
satisfied INCLUDING the live-eyeball items — offscreen-green alone never
closes canvas interaction work (project memory: an offscreen-green
keyboard-focus fix was previously broken live).

## Appendix A — Obsidian canvas constants card (normative numbers)

All values verified against decompiled Obsidian (`domains/canvas.md`); cite
this card in code comments as "Appendix A" rather than re-deriving.

| Constant | Value |
|---|---|
| New text node size | **250×60** |
| New file node size | **400×400** |
| Embeddable-URL (link) node size | 640×360 *(link nodes deferred)* |
| Node/edge id | 16 lowercase hex, `[0-9a-f]{16}` |
| Edge defaults | `fromEnd:"none"` (omit), `toEnd:"arrow"` (omit) |
| Bezier control offset | `clamp(dist/2, 70, 150)` world units, perpendicular to attach face |
| Edge path inset from face | 7px (straight `L` fills the gap when no arrowhead) |
| Arrowhead | triangle 13×10.4 px: `(0,0) (6.5,10.4) (−6.5,10.4)`, rotated per side, at the face |
| Edge visible / hit stroke | 2px visible / **24px** invisible hit path |
| Edge label position | bezier t = 0.5 |
| Grid snap spacing | zoom-dependent **20/40/80/160** scene units |
| Object-snap tolerance | **15 / scale** scene units; candidates = corners + centers |
| No-snap modifier | Ctrl (mac) / **Alt** (ours: Alt) |
| Arrow-key nudge | gridSpacing; **×5 with Shift** |
| Zoom clamp | log2 scale ∈ **[−4, 1]** (scale 0.0625..2); eState `zoom` stores **log2(scale)** |
| Camera easing | rAF factor `1 − 0.984^dt` (≈ ease-out; match by eye) |
| Group z-order | `zValue = −width×height` (bigger → further back) |
| Group membership | **full containment**, recomputed at grab time; no stored parent |
| Node color | `"1".."6"` preset index (see `colorFromCanvasColor`) or `#rrggbb` |
| Save debounce | ~2000 ms, force-save on unload (host-side; already Corbomite's pattern) |
| Node chrome | 8 resize handles + 4 connection points on ONE shared overlay retargeted to the active node |
| Node min size (ours) | 40×40 (`kMinSize` — Corbomite value, Obsidian's unverified; keep) |

## Appendix B — standing rules & traps (read before every phase)

1. **Tools/gestures never mutate `CanvasDocument`.** Every mutation is a
   `Cmd*` on `CanvasScene::undoStack()`. If you find yourself calling
   `document()->addNode(...)` outside a command's `redo()`, stop.
2. **Never let a `.canvas` write regress the disk contract.** Unknown-field
   passthrough, default omission, concrete edge sides, integer geometry, and
   node/edge order are all guarded by `tst_canvasdocument` — run it after any
   change that touches `CanvasNode`/`CanvasEdge` construction. New node/edge
   builders must set only real values and leave defaults absent.
3. **Anchor ids are the `sideToString()` strings** (`"top"/"right"/"bottom"/
   "left"`). Empty anchor id = Graffodil swivel mode = a `.canvas` invariant
   violation. Grep for `""` anchor arguments in review.
4. **The `dynamic_cast`-per-node-type pattern is banned.** It caused the
   file-cards-unselectable bug. Type-blind code paths go through
   `CanvasNodeItem`/`IGraphNode`; type-specific behavior goes through virtual
   methods or `nodeData().type`.
5. **Canvas groups are NOT `Graffodil::GroupItem`** (decorative,
   non-hit-testable, bounds-follow). Canvas groups are persisted, resizable,
   connectable nodes. See spec §3.3.
6. **`QSignalSpy` on Graffodil signals** needs
   `qRegisterMetaType<Graffodil::IGraphNode*>("IGraphNode*")` in
   `initTestCase` — otherwise the spy silently fails to record.
7. **i18n:** every new user-visible string is `i18n(...)`; the pre-M1 canvas
   violated this — do not copy old `QStringLiteral` menu code as a template.
8. **Ephemeral vs. document vs. machine-local state:** viewport → workspace
   leaf eState (shared Obsidian schema, L0 doctrine); snap settings +
   read-only flag → tier-3 app-data; NOTHING UI-ish ever goes into the
   `.canvas` file.
9. **Live-eyeball gate** on anything touching mouse/keyboard/focus (project
   memory `feedback_verify_ui_fixes_live`). Offscreen Qt cannot drive real
   focus chains; QGraphicsView interaction has burned us before.
10. **Stage by path** (`testvaults/` is deliberately dirty; never `git add -A`),
    build with `-j 10`, `QT_QPA_PLATFORM=offscreen` for ctest — see CLAUDE.md.
11. **Session ritual:** on picking up this cluster, read `PROJECT-STATE.md`,
    this plan's checkboxes, then the spec §6 answers (if M1.0 is done). Tick
    checkboxes as tasks land; update PROJECT-STATE "Last touched" + this plan
    at phase close (CONTRIBUTING-OPS rituals 2/3).

## Definition of done — cluster

- All six phase exit criteria met; each phase closes with the standard ritual
  (offscreen suite green **plus live eyeball** — project memory: canvas/editor
  interaction fixes must be re-tested live before closing).
- Golden round-trip test: open → author (every M2/M3/M4 gesture) → save produces
  Obsidian-clean JSON (verified against a real Obsidian re-open at least once).
- `PARITY-MATRIX.md` §5 canvas rows updated; punch-list `[canvas][NEEDS-DESIGN]`
  edge-creation item closed pointing here.

## Blocks / enables

- **Blocked by:** ~~Graffodil 6d (or submodule-consumption sign-off)~~ — resolved 2026-08-19, see M0 resolution above. No upstream blockers remain; M1 is dispatchable.
- **Interacts with:** Cluster L5 soak (M5's eState work touches
  `WorkspaceSerializer`; sequence after L5 resumes/closes or coordinate).
- **Enables:** Graffodil migration precedent for `libs/forcegraph` (same substrate);
  card-fidelity brainstorm (§Deferred) lands on a stable authoring base.

## Deferred (needs its own brainstorm before any phase picks it up)

1. **Card content fidelity (audit §11.3)** — the audit's webview-vs-snapshot fork now
   has a **third option that didn't exist when it was written**: embed
   `Markoff::Canvas::EditorWidget` (since 2026-08-18 the sole LivePreview engine, a
   plain QWidget) per card via proxy for editing, with `StyledRenderEngine` snapshots
   (+`IRenderableCache`) for display. Likely the native sweet spot; needs perf
   validation (N editors) and a zoom-virtualization design (audit §8 inv. 8).
2. **Link nodes** — `NodeType::Link` parses/round-trips but has no item; rendering a
   live web page needs `QWebEngineView` (heavy new dep). A static "URL card" interim
   (favicon/title + open-in-browser) could ship without the dep — decide in M2 review.
3. **Byte-exact `dc()` serializer parity** (audit §13 Q4) — only matters for diff-noise
   in shared vaults; current output is parse-compatible.
