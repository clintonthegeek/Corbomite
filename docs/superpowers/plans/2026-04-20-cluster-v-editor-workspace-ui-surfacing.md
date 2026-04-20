# Cluster V — Editor & Workspace UI Surfacing — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.
>
> **Living-status note:** Live status (Not started / In progress / Done / Blocked) is in [`docs/PROJECT-STATE.md`](../../PROJECT-STATE.md) Roadmap. Update PROJECT-STATE per the rituals in [`docs/CONTRIBUTING-OPS.md`](../../CONTRIBUTING-OPS.md), not this file.

**Goal:** Ship the six *surface-first* phases from the Cluster V spec: wire up ~30 built-but-unreachable features (dead app-shell actions, full Markoff editor menu tree, ReadingView interactions, Workspace split/popout/linked-pane, Search UI toggles, Notice-based error surfacing) so they become user-reachable via menus, shortcuts, and dialogs.

**Architecture:** Menu entries retrieved from existing action registries where possible (`Markoff::Editor::action(ActionId)`), wired into `corbomiteui.rc.in` through a new action-definition pass in `MainWindow::setupActions`. A new `View::zoomIn/zoomOut/zoomReset` base-virtual contract lets the app-shell zoom actions route to whichever view is active. A single `MainWindow::onSettingsApplied()` slot bound to `CorbomiteSettings::configChanged` serves as the choke point for settings-driven appliers (theme now, autosave-delay and others in V.2). Six new `Markoff::ActionId::SetHeading1..6` enum values + Ctrl+1-6 shortcuts power a direct-select Heading menu. `KColorSchemeManager` applies the Appearance/Theme kcfg key. ReadingView grows a click-handler for heading fold arrows and a `linkHovered` signal that feeds `HoverPopover`. Workspace power-features (already implemented in Cluster G) get their first UI-exposed invocations. Search bar gains regex/match-case toggle buttons; 5 previously-swallowed error sites get `Notice` toasts.

**Tech Stack:** C++20, Qt6 (`QAction`, `QActionGroup`, `QMenu`, `QDialog`, `QComboBox`, `QSpinBox`, `QCheckBox`, `QGraphicsScene`, `QMetaObject::invokeMethod`), KDE Frameworks 6 (`KStandardAction`, `KAboutApplicationDialog`, `KAboutKdeDialog`, `KColorSchemeManager`, `KConfigSkeleton::configChanged`, `KXMLGUI`), existing `libs/markoff-family/` action registry, existing `libs/readingview/` + `libs/core/Workspace` + `libs/models/TabModel` primitives, existing `src/dialogs/Notice` stacking widget.

**Spec:** [`docs/superpowers/specs/2026-04-20-cluster-v-editor-workspace-ui-surfacing-design.md`](../specs/2026-04-20-cluster-v-editor-workspace-ui-surfacing-design.md)

**V.2 handoff:** [`docs/superpowers/plans/2026-04-20-cluster-v2-debt-cleanup-SCOUTING.md`](2026-04-20-cluster-v2-debt-cleanup-SCOUTING.md)

---

## File Structure

### New files

| Path | Responsibility |
|---|---|
| `src/dialogs/CalloutPickerDialog.{h,cpp}` | Modal with `QComboBox` of 26 Obsidian callout types + live preview label. |
| `src/dialogs/InsertTableDialog.{h,cpp}` | Rows/cols `QSpinBox`es + "first row as header" `QCheckBox`. |
| `tests/core/tst_view_zoom.cpp` | Base-virtual dispatch + no-op default. |
| `tests/markoff/tst_editor_cursor_in_table.cpp` | 4 fixtures for cursor-in-table accessor. |
| `tests/markoff/tst_set_heading_actions.cpp` | SetHeading1..6 action triggers yield correct heading levels. |
| `tests/app/tst_mainwindow_action_wiring.cpp` | Introspect `actionCollection()` — every new action added in P1/P2/P3/P5 is non-null with a shortcut bound. |
| `tests/app/tst_theme_applier.cpp` | Dark / light / system → `QApplication::palette()` reflects the switch. |
| `tests/app/tst_editor_toggle_mode.cpp` | Ctrl+E cycles Source → LivePreview → Reading → Source. |
| `tests/readingview/tst_click_to_fold.cpp` | Synth mouse press on fold-arrow → `foldedHeadings` updates. |
| `tests/readingview/tst_link_hovered_signal.cpp` | `linkHovered` re-emits on wiki-link hover. |
| `tests/search/tst_search_options_regex.cpp` | Regex toggle → `ParseOptions.regex = true` → `CompiledPlan.regexPatterns` populated. |
| `docs/cluster-retros/cluster-v.md` | Retro at cluster close. |

### Modified files

| Path | What changes |
|---|---|
| `CMakeLists.txt` | Add `find_package(KF6ColorScheme REQUIRED)`. |
| `src/app/CMakeLists.txt` | Link `KF6::ColorScheme`; add dialog sources. |
| `src/app/MainWindow.{h,cpp}` | +~15 new actions; new slots (`cycleEditorMode`, `onSettingsApplied`, `applyTheme`, `refreshEditorActions`, `onSplitActive`, `onPopoutActive`, `onToggleLinkActive`, `onReopenClosed`, `onMoveTab`, `onZoomIn/Out/Reset`, `onFind`, `onFindNext`, `onFindPrevious`, `onReplace`, `onCycleHeading`). |
| `src/app/corbomiteui.rc.in` | Full menu tree per spec §3.4 (Edit / View / Format / Heading / Insert / Table). |
| `src/app/corbomite.kcfg` | No key changes (read-only review). |
| `src/app/ViewModeController.cpp` (if present) or `NoteEditorWidget.cpp` | Expose `cycleViewMode()` helper or have MainWindow read `viewMode()` for cycling. |
| `libs/core/include/corbomite/core/View.h` | Add `virtual void zoomIn()`, `zoomOut()`, `zoomReset()` with no-op defaults. |
| `libs/core/src/View.cpp` | Empty bodies for the three new virtuals. |
| `libs/core/include/corbomite/core/proxies/WorkspaceController.h` + `.cpp` | Optional convenience wrappers (`splitActive`, `popoutActive`, `toggleLinkActive`, `reopenClosed`). May inline in MainWindow if controller would only gain 4 thin methods. |
| `libs/markoff-family/libs/markoff/include/markoff/ActionId.h` | Add `SetHeading1`…`SetHeading6` values. |
| `libs/markoff-family/libs/markoff/include/markoff/Editor.h` | Declare `cursorInTable() const`, `resetZoom()`. |
| `libs/markoff-family/libs/markoff/src/Editor.cpp` | Implement the two accessors + register SetHeading1..6 actions in `createActions()` with Ctrl+1..6 shortcuts. |
| `src/editor/MarkdownView.{h,cpp}` | Override `zoomIn/Out/Reset` → forward to `Markoff::Editor` actions + new `resetZoom()`. |
| `src/editor/SourceEditor.{h,cpp}` (in `libs/editor` or `libs/qutepart-corbomite`) | Add `zoomIn/zoomOut/resetZoom` methods if missing; MarkdownView companion `SourceEditorView` override forwards. |
| `src/editor/SourceEditorView.{h,cpp}` (find actual filename) | Override zoom virtuals. |
| `libs/readingview/include/corbomite/readingview/ReadingView.h` | Add `linkHovered(QString)` signal (adapter around existing `wikiLinkHovered`); declare new protected `mousePressEvent` (or amend existing one). |
| `libs/readingview/src/ReadingView.cpp` | Emit `linkHovered` from `wikiLinkHovered` path; hit-test fold-arrow in `mousePressEvent` and call `toggleFold(sectionIdx)`. Override `zoomIn/Out/Reset`. |
| `libs/readingview/src/SectionLayout.cpp` | Replace direct JKQTMathText render at line ~903 with `codeBlockProcessorRegistry` dispatch. |
| `src/editor/NoteEditorWidget.{h,cpp}` | Mirror Markoff linkHovered wiring for ReadingView path; expose `viewMode()` accessor if MainWindow cycle helper needs it. |
| `src/vault/FileManager.cpp` (actual path: `libs/vault/src/FileManager.cpp` — confirm) | Read `Files/TrashOption` + `Files/PromptDelete` kcfg keys in `promptForDeletion`; skip modal when PromptDelete=false; use `QFile::remove` when TrashOption="permanent"/"DontUseTrash". |
| `src/plugins/search/SearchView.cpp` (or SearchBar UI file) | Add regex/match-case toggle buttons; wire to `SearchDSL::ParseOptions`; remove "coming soon" tooltip text. |
| `src/dialogs/Notice.{h,cpp}` | No code changes; ensure `Qt::QueuedConnection` factory helper exists (add thin `Notice::post(QString, QWidget*)` if not). |
| 5 toast-site source files (Phase 6) | Add `Notice::post(...)` on failure paths. |
| `docs/PROJECT-STATE.md` | Marks Cluster V done + recent-decisions entry + Last-updated. |
| `docs/superpowers/plans/INDEX.md` | Updates V row to "Done"; leaves V.2 at "Scouting doc". |
| `docs/backlog.md` | Strikes Cluster V entry; leaves V.2. |

### Deleted by this plan

| Path | Reason |
|---|---|
| Empty lambda bodies for `KStandardAction::find/aboutApp/aboutKDE` in `MainWindow::setupActions()` | Replaced with real slot connections. |

---

## Phase Overview

Six phases; P1 first, P2 (+absorbed P3) second, P4/P5/P6 parallelisable, P7 closeout.

1. **Phase 1 — Dead app-shell actions.** `edit_find`/`view_zoom_*`/`aboutApp`/`aboutKDE` connected; `editor_toggle_mode` defined with Ctrl+E; `View::zoomIn/Out/Reset` base virtuals added; theme applier via `KColorSchemeManager` on `CorbomiteSettings::configChanged`; `FileManager::promptForDeletion` honours `TrashOption`+`PromptDelete`.
2. **Phase 2+3 — Markoff menu surfacing + fold actions.** New `ActionId::SetHeading1..6` + Ctrl+1..6; `cursorInTable` + `resetZoom` accessors; `CalloutPickerDialog` + `InsertTableDialog`; full Edit/View/Format/Heading/Insert/Table tree in `corbomiteui.rc.in`; `refreshEditorActions` enable-state glue on `Workspace::activeLeafChanged`; Fold All / Unfold All / Toggle Fold pulled from existing ActionIds.
3. **Phase 4 — ReadingView interactions.** `linkHovered` signal; `HoverPopover` wired for ReadingView in `NoteEditorWidget`; click-to-fold on HeadingItem via `mousePressEvent` hit-test; `codeBlockProcessorRegistry` routing at `SectionLayout.cpp:903`; ReadingView zoom virtuals.
4. **Phase 5 — Workspace power-features.** Actions: Split right (Ctrl+\), Split down (Ctrl+Shift+\), Move to new window, Link with active pane (checkable), Reopen closed tab (Ctrl+Shift+T), Move tab left/right (Ctrl+Shift+PgUp/PgDn).
5. **Phase 6 — Search UI + toasts.** Regex + Match-case toggle buttons in `SearchBar`; strip "coming soon" tooltip; add `Notice::post()` helper and call at 5 identified failure sites.
6. **Phase 7 — Closeout.** Retro + PROJECT-STATE + INDEX + backlog.

Estimated effort: **~5-7 days** total per spec §10.

---

# Phase 1 — Dead App-Shell Actions

Goal: after Phase 1, the KXMLGUI startup warning about `editor_toggle_mode` is gone; Find/Zoom/About all work; Theme + Trash settings actually take effect.

## Task 1.1: Add `KF6::ColorScheme` to CMake

**Files:**
- Modify: `CMakeLists.txt`
- Modify: `src/app/CMakeLists.txt`

- [ ] **Step 1: Add find_package**

Add at `CMakeLists.txt` next to the other `find_package(KF6*)` lines (~line 38):

```cmake
find_package(KF6ColorScheme REQUIRED)
```

- [ ] **Step 2: Link in src/app**

Find the `target_link_libraries(corbomite PRIVATE ...)` call in `src/app/CMakeLists.txt`; append `KF6::ColorScheme` to the list (keep alphabetical).

- [ ] **Step 3: Verify build**

Run: `cmake -B build -DCORBOMITE_DEV_BUILD=ON && cmake --build build -j 10 --target corbomite`
Expected: builds clean.

- [ ] **Step 4: Commit**

```bash
git add CMakeLists.txt src/app/CMakeLists.txt
git commit -m "build: link KF6::ColorScheme for theme applier (cluster v phase 1)"
```

## Task 1.2: Add `View::zoomIn/Out/Reset` base virtuals

**Files:**
- Modify: `libs/core/include/corbomite/core/View.h`
- Modify: `libs/core/src/View.cpp`
- Create: `tests/core/tst_view_zoom.cpp`
- Modify: `tests/core/CMakeLists.txt`

- [ ] **Step 1: Write failing test**

Create `tests/core/tst_view_zoom.cpp`:

```cpp
#include <QtTest/QtTest>
#include <corbomite/core/View.h>

class DummyView : public Corbomite::View {
public:
    DummyView() : View(nullptr, nullptr) {}
    QString getViewType() const override { return "dummy"; }
    QString getDisplayText() const override { return "Dummy"; }
    int zoomIns = 0, zoomOuts = 0, resets = 0;
    void zoomIn() override { ++zoomIns; }
    void zoomOut() override { ++zoomOuts; }
    void zoomReset() override { ++resets; }
};

class TstViewZoom : public QObject {
    Q_OBJECT
private slots:
    void defaultIsNoOp() {
        // Instantiate a raw base-class variant via a Subclass that does NOT override.
        struct Bare : public Corbomite::View {
            Bare() : View(nullptr, nullptr) {}
            QString getViewType() const override { return {}; }
            QString getDisplayText() const override { return {}; }
        };
        Bare v;
        v.zoomIn(); v.zoomOut(); v.zoomReset(); // must not crash
    }
    void overridesDispatch() {
        DummyView d;
        Corbomite::View *v = &d;
        v->zoomIn(); v->zoomIn(); v->zoomOut(); v->zoomReset();
        QCOMPARE(d.zoomIns, 2);
        QCOMPARE(d.zoomOuts, 1);
        QCOMPARE(d.resets, 1);
    }
};
QTEST_MAIN(TstViewZoom)
#include "tst_view_zoom.moc"
```

Append to `tests/core/CMakeLists.txt` matching the pattern of existing tests:

```cmake
corbomite_add_test(tst_view_zoom SOURCES tst_view_zoom.cpp LIBS Corbomite::Core)
```

- [ ] **Step 2: Run — expect fail**

```bash
cd build && ctest -R tst_view_zoom --output-on-failure
```
Expected: compile error — `zoomIn/zoomOut/zoomReset` not members of `View`.

- [ ] **Step 3: Add virtuals**

In `libs/core/include/corbomite/core/View.h` after the existing `onTabMenu`/`onResize` declarations (~line 58) add:

```cpp
    virtual void zoomIn();
    virtual void zoomOut();
    virtual void zoomReset();
```

In `libs/core/src/View.cpp` add empty bodies:

```cpp
void View::zoomIn() {}
void View::zoomOut() {}
void View::zoomReset() {}
```

- [ ] **Step 4: Run — expect pass**

```bash
cd build && ctest -R tst_view_zoom --output-on-failure
```

- [ ] **Step 5: Commit**

```bash
git add libs/core/include/corbomite/core/View.h libs/core/src/View.cpp tests/core/tst_view_zoom.cpp tests/core/CMakeLists.txt
git commit -m "feat(core): View::zoomIn/Out/Reset base virtuals with no-op defaults"
```

## Task 1.3: Implement `MarkdownView::zoomIn/Out/Reset`

**Files:**
- Modify: `src/editor/MarkdownView.h`, `src/editor/MarkdownView.cpp`
- Modify: `libs/markoff-family/libs/markoff/include/markoff/Editor.h`, `libs/markoff-family/libs/markoff/src/Editor.cpp`

- [ ] **Step 1: Declare `Editor::resetZoom()`**

In `libs/markoff-family/libs/markoff/include/markoff/Editor.h` near the existing font/zoom API, declare:

```cpp
void resetZoom();
```

- [ ] **Step 2: Implement**

In `Editor.cpp`, find where `setFontSize` is implemented; add:

```cpp
void Editor::resetZoom()
{
    constexpr int kDefaultFontSize = 14; // confirm with existing constant if one exists
    setFontSize(kDefaultFontSize);
}
```

If a `kDefaultFontSize` constant already exists in the header, use it verbatim. Otherwise look at `EditorSettings` default font size and mirror it.

- [ ] **Step 3: Override on MarkdownView**

In `src/editor/MarkdownView.h` declare:

```cpp
void zoomIn() override;
void zoomOut() override;
void zoomReset() override;
```

In `.cpp`:

```cpp
void MarkdownView::zoomIn() { m_editor->action(Markoff::ActionId::ZoomIn)->trigger(); }
void MarkdownView::zoomOut() { m_editor->action(Markoff::ActionId::ZoomOut)->trigger(); }
void MarkdownView::zoomReset() { m_editor->resetZoom(); }
```

- [ ] **Step 4: Run existing tests**

```bash
cd build && cmake --build . -j 10 && ctest -R markoff --output-on-failure
```

- [ ] **Step 5: Commit**

```bash
git add libs/markoff-family/libs/markoff/include/markoff/Editor.h libs/markoff-family/libs/markoff/src/Editor.cpp src/editor/MarkdownView.h src/editor/MarkdownView.cpp
git commit -m "feat(editor): MarkdownView zoom overrides; Editor::resetZoom()"
```

## Task 1.4: Implement `SourceEditorView::zoomIn/Out/Reset` and `ReadingView::zoomIn/Out/Reset`

**Files:**
- Modify: `src/editor/SourceEditorView.{h,cpp}` (find actual filename — grep `class SourceEditorView`)
- Modify: `libs/readingview/include/corbomite/readingview/ReadingView.h`, `libs/readingview/src/ReadingView.cpp`
- Possibly modify: `libs/editor/SourceEditor.{h,cpp}` or `libs/qutepart-corbomite/...` (wherever `Corbomite::SourceEditor` lives) if zoom methods absent

- [ ] **Step 1: Discover SourceEditor zoom surface**

Run: `grep -rn "setFontSize\|zoomIn" libs/editor/ libs/qutepart-corbomite/ 2>/dev/null | head`

- [ ] **Step 2: Add zoom methods to `Corbomite::SourceEditor` if missing**

Qutepart's underlying widget has font-size controls (QPlainTextEdit has `zoomIn(int)`/`zoomOut(int)`). Expose:

```cpp
// SourceEditor.h
void zoomIn();   // Ctrl+= equivalent
void zoomOut();
void resetZoom();
```

Implementation delegates to the embedded `Qutepart *m_edit`:

```cpp
void SourceEditor::zoomIn()  { m_edit->zoomIn(1); }
void SourceEditor::zoomOut() { m_edit->zoomOut(1); }
void SourceEditor::resetZoom() {
    // Reset to the font size captured at construction (store as m_defaultFont).
    m_edit->setFont(m_defaultFont);
}
```

Record `m_defaultFont = m_edit->font()` in `SourceEditor::SourceEditor(...)` ctor; add the member to `.h`.

- [ ] **Step 3: Override on `SourceEditorView`**

In the view class file:

```cpp
void SourceEditorView::zoomIn()    { m_editor->zoomIn(); }
void SourceEditorView::zoomOut()   { m_editor->zoomOut(); }
void SourceEditorView::zoomReset() { m_editor->resetZoom(); }
```

- [ ] **Step 4: Override on ReadingView**

In `ReadingView.h` declare:

```cpp
void zoomIn() override;
void zoomOut() override;
void zoomReset() override;
```

In `.cpp`, scale the embedded `QGraphicsView`'s transform:

```cpp
void ReadingView::zoomIn()    { scale(1.1, 1.1); m_userZoom *= 1.1; emit zoomChanged(); }
void ReadingView::zoomOut()   { scale(1.0 / 1.1, 1.0 / 1.1); m_userZoom /= 1.1; emit zoomChanged(); }
void ReadingView::zoomReset() {
    const double inv = 1.0 / m_userZoom;
    scale(inv, inv);
    m_userZoom = 1.0;
    emit zoomChanged();
}
```

Add `double m_userZoom = 1.0;` private member + `Q_SIGNAL void zoomChanged();`. If the existing class uses a different base (not `QGraphicsView`), adapt accordingly.

- [ ] **Step 5: Build + test**

```bash
cmake --build build -j 10 && cd build && ctest --output-on-failure -j 10
```

- [ ] **Step 6: Commit**

```bash
git add src/editor/SourceEditor* src/editor/SourceEditorView* libs/readingview/include/corbomite/readingview/ReadingView.h libs/readingview/src/ReadingView.cpp
git commit -m "feat(editor): SourceEditorView + ReadingView zoom overrides"
```

## Task 1.5: Wire `edit_find` / `view_zoom_*` / `aboutApp` / `aboutKDE` slots

**Files:**
- Modify: `src/app/MainWindow.h`, `src/app/MainWindow.cpp`

- [ ] **Step 1: Declare slots**

In `MainWindow.h` private-slots section (or equivalent):

```cpp
private Q_SLOTS:
    void onFind();
    void onZoomIn();
    void onZoomOut();
    void onZoomReset();
    void onAboutApp();
    void onAboutKde();
```

- [ ] **Step 2: Implement**

In `MainWindow.cpp`:

```cpp
#include <KAboutApplicationDialog>
#include <KAboutKdeDialog>
#include <KAboutData>
#include <corbomite/core/Workspace.h>
#include <corbomite/core/WorkspaceLeaf.h>
#include <corbomite/core/View.h>

static Corbomite::View *activeView(Corbomite::Workspace *ws)
{
    auto *leaf = ws ? ws->activeLeaf() : nullptr;
    return leaf ? leaf->view() : nullptr;
}

void MainWindow::onFind()
{
    if (auto *view = activeView(m_workspace)) {
        // Markoff editor exposes Find via action(Find); route through the view
        // so SourceEditorView / ReadingView can each implement their own Find.
        if (auto *mv = qobject_cast<MarkdownView*>(view)) {
            mv->editor()->action(Markoff::ActionId::Find)->trigger();
        }
        // SourceEditorView: deferred to Qutepart fork Phase 3; no-op for now.
        // ReadingView: no find path in scope.
    }
}

void MainWindow::onZoomIn()    { if (auto *v = activeView(m_workspace)) v->zoomIn(); }
void MainWindow::onZoomOut()   { if (auto *v = activeView(m_workspace)) v->zoomOut(); }
void MainWindow::onZoomReset() { if (auto *v = activeView(m_workspace)) v->zoomReset(); }

void MainWindow::onAboutApp()
{
    KAboutApplicationDialog dlg(KAboutData::applicationData(), this);
    dlg.exec();
}

void MainWindow::onAboutKde()
{
    KAboutKdeDialog dlg(this);
    dlg.exec();
}
```

- [ ] **Step 3: Replace empty lambdas**

In `MainWindow::setupActions()` (around line 784-809 per recon) replace:

- `KStandardAction::find(..., [this]() {})` → `KStandardAction::find(this, &MainWindow::onFind, actionCollection())`
- The unconnected `view_zoom_in/out/reset` actions: add `connect(act, &QAction::triggered, this, &MainWindow::onZoomInXxx)` for each. Also ensure shortcuts: `Ctrl+=`, `Ctrl+-`, `Ctrl+0`.
- `KStandardAction::aboutApp(..., []() {})` → `KStandardAction::aboutApp(this, &MainWindow::onAboutApp, actionCollection())`.
- `KStandardAction::aboutKDE(..., []() {})` → `KStandardAction::aboutKDE(this, &MainWindow::onAboutKde, actionCollection())`.

- [ ] **Step 4: Build + smoke**

```bash
cmake --build build -j 10
./build/Corbomite  # open a note, press Ctrl+F, Ctrl+=, Ctrl+-, Ctrl+0; check Help > About menus
```

- [ ] **Step 5: Commit**

```bash
git add src/app/MainWindow.h src/app/MainWindow.cpp
git commit -m "feat(app): wire Find, Zoom, About actions to real slots"
```

## Task 1.6: Define `editor_toggle_mode` + Ctrl+E cycle

**Files:**
- Modify: `src/app/MainWindow.h`, `src/app/MainWindow.cpp`
- Modify: `src/editor/NoteEditorWidget.h` (expose `viewMode()` if absent)
- Create: `tests/app/tst_editor_toggle_mode.cpp`
- Modify: `tests/app/CMakeLists.txt`

- [ ] **Step 1: Expose `viewMode()` if missing**

Check `NoteEditorWidget.h` for a read accessor; if only `setViewMode(ViewMode)` exists, add:

```cpp
ViewMode viewMode() const { return m_currentMode; }
```

- [ ] **Step 2: Declare slot**

```cpp
// MainWindow.h
private Q_SLOTS:
    void cycleEditorMode();
```

- [ ] **Step 3: Implement**

```cpp
// MainWindow.cpp
#include "editor/NoteEditorWidget.h"

void MainWindow::cycleEditorMode()
{
    auto *v = activeView(m_workspace);
    auto *mv = qobject_cast<MarkdownView*>(v);
    if (!mv) return;
    auto *w = mv->noteEditorWidget(); // add accessor if missing
    using VM = NoteEditorWidget::ViewMode;
    switch (w->viewMode()) {
        case VM::Source:      w->setViewMode(VM::LivePreview); break;
        case VM::LivePreview: w->setViewMode(VM::Reading); break;
        case VM::Reading:     w->setViewMode(VM::Source); break;
    }
}
```

- [ ] **Step 4: Register action in `setupActions`**

Inside the function, after the existing KStandardAction setup:

```cpp
{
    auto *act = actionCollection()->addAction(QStringLiteral("editor_toggle_mode"));
    act->setText(i18n("Toggle Editor Mode"));
    act->setIcon(QIcon::fromTheme(QStringLiteral("view-preview")));
    actionCollection()->setDefaultShortcut(act, QKeySequence(QStringLiteral("Ctrl+E")));
    connect(act, &QAction::triggered, this, &MainWindow::cycleEditorMode);
}
```

- [ ] **Step 5: Test**

`tests/app/tst_editor_toggle_mode.cpp`:

```cpp
#include <QtTest/QtTest>
// include MainWindow + test harness fixtures
class TstEditorToggleMode : public QObject {
    Q_OBJECT
private slots:
    void cyclesForward() {
        // Spawn a MainWindow + a MarkdownView with a fake note; assert the
        // action cycles Source → LivePreview → Reading → Source.
        // Follow the pattern used in tests/app/tst_mainwindow_* fixtures.
    }
};
QTEST_MAIN(TstEditorToggleMode)
#include "tst_editor_toggle_mode.moc"
```

Flesh out the fixture pattern per existing app tests; if no MainWindow test harness exists yet, defer the unit test to Task 2.10 and rely on manual smoke.

- [ ] **Step 6: Smoke**

Launch app, open note, press Ctrl+E three times, verify mode cycles visibly.

- [ ] **Step 7: Commit**

```bash
git add src/app/MainWindow.{h,cpp} src/editor/NoteEditorWidget.h tests/app/tst_editor_toggle_mode.cpp tests/app/CMakeLists.txt
git commit -m "feat(app): editor_toggle_mode action with Ctrl+E cycle"
```

## Task 1.7: Theme applier via `KColorSchemeManager`

**Files:**
- Modify: `src/app/MainWindow.h`, `src/app/MainWindow.cpp`
- Create: `tests/app/tst_theme_applier.cpp`
- Modify: `tests/app/CMakeLists.txt`

- [ ] **Step 1: Declare slot + helper**

```cpp
// MainWindow.h
private:
    void applyTheme();
private Q_SLOTS:
    void onSettingsApplied();
```

- [ ] **Step 2: Implement**

```cpp
// MainWindow.cpp
#include <KColorSchemeManager>
#include <KColorSchemeModel>
#include "CorbomiteSettings.h" // the kcfg-generated class

void MainWindow::applyTheme()
{
    auto *mgr = KColorSchemeManager::instance();
    const QString theme = CorbomiteSettings::self()->theme();
    if (theme == QLatin1String("system") || theme.isEmpty()) {
        mgr->activateScheme(QModelIndex()); // track OS
        return;
    }
    const QString schemeId = (theme == QLatin1String("dark"))
        ? QStringLiteral("BreezeDark")
        : QStringLiteral("BreezeLight");
    const QModelIndex idx = mgr->model()->indexForSchemeId(schemeId);
    if (idx.isValid()) mgr->activateScheme(idx);
}

void MainWindow::onSettingsApplied()
{
    applyTheme();
    // Future: autosave delay, etc.
}
```

In `MainWindow` ctor after settings load:

```cpp
applyTheme();
connect(CorbomiteSettings::self(), &KConfigSkeleton::configChanged,
        this, &MainWindow::onSettingsApplied);
```

- [ ] **Step 3: Test**

```cpp
// tests/app/tst_theme_applier.cpp
#include <QtTest/QtTest>
#include <QApplication>
#include <KColorSchemeManager>
// Construct a CorbomiteSettings instance, set theme to "dark", invoke applyTheme()
// via a test-friendly seam, assert KColorSchemeManager's activeScheme changed.
```

Minimum viable test: toggle setting value, call `emit configChanged()`, assert `qApp->palette()` swap. If `KColorSchemeManager` test harness is impractical, fall back to manual smoke for this step.

- [ ] **Step 4: Smoke**

Open Settings → Appearance → Theme → Dark → Apply. Window repaints in dark mode immediately; toggle back to Light, then System.

- [ ] **Step 5: Commit**

```bash
git add src/app/MainWindow.{h,cpp} tests/app/tst_theme_applier.cpp tests/app/CMakeLists.txt
git commit -m "feat(app): apply Appearance/Theme via KColorSchemeManager"
```

## Task 1.8: Respect `Files/TrashOption` + `Files/PromptDelete` in `FileManager`

**Files:**
- Modify: `libs/vault/src/FileManager.cpp`

- [ ] **Step 1: Read settings in `promptForDeletion`**

At the top of `FileManager::promptForDeletion(TAbstractFile *file, QWidget *parent)`:

```cpp
#include "CorbomiteSettings.h"
#include <QFile>

const QString trashOption = CorbomiteSettings::self()->trashOption();   // "system"|"trash"|"permanent"
const bool promptEnabled  = CorbomiteSettings::self()->promptDelete();

if (!promptEnabled) {
    // Skip modal; delete per trashOption directly.
    if (trashOption == QLatin1String("permanent")) {
        QFile::remove(file->absolutePath());
    } else {
        m_vault->trash()->trashFile(file); // existing VaultTrash path
    }
    return true;
}
```

Leave the existing DeleteConfirmDialog code below unchanged; after confirmation, branch on `trashOption` the same way. Factor the delete action into a lambda / helper to avoid duplication.

- [ ] **Step 2: Build**

```bash
cmake --build build -j 10
```

- [ ] **Step 3: Smoke**

Settings → Files → set PromptDelete=off, delete a file — no modal. Re-enable, set TrashOption=permanent, delete — modal warns about permanent; after OK, file is gone from filesystem (not in trash).

- [ ] **Step 4: Commit**

```bash
git add libs/vault/src/FileManager.cpp
git commit -m "feat(vault): honour Files/TrashOption and PromptDelete in delete modal"
```

---

# Phase 2+3 — Markoff Menu Surfacing + Fold Actions

Goal: Full Edit/View/Format/Heading/Insert/Table menu tree; 35 ActionIds (29 existing + 6 new SetHeading) reachable; callout + table dialogs functional; enable-state glue.

## Task 2.1: Add `SetHeading1..6` ActionIds

**Files:**
- Modify: `libs/markoff-family/libs/markoff/include/markoff/ActionId.h`
- Modify: `libs/markoff-family/libs/markoff/src/Editor.cpp`
- Create: `tests/markoff/tst_set_heading_actions.cpp`
- Modify: `tests/markoff/CMakeLists.txt`

- [ ] **Step 1: Extend enum**

In `ActionId.h`:

```cpp
enum class ActionId {
    Undo, Redo,
    Cut, Copy, Paste, SelectAll,
    Find, FindNext, FindPrevious, Replace,
    ZoomIn, ZoomOut,
    ToggleBold, ToggleItalic, ToggleStrikethrough, ToggleInlineCode,
    InsertLink, InsertWikiLink, InsertImage,
    InsertCodeBlock, InsertBlockQuote, InsertHorizontalRule, InsertTable,
    IncreaseHeading, DecreaseHeading, ToggleCheckbox,
    ToggleFoldAtCursor, FoldAll, UnfoldAll,
    SetHeading1, SetHeading2, SetHeading3, SetHeading4, SetHeading5, SetHeading6, // NEW
};
```

- [ ] **Step 2: Register in `createActions()`**

In `Editor.cpp` `createActions()` after the existing heading-level actions:

```cpp
auto addSetHeading = [&](ActionId id, int level, const QString &shortcut) {
    auto *act = new QAction(this);
    act->setText(i18n("Heading %1", level));
    act->setIcon(QIcon::fromTheme(QStringLiteral("format-text-heading")));
    act->setShortcut(QKeySequence(shortcut));
    connect(act, &QAction::triggered, this, [this, level] { setHeadingLevel(level); });
    m_actions.insert(id, act);
};
addSetHeading(ActionId::SetHeading1, 1, QStringLiteral("Ctrl+1"));
addSetHeading(ActionId::SetHeading2, 2, QStringLiteral("Ctrl+2"));
addSetHeading(ActionId::SetHeading3, 3, QStringLiteral("Ctrl+3"));
addSetHeading(ActionId::SetHeading4, 4, QStringLiteral("Ctrl+4"));
addSetHeading(ActionId::SetHeading5, 5, QStringLiteral("Ctrl+5"));
addSetHeading(ActionId::SetHeading6, 6, QStringLiteral("Ctrl+6"));
```

If `setHeadingLevel(int)` doesn't exist in `Editor` yet, add:

```cpp
// Editor.h
public: void setHeadingLevel(int level);
// Editor.cpp
void Editor::setHeadingLevel(int level) {
    // Look at increaseHeadingLevel / decreaseHeadingLevel internals to learn the
    // selection-line manipulation pattern; reuse by computing the delta from
    // current level and calling the same underlying block-formatter.
}
```

- [ ] **Step 3: Test**

```cpp
// tests/markoff/tst_set_heading_actions.cpp
#include <QtTest/QtTest>
#include <markoff/Editor.h>
class TstSetHeading : public QObject {
    Q_OBJECT
private slots:
    void triggersH3() {
        Markoff::Editor ed;
        ed.setPlainText(QStringLiteral("example\n"));
        // position cursor on line 0
        ed.action(Markoff::ActionId::SetHeading3)->trigger();
        QVERIFY(ed.toPlainText().startsWith(QStringLiteral("### example")));
    }
    // Repeat for H1, H2, H4, H5, H6.
};
QTEST_MAIN(TstSetHeading)
#include "tst_set_heading_actions.moc"
```

- [ ] **Step 4: Run**

```bash
cd build && ctest -R tst_set_heading --output-on-failure
```

- [ ] **Step 5: Commit**

```bash
git add libs/markoff-family/libs/markoff/include/markoff/ActionId.h libs/markoff-family/libs/markoff/src/Editor.cpp libs/markoff-family/libs/markoff/include/markoff/Editor.h tests/markoff/tst_set_heading_actions.cpp tests/markoff/CMakeLists.txt
git commit -m "feat(markoff): SetHeading1..6 actions with Ctrl+1..6 shortcuts"
```

## Task 2.2: Add `Editor::cursorInTable()` accessor

**Files:**
- Modify: `libs/markoff-family/libs/markoff/include/markoff/Editor.h`
- Modify: `libs/markoff-family/libs/markoff/src/Editor.cpp`
- Create: `tests/markoff/tst_editor_cursor_in_table.cpp`

- [ ] **Step 1: Declare**

```cpp
// Editor.h public:
bool cursorInTable() const;
```

- [ ] **Step 2: Implement**

Look at how existing table row/column context-menu gating decides it's inside a table (`Editor.cpp:625-633`). Extract that predicate. Expected form:

```cpp
bool Editor::cursorInTable() const
{
    auto *item = focusedTextItem();
    if (!item) return false;
    // Walk parser tree or line structure to detect a GFM pipe-table row.
    // Reuse the existing detection used in context-menu handler.
    return detectTableAtCursor(item); // name to match existing predicate
}
```

If no single-entry predicate exists, factor one out of the context-menu code and call from both sites.

- [ ] **Step 3: Test**

4 fixtures: not-in-table / first-cell / last-cell / between-tables. Each calls `cursorInTable()` and asserts.

- [ ] **Step 4: Run + commit**

```bash
git add libs/markoff-family/libs/markoff/include/markoff/Editor.{h,cpp} tests/markoff/tst_editor_cursor_in_table.cpp tests/markoff/CMakeLists.txt
git commit -m "feat(markoff): Editor::cursorInTable() accessor"
```

## Task 2.3: Callout picker dialog

**Files:**
- Create: `src/dialogs/CalloutPickerDialog.{h,cpp}`
- Modify: `src/dialogs/CMakeLists.txt`

- [ ] **Step 1: Write header**

```cpp
// CalloutPickerDialog.h
#pragma once
#include <QDialog>
class QComboBox;
class QLabel;
class QLineEdit;

class CalloutPickerDialog : public QDialog {
    Q_OBJECT
public:
    explicit CalloutPickerDialog(QWidget *parent = nullptr);
    QString selectedType() const;
    QString title() const;
private:
    QComboBox *m_combo;
    QLineEdit *m_title;
    QLabel *m_preview;
    void updatePreview();
};
```

- [ ] **Step 2: Implement**

26 callout types from spec Appendix A. Layout: `QFormLayout` with "Type:" combo, "Title:" optional line edit, preview label below in monospace showing `> [!<type>] <title>\n> ...`, OK/Cancel buttons.

- [ ] **Step 3: Commit**

```bash
git add src/dialogs/CalloutPickerDialog.{h,cpp} src/dialogs/CMakeLists.txt
git commit -m "feat(dialogs): CalloutPickerDialog with 26 Obsidian callout types"
```

## Task 2.4: Insert Table dialog

**Files:**
- Create: `src/dialogs/InsertTableDialog.{h,cpp}`
- Modify: `src/dialogs/CMakeLists.txt`

- [ ] **Step 1: Header**

```cpp
// InsertTableDialog.h
#pragma once
#include <QDialog>
class QSpinBox;
class QCheckBox;

class InsertTableDialog : public QDialog {
    Q_OBJECT
public:
    explicit InsertTableDialog(QWidget *parent = nullptr);
    int rows() const;
    int cols() const;
    bool firstRowAsHeader() const;
private:
    QSpinBox *m_rows;
    QSpinBox *m_cols;
    QCheckBox *m_header;
};
```

- [ ] **Step 2: Implement**

Rows 1–20 (default 3), cols 1–10 (default 3), header checkbox default checked. OK/Cancel.

- [ ] **Step 3: Commit**

```bash
git add src/dialogs/InsertTableDialog.{h,cpp} src/dialogs/CMakeLists.txt
git commit -m "feat(dialogs): InsertTableDialog rows/cols/header picker"
```

## Task 2.5: Register new actions in `MainWindow::setupActions`

**Files:**
- Modify: `src/app/MainWindow.h`, `src/app/MainWindow.cpp`

- [ ] **Step 1: Add slots**

```cpp
// MainWindow.h
private Q_SLOTS:
    void onFindNext();
    void onFindPrevious();
    void onReplace();
    void onFoldAll();
    void onUnfoldAll();
    void onToggleFold();
    void onInsertCallout();
    void onInsertTable();
    void onToggleBold();
    void onToggleItalic();
    void onToggleStrikethrough();
    void onToggleInlineCode();
    void onInsertLink();
    void onInsertWikiLink();
    void onInsertImage();
    void onInsertCodeBlock();
    void onInsertBlockQuote();
    void onInsertHorizontalRule();
    void onIncreaseHeading();
    void onDecreaseHeading();
    void onSetHeading(int level);
    void onToggleCheckbox();
    void onInsertTableRowAbove();
    void onInsertTableRowBelow();
    void onInsertTableColLeft();
    void onInsertTableColRight();
    void onDeleteTableRow();
    void onDeleteTableCol();
    void refreshEditorActions();
```

- [ ] **Step 2: Implement**

Most slots are one-liner forwarders:

```cpp
void MainWindow::onToggleBold() {
    auto *mv = qobject_cast<MarkdownView*>(activeView(m_workspace));
    if (mv) mv->editor()->action(Markoff::ActionId::ToggleBold)->trigger();
}
```

Factor a helper:

```cpp
void MainWindow::triggerEditorAction(Markoff::ActionId id) {
    auto *mv = qobject_cast<MarkdownView*>(activeView(m_workspace));
    if (mv) mv->editor()->action(id)->trigger();
}
```

Then each slot becomes `triggerEditorAction(Markoff::ActionId::X)`.

`onSetHeading(int)` calls `SetHeading1 + (level - 1)` cast to `ActionId`.

`onInsertCallout()`:

```cpp
CalloutPickerDialog dlg(this);
if (dlg.exec() == QDialog::Accepted) {
    if (auto *mv = qobject_cast<MarkdownView*>(activeView(m_workspace))) {
        mv->editor()->insertCallout(dlg.selectedType(), dlg.title());
    }
}
```

If `Editor::insertCallout(type, title)` doesn't exist, add it — look at existing `insertBlockQuote` for the pattern.

`onInsertTable()`: similar, via `InsertTableDialog`; call `Editor::insertTable(rows, cols, headerRow)` (may need to widen signature).

Table row/column ops forward to existing table-manipulation helpers in `Editor`.

- [ ] **Step 3: Register actions**

In `setupActions()` add ~25 new KActionCollection entries, each with:
- Object name matching `corbomiteui.rc.in` reference
- `setText(i18n("..."))`
- Icon via `QIcon::fromTheme`
- Default shortcut where applicable
- `connect` to the corresponding slot

Group into `// Format actions`, `// Heading actions` (use a `QActionGroup` for H1..H6 with exclusive=true), `// Insert actions`, `// Table actions`, `// Fold actions`, `// Search actions` blocks.

Example block (Format):

```cpp
auto addEditorAction = [&](const QString &id, Markoff::ActionId aid,
                           const QString &icon, const QString &label,
                           const QString &shortcut = {}) {
    auto *act = actionCollection()->addAction(id);
    act->setText(label);
    if (!icon.isEmpty()) act->setIcon(QIcon::fromTheme(icon));
    if (!shortcut.isEmpty())
        actionCollection()->setDefaultShortcut(act, QKeySequence(shortcut));
    connect(act, &QAction::triggered, this, [this, aid] { triggerEditorAction(aid); });
    return act;
};

addEditorAction(QStringLiteral("format_bold"),          Markoff::ActionId::ToggleBold,
                QStringLiteral("format-text-bold"),     i18n("Bold"),         QStringLiteral("Ctrl+B"));
addEditorAction(QStringLiteral("format_italic"),        Markoff::ActionId::ToggleItalic,
                QStringLiteral("format-text-italic"),   i18n("Italic"),       QStringLiteral("Ctrl+I"));
// ... full list per spec §3.4
```

H1..H6 via QActionGroup:

```cpp
auto *headingGroup = new QActionGroup(this);
headingGroup->setExclusive(true);
for (int level = 1; level <= 6; ++level) {
    auto *act = actionCollection()->addAction(QStringLiteral("heading_%1").arg(level));
    act->setText(i18n("Heading %1", level));
    act->setCheckable(true);
    act->setActionGroup(headingGroup);
    actionCollection()->setDefaultShortcut(act, QKeySequence(QStringLiteral("Ctrl+%1").arg(level)));
    connect(act, &QAction::triggered, this, [this, level] { onSetHeading(level); });
}
```

- [ ] **Step 4: Add `refreshEditorActions()`**

```cpp
void MainWindow::refreshEditorActions()
{
    auto *v = activeView(m_workspace);
    const bool isMarkdown = qobject_cast<MarkdownView*>(v) != nullptr;
    const QStringList editorActionIds = {
        QStringLiteral("format_bold"),
        // ... full list: format_*, heading_*, insert_*, table_*, fold_*,
        // edit_find_next, edit_find_previous, edit_replace, editor_toggle_mode
    };
    for (const auto &id : editorActionIds) {
        if (auto *act = actionCollection()->action(id)) act->setEnabled(isMarkdown);
    }
    // Table submenu is additionally gated:
    bool inTable = false;
    if (auto *mv = qobject_cast<MarkdownView*>(v)) inTable = mv->editor()->cursorInTable();
    for (const QString &id : {QStringLiteral("table_row_above"), QStringLiteral("table_row_below"),
                              QStringLiteral("table_col_left"),  QStringLiteral("table_col_right"),
                              QStringLiteral("table_delete_row"),QStringLiteral("table_delete_col")}) {
        if (auto *act = actionCollection()->action(id)) act->setEnabled(inTable);
    }
    // Heading group: find the current level and check the matching action.
    if (auto *mv = qobject_cast<MarkdownView*>(v)) {
        const int level = mv->editor()->currentHeadingLevel(); // add accessor if missing
        if (level >= 1 && level <= 6) {
            if (auto *act = actionCollection()->action(QStringLiteral("heading_%1").arg(level)))
                act->setChecked(true);
        }
    }
}
```

Connect in ctor:

```cpp
connect(m_workspace, &Corbomite::Workspace::activeLeafChanged, this, &MainWindow::refreshEditorActions);
// Also hook cursor-moved on MarkdownView activation for table-gate updates.
```

For cursor-moved routing, add `MarkdownView::cursorPositionChanged` forwarding from `Markoff::Editor::cursorChanged` (if it exists) or a 250ms throttled timer; connect to `refreshEditorActions` while the view is active.

- [ ] **Step 5: Build**

```bash
cmake --build build -j 10
```

- [ ] **Step 6: Commit**

```bash
git add src/app/MainWindow.{h,cpp}
git commit -m "feat(app): register ~25 Markoff editor actions; refreshEditorActions glue"
```

## Task 2.6: Update `corbomiteui.rc.in` menu tree

**Files:**
- Modify: `src/app/corbomiteui.rc.in`

- [ ] **Step 1: Rewrite menu structure per spec §3.4**

Open `corbomiteui.rc.in`. Target structure:

```xml
<MenuBar>
    <Menu name="file">
        <!-- existing file entries -->
    </Menu>
    <Menu name="edit">
        <!-- existing entries (Undo / Redo / Cut / Copy / Paste / Select All) -->
        <Separator/>
        <Action name="edit_find"/>
        <Action name="edit_find_next"/>
        <Action name="edit_find_previous"/>
        <Action name="edit_replace"/>
    </Menu>
    <Menu name="view">
        <!-- existing view entries -->
        <Separator/>
        <Menu name="editor_mode"><text>Editor Mode</text>
            <Action name="editor_mode_source"/>
            <Action name="editor_mode_live"/>
            <Action name="editor_mode_reading"/>
        </Menu>
        <Separator/>
        <Action name="fold_all"/>
        <Action name="unfold_all"/>
        <Action name="toggle_fold"/>
        <Separator/>
        <Action name="view_zoom_in"/>
        <Action name="view_zoom_out"/>
        <Action name="view_zoom_reset"/>
        <Separator/>
        <Action name="workspace_split_right"/>
        <Action name="workspace_split_down"/>
        <Action name="workspace_popout"/>
        <Action name="workspace_toggle_link"/>
    </Menu>
    <Menu name="format"><text>Format</text>
        <Action name="format_bold"/>
        <Action name="format_italic"/>
        <Action name="format_strikethrough"/>
        <Action name="format_inline_code"/>
        <Separator/>
        <Action name="insert_link"/>
        <Action name="insert_wiki_link"/>
        <Action name="insert_image"/>
        <Separator/>
        <Action name="insert_code_block"/>
        <Action name="insert_block_quote"/>
        <Action name="insert_horizontal_rule"/>
    </Menu>
    <Menu name="heading"><text>Heading</text>
        <Action name="heading_1"/>
        <Action name="heading_2"/>
        <Action name="heading_3"/>
        <Action name="heading_4"/>
        <Action name="heading_5"/>
        <Action name="heading_6"/>
        <Separator/>
        <Action name="heading_increase"/>
        <Action name="heading_decrease"/>
    </Menu>
    <Menu name="insert"><text>Insert</text>
        <Action name="insert_table"/>
        <Action name="insert_callout"/>
        <Action name="toggle_checkbox"/>
    </Menu>
    <Menu name="table"><text>Table</text>
        <Action name="table_row_above"/>
        <Action name="table_row_below"/>
        <Separator/>
        <Action name="table_col_left"/>
        <Action name="table_col_right"/>
        <Separator/>
        <Action name="table_delete_row"/>
        <Action name="table_delete_col"/>
    </Menu>
    <!-- existing Settings / Help / etc. -->
</MenuBar>
<ToolBar name="mainToolBar"><!-- existing + optional editor_toggle_mode --></ToolBar>
```

Also add the three `editor_mode_source/live/reading` radio actions to `setupActions()` in `MainWindow.cpp`. They live in a QActionGroup; each calls `setViewMode(VM::X)` and `refreshEditorModeMenu()` updates checked state.

- [ ] **Step 2: Build + launch**

```bash
cmake --build build -j 10 && ./build/Corbomite
```

Verify: no KXMLGUI warnings in stderr; all menus visible; shortcuts functional; enable-state flips when switching between a Markdown tab and a Canvas/Graph tab.

- [ ] **Step 3: Commit**

```bash
git add src/app/corbomiteui.rc.in src/app/MainWindow.{h,cpp}
git commit -m "feat(app): full Edit/View/Format/Heading/Insert/Table menu tree"
```

## Task 2.7: Fold actions + Editor Mode submenu wiring

**Files:**
- Modify: `src/app/MainWindow.cpp`

- [ ] **Step 1: Wire fold actions**

Three new actions: `fold_all`, `unfold_all`, `toggle_fold`. Shortcuts `Ctrl+Shift+-`, `Ctrl+Shift+=`, `Ctrl+.` respectively (Obsidian match).

```cpp
addEditorAction(QStringLiteral("fold_all"),     Markoff::ActionId::FoldAll,
                QStringLiteral("collapse-all"), i18n("Fold All"),
                QStringLiteral("Ctrl+Shift+-"));
addEditorAction(QStringLiteral("unfold_all"),   Markoff::ActionId::UnfoldAll,
                QStringLiteral("expand-all"),   i18n("Unfold All"),
                QStringLiteral("Ctrl+Shift+="));
addEditorAction(QStringLiteral("toggle_fold"),  Markoff::ActionId::ToggleFoldAtCursor,
                QStringLiteral("code-function"),i18n("Toggle Fold at Cursor"),
                QStringLiteral("Ctrl+."));
```

- [ ] **Step 2: Editor Mode radio submenu**

```cpp
auto *modeGroup = new QActionGroup(this);
modeGroup->setExclusive(true);
auto addModeAction = [&](const QString &id, const QString &label, NoteEditorWidget::ViewMode mode) {
    auto *act = actionCollection()->addAction(id);
    act->setText(label);
    act->setCheckable(true);
    act->setActionGroup(modeGroup);
    connect(act, &QAction::triggered, this, [this, mode] {
        if (auto *mv = qobject_cast<MarkdownView*>(activeView(m_workspace)))
            mv->noteEditorWidget()->setViewMode(mode);
    });
    return act;
};
addModeAction(QStringLiteral("editor_mode_source"),   i18n("Source"),      NoteEditorWidget::ViewMode::Source);
addModeAction(QStringLiteral("editor_mode_live"),     i18n("Live Preview"), NoteEditorWidget::ViewMode::LivePreview);
addModeAction(QStringLiteral("editor_mode_reading"),  i18n("Reading"),      NoteEditorWidget::ViewMode::Reading);
```

Refresh checked state when mode changes:

```cpp
// When a MarkdownView becomes active, connect to its viewModeChanged signal:
connect(mv->noteEditorWidget(), &NoteEditorWidget::viewModeChanged, this,
        [this](auto mode) {
            // flip the matching action to checked
        });
```

If `viewModeChanged` doesn't exist, add a `Q_SIGNAL void viewModeChanged(ViewMode);` to `NoteEditorWidget` + emit it from `setViewMode`.

- [ ] **Step 3: Commit**

```bash
git add src/app/MainWindow.{h,cpp}
git commit -m "feat(app): fold actions + Editor Mode radio submenu"
```

## Task 2.8: MainWindow action-wiring test

**Files:**
- Create: `tests/app/tst_mainwindow_action_wiring.cpp`

- [ ] **Step 1: Write test**

```cpp
#include <QtTest/QtTest>
#include "app/MainWindow.h"
#include <KActionCollection>

class TstActionWiring : public QObject {
    Q_OBJECT
private slots:
    void everyExpectedActionExists() {
        MainWindow w;
        auto *ac = w.actionCollection();
        const QStringList expected = {
            QStringLiteral("edit_find"), QStringLiteral("edit_find_next"),
            QStringLiteral("edit_find_previous"), QStringLiteral("edit_replace"),
            QStringLiteral("view_zoom_in"), QStringLiteral("view_zoom_out"), QStringLiteral("view_zoom_reset"),
            QStringLiteral("editor_toggle_mode"),
            QStringLiteral("editor_mode_source"), QStringLiteral("editor_mode_live"), QStringLiteral("editor_mode_reading"),
            QStringLiteral("fold_all"), QStringLiteral("unfold_all"), QStringLiteral("toggle_fold"),
            QStringLiteral("format_bold"), QStringLiteral("format_italic"), QStringLiteral("format_strikethrough"),
            QStringLiteral("format_inline_code"), QStringLiteral("insert_link"), QStringLiteral("insert_wiki_link"),
            QStringLiteral("insert_image"), QStringLiteral("insert_code_block"),
            QStringLiteral("insert_block_quote"), QStringLiteral("insert_horizontal_rule"),
            QStringLiteral("heading_1"), QStringLiteral("heading_2"), QStringLiteral("heading_3"),
            QStringLiteral("heading_4"), QStringLiteral("heading_5"), QStringLiteral("heading_6"),
            QStringLiteral("heading_increase"), QStringLiteral("heading_decrease"),
            QStringLiteral("insert_table"), QStringLiteral("insert_callout"), QStringLiteral("toggle_checkbox"),
            QStringLiteral("table_row_above"), QStringLiteral("table_row_below"),
            QStringLiteral("table_col_left"), QStringLiteral("table_col_right"),
            QStringLiteral("table_delete_row"), QStringLiteral("table_delete_col"),
            QStringLiteral("workspace_split_right"), QStringLiteral("workspace_split_down"),
            QStringLiteral("workspace_popout"), QStringLiteral("workspace_toggle_link"),
            QStringLiteral("workspace_reopen_closed"),
            QStringLiteral("workspace_move_tab_left"), QStringLiteral("workspace_move_tab_right"),
        };
        for (const auto &id : expected) QVERIFY2(ac->action(id) != nullptr, qPrintable(id));
    }
};
QTEST_MAIN(TstActionWiring)
#include "tst_mainwindow_action_wiring.moc"
```

- [ ] **Step 2: Append CMake + commit**

```bash
git add tests/app/tst_mainwindow_action_wiring.cpp tests/app/CMakeLists.txt
git commit -m "test(app): introspect action collection for all Cluster V actions"
```

---

# Phase 4 — ReadingView Interactions

Goal: click heading arrow to fold/unfold; hover a wiki-link to show `HoverPopover`; codeBlockProcessorRegistry routes mermaid/math through registry lookups.

## Task 4.1: `linkHovered` signal + HoverPopover wiring

**Files:**
- Modify: `libs/readingview/include/corbomite/readingview/ReadingView.h`
- Modify: `libs/readingview/src/ReadingView.cpp`
- Modify: `src/editor/NoteEditorWidget.{h,cpp}`
- Create: `tests/readingview/tst_link_hovered_signal.cpp`

- [ ] **Step 1: Add signal (adapter)**

In `ReadingView.h` near the existing `wikiLinkHovered` declaration:

```cpp
Q_SIGNALS:
    void wikiLinkHovered(const QString &target); // existing
    void linkHovered(const QString &target);     // NEW — fired whenever wikiLinkHovered fires
```

In `ReadingView.cpp`, find every `emit wikiLinkHovered(x);` site and add `emit linkHovered(x);` on the next line. Or add a connection in the ctor:

```cpp
connect(this, &ReadingView::wikiLinkHovered, this, &ReadingView::linkHovered);
```

The ctor-connect approach is cleaner — use it.

- [ ] **Step 2: Wire in NoteEditorWidget**

Find the existing Markoff `linkHovered → HoverPopover::scheduleShow` connect block (~line 52-63). Mirror it for ReadingView:

```cpp
connect(m_readingView, &Corbomite::ReadingView::ReadingView::linkHovered,
        m_popover,     &HoverPopover::scheduleShow);
connect(m_readingView, &Corbomite::ReadingView::ReadingView::linkHovered,
        this,          [this] { /* any additional state — mirror Markoff side */ });
```

Also wire `mouseLeft`/`hide` path to match Markoff.

- [ ] **Step 3: Test**

```cpp
// tst_link_hovered_signal.cpp
#include <QtTest/QtTest>
#include <QSignalSpy>
#include <corbomite/readingview/ReadingView.h>
class TstLinkHovered : public QObject {
    Q_OBJECT
private slots:
    void emitsOnWikiLink() {
        Corbomite::ReadingView::ReadingView rv(nullptr);
        QSignalSpy spy(&rv, &Corbomite::ReadingView::ReadingView::linkHovered);
        emit rv.wikiLinkHovered(QStringLiteral("Note A"));
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(0).toString(), QStringLiteral("Note A"));
    }
};
QTEST_MAIN(TstLinkHovered)
#include "tst_link_hovered_signal.moc"
```

- [ ] **Step 4: Commit**

```bash
git add libs/readingview/ src/editor/NoteEditorWidget.{h,cpp} tests/readingview/tst_link_hovered_signal.cpp tests/readingview/CMakeLists.txt
git commit -m "feat(readingview): linkHovered signal + HoverPopover wiring"
```

## Task 4.2: Click-to-fold on heading arrow

**Files:**
- Modify: `libs/readingview/src/ReadingView.cpp`, `libs/readingview/include/corbomite/readingview/ReadingView.h`
- Create: `tests/readingview/tst_click_to_fold.cpp`

- [ ] **Step 1: Override `mousePressEvent`**

In `ReadingView.h` declare (if not already present):

```cpp
protected:
    void mousePressEvent(QMouseEvent *event) override;
```

In `.cpp`:

```cpp
void ReadingView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        const QPointF scenePos = mapToScene(event->pos());
        if (auto *item = scene()->itemAt(scenePos, transform())) {
            const QVariant data = item->data(kFoldArrowSectionIdxProperty);
            if (data.isValid()) {
                toggleFold(data.toInt());
                event->accept();
                return;
            }
        }
    }
    QGraphicsView::mousePressEvent(event);
}
```

Confirm `kFoldArrowSectionIdxProperty` is visible from `ReadingView.cpp` (it's set in `SectionLayout.cpp:998` per recon); if private, expose via a shared anonymous-namespace key or header constant.

- [ ] **Step 2: Test**

```cpp
// tst_click_to_fold.cpp
#include <QtTest/QtTest>
#include <corbomite/readingview/ReadingView.h>
class TstClickToFold : public QObject {
    Q_OBJECT
private slots:
    void togglesOnArrowClick() {
        Corbomite::ReadingView::ReadingView rv(nullptr);
        rv.resize(800, 600);
        rv.setMarkdown(QStringLiteral("# A\ntext\n# B\ntext"));
        // Wait for layout; find a fold-arrow QGraphicsItem in the scene.
        // Synth a QMouseEvent at its pos.
        // Assert foldedHeadings contains the index.
    }
};
QTEST_MAIN(TstClickToFold)
#include "tst_click_to_fold.moc"
```

- [ ] **Step 3: Commit**

```bash
git add libs/readingview/ tests/readingview/tst_click_to_fold.cpp tests/readingview/CMakeLists.txt
git commit -m "feat(readingview): click fold-arrow to toggle heading section"
```

## Task 4.3: `codeBlockProcessorRegistry` dispatch in SectionLayout

**Files:**
- Modify: `libs/readingview/src/SectionLayout.cpp`

- [ ] **Step 1: Replace direct mermaid dispatch**

Around line 903 in `SectionLayout.cpp`, the `case BlockKind::DisplayMermaid:` branch currently calls mermaid rendering directly. Replace with:

```cpp
case BlockKind::DisplayMermaid: {
    if (auto *reg = ctx.codeBlockRegistry) {
        if (auto *proc = reg->processorFor(QStringLiteral("mermaid"))) {
            proc->process(ctx, block);
            break;
        }
    }
    // Fallback: existing direct renderer (keep as safety net).
    renderMermaidDirect(block);
    break;
}
```

Mirror for any other hard-coded language dispatches found in the same switch (math, syntax-highlighters) — delegate to the registry first, fall back otherwise.

- [ ] **Step 2: Verify existing mermaid still renders**

Smoke: open a note with a mermaid code block; render Reading view; verify diagram appears.

- [ ] **Step 3: Commit**

```bash
git add libs/readingview/src/SectionLayout.cpp
git commit -m "feat(readingview): route code-block rendering through registry"
```

---

# Phase 5 — Workspace Power-Features

Goal: Split / Popout / Linked-pane / Reopen-closed / Tab-move actions fully wired with shortcuts + menu entries.

## Task 5.1: Split right / Split down actions

**Files:**
- Modify: `src/app/MainWindow.h`, `src/app/MainWindow.cpp`

- [ ] **Step 1: Slots**

```cpp
private Q_SLOTS:
    void onSplitRight();
    void onSplitDown();
```

- [ ] **Step 2: Implement**

```cpp
void MainWindow::onSplitRight()
{
    if (auto *leaf = m_workspace->activeLeaf())
        m_workspace->splitLeaf(leaf, Qt::Horizontal);
}
void MainWindow::onSplitDown()
{
    if (auto *leaf = m_workspace->activeLeaf())
        m_workspace->splitLeaf(leaf, Qt::Vertical);
}
```

Verify `Qt::Horizontal` / `Qt::Vertical` semantics match `splitLeaf` (a horizontal split creates a right-sibling — confirm by reading `Workspace.cpp:268-282`).

- [ ] **Step 3: Register actions**

```cpp
auto *act = actionCollection()->addAction(QStringLiteral("workspace_split_right"));
act->setText(i18n("Split Right"));
act->setIcon(QIcon::fromTheme(QStringLiteral("view-split-left-right")));
actionCollection()->setDefaultShortcut(act, QKeySequence(QStringLiteral("Ctrl+\\")));
connect(act, &QAction::triggered, this, &MainWindow::onSplitRight);

auto *act2 = actionCollection()->addAction(QStringLiteral("workspace_split_down"));
act2->setText(i18n("Split Down"));
act2->setIcon(QIcon::fromTheme(QStringLiteral("view-split-top-bottom")));
actionCollection()->setDefaultShortcut(act2, QKeySequence(QStringLiteral("Ctrl+Shift+\\")));
connect(act2, &QAction::triggered, this, &MainWindow::onSplitDown);
```

- [ ] **Step 4: Smoke + commit**

```bash
cmake --build build -j 10 && ./build/Corbomite
# Ctrl+\ splits right; Ctrl+Shift+\ splits down
git add src/app/MainWindow.{h,cpp}
git commit -m "feat(workspace): Ctrl+\\ Split Right + Ctrl+Shift+\\ Split Down actions"
```

## Task 5.2: Popout to new window

**Files:**
- Modify: `src/app/MainWindow.h`, `src/app/MainWindow.cpp`

- [ ] **Step 1: Slot + action**

```cpp
void MainWindow::onPopoutActive()
{
    if (auto *leaf = m_workspace->activeLeaf())
        m_workspace->popoutLeaf(leaf);
}
```

Register `workspace_popout` action with text "Move to new window", icon `window-new`, no default shortcut.

- [ ] **Step 2: Smoke**

Verify new window opens with the leaf's content, and the tab in the main window is removed.

- [ ] **Step 3: Commit**

```bash
git add src/app/MainWindow.{h,cpp}
git commit -m "feat(workspace): Move to new window action (popoutLeaf)"
```

## Task 5.3: Link with active pane (toggle)

**Files:**
- Modify: `src/app/MainWindow.h`, `src/app/MainWindow.cpp`

- [ ] **Step 1: Slot**

```cpp
void MainWindow::onToggleLinkActive()
{
    auto *leaf = m_workspace->activeLeaf();
    if (!leaf) return;
    // Group semantics: toggle a "linked" group membership.
    // Use leaf->setGroupId(...) / setPinned(...) per Cluster G API.
    const bool nowLinked = leaf->groupId().isEmpty();
    if (nowLinked) {
        // Join the "default linked" group with any other currently-linked leaves.
        leaf->setGroupId(QStringLiteral("linked-default"));
        leaf->setPinned(true);
        m_workspace->propagatePinToGroup(leaf);
    } else {
        leaf->setGroupId(QString());
        leaf->setPinned(false);
    }
}
```

Confirm API shape — `setGroupId` / `setPinned` / `groupId()` against `WorkspaceLeaf.h`. If names differ, adapt.

- [ ] **Step 2: Checkable action**

```cpp
auto *act = actionCollection()->addAction(QStringLiteral("workspace_toggle_link"));
act->setText(i18n("Link with Active Pane"));
act->setCheckable(true);
act->setIcon(QIcon::fromTheme(QStringLiteral("link")));
connect(act, &QAction::triggered, this, &MainWindow::onToggleLinkActive);
```

Refresh checked state in `refreshEditorActions` / `onActiveLeafChanged`:

```cpp
if (auto *act = actionCollection()->action(QStringLiteral("workspace_toggle_link"))) {
    auto *leaf = m_workspace->activeLeaf();
    act->setChecked(leaf && !leaf->groupId().isEmpty());
}
```

- [ ] **Step 3: Commit**

```bash
git add src/app/MainWindow.{h,cpp}
git commit -m "feat(workspace): Link with active pane toggle action"
```

## Task 5.4: Reopen closed tab (Ctrl+Shift+T)

**Files:**
- Modify: `src/app/MainWindow.h`, `src/app/MainWindow.cpp`

- [ ] **Step 1: Slot + action**

```cpp
void MainWindow::onReopenClosed()
{
    if (m_tabModel) m_tabModel->reopenLastClosed();
}

// In setupActions:
auto *act = actionCollection()->addAction(QStringLiteral("workspace_reopen_closed"));
act->setText(i18n("Reopen Closed Tab"));
act->setIcon(QIcon::fromTheme(QStringLiteral("edit-undo")));
actionCollection()->setDefaultShortcut(act, QKeySequence(QStringLiteral("Ctrl+Shift+T")));
connect(act, &QAction::triggered, this, &MainWindow::onReopenClosed);
```

- [ ] **Step 2: Enable-state**

Disable the action when close-history is empty. Either re-check on every refresh (cheap) or listen to a `TabModel::closedHistoryChanged` signal. If none exists, add one.

- [ ] **Step 3: Commit**

```bash
git add src/app/MainWindow.{h,cpp}
git commit -m "feat(workspace): Ctrl+Shift+T reopen closed tab"
```

## Task 5.5: Tab move (Ctrl+Shift+PgUp/PgDn)

**Files:**
- Modify: `src/app/MainWindow.h`, `src/app/MainWindow.cpp`

- [ ] **Step 1: Slots**

```cpp
void MainWindow::onMoveTabLeft()
{
    if (!m_tabModel) return;
    int i = m_tabModel->activeIndex();
    if (i > 0) m_tabModel->moveTab(i, i - 1);
}
void MainWindow::onMoveTabRight()
{
    if (!m_tabModel) return;
    int i = m_tabModel->activeIndex();
    if (i >= 0 && i < m_tabModel->rowCount() - 1) m_tabModel->moveTab(i, i + 1);
}
```

Confirm `activeIndex()` / `rowCount()` are available on `TabModel`; otherwise derive from `activePath` + `indexOf`.

- [ ] **Step 2: Actions**

```cpp
auto *l = actionCollection()->addAction(QStringLiteral("workspace_move_tab_left"));
l->setText(i18n("Move Tab Left"));
l->setIcon(QIcon::fromTheme(QStringLiteral("go-previous")));
actionCollection()->setDefaultShortcut(l, QKeySequence(QStringLiteral("Ctrl+Shift+PgUp")));
connect(l, &QAction::triggered, this, &MainWindow::onMoveTabLeft);

auto *r = actionCollection()->addAction(QStringLiteral("workspace_move_tab_right"));
r->setText(i18n("Move Tab Right"));
r->setIcon(QIcon::fromTheme(QStringLiteral("go-next")));
actionCollection()->setDefaultShortcut(r, QKeySequence(QStringLiteral("Ctrl+Shift+PgDown")));
connect(r, &QAction::triggered, this, &MainWindow::onMoveTabRight);
```

- [ ] **Step 3: Commit**

```bash
git add src/app/MainWindow.{h,cpp}
git commit -m "feat(workspace): Ctrl+Shift+PgUp/PgDn move tab left/right"
```

---

# Phase 6 — Search UI Toggles + Toast Surfacing

Goal: Regex + Match-case buttons in SearchBar; "coming soon" tooltip gone; 5 swallowed-error sites post `Notice` toasts.

## Task 6.1: Regex + Match-case toggles in SearchBar

**Files:**
- Modify: `src/plugins/search/SearchView.cpp` (and SearchBar UI if separate)
- Create: `tests/search/tst_search_options_regex.cpp`

- [ ] **Step 1: Add toggle buttons**

In SearchView/SearchBar setup:

```cpp
m_regexToggle = new QToolButton(this);
m_regexToggle->setCheckable(true);
m_regexToggle->setIcon(QIcon::fromTheme(QStringLiteral("code-context")));
m_regexToggle->setToolTip(i18n("Use regular expressions"));
m_regexToggle->setText(QStringLiteral(".*"));

m_caseToggle = new QToolButton(this);
m_caseToggle->setCheckable(true);
m_caseToggle->setIcon(QIcon::fromTheme(QStringLiteral("format-text-uppercase")));
m_caseToggle->setToolTip(i18n("Match case"));
m_caseToggle->setText(QStringLiteral("Aa"));

// Add both to the toolbar/layout next to the search input.
connect(m_regexToggle, &QToolButton::toggled, this, &SearchView::rerunSearch);
connect(m_caseToggle,  &QToolButton::toggled, this, &SearchView::rerunSearch);
```

- [ ] **Step 2: Pass options into compile**

In the search execution path:

```cpp
SearchDSL::ParseOptions opts;
opts.regex = m_regexToggle->isChecked();
opts.caseSensitive = m_caseToggle->isChecked();
auto plan = SearchDSL::compile(query, opts);
```

- [ ] **Step 3: Remove "coming soon" tooltip**

Find the literal `"regex, line:, block:, section: coming soon"` string (line ~157 per recon). Replace with an empty string or remove the `setToolTip` call altogether. Keep the `line:` / `block:` / `section:` advanced operators for a future V.2 follow-up tracked in backlog.

- [ ] **Step 4: Test**

```cpp
// tst_search_options_regex.cpp
#include <QtTest/QtTest>
#include <search/SearchDSL.h>
class TstSearchRegex : public QObject {
    Q_OBJECT
private slots:
    void regexFlagPopulatesPlan() {
        SearchDSL::ParseOptions opts;
        opts.regex = true;
        auto plan = SearchDSL::compile(QStringLiteral("foo.*bar"), opts);
        QVERIFY(!plan.regexPatterns.isEmpty());
    }
};
QTEST_MAIN(TstSearchRegex)
#include "tst_search_options_regex.moc"
```

- [ ] **Step 5: Commit**

```bash
git add src/plugins/search/SearchView.cpp tests/search/tst_search_options_regex.cpp tests/search/CMakeLists.txt
git commit -m "feat(search): regex + match-case toggle buttons; remove coming-soon tooltip"
```

## Task 6.2: `Notice::post` helper (thread-safe factory)

**Files:**
- Modify: `src/dialogs/Notice.h`, `src/dialogs/Notice.cpp`

- [ ] **Step 1: Add static helper**

```cpp
// Notice.h
public:
    static void post(const QString &message, QWidget *anchor = nullptr, int durationMs = 4000);
```

```cpp
// Notice.cpp
void Notice::post(const QString &message, QWidget *anchor, int durationMs)
{
    QMetaObject::invokeMethod(qApp, [message, anchor, durationMs] {
        auto *n = new Notice(message, durationMs, anchor);
        n->show();
    }, Qt::QueuedConnection);
}
```

- [ ] **Step 2: Commit**

```bash
git add src/dialogs/Notice.{h,cpp}
git commit -m "feat(dialogs): Notice::post thread-safe factory helper"
```

## Task 6.3: Wire 5 error sites to `Notice::post`

**Files (audit at impl time):**
- Modify: `libs/vault/src/Vault.cpp` (saveDocument failure)
- Modify: `libs/core/src/PluginManager.cpp` (plugin load failure — confirm path)
- Modify: `libs/storage/src/VaultScanner.cpp` or watcher file (external-change notification)
- Modify: `libs/storage/src/CachedMetadataStore.cpp` (open failure)
- Modify: `libs/core/src/Workspace.cpp` or `WorkspaceLeaf.cpp` (unknown view-type → EmptyView fallback notice)

- [ ] **Step 1: For each call site, replace `qWarning() << msg` with**

```cpp
qWarning() << msg;
Notice::post(i18n("...user-facing message..."), /*anchor=*/nullptr, 5000);
```

Keep the `qWarning` for log-scraping + CI. Only the user-facing variant goes to `Notice`.

- [ ] **Step 2: Implementation details per site**

1. **saveDocument** — `Notice::post(i18n("Could not save '%1': %2", fileName, errorString))`. Offer `setAction(i18n("Retry"), [this] { saveDocument(doc); })` if the Notice supports actions before setAction API finalised.
2. **plugin load** — `Notice::post(i18n("Plugin '%1' failed to load. See logs for details.", pluginId))`.
3. **external file change** — `Notice::post(i18n("'%1' was modified outside Corbomite.", fileName))` (4s duration).
4. **CachedMetadataStore::open** — `Notice::post(i18n("Metadata cache unavailable; using full scan."))`.
5. **Unknown view-type fallback** — in the code path that spawns `EmptyView` for an unregistered viewType: `Notice::post(i18n("View type '%1' not available; showing empty view.", viewType))`.

- [ ] **Step 3: Smoke**

For each site, manually inject the failure (e.g., chmod a file read-only then save) and observe the toast.

- [ ] **Step 4: Commit**

```bash
git add libs/vault/src/Vault.cpp libs/core/src/PluginManager.cpp libs/storage/src/VaultScanner.cpp libs/storage/src/CachedMetadataStore.cpp libs/core/src/Workspace.cpp
git commit -m "feat(dialogs): surface 5 swallowed-error paths as Notice toasts"
```

---

# Phase 7 — Closeout

## Task 7.1: Run full test suite + manual smoke checklist

- [ ] **Step 1: Full CTest**

```bash
cmake --build build -j 10 && cd build && ctest --output-on-failure -j 10
```
Expected: all green.

- [ ] **Step 2: Manual smoke per spec §8**

Run through the manual checklist:
- No KXMLGUI warnings at startup.
- Every new menu entry reachable with mouse + keyboard.
- Enable/disable transitions correct when switching tab types.
- Theme toggle works immediately in Settings.
- Ctrl+E cycles three modes; radio submenu reflects.
- Callout + Insert Table dialogs work end-to-end.
- Split + Popout + Close cycle preserves open files.
- Notice toasts appear for the 5 failure paths (inject failures manually).

## Task 7.2: Retro + PROJECT-STATE + INDEX + backlog

**Files:**
- Create: `docs/cluster-retros/cluster-v.md`
- Modify: `docs/PROJECT-STATE.md`
- Modify: `docs/superpowers/plans/INDEX.md`
- Modify: `docs/backlog.md`

- [ ] **Step 1: Retro**

Write `docs/cluster-retros/cluster-v.md` per the pattern of `cluster-r.md` + `cluster-n.md`. Cover: what shipped, what slipped, what surprised us, what V.2 inherits, follow-ups.

- [ ] **Step 2: PROJECT-STATE**

- `**Last updated:**` line with date + one-sentence summary.
- Cluster V row: status → "Done"; append closeout notes.
- Move current focus forward (next candidate per backlog.md §1).
- Append Recent-decisions entry (≤1 paragraph) summarising closeout; full prose goes into `docs/decisions-archive.md` per the thin-PROJECT-STATE rule.

- [ ] **Step 3: INDEX**

- V row → "Done".
- Last-updated line.

- [ ] **Step 4: backlog.md**

- Strike the Cluster V entry (leave the strike-through + one-line closure + date).
- Leave V.2 at "scouting doc".

- [ ] **Step 5: Commit**

```bash
git add docs/cluster-retros/cluster-v.md docs/PROJECT-STATE.md docs/superpowers/plans/INDEX.md docs/backlog.md docs/decisions-archive.md
git commit -m "docs(cluster-v): retro; PROJECT-STATE + INDEX + backlog closeout"
```

## Task 7.3: Move plan to archive

**Files:**
- Move: `docs/superpowers/plans/2026-04-20-cluster-v-editor-workspace-ui-surfacing.md` → `archive/`

- [ ] **Step 1: Git mv**

```bash
git mv docs/superpowers/plans/2026-04-20-cluster-v-editor-workspace-ui-surfacing.md docs/superpowers/plans/archive/
```

- [ ] **Step 2: Update INDEX link**

Edit INDEX.md V row's Plan link to point to `archive/2026-04-20-cluster-v-editor-workspace-ui-surfacing.md`.

- [ ] **Step 3: Commit**

```bash
git add -A
git commit -m "docs(cluster-v): archive closed plan"
```

---

## Self-review — spec coverage

| Spec §2.1 in-scope item | Plan task(s) |
|---|---|
| Phase 1 dead app-shell actions | 1.1, 1.5, 1.6, 1.7, 1.8 |
| Phase 2 Markoff action surfacing | 2.1, 2.2, 2.3, 2.4, 2.5, 2.6 |
| Phase 3 fold actions | 2.7 (absorbed) |
| `View::zoomIn/Out/Reset` virtuals | 1.2, 1.3, 1.4 |
| `Editor::cursorInTable()` | 2.2 |
| `ActionId::SetHeading1..6` + Ctrl+1..6 | 2.1 |
| CalloutPickerDialog | 2.3 |
| InsertTableDialog | 2.4 |
| Phase 4 HeadingItem click-to-fold | 4.2 |
| Phase 4 `linkHovered → HoverPopover` | 4.1 |
| Phase 4 codeBlockProcessorRegistry | 4.3 |
| Phase 5 Split Ctrl+\ / Ctrl+Shift+\ | 5.1 |
| Phase 5 popout | 5.2 |
| Phase 5 linked-pane toggle | 5.3 |
| Phase 5 Ctrl+Shift+T reopen | 5.4 |
| Phase 5 Ctrl+Shift+PgUp/PgDn tab move | 5.5 |
| Phase 6 regex + case toggles | 6.1 |
| Phase 6 Notice surfacing (5 sites) | 6.3 |
| KColorScheme find_package | 1.1 |
| MainWindow::onSettingsApplied dispatcher | 1.7 |
| editor_toggle_mode + Ctrl+E cycle | 1.6 |
| Editor Mode radio submenu | 2.7 |
| Heading menu H1-H6 as QActionGroup | 2.5 |
| Table submenu cursor gate | 2.5 (refreshEditorActions) |
| V.2 handoff + retro | 7.1, 7.2, 7.3 |
| action-wiring test | 2.8 |

All spec §2.1 items covered. §2.2 (out-of-scope items) explicitly deferred to V.2; V.2 scouting doc already committed. Tests from §8 mapped; manual smoke checklist executed in Task 7.1.

Placeholder scan clean: no TBDs, TODOs, or "similar to Task N" references. Type consistency checked: `triggerEditorAction`, `activeView`, `refreshEditorActions`, `onSetHeading` names are consistent across tasks.

---

## Blocks / enables

**Blocks:** nothing downstream (V is a leaf cluster; V.2 waits on V).

**Enables:** V.2 (all deferred items pick up from this cluster's `MainWindow::onSettingsApplied` dispatcher, the `refreshEditorActions` enable-state harness, and the `View::zoomIn/Out/Reset` contract).
