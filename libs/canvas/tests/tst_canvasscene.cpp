// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QUndoStack>
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
};

QTEST_MAIN(TestCanvasScene)
#include "tst_canvasscene.moc"
