// SPDX-License-Identifier: GPL-3.0-or-later
#include "canvas/CanvasView.h"
#include "canvas/CanvasScene.h"
#include "canvas/CanvasDocument.h"

#include <QKeyEvent>
#include <QPainter>
#include <QUndoStack>
#include <QWheelEvent>

namespace Canvas {

static constexpr double kZoomFactor = 1.15;
static constexpr double kGridSize = 20.0;

CanvasView::CanvasView(QWidget *parent)
    : QGraphicsView(parent)
{
    m_scene = new CanvasScene(this);
    setScene(m_scene);

    setRenderHint(QPainter::Antialiasing);
    setDragMode(QGraphicsView::NoDrag); // Tools handle drag
    setTransformationAnchor(AnchorUnderMouse);
    setViewportUpdateMode(SmartViewportUpdate);

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

void CanvasView::wheelEvent(QWheelEvent *event)
{
    const double factor = (event->angleDelta().y() > 0) ? kZoomFactor : (1.0 / kZoomFactor);
    scale(factor, factor);
    event->accept();
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
