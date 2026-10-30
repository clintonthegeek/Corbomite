// SPDX-License-Identifier: GPL-3.0-or-later
#include "QuickSwitcherModel.h"

#include "corbomite/storage/proxies/MetadataCacheReader.h"

#include <QJsonArray>
#include <QJsonObject>

namespace Corbomite {

QuickSwitcherModel::QuickSwitcherModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int QuickSwitcherModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return static_cast<int>(m_entries.size());
}

QVariant QuickSwitcherModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_entries.size()) return {};

    const auto &entry = m_entries.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case NoteNameRole:
        return entry.name;
    case NotePathRole:
        return entry.relativePath;
    case FolderPathRole:
        return entry.folder;
    case IsRecentRole:
        return entry.isRecent;
    case AliasRole: {
        // AliasRole reads the cached frontmatter lazily; every role switch
        // past the first pays one hash-check for an already-read path.
        const QString path = entry.relativePath;
        if (!m_aliasCache.contains(path)) {
            m_aliasCache.insert(path,
                m_metadata ? [](const MetadataCacheReader *cache,
                                 const QString &p) {
                    const QJsonObject fm = cache->frontmatterFor(p);
                    QStringList out;
                    for (const char *key : {"aliases", "alias"}) {
                        const QJsonValue v = fm.value(QLatin1String(key));
                        if (v.isString()) {
                            out << v.toString();
                        } else if (v.isArray()) {
                            const QJsonArray arr = v.toArray();
                            for (const QJsonValue &e : arr)
                                if (e.isString()) out << e.toString();
                        }
                    }
                    out.removeAll(QString());
                    return out;
                }(m_metadata, path)
                           : QStringList{});
        }
        return m_aliasCache.value(path);
    }
    }
    return {};
}

void QuickSwitcherModel::setNotes(const QVector<NoteMeta> &notes)
{
    beginResetModel();
    m_entries.clear();
    m_entries.reserve(notes.size());

    for (const auto &meta : notes) {
        Entry entry;
        entry.relativePath = meta.relativePath;
        entry.name = meta.nameFromPath();

        int lastSlash = meta.relativePath.lastIndexOf(QLatin1Char('/'));
        entry.folder = lastSlash > 0 ? meta.relativePath.left(lastSlash) : QString();
        entry.isRecent = m_recentSet.contains(meta.relativePath);

        m_entries.append(entry);
    }
    endResetModel();
}

void QuickSwitcherModel::setMetadata(Corbomite::MetadataCacheReader *cache)
{
    // Aliases live in the cached frontmatter; reading them lazily per row
    // (AliasRole) means a keystroke's filter pass sees them as soon as the
    // cache settles. Clearing the memo re-reads from the new reader.
    m_metadata = cache;
    m_aliasCache.clear();
}

void QuickSwitcherModel::setRecentPaths(const QStringList &recentPaths)
{
    // A set for O(1) recent tests: the entries carry their own flag and
    // rebuild here, since setNotes runs after this on the cold path.
    m_recentSet = QSet<QString>(recentPaths.begin(), recentPaths.end());
    for (auto &entry : m_entries) {
        entry.isRecent = m_recentSet.contains(entry.relativePath);
    }
    // Data roles that read flags changed wholesale:
    if (!m_entries.isEmpty()) {
        emit dataChanged(index(0), index(m_entries.size() - 1),
                         QVector<int>{IsRecentRole});
    }
}

} // namespace Corbomite
