// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QGraphicsScene>
#include "SelectionManager.h"
#include "MarkdownTextItem.h"
#include "StubBlockItem.h"

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

QTEST_MAIN(TestSelection)
#include "tst_selection.moc"
