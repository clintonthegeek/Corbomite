# Ribbon-to-Toolbar Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace `Corbomite::RibbonSlot` (left-docked bespoke widget) with a second `KToolBar` docked top-right of the main toolbar, driven by per-vault state in `workspace.json['left-ribbon']` through the existing `SessionManager`.

**Architecture:** Three new pieces + two changes.
- **`RibbonToolBar`** (src/app/): id-keyed `KToolBar` subclass, visibility-controllable.
- **`RibbonStateController`** (src/app/): bridges `RibbonToolBar` ⇄ `SessionManager`; rebound on vault open/close.
- **`SessionManager::{leftRibbonState, setLeftRibbonState}`**: typed accessors promoting `['left-ribbon']` from unknown-root pass-through to first-class state.
- **`MainWindow::setupRibbonToolBar()`** replaces `setupRibbon()`: `addToolBar(Qt::TopToolBarArea, m_ribbonToolBar)` after the main toolbar.
- **`RibbonSlot` + `tst_ribbonslot` deleted.**

**Spec deviations from `docs/superpowers/specs/2026-04-20-ribbon-to-toolbar-design.md`:**
1. `RibbonStateController` lives in `src/app/`, not `libs/vault/` — `SessionManager` is in `src/app/` and `libs/vault` cannot depend on app code. Lifecycle is still vault-scoped: the controller reacts to `CorbomiteApp::vaultOpened`/`vaultClosed`.
2. `QJsonObject` sorts keys alphabetically on round-trip, so the spec's "map key order = runtime order" invariant cannot be preserved. Since drag-reorder is explicitly dropped, iteration order is alphabetical-by-id; `hiddenItems` (the visibility map) is the only load-bearing field and round-trips faithfully.

**Tech Stack:** Qt6 / KF6 (KToolBar, KXMLGUI), C++20, QTest. Builds through the existing `CorbomiteApp` static library.

---

## File Structure

| File | Role | Change |
|---|---|---|
| `src/app/RibbonToolBar.h` / `.cpp` | KToolBar subclass, id-keyed action registry | **Create** |
| `src/app/RibbonStateController.h` / `.cpp` | Apply/persist left-ribbon state | **Create** |
| `src/app/SessionManager.h` / `.cpp` | Add `leftRibbonState()` / `setLeftRibbonState()` | Modify |
| `src/app/MainWindow.h` / `.cpp` | Replace `setupRibbon`; own controller; drop 3 hardcoded entries | Modify |
| `src/app/RibbonSlot.h` / `.cpp` | Old class | **Delete** |
| `src/CMakeLists.txt` | Source list: add new .cpp, remove `RibbonSlot.cpp` | Modify |
| `tests/dialogs/tst_ribbontoolbar.cpp` | Unit test for `RibbonToolBar` | **Create** |
| `tests/dialogs/tst_ribbonstatecontroller.cpp` | Unit test for `RibbonStateController` | **Create** |
| `tests/dialogs/tst_ribbonslot.cpp` | Old test | **Delete** |
| `tests/dialogs/CMakeLists.txt` | Register new tests, remove `tst_ribbonslot` | Modify |
| `tests/storage/tst_session_manager_roundtrip.cpp` | Add left-ribbon round-trip test | Modify |
| `docs/backlog.md` | Deferred UX: drag-reorder + per-item hide | Modify |

---

## Phase 1 — SessionManager: typed `['left-ribbon']` accessors

### Task 1.1: Write failing round-trip test

**Files:**
- Modify: `tests/storage/tst_session_manager_roundtrip.cpp`

- [ ] **Step 1: Add the failing test at the bottom of the test class**

Append inside `TestSessionManagerRoundtrip` before the closing brace (before `QTEST_MAIN`):

```cpp
    // -----------------------------------------------------------------------
    // 16. setLeftRibbonState round-trips and sits at the workspace.json root
    // (not inside _corbomite), matching Obsidian's schema.
    // -----------------------------------------------------------------------
    void leftRibbonStateRoundTrips()
    {
        QTemporaryDir tmp;
        const QString path = tmp.path() + QStringLiteral("/.obsidian/workspace.json");

        SessionManager sm;
        sm.setSessionPath(path);

        QJsonObject hidden;
        hidden.insert(QStringLiteral("core:graph_view"), true);
        hidden.insert(QStringLiteral("core:quick_switcher"), false);
        QJsonObject ribbon;
        ribbon.insert(QStringLiteral("hiddenItems"), hidden);

        sm.setLeftRibbonState(ribbon);
        sm.saveNow();

        const QJsonObject root = readJson(path);
        QVERIFY2(root.contains(QStringLiteral("left-ribbon")),
                 "left-ribbon must be at the workspace.json root");
        QVERIFY2(!root.value(QStringLiteral("_corbomite")).toObject()
                     .contains(QStringLiteral("left-ribbon")),
                 "left-ribbon must NOT live under _corbomite");

        SessionManager sm2;
        sm2.setSessionPath(path);
        QVERIFY(sm2.load());
        const QJsonObject reloaded = sm2.leftRibbonState();
        const QJsonObject reloadedHidden =
            reloaded.value(QStringLiteral("hiddenItems")).toObject();
        QCOMPARE(reloadedHidden.value(QStringLiteral("core:graph_view")).toBool(), true);
        QCOMPARE(reloadedHidden.value(QStringLiteral("core:quick_switcher")).toBool(), false);
    }

    // -----------------------------------------------------------------------
    // 17. Pre-existing left-ribbon content (written by Obsidian) is preserved
    // on a Corbomite load → save cycle even when Corbomite never calls
    // setLeftRibbonState (unknown-key preservation invariant).
    // -----------------------------------------------------------------------
    void leftRibbonPreservedFromExternalWriter()
    {
        QTemporaryDir tmp;
        const QString path = tmp.path() + QStringLiteral("/.obsidian/workspace.json");

        QJsonObject external;
        QJsonObject hidden;
        hidden.insert(QStringLiteral("obsidian-plugin:whatever"), true);
        QJsonObject ribbon;
        ribbon.insert(QStringLiteral("hiddenItems"), hidden);
        external.insert(QStringLiteral("left-ribbon"), ribbon);
        writeJson(path, external);

        SessionManager sm;
        sm.setSessionPath(path);
        QVERIFY(sm.load());
        sm.saveNow();

        const QJsonObject root = readJson(path);
        const QJsonObject reloadedHidden = root.value(QStringLiteral("left-ribbon"))
            .toObject().value(QStringLiteral("hiddenItems")).toObject();
        QCOMPARE(reloadedHidden.value(QStringLiteral("obsidian-plugin:whatever")).toBool(),
                 true);
    }
```

- [ ] **Step 2: Build and confirm the new tests fail**

```bash
cmake --build build -j 10 --target tst_session_manager_roundtrip
cd build && ctest -R tst_session_manager_roundtrip --output-on-failure
```

Expected: compile fails on `sm.setLeftRibbonState` / `sm2.leftRibbonState` — neither exists yet.

### Task 1.2: Add typed accessors to SessionManager

**Files:**
- Modify: `src/app/SessionManager.h`
- Modify: `src/app/SessionManager.cpp`

- [ ] **Step 1: Declare the field + accessors in the header**

In `src/app/SessionManager.h`, add to the public section (after `setPluginSessionState`, before `setWorkspaceLayout`):

```cpp
    /// Replace the `['left-ribbon']` sub-object at workspace.json root.
    /// Pass `{}` to clear. Triggers scheduleSave().
    void setLeftRibbonState(const QJsonObject &state);
```

Add to the accessors group (after `pluginSessionState`, before `workspaceLayout`):

```cpp
    QJsonObject leftRibbonState() const;
```

Add to the private fields (after `m_mainJson`):

```cpp
    QJsonObject m_leftRibbon;
```

- [ ] **Step 2: Implement in the .cpp**

In `src/app/SessionManager.cpp`, add a constant in the anonymous namespace next to `kCorbomite`:

```cpp
constexpr auto kLeftRibbon = "left-ribbon";
```

In `SessionManager::load()`, just after the `m_corbomiteTail` block (around existing line 74, before the "Everything else" for-loop), add:

```cpp
    if (root.contains(QLatin1String(kLeftRibbon))
            && root.value(QLatin1String(kLeftRibbon)).isObject()) {
        m_leftRibbon = root.value(QLatin1String(kLeftRibbon)).toObject();
    }
```

And extend the skip-list in the unknown-root loop so we don't double-store it. Change the `continue` condition to also skip `kLeftRibbon`:

```cpp
    for (auto it = root.begin(); it != root.end(); ++it) {
        if (it.key() == QLatin1String(kMain)
                || it.key() == QLatin1String(kActive)
                || it.key() == QLatin1String(kCorbomite)
                || it.key() == QLatin1String(kLeftRibbon)) continue;
        m_unknownRoot.insert(it.key(), it.value());
    }
```

Also reset `m_leftRibbon = {};` at the top of `load()` alongside the other clears:

```cpp
    m_loaded = false;
    m_corbomiteTail = {};
    m_unknownRoot = {};
    m_mainJson = {};
    m_leftRibbon = {};
    m_activeLeafId.clear();
```

Add the implementation at the bottom of the `--- Granular setters ---` block:

```cpp
void SessionManager::setLeftRibbonState(const QJsonObject &state)
{
    m_leftRibbon = state;
    scheduleSave();
}
```

Add the accessor in the `--- Accessors ---` block:

```cpp
QJsonObject SessionManager::leftRibbonState() const { return m_leftRibbon; }
```

In `doSave()`, insert the left-ribbon key after `_corbomite` is composed (before `QSaveFile file(...)`):

```cpp
    if (!m_leftRibbon.isEmpty()) {
        root.insert(QLatin1String(kLeftRibbon), m_leftRibbon);
    }
```

- [ ] **Step 3: Build and run the new tests**

```bash
cmake --build build -j 10 --target tst_session_manager_roundtrip
cd build && ctest -R tst_session_manager_roundtrip --output-on-failure
```

Expected: all tests pass, including the two new cases.

- [ ] **Step 4: Commit**

```bash
git add src/app/SessionManager.h src/app/SessionManager.cpp \
        tests/storage/tst_session_manager_roundtrip.cpp
git commit -m "feat(session): typed left-ribbon accessors on SessionManager

Promotes workspace.json['left-ribbon'] from unknown-root pass-through
to a first-class field with leftRibbonState() / setLeftRibbonState().
Preserves the pre-existing Obsidian-written content invariant.

Part of the ribbon-to-toolbar refactor (docs/superpowers/plans/2026-04-20-ribbon-to-toolbar.md)."
```

---

## Phase 2 — `RibbonToolBar` class

### Task 2.1: Write failing unit tests

**Files:**
- Create: `tests/dialogs/tst_ribbontoolbar.cpp`
- Modify: `tests/dialogs/CMakeLists.txt`

- [ ] **Step 1: Create the test file**

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QIcon>
#include <QSignalSpy>
#include <QTest>

#include "app/RibbonToolBar.h"

class TestRibbonToolBar : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void addReturnsIdWhenSuccessful()
    {
        Corbomite::RibbonToolBar bar;
        auto h = bar.addRibbonIcon(QStringLiteral("core:quick_switcher"),
                                   QIcon(),
                                   QStringLiteral("Open quick switcher"),
                                   []() {});
        QCOMPARE(h, QStringLiteral("core:quick_switcher"));
        QVERIFY(bar.hasIcon(h));
        QCOMPARE(bar.iconCount(), 1);
    }

    void emptyIdRejected()
    {
        Corbomite::RibbonToolBar bar;
        auto h = bar.addRibbonIcon(QString(), QIcon(),
                                   QStringLiteral("anything"), []() {});
        QVERIFY(h.isEmpty());
        QCOMPARE(bar.iconCount(), 0);
    }

    void duplicateIdSilentlyDropped()
    {
        // Obsidian compat: id collision (including the title-collision case
        // where id is pluginId:title) leaves the first registration in place.
        Corbomite::RibbonToolBar bar;
        auto first = bar.addRibbonIcon(QStringLiteral("plugin-a:Open"),
                                       QIcon(), QStringLiteral("Open"), []() {});
        auto second = bar.addRibbonIcon(QStringLiteral("plugin-a:Open"),
                                        QIcon(), QStringLiteral("Open"), []() {});
        QVERIFY(!first.isEmpty());
        QVERIFY(second.isEmpty());
        QCOMPARE(bar.iconCount(), 1);
    }

    void removeIcon()
    {
        Corbomite::RibbonToolBar bar;
        auto h = bar.addRibbonIcon(QStringLiteral("core:switcher"), QIcon(),
                                   QStringLiteral("Switcher"), []() {});
        QVERIFY(bar.removeRibbonIcon(h));
        QVERIFY(!bar.hasIcon(h));
        QCOMPARE(bar.iconCount(), 0);
    }

    void removeUnknownReturnsFalse()
    {
        Corbomite::RibbonToolBar bar;
        QVERIFY(!bar.removeRibbonIcon(QStringLiteral("never-added")));
    }

    void callbackFiresOnTrigger()
    {
        Corbomite::RibbonToolBar bar;
        int calls = 0;
        auto h = bar.addRibbonIcon(QStringLiteral("core:counter"), QIcon(),
                                   QStringLiteral("Counter"),
                                   [&calls]() { ++calls; });
        QAction *act = bar.actionForId(h);
        QVERIFY(act);
        act->trigger();
        QCOMPARE(calls, 1);
    }

    void visibilityRoundTrips()
    {
        Corbomite::RibbonToolBar bar;
        auto h = bar.addRibbonIcon(QStringLiteral("core:graph"), QIcon(),
                                   QStringLiteral("Graph"), []() {});
        QVERIFY(bar.isIconVisible(h));
        bar.setIconVisible(h, false);
        QVERIFY(!bar.isIconVisible(h));
        bar.setIconVisible(h, true);
        QVERIFY(bar.isIconVisible(h));
    }

    void iconIdsInOrderReflectsInsertion()
    {
        Corbomite::RibbonToolBar bar;
        bar.addRibbonIcon(QStringLiteral("a"), QIcon(), QStringLiteral("A"), []() {});
        bar.addRibbonIcon(QStringLiteral("b"), QIcon(), QStringLiteral("B"), []() {});
        bar.addRibbonIcon(QStringLiteral("c"), QIcon(), QStringLiteral("C"), []() {});
        QCOMPARE(bar.iconIdsInOrder(),
                 (QStringList{QStringLiteral("a"),
                              QStringLiteral("b"),
                              QStringLiteral("c")}));
    }

    void signalsFireOnAddRemove()
    {
        Corbomite::RibbonToolBar bar;
        QSignalSpy added(&bar, &Corbomite::RibbonToolBar::iconAdded);
        QSignalSpy removed(&bar, &Corbomite::RibbonToolBar::iconRemoved);

        auto h = bar.addRibbonIcon(QStringLiteral("core:x"), QIcon(),
                                   QStringLiteral("X"), []() {});
        QCOMPARE(added.count(), 1);
        QCOMPARE(added.takeFirst().at(0).toString(), QStringLiteral("core:x"));

        bar.removeRibbonIcon(h);
        QCOMPARE(removed.count(), 1);
        QCOMPARE(removed.takeFirst().at(0).toString(), QStringLiteral("core:x"));
    }

    void signalFiresOnVisibilityChange()
    {
        Corbomite::RibbonToolBar bar;
        auto h = bar.addRibbonIcon(QStringLiteral("core:y"), QIcon(),
                                   QStringLiteral("Y"), []() {});
        QSignalSpy changed(&bar, &Corbomite::RibbonToolBar::iconVisibilityChanged);
        bar.setIconVisible(h, false);
        QCOMPARE(changed.count(), 1);
        const auto args = changed.takeFirst();
        QCOMPARE(args.at(0).toString(), QStringLiteral("core:y"));
        QCOMPARE(args.at(1).toBool(), false);
    }
};

QTEST_MAIN(TestRibbonToolBar)
#include "tst_ribbontoolbar.moc"
```

- [ ] **Step 2: Register the test in CMake**

In `tests/dialogs/CMakeLists.txt`, right after the `tst_ribbonslot` block (around line 14–17), add:

```cmake
add_executable(tst_ribbontoolbar tst_ribbontoolbar.cpp)
add_test(NAME tst_ribbontoolbar COMMAND tst_ribbontoolbar)
target_link_libraries(tst_ribbontoolbar PRIVATE Qt6::Test CorbomiteApp)
set_tests_properties(tst_ribbontoolbar PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
```

- [ ] **Step 3: Build and confirm it fails**

```bash
cmake --build build -j 10 --target tst_ribbontoolbar 2>&1 | head -30
```

Expected: fails because `app/RibbonToolBar.h` does not exist.

### Task 2.2: Implement `RibbonToolBar`

**Files:**
- Create: `src/app/RibbonToolBar.h`
- Create: `src/app/RibbonToolBar.cpp`
- Modify: `src/CMakeLists.txt`

- [ ] **Step 1: Create the header**

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <functional>

#include <QHash>
#include <QIcon>
#include <QString>
#include <QStringList>

#include <KToolBar>

class QAction;

namespace Corbomite {

/// Top-docked, programmatically-managed second toolbar. Serves as the
/// Corbomite translation of Obsidian's left-edge ribbon. Action identity
/// is a full id string (Obsidian convention: "<pluginId>:<title>" for
/// plugin-registered items, a stable internal id for core items).
///
/// Per-vault state — visibility per icon — is held in
/// workspace.json['left-ribbon']. This class is stateless with respect
/// to the vault; RibbonStateController drives setIconVisible/...
/// based on SessionManager content.
///
/// Not managed by KXMLGUI. Actions are added programmatically and do
/// not appear in Settings → Configure Toolbars.
class RibbonToolBar : public KToolBar {
    Q_OBJECT

public:
    using Handle = QString;

    explicit RibbonToolBar(QWidget *parent = nullptr);
    explicit RibbonToolBar(const QString &objectName, QMainWindow *parent);

    /// Returns `id` on success, empty Handle if `id` is empty or already
    /// registered (Obsidian title-collision quirk, preserved).
    Handle addRibbonIcon(const Handle &id,
                         const QIcon &icon,
                         const QString &title,
                         std::function<void()> onActivated);

    bool removeRibbonIcon(const Handle &id);
    int iconCount() const;
    bool hasIcon(const Handle &id) const;

    QStringList iconIdsInOrder() const;

    void setIconVisible(const Handle &id, bool visible);
    bool isIconVisible(const Handle &id) const;

    /// Accessor for tests and for plugin code that needs to wire further
    /// behaviour (e.g. keyboard shortcut) onto the generated QAction.
    QAction *actionForId(const Handle &id) const;

Q_SIGNALS:
    void iconAdded(const QString &id);
    void iconRemoved(const QString &id);
    void iconVisibilityChanged(const QString &id, bool visible);

private:
    void commonInit();

    QHash<QString, QAction *> m_actions;
    QStringList m_order;  // preserves insertion order for iconIdsInOrder()
};

} // namespace Corbomite
```

- [ ] **Step 2: Create the implementation**

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "RibbonToolBar.h"

#include <QAction>
#include <QMainWindow>

namespace Corbomite {

RibbonToolBar::RibbonToolBar(QWidget *parent)
    : KToolBar(parent)
{
    commonInit();
}

RibbonToolBar::RibbonToolBar(const QString &objectName, QMainWindow *parent)
    : KToolBar(objectName, parent)
{
    commonInit();
}

void RibbonToolBar::commonInit()
{
    setToolButtonStyle(Qt::ToolButtonIconOnly);
}

RibbonToolBar::Handle RibbonToolBar::addRibbonIcon(const Handle &id,
                                                   const QIcon &icon,
                                                   const QString &title,
                                                   std::function<void()> onActivated)
{
    if (id.isEmpty()) return {};
    if (m_actions.contains(id)) return {};

    auto *action = new QAction(icon, title, this);
    action->setToolTip(title);
    if (onActivated) {
        QObject::connect(action, &QAction::triggered,
                         this, std::move(onActivated));
    }
    addAction(action);
    m_actions.insert(id, action);
    m_order.append(id);

    Q_EMIT iconAdded(id);
    return id;
}

bool RibbonToolBar::removeRibbonIcon(const Handle &id)
{
    auto it = m_actions.find(id);
    if (it == m_actions.end()) return false;
    QAction *action = it.value();
    removeAction(action);
    action->deleteLater();
    m_actions.erase(it);
    m_order.removeAll(id);

    Q_EMIT iconRemoved(id);
    return true;
}

int RibbonToolBar::iconCount() const { return m_actions.size(); }

bool RibbonToolBar::hasIcon(const Handle &id) const
{
    return m_actions.contains(id);
}

QStringList RibbonToolBar::iconIdsInOrder() const { return m_order; }

void RibbonToolBar::setIconVisible(const Handle &id, bool visible)
{
    auto it = m_actions.find(id);
    if (it == m_actions.end()) return;
    if (it.value()->isVisible() == visible) return;
    it.value()->setVisible(visible);
    Q_EMIT iconVisibilityChanged(id, visible);
}

bool RibbonToolBar::isIconVisible(const Handle &id) const
{
    auto it = m_actions.find(id);
    if (it == m_actions.end()) return false;
    return it.value()->isVisible();
}

QAction *RibbonToolBar::actionForId(const Handle &id) const
{
    return m_actions.value(id, nullptr);
}

} // namespace Corbomite
```

- [ ] **Step 3: Add the new source file to `CorbomiteApp`**

In `src/CMakeLists.txt`, in the `add_library(CorbomiteApp STATIC ...)` block, **add** a line immediately after `app/RibbonSlot.cpp`. Both must coexist until Phase 4, because `MainWindow.cpp` still includes `RibbonSlot.h`.

Before:
```cmake
    app/RibbonSlot.cpp
    app/WelcomeScreen.cpp
```

After:
```cmake
    app/RibbonSlot.cpp
    app/RibbonToolBar.cpp
    app/WelcomeScreen.cpp
```

- [ ] **Step 4: Build + run the new test**

```bash
cmake --build build -j 10 --target tst_ribbontoolbar
cd build && ctest -R tst_ribbontoolbar --output-on-failure
```

Expected: all tests pass.

- [ ] **Step 5: Commit**

```bash
git add src/app/RibbonToolBar.h src/app/RibbonToolBar.cpp \
        src/CMakeLists.txt \
        tests/dialogs/tst_ribbontoolbar.cpp \
        tests/dialogs/CMakeLists.txt
git commit -m "feat(app): RibbonToolBar — KToolBar subclass with id-keyed action registry

Stands up the replacement for RibbonSlot. Top-docked by consumers,
id-keyed (pluginId:title for plugins, stable internal id for core),
preserves Obsidian's title-collision quirk, exposes per-icon
visibility for the forthcoming RibbonStateController.

Part of docs/superpowers/plans/2026-04-20-ribbon-to-toolbar.md."
```

---

## Phase 3 — `RibbonStateController`

### Task 3.1: Write failing controller tests

**Files:**
- Create: `tests/dialogs/tst_ribbonstatecontroller.cpp`
- Modify: `tests/dialogs/CMakeLists.txt`

- [ ] **Step 1: Create the test file**

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

#include "SessionManager.h"
#include "app/RibbonStateController.h"
#include "app/RibbonToolBar.h"

using namespace Corbomite;

namespace {

void writeJson(const QString &path, const QJsonObject &obj)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    QVERIFY2(f.open(QIODevice::WriteOnly), qPrintable(path));
    f.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
}

QJsonObject readJson(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    return QJsonDocument::fromJson(f.readAll()).object();
}

} // namespace

class TestRibbonStateController : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void appliesHiddenItemsOnBind()
    {
        // Register icons first, then bind to a SessionManager whose
        // left-ribbon state marks some of them hidden.
        RibbonToolBar bar;
        bar.addRibbonIcon(QStringLiteral("core:a"), QIcon(),
                          QStringLiteral("A"), []() {});
        bar.addRibbonIcon(QStringLiteral("core:b"), QIcon(),
                          QStringLiteral("B"), []() {});

        QTemporaryDir tmp;
        const QString path = tmp.path() + QStringLiteral("/.obsidian/workspace.json");
        QJsonObject external;
        QJsonObject hidden;
        hidden.insert(QStringLiteral("core:a"), true);
        hidden.insert(QStringLiteral("core:b"), false);
        QJsonObject ribbon;
        ribbon.insert(QStringLiteral("hiddenItems"), hidden);
        external.insert(QStringLiteral("left-ribbon"), ribbon);
        writeJson(path, external);

        SessionManager sm;
        sm.setSessionPath(path);
        QVERIFY(sm.load());

        RibbonStateController controller(&bar, &sm);
        controller.applyFromSession();

        QVERIFY(!bar.isIconVisible(QStringLiteral("core:a")));
        QVERIFY(bar.isIconVisible(QStringLiteral("core:b")));
    }

    void appliesRetroactivelyToLateArrivingIcons()
    {
        // Icons registered AFTER bind should still pick up hidden state.
        QTemporaryDir tmp;
        const QString path = tmp.path() + QStringLiteral("/.obsidian/workspace.json");
        QJsonObject external;
        QJsonObject hidden;
        hidden.insert(QStringLiteral("plugin-x:Thing"), true);
        QJsonObject ribbon;
        ribbon.insert(QStringLiteral("hiddenItems"), hidden);
        external.insert(QStringLiteral("left-ribbon"), ribbon);
        writeJson(path, external);

        SessionManager sm;
        sm.setSessionPath(path);
        QVERIFY(sm.load());

        RibbonToolBar bar;
        RibbonStateController controller(&bar, &sm);
        controller.applyFromSession();

        // Register the icon AFTER the controller is already applying.
        bar.addRibbonIcon(QStringLiteral("plugin-x:Thing"), QIcon(),
                          QStringLiteral("Thing"), []() {});

        QVERIFY(!bar.isIconVisible(QStringLiteral("plugin-x:Thing")));
    }

    void visibilityChangeWritesThroughToSession()
    {
        QTemporaryDir tmp;
        const QString path = tmp.path() + QStringLiteral("/.obsidian/workspace.json");
        SessionManager sm;
        sm.setSessionPath(path);

        RibbonToolBar bar;
        RibbonStateController controller(&bar, &sm);
        controller.applyFromSession();

        bar.addRibbonIcon(QStringLiteral("core:graph"), QIcon(),
                          QStringLiteral("Graph"), []() {});
        bar.setIconVisible(QStringLiteral("core:graph"), false);

        sm.saveNow();  // flush the debounced save

        const QJsonObject root = readJson(path);
        const QJsonObject hidden = root.value(QStringLiteral("left-ribbon"))
            .toObject().value(QStringLiteral("hiddenItems")).toObject();
        QCOMPARE(hidden.value(QStringLiteral("core:graph")).toBool(), true);
    }

    void rebindClearsVisibilityFromStaleVault()
    {
        // Simulate a vault switch: rebind to a fresh (empty) SessionManager
        // after having a hidden state in place. Icons should become visible
        // again because the new session has no left-ribbon content.
        RibbonToolBar bar;
        bar.addRibbonIcon(QStringLiteral("core:g"), QIcon(),
                          QStringLiteral("G"), []() {});

        SessionManager sm1;
        QTemporaryDir t1;
        sm1.setSessionPath(t1.path() + QStringLiteral("/.obsidian/workspace.json"));
        QJsonObject hidden; hidden.insert(QStringLiteral("core:g"), true);
        QJsonObject ribbon; ribbon.insert(QStringLiteral("hiddenItems"), hidden);
        sm1.setLeftRibbonState(ribbon);

        RibbonStateController controller(&bar, &sm1);
        controller.applyFromSession();
        QVERIFY(!bar.isIconVisible(QStringLiteral("core:g")));

        SessionManager sm2;
        QTemporaryDir t2;
        sm2.setSessionPath(t2.path() + QStringLiteral("/.obsidian/workspace.json"));

        controller.rebind(&sm2);
        controller.applyFromSession();
        QVERIFY(bar.isIconVisible(QStringLiteral("core:g")));
    }
};

QTEST_MAIN(TestRibbonStateController)
#include "tst_ribbonstatecontroller.moc"
```

- [ ] **Step 2: Register in CMake**

In `tests/dialogs/CMakeLists.txt`, after the `tst_ribbontoolbar` block:

```cmake
add_executable(tst_ribbonstatecontroller tst_ribbonstatecontroller.cpp)
add_test(NAME tst_ribbonstatecontroller COMMAND tst_ribbonstatecontroller)
target_link_libraries(tst_ribbonstatecontroller PRIVATE Qt6::Test CorbomiteApp)
set_tests_properties(tst_ribbonstatecontroller PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
```

- [ ] **Step 3: Confirm build fails**

```bash
cmake --build build -j 10 --target tst_ribbonstatecontroller 2>&1 | head -20
```

Expected: fails because `RibbonStateController.h` does not exist.

### Task 3.2: Implement `RibbonStateController`

**Files:**
- Create: `src/app/RibbonStateController.h`
- Create: `src/app/RibbonStateController.cpp`
- Modify: `src/CMakeLists.txt`

- [ ] **Step 1: Create the header**

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QPointer>
#include <QString>

namespace Corbomite {

class RibbonToolBar;
class SessionManager;

/// Bridges RibbonToolBar visibility state and SessionManager's
/// workspace.json['left-ribbon'].hiddenItems. Lifetime is owned by
/// MainWindow; rebind() on vault open/close. Not thread-safe.
///
/// The controller outlives any single SessionManager — one is created
/// per vault in MainWindow::onVaultOpened. rebind() swaps the current
/// SessionManager pointer; applyFromSession() re-applies visibility
/// to whichever icons are currently registered on the toolbar.
class RibbonStateController : public QObject {
    Q_OBJECT

public:
    RibbonStateController(RibbonToolBar *toolBar,
                          SessionManager *session,
                          QObject *parent = nullptr);
    ~RibbonStateController() override;

    /// Swap the backing SessionManager. The next applyFromSession()
    /// call (or the next icon visibility change) uses the new pointer.
    /// Passing nullptr suspends write-through until a new session binds.
    void rebind(SessionManager *session);

    /// Read the current session's `left-ribbon.hiddenItems` and apply
    /// visibility to every currently-registered icon. Safe to call
    /// repeatedly and before icons are registered (in which case the
    /// controller caches the map and applies on iconAdded).
    void applyFromSession();

private Q_SLOTS:
    void onIconAdded(const QString &id);
    void onIconVisibilityChanged(const QString &id, bool visible);

private:
    void writeThrough(const QString &id, bool hidden);

    QPointer<RibbonToolBar> m_toolBar;
    QPointer<SessionManager> m_session;
    /// Latest hiddenItems map cached from applyFromSession(), used to
    /// colour late-arriving icons.
    QJsonObject m_cachedHiddenItems;
};

} // namespace Corbomite
```

Note: the header uses `QJsonObject` — add `#include <QJsonObject>` after the other Qt includes.

- [ ] **Step 2: Create the implementation**

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "RibbonStateController.h"

#include <QJsonObject>

#include "RibbonToolBar.h"
#include "SessionManager.h"

namespace Corbomite {

namespace {
constexpr auto kHiddenItems = "hiddenItems";
} // namespace

RibbonStateController::RibbonStateController(RibbonToolBar *toolBar,
                                             SessionManager *session,
                                             QObject *parent)
    : QObject(parent)
    , m_toolBar(toolBar)
    , m_session(session)
{
    if (m_toolBar) {
        connect(m_toolBar, &RibbonToolBar::iconAdded,
                this, &RibbonStateController::onIconAdded);
        connect(m_toolBar, &RibbonToolBar::iconVisibilityChanged,
                this, &RibbonStateController::onIconVisibilityChanged);
    }
}

RibbonStateController::~RibbonStateController() = default;

void RibbonStateController::rebind(SessionManager *session)
{
    m_session = session;
    m_cachedHiddenItems = {};
}

void RibbonStateController::applyFromSession()
{
    if (!m_session) {
        m_cachedHiddenItems = {};
        return;
    }
    const QJsonObject ribbon = m_session->leftRibbonState();
    m_cachedHiddenItems = ribbon.value(QLatin1String(kHiddenItems)).toObject();

    if (!m_toolBar) return;
    const QStringList ids = m_toolBar->iconIdsInOrder();
    for (const auto &id : ids) {
        const bool hidden = m_cachedHiddenItems.value(id).toBool(false);
        m_toolBar->setIconVisible(id, !hidden);
    }
}

void RibbonStateController::onIconAdded(const QString &id)
{
    if (!m_toolBar) return;
    const bool hidden = m_cachedHiddenItems.value(id).toBool(false);
    if (hidden) m_toolBar->setIconVisible(id, false);
}

void RibbonStateController::onIconVisibilityChanged(const QString &id, bool visible)
{
    writeThrough(id, !visible);
}

void RibbonStateController::writeThrough(const QString &id, bool hidden)
{
    if (!m_session) return;
    QJsonObject ribbon = m_session->leftRibbonState();
    QJsonObject items = ribbon.value(QLatin1String(kHiddenItems)).toObject();
    if (hidden) {
        items.insert(id, true);
    } else {
        items.remove(id);
    }
    // Update the cache too, so late-arriving icons see consistent state.
    m_cachedHiddenItems = items;
    if (items.isEmpty()) {
        ribbon.remove(QLatin1String(kHiddenItems));
    } else {
        ribbon.insert(QLatin1String(kHiddenItems), items);
    }
    m_session->setLeftRibbonState(ribbon);
}

} // namespace Corbomite
```

- [ ] **Step 3: Wire into CMake**

In `src/CMakeLists.txt`, add the two sources next to `app/RibbonToolBar.cpp`:

```cmake
    app/RibbonToolBar.cpp
    app/RibbonStateController.cpp
```

- [ ] **Step 4: Build + run tests**

```bash
cmake --build build -j 10 --target tst_ribbonstatecontroller
cd build && ctest -R tst_ribbonstatecontroller --output-on-failure
```

Expected: all four tests pass.

- [ ] **Step 5: Commit**

```bash
git add src/app/RibbonStateController.h src/app/RibbonStateController.cpp \
        src/CMakeLists.txt \
        tests/dialogs/tst_ribbonstatecontroller.cpp \
        tests/dialogs/CMakeLists.txt
git commit -m "feat(app): RibbonStateController bridges RibbonToolBar and SessionManager

Applies workspace.json['left-ribbon'].hiddenItems to icon visibility on
vault open, writes visibility changes back via setLeftRibbonState().
Handles late-arriving icons (registered after applyFromSession) and
vault-switch rebind.

Part of docs/superpowers/plans/2026-04-20-ribbon-to-toolbar.md."
```

---

## Phase 4 — MainWindow integration and RibbonSlot deletion

### Task 4.1: Swap MainWindow to RibbonToolBar + controller

**Files:**
- Modify: `src/app/MainWindow.h`
- Modify: `src/app/MainWindow.cpp`

- [ ] **Step 1: Update the header**

In `src/app/MainWindow.h`:

Replace `class RibbonSlot;` (line 53) with:

```cpp
class RibbonToolBar;
class RibbonStateController;
```

Rename the private method declaration (line 96):

```cpp
    void setupRibbonToolBar();
```

Replace the field (line 185):

```cpp
    RibbonToolBar *m_ribbonToolBar = nullptr;
    RibbonStateController *m_ribbonState = nullptr;
```

- [ ] **Step 2: Update MainWindow.cpp includes**

Replace `#include "RibbonSlot.h"` (line 57) with:

```cpp
#include "RibbonToolBar.h"
#include "RibbonStateController.h"
```

- [ ] **Step 3: Replace `setupRibbon()` with `setupRibbonToolBar()`**

Find the call site in the constructor (around line 188):

```cpp
    setupRibbon();
```

Replace with:

```cpp
    setupRibbonToolBar();
```

Find the full `setupRibbon()` definition (lines 1546–1560) and replace it entirely with:

```cpp
void MainWindow::setupRibbonToolBar()
{
    m_ribbonToolBar = new RibbonToolBar(QStringLiteral("ribbonToolBar"), this);
    m_ribbonToolBar->setWindowTitle(i18n("Ribbon"));
    // Dock to the top toolbar area, after the main toolbar inserted by
    // KXMLGUI. addToolBar() places toolbars left-to-right in the same area
    // in insertion order, so calling this after KXMLGUI has added the
    // main toolbar lands us immediately to its right.
    addToolBar(Qt::TopToolBarArea, m_ribbonToolBar);

    // RibbonStateController is bound lazily in onVaultOpened when a
    // SessionManager exists. Until then, no icons are registered and the
    // toolbar is visibly empty — intentional per the design spec.
}
```

- [ ] **Step 4: Remove `prependToMainHLayout(m_ribbon)` call — this was the left-dock**

Searching for any remaining `m_ribbon` (not `m_ribbonToolBar`) reference: `prependToMainHLayout` was called inside the old `setupRibbon()` body which is now gone. Verify:

```bash
grep -n "m_ribbon\b\|RibbonSlot" src/app/MainWindow.cpp
```

Expected: no remaining references. If `prependToMainHLayout` exists only for the ribbon, also remove its declaration from `mdi/CorbomiteMDI.h`. (Keep the check; only delete if no other caller.)

```bash
grep -rn "prependToMainHLayout" src/ tests/
```

If only called from the now-deleted `setupRibbon`, remove the method declaration from `src/mdi/CorbomiteMDI.h` and its definition from `src/mdi/CorbomiteMDI.cpp`. If it is called elsewhere, leave it alone.

- [ ] **Step 5: Wire controller binding in `onVaultOpened` and `onVaultClosed`**

Find `onVaultOpened()` (around line 1720). Inside it, after the existing `m_sessionManager = new SessionManager(this);` + `m_sessionManager->load();` block (around lines 1873–1876), add:

```cpp
    if (!m_ribbonState) {
        m_ribbonState = new RibbonStateController(m_ribbonToolBar,
                                                   m_sessionManager, this);
    } else {
        m_ribbonState->rebind(m_sessionManager);
    }
    m_ribbonState->applyFromSession();
```

Find `onVaultClosed()` (use `grep -n "onVaultClosed" src/app/MainWindow.cpp` to locate). Add at the top of its body:

```cpp
    if (m_ribbonState) m_ribbonState->rebind(nullptr);
```

- [ ] **Step 6: Remove the three hardcoded ribbon entries**

They were inside the old `setupRibbon()` which is already deleted. Confirm:

```bash
grep -n "New note\|quickopen\|preferences-system-network" src/app/MainWindow.cpp | head
```

Any hits unrelated to ribbon registration (e.g. action-collection setup) are fine — ribbon-specific registrations are gone.

- [ ] **Step 7: Build the app**

```bash
cmake --build build -j 10 --target Corbomite
```

Expected: compiles cleanly. No references to `RibbonSlot` or `m_ribbon` remaining.

### Task 4.2: Delete `RibbonSlot` files and test

**Files:**
- Delete: `src/app/RibbonSlot.h`
- Delete: `src/app/RibbonSlot.cpp`
- Delete: `tests/dialogs/tst_ribbonslot.cpp`
- Modify: `src/CMakeLists.txt`
- Modify: `tests/dialogs/CMakeLists.txt`

- [ ] **Step 1: Delete the RibbonSlot source files**

```bash
git rm src/app/RibbonSlot.h src/app/RibbonSlot.cpp tests/dialogs/tst_ribbonslot.cpp
```

- [ ] **Step 2: Remove from `src/CMakeLists.txt`**

Find and delete this line from the `CorbomiteApp` source list:

```cmake
    app/RibbonSlot.cpp
```

- [ ] **Step 3: Remove from `tests/dialogs/CMakeLists.txt`**

Delete the four lines:

```cmake
add_executable(tst_ribbonslot tst_ribbonslot.cpp)
add_test(NAME tst_ribbonslot COMMAND tst_ribbonslot)
target_link_libraries(tst_ribbonslot PRIVATE Qt6::Test CorbomiteApp)
set_tests_properties(tst_ribbonslot PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
```

- [ ] **Step 4: Rebuild from a clean configure (stale autogen artefacts)**

Because sources and headers changed names, stale AUTOMOC files can linger:

```bash
cmake --build build -j 10
```

If linker errors mention `RibbonSlot` or `moc_RibbonSlot`, delete the autogen directory and rebuild:

```bash
rm -rf build/src/CMakeFiles/CorbomiteApp_autogen
cmake --build build -j 10
```

- [ ] **Step 5: Run the full suite**

```bash
cd build && ctest --output-on-failure -j 10
```

Expected: all tests pass. No `tst_ribbonslot` in the run; `tst_ribbontoolbar` and `tst_ribbonstatecontroller` both pass.

- [ ] **Step 6: Commit**

```bash
git add -A src/app/ src/CMakeLists.txt tests/dialogs/ tests/dialogs/CMakeLists.txt \
          src/mdi/
git commit -m "refactor(app): replace RibbonSlot with top-docked RibbonToolBar

MainWindow now owns a RibbonToolBar (KToolBar subclass) docked in
Qt::TopToolBarArea after the main toolbar, plus a RibbonStateController
bound per-vault on onVaultOpened. The three hardcoded legacy entries
(New note / Quick switcher / Graph view) are gone; those actions remain
available via the main toolbar, command palette, and shortcuts.

RibbonSlot and tst_ribbonslot deleted.

Part of docs/superpowers/plans/2026-04-20-ribbon-to-toolbar.md."
```

---

## Phase 5 — Backlog note

### Task 5.1: Record the deferred UX items

**Files:**
- Modify: `docs/backlog.md`

- [ ] **Step 1: Append a backlog entry**

Find the appropriate section (UI ergonomics, or a generic "deferred" section — use `grep -n "^## \|^### " docs/backlog.md` to survey headings). Append:

```markdown
### Ribbon-style toolbar micro-UX experiments

- **Origin:** 2026-04-20 ribbon-to-toolbar refactor (docs/superpowers/plans/2026-04-20-ribbon-to-toolbar.md, docs/superpowers/specs/2026-04-20-ribbon-to-toolbar-design.md)
- **Status:** Deferred; not tied to a cluster.
- **Summary:** In the switch from `RibbonSlot` to the top-docked `RibbonToolBar`, two Obsidian-ribbon UX affordances were dropped: (1) in-place drag-reorder of individual icons, (2) right-click → hide *this* icon. Users reorder and hide via the standard KDE *Settings → Configure Toolbars* dialog, and visibility persists via `workspace.json['left-ribbon'].hiddenItems` when toggled through the controller.
- **Action if revived:** Subclass `KToolBar` (in `RibbonToolBar`) with an event filter for middle-click / right-click → add a per-item context menu exposing "Hide this icon", and implement drag-drop reorder via `mousePressEvent`/`mouseMoveEvent` handlers operating on the underlying `QAction` list. Remember that `QJsonObject` does not preserve key insertion order, so a drag-reorder feature also needs a separate ordering array (e.g. `['left-ribbon'].order: string[]`) — deviating from Obsidian's on-disk schema.
- **Rationale for deferral:** Not worth the subclass complexity until a user complaint lands; *Configure Toolbars* covers the 99% case.
```

- [ ] **Step 2: Commit**

```bash
git add docs/backlog.md
git commit -m "docs(backlog): ribbon micro-UX experiments (drag-reorder, per-item hide)

Records the two Obsidian affordances dropped during the ribbon-to-toolbar
refactor, with an action sketch if they are ever revived.

Closes the work under docs/superpowers/plans/2026-04-20-ribbon-to-toolbar.md."
```

---

## Verification

After all phases:

- [ ] **Full test suite passes**

```bash
cd build && ctest --output-on-failure -j 10
```

- [ ] **App launches, ribbon toolbar is visible top-right of main toolbar, starts empty**

```bash
./build/Corbomite --test-vault testvaults/starter-vault
```

Manual inspection: open a vault; confirm a second toolbar is present top-right of the main toolbar; confirm it is empty (no legacy New note / Quick switcher / Graph view entries); confirm it responds to standard right-click toolbar-area context menu (Lock Toolbars, Text Position, Icon Size).

- [ ] **Per-vault persistence works end-to-end via a manual smoke test**

With no plugin icons available yet, the toolbar is always empty. Persistence is exercised only by Phase 3's unit tests — that is sufficient coverage until a first ribbon icon consumer arrives.

---

## Spec coverage self-check

| Spec section | Plan coverage |
|---|---|
| Two-toolbar architecture | Phase 4 (Task 4.1 Step 3) |
| Top-right default position | Phase 4 (`addToolBar(Qt::TopToolBarArea)`) |
| `RibbonToolBar` API signature | Phase 2 (Task 2.2 Step 1) |
| Id-keyed identity, Obsidian collision quirk | Phase 2 (Task 2.1 + 2.2) |
| `RibbonStateController` responsibilities | Phase 3 (Task 3.1 + 3.2) |
| Vault-lifecycle scoping (rebind on open/close) | Phase 4 (Task 4.1 Step 5), Phase 3 rebind() test |
| `SessionManager::{leftRibbonState, setLeftRibbonState}` | Phase 1 |
| Unknown-key round-trip preservation (pre-existing Obsidian content) | Phase 1 (Task 1.1 Step 1, test #17) |
| Delete `RibbonSlot` + `tst_ribbonslot` | Phase 4 (Task 4.2) |
| Drop 3 hardcoded legacy entries | Phase 4 (Task 4.1 Step 3) |
| Backlog note for drag-reorder + per-item hide | Phase 5 |
| Schema: `{"left-ribbon": {"hiddenItems": {...}}}` at root | Phase 1 (Task 1.2 Step 2) |
