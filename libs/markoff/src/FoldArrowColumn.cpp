// SPDX-License-Identifier: GPL-3.0-or-later
#include "GutterColumn.h"
#include "FoldingModel.h"
#include <QPainter>
#include <QPolygon>

namespace Markoff {

void FoldArrowColumn::paintCell(QPainter *p, const QRect &rect, int idx) {
    const auto &regions = m_model->regions();
    if (idx < 0 || idx >= regions.size()) return;
    const auto &r = regions[idx];
    const bool folded = m_model->isFolded(r.path);

    // 7px triangle centered in the 16px cell. Adapted from
    // ~/src/kde/src/ktexteditor/src/view/kateviewhelpers.cpp:2194.
    const QPoint c = rect.center();
    const int s = 3; // half-size
    QPolygon tri;
    if (folded) {
        // Rightward: closed fold. Base is on the left side of the cell;
        // shift base one pixel left so it lands clearly in the left quarter
        // of a 16-px cell (base at c.x()-s-1 = 3 for center=7).
        tri << QPoint(c.x() - s - 1, c.y() - s)
            << QPoint(c.x() - s - 1, c.y() + s)
            << QPoint(c.x() + s,     c.y());
    } else {
        // Downward: open fold.
        tri << QPoint(c.x() - s, c.y() - s)
            << QPoint(c.x() + s, c.y() - s)
            << QPoint(c.x(),     c.y() + s);
    }
    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);
    p->setPen(Qt::NoPen);
    p->setBrush(QColor(128, 128, 128));  // theme-aware in Task 11
    p->drawPolygon(tri);
    p->restore();
}

bool FoldArrowColumn::handleClick(QPoint, int idx, Qt::KeyboardModifiers mods)
{
    const auto &regions = m_model->regions();
    if (idx < 0 || idx >= regions.size()) return false;
    const auto &r = regions[idx];

    if (mods & Qt::ControlModifier) {
        if (r.type == FoldableRegion::Heading) {
            const int level = r.level;
            bool allFolded = true;
            for (const auto &h : regions) {
                if (h.type == FoldableRegion::Heading && h.level == level
                    && !m_model->isFolded(h.path)) { allFolded = false; break; }
            }
            if (allFolded) m_model->unfoldAllAtLevel(level);
            else m_model->foldAllAtLevel(level);
        } else { // CodeBlock
            if (r.path.size() <= 1) {
                // Preamble code block — fall back to individual toggle.
                m_model->toggle(r.path);
            } else {
                const FoldRegionKey sectionPath = r.path.mid(0, r.path.size() - 1);
                bool allFolded = true;
                for (const auto &cb : regions) {
                    if (cb.type != FoldableRegion::CodeBlock) continue;
                    if (cb.path.size() <= 1) continue;
                    if (cb.path.mid(0, cb.path.size() - 1) != sectionPath) continue;
                    if (!m_model->isFolded(cb.path)) { allFolded = false; break; }
                }
                if (allFolded) m_model->unfoldAllCodeBlocksInSection(sectionPath);
                else m_model->foldAllCodeBlocksInSection(sectionPath);
            }
        }
    } else {
        m_model->toggle(r.path);
    }
    return true;
}

} // namespace Markoff
