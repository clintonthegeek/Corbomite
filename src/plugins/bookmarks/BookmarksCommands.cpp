// SPDX-License-Identifier: GPL-3.0-or-later
#include "BookmarkItem.h"
#include "BookmarksPlugin.h"
#include "BookmarksStore.h"

#include <QDateTime>

namespace Corbomite::Bookmarks {

void BookmarksPlugin::bookmarkFile(BookmarksStore *store, const QString &path,
                                   const QStringList &groupPath)
{
    if (!store || path.isEmpty()) return;
    BookmarkItem item;
    item.type  = QStringLiteral("file");
    item.path  = path;
    item.ctime = QDateTime::currentMSecsSinceEpoch();
    store->addBookmark(std::move(item), groupPath);
}

void BookmarksPlugin::bookmarkAllTabs(BookmarksStore *store,
                                      const QStringList &paths,
                                      const QStringList &groupPath)
{
    if (!store) return;
    for (const QString &p : paths)
        bookmarkFile(store, p, groupPath);
}

void BookmarksPlugin::bookmarkHeading(BookmarksStore *store, const QString &path,
                                      const QString &heading,
                                      const QStringList &groupPath)
{
    if (!store || path.isEmpty() || heading.isEmpty()) return;
    const QString sub = heading.startsWith(QLatin1Char('#'))
                            ? heading
                            : QLatin1Char('#') + heading;
    BookmarkItem item;
    item.type    = QStringLiteral("file");
    item.path    = path + sub;
    item.subpath = sub;
    item.ctime   = QDateTime::currentMSecsSinceEpoch();
    store->addBookmark(std::move(item), groupPath);
}

void BookmarksPlugin::bookmarkBlock(BookmarksStore *store, const QString &path,
                                    const QString &blockId,
                                    const QStringList &groupPath)
{
    if (!store || path.isEmpty() || blockId.isEmpty()) return;
    QString id = blockId;
    if (!id.startsWith(QLatin1String("#^"))) {
        if (id.startsWith(QLatin1Char('^')))
            id.prepend(QLatin1Char('#'));
        else
            id = QStringLiteral("#^") + id;
    }
    BookmarkItem item;
    item.type    = QStringLiteral("file");
    item.path    = path + id;
    item.subpath = id;
    item.ctime   = QDateTime::currentMSecsSinceEpoch();
    store->addBookmark(std::move(item), groupPath);
}

void BookmarksPlugin::bookmarkSearch(BookmarksStore *store, const QString &query,
                                     const QStringList &groupPath)
{
    if (!store || query.isEmpty()) return;
    BookmarkItem item;
    item.type  = QStringLiteral("search");
    item.query = query;
    item.ctime = QDateTime::currentMSecsSinceEpoch();
    store->addBookmark(std::move(item), groupPath);
}

void BookmarksPlugin::bookmarkGraph(BookmarksStore *store,
                                    const QJsonObject &options,
                                    const QStringList &groupPath)
{
    if (!store) return;
    BookmarkItem item;
    item.type    = QStringLiteral("graph");
    item.options = options;
    item.ctime   = QDateTime::currentMSecsSinceEpoch();
    store->addBookmark(std::move(item), groupPath);
}

} // namespace Corbomite::Bookmarks
