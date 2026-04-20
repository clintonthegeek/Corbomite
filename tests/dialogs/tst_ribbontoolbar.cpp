// SPDX-License-Identifier: GPL-3.0-or-later
#include <QIcon>
#include <QSignalSpy>
#include <QTest>

#include "app/RibbonToolBar.h"

class TestRibbonToolBar : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void addReturnsIdWhenSuccessful()
    {
        Corbomite::RibbonToolBar bar;
        auto h = bar.addRibbonIcon(QStringLiteral("core:quick_switcher"),
                                   QIcon(),
                                   QStringLiteral("Open quick switcher"),
                                   []() {});
        QCOMPARE(h, QStringLiteral("core:quick_switcher"));
        QVERIFY(bar.hasIcon(h));
        QCOMPARE(bar.iconCount(), 1);
    }

    void emptyIdRejected()
    {
        Corbomite::RibbonToolBar bar;
        auto h = bar.addRibbonIcon(QString(), QIcon(),
                                   QStringLiteral("anything"), []() {});
        QVERIFY(h.isEmpty());
        QCOMPARE(bar.iconCount(), 0);
    }

    void duplicateIdSilentlyDropped()
    {
        Corbomite::RibbonToolBar bar;
        auto first = bar.addRibbonIcon(QStringLiteral("plugin-a:Open"),
                                       QIcon(), QStringLiteral("Open"), []() {});
        auto second = bar.addRibbonIcon(QStringLiteral("plugin-a:Open"),
                                        QIcon(), QStringLiteral("Open"), []() {});
        QVERIFY(!first.isEmpty());
        QVERIFY(second.isEmpty());
        QCOMPARE(bar.iconCount(), 1);
    }

    void removeIcon()
    {
        Corbomite::RibbonToolBar bar;
        auto h = bar.addRibbonIcon(QStringLiteral("core:switcher"), QIcon(),
                                   QStringLiteral("Switcher"), []() {});
        QVERIFY(bar.removeRibbonIcon(h));
        QVERIFY(!bar.hasIcon(h));
        QCOMPARE(bar.iconCount(), 0);
    }

    void removeUnknownReturnsFalse()
    {
        Corbomite::RibbonToolBar bar;
        QVERIFY(!bar.removeRibbonIcon(QStringLiteral("never-added")));
    }

    void callbackFiresOnTrigger()
    {
        Corbomite::RibbonToolBar bar;
        int calls = 0;
        auto h = bar.addRibbonIcon(QStringLiteral("core:counter"), QIcon(),
                                   QStringLiteral("Counter"),
                                   [&calls]() { ++calls; });
        QAction *act = bar.actionForId(h);
        QVERIFY(act);
        act->trigger();
        QCOMPARE(calls, 1);
    }

    void visibilityRoundTrips()
    {
        Corbomite::RibbonToolBar bar;
        auto h = bar.addRibbonIcon(QStringLiteral("core:graph"), QIcon(),
                                   QStringLiteral("Graph"), []() {});
        QVERIFY(bar.isIconVisible(h));
        bar.setIconVisible(h, false);
        QVERIFY(!bar.isIconVisible(h));
        bar.setIconVisible(h, true);
        QVERIFY(bar.isIconVisible(h));
    }

    void iconIdsInOrderReflectsInsertion()
    {
        Corbomite::RibbonToolBar bar;
        bar.addRibbonIcon(QStringLiteral("a"), QIcon(), QStringLiteral("A"), []() {});
        bar.addRibbonIcon(QStringLiteral("b"), QIcon(), QStringLiteral("B"), []() {});
        bar.addRibbonIcon(QStringLiteral("c"), QIcon(), QStringLiteral("C"), []() {});
        QCOMPARE(bar.iconIdsInOrder(),
                 (QStringList{QStringLiteral("a"),
                              QStringLiteral("b"),
                              QStringLiteral("c")}));
    }

    void signalsFireOnAddRemove()
    {
        Corbomite::RibbonToolBar bar;
        QSignalSpy added(&bar, &Corbomite::RibbonToolBar::iconAdded);
        QSignalSpy removed(&bar, &Corbomite::RibbonToolBar::iconRemoved);

        auto h = bar.addRibbonIcon(QStringLiteral("core:x"), QIcon(),
                                   QStringLiteral("X"), []() {});
        QCOMPARE(added.count(), 1);
        QCOMPARE(added.takeFirst().at(0).toString(), QStringLiteral("core:x"));

        bar.removeRibbonIcon(h);
        QCOMPARE(removed.count(), 1);
        QCOMPARE(removed.takeFirst().at(0).toString(), QStringLiteral("core:x"));
    }

    void signalFiresOnVisibilityChange()
    {
        Corbomite::RibbonToolBar bar;
        auto h = bar.addRibbonIcon(QStringLiteral("core:y"), QIcon(),
                                   QStringLiteral("Y"), []() {});
        QSignalSpy changed(&bar, &Corbomite::RibbonToolBar::iconVisibilityChanged);
        bar.setIconVisible(h, false);
        QCOMPARE(changed.count(), 1);
        const auto args = changed.takeFirst();
        QCOMPARE(args.at(0).toString(), QStringLiteral("core:y"));
        QCOMPARE(args.at(1).toBool(), false);
    }
};

QTEST_MAIN(TestRibbonToolBar)
#include "tst_ribbontoolbar.moc"
