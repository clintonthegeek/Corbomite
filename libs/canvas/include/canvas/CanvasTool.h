// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QPointF>
#include <QRectF>
#include <QHash>
#include <QGraphicsItem>
#include "CanvasTypes.h"

class QGraphicsSceneMouseEvent;
class QGraphicsRectItem;
class QGraphicsLineItem;
class QKeyEvent;

namespace Canvas {

class CanvasScene;
class TextCardItem;
class GroupItem;

class CanvasTool : public QObject {
    Q_OBJECT

public:
    explicit CanvasTool(CanvasScene *scene, QObject *parent = nullptr);
    virtual ~CanvasTool() = default;

    virtual void mousePressEvent(QGraphicsSceneMouseEvent *event) = 0;
    virtual void mouseMoveEvent(QGraphicsSceneMouseEvent *event) = 0;
    virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) = 0;
    virtual void keyPressEvent(QKeyEvent *event);
    virtual void activate() {}
    virtual void deactivate() {}

protected:
    CanvasScene *m_scene = nullptr;
};

class SelectMoveTool : public CanvasTool {
    Q_OBJECT

public:
    explicit SelectMoveTool(CanvasScene *scene, QObject *parent = nullptr);

    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void deactivate() override;

private:
    enum class DragMode { None, Move, Resize, RubberBand };
    DragMode m_dragMode = DragMode::None;

    // Move state
    QPointF m_pressScenePos;
    QHash<QGraphicsItem *, QPointF> m_initialPositions;

    // Resize state
    int m_resizeMode = 0; // TextCardItem::ResizeMode / GroupItem::ResizeMode
    QGraphicsItem *m_resizeItem = nullptr;
    QRectF m_resizeOriginalRect;
    QPointF m_resizeOriginalPos;

    // Rubber-band state
    QGraphicsRectItem *m_rubberBand = nullptr;
    QPointF m_rubberBandOrigin;
};

class CreateCardTool : public CanvasTool {
    Q_OBJECT

public:
    explicit CreateCardTool(CanvasScene *scene, QObject *parent = nullptr);

    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
};

class CreateEdgeTool : public CanvasTool {
    Q_OBJECT

public:
    explicit CreateEdgeTool(CanvasScene *scene, QObject *parent = nullptr);

    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void deactivate() override;

private:
    struct NearCardResult {
        TextCardItem *card = nullptr;
        Side side = Side::Right;
    };
    NearCardResult findNearCard(const QPointF &scenePos, qreal threshold = 15.0) const;

    TextCardItem *m_sourceCard = nullptr;
    Side m_fromSide = Side::Right;
    QGraphicsLineItem *m_previewLine = nullptr;
};

} // namespace Canvas
