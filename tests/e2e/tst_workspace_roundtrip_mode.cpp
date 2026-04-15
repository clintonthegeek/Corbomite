// SPDX-License-Identifier: GPL-3.0-or-later
//
// Cluster E Phase 7 — workspace.json end-to-end round-trip test.
//
// Exercises the full on-disk round-trip via WorkspaceState +
// PaneLayout + EphemeralState: build an in-memory workspace with two
// leaves (Source + Reading), serialise to a temp workspace.json, reload,
// assert that mode + eState.scroll + foldedHeadings + Cluster B's
// unknown-key preservation all survive a full round-trip.
//
// Runs headless via QTemporaryDir — no GUI needed.

#include "corbomite/core/PaneLayout.h"
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
using Corbomite::PaneLayout;
using Corbomite::PaneLayoutIndex;
using Corbomite::PaneLeaf;
using Corbomite::WorkspaceState;

namespace {

QJsonObject readFileObject(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    const auto doc = QJsonDocument::fromJson(f.readAll());
    return doc.object();
}

/// Scan the `main` SplitNode recursively and return every `leaf` node
/// whose state.type == "markdown" (for test inspection).
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

} // namespace

class WorkspaceRoundtripModeTest : public QObject {
    Q_OBJECT

private Q_SLOTS:

    void endToEndRoundTrip()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString path = tmp.path() + QStringLiteral("/workspace.json");

        // Build a PaneLayout with two leaves (different modes + eState).
        PaneLayout layout;
        auto *root = layout.root();

        PaneLeaf sourceLeaf;
        sourceLeaf.id = QStringLiteral("leaf-source-aaaa");
        sourceLeaf.viewType = QStringLiteral("markdown");
        sourceLeaf.filePath = QStringLiteral("notes/one.md");
        sourceLeaf.mode = QStringLiteral("source");
        {
            // Build the viewState.state inner object.
            QJsonObject viewState;
            viewState.insert(QStringLiteral("type"), QStringLiteral("markdown"));
            QJsonObject inner;
            inner.insert(QStringLiteral("file"), sourceLeaf.filePath);
            inner.insert(QStringLiteral("mode"), QStringLiteral("source"));
            viewState.insert(QStringLiteral("state"), inner);
            sourceLeaf.viewState = viewState;

            // Build and attach eState as leaf-level unknown key.
            EphemeralState es;
            es.modeRaw = QStringLiteral("source");
            es.sourceFlag = true;
            es.scroll = 30.5f;
            es.cursor.line = 12;
            es.cursor.column = 4;
            es.foldedHeadings = QVector<int>{42};
            sourceLeaf.unknown.insert(QStringLiteral("eState"), es.toJson());

            // Preserve a fabricated Obsidian-internal key to verify Cluster B's
            // unknown-key preservation still works.
            sourceLeaf.unknown.insert(QStringLiteral("_obsidian_internal"),
                                      QJsonObject{{QStringLiteral("opaque"), 17}});
        }

        PaneLeaf readingLeaf;
        readingLeaf.id = QStringLiteral("leaf-reading-bbbb");
        readingLeaf.viewType = QStringLiteral("markdown");
        readingLeaf.filePath = QStringLiteral("notes/two.md");
        readingLeaf.mode = QStringLiteral("preview");
        {
            QJsonObject viewState;
            viewState.insert(QStringLiteral("type"), QStringLiteral("markdown"));
            QJsonObject inner;
            inner.insert(QStringLiteral("file"), readingLeaf.filePath);
            inner.insert(QStringLiteral("mode"), QStringLiteral("preview"));
            viewState.insert(QStringLiteral("state"), inner);
            readingLeaf.viewState = viewState;

            EphemeralState es;
            es.modeRaw = QStringLiteral("preview");
            es.sourceFlag = false;
            es.scroll = 5.0f;
            readingLeaf.unknown.insert(QStringLiteral("eState"), es.toJson());
        }

        // Split root: first child is the source-leaf tabs, second child is
        // the reading-leaf tabs.
        root->addView(sourceLeaf);
        root->splitWithNewLeaf(readingLeaf, Qt::Horizontal);

        // Serialise into a WorkspaceState + save.
        WorkspaceState ws;
        QJsonObject rootObj = ws.raw();
        rootObj.insert(QStringLiteral("main"), layout.toJson());
        rootObj.insert(QStringLiteral("lastOpenFiles"),
                       QJsonArray{QStringLiteral("notes/one.md"),
                                  QStringLiteral("notes/two.md")});
        ws.setRaw(rootObj);

        FileSystemAdapter fs;
        QVERIFY(ws.save(&fs, path));

        // --- Inspect the raw bytes on disk. ---
        const auto onDisk = readFileObject(path);
        QVERIFY(!onDisk.isEmpty());

        const auto leaves = collectMarkdownLeaves(
            onDisk.value(QStringLiteral("main")).toObject());
        QCOMPARE(leaves.size(), 2);

        // Identify each leaf by its file path. Order depends on traversal but
        // by construction we built source first, reading second.
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

        // Source leaf — mode "source" + eState.scroll ≈ 30.5 + eState.source:true.
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

        // Reading leaf — mode "preview" + eState.scroll ≈ 5.0.
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

        auto reloadedLayout = PaneLayout::fromJson(reloaded->main());
        auto *backSource = reloadedLayout.findLeaf(QStringLiteral("leaf-source-aaaa"));
        QVERIFY(backSource);
        QCOMPARE(backSource->filePath, QStringLiteral("notes/one.md"));
        QCOMPARE(backSource->mode, QStringLiteral("source"));
        auto backSourceEState = EphemeralState::fromJson(
            backSource->unknown.value(QStringLiteral("eState")).toObject());
        QCOMPARE(backSourceEState.modeRaw, QStringLiteral("source"));
        QCOMPARE(backSourceEState.sourceFlag, true);
        QVERIFY(std::abs(backSourceEState.scroll - 30.5f) < 0.01f);
        QCOMPARE(backSourceEState.cursor.line, 12);
        QCOMPARE(backSourceEState.cursor.column, 4);
        QCOMPARE(backSourceEState.foldedHeadings, (QVector<int>{42}));

        auto *backReading = reloadedLayout.findLeaf(QStringLiteral("leaf-reading-bbbb"));
        QVERIFY(backReading);
        QCOMPARE(backReading->filePath, QStringLiteral("notes/two.md"));
        QCOMPARE(backReading->mode, QStringLiteral("preview"));
        auto backReadingEState = EphemeralState::fromJson(
            backReading->unknown.value(QStringLiteral("eState")).toObject());
        QCOMPARE(backReadingEState.modeRaw, QStringLiteral("preview"));
        QVERIFY(std::abs(backReadingEState.scroll - 5.0f) < 0.01f);

        // Obsidian-internal unknown still present after reload.
        QCOMPARE(backSource->unknown.value(QStringLiteral("_obsidian_internal"))
                                     .toObject().value(QStringLiteral("opaque")).toInt(),
                 17);
    }
};

QTEST_MAIN(WorkspaceRoundtripModeTest)
#include "tst_workspace_roundtrip_mode.moc"
