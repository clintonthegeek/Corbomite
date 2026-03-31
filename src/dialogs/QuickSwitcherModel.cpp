// SPDX-License-Identifier: GPL-3.0-or-later
#include "QuickSwitcherModel.h"

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

void QuickSwitcherModel::setRecentPaths(const QStringList &recentPaths)
{
    m_recentSet = QSet<QString>(recentPaths.begin(), recentPaths.end());

    // Update isRecent flags on existing entries
    for (auto &entry : m_entries) {
        entry.isRecent = m_recentSet.contains(entry.relativePath);
    }

    if (!m_entries.isEmpty()) {
        Q_EMIT dataChanged(index(0), index(m_entries.size() - 1), {IsRecentRole});
    }
}

} // namespace Corbomite
