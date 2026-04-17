// SPDX-License-Identifier: GPL-3.0-or-later
//
// Phase 8 rewrite: drives the index via MetadataCache (the deprecated
// SQLiteIndex::rebuildIndex was deleted in Phase 8). Each test writes notes
// under a QTemporaryDir, then calls `MetadataCache::rebuildVault(...)` and
// waits for `indexFinished` before invoking GraphDataBuilder.

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include "graph/GraphDataBuilder.h"
#include "corbomite/storage/FileSystemAdapter.h"
#include "corbomite/storage/LinkResolver.h"
#include "corbomite/storage/MetadataCache.h"
#include "corbomite/storage/SQLiteIndex.h"
#include "corbomite/storage/proxies/SearchProxy.h"
#include "corbomite/vault/Vault.h"
#include "corbomite/vault/proxies/VaultProxy.h"

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

    // Drive a MetadataCache rebuild of `vault` through `index` and wait for
    // indexFinished. Callers own the heap objects (stored on the stack frame).
    void indexVault(const QString &vault,
                    Corbomite::Vault &vaultObj,
                    Corbomite::LinkResolver &resolver,
                    Corbomite::MetadataCache &cache,
                    Corbomite::SQLiteIndex &index,
                    const QString &dbPath)
    {
        QStringList paths;
        for (Corbomite::TFile *f : vaultObj.getMarkdownFiles()) {
            paths.append(f->path);
        }
        resolver.setVaultPaths(paths);

        QVERIFY(index.open(dbPath));
        index.setVaultRoot(vault);
        index.setMetadataCache(&cache);

        QSignalSpy finishedSpy(&cache, &Corbomite::MetadataCache::indexFinished);
        cache.rebuildVault(vault, paths);
        if (!paths.isEmpty()) {
            QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), 1, 10000);
        }
    }

private Q_SLOTS:
    void testGlobalGraphBasic()
    {
        QTemporaryDir tmp;
        QString vault = tmp.path() + "/vault";
        createFile(vault + "/A.md", "Links to [[B]]");
        createFile(vault + "/B.md", "Links to [[C]]");
        createFile(vault + "/C.md", "No links");

        Corbomite::FileSystemAdapter fs;
        Corbomite::Vault vaultObj(&fs);
        vaultObj.load(vault);

        Corbomite::LinkResolver resolver;
        Corbomite::MetadataCache cache(resolver);
        Corbomite::SQLiteIndex index;
        indexVault(vault, vaultObj, resolver, cache, index, tmp.path() + "/index.sqlite");

        auto result = Corbomite::GraphDataBuilder::buildGlobalGraph(&index, &vaultObj);

        QCOMPARE(result.nodes.size(), 3);
        QCOMPARE(result.edges.size(), 2); // A->B, B->C
    }

    void testUnresolvedNodes()
    {
        QTemporaryDir tmp;
        QString vault = tmp.path() + "/vault";
        createFile(vault + "/A.md", "Links to [[NonExistent]]");

        Corbomite::FileSystemAdapter fs;
        Corbomite::Vault vaultObj(&fs);
        vaultObj.load(vault);

        Corbomite::LinkResolver resolver;
        Corbomite::MetadataCache cache(resolver);
        Corbomite::SQLiteIndex index;
        indexVault(vault, vaultObj, resolver, cache, index, tmp.path() + "/index.sqlite");

        auto result = Corbomite::GraphDataBuilder::buildGlobalGraph(&index, &vaultObj);

        // A + NonExistent (unresolved)
        QCOMPARE(result.nodes.size(), 2);
        QCOMPARE(result.edges.size(), 1);

        // Find the unresolved node. Phase 8: unresolved targets come back
        // bare (LinkResolver returns empty path), so the stored target is
        // the raw linktext without a ".md" suffix.
        bool foundUnresolved = false;
        for (const auto &node : result.nodes) {
            if (node.type == ForceGraph::NodeType::Unresolved) {
                foundUnresolved = true;
                QVERIFY(node.id.contains(QStringLiteral("NonExistent")));
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

        Corbomite::FileSystemAdapter fs;
        Corbomite::Vault vaultObj(&fs);
        vaultObj.load(vault);

        Corbomite::LinkResolver resolver;
        Corbomite::MetadataCache cache(resolver);
        Corbomite::SQLiteIndex index;
        indexVault(vault, vaultObj, resolver, cache, index, tmp.path() + "/index.sqlite");

        auto result = Corbomite::GraphDataBuilder::buildGlobalGraph(&index, &vaultObj);

        QCOMPARE(result.nodes.size(), 3);

        // Orphan should be typed as Orphan
        bool foundOrphan = false;
        for (const auto &node : result.nodes) {
            if (node.id == QStringLiteral("orphan.md")) {
                QCOMPARE(node.type, ForceGraph::NodeType::Orphan);
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

        Corbomite::FileSystemAdapter fs;
        Corbomite::Vault vaultObj(&fs);
        vaultObj.load(vault);

        Corbomite::LinkResolver resolver;
        Corbomite::MetadataCache cache(resolver);
        Corbomite::SQLiteIndex index;
        indexVault(vault, vaultObj, resolver, cache, index, tmp.path() + "/index.sqlite");

        auto result = Corbomite::GraphDataBuilder::buildGlobalGraph(&index, &vaultObj);

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

        Corbomite::FileSystemAdapter fs;
        Corbomite::Vault vaultObj(&fs);
        vaultObj.load(vault);

        Corbomite::LinkResolver resolver;
        Corbomite::MetadataCache cache(resolver);
        Corbomite::SQLiteIndex index;
        indexVault(vault, vaultObj, resolver, cache, index, tmp.path() + "/index.sqlite");

        // Depth 1: only center + direct neighbors
        auto result1 = Corbomite::GraphDataBuilder::buildLocalGraph(
            &index, &vaultObj, QStringLiteral("center.md"), 1);

        QCOMPARE(result1.nodes.size(), 2); // center + neighbor
        QCOMPARE(result1.edges.size(), 1);

        // Depth 2: center + neighbor + far
        auto result2 = Corbomite::GraphDataBuilder::buildLocalGraph(
            &index, &vaultObj, QStringLiteral("center.md"), 2);

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

        Corbomite::FileSystemAdapter fs;
        Corbomite::Vault vaultObj(&fs);
        vaultObj.load(vault);

        Corbomite::LinkResolver resolver;
        Corbomite::MetadataCache cache(resolver);
        Corbomite::SQLiteIndex index;
        indexVault(vault, vaultObj, resolver, cache, index, tmp.path() + "/index.sqlite");

        auto result = Corbomite::GraphDataBuilder::buildGlobalGraph(&index, &vaultObj);
        QCOMPARE(result.nodes.size(), 0);
        QCOMPARE(result.edges.size(), 0);
    }

    // ---- Cluster N proxy-typed overload parity ----

    // QSet of node ids (paths) for ordering-insensitive comparison
    static QSet<QString> nodeIdSet(const Corbomite::GraphDataBuilder::Result &r)
    {
        QSet<QString> s;
        for (const auto &n : r.nodes) s.insert(n.id);
        return s;
    }

    // QSet of (sourceId, targetId) pairs for ordering-insensitive comparison
    static QSet<QPair<QString, QString>> edgeSet(const Corbomite::GraphDataBuilder::Result &r)
    {
        QSet<QPair<QString, QString>> s;
        for (const auto &e : r.edges) s.insert({e.sourceId, e.targetId});
        return s;
    }

    void testGlobalGraphProxyOverloadMatchesRaw()
    {
        QTemporaryDir tmp;
        QString vault = tmp.path() + "/vault";
        createFile(vault + "/A.md", "Links to [[B]] and [[C]]");
        createFile(vault + "/B.md", "Links to [[C]] and [[Unresolved]]");
        createFile(vault + "/C.md", "No links");
        createFile(vault + "/orphan.md", "Nothing here");

        Corbomite::FileSystemAdapter fs;
        Corbomite::Vault vaultObj(&fs);
        vaultObj.load(vault);

        Corbomite::LinkResolver resolver;
        Corbomite::MetadataCache cache(resolver);
        Corbomite::SQLiteIndex index;
        indexVault(vault, vaultObj, resolver, cache, index, tmp.path() + "/index.sqlite");

        auto raw = Corbomite::GraphDataBuilder::buildGlobalGraph(&index, &vaultObj);

        QSet<QString> granted = { QStringLiteral("vault.read"),
                                  QStringLiteral("metadata.read") };
        Corbomite::VaultProxy  vaultProxy(&vaultObj, granted, QStringLiteral("t"));
        Corbomite::SearchProxy searchProxy(&index, granted, QStringLiteral("t"));

        auto proxied = Corbomite::GraphDataBuilder::buildGlobalGraph(&searchProxy, &vaultProxy);

        QCOMPARE(proxied.nodes.size(), raw.nodes.size());
        QCOMPARE(proxied.edges.size(), raw.edges.size());
        QCOMPARE(nodeIdSet(proxied), nodeIdSet(raw));
        QCOMPARE(edgeSet(proxied), edgeSet(raw));
    }

    void testLocalGraphProxyOverloadMatchesRaw()
    {
        QTemporaryDir tmp;
        QString vault = tmp.path() + "/vault";
        createFile(vault + "/center.md", "Links to [[neighbor]]");
        createFile(vault + "/neighbor.md", "Links to [[far]]");
        createFile(vault + "/far.md", "Far away");
        createFile(vault + "/unrelated.md", "No connection");

        Corbomite::FileSystemAdapter fs;
        Corbomite::Vault vaultObj(&fs);
        vaultObj.load(vault);

        Corbomite::LinkResolver resolver;
        Corbomite::MetadataCache cache(resolver);
        Corbomite::SQLiteIndex index;
        indexVault(vault, vaultObj, resolver, cache, index, tmp.path() + "/index.sqlite");

        QSet<QString> granted = { QStringLiteral("vault.read"),
                                  QStringLiteral("metadata.read") };
        Corbomite::VaultProxy  vaultProxy(&vaultObj, granted, QStringLiteral("t"));
        Corbomite::SearchProxy searchProxy(&index, granted, QStringLiteral("t"));

        // Depth 1
        auto raw1 = Corbomite::GraphDataBuilder::buildLocalGraph(
            &index, &vaultObj, QStringLiteral("center.md"), 1);
        auto proxied1 = Corbomite::GraphDataBuilder::buildLocalGraph(
            &searchProxy, &vaultProxy, QStringLiteral("center.md"), 1);

        QCOMPARE(proxied1.nodes.size(), raw1.nodes.size());
        QCOMPARE(proxied1.edges.size(), raw1.edges.size());
        QCOMPARE(nodeIdSet(proxied1), nodeIdSet(raw1));
        QCOMPARE(edgeSet(proxied1), edgeSet(raw1));

        // Depth 2
        auto raw2 = Corbomite::GraphDataBuilder::buildLocalGraph(
            &index, &vaultObj, QStringLiteral("center.md"), 2);
        auto proxied2 = Corbomite::GraphDataBuilder::buildLocalGraph(
            &searchProxy, &vaultProxy, QStringLiteral("center.md"), 2);

        QCOMPARE(proxied2.nodes.size(), raw2.nodes.size());
        QCOMPARE(proxied2.edges.size(), raw2.edges.size());
        QCOMPARE(nodeIdSet(proxied2), nodeIdSet(raw2));
        QCOMPARE(edgeSet(proxied2), edgeSet(raw2));
    }
};

QTEST_MAIN(TestGraphDataBuilder)
#include "tst_graphdatabuilder.moc"
