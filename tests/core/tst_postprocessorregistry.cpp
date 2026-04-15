// SPDX-License-Identifier: GPL-3.0-or-later
//
// Tests for Corbomite::Core::PostProcessorRegistry — stable priority-sorted
// post-processor dispatch. Cluster J Phase 2 Task 2.1.

#include <QTest>

#include "corbomite/core/PostProcessorRegistry.h"

class TstPostProcessorRegistry : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testRegisterAndIterate()
    {
        Corbomite::Core::PostProcessorRegistry reg;
        QStringList order;
        reg.registerProcessor(
            10, [&](void *, const Corbomite::Core::PostProcessorContext &) {
                order << QStringLiteral("a");
            });
        reg.registerProcessor(
            -5, [&](void *, const Corbomite::Core::PostProcessorContext &) {
                order << QStringLiteral("b");
            });
        reg.registerProcessor(
            5, [&](void *, const Corbomite::Core::PostProcessorContext &) {
                order << QStringLiteral("c");
            });
        reg.run(nullptr, {});
        // lower priority = earlier
        QCOMPARE(order,
                 (QStringList{QStringLiteral("b"),
                              QStringLiteral("c"),
                              QStringLiteral("a")}));
    }

    void testStableTiesInsertionOrder()
    {
        Corbomite::Core::PostProcessorRegistry reg;
        QStringList order;
        reg.registerProcessor(
            5, [&](void *, const Corbomite::Core::PostProcessorContext &) {
                order << QStringLiteral("first");
            });
        reg.registerProcessor(
            5, [&](void *, const Corbomite::Core::PostProcessorContext &) {
                order << QStringLiteral("second");
            });
        reg.registerProcessor(
            5, [&](void *, const Corbomite::Core::PostProcessorContext &) {
                order << QStringLiteral("third");
            });
        reg.run(nullptr, {});
        QCOMPARE(order,
                 (QStringList{QStringLiteral("first"),
                              QStringLiteral("second"),
                              QStringLiteral("third")}));
    }

    void testUnregister()
    {
        Corbomite::Core::PostProcessorRegistry reg;
        int count = 0;
        auto h = reg.registerProcessor(
            0, [&](void *, const Corbomite::Core::PostProcessorContext &) {
                count++;
            });
        reg.run(nullptr, {});
        QCOMPARE(count, 1);
        reg.unregister(h);
        reg.run(nullptr, {});
        QCOMPARE(count, 1); // unchanged
    }

    void testContextPropagates()
    {
        Corbomite::Core::PostProcessorRegistry reg;
        QString seenPath;
        int seenDepth = -1;
        reg.registerProcessor(
            0,
            [&](void *, const Corbomite::Core::PostProcessorContext &ctx) {
                seenPath = ctx.sourcePath;
                seenDepth = ctx.depth;
            });
        Corbomite::Core::PostProcessorContext ctx;
        ctx.sourcePath = QStringLiteral("Note.md");
        ctx.depth = 2;
        reg.run(nullptr, ctx);
        QCOMPARE(seenPath, QStringLiteral("Note.md"));
        QCOMPARE(seenDepth, 2);
    }
};

QTEST_APPLESS_MAIN(TstPostProcessorRegistry)
#include "tst_postprocessorregistry.moc"
