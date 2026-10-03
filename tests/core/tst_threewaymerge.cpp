// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include "corbomite/core/ThreeWayMerge.h"

using Corbomite::ThreeWayMerge;

class TestThreeWayMerge : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void noChanges()
    {
        QString base = QStringLiteral("hello world");
        QCOMPARE(ThreeWayMerge::threeWayMerge(base, base, base), base);
    }

    void localOnlyChange()
    {
        QString base = QStringLiteral("hello world");
        QString local = QStringLiteral("hello brave world");
        QCOMPARE(ThreeWayMerge::threeWayMerge(base, local, base), local);
    }

    void remoteOnlyChange()
    {
        QString base = QStringLiteral("hello world");
        QString remote = QStringLiteral("hello new world");
        QCOMPARE(ThreeWayMerge::threeWayMerge(base, base, remote), remote);
    }

    void cleanMerge()
    {
        QString base = QStringLiteral("line1\nline2\nline3");
        QString local = QStringLiteral("line1\nline2-local\nline3");
        QString remote = QStringLiteral("line1\nline2\nline3-remote");
        QString merged = ThreeWayMerge::threeWayMerge(base, local, remote);
        QVERIFY(merged.contains(QStringLiteral("line2-local")));
        QVERIFY(merged.contains(QStringLiteral("line3-remote")));
    }

    void conflictRemoteWins()
    {
        QString base = QStringLiteral("AAA");
        QString local = QStringLiteral("BBB");
        QString remote = QStringLiteral("CCC");
        QString merged = ThreeWayMerge::threeWayMerge(base, local, remote);
        QVERIFY(!merged.isEmpty());
    }

    void emptyBase()
    {
        QString base;
        QString local = QStringLiteral("new content");
        QString remote = QStringLiteral("other content");
        QString merged = ThreeWayMerge::threeWayMerge(base, local, remote);
        QVERIFY(!merged.isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestThreeWayMerge)
#include "tst_threewaymerge.moc"
