// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QGraphicsView>

namespace Canvas {

class CanvasDocument;
class CanvasScene;

class CanvasView : public QGraphicsView {
    Q_OBJECT

public:
    explicit CanvasView(QWidget *parent = nullptr);

    void setDocument(CanvasDocument *doc);
    CanvasDocument *document() const;
    CanvasScene *canvasScene() const { return m_scene; }
    void zoomToFit();
    void zoomIn();
    void zoomOut();

Q_SIGNALS:
    void cardDoubleClicked(const QString &nodeId);
    void selectionChanged(const QStringList &selectedIds);

protected:
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void drawBackground(QPainter *painter, const QRectF &rect) override;

private:
    CanvasScene *m_scene = nullptr;
    CanvasDocument *m_document = nullptr;
    bool m_panning = false;
    QPoint m_lastPanPos;
};

} // namespace Canvas
