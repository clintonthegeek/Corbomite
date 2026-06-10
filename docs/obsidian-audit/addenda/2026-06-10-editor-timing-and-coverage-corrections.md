# Audit addendum — editor corrections (`expandText` trigger timing, `lP` redundancy mix-up, unaudited editor regions)

**Corrects:** `domains/editor.md` §1 (autocorrect table prose), §12 #5, §13 Q1, §14 #1. Also records coverage gaps editor.md implies are out-of-domain but does not flag as **unaudited**.

**Date:** 2026-06-10
**Discovered during:** verification pass of audit claims against the decompiled source.
**Source:** decompiled Obsidian 1.12.7 corpus at `/home/clinton/bin/ObsidianRAW/audit/` (paths relative to `renamed/obsidian/`). All claims re-checked 2026-06-10.

## 1. `expandText()` runs from a 10 ms-debounced CM `updateListener`, not from `inputHandler`

**Wrong claim:** editor.md §1 — "CM6's `inputHandler` on `Editor` subclass calls `expandText()` after the IME commit completes"; §12 #5 — "`expandText()` runs them after every text-producing input."

**Verified reality:** the trigger lives in the markdown edit-view class (`tZ` symbol) in `_internal.js`, registered as a plain **update listener** — `EditorView.updateListener.of(this.updateEvent())` (`src/_internal.js:560669`). `updateEvent` (`src/_internal.js:560637-560654`):

```js
// src/_internal.js:560640-560645
n = debounce(function () {
  (e.editorSuggest.trigger(e.editor, e.file, t), (t = !1));
}, 50),
i = debounce(function () {
  e.editor.expandText();
}, 10),
```

The returned listener computes `s = o.docChanged && a.some(PA)` — `PA` is "userEvent !== 'set'" (`src/_internal.js:240377-240379`), i.e. any non-programmatic transaction — and calls the 10 ms-debounced `expandText` when `s` is true. `editorSuggest.trigger` fires from the **same listener** with a **50 ms** debounce, on `focusChanged || docChanged || selectionSet`, with its "did the user type/delete" flag set via `LA` (`isUserEvent("input")||isUserEvent("delete")`, `src/_internal.js:240383-240385`).

**Implementation impact:** timing matters if Markoff mirrors suggester behaviour — autocorrect is *post-commit and asynchronous* (fires ≥10 ms after the document change, on programmatic edits too as long as the userEvent isn't `"set"`), not an input-stream interception. A Markoff implementation hooking the input pipeline (e.g. `inputMethodEvent`) would fire earlier than Obsidian and at different times relative to suggester triggering (10 ms vs 50 ms means autocorrect can rewrite text *before* the suggester evaluates it).

## 2. The third `lP` pattern is redundant with the FIRST pattern, and the `？` sentence is garbled

**Wrong claim:** editor.md §1 — "The third pattern is functionally redundant with the **second's** no-`！` branch but the runtime keeps both — the second pattern only matches when the literal regex captures `？` so the redundancy is defensive."

**Verified reality:** the `lP` table (`tree/obsidian/editor/Editor.js:7-26`) is:

1. `/(！)?【【$/` → `![[` if `！` captured, else `[[`
2. `/】】$/` → `]]`
3. `/【【$/` → `[[`

The second pattern is the **closing**-bracket rule (`】】` → `]]`) and has no `！` branch at all. Pattern 3 is redundant with pattern **1**'s no-`！` branch (`【【` → `[[`), and since matching is first-hit-wins, pattern 3 is dead code in practice. Neither regex contains `？` anywhere — the audit sentence about "capturing `？`" matches nothing in the source and should be disregarded entirely. (editor.md §13 Q1 already asks whether the third entry is dead code; the answer per the source is "yes, unless pattern order is changed".)

## 3. Coverage-gap notice: the heavy editor machinery is UNAUDITED, and editor.md §14 mislocates it

editor.md §14 #1 says the hard parts (live-preview decorations, folding, etc.) "are not in this domain; they're in `editor/markdown` and `vendor/codemirror`" — implying they were or would be covered by the extracted `editor/markdown` tree files. **Wrong location, and never audited.** The extracted `tree/obsidian/editor/markdown/` directory contains only the preview/render classes (`MarkdownPreviewView`, `MarkdownRenderer`, `MarkdownView`, …). The actual editing engine lives in the **unextracted `_internal.js` region** around the markdown edit-view class (`tZ`, original bundle lines 146511-149544) and its supporting view-plugins:

- **Live-preview decoration/widget engine** and `cm-embed-block` widgets — 55 `cm-embed-block` hits in `src/_internal.js`, none in the extracted tree.
- **In-editor table editor** — `editTableCell`/`destroyTableCell` call sites and the `cm-embed-block cm-table-widget markdown-rendered` widget at `src/_internal.js:345870-346231`; methods on the edit-view class at `src/_internal.js:561875/561899`.
- **Vim mode** (`vimMode` config plumbing, `src/_internal.js:104196` etc.), **editor drag-and-drop** (~190 `addEventListener("drop", …)`-style handler hits in `src/_internal.js`), and the **find/replace widget** (`lz`, `src/_internal.js:339088` region, original bundle lines 110776-111869).

None of this is covered by any Pass 2 domain doc. Readers of editor.md should **not** assume the editor domain is audited beyond the thin `Editor` wrapper + `EditorSuggest` + the four `editor*Field`s; anything decoration-, widget-, table-, vim-, DnD-, or find/replace-shaped requires fresh source extraction before implementation can claim Obsidian parity.

**Implementation impact:** plans that cite editor.md as evidence of "bounded port cost" (§14 #1's framing) are sized against the wrapper only. The engine work is unsized and unspecified — treat it as such in cluster estimates.
