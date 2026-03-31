// SPDX-License-Identifier: GPL-3.0-or-later
#include "canvas/CanvasTool.h"
#include "canvas/CanvasScene.h"
#include <QGraphicsSceneMouseEvent>

namespace Canvas {

// --- CanvasTool ---

CanvasTool::CanvasTool(CanvasScene *scene, QObject *parent)
    : QObject(parent)
    , m_scene(scene)
{
}

// --- SelectMoveTool ---

SelectMoveTool::SelectMoveTool(CanvasScene *scene, QObject *parent)
    : CanvasTool(scene, parent)
{
}

void SelectMoveTool::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    Q_UNUSED(event);
    // TODO: implement
}

void SelectMoveTool::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    Q_UNUSED(event);
    // TODO: implement
}

void SelectMoveTool::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    Q_UNUSED(event);
    // TODO: implement
}

// --- CreateCardTool ---

CreateCardTool::CreateCardTool(CanvasScene *scene, QObject *parent)
    : CanvasTool(scene, parent)
{
}

void CreateCardTool::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    Q_UNUSED(event);
    // TODO: implement
}

void CreateCardTool::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    Q_UNUSED(event);
    // TODO: implement
}

void CreateCardTool::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    Q_UNUSED(event);
    // TODO: implement
}

// --- CreateEdgeTool ---

CreateEdgeTool::CreateEdgeTool(CanvasScene *scene, QObject *parent)
    : CanvasTool(scene, parent)
{
}

void CreateEdgeTool::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    Q_UNUSED(event);
    // TODO: implement
}

void CreateEdgeTool::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    Q_UNUSED(event);
    // TODO: implement
}

void CreateEdgeTool::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    Q_UNUSED(event);
    // TODO: implement
}

} // namespace Canvas
