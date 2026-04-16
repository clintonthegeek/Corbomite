// SPDX-License-Identifier: GPL-3.0-or-later
//
// Cluster E Phase 7 — workspace.json end-to-end round-trip test.
//
// Exercises the full on-disk round-trip: build an in-memory workspace JSON
// with two leaves (Source + Reading), serialise to a temp workspace.json,
// reload, assert that mode + eState.scroll + foldedHeadings + unknown-key
// preservation all survive a full round-trip.
//
// Runs headless via QTemporaryDir — no GUI needed.

#include "corbomite/storage/EphemeralState.h"
#include "corbomite/storage/FileSystemAdapter.h"
#include "corbomite/storage/WorkspaceState.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>
#include <QTemporaryDir>
#include <QTest>

using Corbomite::EphemeralState;
using Corbomite::FileSystemAdapter;
using Corbomite::WorkspaceState;

namespace {

QJsonObject readFileObject(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    const auto doc = QJsonDocument::fromJson(f.readAll());
    return doc.object();
}

QList<QJsonObject> collectMarkdownLeaves(const QJsonObject &node)
{
    QList<QJsonObject> out;
    const QString type = node.value(QStringLiteral("type")).toString();
    if (type == QStringLiteral("leaf")) {
        const auto state = node.value(QStringLiteral("state")).toObject();
        if (state.value(QStringLiteral("type")).toString()
                == QStringLiteral("markdown")) {
            out.append(node);
        }
        return out;
    }
    const auto kids = node.value(QStringLiteral("children")).toArray();
    for (const auto &c : kids) {
        if (c.isObject()) out.append(collectMarkdownLeaves(c.toObject()));
    }
    return out;
}

QJsonObject makeLeafJson(const QString &id, const QString &filePath,
                         const QString &mode, const QJsonObject &eState,
                         const QJsonObject &extraUnknown = {})
{
    QJsonObject inner;
    inner[QStringLiteral("file")] = filePath;
    inner[QStringLiteral("mode")] = mode;

    QJsonObject viewState;
    viewState[QStringLiteral("type")] = QStringLiteral("markdown");
    viewState[QStringLiteral("state")] = inner;

    QJsonObject leaf;
    leaf[QStringLiteral("id")] = id;
    leaf[QStringLiteral("type")] = QStringLiteral("leaf");
    leaf[QStringLiteral("state")] = viewState;
    if (!eState.isEmpty())
        leaf[QStringLiteral("eState")] = eState;
    for (auto it = extraUnknown.begin(); it != extraUnknown.end(); ++it)
        leaf.insert(it.key(), it.value());
    return leaf;
}

} // namespace

class WorkspaceRoundtripModeTest : public QObject {
    Q_OBJECT

private Q_SLOTS:

    void endToEndRoundTrip()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString path = tmp.path() + QStringLiteral("/workspace.json");

        // Build eState for the source leaf.
        EphemeralState sourceES;
        sourceES.modeRaw = QStringLiteral("source");
        sourceES.sourceFlag = true;
        sourceES.scroll = 30.5f;
        sourceES.cursor.line = 12;
        sourceES.cursor.column = 4;
        sourceES.foldedHeadings = QVector<int>{42};

        QJsonObject obsidianInternal;
        obsidianInternal[QStringLiteral("opaque")] = 17;
        QJsonObject sourceExtra;
        sourceExtra[QStringLiteral("_obsidian_internal")] = obsidianInternal;

        auto sourceLeafJson = makeLeafJson(
            QStringLiteral("leaf-source-aaaa"),
            QStringLiteral("notes/one.md"),
            QStringLiteral("source"),
            sourceES.toJson(),
            sourceExtra);

        // Build eState for the reading leaf.
        EphemeralState readingES;
        readingES.modeRaw = QStringLiteral("preview");
        readingES.sourceFlag = false;
        readingES.scroll = 5.0f;

        auto readingLeafJson = makeLeafJson(
            QStringLiteral("leaf-reading-bbbb"),
            QStringLiteral("notes/two.md"),
            QStringLiteral("preview"),
            readingES.toJson());

        // Build workspace JSON: split root with two tabs children.
        QJsonObject sourceTabs;
        sourceTabs[QStringLiteral("type")] = QStringLiteral("tabs");
        sourceTabs[QStringLiteral("children")] = QJsonArray{sourceLeafJson};
        sourceTabs[QStringLiteral("currentTab")] = 0;

        QJsonObject readingTabs;
        readingTabs[QStringLiteral("type")] = QStringLiteral("tabs");
        readingTabs[QStringLiteral("children")] = QJsonArray{readingLeafJson};
        readingTabs[QStringLiteral("currentTab")] = 0;

        QJsonObject mainSplit;
        mainSplit[QStringLiteral("type")] = QStringLiteral("split");
        mainSplit[QStringLiteral("direction")] = QStringLiteral("horizontal");
        mainSplit[QStringLiteral("children")] = QJsonArray{sourceTabs, readingTabs};

        // Serialise via WorkspaceState.
        WorkspaceState ws;
        QJsonObject rootObj = ws.raw();
        rootObj[QStringLiteral("main")] = mainSplit;
        rootObj[QStringLiteral("lastOpenFiles")] =
            QJsonArray{QStringLiteral("notes/one.md"),
                       QStringLiteral("notes/two.md")};
        ws.setRaw(rootObj);

        FileSystemAdapter fs;
        QVERIFY(ws.save(&fs, path));

        // --- Inspect the raw bytes on disk. ---
        const auto onDisk = readFileObject(path);
        QVERIFY(!onDisk.isEmpty());

        const auto leaves = collectMarkdownLeaves(
            onDisk.value(QStringLiteral("main")).toObject());
        QCOMPARE(leaves.size(), 2);

        QJsonObject sourceRaw, readingRaw;
        for (const auto &l : leaves) {
            const auto inner = l.value(QStringLiteral("state")).toObject()
                                .value(QStringLiteral("state")).toObject();
            if (inner.value(QStringLiteral("file")).toString()
                    == QStringLiteral("notes/one.md")) sourceRaw = l;
            else if (inner.value(QStringLiteral("file")).toString()
                    == QStringLiteral("notes/two.md")) readingRaw = l;
        }
        QVERIFY(!sourceRaw.isEmpty());
        QVERIFY(!readingRaw.isEmpty());

        // Source leaf — mode "source" + eState.scroll ~ 30.5 + eState.source:true.
        const auto sourceInner = sourceRaw.value(QStringLiteral("state"))
                                          .toObject().value(QStringLiteral("state"))
                                          .toObject();
        QCOMPARE(sourceInner.value(QStringLiteral("mode")).toString(),
                 QStringLiteral("source"));

        const auto sourceEState = sourceRaw.value(QStringLiteral("eState")).toObject();
        QVERIFY(!sourceEState.isEmpty());
        QCOMPARE(sourceEState.value(QStringLiteral("mode")).toString(),
                 QStringLiteral("source"));
        QCOMPARE(sourceEState.value(QStringLiteral("source")).toBool(), true);
        QVERIFY(std::abs(sourceEState.value(QStringLiteral("scroll")).toDouble()
                         - 30.5) < 0.01);

        // Reading leaf — mode "preview" + eState.scroll ~ 5.0.
        const auto readingInner = readingRaw.value(QStringLiteral("state"))
                                            .toObject().value(QStringLiteral("state"))
                                            .toObject();
        QCOMPARE(readingInner.value(QStringLiteral("mode")).toString(),
                 QStringLiteral("preview"));

        const auto readingEState = readingRaw.value(QStringLiteral("eState")).toObject();
        QVERIFY(!readingEState.isEmpty());
        QCOMPARE(readingEState.value(QStringLiteral("mode")).toString(),
                 QStringLiteral("preview"));
        QVERIFY(std::abs(readingEState.value(QStringLiteral("scroll")).toDouble()
                         - 5.0) < 0.01);

        // Cluster B unknown-key preservation: _obsidian_internal survived.
        QCOMPARE(sourceRaw.value(QStringLiteral("_obsidian_internal"))
                          .toObject().value(QStringLiteral("opaque")).toInt(),
                 17);

        // --- Reload + re-verify the typed values come back. ---
        const auto reloaded = WorkspaceState::load(&fs, path);
        QVERIFY(reloaded.has_value());

        const auto reloadedMain = reloaded->main();
        const auto reloadedLeaves = collectMarkdownLeaves(reloadedMain);
        QCOMPARE(reloadedLeaves.size(), 2);

        QJsonObject backSource, backReading;
        for (const auto &l : reloadedLeaves) {
            const auto inner = l.value(QStringLiteral("state")).toObject()
                                .value(QStringLiteral("state")).toObject();
            if (inner.value(QStringLiteral("file")).toString()
                    == QStringLiteral("notes/one.md")) backSource = l;
            else if (inner.value(QStringLiteral("file")).toString()
                    == QStringLiteral("notes/two.md")) backReading = l;
        }
        QVERIFY(!backSource.isEmpty());
        QVERIFY(!backReading.isEmpty());

        // Source leaf round-trip.
        auto backSourceEState = EphemeralState::fromJson(
            backSource.value(QStringLiteral("eState")).toObject());
        QCOMPARE(backSourceEState.modeRaw, QStringLiteral("source"));
        QCOMPARE(backSourceEState.sourceFlag, true);
        QVERIFY(std::abs(backSourceEState.scroll - 30.5f) < 0.01f);
        QCOMPARE(backSourceEState.cursor.line, 12);
        QCOMPARE(backSourceEState.cursor.column, 4);
        QCOMPARE(backSourceEState.foldedHeadings, (QVector<int>{42}));

        // Reading leaf round-trip.
        auto backReadingEState = EphemeralState::fromJson(
            backReading.value(QStringLiteral("eState")).toObject());
        QCOMPARE(backReadingEState.modeRaw, QStringLiteral("preview"));
        QVERIFY(std::abs(backReadingEState.scroll - 5.0f) < 0.01f);

        // Obsidian-internal unknown still present after reload.
        QCOMPARE(backSource.value(QStringLiteral("_obsidian_internal"))
                           .toObject().value(QStringLiteral("opaque")).toInt(),
                 17);
    }
};

QTEST_MAIN(WorkspaceRoundtripModeTest)
#include "tst_workspace_roundtrip_mode.moc"
