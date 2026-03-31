// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QTemporaryDir>
#include "graph/GraphDataBuilder.h"
#include "corbomite/storage/SQLiteIndex.h"
#include "corbomite/models/VaultModel.h"

class TestGraphDataBuilder : public QObject {
    Q_OBJECT

    void createFile(const QString &path, const QString &content)
    {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile f(path);
        f.open(QIODevice::WriteOnly);
        f.write(content.toUtf8());
        f.close();
    }

private Q_SLOTS:
    void testGlobalGraphBasic()
    {
        QTemporaryDir tmp;
        QString vault = tmp.path() + "/vault";
        createFile(vault + "/A.md", "Links to [[B]]");
        createFile(vault + "/B.md", "Links to [[C]]");
        createFile(vault + "/C.md", "No links");

        Corbomite::VaultModel vaultModel;
        vaultModel.open(vault);

        Corbomite::SQLiteIndex index;
        index.open(tmp.path() + "/index.sqlite");
        index.rebuildIndex(vault);

        auto result = Corbomite::GraphDataBuilder::buildGlobalGraph(&index, &vaultModel);

        QCOMPARE(result.nodes.size(), 3);
        QCOMPARE(result.edges.size(), 2); // A->B, B->C
    }

    void testUnresolvedNodes()
    {
        QTemporaryDir tmp;
        QString vault = tmp.path() + "/vault";
        createFile(vault + "/A.md", "Links to [[NonExistent]]");

        Corbomite::VaultModel vaultModel;
        vaultModel.open(vault);

        Corbomite::SQLiteIndex index;
        index.open(tmp.path() + "/index.sqlite");
        index.rebuildIndex(vault);

        auto result = Corbomite::GraphDataBuilder::buildGlobalGraph(&index, &vaultModel);

        // A + NonExistent (unresolved)
        QCOMPARE(result.nodes.size(), 2);
        QCOMPARE(result.edges.size(), 1);

        // Find the unresolved node
        bool foundUnresolved = false;
        for (const auto &node : result.nodes) {
            if (node.id == QStringLiteral("NonExistent.md")) {
                QCOMPARE(node.color, QColor(136, 136, 136)); // Gray
                foundUnresolved = true;
            }
        }
        QVERIFY(foundUnresolved);
    }

    void testOrphanNode()
    {
        QTemporaryDir tmp;
        QString vault = tmp.path() + "/vault";
        createFile(vault + "/linked.md", "Has [[other]]");
        createFile(vault + "/other.md", "Target");
        createFile(vault + "/orphan.md", "No links at all");

        Corbomite::VaultModel vaultModel;
        vaultModel.open(vault);

        Corbomite::SQLiteIndex index;
        index.open(tmp.path() + "/index.sqlite");
        index.rebuildIndex(vault);

        auto result = Corbomite::GraphDataBuilder::buildGlobalGraph(&index, &vaultModel);

        QCOMPARE(result.nodes.size(), 3);

        // Orphan should have light gray color
        bool foundOrphan = false;
        for (const auto &node : result.nodes) {
            if (node.id == QStringLiteral("orphan.md")) {
                QCOMPARE(node.color, QColor(170, 170, 170));
                foundOrphan = true;
            }
        }
        QVERIFY(foundOrphan);
    }

    void testNodeRadiusScalesWithDegree()
    {
        QTemporaryDir tmp;
        QString vault = tmp.path() + "/vault";
        createFile(vault + "/hub.md", "Links to [[A]] and [[B]] and [[C]]");
        createFile(vault + "/A.md", "Just a note");
        createFile(vault + "/B.md", "Just a note");
        createFile(vault + "/C.md", "Just a note");

        Corbomite::VaultModel vaultModel;
        vaultModel.open(vault);

        Corbomite::SQLiteIndex index;
        index.open(tmp.path() + "/index.sqlite");
        index.rebuildIndex(vault);

        auto result = Corbomite::GraphDataBuilder::buildGlobalGraph(&index, &vaultModel);

        // Find hub node — should have larger radius than leaf nodes
        double hubRadius = 0, leafRadius = 0;
        for (const auto &node : result.nodes) {
            if (node.id == QStringLiteral("hub.md")) hubRadius = node.radius;
            if (node.id == QStringLiteral("A.md")) leafRadius = node.radius;
        }
        QVERIFY(hubRadius > leafRadius);
    }

    void testLocalGraph()
    {
        QTemporaryDir tmp;
        QString vault = tmp.path() + "/vault";
        createFile(vault + "/center.md", "Links to [[neighbor]]");
        createFile(vault + "/neighbor.md", "Links to [[far]]");
        createFile(vault + "/far.md", "Far away");
        createFile(vault + "/unrelated.md", "No connection");

        Corbomite::VaultModel vaultModel;
        vaultModel.open(vault);

        Corbomite::SQLiteIndex index;
        index.open(tmp.path() + "/index.sqlite");
        index.rebuildIndex(vault);

        // Depth 1: only center + direct neighbors
        auto result1 = Corbomite::GraphDataBuilder::buildLocalGraph(
            &index, &vaultModel, QStringLiteral("center.md"), 1);

        QCOMPARE(result1.nodes.size(), 2); // center + neighbor
        QCOMPARE(result1.edges.size(), 1);

        // Depth 2: center + neighbor + far
        auto result2 = Corbomite::GraphDataBuilder::buildLocalGraph(
            &index, &vaultModel, QStringLiteral("center.md"), 2);

        QCOMPARE(result2.nodes.size(), 3); // center + neighbor + far
        QCOMPARE(result2.edges.size(), 2);

        // "unrelated" should NOT be included at any depth
        for (const auto &node : result2.nodes) {
            QVERIFY(node.id != QStringLiteral("unrelated.md"));
        }
    }

    void testEmptyIndex()
    {
        QTemporaryDir tmp;
        QString vault = tmp.path() + "/vault";
        QDir().mkpath(vault);

        Corbomite::VaultModel vaultModel;
        vaultModel.open(vault);

        Corbomite::SQLiteIndex index;
        index.open(tmp.path() + "/index.sqlite");
        index.rebuildIndex(vault);

        auto result = Corbomite::GraphDataBuilder::buildGlobalGraph(&index, &vaultModel);
        QCOMPARE(result.nodes.size(), 0);
        QCOMPARE(result.edges.size(), 0);
    }
};

QTEST_MAIN(TestGraphDataBuilder)
#include "tst_graphdatabuilder.moc"
