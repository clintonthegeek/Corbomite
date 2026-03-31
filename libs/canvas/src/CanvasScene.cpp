// SPDX-License-Identifier: GPL-3.0-or-later
#include "canvas/CanvasScene.h"
#include "canvas/CanvasDocument.h"
#include "canvas/CanvasTool.h"
#include "canvas/TextCardItem.h"
#include "canvas/GroupItem.h"
#include "canvas/EdgeItem.h"
#include <QUndoStack>
#include <QKeyEvent>

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

CanvasDocument *CanvasScene::document() const
{
    return m_document;
}

TextCardItem *CanvasScene::addTextCardItem(const CanvasNode &node)
{
    auto *item = new TextCardItem(node);
    addItem(item);
    m_textCardItems.insert(node.id, item);
    return item;
}

GroupItem *CanvasScene::addGroupItemToScene(const CanvasNode &node)
{
    auto *item = new GroupItem(node);
    addItem(item);
    m_groupItems.insert(node.id, item);
    return item;
}

EdgeItem *CanvasScene::addEdgeItemToScene(TextCardItem *from, TextCardItem *to, const CanvasEdge &edge)
{
    auto *item = new EdgeItem(from, to, edge);
    addItem(item);
    m_edgeItems.insert(edge.id, item);
    return item;
}

void CanvasScene::removeTextCardItem(const QString &id)
{
    if (auto *item = m_textCardItems.take(id)) {
        removeItem(item);
        delete item;
    }
}

void CanvasScene::removeEdgeItem(const QString &id)
{
    if (auto *item = m_edgeItems.take(id)) {
        removeItem(item);
        delete item;
    }
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

void CanvasScene::keyPressEvent(QKeyEvent *event)
{
    if (m_activeTool) {
        m_activeTool->keyPressEvent(event);
        return;
    }
    QGraphicsScene::keyPressEvent(event);
}

void CanvasScene::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
{
    // TODO: implement context menus
    QGraphicsScene::contextMenuEvent(event);
}

} // namespace Canvas
