// SPDX-License-Identifier: GPL-3.0-or-later
#include "canvas/CanvasView.h"
#include "canvas/CanvasAlignmentStrategy.h"
#include "canvas/CanvasScene.h"
#include "canvas/CanvasDocument.h"

#include <QApplication>
#include <QClipboard>
#include <QGraphicsItem>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPointer>
#include <QScrollBar>
#include <QTimer>
#include <QUndoStack>

#include <algorithm>
#include <cmath>

// Canvas dark mode — the item's scene palette, or the
// light default when the item has no scene (unit tests, detached items). The
// fallback matters: a `paint()` that returned a default-constructed
// CanvasPalette would silently render light, which is exactly the class of
// bug this change exists to remove.
// `static` (internal linkage) rather than a namespace-scope non-static
// function: this helper is replicated in three translation units, so a plain
// definition would be a duplicate symbol at link time.
static const Canvas::CanvasPalette &canvasPaletteFor(const QGraphicsScene *scene)
{
    static const Canvas::CanvasPalette kLight = Canvas::CanvasPalette::light();
    const auto *cs = qobject_cast<const Canvas::CanvasScene *>(scene);
    return cs ? cs->canvasPalette() : kLight;
}

namespace Canvas {



static constexpr double kZoomFactor = 1.15;

// Minimum on-screen spacing (device pixels) a drawn grid line/dot pitch must
// clear before we stop thinning it further. CanvasAlignmentStrategy::
// gridSpacingForScale()'s own 20/40/80/160 ladder already coarsens the pitch
// down to scale 0.25; below that (the zoom clamp -- Appendix A log2 scale
// in [-4,1], i.e. scale >= 0.0625 -- hasn't landed yet, so zoomOut() has no
// floor today) this keeps doubling the pitch until it's visually sane again,
// rather than the dots merging into noise at extreme zoom-out.
static constexpr double kMinOnScreenGridSpacing = 8.0;

// Edge auto-pan (Appendix A: Obsidian target ~60Hz near the viewport
// wrapper's edge). Margin/step aren't in Appendix A's normative table
// (unspecified by the audit) — picked as plausible, "keep simple" values.
static constexpr int kAutoPanMargin = 30;     // px from viewport edge that triggers auto-pan
static constexpr int kAutoPanStep = 12;       // scroll-bar units per tick, toward the near edge
static constexpr int kAutoPanIntervalMs = 16; // ~60Hz

CanvasView::CanvasView(QWidget *parent)
    : QGraphicsView(parent)
{
    m_scene = new CanvasScene(this);
    setScene(m_scene);

    setRenderHint(QPainter::Antialiasing);
    setDragMode(QGraphicsView::NoDrag); // Tools handle drag
    setTransformationAnchor(AnchorUnderMouse);
    setViewportUpdateMode(SmartViewportUpdate);
    // Required so QGraphicsView forwards drag/drop events to the
    // scene's dragEnterEvent/dragMoveEvent/dropEvent.
    setAcceptDrops(true);

    // Forward cardDoubleClicked from scene
    connect(m_scene, &CanvasScene::cardDoubleClicked,
            this, &CanvasView::cardDoubleClicked);
}

void CanvasView::setDocument(CanvasDocument *doc)
{
    m_document = doc;
    m_scene->setDocument(doc);
}

CanvasDocument *CanvasView::document() const
{
    return m_document;
}

void CanvasView::zoomToFit()
{
    if (!scene() || scene()->items().isEmpty())
        return;
    fitInView(scene()->itemsBoundingRect().adjusted(-50, -50, 50, 50),
              Qt::KeepAspectRatio);
}

QJsonObject CanvasView::viewState() const
{
    if (!scene() || !viewport())
        return {};
    const double s = transform().m11();
    if (!(s > 0.0))
        return {};
    // Obsidian stores the target, so a save mid-gesture persists the
    // destination. QGraphicsView has no separate target, so we report the
    // live transform; a wheel gesture is instantaneous by the time the
    // workspace is written, which is where the two agree.
    const QPointF centre = mapToScene(viewport()->rect().center());
    return QJsonObject{
        {QStringLiteral("x"), centre.x()},
        {QStringLiteral("y"), centre.y()},
        {QStringLiteral("zoom"), std::log2(s)},
    };
}

void CanvasView::ensureWorldCovers(double x, double y, double scale)
{
    if (!(scale > 0.0))
        return;
    const double halfW = viewport()->width() / (2.0 * scale) * kRestoreWorldViewports;
    const double halfH = viewport()->height() / (2.0 * scale) * kRestoreWorldViewports;
    QRectF world = scene()->sceneRect();
    world.setLeft(std::min(world.left(), x - halfW));
    world.setRight(std::max(world.right(), x + halfW));
    world.setTop(std::min(world.top(), y - halfH));
    world.setBottom(std::max(world.bottom(), y + halfH));
    scene()->setSceneRect(world);
}

void CanvasView::setViewState(const QJsonObject &state)
{
    if (!scene() || !viewport())
        return;
    const QJsonValue vx = state.value(QStringLiteral("x"));
    const QJsonValue vy = state.value(QStringLiteral("y"));
    const QJsonValue vz = state.value(QStringLiteral("zoom"));
    if (!vx.isDouble() || !vy.isDouble() || !vz.isDouble())
        return;

    const double x = vx.toDouble();
    const double y = vy.toDouble();
    const double scaleFactor =
        std::pow(2.0, std::clamp(vz.toDouble(), kMinZoomLog2, kMaxZoomLog2));

    // Obsidian's canvas has no world bounds -- x/y may be any finite value,
    // which is why a persisted viewport routinely sits in empty space away
    // from every node. QGraphicsView derives its scrollbar range from
    // sceneRect, and an unset sceneRect means itemsBoundingRect(), so without
    // this the restore silently clamps to the content edge and the pan is
    // lost.
    ensureWorldCovers(x, y, scaleFactor);

    // AnchorUnderMouse (the gesture default, set in the ctor) makes scale()
    // re-anchor around the cursor, which would fight the explicit centring
    // below. AnchorViewCenter scales about the view centre instead.
    const ViewportAnchor savedAnchor = transformationAnchor();
    setTransformationAnchor(AnchorViewCenter);
    resetTransform();
    scale(scaleFactor, scaleFactor);
    centreScenePoint(x, y);

    // Widening the world can bring a scrollbar into existence, and Qt only
    // materialises one on the next layout pass -- which shrinks the viewport
    // and slides the scene point at its centre by half the scrollbar inset.
    // Re-centre once the frame has settled, or the restore lands up to half a
    // scrollbar off. Deferred rather than a nested event loop, which would be
    // re-entrant; guarded by QPointer because a leaf can be torn down inside
    // that window.
    QPointer<CanvasView> self(this);
    QTimer::singleShot(0, this, [self, x, y] {
        if (self)
            self->centreScenePoint(x, y);
    });

    setTransformationAnchor(savedAnchor);
}

void CanvasView::centreScenePoint(double x, double y)
{
    // Scrollbar values are in device pixels, so a shortfall measured in scene
    // units converts back by the scale. Setting the value directly (rather
    // than re-issuing centreOn with a corrected request) keeps each pass
    // monotonic, so this converges instead of oscillating around the
    // quantisation step.
    for (int pass = 0; pass < 4; ++pass) {
        const double s = transform().m11();
        const QPointF here = mapToScene(viewport()->rect().center());
        const int dx = qRound((x - here.x()) * s);
        const int dy = qRound((y - here.y()) * s);
        if (dx == 0 && dy == 0)
            return;
        QScrollBar *hs = horizontalScrollBar();
        QScrollBar *vs = verticalScrollBar();
        hs->setValue(hs->value() + dx);
        vs->setValue(vs->value() + dy);
    }
}

void CanvasView::zoomIn()
{
    scale(kZoomFactor, kZoomFactor);
}

void CanvasView::zoomOut()
{
    scale(1.0 / kZoomFactor, 1.0 / kZoomFactor);
}

void CanvasView::zoomToSelection()
{
    if (!scene()) return;
    const auto selected = scene()->selectedItems();
    if (selected.isEmpty()) return;
    QRectF bounds;
    for (auto *item : selected)
        bounds = bounds.united(item->sceneBoundingRect());
    fitInView(bounds.adjusted(-50, -50, 50, 50), Qt::KeepAspectRatio);
}

void CanvasView::zoomToBbox(const QRectF &bbox)
{
    if (!scene() || !viewport())
        return;
    const double vw = viewport()->width();
    const double vh = viewport()->height();
    if (!(vw > 0.0) || !(vh > 0.0) || !(bbox.width() > 0.0) || !(bbox.height() > 0.0))
        return;

    // app/own/2018-k7.js:573-577 -- fit with a 1.1 margin, clamp the log2
    // scale to [-4, 1] (the same clamp requestFrame() re-applies every
    // frame at :821), then centre on the bbox.
    const double fit = std::min(vw / (1.1 * bbox.width()), vh / (1.1 * bbox.height()));
    const double scaleFactor =
        std::pow(2.0, std::clamp(std::log2(fit), kMinZoomLog2, kMaxZoomLog2));

    const QPointF centre = bbox.center();
    // A group can be smaller than the viewport at the clamped scale, which
    // leaves Qt with no scroll range to centre against.
    ensureWorldCovers(centre.x(), centre.y(), scaleFactor);

    // AnchorUnderMouse (the gesture default) would scale about the cursor and
    // fight the explicit centring; AnchorViewCenter scales about the centre.
    const ViewportAnchor savedAnchor = transformationAnchor();
    setTransformationAnchor(AnchorViewCenter);
    resetTransform();
    scale(scaleFactor, scaleFactor);
    centreScenePoint(centre.x(), centre.y());
    setTransformationAnchor(savedAnchor);

    QPointer<CanvasView> self(this);
    QTimer::singleShot(0, this, [self, centre] {
        if (self)
            self->centreScenePoint(centre.x(), centre.y());
    });
}

void CanvasView::setGridVisible(bool visible)
{
    if (m_gridVisible == visible) return;
    m_gridVisible = visible;
    viewport()->update();
}

void CanvasView::keyPressEvent(QKeyEvent *event)
{
    const bool ctrl = event->modifiers() & Qt::ControlModifier;
    const bool shift = event->modifiers() & Qt::ShiftModifier;

    if (ctrl && event->key() == Qt::Key_Z && !shift) {
        // Ctrl+Z: undo. Gated on the scene's read-only-aware canUndo().
        if (m_scene->canUndo())
            m_scene->undoStack()->undo();
        return;
    }

    if ((ctrl && event->key() == Qt::Key_Y) ||
        (ctrl && shift && event->key() == Qt::Key_Z)) {
        // Ctrl+Y or Ctrl+Shift+Z: redo
        if (m_scene->canRedo())
            m_scene->undoStack()->redo();
        return;
    }

    // Clipboard. Note: when the inline-edit QTextEdit proxy has focus,
    // OS-level keyboard focus is on that child widget directly, so these
    // never fire during in-place text editing (same reasoning as the
    // Ctrl+Z/Y handling above).
    if (ctrl && event->key() == Qt::Key_C && !shift) {
        m_scene->copySelectionToClipboard();
        return;
    }
    if (ctrl && event->key() == Qt::Key_X && !shift) {
        m_scene->cutSelectionToClipboard();
        return;
    }
    if (ctrl && event->key() == Qt::Key_V && !shift) {
        const QString text = QGuiApplication::clipboard()->text();
        const QPointF center = mapToScene(viewport()->rect().center());
        m_scene->pasteCanvasJsonOrText(text, center);
        return;
    }

    switch (event->key()) {
    case Qt::Key_Home:
        zoomToFit();
        return;
    case Qt::Key_Plus:
    case Qt::Key_Equal:
        zoomIn();
        return;
    case Qt::Key_Minus:
        zoomOut();
        return;
    default:
        break;
    }

    // Delegate other keys to scene (tools handle Delete, arrows, Ctrl+A)
    QGraphicsView::keyPressEvent(event);
}

void CanvasView::mouseMoveEvent(QMouseEvent *event)
{
    QGraphicsView::mouseMoveEvent(event);
    m_lastViewportPos = event->pos();
    updateAutoPan();
}

void CanvasView::updateAutoPan()
{
    // Scoped to CanvasScene::isDragActive() — set/cleared around a plain
    // node-move drag (Graffodil SelectMoveTool's dragBegan/dragEnded) —
    // rather than also covering CanvasResizeTool drags. A first pass:
    // resize handles are dragged right at a node's own edge, not typically
    // toward the far side of the viewport, so leaving resize out keeps
    // this mechanism to one drag-state flag per the plan's "keep simple"
    // steer. Wiring CanvasResizeTool's press/release into the same flag
    // is a reasonable follow-up if resize-near-edge turns out to want it.
    if (!m_scene || !m_scene->isDragActive()) {
        if (m_autoPanTimer)
            m_autoPanTimer->stop();
        return;
    }

    const QRect vp = viewport()->rect();
    const bool nearEdge =
        m_lastViewportPos.x() < kAutoPanMargin ||
        m_lastViewportPos.x() > vp.width() - kAutoPanMargin ||
        m_lastViewportPos.y() < kAutoPanMargin ||
        m_lastViewportPos.y() > vp.height() - kAutoPanMargin;

    if (!nearEdge) {
        if (m_autoPanTimer)
            m_autoPanTimer->stop();
        return;
    }

    if (!m_autoPanTimer) {
        m_autoPanTimer = new QTimer(this);
        m_autoPanTimer->setInterval(kAutoPanIntervalMs);
        connect(m_autoPanTimer, &QTimer::timeout, this, &CanvasView::autoPanTick);
    }
    if (!m_autoPanTimer->isActive())
        m_autoPanTimer->start();
}

void CanvasView::autoPanTick()
{
    if (!m_scene || !m_scene->isDragActive()) {
        if (m_autoPanTimer)
            m_autoPanTimer->stop();
        return;
    }

    const QRect vp = viewport()->rect();
    int dx = 0, dy = 0;
    if (m_lastViewportPos.x() < kAutoPanMargin)
        dx = -kAutoPanStep;
    else if (m_lastViewportPos.x() > vp.width() - kAutoPanMargin)
        dx = kAutoPanStep;
    if (m_lastViewportPos.y() < kAutoPanMargin)
        dy = -kAutoPanStep;
    else if (m_lastViewportPos.y() > vp.height() - kAutoPanMargin)
        dy = kAutoPanStep;

    if (dx == 0 && dy == 0) {
        // Cursor drifted back away from the edge without a mouseMoveEvent
        // catching it (e.g. focus/geometry change) — nothing to do.
        m_autoPanTimer->stop();
        return;
    }

    if (dx != 0)
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() + dx);
    if (dy != 0)
        verticalScrollBar()->setValue(verticalScrollBar()->value() + dy);

    // Re-deliver a mouse-move at the SAME viewport-local position through
    // the normal event path (QApplication::sendEvent on the viewport, the
    // same path a real OS mouse-move takes to reach this class's own
    // mouseMoveEvent override / the scene's tool dispatch) so
    // SelectMoveTool keeps extending the drag: scenePos shifts because the
    // view just scrolled underneath an otherwise-unmoved cursor.
    QMouseEvent synthetic(QEvent::MouseMove, m_lastViewportPos,
                           viewport()->mapToGlobal(m_lastViewportPos),
                           Qt::NoButton, QApplication::mouseButtons(),
                           QApplication::keyboardModifiers());
    QApplication::sendEvent(viewport(), &synthetic);
}

void CanvasView::drawBackground(QPainter *painter, const QRectF &rect)
{
    // Fill background with white
    painter->fillRect(rect, canvasPaletteFor(scene()).background);

    if (!m_gridVisible)
        return;

    // Zoom-adaptive dotted grid. Reuses the exact same pitch ladder
    // CanvasAlignmentStrategy uses for grid-SNAP (20/40/80/160 scene units
    // by zoom), so the visible grid and the invisible snap magnetism never
    // drift apart -- then keeps thinning beyond that ladder's own floor for
    // extreme zoom-out (see kMinOnScreenGridSpacing's comment), so the dots
    // never collapse into visual noise regardless of how far out the view
    // is zoomed.
    const double scale = transform().m11();
    double pitch = CanvasAlignmentStrategy::gridSpacingForScale(scale);
    // Defensive cap on the loop: scale can't be zero/negative in practice
    // (QGraphicsView transforms are never singular here), but guard against
    // a runaway loop if it ever were.
    int guard = 0;
    while (pitch * scale < kMinOnScreenGridSpacing && guard++ < 32)
        pitch *= 2.0;

    // Cosmetic pen (width 0) always renders as exactly 1 device pixel
    // regardless of the view's zoom transform -- the dots stay a constant
    // on-screen size instead of shrinking to sub-pixel/invisible at
    // zoom-out or growing at zoom-in.
    painter->setPen(QPen(canvasPaletteFor(scene()).gridDot, 0));

    const double left = std::floor(rect.left() / pitch) * pitch;
    const double top = std::floor(rect.top() / pitch) * pitch;

    for (double x = left; x < rect.right(); x += pitch) {
        for (double y = top; y < rect.bottom(); y += pitch) {
            painter->drawPoint(QPointF(x, y));
        }
    }
}

} // namespace Canvas
