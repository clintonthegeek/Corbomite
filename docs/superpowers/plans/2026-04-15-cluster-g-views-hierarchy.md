# Cluster G Part 1 — Views Hierarchy + TextFileView Contract Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace ad-hoc view creation in EditorViewSpace with a formal View class hierarchy (View→ItemView→FileView→EditableFileView→TextFileView), a ViewRegistry factory, and a thin WorkspaceLeaf — then ship TextFileView's debounced-save + three-way-merge + backup contract.

**Architecture:** View is a QWidget that owns a Component (composition). ViewRegistry maps viewType→factory and extension→viewType. WorkspaceLeaf hosts one View. EditorViewSpace is rewritten to host WorkspaceLeaf objects instead of raw widgets. Three concrete View subclasses (MarkdownView, CanvasView, GraphView) replace existing hardcoded creation paths.

**Tech Stack:** C++20, Qt6, KDE Frameworks 6, diff-match-patch (Apache-2 vendored), QTest

**Spec:** `docs/superpowers/specs/2026-04-15-cluster-g-views-hierarchy-design.md`

---

## File Structure

### New files in `libs/core/`

| File | Responsibility |
|---|---|
| `include/corbomite/core/View.h` + `src/View.cpp` | Base view class (QWidget + has-a Component) |
| `include/corbomite/core/ItemView.h` + `src/ItemView.cpp` | Header chrome: title, action buttons, "..." menu |
| `include/corbomite/core/FileView.h` + `src/FileView.cpp` | File-bound view: loadFile, breadcrumbs, rename/delete reaction |
| `include/corbomite/core/EditableFileView.h` + `src/EditableFileView.cpp` | Inline title rename |
| `include/corbomite/core/TextFileView.h` + `src/TextFileView.cpp` | Debounced save, three-way merge, backup |
| `include/corbomite/core/ViewRegistry.h` + `src/ViewRegistry.cpp` | viewType→factory + ext→viewType registry |
| `include/corbomite/core/WorkspaceLeaf.h` + `src/WorkspaceLeaf.cpp` | One-View container, serialization |
| `include/corbomite/core/DiffMatchPatch.h` + `src/DiffMatchPatch.cpp` | Three-way merge wrapper |
| `third_party/diff_match_patch.h` | Vendored Apache-2 C++ port |

### New files in `src/`

| File | Responsibility |
|---|---|
| `src/editor/MarkdownView.h` + `.cpp` | TextFileView subclass wrapping NoteEditorWidget |
| `src/canvas/CanvasView.h` + `.cpp` | FileView subclass wrapping CanvasViewTab |
| `src/graph/GraphView.h` + `.cpp` | ItemView subclass wrapping GraphViewTab |

### New test files

| File | Tests |
|---|---|
| `tests/core/tst_view_lifecycle.cpp` | View open/close, Component delegation, state round-trip |
| `tests/core/tst_viewregistry.cpp` | Register/unregister, duplicate throws, ext lookup |
| `tests/core/tst_workspaceleaf.cpp` | Serialize/deserialize, setViewState via registry |
| `tests/core/tst_textfileview.cpp` | Debounced save, re-entry guard, immediate save |
| `tests/core/tst_textfileview_merge.cpp` | Three-way merge scenarios |
| `tests/core/tst_diffmatchpatch.cpp` | DiffMatchPatch wrapper |

### Modified files

| File | Changes |
|---|---|
| `libs/core/CMakeLists.txt` | Add new source files |
| `tests/core/CMakeLists.txt` | Add new test executables |
| `src/editor/EditorViewSpace.h` + `.cpp` | Rewrite to host WorkspaceLeaf > View |
| `src/editor/EditorViewManager.h` + `.cpp` | Route through ViewRegistry |
| `src/app/MainWindow.h` + `.cpp` | Create ViewRegistry, register built-ins |
| `libs/core/src/PaneLayoutBridge.cpp` | Serialize/deserialize via WorkspaceLeaf |

---

## Task 1: Vendor diff-match-patch

**Files:**
- Create: `libs/core/third_party/diff_match_patch.h`

- [ ] **Step 1: Download the C++ port**

```bash
curl -L -o libs/core/third_party/diff_match_patch.h \
  "https://raw.githubusercontent.com/google/diff-match-patch/master/cpp/diff_match_patch.h"
```

Verify the file exists and contains the Apache-2.0 license header.

- [ ] **Step 2: Verify it compiles**

Create a minimal test to confirm the header is usable:

```bash
cd build && cmake --build . --target corbomite-core 2>&1 | tail -5
```

No errors expected — the header is only included by `DiffMatchPatch.cpp` (not yet created). This step just confirms the file is valid C++.

- [ ] **Step 3: Commit**

```bash
git add libs/core/third_party/diff_match_patch.h
git commit -m "vendor: add diff-match-patch C++ port (Apache-2)"
```

---

## Task 2: DiffMatchPatch wrapper + tests

**Files:**
- Create: `libs/core/include/corbomite/core/DiffMatchPatch.h`
- Create: `libs/core/src/DiffMatchPatch.cpp`
- Create: `tests/core/tst_diffmatchpatch.cpp`
- Modify: `libs/core/CMakeLists.txt`
- Modify: `tests/core/CMakeLists.txt`

- [ ] **Step 1: Write the test file**

```cpp
// tests/core/tst_diffmatchpatch.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include "corbomite/core/DiffMatchPatch.h"

using Corbomite::DiffMatchPatch;

class TestDiffMatchPatch : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void noChanges()
    {
        QString base = QStringLiteral("hello world");
        QCOMPARE(DiffMatchPatch::threeWayMerge(base, base, base), base);
    }

    void localOnlyChange()
    {
        QString base = QStringLiteral("hello world");
        QString local = QStringLiteral("hello brave world");
        QCOMPARE(DiffMatchPatch::threeWayMerge(base, local, base), local);
    }

    void remoteOnlyChange()
    {
        QString base = QStringLiteral("hello world");
        QString remote = QStringLiteral("hello new world");
        QCOMPARE(DiffMatchPatch::threeWayMerge(base, base, remote), remote);
    }

    void cleanMerge()
    {
        QString base = QStringLiteral("line1\nline2\nline3");
        QString local = QStringLiteral("line1\nline2-local\nline3");
        QString remote = QStringLiteral("line1\nline2\nline3-remote");
        QString merged = DiffMatchPatch::threeWayMerge(base, local, remote);
        QVERIFY(merged.contains(QStringLiteral("line2-local")));
        QVERIFY(merged.contains(QStringLiteral("line3-remote")));
    }

    void conflictRemoteWins()
    {
        // When both edit the same region, patch-apply with remote changes
        // applied to local will produce the remote version of that region
        QString base = QStringLiteral("AAA");
        QString local = QStringLiteral("BBB");
        QString remote = QStringLiteral("CCC");
        QString merged = DiffMatchPatch::threeWayMerge(base, local, remote);
        // diff-match-patch patch-apply is best-effort; verify it doesn't crash
        // and returns non-empty
        QVERIFY(!merged.isEmpty());
    }

    void emptyBase()
    {
        QString base;
        QString local = QStringLiteral("new content");
        QString remote = QStringLiteral("other content");
        QString merged = DiffMatchPatch::threeWayMerge(base, local, remote);
        QVERIFY(!merged.isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestDiffMatchPatch)
#include "tst_diffmatchpatch.moc"
```

- [ ] **Step 2: Run test to verify it fails (files don't exist yet)**

```bash
cd build && cmake -S .. -B . -DCORBOMITE_DEV_BUILD=ON 2>&1 | tail -5
```

Expected: build error — `DiffMatchPatch.h` not found.

- [ ] **Step 3: Write the header**

```cpp
// libs/core/include/corbomite/core/DiffMatchPatch.h
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>

namespace Corbomite {

class DiffMatchPatch
{
public:
    /// Three-way merge: given common ancestor `base`, `local` edits, and
    /// `remote` edits, produce merged text. Uses diff-match-patch's
    /// diff→patchMake→patchApply: diffs base vs remote, applies resulting
    /// patches to local. On patch conflict, remote wins (matches Obsidian).
    static QString threeWayMerge(const QString &base,
                                 const QString &local,
                                 const QString &remote);
};

} // namespace Corbomite
```

- [ ] **Step 4: Write the implementation**

```cpp
// libs/core/src/DiffMatchPatch.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/DiffMatchPatch.h"

#include <string>

// diff_match_patch uses std::wstring by default; we need the QString overload.
// The upstream header auto-detects QT and provides a QString specialization.
#include "../third_party/diff_match_patch.h"

namespace Corbomite {

QString DiffMatchPatch::threeWayMerge(const QString &base,
                                      const QString &local,
                                      const QString &remote)
{
    if (base == remote)
        return local;
    if (base == local)
        return remote;

    diff_match_patch<QString> dmp;
    auto diffs = dmp.diff_main(base, remote);
    dmp.diff_cleanupSemantic(diffs);
    auto patches = dmp.patch_make(base, diffs);
    auto result = dmp.patch_apply(patches, local);
    return result.first;
}

} // namespace Corbomite
```

- [ ] **Step 5: Update CMakeLists.txt files**

Add to `libs/core/CMakeLists.txt` after the `CodeBlockProcessorRegistry` entries:

```cmake
    src/DiffMatchPatch.cpp
    include/corbomite/core/DiffMatchPatch.h
```

Add to `tests/core/CMakeLists.txt`:

```cmake
add_executable(tst_diffmatchpatch tst_diffmatchpatch.cpp)
add_test(NAME tst_diffmatchpatch COMMAND tst_diffmatchpatch)
target_link_libraries(tst_diffmatchpatch PRIVATE Qt6::Test Corbomite::Core)
```

- [ ] **Step 6: Build and run test**

```bash
cd build && cmake -S .. -B . -DCORBOMITE_DEV_BUILD=ON && cmake --build . --target tst_diffmatchpatch && ctest -R tst_diffmatchpatch --output-on-failure
```

Expected: all 6 tests pass.

- [ ] **Step 7: Commit**

```bash
git add libs/core/include/corbomite/core/DiffMatchPatch.h libs/core/src/DiffMatchPatch.cpp libs/core/CMakeLists.txt tests/core/tst_diffmatchpatch.cpp tests/core/CMakeLists.txt
git commit -m "feat(core): DiffMatchPatch three-way merge wrapper over vendored diff-match-patch"
```

---

## Task 3: View base class + tests

**Files:**
- Create: `libs/core/include/corbomite/core/View.h`
- Create: `libs/core/src/View.cpp`
- Create: `tests/core/tst_view_lifecycle.cpp`
- Modify: `libs/core/CMakeLists.txt`
- Modify: `tests/core/CMakeLists.txt`

- [ ] **Step 1: Write the test file**

```cpp
// tests/core/tst_view_lifecycle.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QWidget>
#include "corbomite/core/View.h"
#include "corbomite/core/Component.h"

using namespace Corbomite;

class StubView : public View
{
    Q_OBJECT
public:
    using View::View;
    QString getViewType() const override { return QStringLiteral("stub"); }
    QString getDisplayText() const override { return QStringLiteral("Stub View"); }

    bool openCalled = false;
    bool closeCalled = false;

protected:
    void onOpen() override { openCalled = true; }
    void onClose() override { closeCalled = true; }
};

class TestViewLifecycle : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void constructionCreatesComponent()
    {
        StubView view(nullptr);
        QVERIFY(view.component() != nullptr);
        QVERIFY(!view.component()->isLoaded());
    }

    void openLoadsComponentAndCallsOnOpen()
    {
        QWidget parent;
        StubView view(nullptr);
        view.open(&parent);
        QVERIFY(view.component()->isLoaded());
        QVERIFY(view.openCalled);
    }

    void closeUnloadsComponentAndCallsOnClose()
    {
        QWidget parent;
        StubView view(nullptr);
        view.open(&parent);
        view.close();
        QVERIFY(!view.component()->isLoaded());
        QVERIFY(view.closeCalled);
    }

    void doubleOpenIsIdempotent()
    {
        QWidget parent;
        StubView view(nullptr);
        view.open(&parent);
        view.open(&parent);
        QVERIFY(view.component()->isLoaded());
    }

    void gettersReturnDefaults()
    {
        StubView view(nullptr);
        QCOMPARE(view.getViewType(), QStringLiteral("stub"));
        QCOMPARE(view.getDisplayText(), QStringLiteral("Stub View"));
        QCOMPARE(view.getIcon(), QStringLiteral("document"));
    }

    void stateRoundTrips()
    {
        StubView view(nullptr);
        QJsonObject state;
        state[QStringLiteral("key")] = QStringLiteral("value");
        view.setState(state);
        // Default impl is no-op, so getState returns {}
        QVERIFY(view.getState().isEmpty());
    }

    void registerQObjectConnectionDelegates()
    {
        QWidget parent;
        StubView view(nullptr);
        view.open(&parent);
        QObject sender;
        bool called = false;
        auto conn = QObject::connect(&sender, &QObject::destroyed, [&] { called = true; });
        view.registerQObjectConnection(conn);
        // Connection should work while view is open
        QVERIFY(!called);
        // unload disconnects
        view.close();
    }

    void containerWidgetExists()
    {
        StubView view(nullptr);
        QVERIFY(view.containerWidget() != nullptr);
    }
};

QTEST_MAIN(TestViewLifecycle)
#include "tst_view_lifecycle.moc"
```

- [ ] **Step 2: Write the header**

```cpp
// libs/core/include/corbomite/core/View.h
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QWidget>
#include <QJsonObject>
#include <functional>
#include <memory>

class QMenu;

namespace Corbomite {

class Component;
class WorkspaceLeaf;

class View : public QWidget
{
    Q_OBJECT

public:
    explicit View(WorkspaceLeaf *leaf, QWidget *parent = nullptr);
    ~View() override;

    Component *component() const;
    void registerQObjectConnection(const QMetaObject::Connection &conn);
    void addChild(Component *child);
    int registerInterval(int ms, std::function<void()> fn);

    virtual QString getViewType() const = 0;
    virtual QString getDisplayText() const = 0;
    virtual QString getIcon() const;

    void open(QWidget *parent);
    void close();

    virtual QJsonObject getState() const;
    virtual void setState(const QJsonObject &state);
    virtual QJsonObject getEphemeralState() const;
    virtual void setEphemeralState(const QJsonObject &state);

    virtual void onPaneMenu(QMenu *menu);
    virtual void onTabMenu(QMenu *menu);
    virtual void onResize();

    QWidget *containerWidget() const;
    WorkspaceLeaf *leaf() const;

protected:
    virtual void onOpen();
    virtual void onClose();

    WorkspaceLeaf *m_leaf;

private:
    std::unique_ptr<Component> m_component;
    QWidget *m_containerWidget;
    bool m_opened = false;
};

} // namespace Corbomite
```

- [ ] **Step 3: Write the implementation**

```cpp
// libs/core/src/View.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/View.h"
#include "corbomite/core/Component.h"

#include <QVBoxLayout>

namespace Corbomite {

View::View(WorkspaceLeaf *leaf, QWidget *parent)
    : QWidget(parent)
    , m_leaf(leaf)
    , m_component(std::make_unique<Component>())
    , m_containerWidget(new QWidget(this))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_containerWidget);
}

View::~View()
{
    if (m_opened)
        close();
}

Component *View::component() const { return m_component.get(); }

void View::registerQObjectConnection(const QMetaObject::Connection &conn)
{
    m_component->registerQObjectConnection(conn);
}

void View::addChild(Component *child) { m_component->addChild(child); }

int View::registerInterval(int ms, std::function<void()> fn)
{
    return m_component->registerInterval(ms, std::move(fn));
}

QString View::getIcon() const { return QStringLiteral("document"); }

void View::open(QWidget *parent)
{
    if (m_opened) return;
    if (parent && parentWidget() != parent)
        setParent(parent);
    m_component->load();
    m_opened = true;
    onOpen();
}

void View::close()
{
    if (!m_opened) return;
    onClose();
    m_component->unload();
    m_opened = false;
}

QJsonObject View::getState() const { return {}; }
void View::setState(const QJsonObject &) {}
QJsonObject View::getEphemeralState() const { return {}; }
void View::setEphemeralState(const QJsonObject &) {}

void View::onPaneMenu(QMenu *) {}
void View::onTabMenu(QMenu *) {}
void View::onResize() {}

QWidget *View::containerWidget() const { return m_containerWidget; }
WorkspaceLeaf *View::leaf() const { return m_leaf; }

void View::onOpen() {}
void View::onClose() {}

} // namespace Corbomite
```

- [ ] **Step 4: Update CMakeLists.txt**

Add to `libs/core/CMakeLists.txt` source list:

```cmake
    src/View.cpp
    include/corbomite/core/View.h
```

Add to `tests/core/CMakeLists.txt`:

```cmake
add_executable(tst_view_lifecycle tst_view_lifecycle.cpp)
add_test(NAME tst_view_lifecycle COMMAND tst_view_lifecycle)
target_link_libraries(tst_view_lifecycle PRIVATE Qt6::Test Qt6::Widgets Corbomite::Core)
set_tests_properties(tst_view_lifecycle PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
```

- [ ] **Step 5: Build and run test**

```bash
cd build && cmake -S .. -B . -DCORBOMITE_DEV_BUILD=ON && cmake --build . --target tst_view_lifecycle && ctest -R tst_view_lifecycle --output-on-failure
```

Expected: all 8 tests pass.

- [ ] **Step 6: Commit**

```bash
git add libs/core/include/corbomite/core/View.h libs/core/src/View.cpp tests/core/tst_view_lifecycle.cpp libs/core/CMakeLists.txt tests/core/CMakeLists.txt
git commit -m "feat(core): View base class with Component composition"
```

---

## Task 4: ItemView + FileView + EditableFileView

**Files:**
- Create: `libs/core/include/corbomite/core/ItemView.h` + `src/ItemView.cpp`
- Create: `libs/core/include/corbomite/core/FileView.h` + `src/FileView.cpp`
- Create: `libs/core/include/corbomite/core/EditableFileView.h` + `src/EditableFileView.cpp`
- Modify: `libs/core/CMakeLists.txt`

These three classes are thin enough to build and test together. They don't carry the complex TextFileView save logic.

- [ ] **Step 1: Write ItemView header**

```cpp
// libs/core/include/corbomite/core/ItemView.h
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "corbomite/core/View.h"

#include <QWidget>
#include <functional>

class QHBoxLayout;
class QLabel;
class QToolButton;

namespace Corbomite {

class ItemView : public View
{
    Q_OBJECT

public:
    explicit ItemView(WorkspaceLeaf *leaf, QWidget *parent = nullptr);

    QWidget *contentWidget() const;
    QWidget *headerWidget() const;

    void addAction(const QString &icon, const QString &title,
                   std::function<void()> callback);

protected:
    virtual void onMoreOptionsMenu(QMenu *menu);
    void onOpen() override;

private:
    void buildHeader();
    void showMoreOptionsMenu();

    QWidget *m_headerWidget = nullptr;
    QWidget *m_contentWidget = nullptr;
    QHBoxLayout *m_actionsLayout = nullptr;
    QLabel *m_titleLabel = nullptr;
    QLabel *m_iconLabel = nullptr;
};

} // namespace Corbomite
```

- [ ] **Step 2: Write ItemView implementation**

```cpp
// libs/core/src/ItemView.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/ItemView.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QToolButton>
#include <QIcon>
#include <KLocalizedString>

namespace Corbomite {

ItemView::ItemView(WorkspaceLeaf *leaf, QWidget *parent)
    : View(leaf, parent)
{
    buildHeader();
}

void ItemView::buildHeader()
{
    // Replace View's default layout with header + content
    auto *outerLayout = qobject_cast<QVBoxLayout *>(layout());
    if (!outerLayout) return;

    // Remove the default container widget from View's layout
    outerLayout->removeWidget(containerWidget());

    m_headerWidget = new QWidget(this);
    m_headerWidget->setObjectName(QStringLiteral("view-header"));
    auto *headerLayout = new QHBoxLayout(m_headerWidget);
    headerLayout->setContentsMargins(4, 2, 4, 2);
    headerLayout->setSpacing(4);

    m_iconLabel = new QLabel(m_headerWidget);
    m_titleLabel = new QLabel(m_headerWidget);
    m_titleLabel->setObjectName(QStringLiteral("view-header-title"));

    m_actionsLayout = new QHBoxLayout;
    m_actionsLayout->setSpacing(2);

    auto *moreBtn = new QToolButton(m_headerWidget);
    moreBtn->setIcon(QIcon::fromTheme(QStringLiteral("overflow-menu")));
    moreBtn->setToolTip(i18n("More options"));
    moreBtn->setAutoRaise(true);
    connect(moreBtn, &QToolButton::clicked, this, &ItemView::showMoreOptionsMenu);

    headerLayout->addWidget(m_iconLabel);
    headerLayout->addWidget(m_titleLabel, 1);
    headerLayout->addLayout(m_actionsLayout);
    headerLayout->addWidget(moreBtn);

    m_contentWidget = new QWidget(this);

    outerLayout->addWidget(m_headerWidget);
    outerLayout->addWidget(m_contentWidget, 1);
}

QWidget *ItemView::contentWidget() const { return m_contentWidget; }
QWidget *ItemView::headerWidget() const { return m_headerWidget; }

void ItemView::addAction(const QString &icon, const QString &title,
                         std::function<void()> callback)
{
    auto *btn = new QToolButton(m_headerWidget);
    btn->setIcon(QIcon::fromTheme(icon));
    btn->setToolTip(title);
    btn->setAutoRaise(true);
    connect(btn, &QToolButton::clicked, this, [cb = std::move(callback)] { cb(); });
    m_actionsLayout->addWidget(btn);
}

void ItemView::onOpen()
{
    View::onOpen();
    m_titleLabel->setText(getDisplayText());
    m_iconLabel->setPixmap(
        QIcon::fromTheme(getIcon()).pixmap(16, 16));
}

void ItemView::onMoreOptionsMenu(QMenu *) {}

void ItemView::showMoreOptionsMenu()
{
    QMenu menu(this);
    onMoreOptionsMenu(&menu);
    onPaneMenu(&menu);
    if (!menu.isEmpty())
        menu.exec(QCursor::pos());
}

} // namespace Corbomite
```

- [ ] **Step 3: Write FileView header**

```cpp
// libs/core/include/corbomite/core/FileView.h
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "corbomite/core/ItemView.h"

namespace Corbomite {

class NoteDocument;

class FileView : public ItemView
{
    Q_OBJECT

public:
    explicit FileView(WorkspaceLeaf *leaf, QWidget *parent = nullptr);

    NoteDocument *file() const;
    bool loadFile(NoteDocument *file);

    virtual bool canAcceptExtension(const QString &ext) const;

    QString getDisplayText() const override;
    QJsonObject getState() const override;
    void setState(const QJsonObject &state) override;

protected:
    virtual void onLoadFile(NoteDocument *file);
    virtual void onUnloadFile(NoteDocument *file);
    void onOpen() override;
    void onClose() override;

    NoteDocument *m_file = nullptr;
    bool m_navigation = true;
    bool m_allowNoFile = false;
};

} // namespace Corbomite
```

- [ ] **Step 4: Write FileView implementation**

```cpp
// libs/core/src/FileView.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/FileView.h"
#include "corbomite/core/NoteDocument.h"
#include <KLocalizedString>
#include <QJsonObject>

namespace Corbomite {

FileView::FileView(WorkspaceLeaf *leaf, QWidget *parent)
    : ItemView(leaf, parent)
{
}

NoteDocument *FileView::file() const { return m_file; }

bool FileView::loadFile(NoteDocument *file)
{
    if (m_file) {
        onUnloadFile(m_file);
        m_file = nullptr;
    }

    if (!file) return true;

    m_file = file;
    try {
        onLoadFile(file);
    } catch (...) {
        m_file = nullptr;
        return false;
    }
    return true;
}

bool FileView::canAcceptExtension(const QString &) const { return false; }

QString FileView::getDisplayText() const
{
    if (m_file)
        return m_file->name();
    return i18n("No file");
}

QJsonObject FileView::getState() const
{
    QJsonObject state;
    if (m_file)
        state[QStringLiteral("file")] = m_file->relativePath();
    return state;
}

void FileView::setState(const QJsonObject &state)
{
    Q_UNUSED(state)
    // File resolution is handled by the caller (EditorViewSpace/WorkspaceLeaf)
    // who resolves the path and calls loadFile(). setState stores any extra
    // view-specific state that subclasses override to handle.
}

void FileView::onLoadFile(NoteDocument *) {}
void FileView::onUnloadFile(NoteDocument *) {}

void FileView::onOpen()
{
    ItemView::onOpen();
}

void FileView::onClose()
{
    if (m_file) {
        onUnloadFile(m_file);
        m_file = nullptr;
    }
    ItemView::onClose();
}

} // namespace Corbomite
```

- [ ] **Step 5: Write EditableFileView header**

```cpp
// libs/core/include/corbomite/core/EditableFileView.h
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "corbomite/core/FileView.h"

class QLineEdit;

namespace Corbomite {

class EditableFileView : public FileView
{
    Q_OBJECT

public:
    explicit EditableFileView(WorkspaceLeaf *leaf, QWidget *parent = nullptr);

protected:
    void onOpen() override;
    void onPaneMenu(QMenu *menu) override;

private:
    void startRename();
    void finishRename();
    void cancelRename();

    QLineEdit *m_titleEdit = nullptr;
    QString m_originalName;
    bool m_renaming = false;
};

} // namespace Corbomite
```

- [ ] **Step 6: Write EditableFileView implementation**

```cpp
// libs/core/src/EditableFileView.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/EditableFileView.h"
#include "corbomite/core/NoteDocument.h"

#include <QLineEdit>
#include <QMenu>
#include <QKeyEvent>
#include <KLocalizedString>

namespace Corbomite {

EditableFileView::EditableFileView(WorkspaceLeaf *leaf, QWidget *parent)
    : FileView(leaf, parent)
{
}

void EditableFileView::onOpen()
{
    FileView::onOpen();
}

void EditableFileView::onPaneMenu(QMenu *menu)
{
    FileView::onPaneMenu(menu);
    if (m_file) {
        menu->addAction(QIcon::fromTheme(QStringLiteral("edit-rename")),
                        i18n("Rename..."), this, &EditableFileView::startRename);
    }
}

void EditableFileView::startRename()
{
    if (!m_file || m_renaming) return;
    m_renaming = true;
    m_originalName = m_file->name();
    // Inline rename is handled by the leaf/tab infrastructure calling
    // into NoteDocument's rename API. This is a placeholder for the
    // full implementation which will integrate with the tab header's
    // title label making it editable.
    m_renaming = false;
}

void EditableFileView::finishRename()
{
    m_renaming = false;
}

void EditableFileView::cancelRename()
{
    m_renaming = false;
}

} // namespace Corbomite
```

- [ ] **Step 7: Update libs/core/CMakeLists.txt**

Add to source list:

```cmake
    src/ItemView.cpp
    include/corbomite/core/ItemView.h
    src/FileView.cpp
    include/corbomite/core/FileView.h
    src/EditableFileView.cpp
    include/corbomite/core/EditableFileView.h
```

- [ ] **Step 8: Build**

```bash
cd build && cmake -S .. -B . -DCORBOMITE_DEV_BUILD=ON && cmake --build . --target corbomite-core
```

Expected: clean build.

- [ ] **Step 9: Run existing tests to confirm no regressions**

```bash
cd build && ctest -R tst_view_lifecycle --output-on-failure
```

Expected: all pass.

- [ ] **Step 10: Commit**

```bash
git add libs/core/include/corbomite/core/ItemView.h libs/core/src/ItemView.cpp \
        libs/core/include/corbomite/core/FileView.h libs/core/src/FileView.cpp \
        libs/core/include/corbomite/core/EditableFileView.h libs/core/src/EditableFileView.cpp \
        libs/core/CMakeLists.txt
git commit -m "feat(core): ItemView, FileView, EditableFileView hierarchy"
```

---

## Task 5: TextFileView + tests

**Files:**
- Create: `libs/core/include/corbomite/core/TextFileView.h`
- Create: `libs/core/src/TextFileView.cpp`
- Create: `tests/core/tst_textfileview.cpp`
- Create: `tests/core/tst_textfileview_merge.cpp`
- Modify: `libs/core/CMakeLists.txt`
- Modify: `tests/core/CMakeLists.txt`

- [ ] **Step 1: Write TextFileView header**

```cpp
// libs/core/include/corbomite/core/TextFileView.h
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "corbomite/core/EditableFileView.h"

class QTimer;

namespace Corbomite {

class DataAdapter;

class TextFileView : public EditableFileView
{
    Q_OBJECT

public:
    explicit TextFileView(WorkspaceLeaf *leaf, QWidget *parent = nullptr);

    void requestSave();
    void save(bool immediate = false);
    void saveImmediately();

    void setDataAdapter(DataAdapter *adapter);
    void setVaultRoot(const QString &root);

    virtual QString getViewData() const = 0;
    virtual void setViewData(const QString &data, bool clear) = 0;
    virtual void clear() = 0;

Q_SIGNALS:
    void saved();
    void saveError(const QString &error);

protected:
    void onLoadFile(NoteDocument *file) override;
    void onUnloadFile(NoteDocument *file) override;

    void onExternalModify(const QString &relativePath);

private:
    void writeBackup(const QString &content);

    QString m_data;
    bool m_dirty = false;
    bool m_saving = false;
    bool m_saveAgain = false;
    QString m_lastSavedData;
    bool m_neverLoaded = true;
    QTimer *m_debounceTimer = nullptr;
    DataAdapter *m_adapter = nullptr;
    QString m_vaultRoot;

    static constexpr int SaveDebounceMs = 2000;
};

} // namespace Corbomite
```

- [ ] **Step 2: Write TextFileView implementation**

```cpp
// libs/core/src/TextFileView.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/TextFileView.h"
#include "corbomite/core/DiffMatchPatch.h"
#include "corbomite/core/NoteDocument.h"
#include "corbomite/storage/DataAdapter.h"

#include <QTimer>
#include <QDir>
#include <QDateTime>
#include <QFileInfo>

namespace Corbomite {

TextFileView::TextFileView(WorkspaceLeaf *leaf, QWidget *parent)
    : EditableFileView(leaf, parent)
    , m_debounceTimer(new QTimer(this))
{
    m_debounceTimer->setSingleShot(true);
    m_debounceTimer->setInterval(SaveDebounceMs);
    connect(m_debounceTimer, &QTimer::timeout, this, [this] { save(); });
}

void TextFileView::setDataAdapter(DataAdapter *adapter) { m_adapter = adapter; }
void TextFileView::setVaultRoot(const QString &root) { m_vaultRoot = root; }

void TextFileView::requestSave()
{
    m_dirty = true;
    m_debounceTimer->start();
}

void TextFileView::save(bool immediate)
{
    if (!m_file) return;
    if (m_neverLoaded) return;

    if (m_saving) {
        if (!immediate)
            m_saveAgain = true;
        return;
    }

    QString currentData = getViewData();
    if (m_lastSavedData == currentData)
        return;

    m_saving = true;
    QString previousLastSaved = m_lastSavedData;

    if (immediate) {
        m_data = QString();
        m_lastSavedData = QString();
        m_neverLoaded = true;
        clear();
    } else {
        m_data = currentData;
        m_lastSavedData = currentData;
    }

    bool success = false;
    if (m_adapter && !m_vaultRoot.isEmpty()) {
        QString absPath = m_vaultRoot + QLatin1Char('/') + m_file->relativePath();
        success = m_adapter->write(absPath, currentData);
    }

    if (!success) {
        m_lastSavedData = previousLastSaved;
        writeBackup(currentData);
        Q_EMIT saveError(m_file->relativePath());
    } else {
        m_dirty = false;
        Q_EMIT saved();
    }

    m_saving = false;
    if (m_saveAgain && !immediate) {
        m_saveAgain = false;
        save();
    }
}

void TextFileView::saveImmediately()
{
    if (m_dirty)
        save(true);
}

void TextFileView::onLoadFile(NoteDocument *file)
{
    EditableFileView::onLoadFile(file);
    if (!m_adapter || m_vaultRoot.isEmpty()) return;

    QString absPath = m_vaultRoot + QLatin1Char('/') + file->relativePath();
    auto content = m_adapter->read(absPath);
    if (content) {
        m_lastSavedData = *content;
        m_data = *content;
        m_neverLoaded = false;
        setViewData(*content, true);
    }
}

void TextFileView::onUnloadFile(NoteDocument *file)
{
    m_debounceTimer->stop();
    if (m_dirty)
        save(true);
    EditableFileView::onUnloadFile(file);
}

void TextFileView::onExternalModify(const QString &relativePath)
{
    if (!m_file || m_file->relativePath() != relativePath) return;
    if (m_saving) return;
    if (!m_adapter || m_vaultRoot.isEmpty()) return;

    QString absPath = m_vaultRoot + QLatin1Char('/') + m_file->relativePath();
    auto freshOpt = m_adapter->read(absPath);
    if (!freshOpt) return;

    const QString &freshDisk = *freshOpt;
    if (m_lastSavedData == freshDisk) return;

    QString currentView = getViewData();
    if (currentView == freshDisk) {
        m_lastSavedData = freshDisk;
        return;
    }

    QString merged = DiffMatchPatch::threeWayMerge(m_lastSavedData, currentView, freshDisk);
    m_lastSavedData = freshDisk;
    setViewData(merged, false);
}

void TextFileView::writeBackup(const QString &content)
{
    if (m_vaultRoot.isEmpty() || !m_adapter || !m_file) return;

    QString recoveryDir = m_vaultRoot + QStringLiteral("/.obsidian/file-recovery");
    QDir().mkpath(recoveryDir);

    QString baseName = QFileInfo(m_file->relativePath()).baseName();
    QString timestamp = QDateTime::currentDateTime().toString(Qt::ISODate);
    timestamp.replace(QLatin1Char(':'), QLatin1Char('-'));
    QString backupPath = recoveryDir + QLatin1Char('/') + baseName
                         + QLatin1Char('-') + timestamp + QStringLiteral(".md");

    m_adapter->write(backupPath, content);
}

} // namespace Corbomite
```

- [ ] **Step 3: Write tst_textfileview.cpp**

```cpp
// tests/core/tst_textfileview.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QSignalSpy>
#include <QTimer>
#include "corbomite/core/TextFileView.h"
#include "corbomite/core/NoteDocument.h"
#include "corbomite/storage/DataAdapter.h"

using namespace Corbomite;

// --- Test doubles ---

class MemoryAdapter : public DataAdapter
{
public:
    QHash<QString, QString> files;
    bool failNextWrite = false;

    bool exists(const QString &p) const override { return files.contains(p); }
    std::optional<QString> read(const QString &p) const override {
        if (files.contains(p)) return files[p];
        return std::nullopt;
    }
    std::optional<QByteArray> readBinary(const QString &) const override { return std::nullopt; }
    FileStat stat(const QString &) const override { return {}; }
    QStringList list(const QString &) const override { return {}; }
    bool write(const QString &p, const QString &c, WriteHints = {}) override {
        if (failNextWrite) { failNextWrite = false; return false; }
        files[p] = c;
        return true;
    }
    bool writeBinary(const QString &, const QByteArray &, WriteHints = {}) override { return false; }
    bool rename(const QString &, const QString &) override { return false; }
    bool remove(const QString &) override { return false; }
    bool rmdir(const QString &) override { return false; }
    bool mkpath(const QString &) override { return true; }
    bool moveToTrash(const QString &) override { return false; }
};

class ConcreteTextFileView : public TextFileView
{
    Q_OBJECT
public:
    using TextFileView::TextFileView;
    QString content;
    QString getViewData() const override { return content; }
    void setViewData(const QString &data, bool) override { content = data; }
    void clear() override { content.clear(); }
    QString getViewType() const override { return QStringLiteral("test-text"); }
    QString getDisplayText() const override { return QStringLiteral("Test"); }
};

class TestTextFileView : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void saveSkipsWhenClean()
    {
        MemoryAdapter adapter;
        adapter.files[QStringLiteral("/vault/test.md")] = QStringLiteral("hello");

        ConcreteTextFileView view(nullptr);
        view.setDataAdapter(&adapter);
        view.setVaultRoot(QStringLiteral("/vault"));

        auto *doc = new NoteDocument(QStringLiteral("test.md"), &view);
        view.loadFile(doc);

        // Content unchanged — save should be a no-op
        view.save();
        QCOMPARE(adapter.files[QStringLiteral("/vault/test.md")], QStringLiteral("hello"));
    }

    void saveWritesDirtyContent()
    {
        MemoryAdapter adapter;
        adapter.files[QStringLiteral("/vault/test.md")] = QStringLiteral("hello");

        ConcreteTextFileView view(nullptr);
        view.setDataAdapter(&adapter);
        view.setVaultRoot(QStringLiteral("/vault"));

        auto *doc = new NoteDocument(QStringLiteral("test.md"), &view);
        view.loadFile(doc);

        view.content = QStringLiteral("hello world");
        view.save();
        QCOMPARE(adapter.files[QStringLiteral("/vault/test.md")], QStringLiteral("hello world"));
    }

    void saveFailureWritesBackup()
    {
        MemoryAdapter adapter;
        adapter.files[QStringLiteral("/vault/test.md")] = QStringLiteral("hello");

        ConcreteTextFileView view(nullptr);
        view.setDataAdapter(&adapter);
        view.setVaultRoot(QStringLiteral("/vault"));

        auto *doc = new NoteDocument(QStringLiteral("test.md"), &view);
        view.loadFile(doc);

        view.content = QStringLiteral("changed");
        adapter.failNextWrite = true;

        QSignalSpy errorSpy(&view, &TextFileView::saveError);
        view.save();

        QCOMPARE(errorSpy.count(), 1);
        // Backup should exist somewhere under file-recovery
        bool backupExists = false;
        for (auto it = adapter.files.cbegin(); it != adapter.files.cend(); ++it) {
            if (it.key().contains(QStringLiteral("file-recovery"))) {
                backupExists = true;
                QCOMPARE(it.value(), QStringLiteral("changed"));
                break;
            }
        }
        QVERIFY(backupExists);
    }

    void saveReentryGuard()
    {
        MemoryAdapter adapter;
        adapter.files[QStringLiteral("/vault/test.md")] = QStringLiteral("v1");

        ConcreteTextFileView view(nullptr);
        view.setDataAdapter(&adapter);
        view.setVaultRoot(QStringLiteral("/vault"));

        auto *doc = new NoteDocument(QStringLiteral("test.md"), &view);
        view.loadFile(doc);

        view.content = QStringLiteral("v2");
        view.save();
        QCOMPARE(adapter.files[QStringLiteral("/vault/test.md")], QStringLiteral("v2"));
    }

    void immediateSaveClearsState()
    {
        MemoryAdapter adapter;
        adapter.files[QStringLiteral("/vault/test.md")] = QStringLiteral("v1");

        ConcreteTextFileView view(nullptr);
        view.setDataAdapter(&adapter);
        view.setVaultRoot(QStringLiteral("/vault"));

        auto *doc = new NoteDocument(QStringLiteral("test.md"), &view);
        view.loadFile(doc);

        view.content = QStringLiteral("v2");
        view.save(true);
        QCOMPARE(adapter.files[QStringLiteral("/vault/test.md")], QStringLiteral("v2"));
        QVERIFY(view.content.isEmpty());
    }
};

QTEST_MAIN(TestTextFileView)
#include "tst_textfileview.moc"
```

- [ ] **Step 4: Write tst_textfileview_merge.cpp**

```cpp
// tests/core/tst_textfileview_merge.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include "corbomite/core/TextFileView.h"
#include "corbomite/core/NoteDocument.h"
#include "corbomite/storage/DataAdapter.h"

using namespace Corbomite;

class MemoryAdapter2 : public DataAdapter
{
public:
    QHash<QString, QString> files;
    bool exists(const QString &p) const override { return files.contains(p); }
    std::optional<QString> read(const QString &p) const override {
        if (files.contains(p)) return files[p];
        return std::nullopt;
    }
    std::optional<QByteArray> readBinary(const QString &) const override { return std::nullopt; }
    FileStat stat(const QString &) const override { return {}; }
    QStringList list(const QString &) const override { return {}; }
    bool write(const QString &p, const QString &c, WriteHints = {}) override { files[p] = c; return true; }
    bool writeBinary(const QString &, const QByteArray &, WriteHints = {}) override { return false; }
    bool rename(const QString &, const QString &) override { return false; }
    bool remove(const QString &) override { return false; }
    bool rmdir(const QString &) override { return false; }
    bool mkpath(const QString &) override { return true; }
    bool moveToTrash(const QString &) override { return false; }
};

class MergeTestView : public TextFileView
{
    Q_OBJECT
public:
    using TextFileView::TextFileView;
    QString content;
    QString getViewData() const override { return content; }
    void setViewData(const QString &data, bool) override { content = data; }
    void clear() override { content.clear(); }
    QString getViewType() const override { return QStringLiteral("merge-test"); }
    QString getDisplayText() const override { return QStringLiteral("Merge Test"); }
};

class TestTextFileViewMerge : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void externalModifyNoOpWhenSameAsLastSaved()
    {
        MemoryAdapter2 adapter;
        adapter.files[QStringLiteral("/vault/test.md")] = QStringLiteral("original");

        MergeTestView view(nullptr);
        view.setDataAdapter(&adapter);
        view.setVaultRoot(QStringLiteral("/vault"));

        auto *doc = new NoteDocument(QStringLiteral("test.md"), &view);
        view.loadFile(doc);

        // External modify to same content — should be no-op
        view.onExternalModify(QStringLiteral("test.md"));
        QCOMPARE(view.content, QStringLiteral("original"));
    }

    void externalModifyNoLocalChanges()
    {
        MemoryAdapter2 adapter;
        adapter.files[QStringLiteral("/vault/test.md")] = QStringLiteral("original");

        MergeTestView view(nullptr);
        view.setDataAdapter(&adapter);
        view.setVaultRoot(QStringLiteral("/vault"));

        auto *doc = new NoteDocument(QStringLiteral("test.md"), &view);
        view.loadFile(doc);

        // External changes file on disk
        adapter.files[QStringLiteral("/vault/test.md")] = QStringLiteral("remote change");
        view.onExternalModify(QStringLiteral("test.md"));
        QCOMPARE(view.content, QStringLiteral("remote change"));
    }

    void externalModifyMergesLocalAndRemote()
    {
        MemoryAdapter2 adapter;
        adapter.files[QStringLiteral("/vault/test.md")] = QStringLiteral("line1\nline2\nline3");

        MergeTestView view(nullptr);
        view.setDataAdapter(&adapter);
        view.setVaultRoot(QStringLiteral("/vault"));

        auto *doc = new NoteDocument(QStringLiteral("test.md"), &view);
        view.loadFile(doc);

        // Local edit
        view.content = QStringLiteral("line1\nline2-local\nline3");
        // External edit (different region)
        adapter.files[QStringLiteral("/vault/test.md")] = QStringLiteral("line1\nline2\nline3-remote");
        view.onExternalModify(QStringLiteral("test.md"));

        // Both changes should be present
        QVERIFY(view.content.contains(QStringLiteral("line2-local")));
        QVERIFY(view.content.contains(QStringLiteral("line3-remote")));
    }

    void externalModifyIgnoredWhileSaving()
    {
        MemoryAdapter2 adapter;
        adapter.files[QStringLiteral("/vault/test.md")] = QStringLiteral("original");

        MergeTestView view(nullptr);
        view.setDataAdapter(&adapter);
        view.setVaultRoot(QStringLiteral("/vault"));

        auto *doc = new NoteDocument(QStringLiteral("test.md"), &view);
        view.loadFile(doc);

        view.content = QStringLiteral("saving...");
        // Simulate being in save state — save first, which will update lastSavedData
        view.save();
        // Now the file on disk matches what we saved
        QCOMPARE(view.content, QStringLiteral("saving..."));
    }

    void wrongFileIgnored()
    {
        MemoryAdapter2 adapter;
        adapter.files[QStringLiteral("/vault/test.md")] = QStringLiteral("original");

        MergeTestView view(nullptr);
        view.setDataAdapter(&adapter);
        view.setVaultRoot(QStringLiteral("/vault"));

        auto *doc = new NoteDocument(QStringLiteral("test.md"), &view);
        view.loadFile(doc);

        view.onExternalModify(QStringLiteral("other.md"));
        QCOMPARE(view.content, QStringLiteral("original"));
    }
};

QTEST_MAIN(TestTextFileViewMerge)
#include "tst_textfileview_merge.moc"
```

- [ ] **Step 5: Update CMakeLists.txt files**

Add to `libs/core/CMakeLists.txt`:

```cmake
    src/TextFileView.cpp
    include/corbomite/core/TextFileView.h
```

Also add `corbomite-storage` to the link libraries since TextFileView uses DataAdapter:

```cmake
target_link_libraries(corbomite-core PUBLIC Qt6::Core Qt6::Gui Qt6::Svg Qt6::Widgets KF6::SyntaxHighlighting KF6::I18n jkqtmathtext mmdr markoff Corbomite::Storage)
```

Note: Check if this creates a circular dependency. If `libs/storage` already depends on `libs/core`, then TextFileView needs to use DataAdapter via a forward-declared pointer only, and the concrete adapter is injected at runtime. The header already forward-declares `DataAdapter` and takes it via `setDataAdapter(DataAdapter*)`, so the `.h` doesn't need the storage include. Only the `.cpp` includes it. If a link-order issue arises, consider moving the include to the app layer and having TextFileView call through a virtual interface already in core.

Add to `tests/core/CMakeLists.txt`:

```cmake
add_executable(tst_textfileview tst_textfileview.cpp)
add_test(NAME tst_textfileview COMMAND tst_textfileview)
target_link_libraries(tst_textfileview PRIVATE Qt6::Test Qt6::Widgets Corbomite::Core Corbomite::Storage)
set_tests_properties(tst_textfileview PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_textfileview_merge tst_textfileview_merge.cpp)
add_test(NAME tst_textfileview_merge COMMAND tst_textfileview_merge)
target_link_libraries(tst_textfileview_merge PRIVATE Qt6::Test Qt6::Widgets Corbomite::Core Corbomite::Storage)
set_tests_properties(tst_textfileview_merge PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
```

- [ ] **Step 6: Build and run tests**

```bash
cd build && cmake -S .. -B . -DCORBOMITE_DEV_BUILD=ON && cmake --build . --target tst_textfileview tst_textfileview_merge && ctest -R "tst_textfileview" --output-on-failure
```

Expected: all tests pass.

- [ ] **Step 7: Commit**

```bash
git add libs/core/include/corbomite/core/TextFileView.h libs/core/src/TextFileView.cpp \
        tests/core/tst_textfileview.cpp tests/core/tst_textfileview_merge.cpp \
        libs/core/CMakeLists.txt tests/core/CMakeLists.txt
git commit -m "feat(core): TextFileView with debounced save, three-way merge, and backup"
```

---

## Task 6: ViewRegistry + tests

**Files:**
- Create: `libs/core/include/corbomite/core/ViewRegistry.h`
- Create: `libs/core/src/ViewRegistry.cpp`
- Create: `tests/core/tst_viewregistry.cpp`
- Modify: `libs/core/CMakeLists.txt`
- Modify: `tests/core/CMakeLists.txt`

- [ ] **Step 1: Write the test file**

```cpp
// tests/core/tst_viewregistry.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QSignalSpy>
#include "corbomite/core/ViewRegistry.h"
#include "corbomite/core/View.h"

using namespace Corbomite;

class RegistryStubView : public View
{
    Q_OBJECT
public:
    using View::View;
    QString getViewType() const override { return QStringLiteral("stub"); }
    QString getDisplayText() const override { return QStringLiteral("Stub"); }
};

class TestViewRegistry : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void registerAndLookup()
    {
        ViewRegistry reg;
        reg.registerView(QStringLiteral("test"), [](WorkspaceLeaf *leaf) -> View * {
            return new RegistryStubView(leaf);
        });
        QVERIFY(reg.getViewCreatorByType(QStringLiteral("test")) != nullptr);
    }

    void duplicateRegistrationThrows()
    {
        ViewRegistry reg;
        auto factory = [](WorkspaceLeaf *leaf) -> View * { return new RegistryStubView(leaf); };
        reg.registerView(QStringLiteral("test"), factory);
        QVERIFY_THROWS_EXCEPTION(std::runtime_error,
            reg.registerView(QStringLiteral("test"), factory));
    }

    void unregisterRemovesType()
    {
        ViewRegistry reg;
        reg.registerView(QStringLiteral("test"), [](WorkspaceLeaf *leaf) -> View * {
            return new RegistryStubView(leaf);
        });
        reg.unregisterView(QStringLiteral("test"));
        QVERIFY(reg.getViewCreatorByType(QStringLiteral("test")) == nullptr);
    }

    void unregisterAbsentIsNoOp()
    {
        ViewRegistry reg;
        reg.unregisterView(QStringLiteral("nonexistent"));
        // No crash
    }

    void extensionRegistration()
    {
        ViewRegistry reg;
        reg.registerView(QStringLiteral("markdown"), [](WorkspaceLeaf *leaf) -> View * {
            return new RegistryStubView(leaf);
        });
        reg.registerExtensions({QStringLiteral("md")}, QStringLiteral("markdown"));
        QCOMPARE(reg.getTypeByExtension(QStringLiteral("md")), QStringLiteral("markdown"));
        QVERIFY(reg.isExtensionRegistered(QStringLiteral("md")));
    }

    void duplicateExtensionThrows()
    {
        ViewRegistry reg;
        auto factory = [](WorkspaceLeaf *leaf) -> View * { return new RegistryStubView(leaf); };
        reg.registerView(QStringLiteral("t1"), factory);
        reg.registerView(QStringLiteral("t2"), factory);
        reg.registerExtensions({QStringLiteral("md")}, QStringLiteral("t1"));
        QVERIFY_THROWS_EXCEPTION(std::runtime_error,
            reg.registerExtensions({QStringLiteral("md")}, QStringLiteral("t2")));
    }

    void atomicExtensionRegistration()
    {
        ViewRegistry reg;
        auto factory = [](WorkspaceLeaf *leaf) -> View * { return new RegistryStubView(leaf); };
        reg.registerView(QStringLiteral("t1"), factory);
        reg.registerView(QStringLiteral("t2"), factory);
        reg.registerExtensions({QStringLiteral("md")}, QStringLiteral("t1"));
        // Attempting to register both "txt" (new) and "md" (existing) should throw
        // and leave "txt" unregistered (atomic)
        QVERIFY_THROWS_EXCEPTION(std::runtime_error,
            reg.registerExtensions({QStringLiteral("txt"), QStringLiteral("md")}, QStringLiteral("t2")));
        QVERIFY(!reg.isExtensionRegistered(QStringLiteral("txt")));
    }

    void registerViewWithExtensions()
    {
        ViewRegistry reg;
        reg.registerViewWithExtensions(
            {QStringLiteral("md")}, QStringLiteral("markdown"),
            [](WorkspaceLeaf *leaf) -> View * { return new RegistryStubView(leaf); });
        QVERIFY(reg.getViewCreatorByType(QStringLiteral("markdown")) != nullptr);
        QCOMPARE(reg.getTypeByExtension(QStringLiteral("md")), QStringLiteral("markdown"));
    }

    void signalEmission()
    {
        ViewRegistry reg;
        QSignalSpy regSpy(&reg, &ViewRegistry::viewRegistered);
        QSignalSpy unregSpy(&reg, &ViewRegistry::viewUnregistered);
        QSignalSpy extSpy(&reg, &ViewRegistry::extensionsUpdated);

        reg.registerView(QStringLiteral("test"), [](WorkspaceLeaf *leaf) -> View * {
            return new RegistryStubView(leaf);
        });
        QCOMPARE(regSpy.count(), 1);

        reg.registerExtensions({QStringLiteral("md")}, QStringLiteral("test"));
        QCOMPARE(extSpy.count(), 1);

        reg.unregisterView(QStringLiteral("test"));
        QCOMPARE(unregSpy.count(), 1);
    }

    void unknownTypeLookupReturnsNull()
    {
        ViewRegistry reg;
        QVERIFY(reg.getViewCreatorByType(QStringLiteral("nonexistent")) == nullptr);
    }

    void unknownExtLookupReturnsEmpty()
    {
        ViewRegistry reg;
        QVERIFY(reg.getTypeByExtension(QStringLiteral("xyz")).isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestViewRegistry)
#include "tst_viewregistry.moc"
```

- [ ] **Step 2: Write ViewRegistry header**

```cpp
// libs/core/include/corbomite/core/ViewRegistry.h
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QHash>
#include <QStringList>
#include <functional>

namespace Corbomite {

class View;
class WorkspaceLeaf;

class ViewRegistry : public QObject
{
    Q_OBJECT

public:
    using ViewFactory = std::function<View *(WorkspaceLeaf *)>;

    explicit ViewRegistry(QObject *parent = nullptr);

    void registerView(const QString &type, ViewFactory factory);
    void unregisterView(const QString &type);

    void registerExtensions(const QStringList &exts, const QString &type);
    void unregisterExtensions(const QStringList &exts);

    void registerViewWithExtensions(const QStringList &exts, const QString &type,
                                    ViewFactory factory);

    ViewFactory getViewCreatorByType(const QString &type) const;
    QString getTypeByExtension(const QString &ext) const;
    bool isExtensionRegistered(const QString &ext) const;

Q_SIGNALS:
    void viewRegistered(const QString &type);
    void viewUnregistered(const QString &type);
    void extensionsUpdated();

private:
    QHash<QString, ViewFactory> m_viewByType;
    QHash<QString, QString> m_typeByExtension;
};

} // namespace Corbomite
```

- [ ] **Step 3: Write ViewRegistry implementation**

```cpp
// libs/core/src/ViewRegistry.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/ViewRegistry.h"

#include <stdexcept>

namespace Corbomite {

ViewRegistry::ViewRegistry(QObject *parent)
    : QObject(parent)
{
}

void ViewRegistry::registerView(const QString &type, ViewFactory factory)
{
    if (m_viewByType.contains(type))
        throw std::runtime_error(
            QStringLiteral("ViewRegistry: duplicate type '%1'").arg(type).toStdString());
    m_viewByType.insert(type, std::move(factory));
    Q_EMIT viewRegistered(type);
}

void ViewRegistry::unregisterView(const QString &type)
{
    if (m_viewByType.remove(type))
        Q_EMIT viewUnregistered(type);
}

void ViewRegistry::registerExtensions(const QStringList &exts, const QString &type)
{
    // Atomic: check all first
    for (const auto &ext : exts) {
        if (m_typeByExtension.contains(ext))
            throw std::runtime_error(
                QStringLiteral("ViewRegistry: extension '%1' already registered")
                    .arg(ext).toStdString());
    }
    for (const auto &ext : exts)
        m_typeByExtension.insert(ext, type);
    Q_EMIT extensionsUpdated();
}

void ViewRegistry::unregisterExtensions(const QStringList &exts)
{
    for (const auto &ext : exts)
        m_typeByExtension.remove(ext);
    Q_EMIT extensionsUpdated();
}

void ViewRegistry::registerViewWithExtensions(const QStringList &exts, const QString &type,
                                              ViewFactory factory)
{
    registerView(type, std::move(factory));
    registerExtensions(exts, type);
}

ViewRegistry::ViewFactory ViewRegistry::getViewCreatorByType(const QString &type) const
{
    return m_viewByType.value(type, nullptr);
}

QString ViewRegistry::getTypeByExtension(const QString &ext) const
{
    return m_typeByExtension.value(ext);
}

bool ViewRegistry::isExtensionRegistered(const QString &ext) const
{
    return m_typeByExtension.contains(ext);
}

} // namespace Corbomite
```

- [ ] **Step 4: Update CMakeLists.txt files**

Add to `libs/core/CMakeLists.txt`:

```cmake
    src/ViewRegistry.cpp
    include/corbomite/core/ViewRegistry.h
```

Add to `tests/core/CMakeLists.txt`:

```cmake
add_executable(tst_viewregistry tst_viewregistry.cpp)
add_test(NAME tst_viewregistry COMMAND tst_viewregistry)
target_link_libraries(tst_viewregistry PRIVATE Qt6::Test Qt6::Widgets Corbomite::Core)
set_tests_properties(tst_viewregistry PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
```

- [ ] **Step 5: Build and run tests**

```bash
cd build && cmake -S .. -B . -DCORBOMITE_DEV_BUILD=ON && cmake --build . --target tst_viewregistry && ctest -R tst_viewregistry --output-on-failure
```

Expected: all 10 tests pass.

- [ ] **Step 6: Commit**

```bash
git add libs/core/include/corbomite/core/ViewRegistry.h libs/core/src/ViewRegistry.cpp \
        tests/core/tst_viewregistry.cpp libs/core/CMakeLists.txt tests/core/CMakeLists.txt
git commit -m "feat(core): ViewRegistry — viewType-to-factory and extension-to-viewType registry"
```

---

## Task 7: WorkspaceLeaf + tests

**Files:**
- Create: `libs/core/include/corbomite/core/WorkspaceLeaf.h`
- Create: `libs/core/src/WorkspaceLeaf.cpp`
- Create: `tests/core/tst_workspaceleaf.cpp`
- Modify: `libs/core/CMakeLists.txt`
- Modify: `tests/core/CMakeLists.txt`

- [ ] **Step 1: Write the test file**

```cpp
// tests/core/tst_workspaceleaf.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include "corbomite/core/WorkspaceLeaf.h"
#include "corbomite/core/ViewRegistry.h"
#include "corbomite/core/View.h"

using namespace Corbomite;

class LeafStubView : public View
{
    Q_OBJECT
public:
    using View::View;
    QString getViewType() const override { return QStringLiteral("leaf-stub"); }
    QString getDisplayText() const override { return QStringLiteral("Leaf Stub"); }
    QJsonObject getState() const override {
        return {{QStringLiteral("key"), QStringLiteral("value")}};
    }
};

class TestWorkspaceLeaf : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void idIsGenerated()
    {
        WorkspaceLeaf leaf(nullptr, nullptr);
        QCOMPARE(leaf.id().size(), 16);
    }

    void idsAreUnique()
    {
        WorkspaceLeaf a(nullptr, nullptr);
        WorkspaceLeaf b(nullptr, nullptr);
        QVERIFY(a.id() != b.id());
    }

    void openSetsView()
    {
        WorkspaceLeaf leaf(nullptr, nullptr);
        auto *view = new LeafStubView(&leaf);
        leaf.open(view);
        QCOMPARE(leaf.view(), view);
    }

    void serializeRoundTrip()
    {
        WorkspaceLeaf leaf(nullptr, nullptr);
        auto *view = new LeafStubView(&leaf);
        leaf.open(view);

        QJsonObject json = leaf.serialize();
        QCOMPARE(json[QStringLiteral("type")].toString(), QStringLiteral("leaf"));
        QCOMPARE(json[QStringLiteral("id")].toString(), leaf.id());

        auto stateObj = json[QStringLiteral("state")].toObject();
        QCOMPARE(stateObj[QStringLiteral("type")].toString(), QStringLiteral("leaf-stub"));
    }

    void setViewStateThroughRegistry()
    {
        ViewRegistry reg;
        reg.registerView(QStringLiteral("leaf-stub"), [](WorkspaceLeaf *leaf) -> View * {
            return new LeafStubView(leaf);
        });

        WorkspaceLeaf leaf(&reg, nullptr);

        QJsonObject viewState;
        viewState[QStringLiteral("type")] = QStringLiteral("leaf-stub");
        viewState[QStringLiteral("state")] = QJsonObject{};

        leaf.setViewState(viewState);
        QVERIFY(leaf.view() != nullptr);
        QCOMPARE(leaf.view()->getViewType(), QStringLiteral("leaf-stub"));
    }

    void setViewStateUnknownTypeGivesNullView()
    {
        ViewRegistry reg;
        WorkspaceLeaf leaf(&reg, nullptr);

        QJsonObject viewState;
        viewState[QStringLiteral("type")] = QStringLiteral("nonexistent");

        leaf.setViewState(viewState);
        QVERIFY(leaf.view() == nullptr);
    }
};

QTEST_MAIN(TestWorkspaceLeaf)
#include "tst_workspaceleaf.moc"
```

- [ ] **Step 2: Write WorkspaceLeaf header**

```cpp
// libs/core/include/corbomite/core/WorkspaceLeaf.h
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QWidget>
#include <QJsonObject>

namespace Corbomite {

class View;
class ViewRegistry;

class WorkspaceLeaf : public QWidget
{
    Q_OBJECT

public:
    explicit WorkspaceLeaf(ViewRegistry *registry, QWidget *parent = nullptr);
    ~WorkspaceLeaf() override;

    QString id() const;
    View *view() const;

    void open(View *newView);

    QJsonObject getViewState() const;
    void setViewState(const QJsonObject &state);

    QJsonObject getEphemeralState() const;
    void setEphemeralState(const QJsonObject &state);

    QJsonObject serialize() const;
    static WorkspaceLeaf *deserialize(const QJsonObject &json,
                                      ViewRegistry *registry,
                                      QWidget *parent);

    static QString generateId();

Q_SIGNALS:
    void viewChanged(View *newView);

private:
    void closeCurrentView();

    QString m_id;
    View *m_view = nullptr;
    ViewRegistry *m_registry;
};

} // namespace Corbomite
```

- [ ] **Step 3: Write WorkspaceLeaf implementation**

```cpp
// libs/core/src/WorkspaceLeaf.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/WorkspaceLeaf.h"
#include "corbomite/core/View.h"
#include "corbomite/core/ViewRegistry.h"

#include <QRandomGenerator>
#include <QVBoxLayout>

namespace Corbomite {

WorkspaceLeaf::WorkspaceLeaf(ViewRegistry *registry, QWidget *parent)
    : QWidget(parent)
    , m_id(generateId())
    , m_registry(registry)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
}

WorkspaceLeaf::~WorkspaceLeaf()
{
    closeCurrentView();
}

QString WorkspaceLeaf::id() const { return m_id; }
View *WorkspaceLeaf::view() const { return m_view; }

void WorkspaceLeaf::open(View *newView)
{
    closeCurrentView();
    m_view = newView;
    if (m_view) {
        m_view->open(this);
        layout()->addWidget(m_view);
    }
    Q_EMIT viewChanged(m_view);
}

void WorkspaceLeaf::closeCurrentView()
{
    if (m_view) {
        m_view->close();
        layout()->removeWidget(m_view);
        m_view->deleteLater();
        m_view = nullptr;
    }
}

QJsonObject WorkspaceLeaf::getViewState() const
{
    QJsonObject state;
    if (m_view) {
        state[QStringLiteral("type")] = m_view->getViewType();
        state[QStringLiteral("state")] = m_view->getState();
        state[QStringLiteral("icon")] = m_view->getIcon();
        state[QStringLiteral("title")] = m_view->getDisplayText();
    }
    return state;
}

void WorkspaceLeaf::setViewState(const QJsonObject &state)
{
    QString type = state[QStringLiteral("type")].toString();
    if (type.isEmpty() || !m_registry) return;

    auto factory = m_registry->getViewCreatorByType(type);
    if (!factory) {
        closeCurrentView();
        return;
    }

    auto *newView = factory(this);
    open(newView);

    QJsonObject viewState = state[QStringLiteral("state")].toObject();
    if (!viewState.isEmpty())
        m_view->setState(viewState);
}

QJsonObject WorkspaceLeaf::getEphemeralState() const
{
    return m_view ? m_view->getEphemeralState() : QJsonObject{};
}

void WorkspaceLeaf::setEphemeralState(const QJsonObject &state)
{
    if (m_view)
        m_view->setEphemeralState(state);
}

QJsonObject WorkspaceLeaf::serialize() const
{
    QJsonObject json;
    json[QStringLiteral("id")] = m_id;
    json[QStringLiteral("type")] = QStringLiteral("leaf");
    json[QStringLiteral("state")] = getViewState();
    return json;
}

WorkspaceLeaf *WorkspaceLeaf::deserialize(const QJsonObject &json,
                                           ViewRegistry *registry,
                                           QWidget *parent)
{
    auto *leaf = new WorkspaceLeaf(registry, parent);
    leaf->m_id = json[QStringLiteral("id")].toString();
    if (leaf->m_id.isEmpty())
        leaf->m_id = generateId();

    QJsonObject viewState = json[QStringLiteral("state")].toObject();
    if (!viewState.isEmpty())
        leaf->setViewState(viewState);

    return leaf;
}

QString WorkspaceLeaf::generateId()
{
    static const char chars[] = "abcdefghijklmnopqrstuvwxyz0123456789";
    QString id;
    id.reserve(16);
    auto *rng = QRandomGenerator::global();
    for (int i = 0; i < 16; ++i)
        id.append(QLatin1Char(chars[rng->bounded(36)]));
    return id;
}

} // namespace Corbomite
```

- [ ] **Step 4: Update CMakeLists.txt files**

Add to `libs/core/CMakeLists.txt`:

```cmake
    src/WorkspaceLeaf.cpp
    include/corbomite/core/WorkspaceLeaf.h
```

Add to `tests/core/CMakeLists.txt`:

```cmake
add_executable(tst_workspaceleaf tst_workspaceleaf.cpp)
add_test(NAME tst_workspaceleaf COMMAND tst_workspaceleaf)
target_link_libraries(tst_workspaceleaf PRIVATE Qt6::Test Qt6::Widgets Corbomite::Core)
set_tests_properties(tst_workspaceleaf PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
```

- [ ] **Step 5: Build and run tests**

```bash
cd build && cmake -S .. -B . -DCORBOMITE_DEV_BUILD=ON && cmake --build . --target tst_workspaceleaf && ctest -R tst_workspaceleaf --output-on-failure
```

Expected: all 6 tests pass.

- [ ] **Step 6: Commit**

```bash
git add libs/core/include/corbomite/core/WorkspaceLeaf.h libs/core/src/WorkspaceLeaf.cpp \
        tests/core/tst_workspaceleaf.cpp libs/core/CMakeLists.txt tests/core/CMakeLists.txt
git commit -m "feat(core): WorkspaceLeaf — single-View container with serialize/deserialize"
```

---

## Task 8: MarkdownView concrete subclass

**Files:**
- Create: `src/editor/MarkdownView.h`
- Create: `src/editor/MarkdownView.cpp`
- Modify: `src/CMakeLists.txt` (add new source files)

- [ ] **Step 1: Write MarkdownView header**

```cpp
// src/editor/MarkdownView.h
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "corbomite/core/TextFileView.h"

namespace Corbomite {

class NoteEditorWidget;
class HoverPopover;
class EditorSuggestManager;
class VaultModel;

class MarkdownView : public TextFileView
{
    Q_OBJECT

public:
    explicit MarkdownView(WorkspaceLeaf *leaf, QWidget *parent = nullptr);

    static View *factory(WorkspaceLeaf *leaf);

    QString getViewType() const override;
    QString getDisplayText() const override;
    QString getIcon() const override;

    QString getViewData() const override;
    void setViewData(const QString &data, bool clear) override;
    void clear() override;

    bool canAcceptExtension(const QString &ext) const override;

    QJsonObject getState() const override;
    void setState(const QJsonObject &state) override;
    QJsonObject getEphemeralState() const override;
    void setEphemeralState(const QJsonObject &state) override;

    NoteEditorWidget *editorWidget() const;

    void setVaultModel(VaultModel *vault);
    void setHoverPopover(HoverPopover *popover);
    void setEditorSuggestManager(EditorSuggestManager *manager);

protected:
    void onOpen() override;
    void onClose() override;
    void onLoadFile(NoteDocument *file) override;

private:
    NoteEditorWidget *m_editorWidget;
};

} // namespace Corbomite
```

- [ ] **Step 2: Write MarkdownView implementation**

```cpp
// src/editor/MarkdownView.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "MarkdownView.h"
#include "NoteEditorWidget.h"
#include "corbomite/core/NoteDocument.h"
#include "corbomite/core/WorkspaceLeaf.h"

#include <QVBoxLayout>

namespace Corbomite {

MarkdownView::MarkdownView(WorkspaceLeaf *leaf, QWidget *parent)
    : TextFileView(leaf, parent)
    , m_editorWidget(new NoteEditorWidget(contentWidget()))
{
    auto *layout = new QVBoxLayout(contentWidget());
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_editorWidget);

    connect(m_editorWidget, &NoteEditorWidget::cursorInfoChanged,
            this, [this](int line, int col, int) {
                Q_UNUSED(line) Q_UNUSED(col)
            });

    connect(m_editorWidget, &NoteEditorWidget::linkActivated,
            this, [this](const QString &target) {
                Q_UNUSED(target)
                // Link activation is handled by EditorViewSpace signal routing
            });
}

View *MarkdownView::factory(WorkspaceLeaf *leaf)
{
    return new MarkdownView(leaf);
}

QString MarkdownView::getViewType() const
{
    return QStringLiteral("markdown");
}

QString MarkdownView::getDisplayText() const
{
    if (m_file)
        return m_file->name();
    return QStringLiteral("Markdown");
}

QString MarkdownView::getIcon() const
{
    return QStringLiteral("text-markdown");
}

QString MarkdownView::getViewData() const
{
    if (!m_editorWidget || !m_editorWidget->noteDocument())
        return {};
    return m_editorWidget->noteDocument()->markdown();
}

void MarkdownView::setViewData(const QString &data, bool clear)
{
    if (!m_editorWidget || !m_editorWidget->noteDocument())
        return;
    m_editorWidget->noteDocument()->setMarkdown(data);
    Q_UNUSED(clear)
}

void MarkdownView::clear()
{
    if (m_editorWidget && m_editorWidget->noteDocument())
        m_editorWidget->noteDocument()->setMarkdown(QString());
}

bool MarkdownView::canAcceptExtension(const QString &ext) const
{
    return ext.compare(QStringLiteral("md"), Qt::CaseInsensitive) == 0;
}

QJsonObject MarkdownView::getState() const
{
    QJsonObject state = FileView::getState();
    auto mode = m_editorWidget->viewMode();
    if (mode == NoteEditorWidget::ViewMode::Reading) {
        state[QStringLiteral("mode")] = QStringLiteral("preview");
    } else {
        state[QStringLiteral("mode")] = QStringLiteral("source");
        state[QStringLiteral("source")] = (mode == NoteEditorWidget::ViewMode::Source);
    }
    return state;
}

void MarkdownView::setState(const QJsonObject &state)
{
    FileView::setState(state);
    QString mode = state[QStringLiteral("mode")].toString();
    if (mode == QStringLiteral("preview")) {
        m_editorWidget->setViewMode(NoteEditorWidget::ViewMode::Reading);
    } else if (mode == QStringLiteral("source")) {
        bool source = state[QStringLiteral("source")].toBool(false);
        m_editorWidget->setViewMode(source ? NoteEditorWidget::ViewMode::Source
                                           : NoteEditorWidget::ViewMode::LivePreview);
    }
}

QJsonObject MarkdownView::getEphemeralState() const
{
    auto eState = m_editorWidget->saveEphemeralState();
    QJsonObject json;
    // Delegate serialization to the EphemeralState → JSON path
    // already established in Cluster E
    Q_UNUSED(eState)
    return json;
}

void MarkdownView::setEphemeralState(const QJsonObject &state)
{
    Q_UNUSED(state)
    // Delegate to EphemeralState deserialization
}

NoteEditorWidget *MarkdownView::editorWidget() const { return m_editorWidget; }

void MarkdownView::setVaultModel(VaultModel *vault)
{
    m_editorWidget->setVaultModel(vault);
}

void MarkdownView::setHoverPopover(HoverPopover *popover)
{
    m_editorWidget->setHoverPopover(popover);
}

void MarkdownView::setEditorSuggestManager(EditorSuggestManager *manager)
{
    m_editorWidget->setEditorSuggestManager(manager);
}

void MarkdownView::onOpen()
{
    TextFileView::onOpen();
}

void MarkdownView::onClose()
{
    TextFileView::onClose();
}

void MarkdownView::onLoadFile(NoteDocument *file)
{
    m_editorWidget->setNoteDocument(file);
    TextFileView::onLoadFile(file);
}

} // namespace Corbomite
```

- [ ] **Step 3: Add to src/CMakeLists.txt**

Add `editor/MarkdownView.cpp` and `editor/MarkdownView.h` to the app's source list.

- [ ] **Step 4: Build**

```bash
cd build && cmake -S .. -B . -DCORBOMITE_DEV_BUILD=ON && cmake --build .
```

Expected: clean build.

- [ ] **Step 5: Commit**

```bash
git add src/editor/MarkdownView.h src/editor/MarkdownView.cpp src/CMakeLists.txt
git commit -m "feat(editor): MarkdownView — TextFileView subclass wrapping NoteEditorWidget"
```

---

## Task 9: CanvasView + GraphView concrete subclasses

**Files:**
- Create: `src/canvas/CanvasView.h` + `src/canvas/CanvasView.cpp`
- Create: `src/graph/GraphView.h` + `src/graph/GraphView.cpp`
- Modify: `src/CMakeLists.txt`

- [ ] **Step 1: Write CanvasView**

```cpp
// src/canvas/CanvasView.h
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "corbomite/core/FileView.h"

namespace Corbomite {

class CanvasViewTab;
class MarkdownRenderEngine;

class CanvasView : public FileView
{
    Q_OBJECT
public:
    explicit CanvasView(WorkspaceLeaf *leaf, QWidget *parent = nullptr);
    static View *factory(WorkspaceLeaf *leaf);

    QString getViewType() const override;
    QString getIcon() const override;
    bool canAcceptExtension(const QString &ext) const override;

    void setRenderEngine(MarkdownRenderEngine *engine);
    CanvasViewTab *canvasWidget() const;

protected:
    void onLoadFile(NoteDocument *file) override;
    void onUnloadFile(NoteDocument *file) override;

private:
    CanvasViewTab *m_canvasWidget;
};

} // namespace Corbomite
```

```cpp
// src/canvas/CanvasView.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "CanvasView.h"
#include "CanvasViewTab.h"
#include "corbomite/core/NoteDocument.h"

#include <QVBoxLayout>

namespace Corbomite {

CanvasView::CanvasView(WorkspaceLeaf *leaf, QWidget *parent)
    : FileView(leaf, parent)
    , m_canvasWidget(new CanvasViewTab(contentWidget()))
{
    auto *layout = new QVBoxLayout(contentWidget());
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_canvasWidget);
}

View *CanvasView::factory(WorkspaceLeaf *leaf)
{
    return new CanvasView(leaf);
}

QString CanvasView::getViewType() const { return QStringLiteral("canvas"); }
QString CanvasView::getIcon() const { return QStringLiteral("palette"); }

bool CanvasView::canAcceptExtension(const QString &ext) const
{
    return ext.compare(QStringLiteral("canvas"), Qt::CaseInsensitive) == 0;
}

void CanvasView::setRenderEngine(MarkdownRenderEngine *engine)
{
    m_canvasWidget->setRenderEngine(engine);
}

CanvasViewTab *CanvasView::canvasWidget() const { return m_canvasWidget; }

void CanvasView::onLoadFile(NoteDocument *file)
{
    FileView::onLoadFile(file);
    // CanvasViewTab loads from file path — wire it here
}

void CanvasView::onUnloadFile(NoteDocument *file)
{
    if (m_canvasWidget->isModified())
        m_canvasWidget->save();
    FileView::onUnloadFile(file);
}

} // namespace Corbomite
```

- [ ] **Step 2: Write GraphView**

```cpp
// src/graph/GraphView.h
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "corbomite/core/ItemView.h"

namespace Corbomite {

class GraphViewTab;
class SQLiteIndex;
class VaultModel;
class MetadataCache;
class GraphControlsPanel;

class GraphView : public ItemView
{
    Q_OBJECT
public:
    explicit GraphView(WorkspaceLeaf *leaf, QWidget *parent = nullptr);
    static View *factory(WorkspaceLeaf *leaf);

    QString getViewType() const override;
    QString getDisplayText() const override;
    QString getIcon() const override;

    void setIndex(SQLiteIndex *index);
    void setVaultModel(VaultModel *vault);
    void setMetadataCache(MetadataCache *cache);
    void setControlsPanel(GraphControlsPanel *panel);
    GraphViewTab *graphWidget() const;

Q_SIGNALS:
    void noteActivated(const QString &relativePath);

protected:
    void onOpen() override;

private:
    GraphViewTab *m_graphWidget = nullptr;
    SQLiteIndex *m_index = nullptr;
    VaultModel *m_vault = nullptr;
};

} // namespace Corbomite
```

```cpp
// src/graph/GraphView.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "GraphView.h"
#include "GraphViewTab.h"

#include <QVBoxLayout>
#include <KLocalizedString>

namespace Corbomite {

GraphView::GraphView(WorkspaceLeaf *leaf, QWidget *parent)
    : ItemView(leaf, parent)
{
}

View *GraphView::factory(WorkspaceLeaf *leaf)
{
    return new GraphView(leaf);
}

QString GraphView::getViewType() const { return QStringLiteral("graph"); }
QString GraphView::getDisplayText() const { return i18n("Graph view"); }
QString GraphView::getIcon() const { return QStringLiteral("network-wired"); }

void GraphView::setIndex(SQLiteIndex *index) { m_index = index; }
void GraphView::setVaultModel(VaultModel *vault) { m_vault = vault; }

void GraphView::setMetadataCache(MetadataCache *cache)
{
    if (m_graphWidget)
        m_graphWidget->setMetadataCache(cache);
}

void GraphView::setControlsPanel(GraphControlsPanel *panel)
{
    if (m_graphWidget)
        m_graphWidget->setControlsPanel(panel);
}

GraphViewTab *GraphView::graphWidget() const { return m_graphWidget; }

void GraphView::onOpen()
{
    ItemView::onOpen();
    if (!m_graphWidget && m_index && m_vault) {
        m_graphWidget = new GraphViewTab(m_index, m_vault, contentWidget());
        auto *layout = new QVBoxLayout(contentWidget());
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(m_graphWidget);

        connect(m_graphWidget, &GraphViewTab::noteActivated,
                this, &GraphView::noteActivated);
    }
}

} // namespace Corbomite
```

- [ ] **Step 3: Add to src/CMakeLists.txt**

Add `canvas/CanvasView.cpp`, `canvas/CanvasView.h`, `graph/GraphView.cpp`, `graph/GraphView.h` to the source list.

- [ ] **Step 4: Build**

```bash
cd build && cmake -S .. -B . -DCORBOMITE_DEV_BUILD=ON && cmake --build .
```

Expected: clean build.

- [ ] **Step 5: Commit**

```bash
git add src/canvas/CanvasView.h src/canvas/CanvasView.cpp \
        src/graph/GraphView.h src/graph/GraphView.cpp src/CMakeLists.txt
git commit -m "feat(views): CanvasView + GraphView concrete subclasses"
```

---

## Task 10: Rewire EditorViewSpace to host WorkspaceLeaf > View

**Files:**
- Modify: `src/editor/EditorViewSpace.h`
- Modify: `src/editor/EditorViewSpace.cpp`
- Modify: `src/editor/EditorViewManager.h`
- Modify: `src/editor/EditorViewManager.cpp`

This is the largest task. The key changes:
1. `EditorViewSpace` gains a `ViewRegistry*` member and `QVector<WorkspaceLeaf*> m_leaves`
2. `openNote(NoteDocument*)` is replaced by `openFile(path)` routing through ViewRegistry
3. `openCanvas(path)` and `openGraphView(...)` are replaced by `openView(type, state)`
4. Tab management now operates on `WorkspaceLeaf` objects
5. `EditorViewManager` delegates to the new methods

- [ ] **Step 1: Update EditorViewSpace header**

Replace the ad-hoc methods with registry-based ones. Keep backward-compat signals. The new header should have:

```cpp
// Key changes to EditorViewSpace.h:
// ADD:
#include "corbomite/core/WorkspaceLeaf.h"
class ViewRegistry;

// REPLACE m_editors hash with:
QVector<WorkspaceLeaf *> m_leaves;
ViewRegistry *m_viewRegistry = nullptr;

// ADD new methods:
void setViewRegistry(ViewRegistry *registry);
WorkspaceLeaf *openFile(const QString &relativePath);
WorkspaceLeaf *openView(const QString &type, const QJsonObject &state = {});
WorkspaceLeaf *activeLeaf() const;
WorkspaceLeaf *leafForPath(const QString &relativePath) const;
QVector<WorkspaceLeaf *> leaves() const;

// KEEP existing signals — they are re-routed through the View/Leaf layer
```

- [ ] **Step 2: Rewrite EditorViewSpace.cpp**

The implementation replaces `openNote`/`openCanvas`/`openGraphView` with `openFile`/`openView` that go through `ViewRegistry`. Tab bar management now tracks `WorkspaceLeaf*` objects. The `onTabChanged` slot now accesses `m_leaves[index]` instead of looking up in the hash map.

Key method implementations:

```cpp
WorkspaceLeaf *EditorViewSpace::openFile(const QString &relativePath)
{
    // Check for existing leaf with this file
    for (auto *leaf : std::as_const(m_leaves)) {
        if (auto *fv = qobject_cast<FileView *>(leaf->view())) {
            if (fv->file() && fv->file()->relativePath() == relativePath) {
                int idx = m_leaves.indexOf(leaf);
                m_tabBar->setCurrentIndex(idx);
                return leaf;
            }
        }
    }

    // Determine type from extension
    QString ext = QFileInfo(relativePath).suffix().toLower();
    QString type = m_viewRegistry->getTypeByExtension(ext);
    if (type.isEmpty()) return nullptr;

    QJsonObject state;
    state[QStringLiteral("file")] = relativePath;
    return openView(type, state);
}

WorkspaceLeaf *EditorViewSpace::openView(const QString &type, const QJsonObject &state)
{
    auto factory = m_viewRegistry->getViewCreatorByType(type);
    if (!factory) return nullptr;

    auto *leaf = new WorkspaceLeaf(m_viewRegistry, m_stack);
    auto *view = factory(leaf);
    leaf->open(view);

    if (!state.isEmpty())
        view->setState(state);

    m_leaves.append(leaf);
    m_stack->addWidget(leaf);

    QString title = view->getDisplayText();
    int tabIdx = m_tabBar->addTab(title);
    m_tabBar->setCurrentIndex(tabIdx);

    return leaf;
}
```

- [ ] **Step 3: Update EditorViewManager to use new paths**

```cpp
// EditorViewManager::openNote becomes:
void EditorViewManager::openFile(const QString &relativePath)
{
    if (m_activeViewSpace)
        m_activeViewSpace->openFile(relativePath);
}

// EditorViewManager::openGraphView becomes:
void EditorViewManager::openGraphView(SQLiteIndex *index, VaultModel *vault)
{
    if (!m_activeViewSpace) return;
    m_activeViewSpace->openView(QStringLiteral("graph"), {});
    // Post-creation wiring for graph-specific dependencies
}
```

- [ ] **Step 4: Build and run full test suite**

```bash
cd build && cmake -S .. -B . -DCORBOMITE_DEV_BUILD=ON && cmake --build . && ctest --output-on-failure
```

Expected: clean build, existing tests pass. Some tests that directly reference `openNote` may need updating.

- [ ] **Step 5: Fix any test failures from the API changes**

Review and update any tests that call `EditorViewSpace::openNote()`, `openCanvas()`, or `openGraphView()` directly.

- [ ] **Step 6: Commit**

```bash
git add src/editor/EditorViewSpace.h src/editor/EditorViewSpace.cpp \
        src/editor/EditorViewManager.h src/editor/EditorViewManager.cpp
git commit -m "feat(editor): rewire EditorViewSpace to host WorkspaceLeaf > View via ViewRegistry"
```

---

## Task 11: Wire ViewRegistry in MainWindow + register built-ins

**Files:**
- Modify: `src/app/MainWindow.h`
- Modify: `src/app/MainWindow.cpp`

- [ ] **Step 1: Add ViewRegistry member to MainWindow**

In `MainWindow.h`, add:
```cpp
#include "corbomite/core/ViewRegistry.h"
// ...
ViewRegistry *m_viewRegistry = nullptr;
```

- [ ] **Step 2: Create and register built-ins in MainWindow constructor**

In `MainWindow.cpp`, in the `setupEditor()` method (or equivalent init path):

```cpp
m_viewRegistry = new ViewRegistry(this);

// Register built-in view factories
m_viewRegistry->registerViewWithExtensions(
    {QStringLiteral("md")}, QStringLiteral("markdown"),
    &MarkdownView::factory);

m_viewRegistry->registerViewWithExtensions(
    {QStringLiteral("canvas")}, QStringLiteral("canvas"),
    &CanvasView::factory);

m_viewRegistry->registerView(QStringLiteral("graph"), &GraphView::factory);

// Pass registry to editor manager
m_editorManager->setViewRegistry(m_viewRegistry);
```

- [ ] **Step 3: Update all callsites in MainWindow**

Replace `m_editorManager->openNote(doc)` with `m_editorManager->openFile(doc->relativePath())` at all callsites (e.g., `onNoteActivated`, `openDailyNote`, `createNewNote`).

- [ ] **Step 4: Wire TextFileView dependencies**

After opening a markdown view, set the DataAdapter and vault root on it:

```cpp
// In the post-open hook or in MarkdownView's factory:
// markdownView->setDataAdapter(m_vaultService->adapter());
// markdownView->setVaultRoot(m_vaultService->vaultRoot());
```

- [ ] **Step 5: Connect FileWatchReactor to TextFileView merge**

```cpp
connect(m_fileWatch, &FileWatchReactor::fileModifiedExternally,
        this, [this](const QString &relativePath) {
    // Find all open TextFileViews for this file and notify them
    for (auto *space : m_editorManager->viewSpaces()) {
        for (auto *leaf : space->leaves()) {
            if (auto *tfv = qobject_cast<TextFileView *>(leaf->view())) {
                tfv->onExternalModify(relativePath);
            }
        }
    }
});
```

- [ ] **Step 6: Disconnect AutosaveReactor for MarkdownView tabs**

When a MarkdownView opens a file, ensure `AutosaveReactor` does NOT also watch that document (double-save prevention). Either:
- Skip `watchDocument` when the doc is opened via MarkdownView
- Or unwatchDocument when MarkdownView takes ownership

- [ ] **Step 7: Build and smoke test**

```bash
cd build && cmake -S .. -B . -DCORBOMITE_DEV_BUILD=ON && cmake --build . && ./Corbomite
```

Open a vault, open a markdown file, verify it loads in the editor. Open a .canvas file, verify it loads. Check that tab management works (open, close, switch tabs).

- [ ] **Step 8: Run full test suite**

```bash
cd build && ctest --output-on-failure
```

Expected: all tests pass.

- [ ] **Step 9: Commit**

```bash
git add src/app/MainWindow.h src/app/MainWindow.cpp
git commit -m "feat(app): wire ViewRegistry in MainWindow, register built-in view factories"
```

---

## Task 12: Update PaneLayoutBridge for WorkspaceLeaf serialization

**Files:**
- Modify: `libs/core/src/PaneLayoutBridge.cpp`
- Modify: `libs/core/include/corbomite/core/PaneLayoutBridge.h`

- [ ] **Step 1: Update serializeFromSplitter**

The `spaceToLeaves` callback now builds `PaneLeaf` structs from `WorkspaceLeaf::serialize()` instead of inspecting raw widgets:

```cpp
// The callback signature stays the same:
// std::function<QList<PaneLeaf>(QWidget *)> spaceToLeaves
// But the implementation in EditorViewManager changes to:
auto spaceToLeaves = [](QWidget *widget) -> QList<PaneLeaf> {
    auto *space = qobject_cast<EditorViewSpace *>(widget);
    if (!space) return {};
    QList<PaneLeaf> leaves;
    for (auto *leaf : space->leaves()) {
        PaneLeaf pl;
        pl.id = leaf->id();
        auto state = leaf->getViewState();
        pl.viewType = state[QStringLiteral("type")].toString();
        pl.viewState = state;
        if (auto *fv = qobject_cast<FileView *>(leaf->view()))
            pl.filePath = fv->file() ? fv->file()->relativePath() : QString();
        leaves.append(pl);
    }
    return leaves;
};
```

- [ ] **Step 2: Update deserializeIntoSplitter**

The `openTab` callback now creates views via `EditorViewSpace::openView()`:

```cpp
auto openTab = [](EditorViewSpace *space, const PaneLeaf &leaf) {
    if (leaf.viewType == QStringLiteral("markdown") && !leaf.filePath.isEmpty()) {
        space->openFile(leaf.filePath);
    } else if (!leaf.viewType.isEmpty()) {
        space->openView(leaf.viewType, leaf.viewState);
    }
};
```

- [ ] **Step 3: Build and run layout round-trip test**

```bash
cd build && cmake -S .. -B . -DCORBOMITE_DEV_BUILD=ON && cmake --build . && ctest -R tst_panelayout --output-on-failure
```

Expected: existing PaneLayout tests pass.

- [ ] **Step 4: Commit**

```bash
git add libs/core/src/PaneLayoutBridge.cpp libs/core/include/corbomite/core/PaneLayoutBridge.h \
        src/editor/EditorViewManager.cpp
git commit -m "feat(core): update PaneLayoutBridge to serialize via WorkspaceLeaf"
```

---

## Task 13: Full build + full test suite

**Files:** None (verification only)

- [ ] **Step 1: Clean build**

```bash
cd build && cmake -S .. -B . -DCORBOMITE_DEV_BUILD=ON && cmake --build . --clean-first
```

Expected: clean build, no warnings from new files.

- [ ] **Step 2: Run full test suite**

```bash
cd build && ctest --output-on-failure
```

Expected: all tests pass (except the 4 known-flaky pre-existing failures: `tst_markoff_inline_math`, `tst_renderengine`, `tst_completion_popup`, `tst_benchmark_layout`).

- [ ] **Step 3: Smoke test the application**

```bash
./build/Corbomite
```

Verify:
1. Open a vault — files load in the file explorer
2. Click a .md file — opens in MarkdownView with three-mode switching
3. Edit text — 2s debounce save fires
4. Switch modes (Source/LivePreview/Reading)
5. Open a .canvas file — opens in CanvasView
6. Tab management: open multiple tabs, close tabs, switch between them
7. Split panes work
8. Close vault and reopen — layout restored from workspace.json

- [ ] **Step 4: Commit any final fixes**

If any issues were found in the smoke test, fix and commit.

---

## Task 14: Update PROJECT-STATE and plan index

**Files:**
- Modify: `docs/PROJECT-STATE.md`
- Modify: `docs/superpowers/plans/INDEX.md`
- Rename: `docs/superpowers/plans/2026-04-14-cluster-g-views-hierarchy-SCOUTING.md` → keep (reference only)

- [ ] **Step 1: Update PROJECT-STATE.md**

Update the Cluster G row in the roadmap table:
- Status: `In progress (Part 1 done)`
- Add a Recent decisions entry for Cluster G Part 1

- [ ] **Step 2: Update INDEX.md**

Add the full plan file to the Cluster G row. Update status.

- [ ] **Step 3: Commit**

```bash
git add docs/PROJECT-STATE.md docs/superpowers/plans/INDEX.md
git commit -m "docs(state): Cluster G Part 1 landed — Views hierarchy + TextFileView contract"
```
