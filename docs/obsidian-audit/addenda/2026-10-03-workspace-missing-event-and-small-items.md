# Missing Workspace event `hover-link-sources-changed`, and a batch of smaller unrecorded behaviours

**Date:** 2026-10-03
**Discovered during:** `workspace.md` verification pass against the restored 1.13.7 corpus
**Supersedes / extends:** `domains/workspace.md` §4 (event tables), §1
(`registerHoverLinkSource`, `updateOptions`), §10 (menu section orders), §13 Q3
**Relevant cluster plans:** `superpowers/plans/2026-08-20-cluster-o-context-sensitive-ui.md`
(extension surfaces), `superpowers/plans/2026-08-17-cluster-l-workspace-stabilization.md`

## Finding

### 1. `hover-link-sources-changed` — an 18th Workspace event the doc does not list

```js
// app/own/1932-r4.js:2009-2012
// t.prototype.registerHoverLinkSource = function(e, t) {
//   this.hoverLinkSources[e] = t, this.trigger("hover-link-sources-changed");
// }, t.prototype.unregisterHoverLinkSource = function(e) {
//   delete this.hoverLinkSources[e], this.trigger("hover-link-sources-changed");
// },
```

It is the only Workspace event in 1.13.7 that `workspace.md`'s §4 table omits. An
exhaustive enumeration of `trigger("…")` across the four workspace-own statements gives:

| File | Events triggered |
|---|---|
| `app/own/1932-r4.js` | `active-leaf-change`, `file-menu`, `file-open`, **`hover-link-sources-changed`** (×2), `layout-change`, `layout-ready`, `quick-preview`, `resize`, `swipe`, `url-menu`, `window-frame-change` |
| `app/own/1209-jI.js` | `group-change`, `history-change` (×3), `hover-link`, `leaf-menu`, `pinned-change` |
| `app/own/1928-Y6.js` | `window-close`, `window-open` |

`editor-menu`, `files-menu` and `markdown-viewport-menu` are subscribed by `Workspace`
but emitted elsewhere, which is what the doc's table already says. `hover-link-sources-changed`
is triggered *by this domain* and is simply absent.

### 2. `focusLeaf` closes the Settings tab when it lives in the target window

```js
// app/own/1932-r4.js:1566-1576
// t.prototype.focusLeaf = function(e) {
//   var t = e.getContainer();
//   if ((this.isWorkspaceFocused() && t.focus(), e.setEphemeralState({ focus: !0 }), rd.isMobile) && e.getRoot() === this.rootSplit) {
//     var n = this.leftSplit, i = this.rightSplit;
//     n instanceof GI && n.collapse(), i instanceof GI && i.collapse();
//   }
//   var r = this.app.setting;
//   r.win && r.win === t.win && r.close();
// },
```

Two refinements to §1's one-liner ("`focusLeaf` (mobile auto-collapses sidedocks)"):
the mobile collapse only fires when the leaf's root **is** `rootSplit`, and the Settings
view is closed whenever `app.setting.win` matches the target container's window — i.e.
switching panes in a popout closes a Settings tab hosted in that popout.

### 3. `registerEditorExtension` also rebuilds the spellcheck dictionary

```js
// app/own/1932-r4.js:1807-1810
// t.prototype.updateOptions = function() {
//   this.layoutReady && (this.iterateAllLeaves(function(e) {
//     e && e.view instanceof i5 && e.view.updateOptions();
//   }), rd.isDesktopApp && !Zp && Cf(this.app.getSpellcheckLanguages()));
// },
```

§1 and §12 describe `updateOptions()` purely as "reconfigure every MarkdownView's CM
`Compartment`". On desktop (non-mac) it also refreshes the spellcheck languages. Minor,
but it means an extension registration has a side effect outside the editor.

### 4. `setViewState`'s completion sequence, and `updateHeader` / `recordHistory`

The doc's §1 lists `state.close` and `state.done` but not their position. The full tail
of `setViewState` (one long statement; note `q.js` truncates lines at 2000 characters, so
this needed a raw read):

```js
// app/own/1209-jI.js:688
// }), void 0 !== e.group && this.setGroupMember(e.group), t && this.setEphemeralState(t), (e.popstate || c.sync || r && !o) && (s.history = !1), s.layout && this.workspace.onLayoutChange(), this.updateHeader(), s.history && a && this.recordHistory(a), s.done && s.done(), [3, 11];
```

Order: skip-history flag → `onLayoutChange()` → `updateHeader()` → `recordHistory()` →
`done()`. The `setState` options object is exactly
`{ history: false, layout: false, close: false }` (`:658-661`) — `done` is supplied
separately by the caller, not read off the ViewState. `workspace.md` §1's
"`state.done` post-hook" is accurate but implies it is a field of the state payload.

### 5. Tab-group menu has a **trailing** empty section

```js
// app/own/1209-jI.js:42
// n.dom.addClass("mod-tab-list"), n.setParentElement(e), n.addSections(["action", "close", "", "tablist", ""]), …
```

`workspace.md` §10 lists the tab-group menu as `["action", "close", "", "tablist"]` — four
sections; the actual list has a **fifth**, a trailing `""`. Anything that maps a section
id to an ordinal will be off by one at the end. The tab-header list
(`["title", "close", "pane", "open", "action", "find", "info", "info.copy", "view",
"view.linked", "system", "", "danger"]`, `app/own/1209-jI.js:825`) and the file-menu list
(`["title", "open", "action-primary", "action", "info", "info.copy", "view", "system", "",
"danger"]`, `app/own/1204-gI.js:233`) both match the doc exactly.

### 6. `pushUndoHistory` skips empty leaves

```js
// app/own/1932-r4.js:2019-2033
// t.prototype.pushUndoHistory = function(e, t, n) {
//   if (e) {
//     var i = e.getViewState();
//     if ("empty" !== i.type) {
//       var r = e.getEphemeralState(), o = this.undoHistory;
//       o.unshift({ leafId: e.id, state: i, eState: r, parentId: t, rootId: n, leafHistory: e.history.serialize() }),
//         o.length > 10 && o.pop();
//     }
//   }
// },
```

Cap 10 and the entry shape both confirmed; the `"empty"` guard is not in §7/§8. Closing an
empty tab adds nothing to the undo stack.

## Why noticed now

Cluster O treated the menu and event surfaces as the plugin-facing API and the audit's §4
table as its source of truth. A missing event name means a missing signal name in the Qt
port; a five-element section list where the doc says four means an off-by-one in section
ordering. Both are cheap to get right now and expensive to discover later.

## Action taken

- No code change: Corbomite has no hover-link registry to notify yet (see
  `workspace.md` §11, "Missing"), and no plugin menu-injection surface. Recorded so the
  Qt signal list is complete when `Corbomite::Workspace` is built.
- Linked from `addenda/README.md` and `00-taxonomy.md` §Addenda.

## Confirmed (table)

Batch confirmation of the numeric constants and behaviours `workspace.md` states but does
not cite. All verified in 1.13.7:

| Claim in `workspace.md` | Verdict | Location |
|---|---|---|
| `requestSaveLayout = debounce(saveLayout, 1000)` | confirmed (`1e3`) | `app/own/1932-r4.js:5` |
| `requestResize = debounce(onResize, 0)` | confirmed | `app/own/1932-r4.js:5` |
| `active-leaf-change` / `file-open` are debounced **0 ms** | confirmed (`requestActiveLeafEvents = Ml(activeLeafEvents, 0)`) | `app/own/1932-r4.js:5`, `:1581-1588` |
| `layout-change` is debounced **10 ms** and gated on `layoutReady` | confirmed | `app/own/1932-r4.js:6-7` |
| `onLayoutChange` schedules `updateLayout` on an **idle** callback (not a timer) | confirmed (`requestUpdateLayout = xl(updateLayout)`) | `app/own/1932-r4.js:5`, `:1811-1815` |
| `qD.backHistory` cap is **20**; `pushState` clears forward history and fires `history-change` | confirmed | `app/own/1209-jI.js:473-475` |
| `qD.serialize()` shape `{backHistory, forwardHistory}` | confirmed (shallow-copied via `k([], …, !0)`) | `app/own/1209-jI.js:493-497` |
| `qD.deserialize()` fires `history-change` | confirmed | `app/own/1209-jI.js:498-499` |
| `qD.go(n)` shows `msgTabBusy` and aborts if `owner.working` | confirmed | `app/own/1209-jI.js:482` |
| Leaf history state shape `{title, icon, state, eState}` | confirmed | `app/own/1209-jI.js:858-864` |
| `recordHistory` skips empty views and JSON-equal states | confirmed | `app/own/1209-jI.js:865-873` |
| `canNavigate()` = `view.navigation && !pinned` | confirmed | `app/own/1209-jI.js:562-563` |
| `setPinned` propagates to linked-pane group members | confirmed (guarded by `r.pinned !== e`) | `app/own/1209-jI.js:739-746` |
| `setGroup` forces pinned when any member of the target group is pinned | confirmed | `app/own/1209-jI.js:754-764` |
| `setGroupMember` reuses the target's group id or mints a 16-char one | confirmed | `app/own/1209-jI.js:749-753` |
| `detach()` pushes undo history **before** closing the view and unhighlights group members | confirmed | `app/own/1209-jI.js:766-779` |
| `canPin()` = parent is a tab group | confirmed | `app/own/1209-jI.js:747-748` |
| `active-leaf-change` does not re-fire when the active leaf is unchanged | confirmed (`else n && e && this.focusLeaf(e)`) | `app/own/1932-r4.js:1562-1565` |
| `setActiveLeaf` accepts the legacy `setActiveLeaf(leaf, focus)` boolean overload | confirmed (undocumented) | `app/own/1932-r4.js:1551` |
| `isInSidebar(leaf)` = `leaf.getRoot()` is `leftSplit` or `rightSplit` | confirmed | `app/own/1932-r4.js:1802-1806` |
| `getLeaf(mode, dir)`: `"split"`→`splitActiveLeaf(dir)`, `"tab"`/`true`→`createLeafInTabGroup()`, `"window"`→`openPopoutLeaf()`, else `getUnpinnedLeaf()` | confirmed | `app/own/1932-r4.js:1508-1509` |
| `focusNewTab === false` is the only way to open a tab without stealing focus | confirmed | `app/own/1932-r4.js:1522` |
| `duplicateLeaf` restores the source leaf's history via `history.deserialize` | confirmed | `app/own/1932-r4.js:1476-1489` |
| `iterateLeaves` short-circuits on a truthy callback return, and tolerates `(cb, parent)` argument order | confirmed | `app/own/1932-r4.js:1691-1707` |
| `iterateAllLeaves` covers root, left, right, floating | confirmed | `app/own/1932-r4.js:1710-1711` |
| `updateLayout` synthesises a tab group in an empty root and prunes single-member groups | confirmed | `app/own/1932-r4.js:1816-1846` |
| `workspace:undo-close-pane` restores `rootId`, `parentId`, `leafId`, forces `state.active = true`, restores leaf history | confirmed | `app/own/1932-r4.js:515-539` |
| Quit handler cancels `requestSaveLayout`, runs `vault.requestSaveConfig`, adds every dirty `TextFileView.save()` | confirmed (also waits on `vault.adapter.promise` if any view was mid-save) | `app/own/1932-r4.js:250-265` |
| Built-in hover-link sources: `search`/`editor`/`tab-header` `defaultMod: true`, `preview` `defaultMod: false` | confirmed | `app/own/1932-r4.js:886-898` |
| `handleLinkContextMenu` emits `file-menu` with `source = "link-context-menu"` and payload `(menu, file, source, hoverParent)` | confirmed — but **only when the link resolves**; an unresolved link takes the create-file branch and emits nothing | `app/own/1932-r4.js:1961-1988` |
| `registerObsidianProtocolHandler` throws on duplicate | confirmed (delegates to `protocolHandler.register`) | `app/own/1932-r4.js:2005-2008` |