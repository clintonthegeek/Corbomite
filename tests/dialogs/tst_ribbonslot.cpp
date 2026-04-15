// SPDX-License-Identifier: GPL-3.0-or-later
#include <QIcon>
#include <QSignalSpy>
#include <QTest>

#include "app/RibbonSlot.h"

class TestRibbonSlot : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void testAddIconReturnsHandle()
    {
        Corbomite::RibbonSlot ribbon;
        auto handle = ribbon.addRibbonIcon(QIcon(),
                                            QStringLiteral("New note"),
                                            []() {});
        QCOMPARE(handle, QStringLiteral("New note"));
        QVERIFY(ribbon.hasIcon(handle));
        QCOMPARE(ribbon.iconCount(), 1);
    }

    void testEmptyTitleRejected()
    {
        Corbomite::RibbonSlot ribbon;
        auto handle = ribbon.addRibbonIcon(QIcon(), QString(), []() {});
        QVERIFY(handle.isEmpty());
        QCOMPARE(ribbon.iconCount(), 0);
    }

    void testDuplicateTitleSilentlyDropped()
    {
        // Per Obsidian compat: addRibbonIcon keys on title; same-title
        // collision is silently ignored (first wins).
        Corbomite::RibbonSlot ribbon;
        auto first = ribbon.addRibbonIcon(QIcon(),
                                           QStringLiteral("Open graph"),
                                           []() {});
        auto second = ribbon.addRibbonIcon(QIcon(),
                                            QStringLiteral("Open graph"),
                                            []() {});
        QVERIFY(!first.isEmpty());
        QVERIFY(second.isEmpty());
        QCOMPARE(ribbon.iconCount(), 1);
    }

    void testRemoveIcon()
    {
        Corbomite::RibbonSlot ribbon;
        auto handle = ribbon.addRibbonIcon(QIcon(),
                                            QStringLiteral("Switcher"),
                                            []() {});
        QVERIFY(ribbon.removeRibbonIcon(handle));
        QVERIFY(!ribbon.hasIcon(handle));
        QCOMPARE(ribbon.iconCount(), 0);
    }

    void testRemoveUnknownReturnsFalse()
    {
        Corbomite::RibbonSlot ribbon;
        QVERIFY(!ribbon.removeRibbonIcon(QStringLiteral("never-added")));
    }

    void testCallbackFiresOnTrigger()
    {
        Corbomite::RibbonSlot ribbon;
        int callCount = 0;
        auto handle = ribbon.addRibbonIcon(QIcon(),
                                            QStringLiteral("Counter"),
                                            [&callCount]() { ++callCount; });
        QVERIFY(!handle.isEmpty());
        // The QAction is owned by the toolbar; no public accessor in
        // RibbonSlot, so we walk children to invoke trigger().
        const auto actions = ribbon.findChildren<QAction *>();
        QAction *target = nullptr;
        for (QAction *a : actions) {
            if (a->text() == QStringLiteral("Counter")) { target = a; break; }
        }
        QVERIFY(target);
        target->trigger();
        QCOMPARE(callCount, 1);
    }
};

QTEST_MAIN(TestRibbonSlot)
#include "tst_ribbonslot.moc"
