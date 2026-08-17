// tests/core/tst_workspace_serialize.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QVector>

#include <kddockwidgets/qtwidgets/MainWindow.h>

#include "corbomite/core/Workspace.h"
#include "corbomite/core/WorkspaceLeaf.h"
#include "corbomite/core/ViewRegistry.h"

using namespace Corbomite;

class TestWorkspaceSerialize : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void serializeEmptyWorkspace()
    {
        ViewRegistry registry;
        Workspace ws(&registry);
        QJsonObject json = ws.serialize();

        QVERIFY(json.contains(QStringLiteral("main")));
        QVERIFY(json.contains(QStringLiteral("active")));
    }

    void roundTripSimpleLayout()
    {
        ViewRegistry registry;
        Workspace ws(&registry);

        auto *leaf = ws.createLeafInActiveGroup();
        QVERIFY(leaf);
        ws.setActiveLeaf(leaf);

        QJsonObject json = ws.serialize();
        QCOMPARE(json[QStringLiteral("active")].toString(), leaf->id());

        Workspace ws2(&registry);
        ws2.deserialize(json);
        QCOMPARE(ws2.allLeaves().size(), 1);
    }

    void obsidianSchemaShape()
    {
        ViewRegistry registry;
        Workspace ws(&registry);

        QJsonObject json = ws.serialize();
        auto main = json[QStringLiteral("main")].toObject();
        QCOMPARE(main[QStringLiteral("type")].toString(), QStringLiteral("split"));
        QVERIFY(main.contains(QStringLiteral("children")));
        QVERIFY(main.contains(QStringLiteral("direction")));
    }

    void writeAndReadWorkspaceJson()
    {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        QString vaultPath = tmpDir.path();
        QDir(vaultPath).mkpath(QStringLiteral(".obsidian"));

        ViewRegistry registry;
        Workspace ws(&registry);
        ws.writeWorkspaceJson(vaultPath);

        QFile f(vaultPath + QStringLiteral("/.obsidian/workspace.json"));
        QVERIFY(f.exists());

        Workspace ws2(&registry);
        ws2.readWorkspaceJson(vaultPath);
        QVERIFY(ws2.rootWidget() != nullptr);
    }

    void lastOpenFilesRoundTrip()
    {
        ViewRegistry registry;
        Workspace ws(&registry);
        ws.setLastOpenFiles({QStringLiteral("a.md"), QStringLiteral("b.md")});

        QJsonObject json = ws.serialize();
        Workspace ws2(&registry);
        ws2.deserialize(json);
        QCOMPARE(ws2.lastOpenFiles().size(), 2);
        QCOMPARE(ws2.lastOpenFiles().first(), QStringLiteral("a.md"));
    }

    void activeLeafIdPreserved()
    {
        ViewRegistry registry;
        Workspace ws(&registry);

        auto *leaf1 = ws.createLeafInActiveGroup();
        QVERIFY(leaf1);
        auto *leaf2 = ws.createLeafInGroupOf(leaf1);
        QVERIFY(leaf2);
        ws.setActiveLeaf(leaf2);

        QJsonObject json = ws.serialize();
        QCOMPARE(json[QStringLiteral("active")].toString(), leaf2->id());
    }

    // --- Cluster L / Phase L1 (teardown unification) regression tests ---

    void deserializeManyLayoutsBackToBack_noCrashCorrectCounts()
    {
        // A1 regression: deserialize used to qDeleteAll(m_leaves)
        // synchronously and only clear m_leaves/m_leavesById/m_tabGroupOf/
        // m_stackedGroups *afterwards*, so a KDDW signal firing
        // re-entrantly off one leaf's dock-widget destruction (e.g.
        // isOpenChanged cascading through wireLeafKddwSignals) could still
        // find sibling leaves later in the same batch "registered" while
        // they were mid-delete — a use-after-free on every vault open with
        // a saved session. Round-tripping several different saved layouts
        // (flat tab group, nested splits, a floating window, empty) back
        // to back into the same Workspace, offscreen, is the closest thing
        // to a fuzz test for that ordering hazard without an actual
        // crash repro.
        ViewRegistry registry;
        Workspace ws(QStringLiteral("test-vault-stress"), &registry);
        ws.kddwMainWindow()->show();

        // Fixture: three leaves sharing one tab group.
        QJsonObject flat;
        {
            Workspace src(&registry);
            auto *a = src.createLeafInActiveGroup();
            QVERIFY(a);
            QVERIFY(src.createLeafInGroupOf(a));
            QVERIFY(src.createLeafInGroupOf(a));
            flat = src.serialize();
        }

        // Fixture: nested splits — split A, then split the result again in
        // the other orientation, producing a 3-leaf tree with two levels
        // of split container.
        QJsonObject nested;
        {
            Workspace src(QStringLiteral("test-vault-stress-nested"), &registry);
            auto *a = src.createLeafInActiveGroup();
            QVERIFY(a);
            auto *b = src.splitLeaf(a, Qt::Horizontal);
            QVERIFY(b);
            auto *c = src.splitLeaf(b, Qt::Vertical);
            QVERIFY(c);
            nested = src.serialize();
        }

        // Fixture: a floating (popped-out) window alongside the main tree.
        QJsonObject floating;
        {
            Workspace src(QStringLiteral("test-vault-stress-float"), &registry);
            src.kddwMainWindow()->show();
            auto *a = src.createLeafInActiveGroup();
            QVERIFY(a);
            QVERIFY(src.createLeafInGroupOf(a));
            auto *popped = src.splitLeaf(a, Qt::Horizontal);
            QVERIFY(popped);
            QVERIFY(src.popoutLeaf(popped));
            floating = src.serialize();
        }

        // Fixture: empty workspace. deserialize's fallback for an empty
        // 'main' tree materializes one default empty leaf rather than
        // leaving the workspace leafless (WorkspaceSerializer::fromJson's
        // installDefault()) — so 1, not 0, is the correct expectation here.
        QJsonObject empty;
        {
            Workspace src(&registry);
            empty = src.serialize();
        }

        struct Round { QJsonObject json; int expectedLeaves; };
        const QVector<Round> rounds = {
            { flat, 3 }, { nested, 3 }, { floating, 3 }, { empty, 1 },
            { flat, 3 }, { nested, 3 }, { empty, 1 }, { floating, 3 },
        };
        for (const auto &round : rounds) {
            ws.deserialize(round.json);
            QCOMPARE(ws.allLeaves().size(), round.expectedLeaves);
        }
    }

    void openCloseReopenCycle_noCrashAndConsistentState()
    {
        // One full open -> close -> reopen cycle in a single Workspace
        // instance, exercising resetToDefaultLayout (the A1 twin) followed
        // by deserialize.
        ViewRegistry registry;
        Workspace ws(QStringLiteral("test-vault-cycle"), &registry);
        ws.kddwMainWindow()->show();

        auto *a = ws.createLeafInActiveGroup();
        QVERIFY(a);
        auto *b = ws.createLeafInGroupOf(a);
        QVERIFY(b);
        ws.setActiveLeaf(b);
        QCOMPARE(ws.allLeaves().size(), 2);

        const QJsonObject saved = ws.serialize();

        // "Close" the vault.
        ws.resetToDefaultLayout();
        QCOMPARE(ws.allLeaves().size(), 0);
        QVERIFY(ws.activeLeaf() == nullptr);

        // "Reopen" it.
        ws.deserialize(saved);
        QCOMPARE(ws.allLeaves().size(), 2);
        QVERIFY(ws.activeLeaf() != nullptr);
    }

    void resetToDefaultLayout_clearsStackedGroups()
    {
        // A4 regression: resetToDefaultLayout cleared m_leaves/
        // m_leavesById/m_tabGroupOf but leaked m_stackedGroups across
        // resets (deserialize already cleared it). Set a tab group
        // stacked, reset, and confirm the bit is gone rather than
        // surviving under its old (now-orphaned) id.
        ViewRegistry registry;
        Workspace ws(&registry);

        auto *leaf = ws.createLeafInActiveGroup();
        QVERIFY(leaf);
        const QString groupId = ws.tabGroupIdOf(leaf);
        QVERIFY(!groupId.isEmpty());
        ws.setTabGroupStacked(groupId, true);
        QVERIFY(ws.isTabGroupStacked(groupId));

        ws.resetToDefaultLayout();
        QVERIFY(!ws.isTabGroupStacked(groupId));
    }
};

QTEST_MAIN(TestWorkspaceSerialize)
#include "tst_workspace_serialize.moc"
