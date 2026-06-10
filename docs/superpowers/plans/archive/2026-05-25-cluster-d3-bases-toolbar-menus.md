# Cluster D.3 — Bases Toolbar Menus, Properties Drawer & Inline-Edit Polish — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add the Bases interactive query-management UI — three toolbar popover menus (Properties, Sort+group, Views), a per-row frontmatter properties drawer, and verified inline cell editing — to the read-only table D.2 built.

**Architecture:** All config mutation lives in pure widget-free free functions (`ViewConfigOps`, mirroring D.2's `SortCycle`) that are unit-tested in isolation. The toolbar popups and the drawer are thin GUI shells that call those helpers, then trigger `QueryController::recomputeNow()` + `BasesView::requestSave()`. Frontmatter value edits route through `FileManager::processFrontMatter` (the same path `BasesTreeModel::setData` already uses).

**Tech Stack:** C++20, Qt6 (Widgets: QToolButton, QFrame/Qt::Popup, QListWidget, QComboBox, QSplitter, QFormLayout; Core), QtTest, CMake. Library: `libs/bases` (`Corbomite::Bases`). Spec: [`docs/superpowers/specs/2026-05-25-cluster-d3-bases-toolbar-menus-design.md`](../specs/2026-05-25-cluster-d3-bases-toolbar-menus-design.md).

---

## File structure

- **Create** `libs/bases/include/corbomite/bases/ViewConfigOps.h` + `src/ViewConfigOps.cpp` — pure column/sort/group/view-CRUD mutators. No Qt widgets.
- **Create** `libs/bases/include/corbomite/bases/PropertiesMenuPanel.h` + `src/PropertiesMenuPanel.cpp` — column visibility/reorder popup.
- **Create** `libs/bases/include/corbomite/bases/SortGroupMenuPanel.h` + `src/SortGroupMenuPanel.cpp` — sort-stack + group-by popup.
- **Create** `libs/bases/include/corbomite/bases/ViewsMenuPanel.h` + `src/ViewsMenuPanel.cpp` — view CRUD popup.
- **Create** `libs/bases/include/corbomite/bases/PropertiesDrawer.h` + `src/PropertiesDrawer.cpp` — per-row frontmatter editor pane.
- **Modify** `libs/bases/include/corbomite/bases/BasesView.h` + `src/BasesView.cpp` — toolbar buttons, QSplitter, drawer, panel wiring, selection-tracking.
- **Modify** `libs/bases/src/BasesCellDelegate.cpp` — swap `BasesTableModel::*Role` reads to `BasesTreeModel::` constants.
- **Modify** `libs/bases/CMakeLists.txt` — add the 5 new `src/*.cpp`.
- **Create test** `libs/bases/tests/tst_view_config_ops.cpp` (+ register in `tests/CMakeLists.txt`).

Build: `cmake --build --preset dev -j 10`. Test: `cd build-dev && ctest -R <name> --output-on-failure`.

---

### Task 1: `ViewConfigOps` — pure mutation helpers

**Files:**
- Create: `libs/bases/include/corbomite/bases/ViewConfigOps.h`, `libs/bases/src/ViewConfigOps.cpp`
- Modify: `libs/bases/CMakeLists.txt`
- Test: `libs/bases/tests/tst_view_config_ops.cpp` (+ register in `tests/CMakeLists.txt`)

- [ ] **Step 1: Write the failing test**

Create `libs/bases/tests/tst_view_config_ops.cpp`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include "corbomite/bases/ViewConfigOps.h"
#include "corbomite/bases/BasesQuery.h"
#include "corbomite/bases/BasesViewConfig.h"

using namespace Corbomite::Bases;

namespace {
PropertyId note(const char *n) { return PropertyId{PropertyKind::Note, QString::fromLatin1(n)}; }
QString names(const QVector<PropertyId> &v) {
    QStringList p; for (const auto &x : v) p << x.name; return p.join(QLatin1Char(','));
}
QString sortStr(const QVector<SortKey> &s) {
    QStringList p; for (const auto &k : s) p << k.property.name + QLatin1Char(':') + k.direction;
    return p.join(QLatin1Char(','));
}
// Build a query with `n` views named view0..view(n-1).
std::unique_ptr<BasesQuery> queryWithViews(int n) {
    auto q = std::make_unique<BasesQuery>();
    for (int i = 0; i < n; ++i) {
        auto v = std::make_unique<BasesViewConfig>();
        v->type = QStringLiteral("table");
        v->name = QStringLiteral("view%1").arg(i);
        q->views.push_back(std::move(v));
    }
    return q;
}
QString viewNames(const BasesQuery &q) {
    QStringList p; for (const auto &v : q.views) p << v->name; return p.join(QLatin1Char(','));
}
}

class TestViewConfigOps : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    // --- columns ---
    void testHideColumnRemoves() {
        QVector<PropertyId> order{note("a"), note("b"), note("c")};
        const QVector<PropertyId> all{note("a"), note("b"), note("c")};
        setColumnVisible(order, note("b"), false, all);
        QCOMPARE(names(order), QStringLiteral("a,c"));
    }
    void testShowColumnInsertsAtAllPropsPosition() {
        QVector<PropertyId> order{note("a"), note("c")};      // b hidden
        const QVector<PropertyId> all{note("a"), note("b"), note("c")};
        setColumnVisible(order, note("b"), true, all);
        QCOMPARE(names(order), QStringLiteral("a,b,c"));       // reinserted between a and c
    }
    void testShowAlreadyVisibleIsNoop() {
        QVector<PropertyId> order{note("a"), note("b")};
        const QVector<PropertyId> all{note("a"), note("b")};
        setColumnVisible(order, note("a"), true, all);
        QCOMPARE(names(order), QStringLiteral("a,b"));
    }
    void testMoveColumn() {
        QVector<PropertyId> order{note("a"), note("b"), note("c")};
        moveColumn(order, 0, 2);
        QCOMPARE(names(order), QStringLiteral("b,c,a"));
    }
    void testHideAll() {
        QVector<PropertyId> order{note("a"), note("b")};
        hideAllColumns(order);
        QCOMPARE(names(order), QString{});
    }
    // --- sort ---
    void testAddSortKeyAppends() {
        QVector<SortKey> s{{note("a"), QStringLiteral("ASC")}};
        addSortKey(s, note("b"), QStringLiteral("DESC"));
        QCOMPARE(sortStr(s), QStringLiteral("a:ASC,b:DESC"));
    }
    void testAddSortKeyAbsentOnlyOnce() {
        QVector<SortKey> s{{note("a"), QStringLiteral("ASC")}};
        addSortKey(s, note("a"), QStringLiteral("DESC"));     // already present -> no-op
        QCOMPARE(sortStr(s), QStringLiteral("a:ASC"));
    }
    void testSetSortDirectionExisting() {
        QVector<SortKey> s{{note("a"), QStringLiteral("ASC")}};
        setSortDirection(s, note("a"), QStringLiteral("DESC"));
        QCOMPARE(sortStr(s), QStringLiteral("a:DESC"));
    }
    void testSetSortDirectionInsertsWhenAbsent() {
        QVector<SortKey> s;
        setSortDirection(s, note("a"), QStringLiteral("ASC"));
        QCOMPARE(sortStr(s), QStringLiteral("a:ASC"));
    }
    void testRemoveSortKey() {
        QVector<SortKey> s{{note("a"), QStringLiteral("ASC")}, {note("b"), QStringLiteral("DESC")}};
        removeSortKey(s, note("a"));
        QCOMPARE(sortStr(s), QStringLiteral("b:DESC"));
    }
    // --- group ---
    void testSetGroupBy() {
        BasesViewConfig cfg;
        setGroupBy(cfg, note("status"), QStringLiteral("ASC"));
        QVERIFY(cfg.groupBy.has_value());
        QCOMPARE(cfg.groupBy->property.name, QStringLiteral("status"));
        QCOMPARE(cfg.groupBy->direction, QStringLiteral("ASC"));
    }
    void testClearGroupBy() {
        BasesViewConfig cfg;
        cfg.groupBy = GroupBy{note("x"), QStringLiteral("ASC")};
        setGroupBy(cfg, std::nullopt, QString{});
        QVERIFY(!cfg.groupBy.has_value());
    }
    // --- view CRUD ---
    void testDuplicateView() {
        auto q = queryWithViews(2);
        QVERIFY(duplicateView(*q, QStringLiteral("view0"), QStringLiteral("view0 copy")));
        QCOMPARE(viewNames(*q), QStringLiteral("view0,view1,view0 copy"));
    }
    void testDuplicateRefusesCollision() {
        auto q = queryWithViews(2);
        QVERIFY(!duplicateView(*q, QStringLiteral("view0"), QStringLiteral("view1")));
        QCOMPARE(viewNames(*q), QStringLiteral("view0,view1"));
    }
    void testDeleteView() {
        auto q = queryWithViews(2);
        QVERIFY(deleteView(*q, QStringLiteral("view0")));
        QCOMPARE(viewNames(*q), QStringLiteral("view1"));
    }
    void testDeleteRefusesLastView() {
        auto q = queryWithViews(1);
        QVERIFY(!deleteView(*q, QStringLiteral("view0")));
        QCOMPARE(viewNames(*q), QStringLiteral("view0"));
    }
    void testRenameView() {
        auto q = queryWithViews(2);
        QVERIFY(renameView(*q, QStringLiteral("view0"), QStringLiteral("Active")));
        QCOMPARE(viewNames(*q), QStringLiteral("Active,view1"));
    }
    void testRenameRefusesCollision() {
        auto q = queryWithViews(2);
        QVERIFY(!renameView(*q, QStringLiteral("view0"), QStringLiteral("view1")));
        QCOMPARE(viewNames(*q), QStringLiteral("view0,view1"));
    }
    void testSetDefaultMovesToFront() {
        auto q = queryWithViews(3);
        QVERIFY(setDefaultView(*q, QStringLiteral("view2")));
        QCOMPARE(viewNames(*q), QStringLiteral("view2,view0,view1"));
    }
};

QTEST_APPLESS_MAIN(TestViewConfigOps)
#include "tst_view_config_ops.moc"
```

- [ ] **Step 2: Register the test + run to confirm it fails**

In `libs/bases/tests/CMakeLists.txt`, append:
```cmake
add_executable(tst_bases_view_config_ops tst_view_config_ops.cpp)
add_test(NAME tst_bases_view_config_ops COMMAND tst_bases_view_config_ops)
target_link_libraries(tst_bases_view_config_ops PRIVATE Qt6::Test Corbomite::Bases)
```
Run: `cmake --build --preset dev -j 10 --target tst_bases_view_config_ops`
Expected: FAILS to compile (`ViewConfigOps.h` missing).

- [ ] **Step 3: Implement the header**

Create `libs/bases/include/corbomite/bases/ViewConfigOps.h`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "BasesViewConfig.h"   // SortKey, GroupBy, BasesViewConfig
#include "PropertyId.h"

#include <QString>
#include <QVector>

#include <optional>

namespace Corbomite::Bases {

class BasesQuery;

// --- column ops (visibility == membership in `order`) ---

/// Show or hide `pid` in `order`. When showing, insert at the position implied
/// by `allProps` (so re-shown columns land back near their canonical slot).
/// Showing an already-visible column is a no-op.
void setColumnVisible(QVector<PropertyId> &order, const PropertyId &pid, bool visible,
                      const QVector<PropertyId> &allProps);
/// Move the column at `from` to index `to` (clamped). No-op if out of range.
void moveColumn(QVector<PropertyId> &order, int from, int to);
/// Remove every column (hide all).
void hideAllColumns(QVector<PropertyId> &order);

// --- sort ops (complements SortCycle::cycleHeaderSort) ---

/// Append `[pid, dir]` if `pid` is not already a sort key; else no-op.
void addSortKey(QVector<SortKey> &sort, const PropertyId &pid, const QString &dir);
/// Set `pid`'s direction; insert `[pid, dir]` at the end if absent.
void setSortDirection(QVector<SortKey> &sort, const PropertyId &pid, const QString &dir);
/// Remove `pid` from the sort keys (no-op if absent).
void removeSortKey(QVector<SortKey> &sort, const PropertyId &pid);

// --- group op ---

/// Set `cfg.groupBy` to `{*pid, dir}`; `std::nullopt` clears it.
void setGroupBy(BasesViewConfig &cfg, const std::optional<PropertyId> &pid, const QString &dir);

// --- view CRUD (operate on BasesQuery::views) ---

/// Deep-copy the view named `name` to a new view `newName`, appended at the end.
/// Returns false if `name` is missing or `newName` already exists.
bool duplicateView(BasesQuery &q, const QString &name, const QString &newName);
/// Delete the view named `name`. Refuses (returns false) to delete the last view.
bool deleteView(BasesQuery &q, const QString &name);
/// Rename `oldName` to `newName`. Returns false on missing source or name collision.
bool renameView(BasesQuery &q, const QString &oldName, const QString &newName);
/// Move the view named `name` to index 0 (Obsidian's "default" view). False if missing.
bool setDefaultView(BasesQuery &q, const QString &name);

}  // namespace Corbomite::Bases
```

- [ ] **Step 4: Implement the source**

Create `libs/bases/src/ViewConfigOps.cpp`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/ViewConfigOps.h"

#include "corbomite/bases/BasesQuery.h"

namespace Corbomite::Bases {

namespace {
int indexOfProp(const QVector<PropertyId> &v, const PropertyId &p) {
    for (int i = 0; i < v.size(); ++i) if (v[i] == p) return i;
    return -1;
}
int indexOfSort(const QVector<SortKey> &v, const PropertyId &p) {
    for (int i = 0; i < v.size(); ++i) if (v[i].property == p) return i;
    return -1;
}
int indexOfView(const std::vector<std::unique_ptr<BasesViewConfig>> &views, const QString &name) {
    for (int i = 0; i < int(views.size()); ++i) if (views[i] && views[i]->name == name) return i;
    return -1;
}
}  // namespace

void setColumnVisible(QVector<PropertyId> &order, const PropertyId &pid, bool visible,
                      const QVector<PropertyId> &allProps)
{
    const int at = indexOfProp(order, pid);
    if (visible) {
        if (at >= 0) return;  // already visible
        // Insert before the first already-visible column that comes after `pid`
        // in allProps; if none, append.
        const int canonical = indexOfProp(allProps, pid);
        int insertAt = order.size();
        if (canonical >= 0) {
            for (int i = 0; i < order.size(); ++i) {
                if (indexOfProp(allProps, order[i]) > canonical) { insertAt = i; break; }
            }
        }
        order.insert(insertAt, pid);
    } else {
        if (at >= 0) order.remove(at);
    }
}

void moveColumn(QVector<PropertyId> &order, int from, int to)
{
    if (from < 0 || from >= order.size()) return;
    if (to < 0) to = 0;
    if (to >= order.size()) to = order.size() - 1;
    if (from == to) return;
    order.move(from, to);
}

void hideAllColumns(QVector<PropertyId> &order) { order.clear(); }

void addSortKey(QVector<SortKey> &sort, const PropertyId &pid, const QString &dir)
{
    if (indexOfSort(sort, pid) >= 0) return;
    sort.push_back({pid, dir});
}

void setSortDirection(QVector<SortKey> &sort, const PropertyId &pid, const QString &dir)
{
    const int i = indexOfSort(sort, pid);
    if (i >= 0) sort[i].direction = dir;
    else sort.push_back({pid, dir});
}

void removeSortKey(QVector<SortKey> &sort, const PropertyId &pid)
{
    const int i = indexOfSort(sort, pid);
    if (i >= 0) sort.remove(i);
}

void setGroupBy(BasesViewConfig &cfg, const std::optional<PropertyId> &pid, const QString &dir)
{
    if (pid.has_value()) cfg.groupBy = GroupBy{*pid, dir};
    else cfg.groupBy.reset();
}

bool duplicateView(BasesQuery &q, const QString &name, const QString &newName)
{
    if (indexOfView(q.views, newName) >= 0) return false;
    const int src = indexOfView(q.views, name);
    if (src < 0) return false;
    auto copy = std::make_unique<BasesViewConfig>(*q.views[src]);  // member-wise copy
    copy->name = newName;
    q.views.push_back(std::move(copy));
    return true;
}

bool deleteView(BasesQuery &q, const QString &name)
{
    if (q.views.size() <= 1) return false;  // keep >=1 view (empty-file invariant)
    const int i = indexOfView(q.views, name);
    if (i < 0) return false;
    q.views.erase(q.views.begin() + i);
    return true;
}

bool renameView(BasesQuery &q, const QString &oldName, const QString &newName)
{
    if (indexOfView(q.views, newName) >= 0) return false;
    const int i = indexOfView(q.views, oldName);
    if (i < 0) return false;
    q.views[i]->name = newName;
    return true;
}

bool setDefaultView(BasesQuery &q, const QString &name)
{
    const int i = indexOfView(q.views, name);
    if (i <= 0) return i == 0;  // already default (0) -> true; missing (-1) -> false
    auto v = std::move(q.views[i]);
    q.views.erase(q.views.begin() + i);
    q.views.insert(q.views.begin(), std::move(v));
    return true;
}

}  // namespace Corbomite::Bases
```

> `BasesViewConfig` is copy-constructible (plain-data members: `QString`, `FilterPtr` (shared_ptr), `QVector`, `std::optional`, `QHash`, `QVariantMap`), so `std::make_unique<BasesViewConfig>(*src)` member-wise-copies it.

Add `src/ViewConfigOps.cpp` to the `add_library` source list in `libs/bases/CMakeLists.txt` (after `src/SortCycle.cpp`, line ~42).

- [ ] **Step 5: Run to verify pass**

Run: `cmake --build --preset dev -j 10 --target tst_bases_view_config_ops && cd build-dev && ctest -R tst_bases_view_config_ops --output-on-failure`
Expected: all 18 slots PASS.

- [ ] **Step 6: Commit**
```bash
git add libs/bases/include/corbomite/bases/ViewConfigOps.h libs/bases/src/ViewConfigOps.cpp libs/bases/CMakeLists.txt libs/bases/tests/tst_view_config_ops.cpp libs/bases/tests/CMakeLists.txt
git commit -m "feat(bases): ViewConfigOps — pure column/sort/group/view-CRUD mutators"
```

---

### Task 2: Inline-edit polish — delegate role-constant cleanup

**Files:**
- Modify: `libs/bases/src/BasesCellDelegate.cpp`

Done early (and small) so later GUI tasks build on a clean delegate. Non-behavioural: `BasesTreeModel::ValueTypeRole`/`ValuePtrRole` have the same integer values as `BasesTableModel`'s (both `Qt::UserRole + 1/+2`), so this is a pure clarity rename.

- [ ] **Step 1: Swap the role references**

In `libs/bases/src/BasesCellDelegate.cpp`, replace every `BasesTableModel::ValueTypeRole` with `BasesTreeModel::ValueTypeRole` and every `BasesTableModel::ValuePtrRole` with `BasesTreeModel::ValuePtrRole`. There are 6 occurrences (lines ~33, 47, 64, 67, 74, 81, 137, 179 — search for `BasesTableModel::`). Then remove the now-unused include:
```cpp
#include "corbomite/bases/BasesTableModel.h"
```
(Keep `#include "corbomite/bases/BasesTreeModel.h"`.)

- [ ] **Step 2: Build + run the bases suite**

Run: `cmake --build --preset dev -j 10 && cd build-dev && ctest -R tst_bases --output-on-failure`
Expected: clean build; all bases tests green (no behavioural change).

- [ ] **Step 3: Commit**
```bash
git add libs/bases/src/BasesCellDelegate.cpp
git commit -m "refactor(bases): delegate reads BasesTreeModel role constants (was BasesTableModel)"
```

---

### Task 3: `PropertiesMenuPanel` — column visibility + reorder popup

**Files:**
- Create: `libs/bases/include/corbomite/bases/PropertiesMenuPanel.h`, `libs/bases/src/PropertiesMenuPanel.cpp`
- Modify: `libs/bases/CMakeLists.txt`

GUI surface — verified by build + launch (logic is `ViewConfigOps`, already tested). The panel is a `QFrame` with `Qt::Popup`. It holds a `QListWidget` (`InternalMove` drag for reorder) where each item has a checkbox (visible) and stores its `PropertyId`. The owner passes in the current `order`, the full property list, and two callbacks: one fired after any mutation (to recompute+save), reading the mutated `order` the panel owns a pointer to.

- [ ] **Step 1: Implement the header**

Create `libs/bases/include/corbomite/bases/PropertiesMenuPanel.h`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "PropertyId.h"

#include <QFrame>
#include <QVector>
#include <functional>

class QListWidget;

namespace Corbomite::Bases {

/// Popup panel listing every available property with a visible-checkbox and a
/// drag handle for reorder. Mutates the QVector<PropertyId> the owner passes by
/// pointer (the active view's `order`), then invokes `onChanged` so the owner
/// can recompute + persist.
class PropertiesMenuPanel : public QFrame
{
    Q_OBJECT
public:
    explicit PropertiesMenuPanel(QWidget *parent = nullptr);

    /// (Re)build the row list. `order` is the live order vector (mutated in
    /// place); `allProps` is every property the panel may show/hide.
    void setState(QVector<PropertyId> *order, const QVector<PropertyId> &allProps,
                  std::function<QString(const PropertyId &)> displayName);
    void setOnChanged(std::function<void()> cb) { m_onChanged = std::move(cb); }

private:
    void rebuild();
    void onItemChanged();   // checkbox toggled
    void onRowsMoved();     // drag reorder finished

    QListWidget *m_list = nullptr;
    QVector<PropertyId> *m_order = nullptr;
    QVector<PropertyId> m_allProps;
    std::function<QString(const PropertyId &)> m_displayName;
    std::function<void()> m_onChanged;
    bool m_updating = false;
};

}  // namespace Corbomite::Bases
```

- [ ] **Step 2: Implement the source**

Create `libs/bases/src/PropertiesMenuPanel.cpp`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/PropertiesMenuPanel.h"

#include "corbomite/bases/ViewConfigOps.h"

#include <KLocalizedString>

#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QVariant>

namespace Corbomite::Bases {

namespace {
constexpr int PropRole = Qt::UserRole + 1;
QVariant toVariant(const PropertyId &p) {
    return QVariant::fromValue(QStringList{QString::number(int(p.kind)), p.name});
}
PropertyId fromVariant(const QVariant &v) {
    const QStringList s = v.toStringList();
    if (s.size() != 2) return {};
    return PropertyId{PropertyKind(s[0].toInt()), s[1]};
}
}  // namespace

PropertiesMenuPanel::PropertiesMenuPanel(QWidget *parent)
    : QFrame(parent)
{
    setWindowFlags(Qt::Popup);
    setFrameShape(QFrame::StyledPanel);
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(4, 4, 4, 4);

    m_list = new QListWidget(this);
    m_list->setDragDropMode(QAbstractItemView::InternalMove);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    root->addWidget(m_list);

    auto *hideAll = new QPushButton(i18n("Hide all"), this);
    root->addWidget(hideAll);

    connect(m_list, &QListWidget::itemChanged, this, &PropertiesMenuPanel::onItemChanged);
    connect(m_list->model(), &QAbstractItemModel::rowsMoved,
            this, &PropertiesMenuPanel::onRowsMoved);
    connect(hideAll, &QPushButton::clicked, this, [this]() {
        if (!m_order) return;
        hideAllColumns(*m_order);
        rebuild();
        if (m_onChanged) m_onChanged();
    });
}

void PropertiesMenuPanel::setState(QVector<PropertyId> *order,
                                   const QVector<PropertyId> &allProps,
                                   std::function<QString(const PropertyId &)> displayName)
{
    m_order = order;
    m_allProps = allProps;
    m_displayName = std::move(displayName);
    rebuild();
}

void PropertiesMenuPanel::rebuild()
{
    if (!m_order) return;
    m_updating = true;
    m_list->clear();
    // Visible columns first (in `order`), then hidden ones (from allProps).
    QVector<PropertyId> ordered = *m_order;
    for (const auto &p : m_allProps)
        if (!ordered.contains(p)) ordered.push_back(p);
    for (const auto &p : ordered) {
        auto *it = new QListWidgetItem(m_displayName ? m_displayName(p) : p.name, m_list);
        it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable
                     | Qt::ItemIsUserCheckable | Qt::ItemIsDragEnabled);
        it->setCheckState(m_order->contains(p) ? Qt::Checked : Qt::Unchecked);
        it->setData(PropRole, toVariant(p));
    }
    m_updating = false;
}

void PropertiesMenuPanel::onItemChanged()
{
    if (m_updating || !m_order) return;
    // Rebuild `order` from the visible (checked) rows, in current row order.
    QVector<PropertyId> next;
    for (int i = 0; i < m_list->count(); ++i) {
        auto *it = m_list->item(i);
        if (it->checkState() == Qt::Checked) next.push_back(fromVariant(it->data(PropRole)));
    }
    *m_order = next;
    if (m_onChanged) m_onChanged();
}

void PropertiesMenuPanel::onRowsMoved()
{
    if (m_updating || !m_order) return;
    QVector<PropertyId> next;
    for (int i = 0; i < m_list->count(); ++i) {
        auto *it = m_list->item(i);
        if (it->checkState() == Qt::Checked) next.push_back(fromVariant(it->data(PropRole)));
    }
    *m_order = next;
    if (m_onChanged) m_onChanged();
}

}  // namespace Corbomite::Bases
```

> Reorder + visibility both rederive `*m_order` from the checked rows in row order — drag a hidden row above a visible one and it simply stays hidden until checked. This sidesteps the "Add property" combo: every property is already a (possibly-unchecked) row, so checking it shows it. The spec's "Add property" footer is satisfied by the inline checkboxes; no separate control is needed.

Add `src/PropertiesMenuPanel.cpp` to `libs/bases/CMakeLists.txt`.

- [ ] **Step 3: Build**

Run: `cmake --build --preset dev -j 10 --target corbomite-bases`
Expected: compiles clean. (Wired into `BasesView` in Task 6.)

- [ ] **Step 4: Commit**
```bash
git add libs/bases/include/corbomite/bases/PropertiesMenuPanel.h libs/bases/src/PropertiesMenuPanel.cpp libs/bases/CMakeLists.txt
git commit -m "feat(bases): PropertiesMenuPanel — column visibility + drag reorder popup"
```

---

### Task 4: `SortGroupMenuPanel` — sort stack + group-by popup

**Files:**
- Create: `libs/bases/include/corbomite/bases/SortGroupMenuPanel.h`, `libs/bases/src/SortGroupMenuPanel.cpp`
- Modify: `libs/bases/CMakeLists.txt`

- [ ] **Step 1: Implement the header**

Create `libs/bases/include/corbomite/bases/SortGroupMenuPanel.h`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "BasesViewConfig.h"
#include "PropertyId.h"

#include <QFrame>
#include <QVector>
#include <functional>

class QVBoxLayout;
class QComboBox;

namespace Corbomite::Bases {

/// Popup with a stack of sort-key rows (property + ASC/DESC + remove), an "Add
/// sort" button, and a group-by row (property incl. "(none)" + direction).
/// Mutates the BasesViewConfig the owner passes by pointer, then calls onChanged.
class SortGroupMenuPanel : public QFrame
{
    Q_OBJECT
public:
    explicit SortGroupMenuPanel(QWidget *parent = nullptr);

    void setState(BasesViewConfig *cfg, const QVector<PropertyId> &allProps,
                  std::function<QString(const PropertyId &)> displayName);
    void setOnChanged(std::function<void()> cb) { m_onChanged = std::move(cb); }

private:
    void rebuild();
    void changed();
    QComboBox *makePropertyCombo(const PropertyId &selected, bool withNone);

    BasesViewConfig *m_cfg = nullptr;
    QVector<PropertyId> m_allProps;
    std::function<QString(const PropertyId &)> m_displayName;
    std::function<void()> m_onChanged;

    QVBoxLayout *m_sortRows = nullptr;   // container for sort-key rows
    QComboBox *m_groupCombo = nullptr;
    QComboBox *m_groupDir = nullptr;
    bool m_updating = false;
};

}  // namespace Corbomite::Bases
```

- [ ] **Step 2: Implement the source**

Create `libs/bases/src/SortGroupMenuPanel.cpp`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/SortGroupMenuPanel.h"

#include "corbomite/bases/ViewConfigOps.h"

#include <KLocalizedString>

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QVariant>

namespace Corbomite::Bases {

namespace {
QVariant propVar(const PropertyId &p) { return QStringList{QString::number(int(p.kind)), p.name}; }
PropertyId propFrom(const QVariant &v) {
    const QStringList s = v.toStringList();
    return s.size() == 2 ? PropertyId{PropertyKind(s[0].toInt()), s[1]} : PropertyId{};
}
}  // namespace

SortGroupMenuPanel::SortGroupMenuPanel(QWidget *parent) : QFrame(parent)
{
    setWindowFlags(Qt::Popup);
    setFrameShape(QFrame::StyledPanel);
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(4, 4, 4, 4);

    root->addWidget(new QLabel(i18n("Sort"), this));
    m_sortRows = new QVBoxLayout();
    root->addLayout(m_sortRows);

    auto *addSort = new QPushButton(i18n("Add sort"), this);
    root->addWidget(addSort);
    connect(addSort, &QPushButton::clicked, this, [this]() {
        if (!m_cfg) return;
        // Append the first property not already a sort key.
        for (const auto &p : m_allProps) {
            bool used = false;
            for (const auto &k : m_cfg->sort) if (k.property == p) { used = true; break; }
            if (!used) { addSortKey(m_cfg->sort, p, QStringLiteral("ASC")); break; }
        }
        rebuild();
        changed();
    });

    root->addWidget(new QLabel(i18n("Group by"), this));
    auto *groupRow = new QHBoxLayout();
    m_groupCombo = new QComboBox(this);
    m_groupDir = new QComboBox(this);
    m_groupDir->addItems({QStringLiteral("ASC"), QStringLiteral("DESC")});
    groupRow->addWidget(m_groupCombo, 1);
    groupRow->addWidget(m_groupDir);
    root->addLayout(groupRow);

    connect(m_groupCombo, &QComboBox::currentIndexChanged, this, [this](int) {
        if (m_updating || !m_cfg) return;
        const QVariant v = m_groupCombo->currentData();
        if (v.isValid() && !v.toStringList().isEmpty())
            setGroupBy(*m_cfg, propFrom(v), m_groupDir->currentText());
        else
            setGroupBy(*m_cfg, std::nullopt, QString{});
        changed();
    });
    connect(m_groupDir, &QComboBox::currentTextChanged, this, [this](const QString &d) {
        if (m_updating || !m_cfg || !m_cfg->groupBy.has_value()) return;
        m_cfg->groupBy->direction = d;
        changed();
    });
}

QComboBox *SortGroupMenuPanel::makePropertyCombo(const PropertyId &selected, bool withNone)
{
    auto *c = new QComboBox(this);
    if (withNone) c->addItem(i18n("(none)"), QVariant{});
    int sel = withNone ? 0 : -1;
    for (const auto &p : m_allProps) {
        c->addItem(m_displayName ? m_displayName(p) : p.name, propVar(p));
        if (p == selected) sel = c->count() - 1;
    }
    if (sel >= 0) c->setCurrentIndex(sel);
    return c;
}

void SortGroupMenuPanel::setState(BasesViewConfig *cfg, const QVector<PropertyId> &allProps,
                                  std::function<QString(const PropertyId &)> displayName)
{
    m_cfg = cfg;
    m_allProps = allProps;
    m_displayName = std::move(displayName);
    rebuild();
}

void SortGroupMenuPanel::rebuild()
{
    if (!m_cfg) return;
    m_updating = true;

    // Clear existing sort rows.
    while (QLayoutItem *item = m_sortRows->takeAt(0)) {
        if (QWidget *w = item->widget()) w->deleteLater();
        delete item;
    }
    // One row per sort key.
    for (int i = 0; i < m_cfg->sort.size(); ++i) {
        const SortKey key = m_cfg->sort[i];
        auto *rowWidget = new QWidget(this);
        auto *row = new QHBoxLayout(rowWidget);
        row->setContentsMargins(0, 0, 0, 0);
        QComboBox *prop = makePropertyCombo(key.property, false);
        auto *dir = new QComboBox(rowWidget);
        dir->addItems({QStringLiteral("ASC"), QStringLiteral("DESC")});
        dir->setCurrentText(key.direction);
        auto *remove = new QPushButton(i18n("✕"), rowWidget);
        remove->setFixedWidth(28);
        row->addWidget(prop, 1);
        row->addWidget(dir);
        row->addWidget(remove);
        m_sortRows->addWidget(rowWidget);

        const PropertyId original = key.property;
        connect(prop, &QComboBox::currentIndexChanged, this, [this, original, prop]() {
            if (m_updating || !m_cfg) return;
            const PropertyId next = propFrom(prop->currentData());
            const int idx = [&]{ for (int j = 0; j < m_cfg->sort.size(); ++j)
                                     if (m_cfg->sort[j].property == original) return j; return -1; }();
            if (idx >= 0 && !next.name.isEmpty()) { m_cfg->sort[idx].property = next; changed(); }
        });
        connect(dir, &QComboBox::currentTextChanged, this, [this, original](const QString &d) {
            if (m_updating || !m_cfg) return;
            setSortDirection(m_cfg->sort, original, d);
            changed();
        });
        connect(remove, &QPushButton::clicked, this, [this, original]() {
            if (!m_cfg) return;
            removeSortKey(m_cfg->sort, original);
            rebuild();
            changed();
        });
    }

    // Group-by combo.
    m_groupCombo->clear();
    m_groupCombo->addItem(i18n("(none)"), QVariant{});
    int gsel = 0;
    for (const auto &p : m_allProps) {
        m_groupCombo->addItem(m_displayName ? m_displayName(p) : p.name, propVar(p));
        if (m_cfg->groupBy.has_value() && m_cfg->groupBy->property == p)
            gsel = m_groupCombo->count() - 1;
    }
    m_groupCombo->setCurrentIndex(gsel);
    if (m_cfg->groupBy.has_value()) m_groupDir->setCurrentText(m_cfg->groupBy->direction);

    m_updating = false;
}

void SortGroupMenuPanel::changed() { if (m_onChanged) m_onChanged(); }

}  // namespace Corbomite::Bases
```

Add `src/SortGroupMenuPanel.cpp` to `libs/bases/CMakeLists.txt`.

- [ ] **Step 3: Build**

Run: `cmake --build --preset dev -j 10 --target corbomite-bases`
Expected: compiles clean.

- [ ] **Step 4: Commit**
```bash
git add libs/bases/include/corbomite/bases/SortGroupMenuPanel.h libs/bases/src/SortGroupMenuPanel.cpp libs/bases/CMakeLists.txt
git commit -m "feat(bases): SortGroupMenuPanel — multi-key sort stack + group-by popup"
```

---

### Task 5: `ViewsMenuPanel` — view CRUD popup

**Files:**
- Create: `libs/bases/include/corbomite/bases/ViewsMenuPanel.h`, `libs/bases/src/ViewsMenuPanel.cpp`
- Modify: `libs/bases/CMakeLists.txt`

- [ ] **Step 1: Implement the header**

Create `libs/bases/include/corbomite/bases/ViewsMenuPanel.h`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QFrame>
#include <QString>
#include <functional>

class QListWidget;

namespace Corbomite::Bases {

class BasesQuery;

/// Popup listing the query's views with Rename / Duplicate / Delete / Set
/// default actions. Mutates BasesQuery via ViewConfigOps, then calls onChanged
/// (which the owner uses to re-populate the switcher + persist). Selecting a
/// view calls onActivate(name).
class ViewsMenuPanel : public QFrame
{
    Q_OBJECT
public:
    explicit ViewsMenuPanel(QWidget *parent = nullptr);

    void setState(BasesQuery *query, const QString &activeName);
    void setOnChanged(std::function<void()> cb) { m_onChanged = std::move(cb); }
    void setOnActivate(std::function<void(const QString &)> cb) { m_onActivate = std::move(cb); }

private:
    void rebuild();
    QString selectedName() const;

    QListWidget *m_list = nullptr;
    BasesQuery *m_query = nullptr;
    QString m_activeName;
    std::function<void()> m_onChanged;
    std::function<void(const QString &)> m_onActivate;
};

}  // namespace Corbomite::Bases
```

- [ ] **Step 2: Implement the source**

Create `libs/bases/src/ViewsMenuPanel.cpp`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/ViewsMenuPanel.h"

#include "corbomite/bases/BasesQuery.h"
#include "corbomite/bases/BasesViewConfig.h"
#include "corbomite/bases/ViewConfigOps.h"

#include <KLocalizedString>

#include <QInputDialog>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

namespace Corbomite::Bases {

ViewsMenuPanel::ViewsMenuPanel(QWidget *parent) : QFrame(parent)
{
    setWindowFlags(Qt::Popup);
    setFrameShape(QFrame::StyledPanel);
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(4, 4, 4, 4);

    m_list = new QListWidget(this);
    root->addWidget(m_list);
    connect(m_list, &QListWidget::itemDoubleClicked, this, [this]() {
        const QString n = selectedName();
        if (!n.isEmpty() && m_onActivate) m_onActivate(n);
    });

    auto *rename = new QPushButton(i18n("Rename…"), this);
    auto *dup    = new QPushButton(i18n("Duplicate"), this);
    auto *del    = new QPushButton(i18n("Delete"), this);
    auto *def    = new QPushButton(i18n("Set as default"), this);
    root->addWidget(rename);
    root->addWidget(dup);
    root->addWidget(del);
    root->addWidget(def);

    connect(rename, &QPushButton::clicked, this, [this]() {
        const QString cur = selectedName();
        if (cur.isEmpty() || !m_query) return;
        bool ok = false;
        const QString next = QInputDialog::getText(this, i18n("Rename view"),
            i18n("New name:"), QLineEdit::Normal, cur, &ok);
        if (ok && !next.isEmpty() && renameView(*m_query, cur, next)) {
            m_activeName = next; rebuild(); if (m_onChanged) m_onChanged();
        }
    });
    connect(dup, &QPushButton::clicked, this, [this]() {
        const QString cur = selectedName();
        if (cur.isEmpty() || !m_query) return;
        if (duplicateView(*m_query, cur, i18n("%1 copy", cur))) {
            rebuild(); if (m_onChanged) m_onChanged();
        }
    });
    connect(del, &QPushButton::clicked, this, [this]() {
        const QString cur = selectedName();
        if (cur.isEmpty() || !m_query) return;
        if (deleteView(*m_query, cur)) { rebuild(); if (m_onChanged) m_onChanged(); }
    });
    connect(def, &QPushButton::clicked, this, [this]() {
        const QString cur = selectedName();
        if (cur.isEmpty() || !m_query) return;
        if (setDefaultView(*m_query, cur)) { rebuild(); if (m_onChanged) m_onChanged(); }
    });
}

void ViewsMenuPanel::setState(BasesQuery *query, const QString &activeName)
{
    m_query = query;
    m_activeName = activeName;
    rebuild();
}

void ViewsMenuPanel::rebuild()
{
    m_list->clear();
    if (!m_query) return;
    for (const auto &v : m_query->views) {
        if (!v) continue;
        const QString label = (v->name == m_activeName)
            ? i18n("%1 (active)", v->name) : v->name;
        auto *it = new QListWidgetItem(label, m_list);
        it->setData(Qt::UserRole, v->name);
        if (v->name == m_activeName) m_list->setCurrentItem(it);
    }
}

QString ViewsMenuPanel::selectedName() const
{
    auto *it = m_list->currentItem();
    return it ? it->data(Qt::UserRole).toString() : QString{};
}

}  // namespace Corbomite::Bases
```

Add `src/ViewsMenuPanel.cpp` to `libs/bases/CMakeLists.txt`.

- [ ] **Step 3: Build**

Run: `cmake --build --preset dev -j 10 --target corbomite-bases`
Expected: compiles clean.

- [ ] **Step 4: Commit**
```bash
git add libs/bases/include/corbomite/bases/ViewsMenuPanel.h libs/bases/src/ViewsMenuPanel.cpp libs/bases/CMakeLists.txt
git commit -m "feat(bases): ViewsMenuPanel — view rename/duplicate/delete/set-default popup"
```

---

### Task 6: Wire the three toolbar buttons into `BasesView`

**Files:**
- Modify: `libs/bases/include/corbomite/bases/BasesView.h`, `libs/bases/src/BasesView.cpp`

This connects Tasks 3-5 to the live view. Each `QToolButton` (with a small symbolic icon + text) shows its panel beneath itself; after any mutation the shared handler recomputes + persists.

- [ ] **Step 1: Add members + includes to `BasesView.h`**

In `BasesView.h`, add forward declarations and members. After the existing `class QComboBox;` line add:
```cpp
class QToolButton;
```
After `class QueryController;` (in the `Corbomite::Bases` namespace) add:
```cpp
class PropertiesMenuPanel;
class SortGroupMenuPanel;
class ViewsMenuPanel;
```
In the private members block (after `QComboBox *m_viewSelector = nullptr;`) add:
```cpp
    QToolButton *m_propsBtn = nullptr;
    QToolButton *m_sortBtn = nullptr;
    QToolButton *m_viewsBtn = nullptr;
    PropertiesMenuPanel *m_propsPanel = nullptr;
    SortGroupMenuPanel *m_sortPanel = nullptr;
    ViewsMenuPanel *m_viewsPanel = nullptr;
```
Add private helpers in the `private:` section:
```cpp
    void onConfigMutated();              // recompute + persist after a panel edit
    QVector<PropertyId> availableProperties() const;
    QString displayNameFor(const PropertyId &pid) const;
    void showPanelUnder(QWidget *panel, QToolButton *button);
```

- [ ] **Step 2: Build the toolbar buttons + panels in the `BasesView` ctor**

In `BasesView.cpp`, add includes near the top:
```cpp
#include "corbomite/bases/PropertiesMenuPanel.h"
#include "corbomite/bases/SortGroupMenuPanel.h"
#include "corbomite/bases/ViewsMenuPanel.h"
```
and
```cpp
#include <QToolButton>
```
In the constructor, after `toolbar->addWidget(m_searchEdit);` and before `root->addLayout(toolbar);`, add:
```cpp
    m_propsBtn = new QToolButton(this);
    m_propsBtn->setText(i18n("Properties"));
    m_propsBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);
    toolbar->addWidget(m_propsBtn);

    m_sortBtn = new QToolButton(this);
    m_sortBtn->setText(i18n("Sort & group"));
    m_sortBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);
    toolbar->addWidget(m_sortBtn);

    m_viewsBtn = new QToolButton(this);
    m_viewsBtn->setText(i18n("Views"));
    m_viewsBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);
    toolbar->addWidget(m_viewsBtn);

    m_propsPanel = new PropertiesMenuPanel(this);
    m_sortPanel  = new SortGroupMenuPanel(this);
    m_viewsPanel = new ViewsMenuPanel(this);
    m_propsPanel->setOnChanged([this]() { onConfigMutated(); });
    m_sortPanel->setOnChanged([this]()  { onConfigMutated(); });
    m_viewsPanel->setOnChanged([this]() {
        populateViewSelector();
        requestSave();
    });
    m_viewsPanel->setOnActivate([this](const QString &name) {
        m_viewSelector->setCurrentText(name);   // triggers onViewSelectorChanged
    });

    connect(m_propsBtn, &QToolButton::clicked, this, [this]() {
        if (!m_activeView) return;
        m_propsPanel->setState(&m_activeView->order, availableProperties(),
                               [this](const PropertyId &p) { return displayNameFor(p); });
        showPanelUnder(m_propsPanel, m_propsBtn);
    });
    connect(m_sortBtn, &QToolButton::clicked, this, [this]() {
        if (!m_activeView) return;
        m_sortPanel->setState(m_activeView, availableProperties(),
                              [this](const PropertyId &p) { return displayNameFor(p); });
        showPanelUnder(m_sortPanel, m_sortBtn);
    });
    connect(m_viewsBtn, &QToolButton::clicked, this, [this]() {
        if (!m_query) return;
        m_viewsPanel->setState(m_query.get(), m_activeView ? m_activeView->name : QString{});
        showPanelUnder(m_viewsPanel, m_viewsBtn);
    });
```

- [ ] **Step 3: Implement the helpers in `BasesView.cpp`**

Add these methods (anywhere in the file, e.g. after `onSectionMoved`):
```cpp
void BasesView::onConfigMutated()
{
    if (m_controller) m_controller->recomputeNow();
    if (m_table) m_table->expandAll();
    requestSave();
}

QVector<PropertyId> BasesView::availableProperties() const
{
    if (m_controller && m_controller->result())
        return m_controller->result()->properties();
    return m_activeView ? m_activeView->order : QVector<PropertyId>{};
}

QString BasesView::displayNameFor(const PropertyId &pid) const
{
    if (m_query) {
        auto it = m_query->properties.constFind(pid);
        if (it != m_query->properties.constEnd() && !it->displayName.isEmpty())
            return it->displayName;
    }
    return pid.name;
}

void BasesView::showPanelUnder(QWidget *panel, QToolButton *button)
{
    const QPoint below = button->mapToGlobal(QPoint(0, button->height()));
    panel->move(below);
    panel->show();
    panel->raise();
}
```
Add the include `#include "corbomite/bases/BasesQueryResult.h"` if not already present (for `result()->properties()`).

- [ ] **Step 4: Build + launch verify**

Run: `cmake --build --preset dev -j 10`
Launch: `./build-dev/Corbomite <a vault with a multi-view grouped .base>`
Verify, on opening a `.base`:
- **Properties** button → popup lists all columns; unchecking hides a column, re-checking shows it, dragging reorders; "Hide all" empties the table columns. Each change reflects immediately and the `.base` `order:`/per-view writes persist.
- **Sort & group** → add a sort key, flip a direction, add a second key (table re-sorts multi-key); set a group-by property (groups appear), set "(none)" (groups collapse to flat). `.base` `sort:`/`groupBy:` persist.
- **Views** → list shows views with "(active)"; Duplicate adds "X copy"; Rename renames; Delete is refused on the last view; Set-default reorders. The switcher combo updates; `.base` persists.

Expected: all behaviours work; no crash; popups dismiss on outside-click.

- [ ] **Step 5: Run the bases suite (no regressions)**

Run: `cd build-dev && ctest -R tst_bases --output-on-failure`
Expected: all green.

- [ ] **Step 6: Commit**
```bash
git add libs/bases/include/corbomite/bases/BasesView.h libs/bases/src/BasesView.cpp
git commit -m "feat(bases): wire Properties / Sort+group / Views toolbar popups into BasesView"
```

---

### Task 7: `PropertiesDrawer` — per-row frontmatter editor

**Files:**
- Create: `libs/bases/include/corbomite/bases/PropertiesDrawer.h`, `libs/bases/src/PropertiesDrawer.cpp`
- Modify: `libs/bases/CMakeLists.txt`

A `QWidget` shown in the right pane of a `QSplitter`. Given a selected `BasesEntry`'s file and its frontmatter keys/values, it renders a `QFormLayout` of label+editor rows. Edits commit through `FileManager::processFrontMatter`.

- [ ] **Step 1: Implement the header**

Create `libs/bases/include/corbomite/bases/PropertiesDrawer.h`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QWidget>

namespace Corbomite {
class FileManager;
class TFile;
}

class QFormLayout;
class QLabel;

namespace Corbomite::Bases {

class BasesEntry;

/// Right-pane editor for the selected entry's frontmatter. Renders a form of
/// label + type-appropriate editor; commits via FileManager::processFrontMatter.
class PropertiesDrawer : public QWidget
{
    Q_OBJECT
public:
    explicit PropertiesDrawer(QWidget *parent = nullptr);

    void setFileManager(FileManager *fm) { m_fm = fm; }
    /// Populate from `entry` (its file + frontmatter). Null clears the form.
    void showEntry(BasesEntry *entry);

private:
    void clearForm();
    void commit(const QString &key, const QVariant &value);

    FileManager *m_fm = nullptr;
    TFile *m_file = nullptr;
    QLabel *m_title = nullptr;
    QFormLayout *m_form = nullptr;
};

}  // namespace Corbomite::Bases
```

- [ ] **Step 2: Implement the source**

Create `libs/bases/src/PropertiesDrawer.cpp`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/PropertiesDrawer.h"

#include "corbomite/bases/BasesEntry.h"
#include "corbomite/bases/PropertyId.h"
#include "corbomite/bases/Values.h"

#include "corbomite/vault/FileManager.h"
#include "corbomite/vault/TFile.h"

#include <KLocalizedString>

#include <QCheckBox>
#include <QDateEdit>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QVariantMap>

namespace Corbomite::Bases {

PropertiesDrawer::PropertiesDrawer(QWidget *parent) : QWidget(parent)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(6, 6, 6, 6);

    m_title = new QLabel(i18n("(no selection)"), this);
    QFont f = m_title->font(); f.setBold(true); m_title->setFont(f);
    root->addWidget(m_title);

    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    auto *formHost = new QWidget(scroll);
    m_form = new QFormLayout(formHost);
    scroll->setWidget(formHost);
    root->addWidget(scroll, 1);

    auto *addField = new QPushButton(i18n("+ Add field"), this);
    root->addWidget(addField);
    connect(addField, &QPushButton::clicked, this, [this]() {
        if (!m_file) return;
        bool ok = false;
        const QString key = QInputDialog::getText(this, i18n("Add field"),
            i18n("Property name:"), QLineEdit::Normal, QString{}, &ok);
        if (ok && !key.isEmpty()) commit(key, QString{});
    });
}

void PropertiesDrawer::clearForm()
{
    while (m_form->rowCount() > 0) m_form->removeRow(0);
}

void PropertiesDrawer::showEntry(BasesEntry *entry)
{
    clearForm();
    m_file = entry ? entry->file() : nullptr;
    if (!entry || !m_file) {
        m_title->setText(i18n("(no selection)"));
        return;
    }
    m_title->setText(m_file->path());

    const QStringList keys = entry->getPropertyKeys();
    for (const QString &key : keys) {
        const PropertyId pid{PropertyKind::Note, key};
        const ValuePtr v = entry->getValue(pid);
        const QString type = v ? v->type() : QStringLiteral("String");

        if (type == QLatin1String("Boolean")) {
            auto *cb = new QCheckBox(this);
            auto *b = dynamic_cast<BooleanValue *>(v.get());
            cb->setChecked(b && b->data());
            connect(cb, &QCheckBox::toggled, this, [this, key](bool on) { commit(key, on); });
            m_form->addRow(key, cb);
        } else if (type == QLatin1String("Number")) {
            auto *sb = new QDoubleSpinBox(this);
            sb->setDecimals(6); sb->setRange(-1e15, 1e15);
            auto *n = dynamic_cast<NumberValue *>(v.get());
            sb->setValue(n ? n->data() : 0.0);
            connect(sb, &QDoubleSpinBox::editingFinished, this,
                    [this, key, sb]() { commit(key, sb->value()); });
            m_form->addRow(key, sb);
        } else if (type == QLatin1String("Date")) {
            auto *de = new QDateEdit(this);
            de->setCalendarPopup(true);
            if (auto *d = dynamic_cast<DateValue *>(v.get())) de->setDate(d->dateTime().date());
            connect(de, &QDateEdit::editingFinished, this,
                    [this, key, de]() { commit(key, de->date().toString(Qt::ISODate)); });
            m_form->addRow(key, de);
        } else {
            auto *le = new QLineEdit(this);
            le->setText(v ? v->toString() : QString{});
            connect(le, &QLineEdit::editingFinished, this,
                    [this, key, le]() { commit(key, le->text()); });
            m_form->addRow(key, le);
        }
    }
}

void PropertiesDrawer::commit(const QString &key, const QVariant &value)
{
    if (!m_fm || !m_file) return;
    m_fm->processFrontMatter(m_file, [&](QVariantMap &fm) { fm.insert(key, value); });
    // The resulting cacheChanged → recompute → BasesView re-selects + re-populates us.
}

}  // namespace Corbomite::Bases
```

> `entry->getValue(PropertyId{Note, key})` returns the typed value; `getPropertyKeys()` returns the raw frontmatter keys. `TFile::path()` is the file's vault path. Verify `BasesEntry::getValue` is public (it is — `BasesEntry.h:58`).

Add `src/PropertiesDrawer.cpp` to `libs/bases/CMakeLists.txt`.

- [ ] **Step 3: Build**

Run: `cmake --build --preset dev -j 10 --target corbomite-bases`
Expected: compiles clean.

- [ ] **Step 4: Commit**
```bash
git add libs/bases/include/corbomite/bases/PropertiesDrawer.h libs/bases/src/PropertiesDrawer.cpp libs/bases/CMakeLists.txt
git commit -m "feat(bases): PropertiesDrawer — per-row frontmatter editor pane"
```

---

### Task 8: Mount the drawer in a QSplitter + selection tracking + toggle button

**Files:**
- Modify: `libs/bases/include/corbomite/bases/BasesView.h`, `libs/bases/src/BasesView.cpp`

- [ ] **Step 1: Add members to `BasesView.h`**

Add forward declarations:
```cpp
class QSplitter;
```
and in the `Corbomite::Bases` namespace:
```cpp
class PropertiesDrawer;
```
Add members (after the panel members from Task 6):
```cpp
    QToolButton *m_drawerBtn = nullptr;
    QSplitter *m_splitter = nullptr;
    PropertiesDrawer *m_drawer = nullptr;
```
Add a private slot in the `private Q_SLOTS:` block:
```cpp
    void onSelectionChanged();
```

- [ ] **Step 2: Build the splitter + drawer + toggle in the ctor**

In `BasesView.cpp` add includes:
```cpp
#include "corbomite/bases/PropertiesDrawer.h"
#include <QSplitter>
```
In the ctor, add a drawer-toggle button to the toolbar (after the Views button):
```cpp
    m_drawerBtn = new QToolButton(this);
    m_drawerBtn->setText(i18n("Properties pane"));
    m_drawerBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_drawerBtn->setCheckable(true);
    toolbar->addWidget(m_drawerBtn);
```
Replace the current table-mounting line `root->addWidget(m_table, 1);` with a splitter that holds the table + drawer:
```cpp
    m_splitter = new QSplitter(Qt::Horizontal, this);
    m_splitter->addWidget(m_table);
    m_drawer = new PropertiesDrawer(m_splitter);
    m_drawer->hide();                         // collapsed until toggled
    m_splitter->addWidget(m_drawer);
    m_splitter->setStretchFactor(0, 1);
    m_splitter->setStretchFactor(1, 0);
    root->addWidget(m_splitter, 1);
```
Wire the toggle:
```cpp
    connect(m_drawerBtn, &QToolButton::toggled, this, [this](bool on) {
        m_drawer->setVisible(on);
        if (on) onSelectionChanged();
    });
```

- [ ] **Step 3: Inject the FileManager + wire selection tracking**

In `BasesView::setServices`, after `m_fm = fileManager;`, add:
```cpp
    if (m_drawer) m_drawer->setFileManager(m_fm);
```
(`m_drawer` is created in the ctor, which runs before `setServices`, so this is safe.)

In `rebuildLayout()`, after `m_table->setModel(m_model.get());`, connect the selection model (it is recreated whenever the model is set):
```cpp
    connect(m_table->selectionModel(), &QItemSelectionModel::currentRowChanged,
            this, &BasesView::onSelectionChanged, Qt::UniqueConnection);
```
Add `#include <QItemSelectionModel>` if needed.

Implement the slot:
```cpp
void BasesView::onSelectionChanged()
{
    if (!m_drawer || !m_drawer->isVisible() || !m_model) return;
    const QModelIndex idx = m_table->currentIndex();
    if (!idx.isValid() || m_model->isGroupRow(idx)) { m_drawer->showEntry(nullptr); return; }
    // entryAt is private to the model; expose via a public accessor.
    m_drawer->showEntry(m_model->entryForIndex(idx));
}
```

- [ ] **Step 4: Add the public `entryForIndex` accessor to `BasesTreeModel`**

In `libs/bases/include/corbomite/bases/BasesTreeModel.h`, add a public method (near `valueAt`):
```cpp
    BasesEntry *entryForIndex(const QModelIndex &index) const;  // null for group rows
```
In `libs/bases/src/BasesTreeModel.cpp`, implement it by delegating to the existing private `entryAt`:
```cpp
BasesEntry *BasesTreeModel::entryForIndex(const QModelIndex &index) const
{
    return entryAt(index);
}
```
Add `#include "corbomite/bases/BasesTreeModel.h"` to `BasesView.cpp` (already present from D.2).

- [ ] **Step 5: Build + launch verify**

Run: `cmake --build --preset dev -j 10`
Launch and open a `.base`. Toggle **Properties pane** on: the right drawer appears. Select a row → the drawer shows that note's frontmatter fields with editors. Edit a text field and press Enter / a checkbox / a number → the `.md` frontmatter updates on disk and the table cell reflects the change after recompute. Toggle off → drawer hides.
Expected: drawer tracks selection; edits persist via `processFrontMatter`; no crash on group-row selection (drawer clears).

- [ ] **Step 6: Run the bases suite**

Run: `cd build-dev && ctest -R tst_bases --output-on-failure`
Expected: all green.

- [ ] **Step 7: Commit**
```bash
git add libs/bases/include/corbomite/bases/BasesView.h libs/bases/src/BasesView.cpp libs/bases/include/corbomite/bases/BasesTreeModel.h libs/bases/src/BasesTreeModel.cpp
git commit -m "feat(bases): mount PropertiesDrawer in a QSplitter; selection-tracked frontmatter editing"
```

---

### Task 9: Full suite + close D.3

- [ ] **Step 1: Full bases suite**

Run: `cd build-dev && ctest -R tst_bases --output-on-failure`
Expected: all green (`tst_bases_view_config_ops` + sortcycle + tree_model + cell_delegate + value/dsl/entry suites).

- [ ] **Step 2: Full tree build (no new regressions)**

Run: `cmake --build --preset dev -j 10`
Expected: clean. The pre-existing foundation-port failures outside `libs/bases` are unrelated — confirm no *new* failures.

- [ ] **Step 3: Final inline-edit end-to-end check**

Launch, open a `.base`; double-click a Note cell, edit it inline (line/number/checkbox/date editor per type), confirm the `.md` frontmatter + the cell update. (This exercises the Task 2 cleanup + the D.2 `setData` path on the `QTreeView`.)

- [ ] **Step 4: Update tracking docs**

Per CONTRIBUTING-OPS Ritual 3 (cluster/sub-project done):
- `docs/PROJECT-STATE.md` — replace the D row's "D.3 remain" note: D.3 done; next D.4 (formula editor, filter builder, undo, export, drag, hover, context menu). Add a one-line Recent-decisions entry.
- `docs/superpowers/plans/INDEX.md` — D row status: "In progress (D.1, D.2, D.3 done)".
- `docs/decisions-archive.md` — append a dated D.3 closeout paragraph.

- [ ] **Step 5: Commit the close-out**
```bash
git add docs/PROJECT-STATE.md docs/superpowers/plans/INDEX.md docs/decisions-archive.md
git commit -m "docs(tracking): close out Cluster D.3 (Bases toolbar menus + properties drawer)"
```

---

## Definition of done

- `ViewConfigOps` helpers exist; `tst_bases_view_config_ops` passes (18 slots).
- Properties / Sort+group / Views popups open from toolbar buttons, mutate the active view config, recompute the table, and persist to the `.base`.
- Properties drawer toggles from the toolbar, tracks row selection, edits the selected note's frontmatter via `processFrontMatter`.
- Inline cell editing works end-to-end on the `QTreeView`; delegate references `BasesTreeModel` role constants.
- Full `libs/bases` suite green; clean build; tracking docs updated.
- No formula editor / filter builder / undo / export / drag / hover / context menu (D.4+); "Add formula" absent.

## Notes / risks

- **Popup dismiss vs toggle re-open.** `Qt::Popup` panels close on outside-click; clicking the same toolbar button while open may immediately re-open. If observed, guard with a short `QElapsedTimer` since-last-hide check in the button slot, or use `QToolButton::setPopupMode(QToolButton::InstantPopup)` with a `QMenu` wrapper. Verify during Task 6 Step 4.
- **Drawer re-population after recompute.** A drawer edit triggers `processFrontMatter` → `cacheChanged` → controller recompute → model reset → `currentRowChanged` → `onSelectionChanged` re-populates from the new selection. If selection is lost on reset, the drawer clears gracefully (shows "(no selection)"); acceptable for D.3. Persisting/restoring selection by file path across reset is a D.4 polish.
- **`BasesViewConfig` copyability** (Task 1 `duplicateView`) — relies on all members being copyable; they are (QString / shared_ptr / QVector / optional / QHash / QVariantMap). If a future non-copyable member is added, give `BasesViewConfig` an explicit clone.
- **Property display names** come from `BasesQuery::properties[pid].displayName` when set, else the raw key. Built-in `file.*` humanised names are not specially mapped here (a small follow-up; raw `file.mtime`-style names show meanwhile).
