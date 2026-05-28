# Cluster D — Filter Builder Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a UI to build/edit Bases filters (nested And/Or/Not trees of raw DSL predicates) for both the global and per-view scopes, over the already-complete FilterTree backend, reusing the `FormulaInput` widget for leaf predicates.

**Architecture:** A pure mutable `FilterSpec` value tree + pure `fromFilter`/`toFilter` converters bridge the immutable `FilterNode` backend tree. A recursive `FilterBuilderWidget` renders/edits one `FilterSpec` group (leaves are `FormulaInput`s, nested groups are child `FilterBuilderWidget`s). A `FilterBuilderDialog` holds two builders (per-view + global) behind a scope toggle. `BasesView` opens the dialog from a new toolbar button and applies both specs through its existing `onConfigMutated()` (recompute + `requestSave`) chokepoint.

**Tech Stack:** C++20, Qt6 Widgets, KDE Frameworks (`KLocalizedString`), QtTest. Library: `libs/bases` (`Corbomite::Bases`).

**Spec:** [`docs/superpowers/specs/2026-05-27-cluster-d-filter-builder-design.md`](../specs/2026-05-27-cluster-d-filter-builder-design.md)

**Build/test commands (from repo root):**
- Configure (once): `cmake --preset dev`
- Build: `cmake --build --preset dev -j 10` (on success may print nothing; check exit code: append `; echo "exit=$?"`, and redirect to a log on failure: `> /tmp/b.log 2>&1` then `tail -50 /tmp/b.log`)
- One test: `cd build-dev && ctest -R '<name>' --output-on-failure`
- All bases+formula+filter (regression): `cd build-dev && ctest -R 'bases|formula|filter' --output-on-failure -j 10`

**Key existing facts (verified):**
- `libs/bases/include/corbomite/bases/FilterTree.h`: `class FilterNode` (virtual `bool test(...)`, `QVariant serialize()`); `class FilterRule : FilterNode` (`explicit FilterRule(Formula)`, `const Formula &rule()`, `serialize()` → the formula source `QString`); `enum class Conj { And, Or, Not }`; `class FilterConjunction : FilterNode` (`FilterConjunction(Conj, QVector<FilterPtr>)`, `Conj conj()`, `const QVector<FilterPtr> &children()`, `FilterPtr optimize()`); `using FilterPtr = std::shared_ptr<FilterNode>`.
- `FilterConjunction::optimize()` returns `m_children[0]` when `size()==1 && conj != Not`, else a fresh conjunction. (Use it in `toFilter`.)
- `Formula` (`Formula.h`): `explicit Formula(QString)`, `QString source()`, `bool isValid()`.
- `FilterRule::serialize()` returns a `QString`; `FilterConjunction::serialize()` returns a `QVariantMap` with one key `and`/`or`/`not` → `QVariantList` of child serializations.
- `FormulaInput` (`FormulaInput.h`): `QLineEdit` subclass; `bool isExpressionValid() const`; `void setCandidates(const QStringList&)`; signal `validityChanged(bool)`; text via `text()`/`setText()`.
- `BasesView` (from the formula-editor work): has `formulaCandidateList() const` (returns `FormulaCandidates::build(availableProperties(), funcs, NamedFormula)`), `onConfigMutated()` (recompute + `requestSave`), members `std::shared_ptr<BasesQuery> m_query`, `BasesViewConfig *m_activeView`, toolbar `QToolButton`s (`m_propsBtn`/`m_sortBtn`/`m_viewsBtn`). `BasesQuery::filters` and `BasesViewConfig::filters` are both `FilterPtr`.
- Widget tests in `libs/bases/tests/CMakeLists.txt` set `set_tests_properties(<name> PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")` (see `tst_formula_input`).

---

## File Structure

**New:**
- `libs/bases/include/corbomite/bases/FilterSpec.h` + `src/FilterSpec.cpp` — mutable `FilterSpec` value tree + pure `fromFilter`/`toFilter` converters.
- `libs/bases/include/corbomite/bases/FilterBuilderWidget.h` + `src/FilterBuilderWidget.cpp` — recursive one-group editor.
- `libs/bases/include/corbomite/bases/FilterBuilderDialog.h` + `src/FilterBuilderDialog.cpp` — scope toggle + two builders + OK gating.
- Tests: `tst_filter_spec.cpp`, `tst_filter_builder_widget.cpp`, `tst_filter_builder_dialog.cpp`.

**Modified:**
- `libs/bases/include/corbomite/bases/BasesView.h` + `src/BasesView.cpp` — Filters toolbar button + `openFiltersDialog()` + public `applyFilterSpecs(...)`.
- `libs/bases/CMakeLists.txt` (3 new sources), `libs/bases/tests/CMakeLists.txt` (3 new tests).

---

## Task 1: FilterSpec + pure converters

**Files:**
- Create: `libs/bases/include/corbomite/bases/FilterSpec.h`
- Create: `libs/bases/src/FilterSpec.cpp`
- Modify: `libs/bases/CMakeLists.txt`
- Test: `libs/bases/tests/tst_filter_spec.cpp`
- Modify: `libs/bases/tests/CMakeLists.txt`

- [ ] **Step 1: Create the header** `libs/bases/include/corbomite/bases/FilterSpec.h`

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "FilterTree.h"   // FilterPtr, Conj

#include <QString>
#include <QVector>

namespace Corbomite::Bases {

/// Mutable, copyable mirror of a filter tree used by the filter-builder UI.
/// A node is either a Leaf (a raw predicate string) or a Group (And/Or/Not over
/// ordered children). Converted to/from the immutable FilterNode tree by the
/// pure functions below.
struct FilterSpec
{
    enum class Kind { Leaf, Group };
    Kind kind = Kind::Group;
    QString expression;            ///< Leaf only: the predicate source.
    Conj conj = Conj::And;         ///< Group only.
    QVector<FilterSpec> children;  ///< Group only, ordered.

    static FilterSpec leaf(const QString &expr)
    {
        FilterSpec s; s.kind = Kind::Leaf; s.expression = expr; return s;
    }
    static FilterSpec group(Conj c, QVector<FilterSpec> kids = {})
    {
        FilterSpec s; s.kind = Kind::Group; s.conj = c; s.children = std::move(kids); return s;
    }

    bool operator==(const FilterSpec &o) const
    {
        return kind == o.kind && expression == o.expression
            && conj == o.conj && children == o.children;
    }
};

/// Backend tree -> editable spec. The result is always a Group (the dialog
/// always shows a top-level group): nullptr -> empty And-group; a bare
/// FilterRule -> And-group wrapping one leaf; a FilterConjunction -> a group
/// with converted children.
FilterSpec fromFilter(const FilterPtr &node);

/// Editable spec -> backend tree. Leaf with blank (whitespace-only) text -> null
/// (dropped). Group: convert children, drop nulls; empty -> null; otherwise build
/// a FilterConjunction and return optimize() (single-child And/Or collapses to the
/// bare child; Not and multi-child preserved).
FilterPtr toFilter(const FilterSpec &spec);

}  // namespace Corbomite::Bases
```

- [ ] **Step 2: Write the failing test** `libs/bases/tests/tst_filter_spec.cpp`

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/FilterSpec.h"
#include "corbomite/bases/FilterTree.h"

#include <QtTest>

using namespace Corbomite::Bases;

class TestFilterSpec : public QObject
{
    Q_OBJECT
private slots:
    void fromNull_isEmptyAndGroup()
    {
        FilterSpec s = fromFilter(nullptr);
        QCOMPARE(s.kind, FilterSpec::Kind::Group);
        QCOMPARE(s.conj, Conj::And);
        QVERIFY(s.children.isEmpty());
    }
    void emptyGroup_toNull()
    {
        QVERIFY(toFilter(FilterSpec::group(Conj::And)) == nullptr);
    }
    void bareRule_roundTripsAsString()
    {
        // fromFilter wraps a bare rule in an And-group with one leaf...
        auto rule = std::make_shared<FilterRule>(Formula(QStringLiteral("status == \"done\"")));
        FilterSpec s = fromFilter(rule);
        QCOMPARE(s.kind, FilterSpec::Kind::Group);
        QCOMPARE(s.children.size(), 1);
        QCOMPARE(s.children[0].kind, FilterSpec::Kind::Leaf);
        QCOMPARE(s.children[0].expression, QStringLiteral("status == \"done\""));
        // ...and toFilter collapses the single-child And back to a bare rule.
        FilterPtr back = toFilter(s);
        QVERIFY(back != nullptr);
        QCOMPARE(back->serialize().toString(), QStringLiteral("status == \"done\""));
    }
    void blankLeaf_isDropped()
    {
        FilterSpec g = FilterSpec::group(Conj::And, {
            FilterSpec::leaf(QStringLiteral("a > 1")),
            FilterSpec::leaf(QStringLiteral("   ")),   // blank -> dropped
        });
        FilterPtr back = toFilter(g);   // one surviving child -> collapses to bare rule
        QVERIFY(back != nullptr);
        QCOMPARE(back->serialize().toString(), QStringLiteral("a > 1"));
    }
    void orGroup_roundTrips()
    {
        FilterSpec g = FilterSpec::group(Conj::Or, {
            FilterSpec::leaf(QStringLiteral("a == 1")),
            FilterSpec::leaf(QStringLiteral("b == 2")),
        });
        FilterPtr f = toFilter(g);
        const QVariantMap m = f->serialize().toMap();
        QVERIFY(m.contains(QStringLiteral("or")));
        QCOMPARE(m.value(QStringLiteral("or")).toList().size(), 2);
        // and fromFilter of that yields the same spec
        QCOMPARE(fromFilter(f), g);
    }
    void notWithOneChild_notCollapsed()
    {
        FilterSpec g = FilterSpec::group(Conj::Not, {
            FilterSpec::leaf(QStringLiteral("archived == true")),
        });
        FilterPtr f = toFilter(g);
        const QVariantMap m = f->serialize().toMap();
        QVERIFY(m.contains(QStringLiteral("not")));
        QCOMPARE(m.value(QStringLiteral("not")).toList().size(), 1);
    }
    void nestedGroup_roundTrips()
    {
        FilterSpec g = FilterSpec::group(Conj::And, {
            FilterSpec::leaf(QStringLiteral("a == 1")),
            FilterSpec::group(Conj::Or, {
                FilterSpec::leaf(QStringLiteral("b == 2")),
                FilterSpec::leaf(QStringLiteral("c == 3")),
            }),
        });
        QCOMPARE(fromFilter(toFilter(g)), g);
    }
};

QTEST_APPLESS_MAIN(TestFilterSpec)
#include "tst_filter_spec.moc"
```

- [ ] **Step 3: Register the test** — append to `libs/bases/tests/CMakeLists.txt`

```cmake
add_executable(tst_filter_spec tst_filter_spec.cpp)
add_test(NAME tst_filter_spec COMMAND tst_filter_spec)
target_link_libraries(tst_filter_spec PRIVATE Qt6::Test Corbomite::Bases)
```

- [ ] **Step 4: Run to verify it fails** — `cmake --build --preset dev -j 10` → link error (undefined `fromFilter`/`toFilter`).

- [ ] **Step 5: Implement** `libs/bases/src/FilterSpec.cpp`

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/FilterSpec.h"

namespace Corbomite::Bases {

namespace {

// Recursive backend->spec, preserving leaf-vs-group kind (no top-level wrapping).
FilterSpec specOf(const FilterPtr &node)
{
    if (auto rule = std::dynamic_pointer_cast<FilterRule>(node))
        return FilterSpec::leaf(rule->rule().source());
    if (auto conj = std::dynamic_pointer_cast<FilterConjunction>(node)) {
        QVector<FilterSpec> kids;
        for (const auto &c : conj->children()) kids.push_back(specOf(c));
        return FilterSpec::group(conj->conj(), std::move(kids));
    }
    return FilterSpec::group(Conj::And);  // null/unknown -> empty group
}

}  // namespace

FilterSpec fromFilter(const FilterPtr &node)
{
    if (!node) return FilterSpec::group(Conj::And);
    FilterSpec s = specOf(node);
    if (s.kind == FilterSpec::Kind::Leaf)         // bare top-level rule
        return FilterSpec::group(Conj::And, { s });
    return s;
}

FilterPtr toFilter(const FilterSpec &spec)
{
    if (spec.kind == FilterSpec::Kind::Leaf) {
        if (spec.expression.trimmed().isEmpty()) return nullptr;  // drop blanks
        return std::make_shared<FilterRule>(Formula(spec.expression));
    }
    QVector<FilterPtr> kids;
    for (const auto &c : spec.children) {
        if (FilterPtr p = toFilter(c)) kids.push_back(p);
    }
    if (kids.isEmpty()) return nullptr;
    return FilterConjunction(spec.conj, kids).optimize();
}

}  // namespace Corbomite::Bases
```

- [ ] **Step 6: Add the source** to the `add_library(corbomite-bases STATIC ...)` list in `libs/bases/CMakeLists.txt`: `src/FilterSpec.cpp`.

- [ ] **Step 7: Run to verify pass** — `cmake --build --preset dev -j 10 && cd build-dev && ctest -R tst_filter_spec --output-on-failure`. Expected: PASS.

- [ ] **Step 8: Commit**

```bash
git add libs/bases/include/corbomite/bases/FilterSpec.h libs/bases/src/FilterSpec.cpp \
        libs/bases/CMakeLists.txt libs/bases/tests/tst_filter_spec.cpp libs/bases/tests/CMakeLists.txt
git commit -m "feat(bases): FilterSpec value tree + fromFilter/toFilter converters"
```

---

## Task 2: FilterBuilderWidget

A recursive editor over one `FilterSpec` group: a conj combobox, ordered child rows (leaf = `FormulaInput` + delete; group = nested `FilterBuilderWidget` + delete), and add-rule/add-group buttons.

**Files:**
- Create: `libs/bases/include/corbomite/bases/FilterBuilderWidget.h`
- Create: `libs/bases/src/FilterBuilderWidget.cpp`
- Modify: `libs/bases/CMakeLists.txt`
- Test: `libs/bases/tests/tst_filter_builder_widget.cpp`
- Modify: `libs/bases/tests/CMakeLists.txt`

- [ ] **Step 1: Create the header** `libs/bases/include/corbomite/bases/FilterBuilderWidget.h`

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "FilterSpec.h"

#include <QStringList>
#include <QVector>
#include <QWidget>

class QComboBox;
class QVBoxLayout;

namespace Corbomite::Bases {

class FormulaInput;

/// Recursive editor for ONE filter group. Leaves are FormulaInputs; nested
/// groups are child FilterBuilderWidgets. Emits changed() on any edit and
/// validityChanged() when the aggregate validity flips. spec() reconstructs a
/// FilterSpec from the live widget state on demand.
class FilterBuilderWidget : public QWidget
{
    Q_OBJECT
public:
    explicit FilterBuilderWidget(QWidget *parent = nullptr);

    /// Rebuild the widget from `group` (must be Kind::Group). Candidates are
    /// forwarded to every leaf FormulaInput (and nested group).
    void setSpec(const FilterSpec &group, const QStringList &candidates);

    /// Reconstruct the current group spec from the widgets.
    FilterSpec spec() const;

    /// True iff every descendant leaf is parse-valid (empty leaves count as
    /// valid — they are dropped by toFilter).
    bool isValid() const;

Q_SIGNALS:
    void changed();
    void validityChanged(bool valid);

private:
    struct Row { QWidget *container = nullptr;
                 FormulaInput *leaf = nullptr;
                 FilterBuilderWidget *group = nullptr; };

    void addLeafRow(const QString &expr);
    void addGroupRow(const FilterSpec &groupSpec);
    void removeRow(QWidget *container);
    void clearRows();
    void onAnyChange();          // emits changed() + recomputes validity

    QComboBox *m_conj = nullptr;
    QVBoxLayout *m_rowsLayout = nullptr;
    QVector<Row> m_rows;
    QStringList m_candidates;
    bool m_lastValid = true;
};

}  // namespace Corbomite::Bases
```

- [ ] **Step 2: Write the failing test** `libs/bases/tests/tst_filter_builder_widget.cpp`

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/FilterBuilderWidget.h"
#include "corbomite/bases/FilterSpec.h"

#include <QPushButton>
#include <QtTest>

using namespace Corbomite::Bases;

class TestFilterBuilderWidget : public QObject
{
    Q_OBJECT
private slots:
    void setSpec_thenSpec_roundTrips()
    {
        FilterBuilderWidget w;
        FilterSpec g = FilterSpec::group(Conj::Or, {
            FilterSpec::leaf(QStringLiteral("a == 1")),
            FilterSpec::leaf(QStringLiteral("b == 2")),
        });
        w.setSpec(g, {});
        QCOMPARE(w.spec(), g);
    }
    void addRuleButton_growsSpec()
    {
        FilterBuilderWidget w;
        w.setSpec(FilterSpec::group(Conj::And, { FilterSpec::leaf(QStringLiteral("a == 1")) }), {});
        auto *add = w.findChild<QPushButton *>(QStringLiteral("addRuleButton"));
        QVERIFY(add);
        add->click();
        QCOMPARE(w.spec().children.size(), 2);
        QCOMPARE(w.spec().children[1].kind, FilterSpec::Kind::Leaf);
        QCOMPARE(w.spec().children[1].expression, QString());
    }
    void invalidLeaf_makesGroupInvalid()
    {
        FilterBuilderWidget w;
        w.setSpec(FilterSpec::group(Conj::And, {
            FilterSpec::leaf(QStringLiteral("((1")),   // unbalanced paren -> invalid
        }), {});
        QVERIFY(!w.isValid());
    }
    void emptyLeaf_isValid()
    {
        FilterBuilderWidget w;
        w.setSpec(FilterSpec::group(Conj::And, { FilterSpec::leaf(QString()) }), {});
        QVERIFY(w.isValid());
    }
    void nestedGroup_roundTrips()
    {
        FilterBuilderWidget w;
        FilterSpec g = FilterSpec::group(Conj::And, {
            FilterSpec::leaf(QStringLiteral("a == 1")),
            FilterSpec::group(Conj::Or, { FilterSpec::leaf(QStringLiteral("b == 2")) }),
        });
        w.setSpec(g, {});
        QCOMPARE(w.spec(), g);
    }
};

QTEST_MAIN(TestFilterBuilderWidget)
#include "tst_filter_builder_widget.moc"
```

- [ ] **Step 3: Register the test** — append to `libs/bases/tests/CMakeLists.txt`

```cmake
add_executable(tst_filter_builder_widget tst_filter_builder_widget.cpp)
add_test(NAME tst_filter_builder_widget COMMAND tst_filter_builder_widget)
target_link_libraries(tst_filter_builder_widget PRIVATE Qt6::Test Qt6::Widgets Corbomite::Bases)
set_tests_properties(tst_filter_builder_widget PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
```

- [ ] **Step 4: Run to verify it fails** — build → undefined `FilterBuilderWidget`.

- [ ] **Step 5: Implement** `libs/bases/src/FilterBuilderWidget.cpp`

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/FilterBuilderWidget.h"

#include "corbomite/bases/FormulaInput.h"

#include <KLocalizedString>

#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>

namespace Corbomite::Bases {

FilterBuilderWidget::FilterBuilderWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);

    auto *header = new QHBoxLayout;
    m_conj = new QComboBox(this);
    m_conj->addItem(i18n("All"),  int(Conj::And));   // index 0
    m_conj->addItem(i18n("Any"),  int(Conj::Or));    // index 1
    m_conj->addItem(i18n("None"), int(Conj::Not));   // index 2
    header->addWidget(m_conj);
    header->addStretch(1);

    auto *addRule = new QPushButton(i18n("+ rule"), this);
    addRule->setObjectName(QStringLiteral("addRuleButton"));
    auto *addGroup = new QPushButton(i18n("+ group"), this);
    addGroup->setObjectName(QStringLiteral("addGroupButton"));
    header->addWidget(addRule);
    header->addWidget(addGroup);
    root->addLayout(header);

    m_rowsLayout = new QVBoxLayout;
    m_rowsLayout->setContentsMargins(16, 0, 0, 0);   // indent children
    root->addLayout(m_rowsLayout);

    connect(m_conj, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int) { onAnyChange(); });
    connect(addRule,  &QPushButton::clicked, this,
            [this]() { addLeafRow(QString()); onAnyChange(); });
    connect(addGroup, &QPushButton::clicked, this,
            [this]() { addGroupRow(FilterSpec::group(Conj::And)); onAnyChange(); });
}

void FilterBuilderWidget::clearRows()
{
    for (const Row &r : m_rows) r.container->deleteLater();
    m_rows.clear();
}

void FilterBuilderWidget::setSpec(const FilterSpec &group, const QStringList &candidates)
{
    m_candidates = candidates;
    const int idx = m_conj->findData(int(group.conj));
    m_conj->setCurrentIndex(idx >= 0 ? idx : 0);
    clearRows();
    for (const FilterSpec &child : group.children) {
        if (child.kind == FilterSpec::Kind::Leaf) addLeafRow(child.expression);
        else addGroupRow(child);
    }
    // Set initial validity baseline without emitting.
    m_lastValid = isValid();
}

void FilterBuilderWidget::addLeafRow(const QString &expr)
{
    auto *container = new QWidget(this);
    auto *h = new QHBoxLayout(container);
    h->setContentsMargins(0, 0, 0, 0);

    auto *leaf = new FormulaInput(container);
    leaf->setCandidates(m_candidates);
    leaf->setText(expr);
    h->addWidget(leaf, 1);

    auto *del = new QToolButton(container);
    del->setIcon(QIcon::fromTheme(QStringLiteral("edit-delete")));
    del->setToolTip(i18nc("@action:button", "Remove condition"));
    h->addWidget(del);

    m_rowsLayout->addWidget(container);
    m_rows.push_back({ container, leaf, nullptr });

    connect(leaf, &FormulaInput::textChanged, this, [this]() { onAnyChange(); });
    connect(leaf, &FormulaInput::validityChanged, this, [this](bool) { onAnyChange(); });
    connect(del, &QToolButton::clicked, this,
            [this, container]() { removeRow(container); onAnyChange(); });
}

void FilterBuilderWidget::addGroupRow(const FilterSpec &groupSpec)
{
    auto *container = new QWidget(this);
    auto *h = new QHBoxLayout(container);
    h->setContentsMargins(0, 0, 0, 0);

    auto *nested = new FilterBuilderWidget(container);
    nested->setSpec(groupSpec, m_candidates);
    h->addWidget(nested, 1);

    auto *del = new QToolButton(container);
    del->setIcon(QIcon::fromTheme(QStringLiteral("edit-delete")));
    del->setToolTip(i18nc("@action:button", "Remove group"));
    h->addWidget(del, 0, Qt::AlignTop);

    m_rowsLayout->addWidget(container);
    m_rows.push_back({ container, nullptr, nested });

    connect(nested, &FilterBuilderWidget::changed, this, [this]() { onAnyChange(); });
    connect(nested, &FilterBuilderWidget::validityChanged, this, [this](bool) { onAnyChange(); });
    connect(del, &QToolButton::clicked, this,
            [this, container]() { removeRow(container); onAnyChange(); });
}

void FilterBuilderWidget::removeRow(QWidget *container)
{
    for (int i = 0; i < m_rows.size(); ++i) {
        if (m_rows[i].container == container) {
            m_rows.removeAt(i);
            break;
        }
    }
    container->deleteLater();
}

FilterSpec FilterBuilderWidget::spec() const
{
    const Conj c = Conj(m_conj->currentData().toInt());
    QVector<FilterSpec> kids;
    for (const Row &r : m_rows) {
        if (r.leaf) kids.push_back(FilterSpec::leaf(r.leaf->text()));
        else if (r.group) kids.push_back(r.group->spec());
    }
    return FilterSpec::group(c, std::move(kids));
}

bool FilterBuilderWidget::isValid() const
{
    for (const Row &r : m_rows) {
        if (r.leaf && !r.leaf->isExpressionValid()) return false;
        if (r.group && !r.group->isValid()) return false;
    }
    return true;
}

void FilterBuilderWidget::onAnyChange()
{
    Q_EMIT changed();
    const bool v = isValid();
    if (v != m_lastValid) {
        m_lastValid = v;
        Q_EMIT validityChanged(v);
    }
}

}  // namespace Corbomite::Bases
```

- [ ] **Step 6: Add the source** to `libs/bases/CMakeLists.txt`: `src/FilterBuilderWidget.cpp`.

- [ ] **Step 7: Run to verify pass** — `cmake --build --preset dev -j 10 && cd build-dev && ctest -R tst_filter_builder_widget --output-on-failure`. Expected: PASS.

> Note: `FormulaInput::isExpressionValid()` returns true for empty/whitespace text (neutral), so `emptyLeaf_isValid` passes; `"((1"` is a parse error, so `invalidLeaf_makesGroupInvalid` passes.

- [ ] **Step 8: Commit**

```bash
git add libs/bases/include/corbomite/bases/FilterBuilderWidget.h libs/bases/src/FilterBuilderWidget.cpp \
        libs/bases/CMakeLists.txt libs/bases/tests/tst_filter_builder_widget.cpp libs/bases/tests/CMakeLists.txt
git commit -m "feat(bases): FilterBuilderWidget recursive group editor"
```

---

## Task 3: FilterBuilderDialog

A `QDialog` with a scope toggle over two `FilterBuilderWidget`s (per-view + global) and OK gated on both being valid.

**Files:**
- Create: `libs/bases/include/corbomite/bases/FilterBuilderDialog.h`
- Create: `libs/bases/src/FilterBuilderDialog.cpp`
- Modify: `libs/bases/CMakeLists.txt`
- Test: `libs/bases/tests/tst_filter_builder_dialog.cpp`
- Modify: `libs/bases/tests/CMakeLists.txt`

- [ ] **Step 1: Create the header** `libs/bases/include/corbomite/bases/FilterBuilderDialog.h`

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "FilterSpec.h"

#include <QDialog>
#include <QStringList>

class QComboBox;
class QStackedWidget;
class QDialogButtonBox;

namespace Corbomite::Bases {

class FilterBuilderWidget;

/// Edits both filter scopes. A scope combobox switches a QStackedWidget between
/// the per-view builder (index 0) and the global builder (index 1). OK is
/// disabled while either builder is invalid.
class FilterBuilderDialog : public QDialog
{
    Q_OBJECT
public:
    explicit FilterBuilderDialog(QWidget *parent = nullptr);

    void setScopes(const FilterSpec &globalSpec, const FilterSpec &perViewSpec,
                   const QStringList &candidates);

    FilterSpec globalSpec() const;
    FilterSpec perViewSpec() const;

private Q_SLOTS:
    void updateOkState();

private:
    QComboBox *m_scope = nullptr;
    QStackedWidget *m_stack = nullptr;
    FilterBuilderWidget *m_perView = nullptr;
    FilterBuilderWidget *m_global = nullptr;
    QDialogButtonBox *m_buttons = nullptr;
};

}  // namespace Corbomite::Bases
```

- [ ] **Step 2: Write the failing test** `libs/bases/tests/tst_filter_builder_dialog.cpp`

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/FilterBuilderDialog.h"
#include "corbomite/bases/FilterSpec.h"

#include <QDialogButtonBox>
#include <QPushButton>
#include <QtTest>

using namespace Corbomite::Bases;

class TestFilterBuilderDialog : public QObject
{
    Q_OBJECT
    static QPushButton *okButton(QDialog *d)
    {
        return d->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok);
    }
private slots:
    void accessors_returnEditedScopes()
    {
        FilterBuilderDialog d;
        FilterSpec g = FilterSpec::group(Conj::And, { FilterSpec::leaf(QStringLiteral("a == 1")) });
        FilterSpec pv = FilterSpec::group(Conj::Or, { FilterSpec::leaf(QStringLiteral("b == 2")) });
        d.setScopes(g, pv, {});
        QCOMPARE(d.globalSpec(), g);
        QCOMPARE(d.perViewSpec(), pv);
    }
    void okDisabled_whenEitherScopeInvalid()
    {
        FilterBuilderDialog d;
        d.setScopes(FilterSpec::group(Conj::And),                       // global: empty (valid)
                    FilterSpec::group(Conj::And, { FilterSpec::leaf(QStringLiteral("((1")) }),  // per-view invalid
                    {});
        QVERIFY(!okButton(&d)->isEnabled());
    }
    void okEnabled_whenBothValid()
    {
        FilterBuilderDialog d;
        d.setScopes(FilterSpec::group(Conj::And, { FilterSpec::leaf(QStringLiteral("a == 1")) }),
                    FilterSpec::group(Conj::And, { FilterSpec::leaf(QStringLiteral("b == 2")) }),
                    {});
        QVERIFY(okButton(&d)->isEnabled());
    }
};

QTEST_MAIN(TestFilterBuilderDialog)
#include "tst_filter_builder_dialog.moc"
```

- [ ] **Step 3: Register the test** — append to `libs/bases/tests/CMakeLists.txt`

```cmake
add_executable(tst_filter_builder_dialog tst_filter_builder_dialog.cpp)
add_test(NAME tst_filter_builder_dialog COMMAND tst_filter_builder_dialog)
target_link_libraries(tst_filter_builder_dialog PRIVATE Qt6::Test Qt6::Widgets Corbomite::Bases)
set_tests_properties(tst_filter_builder_dialog PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
```

- [ ] **Step 4: Run to verify it fails** — build → undefined `FilterBuilderDialog`.

- [ ] **Step 5: Implement** `libs/bases/src/FilterBuilderDialog.cpp`

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/FilterBuilderDialog.h"

#include "corbomite/bases/FilterBuilderWidget.h"

#include <KLocalizedString>

#include <QComboBox>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace Corbomite::Bases {

FilterBuilderDialog::FilterBuilderDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(i18n("Edit filters"));
    auto *root = new QVBoxLayout(this);

    m_scope = new QComboBox(this);
    m_scope->addItem(i18n("This view"));          // index 0
    m_scope->addItem(i18n("All views (global)")); // index 1
    root->addWidget(m_scope);

    m_stack = new QStackedWidget(this);
    m_perView = new FilterBuilderWidget(this);   // stack index 0
    m_global  = new FilterBuilderWidget(this);   // stack index 1
    m_stack->addWidget(m_perView);
    m_stack->addWidget(m_global);
    root->addWidget(m_stack, 1);

    m_buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    root->addWidget(m_buttons);

    connect(m_scope, QOverload<int>::of(&QComboBox::currentIndexChanged),
            m_stack, &QStackedWidget::setCurrentIndex);
    connect(m_buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_perView, &FilterBuilderWidget::validityChanged, this, &FilterBuilderDialog::updateOkState);
    connect(m_global,  &FilterBuilderWidget::validityChanged, this, &FilterBuilderDialog::updateOkState);

    updateOkState();
}

void FilterBuilderDialog::setScopes(const FilterSpec &globalSpec, const FilterSpec &perViewSpec,
                                    const QStringList &candidates)
{
    m_perView->setSpec(perViewSpec, candidates);
    m_global->setSpec(globalSpec, candidates);
    updateOkState();
}

FilterSpec FilterBuilderDialog::globalSpec() const  { return m_global->spec(); }
FilterSpec FilterBuilderDialog::perViewSpec() const { return m_perView->spec(); }

void FilterBuilderDialog::updateOkState()
{
    const bool ok = m_perView->isValid() && m_global->isValid();
    m_buttons->button(QDialogButtonBox::Ok)->setEnabled(ok);
}

}  // namespace Corbomite::Bases
```

- [ ] **Step 6: Add the source** to `libs/bases/CMakeLists.txt`: `src/FilterBuilderDialog.cpp`.

- [ ] **Step 7: Run to verify pass** — `cmake --build --preset dev -j 10 && cd build-dev && ctest -R tst_filter_builder_dialog --output-on-failure`. Expected: PASS.

- [ ] **Step 8: Commit**

```bash
git add libs/bases/include/corbomite/bases/FilterBuilderDialog.h libs/bases/src/FilterBuilderDialog.cpp \
        libs/bases/CMakeLists.txt libs/bases/tests/tst_filter_builder_dialog.cpp libs/bases/tests/CMakeLists.txt
git commit -m "feat(bases): FilterBuilderDialog scope toggle + OK gating"
```

---

## Task 4: BasesView wiring

Add a Filters toolbar button that opens the dialog and applies both scopes.

**Files:**
- Modify: `libs/bases/include/corbomite/bases/BasesView.h`
- Modify: `libs/bases/src/BasesView.cpp`
- Test: `libs/bases/tests/tst_bases_view_wiring.cpp`

- [ ] **Step 1: Declare the button + helpers in BasesView.h.** Near the other `QToolButton *m_*Btn = nullptr;` members add:

```cpp
    QToolButton *m_filtersBtn = nullptr;
```

Add a forward declaration `struct FilterSpec;` in the `namespace Corbomite::Bases {` block (near the other forward declarations like `class PropertiesMenuPanel;`). In the `public:` section (next to `applySummaryChoice`) declare:

```cpp
    /// Replace both filter scopes from edited specs, then recompute + persist.
    /// Public for testability (the dialog path calls this on accept).
    void applyFilterSpecs(const FilterSpec &globalSpec, const FilterSpec &perViewSpec);
```

In the `private:` section near `openSummaryDialog` declare:

```cpp
    void openFiltersDialog();
```

- [ ] **Step 2: Write the failing test** — add to `libs/bases/tests/tst_bases_view_wiring.cpp`. FOLLOW the existing fixture in that file (how a `BasesView` is built with stub services and a `.base` loaded — see the `applySummaryChoice_writesViewConfigAndRoundTrips` test added during the formula-editor work; mirror it). Add `#include "corbomite/bases/FilterSpec.h"` to the test's includes. Add this slot:

```cpp
    void applyFilterSpecs_writesBothScopesAndRoundTrips()
    {
        // ... construct BasesView `bv` + load a .base, per this file's fixture ...
        FilterSpec global = FilterSpec::group(Conj::And, { FilterSpec::leaf(QStringLiteral("file.hasTag(\"book\")")) });
        FilterSpec perView = FilterSpec::group(Conj::Or, {
            FilterSpec::leaf(QStringLiteral("status == \"open\"")),
            FilterSpec::leaf(QStringLiteral("status == \"wip\"")),
        });
        bv->applyFilterSpecs(global, perView);

        // Global collapsed to a bare rule; per-view is an or-map.
        QVERIFY(bv->query()->filters != nullptr);
        QCOMPARE(bv->query()->filters->serialize().toString(),
                 QStringLiteral("file.hasTag(\"book\")"));
        QVERIFY(bv->activeView()->filters != nullptr);
        QVERIFY(bv->activeView()->filters->serialize().toMap().contains(QStringLiteral("or")));

        // Round-trips through YAML.
        const QString yaml = bv->getViewData();
        QVERIFY(yaml.contains(QStringLiteral("file.hasTag")));
        QVERIFY(yaml.contains(QStringLiteral("status == \"open\"")));
    }
```

> Adapt the fixture setup lines to match the existing tests (service stubs, `setViewData`/load). `query()` and `activeView()` are existing public accessors on `BasesView`. If a `.base` with a `status`/`tag` column isn't already loaded by the fixture, any loaded `.base` works — the test only asserts on `filters`, which `applyFilterSpecs` sets regardless of columns.

- [ ] **Step 3: Run to verify it fails** — build → `applyFilterSpecs`/`m_filtersBtn` undefined.

- [ ] **Step 4: Implement.** In `libs/bases/src/BasesView.cpp` add includes near the other formula-editor includes:

```cpp
#include "corbomite/bases/FilterBuilderDialog.h"
#include "corbomite/bases/FilterSpec.h"
```

Create the button alongside the others. Find where `m_viewsBtn` is created and added to the toolbar (a `QToolButton` with text/icon then `toolbar->addWidget(m_viewsBtn)`), and immediately after that block add:

```cpp
    m_filtersBtn = new QToolButton(this);
    m_filtersBtn->setText(i18n("Filters"));
    m_filtersBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);
    connect(m_filtersBtn, &QToolButton::clicked, this, &BasesView::openFiltersDialog);
    toolbar->addWidget(m_filtersBtn);
```

> Use whatever the surrounding code names the toolbar layout/widget (it is the same one `m_viewsBtn` is added to) and match the sibling buttons' style calls (icon vs text). The exact style line above is a reasonable default; align it with the neighbours if they differ.

Add the method definitions near `openSummaryDialog`/`applySummaryChoice`:

```cpp
void BasesView::openFiltersDialog()
{
    if (!m_query || !m_activeView) return;
    FilterBuilderDialog dlg(this);
    dlg.setScopes(fromFilter(m_query->filters),
                  fromFilter(m_activeView->filters),
                  formulaCandidateList());
    if (dlg.exec() != QDialog::Accepted) return;
    applyFilterSpecs(dlg.globalSpec(), dlg.perViewSpec());
}

void BasesView::applyFilterSpecs(const FilterSpec &globalSpec, const FilterSpec &perViewSpec)
{
    if (!m_query || !m_activeView) return;
    m_query->filters = toFilter(globalSpec);
    m_activeView->filters = toFilter(perViewSpec);
    onConfigMutated();
}
```

- [ ] **Step 5: Run the wiring test + full regression suite** — `cmake --build --preset dev -j 10 && cd build-dev && ctest -R 'bases|formula|filter' --output-on-failure -j 10`. Expected: ALL pass (incl. `tst_bases_view_wiring`).

- [ ] **Step 6: Launch smoke (offscreen)** — `QT_QPA_PLATFORM=offscreen timeout 8 ./build-dev/Corbomite 2>/dev/null; echo "launch=$?"`. Expected: no crash (124 = timeout kill is fine; a SIGABRT/segfault is not).

- [ ] **Step 7: Commit**

```bash
git add libs/bases/include/corbomite/bases/BasesView.h libs/bases/src/BasesView.cpp \
        libs/bases/tests/tst_bases_view_wiring.cpp
git commit -m "feat(bases): wire filter builder into BasesView toolbar"
```

---

## Task 5: Documentation closeout

**Files:**
- Modify: `docs/decisions-archive.md`, `docs/PROJECT-STATE.md`, `docs/superpowers/plans/INDEX.md`

- [ ] **Step 1: Append a dated H2 to `docs/decisions-archive.md`** (newest on top, after the `---` and before the most recent existing `## 2026-05-27 …` entry). Header `## 2026-05-27 — Cluster D (Filter Builder) shipped`. Summarize: `FilterSpec` + pure `fromFilter`/`toFilter` (blank-leaf drop, single-child-And collapse via `optimize()`, Not preserved); `FilterBuilderWidget` (recursive group editor reusing `FormulaInput` leaves); `FilterBuilderDialog` (scope toggle, OK gated on both scopes valid); `BasesView` Filters toolbar button → `openFiltersDialog` → public `applyFilterSpecs` → `onConfigMutated`. Note tests added (`tst_filter_spec`, `tst_filter_builder_widget`, `tst_filter_builder_dialog`, + a `tst_bases_view_wiring` case), full suite green, build clean, offscreen launch clean. Note **pending user eyeball**: the nested builder tree + dialog rendering. Note **remaining in D: D.5 (plugin API)**.

- [ ] **Step 2: Update `docs/PROJECT-STATE.md`.** Add a top entry under `## Recent decisions` (≤3 sentences per the no-regrow rule); update `## Last touched`; update the cluster-table row D status note to mark the filter builder done (so D reads "… + formula editor + filter builder done; D.5 (plugin API) remains").

- [ ] **Step 3: Update `docs/superpowers/plans/INDEX.md`.** Bump the "Last updated" line and the Cluster D row Notes to record the filter builder done (cite this plan + spec), with only D.5 remaining.

- [ ] **Step 4: Commit**

```bash
git add docs/decisions-archive.md docs/PROJECT-STATE.md docs/superpowers/plans/INDEX.md
git commit -m "docs(tracking): close out Cluster D filter builder"
```

---

## Definition of Done

- Global + per-view filters can be built/edited from the toolbar Filters button as nested And/Or/Not trees of raw predicates; the `.base` round-trips them.
- Leaf predicates validate live and autocomplete; an invalid (non-empty, unparseable) leaf blocks OK.
- All bases+formula+filter tests green; clean build; offscreen launch clean.
- Closeout written; PROJECT-STATE + INDEX updated; interactive verification noted as pending user eyeball.

## Self-Review notes (for the executor)

- **Verify before coding:** `FilterRule::serialize()` returns the formula source `QString` and `FilterConjunction::serialize()` a one-key `QVariantMap` — the Task 1/Task 4 assertions depend on this; confirm against `libs/bases/src/FilterTree.cpp` and adjust the assertion shape if it differs.
- **`fromFilter`/`toFilter` are not perfectly inverse** for shapes that `optimize()` normalizes (a single-child And-group ⇄ a bare rule) — the round-trip tests are written to respect that (they assert via `serialize()` or use already-normalized specs). Don't "fix" a round-trip test by defeating `optimize()`.
- **Toolbar button placement (Task 4):** match the sibling `m_viewsBtn` creation idiom (icon/text style, the toolbar variable name) rather than the literal style line in the plan if the neighbours differ.
- **`tst_bases_view_wiring` fixture:** reuse the existing setup in that file (the formula-editor task added a `applySummaryChoice_*` test with the same shape) — do not invent a new fixture.
