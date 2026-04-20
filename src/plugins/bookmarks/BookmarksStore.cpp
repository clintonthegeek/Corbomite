// SPDX-License-Identifier: GPL-3.0-or-later
#include "BookmarksStore.h"

#include <QJsonArray>

namespace Corbomite::Bookmarks {

// Known canonical keys per item type — anything else rolls into unknownKeys.
static const QSet<QString> &knownKeys()
{
    static const QSet<QString> ks = {
        QStringLiteral("type"), QStringLiteral("ctime"),
        QStringLiteral("path"), QStringLiteral("subpath"),
        QStringLiteral("title"), QStringLiteral("query"),
        QStringLiteral("options"), QStringLiteral("items"),
    };
    return ks;
}

BookmarksStore::BookmarksStore(QObject *parent) : QObject(parent) {}
BookmarksStore::~BookmarksStore() = default;

BookmarkItem BookmarksStore::parseItem(const QJsonObject &obj)
{
    BookmarkItem item;
    item.type    = obj.value(QStringLiteral("type")).toString();
    item.ctime   = obj.value(QStringLiteral("ctime")).toVariant().toLongLong();
    item.path    = obj.value(QStringLiteral("path")).toString();
    item.subpath = obj.value(QStringLiteral("subpath")).toString();
    item.title   = obj.value(QStringLiteral("title")).toString();
    item.query   = obj.value(QStringLiteral("query")).toString();
    item.options = obj.value(QStringLiteral("options")).toObject();

    if (obj.contains(QStringLiteral("items"))) {
        const QJsonArray arr = obj.value(QStringLiteral("items")).toArray();
        for (const auto &v : arr)
            item.children.append(parseItem(v.toObject()));
    }

    for (auto it = obj.begin(); it != obj.end(); ++it) {
        if (!knownKeys().contains(it.key()))
            item.unknownKeys.insert(it.key(), it.value());
    }
    return item;
}

QJsonObject BookmarksStore::itemToJson(const BookmarkItem &item)
{
    QJsonObject obj;
    obj.insert(QStringLiteral("type"), item.type);
    obj.insert(QStringLiteral("ctime"), item.ctime);
    if (!item.path.isEmpty())    obj.insert(QStringLiteral("path"), item.path);
    if (!item.subpath.isEmpty()) obj.insert(QStringLiteral("subpath"), item.subpath);
    if (!item.title.isEmpty())   obj.insert(QStringLiteral("title"), item.title);
    if (!item.query.isEmpty())   obj.insert(QStringLiteral("query"), item.query);
    if (!item.options.isEmpty()) obj.insert(QStringLiteral("options"), item.options);

    if (!item.children.isEmpty()) {
        QJsonArray arr;
        for (const auto &child : item.children)
            arr.append(itemToJson(child));
        obj.insert(QStringLiteral("items"), arr);
    }

    for (auto it = item.unknownKeys.begin(); it != item.unknownKeys.end(); ++it)
        obj.insert(it.key(), it.value());

    return obj;
}

bool BookmarksStore::loadFromJson(const QJsonObject &obj)
{
    m_items.clear();
    const QJsonArray arr = obj.value(QStringLiteral("items")).toArray();
    for (const auto &v : arr)
        m_items.append(parseItem(v.toObject()));
    emit changed();
    return true;
}

QJsonObject BookmarksStore::toJson() const
{
    QJsonArray arr;
    for (const auto &item : m_items)
        arr.append(itemToJson(item));
    QJsonObject obj;
    obj.insert(QStringLiteral("items"), arr);
    return obj;
}

void BookmarksStore::addBookmark(BookmarkItem item, const QStringList &groupPath)
{
    if (groupPath.isEmpty()) {
        m_items.append(std::move(item));
    } else {
        if (auto *parent = find(groupPath))
            parent->children.append(std::move(item));
        else
            m_items.append(std::move(item));
    }
    emit changed();
}

bool BookmarksStore::removeBookmark(const QStringList &itemPath)
{
    if (itemPath.isEmpty()) return false;
    QList<BookmarkItem> *list = &m_items;
    for (int i = 0; i < itemPath.size() - 1; ++i) {
        bool ok = false;
        const int idx = itemPath.at(i).toInt(&ok);
        if (!ok || idx < 0 || idx >= list->size()) return false;
        list = &((*list)[idx].children);
    }
    bool ok = false;
    const int idx = itemPath.last().toInt(&ok);
    if (!ok || idx < 0 || idx >= list->size()) return false;
    list->removeAt(idx);
    emit changed();
    return true;
}

bool BookmarksStore::moveBookmark(const QStringList &fromPath,
                                  const QStringList &toParentPath,
                                  int insertIndex)
{
    BookmarkItem *src = find(fromPath);
    if (!src) return false;
    BookmarkItem copy = *src;

    // Remove inline without emitting (mirror of removeBookmark logic)
    if (fromPath.isEmpty()) return false;
    QList<BookmarkItem> *removeList = &m_items;
    for (int i = 0; i < fromPath.size() - 1; ++i) {
        bool ok = false;
        const int idx = fromPath.at(i).toInt(&ok);
        if (!ok || idx < 0 || idx >= removeList->size()) return false;
        removeList = &((*removeList)[idx].children);
    }
    bool ok = false;
    const int removeIdx = fromPath.last().toInt(&ok);
    if (!ok || removeIdx < 0 || removeIdx >= removeList->size()) return false;
    removeList->removeAt(removeIdx);

    // Insert into destination
    QList<BookmarkItem> *dest = &m_items;
    if (!toParentPath.isEmpty()) {
        BookmarkItem *parent = find(toParentPath);
        if (!parent) {
            m_items.append(std::move(copy));
            emit changed();
            return true;
        }
        dest = &parent->children;
    }
    if (insertIndex < 0 || insertIndex > dest->size()) insertIndex = dest->size();
    dest->insert(insertIndex, std::move(copy));

    // Emit changed() once at the end
    emit changed();
    return true;
}

BookmarkItem *BookmarksStore::find(const QStringList &itemPath)
{
    if (itemPath.isEmpty()) return nullptr;
    QList<BookmarkItem> *list = &m_items;
    BookmarkItem *current = nullptr;
    for (int i = 0; i < itemPath.size(); ++i) {
        bool ok = false;
        const int idx = itemPath.at(i).toInt(&ok);
        if (!ok || idx < 0 || idx >= list->size()) return nullptr;
        current = &(*list)[idx];
        list = &current->children;
    }
    return current;
}

} // namespace Corbomite::Bookmarks
