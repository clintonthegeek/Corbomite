// SPDX-License-Identifier: GPL-3.0-or-later
#include "forcegraph/BatchNodeItem.h"
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QHash>
#include <cmath>

namespace ForceGraph {

// Must match ForceGraphNode constants
static constexpr double TextZoomExponent = 0.75;
static constexpr double BaseFontSize = 10.0;
static constexpr double TextBoxWidth = 120.0;
static constexpr double TextBoxHeight = 40.0;

BatchNodeItem::BatchNodeItem(QGraphicsItem *parent)
    : QGraphicsItem(parent)
{
    setFlag(ItemUsesExtendedStyleOption);
    setZValue(1); // Above edges
}

void BatchNodeItem::setNodes(const QVector<NodeData> &nodes)
{
    m_nodes = nodes;
    m_maxRadius = 0;
    for (const auto &n : nodes)
        m_maxRadius = std::max(m_maxRadius, n.radius);
    rebuildGrid();
    update();
}

void BatchNodeItem::setSizeScale(double scale)
{
    m_sizeScale = scale;
    update();
}

void BatchNodeItem::setTextFadeThreshold(double threshold)
{
    m_textFadeThreshold = threshold;
    update();
}

void BatchNodeItem::setMaxDegree(int maxDeg)
{
    m_maxDegree = std::max(maxDeg, 1);
}

QRectF BatchNodeItem::boundingRect() const
{
    return QRectF(-10000, -10000, 20000, 20000);
}

void BatchNodeItem::rebuildGrid()
{
    if (m_nodes.isEmpty()) return;

    QVector<QPointF> positions;
    positions.reserve(m_nodes.size());
    double minX = 1e9, minY = 1e9, maxX = -1e9, maxY = -1e9;
    for (const auto &n : m_nodes) {
        positions.append(n.position);
        minX = std::min(minX, n.position.x());
        minY = std::min(minY, n.position.y());
        maxX = std::max(maxX, n.position.x());
        maxY = std::max(maxY, n.position.y());
    }
    // Expand bounds slightly to avoid edge cases
    m_grid.build(positions, QRectF(minX - 1, minY - 1, maxX - minX + 2, maxY - minY + 2));
}

void BatchNodeItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *)
{
    double lod = QStyleOptionGraphicsItem::levelOfDetailFromTransform(
        painter->worldTransform());

    // At extreme zoom-out, nothing is visible
    if (lod < 0.03)
        return;

    // Clip to exposed area
    if (option && !option->exposedRect.isEmpty())
        painter->setClipRect(option->exposedRect);

    // Compute visible rect in scene coordinates for viewport culling
    QRectF visibleRect = painter->worldTransform().inverted().mapRect(
        QRectF(painter->viewport()));
    double maxR = m_maxRadius * m_sizeScale;
    visibleRect.adjust(-maxR, -maxR, maxR, maxR);

    // Query spatial grid for visible node indices
    QVector<int> visibleIndices;
    if (!m_grid.isEmpty()) {
        m_grid.query(visibleRect, visibleIndices);
    } else {
        // Fallback: all nodes (grid not built yet)
        visibleIndices.reserve(m_nodes.size());
        for (int i = 0; i < m_nodes.size(); ++i)
            visibleIndices.append(i);
    }

    painter->setPen(Qt::NoPen);

    // Group visible nodes by effective color for minimal state changes
    QHash<QRgb, QVector<std::pair<QPointF, double>>> colorGroups;

    for (int idx : visibleIndices) {
        const auto &node = m_nodes[idx];

        QColor c = node.color;
        if (node.dimmed)
            c.setAlphaF(0.15);
        if (node.highlighted)
            c = c.lighter(130);

        double r = node.radius * m_sizeScale;
        colorGroups[c.rgba()].append({node.position, r});
    }

    // Batch draw circles by color
    for (auto it = colorGroups.constBegin(); it != colorGroups.constEnd(); ++it) {
        painter->setBrush(QColor::fromRgba(it.key()));
        const auto &entries = it.value();

        if (lod < 0.3) {
            for (const auto &[pos, r] : entries) {
                painter->fillRect(QRectF(pos.x() - r, pos.y() - r, 2 * r, 2 * r),
                                  painter->brush());
            }
        } else {
            for (const auto &[pos, r] : entries) {
                painter->drawEllipse(pos, r, r);
            }
        }
    }

    // Labels with parallax text scaling and semantic zoom
    if (lod < m_textFadeThreshold)
        return;

    double threshold = std::max(m_textFadeThreshold, 0.01);
    double labelVisibility = (lod - threshold) / threshold;
    labelVisibility = std::clamp(labelVisibility, 0.0, 1.0);
    double degreeThreshold = (1.0 - labelVisibility) * m_maxDegree;

    double parallaxScale = std::pow(lod, TextZoomExponent - 1.0);
    double fontSize = BaseFontSize * parallaxScale;
    if (fontSize < 0.5)
        return;

    // Text greeking: at borderline font size, draw gray rectangles instead
    bool greekText = fontSize < 3.0;

    if (!greekText) {
        QFont font;
        font.setPointSizeF(fontSize);
        painter->setFont(font);
        painter->setPen(QColor(80, 80, 80));
    }

    double w = TextBoxWidth * parallaxScale;
    double h = TextBoxHeight * parallaxScale;
    double gap = 3.0 * parallaxScale;

    for (int idx : visibleIndices) {
        const auto &node = m_nodes[idx];
        if (node.dimmed || node.label.isEmpty())
            continue;
        if (node.degree < degreeThreshold)
            continue;

        double r = node.radius * m_sizeScale;

        if (greekText) {
            // Gray rectangle approximating text block
            double greekW = std::min(w, node.label.size() * fontSize * 0.6);
            painter->fillRect(
                QRectF(node.position.x() - greekW / 2.0, node.position.y() + r + gap,
                       greekW, fontSize * 1.2),
                QColor(120, 120, 120, 60));
        } else {
            QRectF textRect(QPointF(node.position.x() - w / 2.0, node.position.y() + r + gap),
                            QSizeF(w, h));
            painter->drawText(textRect, Qt::AlignHCenter | Qt::TextWordWrap, node.label);
        }
    }
}

} // namespace ForceGraph
