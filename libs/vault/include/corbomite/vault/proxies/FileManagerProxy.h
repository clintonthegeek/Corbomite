// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QByteArray>
#include <QSet>
#include <QString>

#include "corbomite/vault/FileManager.h"

class QWidget;

namespace Corbomite {

class TAbstractFile;
class TFile;
class TFolder;

/// Permission-gated plugin-facing `FileManager` facade. Mutations gate on
/// `vault.write`; tree/attachment queries gate on `vault.read`;
/// `generateMarkdownLink` gates on `metadata.read` because it reads the
/// host-side `MetadataCache`. Methods return empty / false / nullptr on
/// permission refusal.
class FileManagerProxy
{
public:
    FileManagerProxy(FileManager *fm, const QSet<QString> &granted,
                     QString pluginId);

    FileManagerProxy(const FileManagerProxy &) = delete;
    FileManagerProxy &operator=(const FileManagerProxy &) = delete;

    // ---- Mutation (gated by vault.write) ----
    bool     renameFile(TAbstractFile *f, const QString &newPath);
    bool     processFrontMatter(TFile *f, FileManager::FrontMatterMutator mut);
    TFile   *createNewMarkdownFile(TFolder *parent, const QString &name,
                                   const QByteArray &content = {});
    TFolder *createNewFolder(TFolder *parent);
    bool     insertIntoFile(TFile *f, const QByteArray &content,
                            FileManager::InsertMode mode);
    bool     trashFile(TAbstractFile *f);

    // ---- Interactive prompts (gated by vault.write) ----
    /// Opens the validated `RenameDialog` and commits via `renameFile` —
    /// the link-rewrite-aware path. Returns the new full vault-relative
    /// path on success, empty QString on cancel/refusal.
    QString  promptForFileRename(TAbstractFile *f, QWidget *parent = nullptr);

    /// Opens the trash-option-aware `DeleteConfirmDialog` and routes
    /// through `Vault::trash`/`Vault::remove` per the `[Files]/TrashOption`
    /// setting. Returns true iff the file was actually removed.
    bool     promptForDeletion(TAbstractFile *f, QWidget *parent = nullptr);

    // ---- Query (gated by vault.read) ----
    TFolder *getNewFileParent(const QString &hintPath,
                              const QString &filename = {}) const;
    QString  getAvailablePathForAttachment(const QString &linktext,
                                           const QString &sourcePathHint = {}) const;

    // ---- Query (gated by metadata.read — reads MetadataCache) ----
    QString  generateMarkdownLink(TFile *target, const QString &sourcePath,
                                  const QString &subpath = {},
                                  const QString &displayText = {}) const;

private:
    FileManager   *m_fm;
    QSet<QString>  m_granted;
    QString        m_pluginId;

    bool canRead() const
    {
        return m_granted.contains(QStringLiteral("vault.read"));
    }
    bool canWrite() const
    {
        return m_granted.contains(QStringLiteral("vault.write"));
    }
    bool canMetadataRead() const
    {
        return m_granted.contains(QStringLiteral("metadata.read"));
    }

    void logDenied(const char *method, const char *requiredToken) const;
};

}  // namespace Corbomite
