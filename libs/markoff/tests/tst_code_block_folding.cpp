// SPDX-License-Identifier: GPL-3.0-or-later
#include <QtTest/QtTest>
#include <QApplication>
#include <QSignalSpy>
#include <QGraphicsItem>
#include <markoff/Editor.h>
#include "SceneCoordinator.h"
#include "SelectableItem.h"

using namespace Markoff;

class TstCodeBlockFolding : public QObject {
    Q_OBJECT
private slots:
    void codeBlockPaths_singleBlock_oneEntry();
    void codeBlockPaths_ordinalsWithinSection();
    void fold_codeBlock_emitsSignal();
    void foldAllCodeBlocks_foldsEveryCode_keepsHeadings();
    void persistence_mixedPaths_roundTrip();
    void rename_enclosingHeading_dropsCodeBlockFold();
    void auto_unfold_findText_insideFoldedCode();
};

// ---------------------------------------------------------------------------
// Click-based regression tests for the three visual-testing bugs
// ---------------------------------------------------------------------------

class TstCodeBlockClickFolding : public QObject {
    Q_OBJECT
private slots:
    // Bug 1: plain left-click on code-block arrow must fold THAT code block,
    // not the enclosing heading and not nothing.
    void click_codeBlockArrow_togglesThatSpecificCodeBlock();

    // Bug 2: after Ctrl+Click folds all siblings, plain click on one folded
    // code block must unfold exactly that block.
    void click_afterCtrlFoldSiblings_unfoldsClickedBlock();

    // Bug 3: after folding ALL H2 headings then expanding one, clicking the
    // heading arrow again must re-collapse it.
    void click_headingArrow_afterSiblingsFolded_togglesThisHeadingOnly();
};

static const QString kDoc =
    "# Intro\n\n"
    "Body.\n\n"
    "```python\n"
    "print('hello')\n"
    "```\n\n"
    "```cpp\n"
    "puts(\"hi\");\n"
    "```\n\n"
    "## Other\n\n"
    "```rust\n"
    "fn main() {}\n"
    "```\n";

static void waitReparse() { QTest::qWait(300); }

void TstCodeBlockFolding::codeBlockPaths_singleBlock_oneEntry()
{
    Editor e;
    e.setPlainText(QStringLiteral("```py\nx=1\n```\n"));
    waitReparse();
    const auto paths = e.codeBlockPaths();
    QCOMPARE(paths.size(), 1);
    QCOMPARE(paths[0], (QStringList{"code:0"}));
}

void TstCodeBlockFolding::codeBlockPaths_ordinalsWithinSection()
{
    Editor e;
    e.setPlainText(kDoc);
    waitReparse();
    const auto paths = e.codeBlockPaths();
    QVERIFY(paths.contains((QStringList{"Intro","code:0"})));
    QVERIFY(paths.contains((QStringList{"Intro","code:1"})));
    QVERIFY(paths.contains((QStringList{"Intro","Other","code:0"})));
}

void TstCodeBlockFolding::fold_codeBlock_emitsSignal()
{
    Editor e;
    e.setPlainText(kDoc);
    waitReparse();
    QSignalSpy spy(&e, &Editor::foldStateChanged);
    e.fold({"Intro","code:0"});
    QCOMPARE(spy.count(), 1);
    QVERIFY(e.isFolded({"Intro","code:0"}));
}

void TstCodeBlockFolding::foldAllCodeBlocks_foldsEveryCode_keepsHeadings()
{
    Editor e;
    e.setPlainText(kDoc);
    waitReparse();
    e.foldAllCodeBlocks();
    for (const auto &p : e.codeBlockPaths())
        QVERIFY(e.isFolded(p));
    for (const auto &hp : e.headingPaths())
        QVERIFY(!e.isFolded(hp));
}

void TstCodeBlockFolding::persistence_mixedPaths_roundTrip()
{
    Editor e;
    e.setPlainText(kDoc);
    waitReparse();
    e.fold({"Intro","code:0"});
    e.fold({"Other"});
    const auto j = e.serializeFoldState();

    Editor e2;
    e2.setPlainText(kDoc);
    waitReparse();
    e2.restoreFoldState(j);
    QVERIFY(e2.isFolded({"Intro","code:0"}));
    QVERIFY(e2.isFolded({"Other"}));
}

void TstCodeBlockFolding::rename_enclosingHeading_dropsCodeBlockFold()
{
    Editor e;
    e.setPlainText(kDoc);
    waitReparse();
    e.fold({"Intro","code:0"});

    QString renamed = kDoc;
    renamed.replace(QStringLiteral("# Intro"),
                    QStringLiteral("# Introduction"));
    e.setPlainText(renamed);
    waitReparse();

    QVERIFY(!e.isFolded({"Intro","code:0"}));
}

// Code-block content (BlockItem) is not searched by findText; the
// reachable auto-unfold scenario is: a heading is folded and the match
// lives in an MTI that belongs to the heading's section.
void TstCodeBlockFolding::auto_unfold_findText_insideFoldedCode()
{
    Editor e;
    e.setPlainText(kDoc);
    waitReparse();
    // Fold the "Intro" heading so all its MTI content is hidden.
    e.fold({"Intro"});
    QSignalSpy spy(&e, &Editor::foldsAutoExpanded);

    // "Body." is in the MTI under # Intro — findText must auto-unfold the
    // heading to expose it.
    const bool found = e.findText(QStringLiteral("Body"));
    QTest::qWait(50);

    QVERIFY(found);
    QVERIFY(!e.isFolded({"Intro"}));
    QCOMPARE(spy.count(), 1);
}

// ---------------------------------------------------------------------------
// TstCodeBlockClickFolding implementations
// ---------------------------------------------------------------------------

// Document with a heading and two code blocks under it, then another heading.
// All code blocks live INSIDE a single MarkdownTextItem (item 0) because
// MarkdownSplitter only splits at tables/images, not fenced code blocks.
static const QString kClickDoc =
    "# Heading\n\nBody.\n\n"
    "```cpp\nint x = 1;\n```\n\n"
    "```python\nx = 1\n```\n\n"
    "## Sub\n\nMore.\n";

// Helpers: find the region index for a code block path in the coordinator.
static int regionIdxForPath(SceneCoordinator *coord,
                             const QStringList &path)
{
    const auto regions = coord->computeRegions();
    for (int i = 0; i < regions.size(); ++i)
        if (regions[i].path == path) return i;
    return -1;
}

// Returns the total visible height of all MTI items (sum of document heights
// for visible items; zero for invisible items). Used to detect that blocks
// were actually hidden.
static qreal visibleMtiHeight(const QList<SelectableItem *> &items)
{
    qreal h = 0;
    for (auto *it : items) {
        if (!it->isTextItem()) continue;
        auto *gi = it->asGraphicsItem();
        if (!gi->isVisible()) continue;
        h += gi->boundingRect().height();
    }
    return h;
}

void TstCodeBlockClickFolding::click_codeBlockArrow_togglesThatSpecificCodeBlock()
{
    // Bug 1: clicking at the scene Y of a code-block arrow should fold THAT
    // specific code block, not the enclosing heading.
    Editor e;
    e.setPlainText(kClickDoc);
    waitReparse();

    auto *coord = e.coordinatorForTesting();

    // Find region index of the first code block.
    const QStringList cbPath = {"Heading", "code:0"};
    const int rIdx = regionIdxForPath(coord, cbPath);
    QVERIFY2(rIdx >= 0, "code block region not found in coordinator");

    // Get the scene Y where the gutter will paint the code block's arrow.
    const qreal sceneY = coord->regionSceneY(rIdx);
    QVERIFY2(sceneY >= 0, "regionSceneY returned negative for code block");

    // Verify: the hit test at that Y resolves to THIS region (not a heading).
    const int hitIdx = coord->regionIndexAtSceneY(sceneY);
    QVERIFY2(hitIdx == rIdx,
             qPrintable(QStringLiteral("regionIndexAtSceneY(%1) returned %2, expected %3")
                        .arg(sceneY).arg(hitIdx).arg(rIdx)));

    // Measure visible height before click.
    const qreal heightBefore = visibleMtiHeight(coord->items());

    // Simulate a click. Only THIS code block should get folded.
    const bool handled = e.gutterClickAtSceneY(sceneY);
    QVERIFY(handled);
    QTest::qWait(50);

    // Fold state must be recorded.
    QVERIFY2(e.isFolded(cbPath),
             "plain click on code-block arrow must fold that code block");
    QVERIFY2(!e.isFolded({"Heading"}),
             "plain click on code-block arrow must NOT fold the enclosing heading");
    QVERIFY2(!e.isFolded({"Heading", "code:1"}),
             "plain click on code-block arrow must NOT fold sibling code block");

    // Visual state: the MTI must shrink (content blocks hidden — Bug 1 fix).
    const qreal heightAfter = visibleMtiHeight(coord->items());
    QVERIFY2(heightAfter < heightBefore,
             qPrintable(QStringLiteral(
                 "folding code block must reduce visible height "
                 "(before=%1, after=%2) — Bug 1").arg(heightBefore).arg(heightAfter)));
}

void TstCodeBlockClickFolding::click_afterCtrlFoldSiblings_unfoldsClickedBlock()
{
    // Bug 2: after Ctrl+Click folds all siblings, clicking one should unfold it.
    Editor e;
    e.setPlainText(kClickDoc);
    waitReparse();

    auto *coord = e.coordinatorForTesting();

    // Ctrl+Click on code:0 to fold all siblings in "Heading" section.
    const QStringList cb0Path = {"Heading", "code:0"};
    const int rIdx0 = regionIdxForPath(coord, cb0Path);
    QVERIFY(rIdx0 >= 0);
    const qreal sceneY0 = coord->regionSceneY(rIdx0);
    QVERIFY(sceneY0 >= 0);

    const bool ctrlHandled = e.gutterClickAtSceneY(sceneY0, Qt::ControlModifier);
    QVERIFY(ctrlHandled);
    QTest::qWait(50);

    // Both code blocks under "Heading" should now be folded.
    QVERIFY(e.isFolded({"Heading", "code:0"}));
    QVERIFY(e.isFolded({"Heading", "code:1"}));

    // Now plain-click code:0 to unfold it. After folding, repositionItems
    // runs and sceneY may change. Re-query.
    const qreal sceneY0After = coord->regionSceneY(rIdx0);
    QVERIFY(sceneY0After >= 0);

    // The hit test at the new Y must return the SAME code block (not some
    // other item that moved to overlap with it).
    const int hitAfter = coord->regionIndexAtSceneY(sceneY0After);
    QVERIFY2(hitAfter == rIdx0,
             qPrintable(QStringLiteral("after sibling fold: regionIndexAtSceneY(%1)=%2 expected %3")
                        .arg(sceneY0After).arg(hitAfter).arg(rIdx0)));

    const bool plainHandled = e.gutterClickAtSceneY(sceneY0After);
    QVERIFY(plainHandled);
    QTest::qWait(50);

    QVERIFY2(!e.isFolded({"Heading", "code:0"}),
             "plain click must unfold code:0 after sibling fold");
    QVERIFY2(e.isFolded({"Heading", "code:1"}),
             "code:1 must remain folded (only code:0 was clicked)");
}

void TstCodeBlockClickFolding::click_headingArrow_afterSiblingsFolded_togglesThisHeadingOnly()
{
    // Bug 3: expand a heading then re-collapse it. The re-collapse click
    // at the heading's arrow must correctly resolve to the heading region.
    Editor e;
    e.setPlainText(kClickDoc);
    waitReparse();

    auto *coord = e.coordinatorForTesting();

    // Find the "Heading" region.
    const QStringList hPath = {"Heading"};
    const int hRIdx = regionIdxForPath(coord, hPath);
    QVERIFY(hRIdx >= 0);

    // Step 1: fold the heading via the API.
    e.fold(hPath);
    QTest::qWait(50);
    QVERIFY(e.isFolded(hPath));

    // Step 2: click the heading arrow to expand it.
    const qreal sceneYAfterFold = coord->regionSceneY(hRIdx);
    QVERIFY(sceneYAfterFold >= 0);

    const int hitAfterFold = coord->regionIndexAtSceneY(sceneYAfterFold);
    QVERIFY2(hitAfterFold == hRIdx,
             qPrintable(QStringLiteral("after heading fold: hit=%1 expected=%2 sceneY=%3")
                        .arg(hitAfterFold).arg(hRIdx).arg(sceneYAfterFold)));

    e.gutterClickAtSceneY(sceneYAfterFold);
    QTest::qWait(50);
    QVERIFY2(!e.isFolded(hPath), "click must expand the heading");

    // Step 3: click the heading arrow again to re-collapse it.
    // After expanding, items are repositioned again. Re-query Y.
    const qreal sceneYAfterExpand = coord->regionSceneY(hRIdx);
    QVERIFY(sceneYAfterExpand >= 0);

    const int hitAfterExpand = coord->regionIndexAtSceneY(sceneYAfterExpand);
    QVERIFY2(hitAfterExpand == hRIdx,
             qPrintable(QStringLiteral("after heading expand: hit=%1 expected=%2 sceneY=%3")
                        .arg(hitAfterExpand).arg(hRIdx).arg(sceneYAfterExpand)));

    e.gutterClickAtSceneY(sceneYAfterExpand);
    QTest::qWait(50);
    QVERIFY2(e.isFolded(hPath),
             "second click on heading arrow must re-collapse the heading (Bug 3)");
}

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    int status = 0;
    { TstCodeBlockFolding t;       status |= QTest::qExec(&t, argc, argv); }
    { TstCodeBlockClickFolding t;  status |= QTest::qExec(&t, argc, argv); }
    return status;
}
#include "tst_code_block_folding.moc"
