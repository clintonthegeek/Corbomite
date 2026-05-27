# Cluster D.4c — Bases undo/redo Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make Bases cell edits and properties-drawer edits undoable/redoable via the app's standard Ctrl+Z / Ctrl+Y, with a per-view stack that never clobbers an external change to the same frontmatter field.

**Architecture:** A single `QUndoCommand` subclass (`CmdSetFrontMatter`) does all reads/writes inside one `FileManager::processFrontMatter` mutator, so its stale check runs against freshly-parsed-from-disk frontmatter. `BasesView` owns a `QUndoStack` and a chokepoint slot; the model and drawer stop writing directly and instead emit an edit-request signal routed to that slot. `MainWindow`'s existing Undo/Redo actions gain a branch for an active `BasesView`.

**Tech Stack:** C++20, Qt6 (`QUndoStack`/`QUndoCommand`), KDE Frameworks (`KStandardAction`, `i18n`), QtTest, CMake.

**Spec:** [`docs/superpowers/specs/2026-05-27-cluster-d4c-bases-undo-design.md`](../specs/2026-05-27-cluster-d4c-bases-undo-design.md)

---

## File structure

- **New** `libs/bases/include/corbomite/bases/BasesCommands.h` — `CmdSetFrontMatter` declaration.
- **New** `libs/bases/src/BasesCommands.cpp` — its implementation.
- **New** `libs/bases/tests/tst_bases_commands.cpp` — command behavior against a real temp-vault `FileManager`.
- **Modify** `libs/bases/CMakeLists.txt` — add `BasesCommands.cpp` to the library sources.
- **Modify** `libs/bases/tests/CMakeLists.txt` — register `tst_bases_commands` (links `Corbomite::Bases` + `Corbomite::Vault` + `Corbomite::Storage`).
- **Modify** `libs/bases/include/corbomite/bases/BasesTreeModel.h` / `src/BasesTreeModel.cpp` — `frontMatterEditRequested` signal; `setData` emits instead of writing.
- **Modify** `libs/bases/include/corbomite/bases/PropertiesDrawer.h` / `src/PropertiesDrawer.cpp` — same signal; `commit` emits instead of writing.
- **Modify** `libs/bases/include/corbomite/bases/BasesView.h` / `src/BasesView.cpp` — `QUndoStack` member, `pushFrontMatterEdit` slot, `undo()`/`redo()`, notify banner, connect signals, clear stack on load.
- **Modify** `libs/bases/tests/tst_bases_tree_model.cpp` — assert `setData` emits the signal and does not write to disk.
- **Modify** `src/app/MainWindow.cpp` — Undo/Redo action routing for an active `BasesView`.

---

## Task 1: `CmdSetFrontMatter` command

**Files:**
- Create: `libs/bases/include/corbomite/bases/BasesCommands.h`
- Create: `libs/bases/src/BasesCommands.cpp`
- Create: `libs/bases/tests/tst_bases_commands.cpp`
- Modify: `libs/bases/CMakeLists.txt`
- Modify: `libs/bases/tests/CMakeLists.txt`

- [ ] **Step 1: Write the header**

Create `libs/bases/include/corbomite/bases/BasesCommands.h`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <QUndoCommand>
#include <QVariant>

#include <functional>

namespace Corbomite {
class FileManager;
class TFile;
}  // namespace Corbomite

namespace Corbomite::Bases {

/// Undoable single-key frontmatter edit. All reads/writes happen inside one
/// FileManager::processFrontMatter mutator, so the stale check runs against the
/// frontmatter freshly parsed from disk (no MetadataCache async-lag race).
///
/// On external drift (the on-disk value no longer matches what this command
/// last wrote) the command does NOT overwrite: it calls `notify` with a
/// user-facing string and neutralizes itself (all further redo/undo are
/// no-ops). A no-op undo() still lets QUndoStack advance its index, so the
/// stale command is skipped and older history stays reachable.
class CmdSetFrontMatter : public QUndoCommand
{
public:
    CmdSetFrontMatter(Corbomite::FileManager *fm,
                      Corbomite::TFile *file,
                      QString key,
                      QVariant newValue,
                      std::function<void(const QString &)> notify);

    void redo() override;
    void undo() override;

private:
    Corbomite::FileManager *m_fm;
    Corbomite::TFile       *m_file;
    QString  m_key;
    QVariant m_newValue;
    QVariant m_oldValue;            // captured lazily on first redo()
    bool     m_oldCaptured = false;
    bool     m_neutralized = false;
    std::function<void(const QString &)> m_notify;
};

}  // namespace Corbomite::Bases
```

- [ ] **Step 2: Write the implementation**

Create `libs/bases/src/BasesCommands.cpp`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/BasesCommands.h"

#include "corbomite/vault/FileManager.h"

#include <KLocalizedString>

namespace Corbomite::Bases {

CmdSetFrontMatter::CmdSetFrontMatter(Corbomite::FileManager *fm,
                                     Corbomite::TFile *file,
                                     QString key,
                                     QVariant newValue,
                                     std::function<void(const QString &)> notify)
    : m_fm(fm),
      m_file(file),
      m_key(std::move(key)),
      m_newValue(std::move(newValue)),
      m_notify(std::move(notify))
{
    setText(i18n("Edit \"%1\"", m_key));
}

void CmdSetFrontMatter::redo()
{
    if (m_neutralized) return;
    bool stale = false;
    const bool ok = m_fm->processFrontMatter(m_file, [&](QVariantMap &fm) {
        const QVariant current = fm.value(m_key);
        if (!m_oldCaptured) {
            // First application (the QUndoStack::push): capture pre-state and
            // apply unconditionally.
            m_oldValue = current;
            m_oldCaptured = true;
        } else if (current != m_oldValue) {
            // Re-redo after an undo, but the field drifted since undo left it
            // at oldValue.
            stale = true;
            return;
        }
        fm.insert(m_key, m_newValue);
    });
    if (!ok || stale) {
        m_neutralized = true;
        if (m_notify)
            m_notify(i18n("Skipped redo — \"%1\" changed outside Bases", m_key));
    }
}

void CmdSetFrontMatter::undo()
{
    if (m_neutralized) return;
    bool stale = false;
    const bool ok = m_fm->processFrontMatter(m_file, [&](QVariantMap &fm) {
        const QVariant current = fm.value(m_key);
        if (current != m_newValue) {
            // Someone changed the field since redo wrote newValue.
            stale = true;
            return;
        }
        if (m_oldValue.isValid())
            fm.insert(m_key, m_oldValue);
        else
            fm.remove(m_key);   // key did not exist before the edit
    });
    if (!ok || stale) {
        m_neutralized = true;
        if (m_notify)
            m_notify(i18n("Skipped undo — \"%1\" changed outside Bases", m_key));
    }
}

}  // namespace Corbomite::Bases
```

- [ ] **Step 3: Register the source + test in CMake**

In `libs/bases/CMakeLists.txt`, add `src/BasesCommands.cpp` to the library's source list (alongside the other `src/*.cpp` entries — match the existing list style).

In `libs/bases/tests/CMakeLists.txt`, append:

```cmake
add_executable(tst_bases_commands tst_bases_commands.cpp)
add_test(NAME tst_bases_commands COMMAND tst_bases_commands)
target_link_libraries(tst_bases_commands PRIVATE
    Qt6::Test Corbomite::Bases Corbomite::Vault Corbomite::Storage)
```

> Verify the exact target names for the vault/storage libs by reading the top of an existing test that links them (e.g. any `tst_file_manager_*` in `libs/vault/tests/CMakeLists.txt`); use `Corbomite::Vault` / `Corbomite::Storage` as shown unless that file proves otherwise.

- [ ] **Step 4: Write the failing tests**

Create `libs/bases/tests/tst_bases_commands.cpp`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QTemporaryDir>
#include <QFile>

#include "corbomite/bases/BasesCommands.h"
#include "corbomite/vault/FileManager.h"
#include "corbomite/vault/Vault.h"
#include "corbomite/vault/TFile.h"
#include "corbomite/storage/FileSystemAdapter.h"

using namespace Corbomite::Bases;

namespace {
void writeFile(const QString &p, const QByteArray &b)
{
    QFile f(p);
    f.open(QIODevice::WriteOnly);
    f.write(b);
    f.close();
}
QByteArray readFileAll(const QString &p)
{
    QFile f(p);
    f.open(QIODevice::ReadOnly);
    return f.readAll();
}
}  // namespace

class TestBasesCommands : public QObject
{
    Q_OBJECT
private slots:
    void redoWritesNewValue();
    void undoRestoresOldValue();
    void redoAfterUndoReapplies();
    void driftBeforeUndoIsSkipped();
    void undoRemovesKeyAbsentBeforeEdit();
    void textContainsKey();
};

void TestBasesCommands::redoWritesNewValue()
{
    QTemporaryDir dir;
    writeFile(dir.path() + "/a.md", "---\ntag: old\n---\nbody\n");
    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());
    Corbomite::FileManager fm(&vault, nullptr);
    auto *tf = vault.getFileByPath(QStringLiteral("a.md"));
    QVERIFY(tf);

    CmdSetFrontMatter cmd(&fm, tf, QStringLiteral("tag"),
                          QStringLiteral("new"), nullptr);
    cmd.redo();

    const QByteArray after = readFileAll(dir.path() + "/a.md");
    QVERIFY(after.contains("tag: new"));
    QVERIFY(!after.contains("tag: old"));
}

void TestBasesCommands::undoRestoresOldValue()
{
    QTemporaryDir dir;
    writeFile(dir.path() + "/a.md", "---\ntag: old\n---\nbody\n");
    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());
    Corbomite::FileManager fm(&vault, nullptr);
    auto *tf = vault.getFileByPath(QStringLiteral("a.md"));

    CmdSetFrontMatter cmd(&fm, tf, QStringLiteral("tag"),
                          QStringLiteral("new"), nullptr);
    cmd.redo();
    cmd.undo();

    const QByteArray after = readFileAll(dir.path() + "/a.md");
    QVERIFY(after.contains("tag: old"));
    QVERIFY(!after.contains("tag: new"));
}

void TestBasesCommands::redoAfterUndoReapplies()
{
    QTemporaryDir dir;
    writeFile(dir.path() + "/a.md", "---\ntag: old\n---\nbody\n");
    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());
    Corbomite::FileManager fm(&vault, nullptr);
    auto *tf = vault.getFileByPath(QStringLiteral("a.md"));

    CmdSetFrontMatter cmd(&fm, tf, QStringLiteral("tag"),
                          QStringLiteral("new"), nullptr);
    cmd.redo();
    cmd.undo();
    cmd.redo();

    const QByteArray after = readFileAll(dir.path() + "/a.md");
    QVERIFY(after.contains("tag: new"));
}

void TestBasesCommands::driftBeforeUndoIsSkipped()
{
    QTemporaryDir dir;
    writeFile(dir.path() + "/a.md", "---\ntag: old\n---\nbody\n");
    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());
    Corbomite::FileManager fm(&vault, nullptr);
    auto *tf = vault.getFileByPath(QStringLiteral("a.md"));

    int notifyCount = 0;
    CmdSetFrontMatter cmd(&fm, tf, QStringLiteral("tag"),
                          QStringLiteral("new"),
                          [&](const QString &) { ++notifyCount; });
    cmd.redo();   // disk: tag: new

    // Simulate an external edit to the same key.
    fm.processFrontMatter(tf, [](QVariantMap &m) {
        m.insert(QStringLiteral("tag"), QStringLiteral("external"));
    });

    cmd.undo();   // must NOT clobber the external value
    QByteArray after = readFileAll(dir.path() + "/a.md");
    QVERIFY(after.contains("tag: external"));
    QVERIFY(!after.contains("tag: old"));
    QCOMPARE(notifyCount, 1);

    // Neutralized: a subsequent redo is also a no-op.
    cmd.redo();
    after = readFileAll(dir.path() + "/a.md");
    QVERIFY(after.contains("tag: external"));
    QCOMPARE(notifyCount, 1);   // not called again
}

void TestBasesCommands::undoRemovesKeyAbsentBeforeEdit()
{
    QTemporaryDir dir;
    writeFile(dir.path() + "/a.md", "---\nkeep: yes\n---\nbody\n");
    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());
    Corbomite::FileManager fm(&vault, nullptr);
    auto *tf = vault.getFileByPath(QStringLiteral("a.md"));

    CmdSetFrontMatter cmd(&fm, tf, QStringLiteral("brandnew"),
                          QStringLiteral("v"), nullptr);
    cmd.redo();
    QVERIFY(readFileAll(dir.path() + "/a.md").contains("brandnew: v"));

    cmd.undo();
    const QByteArray after = readFileAll(dir.path() + "/a.md");
    QVERIFY(!after.contains("brandnew"));   // key fully removed
    QVERIFY(after.contains("keep: yes"));   // untouched key preserved
}

void TestBasesCommands::textContainsKey()
{
    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    Corbomite::FileManager fm(&vault, nullptr);
    CmdSetFrontMatter cmd(&fm, nullptr, QStringLiteral("status"),
                          QStringLiteral("x"), nullptr);
    QVERIFY(cmd.text().contains(QStringLiteral("status")));
}

QTEST_MAIN(TestBasesCommands)
#include "tst_bases_commands.moc"
```

- [ ] **Step 5: Configure + run; verify the suite passes**

Run: `cmake --build --preset dev -j 10 && (cd build-dev && ctest --output-on-failure -R tst_bases_commands)`
Expected: build succeeds; all 6 cases PASS.

> If `vault.load()` / `vault.getFileByPath()` signatures differ from what's shown, fix the test calls to match the real `Corbomite::Vault` API (read `libs/vault/include/corbomite/vault/Vault.h`) — these mirror `tst_file_manager_frontmatter.cpp`, so they should match. Tests define expected behavior: if a case fails, fix `BasesCommands.cpp`, not the assertion.

- [ ] **Step 6: Commit**

```bash
git add libs/bases/include/corbomite/bases/BasesCommands.h \
        libs/bases/src/BasesCommands.cpp \
        libs/bases/tests/tst_bases_commands.cpp \
        libs/bases/CMakeLists.txt libs/bases/tests/CMakeLists.txt
git commit -m "feat(bases): CmdSetFrontMatter — stale-guarded undoable frontmatter edit"
```

---

## Task 2: `BasesView` undo stack, chokepoint, and notify

**Files:**
- Modify: `libs/bases/include/corbomite/bases/BasesView.h`
- Modify: `libs/bases/src/BasesView.cpp`

- [ ] **Step 1: Add the stack member, slot, and undo/redo to the header**

In `libs/bases/include/corbomite/bases/BasesView.h`:

Add includes near the top (with the other Qt includes):

```cpp
#include <QUndoStack>
```

In the `public:` section (after the existing host-callback setters, e.g. after `setDeletePrompt`), add:

```cpp
    /// Drive the per-view undo stack (host wires these to Edit ▸ Undo/Redo).
    void undo();
    void redo();

public Q_SLOTS:
    /// Single chokepoint: build a CmdSetFrontMatter and push it. Connected to
    /// BasesTreeModel::frontMatterEditRequested and
    /// PropertiesDrawer::frontMatterEditRequested.
    void pushFrontMatterEdit(Corbomite::TFile *file, const QString &key,
                             const QVariant &value);
```

In the `private:` members section (near `m_model`), add:

```cpp
    QUndoStack m_undoStack;
```

- [ ] **Step 2: Implement the slot + undo/redo + notify in the cpp**

In `libs/bases/src/BasesView.cpp`, add the include near the other bases includes:

```cpp
#include "corbomite/bases/BasesCommands.h"
```

and ensure `#include <QTimer>` is present (add if absent).

Add these definitions (anywhere among the other `BasesView::` method bodies):

```cpp
void BasesView::pushFrontMatterEdit(Corbomite::TFile *file, const QString &key,
                                    const QVariant &value)
{
    if (!m_fm || !file) return;
    auto notify = [this](const QString &msg) {
        m_errorBanner->setText(msg);
        m_errorBanner->show();
        QTimer::singleShot(4000, m_errorBanner, [this]() { m_errorBanner->hide(); });
    };
    m_undoStack.push(new CmdSetFrontMatter(m_fm, file, key, value, notify));
}

void BasesView::undo() { m_undoStack.undo(); }
void BasesView::redo() { m_undoStack.redo(); }
```

- [ ] **Step 3: Clear the stack on (re)load**

In `libs/bases/src/BasesView.cpp`, at the **start** of `BasesView::loadBaseFromVault()` (around line 245), add:

```cpp
    m_undoStack.clear();   // a history never spans two base loads
```

- [ ] **Step 4: Build (no behavior wired yet — just compiles)**

Run: `cmake --build --preset dev -j 10`
Expected: clean build. (Connections come in Task 3; nothing calls `pushFrontMatterEdit` yet, so behavior is unchanged.)

- [ ] **Step 5: Commit**

```bash
git add libs/bases/include/corbomite/bases/BasesView.h libs/bases/src/BasesView.cpp
git commit -m "feat(bases): BasesView undo stack + frontmatter-edit chokepoint + notify"
```

---

## Task 3: Route model + drawer edits through the stack

**Files:**
- Modify: `libs/bases/include/corbomite/bases/BasesTreeModel.h`
- Modify: `libs/bases/src/BasesTreeModel.cpp`
- Modify: `libs/bases/include/corbomite/bases/PropertiesDrawer.h`
- Modify: `libs/bases/src/PropertiesDrawer.cpp`
- Modify: `libs/bases/src/BasesView.cpp`
- Test: `libs/bases/tests/tst_bases_tree_model.cpp`

- [ ] **Step 1: Write the failing model test**

In `libs/bases/tests/tst_bases_tree_model.cpp`, add `#include <QSignalSpy>` and `#include "corbomite/vault/TFile.h"` at the top if absent, and add this test slot to the `private Q_SLOTS:` list + body. It builds a model over real entries is heavy; instead assert the signal contract using the existing `populateForTesting` harness plus a minimal entry that has a file. Because the existing harness uses null-file placeholder entries, this test asserts the simpler invariant that **`setData` no longer writes through a FileManager and emits the request signal** by constructing the model with a real `FileManager` over a temp vault and a real entry.

Add the slot declaration:

```cpp
    void setDataEmitsRequestAndDoesNotWrite();
```

Add the body (include the temp-vault headers used by `tst_bases_commands` at the top of this file as needed: `<QTemporaryDir>`, `<QFile>`, `corbomite/vault/Vault.h`, `corbomite/vault/FileManager.h`, `corbomite/storage/FileSystemAdapter.h`):

```cpp
void TestBasesTreeModel::setDataEmitsRequestAndDoesNotWrite()
{
    QTemporaryDir dir;
    {
        QFile f(dir.path() + "/n.md");
        f.open(QIODevice::WriteOnly);
        f.write("---\nstatus: old\n---\nbody\n");
        f.close();
    }
    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());
    Corbomite::FileManager fm(&vault, nullptr);
    auto *tf = vault.getFileByPath(QStringLiteral("n.md"));
    QVERIFY(tf);

    BasesQuery q;
    BasesTreeModel m(nullptr, &fm);
    BasesEntryGroup g;   // one keyless group holding our real-file entry
    g.entries.push_back(std::make_shared<BasesEntry>(&vault, nullptr, tf, nullptr, q));
    m.populateForTesting({g}, {note("status")});

    const QModelIndex idx = m.index(0, 0, QModelIndex());
    QVERIFY(idx.isValid());
    QVERIFY(!m.isGroupRow(idx));

    QSignalSpy spy(&m, &BasesTreeModel::frontMatterEditRequested);
    QVERIFY(m.setData(idx, QStringLiteral("new"), Qt::EditRole));
    QCOMPARE(spy.count(), 1);

    // The model itself must NOT have written to disk (only the chokepoint does).
    QFile after(dir.path() + "/n.md");
    after.open(QIODevice::ReadOnly);
    const QByteArray bytes = after.readAll();
    QVERIFY(bytes.contains("status: old"));
    QVERIFY(!bytes.contains("status: new"));
}
```

> Confirm the `BasesEntry` constructor argument order `(Vault*, MetadataCache*, TFile*, ... , BasesQuery)` against `corbomite/bases/BasesEntry.h` and the existing `grp()` helper in this file — adjust the `make_shared<BasesEntry>` call to match the real signature. The `populateForTesting` + `note()` helpers already exist in this file.

- [ ] **Step 2: Run the test to verify it fails**

Run: `cmake --build --preset dev -j 10 && (cd build-dev && ctest --output-on-failure -R tst_bases_tree_model)`
Expected: FAIL — `frontMatterEditRequested` is not a member of `BasesTreeModel` (compile error), or the signal isn't emitted.

- [ ] **Step 3: Add the signal to `BasesTreeModel` and emit from `setData`**

In `libs/bases/include/corbomite/bases/BasesTreeModel.h`, add a signals section (ensure `Corbomite::TFile` is forward-declared — it is used via the entry already; add `namespace Corbomite { class TFile; }` near the top if not present):

```cpp
Q_SIGNALS:
    void frontMatterEditRequested(Corbomite::TFile *file, const QString &key,
                                  const QVariant &value);
```

In `libs/bases/src/BasesTreeModel.cpp`, replace the write call in `setData` (currently `m_fm->processFrontMatter(entry->file(), [&](QVariantMap &fm) { fm.insert(pid.name, value); });`) with:

```cpp
    Q_EMIT frontMatterEditRequested(entry->file(), pid.name, value);
```

Leave the `if (... || !m_fm) return false;` guard and the rest of `setData` unchanged.

- [ ] **Step 4: Add the signal to `PropertiesDrawer` and emit from `commit`**

In `libs/bases/include/corbomite/bases/PropertiesDrawer.h`, add (with `Corbomite::TFile` forward-declared as needed):

```cpp
Q_SIGNALS:
    void frontMatterEditRequested(Corbomite::TFile *file, const QString &key,
                                  const QVariant &value);
```

In `libs/bases/src/PropertiesDrawer.cpp`, change `commit` to:

```cpp
void PropertiesDrawer::commit(const QString &key, const QVariant &value)
{
    if (!m_fm || !m_file) return;
    Q_EMIT frontMatterEditRequested(m_file, key, value);
}
```

- [ ] **Step 5: Connect both signals to the chokepoint in `BasesView`**

In `libs/bases/src/BasesView.cpp`:

After the drawer is created (line ~197, `m_drawer = new PropertiesDrawer(m_splitter);`), add:

```cpp
    connect(m_drawer, &PropertiesDrawer::frontMatterEditRequested,
            this, &BasesView::pushFrontMatterEdit);
```

After the model is created (line ~330, `m_model = std::make_unique<BasesTreeModel>(m_controller.get(), m_fm, this);`), add:

```cpp
    connect(m_model.get(), &BasesTreeModel::frontMatterEditRequested,
            this, &BasesView::pushFrontMatterEdit);
```

- [ ] **Step 6: Run the model test + full bases suite**

Run: `cmake --build --preset dev -j 10 && (cd build-dev && ctest --output-on-failure -R 'tst_bases')`
Expected: `tst_bases_tree_model` (incl. the new case) and `tst_bases_commands` PASS; all other bases tests still green.

- [ ] **Step 7: Commit**

```bash
git add libs/bases/include/corbomite/bases/BasesTreeModel.h \
        libs/bases/src/BasesTreeModel.cpp \
        libs/bases/include/corbomite/bases/PropertiesDrawer.h \
        libs/bases/src/PropertiesDrawer.cpp \
        libs/bases/src/BasesView.cpp \
        libs/bases/tests/tst_bases_tree_model.cpp
git commit -m "feat(bases): route cell + drawer edits through the undo chokepoint"
```

---

## Task 4: App-level Undo/Redo routing

**Files:**
- Modify: `src/app/MainWindow.cpp`

- [ ] **Step 1: Add the BasesView branch to the undo action**

In `src/app/MainWindow.cpp`, the `KStandardAction::undo` lambda (around line 1229) currently begins by fetching `activeEditor()`. Add a Bases branch **first** (Bases views are not editors, so `activeEditor()` is null for them):

```cpp
    KStandardAction::undo(this, [this]() {
        if (m_workspace && m_workspace->activeLeaf()) {
            if (auto *bv = qobject_cast<Corbomite::Bases::BasesView *>(
                    m_workspace->activeLeaf()->view())) {
                bv->undo();
                return;
            }
        }
        auto *editor = activeEditor();
        if (!editor) return;
        // ... existing editor body unchanged ...
```

- [ ] **Step 2: Add the BasesView branch to the redo action**

In the `KStandardAction::redo` lambda (around line 1239), add the symmetric branch before the `activeEditor()` fetch:

```cpp
    KStandardAction::redo(this, [this]() {
        if (m_workspace && m_workspace->activeLeaf()) {
            if (auto *bv = qobject_cast<Corbomite::Bases::BasesView *>(
                    m_workspace->activeLeaf()->view())) {
                bv->redo();
                return;
            }
        }
        auto *editor = activeEditor();
        if (!editor) return;
        // ... existing editor body unchanged ...
```

> `MainWindow.cpp` already includes `corbomite/bases/BasesView.h` (line 20) and already calls `m_workspace->activeLeaf()->view()` elsewhere (see `activeMarkdownView`, ~line 646), so no new include or accessor is needed. Confirm `m_workspace->activeLeaf()->view()` is the same call shape used there.

- [ ] **Step 3: Build**

Run: `cmake --build --preset dev -j 10`
Expected: clean build.

- [ ] **Step 4: Commit**

```bash
git add src/app/MainWindow.cpp
git commit -m "feat(app): route Edit Undo/Redo to the active Bases view"
```

---

## Task 5: Full-suite verification + close-out

**Files:**
- Modify: `docs/PROJECT-STATE.md`, `docs/superpowers/plans/INDEX.md`, `docs/decisions-archive.md`

- [ ] **Step 1: Full bases suite + clean build**

Run: `cmake --build --preset dev -j 10 && (cd build-dev && ctest --output-on-failure -j 10 -R 'tst_bases')`
Expected: every bases test green, including `tst_bases_commands` (6) and the extended `tst_bases_tree_model`.

- [ ] **Step 2: Launch smoke**

Run: `./build-dev/Corbomite`
Expected: opens, a `.base` view renders, no crash; clean exit. (Real Ctrl+Z behavior + the drift banner are pending user eyeball — offscreen Qt can't drive focus + action routing.)

- [ ] **Step 3: Update tracking docs**

- `docs/PROJECT-STATE.md`: in §"Recent decisions" add a dated **2026-05-27 — Cluster D.4c shipped** entry (≤3 sentences, slim rule); in §"Active strategic clusters" update the Cluster D row to mark D.4c done and drop "D.4c (undo)" from the remaining list (leaving formula editor + filter builder + D.5); update §"Last touched".
- `docs/superpowers/plans/INDEX.md`: update the Cluster D row — mark D.4c done, link this plan + the spec.
- `docs/decisions-archive.md`: append a full close-out paragraph under a new `## 2026-05-27 — Cluster D.4c (Bases undo/redo for value edits)` H2: the standalone stale-guarded `CmdSetFrontMatter` (all I/O inside one `processFrontMatter` mutator → race-free stale check), the model/drawer signal chokepoint, MainWindow routing, what's covered by tests vs the pending-user-eyeball widget paths, and the remaining D items.

- [ ] **Step 4: Commit the close-out**

```bash
git add docs/PROJECT-STATE.md docs/superpowers/plans/INDEX.md docs/decisions-archive.md
git commit -m "docs(tracking): close out Cluster D.4c (Bases undo/redo)"
```

---

## Self-review notes (for the implementer)

- **Spec coverage:** Task 1 = `CmdSetFrontMatter` (decisions 1 & 3: stale guard, neutralize, key-absent removal) + its tests (spec testing cases 1–6); Task 2 = the per-view `QUndoStack`, chokepoint, notify banner, clear-on-load; Task 3 = model/drawer signal rewiring (decision 2: value edits only — "+New" and context-menu ops are untouched) + spec test case 7; Task 4 = app action routing; Task 5 = verification + tracking. Every spec section maps to a task.
- **Type consistency:** `frontMatterEditRequested(Corbomite::TFile*, const QString&, const QVariant&)` is identical across `BasesTreeModel`, `PropertiesDrawer`, and the `BasesView::pushFrontMatterEdit` slot signature. `CmdSetFrontMatter`'s ctor takes `(FileManager*, TFile*, QString, QVariant, std::function<void(const QString&)>)` everywhere it's constructed (Task 1 tests + Task 2 chokepoint).
- **Verify-against-source flags** (resolve by reading the cited code; do not guess): the vault/storage CMake target names (Task 1 Step 3); `Vault::load`/`getFileByPath` signatures and the `BasesEntry` ctor arg order (Task 1 Step 5, Task 3 Step 1); the `m_workspace->activeLeaf()->view()` call shape (Task 4 Step 2); the exact `loadBaseFromVault` body location for the clear (Task 2 Step 3).
- **Deferred (not in this plan):** "+New"/rename/delete/view-config undo, edit coalescing, cross-view unified undo, eliminating the idempotent re-write on a declined mutator — all explicitly out of scope per the spec.
```
