// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QGraphicsObject>
#include "CanvasTypes.h"

namespace Canvas {

class CanvasScene;

class TextCardItem : public QGraphicsObject {
    Q_OBJECT

public:
    TextCardItem(const CanvasNode &data, CanvasScene *scene);

    void setNodeData(const CanvasNode &data);
    CanvasNode nodeData() const;
    QString nodeId() const;

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    // Resize
    enum ResizeMode { NoResize, TopLeft, Top, TopRight, Right, BottomRight, Bottom, BottomLeft, Left };
    ResizeMode resizeModeAtPos(const QPointF &pos) const;

    // Edge connection points
    QPointF connectionPoint(Side side) const;

Q_SIGNALS:
    void positionChanged();
    void sizeChanged();
    void editingFinished(const QString &newText);

private:
    CanvasNode m_data;
    CanvasScene *m_canvasScene = nullptr;
};

} // namespace Canvas
