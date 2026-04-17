// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <functional>
#include <memory>
#include <unordered_map>

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QObject>
#include <QString>
#include <QVector>

#include "corbomite/storage/DataAdapter.h"  // for WriteHints + FileStat

namespace Corbomite {

class DataAdapter;
class NoteDocument;
class TAbstractFile;
class TFile;
class TFolder;

namespace detail { class Watcher; }

// Forward-declared type pointers appear in Vault's Qt signals. Callers that
// connect via QSignalSpy / Qt::QueuedConnection / Events-mixin dispatch need
// these registered so QMetaType can copy them through QVariant. Registration
// happens at Vault's first ctor call via a static-local flag in Vault.cpp.
}  // namespace Corbomite

#include "corbomite/vault/TAbstractFile.h"
#include "corbomite/vault/TFile.h"
#include "corbomite/vault/TFolder.h"

Q_DECLARE_METATYPE(Corbomite::TAbstractFile *)
Q_DECLARE_METATYPE(Corbomite::TFile *)
Q_DECLARE_METATYPE(Corbomite::TFolder *)

namespace Corbomite {

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

    // ---- Config directory (.obsidian by default) ----
    QString configDir() const { return m_configDir; }
    /// Rejects names without a leading '.' or bare '.'; falls back silently.
    void    setConfigDir(const QString &d);

    // ---- Config-JSON I/O (.obsidian/<name>.json) ----
    QJsonValue readConfigJson(const QString &name) const;
    bool       writeConfigJson(const QString &name, const QJsonValue &value);
    bool       deleteConfigJson(const QString &name);

    // ---- Tree queries ----
    TFolder        *getRoot() const;
    TAbstractFile  *getAbstractFileByPath(const QString &path) const;
    TFile          *getFileByPath(const QString &path) const;
    TFolder        *getFolderByPath(const QString &path) const;
    QVector<TFile *>         getMarkdownFiles() const;
    QVector<TFile *>         getFiles() const;
    QVector<TAbstractFile *> getAllLoadedFiles() const;
    bool    isEmpty() const;

    // ---- Read ----
    QByteArray read(TFile *f) const;
    QByteArray readBinary(TFile *f) const;
    QByteArray readRaw(const QString &path) const;
    QByteArray cachedRead(TFile *f);

    // ---- Write (sync, atomic via DataAdapter) ----
    bool modify(TFile *f, const QByteArray &body, const WriteHints &hints = {});
    bool modifyBinary(TFile *f, const QByteArray &body, const WriteHints &hints = {});
    bool append(TFile *f, const QByteArray &body);

    /// Atomic read-modify-write cycle. Mutator receives the current bytes
    /// and returns the new bytes. Concurrent calls on the same file are
    /// serialised through a per-path mutex (same contract as the absorbed
    /// VaultProcess class). Cross-process safety comes from the watcher +
    /// mtime echo-suppression contract, not this method.
    using ProcessMutator = std::function<QByteArray(const QByteArray &)>;
    bool process(TFile *f, const ProcessMutator &mutator);

    TFile   *create(const QString &path, const QByteArray &body);
    TFile   *createBinary(const QString &path, const QByteArray &body);
    TFolder *createFolder(const QString &path);

    bool rename(TAbstractFile *f, const QString &newPath);
    bool remove(TAbstractFile *f, bool recursive = false);
    bool copy(TAbstractFile *f, const QString &newPath);
    bool trash(TAbstractFile *f, bool useSystem);

    // ---- NoteDocument lifecycle (Phase 10 — absorbed from VaultModel) ----
    /// Returns the cached NoteDocument for `relPath`, creating + hydrating
    /// one from disk on first access. Cache entries live until `unload()`,
    /// the file is externally deleted, or the file is renamed (entry is
    /// rekeyed on rename). Returns nullptr if no vault is loaded or the
    /// path is outside the tree.
    NoteDocument *openDocument(const QString &relPath);

    /// Returns the cached NoteDocument for `relPath` or nullptr when none
    /// has been opened. Safe to call when no vault is loaded.
    NoteDocument *cachedDocument(const QString &relPath) const;

    /// Writes `doc->markdown()` through `modify()` (so echo-suppression
    /// keeps the save from firing as an external event), clears the doc's
    /// modified flag, emits `NoteDocument::saved`, and emits the
    /// `documentSaved` signal for observers. Returns false if the doc's
    /// file isn't in the tree.
    bool saveDocument(NoteDocument *doc);

signals:
    void created(Corbomite::TAbstractFile *f);
    void modified(Corbomite::TFile *f);
    void deletedFile(Corbomite::TAbstractFile *f);
    void renamed(Corbomite::TAbstractFile *f, const QString &oldPath);
    void closed();
    /// Fires after `saveDocument` successfully writes. Emitted with the
    /// relative path so observers don't need to poke at the NoteDocument
    /// pointer lifetime.
    void documentSaved(const QString &relPath);

private Q_SLOTS:
    // Watcher-dispatched handlers. Relative paths only (no basePath prefix).
    void onExternalCreated(const QString &relPath);
    void onExternalModified(const QString &relPath);
    void onExternalDeleted(const QString &relPath);
    void onExternalRenamed(const QString &oldRel, const QString &newRel);

private:
    DataAdapter *m_adapter;
    QString      m_basePath;
    bool         m_loaded = false;

    // Owned; keyed by NFC-normalized path. "/" is always present after load.
    // std::unordered_map (not QHash) because QHash requires value-copyable
    // during rehash; unique_ptr is move-only.
    struct QStringStdHash {
        std::size_t operator()(const QString &s) const noexcept { return qHash(s); }
    };
    std::unordered_map<QString, std::unique_ptr<TAbstractFile>, QStringStdHash> m_fileMap;

    // Non-owning shortcut to the root node (also in m_fileMap at "/").
    TFolder *m_root = nullptr;

    // Filesystem watcher — private detail class; header in src/.
    std::unique_ptr<detail::Watcher> m_watcher;

    // Deferred-deletion queue: entries persist for one event-loop turn after
    // `deletedFile` emission so synchronous subscribers can observe
    // `deleted == true` and read the object without UAF. std::vector
    // (not QVector) because QVector requires value-copyable during grow.
    std::vector<std::unique_ptr<TAbstractFile>> m_pendingDelete;

    QString m_configDir = QStringLiteral(".obsidian");

    // Sparse read cache populated by cachedRead; invalidated on
    // modify/delete/rename. Lives on Vault so every TFile doesn't pay for
    // an unused QByteArray field.
    mutable QHash<QString, QByteArray> m_readCache;

    // NoteDocument cache (Phase 10 — absorbed from VaultModel). Keyed by
    // normalized relative path. Entries are owned by Vault and deleted on
    // `unload()`, `onExternalDeleted`, or rebuild. Rename rekeys the entry.
    QHash<QString, NoteDocument *> m_docs;

    // Self-write echo suppression ledger. Outgoing writes (Phase 3
    // Vault::modify/create/process/...) call stampSelfWrite(rel, mtimeMs)
    // before the adapter write; onExternalModified calls consumeSelfWrite
    // on the event's (rel, mtimeMs) and returns without emitting if the
    // ledger holds a match. Entries auto-expire after 1s.
    QHash<QString, qint64> m_selfWriteMtimes;
    void stampSelfWrite(const QString &rel, qint64 mtimeMs);
    bool consumeSelfWrite(const QString &rel, qint64 mtimeMs);

    void buildTree();
    void teardownTree();
};

} // namespace Corbomite
