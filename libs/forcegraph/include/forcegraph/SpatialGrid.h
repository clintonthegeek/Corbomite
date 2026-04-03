// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QPointF>
#include <QRectF>
#include <QVector>
#include <algorithm>

namespace ForceGraph {

/// Fixed-size spatial hash grid for fast viewport queries.
/// Build once per data update, query with viewport rect to get visible indices.
class SpatialGrid {
public:
    static constexpr int GridSize = 64;

    /// Assign positions to grid cells. Call after data changes.
    void build(const QVector<QPointF> &positions, const QRectF &bounds)
    {
        m_bounds = bounds;
        if (bounds.isEmpty()) {
            m_cellRanges.clear();
            m_indices.clear();
            return;
        }

        m_cellW = bounds.width() / GridSize;
        m_cellH = bounds.height() / GridSize;

        // Count items per cell
        const int cellCount = GridSize * GridSize;
        QVector<int> counts(cellCount, 0);

        for (int i = 0; i < positions.size(); ++i) {
            int cx = std::clamp(int((positions[i].x() - bounds.left()) / m_cellW), 0, GridSize - 1);
            int cy = std::clamp(int((positions[i].y() - bounds.top()) / m_cellH), 0, GridSize - 1);
            counts[cy * GridSize + cx]++;
        }

        // Compute offsets (prefix sum)
        m_cellRanges.resize(cellCount);
        int offset = 0;
        for (int c = 0; c < cellCount; ++c) {
            m_cellRanges[c] = {offset, counts[c]};
            offset += counts[c];
        }

        // Fill indices
        m_indices.resize(positions.size());
        QVector<int> writePos(cellCount);
        for (int c = 0; c < cellCount; ++c)
            writePos[c] = m_cellRanges[c].start;

        for (int i = 0; i < positions.size(); ++i) {
            int cx = std::clamp(int((positions[i].x() - bounds.left()) / m_cellW), 0, GridSize - 1);
            int cy = std::clamp(int((positions[i].y() - bounds.top()) / m_cellH), 0, GridSize - 1);
            int cell = cy * GridSize + cx;
            m_indices[writePos[cell]++] = i;
        }
    }

    /// Append indices of items in cells intersecting rect.
    void query(const QRectF &rect, QVector<int> &result) const
    {
        if (m_cellRanges.isEmpty())
            return;

        int x0 = std::clamp(int((rect.left() - m_bounds.left()) / m_cellW), 0, GridSize - 1);
        int y0 = std::clamp(int((rect.top() - m_bounds.top()) / m_cellH), 0, GridSize - 1);
        int x1 = std::clamp(int((rect.right() - m_bounds.left()) / m_cellW), 0, GridSize - 1);
        int y1 = std::clamp(int((rect.bottom() - m_bounds.top()) / m_cellH), 0, GridSize - 1);

        for (int y = y0; y <= y1; ++y) {
            for (int x = x0; x <= x1; ++x) {
                const auto &range = m_cellRanges[y * GridSize + x];
                for (int i = range.start; i < range.start + range.count; ++i)
                    result.append(m_indices[i]);
            }
        }
    }

    bool isEmpty() const { return m_indices.isEmpty(); }

private:
    struct CellRange {
        int start = 0;
        int count = 0;
    };

    QRectF m_bounds;
    double m_cellW = 1.0;
    double m_cellH = 1.0;
    QVector<CellRange> m_cellRanges;
    QVector<int> m_indices;
};

} // namespace ForceGraph
