// SPDX-License-Identifier: GPL-3.0-or-later
#include "canvas/CanvasScene.h"
#include "canvas/CanvasDocument.h"
#include "canvas/CanvasTool.h"
#include "canvas/TextCardItem.h"
#include "canvas/GroupItem.h"
#include "canvas/EdgeItem.h"
#include <QUndoStack>

namespace Canvas {

CanvasScene::CanvasScene(QObject *parent)
    : QGraphicsScene(parent)
    , m_undoStack(new QUndoStack(this))
{
}

void CanvasScene::setDocument(CanvasDocument *doc)
{
    m_document = doc;
    // TODO: sync items from document
}

void CanvasScene::setActiveTool(CanvasTool *tool)
{
    if (m_activeTool)
        m_activeTool->deactivate();
    m_activeTool = tool;
    if (m_activeTool)
        m_activeTool->activate();
}

CanvasTool *CanvasScene::activeTool() const
{
    return m_activeTool;
}

TextCardItem *CanvasScene::textCardItem(const QString &id) const
{
    return m_textCardItems.value(id, nullptr);
}

GroupItem *CanvasScene::groupItem(const QString &id) const
{
    return m_groupItems.value(id, nullptr);
}

EdgeItem *CanvasScene::edgeItem(const QString &id) const
{
    return m_edgeItems.value(id, nullptr);
}

QUndoStack *CanvasScene::undoStack()
{
    return m_undoStack;
}

void CanvasScene::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_activeTool) {
        m_activeTool->mousePressEvent(event);
        return;
    }
    QGraphicsScene::mousePressEvent(event);
}

void CanvasScene::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_activeTool) {
        m_activeTool->mouseMoveEvent(event);
        return;
    }
    QGraphicsScene::mouseMoveEvent(event);
}

void CanvasScene::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_activeTool) {
        m_activeTool->mouseReleaseEvent(event);
        return;
    }
    QGraphicsScene::mouseReleaseEvent(event);
}

void CanvasScene::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
{
    // TODO: implement context menus
    QGraphicsScene::contextMenuEvent(event);
}

} // namespace Canvas
