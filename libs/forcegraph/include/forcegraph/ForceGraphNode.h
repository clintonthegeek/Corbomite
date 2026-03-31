// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QGraphicsEllipseItem>
#include "GraphTypes.h"
namespace ForceGraph {
class ForceGraphNode : public QGraphicsEllipseItem {
public:
    explicit ForceGraphNode(const GraphNode &data, QGraphicsItem *parent = nullptr);
    void setData(const GraphNode &data);
    QString nodeId() const;
    void setHighlighted(bool highlighted);
    void setDimmed(bool dimmed);
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
private:
    GraphNode m_data;
    bool m_highlighted = false;
    bool m_dimmed = false;
};
} // namespace ForceGraph
