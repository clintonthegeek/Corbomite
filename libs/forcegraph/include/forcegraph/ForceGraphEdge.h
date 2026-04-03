// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QGraphicsLineItem>
namespace ForceGraph {
class ForceGraphNode;
class ForceGraphEdge : public QGraphicsLineItem {
public:
    ForceGraphEdge(ForceGraphNode *source, ForceGraphNode *target, QGraphicsItem *parent = nullptr);
    void adjust();
    ForceGraphNode *sourceNode() const;
    ForceGraphNode *targetNode() const;
    bool isDimmed() const;
    bool isHighlighted() const;
    void setDimmed(bool dimmed);
    void setHighlighted(bool highlighted);
    void setWidthScale(double scale);
    void setShowArrows(bool show);
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
private:
    void updatePen();
    ForceGraphNode *m_source;
    ForceGraphNode *m_target;
    bool m_dimmed = false;
    bool m_highlighted = false;
    double m_widthScale = 1.0;
    bool m_showArrows = false;
};
} // namespace ForceGraph
