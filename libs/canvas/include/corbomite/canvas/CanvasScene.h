// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <graffodil/GraphScene.h>
#include <QHash>

#include <corbomite/canvas/CanvasPalette.h>
#include <QImage>
#include <QPointF>
#include <QRectF>
#include <QVector>
#include <functional>
#include <memory>
#include "CanvasTypes.h"

class QIODevice;
class QMenu;
class QUndoStack;
class QUndoCommand;
class QGraphicsProxyWidget;
class QGraphicsSceneDragDropEvent;


namespace Markoff::Canvas {
class EditorWidget;
} // namespace Markoff::Canvas

namespace Corbomite {
class MarkdownRenderEngine;
struct OpenDocument;
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

namespace Corbomite::Canvas {

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

    // ---- canvas dark mode ----
    //
    // One palette for the whole scene, so a paint() can never mix a
    // half-updated set of colours during a theme change. setCanvasPalette
    // also drives the scene's background brush, which is what
    // renderToImage/renderToSvg read — so PNG/SVG export follows the theme
    // for free, which matters because a canvas authored dark must export
    // dark.
    void setCanvasPalette(const CanvasPalette &palette);
    const CanvasPalette &canvasPalette() const { return m_palette; }

    /// Convenience for the theme wiring: true when the host says the app is
    /// dark. Kept as a bool rather than taking a colour so libs/canvas stays
    /// free of any ThemeService/KColorScheme dependency.
    void applyDarkTheme(bool dark);

    /// Build the canvas context menu for `scenePos` into `menu`
    /// without showing it. Split out of contextMenuEvent() so the
    /// read-only lock's menu filtering is reachable from a test without
    /// driving QMenu::exec()'s nested event loop — the same rationale as
    /// createFileCardViaPicker() and addCardConnectedTo() above. Requires a
    /// document; entries mutate the document through the undo stack when
    /// triggered.
    void populateContextMenu(QMenu *menu, const QPointF &scenePos);

    // Item lookup (typed convenience wrappers over GraphScene::nodeForId/edgeForId)
    TextCardItem *textCardItem(const QString &id) const;
    GroupItem *groupItem(const QString &id) const;
    EdgeItem *edgeItem(const QString &id) const;
    CanvasNodeItem *connectableItem(const QString &id) const;

    // Render engine for file/text card rendering
    void setRenderEngine(Corbomite::MarkdownRenderEngine *engine);
    Corbomite::MarkdownRenderEngine *renderEngine() const;

    /// Vault root and vault-relative `.canvas` path used as Markoff session
    /// key hints. An empty canvas path leaves text-card hints empty, which
    /// is what unit tests do. The card editor opens its document under the
    /// same hint the paint uses.
    void setSessionContext(const QString &vaultRoot, const QString &canvasRelativePath);

    // File content resolver
    using FileResolver = std::function<QString(const QString &filePath)>;
    void setFileResolver(FileResolver resolver);

    // Distinguishes "file missing" from "file exists but is empty" for
    // the file-card placeholder. When unset, an empty resolver result is
    // treated as missing (the earlier behaviour).
    using FileExists = std::function<bool(const QString &filePath)>;
    void setFileExistsChecker(FileExists checker);

    /// Unlike the resolver, this reports whether a file node's target exists.
    bool fileCardResolves(const QString &filePath) const;

    // The three missing-file placeholder button actions. Public and
    // separately callable (same rationale as createFileCardViaPicker()) so
    // tests can exercise them without synthesising mouse events. Each returns
    // true when it acted.
    bool createMissingFileCard(const QString &nodeId);
    bool locateMissingFileCard(const QString &nodeId);
    bool removeMissingFileCard(const QString &nodeId);

    /// Auto-resolve: re-render every file card whose `file` equals
    /// `filePath` (vault create/rename), or every file card when `filePath`
    /// is empty.
    void refreshFileCards(const QString &filePath = QString());

    // File content saver
    using FileSaver = std::function<void(const QString &filePath, const QString &content)>;
    void setFileSaver(FileSaver saver);

    // File-card creation via a modal picker. The real app wires this
    // to open a CanvasFilePickerDialog (vault fuzzy file-suggest); returns
    // the chosen path (vault-relative) or an empty string if cancelled.
    // Kept as an injectable callback so tests can supply a fixed result
    // without driving a real modal dialog.
    using FilePickerRequestor = std::function<QString()>;
    void setFilePickerRequestor(FilePickerRequestor requestor);

    /// Read-only lock. Mirrors Obsidian's `Canvas.readonly`
    /// (`app/own/2018-k7.js:81`, toggled by `Canvas.setReadonly` at `:1395`):
    /// per-canvas-file, machine-local, and gating everything except
    /// pan/zoom/select. The flag lives here rather than in CanvasDocument
    /// because it must never reach a `.canvas` file or `workspace.json` --
    /// CanvasViewTab persists it through the tier-3 store instead.
    ///
    /// Turning it on commits and closes any in-progress inline edit and
    /// abandons an in-flight node drag, so text typed before the lock cannot
    /// be committed after it.
    bool isReadOnly() const { return m_readOnly; }
    void setReadOnly(bool readOnly);

    /// undoStack() stays raw so undo commands can be pushed from anywhere;
    /// these are the read-only-aware answers, and are what the KAction layer
    /// (CanvasFileView::canUndo/canRedo) and CanvasView's Ctrl+Z/Y use.
    /// Obsidian does NOT gate undo behind the lock -- its control-cluster
    /// undo/redo buttons (`app/own/2018-k7.js:239-254`) and `Canvas.undo`
    /// (`:481`) carry no readonly check -- but an undo against a locked canvas
    /// is a mutation, so we close that hole.
    bool canUndo() const;
    bool canRedo() const;

    /// Programmatic entry point mirroring the "New file card…" context-menu
    /// action: invokes the file-picker requestor and, if a path is
    /// returned, pushes a CmdAddCard for a 400x400 file node (Appendix A
    /// default) at scenePos, storing the path vault-relative.
    void createFileCardViaPicker(const QPointF &scenePos);

    /// Testable core of the drop-on-empty create-and-connect menu:
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

    /// Testable core of the Direction submenu: pushes a CmdSetEdgeEnds
    /// that sets `edgeId`'s fromEnd/toEnd to the given pair. Public and
    /// separately callable — same rationale as addCardConnectedTo() above —
    /// so tests can exercise it without driving QMenu::exec()'s nested event
    /// loop. No-op if the edge doesn't exist.
    void setEdgeEnds(const QString &edgeId, EndType fromEnd, EndType toEnd);

    /// Shared "build a reverse command for one edge id" logic, used by
    /// both the context-menu "Reverse Direction" action and the R-key
    /// onReverseRequested() handler (and directly by tests, same rationale
    /// as setEdgeEnds()/addCardConnectedTo() above). If `parent` is
    /// non-null, the CmdReverseEdge is parented under it (compound step)
    /// rather than pushed on the undo stack directly.
    void reverseEdge(const QString &edgeId, QUndoCommand *parent = nullptr);

    // Resolves an absolute filesystem path (from a text/uri-list
    // drag-drop) to a vault-relative path, or returns an empty string if
    // the path is outside the vault (rejected: this scope has no
    // copy-into-vault). If unset, absolute paths are stored as-is
    // (test / no-vault convenience) — the real app always sets one.
    using VaultPathResolver = std::function<QString(const QString &absoluteFilePath)>;
    void setVaultPathResolver(VaultPathResolver resolver);

    // --- "Convert to File…" (Obsidian TextCardItem.convertToFile) ---
    //
    // The operation spans a modal prompt and a libs/vault FileManager write,
    // neither of which libs/canvas may own, so both are seams — same shape
    // and same rationale as setFilePickerRequestor() above.
    /// Asks for the new note's name. Empty string means cancelled.
    using FileNameRequestor = std::function<QString()>;
    void setFileNameRequestor(FileNameRequestor requestor);
    /// Writes `content` as a markdown file named `name`, returning its
    /// vault-relative path, or empty string on failure.
    using MarkdownFileCreator = std::function<QString(const QString &name, const QString &content)>;
    void setMarkdownFileCreator(MarkdownFileCreator creator);

    /// The node id of the selected text card when the selection is exactly
    /// one text node, else empty. Obsidian's `canvas:convert-to-file`
    /// checkCallback (`app/own/2028-n9.js:682-692`) gates the command on
    /// precisely that, so this is the action's Tier-B capability probe.
    QString selectedTextNodeId() const;

    /// Writes the card's text to a new markdown file and swaps the node's
    /// type/file for it, keeping the node's geometry and rebinding every
    /// edge onto the new node — one undo step for the whole conversion.
    /// Returns the new file node's id, or empty when the prompt was
    /// cancelled, the write failed, `nodeId` is not a text node, or the
    /// canvas is locked.
    QString convertTextCardToFile(const QString &nodeId);

    /// One entry of the "Jump to group" candidate list.
    struct GroupJumpTarget {
        QString nodeId;
        QString label;
        QRectF bounds;
    };

    /// The canvas's groups as jump targets: document (paint) order filtered
    /// to `type == "group"`, each label falling back to i18n("Untitled
    /// group"). Mirrors Obsidian's `l9.getItems`/`getItemText`
    /// (`app/own/2028-n9.js:305-313`), which filters the document's node
    /// array rather than the live scene.
    QVector<GroupJumpTarget> groupJumpTargets() const;

    // Clipboard. serializeSelectionAsCanvasJson()/pasteCanvasJsonOrText()
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

    /// True while a plain select/move node drag (Graffodil
    /// SelectMoveTool's dragBegan..dragEnded window) is in progress. Used
    /// by CanvasView to decide whether to run edge auto-pan. Scoped to
    /// node moves only for a first pass (not resize drags) — see
    /// CanvasView.cpp's auto-pan comment for why.
    bool isDragActive() const { return m_dragActive; }

    /// Render the scene's `bounds` region to an image.
    /// `scale` multiplies the output pixel dimensions (2.0 yields a HiDPI
    /// raster). `transparentBg` swaps the scene's background brush for
    /// transparent. `showEdges` hides `EdgeItem`s during render (useful for
    /// "just the nodes" exports).
    QImage renderToImage(const QRectF &bounds,
                          bool transparentBg = false,
                          bool showEdges = true,
                          qreal scale = 2.0);

    /// Render the scene's `bounds` region to SVG via
    /// `out` (caller owns the device). Same `transparentBg` + `showEdges`
    /// semantics as `renderToImage`.
    void renderToSvg(const QRectF &bounds,
                      QIODevice *out,
                      bool transparentBg = false,
                      bool showEdges = true);

    /// Testing/introspection accessor for the shared resize/
    /// connection-point chrome overlay (single overlay retargeted to the
    /// active selected/hovered node — see CanvasNodeChromeOverlay).
    CanvasNodeChromeOverlay *chromeOverlay() const { return m_chromeOverlay; }

    /// Exposes the snap-to-grid/snap-to-object
    /// toggles for CanvasViewActions. Was private with no accessor.
    CanvasAlignmentStrategy *alignmentStrategy() const { return m_alignmentStrategy; }

Q_SIGNALS:
    void cardDoubleClicked(const QString &nodeId);
    /// CanvasViewTab listens to persist the flag to tier 3.
    void readOnlyChanged(bool readOnly);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override;

    /// Double-click on empty canvas creates a new text card
    /// (250x60, centered on the click point), selects it, and immediately
    /// begins inline-edit (card is "born" in edit mode).
    void mouseDoubleClickEventBackground(const QPointF &scenePos) override;

    // Drag-drop node creation. setAcceptDrops(true) is required on
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

    // --- undo wiring (Graffodil tool intent signals -> Cmd*) ---
    void onDragBegan(const QList<Graffodil::IGraphNode *> &nodes);
    void onDragEnded(const QList<Graffodil::IGraphNode *> &nodes);
    void onDeleteRequested(const QList<Graffodil::IGraphNode *> &nodes,
                            const QList<Graffodil::IGraphEdge *> &edges);
    void onResizeCommitted(const QString &nodeId, const QRect &oldRect, const QRect &newRect);

    // --- edge creation gesture (Graffodil::CreateEdgeTool -> CmdAddEdge) ---
    void onEdgeRequested(Graffodil::IGraphNode *source, const QString &sourceAnchorId,
                          Graffodil::IGraphNode *target, const QString &targetAnchorId);

    // --- drop-on-empty -> create-and-connect menu ---
    void onEdgeDroppedOnEmpty(Graffodil::IGraphNode *source, const QString &sourceAnchorId,
                               const QPointF &scenePos);

    // --- endpoint reconnect (ReconnectEdgeTool -> CmdReconnectEdge / CmdRemoveEdge) ---
    void onReconnectRequested(const QString &edgeId, Graffodil::ArrowEnd end,
                               Graffodil::IGraphNode *newNode, const QString &newAnchorId);
    void onReconnectDroppedOnEmpty(const QString &edgeId);

    // --- direction menu / R-key reverse ---
    void onReverseRequested(const QList<Graffodil::IGraphEdge *> &edges);

    // --- chrome overlay retargeting ---
    /// Connected to QGraphicsScene::selectionChanged() (a real built-in Qt
    /// signal, no new plumbing needed) and to each node's hoverChanged().
    void updateActiveChromeTarget();

private:
    void populateFromDocument();
    void clearAllItems();
    void renderFileCard(FileCardItem *item);
    void reRenderAllCards();
    void addColorSubmenu(QMenu *parentMenu, const QString &nodeId, const QString &currentColor);
    /// Re-apply m_dragSnapshot's positions, called from mouseMoveEvent
    /// after SelectMoveTool has moved the drag set. SelectMoveTool fuses
    /// selection and move into one gesture and its drag state is private (and
    /// it lives in the graffodil submodule), so this is the only way to keep
    /// "selection still works, movement does not" without forking it: the
    /// items follow the cursor and are snapped straight back each event.
    void restoreDragSnapshot();
    /// Wires the hover/geometry signals shared by every
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
    /// The sole hovered node, or nullptr. Tracked via each node's
    /// hoverChanged() signal; feeds updateActiveChromeTarget()'s
    /// selection-empty fallback.
    CanvasNodeItem *m_hoveredNode = nullptr;

    QUndoStack *m_undoStack = nullptr;

    /// Canvas dark mode. Held by value so items read a
    /// stable snapshot; never null, never half-updated.
    CanvasPalette m_palette;
    Corbomite::MarkdownRenderEngine *m_renderEngine = nullptr;
    QString m_vaultRoot;
    QString m_canvasRelativePath;
    FileResolver m_fileResolver;
    FileExists m_fileExists;
    FileSaver m_fileSaver;
    FilePickerRequestor m_filePickerRequestor;
    FileNameRequestor m_fileNameRequestor;
    MarkdownFileCreator m_markdownFileCreator;
    VaultPathResolver m_vaultPathResolver;
    void beginFileCardEdit(FileCardItem *card);
    void finishFileCardEdit();
    QString m_editingFileCardId;

    // Inline editing state
    QGraphicsProxyWidget *m_editProxy = nullptr;
    /// The card editor: Markoff's canvas leaf in embedded (card) mode. It
    /// is a window onto the session document below — the leaf never owns
    /// the text, the document does.
    Markoff::Canvas::EditorWidget *m_editWidget = nullptr;
    /// The session document the card editor is bound to. It owns the
    /// Markoff document, so it must outlive the editor: the editor holds a
    /// raw pointer to it.
    std::unique_ptr<Corbomite::OpenDocument> m_editDocument;
    QString m_editingNodeId;
    QMetaObject::Connection m_focusConnection;
    QGraphicsProxyWidget *m_labelEditProxy = nullptr;
    QString m_editingGroupId;

    // Move-drag undo snapshot (dragBegan -> dragEnded)
    QHash<QString, QPointF> m_dragSnapshot;
    // Mirrors whether we're between onDragBegan/onDragEnded, for
    // CanvasView's edge auto-pan.
    bool m_dragActive = false;
    // Groups whose drag-capture was started this gesture (so
    // onDragEnded can symmetrically end capture on exactly the ones begun).
    QVector<GroupItem *> m_capturedGroups;

    // --- read-only lock ---
    bool m_readOnly = false;
    /// True between onDragBegan and onDragEnded while m_dragSnapshot should
    /// be re-applied after every move event. Only ever set while m_readOnly.
    bool m_restoreDragPositions = false;
};

} // namespace Corbomite::Canvas
