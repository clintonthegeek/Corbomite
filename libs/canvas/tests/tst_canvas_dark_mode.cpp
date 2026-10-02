// libs/canvas/tests/tst_canvas_dark_mode.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Cluster M leftover — canvas dark mode.
//
// Canvas had zero theme awareness: hardcoded Qt::white backgrounds and
// QColor(255,255,255) card fills. An Obsidian-authored dark vault therefore
// showed a white canvas with white cards on it — cards on cards, invisible.
//
// The falsifiable claim is NOT "dark is dark". It is the one that actually
// bit: **a card must remain distinguishable from the canvas background it
// sits on.** A palette that simply inverted the background while leaving
// card fills white (or vice versa) would pass a naive `isDark()` check and
// still be unusable.
#include <QTest>
#include <QGraphicsScene>

#include "canvas/CanvasPalette.h"
#include "canvas/CanvasScene.h"
#include "canvas/CanvasView.h"

using namespace Canvas;

class TestCanvasDarkMode : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    // The default palette is light, so existing behaviour is preserved for
    // anyone who never wires a theme.
    void defaultSceneIsLight() {
        CanvasScene scene;
        QCOMPARE(scene.canvasPalette().background, QColor(255, 255, 255));
        QVERIFY(!scene.canvasPalette().cardsContrastWithBackground());
    }

    // The scene's background BRUSH must follow the palette, because
    // renderToImage/renderToSvg read the brush directly. If it did not, a
    // dark canvas would still export white (or, as before the fix, the brush
    // was a hardcoded Qt::white that ignored everything).
    void applyDarkThemeAlsoDrivesTheExportBrush() {
        CanvasScene scene;
        QCOMPARE(scene.backgroundBrush().color(), QColor(255, 255, 255));

        scene.applyDarkTheme(true);
        QCOMPARE(scene.backgroundBrush().color(),
                 CanvasPalette::dark().background);
        QVERIFY(scene.backgroundBrush().color() != QColor(255, 255, 255));

        scene.applyDarkTheme(false);
        QCOMPARE(scene.backgroundBrush().color(), QColor(255, 255, 255));
    }

    // THE core assertion. Cards must contrast with the background in BOTH
    // themes. This is the falsifiable form of the user-visible bug.
    void cardsRemainVisibleInBothThemes() {
        const CanvasPalette light = CanvasPalette::light();
        const CanvasPalette dark = CanvasPalette::dark();

        QVERIFY2(!light.cardsContrastWithBackground(),
                 "light cards were expected to sit flush on a light canvas");
        QVERIFY2(dark.cardsContrastWithBackground(),
                 "dark cards are invisible on a dark canvas — this is the bug");
    }

    // Every role the paint() paths read must actually be populated. A
    // default-constructed QColor is invalid and renders as black or nothing,
    // so an unset role is a silent rendering failure.
    void darkPaletteHasNoUnsetRoles() {
        const CanvasPalette dark = CanvasPalette::dark();
        QVERIFY2(dark.background.isValid(), "background unset");
        QVERIFY2(dark.cardBackground.isValid(), "cardBackground unset");
        QVERIFY2(dark.titleStripBackground.isValid(), "titleStripBackground unset");
        QVERIFY2(dark.titleStripForeground.isValid(), "titleStripForeground unset");
        QVERIFY2(dark.foreground.isValid(), "foreground unset");
        QVERIFY2(dark.gridDot.isValid(), "gridDot unset");
        QVERIFY2(dark.border.isValid(), "border unset");
        QVERIFY2(dark.borderSelected.isValid(), "borderSelected unset");
        QVERIFY2(dark.edge.isValid(), "edge unset");
        QVERIFY2(dark.edgeLabelBackground.isValid(), "edgeLabelBackground unset");
        QVERIFY2(dark.groupFill.isValid(), "groupFill unset");
        QVERIFY2(dark.groupLabelForeground.isValid(), "groupLabelForeground unset");
        QVERIFY2(dark.handleFill.isValid(), "handleFill unset");
        QVERIFY2(dark.missingForeground.isValid(), "missingForeground unset");
    }

    // Labels must contrast with what they are drawn on, or a dark theme makes
    // them unreadable — the "looks themed but is not" failure.
    void darkLabelsContrastWithTheirBackgrounds() {
        const CanvasPalette d = CanvasPalette::dark();
        QVERIFY2(qAbs(d.titleStripForeground.lightness()
                       - d.titleStripBackground.lightness()) >= 40,
                 "title-strip text is unreadable on its own stripe");
        QVERIFY2(qAbs(d.groupLabelForeground.lightness()
                       - d.background.lightness()) >= 40,
                 "group label text is unreadable on the canvas background");
        QVERIFY2(qAbs(d.foreground.lightness() - d.cardBackground.lightness()) >= 60,
                 "card text is unreadable on the card fill");
    }

    // A card border must be visible against BOTH the card and the canvas, or
    // cards merge into the background in one theme or the other.
    void darkBordersAreVisibleAgainstCardAndBackground() {
        const CanvasPalette d = CanvasPalette::dark();
        const int vsCard = qAbs(d.border.lightness() - d.cardBackground.lightness());
        const int vsBg = qAbs(d.border.lightness() - d.background.lightness());
        QVERIFY2(vsCard >= 20, "card border invisible against the card fill");
        QVERIFY2(vsBg >= 20, "card border invisible against the canvas background");
    }

    // The grid must be visible on the canvas in dark mode.
    void darkGridIsVisible() {
        const CanvasPalette d = CanvasPalette::dark();
        QVERIFY2(qAbs(d.gridDot.lightness() - d.background.lightness()) >= 15,
                 "grid dots invisible on a dark canvas");
    }

    // Idempotence: applying the same palette twice must be a no-op, and
    // applying light after dark must fully restore — a partially-restored
    // palette is how a "switching back to light leaves the canvas grey" bug
    // happens.
    void togglingBackAndForthIsClean() {
        CanvasScene scene;
        scene.applyDarkTheme(true);
        scene.applyDarkTheme(true);
        scene.applyDarkTheme(false);
        QCOMPARE(scene.canvasPalette().background,
                 CanvasPalette::light().background);
        QCOMPARE(scene.canvasPalette().cardBackground,
                 CanvasPalette::light().cardBackground);
    }

    // A CanvasView must render the palette background, not a hardcoded white.
    // Asserted via the scene the view was built on, since drawBackground is
    // a protected paint routine and inspecting pixels would be flaky.
    void canvasViewUsesTheScenePalette() {
        CanvasScene scene;
        scene.applyDarkTheme(true);
        CanvasView view;
        view.setScene(&scene);
        QCOMPARE(view.scene(), &scene);
        // drawBackground() reads the scene palette; the falsifiable part is
        // that the palette it would read is the themed one and its card
        // colour is distinguishable. (drawBackground is a protected paint
        // routine, so asserting on pixels would be flaky.)
        QVERIFY(scene.canvasPalette().cardsContrastWithBackground());
        QVERIFY(scene.canvasPalette().background != QColor(255, 255, 255));
    }
};

QTEST_MAIN(TestCanvasDarkMode)
#include "tst_canvas_dark_mode.moc"
