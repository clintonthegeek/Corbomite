// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QGraphicsObject>
#include <memory>
#include "CanvasTypes.h"
#include "ConnectableItem.h"
#include "corbomite/core/RenderedDocument.h"

namespace Canvas {

class FileCardItem : public QGraphicsObject, public ConnectableItem {
    Q_OBJECT

public:
    FileCardItem(const CanvasNode &data, QGraphicsItem *parent = nullptr);

    void setNodeData(const CanvasNode &data);
    CanvasNode nodeData() const;
    QString nodeId() const override;

    void setRenderedDocument(std::unique_ptr<Corbomite::RenderedDocument> doc);

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    QPointF connectionPoint(Side side) const override;
    QGraphicsObject *asGraphicsObject() override { return this; }

    enum ResizeMode { NoResize = 0, TopLeft, Top, TopRight, Right, BottomRight, Bottom, BottomLeft, Left };
    ResizeMode resizeModeAtPos(const QPointF &localPos) const;

Q_SIGNALS:
    void positionChanged();
    void sizeChanged();
    void editRequested();
    void refreshRequested();

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;

private:
    QString displayTitle() const;

    CanvasNode m_data;
    std::unique_ptr<Corbomite::RenderedDocument> m_renderedDoc;
};

} // namespace Canvas
