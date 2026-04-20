# 2026-04-15 — Completion popup rewrite

## Bug

User reports: typing `[[` or `#` in the editor pops up a completion list, but **everything is dead**. Typing in the editor doesn't insert characters, arrow keys don't navigate the popup, Enter doesn't accept, only clicking outside dismisses. Total dead-lock.

Verified scope:
- `Markoff::Editor::wikiLinkTrigger` and `tagTrigger` signals fire correctly. New test `libs/markoff/tests/tst_completion_triggers.cpp` proves it.
- `NoteEditorWidget::triggerWikiLinkCompletion` is reached and constructs a `CompletionPopup`.
- The bug is entirely inside `src/editor/CompletionPopup.{h,cpp}` plus the editor-side key-routing it expects but does not get.

## Root cause

`src/editor/CompletionPopup.cpp:53`:

```cpp
CompletionPopup::CompletionPopup(QAbstractItemModel *sourceModel, QWidget *parent)
    : QFrame(parent, Qt::Popup | Qt::FramelessWindowHint)
```

`Qt::Popup` makes Qt:
- Grab the mouse globally — clicks outside dismiss
- Treat the popup as a top-level window
- **Refuse to forward keypresses to anything outside the popup**

Combined with the popup's children all having default focus policy, the `QListView` does not have the keyboard focus implicitly — so arrow/Enter do nothing inside the popup either. And the editor's `MarkdownTextItem` sits inside a `QGraphicsScene` inside a `QGraphicsView`; the popup-grab interrupts the focus chain that would normally let typing reach it.

## Documented requirements (sources)

- **`docs/superpowers/specs/2026-03-31-completion-and-link-navigation-design.md` lines 27–33** — "Does NOT steal focus from the editor — keystrokes stay in the editor"; arrows/Enter/Tab forwarded *from* editor *to* popup; everything else filters and stays in the editor; dismisses on Esc / focus loss / click outside / accepting.
- **`docs/obsidian-audit/domains/editor.md` line 136** — `updatePosition` recomputes from the *trigger range*, not the cursor.
- **Cluster H plan §`Corbomite::EditorSuggestModal`** — eventual home of this widget; will become Component-based, plugin-extensible. This rewrite stays compatible with that future shape (no API surface decisions that block Cluster H).

## Pattern: KDevelop / KateCompletionWidget

Verified in `~/src/kde/src/ktexteditor/src/completion/katecompletionwidget.{h,cpp}`:

- Plain `QFrame` parented to the view, **not** `Qt::Popup`. (`.cpp:86`)
- `setFocusPolicy(Qt::ClickFocus)`; recursively `Qt::NoFocus` on every child. (`.cpp:157,161`)
- Inline comment: *"This is a non-focus widget, it is passed keyboard input from the view"*. (`.cpp:154`)
- The view's `keyPressEvent` intercepts the navigation keys and routes them to the completion widget; everything else falls through to normal editor handling. (`kateviewinternal.cpp:3019–3050`)
- Dismissal on: cursor leaves the trigger range, vertical scroll, focus-out, Esc.

## Fix scope (this spec)

1. `src/editor/CompletionPopup.{h,cpp}` rewrite:
   - Drop `Qt::Popup | Qt::FramelessWindowHint`. Construct as a plain child `QFrame` of the editor (or its viewport).
   - `setFocusPolicy(Qt::NoFocus)`; recursively `setFocusPolicy(Qt::NoFocus)` on the inner `QListView`.
   - Drop `Qt::WA_DeleteOnClose` (parent owns).
   - Add `acceptCurrent()` slot — emits `itemSelected` for the currently-highlighted row. (Used by the editor's Enter handler.)
   - Keep `selectNext`/`selectPrevious`/`setFilterText` API.
   - Re-position on geometry changes (parent resize / scroll). Defer scroll handling to caller for now (it owns the editor's scroll signal).

2. `src/editor/NoteEditorWidget.{h,cpp}`:
   - When a popup is active, install an event filter on the `Markoff::Editor` (already partly there).
   - In the filter, if event is `KeyPress`:
     - `Up` / `Down` → `popup->selectPrevious/Next`, **eat** the event (return true).
     - `Return` / `Enter` / `Tab` → `popup->acceptCurrent`, eat.
     - `Escape` → `dismissCompletion()`, eat.
     - Anything else → fall through (the editor processes the keypress normally).
   - On every editor `textChanged` while popup active, recompute the filter text as the slice from `m_completionTriggerPos` to current cursor; pass to `popup->setFilterText`.
   - On cursor move that leaves the trigger range (cursor < trigger pos, or moves to a different block), call `dismissCompletion()`.
   - On editor focus-out, call `dismissCompletion()`.
   - Position: `cursorScreenRect().bottomLeft()` mapped to editor-viewport coords (parent of popup). Re-check after every filter change in case wrap shifts the cursor.

3. New e2e test `tests/e2e/tst_completion_popup.cpp`:
   - Open vault, open a note, focus editor.
   - Type `[[`. Assert popup is visible.
   - Type more characters. Assert filter narrows the list **and** the editor inserts the characters.
   - Press Down. Assert popup selection moves and editor cursor does not move.
   - Press Enter. Assert popup dismisses, editor inserts the selected text + closing `]]`.
   - Type `[[` again. Press Esc. Assert dismissed, editor unchanged after the trigger.

## Out of scope (deferred to Cluster H)

- `Corbomite::EditorSuggestManager` plugin registry.
- `EditorSuggest<T>` abstract base; refactoring `[[` and `#` into separate suggesters.
- Shared `FuzzyMatcher` with highlight spans (`KFuzzyMatcher` already used for filter scoring; spans not yet).
- Trigger refactor for `![[`, `#tag/sub`, etc.
- Async `getSuggestions` with focus guard.
- `Component`-ification of the popup lifecycle.

These all stay viable on top of this rewrite — none require API the rewrite forbids.

## Definition of done

- Typing `[[` or `#` shows a popup.
- While the popup is visible, the user can keep typing into the editor and the popup filter narrows in real time.
- Arrow keys move the popup selection without affecting the editor cursor.
- Enter/Tab inserts the selected text and dismisses.
- Esc dismisses without inserting.
- Click outside dismisses.
- Cursor leaving the trigger range dismisses.
- Editor focus-out dismisses.
- E2E test passes; no test regressions.
