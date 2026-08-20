// SPDX-License-Identifier: GPL-3.0-or-later
#include "canvas/CanvasBezierStrategy.h"

#include <QPolygonF>
#include <algorithm>
#include <cmath>

namespace Canvas {

// --- CanvasBezierStrategy ---
//
// Appendix A: "Bezier control offset" = clamp(dist/2, 70, 150) world units
// along each anchor's outwardDirection (Graffodil's stock BezierPathStrategy
// uses min(dist*0.4, 80) -- different curve, hence this class).
//
// Appendix A: "Edge path inset from face" = 7px (straight L fills the gap
// when no arrowhead). The curve itself is shortened by 7px at each end
// (moved further from the node face, out along outwardDirection -- i.e.
// deeper into the open canvas space the edge actually travels through,
// NOT back into the node interior) and a straight lineTo segment fills the
// remaining 7px stub back to the true face point. This is drawn
// unconditionally: when an arrowhead terminus is present it is painted on
// top of (and fully covers, since its height of 10.4px > 7px) that stub;
// when there is no arrowhead (EndType::None), the stub is what makes the
// line visually reach the face instead of stopping short of it. The
// strategy has no visibility into EndType, so it can't conditionally skip
// the stub -- and doesn't need to, since it costs nothing to draw when
// covered.
static constexpr qreal kFaceInset = 7.0;

QPainterPath CanvasBezierStrategy::computePath(
    const Graffodil::Anchor &source, const Graffodil::Anchor &target,
    const QList<QPointF> & /*waypoints*/)
{
    const double dx = target.scenePos.x() - source.scenePos.x();
    const double dy = target.scenePos.y() - source.scenePos.y();
    const double dist = std::sqrt(dx * dx + dy * dy);
    const double curvature = std::clamp(dist / 2.0, 70.0, 150.0);

    const QPointF ctrl1 = source.scenePos + source.outwardDirection * curvature;
    const QPointF ctrl2 = target.scenePos + target.outwardDirection * curvature;

    const QPointF sourceInset = source.scenePos + source.outwardDirection * kFaceInset;
    const QPointF targetInset = target.scenePos + target.outwardDirection * kFaceInset;

    QPainterPath path;
    path.moveTo(source.scenePos);
    path.lineTo(sourceInset);
    path.cubicTo(ctrl1, ctrl2, targetInset);
    path.lineTo(target.scenePos);
    return path;
}

// --- CanvasArrowTerminus ---
//
// Appendix A: triangle 13x10.4px, local polygon (0,0)/(6.5,10.4)/(-6.5,10.4),
// tip at the face, rotated per side. Reuses the same "math-angle" trig
// pattern as Graffodil's TriangleTerminus (see TerminusStyle.cpp) but with
// the exact half-angle/hypotenuse derived from the Obsidian polygon instead
// of TriangleTerminus's 30deg/10px constants.
static constexpr qreal kHalfWidth = 6.5;
static constexpr qreal kHeight = 10.4;

static double directionAngle(const QPointF &tip, const QPointF &from)
{
    return std::atan2(-(tip.y() - from.y()), tip.x() - from.x());
}

QPointF CanvasArrowTerminus::draw(QPainter *painter, const QPointF &tip,
                                   const QPointF &from, const QPen &edgePen)
{
    const double halfAngle = std::atan2(kHalfWidth, kHeight);
    const double hyp = std::sqrt(kHalfWidth * kHalfWidth + kHeight * kHeight);
    const double angle = directionAngle(tip, from);

    const QPointF p1 = tip - QPointF(std::cos(angle - halfAngle) * hyp,
                                     -std::sin(angle - halfAngle) * hyp);
    const QPointF p2 = tip - QPointF(std::cos(angle + halfAngle) * hyp,
                                     -std::sin(angle + halfAngle) * hyp);

    painter->save();
    painter->setPen(Qt::NoPen);
    painter->setBrush(edgePen.color());
    painter->drawPolygon(QPolygonF{tip, p1, p2});
    painter->restore();

    return tip - QPointF(std::cos(angle) * kHeight, -std::sin(angle) * kHeight);
}

qreal CanvasArrowTerminus::size() const
{
    return std::sqrt(kHalfWidth * kHalfWidth + kHeight * kHeight);
}

} // namespace Canvas
