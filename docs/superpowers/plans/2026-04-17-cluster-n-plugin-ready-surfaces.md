# Cluster N — Plugin-Ready Surfaces — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Retire the three stop-gap raw-pointer accessors in `PluginContext`, ship real persistence (QtKeychain + plugin `data.json`), commit to a 12-month API-stability window, and prove the third-party path with a reference plugin + author documentation.

**Architecture:** Extend the existing proxy layer (promoted `VaultProxy` to `QObject` with Qt-signal forwarding; new `SearchProxy` in `libs/storage/proxies/`) so the four currently-raw-reaching built-ins (LocalGraph / FileExplorer / GraphView / Search) can route entirely through proxies. Then delete the three accessors. Add `QtKeychain`-backed `SecretStorage` and Obsidian-shape plugin `data.json`. Introduce a compile-time `corbomite_add_plugin()` helper for trust flag + `X-Corbomite-Trusted` metadata injection. Enforce `X-Corbomite-MinVersion` + new `X-Corbomite-ApiLevel` at load. Ship an in-tree `examples/note-stats-plugin/` as an end-to-end reference built with only public Corbomite CMake targets, and `docs/plugin-development/` as prose + CMake-template companion.

**Tech Stack:** C++20, Qt6, KDE Frameworks 6 (`KPluginFactory`, `KConfigGroup`), `qt6keychain`, tree-sitter-based `libs/markoff-parser` (for note-stats word count), existing `libs/vault/` + `libs/storage/` + `libs/core/` proxy infrastructure from Cluster Q.

**Spec:** `docs/superpowers/specs/2026-04-17-cluster-n-plugin-ready-surfaces-design.md`

---

## File Structure

### New files created by this plan

| Path | Responsibility |
|---|---|
| `libs/storage/include/corbomite/storage/proxies/SearchProxy.h` | Public plugin-facing search API (FTS + backlinks + tags). |
| `libs/storage/src/proxies/SearchProxy.cpp` | Permission-gated SQLiteIndex forwarder. |
| `tests/storage/tst_search_proxy.cpp` | SearchProxy permission-gating + forwarding tests. |
| `libs/vault/include/corbomite/vault/PluginDataStore.h` | `.obsidian/plugins/<id>/data.json` I/O helper (header-only class owned by PluginContext). |
| `libs/vault/src/PluginDataStore.cpp` | Atomic QSaveFile round-trip. |
| `tests/vault/tst_plugin_data_store.cpp` | Round-trip + atomic-write tests. |
| `cmake/CorbomitePlugin.cmake` | `corbomite_add_plugin()` helper; injects `X-Corbomite-Trusted` via metadata.json.in substitution. |
| `cmake/CorbomiteConfig.cmake.in` | CMake package-config template for `find_package(Corbomite)`. |
| `examples/plugin-template/*` | Minimal third-party plugin skeleton (CMakeLists + metadata.json + stub Plugin class + tests). |
| `examples/note-stats-plugin/*` | Reference plugin — reads vault, renders stats view. |
| `docs/plugin-development/README.md` | Quickstart + TOC. |
| `docs/plugin-development/TUTORIAL.md` | Walks through the note-stats reference plugin end-to-end. |
| `docs/plugin-development/API-REFERENCE.md` | Hand-maintained catalog of proxy methods, permissions, lifecycle hooks. |
| `docs/plugin-development/API-STABILITY.md` | 12-month deprecation window contract. |
| `docs/plugin-development/DISTRIBUTION.md` | How to package and ship a plugin via distros. |
| `docs/cluster-retros/cluster-n.md` | Retro at cluster close. |

### Modified files

| Path | What changes |
|---|---|
| `libs/vault/include/corbomite/vault/proxies/VaultProxy.h` | Becomes `QObject`; adds forwarded Qt signals. |
| `libs/vault/src/proxies/VaultProxy.cpp` | Wires forwarded signals; `on()`/`off()` reimplemented on top. |
| `tests/vault/tst_vault_proxy.cpp` | Adds Qt-signal forwarding cases. |
| `libs/vault/include/corbomite/vault/PluginContext.h` | Deletes `vaultRaw()` / `metadataCacheRaw()` / `searchIndex()` / `setSearchIndex()`; adds `search()` proxy accessor; adds `loadData()` / `saveData()`; adds `setPluginDataDir()` for host wiring. |
| `libs/vault/src/PluginContext.cpp` | Matches. |
| `libs/storage/include/corbomite/storage/SQLiteIndex.h` | No public API changes; remains internal type not reachable from plugins. |
| `libs/storage/include/corbomite/storage/proxies/MetadataCacheReader.h` | Gap-fill any methods the four built-ins currently reach for through the raw pointer (verify during Task 1.3). |
| `src/core/SecretStorage.h` / `.cpp` | QtKeychain backend behind `CORBOMITE_USE_KEYRING` option; QHash fallback. |
| `src/plugins/search/SearchView.{h,cpp}` | Ctor takes `SearchProxy *` instead of `SQLiteIndex *`. |
| `src/plugins/search/SearchPlugin.cpp` | Uses `ctx->search()` instead of `ctx->searchIndex()`. |
| `src/plugins/local-graph/LocalGraphView.{h,cpp}` | Ctor takes `SearchProxy *` + `VaultProxy *`. |
| `src/plugins/local-graph/LocalGraphPlugin.cpp` | Uses `ctx->search()` + `ctx->vault()`. |
| `src/plugins/graph-view/GraphView.{h,cpp}` | Setters take proxies. |
| `src/plugins/graph-view/GraphViewTab.{h,cpp}` | Ctor takes proxies. |
| `src/plugins/graph-view/GraphViewPlugin.{h,cpp}` | Uses `ctx->search()` + `ctx->vault()`; drops unused `setMetadataCache`. |
| `src/plugins/file-explorer/FileExplorerView.{h,cpp}` | Ctor takes `VaultProxy *`. |
| `src/plugins/file-explorer/FileExplorerPlugin.cpp` | Uses `ctx->vault()`. |
| `libs/models/src/NotesTreeModel.cpp` (and header) | Accepts `VaultProxy *`; connects to forwarded signals. |
| `src/graph/GraphDataBuilder.{h,cpp}` | Adds `buildGlobalGraph(SearchProxy*, VaultProxy*)` + `buildLocalGraph(SearchProxy*, VaultProxy*, ...)` overloads. (Old `SQLiteIndex*` + `Vault*` overloads stay — called by internal code outside plugins, e.g. MainWindow-hosted graph. If no remaining callers exist, delete the old ones in the same task.) |
| `src/plugins/*/CMakeLists.txt` (×8) | Swap to `corbomite_add_plugin()`. |
| `src/plugins/*/metadata.json` (×8) | Replaced by `metadata.json.in` templates with `@X_CORBOMITE_TRUSTED@` slot. |
| `src/app/MainWindow.cpp` | Injects `search` proxy via PluginContext configurator; wires plugin-data directory. |
| `src/app/PluginManager.cpp` / `.h` | Enforces `X-Corbomite-MinVersion` + `X-Corbomite-ApiLevel` at enable. |
| `CMakeLists.txt` (root) | Exports `CorbomiteConfig.cmake`; adds `examples/` subdir gated on `CORBOMITE_BUILD_EXAMPLES` option. |
| `docs/PROJECT-STATE.md` | Marks Cluster N done. |
| `docs/superpowers/plans/INDEX.md` | Updates row N. |

### Deleted by this plan

| Path | Reason |
|---|---|
| `src/plugins/*/metadata.json` (×8) | Replaced by `.in` templates. |

---

## Phase Overview

Five phases, executable sequentially. Phase 1 is the architectural core — everything else layers on. Phases 2–4 are independently dispatchable (can run as parallel subagents if the execution environment supports it). Phase 5 is closeout.

1. **Phase 1 — Proxy surface expansion.** `VaultProxy` → `QObject`; new `SearchProxy`; `MetadataCacheReader` gap-fill if needed. Enables Phase 2.
2. **Phase 2 — Plugin widget migration.** The four built-ins (Search, LocalGraph, GraphView, FileExplorer) move onto proxies. Delete stop-gaps from `PluginContext`.
3. **Phase 3 — Persistence.** QtKeychain `SecretStorage` + Obsidian-shape plugin `data.json`.
4. **Phase 4 — Trust + API stability.** `corbomite_add_plugin()` helper + `X-Corbomite-MinVersion` + `X-Corbomite-ApiLevel` + `CorbomiteConfig.cmake`.
5. **Phase 5 — Reference plugin + docs + closeout.** `examples/plugin-template/`, `examples/note-stats-plugin/`, `docs/plugin-development/`, retro, PROJECT-STATE.

---

# Phase 1 — Proxy Surface Expansion

Goal: make the proxy surface expressive enough that the four stop-gap consumers can migrate without loss of functionality.

## Task 1.1: Promote `VaultProxy` to `QObject` with forwarded Qt signals

**Files:**
- Modify: `libs/vault/include/corbomite/vault/proxies/VaultProxy.h`
- Modify: `libs/vault/src/proxies/VaultProxy.cpp`
- Modify: `tests/vault/tst_vault_proxy.cpp`

- [ ] **Step 1: Write the failing test for Qt-signal forwarding**

Open `tests/vault/tst_vault_proxy.cpp` and append:

```cpp
void tst_vault_proxy::qObjectSignalsForwardWhenEventsGranted()
{
    Corbomite::Vault vault;
    Corbomite::FileSystemAdapter adapter;
    vault.setAdapter(&adapter);
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    vault.load(dir.path());

    const QSet<QString> granted = { QStringLiteral("vault.read"),
                                    QStringLiteral("vault.events"),
                                    QStringLiteral("vault.write") };
    Corbomite::VaultProxy proxy(&vault, granted, QStringLiteral("test.plugin"));

    QSignalSpy createdSpy(&proxy, &Corbomite::VaultProxy::created);
    QSignalSpy modifiedSpy(&proxy, &Corbomite::VaultProxy::modified);
    QSignalSpy deletedSpy(&proxy, &Corbomite::VaultProxy::deletedFile);
    QSignalSpy renamedSpy(&proxy, &Corbomite::VaultProxy::renamed);

    auto *f = proxy.create(QStringLiteral("a.md"), QByteArray("hello"));
    QVERIFY(f);
    QCOMPARE(createdSpy.count(), 1);

    proxy.modify(f, QByteArray("world"));
    QCOMPARE(modifiedSpy.count(), 1);

    proxy.rename(f, QStringLiteral("b.md"));
    QCOMPARE(renamedSpy.count(), 1);

    auto *f2 = proxy.getFileByPath(QStringLiteral("b.md"));
    proxy.remove(f2);
    QCOMPARE(deletedSpy.count(), 1);
}

void tst_vault_proxy::qObjectSignalsSuppressedWhenEventsDenied()
{
    Corbomite::Vault vault;
    Corbomite::FileSystemAdapter adapter;
    vault.setAdapter(&adapter);
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    vault.load(dir.path());

    // vault.read + vault.write but NOT vault.events
    const QSet<QString> granted = { QStringLiteral("vault.read"),
                                    QStringLiteral("vault.write") };
    Corbomite::VaultProxy proxy(&vault, granted, QStringLiteral("test.plugin"));

    QSignalSpy createdSpy(&proxy, &Corbomite::VaultProxy::created);
    auto *f = proxy.create(QStringLiteral("a.md"), QByteArray("hi"));
    QVERIFY(f);
    QCOMPARE(createdSpy.count(), 0);  // no events permission → no forwarded signal
}
```

Add the matching slot declarations in the `private slots:` section of the test header:

```cpp
void qObjectSignalsForwardWhenEventsGranted();
void qObjectSignalsSuppressedWhenEventsDenied();
```

- [ ] **Step 2: Run test to verify it fails**

```bash
cmake --build build -j 10 --target tst_vault_proxy 2>&1 | tail -20
```

Expected: compile failure — `VaultProxy` is not a `QObject`, no `created`/`modified`/etc. signals declared.

- [ ] **Step 3: Promote `VaultProxy` to `QObject` with signals**

Modify `libs/vault/include/corbomite/vault/proxies/VaultProxy.h` — replace the class declaration:

```cpp
class VaultProxy : public QObject
{
    Q_OBJECT
public:
    VaultProxy(Vault *vault, const QSet<QString> &granted, QString pluginId,
               QObject *parent = nullptr);
    ~VaultProxy() override;

    VaultProxy(const VaultProxy &) = delete;
    VaultProxy &operator=(const VaultProxy &) = delete;

    // ---- Read (gated by vault.read) ----
    QByteArray     read(TFile *f) const;
    QByteArray     cachedRead(TFile *f) const;
    QByteArray     readBinary(TFile *f) const;
    bool           exists(const QString &path) const;
    TFile         *getFileByPath(const QString &path) const;
    TFolder       *getFolderByPath(const QString &path) const;
    TAbstractFile *getAbstractFileByPath(const QString &path) const;
    QVector<TFile *> getMarkdownFiles() const;
    QVector<TFile *> getFiles() const;
    TFolder       *getRoot() const;
    QString        getName() const;

    // ---- Mutation (gated by vault.write) ----
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

    // ---- Events (gated by vault.events) ----
    using EventFn = std::function<void(TAbstractFile *)>;
    QUuid on(const QString &event, EventFn fn);
    void  off(const QUuid &token);

    // ---- Config JSON (read gated by vault.read; write by vault.write) ----
    QJsonValue readConfigJson(const QString &name) const;
    bool       writeConfigJson(const QString &name, const QJsonValue &v);
    bool       deleteConfigJson(const QString &name);

Q_SIGNALS:
    // Forwarded Vault events, gated by vault.events (see wiring in ctor).
    void created(TAbstractFile *f);
    void modified(TAbstractFile *f);
    void deletedFile(TAbstractFile *f);
    void renamed(TAbstractFile *f, const QString &oldPath);

private:
    Vault                                 *m_vault;
    QSet<QString>                          m_granted;
    QString                                m_pluginId;
    QHash<QUuid, QMetaObject::Connection>  m_subscriptions;

    bool canRead() const
    {
        return m_granted.contains(QStringLiteral("vault.read"));
    }
    bool canWrite() const
    {
        return m_granted.contains(QStringLiteral("vault.write"));
    }
    bool canEvents() const
    {
        return m_granted.contains(QStringLiteral("vault.events"));
    }

    void logDenied(const char *method, const char *requiredToken) const;
};
```

Modify `libs/vault/src/proxies/VaultProxy.cpp` — update the ctor to wire signals when `vault.events` is granted:

```cpp
VaultProxy::VaultProxy(Vault *vault, const QSet<QString> &granted,
                       QString pluginId, QObject *parent)
    : QObject(parent),
      m_vault(vault),
      m_granted(granted),
      m_pluginId(std::move(pluginId))
{
    if (m_vault && canEvents()) {
        connect(m_vault, &Vault::created, this, &VaultProxy::created);
        connect(m_vault, &Vault::modified, this, &VaultProxy::modified);
        connect(m_vault, &Vault::deletedFile, this, &VaultProxy::deletedFile);
        connect(m_vault, &Vault::renamed, this, &VaultProxy::renamed);
    }
}
```

(If the existing ctor body did other work — e.g. subscription QHash setup — preserve that under the new QObject parent initialization.)

- [ ] **Step 4: Run test to verify it passes**

```bash
cmake --build build -j 10 --target tst_vault_proxy && cd build && ctest -R tst_vault_proxy --output-on-failure
```

Expected: both new test cases pass; existing cases still pass.

- [ ] **Step 5: Commit**

```bash
git add libs/vault/include/corbomite/vault/proxies/VaultProxy.h \
        libs/vault/src/proxies/VaultProxy.cpp \
        tests/vault/tst_vault_proxy.cpp
git commit -m "$(cat <<'EOF'
feat(vault): VaultProxy becomes QObject with forwarded signals

Enables Qt-signal subscription from plugin widgets, eliminating the
need for raw Vault* pointers. The vault.events permission gates the
signal wiring in the ctor — without it, the signals exist but never
fire.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 1.2: Create `SearchProxy` as the stable FTS/links/tags surface

**Files:**
- Create: `libs/storage/include/corbomite/storage/proxies/SearchProxy.h`
- Create: `libs/storage/src/proxies/SearchProxy.cpp`
- Modify: `libs/storage/CMakeLists.txt`
- Create: `tests/storage/tst_search_proxy.cpp`
- Modify: `tests/storage/CMakeLists.txt`

The proxy derives its surface from actual SQLiteIndex consumers: `search()` + `searchCompiled()` (SearchView), `backlinksFor` + `outlinksFor` + `allLinks` + `allTags` + `notesWithTag` (GraphDataBuilder). Internal methods (`reconcileWithCache`, `repairLinks`, `orphanLinks`) are not exposed — they have no plugin consumers and leak schema shape.

- [ ] **Step 1: Write the failing test for SearchProxy**

Create `tests/storage/tst_search_proxy.cpp`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/storage/proxies/SearchProxy.h"
#include "corbomite/storage/SQLiteIndex.h"
#include "corbomite/storage/MetadataCache.h"

#include <QObject>
#include <QSet>
#include <QTemporaryDir>
#include <QTest>

using namespace Corbomite;

class tst_search_proxy : public QObject
{
    Q_OBJECT
private slots:
    void permissionGating_searchDeniedReturnsEmpty();
    void permissionGating_searchGrantedForwards();
    void hidesInternalMethods_noOrphanOrRepair();

private:
    std::unique_ptr<QTemporaryDir> m_tmp;
};

void tst_search_proxy::permissionGating_searchDeniedReturnsEmpty()
{
    SQLiteIndex index;
    m_tmp = std::make_unique<QTemporaryDir>();
    QVERIFY(m_tmp->isValid());
    QVERIFY(index.open(m_tmp->filePath(QStringLiteral("idx.sqlite"))));

    QSet<QString> granted;  // no metadata.read
    SearchProxy proxy(&index, granted, QStringLiteral("test.plugin"));

    QCOMPARE(proxy.search(QStringLiteral("anything")).size(), 0);
    QCOMPARE(proxy.allTags().size(), 0);
    QCOMPARE(proxy.backlinksFor(QStringLiteral("x.md")).size(), 0);
}

void tst_search_proxy::permissionGating_searchGrantedForwards()
{
    SQLiteIndex index;
    m_tmp = std::make_unique<QTemporaryDir>();
    QVERIFY(m_tmp->isValid());
    QVERIFY(index.open(m_tmp->filePath(QStringLiteral("idx.sqlite"))));
    // An empty index will return empty results but should not error.

    QSet<QString> granted = { QStringLiteral("metadata.read") };
    SearchProxy proxy(&index, granted, QStringLiteral("test.plugin"));

    // Calls don't crash, return empty on empty index.
    QCOMPARE(proxy.search(QStringLiteral("x")).size(), 0);
    QCOMPARE(proxy.allTags().size(), 0);
    QCOMPARE(proxy.backlinksFor(QStringLiteral("x.md")).size(), 0);
    QCOMPARE(proxy.outlinksFor(QStringLiteral("x.md")).size(), 0);
    QCOMPARE(proxy.allLinks().size(), 0);
    QCOMPARE(proxy.notesWithTag(QStringLiteral("#t")).size(), 0);
    QCOMPARE(proxy.searchCompiled(QStringLiteral("x"), {}, {}).size(), 0);
}

void tst_search_proxy::hidesInternalMethods_noOrphanOrRepair()
{
    // Compile-time check: proxy should not expose orphanLinks / repairLinks.
    // If this test compiles, the proxy surface is what we expect.
    QSet<QString> granted = { QStringLiteral("metadata.read") };
    SearchProxy proxy(nullptr, granted, QStringLiteral("test.plugin"));
    // proxy.orphanLinks();          // ← must NOT compile
    // proxy.repairLinks(...);       // ← must NOT compile
    QVERIFY(true);  // reaches here iff the file compiled
}

QTEST_APPLESS_MAIN(tst_search_proxy)
#include "tst_search_proxy.moc"
```

- [ ] **Step 2: Run test to verify it fails**

```bash
cmake --build build -j 10 --target tst_search_proxy 2>&1 | tail -5
```

Expected: build failure — `SearchProxy.h` does not exist.

- [ ] **Step 3: Create SearchProxy header**

Create `libs/storage/include/corbomite/storage/proxies/SearchProxy.h`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>

#include "corbomite/storage/CachedMetadata.h"  // for SearchMatch / LinkInfo
#include "corbomite/storage/SQLiteIndex.h"      // brings in the POD types

namespace Corbomite {

class SQLiteIndex;

/// Permission-gated plugin-facing search / links / tags facade.
///
/// All methods gate on "metadata.read" and return empty collections when
/// the permission is absent. The proxy deliberately hides SQLiteIndex's
/// schema-shaped operations (reconcileWithCache, repairLinks, orphanLinks,
/// internal writes) — only the query surface is stable for plugins.
class SearchProxy
{
public:
    SearchProxy(SQLiteIndex *index, const QSet<QString> &granted,
                QString pluginId);

    SearchProxy(const SearchProxy &) = delete;
    SearchProxy &operator=(const SearchProxy &) = delete;

    // ---- FTS (gated by metadata.read) ----
    QVector<SearchMatch> search(const QString &query,
                                int maxResults = 100) const;
    QVector<SearchMatch> searchCompiled(const QString &fts5Query,
                                        const QStringList &requiredTags,
                                        const QStringList &excludedTags) const;

    // ---- Links (gated by metadata.read) ----
    QVector<LinkInfo> backlinksFor(const QString &targetPath) const;
    QVector<LinkInfo> outlinksFor(const QString &sourcePath) const;
    QVector<LinkInfo> allLinks() const;

    // ---- Tags (gated by metadata.read) ----
    QStringList allTags() const;
    QStringList notesWithTag(const QString &tag) const;

private:
    SQLiteIndex  *m_index;
    QSet<QString> m_granted;
    QString       m_pluginId;

    bool canRead() const
    {
        return m_granted.contains(QStringLiteral("metadata.read"));
    }

    void logDenied(const char *method) const;
};

} // namespace Corbomite
```

- [ ] **Step 4: Create SearchProxy implementation**

Create `libs/storage/src/proxies/SearchProxy.cpp`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/storage/proxies/SearchProxy.h"

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcSearchProxy, "corbomite.plugin.search-proxy")

namespace Corbomite {

SearchProxy::SearchProxy(SQLiteIndex *index, const QSet<QString> &granted,
                         QString pluginId)
    : m_index(index), m_granted(granted), m_pluginId(std::move(pluginId)) {}

void SearchProxy::logDenied(const char *method) const
{
    qCDebug(lcSearchProxy) << m_pluginId << "denied" << method
                           << "- missing metadata.read";
}

QVector<SearchMatch> SearchProxy::search(const QString &query, int maxResults) const
{
    if (!canRead()) { logDenied("search"); return {}; }
    if (!m_index)   return {};
    return m_index->search(query, maxResults);
}

QVector<SearchMatch> SearchProxy::searchCompiled(const QString &fts5Query,
                                                 const QStringList &requiredTags,
                                                 const QStringList &excludedTags) const
{
    if (!canRead()) { logDenied("searchCompiled"); return {}; }
    if (!m_index)   return {};
    return m_index->searchCompiled(fts5Query, requiredTags, excludedTags);
}

QVector<LinkInfo> SearchProxy::backlinksFor(const QString &targetPath) const
{
    if (!canRead()) { logDenied("backlinksFor"); return {}; }
    if (!m_index)   return {};
    return m_index->backlinksFor(targetPath);
}

QVector<LinkInfo> SearchProxy::outlinksFor(const QString &sourcePath) const
{
    if (!canRead()) { logDenied("outlinksFor"); return {}; }
    if (!m_index)   return {};
    return m_index->outlinksFor(sourcePath);
}

QVector<LinkInfo> SearchProxy::allLinks() const
{
    if (!canRead()) { logDenied("allLinks"); return {}; }
    if (!m_index)   return {};
    return m_index->allLinks();
}

QStringList SearchProxy::allTags() const
{
    if (!canRead()) { logDenied("allTags"); return {}; }
    if (!m_index)   return {};
    return m_index->allTags();
}

QStringList SearchProxy::notesWithTag(const QString &tag) const
{
    if (!canRead()) { logDenied("notesWithTag"); return {}; }
    if (!m_index)   return {};
    return m_index->notesWithTag(tag);
}

} // namespace Corbomite
```

- [ ] **Step 5: Wire into CMake**

Modify `libs/storage/CMakeLists.txt` to include the new source:

```cmake
target_sources(storage PRIVATE
    # … existing sources …
    src/proxies/SearchProxy.cpp
)
```

(If the existing pattern globs sources, ensure the new file is picked up; otherwise add explicit line as above.)

Modify `tests/storage/CMakeLists.txt` to register the new test — match the pattern used by existing tests in that file (e.g. `tst_sqliteindex`):

```cmake
ecm_add_test(tst_search_proxy.cpp
    LINK_LIBRARIES Qt6::Test Corbomite::Storage
    TEST_NAME tst_search_proxy
)
```

- [ ] **Step 6: Run tests to verify they pass**

```bash
cmake -B build -DCORBOMITE_DEV_BUILD=ON > /dev/null && \
cmake --build build -j 10 --target tst_search_proxy && \
cd build && ctest -R tst_search_proxy --output-on-failure
```

Expected: all three test cases pass.

- [ ] **Step 7: Commit**

```bash
git add libs/storage/include/corbomite/storage/proxies/SearchProxy.h \
        libs/storage/src/proxies/SearchProxy.cpp \
        libs/storage/CMakeLists.txt \
        tests/storage/tst_search_proxy.cpp \
        tests/storage/CMakeLists.txt
git commit -m "$(cat <<'EOF'
feat(storage): SearchProxy — stable FTS/links/tags plugin facade

Permission-gated on metadata.read. Hides SQLiteIndex schema-shaped
methods (reconcileWithCache, repairLinks, orphanLinks). Surface derived
from actual SQLiteIndex consumers: SearchView (search/searchCompiled)
and GraphDataBuilder (allLinks, allTags, notesWithTag, backlinks/outlinks).

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 1.3: Verify `MetadataCacheReader` needs no gap-fill

**Files:** only read; may modify `libs/storage/include/corbomite/storage/proxies/MetadataCacheReader.h` if gap found.

- [ ] **Step 1: Audit the four built-in plugins for MetadataCache method usage**

```bash
grep -rn "m_metadata\|metadata->\|m_metadataCache\|metadataCache->" \
    src/plugins/local-graph/ src/plugins/graph-view/ \
    src/plugins/search/ src/plugins/file-explorer/ 2>/dev/null
```

Compare calls against the existing `MetadataCacheReader` public surface:
- `backlinksFor(path)` → `QStringList`
- `outlinksFor(path)` → `QStringList`
- `tagsIn(path)` → `QStringList`
- `allTags()` → `QStringList`
- `frontmatterFor(path)` → `QJsonObject`
- signals: `cacheChanged`, `cacheDeleted`, `linksResolvedFor`, `allLinksResolved`, `indexFinished`

- [ ] **Step 2: Decide action**

- If the audit finds only calls already covered by `MetadataCacheReader`: no change needed. Document this in the task commit message as "no-op after audit" and move on.
- If the audit finds an uncovered call: add the method to `MetadataCacheReader` with a matching test in `tests/storage/tst_metadata_cache_reader.cpp` following the same TDD pattern as Task 1.2.

- [ ] **Step 3: Commit (no-op or gap-fill)**

If no gap:

```bash
git commit --allow-empty -m "chore(vault): MetadataCacheReader surface verified — no gap-fill needed"
```

If gap-fill applied, commit the additions with a message describing what was added and why.

---

## Task 1.4: Add `SearchProxy` accessor to `PluginContext`

**Files:**
- Modify: `libs/vault/include/corbomite/vault/PluginContext.h`
- Modify: `libs/vault/src/PluginContext.cpp`
- Modify: `libs/vault/CMakeLists.txt`
- Modify: `tests/core/tst_plugin_context.cpp`

- [ ] **Step 1: Write the failing test for `PluginContext::search()`**

Append to `tests/core/tst_plugin_context.cpp`:

```cpp
void tst_plugin_context::searchAccessorLazyConstructsWhenGranted()
{
    PluginMetaData meta;
    meta.setPluginId(QStringLiteral("test.plugin"));

    QSet<QString> granted = { QStringLiteral("metadata.read") };
    PluginContext ctx(meta, granted);

    SQLiteIndex index;  // not opened — proxy should still be constructible
    ctx.setCoreServices(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                        nullptr, nullptr);
    ctx.setSearchIndex(&index);  // still using the old wire path for now

    auto *proxy = ctx.search();
    QVERIFY(proxy != nullptr);
    QCOMPARE(ctx.search(), proxy);  // idempotent: same pointer on second call
}

void tst_plugin_context::searchAccessorReturnsNullWhenPermissionDenied()
{
    PluginMetaData meta;
    meta.setPluginId(QStringLiteral("test.plugin"));
    QSet<QString> granted;  // no metadata.read
    PluginContext ctx(meta, granted);

    SQLiteIndex index;
    ctx.setCoreServices(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                        nullptr, nullptr);
    ctx.setSearchIndex(&index);

    QVERIFY(ctx.search() == nullptr);
}
```

Declare the new slots in the test class's `private slots:` section.

- [ ] **Step 2: Run to verify fail**

```bash
cmake --build build -j 10 --target tst_plugin_context 2>&1 | tail -5
```

Expected: compile failure — `PluginContext::search()` does not exist.

- [ ] **Step 3: Add the accessor to `PluginContext.h`**

In the public-accessor block (near `VaultProxy *vault() const;`):

```cpp
SearchProxy           *search() const;           // "metadata.read"
```

And in the forward-declarations block at the top of the header:

```cpp
class SearchProxy;
```

And in the private member block:

```cpp
mutable SearchProxy         *m_searchProxy = nullptr;
```

- [ ] **Step 4: Implement the accessor in `PluginContext.cpp`**

```cpp
#include "corbomite/storage/proxies/SearchProxy.h"

// … elsewhere in the file …

SearchProxy *PluginContext::search() const
{
    if (!m_searchIndex) return nullptr;
    if (!m_granted.contains(QStringLiteral("metadata.read"))) return nullptr;
    if (!m_searchProxy) {
        m_searchProxy = new SearchProxy(m_searchIndex, m_granted,
                                        m_meta.pluginId());
    }
    return m_searchProxy;
}
```

And in the destructor, delete the owned proxy:

```cpp
PluginContext::~PluginContext()
{
    // … existing cleanup …
    delete m_searchProxy;
}
```

- [ ] **Step 5: Update `libs/vault/CMakeLists.txt` to link against `Corbomite::Storage`**

Verify the link-line already includes it (Q.0 Phase 9 added this dep); if not:

```cmake
target_link_libraries(vault PUBLIC Corbomite::Core Corbomite::Storage ...)
```

- [ ] **Step 6: Run tests**

```bash
cmake --build build -j 10 --target tst_plugin_context && \
cd build && ctest -R tst_plugin_context --output-on-failure
```

Expected: both new cases plus existing cases pass.

- [ ] **Step 7: Commit**

```bash
git add libs/vault/include/corbomite/vault/PluginContext.h \
        libs/vault/src/PluginContext.cpp \
        libs/vault/CMakeLists.txt \
        tests/core/tst_plugin_context.cpp
git commit -m "$(cat <<'EOF'
feat(vault): PluginContext::search() accessor

Lazy-constructs SearchProxy gated on metadata.read. Stop-gap
searchIndex() / setSearchIndex() accessors remain temporarily for
existing consumers; they are removed after the plugin migrations in
Phase 2.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

# Phase 2 — Plugin Widget Migration

Goal: migrate the four stop-gap-reaching plugins onto the new proxy surfaces. Delete the stop-gap accessors.

## Task 2.1: Migrate `SearchPlugin` + `SearchView` onto `SearchProxy`

**Files:**
- Modify: `src/plugins/search/SearchView.h`
- Modify: `src/plugins/search/SearchView.cpp`
- Modify: `src/plugins/search/SearchPlugin.cpp`
- Modify: `src/plugins/search/tests/tst_search_plugin.cpp`

- [ ] **Step 1: Write the failing test — `SearchPlugin` uses proxy**

Update `src/plugins/search/tests/tst_search_plugin.cpp` — find the test case that installs `setSearchIndex(&index)` and assert the plugin reaches the proxy through `ctx.search()`. Rewrite the assertions to match the new ctor:

```cpp
void tst_search_plugin::createViewUsesSearchProxy()
{
    MetadataCache cache;
    Workspace ws;
    PluginMetaData meta;
    meta.setPluginId(QStringLiteral("corbomite.search"));
    PluginContext ctx(meta, {QStringLiteral("metadata.read"), QStringLiteral("ui.views")});
    SQLiteIndex index;
    ctx.setCoreServices(nullptr, nullptr, &cache, &ws, nullptr, nullptr,
                        nullptr, nullptr);
    ctx.setSearchIndex(&index);

    SearchPlugin plugin;
    plugin.loadAdapter(&ctx);  // or whatever the existing test uses

    MainWindow mw;  // existing test may use a stub
    auto *view = plugin.createView(&mw);
    QVERIFY(view != nullptr);
    // view must have been constructed with a SearchProxy *, not a SQLiteIndex *
}
```

Note: if the existing test already checks `createView` works, just ensure it still passes after the ctor change below.

- [ ] **Step 2: Change `SearchView` ctor signature**

Modify `src/plugins/search/SearchView.h`:

```cpp
#pragma once

#include <QWidget>

namespace Corbomite {
class SearchProxy;
class MetadataCacheReader;
class WorkspaceController;

class SearchView : public QWidget
{
    Q_OBJECT
public:
    SearchView(SearchProxy *search,
               MetadataCacheReader *metadata,
               WorkspaceController *workspace,
               QWidget *parent = nullptr);
    ~SearchView() override;

    void focusSearchInput();

private:
    SearchProxy         *m_search = nullptr;
    MetadataCacheReader *m_metadata = nullptr;
    WorkspaceController *m_workspace = nullptr;
    // … rest of private state (UI widgets, etc.)
};

} // namespace Corbomite
```

Modify `src/plugins/search/SearchView.cpp`:
- Replace the ctor's `SQLiteIndex *index` parameter with `SearchProxy *search`.
- Rename `m_index` → `m_search`.
- Update the two calls that previously did `m_index->search(query)` / `m_index->searchCompiled(...)` to call `m_search->search(query)` / `m_search->searchCompiled(...)`. The return types and arguments are identical; it's a mechanical rename.

- [ ] **Step 3: Update `SearchPlugin::createView`**

```cpp
QObject *SearchPlugin::createView(MainWindow *mainWindow)
{
    auto *ctx = context();
    if (!ctx) return nullptr;
    auto *search = ctx->search();
    auto *metadata = ctx->metadataCache();
    if (!search || !metadata) {
        qWarning() << "SearchPlugin: metadata.read missing; view skipped";
        return nullptr;
    }
    return new SearchView(search, metadata, ctx->workspace(),
                            reinterpret_cast<QWidget *>(mainWindow));
}
```

- [ ] **Step 4: Add the `Corbomite::Storage` dep to the plugin's CMakeLists**

Modify `src/plugins/search/CMakeLists.txt` — the plugin target needs to link `Corbomite::Storage` now that `SearchView.h` transitively pulls in `SearchProxy.h`:

```cmake
target_link_libraries(corbomitesearch PRIVATE
    # … existing deps …
    Corbomite::Storage
)
```

(Depending on how the existing linkage reaches SQLiteIndex, this may already be in place — verify and skip if so.)

- [ ] **Step 5: Build + run plugin tests**

```bash
cmake --build build -j 10 --target tst_search_plugin && \
cd build && ctest -R tst_search_plugin --output-on-failure
```

Expected: pass.

- [ ] **Step 6: Commit**

```bash
git add src/plugins/search/
git commit -m "$(cat <<'EOF'
refactor(plugin): SearchView migrates onto SearchProxy

Replaces raw SQLiteIndex* with SearchProxy*. SearchPlugin switches
from ctx->searchIndex() to ctx->search(). No behavior change; this
is the first step toward deleting PluginContext::searchIndex().

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 2.2: Add `GraphDataBuilder` overloads accepting proxy pointers

The `buildGlobalGraph(SQLiteIndex*, Vault*)` and `buildLocalGraph(SQLiteIndex*, Vault*, path, depth)` free functions are shared between the graph-view and local-graph plugins. We add proxy-accepting overloads rather than replacing the existing signatures, because the host also uses these internally (MainWindow's own graph wiring, any non-plugin caller). After Phase 2 completes, audit whether the raw-typed overloads have any remaining callers; if none, delete them.

**Files:**
- Modify: `src/graph/GraphDataBuilder.h`
- Modify: `src/graph/GraphDataBuilder.cpp`
- Modify: `tests/graph/tst_graphdatabuilder.cpp` (or wherever the existing graph tests live)

- [ ] **Step 1: Write failing test**

Add test cases covering the new overloads — they should produce identical output to the raw-typed versions given the same backing data. Structure:

```cpp
void tst_graphdatabuilder::globalGraphProxyOverloadMatchesRaw()
{
    // Build same graph two ways, compare.
    SQLiteIndex index;
    // … set up index with some nodes/links …
    Vault vault;
    // … set up vault with same files …

    auto raw = GraphDataBuilder::buildGlobalGraph(&index, &vault);

    QSet<QString> granted = { QStringLiteral("vault.read"),
                              QStringLiteral("metadata.read") };
    VaultProxy    vaultProxy(&vault, granted, QStringLiteral("t"));
    SearchProxy   searchProxy(&index, granted, QStringLiteral("t"));

    auto proxied = GraphDataBuilder::buildGlobalGraph(&searchProxy, &vaultProxy);

    QCOMPARE(raw.nodes.size(), proxied.nodes.size());
    QCOMPARE(raw.edges.size(), proxied.edges.size());
    // Compare node paths + edge endpoints in sorted order.
}
```

(Add an analogous test for `buildLocalGraph`.)

- [ ] **Step 2: Run to verify fail**

```bash
cmake --build build -j 10 --target tst_graphdatabuilder 2>&1 | tail -5
```

- [ ] **Step 3: Add the overloads**

Modify `src/graph/GraphDataBuilder.h` — add declarations matching the existing ones:

```cpp
namespace Corbomite {

class VaultProxy;
class SearchProxy;

class GraphDataBuilder
{
public:
    // Existing raw-typed overloads stay …

    // New proxy-typed overloads (added in Cluster N for plugin use)
    static GraphData buildGlobalGraph(SearchProxy *search, VaultProxy *vault);
    static GraphData buildLocalGraph(SearchProxy *search, VaultProxy *vault,
                                     const QString &center, int depth);
};

} // namespace Corbomite
```

Modify `src/graph/GraphDataBuilder.cpp` — implement by duplicating the raw-typed logic, replacing `index->method()` with `search->method()` and `vault->method()` with `vaultProxy->method()`. Every method invoked on the raw types in the existing implementation has a matching proxy method after Phase 1:

- `index->allLinks()` → `search->allLinks()`
- `index->allTags()` → `search->allTags()`
- `index->notesWithTag(t)` → `search->notesWithTag(t)`
- `index->outlinksFor(p)` → `search->outlinksFor(p)`
- `index->backlinksFor(p)` → `search->backlinksFor(p)`
- `vault->getMarkdownFiles()` → `vaultProxy->getMarkdownFiles()`
- etc.

Consider extracting the shared logic into a template parameterized on the two types. For this plan, keep it simple: copy-paste, rename identifiers. Refactoring to template form is a micro-cleanup that can happen after the migration proves out.

- [ ] **Step 4: Run tests**

```bash
cmake --build build -j 10 --target tst_graphdatabuilder && \
cd build && ctest -R tst_graphdatabuilder --output-on-failure
```

Expected: new cases pass; existing cases still pass.

- [ ] **Step 5: Commit**

```bash
git add src/graph/
git commit -m "$(cat <<'EOF'
feat(graph): GraphDataBuilder proxy-typed overloads

Adds buildGlobalGraph(SearchProxy*, VaultProxy*) and the local-graph
equivalent. Semantics match the existing raw-typed overloads. Unblocks
LocalGraphPlugin and GraphViewPlugin's migration onto plugin proxies.
Raw-typed overloads stay until callers are audited in Task 2.5.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 2.3: Migrate `LocalGraphPlugin` + `LocalGraphView`

**Files:**
- Modify: `src/plugins/local-graph/LocalGraphView.{h,cpp}`
- Modify: `src/plugins/local-graph/LocalGraphPlugin.cpp`
- Modify: `src/plugins/local-graph/tests/tst_localgraph_plugin.cpp`
- Modify: `src/plugins/local-graph/CMakeLists.txt` (if needed)

- [ ] **Step 1: Update test**

Switch the test's ctor call to pass proxies instead of raw pointers. Structure:

```cpp
QSet<QString> granted = { ... };
VaultProxy  vaultProxy(&vault, granted, QStringLiteral("local-graph"));
SearchProxy searchProxy(&index, granted, QStringLiteral("local-graph"));
ctx.setCoreServices(...);
ctx.setSearchIndex(&index);
// Call plugin->createView; assert result non-null.
```

- [ ] **Step 2: Run to verify fail**

- [ ] **Step 3: Change `LocalGraphView` ctor**

Modify the header to take `SearchProxy *` + `VaultProxy *` instead of `SQLiteIndex *` + `Vault *`. Member types follow.

Modify `LocalGraphView.cpp` — update the body:
- Rename `m_index` → `m_search`, `m_vault` → `m_vault` (stays — but now `VaultProxy *`).
- Replace `GraphDataBuilder::buildLocalGraph(m_index, m_vault, ...)` with the proxy-typed overload `GraphDataBuilder::buildLocalGraph(m_search, m_vault, ...)`.

- [ ] **Step 4: Update `LocalGraphPlugin::createView`**

```cpp
QObject *LocalGraphPlugin::createView(MainWindow *mainWindow)
{
    auto *ctx = context();
    if (!ctx) return nullptr;
    auto *vault = ctx->vault();
    auto *search = ctx->search();
    auto *metadata = ctx->metadataCache();
    if (!vault || !search || !metadata) {
        qWarning() << "LocalGraphPlugin: vault.read+metadata.read missing";
        return nullptr;
    }
    return new LocalGraphView(search, vault, metadata, ctx->workspace(),
                                reinterpret_cast<QWidget *>(mainWindow));
}
```

- [ ] **Step 5: Ensure `Corbomite::Storage` link**

Same check as Task 2.1 Step 4.

- [ ] **Step 6: Run tests**

- [ ] **Step 7: Commit**

```bash
git commit -m "$(cat <<'EOF'
refactor(plugin): LocalGraphView migrates onto proxy surfaces

Ctor now takes SearchProxy* + VaultProxy* instead of SQLiteIndex* +
Vault*. Uses GraphDataBuilder's proxy-typed overload.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 2.4: Migrate `GraphViewPlugin` + `GraphView` + `GraphViewTab`

**Files:**
- Modify: `src/plugins/graph-view/GraphView.{h,cpp}`
- Modify: `src/plugins/graph-view/GraphViewTab.{h,cpp}`
- Modify: `src/plugins/graph-view/GraphViewPlugin.{h,cpp}`
- Modify: `src/plugins/graph-view/tests/tst_graphview_plugin.cpp`

Before migrating: verify whether `GraphView::setMetadataCache` / the stored `m_metadata` is actually used anywhere in `GraphView.cpp` or `GraphViewTab.cpp`. If it's dead-stored (set but never read), drop the setter and member entirely as part of this task.

- [ ] **Step 1: Audit metadata usage**

```bash
grep -n "m_metadata\b" src/plugins/graph-view/GraphView.cpp \
                      src/plugins/graph-view/GraphView.h \
                      src/plugins/graph-view/GraphViewTab.cpp \
                      src/plugins/graph-view/GraphViewTab.h 2>&1
```

Record what you find. Likely outcome: `m_metadata` is set but never read (because GraphDataBuilder uses SQLiteIndex for all graph shapes). If so, delete it.

- [ ] **Step 2: Update tests**

Rewrite the test setup to construct proxies and pass them through. If `setMetadataCache` is being dropped, remove the `ctx.setCoreServices(..., &cache, ...)` wiring that was solely feeding it.

- [ ] **Step 3: Run to verify fail**

- [ ] **Step 4: Change `GraphView` and `GraphViewTab` ctors**

```cpp
// GraphViewTab.h
class GraphViewTab : public QWidget {
    Q_OBJECT
public:
    explicit GraphViewTab(SearchProxy *search, VaultProxy *vault,
                          QWidget *parent = nullptr);
    // …
private:
    SearchProxy *m_search;
    VaultProxy  *m_vault;
};
```

```cpp
// GraphView.h
class GraphView : public View {
public:
    // …
    void setSearch(SearchProxy *s);
    void setVault(VaultProxy *v);
    // setMetadataCache DELETED if Step 1 showed it's dead-stored
private:
    SearchProxy *m_search = nullptr;
    VaultProxy  *m_vault = nullptr;
};
```

Update `GraphView.cpp` similarly; replace `GraphDataBuilder::buildGlobalGraph(m_index, m_vault)` with the proxy-typed overload.

- [ ] **Step 5: Update `GraphViewPlugin`**

```cpp
void GraphViewPlugin::onLoad(PluginContext *ctx)
{
    if (!ctx) return;
    m_search = ctx->search();
    m_vault  = ctx->vault();

    if (auto *views = ctx->views()) {
        views->registerView(QStringLiteral("graph"),
            [this](WorkspaceLeaf *leaf) -> View * {
                auto *view = new GraphView(leaf);
                view->setSearch(m_search);
                view->setVault(m_vault);
                if (m_controlsPanel) view->setControlsPanel(m_controlsPanel);
                return view;
            });
    }
}
```

Update the header's private members to match (`SearchProxy *m_search;`, `VaultProxy *m_vault;`, remove `m_metadata`).

- [ ] **Step 6: Run tests**

- [ ] **Step 7: Commit**

```bash
git commit -m "$(cat <<'EOF'
refactor(plugin): GraphView{,Tab,Plugin} migrate onto proxies

GraphView and GraphViewTab ctors now take SearchProxy* + VaultProxy*.
GraphViewPlugin captures proxies via ctx->search() / ctx->vault().
Unused metadata-cache pointer dropped (GraphDataBuilder derives all
needed data from SearchProxy + VaultProxy).

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 2.5: Migrate `FileExplorerPlugin` + `FileExplorerView` + `NotesTreeModel`

**Files:**
- Modify: `libs/models/include/corbomite/models/NotesTreeModel.h`
- Modify: `libs/models/src/NotesTreeModel.cpp`
- Modify: `src/plugins/file-explorer/FileExplorerView.{h,cpp}`
- Modify: `src/plugins/file-explorer/FileExplorerPlugin.cpp`
- Modify: `src/plugins/file-explorer/tests/tst_fileexplorer_plugin.cpp`
- Modify: `libs/models/CMakeLists.txt` (likely needs `Corbomite::Vault` explicit link if not already)

This is the most mechanically involved migration: `NotesTreeModel` subscribes via `QObject::connect` to `Vault::created/deletedFile/renamed`, which is exactly the reason `VaultProxy` was QObject-ified in Task 1.1.

- [ ] **Step 1: Update test**

Reshape the test's plugin setup to pass `VaultProxy *` to `FileExplorerView`.

- [ ] **Step 2: Run to verify fail**

- [ ] **Step 3: Change `NotesTreeModel` to accept `VaultProxy *`**

Modify `NotesTreeModel.h`:

```cpp
namespace Corbomite {
class VaultProxy;
class TFile;
class TFolder;

class NotesTreeModel : public QAbstractItemModel
{
    Q_OBJECT
public:
    explicit NotesTreeModel(VaultProxy *vault, QObject *parent = nullptr);
    // …
private:
    VaultProxy *m_vault;
};
}
```

Modify `NotesTreeModel.cpp`:
- Ctor body: `connect(m_vault, &VaultProxy::created, this, &NotesTreeModel::onCreated);` etc.
- Any call that was `m_vault->getFiles()` etc. stays — `VaultProxy::getFiles()` exists and returns the same `QVector<TFile*>`.

- [ ] **Step 4: Change `FileExplorerView` ctor signature**

Replace `Vault *` with `VaultProxy *` in the ctor parameter and member. The 3 call sites in the current `.cpp` (`m_vault->getFolderByPath(...)`, `m_vault->getAbstractFileByPath(...)`) all have matching `VaultProxy::` methods — no further changes needed.

- [ ] **Step 5: Update `FileExplorerPlugin::createView`**

```cpp
QObject *FileExplorerPlugin::createView(MainWindow *mainWindow)
{
    auto *ctx = context();
    if (!ctx) return nullptr;
    auto *vault = ctx->vault();
    auto *fileManager = ctx->fileManager();
    if (!vault || !fileManager) {
        qWarning() << "FileExplorerPlugin: vault.read+vault.write missing";
        return nullptr;
    }
    return new FileExplorerView(vault, fileManager, ctx->workspace(),
                                  reinterpret_cast<QWidget *>(mainWindow));
}
```

- [ ] **Step 6: Confirm `libs/models` links against `Corbomite::Vault` for the proxy header**

Check `libs/models/CMakeLists.txt`; if `Corbomite::Vault` isn't in the PUBLIC link line, add it. (Should already be there from Q.0 Phase 7 — `NotesTreeModel` has subscribed to `Vault` signals since then.)

- [ ] **Step 7: Run tests**

```bash
cmake --build build -j 10 && cd build && ctest -j 10 --output-on-failure
```

Expected: full suite green (or matches pre-cluster baseline on the known-flaky list).

- [ ] **Step 8: Commit**

```bash
git commit -m "$(cat <<'EOF'
refactor(plugin): FileExplorerView + NotesTreeModel migrate onto VaultProxy

NotesTreeModel ctor now takes VaultProxy*; subscribes to forwarded
Qt signals via QObject::connect. FileExplorerView and FileExplorerPlugin
follow. Exercises VaultProxy's QObject promotion from Task 1.1.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 2.6: Delete the stop-gap accessors from `PluginContext`

**Files:**
- Modify: `libs/vault/include/corbomite/vault/PluginContext.h`
- Modify: `libs/vault/src/PluginContext.cpp`
- Modify: `src/app/MainWindow.cpp` (host wiring of `setSearchIndex` disappears with the accessor)
- Modify: `tests/core/tst_plugin_context.cpp`

Before starting: confirm no plugin still uses `vaultRaw()` / `metadataCacheRaw()` / `searchIndex()`.

- [ ] **Step 1: Verify zero callers remain**

```bash
grep -rn "vaultRaw\|metadataCacheRaw\|searchIndex()" \
    src/plugins/ libs/ tests/ 2>&1 | grep -v "spec\|plan\|retro\|audit"
```

Expected: only references inside `PluginContext.{h,cpp}` and possibly `MainWindow.cpp`'s `setSearchIndex` call. If any plugin source shows a call, go fix that plugin and rerun.

- [ ] **Step 2: Delete the declarations**

Remove from `PluginContext.h`:
- `void setSearchIndex(SQLiteIndex *index) { ... }` (inline definition)
- `SQLiteIndex *searchIndex() const;`
- `Vault *vaultRaw() const;`
- `MetadataCache *metadataCacheRaw() const;`
- The `SQLiteIndex *m_searchIndex = nullptr;` member — replaced by a new `void setSearchIndex(SQLiteIndex *)` helper retained internally (because `search()` still needs to reach an `SQLiteIndex *` to construct the proxy).

Actually, *do* retain `setSearchIndex` (it's the host's plumbing hook), but *drop* it from the plugin-facing doc comments and move its declaration into a `// Host wiring — not part of plugin API` section. The method is non-public-to-plugins only by convention; since plugins only see `PluginContext` via `ctx->...`, they can't call it in practice.

Simpler alternative: fold `setSearchIndex`'s job into `setCoreServices` by adding a `SQLiteIndex *` parameter. This is a signature change to an already-frequently-edited method.

**Decision:** fold into `setCoreServices`. New signature:

```cpp
void setCoreServices(Vault *vault,
                     FileManager *fileManager,
                     MetadataCache *metadata,
                     SQLiteIndex *searchIndex,   // ← new, between metadata and workspace
                     Workspace *workspace,
                     CommandRegistry *commands,
                     ViewRegistry *views,
                     MenuEventEmitter *menus,
                     QNetworkAccessManager *network);
```

Update every call site in tests and in `MainWindow.cpp`'s configurator.

- [ ] **Step 3: Delete the implementations in `.cpp`**

Remove the three accessor definitions; update the `setCoreServices` body to store `m_searchIndex` alongside the other refs.

- [ ] **Step 4: Update every `setCoreServices` call site**

```bash
grep -rn "setCoreServices" src/ tests/ 2>&1
```

Visit each result; insert `&searchIndex` (or `&index`, `nullptr`, matching local var names) as the fourth argument.

- [ ] **Step 5: Update `tst_plugin_context.cpp`**

Delete the `searchIndexExposedWhen...` / `vaultRawExposed...` / `metadataRawExposed...` test cases if they exist. Replace with — or keep — the `searchAccessorLazy...` cases from Task 1.4 (those already exercise the `search()` path).

- [ ] **Step 6: Build + full test suite**

```bash
cmake --build build -j 10 && cd build && ctest -j 10 --output-on-failure
```

Expected: green outside pre-existing known-flaky tests.

- [ ] **Step 7: Commit**

```bash
git add libs/vault/ src/app/MainWindow.cpp tests/
git commit -m "$(cat <<'EOF'
refactor(vault): delete vaultRaw / metadataCacheRaw / searchIndex stop-gaps

All four plugins that reached through the raw-pointer escape hatches
now route through proxies. PluginContext's plugin-facing API is a
single surface: VaultProxy, FileManagerProxy, MetadataCacheReader,
SearchProxy, WorkspaceController, CommandRegistrar, ViewRegistrar,
MenuInjector, SecretStorage, ProcessSpawner, QNetworkAccessManager.

setCoreServices absorbs the former setSearchIndex() hook as its
fourth parameter.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 2.7: Audit `GraphDataBuilder`'s raw-typed overloads for remaining callers

**Files:** possibly modify `src/graph/GraphDataBuilder.{h,cpp}` + removed callers.

- [ ] **Step 1: Find remaining callers of the raw-typed overloads**

```bash
grep -rn "buildGlobalGraph(" src/ libs/ tests/ | grep -v "SearchProxy"
grep -rn "buildLocalGraph(" src/ libs/ tests/ | grep -v "SearchProxy"
```

Expected: zero non-test hits outside the plugin code (which has been migrated). If the only remaining raw-typed callers are tests in `tests/graph/`, those can migrate trivially to proxies too.

- [ ] **Step 2: Either delete the raw overloads or keep them with rationale**

If zero production callers: delete the raw-typed overloads. Update the last tests. Commit as:

```
refactor(graph): remove raw-typed GraphDataBuilder overloads

No remaining callers after Cluster N plugin migrations. Proxy-typed
overloads are the single implementation.
```

If production callers remain: document why they stay (they're host-side code with no plugin relationship, and re-routing them through proxies would introduce artificial permission-set-up noise). Commit `--allow-empty` with the rationale.

---

# Phase 3 — Persistence

## Task 3.1: QtKeychain-backed `SecretStorage`

**Files:**
- Modify: `src/core/SecretStorage.h` (or wherever the class lives — verify path first)
- Modify: `src/core/SecretStorage.cpp`
- Modify: `CMakeLists.txt` (add `find_package(Qt6Keychain CONFIG)` behind `CORBOMITE_USE_KEYRING`)
- Modify: `tests/core/tst_secret_storage.cpp`

- [ ] **Step 1: Locate `SecretStorage`**

```bash
find . -name "SecretStorage*" -not -path "./build*"
```

Record the paths — the task below assumes standard locations; correct if they differ.

- [ ] **Step 2: Add CMake option + package discovery**

In the root `CMakeLists.txt`:

```cmake
option(CORBOMITE_USE_KEYRING "Use qt6keychain for SecretStorage" ON)
if(CORBOMITE_USE_KEYRING)
    find_package(Qt6Keychain CONFIG)
    if(NOT Qt6Keychain_FOUND)
        message(STATUS "Qt6Keychain not found; SecretStorage will use in-process QHash fallback")
        set(CORBOMITE_USE_KEYRING OFF CACHE BOOL "" FORCE)
    endif()
endif()
if(CORBOMITE_USE_KEYRING)
    add_compile_definitions(CORBOMITE_HAVE_KEYRING=1)
endif()
```

- [ ] **Step 3: Write failing test**

Append to `tests/core/tst_secret_storage.cpp`:

```cpp
void tst_secret_storage::usesKeyringWhenAvailable()
{
#ifdef CORBOMITE_HAVE_KEYRING
    SecretStorage s(QStringLiteral("corbomite.test.plugin"), {QStringLiteral("secrets")});
    const QString key = QStringLiteral("api-token");
    const QString val = QStringLiteral("test-value-") + QString::number(QDateTime::currentSecsSinceEpoch());

    QVERIFY(s.setSecret(key, val));
    QCOMPARE(s.secret(key), val);
    QVERIFY(s.deleteSecret(key));
    QVERIFY(s.secret(key).isEmpty());
#else
    QSKIP("Qt6Keychain not available; skipping keyring path test");
#endif
}

void tst_secret_storage::usesQHashFallbackWhenDenied()
{
    SecretStorage s(QStringLiteral("corbomite.test.plugin"), {});  // no secrets permission
    QCOMPARE(s.setSecret(QStringLiteral("k"), QStringLiteral("v")), false);
    QVERIFY(s.secret(QStringLiteral("k")).isEmpty());
}
```

- [ ] **Step 4: Run to fail**

- [ ] **Step 5: Implement keyring backend**

In `SecretStorage.cpp`:

```cpp
#include "SecretStorage.h"

#ifdef CORBOMITE_HAVE_KEYRING
#include <qt6keychain/keychain.h>
using namespace QKeychain;
#endif

#include <QEventLoop>

namespace Corbomite {

bool SecretStorage::setSecret(const QString &key, const QString &value)
{
    if (!m_granted.contains(QStringLiteral("secrets"))) return false;
#ifdef CORBOMITE_HAVE_KEYRING
    WritePasswordJob job(QStringLiteral("corbomite-plugin"));
    job.setAutoDelete(false);
    job.setKey(m_pluginId + QLatin1Char('.') + key);
    job.setTextData(value);
    QEventLoop loop;
    QObject::connect(&job, &Job::finished, &loop, &QEventLoop::quit);
    job.start();
    loop.exec();
    if (job.error() == NoError) return true;
    qCWarning(lcSecretStorage) << "keyring write failed:" << job.errorString()
                               << "falling back to in-process QHash";
#endif
    m_fallback.insert(key, value);
    return true;
}

QString SecretStorage::secret(const QString &key) const
{
    if (!m_granted.contains(QStringLiteral("secrets"))) return {};
#ifdef CORBOMITE_HAVE_KEYRING
    ReadPasswordJob job(QStringLiteral("corbomite-plugin"));
    job.setAutoDelete(false);
    job.setKey(m_pluginId + QLatin1Char('.') + key);
    QEventLoop loop;
    QObject::connect(&job, &Job::finished, &loop, &QEventLoop::quit);
    job.start();
    loop.exec();
    if (job.error() == NoError) return job.textData();
#endif
    return m_fallback.value(key);
}

bool SecretStorage::deleteSecret(const QString &key)
{
    if (!m_granted.contains(QStringLiteral("secrets"))) return false;
#ifdef CORBOMITE_HAVE_KEYRING
    DeletePasswordJob job(QStringLiteral("corbomite-plugin"));
    job.setAutoDelete(false);
    job.setKey(m_pluginId + QLatin1Char('.') + key);
    QEventLoop loop;
    QObject::connect(&job, &Job::finished, &loop, &QEventLoop::quit);
    job.start();
    loop.exec();
#endif
    m_fallback.remove(key);
    return true;
}

} // namespace Corbomite
```

Add `QHash<QString, QString> m_fallback;` and the private `m_granted` / `m_pluginId` members to `SecretStorage.h` if not present.

- [ ] **Step 6: Link keyring in the core/plugins CMakeLists where SecretStorage lives**

```cmake
if(CORBOMITE_USE_KEYRING)
    target_link_libraries(<target> PRIVATE Qt6Keychain::Qt6Keychain)
endif()
```

- [ ] **Step 7: Build + test**

```bash
cmake -B build -DCORBOMITE_DEV_BUILD=ON -DCORBOMITE_USE_KEYRING=ON && \
cmake --build build -j 10 && \
cd build && ctest -R tst_secret_storage --output-on-failure
```

If QtKeychain isn't installed on this machine, the guard flips to fallback silently and the keyring test cases `QSKIP`.

- [ ] **Step 8: Commit**

```bash
git add CMakeLists.txt src/core/SecretStorage.{h,cpp} tests/core/tst_secret_storage.cpp
git commit -m "$(cat <<'EOF'
feat(secrets): QtKeychain backend for SecretStorage

Persists plugin secrets to platform keyring (KWallet/GNOME Keyring/
macOS Keychain/Windows Credential Manager) via qt6keychain. Gated by
CORBOMITE_USE_KEYRING CMake option defaulting to ON; when qt6keychain
is unavailable or the option is off, SecretStorage falls back to its
in-process QHash with a qCWarning and 'will not persist' note.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 3.2: Obsidian-shape plugin `data.json` persistence

**Files:**
- Create: `libs/vault/include/corbomite/vault/PluginDataStore.h`
- Create: `libs/vault/src/PluginDataStore.cpp`
- Modify: `libs/vault/include/corbomite/vault/PluginContext.h` (add `loadData()` / `saveData()` / `setPluginDataDir()`)
- Modify: `libs/vault/src/PluginContext.cpp`
- Create: `tests/vault/tst_plugin_data_store.cpp`
- Modify: `src/app/MainWindow.cpp` (wire each PluginContext with its vault-scoped data dir)

- [ ] **Step 1: Write failing test**

Create `tests/vault/tst_plugin_data_store.cpp`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/vault/PluginDataStore.h"

#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

using namespace Corbomite;

class tst_plugin_data_store : public QObject
{
    Q_OBJECT
private slots:
    void roundTrip();
    void emptyOnMissing();
    void atomicOverwrite();
};

void tst_plugin_data_store::roundTrip()
{
    QTemporaryDir dir; QVERIFY(dir.isValid());
    PluginDataStore store(dir.path());

    QJsonObject payload;
    payload.insert(QStringLiteral("count"), 42);
    payload.insert(QStringLiteral("label"), QStringLiteral("hello"));

    QVERIFY(store.save(payload));
    QJsonObject out = store.load();
    QCOMPARE(out.value(QStringLiteral("count")).toInt(), 42);
    QCOMPARE(out.value(QStringLiteral("label")).toString(), QStringLiteral("hello"));
}

void tst_plugin_data_store::emptyOnMissing()
{
    QTemporaryDir dir; QVERIFY(dir.isValid());
    PluginDataStore store(dir.path());
    QCOMPARE(store.load().size(), 0);
}

void tst_plugin_data_store::atomicOverwrite()
{
    QTemporaryDir dir; QVERIFY(dir.isValid());
    PluginDataStore store(dir.path());
    store.save(QJsonObject{ {QStringLiteral("v"), 1} });
    store.save(QJsonObject{ {QStringLiteral("v"), 2} });
    QCOMPARE(store.load().value(QStringLiteral("v")).toInt(), 2);
}

QTEST_APPLESS_MAIN(tst_plugin_data_store)
#include "tst_plugin_data_store.moc"
```

- [ ] **Step 2: Create header**

`libs/vault/include/corbomite/vault/PluginDataStore.h`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QJsonObject>
#include <QString>

namespace Corbomite {

/// Atomic JSON persistence at `<pluginDir>/data.json`.
///
/// `pluginDir` is typically the vault's
/// `.obsidian/plugins/<plugin-id>/` directory; PluginManager creates it
/// before handing a PluginDataStore to the plugin via PluginContext.
class PluginDataStore
{
public:
    explicit PluginDataStore(QString pluginDir);

    /// Returns the stored object, or an empty object if the file is
    /// missing / unreadable / not a JSON object.
    QJsonObject load() const;

    /// Atomically writes `obj` via QSaveFile. Returns false on I/O error.
    bool save(const QJsonObject &obj);

private:
    QString m_pluginDir;

    QString dataFilePath() const;
};

} // namespace Corbomite
```

- [ ] **Step 3: Create impl**

`libs/vault/src/PluginDataStore.cpp`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/vault/PluginDataStore.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QSaveFile>

namespace Corbomite {

PluginDataStore::PluginDataStore(QString pluginDir)
    : m_pluginDir(std::move(pluginDir)) {}

QString PluginDataStore::dataFilePath() const
{
    return m_pluginDir + QStringLiteral("/data.json");
}

QJsonObject PluginDataStore::load() const
{
    QFile f(dataFilePath());
    if (!f.open(QIODevice::ReadOnly)) return {};
    const QByteArray bytes = f.readAll();
    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(bytes, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) return {};
    return doc.object();
}

bool PluginDataStore::save(const QJsonObject &obj)
{
    QDir().mkpath(m_pluginDir);
    QSaveFile f(dataFilePath());
    if (!f.open(QIODevice::WriteOnly)) return false;
    const auto bytes = QJsonDocument(obj).toJson(QJsonDocument::Indented);
    if (f.write(bytes) != bytes.size()) { f.cancelWriting(); return false; }
    return f.commit();
}

} // namespace Corbomite
```

- [ ] **Step 4: Wire into `libs/vault/CMakeLists.txt`**

Add `src/PluginDataStore.cpp` to the target sources.

- [ ] **Step 5: Wire into tests CMakeLists.txt**

`tests/vault/CMakeLists.txt`:

```cmake
ecm_add_test(tst_plugin_data_store.cpp
    LINK_LIBRARIES Qt6::Test Corbomite::Vault
    TEST_NAME tst_plugin_data_store
)
```

- [ ] **Step 6: Add `loadData()` / `saveData()` to `PluginContext`**

In `PluginContext.h`:

```cpp
    /// Persistent per-plugin JSON state at
    /// `<vault>/.obsidian/plugins/<plugin-id>/data.json`.
    /// Returns an empty object if the plugin hasn't saved yet or the
    /// host hasn't wired a data dir. Gated by "config" permission.
    QJsonObject loadData() const;
    bool        saveData(const QJsonObject &obj);

    /// Host wiring — PluginManager calls this before plugin->load().
    void setPluginDataDir(const QString &dir);
```

Add `QString m_pluginDataDir;` + `mutable std::unique_ptr<PluginDataStore> m_dataStore;` to the private members.

In `PluginContext.cpp`:

```cpp
#include "corbomite/vault/PluginDataStore.h"

void PluginContext::setPluginDataDir(const QString &dir)
{
    m_pluginDataDir = dir;
    m_dataStore.reset();
}

QJsonObject PluginContext::loadData() const
{
    if (!m_granted.contains(QStringLiteral("config"))) return {};
    if (m_pluginDataDir.isEmpty()) return {};
    if (!m_dataStore) m_dataStore = std::make_unique<PluginDataStore>(m_pluginDataDir);
    return m_dataStore->load();
}

bool PluginContext::saveData(const QJsonObject &obj)
{
    if (!m_granted.contains(QStringLiteral("config"))) return false;
    if (m_pluginDataDir.isEmpty()) return false;
    if (!m_dataStore) m_dataStore = std::make_unique<PluginDataStore>(m_pluginDataDir);
    return m_dataStore->save(obj);
}
```

- [ ] **Step 7: Wire `PluginManager` to set the data dir**

In the PluginManager code path that enables a plugin — likely in `src/app/PluginManager.cpp` or `libs/vault/src/PluginManager.cpp` — just before calling `plugin->load(ctx)`:

```cpp
if (m_vault) {
    const QString dir = m_vault->configDir() + QStringLiteral("/plugins/") + plugin->metaData().pluginId();
    QDir().mkpath(dir);
    ctx->setPluginDataDir(dir);
}
```

(Exact variable names depend on the current PluginManager shape; grep `setCoreServices` there for the surrounding code block.)

- [ ] **Step 8: Build + test**

```bash
cmake --build build -j 10 && \
cd build && ctest -R "tst_plugin_data_store|tst_plugin_context" --output-on-failure
```

- [ ] **Step 9: Commit**

```bash
git add libs/vault/ tests/vault/tst_plugin_data_store.cpp tests/vault/CMakeLists.txt
git commit -m "$(cat <<'EOF'
feat(vault): plugin data.json persistence

PluginDataStore atomic round-trip at .obsidian/plugins/<id>/data.json.
Exposed on PluginContext as loadData()/saveData() gated on 'config'
permission. Matches Obsidian's plugin-data convention — ported plugins
can map directly.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

# Phase 4 — Trust + API Stability

## Task 4.1: `corbomite_add_plugin()` CMake helper + metadata.json.in templates

**Files:**
- Create: `cmake/CorbomitePlugin.cmake`
- Modify: root `CMakeLists.txt`
- Modify: `src/plugins/*/metadata.json` → rename to `metadata.json.in` (×8)
- Modify: `src/plugins/*/CMakeLists.txt` (×8)
- Modify: `src/app/PluginManager.cpp` (read `X-Corbomite-Trusted` to skip dialog)

- [ ] **Step 1: Create the helper**

`cmake/CorbomitePlugin.cmake`:

```cmake
# SPDX-License-Identifier: GPL-3.0-or-later
#
# corbomite_add_plugin(<target>
#   METADATA_TEMPLATE <path/to/metadata.json.in>
#   SOURCES <sources...>
#   [TRUSTED]
#   [LINK_LIBRARIES <libs...>]
# )
#
# Builds a KPluginFactory .so plugin and configures its metadata.json
# from the template, substituting the X_CORBOMITE_TRUSTED token.
#
# Only in-tree (src/plugins/*) CMakeLists should pass TRUSTED. Third-party
# plugins using this helper (e.g. via find_package(Corbomite)) get
# the "false" default; passing TRUSTED from an out-of-tree CMake is
# discouraged — there is no enforcement, but the social convention is
# that only src/plugins/ sets it.

function(corbomite_add_plugin TARGET)
    cmake_parse_arguments(ARG "TRUSTED" "METADATA_TEMPLATE" "SOURCES;LINK_LIBRARIES" ${ARGN})

    if(NOT ARG_METADATA_TEMPLATE)
        message(FATAL_ERROR "corbomite_add_plugin: METADATA_TEMPLATE required")
    endif()

    if(ARG_TRUSTED)
        set(X_CORBOMITE_TRUSTED "true")
    else()
        set(X_CORBOMITE_TRUSTED "false")
    endif()

    configure_file(
        "${ARG_METADATA_TEMPLATE}"
        "${CMAKE_CURRENT_BINARY_DIR}/metadata.json"
        @ONLY
    )

    add_library(${TARGET} MODULE ${ARG_SOURCES})
    target_link_libraries(${TARGET} PRIVATE
        KF6::CoreAddons
        ${ARG_LINK_LIBRARIES}
    )
    # KPluginFactory looks for metadata.json next to the built .so by
    # default; point it at our configured copy.
    set_target_properties(${TARGET} PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    )

    install(TARGETS ${TARGET} DESTINATION ${KDE_INSTALL_PLUGINDIR}/corbomite)
endfunction()
```

- [ ] **Step 2: Include the helper from the root CMake**

Add to root `CMakeLists.txt`, near the top after project():

```cmake
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")
include(CorbomitePlugin)
```

- [ ] **Step 3: Convert one plugin as exemplar — e.g. Backlinks**

Rename `src/plugins/backlinks/metadata.json` → `metadata.json.in`. Edit the file to replace the (likely absent) `X-Corbomite-Trusted` with the placeholder:

```json
{
    "KPlugin": {
        "Id": "corbomite.backlinks",
        "Name": "Backlinks",
        "Description": "Shows incoming links to the active note"
    },
    "X-Corbomite-Trusted": @X_CORBOMITE_TRUSTED@,
    "X-Corbomite-Permissions": ["vault.read", "metadata.read", "workspace", "ui.views"],
    "X-Corbomite-DockArea": "right"
}
```

(Preserve the exact KPlugin and X-Corbomite-* values from the current file; the only change is `@X_CORBOMITE_TRUSTED@` substitution.)

Modify `src/plugins/backlinks/CMakeLists.txt` to use the helper:

```cmake
corbomite_add_plugin(corbomitebacklinks
    METADATA_TEMPLATE "${CMAKE_CURRENT_SOURCE_DIR}/metadata.json.in"
    SOURCES
        BacklinksPlugin.cpp
        BacklinksView.cpp
    TRUSTED
    LINK_LIBRARIES
        Corbomite::Core
        Corbomite::Vault
        Corbomite::Storage
        Corbomite::Models
        Qt6::Widgets
)
```

Remove any now-duplicate setup (direct `add_library(... MODULE ...)`, manual `target_link_libraries`, `install()`) that the helper now owns.

- [ ] **Step 4: Verify Backlinks still builds + loads**

```bash
cmake -B build -DCORBOMITE_DEV_BUILD=ON && \
cmake --build build -j 10 --target corbomitebacklinks && \
cd build && ctest -R tst_backlinks_plugin --output-on-failure
```

- [ ] **Step 5: Apply the same conversion to the other 7 plugins**

Repeat Step 3 for: outlinks, outline, properties, search, file-explorer, local-graph, graph-view.

- [ ] **Step 6: Update `PluginManager` to read the trusted flag**

In `PluginManager::enablePlugin` (or equivalent), before firing `PluginPermissionGrantDialog`:

```cpp
const bool trusted = plugin->metaData().isTrusted();  // new accessor
if (!trusted) {
    // existing dialog path
}
```

Add the accessor to `PluginMetaData`:

```cpp
// PluginMetaData.h
bool isTrusted() const { return m_trusted; }

// PluginMetaData.cpp (parse)
m_trusted = raw.value(QStringLiteral("X-Corbomite-Trusted")).toBool(false);
```

- [ ] **Step 7: Add a test for the trusted-skip path**

In `tests/vault/tst_plugin_manager.cpp` (or equivalent; grep for PluginManager tests):

```cpp
void tst_plugin_manager::trustedPluginSkipsDialog()
{
    // Construct a PluginMetaData with X-Corbomite-Trusted: true,
    // call enablePlugin, assert no dialog was shown (via a mock
    // grant-dialog callback injected through a setter).
}
```

Implementation depends on existing PluginManager test shape; mirror the closest existing case.

- [ ] **Step 8: Build + full test suite**

```bash
cmake --build build -j 10 && cd build && ctest -j 10 --output-on-failure
```

- [ ] **Step 9: Commit**

```bash
git add cmake/CorbomitePlugin.cmake CMakeLists.txt src/plugins/ \
        libs/vault/include/corbomite/core/PluginMetaData.h \
        libs/core/src/PluginMetaData.cpp  # wherever it lives
git commit -m "$(cat <<'EOF'
feat(cmake): corbomite_add_plugin() helper + X-Corbomite-Trusted wiring

Centralizes KPluginFactory plugin build recipe. TRUSTED argument
injects X-Corbomite-Trusted: true into the metadata.json template —
applied to all 8 in-tree plugins, skipped by third-party CMake that
uses the helper (though nothing enforces the convention, per design).

PluginManager now reads PluginMetaData::isTrusted() and bypasses
PluginPermissionGrantDialog when true.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 4.2: `X-Corbomite-MinVersion` + `X-Corbomite-ApiLevel` enforcement

**Files:**
- Modify: `libs/core/include/corbomite/core/PluginMetaData.h`
- Modify: `libs/core/src/PluginMetaData.cpp`
- Modify: `src/app/PluginManager.cpp` (or `libs/vault/src/PluginManager.cpp`)
- Create: `libs/core/include/corbomite/core/PluginApi.h` (publishes `CORBOMITE_PLUGIN_API_LEVEL`)
- Modify: `tests/vault/tst_plugin_manager.cpp`

- [ ] **Step 1: Publish the host API level**

Create `libs/core/include/corbomite/core/PluginApi.h`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

namespace Corbomite {

/// Host's plugin API level. Plugins declare X-Corbomite-ApiLevel <= this
/// to load. When we make a hard ABI break, bump this integer; compat
/// shims for level N-1 stay in place for one major Corbomite version.
inline constexpr int CORBOMITE_PLUGIN_API_LEVEL = 1;

} // namespace Corbomite
```

- [ ] **Step 2: Extend `PluginMetaData` with the parsed fields**

```cpp
// PluginMetaData.h
QVersionNumber minVersion() const { return m_minVersion; }
int            apiLevel()  const { return m_apiLevel; }
```

```cpp
// PluginMetaData.cpp
m_minVersion = QVersionNumber::fromString(raw.value(QStringLiteral("X-Corbomite-MinVersion")).toString());
m_apiLevel   = raw.value(QStringLiteral("X-Corbomite-ApiLevel")).toInt(1);
```

- [ ] **Step 3: Write failing tests**

```cpp
void tst_plugin_manager::refusesPluginWithHigherMinVersion()
{
    // Construct PluginMetaData with X-Corbomite-MinVersion=99.99.99;
    // enablePlugin returns false / PluginsPage state reports incompatibility.
}

void tst_plugin_manager::refusesPluginWithHigherApiLevel()
{
    // Construct PluginMetaData with X-Corbomite-ApiLevel=99;
    // enablePlugin returns false.
}

void tst_plugin_manager::loadsPluginWithCompatibleVersionAndLevel()
{
    // Baseline — version <= host, apiLevel = 1: loads.
}
```

- [ ] **Step 4: Run to fail**

- [ ] **Step 5: Implement the gate in `PluginManager::enablePlugin`**

```cpp
const QVersionNumber hostVersion =
    QVersionNumber::fromString(QCoreApplication::applicationVersion());
if (!plugin->metaData().minVersion().isNull() &&
    plugin->metaData().minVersion() > hostVersion) {
    m_loadState[pluginId] = LoadState::IncompatibleVersion;
    qCWarning(lcPluginManager) << pluginId << "requires Corbomite >= "
        << plugin->metaData().minVersion().toString()
        << "; host is" << hostVersion.toString();
    return false;
}

if (plugin->metaData().apiLevel() > CORBOMITE_PLUGIN_API_LEVEL) {
    m_loadState[pluginId] = LoadState::IncompatibleApiLevel;
    qCWarning(lcPluginManager) << pluginId << "declares API level"
        << plugin->metaData().apiLevel()
        << "; host supports up to" << CORBOMITE_PLUGIN_API_LEVEL;
    return false;
}
```

- [ ] **Step 6: Surface the states in `PluginsPage`**

Extend the settings page to show "Requires Corbomite ≥ X.Y.Z" / "Requires plugin API level ≥ N" for plugins in those states, with the enable button disabled.

- [ ] **Step 7: Build + test**

- [ ] **Step 8: Commit**

```bash
git add libs/core/ src/app/ tests/vault/tst_plugin_manager.cpp
git commit -m "$(cat <<'EOF'
feat(plugin-manager): enforce X-Corbomite-MinVersion + X-Corbomite-ApiLevel

Plugins declaring a minimum host version higher than
QCoreApplication::applicationVersion refuse to load. Plugins declaring
an API level higher than CORBOMITE_PLUGIN_API_LEVEL (currently 1) refuse
to load. Both states surface in PluginsPage with a clear 'requires…'
message and disabled enable button.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 4.3: `CorbomiteConfig.cmake` for `find_package(Corbomite)`

**Files:**
- Create: `cmake/CorbomiteConfig.cmake.in`
- Modify: root `CMakeLists.txt`

- [ ] **Step 1: Create the template**

`cmake/CorbomiteConfig.cmake.in`:

```cmake
@PACKAGE_INIT@

include(CMakeFindDependencyMacro)
find_dependency(Qt6 6.5 REQUIRED COMPONENTS Core Widgets)
find_dependency(KF6 REQUIRED COMPONENTS CoreAddons ConfigCore I18n)

include("${CMAKE_CURRENT_LIST_DIR}/CorbomiteTargets.cmake")

# Re-export CorbomitePlugin helper so third-party plugins can use
# corbomite_add_plugin() directly.
include("${CMAKE_CURRENT_LIST_DIR}/CorbomitePlugin.cmake")

check_required_components(Corbomite)
```

- [ ] **Step 2: Wire install rules in root CMake**

```cmake
include(CMakePackageConfigHelpers)

configure_package_config_file(
    "${CMAKE_CURRENT_LIST_DIR}/cmake/CorbomiteConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/CorbomiteConfig.cmake"
    INSTALL_DESTINATION "${KDE_INSTALL_CMAKEPACKAGEDIR}/Corbomite"
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/CorbomiteConfig.cmake"
    "${CMAKE_CURRENT_LIST_DIR}/cmake/CorbomitePlugin.cmake"
    DESTINATION "${KDE_INSTALL_CMAKEPACKAGEDIR}/Corbomite"
)

install(EXPORT CorbomiteTargets
    FILE CorbomiteTargets.cmake
    NAMESPACE Corbomite::
    DESTINATION "${KDE_INSTALL_CMAKEPACKAGEDIR}/Corbomite"
)
```

Ensure each publicly-consumable target uses `install(TARGETS ... EXPORT CorbomiteTargets INCLUDES DESTINATION ...)`.

- [ ] **Step 3: Verify with a throwaway test**

```bash
mkdir -p /tmp/corbomite-find-package-test && cd /tmp/corbomite-find-package-test
cat > CMakeLists.txt <<'EOF'
cmake_minimum_required(VERSION 3.20)
project(FindTest)
find_package(Corbomite REQUIRED PATHS /path/to/build-install-prefix)
message(STATUS "Corbomite found: ${Corbomite_FOUND}")
EOF
```

(Skipping this step is acceptable for the plan; full verification happens in Phase 5 via the reference plugin's CMake.)

- [ ] **Step 4: Commit**

```bash
git add cmake/CorbomiteConfig.cmake.in CMakeLists.txt
git commit -m "$(cat <<'EOF'
feat(cmake): CorbomiteConfig for find_package(Corbomite)

Third-party plugins can now find_package(Corbomite) and call
corbomite_add_plugin() directly from their own CMake. Config exports
Corbomite::Core / Vault / Storage / Models targets and re-exports
the CorbomitePlugin helper.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

# Phase 5 — Reference Plugin + Docs + Closeout

## Task 5.1: `examples/plugin-template/` skeleton

**Files:**
- Create: `examples/plugin-template/CMakeLists.txt`
- Create: `examples/plugin-template/metadata.json.in`
- Create: `examples/plugin-template/TemplatePlugin.h`
- Create: `examples/plugin-template/TemplatePlugin.cpp`
- Create: `examples/plugin-template/tests/CMakeLists.txt`
- Create: `examples/plugin-template/tests/tst_template_plugin.cpp`
- Create: `examples/plugin-template/README.md`
- Modify: root `CMakeLists.txt` (add `CORBOMITE_BUILD_EXAMPLES` option + subdir)

- [ ] **Step 1: Add the option + subdir guard**

Root `CMakeLists.txt`:

```cmake
option(CORBOMITE_BUILD_EXAMPLES "Build example plugins" OFF)
if(CORBOMITE_BUILD_EXAMPLES)
    add_subdirectory(examples/plugin-template)
    add_subdirectory(examples/note-stats-plugin)
endif()
```

- [ ] **Step 2: Create the skeleton files**

`examples/plugin-template/CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.20)
project(corbomite-template-plugin VERSION 0.1.0 LANGUAGES CXX)

# In a real out-of-tree plugin:
#   find_package(Corbomite REQUIRED)
#   find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets)
#   find_package(KF6 REQUIRED COMPONENTS CoreAddons)
# In-tree, these targets are already available.

corbomite_add_plugin(corbomite-template
    METADATA_TEMPLATE "${CMAKE_CURRENT_SOURCE_DIR}/metadata.json.in"
    SOURCES
        TemplatePlugin.cpp
    LINK_LIBRARIES
        Corbomite::Core
        Corbomite::Vault
        Qt6::Widgets
)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
```

`examples/plugin-template/metadata.json.in`:

```json
{
    "KPlugin": {
        "Id": "example.template",
        "Name": "Template Plugin",
        "Description": "Skeleton to copy when starting a new Corbomite plugin",
        "Version": "0.1.0",
        "Authors": [{ "Name": "Your Name" }],
        "License": "GPL-3.0-or-later"
    },
    "X-Corbomite-Trusted": @X_CORBOMITE_TRUSTED@,
    "X-Corbomite-Permissions": ["vault.read"],
    "X-Corbomite-MinVersion": "0.1.0",
    "X-Corbomite-ApiLevel": 1
}
```

`examples/plugin-template/TemplatePlugin.h`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "corbomite/vault/Plugin.h"

class TemplatePlugin : public Corbomite::Plugin
{
    Q_OBJECT
public:
    TemplatePlugin(QObject *parent, const QVariantList &args);
    ~TemplatePlugin() override;

    void onLoad(Corbomite::PluginContext *ctx) override;
    void onUnload() override;
};
```

`examples/plugin-template/TemplatePlugin.cpp`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "TemplatePlugin.h"

#include "corbomite/vault/PluginContext.h"
#include "corbomite/vault/proxies/VaultProxy.h"

#include <KPluginFactory>
#include <QDebug>

TemplatePlugin::TemplatePlugin(QObject *parent, const QVariantList &)
    : Corbomite::Plugin(parent) {}

TemplatePlugin::~TemplatePlugin() = default;

void TemplatePlugin::onLoad(Corbomite::PluginContext *ctx)
{
    if (!ctx) return;
    if (auto *vault = ctx->vault()) {
        qInfo() << "template plugin loaded; vault has"
                << vault->getMarkdownFiles().size() << "markdown files";
    }
}

void TemplatePlugin::onUnload() {}

K_PLUGIN_FACTORY_WITH_JSON(TemplatePluginFactory, "metadata.json",
    registerPlugin<TemplatePlugin>();)

#include "TemplatePlugin.moc"
```

`examples/plugin-template/tests/CMakeLists.txt`:

```cmake
ecm_add_test(tst_template_plugin.cpp
    LINK_LIBRARIES Qt6::Test Corbomite::Core Corbomite::Vault
    TEST_NAME tst_template_plugin
)
```

`examples/plugin-template/tests/tst_template_plugin.cpp`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "../TemplatePlugin.h"
#include "corbomite/vault/PluginContext.h"

#include <QTest>

class tst_template_plugin : public QObject
{
    Q_OBJECT
private slots:
    void loadsAndUnloadsCleanly();
};

void tst_template_plugin::loadsAndUnloadsCleanly()
{
    Corbomite::PluginMetaData meta;
    meta.setPluginId(QStringLiteral("example.template"));
    Corbomite::PluginContext ctx(meta, {QStringLiteral("vault.read")});
    ctx.setCoreServices(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                        nullptr, nullptr, nullptr);

    TemplatePlugin p(nullptr, {});
    p.onLoad(&ctx);
    p.onUnload();
    QVERIFY(true);
}

QTEST_APPLESS_MAIN(tst_template_plugin)
#include "tst_template_plugin.moc"
```

`examples/plugin-template/README.md`:

```markdown
# Corbomite Plugin Template

Minimum skeleton for a third-party Corbomite plugin.

## What's here

- `CMakeLists.txt` — build recipe using `corbomite_add_plugin()`.
- `metadata.json.in` — KPlugin + Corbomite metadata template.
- `TemplatePlugin.{h,cpp}` — stub `Plugin` subclass.
- `tests/` — smoke-test harness.

## Adapting

1. Copy this directory as the starting point for your plugin.
2. Rename `TemplatePlugin` to `YourPlugin` (filenames, class, factory macro).
3. Update `metadata.json.in`'s `KPlugin.Id` / `Name` / `Description`.
4. Declare the permissions your plugin needs in `X-Corbomite-Permissions`.
5. Build: `cmake -B build && cmake --build build`.
6. Package: see `docs/plugin-development/DISTRIBUTION.md`.

See `docs/plugin-development/TUTORIAL.md` for a full walkthrough with
the `note-stats` reference plugin.
```

- [ ] **Step 3: Build + test**

```bash
cmake -B build -DCORBOMITE_BUILD_EXAMPLES=ON -DCORBOMITE_DEV_BUILD=ON && \
cmake --build build -j 10 --target corbomite-template tst_template_plugin && \
cd build && ctest -R tst_template_plugin --output-on-failure
```

- [ ] **Step 4: Commit**

```bash
git add examples/plugin-template/ CMakeLists.txt
git commit -m "$(cat <<'EOF'
feat(examples): plugin-template skeleton for third-party authors

Minimum CMakeLists + metadata.json.in + Plugin stub + smoke test.
Gated behind CORBOMITE_BUILD_EXAMPLES=OFF by default. Demonstrates
the intended third-party workflow: corbomite_add_plugin() without
TRUSTED, permissions declared in metadata, tests live alongside.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 5.2: `examples/note-stats-plugin/` reference plugin

**Files:**
- Create: `examples/note-stats-plugin/CMakeLists.txt`
- Create: `examples/note-stats-plugin/metadata.json.in`
- Create: `examples/note-stats-plugin/NoteStatsPlugin.{h,cpp}`
- Create: `examples/note-stats-plugin/NoteStatsView.{h,cpp}`
- Create: `examples/note-stats-plugin/tests/CMakeLists.txt`
- Create: `examples/note-stats-plugin/tests/tst_note_stats_plugin.cpp`
- Create: `examples/note-stats-plugin/README.md`

Note-stats reads the vault's markdown files via `VaultProxy`, counts tags + outlinks via `MetadataCacheReader` and `SearchProxy`, and renders a `NoteStatsView` sidebar with totals. Registered under `X-Corbomite-DockArea: "right"` to mount in the right sidebar.

- [ ] **Step 1: Create the plugin class**

`examples/note-stats-plugin/NoteStatsPlugin.h`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "corbomite/vault/Plugin.h"

namespace NoteStats {

class NoteStatsPlugin : public Corbomite::Plugin
{
    Q_OBJECT
public:
    NoteStatsPlugin(QObject *parent, const QVariantList &args);
    ~NoteStatsPlugin() override;

    QObject *createView(Corbomite::MainWindow *mw) override;
};

} // namespace NoteStats
```

`examples/note-stats-plugin/NoteStatsPlugin.cpp`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "NoteStatsPlugin.h"
#include "NoteStatsView.h"

#include "corbomite/vault/PluginContext.h"
#include "corbomite/vault/proxies/VaultProxy.h"
#include "corbomite/storage/proxies/MetadataCacheReader.h"
#include "corbomite/storage/proxies/SearchProxy.h"

#include <KPluginFactory>
#include <QDebug>

namespace NoteStats {

NoteStatsPlugin::NoteStatsPlugin(QObject *parent, const QVariantList &)
    : Corbomite::Plugin(parent) {}

NoteStatsPlugin::~NoteStatsPlugin() = default;

QObject *NoteStatsPlugin::createView(Corbomite::MainWindow *mw)
{
    auto *ctx = context();
    if (!ctx) return nullptr;
    auto *vault = ctx->vault();
    auto *search = ctx->search();
    auto *metadata = ctx->metadataCache();
    if (!vault || !search || !metadata) {
        qWarning() << "note-stats: missing permissions";
        return nullptr;
    }
    return new NoteStatsView(vault, search, metadata,
                             reinterpret_cast<QWidget *>(mw));
}

} // namespace NoteStats

K_PLUGIN_FACTORY_WITH_JSON(NoteStatsPluginFactory, "metadata.json",
    registerPlugin<NoteStats::NoteStatsPlugin>();)

#include "NoteStatsPlugin.moc"
```

- [ ] **Step 2: Create the view**

`examples/note-stats-plugin/NoteStatsView.h`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QWidget>

class QLabel;

namespace Corbomite {
class VaultProxy;
class SearchProxy;
class MetadataCacheReader;
}

namespace NoteStats {

class NoteStatsView : public QWidget
{
    Q_OBJECT
public:
    NoteStatsView(Corbomite::VaultProxy *vault,
                  Corbomite::SearchProxy *search,
                  Corbomite::MetadataCacheReader *metadata,
                  QWidget *parent = nullptr);

private slots:
    void refresh();

private:
    Corbomite::VaultProxy         *m_vault;
    Corbomite::SearchProxy        *m_search;
    Corbomite::MetadataCacheReader *m_metadata;
    QLabel *m_noteCount;
    QLabel *m_wordCount;
    QLabel *m_tagCount;
    QLabel *m_linkCount;
};

} // namespace NoteStats
```

`examples/note-stats-plugin/NoteStatsView.cpp`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "NoteStatsView.h"

#include "corbomite/storage/proxies/MetadataCacheReader.h"
#include "corbomite/storage/proxies/SearchProxy.h"
#include "corbomite/vault/TFile.h"
#include "corbomite/vault/proxies/VaultProxy.h"

#include <QFormLayout>
#include <QLabel>
#include <QVBoxLayout>

namespace NoteStats {

NoteStatsView::NoteStatsView(Corbomite::VaultProxy *vault,
                             Corbomite::SearchProxy *search,
                             Corbomite::MetadataCacheReader *metadata,
                             QWidget *parent)
    : QWidget(parent),
      m_vault(vault), m_search(search), m_metadata(metadata),
      m_noteCount(new QLabel(this)),
      m_wordCount(new QLabel(this)),
      m_tagCount(new QLabel(this)),
      m_linkCount(new QLabel(this))
{
    auto *form = new QFormLayout(this);
    form->addRow(tr("Notes"), m_noteCount);
    form->addRow(tr("Words (approx.)"), m_wordCount);
    form->addRow(tr("Unique tags"), m_tagCount);
    form->addRow(tr("Total links"), m_linkCount);

    refresh();

    connect(m_vault, &Corbomite::VaultProxy::created,
            this, &NoteStatsView::refresh);
    connect(m_vault, &Corbomite::VaultProxy::modified,
            this, &NoteStatsView::refresh);
    connect(m_vault, &Corbomite::VaultProxy::deletedFile,
            this, &NoteStatsView::refresh);
    connect(m_metadata, &Corbomite::MetadataCacheReader::indexFinished,
            this, &NoteStatsView::refresh);
}

void NoteStatsView::refresh()
{
    const auto files = m_vault->getMarkdownFiles();
    m_noteCount->setText(QString::number(files.size()));

    int words = 0;
    for (auto *f : files) {
        const QByteArray body = m_vault->cachedRead(f);
        words += body.count(' ') + body.count('\n');
    }
    m_wordCount->setText(QString::number(words));

    m_tagCount->setText(QString::number(m_search->allTags().size()));
    m_linkCount->setText(QString::number(m_search->allLinks().size()));
}

} // namespace NoteStats
```

- [ ] **Step 3: metadata.json.in + CMakeLists**

`examples/note-stats-plugin/metadata.json.in`:

```json
{
    "KPlugin": {
        "Id": "example.note-stats",
        "Name": "Note Statistics",
        "Description": "Shows vault-wide note count, word count, tags, and links",
        "Version": "0.1.0",
        "Authors": [{ "Name": "Corbomite Contributors" }],
        "License": "GPL-3.0-or-later"
    },
    "X-Corbomite-Trusted": @X_CORBOMITE_TRUSTED@,
    "X-Corbomite-Permissions": [
        "vault.read",
        "vault.events",
        "metadata.read",
        "ui.views"
    ],
    "X-Corbomite-DockArea": "right",
    "X-Corbomite-MinVersion": "0.1.0",
    "X-Corbomite-ApiLevel": 1
}
```

`examples/note-stats-plugin/CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.20)
project(note-stats-plugin VERSION 0.1.0 LANGUAGES CXX)

corbomite_add_plugin(note-stats
    METADATA_TEMPLATE "${CMAKE_CURRENT_SOURCE_DIR}/metadata.json.in"
    SOURCES
        NoteStatsPlugin.cpp
        NoteStatsView.cpp
    LINK_LIBRARIES
        Corbomite::Core
        Corbomite::Vault
        Corbomite::Storage
        Qt6::Widgets
)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
```

- [ ] **Step 4: smoke test**

`examples/note-stats-plugin/tests/tst_note_stats_plugin.cpp`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "../NoteStatsView.h"

#include "corbomite/storage/MetadataCache.h"
#include "corbomite/storage/SQLiteIndex.h"
#include "corbomite/storage/proxies/MetadataCacheReader.h"
#include "corbomite/storage/proxies/SearchProxy.h"
#include "corbomite/vault/Vault.h"
#include "corbomite/vault/proxies/VaultProxy.h"

#include <QTemporaryDir>
#include <QTest>

class tst_note_stats_plugin : public QObject
{
    Q_OBJECT
private slots:
    void refreshReadsFromProxies();
};

void tst_note_stats_plugin::refreshReadsFromProxies()
{
    Corbomite::Vault vault;
    Corbomite::FileSystemAdapter adapter;
    vault.setAdapter(&adapter);
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QFile(dir.filePath(QStringLiteral("a.md"))).open(QIODevice::WriteOnly);
    QFile(dir.filePath(QStringLiteral("b.md"))).open(QIODevice::WriteOnly);
    vault.load(dir.path());

    QSet<QString> granted = { QStringLiteral("vault.read"),
                              QStringLiteral("vault.events"),
                              QStringLiteral("metadata.read") };
    Corbomite::VaultProxy vaultProxy(&vault, granted, QStringLiteral("t"));
    Corbomite::SQLiteIndex index;
    index.open(dir.filePath(QStringLiteral("idx.sqlite")));
    Corbomite::SearchProxy searchProxy(&index, granted, QStringLiteral("t"));
    Corbomite::MetadataCache cache;
    Corbomite::MetadataCacheReader reader(&cache);

    NoteStats::NoteStatsView view(&vaultProxy, &searchProxy, &reader);
    // If the view constructed without crashing and the connects wired, pass.
    QVERIFY(true);
}

QTEST_MAIN(tst_note_stats_plugin)
#include "tst_note_stats_plugin.moc"
```

`examples/note-stats-plugin/tests/CMakeLists.txt` matches the template's.

- [ ] **Step 5: README doubles as tutorial**

`examples/note-stats-plugin/README.md`:

```markdown
# Note Statistics — Corbomite Reference Plugin

Reads every markdown file in the vault; shows counts in the right
sidebar. Demonstrates `VaultProxy`, `SearchProxy`, `MetadataCacheReader`,
sidebar-view registration, and reactive refresh via Qt signals.

## Build

```
cmake -B build -DCORBOMITE_BUILD_EXAMPLES=ON
cmake --build build --target note-stats
```

## Install

```
cmake --install build --component note-stats
```

Or, for a distro package, see `docs/plugin-development/DISTRIBUTION.md`.

## First enable

The plugin declares `vault.read`, `vault.events`, `metadata.read`,
`ui.views`. On first enable, Corbomite prompts you to approve these —
click "Allow". On subsequent launches, no prompt fires until you disable
and re-enable.

## Code walkthrough

See `docs/plugin-development/TUTORIAL.md`.
```

- [ ] **Step 6: Build + test**

```bash
cmake --build build -j 10 --target note-stats tst_note_stats_plugin && \
cd build && ctest -R tst_note_stats_plugin --output-on-failure
```

- [ ] **Step 7: Commit**

```bash
git add examples/note-stats-plugin/
git commit -m "$(cat <<'EOF'
feat(examples): note-stats reference third-party plugin

Exercises the full plugin ABI end-to-end: VaultProxy (read +
forwarded signals), SearchProxy (tags + links), MetadataCacheReader,
ViewRegistrar. Opt-in build via CORBOMITE_BUILD_EXAMPLES=ON.
Serves as the subject of docs/plugin-development/TUTORIAL.md.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 5.3: `docs/plugin-development/` documentation suite

**Files:**
- Create: `docs/plugin-development/README.md`
- Create: `docs/plugin-development/TUTORIAL.md`
- Create: `docs/plugin-development/API-REFERENCE.md`
- Create: `docs/plugin-development/API-STABILITY.md`
- Create: `docs/plugin-development/DISTRIBUTION.md`

For each file below, create the file with the content shown. No code changes needed.

- [ ] **Step 1: `README.md` (TOC + quickstart)**

```markdown
# Corbomite Plugin Development

Documentation for authors of Corbomite plugins.

## Table of contents

- [**TUTORIAL.md**](TUTORIAL.md) — walks through the `note-stats`
  reference plugin end-to-end.
- [**API-REFERENCE.md**](API-REFERENCE.md) — every public proxy, signal,
  permission, and lifecycle hook.
- [**API-STABILITY.md**](API-STABILITY.md) — the 12-month deprecation
  contract. Read this before committing to a plugin project.
- [**DISTRIBUTION.md**](DISTRIBUTION.md) — how to ship via distro
  package managers.

## Quickstart

```
git clone <your-plugin-repo>
cd your-plugin
cmake -B build
cmake --build build
cmake --install build
```

Then restart Corbomite; your plugin appears in **Settings → Plugins**.
Click **Enable** and approve the permission dialog.

## Plugin model in one paragraph

A Corbomite plugin is a native C++ `.so` module loaded via
`KPluginFactory`. It subclasses `Corbomite::Plugin`, declares a set of
permissions in `metadata.json`, and uses permission-gated proxies from
`PluginContext` to access the vault, metadata cache, search index, and
UI. There is no sandbox — plugins run in-process. Permissions are a
declaration-of-intent layer, enforced at the proxy boundary.
```

- [ ] **Step 2: `TUTORIAL.md`**

Content: a narrative walk through `examples/note-stats-plugin/` — the project layout, what each file does, how permissions map to API access, the reactive-refresh pattern, how to build/install. Target ~300-500 lines. The tutorial references line numbers in the reference plugin so readers can click through.

Rather than re-write every line here, the tutorial expands on these beats:
1. Project layout (CMakeLists + metadata.json.in + Plugin + View + tests)
2. Why `metadata.json.in` + `corbomite_add_plugin()` (trust flag substitution)
3. The `Plugin` lifecycle (`onLoad(ctx)` / `onUnload()` / `createView()`)
4. What proxies are (`VaultProxy` etc.) and why they exist
5. Permissions: what tokens map to what proxy access
6. Reactive UI: subscribing to `VaultProxy::modified` and friends
7. Running the smoke test
8. Installing for your development vault
9. Next steps: copy the template

- [ ] **Step 3: `API-REFERENCE.md`**

Content: catalog of every public proxy method, permission token, lifecycle hook, signal. Organized by proxy class. Each method one line. Example format:

```markdown
## VaultProxy

Inherits `QObject`. Construct via `PluginContext::vault()`.

### Read (gated on `vault.read`)

| Method | Returns | Notes |
|---|---|---|
| `read(TFile*)` | `QByteArray` | |
| `cachedRead(TFile*)` | `QByteArray` | Pulls from in-memory cache when present. |
| `readBinary(TFile*)` | `QByteArray` | |
| `exists(QString path)` | `bool` | |
| `getFileByPath(QString)` | `TFile*` | |
| ... | | |

### Mutation (gated on `vault.write`)

| Method | Returns | Notes |
|---|---|---|
| `modify(TFile*, QByteArray)` | `bool` | Echo-suppressed — no `modified` signal for self-writes. |
| ... | | |

### Signals (gated on `vault.events`)

- `created(TAbstractFile*)`
- `modified(TAbstractFile*)`
- `deletedFile(TAbstractFile*)`
- `renamed(TAbstractFile*, QString oldPath)`

...
```

Continue for `FileManagerProxy`, `MetadataCacheReader`, `SearchProxy`, `WorkspaceController`, `CommandRegistrar`, `ViewRegistrar`, `MenuInjector`, `SecretStorage`, `ProcessSpawner`. Then a `Permissions` section listing all 12 tokens and what each gates, and a `Plugin lifecycle` section with `onLoad` / `onUnload` / `createView` / `focus` / `saveSessionState` / `loadSessionState`.

Target: ~500-800 lines. Readers consult it, they don't read it front-to-back.

- [ ] **Step 4: `API-STABILITY.md`**

```markdown
# Corbomite Plugin API Stability

## Current commitment

From **Cluster N close** (date this document is first committed), the
plugin API is **shape-stable**: no methods will be removed, renamed,
or have their semantics altered without the 12-month deprecation
process described below. Additions are always permitted.

**Shape-stable** is not the same as **1.0-frozen**. See "Pre-1.0
caveats" below.

## Levels

| Level | When | Guarantees |
|---|---|---|
| **Shape-stable** | Now | No removals or semantic changes. Ergonomic tweaks (argument defaults, additional overloads) permitted. |
| **1.0-frozen** | After a proving period with real plugin consumers | Any change requires the full 12-month deprecation window, including ergonomic tweaks. |

The bump from shape-stable to 1.0-frozen is a separate event we'll
announce in the changelog.

## Deprecation process (once 1.0-frozen)

1. **Deprecate:** add `[[deprecated("use X instead")]]` to the
   declaration. Emit `qCWarning` at first call per session. Document
   in `CHANGELOG.md`.
2. **Maintain:** the deprecated method continues to work for 12
   months. Plugins relying on it keep loading.
3. **Remove:** after 12 months from deprecation announcement, the
   method is deleted. Plugins still using it stop loading with a
   clear error.

## Version compatibility

Plugins declare two version-related fields in `metadata.json`:

- `X-Corbomite-MinVersion`: the Corbomite version the plugin was
  built against. Host refuses to load plugins declaring a minimum
  higher than its own version.
- `X-Corbomite-ApiLevel`: an integer API-break marker. Currently `1`.
  If we make a hard ABI break, we bump this; plugins declaring the
  old level continue to load against a compatibility shim for one
  major version.

Plugins that omit either field are treated as targeting the host's
earliest API (level 1, version 0.1).

## Pre-1.0 caveats

While Corbomite is pre-1.0, the following deviations are permitted:

- Minor ergonomic method signatures (adding an overload, adding a
  default parameter value that doesn't change semantics) may happen
  without the deprecation window.
- Permission tokens may be split or merged if feedback from early
  plugin authors reveals the initial split is wrong.

Any such change is called out in `CHANGELOG.md` under a "Plugin API
adjustments" heading.

## Adding new API

Always permitted, no version bump required:

- New proxy methods.
- New permission tokens (plugins that don't declare them don't get them).
- New signals.
- New lifecycle hooks with default empty implementations.
- New registry types.
```

- [ ] **Step 5: `DISTRIBUTION.md`**

```markdown
# Distributing a Corbomite Plugin

The canonical distribution path is a native distro package
(`.deb` / `.rpm` / Arch `PKGBUILD` / nixpkgs / etc). Corbomite
discovers plugins in the standard KDE plugin search path.

## File layout

At install time, your plugin produces:

- `/usr/lib/<triplet>/qt6/plugins/corbomite/<plugin-id>.so` — the
  compiled module.
- (No separate metadata.json — it's embedded in the `.so` via
  `K_PLUGIN_FACTORY_WITH_JSON`.)

The default `install(TARGETS ...)` rule emitted by
`corbomite_add_plugin()` places the `.so` at
`${KDE_INSTALL_PLUGINDIR}/corbomite`, which is the right location
on standard KDE systems.

## Minimal Debian packaging

`debian/control`:

```
Source: corbomite-plugin-example
Priority: optional
Maintainer: Your Name <you@example.com>
Build-Depends: cmake, debhelper-compat (= 13),
               qt6-base-dev, libkf6coreaddons-dev,
               corbomite-dev
Standards-Version: 4.6.0

Package: corbomite-plugin-example
Architecture: any
Depends: ${shlibs:Depends}, corbomite
Description: Example Corbomite plugin
```

`debian/rules`:

```
#!/usr/bin/make -f
%:
    dh $@ --buildsystem=cmake
```

## Minimal Fedora / openSUSE RPM spec

```rpm
%global plugin_id example
Name:           corbomite-plugin-%{plugin_id}
Version:        0.1.0
Release:        1%{?dist}
Summary:        Example Corbomite plugin
License:        GPL-3.0-or-later
BuildRequires:  cmake, extra-cmake-modules, qt6-qtbase-devel,
                kf6-kcoreaddons-devel, corbomite-devel
Requires:       corbomite

%build
cmake -B build
cmake --build build

%install
DESTDIR=%{buildroot} cmake --install build

%files
%{_libdir}/qt6/plugins/corbomite/*.so
```

## Distribution without a distro package

Users can install a plugin manually by placing its `.so` in:

- System-wide: `${KDE_INSTALL_FULL_PLUGINDIR}/corbomite/`
- User-local: `~/.local/share/qt6/plugins/corbomite/`
  (or wherever `qtpaths --plugin-dir` reports)

Corbomite discovers both. No difference in trust — both are untrusted
unless the plugin declares `X-Corbomite-Trusted: true`, which
third-party plugins should not do.

## Versioning your plugin alongside Corbomite

Declare `X-Corbomite-MinVersion` to the Corbomite release you tested
against. Bump it when you use new API. Corbomite refuses to load
plugins whose minimum exceeds its own version, which is a clean failure
mode.
```

- [ ] **Step 6: Commit**

```bash
git add docs/plugin-development/
git commit -m "$(cat <<'EOF'
docs(plugin): plugin-author documentation suite

README (TOC + quickstart), TUTORIAL (walkthrough of the note-stats
reference plugin), API-REFERENCE (proxy + permission + lifecycle
catalog), API-STABILITY (12-month deprecation contract + pre-1.0
caveats), DISTRIBUTION (distro packaging recipes).

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 5.4: Cluster closeout — retro + PROJECT-STATE + INDEX

**Files:**
- Create: `docs/cluster-retros/cluster-n.md`
- Modify: `docs/PROJECT-STATE.md`
- Modify: `docs/superpowers/plans/INDEX.md`

- [ ] **Step 1: Write the retro**

`docs/cluster-retros/cluster-n.md` — short retro, ~100-200 lines, matching the format of `cluster-retros/cluster-q.md`. Sections: goal, what landed, notable design decisions, plan deviations, open follow-ups, verdict.

- [ ] **Step 2: Update PROJECT-STATE**

In `docs/PROJECT-STATE.md`:
- Prepend a new "**Last updated:** … — Cluster N closed." entry, linking the retro.
- Flip the roadmap row N's Status to `Done` with a one-line description pointing at the key artifacts.
- Move any N-related in-flight items under §Recent decisions.

- [ ] **Step 3: Update plans INDEX**

In `docs/superpowers/plans/INDEX.md`:
- Flip row N's Status to `Done` matching the roadmap.
- Add a one-line closing note about the retro.

- [ ] **Step 4: Run the full test suite**

```bash
cmake --build build -j 10 && cd build && ctest -j 10 --output-on-failure
```

Expected: green outside the pre-existing known-flaky baseline.

- [ ] **Step 5: Commit**

```bash
git add docs/cluster-retros/cluster-n.md docs/PROJECT-STATE.md \
        docs/superpowers/plans/INDEX.md
git commit -m "$(cat <<'EOF'
docs: close Cluster N — plugin-ready surfaces

Retro + PROJECT-STATE + INDEX update. All Phase 1-5 deliverables landed.
Plugin ABI is shape-stable; three stop-gap raw accessors deleted; real
keyring + plugin data.json persistence live; reference plugin exercises
the full surface; docs/plugin-development/ provides author-facing
documentation.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Self-review

Spec requirements checked against tasks:

| Spec § | Requirement | Task |
|---|---|---|
| §2.1 #1 | Retire stop-gaps | Tasks 1.1, 1.2, 1.3, 1.4, 2.1–2.7 |
| §2.1 #2 | Commit to stable ABI + MinVersion | Tasks 4.2, 5.3 (API-STABILITY.md) |
| §2.1 #3 | Third-party path proof | Tasks 5.1, 5.2 |
| §2.1 #4 | Real persistence | Tasks 3.1, 3.2 |
| §4.1 | Retire vaultRaw/metadataCacheRaw/searchIndex | Tasks 1.1–1.4 + 2.1–2.6 |
| §4.2 | Real keyring | Task 3.1 |
| §4.3 | plugin data.json | Task 3.2 |
| §4.4 | Trust-flag CMake wiring | Task 4.1 |
| §4.5 | MinVersion + ApiLevel + stability docs | Tasks 4.2, 5.3 |
| §4.6 | Reference plugin | Task 5.2 |
| §4.7 | Plugin-author docs | Task 5.3 |

Type-consistency scan: no naming drift found. `SearchProxy` / `VaultProxy` / `MetadataCacheReader` referenced consistently across tasks. `setCoreServices` signature-expansion change (Task 2.6) is the one point where multiple files change signatures — the task explicitly lists "every call site" and the `grep -rn setCoreServices` command to find them.

Placeholder scan: no "TBD" / "TODO-later" / "add appropriate X" phrases.

Scope check: 5 phases, each deliverable independently, all within a single cluster's scope. No sub-project decomposition needed.
