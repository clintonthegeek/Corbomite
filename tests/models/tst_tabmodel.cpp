// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QSignalSpy>
#include "corbomite/models/TabModel.h"

class TestTabModel : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void testInitiallyEmpty()
    {
        Corbomite::TabModel model;
        QCOMPARE(model.rowCount(), 0);
        QCOMPARE(model.activeTabIndex(), -1);
    }

    void testOpenTabAddsRow()
    {
        Corbomite::TabModel model;
        model.openTab(QStringLiteral("note.md"));

        QCOMPARE(model.rowCount(), 1);
        QCOMPARE(model.activeTabIndex(), 0);
    }

    void testOpenDuplicateActivatesExisting()
    {
        Corbomite::TabModel model;
        model.openTab(QStringLiteral("note.md"));
        model.openTab(QStringLiteral("other.md"));
        model.openTab(QStringLiteral("note.md")); // duplicate

        QCOMPARE(model.rowCount(), 2); // not 3
        QCOMPARE(model.activeTabIndex(), 0); // back to first
    }

    void testCloseTab()
    {
        Corbomite::TabModel model;
        model.openTab(QStringLiteral("a.md"));
        model.openTab(QStringLiteral("b.md"));

        model.closeTab(0);

        QCOMPARE(model.rowCount(), 1);
        QCOMPARE(model.tabPath(0), QStringLiteral("b.md"));
    }

    void testCloseOtherTabs()
    {
        Corbomite::TabModel model;
        model.openTab(QStringLiteral("a.md"));
        model.openTab(QStringLiteral("b.md"));
        model.openTab(QStringLiteral("c.md"));

        model.setActiveTab(1); // b.md active
        model.closeOtherTabs(1);

        QCOMPARE(model.rowCount(), 1);
        QCOMPARE(model.tabPath(0), QStringLiteral("b.md"));
    }

    void testCloseOtherKeepsPinned()
    {
        Corbomite::TabModel model;
        model.openTab(QStringLiteral("a.md"));
        model.openTab(QStringLiteral("b.md"));
        model.openTab(QStringLiteral("c.md"));
        model.pinTab(0, true); // pin a.md

        model.setActiveTab(1);
        model.closeOtherTabs(1);

        QCOMPARE(model.rowCount(), 2); // a.md (pinned) + b.md (active)
    }

    void testReopenLastClosed()
    {
        Corbomite::TabModel model;
        model.openTab(QStringLiteral("a.md"));
        model.openTab(QStringLiteral("b.md"));

        model.closeTab(1); // close b.md
        QCOMPARE(model.rowCount(), 1);

        model.reopenLastClosed();
        QCOMPARE(model.rowCount(), 2);
        QCOMPARE(model.tabPath(1), QStringLiteral("b.md"));
    }

    void testLruOrdering()
    {
        Corbomite::TabModel model;
        model.openTab(QStringLiteral("a.md")); // lru=1
        model.openTab(QStringLiteral("b.md")); // lru=2
        model.openTab(QStringLiteral("c.md")); // lru=3

        // Activate a.md → a becomes most recent
        model.setActiveTab(0); // a.md lru=4

        auto lru = model.lruSortedPaths();
        // Most recent first: a, c, b
        QCOMPARE(lru.at(0), QStringLiteral("a.md"));
        QCOMPARE(lru.at(1), QStringLiteral("c.md"));
        QCOMPARE(lru.at(2), QStringLiteral("b.md"));
    }

    void testPinTab()
    {
        Corbomite::TabModel model;
        model.openTab(QStringLiteral("a.md"));

        QVERIFY(!model.isPinned(0));
        model.pinTab(0, true);
        QVERIFY(model.isPinned(0));
    }

    void testDirtyState()
    {
        Corbomite::TabModel model;
        model.openTab(QStringLiteral("a.md"));

        QVERIFY(!model.isDirty(0));
        model.setDirty(0, true);
        QVERIFY(model.isDirty(0));
    }

    void testMoveTab()
    {
        Corbomite::TabModel model;
        model.openTab(QStringLiteral("a.md"));
        model.openTab(QStringLiteral("b.md"));
        model.openTab(QStringLiteral("c.md"));

        model.moveTab(0, 2); // move a.md to position 2

        QCOMPARE(model.tabPath(0), QStringLiteral("b.md"));
        QCOMPARE(model.tabPath(1), QStringLiteral("c.md"));
        QCOMPARE(model.tabPath(2), QStringLiteral("a.md"));
    }

    void testUpdateNotePath()
    {
        Corbomite::TabModel model;
        model.openTab(QStringLiteral("old.md"));

        model.updateNotePath(QStringLiteral("old.md"), QStringLiteral("new.md"));

        QCOMPARE(model.tabPath(0), QStringLiteral("new.md"));
    }

    void testCloseAllTabs()
    {
        Corbomite::TabModel model;
        model.openTab(QStringLiteral("a.md"));
        model.openTab(QStringLiteral("b.md"));

        model.closeAllTabs();

        QCOMPARE(model.rowCount(), 0);
        QCOMPARE(model.activeTabIndex(), -1);
    }
};

QTEST_MAIN(TestTabModel)
#include "tst_tabmodel.moc"
