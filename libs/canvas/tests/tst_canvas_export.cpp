// SPDX-License-Identifier: GPL-3.0-or-later
//
// Cluster R Task 3.5 — CanvasScene::renderToImage + renderToSvg sanity checks.
// Verifies that the exported images have the expected dimensions and that the
// SVG output is well-formed XML containing the `<svg>` root.

#include <canvas/CanvasScene.h>

#include <QBuffer>
#include <QGraphicsRectItem>
#include <QImage>
#include <QTest>

using Canvas::CanvasScene;

class TestCanvasExport : public QObject
{
    Q_OBJECT

private slots:
    void renderToImageProducesExpectedSize();
    void renderToImageRespectsScale();
    void renderToSvgProducesValidXml();
};

void TestCanvasExport::renderToImageProducesExpectedSize()
{
    CanvasScene scene;
    scene.addRect(QRectF(0, 0, 100, 100));

    const QRectF bounds(0, 0, 200, 200);
    QImage img = scene.renderToImage(bounds, /*transparentBg=*/false,
                                       /*showEdges=*/true, /*scale=*/1.0);
    QCOMPARE(img.width(), 200);
    QCOMPARE(img.height(), 200);
}

void TestCanvasExport::renderToImageRespectsScale()
{
    CanvasScene scene;
    scene.addRect(QRectF(0, 0, 100, 100));

    QImage img = scene.renderToImage(QRectF(0, 0, 150, 100),
                                       /*transparentBg=*/true,
                                       /*showEdges=*/true, /*scale=*/2.0);
    QCOMPARE(img.width(), 300);
    QCOMPARE(img.height(), 200);
    // transparentBg → ARGB32 format
    QCOMPARE(img.format(), QImage::Format_ARGB32);
}

void TestCanvasExport::renderToSvgProducesValidXml()
{
    CanvasScene scene;
    scene.addRect(QRectF(0, 0, 100, 100));

    QBuffer buf;
    buf.open(QIODevice::WriteOnly);
    scene.renderToSvg(QRectF(0, 0, 200, 200), &buf,
                      /*transparentBg=*/false, /*showEdges=*/true);
    buf.close();

    const QByteArray out = buf.data();
    QVERIFY2(out.startsWith("<?xml"),
             qPrintable(QStringLiteral("got: %1").arg(QString::fromUtf8(out.left(64)))));
    QVERIFY(out.contains("<svg"));
    QVERIFY(out.contains("</svg>"));
}

QTEST_MAIN(TestCanvasExport)
#include "tst_canvas_export.moc"
