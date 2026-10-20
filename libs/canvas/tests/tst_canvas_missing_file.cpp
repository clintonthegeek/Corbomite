// SPDX-License-Identifier: GPL-3.0-or-later
//
// The missing-file placeholder. A file card whose target does
// not resolve shows "File not found: <path>" plus Create / Locate… / Remove;
// it re-renders when the target appears. This covers the logic up to the
// visual/interaction check, which is a live eyeball.
#include <QTest>
#include <QGraphicsSceneMouseEvent>
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

/// `CanvasScene::mousePressEvent` is protected (it is an override), so a test
/// that drives a real press has to lift it.
class PressableCanvasScene : public CanvasScene
{
public:
    using CanvasScene::mousePressEvent;
};
}  // namespace

class TestCanvasMissingFile : public QObject
{
    Q_OBJECT
private slots:
    void missingCardShowsPlaceholder();
    void existingEmptyFileRenders();
    void refreshResolvesAfterFileAppears();
    void buttonHitTesting();
    void pressOnButtonReachesItThroughAnItemAbove();
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

void TestCanvasMissingFile::pressOnButtonReachesItThroughAnItemAbove()
{
    // Live finding 2026-10-18: the buttons painted but could not be pressed.
    // The press was routed with QGraphicsScene::itemAt(), and the chrome
    // overlay sits above every node (z 1000) with a shape covering the whole
    // card — and it is up whenever the pointer is over the card, so the
    // top-most item was never the card. Anything above the card reproduces it
    // (a plain rect here, so this does not depend on the overlay's class).
    CanvasDocument doc;
    doc.addNode(fileNode(QStringLiteral("n1"), QStringLiteral("new.md")));

    PressableCanvasScene scene;
    scene.setFileExistsChecker([](const QString &) { return false; });
    QString created;
    scene.setFileSaver([&created](const QString &p, const QString &) { created = p; });
    scene.setDocument(&doc);

    auto *card = scene.fileCardItem(QStringLiteral("n1"));
    QVERIFY(card && card->isMissing());

    auto *above = scene.addRect(card->sceneBoundingRect(), QPen(Qt::NoPen), Qt::NoBrush);
    QVERIFY(above);
    above->setZValue(1000);

    QGraphicsSceneMouseEvent event(QEvent::GraphicsSceneMousePress);
    event.setScenePos(card->mapToScene(card->buttonRect(FileCardItem::CreateButton).center()));
    event.setButton(Qt::LeftButton);
    event.setButtons(Qt::LeftButton);
    scene.mousePressEvent(&event);

    QCOMPARE(created, QStringLiteral("new.md"));
    QVERIFY(event.isAccepted());
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
