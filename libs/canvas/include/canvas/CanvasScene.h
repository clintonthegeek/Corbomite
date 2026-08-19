// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <graffodil/GraphScene.h>
#include <QHash>
#include <QImage>
#include <QPointF>
#include <QRectF>
#include <functional>
#include "CanvasTypes.h"

class QIODevice;
class QMenu;
class QUndoStack;
class QGraphicsProxyWidget;
class QTextEdit;

namespace Corbomite {
class MarkdownRenderEngine;
}

namespace Graffodil {
class CompositeTool;
class SelectMoveTool;
class PanZoomTool;
class IGraphNode;
class IGraphEdge;
}

namespace Canvas {

class CanvasDocument;
class CanvasNodeItem;
class TextCardItem;
class FileCardItem;
class GroupItem;
class EdgeItem;
class CanvasResizeTool;

class CanvasScene : public Graffodil::GraphScene {
    Q_OBJECT

public:
    explicit CanvasScene(QObject *parent = nullptr);

    void setDocument(CanvasDocument *doc);
    CanvasDocument *document() const;

    // Item lookup (typed convenience wrappers over GraphScene::nodeForId/edgeForId)
    TextCardItem *textCardItem(const QString &id) const;
    GroupItem *groupItem(const QString &id) const;
    EdgeItem *edgeItem(const QString &id) const;
    CanvasNodeItem *connectableItem(const QString &id) const;

    // Render engine for file/text card rendering
    void setRenderEngine(Corbomite::MarkdownRenderEngine *engine);
    Corbomite::MarkdownRenderEngine *renderEngine() const;

    // File content resolver
    using FileResolver = std::function<QString(const QString &filePath)>;
    void setFileResolver(FileResolver resolver);

    // File content saver
    using FileSaver = std::function<void(const QString &filePath, const QString &content)>;
    void setFileSaver(FileSaver saver);

    // M2.2 — file-card creation via a modal picker. The real app wires this
    // to open a CanvasFilePickerDialog (vault fuzzy file-suggest); returns
    // the chosen path (vault-relative) or an empty string if cancelled.
    // Kept as an injectable callback so tests can supply a fixed result
    // without driving a real modal dialog.
    using FilePickerRequestor = std::function<QString()>;
    void setFilePickerRequestor(FilePickerRequestor requestor);

    /// Programmatic entry point mirroring the "New file card…" context-menu
    /// action: invokes the file-picker requestor and, if a path is
    /// returned, pushes a CmdAddCard for a 400x400 file node (Appendix A
    /// default) at scenePos, storing the path vault-relative.
    void createFileCardViaPicker(const QPointF &scenePos);

    // Item management (used by tools and undo commands)
    TextCardItem *addTextCardItem(const CanvasNode &node);
    FileCardItem *addFileCardItem(const CanvasNode &node);
    GroupItem *addGroupItemToScene(const CanvasNode &node);
    EdgeItem *addEdgeItemToScene(CanvasNodeItem *from, CanvasNodeItem *to, const CanvasEdge &edge);
    void removeTextCardItem(const QString &id);
    void removeFileCardItem(const QString &id);
    void removeGroupItem(const QString &id);
    void removeEdgeItem(const QString &id);
    FileCardItem *fileCardItem(const QString &id) const;

    // Editing state
    bool isEditing() const { return m_editProxy != nullptr; }

    // Undo
    QUndoStack *undoStack();

    /// Cluster R Task 3.5 — render the scene's `bounds` region to an image.
    /// `scale` multiplies the output pixel dimensions (2.0 yields a HiDPI
    /// raster). `transparentBg` swaps the scene's background brush for
    /// transparent. `showEdges` hides `EdgeItem`s during render (useful for
    /// "just the nodes" exports).
    QImage renderToImage(const QRectF &bounds,
                          bool transparentBg = false,
                          bool showEdges = true,
                          qreal scale = 2.0);

    /// Cluster R Task 3.5 — render the scene's `bounds` region to SVG via
    /// `out` (caller owns the device). Same `transparentBg` + `showEdges`
    /// semantics as `renderToImage`.
    void renderToSvg(const QRectF &bounds,
                      QIODevice *out,
                      bool transparentBg = false,
                      bool showEdges = true);

Q_SIGNALS:
    void cardDoubleClicked(const QString &nodeId);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override;

    /// M2.1 — double-click on empty canvas creates a new text card
    /// (250x60, centered on the click point), selects it, and immediately
    /// begins inline-edit (card is "born" in edit mode).
    void mouseDoubleClickEventBackground(const QPointF &scenePos) override;

private Q_SLOTS:
    void onNodeAdded(const QString &id);
    void onNodeRemoved(const QString &id);
    void onNodeChanged(const QString &id);
    void onEdgeAdded(const QString &id);
    void onEdgeRemoved(const QString &id);
    void onEdgeChanged(const QString &id);
    void beginInlineEdit(TextCardItem *card);
    void beginGroupLabelEdit(GroupItem *group);
    void finishInlineEdit();
    void finishGroupLabelEdit();

    // --- M1.5 undo wiring (Graffodil tool intent signals -> Cmd*) ---
    void onDragBegan(const QList<Graffodil::IGraphNode *> &nodes);
    void onDragEnded(const QList<Graffodil::IGraphNode *> &nodes);
    void onDeleteRequested(const QList<Graffodil::IGraphNode *> &nodes,
                            const QList<Graffodil::IGraphEdge *> &edges);
    void onResizeCommitted(const QString &nodeId, const QRect &oldRect, const QRect &newRect);

private:
    void populateFromDocument();
    void clearAllItems();
    void renderFileCard(FileCardItem *item);
    void reRenderAllCards();
    void addColorSubmenu(QMenu *parentMenu, const QString &nodeId, const QString &currentColor);

    CanvasDocument *m_document = nullptr;

    // Tool assembly (spec §3.5 / §6a V3 — bespoke CompositeTool, NOT
    // DefaultGraphTool: DefaultGraphTool pre-registers its own routes at
    // construction time, leaving no way to prepend the resize route ahead
    // of plain-left-button select/move).
    Graffodil::CompositeTool *m_compositeTool = nullptr;
    Graffodil::SelectMoveTool *m_selectTool = nullptr;
    Graffodil::PanZoomTool *m_panZoomTool = nullptr;
    CanvasResizeTool *m_resizeTool = nullptr;

    QUndoStack *m_undoStack = nullptr;
    Corbomite::MarkdownRenderEngine *m_renderEngine = nullptr;
    FileResolver m_fileResolver;
    FileSaver m_fileSaver;
    FilePickerRequestor m_filePickerRequestor;
    void beginFileCardEdit(FileCardItem *card);
    void finishFileCardEdit();
    QString m_editingFileCardId;

    // Inline editing state
    QGraphicsProxyWidget *m_editProxy = nullptr;
    QTextEdit *m_editWidget = nullptr;
    QString m_editingNodeId;
    QMetaObject::Connection m_focusConnection;
    QGraphicsProxyWidget *m_labelEditProxy = nullptr;
    QString m_editingGroupId;

    // Move-drag undo snapshot (dragBegan -> dragEnded)
    QHash<QString, QPointF> m_dragSnapshot;
};

} // namespace Canvas
