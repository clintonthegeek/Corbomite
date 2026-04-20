# Cluster G Part 2 — Workspace Containers + Advanced Tab Features Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace EditorViewManager + EditorViewSpace + PaneLayoutBridge with an Obsidian-compatible workspace container tree (WorkspaceSplit, WorkspaceTabs, Workspace coordinator) and ship per-leaf history, leaf-close undo, deferred-load, popout windows, stacked tabs, and tab pinning with linked-pane groups.

**Architecture:** Phased migration (Approach C). Build new workspace tree classes alongside old ones, migrate EditorViewSpace → WorkspaceTabs, migrate EditorViewManager → Workspace + WorkspaceSplit, delete old classes, then layer advanced features on top. Each phase compiles and runs.

**Tech Stack:** C++20, Qt6, KDE Frameworks 6, QTest

**Spec:** `docs/superpowers/specs/2026-04-15-cluster-g-part2-workspace-design.md`

---

## File Structure

### New files in `libs/core/`

| File | Responsibility |
|---|---|
| `include/corbomite/core/WorkspaceItem.h` + `src/WorkspaceItem.cpp` | Abstract base: id, dimension, parent, serialize dispatch |
| `include/corbomite/core/WorkspaceParent.h` + `src/WorkspaceParent.cpp` | Abstract parent: children vector, add/remove/move |
| `include/corbomite/core/WorkspaceSplit.h` + `src/WorkspaceSplit.cpp` | Split container: direction, QSplitter, dimension↔stretch sync |
| `include/corbomite/core/WorkspaceTabs.h` + `src/WorkspaceTabs.cpp` | Tab container: QTabBar + QStackedWidget, context menu, stacked mode |
| `include/corbomite/core/WorkspaceWindow.h` + `src/WorkspaceWindow.cpp` | Popout window: QWidget+Qt::Window, geometry, reparent-on-close |
| `include/corbomite/core/Workspace.h` + `src/Workspace.cpp` | Coordinator: tree ops, active leaf, undo stack, workspace.json I/O |
| `include/corbomite/core/LeafHistory.h` + `src/LeafHistory.cpp` | Per-leaf back/forward navigation history (cap 20) |

### Enhanced files

| File | Changes |
|---|---|
| `include/corbomite/core/WorkspaceLeaf.h` + `src/WorkspaceLeaf.cpp` | Add history, pinned, group, deferred, cachedIcon/Title, loadIfDeferred, navigate, goBack/Forward |
| `include/corbomite/core/ItemView.h` + `src/ItemView.cpp` | Wire back/forward buttons in header |

### Deleted files (Tasks 9-10)

| File | Replacement |
|---|---|
| `src/editor/EditorViewSpace.h` + `.cpp` | WorkspaceTabs |
| `src/editor/EditorViewManager.h` + `.cpp` | Workspace + WorkspaceSplit |
| `libs/core/include/corbomite/core/PaneLayoutBridge.h` + `src/PaneLayoutBridge.cpp` | Workspace serialize/deserialize |
| `libs/core/include/corbomite/core/PaneLayout.h` + `src/PaneLayout.cpp` | Workspace serialize/deserialize |

### Modified files

| File | Changes |
|---|---|
| `src/app/MainWindow.h` + `.cpp` | Own Workspace instead of EditorViewManager; rewire all signals |
| `libs/core/CMakeLists.txt` | Add new sources, eventually remove deleted sources |
| `tests/core/CMakeLists.txt` | Add new test executables, eventually remove old ones |

### New test files

| File | Tests |
|---|---|
| `tests/core/tst_workspace_tree.cpp` | WorkspaceItem/Parent/Split tree operations |
| `tests/core/tst_workspace_tabs.cpp` | WorkspaceTabs tab ops, stacked mode, pinning sort |
| `tests/core/tst_workspace_serialize.cpp` | Obsidian workspace.json round-trip |
| `tests/core/tst_leaf_history.cpp` | Push/back/forward, cap 20 |
| `tests/core/tst_leaf_undo.cpp` | Close undo stack, cap 10, restore-to-parent |
| `tests/core/tst_workspace_deferred.cpp` | Deferred load, loadIfDeferred, cached icon/title |
| `tests/core/tst_workspace_window.cpp` | Popout lifecycle, reparent on close |

---

## Task 1: LeafHistory type + tests

**Files:**
- Create: `libs/core/include/corbomite/core/LeafHistory.h`
- Create: `libs/core/src/LeafHistory.cpp`
- Create: `tests/core/tst_leaf_history.cpp`
- Modify: `libs/core/CMakeLists.txt`
- Modify: `tests/core/CMakeLists.txt`

- [ ] **Step 1: Write the test file**

```cpp
// tests/core/tst_leaf_history.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QJsonObject>
#include "corbomite/core/LeafHistory.h"

using Corbomite::LeafHistory;
using Corbomite::LeafHistoryEntry;

class TestLeafHistory : public QObject
{
    Q_OBJECT

private:
    LeafHistoryEntry makeEntry(const QString &title)
    {
        return {title, QStringLiteral("document"),
                QJsonObject{{QStringLiteral("file"), title}},
                QJsonObject{}};
    }

private Q_SLOTS:
    void initiallyEmpty()
    {
        LeafHistory h;
        QVERIFY(!h.canGoBack());
        QVERIFY(!h.canGoForward());
    }

    void pushAndGoBack()
    {
        LeafHistory h;
        h.push(makeEntry(QStringLiteral("a")));
        h.push(makeEntry(QStringLiteral("b")));

        QVERIFY(h.canGoBack());
        QVERIFY(!h.canGoForward());

        auto entry = h.goBack(makeEntry(QStringLiteral("c")));
        QCOMPARE(entry.title, QStringLiteral("b"));
        QVERIFY(h.canGoBack());
        QVERIFY(h.canGoForward());
    }

    void goForwardAfterBack()
    {
        LeafHistory h;
        h.push(makeEntry(QStringLiteral("a")));
        h.push(makeEntry(QStringLiteral("b")));

        auto back = h.goBack(makeEntry(QStringLiteral("c")));
        auto fwd = h.goForward(back);
        QCOMPARE(fwd.title, QStringLiteral("c"));
    }

    void pushClearsForward()
    {
        LeafHistory h;
        h.push(makeEntry(QStringLiteral("a")));
        h.push(makeEntry(QStringLiteral("b")));

        h.goBack(makeEntry(QStringLiteral("c")));
        QVERIFY(h.canGoForward());

        h.push(makeEntry(QStringLiteral("d")));
        QVERIFY(!h.canGoForward());
    }

    void capAt20()
    {
        LeafHistory h;
        for (int i = 0; i < 25; ++i)
            h.push(makeEntry(QString::number(i)));

        int count = 0;
        auto current = makeEntry(QStringLiteral("final"));
        while (h.canGoBack()) {
            current = h.goBack(current);
            ++count;
        }
        QCOMPARE(count, 20);
    }

    void serializeRoundTrip()
    {
        LeafHistory h;
        h.push(makeEntry(QStringLiteral("a")));
        h.push(makeEntry(QStringLiteral("b")));

        QJsonObject json = h.serialize();
        LeafHistory h2 = LeafHistory::deserialize(json);

        QVERIFY(h2.canGoBack());
        auto entry = h2.goBack(makeEntry(QStringLiteral("c")));
        QCOMPARE(entry.title, QStringLiteral("b"));
    }

    void emptyBackReturnsInvalid()
    {
        LeafHistory h;
        auto entry = h.goBack(makeEntry(QStringLiteral("x")));
        QVERIFY(entry.title.isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestLeafHistory)
#include "tst_leaf_history.moc"
```

- [ ] **Step 2: Run test to verify it fails**

```bash
cd build && cmake -S .. -B . -DCORBOMITE_DEV_BUILD=ON 2>&1 | tail -5
```

Expected: build error — `LeafHistory.h` not found.

- [ ] **Step 3: Write the header**

```cpp
// libs/core/include/corbomite/core/LeafHistory.h
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QJsonObject>
#include <QString>
#include <QVector>

namespace Corbomite {

struct LeafHistoryEntry {
    QString title;
    QString icon;
    QJsonObject state;
    QJsonObject eState;

    bool isValid() const { return !title.isEmpty(); }
    QJsonObject serialize() const;
    static LeafHistoryEntry deserialize(const QJsonObject &json);
};

class LeafHistory
{
public:
    static constexpr int Cap = 20;

    void push(const LeafHistoryEntry &current);
    LeafHistoryEntry goBack(const LeafHistoryEntry &current);
    LeafHistoryEntry goForward(const LeafHistoryEntry &current);

    bool canGoBack() const;
    bool canGoForward() const;

    QJsonObject serialize() const;
    static LeafHistory deserialize(const QJsonObject &json);

private:
    QVector<LeafHistoryEntry> m_back;
    QVector<LeafHistoryEntry> m_forward;
};

} // namespace Corbomite
```

- [ ] **Step 4: Write the implementation**

```cpp
// libs/core/src/LeafHistory.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/LeafHistory.h"

#include <QJsonArray>

namespace Corbomite {

QJsonObject LeafHistoryEntry::serialize() const
{
    return {{QStringLiteral("title"), title},
            {QStringLiteral("icon"), icon},
            {QStringLiteral("state"), state},
            {QStringLiteral("eState"), eState}};
}

LeafHistoryEntry LeafHistoryEntry::deserialize(const QJsonObject &json)
{
    return {json[QStringLiteral("title")].toString(),
            json[QStringLiteral("icon")].toString(),
            json[QStringLiteral("state")].toObject(),
            json[QStringLiteral("eState")].toObject()};
}

void LeafHistory::push(const LeafHistoryEntry &current)
{
    m_back.append(current);
    if (m_back.size() > Cap)
        m_back.removeFirst();
    m_forward.clear();
}

LeafHistoryEntry LeafHistory::goBack(const LeafHistoryEntry &current)
{
    if (m_back.isEmpty())
        return {};
    auto entry = m_back.takeLast();
    m_forward.append(current);
    if (m_forward.size() > Cap)
        m_forward.removeFirst();
    return entry;
}

LeafHistoryEntry LeafHistory::goForward(const LeafHistoryEntry &current)
{
    if (m_forward.isEmpty())
        return {};
    auto entry = m_forward.takeLast();
    m_back.append(current);
    if (m_back.size() > Cap)
        m_back.removeFirst();
    return entry;
}

bool LeafHistory::canGoBack() const { return !m_back.isEmpty(); }
bool LeafHistory::canGoForward() const { return !m_forward.isEmpty(); }

QJsonObject LeafHistory::serialize() const
{
    QJsonArray back;
    for (const auto &e : m_back)
        back.append(e.serialize());
    QJsonArray fwd;
    for (const auto &e : m_forward)
        fwd.append(e.serialize());
    return {{QStringLiteral("backHistory"), back},
            {QStringLiteral("forwardHistory"), fwd}};
}

LeafHistory LeafHistory::deserialize(const QJsonObject &json)
{
    LeafHistory h;
    for (const auto &v : json[QStringLiteral("backHistory")].toArray())
        h.m_back.append(LeafHistoryEntry::deserialize(v.toObject()));
    for (const auto &v : json[QStringLiteral("forwardHistory")].toArray())
        h.m_forward.append(LeafHistoryEntry::deserialize(v.toObject()));
    return h;
}

} // namespace Corbomite
```

- [ ] **Step 5: Add to CMakeLists.txt**

In `libs/core/CMakeLists.txt`, add `src/LeafHistory.cpp` to the sources list and `include/corbomite/core/LeafHistory.h` to the headers list, alongside the existing WorkspaceLeaf entries.

In `tests/core/CMakeLists.txt`, add:

```cmake
add_executable(tst_leaf_history tst_leaf_history.cpp)
target_link_libraries(tst_leaf_history PRIVATE Qt6::Test Corbomite::Core)
add_test(NAME tst_leaf_history COMMAND tst_leaf_history)
```

- [ ] **Step 6: Build and run tests**

```bash
cd build && cmake -S .. -B . -DCORBOMITE_DEV_BUILD=ON && cmake --build . --target tst_leaf_history && ctest -R tst_leaf_history --output-on-failure
```

Expected: all 7 tests pass.

- [ ] **Step 7: Commit**

```bash
git add libs/core/include/corbomite/core/LeafHistory.h libs/core/src/LeafHistory.cpp \
        tests/core/tst_leaf_history.cpp libs/core/CMakeLists.txt tests/core/CMakeLists.txt
git commit -m "feat(core): add LeafHistory type for per-leaf back/forward navigation"
```

---

## Task 2: WorkspaceItem + WorkspaceParent base classes + tests

**Files:**
- Create: `libs/core/include/corbomite/core/WorkspaceItem.h`
- Create: `libs/core/src/WorkspaceItem.cpp`
- Create: `libs/core/include/corbomite/core/WorkspaceParent.h`
- Create: `libs/core/src/WorkspaceParent.cpp`
- Create: `tests/core/tst_workspace_tree.cpp`
- Modify: `libs/core/CMakeLists.txt`
- Modify: `tests/core/CMakeLists.txt`

- [ ] **Step 1: Write the test file**

```cpp
// tests/core/tst_workspace_tree.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QJsonObject>
#include "corbomite/core/WorkspaceItem.h"
#include "corbomite/core/WorkspaceParent.h"

using Corbomite::WorkspaceItem;
using Corbomite::WorkspaceParent;

// Concrete test subclasses
class TestItem : public WorkspaceItem
{
    Q_OBJECT
public:
    using WorkspaceItem::WorkspaceItem;
    QWidget *widget() override { return nullptr; }
    QJsonObject serialize() const override
    {
        QJsonObject json;
        json[QStringLiteral("id")] = id();
        json[QStringLiteral("type")] = QStringLiteral("test-item");
        return json;
    }
};

class TestParent : public WorkspaceParent
{
    Q_OBJECT
public:
    using WorkspaceParent::WorkspaceParent;
    QWidget *widget() override { return nullptr; }
    QJsonObject serialize() const override
    {
        QJsonObject json;
        json[QStringLiteral("id")] = id();
        json[QStringLiteral("type")] = QStringLiteral("test-parent");
        return json;
    }
};

class TestWorkspaceTree : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void itemHas16CharId()
    {
        TestItem item;
        QCOMPARE(item.id().length(), 16);
    }

    void itemIdIsUnique()
    {
        TestItem a, b;
        QVERIFY(a.id() != b.id());
    }

    void dimensionDefaultsToNull()
    {
        TestItem item;
        QVERIFY(!item.dimension().has_value());
    }

    void setDimension()
    {
        TestItem item;
        item.setDimension(50);
        QCOMPARE(item.dimension().value(), 50);
    }

    void parentChildRelationship()
    {
        TestParent parent;
        auto *child = new TestItem(&parent);
        parent.addChild(child);

        QCOMPARE(parent.childCount(), 1);
        QCOMPARE(parent.childAt(0), child);
        QCOMPARE(child->parentItem(), &parent);
    }

    void removeChild()
    {
        TestParent parent;
        auto *child = new TestItem(&parent);
        parent.addChild(child);
        parent.removeChild(child);

        QCOMPARE(parent.childCount(), 0);
        QVERIFY(child->parentItem() == nullptr);
    }

    void moveChild()
    {
        TestParent parent;
        auto *a = new TestItem(&parent);
        auto *b = new TestItem(&parent);
        auto *c = new TestItem(&parent);
        parent.addChild(a);
        parent.addChild(b);
        parent.addChild(c);

        parent.moveChild(2, 0); // c to front
        QCOMPARE(parent.childAt(0), c);
        QCOMPARE(parent.childAt(1), a);
        QCOMPARE(parent.childAt(2), b);
    }

    void insertChildAtIndex()
    {
        TestParent parent;
        auto *a = new TestItem(&parent);
        auto *b = new TestItem(&parent);
        parent.addChild(a);
        parent.addChild(b, 0); // insert at front

        QCOMPARE(parent.childAt(0), b);
        QCOMPARE(parent.childAt(1), a);
    }

    void removeChildDeletesIfOwned()
    {
        TestParent parent;
        auto *child = new TestItem(&parent);
        parent.addChild(child);

        QPointer<TestItem> guard(child);
        parent.removeChild(child, true);
        QVERIFY(guard.isNull());
    }
};

QTEST_GUILESS_MAIN(TestWorkspaceTree)
#include "tst_workspace_tree.moc"
```

- [ ] **Step 2: Run test to verify it fails**

```bash
cd build && cmake -S .. -B . -DCORBOMITE_DEV_BUILD=ON 2>&1 | tail -5
```

Expected: build error — `WorkspaceItem.h` not found.

- [ ] **Step 3: Write WorkspaceItem header**

```cpp
// libs/core/include/corbomite/core/WorkspaceItem.h
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QJsonObject>
#include <QObject>
#include <QString>
#include <optional>

namespace Corbomite {

class WorkspaceParent;

class WorkspaceItem : public QObject
{
    Q_OBJECT
public:
    explicit WorkspaceItem(QObject *parent = nullptr);
    ~WorkspaceItem() override;

    QString id() const;
    void setId(const QString &id);

    std::optional<int> dimension() const;
    void setDimension(std::optional<int> dim);

    WorkspaceParent *parentItem() const;
    void setParentItem(WorkspaceParent *parent);

    virtual QWidget *widget() = 0;
    virtual QJsonObject serialize() const = 0;

    static QString generateId();

private:
    QString m_id;
    std::optional<int> m_dimension;
    WorkspaceParent *m_parentItem = nullptr;
};

} // namespace Corbomite
```

- [ ] **Step 4: Write WorkspaceItem implementation**

```cpp
// libs/core/src/WorkspaceItem.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/WorkspaceItem.h"
#include <QRandomGenerator>

namespace Corbomite {

WorkspaceItem::WorkspaceItem(QObject *parent)
    : QObject(parent)
    , m_id(generateId())
{
}

WorkspaceItem::~WorkspaceItem() = default;

QString WorkspaceItem::id() const { return m_id; }

void WorkspaceItem::setId(const QString &id) { m_id = id; }

std::optional<int> WorkspaceItem::dimension() const { return m_dimension; }

void WorkspaceItem::setDimension(std::optional<int> dim) { m_dimension = dim; }

WorkspaceParent *WorkspaceItem::parentItem() const { return m_parentItem; }

void WorkspaceItem::setParentItem(WorkspaceParent *parent) { m_parentItem = parent; }

QString WorkspaceItem::generateId()
{
    static const char chars[] = "0123456789abcdef";
    QString result;
    result.reserve(16);
    auto *rng = QRandomGenerator::global();
    for (int i = 0; i < 16; ++i)
        result.append(QLatin1Char(chars[rng->bounded(16)]));
    return result;
}

} // namespace Corbomite
```

- [ ] **Step 5: Write WorkspaceParent header**

```cpp
// libs/core/include/corbomite/core/WorkspaceParent.h
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "corbomite/core/WorkspaceItem.h"
#include <QVector>

namespace Corbomite {

class WorkspaceParent : public WorkspaceItem
{
    Q_OBJECT
public:
    explicit WorkspaceParent(QObject *parent = nullptr);

    int childCount() const;
    WorkspaceItem *childAt(int index) const;
    int indexOf(WorkspaceItem *child) const;
    QVector<WorkspaceItem *> children() const;

    void addChild(WorkspaceItem *child, int index = -1);
    void removeChild(WorkspaceItem *child, bool deleteChild = false);
    void moveChild(int from, int to);

Q_SIGNALS:
    void childAdded(WorkspaceItem *child, int index);
    void childRemoved(WorkspaceItem *child);

protected:
    QVector<WorkspaceItem *> m_children;
};

} // namespace Corbomite
```

- [ ] **Step 6: Write WorkspaceParent implementation**

```cpp
// libs/core/src/WorkspaceParent.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/WorkspaceParent.h"

namespace Corbomite {

WorkspaceParent::WorkspaceParent(QObject *parent)
    : WorkspaceItem(parent)
{
}

int WorkspaceParent::childCount() const { return m_children.size(); }

WorkspaceItem *WorkspaceParent::childAt(int index) const
{
    if (index < 0 || index >= m_children.size())
        return nullptr;
    return m_children.at(index);
}

int WorkspaceParent::indexOf(WorkspaceItem *child) const
{
    return m_children.indexOf(child);
}

QVector<WorkspaceItem *> WorkspaceParent::children() const { return m_children; }

void WorkspaceParent::addChild(WorkspaceItem *child, int index)
{
    if (!child || m_children.contains(child))
        return;

    if (child->parentItem())
        child->parentItem()->removeChild(child);

    child->setParentItem(this);
    if (index < 0 || index >= m_children.size())
        m_children.append(child);
    else
        m_children.insert(index, child);

    Q_EMIT childAdded(child, m_children.indexOf(child));
}

void WorkspaceParent::removeChild(WorkspaceItem *child, bool deleteChild)
{
    if (!child || !m_children.contains(child))
        return;

    m_children.removeOne(child);
    child->setParentItem(nullptr);
    Q_EMIT childRemoved(child);

    if (deleteChild)
        delete child;
}

void WorkspaceParent::moveChild(int from, int to)
{
    if (from < 0 || from >= m_children.size() ||
        to < 0 || to >= m_children.size() || from == to)
        return;

    auto *child = m_children.takeAt(from);
    m_children.insert(to, child);
}

} // namespace Corbomite
```

- [ ] **Step 7: Add to CMakeLists.txt**

In `libs/core/CMakeLists.txt`, add `src/WorkspaceItem.cpp` and `src/WorkspaceParent.cpp` to the sources list, and `include/corbomite/core/WorkspaceItem.h` and `include/corbomite/core/WorkspaceParent.h` to the headers list.

In `tests/core/CMakeLists.txt`, add:

```cmake
add_executable(tst_workspace_tree tst_workspace_tree.cpp)
target_link_libraries(tst_workspace_tree PRIVATE Qt6::Test Corbomite::Core)
add_test(NAME tst_workspace_tree COMMAND tst_workspace_tree)
```

- [ ] **Step 8: Build and run tests**

```bash
cd build && cmake -S .. -B . -DCORBOMITE_DEV_BUILD=ON && cmake --build . --target tst_workspace_tree && ctest -R tst_workspace_tree --output-on-failure
```

Expected: all 9 tests pass.

- [ ] **Step 9: Commit**

```bash
git add libs/core/include/corbomite/core/WorkspaceItem.h libs/core/src/WorkspaceItem.cpp \
        libs/core/include/corbomite/core/WorkspaceParent.h libs/core/src/WorkspaceParent.cpp \
        tests/core/tst_workspace_tree.cpp libs/core/CMakeLists.txt tests/core/CMakeLists.txt
git commit -m "feat(core): add WorkspaceItem + WorkspaceParent base classes for workspace tree"
```

---

## Task 3: Enhance WorkspaceLeaf with history, pinned, group, deferred fields

**Files:**
- Modify: `libs/core/include/corbomite/core/WorkspaceLeaf.h`
- Modify: `libs/core/src/WorkspaceLeaf.cpp`
- Modify: `tests/core/tst_workspaceleaf.cpp` (extend existing tests)

- [ ] **Step 1: Write new tests in the existing test file**

Add these test slots to the existing `TestWorkspaceLeaf` class in `tests/core/tst_workspaceleaf.cpp`:

```cpp
    void pinnedDefaultFalse()
    {
        ViewRegistry registry;
        WorkspaceLeaf leaf(&registry);
        QVERIFY(!leaf.pinned());
    }

    void setPinned()
    {
        ViewRegistry registry;
        WorkspaceLeaf leaf(&registry);
        QSignalSpy spy(&leaf, &WorkspaceLeaf::pinnedChanged);

        leaf.setPinned(true);
        QVERIFY(leaf.pinned());
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().first().toBool(), true);
    }

    void groupDefaultEmpty()
    {
        ViewRegistry registry;
        WorkspaceLeaf leaf(&registry);
        QVERIFY(leaf.group().isEmpty());
    }

    void setGroup()
    {
        ViewRegistry registry;
        WorkspaceLeaf leaf(&registry);
        QSignalSpy spy(&leaf, &WorkspaceLeaf::groupChanged);

        leaf.setGroup(QStringLiteral("group1"));
        QCOMPARE(leaf.group(), QStringLiteral("group1"));
        QCOMPARE(spy.count(), 1);
    }

    void deferredDefaultFalse()
    {
        ViewRegistry registry;
        WorkspaceLeaf leaf(&registry);
        QVERIFY(!leaf.isDeferred());
    }

    void setDeferred()
    {
        ViewRegistry registry;
        WorkspaceLeaf leaf(&registry);
        leaf.setDeferred(true, QStringLiteral("doc-icon"), QStringLiteral("My Note"));
        QVERIFY(leaf.isDeferred());
        QCOMPARE(leaf.cachedIcon(), QStringLiteral("doc-icon"));
        QCOMPARE(leaf.cachedTitle(), QStringLiteral("My Note"));
        QVERIFY(leaf.view() == nullptr);
    }

    void historyAccess()
    {
        ViewRegistry registry;
        WorkspaceLeaf leaf(&registry);
        QVERIFY(!leaf.history().canGoBack());
    }

    void serializeWithNewFields()
    {
        ViewRegistry registry;
        WorkspaceLeaf leaf(&registry);
        leaf.setPinned(true);
        leaf.setGroup(QStringLiteral("g1"));

        QJsonObject json = leaf.serialize();
        QCOMPARE(json[QStringLiteral("pinned")].toBool(), true);
        QCOMPARE(json[QStringLiteral("group")].toString(), QStringLiteral("g1"));
    }

    void deserializeWithNewFields()
    {
        QJsonObject json;
        json[QStringLiteral("id")] = QStringLiteral("abcdef0123456789");
        json[QStringLiteral("type")] = QStringLiteral("leaf");
        json[QStringLiteral("pinned")] = true;
        json[QStringLiteral("group")] = QStringLiteral("g2");
        json[QStringLiteral("state")] = QJsonObject{{QStringLiteral("type"), QStringLiteral("empty")}};

        ViewRegistry registry;
        auto *leaf = WorkspaceLeaf::deserialize(json, &registry, nullptr);
        QVERIFY(leaf != nullptr);
        QVERIFY(leaf->pinned());
        QCOMPARE(leaf->group(), QStringLiteral("g2"));
        delete leaf;
    }
```

Add `#include "corbomite/core/LeafHistory.h"` to the test file includes.

- [ ] **Step 2: Run tests to verify new ones fail**

```bash
cd build && cmake -S .. -B . -DCORBOMITE_DEV_BUILD=ON && cmake --build . --target tst_workspaceleaf 2>&1 | tail -10
```

Expected: build error — `pinned()`, `group()`, `isDeferred()` not found on WorkspaceLeaf.

- [ ] **Step 3: Update WorkspaceLeaf header**

Add these to `libs/core/include/corbomite/core/WorkspaceLeaf.h`, adding `#include "corbomite/core/LeafHistory.h"` to the includes:

```cpp
    // New public methods — add after existing public section
    bool pinned() const;
    void setPinned(bool pinned);

    QString group() const;
    void setGroup(const QString &group);

    bool isDeferred() const;
    void setDeferred(bool deferred, const QString &icon = {}, const QString &title = {});
    void loadIfDeferred();
    QString cachedIcon() const;
    QString cachedTitle() const;

    LeafHistory &history();
    const LeafHistory &history() const;

    qint64 activeTime() const;
    void updateActiveTime();

    void navigate(const QJsonObject &viewState);
    void goBack();
    void goForward();

// New signals — add after existing signals
Q_SIGNALS:
    void pinnedChanged(bool pinned);
    void groupChanged(const QString &group);

// New private members — add after existing private section
private:
    bool m_pinned = false;
    QString m_group;
    bool m_deferred = false;
    QString m_cachedIcon;
    QString m_cachedTitle;
    LeafHistory m_history;
    qint64 m_activeTime = 0;
```

- [ ] **Step 4: Update WorkspaceLeaf implementation**

Add to `libs/core/src/WorkspaceLeaf.cpp`:

```cpp
bool WorkspaceLeaf::pinned() const { return m_pinned; }

void WorkspaceLeaf::setPinned(bool pinned)
{
    if (m_pinned == pinned)
        return;
    m_pinned = pinned;
    Q_EMIT pinnedChanged(pinned);
}

QString WorkspaceLeaf::group() const { return m_group; }

void WorkspaceLeaf::setGroup(const QString &group)
{
    if (m_group == group)
        return;
    m_group = group;
    Q_EMIT groupChanged(group);
}

bool WorkspaceLeaf::isDeferred() const { return m_deferred; }

void WorkspaceLeaf::setDeferred(bool deferred, const QString &icon, const QString &title)
{
    m_deferred = deferred;
    if (deferred) {
        m_cachedIcon = icon;
        m_cachedTitle = title;
    }
}

void WorkspaceLeaf::loadIfDeferred()
{
    if (!m_deferred)
        return;
    m_deferred = false;
    auto state = getViewState();
    setViewState(state);
}

QString WorkspaceLeaf::cachedIcon() const { return m_cachedIcon; }
QString WorkspaceLeaf::cachedTitle() const { return m_cachedTitle; }

LeafHistory &WorkspaceLeaf::history() { return m_history; }
const LeafHistory &WorkspaceLeaf::history() const { return m_history; }

qint64 WorkspaceLeaf::activeTime() const { return m_activeTime; }

void WorkspaceLeaf::updateActiveTime()
{
    m_activeTime = QDateTime::currentMSecsSinceEpoch();
}

void WorkspaceLeaf::navigate(const QJsonObject &viewState)
{
    if (m_view) {
        LeafHistoryEntry entry;
        entry.title = m_view->getDisplayText();
        entry.icon = m_view->getIcon();
        entry.state = getViewState();
        entry.eState = getEphemeralState();
        m_history.push(entry);
    }
    setViewState(viewState);
}

void WorkspaceLeaf::goBack()
{
    if (!m_history.canGoBack() || !m_view)
        return;
    LeafHistoryEntry current{m_view->getDisplayText(), m_view->getIcon(),
                             getViewState(), getEphemeralState()};
    auto entry = m_history.goBack(current);
    if (entry.isValid()) {
        setViewState(entry.state);
        if (m_view)
            m_view->setEphemeralState(entry.eState);
    }
}

void WorkspaceLeaf::goForward()
{
    if (!m_history.canGoForward() || !m_view)
        return;
    LeafHistoryEntry current{m_view->getDisplayText(), m_view->getIcon(),
                             getViewState(), getEphemeralState()};
    auto entry = m_history.goForward(current);
    if (entry.isValid()) {
        setViewState(entry.state);
        if (m_view)
            m_view->setEphemeralState(entry.eState);
    }
}
```

Update `serialize()` to include pinned and group:

```cpp
// In serialize(), after existing fields:
if (m_pinned)
    json[QStringLiteral("pinned")] = true;
if (!m_group.isEmpty())
    json[QStringLiteral("group")] = m_group;
```

Update `deserialize()` to restore pinned and group:

```cpp
// In deserialize(), after restoring id:
leaf->m_pinned = json[QStringLiteral("pinned")].toBool(false);
leaf->m_group = json[QStringLiteral("group")].toString();
```

Add `#include <QDateTime>` to the implementation includes.

- [ ] **Step 5: Build and run tests**

```bash
cd build && cmake -S .. -B . -DCORBOMITE_DEV_BUILD=ON && cmake --build . --target tst_workspaceleaf && ctest -R tst_workspaceleaf --output-on-failure
```

Expected: all tests pass (existing + 9 new).

- [ ] **Step 6: Commit**

```bash
git add libs/core/include/corbomite/core/WorkspaceLeaf.h libs/core/src/WorkspaceLeaf.cpp \
        tests/core/tst_workspaceleaf.cpp
git commit -m "feat(core): enhance WorkspaceLeaf with history, pinned, group, deferred fields"
```

---

## Task 4: WorkspaceSplit + tests

**Files:**
- Create: `libs/core/include/corbomite/core/WorkspaceSplit.h`
- Create: `libs/core/src/WorkspaceSplit.cpp`
- Modify: `tests/core/tst_workspace_tree.cpp` (extend)
- Modify: `libs/core/CMakeLists.txt`

- [ ] **Step 1: Add WorkspaceSplit tests to tst_workspace_tree.cpp**

Add `#include "corbomite/core/WorkspaceSplit.h"` and these test slots:

```cpp
    void splitDefaultHorizontal()
    {
        WorkspaceSplit split;
        QCOMPARE(split.direction(), Qt::Horizontal);
    }

    void splitOwnsQSplitter()
    {
        WorkspaceSplit split;
        QVERIFY(split.widget() != nullptr);
        QVERIFY(qobject_cast<QSplitter *>(split.widget()));
    }

    void splitAddChildUpdatesQSplitter()
    {
        WorkspaceSplit split;
        auto *child = new WorkspaceSplit(&split);
        split.addChild(child);

        auto *splitter = qobject_cast<QSplitter *>(split.widget());
        QCOMPARE(splitter->count(), 1);
    }

    void splitRemoveChildUpdatesQSplitter()
    {
        WorkspaceSplit split;
        auto *child = new WorkspaceSplit(&split);
        split.addChild(child);
        split.removeChild(child, true);

        auto *splitter = qobject_cast<QSplitter *>(split.widget());
        QCOMPARE(splitter->count(), 0);
    }

    void splitSerialize()
    {
        WorkspaceSplit split;
        split.setDirection(Qt::Vertical);
        split.setDimension(60);

        QJsonObject json = split.serialize();
        QCOMPARE(json[QStringLiteral("type")].toString(), QStringLiteral("split"));
        QCOMPARE(json[QStringLiteral("direction")].toString(), QStringLiteral("vertical"));
        QCOMPARE(json[QStringLiteral("dimension")].toInt(), 60);
    }

    void splitDirectionSyncsToQSplitter()
    {
        WorkspaceSplit split;
        split.setDirection(Qt::Vertical);
        auto *splitter = qobject_cast<QSplitter *>(split.widget());
        QCOMPARE(splitter->orientation(), Qt::Vertical);
    }
```

Add `#include <QSplitter>` to the test includes.

- [ ] **Step 2: Write WorkspaceSplit header**

```cpp
// libs/core/include/corbomite/core/WorkspaceSplit.h
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "corbomite/core/WorkspaceParent.h"

class QSplitter;

namespace Corbomite {

class WorkspaceSplit : public WorkspaceParent
{
    Q_OBJECT
public:
    explicit WorkspaceSplit(QObject *parent = nullptr);

    Qt::Orientation direction() const;
    void setDirection(Qt::Orientation dir);

    QWidget *widget() override;
    QJsonObject serialize() const override;

    void addChild(WorkspaceItem *child, int index = -1);
    void removeChild(WorkspaceItem *child, bool deleteChild = false);

    void syncDimensionsFromSplitter();
    void syncDimensionsToSplitter();

private:
    QSplitter *m_splitter;
    Qt::Orientation m_direction = Qt::Horizontal;
};

} // namespace Corbomite
```

- [ ] **Step 3: Write WorkspaceSplit implementation**

```cpp
// libs/core/src/WorkspaceSplit.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/WorkspaceSplit.h"

#include <QJsonArray>
#include <QSplitter>

namespace Corbomite {

WorkspaceSplit::WorkspaceSplit(QObject *parent)
    : WorkspaceParent(parent)
    , m_splitter(new QSplitter)
{
    m_splitter->setOrientation(m_direction);
    m_splitter->setChildrenCollapsible(false);
}

Qt::Orientation WorkspaceSplit::direction() const { return m_direction; }

void WorkspaceSplit::setDirection(Qt::Orientation dir)
{
    m_direction = dir;
    m_splitter->setOrientation(dir);
}

QWidget *WorkspaceSplit::widget() { return m_splitter; }

void WorkspaceSplit::addChild(WorkspaceItem *child, int index)
{
    WorkspaceParent::addChild(child, index);
    if (auto *w = child->widget()) {
        int idx = m_children.indexOf(child);
        m_splitter->insertWidget(idx, w);
    }
    syncDimensionsToSplitter();
}

void WorkspaceSplit::removeChild(WorkspaceItem *child, bool deleteChild)
{
    if (auto *w = child->widget())
        w->setParent(nullptr);
    WorkspaceParent::removeChild(child, deleteChild);
    syncDimensionsToSplitter();
}

void WorkspaceSplit::syncDimensionsFromSplitter()
{
    QList<int> sizes = m_splitter->sizes();
    int total = 0;
    for (int s : sizes)
        total += s;
    if (total == 0)
        return;

    for (int i = 0; i < m_children.size() && i < sizes.size(); ++i) {
        int pct = (sizes[i] * 100) / total;
        m_children[i]->setDimension(pct);
    }
}

void WorkspaceSplit::syncDimensionsToSplitter()
{
    if (m_children.isEmpty())
        return;

    QList<int> sizes;
    bool anySet = false;
    for (auto *child : m_children) {
        int dim = child->dimension().value_or(0);
        if (dim > 0)
            anySet = true;
        sizes.append(dim);
    }

    if (!anySet) {
        int equal = 100 / m_children.size();
        sizes.fill(equal, m_children.size());
    }

    m_splitter->setSizes(sizes);
}

QJsonObject WorkspaceSplit::serialize() const
{
    QJsonObject json;
    json[QStringLiteral("id")] = id();
    json[QStringLiteral("type")] = QStringLiteral("split");
    json[QStringLiteral("direction")] =
        m_direction == Qt::Horizontal ? QStringLiteral("horizontal")
                                      : QStringLiteral("vertical");

    if (dimension().has_value())
        json[QStringLiteral("dimension")] = dimension().value();

    QJsonArray children;
    for (const auto *child : m_children)
        children.append(child->serialize());
    json[QStringLiteral("children")] = children;

    return json;
}

} // namespace Corbomite
```

- [ ] **Step 4: Add to CMakeLists.txt**

In `libs/core/CMakeLists.txt`, add `src/WorkspaceSplit.cpp` to sources and `include/corbomite/core/WorkspaceSplit.h` to headers.

- [ ] **Step 5: Build and run tests**

```bash
cd build && cmake -S .. -B . -DCORBOMITE_DEV_BUILD=ON && cmake --build . --target tst_workspace_tree && ctest -R tst_workspace_tree --output-on-failure
```

Expected: all 15 tests pass (9 original + 6 new).

- [ ] **Step 6: Commit**

```bash
git add libs/core/include/corbomite/core/WorkspaceSplit.h libs/core/src/WorkspaceSplit.cpp \
        tests/core/tst_workspace_tree.cpp libs/core/CMakeLists.txt
git commit -m "feat(core): add WorkspaceSplit container with QSplitter + dimension sync"
```

---

## Task 5: WorkspaceTabs + tests

**Files:**
- Create: `libs/core/include/corbomite/core/WorkspaceTabs.h`
- Create: `libs/core/src/WorkspaceTabs.cpp`
- Create: `tests/core/tst_workspace_tabs.cpp`
- Modify: `libs/core/CMakeLists.txt`
- Modify: `tests/core/CMakeLists.txt`

- [ ] **Step 1: Write the test file**

```cpp
// tests/core/tst_workspace_tabs.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QTabBar>
#include <QSignalSpy>
#include "corbomite/core/WorkspaceTabs.h"
#include "corbomite/core/WorkspaceLeaf.h"
#include "corbomite/core/ViewRegistry.h"

using Corbomite::WorkspaceTabs;
using Corbomite::WorkspaceLeaf;
using Corbomite::ViewRegistry;

class TestWorkspaceTabs : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void addLeafCreatesTab()
    {
        ViewRegistry registry;
        WorkspaceTabs tabs;
        auto *leaf = new WorkspaceLeaf(&registry);
        tabs.addChild(leaf);

        QCOMPARE(tabs.childCount(), 1);
        QCOMPARE(tabs.tabBar()->count(), 1);
    }

    void removeLeafRemovesTab()
    {
        ViewRegistry registry;
        WorkspaceTabs tabs;
        auto *leaf = new WorkspaceLeaf(&registry);
        tabs.addChild(leaf);
        tabs.removeChild(leaf, true);

        QCOMPARE(tabs.childCount(), 0);
        QCOMPARE(tabs.tabBar()->count(), 0);
    }

    void currentTabTracking()
    {
        ViewRegistry registry;
        WorkspaceTabs tabs;
        auto *a = new WorkspaceLeaf(&registry);
        auto *b = new WorkspaceLeaf(&registry);
        tabs.addChild(a);
        tabs.addChild(b);

        tabs.setCurrentTab(1);
        QCOMPARE(tabs.currentTab(), 1);
        QCOMPARE(tabs.currentLeaf(), b);
    }

    void tabHeaderUsesCachedTitle()
    {
        ViewRegistry registry;
        WorkspaceTabs tabs;
        auto *leaf = new WorkspaceLeaf(&registry);
        leaf->setDeferred(true, QStringLiteral("document"), QStringLiteral("My Note"));
        tabs.addChild(leaf);

        QCOMPARE(tabs.tabBar()->tabText(0), QStringLiteral("My Note"));
    }

    void stackedModeDefault()
    {
        WorkspaceTabs tabs;
        QVERIFY(!tabs.isStacked());
    }

    void setStacked()
    {
        WorkspaceTabs tabs;
        tabs.setStacked(true);
        QVERIFY(tabs.isStacked());
    }

    void serializeRoundTrip()
    {
        ViewRegistry registry;
        WorkspaceTabs tabs;
        auto *leaf = new WorkspaceLeaf(&registry);
        tabs.addChild(leaf);
        tabs.setStacked(true);

        QJsonObject json = tabs.serialize();
        QCOMPARE(json[QStringLiteral("type")].toString(), QStringLiteral("tabs"));
        QCOMPARE(json[QStringLiteral("stacked")].toBool(), true);
        QCOMPARE(json[QStringLiteral("currentTab")].toInt(), 0);
        QVERIFY(json[QStringLiteral("children")].toArray().size() == 1);
    }

    void pinnedTabsSortLeft()
    {
        ViewRegistry registry;
        WorkspaceTabs tabs;
        auto *a = new WorkspaceLeaf(&registry);
        auto *b = new WorkspaceLeaf(&registry);
        auto *c = new WorkspaceLeaf(&registry);
        a->setDeferred(true, QStringLiteral("d"), QStringLiteral("A"));
        b->setDeferred(true, QStringLiteral("d"), QStringLiteral("B"));
        c->setDeferred(true, QStringLiteral("d"), QStringLiteral("C"));
        tabs.addChild(a);
        tabs.addChild(b);
        tabs.addChild(c);

        b->setPinned(true);
        tabs.sortPinnedLeft();

        QCOMPARE(tabs.childAt(0), b);
    }
};

QTEST_MAIN(TestWorkspaceTabs)
#include "tst_workspace_tabs.moc"
```

- [ ] **Step 2: Write WorkspaceTabs header**

```cpp
// libs/core/include/corbomite/core/WorkspaceTabs.h
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "corbomite/core/WorkspaceParent.h"

class QTabBar;
class QStackedWidget;
class QScrollArea;
class QVBoxLayout;

namespace Corbomite {

class WorkspaceLeaf;

class WorkspaceTabs : public WorkspaceParent
{
    Q_OBJECT
public:
    explicit WorkspaceTabs(QObject *parent = nullptr);

    QWidget *widget() override;
    QJsonObject serialize() const override;

    void addChild(WorkspaceItem *child, int index = -1);
    void removeChild(WorkspaceItem *child, bool deleteChild = false);

    QTabBar *tabBar() const;

    int currentTab() const;
    void setCurrentTab(int index);
    WorkspaceLeaf *currentLeaf() const;

    bool isStacked() const;
    void setStacked(bool stacked);

    void sortPinnedLeft();
    void updateTabHeader(int index);
    void updateAllTabHeaders();

    WorkspaceLeaf *leafAt(int index) const;

Q_SIGNALS:
    void currentTabChanged(int index);
    void tabCloseRequested(int index);
    void splitRequested(Qt::Orientation direction);

private:
    void onTabBarCurrentChanged(int index);
    void onTabBarCloseRequested(int index);
    void rebuildTabBar();
    QString tabTextForLeaf(WorkspaceLeaf *leaf) const;
    QIcon tabIconForLeaf(WorkspaceLeaf *leaf) const;

    QWidget *m_widget;
    QVBoxLayout *m_layout;
    QTabBar *m_tabBar;
    QStackedWidget *m_stack;
    QScrollArea *m_scrollArea;
    int m_currentTab = 0;
    bool m_stacked = false;
};

} // namespace Corbomite
```

- [ ] **Step 3: Write WorkspaceTabs implementation**

```cpp
// libs/core/src/WorkspaceTabs.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/WorkspaceTabs.h"
#include "corbomite/core/WorkspaceLeaf.h"
#include "corbomite/core/View.h"

#include <QIcon>
#include <QJsonArray>
#include <QScrollArea>
#include <QStackedWidget>
#include <QTabBar>
#include <QVBoxLayout>

namespace Corbomite {

WorkspaceTabs::WorkspaceTabs(QObject *parent)
    : WorkspaceParent(parent)
    , m_widget(new QWidget)
    , m_layout(new QVBoxLayout(m_widget))
    , m_tabBar(new QTabBar(m_widget))
    , m_stack(new QStackedWidget(m_widget))
    , m_scrollArea(new QScrollArea(m_widget))
{
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(0);

    m_tabBar->setTabsClosable(true);
    m_tabBar->setMovable(true);
    m_tabBar->setExpanding(false);
    m_tabBar->setElideMode(Qt::ElideRight);

    m_layout->addWidget(m_tabBar);
    m_layout->addWidget(m_stack, 1);

    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setVisible(false);
    m_layout->addWidget(m_scrollArea);

    connect(m_tabBar, &QTabBar::currentChanged,
            this, &WorkspaceTabs::onTabBarCurrentChanged);
    connect(m_tabBar, &QTabBar::tabCloseRequested,
            this, &WorkspaceTabs::onTabBarCloseRequested);
}

QWidget *WorkspaceTabs::widget() { return m_widget; }

QTabBar *WorkspaceTabs::tabBar() const { return m_tabBar; }

int WorkspaceTabs::currentTab() const { return m_currentTab; }

void WorkspaceTabs::setCurrentTab(int index)
{
    if (index < 0 || index >= m_children.size())
        return;
    m_currentTab = index;
    m_tabBar->setCurrentIndex(index);
    m_stack->setCurrentIndex(index);
}

WorkspaceLeaf *WorkspaceTabs::currentLeaf() const
{
    return leafAt(m_currentTab);
}

WorkspaceLeaf *WorkspaceTabs::leafAt(int index) const
{
    auto *item = childAt(index);
    return qobject_cast<WorkspaceLeaf *>(item);
}

bool WorkspaceTabs::isStacked() const { return m_stacked; }

void WorkspaceTabs::setStacked(bool stacked)
{
    if (m_stacked == stacked)
        return;
    m_stacked = stacked;

    m_tabBar->setVisible(!stacked);
    m_stack->setVisible(!stacked);
    m_scrollArea->setVisible(stacked);

    if (stacked) {
        auto *container = new QWidget;
        auto *vbox = new QVBoxLayout(container);
        vbox->setContentsMargins(0, 0, 0, 0);
        for (auto *child : m_children) {
            if (auto *w = child->widget())
                vbox->addWidget(w, 1);
        }
        m_scrollArea->setWidget(container);
    } else {
        m_scrollArea->takeWidget();
        for (int i = 0; i < m_children.size(); ++i) {
            if (auto *w = m_children[i]->widget())
                m_stack->insertWidget(i, w);
        }
        m_stack->setCurrentIndex(m_currentTab);
    }
}

void WorkspaceTabs::addChild(WorkspaceItem *child, int index)
{
    WorkspaceParent::addChild(child, index);

    auto *leaf = qobject_cast<WorkspaceLeaf *>(child);
    if (!leaf)
        return;

    int idx = m_children.indexOf(child);
    m_tabBar->insertTab(idx, tabIconForLeaf(leaf), tabTextForLeaf(leaf));

    if (auto *w = leaf->widget()) {
        if (m_stacked) {
            if (auto *container = m_scrollArea->widget()) {
                if (auto *vbox = container->layout())
                    vbox->addWidget(w);
            }
        } else {
            m_stack->insertWidget(idx, w);
        }
    }

    if (m_children.size() == 1)
        setCurrentTab(0);
}

void WorkspaceTabs::removeChild(WorkspaceItem *child, bool deleteChild)
{
    int idx = m_children.indexOf(child);
    if (idx < 0)
        return;

    if (auto *w = child->widget())
        w->setParent(nullptr);

    m_tabBar->removeTab(idx);
    WorkspaceParent::removeChild(child, deleteChild);

    if (m_currentTab >= m_children.size())
        m_currentTab = qMax(0, m_children.size() - 1);
    if (!m_children.isEmpty())
        setCurrentTab(m_currentTab);
}

void WorkspaceTabs::sortPinnedLeft()
{
    std::stable_partition(m_children.begin(), m_children.end(),
        [](WorkspaceItem *item) {
            auto *leaf = qobject_cast<WorkspaceLeaf *>(item);
            return leaf && leaf->pinned();
        });
    rebuildTabBar();
}

void WorkspaceTabs::updateTabHeader(int index)
{
    auto *leaf = leafAt(index);
    if (!leaf)
        return;
    m_tabBar->setTabText(index, tabTextForLeaf(leaf));
    m_tabBar->setTabIcon(index, tabIconForLeaf(leaf));
}

void WorkspaceTabs::updateAllTabHeaders()
{
    for (int i = 0; i < m_children.size(); ++i)
        updateTabHeader(i);
}

void WorkspaceTabs::onTabBarCurrentChanged(int index)
{
    if (index < 0 || index >= m_children.size())
        return;
    m_currentTab = index;
    m_stack->setCurrentIndex(index);
    Q_EMIT currentTabChanged(index);
}

void WorkspaceTabs::onTabBarCloseRequested(int index)
{
    Q_EMIT tabCloseRequested(index);
}

void WorkspaceTabs::rebuildTabBar()
{
    while (m_tabBar->count() > 0)
        m_tabBar->removeTab(0);

    for (auto *child : m_children) {
        auto *leaf = qobject_cast<WorkspaceLeaf *>(child);
        if (leaf)
            m_tabBar->addTab(tabIconForLeaf(leaf), tabTextForLeaf(leaf));
    }

    if (!m_stacked) {
        while (m_stack->count() > 0)
            m_stack->removeWidget(m_stack->widget(0));
        for (auto *child : m_children) {
            if (auto *w = child->widget())
                m_stack->addWidget(w);
        }
    }

    if (m_currentTab < m_children.size())
        setCurrentTab(m_currentTab);
}

QString WorkspaceTabs::tabTextForLeaf(WorkspaceLeaf *leaf) const
{
    if (leaf->isDeferred())
        return leaf->cachedTitle();
    if (auto *v = leaf->view())
        return v->getDisplayText();
    return {};
}

QIcon WorkspaceTabs::tabIconForLeaf(WorkspaceLeaf *leaf) const
{
    QString iconName;
    if (leaf->isDeferred())
        iconName = leaf->cachedIcon();
    else if (auto *v = leaf->view())
        iconName = v->getIcon();

    if (iconName.isEmpty())
        return {};
    return QIcon::fromTheme(iconName);
}

QJsonObject WorkspaceTabs::serialize() const
{
    QJsonObject json;
    json[QStringLiteral("id")] = id();
    json[QStringLiteral("type")] = QStringLiteral("tabs");
    json[QStringLiteral("currentTab")] = m_currentTab;

    if (dimension().has_value())
        json[QStringLiteral("dimension")] = dimension().value();
    if (m_stacked)
        json[QStringLiteral("stacked")] = true;

    QJsonArray children;
    for (const auto *child : m_children)
        children.append(child->serialize());
    json[QStringLiteral("children")] = children;

    return json;
}

} // namespace Corbomite
```

- [ ] **Step 4: Add to CMakeLists.txt**

In `libs/core/CMakeLists.txt`, add `src/WorkspaceTabs.cpp` to sources and `include/corbomite/core/WorkspaceTabs.h` to headers.

In `tests/core/CMakeLists.txt`, add:

```cmake
add_executable(tst_workspace_tabs tst_workspace_tabs.cpp)
target_link_libraries(tst_workspace_tabs PRIVATE Qt6::Test Qt6::Widgets Corbomite::Core)
add_test(NAME tst_workspace_tabs COMMAND tst_workspace_tabs)
```

- [ ] **Step 5: Build and run tests**

```bash
cd build && cmake -S .. -B . -DCORBOMITE_DEV_BUILD=ON && cmake --build . --target tst_workspace_tabs && ctest -R tst_workspace_tabs --output-on-failure
```

Expected: all 8 tests pass.

- [ ] **Step 6: Commit**

```bash
git add libs/core/include/corbomite/core/WorkspaceTabs.h libs/core/src/WorkspaceTabs.cpp \
        tests/core/tst_workspace_tabs.cpp libs/core/CMakeLists.txt tests/core/CMakeLists.txt
git commit -m "feat(core): add WorkspaceTabs container with tab bar, stacked mode, pin sort"
```

---

## Task 6: Workspace coordinator + workspace.json serialization + tests

**Files:**
- Create: `libs/core/include/corbomite/core/Workspace.h`
- Create: `libs/core/src/Workspace.cpp`
- Create: `tests/core/tst_workspace_serialize.cpp`
- Create: `tests/core/tst_leaf_undo.cpp`
- Modify: `libs/core/CMakeLists.txt`
- Modify: `tests/core/CMakeLists.txt`

- [ ] **Step 1: Write the serialization test file**

```cpp
// tests/core/tst_workspace_serialize.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QJsonDocument>
#include <QTemporaryDir>
#include "corbomite/core/Workspace.h"
#include "corbomite/core/WorkspaceSplit.h"
#include "corbomite/core/WorkspaceTabs.h"
#include "corbomite/core/WorkspaceLeaf.h"
#include "corbomite/core/ViewRegistry.h"

using namespace Corbomite;

class TestWorkspaceSerialize : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void serializeEmptyWorkspace()
    {
        ViewRegistry registry;
        Workspace ws(&registry);
        QJsonObject json = ws.serialize();

        QVERIFY(json.contains(QStringLiteral("main")));
        QVERIFY(json.contains(QStringLiteral("active")));
    }

    void roundTripSimpleLayout()
    {
        ViewRegistry registry;
        Workspace ws(&registry);

        auto *tabs = ws.mainRoot()->childCount() > 0
            ? qobject_cast<WorkspaceTabs *>(ws.mainRoot()->childAt(0))
            : nullptr;
        QVERIFY(tabs != nullptr);

        auto *leaf = new WorkspaceLeaf(&registry);
        tabs->addChild(leaf);
        ws.setActiveLeaf(leaf);

        QJsonObject json = ws.serialize();
        QCOMPARE(json[QStringLiteral("active")].toString(), leaf->id());

        Workspace ws2(&registry);
        ws2.deserialize(json);
        QCOMPARE(ws2.mainRoot()->childCount(), 1);
    }

    void obsidianSchemaShape()
    {
        ViewRegistry registry;
        Workspace ws(&registry);

        QJsonObject json = ws.serialize();
        auto main = json[QStringLiteral("main")].toObject();
        QCOMPARE(main[QStringLiteral("type")].toString(), QStringLiteral("split"));
        QVERIFY(main.contains(QStringLiteral("children")));
        QVERIFY(main.contains(QStringLiteral("direction")));
    }

    void writeAndReadWorkspaceJson()
    {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        QString vaultPath = tmpDir.path();
        QDir(vaultPath).mkpath(QStringLiteral(".obsidian"));

        ViewRegistry registry;
        Workspace ws(&registry);
        ws.writeWorkspaceJson(vaultPath);

        QFile f(vaultPath + QStringLiteral("/.obsidian/workspace.json"));
        QVERIFY(f.exists());

        Workspace ws2(&registry);
        ws2.readWorkspaceJson(vaultPath);
        QVERIFY(ws2.mainRoot() != nullptr);
    }

    void lastOpenFilesRoundTrip()
    {
        ViewRegistry registry;
        Workspace ws(&registry);
        ws.setLastOpenFiles({QStringLiteral("a.md"), QStringLiteral("b.md")});

        QJsonObject json = ws.serialize();
        Workspace ws2(&registry);
        ws2.deserialize(json);
        QCOMPARE(ws2.lastOpenFiles().size(), 2);
        QCOMPARE(ws2.lastOpenFiles().first(), QStringLiteral("a.md"));
    }

    void activeLeafIdPreserved()
    {
        ViewRegistry registry;
        Workspace ws(&registry);

        auto *tabs = qobject_cast<WorkspaceTabs *>(ws.mainRoot()->childAt(0));
        auto *leaf1 = new WorkspaceLeaf(&registry);
        auto *leaf2 = new WorkspaceLeaf(&registry);
        tabs->addChild(leaf1);
        tabs->addChild(leaf2);
        ws.setActiveLeaf(leaf2);

        QJsonObject json = ws.serialize();
        QCOMPARE(json[QStringLiteral("active")].toString(), leaf2->id());
    }
};

QTEST_MAIN(TestWorkspaceSerialize)
#include "tst_workspace_serialize.moc"
```

- [ ] **Step 2: Write the undo test file**

```cpp
// tests/core/tst_leaf_undo.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include "corbomite/core/Workspace.h"
#include "corbomite/core/WorkspaceTabs.h"
#include "corbomite/core/WorkspaceLeaf.h"
#include "corbomite/core/ViewRegistry.h"

using namespace Corbomite;

class TestLeafUndo : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void undoHistoryInitiallyEmpty()
    {
        ViewRegistry registry;
        Workspace ws(&registry);
        QVERIFY(!ws.canUndoCloseLeaf());
    }

    void closeLeafPushesUndo()
    {
        ViewRegistry registry;
        Workspace ws(&registry);

        auto *tabs = qobject_cast<WorkspaceTabs *>(ws.mainRoot()->childAt(0));
        auto *leaf = new WorkspaceLeaf(&registry);
        tabs->addChild(leaf);
        ws.setActiveLeaf(leaf);

        ws.closeLeaf(leaf);
        QVERIFY(ws.canUndoCloseLeaf());
    }

    void undoCloseRestoresLeaf()
    {
        ViewRegistry registry;
        Workspace ws(&registry);

        auto *tabs = qobject_cast<WorkspaceTabs *>(ws.mainRoot()->childAt(0));
        auto *leaf = new WorkspaceLeaf(&registry);
        tabs->addChild(leaf);
        ws.setActiveLeaf(leaf);
        QString originalId = leaf->id();

        ws.closeLeaf(leaf);
        QCOMPARE(tabs->childCount(), 0);

        ws.undoCloseLeaf();
        QCOMPARE(tabs->childCount(), 1);
    }

    void undoCapAt10()
    {
        ViewRegistry registry;
        Workspace ws(&registry);

        auto *tabs = qobject_cast<WorkspaceTabs *>(ws.mainRoot()->childAt(0));

        for (int i = 0; i < 12; ++i) {
            auto *leaf = new WorkspaceLeaf(&registry);
            tabs->addChild(leaf);
            ws.setActiveLeaf(leaf);
            ws.closeLeaf(leaf);
        }

        int count = 0;
        while (ws.canUndoCloseLeaf()) {
            ws.undoCloseLeaf();
            ++count;
        }
        QCOMPARE(count, 10);
    }
};

QTEST_MAIN(TestLeafUndo)
#include "tst_leaf_undo.moc"
```

- [ ] **Step 3: Write Workspace header**

```cpp
// libs/core/include/corbomite/core/Workspace.h
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QJsonObject>
#include <QObject>
#include <QStringList>
#include <QVector>

#include "corbomite/core/LeafHistory.h"

namespace Corbomite {

class ViewRegistry;
class WorkspaceItem;
class WorkspaceLeaf;
class WorkspaceParent;
class WorkspaceSplit;
class WorkspaceTabs;
class WorkspaceWindow;

struct UndoEntry {
    QString leafId;
    QJsonObject state;
    QJsonObject eState;
    QString parentId;
    QString rootId;
    LeafHistory leafHistory;
    bool pinned = false;
    QString group;
};

class Workspace : public QObject
{
    Q_OBJECT
public:
    static constexpr int UndoCap = 10;

    explicit Workspace(ViewRegistry *registry, QObject *parent = nullptr);
    ~Workspace() override;

    ViewRegistry *viewRegistry() const;
    WorkspaceSplit *mainRoot() const;

    WorkspaceLeaf *activeLeaf() const;
    void setActiveLeaf(WorkspaceLeaf *leaf);

    QStringList lastOpenFiles() const;
    void setLastOpenFiles(const QStringList &files);
    void pushLastOpenFile(const QString &path);

    // Tree operations
    WorkspaceLeaf *createLeafInTabs(WorkspaceTabs *parent);
    void closeLeaf(WorkspaceLeaf *leaf);
    bool canUndoCloseLeaf() const;
    void undoCloseLeaf();
    WorkspaceSplit *splitLeaf(WorkspaceLeaf *leaf, Qt::Orientation direction);

    // Popout windows
    WorkspaceWindow *popoutLeaf(WorkspaceLeaf *leaf);
    void reparentToMain(WorkspaceWindow *window);
    QVector<WorkspaceWindow *> windows() const;

    // Find nodes
    WorkspaceTabs *activeTabs() const;
    WorkspaceLeaf *findLeafById(const QString &id) const;
    WorkspaceTabs *findTabsById(const QString &id) const;
    QVector<WorkspaceLeaf *> allLeaves() const;

    // Persistence
    QJsonObject serialize() const;
    void deserialize(const QJsonObject &json);
    void readWorkspaceJson(const QString &vaultPath);
    void writeWorkspaceJson(const QString &vaultPath);

Q_SIGNALS:
    void activeLeafChanged(WorkspaceLeaf *leaf);
    void layoutChanged();
    void leafClosed(WorkspaceLeaf *leaf);

private:
    WorkspaceItem *deserializeNode(const QJsonObject &json);
    WorkspaceLeaf *findLeafInTree(WorkspaceItem *root, const QString &id) const;
    WorkspaceTabs *findTabsInTree(WorkspaceItem *root, const QString &id) const;
    void collectLeaves(WorkspaceItem *root, QVector<WorkspaceLeaf *> &out) const;
    WorkspaceTabs *findFirstTabs(WorkspaceItem *root) const;
    void setupDefaultLayout();

    ViewRegistry *m_registry;
    WorkspaceSplit *m_mainRoot = nullptr;
    WorkspaceLeaf *m_activeLeaf = nullptr;
    QVector<WorkspaceWindow *> m_windows;
    QVector<UndoEntry> m_undoHistory;
    QStringList m_lastOpenFiles;
};

} // namespace Corbomite
```

- [ ] **Step 4: Write Workspace implementation**

```cpp
// libs/core/src/Workspace.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/Workspace.h"
#include "corbomite/core/ViewRegistry.h"
#include "corbomite/core/WorkspaceLeaf.h"
#include "corbomite/core/WorkspaceSplit.h"
#include "corbomite/core/WorkspaceTabs.h"
#include "corbomite/core/WorkspaceWindow.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>

namespace Corbomite {

Workspace::Workspace(ViewRegistry *registry, QObject *parent)
    : QObject(parent)
    , m_registry(registry)
{
    setupDefaultLayout();
}

Workspace::~Workspace() = default;

ViewRegistry *Workspace::viewRegistry() const { return m_registry; }

WorkspaceSplit *Workspace::mainRoot() const { return m_mainRoot; }

WorkspaceLeaf *Workspace::activeLeaf() const { return m_activeLeaf; }

void Workspace::setActiveLeaf(WorkspaceLeaf *leaf)
{
    if (m_activeLeaf == leaf)
        return;
    m_activeLeaf = leaf;
    if (leaf)
        leaf->updateActiveTime();
    Q_EMIT activeLeafChanged(leaf);
}

QStringList Workspace::lastOpenFiles() const { return m_lastOpenFiles; }

void Workspace::setLastOpenFiles(const QStringList &files) { m_lastOpenFiles = files; }

void Workspace::pushLastOpenFile(const QString &path)
{
    m_lastOpenFiles.removeAll(path);
    m_lastOpenFiles.prepend(path);
    if (m_lastOpenFiles.size() > 50)
        m_lastOpenFiles.removeLast();
}

WorkspaceLeaf *Workspace::createLeafInTabs(WorkspaceTabs *parent)
{
    auto *leaf = new WorkspaceLeaf(m_registry);
    parent->addChild(leaf);
    Q_EMIT layoutChanged();
    return leaf;
}

void Workspace::closeLeaf(WorkspaceLeaf *leaf)
{
    if (!leaf)
        return;

    UndoEntry entry;
    entry.leafId = leaf->id();
    entry.state = leaf->getViewState();
    entry.eState = leaf->getEphemeralState();
    entry.leafHistory = leaf->history();
    entry.pinned = leaf->pinned();
    entry.group = leaf->group();

    if (auto *parent = leaf->parentItem()) {
        entry.parentId = parent->id();
        if (auto *grandparent = parent->parentItem())
            entry.rootId = grandparent->id();
    }

    m_undoHistory.prepend(entry);
    if (m_undoHistory.size() > UndoCap)
        m_undoHistory.removeLast();

    auto *parentTabs = qobject_cast<WorkspaceTabs *>(leaf->parentItem());

    if (m_activeLeaf == leaf)
        m_activeLeaf = nullptr;

    Q_EMIT leafClosed(leaf);

    if (parentTabs) {
        parentTabs->removeChild(leaf, true);
        if (parentTabs->childCount() > 0 && !m_activeLeaf)
            setActiveLeaf(parentTabs->currentLeaf());
    } else {
        delete leaf;
    }

    Q_EMIT layoutChanged();
}

bool Workspace::canUndoCloseLeaf() const { return !m_undoHistory.isEmpty(); }

void Workspace::undoCloseLeaf()
{
    if (m_undoHistory.isEmpty())
        return;

    UndoEntry entry = m_undoHistory.takeFirst();

    WorkspaceTabs *targetTabs = nullptr;
    if (!entry.parentId.isEmpty())
        targetTabs = findTabsById(entry.parentId);
    if (!targetTabs)
        targetTabs = activeTabs();
    if (!targetTabs)
        targetTabs = findFirstTabs(m_mainRoot);
    if (!targetTabs)
        return;

    auto *leaf = new WorkspaceLeaf(m_registry);
    leaf->setId(entry.leafId);
    leaf->setPinned(entry.pinned);
    leaf->setGroup(entry.group);
    targetTabs->addChild(leaf);

    if (!entry.state.isEmpty())
        leaf->setViewState(entry.state);

    setActiveLeaf(leaf);
    Q_EMIT layoutChanged();
}

WorkspaceSplit *Workspace::splitLeaf(WorkspaceLeaf *leaf, Qt::Orientation direction)
{
    if (!leaf || !leaf->parentItem())
        return nullptr;

    auto *parentTabs = qobject_cast<WorkspaceTabs *>(leaf->parentItem());
    auto *grandparent = parentTabs ? parentTabs->parentItem() : nullptr;
    if (!grandparent)
        return nullptr;

    auto *split = new WorkspaceSplit(this);
    split->setDirection(direction);

    int parentIndex = grandparent->indexOf(parentTabs);
    grandparent->removeChild(parentTabs);
    grandparent->addChild(split, parentIndex);

    split->addChild(parentTabs);

    auto *newTabs = new WorkspaceTabs(this);
    split->addChild(newTabs);

    Q_EMIT layoutChanged();
    return split;
}

WorkspaceWindow *Workspace::popoutLeaf(WorkspaceLeaf *leaf)
{
    Q_UNUSED(leaf)
    return nullptr; // Implemented in Task 11
}

void Workspace::reparentToMain(WorkspaceWindow *window)
{
    Q_UNUSED(window)
    // Implemented in Task 11
}

QVector<WorkspaceWindow *> Workspace::windows() const { return m_windows; }

WorkspaceTabs *Workspace::activeTabs() const
{
    if (m_activeLeaf)
        return qobject_cast<WorkspaceTabs *>(m_activeLeaf->parentItem());
    return findFirstTabs(m_mainRoot);
}

WorkspaceLeaf *Workspace::findLeafById(const QString &id) const
{
    return findLeafInTree(m_mainRoot, id);
}

WorkspaceTabs *Workspace::findTabsById(const QString &id) const
{
    return findTabsInTree(m_mainRoot, id);
}

QVector<WorkspaceLeaf *> Workspace::allLeaves() const
{
    QVector<WorkspaceLeaf *> result;
    collectLeaves(m_mainRoot, result);
    return result;
}

QJsonObject Workspace::serialize() const
{
    QJsonObject json;

    if (m_mainRoot)
        json[QStringLiteral("main")] = m_mainRoot->serialize();

    if (m_activeLeaf)
        json[QStringLiteral("active")] = m_activeLeaf->id();

    if (!m_lastOpenFiles.isEmpty()) {
        QJsonArray files;
        for (const auto &f : m_lastOpenFiles)
            files.append(f);
        json[QStringLiteral("lastOpenFiles")] = files;
    }

    return json;
}

void Workspace::deserialize(const QJsonObject &json)
{
    delete m_mainRoot;
    m_mainRoot = nullptr;
    m_activeLeaf = nullptr;
    m_undoHistory.clear();

    if (json.contains(QStringLiteral("main"))) {
        auto *node = deserializeNode(json[QStringLiteral("main")].toObject());
        m_mainRoot = qobject_cast<WorkspaceSplit *>(node);
    }

    if (!m_mainRoot)
        setupDefaultLayout();

    QString activeId = json[QStringLiteral("active")].toString();
    if (!activeId.isEmpty())
        m_activeLeaf = findLeafById(activeId);
    if (!m_activeLeaf) {
        auto leaves = allLeaves();
        if (!leaves.isEmpty())
            m_activeLeaf = leaves.first();
    }

    m_lastOpenFiles.clear();
    for (const auto &v : json[QStringLiteral("lastOpenFiles")].toArray())
        m_lastOpenFiles.append(v.toString());
}

void Workspace::readWorkspaceJson(const QString &vaultPath)
{
    QString path = vaultPath + QStringLiteral("/.obsidian/workspace.json");
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        setupDefaultLayout();
        return;
    }

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    if (err.error != QJsonParseError::NoError) {
        setupDefaultLayout();
        return;
    }

    deserialize(doc.object());
}

void Workspace::writeWorkspaceJson(const QString &vaultPath)
{
    QString dirPath = vaultPath + QStringLiteral("/.obsidian");
    QDir().mkpath(dirPath);

    QString path = dirPath + QStringLiteral("/workspace.json");
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return;

    QJsonDocument doc(serialize());
    f.write(doc.toJson(QJsonDocument::Indented));
}

WorkspaceItem *Workspace::deserializeNode(const QJsonObject &json)
{
    QString type = json[QStringLiteral("type")].toString();

    if (type == QStringLiteral("split")) {
        auto *split = new WorkspaceSplit(this);
        split->setId(json[QStringLiteral("id")].toString());
        QString dir = json[QStringLiteral("direction")].toString();
        split->setDirection(dir == QStringLiteral("vertical") ? Qt::Vertical : Qt::Horizontal);
        if (json.contains(QStringLiteral("dimension")))
            split->setDimension(json[QStringLiteral("dimension")].toInt());

        for (const auto &child : json[QStringLiteral("children")].toArray()) {
            if (auto *node = deserializeNode(child.toObject()))
                split->addChild(node);
        }
        return split;
    }

    if (type == QStringLiteral("tabs")) {
        auto *tabs = new WorkspaceTabs(this);
        tabs->setId(json[QStringLiteral("id")].toString());
        if (json.contains(QStringLiteral("dimension")))
            tabs->setDimension(json[QStringLiteral("dimension")].toInt());
        if (json[QStringLiteral("stacked")].toBool())
            tabs->setStacked(true);

        for (const auto &child : json[QStringLiteral("children")].toArray()) {
            if (auto *node = deserializeNode(child.toObject()))
                tabs->addChild(node);
        }

        int currentTab = json[QStringLiteral("currentTab")].toInt(0);
        tabs->setCurrentTab(currentTab);
        return tabs;
    }

    if (type == QStringLiteral("leaf")) {
        auto *leaf = WorkspaceLeaf::deserialize(json, m_registry, nullptr);
        return leaf;
    }

    return nullptr;
}

WorkspaceLeaf *Workspace::findLeafInTree(WorkspaceItem *root, const QString &id) const
{
    if (!root)
        return nullptr;
    if (auto *leaf = qobject_cast<WorkspaceLeaf *>(root)) {
        if (leaf->id() == id)
            return leaf;
        return nullptr;
    }
    if (auto *parent = qobject_cast<WorkspaceParent *>(root)) {
        for (auto *child : parent->children()) {
            if (auto *found = findLeafInTree(child, id))
                return found;
        }
    }
    return nullptr;
}

WorkspaceTabs *Workspace::findTabsInTree(WorkspaceItem *root, const QString &id) const
{
    if (!root)
        return nullptr;
    if (auto *tabs = qobject_cast<WorkspaceTabs *>(root)) {
        if (tabs->id() == id)
            return tabs;
    }
    if (auto *parent = qobject_cast<WorkspaceParent *>(root)) {
        for (auto *child : parent->children()) {
            if (auto *found = findTabsInTree(child, id))
                return found;
        }
    }
    return nullptr;
}

void Workspace::collectLeaves(WorkspaceItem *root, QVector<WorkspaceLeaf *> &out) const
{
    if (!root)
        return;
    if (auto *leaf = qobject_cast<WorkspaceLeaf *>(root)) {
        out.append(leaf);
        return;
    }
    if (auto *parent = qobject_cast<WorkspaceParent *>(root)) {
        for (auto *child : parent->children())
            collectLeaves(child, out);
    }
}

WorkspaceTabs *Workspace::findFirstTabs(WorkspaceItem *root) const
{
    if (!root)
        return nullptr;
    if (auto *tabs = qobject_cast<WorkspaceTabs *>(root))
        return tabs;
    if (auto *parent = qobject_cast<WorkspaceParent *>(root)) {
        for (auto *child : parent->children()) {
            if (auto *tabs = findFirstTabs(child))
                return tabs;
        }
    }
    return nullptr;
}

void Workspace::setupDefaultLayout()
{
    delete m_mainRoot;
    m_mainRoot = new WorkspaceSplit(this);
    m_mainRoot->setDirection(Qt::Vertical);

    auto *tabs = new WorkspaceTabs(this);
    m_mainRoot->addChild(tabs);
}

} // namespace Corbomite
```

- [ ] **Step 5: Add to CMakeLists.txt**

In `libs/core/CMakeLists.txt`, add `src/Workspace.cpp` to sources and `include/corbomite/core/Workspace.h` to headers.

In `tests/core/CMakeLists.txt`, add:

```cmake
add_executable(tst_workspace_serialize tst_workspace_serialize.cpp)
target_link_libraries(tst_workspace_serialize PRIVATE Qt6::Test Qt6::Widgets Corbomite::Core)
add_test(NAME tst_workspace_serialize COMMAND tst_workspace_serialize)

add_executable(tst_leaf_undo tst_leaf_undo.cpp)
target_link_libraries(tst_leaf_undo PRIVATE Qt6::Test Qt6::Widgets Corbomite::Core)
add_test(NAME tst_leaf_undo COMMAND tst_leaf_undo)
```

- [ ] **Step 6: Build and run tests**

```bash
cd build && cmake -S .. -B . -DCORBOMITE_DEV_BUILD=ON && cmake --build . --target tst_workspace_serialize tst_leaf_undo && ctest -R "tst_workspace_serialize|tst_leaf_undo" --output-on-failure
```

Expected: all 10 tests pass (6 serialize + 4 undo).

- [ ] **Step 7: Run full test suite**

```bash
cd build && ctest --output-on-failure
```

Expected: all existing tests still pass. New tests pass.

- [ ] **Step 8: Commit**

```bash
git add libs/core/include/corbomite/core/Workspace.h libs/core/src/Workspace.cpp \
        tests/core/tst_workspace_serialize.cpp tests/core/tst_leaf_undo.cpp \
        libs/core/CMakeLists.txt tests/core/CMakeLists.txt
git commit -m "feat(core): add Workspace coordinator with workspace.json serialization + leaf undo"
```

---

## Task 7: WorkspaceWindow (popout) + tests

**Files:**
- Create: `libs/core/include/corbomite/core/WorkspaceWindow.h`
- Create: `libs/core/src/WorkspaceWindow.cpp`
- Create: `tests/core/tst_workspace_window.cpp`
- Modify: `libs/core/src/Workspace.cpp` (wire popout/reparent)
- Modify: `libs/core/CMakeLists.txt`
- Modify: `tests/core/CMakeLists.txt`

- [ ] **Step 1: Write the test file**

```cpp
// tests/core/tst_workspace_window.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include "corbomite/core/Workspace.h"
#include "corbomite/core/WorkspaceWindow.h"
#include "corbomite/core/WorkspaceSplit.h"
#include "corbomite/core/WorkspaceTabs.h"
#include "corbomite/core/WorkspaceLeaf.h"
#include "corbomite/core/ViewRegistry.h"

using namespace Corbomite;

class TestWorkspaceWindow : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void windowIsQtWindow()
    {
        WorkspaceWindow win;
        QVERIFY(win.widget()->windowFlags() & Qt::Window);
    }

    void geometryRoundTrip()
    {
        WorkspaceWindow win;
        win.setWindowGeometry(100, 200, 800, 600);
        win.setMaximized(true);

        QJsonObject json = win.serialize();
        QCOMPARE(json[QStringLiteral("type")].toString(), QStringLiteral("window"));
        QCOMPARE(json[QStringLiteral("x")].toInt(), 100);
        QCOMPARE(json[QStringLiteral("y")].toInt(), 200);
        QCOMPARE(json[QStringLiteral("width")].toInt(), 800);
        QCOMPARE(json[QStringLiteral("height")].toInt(), 600);
        QCOMPARE(json[QStringLiteral("maximize")].toBool(), true);
    }

    void popoutMovesLeafToWindow()
    {
        ViewRegistry registry;
        Workspace ws(&registry);

        auto *tabs = qobject_cast<WorkspaceTabs *>(ws.mainRoot()->childAt(0));
        auto *leaf = new WorkspaceLeaf(&registry);
        tabs->addChild(leaf);
        ws.setActiveLeaf(leaf);

        auto *win = ws.popoutLeaf(leaf);
        QVERIFY(win != nullptr);
        QCOMPARE(tabs->childCount(), 0);
        QCOMPARE(ws.windows().size(), 1);
    }

    void reparentToMainMovesLeavesBack()
    {
        ViewRegistry registry;
        Workspace ws(&registry);

        auto *tabs = qobject_cast<WorkspaceTabs *>(ws.mainRoot()->childAt(0));
        auto *leaf = new WorkspaceLeaf(&registry);
        tabs->addChild(leaf);
        ws.setActiveLeaf(leaf);

        auto *win = ws.popoutLeaf(leaf);
        ws.reparentToMain(win);

        QCOMPARE(ws.windows().size(), 0);
        QVERIFY(tabs->childCount() > 0);
    }
};

QTEST_MAIN(TestWorkspaceWindow)
#include "tst_workspace_window.moc"
```

- [ ] **Step 2: Write WorkspaceWindow header**

```cpp
// libs/core/include/corbomite/core/WorkspaceWindow.h
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "corbomite/core/WorkspaceParent.h"

namespace Corbomite {

// Popout window container. Currently a QWidget + Qt::Window flag.
// NOTE: promote to QMainWindow when plugin menus need per-window menu bars.
class WorkspaceWindow : public WorkspaceParent
{
    Q_OBJECT
public:
    explicit WorkspaceWindow(QObject *parent = nullptr);
    ~WorkspaceWindow() override;

    QWidget *widget() override;
    QJsonObject serialize() const override;

    void setWindowGeometry(int x, int y, int w, int h);
    bool maximized() const;
    void setMaximized(bool max);

    void showWindow();
    void closeWindow();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    QWidget *m_widget;
    int m_x = 0, m_y = 0, m_width = 800, m_height = 600;
    bool m_maximized = false;
};

} // namespace Corbomite
```

- [ ] **Step 3: Write WorkspaceWindow implementation**

```cpp
// libs/core/src/WorkspaceWindow.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/WorkspaceWindow.h"

#include <QCloseEvent>
#include <QJsonArray>
#include <QVBoxLayout>

namespace Corbomite {

WorkspaceWindow::WorkspaceWindow(QObject *parent)
    : WorkspaceParent(parent)
    , m_widget(new QWidget(nullptr, Qt::Window))
{
    auto *layout = new QVBoxLayout(m_widget);
    layout->setContentsMargins(0, 0, 0, 0);
    m_widget->installEventFilter(this);
}

WorkspaceWindow::~WorkspaceWindow()
{
    delete m_widget;
}

QWidget *WorkspaceWindow::widget() { return m_widget; }

void WorkspaceWindow::setWindowGeometry(int x, int y, int w, int h)
{
    m_x = x;
    m_y = y;
    m_width = w;
    m_height = h;
    m_widget->setGeometry(x, y, w, h);
}

bool WorkspaceWindow::maximized() const { return m_maximized; }

void WorkspaceWindow::setMaximized(bool max)
{
    m_maximized = max;
}

void WorkspaceWindow::showWindow()
{
    m_widget->setGeometry(m_x, m_y, m_width, m_height);
    if (m_maximized)
        m_widget->showMaximized();
    else
        m_widget->show();

    for (auto *child : m_children) {
        if (auto *w = child->widget())
            m_widget->layout()->addWidget(w);
    }
}

void WorkspaceWindow::closeWindow()
{
    m_widget->hide();
}

bool WorkspaceWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_widget && event->type() == QEvent::Close) {
        auto *closeEvent = static_cast<QCloseEvent *>(event);
        closeEvent->ignore();
        closeWindow();
        return true;
    }
    return WorkspaceParent::eventFilter(obj, event);
}

QJsonObject WorkspaceWindow::serialize() const
{
    QJsonObject json;
    json[QStringLiteral("id")] = id();
    json[QStringLiteral("type")] = QStringLiteral("window");
    json[QStringLiteral("x")] = m_x;
    json[QStringLiteral("y")] = m_y;
    json[QStringLiteral("width")] = m_width;
    json[QStringLiteral("height")] = m_height;
    if (m_maximized)
        json[QStringLiteral("maximize")] = true;

    QJsonArray children;
    for (const auto *child : m_children)
        children.append(child->serialize());
    json[QStringLiteral("children")] = children;

    return json;
}

} // namespace Corbomite
```

- [ ] **Step 4: Wire popoutLeaf/reparentToMain in Workspace.cpp**

Replace the stub implementations in `libs/core/src/Workspace.cpp`:

```cpp
WorkspaceWindow *Workspace::popoutLeaf(WorkspaceLeaf *leaf)
{
    if (!leaf)
        return nullptr;

    auto *oldParent = qobject_cast<WorkspaceTabs *>(leaf->parentItem());
    if (oldParent)
        oldParent->removeChild(leaf);

    auto *win = new WorkspaceWindow(this);
    auto *tabs = new WorkspaceTabs(this);
    win->addChild(tabs);
    tabs->addChild(leaf);

    m_windows.append(win);
    win->showWindow();
    Q_EMIT layoutChanged();
    return win;
}

void Workspace::reparentToMain(WorkspaceWindow *window)
{
    if (!window)
        return;

    auto *targetTabs = activeTabs();
    if (!targetTabs)
        targetTabs = findFirstTabs(m_mainRoot);
    if (!targetTabs)
        return;

    QVector<WorkspaceLeaf *> leaves;
    collectLeaves(window, leaves);

    for (auto *leaf : leaves) {
        if (auto *parent = qobject_cast<WorkspaceParent *>(leaf->parentItem()))
            parent->removeChild(leaf);
        targetTabs->addChild(leaf);
    }

    window->closeWindow();
    m_windows.removeOne(window);
    delete window;
    Q_EMIT layoutChanged();
}
```

- [ ] **Step 5: Add to CMakeLists.txt**

In `libs/core/CMakeLists.txt`, add `src/WorkspaceWindow.cpp` to sources and `include/corbomite/core/WorkspaceWindow.h` to headers.

In `tests/core/CMakeLists.txt`, add:

```cmake
add_executable(tst_workspace_window tst_workspace_window.cpp)
target_link_libraries(tst_workspace_window PRIVATE Qt6::Test Qt6::Widgets Corbomite::Core)
add_test(NAME tst_workspace_window COMMAND tst_workspace_window)
```

- [ ] **Step 6: Build and run tests**

```bash
cd build && cmake -S .. -B . -DCORBOMITE_DEV_BUILD=ON && cmake --build . --target tst_workspace_window && ctest -R tst_workspace_window --output-on-failure
```

Expected: all 4 tests pass.

- [ ] **Step 7: Commit**

```bash
git add libs/core/include/corbomite/core/WorkspaceWindow.h libs/core/src/WorkspaceWindow.cpp \
        libs/core/src/Workspace.cpp tests/core/tst_workspace_window.cpp \
        libs/core/CMakeLists.txt tests/core/CMakeLists.txt
git commit -m "feat(core): add WorkspaceWindow popout container with reparent-on-close"
```

---

## Task 8: Deferred-load tests

**Files:**
- Create: `tests/core/tst_workspace_deferred.cpp`
- Modify: `tests/core/CMakeLists.txt`

- [ ] **Step 1: Write the test file**

```cpp
// tests/core/tst_workspace_deferred.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include "corbomite/core/Workspace.h"
#include "corbomite/core/WorkspaceTabs.h"
#include "corbomite/core/WorkspaceLeaf.h"
#include "corbomite/core/ViewRegistry.h"

using namespace Corbomite;

class TestWorkspaceDeferred : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void deferredLeafHasNoView()
    {
        ViewRegistry registry;
        WorkspaceLeaf leaf(&registry);
        leaf.setDeferred(true, QStringLiteral("document"), QStringLiteral("Test"));

        QVERIFY(leaf.isDeferred());
        QVERIFY(leaf.view() == nullptr);
    }

    void deferredLeafCachedFields()
    {
        ViewRegistry registry;
        WorkspaceLeaf leaf(&registry);
        leaf.setDeferred(true, QStringLiteral("my-icon"), QStringLiteral("My Title"));

        QCOMPARE(leaf.cachedIcon(), QStringLiteral("my-icon"));
        QCOMPARE(leaf.cachedTitle(), QStringLiteral("My Title"));
    }

    void tabBarShowsCachedTitle()
    {
        ViewRegistry registry;
        WorkspaceTabs tabs;
        auto *leaf = new WorkspaceLeaf(&registry);
        leaf->setDeferred(true, QStringLiteral("doc"), QStringLiteral("Cached Title"));
        tabs.addChild(leaf);

        QCOMPARE(tabs.tabBar()->tabText(0), QStringLiteral("Cached Title"));
    }

    void loadIfDeferredClearsFlag()
    {
        ViewRegistry registry;
        WorkspaceLeaf leaf(&registry);
        leaf.setDeferred(true, QStringLiteral("d"), QStringLiteral("T"));
        QVERIFY(leaf.isDeferred());

        leaf.loadIfDeferred();
        QVERIFY(!leaf.isDeferred());
    }

    void nonDeferredLoadIfDeferredIsNoop()
    {
        ViewRegistry registry;
        WorkspaceLeaf leaf(&registry);
        leaf.loadIfDeferred(); // should not crash
        QVERIFY(!leaf.isDeferred());
    }
};

QTEST_MAIN(TestWorkspaceDeferred)
#include "tst_workspace_deferred.moc"
```

- [ ] **Step 2: Add to CMakeLists.txt**

In `tests/core/CMakeLists.txt`, add:

```cmake
add_executable(tst_workspace_deferred tst_workspace_deferred.cpp)
target_link_libraries(tst_workspace_deferred PRIVATE Qt6::Test Qt6::Widgets Corbomite::Core)
add_test(NAME tst_workspace_deferred COMMAND tst_workspace_deferred)
```

- [ ] **Step 3: Build and run tests**

```bash
cd build && cmake -S .. -B . -DCORBOMITE_DEV_BUILD=ON && cmake --build . --target tst_workspace_deferred && ctest -R tst_workspace_deferred --output-on-failure
```

Expected: all 5 tests pass.

- [ ] **Step 4: Commit**

```bash
git add tests/core/tst_workspace_deferred.cpp tests/core/CMakeLists.txt
git commit -m "test(core): add deferred-load stub tests for WorkspaceLeaf + WorkspaceTabs"
```

---

## Task 9: Wire Workspace into MainWindow (replaces EditorViewManager)

This is the big migration task. MainWindow switches from owning `EditorViewManager` to owning `Workspace`. All signal wiring, panel hookups, and view-opening paths are rewired.

**Files:**
- Modify: `src/app/MainWindow.h`
- Modify: `src/app/MainWindow.cpp`

- [ ] **Step 1: Update MainWindow header**

In `src/app/MainWindow.h`:

Add includes:
```cpp
#include "corbomite/core/Workspace.h"
#include "corbomite/core/WorkspaceSplit.h"
#include "corbomite/core/WorkspaceTabs.h"
```

Replace member:
```cpp
// Old:
EditorViewManager *m_editorManager = nullptr;

// New:
Corbomite::Workspace *m_workspace = nullptr;
```

Add private methods:
```cpp
void setupWorkspace();
Corbomite::WorkspaceTabs *activeWorkspaceTabs() const;
void openFileInWorkspace(const QString &relativePath);
```

- [ ] **Step 2: Update MainWindow constructor — create Workspace**

In `MainWindow.cpp`, replace EditorViewManager creation with:

```cpp
m_workspace = new Corbomite::Workspace(m_viewRegistry, this);
```

Replace `setCentralWidget(m_editorManager)` with:

```cpp
setCentralWidget(m_workspace->mainRoot()->widget());
```

- [ ] **Step 3: Rewire file-open paths**

Replace all `m_editorManager->openNote(doc)` calls with:

```cpp
auto *tabs = m_workspace->activeTabs();
if (!tabs) return;
auto *leaf = m_workspace->createLeafInTabs(tabs);
leaf->navigate(QJsonObject{
    {QStringLiteral("type"), QStringLiteral("markdown")},
    {QStringLiteral("state"), QJsonObject{{QStringLiteral("file"), relativePath}}}
});
```

Or use a helper `openFileInWorkspace(relativePath)` that does the above.

Replace `m_editorManager->openCanvas(path)` with equivalent ViewRegistry-based open via `leaf->navigate()` with type `"canvas"`.

Replace `m_editorManager->openGraphView(...)` with `leaf->navigate()` with type `"graph"`.

- [ ] **Step 4: Rewire split actions**

Replace `m_editorManager->splitActiveHorizontal()` with:
```cpp
if (auto *leaf = m_workspace->activeLeaf())
    m_workspace->splitLeaf(leaf, Qt::Horizontal);
```

Replace `m_editorManager->splitActiveVertical()` with:
```cpp
if (auto *leaf = m_workspace->activeLeaf())
    m_workspace->splitLeaf(leaf, Qt::Vertical);
```

- [ ] **Step 5: Rewire active editor signal chain**

Connect `Workspace::activeLeafChanged` to update the status bar, panels, etc.:

```cpp
connect(m_workspace, &Corbomite::Workspace::activeLeafChanged,
        this, [this](Corbomite::WorkspaceLeaf *leaf) {
    // Update panels, status bar, etc. based on active leaf's view
    if (!leaf || !leaf->view()) return;
    // ... existing panel update logic
});
```

- [ ] **Step 6: Rewire workspace.json persistence**

In vault-open path, after setting up the vault:
```cpp
m_workspace->readWorkspaceJson(vaultPath);
```

In vault-close / app-quit path:
```cpp
m_workspace->writeWorkspaceJson(vaultPath);
```

- [ ] **Step 7: Wire Ctrl+Shift+T for undo close**

```cpp
auto *undoClose = new QAction(i18n("Undo Close Tab"), this);
undoClose->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_T));
connect(undoClose, &QAction::triggered, this, [this] {
    m_workspace->undoCloseLeaf();
});
addAction(undoClose);
```

- [ ] **Step 8: Wire tab close through Workspace**

Connect `WorkspaceTabs::tabCloseRequested` signal. In the workspace setup or where tabs are created:

```cpp
connect(tabs, &Corbomite::WorkspaceTabs::tabCloseRequested,
        this, [this, tabs](int index) {
    if (auto *leaf = tabs->leafAt(index))
        m_workspace->closeLeaf(leaf);
});
```

- [ ] **Step 9: Propagate services to workspace views**

The hover popover, suggest manager, canvas engine, and other services that EditorViewManager propagated to EditorViewSpaces need to be propagated through the Workspace tree. Connect to `Workspace::layoutChanged` to propagate to new leaves/tabs as they're created.

- [ ] **Step 10: Build and verify**

```bash
cd build && cmake -S .. -B . -DCORBOMITE_DEV_BUILD=ON && cmake --build . 2>&1 | tail -20
```

Fix any compilation errors. This is the most complex migration step — expect iterative fixes.

- [ ] **Step 11: Run full test suite**

```bash
cd build && ctest --output-on-failure
```

Expected: all tests pass.

- [ ] **Step 12: Commit**

```bash
git add src/app/MainWindow.h src/app/MainWindow.cpp
git commit -m "feat(app): wire Workspace into MainWindow, replacing EditorViewManager for all view operations"
```

---

## Task 10: Delete old classes (EditorViewManager, EditorViewSpace, PaneLayoutBridge, PaneLayout)

**Files:**
- Delete: `src/editor/EditorViewSpace.h` + `.cpp`
- Delete: `src/editor/EditorViewManager.h` + `.cpp`
- Delete: `libs/core/include/corbomite/core/PaneLayoutBridge.h` + `libs/core/src/PaneLayoutBridge.cpp`
- Delete: `libs/core/include/corbomite/core/PaneLayout.h` + `libs/core/src/PaneLayout.cpp`
- Modify: `libs/core/CMakeLists.txt`
- Modify: `src/CMakeLists.txt` (or wherever EditorViewManager/Space are listed)
- Modify: `tests/core/CMakeLists.txt`
- Delete: `tests/core/tst_panelayout.cpp`
- Delete: `tests/core/tst_panelayoutbridge.cpp`

- [ ] **Step 1: Remove from CMake**

In `libs/core/CMakeLists.txt`, remove `PaneLayout.cpp`, `PaneLayout.h`, `PaneLayoutBridge.cpp`, `PaneLayoutBridge.h` from the source/header lists.

In `src/CMakeLists.txt` (or `src/editor/CMakeLists.txt`), remove `EditorViewSpace.cpp`, `EditorViewSpace.h`, `EditorViewManager.cpp`, `EditorViewManager.h`.

In `tests/core/CMakeLists.txt`, remove the `tst_panelayout` and `tst_panelayoutbridge` test executables.

- [ ] **Step 2: Remove any remaining includes**

Search for `#include` references to the deleted headers and remove them. Likely candidates:
- `MainWindow.cpp` — remove `#include "editor/EditorViewManager.h"` and `#include "editor/EditorViewSpace.h"`
- Any panel or widget that referenced EditorViewManager/Space

```bash
cd /home/clinton/dev/Corbomite && grep -rn "EditorViewManager\|EditorViewSpace\|PaneLayoutBridge\|PaneLayout" --include="*.h" --include="*.cpp" src/ libs/
```

Fix all remaining references.

- [ ] **Step 3: Delete the files**

```bash
rm src/editor/EditorViewSpace.h src/editor/EditorViewSpace.cpp \
   src/editor/EditorViewManager.h src/editor/EditorViewManager.cpp \
   libs/core/include/corbomite/core/PaneLayoutBridge.h libs/core/src/PaneLayoutBridge.cpp \
   libs/core/include/corbomite/core/PaneLayout.h libs/core/src/PaneLayout.cpp \
   tests/core/tst_panelayout.cpp tests/core/tst_panelayoutbridge.cpp
```

- [ ] **Step 4: Build**

```bash
cd build && cmake -S .. -B . -DCORBOMITE_DEV_BUILD=ON && cmake --build . 2>&1 | tail -20
```

Fix any remaining references until the build is clean.

- [ ] **Step 5: Run full test suite**

```bash
cd build && ctest --output-on-failure
```

Expected: all tests pass (minus the deleted test executables which should no longer be listed).

- [ ] **Step 6: Commit**

```bash
git add -A
git commit -m "refactor(core): delete EditorViewManager, EditorViewSpace, PaneLayoutBridge, PaneLayout

Replaced by Workspace + WorkspaceSplit + WorkspaceTabs + workspace.json native serialization."
```

---

## Task 11: Wire per-leaf history into ItemView header

**Files:**
- Modify: `libs/core/include/corbomite/core/ItemView.h`
- Modify: `libs/core/src/ItemView.cpp`

- [ ] **Step 1: Add back/forward buttons to ItemView header**

In `ItemView.h`, add private members:

```cpp
QToolButton *m_backButton = nullptr;
QToolButton *m_forwardButton = nullptr;
```

- [ ] **Step 2: Update buildHeader() in ItemView.cpp**

At the start of the header actions area (before the title), add back/forward buttons:

```cpp
m_backButton = new QToolButton(m_headerWidget);
m_backButton->setIcon(QIcon::fromTheme(QStringLiteral("go-previous")));
m_backButton->setToolTip(i18n("Navigate Back"));
m_backButton->setAutoRaise(true);
m_backButton->setEnabled(false);

m_forwardButton = new QToolButton(m_headerWidget);
m_forwardButton->setIcon(QIcon::fromTheme(QStringLiteral("go-next")));
m_forwardButton->setToolTip(i18n("Navigate Forward"));
m_forwardButton->setAutoRaise(true);
m_forwardButton->setEnabled(false);

// Insert at start of header layout
headerLayout->addWidget(m_backButton);
headerLayout->addWidget(m_forwardButton);

connect(m_backButton, &QToolButton::clicked, this, [this] {
    if (m_leaf) m_leaf->goBack();
});
connect(m_forwardButton, &QToolButton::clicked, this, [this] {
    if (m_leaf) m_leaf->goForward();
});
```

- [ ] **Step 3: Update button enabled state on view state changes**

Add a method to ItemView that updates back/forward button states, called after navigation:

```cpp
void ItemView::updateNavigationButtons()
{
    if (!m_leaf) return;
    m_backButton->setEnabled(m_leaf->history().canGoBack());
    m_forwardButton->setEnabled(m_leaf->history().canGoForward());
}
```

Call this from `onOpen()` and connect it to relevant signals.

- [ ] **Step 4: Build and run tests**

```bash
cd build && cmake -S .. -B . -DCORBOMITE_DEV_BUILD=ON && cmake --build . && ctest --output-on-failure
```

Expected: all tests pass.

- [ ] **Step 5: Commit**

```bash
git add libs/core/include/corbomite/core/ItemView.h libs/core/src/ItemView.cpp
git commit -m "feat(views): wire back/forward navigation buttons in ItemView header"
```

---

## Task 12: Linked-pane group propagation

**Files:**
- Modify: `libs/core/src/Workspace.cpp`
- Modify: `libs/core/include/corbomite/core/Workspace.h`

- [ ] **Step 1: Add group propagation to Workspace**

In `Workspace.h`, add:

```cpp
void propagatePinToGroup(WorkspaceLeaf *leaf);
QVector<WorkspaceLeaf *> groupMembers(const QString &groupId) const;
```

- [ ] **Step 2: Implement group propagation**

In `Workspace.cpp`:

```cpp
QVector<WorkspaceLeaf *> Workspace::groupMembers(const QString &groupId) const
{
    QVector<WorkspaceLeaf *> result;
    if (groupId.isEmpty())
        return result;
    for (auto *leaf : allLeaves()) {
        if (leaf->group() == groupId)
            result.append(leaf);
    }
    return result;
}

void Workspace::propagatePinToGroup(WorkspaceLeaf *leaf)
{
    if (!leaf || leaf->group().isEmpty())
        return;
    bool pinned = leaf->pinned();
    for (auto *member : groupMembers(leaf->group())) {
        if (member != leaf)
            member->setPinned(pinned);
    }
}
```

Connect to leaf's `pinnedChanged` signal when leaves are created:

```cpp
// In createLeafInTabs, after creating the leaf:
connect(leaf, &WorkspaceLeaf::pinnedChanged, this, [this, leaf](bool) {
    propagatePinToGroup(leaf);
});
```

- [ ] **Step 3: Add pinned-tab redirect logic**

In `Workspace.h`, add:

```cpp
WorkspaceLeaf *findOrCreateUnpinnedLeaf(WorkspaceTabs *tabs);
```

In `Workspace.cpp`:

```cpp
WorkspaceLeaf *Workspace::findOrCreateUnpinnedLeaf(WorkspaceTabs *tabs)
{
    for (int i = 0; i < tabs->childCount(); ++i) {
        auto *leaf = tabs->leafAt(i);
        if (leaf && !leaf->pinned())
            return leaf;
    }
    return createLeafInTabs(tabs);
}
```

This is called when navigating from a pinned tab — instead of replacing the pinned tab's content, the navigation happens in the nearest unpinned tab.

- [ ] **Step 4: Build and run tests**

```bash
cd build && cmake -S .. -B . -DCORBOMITE_DEV_BUILD=ON && cmake --build . && ctest --output-on-failure
```

Expected: all tests pass.

- [ ] **Step 5: Commit**

```bash
git add libs/core/include/corbomite/core/Workspace.h libs/core/src/Workspace.cpp
git commit -m "feat(core): add linked-pane group propagation and pinned-tab redirect"
```

---

## Task 13: Deferred-load wiring in Workspace deserialize

**Files:**
- Modify: `libs/core/src/Workspace.cpp`

- [ ] **Step 1: Update deserializeNode for deferred leaves**

In `Workspace::deserializeNode`, when creating leaf nodes, mark non-active leaves as deferred:

```cpp
if (type == QStringLiteral("leaf")) {
    auto *leaf = WorkspaceLeaf::deserialize(json, m_registry, nullptr);
    // Deferred load: don't construct the View yet — will be done in post-pass
    // based on which leaf is active
    return leaf;
}
```

- [ ] **Step 2: Add deferred-load post-pass to deserialize()**

After deserializing all nodes and finding the active leaf, mark all other leaves as deferred:

```cpp
// In Workspace::deserialize(), after setting m_activeLeaf:
for (auto *leaf : allLeaves()) {
    if (leaf == m_activeLeaf)
        continue;
    auto state = leaf->getViewState();
    QString icon = state[QStringLiteral("icon")].toString();
    QString title = state[QStringLiteral("title")].toString();
    leaf->setDeferred(true, icon, title);
}

// Also set current tabs' active leaf as non-deferred
// and construct its view
if (m_activeLeaf && m_activeLeaf->isDeferred())
    m_activeLeaf->loadIfDeferred();
```

- [ ] **Step 3: Wire loadIfDeferred on tab focus**

In `WorkspaceTabs::onTabBarCurrentChanged`, after updating the current tab:

```cpp
if (auto *leaf = leafAt(index)) {
    if (leaf->isDeferred())
        leaf->loadIfDeferred();
}
```

- [ ] **Step 4: Build and run tests**

```bash
cd build && cmake -S .. -B . -DCORBOMITE_DEV_BUILD=ON && cmake --build . && ctest -R "tst_workspace" --output-on-failure
```

Expected: all workspace tests pass, including deferred tests.

- [ ] **Step 5: Commit**

```bash
git add libs/core/src/Workspace.cpp libs/core/src/WorkspaceTabs.cpp
git commit -m "feat(core): wire deferred-load in workspace deserialize — only active tab constructs View"
```

---

## Task 14: Full integration test + smoke test

**Files:**
- No new files — this is a verification task

- [ ] **Step 1: Run full test suite**

```bash
cd build && cmake -S .. -B . -DCORBOMITE_DEV_BUILD=ON && cmake --build . && ctest --output-on-failure
```

Expected: all tests pass (except known-flaky: `tst_markoff_inline_math`, `tst_renderengine`, `tst_completion_popup`, `tst_benchmark_layout`).

- [ ] **Step 2: Manual smoke test**

```bash
./build/Corbomite
```

Verify:
1. Open a vault — files load in the file explorer
2. Click a .md file — opens in a tab
3. Open multiple files — tab bar shows multiple tabs
4. Click back/forward buttons in the view header (if a history exists)
5. Close a tab — Ctrl+Shift+T reopens it
6. Split panes work (horizontal and vertical)
7. Close vault and reopen — layout restored from workspace.json
8. Check `.obsidian/workspace.json` was written and matches expected Obsidian schema

- [ ] **Step 3: Commit any final fixes**

If any issues were found, fix and commit.

---

## Task 15: Update PROJECT-STATE and plan index

**Files:**
- Modify: `docs/PROJECT-STATE.md`
- Modify: `docs/superpowers/plans/INDEX.md`

- [ ] **Step 1: Update PROJECT-STATE.md**

Update the Cluster G row in the roadmap table:
- Status: `Done`
- Add a Recent decisions entry for Cluster G Part 2

Update the "Current focus" section with the Cluster G Part 2 completion summary.

- [ ] **Step 2: Update INDEX.md**

Update the Cluster G row:
- Status: `Done`
- Add the Part 2 plan file reference

- [ ] **Step 3: Commit**

```bash
git add docs/PROJECT-STATE.md docs/superpowers/plans/INDEX.md
git commit -m "docs(state): Cluster G Part 2 done — workspace containers + advanced tab features"
```
