// SPDX-License-Identifier: GPL-3.0-or-later
#include <QPushButton>
#include <QSignalSpy>
#include <QTest>

#include "dialogs/Notice.h"

class TestNotice : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void testDefaultDurationConstant()
    {
        QCOMPARE(Corbomite::Notice::kDefaultDurationMs, 4000);
    }

    void testMessageRetained()
    {
        Corbomite::Notice notice(QStringLiteral("Saved!"), 100);
        QCOMPARE(notice.message(), QStringLiteral("Saved!"));
    }

    void testAutoDismissAfterDuration()
    {
        auto *notice = new Corbomite::Notice(QStringLiteral("Hi"), 50);
        QSignalSpy spy(notice, &QObject::destroyed);
        notice->show();
        QVERIFY(spy.wait(500));
    }

    void testActionButtonFiresCallbackAndCloses()
    {
        auto *notice = new Corbomite::Notice(QStringLiteral("Reverted"), 5000);
        bool clicked = false;
        notice->setAction(QStringLiteral("Undo"), [&clicked]() { clicked = true; });
        QSignalSpy destroyedSpy(notice, &QObject::destroyed);
        notice->show();

        // Find the action button (only QPushButton child) and click it.
        const auto buttons = notice->findChildren<QPushButton *>();
        QCOMPARE(buttons.size(), 1);
        buttons.first()->click();

        QVERIFY(clicked);
        QVERIFY(destroyedSpy.wait(200));
    }

    void testNoActionByDefault()
    {
        Corbomite::Notice notice(QStringLiteral("Plain"), 5000);
        const auto buttons = notice.findChildren<QPushButton *>();
        QCOMPARE(buttons.size(), 0);
    }
};

QTEST_MAIN(TestNotice)
#include "tst_notice.moc"
