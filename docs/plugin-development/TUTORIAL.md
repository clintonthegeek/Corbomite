# Tutorial — the note-stats reference plugin

This walkthrough follows the reference plugin at
[`examples/note-stats-plugin/`](../../examples/note-stats-plugin/) end
to end. By the time you finish, you will have seen every part of the
plugin surface that a sidebar-view plugin touches: metadata, the
`Plugin` subclass, permission-gated proxies, a `QWidget` view,
reactive refresh via Qt signals, a smoke-test harness, and how the
host discovers and enables the result.

The note-stats plugin reads every markdown file in the vault and
shows four counters in the right sidebar: total notes, total words,
unique tags, and total links. Small, but it exercises four proxies
and two permissions-gated signal subscriptions — enough to illustrate
the full shape of a non-trivial plugin.

---

## 1. Project layout

```
examples/note-stats-plugin/
├── CMakeLists.txt
├── metadata.json.in
├── NoteStatsPlugin.h        // Plugin subclass declaration
├── NoteStatsPlugin.cpp      // Plugin subclass + K_PLUGIN_FACTORY
├── NoteStatsView.h          // QWidget declaration
├── NoteStatsView.cpp        // QWidget — refresh logic + signal hookup
├── README.md
└── tests/
    ├── CMakeLists.txt
    └── tst_note_stats_plugin.cpp
```

A typical third-party plugin mirrors this layout. The `metadata.json.in`
template gets configured into a real `metadata.json` next to the `.so`
at build time; the `Plugin` subclass is the entry point the host
instantiates; the `QWidget` subclass is mounted into the sidebar by the
host after the plugin's `createView()` returns it; and the smoke test
is an ordinary QTest executable.

---

## 2. CMakeLists.txt

```cmake
corbomite_add_plugin(note-stats
    METADATA_TEMPLATE "${CMAKE_CURRENT_SOURCE_DIR}/metadata.json.in"
    SOURCES
        NoteStatsPlugin.cpp
        NoteStatsView.cpp
    LINK_LIBRARIES
        Corbomite::Core
        Corbomite::Vault
        Corbomite::Storage
        KF6::I18n
        Qt6::Widgets
)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
```

`corbomite_add_plugin()` is Corbomite's convenience wrapper around
`add_library(MODULE)` + `configure_file(metadata.json.in metadata.json)`.
It accepts these arguments:

- `METADATA_TEMPLATE` — path to your `metadata.json.in`. Required.
  It gets `configure_file`d with `@ONLY` substitution to populate
  `@X_CORBOMITE_TRUSTED@` (see §3 below).
- `SOURCES` — your `.cpp` sources. Required.
- `LINK_LIBRARIES` — `target_link_libraries(PRIVATE ...)` forwarding.
  `KF6::CoreAddons` is always added automatically (needed for
  `KPluginFactory`).
- `INCLUDE_DIRECTORIES` — optional `target_include_directories(PRIVATE
  ...)` forwarding.
- `TRUSTED` — **do not use in a third-party plugin.** The flag exists
  for in-tree `src/plugins/*` CMakeLists and injects
  `X-Corbomite-Trusted: true` into metadata. `PluginManager` demotes
  any User-origin plugin's trusted claim to `false` regardless of
  what its metadata says, so setting this from a third-party plugin
  has no effect — but it is still discouraged as a matter of social
  convention (see [API-STABILITY.md](API-STABILITY.md)).

The helper also:

- Drops the built `.so` into the Corbomite dev-build plugin search
  path (`CORBOMITE_PLUGIN_DEV_DIR`) if defined, so launching
  `./build/Corbomite` from the Corbomite tree picks it up without
  an install step.
- Installs the `.so` alongside its `metadata.json` into
  `${KDE_INSTALL_PLUGINDIR}/corbomite` if that variable is defined
  (i.e. you are using `KDEInstallDirs`).

For a fully out-of-tree build, the top of your `CMakeLists.txt` needs
(as shown in the comment atop `examples/plugin-template/CMakeLists.txt`):

```cmake
cmake_minimum_required(VERSION 3.20)
project(corbomite-my-plugin VERSION 0.1.0 LANGUAGES CXX)
find_package(Corbomite REQUIRED)
find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets)
find_package(KF6 REQUIRED COMPONENTS CoreAddons I18n)
```

After `find_package(Corbomite REQUIRED)`, `corbomite_add_plugin()` is on
the module path.

---

## 3. metadata.json.in

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

The `KPlugin` object is standard `KPluginMetaData`. Pick a namespaced
`Id` (e.g. `yourname.thing`) — it becomes the KConfig group key and
the command-id prefix, so it must be unique per install.

The `X-Corbomite-*` keys are Corbomite-specific:

- **`X-Corbomite-Trusted`** — `true` suppresses the first-enable
  permission dialog. Only ever `true` for in-tree plugins; always
  `false` for third-party plugins (enforced by
  `PluginManager`-level origin demotion).
- **`X-Corbomite-Permissions`** — array of tokens. The full set and
  what each gates is tabulated in
  [API-REFERENCE.md](API-REFERENCE.md) §permission tokens. Declare
  only what you need; plugins that overreach waste user goodwill in
  the grant dialog.
- **`X-Corbomite-DockArea`** — `"left"` or `"right"`. If set,
  `MainWindow` mounts the `QObject` returned by `createView()` into
  that sidebar tool view. Omit to leave placement up to the plugin
  (e.g. if your plugin opens a modal dialog from a command instead).
- **`X-Corbomite-MinVersion`** — minimum Corbomite version your
  plugin was built against. `PluginManager` refuses to enable a
  plugin whose minimum exceeds the host's
  `QCoreApplication::applicationVersion()`. Set it to the Corbomite
  release you tested against.
- **`X-Corbomite-ApiLevel`** — integer ABI-break marker. Defaults
  to `1` when the key is absent. Corbomite only loads plugins
  declaring a level `<= CORBOMITE_PLUGIN_API_LEVEL` (currently `1`).
  Bump only when you need features that require the host's next
  API level. See [API-STABILITY.md](API-STABILITY.md).

---

## 4. The Plugin subclass

`NoteStatsPlugin.h`:

```cpp
#pragma once
#include "corbomite/vault/Plugin.h"

namespace Corbomite { class MainWindow; }

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

Three things to notice:

- The ctor signature matches what `KPluginFactory` hands you:
  `(QObject *parent, const QVariantList &args)`. This is not a
  Corbomite-specific convention; it is
  `K_PLUGIN_FACTORY_WITH_JSON`'s constraint.
- We inherit `Corbomite::Plugin` (from `corbomite/vault/Plugin.h`).
  `Plugin` multi-inherits `QObject` and Corbomite's `Component` lifecycle
  mixin. For most plugins you never care about `Component` directly;
  you override the virtuals documented in
  [API-REFERENCE.md](API-REFERENCE.md) §`Corbomite::Plugin`.
- We override just `createView()`. That is enough for a sidebar plugin —
  the default `onLoad` / `onUnload` / `focus` /
  `saveSessionState` / `loadSessionState` behaviour is fine.

`NoteStatsPlugin.cpp`:

```cpp
#include "NoteStatsPlugin.h"
#include "NoteStatsView.h"

#include "corbomite/storage/proxies/MetadataCacheReader.h"
#include "corbomite/storage/proxies/SearchProxy.h"
#include "corbomite/vault/PluginContext.h"
#include "corbomite/vault/proxies/VaultProxy.h"

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
        qWarning() << "note-stats: missing permissions; skipping view";
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

Several points to absorb:

- `context()` is inherited from `Corbomite::Plugin` and returns the
  `PluginContext *` captured at `load()` time. It is non-null between
  `onLoad()` and `onUnload()`. `createView()` always runs during that
  window (the host calls it after `load()` returns), so the
  `if (!ctx) return nullptr;` check is defensive but not load-bearing.
- `ctx->vault()`, `ctx->search()`, `ctx->metadataCache()` each return
  a proxy **or `nullptr`**. Nullptr means either:
  - the permission was not granted by the user (unlikely if the user
    clicked Allow on the grant dialog, but the user can also deny
    individual permissions if we ever ship that UI), or
  - the host did not wire the underlying service (e.g. a vault is not
    open — `ctx->vault()` is then `nullptr`).
- Returning `nullptr` from `createView()` is the right way to degrade
  when a proxy you need is missing. The host skips mounting the
  sidebar tool view; no crash, no half-wired widget.
- The factory macro embeds `metadata.json` (the configured one, next
  to your `.so`) into the plugin binary for discovery. The include
  at the bottom is the standard `Q_OBJECT`-in-`.cpp` moc dance.

### What if your plugin is headless?

Don't override `createView()`. Do your work in `onLoad(PluginContext
*)`: register commands via `ctx->commands()`, subscribe to signals via
`ctx->vault()`, etc. Undo everything in `onUnload()`. A plugin that
returns `nullptr` from `createView()` (the default) is loaded and live,
just not surfaced in a sidebar.

---

## 5. The view widget

`NoteStatsView.h`:

```cpp
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

private Q_SLOTS:
    void refresh();

private:
    Corbomite::VaultProxy          *m_vault;
    Corbomite::SearchProxy         *m_search;
    Corbomite::MetadataCacheReader *m_metadata;
    QLabel *m_noteCount;
    QLabel *m_wordCount;
    QLabel *m_tagCount;
    QLabel *m_linkCount;
};

} // namespace NoteStats
```

The ctor takes the three proxy pointers the plugin gave it. The
widget keeps them as plain pointers — their lifetime is owned by the
`PluginContext`, and `PluginContext` outlives every `QObject` the
plugin created during `createView()`. (The host tears down plugin-
owned views *before* it destroys the plugin's context.)

`NoteStatsView.cpp` — the full ctor + `refresh()`:

```cpp
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
    form->addRow(i18n("Notes"), m_noteCount);
    form->addRow(i18n("Words (approx.)"), m_wordCount);
    form->addRow(i18n("Unique tags"), m_tagCount);
    form->addRow(i18n("Total links"), m_linkCount);

    refresh();

    if (m_vault) {
        connect(m_vault, &Corbomite::VaultProxy::created,
                this, &NoteStatsView::refresh);
        connect(m_vault, &Corbomite::VaultProxy::modified,
                this, &NoteStatsView::refresh);
        connect(m_vault, &Corbomite::VaultProxy::deletedFile,
                this, &NoteStatsView::refresh);
    }
    if (m_metadata) {
        connect(m_metadata, &Corbomite::MetadataCacheReader::indexFinished,
                this, &NoteStatsView::refresh);
    }
}

void NoteStatsView::refresh()
{
    if (!m_vault || !m_search) return;

    const auto files = m_vault->getMarkdownFiles();
    m_noteCount->setText(QString::number(files.size()));

    int words = 0;
    for (auto *f : files) {
        const QByteArray body = m_vault->cachedRead(f);
        int w = 0;
        bool in = false;
        for (char c : body) {
            if (c == ' ' || c == '\n' || c == '\t' || c == '\r') {
                if (in) { ++w; in = false; }
            } else {
                in = true;
            }
        }
        if (in) ++w;
        words += w;
    }
    m_wordCount->setText(QString::number(words));

    m_tagCount->setText(QString::number(m_search->allTags().size()));
    m_linkCount->setText(QString::number(m_search->allLinks().size()));
}
```

Three things worth pointing out:

- **`VaultProxy` is a `QObject`.** It re-emits its underlying
  `Vault`'s `created` / `modified` / `deletedFile` / `renamed`
  signals as its own, gated on the `vault.events` permission. That's
  why `connect(m_vault, &VaultProxy::created, this, ...)` works —
  widgets can use ordinary Qt signal/slot machinery. If you only
  declared `vault.read` and omitted `vault.events`, the signals are
  defined on the proxy but never fire; your view builds once at
  ctor time and then stays stale.
- **`cachedRead`, not `read`.** Both are on `VaultProxy`, both gate
  on `vault.read`. `cachedRead` reuses the vault's in-memory buffer
  when available; `read` always hits disk. For a counter that re-
  runs on every modify, cache is the right choice. For a command
  that exports the vault to disk, `read` is.
- **`allTags()` + `allLinks()` are cheap.** They hit the SQLite FTS
  index maintained by Corbomite's metadata worker, not the files
  themselves. For a sidebar that refreshes on every vault signal,
  that matters.

---

## 6. Permissions deep-dive

The plugin declares four tokens. Here is what each one actually
enables:

- **`vault.read`** — unlocks `VaultProxy::read`, `cachedRead`,
  `readBinary`, `exists`, `getFileByPath`, `getFolderByPath`,
  `getAbstractFileByPath`, `getMarkdownFiles`, `getFiles`,
  `getRoot`, `getName`, `basePath`, `readConfigJson`. Without it,
  `ctx->vault()` is still non-null (since one of `vault.read` /
  `vault.write` / `vault.events` suffices), but read methods
  return empty / nullptr.
- **`vault.events`** — unlocks `VaultProxy`'s `created`, `modified`,
  `deletedFile`, `renamed` signals. Without it, the signals are
  defined but never fire.
- **`metadata.read`** — unlocks both `ctx->metadataCache()` (the
  parsed-frontmatter reader) and `ctx->search()` (the FTS / links /
  tags query surface). Without it, both accessors return `nullptr`.
- **`ui.views`** — unlocks `ctx->views()`, i.e. `ViewRegistrar`.
  Note-stats does not actually use `ctx->views()` — its view is
  mounted implicitly via `X-Corbomite-DockArea` + `createView()`.
  The permission is declared because mounting a sidebar view is
  reasonably covered by "the plugin wants to put UI in your
  workspace." A future refactor may tighten this.

### What happens when a permission is denied

If the user clicks Cancel on the grant dialog, the plugin is not
enabled at all. If the user grants a subset (not currently
possible through the default UI, but the permission system is
designed for it), the ungranted accessors on the context return
`nullptr`. A careful plugin — note-stats is one — checks for
`nullptr` in `createView()` and returns `nullptr` itself, so the
host skips mounting a broken view rather than showing a
half-wired widget.

The full permission table is in
[API-REFERENCE.md](API-REFERENCE.md) §permission tokens.

---

## 7. Testing

`tests/tst_note_stats_plugin.cpp`:

```cpp
void tst_note_stats_plugin::refreshReadsFromProxies()
{
    Corbomite::FileSystemAdapter adapter;
    Corbomite::Vault vault(&adapter);

    QTemporaryDir dir; QVERIFY(dir.isValid());
    QFile a(dir.filePath(QStringLiteral("a.md")));
    QVERIFY(a.open(QIODevice::WriteOnly));
    a.write("hello world"); a.close();
    QFile b(dir.filePath(QStringLiteral("b.md")));
    QVERIFY(b.open(QIODevice::WriteOnly));
    b.write("another note"); b.close();
    vault.load(dir.path());
    QVERIFY(vault.isLoaded());

    QSet<QString> granted = { QStringLiteral("vault.read"),
                              QStringLiteral("vault.events"),
                              QStringLiteral("metadata.read") };
    Corbomite::VaultProxy vaultProxy(&vault, granted, QStringLiteral("t"));

    Corbomite::SQLiteIndex index;
    QVERIFY(index.open(dir.filePath(QStringLiteral("idx.sqlite"))));
    Corbomite::SearchProxy searchProxy(&index, granted, QStringLiteral("t"));

    Corbomite::LinkResolver resolver;
    Corbomite::MetadataCache cache(resolver);
    Corbomite::MetadataCacheReader reader(&cache);

    NoteStats::NoteStatsView view(&vaultProxy, &searchProxy, &reader);
    QVERIFY(true);
}
```

The test constructs proxies manually instead of going through
`PluginManager`. This is deliberate: a unit test should not require
a running plugin-discovery pipeline. The proxy ctors are public,
take an explicit granted-permission set, and are designed to be
stood up with minimum ceremony. Your plugin's tests can do the same.

`tests/CMakeLists.txt` compiles the plugin's own `.cpp` sources
directly into the test target (rather than linking against the
`.so`), avoiding a `KPluginFactory`-discovery dance in the test:

```cmake
add_executable(tst_note_stats_plugin
    tst_note_stats_plugin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../NoteStatsPlugin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../NoteStatsView.cpp)
target_link_libraries(tst_note_stats_plugin PRIVATE
    Qt6::Test Qt6::Widgets
    Corbomite::Core Corbomite::Storage Corbomite::Vault
    KF6::I18n KF6::CoreAddons)
```

`QT_QPA_PLATFORM=offscreen` is set as a test environment variable
so the widget ctor works in headless CI.

---

## 8. Building

From a clean `git clone` of Corbomite:

```sh
cmake -B build -DCORBOMITE_DEV_BUILD=ON -DCORBOMITE_BUILD_EXAMPLES=ON
cmake --build build --target note-stats -j
```

For a third-party, out-of-tree plugin:

```sh
cd my-plugin
cmake -B build
cmake --build build -j
```

`cmake --build` produces `note-stats.so` (or `corbomite-template.so`,
etc.) alongside its configured `metadata.json`.

---

## 9. Installing for development

Two options during active development:

**Option A — in-tree dev build.** If you are extending Corbomite itself
and building the plugin as part of the Corbomite tree, passing
`-DCORBOMITE_DEV_BUILD=ON` makes `corbomite_add_plugin()` drop the
`.so` into Corbomite's `CORBOMITE_PLUGIN_DEV_DIR`, which the dev app
appends to its plugin search path at startup. Running
`./build/Corbomite` picks it up with no install step.

**Option B — user-local install.** From an out-of-tree build:

```sh
cmake --install build --prefix ~/.local
```

`KDEInstallDirs` puts the `.so` at
`~/.local/lib/qt6/plugins/corbomite/` (exact path varies by distro
layout). Corbomite also searches
`~/.local/share/qt6/plugins/corbomite/` as a fallback. Either path
works; the distro package-manager case is covered in
[DISTRIBUTION.md](DISTRIBUTION.md).

After install, launch Corbomite, open a vault, then Settings →
Plugins. Your plugin should appear in the list. Click Enable; approve
the permissions dialog on first enable. The plugin's sidebar view (if
any) appears immediately.

---

## 10. Next steps

- Read [API-REFERENCE.md](API-REFERENCE.md) for the full catalog of
  proxy methods, signals, and permissions.
- Copy [`examples/plugin-template/`](../../examples/plugin-template/)
  as the starting point for a fresh plugin — it is the minimum
  skeleton with one passing smoke test.
- If you are packaging for a distro, read
  [DISTRIBUTION.md](DISTRIBUTION.md).
- Before committing long-term to a plugin project, read
  [API-STABILITY.md](API-STABILITY.md) — it documents exactly what
  we promise not to break.
