# Editor domain audit

Scope: `obsidian/editor/{Editor.js, EditorSuggest.js, editor*Field.js}` →
Markoff live/source/core + `src/editor/` integration.

## Architecture fit (Markoff vs CodeMirror — translation soundness)

The Obsidian `Editor` is a thin **API wrapper** over a CodeMirror 6 `EditorView`
that enforces an `EditorPosition {line, ch}` shape, batches list/heading
toggles into one CM `transaction`, and re-exports a single boolean
(`editorLivePreviewField`) plus three plugin-readable handles
(`editorEditorField`, `editorInfoField`, `editorViewField`). Three of the four
StateField exports are *aliases* of one underlying instance; the registry
mechanism is "first-non-null `onTrigger` wins, no priority". The hard work
(decoration, multi-cursor, IME, bidi) lives in CM6 itself.

Corbomite is structured very differently:

- The "Obsidian Editor wrapper" role is split across `Markoff::Editor`
  (`libs/markoff-family/libs/markoff-live/include/markoff/Editor.h`),
  `Markoff::Source::SourceEditor`
  (`libs/markoff-family/libs/markoff-source/include/markoff/source/SourceEditor.h`),
  and the abstract `Markoff::MarkdownView`
  (`libs/markoff-family/libs/markoff-core/include/markoff/MarkdownView.h`).
- The host-app integration (`src/editor/NoteEditorWidget.cpp`) holds a
  `QStackedWidget` over the three leaf views and runs the **mode swap**
  (capture eState → detach → swap → attach → restore) at the host layer
  rather than inside Markoff. That's a divergence from CM6 (where mode
  is a `StateField<boolean>` on a single view) — see "live-preview" below.
- The single **`MarkoffDocument`**
  (`libs/markoff-family/libs/markoff-core/include/markoff/MarkoffDocument.h:28-75`)
  carries the canonical text + a `QUndoStack` of `MarkdownDelta` commands,
  and *every* leaf view subscribes to its `contentsChanged`/`documentReloaded`
  signals. That is the sound translation of CM6's "doc is shared, views
  observe" — well-fit, modulo no `userEvent` tagging on transactions
  (Obsidian leans on `userEvent="input.indent"`/`"input.type"` to gate
  behaviour; Corbomite has no analogue).
- The `Markoff::EditorContext` struct
  (`libs/markoff-family/libs/markoff-live/include/markoff/EditorContext.h:22-92`)
  is a Corbomite-original surface for "what is the cursor inside" that has
  no Obsidian counterpart (Obsidian computes this ad hoc from CM `state`
  inside each command). This is *better* than Obsidian for menu-state
  driving, but it is *not yet* exposed for the Source leaf.

Net: the substrate split is sound for live preview / source / reading
separation, but the **public API shape diverges sharply** from Obsidian's
`Editor` class — every plugin written against Obsidian's `editor.getCursor()
.line/.ch` will need a translation shim. The `MarkdownView` base class
(`libs/markoff-family/libs/markoff-core/include/markoff/MarkdownView.h:27-90`)
is the right place to put one, but currently exposes only opaque scroll/
zoom/cursor primitives, not Obsidian's `getLine`/`replaceRange`/`processLines`
family.

## Implemented (parity-equivalent)

- **Live-preview ↔ source ↔ reading mode swap.** The three-mode pivot exists
  as `Corbomite::NoteEditorWidget::ViewMode {Source, LivePreview, Reading}`
  (`src/editor/NoteEditorWidget.h:47`). Mode transitions go through a clean
  capture/detach/swap/attach/restore sequence
  (`src/editor/NoteEditorWidget.cpp:337-371`) that preserves cursor, scroll,
  and (for Reading) folded headings. This is **structurally cleaner** than
  Obsidian's `editorLivePreviewField` boolean + separate `MarkdownPreviewView`
  for reading mode — a single host owns the swap rather than mode state
  living in CM. Ephemeral state round-trips via `EphemeralState`
  (`src/editor/NoteEditorWidget.cpp:265-335`).
- **Cursor get/set.** `Markoff::Editor::cursorLine()/cursorColumn()`
  (`Editor.cpp:1578-1595`) and `goToLine`/`goToLineAndColumn`
  (`Editor.cpp:1609-1642`) cover Obsidian's `getCursor()`/`setCursor(line, ch)`
  semantically. `Markoff::Source::SourceEditor::cursorPosition()`
  (`SourceEditor.cpp:253-258`) returns a `Markoff::CursorPos {line, column}`
  via the abstract `MarkdownView` contract.
- **Plain-text I/O.** `setPlainText` / `toPlainText` exist on `Markoff::Editor`
  (declared at `markoff-live/include/markoff/Editor.h:67-69`) and
  `Markoff::Source::SourceEditor` (`SourceEditor.h:88`). Maps to Obsidian's
  `getValue`/`setValue` (subclass-supplied in CM, but contract-equivalent).
- **`coordsAtPos` analogue.** `Markoff::Editor::cursorScreenRect()`
  (`Editor.cpp:1597-1607`) returns the cursor's global-screen rect, used by
  `NoteEditorWidget::positionCompletionPopup`
  (`NoteEditorWidget.cpp:523-531`). Equivalent of Obsidian's `coordsAtPos`,
  though only one-direction (cursor → coords; no `posAtCoords(x, y)` /
  `posAtMouse(MouseEvent)`).
- **`EditorSuggest` base class + manager.** Full-shape port:
  - `Corbomite::EditorSuggest` abstract base
    (`libs/core/include/corbomite/core/EditorSuggest.h:41-62`) — three
    overrides (`onTrigger`, `getSuggestions`, `selectSuggestion`).
  - `EditorSuggestTriggerInfo {start, end, query}`
    (`EditorSuggest.h:22-26`) — mirrors Obsidian's `EditorSuggestTriggerInfo`
    shape exactly.
  - `Corbomite::EditorSuggestManager`
    (`libs/core/include/corbomite/core/EditorSuggestManager.h:20-47`)
    iterates `m_suggesters` insertion-order and returns the **first
    non-null `onTrigger`**. The dispatch loop
    (`libs/core/src/EditorSuggestManager.cpp:23-32`) is line-for-line
    equivalent to Obsidian's `EditorSuggestManager.trigger`.
  - Built-in suggesters: `WikiLinkSuggest` (`src/editor/WikiLinkSuggest.cpp`)
    for `[[`, `TagSuggest` (`src/editor/TagSuggest.cpp`) for `#`. Both
    register with the manager from MainWindow (assumed; not verified in
    this audit).
  - The popup widget (`Corbomite::CompletionPopup`,
    `src/editor/CompletionPopup.h`) parents to the editor's viewport,
    **non-focus-stealing** (`setFocusPolicy(Qt::NoFocus) +
    WA_ShowWithoutActivating`) — the canonical pattern for in-editor
    completion popups; better than a `Qt::Popup` window. Up/Down/Enter/Esc
    routed through the editor's eventFilter
    (`NoteEditorWidget.cpp:435-456`).
  - Completion accept writes through `MarkoffDocument::undoStack()` via
    `MarkdownDelta` (`NoteEditorWidget.cpp:614-619`) — preserves undo
    history; superior to a naïve `setPlainText` rewrite.
- **CJK full-width-bracket autocorrect.** Implemented in
  `Markoff::MarkdownTextItem::applyCjkBracketAutocorrect`
  (`libs/markoff-family/libs/markoff-live/src/MarkdownTextItem.cpp:999-1033`).
  Triggered on every text-producing keystroke
  (`MarkdownTextItem.cpp:986-987`). Three of Obsidian's three patterns:
  `！【【` → `![[`, `【【` → `[[`, `】】` → `]]`. Longest-match-first ordering
  preserved.
- **IME composition path.** `MarkdownTextItem::inputMethodEvent`
  (`MarkdownTextItem.cpp:1035-1038`) forwards to `TextControl`, which has a
  full preedit-area implementation
  (`libs/markoff-family/libs/markoff-live/src/TextControl.cpp:2176-2270`)
  harvested from `qtbase`. `inputMethodQuery` is forwarded
  (`MarkdownTextItem.cpp:1040-1043`); `Qt::ImCursorRectangle`/`ImCursorPosition`
  are handled (`TextControl.cpp:1586-1598`). This is *substantially* closer
  to a real CM6-quality IME path than a naïve QGraphicsView would give.
- **Find/replace.** `Markoff::Editor::findText`/`replaceText`/`replaceAll`
  (`Editor.cpp:1745-1865`) walk every `MarkdownTextItem` in scene order,
  cross-item, with one wrap pass — equivalent to CM6's per-document
  `find` + a wrap. `Markoff::Source::SourceEditor` exposes a separate
  `ReplaceController` (`SourceEditor.cpp:107-115`) that drives a
  `Markoff::SearchBar`. C7 wired find-bar virtuals through `MarkdownView`
  (`MarkdownView.h:80-84`) so MainWindow dispatch is uniform across leaves.
- **Editor-extensions registry shape (partial).** No `editorExtensions[]`
  flat list exists, but `EditorSuggestManager` is *exactly* the right
  shape (insertion-order, applied to every editor) for the suggester half
  of Obsidian's plugin extension surface. The CM6 `Extension` half (custom
  `StateField`, `ViewPlugin`, `Decoration`) has no analogue.
- **List item autocontinue (Enter inside `- foo`).** Implemented in
  `MarkdownTextItem` / `TextControl` keypress handling (the `keyPressEvent`
  path at `MarkdownTextItem.cpp:980-996` runs before autocorrect). Not
  audited end-to-end here; Markoff has the test
  `tst_textcontrol_input.cpp` covering it.

## Partial / divergent

- **Plain-text mapping vs `EditorPosition`.** Markoff exposes 1-based
  `(line, column)` via `Markoff::CursorPos`
  (`libs/markoff-family/libs/markoff-core/include/markoff/CursorPos.h:11-21`),
  while Obsidian's `EditorPosition {line, ch}` is 0-based. Off-by-one shift
  is hardcoded at every transition site
  (`NoteEditorWidget.cpp:280-291` for Source, `:319-325` for Live restore).
  Any plugin shim must convert at the boundary; this is **fragile** and
  invites silent off-by-one bugs in plugin code. Recommend the eventual
  plugin API exposes 0-based positions matching Obsidian.
- **Live editor `cursorColumn()` returns 1-based.**
  (`Editor.cpp:1590-1595` — `columnNumber() + 1`.) The `EphemeralState`
  capture at `NoteEditorWidget.cpp:291` then subtracts 1 again — the value
  flowing through eState is 0-based. The Editor's *public* signal
  `cursorPositionChanged(int line, int column)` fires 1-based both, which
  is also what `cursorInfoChanged` rebroadcasts. Inconsistency: Source's
  `Markoff::CursorPos.column` is **0-based** (`SourceEditor.cpp:257`,
  `c.columnNumber()`), Live's `cursorColumn()` is **1-based**. This is a
  latent bug for any consumer that compares the two without knowing which
  leaf is active.
- **Find / replace cross-item wrap.** Markoff's wrap is one-pass per
  search call (`Editor.cpp:1810-1820`), not the continuous wrap-on-end
  semantics CM6 has via `cycleMatches`. Edge case: after the cursor is
  past the last match in the focused item, "find next" wraps to the
  *focused item's start*, not to the next item's start. Not necessarily
  a bug — different semantics, document carefully.
- **`EditorSuggest` async support.** Obsidian's `getSuggestions` may
  return a `Promise<T[]>`; the manager re-checks `editor.hasFocus()` after
  resolution and silently `close()`s if focus moved
  (`EditorSuggest.js:55-58`). Corbomite's `EditorSuggest::getSuggestions`
  is **synchronous-only** (`EditorSuggest.h:55` — returns `QStringList`
  by value). For Q-style plugin parity (vault search, network calls)
  this needs `QFuture<QStringList>` or a `QPromise` shape. Mark as a
  pre-1.0 break.
- **EditorSuggest position hint.** Obsidian's `EditorSuggest.updatePosition`
  (`EditorSuggest.js:80-92`) re-anchors via `coordsAtPos(start)` /
  `coordsAtPos(end)` + bidi direction. Corbomite's
  `NoteEditorWidget::positionCompletionPopup`
  (`NoteEditorWidget.cpp:523-531`) only uses the **cursor** rect, not
  the trigger-start/end — popup never reflows to follow the trigger
  range. For `[[long query]]` this is fine; for RTL lines it will be
  wrong (no bidi flip).
- **Mode-swap save.** Obsidian's `MarkdownView.setMode` synchronously
  `await`s `save()` before leaving source mode (Pass 2 editor-markdown
  signal). Corbomite's `setViewMode`
  (`NoteEditorWidget.cpp:337-371`) does NOT save before swapping; the
  shared `MarkoffDocument` carries dirty content across the swap (so
  no data loss), but on Reading mode the user sees *unsaved* rendered
  content. Probably fine since `MarkoffDocument` is canonical, but
  divergent semantics from Obsidian.
- **Edit-block grouping vs single transaction.** Obsidian's
  `Editor.processLines` wraps every list/heading toggle into one CM
  `transaction({changes})` so undo treats the batch atomically. Markoff
  uses Qt's `cursor.beginEditBlock()` / `endEditBlock()` (`Editor.cpp`
  multiple sites: `:1316, :1347, :1372, :1394, :1469, :1856, :2449-2516`)
  — same intent, but the canonical undo stack is *not* `QTextDocument`'s;
  it is `MarkoffDocument::undoStack()` carrying `MarkdownDelta` commands.
  The `MarkdownDelta`s are pushed by per-leaf `contentsChange` listeners
  (`SourceEditor.cpp:157-175`). It's **unclear** whether one `beginEditBlock`
  results in one MarkdownDelta or N (one per QTextDocument::contentsChange
  emission inside the block). If the latter, undo will be granular even
  inside a "single" toggle — verify in tests.
- **Live `setPlainText` / `clear()`.** Public on Markoff::Editor but
  bypass the canonical undo stack — direct doc mutation. Should NOT be
  used by plugin code; Corbomite uses them only on initial mount.
- **`ReadingView` and the `MarkdownView` base.** The base
  (`MarkdownView.h:66-77`) declares `hasCursor()` / `hasEditing()` /
  `hasFold()` capability probes. Reading returns false for `hasCursor()`,
  so its `cursorPosition()` returns a default `CursorPos`. This means
  `goToLine` in Reading mode is a no-op
  (`NoteEditorWidget.cpp:259-261`). Divergent from Obsidian's reading
  mode which has scroll-to-line via `currentMode.applyScroll(line)`.

## Missing

- **`Editor.getDoc() -> this`** — CM5 compat shim. No analogue
  needed unless plugins assume it.
- **`getLine(n) -> string`, `lineCount()`, `lastLine()`, `firstLine()`.**
  No public per-line accessor on `Markoff::Editor` or
  `Markoff::Source::SourceEditor`. `Qutepart::Lines`
  (`qutepart/qutepart.h:186-220`) gives line-by-line access on the Source
  leaf, but it's *not* exposed via the Markoff public surface — the
  `Qutepart::Qutepart*` accessor (`SourceEditor.h:78`) is internal-only.
  Live editor: only `toPlainText()` exists; consumers must split
  themselves. **Plugin-API parity blocker.**
- **`getRange(from, to) -> string`, `replaceRange(text, from, to?)`.** No
  position-by-position range API on either leaf. Workaround at
  `NoteEditorWidget.cpp:611-619` builds a `MarkdownDelta` directly with
  raw byte offsets — that is the *canonical* primitive but is exposed via
  `MarkoffDocument`, not via the editor wrapper. Plugins routing through
  `MarkdownView` have no way to do this.
- **`setLine(n, text)`.** Trivially expressible as `replaceRange(text,
  {line:n,ch:0}, {line:n,ch:getLine(n).length})` — depends on the missing
  range API.
- **`getSelection() -> string`, `replaceSelection(text)`.** Live editor
  has implicit selection access via `focusedTextItem()->textControl()
  ->textCursor().selectedText()` (used internally at
  `Editor.cpp:1878, 1224, 1834`) but not surfaced as a `Markoff::Editor`
  public API. Source has it via `qutepart()->textCursor().selectedText()`
  but the accessor is internal-only.
- **`listSelections() -> EditorSelection[]`.** Multi-cursor selection.
  `Markoff::Editor` is single-cursor: `focusedTextItem()` returns one
  text item with one `textCursor()`. **Notable**: `Qutepart` *does*
  natively support multi-cursors via `extraCursors`
  (`qutepart.cpp:1054-1080, 2043, multipleCursorPaste/Copy/Cut`) — Source
  mode has it; Live mode does not. Cross-mode behaviour for plugins that
  read `listSelections()` is going to be discontinuous.
- **`setSelection(EditorPosition | EditorSelection)` / `somethingSelected()`.**
  No public selection API on either leaf.
- **`posAtCoords(x, y)` / `posAtMouse(MouseEvent)`.** No way for plugin
  code to convert a mouse event to a document position. `cursorScreenRect()`
  goes the other direction. Required for context-menu features that act on
  the clicked-but-not-cursored character.
- **`wordAt(pos) -> {from, to}`.** No analogue.
- **`exec(commandName: string)`.** No string-keyed command execution.
  Markoff exposes commands as `QAction` objects via `Editor::action(ActionId)`
  (`Editor.h:62`), which is the **better** Qt-native shape, but plugins
  porting `editor.exec("indentMore")` will need a name→ActionId map.
- **`Editor.transaction({changes, selection, userEvent})`.** No public
  multi-change-as-one-transaction API on the editor wrapper. The canonical
  `MarkdownDelta` *is* a single-change command; batching N edits into one
  undo step requires either pushing one `MarkdownDelta` with the union
  text (manual diff) or a higher-level `QUndoCommand` that owns N
  `MarkdownDelta` children.
- **`processLines(matcher, mutator)` helper.** No public per-cursor
  per-selected-line iterator that dispatches as one transaction.
- **`Editor.insertText(text)` end-of-doc-append IME fast-path.**
  Markoff has no end-of-doc append helper; `insertAtCursor` does what
  Obsidian's `replaceSelection` would do.
- **`expandText()` and other CJK fallback shapes.** Only the bracket
  table is implemented; no general "expand text post-IME-commit" hook
  for plugins to register additional patterns. (Obsidian's `expandText`
  only knows the three full-width-bracket patterns, but the function is
  the named interception point — Corbomite has none.)
- **`Editor.newlineAndIndentContinueMarkdownList`** — the smart-Enter
  inside lists/blockquotes is the **single biggest list-aware helper** in
  Obsidian's `Editor.js:287-444`. Corbomite has *some* of this in
  `MarkdownTextItem` keypress handling, but no audit of the full state
  machine: blank-line-ends-list, marker-shift-removes-quote-level,
  number-auto-increment, double-marker-suppression. Recommend a focused
  test sweep against `Editor.js:287-444`.
- **Triple-click line-extend (Obsidian's `K$` mouseStyle).** Not
  implemented in `Markoff::Editor` or `TextControl`. `mouseDoubleClickEvent`
  exists (`TextControl.cpp:2006-2050`) but no triple-click line range.
  Qt's default in `QTextEdit` does triple-click-selects-line; verify
  whether `TextControl` (the CM-derived custom control) inherits it.
- **`editorEditorField` / `editorInfoField` / `editorViewField` plugin
  handles.** The "let a plugin's editor extension reach back to the
  wrapping `Editor` / `MarkdownView` / `TFile`" mechanism has **no
  analogue**. This is foundational for the plugin-API parity vector
  flagged in the audit; without it, plugins can't tell which note their
  hook is firing on.
- **`editorLivePreviewField`-equivalent boolean signal.** The mode swap
  fires `viewModeChanged(ViewMode)` (`NoteEditorWidget.h:114`), but it
  encodes three states, not the boolean Obsidian plugins read. Map
  `LivePreview → true; Source/Reading → false` for compat; expose as a
  separate signal for plugins.
- **CodeMirror-style extension surface (`StateField`, `ViewPlugin`,
  `Decoration`, `Facet`, keymap).** Markoff has *no* analogous
  Qt-native plugin-extension model for editor-internal hooks. Fold
  decorations, embed widgets, link decorations all live in concrete
  C++ classes (`SceneCoordinator`, `MarkdownHighlighter`, `LinkRenderer`).
  Plugins porting CM extensions cannot land them today.
- **Spell-checking.** No Sonnet integration found in markoff-live or
  markoff-source. Qutepart's theme defines a `SpellChecking` slot
  (`theme.h:71`) but no live spell-checker hook. Notable gap vs every
  modern editor including Obsidian (which uses the OS-supplied browser
  spellcheck).
- **Vim mode.** Not implemented and not planned (per source grep).
  Obsidian ships an optional `vim` mode via CodeMirror's `@codemirror/vim`
  package; the `cm.html` file exposes `view.dom.dataset.vimMode`. No
  Markoff/Corbomite analogue. Backlog candidate.
- **Paste-as-Markdown (HTML→MD via Turndown).** `TextControl::insertFromMimeData`
  (`TextControl.cpp:1667-1677`) handles **plain text only**; `hasHtml()`
  is ignored. Pasting from a browser drops all formatting. Obsidian's
  `htmlToMarkdown` (Turndown) auto-converts. Backlog.
- **`Editor` "command palette" wired to 118 CM6 commands.** Markoff
  exposes ~30 ActionIds (`Editor.h:29-40`); CM6 + Obsidian collectively
  expose ~150. The remaining ~120 (cursor/select/delete granularity
  variations, fold/unfold-region, jumpToBracket, etc.) are not surfaced
  as `QActions` or commands.
- **Bidi-isolate decorations (`cm-iso` per inline span).** Per-paragraph
  bidi works (Qt's default), but no per-inline isolate — mixed-script
  inline content in headings/paragraphs will render differently from
  Obsidian.
- **Hover-popover modifier-pin.** `HoverPopover` exists (`src/editor/
  HoverPopover.h`) but not audited for the Obsidian "press Mod to pin
  popover open and click links inside it" gesture. Per the audit's
  Pass-2 ui-bundle finding, this is required for hover-link UX parity.

## Notable translation successes

- **`MarkoffDocument` as the single canonical buffer.** Obsidian's CM6
  `EditorState` is per-`EditorView`; cross-leaf consistency in Obsidian
  is ad hoc (the `quick-preview` event re-renders linked previews). In
  Corbomite, every leaf subscribes to `contentsChanged` on the *same*
  `MarkoffDocument` and rebases its display. This is **structurally
  cleaner** than CM6 for the multi-pane / multi-mode case and a real
  improvement on Obsidian's design.
- **`Markoff::EditorContext` block-classification snapshot.**
  (`EditorContext.h:22-92`.) A reactive, debounced
  `contextChanged(EditorContext)` signal carrying `BlockKind`,
  `headingLevel`, `inBold/Italic/Strikethrough/InlineCode`, `hasSelection`,
  `atBlockStart/End`, `readOnly`, and a `TableContext`. Obsidian computes
  this state ad hoc inside each command/menu callback; Markoff exposes
  it as a single pull (`Editor::context()`) plus a debounced push.
  Better foundation for menu/toolbar enable-state than Obsidian.
- **`EditorSuggestManager::dispatch` is a 6-line port.** The "first
  non-null `onTrigger` wins" iteration is line-for-line equivalent
  (`EditorSuggestManager.cpp:23-32`). Insertion-order semantics
  preserved. **Behaviourally identical** to Obsidian for the suggester
  half of plugin extensibility.
- **CompletionPopup-as-child-widget pattern.** Parenting to the
  viewport with `Qt::NoFocus + WA_ShowWithoutActivating` and routing
  Up/Down/Enter via the editor's `eventFilter`
  (`NoteEditorWidget.cpp:435-456`) is the **correct** Qt-native
  translation of CM6's "popup is a sibling DOM node, not a window"
  pattern. Avoids focus-stealing (a real risk on top-level `Qt::Popup`
  widgets) and naturally handles editor-focus-out dismissal
  (`NoteEditorWidget.cpp:430-433`).
- **Completion accept routes through canonical undo stack.**
  (`NoteEditorWidget.cpp:614-619`.) Obsidian-equivalent plugins call
  `editor.replaceRange(...)` from `selectSuggestion`, which goes
  through the CM transaction pipe (and undo). Corbomite's
  `MarkdownDelta` push is the analogue. This was a deliberate
  improvement over an earlier `setPlainText` rewrite (per the
  comment).
- **CJK autocorrect.** Three-pattern table is implemented and
  triggered on every text-producing keystroke after IME commit. The
  longest-match-first ordering is preserved (`MarkdownTextItem.cpp:1002`
  comment). Note: the third Obsidian regex (`【【` redundant fallback)
  is *not* ported because Markoff's two-pattern logic already covers
  it; this is correct, not a gap.

## Notable concerns / suspected bugs

1. **Live `cursorColumn()` is 1-based; Source `CursorPos.column` is
   0-based.** (`Editor.cpp:1590-1595` vs `SourceEditor.cpp:257`.)
   Anything cross-leaf comparing column numbers will break silently.
   `EphemeralState` capture/restore code accommodates this with manual
   ±1 shifts (`NoteEditorWidget.cpp:280-291, 312, 319-325`); plugin
   code via the `MarkdownView` base will hit this with no warning.
   Recommend: pick one convention and audit.

2. **CJK autocorrect cursor-desync on focused-item edits.**
   Documented as a known TODO at `TextControl.cpp:1683-1700`
   (defensive `qCWarning` `markoff.live.text_control.cursor_drift`).
   The `applyCjkBracketAutocorrect` (`MarkdownTextItem.cpp:999-1033`)
   uses a local `QTextCursor` on the same shared `QTextDocument` the
   `TextControlPrivate::cursor` holds — on every non-empty keystroke
   the cached position can desync. **Real bug**; Markoff has flagged
   it for the D2 follow-up. Suggesters' `cursorPos` reads downstream
   of this desync may also see stale values.

3. **`detectCompletionTriggers` runs in parallel with `EditorSuggestManager`.**
   (`Editor.cpp:1681-1707` emits `wikiLinkTrigger`/`tagTrigger` on every
   `[[` / `#`; `NoteEditorWidget` *also* listens to
   `cursorPositionChanged` and dispatches through the manager
   (`NoteEditorWidget.cpp:76-77, 476-521`).) Two trigger paths — the
   legacy hardcoded one in `Markoff::Editor` and the manager-based one
   in `NoteEditorWidget`. The latter dedups the popup
   (`NoteEditorWidget.cpp:498-502`); the former emits signals into
   the void. Cleanup candidate: remove `detectCompletionTriggers` and
   the `wikiLinkTrigger`/`tagTrigger` signals in favour of the
   manager. Confirms `01-markoff-gaps.md` Pass-2 editor item #6.

4. **`completionDismissHint` signal exists** (`Editor.h:322`,
   wired at `NoteEditorWidget.cpp:71`) — but it's emitted from
   nowhere I could find via grep. Likely wired internally; verify
   the editor actually fires it on text-changed-clearing-trigger.

5. **`updateCompletionFilter` dismisses on `]` or newline**
   (`NoteEditorWidget.cpp:570-573`). For the **tag** suggester
   (which has no closing punctuation), this means typing `#tag1, #tag2`
   correctly dismisses on the `,` only by accident (because `,`
   doesn't trip the bail conditions but the *cursor* leaves the
   trigger range and `WikiLinkSuggest::onTrigger` returns nullopt
   on next dispatch). Tag dismissal is happening through fall-through,
   not deliberate logic.

6. **`absoluteCursorPos()` does an O(N) line-scan over `toPlainText()`
   on every cursor change** (`NoteEditorWidget.cpp:533-545`). For a
   large note with deep cursor movement, `cursorPositionChanged` →
   `maybeActivateSuggester` → `absoluteCursorPos` runs a full string
   scan. The `Markoff::Editor` already knows the absolute offset via
   its `focusedTextItem()->textControl()->textCursor().position()` plus
   the `SceneCoordinator::globalPositionOf` machinery used for
   `cursorLine`. Recommend an `Editor::absoluteCursorPosition()`
   accessor.

7. **CompletionPopup forces `setFixedWidth(300)`**
   (`CompletionPopup.cpp:69`). Long wikilink note basenames truncate.
   Obsidian's popup is content-sized with a max. Minor UX gap.

8. **`Markoff::Editor::insertAtCursor` does not call
   `MarkdownDelta`.** (`Editor.cpp:1276-1281`.) It calls
   `textControl()->insertPlainText(text)` directly, which mutates
   `QTextDocument` and only reaches the canonical `MarkdownDocument`
   via `SceneCoordinator`'s `contentsChange` re-emission. Verify this
   round-trip generates the right `MarkdownDelta` on the canonical
   undo stack. The same pattern applies to `wrapSelection`,
   `setHeadingLevel`, `toggleCheckbox` etc. — every formatting action
   bypasses the `MarkdownDelta` constructor and relies on the
   coordinator listener to materialize one. This is brittle: missed
   listener wiring → silent loss of an edit on the canonical stack.

9. **No `setLineModified` (Qutepart line-flag) used by the Source
   leaf** for the canonical-vs-disk dirty state. `Qutepart` has
   per-line `MODIFIED_BIT` (`qutepart.h:32-38`) — could drive a
   per-line modified gutter for free. Backlog.

10. **`Markoff::Editor::clear()` (`Editor.h:68`) bypasses the
    canonical `MarkoffDocument`** — used at `NoteEditorWidget.cpp:109`
    when `m_doc == nullptr`. Cosmetic but: any leaf showing the
    "no document" state via `clear()` will not match what
    `MarkoffDocument::toMarkdown()` returns (which is the *previous*
    document's text until `setDocument(nullptr)` is also called).
    Tightly coupled to the `setDocument(nullptr)` happening before
    `clear()` — verify the ordering invariant holds.

## Markoff API gaps (for future Editor-API parity)

Targeted at the eventual `Markoff::Editor` plugin shim. Roughly priority-
ordered.

1. **Per-line accessors on `MarkdownView`.**
   `virtual int lineCount() const`, `virtual QString getLine(int n) const`,
   `virtual int lineLength(int n) const`. Source can delegate to
   `Qutepart::Lines`; Live can iterate `MarkdownTextItem`s; Reading
   returns the cached source from `MarkoffDocument`.

2. **Range edit primitive.** `virtual void replaceRange(EditorPosition
   from, EditorPosition to, const QString &text, const QString &userEvent
   = {})`. Builds a `MarkdownDelta` and pushes to
   `MarkoffDocument::undoStack()`. Add `userEvent` so plugin observers can
   filter (mirrors Obsidian's `"input.indent"` / `"input.type"`).

3. **Selection accessors.** `virtual QString selectedText() const`,
   `virtual void replaceSelection(const QString &text)`, `virtual bool
   somethingSelected() const`, `virtual QVector<EditorSelection>
   listSelections() const`. The plural form must be a Markoff-canonical
   shape (single Live + multi Source) so cross-leaf code sees a uniform
   API even if the Live half always returns one element.

4. **Multi-cursor uplift on Live.** Either harvest the multi-cursor
   logic from `qutepart.cpp:1054-1080` into `MarkdownTextItem` /
   `TextControl` or document the asymmetry. Currently Source has
   multi-cursor; Live does not — plugins acting on
   `listSelections()` will see different behaviour per mode.

5. **`posAtMouse` / `posAtCoords`.** `virtual std::optional<EditorPosition>
   posAtCoords(int x, int y) const` — the inverse of the existing
   `cursorScreenRect`. Required for context-menu plugins acting on
   under-the-mouse-but-not-cursor positions, hover-link disambiguation,
   and EditorSuggest popup positioning.

6. **0-based / 1-based normalisation.** Pick one (recommend 0-based
   for Obsidian compatibility) and convert at the leaf boundary.
   Audit every `+1` / `-1` site.

7. **`processLines` helper.** A free function on `Markoff::Editor`
   that takes a per-line matcher + mutator and emits one
   `QUndoCommand` (parent) with N `MarkdownDelta` children. Bonus:
   the cursor-stays-at-absolute-character special-case from Obsidian
   (`Editor.js:57-115`) for the single-cursor `ch === 0` selection.

8. **EditorSuggest async support.** Change the return type to
   `QFuture<QStringList>` (or accept both via overload). On resolution,
   re-check `editor.hasFocus()` before showing.

9. **EditorSuggest popup re-anchoring.** Use trigger `start`/`end`,
   not the cursor, for `coordsAtPos` lookup. Add a bidi flip when the
   line direction is RTL (Qt has `QTextOption::TextDirection`).

10. **Plugin-extension surface.** A `Markoff::EditorExtension` interface
    (or `Component` subclass) registered with a hypothetical
    `Markoff::ExtensionRegistry` that mirrors `editorExtensions[]` —
    one flat list, applied to every `MarkoffDocument`. Not a CM6 port;
    a Qt-native interface that maps Obsidian extension hooks
    (decoration, key handling, transaction filter) onto Markoff's
    QGraphicsScene. This is the **single largest bounded plugin-API
    gap**.

11. **Editor-context handle for plugin extensions.** Per the audit's
    `editorEditorField` / `editorInfoField` finding: a plugin extension
    needs a way to ask "which `Markoff::Editor` am I in? which
    `NoteDocument` does it own?" A `Markoff::EditorContextHandle` passed
    into every extension callback (or an opaque field on the extension
    itself) is the correct shape.

12. **Boolean live-preview signal.** Expose
    `Markoff::Editor::isLivePreview()` (or on `MarkdownView`) plus a
    signal mirroring `editorLivePreviewField`. Map `LivePreview ↔ true`,
    `{Source, Reading} ↔ false`.

13. **`exec(commandName)` shim.** Not strictly necessary if plugins
    bind `QAction`s by `ActionId`, but for porting code from Obsidian's
    `editor.exec("indentMore")` style a `QHash<QString, ActionId>`
    lookup is a 5-line shim worth shipping.

14. **`Editor.transaction({changes, selection, userEvent})` analogue.**
    A public batch method on `MarkdownView` that takes a vector of
    range edits + a final cursor and dispatches as one `MarkdownDelta`
    (or one parent `QUndoCommand`). Without it, plugins cannot
    achieve atomic multi-edit undo.

15. **HTML paste → markdown.** A Turndown analogue. Not in this
    domain proper; cross-cuts with `parsing/`. Recommend:
    `Markoff::htmlToMarkdown(QString html)` as a shared free function
    in `markoff-parser`, called by `TextControl::insertFromMimeData`
    when `source->hasHtml()`.

16. **Triple-click line-extend mouseStyle.** Add a triple-click
    handler in `TextControl::mousePressEvent` (or on
    `MarkdownTextItem`) that selects the current line; drag extends
    one line at a time. Match the Obsidian/Qt-native gesture.

17. **Editor extension `null`-window guarantee.** Per the audit:
    `editorEditorField`/`editorInfoField` may be `null` during view
    boot in Obsidian. Markoff should *not* replicate this gap — guarantee
    that any extension callback fires only after the canonical
    `MarkoffDocument` is attached.

---

End of audit.
