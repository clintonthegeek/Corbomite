// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <graffodil/GraphScene.h>
#include <QHash>
#include <QImage>
#include <QPointF>
#include <QRectF>
#include <QVector>
#include <functional>
#include "CanvasTypes.h"

class QIODevice;
class QMenu;
class QUndoStack;
class QUndoCommand;
class QGraphicsProxyWidget;
class QGraphicsSceneDragDropEvent;
class QTextEdit;

namespace Corbomite {
class MarkdownRenderEngine;
}

namespace Graffodil {
class CompositeTool;
class SelectMoveTool;
class PanZoomTool;
class CreateEdgeTool;
class IGraphNode;
class IGraphEdge;
enum class ArrowEnd;
}

namespace Canvas {

class CanvasDocument;
class CanvasNodeItem;
class TextCardItem;
class FileCardItem;
class GroupItem;
class EdgeItem;
class CanvasResizeTool;
class CanvasDuplicateDragTool;
class CanvasEdgeGestureTool;
class ReconnectEdgeTool;
class CanvasAlignmentStrategy;
class CanvasNodeChromeOverlay;

class CanvasScene : public Graffodil::GraphScene {
    Q_OBJECT

public:
    explicit CanvasScene(QObject *parent = nullptr);
    ~CanvasScene() override;

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

    /// M3.3 testable core of the drop-on-empty create-and-connect menu:
    /// pushes one compound undo command (CmdAddCard + CmdAddEdge, parented
    /// under a single QUndoCommand) that adds `node` and connects it to
    /// `source` from `sourceAnchorId`. `node`'s id/type/geometry must
    /// already be filled in by the caller (menu action or test); toSide is
    /// computed here via pickSideToward() so it faces `source`. Public and
    /// separately callable — same rationale as createFileCardViaPicker()
    /// above — so tests can exercise the compound-command logic without
    /// driving QMenu::exec()'s nested event loop.
    void addCardConnectedTo(CanvasNode node, Graffodil::IGraphNode *source,
                             const QString &sourceAnchorId);

    /// M3.5 testable core of the Direction submenu: pushes a CmdSetEdgeEnds
    /// that sets `edgeId`'s fromEnd/toEnd to the given pair. Public and
    /// separately callable — same rationale as addCardConnectedTo() above —
    /// so tests can exercise it without driving QMenu::exec()'s nested event
    /// loop. No-op if the edge doesn't exist.
    void setEdgeEnds(const QString &edgeId, EndType fromEnd, EndType toEnd);

    /// M3.5 shared "build a reverse command for one edge id" logic, used by
    /// both the context-menu "Reverse Direction" action and the R-key
    /// onReverseRequested() handler (and directly by tests, same rationale
    /// as setEdgeEnds()/addCardConnectedTo() above). If `parent` is
    /// non-null, the CmdReverseEdge is parented under it (compound step)
    /// rather than pushed on the undo stack directly.
    void reverseEdge(const QString &edgeId, QUndoCommand *parent = nullptr);

    // M2.3 — resolves an absolute filesystem path (from a text/uri-list
    // drag-drop) to a vault-relative path, or returns an empty string if
    // the path is outside the vault (rejected: M1/M2 scope has no
    // copy-into-vault). If unset, absolute paths are stored as-is
    // (test / no-vault convenience) — the real app always sets one.
    using VaultPathResolver = std::function<QString(const QString &absoluteFilePath)>;
    void setVaultPathResolver(VaultPathResolver resolver);

    // M2.4 — clipboard. serializeSelectionAsCanvasJson()/pasteCanvasJsonOrText()
    // are pure logic (no QClipboard access) so tests can round-trip them
    // directly; copySelectionToClipboard()/cutSelectionToClipboard() are the
    // real-app convenience wrappers that touch QGuiApplication::clipboard().

    /// `.canvas`-shaped JSON (`{"nodes":[...],"edges":[...]}`) for the
    /// current selection — this is literally what Obsidian puts on the
    /// system clipboard, enabling cross-app paste. An edge is included
    /// only when both its endpoints are selected. Empty string if nothing
    /// is selected.
    QString serializeSelectionAsCanvasJson() const;

    /// If `clipboardText` parses as canvas JSON, clones every node/edge
    /// with a fresh 16-hex id (edge endpoints remapped), offsets position
    /// +16px, and pushes one compound undo command. Otherwise, if
    /// non-empty, creates a new text card centered on `pasteCenterScenePos`
    /// (typically the viewport center).
    void pasteCanvasJsonOrText(const QString &clipboardText, const QPointF &pasteCenterScenePos);

    void copySelectionToClipboard();
    /// Copy + delete-compound (reuses the same delete path as the Delete key).
    void cutSelectionToClipboard();

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

    /// M4.2 — true while a plain select/move node drag (Graffodil
    /// SelectMoveTool's dragBegan..dragEnded window) is in progress. Used
    /// by CanvasView to decide whether to run edge auto-pan. Scoped to
    /// node moves only for a first pass (not resize drags) — see
    /// CanvasView.cpp's auto-pan comment for why.
    bool isDragActive() const { return m_dragActive; }

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

    /// M4.4 — testing/introspection accessor for the shared resize/
    /// connection-point chrome overlay (single overlay retargeted to the
    /// active selected/hovered node — see CanvasNodeChromeOverlay).
    CanvasNodeChromeOverlay *chromeOverlay() const { return m_chromeOverlay; }

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

    // M2.3 — drag-drop node creation. setAcceptDrops(true) is required on
    // the owning CanvasView; QGraphicsView forwards translated drag/drop
    // events to these QGraphicsScene virtuals automatically.
    void dragEnterEvent(QGraphicsSceneDragDropEvent *event) override;
    void dragMoveEvent(QGraphicsSceneDragDropEvent *event) override;
    void dropEvent(QGraphicsSceneDragDropEvent *event) override;

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

    // --- M3.2 edge creation gesture (Graffodil::CreateEdgeTool -> CmdAddEdge) ---
    void onEdgeRequested(Graffodil::IGraphNode *source, const QString &sourceAnchorId,
                          Graffodil::IGraphNode *target, const QString &targetAnchorId);

    // --- M3.3 drop-on-empty -> create-and-connect menu ---
    void onEdgeDroppedOnEmpty(Graffodil::IGraphNode *source, const QString &sourceAnchorId,
                               const QPointF &scenePos);

    // --- M3.4 endpoint reconnect (ReconnectEdgeTool -> CmdReconnectEdge / CmdRemoveEdge) ---
    void onReconnectRequested(const QString &edgeId, Graffodil::ArrowEnd end,
                               Graffodil::IGraphNode *newNode, const QString &newAnchorId);
    void onReconnectDroppedOnEmpty(const QString &edgeId);

    // --- M3.5 direction menu / R-key reverse ---
    void onReverseRequested(const QList<Graffodil::IGraphEdge *> &edges);

    // --- M4.4 chrome overlay retargeting ---
    /// Connected to QGraphicsScene::selectionChanged() (a real built-in Qt
    /// signal, no new plumbing needed) and to each node's hoverChanged().
    void updateActiveChromeTarget();

private:
    void populateFromDocument();
    void clearAllItems();
    void renderFileCard(FileCardItem *item);
    void reRenderAllCards();
    void addColorSubmenu(QMenu *parentMenu, const QString &nodeId, const QString &currentColor);
    /// M4.4 — wires the hover/geometry signals shared by every
    /// CanvasNodeItem subclass; called once from each add*Item() factory
    /// (mirrors the per-subclass editRequested() connect() calls already
    /// there).
    void wireNodeChromeSignals(CanvasNodeItem *item);

    CanvasDocument *m_document = nullptr;

    // Tool assembly (spec §3.5 / §6a V3 — bespoke CompositeTool, NOT
    // DefaultGraphTool: DefaultGraphTool pre-registers its own routes at
    // construction time, leaving no way to prepend the resize route ahead
    // of plain-left-button select/move).
    Graffodil::CompositeTool *m_compositeTool = nullptr;
    Graffodil::SelectMoveTool *m_selectTool = nullptr;
    Graffodil::PanZoomTool *m_panZoomTool = nullptr;
    CanvasResizeTool *m_resizeTool = nullptr;
    CanvasDuplicateDragTool *m_duplicateDragTool = nullptr;
    Graffodil::CreateEdgeTool *m_createEdgeTool = nullptr;
    ReconnectEdgeTool *m_reconnectTool = nullptr;
    CanvasEdgeGestureTool *m_edgeGestureTool = nullptr;
    CanvasAlignmentStrategy *m_alignmentStrategy = nullptr;
    CanvasNodeChromeOverlay *m_chromeOverlay = nullptr;
    /// M4.4 — the sole hovered node, or nullptr. Tracked via each node's
    /// hoverChanged() signal; feeds updateActiveChromeTarget()'s
    /// selection-empty fallback.
    CanvasNodeItem *m_hoveredNode = nullptr;

    QUndoStack *m_undoStack = nullptr;
    Corbomite::MarkdownRenderEngine *m_renderEngine = nullptr;
    FileResolver m_fileResolver;
    FileSaver m_fileSaver;
    FilePickerRequestor m_filePickerRequestor;
    VaultPathResolver m_vaultPathResolver;
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
    // M4.2 — mirrors whether we're between onDragBegan/onDragEnded, for
    // CanvasView's edge auto-pan.
    bool m_dragActive = false;
    // M4.3 — groups whose drag-capture was started this gesture (so
    // onDragEnded can symmetrically end capture on exactly the ones begun).
    QVector<GroupItem *> m_capturedGroups;
};

} // namespace Canvas
