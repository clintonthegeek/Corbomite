# Canvas 1.13.7 — the real `.canvas` serializer, three silent round-trip losses, and the full constant set

**Date:** 2026-10-03
**Discovered during:** exploratory spec-verification pass over `domains/canvas.md` (Corbomite canvas disk-contract work)
**Supersedes / extends:** `domains/canvas.md` §2, §3, §8, §9.1, §9.3, §9.4, §9.5, §9.6, §9.7, §11.1, §13; `addenda/2026-04-19-canvas-export-as-image.md` §1 and §3
**Relevant cluster plans:** none open; `libs/canvas` parity work should cite this.

**Corpus:** `RE/obsidian/1.13.7/renamed/src/<bundle>/own/*.js`. Citations below are `<bundle>/<rel>:<line>`.

> **Symbol-map warning first, because it invalidates a working method.** `canvas.md`'s
> "Symbol→role map" and every `app.pretty.js:NNNNNN` citation in it are dead: that
> monolith no longer exists, and in the split corpus **most of those letters now name
> unrelated code**. `dc`/`fc` are a **tree-sitter `TreeCursor`/tree node** (`app/own/0387-dc.js:2`);
> `V5`/`H5`/`j5` are **Bases list-view** symbols (`app/own/1969-L5.js:8-9,265`);
> `A3` is an **internal-embed widget** (`app/own/1955-A3.js:2`). The doc's advice to
> "cite them by role, not by letter" is now actively dangerous. Live role map for 1.13.7:
>
> | Role | Symbol | Location |
> |---|---|---|
> | Canvas controller | `I7` | `app/own/2018-k7.js:66` |
> | Node base | `V7` | `app/own/2020-O7.js:163` |
> | Content-bearing node base | `W7` | `app/own/2022-W7.js` (extends `V7`) |
> | Text / File / Link / Group node | `_7` / `j7` / `G7` / `X7` | `2022-W7.js:358,106,452,602` |
> | Dummy drag node | `z7` | `app/own/2020-O7.js:589` |
> | Edge | `o9` | `app/own/2028-n9.js:10` |
> | Edge label element | `K5`→`a9` | `app/own/2028-n9.js:245,278` |
> | Per-canvas link index | `d9` | `app/own/2028-n9.js:380` |
> | `linkUpdaters["canvas"]` | `f9` | `app/own/2028-n9.js:454` |
> | Per-file local-data store | `v9` | `app/own/2028-n9.js:560` |
> | Plugin / View | `CanvasPlugin` / `g9` | `app/own/2028-n9.js:578,802` |
> | Auto-side (angular sector) | `$7` | `app/own/2023-$7.js:2` |
> | Face-midpoint anchor / 7px inset | `Z7` / `J7` | `app/own/2024-Z7.js:2`, `2025-J7.js:2` |
> | Bezier builder / outward offset | `t9` / `e9` | `app/own/2027-t9.js:2`, `2026-e9.js:2` |
> | Colour normaliser / triplet regex | `Z8` / `$8` | `app/own/1998-Z8.js:2`, `1997-$8.js:2` |
> | `.canvas` serializer / printer | `$d` / `Zd` | `app/own/0563-$d.js:2`, `0564-Zd.js:2` |
> | Edge menu / node+selection menu | `n7` / `Q7`-consumer | `app/own/1999-J8.js` |
> | `.canvas` embed / settings tab | `b9` / `w9` | `app/own/2030-b9.js:2,34` |
> | Minimap renderer | `y9` | `app/own/2029-y9.js:13` |
> | Canvas help modal | `M8` | `app/own/1974-M8.js:2` |
> | id generator | `Gd` | `app/own/0558-Gd.js:2` |
> | Link-node URL transform | `mW` (+ `fW`,`vW`) | `app/own/1496-mW.js:2`, `1495-fW.js:2` |

---

## Finding

### F1 — CORRECTION (HIGH): the `.canvas` serializer is `$d`/`Zd`, not `dc`/`fc`

`canvas.md` §3 item 6 is wrong on the mechanism, and §13 Q4 asks the wrong question as a
result. `getViewData` is `$d(this.canvas.data)` (`app/own/2028-n9.js:873`) and `$d` is
`Zd(e, t).join("\n")` with `t = "\t"` (`app/own/0563-$d.js:2`). `Zd`
(`app/own/0564-Zd.js:2-38`), verbatim rules:

- `void 0 === e` → `"null"`; `Qd(e) || !e || "[object Date]" === Object.prototype.toString.call(e)` → `[JSON.stringify(e)]`, where `Qd(e) { return "object" != typeof e }` (`app/own/0562-Qd.js:2`).
- **Array:** `if (e.every(Qd)) return [JSON.stringify(e)]` — an all-primitive array (including `[]`) collapses to one compact line. Otherwise each element's already-produced lines get a single `"\t"` prefix (depth accumulates by recursion).
- **Object:** if every own value is primitive → `[JSON.stringify(e)]`, one compact line. Otherwise `Object.keys(e).filter(function(t){ return void 0 !== e[t] })` — **`undefined`-valued keys are dropped** — and each key line is `h[0] = JSON.stringify(key) + ":" + h[0]`, i.e. **no space after the colon**.
- Returns `undefined` if the input is `undefined`. `join("\n")` means **no trailing newline**.
- The `indent` argument `t` is threaded through but **never used** by `Zd`; tabs come from the hard-coded `"\t"`.

Because every node and every edge is an all-primitive object, an Obsidian-written canvas
looks like this (shape derived from the rules above; the values are illustrative):

```
{
	"nodes":[
		{"id":"a1b2c3d4e5f60718","x":100,"y":200,"width":250,"height":60,"color":"1","type":"text","text":"# hi"},
		{"id":"0011223344556677","x":400,"y":200,"width":400,"height":400,"type":"file","file":"Note.md"}
	],
	"edges":[
		{"id":"8899aabbccddeeff","fromNode":"a1b2c3d4e5f60718","fromSide":"right","toNode":"0011223344556677","toSide":"left"}
	]
}
```

Node/edge lines carry **two** tabs; the closing `]`/`}` carry one; `"nodes":[` opens on the
key line. An empty canvas serialises to `{\n\t"nodes":[],\n\t"edges":[]\n}`. There is no
second printer to reimplement — **one function, ~35 lines**, and byte parity is achievable.
§13 Q4 is therefore answered *yes, cheaply*.

### F2 — NEW (HIGH): `nodes` is re-sorted by `zIndex` on every save; `edges` are not, and file order **is** z-order

`I7.getData` does `n.sort(L7)` over the node values (`app/own/2018-k7.js:361-366`) where
`function L7(e, t) { return e.zIndex - t.zIndex }` (`app/own/2019-L7.js:2`) — **ascending**.
The edge loop has no sort (`app/own/2018-k7.js:367-370`), so edges keep `Map` insertion order.

`zIndex` is **not on disk**. It is reconstructed so that file order round-trips:
`p.zIndex < s && p.updateZIndex(), s = p.zIndex` (`app/own/2018-k7.js:429`) — a running
maximum over the file's array order that only bumps a node whose z-index went *backwards*.
A node only gets a fresh counter value when `this.zIndex !== canvas.zIndexCounter`
(`app/own/2020-O7.js:269-270`), and `getZIndex` is `this.zIndexCounter += 1`
(`app/own/2018-k7.js:732-739`). Net effect: **the position of a node in the `nodes` array is
its paint order**, and it survives a save/load cycle.

Two consequences the doc does not state:

1. Any consumer that re-sorts `nodes` (by id, by type, by x) will produce a whole-file diff
   from Obsidian on the next save.
2. Corbomite's `m_nodeOrder` (file order, `libs/canvas/src/CanvasDocument.cpp:123-125`)
   is equivalent to Obsidian's stable `zIndex` sort for the first round trip, but Corbomite
   has no z-order concept at all, so the orders diverge as soon as nodes are created.

### F3 — NEW (HIGH, silent data loss): unknown **node types** are dropped and destroyed on save

`importData`'s dispatch (`app/own/2018-k7.js:420-426`) is `file` → `text` → `link` →
`group` → **`if ("group" !== u.type) continue`**. An unrecognised `type` is skipped without
entering `this.nodes`, and `getData` rebuilds the array from `this.nodes`, so the node is
**absent from the next save**. This is not a round-trip of unknown data — it is deletion.
A `.canvas` written by a newer Obsidian or by a plugin that adds a node type loses those
nodes the first time a 1.13.7 user moves anything.

### F4 — NEW (HIGH, silent data loss): edges with unresolvable endpoints are dropped

`app/own/2018-k7.js:442,459`: an edge is only imported `if (o.has(y) && o.has(k))` (both
endpoint node ids present); otherwise `d.has(h) && this.removeEdge(d.get(h))`. Since
`getData` rebuilds `edges` from `this.edges`, a dangling edge is deleted on the next save.
Combined with F3, a canvas that references a node type 1.13.7 does not know loses both the
node and every edge touching it. Note this only bites on the *reconciling* path —
`importData(data, false)` is the non-destructive mode used by paste and mobile duplicate
(`app/own/2018-k7.js:409`, `1030`, `app/own/2020-O7.js:316`).

### F5 — NEW (HIGH): explicit **default** values are preserved verbatim, not stripped

§3 invariant 2 says defaults are omitted. True for values Obsidian *generates*, but the
omit-lists are per-class and a key the class does not re-assign survives in `unknownData`:

- Edge `setData` omits `["fromNode","fromSide","fromEnd","toNode","toSide","toEnd","color","label"]` into `unknownData` (`app/own/2028-n9.js:52`); `getData` re-assigns `fromEnd`/`toEnd` only when non-default and `color`/`label` only when truthy (`app/own/2028-n9.js:42`). So a file containing `"fromEnd":"none"`, `"toEnd":"arrow"` or `"color":""` **keeps that key** — Obsidian does not normalise them away.
- Group `setData` strips only `["label","background"]` before delegating
  (`app/own/2022-W7.js:621`), so `backgroundStyle` lands in the base `unknownData`
  (`app/own/2020-O7.js:191`). `getData` only overwrites it when `bgStyle` is truthy and
  `!== "cover"` (`app/own/2022-W7.js:617`) — therefore **`"backgroundStyle":"cover"` and an
  orphan `"backgroundStyle":"repeat"` with no `background` both round-trip verbatim**.

So the correct rule is: *Obsidian omits a default it would itself have generated; it does
not delete a default the file already contains.*

### F6 — NEW (HIGH): `text`, `file` and `url` are **always** written

§2 says `text` is "always written" — correct — but the same is true of the other two
required fields and the doc does not say so:

- text: `m(m({}, e.prototype.getData.call(this)), { type: "text", text: this.text })` (`app/own/2022-W7.js:364-368`) — unconditional, so an empty card writes `"text":""`.
- file: `file: this.file ? this.file.path : this.filePath` (`app/own/2022-W7.js:124`) — unconditional; an unresolved file node keeps its path string.
- link: `url: this.url` (`app/own/2022-W7.js:457-461`) — unconditional.

A non-string `text` is ignored on read (`String.isString(n) && this.setText(n)`,
`app/own/2022-W7.js:371`) so `this.text` stays `""` and the re-assignment overwrites the
stored value — `"text":42` becomes `"text":""`.

### F7 — CORRECTION (HIGH): node key emission order

Confirmed, and it is *not* what §2's TypeScript block implies. Actual order is
`…unknownData, id, x, y, width, height, [color]` (`app/own/2020-O7.js:167-183`) then the
subclass appends `type` and its fields (`2022-W7.js:364-368,457-461,609-617`). Edge order is
`…unknownData, id, fromNode, fromSide, toNode, toSide, [fromEnd], [toEnd], [color], [label]`
(`app/own/2028-n9.js:35-42`) — §2's stated edge order is right. Node order puts `type`
**after** geometry (doc correct) and `unknownData` **first** (doc only implies it).

> **REVIEWER NOTE 2026-10-03 — one correction.** F8's colour example is wrong: `"300,0,0"`
> yields **`#12c0000`** (seven hex digits), not the five-digit `#12c00`. `zb` is
> `(300).toString(16)` = `"12c"`, then two zero-pads. The *behaviour* claimed — no clamping,
> and the parser re-normalises on read — is correct and is what was implemented.
> Separately, F20's characterisation of the pre-fix parser was slightly generous: it also
> defaulted width/height to 250/60, which F8 does not mention.

### F8 — NEW: geometry, colour and id normalisation edge cases

- **NaN geometry is rejected, not written.** `n = Math.round(n), …, isNaN(n) || this.x === n || (this.x = n, …)` (`app/own/2020-O7.js:194`) — a `NaN`/`x` of `"12"`/`null` leaves the previous value (constructor default `x=0, y=0, width=100, height=100`, `app/own/2020-O7.js:165`) and is **silently dropped on save**. Also `moveAndResize` re-rounds unconditionally (`app/own/2020-O7.js:219`).
- **Colour is normalised on read, so triplets are rewritten.** `Z8` matches `$8 = /^\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*$/i` (`app/own/1998-Z8.js:2`, `1997-$8.js:2`) and converts to `"#" + zb(r) + zb(g) + zb(b)`. So `"255,0,0"` is written back as `"#ff0000"`. **Decimal integers only** — no floats, no hex, no percentages. **No clamping**: `zb` is a bare `toString(16)` pad-to-2 (`app/own/0809-zb.js:2`), so `"300,0,0"` yields the five-hex-digit `"#12c00"`. Preset indices `"1".."6"` and `"#rrggbb"` pass through untouched.
- **`id` generation** is `Gd(n) = Array(n).fill().map(() => (16 * Math.random() | 0).toString(16)).join("")` (`app/own/0558-Gd.js:2`) — 16 lowercase hex chars, no collision check. §3 item 7 confirmed.

### F9 — CORRECTION: `canvas:convert-to-file` writes the card body; there is no embed

`addenda/2026-04-19-canvas-export-as-image.md` §1 says it "Converts a text-card node into a
standalone `.md` file with an embed", and `domains/canvas.md` §6 inherits that. Actual
(`app/own/2022-W7.js:396-435`): a `new fR(app, "Untitled", cb)` prompt (default name
**hard-coded `"Untitled"`**, not i18n) → `fileManager.getNewFileParent(canvasPath, name)` →
`fileManager.createNewMarkdownFile(parent, name, this.text)` — **the card's raw markdown
becomes the note body**; then a file node is created at `pos: this, size: this` (i.e. the
text node's exact rect), **all edges are re-pointed** at the new node via
`p.update({...p.from, node: newNode}, p.to)`, the text node is removed, and
`canvas.requestSave()`. No embed, no `![[…]]`.

### F10 — NEW: the align / arrange / distribute / justify family (§9.5 covers none of it)

Fourteen menu items in five sections, all implemented, none documented. `app/own/1999-J8.js:387-440`
(sections `align-h`, `align-v`, `arrange`, `distribute`, `justify`; rendered with
`setUseNativeMenu(!1)`).

- **Target set + frame** (`app/own/1999-J8.js:270-282`): right-clicking a **group** targets `getContainingNodes(groupBBox).filter(n => n !== group)` with the frame `I8(groupBBox, -20)` — the group's interior **inset 20 px**. Otherwise the frame is the selection's `V7` nodes' bbox union.
- **Align** `left|right|top|bottom|center|middle` (`:284-296`): `center` → `x = (minX + maxX - width) / 2`, `middle` → `y = (minY + maxY - height) / 2`. Ends with `requestSave()`.
- **Distribute** `horizontal|vertical` (`:298-330`): requires `n.length > 1`; sorts by `x` then `y` (or `y` then `x`); gap `l = (frameExtent - Σ size) / (n - 1)`; moves indices `1 … n-2` along one axis only.
- **Justify** `horizontal|vertical` (`:331-345`): `moveAndResize` to the frame extent on that axis, preserving the other.
- **Arrange** `horizontal|vertical|grid` (`:346-385`): nodes are banded by `y` (greedy: a node joins the current band while `node.y < bandBottom`), each band sorted by `x`, then laid out with gap `f = canvas.gridSpacing`. `horizontal`/`vertical` are a single chain from `p[0]`; `grid` flows band-by-band with `rowHeight = max(item.height)` per band. **The gap is `gridSpacing`, i.e. it changes with zoom.**

### F11 — NEW: the full `.obsidian/plugins/canvas/data.json` schema

The doc says snapping is "persisted via `saveOptions`" but never says where. Defaults are
`{snapToObjects: !0, snapToGrid: !0}` merged under whatever `loadData()` returns
(`app/own/2028-n9.js:731-737`). Settings tab `w9` (`app/own/2030-b9.js:34-124`) gives the
complete key set — **8 keys**:

| key | control | default | values |
|---|---|---|---|
| `newFileLocation` | dropdown | `"root"` | `root` \| `current` \| `folder` |
| `newFileFolderPath` | folder picker | — | shown only when `newFileLocation === "folder"` |
| `defaultWheelBehavior` | dropdown | `"pan"` | `pan` \| `zoom` |
| `defaultModDragBehavior` | dropdown | `"menu"` | `menu` \| `card` \| `note` \| `media` \| `webpage` \| `group` |
| `cardLabelVisibility` | dropdown | `"always"` | `always` \| `hover` \| `never` |
| `snapToGrid` | toggle | `true` | |
| `snapToObjects` | toggle | `true` | |
| `zoomBreakpoint` | slider | `0` | min `-0.7`, max `2.3999999999999995`, step `0.01` |

`getNewFileParent` semantics (`app/own/2028-n9.js:769-782`): `folder` uses
`newFileFolderPath` only if it resolves to a `TFolder`; `current` uses the **parent folder
of the active file**; otherwise the vault root. `zoomBreakpoint`'s **display format is
`Math.round(100 * Math.pow(1.2, e)) + "%"`** while the effective threshold is
`T7 - (options.zoomBreakpoint || 0)` with `T7 = -1.7`
(`app/own/2018-k7.js:29,494-497`) — i.e. `scale = 2^(-1.7 - v)`. **At the default `v = 0`
the label reads "100%" but the real threshold is 2^-1.7 ≈ 0.308.** Do not reproduce the
label formula as if it were the threshold.

### F12 — NEW: viewport, snap and gesture constants, pinned

`config` (`app/own/2018-k7.js:69-80`), verbatim: `zoomMultiplier: .5`,
`objectSnapDistance: 15`, `minContainerDimension: 50`,
`defaultTextNodeDimensions: {width:250, height:60}`,
`defaultFileNodeDimensions: {width:400, height:400}`.

- **`gridSpacing`** (`app/own/2018-k7.js:583-589`): `zoom < -3.3 ? 160 : zoom < -2.16 ? 80 : zoom < -.91 ? 40 : 20`. Doc §9.5's "20/40/80/160" confirmed, thresholds now known.
- **`snapDistance`** = `Math.ceil(config.objectSnapDistance / this.scale)` (`:590-593`) — a **screen-space** tolerance, so it widens in world units as you zoom out.
- **Snap points are 4 corners + centre** — `j8` returns `[minX,minY], [minX,maxY], [maxX,minY], [maxX,maxY], [centre]` (`app/own/1989-j8.js:2`). **No edge midpoints.** Doc §9.5 confirmed.
- **Per-axis, object-over-grid** (`:1052-1075`): object snap runs with tolerance `d` on an axis only when that axis is moving (`n ? d : 0`); grid snap fills in **only axes where object snap found nothing** (`s && (!r || !o)`); delta is `Math.round(bbox.minX / v) * v - bbox.minX`. `renderSnapPoints` is called only if object snap produced something, so **grid-only snapping draws no guides**.
- **Auto-pan** (`:1596-1604`): trigger band = wrapper rect **inset 30 px**; overshoot clamped to `[-40, 40]`; rate `0.5 / Math.sqrt(canvas.scale)` world units per tick at `1000/60` ms — 60 Hz confirmed, with the exact inset and zoom-compensated speed.
- **Transform and easing confirmed** (`:839-841`): `translate(W/2 px, H/2 px) scale(v) translate(-x px, -y px)`; easing `a = 1 - Math.pow(.984, dt)` (`:808`) with `finishViewportAnimation` forcing `a = 0` and `S7 = .01` as the snap-to-target epsilon; zoom clamped `[-4, 1]` every frame (`:821`). Also: `--zoom-multiplier` is set to `String(Math.sqrt(1 / v))` (`:847`), `mod-zoomed-out` is toggled at `zoom <= -1.7` (`:839`), and **when the viewport settles `app.workspace.requestSaveLayout()` fires** (`:849`) — that is how the viewport reaches `workspace.json`.
- **`zoomToBbox` uses a 1.1 margin**: `r = W / (1.1 * bboxW), o = H / (1.1 * bboxH), a = clamp(min(r,o), -4, 1)`, centred on `O8(bbox)`, with `zoomCenter = null` (`:568-577`).
- **`zoomBreakpoint` + virtualisation**: content is torn down when `zoom <= zoomBreakpoint` and the node is not editing (`app/own/2020-O7.js:225`). `virtualize` (`:923-957`) attaches **at most 10 *newly*-entering** nodes per pass then re-requests a frame; already-attached and the sole-selected node are free; **intersecting edges attach unconditionally, with no throttle**; and `screenshotting` bypasses the whole thing (`o = all nodes, a = all edges`).
- **Duplicate-drag and snap-disable use *inverted* modifiers**: duplicate is `isMacOS ? altKey : ctrlKey` (`:1651`), snap-disable is `isMacOS ? !ctrlKey : !altKey` (`canSnap`, `:1557-1559`). So mac = Alt-drag duplicates / Ctrl disables snapping; elsewhere it's the reverse. Doc §9.4/§9.5 both correct, but the inversion is unstated and easy to get backwards.
- **Groups are excluded as snap targets** while dragging a new card in: `getViewportNodes().filter(n => !(n instanceof X7))` (`:1567-1570`).
- **Arrow-key nudge filters to nodes** (`Array.from(this.selection).filter(e => e instanceof V7)`, `:518-520`), steps `gridSpacing` (`×5` with shift), and grid-snaps the selection bbox. **Upstream bug worth recording: at `app/own/2018-k7.js:548` the `"down"` branch does `r.x += o` instead of `r.y += o`** — a Down nudge also shifts the selection right by one grid step. Corbomite should *not* copy this.
- **`Duplicate` in the node context menu is mobile-only** (`rd.isMobile &&`, `app/own/2020-O7.js:313-322`); desktop duplicates by drag only. (That path calls `importData(_, false)` + `deselectAll` + reselect with **no `requestSave()`**.)

### F13 — NEW: the help panel is the authoritative gesture contract, and it is setting-dependent

`M8` (`app/own/1974-M8.js:2-71`) enumerates every binding. Crucially, when
`plugin.options.defaultWheelBehavior === "zoom"` the Pan/Zoom modifier sets are
**swapped**: pan becomes `Scroll` or `Mod+Scroll` or `Space+Scroll`, horizontal pan becomes
`Mod+Shift+Scroll` / `Shift+Space+Scroll`, and plain `Scroll` becomes zoom. Under the default
(`"pan"`): pan = `Space+Drag` + `Scroll`, h-pan = `Shift+Scroll`, zoom = `Mod+Scroll` and
`Space+Scroll`. Doc §9.6's "zoom = Ctrl/Space+wheel" should read **`Mod`+Scroll**, not Ctrl.

Full contract: `Shift+1` zoom-to-fit, `Shift+2` zoom-to-selection, `Mod+A` select-all,
`Shift+Click` / `Shift+Drag` add-to-selection, `Mod+Drag to select` create-with-size,
`Alt`(mac)/`Ctrl`+Drag clone, `Shift+Drag` axis-lock, `Ctrl`(mac)/`Alt` no-snap,
`Backspace`/`Delete` remove, `Mod+Z` undo, `Mod+Y`/`Mod+Shift+Z` redo. The last group is
implemented as a **scoped keymap on the view's own `ly(app.scope)`**
(`app/own/2028-n9.js:806-834`) — so §6's "no zoom/undo commands" needs the qualifier that
they exist as *view-scoped shortcuts*, not registered commands.

The same scope also handles `setEphemeralState`: a `focus` flag focuses the wrapper, and a
**`match` payload `{nodeId, content, matches}`** selects the node, pans to it and asks the
card's renderer to `applyScrollDelayed(pos, {center: true, highlight: true})`
(`app/own/2028-n9.js:845-858`) — i.e. find-in-obsidian can land *inside a canvas card*.

### F14 — NEW: the link index (`d9`) in full

`app/own/2028-n9.js:380-452`. `canProcess` is `e.extension === "canvas"` (where
`h9 = "canvas"`, `:378`). `process`:

- `vault.cachedRead(file)` → `JSON.parse` → **bails unless `Array.isArray(nodes)`**. **`edges` are never read** by the index.
- `type === "file"` → embed entry `{file: e.file, subpath: e.subpath}`.
- `type === "group" && e.background` → embed entry `{file: e.background, subpath: ""}` — **group backgrounds participate in the link graph**.
- `type === "text"` → `parseText(e.text)`, cached, stored as `caches[node.id]`, and `refNodeIds.set(cacheObject, node.id)`.
- `type === "link"` and unknown types → nothing.
- A text-parse throw is caught, logged, and the node skipped.
- `parseText` switches implementation on length: **`e.length < 5e3 ? uD(e) : computeMetadataAsync(Md(e))`** — a **5000-UTF-16-unit** threshold between a sync fast parse and the full async metadata path (`:434-437`).
- Finishes with `app.metadataCache.queueFileForLinkResolution(file)` and returns `{embeds, caches}`.

`f9` (`:454-559`): `iterateReferencesForFile` walks `caches` via `CD(cache, cb)`
(`app/own/1115-CD.js:2`, which tries `frontmatterLinks`, then `links`, then `embeds` — **not**
`frontmatterEmbeds`), then emits each embed as `{link: s.file + (s.subpath ?? ""), original: ""}`
— **`original` is always the empty string**, so every canvas embed reference has empty display
text. `applyUpdates` maps each pending update's `reference` back through `refNodeIds`, rewrites
only `type === "text"` nodes with `ZF(text, updates)`, and returns `$d(parsedFile)` — the whole
file re-serialised. `renameSubpath` compares the subpath after `#` case-insensitively, raw for
`^block` ids and via `TD` (whitespace-collapse) for headings (`app/own/1119-TD.js:2`), rewrites
matching `type === "file"` nodes' `subpath` to `"#" + newSubpath`, and raises
`"Canvas detected file renames affecting N canvas files, updating..."` /
`"Canvas updated N embed cards."` (`app/own/2028-n9.js:512-558`). The file-level rename
rewriter lives on the plugin (`:580-624`, `Ml(..., 500, !0)`), rewrites `file` on `type:"file"`
nodes and `background` on `type:"group"` nodes, and shows the same two notices.

### F15 — NEW: export-as-image constants (`addenda/2026-04-19` §2/§3 are substantially wrong)

`generateHDImage` (`:1766-1994`) and `takeScreenshot` (`:1740-1765`):

- Empty canvas → Notice `"Cannot export an empty canvas"`, abort (`:1823`).
- **Output scale `s = Math.round(devicePixelRatio / Math.pow(1.2, webContents.getZoomLevel()) * 1e5) / 1e5`** — the Electron window zoom is folded in, so the PNG resolution changes with app zoom (`:1823`).
- **Bounds are `I8(P8(nodes.map(D8)), 100)` — 100 px padding**, not the addendum's "edge padding (20px)" (`:1823`).
- **Max scale `h = Math.min(2, 16284 / s / width, 16284 / s / height)`** — a hard **2×** ceiling plus a 16284-px raster cap; slider bounds `[Math.min(-1, log2(h)), log2(h)]`, display `2^e.toFixed(2)` (`:1845-1850`).
- Modal has exactly four settings: Frame (`full` \| `viewport`), Zoom (full only), Show logo (default **on**), Privacy mode (default **off**) (`:1834-1858`); one `Save` button; save dialog `defaultPath = file.basename + ".png"` with PNG/All-Files filters (`:1873-1883`).
- **`viewport` mode is not "no edges"** — `takeScreenshot` clips to the union bbox of **all** nodes clamped to the wrapper rect, **hides the status bar**, and captures once (`:1746-1754`). Doc §9.7's parenthetical is right about the single `capturePage`.
- The watermark grows the image: `l.maxY += 80; u += 80` and a `.canvas-watermark` div is placed at `minX+80 / maxY-80` with `scale(4/d)` and `zIndex:"9999999"`, holding an inline `viewBox="0 0 143 25"` SVG at `width=100` (`:1891-1908`).
- `is-screenshotting` forces the settled scale to `≤ 1` (`v = Math.min(1, v)`, `:849`) and re-parents `wrapperEl` into `document.body`, inset by `5 / electronZoom` on all sides (`:1889,1909-1915`).
- Tiling opens a **second window** via `window.open("about:blank","_blank","popup,…")`, injects `<base href=location.href>`, sets `document.title = "Obsidian"`, tiles `V×H` with 100 ms + `nextFrame` + optional `requestIdleCallback` between `capturePage` calls, composites with `drawImage` at `s` scale, and shows a `Rendering tiles... (i/u)` progress bar with a **Stop** button (`:1916-1967`).
- On success, a Notice with a **10 000 ms** timeout and a "Show in folder" button (`:1756,1975`). **PNG only** — confirmed, no SVG.

### F16 — NEW: §13 Q2 and Q3 answered

**§13 Q3 (`backgroundStyle` enum) — closed at three.** The group style submenu is exactly
`cover` (checked when `!bgStyle || "cover" === bgStyle`) / `ratio` / `repeat`
(`app/own/1999-J8.js:527-536`), plus a `style`-section sibling set `image`: Replace/Remove
background. `setBackgroundStyle` forces `""` when there is no background
(`app/own/2022-W7.js:638-639`). The UI label for `"ratio"` is **"Keep aspect ratio"**, not
"Ratio". `ratio` semantics: load the image's natural size, set
`aspectRatio = w / h`, then **shrink-only** fit — `resize({width: Math.min(o, a*r), height: Math.min(a, o/r)})`
— and `canvas.overrideHistory()` so it is one undo step (`app/own/2022-W7.js:698-708`).
File nodes use the identical formula for media (`app/own/2022-W7.js:222-227`). CSS mapping:
`cover` → `no-repeat` / `center center` / `cover`; `repeat` → `repeat` / `top left` / `auto`
(`app/own/2022-W7.js:682-691`).

**§13 Q2 (`Az` URL transform) — it is `mW`, and it covers only YouTube and Twitter/X.**
`app/own/1496-mW.js:2` with `fW`/`vW` at `app/own/1495-fW.js:2`:

```js
if (t = e.match(fW)) return "https:" !== location.protocol && /obsidian/i.test(navigator.userAgent)
  ? (e = "https://releases.obsidian.md/youtube?v=" + t[1], t[2] && (e += "&start=" + t[2]))
  : (e = "https://www.youtube-nocookie.com/embed/" + t[1], t[2] && (e += "?start=" + t[2])), e;
if (t = e.match(vW)) return "https://platform.twitter.com/embed/Tweet.html?dnt=true&theme=" + (document.body.hasClass("theme-dark") ? "dark" : "light") + "&id=" + t[1];
return null;
```

`fW` is also the "embeddable" test that selects the **640×360** link-node default
(`app/own/2018-k7.js:777-780`); every other URL defaults to 400×400 — the doc mentions only
640×360. On desktop the webview's `useragent` is set to `navigator.userAgent` **only** when
the transformed URL starts with `https://releases.obsidian.md/youtube`, otherwise `""`
(`app/own/2022-W7.js:509`). `setFrameUrl` ignores any URL failing `/^https?:\/\//`
(`:507`). And the paste classifier is `Jv`, which requires `new URL()` parseability **and**
an `http:`/`https:` protocol (`app/own/0647-Jv.js:2`) — so pasting `ftp://…` makes a **text**
node, not a link node.

### F17 — NEW: events, registries and local data, exact payloads

All six workspace events confirmed, with exact arity:

| event | payload | site |
|---|---|---|
| `canvas:node-menu` | `(menu, node)` | `app/own/2020-O7.js:329` |
| `canvas:node-connection-drop-menu` | `(menu, node, connection)` — **3 args** | `app/own/2020-O7.js:545` |
| `canvas:edge-menu` | `(menu, edge)` | `app/own/2028-n9.js:188` |
| `canvas:selection-menu` | `(menu, canvas)` | `app/own/2018-k7.js:1549` |
| `file-menu` | `(menu, TFile, "canvas-menu")` | `app/own/2022-W7.js:313` |
| `files-menu` | `(menu, TFile[], "canvas:selection-menu", null)`, fires only when `length > 1` | `app/own/2018-k7.js:1555` |

§7 says "three app registries". There are **five** touch points:
`registerViewType("canvas", …)` in `init` (`app/own/2028-n9.js:636-638`);
`viewRegistry.registerExtensions(["canvas"], "canvas")` in `onEnable` (`:722`);
`embedRegistry.registerExtension("canvas", …)` (`:723`);
`fileManager.registerFileParentCreator("canvas", this.getNewFileParent)` (`:724`);
`metadataCache.linkUpdaters["canvas"] = new f9(app, this)` (`:721`) — and the linkUpdater is
deleted by a `register()` unload callback, **not** by `onDisable`, which only unregisters the
view-registry extension, the embed extension and the file creator (`:727,738-740`).
`onExternalSettingsChange` reloads options and calls `rerenderCanvases()`
(`:743-750`); `onUserDisable` calls `workspace.detachLeavesOfType("canvas")` (`:751`).

**Undocumented feature:** `onFileMenu` adds a **"New canvas"** item into section
`action-primary` of the file-explorer context menu, gated on
`source === "file-explorer-context-menu"` and `file instanceof TFolder`
(`app/own/2028-n9.js:753-768`). Command icons are `lucide-layout-dashboard` (new-file),
`lucide-image` (export), `lucide-move` (jump-to-group), `lucide-file-input`
(convert-to-file); the export command's i18n name is `actionExportPng()` =
**"Export as image"** (`app/own/2028-n9.js:639-691`, `i18n/i18n.js` `bd.plugins.canvas`).

**Local data** `v9` (`app/own/2028-n9.js:560-577`) confirms §2's key format `"canvas-" + file.path`
and adds two behaviours the doc omits: `handleRename(oldPath, newFile)` **migrates** the entry
across a canvas rename, and `handleDelete(file)` nulls it. `saveLocalData` writes `{readonly: !0}`
or `null` — an empty object is normalised to `null` (`:915-918`) — and `setViewData` only reads
it when the view is loaded (`:878-883`). The read-only toggle lives in the controls cluster's
**first item**, whose icon swaps `lucide-settings` ⇄ `lucide-lock` and whose tooltip swaps
`labelCanvasSettings()` ⇄ `labelDisableReadonly()`; when already read-only, clicking it toggles
straight off and saves (`:200-212,1395-1404`). The quick-settings menu that opens otherwise has
exactly three items, all in section `canvas`: snap-to-grid, snap-to-objects, read-only
(`:1405-1421`).

**History**: `new k7(100)` — **100 whole-document snapshots**, with redo-tail truncation on
push and a shift when full (`app/own/2018-k7.js:2-25,81`). `requestPushHistory = Ml(pushHistory, 250, !0)`
(leading + trailing, `:81`); `requestProcessRename = Ml(..., 500, !0)` (`:580`). Save debounce
**2000 ms** trailing, `Ml(n.save.bind(n), 2e3)` (`app/own/1813-CZ.js:6`) — §8 invariant 9 confirmed.

**Clipboard**: MIME `"obsidian/canvas"`, payload `JSON.stringify(getSelectionData())` where
`getSelectionData` returns `{nodes, edges, center}` with `center` = centre of the selection bbox
(`app/own/2018-k7.js:375-401,999`). Paste **rejects the payload unless both `nodes` and `edges`
are arrays** (`:1017-1022`), then `cloneData` **regenerates every node *and* edge id** and maps
`fromNode`/`toNode`, blanking unresolvable references, and offsets node x/y so the selection
centres on the pointer (`:985-997`). Paste precedence: attachments → canvas JSON → URL text →
plain text (`:1002-1044`). File-drop layout is a **10-column grid with a 45 px gutter**:
`x = pos.x + (400+45) * (i % 10)`, `y = pos.y + (400+45) * trunc(i / 10)` (`:759-772`) —
the doc says "grid of file nodes" with no numbers.

**Placeholder**: an empty (but parseable-to-`{}`) canvas shows `.canvas-placeholder-message`
with desktop text "Drag from below or double click" / "Space + Drag to pan" / "<Mod> + Scroll to zoom",
mobile "Drag to pan" / "Pinch to zoom" / "Touch and hold to add / move / select" (`:348-357,408`).

**§12 fold key confirmed**: `K7.linktext` = `canvas.view.file.path + "#^" + node.id`, via
`foldManager.loadPath` / `savePath` (`app/own/2022-W7.js:432-451`); the card also
`pushScope`/`popScope`s its editor keymap and calls `canvas.requestSave()` directly from
`save()` with **no debounce** (`:437-439`).

### F18 — NEW: the `.canvas` minimap is a static thumbnail, now fully specified

`y9` (`app/own/2029-y9.js:13-52`) is used by the embed (`b9`, `app/own/2030-b9.js:2-33`) when
a canvas is transcluded. It reads the raw file text, `JSON.parse`, and bails on a nodes array
that is empty or absent. Rects come from `D8(node)` — i.e. **raw `x/y/width/height`, unrounded**,
so the minimap uses the on-disk geometry rather than the live `Math.round`ed model. Groups are
drawn **first** (behind), each as an SVG `<rect>` with `rx="20" ry="20"` and `Q8(node.color)`;
then edges as `<path>` with `strokeWidth = String(Math.sqrt((maxX - minX) / 10))` and the
edge's colour; then non-group nodes. `viewBox` is the node union padded by
`I8(h, Math.max(w, h) / 20)`. There is **no interaction** — no viewport rectangle, no
click-to-navigate. The embed adds `.canvas-embed` (plus `.inline-embed` and an `embed-title`
header with a `lucide-layout-dashboard` icon and `" " + file.basename` when `ctx.showInline`),
and a click opens the canvas via `openLinkText(ctx.linktext, ctx.sourcePath)` with
`Mod` deciding new-vs-current leaf.

### F19 — CONFIRMED (compact table)

Every row re-checked verbatim in 1.13.7.

| §claim | verdict | proof |
|---|---|---|
| §2 node field set & `type` after geometry | ✅ | `app/own/2020-O7.js:167-183`, `app/own/2022-W7.js:364-368,457-461,609-617` |
| §2 edge emission order | ✅ | `app/own/2028-n9.js:35-42` |
| §2 `subpath` omitted when empty; `backgroundStyle` omitted when `cover` | ✅ | `app/own/2022-W7.js:125,617` |
| §2 viewport in leaf state, never the document | ✅ | `app/own/2018-k7.js:963-974`, `app/own/2028-n9.js:851-854` |
| §2 read-only flag in `"canvas-"+path` local storage | ✅ | `app/own/2028-n9.js:560-577` |
| §3 unknown top-level keys preserved | ✅ | `app/own/2018-k7.js:371-374` |
| §3 `file` is a vault-relative path | ✅ | `app/own/2022-W7.js:124` |
| §3 missing edge side self-heals and is written back | ✅ | `app/own/2018-k7.js:448`, `app/own/2028-n9.js:58` |
| §3 id = 16 lowercase hex | ✅ | `app/own/0558-Gd.js:2` |
| §3 integer geometry | ✅ | `app/own/2020-O7.js:194,219` |
| §8.2 edges attach at face midpoints, 4 sides only | ✅ | `app/own/2024-Z7.js:2` |
| §8.3 default edge = `none` + `arrow` | ✅ | `app/own/2028-n9.js:6-7` |
| §8.4 group membership geometric, recomputed at grab | ✅ | `app/own/1982-L8.js:2`, `app/own/2018-k7.js:1614-1630` |
| §8.5 group `zIndex = -width*height` | ✅ | `app/own/2022-W7.js:710-717` — but see note |
| §8.7 file node = `embedRegistry` dispatch | ✅ | `app/own/2022-W7.js:172-186` |
| §8.8 zoom-breakpoint virtualisation, ≤10 attaches/frame | ✅ | `app/own/2020-O7.js:225`, `app/own/2018-k7.js:938-941` |
| §8.9 2000 ms save debounce, external change re-imports | ✅ | `app/own/1813-CZ.js:6`, `app/own/2018-k7.js:407-408` |
| §9.1 text card = real editor, `useIframe` | ✅ | `app/own/2022-W7.js:432-437` |
| §9.1 link node `<webview>` desktop / `<iframe>` web | ✅ | `app/own/2022-W7.js:468-484` |
| §9.3 `clamp(dist/2, 70, 150)`, 7 px inset, 13×10.4 arrow, 4 side angles | ✅ | `app/own/2027-t9.js:2`, `2025-J7.js:2`, `app/own/2028-n9.js:145-150`, `:2-7` |
| §9.3 dual path: visible + 24 px hit path | ✅ | `app/own/2028-n9.js:112-118` |
| §9.4 defaults 250×60 text, 400×400 file, 640×360 embeddable URL | ✅ | `app/own/2018-k7.js:73-80,777-780` |
| §9.5 snap tolerance `15/scale`, corners + centres, guides in `.canvas-snaps` | ✅ | `app/own/2018-k7.js:590-593,1080-1105`, `app/own/1989-j8.js:2` |
| §9.6 transform string, zoom clamp `[-4,1]`, easing `1-0.984^dt` | ✅ | `app/own/2018-k7.js:808,821,839-841` |
| §9.6 minimap is not a live navigator | ✅ | `app/own/2029-y9.js:13` |
| §9.7 PNG only, 16284 px cap | ✅ | `app/own/2018-k7.js:1823,1873-1883` |
| §12 `canvasFile#^nodeId` fold convention | ✅ | `app/own/2022-W7.js:443-451` |

**Note on §8.5.** The rule is right but the *mechanism* is not what §8.5 implies: the group
**overrides both `updateZIndex` and `renderZIndex`** (`app/own/2022-W7.js:710-717`) so it never
consumes `canvas.zIndexCounter` — and, because it bypasses the base `renderZIndex`
(`app/own/2020-O7.js:272-275`), **a sole-selected group is not raised above the nodes it
contains**, unlike every other node type.

### F20 — CORRECTION: `domains/canvas.md` §11.1 is stale about Corbomite, and the real
remaining deltas are different (and smaller) than it claims

Three rows are now wrong in Corbomite's favour, and one is wrong against it:

- **Bezier** — `CanvasBezierStrategy` already uses `std::clamp(dist / 2.0, 70.0, 150.0)` with `kFaceInset = 7.0` and the 13×10.4 polygon (`libs/canvas/src/CanvasBezierStrategy.cpp:29,38,56-62`). §11.1's "curvature constant differs" is **fixed**.
- **Group membership** — `GroupItem::containedItems` uses `myRect.contains(item->sceneBoundingRect())`, i.e. **full containment**, with a comment explicitly citing Obsidian's rule (`libs/canvas/src/GroupItem.cpp:121-139`). §11.1's "Corbomite uses center test" is **fixed**.
- **Unknown-field passthrough, default omission, integer rounding, side self-heal** — all present (`libs/canvas/src/CanvasDocument.cpp:66-75,104-112,121,145-160,169,224,246-249`). §11.1's four "priority interop fixes" are **done**.
- `pickSideToward` (`libs/canvas/src/CanvasDocument.cpp:66-75`) compares `|dy|*W > |dx|*H`, which is **algebraically identical** to Obsidian's `atan2` sector test in `$7` (`app/own/2023-$7.js:2`), whose boundary rays sit at `atan2(H, W)`. Equivalent.

The deltas that **do** remain, all HIGH severity and all consequences of F1–F6:

1. **Explicit defaults are destroyed.** `fromEnd`, `toEnd`, `color`, `backgroundStyle` are in `knownEdgeKeys()`/`knownNodeKeys()` (`libs/canvas/src/CanvasDocument.cpp:18-41`), so `"fromEnd":"none"`, `"toEnd":"arrow"`, `"color":""` and `"backgroundStyle":"cover"` are dropped where Obsidian preserves them (F5).
2. **Required fields are conditionally written.** `text`/`file`/`url` are emitted only when non-empty (`libs/canvas/src/CanvasDocument.cpp:210-213`), so an empty card, an unresolved file node and an empty link node each *lose a key* that Obsidian always writes (F6).
3. **Unknown node types are coerced, not dropped.** The parse chain has no `else` branch (`libs/canvas/src/CanvasDocument.cpp:98-101`), so `{"type":"table",…}` leaves `node.type` at its `NodeType::Text` default (`libs/canvas/include/corbomite/canvas/CanvasTypes.h:17`) and is written back as `"type":"text"` — Obsidian deletes the node; Corbomite rewrites it as a text card. Different failure, same data loss (F3).
4. **Serializer shape.** Obsidian is tab-indented with the all-primitive-collapse rule and no trailing newline (F1); Corbomite is `QJsonDocument::Indented` (4 spaces), and `mergeExtras` appends `extraData` **last** while Obsidian spreads it **first**, with `type` before geometry in Corbomite and after it in Obsidian (F7).
5. **Not implemented at all** (all specified above): the align/arrange/distribute/justify family (F10), the `canvas/data.json` settings surface (F11), `gridSpacing`/`snapDistance` snap maths and guides (F12), zoom-breakpoint content virtualisation (F12), the 100-deep history with 250 ms coalescing (F17), `canvas:node-connection-drop-menu` and the file-explorer "New canvas" item (F17), the `.canvas` link index with its 5000-unit parse threshold and rename rewriting (F14), `convert-to-file` with edge re-pointing (F9), `canvas:convert-to-file`'s viewport-correct behaviour, the link-node URL transform and the 640×360 default (F16), and the file-drop 10×10 grid (F17).

---

## Why noticed now

Re-verifying `domains/canvas.md`'s `.canvas` disk contract before treating §3 and §11.1 as
the spec for `libs/canvas`. The document was written against a destroyed
`app.pretty.js` monolith whose every citation is now dead, and §3 — the section with the
highest severity in the whole canvas audit, because `.canvas` is an on-disk vault format — turned
out to name the wrong serializer, omit the array-reordering behaviour, and miss three distinct
silent-loss paths. The Corbomite-side table in §11.1 had meanwhile drifted in both directions,
so a fresh implementation reading it would have re-broken things that are already correct.

## Action taken

No domain doc or existing addendum edited. This file is the citation source for canvas disk-contract
work: F1 gives the exact printer to port, F2–F6 are the round-trip-loss checklist to gate a
`libs/canvas` golden test on (Obsidian-written fixture containing an unknown node type, a
dangling edge, explicit `fromEnd:"none"`, an empty text card and `backgroundStyle:"cover"`),
F11 is the `canvas/data.json` schema to mirror, and F20 item 1–3 are the concrete
`CanvasDocument` fixes still open. Corbomite deliberately does **not** replicate
`app/own/2018-k7.js:548`'s Down-nudge `r.x += o` bug. `PARITY-MATRIX.md` and the canvas rows of
any cluster plan should cite this file rather than `domains/canvas.md` §3/§11.