// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QGraphicsView>
#include <QJsonObject>
#include <QPoint>

class QTimer;

namespace Canvas {

class CanvasDocument;
class CanvasScene;

class CanvasView : public QGraphicsView {
    Q_OBJECT

public:
    /// Obsidian clamps the log2 zoom every animation frame to this range
    /// (app/own/2018-k7.js:821), i.e. scale in [0.0625, 2]. A hand-edited
    /// `viewState.zoom` outside it is accepted by its loader and clamped
    /// here, never rejected.
    static constexpr double kMinZoomLog2 = -4.0;
    static constexpr double kMaxZoomLog2 = 1.0;

    /// How many viewport-widths the world is widened to when restoring a
    /// viewport. 1.0 leaves the target exactly on the pannable boundary,
    /// where quantisation and scrollbar appearance cost a whole device pixel;
    /// 1.5 puts it strictly inside with a viewport of slack in every
    /// direction, which measures exact to the rounding step on both axes.
    static constexpr double kRestoreWorldViewports = 1.5;

    explicit CanvasView(QWidget *parent = nullptr);

    void setDocument(CanvasDocument *doc);
    CanvasDocument *document() const;
    CanvasScene *canvasScene() const { return m_scene; }
    void zoomToFit();
    void zoomIn();
    void zoomOut();

    /// Obsidian's canvas-leaf viewport, in exactly Obsidian's shape so it
    /// can live verbatim in a workspace.json `ViewState.state`:
    /// `{x, y, zoom}` where `zoom` is **log2 of the scale factor** and
    /// `x`/`y` are the scene point drawn at the **viewport centre**
    /// (`app/own/2018-k7.js:968-974`; the transform at `:841` is
    /// `translate(W/2,H/2) scale(2^zoom) translate(-x,-y)`).
    ///
    /// `zoom` is read from the *current* transform, so it reports the live
    /// mid-gesture scale rather than a settle target — the closest analogue
    /// of Obsidian's target-vs-current split that QGraphicsView has.
    QJsonObject viewState() const;

    /// Inverse of viewState(). All-or-nothing, matching Obsidian's
    /// `Number.isNumber(x) && Number.isNumber(y) && Number.isNumber(zoom)`
    /// guard (`app/own/2018-k7.js:966`): a partial or non-numeric payload is
    /// ignored wholesale rather than merged, so a hand-edited half-state
    /// cannot nudge the viewport.
    ///
    /// `x`/`y` land to within one scroll unit of the request -- a scroll
    /// unit is one device pixel, and QGraphicsView pans by scrollbar value,
    /// whereas Obsidian sets a continuous CSS transform on the canvas element
    /// (`app/own/2018-k7.js:841`). `zoom` is exact. Measured worst case across
    /// the zoom range and targets spanning the world: 0.5 device pixels.
    void setViewState(const QJsonObject &state);

    /// Cluster O Phase O4 (O4.T3) — fits the view to the current
    /// selection's bounding rect (Obsidian's "zoom to selection"). No-op
    /// if nothing is selected — CanvasViewActions' Tier B disables the
    /// action itself in that case, but this stays defensive.
    void zoomToSelection();

    /// Cluster O Phase O4 (O4.T1/T2) — app-wide "Show grid" toggle,
    /// backed by corbomite.kcfg's Canvas group. Honoured in
    /// drawBackground(); the background fill itself is unaffected.
    void setGridVisible(bool visible);
    bool gridVisible() const { return m_gridVisible; }

Q_SIGNALS:
    void cardDoubleClicked(const QString &nodeId);
    void selectionChanged(const QStringList &selectedIds);

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void drawBackground(QPainter *painter, const QRectF &rect) override;
    void mouseMoveEvent(QMouseEvent *event) override;

private:
    /// Nudge the scrollbars until the scene point (x, y) sits at the viewport
    /// centre. Iterates because where that lands depends on the current
    /// viewport size and scrollbar ranges; converges to within a rounding step.
    void centreScenePoint(double x, double y);

    /// M4.2 — edge auto-pan. Called from mouseMoveEvent() and from the
    /// auto-pan timer tick to decide whether the timer should be
    /// running: only while CanvasScene::isDragActive() AND the last known
    /// viewport-local cursor position is within kAutoPanMargin of an edge.
    void updateAutoPan();
    /// M4.2 — ~60Hz timer tick: scrolls the view a small step toward
    /// whichever edge(s) the cursor is near, then re-delivers a synthetic
    /// mouse-move at the same viewport-local position through the normal
    /// event path so SelectMoveTool keeps extending the drag (the scroll
    /// changes scenePos under an otherwise-unmoved cursor).
    void autoPanTick();

    CanvasScene *m_scene = nullptr;
    CanvasDocument *m_document = nullptr;
    QTimer *m_autoPanTimer = nullptr;
    QPoint m_lastViewportPos;
    bool m_gridVisible = true;
};

} // namespace Canvas
