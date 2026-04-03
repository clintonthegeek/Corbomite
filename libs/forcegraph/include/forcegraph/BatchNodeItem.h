// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "SpatialGrid.h"
#include <QGraphicsItem>
#include <QVector>
#include <QColor>
#include <QPointF>
#include <QString>

namespace ForceGraph {

class BatchNodeItem : public QGraphicsItem {
public:
    struct NodeData {
        QPointF position;
        double radius = 5.0;
        QColor color;
        QString label;
        int degree = 0;
        bool dimmed = false;
        bool highlighted = false;
    };

    explicit BatchNodeItem(QGraphicsItem *parent = nullptr);

    void setNodes(const QVector<NodeData> &nodes);
    void updatePositions(const QVector<QPointF> &positions);
    void setSizeScale(double scale);
    void setTextFadeThreshold(double threshold);
    void setMaxDegree(int maxDeg);

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private:
    void rebuildGrid();

    QVector<NodeData> m_nodes;
    SpatialGrid m_grid;
    double m_sizeScale = 1.0;
    double m_textFadeThreshold = 1.0;
    int m_maxDegree = 1;
    double m_maxRadius = 5.0;
    int m_gridRebuildCounter = 0;
};

} // namespace ForceGraph
