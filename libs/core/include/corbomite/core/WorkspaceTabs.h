// libs/core/include/corbomite/core/WorkspaceTabs.h
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QPointer>
#include "corbomite/core/WorkspaceParent.h"

class QTabBar;
class QStackedWidget;
class QScrollArea;
class QVBoxLayout;

namespace Corbomite {

class WorkspaceLeaf;

class WorkspaceTabs : public WorkspaceParent
{
    Q_OBJECT
public:
    explicit WorkspaceTabs(QObject *parent = nullptr);
    ~WorkspaceTabs() override;

    QWidget *widget() override;
    QJsonObject serialize() const override;

    void addChild(WorkspaceItem *child, int index = -1) override;
    void removeChild(WorkspaceItem *child, bool deleteChild = false) override;

    QTabBar *tabBar() const;

    int currentTab() const;
    void setCurrentTab(int index);
    WorkspaceLeaf *currentLeaf() const;

    bool isStacked() const;
    void setStacked(bool stacked);

    void sortPinnedLeft();
    void updateTabHeader(int index);
    void updateAllTabHeaders();

    WorkspaceLeaf *leafAt(int index) const;

    /// Emit tabCloseRequested for the given index / range — wraps the
    /// signal so non-friend callers (e.g. View::onTabMenu default impl)
    /// can drive the existing host-wired close path.
    void requestCloseTab(int index);
    void requestCloseOthers(int keepIndex);
    void requestCloseToRight(int pivotIndex);
    void requestCloseAll();

Q_SIGNALS:
    void currentTabChanged(int index);
    void tabCloseRequested(int index);
    void splitRequested(Qt::Orientation direction);

private:
    void onTabBarCurrentChanged(int index);
    void onTabBarCloseRequested(int index);
    void rebuildTabBar();
    QString tabTextForLeaf(WorkspaceLeaf *leaf) const;
    QIcon tabIconForLeaf(WorkspaceLeaf *leaf) const;

    QPointer<QWidget> m_widget;
    QVBoxLayout *m_layout;
    QTabBar *m_tabBar;
    QStackedWidget *m_stack;
    QScrollArea *m_scrollArea;
    int m_currentTab = 0;
    bool m_stacked = false;
};

} // namespace Corbomite
