# API reference

Every public surface a Corbomite plugin can call. Each section lists
the header, the permission gate, and the methods.

Every accessor returns `nullptr` / empty / `false` when the required
permission was not granted or the underlying service is not wired. A
well-behaved plugin checks and degrades gracefully.

Unless noted otherwise, all types are in namespace `Corbomite`.

---

## Table of contents

1. [`Corbomite::Plugin`](#corbomiteplugin) — lifecycle base class.
2. [`Corbomite::PluginContext`](#corbomiteplugincontext) — handed to
   `onLoad`; source of all proxies.
3. [`Corbomite::VaultProxy`](#corbomitevaultproxy) — file tree + I/O +
   events.
4. [`Corbomite::FileManagerProxy`](#corbomitefilemanagerproxy) —
   link-aware rename, frontmatter mutation, attachment placement.
5. [`Corbomite::MetadataCacheReader`](#corbomitemetadatacachereader) —
   parsed-frontmatter / link / tag cache reader.
6. [`Corbomite::SearchProxy`](#corbomitesearchproxy) — FTS + link +
   tag queries backed by SQLite.
7. [`Corbomite::WorkspaceController`](#corbomiteworkspacecontroller) —
   open / split / close leaves; drive active-file state.
8. [`Corbomite::CommandRegistrar`](#corbomitecommandregistrar) —
   register command-palette commands.
9. [`Corbomite::ViewRegistrar`](#corbomiteviewregistrar) — register
   view types + file-extension mappings.
10. [`Corbomite::MenuInjector`](#corbomitemenuinjector) — inject items
    into file / editor / tab context menus.
11. [`Corbomite::SecretStorage`](#corbomitesecretstorage) — persistent
    per-plugin secrets via QtKeychain.
12. [`Corbomite::ProcessSpawner`](#corbomiteprocessspawner) — spawn
    external processes with audit logging.
13. [`PluginContext::config()`](#plugincontextconfig) —
    `KConfigGroup` for KDE-native preferences.
14. [`PluginContext::loadData` / `saveData`](#plugincontextloaddata--savedata)
    — Obsidian-shape atomic JSON persistence at
    `.obsidian/plugins/<id>/data.json`.
15. [`PluginContext::network()`](#plugincontextnetwork) —
    `QNetworkAccessManager`.
16. [Permission tokens](#permission-tokens) — the 12 tokens and what
    each gates.
17. [`metadata.json` schema](#metadatajson-schema) — the `KPlugin` +
    `X-Corbomite-*` keys.

---

## `Corbomite::Plugin`

Header: `corbomite/vault/Plugin.h`.

Abstract base for all plugins. Subclass this, override what you need,
and register your subclass with `K_PLUGIN_FACTORY_WITH_JSON`. Inherits
`QObject` and Corbomite's internal `Component` lifecycle mixin.

| Method | Signature | Notes |
|---|---|---|
| ctor | `Plugin(QObject *parent = nullptr)` | Plumbing only; do not use for setup. Override `onLoad` instead. |
| `load` | `void load(PluginContext *ctx)` | Called by `PluginManager` at enable. Captures `ctx`, then drives `onLoad`. Idempotent. Not a virtual override point. |
| `context` | `PluginContext *context() const` | The context captured at `load()`. Non-null between `onLoad` and `onUnload`. |
| `onLoad` | `virtual void onLoad(PluginContext *ctx)` — protected | Fires once at enable. Override to register commands, subscribe to signals, etc. Default is a no-op. |
| `onUnload` | `virtual void onUnload()` — protected | Fires once at disable. Override to undo `onLoad`. Default is a no-op. Proxies owned by `PluginContext` are torn down automatically after this returns; you do not manually `delete` anything you got from `ctx->`. |
| `createView` | `virtual QObject *createView(MainWindow *mainWindow)` | Factory for the plugin's sidebar widget. Called once per MainWindow after `onLoad`. Return `nullptr` if the plugin has no view, or if a required permission is denied. Default returns `nullptr`. |
| `focus` | `virtual void focus(QObject *view)` | Called when the user presses the plugin's focus shortcut (e.g. Ctrl+Shift+F for Search). Default sets focus on `view` if it is a `QWidget`. Override for plugins whose view contains a non-root focus target (e.g. focusing a `QLineEdit` inside a tree panel). |
| `saveSessionState` | `virtual QJsonObject saveSessionState(QObject *view) const` | Serialize per-plugin state (tree expansion, sidebar scroll) across vault close/reopen. Host stores the result under `_corbomite.plugins.<pluginId>` in `workspace.json`. Default returns empty object. |
| `loadSessionState` | `virtual void loadSessionState(QObject *view, const QJsonObject &state)` | Apply previously-saved state. Host calls once after `createView` returns and before the view appears on screen. Default is no-op. |
| `configPages` | `virtual int configPages() const` | Number of KConfig settings pages this plugin provides. Default `0`. |
| `configPage` | `virtual KTextEditor::ConfigPage *configPage(int number, QWidget *parent)` | Factory for a specific KConfig page. Default returns `nullptr`. |

---

## `Corbomite::PluginContext`

Header: `corbomite/vault/PluginContext.h`.

Handed to `Plugin::onLoad()`; lifetime is the plugin's load span.
Owns permission-gated proxy objects. Each accessor returns `nullptr`
when either the corresponding permission is ungranted or the
underlying core service is null.

Permission-gated accessors (all const; proxies are lazy-constructed
and owned by the context):

| Accessor | Return type | Permission gate |
|---|---|---|
| `vault()` | `VaultProxy *` | `vault.read` or `vault.write` or `vault.events` |
| `fileManager()` | `FileManagerProxy *` | `vault.read` or `vault.write` or `metadata.read` |
| `metadataCache()` | `MetadataCacheReader *` | `metadata.read` |
| `search()` | `SearchProxy *` | `metadata.read` |
| `workspace()` | `WorkspaceController *` | `workspace` |
| `commands()` | `CommandRegistrar *` | `ui.commands` |
| `views()` | `ViewRegistrar *` | `ui.views` |
| `menus()` | `MenuInjector *` | `ui.menus` |
| `network()` | `QNetworkAccessManager *` | `network` |
| `secrets()` | `SecretStorage *` | `secrets` |
| `process()` | `ProcessSpawner *` | `process` |

Metadata / permission introspection:

| Method | Signature | Notes |
|---|---|---|
| `metaData` | `const PluginMetaData &metaData() const` | Parsed `metadata.json`. |
| `grantedPermissions` | `const QSet<QString> &grantedPermissions() const` | The tokens the user approved. Useful for plugins that want to branch on partial grants. |
| `hasPermission` | `bool hasPermission(const QString &token) const` | Convenience — sugar over `grantedPermissions().contains`. |

Persistence (see own sections below):

- `KConfigGroup config()` — `config` permission.
- `QJsonObject loadData() const` — `config` permission.
- `bool saveData(const QJsonObject &obj)` — `config` permission.

---

## `Corbomite::VaultProxy`

Header: `corbomite/vault/proxies/VaultProxy.h`. `QObject`.

Permission-gated facade over `Corbomite::Vault`. Methods return empty
/ `false` / `nullptr` / null `QUuid` when the caller lacks the
required token. Signals are defined unconditionally but only fire
when the owning plugin holds `vault.events` (checked once at ctor
time).

### Read (gated by `vault.read`)

| Method | Signature |
|---|---|
| `read` | `QByteArray read(TFile *f) const` |
| `cachedRead` | `QByteArray cachedRead(TFile *f) const` |
| `readBinary` | `QByteArray readBinary(TFile *f) const` |
| `exists` | `bool exists(const QString &path) const` |
| `getFileByPath` | `TFile *getFileByPath(const QString &path) const` |
| `getFolderByPath` | `TFolder *getFolderByPath(const QString &path) const` |
| `getAbstractFileByPath` | `TAbstractFile *getAbstractFileByPath(const QString &path) const` |
| `getMarkdownFiles` | `QVector<TFile *> getMarkdownFiles() const` |
| `getFiles` | `QVector<TFile *> getFiles() const` |
| `getRoot` | `TFolder *getRoot() const` |
| `getName` | `QString getName() const` |
| `basePath` | `QString basePath() const` — absolute filesystem path; empty string if no vault loaded |

`read` always hits disk; `cachedRead` reuses the vault's in-memory
buffer when available (preferable for hot-path reads). `readBinary`
returns the file contents without any text-codec handling.

### Mutation (gated by `vault.write`)

| Method | Signature |
|---|---|
| `modify` | `bool modify(TFile *f, const QByteArray &body)` |
| `modifyBinary` | `bool modifyBinary(TFile *f, const QByteArray &body)` |
| `append` | `bool append(TFile *f, const QByteArray &body)` |
| `process` | `bool process(TFile *f, std::function<QByteArray(const QByteArray &)> mutator)` — atomic read-modify-write |
| `create` | `TFile *create(const QString &path, const QByteArray &body)` |
| `createFolder` | `TFolder *createFolder(const QString &path)` |
| `rename` | `bool rename(TAbstractFile *f, const QString &newPath)` — no link rewrite; use `FileManagerProxy::renameFile` for link-aware rename |
| `trash` | `bool trash(TAbstractFile *f, bool useSystem)` — `useSystem=true` routes to the OS trash; `false` uses the vault-local trash |
| `remove` | `bool remove(TAbstractFile *f)` — hard delete; bypasses trash |

### Events — closure-based (gated by `vault.events`)

| Method | Signature |
|---|---|
| `on` | `QUuid on(const QString &event, std::function<void(TAbstractFile *)> fn)` — `event` is one of `"create"`, `"modify"`, `"delete"`, `"rename"`. Returns a null `QUuid` if the permission is missing or the event name is unknown. |
| `off` | `void off(const QUuid &token)` |

### Events — Qt signals (gated by `vault.events`)

| Signal | Signature |
|---|---|
| `created(TAbstractFile *f)` | Forwarded from `Vault::created`. |
| `modified(TFile *f)` | Forwarded from `Vault::modified`. |
| `deletedFile(TAbstractFile *f)` | Forwarded from `Vault::deletedFile`. The signal is `deletedFile` (not `deleted`) to avoid confusion with `QObject::destroyed`. |
| `renamed(TAbstractFile *f, const QString &oldPath)` | Forwarded from `Vault::renamed`. Second arg is the old relative path at the time of emission. |

### Config JSON (read gated by `vault.read`; write by `vault.write`)

| Method | Signature |
|---|---|
| `readConfigJson` | `QJsonValue readConfigJson(const QString &name) const` |
| `writeConfigJson` | `bool writeConfigJson(const QString &name, const QJsonValue &v)` |
| `deleteConfigJson` | `bool deleteConfigJson(const QString &name)` |

These read/write JSON files under `.obsidian/<name>.json` in the
active vault. For per-plugin persistent state, prefer
`PluginContext::saveData` / `loadData` (which namespaces by plugin id
automatically).

---

## `Corbomite::FileManagerProxy`

Header: `corbomite/vault/proxies/FileManagerProxy.h`.

Higher-level, link-aware operations. Not a `QObject`; no signals.

### Mutation (gated by `vault.write`)

| Method | Signature |
|---|---|
| `renameFile` | `bool renameFile(TAbstractFile *f, const QString &newPath)` — rewrites every backlink that pointed to `f`'s old path. |
| `processFrontMatter` | `bool processFrontMatter(TFile *f, FileManager::FrontMatterMutator mut)` — atomic frontmatter mutation. `FrontMatterMutator` is `std::function<void(QVariantMap &)>`. |
| `createNewMarkdownFile` | `TFile *createNewMarkdownFile(TFolder *parent, const QString &name, const QByteArray &content = {})` |
| `createNewFolder` | `TFolder *createNewFolder(TFolder *parent)` |
| `insertIntoFile` | `bool insertIntoFile(TFile *f, const QByteArray &content, FileManager::InsertMode mode)` — `InsertMode` is `Append` or `Prepend`. |
| `trashFile` | `bool trashFile(TAbstractFile *f)` |

### Query (gated by `vault.read`)

| Method | Signature |
|---|---|
| `getNewFileParent` | `TFolder *getNewFileParent(const QString &hintPath, const QString &filename = {}) const` |
| `getAvailablePathForAttachment` | `QString getAvailablePathForAttachment(const QString &linktext, const QString &sourcePathHint = {}) const` |

### Query (gated by `metadata.read` — reads `MetadataCache`)

| Method | Signature |
|---|---|
| `generateMarkdownLink` | `QString generateMarkdownLink(TFile *target, const QString &sourcePath, const QString &subpath = {}, const QString &displayText = {}) const` |

---

## `Corbomite::MetadataCacheReader`

Header: `corbomite/storage/proxies/MetadataCacheReader.h`. `QObject`.

Read-only facade over `MetadataCache`. Gated on `metadata.read`.

### Queries

| Method | Signature |
|---|---|
| `backlinksFor` | `QStringList backlinksFor(const QString &target) const` |
| `outlinksFor` | `QStringList outlinksFor(const QString &path) const` |
| `tagsIn` | `QStringList tagsIn(const QString &path) const` |
| `allTags` | `QStringList allTags() const` |
| `frontmatterFor` | `QJsonObject frontmatterFor(const QString &path) const` — empty if no frontmatter or no cache entry |

### Signals

| Signal | Signature | Fires when |
|---|---|---|
| `cacheChanged(const QString &path)` | | Cache entry for `path` is inserted or updated. |
| `cacheDeleted(const QString &path)` | | Cache entry for `path` is deleted. |
| `linksResolvedFor(const QString &path)` | | Link resolver has finished resolving `path`'s outlinks. |
| `allLinksResolved()` | | Link resolver drained its queue. |
| `indexFinished()` | | Debounced "index is settled" signal — use this to drive expensive refreshes. |

---

## `Corbomite::SearchProxy`

Header: `corbomite/storage/proxies/SearchProxy.h`.

Query facade over the SQLite FTS5 index. All methods gated on
`metadata.read`; return empty collections otherwise. Schema-shaped
operations (`createSchema`, direct SQL, writes) are intentionally
hidden — this is the stable surface.

### FTS (gated by `metadata.read`)

| Method | Signature |
|---|---|
| `search` | `QVector<SearchMatch> search(const QString &query, int maxResults = 100) const` — user-facing query string (Corbomite's search DSL). |
| `searchCompiled` | `QVector<SearchMatch> searchCompiled(const QString &fts5Query, const QStringList &requiredTags, const QStringList &excludedTags) const` — raw FTS5 MATCH expression plus tag filters. |

`SearchMatch` is defined in `corbomite/storage/SQLiteIndex.h`:

```cpp
struct SearchMatch {
    QString notePath;
    QString snippet;
    double score = 0.0;
    QVector<QPair<int, int>> matches; // UTF-16 [start, end) spans over `snippet`
};
```

### Links (gated by `metadata.read`)

| Method | Signature |
|---|---|
| `backlinksFor` | `QVector<LinkInfo> backlinksFor(const QString &targetPath) const` |
| `outlinksFor` | `QVector<LinkInfo> outlinksFor(const QString &sourcePath) const` |
| `allLinks` | `QVector<LinkInfo> allLinks() const` |

`LinkInfo` is defined in `corbomite/storage/SQLiteIndex.h`:

```cpp
struct LinkInfo {
    QString sourcePath;
    QString targetPath;
    QString linkType;       // "wiki", "markdown", "embed"
    QString displayText;    // alias, if any
    QString subpath;        // "#heading" or "#^block", empty if none
};
```

### Tags (gated by `metadata.read`)

| Method | Signature |
|---|---|
| `allTags` | `QStringList allTags() const` |
| `notesWithTag` | `QStringList notesWithTag(const QString &tag) const` |

---

## `Corbomite::WorkspaceController`

Header: `corbomite/core/proxies/WorkspaceController.h`. `QObject`.

Facade over `Workspace`. Gated on `workspace`. Leaves are addressed
by stable string ids; raw `WorkspaceLeaf *` is not exposed.

| Method | Signature |
|---|---|
| `openFile` | `bool openFile(const QString &relativePath)` — open or activate an existing leaf showing the file. Uses the workspace's `ViewRegistry` to pick a view type by extension. |
| `activeLeafId` | `QString activeLeafId() const` |
| `activeFilePath` | `QString activeFilePath() const` — vault-relative; empty if the active leaf hosts no file |
| `splitLeaf` | `bool splitLeaf(const QString &leafId, Qt::Orientation orientation)` |
| `closeLeaf` | `bool closeLeaf(const QString &leafId)` |
| `popoutLeaf` | `bool popoutLeaf(const QString &leafId)` — move leaf into a detached window |
| `goToLine` | `bool goToLine(int line)` — move the active leaf's cursor to the 1-based `line`. Returns `false` if the active view is not a text editor. |

### Signals

| Signal | Signature |
|---|---|
| `activeFileChanged(const QString &relativePath)` | Emitted when the active leaf changes. `relativePath` is the new file (vault-relative) or empty if the new active leaf is fileless. |

---

## `Corbomite::CommandRegistrar`

Header: `corbomite/core/proxies/CommandRegistrar.h`.

Gated on `ui.commands`. Auto-namespaces command ids as
`<pluginId>:<localId>` (Obsidian-compatible). Tracks every id it
registered and cleans up on destruction, so plugins do not need
per-command `removeCommand` calls on unload.

| Method | Signature |
|---|---|
| `addCommand` | `void addCommand(Command &cmd)` — mutates `cmd.id` in place to `<pluginId>:<cmd.id>`, then forwards to `CommandRegistry::addCommand`. Track the full id for cleanup. |
| `removeCommand` | `bool removeCommand(const QString &localId)` — `localId` is without the plugin prefix. Returns `true` if the command was registered by this registrar. |
| `pluginId` | `const QString &pluginId() const` |

`Corbomite::Command` (`corbomite/core/Command.h`) carries one of four
callback variants:

| Field | Type | Semantics |
|---|---|---|
| `id` | `QString` | Local id. Mutated to `pluginId:id` by `addCommand`. |
| `name` | `QString` | Human-readable label shown in the palette. |
| `icon` | `QString` | Optional `QIcon::fromTheme` key. |
| `mobileOnly` | `bool` | Obsidian-compat flag; ignored on desktop. |
| `callback` | `std::function<void()>` | Fires always, no availability check. |
| `checkCallback` | `std::function<bool(bool checking)>` | `checking=true` returns availability without side effects; `checking=false` executes if available. |
| `editorCallback` | `std::function<void(EditorLike)>` | Fires only if an active editor is present. |
| `editorCheckCallback` | `std::function<bool(bool, EditorLike)>` | `checkCallback` shape with editor context. |

Set exactly one callback variant. An empty `Command` registers but
cannot execute (preserved as an Obsidian-compat quirk).

---

## `Corbomite::ViewRegistrar`

Header: `corbomite/core/proxies/ViewRegistrar.h`.

Gated on `ui.views`. Tracks everything registered and un-registers it
on destruction.

| Method | Signature |
|---|---|
| `registerView` | `void registerView(const QString &type, ViewFactory factory)` — `ViewFactory` is `std::function<View *(WorkspaceLeaf *)>`. |
| `registerExtensions` | `void registerExtensions(const QStringList &extensions, const QString &type)` — map file extensions (without the leading dot) to a registered view type. |
| `unregisterView` | `void unregisterView(const QString &type)` |

`Corbomite::View` (`corbomite/core/View.h`) is the base class for
anything opened into a workspace leaf. For a sidebar plugin that does
not add its own file type, you typically use `createView()` + the
`X-Corbomite-DockArea` metadata key and never touch
`ViewRegistrar` — that path is for plugins adding new file types or
custom content views.

---

## `Corbomite::MenuInjector`

Header: `corbomite/core/proxies/MenuInjector.h`.

Gated on `ui.menus`. Disconnects all subscriptions on destruction.

| Method | Signature |
|---|---|
| `onFileMenuBuilt` | `void onFileMenuBuilt(Handler handler)` — fires when a file context menu is built. `context` arg is the file path. |
| `onEditorMenuBuilt` | `void onEditorMenuBuilt(Handler handler)` — fires when an editor context menu is built. `context` arg is the editor object's address formatted as hex (opaque id — raw `QObject *` is not exposed). |
| `onTabMenuBuilt` | `void onTabMenuBuilt(Handler handler)` — fires when a leaf/tab context menu is built. `context` arg is the leaf id as hex. |

`Handler` is `std::function<void(QMenu *menu, const QString &context)>`.
Add your `QAction`s to the supplied `QMenu` inside the handler; the
host owns the menu's lifetime and shows it immediately after all
handlers run.

---

## `Corbomite::SecretStorage`

Header: `corbomite/core/proxies/SecretStorage.h`.

Gated on `secrets`. Backed by QtKeychain (KWallet / GNOME Keyring /
macOS Keychain / Windows Credential Manager) when compiled with
`CORBOMITE_HAVE_KEYRING`; falls back to an in-process `QHash` (with
a `qCWarning` on first fallback) when QtKeychain is unavailable at
build time or the keyring service is unreachable at runtime. Keys
are namespaced as `<pluginId>.<id>` in all backends so plugins
cannot collide.

| Method | Signature |
|---|---|
| `setSecret` | `bool setSecret(const QString &id, const QString &value)` — `id` is the plugin-local key (no prefix needed). |
| `getSecret` | `QString getSecret(const QString &id) const` |
| `deleteSecret` | `bool deleteSecret(const QString &id)` |
| `listSecrets` | `QStringList listSecrets() const` — sorted local ids, without the plugin prefix. QtKeychain does not expose enumeration, so this only reflects ids observed via the in-process fallback or written during the current session; it is most useful for test introspection. |
| `pluginId` | `const QString &pluginId() const` |

---

## `Corbomite::ProcessSpawner`

Header: `corbomite/core/proxies/ProcessSpawner.h`.

Gated on `process`. Every invocation logs under
`corbomite.process-spawner` with the owning plugin id for auditability.

| Method | Signature |
|---|---|
| `start` | `QProcess *start(const QString &program, const QStringList &args = {}, QObject *parent = nullptr)` — started via `QProcess::start()`. Caller owns the lifetime (or re-parents via `parent`). |
| `startDetached` | `bool startDetached(const QString &program, const QStringList &args = {})` |
| `pluginId` | `const QString &pluginId() const` |

---

## `PluginContext::config()`

```cpp
KConfigGroup PluginContext::config();
```

Gated on `config`. Returns an empty group if the permission is
ungranted. Per-plugin KConfig group under Corbomite's global
configuration. Use this for KDE-native preferences (anything you would
otherwise surface through `KTextEditor::ConfigPage`).

For vault-scoped persistent state, prefer `saveData` / `loadData`
below — they travel with the vault.

---

## `PluginContext::loadData` / `saveData`

```cpp
QJsonObject PluginContext::loadData() const;
bool        PluginContext::saveData(const QJsonObject &obj);
```

Gated on `config`. Persists at
`<vault>/.obsidian/plugins/<plugin-id>/data.json` via atomic
`QSaveFile` I/O. Shape-compatible with Obsidian's
`await this.loadData()` / `await this.saveData(obj)` convention.

`loadData` returns an empty `QJsonObject` when the plugin has not
saved yet or the host has not wired a plugin-data dir (e.g. in a
test harness). `saveData` returns `false` on I/O failure.

State stored here travels with the vault — a vault copied to a USB
stick carries its plugin data with it. State stored via `config()`
lives in `~/.config/corbomiterc` and is machine-local.

---

## `PluginContext::network()`

```cpp
QNetworkAccessManager *PluginContext::network() const;
```

Gated on `network`. Shared host `QNetworkAccessManager`. Useful for
plugins that call out to HTTP APIs (Readwise importer, etc.). Returns
`nullptr` when the permission is ungranted.

---

## Permission tokens

The complete set of 12 tokens. Declared in
`X-Corbomite-Permissions` in `metadata.json`; checked against the
user-granted set at each proxy accessor or method call.

| Token | Gates |
|---|---|
| `vault.read` | `VaultProxy` read methods (`read`, `cachedRead`, `readBinary`, `exists`, `get*ByPath`, `getMarkdownFiles`, `getFiles`, `getRoot`, `getName`, `basePath`, `readConfigJson`); `FileManagerProxy` query methods; contributes to `PluginContext::vault()` / `fileManager()` non-null returns. |
| `vault.write` | `VaultProxy` mutation methods (`modify`, `modifyBinary`, `append`, `process`, `create`, `createFolder`, `rename`, `trash`, `remove`, `writeConfigJson`, `deleteConfigJson`); `FileManagerProxy` mutation methods. |
| `vault.events` | `VaultProxy` signals (`created`, `modified`, `deletedFile`, `renamed`) and closure-based `on` / `off`. |
| `metadata.read` | `PluginContext::metadataCache()` and `search()` non-null; `MetadataCacheReader` methods + signals; `SearchProxy` methods; `FileManagerProxy::generateMarkdownLink`. |
| `workspace` | `PluginContext::workspace()` non-null; `WorkspaceController` methods + signal. |
| `ui.commands` | `PluginContext::commands()` non-null; `CommandRegistrar`. |
| `ui.views` | `PluginContext::views()` non-null; `ViewRegistrar` (main-area view-type registration, e.g. `graph`). **Not required for sidebar `createView()`** — the host's `X-Corbomite-DockArea` mount path does not touch `ViewRegistrar`, so a plugin that only ships a dock view may omit `ui.views` entirely. A plugin that calls `registerView()` for a main-area type must declare it. |
| `ui.menus` | `PluginContext::menus()` non-null; `MenuInjector`. |
| `network` | `PluginContext::network()` non-null. |
| `secrets` | `PluginContext::secrets()` non-null; `SecretStorage` methods. |
| `process` | `PluginContext::process()` non-null; `ProcessSpawner` methods. |
| `config` | `PluginContext::config()` returns a populated group; `loadData` / `saveData` succeed. |

Permission semantics are **declaration-of-intent, not enforcement**.
A plugin running native C++ in-process can call any library it
wants; the permission system communicates to the user what the
plugin says it needs, and the host declines to hand out proxies for
tokens the user did not approve.

---

## `metadata.json` schema

Embedded into the plugin `.so` via `K_PLUGIN_FACTORY_WITH_JSON`.
Configure-file-substituted from `metadata.json.in` so the
`corbomite_add_plugin()` helper can inject `X-Corbomite-Trusted`.

### `KPlugin` object (standard `KPluginMetaData`)

| Key | Type | Notes |
|---|---|---|
| `Id` | string | Namespaced unique id (e.g. `yourname.thing`). Becomes the KConfig group key and the command-id prefix. |
| `Name` | string | Human-readable name (shown in `PluginsPage` and the grant dialog). |
| `Description` | string | One-sentence description. |
| `Version` | string | Your plugin's version. |
| `Authors` | array of `{Name, Email}` | |
| `License` | string | SPDX identifier. |

### `X-Corbomite-*` keys (Corbomite-specific)

| Key | Type | Notes |
|---|---|---|
| `X-Corbomite-Trusted` | bool | `true` suppresses the first-enable permission dialog. Injected by `corbomite_add_plugin(... TRUSTED)` into in-tree plugins only. `PluginManager` demotes any User-origin plugin's trusted claim to `false`. |
| `X-Corbomite-Permissions` | array of string | Declared capability tokens. Absent = empty = no proxies. See the permission table above. |
| `X-Corbomite-DockArea` | string | `"left"` or `"right"`. If set, `MainWindow` mounts the `QObject` returned by `createView()` into that sidebar tool view. Omit for plugins that do not add a sidebar view. |
| `X-Corbomite-MinVersion` | string | Minimum Corbomite version (semver). `PluginManager` refuses to enable plugins whose minimum exceeds `QCoreApplication::applicationVersion()`, surfacing "Requires Corbomite >= X" in `PluginsPage`. |
| `X-Corbomite-ApiLevel` | integer | ABI-break marker. Defaults to `1` when absent. Host accepts plugins declaring a level `<= CORBOMITE_PLUGIN_API_LEVEL`. Plugins declaring a higher level refuse to load with "Requires plugin API level >= N" in `PluginsPage`. |
