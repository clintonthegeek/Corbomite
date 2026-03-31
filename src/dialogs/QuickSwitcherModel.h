// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QAbstractListModel>
#include <QVector>
#include <QSet>
#include "corbomite/core/NoteMeta.h"

namespace Corbomite {

class QuickSwitcherModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum Roles {
        NotePathRole = Qt::UserRole + 1,
        NoteNameRole,
        FolderPathRole,
        IsRecentRole
    };

    explicit QuickSwitcherModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    void setNotes(const QVector<NoteMeta> &notes);
    void setRecentPaths(const QStringList &recentPaths);

    // Future: add setAliases() for frontmatter alias matching

private:
    struct Entry {
        QString relativePath;  // e.g. "folder/note.md"
        QString name;          // e.g. "note"
        QString folder;        // e.g. "folder" or ""
        bool isRecent = false;
    };

    QVector<Entry> m_entries;
    QSet<QString> m_recentSet;
};

} // namespace Corbomite
