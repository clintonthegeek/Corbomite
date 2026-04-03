// SPDX-License-Identifier: GPL-3.0-or-later
#include "forcegraph/BatchEdgeItem.h"
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QtMath>
#include <algorithm>

namespace ForceGraph {

// Conservative test: is any part of the line possibly inside the rect?
static bool edgeIntersectsRect(const QLineF &line, const QRectF &rect)
{
    // Either endpoint inside — definitely visible
    if (rect.contains(line.p1()) || rect.contains(line.p2()))
        return true;

    // Both endpoints on the same side of the rect — definitely invisible
    if (line.x1() < rect.left() && line.x2() < rect.left()) return false;
    if (line.x1() > rect.right() && line.x2() > rect.right()) return false;
    if (line.y1() < rect.top() && line.y2() < rect.top()) return false;
    if (line.y1() > rect.bottom() && line.y2() > rect.bottom()) return false;

    return true; // Might cross — draw it
}

BatchEdgeItem::BatchEdgeItem(QGraphicsItem *parent)
    : QGraphicsItem(parent)
{
    setFlag(ItemUsesExtendedStyleOption);
    setZValue(0); // Behind nodes
}

void BatchEdgeItem::setEdges(const QVector<EdgeData> &edges)
{
    m_edges = edges;
    // Partition: normal edges first, dimmed at end
    m_normalCount = static_cast<int>(
        std::partition(m_edges.begin(), m_edges.end(),
                       [](const EdgeData &e) { return !e.dimmed; })
        - m_edges.begin());
    update();
}

void BatchEdgeItem::setWidthScale(double scale)
{
    m_widthScale = scale;
    update();
}

void BatchEdgeItem::setShowArrows(bool show)
{
    m_showArrows = show;
    update();
}

QRectF BatchEdgeItem::boundingRect() const
{
    return QRectF(-10000, -10000, 20000, 20000);
}

void BatchEdgeItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    if (m_edges.isEmpty())
        return;

    double lod = QStyleOptionGraphicsItem::levelOfDetailFromTransform(
        painter->worldTransform());

    // At extreme zoom-out, edges are invisible
    if (lod < 0.03)
        return;

    // Visible rect in scene coordinates for viewport culling
    QRectF visibleRect = painter->worldTransform().inverted().mapRect(
        QRectF(painter->viewport()));
    visibleRect.adjust(-50, -50, 50, 50); // margin for edges crossing viewport

    // Build visible edge lists, culling by viewport and screen-space length
    QVector<QLineF> normalLines;
    QVector<QLineF> dimmedLines;
    normalLines.reserve(m_normalCount);
    dimmedLines.reserve(m_edges.size() - m_normalCount);

    const QTransform &xform = painter->worldTransform();

    // Normal edges: [0, m_normalCount)
    for (int i = 0; i < m_normalCount; ++i) {
        const auto &edge = m_edges[i];

        if (!edgeIntersectsRect(edge.line, visibleRect))
            continue;

        // Skip sub-pixel edges (both endpoints map to same ~2px area)
        QPointF sp1 = xform.map(edge.line.p1());
        QPointF sp2 = xform.map(edge.line.p2());
        double dx = sp2.x() - sp1.x();
        double dy = sp2.y() - sp1.y();
        if (dx * dx + dy * dy < 4.0) // < 2px screen distance
            continue;

        normalLines.append(edge.line);
    }

    // Dimmed edges: [m_normalCount, end)
    for (int i = m_normalCount; i < m_edges.size(); ++i) {
        const auto &edge = m_edges[i];

        if (!edgeIntersectsRect(edge.line, visibleRect))
            continue;

        QPointF sp1 = xform.map(edge.line.p1());
        QPointF sp2 = xform.map(edge.line.p2());
        double dx = sp2.x() - sp1.x();
        double dy = sp2.y() - sp1.y();
        if (dx * dx + dy * dy < 4.0)
            continue;

        dimmedLines.append(edge.line);
    }

    // Batch-draw normal edges
    if (!normalLines.isEmpty()) {
        painter->setPen(QPen(QColor(150, 150, 150, 60), 0.8 * m_widthScale));
        painter->drawLines(normalLines);
    }

    // Batch-draw dimmed edges
    if (!dimmedLines.isEmpty()) {
        painter->setPen(QPen(QColor(200, 200, 200, 20), 0.3 * m_widthScale));
        painter->drawLines(dimmedLines);
    }

    // Arrowheads on normal edges only — auto-suppress at low LOD
    if (m_showArrows && lod > 0.5 && !normalLines.isEmpty()) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(150, 150, 150, 60));
        double arrowSize = 6.0 * m_widthScale;

        for (const auto &line : normalLines) {
            if (line.length() < 1.0)
                continue;

            double angle = std::atan2(-line.dy(), line.dx());
            QPointF tip = line.p2();
            QPointF p1 = tip + QPointF(std::sin(angle - M_PI / 3) * arrowSize,
                                       std::cos(angle - M_PI / 3) * arrowSize);
            QPointF p2 = tip + QPointF(std::sin(angle - M_PI + M_PI / 3) * arrowSize,
                                       std::cos(angle - M_PI + M_PI / 3) * arrowSize);
            QPolygonF arrow;
            arrow << tip << p1 << p2;
            painter->drawPolygon(arrow);
        }
    }
}

} // namespace ForceGraph
