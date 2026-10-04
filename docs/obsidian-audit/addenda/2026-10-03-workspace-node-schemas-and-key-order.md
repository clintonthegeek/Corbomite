# `workspace.json` node schemas and key order in 1.13.7 — plus `mobile-drawer.pinnedSize`, the hard-coded window `direction`, and the stale-bounds overwrite

**Date:** 2026-10-03
**Discovered during:** `workspace.md` verification pass against the restored 1.13.7 corpus
**Supersedes / extends:** `domains/workspace.md` §2 (`SplitNode` / `LayoutJson`);
extends `addenda/2026-06-10-workspace-serialization-corrections.md`
**Relevant cluster plans:** `superpowers/plans/2026-08-17-cluster-l-workspace-stabilization.md`
(Phase L2, L5)

## Finding

### 1. `mobile-drawer` gains a `pinnedSize` field — in neither the doc nor the 2026-06-10 addendum

The 2026-06-10 addendum enumerated the drawer node as `currentTab` (unconditional),
`pinned` (conditional), and "**no** `width` or `collapsed` fields". That still holds in
1.13.7 — but there is now a fourth field:

```js
// app/own/1209-jI.js:1043-1045  (mobile-drawer, class GI)
// t.prototype.serialize = function() {
//   var t = e.prototype.serialize.call(this);
//   return t.currentTab = this.currentTab, this.isPinned && (t.pinned = !0), null !== this.pinnedSize && (t.pinnedSize = this.pinnedSize), t;
// },
```

`pinnedSize` is a plain **pixel number**, not an object, and is written whenever it is
non-`null`. It is the width of the drawer while it is pinned open:

```js
// app/own/1209-jI.js:1189-1190
// t.prototype.setPinnedSize = function(e) {
//   e = Math.min(e, .5 * window.innerWidth), this.pinnedSize = e, this.containerEl.style.width = "".concat(e, "px");
// },
```

Read back unconditionally at `app/own/1932-r4.js:1155`:

```js
// g.currentTab = e.currentTab, g.recomputeChildrenDimensions(), null != e.pinnedSize && (g.pinnedSize = e.pinnedSize), e.pinned && g.setPinned(!0), [2, g];
```

Note the asymmetry: `setPinnedSize()` clamps to `0.5 * window.innerWidth` and the drag
handler has a 150 px floor (`Math.max(150, o)`, `app/own/1209-jI.js:1207`), but
**deserialisation assigns the raw value with no clamp** — a `pinnedSize` of 4000 survives
the round trip. Mobile-only node type, so this never affects a desktop `workspace.json`,
but it is part of the schema `VAULT-FORMAT.md` should record.

Corrected drawer node: `{ id; type: "mobile-drawer"; dimension?; children; currentTab;
pinned?: true; pinnedSize?: number }`.

### 2. Window nodes always serialize `direction: "vertical"` — it is hard-coded, not read

`WorkspaceWindow`'s constructor passes a literal:

```js
// app/own/1928-Y6.js:315-318  (class Z6)
// function t(t, n, i) {
//   var r = e.call(this, t, "vertical", n) || this;
//   if (r.parent = null, r.children = [], r.type = "window", r.size = {}, …)
```

and `deserializeLayout` never feeds `e.direction` back into it — `new Z6(this, e.id, e)`
(`app/own/1932-r4.js:1132`). So a hand-edited `"direction": "horizontal"` on a popout node
is discarded on load and `"vertical"` is written back on save.

### 3. A maximized / minimized / fullscreen popout is saved with **stale** bounds

```js
// app/own/1928-Y6.js:357-369  (WorkspaceWindow.serialize)
// t.prototype.serialize = function() {
//   var t = e.prototype.serialize.call(this), n = this.win;
//   if (n && !n.closed) {
//     t.x = n.screenX, t.y = n.screenY, t.width = n.outerWidth, t.height = n.outerHeight, this.updateSize();
//     var i = this.size;
//     i && Object.assign(t, i);
//     try {
//       var r = this.win.electronWindow;
//       t.maximize = r.isMaximized(), t.zoom = r.webContents.zoomLevel;
//     } catch (e) {}
//   }
//   return t;
// }, t.prototype.updateSize = function() {
//   if (this.win) {
//     var e = this.win.electronWindow;
//     if (e && !e.isMaximized() && !e.isMinimized() && !e.isFullScreen()) {
//       var t = e.getBounds();
//       this.size = { x: t.x, y: t.y, width: t.width, height: t.height };
//     }
//   }
// },
```

`updateSize()` refreshes `this.size` **only** when the window is in none of the three
maximized/minimized/fullscreen states. `Object.assign(t, this.size)` then runs
**unconditionally**, overwriting the live `t.x/t.y/t.width/t.height` just read off the
window. Net effect: a maximized popout persists `maximize: true` **plus the geometry it
had before it was maximized**. This is a deliberate-looking consequence of the code shape
but it is not recorded anywhere, and it is directly observable in real `workspace.json`
files. A writer that stores true current bounds would differ from Obsidian here.

Two smaller details in the same method: bounds are omitted entirely when
`n.closed`, and `maximize`/`zoom` live inside a `try {} catch {}` that swallows failures,
so they can be silently absent while the bounds are present.

### 4. Root-level key order is `main, left, right, left-ribbon, floating, active, lastOpenFiles`

```js
// app/own/1932-r4.js:1161-1165  (getLayout)
// var e = {};
// e.main = this.rootSplit.serialize(), e.left = this.leftSplit.serialize(), e.right = this.rightSplit.serialize(), e["left-ribbon"] = this.leftRibbon.serialize(), this.floatingSplit.children.length && (e.floating = this.floatingSplit.serialize());
// var t = this.activeLeaf;
// return t && (e.active = t.id), e;
```

```js
// app/own/1932-r4.js:1172-1175  (saveLayout)
// if (!this.layoutReady) return [2];
// e = this.app.vault, (t = this.getLayout()).lastOpenFiles = this.recentFileTracker.serialize(), i.label = 1;
// … n = rd.isMobile ? e4 : J6, [4, e.adapter.write(e.configDir + "/" + n, JSON.stringify(t, null, 2))];
```

`workspace.md` §2's `LayoutJson` lists `floating` **before** `'left-ribbon'`; Obsidian
writes `'left-ribbon'` first and `floating` fourth (and only when
`floatingSplit.children.length > 0`). `lastOpenFiles` is appended after `active`, last of
all. Only the ordering is wrong; the key set is right.

`saveLayout()` also **returns early when `!layoutReady`** (`:1172`) — the doc's §3
lifecycle note does not mention this, and it matters: a save requested during
`clearLayout()`/`setLayout()` is dropped rather than written, so a vault switched to
mid-teardown keeps its previous `workspace.json`.

### 5. `hiddenItems` records carry an explicit `false` for every visible ribbon item

```js
// app/own/1928-Y6.js:221-228  (WorkspaceRibbon.serialize)
// e.prototype.serialize = function() {
//   for (var e = {}, t = 0, n = this.items; t < n.length; t++) {
//     var i = n[t];
//     e[i.id] = i.hidden;
//   }
//   return { hiddenItems: e };
// },
```

Every registered item gets a key; visible ones are written as `false`. `load()` then
reads them with a truthiness test, not a `=== true` test:

```js
// app/own/1928-Y6.js:208-220  (WorkspaceRibbon.load)
// if (e.hasOwnProperty("hiddenItems")) {
//   for (var n = e.hiddenItems, i = 0, r = this.items; i < r.length; i++) {
//     var o = r[i];
//     o.hidden = null !== (t = n[o.id]) && void 0 !== t && t;
//   }
//   var a = Object.keys(n);
//   this.items.sort(function(e, t) { return a.indexOf(e.id) - a.indexOf(t.id); });
// }
// this.onChange(!1);
```

Two facts for a compat writer: (a) key order **is** runtime order, and `load()` calls
`onChange(false)` — it never triggers a save; (b) the **right ribbon is never persisted
at all** — `getLayout()` calls `leftRibbon.serialize()` and nothing else, and `setLayout`
calls `leftRibbon.load(e["left-ribbon"])` and nothing else
(`app/own/1932-r4.js:1030`, `:1163`). Right-ribbon order and hidden state are per-session.

### 6. `dimension` is written under a truthiness test, and the clamp is inclusive

```js
// app/own/1206-OI.js:28-33  (WorkspaceItem.serialize)
// var e = { id: this.id, type: this.type };
// return this.dimension && (e.dimension = this.dimension), e;

// app/own/1206-OI.js:51-55  (setDimension)
// t.prototype.setDimension = function(e) {
//   null !== e && (e <= 0 || e >= 100) && (e = null), this.dimension = e;
//   var t = this.containerEl, n = "";
//   e && (n = String(e)), t.style.flexGrow = n;
// },
```

`<= 0` and `>= 100` are both out of range, so `setDimension(0)` and `setDimension(100)`
normalise to `null` — the valid interval is strictly `(0, 100)`, and `dimension` is only
ever written when non-zero. Key order for every node starts `{id, type, dimension?}`.

`recomputeChildrenDimensions` is **all-or-nothing**, not "normalises non-null sums to 100":

```js
// app/own/1206-OI.js:134-145  (WorkspaceSplit)
// for (var e = this.children, t = 0, n = 0, i = e; n < i.length; n++) {
//   if (null == (a = i[n]).dimension) { t = null; break; }
//   t += a.dimension;
// }
// for (var r = 0, o = e; r < o.length; r++) {
//   var a = o[r];
//   null == t || isNaN(t) ? a.setDimension(null) : a.setDimension(100 * a.dimension / t);
// }
```

If **any** child has a `null` dimension, **every** child is reset to `null` (fully
flexible). One flexible pane poisons the whole sibling set.

## Why noticed now

`workspace.json` is the one Obsidian-shared data file in this domain, so every field's
presence, absence, and ordering is a fidelity obligation. Restoring the corpus made it
possible to read each `serialize()` in full instead of inferring shapes from prose, which
is how `pinnedSize`, the hard-coded window `direction`, and the stale-bounds overwrite
surfaced.

## Action taken

- Code comments in `libs/core/src/WorkspaceSerializer.cpp` not changed by this addendum —
  it already implements the corrected 2026-06-10 shapes. `pinnedSize` and the maximized
  stale-bounds behaviour are mobile/popout-only and outside Corbomite's current scope
  (no popout support; see `workspace.md` §11), so no code change follows yet; the schema
  facts are recorded here for whenever `WorkspaceWindow` support lands.
- Linked from `addenda/README.md` and `00-taxonomy.md` §Addenda.

## Re-verification of the 2026-06-10 addendum (all four claims hold in 1.13.7)

| 2026-06-10 claim | Verdict | 1.13.7 location |
|---|---|---|
| §1 window bounds serialize **flat**, no nested `size`; nested `size` is read-tolerance only | holds; extended by finding 3 above | write `app/own/1928-Y6.js:357-363`; read `app/own/1928-Y6.js:318-324` (`i.size && (i.width = Math.max(i.size.width, 600), i.height = Math.max(i.size.height, 600))`, then only own `["x","y","width","height"]` copied into `this.size`) |
| §2 drawer: `currentTab` unconditional, `pinned` conditional, **no** `width`/`collapsed` | holds; drawer also writes `pinnedSize` (finding 1) | `app/own/1209-jI.js:1043-1045` |
| §3 `pinned: true` at **both** leaf-node and ViewState level | holds | node `app/own/1209-jI.js:559-561`; ViewState `app/own/1209-jI.js:641-648` |
| §4 `group` is a sibling of `state`, not inside ViewState | holds | `app/own/1209-jI.js:561` (`this.group && (t.group = this.group)`); `getViewState` at `:641-648` never writes it |
| §5 `hotkeys.json` records may carry `code`, which takes precedence over `key` | holds | `app/own/0833-$w.js:69` — `key: a.code ? Xw(a.code) : a.key`; mapper `app/own/0831-Xw.js:2-4` — `e.startsWith("Key") && 4 === e.length ? e.charAt(3) : e` |

`getViewState()`'s exact shape, for reference (`app/own/1209-jI.js:641-648`) — key order
`type, state, pinned?, icon?, title?`, with `icon`/`title` written for **any** view
present (not only deferred ones), `|| void 0` so falsy values are dropped, and `title`
trimmed:

```js
// WorkspaceLeaf.prototype.getViewState = function() {
//   var e = this.view, t = this.pinned,
//     n = { type: e ? e.getViewType() : "empty", state: e ? e.getState() : {} };
//   return t && (n.pinned = !0), e && (n.icon = e.getIcon() || void 0, n.title = e.getDisplayText().trim() || void 0), n;
// },
```

## Confirmed (table)

| Claim in `workspace.md` | Verdict | Location |
|---|---|---|
| `readWorkspaceFile()` returns `{}` on any error | confirmed (`catch → n = {}`) | `app/own/1932-r4.js:910-927` |
| Path is `<configDir>/workspace.json`, `workspace-mobile.json` on mobile | confirmed (constants `J6` / `e4`) | `app/own/1932-r4.js:918`, `:1175`; `app/own/1928-Y6.js:409-410` |
| Format is `JSON.stringify(obj, null, 2)` | confirmed | `app/own/1932-r4.js:1175` |
| Write is wrapped so failures are swallowed | confirmed (`try/catch`, no rethrow, no Notice) | `app/own/1932-r4.js:1175-1179` |
| tabs node: `currentTab` omitted when 0, `stacked` only when true | confirmed (`this.currentTab > 0 && …`) | `app/own/1209-jI.js:194-196` |
| tabs `selectTabIndex` clamps to `[0, length-1]` | confirmed (`Math.clamp`) | `app/own/1209-jI.js:197-199` |
| stacked flag captured on dissolve via `workspace.lastTabGroupStacked` | confirmed | `app/own/1209-jI.js:325`; re-applied at `app/own/1932-r4.js:1823` |
| `WorkspaceFloating`: `type = "floating"`, `allowSingleChild = true`, `autoManageDOM = false` | confirmed | `app/own/1928-Y6.js:308-314` |
| `WorkspaceWindow` `close()` reparents the active leaf, detaches subtree leaves, unloads `Component`, fires `window-close`, nulls `win` | confirmed | `app/own/1928-Y6.js:390-404` |
| `WorkspaceWindow` `removeChild` auto-closes at 0 children | confirmed | `app/own/1928-Y6.js:405-406` |
| Unknown `type` values make `deserializeLayout` return `null` and the parent drop the node | confirmed | `app/own/1932-r4.js:1157` |
| `id` is a 16-char random token (`Gd(16)`), stable across save/load | confirmed | `app/own/1206-OI.js:9`; group ids too, `app/own/1209-jI.js:752` |