// SPDX-License-Identifier: GPL-3.0-or-later
#include <QtTest/QtTest>
#include <QApplication>
#include <QSignalSpy>
#include <markoff/Editor.h>

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

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    TstCodeBlockFolding t;
    return QTest::qExec(&t, argc, argv);
}
#include "tst_code_block_folding.moc"
