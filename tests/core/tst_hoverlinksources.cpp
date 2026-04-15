// SPDX-License-Identifier: GPL-3.0-or-later
#include <QSignalSpy>
#include <QTest>

#include "corbomite/core/HoverLinkSource.h"
#include "corbomite/core/HoverLinkSourceRegistry.h"

class TestHoverLinkSources : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void testRegisterAndLookup()
    {
        Corbomite::HoverLinkSourceRegistry reg;
        QVERIFY(reg.registerSource({QStringLiteral("editor"),
                                     QStringLiteral("Editor"),
                                     Qt::NoModifier}));
        QVERIFY(reg.isRegistered(QStringLiteral("editor")));
        const auto src = reg.lookup(QStringLiteral("editor"));
        QCOMPARE(src.id, QStringLiteral("editor"));
        QCOMPARE(src.display, QStringLiteral("Editor"));
        QCOMPARE(src.defaultMod, Qt::NoModifier);
    }

    void testRegisterRejectsDuplicateId()
    {
        Corbomite::HoverLinkSourceRegistry reg;
        QVERIFY(reg.registerSource({QStringLiteral("editor"),
                                     QStringLiteral("Editor"),
                                     Qt::NoModifier}));
        // First-wins per Obsidian behaviour — second registration is dropped.
        QVERIFY(!reg.registerSource({QStringLiteral("editor"),
                                      QStringLiteral("Other Editor"),
                                      Qt::ControlModifier}));
        QCOMPARE(reg.lookup(QStringLiteral("editor")).display,
                 QStringLiteral("Editor"));
    }

    void testRegisterRejectsEmptyId()
    {
        Corbomite::HoverLinkSourceRegistry reg;
        QVERIFY(!reg.registerSource({QString(), QStringLiteral("Empty"), Qt::NoModifier}));
    }

    void testUnregisterEmitsAndRemoves()
    {
        Corbomite::HoverLinkSourceRegistry reg;
        QSignalSpy spy(&reg, &Corbomite::HoverLinkSourceRegistry::sourceUnregistered);
        reg.registerSource({QStringLiteral("graph"),
                            QStringLiteral("Graph"),
                            Qt::NoModifier});
        reg.unregisterSource(QStringLiteral("graph"));
        QVERIFY(!reg.isRegistered(QStringLiteral("graph")));
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("graph"));
    }

    void testUnregisterUnknownIsNoOp()
    {
        Corbomite::HoverLinkSourceRegistry reg;
        QSignalSpy spy(&reg, &Corbomite::HoverLinkSourceRegistry::sourceUnregistered);
        reg.unregisterSource(QStringLiteral("nonexistent"));
        QCOMPARE(spy.count(), 0);
    }

    void testRegisterEmitsSignal()
    {
        Corbomite::HoverLinkSourceRegistry reg;
        QSignalSpy spy(&reg, &Corbomite::HoverLinkSourceRegistry::sourceRegistered);
        reg.registerSource({QStringLiteral("backlinks"),
                            QStringLiteral("Backlinks"),
                            Qt::NoModifier});
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("backlinks"));
    }

    void testRegisterBuiltins()
    {
        Corbomite::HoverLinkSourceRegistry reg;
        reg.registerBuiltins();
        QVERIFY(reg.isRegistered(QStringLiteral("editor")));
        QVERIFY(reg.isRegistered(QStringLiteral("search")));
        QVERIFY(reg.isRegistered(QStringLiteral("backlinks")));
        QVERIFY(reg.isRegistered(QStringLiteral("outlinks")));
        QVERIFY(reg.isRegistered(QStringLiteral("graph")));
        QVERIFY(reg.isRegistered(QStringLiteral("bases")));
        // Search uses Ctrl-modifier for hover preview (Obsidian default).
        QCOMPARE(reg.lookup(QStringLiteral("search")).defaultMod,
                 Qt::ControlModifier);
        // Editor is plain hover.
        QCOMPARE(reg.lookup(QStringLiteral("editor")).defaultMod,
                 Qt::NoModifier);
    }

    void testAllSourcesReturnsEverything()
    {
        Corbomite::HoverLinkSourceRegistry reg;
        reg.registerBuiltins();
        QCOMPARE(reg.allSources().size(), 6);
    }
};

QTEST_MAIN(TestHoverLinkSources)
#include "tst_hoverlinksources.moc"
