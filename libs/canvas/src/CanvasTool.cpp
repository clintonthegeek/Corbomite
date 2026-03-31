// SPDX-License-Identifier: GPL-3.0-or-later
#include "canvas/CanvasTool.h"
#include "canvas/CanvasScene.h"
#include "canvas/CanvasDocument.h"
#include "canvas/TextCardItem.h"
#include "canvas/GroupItem.h"
#include "canvas/EdgeItem.h"

#include <QGraphicsSceneMouseEvent>
#include <QGraphicsRectItem>
#include <QGraphicsLineItem>
#include <QKeyEvent>
#include <QtMath>

namespace Canvas {

// ---------------------------------------------------------------------------
// CanvasTool (abstract base)
// ---------------------------------------------------------------------------

CanvasTool::CanvasTool(CanvasScene *scene, QObject *parent)
    : QObject(parent)
    , m_scene(scene)
{
}

void CanvasTool::keyPressEvent(QKeyEvent *event)
{
    Q_UNUSED(event);
}

// ---------------------------------------------------------------------------
// SelectMoveTool
// ---------------------------------------------------------------------------

SelectMoveTool::SelectMoveTool(CanvasScene *scene, QObject *parent)
    : CanvasTool(scene, parent)
{
}

void SelectMoveTool::deactivate()
{
    // Clean up rubber band if active
    if (m_rubberBand) {
        m_scene->removeItem(m_rubberBand);
        delete m_rubberBand;
        m_rubberBand = nullptr;
    }
    m_dragMode = DragMode::None;
    m_initialPositions.clear();
    m_resizeItem = nullptr;
}

void SelectMoveTool::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
        return;

    const QPointF scenePos = event->scenePos();
    m_pressScenePos = scenePos;

    // Check if click is on an item
    QGraphicsItem *hitItem = m_scene->itemAt(scenePos, QTransform());

    // Find the top-level canvas item (TextCardItem or GroupItem)
    TextCardItem *cardItem = nullptr;
    GroupItem *groupItem = nullptr;
    while (hitItem) {
        cardItem = dynamic_cast<TextCardItem *>(hitItem);
        groupItem = dynamic_cast<GroupItem *>(hitItem);
        if (cardItem || groupItem)
            break;
        hitItem = hitItem->parentItem();
    }

    QGraphicsItem *canvasItem = cardItem ? static_cast<QGraphicsItem *>(cardItem)
                              : groupItem ? static_cast<QGraphicsItem *>(groupItem)
                              : nullptr;

    const bool shiftHeld = event->modifiers() & Qt::ShiftModifier;

    if (!canvasItem) {
        // Click on empty space: start rubber-band selection
        if (!shiftHeld) {
            m_scene->clearSelection();
        }
        m_dragMode = DragMode::RubberBand;
        m_rubberBandOrigin = scenePos;

        m_rubberBand = new QGraphicsRectItem();
        m_rubberBand->setPen(QPen(QColor(58, 134, 255), 1, Qt::DashLine));
        m_rubberBand->setBrush(QColor(58, 134, 255, 30));
        m_rubberBand->setZValue(1000);
        m_scene->addItem(m_rubberBand);
        return;
    }

    // Click on a card or group item
    if (shiftHeld) {
        canvasItem->setSelected(!canvasItem->isSelected());
    } else if (!canvasItem->isSelected()) {
        m_scene->clearSelection();
        canvasItem->setSelected(true);
    }

    // Check for resize handle
    const QPointF localPos = canvasItem->mapFromScene(scenePos);
    int resizeMode = 0;
    if (cardItem) {
        resizeMode = cardItem->resizeModeAtPos(localPos);
    } else if (groupItem) {
        resizeMode = groupItem->resizeModeAtPos(localPos);
    }

    if (resizeMode != 0) {
        // Enter resize mode
        m_dragMode = DragMode::Resize;
        m_resizeMode = resizeMode;
        m_resizeItem = canvasItem;
        m_resizeOriginalRect = canvasItem->boundingRect();
        m_resizeOriginalPos = canvasItem->pos();
        return;
    }

    // Prepare for drag move: store initial positions of all selected items
    m_dragMode = DragMode::Move;
    m_initialPositions.clear();
    for (auto *item : m_scene->selectedItems()) {
        m_initialPositions.insert(item, item->pos());
    }
}

void SelectMoveTool::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    const QPointF scenePos = event->scenePos();
    const QPointF delta = scenePos - m_pressScenePos;

    switch (m_dragMode) {
    case DragMode::Resize: {
        if (!m_resizeItem)
            break;

        // Compute new geometry based on resize mode
        qreal newX = m_resizeOriginalPos.x();
        qreal newY = m_resizeOriginalPos.y();
        qreal newW = m_resizeOriginalRect.width();
        qreal newH = m_resizeOriginalRect.height();

        // Using TextCardItem::ResizeMode values (same as GroupItem)
        // TopLeft=1, Top=2, TopRight=3, Right=4, BottomRight=5, Bottom=6, BottomLeft=7, Left=8
        const bool resizeLeft   = (m_resizeMode == 1 || m_resizeMode == 7 || m_resizeMode == 8);
        const bool resizeRight  = (m_resizeMode == 3 || m_resizeMode == 4 || m_resizeMode == 5);
        const bool resizeTop    = (m_resizeMode == 1 || m_resizeMode == 2 || m_resizeMode == 3);
        const bool resizeBottom = (m_resizeMode == 5 || m_resizeMode == 6 || m_resizeMode == 7);

        if (resizeLeft) {
            newX += delta.x();
            newW -= delta.x();
        }
        if (resizeRight) {
            newW += delta.x();
        }
        if (resizeTop) {
            newY += delta.y();
            newH -= delta.y();
        }
        if (resizeBottom) {
            newH += delta.y();
        }

        // Enforce minimum size
        static constexpr qreal kMinSize = 40.0;
        if (newW < kMinSize) {
            if (resizeLeft) {
                newX -= (kMinSize - newW);
            }
            newW = kMinSize;
        }
        if (newH < kMinSize) {
            if (resizeTop) {
                newY -= (kMinSize - newH);
            }
            newH = kMinSize;
        }

        // Apply to item
        m_resizeItem->setPos(newX, newY);

        if (auto *card = dynamic_cast<TextCardItem *>(m_resizeItem)) {
            CanvasNode data = card->nodeData();
            data.x = qRound(newX);
            data.y = qRound(newY);
            data.width = qRound(newW);
            data.height = qRound(newH);
            card->setNodeData(data);
        } else if (auto *group = dynamic_cast<GroupItem *>(m_resizeItem)) {
            CanvasNode data = group->nodeData();
            data.x = qRound(newX);
            data.y = qRound(newY);
            data.width = qRound(newW);
            data.height = qRound(newH);
            group->setNodeData(data);
        }
        break;
    }

    case DragMode::Move: {
        for (auto it = m_initialPositions.constBegin(); it != m_initialPositions.constEnd(); ++it) {
            it.key()->setPos(it.value() + delta);
        }
        break;
    }

    case DragMode::RubberBand: {
        if (m_rubberBand) {
            const QRectF rect = QRectF(m_rubberBandOrigin, scenePos).normalized();
            m_rubberBand->setRect(rect);
        }
        break;
    }

    case DragMode::None:
        break;
    }
}

void SelectMoveTool::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
        return;

    switch (m_dragMode) {
    case DragMode::Resize: {
        // Finalize resize — update document with new geometry
        if (m_resizeItem && m_scene->document()) {
            if (auto *card = dynamic_cast<TextCardItem *>(m_resizeItem)) {
                m_scene->document()->updateNode(card->nodeData());
            } else if (auto *group = dynamic_cast<GroupItem *>(m_resizeItem)) {
                m_scene->document()->updateNode(group->nodeData());
            }
        }
        // TODO: push CmdResizeCard undo command (Task 6)
        m_resizeItem = nullptr;
        m_resizeMode = 0;
        break;
    }

    case DragMode::Move: {
        // Finalize move — update document with new positions
        if (m_scene->document()) {
            for (auto it = m_initialPositions.constBegin(); it != m_initialPositions.constEnd(); ++it) {
                if (auto *card = dynamic_cast<TextCardItem *>(it.key())) {
                    CanvasNode data = card->nodeData();
                    data.x = qRound(card->pos().x());
                    data.y = qRound(card->pos().y());
                    card->setNodeData(data);
                    m_scene->document()->updateNode(data);
                } else if (auto *group = dynamic_cast<GroupItem *>(it.key())) {
                    CanvasNode data = group->nodeData();
                    data.x = qRound(group->pos().x());
                    data.y = qRound(group->pos().y());
                    group->setNodeData(data);
                    m_scene->document()->updateNode(data);
                }
            }
        }
        // TODO: push CmdMoveCards undo command (Task 6)
        m_initialPositions.clear();
        break;
    }

    case DragMode::RubberBand: {
        if (m_rubberBand) {
            // Select all items within the rubber band rectangle
            const QRectF selectionRect = m_rubberBand->rect();
            const bool shiftHeld = event->modifiers() & Qt::ShiftModifier;
            if (!shiftHeld) {
                m_scene->clearSelection();
            }
            for (auto *item : m_scene->items(selectionRect, Qt::IntersectsItemBoundingRect)) {
                if (dynamic_cast<TextCardItem *>(item) || dynamic_cast<GroupItem *>(item)) {
                    item->setSelected(true);
                }
            }

            m_scene->removeItem(m_rubberBand);
            delete m_rubberBand;
            m_rubberBand = nullptr;
        }
        break;
    }

    case DragMode::None:
        break;
    }

    m_dragMode = DragMode::None;
}

void SelectMoveTool::keyPressEvent(QKeyEvent *event)
{
    if (!m_scene)
        return;

    const auto selectedItems = m_scene->selectedItems();
    const bool shiftHeld = event->modifiers() & Qt::ShiftModifier;

    switch (event->key()) {
    case Qt::Key_Delete:
    case Qt::Key_Backspace: {
        // Remove all selected items
        if (!m_scene->document())
            break;

        QStringList nodeIds;
        QStringList edgeIds;
        for (auto *item : selectedItems) {
            if (auto *card = dynamic_cast<TextCardItem *>(item)) {
                nodeIds.append(card->nodeId());
            } else if (auto *group = dynamic_cast<GroupItem *>(item)) {
                nodeIds.append(group->nodeId());
            } else if (auto *edge = dynamic_cast<EdgeItem *>(item)) {
                edgeIds.append(edge->edgeId());
            }
        }

        // Remove edges first (they reference nodes)
        for (const auto &edgeId : edgeIds) {
            m_scene->removeEdgeItem(edgeId);
            m_scene->document()->removeEdge(edgeId);
        }
        for (const auto &nodeId : nodeIds) {
            // removeNode in document also removes connected edges
            // so remove their scene items first
            const auto connectedEdges = m_scene->document()->edgesForNode(nodeId);
            for (const auto &edge : connectedEdges) {
                m_scene->removeEdgeItem(edge.id);
            }
            m_scene->removeTextCardItem(nodeId);
            m_scene->document()->removeNode(nodeId);
        }
        break;
    }

    case Qt::Key_Left:
    case Qt::Key_Right:
    case Qt::Key_Up:
    case Qt::Key_Down: {
        const qreal step = shiftHeld ? 10.0 : 1.0;
        qreal dx = 0, dy = 0;
        if (event->key() == Qt::Key_Left) dx = -step;
        else if (event->key() == Qt::Key_Right) dx = step;
        else if (event->key() == Qt::Key_Up) dy = -step;
        else if (event->key() == Qt::Key_Down) dy = step;

        for (auto *item : selectedItems) {
            item->moveBy(dx, dy);
        }
        break;
    }

    case Qt::Key_A: {
        if (event->modifiers() & Qt::ControlModifier) {
            for (auto *item : m_scene->items()) {
                if (dynamic_cast<TextCardItem *>(item) || dynamic_cast<GroupItem *>(item)) {
                    item->setSelected(true);
                }
            }
        }
        break;
    }

    default:
        break;
    }
}

// ---------------------------------------------------------------------------
// CreateCardTool
// ---------------------------------------------------------------------------

CreateCardTool::CreateCardTool(CanvasScene *scene, QObject *parent)
    : CanvasTool(scene, parent)
{
}

void CreateCardTool::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
        return;

    if (!m_scene || !m_scene->document())
        return;

    const QPointF scenePos = event->scenePos();

    // Create a new text card node at the click position
    CanvasNode node;
    node.id = CanvasDocument::generateId();
    node.type = NodeType::Text;
    node.x = qRound(scenePos.x());
    node.y = qRound(scenePos.y());
    node.width = 250;
    node.height = 100;

    // Add to document
    m_scene->document()->addNode(node);

    // Create graphics item
    auto *item = m_scene->addTextCardItem(node);

    // Select the new card
    m_scene->clearSelection();
    item->setSelected(true);

    // Switch back to SelectMoveTool
    // The tool manager (CanvasView or whoever manages tools) handles tool switching.
    // We signal that by finding the SelectMoveTool among the scene's children.
    // For now, look for it as a sibling owned by the same parent.
    auto *selectTool = m_scene->findChild<SelectMoveTool *>();
    if (selectTool) {
        m_scene->setActiveTool(selectTool);
    }
}

void CreateCardTool::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    Q_UNUSED(event);
}

void CreateCardTool::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    Q_UNUSED(event);
}

// ---------------------------------------------------------------------------
// CreateEdgeTool
// ---------------------------------------------------------------------------

CreateEdgeTool::CreateEdgeTool(CanvasScene *scene, QObject *parent)
    : CanvasTool(scene, parent)
{
}

void CreateEdgeTool::deactivate()
{
    if (m_previewLine) {
        m_scene->removeItem(m_previewLine);
        delete m_previewLine;
        m_previewLine = nullptr;
    }
    m_sourceCard = nullptr;
}

CreateEdgeTool::NearCardResult CreateEdgeTool::findNearCard(const QPointF &scenePos, qreal threshold) const
{
    NearCardResult result;
    qreal bestDist = threshold;

    for (auto *item : m_scene->items()) {
        auto *card = dynamic_cast<TextCardItem *>(item);
        if (!card)
            continue;

        const QRectF cardRect = card->sceneBoundingRect();

        // Check distance to each edge of the card
        struct { Side side; qreal dist; } edges[] = {
            { Side::Top,    qAbs(scenePos.y() - cardRect.top()) },
            { Side::Bottom, qAbs(scenePos.y() - cardRect.bottom()) },
            { Side::Left,   qAbs(scenePos.x() - cardRect.left()) },
            { Side::Right,  qAbs(scenePos.x() - cardRect.right()) },
        };

        for (const auto &e : edges) {
            // Also check that the point is within the card's range on the other axis
            bool inRange = false;
            if (e.side == Side::Top || e.side == Side::Bottom) {
                inRange = scenePos.x() >= cardRect.left() - threshold
                       && scenePos.x() <= cardRect.right() + threshold;
            } else {
                inRange = scenePos.y() >= cardRect.top() - threshold
                       && scenePos.y() <= cardRect.bottom() + threshold;
            }

            if (inRange && e.dist < bestDist) {
                bestDist = e.dist;
                result.card = card;
                result.side = e.side;
            }
        }
    }

    return result;
}

void CreateEdgeTool::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
        return;

    const QPointF scenePos = event->scenePos();

    auto nearResult = findNearCard(scenePos);
    if (!nearResult.card) {
        // Click not near any card edge — cancel and switch back
        auto *selectTool = m_scene->findChild<SelectMoveTool *>();
        if (selectTool) {
            m_scene->setActiveTool(selectTool);
        }
        return;
    }

    // Store source card and side
    m_sourceCard = nearResult.card;
    m_fromSide = nearResult.side;

    // Create preview line
    const QPointF startPoint = m_sourceCard->connectionPoint(m_fromSide);
    m_previewLine = new QGraphicsLineItem(QLineF(startPoint, scenePos));
    QPen previewPen(QColor(58, 134, 255));
    previewPen.setWidthF(2.0);
    previewPen.setStyle(Qt::DashLine);
    m_previewLine->setPen(previewPen);
    m_previewLine->setZValue(1000);
    m_scene->addItem(m_previewLine);
}

void CreateEdgeTool::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_previewLine && m_sourceCard) {
        const QPointF startPoint = m_sourceCard->connectionPoint(m_fromSide);
        m_previewLine->setLine(QLineF(startPoint, event->scenePos()));
    }
}

void CreateEdgeTool::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
        return;

    if (!m_sourceCard || !m_scene->document()) {
        // Clean up
        if (m_previewLine) {
            m_scene->removeItem(m_previewLine);
            delete m_previewLine;
            m_previewLine = nullptr;
        }
        m_sourceCard = nullptr;
        return;
    }

    // Remove preview line
    if (m_previewLine) {
        m_scene->removeItem(m_previewLine);
        delete m_previewLine;
        m_previewLine = nullptr;
    }

    // Check if release is near another card's edge
    auto nearResult = findNearCard(event->scenePos());

    if (nearResult.card && nearResult.card != m_sourceCard) {
        // Create the edge
        CanvasEdge edge;
        edge.id = CanvasDocument::generateId();
        edge.fromNode = m_sourceCard->nodeId();
        edge.toNode = nearResult.card->nodeId();
        edge.fromSide = m_fromSide;
        edge.toSide = nearResult.side;
        edge.fromEnd = EndType::None;
        edge.toEnd = EndType::Arrow;

        m_scene->document()->addEdge(edge);
        m_scene->addEdgeItemToScene(m_sourceCard, nearResult.card, edge);
    }
    // Else: released on empty space or same card — cancel

    m_sourceCard = nullptr;

    // Switch back to SelectMoveTool
    auto *selectTool = m_scene->findChild<SelectMoveTool *>();
    if (selectTool) {
        m_scene->setActiveTool(selectTool);
    }
}

} // namespace Canvas
