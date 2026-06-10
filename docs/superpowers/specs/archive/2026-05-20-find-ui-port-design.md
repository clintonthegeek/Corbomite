# Find UI port — design

**Date:** 2026-05-20
**Branch:** `port/foundation-exploration` (Corbomite); consumes Markoff `exploration/new-foundation`.
**Scope:** MVP Find UI for the post-foundation-exploration Corbomite. Replaces the per-leaf find UIs retired in Markoff commit `634206b`. No Replace, no flag toggles, no history.
**Status:** Approved (this session). Plan and implementation to follow.

## Why

Master-side Corbomite drove Find through `MarkdownView::showFindBar()` virtuals on each Markoff leaf (Live and Source had their own bars; Reading was a no-op). Those virtuals were retired during the D-arc + E-arc rebuild in favor of a consumer-owned `Markoff::FindController` that exposes search logic only — view leaves render highlights and respond to `navigationRequested`, but the UI lives in the consumer.

This is the port branch's first end-to-end "real" feature port: it exercises the new controller surface from a real consumer, surfaces zero or one Markoff-side micro-spec (per the 2026-05-20 port-first session recap), and re-lights the stubbed `KStandardAction::find` / `FindNext` / `FindPrevious` in MainWindow.

## The Markoff::FindController contract

From `libs/markoff-core/include/markoff/core/FindController.h`:

```cpp
class FindController : public QObject {
  Q_PROPERTY(QString needle             READ needle WRITE setNeedle NOTIFY needleChanged)
  Q_PROPERTY(int     matchCount         READ matchCount             NOTIFY matchesChanged)
  Q_PROPERTY(int     currentMatchIndex  READ currentMatchIndex      NOTIFY currentMatchChanged)
  Q_PROPERTY(bool    isActive           READ isActive               NOTIFY activeChanged)

  Q_INVOKABLE void activate();
  Q_INVOKABLE void deactivate();
  Q_INVOKABLE void findNext();
  Q_INVOKABLE void findPrevious();

Q_SIGNALS:
  // Emitted by findNext/findPrevious only — never by setNeedle.
  void navigationRequested(Markoff::FindController::Match match);
};
```

Both leaves expose symmetric attach hooks:
- `Markoff::Live::EditorWidget::attachFindController(FindController*)` / `detachFindController()`
- `Markoff::Live::LiveListModelBinding::attachFindController(FindController*)` / `detachFindController()`
- `Markoff::Source::Editor::attachFindController(FindController*)` / `detachFindController()`

The Markoff side is complete. Corbomite needs to build the UI and wire it.

## Design

### Components

| Component | Location | Role |
|---|---|---|
| `Corbomite::FindBar` | `src/editor/FindBar.{h,cpp}` (NEW) | Horizontal QWidget hosting line edit + buttons. Binds to a `Markoff::FindController*` via `setController`. |
| `Corbomite::NoteDocument::findController()` | `libs/core/include/corbomite/core/NoteDocument.h` + `libs/core/src/NoteDocument.cpp` (MODIFIED) | Lazy accessor — constructs one `Markoff::FindController` per NoteDocument on first call, bound to `markoff()`. |
| `Corbomite::NoteEditorWidget` | `src/editor/NoteEditorWidget.{h,cpp}` (MODIFIED) | Owns one FindBar instance docked at the bottom of its layout. Public `showFindBar()`/`hideFindBar()`. Attaches/detaches the controller to/from the active leaf on leaf swap. |
| `Corbomite::MainWindow` find actions | `src/app/MainWindow.cpp` (MODIFIED) | Restores `KStandardAction::find` (Ctrl+F) to route to the active NoteEditorWidget; `FindNext`/`FindPrevious` dispatch to the active controller. Currently stubbed since the port. |

### Widget layout

Okular-inspired horizontal bar, KDE-conventional widget order:

```
┌─ FindBar ──────────────────────────────────────────────────┐
│ [✕] [Find:] [needle               ⊗ ] [ 3 of 8 ] [▲] [▼] │
└────────────────────────────────────────────────────────────┘
```

- **Close button** — `QToolButton` (auto-raised), icon `dialog-close`. Triggers `controller.deactivate()` + bar hide.
- **Label** — `Find:` with buddy bound to the line edit (Alt+F mnemonic).
- **Line edit** — `QLineEdit` with `setClearButtonEnabled(true)`.
- **Count label** — `QLabel`. Text policy below.
- **Previous** — `QPushButton`, icon `go-up-search`, tooltip "Previous match (Shift+F3)".
- **Next** — `QPushButton`, icon `go-down-search`, tooltip "Next match (F3)".

Layout: `QHBoxLayout` with `contentsMargins(6,2,6,2)`, spacing `4`. The bar is `QFrame` with `StyledPanel + Sunken` so it visually demarcates from the editor area above it.

### Search behavior

**Search-as-you-type.** Every line-edit `textChanged(QString)` → `controller->setNeedle(text)`. The controller recomputes matches and emits `matchesChanged` + `currentMatchChanged`. Leaves render highlights live.

**No scroll on typing.** `setNeedle` deliberately doesn't emit `navigationRequested`. The user sees highlights appear but the document doesn't scroll until they press Return.

**Return navigation.** Line edit Return → `controller->findNext()` → emits `navigationRequested(Match)`. The active leaf scrolls the match into view and places a non-focusing caret per the find-session-scope spec (D3). Focus stays in the line edit.

**Shift+Return** → `controller->findPrevious()`.

**Esc** → `controller->deactivate()` + hide bar + return focus to the active editor leaf.

### Count label policy

| State | Label text |
|---|---|
| Needle empty | `""` (blank) |
| Needle non-empty, `matchCount == 0` | `"No matches"` |
| `matchCount > 0` | `"{currentMatchIndex+1} of {matchCount}"` |

No coloring on the line edit (markdown editor convention; we don't want the line edit to flash red). Count label color is `palette().text()` throughout — even "No matches" stays neutral. A future spec can revisit if dogfood asks for stronger feedback.

### Button enable state

- Next, Previous: enabled iff `matchCount > 0`.
- Close: always enabled.

### FindBar API

```cpp
class FindBar : public QFrame {
    Q_OBJECT
public:
    explicit FindBar(QWidget *parent = nullptr);
    ~FindBar() override;

    // Bind to a controller. Passing nullptr detaches. Safe to call repeatedly.
    void setController(Markoff::FindController *controller);
    Markoff::FindController *controller() const;

    // Focus the line edit. Called by NoteEditorWidget::showFindBar.
    void focusLineEdit();

Q_SIGNALS:
    void closeRequested();   // Esc or close button clicked. Host handles deactivate + hide.

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;  // intercept Esc / Return / Shift+Return on the line edit

private:
    void onNeedleChanged(const QString &);
    void onMatchesChanged();
    void onCurrentMatchChanged();
    void refreshCountLabel();
    void refreshButtonEnableState();

    QLineEdit       *m_lineEdit     = nullptr;
    QLabel          *m_countLabel   = nullptr;
    QPushButton     *m_prevButton   = nullptr;
    QPushButton     *m_nextButton   = nullptr;
    QToolButton     *m_closeButton  = nullptr;
    Markoff::FindController *m_controller = nullptr;
    bool m_applyingControllerNeedle = false;  // suppress textChanged → setNeedle echo
};
```

### NoteEditorWidget wiring

- Add `FindBar *m_findBar` to the widget. Construct in the ctor; insert into the existing `QVBoxLayout` after the `m_stack` (the view-mode stack). Initial `m_findBar->hide()`.
- Connect `m_findBar->closeRequested()` → `hideFindBar()` lambda.
- New methods:
  ```cpp
  void NoteEditorWidget::showFindBar();
  void NoteEditorWidget::hideFindBar();
  ```
- `showFindBar()` flow:
  1. If `m_doc == nullptr`, no-op (nothing to search).
  2. `auto *fc = m_doc->findController();`
  3. `m_findBar->setController(fc);`
  4. If active leaf is set, `activeLeaf()->attachFindController(fc);`
  5. `fc->activate();`
  6. `m_findBar->show(); m_findBar->focusLineEdit();`
- `hideFindBar()` flow:
  1. If `m_doc && activeLeaf()` then `activeLeaf()->detachFindController();`
  2. If `m_doc` then `m_doc->findController()->deactivate();`
  3. `m_findBar->hide();`
  4. `activeLeaf()` regains focus.

- **View-mode swap** (in `setViewMode`): if the find bar is currently visible, detach from outgoing leaf, attach to incoming leaf after `ensureWidgetConstructed`. The controller's needle and match list survive the swap; the user sees the same matches re-highlighted in the new leaf.

### NoteDocument additions

```cpp
class NoteDocument {
    // ... existing API ...
public:
    Markoff::FindController *findController();   // lazy; non-const because lazy ctor

private:
    Markoff::FindController *m_findController = nullptr;  // owned via QObject parent (this)
};
```

Implementation:
```cpp
Markoff::FindController *NoteDocument::findController()
{
    if (!m_findController) {
        m_findController = new Markoff::FindController(markoff(), this);
    }
    return m_findController;
}
```

Lazy: many notes never get searched. `m_findController` parented to NoteDocument so destruction cascades.

### MainWindow action restoration

Currently the editor-action registration block is wholly disabled (commit `76bcf7b3`). For this port we re-enable only three: `Find`, `FindNext`, `FindPrevious`.

```cpp
KStandardAction::find(this, &MainWindow::onFind, ac);
auto *findNextAct = KStandardAction::findNext(this, &MainWindow::onFindNext, ac);
auto *findPrevAct = KStandardAction::findPrev(this, &MainWindow::onFindPrev, ac);
```

Handlers:
```cpp
void MainWindow::onFind()    { if (auto *w = activeNoteEditor()) w->showFindBar(); }
void MainWindow::onFindNext(){ if (auto *fc = activeFindController()) fc->findNext(); }
void MainWindow::onFindPrev(){ if (auto *fc = activeFindController()) fc->findPrevious(); }
```

`activeFindController()` is a small helper that walks `activeNoteEditor() -> document() -> findController()`. Safe-null at every step.

The remaining action-registration block stays stubbed for this commit; subsequent feature ports re-enable their own slices.

## Out of scope

Deferred to follow-up ports. Listed here so reviewers don't ask:

- **Replace UI** (Ctrl+H). Needs a `ReplaceController` that Markoff doesn't expose yet. Separate spec.
- **Search flag toggles** (case-sensitive, whole-word, regex). `FindController::setFlags` already accepts them; UI is the work. Defer until dogfood asks.
- **Persistent search history** dropdown. Upgrade `KLineEdit` → `KHistoryComboBox` later.
- **Wrap-around indicator**. The controller wraps silently for MVP; a transient "wrapped" toast is a follow-up if dogfood asks.
- **Vault-wide search**. Separate concern; lives in the search plugin (`corbomite-search`), not in the editor.
- **"Highlight all" toggle**. Today all matches highlight; toggle UI is future polish.

## Tests

New target: `tests/editor/tst_findbar.cpp`.

Construct a real `Markoff::FindController` against a small `Markoff::MarkoffDocument` populated with known text. Then:

1. **`unbound_safe`** — Construct FindBar without setController. Verify no crash, count label blank, buttons disabled.
2. **`textChanged_updatesController`** — setController + type into line edit → `controller.needle()` equals typed text.
3. **`matchesChanged_updatesCountLabel`** — Type a needle that matches three known occurrences; assert count label reads `"1 of 3"`.
4. **`noMatch_showsNoMatches`** — Type a needle absent from the doc; assert count label reads `"No matches"`.
5. **`returnKey_callsFindNext`** — Type needle; QSignalSpy on `controller.navigationRequested`; QTest::keyClick(lineEdit, Qt::Key_Return); spy.count() == 1.
6. **`shiftReturn_callsFindPrev`** — Same as above with Shift modifier; assert `currentMatchIndex` moved backwards.
7. **`prevNextButton_disabledWhenNoMatches`** — Empty needle and no-match needle → both buttons `isEnabled() == false`.
8. **`escapeKey_emitsCloseRequested`** — QSignalSpy on `closeRequested`; QTest::keyClick(lineEdit, Qt::Key_Escape); spy.count() == 1.

Integration tests (NoteEditorWidget showFindBar / hideFindBar / leaf-swap rewire) deferred to a follow-up. The unit tests cover the widget contract; the integration is exercised by manual dogfood.

## Build + linkage

- `FindBar` lives in the existing `Corbomite` target alongside other `src/editor/` files.
- Depends on: Qt6::Widgets. The line edit is a plain `QLineEdit` with `setClearButtonEnabled(true)` (Qt 5.2+ built-in). No KF6::Completion dep needed for MVP; upgrading to `KHistoryComboBox` is a post-MVP feature.
- No new submodule or external dep.
- `Markoff::FindController` is in `markoff_core` which Corbomite already links.

## Acceptance criteria

1. Ctrl+F in an open note opens the FindBar at the bottom of the editor; line edit is focused.
2. Typing into the line edit highlights matches in Live mode without moving the document cursor.
3. Return navigates to the next match (scrolls into view if off-screen); focus stays in the line edit.
4. Shift+Return navigates to the previous match.
5. The count label shows `"N of M"` while matches exist; `"No matches"` when there are none.
6. Next/Previous buttons disable when match count is zero.
7. Esc closes the bar, calls `deactivate()`, and returns focus to the editor.
8. Switching from Live to Source while the bar is open preserves the needle; matches re-highlight in the new leaf (modulo any Source-side highlight rendering gaps — those are Markoff-side, not in this port).
9. Closing the document closes the bar and destroys the FindController.

## Risks and gaps

- **Sidebar regression unrelated to this port** may make the dogfood pass awkward (mentioned in the port-first recap). Doesn't block the find work but the user may want to triage that separately.
- **Source-mode highlight rendering**: depends on `Markoff::Source::Editor::attachFindController` actually rendering visible match marks. If it doesn't, the count works but the user can't see where matches are. That's a Markoff-side gap discovered if it surfaces; tracked as a port follow-up.
- **No regression test for view-mode swap with bar open**. Manual dogfood case.

## References

- Port-first session recap: `libs/markoff-family/docs/handoff/2026-05-20-port-first-session-recap.md`
- Markoff find spec: `libs/markoff-family/docs/specs/2026-05-20-find-session-scope-design.md`
- FindController header: `libs/markoff-family/libs/markoff-core/include/markoff/core/FindController.h`
- Master-side find dispatch (for diff context): pre-port `src/app/MainWindow.cpp` `onFind` slot
- UX research basis: Kate (`ktexteditor/src/search/katesearchbar.{h,cpp}`) + Okular (`okular/part/findbar.{h,cpp}` + `searchlineedit.{h,cpp}`)
