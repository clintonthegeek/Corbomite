// SPDX-License-Identifier: GPL-3.0-or-later
#include "canvas/CanvasView.h"
#include "canvas/CanvasScene.h"
#include "canvas/CanvasDocument.h"

#include <QApplication>
#include <QClipboard>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QTimer>
#include <QUndoStack>

namespace Canvas {

static constexpr double kZoomFactor = 1.15;
static constexpr double kGridSize = 20.0;

// M4.2 edge auto-pan (Appendix A: Obsidian target ~60Hz near the viewport
// wrapper's edge). Margin/step aren't in Appendix A's normative table
// (unspecified by the audit) — picked as plausible, "keep simple" values.
static constexpr int kAutoPanMargin = 30;     // px from viewport edge that triggers auto-pan
static constexpr int kAutoPanStep = 12;       // scroll-bar units per tick, toward the near edge
static constexpr int kAutoPanIntervalMs = 16; // ~60Hz

CanvasView::CanvasView(QWidget *parent)
    : QGraphicsView(parent)
{
    m_scene = new CanvasScene(this);
    setScene(m_scene);

    setRenderHint(QPainter::Antialiasing);
    setDragMode(QGraphicsView::NoDrag); // Tools handle drag
    setTransformationAnchor(AnchorUnderMouse);
    setViewportUpdateMode(SmartViewportUpdate);
    // M2.3 — required so QGraphicsView forwards drag/drop events to the
    // scene's dragEnterEvent/dragMoveEvent/dropEvent.
    setAcceptDrops(true);

    // Forward cardDoubleClicked from scene
    connect(m_scene, &CanvasScene::cardDoubleClicked,
            this, &CanvasView::cardDoubleClicked);
}

void CanvasView::setDocument(CanvasDocument *doc)
{
    m_document = doc;
    m_scene->setDocument(doc);
}

CanvasDocument *CanvasView::document() const
{
    return m_document;
}

void CanvasView::zoomToFit()
{
    if (!scene() || scene()->items().isEmpty())
        return;
    fitInView(scene()->itemsBoundingRect().adjusted(-50, -50, 50, 50),
              Qt::KeepAspectRatio);
}

void CanvasView::zoomIn()
{
    scale(kZoomFactor, kZoomFactor);
}

void CanvasView::zoomOut()
{
    scale(1.0 / kZoomFactor, 1.0 / kZoomFactor);
}

void CanvasView::keyPressEvent(QKeyEvent *event)
{
    const bool ctrl = event->modifiers() & Qt::ControlModifier;
    const bool shift = event->modifiers() & Qt::ShiftModifier;

    if (ctrl && event->key() == Qt::Key_Z && !shift) {
        // Ctrl+Z: undo
        m_scene->undoStack()->undo();
        return;
    }

    if ((ctrl && event->key() == Qt::Key_Y) ||
        (ctrl && shift && event->key() == Qt::Key_Z)) {
        // Ctrl+Y or Ctrl+Shift+Z: redo
        m_scene->undoStack()->redo();
        return;
    }

    // M2.4 clipboard. Note: when the inline-edit QTextEdit proxy has focus,
    // OS-level keyboard focus is on that child widget directly, so these
    // never fire during in-place text editing (same reasoning as the
    // Ctrl+Z/Y handling above).
    if (ctrl && event->key() == Qt::Key_C && !shift) {
        m_scene->copySelectionToClipboard();
        return;
    }
    if (ctrl && event->key() == Qt::Key_X && !shift) {
        m_scene->cutSelectionToClipboard();
        return;
    }
    if (ctrl && event->key() == Qt::Key_V && !shift) {
        const QString text = QGuiApplication::clipboard()->text();
        const QPointF center = mapToScene(viewport()->rect().center());
        m_scene->pasteCanvasJsonOrText(text, center);
        return;
    }

    switch (event->key()) {
    case Qt::Key_Home:
        zoomToFit();
        return;
    case Qt::Key_Plus:
    case Qt::Key_Equal:
        zoomIn();
        return;
    case Qt::Key_Minus:
        zoomOut();
        return;
    default:
        break;
    }

    // Delegate other keys to scene (tools handle Delete, arrows, Ctrl+A)
    QGraphicsView::keyPressEvent(event);
}

void CanvasView::mouseMoveEvent(QMouseEvent *event)
{
    QGraphicsView::mouseMoveEvent(event);
    m_lastViewportPos = event->pos();
    updateAutoPan();
}

void CanvasView::updateAutoPan()
{
    // Scoped to CanvasScene::isDragActive() — set/cleared around a plain
    // node-move drag (Graffodil SelectMoveTool's dragBegan/dragEnded) —
    // rather than also covering CanvasResizeTool drags. A first pass:
    // resize handles are dragged right at a node's own edge, not typically
    // toward the far side of the viewport, so leaving resize out keeps
    // this mechanism to one drag-state flag per the plan's "keep simple"
    // steer. Wiring CanvasResizeTool's press/release into the same flag
    // is a reasonable follow-up if resize-near-edge turns out to want it.
    if (!m_scene || !m_scene->isDragActive()) {
        if (m_autoPanTimer)
            m_autoPanTimer->stop();
        return;
    }

    const QRect vp = viewport()->rect();
    const bool nearEdge =
        m_lastViewportPos.x() < kAutoPanMargin ||
        m_lastViewportPos.x() > vp.width() - kAutoPanMargin ||
        m_lastViewportPos.y() < kAutoPanMargin ||
        m_lastViewportPos.y() > vp.height() - kAutoPanMargin;

    if (!nearEdge) {
        if (m_autoPanTimer)
            m_autoPanTimer->stop();
        return;
    }

    if (!m_autoPanTimer) {
        m_autoPanTimer = new QTimer(this);
        m_autoPanTimer->setInterval(kAutoPanIntervalMs);
        connect(m_autoPanTimer, &QTimer::timeout, this, &CanvasView::autoPanTick);
    }
    if (!m_autoPanTimer->isActive())
        m_autoPanTimer->start();
}

void CanvasView::autoPanTick()
{
    if (!m_scene || !m_scene->isDragActive()) {
        if (m_autoPanTimer)
            m_autoPanTimer->stop();
        return;
    }

    const QRect vp = viewport()->rect();
    int dx = 0, dy = 0;
    if (m_lastViewportPos.x() < kAutoPanMargin)
        dx = -kAutoPanStep;
    else if (m_lastViewportPos.x() > vp.width() - kAutoPanMargin)
        dx = kAutoPanStep;
    if (m_lastViewportPos.y() < kAutoPanMargin)
        dy = -kAutoPanStep;
    else if (m_lastViewportPos.y() > vp.height() - kAutoPanMargin)
        dy = kAutoPanStep;

    if (dx == 0 && dy == 0) {
        // Cursor drifted back away from the edge without a mouseMoveEvent
        // catching it (e.g. focus/geometry change) — nothing to do.
        m_autoPanTimer->stop();
        return;
    }

    if (dx != 0)
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() + dx);
    if (dy != 0)
        verticalScrollBar()->setValue(verticalScrollBar()->value() + dy);

    // Re-deliver a mouse-move at the SAME viewport-local position through
    // the normal event path (QApplication::sendEvent on the viewport, the
    // same path a real OS mouse-move takes to reach this class's own
    // mouseMoveEvent override / the scene's tool dispatch) so
    // SelectMoveTool keeps extending the drag: scenePos shifts because the
    // view just scrolled underneath an otherwise-unmoved cursor.
    QMouseEvent synthetic(QEvent::MouseMove, m_lastViewportPos,
                           viewport()->mapToGlobal(m_lastViewportPos),
                           Qt::NoButton, QApplication::mouseButtons(),
                           QApplication::keyboardModifiers());
    QApplication::sendEvent(viewport(), &synthetic);
}

void CanvasView::drawBackground(QPainter *painter, const QRectF &rect)
{
    // Fill background with white
    painter->fillRect(rect, Qt::white);

    // Draw dotted grid
    painter->setPen(QPen(QColor(200, 200, 200), 0.5));

    const double left = static_cast<int>(rect.left() / kGridSize) * kGridSize;
    const double top = static_cast<int>(rect.top() / kGridSize) * kGridSize;

    for (double x = left; x < rect.right(); x += kGridSize) {
        for (double y = top; y < rect.bottom(); y += kGridSize) {
            painter->drawPoint(QPointF(x, y));
        }
    }
}

} // namespace Canvas
