// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QUndoStack>
#include <QCoreApplication>
#include <QGraphicsSceneDragDropEvent>
#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>
#include <QMimeData>
#include <QUrl>
#include <graffodil/IGraphNode.h>
#include "canvas/CanvasDocument.h"
#include "canvas/CanvasScene.h"
#include "canvas/CanvasCommands.h"
#include "canvas/TextCardItem.h"
#include "canvas/FileCardItem.h"
#include "canvas/GroupItem.h"
#include "canvas/EdgeItem.h"
#include "corbomite/core/RegexRenderEngine.h"

class TestCanvasScene : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        // Upstream-documented wart (Graffodil ROADMAP/plan Appendix B rule 6):
        // QSignalSpy on Graffodil tool signals carrying IGraphNode*/IGraphEdge*
        // silently fails to record without this registration.
        qRegisterMetaType<Graffodil::IGraphNode *>("IGraphNode*");
    }

    void testAddTextCard()
    {
        // Create document, add text node -> scene should create TextCardItem
        Canvas::CanvasDocument doc;
        Canvas::CanvasScene scene;
        scene.setDocument(&doc);

        Canvas::CanvasNode node;
        node.id = Canvas::CanvasDocument::generateId();
        node.type = Canvas::NodeType::Text;
        node.text = QStringLiteral("Test card");
        node.x = 100; node.y = 100;
        node.width = 200; node.height = 80;
        doc.addNode(node);

        // Scene should have created a TextCardItem via onNodeAdded slot
        auto *item = scene.textCardItem(node.id);
        QVERIFY(item != nullptr);
        QCOMPARE(item->nodeId(), node.id);
    }

    void testMoveCardUpdatesDocument()
    {
        // Create scene with a card, move the item, verify document sync behavior
        Canvas::CanvasDocument doc;
        Canvas::CanvasNode node;
        node.id = QStringLiteral("movable");
        node.type = Canvas::NodeType::Text;
        node.x = 0; node.y = 0; node.width = 200; node.height = 80;
        doc.addNode(node);

        Canvas::CanvasScene scene;
        scene.setDocument(&doc);

        auto *item = scene.textCardItem(QStringLiteral("movable"));
        QVERIFY(item);

        // Move the item directly via setPos
        item->setPos(150, 200);

        // The scene does NOT auto-update the document on item position change.
        // Only undo commands (CmdMoveCards) update the document model.
        // The positionChanged signal only adjusts connected edges.
        // Verify the document still has the original position:
        auto docNode = doc.node(QStringLiteral("movable"));
        QCOMPARE(docNode.x, 0);
        QCOMPARE(docNode.y, 0);

        // To properly sync, use the undo command:
        QHash<QString, QPointF> oldPos, newPos;
        oldPos[QStringLiteral("movable")] = QPointF(0, 0);
        newPos[QStringLiteral("movable")] = QPointF(150, 200);
        scene.undoStack()->push(new Canvas::CmdMoveCards(&doc, oldPos, newPos));

        // Now the document should be updated
        auto updated = doc.node(QStringLiteral("movable"));
        QCOMPARE(updated.x, 150);
        QCOMPARE(updated.y, 200);
    }

    void testDeleteCardRemovesEdges()
    {
        // Create 2 cards + 1 edge, remove one card -> edge should also be removed
        Canvas::CanvasDocument doc;
        Canvas::CanvasNode n1;
        n1.id = QStringLiteral("a"); n1.type = Canvas::NodeType::Text;
        n1.x = 0; n1.y = 0; n1.width = 200; n1.height = 80;
        Canvas::CanvasNode n2;
        n2.id = QStringLiteral("b"); n2.type = Canvas::NodeType::Text;
        n2.x = 300; n2.y = 0; n2.width = 200; n2.height = 80;
        doc.addNode(n1);
        doc.addNode(n2);

        Canvas::CanvasEdge edge;
        edge.id = QStringLiteral("e1");
        edge.fromNode = QStringLiteral("a");
        edge.toNode = QStringLiteral("b");
        doc.addEdge(edge);

        Canvas::CanvasScene scene;
        scene.setDocument(&doc);

        QVERIFY(scene.edgeItem(QStringLiteral("e1")) != nullptr);

        // Remove node "a" from document
        doc.removeNode(QStringLiteral("a"));

        // Edge should be gone from both document and scene
        QCOMPARE(doc.edges().size(), 0);
        QVERIFY(scene.edgeItem(QStringLiteral("e1")) == nullptr);
        QVERIFY(scene.textCardItem(QStringLiteral("a")) == nullptr);
    }

    void testGroupContainment()
    {
        // Create a group and a card inside it
        Canvas::CanvasDocument doc;
        Canvas::CanvasNode group;
        group.id = QStringLiteral("g1"); group.type = Canvas::NodeType::Group;
        group.x = 0; group.y = 0; group.width = 400; group.height = 300;
        group.label = QStringLiteral("My Group");
        doc.addNode(group);

        Canvas::CanvasNode card;
        card.id = QStringLiteral("c1"); card.type = Canvas::NodeType::Text;
        card.x = 50; card.y = 50; card.width = 200; card.height = 80;
        card.text = QStringLiteral("Inside group");
        doc.addNode(card);

        Canvas::CanvasScene scene;
        scene.setDocument(&doc);

        auto *groupItem = scene.groupItem(QStringLiteral("g1"));
        QVERIFY(groupItem);

        auto contained = groupItem->containedItems();
        QVERIFY(contained.size() >= 1); // card should be inside group
    }

    void testUndoMoveCard()
    {
        // Move a card, undo, verify position restored
        Canvas::CanvasDocument doc;
        Canvas::CanvasNode node;
        node.id = QStringLiteral("undotest");
        node.type = Canvas::NodeType::Text;
        node.x = 100; node.y = 100; node.width = 200; node.height = 80;
        doc.addNode(node);

        Canvas::CanvasScene scene;
        scene.setDocument(&doc);

        // Push a move command
        QHash<QString, QPointF> oldPos, newPos;
        oldPos[QStringLiteral("undotest")] = QPointF(100, 100);
        newPos[QStringLiteral("undotest")] = QPointF(300, 200);

        scene.undoStack()->push(new Canvas::CmdMoveCards(&doc, oldPos, newPos));

        // Verify moved
        auto updated = doc.node(QStringLiteral("undotest"));
        QCOMPARE(updated.x, 300);
        QCOMPARE(updated.y, 200);

        // Undo
        scene.undoStack()->undo();

        auto restored = doc.node(QStringLiteral("undotest"));
        QCOMPARE(restored.x, 100);
        QCOMPARE(restored.y, 100);
    }

    void testEdgeAdjustsOnCardMove()
    {
        // Create 2 cards + edge, verify edge path is non-empty
        Canvas::CanvasDocument doc;
        Canvas::CanvasNode n1;
        n1.id = QStringLiteral("src"); n1.type = Canvas::NodeType::Text;
        n1.x = 0; n1.y = 0; n1.width = 200; n1.height = 80;
        Canvas::CanvasNode n2;
        n2.id = QStringLiteral("tgt"); n2.type = Canvas::NodeType::Text;
        n2.x = 400; n2.y = 0; n2.width = 200; n2.height = 80;
        doc.addNode(n1);
        doc.addNode(n2);

        Canvas::CanvasEdge edge;
        edge.id = QStringLiteral("e1");
        edge.fromNode = QStringLiteral("src");
        edge.toNode = QStringLiteral("tgt");
        edge.fromSide = Canvas::Side::Right;
        edge.toSide = Canvas::Side::Left;
        doc.addEdge(edge);

        Canvas::CanvasScene scene;
        scene.setDocument(&doc);

        auto *edgeItem = scene.edgeItem(QStringLiteral("e1"));
        QVERIFY(edgeItem);
        QVERIFY(!edgeItem->path().isEmpty());
        // Edge exists and has a non-empty path -- it connects the two cards
    }

    void testAddFileCard()
    {
        Canvas::CanvasDocument doc;
        Canvas::CanvasScene scene;
        scene.setDocument(&doc);

        scene.setFileResolver([](const QString &path) -> QString {
            if (path == QStringLiteral("note.md"))
                return QStringLiteral("# Hello\n\nContent here");
            return {};
        });

        Corbomite::RegexRenderEngine engine;
        engine.setProfile(Corbomite::RenderProfile::canvasCard());
        scene.setRenderEngine(&engine);

        Canvas::CanvasNode node;
        node.id = QStringLiteral("file1");
        node.type = Canvas::NodeType::File;
        node.file = QStringLiteral("note.md");
        node.x = 100; node.y = 100;
        node.width = 250; node.height = 200;
        doc.addNode(node);

        auto *item = scene.fileCardItem(QStringLiteral("file1"));
        QVERIFY(item != nullptr);
        QCOMPARE(item->nodeId(), QStringLiteral("file1"));
    }

    void testFileCardWithSubpath()
    {
        Canvas::CanvasDocument doc;
        Canvas::CanvasScene scene;
        scene.setDocument(&doc);

        scene.setFileResolver([](const QString &) -> QString {
            return QStringLiteral("# Title\n\nIntro\n\n## Section\n\nSection content");
        });

        Corbomite::RegexRenderEngine engine;
        engine.setProfile(Corbomite::RenderProfile::canvasCard());
        scene.setRenderEngine(&engine);

        Canvas::CanvasNode node;
        node.id = QStringLiteral("sub1");
        node.type = Canvas::NodeType::File;
        node.file = QStringLiteral("note.md");
        node.subpath = QStringLiteral("#Section");
        node.x = 0; node.y = 0;
        node.width = 250; node.height = 200;
        doc.addNode(node);

        auto *item = scene.fileCardItem(QStringLiteral("sub1"));
        QVERIFY(item != nullptr);
    }

    void testSetRenderEngineAfterCardsExist()
    {
        // Regression: if cards are built BEFORE the engine is set (e.g. the
        // engine is wired by MainWindow after the document loads), the existing
        // cards must be (re-)rendered when setRenderEngine is finally called.
        Canvas::CanvasDocument doc;
        Canvas::CanvasScene scene;
        scene.setDocument(&doc);

        scene.setFileResolver([](const QString &) -> QString {
            return QStringLiteral("# Hello\n\nContent here");
        });

        // Add the file card FIRST, with no engine set yet.
        Canvas::CanvasNode node;
        node.id = QStringLiteral("late1");
        node.type = Canvas::NodeType::File;
        node.file = QStringLiteral("note.md");
        node.x = 0; node.y = 0;
        node.width = 250; node.height = 200;
        doc.addNode(node);

        auto *item = scene.fileCardItem(QStringLiteral("late1"));
        QVERIFY(item != nullptr);
        // No engine yet -> nothing rendered.
        QVERIFY(!item->hasRenderedDocument());

        // Now set the engine AFTER the card already exists.
        Corbomite::RegexRenderEngine engine;
        engine.setProfile(Corbomite::RenderProfile::canvasCard());
        scene.setRenderEngine(&engine);

        // The existing card must now have a rendered document.
        QVERIFY(item->hasRenderedDocument());
    }

    void testFileResolverReturnsEmpty()
    {
        Canvas::CanvasDocument doc;
        Canvas::CanvasScene scene;
        scene.setDocument(&doc);

        scene.setFileResolver([](const QString &) -> QString { return {}; });

        Corbomite::RegexRenderEngine engine;
        engine.setProfile(Corbomite::RenderProfile::canvasCard());
        scene.setRenderEngine(&engine);

        Canvas::CanvasNode node;
        node.id = QStringLiteral("missing");
        node.type = Canvas::NodeType::File;
        node.file = QStringLiteral("nonexistent.md");
        node.x = 0; node.y = 0;
        node.width = 250; node.height = 200;
        doc.addNode(node);

        auto *item = scene.fileCardItem(QStringLiteral("missing"));
        QVERIFY(item != nullptr);
    }

    void testEdgeBetweenTextAndFileCard()
    {
        Canvas::CanvasDocument doc;
        Canvas::CanvasScene scene;
        scene.setDocument(&doc);

        scene.setFileResolver([](const QString &) -> QString {
            return QStringLiteral("# Note\n\nContent");
        });
        Corbomite::RegexRenderEngine engine;
        engine.setProfile(Corbomite::RenderProfile::canvasCard());
        scene.setRenderEngine(&engine);

        Canvas::CanvasNode textNode;
        textNode.id = QStringLiteral("t1");
        textNode.type = Canvas::NodeType::Text;
        textNode.x = 0; textNode.y = 0; textNode.width = 200; textNode.height = 80;
        doc.addNode(textNode);

        Canvas::CanvasNode fileNode;
        fileNode.id = QStringLiteral("f1");
        fileNode.type = Canvas::NodeType::File;
        fileNode.file = QStringLiteral("note.md");
        fileNode.x = 400; fileNode.y = 0; fileNode.width = 250; fileNode.height = 200;
        doc.addNode(fileNode);

        Canvas::CanvasEdge edge;
        edge.id = QStringLiteral("e1");
        edge.fromNode = QStringLiteral("t1");
        edge.toNode = QStringLiteral("f1");
        doc.addEdge(edge);

        auto *edgeItem = scene.edgeItem(QStringLiteral("e1"));
        QVERIFY(edgeItem != nullptr);
        QVERIFY(!edgeItem->path().isEmpty());
    }

    // -----------------------------------------------------------------
    // M1.6 — Graffodil rebase regression/behavior tests (spec §5)
    // -----------------------------------------------------------------

    void testFileCardSelectableAndMovable()
    {
        // Pre-M1 regression (gap #1, plan "Broken / missing" item 1):
        // SelectMoveTool::mousePressEvent dynamic_cast'd only TextCardItem/
        // GroupItem, so a press on a FileCardItem fell through to
        // rubber-band selection instead of selecting+moving it. This test
        // would FAIL against the pre-M1 CanvasTool.cpp SelectMoveTool
        // (which never selects m_dragNode for an unrecognized item type,
        // so the card position never changes and no CmdMoveCards is
        // pushed). Graffodil::SelectMoveTool operates over IGraphNode, so
        // it is type-blind by construction and this now passes.
        Canvas::CanvasDocument doc;
        Canvas::CanvasNode node;
        node.id = QStringLiteral("file1");
        node.type = Canvas::NodeType::File;
        node.file = QStringLiteral("note.md");
        node.x = 0; node.y = 0; node.width = 200; node.height = 150;
        doc.addNode(node);

        Canvas::CanvasScene scene;
        scene.setDocument(&doc);

        auto *item = scene.fileCardItem(QStringLiteral("file1"));
        QVERIFY(item != nullptr);
        QVERIFY(item->flags() & QGraphicsItem::ItemIsSelectable);

        const QPointF pressPos(100, 75);
        const QPointF movePos(300, 75);

        QGraphicsSceneMouseEvent press(QEvent::GraphicsSceneMousePress);
        press.setScenePos(pressPos);
        press.setButton(Qt::LeftButton);
        press.setButtons(Qt::LeftButton);
        QCoreApplication::sendEvent(&scene, &press);

        QVERIFY(item->isSelected());

        QGraphicsSceneMouseEvent move(QEvent::GraphicsSceneMouseMove);
        move.setScenePos(movePos);
        move.setLastScenePos(pressPos);
        move.setButtons(Qt::LeftButton);
        QCoreApplication::sendEvent(&scene, &move);

        QGraphicsSceneMouseEvent release(QEvent::GraphicsSceneMouseRelease);
        release.setScenePos(movePos);
        release.setButton(Qt::LeftButton);
        release.setButtons(Qt::NoButton);
        QCoreApplication::sendEvent(&scene, &release);

        QCOMPARE(item->pos(), QPointF(200, 0));
        QCOMPARE(scene.undoStack()->count(), 1);

        auto updated = doc.node(QStringLiteral("file1"));
        QCOMPARE(updated.x, 200);
        QCOMPARE(updated.y, 0);
    }

    void testRubberBandSelectsAllNodeKinds()
    {
        Canvas::CanvasDocument doc;
        Canvas::CanvasNode textNode;
        textNode.id = QStringLiteral("t1"); textNode.type = Canvas::NodeType::Text;
        textNode.x = 0; textNode.y = 0; textNode.width = 100; textNode.height = 60;
        doc.addNode(textNode);

        Canvas::CanvasNode fileNode;
        fileNode.id = QStringLiteral("f1"); fileNode.type = Canvas::NodeType::File;
        fileNode.file = QStringLiteral("note.md");
        fileNode.x = 200; fileNode.y = 0; fileNode.width = 100; fileNode.height = 60;
        doc.addNode(fileNode);

        Canvas::CanvasNode groupNode;
        groupNode.id = QStringLiteral("g1"); groupNode.type = Canvas::NodeType::Group;
        groupNode.x = 400; groupNode.y = 0; groupNode.width = 100; groupNode.height = 60;
        groupNode.label = QStringLiteral("G");
        doc.addNode(groupNode);

        Canvas::CanvasScene scene;
        scene.setDocument(&doc);

        auto *textItem = scene.textCardItem(QStringLiteral("t1"));
        auto *fileItem = scene.fileCardItem(QStringLiteral("f1"));
        auto *grpItem = scene.groupItem(QStringLiteral("g1"));
        QVERIFY(textItem && fileItem && grpItem);

        // Press on empty space to start a rubber-band, drag across all three.
        QGraphicsSceneMouseEvent press(QEvent::GraphicsSceneMousePress);
        press.setScenePos(QPointF(-50, -50));
        press.setButton(Qt::LeftButton);
        press.setButtons(Qt::LeftButton);
        QCoreApplication::sendEvent(&scene, &press);

        QGraphicsSceneMouseEvent move(QEvent::GraphicsSceneMouseMove);
        move.setScenePos(QPointF(600, 200));
        move.setLastScenePos(QPointF(-50, -50));
        move.setButtons(Qt::LeftButton);
        QCoreApplication::sendEvent(&scene, &move);

        QGraphicsSceneMouseEvent release(QEvent::GraphicsSceneMouseRelease);
        release.setScenePos(QPointF(600, 200));
        release.setButton(Qt::LeftButton);
        release.setButtons(Qt::NoButton);
        QCoreApplication::sendEvent(&scene, &release);

        QVERIFY(textItem->isSelected());
        QVERIFY(fileItem->isSelected());
        QVERIFY(grpItem->isSelected());
    }

    void testDeleteSelectionSingleUndoStep()
    {
        Canvas::CanvasDocument doc;
        Canvas::CanvasNode textNode;
        textNode.id = QStringLiteral("t1"); textNode.type = Canvas::NodeType::Text;
        textNode.x = 0; textNode.y = 0; textNode.width = 100; textNode.height = 60;
        doc.addNode(textNode);

        Canvas::CanvasNode fileNode;
        fileNode.id = QStringLiteral("f1"); fileNode.type = Canvas::NodeType::File;
        fileNode.file = QStringLiteral("note.md");
        fileNode.x = 200; fileNode.y = 0; fileNode.width = 100; fileNode.height = 60;
        doc.addNode(fileNode);

        Canvas::CanvasNode groupNode;
        groupNode.id = QStringLiteral("g1"); groupNode.type = Canvas::NodeType::Group;
        groupNode.x = 400; groupNode.y = 0; groupNode.width = 100; groupNode.height = 60;
        doc.addNode(groupNode);

        Canvas::CanvasEdge edge;
        edge.id = QStringLiteral("e1");
        edge.fromNode = QStringLiteral("t1");
        edge.toNode = QStringLiteral("f1");
        doc.addEdge(edge);

        Canvas::CanvasScene scene;
        scene.setDocument(&doc);

        auto *textItem = scene.textCardItem(QStringLiteral("t1"));
        auto *fileItem = scene.fileCardItem(QStringLiteral("f1"));
        auto *grpItem = scene.groupItem(QStringLiteral("g1"));
        QVERIFY(textItem && fileItem && grpItem);

        // Select all three nodes (edge is deliberately left unselected —
        // it should still vanish as a cascade of removing "t1").
        textItem->setSelected(true);
        fileItem->setSelected(true);
        grpItem->setSelected(true);

        QCOMPARE(scene.undoStack()->count(), 0);

        QKeyEvent keyPress(QEvent::KeyPress, Qt::Key_Delete, Qt::NoModifier);
        QCoreApplication::sendEvent(&scene, &keyPress);

        // Single undo step for the whole mixed-kind selection.
        QCOMPARE(scene.undoStack()->count(), 1);
        QVERIFY(!doc.hasNode(QStringLiteral("t1")));
        QVERIFY(!doc.hasNode(QStringLiteral("f1")));
        QVERIFY(!doc.hasNode(QStringLiteral("g1")));
        QCOMPARE(doc.edges().size(), 0);

        scene.undoStack()->undo();

        QVERIFY(doc.hasNode(QStringLiteral("t1")));
        QVERIFY(doc.hasNode(QStringLiteral("f1")));
        QVERIFY(doc.hasNode(QStringLiteral("g1")));
        QCOMPARE(doc.edges().size(), 1);
    }

    void testResizeToolCommitsUndo()
    {
        Canvas::CanvasDocument doc;
        Canvas::CanvasNode node;
        node.id = QStringLiteral("r1");
        node.type = Canvas::NodeType::Text;
        node.x = 0; node.y = 0; node.width = 200; node.height = 100;
        doc.addNode(node);

        Canvas::CanvasScene scene;
        scene.setDocument(&doc);

        auto *item = scene.textCardItem(QStringLiteral("r1"));
        QVERIFY(item != nullptr);
        item->setSelected(true);

        // Press near the bottom-right resize handle (within kResizeZone=8 of
        // both the right and bottom edges), then drag inward past the
        // kMinSize=40 clamp.
        QGraphicsSceneMouseEvent press(QEvent::GraphicsSceneMousePress);
        press.setScenePos(QPointF(198, 98));
        press.setButton(Qt::LeftButton);
        press.setButtons(Qt::LeftButton);
        QCoreApplication::sendEvent(&scene, &press);

        QGraphicsSceneMouseEvent move(QEvent::GraphicsSceneMouseMove);
        move.setScenePos(QPointF(10, 10)); // would shrink to ~10x10 without clamp
        move.setLastScenePos(QPointF(198, 98));
        move.setButtons(Qt::LeftButton);
        QCoreApplication::sendEvent(&scene, &move);

        // Live clamp applied during the drag (before commit).
        QCOMPARE(item->nodeData().width, 40);
        QCOMPARE(item->nodeData().height, 40);

        QGraphicsSceneMouseEvent release(QEvent::GraphicsSceneMouseRelease);
        release.setScenePos(QPointF(10, 10));
        release.setButton(Qt::LeftButton);
        release.setButtons(Qt::NoButton);
        QCoreApplication::sendEvent(&scene, &release);

        QCOMPARE(scene.undoStack()->count(), 1);
        auto updated = doc.node(QStringLiteral("r1"));
        QCOMPARE(updated.width, 40);
        QCOMPARE(updated.height, 40);

        scene.undoStack()->undo();
        auto restored = doc.node(QStringLiteral("r1"));
        QCOMPARE(restored.width, 200);
        QCOMPARE(restored.height, 100);
    }

    void testEdgeFollowsNodeMove()
    {
        // Adjacency-index path: a node moved via a document-driven write
        // (CmdMoveCards::redo(), same as undo/redo replay) must still keep
        // the edge attached, even though no interactive drag ever ran
        // through SelectMoveTool. Covers the CanvasNodeItem::itemChange ->
        // GraphScene::adjustEdgesForNode() hook (spec §6a V2).
        Canvas::CanvasDocument doc;
        Canvas::CanvasNode n1;
        n1.id = QStringLiteral("src"); n1.type = Canvas::NodeType::Text;
        n1.x = 0; n1.y = 0; n1.width = 200; n1.height = 80;
        Canvas::CanvasNode n2;
        n2.id = QStringLiteral("tgt"); n2.type = Canvas::NodeType::Text;
        n2.x = 400; n2.y = 0; n2.width = 200; n2.height = 80;
        doc.addNode(n1);
        doc.addNode(n2);

        Canvas::CanvasEdge edge;
        edge.id = QStringLiteral("e1");
        edge.fromNode = QStringLiteral("src");
        edge.toNode = QStringLiteral("tgt");
        doc.addEdge(edge);

        Canvas::CanvasScene scene;
        scene.setDocument(&doc);

        auto *edgeItem = scene.edgeItem(QStringLiteral("e1"));
        QVERIFY(edgeItem != nullptr);
        const QPointF pathBefore = edgeItem->path().pointAtPercent(0.0);

        QHash<QString, QPointF> oldPos, newPos;
        oldPos[QStringLiteral("src")] = QPointF(0, 0);
        newPos[QStringLiteral("src")] = QPointF(300, 300);
        scene.undoStack()->push(new Canvas::CmdMoveCards(&doc, oldPos, newPos));

        const QPointF pathAfter = edgeItem->path().pointAtPercent(0.0);
        QVERIFY(pathBefore != pathAfter);
    }

    void testEdgeIdPreserved()
    {
        // Guards the §3.2 override: GraphEdgeItem generates its own
        // uuid-shaped id by default; CanvasEdgeItem::edgeId() must return
        // the .canvas document id instead.
        Canvas::CanvasDocument doc;
        Canvas::CanvasNode n1;
        n1.id = QStringLiteral("a"); n1.type = Canvas::NodeType::Text;
        n1.x = 0; n1.y = 0; n1.width = 100; n1.height = 60;
        Canvas::CanvasNode n2;
        n2.id = QStringLiteral("b"); n2.type = Canvas::NodeType::Text;
        n2.x = 200; n2.y = 0; n2.width = 100; n2.height = 60;
        doc.addNode(n1);
        doc.addNode(n2);

        Canvas::CanvasEdge edge;
        edge.id = QStringLiteral("custom-edge-id-1234");
        edge.fromNode = QStringLiteral("a");
        edge.toNode = QStringLiteral("b");
        doc.addEdge(edge);

        Canvas::CanvasScene scene;
        scene.setDocument(&doc);

        auto *item = scene.edgeItem(QStringLiteral("custom-edge-id-1234"));
        QVERIFY(item != nullptr);
        QCOMPARE(item->edgeId(), QStringLiteral("custom-edge-id-1234"));
        QVERIFY(scene.edgeForId(QStringLiteral("custom-edge-id-1234")) != nullptr);
    }

    // -----------------------------------------------------------------
    // Phase M2 — node creation flows
    // -----------------------------------------------------------------

    void testDoubleClickEmptyCreatesTextCardInEditMode()
    {
        Canvas::CanvasDocument doc;
        Canvas::CanvasScene scene;
        scene.setDocument(&doc);

        QCOMPARE(doc.nodes().size(), 0);

        QGraphicsSceneMouseEvent dblClick(QEvent::GraphicsSceneMouseDoubleClick);
        dblClick.setScenePos(QPointF(500, 500));
        dblClick.setButton(Qt::LeftButton);
        dblClick.setButtons(Qt::LeftButton);
        QCoreApplication::sendEvent(&scene, &dblClick);

        QCOMPARE(doc.nodes().size(), 1);
        const auto node = doc.nodes().first();
        QCOMPARE(node.type, Canvas::NodeType::Text);
        QCOMPARE(node.width, 250);
        QCOMPARE(node.height, 60);
        // Click point is the card's center, not its top-left corner.
        QCOMPARE(node.x, 500 - 125);
        QCOMPARE(node.y, 500 - 30);

        auto *item = scene.textCardItem(node.id);
        QVERIFY(item != nullptr);
        QVERIFY(item->isSelected());
        QVERIFY(scene.isEditing());

        QCOMPARE(scene.undoStack()->count(), 1);

        // Close the in-place editor before the scene tears down (clicking
        // outside the proxy is the normal way this happens in the app;
        // leaving the QGraphicsProxyWidget alive across ~CanvasScene is a
        // pre-existing teardown hazard unrelated to M2.1, not exercised
        // by any other test in this file).
        QGraphicsSceneMouseEvent outsideClick(QEvent::GraphicsSceneMousePress);
        outsideClick.setScenePos(QPointF(-1000, -1000));
        outsideClick.setButton(Qt::LeftButton);
        outsideClick.setButtons(Qt::LeftButton);
        QCoreApplication::sendEvent(&scene, &outsideClick);
        QVERIFY(!scene.isEditing());
    }

    void testSceneDestroyedWhileEditingDoesNotCrash()
    {
        // Regression for a heap-corruption bug surfaced during Cluster M
        // M1.7 live testing: deleting a CanvasScene while an inline edit
        // (or group-label edit) is still open used to crash. Root cause:
        // beginInlineEdit()/beginGroupLabelEdit() connect
        // QApplication::focusChanged back to the scene so losing focus
        // auto-commits the edit. QGraphicsScene::~QGraphicsScene() deletes
        // scene items (including the still-open edit proxy) before that
        // connection is torn down; deleting the focused QTextEdit/QLineEdit
        // synchronously re-emits focusChanged, re-entering
        // finishInlineEdit()/finishGroupLabelEdit() and double-deleting an
        // item that's already mid-teardown. Fixed by giving CanvasScene an
        // explicit destructor that finishes any open edit BEFORE the base
        // class starts deleting items. This test must not crash/ASAN-abort;
        // a document commit is a bonus sanity check, not the main point.
        Canvas::CanvasDocument doc;
        {
            Canvas::CanvasScene scene;
            scene.setDocument(&doc);

            QGraphicsSceneMouseEvent dblClick(QEvent::GraphicsSceneMouseDoubleClick);
            dblClick.setScenePos(QPointF(500, 500));
            dblClick.setButton(Qt::LeftButton);
            dblClick.setButtons(Qt::LeftButton);
            QCoreApplication::sendEvent(&scene, &dblClick);

            QVERIFY(scene.isEditing());
            // scene destructs here at end of scope, still mid-edit.
        }
        QCOMPARE(doc.nodes().size(), 1);
    }

    void testContextMenuCreatesFileCard()
    {
        // M2.2 — the "New file card…" action invokes the injected picker
        // requestor rather than driving a real modal.
        Canvas::CanvasDocument doc;
        Canvas::CanvasScene scene;
        scene.setDocument(&doc);

        scene.setFilePickerRequestor([]() -> QString {
            return QStringLiteral("attachments/diagram.pdf");
        });

        QCOMPARE(doc.nodes().size(), 0);
        scene.createFileCardViaPicker(QPointF(40, 60));

        QCOMPARE(doc.nodes().size(), 1);
        const auto node = doc.nodes().first();
        QCOMPARE(node.type, Canvas::NodeType::File);
        QCOMPARE(node.file, QStringLiteral("attachments/diagram.pdf"));
        QCOMPARE(node.width, 400);
        QCOMPARE(node.height, 400);
        QCOMPARE(node.x, 40);
        QCOMPARE(node.y, 60);

        auto *item = scene.fileCardItem(node.id);
        QVERIFY(item != nullptr);
        QVERIFY(item->isSelected());
        QCOMPARE(scene.undoStack()->count(), 1);
    }

    void testContextMenuFileCardCancelledPickerCreatesNothing()
    {
        Canvas::CanvasDocument doc;
        Canvas::CanvasScene scene;
        scene.setDocument(&doc);

        scene.setFilePickerRequestor([]() -> QString { return QString(); });
        scene.createFileCardViaPicker(QPointF(0, 0));

        QCOMPARE(doc.nodes().size(), 0);
        QCOMPARE(scene.undoStack()->count(), 0);
    }

    void testDropSingleFileCreatesFileCard()
    {
        Canvas::CanvasDocument doc;
        Canvas::CanvasScene scene;
        scene.setDocument(&doc);

        scene.setVaultPathResolver([](const QString &absPath) -> QString {
            if (absPath == QStringLiteral("/vault/notes/dropped.md"))
                return QStringLiteral("notes/dropped.md");
            return QString(); // outside vault -> rejected
        });

        QMimeData mime;
        mime.setUrls({QUrl::fromLocalFile(QStringLiteral("/vault/notes/dropped.md"))});

        QGraphicsSceneDragDropEvent dropEv(QEvent::GraphicsSceneDrop);
        dropEv.setMimeData(&mime);
        dropEv.setScenePos(QPointF(100, 100));
        QCoreApplication::sendEvent(&scene, &dropEv);

        QCOMPARE(doc.nodes().size(), 1);
        const auto node = doc.nodes().first();
        QCOMPARE(node.type, Canvas::NodeType::File);
        QCOMPARE(node.file, QStringLiteral("notes/dropped.md"));
        QCOMPARE(node.width, 400);
        QCOMPARE(node.height, 400);
        QCOMPARE(node.x, 100);
        QCOMPARE(node.y, 100);
        QCOMPARE(scene.undoStack()->count(), 1);

        auto *item = scene.fileCardItem(node.id);
        QVERIFY(item != nullptr);
        QVERIFY(item->isSelected());
    }

    void testDropTextCreatesTextCard()
    {
        Canvas::CanvasDocument doc;
        Canvas::CanvasScene scene;
        scene.setDocument(&doc);

        QMimeData mime;
        mime.setText(QStringLiteral("Dropped plain text"));

        QGraphicsSceneDragDropEvent dropEv(QEvent::GraphicsSceneDrop);
        dropEv.setMimeData(&mime);
        dropEv.setScenePos(QPointF(500, 500));
        QCoreApplication::sendEvent(&scene, &dropEv);

        QCOMPARE(doc.nodes().size(), 1);
        const auto node = doc.nodes().first();
        QCOMPARE(node.type, Canvas::NodeType::Text);
        QCOMPARE(node.text, QStringLiteral("Dropped plain text"));
        QCOMPARE(node.width, 250);
        QCOMPARE(node.height, 60);
        QCOMPARE(node.x, 500 - 125);
        QCOMPARE(node.y, 500 - 30);
        QCOMPARE(scene.undoStack()->count(), 1);
    }

    void testDropMultipleFilesLaysOutInGrid()
    {
        Canvas::CanvasDocument doc;
        Canvas::CanvasScene scene;
        scene.setDocument(&doc);

        scene.setVaultPathResolver([](const QString &absPath) -> QString {
            return absPath.mid(1); // strip leading '/' -> pretend vault root is "/"
        });

        QMimeData mime;
        mime.setUrls({
            QUrl::fromLocalFile(QStringLiteral("/a.md")),
            QUrl::fromLocalFile(QStringLiteral("/b.md")),
            QUrl::fromLocalFile(QStringLiteral("/c.md")),
            QUrl::fromLocalFile(QStringLiteral("/d.md")),
        });

        QGraphicsSceneDragDropEvent dropEv(QEvent::GraphicsSceneDrop);
        dropEv.setMimeData(&mime);
        dropEv.setScenePos(QPointF(0, 0));
        QCoreApplication::sendEvent(&scene, &dropEv);

        QCOMPARE(doc.nodes().size(), 4);
        // Single compound undo command for the whole drop.
        QCOMPARE(scene.undoStack()->count(), 1);

        // 2x2 grid, 400x400 nodes, 20px gap.
        QSet<QPair<int, int>> positions;
        for (const auto &n : doc.nodes())
            positions.insert({n.x, n.y});
        QSet<QPair<int, int>> expected = {
            {0, 0}, {420, 0}, {0, 420}, {420, 420},
        };
        QCOMPARE(positions, expected);
    }

    void testCopyPasteRoundTripReIds()
    {
        Canvas::CanvasDocument doc;
        Canvas::CanvasNode n1;
        n1.id = QStringLiteral("a"); n1.type = Canvas::NodeType::Text;
        n1.x = 0; n1.y = 0; n1.width = 100; n1.height = 60;
        n1.text = QStringLiteral("Hello");
        Canvas::CanvasNode n2;
        n2.id = QStringLiteral("b"); n2.type = Canvas::NodeType::Text;
        n2.x = 200; n2.y = 0; n2.width = 100; n2.height = 60;
        doc.addNode(n1);
        doc.addNode(n2);

        Canvas::CanvasEdge edge;
        edge.id = QStringLiteral("e1");
        edge.fromNode = QStringLiteral("a");
        edge.toNode = QStringLiteral("b");
        doc.addEdge(edge);

        Canvas::CanvasScene scene;
        scene.setDocument(&doc);

        auto *itemA = scene.textCardItem(QStringLiteral("a"));
        auto *itemB = scene.textCardItem(QStringLiteral("b"));
        QVERIFY(itemA && itemB);
        itemA->setSelected(true);
        itemB->setSelected(true);

        const QString json = scene.serializeSelectionAsCanvasJson();
        QVERIFY(!json.isEmpty());
        QVERIFY(json.contains(QStringLiteral("\"nodes\"")));
        QVERIFY(json.contains(QStringLiteral("\"edges\"")));

        scene.pasteCanvasJsonOrText(json, QPointF(1000, 1000));

        // Original 2 nodes + 1 edge, plus 2 pasted nodes + 1 pasted edge.
        QCOMPARE(doc.nodes().size(), 4);
        QCOMPARE(doc.edges().size(), 2);
        QCOMPARE(scene.undoStack()->count(), 1);

        bool foundOffsetA = false, foundOffsetB = false;
        for (const auto &n : doc.nodes()) {
            if (n.id == QStringLiteral("a") || n.id == QStringLiteral("b"))
                continue;
            // Fresh 16-hex id.
            QCOMPARE(n.id.length(), 16);
            if (n.text == QStringLiteral("Hello")) {
                QCOMPARE(n.x, 16);
                QCOMPARE(n.y, 16);
                foundOffsetA = true;
            } else {
                QCOMPARE(n.x, 216);
                QCOMPARE(n.y, 16);
                foundOffsetB = true;
            }
        }
        QVERIFY(foundOffsetA);
        QVERIFY(foundOffsetB);

        // Pasted edge references the new (remapped) node ids, not "a"/"b".
        bool foundPastedEdge = false;
        for (const auto &e : doc.edges()) {
            if (e.id == QStringLiteral("e1"))
                continue;
            QVERIFY(e.fromNode != QStringLiteral("a"));
            QVERIFY(e.toNode != QStringLiteral("b"));
            QVERIFY(doc.hasNode(e.fromNode));
            QVERIFY(doc.hasNode(e.toNode));
            foundPastedEdge = true;
        }
        QVERIFY(foundPastedEdge);
    }

    void testPastePlainTextCreatesCard()
    {
        Canvas::CanvasDocument doc;
        Canvas::CanvasScene scene;
        scene.setDocument(&doc);

        scene.pasteCanvasJsonOrText(QStringLiteral("Just some plain text"), QPointF(500, 500));

        QCOMPARE(doc.nodes().size(), 1);
        const auto node = doc.nodes().first();
        QCOMPARE(node.type, Canvas::NodeType::Text);
        QCOMPARE(node.text, QStringLiteral("Just some plain text"));
        QCOMPARE(node.x, 500 - 125);
        QCOMPARE(node.y, 500 - 30);
        QCOMPARE(scene.undoStack()->count(), 1);
    }

    void testAltDragDuplicates()
    {
        Canvas::CanvasDocument doc;
        Canvas::CanvasNode n1;
        n1.id = QStringLiteral("a"); n1.type = Canvas::NodeType::Text;
        n1.x = 0; n1.y = 0; n1.width = 100; n1.height = 60;
        n1.text = QStringLiteral("Alpha");
        Canvas::CanvasNode n2;
        n2.id = QStringLiteral("b"); n2.type = Canvas::NodeType::Text;
        n2.x = 200; n2.y = 0; n2.width = 100; n2.height = 60;
        n2.text = QStringLiteral("Beta");
        doc.addNode(n1);
        doc.addNode(n2);

        Canvas::CanvasEdge edge;
        edge.id = QStringLiteral("e1");
        edge.fromNode = QStringLiteral("a");
        edge.toNode = QStringLiteral("b");
        doc.addEdge(edge);

        Canvas::CanvasScene scene;
        scene.setDocument(&doc);

        auto *itemA = scene.textCardItem(QStringLiteral("a"));
        auto *itemB = scene.textCardItem(QStringLiteral("b"));
        QVERIFY(itemA && itemB);
        itemA->setSelected(true);
        itemB->setSelected(true);

        // Alt+press on the already-selected node "a", drag by (50, 30).
        const QPointF pressPos(50, 30); // inside itemA's 100x60 rect
        const QPointF movePos = pressPos + QPointF(50, 30);

        QGraphicsSceneMouseEvent press(QEvent::GraphicsSceneMousePress);
        press.setScenePos(pressPos);
        press.setButton(Qt::LeftButton);
        press.setButtons(Qt::LeftButton);
        press.setModifiers(Qt::AltModifier);
        QCoreApplication::sendEvent(&scene, &press);

        QGraphicsSceneMouseEvent move(QEvent::GraphicsSceneMouseMove);
        move.setScenePos(movePos);
        move.setLastScenePos(pressPos);
        move.setButtons(Qt::LeftButton);
        move.setModifiers(Qt::AltModifier);
        QCoreApplication::sendEvent(&scene, &move);

        QGraphicsSceneMouseEvent release(QEvent::GraphicsSceneMouseRelease);
        release.setScenePos(movePos);
        release.setButton(Qt::LeftButton);
        release.setButtons(Qt::NoButton);
        release.setModifiers(Qt::AltModifier);
        QCoreApplication::sendEvent(&scene, &release);

        // Originals untouched, two new nodes + one new edge, single undo step.
        QCOMPARE(doc.nodes().size(), 4);
        QCOMPARE(doc.edges().size(), 2);
        QCOMPARE(scene.undoStack()->count(), 1);

        auto origA = doc.node(QStringLiteral("a"));
        auto origB = doc.node(QStringLiteral("b"));
        QCOMPARE(origA.x, 0);
        QCOMPARE(origA.y, 0);
        QCOMPARE(origB.x, 200);
        QCOMPARE(origB.y, 0);

        bool foundCloneA = false, foundCloneB = false;
        for (const auto &n : doc.nodes()) {
            if (n.id == QStringLiteral("a") || n.id == QStringLiteral("b"))
                continue;
            QCOMPARE(n.id.length(), 16);
            if (n.text == QStringLiteral("Alpha")) {
                QCOMPARE(n.x, 50);  // 0 + drag delta (50,30)
                QCOMPARE(n.y, 30);
                foundCloneA = true;
            } else if (n.text == QStringLiteral("Beta")) {
                QCOMPARE(n.x, 250); // 200 + drag delta (50,30)
                QCOMPARE(n.y, 30);
                foundCloneB = true;
            }
        }
        QVERIFY(foundCloneA);
        QVERIFY(foundCloneB);

        bool foundClonedEdge = false;
        for (const auto &e : doc.edges()) {
            if (e.id == QStringLiteral("e1"))
                continue;
            QVERIFY(e.fromNode != QStringLiteral("a"));
            QVERIFY(e.toNode != QStringLiteral("b"));
            foundClonedEdge = true;
        }
        QVERIFY(foundClonedEdge);

        // The clones (not the originals) end up selected.
        QVERIFY(!itemA->isSelected());
        QVERIFY(!itemB->isSelected());
    }

    void testAltClickWithoutDragDoesNotDuplicate()
    {
        // Regression: mousePressEvent clones the selection eagerly (the
        // tool needs live items to drag), but a bare Alt+click with no
        // real movement used to still commit a duplicate on release. Below
        // QApplication::startDragDistance(), the clones must be discarded
        // and the original selection restored instead.
        Canvas::CanvasDocument doc;
        Canvas::CanvasNode n1;
        n1.id = QStringLiteral("a"); n1.type = Canvas::NodeType::Text;
        n1.x = 0; n1.y = 0; n1.width = 100; n1.height = 60;
        doc.addNode(n1);

        Canvas::CanvasScene scene;
        scene.setDocument(&doc);

        auto *itemA = scene.textCardItem(QStringLiteral("a"));
        QVERIFY(itemA);
        itemA->setSelected(true);

        const QPointF pressPos(50, 30);

        QGraphicsSceneMouseEvent press(QEvent::GraphicsSceneMousePress);
        press.setScenePos(pressPos);
        press.setButton(Qt::LeftButton);
        press.setButtons(Qt::LeftButton);
        press.setModifiers(Qt::AltModifier);
        QCoreApplication::sendEvent(&scene, &press);

        // No move event — release at the exact press position.
        QGraphicsSceneMouseEvent release(QEvent::GraphicsSceneMouseRelease);
        release.setScenePos(pressPos);
        release.setButton(Qt::LeftButton);
        release.setButtons(Qt::NoButton);
        release.setModifiers(Qt::AltModifier);
        QCoreApplication::sendEvent(&scene, &release);

        QCOMPARE(doc.nodes().size(), 1);
        QCOMPARE(scene.undoStack()->count(), 0);
        QVERIFY(itemA->isSelected());
    }

    // -----------------------------------------------------------------
    // M3.2 — hover connection points + edge-create gesture
    // -----------------------------------------------------------------

    void testEdgeCreateDragBetweenFileCards()
    {
        // Two file cards side by side. Press near card1's right-face
        // anchor (compassAnchors midpoint = (200,75) for a 0,0,200,150
        // rect), drag across to card2 (400,0,200,150; left anchor at
        // (400,75)), release. This should route through CompositeTool's
        // anchor route into CreateEdgeTool and emit edgeRequested, which
        // CanvasScene::onEdgeRequested turns into a CmdAddEdge.
        Canvas::CanvasDocument doc;
        Canvas::CanvasNode n1;
        n1.id = QStringLiteral("card1"); n1.type = Canvas::NodeType::File;
        n1.file = QStringLiteral("one.md");
        n1.x = 0; n1.y = 0; n1.width = 200; n1.height = 150;
        doc.addNode(n1);

        Canvas::CanvasNode n2;
        n2.id = QStringLiteral("card2"); n2.type = Canvas::NodeType::File;
        n2.file = QStringLiteral("two.md");
        n2.x = 400; n2.y = 0; n2.width = 200; n2.height = 150;
        doc.addNode(n2);

        Canvas::CanvasScene scene;
        scene.setDocument(&doc);

        QVERIFY(scene.fileCardItem(QStringLiteral("card1")));
        QVERIFY(scene.fileCardItem(QStringLiteral("card2")));

        // Just inside card1's rect, within the 12px anchor hit radius of
        // its right-face anchor (200,75).
        const QPointF pressPos(198, 75);
        // Just inside card2's rect, near its left-face anchor (400,75).
        const QPointF releasePos(402, 75);

        QGraphicsSceneMouseEvent press(QEvent::GraphicsSceneMousePress);
        press.setScenePos(pressPos);
        press.setButton(Qt::LeftButton);
        press.setButtons(Qt::LeftButton);
        QCoreApplication::sendEvent(&scene, &press);

        QGraphicsSceneMouseEvent move(QEvent::GraphicsSceneMouseMove);
        move.setScenePos(releasePos);
        move.setLastScenePos(pressPos);
        move.setButtons(Qt::LeftButton);
        QCoreApplication::sendEvent(&scene, &move);

        QGraphicsSceneMouseEvent release(QEvent::GraphicsSceneMouseRelease);
        release.setScenePos(releasePos);
        release.setButton(Qt::LeftButton);
        release.setButtons(Qt::NoButton);
        QCoreApplication::sendEvent(&scene, &release);

        QCOMPARE(doc.edges().size(), 1);
        const Canvas::CanvasEdge edge = doc.edges().first();
        QCOMPARE(edge.fromNode, QStringLiteral("card1"));
        QCOMPARE(edge.toNode, QStringLiteral("card2"));
        QCOMPARE(edge.fromSide, Canvas::Side::Right);
        QCOMPARE(edge.toSide, Canvas::Side::Left);
        QCOMPARE(scene.undoStack()->count(), 1);

        // Undo removes the edge.
        scene.undoStack()->undo();
        QCOMPARE(doc.edges().size(), 0);
    }

    void testEdgeCreateDefaultsToArrowHead()
    {
        // Appendix A: fromEnd defaults to "none" (nondirectional origin),
        // toEnd defaults to "arrow" (Obsidian's directed-edge default).
        Canvas::CanvasDocument doc;
        Canvas::CanvasNode n1;
        n1.id = QStringLiteral("card1"); n1.type = Canvas::NodeType::Text;
        n1.x = 0; n1.y = 0; n1.width = 200; n1.height = 150;
        doc.addNode(n1);

        Canvas::CanvasNode n2;
        n2.id = QStringLiteral("card2"); n2.type = Canvas::NodeType::Text;
        n2.x = 400; n2.y = 0; n2.width = 200; n2.height = 150;
        doc.addNode(n2);

        Canvas::CanvasScene scene;
        scene.setDocument(&doc);

        const QPointF pressPos(198, 75);
        const QPointF releasePos(402, 75);

        QGraphicsSceneMouseEvent press(QEvent::GraphicsSceneMousePress);
        press.setScenePos(pressPos);
        press.setButton(Qt::LeftButton);
        press.setButtons(Qt::LeftButton);
        QCoreApplication::sendEvent(&scene, &press);

        QGraphicsSceneMouseEvent move(QEvent::GraphicsSceneMouseMove);
        move.setScenePos(releasePos);
        move.setLastScenePos(pressPos);
        move.setButtons(Qt::LeftButton);
        QCoreApplication::sendEvent(&scene, &move);

        QGraphicsSceneMouseEvent release(QEvent::GraphicsSceneMouseRelease);
        release.setScenePos(releasePos);
        release.setButton(Qt::LeftButton);
        release.setButtons(Qt::NoButton);
        QCoreApplication::sendEvent(&scene, &release);

        QCOMPARE(doc.edges().size(), 1);
        const Canvas::CanvasEdge edge = doc.edges().first();
        QCOMPARE(edge.fromEnd, Canvas::EndType::None);
        QCOMPARE(edge.toEnd, Canvas::EndType::Arrow);
    }

    // -----------------------------------------------------------------
    // M3.3 — drop-on-empty create-and-connect
    // -----------------------------------------------------------------

    void testEdgeDropOnEmptyCreatesConnectedCard()
    {
        // A real QMenu::exec() popup blocks in a nested event loop, which
        // this codebase has no existing precedent for driving synchronously
        // in an offscreen unit test (grepped tst_canvasscene.cpp and the
        // rest of libs/canvas/tests for QMenu/QTimer::singleShot patterns —
        // none exist). Per the M3.3 task brief, option (b): the menu-popup
        // slot (CanvasScene::onEdgeDroppedOnEmpty) is a thin QMenu wrapper
        // around CanvasScene::addCardConnectedTo(), which is public
        // (same rationale as createFileCardViaPicker()) and does all the
        // actual compound-command work. This test drives that helper
        // directly — the menu-popup wiring itself is left to the Phase M3
        // live-eyeball gate rather than offscreen coverage (Appendix, exit
        // criteria note; project memory: interactive popups routinely excluded
        // from offscreen suites in this codebase).
        Canvas::CanvasDocument doc;
        Canvas::CanvasNode source;
        source.id = QStringLiteral("card1");
        source.type = Canvas::NodeType::File;
        source.file = QStringLiteral("one.md");
        source.x = 0; source.y = 0; source.width = 200; source.height = 150;
        doc.addNode(source);

        Canvas::CanvasScene scene;
        scene.setDocument(&doc);

        auto *sourceItem = scene.connectableItem(QStringLiteral("card1"));
        QVERIFY(sourceItem != nullptr);

        // New text card dropped well to the right of the source node, so
        // toSide should resolve to Left (facing back toward the source).
        Canvas::CanvasNode node;
        node.id = Canvas::CanvasDocument::generateId();
        node.type = Canvas::NodeType::Text;
        node.x = 600; node.y = 20;
        node.width = 250; node.height = 60;

        QCOMPARE(doc.nodes().size(), 1);
        QCOMPARE(doc.edges().size(), 0);

        scene.addCardConnectedTo(node, sourceItem, QStringLiteral("right"));

        QCOMPARE(doc.nodes().size(), 2);
        QCOMPARE(doc.edges().size(), 1);
        QCOMPARE(scene.undoStack()->count(), 1);

        const Canvas::CanvasEdge edge = doc.edges().first();
        QCOMPARE(edge.fromNode, QStringLiteral("card1"));
        QCOMPARE(edge.toNode, node.id);
        QCOMPARE(edge.fromSide, Canvas::Side::Right);
        QCOMPARE(edge.toSide, Canvas::Side::Left);
        QCOMPARE(edge.fromEnd, Canvas::EndType::None);
        QCOMPARE(edge.toEnd, Canvas::EndType::Arrow);

        auto *newItem = scene.connectableItem(node.id);
        QVERIFY(newItem != nullptr);
        QVERIFY(newItem->isSelected());

        // Both the node add and the edge add undo in one step.
        scene.undoStack()->undo();
        QCOMPARE(doc.nodes().size(), 1);
        QCOMPARE(doc.edges().size(), 0);
    }

    // -----------------------------------------------------------------
    // M3.4 — endpoint reconnect
    // -----------------------------------------------------------------

    void testReconnectEdgeEndpoint()
    {
        // card1 (0,0,200,150) --edge--> card2 (400,0,200,150), plus a third
        // node card3 (400,300,200,150) to drag the target endpoint onto.
        Canvas::CanvasDocument doc;
        Canvas::CanvasNode n1;
        n1.id = QStringLiteral("card1"); n1.type = Canvas::NodeType::File;
        n1.file = QStringLiteral("one.md");
        n1.x = 0; n1.y = 0; n1.width = 200; n1.height = 150;
        doc.addNode(n1);

        Canvas::CanvasNode n2;
        n2.id = QStringLiteral("card2"); n2.type = Canvas::NodeType::File;
        n2.file = QStringLiteral("two.md");
        n2.x = 400; n2.y = 0; n2.width = 200; n2.height = 150;
        doc.addNode(n2);

        Canvas::CanvasNode n3;
        n3.id = QStringLiteral("card3"); n3.type = Canvas::NodeType::File;
        n3.file = QStringLiteral("three.md");
        n3.x = 400; n3.y = 300; n3.width = 200; n3.height = 150;
        doc.addNode(n3);

        Canvas::CanvasEdge edge;
        edge.id = QStringLiteral("edge1");
        edge.fromNode = QStringLiteral("card1");
        edge.toNode = QStringLiteral("card2");
        edge.fromSide = Canvas::Side::Right;
        edge.toSide = Canvas::Side::Left;
        doc.addEdge(edge);

        Canvas::CanvasScene scene;
        scene.setDocument(&doc);

        auto *card3Item = scene.connectableItem(QStringLiteral("card3"));
        QVERIFY(card3Item != nullptr);

        // Sanity: the edge item is really attached to card1/card2 before
        // the drag (this is the "still points at old node" bug guard).
        auto *edgeItemBefore = scene.edgeItem(QStringLiteral("edge1"));
        QVERIFY(edgeItemBefore != nullptr);
        QCOMPARE(edgeItemBefore->sourceNode(), static_cast<Graffodil::IGraphNode *>(scene.connectableItem(QStringLiteral("card1"))));
        QCOMPARE(edgeItemBefore->targetNode(), static_cast<Graffodil::IGraphNode *>(scene.connectableItem(QStringLiteral("card2"))));

        // Press within 8px of the edge's target terminus — card2's left
        // anchor at (400,75), which per M3.1 is exactly where the terminus
        // tip is drawn. Drag down to card3's top anchor at (500,300).
        const QPointF pressPos(400, 75);
        const QPointF releasePos(500, 300);

        QGraphicsSceneMouseEvent press(QEvent::GraphicsSceneMousePress);
        press.setScenePos(pressPos);
        press.setButton(Qt::LeftButton);
        press.setButtons(Qt::LeftButton);
        QCoreApplication::sendEvent(&scene, &press);

        QGraphicsSceneMouseEvent move(QEvent::GraphicsSceneMouseMove);
        move.setScenePos(releasePos);
        move.setLastScenePos(pressPos);
        move.setButtons(Qt::LeftButton);
        QCoreApplication::sendEvent(&scene, &move);

        QGraphicsSceneMouseEvent release(QEvent::GraphicsSceneMouseRelease);
        release.setScenePos(releasePos);
        release.setButton(Qt::LeftButton);
        release.setButtons(Qt::NoButton);
        QCoreApplication::sendEvent(&scene, &release);

        QCOMPARE(doc.edges().size(), 1);
        const Canvas::CanvasEdge updated = doc.edge(QStringLiteral("edge1"));
        QCOMPARE(updated.fromNode, QStringLiteral("card1"));
        QCOMPARE(updated.toNode, QStringLiteral("card3"));
        QCOMPARE(updated.fromSide, Canvas::Side::Right);
        QCOMPARE(updated.toSide, Canvas::Side::Top);
        QCOMPARE(scene.undoStack()->count(), 1);

        // The scene item must now really point at card3 (not just have its
        // anchor-id string updated) — this is the onEdgeChanged node-rebuild
        // gap the M3.4 brief calls out.
        auto *edgeItemAfter = scene.edgeItem(QStringLiteral("edge1"));
        QVERIFY(edgeItemAfter != nullptr);
        QCOMPARE(edgeItemAfter->sourceNode(), static_cast<Graffodil::IGraphNode *>(scene.connectableItem(QStringLiteral("card1"))));
        QCOMPARE(edgeItemAfter->targetNode(), static_cast<Graffodil::IGraphNode *>(card3Item));

        // Undo restores the original connection.
        scene.undoStack()->undo();
        const Canvas::CanvasEdge restored = doc.edge(QStringLiteral("edge1"));
        QCOMPARE(restored.toNode, QStringLiteral("card2"));
        QCOMPARE(restored.toSide, Canvas::Side::Left);

        auto *edgeItemUndone = scene.edgeItem(QStringLiteral("edge1"));
        QVERIFY(edgeItemUndone != nullptr);
        QCOMPARE(edgeItemUndone->targetNode(), static_cast<Graffodil::IGraphNode *>(scene.connectableItem(QStringLiteral("card2"))));
    }

    void testEndpointDropOnEmptyDeletesEdge()
    {
        // Same two-card setup as testEdgeCreateDragBetweenFileCards, but
        // starting from an existing edge and dragging its target endpoint
        // out to empty canvas — Obsidian semantics: deletes the edge.
        Canvas::CanvasDocument doc;
        Canvas::CanvasNode n1;
        n1.id = QStringLiteral("card1"); n1.type = Canvas::NodeType::File;
        n1.file = QStringLiteral("one.md");
        n1.x = 0; n1.y = 0; n1.width = 200; n1.height = 150;
        doc.addNode(n1);

        Canvas::CanvasNode n2;
        n2.id = QStringLiteral("card2"); n2.type = Canvas::NodeType::File;
        n2.file = QStringLiteral("two.md");
        n2.x = 400; n2.y = 0; n2.width = 200; n2.height = 150;
        doc.addNode(n2);

        Canvas::CanvasEdge edge;
        edge.id = QStringLiteral("edge1");
        edge.fromNode = QStringLiteral("card1");
        edge.toNode = QStringLiteral("card2");
        edge.fromSide = Canvas::Side::Right;
        edge.toSide = Canvas::Side::Left;
        doc.addEdge(edge);

        Canvas::CanvasScene scene;
        scene.setDocument(&doc);

        QCOMPARE(doc.edges().size(), 1);

        const QPointF pressPos(400, 75);      // card2's left (target) anchor
        const QPointF releasePos(1000, 1000); // far off in empty canvas

        QGraphicsSceneMouseEvent press(QEvent::GraphicsSceneMousePress);
        press.setScenePos(pressPos);
        press.setButton(Qt::LeftButton);
        press.setButtons(Qt::LeftButton);
        QCoreApplication::sendEvent(&scene, &press);

        QGraphicsSceneMouseEvent move(QEvent::GraphicsSceneMouseMove);
        move.setScenePos(releasePos);
        move.setLastScenePos(pressPos);
        move.setButtons(Qt::LeftButton);
        QCoreApplication::sendEvent(&scene, &move);

        QGraphicsSceneMouseEvent release(QEvent::GraphicsSceneMouseRelease);
        release.setScenePos(releasePos);
        release.setButton(Qt::LeftButton);
        release.setButtons(Qt::NoButton);
        QCoreApplication::sendEvent(&scene, &release);

        QCOMPARE(doc.edges().size(), 0);
        QVERIFY(scene.edgeItem(QStringLiteral("edge1")) == nullptr);
        QCOMPARE(scene.undoStack()->count(), 1);

        // Undo restores the edge.
        scene.undoStack()->undo();
        QCOMPARE(doc.edges().size(), 1);
        QVERIFY(scene.edgeItem(QStringLiteral("edge1")) != nullptr);
    }

    // -----------------------------------------------------------------
    // M3.5 — direction menu + undoable reverse
    // -----------------------------------------------------------------

    void testDirectionMenuWritesEndFields()
    {
        Canvas::CanvasDocument doc;
        Canvas::CanvasNode n1;
        n1.id = QStringLiteral("card1"); n1.type = Canvas::NodeType::File;
        n1.file = QStringLiteral("one.md");
        n1.x = 0; n1.y = 0; n1.width = 200; n1.height = 150;
        doc.addNode(n1);

        Canvas::CanvasNode n2;
        n2.id = QStringLiteral("card2"); n2.type = Canvas::NodeType::File;
        n2.file = QStringLiteral("two.md");
        n2.x = 400; n2.y = 0; n2.width = 200; n2.height = 150;
        doc.addNode(n2);

        Canvas::CanvasEdge edge;
        edge.id = QStringLiteral("edge1");
        edge.fromNode = QStringLiteral("card1");
        edge.toNode = QStringLiteral("card2");
        edge.fromSide = Canvas::Side::Right;
        edge.toSide = Canvas::Side::Left;
        // Starting state is the Obsidian/CanvasEdge default: unidirectional.
        edge.fromEnd = Canvas::EndType::None;
        edge.toEnd = Canvas::EndType::Arrow;
        doc.addEdge(edge);

        Canvas::CanvasScene scene;
        scene.setDocument(&doc);

        struct Choice { Canvas::EndType from; Canvas::EndType to; };
        const Choice choices[] = {
            { Canvas::EndType::None, Canvas::EndType::None },   // Nondirectional
            { Canvas::EndType::None, Canvas::EndType::Arrow },  // Unidirectional
            { Canvas::EndType::Arrow, Canvas::EndType::Arrow }, // Bidirectional
        };

        for (const auto &choice : choices) {
            const Canvas::CanvasEdge before = doc.edge(QStringLiteral("edge1"));
            const int stackCountBefore = scene.undoStack()->count();

            scene.setEdgeEnds(QStringLiteral("edge1"), choice.from, choice.to);

            const Canvas::CanvasEdge after = doc.edge(QStringLiteral("edge1"));
            QCOMPARE(after.fromEnd, choice.from);
            QCOMPARE(after.toEnd, choice.to);
            QCOMPARE(scene.undoStack()->count(), stackCountBefore + 1);

            // Same-two-nodes fast path: the item stays attached, no
            // remove+recreate.
            auto *item = scene.edgeItem(QStringLiteral("edge1"));
            QVERIFY(item != nullptr);

            scene.undoStack()->undo();
            const Canvas::CanvasEdge restored = doc.edge(QStringLiteral("edge1"));
            QCOMPARE(restored.fromEnd, before.fromEnd);
            QCOMPARE(restored.toEnd, before.toEnd);

            // Redo back to the chosen state so the next iteration starts
            // from a clean, known undo-stack position.
            scene.undoStack()->redo();
        }
    }

    void testReverseDirectionIsUndoable()
    {
        Canvas::CanvasDocument doc;
        Canvas::CanvasNode n1;
        n1.id = QStringLiteral("card1"); n1.type = Canvas::NodeType::File;
        n1.file = QStringLiteral("one.md");
        n1.x = 0; n1.y = 0; n1.width = 200; n1.height = 150;
        doc.addNode(n1);

        Canvas::CanvasNode n2;
        n2.id = QStringLiteral("card2"); n2.type = Canvas::NodeType::File;
        n2.file = QStringLiteral("two.md");
        n2.x = 400; n2.y = 0; n2.width = 200; n2.height = 150;
        doc.addNode(n2);

        Canvas::CanvasEdge edge;
        edge.id = QStringLiteral("edge1");
        edge.fromNode = QStringLiteral("card1");
        edge.toNode = QStringLiteral("card2");
        edge.fromSide = Canvas::Side::Right;
        edge.toSide = Canvas::Side::Left;
        edge.fromEnd = Canvas::EndType::None;
        edge.toEnd = Canvas::EndType::Arrow;
        doc.addEdge(edge);

        Canvas::CanvasScene scene;
        scene.setDocument(&doc);

        QCOMPARE(scene.undoStack()->count(), 0);

        // Exercise the same reverseEdge() path the "Reverse Direction"
        // context-menu action and the R-key handler both call through.
        scene.reverseEdge(QStringLiteral("edge1"));

        // Previously this bypassed the undo stack entirely (the bug this
        // task fixes) — it must now be a real, undoable step.
        QCOMPARE(scene.undoStack()->count(), 1);

        const Canvas::CanvasEdge reversed = doc.edge(QStringLiteral("edge1"));
        QCOMPARE(reversed.fromNode, QStringLiteral("card2"));
        QCOMPARE(reversed.toNode, QStringLiteral("card1"));
        QCOMPARE(reversed.fromSide, Canvas::Side::Left);
        QCOMPARE(reversed.toSide, Canvas::Side::Right);
        QCOMPARE(reversed.fromEnd, Canvas::EndType::Arrow);
        QCOMPARE(reversed.toEnd, Canvas::EndType::None);

        // The scene item must really point at the swapped nodes (node
        // identity changed, so this goes through onEdgeChanged's
        // remove+recreate branch, same as reconnect in M3.4).
        auto *itemAfter = scene.edgeItem(QStringLiteral("edge1"));
        QVERIFY(itemAfter != nullptr);
        QCOMPARE(itemAfter->sourceNode(), static_cast<Graffodil::IGraphNode *>(scene.connectableItem(QStringLiteral("card2"))));
        QCOMPARE(itemAfter->targetNode(), static_cast<Graffodil::IGraphNode *>(scene.connectableItem(QStringLiteral("card1"))));

        scene.undoStack()->undo();
        const Canvas::CanvasEdge restored = doc.edge(QStringLiteral("edge1"));
        QCOMPARE(restored.fromNode, QStringLiteral("card1"));
        QCOMPARE(restored.toNode, QStringLiteral("card2"));
        QCOMPARE(restored.fromSide, Canvas::Side::Right);
        QCOMPARE(restored.toSide, Canvas::Side::Left);
        QCOMPARE(restored.fromEnd, Canvas::EndType::None);
        QCOMPARE(restored.toEnd, Canvas::EndType::Arrow);

        auto *itemUndone = scene.edgeItem(QStringLiteral("edge1"));
        QVERIFY(itemUndone != nullptr);
        QCOMPARE(itemUndone->sourceNode(), static_cast<Graffodil::IGraphNode *>(scene.connectableItem(QStringLiteral("card1"))));
        QCOMPARE(itemUndone->targetNode(), static_cast<Graffodil::IGraphNode *>(scene.connectableItem(QStringLiteral("card2"))));
    }
};

QTEST_MAIN(TestCanvasScene)
#include "tst_canvasscene.moc"
