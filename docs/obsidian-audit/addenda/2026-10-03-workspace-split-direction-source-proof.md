# Workspace split `direction` — six independent source confirmations in 1.13.7, and a documentation conflict about which document was wrong

**Date:** 2026-10-03
**Discovered during:** `workspace.md` verification pass against the restored 1.13.7 corpus
**Supersedes / extends:** `domains/workspace.md` §2 (`SplitNode.direction`) and §6
(command table); interacts with
`addenda/2026-10-01-workspace-split-direction-correction.md`.
**Relevant cluster plans:** `superpowers/plans/2026-08-17-cluster-l-workspace-stabilization.md`
(Phase L5)

## Finding

The convention itself is settled and now has direct source proof rather than only an
empirical round-trip: **`direction` names the orientation of the divider line.**
`"vertical"` ⇒ children arranged **left ↔ right**; `"horizontal"` ⇒ children stacked
**top ↔ bottom**. Six independent code paths in 1.13.7 agree, and none of them is
recoverable by accident:

| # | Evidence | Location |
|---|---|---|
| 1 | `getElSize` reads `offsetWidth` for `"vertical"` and `offsetHeight` for `"horizontal"`; `setElSize` writes `style.width` vs `style.height` | `app/own/1206-OI.js:219-222` |
| 2 | `onChildResizeStart` accumulates the drag delta from `clientX` when `direction === "vertical"`, `clientY` when `"horizontal"` | `app/own/1206-OI.js:169` |
| 3 | `getDropDirection` shrinks `rect.width` for `"left"`/`"right"` results and `rect.height` for `"top"`/`"bottom"` | `app/own/1932-r4.js:1427-1442` |
| 4 | `getAdjacentLeafInDirection` walks ancestors whose `direction` equals `l = "left"/"right" ? "vertical" : "horizontal"` | `app/own/1932-r4.js:1761-1763` |
| 5 | `splitLeaf` defaults the new split's direction to `"vertical"`, and `workspace:split-vertical` / `workspace:split-horizontal` are labelled `bd.commands.splitRight()` / `bd.commands.splitDown()` with icons `lucide-separator-vertical` / `lucide-separator-horizontal` | `app/own/1932-r4.js:1449`, `:441-457` |
| 6 | `updateFrameless` recurses into **all** children of a `"vertical"` split but only into `children[0]` of a `"horizontal"` one — only true if `vertical` splits place every pane's header on the top edge | `app/own/1932-r4.js:1884-1894` |

Verbatim, the two load-bearing lines:

```js
// app/own/1206-OI.js:219-222  (WorkspaceSplit)
// t.prototype.getElSize = function(e) {
//   return "vertical" === this.direction ? e.offsetWidth : "horizontal" === this.direction ? e.offsetHeight : void 0;
// }, t.prototype.setElSize = function(e, t) {
//   return e.style.flex = "0 0 auto", "vertical" === this.direction ? e.style.width = "".concat(t, "px") : "horizontal" === this.direction ? e.style.height = "".concat(t, "px") : void 0;
// },
```

```js
// app/own/1932-r4.js:441-457  (command registration)
// c("top", …navigateTabAbove()), c("bottom", …navigateTabBelow()), c("left", …navigateTabLeft()), c("right", …navigateTabRight()), l.addCommand({
//   id: "workspace:split-vertical",
//   name: bd.commands.splitRight(),
//   icon: "lucide-separator-vertical",
//   checkCallback: function(e) {
//     var t = i.activeLeaf;
//     return !(!(t && t.view instanceof ky) || i.isInSidebar(t)) && (e || i.duplicateLeaf(i.activeLeaf, "vertical"), !0);
//   }
// }), l.addCommand({
//   id: "workspace:split-horizontal",
//   name: bd.commands.splitDown(),
//   icon: "lucide-separator-horizontal",
//   …
```

A seventh, user-facing confirmation is the wikilink context menu: "Open to the right",
icon `lucide-separator-vertical`, dispatches `openLinkText(…, "split")`
(`app/own/1932-r4.js:1976-1979`) — i.e. `"split"` mode means *vertical*.

`setLayout` forces `direction` on restore in a way that is only coherent under the
corrected reading (`app/own/1932-r4.js:1016`, `:1020`, `:1024`):

- root → always `"vertical"` — side-by-side panes, Obsidian's default two-column layout;
- left/right sidedocks → always `"horizontal"` — stacked top-to-bottom, which is how a
  sidebar's children actually sit.

### The documentation conflict

The 2026-10-01 addendum states that it "REFUTES" `domains/workspace.md:79`. It does not,
and the audit's own line-79 parentheticals are what source says:

> `workspace.md:79` — `direction "horizontal" (top-to-bottom) or "vertical" (left-to-right)`

Read as "which way the children are arranged", that is exactly the table above. The
2026-10-01 addendum's *own* corrected table also gives `"horizontal"` → top↔bottom and
`"vertical"` → left↔right — i.e. it agrees with the line it claims to refute, and then
describes the agreement as "inverted". The addendum's supporting argument ("`workspace.md:192`
and `:394` record that a restored root is always `"vertical"`, which under the corrected
reading means a side-by-side pair… the audit stated both facts and never reconciled them")
does not hold either: under line 79's gloss as written, `:192`/`:394` reconcile with `:79`
trivially.

What the 2026-10-01 addendum got right is the **implementation** finding and its
consequence: `directionToKddwLocation()` in `WorkspaceSerializer.cpp` did map the two
values backwards, and a nested-split fixture that only asserted JSON shape passed through
the whole bug. That part stands. What it got wrong is the claim about which *document*
was wrong. The inverted thing was never `workspace.md:79`; it was the implementation's
naive "stacking axis" reading of the field name.

## Why noticed now

The corpus was restored (`RE/obsidian/1.13.7/`), which made the split-direction question
falsifiable for the first time. The 2026-10-01 addendum's only evidence was a live
round-trip against an Obsidian-authored vault plus one corroborating invariant — both
good, neither able to say *which* of the two conflicting statements in the record was
the wrong one.

## Action taken

- This addendum written. The 2026-10-01 addendum is **left in place**: its mapping table
  is correct, its account of the implementation bug is correct, and only its attribution
  of the error to `workspace.md:79` is corrected here.
- `addenda/README.md` and `00-taxonomy.md` §Addenda both extended, so an implementer
  arriving via either learns the convention *and* that the frozen doc was already right.

## Confirmed (table)

`WorkspaceSplit` / `WorkspaceSidedock` behaviour, all verified in 1.13.7:

| Claim in `workspace.md` | Verdict | Location |
|---|---|---|
| `type = "split"`, ctor `(workspace, direction, id?)`, `direction` starts `null` | confirmed | `app/own/1206-OI.js:118-121` |
| `serialize()` = parent + `{direction}` | confirmed (key order: after `dimension`, before `children`) | `app/own/1206-OI.js:129-131` |
| `setDirection(dir)` swaps the `mod-vertical` / `mod-horizontal` class | confirmed | `app/own/1206-OI.js:132-133` |
| resize floor is 200 px | confirmed (`Math.max(200, c)`, a bare literal — there is no `AD` constant in 1.13.7) | `app/own/1206-OI.js:201` |
| `finishResize()` converts px → %, then `requestSaveLayout()` + `requestResize()` | confirmed (`a / n * 100`) | `app/own/1206-OI.js:205-218` |
| sidedock `width` written from `size`, `collapsed` only when true; type stays `"split"` | confirmed | `app/own/1206-OI.js:237-239` |
| sidedock defaults: `size = 300`, `collapsed = false` | confirmed | `app/own/1206-OI.js:230` |
| sidedock drag: 200 px min, 80 % workspace max, snap-collapse below the min | confirmed (`Math.clamp(i, II, Math.max(II, .8 * o))`, `II = 200`) | `app/own/1206-OI.js:296-317`, `app/own/1204-gI.js:1250` |
| sidedock `collapse()` retargets `activeLeaf` to the most-recent root leaf | confirmed | `app/own/1206-OI.js:267-271` |
| sidedock empty-children → auto-collapse + empty state; right dock hides right ribbon | confirmed | `app/own/1206-OI.js:326-327` |
| sidedock animation 140 ms `var(--anim-motion-swing)` | confirmed, now a module constant `OI` | `app/own/1206-OI.js:2-5` |
| `WorkspaceContainer`: `allowSingleChild = true`, `onFocus` 100 ms debounce, no focus when a modal is open | confirmed | `app/own/1206-OI.js:330-349` |
| `WorkspaceRoot`: `win = window`, `doc = document`, adds `mod-root` | confirmed | `app/own/1928-Y6.js:301-307` |