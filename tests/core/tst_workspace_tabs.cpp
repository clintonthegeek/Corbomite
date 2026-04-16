// tests/core/tst_workspace_tabs.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QJsonArray>
#include <QTabBar>
#include <QSignalSpy>
#include "corbomite/core/WorkspaceTabs.h"
#include "corbomite/core/WorkspaceLeaf.h"
#include "corbomite/core/ViewRegistry.h"

using Corbomite::WorkspaceTabs;
using Corbomite::WorkspaceLeaf;
using Corbomite::ViewRegistry;

class TestWorkspaceTabs : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void addLeafCreatesTab()
    {
        ViewRegistry registry;
        WorkspaceTabs tabs;
        auto *leaf = new WorkspaceLeaf(&registry);
        tabs.addChild(leaf);

        QCOMPARE(tabs.childCount(), 1);
        QCOMPARE(tabs.tabBar()->count(), 1);
    }

    void removeLeafRemovesTab()
    {
        ViewRegistry registry;
        WorkspaceTabs tabs;
        auto *leaf = new WorkspaceLeaf(&registry);
        tabs.addChild(leaf);
        tabs.removeChild(leaf, true);

        QCOMPARE(tabs.childCount(), 0);
        QCOMPARE(tabs.tabBar()->count(), 0);
    }

    void currentTabTracking()
    {
        ViewRegistry registry;
        WorkspaceTabs tabs;
        auto *a = new WorkspaceLeaf(&registry);
        auto *b = new WorkspaceLeaf(&registry);
        tabs.addChild(a);
        tabs.addChild(b);

        tabs.setCurrentTab(1);
        QCOMPARE(tabs.currentTab(), 1);
        QCOMPARE(tabs.currentLeaf(), b);
    }

    void tabHeaderUsesCachedTitle()
    {
        ViewRegistry registry;
        WorkspaceTabs tabs;
        auto *leaf = new WorkspaceLeaf(&registry);
        leaf->setDeferred(true, QStringLiteral("document"), QStringLiteral("My Note"));
        tabs.addChild(leaf);

        QCOMPARE(tabs.tabBar()->tabText(0), QStringLiteral("My Note"));
    }

    void stackedModeDefault()
    {
        WorkspaceTabs tabs;
        QVERIFY(!tabs.isStacked());
    }

    void setStacked()
    {
        WorkspaceTabs tabs;
        tabs.setStacked(true);
        QVERIFY(tabs.isStacked());
    }

    void serializeRoundTrip()
    {
        ViewRegistry registry;
        WorkspaceTabs tabs;
        auto *leaf = new WorkspaceLeaf(&registry);
        tabs.addChild(leaf);
        tabs.setStacked(true);

        QJsonObject json = tabs.serialize();
        QCOMPARE(json[QStringLiteral("type")].toString(), QStringLiteral("tabs"));
        QCOMPARE(json[QStringLiteral("stacked")].toBool(), true);
        QCOMPARE(json[QStringLiteral("currentTab")].toInt(), 0);
        QVERIFY(json[QStringLiteral("children")].toArray().size() == 1);
    }

    void pinnedTabsSortLeft()
    {
        ViewRegistry registry;
        WorkspaceTabs tabs;
        auto *a = new WorkspaceLeaf(&registry);
        auto *b = new WorkspaceLeaf(&registry);
        auto *c = new WorkspaceLeaf(&registry);
        a->setDeferred(true, QStringLiteral("d"), QStringLiteral("A"));
        b->setDeferred(true, QStringLiteral("d"), QStringLiteral("B"));
        c->setDeferred(true, QStringLiteral("d"), QStringLiteral("C"));
        tabs.addChild(a);
        tabs.addChild(b);
        tabs.addChild(c);

        b->setPinned(true);
        tabs.sortPinnedLeft();

        QCOMPARE(tabs.childAt(0), b);
    }
};

QTEST_MAIN(TestWorkspaceTabs)
#include "tst_workspace_tabs.moc"
