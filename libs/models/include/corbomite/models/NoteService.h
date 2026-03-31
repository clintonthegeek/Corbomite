// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QString>

namespace Corbomite {

class VaultModel;
class NoteDocument;
class SQLiteIndex;

class NoteService : public QObject {
    Q_OBJECT

public:
    explicit NoteService(VaultModel *vault, QObject *parent = nullptr);

    NoteDocument *openNote(const QString &relativePath);
    NoteDocument *createNote(const QString &name, const QString &folderPath);
    bool saveNote(NoteDocument *doc);
    bool renameNote(const QString &oldRelPath, const QString &newRelPath);
    bool deleteNote(const QString &relativePath);

    void setSearchIndex(SQLiteIndex *index);

private:
    QString resolveUniquePath(const QString &baseName, const QString &folder) const;

    VaultModel *m_vault;
    SQLiteIndex *m_searchIndex = nullptr;
};

} // namespace Corbomite
