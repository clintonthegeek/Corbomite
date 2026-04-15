// SPDX-License-Identifier: GPL-3.0-or-later
#include <QAction>
#include <QApplication>
#include <QMenu>
#include <QSignalSpy>
#include <QTest>

#include "corbomite/core/MenuEventEmitter.h"
#include "corbomite/core/MenuSectionHelper.h"

class TestMenuSectionHelper : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void testCanonicalOrder()
    {
        const auto &order = Corbomite::MenuSectionHelper::canonicalSectionOrder();
        QCOMPARE(order.first(), QStringLiteral("title"));
        QCOMPARE(order.last(), QStringLiteral("danger"));
        QVERIFY(order.contains(QStringLiteral("info.copy")));
        QVERIFY(order.contains(QString()));  // unset bucket present
    }

    void testFinalizeSortsByCanonicalOrder()
    {
        QMenu menu;
        Corbomite::MenuSectionHelper helper(&menu);

        QAction copyAct(QStringLiteral("Copy"), this);
        QAction openAct(QStringLiteral("Open"), this);
        QAction deleteAct(QStringLiteral("Delete"), this);
        QAction titleAct(QStringLiteral("Note"), this);

        // Add scrambled across sections.
        helper.addToSection(&copyAct, QStringLiteral("info.copy"));
        helper.addToSection(&deleteAct, QStringLiteral("danger"));
        helper.addToSection(&titleAct, QStringLiteral("title"));
        helper.addToSection(&openAct, QStringLiteral("open"));

        helper.finalize();

        // Filter out separators for the order assertion.
        QList<QAction *> nonSep;
        for (QAction *a : menu.actions()) {
            if (!a->isSeparator()) nonSep.append(a);
        }
        QCOMPARE(nonSep.size(), 4);
        QCOMPARE(nonSep.at(0)->text(), QStringLiteral("Note"));
        QCOMPARE(nonSep.at(1)->text(), QStringLiteral("Open"));
        QCOMPARE(nonSep.at(2)->text(), QStringLiteral("Copy"));
        QCOMPARE(nonSep.at(3)->text(), QStringLiteral("Delete"));
    }

    void testFinalizeInsertsSeparatorsBetweenSections()
    {
        QMenu menu;
        Corbomite::MenuSectionHelper helper(&menu);

        QAction openAct(QStringLiteral("Open"), this);
        QAction copyAct(QStringLiteral("Copy"), this);

        helper.addToSection(&openAct, QStringLiteral("open"));
        helper.addToSection(&copyAct, QStringLiteral("info.copy"));

        helper.finalize();
        QCOMPARE(menu.actions().size(), 3);  // Open, separator, Copy
        QVERIFY(menu.actions().at(1)->isSeparator());
    }

    void testInsertionOrderPreservedWithinSection()
    {
        QMenu menu;
        Corbomite::MenuSectionHelper helper(&menu);
        QAction a1(QStringLiteral("first"), this);
        QAction a2(QStringLiteral("second"), this);
        QAction a3(QStringLiteral("third"), this);

        helper.addToSection(&a1, QStringLiteral("action"));
        helper.addToSection(&a2, QStringLiteral("action"));
        helper.addToSection(&a3, QStringLiteral("action"));

        helper.finalize();
        const auto &actions = menu.actions();
        QCOMPARE(actions.size(), 3);
        QCOMPARE(actions.at(0)->text(), QStringLiteral("first"));
        QCOMPARE(actions.at(2)->text(), QStringLiteral("third"));
    }

    void testUnknownSectionFunnelsToUnsetBucket()
    {
        QMenu menu;
        Corbomite::MenuSectionHelper helper(&menu);
        QAction openAct(QStringLiteral("Open"), this);
        QAction junkAct(QStringLiteral("Junk"), this);
        QAction dangerAct(QStringLiteral("Delete"), this);

        helper.addToSection(&openAct, QStringLiteral("open"));
        helper.addToSection(&junkAct, QStringLiteral("plugin-made-up"));
        helper.addToSection(&dangerAct, QStringLiteral("danger"));

        helper.finalize();
        // Order: Open (open), sep, Junk (unset bucket between system and danger),
        // sep, Delete (danger). Verify Junk lands between Open and Delete.
        QStringList texts;
        for (QAction *a : menu.actions()) {
            if (!a->isSeparator()) texts.append(a->text());
        }
        QCOMPARE(texts, (QStringList{QStringLiteral("Open"),
                                       QStringLiteral("Junk"),
                                       QStringLiteral("Delete")}));
    }

    void testFinalizeIdempotent()
    {
        QMenu menu;
        Corbomite::MenuSectionHelper helper(&menu);
        QAction a(QStringLiteral("X"), this);
        helper.addToSection(&a, QStringLiteral("action"));

        helper.finalize();
        const int firstCount = menu.actions().size();
        helper.finalize();
        QCOMPARE(menu.actions().size(), firstCount);
    }

    // --- MenuEventEmitter ---

    void testEmitFileMenuFires()
    {
        Corbomite::MenuEventEmitter emitter;
        QSignalSpy spy(&emitter, &Corbomite::MenuEventEmitter::fileMenu);
        QMenu m;
        emitter.emitFileMenu(&m, QStringLiteral("note.md"));
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(1).toString(), QStringLiteral("note.md"));
    }

    void testEmitTabGroupMenuFires()
    {
        Corbomite::MenuEventEmitter emitter;
        QSignalSpy spy(&emitter, &Corbomite::MenuEventEmitter::tabGroupMenu);
        QMenu m;
        emitter.emitTabGroupMenu(&m, this);
        QCOMPARE(spy.count(), 1);
    }

    void testEmitterIntegratesWithHelper()
    {
        // Real-world flow: caller builds core items via helper, emits the
        // signal, plugins (we simulate one) push items into a section, then
        // helper.finalize() flushes everything in the right order.
        Corbomite::MenuEventEmitter emitter;
        QMenu m;
        Corbomite::MenuSectionHelper helper(&m);

        QAction openAct(QStringLiteral("Open"), this);
        helper.addToSection(&openAct, QStringLiteral("open"));

        QAction pluginAct(QStringLiteral("PluginThing"), this);
        connect(&emitter, &Corbomite::MenuEventEmitter::fileMenu,
                this, [&](QMenu *, const QString &) {
                    helper.addToSection(&pluginAct, QStringLiteral("action"));
                });
        emitter.emitFileMenu(&m, QStringLiteral("note.md"));

        helper.finalize();
        QStringList texts;
        for (QAction *a : m.actions()) {
            if (!a->isSeparator()) texts.append(a->text());
        }
        QCOMPARE(texts,
                 (QStringList{QStringLiteral("Open"), QStringLiteral("PluginThing")}));
    }
};

QTEST_MAIN(TestMenuSectionHelper)
#include "tst_menusectionhelper.moc"
