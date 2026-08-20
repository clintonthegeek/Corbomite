// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <graffodil/EdgePathStrategy.h>
#include <graffodil/TerminusStyle.h>

namespace Canvas {

/// Obsidian-exact bezier path strategy for canvas edges.
///
/// Differs from Graffodil's stock `BezierPathStrategy` in curvature formula
/// and in inserting a 7px face-inset stub at each end. See
/// docs/superpowers/plans/2026-08-19-cluster-m-canvas-authoring-parity.md
/// Appendix A ("Bezier control offset", "Edge path inset from face").
class CanvasBezierStrategy : public Graffodil::EdgePathStrategy {
public:
    QPainterPath computePath(const Graffodil::Anchor &source,
                             const Graffodil::Anchor &target,
                             const QList<QPointF> &waypoints = {}) override;
};

/// Obsidian-exact arrowhead terminus: a 13x10.4px isosceles triangle
/// (local polygon (0,0)/(6.5,10.4)/(-6.5,10.4)), tip placed at the face,
/// rotated to point along the edge direction. See Appendix A ("Arrowhead").
///
/// Noticeably larger/wider than Graffodil's stock `TriangleTerminus`
/// (which is a 30deg/hypotenuse-10 triangle, ~half-width 5 / height 8.66
/// vs. Obsidian's half-width 6.5 / height 10.4 -- about 20% smaller in
/// both dimensions and at a shallower half-angle, 30deg vs. ~32deg), so
/// this is a distinct implementation rather than reusing the stock one.
class CanvasArrowTerminus : public Graffodil::TerminusStyle {
public:
    QPointF draw(QPainter *painter, const QPointF &tip,
                 const QPointF &from, const QPen &edgePen) override;
    qreal size() const override;
};

} // namespace Canvas
