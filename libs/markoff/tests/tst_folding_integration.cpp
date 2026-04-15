// SPDX-License-Identifier: GPL-3.0-or-later
#include <QtTest/QtTest>
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

QTEST_MAIN(TstFoldingIntegration)
#include "tst_folding_integration.moc"
