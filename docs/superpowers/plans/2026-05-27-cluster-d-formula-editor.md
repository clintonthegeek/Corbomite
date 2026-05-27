# Cluster D — Formula Editor Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a UI for authoring Bases named formulas and per-column summary formulas (validated + autocompleting expression input) over the already-complete DSL engine, plus the backend gap-fill to evaluate custom summary formulas.

**Architecture:** Bottom-up. First two pure/backend tasks (registry name enumeration, `SummaryContext` + custom-summary evaluation). Then pure helpers (`FormulaOps` mutators, `FormulaCandidates` provider). Then widgets (`FormulaInput`, `FormulaEditDialog`). Then the `PropertiesMenuPanel` extension (signals only — no query mutation). Finally `BasesView` wiring that owns the `BasesQuery` mutation and routes through the existing `requestSave()` + recompute path (not the D.4c undo stack).

**Tech Stack:** C++20, Qt6 Widgets, KDE Frameworks (`KLocalizedString` for `i18n`), QtTest. Library: `libs/bases` (`Corbomite::Bases`).

**Spec:** [`docs/superpowers/specs/2026-05-27-cluster-d-formula-editor-design.md`](../specs/2026-05-27-cluster-d-formula-editor-design.md)

**Build/test commands (run from repo root):**
- Configure: `cmake --preset dev`
- Build: `cmake --build --preset dev -j 10`
- Single bases test: `cd build-dev && ctest -R '<name>' --output-on-failure`
- All bases tests: `cd build-dev && ctest -R bases --output-on-failure -j 10`

---

## File Structure

**New files:**
- `libs/bases/include/corbomite/bases/SummaryContext.h` — `values`-binding eval context (header-only, like `LambdaContext`).
- `libs/bases/include/corbomite/bases/FormulaOps.h` + `libs/bases/src/FormulaOps.cpp` — pure add/rename/delete mutators over the formulas map + order list.
- `libs/bases/include/corbomite/bases/FormulaCandidates.h` + `libs/bases/src/FormulaCandidates.cpp` — autocomplete candidate list + current-token extraction.
- `libs/bases/include/corbomite/bases/FormulaInput.h` + `libs/bases/src/FormulaInput.cpp` — validated, autocompleting single-line expression widget.
- `libs/bases/include/corbomite/bases/FormulaEditDialog.h` + `libs/bases/src/FormulaEditDialog.cpp` — name + expression dialog.
- Tests: `tst_formula_ops.cpp`, `tst_formula_candidates.cpp`, `tst_formula_input.cpp`, `tst_formula_edit_dialog.cpp`, `tst_bases_summary.cpp`.

**Modified files:**
- `libs/bases/include/corbomite/bases/FunctionRegistry.h` + `src/FunctionRegistry.cpp` — add name-enumeration accessors.
- `libs/bases/include/corbomite/bases/BasesQueryResult.h` + `src/BasesQueryResult.cpp` — inject `summaryFormulas` map; evaluate custom summaries.
- `libs/bases/src/QueryController.cpp:118` — pass `&m_query->summaryFormulas` to the result.
- `libs/bases/include/corbomite/bases/PropertiesMenuPanel.h` + `src/PropertiesMenuPanel.cpp` — Add-formula button, per-formula edit/delete, per-row summary picker, new signals.
- `libs/bases/include/corbomite/bases/BasesView.h` + `src/BasesView.cpp` — connect panel signals, open dialogs, mutate query, persist.
- `libs/bases/CMakeLists.txt` — add the 4 new `.cpp` sources.
- `libs/bases/tests/CMakeLists.txt` — register the 5 new tests.

---

## Task 1: FunctionRegistry name enumeration

The candidate provider (Task 4) needs to list every registered function name; the registry currently only does lookup.

**Files:**
- Modify: `libs/bases/include/corbomite/bases/FunctionRegistry.h:53` (after `findGlobal`)
- Modify: `libs/bases/src/FunctionRegistry.cpp`
- Test: `libs/bases/tests/tst_builtins.cpp` (add a slot)

- [ ] **Step 1: Write the failing test**

Add to `libs/bases/tests/tst_builtins.cpp` inside the test class's private slots, and a matching method body:

```cpp
void allNames_includesGlobalsAndMembers()
{
    auto &r = FunctionRegistry::global();
    const QStringList names = r.allNames();
    // Globals (addendum §8.2):
    QVERIFY(names.contains(QStringLiteral("now")));
    QVERIFY(names.contains(QStringLiteral("if")));
    // Member functions (addendum §8.4 / §8.7):
    QVERIFY(names.contains(QStringLiteral("startsWith")));
    QVERIFY(names.contains(QStringLiteral("sum")));
    // Deduped + sorted:
    QStringList sorted = names;
    sorted.sort();
    QCOMPARE(names, sorted);
    QCOMPARE(names.count(QStringLiteral("sum")), 1);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cd build-dev && ctest -R tst_bases_builtins --output-on-failure`
Expected: FAIL — `allNames` is not a member of `FunctionRegistry`.

- [ ] **Step 3: Declare the accessors**

In `FunctionRegistry.h`, after the `findGlobal` declaration (line ~53):

```cpp
    /// Sorted, deduped names of every registered function (globals + all
    /// per-type members). For autocomplete candidate lists.
    QStringList allNames() const;
```

- [ ] **Step 4: Implement**

In `FunctionRegistry.cpp` (add `#include <QSet>` if absent):

```cpp
QStringList FunctionRegistry::allNames() const
{
    QSet<QString> set;
    for (auto it = m_global.constBegin(); it != m_global.constEnd(); ++it)
        set.insert(it.key());
    for (auto t = m_byType.constBegin(); t != m_byType.constEnd(); ++t)
        for (auto f = t->constBegin(); f != t->constEnd(); ++f)
            set.insert(f.key());
    QStringList out(set.constBegin(), set.constEnd());
    out.sort();
    return out;
}
```

- [ ] **Step 5: Run test to verify it passes**

Run: `cd build-dev && ctest -R tst_bases_builtins --output-on-failure`
Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add libs/bases/include/corbomite/bases/FunctionRegistry.h libs/bases/src/FunctionRegistry.cpp libs/bases/tests/tst_builtins.cpp
git commit -m "feat(bases): FunctionRegistry::allNames for autocomplete candidates"
```

---

## Task 2: SummaryContext + custom-summary evaluation

`BasesQueryResult::summaryValue` currently hard-codes built-in names and ignores custom `summaryFormulas`. Add a `values`-binding context and resolve custom formulas first.

**Files:**
- Create: `libs/bases/include/corbomite/bases/SummaryContext.h`
- Modify: `libs/bases/include/corbomite/bases/BasesQueryResult.h:35-50,58`
- Modify: `libs/bases/src/BasesQueryResult.cpp:155-180`
- Modify: `libs/bases/src/QueryController.cpp:118`
- Test: create `libs/bases/tests/tst_bases_summary.cpp`

- [ ] **Step 1: Create the SummaryContext header**

`libs/bases/include/corbomite/bases/SummaryContext.h`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "EvalContext.h"

namespace Corbomite::Bases {

/// Binds the identifier `values` to a per-group ListValue for summary-formula
/// evaluation (addendum §9). Every other identifier delegates to an optional
/// parent context; with no parent, unknown identifiers resolve to nullptr.
class SummaryContext : public EvalContext
{
public:
    explicit SummaryContext(ValuePtr values, const EvalContext *parent = nullptr)
        : m_values(std::move(values)), m_parent(parent) {}

    ValuePtr getByIdentifier(const QString &name) const override
    {
        if (name.compare(QLatin1String("values"), Qt::CaseInsensitive) == 0)
            return m_values;
        return m_parent ? m_parent->getByIdentifier(name) : nullptr;
    }

    QStringList keys() const override
    {
        QStringList k = m_parent ? m_parent->keys() : QStringList{};
        k << QStringLiteral("values");
        return k;
    }

    const VaultResolver *vault() const override
    {
        return m_parent ? m_parent->vault() : nullptr;
    }

private:
    ValuePtr m_values;
    const EvalContext *m_parent;
};

}  // namespace Corbomite::Bases
```

- [ ] **Step 2: Write the failing test**

Create `libs/bases/tests/tst_bases_summary.cpp`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/Formula.h"
#include "corbomite/bases/FunctionRegistry.h"
#include "corbomite/bases/SummaryContext.h"
#include "corbomite/bases/Values.h"

#include <QtTest>

using namespace Corbomite::Bases;

class TestBasesSummary : public QObject
{
    Q_OBJECT
private slots:
    void customSummaryFormula_evaluatesAgainstValues()
    {
        // values = [1, 2, 3]; formula "values.sum() * 2" -> 12
        QVector<ValuePtr> nums{ std::make_shared<NumberValue>(1),
                                std::make_shared<NumberValue>(2),
                                std::make_shared<NumberValue>(3) };
        auto list = std::make_shared<ListValue>(nums);
        SummaryContext ctx(list);
        Formula f(QStringLiteral("values.sum() * 2"));
        QVERIFY(f.isValid());
        ValuePtr out = f.getValue(ctx, &FunctionRegistry::global());
        auto num = std::dynamic_pointer_cast<NumberValue>(out);
        QVERIFY(num);
        QCOMPARE(num->data(), 12.0);
    }
};

QTEST_MAIN(TestBasesSummary)
#include "tst_bases_summary.moc"
```

> Note: confirm `NumberValue::data()` accessor name against `libs/bases/include/corbomite/bases/Values.h`; adjust if the accessor differs (e.g. `value()`).

- [ ] **Step 3: Register the test in CMake**

In `libs/bases/tests/CMakeLists.txt`, append:

```cmake
add_executable(tst_bases_summary tst_bases_summary.cpp)
add_test(NAME tst_bases_summary COMMAND tst_bases_summary)
target_link_libraries(tst_bases_summary PRIVATE Qt6::Test Corbomite::Bases)
```

- [ ] **Step 4: Run test to verify it fails (then passes — this part needs no production change)**

Run: `cmake --build --preset dev -j 10 && cd build-dev && ctest -R tst_bases_summary --output-on-failure`
Expected: PASS (SummaryContext + Formula already suffice). This locks the `values`-binding contract before wiring it into `summaryValue`.

- [ ] **Step 5: Inject the summaryFormulas map into BasesQueryResult**

In `BasesQueryResult.h`: add forward-declared include for `Formula` (it is already pulled via `BasesViewConfig.h`? no — add `#include "Formula.h"`). Change the constructor and add a member:

```cpp
    BasesQueryResult(const BasesViewConfig &cfg,
                     QVector<std::shared_ptr<BasesEntry>> entries,
                     FunctionRegistry *funcs = nullptr,
                     const QHash<QString, Formula> *summaryFormulas = nullptr);
```

```cpp
    const QHash<QString, Formula> *m_summaryFormulas = nullptr;  // not owned
```

Add `#include "Formula.h"` and `#include <QHash>` (QHash already included) at the top.

- [ ] **Step 6: Store it in the constructor**

In `BasesQueryResult.cpp`, update the constructor initializer list to set `m_summaryFormulas(summaryFormulas)`. (Find the existing ctor definition near the top of the file and add the 4th param + initializer.)

- [ ] **Step 7: Resolve custom summaries first in summaryValue**

In `BasesQueryResult.cpp`, replace the body of `summaryValue` (currently lines ~155-180). Keep the existing built-in dispatch as the fallback; add the custom path before it:

```cpp
ValuePtr BasesQueryResult::summaryValue(int groupIndex,
                                        const PropertyId &prop,
                                        const QString &summaryFn) const
{
    const auto &gs = groups();
    if (groupIndex < 0 || groupIndex >= gs.size()) return NullValue::instance();
    QVector<ValuePtr> vals;
    for (const auto &e : gs[groupIndex].entries) vals.push_back(e->getValue(prop));
    auto list = std::make_shared<ListValue>(vals);

    // Custom summary formula (addendum §9) takes precedence over the
    // built-in names: evaluate the named formula with `values` bound.
    if (m_summaryFormulas) {
        auto it = m_summaryFormulas->constFind(summaryFn);
        if (it != m_summaryFormulas->constEnd()) {
            SummaryContext ctx(list);
            return it->getValue(ctx, m_funcs);
        }
    }

    // Built-in default names (addendum §9).
    const QString key = summaryFn.toLower();
    if (key == QLatin1String("sum"))     return list->sum();
    if (key == QLatin1String("min"))     return list->min();
    if (key == QLatin1String("max"))     return list->max();
    if (key == QLatin1String("mean")
     || key == QLatin1String("average")) return list->mean();
    if (key == QLatin1String("median"))  return list->median();
    if (key == QLatin1String("stddev"))  return list->stddev();
    if (key == QLatin1String("unique"))  return std::make_shared<NumberValue>(list->unique()->length());
    if (key == QLatin1String("count"))   return std::make_shared<NumberValue>(list->length());
    return NullValue::instance();
}
```

Add `#include "corbomite/bases/SummaryContext.h"` to the `.cpp` includes.

> Note: the original used a stack `ListValue list(vals);` and called `list.sum()`. This step switches to a shared_ptr so the same `list` can feed `SummaryContext`. Verify `ListValue::sum()/min()/...` are callable on the pointer (`list->sum()`); they are the same methods.

- [ ] **Step 8: Pass the map from QueryController**

In `libs/bases/src/QueryController.cpp:118`, change:

```cpp
    m_result = std::make_unique<BasesQueryResult>(
        *m_cfg, filtered, m_funcs,
        m_query ? &m_query->summaryFormulas : nullptr);
```

- [ ] **Step 9: Build + run all bases tests**

Run: `cmake --build --preset dev -j 10 && cd build-dev && ctest -R bases --output-on-failure -j 10`
Expected: all PASS (existing built-in summary behavior preserved; new custom path covered).

- [ ] **Step 10: Commit**

```bash
git add libs/bases/include/corbomite/bases/SummaryContext.h \
        libs/bases/include/corbomite/bases/BasesQueryResult.h \
        libs/bases/src/BasesQueryResult.cpp libs/bases/src/QueryController.cpp \
        libs/bases/tests/tst_bases_summary.cpp libs/bases/tests/CMakeLists.txt
git commit -m "feat(bases): evaluate custom summary formulas via SummaryContext"
```

---

## Task 3: FormulaOps pure mutators

Pure add/rename/delete over the `formulas` map + its order list. Used by `BasesView` for both named and summary formulas (the shapes are identical: `QHash<QString, Formula>` + `QStringList` order).

**Files:**
- Create: `libs/bases/include/corbomite/bases/FormulaOps.h`
- Create: `libs/bases/src/FormulaOps.cpp`
- Modify: `libs/bases/CMakeLists.txt`
- Test: create `libs/bases/tests/tst_formula_ops.cpp`

- [ ] **Step 1: Create the header**

`libs/bases/include/corbomite/bases/FormulaOps.h`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "Formula.h"

#include <QHash>
#include <QString>
#include <QStringList>

namespace Corbomite::Bases {

/// Pure mutators over a formula map + its insertion-order list (used for both
/// BasesQuery::formulas/formulaOrder and summaryFormulas/summaryFormulaOrder).
/// All return true on success, false on a precondition failure (no mutation).
namespace FormulaOps {

/// Add `name`→`source`. Fails if `name` is empty or already present.
bool add(QHash<QString, Formula> &map, QStringList &order,
         const QString &name, const QString &source);

/// Rename `oldName`→`newName`, preserving the formula + its order position.
/// Does NOT rewrite references (e.g. `formula.<oldName>`). Fails if `oldName`
/// is absent, `newName` is empty, or `newName` collides with a different key.
bool rename(QHash<QString, Formula> &map, QStringList &order,
            const QString &oldName, const QString &newName);

/// Replace the source of an existing `name`. Fails if `name` is absent.
bool setSource(QHash<QString, Formula> &map,
               const QString &name, const QString &source);

/// Remove `name`. Fails if absent.
bool remove(QHash<QString, Formula> &map, QStringList &order,
            const QString &name);

}  // namespace FormulaOps
}  // namespace Corbomite::Bases
```

- [ ] **Step 2: Write the failing test**

Create `libs/bases/tests/tst_formula_ops.cpp`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/FormulaOps.h"

#include <QtTest>

using namespace Corbomite::Bases;

class TestFormulaOps : public QObject
{
    Q_OBJECT
private slots:
    void add_insertsAndTracksOrder()
    {
        QHash<QString, Formula> m; QStringList o;
        QVERIFY(FormulaOps::add(m, o, QStringLiteral("ppu"), QStringLiteral("note.a / note.b")));
        QVERIFY(m.contains(QStringLiteral("ppu")));
        QCOMPARE(o, QStringList{QStringLiteral("ppu")});
        QCOMPARE(m.value(QStringLiteral("ppu")).source(), QStringLiteral("note.a / note.b"));
    }
    void add_rejectsDuplicateAndEmpty()
    {
        QHash<QString, Formula> m; QStringList o;
        QVERIFY(FormulaOps::add(m, o, QStringLiteral("x"), QStringLiteral("1")));
        QVERIFY(!FormulaOps::add(m, o, QStringLiteral("x"), QStringLiteral("2")));
        QVERIFY(!FormulaOps::add(m, o, QString(), QStringLiteral("2")));
        QCOMPARE(o.size(), 1);
    }
    void rename_preservesPosition()
    {
        QHash<QString, Formula> m; QStringList o;
        FormulaOps::add(m, o, QStringLiteral("a"), QStringLiteral("1"));
        FormulaOps::add(m, o, QStringLiteral("b"), QStringLiteral("2"));
        QVERIFY(FormulaOps::rename(m, o, QStringLiteral("a"), QStringLiteral("z")));
        QVERIFY(!m.contains(QStringLiteral("a")));
        QVERIFY(m.contains(QStringLiteral("z")));
        QCOMPARE(o, (QStringList{QStringLiteral("z"), QStringLiteral("b")}));
    }
    void rename_rejectsCollision()
    {
        QHash<QString, Formula> m; QStringList o;
        FormulaOps::add(m, o, QStringLiteral("a"), QStringLiteral("1"));
        FormulaOps::add(m, o, QStringLiteral("b"), QStringLiteral("2"));
        QVERIFY(!FormulaOps::rename(m, o, QStringLiteral("a"), QStringLiteral("b")));
    }
    void setSource_updatesExisting()
    {
        QHash<QString, Formula> m; QStringList o;
        FormulaOps::add(m, o, QStringLiteral("a"), QStringLiteral("1"));
        QVERIFY(FormulaOps::setSource(m, QStringLiteral("a"), QStringLiteral("9")));
        QCOMPARE(m.value(QStringLiteral("a")).source(), QStringLiteral("9"));
        QVERIFY(!FormulaOps::setSource(m, QStringLiteral("missing"), QStringLiteral("9")));
    }
    void remove_dropsKeyAndOrder()
    {
        QHash<QString, Formula> m; QStringList o;
        FormulaOps::add(m, o, QStringLiteral("a"), QStringLiteral("1"));
        QVERIFY(FormulaOps::remove(m, o, QStringLiteral("a")));
        QVERIFY(m.isEmpty());
        QVERIFY(o.isEmpty());
        QVERIFY(!FormulaOps::remove(m, o, QStringLiteral("a")));
    }
};

QTEST_APPLESS_MAIN(TestFormulaOps)
#include "tst_formula_ops.moc"
```

- [ ] **Step 3: Register the test**

In `libs/bases/tests/CMakeLists.txt`, append:

```cmake
add_executable(tst_formula_ops tst_formula_ops.cpp)
add_test(NAME tst_formula_ops COMMAND tst_formula_ops)
target_link_libraries(tst_formula_ops PRIVATE Qt6::Test Corbomite::Bases)
```

- [ ] **Step 4: Run test to verify it fails**

Run: `cmake --build --preset dev -j 10`
Expected: link/compile FAIL — `FormulaOps.cpp` not yet implemented (undefined references).

- [ ] **Step 5: Implement FormulaOps.cpp**

`libs/bases/src/FormulaOps.cpp`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/FormulaOps.h"

namespace Corbomite::Bases::FormulaOps {

bool add(QHash<QString, Formula> &map, QStringList &order,
         const QString &name, const QString &source)
{
    if (name.isEmpty() || map.contains(name)) return false;
    map.insert(name, Formula(source));
    order.append(name);
    return true;
}

bool rename(QHash<QString, Formula> &map, QStringList &order,
            const QString &oldName, const QString &newName)
{
    if (oldName == newName) return true;
    if (newName.isEmpty() || !map.contains(oldName) || map.contains(newName))
        return false;
    map.insert(newName, map.take(oldName));
    const int idx = order.indexOf(oldName);
    if (idx >= 0) order[idx] = newName;
    else order.append(newName);
    return true;
}

bool setSource(QHash<QString, Formula> &map,
               const QString &name, const QString &source)
{
    if (!map.contains(name)) return false;
    map.insert(name, Formula(source));
    return true;
}

bool remove(QHash<QString, Formula> &map, QStringList &order,
            const QString &name)
{
    if (!map.contains(name)) return false;
    map.remove(name);
    order.removeAll(name);
    return true;
}

}  // namespace Corbomite::Bases::FormulaOps
```

- [ ] **Step 6: Add source to CMake**

In `libs/bases/CMakeLists.txt`, add to the `add_library(corbomite-bases STATIC ...)` source list (alongside `src/NewItemSeed.cpp`):

```cmake
    src/FormulaOps.cpp
```

- [ ] **Step 7: Run test to verify it passes**

Run: `cmake --build --preset dev -j 10 && cd build-dev && ctest -R tst_formula_ops --output-on-failure`
Expected: PASS.

- [ ] **Step 8: Commit**

```bash
git add libs/bases/include/corbomite/bases/FormulaOps.h libs/bases/src/FormulaOps.cpp \
        libs/bases/CMakeLists.txt libs/bases/tests/tst_formula_ops.cpp libs/bases/tests/CMakeLists.txt
git commit -m "feat(bases): FormulaOps pure add/rename/setSource/remove mutators"
```

---

## Task 4: FormulaCandidates provider + token extraction

Produces the autocomplete candidate list and extracts the identifier token under a cursor. Both pure.

**Files:**
- Create: `libs/bases/include/corbomite/bases/FormulaCandidates.h`
- Create: `libs/bases/src/FormulaCandidates.cpp`
- Modify: `libs/bases/CMakeLists.txt`
- Test: create `libs/bases/tests/tst_formula_candidates.cpp`

- [ ] **Step 1: Create the header**

`libs/bases/include/corbomite/bases/FormulaCandidates.h`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "PropertyId.h"

#include <QString>
#include <QStringList>
#include <QVector>

namespace Corbomite::Bases {

class FunctionRegistry;

namespace FormulaCandidates {

enum class Mode { NamedFormula, SummaryFormula };

/// Build the flat (non-type-aware) candidate token list:
///   - the identifier roots this/note/file/formula
///   - one token per property (its `name`; formulas also as `formula.<name>`)
///   - every function name from the registry
///   - `values` first, in SummaryFormula mode
/// Result is sorted + deduped.
QStringList build(const QVector<PropertyId> &props,
                  const FunctionRegistry *funcs,
                  Mode mode);

/// The identifier token ending at byte offset `cursor` in `text`: scans left
/// over [A-Za-z0-9_$]. Returns {tokenStart, token}. Empty token when the char
/// before the cursor is not an identifier char.
struct TokenSpan { int start; QString token; };
TokenSpan tokenAt(const QString &text, int cursor);

}  // namespace FormulaCandidates
}  // namespace Corbomite::Bases
```

- [ ] **Step 2: Write the failing test**

Create `libs/bases/tests/tst_formula_candidates.cpp`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/FormulaCandidates.h"
#include "corbomite/bases/FunctionRegistry.h"

#include <QtTest>

using namespace Corbomite::Bases;

class TestFormulaCandidates : public QObject
{
    Q_OBJECT
private slots:
    void build_namedMode_hasRootsPropsAndFuncs()
    {
        QVector<PropertyId> props{
            {PropertyKind::Note, QStringLiteral("status")},
            {PropertyKind::Formula, QStringLiteral("ppu")},
        };
        QStringList c = FormulaCandidates::build(props, &FunctionRegistry::global(),
                                                 FormulaCandidates::Mode::NamedFormula);
        QVERIFY(c.contains(QStringLiteral("note")));
        QVERIFY(c.contains(QStringLiteral("file")));
        QVERIFY(c.contains(QStringLiteral("formula")));
        QVERIFY(c.contains(QStringLiteral("status")));
        QVERIFY(c.contains(QStringLiteral("formula.ppu")));
        QVERIFY(c.contains(QStringLiteral("now")));     // a global function
        QVERIFY(!c.contains(QStringLiteral("values"))); // not in named mode
    }
    void build_summaryMode_addsValues()
    {
        QStringList c = FormulaCandidates::build({}, &FunctionRegistry::global(),
                                                 FormulaCandidates::Mode::SummaryFormula);
        QVERIFY(c.contains(QStringLiteral("values")));
    }
    void tokenAt_scansLeftOverIdentChars()
    {
        auto t = FormulaCandidates::tokenAt(QStringLiteral("note.sta"), 8);
        QCOMPARE(t.start, 5);
        QCOMPARE(t.token, QStringLiteral("sta"));
    }
    void tokenAt_emptyAfterNonIdent()
    {
        auto t = FormulaCandidates::tokenAt(QStringLiteral("a + "), 4);
        QCOMPARE(t.token, QString());
        QCOMPARE(t.start, 4);
    }
};

QTEST_APPLESS_MAIN(TestFormulaCandidates)
#include "tst_formula_candidates.moc"
```

> Note: `tokenAt("note.sta", 8)` returns the trailing identifier `sta` (start 5), because `.` is not an identifier char — this matches what the completer needs (it completes the segment after the dot). The `note.` prefix is preserved by the caller during replacement.

- [ ] **Step 3: Register the test**

In `libs/bases/tests/CMakeLists.txt`, append:

```cmake
add_executable(tst_formula_candidates tst_formula_candidates.cpp)
add_test(NAME tst_formula_candidates COMMAND tst_formula_candidates)
target_link_libraries(tst_formula_candidates PRIVATE Qt6::Test Corbomite::Bases)
```

- [ ] **Step 4: Run test to verify it fails**

Run: `cmake --build --preset dev -j 10`
Expected: FAIL — undefined references to `FormulaCandidates::build`/`tokenAt`.

- [ ] **Step 5: Implement FormulaCandidates.cpp**

`libs/bases/src/FormulaCandidates.cpp`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/FormulaCandidates.h"

#include "corbomite/bases/FunctionRegistry.h"

#include <QSet>

namespace Corbomite::Bases::FormulaCandidates {

static bool isIdentChar(QChar c)
{
    return c.isLetterOrNumber() || c == QLatin1Char('_') || c == QLatin1Char('$');
}

QStringList build(const QVector<PropertyId> &props,
                  const FunctionRegistry *funcs, Mode mode)
{
    QSet<QString> set;
    set.insert(QStringLiteral("this"));
    set.insert(QStringLiteral("note"));
    set.insert(QStringLiteral("file"));
    set.insert(QStringLiteral("formula"));
    if (mode == Mode::SummaryFormula) set.insert(QStringLiteral("values"));

    for (const auto &p : props) {
        if (!p.name.isEmpty()) set.insert(p.name);
        if (p.kind == PropertyKind::Formula)
            set.insert(QStringLiteral("formula.") + p.name);
    }

    if (funcs)
        for (const auto &n : funcs->allNames()) set.insert(n);

    QStringList out(set.constBegin(), set.constEnd());
    out.sort();
    return out;
}

TokenSpan tokenAt(const QString &text, int cursor)
{
    cursor = qBound(0, cursor, int(text.size()));
    int start = cursor;
    while (start > 0 && isIdentChar(text.at(start - 1))) --start;
    return { start, text.mid(start, cursor - start) };
}

}  // namespace Corbomite::Bases::FormulaCandidates
```

- [ ] **Step 6: Add source to CMake**

In `libs/bases/CMakeLists.txt` source list:

```cmake
    src/FormulaCandidates.cpp
```

- [ ] **Step 7: Run test to verify it passes**

Run: `cmake --build --preset dev -j 10 && cd build-dev && ctest -R tst_formula_candidates --output-on-failure`
Expected: PASS.

- [ ] **Step 8: Commit**

```bash
git add libs/bases/include/corbomite/bases/FormulaCandidates.h libs/bases/src/FormulaCandidates.cpp \
        libs/bases/CMakeLists.txt libs/bases/tests/tst_formula_candidates.cpp libs/bases/tests/CMakeLists.txt
git commit -m "feat(bases): FormulaCandidates provider + token extraction"
```

---

## Task 5: FormulaInput widget — live validation

A `QLineEdit` subclass with a trailing valid/invalid indicator driven by `Formula::isValid()`.

**Files:**
- Create: `libs/bases/include/corbomite/bases/FormulaInput.h`
- Create: `libs/bases/src/FormulaInput.cpp`
- Modify: `libs/bases/CMakeLists.txt`
- Test: create `libs/bases/tests/tst_formula_input.cpp`

- [ ] **Step 1: Create the header**

`libs/bases/include/corbomite/bases/FormulaInput.h`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QLineEdit>
#include <QStringList>

class QAction;
class QCompleter;
class QStringListModel;

namespace Corbomite::Bases {

/// Single-line expression editor with live parse-level validation (trailing
/// ✓/✕ indicator + error tooltip) and flat token autocomplete. Validation
/// mirrors Obsidian's green-check: a transient Formula is re-parsed on every
/// keystroke; runtime (evaluation) errors are not reflected here.
class FormulaInput : public QLineEdit
{
    Q_OBJECT
public:
    explicit FormulaInput(QWidget *parent = nullptr);

    bool isExpressionValid() const { return m_valid; }

    /// Replace the autocomplete candidate tokens.
    void setCandidates(const QStringList &candidates);

Q_SIGNALS:
    void validityChanged(bool valid);

private Q_SLOTS:
    void revalidate();
    void onCompletionActivated(const QString &completion);
    void maybePopupCompleter();

private:
    QAction *m_indicator = nullptr;
    QCompleter *m_completer = nullptr;
    QStringListModel *m_candModel = nullptr;
    bool m_valid = true;   // empty == valid (neutral)
};

}  // namespace Corbomite::Bases
```

- [ ] **Step 2: Write the failing test (validation only — completer comes in Task 6)**

Create `libs/bases/tests/tst_formula_input.cpp`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/FormulaInput.h"

#include <QSignalSpy>
#include <QtTest>

using namespace Corbomite::Bases;

class TestFormulaInput : public QObject
{
    Q_OBJECT
private slots:
    void emptyIsNeutralValid()
    {
        FormulaInput in;
        QVERIFY(in.isExpressionValid());
    }
    void validExpression_marksValid()
    {
        FormulaInput in;
        QSignalSpy spy(&in, &FormulaInput::validityChanged);
        in.setText(QStringLiteral("note.a + 1"));
        QVERIFY(in.isExpressionValid());
    }
    void invalidExpression_marksInvalid()
    {
        FormulaInput in;
        QSignalSpy spy(&in, &FormulaInput::validityChanged);
        in.setText(QStringLiteral("note.a +"));    // dangling operator
        QVERIFY(!in.isExpressionValid());
        QVERIFY(spy.count() >= 1);
        QCOMPARE(spy.last().at(0).toBool(), false);
    }
};

QTEST_MAIN(TestFormulaInput)
#include "tst_formula_input.moc"
```

> Note: confirm `"note.a +"` is rejected by `Formula::isValid()` — it should be a parse error per the Pratt parser. If the parser tolerates it, substitute a definitely-invalid input such as `"((1"` (unbalanced paren). Pick whichever the existing `tst_parser` treats as invalid.

- [ ] **Step 3: Register the test**

In `libs/bases/tests/CMakeLists.txt`, append:

```cmake
add_executable(tst_formula_input tst_formula_input.cpp)
add_test(NAME tst_formula_input COMMAND tst_formula_input)
target_link_libraries(tst_formula_input PRIVATE Qt6::Test Qt6::Widgets Corbomite::Bases)
```

- [ ] **Step 4: Run test to verify it fails**

Run: `cmake --build --preset dev -j 10`
Expected: FAIL — `FormulaInput.cpp` not implemented.

- [ ] **Step 5: Implement FormulaInput.cpp (validation half; completer members created but inert until Task 6)**

`libs/bases/src/FormulaInput.cpp`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/FormulaInput.h"

#include "corbomite/bases/Formula.h"

#include <QAction>
#include <QCompleter>
#include <QIcon>
#include <QStringListModel>

namespace Corbomite::Bases {

FormulaInput::FormulaInput(QWidget *parent)
    : QLineEdit(parent)
{
    m_indicator = addAction(QIcon(), QLineEdit::TrailingPosition);

    m_candModel = new QStringListModel(this);
    m_completer = new QCompleter(m_candModel, this);
    m_completer->setCaseSensitivity(Qt::CaseInsensitive);
    m_completer->setCompletionMode(QCompleter::PopupCompletion);
    m_completer->setWidget(this);

    connect(this, &QLineEdit::textChanged, this, &FormulaInput::revalidate);
    connect(this, &QLineEdit::textChanged, this, &FormulaInput::maybePopupCompleter);
    connect(m_completer, QOverload<const QString &>::of(&QCompleter::activated),
            this, &FormulaInput::onCompletionActivated);

    revalidate();
}

void FormulaInput::setCandidates(const QStringList &candidates)
{
    m_candModel->setStringList(candidates);
}

void FormulaInput::revalidate()
{
    const QString src = text();
    bool valid;
    QString err;
    if (src.trimmed().isEmpty()) {
        valid = true;               // neutral
    } else {
        Formula f(src);
        valid = f.isValid();
        if (!valid) err = f.parseError().value_or(QString());
    }

    m_indicator->setIcon(
        src.trimmed().isEmpty() ? QIcon()
        : valid ? QIcon::fromTheme(QStringLiteral("dialog-ok-apply"))
                : QIcon::fromTheme(QStringLiteral("dialog-error")));
    m_indicator->setToolTip(valid ? QString() : err);

    if (valid != m_valid) {
        m_valid = valid;
        Q_EMIT validityChanged(m_valid);
    }
}

void FormulaInput::maybePopupCompleter() { /* implemented in Task 6 */ }
void FormulaInput::onCompletionActivated(const QString &) { /* implemented in Task 6 */ }

}  // namespace Corbomite::Bases
```

- [ ] **Step 6: Add source to CMake + ensure Widgets link**

In `libs/bases/CMakeLists.txt` source list add `src/FormulaInput.cpp`. Confirm `corbomite-bases` already links `Qt6::Widgets` (it does — `BasesView` etc. use widgets). If not, add it to the library's `target_link_libraries`.

- [ ] **Step 7: Run test to verify it passes**

Run: `cmake --build --preset dev -j 10 && cd build-dev && ctest -R tst_formula_input --output-on-failure`
Expected: PASS.

- [ ] **Step 8: Commit**

```bash
git add libs/bases/include/corbomite/bases/FormulaInput.h libs/bases/src/FormulaInput.cpp \
        libs/bases/CMakeLists.txt libs/bases/tests/tst_formula_input.cpp libs/bases/tests/CMakeLists.txt
git commit -m "feat(bases): FormulaInput widget with live parse validation"
```

---

## Task 6: FormulaInput autocomplete (token completion)

Wire the completer to complete only the identifier token under the cursor.

**Files:**
- Modify: `libs/bases/src/FormulaInput.cpp` (the two stub slots)
- Test: `libs/bases/tests/tst_formula_input.cpp` (add slots)

- [ ] **Step 1: Write the failing test**

Add to `TestFormulaInput`:

```cpp
    void completion_replacesOnlyCurrentToken()
    {
        FormulaInput in;
        in.setCandidates({QStringLiteral("status"), QStringLiteral("started")});
        in.setText(QStringLiteral("note.sta"));
        in.setCursorPosition(8);
        // Drive the activation path directly (popup isn't shown headless):
        QMetaObject::invokeMethod(&in, "onCompletionActivated",
                                  Q_ARG(QString, QStringLiteral("status")));
        QCOMPARE(in.text(), QStringLiteral("note.status"));
        QCOMPARE(in.cursorPosition(), 11);
    }
```

> `onCompletionActivated` is a private slot; `QMetaObject::invokeMethod` by name works because it is registered with the meta-object. Keep the slot in the `private Q_SLOTS:` section.

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build --preset dev -j 10 && cd build-dev && ctest -R tst_formula_input --output-on-failure`
Expected: FAIL — `onCompletionActivated` is a no-op, text stays `"note.sta"`.

- [ ] **Step 3: Implement the two slots**

Replace the two stub bodies in `FormulaInput.cpp` and add the include `#include "corbomite/bases/FormulaCandidates.h"` and `#include <QAbstractItemView>`:

```cpp
void FormulaInput::maybePopupCompleter()
{
    const auto span = FormulaCandidates::tokenAt(text(), cursorPosition());
    if (span.token.isEmpty()) {
        m_completer->popup()->hide();
        return;
    }
    m_completer->setCompletionPrefix(span.token);
    if (m_completer->completionCount() == 0) {
        m_completer->popup()->hide();
        return;
    }
    QRect r = cursorRect();
    r.setWidth(m_completer->popup()->sizeHintForColumn(0)
               + m_completer->popup()->verticalScrollBar()->sizeHint().width());
    m_completer->complete(r);
}

void FormulaInput::onCompletionActivated(const QString &completion)
{
    const int cursor = cursorPosition();
    const auto span = FormulaCandidates::tokenAt(text(), cursor);
    QString t = text();
    t.replace(span.start, cursor - span.start, completion);
    setText(t);
    setCursorPosition(span.start + completion.size());
}
```

Add includes `#include <QScrollBar>` for the `verticalScrollBar()` call.

> Note: `cursorRect()` is protected on `QLineEdit` in some Qt versions — if it does not compile, replace the `QRect r = cursorRect();` block with `m_completer->complete();` (popup at the widget's default position). The activation/replacement logic (what the test checks) is unaffected.

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build --preset dev -j 10 && cd build-dev && ctest -R tst_formula_input --output-on-failure`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add libs/bases/src/FormulaInput.cpp libs/bases/tests/tst_formula_input.cpp
git commit -m "feat(bases): FormulaInput token autocomplete"
```

---

## Task 7: FormulaEditDialog

A dialog with a name field + `FormulaInput` + help link + OK/Cancel; OK enabled only when name is non-empty, unique, and the expression is valid.

**Files:**
- Create: `libs/bases/include/corbomite/bases/FormulaEditDialog.h`
- Create: `libs/bases/src/FormulaEditDialog.cpp`
- Modify: `libs/bases/CMakeLists.txt`
- Test: create `libs/bases/tests/tst_formula_edit_dialog.cpp`

- [ ] **Step 1: Create the header**

`libs/bases/include/corbomite/bases/FormulaEditDialog.h`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "FormulaCandidates.h"

#include <QDialog>
#include <QStringList>

class QLineEdit;
class QDialogButtonBox;

namespace Corbomite::Bases {

class FormulaInput;

/// Add/edit dialog for a named or summary formula. The owner supplies the
/// candidate list and the set of names already taken (excluding the one being
/// edited) for collision checking.
class FormulaEditDialog : public QDialog
{
    Q_OBJECT
public:
    explicit FormulaEditDialog(FormulaCandidates::Mode mode, QWidget *parent = nullptr);

    void setCandidates(const QStringList &candidates);
    void setExistingNames(const QStringList &names);   ///< names that collide
    void setInitial(const QString &name, const QString &source);  ///< edit mode

    QString formulaName() const;
    QString formulaSource() const;

private Q_SLOTS:
    void updateOkState();

private:
    QLineEdit *m_name = nullptr;
    FormulaInput *m_input = nullptr;
    QDialogButtonBox *m_buttons = nullptr;
    QStringList m_existing;
};

}  // namespace Corbomite::Bases
```

- [ ] **Step 2: Write the failing test**

Create `libs/bases/tests/tst_formula_edit_dialog.cpp`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/FormulaEditDialog.h"

#include <QDialogButtonBox>
#include <QLineEdit>
#include <QPushButton>
#include <QtTest>

using namespace Corbomite::Bases;

class TestFormulaEditDialog : public QObject
{
    Q_OBJECT
    static QPushButton *okButton(QDialog *d)
    {
        auto *box = d->findChild<QDialogButtonBox *>();
        return box->button(QDialogButtonBox::Ok);
    }
    static QLineEdit *nameEdit(QDialog *d)
    {
        // The name field is the first QLineEdit that is not the FormulaInput.
        const auto edits = d->findChildren<QLineEdit *>();
        return edits.isEmpty() ? nullptr : edits.first();
    }
private slots:
    void okDisabledUntilNameAndValidExpr()
    {
        FormulaEditDialog d(FormulaCandidates::Mode::NamedFormula);
        QVERIFY(!okButton(&d)->isEnabled());            // empty name + empty expr
        d.setInitial(QStringLiteral("ppu"), QStringLiteral("note.a +"));  // invalid expr
        QVERIFY(!okButton(&d)->isEnabled());
        d.setInitial(QStringLiteral("ppu"), QStringLiteral("note.a + 1"));
        QVERIFY(okButton(&d)->isEnabled());
    }
    void okDisabledOnNameCollision()
    {
        FormulaEditDialog d(FormulaCandidates::Mode::NamedFormula);
        d.setExistingNames({QStringLiteral("taken")});
        d.setInitial(QStringLiteral("taken"), QStringLiteral("1"));
        QVERIFY(!okButton(&d)->isEnabled());
    }
    void accessorsReturnEnteredValues()
    {
        FormulaEditDialog d(FormulaCandidates::Mode::NamedFormula);
        d.setInitial(QStringLiteral("ppu"), QStringLiteral("note.a / note.b"));
        QCOMPARE(d.formulaName(), QStringLiteral("ppu"));
        QCOMPARE(d.formulaSource(), QStringLiteral("note.a / note.b"));
    }
};

QTEST_MAIN(TestFormulaEditDialog)
#include "tst_formula_edit_dialog.moc"
```

> Note: the `nameEdit` helper assumes the name `QLineEdit` is constructed before the `FormulaInput` (which is itself a `QLineEdit` subclass). Build the name field first in the `.cpp` so `findChildren` order holds; the test only uses `okButton`, but keep construction order deterministic.

- [ ] **Step 3: Register the test**

In `libs/bases/tests/CMakeLists.txt`, append:

```cmake
add_executable(tst_formula_edit_dialog tst_formula_edit_dialog.cpp)
add_test(NAME tst_formula_edit_dialog COMMAND tst_formula_edit_dialog)
target_link_libraries(tst_formula_edit_dialog PRIVATE Qt6::Test Qt6::Widgets Corbomite::Bases)
```

- [ ] **Step 4: Run test to verify it fails**

Run: `cmake --build --preset dev -j 10`
Expected: FAIL — `FormulaEditDialog.cpp` not implemented.

- [ ] **Step 5: Implement FormulaEditDialog.cpp**

`libs/bases/src/FormulaEditDialog.cpp`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/FormulaEditDialog.h"

#include "corbomite/bases/FormulaInput.h"

#include <KLocalizedString>

#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

namespace Corbomite::Bases {

FormulaEditDialog::FormulaEditDialog(FormulaCandidates::Mode mode, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(mode == FormulaCandidates::Mode::SummaryFormula
                       ? i18n("Edit summary formula")
                       : i18n("Edit formula"));

    auto *root = new QVBoxLayout(this);
    auto *form = new QFormLayout;

    m_name = new QLineEdit(this);          // constructed before FormulaInput
    form->addRow(i18n("Name:"), m_name);

    m_input = new FormulaInput(this);
    form->addRow(i18n("Expression:"), m_input);
    root->addLayout(form);

    auto *help = new QLabel(
        i18n("<a href=\"https://help.obsidian.md/bases/functions\">Functions reference</a>"),
        this);
    help->setOpenExternalLinks(false);
    connect(help, &QLabel::linkActivated, this,
            [](const QString &url) { QDesktopServices::openUrl(QUrl(url)); });
    root->addWidget(help);

    m_buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    root->addWidget(m_buttons);
    connect(m_buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    connect(m_name, &QLineEdit::textChanged, this, &FormulaEditDialog::updateOkState);
    connect(m_input, &FormulaInput::validityChanged, this, &FormulaEditDialog::updateOkState);

    updateOkState();
}

void FormulaEditDialog::setCandidates(const QStringList &candidates)
{
    m_input->setCandidates(candidates);
}

void FormulaEditDialog::setExistingNames(const QStringList &names)
{
    m_existing = names;
    updateOkState();
}

void FormulaEditDialog::setInitial(const QString &name, const QString &source)
{
    m_name->setText(name);
    m_input->setText(source);
    updateOkState();
}

QString FormulaEditDialog::formulaName() const { return m_name->text().trimmed(); }
QString FormulaEditDialog::formulaSource() const { return m_input->text(); }

void FormulaEditDialog::updateOkState()
{
    const QString name = m_name->text().trimmed();
    const bool ok = !name.isEmpty()
                    && !m_existing.contains(name)
                    && m_input->isExpressionValid()
                    && !m_input->text().trimmed().isEmpty();
    m_buttons->button(QDialogButtonBox::Ok)->setEnabled(ok);
}

}  // namespace Corbomite::Bases
```

- [ ] **Step 6: Add source to CMake**

In `libs/bases/CMakeLists.txt` add `src/FormulaEditDialog.cpp`.

- [ ] **Step 7: Run test to verify it passes**

Run: `cmake --build --preset dev -j 10 && cd build-dev && ctest -R tst_formula_edit_dialog --output-on-failure`
Expected: PASS.

- [ ] **Step 8: Commit**

```bash
git add libs/bases/include/corbomite/bases/FormulaEditDialog.h libs/bases/src/FormulaEditDialog.cpp \
        libs/bases/CMakeLists.txt libs/bases/tests/tst_formula_edit_dialog.cpp libs/bases/tests/CMakeLists.txt
git commit -m "feat(bases): FormulaEditDialog with name/expr validation"
```

---

## Task 8: PropertiesMenuPanel extensions

Add the "Add formula" button, per-formula edit/delete affordances, a per-row summary picker, and the signals `BasesView` will connect. The panel never mutates the query.

**Files:**
- Modify: `libs/bases/include/corbomite/bases/PropertiesMenuPanel.h`
- Modify: `libs/bases/src/PropertiesMenuPanel.cpp`
- Test: create `libs/bases/tests/tst_properties_menu_panel.cpp`

- [ ] **Step 1: Extend the header**

In `PropertiesMenuPanel.h`, add signals + a summary-options setter. Add `#include <QStringList>`, and inside the class:

```cpp
Q_SIGNALS:
    void addFormulaRequested();
    void editFormulaRequested(const QString &name);
    void deleteFormulaRequested(const QString &name);
    /// `summaryFnName` empty == None. The sentinel "__custom__" means the user
    /// chose "Custom…" (owner opens a summary FormulaEditDialog).
    void summaryChanged(const Corbomite::Bases::PropertyId &prop, const QString &summaryFnName);
```

Add a setter to provide the per-row summary state + available summary names:

```cpp
public:
    /// Configure the summary picker: built-in + custom names to list, and the
    /// current per-property selection. Call before/with setState.
    void setSummaryState(const QStringList &availableSummaryNames,
                         std::function<QString(const PropertyId &)> currentSummary);
```

And matching members:

```cpp
    QStringList m_summaryNames;
    std::function<QString(const PropertyId &)> m_currentSummary;
```

Add the include `#include <QStringList>` and ensure `<functional>` is present (it is).

Add a sentinel constant accessible to the owner — declare in the header inside the namespace:

```cpp
inline constexpr char kCustomSummarySentinel[] = "__custom__";
```

- [ ] **Step 2: Write the failing test**

Create `libs/bases/tests/tst_properties_menu_panel.cpp`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/PropertiesMenuPanel.h"

#include <QPushButton>
#include <QSignalSpy>
#include <QtTest>

using namespace Corbomite::Bases;

class TestPropertiesMenuPanel : public QObject
{
    Q_OBJECT
private slots:
    void addFormulaButton_emitsSignal()
    {
        PropertiesMenuPanel panel;
        QVector<PropertyId> order;
        panel.setState(&order, {}, [](const PropertyId &p) { return p.name; });
        QSignalSpy spy(&panel, &PropertiesMenuPanel::addFormulaRequested);

        // Find the "Add formula" button by object name (set in impl).
        auto *btn = panel.findChild<QPushButton *>(QStringLiteral("addFormulaButton"));
        QVERIFY(btn);
        btn->click();
        QCOMPARE(spy.count(), 1);
    }
};

QTEST_MAIN(TestPropertiesMenuPanel)
#include "tst_properties_menu_panel.moc"
```

- [ ] **Step 3: Register the test**

In `libs/bases/tests/CMakeLists.txt`, append:

```cmake
add_executable(tst_properties_menu_panel tst_properties_menu_panel.cpp)
add_test(NAME tst_properties_menu_panel COMMAND tst_properties_menu_panel)
target_link_libraries(tst_properties_menu_panel PRIVATE Qt6::Test Qt6::Widgets Corbomite::Bases)
```

- [ ] **Step 4: Run test to verify it fails**

Run: `cmake --build --preset dev -j 10`
Expected: FAIL — no `addFormulaButton` child / `addFormulaRequested` signal.

- [ ] **Step 5: Implement the "Add formula" button (minimal to pass), then layer the rest**

In `PropertiesMenuPanel.cpp` constructor, after the `hideAll` button, add:

```cpp
    auto *addFormula = new QPushButton(i18n("Add formula"), this);
    addFormula->setObjectName(QStringLiteral("addFormulaButton"));
    root->addWidget(addFormula);
    connect(addFormula, &QPushButton::clicked, this,
            &PropertiesMenuPanel::addFormulaRequested);
```

- [ ] **Step 6: Run the panel test to verify it passes**

Run: `cmake --build --preset dev -j 10 && cd build-dev && ctest -R tst_properties_menu_panel --output-on-failure`
Expected: PASS.

- [ ] **Step 7: Add the per-row summary picker + formula edit/delete affordances**

Replace `rebuild()` in `PropertiesMenuPanel.cpp` so each row is a composite widget set via `m_list->setItemWidget`. Add includes `#include <QComboBox>`, `#include <QHBoxLayout>`, `#include <QToolButton>`, `#include <QCheckBox>`, `#include <QWidget>`.

```cpp
void PropertiesMenuPanel::setSummaryState(const QStringList &availableSummaryNames,
                                          std::function<QString(const PropertyId &)> currentSummary)
{
    m_summaryNames = availableSummaryNames;
    m_currentSummary = std::move(currentSummary);
}

void PropertiesMenuPanel::rebuild()
{
    if (!m_order) return;
    m_updating = true;
    m_list->clear();
    QVector<PropertyId> ordered = *m_order;
    for (const auto &p : m_allProps)
        if (!ordered.contains(p)) ordered.push_back(p);

    for (const auto &p : ordered) {
        auto *it = new QListWidgetItem(m_list);
        it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled);
        it->setData(PropRole, toVariant(p));

        auto *row = new QWidget(m_list);
        auto *h = new QHBoxLayout(row);
        h->setContentsMargins(2, 0, 2, 0);

        auto *vis = new QCheckBox(m_displayName ? m_displayName(p) : p.name, row);
        vis->setChecked(m_order->contains(p));
        connect(vis, &QCheckBox::toggled, this, [this, p](bool on) {
            if (m_updating || !m_order) return;
            if (on) { if (!m_order->contains(p)) m_order->push_back(p); }
            else m_order->removeAll(p);
            if (m_onChanged) m_onChanged();
        });
        h->addWidget(vis, 1);

        if (p.kind == PropertyKind::Formula) {
            auto *edit = new QToolButton(row);
            edit->setText(QStringLiteral("✎"));
            connect(edit, &QToolButton::clicked, this,
                    [this, p]() { Q_EMIT editFormulaRequested(p.name); });
            h->addWidget(edit);
            auto *del = new QToolButton(row);
            del->setText(QStringLiteral("✕"));
            connect(del, &QToolButton::clicked, this,
                    [this, p]() { Q_EMIT deleteFormulaRequested(p.name); });
            h->addWidget(del);
        }

        auto *summary = new QComboBox(row);
        summary->addItem(i18n("None"), QString());
        for (const auto &n : m_summaryNames) summary->addItem(n, n);
        summary->addItem(i18n("Custom…"), QString::fromLatin1(kCustomSummarySentinel));
        const QString cur = m_currentSummary ? m_currentSummary(p) : QString();
        int idx = summary->findData(cur);
        summary->setCurrentIndex(idx >= 0 ? idx : 0);
        connect(summary, QOverload<int>::of(&QComboBox::activated), this,
                [this, p, summary](int) {
                    if (m_updating) return;
                    Q_EMIT summaryChanged(p, summary->currentData().toString());
                });
        h->addWidget(summary);

        m_list->setItemWidget(it, row);
        it->setSizeHint(row->sizeHint());
    }
    m_updating = false;
}
```

Remove the now-obsolete `onItemChanged`/`onRowsMoved` checkbox-scraping bodies' reliance on `checkState` — since visibility now lives in per-row `QCheckBox`es, replace those two slot bodies with a rebuild-from-widgets pass driven by reorder only:

```cpp
void PropertiesMenuPanel::onItemChanged() { /* visibility handled per-row checkbox */ }

void PropertiesMenuPanel::onRowsMoved()
{
    if (m_updating || !m_order) return;
    QVector<PropertyId> next;
    for (int i = 0; i < m_list->count(); ++i) {
        const PropertyId p = fromVariant(m_list->item(i)->data(PropRole));
        if (m_order->contains(p)) next.push_back(p);
    }
    *m_order = next;
    if (m_onChanged) m_onChanged();
}
```

> Note: with item widgets the drag handle still works (the `QListWidgetItem` carries `ItemIsDragEnabled`). The visibility checkbox moved into the row widget, so `itemChanged` no longer fires for check toggles — that is intentional; the per-row lambda mutates `*m_order` directly.

- [ ] **Step 8: Build + run the panel test + full bases suite**

Run: `cmake --build --preset dev -j 10 && cd build-dev && ctest -R 'bases|formula|properties_menu' --output-on-failure -j 10`
Expected: all PASS.

- [ ] **Step 9: Commit**

```bash
git add libs/bases/include/corbomite/bases/PropertiesMenuPanel.h libs/bases/src/PropertiesMenuPanel.cpp \
        libs/bases/tests/tst_properties_menu_panel.cpp libs/bases/tests/CMakeLists.txt
git commit -m "feat(bases): PropertiesMenuPanel add-formula + edit/delete + summary picker"
```

---

## Task 9: BasesView wiring

Connect the panel signals: open dialogs, mutate `BasesQuery`, persist + recompute. This is where named formulas, summaries, and the candidate provider come together.

**Files:**
- Modify: `libs/bases/src/BasesView.cpp` (panel construction block ~line 119-144; add helper methods)
- Modify: `libs/bases/include/corbomite/bases/BasesView.h` (declare new private helpers)
- Test: extend `libs/bases/tests/tst_bases_view_wiring.cpp`

- [ ] **Step 1: Declare helpers in BasesView.h**

In the `private:` section of `BasesView` (near `displayNameFor`), add:

```cpp
    QStringList summaryNamesForPicker() const;     // built-in defaults + custom summary names
    void openFormulaDialog(const QString &editName);  // add (empty) or edit a named formula
    void openSummaryDialog(const PropertyId &prop);   // create/assign a custom summary
    void applySummaryChoice(const PropertyId &prop, const QString &fnName);
    QStringList formulaCandidateList() const;
```

Add `#include <QStringList>` if not already present (it is, via Qt headers — verify).

- [ ] **Step 2: Write the failing test**

Add to `libs/bases/tests/tst_bases_view_wiring.cpp` a test that drives the panel's `addFormulaRequested`/`summaryChanged` path. Because opening a modal dialog is awkward headless, test the **non-dialog** mutation: `applySummaryChoice` writing to the active view's `summaries` map and persisting. Add a slot:

```cpp
    void summaryChoice_writesViewConfigAndPersists()
    {
        // Build a BasesView over a minimal .base with one note property.
        // (Reuse the existing fixture helpers in this test file for setup.)
        // After applying a built-in summary choice for `note.price`:
        //   activeView()->summaries[{Note,"price"}] == "average"
        // Assert via query()->getViewConfig()->summaries.
        // ... fixture-specific; mirror existing wiring tests in this file ...
    }
```

> Note: this test file already constructs a `BasesView` with stub services in prior D.4 tasks. Follow the existing fixture pattern in `tst_bases_view_wiring.cpp` (look at how `onNewItem`/export tests set up the view). Assert that after `applySummaryChoice({PropertyKind::Note,"price"}, "average")` the active view's `summaries` contains that mapping and `getViewData()` round-trips it. If exposing `applySummaryChoice` directly is cleaner than going through the private slot, make it a `public` method or keep it private and invoke via `QMetaObject::invokeMethod`.

- [ ] **Step 3: Run to verify it fails**

Run: `cmake --build --preset dev -j 10`
Expected: FAIL — helpers undefined.

- [ ] **Step 4: Implement the helpers + connect the panel signals**

In `BasesView.cpp`, add includes:

```cpp
#include "corbomite/bases/FormulaEditDialog.h"
#include "corbomite/bases/FormulaCandidates.h"
#include "corbomite/bases/FormulaOps.h"
#include "corbomite/bases/FunctionRegistry.h"
```

In the panel-construction block (after `m_propsPanel->setOnChanged(...)`, ~line 122), connect the new signals:

```cpp
    connect(m_propsPanel, &PropertiesMenuPanel::addFormulaRequested, this,
            [this]() { openFormulaDialog(QString()); });
    connect(m_propsPanel, &PropertiesMenuPanel::editFormulaRequested, this,
            [this](const QString &name) { openFormulaDialog(name); });
    connect(m_propsPanel, &PropertiesMenuPanel::deleteFormulaRequested, this,
            [this](const QString &name) {
                if (!m_query) return;
                FormulaOps::remove(m_query->formulas, m_query->formulaOrder, name);
                onConfigMutated();
            });
    connect(m_propsPanel, &PropertiesMenuPanel::summaryChanged, this,
            [this](const PropertyId &p, const QString &fn) {
                if (fn == QString::fromLatin1(kCustomSummarySentinel)) openSummaryDialog(p);
                else applySummaryChoice(p, fn);
            });
```

In the `m_propsBtn` clicked lambda (~line 138), after `setState(...)`, also feed the summary state:

```cpp
        m_propsPanel->setSummaryState(summaryNamesForPicker(),
            [this](const PropertyId &p) {
                return m_activeView ? m_activeView->summaries.value(p) : QString();
            });
```

Add the helper definitions (near `displayNameFor`):

```cpp
QStringList BasesView::summaryNamesForPicker() const
{
    // The 15 built-in defaults (addendum §9) + any custom summary formula names.
    QStringList names{
        QStringLiteral("average"), QStringLiteral("sum"), QStringLiteral("min"),
        QStringLiteral("max"), QStringLiteral("median"), QStringLiteral("stddev"),
        QStringLiteral("unique"), QStringLiteral("count") };
    if (m_query)
        for (const auto &n : m_query->summaryFormulaOrder)
            if (!names.contains(n)) names << n;
    return names;
}

QStringList BasesView::formulaCandidateList() const
{
    return FormulaCandidates::build(availableProperties(),
                                    m_funcs ? m_funcs : &FunctionRegistry::global(),
                                    FormulaCandidates::Mode::NamedFormula);
}

void BasesView::openFormulaDialog(const QString &editName)
{
    if (!m_query) return;
    FormulaEditDialog dlg(FormulaCandidates::Mode::NamedFormula, this);
    dlg.setCandidates(formulaCandidateList());
    QStringList existing = m_query->formulaOrder;
    existing.removeAll(editName);                 // editing its own name is fine
    dlg.setExistingNames(existing);
    if (!editName.isEmpty())
        dlg.setInitial(editName, m_query->formulas.value(editName).source());
    if (dlg.exec() != QDialog::Accepted) return;

    const QString name = dlg.formulaName();
    const QString src = dlg.formulaSource();
    if (editName.isEmpty()) {
        FormulaOps::add(m_query->formulas, m_query->formulaOrder, name, src);
    } else if (name != editName) {
        FormulaOps::rename(m_query->formulas, m_query->formulaOrder, editName, name);
        FormulaOps::setSource(m_query->formulas, name, src);
    } else {
        FormulaOps::setSource(m_query->formulas, name, src);
    }
    onConfigMutated();
}

void BasesView::openSummaryDialog(const PropertyId &prop)
{
    if (!m_query) return;
    FormulaEditDialog dlg(FormulaCandidates::Mode::SummaryFormula, this);
    dlg.setCandidates(FormulaCandidates::build(
        availableProperties(),
        m_funcs ? m_funcs : &FunctionRegistry::global(),
        FormulaCandidates::Mode::SummaryFormula));
    dlg.setExistingNames(m_query->summaryFormulaOrder);
    if (dlg.exec() != QDialog::Accepted) return;
    FormulaOps::add(m_query->summaryFormulas, m_query->summaryFormulaOrder,
                    dlg.formulaName(), dlg.formulaSource());
    applySummaryChoice(prop, dlg.formulaName());
}

void BasesView::applySummaryChoice(const PropertyId &prop, const QString &fnName)
{
    if (!m_activeView) return;
    if (fnName.isEmpty()) m_activeView->summaries.remove(prop);
    else m_activeView->summaries.insert(prop, fnName);
    onConfigMutated();
}
```

- [ ] **Step 5: Run the wiring test to verify it passes**

Run: `cmake --build --preset dev -j 10 && cd build-dev && ctest -R tst_bases_view_wiring --output-on-failure`
Expected: PASS.

- [ ] **Step 6: Full bases suite + launch smoke**

Run: `cmake --build --preset dev -j 10 && cd build-dev && ctest -R bases --output-on-failure -j 10`
Expected: all PASS. Then offscreen launch smoke:
Run: `QT_QPA_PLATFORM=offscreen ./build-dev/Corbomite --version 2>/dev/null || QT_QPA_PLATFORM=offscreen timeout 5 ./build-dev/Corbomite`
Expected: no crash / clean start.

- [ ] **Step 7: Commit**

```bash
git add libs/bases/include/corbomite/bases/BasesView.h libs/bases/src/BasesView.cpp \
        libs/bases/tests/tst_bases_view_wiring.cpp
git commit -m "feat(bases): wire formula + summary editing into BasesView"
```

---

## Task 10: Documentation closeout

**Files:**
- Modify: `docs/decisions-archive.md`
- Modify: `docs/PROJECT-STATE.md`
- Modify: `docs/superpowers/plans/INDEX.md`

- [ ] **Step 1: Append a dated closeout to `docs/decisions-archive.md`**

Add an H2 `## 2026-05-27 — Cluster D (Formula Editor) shipped` with: components built (FunctionRegistry::allNames, SummaryContext + custom-summary eval, FormulaOps, FormulaCandidates, FormulaInput, FormulaEditDialog, PropertiesMenuPanel extensions, BasesView wiring); test counts; note that autocomplete-popup visuals + dialog rendering + the real Ctrl-driven flows are **pending user eyeball** (offscreen Qt can't drive the completer popup); deferred items (type-aware member completion, syntax highlighting, filter builder, reference-rewrite on rename, grand-total footer).

- [ ] **Step 2: Update `docs/PROJECT-STATE.md`**

In `## Recent decisions`, add a top entry summarizing this pass (≤3 sentences per the no-regrow rule). Update the `## Last touched` block. In the cluster table row D, update the status note to `formula editor done; filter builder + D.5 remain`.

- [ ] **Step 3: Update `docs/superpowers/plans/INDEX.md`**

Bump "Last updated" and the Cluster D row Notes to record the formula editor as done, citing this plan + spec.

- [ ] **Step 4: Commit**

```bash
git add docs/decisions-archive.md docs/PROJECT-STATE.md docs/superpowers/plans/INDEX.md
git commit -m "docs(tracking): close out Cluster D formula editor"
```

---

## Definition of Done

- Named formulas can be added / edited / renamed / deleted from the Properties menu; `.base` round-trips them.
- A per-column summary can be set to None / a built-in / a custom formula; both built-in and custom summaries evaluate and render in the existing group-heading summary cells.
- `FormulaInput` shows live parse-level validity and offers flat token autocomplete.
- All bases tests green; clean build; offscreen launch clean.
- Closeout written; PROJECT-STATE + INDEX updated; interactive verification noted as pending user eyeball.

## Self-Review notes (for the executor)

- **Verify accessor names against headers before coding:** `NumberValue::data()` (Task 2 test), `ListValue::sum()/min()/max()/mean()/median()/stddev()/unique()/length()` callable via `->` (Task 2 step 7), `Formula::source()`/`isValid()`/`parseError()` (used throughout). The header reads in the spec confirm `source()`, `isValid()`, `parseError()`; double-check the `Value` accessors in `Values.h`.
- **Invalid-expression fixture:** Tasks 5/7 use `"note.a +"`; if the Pratt parser accepts a trailing operator, switch to `"((1"` (unbalanced paren) — whichever `tst_parser` already classifies as invalid.
- **`cursorRect()` visibility** (Task 6): if it doesn't compile, fall back to `m_completer->complete()`; the test path is unaffected.
