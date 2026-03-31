// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/models/TabModel.h"
#include <algorithm>

namespace Corbomite {

TabModel::TabModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int TabModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return static_cast<int>(m_tabs.size());
}

QVariant TabModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_tabs.size()) return {};

    const auto &tab = m_tabs.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case TitleRole: {
        QString name = tab.notePath.mid(tab.notePath.lastIndexOf(QLatin1Char('/')) + 1);
        int dot = name.lastIndexOf(QLatin1Char('.'));
        return dot > 0 ? name.left(dot) : name;
    }
    case NotePathRole: return tab.notePath;
    case IsPinnedRole: return tab.isPinned;
    case IsDirtyRole: return tab.isDirty;
    }
    return {};
}

QHash<int, QByteArray> TabModel::roleNames() const
{
    return {
        {NotePathRole, "notePath"},
        {TitleRole, "title"},
        {IsPinnedRole, "isPinned"},
        {IsDirtyRole, "isDirty"}
    };
}

void TabModel::openTab(const QString &notePath, bool activate)
{
    int existing = findTab(notePath);
    if (existing >= 0) {
        if (activate) setActiveTab(existing);
        return;
    }

    int row = m_tabs.size();
    beginInsertRows(QModelIndex(), row, row);
    TabState tab;
    tab.notePath = notePath;
    tab.lruCounter = ++m_lruCounter;
    m_tabs.append(tab);
    endInsertRows();

    if (activate) setActiveTab(row);
}

void TabModel::closeTab(int index)
{
    if (index < 0 || index >= m_tabs.size()) return;

    m_closedHistory.append(m_tabs.at(index));

    beginRemoveRows(QModelIndex(), index, index);
    m_tabs.removeAt(index);
    endRemoveRows();

    if (m_tabs.isEmpty()) {
        m_activeIndex = -1;
    } else if (m_activeIndex >= m_tabs.size()) {
        m_activeIndex = m_tabs.size() - 1;
    } else if (m_activeIndex > index) {
        --m_activeIndex;
    }
    Q_EMIT activeTabChanged(m_activeIndex);
}

void TabModel::closeOtherTabs(int keepIndex)
{
    // Close from end to avoid index shifting issues
    for (int i = m_tabs.size() - 1; i >= 0; --i) {
        if (i == keepIndex) continue;
        if (m_tabs.at(i).isPinned) continue;
        closeTab(i);
        if (keepIndex > i) --keepIndex;
    }
}

void TabModel::closeAllTabs()
{
    beginResetModel();
    m_tabs.clear();
    m_activeIndex = -1;
    endResetModel();
    Q_EMIT activeTabChanged(-1);
}

void TabModel::pinTab(int index, bool pinned)
{
    if (index < 0 || index >= m_tabs.size()) return;
    m_tabs[index].isPinned = pinned;
    Q_EMIT dataChanged(this->index(index), this->index(index), {IsPinnedRole});
}

void TabModel::moveTab(int fromIndex, int toIndex)
{
    if (fromIndex < 0 || fromIndex >= m_tabs.size()) return;
    if (toIndex < 0 || toIndex >= m_tabs.size()) return;
    if (fromIndex == toIndex) return;

    // beginMoveRows needs special handling
    int destRow = toIndex > fromIndex ? toIndex + 1 : toIndex;
    beginMoveRows(QModelIndex(), fromIndex, fromIndex, QModelIndex(), destRow);

    TabState tab = m_tabs.takeAt(fromIndex);
    m_tabs.insert(toIndex, tab);

    endMoveRows();

    // Update active index
    if (m_activeIndex == fromIndex) {
        m_activeIndex = toIndex;
    } else if (fromIndex < m_activeIndex && toIndex >= m_activeIndex) {
        --m_activeIndex;
    } else if (fromIndex > m_activeIndex && toIndex <= m_activeIndex) {
        ++m_activeIndex;
    }
}

void TabModel::setActiveTab(int index)
{
    if (index < 0 || index >= m_tabs.size()) return;
    m_activeIndex = index;
    m_tabs[index].lruCounter = ++m_lruCounter;
    Q_EMIT activeTabChanged(index);
}

int TabModel::activeTabIndex() const
{
    return m_activeIndex;
}

QString TabModel::tabPath(int index) const
{
    if (index < 0 || index >= m_tabs.size()) return {};
    return m_tabs.at(index).notePath;
}

bool TabModel::isPinned(int index) const
{
    if (index < 0 || index >= m_tabs.size()) return false;
    return m_tabs.at(index).isPinned;
}

bool TabModel::isDirty(int index) const
{
    if (index < 0 || index >= m_tabs.size()) return false;
    return m_tabs.at(index).isDirty;
}

void TabModel::setDirty(int index, bool dirty)
{
    if (index < 0 || index >= m_tabs.size()) return;
    m_tabs[index].isDirty = dirty;
    Q_EMIT dataChanged(this->index(index), this->index(index), {IsDirtyRole});
}

void TabModel::updateNotePath(const QString &oldPath, const QString &newPath)
{
    int idx = findTab(oldPath);
    if (idx < 0) return;
    m_tabs[idx].notePath = newPath;
    Q_EMIT dataChanged(index(idx), index(idx), {NotePathRole, TitleRole});
}

QStringList TabModel::lruSortedPaths() const
{
    auto sorted = m_tabs;
    std::sort(sorted.begin(), sorted.end(), [](const TabState &a, const TabState &b) {
        return a.lruCounter > b.lruCounter; // Most recent first
    });

    QStringList result;
    result.reserve(sorted.size());
    for (const auto &tab : sorted) {
        result.append(tab.notePath);
    }
    return result;
}

void TabModel::reopenLastClosed()
{
    if (m_closedHistory.isEmpty()) return;
    TabState tab = m_closedHistory.takeLast();
    openTab(tab.notePath);
}

int TabModel::findTab(const QString &notePath) const
{
    for (int i = 0; i < m_tabs.size(); ++i) {
        if (m_tabs.at(i).notePath == notePath) return i;
    }
    return -1;
}

} // namespace Corbomite
