// libs/core/src/WorkspaceTabs.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/WorkspaceTabs.h"
#include "corbomite/core/WorkspaceLeaf.h"
#include "corbomite/core/View.h"

#include <QIcon>
#include <QJsonArray>
#include <QScrollArea>
#include <QStackedWidget>
#include <QTabBar>
#include <QVBoxLayout>

namespace Corbomite {

WorkspaceTabs::WorkspaceTabs(QObject *parent)
    : WorkspaceItem(parent)
    , m_widget(new QWidget)
    , m_layout(new QVBoxLayout(m_widget))
    , m_tabBar(new QTabBar(m_widget))
    , m_stack(new QStackedWidget(m_widget))
    , m_scrollArea(new QScrollArea(m_widget))
{
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(0);

    m_tabBar->setTabsClosable(true);
    m_tabBar->setMovable(true);
    m_tabBar->setExpanding(false);
    m_tabBar->setElideMode(Qt::ElideRight);

    m_layout->addWidget(m_tabBar);
    m_layout->addWidget(m_stack, 1);

    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setVisible(false);
    m_layout->addWidget(m_scrollArea);

    connect(m_tabBar, &QTabBar::currentChanged,
            this, &WorkspaceTabs::onTabBarCurrentChanged);
    connect(m_tabBar, &QTabBar::tabCloseRequested,
            this, &WorkspaceTabs::onTabBarCloseRequested);
}

QWidget *WorkspaceTabs::widget() { return m_widget; }

QTabBar *WorkspaceTabs::tabBar() const { return m_tabBar; }

int WorkspaceTabs::childCount() const { return m_leaves.size(); }

WorkspaceLeaf *WorkspaceTabs::childAt(int index) const
{
    if (index < 0 || index >= m_leaves.size())
        return nullptr;
    return m_leaves.at(index);
}

int WorkspaceTabs::currentTab() const { return m_currentTab; }

void WorkspaceTabs::setCurrentTab(int index)
{
    if (index < 0 || index >= m_leaves.size())
        return;
    m_currentTab = index;
    m_tabBar->setCurrentIndex(index);
    m_stack->setCurrentIndex(index);
}

WorkspaceLeaf *WorkspaceTabs::currentLeaf() const
{
    return leafAt(m_currentTab);
}

WorkspaceLeaf *WorkspaceTabs::leafAt(int index) const
{
    return childAt(index);
}

bool WorkspaceTabs::isStacked() const { return m_stacked; }

void WorkspaceTabs::setStacked(bool stacked)
{
    if (m_stacked == stacked)
        return;
    m_stacked = stacked;

    m_tabBar->setVisible(!stacked);
    m_stack->setVisible(!stacked);
    m_scrollArea->setVisible(stacked);

    if (stacked) {
        auto *container = new QWidget;
        auto *vbox = new QVBoxLayout(container);
        vbox->setContentsMargins(0, 0, 0, 0);
        for (auto *leaf : m_leaves)
            vbox->addWidget(leaf, 1);
        m_scrollArea->setWidget(container);
    } else {
        m_scrollArea->takeWidget();
        for (int i = 0; i < m_leaves.size(); ++i)
            m_stack->insertWidget(i, m_leaves[i]);
        m_stack->setCurrentIndex(m_currentTab);
    }
}

void WorkspaceTabs::addChild(WorkspaceLeaf *child, int index)
{
    if (!child || m_leaves.contains(child))
        return;

    if (index < 0 || index >= m_leaves.size())
        m_leaves.append(child);
    else
        m_leaves.insert(index, child);

    int idx = m_leaves.indexOf(child);
    m_tabBar->insertTab(idx, tabIconForLeaf(child), tabTextForLeaf(child));

    if (m_stacked) {
        if (auto *container = m_scrollArea->widget()) {
            if (auto *vbox = container->layout())
                vbox->addWidget(child);
        }
    } else {
        m_stack->insertWidget(idx, child);
    }

    if (m_leaves.size() == 1)
        setCurrentTab(0);
}

void WorkspaceTabs::removeChild(WorkspaceLeaf *child, bool deleteChild)
{
    int idx = m_leaves.indexOf(child);
    if (idx < 0)
        return;

    // Reparent the leaf widget out of the stack/scroll area
    child->setParent(nullptr);

    m_tabBar->removeTab(idx);
    m_leaves.removeAt(idx);

    if (m_currentTab >= m_leaves.size())
        m_currentTab = qMax(0, m_leaves.size() - 1);
    if (!m_leaves.isEmpty())
        setCurrentTab(m_currentTab);

    if (deleteChild)
        delete child;
}

void WorkspaceTabs::sortPinnedLeft()
{
    std::stable_partition(m_leaves.begin(), m_leaves.end(),
        [](WorkspaceLeaf *leaf) {
            return leaf->pinned();
        });
    rebuildTabBar();
}

void WorkspaceTabs::updateTabHeader(int index)
{
    auto *leaf = leafAt(index);
    if (!leaf)
        return;
    m_tabBar->setTabText(index, tabTextForLeaf(leaf));
    m_tabBar->setTabIcon(index, tabIconForLeaf(leaf));
}

void WorkspaceTabs::updateAllTabHeaders()
{
    for (int i = 0; i < m_leaves.size(); ++i)
        updateTabHeader(i);
}

void WorkspaceTabs::onTabBarCurrentChanged(int index)
{
    if (index < 0 || index >= m_leaves.size())
        return;
    m_currentTab = index;
    m_stack->setCurrentIndex(index);
    Q_EMIT currentTabChanged(index);
}

void WorkspaceTabs::onTabBarCloseRequested(int index)
{
    Q_EMIT tabCloseRequested(index);
}

void WorkspaceTabs::rebuildTabBar()
{
    while (m_tabBar->count() > 0)
        m_tabBar->removeTab(0);

    for (auto *leaf : m_leaves)
        m_tabBar->addTab(tabIconForLeaf(leaf), tabTextForLeaf(leaf));

    if (!m_stacked) {
        while (m_stack->count() > 0)
            m_stack->removeWidget(m_stack->widget(0));
        for (auto *leaf : m_leaves)
            m_stack->addWidget(leaf);
    }

    if (m_currentTab < m_leaves.size())
        setCurrentTab(m_currentTab);
}

QString WorkspaceTabs::tabTextForLeaf(WorkspaceLeaf *leaf) const
{
    if (leaf->isDeferred())
        return leaf->cachedTitle();
    if (auto *v = leaf->view())
        return v->getDisplayText();
    return {};
}

QIcon WorkspaceTabs::tabIconForLeaf(WorkspaceLeaf *leaf) const
{
    QString iconName;
    if (leaf->isDeferred())
        iconName = leaf->cachedIcon();
    else if (auto *v = leaf->view())
        iconName = v->getIcon();

    if (iconName.isEmpty())
        return {};
    return QIcon::fromTheme(iconName);
}

QJsonObject WorkspaceTabs::serialize() const
{
    QJsonObject json;
    json[QStringLiteral("id")] = id();
    json[QStringLiteral("type")] = QStringLiteral("tabs");
    json[QStringLiteral("currentTab")] = m_currentTab;

    if (dimension().has_value())
        json[QStringLiteral("dimension")] = dimension().value();
    if (m_stacked)
        json[QStringLiteral("stacked")] = true;

    QJsonArray children;
    for (const auto *leaf : m_leaves)
        children.append(leaf->serialize());
    json[QStringLiteral("children")] = children;

    return json;
}

} // namespace Corbomite
