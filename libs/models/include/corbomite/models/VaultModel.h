// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QHash>
#include <QVector>
#include "corbomite/core/NoteMeta.h"
#include "corbomite/core/NoteDocument.h"
#include "corbomite/storage/FileSystemAdapter.h"
#include "corbomite/vault/VaultScanner.h"

namespace Corbomite {

class SQLiteIndex;

class VaultModel : public QObject {
    Q_OBJECT

public:
    explicit VaultModel(QObject *parent = nullptr);
    ~VaultModel() override;

    // Vault lifecycle
    void open(const QString &vaultPath);
    void close();
    bool isOpen() const;

    // Vault identity
    QString path() const;
    QString name() const;
    QString configPath() const;

    // Note collection
    QVector<NoteMeta> allNotes() const;
    QStringList allTags() const;
    void invalidateTagCache();
    void setSearchIndex(SQLiteIndex *index);
    NoteMeta noteMeta(const QString &relativePath) const;
    bool noteExists(const QString &relativePath) const;

    // Document cache — loads content on first access
    NoteDocument *openDocument(const QString &relativePath);
    NoteDocument *cachedDocument(const QString &relativePath) const;

    // Mutation (internal bookkeeping — formerly driven by NoteService /
    // FileWatchReactor; now also invoked by the high-level note ops below
    // after their filesystem half completes).
    void addNote(const QString &relativePath);
    void removeNote(const QString &relativePath);
    void renameNote(const QString &oldPath, const QString &newPath);
    void updateNoteMeta(const QString &relativePath);

    // High-level note operations absorbed from the deleted `NoteService`
    // during Q.0 Phase 8. All five write the vault through the legacy
    // FileSystemAdapter path (same behaviour as before) and emit their
    // bookkeeping counterparts on success. Link-repair on rename routes
    // through `m_searchIndex->repairLinks` when the search index is
    // attached.
    NoteDocument *createNote(const QString &name, const QString &folder);
    bool          saveNote(NoteDocument *doc);
    bool          renameNoteByPath(const QString &oldRelativePath,
                                   const QString &newRelativePath);
    bool          deleteNoteByPath(const QString &relativePath);

Q_SIGNALS:
    void vaultScanned();
    void noteAdded(const QString &relativePath);
    void noteRemoved(const QString &relativePath);
    void noteRenamed(const QString &oldPath, const QString &newPath);
    void noteModified(const QString &relativePath);
    void noteSaved(const QString &relativePath);

private:
    QString m_vaultPath;
    QHash<QString, NoteMeta> m_notes;        // relativePath → meta
    QHash<QString, NoteDocument *> m_docs;   // relativePath → cached document
    FileSystemAdapter m_fs;
    VaultScanner m_scanner;
    mutable QStringList m_cachedTags;
    mutable bool m_tagCacheDirty = true;
    SQLiteIndex *m_searchIndex = nullptr;
};

} // namespace Corbomite
