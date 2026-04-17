# Vault Architecture — Design Spec

**Date:** 2026-04-16
**Status:** Design approved; awaiting implementation plan (writing-plans).
**Cluster designation:** Cluster Q.0 (Vault foundation) — prerequisite for the
existing Cluster Q Tasks 7–12.
**Supersedes:** the path-only `Corbomite::Vault` stub introduced in Cluster Q
Task 7 (commit `b9a271d`), the `VaultReader` / `VaultWriter` pair defined in
`libs/core/include/corbomite/core/proxies/`, and the `Corbomite::VaultModel`
class in `libs/models/`.

---

## 1. Problem statement

Corbomite currently has three overlapping "vault"-shaped entities, at three
library layers:

1. **`Corbomite::Vault`** — `libs/core/`, 96 LOC, introduced in Cluster Q
   Task 7 (commit `b9a271d`). Path-only wrapper directly over `QFile` /
   `QSaveFile`. No signals, no caching, no awareness of the rest of the system.
2. **`Corbomite::VaultModel`** — `libs/models/`, 213 LOC. The actual in-memory
   vault — holds the `QHash<QString, NoteMeta>` note map, the `NoteDocument`
   cache, emits `noteAdded` / `noteRemoved` / `noteModified` / `noteRenamed`
   signals. Uses `FileSystemAdapter` + `VaultScanner`. Lives in `libs/models/`
   which is downstream of `libs/core/`.
3. **`Corbomite::VaultService`** — `src/app/`, 86 LOC. App-layer coordinator.
   Owns `VaultModel` + `NoteService`. Handles `openVault` / `closeVault` +
   recent-vaults via `KSharedConfig`.

Plus orbital file-I/O-shaped classes: `VaultScanner`, `VaultConfig`,
`VaultTrash`, `VaultProcess`, `FileSystemAdapter` / `DataAdapter`,
`FileWatchReactor`, `NoteService`, `MetadataCache`, `SQLiteIndex`,
`FrontMatterWriter`.

The split happened as a library-boundary artifact, not by design. The
Cluster Q Task 7 workaround synthesized a path-only `Vault` in `libs/core/`
because the plan assumed one existed and the "real" vault (`VaultModel`)
wasn't reachable from `libs/core/` without inverting the dependency graph.
The consequence: **plugin writes via `VaultWriter::{create,write,rename,
remove}` bypass `VaultModel`** — no `noteAdded` / `noteRemoved` /
`noteModified` / `noteRenamed` signals, no `NoteDocument` cache
invalidation, no `FileWatchReactor` awareness, no `MetadataCache`
reconciliation. Reads are safe; mutations are a latent consistency bug.

Obsidian's `App.vault` is a single canonical class: the in-memory
`TFile` / `TFolder` tree + event emitter + mutation API + adapter owner.
Our split has no analog there. This spec collapses the three-way Corbomite
split into one Obsidian-shaped aggregate, relocates it to a new dedicated
library, and rewires the plugin proxy layer to match.

---

## 2. Design principles

1. **Shape-parity with `Obsidian.App.vault` + `App.fileManager`.** One `Vault`
   class (file tree + CRUD + events + config I/O) + one `FileManager` class
   (link-aware rename, frontmatter mutation, attachment placement,
   new-file-with-collision-free-naming). Methods named to match Obsidian's
   API verbatim where possible; plugin authors porting from Obsidian docs
   change `.` to `->` and `await` goes away.
2. **Sync API.** All Vault / FileManager methods return `bool` / value types,
   not `QFuture<T>`. Obsidian's Promise return types are a JS-async idiom we
   would import only at the cost of distorting our otherwise-sync Qt code.
3. **Single vault per process.** Matches Obsidian. Second vault requires a
   second Corbomite instance. `Vault` is owned by `CorbomiteApp`.
4. **One-place-for-mutation.** Every file write (UI, plugin, FileManager)
   routes through `Vault::{modify,create,rename,remove,process,...}`.
   MetadataCache + SQLiteIndex + sidebars + graph + editor all subscribe to
   Vault signals — no bypasses.
5. **Permission enforcement at method level, not accessor level.** Plugin
   proxies (`VaultProxy`, `FileManagerProxy`) are single objects per plugin;
   individual methods check permissions internally. Matches Obsidian shape.
6. **Skip Obsidian's Electron/mobile cruft.** No `CapacitorAdapter`, no
   `app://local/...?<mtime>` URL scheme (we have `VaultResourceProvider`),
   no `WeakMap<TFile, string>` 64KB cache (we have `NoteDocument`), no
   `killLastAction` / 60-s Electron watchdog, no `raw` event firehose
   (typed Qt signals subsume it).

---

## 3. Library layout & dep graph

```
libs/core        (primitives: Component, Events, Plugin, Hotkey, Command,
                  ViewRegistry, Workspace*, ...)                       no change
libs/storage  →  core                                                  no change
libs/vault    →  core + storage                                        NEW
libs/models   →  vault + core + storage
libs/markoff, libs/readingview, libs/canvas, libs/forcegraph ─→ vault
src/app       →  vault + everything above
```

Contents of `libs/vault/`:

```
include/corbomite/vault/
  TAbstractFile.h
  TFile.h
  TFolder.h
  Vault.h
  FileManager.h
  FileWatcher.h              (folded from src/reactors/FileWatchReactor)
  VaultScanner.h             (moved from libs/storage/)
  proxies/
    VaultProxy.h
    FileManagerProxy.h
src/
  TAbstractFile.cpp / TFile.cpp / TFolder.cpp
  Vault.cpp
  FileManager.cpp
  FileWatcher.cpp
  VaultScanner.cpp
  proxies/VaultProxy.cpp / FileManagerProxy.cpp
tests/
  tst_vault_tree.cpp / tst_vault_crud.cpp / tst_vault_watcher.cpp
  tst_vault_config.cpp / tst_vault_lifecycle.cpp
  tst_file_manager_rename.cpp / tst_file_manager_frontmatter.cpp
  tst_file_manager_newfile.cpp / tst_file_manager_attachments.cpp
  tst_file_manager_generate_link.cpp
  tst_vault_proxy.cpp / tst_file_manager_proxy.cpp
CMakeLists.txt
CLAUDE.md
```

---

## 4. Core types: `TAbstractFile` / `TFile` / `TFolder`

Value-bearing handle types. Non-QObject (cheap to allocate). Vault owns the
backing storage; consumers hold non-owning raw pointers.

```cpp
namespace Corbomite {

class Vault;
class TFolder;

class TAbstractFile {
public:
    QString  path;                // NFC-normalized, /-separated, root-relative
    QString  name;                // basename(path)
    TFolder *parent = nullptr;    // non-owning; Vault owns the tree
    bool     deleted = false;     // tombstone — set true on removal, never reset

    Vault *vault() const { return m_vault; }
    void setPath(const QString &newPath);
    QString getNewPathAfterRename(const QString &newName) const;

protected:
    TAbstractFile(Vault *v, QString p);
    virtual ~TAbstractFile() = default;

private:
    Vault *m_vault;
};

struct FileStat {
    qint64 sizeBytes = 0;
    qint64 mtimeMs = 0;
    qint64 ctimeMs = 0;
};

class TFile : public TAbstractFile {
public:
    QString                  basename;        // name without extension
    QString                  extension;       // lowercase, no dot
    std::optional<FileStat>  stat;            // nullopt until first reconcile
    bool                     saving = false;  // set during in-flight mutations

    QString getShortName() const;             // basename for .md, else name
};

class TFolder : public TAbstractFile {
public:
    QList<TAbstractFile *> children;          // non-owning; Vault owns entries
    bool    isRoot() const { return path == QStringLiteral("/"); }
    QString getParentPrefix() const;          // "" for root else path+"/"
    int     getFileCount() const;             // recursive
    int     getFolderCount() const;           // recursive
};

} // namespace Corbomite
```

**Ownership model.** `Vault` holds `QHash<QString, std::unique_ptr
<TAbstractFile>>` keyed by normalized path. `TFolder::children` +
`TAbstractFile::parent` are raw pointers into that map's values.

**Tombstone-on-delete.** Deletion sets `deleted = true` then defers the
`unique_ptr` destruction by one event-loop turn (e.g., via `deleteLater`-
shaped queue or a `QVector<std::unique_ptr<...>> m_pendingDeletion` drained
at end-of-mutation). Subscribers holding `TFile *` and receiving the
`deletedFile` signal can check `deleted == true` and react without a
use-after-free.

**NFC normalization.** Every path crossing Vault's surface passes through
`NFC`-normalization + `/`-normalization + collapse-consecutive-slashes +
trim-trailing-slash. Mirrors Obsidian's `normalizePath`.

---

## 5. `Vault` class API

```cpp
class Vault : public QObject, public Events
{
    Q_OBJECT
public:
    explicit Vault(DataAdapter *adapter, QObject *parent = nullptr);
    ~Vault() override;

    // ---- Lifecycle ----
    void    load(const QString &basePath);   // scan + watch + fire 'loaded'
    void    unload();                         // stop watch + fire 'closed'
    bool    isLoaded() const;
    QString getName() const;                  // basename of basePath
    QString basePath() const;

    QString configDir() const;                // default ".obsidian"
    void    setConfigDir(const QString &d);   // validated — must start with '.'

    // ---- Tree queries ----
    TFolder        *getRoot() const;
    TAbstractFile  *getAbstractFileByPath(const QString &path) const;
    TAbstractFile  *getAbstractFileByPathInsensitive(const QString &path) const;
    TFile          *getFileByPath(const QString &path) const;
    TFolder        *getFolderByPath(const QString &path) const;
    QVector<TFile *>         getMarkdownFiles() const;
    QVector<TFile *>         getFiles() const;
    QVector<TFolder *>       getAllFolders(bool includeRoot = false) const;
    QVector<TAbstractFile *> getAllLoadedFiles() const;
    bool    exists(const QString &path, bool caseInsensitive = false) const;
    bool    isEmpty() const;

    // ---- Read ----
    QByteArray read(TFile *f) const;
    QByteArray cachedRead(TFile *f);
    QByteArray readBinary(TFile *f) const;
    QByteArray readRaw(const QString &path) const;

    // ---- Write (all sync, atomic via DataAdapter) ----
    bool     modify(TFile *f, const QByteArray &body, const WriteHints &h = {});
    bool     modifyBinary(TFile *f, const QByteArray &body, const WriteHints &h = {});
    bool     append(TFile *f, const QByteArray &body);
    bool     process(TFile *f,
                     std::function<QByteArray(const QByteArray &)> mutator);
    TFile   *create(const QString &path, const QByteArray &body);
    TFile   *createBinary(const QString &path, const QByteArray &body);
    TFolder *createFolder(const QString &path);
    bool     rename(TAbstractFile *f, const QString &newPath);  // no link rewrite
    bool     trash(TAbstractFile *f, bool useSystem);
    bool     remove(TAbstractFile *f, bool recursive = false);
    bool     copy(TAbstractFile *f, const QString &newPath);

    // ---- Path helpers ----
    QString getAvailablePath(const QString &pathNoExt, const QString &ext) const;

    // ---- Config-dir I/O (.obsidian/<name>.json) ----
    QJsonValue readConfigJson(const QString &name) const;
    bool       writeConfigJson(const QString &name, const QJsonValue &value);
    bool       deleteConfigJson(const QString &name);

signals:
    // Qt-native signals. `Events` mixin also exposes these via on(name, fn).
    void created(Corbomite::TAbstractFile *f);
    void modified(Corbomite::TFile *f);
    void deletedFile(Corbomite::TAbstractFile *f);
                              // "deleted" collides with TAbstractFile::deleted
    void renamed(Corbomite::TAbstractFile *f, const QString &oldPath);
    void closed();
};
```

**Threading.** All methods are main-thread only. The `FileWatcher` runs on
a helper thread but marshalls all reconciliation events back to the main
thread via `Qt::QueuedConnection` before touching `m_fileMap` or firing
signals.

**Echo suppression.** `DataAdapter::WriteHints::mtimeMs` stamps outbound
writes; `FileWatcher::reconcile` checks the stamp against the queued
watcher event and no-ops on match. Same mechanism Cluster B shipped.

**`cachedRead` cache.** Lives as `QHash<QString, QByteArray> m_readCache`
owned by Vault (keyed by normalized path). Sparse — only populated on
`cachedRead` call. Invalidated on `modified` / `deletedFile` / `renamed`.
Does NOT use Obsidian's 64KB-cap `WeakMap<TFile, string>` (a JS-GC
workaround); does NOT store on TFile directly (every TFile would pay for
the unused field). The existing `NoteDocument` cache in `VaultModel`
migrates to this mechanism; `NoteDocument` itself (the parsed-markdown
host for editor use) stays in `libs/core/` and is orthogonal.

**`Events` mixin.** Vault multi-inherits `QObject` + `Corbomite::Events`
(Cluster C primitive). `Events::trigger("modified", f)` + `on("modified",
fn)` give plugin authors the Obsidian-shape subscription API; Qt signals
give internal consumers the Qt-native shape. Both paths fire on the same
mutation.

---

## 6. `FileManager` class API

```cpp
class FileManager : public QObject
{
    Q_OBJECT
public:
    FileManager(Vault *vault, MetadataCache *cache, QObject *parent = nullptr);

    // ---- Rename with link rewrite ----
    // Snapshots backlinks via MetadataCache::iterateAllRefs before
    // vault->rename; after rename, walks the snapshot and rewrites [[old]]
    // → [[new]] (and markdown-link equivalents) in every source file via
    // vault->process. Serialised through a single-consumer update queue
    // (Obsidian's runAsyncLinkUpdate semantics).
    bool renameFile(TAbstractFile *f, const QString &newPath);

    // ---- Atomic frontmatter mutation (subsumes FrontMatterWriter) ----
    using FrontMatterMutator = std::function<void(QVariantMap &)>;
    bool processFrontMatter(TFile *f, FrontMatterMutator mut);

    // ---- Bulk frontmatter ops ----
    bool deleteProperty(const QString &key);         // deferred — phase 5 stub
    bool renameProperty(const QString &oldK,
                        const QString &newK);         // deferred — phase 5 stub

    // ---- New-file placement (honours newFileLocation setting) ----
    TFolder *getNewFileParent(const QString &hintPath,
                              const QString &filename = {}) const;
    TFile   *createNewMarkdownFile(TFolder *parent,
                                   const QString &name,
                                   const QByteArray &content = {});
    TFile   *createNewMarkdownFileFromLinktext(const QString &linkText,
                                                const QString &hintPath);
    TFolder *createNewFolder(TFolder *parent);

    // ---- Attachment placement (honours attachmentFolderPath setting) ----
    QString getAvailablePathForAttachment(const QString &linktext,
                                          const QString &sourcePathHint = {}) const;

    // ---- Content merge ----
    enum class InsertMode { Append, Prepend };
    bool insertIntoFile(TFile *f,
                        const QByteArray &content,
                        InsertMode mode);             // frontmatter merge deferred

    // ---- Link generation ----
    QString generateMarkdownLink(TFile *target,
                                 const QString &sourcePath,
                                 const QString &subpath = {},
                                 const QString &displayText = {}) const;

    // ---- Trash router (branches on trashOption setting) ----
    bool trashFile(TAbstractFile *f);

signals:
    void renameStarted(Corbomite::TAbstractFile *f, const QString &newPath);
    void renameFinished(Corbomite::TAbstractFile *f, const QString &oldPath);
    void linkUpdateProgress(int done, int total);
};
```

**Rename-with-link-rewrite contract.** `renameFile` wraps:

1. Emit `renameStarted`.
2. Call `cache->iterateAllRefs()` → snapshot of every `{sourceFile,
   reference, resolvedFile, resolvedPaths[]}`.
3. Call `vault->rename(f, newPath)`. Vault fires `renamed(f, oldPath)`.
4. For each ref in the snapshot whose `resolvedFile == f`: `vault->process(
   sourceFile, rewrite-body-lambda)`. Emit `linkUpdateProgress(i, total)`.
5. Emit `renameFinished(f, oldPath)`.

Concurrent `renameFile` calls are serialised through an internal `QQueue`
(`runAsyncLinkUpdate`). Nested calls during an in-flight rename queue.

**Modal UI** (`promptForDeletion`, `promptForFileRename`,
`promptForImageDownload`) stays in `src/app/` as thin adapters that call
`FileManager::trashFile` / `FileManager::renameFile` + `QMessageBox` /
`QInputDialog`. Out of scope for this spec.

---

## 7. Plugin proxy layer

Replaces `libs/core/include/corbomite/core/proxies/VaultReader.h` +
`VaultWriter.h`. New home: `libs/vault/include/corbomite/vault/proxies/`.
`libs/core/` loses all `Vault*`-referring proxy code.

### 7.1 Permission tokens

| Token | Gates |
|---|---|
| `vault.read` | `Vault` read methods, tree queries, `readConfigJson`, `FileManagerProxy` query methods |
| `vault.write` | `Vault` mutation methods, `writeConfigJson`, `deleteConfigJson`, all `FileManagerProxy` mutation methods |
| `vault.events` | `Vault::on(...)` / Qt-signal subscription proxy (new token) |
| `metadata.read` | existing; `FileManagerProxy::generateMarkdownLink` uses host-side `MetadataCache` so plugins need this for that one method |

### 7.2 `VaultProxy`

```cpp
class VaultProxy
{
public:
    VaultProxy(Vault *vault,
               const QSet<QString> &granted,
               QString pluginId);

    // Read (gated by vault.read; returns empty/nullptr if ungranted)
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

    // Mutation (gated by vault.write; returns false/nullptr if ungranted)
    bool     modify(TFile *f, const QByteArray &body);
    bool     modifyBinary(TFile *f, const QByteArray &body);
    bool     append(TFile *f, const QByteArray &body);
    bool     process(TFile *f,
                     std::function<QByteArray(const QByteArray &)> mutator);
    TFile   *create(const QString &path, const QByteArray &body);
    TFolder *createFolder(const QString &path);
    bool     rename(TAbstractFile *f, const QString &newPath);  // NO link rewrite
    bool     trash(TAbstractFile *f, bool useSystem);
    bool     remove(TAbstractFile *f);

    // Events (gated by vault.events)
    using EventFn = std::function<void(TAbstractFile *)>;
    QUuid on(const QString &event, EventFn fn);  // create / modify / delete / rename / closed
    void  off(const QUuid &token);

    // Config JSON (read gated by vault.read; write by vault.write)
    QJsonValue readConfigJson(const QString &name) const;
    bool       writeConfigJson(const QString &name, const QJsonValue &v);
    bool       deleteConfigJson(const QString &name);
};
```

### 7.3 `FileManagerProxy`

```cpp
class FileManagerProxy
{
public:
    FileManagerProxy(FileManager *fm,
                     const QSet<QString> &granted,
                     QString pluginId);

    // Mutation (gated by vault.write)
    bool     renameFile(TAbstractFile *f,
                        const QString &newPath);  // WITH link rewrite
    bool     processFrontMatter(TFile *f,
                                FileManager::FrontMatterMutator mut);
    TFile   *createNewMarkdownFile(TFolder *parent,
                                   const QString &name,
                                   const QByteArray &content = {});
    TFolder *createNewFolder(TFolder *parent);
    bool     insertIntoFile(TFile *f,
                            const QByteArray &content,
                            FileManager::InsertMode mode);
    bool     trashFile(TAbstractFile *f);

    // Query (gated by vault.read)
    TFolder *getNewFileParent(const QString &hintPath,
                              const QString &filename = {}) const;
    QString  getAvailablePathForAttachment(const QString &linktext,
                                           const QString &sourcePathHint = {}) const;

    // Query (gated by metadata.read — uses host-side MetadataCache)
    QString  generateMarkdownLink(TFile *target,
                                  const QString &sourcePath,
                                  const QString &subpath = {},
                                  const QString &displayText = {}) const;
};
```

### 7.4 Permission-failure policy

- Returning method: empty value (`{}` / `QByteArray{}` / `nullptr` / `false`).
- Logging: `qCWarning(lcPluginVault) << "plugin" << pluginId << "denied" <<
  methodName << "— missing" << requiredToken;` deduplicated per
  `(plugin, method)` pair per session to avoid log spam.

### 7.5 `PluginContext` rewiring

```cpp
// BEFORE (current Cluster Q, superseded)
void setCoreServices(Vault *v, MetadataCache *m, Workspace *w,
                     CommandRegistry *c, ViewRegistry *vr,
                     MenuEventEmitter *me,
                     QNetworkAccessManager *n);
VaultReader *vaultReader() const;
VaultWriter *vaultWriter() const;

// AFTER (this spec)
void setCoreServices(Vault *v, FileManager *fm,   // ← FileManager added
                     MetadataCache *m, Workspace *w,
                     CommandRegistry *c, ViewRegistry *vr,
                     MenuEventEmitter *me,
                     QNetworkAccessManager *n);
VaultProxy       *vault();         // ← replaces vaultReader() + vaultWriter()
FileManagerProxy *fileManager();   // ← new
// All other accessors unchanged.
```

---

## 8. Class-level migration summary

| Current class | Location | Fate |
|---|---|---|
| `Corbomite::Vault` (Task 7 stub) | `libs/core/` | **Deleted.** Replaced by new `libs/vault/` Vault. |
| `Corbomite::VaultModel` | `libs/models/` | **Dissolved.** Replaced by new `Vault`. |
| `Corbomite::NoteService` | `libs/models/` | **Dissolved.** Functions move to `FileManager` or `Vault`. |
| `VaultService` | `src/app/` | **Dissolved.** `openVault`/`closeVault` move to `CorbomiteApp`; recent-vaults extracted to a small `RecentVaults` helper. |
| `FrontMatterWriter` | `libs/core/` | **Dissolved** into `FileManager::processFrontMatter`. |
| `VaultProcess` (static helpers) | `libs/storage/` | **Dissolved** into `Vault::process`. |
| `VaultTrash` | `libs/storage/` | **Dissolved** into `Vault::trash`. |
| `VaultScanner` | `libs/storage/` | **Moved** to `libs/vault/`. |
| `FileWatchReactor` | `src/reactors/` | **Moved** to `libs/vault/` + folded into `Vault::Watcher` (private). |
| `VaultReader` / `VaultWriter` proxies | `libs/core/proxies/` | **Deleted.** Replaced by `VaultProxy` + `FileManagerProxy` in `libs/vault/proxies/`. |
| `FileSystemAdapter` / `DataAdapter` | `libs/storage/` | **Unchanged.** Vault composes the `DataAdapter *`. |
| `VaultConfig` | `libs/storage/` | **Unchanged.** Vault uses it for `.obsidian/*.json` I/O. |
| `MetadataCache` | `libs/storage/` | **Unchanged internals.** Subscribes to new Vault signals in place of old `VaultModel` signals. |
| `SQLiteIndex` | `libs/storage/` | **Unchanged internals.** Still derives from `MetadataCache::cacheChanged`. |
| `NoteMeta`, `NoteDocument` | `libs/core/` | **Unchanged.** `NoteDocument` cache lifts into `Vault::cachedRead`. |

---

## 9. Migration phases

Big-bang cluster. Phased so master builds at every landing. Full detail
emerges in the implementation plan (writing-plans). High-level phase
boundaries:

1. **Scaffold `libs/vault/`** + `TAbstractFile` / `TFile` / `TFolder` +
   skeletal `Vault` (load/unload/getRoot/getAbstractFileByPath only).
   **Delete the Task-7 `Corbomite::Vault` in `libs/core/`** AND the now-
   unbacked `VaultReader` / `VaultWriter` proxies + `tst_proxy_vault.cpp`
   + their `PluginContext` accessors. `PluginContext::setCoreServices`
   temporarily drops its `Vault *` parameter (added back in Phase 9). The
   other proxies (MetadataCacheReader, WorkspaceController, etc.) are
   untouched. Cluster Q is already on hold; this ratifies the state. New
   tests for type round-trips on TAbstractFile/TFile/TFolder.
2. **File-system ownership + watcher.** Move `VaultScanner` + fold
   `FileWatchReactor` → `Vault::Watcher`. Vault composes `DataAdapter *`.
   Tests: watcher-driven `created`/`deleted`/`modified`/`renamed` signal
   emission + mtime echo suppression.
3. **Vault mutation API.** Absorb `VaultProcess` → `Vault::process`.
   Absorb `VaultTrash` → `Vault::trash`. Implement
   `read/cachedRead/modify/append/create/createFolder/rename/remove/copy`.
   Signals fire on every mutation. `VaultModel` still present, consumers
   unchanged yet. Tests: round-trip every op + signal emission.
4. **Config-dir I/O.** `Vault::readConfigJson` / `writeConfigJson` /
   `deleteConfigJson`. Tests: `.obsidian/*.json` round-trip.
5. **`FileManager` class.** Absorb `FrontMatterWriter` → `processFrontMatter`.
   Link-rewrite-on-rename via `MetadataCache::iterateAllRefs` snapshot.
   `getNewFileParent` / `createNewMarkdownFile` / `createNewFolder` /
   `getAvailablePathForAttachment` / `insertIntoFile` / `generateMarkdownLink`
   / `trashFile`. Tests: each op + link-rewrite end-to-end.
6. **Consumer migration wave 1 — sidebars + panels.** `OutlinksPanel`,
   `BacklinksPanel`, `LocalGraphPanel`, `PropertiesPanel`,
   `FileExplorerPanel` → `Vault *`. All tests green.
7. **Consumer migration wave 2 — editor + graph + search.**
   `NoteEditorWidget`, Markoff `VaultResourceProvider` adapter,
   `GraphViewTab`, `GraphView`, `SearchPanel`, `QuickSwitcher`,
   `GraphDataBuilder`, `NotesTreeModel`. `MetadataCache` subscribes
   directly to Vault signals.
8. **App-level reshape.** `VaultService` dissolves. `MainWindow` signature
   changes: `MainWindow(VaultService *)` → `MainWindow(Vault *, FileManager *)`.
   `NoteService` dissolves into `FileManager`.
9. **Proxy layer rewrite.** Delete `libs/core/.../proxies/VaultReader.h` +
   `VaultWriter.h`. Create `libs/vault/.../proxies/VaultProxy.h` +
   `FileManagerProxy.h`. Rewrite `PluginContext::setCoreServices`. Rewrite
   `tst_proxy_vault.cpp` as `tst_vault_proxy.cpp`; new
   `tst_file_manager_proxy.cpp`.
10. **Delete VaultModel / NoteService / VaultService / FrontMatterWriter /
    VaultProcess / VaultTrash.** Single delete commit. `libs/models/` shrinks
    to `NotesTreeModel` + `TabModel` + `DailyNoteService` + `TemplateService`.
11. **Update Cluster Q.** Rewrite Cluster Q Tasks 7–12 against the new
    Vault / FileManager proxies. Resume Cluster Q implementation.

At every phase: `ctest --output-on-failure` must be green outside the 4
known-flaky pre-existing failures documented in `PROJECT-STATE.md`.

---

## 10. Testing strategy

**New test executables in `libs/vault/tests/`:**

- `tst_vault_tree` — `fileMap` integrity, parent/children invariants,
  NFC normalization, case-insensitive lookup, root invariants, tombstone
  on delete.
- `tst_vault_crud` — read/modify/create/rename/remove/trash round-trips +
  signal emission + `saving` flag toggling + `stat` population + atomic
  write contract.
- `tst_vault_watcher` — external modification → signals; mtime echo
  suppression; case-collision handling; `raw`-equivalent firehose via
  typed signals; `.obsidian/` internal change → `raw` or dedicated signal.
- `tst_vault_config` — `.obsidian/*.json` round-trip + unknown-key
  preservation (Cluster B contract).
- `tst_vault_lifecycle` — load/unload/vault-switch; replaces the vault-
  switch e2e test's unit-level scope. E2E test (`tst_vault_switch.cpp`)
  stays as regression guard per `memory/project_vault_switching`.
- `tst_file_manager_rename` — link-rewrite across a fixture vault; queue
  serialisation; `linkUpdateProgress` signal.
- `tst_file_manager_frontmatter` — `processFrontMatter` round-trips
  (absorbs existing `tst_frontmatter_writer.cpp`).
- `tst_file_manager_newfile` — `getNewFileParent` / `createNewMarkdownFile` /
  `createNewFolder` under various `newFileLocation` settings.
- `tst_file_manager_attachments` — `getAvailablePathForAttachment` under
  various `attachmentFolderPath` settings.
- `tst_file_manager_generate_link` — `generateMarkdownLink` under
  `useMarkdownLinks` + `newLinkFormat` settings.
- `tst_vault_proxy` — permission-gated method round-trips; positive +
  negative paths for every token.
- `tst_file_manager_proxy` — same for `FileManagerProxy`.

**Existing tests migrated:**

- `tst_vault_switch.cpp` (e2e) retargets onto new `Vault`. Stays green as
  regression guard.
- `tst_frontmatter_writer.cpp` → renamed / absorbed into
  `tst_file_manager_frontmatter.cpp`.
- `tst_proxy_vault.cpp` (current) → rewritten as `tst_vault_proxy.cpp`.

**Integration tier (`tests/integration/`):**

- `tst_cross_session.cpp` additions — verify vault mutations from one
  surface are visible to another after reload (Ritual-4 pattern from
  2026-04-15 test-enrichment cycle).

**E2E tier (`tests/e2e/`):**

- `tst_e2e_gui.cpp` + `tst_panels_populated.cpp` + `tst_vault_switch.cpp`
  — retarget onto `Vault *` + `FileManager *` constructor args as part of
  Phase 8.

---

## 11. Non-goals / explicit deferrals

**Never in scope for this cluster (or ever, in most cases):**

- Mobile support. No `CapacitorAdapter`.
- Async / `QFuture`-returning Vault API. Sync only.
- Electron `app://local/...?<mtime>` URL scheme —
  `VaultResourceProvider` (libs/readingview + libs/markoff) covers
  resource URLs.
- Multi-vault per process. Single `Vault *` owned by `CorbomiteApp`.
- Obsidian's `Events.raw` firehose — typed Qt signals subsume it; skip.

**Scope-deferred (build the hook, not the feature):**

- `Vault::copy(TAbstractFile *, newPath)` — declared; recursive folder
  copy body deferred.
- `FileManager::deleteProperty` / `renameProperty` bulk ops — declared;
  implementation deferred until a consumer needs them.
- `FileManager::insertIntoFile` with frontmatter merge — Phase 5 ships
  append/prepend only; frontmatter merge is a follow-up.
- `FileManager::storeTextFileBackup` + `notifyForBulkUndo` (file-recovery
  plugin interface) — wait for Cluster Q to finish internal-plugin
  wrapping.
- Plugin-driven file-parent-creator registry (`registerFileParentCreator`)
  — wait for a real plugin consumer.
- `FileManager::downloadAttachmentsForNote` + `promptForImageDownload`
  (web-clipper) — Cluster N territory.
- Modal UI helpers (`promptForDeletion` / `promptForFileRename` /
  `promptForFolderDeletion`) — stay in `src/app/` as thin adapters over
  `FileManager::trashFile` / `FileManager::renameFile` + `QMessageBox` /
  `QInputDialog`.

**Not changed by this cluster (subscribers only):**

- `MetadataCache`, `SQLiteIndex`, `Corbomite::Events` mixin,
  `Corbomite::Component`, `Workspace` / `WorkspaceLeaf` / `ViewRegistry`.
  Internals untouched; only one-line connection changes to subscribe to
  Vault signals instead of VaultModel signals.
- Existing plugin permissions (`metadata.read`, `workspace`,
  `ui.commands`, `ui.views`, `ui.menus`, `network`, `secrets`, `process`,
  `config`) — unchanged. Only `vault.*` tokens change (add `vault.events`).
- `VaultConfig` — stays in `libs/storage/`; Vault uses it via composition.
- `FileSystemAdapter` / `DataAdapter` — stay in `libs/storage/`; Vault
  owns the pointer but not the type.

---

## 12. Cluster designation

This becomes **Cluster Q.0 — Vault foundation**. It is a prerequisite
inserted before the existing Cluster Q Tasks 7–12 (which rewrite PluginContext
wiring + migrate 8 internal plugins). After Q.0 lands, the Cluster Q
implementation plan's Tasks 7–12 are rewritten to reference the new
`VaultProxy` + `FileManagerProxy` + the new `PluginContext::setCoreServices`
signature. Cluster Q Tasks 1–6 (PluginMetaData / Plugin / PluginContext
skeletal / PluginManager / PluginPermissionGrantDialog) remain as-shipped;
only the proxies and wiring are affected.

Alternative considered: promote the Vault foundation to a separate
**Cluster R** and keep Cluster Q's task numbering untouched. Rejected on
the grounds that the work is semantically part of the plugin-infrastructure
conversation opened by Cluster Q and stays more discoverable under the Q.0
designation.

---

## 13. Open questions

None as of spec-write time. Subsequent questions that arise during
plan-writing or implementation append here with `Asked: YYYY-MM-DD by <agent>`
per the `PROJECT-STATE.md` open-questions format and resolve into §"Recent
decisions".
