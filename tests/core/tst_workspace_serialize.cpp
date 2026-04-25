// tests/core/tst_workspace_serialize.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QTemporaryDir>
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
};

QTEST_MAIN(TestWorkspaceSerialize)
#include "tst_workspace_serialize.moc"
