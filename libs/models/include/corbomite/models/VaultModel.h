// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QHash>
#include <QVector>
#include "corbomite/core/NoteMeta.h"
#include "corbomite/core/NoteDocument.h"
#include "corbomite/storage/FileSystemAdapter.h"
#include "corbomite/storage/VaultScanner.h"

namespace Corbomite {

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
    NoteMeta noteMeta(const QString &relativePath) const;
    bool noteExists(const QString &relativePath) const;

    // Document cache — loads content on first access
    NoteDocument *openDocument(const QString &relativePath);
    NoteDocument *cachedDocument(const QString &relativePath) const;

    // Mutation (called by NoteService or FileWatchReactor)
    void addNote(const QString &relativePath);
    void removeNote(const QString &relativePath);
    void renameNote(const QString &oldPath, const QString &newPath);
    void updateNoteMeta(const QString &relativePath);

Q_SIGNALS:
    void vaultScanned();
    void noteAdded(const QString &relativePath);
    void noteRemoved(const QString &relativePath);
    void noteRenamed(const QString &oldPath, const QString &newPath);
    void noteModified(const QString &relativePath);

private:
    QString m_vaultPath;
    QHash<QString, NoteMeta> m_notes;        // relativePath → meta
    QHash<QString, NoteDocument *> m_docs;   // relativePath → cached document
    FileSystemAdapter m_fs;
    VaultScanner m_scanner;
};

} // namespace Corbomite
