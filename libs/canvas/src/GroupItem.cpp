// SPDX-License-Identifier: GPL-3.0-or-later
#include "canvas/GroupItem.h"

#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <utility>

namespace Canvas {

static constexpr qreal kLabelPadding = 8.0;

GroupItem::GroupItem(const CanvasNode &data, QGraphicsItem *parent)
    : CanvasNodeItem(data, parent)
    , m_lastPos(data.x, data.y)
{
    updateZOrder();
}

void GroupItem::setNodeData(const CanvasNode &data)
{
    CanvasNodeItem::setNodeData(data);
    m_lastPos = pos();
    updateZOrder();
}

void GroupItem::updateZOrder()
{
    // Appendix A "Group z-order": zValue = -width*height, so bigger groups
    // render further back. Recomputed on every geometry change (construction
    // + setNodeData, which covers resize via CmdResizeCard/document reload —
    // GraphScene::DefaultEdgeZ is -1.0 and DefaultNodeZ is 0.0, so even the
    // smallest group (kMinSize 40x40 -> -1600) stays behind both.
    setZValue(-(m_data.width * m_data.height));
}

QRectF GroupItem::boundingRect() const
{
    return QRectF(0, 0, m_data.width, m_data.height);
}

void GroupItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(widget);

    painter->setRenderHint(QPainter::Antialiasing);

    const QRectF rect = boundingRect();
    const bool selected = (option->state & QStyle::State_Selected);

    // 1. Fill background with group color at 10% opacity, or light gray if no color
    const QColor baseColor = colorFromCanvasColor(m_data.color);
    QColor fillColor;
    if (baseColor.isValid()) {
        fillColor = baseColor;
        fillColor.setAlpha(25); // ~10% opacity
    } else {
        fillColor = QColor(200, 200, 200, 25);
    }
    painter->fillRect(rect, fillColor);

    // 2. Draw border (group color at 40% opacity, or gray; blue 2px when selected)
    if (selected) {
        QPen borderPen(QColor(58, 134, 255));
        borderPen.setWidthF(2.0);
        painter->setPen(borderPen);
    } else {
        QColor borderColor;
        if (baseColor.isValid()) {
            borderColor = baseColor;
            borderColor.setAlpha(102); // ~40% opacity
        } else {
            borderColor = QColor(180, 180, 180);
        }
        QPen borderPen(borderColor);
        borderPen.setWidthF(1.0);
        painter->setPen(borderPen);
    }
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(rect);

    // 3. Draw label text at top-left with padding, bold, 11pt
    if (!m_data.label.isEmpty()) {
        QFont labelFont = painter->font();
        labelFont.setBold(true);
        labelFont.setPointSizeF(11.0);
        painter->setFont(labelFont);

        QColor textColor;
        if (baseColor.isValid()) {
            textColor = baseColor.darker(120);
        } else {
            textColor = QColor(80, 80, 80);
        }
        painter->setPen(textColor);

        const QRectF textRect(kLabelPadding, kLabelPadding,
                              rect.width() - 2 * kLabelPadding,
                              labelFont.pointSizeF() * 2);
        painter->drawText(textRect, Qt::AlignLeft | Qt::AlignTop, m_data.label);
    }

    // Resize-handle drawing moved to CanvasNodeChromeOverlay (M4.4) — one
    // shared, zoom-constant overlay retargeted to the active node, instead
    // of this triplicated per-item block. Do not re-add handle painting
    // here; see CanvasNodeChromeOverlay.h.
}

QVector<QGraphicsItem *> GroupItem::containedItems() const
{
    QVector<QGraphicsItem *> result;
    if (!scene())
        return result;

    const QRectF myRect = sceneBoundingRect();
    for (auto *item : scene()->items()) {
        if (item == this)
            continue;
        if (dynamic_cast<GroupItem *>(item))
            continue; // Skip other groups
        if (!dynamic_cast<QGraphicsObject *>(item))
            continue; // Skip non-objects (edges are QGraphicsPathItem, not QGraphicsObject)
        // M4.3 / Appendix A: full containment of the candidate's own scene
        // rect, not just its center point — a node straddling the group's
        // edge is not a member (Obsidian's rule; the old center-test picked
        // it up, which is exactly the bug M4.3 fixes).
        if (myRect.contains(item->sceneBoundingRect())) {
            result.append(item);
        }
    }
    return result;
}

QStringList GroupItem::beginDragCapture()
{
    m_capturedChildren = containedItems();
    m_capturing = true;

    QStringList ids;
    ids.reserve(m_capturedChildren.size());
    for (auto *item : std::as_const(m_capturedChildren)) {
        if (auto *node = dynamic_cast<CanvasNodeItem *>(item))
            ids.append(node->nodeId());
    }
    return ids;
}

void GroupItem::endDragCapture()
{
    m_capturedChildren.clear();
    m_capturing = false;
}

QVariant GroupItem::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if (change == ItemPositionHasChanged && !m_movingChildren) {
        const QPointF newPos = value.toPointF();
        const QPointF delta = newPos - m_lastPos;
        m_lastPos = newPos;

        // M4.3: membership is frozen at drag start (beginDragCapture), not
        // re-tested here. If no capture is active, this position change came
        // from something other than an in-progress drag of this group (undo/
        // redo replay, a reactive setNodeData from the document, an arrow-key
        // nudge) and children must NOT be dragged along.
        if (m_capturing) {
            m_movingChildren = true;
            for (auto *item : std::as_const(m_capturedChildren)) {
                item->moveBy(delta.x(), delta.y());
            }
            m_movingChildren = false;
        }
    }
    return CanvasNodeItem::itemChange(change, value);
}

void GroupItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
    Q_UNUSED(event);
    Q_EMIT labelEditRequested();
}

} // namespace Canvas
