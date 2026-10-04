// tests/core/tst_workspace_serialize.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QDir>
#include <QFile>
#include <QJsonArray>
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

    // --- Obsidian 1.13.7 recent-file contract ---------------------------
    //
    // n4.collect (app/own/1930-n4.js:10-28) keeps four *independent*
    // per-category quotas, so the array written into workspace.json has a
    // hard ceiling of 25 md + 10 canvas + 10 image + 10 other = 55. Corbomite
    // used a flat cap of 50, which silently truncated a real
    // Obsidian-authored list on the first save.
    void lastOpenFiles_quotaPerCategory()
    {
        ViewRegistry registry;
        Workspace ws(&registry);

        for (int i = 1; i <= 30; ++i)
            ws.pushLastOpenFile(QStringLiteral("notes/md%1.md").arg(i, 2, 10, QLatin1Char('0')));
        for (int i = 1; i <= 10; ++i)
            ws.pushLastOpenFile(QStringLiteral("notes/c%1.canvas").arg(i));
        for (int i = 1; i <= 10; ++i)
            ws.pushLastOpenFile(QStringLiteral("assets/p%1.png").arg(i));
        for (int i = 1; i <= 10; ++i)
            ws.pushLastOpenFile(QStringLiteral("notes/d%1.pdf").arg(i));

        // Survivors, in exactly the order collect()'s walk produces: newest
        // first, then each category's own recency order, interleaved only by
        // how they were pushed.
        QStringList expected;
        for (int i = 10; i >= 1; --i)
            expected << QStringLiteral("notes/d%1.pdf").arg(i);
        for (int i = 10; i >= 1; --i)
            expected << QStringLiteral("assets/p%1.png").arg(i);
        for (int i = 10; i >= 1; --i)
            expected << QStringLiteral("notes/c%1.canvas").arg(i);
        for (int i = 30; i >= 6; --i)
            expected << QStringLiteral("notes/md%1.md").arg(i, 2, 10, QLatin1Char('0'));

        QCOMPARE(ws.lastOpenFiles(), expected);
        QCOMPARE(ws.lastOpenFiles().size(), 55);

        // The incoming path is seeded outside the quota loop
        // (`n = [e.path]`, app/own/1930-n4.js:13), so a push into an
        // already-full category lands as a 56th entry rather than evicting
        // one of the ten.
        ws.pushLastOpenFile(QStringLiteral("notes/d11.pdf"));
        QCOMPARE(ws.lastOpenFiles().first(), QStringLiteral("notes/d11.pdf"));
        QCOMPARE(ws.lastOpenFiles().count(QStringLiteral("notes/d1.pdf")), 1);
        QCOMPARE(ws.lastOpenFiles().size(), 56);
    }

    // Category routing is by Obsidian's own extension tables (app/own/
    // 0794-hb.js:2-8) and Fl()'s lowercasing (app/own/0365-Fl.js:2-5):
    // .base/.pdf/extensionless count as `other`, and an uppercase suffix
    // still matches. Interleaving proves the quotas are per-category rather
    // than one shared budget.
    void lastOpenFiles_extensionRouting()
    {
        ViewRegistry registry;
        Workspace ws(&registry);

        const QStringList others = {
            QStringLiteral("a.base"),   QStringLiteral("b.pdf"),
            QStringLiteral("c.wav"),    QStringLiteral("d"),
            QStringLiteral(".dotfile"), QStringLiteral("trailing."),
            QStringLiteral("g.mp4"),    QStringLiteral("h.excalidraw.md"),
        };
        for (const auto &p : others)
            ws.pushLastOpenFile(p);
        for (int i = 1; i <= 10; ++i)
            ws.pushLastOpenFile(QStringLiteral("i%1.PNG").arg(i));
        for (int i = 1; i <= 10; ++i)
            ws.pushLastOpenFile(QStringLiteral("j%1.MD").arg(i));

        const auto files = ws.lastOpenFiles();
        // 8 `other` + 10 image + 10 md, and the image budget was not eaten
        // by the `other` entries pushed before it.
        QCOMPARE(files.size(), 28);
        QCOMPARE(files.count(QStringLiteral("i1.PNG")), 1);
        QCOMPARE(files.count(QStringLiteral("j1.MD")), 1);
        QCOMPARE(files.count(QStringLiteral("h.excalidraw.md")), 1);
        for (const auto &p : others)
            QCOMPARE(files.count(p), 1);

        // The budgets are independent, and collect() seeds `n` with the
        // incoming path *outside* the quota loop — so the newcomer always
        // survives and its category may hold one more than its quota until
        // the next push in some other category re-walks the list.
        ws.pushLastOpenFile(QStringLiteral("overflow.base"));
        QCOMPARE(ws.lastOpenFiles().count(QStringLiteral("overflow.base")), 1);
        QCOMPARE(ws.lastOpenFiles().size(), 29);
        ws.pushLastOpenFile(QStringLiteral("overflow.png"));
        QCOMPARE(ws.lastOpenFiles().first(), QStringLiteral("overflow.png"));
        QCOMPARE(ws.lastOpenFiles().count(QStringLiteral("overflow.png")), 1);
        QCOMPARE(ws.lastOpenFiles().count(QStringLiteral("i1.PNG")), 1);
        QCOMPARE(ws.lastOpenFiles().size(), 30);
        // The next push re-walks the list, and only then does the image
        // overage cost anything: the oldest image is dropped to restore its
        // category to 10, and the total holds at 30.
        ws.pushLastOpenFile(QStringLiteral("overflow.md"));
        QCOMPARE(ws.lastOpenFiles().count(QStringLiteral("overflow.md")), 1);
        QCOMPARE(ws.lastOpenFiles().count(QStringLiteral("i1.PNG")), 0);
        QCOMPARE(ws.lastOpenFiles().count(QStringLiteral("i2.PNG")), 1);
        QCOMPARE(ws.lastOpenFiles().size(), 30);
    }

    void lastOpenFiles_collectSemantics()
    {
        ViewRegistry registry;
        Workspace ws(&registry);

        ws.setLastOpenFiles({QStringLiteral("a.md"), QStringLiteral("b.md")});
        ws.pushLastOpenFile(QStringLiteral("c.md"));
        QCOMPARE(ws.lastOpenFiles(),
                 QStringList({QStringLiteral("c.md"), QStringLiteral("a.md"),
                              QStringLiteral("b.md")}));
        // A path already in the list is moved to the front, not duplicated.
        ws.pushLastOpenFile(QStringLiteral("b.md"));
        QCOMPARE(ws.lastOpenFiles(),
                 QStringList({QStringLiteral("b.md"), QStringLiteral("c.md"),
                              QStringLiteral("a.md")}));
        // collect() is a no-op when the path is already at index 0.
        ws.pushLastOpenFile(QStringLiteral("b.md"));
        QCOMPARE(ws.lastOpenFiles(),
                 QStringList({QStringLiteral("b.md"), QStringLiteral("c.md"),
                              QStringLiteral("a.md")}));
    }

    // saveLayout assigns lastOpenFiles unconditionally
    // (app/own/1932-r4.js:1173), so a workspace that has opened nothing still
    // emits the key with an empty array.
    void lastOpenFiles_writtenEvenWhenEmpty()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        ViewRegistry registry;
        Workspace ws(&registry);
        ws.writeWorkspaceJson(tmp.path());

        QFile f(tmp.path() + QStringLiteral("/.obsidian/workspace.json"));
        QVERIFY(f.open(QIODevice::ReadOnly));
        const auto root = QJsonDocument::fromJson(f.readAll()).object();
        QVERIFY2(root.contains(QStringLiteral("lastOpenFiles")),
                 "lastOpenFiles must be present even when empty");
        QCOMPARE(root.value(QStringLiteral("lastOpenFiles")).toArray().size(), 0);
    }

    // saveLayout() returns early while layoutReady is false
    // (app/own/1932-r4.js:1172), so a save requested from inside a
    // clearLayout()/setLayout() pair must not overwrite the previous file.
    void writeWorkspaceJson_skippedWhileLayoutNotReady()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString path = tmp.path() + QStringLiteral("/.obsidian/workspace.json");
        const auto readAll = [&path] {
            QFile f(path);
            return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
        };

        ViewRegistry registry;
        Workspace ws(&registry);
        ws.writeWorkspaceJson(tmp.path());
        QVERIFY(QFile::exists(path));
        const QByteArray original = readAll();
        QVERIFY(!original.isEmpty());

        ws.setLayoutReady(false);
        ws.pushLastOpenFile(QStringLiteral("should-not-be-written.md"));
        ws.writeWorkspaceJson(tmp.path());
        QCOMPARE(readAll(), original);

        // ...and once the layout is ready again the write does land.
        ws.setLayoutReady(true);
        ws.writeWorkspaceJson(tmp.path());
        QVERIFY(readAll() != original);
    }

    // End-to-end check of the root key order on disk: getLayout's assignment
    // order (app/own/1932-r4.js:1163) followed by saveLayout's lastOpenFiles
    // append (:1173). Qt's QJsonObject sorts, so this only holds because
    // writeWorkspaceJson emits through WorkspaceSerializer::toJsonText.
    void workspaceJson_rootKeyOrderOnDisk()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        QDir().mkpath(tmp.path() + QStringLiteral("/.obsidian"));

        const QString leafId = QStringLiteral("ordleafaaaaaaaaa");
        const QString tabsId = QStringLiteral("ordtabsaaaaaaaa");
        const QString rootId = QStringLiteral("ordrootaaaaaaaa");
        QJsonObject leaf;
        leaf[QStringLiteral("id")] = leafId;
        leaf[QStringLiteral("type")] = QStringLiteral("leaf");
        QJsonObject leafState;
        leafState[QStringLiteral("type")] = QStringLiteral("empty");
        leafState[QStringLiteral("state")] = QJsonObject();
        leaf[QStringLiteral("state")] = leafState;
        QJsonObject tabs;
        tabs[QStringLiteral("id")] = tabsId;
        tabs[QStringLiteral("type")] = QStringLiteral("tabs");
        tabs[QStringLiteral("children")] = QJsonArray{leaf};
        QJsonObject main;
        main[QStringLiteral("id")] = rootId;
        main[QStringLiteral("type")] = QStringLiteral("split");
        main[QStringLiteral("direction")] = QStringLiteral("vertical");
        main[QStringLiteral("children")] = QJsonArray{tabs};
        QJsonObject dock;
        dock[QStringLiteral("id")] = QStringLiteral("ordleftaaaaaaaa");
        dock[QStringLiteral("type")] = QStringLiteral("split");
        dock[QStringLiteral("direction")] = QStringLiteral("horizontal");
        dock[QStringLiteral("width")] = 250;

        QJsonObject seed;
        seed[QStringLiteral("main")] = main;
        seed[QStringLiteral("left")] = dock;
        seed[QStringLiteral("right")] = dock;
        seed[QStringLiteral("active")] = leafId;
        seed[QStringLiteral("lastOpenFiles")] = QJsonArray{QStringLiteral("n.md")};
        {
            QFile out(tmp.path() + QStringLiteral("/.obsidian/workspace.json"));
            QVERIFY(out.open(QIODevice::WriteOnly));
            out.write(QJsonDocument(seed).toJson());
        }

        ViewRegistry registry;
        Workspace ws(QStringLiteral("test-vault-order"), &registry);
        ws.readWorkspaceJson(tmp.path());
        ws.writeWorkspaceJson(tmp.path());

        QFile f(tmp.path() + QStringLiteral("/.obsidian/workspace.json"));
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QByteArray text = f.readAll();
        const QStringList order = [&] {
            QStringList keys;
            for (const auto &line : QString::fromUtf8(text).split(QLatin1Char('\n'))) {
                if (line.size() > 2 && line.startsWith(QLatin1String("  "))
                    && line.at(2) == QLatin1Char('"')) {
                    const qsizetype close = line.indexOf(QLatin1Char('"'), 3);
                    if (close > 0)
                        keys.append(line.mid(3, close - 3));
                }
            }
            return keys;
        }();
        // left/right are passthrough (Workspace does not model sidedocks),
        // so they appear in their Obsidian slots but are not rewritten.
        QCOMPARE(order,
                 QStringList({QStringLiteral("main"), QStringLiteral("left"),
                              QStringLiteral("right"), QStringLiteral("active"),
                              QStringLiteral("lastOpenFiles")}));
    }
};

QTEST_MAIN(TestWorkspaceSerialize)
#include "tst_workspace_serialize.moc"
