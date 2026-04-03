// SPDX-License-Identifier: GPL-3.0-or-later
#include "forcegraph/ForceGraphNode.h"
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <cmath>

namespace ForceGraph {

// Text grows at 3/4 the rate of graph zoom — parallax depth effect
static constexpr double TextZoomExponent = 0.75;
static constexpr double BaseFontSize = 10.0;
static constexpr double TextBoxWidth = 120.0;
static constexpr double TextBoxHeight = 40.0;

ForceGraphNode::ForceGraphNode(const GraphNode &data, QGraphicsItem *parent)
    : QGraphicsEllipseItem(parent)
    , m_data(data)
{
    double r = m_data.radius;
    setRect(-r, -r, 2 * r, 2 * r);
    setPos(m_data.position);
    setFlag(QGraphicsItem::ItemIsMovable, true);
    setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
    setAcceptHoverEvents(true);
    setZValue(1); // Above edges
    if (!m_data.tooltip.isEmpty())
        setToolTip(m_data.tooltip);
}

void ForceGraphNode::setData(const GraphNode &data)
{
    m_data = data;
    double r = m_data.radius * m_sizeScale;
    setRect(-r, -r, 2 * r, 2 * r);
}

QString ForceGraphNode::nodeId() const
{
    return m_data.id;
}

QString ForceGraphNode::nodeLabel() const
{
    return m_data.label;
}

QColor ForceGraphNode::nodeColor() const
{
    return m_data.color;
}

double ForceGraphNode::nodeRadius() const
{
    return m_data.radius;
}

int ForceGraphNode::nodeDegree() const
{
    return m_data.degree;
}

bool ForceGraphNode::isDimmed() const
{
    return m_dimmed;
}

bool ForceGraphNode::isHighlighted() const
{
    return m_highlighted;
}

void ForceGraphNode::setHighlighted(bool highlighted)
{
    m_highlighted = highlighted;
    update();
}

void ForceGraphNode::setDimmed(bool dimmed)
{
    m_dimmed = dimmed;
    update();
}

void ForceGraphNode::setNodeSizeScale(double scale)
{
    m_sizeScale = scale;
    double r = m_data.radius * m_sizeScale;
    setRect(-r, -r, 2 * r, 2 * r);
    update();
}

void ForceGraphNode::setTextFadeThreshold(double threshold)
{
    m_textFadeThreshold = threshold;
    update();
}

void ForceGraphNode::setMaxDegree(int maxDeg)
{
    m_maxDegree = std::max(maxDeg, 1);
}

void ForceGraphNode::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    double lod = QStyleOptionGraphicsItem::levelOfDetailFromTransform(
        painter->worldTransform());

    QColor color = m_data.color;
    if (m_dimmed) {
        color.setAlphaF(0.15);
    }
    if (m_highlighted) {
        color = color.lighter(130);
    }

    painter->setPen(Qt::NoPen);
    painter->setBrush(color);

    if (lod < 0.05) {
        painter->fillRect(QRectF(-1, -1, 2, 2), color);
        return;
    }

    if (lod < 0.3) {
        painter->drawEllipse(rect());
        return;
    }

    // Hub glow: soft halo behind the node (only at medium+ zoom)
    if (m_data.type == NodeType::Hub && !m_dimmed) {
        QColor glow = color;
        glow.setAlphaF(0.25);
        double gr = m_data.radius * m_sizeScale * 1.8;
        painter->setBrush(glow);
        painter->drawEllipse(QPointF(0, 0), gr, gr);
        painter->setBrush(color);
    }

    // Unresolved: dashed border
    if (m_data.type == NodeType::Unresolved && !m_dimmed) {
        QPen dashPen(color.darker(120), 1.0, Qt::DashLine);
        painter->setPen(dashPen);
    } else if (m_highlighted) {
        painter->setPen(QPen(color.darker(150), 2));
    }

    painter->drawEllipse(rect());

    // Label: semantic zoom — high-degree nodes show labels first
    if (m_data.label.isEmpty() || m_dimmed || lod < m_textFadeThreshold)
        return;

    // Degree-based label visibility: at threshold LOD only top-degree nodes
    // show labels; by 2x threshold all labels are visible
    double threshold = std::max(m_textFadeThreshold, 0.01);
    double labelVisibility = (lod - threshold) / threshold;
    labelVisibility = std::clamp(labelVisibility, 0.0, 1.0);
    double degreeThreshold = (1.0 - labelVisibility) * m_maxDegree;
    if (m_data.degree < degreeThreshold)
        return;

    // Parallax text scaling: font grows at 3/4 the rate of zoom.
    // In scene coords the painter is at `lod` scale, so to achieve
    // apparent growth of lod^0.75 we set font size to base / lod^0.25.
    double parallaxScale = std::pow(lod, TextZoomExponent - 1.0); // lod^(-0.25)
    double fontSize = BaseFontSize * parallaxScale;
    if (fontSize < 0.5)
        return;

    QFont font;
    font.setPointSizeF(fontSize);
    painter->setFont(font);
    painter->setPen(QColor(80, 80, 80));

    // Text rect: also counter-scaled so it doesn't blow up with zoom
    double w = TextBoxWidth * parallaxScale;
    double h = TextBoxHeight * parallaxScale;
    double r = m_data.radius * m_sizeScale;
    double gap = 3.0 * parallaxScale;
    QRectF textRect(QPointF(-w / 2.0, r + gap), QSizeF(w, h));
    painter->drawText(textRect, Qt::AlignHCenter | Qt::TextWordWrap, m_data.label);
}

} // namespace ForceGraph
