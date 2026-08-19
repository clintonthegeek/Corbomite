// SPDX-License-Identifier: GPL-3.0-or-later
#include "canvas/CanvasDuplicateDragTool.h"
#include "canvas/CanvasCommands.h"
#include "canvas/CanvasDocument.h"
#include "canvas/CanvasScene.h"
#include "canvas/FileCardItem.h"
#include "canvas/GroupItem.h"
#include "canvas/TextCardItem.h"

#include <graffodil/GraphScene.h>
#include <graffodil/IGraphNode.h>

#include <KLocalizedString>
#include <QApplication>
#include <QGraphicsSceneMouseEvent>
#include <QLineF>
#include <QUndoCommand>
#include <QUndoStack>

namespace Canvas {

CanvasNodeItem *findAltDragDuplicateTarget(Graffodil::GraphScene *scene, const QPointF &scenePos)
{
    if (!scene)
        return nullptr;

    CanvasNodeItem *best = nullptr;
    qreal bestZ = 0.0;
    bool haveBest = false;
    for (auto *node : scene->nodes()) {
        auto *item = dynamic_cast<CanvasNodeItem *>(node);
        if (!item || !item->isSelected())
            continue;
        const QPointF localPos = item->mapFromScene(scenePos);
        if (!item->boundingRect().contains(localPos))
            continue;
        if (!haveBest || item->zValue() > bestZ) {
            best = item;
            bestZ = item->zValue();
            haveBest = true;
        }
    }
    return best;
}

void CanvasDuplicateDragTool::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    m_canvasScene = dynamic_cast<CanvasScene *>(scene());
    if (!m_canvasScene || event->button() != Qt::LeftButton)
        return;

    if (!findAltDragDuplicateTarget(scene(), event->scenePos()))
        return;

    auto *document = m_canvasScene->document();
    if (!document)
        return;

    // Snapshot the current selection (the originals) before touching it.
    QVector<CanvasNode> originals;
    for (auto *node : scene()->selectedNodes()) {
        if (auto *item = dynamic_cast<CanvasNodeItem *>(node))
            originals.append(item->nodeData());
    }
    if (originals.isEmpty())
        return;

    QHash<QString, QString> idMap; // original id -> clone id
    m_clones.clear();
    m_cloneStartPositions.clear();
    m_cloneEdges.clear();
    m_originalIds.clear();
    for (const auto &orig : originals)
        m_originalIds.append(orig.id);

    for (const auto &orig : originals) {
        CanvasNode clone = orig;
        clone.id = CanvasDocument::generateId();
        idMap.insert(orig.id, clone.id);

        CanvasNodeItem *item = nullptr;
        switch (clone.type) {
        case NodeType::Text:
            item = m_canvasScene->addTextCardItem(clone);
            break;
        case NodeType::File:
            item = m_canvasScene->addFileCardItem(clone);
            break;
        case NodeType::Group:
            item = m_canvasScene->addGroupItemToScene(clone);
            break;
        case NodeType::Link:
            break;
        }
        if (!item)
            continue;
        m_clones.insert(clone.id, item);
        m_cloneStartPositions.insert(clone.id, item->pos());
    }

    // Clone edges whose BOTH endpoints were in the original selection.
    for (const auto &edge : document->edges()) {
        if (idMap.contains(edge.fromNode) && idMap.contains(edge.toNode)) {
            CanvasEdge cloneEdge = edge;
            cloneEdge.id = CanvasDocument::generateId();
            cloneEdge.fromNode = idMap.value(edge.fromNode);
            cloneEdge.toNode = idMap.value(edge.toNode);
            m_cloneEdges.append(cloneEdge);
        }
    }

    // Drag the clones instead of the originals: deselect originals, select
    // the clones.
    for (auto *node : scene()->selectedNodes()) {
        if (auto *item = dynamic_cast<CanvasNodeItem *>(node))
            item->setSelected(false);
    }
    for (auto *item : std::as_const(m_clones))
        item->setSelected(true);

    m_pressScenePos = event->scenePos();
    m_dragging = true;
}

void CanvasDuplicateDragTool::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (!m_dragging)
        return;

    const QPointF delta = event->scenePos() - m_pressScenePos;
    for (auto it = m_clones.constBegin(); it != m_clones.constEnd(); ++it)
        it.value()->setPos(m_cloneStartPositions.value(it.key()) + delta);
}

void CanvasDuplicateDragTool::removeUncommittedClones()
{
    for (auto it = m_clones.constBegin(); it != m_clones.constEnd(); ++it) {
        switch (it.value()->nodeData().type) {
        case NodeType::Text: m_canvasScene->removeTextCardItem(it.key()); break;
        case NodeType::File: m_canvasScene->removeFileCardItem(it.key()); break;
        case NodeType::Group: m_canvasScene->removeGroupItem(it.key()); break;
        case NodeType::Link: break;
        }
    }
    // Restore the original selection so a discarded (below-threshold)
    // Alt+click behaves like a plain click rather than leaving nothing
    // selected.
    for (const auto &id : std::as_const(m_originalIds)) {
        if (auto *item = m_canvasScene->connectableItem(id))
            item->setSelected(true);
    }
}

void CanvasDuplicateDragTool::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (!m_dragging || !m_canvasScene || m_clones.isEmpty()) {
        m_dragging = false;
        m_clones.clear();
        m_cloneStartPositions.clear();
        m_cloneEdges.clear();
        m_originalIds.clear();
        return;
    }

    auto *document = m_canvasScene->document();

    // A bare Alt+click (press+release with no real drag) still clones the
    // selection eagerly in mousePressEvent (the tool needs live items to
    // drag), but committing a duplicate from a non-drag click would be a
    // surprising side effect of what looks like a plain click. Below
    // QApplication::startDragDistance() (Qt's own click-vs-drag threshold),
    // discard the clones instead of committing them.
    const qreal distance = event ? QLineF(QPointF(), event->scenePos() - m_pressScenePos).length() : 0.0;
    if (!document || distance < QApplication::startDragDistance()) {
        removeUncommittedClones();
        m_dragging = false;
        m_clones.clear();
        m_cloneStartPositions.clear();
        m_cloneEdges.clear();
        m_originalIds.clear();
        return;
    }

    // One compound undo command for the whole duplicate-and-drag gesture.
    auto *parentCmd = new QUndoCommand(i18n("Duplicate"));
    for (auto it = m_clones.constBegin(); it != m_clones.constEnd(); ++it) {
        CanvasNode data = it.value()->nodeData();
        // nodeData() doesn't track live setPos() drags (only Cmd* writes
        // do) — read the final position off the graphics item itself.
        data.x = qRound(it.value()->pos().x());
        data.y = qRound(it.value()->pos().y());
        new CmdAddCard(document, data, parentCmd);
    }
    for (const auto &edge : std::as_const(m_cloneEdges))
        new CmdAddEdge(document, edge, parentCmd);

    m_canvasScene->undoStack()->push(parentCmd);

    m_dragging = false;
    m_clones.clear();
    m_cloneStartPositions.clear();
    m_cloneEdges.clear();
    m_originalIds.clear();
}

void CanvasDuplicateDragTool::deactivate()
{
    m_dragging = false;
    m_clones.clear();
    m_cloneStartPositions.clear();
    m_cloneEdges.clear();
    m_originalIds.clear();
    GraphTool::deactivate();
}

} // namespace Canvas
