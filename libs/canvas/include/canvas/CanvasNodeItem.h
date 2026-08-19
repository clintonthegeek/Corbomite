// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QGraphicsObject>
#include <graffodil/IGraphNode.h>
#include "CanvasTypes.h"

namespace Canvas {

/// Common base for every canvas node graphics item (TextCardItem,
/// FileCardItem, GroupItem). Bridges Corbomite's CanvasNode data model to
/// Graffodil::IGraphNode, and hosts the shared resize-zone geometry math
/// that used to be triplicated across the three item classes.
///
/// See docs/superpowers/specs/2026-08-19-cluster-m1-graffodil-rebase-design.md
/// §3.1 for the full contract.
class CanvasNodeItem : public QGraphicsObject, public Graffodil::IGraphNode {
    Q_OBJECT

public:
    explicit CanvasNodeItem(const CanvasNode &data, QGraphicsItem *parent = nullptr);

    // --- Graffodil::IGraphNode ---
    QString nodeId() const override;
    QList<Graffodil::Anchor> anchors() const override;
    QRectF nodeBoundingRect() const override;
    QGraphicsItem *graphicsItem() override { return this; }
    void setGeometry(const QRectF &rect) override;

    // --- Shared canvas surface (replaces ConnectableItem + duplicated enums) ---
    virtual void setNodeData(const CanvasNode &data);
    CanvasNode nodeData() const;

    enum ResizeMode { NoResize = 0, TopLeft, Top, TopRight, Right,
                       BottomRight, Bottom, BottomLeft, Left };
    ResizeMode resizeModeAtPos(const QPointF &localPos) const;

Q_SIGNALS:
    /// Fired on double-click. Subclass-specific meaning (begin inline edit,
    /// begin group-label edit, ...).
    void editRequested();

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;

    CanvasNode m_data;
};

} // namespace Canvas
