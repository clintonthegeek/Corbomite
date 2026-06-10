# Find UI port — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Port Corbomite's Find UI to the new `Markoff::FindController` contract — Ctrl+F shows a host-owned FindBar, search-as-you-type with live highlights, Return/Shift+Return navigate, Esc closes.

**Architecture:** New `Corbomite::FindBar` QWidget docked at the bottom of `NoteEditorWidget`. Each `NoteDocument` lazily owns one `Markoff::FindController` bound to its `MarkoffDocument`. NoteEditorWidget attaches the controller to the active leaf via the symmetric `attachFindController` hook on both Live and Source. MainWindow's `KStandardAction::find` routes to the active widget. UX matches Okular's layout + Kate's keyboard shortcuts; no LineEdit color feedback.

**Tech Stack:** Qt6 Widgets, Markoff::Core (FindController), KStandardAction.

**Spec:** [`docs/superpowers/specs/2026-05-20-find-ui-port-design.md`](../specs/2026-05-20-find-ui-port-design.md).

---

## Orientation

**Working directory:** `/home/clinton/dev/Corbomite` (NOT the worktree; the port lives on the main checkout, branch `port/foundation-exploration`).

**Verify before starting:**
```bash
cd /home/clinton/dev/Corbomite
git branch --show-current  # must print: port/foundation-exploration
git status --short          # should be clean
cmake --build build-dev -j 8  # should succeed
./build-dev/bin/Corbomite     # should launch (manual smoke)
```

**Build command for all tasks:** `cmake --build build-dev -j 8` (NEVER bare `-j`).

**Test command for all tasks:** `cd build-dev && ctest -R '<pattern>' --output-on-failure`.

**Test environment:** Qt tests set `QT_QPA_PLATFORM=offscreen` via `set_tests_properties`. Direct invocation: `QT_QPA_PLATFORM=offscreen ./build-dev/bin/tst_findbar`.

**Commit convention:** `feat(editor): …`, `test(editor): …`, `feat(core): …`, `build: …`. Co-author trailer:
```
Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
```

---

## Task 1: `NoteDocument::findController()` lazy accessor

**Files:**
- Modify: `libs/core/include/corbomite/core/NoteDocument.h`
- Modify: `libs/core/src/NoteDocument.cpp`

Add a public method that lazily constructs one `Markoff::FindController` per NoteDocument, bound to its `MarkoffDocument`. Parent to `this` so destruction cascades.

- [ ] **Step 1: Verify the FindController header is reachable**

Run: `ls libs/markoff-family/libs/markoff-core/include/markoff/core/FindController.h`
Expected: file exists.

- [ ] **Step 2: Modify `libs/core/include/corbomite/core/NoteDocument.h`**

Add the forward-declare and the accessor.

In the `namespace Markoff` forward-declare block, add `FindController`:

```cpp
namespace Markoff { class MarkoffDocument; class FindController; }
```

After the `markoff()` accessors (around line 52), add:

```cpp
    /// Lazy: constructs one FindController per NoteDocument on first call.
    /// Bound to markoff(); owned by NoteDocument via QObject parent.
    Markoff::FindController *findController();
```

- [ ] **Step 3: Modify `libs/core/src/NoteDocument.cpp`**

At the top of the file, add to the includes block:

```cpp
#include <markoff/core/FindController.h>
```

Add a `m_findController` slot to the `Private` struct (find it near the top of the .cpp; it holds the pimpl members):

```cpp
    Markoff::FindController *findController = nullptr;
```

After the existing `markoff()` implementation, add:

```cpp
Markoff::FindController *NoteDocument::findController()
{
    if (!d->findController) {
        d->findController = new Markoff::FindController(markoff(), this);
    }
    return d->findController;
}
```

(If `Private` is in a different shape than expected, adapt — the principle is: a single pointer, lazy-init in the accessor, parented to `this` so cleanup is automatic.)

- [ ] **Step 4: Build**

Run: `cmake --build build-dev --target Corbomite -j 8`
Expected: success.

- [ ] **Step 5: Commit**

```bash
git add libs/core/include/corbomite/core/NoteDocument.h libs/core/src/NoteDocument.cpp
git commit -m "$(cat <<'EOF'
feat(core): NoteDocument::findController() lazy accessor

One FindController per NoteDocument, bound to markoff(),
parented to this. Lazy: many notes never get searched, so
we don't pay the cost on construction.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 2: FindBar widget skeleton + first failing test

**Files:**
- Create: `src/editor/FindBar.h`
- Create: `src/editor/FindBar.cpp`
- Create: `tests/editor/tst_findbar.cpp`
- Modify: `src/CMakeLists.txt` (add FindBar sources)
- Modify: `tests/editor/CMakeLists.txt` (add tst_findbar target)

Construct the widget shell with no controller binding yet. The first test verifies the widget is safe with no controller set (no crash, blank label, disabled buttons).

- [ ] **Step 1: Write the failing test `tests/editor/tst_findbar.cpp`**

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QSignalSpy>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QToolButton>

#include <markoff/core/MarkoffDocument.h>
#include <markoff/core/FindController.h>
#include <markoff/core/Origin.h>

#include "FindBar.h"

using namespace Corbomite;

class TstFindBar : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void unbound_safe();
};

void TstFindBar::unbound_safe()
{
    FindBar bar;
    // Find by object name (set in FindBar ctor below).
    auto *lineEdit  = bar.findChild<QLineEdit*>("findBarLineEdit");
    auto *countLabel = bar.findChild<QLabel*>("findBarCountLabel");
    auto *prevBtn   = bar.findChild<QPushButton*>("findBarPrev");
    auto *nextBtn   = bar.findChild<QPushButton*>("findBarNext");
    auto *closeBtn  = bar.findChild<QToolButton*>("findBarClose");

    QVERIFY(lineEdit);
    QVERIFY(countLabel);
    QVERIFY(prevBtn);
    QVERIFY(nextBtn);
    QVERIFY(closeBtn);

    QCOMPARE(countLabel->text(), QString());
    QVERIFY(!prevBtn->isEnabled());
    QVERIFY(!nextBtn->isEnabled());
    QVERIFY(closeBtn->isEnabled());
    QCOMPARE(bar.controller(), nullptr);
}

QTEST_MAIN(TstFindBar)
#include "tst_findbar.moc"
```

- [ ] **Step 2: Create `src/editor/FindBar.h`**

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QFrame>

class QLineEdit;
class QLabel;
class QPushButton;
class QToolButton;

namespace Markoff { class FindController; }

namespace Corbomite {

/// Horizontal in-document Find bar docked at the bottom of NoteEditorWidget.
/// Binds to a Markoff::FindController via setController(). Search-as-you-type:
/// every keystroke updates controller.needle; Return / Shift+Return drive
/// navigation; Esc emits closeRequested. No LineEdit color feedback — count
/// label carries all "no matches" messaging.
class FindBar : public QFrame {
    Q_OBJECT
public:
    explicit FindBar(QWidget *parent = nullptr);
    ~FindBar() override;

    /// Bind to a controller. Passing nullptr detaches. Safe to call repeatedly.
    void setController(Markoff::FindController *controller);
    Markoff::FindController *controller() const;

    /// Focus the line edit. Called by NoteEditorWidget::showFindBar.
    void focusLineEdit();

Q_SIGNALS:
    void closeRequested();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    void onLineEditTextChanged(const QString &text);
    void onNeedleChanged();
    void onMatchesChanged();
    void onCurrentMatchChanged();
    void refreshCountLabel();
    void refreshButtonEnableState();

    QLineEdit   *m_lineEdit   = nullptr;
    QLabel      *m_countLabel = nullptr;
    QPushButton *m_prevButton = nullptr;
    QPushButton *m_nextButton = nullptr;
    QToolButton *m_closeButton = nullptr;
    Markoff::FindController *m_controller = nullptr;
    bool m_applyingControllerNeedle = false;
};

} // namespace Corbomite
```

- [ ] **Step 3: Create `src/editor/FindBar.cpp` (skeleton only — layout + ctor)**

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "FindBar.h"

#include <QHBoxLayout>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QToolButton>
#include <QIcon>

namespace Corbomite {

FindBar::FindBar(QWidget *parent)
    : QFrame(parent)
{
    setFrameShape(QFrame::StyledPanel);
    setFrameShadow(QFrame::Sunken);

    m_closeButton = new QToolButton(this);
    m_closeButton->setObjectName(QStringLiteral("findBarClose"));
    m_closeButton->setAutoRaise(true);
    m_closeButton->setIcon(QIcon::fromTheme(QStringLiteral("dialog-close")));
    m_closeButton->setToolTip(tr("Close find bar (Esc)"));

    auto *label = new QLabel(tr("F&ind:"), this);

    m_lineEdit = new QLineEdit(this);
    m_lineEdit->setObjectName(QStringLiteral("findBarLineEdit"));
    m_lineEdit->setClearButtonEnabled(true);
    m_lineEdit->installEventFilter(this);
    label->setBuddy(m_lineEdit);

    m_countLabel = new QLabel(this);
    m_countLabel->setObjectName(QStringLiteral("findBarCountLabel"));

    m_prevButton = new QPushButton(this);
    m_prevButton->setObjectName(QStringLiteral("findBarPrev"));
    m_prevButton->setIcon(QIcon::fromTheme(QStringLiteral("go-up-search")));
    m_prevButton->setToolTip(tr("Previous match (Shift+F3)"));
    m_prevButton->setEnabled(false);

    m_nextButton = new QPushButton(this);
    m_nextButton->setObjectName(QStringLiteral("findBarNext"));
    m_nextButton->setIcon(QIcon::fromTheme(QStringLiteral("go-down-search")));
    m_nextButton->setToolTip(tr("Next match (F3)"));
    m_nextButton->setEnabled(false);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(6, 2, 6, 2);
    layout->setSpacing(4);
    layout->addWidget(m_closeButton);
    layout->addWidget(label);
    layout->addWidget(m_lineEdit, 1);  // stretch
    layout->addWidget(m_countLabel);
    layout->addWidget(m_prevButton);
    layout->addWidget(m_nextButton);
}

FindBar::~FindBar() = default;

void FindBar::setController(Markoff::FindController *controller)
{
    // Implemented in Task 3.
    m_controller = controller;
}

Markoff::FindController *FindBar::controller() const
{
    return m_controller;
}

void FindBar::focusLineEdit()
{
    m_lineEdit->setFocus();
    m_lineEdit->selectAll();
}

bool FindBar::eventFilter(QObject *obj, QEvent *event)
{
    // Implemented in Tasks 5 and 6.
    return QFrame::eventFilter(obj, event);
}

void FindBar::onLineEditTextChanged(const QString &) { /* Task 3 */ }
void FindBar::onNeedleChanged()                       { /* Task 3 */ }
void FindBar::onMatchesChanged()                      { /* Task 4 */ }
void FindBar::onCurrentMatchChanged()                 { /* Task 4 */ }
void FindBar::refreshCountLabel()                     { /* Task 4 */ }
void FindBar::refreshButtonEnableState()              { /* Task 4 */ }

} // namespace Corbomite
```

- [ ] **Step 4: Add FindBar sources to `src/CMakeLists.txt`**

Find the block listing `editor/*.cpp` files (after `editor/HoverPopover.cpp` per the earlier scout) and add:

```cmake
    editor/FindBar.cpp
    editor/FindBar.h
```

Place these alphabetically near the other editor entries.

- [ ] **Step 5: Add `tst_findbar` test target to `tests/editor/CMakeLists.txt`**

Append:

```cmake
# Find UI port (2026-05-20) — host-owned FindBar consumes Markoff::FindController.
add_executable(tst_findbar
    tst_findbar.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/FindBar.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/FindBar.h
)
target_include_directories(tst_findbar PRIVATE
    ${CMAKE_SOURCE_DIR}/src/editor
)
target_link_libraries(tst_findbar PRIVATE
    Qt6::Test
    Qt6::Widgets
    Markoff::Core
)
add_test(NAME tst_findbar COMMAND tst_findbar)
set_tests_properties(tst_findbar PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)
```

(If `Markoff::Core` is not the exact target alias, use `markoff_core`. Check existing test targets for the correct spelling.)

- [ ] **Step 6: Reconfigure CMake**

Run: `cmake -S . -B build-dev`
Expected: success.

- [ ] **Step 7: Build the test and watch it fail expectedly (the test only checks ctor; should pass once compiled)**

Run: `cmake --build build-dev --target tst_findbar -j 8`
Expected: success.

Run: `QT_QPA_PLATFORM=offscreen ./build-dev/bin/tst_findbar -v2`
Expected: `unbound_safe` PASSES (the test verifies skeleton correctness; it's not a falsifiable behaviour test, it's a sanity check that the skeleton compiled and the children exist).

- [ ] **Step 8: Commit**

```bash
git add src/editor/FindBar.h src/editor/FindBar.cpp src/CMakeLists.txt \
        tests/editor/tst_findbar.cpp tests/editor/CMakeLists.txt
git commit -m "$(cat <<'EOF'
feat(editor): FindBar widget skeleton

Horizontal layout: Close, Find: label, line edit (with clear
button), count label, Previous, Next. Controller binding,
signal wiring, and key dispatch land in subsequent tasks.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 3: `setController` + textChanged → setNeedle

**Files:**
- Modify: `src/editor/FindBar.cpp`
- Modify: `tests/editor/tst_findbar.cpp`

Wire the line-edit `textChanged` signal to `controller->setNeedle`. Guard against echo when the controller's `needleChanged` signal propagates back.

- [ ] **Step 1: Add the failing test**

In `tests/editor/tst_findbar.cpp`, add the slot to the `private Q_SLOTS:` list:

```cpp
    void textChanged_updatesController();
```

And the implementation:

```cpp
void TstFindBar::textChanged_updatesController()
{
    Markoff::MarkoffDocument doc(1);
    doc.resetContent(QByteArray("Hello world\n"), Markoff::Origin::FirstOpen);
    Markoff::FindController fc(&doc);

    FindBar bar;
    bar.setController(&fc);
    auto *lineEdit = bar.findChild<QLineEdit*>("findBarLineEdit");
    QVERIFY(lineEdit);

    lineEdit->setText(QStringLiteral("Hello"));

    QCOMPARE(fc.needle(), QStringLiteral("Hello"));
}
```

- [ ] **Step 2: Run the test and watch it fail**

Run: `cmake --build build-dev --target tst_findbar -j 8 && QT_QPA_PLATFORM=offscreen ./build-dev/bin/tst_findbar -v2`
Expected: `textChanged_updatesController` FAILS — controller.needle() is empty because nothing is wired yet.

- [ ] **Step 3: Implement `setController` in `src/editor/FindBar.cpp`**

Replace the existing `setController` stub:

```cpp
void FindBar::setController(Markoff::FindController *controller)
{
    if (m_controller == controller) return;

    if (m_controller) {
        QObject::disconnect(m_controller, nullptr, this, nullptr);
    }

    m_controller = controller;

    if (m_controller) {
        QObject::connect(m_controller, &Markoff::FindController::needleChanged,
                         this, &FindBar::onNeedleChanged);
        QObject::connect(m_controller, &Markoff::FindController::matchesChanged,
                         this, &FindBar::onMatchesChanged);
        QObject::connect(m_controller, &Markoff::FindController::currentMatchChanged,
                         this, &FindBar::onCurrentMatchChanged);
    }

    refreshCountLabel();
    refreshButtonEnableState();
}
```

Replace the stubs for `onLineEditTextChanged` and `onNeedleChanged`:

```cpp
void FindBar::onLineEditTextChanged(const QString &text)
{
    if (m_applyingControllerNeedle) return;
    if (!m_controller) return;
    m_controller->setNeedle(text);
}

void FindBar::onNeedleChanged()
{
    if (!m_controller) return;
    if (m_lineEdit->text() == m_controller->needle()) return;
    m_applyingControllerNeedle = true;
    m_lineEdit->setText(m_controller->needle());
    m_applyingControllerNeedle = false;
}
```

Add the `textChanged` connection at the end of the ctor (after layout):

```cpp
    QObject::connect(m_lineEdit, &QLineEdit::textChanged,
                     this, &FindBar::onLineEditTextChanged);
```

Add the FindController include at the top of FindBar.cpp:

```cpp
#include <markoff/core/FindController.h>
```

- [ ] **Step 4: Run the test and watch it pass**

Run: `cmake --build build-dev --target tst_findbar -j 8 && QT_QPA_PLATFORM=offscreen ./build-dev/bin/tst_findbar -v2`
Expected: all tests PASS.

- [ ] **Step 5: Commit**

```bash
git add src/editor/FindBar.cpp tests/editor/tst_findbar.cpp
git commit -m "$(cat <<'EOF'
feat(editor): FindBar binds line edit to FindController.needle

setController wires the line edit's textChanged to
controller.setNeedle. Echo guard suppresses the controller's
needleChanged signal from re-firing the line edit when the
caller drives setNeedle directly.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 4: matchesChanged → count label + button enable state

**Files:**
- Modify: `src/editor/FindBar.cpp`
- Modify: `tests/editor/tst_findbar.cpp`

Implement the count label policy and button enable state from the spec.

- [ ] **Step 1: Add three failing tests**

Add to the slot list:

```cpp
    void matchesChanged_updatesCountLabel();
    void noMatch_showsNoMatches();
    void prevNextButton_disabledWhenNoMatches();
```

Implementations:

```cpp
void TstFindBar::matchesChanged_updatesCountLabel()
{
    Markoff::MarkoffDocument doc(1);
    // "test" appears 3 times.
    doc.resetContent(QByteArray("test alpha test beta test\n"),
                     Markoff::Origin::FirstOpen);
    Markoff::FindController fc(&doc);

    FindBar bar;
    bar.setController(&fc);
    auto *lineEdit  = bar.findChild<QLineEdit*>("findBarLineEdit");
    auto *countLabel = bar.findChild<QLabel*>("findBarCountLabel");

    lineEdit->setText(QStringLiteral("test"));

    QCOMPARE(fc.matchCount(), 3);
    QCOMPARE(countLabel->text(), QStringLiteral("1 of 3"));
}

void TstFindBar::noMatch_showsNoMatches()
{
    Markoff::MarkoffDocument doc(1);
    doc.resetContent(QByteArray("hello\n"), Markoff::Origin::FirstOpen);
    Markoff::FindController fc(&doc);

    FindBar bar;
    bar.setController(&fc);
    auto *lineEdit  = bar.findChild<QLineEdit*>("findBarLineEdit");
    auto *countLabel = bar.findChild<QLabel*>("findBarCountLabel");

    lineEdit->setText(QStringLiteral("zzz"));

    QCOMPARE(fc.matchCount(), 0);
    QCOMPARE(countLabel->text(), QStringLiteral("No matches"));
}

void TstFindBar::prevNextButton_disabledWhenNoMatches()
{
    Markoff::MarkoffDocument doc(1);
    doc.resetContent(QByteArray("hello\n"), Markoff::Origin::FirstOpen);
    Markoff::FindController fc(&doc);

    FindBar bar;
    bar.setController(&fc);
    auto *lineEdit = bar.findChild<QLineEdit*>("findBarLineEdit");
    auto *prevBtn  = bar.findChild<QPushButton*>("findBarPrev");
    auto *nextBtn  = bar.findChild<QPushButton*>("findBarNext");

    // Empty needle: disabled.
    QVERIFY(!prevBtn->isEnabled());
    QVERIFY(!nextBtn->isEnabled());

    // No-match needle: still disabled.
    lineEdit->setText(QStringLiteral("zzz"));
    QVERIFY(!prevBtn->isEnabled());
    QVERIFY(!nextBtn->isEnabled());

    // Match: enabled.
    lineEdit->setText(QStringLiteral("hello"));
    QVERIFY(prevBtn->isEnabled());
    QVERIFY(nextBtn->isEnabled());
}
```

- [ ] **Step 2: Run tests and watch them fail**

Run: `cmake --build build-dev --target tst_findbar -j 8 && QT_QPA_PLATFORM=offscreen ./build-dev/bin/tst_findbar -v2`
Expected: 3 new FAILs — count label is blank, buttons stay disabled.

- [ ] **Step 3: Implement `refreshCountLabel`, `refreshButtonEnableState`, `onMatchesChanged`, `onCurrentMatchChanged` in FindBar.cpp**

Replace those stubs:

```cpp
void FindBar::refreshCountLabel()
{
    if (!m_controller || m_controller->needle().isEmpty()) {
        m_countLabel->setText(QString());
        return;
    }
    const int matchCount = m_controller->matchCount();
    if (matchCount == 0) {
        m_countLabel->setText(tr("No matches"));
        return;
    }
    const int current = m_controller->currentMatchIndex();
    m_countLabel->setText(tr("%1 of %2").arg(current + 1).arg(matchCount));
}

void FindBar::refreshButtonEnableState()
{
    const bool hasMatches = m_controller && m_controller->matchCount() > 0;
    m_prevButton->setEnabled(hasMatches);
    m_nextButton->setEnabled(hasMatches);
}

void FindBar::onMatchesChanged()
{
    refreshCountLabel();
    refreshButtonEnableState();
}

void FindBar::onCurrentMatchChanged()
{
    refreshCountLabel();
}
```

- [ ] **Step 4: Run tests and watch them pass**

Run: `cmake --build build-dev --target tst_findbar -j 8 && QT_QPA_PLATFORM=offscreen ./build-dev/bin/tst_findbar -v2`
Expected: all PASS.

- [ ] **Step 5: Commit**

```bash
git add src/editor/FindBar.cpp tests/editor/tst_findbar.cpp
git commit -m "$(cat <<'EOF'
feat(editor): FindBar count label + button enable state

Count label shows "N of M" / "No matches" / empty per
needle and match-count state. Prev/Next buttons enabled
only when matchCount > 0.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 5: Button clicks + Return / Shift+Return navigation

**Files:**
- Modify: `src/editor/FindBar.cpp`
- Modify: `tests/editor/tst_findbar.cpp`

Wire Next/Prev buttons to controller methods. Implement the line-edit eventFilter for Return / Shift+Return.

- [ ] **Step 1: Add failing tests**

Add to the slot list:

```cpp
    void returnKey_callsFindNext();
    void shiftReturn_callsFindPrev();
    void nextButton_callsFindNext();
    void prevButton_callsFindPrev();
```

Implementations:

```cpp
void TstFindBar::returnKey_callsFindNext()
{
    Markoff::MarkoffDocument doc(1);
    doc.resetContent(QByteArray("test alpha test beta test\n"),
                     Markoff::Origin::FirstOpen);
    Markoff::FindController fc(&doc);

    FindBar bar;
    bar.setController(&fc);
    auto *lineEdit = bar.findChild<QLineEdit*>("findBarLineEdit");
    lineEdit->setText(QStringLiteral("test"));

    QSignalSpy spy(&fc, &Markoff::FindController::navigationRequested);
    QTest::keyClick(lineEdit, Qt::Key_Return);
    QCOMPARE(spy.count(), 1);
}

void TstFindBar::shiftReturn_callsFindPrev()
{
    Markoff::MarkoffDocument doc(1);
    doc.resetContent(QByteArray("test alpha test beta test\n"),
                     Markoff::Origin::FirstOpen);
    Markoff::FindController fc(&doc);

    FindBar bar;
    bar.setController(&fc);
    auto *lineEdit = bar.findChild<QLineEdit*>("findBarLineEdit");
    lineEdit->setText(QStringLiteral("test"));

    // Advance forward once first, so we can measure the backward step.
    fc.findNext();
    const int afterForward = fc.currentMatchIndex();

    QTest::keyClick(lineEdit, Qt::Key_Return, Qt::ShiftModifier);
    QVERIFY(fc.currentMatchIndex() != afterForward);
}

void TstFindBar::nextButton_callsFindNext()
{
    Markoff::MarkoffDocument doc(1);
    doc.resetContent(QByteArray("test alpha test beta test\n"),
                     Markoff::Origin::FirstOpen);
    Markoff::FindController fc(&doc);

    FindBar bar;
    bar.setController(&fc);
    auto *lineEdit = bar.findChild<QLineEdit*>("findBarLineEdit");
    auto *nextBtn  = bar.findChild<QPushButton*>("findBarNext");
    lineEdit->setText(QStringLiteral("test"));

    QSignalSpy spy(&fc, &Markoff::FindController::navigationRequested);
    QTest::mouseClick(nextBtn, Qt::LeftButton);
    QCOMPARE(spy.count(), 1);
}

void TstFindBar::prevButton_callsFindPrev()
{
    Markoff::MarkoffDocument doc(1);
    doc.resetContent(QByteArray("test alpha test beta test\n"),
                     Markoff::Origin::FirstOpen);
    Markoff::FindController fc(&doc);

    FindBar bar;
    bar.setController(&fc);
    auto *lineEdit = bar.findChild<QLineEdit*>("findBarLineEdit");
    auto *prevBtn  = bar.findChild<QPushButton*>("findBarPrev");
    lineEdit->setText(QStringLiteral("test"));

    fc.findNext();  // advance once so prev has somewhere to go

    QSignalSpy spy(&fc, &Markoff::FindController::navigationRequested);
    QTest::mouseClick(prevBtn, Qt::LeftButton);
    QCOMPARE(spy.count(), 1);
}
```

- [ ] **Step 2: Run and watch them fail**

Run: `cmake --build build-dev --target tst_findbar -j 8 && QT_QPA_PLATFORM=offscreen ./build-dev/bin/tst_findbar -v2`
Expected: 4 new FAILs.

- [ ] **Step 3: Add `QKeyEvent` include + wire buttons + implement eventFilter**

Add to FindBar.cpp includes:

```cpp
#include <QKeyEvent>
```

In the ctor (after the button setup), add the click connections:

```cpp
    QObject::connect(m_prevButton, &QPushButton::clicked, this, [this]() {
        if (m_controller) m_controller->findPrevious();
    });
    QObject::connect(m_nextButton, &QPushButton::clicked, this, [this]() {
        if (m_controller) m_controller->findNext();
    });
```

Replace the `eventFilter` stub:

```cpp
bool FindBar::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_lineEdit && event->type() == QEvent::KeyPress) {
        auto *ke = static_cast<QKeyEvent*>(event);
        if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) {
            if (m_controller) {
                if (ke->modifiers() & Qt::ShiftModifier) m_controller->findPrevious();
                else                                     m_controller->findNext();
            }
            return true;  // consume — don't let Return cascade
        }
        // Escape handled in Task 6.
    }
    return QFrame::eventFilter(obj, event);
}
```

- [ ] **Step 4: Run and watch them pass**

Run: `cmake --build build-dev --target tst_findbar -j 8 && QT_QPA_PLATFORM=offscreen ./build-dev/bin/tst_findbar -v2`
Expected: all PASS.

- [ ] **Step 5: Commit**

```bash
git add src/editor/FindBar.cpp tests/editor/tst_findbar.cpp
git commit -m "$(cat <<'EOF'
feat(editor): FindBar Return / buttons drive controller navigation

Line-edit Return → findNext, Shift+Return → findPrevious;
Next/Prev buttons same. Events consumed so they don't
cascade to the parent NoteEditorWidget.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 6: Escape key + close button → closeRequested

**Files:**
- Modify: `src/editor/FindBar.cpp`
- Modify: `tests/editor/tst_findbar.cpp`

Wire the close button click and the Escape key in the line edit to emit `closeRequested`. The host owns hide / deactivate.

- [ ] **Step 1: Add failing tests**

Add to the slot list:

```cpp
    void escapeKey_emitsCloseRequested();
    void closeButton_emitsCloseRequested();
```

Implementations:

```cpp
void TstFindBar::escapeKey_emitsCloseRequested()
{
    FindBar bar;
    auto *lineEdit = bar.findChild<QLineEdit*>("findBarLineEdit");
    QSignalSpy spy(&bar, &FindBar::closeRequested);
    QTest::keyClick(lineEdit, Qt::Key_Escape);
    QCOMPARE(spy.count(), 1);
}

void TstFindBar::closeButton_emitsCloseRequested()
{
    FindBar bar;
    auto *closeBtn = bar.findChild<QToolButton*>("findBarClose");
    QSignalSpy spy(&bar, &FindBar::closeRequested);
    QTest::mouseClick(closeBtn, Qt::LeftButton);
    QCOMPARE(spy.count(), 1);
}
```

- [ ] **Step 2: Run and watch them fail**

Run: `cmake --build build-dev --target tst_findbar -j 8 && QT_QPA_PLATFORM=offscreen ./build-dev/bin/tst_findbar -v2`
Expected: 2 new FAILs.

- [ ] **Step 3: Wire close button + Escape handler**

In FindBar.cpp ctor (after the existing button connections), add:

```cpp
    QObject::connect(m_closeButton, &QToolButton::clicked,
                     this, &FindBar::closeRequested);
```

Extend `eventFilter` to handle Escape. The Return handling stays; add the Escape branch:

```cpp
bool FindBar::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_lineEdit && event->type() == QEvent::KeyPress) {
        auto *ke = static_cast<QKeyEvent*>(event);
        if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) {
            if (m_controller) {
                if (ke->modifiers() & Qt::ShiftModifier) m_controller->findPrevious();
                else                                     m_controller->findNext();
            }
            return true;
        }
        if (ke->key() == Qt::Key_Escape) {
            Q_EMIT closeRequested();
            return true;
        }
    }
    return QFrame::eventFilter(obj, event);
}
```

- [ ] **Step 4: Run and watch them pass**

Run: `cmake --build build-dev --target tst_findbar -j 8 && QT_QPA_PLATFORM=offscreen ./build-dev/bin/tst_findbar -v2`
Expected: all PASS.

- [ ] **Step 5: Commit**

```bash
git add src/editor/FindBar.cpp tests/editor/tst_findbar.cpp
git commit -m "$(cat <<'EOF'
feat(editor): FindBar Esc / close button emit closeRequested

The widget never hides itself; host owns lifecycle. Esc
in the line edit and the close button both emit
closeRequested for NoteEditorWidget to translate into
controller.deactivate() + hide.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 7: NoteEditorWidget hosts FindBar — `showFindBar()` / `hideFindBar()`

**Files:**
- Modify: `src/editor/NoteEditorWidget.h`
- Modify: `src/editor/NoteEditorWidget.cpp`

Construct one FindBar per NoteEditorWidget, dock it below the editor stack, hidden by default. Provide public `showFindBar()` / `hideFindBar()`.

This task has no new unit test — integration is exercised by manual dogfood (see Task 10) and the existing widget tests already cover NoteEditorWidget's other behaviors. Use the existing `tst_findbar` to verify nothing regresses.

- [ ] **Step 1: Modify `src/editor/NoteEditorWidget.h`**

In the forward-declares (near top), add:

```cpp
namespace Corbomite { class FindBar; }
```

In the public method block, add (next to `setNoteDocument`):

```cpp
    void showFindBar();
    void hideFindBar();
    bool isFindBarVisible() const;
```

In the private members block, add:

```cpp
    FindBar *m_findBar = nullptr;
```

- [ ] **Step 2: Modify `src/editor/NoteEditorWidget.cpp`**

Add includes at the top:

```cpp
#include "FindBar.h"
#include <markoff/core/FindController.h>
```

In the ctor, after `layout->addWidget(m_stack);`, add:

```cpp
    m_findBar = new FindBar(this);
    m_findBar->hide();
    layout->addWidget(m_findBar);
    QObject::connect(m_findBar, &FindBar::closeRequested,
                     this, &NoteEditorWidget::hideFindBar);
```

Add the three new methods at the end of the file (still inside `namespace Corbomite`):

```cpp
void NoteEditorWidget::showFindBar()
{
    if (!m_doc) return;
    auto *fc = m_doc->findController();
    m_findBar->setController(fc);
    if (auto *leaf = activeLeaf()) {
        // Use the polymorphic attach hook present on both Live::EditorWidget
        // and Source::Editor. Symmetric API; no leaf-type switch needed.
        if (auto *live = qobject_cast<Markoff::Live::EditorWidget*>(leaf))
            live->attachFindController(fc);
        else if (auto *src = qobject_cast<Markoff::Source::Editor*>(leaf))
            src->attachFindController(fc);
    }
    fc->activate();
    m_findBar->show();
    m_findBar->focusLineEdit();
}

void NoteEditorWidget::hideFindBar()
{
    if (m_doc) {
        if (auto *leaf = activeLeaf()) {
            if (auto *live = qobject_cast<Markoff::Live::EditorWidget*>(leaf))
                live->detachFindController();
            else if (auto *src = qobject_cast<Markoff::Source::Editor*>(leaf))
                src->detachFindController();
        }
        m_doc->findController()->deactivate();
    }
    m_findBar->hide();
    if (auto *leaf = activeLeaf()) leaf->setFocus();
}

bool NoteEditorWidget::isFindBarVisible() const
{
    return m_findBar && m_findBar->isVisible();
}
```

- [ ] **Step 3: Build**

Run: `cmake --build build-dev --target Corbomite -j 8`
Expected: success.

- [ ] **Step 4: Run all tests, confirm no regressions**

Run: `cd build-dev && ctest --output-on-failure -j 8`
Expected: full test suite passes (or only pre-existing failures remain).

- [ ] **Step 5: Commit**

```bash
git add src/editor/NoteEditorWidget.h src/editor/NoteEditorWidget.cpp
git commit -m "$(cat <<'EOF'
feat(editor): NoteEditorWidget hosts FindBar

One FindBar per editor widget, docked at the bottom of the
layout, hidden by default. showFindBar() lazily wires the
NoteDocument's FindController, attaches to the active leaf
via the symmetric attachFindController hook, activates, and
focuses the line edit. hideFindBar() reverses the sequence.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 8: Leaf-swap rewire in `setViewMode`

**Files:**
- Modify: `src/editor/NoteEditorWidget.cpp`

When the user switches between Live and Source while the FindBar is open, detach the controller from the outgoing leaf and attach it to the incoming leaf so highlights re-render and Next/Prev keep working.

- [ ] **Step 1: Locate the existing `setViewMode` (around line 250 per the earlier scout)**

It currently:
1. Detaches outgoing leaf via `leaf->setDocument(nullptr)`.
2. Switches the stack index.
3. Attaches incoming leaf via `leaf->setDocument(m_doc->markoff())`.

We add FindController re-attach in step 1 (before setDocument(nullptr)) and step 3 (after setDocument).

- [ ] **Step 2: Modify `setViewMode` in NoteEditorWidget.cpp**

Inside `setViewMode`, just before `leaf->setDocument(nullptr)`, detach the controller if the bar is visible:

```cpp
    if (isFindBarVisible() && m_doc) {
        if (auto *leaf = activeLeaf()) {
            if (auto *live = qobject_cast<Markoff::Live::EditorWidget*>(leaf))
                live->detachFindController();
            else if (auto *src = qobject_cast<Markoff::Source::Editor*>(leaf))
                src->detachFindController();
        }
    }
```

Then after the incoming `leaf->setDocument(m_doc->markoff())` block, re-attach:

```cpp
    if (isFindBarVisible() && m_doc) {
        auto *fc = m_doc->findController();
        if (auto *leaf = activeLeaf()) {
            if (auto *live = qobject_cast<Markoff::Live::EditorWidget*>(leaf))
                live->attachFindController(fc);
            else if (auto *src = qobject_cast<Markoff::Source::Editor*>(leaf))
                src->attachFindController(fc);
        }
    }
```

- [ ] **Step 3: Build**

Run: `cmake --build build-dev --target Corbomite -j 8`
Expected: success.

- [ ] **Step 4: Run all tests**

Run: `cd build-dev && ctest --output-on-failure -j 8`
Expected: no new failures.

- [ ] **Step 5: Commit**

```bash
git add src/editor/NoteEditorWidget.cpp
git commit -m "$(cat <<'EOF'
feat(editor): NoteEditorWidget rewires FindController on view-mode swap

If the find bar is visible during setViewMode, detach the
controller from the outgoing leaf and reattach to the
incoming one so the user sees their search highlights
re-render in the new mode and Next/Prev keep working.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 9: MainWindow `Find` / `FindNext` / `FindPrev` action restoration

**Files:**
- Modify: `src/app/MainWindow.h`
- Modify: `src/app/MainWindow.cpp`

Restore the Ctrl+F / F3 / Shift+F3 standard actions, currently part of the wholly-disabled editor-action registration block.

- [ ] **Step 1: Read `src/app/MainWindow.cpp` to find the disabled action block**

Run: `grep -n 'KStandardAction\|onFind\|action.*find\|TODO(port-foundation' src/app/MainWindow.cpp | head -20`

You'll see existing slots like `onFind` may still exist as stubs. Inspect to know what's present.

- [ ] **Step 2: Add private slots to `src/app/MainWindow.h`**

If `onFind`, `onFindNext`, `onFindPrev` don't already exist in `private Q_SLOTS:`, add them:

```cpp
private Q_SLOTS:
    // ... existing slots ...
    void onFind();
    void onFindNext();
    void onFindPrev();
```

- [ ] **Step 3: Add the action wiring and slot bodies in MainWindow.cpp**

Include the needed Markoff header at the top:

```cpp
#include <markoff/core/FindController.h>
```

Add a small helper near the top of the anonymous namespace (or as a private method, your choice — but keep it local since it's only used by three slots). Helper signature:

```cpp
namespace {
// Walk: active editor → its note document → its find controller. Null-safe at
// every step.
Markoff::FindController *activeFindController(MainWindow *self);
} // namespace
```

Implementation (place near the top of the anonymous block at file scope, after the existing helpers):

```cpp
namespace {
Markoff::FindController *activeFindController(MainWindow *self)
{
    auto *editor = self->activeEditor();   // existing accessor; check the .h
    if (!editor) return nullptr;
    auto *noteEditorWidget = editor->editorWidget();
    if (!noteEditorWidget) return nullptr;
    auto *noteDoc = noteEditorWidget->document();  // adapt to existing accessor
    if (!noteDoc) return nullptr;
    return noteDoc->findController();
}
} // namespace
```

If `editor->editorWidget()` or `noteEditorWidget->document()` aren't exactly those accessor names, find the equivalent in `src/editor/MarkdownView.h` and `src/editor/NoteEditorWidget.h` and adapt. The principle is: drill from MainWindow's active editor → NoteEditorWidget → NoteDocument → FindController.

Add the standard-action wiring inside `setupActions()` (or wherever existing `KStandardAction` calls live; search for `KStandardAction::` to find the spot):

```cpp
    KStandardAction::find(this, &MainWindow::onFind, ac);
    KStandardAction::findNext(this, &MainWindow::onFindNext, ac);
    KStandardAction::findPrev(this, &MainWindow::onFindPrev, ac);
```

Add the slot bodies at the end of MainWindow.cpp (or alongside other on…() slots):

```cpp
void MainWindow::onFind()
{
    auto *editor = activeEditor();
    if (!editor) return;
    auto *neWidget = editor->editorWidget();
    if (!neWidget) return;
    neWidget->showFindBar();
}

void MainWindow::onFindNext()
{
    if (auto *fc = activeFindController(this)) fc->findNext();
}

void MainWindow::onFindPrev()
{
    if (auto *fc = activeFindController(this)) fc->findPrevious();
}
```

(If a stub `onFind()` already exists, REPLACE its body with the above. The disabled `TODO(port-foundation-exploration)` comment can be removed once the slot is functional.)

- [ ] **Step 4: Build**

Run: `cmake --build build-dev --target CorbomiteApp -j 8`
Expected: success.

If linker complains about `activeEditor()`/`editorWidget()`/`document()` symbol mismatches, adjust the helper to use the actual accessor names found in the headers.

- [ ] **Step 5: Run all tests**

Run: `cd build-dev && ctest --output-on-failure -j 8`
Expected: no new failures.

- [ ] **Step 6: Commit**

```bash
git add src/app/MainWindow.h src/app/MainWindow.cpp
git commit -m "$(cat <<'EOF'
feat(app): restore Find / FindNext / FindPrev standard actions

Ctrl+F opens the active NoteEditorWidget's FindBar; F3 /
Shift+F3 dispatch to the active NoteDocument's
FindController. First slice of the wholly-disabled editor-
action registration block to come back; the rest re-lights
per feature port.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 10: Full build + manual smoke

**Files:** none (verification only).

- [ ] **Step 1: Full clean build to verify nothing is broken**

Run: `cmake --build build-dev -j 8`
Expected: success across all targets.

- [ ] **Step 2: Full test suite**

Run: `cd build-dev && ctest --output-on-failure -j 8`
Expected: no new failures vs the pre-port baseline. The new `tst_findbar` is in the suite and passes.

- [ ] **Step 3: Launch Corbomite and dogfood the find UI**

Run: `./build-dev/bin/Corbomite`

Manual checklist (acceptance criteria from the spec):

- [ ] Open a vault; open a note with content.
- [ ] Press **Ctrl+F** — FindBar appears at the bottom of the editor; line edit is focused.
- [ ] Type a needle that exists in the doc — matches highlight in Live mode; count label updates as you type.
- [ ] Press **Return** — view scrolls to next match; focus stays in the line edit.
- [ ] Press **Shift+Return** — view goes to previous match.
- [ ] Type a needle that doesn't exist — count label shows "No matches"; Next/Prev buttons disable.
- [ ] Press **F3** in the editor (with the bar still open or closed) — next match (per KStandardAction::findNext).
- [ ] Press **Esc** — bar hides; controller deactivates; focus returns to the editor.
- [ ] Open the bar, switch from Live to Source mid-search — bar stays open; needle preserved.
- [ ] Close the note (close tab) — no warnings; no crash.

- [ ] **Step 4: If everything works, no commit needed for this task.** If anything fails, capture the symptom in a follow-up dogfood-findings doc at `docs/dogfood/2026-05-20-find-ui-port-findings.md` and surface to the user before adding bandages.

---

## Definition of done

- 9 commits landed on `port/foundation-exploration` (the 9 task commits).
- New test target `tst_findbar` in the build with at least 11 passing slots.
- `Corbomite` and `CorbomiteApp` build clean.
- Full ctest passes (or only pre-existing failures remain).
- Manual dogfood checklist signed off — or follow-up doc filed if regressions found.
- Spec acceptance criteria 1–9 met.

## Self-review

**Spec coverage:**
- "NoteDocument owns FindController, lazy" → Task 1 ✓
- "FindBar QWidget with Okular layout" → Task 2 ✓
- "Search-as-you-type via setNeedle" → Task 3 ✓
- "Count label policy + button enable" → Task 4 ✓
- "Return / Shift+Return + buttons → findNext/findPrevious" → Task 5 ✓
- "Esc + close button emit closeRequested" → Task 6 ✓
- "NoteEditorWidget showFindBar/hideFindBar + leaf attach" → Task 7 ✓
- "View-mode swap reattaches" → Task 8 ✓
- "MainWindow KStandardAction::find/findNext/findPrev" → Task 9 ✓
- "8 unit tests" → Tasks 2–6 (11 slots total, exceeds spec's 8) ✓
- "Manual dogfood" → Task 10 ✓

All spec sections covered.

**Placeholder scan:** None. Every step has concrete code, exact commands, and expected output.

**Type consistency:** `setController`, `controller()`, `focusLineEdit`, `closeRequested`, `findController()` consistent across tasks. Tests reference exact same names as the API.

**One known gap:** Task 9 has a small "adapt to existing accessor" note because I haven't read MainWindow's exact `activeEditor()` / `editorWidget()` shape from this branch. The first build step in Task 9 will surface the right names; small fixup is expected. Flagged in the task body.
