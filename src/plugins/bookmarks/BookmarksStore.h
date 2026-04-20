// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "BookmarkItem.h"

#include <QJsonObject>
#include <QObject>
#include <QStringList>

namespace Corbomite { class Vault; }

namespace Corbomite::Bookmarks {

/// In-memory model of bookmarks.json. Load/save is stateless on the store
/// (plugin shell drives debounced writes). `itemPath` is a QStringList of
/// integer-as-string indices walking the tree (e.g. ["0","2"] = root->0th
/// child->2nd child).
class BookmarksStore : public QObject
{
    Q_OBJECT
public:
    explicit BookmarksStore(QObject *parent = nullptr);
    ~BookmarksStore() override;

    bool loadFromJson(const QJsonObject &obj);
    QJsonObject toJson() const;

    const QList<BookmarkItem> &rootItems() const { return m_items; }

    void addBookmark(BookmarkItem item, const QStringList &groupPath = {});
    bool removeBookmark(const QStringList &itemPath);
    bool moveBookmark(const QStringList &fromPath,
                      const QStringList &toParentPath,
                      int insertIndex);

    /// Resolve a tree path to a pointer into m_items. Nullptr if invalid.
    BookmarkItem *find(const QStringList &itemPath);

    /// Rewrite every bookmark whose `path` starts with `oldPath` to the
    /// corresponding `newPath`. Preserves any `#subpath` suffix. Returns the
    /// number of bookmarks touched (recursively). Emits `changed()` iff > 0.
    int renamePath(const QString &oldPath, const QString &newPath);

    /// Mark every bookmark whose `path` matches `path` (or whose prefix up to
    /// the `#` subpath matches) as orphaned by setting
    /// `unknownKeys["_orphaned"] = true`. Preserved through round-trip so the
    /// user can visually distinguish dead links. Returns the number of
    /// bookmarks touched. Emits `changed()` iff > 0.
    int markOrphaned(const QString &path);

    /// Update a bookmark's user-supplied title. Emits changed().
    bool setTitle(const QStringList &itemPath, const QString &title);

signals:
    void changed();

private:
    QList<BookmarkItem> m_items;

    static BookmarkItem parseItem(const QJsonObject &obj);
    static QJsonObject  itemToJson(const BookmarkItem &item);
};

} // namespace Corbomite::Bookmarks
