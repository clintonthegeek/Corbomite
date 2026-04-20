// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QAbstractItemModel>

namespace Corbomite::Bookmarks {

class BookmarksStore;
struct BookmarkItem;

class BookmarksModel : public QAbstractItemModel
{
    Q_OBJECT
public:
    enum CustomRoles {
        BookmarksTypeRole = Qt::UserRole + 1,  ///< raw `type` string
        BookmarksPathRole = Qt::UserRole + 2,  ///< full `path` (or query)
    };

    explicit BookmarksModel(BookmarksStore *store, QObject *parent = nullptr);
    ~BookmarksModel() override;

    // QAbstractItemModel
    QModelIndex index(int row, int column, const QModelIndex &parent) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    int rowCount(const QModelIndex &parent) const override;
    int columnCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;

    Qt::ItemFlags flags(const QModelIndex &index) const override;
    Qt::DropActions supportedDropActions() const override;
    QStringList mimeTypes() const override;
    QMimeData *mimeData(const QModelIndexList &indexes) const override;
    bool dropMimeData(const QMimeData *data, Qt::DropAction action,
                      int row, int column, const QModelIndex &parent) override;

    /// Translate a QModelIndex to a store tree-path (list of int-as-string).
    QStringList pathOf(const QModelIndex &index) const;

private slots:
    void onStoreChanged();

private:
    BookmarksStore *m_store = nullptr;

    const BookmarkItem *itemForIndex(const QModelIndex &index) const;
};

} // namespace Corbomite::Bookmarks
