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
};

QTEST_MAIN(TestCanvasScene)
#include "tst_canvasscene.moc"
