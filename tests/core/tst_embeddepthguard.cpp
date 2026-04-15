// SPDX-License-Identifier: GPL-3.0-or-later
//
// Tests for Corbomite::Core::EmbedDepthGuard — Cluster J Phase 1 Task 1.3.
// Audit-confirmed cap (= 5) documented at
// docs/superpowers/research/2026-04-15-embed-depth-findings.md, matching
// Obsidian _internal.js ~line 627926 (`if (e.depth <= 5)`).

#include <QTest>

#include "corbomite/core/EmbedDepthGuard.h"

class TstEmbedDepthGuard : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testCapConstantMatchesAudit()
    {
        QCOMPARE(Corbomite::Core::EmbedDepthGuard::kMaxDepth, 5);
    }

    void testAllowsUpToMax()
    {
        Corbomite::Core::EmbedDepthGuard g;
        for (int d = 0; d < Corbomite::Core::EmbedDepthGuard::kMaxDepth; ++d) {
            QVERIFY2(g.allow(d), qPrintable(QStringLiteral("depth %1 must allow").arg(d)));
        }
    }

    void testRejectsAtMax()
    {
        Corbomite::Core::EmbedDepthGuard g;
        QVERIFY(!g.allow(Corbomite::Core::EmbedDepthGuard::kMaxDepth));
        QVERIFY(!g.allow(Corbomite::Core::EmbedDepthGuard::kMaxDepth + 1));
        QVERIFY(!g.allow(99));
    }

    void testPlaceholderShape()
    {
        const QString p = Corbomite::Core::EmbedDepthGuard::placeholder(
            QStringLiteral("FooNote"));
        QVERIFY(p.contains(QStringLiteral("FooNote")));
        QVERIFY(p.contains(QStringLiteral("embed depth")));
    }

    void testPlaceholderIsClickable()
    {
        // Matches Obsidian's oJ class: placeholder must expose the target
        // so the hosting widget can make it clickable (open in new pane).
        const QString t = Corbomite::Core::EmbedDepthGuard::placeholderTarget(
            QStringLiteral("FooNote"));
        QCOMPARE(t, QStringLiteral("FooNote"));
    }
};

QTEST_APPLESS_MAIN(TstEmbedDepthGuard)
#include "tst_embeddepthguard.moc"
