# Cluster S — Bookmarks core plugin — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ship `src/plugins/bookmarks/` — an Obsidian-compatible Bookmarks internal plugin with `.obsidian/bookmarks.json` round-trip, right-dock tree panel, seven `bookmarks:*` commands, and a "Bookmark…" modal that flips Cluster R's disabled menu slot live.

**Architecture:** Single `KPluginFactory` `.so` at `src/plugins/bookmarks/` built via `corbomite_add_plugin()`. `BookmarksStore` owns the in-memory item tree + atomic JSON round-trip (unknown-key preservation, Cluster B idiom). `BookmarksModel : QAbstractItemModel` backs a `QTreeView` inside `BookmarksView`. `BookmarksPlugin::onLoad` registers the view type + 7 commands via the PluginContext proxies. A `BookmarkModal` dialog (name + group picker) is invoked both from the plugin's `+` header button and from Cluster R's `EditableFileView` "Bookmark" menu slot (currently disabled placeholder at `libs/core/src/EditableFileView.cpp:98-104`).

**Tech Stack:** C++20, Qt6 (QTreeView, QAbstractItemModel, QDialog, QJson*), KF6 (KPluginFactory, KLocalizedString, KIconTheme). `Corbomite::Core` (Command, ViewRegistrar, WorkspaceController), `Corbomite::Vault` (Plugin, PluginContext, Vault::readConfigJson/writeConfigJson). GPLv3 headers on all files.

---

## Pre-flight context for the implementing engineer

**Read first:**
- Spec: `docs/superpowers/specs/2026-04-19-cluster-s-bookmarks-design.md` — authoritative design.
- Addendum: `docs/obsidian-audit/addenda/2026-04-19-bookmarks-core-plugin.md` — Obsidian on-disk schema.
- Pattern reference plugin: `src/plugins/backlinks/` (5-file layout, `corbomite_add_plugin()` usage, metadata.json.in).
- Richer reference plugin (with more state): `src/plugins/search/`.
- Plugin API: `libs/vault/include/corbomite/vault/Plugin.h`, `PluginContext.h`; `libs/core/include/corbomite/core/Command.h`, `proxies/CommandRegistrar.h`, `proxies/ViewRegistrar.h`, `proxies/WorkspaceController.h`.

**Plugin id:** `corbomite-bookmarks` (matches filesystem/KPlugin convention of existing plugins). Command-id namespace as emitted by `CommandRegistrar::addCommand` therefore becomes `corbomite-bookmarks:<localId>` — **not** `bookmarks:<localId>`. This diverges from Obsidian's exact hotkeys.json ids. Resolution: register the seven commands via the raw `CommandRegistry *` (obtained through `PluginContext::setCoreServices`? No — plugin can't reach it directly) — instead, set each `Command::id = "bookmarks:bookmark-current-file"` **after** `addCommand` would prefix it, by bypassing the prefix. The cleanest path: add a **single** new overload `CommandRegistrar::addCommandRaw(Command &cmd)` that does not prefix, and call it from this plugin only. Document this in the cpp file as an Obsidian-parity exception. (This cost is acceptable; a wider Obsidian-id CommandRegistry mirror is tracked in `backlog.md` §3 as a Cluster V follow-up.)

**Vault on-disk API:** `Corbomite::Vault::readConfigJson(const QString &name) → QJsonValue` and `writeConfigJson(name, value) → bool` at `libs/vault/include/corbomite/vault/Vault.h:72-73`. `name` is vault-relative starting under `.obsidian/` — pass `"bookmarks.json"` (see how existing plugins read `core-plugins.json`).

**Session state:** `Plugin::saveSessionState(QObject *view) const` + `loadSessionState(QObject *view, QJsonObject)` at `Plugin.h:64-70`. Use for group-expand/collapse persistence (addendum §3 says this belongs in plugin `data.json`, not bookmarks.json — we route it through the session-state hook, which the host persists under `_corbomite.plugins.<pluginId>` in workspace.json; that's equivalent for our purposes and matches FileExplorer).

**Command callback signature:** `Command` variant at `libs/core/include/corbomite/core/Command.h:38-54`. Most bookmarks commands need availability gating ("active view is markdown / search / graph") — use `checkCallback` for those, plain `callback` for `bookmark-current-file`, `bookmark-all-tabs`, `open`.

**Cluster R integration hook:** `libs/core/src/EditableFileView.cpp:98-104` currently adds a disabled `QAction` with tooltip "Requires Cluster S: Bookmarks plugin". Task 3.3 flips this to a live action.

**Do not** invent `MarkdownView::ensureBlockIdAtCursor()` in this plan — block-id auto-insertion is deferred per spec §2.5 fallback path. The `bookmark-current-block` command shows a Notice ("Move cursor into a block with a block id to bookmark it.") when the block under the cursor has no id. Add the auto-insert helper as a backlog item.

**Build:** `cmake --build build -j 10`. Single test: `cd build && ctest -R tst_bookmarks --output-on-failure -j 10`.

---

## File Structure

**New files — all under `src/plugins/bookmarks/`:**

| File | Responsibility |
|---|---|
| `BookmarkItem.h` | Plain struct: `type`, `path`, `subpath`, `title`, `query`, `options`, `ctime`, `children`, `unknownKeys`. No methods. |
| `BookmarksStore.h/.cpp` | Owns root `QList<BookmarkItem>`. JSON load/save (atomic via `Vault::writeConfigJson`). CRUD: `addBookmark`, `removeBookmark`, `moveBookmark`, `find`. Emits `changed()`. Debounced-save timer wiring belongs to the *plugin*, not the store. |
| `BookmarksModel.h/.cpp` | `QAbstractItemModel` adapter over `BookmarksStore`. Data roles: Display, Decoration, ToolTip, custom `BookmarksTypeRole`. Supports drag-drop via internal MIME `application/x-corbomite-bookmarks-drag`. |
| `BookmarksView.h/.cpp` | `QWidget` (not `ItemView` — follow Backlinks precedent: plain QWidget with QLabel header + QTreeView). Handles click routing to `WorkspaceController::openLinkText`. Right-click context menu. `+` header button. |
| `BookmarkModal.h/.cpp` | `QDialog` with `QLineEdit` name + `QComboBox` group picker + QDialogButtonBox. Static helper `runFor(BookmarkItem inferred, BookmarksStore *store, QWidget *parent) → bool` that opens the modal and, on Accept, commits via `store->addBookmark`. |
| `BookmarksPlugin.h/.cpp` | KPluginFactory entry. `onLoad` reads bookmarks.json → BookmarksStore, registers view, registers 7 commands, wires debounced save. `createView` returns a new `BookmarksView`. `saveSessionState`/`loadSessionState` serialize tree expand state. |
| `metadata.json.in` | KPlugin metadata. Id `corbomite-bookmarks`, EnabledByDefault true, DockArea right, DockIcon `bookmark-new`, permissions: `vault.read`, `vault.write`, `vault.events`, `workspace`, `ui.views`, `ui.commands`. |
| `CMakeLists.txt` | Single `corbomite_add_plugin()` call listing all 6 `.cpp` files. `TRUSTED`. Links `Qt6::Widgets`, `KF6::I18n`, `Corbomite::Core`, `Corbomite::Storage`, `Corbomite::Vault`. Adds `tests/` subdirectory under `BUILD_TESTING`. |
| `tests/CMakeLists.txt` | Three test executables: `tst_bookmarks_store`, `tst_bookmarks_model`, `tst_bookmarks_commands`. Mirror `src/plugins/backlinks/tests/CMakeLists.txt` scaffolding (AUTOMOC_MOC_OPTIONS, QT_QPA_PLATFORM=offscreen). |
| `tests/tst_bookmarks_store.cpp` | JSON round-trip including unknown-key preservation, CRUD, groups. |
| `tests/tst_bookmarks_model.cpp` | Tree indices, data roles, drag-drop MIME encode/decode. |
| `tests/tst_bookmarks_commands.cpp` | Each of the 7 commands produces the expected store delta. |

**Modified files:**

| File | Change |
|---|---|
| `src/CMakeLists.txt` | `add_subdirectory(plugins/bookmarks)` after `plugins/graph-view` (line ~105). |
| `libs/core/src/EditableFileView.cpp` | Replace the disabled `bookmarkAct` placeholder (lines 98-104) with a live action that invokes `BookmarkModal::runFor(...)`-equivalent via a callback the view surfaces (exact hook: add `std::function<void(TFile *, View *)> m_bookmarkCallback` paralleling `m_renameCallback`; the host wires it in `MainWindow` similarly to how rename is wired). |
| `src/app/MainWindow.cpp` | Wire `EditableFileView::setBookmarkCallback(...)` (new) to dispatch `bookmarks:bookmark-current-file-with-modal` via CommandRegistry. Add slug entry `{"bookmarks", "corbomite-bookmarks"}` to the `slugToPluginId` map at line ~1357. |
| `libs/core/include/corbomite/core/EditableFileView.h` | Add `setBookmarkCallback` + `using BookmarkCallback = std::function<void(TFile *, View *)>`. |
| `libs/core/include/corbomite/core/proxies/CommandRegistrar.h` + `.cpp` | Add `void addCommandRaw(Command &cmd)` that skips the `pluginId:` prefix — used only for Obsidian-id parity in bookmarks plugin. |
| `docs/PROJECT-STATE.md` | On cluster-close, move Cluster S from "Plan-needed" to "Done". |
| `docs/backlog.md` | Add two followups: (a) `MarkdownView::ensureBlockIdAtCursor()` for block-bookmark auto-insertion; (b) deferred "Remove all broken bookmarks" bulk-clean context menu. |

---

## Task Group 1 — Store + model (foundation, no plugin shell yet)

### Task 1.1: Create `BookmarkItem.h`

**Files:**
- Create: `src/plugins/bookmarks/BookmarkItem.h`

- [ ] **Step 1: Write the header**

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>

namespace Corbomite::Bookmarks {

/// One entry in the bookmarks tree. Mirrors Obsidian's bookmarks.json item
/// shape (see docs/obsidian-audit/addenda/2026-04-19-bookmarks-core-plugin.md §2).
/// `type` is one of: "file", "folder", "search", "graph", "group".
/// Unknown types are preserved on round-trip via unknownKeys + unknownType.
struct BookmarkItem
{
    QString type;
    QString path;         ///< file/folder/heading/block (path#subpath for heading/block)
    QString subpath;      ///< "#Heading" or "#^blockId"; redundant with path suffix
    QString title;        ///< user override; empty = infer
    QString query;        ///< search only
    QJsonObject options;  ///< graph only
    qint64 ctime = 0;
    QList<BookmarkItem> children;  ///< group only
    QJsonObject unknownKeys;       ///< preserved round-trip surplus
};

} // namespace Corbomite::Bookmarks
```

- [ ] **Step 2: Commit**

```bash
git add src/plugins/bookmarks/BookmarkItem.h
git commit -m "feat(bookmarks): add BookmarkItem struct (Cluster S task 1.1)"
```

---

### Task 1.2: `BookmarksStore` — JSON round-trip (test first)

**Files:**
- Create: `src/plugins/bookmarks/BookmarksStore.h`
- Create: `src/plugins/bookmarks/BookmarksStore.cpp`
- Create: `src/plugins/bookmarks/tests/tst_bookmarks_store.cpp`
- Create: `src/plugins/bookmarks/tests/CMakeLists.txt`
- Create: `src/plugins/bookmarks/CMakeLists.txt`
- Modify: `src/CMakeLists.txt` (add `add_subdirectory(plugins/bookmarks)`)

- [ ] **Step 1: Write failing test — empty-input round-trip + unknown-key preservation**

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "../BookmarkItem.h"
#include "../BookmarksStore.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>

using namespace Corbomite::Bookmarks;

class TstBookmarksStore : public QObject
{
    Q_OBJECT
private slots:
    void emptyJsonLoadsAsEmptyStore();
    void fileItemRoundTrips();
    void groupWithChildrenRoundTrips();
    void unknownTypeIsPreserved();
    void unknownKeysOnKnownTypeArePreserved();
    void addBookmarkAppendsAtRoot();
    void removeBookmarkByPath();
};

void TstBookmarksStore::emptyJsonLoadsAsEmptyStore()
{
    BookmarksStore store;
    QVERIFY(store.loadFromJson(QJsonDocument::fromJson("{\"items\":[]}").object()));
    QCOMPARE(store.rootItems().size(), 0);
}

void TstBookmarksStore::fileItemRoundTrips()
{
    const QByteArray raw = R"({"items":[{"type":"file","ctime":1713544200000,"path":"notes/foo.md","title":"Foo"}]})";
    BookmarksStore store;
    QVERIFY(store.loadFromJson(QJsonDocument::fromJson(raw).object()));
    QCOMPARE(store.rootItems().size(), 1);
    const auto &item = store.rootItems().at(0);
    QCOMPARE(item.type, QStringLiteral("file"));
    QCOMPARE(item.path, QStringLiteral("notes/foo.md"));
    QCOMPARE(item.title, QStringLiteral("Foo"));
    QCOMPARE(item.ctime, 1713544200000LL);

    const QJsonObject out = store.toJson();
    QCOMPARE(QJsonDocument(out).toJson(QJsonDocument::Compact), raw);
}

void TstBookmarksStore::groupWithChildrenRoundTrips()
{
    const QByteArray raw = R"({"items":[{"type":"group","ctime":1,"title":"Reading","items":[{"type":"file","ctime":2,"path":"a.md"}]}]})";
    BookmarksStore store;
    QVERIFY(store.loadFromJson(QJsonDocument::fromJson(raw).object()));
    QCOMPARE(store.rootItems().size(), 1);
    QCOMPARE(store.rootItems().at(0).children.size(), 1);
    QCOMPARE(store.rootItems().at(0).children.at(0).path, QStringLiteral("a.md"));

    QCOMPARE(QJsonDocument(store.toJson()).toJson(QJsonDocument::Compact), raw);
}

void TstBookmarksStore::unknownTypeIsPreserved()
{
    const QByteArray raw = R"({"items":[{"type":"future-kind","ctime":1,"customField":"hello"}]})";
    BookmarksStore store;
    QVERIFY(store.loadFromJson(QJsonDocument::fromJson(raw).object()));
    QCOMPARE(store.rootItems().size(), 1);
    QCOMPARE(store.rootItems().at(0).type, QStringLiteral("future-kind"));
    QCOMPARE(QJsonDocument(store.toJson()).toJson(QJsonDocument::Compact), raw);
}

void TstBookmarksStore::unknownKeysOnKnownTypeArePreserved()
{
    const QByteArray raw = R"({"items":[{"type":"file","ctime":1,"path":"x.md","futureField":42}]})";
    BookmarksStore store;
    QVERIFY(store.loadFromJson(QJsonDocument::fromJson(raw).object()));
    QCOMPARE(QJsonDocument(store.toJson()).toJson(QJsonDocument::Compact), raw);
}

void TstBookmarksStore::addBookmarkAppendsAtRoot()
{
    BookmarksStore store;
    BookmarkItem item;
    item.type = QStringLiteral("file");
    item.path = QStringLiteral("a.md");
    item.ctime = 100;
    store.addBookmark(item, {});
    QCOMPARE(store.rootItems().size(), 1);
    QCOMPARE(store.rootItems().at(0).path, QStringLiteral("a.md"));
}

void TstBookmarksStore::removeBookmarkByPath()
{
    BookmarksStore store;
    BookmarkItem a; a.type = "file"; a.path = "a.md"; a.ctime = 1;
    BookmarkItem b; b.type = "file"; b.path = "b.md"; b.ctime = 2;
    store.addBookmark(a, {});
    store.addBookmark(b, {});
    QCOMPARE(store.rootItems().size(), 2);
    store.removeBookmark({QStringLiteral("0")});
    QCOMPARE(store.rootItems().size(), 1);
    QCOMPARE(store.rootItems().at(0).path, QStringLiteral("b.md"));
}

QTEST_MAIN(TstBookmarksStore)
#include "tst_bookmarks_store.moc"
```

- [ ] **Step 2: Write `BookmarksStore.h` (API surface)**

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "BookmarkItem.h"

#include <QJsonObject>
#include <QObject>
#include <QStringList>

namespace Corbomite { class Vault; }

namespace Corbomite::Bookmarks {

/// In-memory model of bookmarks.json. Load/save is stateless on the store
/// (plugin shell drives debounced writes). `itemPath` is a QStringList of
/// integer-as-string indices walking the tree (e.g. ["0","2"] = root->0th
/// child->2nd child).
class BookmarksStore : public QObject
{
    Q_OBJECT
public:
    explicit BookmarksStore(QObject *parent = nullptr);
    ~BookmarksStore() override;

    bool loadFromJson(const QJsonObject &obj);
    QJsonObject toJson() const;

    const QList<BookmarkItem> &rootItems() const { return m_items; }

    void addBookmark(BookmarkItem item, const QStringList &groupPath = {});
    bool removeBookmark(const QStringList &itemPath);
    bool moveBookmark(const QStringList &fromPath,
                      const QStringList &toParentPath,
                      int insertIndex);

    /// Resolve a tree path to a pointer into m_items. Nullptr if invalid.
    BookmarkItem *find(const QStringList &itemPath);

signals:
    void changed();

private:
    QList<BookmarkItem> m_items;

    static BookmarkItem parseItem(const QJsonObject &obj);
    static QJsonObject  itemToJson(const BookmarkItem &item);
};

} // namespace Corbomite::Bookmarks
```

- [ ] **Step 3: Write `BookmarksStore.cpp` (minimal implementation)**

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "BookmarksStore.h"

#include <QJsonArray>

namespace Corbomite::Bookmarks {

// Known canonical keys per item type — anything else rolls into unknownKeys.
static const QSet<QString> &knownKeys()
{
    static const QSet<QString> ks = {
        QStringLiteral("type"), QStringLiteral("ctime"),
        QStringLiteral("path"), QStringLiteral("subpath"),
        QStringLiteral("title"), QStringLiteral("query"),
        QStringLiteral("options"), QStringLiteral("items"),
    };
    return ks;
}

BookmarksStore::BookmarksStore(QObject *parent) : QObject(parent) {}
BookmarksStore::~BookmarksStore() = default;

BookmarkItem BookmarksStore::parseItem(const QJsonObject &obj)
{
    BookmarkItem item;
    item.type    = obj.value(QStringLiteral("type")).toString();
    item.ctime   = obj.value(QStringLiteral("ctime")).toVariant().toLongLong();
    item.path    = obj.value(QStringLiteral("path")).toString();
    item.subpath = obj.value(QStringLiteral("subpath")).toString();
    item.title   = obj.value(QStringLiteral("title")).toString();
    item.query   = obj.value(QStringLiteral("query")).toString();
    item.options = obj.value(QStringLiteral("options")).toObject();

    if (obj.contains(QStringLiteral("items"))) {
        const QJsonArray arr = obj.value(QStringLiteral("items")).toArray();
        for (const auto &v : arr)
            item.children.append(parseItem(v.toObject()));
    }

    for (auto it = obj.begin(); it != obj.end(); ++it) {
        if (!knownKeys().contains(it.key()))
            item.unknownKeys.insert(it.key(), it.value());
    }
    return item;
}

QJsonObject BookmarksStore::itemToJson(const BookmarkItem &item)
{
    QJsonObject obj;
    obj.insert(QStringLiteral("type"), item.type);
    obj.insert(QStringLiteral("ctime"), item.ctime);
    if (!item.path.isEmpty())    obj.insert(QStringLiteral("path"), item.path);
    if (!item.subpath.isEmpty()) obj.insert(QStringLiteral("subpath"), item.subpath);
    if (!item.title.isEmpty())   obj.insert(QStringLiteral("title"), item.title);
    if (!item.query.isEmpty())   obj.insert(QStringLiteral("query"), item.query);
    if (!item.options.isEmpty()) obj.insert(QStringLiteral("options"), item.options);

    if (!item.children.isEmpty()) {
        QJsonArray arr;
        for (const auto &child : item.children)
            arr.append(itemToJson(child));
        obj.insert(QStringLiteral("items"), arr);
    }

    for (auto it = item.unknownKeys.begin(); it != item.unknownKeys.end(); ++it)
        obj.insert(it.key(), it.value());

    return obj;
}

bool BookmarksStore::loadFromJson(const QJsonObject &obj)
{
    m_items.clear();
    const QJsonArray arr = obj.value(QStringLiteral("items")).toArray();
    for (const auto &v : arr)
        m_items.append(parseItem(v.toObject()));
    emit changed();
    return true;
}

QJsonObject BookmarksStore::toJson() const
{
    QJsonArray arr;
    for (const auto &item : m_items)
        arr.append(itemToJson(item));
    QJsonObject obj;
    obj.insert(QStringLiteral("items"), arr);
    return obj;
}

void BookmarksStore::addBookmark(BookmarkItem item, const QStringList &groupPath)
{
    if (groupPath.isEmpty()) {
        m_items.append(std::move(item));
    } else {
        if (auto *parent = find(groupPath))
            parent->children.append(std::move(item));
        else
            m_items.append(std::move(item));
    }
    emit changed();
}

bool BookmarksStore::removeBookmark(const QStringList &itemPath)
{
    if (itemPath.isEmpty()) return false;
    QList<BookmarkItem> *list = &m_items;
    for (int i = 0; i < itemPath.size() - 1; ++i) {
        bool ok = false;
        const int idx = itemPath.at(i).toInt(&ok);
        if (!ok || idx < 0 || idx >= list->size()) return false;
        list = &((*list)[idx].children);
    }
    bool ok = false;
    const int idx = itemPath.last().toInt(&ok);
    if (!ok || idx < 0 || idx >= list->size()) return false;
    list->removeAt(idx);
    emit changed();
    return true;
}

bool BookmarksStore::moveBookmark(const QStringList &fromPath,
                                  const QStringList &toParentPath,
                                  int insertIndex)
{
    BookmarkItem *src = find(fromPath);
    if (!src) return false;
    BookmarkItem copy = *src;
    if (!removeBookmark(fromPath)) return false;

    QList<BookmarkItem> *dest = &m_items;
    if (!toParentPath.isEmpty()) {
        BookmarkItem *parent = find(toParentPath);
        if (!parent) { m_items.append(std::move(copy)); emit changed(); return true; }
        dest = &parent->children;
    }
    if (insertIndex < 0 || insertIndex > dest->size()) insertIndex = dest->size();
    dest->insert(insertIndex, std::move(copy));
    emit changed();
    return true;
}

BookmarkItem *BookmarksStore::find(const QStringList &itemPath)
{
    if (itemPath.isEmpty()) return nullptr;
    QList<BookmarkItem> *list = &m_items;
    BookmarkItem *current = nullptr;
    for (int i = 0; i < itemPath.size(); ++i) {
        bool ok = false;
        const int idx = itemPath.at(i).toInt(&ok);
        if (!ok || idx < 0 || idx >= list->size()) return nullptr;
        current = &(*list)[idx];
        list = &current->children;
    }
    return current;
}

} // namespace Corbomite::Bookmarks
```

- [ ] **Step 4: Write plugin `CMakeLists.txt`**

```cmake
corbomite_add_plugin(corbomite-bookmarks
    METADATA_TEMPLATE "${CMAKE_CURRENT_SOURCE_DIR}/metadata.json.in"
    SOURCES
        BookmarksPlugin.cpp
        BookmarksView.cpp
        BookmarksModel.cpp
        BookmarksStore.cpp
        BookmarkModal.cpp
    TRUSTED
    LINK_LIBRARIES
        Qt6::Widgets
        KF6::I18n
        Corbomite::Core
        Corbomite::Storage
        Corbomite::Vault)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
```

Note: this CMakeLists references files we'll create in later tasks. To keep the build green while only BookmarksStore exists, temporarily list only `BookmarksStore.cpp` in SOURCES and empty-stub the factory. Cleaner: defer adding this file (and the `add_subdirectory` line in `src/CMakeLists.txt`) until Task 2.1 and build the store library solely via the test target for now.

**Chosen path:** test target compiles `BookmarksStore.cpp` directly (no plugin target yet). Update:

- [ ] **Step 5: Write `tests/CMakeLists.txt` (store-only first)**

```cmake
cmake_minimum_required(VERSION 3.19)
project(Corbomite_BookmarksPluginTests LANGUAGES CXX)
enable_testing()
find_package(Qt6 REQUIRED COMPONENTS Test Widgets)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_AUTOMOC ON)

add_executable(tst_bookmarks_store tst_bookmarks_store.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../BookmarksStore.cpp)
target_include_directories(tst_bookmarks_store PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..)
add_test(NAME tst_bookmarks_store COMMAND tst_bookmarks_store)
target_link_libraries(tst_bookmarks_store PRIVATE
    Qt6::Test Qt6::Widgets
    Corbomite::Core Corbomite::Vault
    KF6::I18n)
set_tests_properties(tst_bookmarks_store PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
```

(Later tasks add `tst_bookmarks_model` and `tst_bookmarks_commands` executables alongside.)

- [ ] **Step 6: Temporarily hook the store tests into the main build**

Add to `src/plugins/bookmarks/CMakeLists.txt`:

```cmake
# Phase-in: the plugin target lands in Task 2.1. Until then, only build tests.
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
```

Add to `src/CMakeLists.txt` after line 104 (`add_subdirectory(plugins/graph-view)`):

```cmake
add_subdirectory(plugins/bookmarks)
```

- [ ] **Step 7: Configure + build + run the failing test**

```bash
cmake -B build -DCORBOMITE_DEV_BUILD=ON
cmake --build build -j 10 --target tst_bookmarks_store
cd build && ctest -R tst_bookmarks_store --output-on-failure
```

Expected: all tests PASS (implementation is already written above).

- [ ] **Step 8: Commit**

```bash
git add src/plugins/bookmarks/ src/CMakeLists.txt
git commit -m "feat(bookmarks): BookmarksStore with JSON round-trip + unknown-key preservation (Cluster S task 1.2)"
```

---

### Task 1.3: `BookmarksModel` — QAbstractItemModel adapter

**Files:**
- Create: `src/plugins/bookmarks/BookmarksModel.h`
- Create: `src/plugins/bookmarks/BookmarksModel.cpp`
- Create: `src/plugins/bookmarks/tests/tst_bookmarks_model.cpp`
- Modify: `src/plugins/bookmarks/tests/CMakeLists.txt`

- [ ] **Step 1: Write failing test**

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "../BookmarkItem.h"
#include "../BookmarksModel.h"
#include "../BookmarksStore.h"

#include <QMimeData>
#include <QTest>

using namespace Corbomite::Bookmarks;

class TstBookmarksModel : public QObject
{
    Q_OBJECT
private slots:
    void init();
    void cleanup();
    void rowCountMatchesStoreDepth();
    void displayRoleInfersFromFilePath();
    void displayRoleUsesTitleOverride();
    void decorationRoleDiffersByType();
    void typeRoleExposesRawType();
    void dragMimeRoundTripsTreePath();

private:
    BookmarksStore *m_store = nullptr;
    BookmarksModel *m_model = nullptr;
};

void TstBookmarksModel::init()
{
    m_store = new BookmarksStore;
    BookmarkItem a; a.type = "file"; a.path = "notes/foo.md"; a.ctime = 1;
    BookmarkItem b; b.type = "group"; b.title = "Reading"; b.ctime = 2;
    BookmarkItem c; c.type = "file"; c.path = "a.md"; c.title = "Alpha"; c.ctime = 3;
    b.children.append(c);
    m_store->addBookmark(a, {});
    m_store->addBookmark(b, {});
    m_model = new BookmarksModel(m_store);
}

void TstBookmarksModel::cleanup()
{
    delete m_model; m_model = nullptr;
    delete m_store; m_store = nullptr;
}

void TstBookmarksModel::rowCountMatchesStoreDepth()
{
    QCOMPARE(m_model->rowCount(QModelIndex()), 2);
    const QModelIndex group = m_model->index(1, 0, QModelIndex());
    QCOMPARE(m_model->rowCount(group), 1);
}

void TstBookmarksModel::displayRoleInfersFromFilePath()
{
    const QModelIndex idx = m_model->index(0, 0, QModelIndex());
    QCOMPARE(idx.data(Qt::DisplayRole).toString(), QStringLiteral("foo"));
}

void TstBookmarksModel::displayRoleUsesTitleOverride()
{
    const QModelIndex group = m_model->index(1, 0, QModelIndex());
    const QModelIndex child = m_model->index(0, 0, group);
    QCOMPARE(child.data(Qt::DisplayRole).toString(), QStringLiteral("Alpha"));
}

void TstBookmarksModel::decorationRoleDiffersByType()
{
    const QModelIndex file = m_model->index(0, 0, QModelIndex());
    const QModelIndex group = m_model->index(1, 0, QModelIndex());
    QVERIFY(file.data(Qt::DecorationRole).isValid());
    QVERIFY(group.data(Qt::DecorationRole).isValid());
}

void TstBookmarksModel::typeRoleExposesRawType()
{
    const QModelIndex file = m_model->index(0, 0, QModelIndex());
    QCOMPARE(file.data(BookmarksModel::BookmarksTypeRole).toString(),
             QStringLiteral("file"));
}

void TstBookmarksModel::dragMimeRoundTripsTreePath()
{
    const QModelIndex file = m_model->index(0, 0, QModelIndex());
    QModelIndexList indices; indices << file;
    QMimeData *mime = m_model->mimeData(indices);
    QVERIFY(mime->hasFormat(QStringLiteral("application/x-corbomite-bookmarks-drag")));
    const QByteArray payload = mime->data(
        QStringLiteral("application/x-corbomite-bookmarks-drag"));
    QCOMPARE(QString::fromUtf8(payload), QStringLiteral("0"));
    delete mime;
}

QTEST_MAIN(TstBookmarksModel)
#include "tst_bookmarks_model.moc"
```

- [ ] **Step 2: Write `BookmarksModel.h`**

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QAbstractItemModel>

namespace Corbomite::Bookmarks {

class BookmarksStore;
struct BookmarkItem;

class BookmarksModel : public QAbstractItemModel
{
    Q_OBJECT
public:
    enum CustomRoles {
        BookmarksTypeRole = Qt::UserRole + 1,  ///< raw `type` string
        BookmarksPathRole = Qt::UserRole + 2,  ///< full `path` (or query)
    };

    explicit BookmarksModel(BookmarksStore *store, QObject *parent = nullptr);
    ~BookmarksModel() override;

    // QAbstractItemModel
    QModelIndex index(int row, int column, const QModelIndex &parent) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    int rowCount(const QModelIndex &parent) const override;
    int columnCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;

    Qt::ItemFlags flags(const QModelIndex &index) const override;
    Qt::DropActions supportedDropActions() const override;
    QStringList mimeTypes() const override;
    QMimeData *mimeData(const QModelIndexList &indexes) const override;
    bool dropMimeData(const QMimeData *data, Qt::DropAction action,
                      int row, int column, const QModelIndex &parent) override;

    /// Translate a QModelIndex to a store tree-path (list of int-as-string).
    QStringList pathOf(const QModelIndex &index) const;

private slots:
    void onStoreChanged();

private:
    BookmarksStore *m_store = nullptr;

    const BookmarkItem *itemForIndex(const QModelIndex &index) const;
};

} // namespace Corbomite::Bookmarks
```

- [ ] **Step 3: Write `BookmarksModel.cpp`**

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "BookmarksModel.h"

#include "BookmarkItem.h"
#include "BookmarksStore.h"

#include <QFileInfo>
#include <QIcon>
#include <QMimeData>

namespace Corbomite::Bookmarks {

BookmarksModel::BookmarksModel(BookmarksStore *store, QObject *parent)
    : QAbstractItemModel(parent), m_store(store)
{
    if (m_store) connect(m_store, &BookmarksStore::changed,
                         this, &BookmarksModel::onStoreChanged);
}
BookmarksModel::~BookmarksModel() = default;

void BookmarksModel::onStoreChanged()
{
    beginResetModel();
    endResetModel();
}

// Internal pointer layout: store a `const BookmarkItem *` directly. Root-level
// rows carry a nullptr parent pointer; nested rows carry a pointer to their
// parent item. (This is stable because BookmarksStore::changed resets the model.)

QModelIndex BookmarksModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!m_store || column != 0) return {};
    const BookmarkItem *parentItem = itemForIndex(parent);
    const auto &list = parentItem ? parentItem->children : m_store->rootItems();
    if (row < 0 || row >= list.size()) return {};
    return createIndex(row, column, const_cast<BookmarkItem *>(&list.at(row)));
}

QModelIndex BookmarksModel::parent(const QModelIndex &child) const
{
    if (!child.isValid() || !m_store) return {};
    const auto *childItem = static_cast<const BookmarkItem *>(child.internalPointer());
    // Walk the store to find childItem's parent (linear — acceptable for expected tree sizes).
    std::function<const BookmarkItem *(const QList<BookmarkItem> &, const BookmarkItem *)> find =
        [&](const QList<BookmarkItem> &siblings,
            const BookmarkItem *target) -> const BookmarkItem * {
        for (const auto &s : siblings) {
            for (const auto &c : s.children) {
                if (&c == target) return &s;
            }
            if (auto *r = find(s.children, target)) return r;
        }
        return nullptr;
    };
    const BookmarkItem *parentItem = find(m_store->rootItems(), childItem);
    if (!parentItem) return {};

    // Find parentItem's row among its parent's siblings (or root).
    std::function<int(const QList<BookmarkItem> &, const BookmarkItem *)> rowIn =
        [&](const QList<BookmarkItem> &list, const BookmarkItem *it) -> int {
        for (int i = 0; i < list.size(); ++i)
            if (&list.at(i) == it) return i;
        return -1;
    };
    int row = rowIn(m_store->rootItems(), parentItem);
    if (row >= 0)
        return createIndex(row, 0, const_cast<BookmarkItem *>(parentItem));
    // Nested — recurse once more.
    std::function<int(const QList<BookmarkItem> &, const BookmarkItem *)> findRow =
        [&](const QList<BookmarkItem> &list, const BookmarkItem *it) -> int {
        for (const auto &s : list) {
            int r = rowIn(s.children, it);
            if (r >= 0) return r;
            r = findRow(s.children, it);
            if (r >= 0) return r;
        }
        return -1;
    };
    row = findRow(m_store->rootItems(), parentItem);
    return row >= 0 ? createIndex(row, 0, const_cast<BookmarkItem *>(parentItem)) : QModelIndex{};
}

int BookmarksModel::rowCount(const QModelIndex &parent) const
{
    if (!m_store) return 0;
    const BookmarkItem *item = itemForIndex(parent);
    return item ? item->children.size() : m_store->rootItems().size();
}

int BookmarksModel::columnCount(const QModelIndex &) const { return 1; }

QVariant BookmarksModel::data(const QModelIndex &index, int role) const
{
    const BookmarkItem *item = itemForIndex(index);
    if (!item) return {};
    switch (role) {
    case Qt::DisplayRole:
        if (!item->title.isEmpty()) return item->title;
        if (item->type == QStringLiteral("file") || item->type == QStringLiteral("folder"))
            return QFileInfo(item->path).completeBaseName();
        if (item->type == QStringLiteral("search")) return item->query;
        if (item->type == QStringLiteral("graph")) return QStringLiteral("Graph view");
        if (item->type == QStringLiteral("group")) return QStringLiteral("Group");
        return item->type;
    case Qt::DecorationRole: {
        static const QHash<QString, QString> iconByType = {
            {QStringLiteral("file"),   QStringLiteral("text-x-generic")},
            {QStringLiteral("folder"), QStringLiteral("folder")},
            {QStringLiteral("search"), QStringLiteral("edit-find")},
            {QStringLiteral("graph"),  QStringLiteral("view-sort")},
            {QStringLiteral("group"),  QStringLiteral("folder-bookmark")},
        };
        return QIcon::fromTheme(iconByType.value(item->type, QStringLiteral("bookmark-new")));
    }
    case Qt::ToolTipRole:
        return item->path.isEmpty() ? item->query : item->path;
    case BookmarksTypeRole: return item->type;
    case BookmarksPathRole: return item->path.isEmpty() ? item->query : item->path;
    }
    return {};
}

Qt::ItemFlags BookmarksModel::flags(const QModelIndex &index) const
{
    Qt::ItemFlags f = QAbstractItemModel::flags(index);
    if (index.isValid()) f |= Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled;
    else                 f |= Qt::ItemIsDropEnabled;
    return f;
}

Qt::DropActions BookmarksModel::supportedDropActions() const { return Qt::MoveAction; }

QStringList BookmarksModel::mimeTypes() const
{
    return {QStringLiteral("application/x-corbomite-bookmarks-drag")};
}

QMimeData *BookmarksModel::mimeData(const QModelIndexList &indexes) const
{
    auto *mime = new QMimeData;
    if (indexes.isEmpty()) return mime;
    const QStringList path = pathOf(indexes.first());
    mime->setData(QStringLiteral("application/x-corbomite-bookmarks-drag"),
                  path.join(QLatin1Char('/')).toUtf8());
    return mime;
}

bool BookmarksModel::dropMimeData(const QMimeData *data, Qt::DropAction action,
                                  int row, int /*column*/, const QModelIndex &parent)
{
    if (action != Qt::MoveAction || !m_store) return false;
    if (!data->hasFormat(QStringLiteral("application/x-corbomite-bookmarks-drag"))) return false;
    const QString joined = QString::fromUtf8(
        data->data(QStringLiteral("application/x-corbomite-bookmarks-drag")));
    const QStringList fromPath = joined.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    const QStringList toParent = pathOf(parent);
    return m_store->moveBookmark(fromPath, toParent, row);
}

QStringList BookmarksModel::pathOf(const QModelIndex &index) const
{
    if (!index.isValid()) return {};
    QStringList out;
    QModelIndex cur = index;
    while (cur.isValid()) {
        out.prepend(QString::number(cur.row()));
        cur = cur.parent();
    }
    return out;
}

const BookmarkItem *BookmarksModel::itemForIndex(const QModelIndex &index) const
{
    if (!index.isValid()) return nullptr;
    return static_cast<const BookmarkItem *>(index.internalPointer());
}

} // namespace Corbomite::Bookmarks
```

- [ ] **Step 4: Extend `tests/CMakeLists.txt`**

Append to `src/plugins/bookmarks/tests/CMakeLists.txt`:

```cmake
add_executable(tst_bookmarks_model tst_bookmarks_model.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../BookmarksStore.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../BookmarksModel.cpp)
target_include_directories(tst_bookmarks_model PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..)
add_test(NAME tst_bookmarks_model COMMAND tst_bookmarks_model)
target_link_libraries(tst_bookmarks_model PRIVATE
    Qt6::Test Qt6::Widgets Qt6::Gui
    Corbomite::Core Corbomite::Vault
    KF6::I18n)
set_tests_properties(tst_bookmarks_model PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
```

- [ ] **Step 5: Build + test**

```bash
cmake --build build -j 10 --target tst_bookmarks_model
cd build && ctest -R tst_bookmarks_model --output-on-failure
```

Expected: all tests PASS.

- [ ] **Step 6: Commit**

```bash
git add src/plugins/bookmarks/BookmarksModel.* src/plugins/bookmarks/tests/
git commit -m "feat(bookmarks): BookmarksModel QAbstractItemModel adapter with DnD mime (Cluster S task 1.3)"
```

---

## Task Group 2 — Plugin shell, view, commands

### Task 2.1: Plugin skeleton + metadata + view factory

**Files:**
- Create: `src/plugins/bookmarks/BookmarksPlugin.h`
- Create: `src/plugins/bookmarks/BookmarksPlugin.cpp`
- Create: `src/plugins/bookmarks/BookmarksView.h`
- Create: `src/plugins/bookmarks/BookmarksView.cpp`
- Create: `src/plugins/bookmarks/metadata.json.in`
- Create: `src/plugins/bookmarks/BookmarkModal.h` (stub, implemented in Task 3.1)
- Create: `src/plugins/bookmarks/BookmarkModal.cpp` (stub)
- Modify: `src/plugins/bookmarks/CMakeLists.txt` (add all sources)

- [ ] **Step 1: Write `metadata.json.in`**

```json
{
    "KPlugin": {
        "Id": "corbomite-bookmarks",
        "Name": "Bookmarks",
        "Description": "Save and organize references to files, folders, headings, blocks, searches, and graphs",
        "Icon": "bookmark-new",
        "Version": "1.0",
        "License": "GPL-3.0-or-later",
        "Category": "Core",
        "EnabledByDefault": true,
        "Authors": [{"Name": "Corbomite Developers"}]
    },
    "X-Corbomite-Trusted": @X_CORBOMITE_TRUSTED@,
    "X-Corbomite-Permissions": ["vault.read", "vault.write", "vault.events", "workspace", "ui.views", "ui.commands"],
    "X-Corbomite-MinVersion": "0.1.0",
    "X-Corbomite-ApiLevel": 1,
    "X-Corbomite-DockArea": "right",
    "X-Corbomite-DockIcon": "bookmark-new",
    "X-Corbomite-DockTitle": "Bookmarks"
}
```

- [ ] **Step 2: Write `BookmarksView.h` + `.cpp` (minimal)**

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QPointer>
#include <QWidget>

class QLabel;
class QToolButton;
class QTreeView;

namespace Corbomite { class WorkspaceController; }

namespace Corbomite::Bookmarks {

class BookmarksModel;
class BookmarksStore;

class BookmarksView : public QWidget
{
    Q_OBJECT
public:
    BookmarksView(BookmarksStore *store,
                  Corbomite::WorkspaceController *workspace,
                  QWidget *parent = nullptr);
    ~BookmarksView() override;

    QTreeView *treeView() const { return m_tree; }

signals:
    void requestNewBookmark();  ///< `+` header button pressed; host opens modal

private slots:
    void onActivated(const QModelIndex &index);
    void onContextMenu(const QPoint &pos);

private:
    QPointer<BookmarksStore>               m_store;
    QPointer<Corbomite::WorkspaceController> m_workspace;
    BookmarksModel                        *m_model = nullptr;

    QLabel      *m_header = nullptr;
    QToolButton *m_plusBtn = nullptr;
    QTreeView   *m_tree = nullptr;
};

} // namespace Corbomite::Bookmarks
```

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "BookmarksView.h"

#include "BookmarksModel.h"
#include "BookmarksStore.h"

#include "corbomite/core/proxies/WorkspaceController.h"

#include <KLocalizedString>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QLabel>
#include <QMenu>
#include <QToolButton>
#include <QTreeView>
#include <QVBoxLayout>

namespace Corbomite::Bookmarks {

BookmarksView::BookmarksView(BookmarksStore *store,
                             Corbomite::WorkspaceController *workspace,
                             QWidget *parent)
    : QWidget(parent), m_store(store), m_workspace(workspace)
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);

    auto *headerRow = new QHBoxLayout;
    m_header = new QLabel(i18n("Bookmarks"), this);
    m_plusBtn = new QToolButton(this);
    m_plusBtn->setIcon(QIcon::fromTheme(QStringLiteral("list-add")));
    m_plusBtn->setToolTip(i18n("New bookmark from current"));
    headerRow->addWidget(m_header);
    headerRow->addStretch();
    headerRow->addWidget(m_plusBtn);
    outer->addLayout(headerRow);

    m_tree = new QTreeView(this);
    m_tree->setHeaderHidden(true);
    m_tree->setDragDropMode(QAbstractItemView::InternalMove);
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    outer->addWidget(m_tree);

    m_model = new BookmarksModel(m_store, this);
    m_tree->setModel(m_model);

    connect(m_tree, &QAbstractItemView::activated, this, &BookmarksView::onActivated);
    connect(m_tree, &QWidget::customContextMenuRequested, this, &BookmarksView::onContextMenu);
    connect(m_plusBtn, &QToolButton::clicked, this, &BookmarksView::requestNewBookmark);
}

BookmarksView::~BookmarksView() = default;

void BookmarksView::onActivated(const QModelIndex &index)
{
    if (!index.isValid() || !m_workspace) return;
    const QString path = index.data(BookmarksModel::BookmarksPathRole).toString();
    if (path.isEmpty()) return;
    m_workspace->openLinkText(path, QString());
}

void BookmarksView::onContextMenu(const QPoint &pos)
{
    const QModelIndex idx = m_tree->indexAt(pos);
    if (!idx.isValid() || !m_store) return;
    QMenu menu(this);
    auto *del = menu.addAction(QIcon::fromTheme(QStringLiteral("edit-delete")), i18n("Delete"));
    connect(del, &QAction::triggered, this, [this, idx] {
        if (m_store) m_store->removeBookmark(m_model->pathOf(idx));
    });
    menu.exec(m_tree->viewport()->mapToGlobal(pos));
}

} // namespace Corbomite::Bookmarks
```

(Rename / Move-to-group context menu entries deferred to Task 2.3.)

**Note:** `WorkspaceController::openLinkText` must exist on the proxy. If it does not (confirm by grepping the header), fall back to `WorkspaceController::openFile(path)` for the MVP and leave a TODO with a reference to Cluster G follow-up #3 (`Workspace::openLinkText`). Do **not** block on this.

- [ ] **Step 3: Write stub `BookmarkModal.h` + `.cpp`** (just a compilable no-op; filled in at Task 3.1)

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "BookmarkItem.h"
#include <QDialog>
namespace Corbomite::Bookmarks {
class BookmarksStore;
class BookmarkModal : public QDialog {
    Q_OBJECT
public:
    static bool runFor(BookmarkItem inferred, BookmarksStore *store, QWidget *parent);
};
} // namespace
```

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "BookmarkModal.h"
#include "BookmarksStore.h"
namespace Corbomite::Bookmarks {
bool BookmarkModal::runFor(BookmarkItem inferred, BookmarksStore *store, QWidget *)
{
    // Stub: silently commit. Replaced with full modal in Task 3.1.
    if (store) store->addBookmark(std::move(inferred), {});
    return store != nullptr;
}
} // namespace
```

- [ ] **Step 4: Write `BookmarksPlugin.h` + `.cpp`**

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "corbomite/vault/Plugin.h"

#include <QPointer>
#include <QTimer>

namespace Corbomite::Bookmarks {

class BookmarksStore;

class BookmarksPlugin : public Corbomite::Plugin
{
    Q_OBJECT
public:
    BookmarksPlugin(QObject *parent, const QVariantList &args);
    ~BookmarksPlugin() override;

    QObject *createView(Corbomite::MainWindow *mainWindow) override;

    QJsonObject saveSessionState(QObject *view) const override;
    void loadSessionState(QObject *view, const QJsonObject &state) override;

protected:
    void onLoad(Corbomite::PluginContext *ctx) override;
    void onUnload() override;

private:
    void registerCommands(Corbomite::PluginContext *ctx);
    void scheduleSave();
    void doSave();

    QPointer<BookmarksStore>   m_store;
    QTimer                     m_saveTimer;
};

} // namespace
```

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "BookmarksPlugin.h"

#include "BookmarkItem.h"
#include "BookmarkModal.h"
#include "BookmarksStore.h"
#include "BookmarksView.h"

#include "corbomite/core/Command.h"
#include "corbomite/core/proxies/CommandRegistrar.h"
#include "corbomite/core/proxies/WorkspaceController.h"
#include "corbomite/vault/PluginContext.h"
#include "corbomite/vault/Vault.h"
#include "corbomite/vault/proxies/VaultProxy.h"

#include <KLocalizedString>
#include <KPluginFactory>
#include <QDateTime>
#include <QHeaderView>
#include <QJsonObject>
#include <QTreeView>

namespace Corbomite::Bookmarks {

BookmarksPlugin::BookmarksPlugin(QObject *parent, const QVariantList &)
    : Corbomite::Plugin(parent)
{
    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(500);
    connect(&m_saveTimer, &QTimer::timeout, this, &BookmarksPlugin::doSave);
}

BookmarksPlugin::~BookmarksPlugin() = default;

void BookmarksPlugin::onLoad(Corbomite::PluginContext *ctx)
{
    if (!ctx) return;

    m_store = new BookmarksStore(this);
    // Initial read via VaultProxy.
    if (auto *vp = ctx->vault()) {
        const QJsonObject obj = vp->readConfigJson(QStringLiteral("bookmarks.json")).toObject();
        m_store->loadFromJson(obj);
    }
    connect(m_store, &BookmarksStore::changed, this, &BookmarksPlugin::scheduleSave);

    registerCommands(ctx);

    // View type registration — the view is created per-window via createView().
    // (If ViewRegistrar needs an explicit registerType(), add it here; most
    // dock-plugin implementations rely on the DockArea metadata alone.)
}

void BookmarksPlugin::onUnload()
{
    if (m_saveTimer.isActive()) { m_saveTimer.stop(); doSave(); }
}

void BookmarksPlugin::scheduleSave() { m_saveTimer.start(); }

void BookmarksPlugin::doSave()
{
    if (!m_store || !context()) return;
    auto *vp = context()->vault();
    if (!vp) return;
    vp->writeConfigJson(QStringLiteral("bookmarks.json"), m_store->toJson());
}

QObject *BookmarksPlugin::createView(Corbomite::MainWindow *mainWindow)
{
    auto *ctx = context();
    if (!ctx || !m_store) return nullptr;
    auto *view = new BookmarksView(m_store, ctx->workspace(),
                                    reinterpret_cast<QWidget *>(mainWindow));
    connect(view, &BookmarksView::requestNewBookmark, this, [this, view] {
        auto *ws = context() ? context()->workspace() : nullptr;
        BookmarkItem inferred;
        inferred.type = QStringLiteral("file");
        inferred.ctime = QDateTime::currentMSecsSinceEpoch();
        // Populate path from active file if WorkspaceController exposes it;
        // otherwise the modal re-asks.
        BookmarkModal::runFor(std::move(inferred), m_store, view);
    });
    return view;
}

QJsonObject BookmarksPlugin::saveSessionState(QObject *view) const
{
    auto *bv = qobject_cast<BookmarksView *>(view);
    if (!bv) return {};
    QJsonObject obj;
    // Record expanded group indices as "0", "1", "1/0", ...
    QStringList expanded;
    auto *tree = bv->treeView();
    std::function<void(const QModelIndex &)> walk = [&](const QModelIndex &parent) {
        const int rows = tree->model()->rowCount(parent);
        for (int r = 0; r < rows; ++r) {
            const QModelIndex idx = tree->model()->index(r, 0, parent);
            if (tree->isExpanded(idx)) {
                QStringList path;
                QModelIndex cur = idx;
                while (cur.isValid()) {
                    path.prepend(QString::number(cur.row()));
                    cur = cur.parent();
                }
                expanded.append(path.join(QLatin1Char('/')));
            }
            walk(idx);
        }
    };
    walk({});
    obj.insert(QStringLiteral("expanded"), QJsonArray::fromStringList(expanded));
    return obj;
}

void BookmarksPlugin::loadSessionState(QObject *view, const QJsonObject &state)
{
    auto *bv = qobject_cast<BookmarksView *>(view);
    if (!bv) return;
    auto *tree = bv->treeView();
    const QJsonArray arr = state.value(QStringLiteral("expanded")).toArray();
    for (const auto &v : arr) {
        const QStringList parts = v.toString().split(QLatin1Char('/'), Qt::SkipEmptyParts);
        QModelIndex cur;
        for (const QString &p : parts) {
            bool ok = false;
            int r = p.toInt(&ok);
            if (!ok) break;
            cur = tree->model()->index(r, 0, cur);
            if (!cur.isValid()) break;
        }
        if (cur.isValid()) tree->expand(cur);
    }
}

void BookmarksPlugin::registerCommands(Corbomite::PluginContext *ctx)
{
    auto *commands = ctx->commands();
    if (!commands) return;
    auto *workspace = ctx->workspace();

    auto makeCmd = [](const QString &id, const QString &name, const QString &icon,
                      Corbomite::Command::SimpleCallback cb) {
        Corbomite::Command c;
        c.id = id; c.name = name; c.icon = icon; c.callback = std::move(cb);
        return c;
    };

    // `open` — reveals the panel. This one DOES go through normal prefixing
    // because Cluster R's slug dispatcher expects `corbomite-bookmarks:open`.
    {
        Corbomite::Command c = makeCmd(QStringLiteral("open"),
            i18n("Open bookmarks"), QStringLiteral("bookmark-new"),
            [workspace] {
                if (workspace) workspace->revealDockView(QStringLiteral("bookmarks"));
            });
        commands->addCommand(c);
    }

    // Obsidian-id commands — use addCommandRaw to preserve `bookmarks:*` prefix
    // for .obsidian/hotkeys.json round-trip (addendum §4). If addCommandRaw is
    // not available yet (implement it in CommandRegistrar), fall back to
    // addCommand and accept the `corbomite-bookmarks:` prefix temporarily.
    auto addRaw = [commands](Corbomite::Command c) {
        commands->addCommandRaw(c);   // NEW overload added in this task group
    };

    addRaw(makeCmd(QStringLiteral("bookmarks:bookmark-current-file"),
        i18n("Bookmark current file"), QStringLiteral("bookmark-new"),
        [this, workspace] {
            if (!m_store || !workspace) return;
            const QString path = workspace->activeFilePath();
            if (path.isEmpty()) return;
            BookmarkItem item;
            item.type = QStringLiteral("file");
            item.path = path;
            item.ctime = QDateTime::currentMSecsSinceEpoch();
            m_store->addBookmark(std::move(item), {});
        }));

    addRaw(makeCmd(QStringLiteral("bookmarks:bookmark-all-tabs"),
        i18n("Bookmark all open tabs"), QStringLiteral("bookmark-new"),
        [this, workspace] {
            if (!m_store || !workspace) return;
            const QStringList paths = workspace->openTabPaths();
            for (const QString &p : paths) {
                BookmarkItem item;
                item.type = QStringLiteral("file");
                item.path = p;
                item.ctime = QDateTime::currentMSecsSinceEpoch();
                m_store->addBookmark(std::move(item), {});
            }
        }));

    // bookmark-current-heading, bookmark-current-block, bookmark-current-search,
    // bookmark-current-graph — all use checkCallback so palette greys them out
    // when the context is wrong. Implement per spec §3.3.
    //
    // Each follows the same shape; `checking=true` returns whether the
    // active view supports the operation without mutating store. Filled in
    // at Task 2.2 once the WorkspaceController surface is confirmed.
}

} // namespace

K_PLUGIN_FACTORY_WITH_JSON(BookmarksPluginFactory, "metadata.json",
    registerPlugin<Corbomite::Bookmarks::BookmarksPlugin>();)

#include "BookmarksPlugin.moc"
```

- [ ] **Step 5: Add `CommandRegistrar::addCommandRaw`**

In `libs/core/include/corbomite/core/proxies/CommandRegistrar.h`:

```cpp
    /// Like addCommand, but does NOT prefix `cmd.id` with pluginId. Used by
    /// plugins that must register commands under a canonical Obsidian-compat
    /// namespace (e.g. `bookmarks:bookmark-current-file`) rather than the
    /// default `<pluginId>:<localId>` shape. Tracked for cleanup on destruction
    /// the same way as addCommand.
    void addCommandRaw(Command &cmd);
```

In `libs/core/src/proxies/CommandRegistrar.cpp` add the implementation (mirror of `addCommand` minus the prefix-mutation line).

- [ ] **Step 6: Update `CMakeLists.txt` to list all plugin sources**

Replace `src/plugins/bookmarks/CMakeLists.txt` content with:

```cmake
corbomite_add_plugin(corbomite-bookmarks
    METADATA_TEMPLATE "${CMAKE_CURRENT_SOURCE_DIR}/metadata.json.in"
    SOURCES
        BookmarksPlugin.cpp
        BookmarksView.cpp
        BookmarksModel.cpp
        BookmarksStore.cpp
        BookmarkModal.cpp
    TRUSTED
    LINK_LIBRARIES
        Qt6::Widgets
        KF6::I18n
        Corbomite::Core
        Corbomite::Storage
        Corbomite::Vault)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
```

- [ ] **Step 7: Build + smoke-test**

```bash
cmake --build build -j 10
cd build && ctest -R tst_bookmarks --output-on-failure
ls build/lib/plugins/corbomite/corbomite-bookmarks.so
```

Expected: plugin builds, existing tests pass, `.so` exists.

- [ ] **Step 8: Manual smoke test — plugin loads and panel appears**

```bash
./build/Corbomite
```

Open a vault. Check the right dock for a "Bookmarks" panel. It should be empty. Close Corbomite cleanly.

- [ ] **Step 9: Commit**

```bash
git add src/plugins/bookmarks/ libs/core/include/corbomite/core/proxies/CommandRegistrar.h libs/core/src/proxies/CommandRegistrar.cpp
git commit -m "feat(bookmarks): plugin shell, view, store load/save wiring (Cluster S task 2.1)"
```

---

### Task 2.2: Commands — commands test + wiring

**Files:**
- Create: `src/plugins/bookmarks/tests/tst_bookmarks_commands.cpp`
- Modify: `src/plugins/bookmarks/BookmarksPlugin.cpp` (fill in 4 remaining commands)
- Modify: `src/plugins/bookmarks/tests/CMakeLists.txt`

- [ ] **Step 1: Write failing test**

The test instantiates `BookmarksStore` + a mock `WorkspaceController`-alike and runs each command, asserting the store delta. Since exercising real `CommandRegistrar` requires a full PluginContext, test the plugin's callbacks directly by extracting the command-construction helpers into a free function in an anonymous namespace or by adding a friend-test accessor.

Pragmatic approach: expose a `BookmarksPlugin::commandsForTest()` method behind `#ifdef BOOKMARKS_TEST_ACCESS` that returns a `QVector<Command>` built the same way as `registerCommands`. Drive tests against that vector — the tests exercise the callback logic, not the PluginContext plumbing (which is exercised by the existing PluginManager integration tests).

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#define BOOKMARKS_TEST_ACCESS
#include "../BookmarksPlugin.h"
#include "../BookmarksStore.h"

#include <QTest>

using namespace Corbomite::Bookmarks;

class TstBookmarksCommands : public QObject
{
    Q_OBJECT
private slots:
    void bookmarkCurrentFileAppendsAtRoot();
    void bookmarkAllTabsAppendsEachPath();
    void bookmarkCurrentHeadingUsesSubpath();
    void bookmarkCurrentBlockNoticeWhenNoBlockId();
    void bookmarkCurrentSearchSnapshotsQuery();
    void bookmarkCurrentGraphSnapshotsOptions();
    void openCommandRevealsDock();
};
// Bodies assert store size + item shape after invoking each callback with a
// synthetic context. See BookmarksPlugin::commandsForTest for the surface.
```

(Stub bodies showing the assertion shape — fill each in using the test-access helper.)

- [ ] **Step 2: Add `commandsForTest`** (header guarded by `#ifdef BOOKMARKS_TEST_ACCESS`).

- [ ] **Step 3: Fill in 4 remaining command callbacks in `BookmarksPlugin::registerCommands`**

For each of `bookmark-current-heading`, `bookmark-current-block`, `bookmark-current-search`, `bookmark-current-graph`, write a `checkCallback` per spec §3.3. If the necessary WorkspaceController accessor (e.g. `activeViewType()`, `activeSearchQuery()`, `activeGraphOptions()`) does not exist, register the command with a `checkCallback` that returns `false` and leave a `// TODO(Cluster S follow-up):` comment pointing to a `backlog.md` entry. Ship the MVP with at least `bookmark-current-file`, `bookmark-all-tabs`, `open`, and `bookmark-current-search` (if `WorkspaceController::activeSearchQuery()` exists — grep to confirm).

- [ ] **Step 4: Update `tests/CMakeLists.txt`** with `tst_bookmarks_commands` executable (same shape as others; compile `#define BOOKMARKS_TEST_ACCESS`).

- [ ] **Step 5: Build + run**

```bash
cmake --build build -j 10 --target tst_bookmarks_commands
cd build && ctest -R tst_bookmarks_commands --output-on-failure
```

- [ ] **Step 6: Commit**

```bash
git add src/plugins/bookmarks/
git commit -m "feat(bookmarks): 7 bookmarks:* commands with availability gating (Cluster S task 2.2)"
```

---

### Task 2.3: Stale-bookmark handling + richer context menu

**Files:**
- Modify: `src/plugins/bookmarks/BookmarksPlugin.cpp` (connect Vault rename/delete signals)
- Modify: `src/plugins/bookmarks/BookmarksView.cpp` (Rename / Move-to-group / Delete context menu items)
- Modify: `src/plugins/bookmarks/BookmarksStore.h/.cpp` (add `markOrphaned(const QString &path)` + `renamePath(old, new)`)
- Modify: `src/plugins/bookmarks/tests/tst_bookmarks_store.cpp` (rename test + orphaned-marker test)

- [ ] **Step 1: Add test cases for `renamePath` (walk tree, rewrite matching `path` with preserved `#subpath`) and `markOrphaned` (sets `unknownKeys["_orphaned"] = true`).**

- [ ] **Step 2: Implement in `BookmarksStore`.**

- [ ] **Step 3: Wire `VaultProxy::fileRenamed` / `fileDeleted` signals** in `BookmarksPlugin::onLoad` to call the new store methods. Confirm exact signal names via `libs/vault/include/corbomite/vault/proxies/VaultProxy.h`; if missing, gate behind `if (auto *v = ctx->vault())` + a `// TODO(Cluster S followup): needs VaultProxy rename signal` and continue.

- [ ] **Step 4: Extend `BookmarksView::onContextMenu`** — Rename (pops an inline `QInputDialog::getText` seeded with current title, writes via store), Move to group (QMenu of group titles from `store->rootItems()` walk), Delete (already wired).

- [ ] **Step 5: Build + test + manual smoke (rename a file in Files panel, confirm bookmark follows).**

- [ ] **Step 6: Commit**

```bash
git add src/plugins/bookmarks/
git commit -m "feat(bookmarks): stale-bookmark handling and context menu (Cluster S task 2.3)"
```

---

## Task Group 3 — Modal, settings, and R integration

### Task 3.1: `BookmarkModal` — full implementation

**Files:**
- Modify: `src/plugins/bookmarks/BookmarkModal.h/.cpp` (replace stub with real QDialog)
- Create: `src/plugins/bookmarks/tests/tst_bookmarks_modal.cpp`
- Modify: `src/plugins/bookmarks/tests/CMakeLists.txt`

- [ ] **Step 1: Write failing test**

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "../BookmarkItem.h"
#include "../BookmarkModal.h"
#include "../BookmarksStore.h"

#include <QComboBox>
#include <QLineEdit>
#include <QTest>

using namespace Corbomite::Bookmarks;

class TstBookmarkModal : public QObject
{
    Q_OBJECT
private slots:
    void titlePrefilledFromFilePath();
    void groupComboListsRootPlusGroupsRecursively();
    void saveCommitsToStore();
    void cancelDoesNotMutateStore();
};
// Each test constructs a BookmarkModal directly (not via runFor), inspects
// its widgets, simulates Save/Cancel via QDialogButtonBox, and asserts
// store state.
```

- [ ] **Step 2: Replace `BookmarkModal.h/.cpp` with full implementation** — `QLineEdit` name (pre-filled per type), `QComboBox` group picker populated via a recursive walk of store groups (joined titles with `/`), `QDialogButtonBox{Cancel,Save}`. On Accept: compose `BookmarkItem` from `inferred` + editable fields, call `store->addBookmark(item, groupPathParts)`.

- [ ] **Step 3: Build + test.**

- [ ] **Step 4: Commit**

```bash
git add src/plugins/bookmarks/BookmarkModal.* src/plugins/bookmarks/tests/
git commit -m "feat(bookmarks): BookmarkModal (name + group picker) (Cluster S task 3.1)"
```

---

### Task 3.2: Cluster R "Bookmark…" menu slot goes live

**Files:**
- Modify: `libs/core/include/corbomite/core/EditableFileView.h` (add `BookmarkCallback` + setter)
- Modify: `libs/core/src/EditableFileView.cpp:98-104` (replace disabled placeholder with live action)
- Modify: `src/app/MainWindow.cpp` (wire the bookmark callback to `bookmarks:bookmark-current-file` via `CommandRegistry::executeById`, adding a one-shot "open modal" flag so the user-triggered path hits the dialog — see spec §4 note about modal vs non-modal)

- [ ] **Step 1: Add the callback signature + setter in `EditableFileView.h`.** Mirror the existing `setRenameCallback` / `setMoveCallback` shape.

- [ ] **Step 2: In `EditableFileView::onMoreOptionsMenu`**, replace lines 98-104 with:

```cpp
auto *bookmarkAct = new QAction(
    QIcon::fromTheme(QStringLiteral("bookmark-new")),
    i18n("Bookmark..."), this);
connect(bookmarkAct, &QAction::triggered, this, [this] {
    if (m_bookmarkCallback && m_file) m_bookmarkCallback(m_file, this);
});
helper.addToSection(bookmarkAct, QStringLiteral("pane"));
```

- [ ] **Step 3: In `MainWindow.cpp`, after constructing each `EditableFileView` (grep for `setRenameCallback` to find all sites)**, add:

```cpp
view->setBookmarkCallback([this](TFile *file, View *src) {
    if (!file || !m_commandRegistry) return;
    // Via-modal path: open the modal seeded with this file's path/title.
    // The plugin's "+" button / keyboard command path does not open a modal
    // — this entry point does, matching Obsidian's UX.
    if (auto *pm = m_app ? m_app->pluginManager() : nullptr) {
        if (auto *info = pm->pluginById(QStringLiteral("corbomite-bookmarks"))) {
            if (info->instance)
                QMetaObject::invokeMethod(info->instance, "openBookmarkModalForFile",
                                           Q_ARG(QString, file->path()));
        }
    }
});
```

Add matching `Q_INVOKABLE void openBookmarkModalForFile(const QString &path)` to `BookmarksPlugin` that composes a `BookmarkItem{type:"file", path, ctime:now}` and calls `BookmarkModal::runFor(...)`.

- [ ] **Step 4: Add slug map entry** — in `MainWindow.cpp` line ~1357, add `{QStringLiteral("bookmarks"), QStringLiteral("corbomite-bookmarks")}`.

- [ ] **Step 5: Build + manual smoke test** — open a markdown file, hamburger menu, "Bookmark…" opens the modal; save; bookmark appears in the right-dock panel.

- [ ] **Step 6: Commit**

```bash
git add libs/core/include/corbomite/core/EditableFileView.h libs/core/src/EditableFileView.cpp src/app/MainWindow.cpp src/plugins/bookmarks/
git commit -m "feat(bookmarks): Cluster R menu slot goes live (Cluster S task 3.2)"
```

---

### Task 3.3: Settings tab (optional — MVP can defer)

**Scope:** one checkbox — "Back up bookmarks.json before first write each session". If `BookmarksSettingsTab` plumbing (Cluster N `createSettingsTab`) is already wired up for other plugins, add a minimal tab. If not, defer to backlog and close the cluster without it. Check with `grep -rn createSettingsTab src/plugins/`.

- [ ] **Step 1: Grep** `grep -rn "createSettingsTab\|SettingsTab" src/plugins/` — if zero results, skip this task, add a backlog entry, and proceed to Task 3.4.

- [ ] **Step 2: If infrastructure exists, add `BookmarksSettingsTab.h/.cpp`** with the single checkbox bound to the plugin's `data.json` (`ctx->saveData` / `loadData`). Add to CMakeLists. Commit.

---

### Task 3.4: Close the cluster

- [ ] **Step 1: Full test run**

```bash
cd build && ctest --output-on-failure -j 10
```

Expected: all bookmark tests green; no new failures elsewhere.

- [ ] **Step 2: Full smoke**

```bash
./build/Corbomite
```

Checklist:
1. Open a vault → Bookmarks panel visible in right dock.
2. Open a note → hamburger → Bookmark… → modal appears pre-filled → Save → bookmark appears.
3. Click the bookmark → the note opens.
4. `+` button in panel header → modal appears.
5. Right-click a bookmark → Delete → it disappears.
6. Drag a bookmark between siblings → reorder persists after restart.
7. Close + reopen Corbomite → bookmarks persist on disk.
8. Inspect `<vault>/.obsidian/bookmarks.json` → JSON matches the Obsidian schema.
9. If an Obsidian-written `bookmarks.json` with a `type:"graph"` or a future unknown type is placed in the vault, round-trip preserves it verbatim.

- [ ] **Step 3: Write retro + update PROJECT-STATE**

Create `docs/cluster-retros/cluster-s.md` (~one page; what shipped, what slipped, what to do next).

Update `docs/PROJECT-STATE.md`:
- Move Cluster S from "Plan-needed" to "Done" with closing line.
- Replace the top "Current focus" with a fresh ≤3-sentence entry pointing at the next cluster (per CLAUDE.md's "don't regrow PROJECT-STATE" rule).
- Append closeout paragraph to `docs/decisions-archive.md` under a new dated H2.

Update `docs/backlog.md`:
- Add `MarkdownView::ensureBlockIdAtCursor()` auto-insert follow-up.
- Add "Remove all broken bookmarks" bulk-clean context menu.
- Add any deferred commands (heading/block/graph) if they couldn't be wired for lack of WorkspaceController accessors.
- Add settings-tab follow-up if deferred in Task 3.3.

Update `docs/superpowers/plans/INDEX.md` — move S plan row to the archived section, link to retro.

- [ ] **Step 4: Commit**

```bash
git add docs/
git commit -m "docs(cluster-s): retro + project-state closeout (Cluster S)"
```

---

## Definition of done

- All seven `bookmarks:*` commands register and are discoverable in the command palette (at minimum `open`, `bookmark-current-file`, `bookmark-all-tabs` fully functional; the four context-gated ones register even if WorkspaceController lacks some accessors — their `checkCallback` returns false cleanly).
- `.obsidian/bookmarks.json` round-trips a fixture exported from Obsidian 1.12.7 byte-for-byte (up to JSON key ordering) with unknown-key preservation.
- Right-dock panel renders the tree, responds to single-click (opens file), drag-reorder, and context menu Delete/Rename.
- Cluster R's "Bookmark…" menu slot (previously disabled) fires the modal; saved bookmarks appear in the panel.
- `ctest` fully green.
- Retro + PROJECT-STATE + decisions-archive + INDEX updated.

---

## Open questions carried from the spec (flag during execution if defaults are wrong)

1. Auto-insert block ids on `bookmark-current-block`? **Plan's default: no — show Notice.** Auto-insert is a backlog follow-up.
2. Default dock side. **Plan's default: right** (matches other dock plugins).
3. Default keybindings for `bookmarks:*` commands. **Plan's default: none** (Obsidian matches).

If the engineer disagrees with any default, raise in a comment on the closing commit rather than silently changing — these are cross-cluster UX choices.
