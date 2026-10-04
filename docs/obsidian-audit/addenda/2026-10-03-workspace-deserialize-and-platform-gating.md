# `deserializeLayout` behaviour in 1.13.7 — platform gating, input mutation, and what `setLayout` actually honours

**Date:** 2026-10-03
**Discovered during:** `workspace.md` verification pass against the restored 1.13.7 corpus
**Supersedes / extends:** `domains/workspace.md` §1 (`setLayout`), §2, §3 (migration),
§8, and §13 Q5/Q8
**Relevant cluster plans:** `superpowers/plans/2026-08-17-cluster-l-workspace-stabilization.md`
(Phase L2, L5)

## Finding

`deserializeLayout` (`app/own/1932-r4.js:1066-1160`) is the whole read path for
`workspace.json`. Six behaviours in it are not recorded anywhere in the audit.

### 1. Deserialisation **mutates the caller's JSON array**

When a child fails to deserialise it is spliced out of the *input* object:

```js
// app/own/1932-r4.js:1097-1099  (the recursive child walker `n`)
// return n < e.children.length ? [4, this.deserializeLayout(e.children[n], null)] : [3, 4];
// case 2:
// (i = o.sent()) ? ("leaf" !== i.type || "tabs" === t.type || t instanceof GI || ((r = new jI(this)).insertChild(0, i), i = r), t.insertChild(n, i)) : (e.children.splice(n, 1), n…
```

and again for the floating node's non-window children:

```js
// app/own/1932-r4.js:1127
// for (l = new $6(this, e.id), u = 0; u < e.children.length; u++) "window" !== e.children[u].type && (e.children.splice(u, 1), u--);
```

This is why the post-walk dimension loop is safe:

```js
// app/own/1932-r4.js:1122-1124
// if (C.sent(), 0 === i.children.length && !(i instanceof BI)) return [2, null];
// for (u = 0; u < e.children.length; u++) h = e.children[u].dimension, i.children[u].setDimension(h);
// return i.recomputeChildrenDimensions(), [2, i];
```

`e.children.length` tracks the surviving nodes because the splices keep the two in step.
A Corbomite reader that validates into a separate structure has no such coupling and must
reproduce the pairing itself. Note also `0 === i.children.length && !(i instanceof BI)`:
an empty-sidedock is preserved, every other empty container is dropped.

### 2. A bare `leaf` child of a `split` is silently wrapped in a synthesised tab group

Same line, the `true` branch:

```js
"leaf" !== i.type || "tabs" === t.type || t instanceof GI || ((r = new jI(this)).insertChild(0, i), i = r)
```

A `leaf` directly under a `split` becomes `split → tabs → leaf`. Obsidian tolerates
hand-authored or older layouts in this shape; a reader that assumes `split` children are
only `split`/`tabs` will mis-handle it.

### 3. Leaf restore: `e.state || {}`, then `group`, then `pinned` — in that order, via `togglePinned`

```js
// app/own/1932-r4.js:1143, 1151
// f = new _I(this.app, e.id), v = e.state || {}, C.label = 9;
// … f.view ? (e.group && f.setGroup(e.group), e.pinned && f.togglePinned(), [2, f]) : (f.detach(), [2, null]);
```

Three details: a `leaf` node with no `state` is fed the literal `{}` to `setViewState`; a
leaf whose view fails to materialise is `detach()`ed and the node is dropped (`null`); and
`pinned` is applied with `togglePinned()`, **not** `setPinned(true)` — so a node carrying
`pinned: true` while `ViewState.pinned` is absent flips the leaf to pinned either way, but
a reader that implements "read the flag" and "call the setter" will diverge in the
`setGroup`-before-`setPinned` interaction (`setGroup` forces pinned if any group member is
pinned, `app/own/1209-jI.js:754-764`).

### 4. Popout windows are gated on desktop **and Electron major ≥ 13**

```js
// app/own/1932-r4.js:1132
// "window" === e.type && rd.isDesktopApp && mf >= 13 ? (c = new Z6(this, e.id, e), [4, n(c)]) : [3, 6];
```

`mf` is the **Electron** major version, not an Obsidian or plugin version — `app/own/0579-expressionstatement.js:3` sets `ff = parseInt(process.versions.electron.split(".")[0])` and `app/own/0580-vf.js:3` aliases `mf = ff`. (Cross-check: `app/own/0588-Mf.js:3` uses `mf < 40` to pick `electron.clipboard` vs `electron.remote.clipboard`.) The same constant is the popout capability guard:

```js
// app/own/1931-i4.js:2-11  (i4 = canPopoutWindow)
// function i4() {
//   if (!rd.isDesktopApp) {
//     var e = bd.interface.msgDesktopOnlyFeature();
//     throw new Aw(e), new Error(e);
//   }
//   if (mf < 13) {
//     e = bd.interface.msgUpgradeInstaller();
//     throw new Aw(e), new Error(e);
//   }
// }
```

On mobile, a `"window"` node falls through to the unknown-type branch and becomes `null`.
The doc's §3 "unknown `type` values cause `deserializeLayout` to return `null`" is
therefore slightly under-stated: `"window"` and `"mobile-drawer"` are *also* `null` when
the platform doesn't support them, without being unknown.

### 5. `stacked: true` is silently dropped where `Platform.canStackTabs` is false

```js
// app/own/1209-jI.js:192-193  (WorkspaceTabs.setStacked)
// t.prototype.setStacked = function(e) {
//   rd.canStackTabs || (e = !1), this.isStacked !== e && (this.isStacked = e, …)
```

```js
// app/own/1932-r4.js:1138
// (p = new jI(this, e.id), e.stacked && p.setStacked(!0), [4, n(p)]);
```

Round-tripping a `stacked: true` layout through a platform without stacked tabs drops the
flag on the next save. Same class of behaviour for the drawer: `setPinned` forces `false`
when `!Platform.canPinSidebar` (`app/own/1209-jI.js:1187-1188`).

### 6. When `!Platform.canSplit`, the root tree is flattened to a single tab group

```js
// app/own/1932-r4.js:1108-1115
// if ("root" === t) {
//   if (!rd.canSplit && (r = [], (o = function(e) {
//     "tabs" === e.type ? r.push(e) : e.children && e.children.forEach(o);
//   })(e), a = r[0])) {
//     for (u = 1; u < r.length; u++) (w = a.children).push.apply(w, r[u].children);
//     e.children = [a];
//   }
//   i = new Q6(this, e.direction, e.id);
// }
```

All split structure below the root is discarded (collect every `tabs` node at any depth,
concatenate all their children into the first one). This is phone/tablet behaviour, but it
also means the **input JSON is rewritten**, so a mobile save of a desktop-authored file
loses the split tree irreversibly — relevant to `workspace-mobile.json` round-tripping.

### 7. `active` is resolved by leaf id, and skipped if it lives in a collapsed dock

```js
// app/own/1932-r4.js:1036  (setLayout, tail)
// E = e.active, (M = this.getLeafById(E)) && (x = (S = this).leftSplit, T = S.rightSplit, D = M.getRoot(), D === x && x.collapsed || D === T && T.collapsed || this.setActiveLeaf(M, {…
```

A `desktop` left dock with no `left` key is created **expanded**, and a `desktop` right
dock with no `right` key is created **and immediately collapsed** — same line:

```js
r || (n ? (r = this.leftSplit = new YI(this)).collapse() : r = this.leftSplit = new BI(this, "horizontal", "left")),
o || (o = this.rightSplit = n ? new XI(this) : new BI(this, "horizontal", "right")).collapse(),
```

The doc's §1 `setLayout` summary ("falls back to a fresh `WorkspaceRoot("vertical")` with
one leaf opening `lastOpenFiles[0]` if no `.main`") is right about the root, but the
fallback leaf is opened from `getLastOpenFiles()[0]` — the **filtered, 10-entry-capped**
accessor — not from `lastOpenFiles[0]` directly (see
`addenda/2026-10-03-workspace-last-open-files-quota.md`):

```js
// app/own/1932-r4.js:1030
// (v = e["left-ribbon"]) && this.leftRibbon.load(v), i ? [3, 11] : (i = this.rootSplit = new Q6(this, "vertical"), m = new jI(this), g = new _I(this.app), m.insertChild(0, g), i.insertChild(0, m), (y = this.getLastOpenFiles()).length > 0 ? (w = y[0], (k = t.getAbstractFileByPath(w)) && k instanceof GD ? [4, g.openFile(k)] : [3, 10]) : [3, 10]);
```

## Why noticed now

`workspace.md` §3's "Migration" bullet is the paragraph that governs how a reader should
degrade a foreign or future `workspace.json`, and it is the one Corbomite's serializer
most needs to match. Reading the whole switch statement for the first time since the
corpus was restored showed that "unknown type → `null`" is only one of six reasons a node
disappears, and that three of them are platform-dependent rather than version-dependent.

## Action taken

- No code change: `WorkspaceSerializer.cpp` is a desktop-only reader and none of these
  gates fire for it. Recorded for the mobile/popout work that `workspace.md` §11 lists as
  **Missing**.
- §13 Q5 (`workspaces.json` schema) is answered separately in
  `addenda/2026-10-03-workspace-named-workspaces-data-json.md`.
- Linked from `addenda/README.md` and `00-taxonomy.md` §Addenda.

## Confirmed (table)

| Claim in `workspace.md` | Verdict | Location |
|---|---|---|
| `setLayout` falls back to `WorkspaceRoot("vertical") > tabs > empty leaf` when `.main` is absent | confirmed | `app/own/1932-r4.js:1030` |
| `setLayout` forces root `direction` to `"vertical"` after restore | confirmed | `app/own/1932-r4.js:1016` |
| `changeLayout` = `clearLayout()` then `setLayout()` | confirmed, but only when `layoutReady` | `app/own/1932-r4.js:1053-1065` |
| `clearLayout()` nulls the four splits and `activeLeaf`, clears the queue | confirmed (also `layoutReady = false` + `requestLayoutChangeEvents.cancel()`) | `app/own/1932-r4.js:993-1007` |
| `onLayoutReady(cb)` queues before ready, fires synchronously after | confirmed (queued entries carry `pluginId`; failures surface as `msgPluginError`) | `app/own/1932-r4.js:1048-1052`, `:960-983` |
| `layout-ready` fires after the sandbox special-case | confirmed | `app/own/1932-r4.js:945-960` |
| Sandbox detection = desktop + vault named `"Obsidian Sandbox"` + IPC base-path match; detaches every leaf in `rootSplit` + `floatingSplit` and opens "Start here" | confirmed (it does **not** touch the sidedocks) | `app/own/1932-r4.js:945-956` |
| Side dock restore reads `width` then `collapsed` | confirmed (`e.width && s.setSize(e.width), e.collapsed && s.collapse()`) | `app/own/1932-r4.js:1118` |
| Desktop DOM order `[leftRibbon, leftSplit, rootSplit, rightSplit, rightRibbon]` | confirmed (mobile: `[leftSplit, rootSplit, rightSplit]`) | `app/own/1932-r4.js:1036` |
| `floating` children that are not `type: "window"` are filtered out | confirmed | `app/own/1932-r4.js:1127` |