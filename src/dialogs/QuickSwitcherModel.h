// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QPointer>
#include <QSet>
#include <QStringList>
#include <QVector>

#include "corbomite/core/NoteMeta.h"

namespace Corbomite {

class MetadataCacheReader;

class QuickSwitcherModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum Roles {
        NotePathRole = Qt::UserRole + 1,
        NoteNameRole,
        FolderPathRole,
        IsRecentRole,
        AliasRole
    };

    explicit QuickSwitcherModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    void setNotes(const QVector<NoteMeta> &notes);
    void setRecentPaths(const QStringList &recentPaths);

    /// The metadata reader the frontend lives with; aliases come from
    /// that cache's frontmatter lazily (AliasRole) and are cached per path
    /// the first time a row is matched. A null reader means no aliases.
    void setMetadata(Corbomite::MetadataCacheReader *cache);

private:
    struct Entry {
        QString relativePath;  // e.g. "folder/note.md"
        QString name;          // e.g. "note"
        QString folder;        // e.g. "folder" or ""
        bool isRecent = false;
    };

    QVector<Entry> m_entries;
    QSet<QString> m_recentSet;
    QPointer<Corbomite::MetadataCacheReader> m_metadata;
    /// Alias memo: path -> frontmatter aliases (read lazily via
    /// MetadataCacheReader::frontmatterFor when the cache is present).
    mutable QHash<QString, QStringList> m_aliasCache;
};

} // namespace Corbomite
