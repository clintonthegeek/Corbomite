// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/models/SearchResultsModel.h"

namespace Corbomite {

SearchResultsModel::SearchResultsModel(QObject *parent)
    : QAbstractItemModel(parent)
{
}

QModelIndex SearchResultsModel::index(int row, int column, const QModelIndex &parent) const
{
    if (column != 0) return {};

    if (!parent.isValid()) {
        // Top level: file groups
        if (row >= 0 && row < m_groups.size()) {
            return createIndex(row, 0, quintptr(-1));
        }
    } else if (parent.internalId() == quintptr(-1)) {
        // Child level: matches within a file group
        int groupIdx = parent.row();
        if (groupIdx >= 0 && groupIdx < m_groups.size()
            && row >= 0 && row < m_groups[groupIdx].matches.size()) {
            return createIndex(row, 0, quintptr(groupIdx));
        }
    }
    return {};
}

QModelIndex SearchResultsModel::parent(const QModelIndex &child) const
{
    if (!child.isValid()) return {};
    quintptr id = child.internalId();
    if (id == quintptr(-1)) return {}; // Top-level item
    return createIndex(int(id), 0, quintptr(-1)); // Parent is the file group
}

int SearchResultsModel::rowCount(const QModelIndex &parent) const
{
    if (!parent.isValid()) {
        return m_groups.size();
    }
    if (parent.internalId() == quintptr(-1)) {
        int groupIdx = parent.row();
        if (groupIdx >= 0 && groupIdx < m_groups.size()) {
            return m_groups[groupIdx].matches.size();
        }
    }
    return 0;
}

int SearchResultsModel::columnCount(const QModelIndex &) const
{
    return 1;
}

QVariant SearchResultsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) return {};

    if (index.internalId() == quintptr(-1)) {
        // File group row
        int groupIdx = index.row();
        if (groupIdx < 0 || groupIdx >= m_groups.size()) return {};
        const auto &group = m_groups[groupIdx];

        switch (role) {
        case Qt::DisplayRole:
            return QStringLiteral("%1 (%2)").arg(group.noteName).arg(group.matches.size());
        case NotePathRole:
            return group.notePath;
        case MatchCountRole:
            return group.matches.size();
        }
    } else {
        // Match row within a file
        int groupIdx = int(index.internalId());
        if (groupIdx < 0 || groupIdx >= m_groups.size()) return {};
        const auto &matches = m_groups[groupIdx].matches;
        int matchIdx = index.row();
        if (matchIdx < 0 || matchIdx >= matches.size()) return {};
        const auto &match = matches[matchIdx];

        switch (role) {
        case Qt::DisplayRole:
        case SnippetRole:
            return match.snippet;
        case NotePathRole:
            return match.notePath;
        case MatchRangesRole:
            return QVariant::fromValue(match.matches);
        }
    }
    return {};
}

void SearchResultsModel::setResults(const QVector<SearchMatch> &results)
{
    beginResetModel();
    m_groups.clear();

    // Group by file path
    QHash<QString, int> pathToGroup;
    for (const auto &match : results) {
        auto it = pathToGroup.find(match.notePath);
        if (it == pathToGroup.end()) {
            FileGroup group;
            group.notePath = match.notePath;
            // Extract name from path
            QString name = match.notePath.mid(match.notePath.lastIndexOf(QLatin1Char('/')) + 1);
            int dot = name.lastIndexOf(QLatin1Char('.'));
            group.noteName = dot > 0 ? name.left(dot) : name;
            group.matches.append(match);
            pathToGroup[match.notePath] = m_groups.size();
            m_groups.append(group);
        } else {
            m_groups[it.value()].matches.append(match);
        }
    }
    endResetModel();
}

void SearchResultsModel::clear()
{
    beginResetModel();
    m_groups.clear();
    endResetModel();
}

int SearchResultsModel::fileCount() const
{
    return m_groups.size();
}

int SearchResultsModel::totalMatchCount() const
{
    int total = 0;
    for (const auto &g : m_groups) total += g.matches.size();
    return total;
}

} // namespace Corbomite
