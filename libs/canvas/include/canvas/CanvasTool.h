// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>

class QGraphicsSceneMouseEvent;

namespace Canvas {

class CanvasScene;

class CanvasTool : public QObject {
    Q_OBJECT

public:
    explicit CanvasTool(CanvasScene *scene, QObject *parent = nullptr);

    virtual void mousePressEvent(QGraphicsSceneMouseEvent *event) = 0;
    virtual void mouseMoveEvent(QGraphicsSceneMouseEvent *event) = 0;
    virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) = 0;
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
};

} // namespace Canvas
