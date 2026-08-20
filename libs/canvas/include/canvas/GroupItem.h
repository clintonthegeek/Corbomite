// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "CanvasNodeItem.h"
#include <QStringList>
#include <QVector>

namespace Canvas {

class GroupItem : public CanvasNodeItem {
    Q_OBJECT

public:
    GroupItem(const CanvasNode &data, QGraphicsItem *parent = nullptr);

    void setNodeData(const CanvasNode &data) override;

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    /// Full-containment membership test (group.sceneRect contains the
    /// candidate's sceneRect — Appendix A "Group membership"). No stored
    /// parent; recomputed on demand.
    QVector<QGraphicsItem *> containedItems() const;

    /// M4.3 — freeze group membership for one drag gesture. Computes
    /// containedItems() once and caches it; while a capture is active,
    /// itemChange() moves exactly that cached set instead of re-testing
    /// membership every frame (replaces the old always-live center-test
    /// cascade). Returns the captured node ids so the caller (CanvasScene)
    /// can fold their final positions into the same undo command as the
    /// group's own move — Graffodil's SelectMoveTool::m_draggedNodes is
    /// private, so the captured set can't be unioned into it directly.
    QStringList beginDragCapture();
    void endDragCapture();

Q_SIGNALS:
    void labelEditRequested();

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;

private:
    void updateZOrder();

    QPointF m_lastPos;
    bool m_movingChildren = false;
    QVector<QGraphicsItem *> m_capturedChildren;
    bool m_capturing = false;
};

} // namespace Canvas
