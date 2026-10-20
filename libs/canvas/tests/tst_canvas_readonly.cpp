// SPDX-License-Identifier: GPL-3.0-or-later
//
// The canvas read-only lock. Obsidian's side of the
// contract, re-verified against RE/obsidian/1.13.7:
//   * flag       app/own/2018-k7.js:81 (`this.readonly = !1`), toggled by
//                Canvas.setReadonly at :1395-1403
//   * per-file   localStorage["canvas-" + TFile.path] (:2028-n9.js:560-579),
//                payload {readonly: true}
//   * load guard :2028-n9.js:879 — hasOwnProperty("readonly") plus
//                typeof === "boolean", else ignored
//   * gates      delete :715, cut :976, paste :1003, double-click-create
//                :1368, background context menu :1377, selection-drag :1612,
//                selection-menu items :1519/1527/1537, edge menu
//                (:2028-n9.js:168), card/file/group inline edit
//                (:2022-W7.js:41/257/382/720-723), base startEditing
//                (:2020-O7.js:247)
//
// Verified NOT gated in Obsidian, gated here on purpose (each has a test):
//   * drop        app/own/2018-k7.js:265-341 has no readonly check
//   * resize      app/own/1999-J8.js:60 onResizePointerdown
//   * undo/redo   app/own/2018-k7.js:239-254 (control cluster) and :481
//
// Verified NOT gated in Obsidian and NOT gated here: selection (the onKeydown
// handler at :1108 only deselects; Escape works either way) and copy (:996).

#include <QCoreApplication>
#include <QGraphicsSceneDragDropEvent>
#include <QGraphicsSceneMouseEvent>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QApplication>
#include <QGraphicsProxyWidget>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMenu>
#include <QMimeData>
#include <QScrollBar>
#include <QSignalSpy>
#include <QTest>
#include <QUrl>

#include <KLocalizedString>

#include <graffodil/IGraphEdge.h>
#include <graffodil/IGraphNode.h>

#include "canvas/CanvasCommands.h"
#include "canvas/CanvasDocument.h"
#include "canvas/CanvasNodeChromeOverlay.h"
#include "canvas/CanvasScene.h"
#include "canvas/CanvasView.h"
#include "canvas/EdgeItem.h"
#include "canvas/FileCardItem.h"
#include "canvas/GroupItem.h"
#include "canvas/TextCardItem.h"

namespace {

/// CanvasScene narrows QGraphicsScene::keyPressEvent to protected; this is
/// the narrowest way to drive the scene's own key routing.
class SceneHarness : public Canvas::CanvasScene
{
public:
    using Canvas::CanvasScene::CanvasScene;
    using Canvas::CanvasScene::keyPressEvent;
};

void doubleClick(QGraphicsScene *scene, const QPointF &pos)
{
    QGraphicsSceneMouseEvent ev(QEvent::GraphicsSceneMouseDoubleClick);
    ev.setScenePos(pos);
    ev.setButton(Qt::LeftButton);
    ev.setButtons(Qt::LeftButton);
    QCoreApplication::sendEvent(scene, &ev);
}

Canvas::CanvasNode textNode(const QString &id, int x, int y, const QString &text = {})
{
    Canvas::CanvasNode n;
    n.id = id;
    n.type = Canvas::NodeType::Text;
    n.x = x;
    n.y = y;
    n.width = 200;
    n.height = 80;
    n.text = text;
    return n;
}

Canvas::CanvasNode groupNode(const QString &id, int x, int y)
{
    Canvas::CanvasNode n;
    n.id = id;
    n.type = Canvas::NodeType::Group;
    n.x = x;
    n.y = y;
    n.width = 400;
    n.height = 300;
    n.label = QStringLiteral("G");
    return n;
}

/// Press / drag / release a gesture through the scene, the way
/// tst_canvasscene.cpp's own gesture tests do.
void drag(QGraphicsScene *scene, const QPointF &from, const QPointF &to,
          Qt::MouseButton button = Qt::LeftButton,
          Qt::KeyboardModifiers mods = Qt::NoModifier)
{
    QGraphicsSceneMouseEvent press(QEvent::GraphicsSceneMousePress);
    press.setScenePos(from);
    press.setScreenPos(from.toPoint());   // PanZoomTool reads screenPos, not scenePos
    press.setButton(button);
    press.setButtons(button);
    press.setModifiers(mods);
    QCoreApplication::sendEvent(scene, &press);

    QGraphicsSceneMouseEvent move(QEvent::GraphicsSceneMouseMove);
    move.setScenePos(to);
    move.setLastScenePos(from);
    move.setScreenPos(to.toPoint());
    move.setButton(button);
    move.setButtons(button);
    move.setModifiers(mods);
    QCoreApplication::sendEvent(scene, &move);

    QGraphicsSceneMouseEvent release(QEvent::GraphicsSceneMouseRelease);
    release.setScenePos(to);
    release.setScreenPos(to.toPoint());
    release.setButton(button);
    release.setButtons(Qt::NoButton);
    release.setModifiers(mods);
    QCoreApplication::sendEvent(scene, &release);
}

void pressKey(SceneHarness *scene, Qt::Key key,
              Qt::KeyboardModifiers mods = Qt::NoModifier)
{
    QKeyEvent ev(QEvent::KeyPress, key, mods);
    scene->keyPressEvent(&ev);
}

/// The group-label editor is a QLineEdit in a proxy widget, and
/// CanvasScene::isEditing() only reports the text-card / file-card proxy, so
/// the scene's own item set is the only externally visible signal that a
/// label edit opened. (Focus is not usable as the signal: a proxy widget in a
/// view-less scene does not take keyboard focus offscreen.)
bool hasLabelEditor(const QGraphicsScene &scene)
{
    const auto items = scene.items();
    for (auto *item : items) {
        if (auto *proxy = dynamic_cast<QGraphicsProxyWidget *>(item)) {
            if (qobject_cast<QLineEdit *>(proxy->widget()))
                return true;
        }
    }
    return false;
}

QStringList menuTexts(const QMenu &menu)
{
    QStringList out;
    const auto actions = menu.actions();
    for (const auto *a : actions)
        out << a->text();
    return out;
}

} // namespace

class TestCanvasReadOnly : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        // The context-menu test builds real menus, so i18n() runs; without a
        // domain every entry spams a KLocalizedString warning.
        KLocalizedString::setApplicationDomain("corbomite");
        qRegisterMetaType<Graffodil::IGraphNode *>("IGraphNode*");
        qRegisterMetaType<Graffodil::IGraphEdge *>("IGraphEdge*");
    }

    // ------------------------------------------------------------------
    // The umbrella gate named in the plan: every mutation path at once.
    // ------------------------------------------------------------------
    void testReadOnlyBlocksMutations()
    {
        Canvas::CanvasDocument doc;
        doc.addNode(textNode(QStringLiteral("t1"), 0, 0, QStringLiteral("hello")));
        doc.addNode(groupNode(QStringLiteral("g1"), 400, 0));
        doc.setModified(false);

        SceneHarness scene;
        scene.setDocument(&doc);
        scene.setFilePickerRequestor([]() { return QStringLiteral("picked.md"); });

        auto *card = scene.textCardItem(QStringLiteral("t1"));
        QVERIFY(card);

        const int nodesBefore = doc.nodes().size();
        const int edgesBefore = doc.edges().size();

        scene.setReadOnly(true);
        QVERIFY(scene.isReadOnly());
        QCOMPARE(scene.undoStack()->count(), 0);

        // 1. double-click empty canvas -> new text card (Obsidian :1368)
        QGraphicsSceneMouseEvent dbl(QEvent::GraphicsSceneMouseDoubleClick);
        dbl.setScenePos(QPointF(2000, 2000));
        dbl.setButton(Qt::LeftButton);
        dbl.setButtons(Qt::LeftButton);
        QCoreApplication::sendEvent(&scene, &dbl);
        QCOMPARE(doc.nodes().size(), nodesBefore);

        // 2. drop a file (Obsidian leaves this open; gated here)
        QMimeData dropMime;
        dropMime.setUrls({QUrl::fromLocalFile(QStringLiteral("/vault/dropped.md"))});
        QGraphicsSceneDragDropEvent dropEv(QEvent::GraphicsSceneDrop);
        dropEv.setMimeData(&dropMime);
        dropEv.setScenePos(QPointF(100, 400));
        QCoreApplication::sendEvent(&scene, &dropEv);
        QCOMPARE(doc.nodes().size(), nodesBefore);

        // 3. dragEnter/dragMove must decline too, or the cursor still
        // promises a drop.
        QGraphicsSceneDragDropEvent dragEnter(QEvent::GraphicsSceneDragEnter);
        dragEnter.setMimeData(&dropMime);
        dragEnter.setScenePos(QPointF(100, 400));
        QCoreApplication::sendEvent(&scene, &dragEnter);
        QVERIFY(!dragEnter.isAccepted());

        // 4. drop plain text -> text card
        QMimeData textMime;
        textMime.setText(QStringLiteral("dropped"));
        QGraphicsSceneDragDropEvent textDrop(QEvent::GraphicsSceneDrop);
        textDrop.setMimeData(&textMime);
        textDrop.setScenePos(QPointF(100, 400));
        QCoreApplication::sendEvent(&scene, &textDrop);
        QCOMPARE(doc.nodes().size(), nodesBefore);

        // 5. paste plain text (Obsidian :1003)
        scene.pasteCanvasJsonOrText(QStringLiteral("pasted"), QPointF(10, 10));
        QCOMPARE(doc.nodes().size(), nodesBefore);

        // 6. paste canvas JSON
        const QByteArray clip = QJsonDocument(QJsonObject{
            {QStringLiteral("nodes"), QJsonArray{QJsonObject{
                {QStringLiteral("id"), QStringLiteral("x")},
                {QStringLiteral("type"), QStringLiteral("text")},
                {QStringLiteral("x"), 0}, {QStringLiteral("y"), 0},
                {QStringLiteral("width"), 10}, {QStringLiteral("height"), 10}}}},
            {QStringLiteral("edges"), QJsonArray{}},
        }).toJson(QJsonDocument::Compact);
        scene.pasteCanvasJsonOrText(QString::fromUtf8(clip), QPointF(10, 10));
        QCOMPARE(doc.nodes().size(), nodesBefore);

        // 7. cut -> the copy half is allowed, the delete half is not
        card->setSelected(true);
        scene.cutSelectionToClipboard();
        QVERIFY(card->isSelected());
        QCOMPARE(doc.nodes().size(), nodesBefore);

        // 8. Delete / Backspace (Obsidian :715 via :1108)
        pressKey(&scene, Qt::Key_Delete);
        QCOMPARE(doc.nodes().size(), nodesBefore);

        // 9. arrow-key nudge (Corbomite's own move)
        pressKey(&scene, Qt::Key_Right);
        QCOMPARE(doc.node(QStringLiteral("t1")).x, 0);

        // 10. programmatic command entry points
        scene.createFileCardViaPicker(QPointF(0, 0));
        scene.setEdgeEnds(QStringLiteral("no-such-edge"), Canvas::EndType::Arrow,
                          Canvas::EndType::Arrow);
        scene.addCardConnectedTo(textNode(QStringLiteral("n"), 0, 0), nullptr,
                                 QStringLiteral("right"));
        QCOMPARE(doc.nodes().size(), nodesBefore);
        QCOMPARE(doc.edges().size(), edgesBefore);

        // The document is byte-for-byte what it was.
        QCOMPARE(scene.undoStack()->count(), 0);
        QCOMPARE(doc.node(QStringLiteral("t1")).x, 0);
        QCOMPARE(doc.node(QStringLiteral("t1")).text, QStringLiteral("hello"));
        QVERIFY(!doc.isModified());
    }

    // ------------------------------------------------------------------
    // The negative: what must keep working. Pan/zoom/select are the entire
    // point of a read-only canvas, so this is the test that catches an
    // over-broad gate.
    // ------------------------------------------------------------------
    void testReadOnlyAllowsPanZoomAndSelect()
    {
        Canvas::CanvasDocument doc;
        doc.addNode(textNode(QStringLiteral("t1"), 0, 0));
        doc.addNode(textNode(QStringLiteral("t2"), 400, 0));
        doc.addNode(groupNode(QStringLiteral("g1"), 800, 0));
        doc.setModified(false);

        SceneHarness scene;
        Canvas::CanvasView view;
        view.resize(600, 400);
        view.setScene(&scene);
        scene.setDocument(&doc);
        scene.setReadOnly(true);

        // Zoom in / out / reset still move the transform.
        const qreal base = view.transform().m11();
        view.zoomIn();
        QVERIFY(view.transform().m11() > base);
        view.zoomOut();
        QCOMPARE(view.transform().m11(), base);
        view.zoomOut();
        QVERIFY(view.transform().m11() < base);
        view.resetTransform();
        QCOMPARE(view.transform().m11(), base);

        // Pan (middle-button drag, the CompositeTool's middle route into
        // PanZoomTool) still scrolls the viewport.
        view.zoomIn();
        QTest::qWait(1);
        const QPointF centreBefore = view.mapToScene(view.viewport()->rect().center());
        drag(&scene, QPointF(300, 200), QPointF(200, 150), Qt::MiddleButton);
        QVERIFY2(view.mapToScene(view.viewport()->rect().center()) != centreBefore,
                 "middle-button pan did nothing while locked");

        // Click-select a node.
        auto *t1 = scene.textCardItem(QStringLiteral("t1"));
        QVERIFY(t1);
        drag(&scene, QPointF(100, 40), QPointF(100, 40));
        QVERIFY(t1->isSelected());

        // Rubber-band select across all three node kinds.
        scene.clearSelection();
        drag(&scene, QPointF(-500, -500), QPointF(1400, 500));
        QVERIFY(scene.textCardItem(QStringLiteral("t1"))->isSelected());
        QVERIFY(scene.textCardItem(QStringLiteral("t2"))->isSelected());
        QVERIFY(scene.groupItem(QStringLiteral("g1"))->isSelected());

        // Ctrl+A still selects everything.
        scene.clearSelection();
        pressKey(&scene, Qt::Key_A, Qt::ControlModifier);
        QCOMPARE(scene.selectedItems().size(), 3);

        // None of it touched the document.
        QCOMPARE(scene.undoStack()->count(), 0);
        QVERIFY(!doc.isModified());
        QCOMPARE(doc.node(QStringLiteral("t1")).x, 0);
    }

    void testReadOnlyBlocksEdgeCommands()
    {
        Canvas::CanvasDocument doc;
        doc.addNode(textNode(QStringLiteral("a"), 0, 0));
        doc.addNode(textNode(QStringLiteral("b"), 400, 0));
        Canvas::CanvasEdge edge;
        edge.id = QStringLiteral("e1");
        edge.fromNode = QStringLiteral("a");
        edge.toNode = QStringLiteral("b");
        edge.fromEnd = Canvas::EndType::None;
        edge.toEnd = Canvas::EndType::None;
        doc.addEdge(edge);
        doc.setModified(false);

        SceneHarness scene;
        scene.setDocument(&doc);
        QCOMPARE(doc.edge(QStringLiteral("e1")).fromEnd, Canvas::EndType::None);

        scene.setReadOnly(true);

        // The Direction submenu / R-key reverse path.
        scene.reverseEdge(QStringLiteral("e1"));
        QCOMPARE(doc.edge(QStringLiteral("e1")).fromNode, QStringLiteral("a"));
        QCOMPARE(doc.edge(QStringLiteral("e1")).toNode, QStringLiteral("b"));
        scene.setEdgeEnds(QStringLiteral("e1"), Canvas::EndType::Arrow,
                          Canvas::EndType::Arrow);
        QCOMPARE(doc.edge(QStringLiteral("e1")).fromEnd, Canvas::EndType::None);
        QCOMPARE(doc.edge(QStringLiteral("e1")).toEnd, Canvas::EndType::None);

        QCOMPARE(scene.undoStack()->count(), 0);
        QVERIFY(!doc.isModified());

        // Control leg: both work once unlocked.
        scene.setReadOnly(false);
        scene.reverseEdge(QStringLiteral("e1"));
        QCOMPARE(doc.edge(QStringLiteral("e1")).fromNode, QStringLiteral("b"));
        scene.setEdgeEnds(QStringLiteral("e1"), Canvas::EndType::Arrow,
                          Canvas::EndType::Arrow);
        QCOMPARE(doc.edge(QStringLiteral("e1")).fromEnd, Canvas::EndType::Arrow);
    }

    void testReadOnlyKeepsSelectionButBlocksMove()
    {
        Canvas::CanvasDocument doc;
        doc.addNode(textNode(QStringLiteral("t1"), 100, 100));

        SceneHarness scene;
        Canvas::CanvasView view;
        view.resize(600, 400);
        view.setScene(&scene);
        scene.setDocument(&doc);

        auto *card = scene.textCardItem(QStringLiteral("t1"));
        QVERIFY(card);

        // Unlocked, the identical gesture moves the card. Without this leg the
        // test would pass on a scene where the gesture never worked at all.
        drag(&scene, QPointF(200, 140), QPointF(400, 240));
        QCOMPARE(card->pos(), QPointF(300, 200));
        QCOMPARE(scene.undoStack()->count(), 1);
        doc.setModified(false);

        scene.setReadOnly(true);
        card->setSelected(true);
        scene.undoStack()->clear();
        const QPointF posBefore = card->pos();

        drag(&scene, card->sceneBoundingRect().center(),
             card->sceneBoundingRect().center() + QPointF(300, 200));

        // Selection survives the gesture; position does not.
        QVERIFY(card->isSelected());
        QCOMPARE(card->pos(), posBefore);
        QCOMPARE(doc.node(QStringLiteral("t1")).x, 300);
        QCOMPARE(doc.node(QStringLiteral("t1")).y, 200);
        QCOMPARE(scene.undoStack()->count(), 0);
        QVERIFY(!doc.isModified());
    }

    void testReadOnlyBlocksMoveThatStartedBeforeLocking()
    {
        // Locking mid-gesture must abandon the in-flight SelectMoveTool drag
        // rather than let it finish and push CmdMoveCards.
        Canvas::CanvasDocument doc;
        doc.addNode(textNode(QStringLiteral("t1"), 100, 100));
        doc.setModified(false);

        SceneHarness scene;
        scene.setDocument(&doc);
        auto *card = scene.textCardItem(QStringLiteral("t1"));
        QVERIFY(card);

        QGraphicsSceneMouseEvent press(QEvent::GraphicsSceneMousePress);
        press.setScenePos(QPointF(200, 140));
        press.setButton(Qt::LeftButton);
        press.setButtons(Qt::LeftButton);
        QCoreApplication::sendEvent(&scene, &press);

        QGraphicsSceneMouseEvent move(QEvent::GraphicsSceneMouseMove);
        move.setScenePos(QPointF(500, 400));
        move.setLastScenePos(QPointF(200, 140));
        move.setButtons(Qt::LeftButton);
        QCoreApplication::sendEvent(&scene, &move);
        const QPointF draggedTo = card->pos();
        QVERIFY(draggedTo != QPointF(100, 100));   // the drag really happened

        scene.setReadOnly(true);
        QCOMPARE(card->pos(), QPointF(100, 100));

        QGraphicsSceneMouseEvent move2(QEvent::GraphicsSceneMouseMove);
        move2.setScenePos(QPointF(700, 600));
        move2.setLastScenePos(QPointF(500, 400));
        move2.setButtons(Qt::LeftButton);
        QCoreApplication::sendEvent(&scene, &move2);
        QCOMPARE(card->pos(), QPointF(100, 100));

        QGraphicsSceneMouseEvent release(QEvent::GraphicsSceneMouseRelease);
        release.setScenePos(QPointF(700, 600));
        release.setButton(Qt::LeftButton);
        release.setButtons(Qt::NoButton);
        QCoreApplication::sendEvent(&scene, &release);

        QCOMPARE(doc.node(QStringLiteral("t1")).x, 100);
        QCOMPARE(doc.node(QStringLiteral("t1")).y, 100);
        QCOMPARE(scene.undoStack()->count(), 0);
        QVERIFY(!doc.isModified());
    }

    void testReadOnlyBlocksResizeAndAltDragDuplicate()
    {
        // Control leg: both gestures must work while unlocked, or the
        // assertions below would pass on a scene where they never worked.
        {
            Canvas::CanvasDocument unlockedDoc;
            unlockedDoc.addNode(textNode(QStringLiteral("t1"), 0, 0));
            SceneHarness unlockedScene;
            Canvas::CanvasView unlockedView;
            unlockedView.resize(800, 600);
            unlockedView.setScene(&unlockedScene);
            unlockedScene.setDocument(&unlockedDoc);
            auto *unlockedCard = unlockedScene.textCardItem(QStringLiteral("t1"));
            QVERIFY(unlockedCard);
            unlockedCard->setSelected(true);

            const QRectF unlockedRect = unlockedCard->sceneBoundingRect();
            drag(&unlockedScene, unlockedRect.center(),
                 unlockedRect.center() + QPointF(300, 300),
                 Qt::LeftButton, Qt::AltModifier);
            QCOMPARE(unlockedDoc.nodes().size(), 2);
        }

        Canvas::CanvasDocument doc;
        doc.addNode(textNode(QStringLiteral("t1"), 0, 0));
        doc.setModified(false);

        SceneHarness scene;
        Canvas::CanvasView view;
        view.resize(800, 600);
        view.setScene(&scene);
        scene.setDocument(&doc);
        auto *card = scene.textCardItem(QStringLiteral("t1"));
        QVERIFY(card);

        card->setSelected(true);
        QVERIFY(scene.chromeOverlay()->handlesVisible());

        scene.setReadOnly(true);

        // The chrome overlay is the visible affordance for both gestures, so
        // the lock hides it. Obsidian does the same through the
        // `.mod-readonly` class it toggles at app/own/2018-k7.js:1402, which
        // lands in app.css and so is not in the JS corpus.
        QVERIFY(!scene.chromeOverlay()->handlesVisible());
        QVERIFY(!scene.chromeOverlay()->dotsVisible());

        // Press-and-drag a corner handle: no resize.
        const QRectF rectBefore = card->sceneBoundingRect();
        drag(&scene, QPointF(rectBefore.right(), rectBefore.bottom()),
             QPointF(rectBefore.right() + 200, rectBefore.bottom() + 120));
        QCOMPARE(card->sceneBoundingRect().size(), rectBefore.size());
        QCOMPARE(doc.node(QStringLiteral("t1")).width, 200);
        QCOMPARE(doc.node(QStringLiteral("t1")).height, 80);

        // Alt-drag duplicate: no new node.
        drag(&scene, rectBefore.center(), rectBefore.center() + QPointF(300, 300),
             Qt::LeftButton, Qt::AltModifier);
        QCOMPARE(doc.nodes().size(), 1);
        QCOMPARE(scene.undoStack()->count(), 0);

        // Unlock restores the affordance and the gestures work again.
        scene.clearSelection();
        card->setSelected(true);
        scene.setReadOnly(false);
        QVERIFY(scene.chromeOverlay()->handlesVisible());
        drag(&scene, rectBefore.center(), rectBefore.center() + QPointF(300, 300),
             Qt::LeftButton, Qt::AltModifier);
        QCOMPARE(doc.nodes().size(), 2);
    }

    void testReadOnlyBlocksEdgeCreation()
    {
        Canvas::CanvasDocument doc;
        doc.addNode(textNode(QStringLiteral("a"), 0, 0));
        doc.addNode(textNode(QStringLiteral("b"), 600, 0));
        doc.setModified(false);

        SceneHarness scene;
        Canvas::CanvasView view;
        view.resize(1000, 600);
        view.setScene(&scene);
        scene.setDocument(&doc);

        auto *itemA = scene.textCardItem(QStringLiteral("a"));
        auto *itemB = scene.textCardItem(QStringLiteral("b"));
        QVERIFY(itemA && itemB);

        // Control leg: unlocked, dragging anchor-to-node creates an edge.
        // (Dropping on EMPTY space instead would open
        // onEdgeDroppedOnEmpty()'s modal create-and-connect menu, which a
        // test cannot dismiss.)
        const QPointF anchor = itemA->anchors().first().scenePos;
        drag(&scene, anchor, itemB->sceneBoundingRect().center());
        QCOMPARE(doc.edges().size(), 1);
        doc.setModified(false);
        scene.undoStack()->clear();

        scene.setReadOnly(true);

        drag(&scene, anchor, itemB->sceneBoundingRect().center());
        QCOMPARE(doc.edges().size(), 1);
        QCOMPARE(scene.undoStack()->count(), 0);
        QVERIFY(!doc.isModified());
    }

    void testReadOnlyBlocksInlineEdit()
    {
        // Control leg: a double-click on each of the three card kinds opens
        // its editor while unlocked, so the locked-leg assertions below are
        // about the lock and not about a dead trigger.
        {
            Canvas::CanvasDocument controlDoc;
            controlDoc.addNode(textNode(QStringLiteral("t"), 0, 0, QStringLiteral("b")));
            SceneHarness control;
            control.setDocument(&controlDoc);
            doubleClick(&control, QPointF(100, 40));
            QVERIFY(control.isEditing());
        }
        {
            Canvas::CanvasDocument controlDoc;
            controlDoc.addNode(groupNode(QStringLiteral("g"), 0, 0));
            SceneHarness control;
            control.setDocument(&controlDoc);
            QVERIFY(!hasLabelEditor(control));
            doubleClick(&control, QPointF(200, 150));
            QVERIFY(hasLabelEditor(control));

            // ...and locking closes it again (the proxy is deleteLater'd).
            control.setReadOnly(true);
            QTest::qWait(1);
            QVERIFY(!hasLabelEditor(control));
        }

        Canvas::CanvasDocument doc;
        doc.addNode(textNode(QStringLiteral("t1"), 0, 0, QStringLiteral("body")));
        doc.addNode(groupNode(QStringLiteral("g1"), 400, 0));
        Canvas::CanvasNode file;
        file.id = QStringLiteral("f1");
        file.type = Canvas::NodeType::File;
        file.file = QStringLiteral("note.md");
        file.x = 900;
        file.y = 0;
        file.width = 200;
        file.height = 150;
        doc.addNode(file);
        doc.setModified(false);

        SceneHarness scene;
        scene.setDocument(&doc);

        QString saved;
        scene.setFileSaver([&saved](const QString &, const QString &content) {
            saved = content;
        });

        scene.setReadOnly(true);

        doubleClick(&scene, QPointF(100, 40));      // text card
        QVERIFY(!scene.isEditing());
        doubleClick(&scene, QPointF(500, 40));      // group label
        QVERIFY(!hasLabelEditor(scene));
        doubleClick(&scene, QPointF(1000, 75));     // file card
        QVERIFY(!scene.isEditing());
        QVERIFY(saved.isEmpty());

        QCOMPARE(doc.node(QStringLiteral("t1")).text, QStringLiteral("body"));
        QCOMPARE(scene.undoStack()->count(), 0);
        QVERIFY(!doc.isModified());
    }

    void testLockingMidInlineEditClosesTheEditor()
    {
        Canvas::CanvasDocument doc;
        doc.addNode(textNode(QStringLiteral("t1"), 0, 0, QStringLiteral("body")));

        SceneHarness scene;
        scene.setDocument(&doc);
        doubleClick(&scene, QPointF(100, 40));
        QVERIFY(scene.isEditing());

        scene.setReadOnly(true);
        QVERIFY(!scene.isEditing());
        // Nothing was typed, so the commit is a no-op rather than a spurious
        // undo entry.
        QCOMPARE(scene.undoStack()->count(), 0);
        QCOMPARE(doc.node(QStringLiteral("t1")).text, QStringLiteral("body"));
    }

    void testReadOnlyBlocksContextMenuMutations()
    {
        Canvas::CanvasDocument doc;
        doc.addNode(textNode(QStringLiteral("t1"), 0, 0));
        doc.addNode(groupNode(QStringLiteral("g1"), 400, 0));

        SceneHarness scene;
        Canvas::CanvasView view;
        view.resize(1000, 600);
        view.setScene(&scene);
        scene.setDocument(&doc);

        // Populated when unlocked.
        {
            QMenu unlocked;
            scene.populateContextMenu(&unlocked, QPointF(100, 40));
            const auto texts = menuTexts(unlocked);
            QVERIFY(texts.contains(QStringLiteral("Delete")));
            QVERIFY(texts.contains(QStringLiteral("Duplicate")));
            QVERIFY(texts.contains(QStringLiteral("Edit")));
        }
        {
            QMenu onEmpty;
            scene.populateContextMenu(&onEmpty, QPointF(3000, 3000));
            QVERIFY(menuTexts(onEmpty).contains(QStringLiteral("New group")));
        }

        scene.setReadOnly(true);

        QMenu onCard;
        scene.populateContextMenu(&onCard, QPointF(100, 40));
        QVERIFY(onCard.actions().isEmpty());

        QMenu onGroup;
        scene.populateContextMenu(&onGroup, QPointF(500, 40));
        QVERIFY(onGroup.actions().isEmpty());

        QMenu onEmpty;
        scene.populateContextMenu(&onEmpty, QPointF(3000, 3000));
        QVERIFY(onEmpty.actions().isEmpty());

        QCOMPARE(doc.nodes().size(), 2);
        QCOMPARE(scene.undoStack()->count(), 0);
    }

    void testReadOnlyBlocksUndoRedo()
    {
        Canvas::CanvasDocument doc;
        doc.addNode(textNode(QStringLiteral("t1"), 0, 0));

        SceneHarness scene;
        Canvas::CanvasView view;
        view.resize(600, 400);
        view.setScene(&scene);
        scene.setDocument(&doc);

        scene.undoStack()->push(new Canvas::CmdMoveCards(
            &doc, QHash<QString, QPointF>{{QStringLiteral("t1"), QPointF(0, 0)}},
            QHash<QString, QPointF>{{QStringLiteral("t1"), QPointF(500, 500)}}));
        QCOMPARE(doc.node(QStringLiteral("t1")).x, 500);
        QVERIFY(scene.canUndo());

        scene.setReadOnly(true);
        QVERIFY(!scene.canUndo());
        QVERIFY(!scene.canRedo());

        // CanvasView's Ctrl+Z / Ctrl+Y route through the same predicates.
        QTest::keyClick(&view, Qt::Key_Z, Qt::ControlModifier);
        QCOMPARE(doc.node(QStringLiteral("t1")).x, 500);
        QTest::keyClick(&view, Qt::Key_Y, Qt::ControlModifier);
        QCOMPARE(doc.node(QStringLiteral("t1")).x, 500);

        scene.setReadOnly(false);
        QVERIFY(scene.canUndo());
        scene.undoStack()->undo();
        QCOMPARE(doc.node(QStringLiteral("t1")).x, 0);
    }

    void testReadOnlyToggleEmitsOnceAndOnlyOnChange()
    {
        SceneHarness scene;
        QSignalSpy spy(&scene, &Canvas::CanvasScene::readOnlyChanged);

        scene.setReadOnly(true);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toBool(), true);
        QVERIFY(scene.isReadOnly());

        scene.setReadOnly(true);
        QCOMPARE(spy.count(), 1);

        scene.setReadOnly(false);
        QCOMPARE(spy.count(), 2);
        QCOMPARE(spy.at(1).at(0).toBool(), false);
        QVERIFY(!scene.isReadOnly());
    }
};

QTEST_MAIN(TestCanvasReadOnly)
#include "tst_canvas_readonly.moc"