# HoverPopover — Mod-key pinning + grace-timer dismissal

**Punch-list reference:** P2 #4, sub-items 1 (Mod-key pinning) + 4 (`elementFromPoint`-style poll). Sub-items 2 (child-popover chains) and 3 (anchor-to-mouse for tall targets) are explicitly **out of scope**, deferred as separate punch-list entries.

**Audit reference:** `docs/audit-2026-04-26/ui-bundle.md` §HoverPopover, "Critical missing pieces" #1 + #3.

**Files touched:** `src/editor/HoverPopover.{h,cpp}`, `src/editor/NoteEditorWidget.cpp` (one signal-handler line), `tests/editor/tst_hover_popover_pinning.cpp` (new).

## Background

The current `HoverPopover` (170 + 82 LOC) is a single-instance per-editor frameless `Qt::ToolTip` window. `NoteEditorWidget::linkHovered(target, globalPos)` schedules it via a 300ms delay timer; `linkHovered("")` calls `cancel()` immediately. Dismissal sources today:

- `cancel()` from `linkHovered("")`
- `leaveEvent` of the popover frame
- Esc keypress while popover has focus

Two consequences:

1. The popover is purely "view-only" — once visible, the user has no way to interact with it (scroll long content, click inline links to navigate). Any cursor motion off the source link tears it down.
2. There is no gap-tolerance window: moving the cursor from link → popover crosses non-popover, non-link pixels (the popover anchors at `globalPos + (0, 20)`); if `linkHovered("")` fires during that traverse, the popover hides before the user reaches it. In practice the source-link tracking inside Markoff is generous enough that this rarely bites, but it is the classic "pixel race" UX bug.

## Design

### State machine

Replace the implicit state-via-`m_pendingTarget` checks with an explicit enum:

```cpp
enum class State {
    Hidden,       // not visible, no pending show
    Pending,      // 300ms delay timer running, will show on timeout
    Visible,      // shown, auto-dismissable (grace timer may be running)
    Pinned,       // shown, sticky — only Esc / click-outside / replacement dismisses
};
```

Transitions (`gT` = grace timer, 500 ms one-shot; `dT` = delay timer, 300 ms one-shot):

| From | Event | To | Side effects |
|---|---|---|---|
| Hidden | `scheduleShow(t,a)` | Pending | start `dT` |
| Pending | `dT` fires | Visible | render + show + install app event filter |
| Pending | `scheduleShow(t',a')` | Pending | restart `dT` with new (t', a') |
| Pending | `linkHoverEnded()` / `cancel()` | Hidden | stop `dT` |
| Visible | Ctrl-press (via app filter) | Pinned | apply visual accent, stop `gT` if running |
| Visible | `linkHoverEnded()` | Visible | start `gT` |
| Visible (`gT` running) | `gT` fires | depends | hide iff `widgetAt(QCursor::pos())` is neither popover nor descendant; else stay Visible |
| Visible (`gT` running) | `linkHovered(non-empty)` for current target | Visible | stop `gT` (cursor returned to link) |
| Visible (`gT` running) | cursor enters popover (enterEvent) | Visible | stop `gT` |
| Visible (`gT` running) | popover `leaveEvent` | Visible | start `gT` (re-arm) |
| any visible | `scheduleShow(new target)` | Pending | hide current + restart `dT` (Q3 = A: replacement wins, even when pinned) |
| Pinned | Esc | Hidden | unpin + hide |
| Pinned | mouse-press outside popover (via app filter) | Hidden | unpin + hide |
| any | `cancel()` | Hidden | hard reset (used by vault close) |

### Public API additions

```cpp
class HoverPopover : public QFrame {
    // existing methods unchanged...

    // Soft hint that the source-link hover ended. In Visible state starts
    // the 500 ms grace timer instead of dismissing immediately. In Pinned
    // state this is a no-op. In Pending state behaves as cancel(). NEW.
    void linkHoverEnded();

    // Test hooks.
    enum class State { Hidden, Pending, Visible, Pinned };  // NEW
    State stateForTest() const;                              // NEW
    bool isPinned() const { return m_state == State::Pinned; }
};
```

`cancel()` keeps its existing "hard reset" semantics (used by `MainWindow::onVaultClosed` and Esc handling). The Markoff hover-end signal handler in `NoteEditorWidget.cpp:60` switches from `cancel()` to `linkHoverEnded()`.

### Pinning trigger detection

`HoverPopover` installs an app-level event filter on `qApp` when entering Visible state and removes it on Hidden. The filter watches:

- `QEvent::KeyPress` with `key() == Qt::Key_Control` → if state is Visible, transition to Pinned. (Linux convention; Mac would map to `Qt::Key_Meta` — out of scope, single-platform decision.)
- `QEvent::MouseButtonPress` while state is Pinned → if the global press position is not inside the popover's `frameGeometry()`, transition to Hidden (unpin + hide).

The app-level filter is the right scope because the popover itself doesn't accept focus (`Qt::ToolTip + WA_ShowWithoutActivating + Qt::NoFocus`), so a `keyPressEvent` would only fire if the popover received focus, which it doesn't.

### Grace timer

A `QTimer m_graceTimer` with `singleShot = true`, `interval = 500ms` (matches the audit's cited Obsidian poll). The timer is started in two places (Visible state):

1. `linkHoverEnded()` — cursor left the source link.
2. Popover `leaveEvent` — cursor left the popover frame.

It is stopped (without firing) in two places:

1. `scheduleShow()` re-fires for the same target — cursor returned to link.
2. Popover `enterEvent` — cursor entered the popover frame.

On fire, the slot calls `QApplication::widgetAt(QCursor::pos())` and walks up the parent chain to test whether the result is the popover or one of its descendants. If yes, the popover stays visible (re-arm not needed; `enterEvent` already stopped the timer in the typical path; this is the safety net for synthetic-event cases where `enterEvent` didn't fire). If no, transition to Hidden.

### Visual cue for Pinned

A `QSS`-style stylesheet swap — when entering Pinned, set:

```cpp
setStyleSheet(QStringLiteral("QFrame { border: 2px solid %1; }")
                  .arg(palette().highlight().color().name()));
```

When leaving Pinned (back to Hidden), clear the stylesheet. This is purely visual feedback that the popover is "grabbed". No icon overlay.

### Replacement-wins semantics (Q3 = A)

`scheduleShow(target, anchor)` always wins. If the current state is Visible or Pinned and the new `target` differs from the currently-shown target, the popover hides (any Pinned border accent clears) and re-enters Pending with the new target. This preserves the single-popover invariant — child-chains are P2 #4 sub-item 2, deferred.

Same-target re-fire from `scheduleShow` (same string, same anchor) during Visible state is a "cursor returned to link" hint: stop `gT` if running, do not re-show or re-render.

## Out of scope

- **Sub-item 2 (child-popover chains)** — requires a popover registry + parent/child lifecycle. Stays in P2 #4 with a sub-bullet.
- **Sub-item 3 (anchor-to-mouse for tall targets)** — needs a Markoff API extension to expose the source-link rect (`hoveredLinkRect()`); today only a global point is plumbed through. Stays in P2 #4 with a sub-bullet.
- **Hover-link source registry consultation** (audit §HoverPopover #5) — orthogonal; tracked separately.
- **`PopoverState` enum** as named in the audit — `State` here is similar but locally scoped; no need to expose as a Markoff or Corbomite::Core type.

## Testing

`tests/editor/tst_hover_popover_pinning.cpp` (new), QTEST_MAIN, headless via `QT_QPA_PLATFORM=offscreen`. Cases:

1. **`pinTransitionsOnCtrlPress`** — schedule + advance past 300ms, verify state == Visible. Post a `Qt::Key_Control` `KeyPress` to `qApp` via `QTest::keyEvent`. Verify state == Pinned. Verify popover still visible.
2. **`escUnpins`** — pin (steps 1), post Esc to popover. Verify state == Hidden, not visible.
3. **`outsideClickUnpins`** — pin, post `MouseButtonPress` at a point outside the popover's `frameGeometry`. Verify state == Hidden.
4. **`graceTimerKeepsOpenIfCursorOverPopover`** — show, call `linkHoverEnded()` while cursor is positioned over the popover (use `QTest::mouseMove` to move into popover, or directly invoke `enterEvent`). Wait > 500ms. Verify state == Visible (still).
5. **`graceTimerHidesIfCursorElsewhere`** — show, call `linkHoverEnded()` while cursor is at `QPoint(0,0)` (well outside). Wait > 500ms + slack. Verify state == Hidden.
6. **`replacementWinsOverPinned`** — pin, then `scheduleShow("OtherTarget", anchor)`. Verify state == Pending (or Visible after delay), not Pinned, and target updated.
7. **`pinningPersistsAcrossLinkHoverEnded`** — pin, then call `linkHoverEnded()`. Wait > 500ms. Verify state == Pinned (still).

Cursor-position assertions need `QCursor::setPos()` calls; under offscreen platform this works. The existing `tst_hover_popover_render.cpp` runs under offscreen so the test infrastructure pattern is proven.

## Risk + open questions

- **Application-level event filter installed/removed dynamically.** Risk: forgetting to remove it on Hidden leaks a filter that fires for every key/mouse event in the app. Mitigated by `installAppFilter()` / `removeAppFilter()` helpers called from a single state-transition function (`enterState(State)`), with a `Q_ASSERT` guard.
- **Mouse-press-outside detection during Pinned.** A click on another window of the same app correctly unpins. A click on a different application's window does NOT generate a `QEvent::MouseButtonPress` in our filter; the popover would stay Pinned. Acceptable — Esc still works, and most users won't notice (the popover loses focus visually via Qt's own focus-out handling on most window managers, even though our state stays Pinned). Documented limitation; not a blocker.
- **`QApplication::widgetAt`** uses the global widget hierarchy. Under offscreen platform, returns valid widgets. Under some Wayland configurations historically returned `nullptr` for cross-window queries; we read it on our own popover's geometry containment, so this is safe.
