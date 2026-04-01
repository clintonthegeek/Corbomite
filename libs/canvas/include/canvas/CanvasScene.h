// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QGraphicsScene>
#include <QHash>
#include "CanvasTypes.h"

class QUndoStack;
class QGraphicsProxyWidget;
class QTextEdit;

namespace Canvas {

class CanvasDocument;
class CanvasTool;
class SelectMoveTool;
class ConnectableItem;
class TextCardItem;
class GroupItem;
class EdgeItem;

class CanvasScene : public QGraphicsScene {
    Q_OBJECT

public:
    explicit CanvasScene(QObject *parent = nullptr);

    void setDocument(CanvasDocument *doc);
    CanvasDocument *document() const;

    // Tool management
    void setActiveTool(CanvasTool *tool);
    CanvasTool *activeTool() const;

    // Item lookup
    TextCardItem *textCardItem(const QString &id) const;
    GroupItem *groupItem(const QString &id) const;
    EdgeItem *edgeItem(const QString &id) const;
    ConnectableItem *connectableItem(const QString &id) const;

    // Item management (used by tools and undo commands)
    TextCardItem *addTextCardItem(const CanvasNode &node);
    GroupItem *addGroupItemToScene(const CanvasNode &node);
    EdgeItem *addEdgeItemToScene(ConnectableItem *from, ConnectableItem *to, const CanvasEdge &edge);
    void removeTextCardItem(const QString &id);
    void removeGroupItem(const QString &id);
    void removeEdgeItem(const QString &id);

    // Editing state
    bool isEditing() const { return m_editProxy != nullptr; }

    // Undo
    QUndoStack *undoStack();

Q_SIGNALS:
    void cardDoubleClicked(const QString &nodeId);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override;

private Q_SLOTS:
    void onNodeAdded(const QString &id);
    void onNodeRemoved(const QString &id);
    void onNodeChanged(const QString &id);
    void onEdgeAdded(const QString &id);
    void onEdgeRemoved(const QString &id);
    void beginInlineEdit(TextCardItem *card);
    void beginGroupLabelEdit(GroupItem *group);
    void finishInlineEdit();
    void finishGroupLabelEdit();

private:
    void populateFromDocument();
    void clearAllItems();

    CanvasDocument *m_document = nullptr;
    CanvasTool *m_activeTool = nullptr;
    SelectMoveTool *m_defaultTool = nullptr;
    QUndoStack *m_undoStack = nullptr;
    QHash<QString, TextCardItem *> m_textCardItems;
    QHash<QString, GroupItem *> m_groupItems;
    QHash<QString, EdgeItem *> m_edgeItems;

    // Inline editing state
    QGraphicsProxyWidget *m_editProxy = nullptr;
    QTextEdit *m_editWidget = nullptr;
    QString m_editingNodeId;
    QMetaObject::Connection m_focusConnection;
    QGraphicsProxyWidget *m_labelEditProxy = nullptr;
    QString m_editingGroupId;
};

} // namespace Canvas
