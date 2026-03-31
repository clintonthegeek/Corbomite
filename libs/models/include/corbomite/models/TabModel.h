// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QAbstractListModel>
#include <QString>
#include <QVector>

namespace Corbomite {

struct TabState {
    QString notePath;
    int scrollPosition = 0;
    int cursorLine = 0;
    int cursorColumn = 0;
    bool isPinned = false;
    bool isDirty = false;
    quint64 lruCounter = 0;
};

class TabModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum Roles {
        NotePathRole = Qt::UserRole + 1,
        TitleRole,
        IsPinnedRole,
        IsDirtyRole
    };

    explicit TabModel(QObject *parent = nullptr);

    // QAbstractListModel interface
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Tab operations
    void openTab(const QString &notePath, bool activate = true);
    void closeTab(int index);
    void closeOtherTabs(int keepIndex);
    void closeAllTabs();
    void pinTab(int index, bool pinned);
    void moveTab(int fromIndex, int toIndex);
    void setActiveTab(int index);
    int activeTabIndex() const;

    // Query
    QString tabPath(int index) const;
    bool isPinned(int index) const;
    bool isDirty(int index) const;
    void setDirty(int index, bool dirty);

    // Rename support
    void updateNotePath(const QString &oldPath, const QString &newPath);

    // LRU navigation
    QStringList lruSortedPaths() const;

    // Closed tab history
    void reopenLastClosed();

Q_SIGNALS:
    void activeTabChanged(int index);

private:
    int findTab(const QString &notePath) const;

    QVector<TabState> m_tabs;
    int m_activeIndex = -1;
    quint64 m_lruCounter = 0;
    QVector<TabState> m_closedHistory;
};

} // namespace Corbomite
