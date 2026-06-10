# Audit addendum — workspace.json serialization corrections (window bounds, mobile drawer, pinned duplication, group placement, hotkey `code`)

**Corrects:** `domains/workspace.md` §2 (SplitNode schema), `domains/views.md` §2 (ViewState shape), `domains/settings.md` §2 (`hotkeys.json` schema).

**Date:** 2026-06-10
**Discovered during:** verification pass of audit claims against the decompiled source.
**Source:** decompiled Obsidian 1.12.7 corpus at `/home/clinton/bin/ObsidianRAW/audit/` (paths relative to `renamed/obsidian/`). All claims re-checked 2026-06-10.

These corrections matter for byte-faithful `workspace.json` writes — Corbomite emitting the audit-documented shapes would produce files Obsidian reads fine but that diff against Obsidian's own output.

## 1. Window (floating) nodes serialize bounds FLAT — no nested `size` key is written

**Wrong claim:** workspace.md §2 SplitNode schema gives the window node as `{ id; type: "window"; …; x?; y?; width?; height?; size?: {width, height, x, y}; maximize?; zoom?; }` and §1 ("Serialisation: parent's … + `{type:"window", x, y, width, height, size, maximize, zoom}`") — i.e. a nested `size` object alongside the flat fields.

**Verified reality:** `WorkspaceWindow.serialize` (`tree/obsidian/workspace/WorkspaceFloating.js:302-318`) writes `t.x/t.y/t.width/t.height` from the live `window` object, then **spreads** `this.size` (which `updateSize` populates as flat `{x, y, width, height}`, `:331-336`) onto the same top level:

```js
// tree/obsidian/workspace/WorkspaceFloating.js:310-312
var i = this.size;
i && Object.assign(t, i);
```

No `size` key is ever written. The **constructor** tolerates a nested `opts.size` on read (`:209-211`: `i.size && ((i.width = Math.max(i.size.width, 600)), …)`) — that read-tolerance is what the audit mistook for a write shape. Read-tolerance ≠ write-shape.

**Implementation impact:** a writer emitting `size: {…}` produces a key Obsidian itself never writes (round-trip diff noise); readers must still accept it for forward-compat, mirroring `:209-211`.

## 2. Mobile-drawer serialize: `currentTab` unconditional, optional `pinned` — no `width`/`collapsed`

**Wrong claim:** workspace.md §2 SplitNode schema — `{ id; type: "mobile-drawer"; width?; collapsed?: true; children: SplitNode[]; currentTab?; pinned?: true; }`.

**Verified reality:** the mobile-drawer class (`WD`, `tree/obsidian/workspace/WorkspaceLeaf.js:1500`, `type = "mobile-drawer"` at `:1503`) serializes only:

```js
// tree/obsidian/workspace/WorkspaceLeaf.js:1916-1922
(t.prototype.serialize = function () {
  var t = e.prototype.serialize.call(this);
  return ((t.currentTab = this.currentTab), this.isPinned && (t.pinned = !0), t);
}),
```

`currentTab` is written **unconditionally** (including `0` — unlike `WorkspaceTabs`, which omits the default), `pinned` only when true, and there are **no** `width` or `collapsed` fields in the drawer node. The audit apparently copied the sidedock-split variant's fields onto the drawer.

## 3. A pinned leaf carries `pinned: true` at BOTH levels

**Under-specified in:** workspace.md §2 / views.md §2 — each doc shows `pinned` once, at different levels, without noting the duplication.

**Verified reality:** the leaf node's own serialize writes `pinned` at the node level *and* embeds `getViewState()`, which writes it again inside the state object:

```js
// tree/obsidian/workspace/WorkspaceLeaf.js:905-913
(t.prototype.serialize = function () {
  var t = e.prototype.serialize.call(this);
  return (
    this.pinned && (t.pinned = !0),
    (t.state = this.getViewState()),
    this.group && (t.group = this.group),
    t
  );
}),
// tree/obsidian/workspace/WorkspaceLeaf.js:1056-1070 (getViewState)
var t = this.pinned, n = { type: …, state: … };
return (t && (n.pinned = !0), …);
```

A pinned leaf therefore serializes as `{ id, type: "leaf", pinned: true, state: { type, state, pinned: true, … }, … }`. Writing it in only one place is read-compatible but not byte-faithful.

## 4. `group` is a sibling of `state` in the leaf node — NOT inside ViewState

**Wrong claim:** views.md §2 places `group?: string` inside the `ViewState` object ("persisted per leaf … built by `WorkspaceLeaf.getViewState`").

**Verified reality:** same evidence as #3 — `getViewState()` (`WorkspaceLeaf.js:1056-1070`) never writes `group`; the leaf serialize (`WorkspaceLeaf.js:910`) writes `this.group && (t.group = this.group)` on the **leaf node**, beside `state`. workspace.md §2's SplitNode schema (`{ id; type: "leaf"; …; state: ViewState; pinned?: true; group?: string }`) has the placement right; views.md is the wrong one. Also note `getViewState()` does include `icon`/`title` (for deferred-tab painting) — views.md's field list is otherwise correct.

## 5. Stored hotkey records may carry a `code` field — settings.md's schema is incomplete

**Wrong claim (by omission):** settings.md §2 gives `type Hotkey = { modifiers: (…)[]; key: string }` as the full `hotkeys.json` record shape.

**Verified reality:** the hotkey manager's `bake()` resolves each stored record via:

```js
// src/_internal.js:93977
key: a.code ? Nb(a.code) : a.key,
```

i.e. a record may carry a **`code`** field (a physical-key `KeyboardEvent.code` value such as `"KeyB"`); when present it takes precedence over `key`, mapped through `Nb` (`src/_internal.js:93851-93854`: `"KeyX"` → `"X"`, all other codes passed through). A schema-validating reader/writer that only knows `{modifiers, key}` would drop or reject physical-key bindings.

**Implementation impact (all five points):** Corbomite's workspace/session serializer and any `hotkeys.json` round-trip code should follow this addendum, not the frozen schemas, when targeting byte-identical output against an Obsidian-managed vault.
