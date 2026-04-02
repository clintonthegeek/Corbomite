// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QGraphicsScene>
#include <QMimeData>
#include <QTextCursor>
#include "SelectionManager.h"
#include "MarkdownTextItem.h"
#include "StubBlockItem.h"
#include "TextControl.h"

using namespace Markoff;

class TestSelection : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testInitialState();
    void testPressTransitionsToWithinItem();
    void testMoveWithinItemStaysWithinItem();
    void testMoveCrossBoundaryTransitions();
    void testReleaseFromCrossBoundaryResetsToNone();
    void testReleaseFromWithinItemResetsToNone();
    void testApplySelectionForward();
    void testApplySelectionBackward();
    void testApplySelectionMiddleItemFullySelected();
    void testApplySelectionClearsOutOfRange();
    void testApplySelectionDragReversal();
    void testSerializeMarkdownForward();
    void testSerializeMarkdownWithBlockItem();
    void testCtrlCCreatesCorrectMimeData();
    void testCtrlAClearsAndSelectsAll();
    void testEscapeClearsSelection();
    void testClearSelectionResetsAll();
};

void TestSelection::testInitialState()
{
    SelectionManager mgr;
    QCOMPARE(mgr.mode(), SelectionMode::None);
    QVERIFY(!mgr.hasSelection());
}

void TestSelection::testPressTransitionsToWithinItem()
{
    QGraphicsScene scene;
    auto *text1 = new MarkdownTextItem;
    text1->setPlainText(QStringLiteral("Hello world"));
    scene.addItem(text1);
    text1->setPos(0, 0);

    SelectionManager mgr;
    mgr.setItems({text1});

    // Press inside the text item
    QPointF pressPos(10, 5);
    mgr.handleMousePress(pressPos, Qt::NoModifier);
    QCOMPARE(mgr.mode(), SelectionMode::WithinItem);
}

void TestSelection::testMoveWithinItemStaysWithinItem()
{
    QGraphicsScene scene;
    auto *text1 = new MarkdownTextItem;
    text1->setPlainText(QStringLiteral("Hello world"));
    scene.addItem(text1);
    text1->setPos(0, 0);

    SelectionManager mgr;
    mgr.setItems({text1});

    mgr.handleMousePress(QPointF(10, 5), Qt::NoModifier);
    // Move within the same item's bounding rect
    bool consumed = mgr.handleMouseMove(QPointF(50, 5));
    QVERIFY(!consumed); // not consumed, Qt handles within-item
    QCOMPARE(mgr.mode(), SelectionMode::WithinItem);
}

void TestSelection::testMoveCrossBoundaryTransitions()
{
    QGraphicsScene scene;
    auto *text1 = new MarkdownTextItem;
    text1->setPlainText(QStringLiteral("First region"));
    scene.addItem(text1);
    text1->setPos(0, 0);

    auto *block = new StubBlockItem(QStringLiteral("| A |\n|---|\n| 1 |"), 600, 60);
    scene.addItem(block);
    block->setPos(0, 30); // below text1

    auto *text2 = new MarkdownTextItem;
    text2->setPlainText(QStringLiteral("Second region"));
    scene.addItem(text2);
    text2->setPos(0, 100);

    SelectionManager mgr;
    mgr.setItems({text1, block, text2});

    // Press in text1
    mgr.handleMousePress(QPointF(10, 5), Qt::NoModifier);
    QCOMPARE(mgr.mode(), SelectionMode::WithinItem);

    // Move into the block item (below text1's bounding rect)
    bool consumed = mgr.handleMouseMove(QPointF(10, 50));
    QVERIFY(consumed);
    QCOMPARE(mgr.mode(), SelectionMode::CrossBoundary);
}

void TestSelection::testReleaseFromCrossBoundaryResetsToNone()
{
    QGraphicsScene scene;
    auto *text1 = new MarkdownTextItem;
    text1->setPlainText(QStringLiteral("Hello"));
    scene.addItem(text1);
    text1->setPos(0, 0);

    auto *text2 = new MarkdownTextItem;
    text2->setPlainText(QStringLiteral("World"));
    scene.addItem(text2);
    text2->setPos(0, 30);

    SelectionManager mgr;
    mgr.setItems({text1, text2});

    mgr.handleMousePress(QPointF(10, 5), Qt::NoModifier);
    mgr.handleMouseMove(QPointF(10, 35)); // cross into text2
    QCOMPARE(mgr.mode(), SelectionMode::CrossBoundary);

    bool consumed = mgr.handleMouseRelease(QPointF(10, 35));
    QVERIFY(consumed);
    QCOMPARE(mgr.mode(), SelectionMode::None);
}

void TestSelection::testReleaseFromWithinItemResetsToNone()
{
    QGraphicsScene scene;
    auto *text1 = new MarkdownTextItem;
    text1->setPlainText(QStringLiteral("Hello"));
    scene.addItem(text1);
    text1->setPos(0, 0);

    SelectionManager mgr;
    mgr.setItems({text1});

    mgr.handleMousePress(QPointF(10, 5), Qt::NoModifier);
    bool consumed = mgr.handleMouseRelease(QPointF(50, 5));
    QVERIFY(!consumed); // Qt handles within-item release
    QCOMPARE(mgr.mode(), SelectionMode::None);
}

void TestSelection::testApplySelectionForward()
{
    QGraphicsScene scene;
    auto *text1 = new MarkdownTextItem;
    text1->setPlainText(QStringLiteral("AAABBB"));
    scene.addItem(text1);
    text1->setPos(0, 0);

    auto *text2 = new MarkdownTextItem;
    text2->setPlainText(QStringLiteral("CCCDDD"));
    scene.addItem(text2);
    text2->setPos(0, 30);

    SelectionManager mgr;
    mgr.setItems({text1, text2});

    mgr.handleMousePress(QPointF(10, 5), Qt::NoModifier);
    mgr.handleMouseMove(QPointF(10, 35));

    QTextCursor c1 = text1->textControl()->textCursor();
    QVERIFY(c1.hasSelection());

    QTextCursor c2 = text2->textControl()->textCursor();
    QVERIFY(c2.hasSelection());
    QCOMPARE(c2.anchor(), 0);
}

void TestSelection::testApplySelectionBackward()
{
    QGraphicsScene scene;
    auto *text1 = new MarkdownTextItem;
    text1->setPlainText(QStringLiteral("AAABBB"));
    scene.addItem(text1);
    text1->setPos(0, 0);

    auto *text2 = new MarkdownTextItem;
    text2->setPlainText(QStringLiteral("CCCDDD"));
    scene.addItem(text2);
    text2->setPos(0, 30);

    SelectionManager mgr;
    mgr.setItems({text1, text2});

    mgr.handleMousePress(QPointF(10, 35), Qt::NoModifier);
    mgr.handleMouseMove(QPointF(10, 5));
    QCOMPARE(mgr.mode(), SelectionMode::CrossBoundary);

    QTextCursor c2 = text2->textControl()->textCursor();
    QVERIFY(c2.hasSelection());

    QTextCursor c1 = text1->textControl()->textCursor();
    QVERIFY(c1.hasSelection());
}

void TestSelection::testApplySelectionMiddleItemFullySelected()
{
    QGraphicsScene scene;
    auto *text1 = new MarkdownTextItem;
    text1->setPlainText(QStringLiteral("First"));
    scene.addItem(text1);
    text1->setPos(0, 0);

    auto *block = new StubBlockItem(QStringLiteral("| A |\n|---|\n| 1 |"), 600, 60);
    scene.addItem(block);
    block->setPos(0, 30);

    auto *text2 = new MarkdownTextItem;
    text2->setPlainText(QStringLiteral("Second"));
    scene.addItem(text2);
    text2->setPos(0, 100);

    SelectionManager mgr;
    mgr.setItems({text1, block, text2});

    mgr.handleMousePress(QPointF(10, 5), Qt::NoModifier);
    mgr.handleMouseMove(QPointF(10, 105));
    QCOMPARE(mgr.mode(), SelectionMode::CrossBoundary);
    QVERIFY(block->isFullySelected());
}

void TestSelection::testApplySelectionClearsOutOfRange()
{
    QGraphicsScene scene;
    auto *text1 = new MarkdownTextItem;
    text1->setPlainText(QStringLiteral("First"));
    scene.addItem(text1);
    text1->setPos(0, 0);

    auto *block = new StubBlockItem(QStringLiteral("| A |"), 600, 60);
    scene.addItem(block);
    block->setPos(0, 30);

    auto *text2 = new MarkdownTextItem;
    text2->setPlainText(QStringLiteral("Second"));
    scene.addItem(text2);
    text2->setPos(0, 100);

    SelectionManager mgr;
    mgr.setItems({text1, block, text2});

    mgr.handleMousePress(QPointF(10, 5), Qt::NoModifier);
    mgr.handleMouseMove(QPointF(10, 105));

    // Now drag back to block (text2 should be cleared)
    mgr.handleMouseMove(QPointF(10, 50));
    QTextCursor c2 = text2->textControl()->textCursor();
    QVERIFY(!c2.hasSelection());
}

void TestSelection::testApplySelectionDragReversal()
{
    QGraphicsScene scene;
    auto *text1 = new MarkdownTextItem;
    text1->setPlainText(QStringLiteral("Above"));
    scene.addItem(text1);
    text1->setPos(0, 0);

    auto *text2 = new MarkdownTextItem;
    text2->setPlainText(QStringLiteral("Middle"));
    scene.addItem(text2);
    text2->setPos(0, 30);

    auto *text3 = new MarkdownTextItem;
    text3->setPlainText(QStringLiteral("Below"));
    scene.addItem(text3);
    text3->setPos(0, 60);

    SelectionManager mgr;
    mgr.setItems({text1, text2, text3});

    mgr.handleMousePress(QPointF(10, 35), Qt::NoModifier);
    mgr.handleMouseMove(QPointF(10, 65));
    QCOMPARE(mgr.mode(), SelectionMode::CrossBoundary);

    // Reverse: drag up to text1
    mgr.handleMouseMove(QPointF(10, 5));
    QTextCursor c3 = text3->textControl()->textCursor();
    QVERIFY(!c3.hasSelection());
    QTextCursor c1 = text1->textControl()->textCursor();
    QVERIFY(c1.hasSelection());
}

void TestSelection::testSerializeMarkdownForward()
{
    QGraphicsScene scene;
    auto *text1 = new MarkdownTextItem;
    text1->setPlainText(QStringLiteral("Hello World"));
    scene.addItem(text1);
    text1->setPos(0, 0);

    auto *text2 = new MarkdownTextItem;
    text2->setPlainText(QStringLiteral("Goodbye Moon"));
    scene.addItem(text2);
    text2->setPos(0, 30);

    SelectionManager mgr;
    mgr.setItems({text1, text2});

    mgr.handleMousePress(QPointF(0, 5), Qt::NoModifier);
    mgr.handleMouseMove(QPointF(200, 35));

    std::unique_ptr<QMimeData> data(mgr.createMimeData());
    QString md = data->text();
    QVERIFY(md.contains(QStringLiteral("Hello")));
    QVERIFY(md.contains(QStringLiteral("Goodbye")));
}

void TestSelection::testSerializeMarkdownWithBlockItem()
{
    QGraphicsScene scene;
    auto *text1 = new MarkdownTextItem;
    text1->setPlainText(QStringLiteral("Before table"));
    scene.addItem(text1);
    text1->setPos(0, 0);

    QString tableMd = QStringLiteral("| A | B |\n|---|---|\n| 1 | 2 |");
    auto *block = new StubBlockItem(tableMd, 600, 60);
    scene.addItem(block);
    block->setPos(0, 30);

    auto *text2 = new MarkdownTextItem;
    text2->setPlainText(QStringLiteral("After table"));
    scene.addItem(text2);
    text2->setPos(0, 100);

    SelectionManager mgr;
    mgr.setItems({text1, block, text2});

    mgr.handleMousePress(QPointF(0, 5), Qt::NoModifier);
    mgr.handleMouseMove(QPointF(600, 115));

    std::unique_ptr<QMimeData> data(mgr.createMimeData());
    QString md = data->text();
    QVERIFY(md.contains(QStringLiteral("Before table")));
    QVERIFY(md.contains(QStringLiteral("| A | B |")));
    QVERIFY(md.contains(QStringLiteral("After table")));
}

void TestSelection::testCtrlCCreatesCorrectMimeData()
{
    QGraphicsScene scene;
    auto *text1 = new MarkdownTextItem;
    text1->setPlainText(QStringLiteral("Copy me"));
    scene.addItem(text1);
    text1->setPos(0, 0);

    auto *text2 = new MarkdownTextItem;
    text2->setPlainText(QStringLiteral("And me"));
    scene.addItem(text2);
    text2->setPos(0, 30);

    SelectionManager mgr;
    mgr.setItems({text1, text2});

    mgr.handleMousePress(QPointF(0, 5), Qt::NoModifier);
    mgr.handleMouseMove(QPointF(10, 35));
    QCOMPARE(mgr.mode(), SelectionMode::CrossBoundary);

    QKeyEvent copyEvent(QEvent::KeyPress, Qt::Key_C, Qt::ControlModifier);
    bool consumed = mgr.handleKeyPress(&copyEvent);
    QVERIFY(consumed);
}

void TestSelection::testCtrlAClearsAndSelectsAll()
{
    QGraphicsScene scene;
    auto *text1 = new MarkdownTextItem;
    text1->setPlainText(QStringLiteral("First"));
    scene.addItem(text1);
    text1->setPos(0, 0);

    auto *block = new StubBlockItem(QStringLiteral("| T |"), 600, 60);
    scene.addItem(block);
    block->setPos(0, 30);

    auto *text2 = new MarkdownTextItem;
    text2->setPlainText(QStringLiteral("Second"));
    scene.addItem(text2);
    text2->setPos(0, 100);

    SelectionManager mgr;
    mgr.setItems({text1, block, text2});

    QKeyEvent selectAllEvent(QEvent::KeyPress, Qt::Key_A, Qt::ControlModifier);
    bool consumed = mgr.handleKeyPress(&selectAllEvent);
    QVERIFY(consumed);
    QCOMPARE(mgr.mode(), SelectionMode::CrossBoundary);
    QVERIFY(block->isFullySelected());

    QTextCursor c1 = text1->textControl()->textCursor();
    QVERIFY(c1.hasSelection());
    QCOMPARE(c1.selectedText(), QStringLiteral("First"));

    QTextCursor c2 = text2->textControl()->textCursor();
    QVERIFY(c2.hasSelection());
    QCOMPARE(c2.selectedText(), QStringLiteral("Second"));
}

void TestSelection::testEscapeClearsSelection()
{
    QGraphicsScene scene;
    auto *text1 = new MarkdownTextItem;
    text1->setPlainText(QStringLiteral("Hello"));
    scene.addItem(text1);
    text1->setPos(0, 0);

    auto *text2 = new MarkdownTextItem;
    text2->setPlainText(QStringLiteral("World"));
    scene.addItem(text2);
    text2->setPos(0, 30);

    SelectionManager mgr;
    mgr.setItems({text1, text2});

    mgr.handleMousePress(QPointF(10, 5), Qt::NoModifier);
    mgr.handleMouseMove(QPointF(10, 35));
    QCOMPARE(mgr.mode(), SelectionMode::CrossBoundary);

    QKeyEvent escEvent(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
    bool consumed = mgr.handleKeyPress(&escEvent);
    QVERIFY(consumed);
    QCOMPARE(mgr.mode(), SelectionMode::None);
    QVERIFY(!mgr.hasSelection());
}

void TestSelection::testClearSelectionResetsAll()
{
    QGraphicsScene scene;
    auto *text1 = new MarkdownTextItem;
    text1->setPlainText(QStringLiteral("Hello"));
    scene.addItem(text1);
    text1->setPos(0, 0);

    auto *block = new StubBlockItem(QStringLiteral("| A |"), 600, 60);
    scene.addItem(block);
    block->setPos(0, 30);

    SelectionManager mgr;
    mgr.setItems({text1, block});

    mgr.handleMousePress(QPointF(10, 5), Qt::NoModifier);
    mgr.handleMouseMove(QPointF(10, 50));
    QVERIFY(block->isFullySelected());

    mgr.clearSelection();
    QVERIFY(!block->isFullySelected());
    QTextCursor c1 = text1->textControl()->textCursor();
    QVERIFY(!c1.hasSelection());
    QCOMPARE(mgr.mode(), SelectionMode::None);
}

QTEST_MAIN(TestSelection)
#include "tst_selection.moc"
