// tests/core/tst_workspace_deferred.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QTabBar>
#include "corbomite/core/Workspace.h"
#include "corbomite/core/WorkspaceSplit.h"
#include "corbomite/core/WorkspaceTabs.h"
#include "corbomite/core/WorkspaceLeaf.h"
#include "corbomite/core/ViewRegistry.h"

using namespace Corbomite;

class TestWorkspaceDeferred : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void deferredLeafHasNoView()
    {
        ViewRegistry registry;
        WorkspaceLeaf leaf(&registry);
        leaf.setDeferred(true, QStringLiteral("document"), QStringLiteral("Test"));

        QVERIFY(leaf.isDeferred());
        QVERIFY(leaf.view() == nullptr);
    }

    void deferredLeafCachedFields()
    {
        ViewRegistry registry;
        WorkspaceLeaf leaf(&registry);
        leaf.setDeferred(true, QStringLiteral("my-icon"), QStringLiteral("My Title"));

        QCOMPARE(leaf.cachedIcon(), QStringLiteral("my-icon"));
        QCOMPARE(leaf.cachedTitle(), QStringLiteral("My Title"));
    }

    void tabBarShowsCachedTitle()
    {
        ViewRegistry registry;
        WorkspaceTabs tabs;
        auto *leaf = new WorkspaceLeaf(&registry);
        leaf->setDeferred(true, QStringLiteral("doc"), QStringLiteral("Cached Title"));
        tabs.addChild(leaf);

        QCOMPARE(tabs.tabBar()->tabText(0), QStringLiteral("Cached Title"));
    }

    void loadIfDeferredClearsFlag()
    {
        ViewRegistry registry;
        WorkspaceLeaf leaf(&registry);
        leaf.setDeferred(true, QStringLiteral("d"), QStringLiteral("T"));
        QVERIFY(leaf.isDeferred());

        leaf.loadIfDeferred();
        QVERIFY(!leaf.isDeferred());
    }

    void nonDeferredLoadIfDeferredIsNoop()
    {
        ViewRegistry registry;
        WorkspaceLeaf leaf(&registry);
        leaf.loadIfDeferred(); // should not crash
        QVERIFY(!leaf.isDeferred());
    }
};

QTEST_MAIN(TestWorkspaceDeferred)
#include "tst_workspace_deferred.moc"
