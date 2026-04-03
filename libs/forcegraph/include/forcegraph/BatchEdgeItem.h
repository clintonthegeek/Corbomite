// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QGraphicsItem>
#include <QVector>
#include <QLineF>
#include <QColor>

namespace ForceGraph {

class BatchEdgeItem : public QGraphicsItem {
public:
    struct EdgeData {
        QLineF line;
        bool dimmed = false;
    };

    explicit BatchEdgeItem(QGraphicsItem *parent = nullptr);

    void setEdges(const QVector<EdgeData> &edges);
    void setWidthScale(double scale);
    void setShowArrows(bool show);

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private:
    QVector<EdgeData> m_edges;
    int m_normalCount = 0; // edges[0..m_normalCount) are normal, rest dimmed
    double m_widthScale = 1.0;
    bool m_showArrows = false;
};

} // namespace ForceGraph
