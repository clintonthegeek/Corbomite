// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <functional>

#include <QObject>
#include <QString>
#include <QVariantMap>

class QWidget;

namespace Corbomite {

class Vault;
class TAbstractFile;
class TFile;
class TFolder;
class MetadataCache;

/// Higher-level Vault operations: link-aware rename, frontmatter mutation,
/// new-file placement, attachment placement, link generation, trash routing.
/// Depends on Vault + MetadataCache.
class FileManager : public QObject
{
    Q_OBJECT
public:
    FileManager(Vault *vault, MetadataCache *cache, QObject *parent = nullptr);

    Vault         *vault() const { return m_vault; }
    MetadataCache *metadataCache() const { return m_cache; }

    // ---- Rename with link rewrite ----
    bool renameFile(TAbstractFile *f, const QString &newPath);

    // ---- Atomic frontmatter mutation ----
    using FrontMatterMutator = std::function<void(QVariantMap &)>;
    bool processFrontMatter(TFile *f, FrontMatterMutator mut);

    // ---- Bulk property ops (declared; bodies deferred per spec §11) ----
    bool deleteProperty(const QString &key);
    bool renameProperty(const QString &oldK, const QString &newK);

    // ---- New-file placement ----
    TFolder *getNewFileParent(const QString &hintPath,
                              const QString &filename = {}) const;
    TFile   *createNewMarkdownFile(TFolder *parent,
                                   const QString &name,
                                   const QByteArray &content = {});
    TFile   *createNewMarkdownFileFromLinktext(const QString &linkText,
                                               const QString &hintPath);
    TFolder *createNewFolder(TFolder *parent);

    // ---- Path-based convenience (Phase 10 — absorbed from VaultModel) ----
    /// Creates `<folder>/<name>.md` (dedup-appending " 1", " 2", ... on
    /// collision), auto-creating intermediate folders. Returns the new
    /// TFile or nullptr. Pass an empty folder to create at the vault
    /// root.
    TFile *createMarkdownNote(const QString &name, const QString &folder);

    /// Looks up `oldRel` as a TAbstractFile and renames it to `newRel`
    /// (link rewrite via MetadataCache applies per `renameFile`).
    bool   renameFileByPath(const QString &oldRel, const QString &newRel);

    /// Looks up `relPath` as a TAbstractFile and routes it through
    /// `trashFile` (local trash; system-trash fallback).
    bool   trashFileByPath(const QString &relPath);

    // ---- Attachments ----
    QString getAvailablePathForAttachment(const QString &linktext,
                                          const QString &sourcePathHint = {}) const;

    // ---- Content merge ----
    enum class InsertMode { Append, Prepend };
    bool insertIntoFile(TFile *f, const QByteArray &content, InsertMode mode);

    // ---- Link generation ----
    QString generateMarkdownLink(TFile *target,
                                 const QString &sourcePath,
                                 const QString &subpath = {},
                                 const QString &displayText = {}) const;

    // ---- Trash router ----
    bool trashFile(TAbstractFile *f);

    // ---- Interactive prompts (Cluster R Phase 2) ----
    /// Opens a modal rename dialog anchored to `parent`. Returns the new
    /// full vault-relative path on success (after `renameFile` committed),
    /// empty QString on cancel or failure.
    QString promptForFileRename(TAbstractFile *file, QWidget *parent = nullptr);

Q_SIGNALS:
    void renameStarted(Corbomite::TAbstractFile *f, const QString &newPath);
    void renameFinished(Corbomite::TAbstractFile *f, const QString &oldPath);
    void linkUpdateProgress(int done, int total);

private:
    Vault         *m_vault;
    MetadataCache *m_cache;
};

} // namespace Corbomite
