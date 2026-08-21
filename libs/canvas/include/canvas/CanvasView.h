// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QGraphicsView>
#include <QPoint>

class QTimer;

namespace Canvas {

class CanvasDocument;
class CanvasScene;

class CanvasView : public QGraphicsView {
    Q_OBJECT

public:
    explicit CanvasView(QWidget *parent = nullptr);

    void setDocument(CanvasDocument *doc);
    CanvasDocument *document() const;
    CanvasScene *canvasScene() const { return m_scene; }
    void zoomToFit();
    void zoomIn();
    void zoomOut();

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
