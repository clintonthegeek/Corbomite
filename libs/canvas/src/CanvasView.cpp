// SPDX-License-Identifier: GPL-3.0-or-later
#include "canvas/CanvasView.h"
#include "canvas/CanvasScene.h"
#include "canvas/CanvasDocument.h"

namespace Canvas {

CanvasView::CanvasView(QWidget *parent)
    : QGraphicsView(parent)
{
    m_scene = new CanvasScene(this);
    setScene(m_scene);
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
    // TODO: implement
}

void CanvasView::zoomIn()
{
    // TODO: implement
}

void CanvasView::zoomOut()
{
    // TODO: implement
}

void CanvasView::wheelEvent(QWheelEvent *event)
{
    QGraphicsView::wheelEvent(event);
}

void CanvasView::keyPressEvent(QKeyEvent *event)
{
    QGraphicsView::keyPressEvent(event);
}

void CanvasView::drawBackground(QPainter *painter, const QRectF &rect)
{
    QGraphicsView::drawBackground(painter, rect);
}

} // namespace Canvas
