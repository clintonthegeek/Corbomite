// SPDX-License-Identifier: GPL-3.0-or-later
//
// Canvas persistence — two live findings from 2026-10-18, both pinned here:
//
//  1. Edits were written only when the view unloaded, so anything in between
//     was lost to a crash, and the close-time write put a stale in-memory
//     document back over the file. A mutation must now reach disk on its own
//     (debounced autosave, the same 2 s default as TextFileView).
//  2. An external edit to an open `.canvas` was never noticed. The policy is
//     deliberately *not* Obsidian's: their canvas view sets
//     `isPlaintext = false`, which skips both the merge and the "File changed"
//     notice and quietly replaces the view with the file even when the view
//     has unsaved edits (`app/own/2028-n9.js:805`, `app/own/1813-CZ.js:83-92`).
//     We adopt the file only when nothing is unsaved, and otherwise keep the
//     local edits and tell the user.
//
// Runs under QT_QPA_PLATFORM=offscreen.

#include <QTest>

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include "corbomite/canvas/CanvasDocument.h"
#include "corbomite/canvas/CanvasScene.h"
#include "canvas/CanvasViewTab.h"
#include "corbomitesettings.h"

using Corbomite::Canvas::CanvasDocument;
using Corbomite::Canvas::CanvasNode;
using Corbomite::Canvas::NodeType;
using Corbomite::CanvasViewTab;

namespace {

QByteArray canvasBytes(std::initializer_list<QPair<QString, double>> nodes)
{
    QJsonArray array;
    for (const auto &[id, x] : nodes) {
        array.append(QJsonObject{
            {QStringLiteral("id"), id},
            {QStringLiteral("type"), QStringLiteral("file")},
            {QStringLiteral("file"), QStringLiteral("note.md")},
            {QStringLiteral("x"), x},
            {QStringLiteral("y"), 0.0},
            {QStringLiteral("width"), 200.0},
            {QStringLiteral("height"), 100.0},
        });
    }
    const QJsonObject root{
        {QStringLiteral("nodes"), array},
        {QStringLiteral("edges"), QJsonArray{}},
    };
    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

/// Write `bytes` as `<dir>/<name>` and return the absolute path.
QString writeCanvas(const QString &dir, const QString &name, const QByteArray &bytes)
{
    QFile file(QDir(dir).filePath(name));
    if (!file.open(QIODevice::WriteOnly))
        return {};
    file.write(bytes);
    file.close();
    return file.fileName();
}

/// The `x` of node `id` as it is on disk right now, or a sentinel when the
/// file or the node is not there yet.
double diskX(const QString &path, const QString &id)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return -1e9;
    const auto doc = QJsonDocument::fromJson(file.readAll());
    const auto nodes = doc.object().value(QStringLiteral("nodes")).toArray();
    for (const auto &value : nodes) {
        const auto object = value.toObject();
        if (object.value(QStringLiteral("id")).toString() == id)
            return object.value(QStringLiteral("x")).toDouble();
    }
    return -1e9;
}

} // namespace

class TestCanvasPersistence : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void classifyExternalEditHandlesEveryCase();
    void mutationReachesDiskWithoutUnload();
    void reloadFromDiskAdoptsExternalBytes();
};

void TestCanvasPersistence::initTestCase()
{
    // Keep the config this binary's singleton writes out of the developer's
    // ~/.config, and make the autosave debounce the shortest the setting
    // allows so the test does not wait the 2 s default.
    QStandardPaths::setTestModeEnabled(true);
    CorbomiteSettings::self()->setAutoSaveDelayMs(500);
}

void TestCanvasPersistence::classifyExternalEditHandlesEveryCase()
{
    const QByteArray ours = canvasBytes({{QStringLiteral("n1"), 0.0}});
    const QByteArray theirs = canvasBytes({{QStringLiteral("n1"), 50.0}});
    const QByteArray empty;

    // A failed read is never a reason to touch the document.
    QCOMPARE(CanvasViewTab::classifyExternalEdit(ours, QByteArray(), false, empty),
             CanvasViewTab::ExternalEdit::NoChange);
    // Identical content is nothing to reconcile, even with edits in flight —
    // the pending autosave would write these very bytes.
    QCOMPARE(CanvasViewTab::classifyExternalEdit(ours, ours, true, empty),
             CanvasViewTab::ExternalEdit::NoChange);
    // One external write must warn once, not once per Vault::modified event.
    QCOMPARE(CanvasViewTab::classifyExternalEdit(ours, theirs, true, theirs),
             CanvasViewTab::ExternalEdit::NoChange);
    // Nothing unsaved here: adopt the file.
    QCOMPARE(CanvasViewTab::classifyExternalEdit(ours, theirs, false, empty),
             CanvasViewTab::ExternalEdit::ReloadFromDisk);
    // Edits in flight: keep them and tell the user.
    QCOMPARE(CanvasViewTab::classifyExternalEdit(ours, theirs, true, empty),
             CanvasViewTab::ExternalEdit::KeepLocalAndWarn);
}

void TestCanvasPersistence::mutationReachesDiskWithoutUnload()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = writeCanvas(dir.path(), QStringLiteral("board.canvas"),
                                      canvasBytes({{QStringLiteral("n1"), 0.0}}));
    QVERIFY(!path.isEmpty());
    QCOMPARE(diskX(path, QStringLiteral("n1")), 0.0);

    CanvasViewTab tab(path, dir.path());
    auto *document = tab.canvasDocument();
    QVERIFY(document);
    QVERIFY(document->hasNode(QStringLiteral("n1")));
    QVERIFY2(!tab.isModified(), "a freshly loaded canvas is not modified");

    CanvasNode node = document->node(QStringLiteral("n1"));
    node.x = 123;
    document->updateNode(node);
    QVERIFY(tab.isModified());

    // The whole point: the debounce alone writes it. No unload, no Ctrl+S —
    // the tab is still alive and is destroyed only after this assertion, and
    // CanvasViewTab has no unload-time save of its own (that lives in
    // CanvasFileView::onUnloadFile).
    QTRY_COMPARE_WITH_TIMEOUT(diskX(path, QStringLiteral("n1")), 123.0, 5000);
}

void TestCanvasPersistence::reloadFromDiskAdoptsExternalBytes()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = writeCanvas(dir.path(), QStringLiteral("board.canvas"),
                                      canvasBytes({{QStringLiteral("n1"), 0.0}}));
    QVERIFY(!path.isEmpty());

    CanvasViewTab tab(path, dir.path());
    QVERIFY(tab.canvasDocument()->hasNode(QStringLiteral("n1")));

    // Another writer adds a node, and the scene must end up showing it.
    const QString external = writeCanvas(dir.path(), QStringLiteral("board.canvas"),
                                          canvasBytes({{QStringLiteral("n1"), 0.0},
                                                       {QStringLiteral("n2"), 40.0}}));
    QVERIFY(!external.isEmpty());

    QVERIFY(tab.reloadFromDisk());
    QVERIFY(tab.canvasDocument()->hasNode(QStringLiteral("n2")));
    QVERIFY2(!tab.isModified(), "adopting the file leaves nothing unsaved");
    QVERIFY2(tab.canvasScene()->fileCardItem(QStringLiteral("n2")),
             "the scene must be repopulated, not just the document");
}

QTEST_MAIN(TestCanvasPersistence)
#include "tst_canvas_persistence.moc"
