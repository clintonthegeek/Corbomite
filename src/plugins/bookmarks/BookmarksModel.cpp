// SPDX-License-Identifier: GPL-3.0-or-later
#include "BookmarksModel.h"

#include "BookmarkItem.h"
#include "BookmarksStore.h"

#include <QFileInfo>
#include <QIcon>
#include <QMimeData>

namespace Corbomite::Bookmarks {

BookmarksModel::BookmarksModel(BookmarksStore *store, QObject *parent)
    : QAbstractItemModel(parent), m_store(store)
{
    if (m_store) connect(m_store, &BookmarksStore::changed,
                         this, &BookmarksModel::onStoreChanged);
}
BookmarksModel::~BookmarksModel() = default;

void BookmarksModel::onStoreChanged()
{
    beginResetModel();
    endResetModel();
}

// Internal pointer layout: store a `const BookmarkItem *` directly. Root-level
// rows carry a nullptr parent pointer; nested rows carry a pointer to their
// parent item. (This is stable because BookmarksStore::changed resets the model.)

QModelIndex BookmarksModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!m_store || column != 0) return {};
    const BookmarkItem *parentItem = itemForIndex(parent);
    const auto &list = parentItem ? parentItem->children : m_store->rootItems();
    if (row < 0 || row >= list.size()) return {};
    return createIndex(row, column, const_cast<BookmarkItem *>(&list.at(row)));
}

QModelIndex BookmarksModel::parent(const QModelIndex &child) const
{
    if (!child.isValid() || !m_store) return {};
    const auto *childItem = static_cast<const BookmarkItem *>(child.internalPointer());
    // Walk the store to find childItem's parent (linear — acceptable for expected tree sizes).
    std::function<const BookmarkItem *(const QList<BookmarkItem> &, const BookmarkItem *)> find =
        [&](const QList<BookmarkItem> &siblings,
            const BookmarkItem *target) -> const BookmarkItem * {
        for (const auto &s : siblings) {
            for (const auto &c : s.children) {
                if (&c == target) return &s;
            }
            if (auto *r = find(s.children, target)) return r;
        }
        return nullptr;
    };
    const BookmarkItem *parentItem = find(m_store->rootItems(), childItem);
    if (!parentItem) return {};

    // Find parentItem's row among its parent's siblings (or root).
    std::function<int(const QList<BookmarkItem> &, const BookmarkItem *)> rowIn =
        [&](const QList<BookmarkItem> &list, const BookmarkItem *it) -> int {
        for (int i = 0; i < list.size(); ++i)
            if (&list.at(i) == it) return i;
        return -1;
    };
    int row = rowIn(m_store->rootItems(), parentItem);
    if (row >= 0)
        return createIndex(row, 0, const_cast<BookmarkItem *>(parentItem));
    // Nested — recurse once more.
    std::function<int(const QList<BookmarkItem> &, const BookmarkItem *)> findRow =
        [&](const QList<BookmarkItem> &list, const BookmarkItem *it) -> int {
        for (const auto &s : list) {
            int r = rowIn(s.children, it);
            if (r >= 0) return r;
            r = findRow(s.children, it);
            if (r >= 0) return r;
        }
        return -1;
    };
    row = findRow(m_store->rootItems(), parentItem);
    return row >= 0 ? createIndex(row, 0, const_cast<BookmarkItem *>(parentItem)) : QModelIndex{};
}

int BookmarksModel::rowCount(const QModelIndex &parent) const
{
    if (!m_store) return 0;
    const BookmarkItem *item = itemForIndex(parent);
    return item ? item->children.size() : m_store->rootItems().size();
}

int BookmarksModel::columnCount(const QModelIndex &) const { return 1; }

QVariant BookmarksModel::data(const QModelIndex &index, int role) const
{
    const BookmarkItem *item = itemForIndex(index);
    if (!item) return {};
    switch (role) {
    case Qt::DisplayRole:
        if (!item->title.isEmpty()) return item->title;
        if (item->type == QStringLiteral("file") || item->type == QStringLiteral("folder"))
            return QFileInfo(item->path).completeBaseName();
        if (item->type == QStringLiteral("search")) return item->query;
        if (item->type == QStringLiteral("graph")) return QStringLiteral("Graph view");
        if (item->type == QStringLiteral("group")) return QStringLiteral("Group");
        return item->type;
    case Qt::DecorationRole: {
        static const QHash<QString, QString> iconByType = {
            {QStringLiteral("file"),   QStringLiteral("text-x-generic")},
            {QStringLiteral("folder"), QStringLiteral("folder")},
            {QStringLiteral("search"), QStringLiteral("edit-find")},
            {QStringLiteral("graph"),  QStringLiteral("view-sort")},
            {QStringLiteral("group"),  QStringLiteral("folder-bookmark")},
        };
        return QIcon::fromTheme(iconByType.value(item->type, QStringLiteral("bookmark-new")));
    }
    case Qt::ToolTipRole:
        return item->path.isEmpty() ? item->query : item->path;
    case BookmarksTypeRole: return item->type;
    case BookmarksPathRole: return item->path.isEmpty() ? item->query : item->path;
    }
    return {};
}

Qt::ItemFlags BookmarksModel::flags(const QModelIndex &index) const
{
    Qt::ItemFlags f = QAbstractItemModel::flags(index);
    if (index.isValid()) f |= Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled;
    else                 f |= Qt::ItemIsDropEnabled;
    return f;
}

Qt::DropActions BookmarksModel::supportedDropActions() const { return Qt::MoveAction; }

QStringList BookmarksModel::mimeTypes() const
{
    return {QStringLiteral("application/x-corbomite-bookmarks-drag")};
}

QMimeData *BookmarksModel::mimeData(const QModelIndexList &indexes) const
{
    auto *mime = new QMimeData;
    if (indexes.isEmpty()) return mime;
    const QStringList path = pathOf(indexes.first());
    mime->setData(QStringLiteral("application/x-corbomite-bookmarks-drag"),
                  path.join(QLatin1Char('/')).toUtf8());
    return mime;
}

bool BookmarksModel::dropMimeData(const QMimeData *data, Qt::DropAction action,
                                  int row, int /*column*/, const QModelIndex &parent)
{
    if (action != Qt::MoveAction || !m_store) return false;
    if (!data->hasFormat(QStringLiteral("application/x-corbomite-bookmarks-drag"))) return false;
    const QString joined = QString::fromUtf8(
        data->data(QStringLiteral("application/x-corbomite-bookmarks-drag")));
    const QStringList fromPath = joined.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    const QStringList toParent = pathOf(parent);
    return m_store->moveBookmark(fromPath, toParent, row);
}

QStringList BookmarksModel::pathOf(const QModelIndex &index) const
{
    if (!index.isValid()) return {};
    QStringList out;
    QModelIndex cur = index;
    while (cur.isValid()) {
        out.prepend(QString::number(cur.row()));
        cur = cur.parent();
    }
    return out;
}

const BookmarkItem *BookmarksModel::itemForIndex(const QModelIndex &index) const
{
    if (!index.isValid()) return nullptr;
    return static_cast<const BookmarkItem *>(index.internalPointer());
}

} // namespace Corbomite::Bookmarks
