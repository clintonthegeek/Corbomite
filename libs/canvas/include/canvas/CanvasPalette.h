// libs/canvas/include/canvas/CanvasPalette.h
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QColor>

namespace Canvas {

/// Cluster M leftover (canvas dark mode) — the theme-derived colours every
/// canvas paint() reads.
///
/// Canvas had **zero** theme awareness: ~15 hardcoded literals across
/// CanvasView, CanvasScene, TextCardItem, FileCardItem, GroupItem and
/// EdgeItem. `CanvasScene`'s own comment admitted it ("Full theme/dark-mode
/// awareness for canvas is not wired anywhere yet — punch-listed"), so an
/// Obsidian-authored dark vault showed a white canvas with white cards on it
/// — cards on cards, invisible.
///
/// One struct with named roles, rather than `QPalette`, because these are
/// not widget roles: there is no Window/Base/Highlight mapping that puts
/// "canvas grid dot" anywhere sensible, and export needs the *same* values
/// the viewport uses (renderToImage/renderToSvg read the scene's background
/// brush, which is one of these fields).
///
/// Deliberately NOT a dark-mode site: `colorFromCanvasColor()`'s six Obsidian
/// presets (CanvasTypes.h). Those are the user's chosen node accents; making
/// them theme-derived would change stored data's appearance.
struct CanvasPalette
{
    /// Viewport fill and export background. Kept in sync with the scene's
    /// background brush so export matches what the user sees.
    QColor background = QColor(255, 255, 255);
    /// Text-card and file-card fills.
    QColor cardBackground = QColor(255, 255, 255);
    /// File-card title stripe, default (no accent colour) case.
    QColor titleStripBackground = QColor(240, 240, 240);
    QColor titleStripForeground = QColor(40, 40, 40);
    /// Text painted on cards and group labels.
    QColor foreground = QColor(0, 0, 0);
    /// Drawn dot grid.
    QColor gridDot = QColor(200, 200, 200);
    /// Card and group borders, unselected.
    QColor border = QColor(200, 200, 200);
    /// Selected card/group border. Already dark-safe in both themes (it is
    /// Obsidian's selection blue), so one value serves both.
    QColor borderSelected = QColor(58, 134, 255);
    /// Edge strokes and edge labels.
    QColor edge = QColor(80, 80, 80);
    QColor edgeLabelBackground = QColor(255, 255, 255, 220);
    /// Group fill (translucent) and label text.
    QColor groupFill = QColor(200, 200, 200, 25);
    QColor groupLabelForeground = QColor(80, 80, 80);
    /// Selection handles and connection dots on nodes.
    QColor handleFill = QColor(58, 134, 255);
    /// "File not found" placeholder text on an unresolved file card.
    QColor missingForeground = QColor(160, 160, 160);

    static CanvasPalette light() { return {}; }

    /// Dark values chosen to hold contrast against each other: cards are
    /// lifted off the background (not merely "not white") so a card on a
    /// card stays visible, and borders/labels are checked against their own
    /// backgrounds rather than copied from the light set.
    static CanvasPalette dark()
    {
        CanvasPalette p;
        p.background = QColor(30, 30, 30);
        p.cardBackground = QColor(48, 48, 48);
        p.titleStripBackground = QColor(64, 64, 64);
        p.titleStripForeground = QColor(235, 235, 235);
        p.foreground = QColor(230, 230, 230);
        p.gridDot = QColor(70, 70, 70);
        p.border = QColor(90, 90, 90);
        p.borderSelected = QColor(90, 150, 255);
        p.edge = QColor(150, 150, 150);
        p.edgeLabelBackground = QColor(48, 48, 48, 220);
        p.groupFill = QColor(120, 120, 120, 30);
        p.groupLabelForeground = QColor(190, 190, 190);
        p.handleFill = QColor(90, 150, 255);
        p.missingForeground = QColor(140, 140, 140);
        return p;
    }

    /// True when this palette's card fill differs enough from the
    /// background to be distinguishable. Used by tests as the falsifiable
    /// form of "cards are visible on the canvas".
    bool cardsContrastWithBackground() const
    {
        const int d = qAbs(background.lightness() - cardBackground.lightness());
        return d >= 8;
    }
};

} // namespace Canvas