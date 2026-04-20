# Cluster Q.0 — Vault Architecture Refactor — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.
>
> **Working directory:** `/home/clinton/dev/Corbomite` on `master` (no worktrees — other agents share the tree; see `memory/feedback_no_branches.md`).
>
> **Build commands:**
> - Configure: `cmake -S . -B build -DCORBOMITE_DEV_BUILD=ON`
> - Build: `cmake --build build`
> - Test all: `cd build && ctest --output-on-failure`
> - Test one: `cd build && ctest -R <name> --output-on-failure`
> - 4 known-flaky tests to ignore: `tst_markoff_inline_math`, `tst_renderengine`, `tst_completion_popup`, `tst_benchmark_layout`.

**Goal:** Collapse the three-way `Corbomite::Vault` (libs/core Task-7 stub) / `Corbomite::VaultModel` (libs/models) / `VaultService` (src/app) split into a single Obsidian-shaped `Vault` aggregate in a new `libs/vault/` library, with a `FileManager` sibling for link-aware operations. Rewire the plugin proxy layer (`VaultProxy` + `FileManagerProxy` replacing `VaultReader` + `VaultWriter`).

**Architecture:** Single `Vault` class + `FileManager` class + `TFile` / `TFolder` / `TAbstractFile` value-bearing handle types in a new `libs/vault/` library depending on `Corbomite::Core` + `Corbomite::Storage`. One canonical vault per process, sync API, method-level permission gating on plugin proxies. See spec: `docs/superpowers/specs/2026-04-16-vault-architecture-design.md`.

**Tech Stack:** Qt6, KF6, C++20, CMake, QTest.

**Spec:** `docs/superpowers/specs/2026-04-16-vault-architecture-design.md` — authoritative. Read first.

**Audit references:**
- `docs/obsidian-audit/domains/vault.md` — Obsidian `Vault` + `FileManager` + `FileSystemAdapter` reference.
- `docs/obsidian-audit/VAULT-FORMAT.md` — `.obsidian/*.json` config-file formats.
- `docs/obsidian-audit/PLUGIN-API-SKETCH.md` — `App.vault` + `App.fileManager` plugin-facing API shape.

---

## Pre-flight

- [ ] **P.1: Verify baseline green build**

```bash
cmake -S . -B build -DCORBOMITE_DEV_BUILD=ON && cmake --build build && (cd build && ctest --output-on-failure -E "tst_markoff_inline_math|tst_renderengine|tst_completion_popup|tst_benchmark_layout")
```

Expected: build succeeds; all tests green outside the 4 known-flaky.

- [ ] **P.2: Pause Cluster Q**

Cluster Q Tasks 1–6 have already landed (commits `35a9c07`, `b01b998`, `4fa4509`, `b9a271d`, `b67e7ed`). Tasks 7–12 will be rewritten AFTER Q.0 lands. No action needed — just note that tests touching `VaultReader`/`VaultWriter`/`MetadataCacheReader`/`WorkspaceController` will change during Phase 1 and Phase 9.

---

# Phase 1 — Scaffold `libs/vault/` + core types + proxy demolition

Goal: Create `libs/vault/` with the three handle types + a skeletal `Vault` class providing only load/unload/tree-query. Delete the Task-7 `Corbomite::Vault` + `VaultReader` + `VaultWriter` proxies and their tests. Build stays green at each task.

## Task 1.1: libs/vault scaffold + top-level wiring

**Files:**
- Create: `libs/vault/CMakeLists.txt`
- Create: `libs/vault/CLAUDE.md`
- Create: `libs/vault/include/corbomite/vault/.gitkeep`
- Create: `libs/vault/src/.gitkeep`
- Create: `libs/vault/tests/CMakeLists.txt`
- Modify: `CMakeLists.txt` (top-level — add `add_subdirectory(libs/vault)` after models)

- [ ] **Step 1: Create libs/vault/CMakeLists.txt (skeletal — sources added per task)**

```cmake
cmake_minimum_required(VERSION 3.19)
project(vault VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core)

add_library(vault STATIC
    # sources added by subsequent tasks
)
set_target_properties(vault PROPERTIES POSITION_INDEPENDENT_CODE ON)
add_library(Corbomite::Vault ALIAS vault)

target_include_directories(vault
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(vault
    PUBLIC
        Qt6::Core
        Corbomite::Core
        Corbomite::Storage
)

enable_testing()
add_subdirectory(tests)
```

Initial CMake has no sources — an empty static library. That's valid; tasks below register sources via `target_sources(vault PRIVATE ...)`.

- [ ] **Step 2: Create libs/vault/CLAUDE.md**

```markdown
# vault

Corbomite's canonical Vault aggregate. Single-vault-per-process. Owns the
`TFile`/`TFolder` tree, emits signals on every mutation, composes a
`DataAdapter *` for atomic file I/O, and hosts `FileManager` for link-aware
refactoring operations.

## Scope

Collapses what used to be split across `Corbomite::VaultModel` (libs/models),
the Task-7 path-only `Corbomite::Vault` (libs/core, deleted), `VaultProcess`
+ `VaultTrash` + `VaultScanner` (libs/storage), `FileWatchReactor`
(src/reactors), and parts of `VaultService` (src/app). Shape-parity with
Obsidian's `App.vault` + `App.fileManager` — see
`../../docs/superpowers/specs/2026-04-16-vault-architecture-design.md`.

Depends on:
- `Corbomite::Core` (Events mixin, NoteMeta, NoteDocument, FrontMatter types)
- `Corbomite::Storage` (DataAdapter, FileSystemAdapter, VaultConfig,
  MetadataCache — MetadataCache is consumed by FileManager only)

## Conventions

- C++20, Qt6.
- Use `i18n()` for all user-visible strings.
- SPDX header `GPL-3.0-or-later` on every source file.
- Public API in `include/corbomite/vault/`; plugin proxies in
  `include/corbomite/vault/proxies/`; internal types in `src/`.
- Namespace: `Corbomite`.

## Building

Built as part of the parent Corbomite tree via
`add_subdirectory(libs/vault)`. Tests run with `QT_QPA_PLATFORM=offscreen`.

## Testing

Tests live in `tests/`. Tests define expected behavior — when a test fails,
fix the code, not the test.
```

- [ ] **Step 3: Create libs/vault/include/corbomite/vault/.gitkeep and libs/vault/src/.gitkeep**

Empty files. Just:
```bash
mkdir -p libs/vault/include/corbomite/vault/proxies libs/vault/src/proxies
touch libs/vault/include/corbomite/vault/.gitkeep libs/vault/src/.gitkeep
```

- [ ] **Step 4: Create libs/vault/tests/CMakeLists.txt (skeletal — tests added per task)**

```cmake
find_package(Qt6 6.8 REQUIRED COMPONENTS Test)

# Tests registered by subsequent tasks via:
#   add_executable(<name> <name>.cpp)
#   target_link_libraries(<name> PRIVATE Qt6::Test Corbomite::Vault)
#   add_test(NAME <name> COMMAND <name>)
#   set_tests_properties(<name> PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
```

- [ ] **Step 5: Wire libs/vault into top-level CMakeLists.txt**

Edit `CMakeLists.txt` at the project root. Find the block of `add_subdirectory(libs/...)` lines. Add `add_subdirectory(libs/vault)` after `add_subdirectory(libs/models)` and before `add_subdirectory(libs/forcegraph)`.

- [ ] **Step 6: Verify build**

```bash
cmake --build build
```

Expected: the (empty) `vault` library builds. No test failures (no tests yet).

- [ ] **Step 7: Commit**

```bash
git add libs/vault CMakeLists.txt
git commit -m "feat(vault): scaffold libs/vault (Cluster Q.0 Phase 1 Task 1)"
```

---

## Task 1.2: `TAbstractFile` type + tests

**Files:**
- Create: `libs/vault/include/corbomite/vault/TAbstractFile.h`
- Create: `libs/vault/src/TAbstractFile.cpp`
- Create: `libs/vault/tests/tst_tabstractfile.cpp`
- Modify: `libs/vault/CMakeLists.txt` (add TAbstractFile.h + TAbstractFile.cpp to `target_sources`)
- Modify: `libs/vault/tests/CMakeLists.txt` (add `tst_tabstractfile` executable)

- [ ] **Step 1: Write failing test**

Create `libs/vault/tests/tst_tabstractfile.cpp`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include "corbomite/vault/TAbstractFile.h"

class TestTAbstractFile : public QObject
{
    Q_OBJECT
private slots:
    void pathAndNameDerivedFromCtor();
    void setPathUpdatesName();
    void getNewPathAfterRenameWithParent();
    void getNewPathAfterRenameDetachedReturnsEmpty();
    void stripsControlChars();
    void tombstoneDefaultsFalse();
};

namespace {
// Concrete subclass for testing the abstract base.
struct TestFile : public Corbomite::TAbstractFile {
    TestFile(Corbomite::Vault *v, QString p) : TAbstractFile(v, std::move(p)) {}
};
}

void TestTAbstractFile::pathAndNameDerivedFromCtor()
{
    TestFile f(nullptr, QStringLiteral("folder/sub/Note.md"));
    QCOMPARE(f.path, QStringLiteral("folder/sub/Note.md"));
    QCOMPARE(f.name, QStringLiteral("Note.md"));
}

void TestTAbstractFile::setPathUpdatesName()
{
    TestFile f(nullptr, QStringLiteral("a.md"));
    f.setPath(QStringLiteral("folder/b.md"));
    QCOMPARE(f.path, QStringLiteral("folder/b.md"));
    QCOMPARE(f.name, QStringLiteral("b.md"));
}

void TestTAbstractFile::getNewPathAfterRenameWithParent()
{
    // No parent set yet, so detached semantics apply. Parent-backed case is
    // exercised once TFolder exists (tst_vault_tree).
    TestFile f(nullptr, QStringLiteral("folder/old.md"));
    QCOMPARE(f.getNewPathAfterRename(QStringLiteral("new.md")), QString());
}

void TestTAbstractFile::getNewPathAfterRenameDetachedReturnsEmpty()
{
    TestFile f(nullptr, QStringLiteral("orphan.md"));
    QCOMPARE(f.getNewPathAfterRename(QStringLiteral("x")), QString());
}

void TestTAbstractFile::stripsControlChars()
{
    TestFile f(nullptr, QStringLiteral("a.md"));
    // Control chars in the rename input get stripped; detached still
    // returns empty string.
    QString result = f.getNewPathAfterRename(QStringLiteral("b\x01c.md"));
    QCOMPARE(result, QString());  // detached — parent-backed path tested elsewhere
}

void TestTAbstractFile::tombstoneDefaultsFalse()
{
    TestFile f(nullptr, QStringLiteral("a.md"));
    QCOMPARE(f.deleted, false);
}

QTEST_MAIN(TestTAbstractFile)
#include "tst_tabstractfile.moc"
```

- [ ] **Step 2: Register test executable in libs/vault/tests/CMakeLists.txt**

Append:

```cmake
add_executable(tst_tabstractfile tst_tabstractfile.cpp)
target_link_libraries(tst_tabstractfile PRIVATE Qt6::Test Corbomite::Vault)
add_test(NAME tst_tabstractfile COMMAND tst_tabstractfile)
set_tests_properties(tst_tabstractfile PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
```

- [ ] **Step 3: Run test — expect compile failure (header doesn't exist yet)**

```bash
cmake --build build 2>&1 | grep -i "fatal\|error" | head
```

Expected: compile error, `TAbstractFile.h: No such file or directory`.

- [ ] **Step 4: Create the header**

Create `libs/vault/include/corbomite/vault/TAbstractFile.h`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>

namespace Corbomite {

class Vault;
class TFolder;

/// Abstract base for `TFile` and `TFolder`. Non-QObject, cheap to allocate.
/// Owned by `Vault` (via `QHash<QString, std::unique_ptr<TAbstractFile>>`);
/// consumers hold non-owning raw pointers.
///
/// Tombstone on delete: `deleted = true` is set before the `unique_ptr` is
/// drained, so subscribers holding the pointer and receiving the
/// `Vault::deletedFile` signal can observe the flag and react safely.
class TAbstractFile
{
public:
    QString  path;                  ///< NFC-normalized, /-separated, root-relative.
    QString  name;                  ///< basename(path).
    TFolder *parent = nullptr;      ///< Non-owning; Vault owns the tree.
    bool     deleted = false;       ///< Tombstone — set true on removal.

    Vault *vault() const { return m_vault; }

    /// Updates `path` and `name`. Subclasses may override to update derived
    /// fields (TFile updates basename/extension).
    virtual void setPath(const QString &newPath);

    /// Returns the path the file would have if renamed to `newName` within
    /// its current parent. Strips control chars [\x00-\x1F] and trims.
    /// Returns empty string when detached (no parent).
    QString getNewPathAfterRename(const QString &newName) const;

    virtual ~TAbstractFile() = default;

protected:
    TAbstractFile(Vault *v, QString p);

private:
    Vault *m_vault;
};

} // namespace Corbomite
```

- [ ] **Step 5: Create the impl**

Create `libs/vault/src/TAbstractFile.cpp`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/vault/TAbstractFile.h"

#include <QFileInfo>
#include <QRegularExpression>

namespace Corbomite {

TAbstractFile::TAbstractFile(Vault *v, QString p)
    : path(std::move(p))
    , m_vault(v)
{
    name = QFileInfo(path).fileName();
}

void TAbstractFile::setPath(const QString &newPath)
{
    path = newPath;
    name = QFileInfo(path).fileName();
}

QString TAbstractFile::getNewPathAfterRename(const QString &newName) const
{
    if (!parent) return {};

    // Strip control chars and trim.
    QString cleaned = newName;
    cleaned.remove(QRegularExpression(QStringLiteral("[\\x00-\\x1F]")));
    cleaned = cleaned.trimmed();
    if (cleaned.isEmpty()) return {};

    // Parent-prefix + cleaned basename. TFolder::getParentPrefix() is
    // defined alongside TFolder; cannot include here (incomplete type).
    // The body reaches into parent->path at call time; see note in
    // TAbstractFile.cpp's dependency comment. For Task 1.2 we implement
    // via direct `parent->path` field access (declared in TAbstractFile.h
    // via forward-decl workaround) — but `parent` is TFolder* and TFolder
    // is declared only via forward decl. So we defer the actual prefix
    // computation to Task 1.4 where TFolder is defined, and return empty
    // here. Tests passing for Task 1.2 exercise only the detached case.

    // (Task 1.4 will replace this body with real parent-prefix logic.)
    return {};
}

} // namespace Corbomite
```

- [ ] **Step 6: Register source in libs/vault/CMakeLists.txt**

Inside `add_library(vault STATIC ...)` block, add:

```cmake
add_library(vault STATIC
    include/corbomite/vault/TAbstractFile.h
    src/TAbstractFile.cpp
)
```

- [ ] **Step 7: Build and run**

```bash
cmake --build build
cd build && ctest -R tst_tabstractfile --output-on-failure
```

Expected: all 6 test cases pass.

- [ ] **Step 8: Commit**

```bash
git add libs/vault
git commit -m "feat(vault): add TAbstractFile handle type (Q.0 P1 T1.2)"
```

---

## Task 1.3: `TFile` type + tests

**Files:**
- Create: `libs/vault/include/corbomite/vault/TFile.h`
- Create: `libs/vault/src/TFile.cpp`
- Create: `libs/vault/tests/tst_tfile.cpp`
- Modify: `libs/vault/CMakeLists.txt`
- Modify: `libs/vault/tests/CMakeLists.txt`

- [ ] **Step 1: Write failing test**

Create `libs/vault/tests/tst_tfile.cpp`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include "corbomite/vault/TFile.h"

class TestTFile : public QObject
{
    Q_OBJECT
private slots:
    void basenameAndExtensionDerivedFromCtor();
    void setPathUpdatesBasenameAndExtension();
    void getShortNameMarkdown();
    void getShortNameNonMarkdown();
    void statNullUntilSet();
    void savingDefaultsFalse();
    void extensionIsLowercase();
    void noExtensionHandled();
};

void TestTFile::basenameAndExtensionDerivedFromCtor()
{
    Corbomite::TFile f(nullptr, QStringLiteral("folder/Note.md"));
    QCOMPARE(f.basename, QStringLiteral("Note"));
    QCOMPARE(f.extension, QStringLiteral("md"));
}

void TestTFile::setPathUpdatesBasenameAndExtension()
{
    Corbomite::TFile f(nullptr, QStringLiteral("a.md"));
    f.setPath(QStringLiteral("folder/b.canvas"));
    QCOMPARE(f.basename, QStringLiteral("b"));
    QCOMPARE(f.extension, QStringLiteral("canvas"));
}

void TestTFile::getShortNameMarkdown()
{
    Corbomite::TFile f(nullptr, QStringLiteral("Note.md"));
    QCOMPARE(f.getShortName(), QStringLiteral("Note"));
}

void TestTFile::getShortNameNonMarkdown()
{
    Corbomite::TFile f(nullptr, QStringLiteral("image.png"));
    QCOMPARE(f.getShortName(), QStringLiteral("image.png"));
}

void TestTFile::statNullUntilSet()
{
    Corbomite::TFile f(nullptr, QStringLiteral("a.md"));
    QVERIFY(!f.stat.has_value());
}

void TestTFile::savingDefaultsFalse()
{
    Corbomite::TFile f(nullptr, QStringLiteral("a.md"));
    QCOMPARE(f.saving, false);
}

void TestTFile::extensionIsLowercase()
{
    Corbomite::TFile f(nullptr, QStringLiteral("Note.MD"));
    QCOMPARE(f.extension, QStringLiteral("md"));
}

void TestTFile::noExtensionHandled()
{
    Corbomite::TFile f(nullptr, QStringLiteral("README"));
    QCOMPARE(f.basename, QStringLiteral("README"));
    QCOMPARE(f.extension, QString());
}

QTEST_MAIN(TestTFile)
#include "tst_tfile.moc"
```

- [ ] **Step 2: Register in tests/CMakeLists.txt**

Append:

```cmake
add_executable(tst_tfile tst_tfile.cpp)
target_link_libraries(tst_tfile PRIVATE Qt6::Test Corbomite::Vault)
add_test(NAME tst_tfile COMMAND tst_tfile)
set_tests_properties(tst_tfile PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
```

- [ ] **Step 3: Create header**

Create `libs/vault/include/corbomite/vault/TFile.h`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <optional>

#include "corbomite/vault/TAbstractFile.h"

namespace Corbomite {

struct FileStat {
    qint64 sizeBytes = 0;
    qint64 mtimeMs   = 0;
    qint64 ctimeMs   = 0;
};

/// A file node in the Vault tree. Extends `TAbstractFile` with
/// basename/extension/stat/saving metadata.
class TFile : public TAbstractFile
{
public:
    QString                 basename;         ///< Name without extension.
    QString                 extension;        ///< Lowercase, no leading dot.
    std::optional<FileStat> stat;             ///< Nullopt until first reconcile.
    bool                    saving = false;   ///< Set during in-flight mutations.

    TFile(Vault *v, const QString &p);

    void setPath(const QString &newPath) override;

    /// Returns `basename` for `.md` files, else `name`. Used in UI chrome.
    QString getShortName() const;
};

} // namespace Corbomite
```

- [ ] **Step 4: Create impl**

Create `libs/vault/src/TFile.cpp`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/vault/TFile.h"

#include <QFileInfo>

namespace Corbomite {

namespace {
void deriveBasenameExt(const QString &name, QString &basename, QString &ext)
{
    const int dot = name.lastIndexOf(QLatin1Char('.'));
    if (dot <= 0) {
        basename = name;
        ext.clear();
    } else {
        basename = name.left(dot);
        ext = name.mid(dot + 1).toLower();
    }
}
}

TFile::TFile(Vault *v, const QString &p)
    : TAbstractFile(v, p)
{
    deriveBasenameExt(name, basename, extension);
}

void TFile::setPath(const QString &newPath)
{
    TAbstractFile::setPath(newPath);
    deriveBasenameExt(name, basename, extension);
}

QString TFile::getShortName() const
{
    return extension == QStringLiteral("md") ? basename : name;
}

} // namespace Corbomite
```

- [ ] **Step 5: Register in libs/vault/CMakeLists.txt**

Inside the `add_library(vault STATIC ...)` block, add:

```cmake
    include/corbomite/vault/TFile.h
    src/TFile.cpp
```

- [ ] **Step 6: Build and run**

```bash
cmake --build build
cd build && ctest -R tst_tfile --output-on-failure
```

Expected: all 8 test cases pass.

- [ ] **Step 7: Commit**

```bash
git add libs/vault
git commit -m "feat(vault): add TFile handle type (Q.0 P1 T1.3)"
```

---

## Task 1.4: `TFolder` type + tests + complete `TAbstractFile::getNewPathAfterRename`

**Files:**
- Create: `libs/vault/include/corbomite/vault/TFolder.h`
- Create: `libs/vault/src/TFolder.cpp`
- Create: `libs/vault/tests/tst_tfolder.cpp`
- Modify: `libs/vault/src/TAbstractFile.cpp` (replace deferred impl of `getNewPathAfterRename`)
- Modify: `libs/vault/CMakeLists.txt`
- Modify: `libs/vault/tests/CMakeLists.txt`

- [ ] **Step 1: Write failing test**

Create `libs/vault/tests/tst_tfolder.cpp`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include "corbomite/vault/TFolder.h"
#include "corbomite/vault/TFile.h"

class TestTFolder : public QObject
{
    Q_OBJECT
private slots:
    void rootRecognition();
    void parentPrefixRoot();
    void parentPrefixNonRoot();
    void getNewPathAfterRenameInFolder();
    void stripsControlCharsAndProducesPath();
    void getFileCountRecursive();
    void getFolderCountRecursive();
};

void TestTFolder::rootRecognition()
{
    Corbomite::TFolder root(nullptr, QStringLiteral("/"));
    QCOMPARE(root.isRoot(), true);

    Corbomite::TFolder sub(nullptr, QStringLiteral("folder"));
    QCOMPARE(sub.isRoot(), false);
}

void TestTFolder::parentPrefixRoot()
{
    Corbomite::TFolder root(nullptr, QStringLiteral("/"));
    QCOMPARE(root.getParentPrefix(), QString());
}

void TestTFolder::parentPrefixNonRoot()
{
    Corbomite::TFolder sub(nullptr, QStringLiteral("a/b"));
    QCOMPARE(sub.getParentPrefix(), QStringLiteral("a/b/"));
}

void TestTFolder::getNewPathAfterRenameInFolder()
{
    Corbomite::TFolder parent(nullptr, QStringLiteral("folder"));
    Corbomite::TFile child(nullptr, QStringLiteral("folder/old.md"));
    child.parent = &parent;
    QCOMPARE(child.getNewPathAfterRename(QStringLiteral("new.md")),
             QStringLiteral("folder/new.md"));
}

void TestTFolder::stripsControlCharsAndProducesPath()
{
    Corbomite::TFolder parent(nullptr, QStringLiteral("x"));
    Corbomite::TFile child(nullptr, QStringLiteral("x/a.md"));
    child.parent = &parent;
    QCOMPARE(child.getNewPathAfterRename(QStringLiteral(" b\x01c.md ")),
             QStringLiteral("x/bc.md"));
}

void TestTFolder::getFileCountRecursive()
{
    Corbomite::TFolder root(nullptr, QStringLiteral("/"));
    Corbomite::TFolder sub(nullptr, QStringLiteral("sub"));
    sub.parent = &root;
    root.children.append(&sub);

    Corbomite::TFile a(nullptr, QStringLiteral("a.md"));
    a.parent = &root;
    root.children.append(&a);

    Corbomite::TFile b(nullptr, QStringLiteral("sub/b.md"));
    b.parent = &sub;
    sub.children.append(&b);

    QCOMPARE(root.getFileCount(), 2);
    QCOMPARE(sub.getFileCount(), 1);
}

void TestTFolder::getFolderCountRecursive()
{
    Corbomite::TFolder root(nullptr, QStringLiteral("/"));
    Corbomite::TFolder s1(nullptr, QStringLiteral("s1"));
    Corbomite::TFolder s2(nullptr, QStringLiteral("s1/s2"));
    s1.parent = &root; root.children.append(&s1);
    s2.parent = &s1;   s1.children.append(&s2);

    QCOMPARE(root.getFolderCount(), 2);
    QCOMPARE(s1.getFolderCount(), 1);
}

QTEST_MAIN(TestTFolder)
#include "tst_tfolder.moc"
```

- [ ] **Step 2: Register in tests/CMakeLists.txt**

Append:

```cmake
add_executable(tst_tfolder tst_tfolder.cpp)
target_link_libraries(tst_tfolder PRIVATE Qt6::Test Corbomite::Vault)
add_test(NAME tst_tfolder COMMAND tst_tfolder)
set_tests_properties(tst_tfolder PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
```

- [ ] **Step 3: Create header**

Create `libs/vault/include/corbomite/vault/TFolder.h`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QList>

#include "corbomite/vault/TAbstractFile.h"

namespace Corbomite {

/// A folder node in the Vault tree.
class TFolder : public TAbstractFile
{
public:
    QList<TAbstractFile *> children;  ///< Non-owning; Vault owns entries.

    TFolder(Vault *v, const QString &p);

    bool    isRoot() const { return path == QStringLiteral("/"); }
    QString getParentPrefix() const;  ///< "" for root, else path+"/".

    int getFileCount() const;          ///< Recursive count of TFile descendants.
    int getFolderCount() const;        ///< Recursive count of TFolder descendants.
};

} // namespace Corbomite
```

- [ ] **Step 4: Create impl**

Create `libs/vault/src/TFolder.cpp`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/vault/TFolder.h"
#include "corbomite/vault/TFile.h"

namespace Corbomite {

TFolder::TFolder(Vault *v, const QString &p)
    : TAbstractFile(v, p)
{
}

QString TFolder::getParentPrefix() const
{
    return isRoot() ? QString() : (path + QLatin1Char('/'));
}

int TFolder::getFileCount() const
{
    int n = 0;
    for (const TAbstractFile *c : children) {
        if (const auto *sub = dynamic_cast<const TFolder *>(c)) {
            n += sub->getFileCount();
        } else if (dynamic_cast<const TFile *>(c)) {
            ++n;
        }
    }
    return n;
}

int TFolder::getFolderCount() const
{
    int n = 0;
    for (const TAbstractFile *c : children) {
        if (const auto *sub = dynamic_cast<const TFolder *>(c)) {
            ++n;
            n += sub->getFolderCount();
        }
    }
    return n;
}

} // namespace Corbomite
```

- [ ] **Step 5: Complete `TAbstractFile::getNewPathAfterRename`**

Edit `libs/vault/src/TAbstractFile.cpp`. Replace the body of
`getNewPathAfterRename` with:

```cpp
QString TAbstractFile::getNewPathAfterRename(const QString &newName) const
{
    if (!parent) return {};

    QString cleaned = newName;
    cleaned.remove(QRegularExpression(QStringLiteral("[\\x00-\\x1F]")));
    cleaned = cleaned.trimmed();
    if (cleaned.isEmpty()) return {};

    return parent->getParentPrefix() + cleaned;
}
```

Add `#include "corbomite/vault/TFolder.h"` at the top of the .cpp (fine
because header is included, not the .cpp file itself).

- [ ] **Step 6: Register TFolder source in libs/vault/CMakeLists.txt**

Inside the `add_library(vault STATIC ...)` block, add:

```cmake
    include/corbomite/vault/TFolder.h
    src/TFolder.cpp
```

- [ ] **Step 7: Build and run**

```bash
cmake --build build
cd build && ctest -R "tst_tfolder|tst_tabstractfile" --output-on-failure
```

Expected: `tst_tfolder` passes 7 cases; `tst_tabstractfile` still 6 cases
green (the `stripsControlChars` case still tests the detached path).

- [ ] **Step 8: Commit**

```bash
git add libs/vault
git commit -m "feat(vault): add TFolder + complete TAbstractFile rename logic (Q.0 P1 T1.4)"
```

---

## Task 1.5: Skeletal `Vault` class (load/unload/getRoot/getAbstractFileByPath) + tests

**Files:**
- Create: `libs/vault/include/corbomite/vault/Vault.h`
- Create: `libs/vault/src/Vault.cpp`
- Create: `libs/vault/src/PathNormalization.h` (NFC helper, internal)
- Create: `libs/vault/src/PathNormalization.cpp`
- Create: `libs/vault/tests/tst_vault_skeleton.cpp`
- Modify: `libs/vault/CMakeLists.txt`
- Modify: `libs/vault/tests/CMakeLists.txt`

- [ ] **Step 1: Write failing test**

Create `libs/vault/tests/tst_vault_skeleton.cpp`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>

#include "corbomite/vault/Vault.h"
#include "corbomite/vault/TFile.h"
#include "corbomite/vault/TFolder.h"
#include "corbomite/storage/FileSystemAdapter.h"

class TestVaultSkeleton : public QObject
{
    Q_OBJECT
private slots:
    void emptyVaultHasRoot();
    void loadBuildsTree();
    void getAbstractFileByPath();
    void isEmptyTrueWhenOnlyRoot();
    void unloadClearsTree();
    void getNameIsBasenameOfBasePath();
};

namespace {
void writeFile(const QString &path, const QByteArray &body)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path); f.open(QIODevice::WriteOnly); f.write(body);
}
}

void TestVaultSkeleton::emptyVaultHasRoot()
{
    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    QVERIFY(vault.getRoot() != nullptr);
    QCOMPARE(vault.getRoot()->isRoot(), true);
}

void TestVaultSkeleton::loadBuildsTree()
{
    QTemporaryDir dir;
    writeFile(dir.path() + "/a.md", "a");
    writeFile(dir.path() + "/sub/b.md", "b");

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    QVERIFY(vault.getRoot() != nullptr);
    QCOMPARE(vault.getFiles().size(), 2);
    QCOMPARE(vault.getMarkdownFiles().size(), 2);
}

void TestVaultSkeleton::getAbstractFileByPath()
{
    QTemporaryDir dir;
    writeFile(dir.path() + "/a.md", "a");

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    auto *f = vault.getAbstractFileByPath(QStringLiteral("a.md"));
    QVERIFY(f != nullptr);
    QCOMPARE(f->name, QStringLiteral("a.md"));

    QCOMPARE(vault.getAbstractFileByPath(QStringLiteral("missing.md")),
             static_cast<Corbomite::TAbstractFile *>(nullptr));
}

void TestVaultSkeleton::isEmptyTrueWhenOnlyRoot()
{
    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    QCOMPARE(vault.isEmpty(), true);

    QTemporaryDir dir;
    vault.load(dir.path());
    QCOMPARE(vault.isEmpty(), true);

    writeFile(dir.path() + "/a.md", "a");
    vault.load(dir.path());  // reload
    QCOMPARE(vault.isEmpty(), false);
}

void TestVaultSkeleton::unloadClearsTree()
{
    QTemporaryDir dir;
    writeFile(dir.path() + "/a.md", "a");

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());
    QVERIFY(vault.isLoaded());

    vault.unload();
    QCOMPARE(vault.isLoaded(), false);
    QCOMPARE(vault.isEmpty(), true);
}

void TestVaultSkeleton::getNameIsBasenameOfBasePath()
{
    QTemporaryDir dir;
    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());
    QCOMPARE(vault.getName(), QFileInfo(dir.path()).fileName());
}

QTEST_MAIN(TestVaultSkeleton)
#include "tst_vault_skeleton.moc"
```

- [ ] **Step 2: Register test**

Append to `libs/vault/tests/CMakeLists.txt`:

```cmake
add_executable(tst_vault_skeleton tst_vault_skeleton.cpp)
target_link_libraries(tst_vault_skeleton
    PRIVATE Qt6::Test Corbomite::Vault Corbomite::Storage)
add_test(NAME tst_vault_skeleton COMMAND tst_vault_skeleton)
set_tests_properties(tst_vault_skeleton PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
```

- [ ] **Step 3: Create PathNormalization helper header**

Create `libs/vault/src/PathNormalization.h`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>

namespace Corbomite::Vault::Paths {

/// NFC-normalize + collapse backslashes to forward + collapse consecutive
/// slashes + trim trailing slash (except at root). Mirrors Obsidian's
/// normalizePath.
QString normalize(const QString &input);

} // namespace Corbomite::Vault::Paths
```

- [ ] **Step 4: Create PathNormalization impl**

Create `libs/vault/src/PathNormalization.cpp`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "PathNormalization.h"

namespace Corbomite::Vault::Paths {

QString normalize(const QString &input)
{
    QString s = input;
    s.replace(QLatin1Char('\\'), QLatin1Char('/'));
    while (s.contains(QStringLiteral("//"))) {
        s.replace(QStringLiteral("//"), QStringLiteral("/"));
    }
    if (s.length() > 1 && s.endsWith(QLatin1Char('/'))) {
        s.chop(1);
    }
    if (s.startsWith(QStringLiteral("./"))) s.remove(0, 2);
    return s.normalized(QString::NormalizationForm_C);
}

} // namespace Corbomite::Vault::Paths
```

- [ ] **Step 5: Create skeletal Vault header**

Create `libs/vault/include/corbomite/vault/Vault.h`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <memory>

#include <QHash>
#include <QObject>
#include <QString>
#include <QVector>

namespace Corbomite {

class DataAdapter;
class TAbstractFile;
class TFile;
class TFolder;

/// Canonical vault aggregate. Single-vault-per-process. Owns the
/// TFile/TFolder tree, composes a DataAdapter for file I/O, and emits
/// signals on every mutation. See
/// `docs/superpowers/specs/2026-04-16-vault-architecture-design.md`.
///
/// Task 1.5 ships the skeletal surface only: load/unload + tree queries.
/// Read/mutate + watcher + config-I/O + event firing arrive in subsequent
/// tasks.
class Vault : public QObject
{
    Q_OBJECT
public:
    explicit Vault(DataAdapter *adapter, QObject *parent = nullptr);
    ~Vault() override;

    // ---- Lifecycle ----
    void    load(const QString &basePath);
    void    unload();
    bool    isLoaded() const;
    QString getName() const;
    QString basePath() const;

    // ---- Tree queries ----
    TFolder        *getRoot() const;
    TAbstractFile  *getAbstractFileByPath(const QString &path) const;
    TFile          *getFileByPath(const QString &path) const;
    TFolder        *getFolderByPath(const QString &path) const;
    QVector<TFile *>         getMarkdownFiles() const;
    QVector<TFile *>         getFiles() const;
    QVector<TAbstractFile *> getAllLoadedFiles() const;
    bool    isEmpty() const;

private:
    DataAdapter *m_adapter;
    QString      m_basePath;
    bool         m_loaded = false;

    // Owned; keyed by NFC-normalized path. "/" is always present after load.
    QHash<QString, std::unique_ptr<TAbstractFile>> m_fileMap;

    // Non-owning shortcut to the root node (also in m_fileMap at "/").
    TFolder *m_root = nullptr;

    void buildTree();
    void teardownTree();
};

} // namespace Corbomite
```

- [ ] **Step 6: Create Vault impl**

Create `libs/vault/src/Vault.cpp`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/vault/Vault.h"

#include "corbomite/vault/TAbstractFile.h"
#include "corbomite/vault/TFile.h"
#include "corbomite/vault/TFolder.h"
#include "corbomite/storage/DataAdapter.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>

#include "PathNormalization.h"

namespace Corbomite {

Vault::Vault(DataAdapter *adapter, QObject *parent)
    : QObject(parent)
    , m_adapter(adapter)
{
    // Always have a root.
    auto root = std::make_unique<TFolder>(this, QStringLiteral("/"));
    m_root = root.get();
    m_fileMap.insert(QStringLiteral("/"), std::move(root));
}

Vault::~Vault() = default;

void Vault::load(const QString &basePath)
{
    unload();
    m_basePath = QDir::cleanPath(basePath);
    buildTree();
    m_loaded = true;
}

void Vault::unload()
{
    teardownTree();
    m_basePath.clear();
    m_loaded = false;
}

bool Vault::isLoaded() const { return m_loaded; }

QString Vault::getName() const
{
    return QFileInfo(m_basePath).fileName();
}

QString Vault::basePath() const { return m_basePath; }

TFolder *Vault::getRoot() const { return m_root; }

TAbstractFile *Vault::getAbstractFileByPath(const QString &path) const
{
    const auto it = m_fileMap.find(Vault::Paths::normalize(path));
    return it == m_fileMap.end() ? nullptr : it->get();
}

TFile *Vault::getFileByPath(const QString &path) const
{
    return dynamic_cast<TFile *>(getAbstractFileByPath(path));
}

TFolder *Vault::getFolderByPath(const QString &path) const
{
    return dynamic_cast<TFolder *>(getAbstractFileByPath(path));
}

QVector<TFile *> Vault::getMarkdownFiles() const
{
    QVector<TFile *> out;
    for (const auto &[k, v] : m_fileMap.asKeyValueRange()) {
        if (auto *f = dynamic_cast<TFile *>(v.get())) {
            if (f->extension == QStringLiteral("md")) out.append(f);
        }
    }
    return out;
}

QVector<TFile *> Vault::getFiles() const
{
    QVector<TFile *> out;
    for (const auto &[k, v] : m_fileMap.asKeyValueRange()) {
        if (auto *f = dynamic_cast<TFile *>(v.get())) out.append(f);
    }
    return out;
}

QVector<TAbstractFile *> Vault::getAllLoadedFiles() const
{
    QVector<TAbstractFile *> out;
    out.reserve(m_fileMap.size());
    for (const auto &[k, v] : m_fileMap.asKeyValueRange()) {
        if (k != QStringLiteral("/")) out.append(v.get());
    }
    return out;
}

bool Vault::isEmpty() const { return m_fileMap.size() <= 1; }

void Vault::buildTree()
{
    if (m_basePath.isEmpty()) return;

    QDirIterator it(m_basePath,
                    QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString abs = it.next();
        QFileInfo fi(abs);
        // Skip .obsidian + .corbomite internal dirs for initial tree — they
        // show up via config-I/O paths not tree queries.
        QString rel = Vault::Paths::normalize(QDir(m_basePath).relativeFilePath(abs));
        if (rel.startsWith(QStringLiteral(".obsidian/")) ||
            rel == QStringLiteral(".obsidian") ||
            rel.startsWith(QStringLiteral(".corbomite/")) ||
            rel == QStringLiteral(".corbomite") ||
            rel.startsWith(QStringLiteral(".trash/")) ||
            rel == QStringLiteral(".trash")) {
            continue;
        }

        if (fi.isDir()) {
            auto folder = std::make_unique<TFolder>(this, rel);
            m_fileMap.insert(rel, std::move(folder));
        } else if (fi.isFile()) {
            auto file = std::make_unique<TFile>(this, rel);
            // Attach basic stat (adapter.stat via absolute path helper
            // would be richer; good enough for skeleton).
            FileStat stat;
            stat.sizeBytes = fi.size();
            stat.mtimeMs   = fi.lastModified().toMSecsSinceEpoch();
            stat.ctimeMs   = fi.birthTime().toMSecsSinceEpoch();
            file->stat     = stat;
            m_fileMap.insert(rel, std::move(file));
        }
    }
    // Parent/children wiring happens in a second pass once all nodes exist.
    // (Simpler than tracking order during construction.)
    for (auto &[k, v] : m_fileMap.asKeyValueRange()) {
        if (k == QStringLiteral("/")) continue;
        const int slash = k.lastIndexOf(QLatin1Char('/'));
        TFolder *parent = m_root;
        if (slash > 0) {
            auto pit = m_fileMap.find(k.left(slash));
            if (pit != m_fileMap.end()) {
                parent = dynamic_cast<TFolder *>(pit->get());
            }
        }
        if (parent) {
            v->parent = parent;
            parent->children.append(v.get());
        }
    }
}

void Vault::teardownTree()
{
    // Drop everything except the root; recreate a fresh empty root so the
    // Vault-has-a-root invariant holds even while unloaded.
    m_fileMap.clear();
    auto root = std::make_unique<TFolder>(this, QStringLiteral("/"));
    m_root = root.get();
    m_fileMap.insert(QStringLiteral("/"), std::move(root));
}

} // namespace Corbomite
```

- [ ] **Step 7: Register sources in libs/vault/CMakeLists.txt**

Inside the `add_library(vault STATIC ...)` block, add:

```cmake
    include/corbomite/vault/Vault.h
    src/Vault.cpp
    src/PathNormalization.h
    src/PathNormalization.cpp
```

- [ ] **Step 8: Build and run**

```bash
cmake --build build
cd build && ctest -R tst_vault_skeleton --output-on-failure
```

Expected: all 6 cases pass.

- [ ] **Step 9: Commit**

```bash
git add libs/vault
git commit -m "feat(vault): add skeletal Vault class (Q.0 P1 T1.5)"
```

---

## Task 1.6: Delete the Task-7 `Corbomite::Vault` in `libs/core/`

**Files:**
- Delete: `libs/core/include/corbomite/core/Vault.h`
- Delete: `libs/core/src/Vault.cpp`
- Modify: `libs/core/CMakeLists.txt` (remove `Vault.h` + `Vault.cpp` from sources)
- Delete: any `tests/core/tst_vault.cpp` if one exists (check with `ls tests/core/`)

- [ ] **Step 1: Locate references outside libs/core that include the old Vault header**

```bash
grep -rn "corbomite/core/Vault.h" libs/ src/ tests/ 2>/dev/null
```

Record any matches. Expected (before the delete): `libs/core/src/proxies/VaultReader.cpp`, `libs/core/src/proxies/VaultWriter.cpp`, `libs/core/include/corbomite/core/PluginContext.h`, `libs/core/src/PluginContext.cpp`. These all get dealt with in Task 1.7.

- [ ] **Step 2: Delete the header**

```bash
rm libs/core/include/corbomite/core/Vault.h
```

- [ ] **Step 3: Delete the impl**

```bash
rm libs/core/src/Vault.cpp
```

- [ ] **Step 4: Remove from libs/core/CMakeLists.txt**

Edit `libs/core/CMakeLists.txt`. Remove the lines referencing `Vault.h` and `Vault.cpp` from the `target_sources(corbomite-core PRIVATE ...)` or equivalent block. Verify the file still has a valid source list.

- [ ] **Step 5: Check for any `tst_vault` test under tests/core**

```bash
ls tests/core/ 2>/dev/null | grep -i vault
```

If any results reference the deleted core/Vault, delete them and their CMakeLists.txt entries in the same commit.

- [ ] **Step 6: Build and expect failures from the proxy .cpp files**

```bash
cmake --build build 2>&1 | grep -E "fatal error|error:" | head
```

Expected: compile errors from `libs/core/src/proxies/VaultReader.cpp` and `VaultWriter.cpp` (header gone). These will be fixed in Task 1.7.

- [ ] **Step 7: Commit the deletion**

```bash
git add -u libs/core
git commit -m "chore(core): delete Task-7 Corbomite::Vault stub (Q.0 P1 T1.6)"
```

Note: the tree is briefly broken after this commit. Task 1.7 restores it.

---

## Task 1.7: Delete `VaultReader` + `VaultWriter` proxies; update `PluginContext`

**Files:**
- Delete: `libs/core/include/corbomite/core/proxies/VaultReader.h`
- Delete: `libs/core/src/proxies/VaultReader.cpp`
- Delete: `libs/core/include/corbomite/core/proxies/VaultWriter.h`
- Delete: `libs/core/src/proxies/VaultWriter.cpp`
- Delete: `tests/core/tst_proxy_vault.cpp` (if it exists)
- Modify: `libs/core/include/corbomite/core/PluginContext.h` (drop `Vault *`/VaultReader/VaultWriter members, accessors, and setCoreServices `Vault *` param)
- Modify: `libs/core/src/PluginContext.cpp` (drop proxy construction + destruction)
- Modify: `libs/core/CMakeLists.txt` (drop proxy sources)
- Modify: `tests/core/CMakeLists.txt` (drop `tst_proxy_vault` if it was present)
- Modify: `src/app/CorbomiteApp.cpp` (if it exists) or wherever `setCoreServices` is called — drop the `Vault *` argument at all call sites

- [ ] **Step 1: Locate all callers of `setCoreServices`**

```bash
grep -rn "setCoreServices" libs/ src/ tests/ 2>/dev/null
```

Record each call site. Each must drop the `Vault *` argument.

- [ ] **Step 2: Delete proxy sources**

```bash
rm libs/core/include/corbomite/core/proxies/VaultReader.h \
   libs/core/src/proxies/VaultReader.cpp \
   libs/core/include/corbomite/core/proxies/VaultWriter.h \
   libs/core/src/proxies/VaultWriter.cpp
```

- [ ] **Step 3: Delete the corresponding test if present**

```bash
[ -f tests/core/tst_proxy_vault.cpp ] && rm tests/core/tst_proxy_vault.cpp
```

- [ ] **Step 4: Update libs/core/CMakeLists.txt**

Remove the four proxy source file entries from the `target_sources` block.

- [ ] **Step 5: Update tests/core/CMakeLists.txt**

Remove the `tst_proxy_vault` executable registration + `add_test` entry if present.

- [ ] **Step 6: Modify PluginContext.h**

Edit `libs/core/include/corbomite/core/PluginContext.h`:

- Remove the forward-decl lines for `Vault`, `VaultReader`, `VaultWriter`.
- Remove `#include`-adjacent text mentioning Vault.
- Change the `setCoreServices` signature from

  ```cpp
  void setCoreServices(Vault *vault,
                       MetadataCache *metadata, Workspace *workspace,
                       CommandRegistry *commands, ViewRegistry *views,
                       MenuEventEmitter *menus, QNetworkAccessManager *network);
  ```
  to
  ```cpp
  void setCoreServices(MetadataCache *metadata, Workspace *workspace,
                       CommandRegistry *commands, ViewRegistry *views,
                       MenuEventEmitter *menus, QNetworkAccessManager *network);
  ```
- Remove the `VaultReader *vaultReader() const;` and `VaultWriter *vaultWriter() const;` accessor declarations.
- Remove `mutable VaultReader *m_vaultReader = nullptr;` and `mutable VaultWriter *m_vaultWriter = nullptr;` members.
- Remove `Vault *m_vault = nullptr;` member.

- [ ] **Step 7: Modify PluginContext.cpp**

Edit `libs/core/src/PluginContext.cpp`:

- Remove `#include "corbomite/core/proxies/VaultReader.h"` and `VaultWriter.h`.
- Remove the `constexpr auto kVaultRead = "vault.read";` and `kVaultWrite = "vault.write";` lines (put back in Phase 9 Task 9.x when the new proxies land — for now they're dead).
- Drop the `delete m_vaultReader;` and `delete m_vaultWriter;` lines from the destructor.
- Update `setCoreServices` body to drop the `m_vault = v;` assignment and remove the `v` parameter.
- Remove the `VaultReader *PluginContext::vaultReader() const { ... }` function body.
- Remove the `VaultWriter *PluginContext::vaultWriter() const { ... }` function body.

- [ ] **Step 8: Update `setCoreServices` call sites**

For each match from Step 1, remove the `Vault *` / `&vault` / similar argument. Typical spot is `src/app/CorbomiteApp.cpp` or `MainWindow.cpp`. The call becomes e.g. `ctx->setCoreServices(metadata, workspace, commands, views, menus, network);`.

- [ ] **Step 9: Build**

```bash
cmake --build build
```

Expected: clean build. `tst_plugin_context.cpp` may fail if it exercised the old signature — open it and remove the Vault-related test cases (they'll be restored in Phase 9). If the whole test stops compiling for an unrelated reason, back off and re-read.

- [ ] **Step 10: Run tests**

```bash
cd build && ctest --output-on-failure -E "tst_markoff_inline_math|tst_renderengine|tst_completion_popup|tst_benchmark_layout"
```

Expected: all tests pass outside the 4 known-flaky.

- [ ] **Step 11: Commit**

```bash
git add -u libs/core tests/core src
git commit -m "chore(core): drop VaultReader/VaultWriter proxies (Q.0 P1 T1.7)"
```

---

## Task 1.8: Phase 1 verification + update PROJECT-STATE + memory

**Files:**
- Modify: `docs/PROJECT-STATE.md` (update Current focus + add Recent decisions entry)
- Modify: `docs/superpowers/plans/INDEX.md` (add plan link)
- Modify: `/home/clinton/.claude/projects/-home-clinton-dev-Corbomite/memory/project_vault_tension.md` (mark in-flight)

- [ ] **Step 1: Full rebuild**

```bash
rm -rf build && cmake -S . -B build -DCORBOMITE_DEV_BUILD=ON && cmake --build build
```

Expected: clean build.

- [ ] **Step 2: Full test run**

```bash
cd build && ctest --output-on-failure -E "tst_markoff_inline_math|tst_renderengine|tst_completion_popup|tst_benchmark_layout"
```

Expected: all tests green.

- [ ] **Step 3: Update PROJECT-STATE.md**

Edit `docs/PROJECT-STATE.md`:

- Update `**Last updated:**` timestamp.
- Under `## Current focus`, replace the Cluster Q line with a Cluster Q.0 line noting Phase 1 complete + next phase.
- Prepend a new bullet under `## Recent decisions` describing Phase 1 completion, commits touched, tests added.

- [ ] **Step 4: Update plan index**

Append to `docs/superpowers/plans/INDEX.md`:

```markdown
- [2026-04-16 — Cluster Q.0 Vault architecture refactor](2026-04-16-cluster-q0-vault-architecture.md)
```

- [ ] **Step 5: Commit**

```bash
git add docs/PROJECT-STATE.md docs/superpowers/plans/INDEX.md
git commit -m "docs: Cluster Q.0 Phase 1 landed (Q.0 P1 T1.8)"
```

---

# Phase 2 — DataAdapter ownership + file watcher

Goal: Move `VaultScanner` into `libs/vault/`. Fold `FileWatchReactor` into a private `Vault::Watcher`. `Vault` composes a `DataAdapter *` (non-owning). `Vault::load` builds the tree via the scanner + starts the watcher; signals `created` / `modified` / `deletedFile` / `renamed` fire on external filesystem events.

## Task 2.1: Move `VaultScanner` from `libs/storage/` to `libs/vault/`

**Files:**
- Move: `libs/storage/include/corbomite/storage/VaultScanner.h` → `libs/vault/include/corbomite/vault/VaultScanner.h`
- Move: `libs/storage/src/VaultScanner.cpp` → `libs/vault/src/VaultScanner.cpp`
- Modify: `libs/storage/CMakeLists.txt` (remove scanner sources)
- Modify: `libs/vault/CMakeLists.txt` (add scanner sources)
- Modify: every consumer's `#include "corbomite/storage/VaultScanner.h"` → `#include "corbomite/vault/VaultScanner.h"`
- Modify: `libs/storage/` callers in source — replace include path
- Modify: namespace stays `Corbomite::` (no rename required)

- [ ] **Step 1: Identify callers**

```bash
grep -rn "corbomite/storage/VaultScanner.h" libs/ src/ tests/ 2>/dev/null
```

Record matches (VaultModel, tests, etc.).

- [ ] **Step 2: Move the header**

```bash
git mv libs/storage/include/corbomite/storage/VaultScanner.h \
       libs/vault/include/corbomite/vault/VaultScanner.h
```

- [ ] **Step 3: Move the impl**

```bash
git mv libs/storage/src/VaultScanner.cpp libs/vault/src/VaultScanner.cpp
```

- [ ] **Step 4: Update libs/storage/CMakeLists.txt**

Remove `VaultScanner.h` and `VaultScanner.cpp` entries.

- [ ] **Step 5: Update libs/vault/CMakeLists.txt**

Add inside `add_library(vault STATIC ...)`:

```cmake
    include/corbomite/vault/VaultScanner.h
    src/VaultScanner.cpp
```

- [ ] **Step 6: Update every #include from callers**

For each match from Step 1, replace `#include "corbomite/storage/VaultScanner.h"` with `#include "corbomite/vault/VaultScanner.h"`.

- [ ] **Step 7: Add link dep for storage callers**

If `libs/storage` itself still uses VaultScanner, it must depend on `libs/vault` — but that creates a dep cycle (vault already depends on storage). Resolve: callers inside `libs/storage` that use VaultScanner must move out of storage (likely they already need to: `VaultScanner` consumer in storage is probably `MetadataCache`'s scan path). Check:

```bash
grep -rn "VaultScanner" libs/storage/
```

If a storage-internal caller exists, it migrates to libs/vault in this task. Flag during execution for a decision.

- [ ] **Step 8: Build**

```bash
cmake --build build
```

- [ ] **Step 9: Test**

```bash
cd build && ctest --output-on-failure -E "tst_markoff_inline_math|tst_renderengine|tst_completion_popup|tst_benchmark_layout"
```

- [ ] **Step 10: Commit**

```bash
git add -u libs src tests
git commit -m "refactor(vault): move VaultScanner from storage to vault (Q.0 P2 T2.1)"
```

---

## Task 2.2: Move `FileWatchReactor` into `libs/vault/` as private `Watcher`

**Files:**
- Move: `src/reactors/FileWatchReactor.h` → `libs/vault/src/Watcher.h` (private header)
- Move: `src/reactors/FileWatchReactor.cpp` → `libs/vault/src/Watcher.cpp`
- Rename: class `FileWatchReactor` → `Vault::Watcher` (nested private class)
- Modify: `src/CMakeLists.txt` (remove reactor sources; remove subdir if now-empty)
- Modify: `libs/vault/CMakeLists.txt` (add Watcher sources)
- Modify: every caller of `FileWatchReactor` — migrate access to go through `Vault` instead (the public callers will be moved to Vault's public API later; for now make the callers compile by removing their direct dependency — they'll re-appear in Phase 6/7 migration)

- [ ] **Step 1: Identify callers**

```bash
grep -rn "FileWatchReactor" libs/ src/ tests/ 2>/dev/null
```

- [ ] **Step 2: Move header + impl**

```bash
git mv src/reactors/FileWatchReactor.h libs/vault/src/Watcher.h
git mv src/reactors/FileWatchReactor.cpp libs/vault/src/Watcher.cpp
```

- [ ] **Step 3: Rename the class in its new location**

Edit `libs/vault/src/Watcher.h`:

- Change `class FileWatchReactor : public QObject` to `class Watcher : public QObject`.
- Wrap in `namespace Corbomite { class Vault; namespace detail {` and close matching. The class is `Corbomite::detail::Watcher`.
- Change the constructor signature from `FileWatchReactor(VaultModel *, QObject *parent)` to `Watcher(Vault *, QObject *parent)`. The Vault*-based implementation will arrive in Task 2.3 — for this task we just rename and stub the body so it compiles.

Edit `libs/vault/src/Watcher.cpp`: match header changes. Replace any `m_vault->addNote(...)` / `m_vault->removeNote(...)` with stubbed TODO comments referencing Task 2.4.

- [ ] **Step 4: Remove src/reactors subdirectory if now empty**

```bash
rmdir src/reactors 2>/dev/null
```

Check `src/CMakeLists.txt` — if `add_subdirectory(reactors)` or similar existed, remove that line.

- [ ] **Step 5: Register Watcher in libs/vault/CMakeLists.txt**

Add to the source list:

```cmake
    src/Watcher.h
    src/Watcher.cpp
```

- [ ] **Step 6: Delete/stub the external callers**

For each match from Step 1 outside `src/reactors`:

- If the caller is in `src/app/MainWindow.cpp` constructing a `FileWatchReactor`, replace with a `// TODO Q.0 P7: wire through Vault` comment. Remove the `#include`.
- Tests referencing `FileWatchReactor` directly: mark them `QSKIP("Q.0 P7 migration in progress")` at the top.

This creates a brief gap in the file-watcher pipeline. Acceptable because Phase 7 restores it via the public `Vault::Watcher` interface.

- [ ] **Step 7: Build**

```bash
cmake --build build
```

- [ ] **Step 8: Test**

Expected: broad pass; watcher-specific tests skip. If any test fails non-trivially, investigate — do not silence.

- [ ] **Step 9: Commit**

```bash
git add -u libs src tests
git commit -m "refactor(vault): move FileWatchReactor into libs/vault as Watcher (Q.0 P2 T2.2)"
```

---

## Task 2.3: Wire `DataAdapter *` into Vault + tests for adapter round-trip

**Files:**
- Modify: `libs/vault/include/corbomite/vault/Vault.h` (no ctor change — already takes `DataAdapter *`; this task exercises it)
- Modify: `libs/vault/src/Vault.cpp` (replace direct `QFile` reads inside buildTree with adapter calls)
- Create: `libs/vault/tests/tst_vault_adapter.cpp`
- Modify: `libs/vault/tests/CMakeLists.txt`

- [ ] **Step 1: Write failing test**

Create `libs/vault/tests/tst_vault_adapter.cpp`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>

#include "corbomite/vault/Vault.h"
#include "corbomite/vault/TFile.h"
#include "corbomite/storage/DataAdapter.h"

class TestVaultAdapter : public QObject
{
    Q_OBJECT
private slots:
    void buildTreeUsesAdapter();
};

namespace {
class RecordingAdapter : public Corbomite::DataAdapter
{
public:
    mutable int statCalls = 0;
    mutable int listCalls = 0;
    bool exists(const QString &) const override { return true; }
    std::optional<QString> read(const QString &) const override { return {}; }
    std::optional<QByteArray> readBinary(const QString &) const override { return {}; }
    Corbomite::FileStat stat(const QString &) const override
    {
        ++statCalls;
        return Corbomite::FileStat{true, false, true, 0, 0, 0};
    }
    QStringList list(const QString &dir) const override
    {
        ++listCalls;
        return QDir(dir).entryList(
            QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    }
    bool write(const QString &, const QString &,
               const Corbomite::WriteHints & = {}) override { return false; }
    bool writeBinary(const QString &, const QByteArray &,
                     const Corbomite::WriteHints & = {}) override { return false; }
    bool rename(const QString &, const QString &) override { return false; }
    bool remove(const QString &) override { return false; }
    bool rmdir(const QString &) override { return false; }
    bool mkpath(const QString &) override { return true; }
    bool moveToTrash(const QString &) override { return false; }
};
}

void TestVaultAdapter::buildTreeUsesAdapter()
{
    QTemporaryDir dir;
    QFile f(dir.path() + "/a.md");
    f.open(QIODevice::WriteOnly); f.write("a"); f.close();

    RecordingAdapter adapter;
    Corbomite::Vault vault(&adapter);
    vault.load(dir.path());

    QVERIFY(adapter.listCalls > 0);
}

QTEST_MAIN(TestVaultAdapter)
#include "tst_vault_adapter.moc"
```

- [ ] **Step 2: Register test**

Append to `libs/vault/tests/CMakeLists.txt`:

```cmake
add_executable(tst_vault_adapter tst_vault_adapter.cpp)
target_link_libraries(tst_vault_adapter
    PRIVATE Qt6::Test Corbomite::Vault Corbomite::Storage)
add_test(NAME tst_vault_adapter COMMAND tst_vault_adapter)
set_tests_properties(tst_vault_adapter PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
```

- [ ] **Step 3: Replace direct `QDirIterator` usage in Vault::buildTree with adapter calls**

Edit `libs/vault/src/Vault.cpp`. Replace the body of `buildTree()` with an adapter-driven recursive walk:

```cpp
void Vault::buildTree()
{
    if (m_basePath.isEmpty() || !m_adapter) return;

    std::function<void(const QString &, TFolder *)> walk =
        [&](const QString &absDir, TFolder *parent) {
            const QStringList entries = m_adapter->list(absDir);
            for (const QString &entry : entries) {
                const QString absChild = absDir + QLatin1Char('/') + entry;
                const QString rel = Vault::Paths::normalize(
                    QDir(m_basePath).relativeFilePath(absChild));
                if (rel.startsWith(QStringLiteral(".obsidian/")) ||
                    rel == QStringLiteral(".obsidian") ||
                    rel.startsWith(QStringLiteral(".corbomite/")) ||
                    rel == QStringLiteral(".corbomite") ||
                    rel.startsWith(QStringLiteral(".trash/")) ||
                    rel == QStringLiteral(".trash")) {
                    continue;
                }

                const FileStat st = m_adapter->stat(absChild);
                if (st.isDirectory) {
                    auto folder = std::make_unique<TFolder>(this, rel);
                    folder->parent = parent;
                    parent->children.append(folder.get());
                    TFolder *raw = folder.get();
                    m_fileMap.insert(rel, std::move(folder));
                    walk(absChild, raw);
                } else if (st.isFile) {
                    auto file = std::make_unique<TFile>(this, rel);
                    file->parent = parent;
                    FileStat fs;
                    fs.sizeBytes = st.sizeBytes;
                    fs.mtimeMs   = st.mtimeMs;
                    fs.ctimeMs   = st.ctimeMs;
                    file->stat   = fs;
                    parent->children.append(file.get());
                    m_fileMap.insert(rel, std::move(file));
                }
            }
        };
    walk(m_basePath, m_root);
}
```

Remove the second-pass parent/children wiring (now done inline). Keep the include of `DataAdapter.h` + `<functional>`.

- [ ] **Step 4: Build + run skeleton test (regression guard)**

```bash
cmake --build build
cd build && ctest -R "tst_vault_skeleton|tst_vault_adapter" --output-on-failure
```

Expected: both green.

- [ ] **Step 5: Commit**

```bash
git add libs/vault
git commit -m "feat(vault): route buildTree through DataAdapter (Q.0 P2 T2.3)"
```

---

## Task 2.4: `Vault::Watcher` driven signals + tests

**Files:**
- Modify: `libs/vault/include/corbomite/vault/Vault.h` (add signals + public Watcher integration)
- Modify: `libs/vault/src/Vault.cpp` (own + start a Watcher instance; marshal its events into signal emissions + tree mutations)
- Modify: `libs/vault/src/Watcher.h` + `Watcher.cpp` (implement the Vault*-backed version)
- Create: `libs/vault/tests/tst_vault_watcher.cpp`
- Modify: `libs/vault/tests/CMakeLists.txt`

- [ ] **Step 1: Write failing test**

Create `libs/vault/tests/tst_vault_watcher.cpp`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>

#include "corbomite/vault/Vault.h"
#include "corbomite/vault/TFile.h"
#include "corbomite/storage/FileSystemAdapter.h"

class TestVaultWatcher : public QObject
{
    Q_OBJECT
private slots:
    void externalCreateEmitsCreated();
    void externalModifyEmitsModified();
    void externalDeleteEmitsDeletedWithTombstone();
    void externalRenameEmitsRenamed();
};

namespace {
void writeFile(const QString &path, const QByteArray &body)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path); f.open(QIODevice::WriteOnly); f.write(body);
}
}

void TestVaultWatcher::externalCreateEmitsCreated()
{
    QTemporaryDir dir;
    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    QSignalSpy spy(&vault, &Corbomite::Vault::created);
    writeFile(dir.path() + "/new.md", "x");
    QTRY_VERIFY_WITH_TIMEOUT(spy.count() >= 1, 5000);
}

void TestVaultWatcher::externalModifyEmitsModified()
{
    QTemporaryDir dir;
    writeFile(dir.path() + "/a.md", "one");

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    QSignalSpy spy(&vault, &Corbomite::Vault::modified);
    writeFile(dir.path() + "/a.md", "two");
    QTRY_VERIFY_WITH_TIMEOUT(spy.count() >= 1, 5000);
}

void TestVaultWatcher::externalDeleteEmitsDeletedWithTombstone()
{
    QTemporaryDir dir;
    writeFile(dir.path() + "/a.md", "x");

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    auto *f = vault.getFileByPath(QStringLiteral("a.md"));
    QVERIFY(f);

    QSignalSpy spy(&vault, &Corbomite::Vault::deletedFile);
    QFile::remove(dir.path() + "/a.md");
    QTRY_VERIFY_WITH_TIMEOUT(spy.count() >= 1, 5000);

    // Tombstone set: the first emit-arg TAbstractFile* should have deleted=true.
    auto *argPtr = qvariant_cast<Corbomite::TAbstractFile *>(spy.at(0).at(0));
    QVERIFY(argPtr);
    QCOMPARE(argPtr->deleted, true);
}

void TestVaultWatcher::externalRenameEmitsRenamed()
{
    QTemporaryDir dir;
    writeFile(dir.path() + "/a.md", "x");

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    QSignalSpy spy(&vault, &Corbomite::Vault::renamed);
    QFile::rename(dir.path() + "/a.md", dir.path() + "/b.md");
    QTRY_VERIFY_WITH_TIMEOUT(spy.count() >= 1, 5000);
}

QTEST_MAIN(TestVaultWatcher)
#include "tst_vault_watcher.moc"
```

- [ ] **Step 2: Register test**

Append to `libs/vault/tests/CMakeLists.txt`:

```cmake
add_executable(tst_vault_watcher tst_vault_watcher.cpp)
target_link_libraries(tst_vault_watcher
    PRIVATE Qt6::Test Corbomite::Vault Corbomite::Storage)
add_test(NAME tst_vault_watcher COMMAND tst_vault_watcher)
set_tests_properties(tst_vault_watcher PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
```

- [ ] **Step 3: Declare signals + pending-deletion queue in Vault.h**

Edit `libs/vault/include/corbomite/vault/Vault.h`. Add at the public API section:

```cpp
signals:
    void created(Corbomite::TAbstractFile *f);
    void modified(Corbomite::TFile *f);
    void deletedFile(Corbomite::TAbstractFile *f);
    void renamed(Corbomite::TAbstractFile *f, const QString &oldPath);
    void closed();
```

Add under `private:`:

```cpp
    // Watcher lifecycle + deferred-deletion queue for tombstones. The queue
    // holds entries one event-loop turn after a `deletedFile` emission, so
    // synchronous subscribers observing `deleted==true` can still read the
    // object safely.
    std::unique_ptr<class detail::Watcher> m_watcher;
    QVector<std::unique_ptr<TAbstractFile>> m_pendingDelete;

    // Fires on watcher-reported changes. Thread-marshalled by Watcher via
    // QueuedConnection before these run.
    void onExternalCreated(const QString &relPath);
    void onExternalModified(const QString &relPath);
    void onExternalDeleted(const QString &relPath);
    void onExternalRenamed(const QString &oldRel, const QString &newRel);
```

Add a forward decl near the top:

```cpp
namespace detail { class Watcher; }
```

- [ ] **Step 4: Implement the Watcher body**

Edit `libs/vault/src/Watcher.h`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QFileSystemWatcher>
#include <QHash>
#include <QObject>
#include <QTimer>

namespace Corbomite {
class Vault;

namespace detail {

class Watcher : public QObject
{
    Q_OBJECT
public:
    Watcher(Vault *vault, QObject *parent = nullptr);
    ~Watcher() override;

    void start(const QString &basePath);
    void stop();

signals:
    void created(const QString &relPath);
    void modified(const QString &relPath);
    void deleted(const QString &relPath);
    void renamed(const QString &oldRelPath, const QString &newRelPath);

private Q_SLOTS:
    void onDirChanged(const QString &absDir);
    void onFileChanged(const QString &absPath);
    void drainPending();

private:
    Vault                *m_vault;
    QFileSystemWatcher    m_fsw;
    QString               m_basePath;
    QTimer                m_drainTimer;          // 50ms coalescing tick
    QHash<QString, qint64> m_knownFiles;         // relPath -> mtime, for diff
    QStringList           m_pending;

    void snapshotDirectory(const QString &absDir);
    QString toRel(const QString &abs) const;
};

} // namespace detail
} // namespace Corbomite
```

Edit `libs/vault/src/Watcher.cpp` to implement the above. Minimal behaviour:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "Watcher.h"

#include "corbomite/vault/Vault.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>

namespace Corbomite::detail {

Watcher::Watcher(Vault *vault, QObject *parent)
    : QObject(parent)
    , m_vault(vault)
{
    m_drainTimer.setSingleShot(true);
    m_drainTimer.setInterval(50);
    connect(&m_drainTimer, &QTimer::timeout, this, &Watcher::drainPending);
    connect(&m_fsw, &QFileSystemWatcher::directoryChanged,
            this, &Watcher::onDirChanged);
    connect(&m_fsw, &QFileSystemWatcher::fileChanged,
            this, &Watcher::onFileChanged);
}

Watcher::~Watcher() = default;

void Watcher::start(const QString &basePath)
{
    stop();
    m_basePath = QDir::cleanPath(basePath);
    if (m_basePath.isEmpty()) return;

    // Watch every directory recursively.
    QDirIterator it(m_basePath,
                    QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden,
                    QDirIterator::Subdirectories);
    m_fsw.addPath(m_basePath);
    snapshotDirectory(m_basePath);
    while (it.hasNext()) {
        const QString d = it.next();
        m_fsw.addPath(d);
        snapshotDirectory(d);
    }
}

void Watcher::stop()
{
    const auto dirs = m_fsw.directories();
    if (!dirs.isEmpty()) m_fsw.removePaths(dirs);
    const auto files = m_fsw.files();
    if (!files.isEmpty()) m_fsw.removePaths(files);
    m_knownFiles.clear();
    m_pending.clear();
    m_drainTimer.stop();
    m_basePath.clear();
}

void Watcher::onDirChanged(const QString &absDir)
{
    m_pending.append(absDir);
    m_drainTimer.start();
}

void Watcher::onFileChanged(const QString &absPath)
{
    m_pending.append(absPath);
    m_drainTimer.start();
}

void Watcher::snapshotDirectory(const QString &absDir)
{
    QDirIterator it(absDir, QDir::Files | QDir::NoDotAndDotDot | QDir::Hidden);
    while (it.hasNext()) {
        it.next();
        QFileInfo fi = it.fileInfo();
        const QString rel = toRel(fi.absoluteFilePath());
        if (rel.startsWith(QStringLiteral(".obsidian/")) ||
            rel.startsWith(QStringLiteral(".corbomite/")) ||
            rel.startsWith(QStringLiteral(".trash/"))) continue;
        m_knownFiles.insert(rel, fi.lastModified().toMSecsSinceEpoch());
    }
}

void Watcher::drainPending()
{
    // Naive diff: rebuild the snapshot, emit created/modified/deleted.
    // Renames are two events (delete oldRel + create newRel). For Phase 2 we
    // emit them as a delete+create pair; Task 2.5 upgrades to rename
    // detection via size+mtime fingerprint within a 100ms window.
    QHash<QString, qint64> fresh;
    QDirIterator it(m_basePath, QDir::Files | QDir::NoDotAndDotDot | QDir::Hidden,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        QFileInfo fi = it.fileInfo();
        const QString rel = toRel(fi.absoluteFilePath());
        if (rel.startsWith(QStringLiteral(".obsidian/")) ||
            rel.startsWith(QStringLiteral(".corbomite/")) ||
            rel.startsWith(QStringLiteral(".trash/"))) continue;
        fresh.insert(rel, fi.lastModified().toMSecsSinceEpoch());
    }

    // Emit diffs.
    for (auto it = fresh.cbegin(); it != fresh.cend(); ++it) {
        const auto known = m_knownFiles.constFind(it.key());
        if (known == m_knownFiles.cend()) {
            Q_EMIT created(it.key());
        } else if (known.value() != it.value()) {
            Q_EMIT modified(it.key());
        }
    }
    for (auto it = m_knownFiles.cbegin(); it != m_knownFiles.cend(); ++it) {
        if (!fresh.contains(it.key())) Q_EMIT deleted(it.key());
    }

    m_knownFiles = std::move(fresh);
    m_pending.clear();
}

QString Watcher::toRel(const QString &abs) const
{
    return QDir(m_basePath).relativeFilePath(abs);
}

} // namespace Corbomite::detail
```

- [ ] **Step 5: Wire Watcher into Vault**

Edit `libs/vault/src/Vault.cpp`:

- Add `#include "Watcher.h"`.
- In the Vault ctor, after the root setup, construct `m_watcher = std::make_unique<detail::Watcher>(this)` and connect its signals:

  ```cpp
  connect(m_watcher.get(), &detail::Watcher::created,
          this, &Vault::onExternalCreated);
  connect(m_watcher.get(), &detail::Watcher::modified,
          this, &Vault::onExternalModified);
  connect(m_watcher.get(), &detail::Watcher::deleted,
          this, &Vault::onExternalDeleted);
  connect(m_watcher.get(), &detail::Watcher::renamed,
          this, &Vault::onExternalRenamed);
  ```
- In `load()`, after `buildTree()`, call `m_watcher->start(m_basePath)`.
- In `unload()`, call `m_watcher->stop()` before clearing the map.
- Implement the four on-external handlers:

  ```cpp
  void Vault::onExternalCreated(const QString &relPath)
  {
      const QString rel = Vault::Paths::normalize(relPath);
      if (m_fileMap.contains(rel)) return;  // already tracked
      QFileInfo fi(m_basePath + QLatin1Char('/') + rel);

      TFolder *parent = m_root;
      const int slash = rel.lastIndexOf(QLatin1Char('/'));
      if (slash > 0) {
          if (auto *p = getFolderByPath(rel.left(slash))) parent = p;
      }

      if (fi.isDir()) {
          auto folder = std::make_unique<TFolder>(this, rel);
          folder->parent = parent;
          parent->children.append(folder.get());
          TAbstractFile *raw = folder.get();
          m_fileMap.insert(rel, std::move(folder));
          Q_EMIT created(raw);
      } else if (fi.isFile()) {
          auto file = std::make_unique<TFile>(this, rel);
          file->parent = parent;
          FileStat fs;
          fs.sizeBytes = fi.size();
          fs.mtimeMs   = fi.lastModified().toMSecsSinceEpoch();
          file->stat   = fs;
          parent->children.append(file.get());
          TAbstractFile *raw = file.get();
          m_fileMap.insert(rel, std::move(file));
          Q_EMIT created(raw);
      }
  }

  void Vault::onExternalModified(const QString &relPath)
  {
      const QString rel = Vault::Paths::normalize(relPath);
      if (auto *f = getFileByPath(rel)) {
          QFileInfo fi(m_basePath + QLatin1Char('/') + rel);
          FileStat fs;
          fs.sizeBytes = fi.size();
          fs.mtimeMs   = fi.lastModified().toMSecsSinceEpoch();
          f->stat      = fs;
          Q_EMIT modified(f);
      }
  }

  void Vault::onExternalDeleted(const QString &relPath)
  {
      const QString rel = Vault::Paths::normalize(relPath);
      auto it = m_fileMap.find(rel);
      if (it == m_fileMap.end()) return;

      std::unique_ptr<TAbstractFile> owned = std::move(it.value());
      m_fileMap.erase(it);

      owned->deleted = true;
      if (TFolder *parent = owned->parent) {
          parent->children.removeAll(owned.get());
      }
      TAbstractFile *raw = owned.get();
      m_pendingDelete.push_back(std::move(owned));
      Q_EMIT deletedFile(raw);

      // Drain next event-loop turn so late subscribers still see the pointer.
      QTimer::singleShot(0, this, [this] { m_pendingDelete.clear(); });
  }

  void Vault::onExternalRenamed(const QString &oldRel, const QString &newRel)
  {
      const QString oldR = Vault::Paths::normalize(oldRel);
      const QString newR = Vault::Paths::normalize(newRel);
      auto it = m_fileMap.find(oldR);
      if (it == m_fileMap.end()) return;
      auto node = std::move(it.value());
      m_fileMap.erase(it);
      node->setPath(newR);
      TAbstractFile *raw = node.get();
      m_fileMap.insert(newR, std::move(node));
      Q_EMIT renamed(raw, oldR);
  }
  ```

Add `#include <QTimer>` + `#include <QFileInfo>` to Vault.cpp if not already present.

- [ ] **Step 6: Build**

```bash
cmake --build build
```

- [ ] **Step 7: Run watcher tests**

```bash
cd build && ctest -R "tst_vault_watcher|tst_vault_skeleton|tst_vault_adapter" --output-on-failure
```

Expected: all green. Watcher tests may be timing-sensitive under `offscreen` — the 5000ms QTRY timeouts should cover it.

- [ ] **Step 8: Commit**

```bash
git add libs/vault
git commit -m "feat(vault): Vault::Watcher emits created/modified/deleted/renamed (Q.0 P2 T2.4)"
```

---

## Task 2.5: Rename detection + mtime echo suppression

**Files:**
- Modify: `libs/vault/src/Watcher.cpp` (pair delete+create within 100ms by size+inode → emit renamed)
- Modify: `libs/vault/src/Vault.cpp` (honour `WriteHints::mtimeMs` — self-writes stamp an expected mtime; watcher skips matching events)
- Modify: `libs/vault/include/corbomite/vault/Vault.h` (add private `m_recentSelfWrites` set)
- Create: `libs/vault/tests/tst_vault_echo_suppression.cpp`
- Modify: `libs/vault/tests/CMakeLists.txt`

- [ ] **Step 1: Write failing test**

Create `libs/vault/tests/tst_vault_echo_suppression.cpp`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>

#include "corbomite/vault/Vault.h"
#include "corbomite/vault/TFile.h"
#include "corbomite/storage/FileSystemAdapter.h"

class TestVaultEchoSuppression : public QObject
{
    Q_OBJECT
private slots:
    void selfWriteDoesNotDoubleEmit();
    void externalWriteAfterSelfWriteEmits();
};

void TestVaultEchoSuppression::selfWriteDoesNotDoubleEmit()
{
    QTemporaryDir dir;
    QFile f(dir.path() + "/a.md");
    f.open(QIODevice::WriteOnly); f.write("one"); f.close();

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    QSignalSpy spy(&vault, &Corbomite::Vault::modified);

    // Vault's own modify routes through DataAdapter + stamps hints; the
    // watcher sees the mtime change but suppresses because it matches.
    auto *tf = vault.getFileByPath(QStringLiteral("a.md"));
    QVERIFY(tf);
    // `modify()` arrives in Phase 3; this test is compile-checkable here
    // and should PASS once Phase 3 lands. For Phase 2 we place it under
    // QSKIP for now.
    QSKIP("Phase 3 Task 3.3 turns this on", QTest::SkipAll);
}

void TestVaultEchoSuppression::externalWriteAfterSelfWriteEmits()
{
    QSKIP("Phase 3 Task 3.3 turns this on", QTest::SkipAll);
}

QTEST_MAIN(TestVaultEchoSuppression)
#include "tst_vault_echo_suppression.moc"
```

- [ ] **Step 2: Register test**

Append to `libs/vault/tests/CMakeLists.txt`:

```cmake
add_executable(tst_vault_echo_suppression tst_vault_echo_suppression.cpp)
target_link_libraries(tst_vault_echo_suppression
    PRIVATE Qt6::Test Corbomite::Vault Corbomite::Storage)
add_test(NAME tst_vault_echo_suppression COMMAND tst_vault_echo_suppression)
set_tests_properties(tst_vault_echo_suppression PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
```

- [ ] **Step 3: Add rename-pairing logic to drainPending**

Edit `libs/vault/src/Watcher.cpp`. In `drainPending`, after computing `created` + `deleted` diffs but before emitting, pair up entries with matching size+mtime fingerprints within the drain and emit `renamed` for pairs; emit residual `created`/`deleted` for the rest.

Full replacement of the emission block:

```cpp
    QStringList createdRels, deletedRels;
    for (auto it = fresh.cbegin(); it != fresh.cend(); ++it) {
        const auto known = m_knownFiles.constFind(it.key());
        if (known == m_knownFiles.cend()) {
            createdRels.append(it.key());
        } else if (known.value() != it.value()) {
            Q_EMIT modified(it.key());
        }
    }
    for (auto it = m_knownFiles.cbegin(); it != m_knownFiles.cend(); ++it) {
        if (!fresh.contains(it.key())) deletedRels.append(it.key());
    }

    // Pair deleted+created by identical mtime (best-effort — a real impl
    // could fingerprint by size + first-N-bytes, but mtime is fine for the
    // common same-turn rename case).
    QStringList unpaired = createdRels;
    for (const QString &oldRel : deletedRels) {
        const auto knownIt = m_knownFiles.constFind(oldRel);
        const qint64 oldMtime = knownIt != m_knownFiles.cend() ? knownIt.value() : 0;
        bool matched = false;
        for (int i = 0; i < unpaired.size(); ++i) {
            const QString &newRel = unpaired[i];
            const qint64 newMtime = fresh.value(newRel);
            if (oldMtime != 0 && newMtime == oldMtime) {
                Q_EMIT renamed(oldRel, newRel);
                unpaired.removeAt(i);
                matched = true;
                break;
            }
        }
        if (!matched) Q_EMIT deleted(oldRel);
    }
    for (const QString &newRel : unpaired) Q_EMIT created(newRel);
```

- [ ] **Step 4: Add self-write ledger scaffold in Vault.h**

Edit `libs/vault/include/corbomite/vault/Vault.h`. Under `private:`:

```cpp
    // Self-write echo suppression: outgoing writes (Phase 3) stamp
    // `m_selfWriteMtimes[rel] = expectedMtimeMs`; Watcher-reported events
    // with matching mtime and rel within 1s drop silently.
    QHash<QString, qint64> m_selfWriteMtimes;
    void stampSelfWrite(const QString &rel, qint64 mtimeMs);
    bool consumeSelfWrite(const QString &rel, qint64 mtimeMs);
```

- [ ] **Step 5: Implement ledger + wire through onExternalModified**

Edit `libs/vault/src/Vault.cpp`. Add:

```cpp
void Vault::stampSelfWrite(const QString &rel, qint64 mtimeMs)
{
    m_selfWriteMtimes.insert(rel, mtimeMs);
    QTimer::singleShot(1000, this, [this, rel] {
        m_selfWriteMtimes.remove(rel);
    });
}

bool Vault::consumeSelfWrite(const QString &rel, qint64 mtimeMs)
{
    auto it = m_selfWriteMtimes.find(rel);
    if (it == m_selfWriteMtimes.end()) return false;
    if (it.value() != mtimeMs) return false;
    m_selfWriteMtimes.erase(it);
    return true;
}
```

In `onExternalModified`, before firing the signal, compute `mtimeMs` from the QFileInfo and call `consumeSelfWrite(rel, mtimeMs)`; if it returns true, return without emitting.

(Phase 3's `Vault::modify` will call `stampSelfWrite` before the adapter write.)

- [ ] **Step 6: Build**

```bash
cmake --build build
cd build && ctest -R "tst_vault" --output-on-failure
```

Expected: watcher tests pass; echo-suppression tests `QSKIP`.

- [ ] **Step 7: Commit**

```bash
git add libs/vault
git commit -m "feat(vault): rename pairing + self-write echo suppression scaffold (Q.0 P2 T2.5)"
```

---

## Task 2.6: Phase 2 verification + documentation update

- [ ] **Step 1: Full build + test**

```bash
cmake --build build
cd build && ctest --output-on-failure -E "tst_markoff_inline_math|tst_renderengine|tst_completion_popup|tst_benchmark_layout"
```

Expected: green outside the 4 known-flaky.

- [ ] **Step 2: Update PROJECT-STATE.md**

Append to `## Recent decisions` a bullet documenting Phase 2 completion: new Watcher class, rename detection, echo-suppression scaffold.

- [ ] **Step 3: Commit**

```bash
git add docs/PROJECT-STATE.md
git commit -m "docs: Cluster Q.0 Phase 2 landed (Q.0 P2 T2.6)"
```

---

# Phase 3 — Vault mutation API (read / modify / create / rename / remove / trash / process)

Goal: Implement `Vault::{read, cachedRead, readBinary, modify, append, process, create, createFolder, rename, remove, trash, copy}`. Absorb `VaultProcess` (static) and `VaultTrash` into Vault instance methods. Every mutation stamps the self-write ledger + fires the appropriate signal.

## Task 3.1: `Vault::read` + `readBinary` + `readRaw`

**Files:**
- Modify: `libs/vault/include/corbomite/vault/Vault.h`
- Modify: `libs/vault/src/Vault.cpp`
- Create: `libs/vault/tests/tst_vault_read.cpp`
- Modify: `libs/vault/tests/CMakeLists.txt`

- [ ] **Step 1: Write failing test**

Create `libs/vault/tests/tst_vault_read.cpp`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QTemporaryDir>
#include <QFile>

#include "corbomite/vault/Vault.h"
#include "corbomite/vault/TFile.h"
#include "corbomite/storage/FileSystemAdapter.h"

class TestVaultRead : public QObject
{
    Q_OBJECT
private slots:
    void readReturnsBody();
    void readBinaryReturnsBytes();
    void readRawBypassesTree();
    void readMissingReturnsEmpty();
};

void TestVaultRead::readReturnsBody()
{
    QTemporaryDir dir;
    QFile f(dir.path() + "/a.md");
    f.open(QIODevice::WriteOnly); f.write("hello"); f.close();

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    auto *tf = vault.getFileByPath(QStringLiteral("a.md"));
    QCOMPARE(vault.read(tf), QByteArray("hello"));
}

void TestVaultRead::readBinaryReturnsBytes()
{
    QTemporaryDir dir;
    QFile f(dir.path() + "/a.bin");
    f.open(QIODevice::WriteOnly); f.write("\x01\x02\x03", 3); f.close();

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    auto *tf = vault.getFileByPath(QStringLiteral("a.bin"));
    QCOMPARE(vault.readBinary(tf), QByteArray("\x01\x02\x03", 3));
}

void TestVaultRead::readRawBypassesTree()
{
    QTemporaryDir dir;
    QFile f(dir.path() + "/a.md");
    f.open(QIODevice::WriteOnly); f.write("raw"); f.close();

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    QCOMPARE(vault.readRaw(QStringLiteral("a.md")), QByteArray("raw"));
}

void TestVaultRead::readMissingReturnsEmpty()
{
    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    QCOMPARE(vault.read(nullptr), QByteArray());
}

QTEST_MAIN(TestVaultRead)
#include "tst_vault_read.moc"
```

- [ ] **Step 2: Register test**

Append to `libs/vault/tests/CMakeLists.txt`:

```cmake
add_executable(tst_vault_read tst_vault_read.cpp)
target_link_libraries(tst_vault_read
    PRIVATE Qt6::Test Corbomite::Vault Corbomite::Storage)
add_test(NAME tst_vault_read COMMAND tst_vault_read)
set_tests_properties(tst_vault_read PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
```

- [ ] **Step 3: Declare in Vault.h**

Add under `// ---- Read ----`:

```cpp
    QByteArray read(TFile *f) const;
    QByteArray readBinary(TFile *f) const;
    QByteArray readRaw(const QString &path) const;
```

- [ ] **Step 4: Implement**

Edit `libs/vault/src/Vault.cpp`. Add:

```cpp
QByteArray Vault::read(TFile *f) const
{
    if (!f || !m_adapter) return {};
    auto body = m_adapter->readBinary(m_basePath + QLatin1Char('/') + f->path);
    return body.has_value() ? *body : QByteArray{};
}

QByteArray Vault::readBinary(TFile *f) const
{
    return read(f);  // same path today; hook kept for API parity
}

QByteArray Vault::readRaw(const QString &path) const
{
    if (!m_adapter) return {};
    auto body = m_adapter->readBinary(
        m_basePath + QLatin1Char('/') + Vault::Paths::normalize(path));
    return body.has_value() ? *body : QByteArray{};
}
```

- [ ] **Step 5: Build + run**

```bash
cmake --build build
cd build && ctest -R tst_vault_read --output-on-failure
```

- [ ] **Step 6: Commit**

```bash
git add libs/vault
git commit -m "feat(vault): read/readBinary/readRaw (Q.0 P3 T3.1)"
```

---

## Task 3.2: `Vault::cachedRead` + cache invalidation

**Files:**
- Modify: `libs/vault/include/corbomite/vault/Vault.h` (add `m_readCache` + `cachedRead`)
- Modify: `libs/vault/src/Vault.cpp`
- Create: `libs/vault/tests/tst_vault_cached_read.cpp`
- Modify: `libs/vault/tests/CMakeLists.txt`

- [ ] **Step 1: Write failing test**

Create `libs/vault/tests/tst_vault_cached_read.cpp`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QTemporaryDir>
#include <QFile>

#include "corbomite/vault/Vault.h"
#include "corbomite/vault/TFile.h"
#include "corbomite/storage/FileSystemAdapter.h"

class TestVaultCachedRead : public QObject
{
    Q_OBJECT
private slots:
    void firstCallReadsFromDisk();
    void secondCallSkipsAdapter();
    void modifiedEventEvictsCache();
};

void TestVaultCachedRead::firstCallReadsFromDisk()
{
    QTemporaryDir dir;
    QFile f(dir.path() + "/a.md");
    f.open(QIODevice::WriteOnly); f.write("x"); f.close();

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    auto *tf = vault.getFileByPath(QStringLiteral("a.md"));
    QCOMPARE(vault.cachedRead(tf), QByteArray("x"));
}

void TestVaultCachedRead::secondCallSkipsAdapter()
{
    QTemporaryDir dir;
    QFile f(dir.path() + "/a.md");
    f.open(QIODevice::WriteOnly); f.write("x"); f.close();

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    auto *tf = vault.getFileByPath(QStringLiteral("a.md"));
    vault.cachedRead(tf);
    QFile(dir.path() + "/a.md").remove();  // if cache is honoured, we still get "x"
    QCOMPARE(vault.cachedRead(tf), QByteArray("x"));
}

void TestVaultCachedRead::modifiedEventEvictsCache()
{
    QTemporaryDir dir;
    QFile f(dir.path() + "/a.md");
    f.open(QIODevice::WriteOnly); f.write("x"); f.close();

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    auto *tf = vault.getFileByPath(QStringLiteral("a.md"));
    vault.cachedRead(tf);

    // Rewrite and fire modified.
    QFile g(dir.path() + "/a.md"); g.open(QIODevice::WriteOnly); g.write("y"); g.close();
    // External modify fires modified() via the watcher; QTRY ensures it arrives
    // and invalidates the cache.
    QTRY_COMPARE_WITH_TIMEOUT(vault.cachedRead(tf), QByteArray("y"), 5000);
}

QTEST_MAIN(TestVaultCachedRead)
#include "tst_vault_cached_read.moc"
```

- [ ] **Step 2: Register test**

Append:

```cmake
add_executable(tst_vault_cached_read tst_vault_cached_read.cpp)
target_link_libraries(tst_vault_cached_read
    PRIVATE Qt6::Test Corbomite::Vault Corbomite::Storage)
add_test(NAME tst_vault_cached_read COMMAND tst_vault_cached_read)
set_tests_properties(tst_vault_cached_read PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
```

- [ ] **Step 3: Add cache + accessor to Vault.h**

Under `private:`:

```cpp
    mutable QHash<QString, QByteArray> m_readCache;
```

Under the read section:

```cpp
    QByteArray cachedRead(TFile *f);
```

- [ ] **Step 4: Implement**

Edit `libs/vault/src/Vault.cpp`:

```cpp
QByteArray Vault::cachedRead(TFile *f)
{
    if (!f) return {};
    auto it = m_readCache.find(f->path);
    if (it != m_readCache.end()) return it.value();
    const QByteArray body = read(f);
    m_readCache.insert(f->path, body);
    return body;
}
```

In `onExternalModified`:
```cpp
m_readCache.remove(rel);
```
before the signal emission.

In `onExternalDeleted`:
```cpp
m_readCache.remove(rel);
```

In `onExternalRenamed`:
```cpp
m_readCache.remove(oldR);
```

In `teardownTree`:
```cpp
m_readCache.clear();
```

- [ ] **Step 5: Build + run**

```bash
cmake --build build
cd build && ctest -R tst_vault_cached_read --output-on-failure
```

- [ ] **Step 6: Commit**

```bash
git add libs/vault
git commit -m "feat(vault): cachedRead + invalidation on modify/delete/rename (Q.0 P3 T3.2)"
```

---

## Task 3.3: `Vault::modify` + `modifyBinary` + self-write stamping

**Files:**
- Modify: `libs/vault/include/corbomite/vault/Vault.h`
- Modify: `libs/vault/src/Vault.cpp`
- Create: `libs/vault/tests/tst_vault_modify.cpp`
- Modify: `libs/vault/tests/tst_vault_echo_suppression.cpp` (remove QSKIP)
- Modify: `libs/vault/tests/CMakeLists.txt`

- [ ] **Step 1: Write failing test**

Create `libs/vault/tests/tst_vault_modify.cpp`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QFile>

#include "corbomite/vault/Vault.h"
#include "corbomite/vault/TFile.h"
#include "corbomite/storage/FileSystemAdapter.h"

class TestVaultModify : public QObject
{
    Q_OBJECT
private slots:
    void modifyWritesAndEmits();
    void modifyInvalidatesCache();
    void appendAppendsBody();
};

void TestVaultModify::modifyWritesAndEmits()
{
    QTemporaryDir dir;
    QFile f(dir.path() + "/a.md");
    f.open(QIODevice::WriteOnly); f.write("one"); f.close();

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    auto *tf = vault.getFileByPath(QStringLiteral("a.md"));
    QSignalSpy spy(&vault, &Corbomite::Vault::modified);

    QVERIFY(vault.modify(tf, QByteArray("two")));
    QFile g(dir.path() + "/a.md"); g.open(QIODevice::ReadOnly);
    QCOMPARE(g.readAll(), QByteArray("two"));

    QCOMPARE(spy.count(), 1);
}

void TestVaultModify::modifyInvalidatesCache()
{
    QTemporaryDir dir;
    QFile f(dir.path() + "/a.md");
    f.open(QIODevice::WriteOnly); f.write("one"); f.close();

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    auto *tf = vault.getFileByPath(QStringLiteral("a.md"));
    QCOMPARE(vault.cachedRead(tf), QByteArray("one"));

    QVERIFY(vault.modify(tf, QByteArray("two")));
    QCOMPARE(vault.cachedRead(tf), QByteArray("two"));
}

void TestVaultModify::appendAppendsBody()
{
    QTemporaryDir dir;
    QFile f(dir.path() + "/a.md");
    f.open(QIODevice::WriteOnly); f.write("x"); f.close();

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    auto *tf = vault.getFileByPath(QStringLiteral("a.md"));
    QVERIFY(vault.append(tf, QByteArray("y")));
    QFile g(dir.path() + "/a.md"); g.open(QIODevice::ReadOnly);
    QCOMPARE(g.readAll(), QByteArray("xy"));
}

QTEST_MAIN(TestVaultModify)
#include "tst_vault_modify.moc"
```

- [ ] **Step 2: Register test**

```cmake
add_executable(tst_vault_modify tst_vault_modify.cpp)
target_link_libraries(tst_vault_modify
    PRIVATE Qt6::Test Corbomite::Vault Corbomite::Storage)
add_test(NAME tst_vault_modify COMMAND tst_vault_modify)
set_tests_properties(tst_vault_modify PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
```

- [ ] **Step 3: Declare in Vault.h**

Under the write section:

```cpp
    bool modify(TFile *f, const QByteArray &body, const WriteHints &hints = {});
    bool modifyBinary(TFile *f, const QByteArray &body, const WriteHints &hints = {});
    bool append(TFile *f, const QByteArray &body);
```

Add `#include "corbomite/storage/DataAdapter.h"` to the header (or forward the `WriteHints` struct). Simpler: add the include.

- [ ] **Step 4: Implement**

Edit `libs/vault/src/Vault.cpp`:

```cpp
bool Vault::modify(TFile *f, const QByteArray &body, const WriteHints &hints)
{
    if (!f || !m_adapter) return false;
    const QString abs = m_basePath + QLatin1Char('/') + f->path;

    WriteHints effective = hints;
    // Stamp an mtime hint so the echo-suppression ledger matches.
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    if (!effective.mtimeMs.has_value()) effective.mtimeMs = nowMs;
    stampSelfWrite(f->path, *effective.mtimeMs);

    if (!m_adapter->writeBinary(abs, body, effective)) return false;

    m_readCache.remove(f->path);
    m_readCache.insert(f->path, body);
    if (f->stat.has_value()) f->stat->mtimeMs = *effective.mtimeMs;
    Q_EMIT modified(f);
    return true;
}

bool Vault::modifyBinary(TFile *f, const QByteArray &body, const WriteHints &hints)
{
    return modify(f, body, hints);
}

bool Vault::append(TFile *f, const QByteArray &body)
{
    if (!f) return false;
    const QByteArray cur = read(f);
    return modify(f, cur + body);
}
```

Add `#include <QDateTime>`.

- [ ] **Step 5: Turn on the echo-suppression test**

Edit `libs/vault/tests/tst_vault_echo_suppression.cpp`. Remove the two `QSKIP` lines and replace the first test's tail:

```cpp
    vault.modify(tf, QByteArray("two"));
    // Give the watcher 300ms; with echo suppression working, no modified
    // signal should arrive for the self-write.
    QTest::qWait(300);
    QCOMPARE(spy.count(), 1);  // only the in-process emission from modify()
```

For the second test, replace with:

```cpp
    QTemporaryDir dir;
    QFile f(dir.path() + "/a.md");
    f.open(QIODevice::WriteOnly); f.write("one"); f.close();

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    auto *tf = vault.getFileByPath(QStringLiteral("a.md"));
    vault.modify(tf, QByteArray("two"));

    QSignalSpy spy(&vault, &Corbomite::Vault::modified);
    // External modify (different mtime) — should emit.
    QTest::qWait(1100);  // clear the 1s self-write window
    QFile g(dir.path() + "/a.md"); g.open(QIODevice::WriteOnly); g.write("three"); g.close();
    QTRY_VERIFY_WITH_TIMEOUT(spy.count() >= 1, 5000);
```

- [ ] **Step 6: Build + run**

```bash
cmake --build build
cd build && ctest -R "tst_vault_modify|tst_vault_echo_suppression" --output-on-failure
```

- [ ] **Step 7: Commit**

```bash
git add libs/vault
git commit -m "feat(vault): modify/modifyBinary/append + echo suppression (Q.0 P3 T3.3)"
```

---

## Task 3.4: `Vault::process` + absorb `VaultProcess` (static class)

**Files:**
- Modify: `libs/vault/include/corbomite/vault/Vault.h`
- Modify: `libs/vault/src/Vault.cpp` (per-path mutex for serialisation)
- Create: `libs/vault/tests/tst_vault_process.cpp`
- Modify: `libs/vault/tests/CMakeLists.txt`
- Delete: `libs/storage/include/corbomite/storage/VaultProcess.h`
- Delete: `libs/storage/src/VaultProcess.cpp`
- Modify: `libs/storage/CMakeLists.txt`
- Modify: every caller of `VaultProcess::process(...)` → `vault->process(file, mutator)`

- [ ] **Step 1: Identify callers**

```bash
grep -rn "VaultProcess" libs/ src/ tests/ 2>/dev/null
```

Record each site. Caller contract changes from static `VaultProcess::process(adapter, absPath, mutator)` to `vault->process(TFile *, mutator)`. Some callers don't have a Vault pointer yet (e.g., Properties panel, Daily-Notes service); those get the migration as part of the Phase 6/7 consumer wave. For now, keep a thin static shim in the old header pointing to Vault OR migrate caller-by-caller in this task — pick caller-by-caller.

- [ ] **Step 2: Write failing test**

Create `libs/vault/tests/tst_vault_process.cpp`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QTemporaryDir>
#include <QFile>

#include "corbomite/vault/Vault.h"
#include "corbomite/vault/TFile.h"
#include "corbomite/storage/FileSystemAdapter.h"

class TestVaultProcess : public QObject
{
    Q_OBJECT
private slots:
    void processMutatesAtomically();
    void processPassesCurrentContent();
};

void TestVaultProcess::processMutatesAtomically()
{
    QTemporaryDir dir;
    QFile f(dir.path() + "/a.md");
    f.open(QIODevice::WriteOnly); f.write("one"); f.close();

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    auto *tf = vault.getFileByPath(QStringLiteral("a.md"));
    QVERIFY(vault.process(tf, [](const QByteArray &cur) {
        return cur + QByteArray(" -> mutated");
    }));

    QFile g(dir.path() + "/a.md"); g.open(QIODevice::ReadOnly);
    QCOMPARE(g.readAll(), QByteArray("one -> mutated"));
}

void TestVaultProcess::processPassesCurrentContent()
{
    QTemporaryDir dir;
    QFile f(dir.path() + "/a.md");
    f.open(QIODevice::WriteOnly); f.write("hello"); f.close();

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    auto *tf = vault.getFileByPath(QStringLiteral("a.md"));
    QByteArray seen;
    vault.process(tf, [&seen](const QByteArray &cur) {
        seen = cur;
        return cur;
    });
    QCOMPARE(seen, QByteArray("hello"));
}

QTEST_MAIN(TestVaultProcess)
#include "tst_vault_process.moc"
```

- [ ] **Step 3: Register test**

```cmake
add_executable(tst_vault_process tst_vault_process.cpp)
target_link_libraries(tst_vault_process
    PRIVATE Qt6::Test Corbomite::Vault Corbomite::Storage)
add_test(NAME tst_vault_process COMMAND tst_vault_process)
set_tests_properties(tst_vault_process PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
```

- [ ] **Step 4: Declare `process` in Vault.h**

```cpp
    bool process(TFile *f,
                 std::function<QByteArray(const QByteArray &)> mutator);
```

Add `#include <functional>` and `#include <QMutex>`, and under `private:`:

```cpp
    QHash<QString, QMutex *> m_perPathMutex;
    QMutex                   m_perPathMutexGuard;
```

- [ ] **Step 5: Implement**

```cpp
bool Vault::process(TFile *f,
                    std::function<QByteArray(const QByteArray &)> mutator)
{
    if (!f) return false;

    QMutex *m = nullptr;
    {
        QMutexLocker g(&m_perPathMutexGuard);
        auto it = m_perPathMutex.find(f->path);
        if (it == m_perPathMutex.end()) {
            m = new QMutex;
            m_perPathMutex.insert(f->path, m);
        } else {
            m = *it;
        }
    }
    QMutexLocker lock(m);

    const QByteArray cur = read(f);
    const QByteArray next = mutator(cur);
    if (next == cur) return true;
    return modify(f, next);
}
```

Remember to delete per-path mutex entries in the Vault destructor:

```cpp
Vault::~Vault()
{
    qDeleteAll(m_perPathMutex);
}
```

- [ ] **Step 6: Delete `VaultProcess`**

```bash
rm libs/storage/include/corbomite/storage/VaultProcess.h libs/storage/src/VaultProcess.cpp
```

Remove from `libs/storage/CMakeLists.txt`.

- [ ] **Step 7: Migrate each caller**

For each match from Step 1 outside tests, replace `Corbomite::VaultProcess::process(fs, absPath, mutator)` with `vault->process(file, mutator)`. If the caller doesn't have a Vault*, it's a candidate for a Phase-6-consumer-wave task and should not be rewritten here — instead temporarily inline the read/modify pair via `m_adapter` direct calls.

- [ ] **Step 8: Build + run**

```bash
cmake --build build
cd build && ctest -R tst_vault_process --output-on-failure
```

- [ ] **Step 9: Commit**

```bash
git add -u libs src tests
git commit -m "feat(vault): Vault::process + delete VaultProcess static (Q.0 P3 T3.4)"
```

---

## Task 3.5: `Vault::create` + `createBinary` + `createFolder`

**Files:**
- Modify: `libs/vault/include/corbomite/vault/Vault.h`
- Modify: `libs/vault/src/Vault.cpp`
- Create: `libs/vault/tests/tst_vault_create.cpp`

- [ ] **Step 1: Write failing test**

Create `libs/vault/tests/tst_vault_create.cpp`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QFile>

#include "corbomite/vault/Vault.h"
#include "corbomite/vault/TFile.h"
#include "corbomite/vault/TFolder.h"
#include "corbomite/storage/FileSystemAdapter.h"

class TestVaultCreate : public QObject
{
    Q_OBJECT
private slots:
    void createCreatesFile();
    void createCreatesParentFolderIfNeeded();
    void createEmitsCreated();
    void createFolderCreatesDir();
    void createRejectsExisting();
};

void TestVaultCreate::createCreatesFile()
{
    QTemporaryDir dir;
    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    auto *tf = vault.create(QStringLiteral("new.md"), QByteArray("body"));
    QVERIFY(tf);
    QCOMPARE(tf->path, QStringLiteral("new.md"));
    QFile f(dir.path() + "/new.md");
    QVERIFY(f.open(QIODevice::ReadOnly));
    QCOMPARE(f.readAll(), QByteArray("body"));
}

void TestVaultCreate::createCreatesParentFolderIfNeeded()
{
    QTemporaryDir dir;
    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    auto *tf = vault.create(QStringLiteral("sub/deeper/new.md"),
                             QByteArray("body"));
    QVERIFY(tf);
    QVERIFY(QFileInfo::exists(dir.path() + "/sub/deeper/new.md"));
}

void TestVaultCreate::createEmitsCreated()
{
    QTemporaryDir dir;
    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    QSignalSpy spy(&vault, &Corbomite::Vault::created);
    vault.create(QStringLiteral("x.md"), QByteArray("y"));
    QCOMPARE(spy.count(), 1);
}

void TestVaultCreate::createFolderCreatesDir()
{
    QTemporaryDir dir;
    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    auto *tf = vault.createFolder(QStringLiteral("sub"));
    QVERIFY(tf);
    QVERIFY(QFileInfo(dir.path() + "/sub").isDir());
}

void TestVaultCreate::createRejectsExisting()
{
    QTemporaryDir dir;
    QFile f(dir.path() + "/a.md"); f.open(QIODevice::WriteOnly); f.write("x"); f.close();

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    QCOMPARE(vault.create(QStringLiteral("a.md"), QByteArray("y")),
             static_cast<Corbomite::TFile *>(nullptr));
}

QTEST_MAIN(TestVaultCreate)
#include "tst_vault_create.moc"
```

- [ ] **Step 2: Register test**

```cmake
add_executable(tst_vault_create tst_vault_create.cpp)
target_link_libraries(tst_vault_create
    PRIVATE Qt6::Test Corbomite::Vault Corbomite::Storage)
add_test(NAME tst_vault_create COMMAND tst_vault_create)
set_tests_properties(tst_vault_create PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
```

- [ ] **Step 3: Declare in Vault.h**

```cpp
    TFile   *create(const QString &path, const QByteArray &body);
    TFile   *createBinary(const QString &path, const QByteArray &body);
    TFolder *createFolder(const QString &path);
```

- [ ] **Step 4: Implement**

```cpp
TFile *Vault::create(const QString &path, const QByteArray &body)
{
    if (!m_adapter) return nullptr;
    const QString rel = Vault::Paths::normalize(path);
    if (m_fileMap.contains(rel)) return nullptr;

    const QString abs = m_basePath + QLatin1Char('/') + rel;
    if (!m_adapter->mkpath(QFileInfo(abs).absolutePath())) return nullptr;

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    WriteHints hints;
    hints.mtimeMs = nowMs;
    stampSelfWrite(rel, nowMs);
    if (!m_adapter->writeBinary(abs, body, hints)) return nullptr;

    auto owned = std::make_unique<TFile>(this, rel);
    FileStat st; st.sizeBytes = body.size(); st.mtimeMs = nowMs; st.ctimeMs = nowMs;
    owned->stat = st;

    TFolder *parent = m_root;
    const int slash = rel.lastIndexOf(QLatin1Char('/'));
    if (slash > 0) {
        if (auto *p = getFolderByPath(rel.left(slash))) parent = p;
        else {
            // Build intermediate folders.
            QStringList segments = rel.left(slash).split(QLatin1Char('/'));
            QString cur;
            TFolder *p2 = m_root;
            for (const QString &seg : segments) {
                cur = cur.isEmpty() ? seg : cur + QLatin1Char('/') + seg;
                if (auto *existing = getFolderByPath(cur)) { p2 = existing; continue; }
                auto f = std::make_unique<TFolder>(this, cur);
                f->parent = p2;
                p2->children.append(f.get());
                TFolder *raw = f.get();
                m_fileMap.insert(cur, std::move(f));
                Q_EMIT created(raw);
                p2 = raw;
            }
            parent = p2;
        }
    }

    owned->parent = parent;
    parent->children.append(owned.get());
    TFile *raw = owned.get();
    m_fileMap.insert(rel, std::move(owned));
    m_readCache.insert(rel, body);
    Q_EMIT created(raw);
    return raw;
}

TFile *Vault::createBinary(const QString &path, const QByteArray &body)
{
    return create(path, body);
}

TFolder *Vault::createFolder(const QString &path)
{
    if (!m_adapter) return nullptr;
    const QString rel = Vault::Paths::normalize(path);
    if (m_fileMap.contains(rel)) return nullptr;
    const QString abs = m_basePath + QLatin1Char('/') + rel;
    if (!m_adapter->mkpath(abs)) return nullptr;

    auto owned = std::make_unique<TFolder>(this, rel);
    TFolder *parent = m_root;
    const int slash = rel.lastIndexOf(QLatin1Char('/'));
    if (slash > 0) {
        if (auto *p = getFolderByPath(rel.left(slash))) parent = p;
    }
    owned->parent = parent;
    parent->children.append(owned.get());
    TFolder *raw = owned.get();
    m_fileMap.insert(rel, std::move(owned));
    Q_EMIT created(raw);
    return raw;
}
```

- [ ] **Step 5: Build + run**

```bash
cmake --build build
cd build && ctest -R tst_vault_create --output-on-failure
```

- [ ] **Step 6: Commit**

```bash
git add libs/vault
git commit -m "feat(vault): create/createBinary/createFolder (Q.0 P3 T3.5)"
```

---

## Task 3.6: `Vault::rename` + `remove` + `copy`

**Files:**
- Modify: `libs/vault/include/corbomite/vault/Vault.h`
- Modify: `libs/vault/src/Vault.cpp`
- Create: `libs/vault/tests/tst_vault_rename_remove.cpp`

- [ ] **Step 1: Write failing test**

Create `libs/vault/tests/tst_vault_rename_remove.cpp`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QFile>

#include "corbomite/vault/Vault.h"
#include "corbomite/vault/TFile.h"
#include "corbomite/vault/TFolder.h"
#include "corbomite/storage/FileSystemAdapter.h"

class TestVaultRenameRemove : public QObject
{
    Q_OBJECT
private slots:
    void renameUpdatesPathAndFireSignal();
    void removeDeletesFileAndFireSignal();
    void removeTombstonesHandle();
    void copyDuplicatesFile();
};

void TestVaultRenameRemove::renameUpdatesPathAndFireSignal()
{
    QTemporaryDir dir;
    QFile f(dir.path() + "/a.md"); f.open(QIODevice::WriteOnly); f.write("x"); f.close();

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    auto *tf = vault.getFileByPath(QStringLiteral("a.md"));
    QSignalSpy spy(&vault, &Corbomite::Vault::renamed);

    QVERIFY(vault.rename(tf, QStringLiteral("b.md")));
    QCOMPARE(tf->path, QStringLiteral("b.md"));
    QCOMPARE(spy.count(), 1);
    QVERIFY(vault.getFileByPath(QStringLiteral("b.md")));
    QVERIFY(!vault.getFileByPath(QStringLiteral("a.md")));
}

void TestVaultRenameRemove::removeDeletesFileAndFireSignal()
{
    QTemporaryDir dir;
    QFile f(dir.path() + "/a.md"); f.open(QIODevice::WriteOnly); f.write("x"); f.close();

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    auto *tf = vault.getFileByPath(QStringLiteral("a.md"));
    QSignalSpy spy(&vault, &Corbomite::Vault::deletedFile);

    QVERIFY(vault.remove(tf));
    QCOMPARE(spy.count(), 1);
    QVERIFY(!QFileInfo::exists(dir.path() + "/a.md"));
}

void TestVaultRenameRemove::removeTombstonesHandle()
{
    QTemporaryDir dir;
    QFile f(dir.path() + "/a.md"); f.open(QIODevice::WriteOnly); f.write("x"); f.close();

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    auto *tf = vault.getFileByPath(QStringLiteral("a.md"));
    vault.remove(tf);
    QCOMPARE(tf->deleted, true);
}

void TestVaultRenameRemove::copyDuplicatesFile()
{
    QTemporaryDir dir;
    QFile f(dir.path() + "/a.md"); f.open(QIODevice::WriteOnly); f.write("x"); f.close();

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    auto *tf = vault.getFileByPath(QStringLiteral("a.md"));
    QVERIFY(vault.copy(tf, QStringLiteral("b.md")));
    QVERIFY(QFileInfo::exists(dir.path() + "/b.md"));
    QVERIFY(QFileInfo::exists(dir.path() + "/a.md"));
}

QTEST_MAIN(TestVaultRenameRemove)
#include "tst_vault_rename_remove.moc"
```

- [ ] **Step 2: Register test**

```cmake
add_executable(tst_vault_rename_remove tst_vault_rename_remove.cpp)
target_link_libraries(tst_vault_rename_remove
    PRIVATE Qt6::Test Corbomite::Vault Corbomite::Storage)
add_test(NAME tst_vault_rename_remove COMMAND tst_vault_rename_remove)
set_tests_properties(tst_vault_rename_remove PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
```

- [ ] **Step 3: Declare in Vault.h**

```cpp
    bool rename(TAbstractFile *f, const QString &newPath);
    bool remove(TAbstractFile *f, bool recursive = false);
    bool copy(TAbstractFile *f, const QString &newPath);
```

- [ ] **Step 4: Implement**

```cpp
bool Vault::rename(TAbstractFile *f, const QString &newPath)
{
    if (!f || !m_adapter) return false;
    const QString oldRel = f->path;
    const QString newRel = Vault::Paths::normalize(newPath);
    if (oldRel == newRel) return true;
    if (m_fileMap.contains(newRel)) return false;

    const QString oldAbs = m_basePath + QLatin1Char('/') + oldRel;
    const QString newAbs = m_basePath + QLatin1Char('/') + newRel;
    m_adapter->mkpath(QFileInfo(newAbs).absolutePath());
    if (!m_adapter->rename(oldAbs, newAbs)) return false;

    auto it = m_fileMap.find(oldRel);
    if (it == m_fileMap.end()) return false;
    std::unique_ptr<TAbstractFile> node = std::move(it.value());
    m_fileMap.erase(it);

    // Reparent: detach from old parent, attach to new.
    if (TFolder *p = node->parent) p->children.removeAll(node.get());
    TFolder *newParent = m_root;
    const int slash = newRel.lastIndexOf(QLatin1Char('/'));
    if (slash > 0) {
        if (auto *p = getFolderByPath(newRel.left(slash))) newParent = p;
    }
    node->parent = newParent;
    newParent->children.append(node.get());
    node->setPath(newRel);

    // Cache move
    if (m_readCache.contains(oldRel)) {
        m_readCache.insert(newRel, m_readCache.take(oldRel));
    }

    TAbstractFile *raw = node.get();
    m_fileMap.insert(newRel, std::move(node));
    Q_EMIT renamed(raw, oldRel);
    return true;
}

bool Vault::remove(TAbstractFile *f, bool recursive)
{
    if (!f || !m_adapter) return false;
    const QString rel = f->path;
    const QString abs = m_basePath + QLatin1Char('/') + rel;

    if (auto *folder = dynamic_cast<TFolder *>(f)) {
        if (!recursive && !folder->children.isEmpty()) return false;
        if (!m_adapter->rmdir(abs)) return false;
    } else {
        if (!m_adapter->remove(abs)) return false;
    }

    auto it = m_fileMap.find(rel);
    if (it == m_fileMap.end()) return false;
    std::unique_ptr<TAbstractFile> node = std::move(it.value());
    m_fileMap.erase(it);

    if (TFolder *p = node->parent) p->children.removeAll(node.get());
    node->deleted = true;
    m_readCache.remove(rel);

    TAbstractFile *raw = node.get();
    m_pendingDelete.push_back(std::move(node));
    Q_EMIT deletedFile(raw);
    QTimer::singleShot(0, this, [this] { m_pendingDelete.clear(); });
    return true;
}

bool Vault::copy(TAbstractFile *f, const QString &newPath)
{
    if (!f || !m_adapter) return false;
    const QString newRel = Vault::Paths::normalize(newPath);
    if (m_fileMap.contains(newRel)) return false;

    if (auto *file = dynamic_cast<TFile *>(f)) {
        const QByteArray body = read(file);
        return create(newRel, body) != nullptr;
    }
    // Recursive folder copy — deferred per spec §11.
    return false;
}
```

- [ ] **Step 5: Build + run**

```bash
cmake --build build
cd build && ctest -R tst_vault_rename_remove --output-on-failure
```

- [ ] **Step 6: Commit**

```bash
git add libs/vault
git commit -m "feat(vault): rename/remove/copy (Q.0 P3 T3.6)"
```

---

## Task 3.7: `Vault::trash` + absorb `VaultTrash`

**Files:**
- Modify: `libs/vault/include/corbomite/vault/Vault.h`
- Modify: `libs/vault/src/Vault.cpp`
- Create: `libs/vault/tests/tst_vault_trash.cpp`
- Delete: `libs/storage/include/corbomite/storage/VaultTrash.h`
- Delete: `libs/storage/src/VaultTrash.cpp`
- Modify: `libs/storage/CMakeLists.txt`
- Migrate: `VaultTrash` callers to `vault->trash`

- [ ] **Step 1: Identify callers**

```bash
grep -rn "VaultTrash" libs/ src/ tests/ 2>/dev/null
```

- [ ] **Step 2: Write failing test**

Create `libs/vault/tests/tst_vault_trash.cpp`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QTemporaryDir>
#include <QFile>

#include "corbomite/vault/Vault.h"
#include "corbomite/vault/TFile.h"
#include "corbomite/storage/FileSystemAdapter.h"

class TestVaultTrash : public QObject
{
    Q_OBJECT
private slots:
    void trashLocalMovesToDotTrash();
    void trashCollisionRenames();
};

void TestVaultTrash::trashLocalMovesToDotTrash()
{
    QTemporaryDir dir;
    QFile f(dir.path() + "/a.md"); f.open(QIODevice::WriteOnly); f.write("x"); f.close();

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    auto *tf = vault.getFileByPath(QStringLiteral("a.md"));
    QVERIFY(vault.trash(tf, /*useSystem=*/false));
    QVERIFY(QFileInfo::exists(dir.path() + "/.trash/a.md"));
    QVERIFY(!QFileInfo::exists(dir.path() + "/a.md"));
}

void TestVaultTrash::trashCollisionRenames()
{
    QTemporaryDir dir;
    QDir().mkpath(dir.path() + "/.trash");
    QFile existing(dir.path() + "/.trash/a.md");
    existing.open(QIODevice::WriteOnly); existing.write("old"); existing.close();

    QFile f(dir.path() + "/a.md"); f.open(QIODevice::WriteOnly); f.write("new"); f.close();

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    auto *tf = vault.getFileByPath(QStringLiteral("a.md"));
    QVERIFY(vault.trash(tf, false));
    QVERIFY(QFileInfo::exists(dir.path() + "/.trash/a 2.md"));
}

QTEST_MAIN(TestVaultTrash)
#include "tst_vault_trash.moc"
```

- [ ] **Step 3: Register test**

```cmake
add_executable(tst_vault_trash tst_vault_trash.cpp)
target_link_libraries(tst_vault_trash
    PRIVATE Qt6::Test Corbomite::Vault Corbomite::Storage)
add_test(NAME tst_vault_trash COMMAND tst_vault_trash)
set_tests_properties(tst_vault_trash PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
```

- [ ] **Step 4: Declare + implement**

Vault.h:

```cpp
    bool trash(TAbstractFile *f, bool useSystem);
```

Vault.cpp:

```cpp
bool Vault::trash(TAbstractFile *f, bool useSystem)
{
    if (!f || !m_adapter) return false;
    if (dynamic_cast<TFolder *>(f) && f->path == QStringLiteral("/")) return false;

    if (useSystem) {
        const QString abs = m_basePath + QLatin1Char('/') + f->path;
        if (m_adapter->moveToTrash(abs)) {
            return remove(f, /*recursive=*/true) || true;  // tree state already gone
        }
        // Fall through to local trash.
    }

    // Local trash: move into <vault>/.trash/<base><suffix>.<ext>
    const QString rel = f->path;
    const QFileInfo fi(rel);
    const QString base = fi.completeBaseName();
    const QString ext  = fi.suffix();
    const QString trashRoot = QStringLiteral(".trash");
    m_adapter->mkpath(m_basePath + QLatin1Char('/') + trashRoot);

    QString candidate = base + (ext.isEmpty() ? QString() : (QLatin1Char('.') + ext));
    QString candidateRel = trashRoot + QLatin1Char('/') + candidate;
    int n = 2;
    while (QFileInfo::exists(m_basePath + QLatin1Char('/') + candidateRel)) {
        candidate = base + QStringLiteral(" ") + QString::number(n)
                  + (ext.isEmpty() ? QString() : (QLatin1Char('.') + ext));
        candidateRel = trashRoot + QLatin1Char('/') + candidate;
        ++n;
    }

    if (!m_adapter->rename(m_basePath + QLatin1Char('/') + rel,
                           m_basePath + QLatin1Char('/') + candidateRel)) {
        return false;
    }

    auto it = m_fileMap.find(rel);
    if (it != m_fileMap.end()) {
        std::unique_ptr<TAbstractFile> node = std::move(it.value());
        m_fileMap.erase(it);
        if (TFolder *p = node->parent) p->children.removeAll(node.get());
        node->deleted = true;
        m_readCache.remove(rel);
        TAbstractFile *raw = node.get();
        m_pendingDelete.push_back(std::move(node));
        Q_EMIT deletedFile(raw);
        QTimer::singleShot(0, this, [this] { m_pendingDelete.clear(); });
    }
    return true;
}
```

- [ ] **Step 5: Delete `VaultTrash`**

```bash
rm libs/storage/include/corbomite/storage/VaultTrash.h libs/storage/src/VaultTrash.cpp
```

Remove from `libs/storage/CMakeLists.txt`. Migrate callers (Step 1) to `vault->trash(...)`.

- [ ] **Step 6: Build + run**

```bash
cmake --build build
cd build && ctest -R tst_vault_trash --output-on-failure
```

- [ ] **Step 7: Commit**

```bash
git add -u libs src tests
git commit -m "feat(vault): Vault::trash + delete VaultTrash (Q.0 P3 T3.7)"
```

---

## Task 3.8: Phase 3 verification

- [ ] **Step 1: Full rebuild**

```bash
rm -rf build && cmake -S . -B build -DCORBOMITE_DEV_BUILD=ON && cmake --build build
```

- [ ] **Step 2: Full test run**

```bash
cd build && ctest --output-on-failure -E "tst_markoff_inline_math|tst_renderengine|tst_completion_popup|tst_benchmark_layout"
```

Expected: green outside the 4 known-flaky.

- [ ] **Step 3: Update PROJECT-STATE.md + commit**

```bash
git add docs/PROJECT-STATE.md
git commit -m "docs: Cluster Q.0 Phase 3 landed (Q.0 P3 T3.8)"
```

---

# Phase 4 — Config-dir I/O (`.obsidian/*.json`)

Goal: Implement `Vault::readConfigJson` / `writeConfigJson` / `deleteConfigJson` + `configDir()` / `setConfigDir()`. Delegate to the existing `VaultConfig` primitive in `libs/storage/`.

## Task 4.1: `configDir()` / `setConfigDir()` + tests

**Files:**
- Modify: `libs/vault/include/corbomite/vault/Vault.h` (add getters/setters + private `m_configDir`)
- Modify: `libs/vault/src/Vault.cpp`
- Create: `libs/vault/tests/tst_vault_configdir.cpp`

- [ ] **Step 1: Write failing test**

Create `libs/vault/tests/tst_vault_configdir.cpp`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include "corbomite/vault/Vault.h"
#include "corbomite/storage/FileSystemAdapter.h"

class TestVaultConfigDir : public QObject
{
    Q_OBJECT
private slots:
    void defaultsToDotObsidian();
    void setConfigDirAcceptsValid();
    void setConfigDirRejectsInvalid();
};

void TestVaultConfigDir::defaultsToDotObsidian()
{
    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault v(&fs);
    QCOMPARE(v.configDir(), QStringLiteral(".obsidian"));
}

void TestVaultConfigDir::setConfigDirAcceptsValid()
{
    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault v(&fs);
    v.setConfigDir(QStringLiteral(".custom"));
    QCOMPARE(v.configDir(), QStringLiteral(".custom"));
}

void TestVaultConfigDir::setConfigDirRejectsInvalid()
{
    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault v(&fs);
    v.setConfigDir(QStringLiteral("notleadingdot"));
    QCOMPARE(v.configDir(), QStringLiteral(".obsidian"));  // rejected → fallback
    v.setConfigDir(QStringLiteral("."));
    QCOMPARE(v.configDir(), QStringLiteral(".obsidian"));  // bare dot rejected
}

QTEST_MAIN(TestVaultConfigDir)
#include "tst_vault_configdir.moc"
```

- [ ] **Step 2: Register test**

```cmake
add_executable(tst_vault_configdir tst_vault_configdir.cpp)
target_link_libraries(tst_vault_configdir
    PRIVATE Qt6::Test Corbomite::Vault Corbomite::Storage)
add_test(NAME tst_vault_configdir COMMAND tst_vault_configdir)
set_tests_properties(tst_vault_configdir PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
```

- [ ] **Step 3: Declare + implement**

In `Vault.h` under `// ---- Lifecycle ----`:

```cpp
    QString configDir() const { return m_configDir; }
    void    setConfigDir(const QString &d);
```

Under `private:`:
```cpp
    QString m_configDir = QStringLiteral(".obsidian");
```

In `Vault.cpp`:

```cpp
void Vault::setConfigDir(const QString &d)
{
    if (d.isEmpty() || d == QStringLiteral(".")) return;
    if (!d.startsWith(QLatin1Char('.'))) return;
    m_configDir = d;
}
```

- [ ] **Step 4: Build + run**

```bash
cmake --build build
cd build && ctest -R tst_vault_configdir --output-on-failure
```

- [ ] **Step 5: Commit**

```bash
git add libs/vault
git commit -m "feat(vault): configDir getter + validated setter (Q.0 P4 T4.1)"
```

---

## Task 4.2: `readConfigJson` / `writeConfigJson` / `deleteConfigJson`

**Files:**
- Modify: `libs/vault/include/corbomite/vault/Vault.h`
- Modify: `libs/vault/src/Vault.cpp` (delegate to `Corbomite::VaultConfig` from libs/storage)
- Create: `libs/vault/tests/tst_vault_config_json.cpp`

- [ ] **Step 1: Write failing test**

Create `libs/vault/tests/tst_vault_config_json.cpp`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QTemporaryDir>
#include <QJsonObject>
#include <QFile>

#include "corbomite/vault/Vault.h"
#include "corbomite/storage/FileSystemAdapter.h"

class TestVaultConfigJson : public QObject
{
    Q_OBJECT
private slots:
    void writeReadRoundTrip();
    void writeCreatesConfigDir();
    void deleteRemovesFile();
    void readMissingReturnsNull();
};

void TestVaultConfigJson::writeReadRoundTrip()
{
    QTemporaryDir dir;
    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault v(&fs);
    v.load(dir.path());

    QJsonObject obj{{"k", 1}};
    QVERIFY(v.writeConfigJson(QStringLiteral("test"), obj));
    const auto loaded = v.readConfigJson(QStringLiteral("test")).toObject();
    QCOMPARE(loaded.value("k").toInt(), 1);
}

void TestVaultConfigJson::writeCreatesConfigDir()
{
    QTemporaryDir dir;
    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault v(&fs);
    v.load(dir.path());
    v.writeConfigJson(QStringLiteral("x"), QJsonObject{});
    QVERIFY(QFileInfo(dir.path() + "/.obsidian/x.json").exists());
}

void TestVaultConfigJson::deleteRemovesFile()
{
    QTemporaryDir dir;
    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault v(&fs);
    v.load(dir.path());
    v.writeConfigJson(QStringLiteral("x"), QJsonObject{});
    QVERIFY(v.deleteConfigJson(QStringLiteral("x")));
    QVERIFY(!QFileInfo(dir.path() + "/.obsidian/x.json").exists());
}

void TestVaultConfigJson::readMissingReturnsNull()
{
    QTemporaryDir dir;
    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault v(&fs);
    v.load(dir.path());
    QVERIFY(v.readConfigJson(QStringLiteral("missing")).isNull());
}

QTEST_MAIN(TestVaultConfigJson)
#include "tst_vault_config_json.moc"
```

- [ ] **Step 2: Register test**

```cmake
add_executable(tst_vault_config_json tst_vault_config_json.cpp)
target_link_libraries(tst_vault_config_json
    PRIVATE Qt6::Test Corbomite::Vault Corbomite::Storage)
add_test(NAME tst_vault_config_json COMMAND tst_vault_config_json)
set_tests_properties(tst_vault_config_json PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
```

- [ ] **Step 3: Declare + implement**

Vault.h:

```cpp
    QJsonValue readConfigJson(const QString &name) const;
    bool       writeConfigJson(const QString &name, const QJsonValue &value);
    bool       deleteConfigJson(const QString &name);
```

Add `#include <QJsonValue>` and `#include <QJsonDocument>` to the header.

Vault.cpp:

```cpp
#include <QJsonDocument>

namespace {
QString configJsonAbs(const QString &basePath, const QString &configDir,
                      const QString &name)
{
    return basePath + QLatin1Char('/') + configDir + QLatin1Char('/')
         + name + QStringLiteral(".json");
}
}

QJsonValue Vault::readConfigJson(const QString &name) const
{
    if (!m_adapter || m_basePath.isEmpty()) return {};
    const QString abs = configJsonAbs(m_basePath, m_configDir, name);
    auto body = m_adapter->readBinary(abs);
    if (!body.has_value()) return {};
    QJsonParseError err;
    const auto doc = QJsonDocument::fromJson(*body, &err);
    if (err.error != QJsonParseError::NoError) return {};
    if (doc.isObject()) return doc.object();
    if (doc.isArray())  return doc.array();
    return {};
}

bool Vault::writeConfigJson(const QString &name, const QJsonValue &value)
{
    if (!m_adapter || m_basePath.isEmpty()) return false;
    m_adapter->mkpath(m_basePath + QLatin1Char('/') + m_configDir);
    const QString abs = configJsonAbs(m_basePath, m_configDir, name);

    QJsonDocument doc;
    if (value.isObject()) doc = QJsonDocument(value.toObject());
    else if (value.isArray()) doc = QJsonDocument(value.toArray());
    else return false;

    const QByteArray body = doc.toJson(QJsonDocument::Indented);
    return m_adapter->writeBinary(abs, body);
}

bool Vault::deleteConfigJson(const QString &name)
{
    if (!m_adapter) return false;
    return m_adapter->remove(configJsonAbs(m_basePath, m_configDir, name));
}
```

- [ ] **Step 4: Build + run**

```bash
cmake --build build
cd build && ctest -R tst_vault_config_json --output-on-failure
```

- [ ] **Step 5: Commit**

```bash
git add libs/vault
git commit -m "feat(vault): readConfigJson/writeConfigJson/deleteConfigJson (Q.0 P4 T4.2)"
```

---

# Phase 5 — FileManager

Goal: Build `Corbomite::FileManager` with `renameFile` (link rewrite), `processFrontMatter` (absorbs `FrontMatterWriter`), `getNewFileParent`, `createNewMarkdownFile`, `createNewFolder`, `getAvailablePathForAttachment`, `insertIntoFile`, `generateMarkdownLink`, `trashFile`. Depends on `Vault *` + `MetadataCache *`.

## Task 5.1: FileManager skeleton + constructor + tests

**Files:**
- Create: `libs/vault/include/corbomite/vault/FileManager.h`
- Create: `libs/vault/src/FileManager.cpp`
- Create: `libs/vault/tests/tst_file_manager.cpp`

- [ ] **Step 1: Write failing test**

Create `libs/vault/tests/tst_file_manager.cpp`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include "corbomite/vault/FileManager.h"
#include "corbomite/vault/Vault.h"
#include "corbomite/storage/FileSystemAdapter.h"

class TestFileManager : public QObject
{
    Q_OBJECT
private slots:
    void constructsWithVault();
};

void TestFileManager::constructsWithVault()
{
    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    Corbomite::FileManager fm(&vault, /*cache=*/nullptr);
    QVERIFY(fm.vault() == &vault);
}

QTEST_MAIN(TestFileManager)
#include "tst_file_manager.moc"
```

- [ ] **Step 2: Register test**

```cmake
add_executable(tst_file_manager tst_file_manager.cpp)
target_link_libraries(tst_file_manager
    PRIVATE Qt6::Test Corbomite::Vault Corbomite::Storage)
add_test(NAME tst_file_manager COMMAND tst_file_manager)
set_tests_properties(tst_file_manager PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
```

- [ ] **Step 3: Create header**

Create `libs/vault/include/corbomite/vault/FileManager.h`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <functional>

#include <QObject>
#include <QString>
#include <QVariantMap>

namespace Corbomite {

class Vault;
class TAbstractFile;
class TFile;
class TFolder;
class MetadataCache;

/// Higher-level Vault operations: link-aware rename, frontmatter mutation,
/// new-file placement, attachment placement, link generation, trash routing.
/// Depends on Vault + MetadataCache. Subsumes the legacy
/// `Corbomite::FrontMatterWriter`.
class FileManager : public QObject
{
    Q_OBJECT
public:
    FileManager(Vault *vault, MetadataCache *cache, QObject *parent = nullptr);

    Vault         *vault() const { return m_vault; }
    MetadataCache *metadataCache() const { return m_cache; }

    // ---- Rename with link rewrite ----
    bool renameFile(TAbstractFile *f, const QString &newPath);

    // ---- Atomic frontmatter mutation ----
    using FrontMatterMutator = std::function<void(QVariantMap &)>;
    bool processFrontMatter(TFile *f, FrontMatterMutator mut);

    // ---- Bulk property ops (deferred bodies — declared only) ----
    bool deleteProperty(const QString &key);
    bool renameProperty(const QString &oldK, const QString &newK);

    // ---- New-file placement ----
    TFolder *getNewFileParent(const QString &hintPath,
                              const QString &filename = {}) const;
    TFile   *createNewMarkdownFile(TFolder *parent,
                                   const QString &name,
                                   const QByteArray &content = {});
    TFile   *createNewMarkdownFileFromLinktext(const QString &linkText,
                                               const QString &hintPath);
    TFolder *createNewFolder(TFolder *parent);

    // ---- Attachments ----
    QString getAvailablePathForAttachment(const QString &linktext,
                                          const QString &sourcePathHint = {}) const;

    // ---- Content merge ----
    enum class InsertMode { Append, Prepend };
    bool insertIntoFile(TFile *f, const QByteArray &content, InsertMode mode);

    // ---- Link generation ----
    QString generateMarkdownLink(TFile *target,
                                 const QString &sourcePath,
                                 const QString &subpath = {},
                                 const QString &displayText = {}) const;

    // ---- Trash router ----
    bool trashFile(TAbstractFile *f);

Q_SIGNALS:
    void renameStarted(Corbomite::TAbstractFile *f, const QString &newPath);
    void renameFinished(Corbomite::TAbstractFile *f, const QString &oldPath);
    void linkUpdateProgress(int done, int total);

private:
    Vault         *m_vault;
    MetadataCache *m_cache;
};

} // namespace Corbomite
```

- [ ] **Step 4: Create impl skeleton**

Create `libs/vault/src/FileManager.cpp`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/vault/FileManager.h"
#include "corbomite/vault/Vault.h"

namespace Corbomite {

FileManager::FileManager(Vault *vault, MetadataCache *cache, QObject *parent)
    : QObject(parent)
    , m_vault(vault)
    , m_cache(cache)
{
}

// Method bodies are implemented in subsequent Phase-5 tasks. Stubs below
// return sensible defaults so builds remain green; each task replaces one.

bool FileManager::renameFile(TAbstractFile *, const QString &) { return false; }
bool FileManager::processFrontMatter(TFile *, FrontMatterMutator) { return false; }
bool FileManager::deleteProperty(const QString &) { return false; }
bool FileManager::renameProperty(const QString &, const QString &) { return false; }
TFolder *FileManager::getNewFileParent(const QString &, const QString &) const { return nullptr; }
TFile *FileManager::createNewMarkdownFile(TFolder *, const QString &, const QByteArray &) { return nullptr; }
TFile *FileManager::createNewMarkdownFileFromLinktext(const QString &, const QString &) { return nullptr; }
TFolder *FileManager::createNewFolder(TFolder *) { return nullptr; }
QString FileManager::getAvailablePathForAttachment(const QString &, const QString &) const { return {}; }
bool FileManager::insertIntoFile(TFile *, const QByteArray &, InsertMode) { return false; }
QString FileManager::generateMarkdownLink(TFile *, const QString &, const QString &, const QString &) const { return {}; }
bool FileManager::trashFile(TAbstractFile *f) { return m_vault && m_vault->trash(f, false); }

} // namespace Corbomite
```

- [ ] **Step 5: Register sources in libs/vault/CMakeLists.txt**

Add:
```cmake
    include/corbomite/vault/FileManager.h
    src/FileManager.cpp
```

`target_link_libraries(vault PUBLIC ...)` already includes Corbomite::Storage. If FileManager needs MetadataCache, that's in Corbomite::Storage — already linked.

- [ ] **Step 6: Build + run**

```bash
cmake --build build
cd build && ctest -R tst_file_manager --output-on-failure
```

- [ ] **Step 7: Commit**

```bash
git add libs/vault
git commit -m "feat(vault): FileManager class skeleton (Q.0 P5 T5.1)"
```

---

## Task 5.2: `FileManager::processFrontMatter` + absorb `FrontMatterWriter`

**Files:**
- Modify: `libs/vault/src/FileManager.cpp` (real body)
- Modify: `libs/vault/CMakeLists.txt` (no change — MarkoffParser::MarkoffParser link added if needed)
- Create: `libs/vault/tests/tst_file_manager_frontmatter.cpp`
- Delete: `libs/core/include/corbomite/core/FrontMatterWriter.h`
- Delete: `libs/core/src/FrontMatterWriter.cpp`
- Modify: `libs/core/CMakeLists.txt`
- Migrate: every caller of `Corbomite::FrontMatterWriter::process(...)` → `fileManager.processFrontMatter(...)`

- [ ] **Step 1: Identify callers**

```bash
grep -rn "FrontMatterWriter" libs/ src/ tests/ 2>/dev/null
```

Record sites. Properties panel (`src/sidebar/PropertiesPanel.cpp`) is the largest caller.

- [ ] **Step 2: Write failing test**

Create `libs/vault/tests/tst_file_manager_frontmatter.cpp`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QTemporaryDir>
#include <QFile>

#include "corbomite/vault/FileManager.h"
#include "corbomite/vault/Vault.h"
#include "corbomite/vault/TFile.h"
#include "corbomite/storage/FileSystemAdapter.h"

class TestFileManagerFrontmatter : public QObject
{
    Q_OBJECT
private slots:
    void addsFrontMatterWhenAbsent();
    void mutatesExistingFrontMatter();
    void preservesBodyVerbatim();
    void noopOnNonMarkdown();
};

namespace {
void writeFile(const QString &p, const QByteArray &b)
{
    QFile f(p); f.open(QIODevice::WriteOnly); f.write(b); f.close();
}
QByteArray readFileAll(const QString &p)
{
    QFile f(p); f.open(QIODevice::ReadOnly); return f.readAll();
}
}

void TestFileManagerFrontmatter::addsFrontMatterWhenAbsent()
{
    QTemporaryDir dir;
    writeFile(dir.path() + "/a.md", "body");

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs); vault.load(dir.path());
    Corbomite::FileManager fm(&vault, nullptr);

    auto *tf = vault.getFileByPath(QStringLiteral("a.md"));
    QVERIFY(fm.processFrontMatter(tf, [](QVariantMap &m) {
        m.insert(QStringLiteral("tag"), QStringLiteral("new"));
    }));

    const QByteArray after = readFileAll(dir.path() + "/a.md");
    QVERIFY(after.startsWith("---\n"));
    QVERIFY(after.contains("tag: new"));
    QVERIFY(after.contains("body"));
}

void TestFileManagerFrontmatter::mutatesExistingFrontMatter()
{
    QTemporaryDir dir;
    writeFile(dir.path() + "/a.md", "---\ntag: old\n---\nbody\n");

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs); vault.load(dir.path());
    Corbomite::FileManager fm(&vault, nullptr);

    auto *tf = vault.getFileByPath(QStringLiteral("a.md"));
    fm.processFrontMatter(tf, [](QVariantMap &m) {
        m.insert(QStringLiteral("tag"), QStringLiteral("new"));
    });

    const QByteArray after = readFileAll(dir.path() + "/a.md");
    QVERIFY(after.contains("tag: new"));
    QVERIFY(!after.contains("tag: old"));
    QVERIFY(after.contains("body"));
}

void TestFileManagerFrontmatter::preservesBodyVerbatim()
{
    QTemporaryDir dir;
    writeFile(dir.path() + "/a.md", "---\nk: v\n---\nLine1\nLine2\n");

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs); vault.load(dir.path());
    Corbomite::FileManager fm(&vault, nullptr);

    auto *tf = vault.getFileByPath(QStringLiteral("a.md"));
    fm.processFrontMatter(tf, [](QVariantMap &) {});

    QVERIFY(readFileAll(dir.path() + "/a.md").contains("Line1\nLine2\n"));
}

void TestFileManagerFrontmatter::noopOnNonMarkdown()
{
    QTemporaryDir dir;
    writeFile(dir.path() + "/a.canvas", "{}");

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs); vault.load(dir.path());
    Corbomite::FileManager fm(&vault, nullptr);

    auto *tf = vault.getFileByPath(QStringLiteral("a.canvas"));
    QCOMPARE(fm.processFrontMatter(tf, [](QVariantMap &) {}), false);
}

QTEST_MAIN(TestFileManagerFrontmatter)
#include "tst_file_manager_frontmatter.moc"
```

- [ ] **Step 3: Register test**

```cmake
add_executable(tst_file_manager_frontmatter tst_file_manager_frontmatter.cpp)
target_link_libraries(tst_file_manager_frontmatter
    PRIVATE Qt6::Test Corbomite::Vault Corbomite::Storage)
add_test(NAME tst_file_manager_frontmatter COMMAND tst_file_manager_frontmatter)
set_tests_properties(tst_file_manager_frontmatter PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
```

- [ ] **Step 4: Implement `processFrontMatter`**

The legacy `FrontMatterWriter::process` lives at `libs/core/src/FrontMatterWriter.cpp`. Read it first to understand the exact YAML-mutation contract; carry the logic verbatim into `FileManager`. Summary of what the impl does: split the body on `---\n...\n---\n`, parse YAML via `Markoff::YamlValue::parse`, apply the mutator on a `QVariantMap` view, re-stringify preserving key order, write the combined `---\n<yaml>---\n<body>` via `vault->process`.

Edit `libs/vault/src/FileManager.cpp`. Replace the `processFrontMatter` stub with:

```cpp
#include "corbomite/vault/TFile.h"
#include <MarkoffParser/YamlValue.h>  // existing; adjust include path per repo

bool FileManager::processFrontMatter(TFile *f, FrontMatterMutator mut)
{
    if (!f || !m_vault) return false;
    if (f->extension != QStringLiteral("md")) return false;

    return m_vault->process(f, [&](const QByteArray &cur) -> QByteArray {
        // Frontmatter detection: does the body start with `---\n...\n---\n`?
        QByteArray fm;
        QByteArray body = cur;
        const QByteArray fence = "---\n";

        if (cur.startsWith(fence)) {
            const int close = cur.indexOf("\n---\n", fence.size());
            if (close >= 0) {
                fm = cur.mid(fence.size(), close - fence.size());
                body = cur.mid(close + 5);
            }
        }

        QVariantMap map;
        if (!fm.isEmpty()) {
            auto parsed = Markoff::YamlValue::parse(QString::fromUtf8(fm));
            if (parsed.isMap()) map = parsed.toVariantMap();
        }
        mut(map);

        // Re-stringify. Use ordered keys via the existing helper in
        // Markoff::YamlValue::dump — see libs/core/src/FrontMatterWriter.cpp
        // for the idiom it inherits. For Phase 5 Task 5.2 we carry that idiom
        // over verbatim.
        const QString yaml = Markoff::YamlValue::fromVariantMap(map).dump();

        QByteArray out;
        out.append("---\n");
        out.append(yaml.toUtf8());
        if (!yaml.endsWith(QLatin1Char('\n'))) out.append('\n');
        out.append("---\n");
        out.append(body);
        return out;
    });
}
```

Add `target_link_libraries(vault PRIVATE MarkoffParser::MarkoffParser)` to `libs/vault/CMakeLists.txt`.

- [ ] **Step 5: Build + run**

```bash
cmake --build build
cd build && ctest -R tst_file_manager_frontmatter --output-on-failure
```

- [ ] **Step 6: Migrate callers of `FrontMatterWriter`**

For each match from Step 1, rewrite:

```cpp
Corbomite::FrontMatterWriter::process(fs, absPath, [](QVariantMap &m) { ... });
```

to:

```cpp
fileManager->processFrontMatter(tf, [](QVariantMap &m) { ... });
```

Callers that don't yet have a FileManager pointer get an inline comment `// TODO Q.0 P6/P7: wire FileManager via MainWindow/CorbomiteApp` and temporarily keep using the old static during Phase 5 — the final delete happens in Phase 10 after all consumers are migrated.

Actually: because we want the commit to stay green, defer the static-delete to Phase 10. Keep `FrontMatterWriter.{h,cpp}` in `libs/core/` until then. Only add an `#ifdef`-free duplicate code path via FileManager. This sidesteps the two-way dependency.

- [ ] **Step 7: Commit**

```bash
git add -u libs src tests
git commit -m "feat(vault): FileManager::processFrontMatter (Q.0 P5 T5.2)"
```

---

## Task 5.3: `FileManager::renameFile` with link rewrite + tests

**Files:**
- Modify: `libs/vault/src/FileManager.cpp`
- Create: `libs/vault/tests/tst_file_manager_rename.cpp`

The Obsidian contract is documented in `docs/obsidian-audit/domains/vault.md` §`FileManager.renameFile`. Steps: emit renameStarted; snapshot refs via `MetadataCache::iterateAllRefs`; `vault->rename`; rewrite each snapshot entry whose `resolvedFile` matched the old path, using `vault->process`; emit `linkUpdateProgress`; emit `renameFinished`.

- [ ] **Step 1: Write failing test**

Create `libs/vault/tests/tst_file_manager_rename.cpp`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QFile>

#include "corbomite/vault/FileManager.h"
#include "corbomite/vault/Vault.h"
#include "corbomite/vault/TFile.h"
#include "corbomite/storage/FileSystemAdapter.h"
#include "corbomite/storage/MetadataCache.h"

class TestFileManagerRename : public QObject
{
    Q_OBJECT
private slots:
    void renameRewritesWikiLinks();
    void emitsProgress();
};

namespace {
void writeFile(const QString &p, const QByteArray &b)
{
    QFile f(p); f.open(QIODevice::WriteOnly); f.write(b); f.close();
}
}

void TestFileManagerRename::renameRewritesWikiLinks()
{
    QTemporaryDir dir;
    writeFile(dir.path() + "/Foo.md", "Foo body");
    writeFile(dir.path() + "/Linker.md", "See [[Foo]]");

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs); vault.load(dir.path());
    Corbomite::MetadataCache cache(/*resolver=*/nullptr);
    cache.open(dir.path() + "/.corbomite/metadata-cache.db");
    cache.rebuildVault(dir.path(), {QStringLiteral("Foo.md"), QStringLiteral("Linker.md")});
    Corbomite::FileManager fm(&vault, &cache);

    auto *foo = vault.getFileByPath(QStringLiteral("Foo.md"));
    QVERIFY(fm.renameFile(foo, QStringLiteral("Bar.md")));

    QFile g(dir.path() + "/Linker.md"); g.open(QIODevice::ReadOnly);
    QCOMPARE(g.readAll(), QByteArray("See [[Bar]]"));
}

void TestFileManagerRename::emitsProgress()
{
    // A minimal one-link fixture — progress fires for each referencing file.
    QTemporaryDir dir;
    writeFile(dir.path() + "/Foo.md", "x");
    writeFile(dir.path() + "/A.md", "[[Foo]]");
    writeFile(dir.path() + "/B.md", "[[Foo]]");

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs); vault.load(dir.path());
    Corbomite::MetadataCache cache(nullptr);
    cache.open(dir.path() + "/.corbomite/metadata-cache.db");
    cache.rebuildVault(dir.path(), {QStringLiteral("Foo.md"),
                                    QStringLiteral("A.md"),
                                    QStringLiteral("B.md")});
    Corbomite::FileManager fm(&vault, &cache);

    QSignalSpy spy(&fm, &Corbomite::FileManager::linkUpdateProgress);
    auto *foo = vault.getFileByPath(QStringLiteral("Foo.md"));
    fm.renameFile(foo, QStringLiteral("Bar.md"));

    QVERIFY(spy.count() >= 2);
}

QTEST_MAIN(TestFileManagerRename)
#include "tst_file_manager_rename.moc"
```

- [ ] **Step 2: Register test**

```cmake
add_executable(tst_file_manager_rename tst_file_manager_rename.cpp)
target_link_libraries(tst_file_manager_rename
    PRIVATE Qt6::Test Corbomite::Vault Corbomite::Storage)
add_test(NAME tst_file_manager_rename COMMAND tst_file_manager_rename)
set_tests_properties(tst_file_manager_rename PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
```

- [ ] **Step 3: Implement**

Edit `libs/vault/src/FileManager.cpp`:

```cpp
#include "corbomite/storage/MetadataCache.h"
#include "corbomite/vault/TAbstractFile.h"

bool FileManager::renameFile(TAbstractFile *f, const QString &newPath)
{
    if (!f || !m_vault) return false;
    const QString oldPath = f->path;
    const QString targetBase = QFileInfo(newPath).completeBaseName();
    const QString oldBase    = QFileInfo(oldPath).completeBaseName();

    Q_EMIT renameStarted(f, newPath);

    // Snapshot link refs (who points to this file?). MetadataCache exposes
    // `backlinksFor(targetPath)` — reuse it for a deterministic list of
    // `(sourcePath, resolvedSubpath)` entries.
    QVector<QString> sources;
    if (m_cache) {
        const auto backlinks = m_cache->backlinksFor(oldPath);
        for (const auto &bl : backlinks) sources.append(bl.sourcePath);
    }

    if (!m_vault->rename(f, newPath)) return false;

    // Rewrite [[oldBase]] → [[targetBase]] in each source file.
    int done = 0;
    const int total = sources.size();
    for (const QString &src : sources) {
        auto *sf = m_vault->getFileByPath(src);
        if (!sf) { ++done; continue; }
        m_vault->process(sf, [&](const QByteArray &body) -> QByteArray {
            QString s = QString::fromUtf8(body);
            // Obsidian's link rewrite is broader (markdown-link + headings);
            // Phase 5 ships the [[base]] → [[targetBase]] slice; markdown
            // links + subpath preservation arrive as follow-ups once the
            // first consumer demands them.
            s.replace(QStringLiteral("[[") + oldBase + QStringLiteral("]]"),
                      QStringLiteral("[[") + targetBase + QStringLiteral("]]"));
            s.replace(QStringLiteral("[[") + oldBase + QStringLiteral("|"),
                      QStringLiteral("[[") + targetBase + QStringLiteral("|"));
            return s.toUtf8();
        });
        ++done;
        Q_EMIT linkUpdateProgress(done, total);
    }

    Q_EMIT renameFinished(f, oldPath);
    return true;
}
```

- [ ] **Step 4: Build + run**

```bash
cmake --build build
cd build && ctest -R tst_file_manager_rename --output-on-failure
```

- [ ] **Step 5: Commit**

```bash
git add libs/vault
git commit -m "feat(vault): FileManager::renameFile with link rewrite (Q.0 P5 T5.3)"
```

---

## Task 5.4: `FileManager::createNewMarkdownFile` + `createNewFolder` + `getNewFileParent`

**Files:**
- Modify: `libs/vault/src/FileManager.cpp`
- Create: `libs/vault/tests/tst_file_manager_newfile.cpp`

Legacy equivalents live in `libs/models/src/NoteService.cpp`. Read NoteService first; port `newNote` / `newFolder` / collision-free naming logic into FileManager.

- [ ] **Step 1: Write failing test**

Create `libs/vault/tests/tst_file_manager_newfile.cpp`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QTemporaryDir>

#include "corbomite/vault/FileManager.h"
#include "corbomite/vault/Vault.h"
#include "corbomite/vault/TFile.h"
#include "corbomite/vault/TFolder.h"
#include "corbomite/storage/FileSystemAdapter.h"

class TestFileManagerNewFile : public QObject
{
    Q_OBJECT
private slots:
    void createNewMarkdownFileAtRoot();
    void collisionFreeNaming();
    void getNewFileParentUsesHint();
    void createNewFolderCollisionFree();
};

void TestFileManagerNewFile::createNewMarkdownFileAtRoot()
{
    QTemporaryDir dir;
    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault v(&fs); v.load(dir.path());
    Corbomite::FileManager fm(&v, nullptr);

    auto *f = fm.createNewMarkdownFile(v.getRoot(), QStringLiteral("Note"));
    QVERIFY(f);
    QCOMPARE(f->extension, QStringLiteral("md"));
}

void TestFileManagerNewFile::collisionFreeNaming()
{
    QTemporaryDir dir;
    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault v(&fs); v.load(dir.path());
    Corbomite::FileManager fm(&v, nullptr);

    auto *a = fm.createNewMarkdownFile(v.getRoot(), QStringLiteral("Note"));
    auto *b = fm.createNewMarkdownFile(v.getRoot(), QStringLiteral("Note"));
    QVERIFY(a && b);
    QVERIFY(a->path != b->path);
}

void TestFileManagerNewFile::getNewFileParentUsesHint()
{
    QTemporaryDir dir;
    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault v(&fs); v.load(dir.path());
    v.createFolder(QStringLiteral("sub"));
    Corbomite::FileManager fm(&v, nullptr);

    auto *parent = fm.getNewFileParent(QStringLiteral("sub/x.md"));
    QVERIFY(parent);
    QCOMPARE(parent->path, QStringLiteral("sub"));
}

void TestFileManagerNewFile::createNewFolderCollisionFree()
{
    QTemporaryDir dir;
    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault v(&fs); v.load(dir.path());
    Corbomite::FileManager fm(&v, nullptr);

    auto *a = fm.createNewFolder(v.getRoot());
    auto *b = fm.createNewFolder(v.getRoot());
    QVERIFY(a && b);
    QVERIFY(a->path != b->path);
}

QTEST_MAIN(TestFileManagerNewFile)
#include "tst_file_manager_newfile.moc"
```

- [ ] **Step 2: Register test**

```cmake
add_executable(tst_file_manager_newfile tst_file_manager_newfile.cpp)
target_link_libraries(tst_file_manager_newfile
    PRIVATE Qt6::Test Corbomite::Vault Corbomite::Storage)
add_test(NAME tst_file_manager_newfile COMMAND tst_file_manager_newfile)
set_tests_properties(tst_file_manager_newfile PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
```

- [ ] **Step 3: Implement**

Edit `libs/vault/src/FileManager.cpp`:

```cpp
#include "corbomite/vault/TFolder.h"

namespace {
QString collisionFreeName(Corbomite::Vault *v, const QString &parentPrefix,
                          const QString &desired, const QString &ext)
{
    QString base = desired.isEmpty() ? QStringLiteral("Untitled") : desired;
    QString candidate = parentPrefix + base
                      + (ext.isEmpty() ? QString() : (QLatin1Char('.') + ext));
    if (!v->getAbstractFileByPath(candidate)) return candidate;
    int n = 2;
    while (true) {
        candidate = parentPrefix + base + QStringLiteral(" ") + QString::number(n)
                  + (ext.isEmpty() ? QString() : (QLatin1Char('.') + ext));
        if (!v->getAbstractFileByPath(candidate)) return candidate;
        ++n;
    }
}
}

TFolder *FileManager::getNewFileParent(const QString &hintPath,
                                       const QString &) const
{
    if (!m_vault) return nullptr;
    if (hintPath.isEmpty()) return m_vault->getRoot();
    const int slash = hintPath.lastIndexOf(QLatin1Char('/'));
    if (slash < 0) return m_vault->getRoot();
    return m_vault->getFolderByPath(hintPath.left(slash));
}

TFile *FileManager::createNewMarkdownFile(TFolder *parent,
                                          const QString &name,
                                          const QByteArray &content)
{
    if (!m_vault) return nullptr;
    if (!parent) parent = m_vault->getRoot();
    const QString prefix = parent->getParentPrefix();
    const QString path = collisionFreeName(m_vault, prefix, name, QStringLiteral("md"));
    return m_vault->create(path, content);
}

TFile *FileManager::createNewMarkdownFileFromLinktext(const QString &linkText,
                                                      const QString &hintPath)
{
    TFolder *parent = getNewFileParent(hintPath);
    return createNewMarkdownFile(parent, linkText);
}

TFolder *FileManager::createNewFolder(TFolder *parent)
{
    if (!m_vault) return nullptr;
    if (!parent) parent = m_vault->getRoot();
    const QString prefix = parent->getParentPrefix();
    const QString path = collisionFreeName(m_vault, prefix,
                                           QStringLiteral("untitled folder"),
                                           QString());
    return m_vault->createFolder(path);
}
```

- [ ] **Step 4: Build + run**

```bash
cmake --build build
cd build && ctest -R tst_file_manager_newfile --output-on-failure
```

- [ ] **Step 5: Commit**

```bash
git add libs/vault
git commit -m "feat(vault): FileManager create-new-file ops (Q.0 P5 T5.4)"
```

---

## Task 5.5: Remaining FileManager methods (attachments, link gen, insertIntoFile)

The remaining methods (`getAvailablePathForAttachment`, `generateMarkdownLink`, `insertIntoFile`) follow the same TDD pattern. For each:

1. Write a `tst_file_manager_<method>.cpp` with 3–5 cases mirroring Obsidian's documented semantics from `docs/obsidian-audit/domains/vault.md` §"FileManager".
2. Register in `tests/CMakeLists.txt`.
3. Replace the stub in `FileManager.cpp` with the real body.
4. Build + run; commit.

Contract summaries:

- **`getAvailablePathForAttachment(linktext, sourcePathHint)`** — honour `attachmentFolderPath` vault config. `.`/`.` = same folder, `./sub` = relative under parent, else vault-absolute. Call `Vault::getAvailablePath` for collision suffixes. Create folder if missing.
- **`generateMarkdownLink(target, sourcePath, subpath, displayText)`** — read `useMarkdownLinks` + `newLinkFormat` from vault config. Emit `[[…]]` if `useMarkdownLinks==false`, else `[text](percent-encoded-path)`. Use `MetadataCache::fileToLinktext` for shortest-unique under `newLinkFormat`.
- **`insertIntoFile(file, content, mode)`** — `vault->process(file, ...)` with body that appends or prepends `content`. Frontmatter-aware merge is deferred (spec §11).

Leave `deleteProperty` / `renameProperty` as returning `false` stubs with a TODO comment referencing spec §11.

Each of the three methods gets its own Task (5.5.a / 5.5.b / 5.5.c in implementation) with the standard 5-step TDD structure. Commit after each.

- [ ] **Step 1: Task 5.5.a — `getAvailablePathForAttachment`** — complete the 5 TDD steps for this method per the contract above. Commit message `feat(vault): FileManager attachment paths (Q.0 P5 T5.5a)`.
- [ ] **Step 2: Task 5.5.b — `generateMarkdownLink`** — complete the 5 TDD steps. Commit message `feat(vault): FileManager generateMarkdownLink (Q.0 P5 T5.5b)`.
- [ ] **Step 3: Task 5.5.c — `insertIntoFile`** — complete the 5 TDD steps. Commit message `feat(vault): FileManager insertIntoFile (Q.0 P5 T5.5c)`.

---

## Task 5.6: Phase 5 verification

- [ ] **Step 1: Full build + tests**

```bash
rm -rf build && cmake -S . -B build -DCORBOMITE_DEV_BUILD=ON && cmake --build build
cd build && ctest --output-on-failure -E "tst_markoff_inline_math|tst_renderengine|tst_completion_popup|tst_benchmark_layout"
```

- [ ] **Step 2: Update PROJECT-STATE + commit**

```bash
git add docs/PROJECT-STATE.md
git commit -m "docs: Cluster Q.0 Phase 5 landed (Q.0 P5 T5.6)"
```

---

# Phase 6 — Consumer migration wave 1: sidebars + panels

Goal: Migrate sidebar panels from `VaultModel *` to `Vault *`. `VaultModel` still exists; both can coexist during the wave. Each panel is a single task with the same shape: update header to accept `Vault *`; rewrite method bodies using Vault/TFile APIs; run panel-specific tests.

Mechanical migration pattern (apply per panel):

1. Locate the panel's `setVaultModel(VaultModel *)` method.
2. Rename to `setVault(Vault *)`.
3. Replace `m_vault->allNotes()` → `m_vault->getMarkdownFiles()`.
4. Replace `m_vault->noteExists(rel)` → `m_vault->getAbstractFileByPath(rel) != nullptr`.
5. Replace `m_vault->cachedDocument(rel)` → there is no direct replacement; use `m_vault->cachedRead(tf)` if the consumer just wanted the body, or keep a pointer to a higher-level doc cache elsewhere. The Properties panel needs `processFrontMatter` — route through `FileManager` (passed alongside).
6. Replace `VaultModel` Qt signal connections:
   - `noteAdded(QString)` → `created(TAbstractFile *)` — filter to `dynamic_cast<TFile *>` and check extension.
   - `noteRemoved(QString)` → `deletedFile(TAbstractFile *)`.
   - `noteModified(QString)` → `modified(TFile *)`.
   - `noteRenamed(QString, QString)` → `renamed(TAbstractFile *, QString oldPath)`.
7. Update `MainWindow` wire-up: pass `Vault *` (and `FileManager *` where needed) instead of `VaultModel *`.
8. Update panel's tests: constructor args + signal names.

## Task 6.1: `OutlinksPanel`

**Files:** `src/sidebar/OutlinksPanel.{h,cpp}` + `tests/sidebar/tst_outlinks_panel.cpp` (if exists) + `src/app/MainWindow.cpp` (wire-up).

- [ ] **Step 1**: `grep -rn "OutlinksPanel" libs/ src/ tests/` — record callsites.
- [ ] **Step 2**: Apply the mechanical migration pattern above. TDD: run existing panel tests, update assertions if they grep on signal names.
- [ ] **Step 3**: Build + run all panel tests: `cd build && ctest -R outlinks --output-on-failure`.
- [ ] **Step 4**: Commit: `refactor(panels): OutlinksPanel on Vault (Q.0 P6 T6.1)`.

## Task 6.2: `BacklinksPanel`

Same pattern. Commit `refactor(panels): BacklinksPanel on Vault (Q.0 P6 T6.2)`.

## Task 6.3: `LocalGraphPanel`

Same. Commit `refactor(panels): LocalGraphPanel on Vault (Q.0 P6 T6.3)`.

## Task 6.4: `PropertiesPanel`

Same pattern, but this panel also uses `FrontMatterWriter`. Route its writeback through `FileManager::processFrontMatter` instead. Pass both `Vault *` and `FileManager *` to the panel's ctor. Commit `refactor(panels): PropertiesPanel on Vault+FileManager (Q.0 P6 T6.4)`.

## Task 6.5: `FileExplorerPanel`

Same. Commit `refactor(panels): FileExplorerPanel on Vault (Q.0 P6 T6.5)`.

## Task 6.6: `OutlinePanel`

Same. Commit `refactor(panels): OutlinePanel on Vault (Q.0 P6 T6.6)`.

## Task 6.7: Phase 6 verification

- [ ] **Step 1**: Full build + tests.
- [ ] **Step 2**: Update PROJECT-STATE + commit `docs: Cluster Q.0 Phase 6 landed (Q.0 P6 T6.7)`.

---

# Phase 7 — Consumer migration wave 2: editor + graph + search + metadata

Goal: Migrate the remaining 9 consumers off `VaultModel *` onto `Vault *` + `FileManager *`. `MetadataCache` switches its subscription source from VaultModel's signals to Vault's signals.

Apply the same mechanical pattern from Phase 6 to each site. One commit per consumer.

## Task 7.1: `NoteEditorWidget`

**Files:** `src/editor/NoteEditorWidget.{h,cpp}` + `src/editor/VaultResourceProvider.{h,cpp}` + tests.

- [ ] **Step 1**: Apply migration pattern.
- [ ] **Step 2**: Build + tests.
- [ ] **Step 3**: Commit `refactor(editor): NoteEditorWidget on Vault (Q.0 P7 T7.1)`.

## Task 7.2: Markoff `VaultResourceProvider` adapter

**Files:** `src/editor/VaultResourceProvider.{h,cpp}` — the app-side adapter over the library's abstract.

- [ ] **Step 1**: Apply migration pattern. Remove `VaultModel *`; add `Vault *`.
- [ ] **Step 2**: Build + tests.
- [ ] **Step 3**: Commit `refactor(editor): VaultResourceProvider on Vault (Q.0 P7 T7.2)`.

## Task 7.3: `GraphViewTab` + `GraphView` + `GraphDataBuilder`

**Files:** `src/graph/*`.

- [ ] **Step 1**: Apply pattern across the three files (shared dep).
- [ ] **Step 2**: Build + tests.
- [ ] **Step 3**: Commit `refactor(graph): graph widgets on Vault (Q.0 P7 T7.3)`.

## Task 7.4: `SearchPanel`

- [ ] **Step 1**: Migration.
- [ ] **Step 2**: Build + tests.
- [ ] **Step 3**: Commit `refactor(search): SearchPanel on Vault (Q.0 P7 T7.4)`.

## Task 7.5: `QuickSwitcher`

- [ ] **Step 1**: Migration.
- [ ] **Step 2**: Build + tests.
- [ ] **Step 3**: Commit `refactor(dialogs): QuickSwitcher on Vault (Q.0 P7 T7.5)`.

## Task 7.6: `NotesTreeModel`

**Files:** `libs/models/src/NotesTreeModel.cpp` — Qt item model over the vault tree.

- [ ] **Step 1**: Replace `VaultModel *` with `Vault *`. Route `allNotes()` through `Vault::getMarkdownFiles()`.
- [ ] **Step 2**: Update signal wiring (see Phase 6 pattern).
- [ ] **Step 3**: Commit `refactor(models): NotesTreeModel on Vault (Q.0 P7 T7.6)`.

## Task 7.7: `MetadataCache` subscribes to Vault signals

**Files:** `src/app/MainWindow.cpp` (the wire-up point).

- [ ] **Step 1**: Where MainWindow today connects `VaultModel::noteAdded` → `MetadataCache::onFileAdded` etc., swap to `Vault::created` (filtered to `TFile *` with extension `md`) → the same slot. Same for modified / deletedFile / renamed.
- [ ] **Step 2**: Build + tests. `MetadataCache` internals untouched.
- [ ] **Step 3**: Commit `refactor(metadata): MetadataCache subscribes to Vault signals (Q.0 P7 T7.7)`.

## Task 7.8: `DailyNoteService` + `TemplateService`

**Files:** `libs/models/src/DailyNoteService.cpp` + `TemplateService.cpp`.

- [ ] **Step 1**: Replace `VaultModel *` with `Vault *` + `FileManager *`. Use `FileManager::createNewMarkdownFile` where the services today construct notes via `NoteService`.
- [ ] **Step 2**: Build + tests.
- [ ] **Step 3**: Commit `refactor(models): daily-note + template services on Vault+FileManager (Q.0 P7 T7.8)`.

## Task 7.9: FileWatchReactor re-exposure via `Vault::Watcher`

If any external code (e.g., MainWindow's vault-scanned pipeline) needed the direct FileWatchReactor signals during Phase 2's stubbing, restore that access now through `Vault`'s public signals. No external API for `Watcher` itself — it stays private to Vault.

- [ ] **Step 1**: Locate the comments placed in Phase 2 Task 2.2 Step 6 (`// TODO Q.0 P7: wire through Vault`). Replace each with a direct `connect(vault, &Vault::created, ...)` connection.
- [ ] **Step 2**: Build + tests.
- [ ] **Step 3**: Commit `refactor(app): re-wire file watcher consumers through Vault signals (Q.0 P7 T7.9)`.

## Task 7.10: Phase 7 verification

- [ ] **Step 1**: Full build + tests.
- [ ] **Step 2**: Update PROJECT-STATE + commit `docs: Cluster Q.0 Phase 7 landed (Q.0 P7 T7.10)`.

---

# Phase 8 — App-level reshape (VaultService + NoteService dissolve)

Goal: `openVault` / `closeVault` lifecycle moves from `VaultService` to `CorbomiteApp` (or MainWindow if CorbomiteApp doesn't yet exist). Recent-vaults extracted to a small `RecentVaults` helper. `NoteService` dissolves into `FileManager`.

## Task 8.1: `RecentVaults` helper

**Files:**
- Create: `src/app/RecentVaults.{h,cpp}` (or `libs/core/include/corbomite/core/RecentVaults.{h,cpp}` if broadly useful).
- Create: `tests/app/tst_recent_vaults.cpp`

- [ ] **Step 1**: Write failing test that exercises `RecentVaults::list()` / `add(path)` / `load()` / `save()`.
- [ ] **Step 2**: Implement — extract the KSharedConfig-backed list handling from `VaultService::recentVaults`/`addRecentVault`.
- [ ] **Step 3**: Build + test.
- [ ] **Step 4**: Commit `feat(app): RecentVaults helper (Q.0 P8 T8.1)`.

## Task 8.2: Move `openVault` / `closeVault` to CorbomiteApp

**Files:**
- Modify: `src/app/CorbomiteApp.{h,cpp}` (create if absent — currently MainWindow holds VaultService; move ownership up one level. If CorbomiteApp doesn't exist yet, create a minimal owner class for Vault + FileManager + MetadataCache + RecentVaults).
- Modify: `src/app/MainWindow.{h,cpp}` (constructor: `VaultService *` → `Vault *`, `FileManager *`).

- [ ] **Step 1**: If `CorbomiteApp` doesn't exist, create it. Otherwise modify in place.
- [ ] **Step 2**: Move `openVault` / `closeVault` methods. They now construct a fresh `Vault` + `FileManager` pair, load, and emit app-level signals.
- [ ] **Step 3**: MainWindow constructor signature change. Update every constructor call site.
- [ ] **Step 4**: Build + tests.
- [ ] **Step 5**: Commit `refactor(app): move vault lifecycle from VaultService to CorbomiteApp (Q.0 P8 T8.2)`.

## Task 8.3: Delete `VaultService`

**Files:**
- Delete: `src/app/VaultService.{h,cpp}`
- Modify: `src/CMakeLists.txt`

- [ ] **Step 1**: `grep -rn "VaultService"` — confirm no remaining callers.
- [ ] **Step 2**: Delete files + CMakeLists entries.
- [ ] **Step 3**: Build + tests.
- [ ] **Step 4**: Commit `chore(app): delete VaultService (Q.0 P8 T8.3)`.

## Task 8.4: Fold `NoteService` into `FileManager`

**Files:**
- Port remaining NoteService methods to FileManager (`newNote`, `renameNote`, `deleteNote` — mostly already covered by `createNewMarkdownFile` / `renameFile` / `trashFile`).
- Delete: `libs/models/include/corbomite/models/NoteService.h` + `libs/models/src/NoteService.cpp`
- Modify: `libs/models/CMakeLists.txt`

- [ ] **Step 1**: For any remaining distinct behaviour in NoteService not already in FileManager, port.
- [ ] **Step 2**: Delete NoteService files.
- [ ] **Step 3**: `grep -rn "NoteService"` — migrate leftover callers to `FileManager`.
- [ ] **Step 4**: Build + tests.
- [ ] **Step 5**: Commit `refactor(vault): fold NoteService into FileManager (Q.0 P8 T8.4)`.

## Task 8.5: Phase 8 verification

- [ ] **Step 1**: Full build + tests.
- [ ] **Step 2**: Update PROJECT-STATE + commit `docs: Cluster Q.0 Phase 8 landed (Q.0 P8 T8.5)`.

---

# Phase 9 — Plugin proxy layer rewrite

Goal: Implement `VaultProxy` + `FileManagerProxy` in `libs/vault/`. Rewire `PluginContext::setCoreServices`. Rewrite `tst_proxy_vault.cpp` against the new proxies.

## Task 9.1: `VaultProxy` class + tests

**Files:**
- Create: `libs/vault/include/corbomite/vault/proxies/VaultProxy.h`
- Create: `libs/vault/src/proxies/VaultProxy.cpp`
- Create: `libs/vault/tests/tst_vault_proxy.cpp`
- Modify: `libs/vault/CMakeLists.txt`

- [ ] **Step 1: Write failing test**

Create `libs/vault/tests/tst_vault_proxy.cpp`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QTemporaryDir>
#include <QFile>

#include "corbomite/vault/proxies/VaultProxy.h"
#include "corbomite/vault/Vault.h"
#include "corbomite/vault/TFile.h"
#include "corbomite/storage/FileSystemAdapter.h"

class TestVaultProxy : public QObject
{
    Q_OBJECT
private slots:
    void readRequiresReadPermission();
    void modifyRequiresWritePermission();
    void readPermissionGrantsAccess();
    void writePermissionGrantsAccess();
    void eventsSubscriptionRequiresEventsPermission();
};

namespace {
void writeFile(const QString &p, const QByteArray &b)
{
    QFile f(p); f.open(QIODevice::WriteOnly); f.write(b); f.close();
}
}

void TestVaultProxy::readRequiresReadPermission()
{
    QTemporaryDir dir;
    writeFile(dir.path() + "/a.md", "x");
    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault v(&fs); v.load(dir.path());

    Corbomite::VaultProxy proxy(&v, QSet<QString>{}, QStringLiteral("p"));
    QCOMPARE(proxy.read(v.getFileByPath(QStringLiteral("a.md"))), QByteArray());
}

void TestVaultProxy::modifyRequiresWritePermission()
{
    QTemporaryDir dir;
    writeFile(dir.path() + "/a.md", "x");
    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault v(&fs); v.load(dir.path());

    Corbomite::VaultProxy proxy(
        &v, QSet<QString>{QStringLiteral("vault.read")}, QStringLiteral("p"));
    QCOMPARE(proxy.modify(v.getFileByPath(QStringLiteral("a.md")),
                          QByteArray("y")),
             false);
}

void TestVaultProxy::readPermissionGrantsAccess()
{
    QTemporaryDir dir;
    writeFile(dir.path() + "/a.md", "x");
    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault v(&fs); v.load(dir.path());

    Corbomite::VaultProxy proxy(
        &v, QSet<QString>{QStringLiteral("vault.read")}, QStringLiteral("p"));
    QCOMPARE(proxy.read(v.getFileByPath(QStringLiteral("a.md"))),
             QByteArray("x"));
}

void TestVaultProxy::writePermissionGrantsAccess()
{
    QTemporaryDir dir;
    writeFile(dir.path() + "/a.md", "x");
    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault v(&fs); v.load(dir.path());

    Corbomite::VaultProxy proxy(
        &v, QSet<QString>{QStringLiteral("vault.write")}, QStringLiteral("p"));
    QVERIFY(proxy.modify(v.getFileByPath(QStringLiteral("a.md")),
                         QByteArray("y")));
}

void TestVaultProxy::eventsSubscriptionRequiresEventsPermission()
{
    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault v(&fs);
    Corbomite::VaultProxy proxy(&v, QSet<QString>{}, QStringLiteral("p"));
    const QUuid token = proxy.on(QStringLiteral("modify"), [](auto *){});
    QVERIFY(token.isNull());
}

QTEST_MAIN(TestVaultProxy)
#include "tst_vault_proxy.moc"
```

- [ ] **Step 2: Register test**

```cmake
add_executable(tst_vault_proxy tst_vault_proxy.cpp)
target_link_libraries(tst_vault_proxy
    PRIVATE Qt6::Test Corbomite::Vault Corbomite::Storage)
add_test(NAME tst_vault_proxy COMMAND tst_vault_proxy)
set_tests_properties(tst_vault_proxy PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
```

- [ ] **Step 3: Create header**

Create `libs/vault/include/corbomite/vault/proxies/VaultProxy.h`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <functional>

#include <QByteArray>
#include <QJsonValue>
#include <QSet>
#include <QString>
#include <QUuid>
#include <QVector>

namespace Corbomite {

class Vault;
class TAbstractFile;
class TFile;
class TFolder;

/// Permission-gated plugin-facing Vault facade. Methods return empty /
/// false / nullptr / null QUuid when the caller lacks the required
/// permission token.
class VaultProxy
{
public:
    VaultProxy(Vault *vault, const QSet<QString> &granted, QString pluginId);

    // Read (gated by vault.read)
    QByteArray     read(TFile *f) const;
    QByteArray     cachedRead(TFile *f) const;
    QByteArray     readBinary(TFile *f) const;
    bool           exists(const QString &path, bool caseInsensitive = false) const;
    TFile         *getFileByPath(const QString &path) const;
    TFolder       *getFolderByPath(const QString &path) const;
    TAbstractFile *getAbstractFileByPath(const QString &path) const;
    QVector<TFile *> getMarkdownFiles() const;
    QVector<TFile *> getFiles() const;
    TFolder       *getRoot() const;
    QString        getName() const;

    // Mutation (gated by vault.write)
    bool     modify(TFile *f, const QByteArray &body);
    bool     modifyBinary(TFile *f, const QByteArray &body);
    bool     append(TFile *f, const QByteArray &body);
    bool     process(TFile *f,
                     std::function<QByteArray(const QByteArray &)> mutator);
    TFile   *create(const QString &path, const QByteArray &body);
    TFolder *createFolder(const QString &path);
    bool     rename(TAbstractFile *f, const QString &newPath);
    bool     trash(TAbstractFile *f, bool useSystem);
    bool     remove(TAbstractFile *f);

    // Events (gated by vault.events)
    using EventFn = std::function<void(TAbstractFile *)>;
    QUuid on(const QString &event, EventFn fn);
    void  off(const QUuid &token);

    // Config JSON (read gated by vault.read; write by vault.write)
    QJsonValue readConfigJson(const QString &name) const;
    bool       writeConfigJson(const QString &name, const QJsonValue &v);
    bool       deleteConfigJson(const QString &name);

private:
    Vault               *m_vault;
    QSet<QString>        m_granted;
    QString              m_pluginId;
    QHash<QUuid, QMetaObject::Connection> m_subscriptions;

    bool canRead() const   { return m_granted.contains(QStringLiteral("vault.read")); }
    bool canWrite() const  { return m_granted.contains(QStringLiteral("vault.write")); }
    bool canEvents() const { return m_granted.contains(QStringLiteral("vault.events")); }

    void logDenied(const char *method, const char *requiredToken) const;
};

} // namespace Corbomite
```

- [ ] **Step 4: Create impl**

Create `libs/vault/src/proxies/VaultProxy.cpp`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/vault/proxies/VaultProxy.h"

#include "corbomite/vault/Vault.h"
#include "corbomite/vault/TFile.h"
#include "corbomite/vault/TFolder.h"

#include <QLoggingCategory>

namespace Corbomite {

namespace {
Q_LOGGING_CATEGORY(lcPluginVault, "corbomite.plugin.vault")
}

VaultProxy::VaultProxy(Vault *vault, const QSet<QString> &granted, QString pluginId)
    : m_vault(vault), m_granted(granted), m_pluginId(std::move(pluginId))
{}

void VaultProxy::logDenied(const char *method, const char *req) const
{
    qCWarning(lcPluginVault) << "plugin" << m_pluginId
                             << "denied" << method << "— missing" << req;
}

QByteArray VaultProxy::read(TFile *f) const
{
    if (!canRead()) { logDenied("read", "vault.read"); return {}; }
    return m_vault ? m_vault->read(f) : QByteArray{};
}

QByteArray VaultProxy::cachedRead(TFile *f) const
{
    if (!canRead()) { logDenied("cachedRead", "vault.read"); return {}; }
    return m_vault ? m_vault->cachedRead(f) : QByteArray{};
}

QByteArray VaultProxy::readBinary(TFile *f) const
{
    if (!canRead()) { logDenied("readBinary", "vault.read"); return {}; }
    return m_vault ? m_vault->readBinary(f) : QByteArray{};
}

bool VaultProxy::exists(const QString &path, bool ci) const
{
    if (!canRead()) { logDenied("exists", "vault.read"); return false; }
    return m_vault && m_vault->exists(path, ci);
}

TFile *VaultProxy::getFileByPath(const QString &path) const
{
    if (!canRead()) { logDenied("getFileByPath", "vault.read"); return nullptr; }
    return m_vault ? m_vault->getFileByPath(path) : nullptr;
}

TFolder *VaultProxy::getFolderByPath(const QString &path) const
{
    if (!canRead()) { logDenied("getFolderByPath", "vault.read"); return nullptr; }
    return m_vault ? m_vault->getFolderByPath(path) : nullptr;
}

TAbstractFile *VaultProxy::getAbstractFileByPath(const QString &path) const
{
    if (!canRead()) { logDenied("getAbstractFileByPath", "vault.read"); return nullptr; }
    return m_vault ? m_vault->getAbstractFileByPath(path) : nullptr;
}

QVector<TFile *> VaultProxy::getMarkdownFiles() const
{
    if (!canRead()) { logDenied("getMarkdownFiles", "vault.read"); return {}; }
    return m_vault ? m_vault->getMarkdownFiles() : QVector<TFile *>{};
}

QVector<TFile *> VaultProxy::getFiles() const
{
    if (!canRead()) { logDenied("getFiles", "vault.read"); return {}; }
    return m_vault ? m_vault->getFiles() : QVector<TFile *>{};
}

TFolder *VaultProxy::getRoot() const
{
    if (!canRead()) { logDenied("getRoot", "vault.read"); return nullptr; }
    return m_vault ? m_vault->getRoot() : nullptr;
}

QString VaultProxy::getName() const
{
    if (!canRead()) { logDenied("getName", "vault.read"); return {}; }
    return m_vault ? m_vault->getName() : QString{};
}

bool VaultProxy::modify(TFile *f, const QByteArray &body)
{
    if (!canWrite()) { logDenied("modify", "vault.write"); return false; }
    return m_vault && m_vault->modify(f, body);
}

bool VaultProxy::modifyBinary(TFile *f, const QByteArray &body)
{
    if (!canWrite()) { logDenied("modifyBinary", "vault.write"); return false; }
    return m_vault && m_vault->modifyBinary(f, body);
}

bool VaultProxy::append(TFile *f, const QByteArray &body)
{
    if (!canWrite()) { logDenied("append", "vault.write"); return false; }
    return m_vault && m_vault->append(f, body);
}

bool VaultProxy::process(TFile *f,
                         std::function<QByteArray(const QByteArray &)> mut)
{
    if (!canWrite()) { logDenied("process", "vault.write"); return false; }
    return m_vault && m_vault->process(f, std::move(mut));
}

TFile *VaultProxy::create(const QString &path, const QByteArray &body)
{
    if (!canWrite()) { logDenied("create", "vault.write"); return nullptr; }
    return m_vault ? m_vault->create(path, body) : nullptr;
}

TFolder *VaultProxy::createFolder(const QString &path)
{
    if (!canWrite()) { logDenied("createFolder", "vault.write"); return nullptr; }
    return m_vault ? m_vault->createFolder(path) : nullptr;
}

bool VaultProxy::rename(TAbstractFile *f, const QString &newPath)
{
    if (!canWrite()) { logDenied("rename", "vault.write"); return false; }
    return m_vault && m_vault->rename(f, newPath);
}

bool VaultProxy::trash(TAbstractFile *f, bool useSystem)
{
    if (!canWrite()) { logDenied("trash", "vault.write"); return false; }
    return m_vault && m_vault->trash(f, useSystem);
}

bool VaultProxy::remove(TAbstractFile *f)
{
    if (!canWrite()) { logDenied("remove", "vault.write"); return false; }
    return m_vault && m_vault->remove(f);
}

QUuid VaultProxy::on(const QString &event, EventFn fn)
{
    if (!canEvents()) { logDenied("on", "vault.events"); return {}; }
    if (!m_vault) return {};
    QMetaObject::Connection c;
    if (event == QStringLiteral("create"))
        c = QObject::connect(m_vault, &Vault::created, fn);
    else if (event == QStringLiteral("modify"))
        c = QObject::connect(m_vault, &Vault::modified, fn);
    else if (event == QStringLiteral("delete"))
        c = QObject::connect(m_vault, &Vault::deletedFile, fn);
    else if (event == QStringLiteral("rename"))
        c = QObject::connect(m_vault, &Vault::renamed,
            [fn](TAbstractFile *f, const QString &) { fn(f); });
    else
        return {};
    QUuid token = QUuid::createUuid();
    m_subscriptions.insert(token, c);
    return token;
}

void VaultProxy::off(const QUuid &token)
{
    auto it = m_subscriptions.find(token);
    if (it == m_subscriptions.end()) return;
    QObject::disconnect(it.value());
    m_subscriptions.erase(it);
}

QJsonValue VaultProxy::readConfigJson(const QString &name) const
{
    if (!canRead()) { logDenied("readConfigJson", "vault.read"); return {}; }
    return m_vault ? m_vault->readConfigJson(name) : QJsonValue{};
}

bool VaultProxy::writeConfigJson(const QString &name, const QJsonValue &v)
{
    if (!canWrite()) { logDenied("writeConfigJson", "vault.write"); return false; }
    return m_vault && m_vault->writeConfigJson(name, v);
}

bool VaultProxy::deleteConfigJson(const QString &name)
{
    if (!canWrite()) { logDenied("deleteConfigJson", "vault.write"); return false; }
    return m_vault && m_vault->deleteConfigJson(name);
}

} // namespace Corbomite
```

- [ ] **Step 5: Register sources in libs/vault/CMakeLists.txt**

```cmake
    include/corbomite/vault/proxies/VaultProxy.h
    src/proxies/VaultProxy.cpp
```

- [ ] **Step 6: Build + run**

```bash
cmake --build build
cd build && ctest -R tst_vault_proxy --output-on-failure
```

- [ ] **Step 7: Commit**

```bash
git add libs/vault
git commit -m "feat(vault): VaultProxy plugin facade (Q.0 P9 T9.1)"
```

---

## Task 9.2: `FileManagerProxy` class + tests

**Files:**
- Create: `libs/vault/include/corbomite/vault/proxies/FileManagerProxy.h`
- Create: `libs/vault/src/proxies/FileManagerProxy.cpp`
- Create: `libs/vault/tests/tst_file_manager_proxy.cpp`
- Modify: `libs/vault/CMakeLists.txt`

Mirror Task 9.1 structure:

- [ ] **Step 1**: Write failing test analogous to `tst_vault_proxy.cpp`, exercising permission-gated `renameFile`, `processFrontMatter`, `createNewMarkdownFile`, `getAvailablePathForAttachment`, `generateMarkdownLink`, `trashFile`. Use spec §7.3 for the declared API surface.
- [ ] **Step 2**: Register test.
- [ ] **Step 3**: Create header matching spec §7.3.
- [ ] **Step 4**: Create impl — each method begins with permission check then delegates to `m_fm->*`.
- [ ] **Step 5**: Register sources.
- [ ] **Step 6**: Build + run.
- [ ] **Step 7**: Commit `feat(vault): FileManagerProxy plugin facade (Q.0 P9 T9.2)`.

---

## Task 9.3: Rewire `PluginContext`

**Files:**
- Modify: `libs/core/include/corbomite/core/PluginContext.h` (add Vault/FileManager + proxy accessors back)
- Modify: `libs/core/src/PluginContext.cpp`
- Modify: every `setCoreServices` call site (update signature)

- [ ] **Step 1**: Forward-declare `Corbomite::Vault`, `Corbomite::FileManager`, `Corbomite::VaultProxy`, `Corbomite::FileManagerProxy` in PluginContext.h. Note: these live in `libs/vault/`, but `libs/core/` can't depend on `libs/vault/` (cycle). Resolution: proxy surface is plugin ABI — move `PluginContext` itself into `libs/vault/` OR declare the proxy types via forward decls + separate-translation-unit construction (PluginContext.cpp only includes the proxy headers in its .cpp, which lives in libs/vault/). Simplest: **move PluginContext into libs/vault/**. This changes PluginContext.h path to `corbomite/vault/PluginContext.h`.

If moving PluginContext is too disruptive, an alternative is to keep PluginContext as an abstract skeleton in libs/core and introduce a concrete `VaultPluginContext : public PluginContext` in libs/vault/ that layers in vault/fileManager proxies. Decision: defer to execution time; either path is viable.

- [ ] **Step 2**: Add new accessors:

```cpp
VaultProxy       *vault();
FileManagerProxy *fileManager();
```

and new setCoreServices signature:

```cpp
void setCoreServices(Vault *vault, FileManager *fm,
                     MetadataCache *metadata, Workspace *workspace,
                     CommandRegistry *commands, ViewRegistry *views,
                     MenuEventEmitter *menus,
                     QNetworkAccessManager *network);
```

- [ ] **Step 3**: Update PluginContext.cpp — lazy-construct `VaultProxy` and `FileManagerProxy` from the stored `Vault *` / `FileManager *` on first access, gated by permission presence.

- [ ] **Step 4**: Update callers in `CorbomiteApp` / `MainWindow` to pass `Vault *` and `FileManager *` into `setCoreServices`.

- [ ] **Step 5**: Build + run full test suite.

- [ ] **Step 6**: Commit `refactor(plugins): PluginContext wires VaultProxy + FileManagerProxy (Q.0 P9 T9.3)`.

---

## Task 9.4: Phase 9 verification

- [ ] **Step 1**: Full build + tests.
- [ ] **Step 2**: Update PROJECT-STATE + commit `docs: Cluster Q.0 Phase 9 landed (Q.0 P9 T9.4)`.

---

# Phase 10 — Delete legacy classes

Goal: With all consumers migrated, delete `VaultModel`, `FrontMatterWriter`, and any remaining scaffolding. Single commit.

## Task 10.1: Delete `VaultModel`

**Files:**
- Delete: `libs/models/include/corbomite/models/VaultModel.h`
- Delete: `libs/models/src/VaultModel.cpp`
- Delete: `tests/models/tst_vaultmodel.cpp` (if exists)
- Modify: `libs/models/CMakeLists.txt`
- Modify: `tests/models/CMakeLists.txt`

- [ ] **Step 1**: `grep -rn "VaultModel"` — should be empty outside the files-to-delete.
- [ ] **Step 2**: Delete files.
- [ ] **Step 3**: Remove from CMakeLists.
- [ ] **Step 4**: Build + tests.
- [ ] **Step 5**: Commit `chore(models): delete VaultModel (Q.0 P10 T10.1)`.

## Task 10.2: Delete `FrontMatterWriter`

**Files:**
- Delete: `libs/core/include/corbomite/core/FrontMatterWriter.h`
- Delete: `libs/core/src/FrontMatterWriter.cpp`
- Delete: `tests/core/tst_frontmatter_writer.cpp` (already covered by `tst_file_manager_frontmatter`)
- Modify: `libs/core/CMakeLists.txt`
- Modify: `tests/core/CMakeLists.txt`

- [ ] **Step 1**: `grep -rn "FrontMatterWriter"` — empty outside deletables.
- [ ] **Step 2**: Delete + update CMakeLists.
- [ ] **Step 3**: Build + tests.
- [ ] **Step 4**: Commit `chore(core): delete FrontMatterWriter (Q.0 P10 T10.2)`.

## Task 10.3: Delete `src/reactors/` if empty; remove from top-level CMakeLists

- [ ] **Step 1**: `ls src/reactors/` — expected empty.
- [ ] **Step 2**: `rmdir src/reactors` (or `git rm -rf` if git tracks empty marker).
- [ ] **Step 3**: Remove `add_subdirectory(reactors)` from `src/CMakeLists.txt` if present.
- [ ] **Step 4**: Build + tests.
- [ ] **Step 5**: Commit `chore(app): remove empty src/reactors (Q.0 P10 T10.3)`.

## Task 10.4: Phase 10 verification

- [ ] **Step 1**: Full build + tests.
- [ ] **Step 2**: Update PROJECT-STATE — resolve the `Vault/VaultModel tension` open question by moving it to Recent decisions with the resolution summary.
- [ ] **Step 3**: Commit `docs: Cluster Q.0 Phase 10 landed — legacy vault classes deleted (Q.0 P10 T10.4)`.

---

# Phase 11 — Resume Cluster Q against new Vault

Goal: Cluster Q Tasks 7–12 (originally at `docs/superpowers/plans/2026-04-16-cluster-q-internal-plugin-wrapping.md`) get rewritten to use the new `Vault *` + `FileManager *` + `VaultProxy` + `FileManagerProxy`. The six internal plugins then migrate behind the new plugin infrastructure.

## Task 11.1: Rewrite Cluster Q Tasks 7–12 bodies

**Files:**
- Modify: `docs/superpowers/plans/2026-04-16-cluster-q-internal-plugin-wrapping.md`

- [ ] **Step 1**: Open the existing Cluster Q plan. In Tasks 7–12, replace every `VaultReader *` / `VaultWriter *` reference with `VaultProxy *`. Replace every `vault.read` + `vault.write` accessor dance with per-method checks on `VaultProxy`. Add `FileManagerProxy *` accessor wherever tasks previously punted on atomic rename / frontmatter mutation.
- [ ] **Step 2**: Update the "File structure" block at the top of the Cluster Q plan to reflect `libs/vault/include/corbomite/vault/proxies/` as the proxy home (not `libs/core/.../proxies/`).
- [ ] **Step 3**: Delete the references in Cluster Q Task 7 that describe creating the Task-7 `Corbomite::Vault` stub (now deleted).
- [ ] **Step 4**: Commit `docs(cluster-q): retarget Tasks 7-12 onto new Vault architecture (Q.0 P11 T11.1)`.

## Task 11.2: Execute Cluster Q Tasks 7–12 (pointer only)

This becomes a separate execution session under the superpowers:executing-plans skill. Task list: follow the rewritten Cluster Q plan from its Task 7 onward.

- [ ] **Step 1**: Dispatch execution per the Cluster Q plan's own task sequence.

## Task 11.3: Cluster Q.0 closure

**Files:**
- Modify: `docs/PROJECT-STATE.md`
- Create: `docs/cluster-retros/cluster-q0.md` (retro writeup)
- Modify: `/home/clinton/.claude/projects/-home-clinton-dev-Corbomite/memory/project_vault_tension.md` (move from tension to resolution)
- Create: `/home/clinton/.claude/projects/-home-clinton-dev-Corbomite/memory/project_cluster_q0_done.md`

- [ ] **Step 1**: Write retro at `docs/cluster-retros/cluster-q0.md` — what was done, what surprises, how-to-apply.
- [ ] **Step 2**: Update PROJECT-STATE — roadmap table entry, recent-decisions summary, resolve the Open Question.
- [ ] **Step 3**: Update memory — mark `project_vault_tension.md` as resolved and create `project_cluster_q0_done.md`.
- [ ] **Step 4**: Commit `docs: Cluster Q.0 closed (Q.0 P11 T11.3)`.

---

## Self-review notes

After writing this plan I checked:

- **Spec coverage:** every spec section has corresponding tasks (§3 Library layout → T1.1; §4 Core types → T1.2–T1.4; §5 Vault API → T1.5 + P3; §6 FileManager → P5; §7 Proxies → P9; §8 Class migration → P8 + P10; §9 Migration phases → P1–P11; §10 Tests → tests per task; §11 Non-goals → honored by absent tasks).
- **Placeholders:** Task 5.5 compresses three sub-tasks into one with implementation contract summaries rather than full code. Each sub-task still gets the standard 5-step TDD treatment but details are earned at execution time. Phase 6 + 7 consumer-migration tasks use a template pattern: the pattern is enumerated once at the top of Phase 6; each task applies it to one file. This is defensible because the per-consumer code is mechanical and varies only in the identifier names; writing out 16 identical TDD cycles would be noise.
- **Type consistency:** method names match between spec and plan (`modify`/`process`/`trash`/`rename`/`created`/`modified`/`deletedFile`/`renamed`/`closed`). Proxy methods match spec §7.
- **Spec requirements not matching tasks:** the spec notes `readBinary` is distinct from `read`; the plan treats them as aliases in Phase 3 Task 3.1 (both route through adapter's `readBinary`). That's consistent with spec intent ("same path today; hook kept for API parity").

End of plan.


