// SPDX-License-Identifier: GPL-3.0-or-later
//
// Cluster M M6.3 — the missing-file placeholder. A file card whose target does
// not resolve shows "File not found: <path>" plus Create / Locate… / Remove;
// it re-renders when the target appears. This covers the logic up to the
// visual/interaction check, which is a live eyeball (the plan's M6 exit).
#include <QTest>
#include <QSet>
#include <QPointF>

#include "canvas/CanvasDocument.h"
#include "canvas/CanvasScene.h"
#include "canvas/FileCardItem.h"
#include "corbomite/core/StyledRenderEngine.h"

using Canvas::CanvasDocument;
using Canvas::CanvasNode;
using Canvas::CanvasScene;
using Canvas::FileCardItem;
using Canvas::NodeType;

namespace {
CanvasNode fileNode(const QString &id, const QString &file)
{
    CanvasNode n;
    n.id = id;
    n.type = NodeType::File;
    n.file = file;
    n.x = 0;
    n.y = 0;
    n.width = 200;
    n.height = 100;
    return n;
}
}  // namespace

class TestCanvasMissingFile : public QObject
{
    Q_OBJECT
private slots:
    void missingCardShowsPlaceholder();
    void existingEmptyFileRenders();
    void refreshResolvesAfterFileAppears();
    void buttonHitTesting();
    void createButtonWritesAndResolves();
    void locateButtonReassignsFile();
    void removeButtonRemovesNode();
};

void TestCanvasMissingFile::missingCardShowsPlaceholder()
{
    CanvasDocument doc;
    doc.addNode(fileNode(QStringLiteral("n1"), QStringLiteral("missing.md")));

    CanvasScene scene;
    scene.setFileExistsChecker([](const QString &) { return false; });
    scene.setDocument(&doc);

    auto *item = scene.fileCardItem(QStringLiteral("n1"));
    QVERIFY(item);
    QVERIFY(item->isMissing());
    QVERIFY(!item->hasRenderedDocument());
    QCOMPARE(item->missingPath(), QStringLiteral("missing.md"));
}

void TestCanvasMissingFile::existingEmptyFileRenders()
{
    // The exists checker distinguishes "empty file" from "missing file": an
    // empty-but-present file must render (not show the placeholder).
    CanvasDocument doc;
    doc.addNode(fileNode(QStringLiteral("n1"), QStringLiteral("empty.md")));

    CanvasScene scene;
    Corbomite::StyledRenderEngine engine;
    scene.setRenderEngine(&engine);
    scene.setFileResolver([](const QString &) { return QString(); }); // empty body
    scene.setFileExistsChecker([](const QString &) { return true; });
    scene.setDocument(&doc);

    auto *item = scene.fileCardItem(QStringLiteral("n1"));
    QVERIFY(item);
    QVERIFY(!item->isMissing());
    QVERIFY(item->hasRenderedDocument());
}

void TestCanvasMissingFile::refreshResolvesAfterFileAppears()
{
    QSet<QString> existing;
    CanvasDocument doc;
    doc.addNode(fileNode(QStringLiteral("n1"), QStringLiteral("late.md")));

    CanvasScene scene;
    Corbomite::StyledRenderEngine engine;
    scene.setRenderEngine(&engine);
    scene.setFileResolver([](const QString &) { return QStringLiteral("body"); });
    scene.setFileExistsChecker(
        [&existing](const QString &p) { return existing.contains(p); });
    scene.setDocument(&doc);

    auto *item = scene.fileCardItem(QStringLiteral("n1"));
    QVERIFY(item->isMissing());

    existing.insert(QStringLiteral("late.md"));
    scene.refreshFileCards(QStringLiteral("late.md"));

    QVERIFY(!item->isMissing());
    QVERIFY(item->hasRenderedDocument());
}

void TestCanvasMissingFile::buttonHitTesting()
{
    CanvasDocument doc;
    doc.addNode(fileNode(QStringLiteral("n1"), QStringLiteral("x.md")));

    CanvasScene scene;
    scene.setFileExistsChecker([](const QString &) { return false; });
    scene.setDocument(&doc);

    auto *item = scene.fileCardItem(QStringLiteral("n1"));
    QVERIFY(item->isMissing());

    for (int b = 0; b < 3; ++b) {
        const QRectF r = item->buttonRect(b);
        QVERIFY2(!r.isNull(), "missing card must expose three button rects");
        QCOMPARE(item->buttonAt(r.center()), b);
    }
    QCOMPARE(item->buttonAt(QPointF(1, 1)), -1);
    QVERIFY(item->buttonRect(99).isNull());
}

void TestCanvasMissingFile::createButtonWritesAndResolves()
{
    QSet<QString> existing;
    CanvasDocument doc;
    doc.addNode(fileNode(QStringLiteral("n1"), QStringLiteral("new.md")));

    CanvasScene scene;
    Corbomite::StyledRenderEngine engine;
    scene.setRenderEngine(&engine);
    QString createdPath;
    QString createdContent = QStringLiteral("<unset>");
    scene.setFileSaver([&](const QString &p, const QString &c) {
        createdPath = p;
        createdContent = c;
        existing.insert(p);
    });
    scene.setFileResolver([](const QString &) { return QStringLiteral("body"); });
    scene.setFileExistsChecker(
        [&existing](const QString &p) { return existing.contains(p); });
    scene.setDocument(&doc);

    auto *item = scene.fileCardItem(QStringLiteral("n1"));
    QVERIFY(item->isMissing());

    QVERIFY(scene.createMissingFileCard(QStringLiteral("n1")));
    QCOMPARE(createdPath, QStringLiteral("new.md"));
    QVERIFY(createdContent.isEmpty());
    QVERIFY(!item->isMissing());
    QVERIFY(item->hasRenderedDocument());
}

void TestCanvasMissingFile::locateButtonReassignsFile()
{
    QSet<QString> existing;
    CanvasDocument doc;
    doc.addNode(fileNode(QStringLiteral("n1"), QStringLiteral("missing.md")));

    CanvasScene scene;
    scene.setFileExistsChecker(
        [&existing](const QString &p) { return existing.contains(p); });
    scene.setFilePickerRequestor([]() { return QStringLiteral("found.md"); });
    scene.setDocument(&doc);

    auto *item = scene.fileCardItem(QStringLiteral("n1"));
    QVERIFY(item->isMissing());

    existing.insert(QStringLiteral("found.md"));
    QVERIFY(scene.locateMissingFileCard(QStringLiteral("n1")));
    QCOMPARE(doc.node(QStringLiteral("n1")).file, QStringLiteral("found.md"));
    QVERIFY(!item->isMissing());
}

void TestCanvasMissingFile::removeButtonRemovesNode()
{
    CanvasDocument doc;
    doc.addNode(fileNode(QStringLiteral("n1"), QStringLiteral("gone.md")));

    CanvasScene scene;
    scene.setFileExistsChecker([](const QString &) { return false; });
    scene.setDocument(&doc);
    QVERIFY(doc.hasNode(QStringLiteral("n1")));

    QVERIFY(scene.removeMissingFileCard(QStringLiteral("n1")));
    QVERIFY(!doc.hasNode(QStringLiteral("n1")));
}

QTEST_MAIN(TestCanvasMissingFile)
#include "tst_canvas_missing_file.moc"
