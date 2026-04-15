// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/readingview/ReadingView.h"

#include <QGraphicsScene>

namespace Corbomite::ReadingView {

ReadingView::ReadingView(QWidget *parent)
    : QGraphicsView(parent)
{
    auto *scene = new QGraphicsScene(this);
    setScene(scene);
    setFrameShape(QFrame::NoFrame);
    setAlignment(Qt::AlignLeft | Qt::AlignTop);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    // Phase 3 (Cluster E) will replace this empty scene with a
    // VirtualScrollController-driven mount pool.
}

ReadingView::~ReadingView() = default;

void ReadingView::setPlainText(const QString & /*text*/)
{
    // TODO(Cluster E Phase 3): parse via libs/markoff-parser, split into
    // ReadingSection boundaries, feed the ReadingPipeline.
}

float ReadingView::scrollPositionVisualLine() const
{
    // TODO(Cluster E Phase 3): compute from mounted-section geometry + the
    // pre-layout line offsets of unmounted sections above. The Phase 2
    // NoteEditorWidget wiring already treats this as authoritative, so when
    // Phase 3 lands no caller needs to change.
    return 0.0f;
}

void ReadingView::setScrollPositionVisualLine(float /*visualLine*/)
{
    // TODO(Cluster E Phase 3): seek to the section/offset that matches the
    // target visual-line, mounting as necessary. Emit
    // scrollPositionVisualLineChanged when the VirtualScrollController
    // settles at the new position.
}

} // namespace Corbomite::ReadingView
