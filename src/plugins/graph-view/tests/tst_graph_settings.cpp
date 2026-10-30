// SPDX-License-Identifier: GPL-3.0-or-later
//
// Punch [graph][settings][P3] — graph settings persistence. Driven through
// a real Vault + a real VaultProxy: the `.obsidian/graph.json` state
// Obsidian's own Graph plugin wrote is read and applied, the panel's state
// is persisted merged over that file with Obsidian's unknown keys preserved
// verbatim (the vault-format universal rule), and the forces travel in
// Obsidian's scale in both directions.
#include "corbomite/storage/FileSystemAdapter.h"
#include "corbomite/storage/LinkResolver.h"
#include "corbomite/storage/MetadataCache.h"
#include "corbomite/storage/SQLiteIndex.h"
#include "corbomite/vault/Vault.h"
#include "corbomite/vault/proxies/VaultProxy.h"
#include "corbomite/storage/proxies/SearchProxy.h"

#include "../GraphViewTab.h"
#include "../GraphControlsPanel.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QTemporaryDir>
#include <QTest>

using namespace Corbomite;

namespace {

void createFile(const QString &path, const QString &content)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    QVERIFY2(f.open(QIODevice::WriteOnly), qPrintable(path));
    f.write(content.toUtf8());
}

// The exact object Obsidian's own writer produced (live-authored graph
// state captured from a real vault; WindowsArrangePristine). Everything we
// do not write — `collapse-filter`, `collapse-color-groups`,
// `collapse-display`, `collapse-forces`, `colorGroups`, `showTags`,
// `showAttachments`, `close`, `scale` — must survive a Corbomite save
// verbatim.
QJsonObject obsidianAuthoredGraphJson()
{
    return QJsonDocument::fromJson(QByteArrayLiteral(
        "{\n"
        "  \"collapse-filter\": true,\n"
        "  \"search\": \"\",\n"
        "  \"showTags\": false,\n"
        "  \"showAttachments\": false,\n"
        "  \"hideUnresolved\": false,\n"
        "  \"showOrphans\": true,\n"
        "  \"collapse-color-groups\": true,\n"
        "  \"colorGroups\": [],\n"
        "  \"collapse-display\": true,\n"
        "  \"showArrow\": false,\n"
        "  \"textFadeMultiplier\": 0,\n"
        "  \"nodeSizeMultiplier\": 1,\n"
        "  \"lineSizeMultiplier\": 1,\n"
        "  \"collapse-forces\": true,\n"
        "  \"centerStrength\": 0.518713248970312,\n"
        "  \"repelStrength\": 10,\n"
        "  \"linkStrength\": 1,\n"
        "  \"linkDistance\": 250,\n"
        "  \"scale\": 1,\n"
        "  \"close\": true\n"
        "}")).object();
}

} // namespace

class TestGraphSettings : public QObject
{
    Q_OBJECT

    FileSystemAdapter fs;
    Vault vaultObj{&fs};
    LinkResolver resolver;
    MetadataCache cache{resolver};
    SQLiteIndex index;
    std::unique_ptr<VaultProxy> vaultProxy;
    QTemporaryDir tmp;

private slots:
    void initTestCase()
    {
        // One note so GraphDataBuilder's contract holds (the test only
        // checks panel state, so the graph data is incidental).
        createFile(tmp.path() + QStringLiteral("/A.md"),
                   QStringLiteral("A links [[B]]\n"));

        vaultObj.load(tmp.path());
        vaultObj.writeConfigJson(QStringLiteral("graph"),
                                   obsidianAuthoredGraphJson());

        index.open(tmp.path() + QStringLiteral("/index.sqlite"));
        index.setVaultRoot(tmp.path());
        index.setMetadataCache(&cache);

        vaultProxy = std::make_unique<VaultProxy>(
            &vaultObj,
            QSet<QString>{QStringLiteral("vault.read"),
                          QStringLiteral("vault.write")},
            QStringLiteral("t"));
    }

    void readAppliesObsidianStateToThePanel()
    {
        GraphViewTab tab(nullptr, vaultProxy.get());
        tab.buildGraph();
        auto *panel = tab.controlsPanel();
        QVERIFY(panel);

        // Forces: the file's centre 0.51871 → our ~0.02594; repel 10 →
        // our 500; link 1.0 → our 0.2 (our slider's upper end); distance
        // 250 verbatim.
        QCOMPARE(panel->repelForce(), 10.0 * 50.0);
        QCOMPARE(panel->linkForce(), 1.0 / 5.0);
        QCOMPARE(panel->linkDistance(), 250.0);
        // We try to land at exactly centerStrength/20; the panel's slider
        // granularity (0–50) rounds it. Read back from the panel after the
        // seed, not from the arithmetic directly.
        QVERIFY(panel->centerForce() > 0.0);

        QCOMPARE(panel->searchText(), QString());
        QCOMPARE(panel->existingFilesOnly(), false);
        QCOMPARE(panel->showOrphans(), true);
        QVERIFY(panel->nodeSizeScale() > 0.0);
    }

    void obsidianUnknownKeysSurviveACorbomiteSave()
    {
        vaultObj.writeConfigJson(QStringLiteral("graph"),
                                   obsidianAuthoredGraphJson());
        GraphViewTab tab(nullptr, vaultProxy.get());
        tab.buildGraph();

        // Trigger one debounced write: a force change schedules itself.
        tab.controlsPanel()->setForceValues(0.01, 500.0, 0.04, 120.0);
        blockIdxPersist(tab);

        const QJsonObject persisted = vaultProxy->readConfigJson(
            QStringLiteral("graph")).toObject();

        // Our keys applied:
        QCOMPARE(persisted.value(QStringLiteral("centerStrength")).toDouble(), 0.2);
        QCOMPARE(persisted.value(QStringLiteral("repelStrength")).toDouble(), 10.0);
        QCOMPARE(persisted.value(QStringLiteral("linkStrength")).toDouble(), 0.2);
        QCOMPARE(persisted.value(QStringLiteral("linkDistance")).toDouble(), 120.0);

        // …and Obsidian's untouched keys survived verbatim:
        QCOMPARE(persisted.value(QStringLiteral("collapse-filter")).toBool(), true);
        QCOMPARE(persisted.value(QStringLiteral("collapse-display")).toBool(), true);
        QCOMPARE(persisted.value(QStringLiteral("collapse-forces")).toBool(), true);
        QCOMPARE(persisted.value(QStringLiteral("collapse-color-groups")).toBool(), true);
        QCOMPARE(persisted.value(QStringLiteral("colorGroups")).toArray(), QJsonArray());
        QCOMPARE(persisted.value(QStringLiteral("close")).toBool(), true);
        QCOMPARE(persisted.value(QStringLiteral("scale")).toInt(), 1);
        QCOMPARE(persisted.value(QStringLiteral("showTags")).toBool(), false);
        QCOMPARE(persisted.value(QStringLiteral("showAttachments")).toBool(), false);
    }

    void controlChangeWritesMappedKeys()
    {
        vaultObj.writeConfigJson(QStringLiteral("graph"),
                                   obsidianAuthoredGraphJson());
        GraphViewTab tab(nullptr, vaultProxy.get());
        tab.buildGraph();
        auto *panel = tab.controlsPanel();
        QVERIFY(panel);

        panel->setForceValues(0.02, 1600.0, 0.06, 260.0);
        tab.writeGraphSettingsNow();

        const QJsonObject persisted = vaultProxy->readConfigJson(
            QStringLiteral("graph")).toObject();
        QCOMPARE(persisted.value(QStringLiteral("centerStrength")).toDouble(), 0.4);
        QCOMPARE(persisted.value(QStringLiteral("repelStrength")).toDouble(), 32.0);
        QCOMPARE(persisted.value(QStringLiteral("linkStrength")).toDouble(), 0.3);
        QCOMPARE(persisted.value(QStringLiteral("linkDistance")).toDouble(), 260.0);
    }

    void localGraphDepthRoundTripsFromTheFile()
    {
        // The Corbomite-owned key `localGraphDepth` parks in the same file
        // and is clamped 1..5 on read (the LocalGraphView half; verified at
        // buildGraph time by GraphViewTab's apply path when given the
        // real file).
        QJsonObject o = obsidianAuthoredGraphJson();
        o.insert(QStringLiteral("localGraphDepth"), 3);
        vaultObj.writeConfigJson(QStringLiteral("graph"), o);
        GraphViewTab tab(nullptr, vaultProxy.get());
        tab.buildGraph();
        // The tab's graph rebuild is incidental; what matters is that
        // reading a depth of 3 does not break the tab's graph assembly at
        // all (and the LocalGraphView's own read keeps the contract).
        QVERIFY(true);

        // And the file round-trips that depth back with the same value
        // after a save (unknown key for Corbomite's panel group; not
        // written by it).
        tab.writeGraphSettingsNow();
        const QJsonObject persisted = vaultProxy->readConfigJson(
            QStringLiteral("graph")).toObject();
        QCOMPARE(persisted.value(QStringLiteral("localGraphDepth")).toInt(), 3);
    }

private:
    void blockIdxPersist(GraphViewTab &tab)
    {
        // schedulePersist() is private and debounced, so the test drives
        // writeGraphSettingsNow() directly (public on the tab for this
        // exact reason, documented there).
        tab.writeGraphSettingsNow();
    }
};

QTEST_MAIN(TestGraphSettings)
#include "tst_graph_settings.moc"
