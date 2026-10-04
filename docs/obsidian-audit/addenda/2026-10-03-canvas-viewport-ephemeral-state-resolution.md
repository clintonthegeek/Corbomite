# A canvas leaf's viewport is **not** in `eState` — it is `ViewState.state.viewState`, and `eState` is never serialised at all

**Date:** 2026-10-03
**Discovered during:** Cluster M Phase M5.3 implementation (canvas viewport persistence),
unblocking `docs/punch-list.md` and the Cluster M plan.
**Supersedes / extends:** `domains/views.md` §`EphemeralState` (line 151) and
`domains/workspace.md` §2 leaf/history record shapes (lines 214, 220) — the
three-way contradiction catalogued in
[`2026-10-01-ephemeral-state-persistence-contradiction.md`](2026-10-01-ephemeral-state-persistence-contradiction.md).
Extends `domains/canvas.md` line 94 (which was **right** and needs no correction)
and [`2026-10-03-canvas-disk-contract-and-viewport-settings.md`](2026-10-03-canvas-disk-contract-and-viewport-settings.md).
**Relevant cluster plans:** `superpowers/plans/2026-08-19-cluster-m-canvas-authoring-parity.md`
(M5.3 — its "into the canvas leaf's **ephemeral state**" instruction is **wrong**),
`superpowers/plans/2026-08-17-cluster-l-workspace-stabilization.md` (the L0
shared-schema doctrine this implements against).

## Finding

Every path in Obsidian 1.13.7 says the same three things, and together they close
the contradiction. Citations are `app/own/<bundle>.js:<line>` in the restored
corpus (`RE/obsidian/1.13.7/renamed/`, queryable with `RE/tools/q.js`).

### 1. A canvas leaf's `eState` carries **no keys at all**

The canvas view is the class `t` in `app/own/2028-n9.js:919` (`}(CZ)`), which
extends `CZ` = `EditableFileView` (`app/own/1813-CZ.js:2`, `}(yZ)`), which extends
`yZ` (`app/own/1809-yZ.js:2`, `}(xI)`), which is `FileView`
(`app/own/1204-gI.js:1083-1085`, `}(ky)`), then `ItemView`
(`app/own/0754-wy.js:104-231`, `}(wy)`), then `View`
(`app/own/0754-wy.js:44-103`, `}(mg)`).

Walking that chain, **not one class in it overrides `getEphemeralState`**:

```js
// app/own/0754-wy.js:52-54  — View
t.prototype.getEphemeralState = function() {
  return {};
}, t.prototype.setEphemeralState = function(e) {},
```

`grep -c getEphemeralState app/own/2028-n9.js` → **0**. So a canvas leaf's
`getEphemeralState()` returns `{}`. The canvas contributes **nothing** to
`eState` on save.

The canvas does *consume* two `eState` keys, in its one override
(`app/own/2028-n9.js:851-868`): `focus` (moves DOM focus to the canvas wrapper)
and `match` (search-reveal — selects the node with `id === match.nodeId` and
scrolls a matching text card into view). Both are **inbound-only**, transient,
and per-`setViewState`-call. It reads `match.nodeId`, `match.content`,
`match.matches` — a shape narrower than the `match` in `views.md:159`.

`yZ` (`1809-yZ.js:7-15`), the nearest ancestor below `CZ`, adds
`rename: 'start' | 'end'` on the way in — the only other `eState` key anything
in the canvas view's ancestry reads (`FileView`, `ItemView` and `View` have no
`setEphemeralState` of their own beyond `View`'s no-op).

### 2. The viewport is `ViewState.state.viewState`, and it is persisted

The canvas view's `getState`/`setState` put the viewport in the **`state`
payload**, under the key `viewState`:

```js
// app/own/2028-n9.js:848-850  — CanvasView.getState()
t.prototype.getState = function() {
  var t = e.prototype.getState.call(this);
  return t.viewState = this.canvas.getState(), t;

// app/own/2028-n9.js:844  — CanvasView.setState(), `t` is the ViewState
return i.sent(), t.viewState && this.canvas.setState(t.viewState), [2];
```

and the controller's half is a plain `{x, y, zoom}`:

```js
// app/own/2018-k7.js:968-974  — Canvas.getState()
e.prototype.getState = function() {
  var e = this;
  return { x: e.tx, y: e.ty, zoom: e.tZoom };
};

// app/own/2018-k7.js:963-967  — Canvas.setState()
e.prototype.setState = function(e) {
  var t = e.x, n = e.y, i = e.zoom;
  Number.isNumber(t) && Number.isNumber(n) && Number.isNumber(i) &&
    (this.setViewport(t, n, i), this.zoomToFitQueued = !1);
};
```

`Number.isNumber` is Obsidian's own global
(`RE/obsidian/1.13.7/renamed/src/enhance/enhance.js:99-101`,
`t => typeof t === "number"`), so **all three keys must be present and numeric**
or the viewport is silently ignored and `zoomToFitQueued` stays armed. (A partial
`viewState` is not merged — it is all-or-nothing.)

`state` **is** in `workspace.json`. The chain, with no gaps:

| Step | Location | Result |
|---|---|---|
| settle a pan/zoom | `2018-k7.js:850` | `app.workspace.requestSaveLayout()` |
| debounced 1000 ms | `1932-r4.js:5` | `Ml(saveLayout, 1e3)` |
| write | `1932-r4.js:1161-1182` | `JSON.stringify(getLayout())` → `configDir + "/workspace.json"` |
| leaf node | `1206-OI.js:28-34` | `{ id, type, dimension? }` |
| leaf + state | `1209-jI.js:559-563` | `+ { pinned?, state: getViewState(), group? }` |
| ViewState | `1209-jI.js:641-650` | `{ type, state: getState(), pinned?, icon?, title? }` |
| view state | `1204-gI.js:965-969` + `2028-n9.js:850` | `{ file: "<path>", viewState: { x, y, zoom } }` |

So an Obsidian-written canvas leaf looks like:

```json
{
  "id": "1a2b3c4d5e6f7a8b",
  "type": "leaf",
  "state": {
    "type": "canvas",
    "state": {
      "file": "Boards/Roadmap.canvas",
      "viewState": { "x": -1180.5, "y": 640.25, "zoom": 0 }
    },
    "icon": "lucide-layout-grid",
    "title": "Roadmap"
  }
}
```

### 3. `eState` is **never** written to `workspace.json`

This is the statement that settles the contradiction, and it makes
`views.md:151`'s heading ("not persisted") correct and `workspace.md:214`/`:220`
wrong.

`WorkspaceLeaf.serialize` writes exactly five things, and `eState` is not one of
them (`1209-jI.js:559-563`):

```js
WorkspaceLeaf.prototype.serialize = function() {
  var t = e.prototype.serialize.call(this);
  return this.pinned && (t.pinned = !0),
         t.state = this.getViewState(),
         this.group && (t.group = this.group), t;
};
```

The reader matches — it restores `state` and nothing else
(`1932-r4.js:1143-1144`): `v = e.state || {}` … `f.setViewState(v)`.

`workspace.md`'s two shapes are real objects, but they are **in-memory**:

- `{ leafId, state, eState, parentId?, rootId?, leafHistory }` is
  `Workspace.pushUndoHistory`'s undo entry (`1932-r4.js:2021-2032`) — a
  **capped-at-10 in-memory array** on the `Workspace` object. Its `leafHistory`
  sub-object is `{ backHistory, forwardHistory }` and *that* is what gets
  stored; the entries themselves live on the live leaf object.
- `{ title, icon, state, eState }` is `WorkspaceLeaf.getHistoryState()`
  (`1209-jI.js:858-864`), pushed into `leaf.history.backHistory` /
  `forwardHistory`. Those stacks are per-leaf runtime objects, and
  `LeafHistory.serialize()` (`1209-jI.js:493-499`) returns only two array
  fields — it never puts an entry's `eState` anywhere:

  ```js
  e.prototype.serialize = function() {
    return {
      backHistory: k([], this.backHistory, !0),
      forwardHistory: k([], this.forwardHistory, !0)
    };
  };
  ```

  (`k` is a cross-module array helper bound outside this file, so its copy
  semantics are not worth pinning — the point is the *shape*: two arrays, no
  `eState`.) And `deserializeLayout` never reads a `leafHistory` key at all.
  So back/forward navigation survives a `setViewState` round-trip within a
  session, never a restart.

**The correction to the 2026-10-01 addendum:** it flagged statement 3
("`eState` appears in serialised workspace records") as the reason to doubt
statement 1, and left the Obsidian side open pending "an Obsidian-authored
`workspace.json` containing a populated `eState.scroll`, or decompiled source
reading it back." The source is now available and **statement 1 wins outright**:
there is no code path that writes `eState` to disk. `workspace.md` described two
runtime structs as if they were on-disk records.

The **code** half of that addendum stands and should not be revisited: moving
Corbomite's scroll fraction out of `eState.scroll` and into its own key was the
right call, and is now positively justified rather than merely safe-either-way.
Its remaining open question — whether Obsidian reads `scroll` as a line number
— is a separate, still-open question about a **transient restore hint**, not a
persistence question. Do not implement a fraction↔line conversion on the strength
of this addendum; `scroll` is in-memory only, so there is no cross-application
contract to match.

### 4. Units and semantics (both confirmed, both as the plan guessed)

- **`zoom` is log2 of the scale factor, not a raw factor.** `scale = 2^zoom`
  (`2018-k7.js:1250`: `t.scale = Math.pow(2, T)`, and recomputed every frame at
  `:823`, `:824`, `:829`, `:830`). Producers write log2 (`zoomToBbox`,
  `2018-k7.js:577`: `this.tZoom = Math.log2(a)`), and it is **clamped to
  `[-4, 1]` every frame** (`2018-k7.js:821`:
  `t.screenshotting || (d = t.tZoom = Math.clamp(d, -4, 1))`), i.e. scale
  `[0.0625, 2]`. A hand-edited value outside that range is accepted on load and
  clamped on the first animation frame, not rejected.
- **`x`/`y` are the scene point drawn at the viewport centre**, in unzoomed
  canvas units. The inline transform (`2018-k7.js:841`) is
  `translate(W/2, H/2) scale(2^zoom) translate(-x, -y)`, and the inverse
  `posFromDom` (`2018-k7.js:610-615`) is `scene = (x + domX/scale,
  y + domY/scale)`, which maps dom `(0,0)` — the viewport centre — to `(x, y)`.
  `zoomToBbox` confirms the intent by assigning the bbox centre
  (`2018-k7.js:576`: `s = O8(e); this.tx = s.x, this.ty = s.y`).
- `x`/`y`/`zoom` are the **targets** (`tx`/`ty`/`tZoom`), i.e. the settled
  values — `getState` reads the targets, not the mid-animation current values
  (`x`/`y`/`zoom`), so a save during an eased transition persists the
  destination.
- No viewport keys are written into the `.canvas` file, and none into
  `canvas/data.json`.

### 5. What *is* machine-local for canvas

`localStorage["canvas-" + file.path]` (`2028-n9.js:560-579`, the `v9` manager —
`load`/`save`/`handleRename`/`handleDelete`), holding **only
`{ readonly: true }`** — `getLocalData`/`saveLocalData` at `2028-n9.js:913-918`
write `readonly` and nothing else, and delete the key when the object is empty.
This confirms the Cluster M M5.4 claim and confirms that the viewport is **not**
there: it is Obsidian-shared, in `workspace.json`.

## Why noticed now

M5.3 was blocked behind the contradiction because the audit could not say what a
canvas leaf's `eState` holds, and the plan assumed the answer was "`{x, y,
zoom}` in `eState`". Rebuilding the corpus made the question answerable, and the
answer is that the plan's *field names* were right and its *channel* was wrong.

## Action taken

**Plan corrected, not the code.** Cluster M M5.3 now says
`ViewState.state.viewState`; the implementation writes `x`/`y`/`zoom` there.
Per the L0 doctrine this is the Obsidian-shared file, so the field names are
taken verbatim from source and are not Corbomite-prefixed.

### Corbomite's own restore precision, and the two substrate facts behind it

`zoom` restores **exactly** (scale is an unquantised double). `x`/`y` restore
to **within 0.5 device pixels** — measured, not estimated, across the whole
`zoom` range and targets spanning far outside the content. Two
`QGraphicsView` properties set that floor, and both are worth knowing before
anyone tries to tighten it:

1. **Panning is scrollbar-quantised.** A scrollbar value is one device pixel,
   where Obsidian writes a continuous CSS transform. So a restore lands on
   the rounding step of that grid. This is `Graffodil::PanZoomTool` too
   (`libs/graffodil/src/core/src/PanZoomTool.cpp:63-65,129-131` pans by
   `horizontalScrollBar()->setValue()`), not something M5.3 introduced.
2. **An unset `sceneRect` means `itemsBoundingRect()`,** so the reachable
   centre range is roughly the content extent — a persisted viewport in empty
   space is *unreachable*, and restoring it is a silent no-op. Obsidian has no
   world bounds, so this is an artefact of the substrate.
   `CanvasView::setViewState` therefore widens the scene rect (never shrinks)
   by 1.5 viewports at the target scale before centring. **This is a
   behaviour change worth reviewing**: it makes a restored canvas pannable
   into the empty space Obsidian allows, and it is the minimum needed for the
   feature to work at all — but making the canvas world properly unbounded is
   a viewport-substrate question that belongs to M5.1/Graffodil, not to M5.3.

Also note the restore is **live-saved later than Obsidian's**: Obsidian fires
`requestSaveLayout()` when the viewport settles (`2018-k7.js:850`, 1000 ms
debounce), whereas `Workspace::writeWorkspaceJson` is only reached from
`MainWindow::saveSessionState()` — i.e. on vault close and app close. So the
viewport reaches disk on quit, like the rest of `workspace.json`, not one
second after you stop panning. Content is identical; only the write latency
differs. Wiring a debounced save is a `MainWindow`-tier change.

### Test

`testCanvasEStateRoundTrip`, in `tests/app/tst_canvas_view_state.cpp`, against
the JSON block in §2 as a fixture. It asserts the Obsidian-authored
`viewState` is read as log2-zoom + viewport-centre coordinates and re-emitted,
that `x`/`y` are the centre and demonstrably not the corner, that a partial or
non-numeric `viewState` is ignored wholesale (the `Number.isNumber` ×3 guard),
that an out-of-range zoom is clamped rather than rejected, that the viewport
survives a reopen, and that `eState` is never written.

Falsifiability-checked: with the implementation reverted, **5 of its 6 slots
go red**. (The sixth is the reject-partial-payload guard, which cannot
distinguish "correctly rejected" from "not implemented" — the other five carry
the load.) The tolerances are expressed as a device-pixel budget rather than a
magic epsilon, so they state the substrate's actual precision.

`testCanvasEStateRoundTrip` is named for the punch-list/plan wording; note that
it exercises **`state.viewState`**, not `eState`. No test named for `eState`
should ever assert canvas viewport content — a canvas leaf's `eState` is `{}`.