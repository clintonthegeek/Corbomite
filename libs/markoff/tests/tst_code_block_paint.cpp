// SPDX-License-Identifier: GPL-3.0-or-later
#include <QtTest/QtTest>
#include <QApplication>
#include <QImage>
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include "TableBlockItem.h"

using namespace Markoff;

class TstCodeBlockPaint : public QObject {
    Q_OBJECT
private slots:
    void setFolded_true_reducesBoundingRect();
    void setFolded_false_restoresBoundingRect();
    void paint_folded_rendersAnyPixels();
    void paint_foldedSingular_formatsOneLine();
};

static TableBlockItem *makeItem(const QString &fence) {
    auto *it = new TableBlockItem(fence, 400.0);
    return it;
}

void TstCodeBlockPaint::setFolded_true_reducesBoundingRect()
{
    auto *it = makeItem(QStringLiteral("```cpp\nint a = 1;\nint b = 2;\n```"));
    const qreal unfoldedHeight = it->boundingRect().height();
    it->setFolded(true, QStringLiteral("cpp"), 2);
    QVERIFY(it->boundingRect().height() < unfoldedHeight);
    delete it;
}

void TstCodeBlockPaint::setFolded_false_restoresBoundingRect()
{
    auto *it = makeItem(QStringLiteral("```cpp\nint a = 1;\nint b = 2;\n```"));
    const qreal unfoldedHeight = it->boundingRect().height();
    it->setFolded(true, QStringLiteral("cpp"), 2);
    it->setFolded(false);
    QCOMPARE(it->boundingRect().height(), unfoldedHeight);
    delete it;
}

void TstCodeBlockPaint::paint_folded_rendersAnyPixels()
{
    auto *it = makeItem(QStringLiteral("```cpp\nint a = 1;\nint b = 2;\n```"));
    it->setFolded(true, QStringLiteral("cpp"), 2);
    QImage img(int(it->boundingRect().width()),
               int(it->boundingRect().height()),
               QImage::Format_ARGB32);
    img.fill(Qt::transparent);
    QPainter p(&img);
    QStyleOptionGraphicsItem opt;
    it->paint(&p, &opt, nullptr);
    p.end();
    bool hasAny = false;
    for (int y = 0; y < img.height() && !hasAny; ++y)
        for (int x = 0; x < img.width(); ++x)
            if (qAlpha(img.pixel(x, y)) > 0) { hasAny = true; break; }
    QVERIFY(hasAny);
    delete it;
}

void TstCodeBlockPaint::paint_foldedSingular_formatsOneLine()
{
    // Not a paint test per se — verifies the summary-string formatting
    // helper renders "(1 line)" not "(1 lines)".
    auto *it = makeItem(QStringLiteral("```cpp\nint a;\n```"));
    it->setFolded(true, QStringLiteral("cpp"), 1);
    // Expose a summaryForTesting() method on TableBlockItem that returns
    // the summary string (see impl below).
    QCOMPARE(it->summaryForTesting(),
             QStringLiteral("```cpp (1 line)"));
    delete it;
}

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    TstCodeBlockPaint t;
    return QTest::qExec(&t, argc, argv);
}
#include "tst_code_block_paint.moc"
