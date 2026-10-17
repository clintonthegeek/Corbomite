// SPDX-License-Identifier: GPL-3.0-or-later
//
// ThreeWayMerge is a deliberate divergence from Obsidian's external-modify
// merge (`bZ`, a two-sided diff/patch that drops and relocates overlapping
// edits). These tests pin the Corbomite contract: an overlapping same-line
// change keeps BOTH edits rather than picking a winner or dropping one.
// See docs/obsidian-audit/addenda/2026-10-17-views-three-way-merge-deliberate-divergence.md
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

    // A single line changed on both sides keeps both edits, local first.
    void conflictingSingleLineKeepsBoth()
    {
        QString base = QStringLiteral("AAA");
        QString local = QStringLiteral("BBB");
        QString remote = QStringLiteral("CCC");
        QCOMPARE(ThreeWayMerge::threeWayMerge(base, local, remote),
                 QStringLiteral("BBB\nCCC"));
    }

    // The addendum's overlapping same-line case: neither edit is lost and
    // neither is relocated onto a neighbouring line (the failure mode of
    // Obsidian's two-sided patch). This is the regression the 2026-10-03
    // addendum asked for.
    void overlappingSameLineEditKeepsBothUnrelocated()
    {
        const QString base = QStringLiteral("para one\npara two\npara three\n");
        const QString local = QStringLiteral("para one\npara two USER EDIT\npara three\n");
        const QString remote = QStringLiteral("para one\npara two DISK EDIT\npara three\n");
        QCOMPARE(ThreeWayMerge::threeWayMerge(base, local, remote),
                 QStringLiteral("para one\npara two USER EDIT\npara two DISK EDIT\npara three\n"));
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
