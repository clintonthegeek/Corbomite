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
    : WorkspaceParent(parent)
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
    connect(m_tabBar, &QTabBar::tabMoved,
            this, [this](int from, int to) {
        if (from >= 0 && from < m_children.size()
            && to >= 0 && to < m_children.size()) {
            m_children.move(from, to);
        }
    });
}

WorkspaceTabs::~WorkspaceTabs()
{
    delete m_widget;
}

QWidget *WorkspaceTabs::widget() { return m_widget; }

QTabBar *WorkspaceTabs::tabBar() const { return m_tabBar; }

int WorkspaceTabs::currentTab() const { return m_currentTab; }

void WorkspaceTabs::setCurrentTab(int index)
{
    if (index < 0 || index >= m_children.size())
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
    auto *item = childAt(index);
    return qobject_cast<WorkspaceLeaf *>(item);
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
        for (auto *child : m_children) {
            if (auto *w = child->widget())
                vbox->addWidget(w, 1);
        }
        m_scrollArea->setWidget(container);
    } else {
        m_scrollArea->takeWidget();
        for (int i = 0; i < m_children.size(); ++i) {
            if (auto *w = m_children[i]->widget())
                m_stack->insertWidget(i, w);
        }
        m_stack->setCurrentIndex(m_currentTab);
    }
}

void WorkspaceTabs::addChild(WorkspaceItem *child, int index)
{
    WorkspaceParent::addChild(child, index);

    auto *leaf = qobject_cast<WorkspaceLeaf *>(child);
    if (!leaf)
        return;

    int idx = m_children.indexOf(child);
    m_tabBar->insertTab(idx, tabIconForLeaf(leaf), tabTextForLeaf(leaf));

    connect(leaf, &WorkspaceLeaf::viewChanged, this, [this, child]() {
        int i = m_children.indexOf(child);
        if (i >= 0) updateTabHeader(i);
    });

    if (auto *w = leaf->widget()) {
        if (m_stacked) {
            if (auto *container = m_scrollArea->widget()) {
                if (auto *vbox = container->layout())
                    vbox->addWidget(w);
            }
        } else {
            m_stack->insertWidget(idx, w);
        }
    }

    if (m_children.size() == 1)
        setCurrentTab(0);
}

void WorkspaceTabs::removeChild(WorkspaceItem *child, bool deleteChild)
{
    int idx = m_children.indexOf(child);
    if (idx < 0)
        return;

    if (auto *w = child->widget())
        w->setParent(nullptr);

    m_tabBar->removeTab(idx);
    WorkspaceParent::removeChild(child, deleteChild);

    if (m_currentTab >= m_children.size())
        m_currentTab = qMax(0, m_children.size() - 1);
    if (!m_children.isEmpty())
        setCurrentTab(m_currentTab);
}

void WorkspaceTabs::sortPinnedLeft()
{
    std::stable_partition(m_children.begin(), m_children.end(),
        [](WorkspaceItem *item) {
            auto *leaf = qobject_cast<WorkspaceLeaf *>(item);
            return leaf && leaf->pinned();
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
    for (int i = 0; i < m_children.size(); ++i)
        updateTabHeader(i);
}

void WorkspaceTabs::onTabBarCurrentChanged(int index)
{
    if (index < 0 || index >= m_children.size())
        return;
    m_currentTab = index;
    m_stack->setCurrentIndex(index);
    Q_EMIT currentTabChanged(index);

    // Load a deferred leaf the first time the user switches to it.
    if (auto *leaf = leafAt(index)) {
        if (leaf->isDeferred())
            leaf->loadIfDeferred();
    }
}

void WorkspaceTabs::onTabBarCloseRequested(int index)
{
    Q_EMIT tabCloseRequested(index);
}

void WorkspaceTabs::rebuildTabBar()
{
    while (m_tabBar->count() > 0)
        m_tabBar->removeTab(0);

    for (auto *child : m_children) {
        auto *leaf = qobject_cast<WorkspaceLeaf *>(child);
        if (leaf)
            m_tabBar->addTab(tabIconForLeaf(leaf), tabTextForLeaf(leaf));
    }

    if (!m_stacked) {
        while (m_stack->count() > 0)
            m_stack->removeWidget(m_stack->widget(0));
        for (auto *child : m_children) {
            if (auto *w = child->widget())
                m_stack->addWidget(w);
        }
    }

    if (m_currentTab < m_children.size())
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
    for (const auto *child : m_children)
        children.append(child->serialize());
    json[QStringLiteral("children")] = children;

    return json;
}

} // namespace Corbomite
