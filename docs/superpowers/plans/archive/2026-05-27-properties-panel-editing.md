# Properties Panel Full Editing — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the Properties sidebar panel a complete frontmatter editor — add (typed), edit, delete, rename, and drag-reorder properties — persisting key order to YAML and never corrupting value shapes the editors can't represent.

**Architecture:** A new order-authoritative `FileManager::setFrontMatter(TFile*, QList<FrontMatterEntry>)` rebuilds frontmatter in caller order, copying complex (nested-map / list-of-maps) values verbatim via a new `YamlValue::setChildFrom` primitive. `PropertiesView` is reworked from a `QFormLayout` into an ordered list of `PropertyRow` widgets (grip + key + editor + delete) and is the single source of truth for order; every interaction funnels through one debounced wholesale write.

**Tech Stack:** C++20, Qt6 Widgets, KDE Frameworks 6, ryml (via `Markoff::YamlValue`), QtTest. Markoff parser lives in the submodule `libs/markoff-family/libs/markoff-parser`.

**Spec:** [`docs/superpowers/specs/2026-05-27-properties-panel-editing-design.md`](../specs/2026-05-27-properties-panel-editing-design.md)

**Build/test commands (Corbomite):**
- Configure: `cmake --preset dev`
- Build all: `cmake --build --preset dev -j 10`
- Build one target: `cmake --build --preset dev -j 10 --target <target>`
- Run a test binary: `./build-dev/bin/<binary> [testSlot]`
- Full suite: `cd build-dev && ctest --output-on-failure -j 10`

**Markoff build/test (submodule, separate tree):**
- Configure: `cmake -S libs/markoff-family -B libs/markoff-family/build-dev -DCMAKE_EXPORT_COMPILE_COMMANDS=ON`
- Build: `cmake --build libs/markoff-family/build-dev -j 10`
- Tests: `cd libs/markoff-family && scripts/run-tests.sh -R yaml` (offscreen by default)

---

## File Structure

| File | Responsibility |
|---|---|
| `libs/markoff-family/.../include/markoff/parser/YamlValue.h` | declare `setChildFrom` |
| `libs/markoff-family/.../src/YamlValue.cpp` | implement `setChildFrom` via ryml `duplicate` |
| `libs/markoff-family/.../tests/tst_yamlvalue*.cpp` | `setChildFrom` unit tests |
| `libs/vault/include/corbomite/vault/FileManager.h` | `FrontMatterEntry` struct + `setFrontMatter` decl |
| `libs/vault/src/FileManager.cpp` | `setFrontMatter` impl |
| `libs/vault/include/corbomite/vault/proxies/FileManagerProxy.h` | proxy decl |
| `libs/vault/src/proxies/FileManagerProxy.cpp` | proxy impl (`vault.write`-gated) |
| `libs/vault/tests/tst_setfrontmatter.cpp` | new vault-layer test |
| `libs/vault/tests/CMakeLists.txt` | register `tst_setfrontmatter` |
| `src/sidebar/PropertyRow.{h,cpp}` | **new** row widget (grip/key/editor/delete) + pure `isEditableFrontmatterValue` helper |
| `src/plugins/properties/PropertiesView.{h,cpp}` | rework to `PropertyRow` list; add/delete/rename/reorder; write via `setFrontMatter` |
| `src/sidebar/CMakeLists.txt` | add `PropertyRow` sources to the sidebar target |
| `src/plugins/properties/tests/tst_properties_plugin.cpp` | extend with editing tests |

---

## Phase 0 — Markoff prerequisite: `YamlValue::setChildFrom`

> Cross-repo (CONTRIBUTING-OPS Ritual 5): land in Markoff first, push, then bump the Corbomite submodule pin. Markoff submodule is currently detached at `082b063`; check out the tip of `master` before working (Markoff master has advanced past `v0.7.0-freeze`).

### Task 0: `YamlValue::setChildFrom` in the Markoff submodule

**Files:**
- Modify: `libs/markoff-family/libs/markoff-parser/include/markoff/parser/YamlValue.h` (mutation section)
- Modify: `libs/markoff-family/libs/markoff-parser/src/YamlValue.cpp`
- Test: a `YamlValue` test in `libs/markoff-family/libs/markoff-parser/tests/` (find the existing one with `grep -rl "YamlValue::parse\|emptyMap" libs/markoff-family/libs/markoff-parser/tests/`)

- [ ] **Step 1: Prepare the Markoff working branch**

```bash
cd libs/markoff-family
git fetch origin
git checkout master
git pull --ff-only
cd ../..
```

Expected: submodule now on `master` tip (no longer detached).

- [ ] **Step 2: Write the failing test**

Add to the existing YamlValue test class (a new private slot). It proves verbatim copy of a nested map and order-append:

```cpp
void testSetChildFromCopiesNestedMapVerbatim()
{
    using namespace Markoff;
    QString err;
    YamlValue src = YamlValue::parse(QStringLiteral(
        "outer:\n  inner: value\n  count: 3\n  nested:\n    deep: yes\n"), &err);
    QVERIFY(err.isEmpty());

    YamlValue dst = YamlValue::emptyMap();
    dst.setString(QStringLiteral("first"), QStringLiteral("a"));
    dst.setChildFrom(QStringLiteral("outer"), src.get(QStringLiteral("outer")));

    // Verbatim: the copied subtree stringifies identically to the source's.
    QCOMPARE(dst.get(QStringLiteral("outer")).stringify(),
             src.get(QStringLiteral("outer")).stringify());
    // Appended last → order is first, then outer.
    QCOMPARE(dst.keys(), QStringList({QStringLiteral("first"), QStringLiteral("outer")}));
}

void testSetChildFromReplacesExistingKey()
{
    using namespace Markoff;
    YamlValue dst = YamlValue::emptyMap();
    dst.setString(QStringLiteral("k"), QStringLiteral("old"));
    YamlValue src = YamlValue::parse(QStringLiteral("k:\n  - x\n  - y\n"));
    dst.setChildFrom(QStringLiteral("k"), src.get(QStringLiteral("k")));
    QCOMPARE(dst.keys(), QStringList({QStringLiteral("k")}));
    QVERIFY(dst.get(QStringLiteral("k")).isSeq());
    QCOMPARE(dst.get(QStringLiteral("k")).size(), 2);
}
```

- [ ] **Step 3: Run the test to verify it fails**

```bash
cd libs/markoff-family
cmake --build build-dev -j 10 2>&1 | tail -5
```

Expected: **compile error** — `setChildFrom` is not a member of `YamlValue`. (That counts as red.)

- [ ] **Step 4: Declare the method**

In `YamlValue.h`, in the `// --- Mutation` block, after `void remove(const QString &key);`:

```cpp
    /// Deep-copy `src`'s subtree (any kind, nested arbitrarily) into this map
    /// under `key`, appended as the last child so a fresh build preserves call
    /// order. Replaces `key` if it already exists. Byte-faithful, unlike the
    /// scalar setters. `this` is coerced to a map.
    void setChildFrom(const QString &key, const YamlValue &src);
```

- [ ] **Step 5: Implement the method**

In `YamlValue.cpp`, after the `YamlValue::remove` implementation, add (uses the existing file-local helpers `qstringToCsubstr`, `ensureMap`, and the ryml `Tree::duplicate(const Tree*, id_type, id_type, id_type)` overload):

```cpp
void YamlValue::setChildFrom(const QString &key, const YamlValue &src)
{
    ensureMap(d->tree, d->nodeId);
    if (!src.d || !src.d->tree || src.d->nodeId == ryml::NONE) {
        setNull(key);
        return;
    }
    QByteArray keyUtf8 = key.toUtf8();
    ryml::csubstr keyView = d->tree->copy_to_arena(qstringToCsubstr(keyUtf8));

    // Replace any existing key so position is controlled by call order.
    ryml::id_type existing = d->tree->find_child(d->nodeId, keyView);
    if (existing != ryml::NONE)
        d->tree->remove(existing);

    // Append a verbatim deep copy of src as the last child, then stamp the key.
    ryml::id_type after = d->tree->last_child(d->nodeId);  // NONE if empty → first
    ryml::id_type dup =
        d->tree->duplicate(src.d->tree.get(), src.d->nodeId, d->nodeId, after);
    d->tree->set_key(dup, keyView);
}
```

- [ ] **Step 6: Build and run the test to verify it passes**

```bash
cd libs/markoff-family
cmake --build build-dev -j 10 2>&1 | tail -3
scripts/run-tests.sh -R yaml 2>&1 | tail -15
```

Expected: the two new slots PASS; no regressions in the YamlValue suite.

- [ ] **Step 7: Commit + push in Markoff**

```bash
cd libs/markoff-family
git add libs/markoff-parser/include/markoff/parser/YamlValue.h \
        libs/markoff-parser/src/YamlValue.cpp \
        libs/markoff-parser/tests/
git commit -m "feat(parser): YamlValue::setChildFrom — verbatim node deep-copy

Backed by ryml Tree::duplicate. Copies an arbitrary subtree (scalar/seq/
map, nested) into a map under a given key, appended last so fresh builds
preserve insertion order. Needed by Corbomite's order-authoritative
front-matter setter to preserve complex values across reorders.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
git push origin master
cd ../..
```

- [ ] **Step 8: Bump the Corbomite submodule pin + commit**

```bash
git add libs/markoff-family
git commit -m "chore(submodule): bump Markoff for YamlValue::setChildFrom

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

Expected: `git diff --cached --submodule` (before commit) shows the new Markoff commit as the pin.

---

## Phase 1 — Vault: `FileManager::setFrontMatter` + proxy

### Task 1: `FrontMatterEntry` + `FileManager::setFrontMatter`

**Files:**
- Modify: `libs/vault/include/corbomite/vault/FileManager.h`
- Modify: `libs/vault/src/FileManager.cpp`
- Test: `libs/vault/tests/tst_setfrontmatter.cpp` (create)
- Modify: `libs/vault/tests/CMakeLists.txt`

- [ ] **Step 1: Write the failing test**

Create `libs/vault/tests/tst_setfrontmatter.cpp`. Mirror the harness of a sibling vault test (open one, e.g. `tst_processfrontmatter.cpp`, for the exact include set + how it constructs `Vault`/`FileManager` and writes a file). The test below assumes the same `FileSystemAdapter` + `QTemporaryDir` + `Vault::load` + `FileManager` setup:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTemporaryDir>
#include <QTest>

#include "corbomite/storage/FileSystemAdapter.h"
#include "corbomite/storage/LinkResolver.h"
#include "corbomite/storage/MetadataCache.h"
#include "corbomite/vault/FileManager.h"
#include "corbomite/vault/Vault.h"
#include "corbomite/vault/TFile.h"

#include <markoff/parser/Document.h>
#include <markoff/parser/YamlValue.h>

using namespace Corbomite;

class TestSetFrontMatter : public QObject
{
    Q_OBJECT

    // Helper: write a note, return its TFile*.
    TFile *seed(Vault &v, const QString &rel, const QByteArray &body)
    {
        v.write(rel, body);
        return v.getFileByPath(rel);
    }
    // Helper: read a note's raw bytes back.
    QString readBack(Vault &v, const QString &rel) { return QString::fromUtf8(v.read(rel)); }

private slots:
    void writesEntriesInGivenOrder();
    void omittedKeyIsDeleted();
    void emptyListStripsFrontmatter();
    void preserveFromDiskKeepsNestedMapVerbatim();
    void renameShapedChangeRoundTrips();
    void nonMarkdownReturnsFalse();
};

void TestSetFrontMatter::writesEntriesInGivenOrder()
{
    FileSystemAdapter fs; QTemporaryDir dir; Vault v(&fs); v.load(dir.path());
    LinkResolver r; MetadataCache c(r); FileManager fm(&v, &c);
    TFile *f = seed(v, QStringLiteral("n.md"),
                    "---\nalpha: 1\nbeta: 2\n---\nbody\n");

    QList<FileManager::FrontMatterEntry> entries{
        {QStringLiteral("beta"),  QVariant::fromValue<qlonglong>(2), false},
        {QStringLiteral("gamma"), QStringLiteral("g"),               false},
        {QStringLiteral("alpha"), QVariant::fromValue<qlonglong>(1), false},
    };
    QVERIFY(fm.setFrontMatter(f, entries));

    auto doc = Markoff::Document::fromMarkdown(readBack(v, QStringLiteral("n.md")));
    QCOMPARE(doc->parsedFrontmatter().keys(),
             QStringList({QStringLiteral("beta"), QStringLiteral("gamma"),
                          QStringLiteral("alpha")}));
}

void TestSetFrontMatter::omittedKeyIsDeleted()
{
    FileSystemAdapter fs; QTemporaryDir dir; Vault v(&fs); v.load(dir.path());
    LinkResolver r; MetadataCache c(r); FileManager fm(&v, &c);
    TFile *f = seed(v, QStringLiteral("n.md"), "---\nkeep: 1\ndrop: 2\n---\nx\n");

    QVERIFY(fm.setFrontMatter(f, {{QStringLiteral("keep"),
                                   QVariant::fromValue<qlonglong>(1), false}}));
    auto doc = Markoff::Document::fromMarkdown(readBack(v, QStringLiteral("n.md")));
    QCOMPARE(doc->parsedFrontmatter().keys(), QStringList({QStringLiteral("keep")}));
}

void TestSetFrontMatter::emptyListStripsFrontmatter()
{
    FileSystemAdapter fs; QTemporaryDir dir; Vault v(&fs); v.load(dir.path());
    LinkResolver r; MetadataCache c(r); FileManager fm(&v, &c);
    TFile *f = seed(v, QStringLiteral("n.md"), "---\nk: 1\n---\nbody\n");

    QVERIFY(fm.setFrontMatter(f, {}));
    const QString out = readBack(v, QStringLiteral("n.md"));
    QVERIFY(!out.startsWith(QStringLiteral("---")));   // fence removed
    QVERIFY(out.contains(QStringLiteral("body")));
}

void TestSetFrontMatter::preserveFromDiskKeepsNestedMapVerbatim()
{
    FileSystemAdapter fs; QTemporaryDir dir; Vault v(&fs); v.load(dir.path());
    LinkResolver r; MetadataCache c(r); FileManager fm(&v, &c);
    TFile *f = seed(v, QStringLiteral("n.md"),
        "---\nscalar: old\nmeta:\n  a: 1\n  b: two\n---\nbody\n");

    // Edit the scalar; preserve the nested map verbatim.
    QList<FileManager::FrontMatterEntry> entries{
        {QStringLiteral("scalar"), QStringLiteral("new"), false},
        {QStringLiteral("meta"),   QVariant{},            true},   // preserveFromDisk
    };
    QVERIFY(fm.setFrontMatter(f, entries));

    auto doc = Markoff::Document::fromMarkdown(readBack(v, QStringLiteral("n.md")));
    Markoff::YamlValue fmv = doc->parsedFrontmatter();
    QCOMPARE(fmv.get(QStringLiteral("scalar")).asString(), QStringLiteral("new"));
    // The map survived as a map (NOT flattened to a string), with its children.
    QVERIFY(fmv.get(QStringLiteral("meta")).isMap());
    QCOMPARE(fmv.get(QStringLiteral("meta")).get(QStringLiteral("a")).asInt(), 1);
    QCOMPARE(fmv.get(QStringLiteral("meta")).get(QStringLiteral("b")).asString(),
             QStringLiteral("two"));
}

void TestSetFrontMatter::renameShapedChangeRoundTrips()
{
    FileSystemAdapter fs; QTemporaryDir dir; Vault v(&fs); v.load(dir.path());
    LinkResolver r; MetadataCache c(r); FileManager fm(&v, &c);
    TFile *f = seed(v, QStringLiteral("n.md"), "---\nold: v\nother: 9\n---\nb\n");

    // Rename `old`→`new` at same index = drop old, emit new first.
    QList<FileManager::FrontMatterEntry> entries{
        {QStringLiteral("new"),   QStringLiteral("v"),               false},
        {QStringLiteral("other"), QVariant::fromValue<qlonglong>(9), false},
    };
    QVERIFY(fm.setFrontMatter(f, entries));
    auto doc = Markoff::Document::fromMarkdown(readBack(v, QStringLiteral("n.md")));
    QCOMPARE(doc->parsedFrontmatter().keys(),
             QStringList({QStringLiteral("new"), QStringLiteral("other")}));
    QCOMPARE(doc->parsedFrontmatter().get(QStringLiteral("new")).asString(),
             QStringLiteral("v"));
}

void TestSetFrontMatter::nonMarkdownReturnsFalse()
{
    FileSystemAdapter fs; QTemporaryDir dir; Vault v(&fs); v.load(dir.path());
    LinkResolver r; MetadataCache c(r); FileManager fm(&v, &c);
    QVERIFY(!fm.setFrontMatter(nullptr, {}));
}

QTEST_MAIN(TestSetFrontMatter)
#include "tst_setfrontmatter.moc"
```

> If `Vault`'s seed/read API differs from `write`/`read`/`getFileByPath`, copy the exact calls from `tst_processfrontmatter.cpp` — do not invent method names.

- [ ] **Step 2: Register the test in CMake**

In `libs/vault/tests/CMakeLists.txt`, copy the block used by an existing test (e.g. `tst_processfrontmatter`) and adapt names. It will look like:

```cmake
add_executable(tst_setfrontmatter tst_setfrontmatter.cpp)
target_link_libraries(tst_setfrontmatter PRIVATE
    Corbomite::Vault Corbomite::Storage Markoff::Parser Qt6::Test)
add_test(NAME tst_setfrontmatter COMMAND tst_setfrontmatter)
```

> Match the exact target/link names used by neighbouring tests in that file.

- [ ] **Step 3: Run to verify it fails**

```bash
cmake --build --preset dev -j 10 --target tst_setfrontmatter 2>&1 | tail -5
```

Expected: **compile error** — no `FrontMatterEntry` / `setFrontMatter` member.

- [ ] **Step 4: Declare the struct + method**

In `FileManager.h`, near the existing `FrontMatterMutator` typedef / `processFrontMatter` declaration:

```cpp
    /// One ordered front-matter entry for setFrontMatter.
    /// When preserveFromDisk is true, the key's value is copied verbatim from
    /// the existing on-disk YAML (value is ignored) — used for complex shapes
    /// the editors can't round-trip.
    struct FrontMatterEntry {
        QString  key;
        QVariant value;
        bool     preserveFromDisk = false;
    };

    /// Rewrite a note's front-matter wholesale, in the given order. Keys present
    /// on disk but absent from `ordered` are deleted. An empty list strips the
    /// front-matter block. Returns false for null/non-.md files.
    bool setFrontMatter(TFile *f, const QList<FrontMatterEntry> &ordered);
```

Ensure `#include <QList>` / `#include <QVariant>` are present in the header (add if missing).

- [ ] **Step 5: Implement `setFrontMatter`**

In `FileManager.cpp`, after `processFrontMatter`. It reuses the same `m_vault->process` + parse + strip-empty pattern, but builds a fresh ordered map and routes preserve entries through `setChildFrom`:

```cpp
bool FileManager::setFrontMatter(TFile *f, const QList<FrontMatterEntry> &ordered)
{
    if (!f || !m_vault) return false;
    if (f->extension != QStringLiteral("md")) return false;

    return m_vault->process(f, [&](const QByteArray &cur) -> QByteArray {
        auto doc = Markoff::Document::fromMarkdown(QString::fromUtf8(cur));
        if (!doc) return cur;

        Markoff::YamlValue current = doc->parsedFrontmatter();
        Markoff::YamlValue working = current.isNull()
            ? Markoff::YamlValue::emptyMap()
            : current.clone();

        Markoff::YamlValue next = Markoff::YamlValue::emptyMap();
        for (const FrontMatterEntry &e : ordered) {
            if (e.preserveFromDisk) {
                if (working.contains(e.key))
                    next.setChildFrom(e.key, working.get(e.key));
                else
                    next.setNull(e.key);
                continue;
            }
            const QVariant &val = e.value;
            switch (val.typeId()) {
            case QMetaType::Bool:
                next.setBool(e.key, val.toBool()); break;
            case QMetaType::Int:
            case QMetaType::LongLong:
            case QMetaType::UInt:
            case QMetaType::ULongLong:
                next.setInt(e.key, val.toLongLong()); break;
            case QMetaType::Double:
            case QMetaType::Float:
                next.setDouble(e.key, val.toDouble()); break;
            case QMetaType::QStringList:
                next.setSeq(e.key, val.toStringList()); break;
            case QMetaType::QString:
                next.setString(e.key, val.toString()); break;
            default:
                if (!val.isValid() || val.isNull()) next.setNull(e.key);
                else next.setString(e.key, val.toString());
                break;
            }
        }

        // Empty → strip the fence (same convention as processFrontMatter).
        const Markoff::YamlValue out =
            ordered.isEmpty() ? Markoff::YamlValue() : next;
        return doc->withFrontmatter(out).toUtf8();
    });
}
```

- [ ] **Step 6: Run to verify it passes**

```bash
cmake --build --preset dev -j 10 --target tst_setfrontmatter 2>&1 | tail -3
./build-dev/bin/tst_setfrontmatter 2>&1 | tail -12
```

Expected: all six slots PASS.

- [ ] **Step 7: Commit**

```bash
git add libs/vault/include/corbomite/vault/FileManager.h \
        libs/vault/src/FileManager.cpp \
        libs/vault/tests/tst_setfrontmatter.cpp \
        libs/vault/tests/CMakeLists.txt
git commit -m "feat(vault): FileManager::setFrontMatter — order-authoritative write

Wholesale ordered front-matter rewrite: caller order is authoritative,
omitted keys are deleted, empty strips the fence, and preserveFromDisk
entries copy complex values verbatim via YamlValue::setChildFrom.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

### Task 2: `FileManagerProxy::setFrontMatter`

**Files:**
- Modify: `libs/vault/include/corbomite/vault/proxies/FileManagerProxy.h`
- Modify: `libs/vault/src/proxies/FileManagerProxy.cpp`

- [ ] **Step 1: Declare on the proxy**

In `FileManagerProxy.h`, beside `processFrontMatter`:

```cpp
    bool setFrontMatter(TFile *f, const QList<FileManager::FrontMatterEntry> &ordered);
```

- [ ] **Step 2: Implement the gated forward**

In `FileManagerProxy.cpp`, after `processFrontMatter`, mirroring its `canWrite()`/`logDenied` pattern:

```cpp
bool FileManagerProxy::setFrontMatter(TFile *f,
                                      const QList<FileManager::FrontMatterEntry> &ordered)
{
    if (!canWrite()) {
        logDenied("setFrontMatter", "vault.write");
        return false;
    }
    return m_fm && m_fm->setFrontMatter(f, ordered);
}
```

- [ ] **Step 3: Build the vault library to verify it compiles**

```bash
cmake --build --preset dev -j 10 --target Corbomite_Vault 2>&1 | tail -5
```

> If the vault target name differs, find it with `grep -rn "add_library" libs/vault/CMakeLists.txt`.
Expected: clean build.

- [ ] **Step 4: Commit**

```bash
git add libs/vault/include/corbomite/vault/proxies/FileManagerProxy.h \
        libs/vault/src/proxies/FileManagerProxy.cpp
git commit -m "feat(vault): FileManagerProxy::setFrontMatter (vault.write-gated)

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

## Phase 2 — `PropertyRow` widget + value classification

### Task 3: pure `isEditableFrontmatterValue` helper

**Files:**
- Create: `src/sidebar/PropertyRow.h`, `src/sidebar/PropertyRow.cpp`
- Modify: `src/sidebar/CMakeLists.txt`
- Test: add a slot to `src/plugins/properties/tests/tst_properties_plugin.cpp`

> Build the free classification function first (pure, no widgets) so the read-only/preserve decision is unit-tested without a metadata cache.

- [ ] **Step 1: Write the failing test**

Add to `tst_properties_plugin.cpp` (declare the slot in the class + include `"../../../sidebar/PropertyRow.h"` — adjust the relative path to reach `src/sidebar/`):

```cpp
void TestPropertiesPlugin::classifiesComplexValuesAsNonEditable()
{
    using namespace Corbomite;
    using YV = Markoff::YamlValue;
    QString e;
    YV scalars = YV::parse(QStringLiteral(
        "s: text\nn: 3\nb: true\nlist:\n  - a\n  - b\n"), &e);
    QVERIFY(isEditableFrontmatterValue(scalars.get(QStringLiteral("s"))));
    QVERIFY(isEditableFrontmatterValue(scalars.get(QStringLiteral("n"))));
    QVERIFY(isEditableFrontmatterValue(scalars.get(QStringLiteral("b"))));
    QVERIFY(isEditableFrontmatterValue(scalars.get(QStringLiteral("list")))); // scalar list

    YV complex = YV::parse(QStringLiteral(
        "m:\n  k: v\nlistofmaps:\n  - x: 1\n"), &e);
    QVERIFY(!isEditableFrontmatterValue(complex.get(QStringLiteral("m"))));          // map
    QVERIFY(!isEditableFrontmatterValue(complex.get(QStringLiteral("listofmaps")))); // seq of maps
}
```

Add `#include <markoff/parser/YamlValue.h>` to the test if not present.

- [ ] **Step 2: Run to verify it fails**

```bash
cmake --build --preset dev -j 10 --target tst_properties_plugin 2>&1 | tail -5
```

Expected: compile error — `isEditableFrontmatterValue` / `PropertyRow.h` not found.

- [ ] **Step 3: Create `PropertyRow.h` with the helper + the widget shell**

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "corbomite/models/PropertyType.h"

#include <markoff/parser/YamlValue.h>

#include <QWidget>

class QLabel;
class QLineEdit;
class QStackedWidget;
class QToolButton;

namespace Corbomite {

class PropertyEditorWidget;

/// True if a frontmatter value can be losslessly round-tripped by the
/// per-type editors (scalars + flat string lists). Maps and lists containing
/// non-scalar elements return false → such rows are read-only + preserved.
bool isEditableFrontmatterValue(const Markoff::YamlValue &value);

/// One row in the Properties panel: [grip] [key] [editor|summary] [delete].
/// Editable rows host a PropertyEditorWidget; read-only rows show a greyed
/// summary and carry preserveFromDisk so the view writes the on-disk value
/// verbatim.
class PropertyRow : public QWidget
{
    Q_OBJECT
public:
    PropertyRow(const QString &key,
                PropertyType type,
                const Markoff::YamlValue &value,
                bool editable,
                QWidget *parent = nullptr);

    QString key() const { return m_key; }
    PropertyType type() const { return m_type; }
    bool isReadOnly() const { return !m_editable; }
    bool preserveFromDisk() const { return !m_editable; }

    /// Current editor value (editable rows only; read-only rows return Null).
    Markoff::YamlValue currentValue() const;

Q_SIGNALS:
    void valueChanged();
    void deleteRequested();
    void keyRenameRequested(const QString &oldKey, const QString &newKey);
    void reorderRequested(int fromVisualY);  // emitted on drag-drop; view maps to index

private:
    void beginInlineRename();
    void commitInlineRename();

    QString m_key;
    PropertyType m_type;
    bool m_editable;

    QToolButton *m_grip = nullptr;
    QStackedWidget *m_keyStack = nullptr;  // label ↔ line edit
    QLabel *m_keyLabel = nullptr;
    QLineEdit *m_keyEdit = nullptr;
    PropertyEditorWidget *m_editor = nullptr;  // null for read-only rows
    QToolButton *m_deleteButton = nullptr;
};

}  // namespace Corbomite
```

- [ ] **Step 4: Implement the helper in `PropertyRow.cpp` (widget impl follows in Task 4 wiring)**

Create `PropertyRow.cpp` with the helper and a minimal constructor (the drag wiring is finished in Task 7/8; here we get it compiling + the helper correct):

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "PropertyRow.h"

#include "PropertyEditorWidget.h"

#include "corbomite/models/PropertyTypeInference.h"

#include <KLocalizedString>

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QStackedWidget>
#include <QToolButton>

namespace Corbomite {

bool isEditableFrontmatterValue(const Markoff::YamlValue &value)
{
    using Kind = Markoff::YamlValue::Kind;
    switch (value.kind()) {
    case Kind::Null:
    case Kind::Bool:
    case Kind::Int:
    case Kind::Double:
    case Kind::String:
        return true;
    case Kind::Seq:
        // Editable only if every element is a scalar.
        for (int i = 0, n = value.size(); i < n; ++i) {
            const auto el = value.at(i);
            if (el.kind() == Kind::Seq || el.kind() == Kind::Map) return false;
        }
        return true;
    case Kind::Map:
    default:
        return false;
    }
}

PropertyRow::PropertyRow(const QString &key, PropertyType type,
                         const Markoff::YamlValue &value, bool editable,
                         QWidget *parent)
    : QWidget(parent), m_key(key), m_type(type), m_editable(editable)
{
    auto *h = new QHBoxLayout(this);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(4);

    m_grip = new QToolButton(this);
    m_grip->setIcon(QIcon::fromTheme(QStringLiteral("application-menu")));  // grip affordance; drag wired in Task 7
    m_grip->setAutoRaise(true);
    m_grip->setCursor(Qt::SizeAllCursor);
    m_grip->setToolTip(i18n("Drag to reorder"));
    h->addWidget(m_grip);

    m_keyStack = new QStackedWidget(this);
    m_keyLabel = new QLabel(key, this);
    m_keyEdit = new QLineEdit(key, this);
    m_keyStack->addWidget(m_keyLabel);
    m_keyStack->addWidget(m_keyEdit);
    m_keyStack->setCurrentWidget(m_keyLabel);
    h->addWidget(m_keyStack);

    if (editable) {
        m_editor = makePropertyEditor(type, value, this);
        connect(m_editor, &PropertyEditorWidget::valueChanged,
                this, &PropertyRow::valueChanged);
        h->addWidget(m_editor, 1);
    } else {
        auto *summary = new QLabel(
            value.kind() == Markoff::YamlValue::Kind::Map
                ? i18n("{…} (not editable here)")
                : i18n("[…] (not editable here)"),
            this);
        summary->setStyleSheet(QStringLiteral("color: gray;"));
        h->addWidget(summary, 1);
    }

    m_deleteButton = new QToolButton(this);
    m_deleteButton->setIcon(QIcon::fromTheme(QStringLiteral("edit-delete")));
    m_deleteButton->setAutoRaise(true);
    m_deleteButton->setToolTip(i18n("Delete property"));
    connect(m_deleteButton, &QToolButton::clicked,
            this, &PropertyRow::deleteRequested);
    h->addWidget(m_deleteButton);

    // Inline rename (editable rows only).
    if (editable) {
        m_keyLabel->setCursor(Qt::IBeamCursor);
        m_keyLabel->installEventFilter(this);  // handled in Task 7
    }
}

Markoff::YamlValue PropertyRow::currentValue() const
{
    if (m_editor) return m_editor->currentValue();
    return Markoff::YamlValue();  // read-only → caller uses preserveFromDisk
}

void PropertyRow::beginInlineRename()
{
    m_keyEdit->setText(m_key);
    m_keyStack->setCurrentWidget(m_keyEdit);
    m_keyEdit->setFocus();
    m_keyEdit->selectAll();
}

void PropertyRow::commitInlineRename()
{
    const QString proposed = m_keyEdit->text().trimmed();
    m_keyStack->setCurrentWidget(m_keyLabel);
    if (proposed.isEmpty() || proposed == m_key) return;
    Q_EMIT keyRenameRequested(m_key, proposed);
}

}  // namespace Corbomite
```

> `eventFilter` (rename trigger) is added in Task 6 and the grip drag in Task 7. This compiles now without an `eventFilter` override because `installEventFilter` only registers and `QObject::eventFilter` (the base impl) handles events until Task 6 overrides it. Do not add a stub override here — adding it in Task 6 with the real body avoids an unused-parameter warning in between.

- [ ] **Step 5: Add `PropertyRow` to the sidebar CMake target**

In `src/sidebar/CMakeLists.txt`, add `PropertyRow.cpp` to the same source list that contains `PropertyEditorWidget.cpp` (find it with `grep -n PropertyEditorWidget src/sidebar/CMakeLists.txt`).

- [ ] **Step 6: Run to verify the helper test passes**

```bash
cmake --build --preset dev -j 10 --target tst_properties_plugin 2>&1 | tail -3
./build-dev/bin/tst_properties_plugin classifiesComplexValuesAsNonEditable 2>&1 | tail -8
```

Expected: PASS.

- [ ] **Step 7: Commit**

```bash
git add src/sidebar/PropertyRow.h src/sidebar/PropertyRow.cpp \
        src/sidebar/CMakeLists.txt \
        src/plugins/properties/tests/tst_properties_plugin.cpp
git commit -m "feat(properties): PropertyRow widget + isEditableFrontmatterValue

Row widget (grip/key/editor/delete) and the pure classifier that marks
maps + lists-of-non-scalars read-only (preserved verbatim on write).

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

## Phase 3 — PropertiesView rework

> The view becomes a `QVBoxLayout` of `PropertyRow`s. All five interactions funnel through `flushWrite()` → `setFrontMatter`. Each interaction gets a public method (UI wrappers call them) so they're testable without driving real dialogs/drag.

### Task 4: swap layout to `PropertyRow` list + write via `setFrontMatter`

**Files:**
- Modify: `src/plugins/properties/PropertiesView.h`
- Modify: `src/plugins/properties/PropertiesView.cpp`
- Test: add a write-path slot to `tst_properties_plugin.cpp`

- [ ] **Step 1: Write the failing test (add + write produces correct on-disk order)**

Add to `tst_properties_plugin.cpp`. Build a view via the existing granted-context `createView` path, then drive the public API and assert on disk. Reuse the harness from `createsViewWhenMetadataAndWriteGranted` (real `Vault`/`FileManager`/`MetadataCache`, `vault.read|write|metadata.read|workspace`). After creating the file on disk, set the active path so the view targets it:

```cpp
void TestPropertiesPlugin::addThenWritePersistsTypedKeysInOrder()
{
    using namespace Corbomite;
    FileSystemAdapter fs; QTemporaryDir dir; Vault vault(&fs); vault.load(dir.path());
    LinkResolver resolver; MetadataCache cache(resolver); FileManager fm(&vault, &cache);
    vault.write(QStringLiteral("note.md"), "---\nexisting: 1\n---\nbody\n");

    PropertiesPlugin plugin;
    PluginContext ctx(makeMeta(),
        {QStringLiteral("vault.read"), QStringLiteral("vault.write"),
         QStringLiteral("metadata.read"), QStringLiteral("workspace")});
    ctx.setCoreServices(&vault, &fm, &cache, nullptr, nullptr, nullptr,
                        nullptr, nullptr, nullptr);
    plugin.load(&ctx);
    auto *view = qobject_cast<PropertiesView *>(plugin.createView(nullptr));
    QVERIFY(view);
    view->setActiveFileForTest(QStringLiteral("note.md"));

    view->addProperty(QStringLiteral("title"), PropertyType::Text);
    view->setRowValueForTest(QStringLiteral("title"), QStringLiteral("Hello"));
    view->flushPendingWrite();

    auto doc = Markoff::Document::fromMarkdown(
        QString::fromUtf8(vault.read(QStringLiteral("note.md"))));
    QVERIFY(doc->parsedFrontmatter().keys().contains(QStringLiteral("title")));
    QCOMPARE(doc->parsedFrontmatter().get(QStringLiteral("title")).asString(),
             QStringLiteral("Hello"));
    delete view;
}
```

> Add `setActiveFileForTest`/`setRowValueForTest` as thin public test seams (Step 3). If `PropertiesView::addProperty` doesn't exist yet, that's the red.

- [ ] **Step 2: Run to verify it fails**

```bash
cmake --build --preset dev -j 10 --target tst_properties_plugin 2>&1 | tail -5
```

Expected: compile error — `addProperty`/test seams not declared.

- [ ] **Step 3: Rework the header**

In `PropertiesView.h`, replace the `EditorRow`/`QFormLayout` members with a `PropertyRow` list + public interaction API:

```cpp
public:
    // ... existing ctor/dtor ...
    int rowCount() const;
    void flushPendingWrite();

    // Interaction API (UI wrappers + tests call these).
    void addProperty(const QString &name, PropertyType type);
    bool renameProperty(const QString &oldKey, const QString &newKey);
    void deleteProperty(const QString &key);
    void moveProperty(int from, int to);

    // Test seams.
    void setActiveFileForTest(const QString &path) { onActiveFileChanged(path); }
    void setRowValueForTest(const QString &key, const QString &text);

private:
    void rebuildFromFrontmatter(const QJsonObject &fm);
    void appendRow(const QString &key, PropertyType type,
                   const Markoff::YamlValue &value, bool editable);
    void clearRows();
    int  indexOfKey(const QString &key) const;
    bool keyExists(const QString &key) const;  // case-insensitive

    // ... keep m_metadata/m_vaultProxy/m_fmProxy/m_workspace/m_headerLabel/
    //     m_emptyLabel/m_addPropertyButton/m_writeDebounce/m_currentPath ...
    QWidget    *m_rowsContainer = nullptr;
    QVBoxLayout *m_rowsLayout = nullptr;
    QVector<PropertyRow *> m_rows;
```

Add includes: `#include "corbomite/models/PropertyType.h"`, forward-declare `class PropertyRow;` and `class QVBoxLayout;`. Remove the old `EditorRow` struct + `QFormLayout *m_form` + `m_formContainer` + `PropertyEditorWidget` fwd decl that are no longer used.

- [ ] **Step 4: Rework the .cpp — construction, refresh, append, clear, flushWrite**

Replace the `QFormLayout` plumbing. Key changes (full method bodies):

Constructor — swap the form for a rows container:

```cpp
    m_rowsContainer = new QWidget(this);
    m_rowsLayout = new QVBoxLayout(m_rowsContainer);
    m_rowsLayout->setContentsMargins(0, 0, 0, 0);
    m_rowsLayout->setSpacing(2);
    layout->addWidget(m_rowsContainer);
```

`clearRows`:

```cpp
void PropertiesView::clearRows()
{
    for (PropertyRow *r : m_rows) { m_rowsLayout->removeWidget(r); r->deleteLater(); }
    m_rows.clear();
}
```

`appendRow` (wires every row signal to the interaction API):

```cpp
void PropertiesView::appendRow(const QString &key, PropertyType type,
                               const Markoff::YamlValue &value, bool editable)
{
    auto *row = new PropertyRow(key, type, value, editable, m_rowsContainer);
    connect(row, &PropertyRow::valueChanged, this, &PropertiesView::scheduleWrite);
    connect(row, &PropertyRow::deleteRequested, this,
            [this, row]() { deleteProperty(row->key()); });
    connect(row, &PropertyRow::keyRenameRequested, this,
            [this](const QString &o, const QString &n) { renameProperty(o, n); });
    m_rowsLayout->addWidget(row);
    m_rows.push_back(row);
}
```

`refresh` → delegate to `rebuildFromFrontmatter` using the classifier:

```cpp
void PropertiesView::refresh()
{
    clearRows();
    if (m_currentPath.isEmpty() || !m_metadata) {
        m_headerLabel->setText(i18n("Properties"));
        m_emptyLabel->setVisible(true);
        m_rowsContainer->setVisible(false);
        m_addPropertyButton->setEnabled(false);
        return;
    }
    m_addPropertyButton->setEnabled(true);
    const QJsonObject fm = m_metadata->frontmatterFor(m_currentPath);
    if (fm.isEmpty()) {
        m_headerLabel->setText(i18n("Properties"));
        m_emptyLabel->setVisible(true);
        m_rowsContainer->setVisible(false);
        return;
    }
    m_emptyLabel->setVisible(false);
    m_rowsContainer->setVisible(true);
    m_headerLabel->setText(i18n("Properties (%1)", fm.size()));
    rebuildFromFrontmatter(fm);
}

void PropertiesView::rebuildFromFrontmatter(const QJsonObject &fm)
{
    for (auto it = fm.begin(); it != fm.end(); ++it) {
        const Markoff::YamlValue yval = qJsonValueToYaml(it.value());
        const bool editable = isEditableFrontmatterValue(yval);
        const PropertyType type = editable ? inferPropertyType(yval) : PropertyType::Text;
        appendRow(it.key(), type, yval, editable);
    }
}
```

`flushWrite` — build ordered entries from rows, call `setFrontMatter`:

```cpp
void PropertiesView::flushWrite()
{
    if (m_currentPath.isEmpty() || !m_vaultProxy || !m_fmProxy) return;
    auto *tf = m_vaultProxy->getFileByPath(m_currentPath);
    if (!tf) return;

    QList<FileManager::FrontMatterEntry> entries;
    entries.reserve(m_rows.size());
    for (PropertyRow *row : m_rows) {
        if (row->preserveFromDisk()) {
            entries.push_back({row->key(), QVariant{}, true});
            continue;
        }
        const Markoff::YamlValue v = row->currentValue();
        QVariant qv;
        switch (v.kind()) {
        case Markoff::YamlValue::Kind::Bool:   qv = v.asBool(); break;
        case Markoff::YamlValue::Kind::Int:    qv = QVariant::fromValue<qlonglong>(v.asInt()); break;
        case Markoff::YamlValue::Kind::Double: qv = v.asDouble(); break;
        case Markoff::YamlValue::Kind::String: qv = v.asString(); break;
        case Markoff::YamlValue::Kind::Seq:    qv = v.asStringList(); break;
        case Markoff::YamlValue::Kind::Null:
        default:                               qv = QVariant(); break;
        }
        entries.push_back({row->key(), qv, false});
    }
    if (!m_fmProxy->setFrontMatter(tf, entries))
        qWarning() << "PropertiesView: setFrontMatter failed for" << m_currentPath;
}
```

Add the test seam + `indexOfKey`/`keyExists`:

```cpp
void PropertiesView::setRowValueForTest(const QString &key, const QString &text)
{
    const int i = indexOfKey(key);
    if (i < 0 || m_rows[i]->isReadOnly()) return;
    // Editable rows are TextPropertyEditor for Text type; set via the editor.
    // (Tests only use Text here.) Find the line edit and set it.
    if (auto *le = m_rows[i]->findChild<QLineEdit *>()) le->setText(text);
}

int PropertiesView::indexOfKey(const QString &key) const
{
    for (int i = 0; i < m_rows.size(); ++i)
        if (m_rows[i]->key() == key) return i;
    return -1;
}

bool PropertiesView::keyExists(const QString &key) const
{
    for (PropertyRow *r : m_rows)
        if (r->key().compare(key, Qt::CaseInsensitive) == 0) return true;
    return false;
}
```

Add includes to the .cpp: `"../../sidebar/PropertyRow.h"`, `<QVBoxLayout>`, `<QLineEdit>`. Add the necessary `#include "corbomite/vault/FileManager.h"` for `FrontMatterEntry`.

- [ ] **Step 5: Implement `addProperty` (minimal, so the test passes)**

```cpp
void PropertiesView::addProperty(const QString &name, PropertyType type)
{
    const QString n = name.trimmed();
    if (n.isEmpty() || m_currentPath.isEmpty() || keyExists(n)) return;
    appendRow(n, type, Markoff::YamlValue(), /*editable=*/true);
    m_emptyLabel->setVisible(false);
    m_rowsContainer->setVisible(true);
    scheduleWrite();
}
```

> `deleteProperty`, `renameProperty`, `moveProperty` get full bodies in Tasks 5–7; add **temporary stubs** now (`{ /* Task N */ }` returning false where needed) so the file links. They are replaced, not appended to, in later tasks.

- [ ] **Step 6: Run to verify the add/write test passes**

```bash
cmake --build --preset dev -j 10 --target tst_properties_plugin 2>&1 | tail -3
./build-dev/bin/tst_properties_plugin addThenWritePersistsTypedKeysInOrder 2>&1 | tail -8
```

Expected: PASS. Also re-run the pre-existing slots to confirm no regression:

```bash
./build-dev/bin/tst_properties_plugin 2>&1 | tail -6
```

- [ ] **Step 7: Commit**

```bash
git add src/plugins/properties/PropertiesView.h \
        src/plugins/properties/PropertiesView.cpp \
        src/plugins/properties/tests/tst_properties_plugin.cpp
git commit -m "refactor(properties): PropertyRow list + setFrontMatter write path

Replace QFormLayout with an ordered PropertyRow list; classify complex
values read-only; write wholesale via FileManagerProxy::setFrontMatter.
Add-with-type lands; delete/rename/reorder stubbed for following tasks.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

### Task 5: delete

**Files:** Modify `src/plugins/properties/PropertiesView.cpp`; test in `tst_properties_plugin.cpp`.

- [ ] **Step 1: Write the failing test**

```cpp
void TestPropertiesPlugin::deleteRemovesKeyFromDisk()
{
    using namespace Corbomite;
    FileSystemAdapter fs; QTemporaryDir dir; Vault vault(&fs); vault.load(dir.path());
    LinkResolver resolver; MetadataCache cache(resolver); FileManager fm(&vault, &cache);
    vault.write(QStringLiteral("n.md"), "---\nkeep: 1\ndrop: 2\n---\nx\n");

    PropertiesPlugin plugin;
    PluginContext ctx(makeMeta(),
        {QStringLiteral("vault.read"), QStringLiteral("vault.write"),
         QStringLiteral("metadata.read"), QStringLiteral("workspace")});
    ctx.setCoreServices(&vault, &fm, &cache, nullptr, nullptr, nullptr,
                        nullptr, nullptr, nullptr);
    plugin.load(&ctx);
    auto *view = qobject_cast<PropertiesView *>(plugin.createView(nullptr));
    view->setActiveFileForTest(QStringLiteral("n.md"));
    // Seed rows directly (no cache dependency): add both keys.
    view->addProperty(QStringLiteral("keep"), PropertyType::Number);
    view->setRowValueForTest(QStringLiteral("keep"), QStringLiteral("1"));
    view->addProperty(QStringLiteral("drop"), PropertyType::Number);
    view->setRowValueForTest(QStringLiteral("drop"), QStringLiteral("2"));

    view->deleteProperty(QStringLiteral("drop"));
    view->flushPendingWrite();

    auto doc = Markoff::Document::fromMarkdown(
        QString::fromUtf8(vault.read(QStringLiteral("n.md"))));
    QVERIFY(!doc->parsedFrontmatter().keys().contains(QStringLiteral("drop")));
    QVERIFY(doc->parsedFrontmatter().keys().contains(QStringLiteral("keep")));
    delete view;
}
```

> Number-typed rows: `setRowValueForTest` sets a `QLineEdit` — `NumberPropertyEditor` uses a `QDoubleSpinBox`, not a line edit. To keep the seam simple, use `PropertyType::Text` for these seed rows instead and write string values; assert key presence/absence (not the numeric value). Adjust the test to `PropertyType::Text` accordingly.

- [ ] **Step 2: Run to verify it fails**

```bash
./build-dev/bin/tst_properties_plugin deleteRemovesKeyFromDisk 2>&1 | tail -8
```

Expected: FAIL (stub `deleteProperty` is a no-op, so `drop` survives).

- [ ] **Step 3: Implement `deleteProperty`**

```cpp
void PropertiesView::deleteProperty(const QString &key)
{
    const int i = indexOfKey(key);
    if (i < 0) return;
    PropertyRow *row = m_rows.takeAt(i);
    m_rowsLayout->removeWidget(row);
    row->deleteLater();
    if (m_rows.isEmpty()) { m_emptyLabel->setVisible(true); m_rowsContainer->setVisible(false); }
    scheduleWrite();
}
```

- [ ] **Step 4: Run to verify it passes**

```bash
cmake --build --preset dev -j 10 --target tst_properties_plugin 2>&1 | tail -3
./build-dev/bin/tst_properties_plugin deleteRemovesKeyFromDisk 2>&1 | tail -8
```

Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add src/plugins/properties/PropertiesView.cpp \
        src/plugins/properties/tests/tst_properties_plugin.cpp
git commit -m "feat(properties): delete a property row + erase the key on save

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

### Task 6: rename

**Files:** Modify `PropertiesView.cpp`, `PropertyRow.cpp`/`.h` (rename trigger); test in `tst_properties_plugin.cpp`.

- [ ] **Step 1: Write the failing test**

```cpp
void TestPropertiesPlugin::renameChangesKeyPreservingPositionAndValue()
{
    using namespace Corbomite;
    FileSystemAdapter fs; QTemporaryDir dir; Vault vault(&fs); vault.load(dir.path());
    LinkResolver resolver; MetadataCache cache(resolver); FileManager fm(&vault, &cache);
    vault.write(QStringLiteral("n.md"), "---\na: x\nb: y\n---\nbody\n");

    PropertiesPlugin plugin;
    PluginContext ctx(makeMeta(),
        {QStringLiteral("vault.read"), QStringLiteral("vault.write"),
         QStringLiteral("metadata.read"), QStringLiteral("workspace")});
    ctx.setCoreServices(&vault, &fm, &cache, nullptr, nullptr, nullptr,
                        nullptr, nullptr, nullptr);
    plugin.load(&ctx);
    auto *view = qobject_cast<PropertiesView *>(plugin.createView(nullptr));
    view->setActiveFileForTest(QStringLiteral("n.md"));
    view->addProperty(QStringLiteral("a"), PropertyType::Text);
    view->setRowValueForTest(QStringLiteral("a"), QStringLiteral("x"));
    view->addProperty(QStringLiteral("b"), PropertyType::Text);
    view->setRowValueForTest(QStringLiteral("b"), QStringLiteral("y"));

    QVERIFY(view->renameProperty(QStringLiteral("a"), QStringLiteral("alpha")));
    QVERIFY(!view->renameProperty(QStringLiteral("alpha"), QStringLiteral("b"))); // dup rejected
    view->flushPendingWrite();

    auto doc = Markoff::Document::fromMarkdown(
        QString::fromUtf8(vault.read(QStringLiteral("n.md"))));
    QCOMPARE(doc->parsedFrontmatter().keys(),
             QStringList({QStringLiteral("alpha"), QStringLiteral("b")})); // position kept
    QCOMPARE(doc->parsedFrontmatter().get(QStringLiteral("alpha")).asString(),
             QStringLiteral("x")); // value kept
    delete view;
}
```

- [ ] **Step 2: Run to verify it fails**

Expected: FAIL — stub `renameProperty` returns false.

- [ ] **Step 3: Implement `renameProperty` (rebuild the affected row in place)**

```cpp
bool PropertiesView::renameProperty(const QString &oldKey, const QString &newKey)
{
    const QString n = newKey.trimmed();
    const int i = indexOfKey(oldKey);
    if (i < 0 || n.isEmpty()) return false;
    if (m_rows[i]->isReadOnly()) return false;          // rename disabled on complex rows
    if (keyExists(n) && n.compare(oldKey, Qt::CaseInsensitive) != 0) return false;

    // Rebuild the row in place under the new key, carrying type + current value.
    PropertyRow *old = m_rows[i];
    const PropertyType type = old->type();
    const Markoff::YamlValue val = old->currentValue();
    auto *row = new PropertyRow(n, type, val, /*editable=*/true, m_rowsContainer);
    connect(row, &PropertyRow::valueChanged, this, &PropertiesView::scheduleWrite);
    connect(row, &PropertyRow::deleteRequested, this,
            [this, row]() { deleteProperty(row->key()); });
    connect(row, &PropertyRow::keyRenameRequested, this,
            [this](const QString &o, const QString &nn) { renameProperty(o, nn); });
    m_rowsLayout->insertWidget(i, row);
    m_rowsLayout->removeWidget(old);
    old->deleteLater();
    m_rows[i] = row;
    scheduleWrite();
    return true;
}
```

- [ ] **Step 4: Wire the inline-rename trigger in `PropertyRow`**

Add to `PropertyRow.h`: `protected: bool eventFilter(QObject *obj, QEvent *ev) override;`. Implement in `PropertyRow.cpp`:

```cpp
bool PropertyRow::eventFilter(QObject *obj, QEvent *ev)
{
    if (obj == m_keyLabel && ev->type() == QEvent::MouseButtonRelease) {
        beginInlineRename();
        return true;
    }
    return QWidget::eventFilter(obj, ev);
}
```

And in the constructor (editable branch) connect commit on the line edit:

```cpp
    connect(m_keyEdit, &QLineEdit::editingFinished, this, &PropertyRow::commitInlineRename);
```

- [ ] **Step 5: Run to verify it passes**

```bash
cmake --build --preset dev -j 10 --target tst_properties_plugin 2>&1 | tail -3
./build-dev/bin/tst_properties_plugin renameChangesKeyPreservingPositionAndValue 2>&1 | tail -8
```

Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add src/plugins/properties/PropertiesView.cpp \
        src/sidebar/PropertyRow.h src/sidebar/PropertyRow.cpp \
        src/plugins/properties/tests/tst_properties_plugin.cpp
git commit -m "feat(properties): inline rename a property (position + value kept)

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

### Task 7: reorder

**Files:** Modify `PropertiesView.cpp` (`moveProperty`), `PropertyRow.{h,cpp}` (drag emit); test in `tst_properties_plugin.cpp`.

- [ ] **Step 1: Write the failing test (model-level reorder)**

```cpp
void TestPropertiesPlugin::moveReordersPersistedKeys()
{
    using namespace Corbomite;
    FileSystemAdapter fs; QTemporaryDir dir; Vault vault(&fs); vault.load(dir.path());
    LinkResolver resolver; MetadataCache cache(resolver); FileManager fm(&vault, &cache);
    vault.write(QStringLiteral("n.md"), "---\none: 1\ntwo: 2\nthree: 3\n---\nb\n");

    PropertiesPlugin plugin;
    PluginContext ctx(makeMeta(),
        {QStringLiteral("vault.read"), QStringLiteral("vault.write"),
         QStringLiteral("metadata.read"), QStringLiteral("workspace")});
    ctx.setCoreServices(&vault, &fm, &cache, nullptr, nullptr, nullptr,
                        nullptr, nullptr, nullptr);
    plugin.load(&ctx);
    auto *view = qobject_cast<PropertiesView *>(plugin.createView(nullptr));
    view->setActiveFileForTest(QStringLiteral("n.md"));
    for (auto k : {QStringLiteral("one"), QStringLiteral("two"), QStringLiteral("three")}) {
        view->addProperty(k, PropertyType::Text);
        view->setRowValueForTest(k, QStringLiteral("v"));
    }

    view->moveProperty(2, 0);   // move "three" to the front
    view->flushPendingWrite();

    auto doc = Markoff::Document::fromMarkdown(
        QString::fromUtf8(vault.read(QStringLiteral("n.md"))));
    QCOMPARE(doc->parsedFrontmatter().keys(),
             QStringList({QStringLiteral("three"), QStringLiteral("one"),
                          QStringLiteral("two")}));
    delete view;
}
```

- [ ] **Step 2: Run to verify it fails**

Expected: FAIL — stub `moveProperty` is a no-op.

- [ ] **Step 3: Implement `moveProperty`**

```cpp
void PropertiesView::moveProperty(int from, int to)
{
    if (from < 0 || from >= m_rows.size() || to < 0 || to >= m_rows.size() || from == to)
        return;
    PropertyRow *row = m_rows.takeAt(from);
    m_rows.insert(to, row);
    m_rowsLayout->removeWidget(row);
    m_rowsLayout->insertWidget(to, row);
    scheduleWrite();
}
```

- [ ] **Step 4: Wire drag on the grip → `reorderRequested` → `moveProperty`**

In `PropertyRow.cpp`, make the grip start a drag carrying the source row's pointer-as-index. The simplest robust approach: emit `reorderRequested` with the drop target resolved by the view. Implement a minimal `QDrag` on the grip press and accept drops on `PropertyRow`, computing from/to via the view. Concretely:

- `PropertyRow` installs an event filter on `m_grip` for `MouseButtonPress` → starts a `QDrag` whose mime data carries the row's current index (the view sets it via a `setVisualIndex(int)` setter called in `appendRow`/`moveProperty`).
- `PropertiesView` enables `setAcceptDrops(true)` on `m_rowsContainer` and overrides `dragEnterEvent`/`dropEvent` to compute the target index from the drop Y position (`childAt`) and call `moveProperty(srcIndex, targetIndex)`.

Add to `PropertyRow.h`: `void setVisualIndex(int i) { m_visualIndex = i; }` + `int m_visualIndex = -1;` and the grip mouse handling in `eventFilter`. Add to `PropertiesView`: `dragEnterEvent`/`dropEvent` overrides + `setAcceptDrops(true)`.

> The model-level test (`moveProperty`) is the regression guard. The pixel-level drag is exercised manually (offscreen Qt can't synthesize DnD reliably) and flagged for user eyeball.

- [ ] **Step 5: Run to verify it passes**

```bash
cmake --build --preset dev -j 10 --target tst_properties_plugin 2>&1 | tail -3
./build-dev/bin/tst_properties_plugin moveReordersPersistedKeys 2>&1 | tail -8
```

Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add src/plugins/properties/PropertiesView.cpp src/plugins/properties/PropertiesView.h \
        src/sidebar/PropertyRow.h src/sidebar/PropertyRow.cpp \
        src/plugins/properties/tests/tst_properties_plugin.cpp
git commit -m "feat(properties): drag-reorder property rows (order persisted to YAML)

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

### Task 8: add-with-type dialog (UI wrapper)

**Files:** Modify `src/plugins/properties/PropertiesView.cpp` (`onAddPropertyClicked`).

- [ ] **Step 1: Replace the name-only prompt with a name + type dialog**

`onAddPropertyClicked` builds a small `QDialog` with a `QLineEdit` (name) and a `QComboBox` (type), then calls the already-tested `addProperty(name, type)`:

```cpp
void PropertiesView::onAddPropertyClicked()
{
    QDialog dlg(this);
    dlg.setWindowTitle(i18n("Add property"));
    auto *form = new QFormLayout(&dlg);
    auto *nameEdit = new QLineEdit(&dlg);
    auto *typeCombo = new QComboBox(&dlg);
    typeCombo->addItem(i18n("Text"),      static_cast<int>(PropertyType::Text));
    typeCombo->addItem(i18n("Number"),    static_cast<int>(PropertyType::Number));
    typeCombo->addItem(i18n("Checkbox"),  static_cast<int>(PropertyType::Checkbox));
    typeCombo->addItem(i18n("Date"),      static_cast<int>(PropertyType::Date));
    typeCombo->addItem(i18n("Date & time"), static_cast<int>(PropertyType::DateTime));
    typeCombo->addItem(i18n("List"),      static_cast<int>(PropertyType::List));
    form->addRow(i18n("Name:"), nameEdit);
    form->addRow(i18n("Type:"), typeCombo);
    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted) return;
    const QString name = nameEdit->text().trimmed();
    if (name.isEmpty()) return;
    if (keyExists(name)) {
        QMessageBox::warning(this, i18n("Add property"),
            i18n("A property named '%1' already exists.", name));
        return;
    }
    addProperty(name, static_cast<PropertyType>(typeCombo->currentData().toInt()));
}
```

Add includes: `<QComboBox>`, `<QDialog>`, `<QDialogButtonBox>`, `<QFormLayout>`, `<QMessageBox>`. Remove the now-unused `<QInputDialog>` include if nothing else uses it.

- [ ] **Step 2: Build + smoke (no new automated test — addProperty is already covered)**

```bash
cmake --build --preset dev -j 10 --target Corbomite 2>&1 | tail -5
```

Expected: clean build.

- [ ] **Step 3: Commit**

```bash
git add src/plugins/properties/PropertiesView.cpp
git commit -m "feat(properties): add-property dialog with a type picker

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

## Phase 4 — verification + closeout

### Task 9: full suite + launch smoke

- [ ] **Step 1: Build everything**

```bash
cmake --build --preset dev -j 10 2>&1 | tail -5
```

Expected: clean build.

- [ ] **Step 2: Run the full test suite**

```bash
cd build-dev && ctest --output-on-failure -j 10 2>&1 | tail -25; cd ..
```

Expected: green except the pre-existing known failures recorded in `docs/punch-list.md` (the render trio `tst_markdownrenderer`/`tst_renderengine`, `tst_metadataparser` embed slots, `tst_editorsuggest` SIGABRT, `tst_benchmark_layout` timeout). Confirm `tst_setfrontmatter` and `tst_properties_plugin` are green and no **new** failures appeared.

- [ ] **Step 3: Offscreen launch smoke**

```bash
QT_QPA_PLATFORM=offscreen ./build-dev/Corbomite --version 2>&1 | tail -3 || \
QT_QPA_PLATFORM=offscreen timeout 5 ./build-dev/Corbomite 2>&1 | tail -10
```

Expected: launches without crash/abort.

### Task 10: docs closeout

- [ ] **Step 1: Mark the punch-list item resolved**

In `docs/punch-list.md`, change the `[plugin][ui-bundle][P2] PropertiesView is read-mostly` item to `[x]` with a `**[RESOLVED 2026-05-27 — <commit>]**` prefix summarizing: add-with-type / delete / rename / drag-reorder shipped; ordered `setFrontMatter` + `YamlValue::setChildFrom`; fixed the pre-existing nested-map blank-on-edit corruption; interactive paths pending user eyeball.

- [ ] **Step 2: PROJECT-STATE + decisions-archive**

- Replace the top `## Current focus` / `## Last touched` entry in `docs/PROJECT-STATE.md` (≤3 sentences) noting the Properties editing surface shipped.
- Append a dated `## 2026-05-27 — Properties panel editing` H2 to `docs/decisions-archive.md` with the full closeout paragraph (the cross-repo Markoff primitive, the setFrontMatter API, the corruption fix, the pending-eyeball interactive paths).

- [ ] **Step 3: Commit docs**

```bash
git add docs/punch-list.md docs/PROJECT-STATE.md docs/decisions-archive.md
git commit -m "docs: close P2 properties-panel-editing; record closeout

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

- [ ] **Step 4: Hand back for user verification**

Report: full-suite result, the commits, and the explicit list of interactive paths needing user eyeball (add dialog render, inline rename click, drag-reorder, complex-value read-only summary) — joining the D.2–D.4b verification backlog.

---

## Definition of Done

- Markoff `YamlValue::setChildFrom` shipped + tested + pin bumped.
- `FileManager::setFrontMatter` (+ proxy) shipped; `tst_setfrontmatter` green (order, delete, strip, verbatim-preserve, rename-shape).
- PropertiesView: add-with-type, edit, delete, rename, drag-reorder — all funnel through `setFrontMatter`.
- Nested-map / complex-list values render read-only and survive edits of other keys (corruption fix proven by `tst_setfrontmatter::preserveFromDiskKeepsNestedMapVerbatim`).
- Full `ctest` green except documented pre-existing failures; clean build; offscreen launch clean.
- Punch-list P2 resolved; PROJECT-STATE + decisions-archive updated.
- Interactive paths flagged pending user eyeball.
