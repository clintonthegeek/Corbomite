// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>

namespace Corbomite::Bookmarks {

/// One entry in the bookmarks tree. Mirrors Obsidian's bookmarks.json item
/// shape (see docs/obsidian-audit/addenda/2026-04-19-bookmarks-core-plugin.md §2).
/// `type` is one of: "file", "folder", "search", "graph", "group".
/// Unknown types are preserved on round-trip via unknownKeys + unknownType.
struct BookmarkItem
{
    QString type;
    QString path;         ///< file/folder/heading/block (path#subpath for heading/block)
    QString subpath;      ///< "#Heading" or "#^blockId"; redundant with path suffix
    QString title;        ///< user override; empty = infer
    QString query;        ///< search only
    QJsonObject options;  ///< graph only
    qint64 ctime = 0;
    QList<BookmarkItem> children;  ///< group only
    QJsonObject unknownKeys;       ///< preserved round-trip surplus
};

} // namespace Corbomite::Bookmarks
