// SPDX-License-Identifier: GPL-3.0-or-later
#include "canvas/EdgeItem.h"
#include "canvas/CanvasScene.h"
#include <canvas/CanvasPalette.h>
#include "canvas/CanvasNodeItem.h"
#include "canvas/CanvasBezierStrategy.h"

#include <graffodil/EdgePathStrategy.h>
#include <graffodil/TerminusStyle.h>

// Cluster M leftover (canvas dark mode) — see CanvasPalette.h.
static const Canvas::CanvasPalette &canvasPaletteFor(const QGraphicsScene *scene)
{
    static const Canvas::CanvasPalette kLight = Canvas::CanvasPalette::light();
    const auto *cs = qobject_cast<const Canvas::CanvasScene *>(scene);
    return cs ? cs->canvasPalette() : kLight;
}


namespace Canvas {

static constexpr qreal kDefaultPenWidth = 2.0;
static constexpr qreal kHitWidth = 24.0;

EdgeItem::EdgeItem(CanvasNodeItem *from, CanvasNodeItem *to, const CanvasEdge &data)
    : Graffodil::GraphEdgeItem(from, sideToString(data.fromSide),
                               to, sideToString(data.toSide),
                               std::make_unique<CanvasBezierStrategy>())
    , m_data(data)
{
    setHitWidth(kHitWidth);

    Graffodil::EdgeLabelStyle style;
    style.font = QFont();
    style.font.setPointSize(9);
    // Cluster M leftover: edge label colours follow the scene palette. The
    // ctor may run before the item has a scene, so fall back to light here and
    // let applyEndsAndPen()'s sibling re-apply once it is in the scene.
    {
        const Canvas::CanvasPalette &pal = canvasPaletteFor(nullptr);
        style.color = pal.edge;
        style.background = pal.edgeLabelBackground;
    }
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
    // Fallback pen colour comes from the scene palette when the item is in a
    // scene, so an un-accented edge is visible on a dark canvas.
    QPen edgePen(canvasPaletteFor(scene()).edge);
    edgePen.setWidthF(kDefaultPenWidth);
    const QColor edgeColor = colorFromCanvasColor(m_data.color);
    if (edgeColor.isValid())
        edgePen.setColor(edgeColor);
    setPen(edgePen);

    setTerminus(Graffodil::ArrowEnd::Target,
                m_data.toEnd == EndType::Arrow
                    ? std::make_unique<CanvasArrowTerminus>()
                    : std::unique_ptr<Graffodil::TerminusStyle>(std::make_unique<Graffodil::NoTerminus>()));
    setTerminus(Graffodil::ArrowEnd::Source,
                m_data.fromEnd == EndType::Arrow
                    ? std::make_unique<CanvasArrowTerminus>()
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
