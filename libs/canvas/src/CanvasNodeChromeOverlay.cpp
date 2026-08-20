// SPDX-License-Identifier: GPL-3.0-or-later
#include "canvas/CanvasNodeChromeOverlay.h"
#include "canvas/CanvasNodeItem.h"

#include <graffodil/Types.h>

#include <QGraphicsScene>
#include <QGraphicsView>
#include <QPainter>
#include <QStyleOptionGraphicsItem>

namespace Canvas {

static constexpr qreal kHandleSize = 6.0;
static constexpr qreal kDotRadius = 4.0;

CanvasNodeChromeOverlay::CanvasNodeChromeOverlay(QGraphicsItem *parent)
    : QGraphicsObject(parent)
{
    // Zoom-constant screen-pixel geometry — see header comment.
    setFlag(ItemIgnoresTransformations, true);
    // Pure visual chrome: never intercept mouse/hover so it can't shadow
    // the real node items' resize/anchor hit-testing underneath it.
    setAcceptedMouseButtons(Qt::NoButton);
    setAcceptHoverEvents(false);
    // Above every node/edge (GraphScene::DefaultNodeZ is 0.0).
    setZValue(1000.0);
    setVisible(false);
}

void CanvasNodeChromeOverlay::retarget(CanvasNodeItem *node, bool showHandles, bool showDots)
{
    m_target = node;
    m_showHandles = showHandles;
    m_showDots = showDots;

    if (!m_target || (!m_showHandles && !m_showDots)) {
        setVisible(false);
        return;
    }

    setVisible(true);
    syncToTarget();
}

void CanvasNodeChromeOverlay::clear()
{
    m_target = nullptr;
    m_showHandles = false;
    m_showDots = false;
    setVisible(false);
}

void CanvasNodeChromeOverlay::syncToTarget()
{
    if (!m_target)
        return;

    prepareGeometryChange();
    setPos(m_target->sceneBoundingRect().topLeft());
    update();
}

qreal CanvasNodeChromeOverlay::currentScale() const
{
    if (!scene() || scene()->views().isEmpty())
        return 1.0;
    return scene()->views().first()->transform().m11();
}

QList<QPointF> CanvasNodeChromeOverlay::connectionDotScenePositions() const
{
    QList<QPointF> result;
    if (!m_target)
        return result;
    const auto anchors = m_target->anchors();
    result.reserve(anchors.size());
    for (const auto &anchor : anchors)
        result.append(anchor.scenePos);
    return result;
}

QRectF CanvasNodeChromeOverlay::boundingRect() const
{
    if (!m_target)
        return QRectF();

    const qreal scale = currentScale();
    const QRectF r = m_target->sceneBoundingRect();
    const qreal w = r.width() * scale;
    const qreal h = r.height() * scale;
    const qreal pad = kHandleSize + kDotRadius + 4.0;
    return QRectF(-pad, -pad, w + 2.0 * pad, h + 2.0 * pad);
}

void CanvasNodeChromeOverlay::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);

    if (!m_target)
        return;

    painter->setRenderHint(QPainter::Antialiasing);

    const qreal scale = currentScale();
    const QRectF r = m_target->sceneBoundingRect();
    const qreal w = r.width() * scale;
    const qreal h = r.height() * scale;

    if (m_showHandles) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(58, 134, 255));

        const qreal hs = kHandleSize;
        const qreal hh = hs / 2.0;
        const QPointF corners[8] = {
            {0, 0}, {w, 0}, {w, h}, {0, h},           // TL, TR, BR, BL
            {w / 2.0, 0}, {w, h / 2.0}, {w / 2.0, h}, {0, h / 2.0}, // Top, Right, Bottom, Left
        };
        for (const auto &c : corners)
            painter->drawRect(QRectF(c.x() - hh, c.y() - hh, hs, hs));
    }

    if (m_showDots) {
        painter->setPen(QPen(QColor(58, 134, 255), 1.5));
        painter->setBrush(QColor(255, 255, 255));

        const QPointF origin = r.topLeft();
        for (const auto &pt : connectionDotScenePositions()) {
            const QPointF local = (pt - origin) * scale;
            painter->drawEllipse(local, kDotRadius, kDotRadius);
        }
    }
}

} // namespace Canvas
