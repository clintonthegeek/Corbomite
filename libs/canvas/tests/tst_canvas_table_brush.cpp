// SPDX-License-Identifier: GPL-3.0-or-later
//
// The canvas leaf's painter-state leak, seen from the host.
//
// Upstream Markoff v0.7.1 fixed a leak in the canvas leaf: `QPainter::drawRect`,
// `drawRoundedRect` and `drawText` FILL with the current brush, and the fold
// affordance set the brush to the block's own foreground without clearing it —
// so the brush leaked into the next block's shapes. The visible damage was a
// table after a heading rendering as an unreadable dark slab, every cell
// filled with the heading's foreground colour.
//
// Corbomite hosts that widget as the note editor's live preview
// (`NoteEditorWidget`'s `Markoff::Canvas::EditorWidget`, in its default — NOT
// embedded — mode, which is the mode that draws the fold affordance). So the
// leak is user-visible here: a note with a heading followed by a table.
//
// Upstream's own test for this lives in the markoff-canvas leaf, and this
// build does not compile Markoff's test suites (`MARKOFF_BUILD_TESTS` is
// forced OFF), so nothing in *our* suite would notice a regression. This test
// pins the behaviour at the host surface.
//
// The canvas CARD editor is a different surface and is not affected: it runs
// in embedded mode, which draws no fold affordance, so no brush is ever set.
#include <QApplication>
#include <QImage>
#include <QTest>

#include <markoff/canvas/EditorWidget.h>
#include <markoff/core/MarkoffDocument.h>
#include <markoff/core/Theme.h>
#include <markoff/vnext/DocumentStorageProvider.h>

#include <memory>

class TestCanvasTableBrush : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void tableAfterHeadingDoesNotInheritTheHeadingBrush();
    void theCardEditorIsNotAffectedBecauseItDrawsNoFoldAffordance();
};

namespace {

struct Fixture {
    std::unique_ptr<Markoff::VNext::DocumentStorageProvider> provider;
    std::unique_ptr<Markoff::MarkoffDocument> document;
};

/// A document holding a heading followed by a table.
Fixture openHeadingThenTable()
{
    auto provider = Markoff::VNext::makeEphemeralDocumentStorageProvider();
    if (!provider.ok())
        qFatal("provider creation failed");
    Fixture fixture{std::move(provider).value(), {}};
    auto opened = Markoff::MarkoffDocument::open(*fixture.provider,
        {QStringLiteral("table-brush"), 1},
        Markoff::VNext::StorageMode::ephemeral);
    if (!opened.ok())
        qFatal("document open failed");
    fixture.document = std::move(opened).value();
    const auto res = fixture.document->importInitialMarkdown(
        QByteArrayLiteral("# Heading\n\n| A | B |\n|---|---|\n| 1 | 2 |\n"));
    if (res.state != Markoff::VNext::ActionPublicationState::committed
        && res.state != Markoff::VNext::ActionPublicationState::committedHistoryPending)
        qFatal("initial import failed");
    return fixture;
}

/// The theme's heading colour is what the leaked brush carries, so make it
/// unmistakable: any pixel of it below the heading is a leak.
const QColor kHeadingColour(255, 0, 0);

Markoff::Theme themeWithRedHeading()
{
    Markoff::Theme theme = Markoff::Theme::defaultLight();
    theme.setColor(Markoff::Theme::Slot::EditorBackground, Qt::white);
    theme.setColor(Markoff::Theme::Slot::TextDefault, QColor(20, 20, 20));
    theme.setColor(Markoff::Theme::Slot::Heading1, kHeadingColour);
    return theme;
}

/// The number of pixels painted in the heading's own colour below the
/// heading's band — that is, in the table.
int leakedPixels(const QImage &painted)
{
    int leaked = 0;
    const int tableTop = int(painted.height() * 0.25);
    for (int y = tableTop; y < painted.height(); ++y) {
        for (int x = 0; x < painted.width(); ++x) {
            if (painted.pixelColor(x, y) == kHeadingColour)
                ++leaked;
        }
    }
    return leaked;
}

} // namespace

void TestCanvasTableBrush::tableAfterHeadingDoesNotInheritTheHeadingBrush()
{
    auto fixture = openHeadingThenTable();

    Markoff::Canvas::EditorWidget editor;
    QVERIFY(!editor.embeddedMode());   // the live preview's mode
    editor.setTheme(themeWithRedHeading());
    editor.setDocument(fixture.document.get());
    editor.resize(400, 300);
    editor.show();
    QCoreApplication::processEvents();

    const auto blocks = fixture.document->iterateBlocks();
    QVERIFY2(blocks.size() >= 2, "the document has fewer than two blocks");

    const QImage painted = editor.grab().toImage();
    const int leaked = leakedPixels(painted);

    QVERIFY2(leaked == 0,
             qPrintable(QStringLiteral(
                 "%1 pixels in the table were filled with the heading's "
                 "chevron brush — the table rendered as a slab").arg(leaked)));
}

void TestCanvasTableBrush::theCardEditorIsNotAffectedBecauseItDrawsNoFoldAffordance()
{
    // The card editor runs embedded, so the fold affordance that sets the
    // brush is never drawn. This case says so out loud: if a future change
    // made the card editor draw fold affordances, THIS is the surface that
    // would start leaking, and the case above would not cover it.
    auto fixture = openHeadingThenTable();

    Markoff::Canvas::EditorWidget editor;
    editor.setEmbeddedMode(true);
    QVERIFY(editor.embeddedMode());
    editor.setTheme(themeWithRedHeading());
    editor.setDocument(fixture.document.get());
    editor.resize(400, 300);
    editor.show();
    QCoreApplication::processEvents();

    const QImage painted = editor.grab().toImage();
    const int leaked = leakedPixels(painted);

    QVERIFY2(leaked == 0,
             qPrintable(QStringLiteral(
                 "%1 pixels in the card's table were filled with the heading's "
                 "chevron brush").arg(leaked)));
}

QTEST_MAIN(TestCanvasTableBrush)
#include "tst_canvas_table_brush.moc"
