// SPDX-License-Identifier: GPL-3.0-or-later
#include "canvas/EdgeItem.h"
#include "canvas/CanvasNodeItem.h"

#include <graffodil/EdgePathStrategy.h>
#include <graffodil/TerminusStyle.h>

namespace Canvas {

static constexpr qreal kDefaultPenWidth = 2.0;
static constexpr qreal kHitWidth = 24.0;

EdgeItem::EdgeItem(CanvasNodeItem *from, CanvasNodeItem *to, const CanvasEdge &data)
    : Graffodil::GraphEdgeItem(from, sideToString(data.fromSide),
                               to, sideToString(data.toSide),
                               std::make_unique<Graffodil::BezierPathStrategy>())
    , m_data(data)
{
    setHitWidth(kHitWidth);

    Graffodil::EdgeLabelStyle style;
    style.font = QFont();
    style.font.setPointSize(9);
    style.color = QColor(80, 80, 80);       // #505050
    style.background = QColor(255, 255, 255, 220);
    style.backgroundPadding = 4.0;
    style.positionOnPath = 0.5;
    style.offset = QPointF(0, 0);           // centered on the path, not above it
    style.rotateWithPath = false;
    setLabelStyle(style);

    applyEndsAndPen();
    setLabel(m_data.label);
    adjust();
}

QString EdgeItem::edgeId() const
{
    return m_data.id;
}

void EdgeItem::applyEndsAndPen()
{
    QPen edgePen(QColor(150, 150, 150)); // #969696 fallback
    edgePen.setWidthF(kDefaultPenWidth);
    const QColor edgeColor = colorFromCanvasColor(m_data.color);
    if (edgeColor.isValid())
        edgePen.setColor(edgeColor);
    setPen(edgePen);

    setTerminus(Graffodil::ArrowEnd::Target,
                m_data.toEnd == EndType::Arrow
                    ? std::make_unique<Graffodil::TriangleTerminus>()
                    : std::unique_ptr<Graffodil::TerminusStyle>(std::make_unique<Graffodil::NoTerminus>()));
    setTerminus(Graffodil::ArrowEnd::Source,
                m_data.fromEnd == EndType::Arrow
                    ? std::make_unique<Graffodil::TriangleTerminus>()
                    : std::unique_ptr<Graffodil::TerminusStyle>(std::make_unique<Graffodil::NoTerminus>()));
}

void EdgeItem::setEdgeData(const CanvasEdge &data)
{
    m_data = data;
    setSourceAnchorId(sideToString(data.fromSide));
    setTargetAnchorId(sideToString(data.toSide));
    applyEndsAndPen();
    setLabel(m_data.label);
    adjust();
}

CanvasEdge EdgeItem::edgeData() const
{
    return m_data;
}

} // namespace Canvas
