// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QGraphicsScene>
#include <QHash>

class QUndoStack;

namespace Canvas {

class CanvasDocument;
class CanvasTool;
class TextCardItem;
class GroupItem;
class EdgeItem;

class CanvasScene : public QGraphicsScene {
    Q_OBJECT

public:
    explicit CanvasScene(QObject *parent = nullptr);

    void setDocument(CanvasDocument *doc);

    // Tool management
    void setActiveTool(CanvasTool *tool);
    CanvasTool *activeTool() const;

    // Item lookup
    TextCardItem *textCardItem(const QString &id) const;
    GroupItem *groupItem(const QString &id) const;
    EdgeItem *edgeItem(const QString &id) const;

    // Undo
    QUndoStack *undoStack();

Q_SIGNALS:
    void cardDoubleClicked(const QString &nodeId);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override;

private:
    CanvasDocument *m_document = nullptr;
    CanvasTool *m_activeTool = nullptr;
    QUndoStack *m_undoStack = nullptr;
    QHash<QString, TextCardItem *> m_textCardItems;
    QHash<QString, GroupItem *> m_groupItems;
    QHash<QString, EdgeItem *> m_edgeItems;
};

} // namespace Canvas
