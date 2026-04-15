# `obsidian/editor` — editor wrapper, suggesters, CodeMirror state-fields

**Source:** `/home/clinton/bin/ObsidianRAW/audit/renamed/obsidian/tree/obsidian/editor/`
**File count:** 6
**Files:** `Editor.js`, `EditorSuggest.js`, `editorEditorField.js`, `editorInfoField.js`, `editorLivePreviewField.js`, `editorViewField.js`
**Pass 1 summary (verbatim from `00-taxonomy.md`):**
> Wraps the underlying CodeMirror 6 `EditorView` in an Obsidian-flavoured `Editor` API (cursor/selection getters, `replaceRange`, `posAtMouse`, list-aware `getLine` helpers, IME handling). The four `editor*Field` exports are CodeMirror `StateField`s plugins use to read the live `Editor` / `EditorInfo` / `EditorView` / live-preview-state from inside CodeMirror extensions. `EditorSuggest` is the plugin-extensible inline autocomplete (slash-menu, `[[wikilink]]`, `@mention` patterns).

**De-minifier artifact note:** Four of the six files (`editorEditorField.js`, `editorInfoField.js`, `editorViewField.js`, `editorLivePreviewField.js`) are **byte-identical extracts** of `app.js` `lines 146160-146285`. MD5 differs only in the leading `// public API symbol:` comment; the body is the same 130-line block defining all four StateFields plus helpers (`H$`, `z$` — indent/dedent commands; `K$` — triple-click line-selection mouseStyle; `Y$` — the `Compartment` for plugin `editorExtensions`). Real primitives are `j$(initial)` (StateField factory) and `q$` (`StateEffect.define()` for set-field). Only one of these four files needs auditing; this doc treats them as the "editor StateField bundle." `Editor.js` (`lines 92107-92619`) and `EditorSuggest.js` (`lines 99740-99836`) are non-duplicate single-class files.

---

## 1. Public API surface

Five exported symbols. `Editor` is the heaviest (≈30 methods); the four `editor*Field` symbols share a backing factory; `EditorSuggest` is a `PopoverSuggest` subclass with one composite `trigger` orchestration method. Methods are grouped by role.

### `Editor`

- **Kind:** class (IIFE returning constructor `e()`; aliased to `Editor`).
- **Exported as:** `Editor`.
- **Signature:** `new Editor()` — empty constructor body. All state lives on the *concrete* CM-derived subclass; this class only contributes prototype methods. Plugin code never instantiates directly — instances come from `MarkdownView.editor` / `editorEditorField`.
- **Purpose:** Documented "Obsidian dialect" of cursor/selection/document operations layered on top of an unstable `EditorView`.
- **Lifecycle:** owned by `MarkdownView.editor` (one per source-mode); created when the view is first shown, destroyed when the view unloads. Persists across `setViewData` calls (CM doc is replaced; wrapper isn't).
- **Mixes in:** neither. Concrete subclass adds `cm: EditorView` plumbing.
- **Method roles:** doc/line accessors, cursor & selection, coordinate mapping, IME / autocorrect, list-aware editing helpers, transactions, command-exec.

#### Doc / line accessors (subclass-supplied)

These are **referenced but not defined** in `Editor.js` — they're inherited from the concrete CM subclass that wires `cm: EditorView` into the prototype chain (subclass body lives outside this domain). The contract every plugin assumes:

- `getValue() / setValue(string)` — full-document text.
- `getLine(n: number) -> string`, `lineCount() -> number`, `lastLine() -> number`.
- `getRange(from, to) -> string`, `replaceRange(text, from, to?)`, `replaceSelection(text)`.
- `getCursor(side?: "from"|"to"|"head"|"anchor") -> EditorPosition`.
- `getSelection() -> string`, `listSelections() -> EditorSelection[]`.
- `setSelection(range: EditorPosition | EditorSelection)`.
- `transaction(spec, userEvent?)` — see "Transactions" below.
- `coordsAtPos(pos) -> {top, bottom, left, right} | null`, `posAtCoords(x, y) -> EditorPosition | null`.
- `wordAt(pos) -> {from, to} | null`, `hasFocus() -> boolean`.
- `exec(commandName: string)` — invokes a CM6 command by string id (used for `"indentMore"`/`"indentLess"`/`"newlineAndIndent"`).

This domain *defines the contract*; CM provides the implementation.

#### Editor-domain-defined methods (declared in `Editor.js`)

- `getDoc() -> this` (`Editor.js:30`). CM5 compat shim — `Doc` and `Editor` are conflated in the modern API.
- `setLine(n, text)` (`Editor.js:33`). `replaceRange(text, iP(n, 0), iP(n, getLine(n).length))`.
- `somethingSelected() -> boolean` (`Editor.js:36`). True iff `getSelection() !== ""`.
- `setCursor(line, ch?)` (`Editor.js:39`). Two-arg `(line, ch)` or one-arg `(EditorPosition | EditorSelection)`; always becomes `setSelection(...)`.
- `posAtMouse(MouseEvent) -> EditorPosition` (`Editor.js:47`). Wrapper: `posAtCoords(e.clientX, e.clientY)`.
- `insertText(text)` (`Editor.js:50`). **Append at end of document, then move cursor to new end.** IME composition end / paste fast-path. NOT "insert at cursor" (that's `replaceSelection` / `replaceRange`).

#### List-aware / block-level helpers

These are the live-reference set of "editor commands" that the command palette wires to. All take **no args** unless noted, operate on the current selection set (multi-cursor-aware), and dispatch as a *single* `transaction` so undo treats the change atomically.

- `processLines(matcher, mutator, skipBlankLines = true)` (`Editor.js:57`). Internal helper used by every list/heading toggle. Iterates lines covered by every `listSelection`, dedupes/sorts (`ac(...).sort(pb)`), runs `matcher(lineNo, text)` per line, then `mutator(lineNo, text, matcherResult)` to produce a `Change` object (or `null`). Single-line single-cursor "anchor at column 0" gets a special-case selection nudge (cursor stays at the same absolute character). Otherwise dispatches all changes in one `transaction({changes})`. `skipBlankLines = true` skips empty lines when there are multiple selections.
- `setHeading(level)` (`Editor.js:117`). Replaces leading `#…# ` of each selected line with `level` × `#`+space (or empty for `0`). Uses `sP = /^([>\s]*)(#{1,6} )?(.*)/`. Honours blockquote prefix.
- `toggleBlockquote()` (`Editor.js:132`). Adds `> ` to lines that have zero blockquote depth; otherwise un-quotes by one level. Asymmetric: the *first matched line* sets the mode (`add` vs `remove`) for the whole batch.
- `toggleBulletList()` / `toggleNumberList()` (`Editor.js:157/175`). Toggle on if any selected line is missing a list marker, off otherwise. Uses `aP = /^([>\s]*)(([*+-] |(\d+)([.)] ))(?:\[(.)\] )?)?/`.
- `toggleCheckList(check: boolean)` (`Editor.js:193`). Three-state machine on counter `t` initialised at 3: `t>2 && marker===" "` → `t=2` (some are `[ ]`, going to `[x]`); `t>1 && !marker` → `t=1` (no checkbox; will add `[ ]`); `check && t>0 && !listMarker` → `t=0` (must add list AND checkbox). Per-line emit list marker + `[ ] `/`[x] ` per state. The `check` arg is the "force checked" flag (command `Toggle checkbox status` passes `false`; `Toggle done` passes `true`).
- `insertCallout()` (`Editor.js:228`). On a clean cursor line: inserts `> [!NOTE]\n> ` block (with blank-line padding) and selects literal `NOTE` so the user types the type. With non-empty prefix: inserts `\n> [!NOTE] Title\n> Contents\n` after cursor and selects `Title`.
- `insertCodeblock()` / `insertMathBlock()` (`Editor.js:275/278`). Wrap selection in `` ``` `` / `$$` fences (delegate to `insertBlock` — defined on the CM subclass).
- `indentList()` / `unindentList()` (`Editor.js:281`). `exec("indentMore"/"indentLess")` — pure delegation.
- `newlineAndIndentContinueMarkdownList()` (`Editor.js:287`). The "smart-Enter" inside a list / blockquote. ≈150-line per-cursor state machine: rejects multi-char selections (falls back to `exec("newlineAndIndent")`); matches leading list/quote prefix; if cursor at-or-before marker column, default newline; if line is empty after marker, "end the list" (strip indent / strip one `>` from current and the line above / strip the marker); otherwise insert `\n` + prefix, bullet `[ ] ` if previous had it, auto-increment numbers, and if the *next* line starts with the same marker, blank our inserted marker to avoid double-bullet.
- `newlineAndIndentOnly()` (`Editor.js:446`). Hanging-indent variant: aligns the new line under the previous line's first non-whitespace character.
- `newlineOnly()` (`Editor.js:491`). Just `transaction({replaceSelection: "\n"})` — the "no-formatting" newline.
- `expandText()` (`Editor.js:496`). The IME / full-width-bracket autocorrect dispatcher — see "Autocorrect table" below.

#### Autocorrect table (full-width brackets → wikilink syntax)

The `lP` array (`Editor.js:7`) is the **full set** of three patterns matched against text immediately preceding the cursor. Match is left-to-right; first hit wins:

| Pattern (regex) | Match example | Replaces with |
|---|---|---|
| `/(！)?【【$/` | `！【【` (full-width `!【【`) | `![[` (image embed) |
| `/(！)?【【$/` | `【【` (no preceding `！`) | `[[` (wikilink) |
| `/】】$/` | `】】` | `]]` |
| `/【【$/` | `【【` (fallback identical to first sans-`！` branch) | `[[` |

`expandText()` runs the regex set against `getLine(cursor.line).substr(0, cursor.ch)` whenever called — typically bound to *every* keypress that produces text (CM6's `inputHandler` on `Editor` subclass calls `expandText()` after the IME commit completes). The third pattern is functionally redundant with the second's no-`！` branch but the runtime keeps both — the second pattern only matches when the literal regex captures `？` so the redundancy is defensive.

**Markoff implication:** This is the entire CJK convenience surface. Three patterns is small but load-bearing — Chinese/Japanese users typing wikilinks expect `【【` to expand. See Section 12.

#### Coordinate mapping

`posAtMouse(MouseEvent)` is the only domain-internal one. The CM-subclass `posAtCoords(x, y)` and `coordsAtPos(pos)` are referenced by `EditorSuggest.updatePosition` (`EditorSuggest.js:80–82`) which uses them to pin the suggestion popup to the trigger's screen rect.

#### Transactions

A `transaction(spec, userEvent?)` call in this code style maps to a CM6 `state.update({changes, selection, userEvent})` followed by `dispatch`. The `userEvent` string is observed in the source: `"input.type"` (newline-and-indent), `"input.indent"` (`H$` indent command), `"delete.dedent"` (`z$` dedent command). Plugins listening on `EditorView.updateListener` filter on these prefixes to distinguish user typing from programmatic edits.

### `EditorSuggest<T>`

- **Kind:** class — `EditorSuggest extends PopoverSuggest`.
- **Exported as:** `EditorSuggest` (`// public API symbol: EditorSuggest`).
- **Signature:** `new EditorSuggest(app)` (passed to `super(app)`). Subclasses override four methods; see "Subclass contract" below.
- **Purpose:** The base class for inline, in-editor autocompleters. Built-ins (`_I`, `YI`, `d0` — file-link, header-link, tag suggesters) inherit from this; plugins inherit too.
- **Lifecycle:** registered via `Plugin.registerEditorSuggest(suggest)` (`plugin/Plugin.js:214`). On `Plugin.unload` the suggest is removed via `workspace.editorSuggest.removeSuggest(suggest)`. The class is `PopoverSuggest`-derived, so it *inherits a `Component`-managed `unload` sequence* — DOM children, registered events, registered intervals all get torn down on unload. Active suggesters go through the central `EditorSuggestManager` (`f0` in `views/ViewRegistry.js:238`).

#### Subclass contract (four overridable methods)

1. **`onTrigger(cursor, editor, file) -> EditorSuggestTriggerInfo | null`** — "should I appear?" check. `null` skips; `{start, end, query}` commits.
2. **`getSuggestions(context) -> T[] | Promise<T[]>`** — up to `this.limit` (default 100) candidates. Async supported (`EditorSuggest.js:46–61`); on resolution the call re-checks `editor.hasFocus()` and closes the popup if focus moved.
3. **`renderSuggestion(value, el)`** — plugin draws into the row `el`.
4. **`selectSuggestion(value, evt)`** — user clicked/Entered. Plugin must call `editor.replaceRange(...)` itself; base class does NOT auto-replace anything from the trigger range.

#### Lifecycle of a single trigger cycle (`EditorSuggest.prototype.trigger`)

`trigger(editor, file, isOpen)` (`EditorSuggest.js:26`) runs from a CM `updateListener` on every selection change:

1. If the selection is non-empty (`from !== to`), set `context = null` and return `false` (no popup over a selection).
2. Call `onTrigger(from, editor, file)`. If `null`, set `context = null` and return `false`.
3. Build `context = {editor, file, start, end, query}`.
4. **If `isOpen` is true OR this suggester was already open**, fetch suggestions:
   - Sync `T[]` → `showSuggestions(arr)` immediately.
   - Async `Promise<T[]>` → await; if `editor.hasFocus()`, show; else `close()`.
5. Return `true` (signals to the manager that this suggester is the "active" one and others should be skipped).

#### Multiple-suggester coordination

The manager (`f0` / `EditorSuggestManager`, `views/ViewRegistry.js:238–281`) holds a flat array `suggests: EditorSuggest[]` (built-ins added in registration order; plugin suggesters appended via `addSuggest`). On every cursor change:

```
for s in suggests:
    if s.trigger(editor, file, isOpen): setCurrentSuggest(s); return
close()
```

**First non-null `onTrigger` wins.** There is no priority system; insertion order is the entire mechanism. Built-ins (`_I = file-link` for `[[`, `YI = headers-of-current-file` for `#` after `[[`, `d0 = tag` for `#`) are registered first in the manager constructor, so a plugin that wants to *override* `[[` autocompletion has to either (a) register first (impossible from a community plugin) or (b) accept being shadowed.

#### Other methods + properties

- `setInstructions(items)` (`EditorSuggest.js:20`) — builds the popup footer ("`↑↓` to navigate, `↵` to insert"); empty array detaches it.
- `showSuggestions(arr)` (`EditorSuggest.js:68`) — truncates to `this.limit`, sets and positions.
- `updatePosition(open)` (`EditorSuggest.js:76`) — recomputes rect from `coordsAtPos(start)`/`coordsAtPos(end)`; `reposition(rect, dir)` where `dir = LI(getLine(start.line))` (bidi-aware).
- `close()` (`EditorSuggest.js:96`) — drops `context`, calls `PopoverSuggest.prototype.close`.
- Properties: `context: EditorSuggestContext | null`, `limit = 100`, `instructionsEl`, plus inherited `suggestEl` / `app` / `scope` (plugins can `scope.register(mod, key, cb)` for custom keybindings).

### `editorEditorField`

- **Kind:** constant — `StateField<Editor | null>` (initial `null`).
- **Stored type:** the `Editor` wrapper for the `EditorView` this state belongs to.
- **Use:** plugin `ViewPlugin`s/`StateField`s call `view.state.field(editorEditorField)` to get back the Obsidian `Editor` they're embedded in.
- **Population:** `MarkdownView` source-mode bootstrap dispatches `q$.of({field: editorEditorField, value: editor})` once after constructing the view.

### `editorInfoField`

- **Kind:** constant — `StateField<MarkdownView | null>` (initial `null`).
- **Stored type:** the surrounding `MarkdownView` (or `MarkdownEditView`-shaped object exposing `file`, `app`, `getMode()`).
- **Use:** lets a CM extension reach back for `file` (`TFile`), `app`, `leaf` — plugins read `file.path` from inside a ViewPlugin without globals.

### `editorViewField`

- **Alias of `editorInfoField`** (`editorInfoField.js:105`: `editorViewField = editorInfoField`). Two public names, **same field instance**. Reading either via `state.field(...)` returns the same value. Historical migration artifact; new code should prefer `editorInfoField`.

### `editorLivePreviewField`

- **Kind:** constant — `StateField<boolean>` (initial `false`).
- **Stored type:** boolean. `true` = live-preview; `false` = source mode.
- **Use:** the authoritative live-preview flag for any CM extension. ViewPlugins gate decoration logic on this. Embed/inline-decoration extensions read this to decide whether to draw replacement widgets.
- **Population:** dispatched via `q$.of(...)` whenever the user toggles per-view OR the `livePreview` vault-config setting changes. Vault config sets the *default*; per-view toggle sets *this* field; reading-mode is a separate `MarkdownPreviewView`, not a CM state at all.

---

## 2. Data structures

### `EditorPosition`

```typescript
{
  line: number;  // 0-based line index
  ch: number;    // 0-based character offset within the line
}
```

Constructed everywhere in `Editor.js` as `iP(line, ch)` — a small global factory in core/utils. Equivalent to a `{line, ch}` object literal. **Never** uses CM6's flat `number` offsets in the public API; the conversion lives in the CM-subclass methods.

### `EditorSelection`

```typescript
{
  anchor: EditorPosition;  // where selection started
  head: EditorPosition;    // where the cursor is (drag end)
}
```

When `anchor.line === head.line && anchor.ch === head.ch`, the selection is a "cursor." `listSelections()` returns an array of these (multi-cursor support).

### `EditorSuggestTriggerInfo`

```typescript
{
  start: EditorPosition;  // first char of the matched trigger
  end: EditorPosition;    // last char (typically the cursor)
  query: string;          // the substring the popup will match against
}
```

Returned from `EditorSuggest.onTrigger`. The `query` is independent of the `[start,end)` range — a suggester can match `[[Foo` and report `query = "Foo"` while range covers the whole `[[Foo`.

### `EditorSuggestContext`

```typescript
{
  editor: Editor;
  file: TFile | null;
  start: EditorPosition;
  end: EditorPosition;
  query: string;
}
```

Built by `EditorSuggest.trigger` and passed to `getSuggestions(context)`. Persists on `this.context` for the lifetime of one popup-open cycle.

### `Change` (transaction input)

```typescript
{
  from: EditorPosition;
  to?: EditorPosition;     // optional; if omitted, an insert
  text: string;            // replacement text
}
```

The change-objects passed to `transaction({changes: Change[]})` and pushed onto the `n[]` accumulator inside `processLines`. The minimal shape; for a multi-cursor batch the platform validates non-overlap before dispatching.

### `editorStateField` storage shape (factory `j$`)

```typescript
StateField<T> {
  initialValue: T;
  // updates only when receiving a q$.of({field, value}) effect that
  // matches THIS field by reference identity:
  update(value: T, transaction): T
}
```

The factory `j$(initial)` (`editorInfoField.js:88–102`) creates a field whose `update` walks `transaction.effects`, checks `effect.is(q$) && effect.value.field === this`, and replaces the value when matched. **Field identity is by reference** — the StateEffect must carry a pointer to the *same field instance* the caller is reading. This is why `editorViewField = editorInfoField` (they share an instance) reads the same value: a single dispatch updates both names.

---

## 3. On-disk contracts

`No on-disk contracts.`

This domain is purely in-memory CM-state and DOM. The relevant *config* setting `livePreview: boolean` lives in `.obsidian/app.json` and is owned by the `vault` domain (read here via `app.vault.getConfig("livePreview")`); see Wave 1 `domains/vault.md`. The `defaultViewMode: "source" | "preview" | "live"` setting (read at `editor/markdown/MarkdownView.js:1628`) is also vault-config.

---

## 4. Events emitted

`No events emitted.` (Neither `Editor` nor `EditorSuggest` extends `Events`. Selection-change observation goes through CM6's `EditorView.updateListener.of(...)` extension surface; the Obsidian-level `editor-change` event is emitted by `MarkdownView`, not by this domain.)

---

## 5. Events consumed

| Listener file | Subscribes to | Why |
|---|---|---|
| `editor/EditorSuggest.js` | (indirect) CM6 `EditorView.updateListener` via `EditorSuggestManager` | invokes `trigger(editor, file, isOpen)` on every selection change so the popup can react to typing |

The manager (`EditorSuggestManager`, `views/ViewRegistry.js:238`) is the actual subscriber to CM updates; it forwards into each registered `EditorSuggest`. From this domain's view, the contract is "be ready to be called per-keystroke."

---

## 6. Commands registered

`No commands registered here.`

The list-aware methods (`toggleBulletList`, `setHeading(N)`, `insertCallout`, etc.) are *targets* of commands — the commands themselves (id `editor:toggle-bullet-list`, `editor:set-heading-1` … `editor:set-heading-6`, `editor:insert-callout`, `editor:toggle-checklist-status`, `editor:insert-codeblock`, `editor:insert-mathblock`, `editor:indent-list`, `editor:unindent-list`, `editor:newline-and-indent-continue`, `editor:expand-text`, etc.) are registered in `editor/markdown/MarkdownView.js`, not here. See `domains/editor-markdown.md` for the full table.

---

## 7. Registries owned

This domain owns no registry directly. The registry that holds `EditorSuggest` instances is `EditorSuggestManager.suggests` (`views/ViewRegistry.js:238–281`); see `domains/views.md` for that registry. The registry that holds plugin-supplied editor-extensions is `Workspace.editorExtensions: Extension[]` plus the `Y$` `Compartment` (`editorEditorField.js:130`) that holds them in CM state; see `domains/workspace.md` Section 7.

For completeness, the **two CM6 primitives this domain creates** that act as registry-shaped storage:

### `q$` — the "set state field" `StateEffect`

- **Stores:** ephemeral per-transaction `{field: StateField<T>, value: T}` payloads.
- **Used by:** any code that wants to push a new value into one of `editorEditorField` / `editorInfoField` / `editorLivePreviewField`. Dispatched as `view.dispatch({effects: q$.of({field, value})})`.
- **Persistence:** transient (lives for one transaction).
- **Lifecycle:** created once at module init (`StateEffect.define()` at `editorInfoField.js:87`).

### `Y$` — the editor-extensions `Compartment`

- **Stores:** the **flat list** of plugin-registered CM extensions, reconfigured into the active `EditorState`.
- **Used by:** `MarkdownView.updateOptions` (in `editor/markdown/MarkdownView.js`) → reconfigures `Y$` with `app.workspace.editorExtensions` whenever the registry changes.
- **Persistence:** in-memory only; rebuilt from `app.workspace.editorExtensions` on every `updateOptions()`.
- **Lifecycle:** singleton compartment, created once at module init (`editorInfoField.js:130`); the *contents* are swapped on every plugin (un)registration.

---

## 8. Invariants

- `editorViewField === editorInfoField` (literal reference equality). Any code testing one is implicitly testing both.
- `editorLivePreviewField`'s value is **never `null`** — the StateField initial value is `false`. Plugins reading it can trust the boolean type without null-checking.
- `editorEditorField` and `editorInfoField` may be `null` *during view boot* (between `EditorView` construction and the first `q$.of(...)` dispatch). Plugin extensions should null-check before deref.
- `Editor.somethingSelected() === !!Editor.getSelection()` — empty-string selection counts as no-selection. **Caveat:** a multi-cursor with all empty heads also reports `false`, even though `listSelections().length > 1`.
- `Editor.processLines` guarantees: changes are applied as a **single CM transaction**, so undo/redo treats the whole batch atomically.
- Single-cursor edge case in `processLines`: if the cursor's `ch === 0` AND the cursor anchor === head AND cursor's line is the same as the change's `from.line`, the cursor's `ch` is **shifted by `(textLength - rangeWidth)`** so the cursor stays at the same absolute character. In all other cases the cursor is left to CM's default mapping.
- `EditorSuggest.trigger` returns `false` (and clears `context`) for any non-collapsed selection — popups never open over an active selection.
- `EditorSuggest.onTrigger` returning a falsy value is a hard "skip me" — the manager moves to the next suggester. There is no "this suggester is interested but defers" signal.
- `EditorSuggest.getSuggestions` returning a `Promise` is supported, but if the editor loses focus before resolution, the popup is **closed silently** — `selectSuggestion` won't be called even if the promise eventually resolves.
- `EditorSuggest.limit` is a soft cap (`-1` or `0` → no cap). The default is `100`. Plugins overriding need to assign in the constructor before `super()` returns to take effect on the first trigger.
- `expandText()`'s autocorrect runs **left-to-right against the substring before the cursor**, on every keystroke that triggers it. The first regex match commits and the function returns; later patterns don't fire even if they would also match.
- `insertCallout()` always selects the literal text `NOTE` (or `Title` in the wrap-existing-text branch) so the user can immediately type the callout type.
- `Editor.getDoc()` returning `this` is a permanent compatibility stub; CM5-style `editor.getDoc().setLine(...)` and modern `editor.setLine(...)` produce identical effects.

---

## 9. Observable user features

- The user can **press Enter inside a list/blockquote** and the next line auto-continues the list marker (and increments numbers / preserves checkboxes / strips the marker on a blank trailing line to "end" the list).
- The user can **press Tab/Shift-Tab in a list** to indent/outdent the entire current item (delegates to CM `indentMore`/`indentLess`; the indent command is bidi-aware via the `H$`/`z$` pair, which respects `> ` blockquote prefixes when computing the indent column).
- The user can **type `【【` (full-width brackets, common on CJK IMEs) and have it auto-corrected to `[[`** to start a wikilink, `】】` → `]]` to close one, `！【【` → `![[` for an image embed.
- The user can **toggle bullet/numbered/check lists** on the current selection (and toggling is "asymmetric" — if any line is missing the marker, the whole batch gets it; otherwise the whole batch loses it).
- The user can **insert a callout** via `Ctrl+Shift+C` (default keymap; command id `editor:insert-callout`); the cursor lands inside the `[!NOTE]` so they can type the type.
- The user can **set heading level 0–6** on the current selection.
- The user can **paste text via IME** without the composition being lost (CM's IME path lands in `insertText` which performs the append-and-set-cursor at end-of-doc).
- The user can **trigger an inline autocomplete popup** by typing a sequence that matches a registered `EditorSuggest` trigger (the built-ins: `[[` for note links, `[[Note#` for headings, `#` for tags; plugins add more).
- The user can **navigate the autocomplete popup** with `↑↓` and commit with Enter or click; `Esc` closes it. The popup auto-positions at the trigger range; on RTL lines it flips to right-aligned.
- The user can **click and drag with triple-click** to extend the selection by whole lines (`K$ = EditorView.mouseSelectionStyle.of(...)` — `editorInfoField.js:107–129`). The first triple-click anchors a line range; drag extends one line at a time.
- The user can **switch between live-preview and source mode** per-view (the toggle dispatches `q$.of({field: editorLivePreviewField, value: true|false})`); plugin-added decorations re-render based on the new field value within a single CM update cycle.

---

## 10. Extension surfaces exposed

| Surface | Registration verb | Consumer call site | What plugins supply |
|---|---|---|---|
| Inline autocomplete | `Plugin.registerEditorSuggest(suggest)` (`plugin/Plugin.js:214`) | `views/ViewRegistry.js:254` (`EditorSuggestManager.trigger` iterates `suggests` per cursor change) | An `EditorSuggest<T>` subclass that implements `onTrigger`, `getSuggestions`, `renderSuggestion`, `selectSuggestion` |
| CodeMirror extension | `Plugin.registerEditorExtension(ext)` (`plugin/Plugin.js:197`) → `Workspace.registerEditorExtension` (`workspace/Workspace.js:3889`) → pushed onto `Workspace.editorExtensions[]`, then `updateOptions()` reconfigures the `Y$` Compartment in every `MarkdownView.cm` (`editor/markdown/MarkdownView.js:1521`) | Any CM6 `Extension`: `StateField`, `ViewPlugin`, `Decoration` set, `Facet` value, keymap, `transactionFilter`, `inputHandler`, etc. The extension can read live state via `state.field(editorEditorField)` / `editorInfoField` / `editorLivePreviewField`. |
| Read-back of the wrapping `Editor` | `state.field(editorEditorField)` | n/a (it's a CM6 field-read) | nothing — it's the *consumer* surface for the registered extension above |
| Read-back of the surrounding `MarkdownView` | `state.field(editorInfoField)` (or alias `editorViewField`) | n/a | nothing — same |
| Read-back of live-preview state | `state.field(editorLivePreviewField)` | n/a | nothing — same |

The four `editor*Field` exports are *not* registration verbs — they're **handles** plugin-supplied extensions use to peer back at the host. They *are* part of the public plugin surface (a plugin extension that doesn't import these has no way to tell which note it's looking at), so they belong here.

---

## 11. Corbomite mapping

| Obsidian concept | Corbomite equivalent | Status | Notes |
|---|---|---|---|
| `Editor` (the wrapper class) | `Markoff::Editor` (`libs/markoff/include/markoff/Editor.h`) | Partial | Markoff exposes `setPlainText/toPlainText`, `cursorLine/cursorColumn`, `findText/replaceText`, and formatting slots (`toggleBold`, `insertCallout`, etc.). Missing: `EditorPosition`/`EditorSelection` shape and per-line `getLine`/`replaceRange`/`getRange`/`listSelections`/`processLines` family that every plugin assumes. Surface is "command-flavoured" (Qt slots) vs Obsidian's "doc-API-flavoured" (CM-style positions). A plugin shim must translate. |
| `EditorPosition` / `EditorSelection` (multi-cursor) | (none) | Missing | `cursorPositionChanged` signals `(int line, int column)` but no first-class struct; QTextDocument supports a single cursor only. Plugin features iterating `listSelections()` have no equivalent. |
| `Editor.processLines` (atomic multi-line edit) | (none) | Missing | List/heading toggles rely on this. Markoff slots mostly operate on the current block; no public "submit a batch of changes as one transaction" entry. |
| `Editor.newlineAndIndentContinueMarkdownList()` | Implicit in `Markoff::Editor::keyPressEvent` | Unverified | Confirm Enter-inside-`- foo` continues with `- ` and ends-on-blank. The plan in `docs/superpowers/specs/2026-04-03-markoff-migration-design.md` may cover it. |
| Full-width-bracket autocorrect (`【【`→`[[`) | (none) | Missing | Three regexes via `inputMethodEvent` after composition end. |
| `Editor.exec("indentMore"/...)` | `Markoff::Editor::keyPressEvent` | Partial | Tab/Shift-Tab handling is internal; no public command-by-name surface. |
| `EditorSuggest<T>` base class | `Markoff::CompletionPopup` (`src/editor/CompletionPopup.*`) | Partial | Popup exists, driven by `wikiLinkTrigger`/`tagTrigger` signals from `Markoff::Editor`. Missing: plugin-extensible register-suggester surface; trigger detection is hardcoded in `Markoff::Editor::detectCompletionTriggers`. To match Obsidian: expose `MarkoffSuggestRegistry` (insertion-ordered) with per-entry `bool tryTrigger(cursor, doc, file) -> std::optional<TriggerInfo>`. |
| `editorEditorField` / `editorInfoField` / `editorViewField` | (none) | Missing | Plugin extensions need to ask "which editor / note am I in?" from inside their hook. Recommend a per-`Markoff::Editor` opaque handle passed into the extension callback. |
| `editorLivePreviewField` (live-preview boolean) | `Markoff::Editor::setReadOnly` (closest mode-flag today) | Missing | Three-mode pivot is the QGraphicsView migration goal. The boolean is the cleanest plugin-observable contract. |
| `Editor.transaction({changes, selection, userEvent})` | Per-slot undo grouping in `Markoff::Editor` (`insertAtCursor`, `wrapSelection` private) | Partial | No public batch-as-one-transaction API. Plugin equivalence requires exposing this. |
| `editor-change` event (emitted by `MarkdownView`) | `Markoff::Editor::textChanged` | Have | Corbomite signal lacks the richer payload (CM transactions list). |
| `coordsAtPos` / `posAtMouse` | `Markoff::Editor::cursorScreenRect` (one-direction); no public `posAtMouse` | Partial | Needed for popup positioning. |

**Corbomite-side action items implied by this mapping:**

1. Add public `Markoff::EditorPosition { int line; int ch; }` + `EditorSelection { EditorPosition anchor, head; }` to `libs/markoff/include/markoff/Editor.h`.
2. Promote private `wrapSelection`/`insertAtCursor` to public slots that take explicit `EditorPosition` ranges.
3. Add a `Markoff::SuggestRegistry` with insertion-order semantics.
4. Add an IME `inputMethodEvent` hook in `Markoff::Editor` running the three full-width-bracket regexes on composition end.
5. When the live-preview spec lands, expose `Markoff::Editor::isLivePreview() const` + signal to mirror `state.field(editorLivePreviewField)`.

---

## 12. Markoff gap confirmations / discoveries

Validating each Pass 1 "Editor / live-preview surface" bullet from `01-markoff-gaps.md`:

1. **Live-preview mode** — *Confirmed.* `editorLivePreviewField: StateField<boolean>` is the flag; `livePreview` vault config is the persisted default. The "cursor in block reveals source" semantic is implemented in `editor/markdown`, but the contract (a plugin-readable boolean) lives here. Markoff's QGraphicsView migration must materialise this boolean on `Markoff::Editor`, drive decorations from it, and expose it in any future plugin-extension API.

2. **Editor extensions registry** — *Confirmed.* `Plugin.registerEditorExtension(ext)` → `Workspace.editorExtensions[]` → `Y$` `Compartment` (`editorInfoField.js:130`). One flat list, applied to *every* `MarkdownView.cm` via `updateOptions()`. Markoff needs a Qt-native equivalent — a `Markoff::ExtensionRegistry` that cannot be the same *shape* (foundation is QGraphicsScene, not CM) but must preserve the contract: "one list, applied to every editor instance, registered/unregistered atomically with plugin load/unload."

3. **Bidi / RTL isolates** — *Indirect confirmation.* `EditorSuggest.updatePosition` (`EditorSuggest.js:92`) passes `LI(getLine(start.line))` (inferred bidi direction) into `reposition(rect, dir)` so the popup flips on RTL lines. At minimum, suggester popup positioning needs bidi-awareness. Bidi rendering itself is `rendering/RenderContext.js`'s problem.

4. **Multi-cursor / rectangular selection** — *Confirmed missing in Markoff.* `listSelections()` is plural; every list/heading toggle operates on the multi-cursor set. Markoff's QTextDocument single-cursor foundation cannot natively express this. Risk: plugins iterating `listSelections()` silently see only one selection. Document as explicit incompatibility in the plugin shim's docs.

5. **CJK IME input corners** — *Confirmed.* Autocorrect is **just three regexes** (`lP` at `Editor.js:7`): `／(！)?【【$／` → `![[`/`[[`, `／】】$／` → `]]`, `／【【$／` → `[[` (redundant fallback). `expandText()` runs them after every text-producing input. Cost to port is low. Additionally, `Editor.insertText` is the IME-composition-end fast-path — append to end-of-doc, set cursor at new end. Markoff's `inputMethodEvent` should mirror this.

6. **`EditorSuggest`** — *Confirmed.* Small, plugin-extensible. Four override methods; manager (`f0` / `EditorSuggestManager`, `views/ViewRegistry.js:238`) iterates suggesters in insertion order, stops at first non-null `onTrigger`. **First-non-null-wins, no priority.** Markoff's hardcoded `Markoff::Editor::detectCompletionTriggers` should be replaced by a `MarkoffSuggestRegistry` with insertion-order semantics. Subtle gap: the popup should re-position via `coordsAtPos` on trigger *start* and *end*, not just the cursor.

7. **Command-driven editor operations** — *Partly confirmed here.* This domain only exposes `exec(commandName)` as a delegate; the actual command catalog comes from CM6 (118 commands) plus Obsidian's own `editor:*` commands (in `editor/markdown`). Markoff exposes ≈30 slots. Bulk of the gap lives outside this domain.

### New Markoff signals discovered in this audit

8. **Async `getSuggestions` re-checks `editor.hasFocus()` before showing** (`EditorSuggest.js:55–58`). Markoff's `CompletionPopup` may race-show after focus moves; mirror this guard.
9. **`EditorSuggest.context` is replaced, not merged, on every trigger.** Plugin authors expecting persistence across keystrokes will be surprised.
10. **`Editor.insertText` appends at end-of-doc, NOT at cursor** (`Editor.js:50`). Naming gotcha; IME-composition-end fast-path. Markoff should mirror the end-of-doc semantic for the same name (or pick a different name and document the divergence).
11. **Triple-click line-extend selection** (`K$`, `editorInfoField.js:107–129`). Three rapid clicks anchor a line range; drag extends one line at a time. Verify Markoff implements the gesture end-to-end.
12. **`editorViewField === editorInfoField` literal reference identity** — any Corbomite shim exposing both names must update them together.
13. **`expandText` runs first-match-wins with a redundant third pattern.** Port both for behavioural parity.
14. **`editorEditorField` / `editorInfoField` may be `null` during view boot.** Markoff's analogue must guarantee init ordering (compartment populated *before* first `update`) or document the null-window.

These additions are appended to `01-markoff-gaps.md` under `## Pass 2 additions — editor`.

---

## 13. Open questions

1. Is the third entry in `lP` (`／【【$／` → `[[`) dead code or does ordering matter for some IME edge case? Cannot determine from source alone — would need a JP/CN IME composition trace.
2. Does `EditorSuggest.trigger`'s call chain ever pass `isOpen = true` for a first-time open vs a re-trigger? `if (n || this.isOpen)` (`EditorSuggest.js:41`) suggests `n` is a manual-open hint; the contract for plugin authors isn't documented.
3. Are the four `editor*Field` exports guaranteed to be the same `StateField` instance for the whole session, or could vault-switch / plugin-reload create new instances? Best guess from source: module-level constants, single-init.
4. `EditorSuggest.limit = 100` — public setter? (Plugins seem to assign `this.limit = N` in their constructor.) Would a per-trigger override be useful?
5. `Editor.processLines` `skipBlankLines = true` default — documented? Plugins that toggle list markers across a multi-line selection with embedded blanks may want it `false`.
6. Where is `B$` defined (per-line iteration helper at `editorEditorField.js:13/41`)? Not in this domain — likely `core` or `utils`.
7. Where is the `Y$.reconfigure(app.workspace.editorExtensions)` dispatch wired? Call chain is clear (workspace → updateOptions → MarkdownView → mode.updateOptions) but the actual `view.dispatch({effects: Y$.reconfigure(...)})` lives in `editor/markdown` code. Pass 2 of `editor/markdown` should pin this exactly.

---

## 14. Recommended Pass 3 synthesis input

1. **`Editor` is a thin documented wrapper around CodeMirror 6**, *and the wrapper itself is small*. This is good news for Markoff: porting the public surface is a bounded task (≈30 methods, mostly `processLines`-shaped). The hard parts (live-preview decorations, code-folding, multi-cursor) are *not* in this domain; they're in `editor/markdown` and `vendor/codemirror`. Promote into `GAP-ANALYSIS.md`: "the Obsidian-side editor wrapper API is bounded and portable; the depth of the gap is in the underlying engine, not the wrapper."
2. **The four `editor*Field` exports are the entire mechanism by which a plugin-supplied CM extension reaches back to Obsidian-level objects.** Any future Corbomite plugin API that wants to support "drop in a custom editor extension" needs an analogous handle-passing convention (e.g. an opaque `Markoff::EditorContext` passed into the extension callback). Promote into `FEATURE-MATRIX.md` as a foundational plugin-API row.
3. **Inline autocomplete is `EditorSuggest` + a flat insertion-order manager.** The mechanism is *small* and Markoff already has the popup widget. Promote into Pass 3 as a high-leverage "easy win" for the Corbomite plugin API: implementing `Markoff::SuggestRegistry` with insertion-order semantics gives `[[wikilink]]`, `#tag`, `@mention` etc. all at once.

---

## 15. Cross-domain references

| Other domain | Reference type | Brief description |
|---|---|---|
| `editor/markdown` | sibling | `MarkdownView` owns the `Editor` instance via `view.editor`, populates the four `editor*Field`s via `q$.of(...)` dispatches, and gates list-toggle commands on its mode. The `getDynamicExtensions` base lives in this sibling, not here. |
| `workspace` | consumer | `Workspace.editorExtensions[]` holds plugin-registered CM extensions; `Workspace.registerEditorExtension` is the registration verb; `Workspace.editorSuggest` (the `EditorSuggestManager` instance) holds plugin-registered `EditorSuggest`s. |
| `views` | consumer | `views/ViewRegistry.js` defines the `EditorSuggestManager` (`f0`) which owns `suggests: EditorSuggest[]` and dispatches `trigger` per cursor change. |
| `plugin` | consumer | `plugin/Plugin.js:197` (`registerEditorExtension`) and `plugin/Plugin.js:214` (`registerEditorSuggest`) are the public registration verbs. |
| `settings` | consumer | `settings/PluginSettingTab.js:197/214` mirror the same registration verbs. |
| `ui/popups` | dependency | `EditorSuggest extends PopoverSuggest` — the popup chrome (`suggestEl`, `suggestions`, `scope`, `open/close/reposition`) lives in `ui/popups/PopoverSuggest.js`. |
| `vault` | dependency | `app.vault.getConfig("livePreview")` is the persisted default that seeds `editorLivePreviewField`'s initial dispatch; `app.vault.getConfig("defaultViewMode")` (`"source" | "live" | "preview"`) is the per-view default. |
| `core` | dependency | `Editor.js` references `iP`, `oP`, `rP`, `_A`, `ac`, `pb`, `B$`, `LI`, `aP`, `sP` — all defined in core/utils slices outside this domain. |

| Short symbol | Defined in | Used here for |
|---|---|---|
| `iP` / `oP` / `rP` | `core` (likely) | `EditorPosition` constructor / equality / shift |
| `aP`, `sP`, `lP` | `editor/Editor.js` (locally) | list/heading regexes; full-width-bracket autocorrect table |
| `_A`, `ac`, `pb`, `LI`, `B$` | `core`/`utils` (likely) | blockquote-depth, array-dedup, number-cmp, bidi-direction, per-line iteration helper |
| `j$`, `q$`, `Y$`, `K$`, `H$`, `z$` | `editor/editorInfoField.js` (locally) | StateField factory; set-field StateEffect; extensions Compartment; triple-click selection style; indent/dedent commands |
| `f0` | `views/ViewRegistry.js` | `EditorSuggestManager` — dispatcher iterating `suggests` |
| `_I`, `YI`, `d0` | `views/ViewRegistry.js` (built-in suggesters) | file-link, header-of-current-file, tag |
| `PopoverSuggest` | `ui/popups/PopoverSuggest.js` | `EditorSuggest`'s base class |
| `Scope` | `core` (keymap scope) | `EditorSuggest.scope` keybinding scope |
| `Compartment`, `StateEffect`, `StateField`, `EditorView`, `EditorSelection` | CM6 (`vendor/codemirror`) | not audited per scope rules |
