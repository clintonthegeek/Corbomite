# Cluster K — Bases Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ship a functional-MVP Bases feature — `.base` files open as an interactive Qt table with typed cell rendering, filters, sorts, groups, formulas and inline frontmatter-writeback editing — with enough ABI surface baked in for Cluster-N-style plugin extensions later.

**Architecture:** New `libs/bases/` library (namespace `Corbomite::Bases`) hosts the DSL and widgets. The formula language is a **hand-rolled Pratt parser** over a small lexer, producing an AST of `std::unique_ptr<Expr>` nodes. The typed Value layer is a **polymorphic `std::shared_ptr<Value>` hierarchy** (per DSL-addendum §15.2 recommendation) mirroring Obsidian's JS class structure so `isType()`, `instanceof`-style per-class function registration and `objectAccess` port cleanly. The `.base` file is parsed through `Markoff::YamlValue` (already the project's YAML engine). `BasesView` is a `Corbomite::TextFileView` subclass hosting a `QTableView` with a `QStyledItemDelegate` that dispatches per-cell-type rendering + inline editing. Rendering writes back to source notes through `Corbomite::FileManager::processFrontMatter`. Incremental refresh subscribes to `MetadataCache`'s Phase-I signals. The Bases feature ships as an internal plugin at `src/plugins/bases/` (Cluster-Q pattern).

**Tech Stack:** C++20, Qt6 (`QTableView`, `QStyledItemDelegate`, `QAbstractTableModel`, `QUndoStack`, `QDate`, `QDateTime`, `QRegularExpression`), KF6 (i18n), existing `Markoff::YamlValue` (rapidyaml), `Corbomite::MetadataCache`, `Corbomite::FileManager`, `Corbomite::Vault`, `Corbomite::MomentFormatter` (for date formatting).

**Source references:**
- Audit: `docs/obsidian-audit/domains/bases.md` (§1 Value hierarchy, §3 YAML schema, §8 invariants, §11 Corbomite mapping, §12 Markoff gaps).
- DSL addendum: `docs/obsidian-audit/addenda/2026-04-17-bases-formula-dsl.md` (§2 EBNF, §3 precedence, §4 operator semantics, §5 evaluation context, §6 full Value hierarchy, §7 string escapes, §8 function catalog, §9 summary formulas, §10 filter structure, §11 identifier resolution, §12 error surfaces, §13 plugin surface, §14 doc-vs-impl divergences, §15 implementation options).
- Prior art: `libs/storage/MetadataCache.{h,cpp}` (signals), `libs/vault/FileManager.{h,cpp}::processFrontMatter` (inline-edit backend), `libs/core/ViewRegistry.{h,cpp}` (view-type registration), `src/plugins/graph-view/` (internal-plugin shape, main-area view-type registration).

**Supersedes:** `docs/superpowers/plans/2026-04-14-cluster-k-bases-SCOUTING.md` (renamed after this plan lands).

**Explicitly deferred (post-MVP follow-ups — NOT in this plan):**
- Cards and List layouts (only Table for MVP).
- `registerGlobalFunc` / `registerInstanceFunc` plugin API — stubbed but not wired.
- View-rename wikilink rewrite (`[[basefile#viewname]]` → updated on rename).
- `![[Foo.base]]` embed in markdown (EmbedRegistry integration).
- Clipboard export (TSV / Markdown / HTML / `obsidian/table` MIME).
- Formula editor with syntax highlighting / autocomplete (plain `QLineEdit` / `QPlainTextEdit` for MVP).
- Per-`BasesView` undo/redo stack (Qt default only).
- Rich inline-edit widgets beyond `QLineEdit` / `QCheckBox` / `QDateEdit` (the audit's `metadataTypeManager.registeredTypeWidgets` is not ported).
- The `NewItemMenu` "+" button (pre-populating frontmatter from the filter).
- Per-cell hover-link popover via Cluster-J `HoverPopover` (follow-up under Cluster H #3).
- Tree-sitter grammar port for Bases (noted as option 15.1 in the addendum; punted in favour of the hand-rolled Pratt parser).

---

## Phase 1 — libs/bases/ scaffold + Value hierarchy foundation

**Phase goal:** New library compiles + links. `Value`, `NullValue`, `BooleanValue`, `NumberValue`, `StringValue`, `ListValue` (with core aggregates stub) land with unit tests. No parser yet, no YAML yet.

**Phase dependencies:** none. Starts from current master.

### Task 1.1 — Top-level CMake registration + library skeleton

**Files:**
- Create: `libs/bases/CMakeLists.txt`
- Create: `libs/bases/include/corbomite/bases/ValuePtr.h`
- Create: `libs/bases/include/corbomite/bases/Value.h`
- Create: `libs/bases/src/Value.cpp`
- Modify: `CMakeLists.txt` (add `add_subdirectory(libs/bases)` after `libs/vault`)

- [ ] **Step 1: Create library CMakeLists**

```cmake
# libs/bases/CMakeLists.txt
cmake_minimum_required(VERSION 3.19)
project(corbomite-bases VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Widgets)
find_package(KF6I18n REQUIRED)

add_library(corbomite-bases STATIC
    src/Value.cpp
    include/corbomite/bases/ValuePtr.h
    include/corbomite/bases/Value.h
)
set_target_properties(corbomite-bases PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    EXPORT_NAME Bases)
add_library(Corbomite::Bases ALIAS corbomite-bases)

target_include_directories(corbomite-bases
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${KDE_INSTALL_INCLUDEDIR}>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(corbomite-bases
    PUBLIC
        Qt6::Core
        Qt6::Widgets
        KF6::I18n
        $<BUILD_INTERFACE:Corbomite::Core>
        $<BUILD_INTERFACE:Corbomite::Storage>
        $<BUILD_INTERFACE:Corbomite::Vault>
        $<BUILD_INTERFACE:MarkoffParser::MarkoffParser>
)

if(DEFINED KDE_INSTALL_INCLUDEDIR)
    install(TARGETS corbomite-bases
        EXPORT CorbomiteTargets
        LIBRARY DESTINATION ${KDE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${KDE_INSTALL_LIBDIR})
    install(DIRECTORY include/corbomite/bases
            DESTINATION ${KDE_INSTALL_INCLUDEDIR}/corbomite)
endif()

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
```

- [ ] **Step 2: Create ValuePtr.h**

```cpp
// libs/bases/include/corbomite/bases/ValuePtr.h
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <memory>
namespace Corbomite::Bases {
class Value;
using ValuePtr = std::shared_ptr<Value>;
}
```

- [ ] **Step 3: Create Value.h (abstract base)**

```cpp
// libs/bases/include/corbomite/bases/Value.h
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "ValuePtr.h"
#include <QString>
#include <QStringList>

namespace Corbomite::Bases {

/// Abstract base for every typed cell value in a Bases table.
///
/// Subclasses override `type()` (discriminator used by `isType(name)`
/// and the per-type function registry), `isTruthy()`, `toString()`,
/// `equals()`, `looseEquals()`, `objectAccess()`, `keys()`.
///
/// `equals()` is type-strict structural equality. `looseEquals()` is
/// cross-type coerced (e.g. "2024-01-01"/String == 2024-01-01/Date).
/// Runtime identity is via `std::shared_ptr<Value>` (ValuePtr) — never
/// copy a Value by value, never store Value by value in a container.
class Value {
public:
    virtual ~Value() = default;

    /// Static-like type discriminator. Values: "Null", "Boolean",
    /// "Number", "String", "List", "Object", "Date", "Duration",
    /// "Regex", "File", "Link", "Url", "Tag", "Icon", "Image",
    /// "HTML", "Markdown", "Error", "ThisFile".
    virtual QString type() const = 0;

    /// Truthiness. Abstract; subclasses define type-specific truth.
    virtual bool isTruthy() const = 0;

    /// Per-type emptiness (used by the `.isEmpty()` function).
    /// Default: !isTruthy().
    virtual bool isEmpty() const { return !isTruthy(); }

    /// String rendering — used by toString() and `+` concat coercion.
    virtual QString toString() const { return {}; }

    /// Type-strict structural equality. Default: class + toString() match.
    virtual bool equals(const Value &other) const;

    /// Cross-type coerced equality. Delegated through staticLooseEquals.
    /// Override only to specialise (DateValue/DurationValue/LinkValue).
    virtual bool looseEquals(const Value &other) const;

    /// Identifier-style property lookup. Default: null.
    virtual ValuePtr objectAccess(const QString &key) const;

    /// Keys exposed for auto-complete. Default: empty.
    virtual QStringList keys() const { return {}; }

    /// Static helpers with null-safety (null <op> null == true, etc.).
    static bool staticEquals(const Value *a, const Value *b);
    static bool staticLooseEquals(const Value *a, const Value *b);
    static bool staticEquals(const ValuePtr &a, const ValuePtr &b)
    { return staticEquals(a.get(), b.get()); }
    static bool staticLooseEquals(const ValuePtr &a, const ValuePtr &b)
    { return staticLooseEquals(a.get(), b.get()); }
};

}  // namespace Corbomite::Bases
```

- [ ] **Step 4: Create Value.cpp (base-class null-safe helpers)**

```cpp
// libs/bases/src/Value.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/Value.h"

namespace Corbomite::Bases {

bool Value::equals(const Value &other) const
{
    return type() == other.type() && toString() == other.toString();
}

bool Value::looseEquals(const Value &other) const
{
    return equals(other);
}

ValuePtr Value::objectAccess(const QString &) const { return nullptr; }

bool Value::staticEquals(const Value *a, const Value *b)
{
    if (a == b) return true;
    if (!a || !b) return false;
    return a->equals(*b);
}

bool Value::staticLooseEquals(const Value *a, const Value *b)
{
    if (a == b) return true;
    if (!a || !b) return false;
    return a->looseEquals(*b) || b->looseEquals(*a);
}

}  // namespace Corbomite::Bases
```

- [ ] **Step 5: Register library in top-level CMake**

Edit `CMakeLists.txt` after the `add_subdirectory(libs/vault)` line. Insert:

```cmake
add_subdirectory(libs/bases)
```

- [ ] **Step 6: Configure + build**

Run: `cmake -B build -DCORBOMITE_DEV_BUILD=ON && cmake --build build -j 10 --target corbomite-bases`
Expected: succeeds, `libcorbomite-bases.a` exists under `build/libs/bases/`.

- [ ] **Step 7: Commit**

```bash
git add libs/bases/CMakeLists.txt libs/bases/include/ libs/bases/src/ CMakeLists.txt
git commit -m "$(cat <<'EOF'
feat(bases): libs/bases/ scaffold with abstract Value base

Cluster K Phase 1 Task 1.1 — new library target `Corbomite::Bases`
with the polymorphic Value base class. Shared-pointer hierarchy per
DSL addendum §15.2 recommendation.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

### Task 1.2 — NullValue + test harness

**Files:**
- Create: `libs/bases/include/corbomite/bases/Values.h`
- Create: `libs/bases/src/NullValue.cpp`
- Create: `libs/bases/tests/CMakeLists.txt`
- Create: `libs/bases/tests/tst_value_null.cpp`
- Modify: `libs/bases/CMakeLists.txt` (add `src/NullValue.cpp`)

- [ ] **Step 1: Write failing test first**

```cpp
// libs/bases/tests/tst_value_null.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include "corbomite/bases/Values.h"

using namespace Corbomite::Bases;

class TestNullValue : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void testNullIsSingleton()
    {
        auto a = NullValue::instance();
        auto b = NullValue::instance();
        QCOMPARE(a.get(), b.get());
    }

    void testNullType()
    {
        QCOMPARE(NullValue::instance()->type(), QStringLiteral("Null"));
    }

    void testNullIsNotTruthy()
    {
        QVERIFY(!NullValue::instance()->isTruthy());
    }

    void testNullToString()
    {
        QCOMPARE(NullValue::instance()->toString(), QString{});
    }

    void testStaticEqualsBothNull()
    {
        QVERIFY(Value::staticEquals(NullValue::instance(), NullValue::instance()));
    }
};

QTEST_APPLESS_MAIN(TestNullValue)
#include "tst_value_null.moc"
```

- [ ] **Step 2: Create Values.h header (will grow through Phases 1 + 2)**

```cpp
// libs/bases/include/corbomite/bases/Values.h
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "Value.h"

namespace Corbomite::Bases {

/// Singleton "no value". `NullValue::instance()` is the only way to
/// obtain one — constructor is private.
class NullValue : public Value
{
public:
    static ValuePtr instance();

    QString type() const override { return QStringLiteral("Null"); }
    bool isTruthy() const override { return false; }
    bool isEmpty() const override { return true; }
    QString toString() const override { return {}; }

private:
    NullValue() = default;
};

}  // namespace Corbomite::Bases
```

- [ ] **Step 3: Implement NullValue.cpp**

```cpp
// libs/bases/src/NullValue.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/Values.h"

namespace Corbomite::Bases {

ValuePtr NullValue::instance()
{
    // Leak-free singleton via shared_ptr with private-ctor access gate.
    struct Accessor : NullValue {};
    static ValuePtr s_null = std::shared_ptr<Value>(new Accessor());
    return s_null;
}

}  // namespace Corbomite::Bases
```

Add `src/NullValue.cpp` to the `add_library(corbomite-bases STATIC ...)` list in `libs/bases/CMakeLists.txt`.

- [ ] **Step 4: Create tests CMakeLists**

```cmake
# libs/bases/tests/CMakeLists.txt
cmake_minimum_required(VERSION 3.19)
project(Corbomite_BasesTests LANGUAGES CXX)
enable_testing()
find_package(Qt6 REQUIRED COMPONENTS Test)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

add_executable(tst_bases_value_null tst_value_null.cpp)
add_test(NAME tst_bases_value_null COMMAND tst_bases_value_null)
target_link_libraries(tst_bases_value_null PRIVATE Qt6::Test Corbomite::Bases)
```

- [ ] **Step 5: Build + run test — expect pass**

Run: `cmake --build build -j 10 --target tst_bases_value_null && cd build && ctest -R tst_bases_value_null --output-on-failure`
Expected: PASS (5 cases).

- [ ] **Step 6: Commit**

```bash
git add libs/bases/include/corbomite/bases/Values.h libs/bases/src/NullValue.cpp \
        libs/bases/tests/ libs/bases/CMakeLists.txt
git commit -m "feat(bases): NullValue singleton + test harness

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

### Task 1.3 — BooleanValue + NumberValue + StringValue (primitives)

**Files:**
- Modify: `libs/bases/include/corbomite/bases/Values.h` (append 3 classes)
- Create: `libs/bases/src/BooleanValue.cpp`
- Create: `libs/bases/src/NumberValue.cpp`
- Create: `libs/bases/src/StringValue.cpp`
- Create: `libs/bases/tests/tst_value_primitive.cpp`
- Modify: `libs/bases/tests/CMakeLists.txt` (add new test)
- Modify: `libs/bases/CMakeLists.txt` (add 3 .cpp files)

- [ ] **Step 1: Write failing tests**

```cpp
// libs/bases/tests/tst_value_primitive.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include "corbomite/bases/Values.h"

using namespace Corbomite::Bases;

class TestPrimitiveValues : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    // Boolean
    void testBoolType() {
        QCOMPARE(BooleanValue(true).type(), QStringLiteral("Boolean"));
    }
    void testBoolTruthy() {
        QVERIFY(BooleanValue(true).isTruthy());
        QVERIFY(!BooleanValue(false).isTruthy());
    }
    void testBoolEquals() {
        BooleanValue t1(true), t2(true), f1(false);
        QVERIFY(t1.equals(t2));
        QVERIFY(!t1.equals(f1));
    }

    // Number
    void testNumberType() {
        QCOMPARE(NumberValue(3.14).type(), QStringLiteral("Number"));
    }
    void testNumberTruthy() {
        QVERIFY(NumberValue(1).isTruthy());
        QVERIFY(!NumberValue(0).isTruthy());
        QVERIFY(!NumberValue(std::nan("")).isTruthy());
    }
    void testNumberToString() {
        QCOMPARE(NumberValue(42).toString(), QStringLiteral("42"));
        QCOMPARE(NumberValue(3.14).toString(), QStringLiteral("3.14"));
    }
    void testNumberInfinityRender() {
        QCOMPARE(NumberValue(std::numeric_limits<double>::infinity()).toString(),
                 QStringLiteral("∞"));
    }

    // String
    void testStringType() {
        QCOMPARE(StringValue(QStringLiteral("hi")).type(), QStringLiteral("String"));
    }
    void testStringTruthyByLength() {
        QVERIFY(StringValue(QStringLiteral("x")).isTruthy());
        QVERIFY(!StringValue(QString{}).isTruthy());
    }
    void testStringLengthObjectAccess() {
        StringValue s(QStringLiteral("hello"));
        auto len = s.objectAccess(QStringLiteral("length"));
        QVERIFY(len);
        QCOMPARE(len->type(), QStringLiteral("Number"));
        QCOMPARE(len->toString(), QStringLiteral("5"));
    }
};

QTEST_APPLESS_MAIN(TestPrimitiveValues)
#include "tst_value_primitive.moc"
```

- [ ] **Step 2: Extend Values.h with 3 primitive classes**

Append to `Values.h` (before the closing namespace brace):

```cpp
class BooleanValue : public Value
{
public:
    explicit BooleanValue(bool v) : m_data(v) {}
    bool data() const { return m_data; }
    QString type() const override { return QStringLiteral("Boolean"); }
    bool isTruthy() const override { return m_data; }
    bool isEmpty() const override { return !m_data; }
    QString toString() const override
    { return m_data ? QStringLiteral("true") : QStringLiteral("false"); }
    bool equals(const Value &other) const override;
private:
    bool m_data;
};

class NumberValue : public Value
{
public:
    explicit NumberValue(double v) : m_data(v) {}
    double data() const { return m_data; }
    QString type() const override { return QStringLiteral("Number"); }
    bool isTruthy() const override;      // false for 0 and NaN
    bool isEmpty() const override { return false; }  // per addendum §8.3 "isEmpty" entry
    QString toString() const override;   // "∞" for infinite
    bool equals(const Value &other) const override;
    bool looseEquals(const Value &other) const override;
private:
    double m_data;
};

class StringValue : public Value
{
public:
    explicit StringValue(QString v) : m_data(std::move(v)) {}
    const QString &data() const { return m_data; }
    QString type() const override { return QStringLiteral("String"); }
    bool isTruthy() const override { return !m_data.isEmpty(); }
    bool isEmpty() const override { return m_data.isEmpty(); }
    QString toString() const override { return m_data; }
    bool equals(const Value &other) const override;
    ValuePtr objectAccess(const QString &key) const override;  // "length" → NumberValue
    QStringList keys() const override;
protected:
    QString m_data;  // protected so TagValue/LinkValue/UrlValue/IconValue/ImageValue/HTMLValue subclass cleanly
};
```

- [ ] **Step 3: Implement BooleanValue.cpp**

```cpp
// libs/bases/src/BooleanValue.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/Values.h"
namespace Corbomite::Bases {
bool BooleanValue::equals(const Value &other) const
{
    if (auto *b = dynamic_cast<const BooleanValue *>(&other))
        return m_data == b->m_data;
    return false;
}
}  // namespace Corbomite::Bases
```

- [ ] **Step 4: Implement NumberValue.cpp**

```cpp
// libs/bases/src/NumberValue.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/Values.h"
#include <QLocale>
#include <cmath>

namespace Corbomite::Bases {

bool NumberValue::isTruthy() const
{
    return m_data != 0.0 && !std::isnan(m_data);
}

QString NumberValue::toString() const
{
    if (std::isnan(m_data))
        return QStringLiteral("NaN");
    if (std::isinf(m_data))
        return QStringLiteral("∞");
    // Match JS Number.prototype.toString default: integer-form for integers.
    if (m_data == std::floor(m_data)
        && std::fabs(m_data) < 1e15) {
        return QString::number(static_cast<qint64>(m_data));
    }
    return QString::number(m_data, 'g', 15);
}

bool NumberValue::equals(const Value &other) const
{
    if (auto *n = dynamic_cast<const NumberValue *>(&other))
        return m_data == n->m_data;
    return false;
}

bool NumberValue::looseEquals(const Value &other) const
{
    if (equals(other)) return true;
    // JS-style: NumberValue == BooleanValue coerces bool to 0/1.
    if (auto *b = dynamic_cast<const BooleanValue *>(&other))
        return m_data == (b->data() ? 1.0 : 0.0);
    return false;
}

}  // namespace Corbomite::Bases
```

- [ ] **Step 5: Implement StringValue.cpp**

```cpp
// libs/bases/src/StringValue.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/Values.h"
namespace Corbomite::Bases {

bool StringValue::equals(const Value &other) const
{
    if (auto *s = dynamic_cast<const StringValue *>(&other))
        return m_data == s->m_data;
    return false;
}

ValuePtr StringValue::objectAccess(const QString &key) const
{
    if (key == QLatin1String("length"))
        return std::make_shared<NumberValue>(static_cast<double>(m_data.size()));
    return nullptr;
}

QStringList StringValue::keys() const
{
    return {QStringLiteral("length")};
}

}  // namespace Corbomite::Bases
```

- [ ] **Step 6: Register tests + sources**

Add new sources to `libs/bases/CMakeLists.txt`:
```
src/BooleanValue.cpp
src/NumberValue.cpp
src/StringValue.cpp
```

Add new test to `libs/bases/tests/CMakeLists.txt`:
```cmake
add_executable(tst_bases_value_primitive tst_value_primitive.cpp)
add_test(NAME tst_bases_value_primitive COMMAND tst_bases_value_primitive)
target_link_libraries(tst_bases_value_primitive PRIVATE Qt6::Test Corbomite::Bases)
```

- [ ] **Step 7: Build + run — expect pass**

Run: `cmake --build build -j 10 --target tst_bases_value_primitive && cd build && ctest -R tst_bases_value_primitive --output-on-failure`

- [ ] **Step 8: Commit**

```bash
git add libs/bases/ && git commit -m "feat(bases): Boolean/Number/String primitive values

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

### Task 1.4 — ListValue + core aggregates

**Files:**
- Modify: `libs/bases/include/corbomite/bases/Values.h`
- Create: `libs/bases/src/ListValue.cpp`
- Create: `libs/bases/tests/tst_value_list.cpp`
- Modify: `libs/bases/CMakeLists.txt`, `libs/bases/tests/CMakeLists.txt`

- [ ] **Step 1: Write failing tests**

Write file `libs/bases/tests/tst_value_list.cpp` covering: `type()` == "List"; `isTruthy()` by length; `length` objectAccess; iteration (`get(i)`); `length()` method; `includes()` via `looseEquals`; `concat()`; `sort()` ascending; `unique()` deduplication; aggregates `sum`/`mean`/`min`/`max`/`median`/`stddev`; `earliest`/`latest`; `flatten` one-level.

Representative cases (full file is ~150 lines):

```cpp
void testListSum() {
    QVector<ValuePtr> xs {
        std::make_shared<NumberValue>(1),
        std::make_shared<NumberValue>(2),
        std::make_shared<NumberValue>(3)
    };
    ListValue l(xs);
    auto s = l.sum();
    QCOMPARE(s->type(), QStringLiteral("Number"));
    QCOMPARE(std::static_pointer_cast<NumberValue>(s)->data(), 6.0);
}

void testListStddev() {
    QVector<ValuePtr> xs {
        std::make_shared<NumberValue>(2),
        std::make_shared<NumberValue>(4),
        std::make_shared<NumberValue>(4),
        std::make_shared<NumberValue>(4),
        std::make_shared<NumberValue>(5),
        std::make_shared<NumberValue>(5),
        std::make_shared<NumberValue>(7),
        std::make_shared<NumberValue>(9),
    };
    ListValue l(xs);
    auto sd = l.stddev();  // population stddev == 2.0 (expected)
    QCOMPARE(std::static_pointer_cast<NumberValue>(sd)->data(), 2.0);
}

void testListUniqueViaLooseEquals() {
    QVector<ValuePtr> xs {
        std::make_shared<StringValue>(QStringLiteral("a")),
        std::make_shared<StringValue>(QStringLiteral("a")),
        std::make_shared<StringValue>(QStringLiteral("b")),
    };
    auto u = ListValue(xs).unique();
    QCOMPARE(u->length(), 2);
}
```

- [ ] **Step 2: Extend Values.h with ListValue**

```cpp
class ListValue : public Value
{
public:
    ListValue() = default;
    explicit ListValue(QVector<ValuePtr> items) : m_data(std::move(items)) {}

    const QVector<ValuePtr> &data() const { return m_data; }

    QString type() const override { return QStringLiteral("List"); }
    bool isTruthy() const override { return !m_data.isEmpty(); }
    bool isEmpty() const override { return m_data.isEmpty(); }
    QString toString() const override;  // `[a, b, c]`-like debug form
    bool equals(const Value &other) const override;
    ValuePtr objectAccess(const QString &key) const override;  // "length"
    QStringList keys() const override;

    // --- iteration helpers ---
    int length() const { return static_cast<int>(m_data.size()); }
    ValuePtr get(int i) const;
    bool includes(const ValuePtr &v) const;  // uses staticLooseEquals
    std::shared_ptr<ListValue> concat(const ListValue &other) const;
    std::shared_ptr<ListValue> reverse() const;
    std::shared_ptr<ListValue> flatten() const;
    std::shared_ptr<ListValue> unique() const;
    std::shared_ptr<ListValue> sort() const;
    std::shared_ptr<ListValue> slice(int start, int endOrNeg = -1) const;
    QString join(const QString &sep) const;

    // --- numeric aggregates (list must hold NumberValues) ---
    ValuePtr min() const;
    ValuePtr max() const;
    ValuePtr sum() const;
    ValuePtr mean() const;
    ValuePtr median() const;
    ValuePtr stddev() const;  // population stddev

    // --- date aggregates (list must hold DateValues) ---
    ValuePtr earliest() const;
    ValuePtr latest() const;

private:
    QVector<ValuePtr> m_data;
};
```

- [ ] **Step 3: Implement ListValue.cpp**

(~180 lines. Aggregate implementations use `std::accumulate`, `std::sort`, `<algorithm>`. Non-numeric elements in numeric aggregates propagate to `NullValue::instance()`. `unique()` + `includes()` use `Value::staticLooseEquals`. `flatten()` one-level only — nested ListValue elements are spliced in, non-list elements kept.)

Key snippets:

```cpp
ValuePtr ListValue::sum() const
{
    double acc = 0.0;
    for (const auto &v : m_data) {
        auto *n = dynamic_cast<NumberValue *>(v.get());
        if (!n) return NullValue::instance();
        acc += n->data();
    }
    return std::make_shared<NumberValue>(acc);
}

ValuePtr ListValue::median() const
{
    if (m_data.isEmpty()) return NullValue::instance();
    std::vector<double> xs; xs.reserve(m_data.size());
    for (const auto &v : m_data) {
        auto *n = dynamic_cast<NumberValue *>(v.get());
        if (!n) return NullValue::instance();
        xs.push_back(n->data());
    }
    std::sort(xs.begin(), xs.end());
    const auto n = xs.size();
    const double m = (n % 2) ? xs[n / 2] : 0.5 * (xs[n / 2 - 1] + xs[n / 2]);
    return std::make_shared<NumberValue>(m);
}

ValuePtr ListValue::stddev() const
{
    // Population stddev. Sample stddev is a follow-up (addendum §8.7 flags
    // this as unconfirmed; we choose population to match the reference impl's
    // divisor=n observed in the help-doc fixture).
    if (m_data.isEmpty()) return NullValue::instance();
    auto m = mean();
    auto *mn = dynamic_cast<NumberValue *>(m.get());
    if (!mn) return NullValue::instance();
    double sumSq = 0.0;
    for (const auto &v : m_data) {
        auto *n = dynamic_cast<NumberValue *>(v.get());
        if (!n) return NullValue::instance();
        const double d = n->data() - mn->data();
        sumSq += d * d;
    }
    return std::make_shared<NumberValue>(std::sqrt(sumSq / static_cast<double>(m_data.size())));
}
```

- [ ] **Step 4: Register + run test**

Append `src/ListValue.cpp` to the library; `tst_bases_value_list` to tests. Build + run.

- [ ] **Step 5: Commit**

```bash
git add libs/bases/ && git commit -m "feat(bases): ListValue + numeric/date aggregates

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

### Task 1.5 — Ritual 2: Phase 1 complete

- [ ] **Step 1: Update PROJECT-STATE.md**

In the Roadmap table, set Cluster K status to `In progress (phase 2)`. Add an in-flight row:

```
### Cluster K — Bases
- **Phase:** 2 of 9 (Phase 1 complete)
- **Last completed step:** Phase 1 — libs/bases/ scaffold + Value/Null/Boolean/Number/String/List primitive hierarchy (YYYY-MM-DD)
- **Next expected step:** Phase 2 — remaining Value subclasses (Object, Date, Duration, Regex, File, Link, Url, Tag, Image, Icon, HTML, Markdown, Error)
- **Owner:** agent session
- **Date last touched:** YYYY-MM-DD
```

- [ ] **Step 2: Commit**

```bash
git add docs/PROJECT-STATE.md && git commit -m "docs(cluster-k): Phase 1 complete

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Phase 2 — Value hierarchy completion

**Phase goal:** All remaining `Value` subclasses land with tests: `ObjectValue`, `DateValue` + `RelativeDateValue`, `DurationValue`, `RegExpValue`, `FileValue`, `LinkValue`, `UrlValue`, `TagValue` (+ tag-list specialisation of ListValue), `IconValue`, `ImageValue`, `HTMLValue`, `MarkdownValue`, `FormulaErrorValue`. Static `parseFromString` helpers for `DateValue`/`DurationValue`/`LinkValue`. `ObjectValue::fromFrontMatter` lazy coercer reads a `QJsonObject` (Obsidian frontmatter shape).

**Phase dependencies:** Phase 1.

### Task 2.1 — DateValue + RelativeDateValue + parseFromString

**Files:**
- Modify: `libs/bases/include/corbomite/bases/Values.h`
- Create: `libs/bases/src/DateValue.cpp`
- Create: `libs/bases/tests/tst_value_date.cpp`
- Modify: library CMake + tests CMake

- [ ] **Step 1: Write failing tests**

Cover: `type()` == "Date"; `parseFromString("2024-01-15")` → date-only (`hasTime()==false`); `parseFromString("2024-01-15T14:30:00")` → datetime; `parseFromString("2024-01-15 14:30:45.123")` → datetime with ms; parse reject on bogus; `objectAccess("year"/"month"/"day"/"hour"/"minute"/"second"/"millisecond"/"timestamp")` → NumberValue; month is 1-based; `equals` time-awareness (date-only vs datetime for same YMD → false); `looseEquals(StringValue)` coerces via parseFromString; `RelativeDateValue::toString()` returns a fromNow-style string ("3 days ago").

- [ ] **Step 2: Extend Values.h**

```cpp
class DateValue : public Value
{
public:
    DateValue(QDateTime dt, bool hasTime) : m_dt(std::move(dt)), m_hasTime(hasTime) {}

    const QDateTime &dateTime() const { return m_dt; }
    bool hasTime() const { return m_hasTime; }

    QString type() const override { return QStringLiteral("Date"); }
    bool isTruthy() const override { return true; }
    bool isEmpty() const override { return false; }
    QString toString() const override;
    bool equals(const Value &other) const override;
    bool looseEquals(const Value &other) const override;
    ValuePtr objectAccess(const QString &key) const override;
    QStringList keys() const override;

    /// `YYYY-MM-DD` (date-only) or `YYYY-MM-DD[ T]HH:MM[:SS[.ms]][TZ]`.
    /// Returns nullptr on malformed input.
    static std::shared_ptr<DateValue> parseFromString(const QString &text);

private:
    QDateTime m_dt;
    bool m_hasTime;
};

class RelativeDateValue : public DateValue
{
public:
    RelativeDateValue(QDateTime dt, bool hasTime) : DateValue(std::move(dt), hasTime) {}
    QString toString() const override;  // "3 days ago" style
};
```

- [ ] **Step 3: Implement DateValue.cpp**

Parser uses `QRegularExpression` with the two patterns from addendum §6.1:
- `^\d{4}-\d{2}-\d{2}$` (date-only)
- `^\d{4}-\d{2}-\d{2}[ T]\d{2}:\d{2}(:\d{2}(\.\d{1,9})?)?(Z|[+-]\d{2}(:?\d{2})?)?$` (datetime)

`equals` considers both YMD and time iff either has time=true; `hasTime` mismatch → false.

`looseEquals` against a StringValue tries `parseFromString(other.toString())`.

`objectAccess` for `"year"`, `"month"` (1-based), `"day"`, `"hour"`, `"minute"`, `"second"`, `"millisecond"`, `"timestamp"` (msecsSinceEpoch).

`RelativeDateValue::toString` uses `Corbomite::MomentFormatter` if a humanize helper exists; otherwise a simple dispatch over `qint64 secs = m_dt.secsTo(QDateTime::currentDateTimeUtc())` emitting "N days ago" / "in N days" / "moments ago". (Implement the dispatch inline — follow-up replaces with a Moment-style humanize helper.)

- [ ] **Step 4: Build + run test — expect pass**

- [ ] **Step 5: Commit**

### Task 2.2 — DurationValue + parseFromString

**Files:**
- Modify: `Values.h`
- Create: `libs/bases/src/DurationValue.cpp`
- Create: `libs/bases/tests/tst_value_duration.cpp`

- [ ] **Step 1: Write failing tests**

Cover: `parseFromString("P1Y2M3W4DT5H6M7S")` → all 7 components set; `parseFromString("5 days")` / `parseFromString("3w")` / `parseFromString("-2h")` / `parseFromString("100ms")` / `parseFromString("1 year")`; `toString()` human form; `addToDate(DateValue, subtract=false)` calendar-aware (months roll over correctly via `QDate::addMonths`); `looseEquals(StringValue)` via parseFromString; `objectAccess("weeks")` is days/7.

- [ ] **Step 2: Extend Values.h**

```cpp
struct DurationComponents
{
    qint64 years = 0, months = 0, days = 0;
    qint64 hours = 0, minutes = 0, seconds = 0, milliseconds = 0;
};

class DurationValue : public Value
{
public:
    explicit DurationValue(DurationComponents c) : m_c(c) {}

    const DurationComponents &components() const { return m_c; }
    qint64 totalMilliseconds() const;  // best-effort (days*86400000 + ...)

    QString type() const override { return QStringLiteral("Duration"); }
    bool isTruthy() const override;  // any non-zero component
    QString toString() const override;  // humanised e.g. "3 days 2 hours"
    bool equals(const Value &other) const override;
    bool looseEquals(const Value &other) const override;
    ValuePtr objectAccess(const QString &key) const override;
    QStringList keys() const override;

    /// Arithmetic: returns a new DateValue = date ± this.
    std::shared_ptr<DateValue> addToDate(const DateValue &d, bool subtract = false) const;

    /// Componentwise combine.
    DurationComponents plus(const DurationComponents &o) const;
    DurationComponents minus(const DurationComponents &o) const;
    DurationComponents timesScalar(double n) const;

    /// ISO-8601 `PnYnMnWnDTnHnMnS` **or** shorthand (`"5 days"`, `"3w"`, `"-2h"`,
    /// singular/plural, supports `ms|millisecond|milliseconds` which Obsidian's
    /// help docs omit — see addendum §14).
    static std::shared_ptr<DurationValue> parseFromString(const QString &text);

    static std::shared_ptr<DurationValue> fromMilliseconds(qint64 ms);

private:
    DurationComponents m_c;
};
```

- [ ] **Step 3: Implement**

Two parsers:
1. ISO-8601 regex: `^P(?:(\d+)Y)?(?:(\d+)M)?(?:(\d+)W)?(?:(\d+)D)?(?:T(?:(\d+)H)?(?:(\d+)M)?(?:(\d+(?:\.\d+)?)S)?)?$`. Weeks fold into days (×7).
2. Shorthand regex: `^(-?\d+)\s*(y|year|years|M|month|months|w|week|weeks|d|day|days|h|hour|hours|m|minute|minutes|s|second|seconds|ms|millisecond|milliseconds)$`. Note the **case-sensitive** `M`=months / `m`=minutes distinction (addendum §6.1).

`addToDate` computes the date arithmetic via `QDate::addYears(y).addMonths(m).addDays(d)` for the calendar portion, then `QDateTime::addMSecs(h*3600000 + m*60000 + s*1000 + ms)`.

`totalMilliseconds` approximates years/months with Date math: `DateValue(2000-01-01) + components - DateValue(2000-01-01)`. Exact for well-defined inputs; imprecise for partial years but matches reference impl.

- [ ] **Step 4 + 5: Build + run + commit**

### Task 2.3 — ObjectValue + lazy fromFrontMatter

**Files:**
- Modify: `Values.h`
- Create: `libs/bases/src/ObjectValue.cpp`
- Create: `libs/bases/tests/tst_value_object.cpp`

- [ ] **Step 1: Write failing tests**

Cover: `type()` == "Object"; empty-map `isEmpty()`; `keys()` returns insertion-order keys; case-insensitive `getInsensitive`/`objectAccess`; `fromFrontMatter` coerces:
- string "2024-01-01" → DateValue
- string "[[Page]]" → LinkValue
- string "https://..." → UrlValue
- plain string → StringValue
- `"tags": ["#a","#b"]` → a ListValue of TagValue
- nested map → recursive ObjectValue
- array → ListValue with element-wise coercion
- numbers / bools pass through
- null → NullValue::instance()

- [ ] **Step 2: Extend Values.h**

```cpp
class ObjectValue : public Value
{
public:
    ObjectValue() = default;

    /// Build from an Obsidian-shape frontmatter QJsonObject. Lazily wraps
    /// values — evaluation is eager in this port for simplicity (no lazy
    /// coercer; profiling can revisit).
    static std::shared_ptr<ObjectValue> fromFrontMatter(const QJsonObject &fm);

    void set(const QString &key, ValuePtr value);
    ValuePtr get(const QString &key) const;             // exact
    ValuePtr getInsensitive(const QString &key) const;  // case-insensitive

    QString type() const override { return QStringLiteral("Object"); }
    bool isTruthy() const override { return !m_order.isEmpty(); }
    bool isEmpty() const override { return m_order.isEmpty(); }
    bool equals(const Value &other) const override;
    ValuePtr objectAccess(const QString &key) const override;  // case-insensitive
    QStringList keys() const override { return m_order; }

    /// List of the stored Values in insertion order. Used by `object.values()`.
    QVector<ValuePtr> values() const;

    /// Keys/values iterable for object.map/object.filter.
    QVector<std::pair<QString, ValuePtr>> entries() const;

private:
    QStringList m_order;
    QHash<QString, ValuePtr> m_data;
};
```

- [ ] **Step 3: Implement**

`fromFrontMatter`:
```cpp
static ValuePtr coerceLeaf(const QJsonValue &v)
{
    if (v.isNull()) return NullValue::instance();
    if (v.isBool()) return std::make_shared<BooleanValue>(v.toBool());
    if (v.isDouble()) return std::make_shared<NumberValue>(v.toDouble());
    if (v.isString()) {
        const QString s = v.toString();
        // Wikilink? [[...]]  →  LinkValue
        if (s.startsWith(QLatin1String("[[")) && s.endsWith(QLatin1String("]]"))) {
            if (auto link = LinkValue::parseFromString(s)) return link;
        }
        // URL?
        if (s.startsWith(QLatin1String("http://")) || s.startsWith(QLatin1String("https://")))
            return std::make_shared<UrlValue>(s);
        // Date?
        if (auto d = DateValue::parseFromString(s)) return d;
        return std::make_shared<StringValue>(s);
    }
    if (v.isArray()) {
        QVector<ValuePtr> items;
        for (const auto &e : v.toArray()) items.push_back(coerceLeaf(e));
        return std::make_shared<ListValue>(items);
    }
    if (v.isObject()) return ObjectValue::fromFrontMatter(v.toObject());
    return NullValue::instance();
}

std::shared_ptr<ObjectValue> ObjectValue::fromFrontMatter(const QJsonObject &fm)
{
    auto obj = std::make_shared<ObjectValue>();
    for (auto it = fm.begin(); it != fm.end(); ++it) {
        const QString key = it.key();
        if (key.compare(QLatin1String("tags"), Qt::CaseInsensitive) == 0
            && it.value().isArray()) {
            // Special-cased — arr of strings → ListValue<TagValue>
            QVector<ValuePtr> tags;
            for (const auto &e : it.value().toArray()) {
                if (e.isString()) tags.push_back(std::make_shared<TagValue>(e.toString()));
            }
            obj->set(key, std::make_shared<ListValue>(tags));
        } else {
            obj->set(key, coerceLeaf(it.value()));
        }
    }
    return obj;
}
```

- [ ] **Step 4 + 5: Build + run + commit**

### Task 2.4 — RegExpValue

**Files:** `Values.h`, `src/RegExpValue.cpp`, `tests/tst_value_regex.cpp`

- [ ] **Step 1: Tests**

`type()` == "Regex"; `isTruthy()` true; `matches(s)` delegates to `QRegularExpression::match`; `parseFromString("/pat/flags")` returns non-null; `parseFromString("malformed")` returns null.

- [ ] **Step 2: Extend Values.h**

```cpp
class RegExpValue : public Value
{
public:
    explicit RegExpValue(QRegularExpression re) : m_re(std::move(re)) {}

    const QRegularExpression &regex() const { return m_re; }
    bool matches(const QString &s) const { return m_re.match(s).hasMatch(); }

    QString type() const override { return QStringLiteral("Regex"); }
    bool isTruthy() const override { return true; }
    QString toString() const override { return m_re.pattern(); }

    /// Parse `/body/flags`. Flags: g|i|m|s|u|y (unsupported flags retained
    /// for stringify round-trip but don't affect matching). Returns nullptr
    /// on malformed input.
    static std::shared_ptr<RegExpValue> parseFromString(const QString &literal);

private:
    QRegularExpression m_re;
};
```

- [ ] **Steps 3-5: Implement + test + commit**

### Task 2.5 — FileValue (TFile-backed)

**Files:** `Values.h`, `src/FileValue.cpp`, `tests/tst_value_file.cpp`

**Design note:** `FileValue` wraps a `Corbomite::TFile *` borrowed from the host `Vault`. The `file.*` accessors port from the audit's 14 FILE_PROPERTIES. Aggregate caches (`file.links`, `file.backlinks`, etc.) are fetched lazily from a `MetadataCache *` which the FileValue holds as a borrowed pointer. No ownership transfer.

- [ ] **Step 1: Tests**

Construct a `FileValue` from a test-fixture `TFile`; verify `objectAccess("name"/"basename"/"path"/"folder"/"ext")` against expected strings; `objectAccess("ctime"/"mtime"/"size")` returns NumberValue; `objectAccess("file")` returns self-pointer-same-shared_ptr (self-reference).

`file.tags` / `file.links` / `file.backlinks` / `file.embeds` / `file.properties` deferred to Task 2.6 once the MetadataCache wiring is in place — placeholder returns empty ListValue / ObjectValue in this task.

- [ ] **Step 2: Extend Values.h**

```cpp
class FileValue : public Value, public std::enable_shared_from_this<FileValue>
{
public:
    FileValue(TFile *file, MetadataCache *cache);

    TFile *file() const { return m_file; }

    QString type() const override { return QStringLiteral("File"); }
    bool isTruthy() const override { return m_file != nullptr; }
    QString toString() const override;  // file.name
    bool equals(const Value &other) const override;
    bool looseEquals(const Value &other) const override;
    ValuePtr objectAccess(const QString &key) const override;
    QStringList keys() const override;

    /// Aggregate accessors — populated lazily from MetadataCache.
    std::shared_ptr<ListValue> getLinks() const;
    std::shared_ptr<ListValue> getBacklinks() const;
    std::shared_ptr<ListValue> getEmbeds() const;
    std::shared_ptr<ListValue> getTags() const;
    std::shared_ptr<ObjectValue> getProperties() const;

    bool hasLink(const ValuePtr &other) const;    // FileValue|StringValue
    bool inFolder(const QString &folderPath) const;
    bool hasTag(const QStringList &tags) const;
    bool hasProperty(const QString &name) const;

protected:
    TFile *m_file;
    MetadataCache *m_cache;
    mutable std::shared_ptr<ListValue> m_cachedLinks;
    mutable std::shared_ptr<ListValue> m_cachedBacklinks;
    mutable std::shared_ptr<ListValue> m_cachedEmbeds;
    mutable std::shared_ptr<ListValue> m_cachedTags;
    mutable std::shared_ptr<ObjectValue> m_cachedProperties;
};

/// `this` binding. Forwards objectAccess to the enclosing BasesEntry's
/// getByIdentifier (set by QueryController on evaluation entry).
class ThisFileValue : public FileValue
{
public:
    using Forwarder = std::function<ValuePtr(const QString &)>;

    ThisFileValue(TFile *file, MetadataCache *cache, Forwarder forwarder);

    QString type() const override { return QStringLiteral("ThisFile"); }
    ValuePtr objectAccess(const QString &key) const override;

private:
    Forwarder m_forwarder;
};
```

- [ ] **Step 3: Implement FileValue.cpp**

`objectAccess` dispatch: `file`, `name`, `basename`, `fullname`, `path`, `folder`, `ext`, `ctime`, `mtime`, `size`, `links`, `backlinks`, `embeds`, `tags`, `properties`. `file` self-accessor returns `shared_from_this()`.

Aggregate accessors stub (return empty ListValue) — wire up MetadataCache reads in Task 2.6.

- [ ] **Step 4 + 5: Build + run + commit**

### Task 2.6 — FileValue aggregate accessors wired to MetadataCache

**Files:** `src/FileValue.cpp` (extend), `tests/tst_value_file_cache.cpp`

- [ ] **Step 1: Tests**

Populate an in-memory MetadataCache with a fixture note containing `links`, `embeds`, frontmatter, tags. Verify FileValue::getLinks / getBacklinks / getEmbeds / getTags / getProperties return non-empty ListValue/ObjectValue matching the fixture. Verify `file.properties.status == 'open'` resolves.

- [ ] **Step 2: Implement aggregate accessors**

```cpp
std::shared_ptr<ListValue> FileValue::getLinks() const
{
    if (m_cachedLinks) return m_cachedLinks;
    if (!m_cache || !m_file) return std::make_shared<ListValue>();
    const auto cache = m_cache->getFileCache(m_file->path());
    QVector<ValuePtr> items;
    if (cache && cache->links) {
        for (const LinkCache &l : *cache->links)
            items.push_back(std::make_shared<LinkValue>(l.link, m_file->path(), l.displayText.value_or(QString{})));
    }
    m_cachedLinks = std::make_shared<ListValue>(items);
    return m_cachedLinks;
}
// ... getBacklinks reverse-scans metadata cache for paths linking to m_file->path()
// ... getEmbeds reads cache->embeds
// ... getTags reads cache->tags → ListValue<TagValue>
// ... getProperties reads cache->frontmatter → ObjectValue::fromFrontMatter
```

`getBacklinks` iterates `m_cache->allPaths()`, reads each cache entry's `links`, and collects paths that point to `m_file->path()` into a ListValue of LinkValues. Per the audit (§11 "file.backlinks — performance heavy"), this is intentionally O(vault), cached per-instance.

- [ ] **Step 3 + 4 + 5: Build + run + commit**

### Task 2.7 — LinkValue + UrlValue + TagValue + IconValue + ImageValue + HTMLValue + MarkdownValue + FormulaErrorValue

**Files:** `Values.h`, one .cpp per class, `tests/tst_value_string_subclasses.cpp`

**Design note:** All subclasses of StringValue (Link/Url/Tag/Icon/Image/HTML/Markdown) reuse `m_data` from the parent; override `type()` and `renderTo` semantics. For MVP we skip `renderTo` (no Qt rendering yet — Phase 8's delegate dispatches by type name).

- [ ] **Step 1: Tests**

Aggregate tests (one file covering each class's `type()`, `toString()`, key specialisations):
- `LinkValue("note", "src.md", "Display").toString()` == `"[[note|Display]]"`; `resolve()` returns TFile* via metadata lookup; `linksTo(FileValue)` checks against link index.
- `TagValue("#foo/bar").tagMatches("#foo")` true (hierarchical prefix, `/` boundary); `TagValue("#foo").tagMatches("#foobar")` false.
- `UrlValue("https://x.y").type()` == "URL"; `toString()` == "https://x.y".
- `IconValue("star").type()` == "Icon" (class field; inherits "String" dispatch per addendum §6).
- Etc. One test each.

- [ ] **Step 2: Extend Values.h with the 8 classes**

```cpp
class LinkValue : public StringValue
{
public:
    LinkValue(QString link, QString sourcePath = {}, QString display = {})
        : StringValue(std::move(link)),
          m_sourcePath(std::move(sourcePath)),
          m_display(std::move(display)) {}

    const QString &sourcePath() const { return m_sourcePath; }
    const QString &display() const { return m_display; }

    QString type() const override { return QStringLiteral("Link"); }
    QString toString() const override;  // [[data|display]] or [[data]]
    bool looseEquals(const Value &other) const override;

    /// Parse `[[...]]` or `[[...|display]]`. Returns nullptr if the string
    /// is not a wikilink literal.
    static std::shared_ptr<LinkValue> parseFromString(const QString &text,
                                                       const QString &sourcePath = {});

    /// Resolve via metadata; returns the TFile or nullptr. Needs a Vault
    /// pointer wired via QueryContext (set in Phase 7). Returns nullptr
    /// until wiring.
    TFile *resolve(Vault *vault) const;

private:
    QString m_sourcePath;
    QString m_display;
};

class UrlValue : public StringValue { /* type() == "URL" */ };
class TagValue : public StringValue {
public:
    explicit TagValue(QString tag) : StringValue(std::move(tag)) {}
    QString type() const override { return QStringLiteral("Tag"); }
    bool tagMatches(const QString &other) const;  // hierarchical / boundary
};
class IconValue : public StringValue { /* type() == "Icon" */ };
class ImageValue : public StringValue { /* type() == "Image" */ };
class HTMLValue : public StringValue { /* type() == "HTML" */ };
class MarkdownValue : public StringValue { /* type() == "Markdown" */ };
class FormulaErrorValue : public Value {
public:
    explicit FormulaErrorValue(QString msg) : m_msg(std::move(msg)) {}
    const QString &message() const { return m_msg; }
    QString type() const override { return QStringLiteral("Error"); }
    bool isTruthy() const override { return false; }
    QString toString() const override { return m_msg; }
private:
    QString m_msg;
};
```

- [ ] **Step 3: Implement**

Each class: small (<20 LoC). `TagValue::tagMatches`: `other == this` OR `this.startsWith(other + "/")`.

- [ ] **Step 4 + 5: Build + run + commit**

### Task 2.8 — Phase 2 Ritual 2 closeout

Update `docs/PROJECT-STATE.md` in-flight row to Phase 3; commit.

---

## Phase 3 — Lexer + Pratt parser + AST

**Phase goal:** A string like `note.status == 'open' && file.size > 1000` parses into an `ExprPtr` AST tree. Error recovery produces a `FormulaErrorValue` sentinel at the AST root. Tests fix parses to specific AST shapes.

**Phase dependencies:** Phase 1 (for NumberValue / StringValue literal nodes), Phase 2 (for RegExpValue).

### Task 3.1 — Lexer

**Files:**
- Create: `libs/bases/include/corbomite/bases/Lexer.h`
- Create: `libs/bases/src/Lexer.cpp`
- Create: `libs/bases/tests/tst_lexer.cpp`

- [ ] **Step 1: Tests**

Cover:
- Keywords: `null`, `true`, `false` → dedicated token kinds (reserved; not identifiers).
- Identifiers: `[A-Za-z_$][A-Za-z_$0-9]*`.
- Integer + decimal literal: `42`, `3.14`. Integer overflow guard: `1e309` → NaN (passed through unmodified to the runtime NumberValue).
- Strings: double-quoted via JS/JSON escapes (`"hi\n"`, `"\"quoted\""`, `"\u00e9"`); single-quoted rewritten through the addendum §7 transform (`'it\'s "cool"'` → `it's "cool"`).
- Regex literal: `/pattern/flags`. Distinguished from division by a simple prev-token heuristic (after operator/`(`/`,` → regex; after identifier/number/`)`/`]` → division).
- Operators: `||`, `&&`, `==`, `!=`, `<=`, `>=`, `<`, `>`, `+`, `-`, `*`, `/`, `%`, `!`, `(`, `)`, `[`, `]`, `,`, `.`.
- Whitespace skipping.
- Unknown char: emits `Invalid` token with position.

- [ ] **Step 2: Lexer.h**

```cpp
// libs/bases/include/corbomite/bases/Lexer.h
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QString>
#include <QVector>
#include <variant>

namespace Corbomite::Bases {

enum class TokKind {
    // Literals
    Null, True, False, Number, String, Regex, Identifier,
    // Grouping / postfix
    LParen, RParen, LBracket, RBracket, Comma, Dot,
    // Binary
    OrOr, AndAnd,
    EqEq, BangEq,
    Lt, Gt, LtEq, GtEq,
    Plus, Minus, Star, Slash, Percent,
    // Unary
    Bang,
    // End
    End,
    // Error recovery
    Invalid,
};

struct Token {
    TokKind kind = TokKind::End;
    int start = 0;   // source offset
    int length = 0;
    // Literal payload:
    double numberValue = 0.0;
    QString textValue;  // identifier text / string body / regex pattern
    QString regexFlags;
};

class Lexer
{
public:
    explicit Lexer(QString src);

    QVector<Token> tokenize();  // drives to EOF; Invalid tokens included with error message

    /// Helper used by the parser to decide whether a `/` is division vs regex.
    static bool isRegexAllowedAfter(TokKind prev);

    static QString applySingleQuoteEscape(const QString &inner);  // addendum §7

private:
    Token nextToken();
    Token lexNumber();
    Token lexString(QChar quote);
    Token lexRegex();
    Token lexIdentifier();
    void skipWhitespace();
    bool atEnd() const { return m_pos >= m_src.size(); }
    QChar peek(int n = 0) const;

    QString m_src;
    int m_pos = 0;
    TokKind m_prevKind = TokKind::End;
};

}  // namespace Corbomite::Bases
```

- [ ] **Step 3: Lexer.cpp**

~250 lines. String lexing routes double-quoted through `QJsonDocument::fromJson` of a synthesised wrapper (`[\"...\"]` → array → element) to leverage JSON escape handling; single-quoted uses `applySingleQuoteEscape` (the §7 transform) then the same JSON path.

- [ ] **Step 4: Build + run tests — expect pass**

- [ ] **Step 5: Commit**

### Task 3.2 — AST node types

**Files:**
- Create: `libs/bases/include/corbomite/bases/Ast.h`
- Create: `libs/bases/src/Ast.cpp`

- [ ] **Step 1: Define AST classes**

```cpp
// libs/bases/include/corbomite/bases/Ast.h
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "ValuePtr.h"
#include <memory>
#include <QString>
#include <QVector>

namespace Corbomite::Bases {

class Expr;
using ExprPtr = std::unique_ptr<Expr>;

/// Abstract AST node. Evaluator visits via `accept` (or virtual eval
/// below — we use straight virtual dispatch for simplicity).
class Expr
{
public:
    virtual ~Expr() = default;
    virtual QString nodeName() const = 0;  // for debug + error paths
};

// Primitive literal: Null/Bool/Number/String/Regex.
class LiteralExpr : public Expr
{
public:
    explicit LiteralExpr(ValuePtr v) : value(std::move(v)) {}
    QString nodeName() const override { return QStringLiteral("Literal"); }
    ValuePtr value;
};

// Bare identifier (e.g. `status`, `note`, `file`, `formula`, `this`).
class IdentExpr : public Expr
{
public:
    explicit IdentExpr(QString n) : name(std::move(n)) {}
    QString nodeName() const override { return QStringLiteral("Ident"); }
    QString name;
};

// Array literal `[e1, e2, ...]`.
class ArrayExpr : public Expr
{
public:
    explicit ArrayExpr(QVector<ExprPtr> es) : elems(std::move(es)) {}
    QString nodeName() const override { return QStringLiteral("Array"); }
    QVector<ExprPtr> elems;
};

// Binary operator.
enum class BinOp { OrOr, AndAnd, Eq, Neq, Lt, Gt, LtEq, GtEq,
                   Add, Sub, Mul, Div, Mod };
class BinaryExpr : public Expr
{
public:
    BinaryExpr(BinOp op_, ExprPtr l, ExprPtr r)
        : op(op_), left(std::move(l)), right(std::move(r)) {}
    QString nodeName() const override { return QStringLiteral("Binary"); }
    BinOp op;
    ExprPtr left, right;
};

// Unary operator.
enum class UnOp { Not, Negate };
class UnaryExpr : public Expr
{
public:
    UnaryExpr(UnOp op_, ExprPtr e) : op(op_), operand(std::move(e)) {}
    QString nodeName() const override { return QStringLiteral("Unary"); }
    UnOp op;
    ExprPtr operand;
};

// `expr(args...)` — args[0] may be null for bare-call (no subject).
class CallExpr : public Expr
{
public:
    CallExpr(ExprPtr f, QVector<ExprPtr> a)
        : callee(std::move(f)), args(std::move(a)) {}
    QString nodeName() const override { return QStringLiteral("Call"); }
    ExprPtr callee;
    QVector<ExprPtr> args;
};

// `obj[key]`
class IndexExpr : public Expr
{
public:
    IndexExpr(ExprPtr o, ExprPtr i) : object(std::move(o)), index(std::move(i)) {}
    QString nodeName() const override { return QStringLiteral("Index"); }
    ExprPtr object, index;
};

// `obj.member`
class MemberExpr : public Expr
{
public:
    MemberExpr(ExprPtr o, QString m)
        : object(std::move(o)), member(std::move(m)) {}
    QString nodeName() const override { return QStringLiteral("Member"); }
    ExprPtr object;
    QString member;
};

// Parse-error sentinel (AST can still be handed to the evaluator which
// returns an Error value).
class InvalidExpr : public Expr
{
public:
    explicit InvalidExpr(QString msg) : message(std::move(msg)) {}
    QString nodeName() const override { return QStringLiteral("Invalid"); }
    QString message;
};

// Empty-formula sentinel — the addendum §1 `BK` class. Evaluates to null.
class EmptyExpr : public Expr
{
public:
    QString nodeName() const override { return QStringLiteral("Empty"); }
};

}  // namespace Corbomite::Bases
```

- [ ] **Step 2: Ast.cpp (empty — all inline)**

Just the SPDX + include. (Retained to give the library a compile unit if needed later.)

- [ ] **Step 3: Build — expect clean**

- [ ] **Step 4: Commit**

### Task 3.3 — Parser (Pratt / precedence-climbing)

**Files:**
- Create: `libs/bases/include/corbomite/bases/Parser.h`
- Create: `libs/bases/src/Parser.cpp`
- Create: `libs/bases/tests/tst_parser.cpp`

- [ ] **Step 1: Write failing tests first**

Tests cover:
- `"42"` → `LiteralExpr(42)`
- `"a + b * c"` → `Binary(Add, Ident(a), Binary(Mul, Ident(b), Ident(c)))` (precedence).
- `"(a + b) * c"` → `Binary(Mul, Binary(Add, ...), Ident(c))` (parens).
- `"a || b && c"` → `Binary(OrOr, Ident(a), Binary(AndAnd, Ident(b), Ident(c)))`.
- `"!a"` → `Unary(Not, Ident(a))`.
- `"-42"` → `Literal(NumberValue(-42))` (constant folding — addendum §4.4).
- `"-a"` → `Unary(Negate, Ident(a))` (no folding for non-literal).
- `"a.b.c"` → `Member(Member(Ident(a), "b"), "c")`.
- `"a[0]"` → `Index(Ident(a), Literal(0))`.
- `"f(1, 2)"` → `Call(Ident(f), [Lit(1), Lit(2)])`.
- `"a.b(1)"` → `Call(Member(Ident(a), "b"), [Lit(1)])`.
- `"[1, 2, 3]"` → `Array([...])`.
- `""` (empty input) → `EmptyExpr`.
- `"+++"` → root is `InvalidExpr`.
- Left-assoc: `"a - b - c"` → `Binary(Sub, Binary(Sub, a, b), c)`.

- [ ] **Step 2: Parser.h**

```cpp
// libs/bases/include/corbomite/bases/Parser.h
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "Ast.h"
#include "Lexer.h"

namespace Corbomite::Bases {

/// Pratt-style expression parser. Each precedence level maps to a numeric
/// binding power (bp). Invalid input yields an `InvalidExpr` whose
/// `message` explains the failure. Empty input yields `EmptyExpr`.
class Parser
{
public:
    explicit Parser(QVector<Token> tokens);

    ExprPtr parseProgram();  // may return Empty / Invalid at root

    /// One-shot convenience: lex + parse.
    static ExprPtr parse(const QString &source, QString *errorOut = nullptr);

private:
    ExprPtr parseExpression(int minBp);
    ExprPtr parsePrefix();
    ExprPtr parsePostfix(ExprPtr lhs);
    ExprPtr parsePrimary();
    ExprPtr parseArray();

    // Precedence binding powers (left,right). Higher = tighter.
    struct Bp { int left; int right; };
    static Bp infixBp(TokKind k);
    static int prefixBp(TokKind k);  // 0 if not unary

    const Token &peek(int k = 0) const;
    const Token &advance();
    bool match(TokKind k);
    bool check(TokKind k) const { return peek().kind == k; }
    ExprPtr invalid(const QString &msg);

    QVector<Token> m_toks;
    int m_pos = 0;
};

}  // namespace Corbomite::Bases
```

- [ ] **Step 3: Parser.cpp**

Binding-power table (matches addendum §3):

| Token | left | right | Kind |
|---|---|---|---|
| `\|\|` | 1 | 2 | infix (left-assoc) |
| `&&` | 3 | 4 | infix |
| `==`, `!=` | 5 | 6 | infix |
| `<`, `>`, `<=`, `>=` | 7 | 8 | infix |
| `+`, `-` | 9 | 10 | infix |
| `*`, `/`, `%` | 11 | 12 | infix |
| `!` (prefix) | - | 13 | prefix |
| `-` (prefix) | - | 13 | prefix |
| `(`, `[`, `.` (postfix) | 14 | - | postfix |

`parseExpression(minBp)`:
1. `lhs = parsePrefix()` (may be `parsePostfix(parsePrimary())` internally).
2. Loop: if `infixBp(peek()).left < minBp` → return lhs. Else consume, recurse with `infixBp.right`, build `BinaryExpr`.
3. Postfix loop — keeps binding at bp=14.

Constant-fold `UnaryExpr(Negate, LiteralExpr(NumberValue))` → `LiteralExpr(NumberValue(-x))` at build-time (addendum §4.4).

`parsePrimary`:
- `Null`/`True`/`False` → LiteralExpr.
- `Number`/`String`/`Regex` → LiteralExpr.
- `Identifier` → IdentExpr.
- `LParen Expression RParen` → wrapped.
- `LBracket` → `parseArray()`.
- Anything else → `invalid("expected primary")`.

`parsePostfix(lhs)` handles `.name`, `[expr]`, `(args)` (also trailing comma tolerance in args).

Errors accumulate into a single `InvalidExpr` at the top level. A follow-up can improve error messages; for MVP, "parse error at pos N: <context>" suffices.

- [ ] **Step 4: Build + run — expect pass**

Run: `cmake --build build -j 10 --target tst_bases_parser && ctest -R tst_bases_parser --output-on-failure`

- [ ] **Step 5: Commit**

### Task 3.4 — Phase 3 closeout

Update PROJECT-STATE.md → Phase 4.

---

## Phase 4 — Evaluator

**Phase goal:** An `ExprPtr` + an evaluation context produces a `ValuePtr`. Type-aware operator semantics (addendum §4) are implemented. Null propagation works. No function-registry dispatch yet — `CallExpr` on an unknown callee returns `FormulaErrorValue`.

**Phase dependencies:** Phases 1-3.

### Task 4.1 — EvalContext + identifier resolution interface

**Files:**
- Create: `libs/bases/include/corbomite/bases/EvalContext.h`
- Create: `libs/bases/src/EvalContext.cpp`

- [ ] **Step 1: Define EvalContext (abstract)**

```cpp
// libs/bases/include/corbomite/bases/EvalContext.h
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "ValuePtr.h"
#include <QString>
#include <QStringList>
#include <functional>

namespace Corbomite::Bases {

/// Runtime context passed to the evaluator. Abstract interface; concrete
/// implementations:
///   - `BasesEntry` (Phase 7) — the canonical "one row" context.
///   - `ShadowingContext` (Phase 5) — iteration-bound scope for
///     list.map/filter/reduce and object.map/filter bodies.
///   - `SummaryContext` (Phase 8) — binds `values` to the per-group values list.
class EvalContext
{
public:
    virtual ~EvalContext() = default;

    /// Resolve a bare identifier. Returns nullptr (not NullValue) if the
    /// name is unrecognised — the caller treats this as an error.
    virtual ValuePtr getByIdentifier(const QString &name) const = 0;

    /// Keys exposed for auto-complete. (Not used at evaluation time.)
    virtual QStringList keys() const { return {}; }
};

/// Non-virtual adapter for one-off evaluation contexts built from a lambda.
class LambdaContext : public EvalContext
{
public:
    using Fn = std::function<ValuePtr(const QString &)>;
    explicit LambdaContext(Fn f) : m_f(std::move(f)) {}
    ValuePtr getByIdentifier(const QString &name) const override { return m_f(name); }
private:
    Fn m_f;
};

}  // namespace Corbomite::Bases
```

- [ ] **Step 2: EvalContext.cpp** — empty compile unit.

- [ ] **Step 3: Commit**

### Task 4.2 — Evaluator core (no function registry yet)

**Files:**
- Create: `libs/bases/include/corbomite/bases/Evaluator.h`
- Create: `libs/bases/src/Evaluator.cpp`
- Create: `libs/bases/tests/tst_evaluator.cpp`

- [ ] **Step 1: Tests**

Cover every operator row of §4.3 arithmetic, §4.1 equality, §4.2 relational, §4.4 unary, §4.5 null propagation:
- `1 + 2` → 3.
- `"a" + "b"` → "ab".
- `"x" + 1` → "x1" (numeric-to-string coercion on mixed + ).
- `true && 1` → `BooleanValue(true)` (fresh, not 1 — addendum §3 note).
- `null == null` → true (via staticLooseEquals both null path).
- `null + 5` → NullValue (propagate).
- `!null` → NullValue (propagate).
- `!false` → BooleanValue(true).
- `-5` → NumberValue(-5) (literal-fold at parse).
- `date("2024-01-01") - date("2023-01-01")` → DurationValue ≈ 365 days. *(skip till Phase 5 when functions land; for Phase 4 use ValuePtr direct literal injection via a hand-built AST)*
- `DurationValue(1day) * 3` → DurationValue(3days). Build the test via a hand-built AST so we don't depend on Phase 5's function registry.
- `N * Du` throws (returns FormulaErrorValue wrapped in the call site).
- `D - D` valid (returns DurationValue from ms delta).
- `D + D` → FormulaErrorValue.

- [ ] **Step 2: Evaluator.h**

```cpp
// libs/bases/include/corbomite/bases/Evaluator.h
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "Ast.h"
#include "EvalContext.h"

namespace Corbomite::Bases {

class FunctionRegistry;  // Phase 5; forward-decl only

/// Walks an AST against a context + (optional) function registry and
/// produces a ValuePtr. Runtime errors are converted to FormulaErrorValue
/// at the enclosing call site and propagate from there.
class Evaluator
{
public:
    explicit Evaluator(FunctionRegistry *funcs = nullptr) : m_funcs(funcs) {}

    ValuePtr eval(const Expr &expr, const EvalContext &ctx) const;

private:
    ValuePtr evalBinary(const BinaryExpr &b, const EvalContext &ctx) const;
    ValuePtr evalUnary(const UnaryExpr &u, const EvalContext &ctx) const;
    ValuePtr evalCall(const CallExpr &c, const EvalContext &ctx) const;
    ValuePtr evalIndex(const IndexExpr &e, const EvalContext &ctx) const;
    ValuePtr evalMember(const MemberExpr &e, const EvalContext &ctx) const;

    // Operator dispatch (addendum §4.1-4.4)
    ValuePtr applyEq(const Value *l, const Value *r, bool invert) const;
    ValuePtr applyRelational(BinOp op, const Value *l, const Value *r) const;
    ValuePtr applyArithmetic(BinOp op, const Value *l, const Value *r) const;

    FunctionRegistry *m_funcs;
};

}  // namespace Corbomite::Bases
```

- [ ] **Step 3: Evaluator.cpp**

Key arithmetic-dispatch skeleton (~80 lines per arithmetic/relational):

```cpp
ValuePtr Evaluator::applyArithmetic(BinOp op, const Value *l, const Value *r) const
{
    // Null propagation first (addendum §4.5).
    if (dynamic_cast<const NullValue *>(l) || dynamic_cast<const NullValue *>(r))
        return NullValue::instance();

    auto *ln = dynamic_cast<const NumberValue *>(l);
    auto *rn = dynamic_cast<const NumberValue *>(r);
    if (ln && rn) {
        const double a = ln->data(), b = rn->data();
        switch (op) {
        case BinOp::Add: return std::make_shared<NumberValue>(a + b);
        case BinOp::Sub: return std::make_shared<NumberValue>(a - b);
        case BinOp::Mul: return std::make_shared<NumberValue>(a * b);
        case BinOp::Div: return std::make_shared<NumberValue>(a / b);
        case BinOp::Mod: return std::make_shared<NumberValue>(std::fmod(a, b));
        default: break;
        }
    }

    auto *ld = dynamic_cast<const DateValue *>(l);
    auto *rd = dynamic_cast<const DateValue *>(r);
    auto *ldu = dynamic_cast<const DurationValue *>(l);
    auto *rdu = dynamic_cast<const DurationValue *>(r);
    auto *ls = dynamic_cast<const StringValue *>(l);

    // Date +- String(coerce to Duration)
    if (ld && !rdu && r) {
        if (auto coerced = DurationValue::parseFromString(r->toString()))
            rdu = coerced.get();  // coerced lifetime extended via fall-through to next branch
    }
    // Date +- Duration
    if (ld && rdu && (op == BinOp::Add || op == BinOp::Sub))
        return rdu->addToDate(*ld, op == BinOp::Sub);

    // Duration * / Number  (not commutative)
    if (ldu && rn && (op == BinOp::Mul || op == BinOp::Div)) {
        const double n = rn->data();
        return std::make_shared<DurationValue>(ldu->timesScalar(op == BinOp::Div ? 1.0 / n : n));
    }

    // Duration +- Duration
    if (ldu && rdu) {
        if (op == BinOp::Add) return std::make_shared<DurationValue>(ldu->plus(rdu->components()));
        if (op == BinOp::Sub) return std::make_shared<DurationValue>(ldu->minus(rdu->components()));
    }

    // Date - Date → Duration.fromMilliseconds
    if (ld && rd && op == BinOp::Sub)
        return DurationValue::fromMilliseconds(
            ld->dateTime().toMSecsSinceEpoch() - rd->dateTime().toMSecsSinceEpoch());

    // List + List concat
    if (op == BinOp::Add) {
        auto *ll = dynamic_cast<const ListValue *>(l);
        auto *rl = dynamic_cast<const ListValue *>(r);
        if (ll && rl) return ll->concat(*rl);
    }

    // Fallback: + with any String coerces both to string.
    if (op == BinOp::Add) {
        if (ls || dynamic_cast<const StringValue *>(r)) {
            return std::make_shared<StringValue>(l->toString() + r->toString());
        }
    }

    return std::make_shared<FormulaErrorValue>(
        QStringLiteral("Invalid operator between %1 and %2")
            .arg(l ? l->type() : QStringLiteral("null"))
            .arg(r ? r->type() : QStringLiteral("null")));
}
```

Corresponding `applyRelational`, `applyEq`, and `Unary` (`!` propagates Null; `-` requires NumberValue).

`evalBinary` for `&&` / `||` short-circuits (addendum §3 note): `return BooleanValue(l.isTruthy() && r.isTruthy())` (always fresh BooleanValue, never the operand).

`evalCall`: if `m_funcs == nullptr`, return `FormulaErrorValue("no function registry")`. Else delegate — Phase 5 wires it up.

`evalMember(obj, "x")`: call `obj->objectAccess("x")`. If that returns null, return `NullValue::instance()` (propagation).

`evalIndex(obj, idx)`:
- ListValue + NumberValue → `list.get(int(idx))` with bounds check (out-of-range → NullValue).
- ObjectValue + StringValue → `obj.getInsensitive(idx.toString())`.
- Else → FormulaErrorValue.

- [ ] **Step 4: Build + run — expect pass**

- [ ] **Step 5: Commit**

### Task 4.3 — Phase 4 closeout

Update PROJECT-STATE.md → Phase 5.

---

## Phase 5 — Function registry + built-ins

**Phase goal:** `f(x, y)` and `x.method(y)` work. Global and per-type functions registered at static-init time mirror the addendum §8 catalog. Hard-cased functions (`if`, `list.map/filter/reduce`, `object.map/filter`) bypass the registry with their own dispatch that builds a `ShadowingContext`.

**Phase dependencies:** Phases 1-4.

### Task 5.1 — FunctionRegistry

**Files:** `include/corbomite/bases/FunctionRegistry.h`, `src/FunctionRegistry.cpp`, `tests/tst_function_registry.cpp`

- [ ] **Step 1: Define registry types**

```cpp
// libs/bases/include/corbomite/bases/FunctionRegistry.h
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "ValuePtr.h"
#include <QString>
#include <QVector>
#include <QHash>
#include <functional>
#include <typeindex>

namespace Corbomite::Bases {

class EvalContext;

/// Parameter descriptor (addendum §8.1).
struct FnParam {
    QString name;
    QVector<std::type_index> types;  // union of allowed Value subclasses (empty = any)
    bool optional = false;
    bool variadic = false;
    // customWidget field (addendum §8.1 `_W`) intentionally omitted for MVP.
};

/// A function callable from formulas. `apply` receives already-evaluated
/// argument ValuePtrs (or the subject receiver as args[0] for instance
/// functions — see registry design note below).
struct BasesFunction {
    QString name;
    QVector<FnParam> params;
    /// Semantics varies by registration site:
    /// - Global: `args` is the provided arg list.
    /// - Per-type: `args[0]` is the receiver; `args[1...]` the call args.
    std::function<ValuePtr(const EvalContext &, const QVector<ValuePtr> &)> apply;
    QString docString;
};

/// Registry used by Evaluator::evalCall.
class FunctionRegistry
{
public:
    void addGlobal(BasesFunction fn);
    void addForType(std::type_index valueClass, BasesFunction fn);

    /// Per-subject lookup (the subject is the evaluated receiver of `x.foo`).
    /// Walks `subject`'s class chain — exact class first, then parents — and
    /// returns the first match. Falls through to findGlobal if no subject
    /// is provided or no per-type function is found.
    const BasesFunction *findInstance(const Value *subject, const QString &name) const;
    const BasesFunction *findGlobal(const QString &name) const;

    /// Remove (for plugin teardown — Cluster N hook).
    void removeGlobal(const QString &name);
    void removeForType(std::type_index valueClass, const QString &name);

    /// Global singleton — built-ins register into this.
    static FunctionRegistry &global();

private:
    QHash<QString, BasesFunction> m_global;  // name lower-cased
    QHash<std::type_index, QHash<QString, BasesFunction>> m_byType;
};

/// Helpers to build FnParams concisely — addendum §8.1 naming.
FnParam requiredParam(QString name, QVector<std::type_index> types = {});
FnParam optionalParam(QString name, QVector<std::type_index> types = {});
FnParam variadicTail(QString name, QVector<std::type_index> types = {});

}  // namespace Corbomite::Bases
```

- [ ] **Step 2: Implement FunctionRegistry.cpp**

`findInstance` walks the `Value` subclass chain via recursive `dynamic_cast` probes — per Obsidian's audit, "the first class in the prototype chain that has the function." We encode this as a fixed list ordered from most-derived to most-base:

```cpp
// chain from a concrete subject to Value base
static const QVector<std::type_index> &classChainFor(const Value &subject)
{
    static QHash<QString, QVector<std::type_index>> table = {
        {"Null",     {typeid(NullValue), typeid(Value)}},
        {"Boolean",  {typeid(BooleanValue), typeid(Value)}},
        {"Number",   {typeid(NumberValue), typeid(Value)}},
        {"String",   {typeid(StringValue), typeid(Value)}},
        {"Tag",      {typeid(TagValue), typeid(StringValue), typeid(Value)}},
        {"Link",     {typeid(LinkValue), typeid(StringValue), typeid(Value)}},
        {"URL",      {typeid(UrlValue), typeid(StringValue), typeid(Value)}},
        {"Icon",     {typeid(IconValue), typeid(StringValue), typeid(Value)}},
        {"Image",    {typeid(ImageValue), typeid(StringValue), typeid(Value)}},
        {"HTML",     {typeid(HTMLValue), typeid(StringValue), typeid(Value)}},
        {"Markdown", {typeid(MarkdownValue), typeid(StringValue), typeid(Value)}},
        {"Date",     {typeid(DateValue), typeid(Value)}},
        {"Duration", {typeid(DurationValue), typeid(Value)}},
        {"List",     {typeid(ListValue), typeid(Value)}},
        {"Object",   {typeid(ObjectValue), typeid(Value)}},
        {"File",     {typeid(FileValue), typeid(Value)}},
        {"ThisFile", {typeid(ThisFileValue), typeid(FileValue), typeid(Value)}},
        {"Regex",    {typeid(RegExpValue), typeid(Value)}},
        {"Error",    {typeid(FormulaErrorValue), typeid(Value)}},
    };
    static QVector<std::type_index> empty{typeid(Value)};
    auto it = table.find(subject.type());
    return it != table.end() ? *it : empty;
}
```

- [ ] **Step 3: Run tests — expect pass**

Tests register a custom function and invoke via `findInstance` / `findGlobal`.

- [ ] **Step 4: Commit**

### Task 5.2 — Hook evaluator into FunctionRegistry

**Files:** `src/Evaluator.cpp` (extend), `tests/tst_evaluator_calls.cpp`

- [ ] **Step 1: Tests**

- Register a trivial `add(a, b)` global; evaluate `"add(1, 2)"` → NumberValue(3).
- Register a per-type `Number.times(n)`; evaluate `"5.times(3)"` → NumberValue(15).
- Unknown function → `FormulaErrorValue`.

- [ ] **Step 2: Implement evalCall with lookup + arg validation**

```cpp
ValuePtr Evaluator::evalCall(const CallExpr &c, const EvalContext &ctx) const
{
    // Hard-cased dispatch (addendum §5.2, §8): intercept these names BEFORE
    // arg evaluation so lambda-style bodies can evaluate inside a shadowed
    // context. See Task 5.3.
    if (auto *ident = dynamic_cast<IdentExpr *>(c.callee.get())) {
        if (ident->name.compare(QLatin1String("if"), Qt::CaseInsensitive) == 0)
            return evalIfSpecial(c, ctx);
    }
    if (auto *member = dynamic_cast<MemberExpr *>(c.callee.get())) {
        const QString lower = member->member.toLower();
        if (lower == QLatin1String("map") || lower == QLatin1String("filter")
            || lower == QLatin1String("reduce")) {
            return evalLambdaSpecial(member->object.get(), lower, c.args, ctx);
        }
    }

    // Regular dispatch.
    ValuePtr subject;
    QVector<ValuePtr> args;
    args.reserve(c.args.size() + 1);

    const BasesFunction *fn = nullptr;
    if (auto *member = dynamic_cast<MemberExpr *>(c.callee.get())) {
        subject = eval(*member->object, ctx);
        args.push_back(subject);
        if (m_funcs) fn = m_funcs->findInstance(subject.get(), member->member);
    } else if (auto *ident = dynamic_cast<IdentExpr *>(c.callee.get())) {
        if (m_funcs) fn = m_funcs->findGlobal(ident->name);
    }

    if (!fn)
        return std::make_shared<FormulaErrorValue>(
            i18n("Unknown function").toString());

    for (const auto &a : c.args) args.push_back(eval(*a, ctx));

    // Arity + type validation per addendum §8.1.
    if (auto err = validateArgs(fn->params, args, subject != nullptr))
        return err;

    return fn->apply(ctx, args);
}
```

- [ ] **Step 3: Run tests — expect pass**

- [ ] **Step 4: Commit**

### Task 5.3 — Hard-cased `if`, `list.map/filter/reduce`, `object.map/filter`

**Files:** `src/Evaluator.cpp` (extend), `tests/tst_evaluator_lambdas.cpp`

**Design note:** These six names bypass the function registry entirely (addendum §8) because the argument is evaluated as a **lambda body** in a shadowing scope that binds `value`/`index`/`acc`/`key` — the ordinary call-evaluator's eager arg evaluation would see `value` as an unresolved identifier.

- [ ] **Step 1: Tests**

- `list.filter(value > 3)` over `[1,2,3,4,5]` → `[4,5]`.
- `list.map(value * 2)` over `[1,2,3]` → `[2,4,6]`.
- `list.reduce(acc + value, 0)` over `[1,2,3]` → `6`.
- `object.map(value)` over `{a:1, b:2}` → `[1, 2]` (list, not object; addendum §8.8).
- `object.filter(key == 'a')` → `{a:1}`.
- `if(true, 1, 2)` → `1`. `if(false, 1, 2)` → `2`. `if(false, 1)` → `NullValue` (else defaults to null).

- [ ] **Step 2: Implement ShadowingContext + evalLambdaSpecial**

```cpp
class ShadowingContext : public EvalContext
{
public:
    ShadowingContext(const EvalContext &outer, QHash<QString, ValuePtr> binds)
        : m_outer(outer), m_binds(std::move(binds)) {}
    ValuePtr getByIdentifier(const QString &name) const override
    {
        auto it = m_binds.constFind(name);
        if (it != m_binds.constEnd()) return *it;
        return m_outer.getByIdentifier(name);
    }
private:
    const EvalContext &m_outer;
    QHash<QString, ValuePtr> m_binds;
};

ValuePtr Evaluator::evalLambdaSpecial(const Expr *subjectExpr,
                                      const QString &fn,
                                      const QVector<ExprPtr> &callArgs,
                                      const EvalContext &outer) const
{
    auto subject = eval(*subjectExpr, outer);
    if (auto *list = dynamic_cast<ListValue *>(subject.get())) {
        if (fn == QLatin1String("map")) {
            if (callArgs.size() != 1)
                return std::make_shared<FormulaErrorValue>(i18n("map(expr)").toString());
            QVector<ValuePtr> out;
            out.reserve(list->length());
            for (int i = 0; i < list->length(); ++i) {
                QHash<QString, ValuePtr> binds;
                binds[QStringLiteral("index")] = std::make_shared<NumberValue>(i);
                binds[QStringLiteral("value")] = list->get(i);
                ShadowingContext ctx(outer, binds);
                out.push_back(eval(*callArgs[0], ctx));
            }
            return std::make_shared<ListValue>(out);
        }
        if (fn == QLatin1String("filter")) { /* same shape; predicate → isTruthy */ }
        if (fn == QLatin1String("reduce")) { /* expr, acc0 */ }
    }
    if (auto *obj = dynamic_cast<ObjectValue *>(subject.get())) {
        // addendum §5.2 / §8.8
        if (fn == QLatin1String("map")) { /* produces ListValue */ }
        if (fn == QLatin1String("filter")) { /* produces ObjectValue */ }
    }
    return std::make_shared<FormulaErrorValue>(
        i18n("%1 is not callable on %2").subs(fn).subs(subject->type()).toString());
}
```

And `evalIfSpecial`:

```cpp
ValuePtr Evaluator::evalIfSpecial(const CallExpr &c, const EvalContext &ctx) const
{
    if (c.args.size() < 2 || c.args.size() > 3)
        return std::make_shared<FormulaErrorValue>(
            i18n("if takes 2 or 3 arguments").toString());
    auto cond = eval(*c.args[0], ctx);
    if (cond && cond->isTruthy()) return eval(*c.args[1], ctx);
    if (c.args.size() == 3) return eval(*c.args[2], ctx);
    return NullValue::instance();
}
```

- [ ] **Step 3: Run tests — expect pass**

- [ ] **Step 4: Commit**

### Task 5.4 — Global built-in functions

**Files:** `src/BuiltinGlobals.cpp`, `tests/tst_builtin_globals.cpp`

Register all 16 global functions from addendum §8.2: `now`, `today`, `date`, `random`, `min`, `max`, `list`, `link`, `number`, `duration`, `image`, `icon`, `file`, `html`, `escapeHTML`. (`if` is hard-cased in Task 5.3; not re-registered.)

- [ ] **Step 1: Tests**

One test per function covering the canonical inputs (addendum §8.2 signatures).

- [ ] **Step 2: BuiltinGlobals.cpp**

```cpp
// libs/bases/src/BuiltinGlobals.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/FunctionRegistry.h"
#include "corbomite/bases/Values.h"

namespace Corbomite::Bases {

void registerBuiltinGlobals(FunctionRegistry &r)
{
    using namespace std;
    r.addGlobal({
        QStringLiteral("now"), {},
        [](const EvalContext &, const QVector<ValuePtr> &) -> ValuePtr {
            return std::make_shared<DateValue>(QDateTime::currentDateTime(), true);
        },
        QStringLiteral("Current date + time.")});

    r.addGlobal({
        QStringLiteral("today"), {},
        [](const EvalContext &, const QVector<ValuePtr> &) -> ValuePtr {
            const auto today = QDate::currentDate();
            return std::make_shared<DateValue>(QDateTime(today, QTime(0, 0)), false);
        },
        QStringLiteral("Today (time zeroed).")});

    r.addGlobal({
        QStringLiteral("date"),
        {requiredParam(QStringLiteral("str"), {typeid(StringValue)})},
        [](const EvalContext &, const QVector<ValuePtr> &args) -> ValuePtr {
            auto d = DateValue::parseFromString(args[0]->toString());
            return d ? std::static_pointer_cast<Value>(d)
                     : std::make_shared<FormulaErrorValue>(i18n("invalid date").toString());
        },
        QStringLiteral("Parse a date literal.")});
    // ... 13 more. Each ~6 lines. Full file ~250 lines.
}

}  // namespace Corbomite::Bases
```

Called from a static initialiser or from `FunctionRegistry::global()`'s first-call construction hook.

- [ ] **Step 3 + 4: Run + commit**

### Task 5.5 — Per-type built-ins (String, Number, Date, List, Object, RegExp, Link, File)

**Files:** `src/BuiltinMethods.cpp` (one file, sectioned by receiver type), `tests/tst_builtin_methods.cpp`

Registrations per addendum §8.3–8.11. Full catalog:

- **String (13 methods):** `startsWith`, `endsWith`, `trim`, `title`, `isEmpty`, `replace`, `reverse`, `lower`, `split`, `contains`, `containsAny`, `containsAll`, `slice`, `repeat`.
- **Number (6):** `round`, `ceil`, `floor`, `abs`, `toFixed`, `isEmpty`.
- **Date (5):** `format`, `date`, `time`, `relative`, `isEmpty`.
- **List (19):** `earliest`, `latest`, `median`, `mean`, `max`, `min`, `sum`, `stddev`, `join`, `reverse`, `flat`, `unique`, `contains`, `containsAny`, `containsAll`, `slice`, `sort`, `isEmpty`. Map/filter/reduce hard-cased in Task 5.3; *not* re-registered.
- **Object (3 not-special):** `isEmpty`, `keys`, `values`. `map`, `filter` hard-cased.
- **Regex (1):** `matches`.
- **Link (2):** `asFile`, `linksTo`.
- **File (5):** `asLink`, `hasLink`, `inFolder`, `hasTag`, `hasProperty`.

~50 methods, ~500 LoC of registrations.

- [ ] **Step 1: Tests**

One per method. Fixture helper at top of test file builds a Vault + MetadataCache + FileValue for link/file tests.

- [ ] **Step 2: Implement BuiltinMethods.cpp**

~500 lines of ergonomic registrations. Pattern per method:

```cpp
r.addForType(typeid(StringValue), {
    QStringLiteral("startsWith"),
    {requiredParam(QStringLiteral("q"), {typeid(StringValue)})},
    [](const EvalContext &, const QVector<ValuePtr> &args) -> ValuePtr {
        return std::make_shared<BooleanValue>(
            args[0]->toString().startsWith(args[1]->toString()));
    }});
```

`Date::format` calls `Corbomite::MomentFormatter::format(dt, fmt)` already in libs/core.

- [ ] **Step 3 + 4: Run + commit**

### Task 5.6 — Formula wrapper (DK equivalent)

**Files:** `include/corbomite/bases/Formula.h`, `src/Formula.cpp`, `tests/tst_formula.cpp`

**Design note:** Top-level wrapper. Holds original source + parsed AST + error state. `.getValue(ctx)` returns ValuePtr; `.test(ctx)` returns bool via `isTruthy()`.

- [ ] **Step 1: Tests**

- `Formula f("1 + 1"); f.getValue(ctx)` == NumberValue(2).
- `Formula f("invalid+++"); f.isValid()` == false, `f.getValue(ctx)` == NullValue, `f.test(ctx)` == false.
- `Formula::toString()` round-trips.

- [ ] **Step 2: Formula.h**

```cpp
class Formula
{
public:
    Formula() = default;
    explicit Formula(const QString &source);

    const QString &source() const { return m_source; }
    QString toString() const { return m_source; }
    bool isValid() const { return !m_parseError.has_value(); }
    std::optional<QString> parseError() const { return m_parseError; }

    ValuePtr getValue(const EvalContext &ctx, FunctionRegistry *funcs = nullptr) const;
    bool test(const EvalContext &ctx, FunctionRegistry *funcs = nullptr) const;

private:
    QString m_source;
    ExprPtr m_ast;
    std::optional<QString> m_parseError;
};
```

- [ ] **Step 3 + 4: Run + commit**

### Task 5.7 — Phase 5 closeout

Update PROJECT-STATE.md → Phase 6.

---

## Phase 6 — `.base` YAML schema

**Phase goal:** A `.base` file round-trips through `BasesQuery::fromString` / `toString` preserving key order + unknown keys. `BasesViewConfig`, `FilterTree`, `PropertyConfig`, `PropertyId` parsing + serialisation work.

**Phase dependencies:** Phases 1-5 (formulas parse at load time; filter leaves hold `Formula`).

### Task 6.1 — PropertyId parser

**Files:** `include/corbomite/bases/PropertyId.h`, `src/PropertyId.cpp`, `tests/tst_propertyid.cpp`

- [ ] **Step 1: Tests**

- `parsePropertyId("note.status")` → `{Note, "status"}`.
- `parsePropertyId("file.name")` → `{File, "name"}`.
- `parsePropertyId("formula.priority")` → `{Formula, "priority"}`.
- `parsePropertyId("status")` → `{Note, "status"}` (unprefixed default).
- `buildPropertyId({Note, "status"})` → `"note.status"`.
- Round-trip.

- [ ] **Step 2: Implement**

```cpp
// libs/bases/include/corbomite/bases/PropertyId.h
enum class PropertyKind { Note, File, Formula };

struct PropertyId {
    PropertyKind kind = PropertyKind::Note;
    QString name;
    bool operator==(const PropertyId &o) const { return kind == o.kind && name == o.name; }
    QString toString() const;
};
uint qHash(const PropertyId &p, uint seed = 0);

PropertyId parsePropertyId(const QString &s);
QString buildPropertyId(const PropertyId &id);

/// Built-in `file.*` member set — the addendum §6 14 FILE_PROPERTIES.
extern const QStringList kFilePropertyMembers;
```

Implementation is ~30 lines.

- [ ] **Step 3 + 4: Run + commit**

### Task 6.2 — FilterTree

**Files:** `include/corbomite/bases/FilterTree.h`, `src/FilterTree.cpp`, `tests/tst_filtertree.cpp`

- [ ] **Step 1: Tests**

- String rule → `FilterRule` with `test(entry)` delegating to `Formula::test`.
- `{ and: [r1, r2] }` → AndFilter.
- Nested: `{ or: [{ and: [a, b] }, c] }` parses into correct tree.
- `optimize()` collapses single-child conjunctions into their child.
- Empty list → optimises to `None` (no-op / always-true?). Decision: empty `and` → true; empty `or` → false. Follow-up verify against Obsidian.
- `serialize()` round-trips.

- [ ] **Step 2: FilterTree.h**

```cpp
namespace Corbomite::Bases {

class BasesEntry;
class YamlValue;  // Markoff's — exposed via markoff-parser

class FilterNode
{
public:
    virtual ~FilterNode() = default;
    virtual bool test(const BasesEntry &entry, FunctionRegistry *funcs) const = 0;
    /// Produces an `ObjectValue`-shaped round-trip. Use YAML emitter in Phase 6.4.
    virtual QVariant serialize() const = 0;
};
using FilterPtr = std::shared_ptr<FilterNode>;

class FilterRule : public FilterNode {
public:
    explicit FilterRule(Formula rule) : m_rule(std::move(rule)) {}
    bool test(const BasesEntry &, FunctionRegistry *) const override;
    QVariant serialize() const override { return m_rule.source(); }
    const Formula &rule() const { return m_rule; }
private:
    Formula m_rule;
};

enum class Conj { And, Or, Not };
class FilterConjunction : public FilterNode {
public:
    FilterConjunction(Conj c, QVector<FilterPtr> children)
        : m_conj(c), m_children(std::move(children)) {}
    Conj conj() const { return m_conj; }
    const QVector<FilterPtr> &children() const { return m_children; }
    bool test(const BasesEntry &, FunctionRegistry *) const override;
    QVariant serialize() const override;
    FilterPtr optimize() const;
private:
    Conj m_conj;
    QVector<FilterPtr> m_children;
};

/// Parse a YAML node (Markoff::YamlValue) into a FilterPtr.
FilterPtr parseFilter(const Markoff::YamlValue &node);

}  // namespace Corbomite::Bases
```

`test()` for `FilterRule`: calls `rule.test(entry)`. `FilterConjunction::test`: short-circuit `all`/`any`/`!all` for And/Or/Not.

- [ ] **Step 3 + 4: Run + commit**

### Task 6.3 — PropertyConfig + BasesViewConfig

**Files:** `include/corbomite/bases/BasesViewConfig.h`, `src/BasesViewConfig.cpp`, `tests/tst_basesviewconfig.cpp`

- [ ] **Step 1: Tests**

Cover the audit §3 schema:
- Minimal view `{type: table, name: "All"}` round-trips.
- `order: [file.name, note.status]` parsed + serialised.
- `sort: [{property: note.due, direction: ASC}]` round-trips.
- `groupBy: {property: note.status, direction: DESC}`.
- `limit: 100`; `limit: 0` round-trips as "unlimited".
- `summaries: {note.amount: sum}`.
- `filters:` per-view AND-merged with global.
- **Invariant:** unknown keys at view level preserve round-trip (`unrecognizedData`).
- Direction strings are uppercase `ASC`/`DESC` only.

- [ ] **Step 2: BasesViewConfig.h**

```cpp
struct SortKey { PropertyId property; QString direction; };  // "ASC" | "DESC"
struct GroupBy { PropertyId property; QString direction; };

class BasesQuery;  // fwd

class BasesViewConfig
{
public:
    BasesViewConfig() = default;
    BasesViewConfig(QString type, QString name);

    QString type;       // "table" | plugin-registered
    QString name;       // unique within query.views
    FilterPtr filters;  // optional
    QVector<PropertyId> order;
    QVector<SortKey> sort;
    std::optional<GroupBy> groupBy;
    int limit = 0;      // 0 = unlimited
    QHash<PropertyId, QString> summaries;  // propId → summary-fn-name
    QVariantMap data;            // free-form view-type-specific options
    QVariantMap unrecognizedData;

    /// Load from a `views[]` entry. Returns nullptr on malformed shape;
    /// caller handles with i18n-error banner (see BasesView).
    static std::unique_ptr<BasesViewConfig> fromYaml(const Markoff::YamlValue &node,
                                                      QString *errorOut);

    /// Serialise into a QVariantMap suitable for rapidyaml emit.
    QVariantMap toMap() const;
};

/// Per-property settings (optional).
struct PropertyConfig {
    QString displayName;
    QVariantMap unrecognizedData;
};
```

`toMap()` emits keys in the order: `type`, `name`, `filters`, `order`, `sort`, `groupBy`, `limit`, `summaries`, `data`, plus any `unrecognizedData` merged in at the end.

- [ ] **Step 3 + 4: Run + commit**

### Task 6.4 — BasesQuery (top-level .base)

**Files:** `include/corbomite/bases/BasesQuery.h`, `src/BasesQuery.cpp`, `tests/tst_basesquery.cpp`

- [ ] **Step 1: Tests**

- Empty `.base` → `views.size() == 1` with default `{type:table, name: <localised "All">}`. (Matches audit §3 "Empty file → default 1-view 'Table'".)
- Non-object YAML root → `parseError` populated.
- Legacy `display: {note.x: Status}` migrates to `properties.note.x.displayName == "Status"` on parse.
- Round-trip: parse → `toString()` → parse — equal maps.
- `formulas: {priority: "if(note.urgent, 1, 2)"}` parses each formula string into `Formula`.
- Unknown top-level keys preserved under `unrecognizedData`.

- [ ] **Step 2: BasesQuery.h**

```cpp
class BasesQuery
{
public:
    QVector<std::unique_ptr<BasesViewConfig>> views;
    FilterPtr filters;                        // global
    QHash<QString, Formula> formulas;
    QHash<QString, Formula> summaryFormulas;
    QHash<PropertyId, PropertyConfig> properties;
    std::optional<QString> newItemFolder;
    std::optional<QString> newItemTemplate;
    QVariantMap unrecognizedData;

    // Attached post-load by BasesView.
    QString filePath;

    /// Parse YAML body. Empty input returns a default 1-view "Table" query.
    /// On parse error, returns a BasesQuery containing a single default view
    /// and populates `parseError`.
    static std::unique_ptr<BasesQuery> fromString(const QString &text,
                                                   QString *parseError);

    QString toString() const;   // stringify via rapidyaml with preserved ordering
    std::unique_ptr<BasesQuery> clone() const;

    BasesViewConfig *getViewConfig(const QString &name = QString{}) const;
};
```

`fromString` uses `Markoff::YamlValue::parse`. Serialisation writes through a tree-aware builder that invokes `YamlValue::setString`/`setInt`/`setSeqNode`/`setMap` in the canonical key order. Unknown keys come last.

`clone()` implemented as `fromString(toString())`. Good enough for MVP.

- [ ] **Step 3 + 4: Run + commit**

### Task 6.5 — Phase 6 closeout

Update PROJECT-STATE.md → Phase 7. Commit.

---

## Phase 7 — BasesEntry + BasesQueryResult + QueryController

**Phase goal:** Wire the DSL into vault data. `BasesEntry` is a per-note view of frontmatter + implicit `file`; `QueryController` owns the live result set and recomputes it reactively against `MetadataCache` signals.

**Phase dependencies:** Phases 1-6, plus existing `Corbomite::Vault`, `Corbomite::MetadataCache`.

### Task 7.1 — BasesEntry

**Files:** `include/corbomite/bases/BasesEntry.h`, `src/BasesEntry.cpp`, `tests/tst_basesentry.cpp`

- [ ] **Step 1: Tests**

- Construct an entry from a `TFile` + `MetadataCache`; `getByIdentifier("file")` returns FileValue; `getByIdentifier("note")` returns ObjectValue; `getByIdentifier("this")` returns ThisFileValue.
- `getValue("file.name")` returns StringValue matching file.basename.
- `getValue("note.status")` returns the frontmatter value.
- `getValue("formula.priority")` evaluates the query-level formula.
- Cycle detection: two formulas referring to each other → FormulaErrorValue with cycle message.
- Memoisation: same formula evaluated twice returns same ValuePtr (identity-equal).

- [ ] **Step 2: BasesEntry.h**

```cpp
class BasesEntry : public EvalContext
{
public:
    BasesEntry(Vault *vault, MetadataCache *cache,
               TFile *file, TFile *localFile,
               const BasesQuery &query,
               FunctionRegistry *funcs);

    TFile *file() const { return m_file; }
    TFile *localFile() const { return m_local; }  // for `this.*`
    const QJsonObject &frontmatter() const;        // live alias into MetadataCache
    QStringList getPropertyKeys() const;           // raw frontmatter keys

    /// Identifier dispatch (EvalContext override). Resolves `this`, `note`,
    /// `file`, `formula`, otherwise case-insensitive frontmatter lookup.
    ValuePtr getByIdentifier(const QString &name) const override;
    QStringList keys() const override;

    /// PropertyId-keyed accessor (dispatches by kind).
    ValuePtr getValue(const PropertyId &id) const;

    /// Evaluate a named formula under this entry, memoised; detects cycles.
    ValuePtr formulaValue(const QString &name) const;

    static const QStringList &filePropertyIds();  // 14 "file.<member>" names

private:
    Vault *m_vault;
    MetadataCache *m_cache;
    TFile *m_file;
    TFile *m_local;
    const BasesQuery &m_query;
    FunctionRegistry *m_funcs;

    mutable std::shared_ptr<FileValue> m_implicitFile;
    mutable std::shared_ptr<ObjectValue> m_note;  // lazy ObjectValue::fromFrontMatter
    mutable QHash<QString, ValuePtr> m_formulaCache;
    mutable QSet<QString> m_inProgressFormulas;  // cycle sentinel
};
```

- [ ] **Step 3: Implement**

```cpp
ValuePtr BasesEntry::getByIdentifier(const QString &name) const
{
    const QString lower = name.toLower();
    if (lower == QLatin1String("this")) {
        // ThisFileValue forwards objectAccess through this entry.
        return std::make_shared<ThisFileValue>(m_local, m_cache,
            [this](const QString &n) { return getByIdentifier(n); });
    }
    if (lower == QLatin1String("note")) return noteObject();
    if (lower == QLatin1String("file")) return implicitFile();
    if (lower == QLatin1String("formula")) {
        // Return a small adapter whose objectAccess resolves to formulaValue.
        return std::make_shared<LambdaObjectValue>(
            [this](const QString &formulaName) { return formulaValue(formulaName); });
    }
    // Default: frontmatter property, case-insensitive.
    return noteObject()->getInsensitive(name);
}

ValuePtr BasesEntry::formulaValue(const QString &name) const
{
    if (m_formulaCache.contains(name)) return m_formulaCache.value(name);
    if (m_inProgressFormulas.contains(name))
        return std::make_shared<FormulaErrorValue>(
            i18n("formula '%1' has a cycle").subs(name).toString());
    auto it = m_query.formulas.constFind(name);
    if (it == m_query.formulas.constEnd())
        return NullValue::instance();
    m_inProgressFormulas.insert(name);
    auto v = it->getValue(*this, m_funcs);
    m_inProgressFormulas.remove(name);
    m_formulaCache.insert(name, v);
    return v;
}
```

(`LambdaObjectValue` is a small helper class that implements `objectAccess` through a closure. ~15 LoC in `Values.h`.)

- [ ] **Step 4 + 5: Build + run + commit**

### Task 7.2 — BasesQueryResult + sort + group + limit

**Files:** `include/corbomite/bases/BasesQueryResult.h`, `src/BasesQueryResult.cpp`, `tests/tst_basesqueryresult.cpp`

- [ ] **Step 1: Tests**

- Build result from 5 entries + SortKey; verify order.
- Multi-key sort (tie on primary → break by secondary).
- Nulls last.
- `applyLimit(3)` leaves first 3.
- Grouping by property yields `BasesEntryGroup`s; null-keyed group goes last.
- `getSummaryValue(group, propId, "sum")` aggregates numeric column.

- [ ] **Step 2: BasesQueryResult.h**

```cpp
struct BasesEntryGroup
{
    QVector<std::shared_ptr<BasesEntry>> entries;
    ValuePtr key;  // may be null
    bool hasKey() const;
};

class BasesQueryResult
{
public:
    BasesQueryResult(const BasesViewConfig &cfg,
                     QVector<std::shared_ptr<BasesEntry>> entries,
                     FunctionRegistry *funcs);

    const QVector<std::shared_ptr<BasesEntry>> &rows() const { return m_rows; }

    /// Lazy (cached after first call).
    const QVector<BasesEntryGroup> &groups() const;

    /// Union of configured order + note.* keys seen.
    const QVector<PropertyId> &properties() const;

    ValuePtr summaryValue(int groupIndex, const PropertyId &prop,
                          const QString &summaryFn) const;

private:
    void applySort();
    void applyLimit();

    const BasesViewConfig &m_cfg;
    QVector<std::shared_ptr<BasesEntry>> m_rows;
    FunctionRegistry *m_funcs;

    mutable std::optional<QVector<BasesEntryGroup>> m_groups;
    mutable std::optional<QVector<PropertyId>> m_props;
    mutable QHash<int, QHash<PropertyId, QHash<QString, ValuePtr>>> m_summaryCache;
};
```

`applySort` per-pair comparator dispatches on Value type: Number/Date/Duration numeric; Boolean by truthiness; else locale-aware `QString::localeAwareCompare(a.toString(), b.toString())`. Multi-key recurses to next key on tie.

`summaryValue(group, prop, fnName)` builds a `ListValue` of that column's values across `group.entries`, then evaluates the summary formula from §9's default table (or `query.summaryFormulas` for custom) against a `SummaryContext` binding `values → list`.

- [ ] **Step 3 + 4 + 5: Run + commit**

### Task 7.3 — Default summary formulas

**Files:** `src/BuiltinSummaries.cpp`, `tests/tst_builtin_summaries.cpp`

15 entries from addendum §9. Each is registered as a named `Formula` string parsed once at `FunctionRegistry::global` construction.

- [ ] **Step 1: Tests**

One per summary: `Average` on `[1,2,3]` → 2.0; `Sum` → 6; `Median` → 2; `Range` (Number variant) = max-min = 2; `Checked` on `[true, false, true]` → 2; `Unique` on `['a','a','b']` → 2; etc.

- [ ] **Step 2: BuiltinSummaries.cpp**

```cpp
void registerDefaultSummaries(QHash<QString, Formula> &out)
{
    out.insert(QStringLiteral("Average"),  Formula(QStringLiteral("values.mean().round(2)")));
    out.insert(QStringLiteral("Min"),      Formula(QStringLiteral("values.min()")));
    // ... 13 more from addendum §9
}
```

Called from `BasesQuery::fromString` during initial summaryFormulas population if unspecified.

- [ ] **Step 3 + 4: Run + commit**

### Task 7.4 — QueryController

**Files:** `include/corbomite/bases/QueryController.h`, `src/QueryController.cpp`, `tests/tst_querycontroller.cpp`

- [ ] **Step 1: Tests**

- Create controller over a fixture vault; on construction, result set contains exactly the notes matching global+view filter.
- `MetadataCache::cacheChanged(path, ...)` signal fires → controller debounces 50ms → emits `resultsChanged` with new set.
- `MetadataCache::cacheDeleted(path)` removes the row.
- `setCurrentFile(TFile*)` rebuilds `QueryContext.local`; emits `resultsChanged` if any formula references `this.*`.

- [ ] **Step 2: QueryController.h**

```cpp
class QueryController : public QObject
{
    Q_OBJECT

public:
    QueryController(Vault *vault, MetadataCache *cache,
                    FunctionRegistry *funcs, QObject *parent = nullptr);

    void setQuery(std::shared_ptr<BasesQuery> query);
    void setViewConfig(BasesViewConfig *cfg);  // one of query->views
    void setCurrentFile(TFile *local);
    void setSearchQuery(const QString &q);

    const BasesQueryResult *result() const { return m_result.get(); }

Q_SIGNALS:
    void resultsChanged();

private Q_SLOTS:
    void onCacheChanged(const QString &path);
    void onCacheDeleted(const QString &path);
    void onDebouncedRecompute();

private:
    void scheduleRecompute();
    void recomputeNow();

    Vault *m_vault;
    MetadataCache *m_cache;
    FunctionRegistry *m_funcs;

    std::shared_ptr<BasesQuery> m_query;
    BasesViewConfig *m_cfg = nullptr;
    TFile *m_local = nullptr;
    QString m_searchQuery;

    std::unique_ptr<BasesQueryResult> m_result;
    QTimer *m_recomputeTimer;  // 50ms debounce
};
```

Constructor connects `m_cache->cacheChanged` and `m_cache->cacheDeleted` to `scheduleRecompute`. `recomputeNow`:

1. Collect all markdown `TFile*`s from `m_vault->getMarkdownFiles()`.
2. Build one `BasesEntry` per file.
3. Filter: apply `(m_query->filters && m_cfg->filters)` — short-circuit via FilterConjunction logic.
4. Apply `m_searchQuery` by string-matching against every visible property's `toString()`.
5. Construct `BasesQueryResult(*m_cfg, entries, m_funcs)` — this does sort+limit eagerly.
6. Emit `resultsChanged`.

(Full rescan is the MVP strategy. Addendum Open-Q3 notes Obsidian's strategy is unconfirmed. This simple strategy is correct; optimisation is a follow-up.)

- [ ] **Step 3 + 4 + 5: Run + commit**

### Task 7.5 — Phase 7 closeout

Update PROJECT-STATE.md → Phase 8. Commit.

---

## Phase 8 — BasesView + Table layout

**Phase goal:** A real Qt widget shows a table of vault notes per the `.base` config. Column headers show display names. Sort-by-click header. Inline-edit frontmatter on cell commit. Filter/sort/group/limit already apply because `QueryController` owns them.

**Phase dependencies:** Phases 1-7.

### Task 8.1 — BasesTableModel (QAbstractTableModel)

**Files:** `include/corbomite/bases/BasesTableModel.h`, `src/BasesTableModel.cpp`, `tests/tst_basestable_model.cpp`

- [ ] **Step 1: Tests**

- `rowCount` matches result row count (accounting for group headers if grouping enabled).
- `columnCount` matches ordered property count.
- `data(DisplayRole)` returns cell `Value::toString()`.
- `headerData` returns property display names.
- On `QueryController::resultsChanged`, model resets (captured via `QSignalSpy` on `modelReset`).
- Inline commit: `setData(index, newValue)` writes through to `FileManager::processFrontMatter`.

- [ ] **Step 2: BasesTableModel.h**

```cpp
class BasesTableModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    BasesTableModel(QueryController *controller,
                    FileManager *fileManager,
                    QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    bool setData(const QModelIndex &index, const QVariant &value,
                 int role = Qt::EditRole) override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QVariant headerData(int section, Qt::Orientation o,
                        int role = Qt::DisplayRole) const override;

    /// Raw Value pointer for the cell — used by the delegate for type
    /// dispatch + rendering.
    ValuePtr valueAt(const QModelIndex &index) const;

    /// PropertyId for the column — used by the delegate for edit routing.
    PropertyId propertyAt(int column) const;

private Q_SLOTS:
    void onResultsChanged();

private:
    QueryController *m_controller;
    FileManager *m_fm;
    QVector<PropertyId> m_columns;
};
```

- [ ] **Step 3: Implement**

`data(DisplayRole)` returns `valueAt(index)->toString()`. `data(Qt::UserRole)` returns the `Value::type()` string (used by delegate for paint-dispatch).

`setData` on `EditRole`: reads `propertyAt(col).kind` — only `PropertyKind::Note` is editable (Formula, File are read-only); call `m_fm->processFrontMatter(file, [&](QVariantMap &fm){ fm[key] = value; })`. MetadataCache will re-emit `cacheChanged` → QueryController recomputes → model resets.

`flags`: `Qt::ItemIsEditable` only for note.* columns.

`onResultsChanged` calls `beginResetModel()` / `endResetModel()`.

- [ ] **Step 4 + 5: Run + commit**

### Task 8.2 — BasesCellDelegate (QStyledItemDelegate)

**Files:** `include/corbomite/bases/BasesCellDelegate.h`, `src/BasesCellDelegate.cpp`, `tests/tst_basescelldelegate.cpp`

**Design note:** MVP uses simple Qt built-in editors dispatched by `Value::type()` — `QLineEdit` for String/Number/Link/Url/Tag, `QCheckBox` for Boolean, `QDateEdit`/`QDateTimeEdit` for Date, plain text for Duration/List/Object/Regex/Image/Icon/HTML/Markdown. Rich per-type widgets (audit's `metadataTypeManager.registeredTypeWidgets`) are a follow-up.

- [ ] **Step 1: Tests**

- Boolean cell: `createEditor` returns a `QCheckBox`; commit round-trips.
- Date cell: `createEditor` returns a `QDateEdit` when `hasTime()==false`, `QDateTimeEdit` when true.
- String cell: `QLineEdit`.
- Number cell: `QDoubleSpinBox`.
- `paint` dispatch: paint doesn't crash on any Value::type() (smoke test).
- Error cell: renders with "error" style + tooltip set to message.

- [ ] **Step 2: BasesCellDelegate.h**

```cpp
class BasesCellDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit BasesCellDelegate(QObject *parent = nullptr);

    QWidget *createEditor(QWidget *parent,
                          const QStyleOptionViewItem &option,
                          const QModelIndex &index) const override;

    void setEditorData(QWidget *editor, const QModelIndex &index) const override;
    void setModelData(QWidget *editor, QAbstractItemModel *model,
                      const QModelIndex &index) const override;

    void paint(QPainter *p, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;
};
```

- [ ] **Step 3: Implement**

`createEditor` switches on `index.data(Qt::UserRole).toString()`:

```cpp
const QString type = index.data(Qt::UserRole).toString();
if (type == QLatin1String("Boolean")) {
    auto *cb = new QCheckBox(parent); cb->setTristate(false);
    return cb;
}
if (type == QLatin1String("Number")) {
    auto *sb = new QDoubleSpinBox(parent);
    sb->setDecimals(6); sb->setRange(-1e15, 1e15); return sb;
}
if (type == QLatin1String("Date")) {
    // Delegate between QDateEdit and QDateTimeEdit per Value::hasTime().
    // Pull the underlying ValuePtr from the model's UserRole+1.
    // ...
}
// Default:
return new QLineEdit(parent);
```

`paint`: look up `Value::type()` from UserRole. For "Error", paint a small warning pill. For "Tag", paint with a `<chip>`-style rounded background. For "Link", paint underlined + blue. For "Image" / "Icon" / "HTML" / "Markdown", paint placeholder text `"[image]"` / `"[html]"` / etc. (rich rendering is a follow-up).

- [ ] **Step 4 + 5: Run + commit**

### Task 8.3 — BasesView (TextFileView subclass)

**Files:** `include/corbomite/bases/BasesView.h`, `src/BasesView.cpp`, `tests/tst_basesview.cpp`

- [ ] **Step 1: Tests**

- Fixture: create a temp `.base` file in a fixture vault. Instantiate `BasesView`. `setViewData(fileBytes, false)` → `view->query()` non-null, first view visible.
- Dirty flag: `setData` via model triggers MetadataCache change → no-op save-loop (note saves don't touch the `.base` file).
- Save: modify view config via `setSortKey(propId, Asc)` → `getViewData()` returns YAML with the sort applied → `TextFileView::save` persists.
- Error: malformed YAML → banner shown.

- [ ] **Step 2: BasesView.h**

```cpp
class BasesView : public TextFileView
{
    Q_OBJECT
public:
    BasesView(WorkspaceLeaf *leaf, QWidget *parent = nullptr);
    ~BasesView() override;

    void setServices(Vault *vault, MetadataCache *cache,
                     FileManager *fileManager, FunctionRegistry *funcs);

    QString getViewData() const override;
    void setViewData(const QString &data, bool clear) override;
    void clear() override;
    bool canAcceptExtension(const QString &ext) const { return ext == QLatin1String("base"); }

    std::shared_ptr<BasesQuery> query() const { return m_query; }
    BasesViewConfig *activeView() const { return m_activeView; }
    void setActiveView(const QString &name);

    /// Header-click sort.
    void onHeaderClicked(int column);

private:
    void rebuildLayout();

    Vault *m_vault = nullptr;
    MetadataCache *m_cache = nullptr;
    FileManager *m_fm = nullptr;
    FunctionRegistry *m_funcs = nullptr;

    std::shared_ptr<BasesQuery> m_query;
    BasesViewConfig *m_activeView = nullptr;

    std::unique_ptr<QueryController> m_controller;
    std::unique_ptr<BasesTableModel> m_model;
    QTableView *m_table = nullptr;
    BasesCellDelegate *m_delegate = nullptr;

    QLabel *m_errorBanner = nullptr;
    QToolBar *m_toolbar = nullptr;   // Phase 8.4 basic
};
```

- [ ] **Step 3: Implement**

`setViewData(text, clear)`:
1. Parse via `BasesQuery::fromString`.
2. If parseError → show banner, keep `m_query` at default.
3. Else → m_query = new query, activate `views[0]`, rebuild controller + model + table.

`getViewData()` calls `m_query->toString()`.

`rebuildLayout`:
```cpp
m_controller = std::make_unique<QueryController>(m_vault, m_cache, m_funcs, this);
m_controller->setQuery(m_query);
m_controller->setViewConfig(m_activeView);

m_model = std::make_unique<BasesTableModel>(m_controller.get(), m_fm, this);
m_table->setModel(m_model.get());
m_table->setItemDelegate(m_delegate);
```

`onHeaderClicked(col)` calls `m_activeView->setSortProperty(prop, toggle)`, reblasts through the controller, updates the table.

Any config mutation (sort toggle, limit change, search text) calls `requestSave()` (TextFileView debounce 2s).

- [ ] **Step 4 + 5: Run + commit**

### Task 8.4 — Basic toolbar (search + sort-indicator + view switcher)

**Files:** `include/corbomite/bases/BasesToolbar.h`, `src/BasesToolbar.cpp`, `tests/tst_basestoolbar.cpp`

MVP toolbar only: a `QLineEdit` search field (routes to `QueryController::setSearchQuery`), a `QComboBox` to switch among `views`, a button to add/remove columns. Properties/sort/group/views menus are follow-ups.

- [ ] **Step 1: Tests**

- Typing into search → `QueryController::setSearchQuery` called with the string.
- Selecting a different view in combo → `BasesView::setActiveView(name)` called.

- [ ] **Step 2-4: Implement + test + commit**

### Task 8.5 — Phase 8 closeout

Update PROJECT-STATE.md → Phase 9.

---

## Phase 9 — Bases plugin + closeout

**Phase goal:** `.base` files open in `BasesView` automatically. Feature ships as an InternalPlugin at `src/plugins/bases/`. Cluster closed; retro written; PROJECT-STATE marks K done.

**Phase dependencies:** Phases 1-8.

### Task 9.1 — BasesPlugin (src/plugins/bases)

**Files:**
- Create: `src/plugins/bases/CMakeLists.txt`
- Create: `src/plugins/bases/metadata.json.in`
- Create: `src/plugins/bases/BasesPlugin.h`
- Create: `src/plugins/bases/BasesPlugin.cpp`
- Create: `src/plugins/bases/tests/` (smoke test for plugin load/unload)
- Modify: `src/CMakeLists.txt` (add subdirectory)

- [ ] **Step 1: metadata.json.in**

```json
{
    "KPlugin": {
        "Name": "Bases",
        "Description": "Database-style table views over vault notes (.base files)",
        "Icon": "view-list-details",
        "Version": "1.0",
        "License": "GPL-3.0-or-later",
        "Category": "Core",
        "EnabledByDefault": true,
        "Authors": [{"Name": "Corbomite Developers"}]
    },
    "X-Corbomite-Trusted": @X_CORBOMITE_TRUSTED@,
    "X-Corbomite-Permissions": ["vault.read", "vault.write", "vault.events", "metadata.read"],
    "X-Corbomite-MinVersion": "0.1.0"
}
```

(No `X-Corbomite-DockArea` — Bases is main-area only.)

- [ ] **Step 2: BasesPlugin.h / .cpp**

```cpp
// src/plugins/bases/BasesPlugin.h
#pragma once
#include "corbomite/vault/Plugin.h"

namespace Corbomite {
class BasesPlugin : public Plugin
{
    Q_OBJECT
public:
    BasesPlugin(QObject *parent = nullptr, const QVariantList & = {});
    ~BasesPlugin() override;

    void onLoad(PluginContext *ctx) override;
    void onUnload() override;

private:
    Corbomite::Bases::FunctionRegistry *m_funcs = nullptr;
};
}  // namespace Corbomite
```

`onLoad`:
1. Resolve `Vault`/`MetadataCache`/`FileManager` from context via proxies. (Plugin is `X-Corbomite-Trusted=true` so it can reach the raw services — but still goes through the proxies.)
2. Build a `FunctionRegistry` (`global()` singleton reference is fine — the registry is process-wide). Register Phase 5 built-ins.
3. Register the "bases" view type through `ViewRegistrar`:
   ```cpp
   views->registerView(QStringLiteral("bases"),
       [this](WorkspaceLeaf *leaf) -> View * {
           auto *v = new Bases::BasesView(leaf);
           v->setServices(m_vault, m_cache, m_fm, m_funcs);
           return v;
       });
   views->registerExtensions({QStringLiteral("base")}, QStringLiteral("bases"));
   ```

`onUnload` — unregister.

- [ ] **Step 3: CMakeLists.txt**

```cmake
corbomite_add_plugin(corbomite-bases-plugin
    METADATA_TEMPLATE "${CMAKE_CURRENT_SOURCE_DIR}/metadata.json.in"
    SOURCES BasesPlugin.cpp
    TRUSTED
    LINK_LIBRARIES
        Qt6::Widgets KF6::I18n
        Corbomite::Core Corbomite::Storage Corbomite::Vault Corbomite::Bases)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
```

- [ ] **Step 4: Smoke test**

Write `tests/tst_basesplugin.cpp` that constructs `BasesPlugin`, loads it with a fake `PluginContext`, verifies the "bases" type + "base" extension registered.

- [ ] **Step 5: Run full build + e2e smoke**

Run `cmake --build build -j 10 && cd build && ctest --output-on-failure -j 10`

Expected: full suite green outside the known-flaky `tst_benchmark_layout`.

Additionally, run the app with a fixture vault + a `.base` file (create `testvaults/starter-vault/PKM LM/Test Base.base` with `{views: [{type: table, name: All}]}`) and confirm:
- Double-clicking the `.base` file opens a `BasesView` tab.
- The table populates with all vault notes.
- Header click sorts; save is persisted.

- [ ] **Step 6: Commit**

### Task 9.2 — Retro + PROJECT-STATE closeout

**Files:**
- Create: `docs/cluster-retros/cluster-k.md`
- Modify: `docs/PROJECT-STATE.md`
- Modify: `docs/superpowers/plans/INDEX.md`
- Rename: `docs/superpowers/plans/2026-04-14-cluster-k-bases-SCOUTING.md` → the scouting doc can be left in place with a pointer to this plan, *or* deleted since this plan supersedes. **Choice:** delete (per Ritual 3).

- [ ] **Step 1: Retro**

Write `cluster-retros/cluster-k.md` following the Cluster J / Cluster N template. Key sections:
- What shipped: full Value hierarchy + Pratt parser + Evaluator + function registry + .base YAML + BasesEntry/Result/Controller + BasesView + BasesPlugin.
- What was deferred (see top of this plan).
- Commit count (count on completion).
- Deviations from the plan (document inline per Ritual 3).

- [ ] **Step 2: Update PROJECT-STATE.md**

- Roadmap row: K `Done` with retro pointer.
- Current focus: "Cluster K closed. Next user-selected."
- Move in-flight row out; add "Previous: **Cluster K closed ...** " paragraph at top of "Last updated:" line.

- [ ] **Step 3: Update INDEX.md**

Swap K's plan from SCOUTING to the full-plan filename. Add a "Done" row.

- [ ] **Step 4: Delete old SCOUTING doc**

```bash
git rm docs/superpowers/plans/2026-04-14-cluster-k-bases-SCOUTING.md
```

- [ ] **Step 5: Commit**

```bash
git add docs/cluster-retros/cluster-k.md docs/PROJECT-STATE.md \
        docs/superpowers/plans/INDEX.md
git commit -m "$(cat <<'EOF'
docs(cluster-k): closeout — Bases ships

Full Value hierarchy + hand-rolled Pratt parser + typed evaluator
+ function registry + .base YAML round-trip + BasesView with
inline-edit frontmatter writeback. Shipped as an InternalPlugin at
src/plugins/bases/ (KPluginFactory + permission-gated proxies).

Retro at cluster-retros/cluster-k.md.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Definition of Done (cluster-level)

- [ ] All 9 phases committed with Ritual 2 status bumps.
- [ ] `.base` files in a fixture vault open in `BasesView`, render a sortable+editable table of notes, and round-trip through YAML without loss (key order + unknown keys preserved).
- [ ] Formula engine handles the canonical samples from addendum §8 (all global functions + all per-type methods + hard-cased `if`/map/filter/reduce) with test coverage per Task ≥ 80% of the named functions.
- [ ] Inline-edit in a cell writes through `FileManager::processFrontMatter` and the MetadataCache round-trip (re-fire of `cacheChanged`) keeps the table stable (no save-loop).
- [ ] Filter + sort + group + limit applied correctly per audit §8 invariants.
- [ ] Default summary formulas (addendum §9) work with per-group caching.
- [ ] Bases InternalPlugin loads at app startup (after vault open) and registers the `"bases"` view type + `"base"` extension.
- [ ] Retro + PROJECT-STATE closeout landed.

---

## Blocks / enables

**Blocks:**
- Cluster O (Advanced query layer — post-parity): K's BasesEntry/QueryController shape + function-registry hook give O a concrete surface to extend with graph-aware filters.
- Cluster N follow-ups: plugin-facing `registerGlobalFunc` / `registerInstanceFunc` wrappers are now meaningful — the FunctionRegistry exists.

**Enables:**
- User-facing database view — largest single missing feature per GAP-ANALYSIS.md.
- Embed-in-markdown of `.base` files (Cluster J follow-up) — infrastructure for main-area rendering is in place.
- Future rich cell rendering (follow-up) can layer on Cluster J's `EmbedRenderer` once per-type renderers become Value-native.

---

## Preserved-compat quirks (from addendum §14)

- `duration` shorthand accepts `ms` / `millisecond` / `milliseconds` (undocumented).
- `number(bool)` returns 0/1.
- `date.timestamp` objectAccess member (undocumented).
- `!NullValue` → NullValue, not `true` (diverges from JS `!null`).
- `D + D` throws (only `D - D`, `D + Du`, `D - Du` valid).
- `N * Du` throws (duration must be on the left).
- `ObjectValue.objectAccess` case-insensitive.
- `NullValue` is a singleton — never construct directly.
- Sort puts nulls last.
- `views[0]` is the default when `getViewConfig(undefined)`.
- `limit: 0` means "unlimited" (not null, not absent, not negative).
- Empty `.base` is valid (yields default 1-view "Table").

---

## Self-review checklist

- [ ] Every Phase maps to a Ritual 2 commit + Phase-N-of-9 update in PROJECT-STATE.md.
- [ ] Every task produces at least one test-before-code step (TDD).
- [ ] No "TBD" / "implement later" / "similar to above" placeholders.
- [ ] Class/method names consistent across phases (e.g. `BasesEntry::formulaValue`, not `getFormulaValue` in one place and `formulaVal` in another).
- [ ] Follow-ups are explicitly enumerated at plan top, not hidden inside phases.
- [ ] Addendum §14 quirks are preserved (call out in the "Preserved-compat quirks" section).
