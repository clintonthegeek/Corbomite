// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <graffodil/GraphEdgeItem.h>
#include "CanvasTypes.h"

namespace Canvas {

class CanvasNodeItem;

/// Canvas edge — a Graffodil::GraphEdgeItem subclass. Path/arrow/label/
/// hit-shape rendering all come from the base class; this wraps CanvasEdge
/// disk-model data around it and preserves the document id (the base class
/// generates its own uuid-shaped id which we must NOT use — .canvas edge
/// ids are load-bearing).
///
/// See docs/superpowers/specs/2026-08-19-cluster-m1-graffodil-rebase-design.md §3.2.
class EdgeItem : public Graffodil::GraphEdgeItem {
public:
    EdgeItem(CanvasNodeItem *from, CanvasNodeItem *to, const CanvasEdge &data);

    // --- Graffodil::IGraphEdge ---
    QString edgeId() const override;

    void setEdgeData(const CanvasEdge &data);
    CanvasEdge edgeData() const;

private:
    void applyEndsAndPen();

    CanvasEdge m_data;
};

// Alias used by the migration spec / plan; CanvasEdgeItem is the "new" name,
// EdgeItem is kept as the actual symbol for source-compat with the rest of
// this library (file disposition table keeps EdgeItem.h/.cpp file names).
using CanvasEdgeItem = EdgeItem;

} // namespace Canvas
