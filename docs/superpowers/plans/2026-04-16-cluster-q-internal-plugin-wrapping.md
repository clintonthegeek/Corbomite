# Cluster Q — Internal-Plugin Wrapping + Permissions — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.
>
> **Working directory:** `/home/clinton/dev/Corbomite` on `master` (no worktrees — other agents share the tree; see `memory/feedback_no_branches.md`).
>
> **Build commands:**
> - Configure: `cmake -S . -B build -DCORBOMITE_DEV_BUILD=ON`
> - Build: `cmake --build build`
> - Test all: `cd build && ctest --output-on-failure`
> - Test one: `cd build && ctest -R tst_plugin_manager --output-on-failure`
> - 4 known-flaky tests to ignore: `tst_markoff_inline_math`, `tst_renderengine`, `tst_completion_popup`, `tst_benchmark_layout`.

**Goal:** Land the plugin infrastructure (`Corbomite::Plugin` + `PluginContext` + `PluginManager` + 9 capability-token proxies + Settings Plugins page) and migrate the 8 existing UI features (FileExplorer, Search, Backlinks, Outlinks, Outline, Properties, LocalGraph, GraphView) off MainWindow direct-construction into first-class `.so` InternalPlugins loaded via `KPluginFactory`.

**Architecture:** KDE-native plugin model (Kate's `PluginManager` pattern). One loader path for built-in and future community plugins; trust established via install-path origin; declarative-contract permission model with capability-token proxies (honest about *not* being runtime sandboxing). See `docs/superpowers/specs/2026-04-16-cluster-q-internal-plugin-wrapping-design.md` (spec — authoritative).

**Tech Stack:** Qt6, KF6 (`KPluginFactory`, `KPluginMetaData`, `KConfigGroup`, `KCMUtils`, `KXMLGUIClient`), C++20.

**Audit references:**
- `docs/superpowers/specs/2026-04-16-cluster-q-internal-plugin-wrapping-design.md` — spec; read first.
- `docs/obsidian-audit/domains/plugin.md` — Obsidian `Plugin` base class reference.
- `docs/obsidian-audit/PLUGIN-API-SKETCH.md` — API shape reference for shim-friendly design choices.
- `docs/kde-power-software-design-guide/06-plugin-architecture.md` — Kate's PluginManager pattern; directly modeled here.
- Local KDE source: `~/src/kde/src/kate/apps/lib/katepluginmanager.cpp` — read once before Task 3 for idiomatic KPluginFactory usage.

---

## File structure (overview — locks decomposition)

```
libs/core/include/corbomite/core/
  Plugin.h                       # abstract base
  PluginMetaData.h               # KPluginMetaData wrapper
  PluginContext.h                # handed to plugin->onLoad(ctx)
  PluginManager.h                # singleton-ish, owned by CorbomiteApp
  PluginPermissionGrantDialog.h  # modal dialog
  proxies/
    VaultReader.h
    VaultWriter.h
    MetadataCacheReader.h
    WorkspaceController.h
    CommandRegistrar.h
    ViewRegistrar.h
    MenuInjector.h
    SecretStorage.h
    ProcessSpawner.h

libs/core/src/
  Plugin.cpp
  PluginMetaData.cpp
  PluginContext.cpp
  PluginManager.cpp
  PluginPermissionGrantDialog.cpp
  proxies/*.cpp

tests/core/
  tst_plugin.cpp
  tst_plugin_metadata.cpp
  tst_plugin_context.cpp
  tst_plugin_manager.cpp
  tst_plugin_permission_dialog.cpp
  tst_proxy_vault.cpp           # covers VaultReader + VaultWriter
  tst_proxy_metadata.cpp
  tst_proxy_workspace.cpp
  tst_proxy_ui.cpp              # covers CommandRegistrar + ViewRegistrar + MenuInjector
  tst_proxy_secrets_process.cpp

src/plugins/
  backlinks/
    BacklinksPlugin.{h,cpp}
    BacklinksView.{h,cpp}       # moved from src/sidebar/BacklinksPanel
    metadata.json
    CMakeLists.txt
    tests/tst_backlinks_plugin.cpp
  outlinks/ ... (same shape)
  outline/ ... (same shape)
  properties/ ... (same shape)
  search/ ... (same shape)
  file-explorer/ ... (same shape)
  local-graph/ ... (same shape)
  graph-view/ ... (same shape)

src/dialogs/
  PluginsPage.{h,cpp}            # new SettingsDialog tab

src/app/
  CorbomiteApp.{h,cpp}           # PluginManager owner + startup wiring
  SettingsDialog.cpp             # add PluginsPage to tab list
  MainWindow.{h,cpp}             # lose direct panel construction; gain plugin-view hosting
```

---

# Phase 1 — Core infrastructure

## Task 1: PluginMetaData wrapper + tests

**Files:**
- Create: `libs/core/include/corbomite/core/PluginMetaData.h`
- Create: `libs/core/src/PluginMetaData.cpp`
- Create: `tests/core/tst_plugin_metadata.cpp`
- Modify: `libs/core/CMakeLists.txt` (add to SOURCES + HEADERS lists)
- Modify: `tests/core/CMakeLists.txt` (add `tst_plugin_metadata` executable)

- [ ] **Step 1: Write failing test**

Create `tests/core/tst_plugin_metadata.cpp`:

```cpp
#include <QTest>
#include <QJsonObject>
#include <QJsonArray>
#include <KPluginMetaData>
#include "corbomite/core/PluginMetaData.h"

class TestPluginMetaData : public QObject
{
    Q_OBJECT
private slots:
    void parsesPermissions();
    void parsesTrustedFlag();
    void parsesMinVersion();
    void absentKeysGiveDefaults();
};

static KPluginMetaData makeMeta(const QJsonObject &body)
{
    QJsonObject full = body;
    full.insert(QStringLiteral("KPlugin"),
        QJsonObject{{QStringLiteral("Id"), QStringLiteral("test-plugin")},
                    {QStringLiteral("Name"), QStringLiteral("Test")}});
    return KPluginMetaData(full, QStringLiteral("test-plugin"));
}

void TestPluginMetaData::parsesPermissions()
{
    QJsonObject body{
        {QStringLiteral("X-Corbomite-Permissions"),
         QJsonArray{QStringLiteral("vault.read"), QStringLiteral("ui.views")}}
    };
    Corbomite::PluginMetaData meta(makeMeta(body));
    const QStringList perms = meta.permissions();
    QCOMPARE(perms.size(), 2);
    QVERIFY(perms.contains(QStringLiteral("vault.read")));
    QVERIFY(perms.contains(QStringLiteral("ui.views")));
}

void TestPluginMetaData::parsesTrustedFlag()
{
    Corbomite::PluginMetaData trusted(makeMeta({
        {QStringLiteral("X-Corbomite-Trusted"), true}}));
    QVERIFY(trusted.trusted());

    Corbomite::PluginMetaData untrusted(makeMeta({
        {QStringLiteral("X-Corbomite-Trusted"), false}}));
    QVERIFY(!untrusted.trusted());
}

void TestPluginMetaData::parsesMinVersion()
{
    Corbomite::PluginMetaData meta(makeMeta({
        {QStringLiteral("X-Corbomite-MinVersion"), QStringLiteral("1.2.3")}}));
    QCOMPARE(meta.minAppVersion(), QVersionNumber(1, 2, 3));
}

void TestPluginMetaData::absentKeysGiveDefaults()
{
    Corbomite::PluginMetaData meta(makeMeta({}));
    QVERIFY(meta.permissions().isEmpty());
    QVERIFY(!meta.trusted());
    QVERIFY(meta.minAppVersion().isNull());
}

QTEST_MAIN(TestPluginMetaData)
#include "tst_plugin_metadata.moc"
```

- [ ] **Step 2: Run test, verify it fails to compile**

```bash
cd build && cmake --build . --target tst_plugin_metadata 2>&1 | tail -10
```

Expected: compile error — `PluginMetaData.h: No such file or directory`.

- [ ] **Step 3: Implement the header**

Create `libs/core/include/corbomite/core/PluginMetaData.h`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <KPluginMetaData>
#include <QStringList>
#include <QVersionNumber>

namespace Corbomite {

/// Thin wrapper over KPluginMetaData exposing Corbomite-specific JSON keys.
/// Zero runtime overhead beyond what KPluginMetaData itself does.
class PluginMetaData
{
public:
    explicit PluginMetaData(const KPluginMetaData &base) : m_base(base) {}

    const KPluginMetaData &base() const { return m_base; }

    /// Returns the X-Corbomite-Permissions array as QStringList. Empty if absent.
    QStringList permissions() const;

    /// True if X-Corbomite-Trusted is explicitly true. Defaults to false.
    bool trusted() const;

    /// Parses X-Corbomite-MinVersion. Returns null QVersionNumber if absent.
    QVersionNumber minAppVersion() const;

    /// Origin hint — set by PluginManager during discovery (system vs user path).
    /// Used in Task 3 for trusted-claim normalization.
    enum class Origin { System, User, Unknown };
    void setOrigin(Origin o) { m_origin = o; }
    Origin origin() const { return m_origin; }

private:
    KPluginMetaData m_base;
    Origin m_origin = Origin::Unknown;
};

} // namespace Corbomite
```

Create `libs/core/src/PluginMetaData.cpp`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/PluginMetaData.h"

namespace Corbomite {

QStringList PluginMetaData::permissions() const
{
    QStringList out;
    const auto val = m_base.rawData().value(QStringLiteral("X-Corbomite-Permissions"));
    if (val.isArray()) {
        for (const auto &v : val.toArray()) {
            out << v.toString();
        }
    }
    return out;
}

bool PluginMetaData::trusted() const
{
    return m_base.rawData()
        .value(QStringLiteral("X-Corbomite-Trusted"))
        .toBool(false);
}

QVersionNumber PluginMetaData::minAppVersion() const
{
    const QString s = m_base.rawData()
        .value(QStringLiteral("X-Corbomite-MinVersion"))
        .toString();
    if (s.isEmpty()) return {};
    return QVersionNumber::fromString(s);
}

} // namespace Corbomite
```

Add to `libs/core/CMakeLists.txt` in the appropriate SOURCES and HEADERS lists:

```cmake
# Under HEADERS (or the equivalent list variable)
include/corbomite/core/PluginMetaData.h

# Under SOURCES
src/PluginMetaData.cpp
```

Add to `tests/core/CMakeLists.txt`:

```cmake
ecm_add_test(tst_plugin_metadata.cpp
    TEST_NAME tst_plugin_metadata
    LINK_LIBRARIES
        Qt6::Test
        Corbomite::Core
        KF6::CoreAddons
)
```

- [ ] **Step 4: Build + run test, verify it passes**

```bash
cd build && cmake --build . 2>&1 | tail -10 && ctest -R tst_plugin_metadata --output-on-failure
```

Expected: all 4 test slots PASS.

- [ ] **Step 5: Commit**

```bash
git add libs/core/include/corbomite/core/PluginMetaData.h \
        libs/core/src/PluginMetaData.cpp \
        libs/core/CMakeLists.txt \
        tests/core/tst_plugin_metadata.cpp \
        tests/core/CMakeLists.txt
git commit -m "feat(core): add PluginMetaData wrapper over KPluginMetaData

Cluster Q phase 1: exposes Corbomite-specific JSON keys
(X-Corbomite-Permissions, X-Corbomite-Trusted, X-Corbomite-MinVersion)
plus an Origin field for install-path trust normalization."
```

---

## Task 2: Plugin abstract base class + tests

**Files:**
- Create: `libs/core/include/corbomite/core/Plugin.h`
- Create: `libs/core/src/Plugin.cpp`
- Create: `tests/core/tst_plugin.cpp`
- Modify: `libs/core/CMakeLists.txt`, `tests/core/CMakeLists.txt`

- [ ] **Step 1: Write failing test**

`tests/core/tst_plugin.cpp`:

```cpp
#include <QTest>
#include <QSignalSpy>
#include "corbomite/core/Plugin.h"
#include "corbomite/core/PluginContext.h"

class TrackingPlugin : public Corbomite::Plugin
{
    Q_OBJECT
public:
    int onLoadCalls = 0;
    int onUnloadCalls = 0;
    Corbomite::PluginContext *lastContext = nullptr;

    void onLoad(Corbomite::PluginContext *ctx) override
    {
        ++onLoadCalls;
        lastContext = ctx;
    }
    void onUnload() override { ++onUnloadCalls; }
};

class TestPlugin : public QObject
{
    Q_OBJECT
private slots:
    void lifecycleFiresOnLoadOnce();
    void lifecycleFiresOnUnloadOnceOnDestroy();
    void componentCleanupsFire();
};

void TestPlugin::lifecycleFiresOnLoadOnce()
{
    TrackingPlugin p;
    Corbomite::PluginContext ctx(Corbomite::PluginMetaData(KPluginMetaData{}), {});
    p.load(&ctx);
    QCOMPARE(p.onLoadCalls, 1);
    QCOMPARE(p.lastContext, &ctx);
    p.load(&ctx); // idempotent
    QCOMPARE(p.onLoadCalls, 1);
}

void TestPlugin::lifecycleFiresOnUnloadOnceOnDestroy()
{
    TrackingPlugin p;
    Corbomite::PluginContext ctx(Corbomite::PluginMetaData(KPluginMetaData{}), {});
    p.load(&ctx);
    p.unload();
    QCOMPARE(p.onUnloadCalls, 1);
    p.unload();
    QCOMPARE(p.onUnloadCalls, 1);
}

void TestPlugin::componentCleanupsFire()
{
    TrackingPlugin p;
    Corbomite::PluginContext ctx(Corbomite::PluginMetaData(KPluginMetaData{}), {});
    p.load(&ctx);
    int cleanupRuns = 0;
    p.registerCleanup([&] { ++cleanupRuns; });
    p.unload();
    QCOMPARE(cleanupRuns, 1);
}

QTEST_MAIN(TestPlugin)
#include "tst_plugin.moc"
```

- [ ] **Step 2: Run test, verify it fails**

```bash
cd build && cmake --build . --target tst_plugin 2>&1 | tail -10
```

Expected: compile error — Plugin.h missing.

- [ ] **Step 3: Implement Plugin base**

Create `libs/core/include/corbomite/core/Plugin.h`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "corbomite/core/Component.h"

namespace KTextEditor { class ConfigPage; }

class QWidget;

namespace Corbomite {

class PluginContext;
class MainWindow; // fwd; real type lives in src/app

/// Abstract base for all Corbomite plugins (built-in and community).
/// Extends Component so every register*/addChild call auto-cleans on unload.
///
/// Subclasses MUST override:
///   - onLoad(PluginContext*)  — called once at plugin enable
///   - onUnload()              — called once at plugin disable (LIFO after cleanups)
///
/// Subclasses MAY override:
///   - createView(MainWindow*) — create per-window UI (return nullptr if headless)
///   - configPage(int, QWidget*) — up to configPages() KConfig pages
class Plugin : public Component
{
    Q_OBJECT
public:
    explicit Plugin(QObject *parent = nullptr);
    ~Plugin() override;

    /// Full lifecycle: idempotent guard, captures context, calls onLoad.
    void load(PluginContext *ctx);

    /// The context handed to this plugin at load. nullptr before load() / after unload().
    PluginContext *context() const { return m_context; }

    /// Optional per-MainWindow view factory. Default returns nullptr.
    virtual QObject *createView(MainWindow *mainWindow);

    /// Optional KConfig page count. Default returns 0.
    virtual int configPages() const { return 0; }

    /// Optional KConfig page factory. Default returns nullptr.
    virtual KTextEditor::ConfigPage *configPage(int number, QWidget *parent);

protected:
    /// Override point — called by load(). Default is no-op.
    virtual void onLoad(PluginContext *ctx) { Q_UNUSED(ctx); }

    /// Override from Component; routes to context-clearing + subclass cleanup.
    void onUnload() override {}

private:
    PluginContext *m_context = nullptr;
};

} // namespace Corbomite
```

Create `libs/core/src/Plugin.cpp`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/Plugin.h"
#include "corbomite/core/PluginContext.h"

namespace Corbomite {

Plugin::Plugin(QObject *parent) : Component(parent) {}
Plugin::~Plugin() = default;

void Plugin::load(PluginContext *ctx)
{
    if (isLoaded()) return;
    m_context = ctx;
    onLoad(ctx);
    Component::load(); // fires Component's onload in subclass-onload-then-children order
}

QObject *Plugin::createView(MainWindow *)
{
    return nullptr;
}

KTextEditor::ConfigPage *Plugin::configPage(int, QWidget *)
{
    return nullptr;
}

} // namespace Corbomite
```

Add to CMakeLists.txt + test CMakeLists.txt as in Task 1.

- [ ] **Step 4: Build + run test, verify passes**

Expected: all 3 test slots PASS. If `PluginContext` doesn't yet exist as a type (it comes in Task 3), temporarily stub it with a minimal `class PluginContext { public: PluginContext(PluginMetaData, QSet<QString>) {} };` in a forward-declaration header; Task 3 will replace with the real one.

**Note:** this task depends on Task 3 (PluginContext). If dispatched out of order, stub `PluginContext` with the 2-arg constructor as shown; Task 3 will subsume the stub.

- [ ] **Step 5: Commit**

```bash
git add libs/core/include/corbomite/core/Plugin.h \
        libs/core/src/Plugin.cpp \
        libs/core/CMakeLists.txt \
        tests/core/tst_plugin.cpp \
        tests/core/CMakeLists.txt
git commit -m "feat(core): add Plugin abstract base class

Cluster Q phase 1: Component subclass with onLoad/onUnload/createView/
configPage virtuals. All Corbomite plugins (built-in and community)
inherit this; PluginManager drives lifecycle in Task 3."
```

---

## Task 3: PluginContext + 9 proxy stubs

**Files:**
- Create: `libs/core/include/corbomite/core/PluginContext.h`
- Create: `libs/core/include/corbomite/core/proxies/{VaultReader,VaultWriter,MetadataCacheReader,WorkspaceController,CommandRegistrar,ViewRegistrar,MenuInjector,SecretStorage,ProcessSpawner}.h`
- Create: `libs/core/src/PluginContext.cpp`
- Create: `libs/core/src/proxies/{...}.cpp`
- Create: `tests/core/tst_plugin_context.cpp`
- Modify: `libs/core/CMakeLists.txt`, `tests/core/CMakeLists.txt`

> **Scope note for this task:** proxy classes ship as STUBS here — each proxy's header declares the class, its constructor takes a reference to the underlying service, and it has a minimal forwarding method (e.g., `VaultReader::read(path)` forwards to `Vault::readNote(path)`). Full proxy API surface comes in per-proxy follow-up tasks (5-8). This task establishes the *shape* — `PluginContext` accessors return proxy or nullptr based on granted permissions.

- [ ] **Step 1: Write failing test**

`tests/core/tst_plugin_context.cpp`:

```cpp
#include <QTest>
#include "corbomite/core/PluginContext.h"
#include "corbomite/core/proxies/VaultReader.h"
#include "corbomite/core/proxies/VaultWriter.h"

class TestPluginContext : public QObject
{
    Q_OBJECT
private slots:
    void ungrantedAccessorsReturnNull();
    void grantedAccessorsReturnProxy();
    void grantedPermissionsRetrievable();
};

static Corbomite::PluginMetaData emptyMeta()
{
    return Corbomite::PluginMetaData(KPluginMetaData{});
}

void TestPluginContext::ungrantedAccessorsReturnNull()
{
    Corbomite::PluginContext ctx(emptyMeta(), {});
    QCOMPARE(ctx.vaultReader(), nullptr);
    QCOMPARE(ctx.vaultWriter(), nullptr);
    QCOMPARE(ctx.metadataCache(), nullptr);
    QCOMPARE(ctx.workspace(), nullptr);
    QCOMPARE(ctx.commands(), nullptr);
    QCOMPARE(ctx.views(), nullptr);
    QCOMPARE(ctx.menus(), nullptr);
    QCOMPARE(ctx.network(), nullptr);
    QCOMPARE(ctx.secrets(), nullptr);
    QCOMPARE(ctx.process(), nullptr);
}

void TestPluginContext::grantedAccessorsReturnProxy()
{
    Corbomite::PluginContext ctx(emptyMeta(),
        {QStringLiteral("vault.read"), QStringLiteral("vault.write")});
    QVERIFY(ctx.vaultReader() != nullptr);
    QVERIFY(ctx.vaultWriter() != nullptr);
    QCOMPARE(ctx.network(), nullptr); // not granted
}

void TestPluginContext::grantedPermissionsRetrievable()
{
    QSet<QString> granted{QStringLiteral("vault.read"), QStringLiteral("ui.views")};
    Corbomite::PluginContext ctx(emptyMeta(), granted);
    QCOMPARE(ctx.grantedPermissions(), granted);
    QVERIFY(ctx.hasPermission(QStringLiteral("vault.read")));
    QVERIFY(!ctx.hasPermission(QStringLiteral("network")));
}

QTEST_MAIN(TestPluginContext)
#include "tst_plugin_context.moc"
```

- [ ] **Step 2: Run test, verify it fails**

```bash
cd build && cmake --build . --target tst_plugin_context 2>&1 | tail -10
```

Expected: compile error — PluginContext.h missing.

- [ ] **Step 3: Implement PluginContext + stub proxies**

Create `libs/core/include/corbomite/core/PluginContext.h`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "corbomite/core/PluginMetaData.h"
#include <QSet>
#include <QString>
#include <KConfigGroup>

class QNetworkAccessManager;

namespace Corbomite {

class Vault;
class MetadataCache;
class Workspace;
class CommandRegistry;
class ViewRegistry;
class MenuEventEmitter;
class VaultReader;
class VaultWriter;
class MetadataCacheReader;
class WorkspaceController;
class CommandRegistrar;
class ViewRegistrar;
class MenuInjector;
class SecretStorage;
class ProcessSpawner;

/// Handed to Plugin::onLoad(). Lives as long as the plugin.
/// Owns proxy objects for services the plugin's declared+granted permissions unlock.
/// nullptr from an accessor means the corresponding permission was not granted.
class PluginContext
{
public:
    /// Used by tests and PluginManager. In production, PluginManager also calls
    /// setCoreServices() before handing the context to plugin->load().
    PluginContext(PluginMetaData meta, QSet<QString> granted);
    ~PluginContext();

    PluginContext(const PluginContext &) = delete;
    PluginContext &operator=(const PluginContext &) = delete;

    /// Installs references to core services. Must be called before any
    /// granted accessor is invoked by the plugin. No-op for ungranted services.
    void setCoreServices(Vault *vault,
                         MetadataCache *metadata,
                         Workspace *workspace,
                         CommandRegistry *commands,
                         ViewRegistry *views,
                         MenuEventEmitter *menus,
                         QNetworkAccessManager *network);

    // Metadata
    const PluginMetaData &metaData() const { return m_meta; }
    const QSet<QString>  &grantedPermissions() const { return m_granted; }
    bool hasPermission(const QString &token) const { return m_granted.contains(token); }

    // Permission-gated accessors (nullptr if ungranted or core service absent)
    VaultReader         *vaultReader() const;     // "vault.read"
    VaultWriter         *vaultWriter() const;     // "vault.write"
    MetadataCacheReader *metadataCache() const;   // "metadata.read"
    WorkspaceController *workspace() const;       // "workspace"
    CommandRegistrar    *commands() const;        // "ui.commands"
    ViewRegistrar       *views() const;           // "ui.views"
    MenuInjector        *menus() const;           // "ui.menus"
    QNetworkAccessManager *network() const;       // "network"
    SecretStorage       *secrets() const;         // "secrets"
    ProcessSpawner      *process() const;         // "process"
    KConfigGroup         config();                // "config"

private:
    PluginMetaData m_meta;
    QSet<QString>  m_granted;

    // Owned proxies (constructed lazily the first time the accessor is called,
    // but only if the corresponding permission is granted)
    mutable VaultReader         *m_vaultReader = nullptr;
    mutable VaultWriter         *m_vaultWriter = nullptr;
    mutable MetadataCacheReader *m_metadataReader = nullptr;
    mutable WorkspaceController *m_workspaceController = nullptr;
    mutable CommandRegistrar    *m_commandRegistrar = nullptr;
    mutable ViewRegistrar       *m_viewRegistrar = nullptr;
    mutable MenuInjector        *m_menuInjector = nullptr;
    mutable SecretStorage       *m_secretStorage = nullptr;
    mutable ProcessSpawner      *m_processSpawner = nullptr;

    // Core services (set via setCoreServices — non-owning)
    Vault             *m_vault = nullptr;
    MetadataCache     *m_metadata = nullptr;
    Workspace         *m_workspace = nullptr;
    CommandRegistry   *m_commandRegistry = nullptr;
    ViewRegistry      *m_viewRegistry = nullptr;
    MenuEventEmitter  *m_menuEmitter = nullptr;
    QNetworkAccessManager *m_network = nullptr;
};

} // namespace Corbomite
```

Create the 9 proxy headers under `libs/core/include/corbomite/core/proxies/`. Each has this shape — substitute `Xxx` and the underlying service type. Example:

`proxies/VaultReader.h`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <QByteArray>

namespace Corbomite {
class Vault;

/// Read-only facade over Vault for plugins with the "vault.read" permission.
/// Thin wrapper — no per-call permission re-check (check happened at
/// PluginContext construction).
class VaultReader
{
public:
    explicit VaultReader(Vault *vault) : m_vault(vault) {}

    /// Returns note body bytes, or empty if not found.
    QByteArray read(const QString &relativePath) const;

private:
    Vault *m_vault;
};
} // namespace Corbomite
```

`proxies/VaultReader.cpp` (stub forwarding):
```cpp
#include "corbomite/core/proxies/VaultReader.h"
// #include "corbomite/core/Vault.h"  // include once Vault is the real type

namespace Corbomite {
QByteArray VaultReader::read(const QString &relativePath) const
{
    Q_UNUSED(relativePath);
    // Task 5 will wire to m_vault->readNote(relativePath)
    return {};
}
} // namespace Corbomite
```

Repeat the pattern for the other 8 proxies. Keep implementations as stubs that return defaults; wire-up tasks (5-8) will fill the forwarding.

Create `libs/core/src/PluginContext.cpp`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/PluginContext.h"
#include "corbomite/core/proxies/VaultReader.h"
#include "corbomite/core/proxies/VaultWriter.h"
#include "corbomite/core/proxies/MetadataCacheReader.h"
#include "corbomite/core/proxies/WorkspaceController.h"
#include "corbomite/core/proxies/CommandRegistrar.h"
#include "corbomite/core/proxies/ViewRegistrar.h"
#include "corbomite/core/proxies/MenuInjector.h"
#include "corbomite/core/proxies/SecretStorage.h"
#include "corbomite/core/proxies/ProcessSpawner.h"

namespace Corbomite {

PluginContext::PluginContext(PluginMetaData meta, QSet<QString> granted)
    : m_meta(std::move(meta)), m_granted(std::move(granted)) {}

PluginContext::~PluginContext()
{
    delete m_vaultReader;
    delete m_vaultWriter;
    delete m_metadataReader;
    delete m_workspaceController;
    delete m_commandRegistrar;
    delete m_viewRegistrar;
    delete m_menuInjector;
    delete m_secretStorage;
    delete m_processSpawner;
}

void PluginContext::setCoreServices(Vault *v, MetadataCache *m, Workspace *w,
    CommandRegistry *c, ViewRegistry *vr, MenuEventEmitter *me,
    QNetworkAccessManager *n)
{
    m_vault = v; m_metadata = m; m_workspace = w;
    m_commandRegistry = c; m_viewRegistry = vr; m_menuEmitter = me;
    m_network = n;
}

VaultReader *PluginContext::vaultReader() const
{
    if (!hasPermission(QStringLiteral("vault.read")) || !m_vault) return nullptr;
    if (!m_vaultReader) m_vaultReader = new VaultReader(m_vault);
    return m_vaultReader;
}

// ... identical pattern for the other 8 accessors ...

QNetworkAccessManager *PluginContext::network() const
{
    return hasPermission(QStringLiteral("network")) ? m_network : nullptr;
}

KConfigGroup PluginContext::config()
{
    if (!hasPermission(QStringLiteral("config"))) return {};
    return KConfigGroup(KSharedConfig::openConfig(),
        QStringLiteral("Plugin-") + m_meta.base().pluginId());
}

} // namespace Corbomite
```

Add all files to CMakeLists.txt + test CMakeLists.txt.

- [ ] **Step 4: Build + run test**

Expected: all 3 test slots PASS.

- [ ] **Step 5: Commit**

```bash
git add libs/core/include/corbomite/core/PluginContext.h \
        libs/core/include/corbomite/core/proxies/ \
        libs/core/src/PluginContext.cpp \
        libs/core/src/proxies/ \
        libs/core/CMakeLists.txt \
        tests/core/tst_plugin_context.cpp \
        tests/core/CMakeLists.txt
git commit -m "feat(core): add PluginContext + 9 proxy class stubs

Cluster Q phase 1: PluginContext carries granted-permission set +
owned proxy objects. Accessors return nullptr for ungranted
permissions; proxies are stubs pending per-proxy wire-up tasks."
```

---

## Task 4: PluginPermissionGrantDialog + tests

**Files:**
- Create: `libs/core/include/corbomite/core/PluginPermissionGrantDialog.h`
- Create: `libs/core/src/PluginPermissionGrantDialog.cpp`
- Create: `tests/core/tst_plugin_permission_dialog.cpp`

- [ ] **Step 1: Write failing test**

`tests/core/tst_plugin_permission_dialog.cpp`:

```cpp
#include <QTest>
#include <QSet>
#include "corbomite/core/PluginPermissionGrantDialog.h"

class TestPluginPermissionDialog : public QObject
{
    Q_OBJECT
private slots:
    void grantAllReturnsFullSet();
    void uncheckDropsFromSet();
    void cancelReturnsEmpty();
};

void TestPluginPermissionDialog::grantAllReturnsFullSet()
{
    QStringList requested{QStringLiteral("vault.read"), QStringLiteral("network")};
    Corbomite::PluginPermissionGrantDialog dlg(QStringLiteral("Test Plugin"),
                                                QStringLiteral("A test"), requested);
    QSet<QString> granted = dlg.grantedIfAccepted();
    QCOMPARE(granted.size(), 2);
    QVERIFY(granted.contains(QStringLiteral("vault.read")));
    QVERIFY(granted.contains(QStringLiteral("network")));
}

void TestPluginPermissionDialog::uncheckDropsFromSet()
{
    QStringList requested{QStringLiteral("vault.read"), QStringLiteral("network")};
    Corbomite::PluginPermissionGrantDialog dlg(QStringLiteral("Test"),
                                                QStringLiteral(""), requested);
    dlg.setCheckedForTest(QStringLiteral("network"), false);
    QSet<QString> granted = dlg.grantedIfAccepted();
    QCOMPARE(granted.size(), 1);
    QVERIFY(granted.contains(QStringLiteral("vault.read")));
}

void TestPluginPermissionDialog::cancelReturnsEmpty()
{
    QStringList requested{QStringLiteral("vault.read")};
    Corbomite::PluginPermissionGrantDialog dlg(QStringLiteral("Test"),
                                                QStringLiteral(""), requested);
    dlg.cancelForTest();
    QVERIFY(dlg.wasCancelled());
}

QTEST_MAIN(TestPluginPermissionDialog)
#include "tst_plugin_permission_dialog.moc"
```

- [ ] **Step 2: Run test, verify fails**

Expected: compile error.

- [ ] **Step 3: Implement**

Create `libs/core/include/corbomite/core/PluginPermissionGrantDialog.h`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDialog>
#include <QSet>
#include <QStringList>

class QCheckBox;

namespace Corbomite {

/// Modal dialog shown when an untrusted plugin is being enabled
/// for the first time (or after a metadata update adds permissions).
/// Shows declared permissions + human-readable descriptions; user checks
/// which to grant. Accept → grantedIfAccepted() returns the checked set.
/// Cancel → wasCancelled() true, grantedIfAccepted() empty.
class PluginPermissionGrantDialog : public QDialog
{
    Q_OBJECT
public:
    PluginPermissionGrantDialog(const QString &pluginName,
                                 const QString &pluginDescription,
                                 const QStringList &requestedPermissions,
                                 QWidget *parent = nullptr);

    QSet<QString> grantedIfAccepted() const;
    bool wasCancelled() const { return m_cancelled; }

    // Test-facing hooks (avoid opening a real modal in tests)
    void setCheckedForTest(const QString &token, bool checked);
    void cancelForTest();

    /// Human-readable description of a permission token, for the dialog +
    /// the Settings page. One-line format.
    static QString describe(const QString &token);

private Q_SLOTS:
    void onAccepted();
    void onRejected();

private:
    QHash<QString, QCheckBox *> m_boxes;
    bool m_cancelled = false;
    bool m_accepted = false;
};

} // namespace Corbomite
```

Implementation in `libs/core/src/PluginPermissionGrantDialog.cpp` — build UI with a `QVBoxLayout`, label with plugin name + description, checkbox-per-permission where the label is the token + `describe(token)`, `QDialogButtonBox` with Grant/Cancel.

The `describe()` static method:
```cpp
QString PluginPermissionGrantDialog::describe(const QString &t)
{
    static const QHash<QString, QString> descs{
        {QStringLiteral("vault.read"),    i18n("Read note contents from your vault")},
        {QStringLiteral("vault.write"),   i18n("Create, modify, delete, or rename notes")},
        {QStringLiteral("metadata.read"), i18n("Read note metadata (frontmatter, links, tags)")},
        {QStringLiteral("ui.commands"),   i18n("Add commands and keyboard shortcuts")},
        {QStringLiteral("ui.views"),      i18n("Add sidebar panels and main-area views")},
        {QStringLiteral("ui.menus"),      i18n("Inject items into context menus")},
        {QStringLiteral("workspace"),     i18n("Open files, split panes, manage tabs")},
        {QStringLiteral("network"),       i18n("Connect to external network services")},
        {QStringLiteral("secrets"),       i18n("Store and retrieve credentials")},
        {QStringLiteral("process"),       i18n("Run external programs")},
        {QStringLiteral("config"),        i18n("Read and write application settings")},
        {QStringLiteral("render"),        i18n("Extend note rendering (mermaid, math, syntax, embeds)")},
    };
    return descs.value(t, t);
}
```

- [ ] **Step 4: Build + run test, verify passes**

Expected: 3/3 PASS.

- [ ] **Step 5: Commit**

```bash
git add libs/core/include/corbomite/core/PluginPermissionGrantDialog.h \
        libs/core/src/PluginPermissionGrantDialog.cpp \
        libs/core/CMakeLists.txt \
        tests/core/tst_plugin_permission_dialog.cpp \
        tests/core/CMakeLists.txt
git commit -m "feat(core): add PluginPermissionGrantDialog

Cluster Q phase 1: modal dialog for first-enable permission grant
prompts on untrusted plugins. Human-readable descriptions for all
12 capability tokens."
```

---

## Task 5: PluginManager — discovery + trust normalization

**Files:**
- Create: `libs/core/include/corbomite/core/PluginManager.h`
- Create: `libs/core/src/PluginManager.cpp`
- Create: `tests/core/tst_plugin_manager.cpp`

> **Pre-read:** `~/src/kde/src/kate/apps/lib/katepluginmanager.cpp` — Kate's reference implementation. Browse the discovery + KConfig-persistence sections before writing.

- [ ] **Step 1: Write failing test (discovery only — load/unload come in Task 6)**

```cpp
// tests/core/tst_plugin_manager.cpp
#include <QTest>
#include <QTemporaryDir>
#include "corbomite/core/PluginManager.h"

class TestPluginManager : public QObject
{
    Q_OBJECT
private slots:
    void discoversPluginsInSystemPath();
    void trustedClaimDroppedForUserPath();
    void versionCompatRejectsNewerRequirement();
};

// Helpers: make mock metadata-only plugin files in a QTemporaryDir.
// Real .so loading tested in Task 6 via a fixture plugin.

void TestPluginManager::discoversPluginsInSystemPath()
{
    QTemporaryDir systemDir;
    QTemporaryDir userDir;
    // Place mock .json metadata in each (KPluginMetaData::findPlugins
    // accepts a path). For this test we call findPluginsIn() directly.

    Corbomite::PluginManager mgr;
    mgr.setSystemSearchPath(systemDir.path());
    mgr.setUserSearchPath(userDir.path());
    // ... populate fake plugin manifests in systemDir ...
    mgr.discoverPlugins();

    QVERIFY(mgr.pluginCount() >= 1);
    QCOMPARE(mgr.pluginByIndex(0).origin(),
             Corbomite::PluginMetaData::Origin::System);
}

void TestPluginManager::trustedClaimDroppedForUserPath()
{
    // Plugin in userDir declares X-Corbomite-Trusted: true
    // Expected: mgr treats it as trusted=false
    // Plugin in systemDir declares X-Corbomite-Trusted: true
    // Expected: mgr treats it as trusted=true
    // (Full test body follows the discovery pattern above)
}

void TestPluginManager::versionCompatRejectsNewerRequirement()
{
    // Plugin declares X-Corbomite-MinVersion: 99.0.0
    // Expected: discover() skips it and emits a qWarning (verify with
    //           QTest::ignoreMessage)
}

QTEST_MAIN(TestPluginManager)
#include "tst_plugin_manager.moc"
```

- [ ] **Step 2: Run test, verify fails**

- [ ] **Step 3: Implement discovery-only PluginManager**

Create `libs/core/include/corbomite/core/PluginManager.h`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "corbomite/core/PluginMetaData.h"
#include <QObject>
#include <QList>
#include <QHash>
#include <QVersionNumber>

namespace Corbomite {

class Plugin;
class PluginContext;

/// Owns the lifecycle of all Corbomite plugins (built-in + community).
/// Singleton-ish: owned by CorbomiteApp, one per application.
class PluginManager : public QObject
{
    Q_OBJECT
public:
    struct PluginInfo {
        PluginMetaData metaData;
        Plugin        *instance = nullptr;
        PluginContext *context = nullptr;
        bool           enabled = false;
    };

    explicit PluginManager(QObject *parent = nullptr);
    ~PluginManager() override;

    // Paths — overridable for testing
    void setSystemSearchPath(const QString &);
    void setUserSearchPath(const QString &);

    /// Scans system + user paths; populates m_plugins with all discovered
    /// metadata. Does NOT load any plugin yet. Enforces trusted-claim
    /// normalization + version-compat gate.
    void discoverPlugins();

    int pluginCount() const { return m_plugins.size(); }
    const PluginInfo &pluginByIndex(int i) const { return m_plugins[i]; }
    const PluginInfo *pluginById(const QString &id) const;

Q_SIGNALS:
    void pluginDiscovered(const QString &id);
    void pluginLoaded(const QString &id);
    void pluginUnloading(const QString &id);
    void pluginEnabled(const QString &id);
    void pluginDisabled(const QString &id);

private:
    void discoverIn(const QString &path, PluginMetaData::Origin origin);
    QVersionNumber appVersion() const;

    QString m_systemPath;
    QString m_userPath;
    QList<PluginInfo> m_plugins;
};

} // namespace Corbomite
```

Create `libs/core/src/PluginManager.cpp`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/PluginManager.h"
#include "corbomite/core/Plugin.h"
#include "corbomite/core/PluginContext.h"

#include <KPluginMetaData>
#include <QCoreApplication>
#include <QStandardPaths>
#include <QDir>
#include <QDebug>

namespace Corbomite {

PluginManager::PluginManager(QObject *parent) : QObject(parent)
{
    m_systemPath = QStringLiteral(KDE_INSTALL_PLUGINDIR) +
                   QStringLiteral("/corbomite");
    m_userPath = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) +
                 QStringLiteral("/corbomite/plugins");
}

PluginManager::~PluginManager()
{
    // Task 6 will populate shutdown ordering here
    for (auto &info : m_plugins) {
        delete info.context;
        // instance delete deferred to Task 6
    }
}

void PluginManager::setSystemSearchPath(const QString &p) { m_systemPath = p; }
void PluginManager::setUserSearchPath(const QString &p)   { m_userPath = p; }

QVersionNumber PluginManager::appVersion() const
{
    // APP_VERSION is a compile-time define set by CMake's project(... VERSION ...).
    // If it's not set, fall back to 0.0.0 to avoid spurious rejections.
#ifdef CORBOMITE_APP_VERSION
    return QVersionNumber::fromString(QStringLiteral(CORBOMITE_APP_VERSION));
#else
    return QVersionNumber(0, 0, 0);
#endif
}

void PluginManager::discoverIn(const QString &path, PluginMetaData::Origin origin)
{
    const auto metas = KPluginMetaData::findPlugins(path);
    for (const auto &base : metas) {
        PluginMetaData meta(base);
        meta.setOrigin(origin);

        // Version-compat gate
        const auto minVer = meta.minAppVersion();
        if (!minVer.isNull() && appVersion() < minVer) {
            qWarning() << "Plugin" << meta.base().pluginId()
                       << "requires Corbomite >=" << minVer.toString()
                       << "but app is" << appVersion().toString()
                       << "— skipping.";
            continue;
        }

        // Trusted-claim normalization: user-path plugins can't self-assert trust
        if (origin == PluginMetaData::Origin::User && meta.trusted()) {
            qWarning() << "Plugin" << meta.base().pluginId()
                       << "in user path declared X-Corbomite-Trusted; forcing to false.";
            // The base KPluginMetaData rawData is immutable; we carry a flag on
            // PluginMetaData itself. Add an override: see header for
            // setTrustedOverride(false). For simplicity here, origin==User
            // is the authoritative signal — PluginMetaData::trusted() should
            // return false when origin==User regardless of JSON.
            // (Update PluginMetaData::trusted() to honor origin.)
        }

        PluginInfo info;
        info.metaData = meta;
        m_plugins.append(info);
        Q_EMIT pluginDiscovered(meta.base().pluginId());
    }
}

void PluginManager::discoverPlugins()
{
    m_plugins.clear();
    discoverIn(m_systemPath, PluginMetaData::Origin::System);
    discoverIn(m_userPath, PluginMetaData::Origin::User);
}

const PluginManager::PluginInfo *PluginManager::pluginById(const QString &id) const
{
    for (const auto &info : m_plugins) {
        if (info.metaData.base().pluginId() == id) return &info;
    }
    return nullptr;
}

} // namespace Corbomite
```

**Also update `PluginMetaData::trusted()`** to honor origin:

```cpp
// In PluginMetaData.cpp:
bool PluginMetaData::trusted() const
{
    // User-path plugins can never be trusted regardless of JSON claim
    if (m_origin == Origin::User) return false;
    return m_base.rawData().value(QStringLiteral("X-Corbomite-Trusted")).toBool(false);
}
```

Add `target_compile_definitions(corbomite-core PRIVATE CORBOMITE_APP_VERSION="${PROJECT_VERSION}")` to `libs/core/CMakeLists.txt`.

- [ ] **Step 4: Build + run test, verify passes**

Expected: 3/3 PASS. May need to create fixture JSON files in the test; use `QFile::copy` from `tests/core/fixtures/` into the temp dirs.

- [ ] **Step 5: Commit**

```bash
git add libs/core/include/corbomite/core/PluginManager.h \
        libs/core/src/PluginManager.cpp \
        libs/core/src/PluginMetaData.cpp \
        libs/core/CMakeLists.txt \
        tests/core/tst_plugin_manager.cpp \
        tests/core/CMakeLists.txt \
        tests/core/fixtures/
git commit -m "feat(core): add PluginManager discovery + trust normalization

Cluster Q phase 1: scans system + user plugin paths; enforces
X-Corbomite-MinVersion gate; normalizes X-Corbomite-Trusted=false
for user-path plugins regardless of JSON claim."
```

---

## Task 6: PluginManager — load/unload/enable/disable/KConfig

**Files:**
- Modify: `libs/core/include/corbomite/core/PluginManager.h`
- Modify: `libs/core/src/PluginManager.cpp`
- Modify: `tests/core/tst_plugin_manager.cpp`
- Create: `tests/core/fixture-plugin/` — a minimal `.so` fixture plugin used by tests

- [ ] **Step 1: Write failing test**

Extend `tst_plugin_manager.cpp` with:

```cpp
void TestPluginManager::loadsFixturePlugin()
{
    Corbomite::PluginManager mgr;
    mgr.setSystemSearchPath(QStringLiteral(FIXTURE_PLUGIN_DIR));
    mgr.discoverPlugins();
    QVERIFY(mgr.pluginCount() >= 1);

    QSignalSpy loadedSpy(&mgr, &Corbomite::PluginManager::pluginLoaded);
    mgr.enablePlugin(QStringLiteral("corbomite-test-fixture"));
    QCOMPARE(loadedSpy.count(), 1);
    QVERIFY(mgr.pluginById(QStringLiteral("corbomite-test-fixture"))->instance != nullptr);
}

void TestPluginManager::disableUnloads()
{
    // after loadsFixturePlugin:
    QSignalSpy unloadSpy(&mgr, &Corbomite::PluginManager::pluginUnloading);
    mgr.disablePlugin(QStringLiteral("corbomite-test-fixture"));
    QCOMPARE(unloadSpy.count(), 1);
    QVERIFY(mgr.pluginById(QStringLiteral("corbomite-test-fixture"))->instance == nullptr);
}

void TestPluginManager::kconfigRoundTripsEnabled()
{
    // Create mgr, enable plugin, destroy mgr, recreate, verify plugin is enabled
    // (read from a test-scoped KConfig file)
}
```

Create fixture plugin at `tests/core/fixture-plugin/` — a minimal `.so` that subclasses `Corbomite::Plugin`, records load/unload calls, ships a metadata.json, and has its own CMakeLists.txt producing a MODULE library.

- [ ] **Step 2: Run tests, verify they fail**

- [ ] **Step 3: Implement**

Add to `PluginManager.h`:
```cpp
public:
    bool enablePlugin(const QString &id);
    bool disablePlugin(const QString &id);
    void loadEnabledStateFromConfig();
    void saveEnabledStateToConfig();
private:
    QSet<QString> loadGrantedPermissions(const QString &id) const;
    void saveGrantedPermissions(const QString &id, const QSet<QString> &);
```

Implement in `PluginManager.cpp`:
```cpp
bool PluginManager::enablePlugin(const QString &id)
{
    auto *info = const_cast<PluginInfo*>(pluginById(id));
    if (!info || info->instance) return false; // already loaded

    // Permission grant
    QSet<QString> granted;
    if (info->metaData.trusted()) {
        // Auto-grant all declared
        const auto perms = info->metaData.permissions();
        for (const auto &p : perms) granted.insert(p);
    } else {
        granted = loadGrantedPermissions(id);
        const auto declared = info->metaData.permissions();
        QSet<QString> needed(declared.begin(), declared.end());
        if ((needed - granted).isEmpty() == false) {
            // Untrusted plugin with ungranted declared perms — prompt
            PluginPermissionGrantDialog dlg(info->metaData.base().name(),
                info->metaData.base().description(), declared);
            if (dlg.exec() != QDialog::Accepted) return false;
            granted = dlg.grantedIfAccepted();
            saveGrantedPermissions(id, granted);
        }
    }

    // Factory load
    auto factoryResult = KPluginFactory::loadFactory(info->metaData.base());
    if (!factoryResult.plugin) {
        qWarning() << "Failed to load" << id << ":" << factoryResult.errorString;
        return false;
    }
    auto *plugin = factoryResult.plugin->create<Plugin>(this);
    if (!plugin) { qWarning() << "Factory returned nullptr for" << id; return false; }

    info->context = new PluginContext(info->metaData, granted);
    // setCoreServices() called by CorbomiteApp wiring in Task 13
    info->instance = plugin;
    plugin->load(info->context);
    info->enabled = true;

    KConfigGroup cfg(KSharedConfig::openConfig(), QStringLiteral("Plugins"));
    cfg.writeEntry(id + QStringLiteral("Enabled"), true);

    Q_EMIT pluginLoaded(id);
    Q_EMIT pluginEnabled(id);
    return true;
}

bool PluginManager::disablePlugin(const QString &id)
{
    auto *info = const_cast<PluginInfo*>(pluginById(id));
    if (!info || !info->instance) return false;

    Q_EMIT pluginUnloading(id);
    info->instance->unload();
    delete info->instance;
    delete info->context;
    info->instance = nullptr;
    info->context = nullptr;
    info->enabled = false;

    KConfigGroup cfg(KSharedConfig::openConfig(), QStringLiteral("Plugins"));
    cfg.writeEntry(id + QStringLiteral("Enabled"), false);

    Q_EMIT pluginDisabled(id);
    return true;
}

void PluginManager::loadEnabledStateFromConfig()
{
    KConfigGroup cfg(KSharedConfig::openConfig(), QStringLiteral("Plugins"));
    for (auto &info : m_plugins) {
        const auto id = info.metaData.base().pluginId();
        const bool defaultOn =
            info.metaData.base().rawData()
                .value(QStringLiteral("KPlugin")).toObject()
                .value(QStringLiteral("EnabledByDefault")).toBool(false);
        const bool enabled = cfg.readEntry(id + QStringLiteral("Enabled"), defaultOn);
        if (enabled) enablePlugin(id);
    }
}

QSet<QString> PluginManager::loadGrantedPermissions(const QString &id) const
{
    KConfigGroup cfg(KSharedConfig::openConfig(), QStringLiteral("PluginPermissions"));
    const QStringList list = cfg.readEntry(id + QStringLiteral("Granted"), QStringList());
    return QSet<QString>(list.begin(), list.end());
}

void PluginManager::saveGrantedPermissions(const QString &id, const QSet<QString> &g)
{
    KConfigGroup cfg(KSharedConfig::openConfig(), QStringLiteral("PluginPermissions"));
    cfg.writeEntry(id + QStringLiteral("Granted"),
                   QStringList(g.begin(), g.end()));
    cfg.sync();
}
```

- [ ] **Step 4: Build + run tests, verify they pass**

Expected: all tests PASS. Fixture plugin builds to a `.so` during test configure.

- [ ] **Step 5: Commit**

```bash
git add libs/core/include/corbomite/core/PluginManager.h \
        libs/core/src/PluginManager.cpp \
        tests/core/tst_plugin_manager.cpp \
        tests/core/fixture-plugin/ \
        tests/core/CMakeLists.txt
git commit -m "feat(core): PluginManager load/unload + KConfig persistence

Cluster Q phase 1: enablePlugin / disablePlugin / KConfig round-trip
for enabled state + granted permissions. Untrusted plugins trigger
PluginPermissionGrantDialog on first enable with ungranted perms."
```

---

## Task 7: Proxy wire-up — Vault (reader + writer)

**Files:**
- Modify: `libs/core/src/proxies/VaultReader.cpp`
- Modify: `libs/core/src/proxies/VaultWriter.cpp`
- Modify: `libs/core/include/corbomite/core/proxies/{VaultReader,VaultWriter}.h` — add method signatures
- Create: `tests/core/tst_proxy_vault.cpp`

- [ ] **Step 1: Write failing test**

```cpp
// tests/core/tst_proxy_vault.cpp
#include <QTest>
#include <QTemporaryDir>
#include "corbomite/core/proxies/VaultReader.h"
#include "corbomite/core/proxies/VaultWriter.h"
#include "corbomite/storage/Vault.h"

class TestProxyVault : public QObject
{
    Q_OBJECT
private slots:
    void readerReturnsNoteBytes();
    void writerCreatesNote();
    void writerRenameMovesFile();
};

void TestProxyVault::readerReturnsNoteBytes()
{
    QTemporaryDir vaultDir;
    // ... seed a note.md with known content ...
    Corbomite::Vault vault(vaultDir.path());
    Corbomite::VaultReader reader(&vault);
    const QByteArray body = reader.read(QStringLiteral("note.md"));
    QCOMPARE(body, QByteArrayLiteral("# Hello\n"));
}
// ... other tests forward-compatible with VaultWriter::create / rename ...

QTEST_MAIN(TestProxyVault)
#include "tst_proxy_vault.moc"
```

- [ ] **Step 2: Run, verify fails**

- [ ] **Step 3: Extend proxy headers + implementations**

`VaultReader.h`:
```cpp
namespace Corbomite {
class Vault;
class VaultReader
{
public:
    explicit VaultReader(Vault *v) : m_vault(v) {}
    QByteArray read(const QString &relativePath) const;
    bool exists(const QString &relativePath) const;
    QStringList list(const QString &subdir = {}) const;
private:
    Vault *m_vault;
};
}
```

`VaultReader.cpp` — forward each method to the corresponding `Vault` API.

`VaultWriter.h`:
```cpp
namespace Corbomite {
class Vault;
class VaultWriter
{
public:
    explicit VaultWriter(Vault *v) : m_vault(v) {}
    bool create(const QString &relativePath, const QByteArray &body);
    bool write(const QString &relativePath, const QByteArray &body);
    bool rename(const QString &oldPath, const QString &newPath);
    bool remove(const QString &relativePath);
private:
    Vault *m_vault;
};
}
```

`VaultWriter.cpp` — same pattern.

- [ ] **Step 4: Build + run tests, verify pass**

- [ ] **Step 5: Commit**

```bash
git add libs/core/include/corbomite/core/proxies/VaultReader.h \
        libs/core/include/corbomite/core/proxies/VaultWriter.h \
        libs/core/src/proxies/VaultReader.cpp \
        libs/core/src/proxies/VaultWriter.cpp \
        tests/core/tst_proxy_vault.cpp \
        tests/core/CMakeLists.txt
git commit -m "feat(core): wire VaultReader + VaultWriter proxies to Vault

Cluster Q phase 1: proxies forward to Corbomite::Vault;
vault.read / vault.write permissions gate access."
```

---

## Task 8: Proxy wire-up — Metadata + Workspace

**Files:**
- Modify: `libs/core/include/corbomite/core/proxies/{MetadataCacheReader,WorkspaceController}.h`
- Modify: `libs/core/src/proxies/{MetadataCacheReader,WorkspaceController}.cpp`
- Create: `tests/core/tst_proxy_metadata.cpp`, `tst_proxy_workspace.cpp`

Same TDD pattern as Task 7: write failing test, verify fail, implement, verify pass, commit.

**MetadataCacheReader** methods: `getFileCache(path)`, `backlinksFor(target)`, `outlinksFor(path)`, `tagsIn(path)`, `allTags()`. Each forwards to the underlying `MetadataCache`.

**WorkspaceController** methods: `openFile(path)`, `activeLeafId()`, `splitLeaf(leafId, orient)`, `closeLeaf(leafId)`, `popoutLeaf(leafId)`. Each forwards to the underlying `Workspace`.

- [ ] **Step 1-5:** follow Task 7 pattern.

Commit:
```bash
git commit -m "feat(core): wire MetadataCacheReader + WorkspaceController proxies

Cluster Q phase 1: metadata.read + workspace permissions gate access."
```

---

## Task 9: Proxy wire-up — UI (Commands, Views, Menus)

**Files:**
- Modify: `libs/core/include/corbomite/core/proxies/{CommandRegistrar,ViewRegistrar,MenuInjector}.h`
- Modify: `libs/core/src/proxies/{CommandRegistrar,ViewRegistrar,MenuInjector}.cpp`
- Create: `tests/core/tst_proxy_ui.cpp`

**CommandRegistrar** methods: `addCommand(Command&)` with auto-namespacing (`cmd.id = pluginId + ":" + cmd.id`), `removeCommand(localId)`. Mirror `PluginInstance`'s existing auto-namespacing logic.

**ViewRegistrar** methods: `registerView(type, factory)`, `registerExtensions(exts, type)`, `unregisterView(type)`. Forward to `ViewRegistry`.

**MenuInjector** methods: `onFileMenuBuilt(handler)`, `onEditorMenuBuilt(handler)`, etc. — thin wrappers over `MenuEventEmitter` signals with auto-disconnect on plugin unload.

- [ ] **Step 1-5:** follow Task 7 pattern.

Commit:
```bash
git commit -m "feat(core): wire CommandRegistrar + ViewRegistrar + MenuInjector proxies

Cluster Q phase 1: ui.commands + ui.views + ui.menus permissions
gate UI-registration surfaces. Auto-namespacing, auto-cleanup on unload."
```

---

## Task 10: Proxy wire-up — Secrets + Process

**Files:**
- Modify: `libs/core/include/corbomite/core/proxies/{SecretStorage,ProcessSpawner}.h`
- Modify: `libs/core/src/proxies/{SecretStorage,ProcessSpawner}.cpp`
- Create: `tests/core/tst_proxy_secrets_process.cpp`

**SecretStorage** methods: `setSecret(id, value)`, `getSecret(id)`, `deleteSecret(id)`, `listSecrets()`. Backend: KWallet via `KF6::Wallet` if available, else QtKeychain fallback.

**ProcessSpawner** methods: `start(program, args)`, `startDetached(program, args)`. Wraps `QProcess`; logs every invocation at qCDebug with plugin id.

- [ ] **Step 1-5:** follow Task 7 pattern.

Commit:
```bash
git commit -m "feat(core): wire SecretStorage + ProcessSpawner proxies

Cluster Q phase 1: secrets + process permissions gate KWallet and
QProcess access respectively; all calls logged with plugin id."
```

---

# Phase 2 — Settings + app wiring

## Task 11: Plugins page in SettingsDialog

**Files:**
- Create: `src/dialogs/PluginsPage.h`, `src/dialogs/PluginsPage.cpp`
- Modify: `src/dialogs/SettingsDialog.cpp` — add PluginsPage to tab list
- Modify: `src/dialogs/CMakeLists.txt`

- [ ] **Step 1: Write failing test**

Since this is a GUI page, the test uses `offscreen` Qt and verifies the widget tree after construction:

```cpp
// tests/dialogs/tst_plugins_page.cpp
#include <QTest>
#include <QListWidget>
#include "dialogs/PluginsPage.h"
#include "corbomite/core/PluginManager.h"

class TestPluginsPage : public QObject
{
    Q_OBJECT
private slots:
    void listsDiscoveredPlugins();
    void checkboxTogglesEnableState();
    void trustedPluginPermissionsAreReadOnly();
};
```

Implementation tests: given a `PluginManager` with N discovered plugins, the page's `QListWidget` has N rows. Clicking the enable checkbox calls `PluginManager::enablePlugin`. For trusted plugins, the permission checkboxes are `setEnabled(false)`.

- [ ] **Step 2: Verify fails**

- [ ] **Step 3: Implement**

`PluginsPage.h`:
```cpp
#pragma once
#include <QWidget>
namespace Corbomite { class PluginManager; }

class PluginsPage : public QWidget
{
    Q_OBJECT
public:
    PluginsPage(Corbomite::PluginManager *mgr, QWidget *parent = nullptr);

private Q_SLOTS:
    void onPluginToggled(bool);
    void onPermissionToggled(bool);
    void refreshList();

private:
    Corbomite::PluginManager *m_mgr;
    // QListWidget or similar — see KCMUtils::KPluginWidget for reference
    class QListWidget *m_list;
    class QWidget *m_detailPane;
};
```

`PluginsPage.cpp` — build a `QSplitter` with `QListWidget` left, detail pane right. Populate list from `m_mgr->pluginCount()`. On selection, update detail pane with per-plugin info: name, version, author, declared permissions (checkboxes), configure button if `plugin->configPages() > 0`.

Modify `SettingsDialog.cpp` to include:
```cpp
auto *pluginsPage = new PluginsPage(m_pluginManager, this);
addPage(pluginsPage, i18n("Plugins"),
        QStringLiteral("preferences-plugin"));
```

- [ ] **Step 4: Build + run, verify passes**

- [ ] **Step 5: Commit**

```bash
git commit -m "feat(app): add Plugins page to SettingsDialog

Cluster Q phase 2: per-plugin enable checkbox + permission review +
configure button. Backed by Corbomite::PluginManager."
```

---

## Task 12: Wire PluginManager into CorbomiteApp

**Files:**
- Modify: `src/app/CorbomiteApp.h`, `src/app/CorbomiteApp.cpp`
- Modify: `src/app/MainWindow.h`, `src/app/MainWindow.cpp`
- Create: `tests/integration/tst_plugin_lifecycle.cpp`

- [ ] **Step 1: Write failing integration test**

```cpp
// tests/integration/tst_plugin_lifecycle.cpp
#include <QTest>
#include "app/CorbomiteApp.h"
#include "app/MainWindow.h"

class TestPluginLifecycle : public QObject
{
    Q_OBJECT
private slots:
    void appStartupLoadsEnabledPlugins();
    void pluginViewAppearsInMainWindow();
    void pluginUnloadRemovesView();
};
```

Using a fixture plugin that registers a simple ToolView; verify the tool view appears after app startup.

- [ ] **Step 2: Verify fails**

- [ ] **Step 3: Implement**

In `CorbomiteApp`:
```cpp
class CorbomiteApp : public KUniqueApplication {
public:
    // ...
    Corbomite::PluginManager *pluginManager() const { return m_pluginManager; }

private:
    void initializePlugins();
    void shutdownPlugins();
    Corbomite::PluginManager *m_pluginManager = nullptr;
};
```

Implementation:
```cpp
void CorbomiteApp::initializePlugins()
{
    m_pluginManager = new Corbomite::PluginManager(this);
    m_pluginManager->discoverPlugins();
    m_pluginManager->loadEnabledStateFromConfig(); // loads + calls enablePlugin() for each enabled

    // For each loaded plugin, connect to pluginLoaded/pluginUnloading to
    // propagate view creation/destruction to every MainWindow
    connect(m_pluginManager, &Corbomite::PluginManager::pluginLoaded,
            this, &CorbomiteApp::onPluginLoaded);
    connect(m_pluginManager, &Corbomite::PluginManager::pluginUnloading,
            this, &CorbomiteApp::onPluginUnloading);
}

void CorbomiteApp::shutdownPlugins()
{
    // Reverse order unload
    for (int i = m_pluginManager->pluginCount() - 1; i >= 0; --i) {
        const auto &info = m_pluginManager->pluginByIndex(i);
        if (info.instance) {
            m_pluginManager->disablePlugin(info.metaData.base().pluginId());
        }
    }
}
```

In `MainWindow`:
```cpp
// During setup:
for (int i = 0; i < app->pluginManager()->pluginCount(); ++i) {
    const auto &info = app->pluginManager()->pluginByIndex(i);
    if (!info.instance) continue;
    hostPluginView(info.instance);
}

// Slot for pluginLoaded signal:
void MainWindow::onPluginLoaded(const QString &id)
{
    auto *info = app->pluginManager()->pluginById(id);
    if (info && info->instance) hostPluginView(info->instance);
}

void MainWindow::hostPluginView(Corbomite::Plugin *plugin)
{
    QObject *view = plugin->createView(this);
    if (!view) return;
    // Dispatch on view type: QWidget → ToolView, QWidget-that-is-a-WorkspaceLeaf-view → Workspace tab, etc.
    // For Q's 8 plugins, most are ToolView-based. GraphViewTab is a main-area tab.
    m_hostedViews[plugin] = view;
}
```

- [ ] **Step 4: Build + run, verify passes**

Expected: integration test passes.

- [ ] **Step 5: Commit**

```bash
git commit -m "feat(app): wire PluginManager into CorbomiteApp startup

Cluster Q phase 2: app discovers + loads enabled plugins at startup;
propagates plugin views to MainWindow via createView(mainWindow);
reverse-order unload at shutdown."
```

---

# Phase 3 — Pilot plugin (Backlinks)

## Task 13: Migrate Backlinks to InternalPlugin (canonical pattern)

**Files:**
- Create: `src/plugins/backlinks/BacklinksPlugin.h`, `src/plugins/backlinks/BacklinksPlugin.cpp`
- Create: `src/plugins/backlinks/BacklinksView.h`, `src/plugins/backlinks/BacklinksView.cpp` (moved from `src/sidebar/BacklinksPanel.{h,cpp}`)
- Create: `src/plugins/backlinks/metadata.json`
- Create: `src/plugins/backlinks/CMakeLists.txt`
- Create: `src/plugins/backlinks/tests/tst_backlinks_plugin.cpp`
- Modify: `src/CMakeLists.txt` — add `add_subdirectory(plugins/backlinks)`
- Modify: `src/sidebar/CMakeLists.txt` — remove BacklinksPanel sources
- Modify: `src/app/MainWindow.cpp` — remove `m_backlinksPanel` construction + `connect()` wiring
- Modify: `src/app/MainWindow.h` — remove `m_backlinksPanel` member
- Delete: `src/sidebar/BacklinksPanel.h`, `src/sidebar/BacklinksPanel.cpp` (moved, not simply deleted)

- [ ] **Step 1: Write the plugin test**

```cpp
// src/plugins/backlinks/tests/tst_backlinks_plugin.cpp
#include <QTest>
#include "BacklinksPlugin.h"
#include "corbomite/core/PluginContext.h"
#include "corbomite/core/proxies/MetadataCacheReader.h"
#include "corbomite/core/proxies/ViewRegistrar.h"

class TestBacklinksPlugin : public QObject
{
    Q_OBJECT
private slots:
    void createsViewWithGrantedContext();
    void returnsNullWithoutMetadataReadPermission();
};

void TestBacklinksPlugin::createsViewWithGrantedContext()
{
    // Construct plugin, build context with granted metadata.read + ui.views,
    // call plugin->load(&ctx), call plugin->createView(mockMainWindow),
    // assert returned QWidget is a BacklinksView instance.
}
```

- [ ] **Step 2: Verify fails**

- [ ] **Step 3: Implement the plugin**

`src/plugins/backlinks/BacklinksPlugin.h`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "corbomite/core/Plugin.h"

class BacklinksPlugin : public Corbomite::Plugin
{
    Q_OBJECT
public:
    BacklinksPlugin(QObject *parent = nullptr, const QVariantList & = {});
    ~BacklinksPlugin() override;

    QObject *createView(Corbomite::MainWindow *mw) override;
};
```

`src/plugins/backlinks/BacklinksPlugin.cpp`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "BacklinksPlugin.h"
#include "BacklinksView.h"
#include "corbomite/core/PluginContext.h"
#include "corbomite/core/proxies/MetadataCacheReader.h"

#include <KPluginFactory>

K_PLUGIN_FACTORY_WITH_JSON(BacklinksPluginFactory, "metadata.json",
                           registerPlugin<BacklinksPlugin>();)

BacklinksPlugin::BacklinksPlugin(QObject *parent, const QVariantList &)
    : Corbomite::Plugin(parent) {}

BacklinksPlugin::~BacklinksPlugin() = default;

QObject *BacklinksPlugin::createView(Corbomite::MainWindow *mw)
{
    auto *ctx = context();
    auto *metadata = ctx ? ctx->metadataCache() : nullptr;
    if (!metadata) {
        qWarning() << "BacklinksPlugin: metadata.read permission not granted; "
                      "view will not be created";
        return nullptr;
    }
    return new BacklinksView(metadata, mw);
}

#include "BacklinksPlugin.moc"
```

Move `sidebar/BacklinksPanel.{h,cpp}` → `plugins/backlinks/BacklinksView.{h,cpp}` (class rename, file rename). Adjust its constructor to take a `MetadataCacheReader*` instead of the raw `MetadataCache*`.

`src/plugins/backlinks/metadata.json`:
```json
{
  "KPlugin": {
    "Id": "corbomite-backlinks",
    "Name": "Backlinks",
    "Description": "Show notes that link to the active note",
    "Icon": "link-symbolic",
    "Version": "1.0",
    "License": "GPL-3.0-or-later",
    "Category": "Core",
    "EnabledByDefault": true,
    "Authors": [{"Name": "Corbomite Developers"}]
  },
  "X-Corbomite-Trusted": true,
  "X-Corbomite-Permissions": ["metadata.read", "ui.views"],
  "X-Corbomite-MinVersion": "0.1.0"
}
```

`src/plugins/backlinks/CMakeLists.txt`:
```cmake
add_library(corbomite-backlinks MODULE)
target_sources(corbomite-backlinks PRIVATE
    BacklinksPlugin.cpp
    BacklinksView.cpp
)
target_link_libraries(corbomite-backlinks
    Corbomite::Core
    Corbomite::Storage
    KF6::I18n
    KF6::CoreAddons
    Qt6::Widgets
)
install(TARGETS corbomite-backlinks
        DESTINATION ${KDE_INSTALL_PLUGINDIR}/corbomite/)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
```

In `src/app/MainWindow.cpp`, remove (around line 882):
```cpp
m_backlinksPanel = new BacklinksPanel(backlinksView);
connect(m_backlinksPanel, &BacklinksPanel::noteActivated, ...);
```

Remove `#include "sidebar/BacklinksPanel.h"` (replaced by nothing — MainWindow no longer knows about Backlinks).

Remove `m_backlinksPanel` member from `MainWindow.h`.

Adapt the active-leaf signal propagation: plugin hosts receive `activeLeafChanged` via the `PluginContext`'s `WorkspaceController`; `BacklinksView` subscribes there.

- [ ] **Step 4: Build + run tests, verify passes**

```bash
cd build && cmake --build . 2>&1 | tail -20 && ctest -R backlinks --output-on-failure
```

Also run full suite to catch any MainWindow-integration regression:
```bash
cd build && ctest --output-on-failure 2>&1 | tail -30
```

Expected: all tests pass (modulo 4 known-flaky). The integration test `tst_plugin_lifecycle` should now actually load the Backlinks plugin at startup.

- [ ] **Step 5: Commit**

```bash
git add src/plugins/backlinks/ \
        src/app/MainWindow.h src/app/MainWindow.cpp \
        src/CMakeLists.txt src/sidebar/CMakeLists.txt
git rm src/sidebar/BacklinksPanel.h src/sidebar/BacklinksPanel.cpp
git commit -m "refactor(app): migrate Backlinks panel to InternalPlugin

Cluster Q phase 3 (pilot): Backlinks is now an .so plugin loaded
via KPluginFactory. Moved sidebar/BacklinksPanel.{h,cpp} to
src/plugins/backlinks/BacklinksView.{h,cpp}; MainWindow loses
direct construction. Pattern canonical for subsequent panel migrations."
```

---

# Phase 4 — Remaining panel migrations

## Task 14: Migrate Outlinks to InternalPlugin

**Files:**
- Create: `src/plugins/outlinks/OutlinksPlugin.{h,cpp}`, `OutlinksView.{h,cpp}`, `metadata.json`, `CMakeLists.txt`, `tests/`
- Modify: `src/CMakeLists.txt`, `src/sidebar/CMakeLists.txt`, `src/app/MainWindow.{h,cpp}`
- Delete: `src/sidebar/OutlinksPanel.{h,cpp}`

Permissions: `metadata.read`, `ui.views`.

Pattern identical to Task 13 — OutlinksPlugin wraps OutlinksView (moved from sidebar), `metadata.json` declares perms, CMake target installs to plugin dir, MainWindow loses `m_outlinksPanel`.

- [ ] **Step 1:** Write per-plugin test `tst_outlinks_plugin.cpp` mirroring Task 13's BacklinksPlugin test.
- [ ] **Step 2:** Verify fails.
- [ ] **Step 3:** Implement. See Task 13 for the canonical pattern.
- [ ] **Step 4:** Build + run tests; verify full suite passes.
- [ ] **Step 5:** Commit:
```bash
git commit -m "refactor(app): migrate Outlinks panel to InternalPlugin

Cluster Q phase 4: Outlinks ships as .so plugin; MainWindow loses
direct construction."
```

---

## Task 15: Migrate Outline to InternalPlugin

**Files:** `src/plugins/outline/*` — `OutlinePlugin`, `OutlineView`, etc.

Permissions: `metadata.read`, `ui.views`, `workspace` (for "jump to heading" via active editor).

Same pattern as Task 13. Notes specific to Outline:
- `OutlineView` needs `WorkspaceController::activeLeaf()` to access the active editor for jump-to-line.
- Subscribe to `activeLeafChanged` via the WorkspaceController proxy.

- [ ] **Step 1-5:** follow Task 14 pattern.

Commit:
```bash
git commit -m "refactor(app): migrate Outline panel to InternalPlugin

Cluster Q phase 4: Outline ships as .so plugin; uses metadata.read +
ui.views + workspace permissions."
```

---

## Task 16: Migrate Properties to InternalPlugin

**Files:** `src/plugins/properties/*` — `PropertiesPlugin`, `PropertiesView`, etc. Also moves `PropertyEditorWidget.{h,cpp}` + `PropertyType` enum if they're closely coupled.

Permissions: `vault.write` (to write frontmatter via `FrontMatterWriter`), `metadata.read` (to query frontmatter state), `ui.views`.

Notes:
- `PropertiesView`'s `FrontMatterWriter` writeback routes through `VaultWriter` proxy now — extend `VaultWriter` with `writeFrontMatter(path, yaml)` if not already present (else add in this task).
- 500ms debounced writeback + subscribe to `MetadataCacheReader::cacheChanged` (add the signal to the proxy).

- [ ] **Step 1-5:** Follow Task 14 pattern.

Commit:
```bash
git commit -m "refactor(app): migrate Properties panel to InternalPlugin

Cluster Q phase 4: Properties ships as .so plugin; uses vault.write +
metadata.read + ui.views."
```

---

## Task 17: Migrate Search to InternalPlugin

**Files:** `src/plugins/search/*` — `SearchPlugin`, `SearchView`, etc.

Permissions: `vault.read` (for file previews), `metadata.read` (for FTS), `ui.views`, `ui.commands` (Ctrl+Shift+F), `workspace` (open result → navigate to file).

Notes:
- SearchView uses `SQLiteIndex` which is part of `Corbomite::Storage`. `VaultReader` proxy needs a `search()` method OR we expose `SQLiteIndex` as part of `MetadataCacheReader` surface.
- Ctrl+Shift+F command registration goes through `CommandRegistrar` proxy.

- [ ] **Step 1-5:** Follow Task 14 pattern.

Commit:
```bash
git commit -m "refactor(app): migrate Search panel to InternalPlugin

Cluster Q phase 4: Search ships as .so plugin; uses vault.read +
metadata.read + ui.views + ui.commands + workspace."
```

---

## Task 18: Migrate FileExplorer to InternalPlugin

**Files:** `src/plugins/file-explorer/*` — `FileExplorerPlugin`, `FileExplorerView`, etc.

Permissions: `vault.read`, `vault.write` (rename/new/delete), `ui.views`, `ui.menus` (file-context menu items), `workspace` (open file on click).

Notes:
- File-menu items (New Note, Delete, Rename) go through `MenuInjector` proxy.
- FileExplorer's tree uses `VaultModel` — ensure `VaultReader` exposes the model OR the Plugin holds a reference via the context.

- [ ] **Step 1-5:** Follow Task 14 pattern.

Commit:
```bash
git commit -m "refactor(app): migrate FileExplorer panel to InternalPlugin

Cluster Q phase 4: FileExplorer ships as .so plugin; uses vault.read +
vault.write + ui.views + ui.menus + workspace."
```

---

## Task 19: Migrate LocalGraph to InternalPlugin

**Files:** `src/plugins/local-graph/*` — `LocalGraphPlugin`, `LocalGraphView`, etc.

Permissions: `metadata.read`, `ui.views`, `workspace`.

Pattern identical to Task 13. LocalGraph source files currently under `src/graph/` move into `src/plugins/local-graph/` — note that `GraphViewTab` (global graph view, main-area tab) is a SEPARATE plugin (Task 20), so only the sidebar local-graph bits move here.

- [ ] **Step 1-5:** Follow Task 14 pattern.

Commit:
```bash
git commit -m "refactor(app): migrate LocalGraph panel to InternalPlugin

Cluster Q phase 4: LocalGraph ships as .so plugin; uses metadata.read +
ui.views + workspace."
```

---

## Task 20: Migrate GraphView to InternalPlugin

**Files:** `src/plugins/graph-view/*` — `GraphViewPlugin`, `GraphViewWidget` (the actual `GraphViewTab`), etc.

Permissions: `metadata.read`, `ui.views`, `workspace`.

**Distinction from other plugins:** GraphView is a main-area tab, not a sidebar ToolView. `createView(mainWindow)` returns a `QWidget` that MainWindow routes into the `Workspace` tree (as a view type registered via `ViewRegistrar::registerView("graph", ...)`).

- [ ] **Step 1-5:** Follow Task 14 pattern.

Commit:
```bash
git commit -m "refactor(app): migrate GraphView tab to InternalPlugin

Cluster Q phase 4: GraphView ships as .so plugin; main-area view
type registered via ViewRegistrar."
```

---

# Phase 5 — Closeout

## Task 21: Retrospective, PROJECT-STATE, INDEX update

**Files:**
- Create: `docs/cluster-retros/cluster-q.md`
- Modify: `docs/PROJECT-STATE.md`
- Modify: `docs/superpowers/plans/INDEX.md`
- Update memory: `~/.claude/projects/-home-clinton-dev-Corbomite/memory/project_cluster_q_done.md` (new file)
- Modify: `~/.claude/projects/-home-clinton-dev-Corbomite/memory/MEMORY.md`

- [ ] **Step 1: Write retrospective**

Follow the template from `docs/cluster-retros/cluster-j.md` and `docs/cluster-retros/cluster-g.md`. Sections: "What changed vs the original plan", "What surprised", "Downstream effects", "Lessons for the next cluster". ~300-500 words.

- [ ] **Step 2: Update PROJECT-STATE**

Bump `Last updated:` line; update Current focus; flip Roadmap row Q to `Done`; prepend a Recent-decisions bullet; remove (or update) the Open-question about Q.

- [ ] **Step 3: Update INDEX.md**

Bump Last updated; row Q status → `Done`.

- [ ] **Step 4: Update memory**

Write `project_cluster_q_done.md` capturing load-bearing facts (e.g., "Corbomite plugins are KPluginFactory `.so`s at `${KDE_INSTALL_PLUGINDIR}/corbomite/`; trust enforced by install-path origin; 12-token permission model is declarative-contract not runtime sandbox").

Update `MEMORY.md` index entry.

- [ ] **Step 5: Commit**

```bash
git add docs/cluster-retros/cluster-q.md \
        docs/PROJECT-STATE.md \
        docs/superpowers/plans/INDEX.md
git commit -m "docs: close Cluster Q ritual 3 + retrospective

Cluster Q (internal-plugin wrapping + permissions) landed. 8 panels
migrated to .so plugins loaded via KPluginFactory; PluginManager +
PluginContext + 9 proxies + PluginPermissionGrantDialog + Settings
Plugins page shipped. Cluster N scope shrinks accordingly."
```

---

# Definition of Done

All tasks above completed, meaning:

1. `libs/core/` gains `Plugin`, `PluginContext`, `PluginManager`, `PluginMetaData`, `PluginPermissionGrantDialog` + 9 proxies with unit tests.
2. Eight InternalPlugins build as `.so` MODULE targets at `src/plugins/<slug>/`, install to `${KDE_INSTALL_PLUGINDIR}/corbomite/`, are discovered + loaded at app startup.
3. `MainWindow` no longer directly constructs any of the 8 migrated panels; all are created by their plugins' `createView(mainWindow)` methods.
4. Settings has a Plugins page listing all 8, with enable checkbox, declared-permissions review, and (where applicable) per-plugin configure page link.
5. KConfig round-trip: toggle plugin off in Settings → restart → plugin stays off; toggle back on → plugin loads + view appears.
6. Full test suite passes (modulo the 4 known-flaky).
7. Retrospective at `docs/cluster-retros/cluster-q.md`. PROJECT-STATE and INDEX updated.

# Blocks / Enables

- **Blocked by:** Cluster G (Workspace integration) — DONE. No other blockers.
- **Unblocks:** Cluster N (Plugin-ready surfaces) scope shrinks substantially. N becomes distribution-UX + sandbox-decision, not an architectural cluster.
- **Enables (deferred):** gradient phase 2 rendering plugins (once Markoff is stable-standalone), Obsidian-JS-shim track (if ever pursued), optional/graceful-degradation permissions.

# Preserved compat quirks

- Plugin command id auto-namespacing (`<pluginId>:<cmdId>`) — matches Obsidian.
- LIFO cleanup on unload — matches Obsidian's `Component.unload`.
- Ribbon id uses `<pluginId>:<title>` not `<pluginId>:<iconId>` — matches Obsidian's quirk.
- Plugin `data.json` format: pretty-printed JSON, 2-space indent, no trailing newline, keys in insertion order — matches Obsidian.

Note: atomic `QSaveFile` writes for `data.json` are a strict improvement over Obsidian's non-atomic `adapter.write`; compat-safe (plugins only care about `loadData()` returning `null` vs populated, not about crash-truncation behavior).
