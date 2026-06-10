# Cluster B — Plugin API surface completion (implementation plan)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Close the 16-item gap between Obsidian's plugin-registration verbs and Corbomite's plugin-host surface, drawing residuals from closed Cluster A.

**Architecture:** Each new verb follows the existing `CommandRegistrar` pattern — host owns a singleton registry, `PluginContext` lazily creates a permission-gated proxy on first plugin access, proxy prefixes ids with `<pluginId>:` and walks tracked ids on destruction for cleanup. New host-side substrates (status bar, lucide icons, protocol handlers, decoration providers, markdown renderer, expanded watcher) are added under the same model.

**Tech Stack:** C++20, Qt 6.8+, KDE Frameworks 6, KDDockWidgets 2, Markoff-family submodule, KPluginFactory plugin .so modules.

**Spec:** [`docs/superpowers/specs/2026-04-28-cluster-b-plugin-api-surface-design.md`](../specs/2026-04-28-cluster-b-plugin-api-surface-design.md)

---

## File structure

### New files (libs/core)

| Path | Responsibility |
|---|---|
| `libs/core/include/corbomite/core/PluginPermissions.h` | Single source of truth for permission token strings (replaces .cpp constants in PluginContext.cpp). |
| `libs/core/include/corbomite/core/proxies/HoverLinkSourceRegistrar.h` | Proxy over `HoverLinkSourceRegistry`. |
| `libs/core/src/proxies/HoverLinkSourceRegistrar.cpp` | Impl. |
| `libs/core/include/corbomite/core/proxies/EditorSuggestRegistrar.h` | Proxy over `EditorSuggestManager`. |
| `libs/core/src/proxies/EditorSuggestRegistrar.cpp` | Impl. |
| `libs/core/include/corbomite/core/proxies/PostProcessorRegistrar.h` | Proxy over `PostProcessorRegistry`. |
| `libs/core/src/proxies/PostProcessorRegistrar.cpp` | Impl. |
| `libs/core/include/corbomite/core/proxies/RibbonRegistrar.h` | Proxy over `RibbonToolBar::addRibbonIcon`. |
| `libs/core/src/proxies/RibbonRegistrar.cpp` | Impl. |
| `libs/core/include/corbomite/core/proxies/EmbedRegistrar.h` | Proxy over Markoff's `EmbedRegistry`. |
| `libs/core/src/proxies/EmbedRegistrar.cpp` | Impl. |
| `libs/core/include/corbomite/core/proxies/CodeBlockRegistrar.h` | Proxy over Markoff's `CodeBlockProcessorRegistry`. |
| `libs/core/src/proxies/CodeBlockRegistrar.cpp` | Impl. |
| `libs/core/include/corbomite/core/StatusBarRegistry.h` | Host-side registry of `(pluginId, widget)` tuples wired into `QMainWindow::statusBar()`. |
| `libs/core/src/StatusBarRegistry.cpp` | Impl. |
| `libs/core/include/corbomite/core/proxies/StatusBarRegistrar.h` | Proxy. |
| `libs/core/src/proxies/StatusBarRegistrar.cpp` | Impl. |
| `libs/core/include/corbomite/core/LucideIconRegistry.h` | Singleton mapping `lucide-*` names → `QIcon`. Bundled set + ad-hoc `addIcon`. |
| `libs/core/src/LucideIconRegistry.cpp` | Impl. |
| `libs/core/include/corbomite/core/proxies/LucideIconRegistrar.h` | Proxy for `addIcon`/`removeIcon`. |
| `libs/core/src/proxies/LucideIconRegistrar.cpp` | Impl. |
| `libs/core/include/corbomite/core/ProtocolHandlerRegistry.h` | Routes `corbomite://`/`obsidian://` URLs to registered plugin handlers. |
| `libs/core/src/ProtocolHandlerRegistry.cpp` | Impl. |
| `libs/core/include/corbomite/core/proxies/ProtocolHandlerRegistrar.h` | Proxy. |
| `libs/core/src/proxies/ProtocolHandlerRegistrar.cpp` | Impl. |
| `libs/core/include/corbomite/core/DecorationProviderRegistry.h` | Singleton list of decoration providers; consumed by Markoff's render path. |
| `libs/core/src/DecorationProviderRegistry.cpp` | Impl. |
| `libs/core/include/corbomite/core/Decoration.h` | POD struct + enum used by providers. |
| `libs/core/include/corbomite/core/proxies/DecorationProviderRegistrar.h` | Proxy. |
| `libs/core/src/proxies/DecorationProviderRegistrar.cpp` | Impl. |
| `libs/core/include/corbomite/core/MarkdownRenderer.h` | Free-function `render()` returning `QFuture<void>`. |
| `libs/core/src/MarkdownRenderer.cpp` | Impl. |

### Modified files

| Path | Change |
|---|---|
| `libs/vault/include/corbomite/vault/Plugin.h` | Add facade methods: `registerHoverLinkSource`, `registerEditorSuggest`, `registerMarkdownPostProcessor`, `addRibbonIcon`, `registerEmbed`, `registerMarkdownCodeBlockProcessor`, `addStatusBarItem`, `addIcon`, `registerObsidianProtocolHandler`, `registerEditorExtension`. Add `onExternalSettingsChange()` virtual. |
| `libs/vault/include/corbomite/vault/PluginContext.h` | Add lazy accessors for each new registrar. Inject pointers to host registries via setters used by `PluginManager`. |
| `libs/vault/src/PluginContext.cpp` | Add accessor impls + permission gates referencing `PluginPermissions.h`. Replace file-scope `kVaultRead` etc. with includes from the new header. |
| `libs/vault/src/PluginManager.cpp` | Wire new registries into `PluginContext`. Watch per-plugin `data.json` paths; invoke `Plugin::onExternalSettingsChange()` on watcher fire. |
| `libs/vault/include/corbomite/vault/Vault.h` | Add `Q_SIGNALS: void raw(const QString &relPath); void configChanged(const QString &relPath);`. |
| `libs/vault/src/Vault.cpp` | Emit `raw`/`configChanged` from extended watcher; extend `stampSelfWrite` ledger to cover `.obsidian/` paths. |
| `libs/vault/src/Watcher.cpp` | Remove `.obsidian/` exclusion (lines 28-39); add config-area filter to determine `configChanged` vs `raw`-only. |
| `src/app/RibbonToolBar.h`/`.cpp` | Already exposes `addRibbonIcon(Handle, ...)`; no changes needed. |
| `src/app/MainWindow.cpp` | Construct `StatusBarRegistry`, `LucideIconRegistry`, `ProtocolHandlerRegistry`, `DecorationProviderRegistry`; pass them through `PluginContext` setters. Wire `QDesktopServices::setUrlHandler("corbomite", &registry, "dispatch")` (and `"obsidian"` if opted in). |
| `libs/markoff-family/libs/markoff/include/markoff/Editor.h` | Add `void installDecorationProviders(QList<DecorationProviderHook*>)` (or equivalent global registry consult). |
| `libs/markoff-family/libs/markoff/src/...` | One virtual call site in the decoration build path that consults `Corbomite::DecorationProviderRegistry::instance().providers()` (or via a markoff-side type-erased interface to keep the dependency direction clean — see Phase 2 Task 2.4). |
| `libs/markoff-family` (submodule pin) | Bump to include the decoration hook. |

### New test files

| Path | What it covers |
|---|---|
| `libs/core/tests/tst_hover_link_source_registrar.cpp` | Phase 1 — proxy register/unregister cycle. |
| `libs/core/tests/tst_editor_suggest_registrar.cpp` | Phase 1. |
| `libs/core/tests/tst_post_processor_registrar.cpp` | Phase 1. |
| `libs/core/tests/tst_ribbon_registrar.cpp` | Phase 1. |
| `libs/core/tests/tst_embed_registrar.cpp` | Phase 1. |
| `libs/core/tests/tst_code_block_registrar.cpp` | Phase 1. |
| `libs/core/tests/tst_status_bar_registry.cpp` | Phase 2. |
| `libs/core/tests/tst_lucide_icon_registry.cpp` | Phase 2. |
| `libs/core/tests/tst_markdown_renderer.cpp` | Phase 2. |
| `libs/core/tests/tst_decoration_provider_registry.cpp` | Phase 2. |
| `libs/vault/tests/tst_vault_raw_event.cpp` | Phase 3 — `Vault::raw` emission + echo-suppression. |
| `libs/vault/tests/tst_vault_config_changed.cpp` | Phase 3 — `Vault::configChanged` emission. |
| `libs/core/tests/tst_protocol_handler_registry.cpp` | Phase 3. |
| `libs/vault/tests/tst_plugin_external_settings_change.cpp` | Phase 3 — `Plugin::onExternalSettingsChange` invocation. |
| `tests/plugins/cluster-b-kitchen-sink/` | End-of-cluster — kitchen-sink reference plugin exercising every new verb. |

### New docs

| Path | Content |
|---|---|
| `docs/plugin-development/permissions.md` | Per-token reference: what each gates, examples, deny criteria. |

---

## Pattern reference — proxy class template

Every Phase 1 proxy follows this skeleton (showing `HoverLinkSourceRegistrar` as canonical; concrete tasks below note the per-class differences):

**Header (`libs/core/include/corbomite/core/proxies/HoverLinkSourceRegistrar.h`):**
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

#include "corbomite/core/HoverLinkSource.h"

namespace Corbomite {

class HoverLinkSourceRegistry;

class HoverLinkSourceRegistrar : public QObject {
    Q_OBJECT
public:
    HoverLinkSourceRegistrar(HoverLinkSourceRegistry *registry,
                             QString pluginId,
                             QObject *parent = nullptr);
    ~HoverLinkSourceRegistrar() override;

    bool registerSource(const HoverLinkSource &source);
    void unregisterSource(const QString &localId);

private:
    HoverLinkSourceRegistry *m_registry = nullptr;
    QString m_pluginId;
    QStringList m_registeredIds;
};

} // namespace Corbomite
```

**Impl (`libs/core/src/proxies/HoverLinkSourceRegistrar.cpp`):**
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/proxies/HoverLinkSourceRegistrar.h"

#include "corbomite/core/HoverLinkSourceRegistry.h"

namespace Corbomite {

HoverLinkSourceRegistrar::HoverLinkSourceRegistrar(
    HoverLinkSourceRegistry *registry,
    QString pluginId,
    QObject *parent)
    : QObject(parent)
    , m_registry(registry)
    , m_pluginId(std::move(pluginId))
{}

HoverLinkSourceRegistrar::~HoverLinkSourceRegistrar()
{
    if (!m_registry) return;
    for (int i = m_registeredIds.size() - 1; i >= 0; --i) {
        m_registry->unregisterSource(m_registeredIds.at(i));
    }
}

bool HoverLinkSourceRegistrar::registerSource(const HoverLinkSource &source)
{
    if (!m_registry) return false;
    HoverLinkSource scoped = source;
    scoped.id = m_pluginId + QLatin1Char(':') + source.id;
    if (!m_registry->registerSource(scoped)) return false;
    m_registeredIds.append(scoped.id);
    return true;
}

void HoverLinkSourceRegistrar::unregisterSource(const QString &localId)
{
    if (!m_registry) return;
    const QString fullId = m_pluginId + QLatin1Char(':') + localId;
    m_registry->unregisterSource(fullId);
    m_registeredIds.removeAll(fullId);
}

} // namespace Corbomite
```

Each Phase 1 task substitutes:
- Class names (`HoverLinkSourceRegistrar` → `EditorSuggestRegistrar`, etc.)
- Registry type + header
- Item type (`HoverLinkSource` → registry-specific entry type)
- The `registry->register*` / `unregister*` method names

**PluginContext lazy accessor pattern (`libs/vault/src/PluginContext.cpp`):**
```cpp
HoverLinkSourceRegistrar *PluginContext::hoverLinkSources()
{
    using namespace Corbomite::Permissions;
    if (!hasPermission(QLatin1String(kUiRendering)) || !m_hoverLinkSourceRegistry) return nullptr;
    if (!m_hoverLinkSourcesRegistrar) {
        m_hoverLinkSourcesRegistrar = std::make_unique<HoverLinkSourceRegistrar>(
            m_hoverLinkSourceRegistry, m_pluginId, this);
    }
    return m_hoverLinkSourcesRegistrar.get();
}
```

**Test pattern (canonical `tst_hover_link_source_registrar.cpp`):**
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include "corbomite/core/HoverLinkSourceRegistry.h"
#include "corbomite/core/proxies/HoverLinkSourceRegistrar.h"

class TestHoverLinkSourceRegistrar : public QObject {
    Q_OBJECT
private slots:
    void registerPrefixesIdAndAppearsInRegistry();
    void unregisterRemovesEntry();
    void destructorRemovesAllRegistered();
};

void TestHoverLinkSourceRegistrar::registerPrefixesIdAndAppearsInRegistry()
{
    Corbomite::HoverLinkSourceRegistry registry;
    Corbomite::HoverLinkSourceRegistrar registrar(&registry, QStringLiteral("plug-a"));
    Corbomite::HoverLinkSource src;
    src.id = QStringLiteral("backlinks");
    QVERIFY(registrar.registerSource(src));
    QVERIFY(registry.isRegistered(QStringLiteral("plug-a:backlinks")));
}

void TestHoverLinkSourceRegistrar::unregisterRemovesEntry()
{
    Corbomite::HoverLinkSourceRegistry registry;
    Corbomite::HoverLinkSourceRegistrar registrar(&registry, QStringLiteral("plug-a"));
    Corbomite::HoverLinkSource src;
    src.id = QStringLiteral("backlinks");
    QVERIFY(registrar.registerSource(src));
    registrar.unregisterSource(QStringLiteral("backlinks"));
    QVERIFY(!registry.isRegistered(QStringLiteral("plug-a:backlinks")));
}

void TestHoverLinkSourceRegistrar::destructorRemovesAllRegistered()
{
    Corbomite::HoverLinkSourceRegistry registry;
    {
        Corbomite::HoverLinkSourceRegistrar registrar(&registry, QStringLiteral("plug-a"));
        Corbomite::HoverLinkSource a; a.id = QStringLiteral("a");
        Corbomite::HoverLinkSource b; b.id = QStringLiteral("b");
        registrar.registerSource(a);
        registrar.registerSource(b);
    }
    QVERIFY(!registry.isRegistered(QStringLiteral("plug-a:a")));
    QVERIFY(!registry.isRegistered(QStringLiteral("plug-a:b")));
}

QTEST_MAIN(TestHoverLinkSourceRegistrar)
#include "tst_hover_link_source_registrar.moc"
```

Each Phase 1 test subs in the analogous registry/registrar/entry-type. Add CMake hook in `libs/core/tests/CMakeLists.txt` per existing test entries.

---

## Phase 0 — Permission tokens header (prerequisite for all phases)

### Task 0.1 — Create `PluginPermissions.h`

**Files:**
- Create: `libs/core/include/corbomite/core/PluginPermissions.h`

- [ ] **Step 1: Create the header**

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

namespace Corbomite::Permissions {

// Existing tokens (moved from libs/vault/src/PluginContext.cpp:21-32)
inline constexpr auto kVaultRead    = "vault.read";
inline constexpr auto kVaultWrite   = "vault.write";
inline constexpr auto kVaultEvents  = "vault.events";
inline constexpr auto kMetadataRead = "metadata.read";
inline constexpr auto kWorkspace    = "workspace";
inline constexpr auto kUiCommands   = "ui.commands";
inline constexpr auto kUiViews      = "ui.views";
inline constexpr auto kUiMenus      = "ui.menus";
inline constexpr auto kNetwork      = "network";
inline constexpr auto kSecrets      = "secrets";
inline constexpr auto kProcess      = "process";
inline constexpr auto kConfig       = "config";

// Cluster B additions
inline constexpr auto kUiRendering  = "ui.rendering"; // popovers, embeds, post-processors, code-block processors
inline constexpr auto kUiEditor     = "ui.editor";    // editor suggest, decoration providers
inline constexpr auto kUiStatusbar  = "ui.statusbar"; // status bar widgets
inline constexpr auto kUiIcons      = "ui.icons";     // lucide-named icon registration
inline constexpr auto kProtocol     = "protocol";     // corbomite://, obsidian:// URL handlers

} // namespace Corbomite::Permissions
```

- [ ] **Step 2: Replace local consts in PluginContext.cpp**

Open `libs/vault/src/PluginContext.cpp`. Remove lines 20-33 (the `namespace { constexpr auto kVaultRead = ...` block). Add `#include "corbomite/core/PluginPermissions.h"` near the other includes. Replace each `QLatin1String(kVaultRead)` etc. with `QLatin1String(Permissions::kVaultRead)` (use `using namespace Corbomite::Permissions;` once at the top of impl methods, or qualify each).

- [ ] **Step 3: Build & test**

```bash
cmake --build build -j 10
cd build && ctest -R "tst_plugin" --output-on-failure
```

Expected: build green, all existing plugin tests pass (no behavior change).

- [ ] **Step 4: Commit**

```bash
git add libs/core/include/corbomite/core/PluginPermissions.h libs/vault/src/PluginContext.cpp
git commit -m "core+vault: extract permission tokens to public header"
```

---

## Phase 1 — Mechanical proxies over existing registries

Six tasks (1.1–1.6), one per registry. Each follows the proxy pattern documented above. Per-registry differences are noted inline.

### Task 1.1 — `HoverLinkSourceRegistrar`

**Files:**
- Create: `libs/core/include/corbomite/core/proxies/HoverLinkSourceRegistrar.h`
- Create: `libs/core/src/proxies/HoverLinkSourceRegistrar.cpp`
- Create: `libs/core/tests/tst_hover_link_source_registrar.cpp`
- Modify: `libs/core/CMakeLists.txt` (add proxy source to corbomite-core target)
- Modify: `libs/core/tests/CMakeLists.txt` (add test entry)
- Modify: `libs/vault/include/corbomite/vault/PluginContext.h` (lazy accessor + setter)
- Modify: `libs/vault/src/PluginContext.cpp` (impl)
- Modify: `libs/vault/include/corbomite/vault/Plugin.h` (facade method)

- [ ] **Step 1: Write failing test** — copy the canonical test pattern from "Pattern reference" above. Save as `libs/core/tests/tst_hover_link_source_registrar.cpp`.
- [ ] **Step 2: Add test entry to `libs/core/tests/CMakeLists.txt`** following existing pattern (e.g., `corbomite_add_test(tst_hover_link_source_registrar tst_hover_link_source_registrar.cpp)` or whatever helper is in use — read the file for the convention).
- [ ] **Step 3: Run** `cmake --build build -j 10 --target tst_hover_link_source_registrar`. Expected: link error, registrar class missing.
- [ ] **Step 4: Create header + impl** per pattern reference.
- [ ] **Step 5: Add proxy source to `libs/core/CMakeLists.txt`** (add path to `target_sources(corbomite-core …)` or equivalent).
- [ ] **Step 6: Build + run test**. Expected: 3 tests pass.
- [ ] **Step 7: Add to `Plugin.h` facade**:
```cpp
bool registerHoverLinkSource(const HoverLinkSource &source);
void unregisterHoverLinkSource(const QString &localId);
```
Implement in `Plugin.cpp` to delegate to `context()->hoverLinkSources()->registerSource(...)` etc.
- [ ] **Step 8: Add to `PluginContext`** — accessor + setter for the host registry pointer. `PluginManager` later wires it.
- [ ] **Step 9: Build full tree, run vault tests** to confirm no regression.
- [ ] **Step 10: Commit**:
```bash
git commit -m "core+vault: HoverLinkSourceRegistrar plugin proxy"
```

### Task 1.2 — `EditorSuggestRegistrar`

**Files:** analogous to Task 1.1 with these substitutions:
- Registry: `EditorSuggestManager` (from `corbomite/core/EditorSuggestManager.h`)
- Item type: `EditorSuggest *` (Obsidian-shape — instance ownership transfers; lookup [`docs/audit-2026-04-26/plugin.md`](../../audit-2026-04-26/plugin.md) §"EditorSuggest" for shape if uncertain)
- Permission token: `kUiEditor`
- Manager methods: check `EditorSuggestManager.h` for the precise verb names; substitute for `register*`/`unregister*`.

- [ ] **Step 1: Read** `libs/core/include/corbomite/core/EditorSuggestManager.h` to confirm the registration verbs.
- [ ] Steps 2–10: follow Task 1.1 substituting names. Test name: `tst_editor_suggest_registrar`. Commit message: `core+vault: EditorSuggestRegistrar plugin proxy`.

### Task 1.3 — `PostProcessorRegistrar`

**Files:** analogous:
- Registry: `PostProcessorRegistry` (from `corbomite/core/PostProcessorRegistry.h`)
- Item type: `MarkdownPostProcessor *` or callable (read header to confirm)
- Permission token: `kUiRendering`

- [ ] **Step 1: Read** `libs/core/include/corbomite/core/PostProcessorRegistry.h`.
- [ ] Steps 2–10: follow Task 1.1. Commit: `core+vault: PostProcessorRegistrar plugin proxy`.

### Task 1.4 — `RibbonRegistrar`

**Files:** analogous, with one twist — `RibbonToolBar` lives in `src/app/`, not `libs/core/`. Two options:
1. Move `RibbonToolBar` to `libs/core/` (out of scope for this cluster).
2. Have the registrar take a `QObject *` (or a small abstract `RibbonHandle` interface) so the `libs/core/` proxy doesn't depend on `src/app/`.

Pick option 2. Define minimal interface:

```cpp
// libs/core/include/corbomite/core/RibbonHandle.h
namespace Corbomite {
class RibbonHandle {
public:
    virtual ~RibbonHandle() = default;
    virtual QString addRibbonIcon(const QString &id, const QString &iconName,
                                   const QString &title, std::function<void()> cb) = 0;
    virtual void removeRibbonIcon(const QString &id) = 0;
};
}
```

`RibbonToolBar` (in `src/app/`) implements `RibbonHandle`. Registrar takes `RibbonHandle *`.

- Permission token: `kUiCommands` (existing)

- [ ] **Step 1: Read** `src/app/RibbonToolBar.h` for the existing `addRibbonIcon` signature; mirror it in `RibbonHandle`.
- [ ] **Step 2: Create `RibbonHandle.h`** as above.
- [ ] **Step 3: Make `RibbonToolBar` inherit `RibbonHandle`** and stub the two virtuals to call existing impl.
- [ ] Steps 4–13: follow Task 1.1. Test uses a `MockRibbonHandle` capturing add/remove calls. Commit: `core+app: RibbonRegistrar plugin proxy + RibbonHandle interface`.

### Task 1.5 — `EmbedRegistrar`

**Files:** analogous, with the wrinkle that `EmbedRegistry` lives in `libs/markoff-family/libs/markoff-core/include/markoff/EmbedRegistry.h`.

The `corbomite-core` target already links `markoff-core` indirectly. Confirm with:
```bash
grep -n "markoff-core\|markoff_core\|Markoff::Core" libs/core/CMakeLists.txt
```

If not directly linked, add `target_link_libraries(corbomite-core PUBLIC markoff_core)` (or whatever the precise target name is).

- Permission token: `kUiRendering`
- Item type: read `EmbedRegistry.h` for the register/unregister API.

- [ ] **Step 1: Read** `libs/markoff-family/libs/markoff-core/include/markoff/EmbedRegistry.h`.
- [ ] Steps 2–10: follow Task 1.1. Commit: `core+vault: EmbedRegistrar plugin proxy`.

### Task 1.6 — `CodeBlockRegistrar`

**Files:** analogous to Task 1.5:
- Registry: `Markoff::CodeBlockProcessorRegistry`
- Permission token: `kUiRendering`

- [ ] **Step 1: Read** `libs/markoff-family/libs/markoff-core/include/markoff/CodeBlockProcessorRegistry.h`.
- [ ] Steps 2–10: follow Task 1.1. Commit: `core+vault: CodeBlockRegistrar plugin proxy`.

### Task 1.7 — Wire all six into `MainWindow` + `PluginManager`

**Files:**
- Modify: `src/app/MainWindow.cpp` (construct registries, pass through `PluginManager::setPluginContextProvider` or whatever the existing seam is — read existing init code for the pattern)
- Modify: `libs/vault/src/PluginManager.cpp` (pass each registry pointer to `PluginContext` setters when creating contexts)

- [ ] **Step 1: Read** `src/app/MainWindow.cpp` for existing registry construction (e.g., `m_commandRegistry = new CommandRegistry(this)`).
- [ ] **Step 2: Read** `libs/vault/src/PluginManager.cpp` for the existing context-setup code path.
- [ ] **Step 3: Construct the four host registries** that don't already exist (`HoverLinkSourceRegistry`, `EditorSuggestManager`, `PostProcessorRegistry` — these may already exist on `MainWindow`; check first; the markoff-side `EmbedRegistry` and `CodeBlockProcessorRegistry` are separate singletons that may already be alive).
- [ ] **Step 4: Pass pointers to PluginContext** via the new setters added in 1.1–1.6.
- [ ] **Step 5: Build full tree, run all tests**. Expected: green.
- [ ] **Step 6: Commit**:
```bash
git commit -m "app+vault: wire Phase 1 registries into PluginContext"
```

---

## Phase 2 — New host-side surfaces

### Task 2.1 — `StatusBarRegistry` + `StatusBarRegistrar`

**Files:**
- Create: `libs/core/include/corbomite/core/StatusBarRegistry.h`
- Create: `libs/core/src/StatusBarRegistry.cpp`
- Create: `libs/core/include/corbomite/core/proxies/StatusBarRegistrar.h`
- Create: `libs/core/src/proxies/StatusBarRegistrar.cpp`
- Create: `libs/core/tests/tst_status_bar_registry.cpp`
- Modify: `libs/core/CMakeLists.txt`, `libs/core/tests/CMakeLists.txt`
- Modify: `src/app/MainWindow.cpp` (construct registry, wire to `statusBar()`)
- Modify: `libs/vault/include/corbomite/vault/Plugin.h` + `PluginContext`

**Registry shape:**
```cpp
class StatusBarRegistry : public QObject {
    Q_OBJECT
public:
    explicit StatusBarRegistry(QStatusBar *bar, QObject *parent = nullptr);
    QWidget *addItem(const QString &id);  // returns owned widget, parented to bar
    void removeItem(const QString &id);
private:
    QStatusBar *m_bar = nullptr;
    QHash<QString, QPointer<QWidget>> m_items;
};
```

`addItem` creates a new `QWidget` (or could take a caller-supplied widget — picking the latter for plugin-API parity: plugin builds its own widget, registry wires it in).

Revised:
```cpp
QWidget *addItem(const QString &id);  // reserves slot; returns container widget plugin populates
// or equivalently: void addItem(const QString &id, QWidget *widget); — pick this; closer to addPermanentWidget
void addItem(const QString &id, QWidget *widget);
```

`addItem` calls `m_bar->addPermanentWidget(widget)` and stores the QPointer. `removeItem` calls `m_bar->removeWidget(widget)` if non-null and erases from map.

- Permission token: `kUiStatusbar`

- [ ] **Step 1: Write test** `tst_status_bar_registry.cpp` — construct a `QStatusBar`, registry, add a `QLabel`, assert widget count went up; remove, assert widget removed.
- [ ] Steps 2–6: implement registry + registrar following pattern.
- [ ] **Step 7: Wire into MainWindow** — in `setupStatusBar()` (line 1759 per existing code), construct `StatusBarRegistry(statusBar(), this)` and store as `m_statusBarRegistry`.
- [ ] **Step 8: Plugin facade**: `Plugin::addStatusBarItem(QWidget *)` returns the widget after registering; auto-removes on plugin unload.
- [ ] **Step 9: Build, test, commit** — `core+app+vault: StatusBarRegistry + addStatusBarItem plugin verb`.

### Task 2.2 — `LucideIconRegistry` + `LucideIconRegistrar`

**Files:**
- Create: `libs/core/include/corbomite/core/LucideIconRegistry.h`
- Create: `libs/core/src/LucideIconRegistry.cpp`
- Create: `libs/core/include/corbomite/core/proxies/LucideIconRegistrar.h`
- Create: `libs/core/src/proxies/LucideIconRegistrar.cpp`
- Create: `libs/core/tests/tst_lucide_icon_registry.cpp`
- Create: `libs/core/data/icons/lucide/` (~50 SVG files — initial set; see step 1 below)
- Create: `libs/core/data/icons/lucide.qrc` (Qt resource file pointing at the SVGs)
- Modify: `libs/core/CMakeLists.txt` (compile the .qrc; add proxy source)

**Registry shape:**
```cpp
class LucideIconRegistry {
public:
    static LucideIconRegistry &instance();
    void registerBuiltins(); // load bundled SVGs from :/icons/lucide/<name>.svg
    void addIcon(const QString &name, const QByteArray &svg);
    void removeIcon(const QString &name);
    QIcon get(const QString &name) const;
private:
    QHash<QString, QIcon> m_icons;
};
```

The bundled set: pick the ~50 most-referenced lucide icons from Obsidian's command set. Recommended initial pull: `book-open, search, settings, file-text, folder, plus, x, chevron-right, arrow-up-down, copy, trash-2, calendar-days, command, edit, save, dot, more-horizontal, link, unlink, hash, list, list-checks, eye, eye-off, layout, layout-grid, sidebar, columns-2, file-plus, folder-plus, refresh-cw, sun, moon, sun-moon, settings-2, info, alert-triangle, archive, bookmark, star, pin, lock, unlock, monitor, palette, type, image, panel-left, panel-right, layout-template`.

Sources: https://lucide.dev (MIT license — bundle SVGs locally; do not fetch at runtime).

- Permission token: `kUiIcons`

- [ ] **Step 1: Acquire the lucide SVG set.** Either:
  - clone `https://github.com/lucide-icons/lucide` into `~/src/lucide`, copy the 50 SVGs from `icons/<name>.svg`; or
  - use an inline minimal SVG generator to produce placeholder paths (viable if network access is restricted; document as TODO follow-up to swap to real SVGs).

  Per the Corbomite memory's "harvest don't hand-roll" guidance, the lucide-icons repo is the canonical source. Plan path A.

- [ ] **Step 2: Place files** at `libs/core/data/icons/lucide/<name>.svg`.
- [ ] **Step 3: Create `lucide.qrc`**:
```xml
<RCC>
  <qresource prefix="/icons/lucide">
    <file>book-open.svg</file>
    <!-- one entry per file, in alphabetical order -->
  </qresource>
</RCC>
```
- [ ] **Step 4: Add to CMake** — `qt_add_resources(corbomite-core "lucide-icons" PREFIX "/" FILES ...)` or list .qrc as a source.
- [ ] **Step 5: Implement registry** — `registerBuiltins()` walks `:/icons/lucide/*.svg` and inserts each. `addIcon(name, svg)` writes svg bytes to a temp file or to a `QSvgRenderer`/`QIcon`.
- [ ] **Step 6: Test** — instantiate registry, register builtins, assert `get("book-open").isNull() == false`. Plus add/remove ad-hoc.
- [ ] **Step 7: Implement registrar** — proxy with destructor cleanup.
- [ ] **Step 8: Plugin facade** — `Plugin::addIcon(QString name, QByteArray svg)`.
- [ ] **Step 9: Build, test, commit**: `core: LucideIconRegistry + addIcon plugin verb`.

### Task 2.3 — `MarkdownRenderer::render`

**Files:**
- Create: `libs/core/include/corbomite/core/MarkdownRenderer.h`
- Create: `libs/core/src/MarkdownRenderer.cpp`
- Create: `libs/core/tests/tst_markdown_renderer.cpp`
- Modify: `libs/core/CMakeLists.txt`

**Header:**
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QFuture>
#include <QString>

class QObject;
class QWidget;

namespace Corbomite {

class Vault;

namespace MarkdownRenderer {
    QFuture<void> render(Vault *vault,
                          const QString &markdown,
                          QWidget *parent,
                          const QString &sourcePath,
                          QObject *lifetime);
}

} // namespace Corbomite
```

**Impl:**
- Instantiate a `Markoff::ReadingView` parented to `parent`.
- Call `setSource(sourcePath)` and `setMarkdown(markdown)` on it.
- Tie lifetime: `connect(lifetime, &QObject::destroyed, readingView, &QObject::deleteLater)`.
- Return a `QFuture<void>` that resolves once `ReadingView` signals all-renders-complete.
  - `Markoff::ReadingView` may not have an explicit "all rendered" signal today. If not: in this cluster, return a future that resolves on the next event-loop turn after `setMarkdown` (sync content). Mark math/mermaid completion as a Markoff-side enhancement (out of scope here; document in spec deferred section if not already).

- [ ] **Step 1: Read** `libs/markoff-family/libs/markoff/include/markoff/ReadingView.h` for the public render API. Identify whether there's an "all child renderers complete" signal.
- [ ] **Step 2: Write test** `tst_markdown_renderer.cpp` — construct a parent `QWidget`, call `render(nullptr, "# Hi", parent, "test.md", parent)`, await the future with `QFutureSynchronizer`, assert parent has one child of type `ReadingView`. Skip the math-complete assertion if the signal isn't there.
- [ ] **Step 3: Implement** per pattern.
- [ ] **Step 4: Build + test**.
- [ ] **Step 5: Commit**: `core: MarkdownRenderer::render API`.

### Task 2.4 — `DecorationProviderRegistry` + `Decoration` POD + Markoff hook

**Files (Corbomite side):**
- Create: `libs/core/include/corbomite/core/Decoration.h`
- Create: `libs/core/include/corbomite/core/DecorationProviderRegistry.h`
- Create: `libs/core/src/DecorationProviderRegistry.cpp`
- Create: `libs/core/include/corbomite/core/proxies/DecorationProviderRegistrar.h`
- Create: `libs/core/src/proxies/DecorationProviderRegistrar.cpp`
- Create: `libs/core/tests/tst_decoration_provider_registry.cpp`

**Files (Markoff side, submodule):**
- Modify: `libs/markoff-family/libs/markoff/include/markoff/DecorationProviderHook.h` (new — abstract interface in Markoff namespace)
- Modify: `libs/markoff-family/libs/markoff/src/...` (call site in the editor build path)

**Decoration POD (`Decoration.h`):**
```cpp
#pragma once
#include <QString>
#include <QVariant>

namespace Corbomite {

enum class DecorationKind {
    Highlight,
    InlineWidget,
    HoverBadge,
};

struct Decoration {
    int start = 0;     // character offset
    int end = 0;       // character offset (exclusive)
    DecorationKind kind = DecorationKind::Highlight;
    QVariantMap payload;
};

class DecorationProvider {
public:
    virtual ~DecorationProvider() = default;
    virtual QList<Decoration> produceDecorations(
        const QString &sourcePath, const QString &markdown) = 0;
};

} // namespace Corbomite
```

Note: passing markdown text rather than a `Markoff::Document*` keeps the dependency direction clean — Corbomite's DecorationProvider interface doesn't reach into Markoff types.

**Registry:**
```cpp
class DecorationProviderRegistry {
public:
    static DecorationProviderRegistry &instance();
    void registerProvider(const QString &id, DecorationProvider *provider);
    void unregisterProvider(const QString &id);
    QList<DecorationProvider*> providers() const;
};
```

**Markoff hook (`DecorationProviderHook.h` in markoff namespace):**
```cpp
namespace Markoff {
class DecorationProviderHook {
public:
    virtual ~DecorationProviderHook() = default;
    virtual QList<Markoff::Decoration> produce(
        const QString &sourcePath, const QString &markdown) = 0;
};

void installDecorationProviderHook(DecorationProviderHook *hook); // sets a global; nullptr clears
}
```

Markoff's editor build path consults the installed hook. Corbomite registers an adapter that bridges `DecorationProviderRegistry::providers()` into `Markoff::DecorationProviderHook::produce`.

This keeps Markoff's public surface minimal: one global hook setter + one virtual `produce()`. Corbomite owns the registry and the adapter.

- [ ] **Step 1: Add Markoff hook** — create `DecorationProviderHook.h` in markoff submodule. Wire one call site in the existing decoration build path (`Markoff::ReadingView::buildScene` or equivalent — read existing code to find).
- [ ] **Step 2: Build markoff submodule**, run markoff tests, commit submodule change.
- [ ] **Step 3: Bump submodule pin in Corbomite parent repo**.
- [ ] **Step 4: Create Corbomite Decoration POD + registry** (header + impl).
- [ ] **Step 5: Create adapter** that implements `Markoff::DecorationProviderHook` and translates `Corbomite::Decoration` → `Markoff::Decoration` (likely identical layout; cast or member-copy).
- [ ] **Step 6: Install adapter from `MainWindow`** at startup: `Markoff::installDecorationProviderHook(&adapter)`.
- [ ] **Step 7: Create registrar + test** following pattern.
- [ ] **Step 8: Plugin facade** — `Plugin::registerEditorExtension(DecorationProvider *)` (note the verb name; per the spec, decoration-only is the implementation but the verb retains the Obsidian-API name).
- [ ] **Step 9: Permission token**: `kUiEditor`.
- [ ] **Step 10: Build full tree, test, commit**:
```bash
git commit -m "core+markoff: DecorationProviderRegistry + registerEditorExtension plugin verb (decoration-only hook)"
```

---

## Phase 3 — Lifecycle / events

### Task 3.1 — `Vault::raw` + `Vault::configChanged` signals + watcher expansion

**Files:**
- Modify: `libs/vault/include/corbomite/vault/Vault.h` (add signals)
- Modify: `libs/vault/src/Vault.cpp` (emit + extend ledger)
- Modify: `libs/vault/src/Watcher.cpp` (remove `.obsidian/` exclusion at lines 28-39; route writes to new signals based on path)
- Create: `libs/vault/tests/tst_vault_raw_event.cpp`
- Create: `libs/vault/tests/tst_vault_config_changed.cpp`

- [ ] **Step 1: Add signals** to `Vault.h`:
```cpp
Q_SIGNALS:
    void raw(const QString &relPath);
    void configChanged(const QString &relPath);
```

- [ ] **Step 2: Read** `libs/vault/src/Watcher.cpp` to understand the existing exclusion mechanism + ledger interaction (`stampSelfWrite` calls).

- [ ] **Step 3: Remove `.obsidian/` exclusion** in `Watcher.cpp:28-39`. Add a classifier: if relPath starts with `.obsidian/`, route to `configChanged`-eligible path; otherwise normal vault file.

- [ ] **Step 4: Extend `Vault::stampSelfWrite`** to be called from `writeConfigJson` and `VaultConfig::serializeObsidianStyle` writers (read the source to find the call sites). Ensure self-writes don't fire `raw` or `configChanged`.

- [ ] **Step 5: Emit `raw`** from the watcher path for *every* external change (vault file or `.obsidian/`).

- [ ] **Step 6: Emit `configChanged`** from the watcher path only when relPath starts with `.obsidian/` AND ends with `.json`.

- [ ] **Step 7: Write `tst_vault_raw_event`** — load a vault, externally modify a vault file via the adapter, await signal, assert relPath. Then externally modify `.obsidian/data.json`, assert `raw` fires (since raw covers both). Stamp + write via Vault and assert no `raw` (echo-suppressed).

- [ ] **Step 8: Write `tst_vault_config_changed`** — externally modify `.obsidian/appearance.json`, assert `configChanged` fires with `.obsidian/appearance.json`. Modify a regular `.md` file, assert `configChanged` does NOT fire.

- [ ] **Step 9: Build + test**.

- [ ] **Step 10: Commit**: `vault: raw + configChanged events; expand watcher to .obsidian/`.

### Task 3.2 — `ProtocolHandlerRegistry` + xdg-mime registration

**Files:**
- Create: `libs/core/include/corbomite/core/ProtocolHandlerRegistry.h`
- Create: `libs/core/src/ProtocolHandlerRegistry.cpp`
- Create: `libs/core/include/corbomite/core/proxies/ProtocolHandlerRegistrar.h`
- Create: `libs/core/src/proxies/ProtocolHandlerRegistrar.cpp`
- Create: `libs/core/tests/tst_protocol_handler_registry.cpp`
- Modify: `src/app/MainWindow.cpp` (construct registry, register URL handlers via `QDesktopServices::setUrlHandler`)
- Modify: settings UI (defer the opt-in checkbox to a follow-up task — see Step 8 below)

**Registry shape:**
```cpp
class ProtocolHandlerRegistry : public QObject {
    Q_OBJECT
public:
    using Handler = std::function<void(const QUrl&)>;
    void registerHandler(const QString &action, Handler handler);
    void unregisterHandler(const QString &action);
    Q_INVOKABLE void dispatch(const QUrl &url); // slot for QDesktopServices::setUrlHandler

private:
    QHash<QString, Handler> m_handlers;
};
```

URL shape: `corbomite://<action>?<key1=value1>&<key2=value2>` and `obsidian://<action>?...`. `dispatch` extracts host (action), looks up handler, invokes with full URL (so handler can read query params).

xdg-mime registration: defer the actual `xdg-mime default <desktop> x-scheme-handler/corbomite` call to first-run via `QProcess`. For tests + dev, just `QDesktopServices::setUrlHandler` is enough (it intercepts in-process URL opens).

- [ ] **Step 1: Implement registry** + dispatch.
- [ ] **Step 2: Implement registrar** + permission gate (`kProtocol`).
- [ ] **Step 3: Test** — register a stub handler, call `dispatch(QUrl("corbomite://foo?bar=baz"))`, assert handler invoked with URL containing `bar=baz`.
- [ ] **Step 4: Wire into MainWindow** — construct registry, call `QDesktopServices::setUrlHandler("corbomite", registry, "dispatch")`.
- [ ] **Step 5: Plugin facade** — `Plugin::registerObsidianProtocolHandler(QString action, std::function<void(QUrl)>)`.
- [ ] **Step 6: First-run xdg-mime registration** for `corbomite://` only (opt-in for `obsidian://` deferred to Settings page work; document as known follow-up).
- [ ] **Step 7: Build, test, commit**: `core+app+vault: ProtocolHandlerRegistry + registerObsidianProtocolHandler plugin verb`.
- [ ] **Step 8: Note in punch-list** that Settings checkbox for `obsidian://` opt-in is open follow-up.

### Task 3.3 — `Plugin::onExternalSettingsChange()` + per-plugin `data.json` watcher

**Files:**
- Modify: `libs/vault/include/corbomite/vault/Plugin.h` (virtual `onExternalSettingsChange()`)
- Modify: `libs/vault/src/PluginManager.cpp` (per-plugin watcher; invoke virtual on signal)
- Create: `libs/vault/tests/tst_plugin_external_settings_change.cpp`

- [ ] **Step 1: Add virtual** to `Plugin.h`:
```cpp
public:
    virtual void onExternalSettingsChange() {}
```

- [ ] **Step 2: Read** `libs/vault/src/PluginManager.cpp` for existing `data.json` write paths and the per-plugin path resolution.

- [ ] **Step 3: Add `QFileSystemWatcher m_dataJsonWatcher`** to `PluginManager`; on plugin enable, add the plugin's `data.json` path to the watcher; on disable, remove.

- [ ] **Step 4: Connect watcher signal** — on `fileChanged(path)`, look up which plugin owns `path`, call `plugin->onExternalSettingsChange()`. Echo-suppress same-process writes (PluginManager already writes data.json — track last-write timestamp + skip if recent).

- [ ] **Step 5: Write test** — instantiate manager, enable a stub plugin, externally modify its data.json, assert virtual called.

- [ ] **Step 6: Build, test, commit**: `vault: onExternalSettingsChange lifecycle hook + per-plugin data.json watcher`.

---

## Phase 4 — Permissions docs + kitchen-sink reference plugin

### Task 4.1 — `docs/plugin-development/permissions.md`

**Files:**
- Create: `docs/plugin-development/permissions.md`

Per-token table:

| Token | Gates | Example plugins | Deny criteria |
|---|---|---|---|
| `vault.read` | Reading vault file content; receiving raw/config-changed events without write access | search-extension, backlink-aggregator | Plugin claims read but never reads — review |
| `vault.write` | Modify/create/delete/rename | note-template, daily-notes | Plugin doesn't need to write — deny |
| `vault.events` | `Vault::raw`, `configChanged`, change/rename/delete signals | linter, indexer | Plugin only renders, no event listening — deny |
| `metadata.read` | MetadataCache reads | breadcrumb-nav, related-files | n/a (read-only) |
| `workspace` | Open/close leaves, switch tabs | quick-switcher, tab-manager | Read-only viewer plugins — deny |
| `ui.commands` | `addCommand`, `addRibbonIcon` | most plugins | Hidden background plugins (rare) |
| `ui.views` | `registerView` (custom panes/leaves) | bases, kanban | Plugins with no UI — deny |
| `ui.menus` | `MenuInjector` | context-menu plugins | Plugins not adding menu entries — deny |
| `ui.rendering` | `registerHoverLinkSource`, `registerEmbed`, `registerMarkdownPostProcessor`, `registerMarkdownCodeBlockProcessor` | math, mermaid, custom-embed | Non-rendering plugins — deny |
| `ui.editor` | `registerEditorSuggest`, `registerEditorExtension` | autocomplete-dictionary, smart-typography | n/a |
| `ui.statusbar` | `addStatusBarItem` | word-count, sync-status | Most plugins — deny by default |
| `ui.icons` | `addIcon` | theme-extension, branding | Most plugins — deny |
| `network` | HTTP requests | sync, web-clipper | Local-only plugins — deny |
| `secrets` | QtKeychain via `SecretStorage` | sync-providers, oauth | n/a |
| `process` | Spawn external processes | git-integration, pandoc-export | Most plugins — deny |
| `config` | `data.json` read/write, `onExternalSettingsChange` | every plugin with persistent state | n/a |
| `protocol` | `registerObsidianProtocolHandler` | url-router, deep-link plugins | Most plugins — deny |

- [ ] **Step 1: Write file** with the table + intro paragraph + per-token expanded sections.
- [ ] **Step 2: Commit**: `docs: plugin permissions reference`.

### Task 4.2 — Kitchen-sink reference plugin

**Files:**
- Create: `tests/plugins/cluster-b-kitchen-sink/CMakeLists.txt`
- Create: `tests/plugins/cluster-b-kitchen-sink/KitchenSinkPlugin.h`
- Create: `tests/plugins/cluster-b-kitchen-sink/KitchenSinkPlugin.cpp`
- Create: `tests/plugins/cluster-b-kitchen-sink/metadata.json.in`
- Create: `tests/plugins/cluster-b-kitchen-sink/tst_kitchen_sink.cpp`

A plugin that exercises every new verb in `onload()`:
1. `addCommand("kitchen-sink-cmd", ...)` (existing)
2. `registerHoverLinkSource(...)` ← Task 1.1
3. `registerEditorSuggest(...)` ← Task 1.2
4. `registerMarkdownPostProcessor(...)` ← Task 1.3
5. `addRibbonIcon(...)` ← Task 1.4
6. `registerEmbed(...)` ← Task 1.5
7. `registerMarkdownCodeBlockProcessor(...)` ← Task 1.6
8. `addStatusBarItem(new QLabel("KS"))` ← Task 2.1
9. `addIcon("ks-icon", "<svg>...</svg>")` ← Task 2.2
10. `registerEditorExtension(new MyDecoProvider())` ← Task 2.4
11. `registerObsidianProtocolHandler("ks-action", [](QUrl){})` ← Task 3.2

Plus override `onExternalSettingsChange()` and write a test asserting it fires.

- [ ] **Step 1: Build CMake skeleton** following an existing plugin (e.g., `src/plugins/note-stats/CMakeLists.txt`) — declares the plugin via `corbomite_add_plugin`.
- [ ] **Step 2: Declare permissions in metadata.json.in**: the union of tokens needed.
- [ ] **Step 3: Implement plugin** — one method call per verb in `onload()`; cleanup is automatic.
- [ ] **Step 4: Test** — `tst_kitchen_sink.cpp` loads the plugin in a `PluginManager` instance, enables it, asserts each registry now contains the expected entry. Disables it, asserts each registry empty.
- [ ] **Step 5: Build, test, commit**: `tests: cluster-b kitchen-sink reference plugin`.

### Task 4.3 — Update audit-2026-04-26 follow-up notes + INDEX

**Files:**
- Modify: `docs/superpowers/plans/INDEX.md` (move Cluster B from "Active clusters" to "Closed in this scheme"; bump the closed-list)
- Modify: `docs/PROJECT-STATE.md` (active-cluster snapshot drops to 7)
- Modify: `docs/decisions-archive.md` (Cluster B closeout entry)
- Modify: `docs/punch-list.md` (mark items #71-#76 P3 done — `addStatusBarItem`, Lucide, Events mixin proxy bits if applicable; Settings opt-in for `obsidian://` is new follow-up)

- [ ] **Step 1: Closeout per Ritual 3** in `CONTRIBUTING-OPS.md` — write closeout summary.
- [ ] **Step 2: Update INDEX, PROJECT-STATE, punch-list, decisions-archive** in one commit.
- [ ] **Step 3: Commit**: `docs: close cluster B (plugin API surface completion)`.

---

## Self-review checklist (run before declaring done)

- [ ] Spec coverage — every of 16 spec items has at least one task. Verified by ticking spec table against task list.
- [ ] No placeholders — searched plan for "TBD"/"TODO"/"fill in"; only present where genuinely deferred and documented.
- [ ] Type consistency — registrar method names match between header/impl/test/facade.
- [ ] Build clean — `cmake --build build -j 10` exits 0.
- [ ] Tests green — `ctest -j 10` reports new tests pass + no pre-existing-pass tests turn red.
- [ ] Commits well-scoped — one commit per task; messages match `core+vault: <verb> plugin proxy` pattern.
- [ ] Markoff submodule pin advanced cleanly — Corbomite parent repo references the new submodule SHA.
