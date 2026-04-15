// SPDX-License-Identifier: GPL-3.0-or-later
#include <QtTest/QtTest>
#include <QApplication>
#include <markoff/Editor.h>

using namespace Markoff;

class TstFoldingIntegration : public QObject {
    Q_OBJECT
private slots:
    void editor_setPlainText_populatesHeadingPaths();
    void editor_foldAndUnfold_emitsSignal();
    void editor_serializeAndRestore_roundTrip();
    void editor_renameHeading_dropsStaleFold();
};

static QString kSample =
    "# Intro\n\nBody\n\n## Goals\n\nMore body\n\n"
    "## Non-goals\n\nText\n\n# Other\n\nEnd\n";

static void waitForReparse() {
    // Coordinator uses a debounce timer for reparse. Spin the event loop
    // a short while. Actual timer period is ~50ms; 500ms for slow machines.
    QTest::qWait(500);
}

void TstFoldingIntegration::editor_setPlainText_populatesHeadingPaths() {
    Editor e;
    e.setPlainText(kSample);
    waitForReparse();
    const auto paths = e.headingPaths();
    QVERIFY(paths.contains((QStringList{"Intro"})));
    QVERIFY(paths.contains((QStringList{"Intro","Goals"})));
    QVERIFY(paths.contains((QStringList{"Other"})));
}

void TstFoldingIntegration::editor_foldAndUnfold_emitsSignal() {
    Editor e;
    e.setPlainText(kSample);
    waitForReparse();
    QSignalSpy spy(&e, &Editor::foldStateChanged);
    e.fold({"Intro","Goals"});
    QCOMPARE(spy.count(), 1);
    QVERIFY(e.isFolded({"Intro","Goals"}));
    e.unfold({"Intro","Goals"});
    QCOMPARE(spy.count(), 2);
}

void TstFoldingIntegration::editor_serializeAndRestore_roundTrip() {
    Editor e;
    e.setPlainText(kSample);
    waitForReparse();
    e.fold({"Intro","Goals"});
    e.fold({"Other"});

    auto j = e.serializeFoldState();

    Editor e2;
    e2.setPlainText(kSample);
    waitForReparse();
    e2.restoreFoldState(j);

    QVERIFY(e2.isFolded({"Intro","Goals"}));
    QVERIFY(e2.isFolded({"Other"}));
}

void TstFoldingIntegration::editor_renameHeading_dropsStaleFold() {
    Editor e;
    e.setPlainText(kSample);
    waitForReparse();
    e.fold({"Intro","Goals"});

    QString renamed = kSample;
    renamed.replace("## Goals", "## Objectives");
    e.setPlainText(renamed);
    waitForReparse();

    QVERIFY(!e.isFolded({"Intro","Goals"}));
}

// ---------------------------------------------------------------------------
// Visibility tests — Task 7
// ---------------------------------------------------------------------------
// NOTE: MarkdownSplitter only splits on tables/images — plain heading+para
// text becomes a single MarkdownTextItem with multiple QTextBlocks. Folding
// therefore hides blocks within the item (via zero line-height), not whole
// items. We measure "visible height" of each item's document as the proxy.
// ---------------------------------------------------------------------------

#include "SceneCoordinator.h"
#include "SelectableItem.h"
#include "MarkdownTextItem.h"
#include <QGraphicsItem>

/// Sum of document heights for all MarkdownTextItem instances in the scene.
/// Non-text items (tables/images) contribute their full bounding rect height.
static qreal totalVisibleHeight(const QList<SelectableItem *> &items) {
    qreal h = 0;
    for (auto *it : items) {
        if (!it->asGraphicsItem()->isVisible()) continue;
        h += it->asGraphicsItem()->boundingRect().height();
    }
    return h;
}

class TstFoldingVisibility : public QObject {
    Q_OBJECT
private slots:
    void foldH1_hidesChildrenButKeepsHeading();
    void unfold_reshowsChildren();
    void nestedFold_independent();
};

void TstFoldingVisibility::foldH1_hidesChildrenButKeepsHeading() {
    Editor e;
    e.setPlainText(kSample);
    waitForReparse();

    auto *coord = e.coordinatorForTesting();
    const qreal total = totalVisibleHeight(coord->items());

    e.fold({"Intro"});
    QTest::qWait(50);

    const qreal afterFold = totalVisibleHeight(coord->items());
    QVERIFY2(afterFold < total, "folding should reduce total visible height");
}

void TstFoldingVisibility::unfold_reshowsChildren() {
    Editor e;
    e.setPlainText(kSample);
    waitForReparse();
    auto *coord = e.coordinatorForTesting();
    const qreal total = totalVisibleHeight(coord->items());

    e.fold({"Intro"});
    QTest::qWait(50);
    e.unfold({"Intro"});
    QTest::qWait(50);

    QCOMPARE(totalVisibleHeight(coord->items()), total);
}

void TstFoldingVisibility::nestedFold_independent() {
    Editor e;
    e.setPlainText(kSample);
    waitForReparse();
    auto *coord = e.coordinatorForTesting();

    e.fold({"Intro","Goals"});
    e.fold({"Intro"});
    QTest::qWait(50);
    const qreal bothFolded = totalVisibleHeight(coord->items());

    e.unfold({"Intro"});
    QTest::qWait(50);
    // Goals is still folded — blocks under Goals still hidden.
    const qreal onlyGoalsFolded = totalVisibleHeight(coord->items());
    QVERIFY(onlyGoalsFolded > bothFolded); // Intro body re-shown
    QVERIFY(e.isFolded({"Intro","Goals"})); // unchanged
}

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    int status = 0;
    { TstFoldingIntegration t;  status |= QTest::qExec(&t, argc, argv); }
    { TstFoldingVisibility  t;  status |= QTest::qExec(&t, argc, argv); }
    return status;
}
#include "tst_folding_integration.moc"
