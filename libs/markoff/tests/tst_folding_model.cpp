// SPDX-License-Identifier: GPL-3.0-or-later
#include <QtTest/QtTest>
#include <markoff/FoldingTypes.h>
#include <markoff-parser/Document.h>

using namespace Markoff;

class TstFoldingModel : public QObject {
    Q_OBJECT
private slots:
    // --- Path computation ---
    void path_singleHeading_returnsOwnText();
    void path_nestedHeadings_includesAncestors();
    void path_skippedLevels_skipsMissingAncestors();
    void path_boldMarkdownInHeading_isStripped();
    void path_duplicateSiblings_getSuffix();
    void path_duplicateSiblings_firstHasNoSuffix();
};

static HeadingInfo h(int level, QString text, int off = 0) {
    return HeadingInfo{level, std::move(text), off};
}

void TstFoldingModel::path_singleHeading_returnsOwnText() {
    QList<HeadingInfo> headings = { h(1, "Intro") };
    auto paths = computeHeadingPaths(headings);
    QCOMPARE(paths.size(), 1);
    QCOMPARE(paths[0], QStringList{ "Intro" });
}

void TstFoldingModel::path_nestedHeadings_includesAncestors() {
    QList<HeadingInfo> headings = {
        h(1, "Intro"),
        h(2, "Goals"),
        h(3, "Non-goals"),
    };
    auto paths = computeHeadingPaths(headings);
    QCOMPARE(paths[0], (QStringList{ "Intro" }));
    QCOMPARE(paths[1], (QStringList{ "Intro", "Goals" }));
    QCOMPARE(paths[2], (QStringList{ "Intro", "Goals", "Non-goals" }));
}

void TstFoldingModel::path_skippedLevels_skipsMissingAncestors() {
    // # A \n ### C — no H2 between. Path is ["A", "C"] (skipped level).
    QList<HeadingInfo> headings = { h(1, "A"), h(3, "C") };
    auto paths = computeHeadingPaths(headings);
    QCOMPARE(paths[1], (QStringList{ "A", "C" }));
}

void TstFoldingModel::path_boldMarkdownInHeading_isStripped() {
    QList<HeadingInfo> headings = { h(2, "**Goals**") };
    auto paths = computeHeadingPaths(headings);
    QCOMPARE(paths[0], QStringList{ "Goals" });
}

void TstFoldingModel::path_duplicateSiblings_getSuffix() {
    QList<HeadingInfo> headings = {
        h(1, "A"),
        h(2, "Goals"),
        h(2, "Goals"),
        h(2, "Goals"),
    };
    auto paths = computeHeadingPaths(headings);
    QCOMPARE(paths[1], (QStringList{ "A", "Goals" }));
    QCOMPARE(paths[2], (QStringList{ "A", "Goals#2" }));
    QCOMPARE(paths[3], (QStringList{ "A", "Goals#3" }));
}

void TstFoldingModel::path_duplicateSiblings_firstHasNoSuffix() {
    // Re-asserts the no-suffix-on-first rule in isolation.
    QList<HeadingInfo> headings = { h(1, "Same"), h(1, "Same") };
    auto paths = computeHeadingPaths(headings);
    QCOMPARE(paths[0], QStringList{ "Same" });
    QCOMPARE(paths[1], QStringList{ "Same#2" });
}

QTEST_MAIN(TstFoldingModel)
#include "tst_folding_model.moc"
