// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QGraphicsScene>
#include <QHash>
#include <functional>
#include "CanvasTypes.h"

class QUndoStack;
class QGraphicsProxyWidget;
class QTextEdit;

namespace Corbomite {
class MarkdownRenderEngine;
}

namespace Canvas {

class CanvasDocument;
class CanvasTool;
class SelectMoveTool;
class ConnectableItem;
class TextCardItem;
class FileCardItem;
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

    // Render engine for file/text card rendering
    void setRenderEngine(Corbomite::MarkdownRenderEngine *engine);
    Corbomite::MarkdownRenderEngine *renderEngine() const;

    // File content resolver
    using FileResolver = std::function<QString(const QString &filePath)>;
    void setFileResolver(FileResolver resolver);

    // Item management (used by tools and undo commands)
    TextCardItem *addTextCardItem(const CanvasNode &node);
    FileCardItem *addFileCardItem(const CanvasNode &node);
    GroupItem *addGroupItemToScene(const CanvasNode &node);
    EdgeItem *addEdgeItemToScene(ConnectableItem *from, ConnectableItem *to, const CanvasEdge &edge);
    void removeTextCardItem(const QString &id);
    void removeFileCardItem(const QString &id);
    void removeGroupItem(const QString &id);
    void removeEdgeItem(const QString &id);
    FileCardItem *fileCardItem(const QString &id) const;

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
    void renderFileCard(FileCardItem *item);

    CanvasDocument *m_document = nullptr;
    CanvasTool *m_activeTool = nullptr;
    SelectMoveTool *m_defaultTool = nullptr;
    QUndoStack *m_undoStack = nullptr;
    QHash<QString, TextCardItem *> m_textCardItems;
    QHash<QString, FileCardItem *> m_fileCardItems;
    QHash<QString, GroupItem *> m_groupItems;
    QHash<QString, EdgeItem *> m_edgeItems;
    Corbomite::MarkdownRenderEngine *m_renderEngine = nullptr;
    FileResolver m_fileResolver;

    // Inline editing state
    QGraphicsProxyWidget *m_editProxy = nullptr;
    QTextEdit *m_editWidget = nullptr;
    QString m_editingNodeId;
    QMetaObject::Connection m_focusConnection;
    QGraphicsProxyWidget *m_labelEditProxy = nullptr;
    QString m_editingGroupId;
};

} // namespace Canvas
