// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <memory>
#include <unordered_map>

#include <QHash>
#include <QObject>
#include <QString>
#include <QVector>

namespace Corbomite {

class DataAdapter;
class TAbstractFile;
class TFile;
class TFolder;

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

    // ---- Tree queries ----
    TFolder        *getRoot() const;
    TAbstractFile  *getAbstractFileByPath(const QString &path) const;
    TFile          *getFileByPath(const QString &path) const;
    TFolder        *getFolderByPath(const QString &path) const;
    QVector<TFile *>         getMarkdownFiles() const;
    QVector<TFile *>         getFiles() const;
    QVector<TAbstractFile *> getAllLoadedFiles() const;
    bool    isEmpty() const;

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

    void buildTree();
    void teardownTree();
};

} // namespace Corbomite
