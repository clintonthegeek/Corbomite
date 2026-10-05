# Editor wrapper is half a class; `Editor.commands`, the real keymap, and the fold/history/cache contracts

**Date:** 2026-10-03
**Discovered during:** verification pass of `domains/editor.md` against the restored Obsidian 1.13.7 corpus
**Supersedes / extends:** `domains/editor.md` §1, §2, §3, §6, §8, §9, §11, §12, §14; `addenda/2026-06-10-editor-timing-and-coverage-corrections.md` §1
**Relevant cluster plans:** `superpowers/plans/archive/2026-08-20-cluster-n-rich-clipboard.md`, the Cluster O context-sensitive-UI plan, `superpowers/plans/archive/2026-08-15-cluster-k-markoff-canvas-adoption.md` (table-cell editing)

## Finding

`editor.md` documents a ~30-method thin wrapper. That is **the mixin half of `Editor` and only the mixin half.** In 1.13.7 the exported `Editor` is a two-level construction: a mixin class (`app/own/1263-bO.js:20`, `CO`) supplying the 20 list/heading/IME methods the doc enumerates, and a concrete `EditorView`-owning subclass (`app/own/1823-RJ.js:2-885`, `RJ`) adding **~45 more** methods — undo/redo, `transaction`, scroll, highlight, fold, formatting, table insert, comment toggle, find/replace, clickable-token resolution, and footnote insert. The public export binds the mixin (`app/own/0025-expressionstatement.js:23`: `Editor: () => CO`) and the subclass is created as `function (CO) { … } (CO)` at `app/own/1823-RJ.js:885`, instantiated at `app/own/1823-RJ.js:895` (`r.editor = new BJ(r)`). Neither the doc nor the 2026-06-10 addendum mentions the second half, so §14 #1's "≈30 methods, mostly `processLines`-shaped … bounded task" sizing is roughly **half** the real wrapper.

The second half is where the load-bearing *behaviour* lives, and it is a great deal of it: the exact on-disk markup table for all seven format verbs, a fixed 18-entry command table behind `exec`, the complete editor keymap (≈48 bindings plus a 7-key removal list), undo-history snapshotting across tab switches, fold-state restore with a line-count gate, `smartIndentList` gating the Enter keymap, a 13-class spellcheck exclusion list, the auto-pair bracket sets, and the whole link/tag/footnote suggester trigger grammar.

Two further corrections are load-bearing enough to state up front: **`insertCallout()` selects `NOTE` in *both* branches** (the doc's "`Title` in the wrap-existing-text branch" is wrong), and **`exec` does not look up CM6 commands by string id** — it indexes a fixed static table and throws on an unknown name.

## Why noticed now

Cluster N (rich clipboard) needed the editor's copy/paste hooks and the format-verb markup; Cluster O needed the editor context menu's section list; the table-cell work needed `editTableCell`. All three pull from `1823-RJ.js`, which the frozen doc does not cite at all — every `Editor.js:*` citation in `editor.md` points into `1263-bO.js` (the mixin) and describes only that half. The 2026-10-03 corpus restorations made the rest readable for the first time (the 1.12.7 `_internal.js` cited by the 2026-06-10 addendum no longer exists).

## Action taken

No cluster plan edits were made by this pass (rule 7: linkage is centralised by the caller). `PARITY-MATRIX.md` and the Cluster O plan should cite this addendum before any of the §11 Corbomite-mapping rows are sized or implemented — in particular the `Markoff::Editor` gap list, which currently assumes a ~30-method surface and a `MarkoffSuggestRegistry` with "three built-in trigger kinds" that are actually link / tag / **footnote**.

---

## 1. The two halves of `Editor`

`CO` (mixin, `1263-bO.js:20-325`), prototype methods in source order:

`getDoc` (22) · `setLine` (24) · `somethingSelected` (26) · `setCursor` (28) · `posAtMouse` (33) · `insertText` (35) · `processLines` (39) · `setHeading` (77) · `toggleBlockquote` (88) · `toggleBulletList` (104) · `toggleNumberList` (116) · `toggleCheckList` (128) · `insertCallout` (146) · `insertCodeblock` (182) · `insertMathBlock` (184) · `indentList` (186) · `unindentList` (188) · `newlineAndIndentContinueMarkdownList` (190) · `newlineAndIndentOnly` (285) · `newlineOnly` (306) · `expandText` (310)

`RJ` (concrete subclass, `1823-RJ.js`), prototype methods:

`activeCM` getter (9) · `init` (15) · `refresh` (17) · `getValue` (19) · `setValue` (21) · `getLine` (30) · `lineCount` (33) · `lastLine` (35) · `getSelection` (37) · `getRange` (41) · `replaceSelection` (45) · `replaceRange` (49) · `getCursor` (63) · `listSelections` (79) · `setSelection` (88) · `setSelections` (100) · `exec` (111) · `focus` (114) · `blur` (116) · `hasFocus` (118) · `getScrollInfo` (120) · `scrollTo` (130) · `scrollIntoView` (133) · `transaction` (143) · `wordAt` (184) · `posToOffset` (191) · `offsetToPos` (193) · `undo` (195) · `redo` (197) · `coordsAtPos` (199) · `posAtCoords` (216) · `addHighlights` (223) · `removeHighlights` (249) · `hasHighlight` (263) · `getAllFoldableLines` (271) · `getFoldOffsets` (278) · `foldLess` (281) · `foldMore` (303) · `insertHorizontalRule` (325) · `insertBlock` (339) · `insertMarkdownLink` (361) · `triggerWikilink` (377) · `clearMarkdownFormatting` (394) · `inTableCell` getter (418) · `getTableSelection` (425) · `toggleMarkdownFormatting` (427) · `insertTable` (516) · `toggleComment` (531) · `searchCursor` (557) · `getClickableTokenAt` (633) · `insertFootnote` (790) · static `commands` (864)

Note `posToOffset` / `offsetToPos` are public: the doc's §2 claim that the `EditorPosition` ⇄ CM6-offset conversion "lives in the CM-subclass methods" is right, but those two are the named, callable conversion points, and the source-mode context menu uses them (`1823-RJ.js:1483-1484`).

### 1.1 `exec` is a fixed 18-key table, not a CM6 command lookup — CORRECTION

`editor.md` §1 says `exec(commandName: string)` "invokes a CM6 command by string id". Actual:

```js
}, Editor.prototype.exec = function(e) {
  var n = this.editorComponent.activeCM;
  Editor.commands[e](n);
}, …
}, Editor.commands = {
  goUp: nV, goDown: iV, goLeft: IB, goRight: LB, goStart: vJ, goEnd: QV,
  goWordLeft: zB, goWordRight: qB, indentMore: bJ, indentLess: wJ,
  newlineAndIndent: OH, swapLineUp: SH, swapLineDown: xH, deleteLine: PH,
  toggleFold: function(e) { return Eh(e) || Ch(e); },
  foldAll: mJ, unfoldAll: xh
}, Editor;
```

(`1823-RJ.js:111-113`, `864-884`.)

An unknown name throws `TypeError: Editor.commands[e] is not a function`. `toggleFold` is a two-try fold: language fold first, then whatever `Ch` provides.

### 1.2 `exec`/`focus`/`blur`/`hasFocus` route through `activeCM` — NEW

All four use `this.editorComponent.activeCM` (or `.cm` for the rest), not `this.cm`. And:

```js
Object.defineProperty(t.prototype, "activeCM", {
  get: function() { return this.tableCell ? this.tableCell.cm : this.cm; },
```

(`1823-RJ.js:1669-1672`.) So while an in-editor table cell is being edited, `exec("indentMore")`, `focus()`, `hasFocus()` and `undo()` all address **the cell's** nested CodeMirror instance, while `getValue`/`setValue`/`transaction`/`getSelection` still address the **document**. That split is the whole point of `activeCM`, and a plugin shim that resolves everything against one editor will misroute Tab/undo while a table cell is live.

### 1.3 Contract details the doc leaves implicit

| Behaviour | Source |
|---|---|
| `getLine(n)` returns `""` for `n >= doc.lines` (no throw) | `1823-RJ.js:30-32` |
| `getCursor(side)` returns `null` for any side outside `from`/`to`/`anchor`/`head`; default `"head"` | `1823-RJ.js:63-78` |
| `replaceRange` **always** sets `scrollIntoView: !0`, even when called from `expandText` | `1823-RJ.js:49-62` |
| `transaction` **always** sets `scrollIntoView = !0` | `1823-RJ.js:152` |
| `setSelection` / `setSelections` always set `scrollIntoView: !0` | `1823-RJ.js:98`, `109` |
| `setSelections` treats a missing `head` as `anchor` (collapsed) | `1823-RJ.js:106` |
| `coordsAtPos(pos, true)` converts **viewport → document** coords by subtracting `scrollLeft`/`scrollTop` | `1823-RJ.js:209-214` |
| `posAtCoords(x, y)` calls `cm.posAtCoords({x, y}, false)` and maps to a position or `null` | `1823-RJ.js:216-222` |
| `scrollIntoView(pos, center)` → `y: center ? "center" : "nearest"` | `1823-RJ.js:133-142` |
| `getScrollInfo()` returns exactly `{top, left, clientHeight, clientWidth, height, width}` read off `cm.scrollDOM` | `1823-RJ.js:120-129` |
| `setValue(str)` dispatches a full-document replace **with no `userEvent`** | `1823-RJ.js:21-29` |

That last row matters — see §5.

---

## 2. On-disk markup the doc never spells out

### 2.1 The format table — NEW

```js
Np = {
  bold:          { nodeType: "strong",        surroundingChars: "**", altSurroundingChars: "__" },
  italic:        { nodeType: "em",            surroundingChars: "*",  altSurroundingChars: "_"  },
  code:          { nodeType: "inline-code",   surroundingChars: "`" },
  highlight:     { nodeType: "highlight",     surroundingChars: "==" },
  strikethrough: { nodeType: "strikethrough", surroundingChars: "~~" },
  comment:       { nodeType: "comment",       surroundingChars: "%%", isBlock: !0 },
  math:          { nodeType: "math",          surroundingChars: "$" }
}
```

(`app/own/0506-Rp.js:28-59`.) These are the exact bytes `toggleMarkdownFormatting(key)` emits. Note `math` is **inline** (`$`), `comment` is `%%` and flagged `isBlock`, and only bold/italic carry an alternate (`__` / `_`) that the toggler reads but the emitter uses only when the surrounding mark already uses it. The `altSurroundingChars` are consumed by the un-wrap side (`Up`/`Wp` at `1823-RJ.js:467-472`).

The parallel node-class → flag map used for the "is this already bold?" query is:

```js
i = { italic: "em", code: "inline-code", comment: "comment", bold: "strong",
      highlight: "highlight", strikethrough: "strikethrough", math: "math" }
```

(`app/own/0509-Hp.js:3-10`, `Hp(state, range)` returning `{headingLevel, uniformHeading, bold, italic, code, comment, highlight, strikethrough, math}` at `:44-54`.) Two rules in `Hp` are load-bearing and undocumented: any ancestor node whose class list contains `"formatting"` clears **every** flag (`for (y in i) … g.has("formatting") || (r = !0, g.has(i[y]) || delete i[y])`, then `if (r || (i = {}), i.math)` at `:24`, `:36`) — so a cursor inside formatting marks reports nothing active; and an inline `$…$` nested inside a `$$` block does **not** report `math` (`:36-43` walks `prevSibling` for `format-math-begin` and drops `math` if that node also has `math-block`).

### 2.2 `insertTable` literal — NEW

```js
i = n.text.length ? (t = n.to, i = "\n\n") : "\n";
… changes: [{ from: t, insert: i + "|     |     |\n| --- | --- |\n|     |     |\n" }]
… selection: { anchor: t + i.length + 1 }
```

(`1823-RJ.js:516-530`.) Exactly **two columns**, five spaces per cell, and a `| --- | --- |` separator (space-dash-dash-dash-space, not `|---|---|`). Cursor lands in the first cell of the second row.

### 2.3 `insertHorizontalRule` — NEW

```js
var n = PL(e.doc, t.from).ch > 0 ? 2 : 1, i = "\n".repeat(n) + "---\n";
```

(`1823-RJ.js:325-338`.) One leading blank line at column 0, two when the cursor is mid-line; the `---` is never fenced or indented past column 0 of the cursor.

### 2.4 `insertBlock` rewrites whole lines, not the selection — NEW

```js
o = pe.create(r.ranges.map(function(e) { var t = i.lineAt(e.from), n = i.lineAt(e.to); return pe.range(t.from, n.to); }))
…
a.push({ insert: e + "\n", from: c.from }), a.push({ insert: "\n" + t, from: c.to });
var u = e.length + 1;
… selection: pe.create(r.ranges.map(function(e) { return pe.range(e.anchor + u, e.head + u); }))
```

(`1823-RJ.js:339-360`.) `insertCodeblock`/`insertMathBlock` therefore **normalise a partial-line selection to whole lines** before fencing, and shift the selection by `open.length + 1`. On an empty line this yields ` ```\n\n``` ` — a blank line between the fences — and the cursor on it.

### 2.5 `insertMarkdownLink` / `triggerWikilink` — NEW

```js
i = "[" + n + "]()", r = t.empty ? 1 : 1 + n.length + 2;   // insertMarkdownLink
i = e ? "![[" : "[[";  o = i + r + "]]";  a = n.from + i.length + r.length;  // triggerWikilink(embed?)
```

(`1823-RJ.js:361-393`.) Empty selection → cursor at `from + 1`, i.e. **inside the `[]`**. Non-empty → cursor at `from + 1 + len + 2`, i.e. **inside the `()`**. Both dispatch `userEvent: "input.autocomplete"`.

### 2.6 `insertCallout()` selects `NOTE` in both branches — CORRECTION

The doc (§1, §8, §12) says the clean-cursor branch selects `NOTE` and the wrap-existing-text branch selects `Title`. Both branches select four characters starting at column 4 of a `> [!NOTE]` line, which is `NOTE`:

```js
r = { from: mO(e.line, 4), to: mO(e.line, 8) };            // non-clean branch, 152-155
…
selection: { from: mO(e.line + 1, 4), to: mO(e.line + 1, 8) }  // wrap branch, 177-180
```

(`1263-bO.js:152-155`, `177-180`.) In the wrap branch the inserted text is `"\n> [!NOTE] Title\n> Contents\n"` (`175`) and the selection still lands on `NOTE`, **not** on `Title` — so the user overwrites the callout *type*, and `Title`/`Contents` are left as literal filler. That is a real usability wart worth reproducing or documenting.

The non-clean branch also: prepends `"\n"` when `e.line > 0` (`156`), prefixes `"> "` to every line of the selection (`160-163`), and appends a blank line after the last selected line when one exists (`164-166`).

### 2.7 List toggles normalise the marker, and only blockquote disables blank-skipping — CORRECTION

```js
}, e.prototype.toggleBlockquote = function() { … }, !0 … }, !1);   // 88-103, third arg = false
```

```js
e.prototype.toggleBulletList = function() {
  var e = !1;
  this.processLines(function(t, n) { var i = n.match(bO); return (!i[3] || i[4] || i[6]) && (e = !0), i; },
    function(t, n, i) { return i ? { from: mO(t, 0), to: mO(t, i[0].length), text: i[1] + (e ? "- " : "") } : null; });
},
e.prototype.toggleNumberList = function() {
  var e = !1;
  this.processLines(function(t, n) { var i = n.match(bO); return i[4] || (e = !0), i; },
    function(t, n, i) { return i ? { from: mO(t, 0), to: mO(t, i[0].length), text: i[1] + (e ? "1. " : "") } : null; });
}
```

(`1263-bO.js:88-127`.) Three corrections:

1. **`toggleBlockquote` is the only toggle that passes `skipBlankLines = false`** (`:103`). Blank lines *inside* a blockquote selection are therefore quoted/unquoted, whereas bullet/number/heading toggles skip them (the skip is additionally gated on `r.length > 1`, `:48`).
2. **The bullet emitter always writes `- `** and the number emitter always `1. `. A `* item` / `+ item` / `1) item` that is toggled *off* has its marker rewritten to `- ` / `1. `, and the toggle-off range is `[0, match[0].length)` — so the marker identity is lost even when nothing else changes. A Corbomite toggle must do the same or it will produce `*`/`-` mixes Obsidian never produces.
3. The add-mode predicates are format-specific, not "is a marker missing": bullet-mode triggers if the line is not a bullet (`!i[3]`) **or** is numbered (`i[4]`) **or** has a checkbox (`i[6]`); number-mode triggers only if `i[4]` (already numbered). So converting a checkbox item to a numbered list in one step is not possible via two toggles — the first toggle strips the checkbox into a bullet.

### 2.8 `toggleCheckList(true)` at state 3 emits a bare list marker — NEW

```js
}, e.prototype.toggleCheckList = function(e) {
  var t = 3;
  this.processLines(function(n, i) {
    var r = i.match(bO);
    return t > 2 && " " === r[6] && (t = 2), t > 1 && !r[6] && (t = 1), e && t > 0 && !r[3] && (t = 0), r;
  }, function(n, i, r) {
    …
    c = a + (s || "- ");
    return l.text = c, 1 === t ? r[6] || (l.text = c + "[ ] ") : 2 === t ? l.text = c + "[x] " : 3 === t && (l.text = e ? c : c + "[ ] "), l;
  });
}
```

(`1263-bO.js:128-145`.) The three-state machine is as documented, but the `t === 3` branch has **two** outcomes: with `check` falsy it appends `"[ ] "`, with `check` truthy it emits **only the list marker, no checkbox at all**. So "Toggle done" on a plain paragraph turns it into `- ` (a bullet), not into `- [x] `. The doc's §1 wording ("`check` arg is the 'force checked' flag") does not surface this.

Also note the existing list marker is preserved (`s || "- "`), unlike `toggleBulletList`'s unconditional `- `.

### 2.9 `toggleBlockquote`'s "depth" is a column width, not a nesting count — NEW

```js
var WL = /^\s{0,3}>(\s*)/;
function jL(e) {
  var t = e.match(WL);
  if (!t) return 0;
  for (var n = t[0].length - t[1].length, i = t[1]; i.startsWith("    ");) i = i.substring(4);
  return i.startsWith(" ") ? n + 1 : n;
}
```

(`app/own/1241-WL.js:2`, `app/own/1242-jL.js:2-13`, used at `1263-bO.js:91` and `:100`.) Only the *first* `>` is matched, so `"> > > x"` yields **2**, not 3 — and `toggleBlockquote` uses the value as the delete width, so exactly one level is removed per invocation, which is correct. But the "tab = 4 spaces, odd remainder costs one extra column" rule means the delete range on a tab-indented quote is a column count, not a character count. Anyone reimplementing `toggleBlockquote` from "blockquote depth" will get nested-quote behaviour right and tabbed-quote behaviour wrong.

---

## 3. `expandText`, the update listener, and the debounce semantics

### 3.1 The autocorrect requires a collapsed selection — CORRECTION

```js
e.prototype.expandText = function() {
  var e = this.getCursor("anchor"), t = this.getCursor("head");
  if (e.line === t.line && e.ch === t.ch)
    for (var n = this.getLine(e.line).substr(0, e.ch), i = 0, r = kO; i < r.length; i++) {
      var o = r[i], a = n.match(o.regex);
      if (a) { var s = a[0].length, l = o.replace(a); this.replaceRange(l, gO(e, -s), e); break; }
    }
}
```

(`1263-bO.js:310-324`.) The doc's §1 describes it as running "against `getLine(cursor.line).substr(0, cursor.ch)` **whenever called**". There is an `anchor === head` gate first: **with a non-empty selection the CJK autocorrect is a no-op.** The regex set (`kO`, `1263-bO.js:4-19`) and the first-hit-wins `break` are exactly as documented and the 2026-06-10 addendum's "third pattern is dead" is **confirmed**.

### 3.2 `Editor.setValue` with no `userEvent` fires the autocorrect — NEW, and it inverts a doc invariant

The 2026-06-10 addendum established the gate: `s = o.docChanged && a.some(TL)` where

```js
function TL(e) { return "set" !== e.annotation(_e.userEvent); }
function DL(e) { return e.isUserEvent("input") || e.isUserEvent("delete"); }
```

(`app/own/1226-TL.js:2`, `app/own/1227-DL.js:2`.) `Editor.prototype.setValue` dispatches `{changes: {from: 0, to: doc.length, insert: e}}` with **no `userEvent`** at all (`1823-RJ.js:21-29`), so `TL` is `true` and the 10 ms-debounced `expandText` runs against the freshly replaced document. `Editor.prototype.replaceSelection(e, t)` does `i.userEvent = t; n.dispatch(i)` (`1823-RJ.js:45-48`), so the documented one-argument form `replaceSelection(text)` also leaves `userEvent` undefined — likewise `clearMarkdownFormatting` (`1823-RJ.js:415`), `insertTable` (`:521`) and `insertFootnote` (`:824`).

By contrast the *view's* `set(text)` — the `setViewData` path — explicitly stamps `userEvent: "set"` (`1823-RJ.js:997`), so external file reloads do **not** trigger it.

Net behaviour to reproduce: `editor.setValue(x)` is a **user-visible edit**, not a silent state assignment. It also trips `editor-selection-change` (§6.3).

### 3.3 The listener has **three** debounces, not two — CORRECTION

The 2026-06-10 addendum §1 lists the 10 ms `expandText` and the 50 ms `editorSuggest.trigger`. There is a third:

```js
t.prototype.updateEvent = function() {
  var e = this, t = !1,
    n = Ml(function() { e.editorSuggest.trigger(e.editor, e.file, t), t = !1; }, 50),
    i = Ml(function() { e.editor.expandText(); }, 10),
    r = Ml(this.updateLinkPopup.bind(this), 300);
  return function(o) {
    var a = o.transactions, s = o.docChanged && a.some(TL);
    s && (i(), a.some(DL) && (t = !0)),
    (o.focusChanged || o.docChanged || o.selectionSet) && (n(), r()),
    e.onUpdate(o, s);
  };
}
```

(`1823-RJ.js:1043-1057`.) `updateLinkPopup` is the mobile "tap a link to follow/open it" floating chip (`1823-RJ.js:1288-1327`; desktop no-op because of `if (rd.isMobile && this.sourceMode)`).

### 3.4 Obsidian's `debounce` is a leading-arm, **non-resetting** throttle — CORRECTION (affects every debounce claim in the audit)

`Ml` is the exported `debounce` (`app/own/0025-expressionstatement.js:110`: `debounce: () => Ml`):

```js
function Ml(e, t, n) {
  void 0 === t && (t = 0); void 0 === n && (n = !1);
  var i = null, r = null, o = null, a = 0, s = 0, l = El(), …
  return h;
}
```

with the hot path (`app/own/0355-Ml.js:22-27`):

```js
var h = function() {
  for (var e = [], c = 0; c < arguments.length; c++) e[c] = arguments[c];
  r = this, o = e;
  var p = Date.now();
  return i ? n ? a = s = p + t : l !== El() && s <= p && (l.clearTimeout(i), l = El(), i = l.setTimeout(u, 0)) : (l = El(), s = p + t, i = l.setTimeout(u, t)), h;
}
```

Read the ternary: **if a timer is already pending and the third arg is falsy, the timer is left alone** — the new call's arguments are captured into `r`/`o` (so the *latest* arguments are used) but the fire time is **not** pushed out. Only the first call in a window arms the timer. So this is *not* lodash-style trailing debounce and *not* `QTimer::singleShot`-restart semantics:

- fire time is `firstCall + timeout`;
- calls arriving inside the window **do not** extend it;
- a call arriving in the same tick as a fired-but-not-yet-run timer collapses to a `0 ms` re-arm (`s <= p` branch) rather than waiting a fresh `timeout`;
- third arg truthy switches to a *max-wait* mode (`a = s = p + t`).

Consequences for the editor specifically: `expandText` can fire at most every 10 ms and cannot be starved by fast typing (the 2026-06-10 addendum's "fires ≥10 ms after the document change" is right in magnitude but wrong in mechanism), and `editorSuggest.trigger` runs at most **20×/s** during sustained typing while **every** `focusChanged || docChanged || selectionSet` event still calls `n()` — calls inside the window are dropped, not queued, so a selection change 5 ms after a doc change can be missed entirely. `h.cancel()` and `h.run()` (fire-now-if-pending) exist and are used: `beforeUnload` does `this.requestSaveFolds.run(), this.saveHistory()` (`app/own/1961-Q3.js:713`).

The three editor-scoped debounce constants, verbatim:

| Constant | Value | Site |
|---|---|---|
| `expandText` | **10 ms** | `1823-RJ.js:1049-1051` |
| `editorSuggest.trigger` | **50 ms** | `1823-RJ.js:1046-1048` |
| `updateLinkPopup` | **300 ms** | `1823-RJ.js:1052` |
| `requestOnInternalDataChange` → `view.onInternalDataChange()` | **10 ms** | `1961-Q3.js:689-691` |
| `requestSaveFolds` → `view.onMarkdownFold()` | **500 ms** | `1961-Q3.js:691-693` |
| iOS tap → `selectParagraphAt` | **50 ms** | `1823-RJ.js:897-900` |
| `selectionend` composition settle → `dispatch({})` | **100 ms** | `1822-expressionstatement.js:39-44` |

---

## 4. Enter / Tab / Escape — the keymap the doc attributes to "commands"

### 4.1 `smartIndentList` gates the whole Enter keymap — CORRECTION

`editor.md` §9 states flatly: "The user can press Enter inside a list/blockquote and the next line auto-continues the list marker." The keymap is a **per-view extension** and it is gated:

```js
n.push(sa.of([{ key: "Enter",
  run: function() {
    if (t.app.vault.getConfig("smartIndentList")) {
      var e = t.editor.cm.state, n = e.selection.main.from;
      return zp(e, e.doc.lineAt(n)) ? (t.editor.newlineAndIndentOnly(), !0) : (t.editor.newlineAndIndentContinueMarkdownList(), !0);
    }
    return !1;
  },
  shift: function() { return !!t.app.vault.getConfig("smartIndentList") && (t.editor.newlineAndIndentOnly(), !0); },
  preventDefault: !0
}, { key: "Tab", run: function() { return t.editor.indentList(), !0; },
    shift: function() { return t.editor.unindentList(), !0; } }]))
```

(`1823-RJ.js:1789-1811`, inside `zJ.prototype.buildLocalExtensions`, the live-preview/source view.)

Two behaviours:

1. With `smartIndentList` **off**, `run` returns `false` and the key falls through to the base binding — which is `{key: "Enter", run: OH, shift: OH}` (`app/own/1421-BH.js:210-213`, `OH` = the CM `newlineAndIndent` command). So the list-continuation behaviour is a **setting**, not a constant.
2. `zp(state, line)` is a **verbatim-line predicate** and it splits the two paths: `zp` returns `true` when any syntax node in the line carries class `HyperMD-table-2` or `HyperMD-codeblock` (`app/own/0510-zp.js:2-13`). Inside a table or a code block, Enter uses the plain hanging-indent `newlineAndIndentOnly` — the ~120-line list-continuation state machine is **not** reached. Same predicate gates `clearMarkdownFormatting` (`1823-RJ.js:482`) and `toggleMarkdownFormatting` (`:482`), where it means "skip lines inside tables/code blocks unless the selection is entirely inside one".

`Shift-Enter` is therefore *only* bound when `smartIndentList` is on, and `Tab`/`Shift-Tab` are bound unconditionally.

### 4.2 While the suggest popup is open: four keys swallowed, Escape clears highlights — CORRECTION

```js
var n = function() { return e.editorSuggest.isShowingSuggestion(); };
t.push(sa.of([{key:"ArrowUp",run:n,preventDefault:!0},{key:"ArrowDown",run:n,preventDefault:!0},
              {key:"Enter",run:n,preventDefault:!0},{key:"Tab",run:n,preventDefault:!0},
              {key:"Escape",run:function(){ return e.editor.removeHighlights(), !1; },preventDefault:!0}]))
```

(`1823-RJ.js:1066-1090`.) `isShowingSuggestion()` is `currentSuggest && currentSuggest.isOpen` (`app/own/1925-P6.js:162-164`). So while a suggester is open, arrows/Enter/Tab are consumed **before** any editor binding sees them.

The doc's §9 says "Esc closes it". It does — but the binding here returns `false`, so Escape also runs `editor.removeHighlights()` first and then **falls through** to the base Escape handler, which collapses the selection (§5.5). `onViewClick` does the same highlight clear on a `setTimeout(…, 0)` (`1823-RJ.js:1840-1844`).

### 4.3 The complete base keymap — NEW

Obsidian replaces CM6's default handling wholesale. Two arrays: `UH` (`app/own/1421-BH.js:125-243`, 26 bindings) for arrows/Home/End/Page/Enter/`Mod-a`/Backspace/Delete, all `preventDefault: !0` except the mac-only variants, and `_H = […22 bindings…].concat(UH)` (`1421-BH.js:250-317`). `FJ` then composes, in order, the **list-aware Home/End**, `sJ` (the filtered `_H`), and `MB` (undo/redo) (`1822-expressionstatement.js:12-36`).

**Home / End are list-aware** and reuse the *same* `bO` list regex as the toggles — this is the single most Markoff-relevant item in this section:

```js
var v = i.sliceString(p.from, p.to), m = bO.exec(v);
if (m && m[0] && f.head <= p.from + m[0].length) {
  var g = m[1], y = void 0 === g ? "" : g, b = m[2], w = void 0 === b ? "" : b,
    k = p.from + y.length + w.length, C = k < u ? k : p.from;
  f = pe.cursor(C);
}
```

(`app/own/1821-PJ.js:12-22`, for `Home`/`PJ`; mirrored at `:33-46` for `End`/`IJ`.) **Home lands after the list marker**, not at column 0: on `- foo` it goes to column 2; if the marker is longer than the cursor it falls back to `p.from`.

`_H` verbatim (`1421-BH.js:250-317`):

| Key | mac | run | shift |
|---|---|---|---|
| `Alt-ArrowLeft` | `Ctrl-ArrowLeft` | `JB` | `RV` |
| `Alt-ArrowRight` | `Ctrl-ArrowRight` | `eV` | `NV` |
| `Alt-ArrowUp` | — | `SH` | — |
| `Shift-Alt-ArrowUp` | — | `DH` | — |
| `Alt-ArrowDown` | — | `xH` | — |
| `Shift-Alt-ArrowDown` | — | `AH` | — |
| `Mod-Alt-ArrowUp` | — | `iH` | — |
| `Mod-Alt-ArrowDown` | — | `rH` | — |
| `Escape` | — | `oH` | — |
| `Mod-Enter` | — | `FH` | — |
| `Alt-l` | `Ctrl-l` | `eH` | — |
| `Mod-i` | — | `tH` | `preventDefault: !0` |
| `Mod-[` | — | `HH` (indentLess) | — |
| `Mod-]` | — | `VH` (indentMore) | — |
| `Mod-Alt-\` | — | `BH` (re-indent) | — |
| `Shift-Mod-k` | — | `PH` (deleteLine) | — |
| `Shift-Mod-\` | — | `mV` | — |
| `Mod-/` | — | `HN` | — |
| `Alt-A` | — | `UN` | — |
| `Ctrl-m` | `Shift-Alt-m` | `zH` | — |

`_H` is then filtered (`app/own/1817-aJ.js:2-5`):

```js
aJ = new Set(["Alt-ArrowUp","Shift-Alt-ArrowUp","Alt-ArrowDown","Shift-Alt-ArrowDown","Alt-l","Alt-v","Ctrl-m"]),
sJ = _H.filter(function(e) { return !aJ.has(e.key); })
```

The filter tests `e.key` **only**, so the five removed bindings' mac alternates (`Ctrl-ArrowLeft`-adjacent `Ctrl-l`, `Shift-Alt-m`) **survive on macOS** while being dead elsewhere. `jH` (`1421-BH.js:78-124`, Ctrl-b/f/p/n/a/e/d/h/k/o/t/v) is remapped to mac-only via `.concat(jH.map(e => ({mac: e.key, run: e.run, shift: e.shift, preventDefault: …})))` (`1421-BH.js:243-249`) — emacs cursor bindings are **macOS-only**.

### 4.4 Undo/redo keymap — NEW

```js
MB = [{ key: "Mod-z", run: aB, preventDefault: !0 },
      { key: "Mod-y", mac: "Mod-Shift-z", run: sB, preventDefault: !0 },
      { linux: "Ctrl-Shift-z", run: sB, preventDefault: !0 },
      { key: "Mod-u", run: lB, preventDefault: !0 },
      { key: "Alt-u", mac: "Mod-Shift-u", run: cB, preventDefault: !0 }]
```

(`app/own/1367-MB.js:2-24`.) Undo = `Mod-z`; redo = `Mod-y`, plus `Mod-Shift-z` on macOS and **`Ctrl-Shift-z` on Linux** (a Linux-only entry). `Mod-u` / `Alt-u`+mac `Mod-Shift-u` are undo-selection/redo-selection. `Editor.prototype.undo`/`redo` call the same `aB`/`sB` (`1823-RJ.js:195-198`).

### 4.5 `userEvent` inventory — CORRECTION

The doc's §1 "Transactions" names three strings. The full set emitted by editor code:

| `userEvent` | Emitter | Site |
|---|---|---|
| `"input.indent"` | indentMore (`VH`), the `Mod-]`/`Tab` path | `1421-BH.js:39` |
| `"delete.dedent"` | indentLess (`HH`), the `Mod-[`/`Shift-Tab` path | `1421-BH.js:56` |
| `"indent"` | `BH` = `Mod-Alt-\` re-indent to canonical nesting (**not** `input.indent`) | `1421-BH.js:27` |
| `"delete.line"` | `PH` = `Shift-Mod-k` | `1417-DH.js:32` |
| `"input.type"` | list-continuation Enter (`1263-bO.js:284`), `toggleMarkdownFormatting` (`1823-RJ.js:514`) | — |
| `"input.autocomplete"` | `insertMarkdownLink`, `triggerWikilink` | `1823-RJ.js:376`, `393` |
| `"input.paste"` | clipboard paste hook, URL-over-selection paste | `1823-RJ.js:1098`, `1151` |
| `"input"` | literal `"\t"` insert | `1421-BH.js:75` |
| `"select"` | `selectParagraphAt`, `eH` (`Alt-l`), `oH` (Escape) via `xB` | `1823-RJ.js:916`, `1407-qV.js:86`, `1409-iH.js:14` |
| `"set"` | the view's `set(text)` (external reload) and `resetSyntaxHighlighting` | `1823-RJ.js:997`, `1214` |
| *(none)* | `Editor.setValue`, `Editor.replaceSelection(x)` with no arg, `insertTable`, `insertFootnote`, `clearMarkdownFormatting` | `1823-RJ.js:21-29`, `45-48`, `521`, `824`, `415` |

`"set"` is the only value the autocorrect/suggester pipeline is immune to (§3.2).

### 4.6 Other editor-level keymap/scope facts — NEW

- The local keymap is pushed into a **Facet** `sa = ve.define({ enables: aa })` (`app/own/0273-aa.js:7-9`) whose `enables` installs `Prec.default(EditorView.domEventHandlers({keydown: e => fa(ca(t.state), e, t, "editor")}))` — i.e. Obsidian's own key-handler dispatch, scoped to `"editor"`, layered on top of CM6's `keymap`.
- Link suggester registers, on its own `scope`: `Shift-Enter` → select the synthetic "none" item (unless `e.isComposing`), `Tab` → `suggestions.useSelectedItem(e)` (unless composing), and **`#` and `^` as bare keys** → `useSelectedItem` when the manager's `mode` is `"file"`/`"heading"`/`"file"` respectively (`app/own/1332-GR.js:336-347`).
- Tag suggester's `Tab` builds `"#" + ub(i.tag, i.matches)` and commits it with a bare `editor.transaction({changes:[{from,to,text}]})` — **no `userEvent`** (`app/own/1334-QR.js:3-27`).
- `Mod-i` (`tH`) selects the **enclosing syntax node** and returns it **reversed** (`pe.range(a.to, a.from)`), with an inner-node preference when the cursor is empty (`1407-qV.js:89-107`).
- `Escape` (`oH`) reduces the selection: if `ranges.length > 1`, keep only `main`; if `main` is non-empty, collapse to a cursor at `head` (`1409-iH.js:8-15`).
- `Alt-l`/mac `Ctrl-l` (`eH`) selects whole lines **including the trailing newline** (`Math.min(i + 1, doc.length)`) with `userEvent: "select"` (`1407-qV.js:76-88`).
- `Shift-Mod-k` (`PH`) extends each line range by one character to swallow the break, then `moveVertically` to keep the visual row, with `userEvent: "delete.line"` (`app/own/1417-DH.js:8-34`).
- Context-menu "Select all" is `{line: 0, ch: 0}` → `{line: lineCount()-1, ch: getLine(last).length}` (`1823-RJ.js:1575-1584`).

---

## 5. Other wrapper behaviour the doc omits

### 5.1 Highlights — NEW

`addHighlights(ranges, class, skipWithinLine /*default-ish*/, wholeLine /*default-ish*/)` at `1823-RJ.js:223-248`: builds one `MarkSpec` per distinct class, memoised in a module-level `Map` (`tO`), and dispatches `JL.of(ranges)` plus, per class, an `eO.of(fn)` **predicate** keyed on mark identity. `removeHighlights(class)` dispatches a predicate that removes marks of that class; `removeHighlights()` with **no argument** dispatches `eO.of(() => false)`, i.e. **removes every highlight class at once** (`:258-262`). `hasHighlight(class)` reads the `nO` field; with no argument it is `t.size > 0` (`:263-270`). Note the `this.cm.dom.isShown()` guard on removal (`:250`).

### 5.2 Folding — NEW, including a hard restore gate

`getAllFoldableLines()` (`:271-277`) returns every foldable syntax node with `from`/`to`; `getFoldOffsets()` (`:278-280`) returns a `Set` of currently-folded `from` offsets. `foldLess` (`:281-302`) walks each selection range, keeps the foldables **contained in it**, and folds the **first already-folded** one (`s` filtered by `IL`, then first hit). `foldMore` (`:303-325`) does the mirror: folds the **last not-yet-folded** contained foldable. So `foldMore`/`foldLess` are "one level of nesting per press", innermost-first / outermost-first respectively, and both are no-ops when the level is exhausted.

Persistence and restore (`1823-RJ.js:1216-1248`):

```js
t.prototype.getFoldInfo = function() { … return { folds: n, lines: t.lines }; }
t.prototype.applyFoldInfo = function(e) {
  var t = this.cm;
  if (!e || t.state.doc.lines === e.lines) {
    xh(t);                                     // unfoldAll
    … if (c) n.push(fh.of({ from: c.from, to: Math.max(c.to, l.to) }));
    0 !== n.length && t.dispatch({ effects: n });
  }
}
```

Two consequences: the saved payload carries the **line count** and restore is **skipped entirely — including the `unfoldAll`** — if the document's line count differs. Fold state is flushed through a **500 ms** debounce (`1961-Q3.js:691-693`) and forced on `beforeUnload` (`1961-Q3.js:713`).

`toggleFoldFrontmatter()` (`:1249-1258`) folds **only** when `cm.state.doc.lineAt(0).text === "---"` exactly — a frontmatter opener with trailing whitespace will not fold.

### 5.3 Undo history survives tab switches — NEW

```js
t.prototype.saveHistory = function() {
  var e = this.path;
  if (e) try {
    var t = this.cm.state, n = t.field(rB);
    if (n) {
      var i = rB.spec.toJSON(n, t), r = function(e) { … "changes" in items … };
      if (!r(i.done) && !r(i.undone)) return;
      (VJ = VJ.filter(function(t) { return t.file !== e; })).push({ ts: Date.now(), file: e, data: i, length: t.doc.length }),
      VJ.length > 20 && VJ.shift();
    }
  } catch (e) { console.error(e); }
}
```

(`1823-RJ.js:1000-1027`; module-level `VJ = []` at `:886`.) Called only from the source view's `beforeUnload` (`app/own/1961-Q3.js:713`), alongside `requestSaveFolds.run()`. Restored in `set()` (`1823-RJ.js:944-960`) — **only when the new text has the same length as when history was saved**:

```js
var o = VJ.find(function(e) { return e.file === r; });
if (o && o.length === e.length) { var a = rB.spec.fromJSON(o.data); i.push(rB.init(function() { return a; })); }
```

So: **undo history is per-file, in-memory only, capped at 20 files, snapshotted on unload, and restored only if the document's character length is unchanged.** A vault switch or an external edit that changes the length silently loses the stack. This works because the history field is serialisable — `rB` carries `toJSON`/`fromJSON` (`app/own/1351-ZN.js:44-56`).

`rB` also carries Obsidian's **history facet**, which changes undo grouping from CM6's defaults:

```js
tB = ve.define({ combine: function(e) {
  return it(e, { minDepth: 100, newGroupDelay: 500, joinToEvent: (e,t) => t },
                { minDepth: Math.max, newGroupDelay: Math.min, joinToEvent: (e,t) => (n,i) => e(n,i) || t(n,i) });
} })
```

(`app/own/1351-ZN.js:5-23`.) **`minDepth: 100`** — a new undo event is only created once the accumulated change exceeds 100 characters (CM6's default is 500). Plugins may only tighten it (`Math.max`) or shorten the grouping delay (`Math.min`).

### 5.4 `setViewData` does a minimal diff — NEW

`set(text, force)` (`1823-RJ.js:940-999`): when `cmInit` and not forced, it finds the common prefix/suffix **by whole lines**, computes a char-level `b`/`w` common affix inside the single changed line, and applies **one** `ChangeSpec` with `userEvent: "set"`. The undo stack and selection therefore survive a same-file `setViewData`. Only a genuinely new file (or `force`) takes the `nt.create({doc, extensions})` + `cm.setState` path, which *does* replace the whole state (`:961-969`) — and that path is the one that consults `VJ` for restored history. Forcing a re-init is also what `clear()` does (`:1032-1040`).

`resetSyntaxHighlighting()` (`:1206-1215`) replaces character 0 with itself under `userEvent: "set"` — a deliberate trick to make CM re-highlight the whole document. Its only caller is a **`CodeMirror.getMode` monkey-patch** (`app/own/1932-r4.js:899-908`) that fires when a legacy plugin asks for a mode name that resolves to `null`: the patch restores the original `getMode` and then, once a lazy promise resolves, resets highlighting in every markdown view. The legacy CM5 surface is therefore a **live compat path**, not dead weight.

### 5.5 `searchCursor(query)` — NEW

`1823-RJ.js:557-632`. Contract:

- matching is **NFKD-normalised and lowercased**: `new YH(t.state.doc, e, n, i, function(e) { return e.toLowerCase(); })`, where `YH` applies `GH = s => s.normalize("NFKD")` to both query and text before comparison (`app/own/1421-BH.js:323-327` for `GH`, `:334-336` for the `normalize` composition, `:347-360` for `nextOverlapping`).
- `getIndexAndCount()` returns `[0, 0]` when nothing is current, otherwise the 1-based index of the current match and a **total capped at 9999** (`for (… n < 9999 && !o.next().done;)`).
- `findPrevious()` scans backwards in **10000-character chunks** (`Math.max(t, r - 1e4 - e.length)`, `r -= 1e4`), wrapping from 0 to `doc.length`; `findNext()` wraps from `nextOverlapping().done` back to `i(0).nextOverlapping()`.
- overlapping matches are supported via `nextOverlapping()` (`1421-BH.js:347-360`).
- `replace(text, userEvent)` is a single `replaceRange`; **`replaceAll(text, userEvent)` is a single `editor.transaction({changes})`** — one undo step for the whole document (`:620-631`).
- positions are `{from, to}` **without** `line`/`ch`, unlike every other `Editor` return.

### 5.6 `getClickableTokenAt(pos)` — NEW, and it owns the editor's link rules

`1823-RJ.js:633-789`. The markdown view walks the syntax tree for the line and splits it into typed segments (plain text between tokens), then resolves the segment containing `pos` into one of six token types: `internal-link`, `external-link`, `external-ref-link`, `tag`, `blockid`, `footref` (or `null`).

Rules worth porting:

- **Internal links** (`hmd-internal-link` marked, or `formatting-link` whose whole text is `[[`/`![[`/`]]`, `1823-RJ.js:681`): adjacent same-type segments are merged (`:683-699`), the accumulated text is parsed with the **public** `parseLinktext` (`wD`, `:704-709`), and the returned object is `{type: "internal-link", text: path + subpath, displayText, start, end}` where `start`/`end` span the **whole** link (min `from`, max `to` of the merged segments), not just the hit token. When the target's extension is an image extension (`hb = ["bmp","png","jpg","jpeg","gif","svg","webp","avif"]`, `app/own/0794-hb.js:2`; `Fl` = lowercased extension, `app/own/0365-Fl.js`) **and** the alias parses as a size spec (`FT` matches `/^\s*([0-9]+)\s*(?:x\s*([0-9]+)\s*)?$/`, `app/own/1075-FT.js:2-9`), then `displayText` is blanked (`:714`) — this is the editor-side half of the `![[img.png|200x100]]` carve-out the rendering addendum records from the other direction.
- **URLs** (`:738-759`): `(`/`[`  → take the next segment, `)`/`]` → take the previous one. A `string`-classified URL that `WE(UE(…))`-validates becomes `internal-link` (with `decodeURI` in a `try`, returning `null` on failure, and `Dl(F).trim()` normalisation); a `tm(…)` mailto target is prefixed `"mailto:"`; anything matching `/^([a-z0-9+.-]+):/` is left alone; **everything else gets `https://` prepended**. `hmd-footref2` makes it `external-ref-link` instead. `jE(F)` is applied **only** when the type ended up `external-link`.
- **Tags** (`:761-777`): consecutive `hashtag` segments are concatenated and the range widened, but a non-hashtag segment **at or before** the hit resets and breaks — so `[[#a #b]]`-style adjacency does not bleed.
- **Footrefs** (`:783-788`): `text` is **lowercased**, `start: p.from - 2`, `end: p.to + 1` (so the `[^` and the `]` are inside the range), and `inline-footnote` tokens are excluded. This lowercasing is consistent with the metadata addendum's finding that block ids are lowercased and footnote ids are not — here it is a separate, deliberate lowercase.

Consumers:

- **Click** (`onEditorClick`, `:1332-1374`): in **source mode** any button navigates; in **live preview** it only navigates if the click target passes the class test at `:1352-1368` — for `internal-link`: must be inside `.cm-underline` **and** (`.cm-hmd-internal-link` or `.cm-link`); for `external-link`/`external-ref-link`: must be inside `.external-link`, or inside `.cm-underline` **and** (`.cm-url` or `.cm-link`); `tag` always navigates. A click on the link **hairline** next to a live-preview link therefore does nothing.
- **Follow** (`triggerClickableToken`, `:1271-1287`): `internal-link` → `setTimeout(() => workspace.openLinkText(text, path, mod), 100)` — a **100 ms delay** so the click can complete; `external-*` → `window.open(href, "tab")` for mod-click, `window.open(href)` for alt-click, `window.open(href, buttonNumber)` otherwise, guarded by `$v` (`new URL` parseability, `app/own/0645-$v.js`) with a `Notice` on failure; `tag` → prepend `#` if absent and `internalPlugins.getEnabledPluginById("global-search").openGlobalSearch("tag:" + r)`.
- **Hover** (`onEditorLinkMouseover`, `:1375-1387`): fires `hover-link` with `linktext: text`, except for a footref where it is **`"#[^" + text + "]"`** — a block-path form with a leading `#`.
- **Drag** (`onEditorDragStart`, `:1388-1401`): internal links → `dragManager.dragLink(e, text, path, displayText)`; external → `NP(e.dataTransfer, href)`.
- **`external-ref-link` resolution** (`getClickableTokenHref`, `:1259-1270`): looks the id up in `metadataCache.getFileCache(file).referenceLinks` by `t.id === e.text` and returns `jE(i.link)` — the metadata addendum's "undocumented tenth field" `referenceLinks` is exactly what the editor uses to resolve a reference link for clicking.

### 5.7 `insertFootnote()` — NEW

`1823-RJ.js:790-863`:

```js
a = o ? i.metadataCache.getFileCache(o) : null,
s = "[^" + OD(a) + "]",
…
h = [{ from: c, insert: s }],
d = p.slice(u).toString().match(/\n*$/), f = d[0].length,
v = "\n".repeat(2 - Math.min(f, 2)),
h.push({ from: p.length, insert: v + s + ": \n" }),
t.dispatch({ changes: h, selection: pe.cursor(c + s.length) })
```

- The label is `OD(cache) = max(purely-numeric footnote ids not starting with "[inline") + 1` (`app/own/1125-OD.js:2-13`, `zE = "[inline"` at `app/own/0926-VE.js:4`). **Named footnotes and inline footnotes are ignored when numbering**, so a note with only `[^abc]` gets `[^1]`.
- The **definition is appended at end-of-document**, not after the reference, padded with `"\n".repeat(2 - min(trailingNewlines, 2))` so there are always two newlines between the last body line and the definition.
- Then, in order: `saveImmediately()` (or `save(text, true)`) — **it writes to disk synchronously in the middle of the operation** — `metadataCache.computeFileMetadataAsync(o)`, then a `HoverPopover` in `state: {mode: "source"}` with `waitTime: 0` anchored at `domAtPos(c+1)`'s element and `coordsAtPos(c)`, choosing `right` or `left` by `getComputedStyle(el).direction === "rtl"` (`:838-852`).
- The created embed's `editMode.register` re-focuses the editor if focus fell back to `document.body` (`:854-860`).

---

## 6. Events, listeners and the live-preview field

### 6.1 `updateOptions` re-seeds the live-preview field — CONFIRMED + NEW detail

```js
t.prototype.updateOptions = function() {
  this.cmInit && this.cm.dispatch({ effects: [NJ.reconfigure(this.getDynamicExtensions()),
    kJ.of({ field: xJ, value: !this.sourceMode })] });
}
```

(`1823-RJ.js:1199-1205`.) The doc's §1/§7 description of the `Y$`-equivalent `Compartment` is **confirmed** — `NJ = new Ae()` (`1823-RJ.js:3`, `Ae` = `Compartment`) holds `getDynamicExtensions()`, which ends with `e.push.apply(e, this.app.workspace.editorExtensions)` (`:1196`). NEW: the same dispatch **also re-stamps the live-preview `StateField`**, so the field is not only set on toggle but on every option change; and the doc's "toggle dispatches `q$.of({field: editorLivePreviewField, …})`" is really two call sites (`updateOptions` here and `clear()` at `:1036-1038`).

### 6.2 The four `editor*Field` exports, re-verified — CONFIRMED

`app/own/0025-expressionstatement.js:112-115`:
`editorEditorField: () => EJ`, `editorInfoField: () => MJ`, `editorLivePreviewField: () => xJ`, `editorViewField: () => SJ`.

Factory (`app/own/1819-yJ.js:51-70`):

```js
kJ = Ue.define(),                                   // StateEffect.define()
CJ = function(e) { var t = ke.define({ create: …, update: … }); return t; },
EJ = CJ(null), MJ = CJ(null), SJ = MJ, xJ = CJ(!1)
```

**Confirmed:** `SJ = MJ` — `editorViewField === editorInfoField`, same instance, exactly as the doc's §1 invariant states. **Confirmed:** `xJ`'s initial value is `!1`, never `null`. **Confirmed:** field identity is by reference (`o.value.field === t`), so a single dispatch updates both public names.

**CORRECTION to §1's table:** the doc types `editorEditorField` as `StateField<Editor | null>` ("the `Editor` wrapper"). It is populated with `EJ.init(() => e.cm)` (`1823-RJ.js:1061-1062`) — the raw CM **`EditorView`**, not the `Editor` wrapper. There is **no** public field exposing the `Editor` wrapper; `editorInfoField`/`editorViewField` carry the owning view (`MJ.init(() => e.owner)`, `:1063-1064`), and `Editor` reaches the wrapper through `editorComponent`. §11's mapping row "editorEditorField / editorInfoField / editorViewField → (none)" should be re-read as "the raw CM view + the owning view"; a plugin that wants the `Editor` wrapper's doc-API has no field for it.

### 6.3 `editor-change` and `editor-selection-change` — CONFIRMED triggers, NEW exact conditions

```js
t.prototype.onUpdate = function(t, n) {
  e.prototype.onUpdate.call(this, t, n),
  t.docChanged && this.app.workspace.trigger("editor-change", this.editor, this.owner),
  (t.selectionSet || n || t.focusChanged) && this.app.workspace.trigger("editor-selection-change", this.editor, this.owner);
}
```

(`1823-RJ.js:1845-1846`.) The doc's §4 says "the Obsidian-level `editor-change` event is emitted by `MarkdownView`, not by this domain" — **refuted in placement**: it is emitted by the source-mode view class (`zJ`, `1823-RJ.js:1705-1848`) with payload `(editor, view)`, not by the `MarkdownView` base. And `editor-selection-change` fires on `selectionSet || focusChanged || **n**` where `n` is the `docChanged && isUserEvent` flag — i.e. it **also fires on any non-`set` document change**, which includes undo/redo, `Editor.setValue`, the `expandText` autocorrect, tag-suggest `Tab`, and URL paste.

### 6.4 Editor config surface — NEW (the full gate list and the exact extensions)

Base `getDynamicExtensions` (`1823-RJ.js:1156-1196`):

| Config key | Effect |
|---|---|
| `tabSize` | `EditorState.tabSize.of(i)` |
| `useTab` | indent unit `_u.of(n ? "\t" : "    ")` — **4 spaces**, not the tab size, when off |
| `rightToLeft` **or** `mod-rtl` class on `<body>` | toggles `is-rtl` + `dir` attr, pushes `EditorView.theme({direction: "rtl"})` |
| `spellcheck` | `contentAttributes.of({spellcheck: String(o), **autocorrect: "on"**, **autocapitalize: "on"**, contenteditable: "true"})` — the last three are **hard-coded**, not configurable |
| `livePreview` | `is-live-preview` class + `xJ.init(() => !this.sourceMode)` + the live-preview plugin |
| `autoPairBrackets` / `autoPairMarkdown` | `EditorState.languageData.of(() => [{closeBrackets: {brackets: [...]}}])` where the set is `["(", "[", "{", "'", '"']` and/or `["*", "_", "`", "```"]`, plus `[hq, aq]`, `sa.of(pq)` and `Zj` |
| — | `this.app.isVimEnabled()` → `[Wj, Te.highest(jj), fj, Kj]` |
| — | `e.push.apply(e, this.app.workspace.editorExtensions)` |

Subclass additions (`1823-RJ.js:1814-1826`): `showLineNumber` → line numbers + gutters; `smartIndentList` → `sj(this)`; `showIndentGuide` → `Bq`; `foldHeading`/`foldIndent` → `is-folding` class + `[Lh(), Oq]` + `dJ`/`cJ` + `fJ`, **and a second** `[rO]` pushed when either auto-pair setting is on; `propertiesInDocument === "visible"` → `show-properties` class.

`onConfigChanged` gate, base (`:1197-1198`) and subclass (`:1812-1813`), full union:

`useTab`, `tabSize`, `rightToLeft`, `spellcheck`, `autoPairBrackets`, `autoPairMarkdown`, `smartIndentList`, `vimMode`, `showLineNumber`, `showIndentGuide`, `foldHeading`, `foldIndent`, `propertiesInDocument`

Note `smartIndentList` and `vimMode` are **gated but read elsewhere** (§4.1 keymap, and `app.isVimEnabled()`), not in `getDynamicExtensions` — a reader grepping only that function will conclude both are unused.

### 6.5 Always-on editor configuration — NEW

From `FJ` (`app/own/1822-expressionstatement.js:2-45`), present in **every** Obsidian editor regardless of settings:

- `EditorView.exceptionSink.of(console.error)`
- `EditorState.allowMultipleSelections.of(!0)` — multi-cursor always available
- `ns({eventFilter: function(e) { return !(0 !== e.button || !e.altKey || !e.shiftKey) || (!rd.isDesktopApp || !rd.isLinux) && 1 === e.button; }})` — a rectangular-selection-style extension gated on **Alt+Shift+left-click**, or middle-click on non-Linux / non-desktop
- `EditorState.phrases.of({"folded code": ""})` — a **folded code block renders an empty placeholder**, not the words "folded code"
- `EditorView.lineWrapping` — **line wrapping is not a setting**
- `[BR, EditorView.perLineTextDirection.of(!0)]`
- `EditorView.clickAddsSelectionRange.of(e => e.altKey && !e.ctrlKey && !e.metaKey)` — **Alt+click adds a cursor**
- `Dp.of("url inline-code property footref hmd-footnote math link-has-alias formatting-link hmd-codeblock blockid hmd-frontmatter hashtag")` — the **spellcheck exclusion list**: exactly these 13 token classes get `spellcheck="false"` (`app/own/0499-Mp.js:40-57`, `Dp` is a Facet at `:71`)
- `Uu.of(fn)` returning the column width of the line's leading whitespace
- `domEventHandlers({compositionend: e => setTimeout(() => view.dispatch({}), 100)})` — **an empty CM transaction 100 ms after every IME composition ends**, `userEvent`-less, therefore `docChanged === false` and therefore *not* an `expandText` trigger; it exists to force a re-measure/re-highlight after composition
- the decoration engine (`Tp` → `xp`, `app/own/0499-Mp.js:4-70`) rebuilds only when `tree.length < view.viewport.to || view.compositionStarted` is false — i.e. it **skips decoration rebuilds during composition**

### 6.6 Bidi direction — CORRECTION to the naming and a new rule

The audit calls the helper `LI`. In 1.13.7 it is `FR` + `om` (`app/own/1320-FR.js:2-5`, `app/own/0653-om.js:2-7`):

```js
function FR(e) { var t = bO.exec(e); return t && t[0] && (e = e.slice(t[0].length)), om(e); }
function om(e) { var t = e.match(rm); if (t) { if (t[1]) return "rtl"; if (t[2]) return "ltr"; } return "auto"; }
```

Two corrections/additions: (a) the bidi probe runs on the line **with the list/blockquote prefix stripped** by the same `bO` regex the toggles use, so `- مرحبا` is `"auto"` (first strong char is ASCII `-`… actually the `>`/whitespace prefix class), whereas a bare `مرحبا` is `"rtl"`; (b) `rm` is the **non-UBA** first-strong regex (explicit RLM/LRM plus `\p{sc=…}` script classes) — this is the same `rm` the rendering addendum identifies for per-table-cell `dir`. It is used both by the suggester popup (`1324-VR.js:64`) and by a per-line `dir` decoration (`app/own/1322-NR.js:2-18`) whose **`"auto"` resolves to the previous line's last resolved direction**, seeded with `"auto"`. The doc's §12 #3 ("bidi rendering itself is `rendering/RenderContext.js`'s problem") is wrong for the editor: the editor computes per-line direction itself and inherits downward.

---

## 7. Suggesters — CORRECTION to the built-in list and the coordination rule

### 7.1 Built-ins are link / tag / **footnote** — CORRECTION

`editor.md` §1 names `_I` (file-link), `YI` (headers-of-current-file) and `d0` (tag). Actual registration (`app/own/1925-P6.js:145`):

```js
this.suggests = [], this.currentSuggest = null,
this.addSuggest(new YR(e)), this.addSuggest(new $R(e)), this.addSuggest(new I6(e));
```

- `YR` (`app/own/1332-GR.js:328+`) — the **link** suggester. `onTrigger`: `o = before.lastIndexOf("[[")`, `a = before.lastIndexOf("]")`, requires `-1 !== o && a < o` (**only an unclosed `[[`**); the query is everything after the `[[` **including any `|` alias part**; if the pre-`|` part parses to a path whose extension is in `hb`, it bails (`1332-GR.js:348-372`). Headings are **not** a separate suggester — they are a `mode` of this one (`"file"`/`"heading"`, `1332-GR.js:342-347`). `getSuggestions` cancels the previous `suggestManager.runnable` and returns `null` if cancelled (`1332-GR.js:373-384`), substitutes a single "no results" item (`qR`) for an empty list, then `i.sort(zR)`. `showSuggestions` is overridden to **deselect** when the query is exactly `"^"` or `"#"` (`1332-GR.js:385-391`).
- `$R` (`app/own/1334-QR.js:2+`) — the **tag** suggester. Trigger regex verbatim: `/(^|\s)#[^\u2000-\u206F\u2E00-\u2E7F'!"#$%&()*+,.:;<=>?@^\`{|}~\[\]\\\s]*$/g`, plus the guard `"#" !== line.substr(pos.ch, 1)` — so a `#` immediately after the cursor suppresses it, and the tag must be preceded by whitespace or BOF.
- `I6` (`app/own/1925-P6.js:57+`) — the **footnote** suggester, which the audit does not mention at all. Trigger: `/(?:^|[^\[])(\[\^)([^\]]*)$/` on the text before the cursor; and if the text from the cursor starts with `]` immediately followed by `:`, it returns `null` — i.e. an existing `[^x]:` **definition** suppresses the suggester (`1925-P6.js:75-77`). `selectSuggestion` with `type === "create"` clears the typed text and delegates to `editor.insertFootnote()`; otherwise it rewrites the range to `[^<id>]` and re-focuses on a `0 ms` `activeWindow.setTimeout` (`app/own/1925-P6.js:88-110`).

### 7.2 The manager does **not** always make the first truthy suggester current — CORRECTION

```js
e.prototype.trigger = function(e, t, n) {
  if (e.cm.hasFocus) {
    for (var i = 0, r = this.suggests; i < r.length; i++) {
      var o = r[i];
      if (o.trigger(e, t, n)) return void (n && this.setCurrentSuggest(o));
    }
    this.close();
  }
},
e.prototype.setCurrentSuggest = function(e) { var t = this.currentSuggest; t !== e && (t && t.close(), this.currentSuggest = e); },
e.prototype.reposition = function() { var e = this.currentSuggest; e && e.isOpen && e.updatePosition(); }
```

(`app/own/1925-P6.js:151-170`.) The doc's §1 pseudocode is `for s in suggests: if s.trigger(...): setCurrentSuggest(s); return` — actual is `if (s.trigger(...)) return void (n && setCurrentSuggest(s))`. Three differences:

1. `setCurrentSuggest` is called **only when `n` is truthy** — `n` is the third argument, which is the `t` flag from `updateEvent` (`a.some(DL)`, i.e. "this transaction was `input` or `delete`"). On a pure selection/focus change with no typing, the first suggester whose `onTrigger` matched returns `true` and **stops the loop but does not become current** — so an already-open popup is *not* switched to the newly-matching suggester and is *not* closed either.
2. The whole loop is gated on **`e.cm.hasFocus`** (the raw CM view), not on `editor.hasFocus()` (which routes through `editorComponent.activeCM`, §1.2). A focused table cell therefore does **not** drive the suggester manager.
3. `removeSuggest(e)` closes first if `e` is current, then removes (`:149-150`); `Plugin.registerEditorSuggest` registers and auto-removes on unload (`app/own/1938-c4.js:127-132`) — **confirmed**.

First-non-null-wins with insertion-order semantics is otherwise **confirmed** (`this.suggests.push(e)`, `:147-149`).

### 7.3 `EditorSuggest` base class — CONFIRMED, with three additions

`app/own/1324-VR.js` (`VR`, exported as `EditorSuggest` at `0025-expressionstatement.js:24`, base `TR`):

- **CONFIRMED:** `context = null` and `limit = 100` in the constructor (`:5`); a `mousedown` listener on `suggestEl` calls `preventDefault()` so the popup never steals focus (`:5-7`).
- **CONFIRMED:** `trigger` returns `false` and clears `context` on a non-collapsed selection (`:15`); context shape `{editor, file, start, end, query}` (`:18-24`); `n || this.isOpen` gate (`:25`); sync array → `showSuggestions`, promise → await then `editor.hasFocus() ? show : close` (`:26-41`); `close()` clears `context` before `super.close()` (`:67-69`); `limit` is applied as `limit > 0 && len > limit && (e = e.slice(0, limit))` so `0`/negative means no cap (`:48-49`); async `getSuggestions` returning a falsy non-array is a silent bail (`:29`).
- **NEW 1:** `showSuggestions([])` calls **`close()`** — an empty result list dismisses the popup (`:47-50`). `updatePosition(true)` is called from inside `showSuggestions`, and `updatePosition` itself calls `this.open()` on every invocation (`:58-64`) — so a scroll-driven `reposition()` re-`open()`s the popup each time.
- **NEW 2:** the popup rect is the **union of `coordsAtPos(start)` and `coordsAtPos(end)`** — `Km({top: i.top, left: Math.min(i.left, r.left), right: Math.max(i.right, r.right), bottom: r.bottom}, win).rect` (`:58-63`) — so it is as wide as the whole trigger range, not the caret.
- **NEW 3:** `setInstructions([])` **detaches** the footer element rather than clearing it (`:9-10`).

---

## 8. Context menu — NEW

The editor's right-click menu is built by `onContextMenu` (`1823-RJ.js:1402-1662`) with a **21-entry section list including a trailing `""`**:

```js
new my().addSections(["title","correction","spellcheck","open","selection-link","selection",
  "selection.format","selection.paragraph","selection.paragraph.list",
  "selection.paragraph.heading","selection.paragraph.block","selection.insert.basic",
  "selection.insert.advanced","insert","clipboard","info","info.copy","action","view","","danger"])
```

(`:1441`.) `Wq` (`app/own/1478-Wq.js:2-3`) then adds three more, `${i}.basic` / `${i}.advanced` / `${i}.danger` with `i = "selection.format"` — so the real section ids are `selection.format.basic|advanced|danger`, and the parent is a `setSubmenu()` entry titled "Formatting" with icon `lucide-paintbrush` and `setShowMacWritingTools(!0)`. Items: bold/italic/strikethrough/highlight (basic, icons `lucide-bold` / `lucide-italic` / `lucide-strikethrough` / `lucide-highlighter`), code/math/comments (advanced, `lucide-code-2` / `lucide-sigma` / `lucide-percent`), clear (danger, `lucide-eraser`) — each `setChecked(n.<key>)` off the `Hp` state.

Other concrete menu facts:

- **Spellcheck suggestions are capped at 6**: `f.length > 6 && (f = f.slice(0, 6))`, each with icon `lucide-repeat`; an empty list yields a single disabled "no suggestion" item; "Add to dictionary" uses `lucide-folder-tree` (`:1445-1460`).
- **"Insert link" / "Insert external link" are disabled when the selection contains a newline**: `setDisabled(/\n/.test(S))` (`:1586`, `:1590`).
- **The `selection.paragraph` and `selection.insert` submenus are disabled inside a table cell**: `disabled: r.inTableCell` (`:1595`, `:1631`), where `inTableCell` is `editorComponent instanceof UJ || !!editorComponent.tableCell` (`:418-425`).
- **Heading checkmarks require a uniform level**: `setChecked(x.uniformHeading && x.headingLevel === e)` for levels 1–6 (icons `lucide-heading-1`…`lucide-heading-6`) and `x.uniformHeading && 0 === x.headingLevel` for "No heading" (`lucide-text`) (`:1612-1624`).
- The `Paragraph` submenu icon is `lucide-pilcrow`; Insert is `lucide-list-plus`; `Insert` group items use `lucide-file-signature` / `lucide-table` / `lucide-quote` / `lucide-minus` (footnote, table, callout, horizontal rule) and `lucide-code` / `lucide-sigma-square` for the advanced pair (code block, math block) (`:1593-1657`).
- Clicking a token adds an "Edit link" (`lucide-text-cursor-input`, selects `k.start…k.end`) or "Edit tag" (selects `gO(k.start, 1) … k.end` — **one character in from the start**, skipping the `#`) item in section `selection` (`:1469-1476`).
- A `footref` under the cursor with a matching `cache.footnotes` entry gets "Delete footnote and note" (`lucide-file-signature`) in section `action`, which dispatches **two** changes: remove `[k.start, k.end)` and remove the definition at `[M.position.start.offset - 1, M.position.end.offset]` — the `-1` eats the preceding newline (`:1477-1493`).
- macOS-only "Look up" item: `rd.isDesktopApp && rd.isMacOS && S.length > 0`, section `title`, icon `lucide-library`, selection preview truncated by `nf(S, 25)` (`:1495-1500`).
- Clipboard items use `lucide-scissors` / `lucide-copy` / `lucide-clipboard-check` / `lucide-clipboard-type` / `lucide-box-select` and fall back to the async Clipboard API when Electron `editFlags` are unavailable (`:1502-1584`).
- The whole menu is announced via `workspace.trigger("editor-menu", menu, editor, view)` then `onMenu(menu)` (a subclass no-op hook) (`:1659`).
- A **separate** surface exists for the scroll area: a `contextmenu` listener on `cm.scrollDOM` that fires `workspace.trigger("markdown-viewport-menu", menu.addSections(["view", ""]), view, "source", "gutter")` when the target is in the sizer and not the gutters (`app/own/1961-Q3.js:696-701`). Same trailing-`""` pattern as the workspace addendum's tab-group menu.

---

## 9. Clipboard — NEW (relevant to Cluster N)

`buildLocalExtensions` (`1823-RJ.js:1091-1100`):

```js
t.push(Jo.clipboardCopyHook.of(function(e, t, n) {
  var i = n.text;
  t.setData("text/markdown", i), i.length <= 1e6 && t.setData("text/html", BF + GT(WT(i)));
})),
t.push(Jo.clipboardPasteHook.of(function(t, n, i) {
  var r = e.clipboardManager.handleDataTransfer(n), o = r || i;
  if (o && e.tryPasteUrl(t, o)) return !0;
  if (r) return e.editor.replaceSelection(r, "input.paste"), !0;
  var a = VP(n, "clipboard", !0);
  return a.length > 0 && (e.clipboardManager.insertFiles(a), !0);
}))
```

- **Copy always writes `text/markdown`; `text/html` is written only when the payload is ≤ 1,000,000 characters**, and is prefixed with the literal marker `"<!-- obsidian -->"` (`BF`, `app/own/1296-BF.js:2`: `var BF = "\x3c!-- obsidian --\x3e"`). So a copy larger than a megabyte is markdown-only — an interoperable-copy consumer must tolerate an HTML-less payload at that size.
- **Paste order**: internal `handleDataTransfer` first, then URL-over-selection, then external files.
- `handlePaste` additionally honours a custom `obsidian/properties` clipboard MIME and triggers `editor-paste` (`e.clipboardData.getData("obsidian/properties")`, `app/own/1296-BF.js:16-18`).

**URL-over-selection** (`tryPasteUrl`, `1823-RJ.js:1121-1152`) — a behaviour the doc and Cluster N both miss:

```js
if (!o.ranges.some(e => !e.empty)) return !1;                       // needs a non-empty selection
if (!o.ranges.every(e => e.empty || r.lineAt(e.from).number === r.lineAt(e.to).number)) return !1;   // single-line only
if (!t.contains("\n") && Zv(t)) n = o.ranges.map(() => t);
else { var a = t.split("\n"); if (a.length !== o.ranges.length || !a.every(Zv)) return !1; n = a; }
… insert: e.empty ? n[t] : "[".concat(r.sliceString(e.from, e.to), "](").concat(n[t], ")")
… userEvent: "input.paste"
```

So: pasting a URL over selected text inserts a **markdown link**, one per selection range, requires each non-empty range to be single-line, and accepts a newline-separated URL block **only if the line count equals the number of ranges**. `Zv(s) = !s.contains(" ") && $v(s)` (`app/own/0646-Zv.js:2`) — no spaces and `new URL`-parseable. Empty ranges receive the bare URL.

---

## 10. Table cells, scroll sync, teardown — NEW

- `editTableCell(table, cell)` (`:1681-1690`) creates a nested `UJ` component whose `cm` becomes `activeCM`; it runs the DOM surgery inside `cm.observer.ignore(...)`, calls `cell.lockDimensions()`, hides `cell.contentEl`, appends the wrapper, focuses the cell editor, destroys any previous cell editor, then `table.setActiveDragHandles(cell)` and `containerEl.addClass("has-focus")` on the **table** container. `destroyTableCell` is the inverse (`unsetActiveDragHandles`, remove `has-focus`, `lockDimensions`, detach, `contentEl.show()`, `rerenderCell(cell)`, `removeChild`) and is called from `clear()`, `destroy()` **and on every editor `focus`** (`:1114-1116`) — so clicking out of a cell always tears it down.
- `imageResizeMaxWidth` is `cm.contentDOM.offsetWidth` (`:1675-1680`).
- **Scroll sync** (`getScroll`/`applyScroll`, `:1731-1780`) uses a fractional coordinate `line + fraction`; `applyScroll` clamps to `[0, doc.lines - 0.99]`, sets `isScrolling = !0` before writing `scrollDOM.scrollTop`, and `onScroll` (`1719-1720`) skips `syncScroll()` on the echo pass. `focus()` saves `getScrollInfo()`, focuses, then restores `scrollTo(left, top)` — **focusing never changes scroll position** (`:1727-1730`).
- `destroy()` (`:1041-1042`) order: null `localExtensions`, `cleanup()`, `cm.setState(EditorState.create({}))`, `cm.destroy()`, `editorSuggest.close()`, pop the keymap scope, mobile toolbar update, `destroyTableCell()`. Emptying the state before `destroy()` is the "retire on destroy" measure the 2026-06-10 dogfood log describes.
- The view constructor (`1706-1717`) **detaches `cm.contentDOM`**, rebuilds a `cm-sizer > cm-contentContainer` wrapper, and **moves the gutters element into the content container** — which is why the viewport context-menu test is `sizerEl.contains(target) && !target.parentElement.closest(".cm-gutters")`.
- `reparent(el)` re-parents the whole editor and re-inits (`:1030-1031`), used when the view is moved between windows.

---

## 11. CONFIRMED (unchanged in 1.13.7)

| Claim in `editor.md` | 1.13.7 evidence |
|---|---|
| `editorViewField === editorInfoField`, same instance | `0025-expressionstatement.js:113,115`; `1819-yJ.js:69` (`SJ = MJ`) |
| `editorLivePreviewField` initial `false`, never `null` | `1819-yJ.js:70` (`xJ = CJ(!1)`) |
| Field update is by field **reference** identity | `1819-yJ.js:57-63` (`o.is(kJ) && o.value.field === t`) |
| `q$`-equivalent is one `StateEffect.define()` created at module init | `1819-yJ.js:51` (`kJ = Ue.define()`) |
| `Y$`-equivalent `Compartment` reconfigured from `Workspace.editorExtensions` | `1823-RJ.js:3`, `944`, `1196`, `1199-1205` |
| `lP` autocorrect table, exactly three patterns, left-to-right first-hit-wins | `1263-bO.js:4-19`, `314-322` |
| the third `lP` pattern is dead code | pattern 1 (`:4-9`) already matches what pattern 3 (`:14-18`) matches, and matching is first-hit-wins |
| `expandText` runs from a **10 ms** debounced update listener, not `inputHandler` (2026-06-10 §1) | `1823-RJ.js:1049-1051`, `1065` |
| gate is `userEvent !== "set"` | `1226-TL.js:2` used at `1823-RJ.js:1055` |
| suggester trigger on `focusChanged || docChanged || selectionSet`, **50 ms** | `1823-RJ.js:1046-1048`, `1056` |
| "did the user type/delete" = `isUserEvent("input") \|\| isUserEvent("delete")` | `1227-DL.js:2` used at `1823-RJ.js:1055` |
| `insertText` appends at **end of document** then sets the cursor there | `1263-bO.js:35-38` |
| `getDoc()` returns `this`; `setLine` = `replaceRange(text, mO(n,0), mO(n, len))` | `1263-bO.js:22-25` |
| `somethingSelected() === !!getSelection()` | `1263-bO.js:26-27` |
| `setCursor(line, ch)` vs `setCursor(pos)` dispatch | `1263-bO.js:28-32` |
| `posAtMouse` = `posAtCoords(clientX, clientY)` | `1263-bO.js:33-34` |
| `processLines` dedupes via `Array.from(new Set(...))`, sorts numerically (`a - b`), dispatches **one** transaction | `1263-bO.js:39-76`; `0555-Ud.js:2`; `Tw` at `app/own/0819-ww.js:237`, body `return e - t;` |
| `processLines` blank-line skip only when >1 distinct line | `1263-bO.js:48` |
| anchor-nudge: `ch === 0 && collapsed && anchor.line === change.from.line`, shift by `textLength - rangeWidth`, clamped at 0 | `1263-bO.js:59-71` |
| `setHeading` honours the blockquote prefix via `/^([>\s]*)(#{1,6} )?(.*)/` | `1263-bO.js:3` (`wO`), `:77-87` |
| `indentMore`/`indentLess` `userEvent` = `"input.indent"` / `"delete.dedent"` | `1421-BH.js:39`, `:56` |
| both refuse when `state.readOnly` | `1421-BH.js:32`, `45` |
| `EditorSuggest.trigger` returns `false` for a non-collapsed selection | `1324-VR.js:15` |
| async `getSuggestions` re-checks `editor.hasFocus()` and closes silently | `1324-VR.js:37` |
| `limit` default 100; `<= 0` means no cap | `1324-VR.js:5`, `48-49` |
| manager: flat array, insertion order, **first truthy `trigger` stops the loop** | `1925-P6.js:145-148`, `:151-158` |
| `Plugin.registerEditorSuggest` removes on unload | `1938-c4.js:127-132` |
| triple-click line-select is `EditorView.mouseSelectionStyle.of(...)`, triple left-button only | `1819-yJ.js:71-90` (`3 != t.detail \|\| 0 != t.button`); installed at `1822-expressionstatement.js:10` |

---

## 12. Unverifiable / out of scope

- `rsv.push`, `Ns`, `qh`, `JO`, `Vl`, `Ll`, `Hf`, `zp`'s consumers in the live-preview decoration engine (`G3(this, cm)` at `1823-RJ.js:1180`) — the live-preview ViewPlugin itself was not read; the 2026-06-10 addendum §3's "unaudited engine" caveat still stands for decoration/widget internals. Everything cited here is the *wrapper*, the *keymap*, the *listener*, or the *cache/fold/history contract*.
- The link suggester's `suggestManager` (`GR`, `app/own/1332-GR.js:328+`) internals — query construction, heading enumeration, and the ranking/render helpers behind `ub`/`zR`/`qR`/`_R` — were located but not read, so no claim is made about suggester ordering or scoring.
- `BF + GT(WT(i))`'s inner markdown→HTML chain (`WT`, `GT`) was not read; only the `<!-- obsidian -->` prefix and the 1e6 length gate are claimed.
- `tW` (the lazy promise awaited by the `CodeMirror.getMode` monkey-patch) is unidentified.
