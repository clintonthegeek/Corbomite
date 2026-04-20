// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QAbstractItemModel>
#include <QVector>
#include "corbomite/storage/SQLiteIndex.h"

namespace Corbomite {

class SearchResultsModel : public QAbstractItemModel {
    Q_OBJECT

public:
    enum Roles {
        NotePathRole = Qt::UserRole + 1,
        SnippetRole,
        MatchCountRole,
        /// Merge-sorted, non-overlapping [start, end) UTF-16 ranges over
        /// the snippet text — for rich rendering in a custom delegate.
        /// Returns an empty QVector on group/parent rows.
        MatchRangesRole,
    };

    explicit SearchResultsModel(QObject *parent = nullptr);

    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    void setResults(const QVector<SearchMatch> &results);
    void clear();

    int fileCount() const;
    int totalMatchCount() const;

private:
    // Group matches by file
    struct FileGroup {
        QString notePath;
        QString noteName;
        QVector<SearchMatch> matches;
    };

    QVector<FileGroup> m_groups;
};

} // namespace Corbomite
