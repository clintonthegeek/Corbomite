# Cluster D.2 — Bases Read-Side Rendering Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Render the already-computed Bases result in the view: collapsible group headings (label + count + summary cells), multi-key header-click sort with indicators, and rich Icon/Image/HTML cells.

**Architecture:** Replace the flat `QTableView` + `BasesTableModel` (`QAbstractTableModel`) with a `QTreeView` + new `BasesTreeModel` (`QAbstractItemModel`, 2-level: group nodes → entry leaves; ungrouped = flat). Extend `BasesCellDelegate` for group-heading styling + rich cells. Extend the existing `BasesView::onHeaderClicked` single-key cycle to multi-key via a pure helper. Header-sort persists via the existing `requestSave()` (no new write path).

**Tech Stack:** C++20, Qt6 (Widgets: QTreeView, QAbstractItemModel, QStyledItemDelegate, QTextDocument; Core), QtTest, CMake. Library: `libs/bases` (`Corbomite::Bases`).

---

## File structure

- **Create** `libs/bases/include/corbomite/bases/SortCycle.h` + `src/SortCycle.cpp` — pure `cycleHeaderSort(QVector<SortKey>&, PropertyId, bool shift)` (no Qt-widget deps; testable).
- **Create** `libs/bases/include/corbomite/bases/BasesTreeModel.h` + `src/BasesTreeModel.cpp` — the 2-level tree model. Supersedes `BasesTableModel` as `BasesView`'s model.
- **Modify** `libs/bases/src/BasesCellDelegate.cpp` (+ `.h` if needed) — group-heading styling (gated on `IsGroupRowRole`) + Icon/Image/HTML paint branches.
- **Modify** `libs/bases/include/corbomite/bases/BasesView.h` + `src/BasesView.cpp` — `QTableView`→`QTreeView`, `BasesTableModel`→`BasesTreeModel`, multi-key sort wiring, header-indicator painting.
- **Modify** `libs/bases/CMakeLists.txt` — add `src/SortCycle.cpp`, `src/BasesTreeModel.cpp`.
- **Create tests** `libs/bases/tests/tst_sortcycle.cpp`, `tst_bases_tree_model.cpp`; register in `libs/bases/tests/CMakeLists.txt`.
- `BasesTableModel.{h,cpp}` is left in place (other code may reference the role constants); `BasesView` simply stops using it. (A later cleanup may delete it once nothing references it.)

Build: `cmake --build --preset dev -j 10`. Test: `cd build-dev && ctest -R <name> --output-on-failure`.

---

### Task 1: Pure multi-key sort-cycle helper

**Files:**
- Create: `libs/bases/include/corbomite/bases/SortCycle.h`, `libs/bases/src/SortCycle.cpp`
- Modify: `libs/bases/CMakeLists.txt`
- Test: `libs/bases/tests/tst_sortcycle.cpp` (+ register in `tests/CMakeLists.txt`)

The behavior (matches the existing single-key handler, generalized): a **plain** click on column C — if C is already the *sole/primary* key, cycle its direction ASC→DESC→remove (when removed, remaining keys shift up); otherwise replace the whole sort with `[C ASC]`. A **shift** click on C — if C is already present, cycle that key's direction ASC→DESC→remove-just-C; else append `C ASC` (building a secondary/tertiary key) keeping existing keys.

- [ ] **Step 1: Write the failing test**

Create `libs/bases/tests/tst_sortcycle.cpp`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include "corbomite/bases/SortCycle.h"
#include "corbomite/bases/BasesViewConfig.h"

using namespace Corbomite::Bases;

namespace {
PropertyId note(const char *n) { return PropertyId{PropertyKind::Note, QString::fromLatin1(n)}; }
QString dirs(const QVector<SortKey> &s) {  // compact "a:ASC,b:DESC" for asserts
    QStringList parts;
    for (const auto &k : s) parts << k.property.name + QLatin1Char(':') + k.direction;
    return parts.join(QLatin1Char(','));
}
}

class TestSortCycle : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void testPlainClickEmptySetsAsc()
    {
        QVector<SortKey> s;
        cycleHeaderSort(s, note("a"), false);
        QCOMPARE(dirs(s), QStringLiteral("a:ASC"));
    }
    void testPlainClickPrimaryCyclesAscDescRemove()
    {
        QVector<SortKey> s{{note("a"), QStringLiteral("ASC")}};
        cycleHeaderSort(s, note("a"), false);
        QCOMPARE(dirs(s), QStringLiteral("a:DESC"));
        cycleHeaderSort(s, note("a"), false);
        QCOMPARE(dirs(s), QString{});                 // removed
    }
    void testPlainClickDifferentColumnReplaces()
    {
        QVector<SortKey> s{{note("a"), QStringLiteral("DESC")}};
        cycleHeaderSort(s, note("b"), false);
        QCOMPARE(dirs(s), QStringLiteral("b:ASC"));    // replaced, not appended
    }
    void testShiftClickAppendsSecondary()
    {
        QVector<SortKey> s{{note("a"), QStringLiteral("ASC")}};
        cycleHeaderSort(s, note("b"), true);
        QCOMPARE(dirs(s), QStringLiteral("a:ASC,b:ASC"));
    }
    void testShiftClickCyclesExistingKeyInPlace()
    {
        QVector<SortKey> s{{note("a"), QStringLiteral("ASC")}, {note("b"), QStringLiteral("ASC")}};
        cycleHeaderSort(s, note("a"), true);
        QCOMPARE(dirs(s), QStringLiteral("a:DESC,b:ASC")); // a flips, b kept, order kept
        cycleHeaderSort(s, note("a"), true);
        QCOMPARE(dirs(s), QStringLiteral("b:ASC"));         // a removed, b kept
    }
};

QTEST_APPLESS_MAIN(TestSortCycle)
#include "tst_sortcycle.moc"
```

- [ ] **Step 2: Register the test + run to confirm it fails (no SortCycle.h yet)**

In `libs/bases/tests/CMakeLists.txt`, append:
```cmake
add_executable(tst_bases_sortcycle tst_sortcycle.cpp)
add_test(NAME tst_bases_sortcycle COMMAND tst_bases_sortcycle)
target_link_libraries(tst_bases_sortcycle PRIVATE Qt6::Test Corbomite::Bases)
```
Run: `cmake --build --preset dev -j 10 --target tst_bases_sortcycle`
Expected: FAILS to compile (`SortCycle.h` missing).

- [ ] **Step 3: Implement the helper**

Create `libs/bases/include/corbomite/bases/SortCycle.h`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "BasesViewConfig.h"
#include "PropertyId.h"
#include <QVector>

namespace Corbomite::Bases {

/// Mutate `sort` for a header click on `clicked`. Plain click: if `clicked`
/// is the sole/primary key, cycle ASC->DESC->remove; else replace sort with
/// [clicked ASC]. Shift click: if present, cycle that key ASC->DESC->remove
/// in place; else append [clicked ASC] preserving existing keys.
void cycleHeaderSort(QVector<SortKey> &sort, const PropertyId &clicked, bool shiftHeld);

}  // namespace Corbomite::Bases
```

Create `libs/bases/src/SortCycle.cpp`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/SortCycle.h"

namespace Corbomite::Bases {

namespace {
// ASC -> DESC -> (empty = remove)
QString nextDir(const QString &d) {
    return d == QLatin1String("ASC") ? QStringLiteral("DESC") : QString{};
}
int indexOf(const QVector<SortKey> &s, const PropertyId &p) {
    for (int i = 0; i < s.size(); ++i) if (s[i].property == p) return i;
    return -1;
}
}  // namespace

void cycleHeaderSort(QVector<SortKey> &sort, const PropertyId &clicked, bool shiftHeld)
{
    if (shiftHeld) {
        const int i = indexOf(sort, clicked);
        if (i < 0) { sort.push_back({clicked, QStringLiteral("ASC")}); return; }
        const QString nd = nextDir(sort[i].direction);
        if (nd.isEmpty()) sort.remove(i);
        else sort[i].direction = nd;
        return;
    }
    // Plain click.
    if (!sort.isEmpty() && sort.front().property == clicked) {
        const QString nd = nextDir(sort.front().direction);
        sort.clear();
        if (!nd.isEmpty()) sort.push_back({clicked, nd});
        return;
    }
    sort.clear();
    sort.push_back({clicked, QStringLiteral("ASC")});
}

}  // namespace Corbomite::Bases
```

Add `src/SortCycle.cpp` to the `add_library(corbomite-bases ...)` list in `libs/bases/CMakeLists.txt` (after `src/BasesViewConfig.cpp`).

- [ ] **Step 4: Run to verify pass**

Run: `cmake --build --preset dev -j 10 --target tst_bases_sortcycle && cd build-dev && ctest -R tst_bases_sortcycle --output-on-failure`
Expected: all 5 slots PASS.

- [ ] **Step 5: Commit**
```bash
git add libs/bases/include/corbomite/bases/SortCycle.h libs/bases/src/SortCycle.cpp libs/bases/CMakeLists.txt libs/bases/tests/tst_sortcycle.cpp libs/bases/tests/CMakeLists.txt
git commit -m "feat(bases): pure multi-key sort-cycle helper (cycleHeaderSort)"
```

---

### Task 2: `BasesTreeModel` — tree structure

**Files:**
- Create: `libs/bases/include/corbomite/bases/BasesTreeModel.h`, `libs/bases/src/BasesTreeModel.cpp`
- Modify: `libs/bases/CMakeLists.txt`
- Test: `libs/bases/tests/tst_bases_tree_model.cpp` (+ register)

A 2-level `QAbstractItemModel`. It snapshots `result()->groups()` + `properties()` into members on rebuild, so tree navigation is pure index arithmetic. `internalId` encoding: `GROUP_ID = quintptr(-1)` (group node, parent = root); `FLAT_ID = quintptr(-2)` (flat entry node, parent = root); any other id `g` = entry node whose parent is group `g`. **Flat** mode (no headings) when there is exactly one group and it is keyless.

- [ ] **Step 1: Write the failing structure test**

Create `libs/bases/tests/tst_bases_tree_model.cpp`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QAbstractItemModelTester>
#include "corbomite/bases/BasesTreeModel.h"
#include "corbomite/bases/BasesQueryResult.h"   // BasesEntryGroup
#include "corbomite/bases/BasesEntry.h"
#include "corbomite/bases/BasesQuery.h"
#include "corbomite/bases/Values.h"

using namespace Corbomite::Bases;

namespace {
PropertyId note(const char *n) { return PropertyId{PropertyKind::Note, QString::fromLatin1(n)}; }

// A group with a string key and `n` placeholder entries (null vault/cache/file).
BasesEntryGroup grp(const char *key, int n, const BasesQuery &q) {
    BasesEntryGroup g;
    if (key) g.key = std::make_shared<StringValue>(QString::fromLatin1(key));
    for (int i = 0; i < n; ++i)
        g.entries.push_back(std::make_shared<BasesEntry>(nullptr, nullptr, nullptr, nullptr, q));
    return g;
}
}

class TestBasesTreeModel : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void testGroupedStructure()
    {
        BasesQuery q;
        BasesTreeModel m(nullptr, nullptr);   // controller/fm null; populate directly
        QVector<BasesEntryGroup> groups{ grp("Active", 2, q), grp("Done", 3, q) };
        m.populateForTesting(groups, {note("status"), note("title")});

        QAbstractItemModelTester tester(&m);  // validates model invariants
        QCOMPARE(m.columnCount(QModelIndex()), 2);
        QCOMPARE(m.rowCount(QModelIndex()), 2);                 // two group rows
        const QModelIndex g0 = m.index(0, 0, QModelIndex());
        QVERIFY(g0.isValid());
        QVERIFY(m.isGroupRow(g0));
        QCOMPARE(m.rowCount(g0), 2);                            // group 0 has 2 entries
        const QModelIndex e = m.index(1, 0, g0);
        QVERIFY(e.isValid());
        QVERIFY(!m.isGroupRow(e));
        QCOMPARE(m.parent(e), g0);                             // entry's parent is its group
        QCOMPARE(m.rowCount(m.index(1, 0, QModelIndex())), 3); // group 1 has 3 entries
    }

    void testFlatWhenSingleKeylessGroup()
    {
        BasesQuery q;
        BasesTreeModel m(nullptr, nullptr);
        QVector<BasesEntryGroup> groups{ grp(nullptr, 4, q) };  // one keyless group
        m.populateForTesting(groups, {note("title")});

        QCOMPARE(m.rowCount(QModelIndex()), 4);                 // flat: entries at root
        const QModelIndex e = m.index(2, 0, QModelIndex());
        QVERIFY(e.isValid());
        QVERIFY(!m.isGroupRow(e));
        QVERIFY(!m.parent(e).isValid());                       // parent is root
    }
};

QTEST_MAIN(TestBasesTreeModel)
#include "tst_bases_tree_model.moc"
```

Register in `libs/bases/tests/CMakeLists.txt`:
```cmake
add_executable(tst_bases_tree_model tst_bases_tree_model.cpp)
add_test(NAME tst_bases_tree_model COMMAND tst_bases_tree_model)
target_link_libraries(tst_bases_tree_model PRIVATE Qt6::Test Qt6::Widgets Corbomite::Bases)
```
(`Qt6::Widgets` for `QAbstractItemModelTester`; add `find_package(Qt6 ... Widgets)` to the test CMakeLists `find_package` line if not present.)

- [ ] **Step 2: Run to confirm it fails (no BasesTreeModel yet)**

Run: `cmake --build --preset dev -j 10 --target tst_bases_tree_model`
Expected: FAILS to compile.

- [ ] **Step 3: Implement the header**

Create `libs/bases/include/corbomite/bases/BasesTreeModel.h`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "BasesQueryResult.h"   // BasesEntryGroup
#include "PropertyId.h"
#include "ValuePtr.h"

#include <QAbstractItemModel>
#include <QHash>
#include <QVector>

namespace Corbomite { class FileManager; }

namespace Corbomite::Bases {

class QueryController;

/// 2-level tree facade over a QueryController's BasesQueryResult: group
/// nodes -> entry leaves. Ungrouped (single keyless group) renders flat.
class BasesTreeModel : public QAbstractItemModel
{
    Q_OBJECT
public:
    static constexpr int ValueTypeRole  = Qt::UserRole + 1;  // matches BasesTableModel
    static constexpr int ValuePtrRole   = Qt::UserRole + 2;
    static constexpr int IsGroupRowRole = Qt::UserRole + 3;
    static constexpr int GroupCountRole = Qt::UserRole + 4;

    BasesTreeModel(QueryController *controller, FileManager *fileManager,
                   QObject *parent = nullptr);
    ~BasesTreeModel() override;

    QModelIndex index(int row, int column, const QModelIndex &parent = {}) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QVariant headerData(int section, Qt::Orientation o, int role = Qt::DisplayRole) const override;

    bool isGroupRow(const QModelIndex &index) const;
    ValuePtr valueAt(const QModelIndex &index) const;       // entry cells; null for group rows
    PropertyId propertyAt(int column) const;

    /// Test seam: populate the snapshot directly, bypassing the controller.
    void populateForTesting(const QVector<BasesEntryGroup> &groups,
                            const QVector<PropertyId> &columns);

private Q_SLOTS:
    void onResultsChanged();

private:
    static constexpr quintptr GROUP_ID = quintptr(-1);
    static constexpr quintptr FLAT_ID  = quintptr(-2);
    bool isFlat() const;
    BasesEntry *entryAt(const QModelIndex &index) const;

    QueryController *m_controller;
    FileManager *m_fm;
    QVector<BasesEntryGroup> m_groups;   // snapshot
    QVector<PropertyId> m_columns;       // snapshot
    QHash<PropertyId, QString> m_summaries;  // from active view config (display only)
};

}  // namespace Corbomite::Bases
```

- [ ] **Step 4: Implement the structure methods**

Create `libs/bases/src/BasesTreeModel.cpp` (structure only; `data`/`setData` fleshed out in Task 3):
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/BasesTreeModel.h"

#include "corbomite/bases/BasesEntry.h"
#include "corbomite/bases/BasesViewConfig.h"
#include "corbomite/bases/QueryController.h"
#include "corbomite/bases/Values.h"

namespace Corbomite::Bases {

BasesTreeModel::BasesTreeModel(QueryController *controller, FileManager *fileManager,
                               QObject *parent)
    : QAbstractItemModel(parent), m_controller(controller), m_fm(fileManager)
{
    if (m_controller)
        connect(m_controller, &QueryController::resultsChanged, this,
                &BasesTreeModel::onResultsChanged);
    onResultsChanged();
}

BasesTreeModel::~BasesTreeModel() = default;

bool BasesTreeModel::isFlat() const
{
    return m_groups.size() == 1 && !m_groups.front().hasKey();
}

QModelIndex BasesTreeModel::index(int row, int column, const QModelIndex &parent) const
{
    if (row < 0 || column < 0 || column >= m_columns.size()) return {};
    if (!parent.isValid())
        return createIndex(row, column, isFlat() ? FLAT_ID : GROUP_ID);
    // parent must be a group node
    if (parent.internalId() != GROUP_ID) return {};
    return createIndex(row, column, quintptr(parent.row()));
}

QModelIndex BasesTreeModel::parent(const QModelIndex &child) const
{
    if (!child.isValid()) return {};
    const quintptr id = child.internalId();
    if (id == GROUP_ID || id == FLAT_ID) return {};   // top-level
    return createIndex(int(id), 0, GROUP_ID);          // entry's parent group
}

int BasesTreeModel::rowCount(const QModelIndex &parent) const
{
    if (!parent.isValid()) return isFlat() ? int(m_groups.front().entries.size())
                                           : int(m_groups.size());
    if (parent.internalId() == GROUP_ID) {
        const int g = parent.row();
        if (g < 0 || g >= m_groups.size()) return 0;
        return int(m_groups[g].entries.size());
    }
    return 0;  // entry leaves have no children
}

int BasesTreeModel::columnCount(const QModelIndex &) const { return m_columns.size(); }

bool BasesTreeModel::isGroupRow(const QModelIndex &index) const
{
    return index.isValid() && index.internalId() == GROUP_ID;
}

PropertyId BasesTreeModel::propertyAt(int column) const
{
    if (column < 0 || column >= m_columns.size()) return {};
    return m_columns[column];
}

BasesEntry *BasesTreeModel::entryAt(const QModelIndex &index) const
{
    if (!index.isValid() || isGroupRow(index)) return nullptr;
    const QVector<std::shared_ptr<BasesEntry>> *entries = nullptr;
    if (index.internalId() == FLAT_ID)
        entries = m_groups.isEmpty() ? nullptr : &m_groups.front().entries;
    else {
        const int g = int(index.internalId());
        if (g < 0 || g >= m_groups.size()) return nullptr;
        entries = &m_groups[g].entries;
    }
    if (!entries || index.row() < 0 || index.row() >= entries->size()) return nullptr;
    return (*entries)[index.row()].get();
}

void BasesTreeModel::populateForTesting(const QVector<BasesEntryGroup> &groups,
                                        const QVector<PropertyId> &columns)
{
    beginResetModel();
    m_groups = groups;
    m_columns = columns;
    m_summaries.clear();
    endResetModel();
}

void BasesTreeModel::onResultsChanged()
{
    beginResetModel();
    m_groups.clear();
    m_columns.clear();
    m_summaries.clear();
    if (m_controller && m_controller->result()) {
        m_groups = m_controller->result()->groups();
        m_columns = m_controller->result()->properties();
        if (auto *cfg = m_controller->viewConfig()) m_summaries = cfg->summaries;
    }
    endResetModel();
}

// --- data()/setData()/flags()/headerData() land in Task 3 ---
QVariant BasesTreeModel::data(const QModelIndex &, int) const { return {}; }
bool BasesTreeModel::setData(const QModelIndex &, const QVariant &, int) { return false; }
Qt::ItemFlags BasesTreeModel::flags(const QModelIndex &index) const
{ return index.isValid() ? (Qt::ItemIsEnabled | Qt::ItemIsSelectable) : Qt::NoItemFlags; }
QVariant BasesTreeModel::headerData(int, Qt::Orientation, int) const { return {}; }
ValuePtr BasesTreeModel::valueAt(const QModelIndex &) const { return nullptr; }

}  // namespace Corbomite::Bases
```

> Verify `QueryController` exposes `viewConfig()` returning `BasesViewConfig*` (it has `setViewConfig`). If the getter is named differently or absent, add a trivial `BasesViewConfig *viewConfig() const { return m_cfg; }` accessor to `QueryController` — needed for summary config.

Add `src/BasesTreeModel.cpp` to `libs/bases/CMakeLists.txt` (after `src/BasesTableModel.cpp`).

- [ ] **Step 5: Run to verify structure tests pass**

Run: `cmake --build --preset dev -j 10 --target tst_bases_tree_model && cd build-dev && ctest -R tst_bases_tree_model --output-on-failure`
Expected: PASS (incl. `QAbstractItemModelTester` invariants).

- [ ] **Step 6: Commit**
```bash
git add libs/bases/include/corbomite/bases/BasesTreeModel.h libs/bases/src/BasesTreeModel.cpp libs/bases/CMakeLists.txt libs/bases/tests/tst_bases_tree_model.cpp libs/bases/tests/CMakeLists.txt
git commit -m "feat(bases): BasesTreeModel — 2-level grouped/flat tree structure"
```

---

### Task 3: `BasesTreeModel::data()` / `setData()` / `flags()` / `headerData()`

**Files:**
- Modify: `libs/bases/src/BasesTreeModel.cpp`
- Test: `libs/bases/tests/tst_bases_tree_model.cpp`

- [ ] **Step 1: Add failing data() tests**

Add slots to `TestBasesTreeModel` (reuse `grp`/`note` helpers):
```cpp
    void testGroupRowData()
    {
        BasesQuery q;
        BasesTreeModel m(nullptr, nullptr);
        m.populateForTesting({ grp("Active", 2, q) }, {note("status"), note("title")});
        // The single keyed group is NOT flat -> a group row exists.
        const QModelIndex g0 = m.index(0, 0, QModelIndex());
        QVERIFY(m.isGroupRow(g0));
        QCOMPARE(m.data(g0, BasesTreeModel::IsGroupRowRole).toBool(), true);
        QCOMPARE(m.data(g0, BasesTreeModel::GroupCountRole).toInt(), 2);
        QCOMPARE(m.data(g0, Qt::DisplayRole).toString(), QStringLiteral("Active"));
        // entry rows are not group rows
        const QModelIndex e = m.index(0, 0, g0);
        QCOMPARE(m.data(e, BasesTreeModel::IsGroupRowRole).toBool(), false);
    }
    void testNullKeyGroupLabel()
    {
        BasesQuery q;
        BasesTreeModel m(nullptr, nullptr);
        // two groups so it's not flat; second is keyless
        m.populateForTesting({ grp("Active", 1, q), grp(nullptr, 1, q) }, {note("status")});
        const QModelIndex g1 = m.index(1, 0, QModelIndex());
        QCOMPARE(m.data(g1, Qt::DisplayRole).toString(), QStringLiteral("(no value)"));
    }
```

- [ ] **Step 2: Run to confirm failure**

Run: `cmake --build --preset dev -j 10 --target tst_bases_tree_model && cd build-dev && ctest -R tst_bases_tree_model --output-on-failure`
Expected: new slots FAIL (`data()` returns `{}`).

- [ ] **Step 3: Implement data/setData/flags/headerData**

Replace the four stub bodies at the bottom of `BasesTreeModel.cpp` with:
```cpp
QVariant BasesTreeModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) return {};

    if (isGroupRow(index)) {
        const int g = index.row();
        if (g < 0 || g >= m_groups.size()) return {};
        const BasesEntryGroup &grp = m_groups[g];
        if (role == IsGroupRowRole) return true;
        if (role == GroupCountRole) return int(grp.entries.size());
        if (role == Qt::DisplayRole) {
            if (index.column() == 0)
                return grp.hasKey() ? grp.key->toString() : QStringLiteral("(no value)");
            // summary cell for this column iff a summary fn is configured
            const PropertyId pid = propertyAt(index.column());
            const QString fn = m_summaries.value(pid);
            if (!fn.isEmpty() && m_controller && m_controller->result()) {
                auto sv = m_controller->result()->summaryValue(g, pid, fn);
                return sv ? sv->toString() : QString{};
            }
            return {};
        }
        return {};
    }

    // entry row
    if (role == IsGroupRowRole) return false;
    const auto v = valueAt(index);
    if (!v) return {};
    if (role == ValueTypeRole) return v->type();
    if (role == ValuePtrRole)  return QVariant::fromValue(v);
    if (role == Qt::DisplayRole || role == Qt::EditRole) return v->toString();
    return {};
}

ValuePtr BasesTreeModel::valueAt(const QModelIndex &index) const
{
    auto *entry = entryAt(index);
    if (!entry) return nullptr;
    return entry->getValue(propertyAt(index.column()));
}

bool BasesTreeModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (role != Qt::EditRole || isGroupRow(index) || !m_fm) return false;
    auto *entry = entryAt(index);
    if (!entry || !entry->file()) return false;
    const PropertyId pid = propertyAt(index.column());
    if (pid.kind != PropertyKind::Note) return false;   // only frontmatter editable
    m_fm->processFrontMatter(entry->file(), [&](QVariantMap &fm) { fm.insert(pid.name, value); });
    return true;  // QueryController recompute -> resultsChanged -> reset
}

Qt::ItemFlags BasesTreeModel::flags(const QModelIndex &index) const
{
    if (!index.isValid()) return Qt::NoItemFlags;
    Qt::ItemFlags f = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    if (!isGroupRow(index) && propertyAt(index.column()).kind == PropertyKind::Note)
        f |= Qt::ItemIsEditable;
    return f;
}

QVariant BasesTreeModel::headerData(int section, Qt::Orientation o, int role) const
{
    if (role != Qt::DisplayRole) return {};
    if (o == Qt::Horizontal) {
        if (section < 0 || section >= m_columns.size()) return {};
        return m_columns[section].toString();
    }
    return {};
}
```
Add the includes the bodies need to `BasesTreeModel.cpp`: `#include "corbomite/vault/FileManager.h"`, `#include "corbomite/vault/TFile.h"`, `#include <QVariantMap>`, and `Q_DECLARE_METATYPE(Corbomite::Bases::ValuePtr)` near the top (mirror `BasesTableModel.cpp`); register the metatype in the ctor (`qRegisterMetaType<ValuePtr>("Corbomite::Bases::ValuePtr");`).

- [ ] **Step 4: Run to verify pass**

Run: `cmake --build --preset dev -j 10 --target tst_bases_tree_model && cd build-dev && ctest -R tst_bases_tree_model --output-on-failure`
Expected: all slots PASS.

- [ ] **Step 5: Commit**
```bash
git add libs/bases/src/BasesTreeModel.cpp libs/bases/tests/tst_bases_tree_model.cpp
git commit -m "feat(bases): BasesTreeModel data() — entry cells + group label/count/summary"
```

---

### Task 4: Swap `BasesView` to `QTreeView` + `BasesTreeModel`; group-heading styling

**Files:**
- Modify: `libs/bases/include/corbomite/bases/BasesView.h`, `src/BasesView.cpp`
- Modify: `libs/bases/src/BasesCellDelegate.cpp`

This is a GUI swap — verified by build + launch, not unit test (the model logic is covered by Tasks 2-3).

- [ ] **Step 1: Swap the widget + model in BasesView**

In `BasesView.h`: change the forward decl `class QTableView;` → `class QTreeView;`, the member `QTableView *m_table` → `QTreeView *m_table`, and `std::unique_ptr<BasesTableModel> m_model` → `std::unique_ptr<BasesTreeModel> m_model` (add `class BasesTreeModel;` fwd decl, drop `BasesTableModel`).

In `BasesView.cpp`: replace `#include ...BasesTableModel.h` with `BasesTreeModel.h` and `#include <QTableView>` with `#include <QTreeView>`. In the constructor, build `m_table = new QTreeView(this);`; keep `setSelectionBehavior(SelectRows)`, edit-triggers, and the `header()` connections — but `QTreeView` uses `header()` not `horizontalHeader()`:
```cpp
    m_table = new QTreeView(this);
    m_table->setHeader(new QHeaderView(Qt::Horizontal, m_table));  // or use default header()
    m_table->header()->setSectionsClickable(true);
    m_table->header()->setSectionsMovable(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setRootIsDecorated(true);     // show expand/collapse chevrons
    m_table->setExpandsOnDoubleClick(true);
    m_table->setItemsExpandable(true);
    m_table->setEditTriggers(QAbstractItemView::DoubleClicked
                           | QAbstractItemView::SelectedClicked
                           | QAbstractItemView::EditKeyPressed);
    m_delegate = new BasesCellDelegate(this);
    m_table->setItemDelegate(m_delegate);
    root->addWidget(m_table, 1);

    connect(m_table->header(), &QHeaderView::sectionClicked, this, &BasesView::onHeaderClicked);
    connect(m_table->header(), &QHeaderView::sectionMoved, this, &BasesView::onSectionMoved);
```
In `rebuildLayout()`: `m_model = std::make_unique<BasesTreeModel>(m_controller.get(), m_fm, this);` then `m_table->setModel(m_model.get()); m_table->expandAll();` (groups start expanded — in-memory collapse per spec). In `clear()`: unchanged except the model type. In `onSectionMoved`, replace `m_table->horizontalHeader()` with `m_table->header()`.

- [ ] **Step 2: Group-heading styling in the delegate**

In `BasesCellDelegate.cpp`, at the top of `paint()`, before the type dispatch, add a group-row branch (uses the new role):
```cpp
#include "corbomite/bases/BasesTreeModel.h"   // for the role constants
...
void BasesCellDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                              const QModelIndex &index) const
{
    if (index.data(BasesTreeModel::IsGroupRowRole).toBool()) {
        painter->save();
        painter->fillRect(option.rect, option.palette.alternateBase());
        QFont f = option.font; f.setBold(true); painter->setFont(f);
        QString text = index.data(Qt::DisplayRole).toString();
        if (index.column() == 0) {
            const int n = index.data(BasesTreeModel::GroupCountRole).toInt();
            text = QStringLiteral("%1  (%2)").arg(text).arg(n);
        }
        painter->drawText(option.rect.adjusted(4, 0, -4, 0),
                          Qt::AlignVCenter | Qt::AlignLeft, text);
        painter->restore();
        return;
    }
    // ... existing Error / Boolean / fallback dispatch unchanged ...
```

- [ ] **Step 3: Build + launch verify**

Run: `cmake --build --preset dev -j 10`
Then launch and open a grouped `.base` (the dev test vault, or create one with a `group_by`): `./build-dev/bin/Corbomite testvaults/starter-vault`
Expected: builds clean; a grouped base shows collapsible group headings (bold label + count) with entry rows beneath; chevrons expand/collapse; an ungrouped base shows a flat list with no headings. (Manual visual check — no crash, headings present.)

- [ ] **Step 4: Run existing bases suite (no regressions)**

Run: `cd build-dev && ctest -R 'tst_bases' --output-on-failure`
Expected: all green (tree-model + sortcycle + existing value/dsl tests).

- [ ] **Step 5: Commit**
```bash
git add libs/bases/include/corbomite/bases/BasesView.h libs/bases/src/BasesView.cpp libs/bases/src/BasesCellDelegate.cpp
git commit -m "feat(bases): BasesView on QTreeView + BasesTreeModel; group-heading rendering"
```

---

### Task 5: Wire multi-key header sort

**Files:**
- Modify: `libs/bases/src/BasesView.cpp`

The cycle logic is already tested (Task 1). This task wires it in, adding Shift detection. `requestSave()` is kept (persists, matching reorder/view-switch).

- [ ] **Step 1: Replace `onHeaderClicked` body**

In `BasesView.cpp`, add `#include "corbomite/bases/SortCycle.h"` and `#include <QGuiApplication>`, and replace the body:
```cpp
void BasesView::onHeaderClicked(int column)
{
    if (!m_activeView || !m_model) return;
    const PropertyId pid = m_model->propertyAt(column);
    if (pid.name.isEmpty()) return;
    const bool shift = QGuiApplication::keyboardModifiers().testFlag(Qt::ShiftModifier);
    cycleHeaderSort(m_activeView->sort, pid, shift);
    if (m_controller) m_controller->recomputeNow();
    if (m_table) m_table->expandAll();   // keep groups visible after re-sort
    requestSave();                       // persist, as reorder/view-switch already do
}
```

- [ ] **Step 2: Build + verify behavior**

Run: `cmake --build --preset dev -j 10`
Launch, open a base, click a header (cycles ASC→DESC→unsorted), Shift-click another (adds a secondary key — verify ordering changes accordingly). Confirm the `.base` file on disk updates its `sort:` block after a click (persistence).
Expected: multi-key sort works; `.base` persists.

- [ ] **Step 3: Commit**
```bash
git add libs/bases/src/BasesView.cpp
git commit -m "feat(bases): multi-key header sort via cycleHeaderSort (Shift = add key)"
```

---

### Task 6: Sort header indicators (direction arrow + priority index)

**Files:**
- Create: `libs/bases/include/corbomite/bases/BasesHeaderView.h`, `src/BasesHeaderView.cpp`
- Modify: `libs/bases/CMakeLists.txt`, `libs/bases/src/BasesView.cpp`

`QHeaderView` shows only one native sort indicator; multi-key needs custom painting. A `BasesHeaderView : QHeaderView` overrides `paintSection` to draw, per sorted column, an ASC/DESC arrow + a small priority number.

- [ ] **Step 1: Implement the header view**

Create `libs/bases/include/corbomite/bases/BasesHeaderView.h`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "BasesViewConfig.h"
#include "PropertyId.h"
#include <QHeaderView>
#include <QVector>
#include <functional>

namespace Corbomite::Bases {

/// Header that paints multi-key sort indicators. The owner supplies the
/// current sort keys + a column->PropertyId map via setSortProvider.
class BasesHeaderView : public QHeaderView
{
    Q_OBJECT
public:
    explicit BasesHeaderView(QWidget *parent = nullptr);
    using SortProvider = std::function<QVector<SortKey>()>;
    using PropertyForColumn = std::function<PropertyId(int)>;
    void setProviders(SortProvider sort, PropertyForColumn prop);
protected:
    void paintSection(QPainter *painter, const QRect &rect, int logicalIndex) const override;
private:
    SortProvider m_sort;
    PropertyForColumn m_prop;
};

}  // namespace Corbomite::Bases
```

Create `libs/bases/src/BasesHeaderView.cpp`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/BasesHeaderView.h"
#include <QPainter>

namespace Corbomite::Bases {

BasesHeaderView::BasesHeaderView(QWidget *parent)
    : QHeaderView(Qt::Horizontal, parent) { setSectionsClickable(true); setSectionsMovable(true); }

void BasesHeaderView::setProviders(SortProvider sort, PropertyForColumn prop)
{ m_sort = std::move(sort); m_prop = std::move(prop); update(); }

void BasesHeaderView::paintSection(QPainter *painter, const QRect &rect, int logicalIndex) const
{
    QHeaderView::paintSection(painter, rect, logicalIndex);
    if (!m_sort || !m_prop) return;
    const PropertyId pid = m_prop(logicalIndex);
    const QVector<SortKey> keys = m_sort();
    for (int i = 0; i < keys.size(); ++i) {
        if (!(keys[i].property == pid)) continue;
        const bool asc = keys[i].direction == QLatin1String("ASC");
        const QString glyph = (asc ? QStringLiteral("▲") : QStringLiteral("▼"))
                            + (keys.size() > 1 ? QString::number(i + 1) : QString{});
        painter->save();
        painter->drawText(rect.adjusted(0, 0, -2, 0), Qt::AlignRight | Qt::AlignVCenter, glyph);
        painter->restore();
        break;
    }
}

}  // namespace Corbomite::Bases
```
Add `src/BasesHeaderView.cpp` to `libs/bases/CMakeLists.txt`.

- [ ] **Step 2: Install the header in BasesView**

In `BasesView.cpp` ctor, after creating `m_table`, install the custom header and wire providers (replace the plain `header()` setup):
```cpp
    auto *hdr = new BasesHeaderView(m_table);
    m_table->setHeader(hdr);
    hdr->setProviders(
        [this]() { return m_activeView ? m_activeView->sort : QVector<SortKey>{}; },
        [this](int c) { return m_model ? m_model->propertyAt(c) : PropertyId{}; });
    connect(hdr, &QHeaderView::sectionClicked, this, &BasesView::onHeaderClicked);
    connect(hdr, &QHeaderView::sectionMoved, this, &BasesView::onSectionMoved);
```
Add `#include "corbomite/bases/BasesHeaderView.h"`. After a sort change in `onHeaderClicked`, call `m_table->header()->update();` so indicators repaint.

- [ ] **Step 3: Build + visual verify**

Run: `cmake --build --preset dev -j 10`
Launch; click/Shift-click headers; confirm arrows + priority numbers (1, 2, …) appear on the sorted columns.
Expected: indicators render and update on each click.

- [ ] **Step 4: Commit**
```bash
git add libs/bases/include/corbomite/bases/BasesHeaderView.h libs/bases/src/BasesHeaderView.cpp libs/bases/CMakeLists.txt libs/bases/src/BasesView.cpp
git commit -m "feat(bases): multi-key sort indicators (arrow + priority) via BasesHeaderView"
```

---

### Task 7: Rich read-only cells — Icon, Image, HTML

**Files:**
- Modify: `libs/bases/src/BasesCellDelegate.cpp`
- Test: `libs/bases/tests/tst_bases_cell_delegate.cpp` (+ register) — smoke test

- [ ] **Step 1: Smoke test (paint each rich type without crashing)**

Create `libs/bases/tests/tst_bases_cell_delegate.cpp`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QPixmap>
#include <QPainter>
#include <QStyleOptionViewItem>
#include <QStandardItemModel>
#include "corbomite/bases/BasesCellDelegate.h"
#include "corbomite/bases/BasesTreeModel.h"

using namespace Corbomite::Bases;

class TestBasesCellDelegate : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void testPaintsRichTypesWithoutCrash()
    {
        BasesCellDelegate d;
        QPixmap pm(80, 20);
        for (const QString &type : {QStringLiteral("Icon"), QStringLiteral("Image"),
                                    QStringLiteral("HTML"), QStringLiteral("Markdown")}) {
            QStandardItemModel m(1, 1);
            auto *it = new QStandardItem(QStringLiteral("x"));
            it->setData(type, BasesTreeModel::ValueTypeRole);
            m.setItem(0, 0, it);
            pm.fill(Qt::white);
            QPainter p(&pm);
            QStyleOptionViewItem opt; opt.rect = QRect(0, 0, 80, 20);
            d.paint(&p, opt, m.index(0, 0));   // must not crash / assert
            p.end();
        }
        QVERIFY(true);
    }
};

QTEST_MAIN(TestBasesCellDelegate)
#include "tst_bases_cell_delegate.moc"
```
Register in `tests/CMakeLists.txt`:
```cmake
add_executable(tst_bases_cell_delegate tst_bases_cell_delegate.cpp)
add_test(NAME tst_bases_cell_delegate COMMAND tst_bases_cell_delegate)
target_link_libraries(tst_bases_cell_delegate PRIVATE Qt6::Test Qt6::Widgets Corbomite::Bases)
```

- [ ] **Step 2: Run to confirm it builds + passes against current delegate (baseline)**

Run: `cmake --build --preset dev -j 10 --target tst_bases_cell_delegate && cd build-dev && ctest -R tst_bases_cell_delegate --output-on-failure`
Expected: PASS (current delegate falls back to plain text — no crash). This pins "no crash"; the rich rendering is added next and must keep it green.

- [ ] **Step 3: Add Icon/Image/HTML paint branches**

In `BasesCellDelegate.cpp` `paint()`, after the group-row branch (Task 4) and before the `Error`/`Boolean` branches, add (pull `ValuePtrRole` for the value string via the existing `valueVar` pattern):
```cpp
    if (type == QLatin1String("Icon")) {
        const QString name = index.data(Qt::DisplayRole).toString();   // IconValue::data() == name
        const QIcon ic = Corbomite::LucideIconRegistry::instance().get(name);
        painter->save();
        if (!ic.isNull()) ic.paint(painter, option.rect, Qt::AlignCenter);
        else painter->drawText(option.rect, Qt::AlignCenter, name);     // fallback
        painter->restore();
        return;
    }
    if (type == QLatin1String("Image")) {
        const QString ref = index.data(Qt::DisplayRole).toString();
        QPixmap pm(ref);                       // path resolution refined below
        painter->save();
        if (!pm.isNull())
            painter->drawPixmap(option.rect,
                pm.scaled(option.rect.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        else painter->drawText(option.rect, Qt::AlignVCenter | Qt::AlignLeft, ref);
        painter->restore();
        return;
    }
    if (type == QLatin1String("HTML")) {
        const QString html = index.data(Qt::DisplayRole).toString();
        QTextDocument doc; doc.setHtml(html); doc.setTextWidth(option.rect.width());
        painter->save();
        painter->translate(option.rect.topLeft());
        doc.drawContents(painter, QRectF(0, 0, option.rect.width(), option.rect.height()));
        painter->restore();
        return;
    }
    // "Markdown" intentionally falls through to the plain-text fallback (deferred).
```
Add includes: `#include "corbomite/core/LucideIconRegistry.h"`, `#include <QIcon>`, `#include <QPixmap>`, `#include <QTextDocument>`. Ensure the delegate links `Corbomite::Core` (the lib already does transitively via Bases; if the link fails, add `Corbomite::Core` to `corbomite-bases`'s `target_link_libraries`).

> **Image path resolution:** `QPixmap(ref)` only works for absolute/relative-to-cwd paths. The Obsidian semantics resolve `ref` against the vault (and support external URLs). Resolve the exact behavior against the source (`ImageValue` `renderTo` in the canonical chunk under `/home/clinton/bin/ObsidianRAW/audit/renamed/obsidian/tree/obsidian/bases/`); minimally, resolve a vault-relative path via the controller's `Vault` to an absolute path before constructing the `QPixmap`. If the renderer needs vault access the delegate lacks, pass the resolved absolute path through a model role instead. Keep the text fallback for unresolved refs.

- [ ] **Step 4: Run smoke test + build**

Run: `cmake --build --preset dev -j 10 && cd build-dev && ctest -R tst_bases_cell_delegate --output-on-failure`
Expected: PASS (no crash with the new branches). Launch and eyeball a base with icon/image/html columns if available.

- [ ] **Step 5: Commit**
```bash
git add libs/bases/src/BasesCellDelegate.cpp libs/bases/tests/tst_bases_cell_delegate.cpp libs/bases/tests/CMakeLists.txt
git commit -m "feat(bases): rich read-only cells — Icon/Image/HTML (Markdown deferred)"
```

---

### Task 8: Full suite + close

- [ ] **Step 1: Full bases suite**

Run: `cd build-dev && ctest -R 'tst_bases' --output-on-failure`
Expected: all green (sortcycle, tree_model, cell_delegate, + existing value/dsl/entry tests).

- [ ] **Step 2: Full tree build (no regressions elsewhere)**

Run: `cmake --build --preset dev -j 10`
Expected: clean. (The 10 pre-existing foundation-port failures outside `libs/bases` are unrelated — confirm no *new* failures in bases.)

- [ ] **Step 3: Commit any final wiring**
```bash
git add -A libs/bases
git commit -m "test(bases): D.2 read-side rendering suite green; close D.2" || echo "nothing to commit"
```

---

## Definition of done

- Grouped `.base` → collapsible headings (bold label + `(N)` count) + summary cells where configured; ungrouped → flat, no headings; null-key group last labelled `(no value)`.
- Icon/Image/HTML cells render richly; Markdown falls back to text.
- Header click builds/cycles a multi-key sort (Shift = add key) with arrow + priority indicators; persists via `requestSave()`.
- `tst_bases_sortcycle`, `tst_bases_tree_model`, `tst_bases_cell_delegate` pass; full `libs/bases` suite green.
- No inline-edit/toolbar work (D.3); Markdown cells deferred (frozen).

## Notes / risks

- **`QueryController::viewConfig()` accessor** (Task 2 Step 4) — add if absent; needed for summary config.
- **Image path resolution** (Task 7) — resolve against source + vault; text fallback meanwhile.
- **Collapse persistence** is intentionally out (in-memory; `expandAll()` on rebuild). Persisting collapsed keys to leaf ephemeral state is a follow-up.
- `BasesTableModel` is left intact but unused by `BasesView`; deleting it is a later cleanup once nothing references its role constants.
