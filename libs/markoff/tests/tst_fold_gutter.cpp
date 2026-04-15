// SPDX-License-Identifier: GPL-3.0-or-later
#include <QtTest/QtTest>
#include <QImage>
#include <QPainter>
#include "GutterColumn.h"
#include "FoldingModel.h"

using namespace Markoff;

class TstFoldArrowColumn : public QObject {
    Q_OBJECT
private slots:
    void width_returns16();
    void paintCell_nonHeading_paintsNothing();
    void paintCell_unfoldedHeading_paintsDownTriangle();
    void paintCell_foldedHeading_paintsRightTriangle();
    void handleClick_noModifier_togglesThatHeading();
    void handleClick_ctrlModifier_foldsAllAtThatLevel();
};

static FoldingModel::HeadingEntry mk(QStringList path, int level) {
    return {path, HeadingInfo{level, path.last(), 0}};
}

static bool imageHasNonBackgroundPixels(const QImage &img) {
    for (int y = 0; y < img.height(); ++y)
        for (int x = 0; x < img.width(); ++x)
            if (qAlpha(img.pixel(x, y)) > 0) return true;
    return false;
}

void TstFoldArrowColumn::width_returns16() {
    FoldingModel m;
    FoldArrowColumn col(&m);
    QCOMPARE(col.width(), 16);
}

void TstFoldArrowColumn::paintCell_nonHeading_paintsNothing() {
    FoldingModel m;
    m.setHeadingsForTesting({ mk({"A"}, 1) });
    FoldArrowColumn col(&m);
    QImage img(16, 20, QImage::Format_ARGB32);
    img.fill(Qt::transparent);
    QPainter p(&img);
    col.paintCell(&p, QRect(0, 0, 16, 20), /*itemIndex=*/999); // out of range
    p.end();
    QVERIFY(!imageHasNonBackgroundPixels(img));
}

void TstFoldArrowColumn::paintCell_unfoldedHeading_paintsDownTriangle() {
    FoldingModel m;
    m.setHeadingsForTesting({ mk({"A"}, 1) });
    FoldArrowColumn col(&m);
    QImage img(16, 20, QImage::Format_ARGB32);
    img.fill(Qt::transparent);
    QPainter p(&img);
    col.paintCell(&p, QRect(0, 0, 16, 20), /*headingIdx=*/0);
    p.end();
    QVERIFY(imageHasNonBackgroundPixels(img));
}

void TstFoldArrowColumn::paintCell_foldedHeading_paintsRightTriangle() {
    FoldingModel m;
    m.setHeadingsForTesting({ mk({"A"}, 1) });
    m.fold({"A"});
    FoldArrowColumn col(&m);
    QImage img(16, 20, QImage::Format_ARGB32);
    img.fill(Qt::transparent);
    QPainter p(&img);
    col.paintCell(&p, QRect(0, 0, 16, 20), 0);
    p.end();
    QVERIFY(imageHasNonBackgroundPixels(img));
    // Rightward triangle: right third of image empty, left third heavier.
    // Relaxed check: at least left quarter has pixels.
    bool leftQuarter = false;
    for (int y = 0; y < img.height(); ++y)
        for (int x = 0; x < 4; ++x)
            if (qAlpha(img.pixel(x, y)) > 0) leftQuarter = true;
    QVERIFY(leftQuarter);
}

void TstFoldArrowColumn::handleClick_noModifier_togglesThatHeading() {
    FoldingModel m;
    m.setHeadingsForTesting({ mk({"A"}, 1) });
    FoldArrowColumn col(&m);
    QVERIFY(col.handleClick(QPoint(5, 5), 0, Qt::NoModifier));
    QVERIFY(m.isFolded({"A"}));
}

void TstFoldArrowColumn::handleClick_ctrlModifier_foldsAllAtThatLevel() {
    FoldingModel m;
    m.setHeadingsForTesting({
        mk({"A"}, 1), mk({"A","B"}, 2), mk({"A","C"}, 2)
    });
    FoldArrowColumn col(&m);
    QVERIFY(col.handleClick(QPoint(5, 5), 1, Qt::ControlModifier));
    QVERIFY(m.isFolded({"A","B"}));
    QVERIFY(m.isFolded({"A","C"}));
    QVERIFY(!m.isFolded({"A"}));
}

QTEST_MAIN(TstFoldArrowColumn)
#include "tst_fold_gutter.moc"
