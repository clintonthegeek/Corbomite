// SPDX-License-Identifier: GPL-3.0-or-later
#include "canvas/CanvasScene.h"
#include "canvas/CanvasCommands.h"
#include "canvas/CanvasDocument.h"
#include "canvas/CanvasAlignmentStrategy.h"
#include "canvas/CanvasNodeChromeOverlay.h"
#include "canvas/CanvasDuplicateDragTool.h"
#include "canvas/CanvasEdgeGestureTool.h"
#include "canvas/CanvasNodeItem.h"
#include "canvas/CanvasResizeTool.h"
#include "canvas/ReconnectEdgeTool.h"
#include "canvas/TextCardItem.h"
#include "canvas/FileCardItem.h"
#include "canvas/GroupItem.h"
#include "canvas/EdgeItem.h"
#include "corbomite/core/MarkdownRenderEngine.h"
#include "corbomite/core/RenderOptions.h"

#include <graffodil/CompositeTool.h>
#include <graffodil/SelectMoveTool.h>
#include <graffodil/PanZoomTool.h>
#include <graffodil/CreateEdgeTool.h>
#include <graffodil/IGraphNode.h>
#include <graffodil/IGraphEdge.h>

#include <QApplication>
#include <QClipboard>
#include <QCursor>
#include <QGraphicsProxyWidget>
#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneDragDropEvent>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsView>
#include <QGuiApplication>
#include <QIODevice>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QKeyEvent>
#include <QLineEdit>
#include <QList>
#include <QMenu>
#include <QMimeData>
#include <QPainter>
#include <QSet>
#include <QSvgGenerator>
#include <utility>
#include <QTextEdit>
#include <QUndoStack>
#include <QtMath>
#include <KLocalizedString>

namespace Canvas {

CanvasScene::CanvasScene(QObject *parent)
    : Graffodil::GraphScene(parent)
    , m_undoStack(new QUndoStack(this))
{
    // CanvasView paints white via drawBackground() (view-level, not part of
    // the scene). renderToImage/renderToSvg render straight from the scene
    // and fall back to QGraphicsScene::backgroundBrush()'s default
    // (Qt::NoBrush, .color() == black) when it's unset — that's what was
    // producing black export backgrounds while the app showed white. Match
    // the view's fill here so both paths agree. Full theme/dark-mode
    // awareness for canvas is not wired anywhere yet — punch-listed.
    setBackgroundBrush(Qt::white);
    // Bespoke CompositeTool assembly — see spec §6a V3: Graffodil::
    // DefaultGraphTool pre-registers its own select/pan routes at
    // construction time, so a resize route added afterward would always
    // lose to its plain-left-button select route. Building our own
    // CompositeTool from parts lets the resize predicate go first.
    m_selectTool = new Graffodil::SelectMoveTool(this);
    // M4.1 — grid + object snap (both default ON) plus M4.2's Shift
    // axis-lock, both hooked into the one align() callback SelectMoveTool
    // consults per mouseMove during a drag.
    m_alignmentStrategy = new CanvasAlignmentStrategy(this);
    m_selectTool->setAlignmentStrategy(m_alignmentStrategy);
    m_panZoomTool = new Graffodil::PanZoomTool(this);
    m_resizeTool = new CanvasResizeTool(this);
    m_duplicateDragTool = new CanvasDuplicateDragTool(this);
    m_createEdgeTool = new Graffodil::CreateEdgeTool(this);
    // Obsidian-strict: releasing without a drag cancels edge creation
    // rather than falling into click-click mode (plan M3.2).
    m_createEdgeTool->setDragOnly(true);
    // M3.4: 12.0 is CreateEdgeTool's own default anchor-hover radius, but
    // set it explicitly now that it's routed via m_edgeGestureTool instead
    // of being addAnchorRoute()'s direct target — that call used to
    // auto-sync this via its `qobject_cast<CreateEdgeTool*>` special case,
    // which no longer fires once the wrapper is what's registered.
    m_createEdgeTool->setAnchorHoverRadius(12.0);
    m_reconnectTool = new ReconnectEdgeTool(this);
    // M3.4 dispatcher: decides per-press between reconnecting an existing
    // edge's endpoint and creating a new edge — see CanvasEdgeGestureTool's
    // header comment for why this can't be expressed as two ordinary
    // CompositeTool routes.
    m_edgeGestureTool = new CanvasEdgeGestureTool(m_createEdgeTool, m_reconnectTool, this);
    m_compositeTool = new Graffodil::CompositeTool(this);

    // M3.2/M3.4 — route left-button presses within the anchor hover radius
    // to the edge-gesture dispatcher (create vs. reconnect). addAnchorRoute()
    // always prepends, so this wins over every route added below regardless
    // of call order — but it's placed first anyway for readability. This IS
    // load-bearing versus the resize route: CanvasNodeItem::resizeModeAtPos()
    // treats an entire selected edge (not just its corners) as a resize zone
    // (kResizeZone=8 from either the left/right or top/bottom face), so a
    // press at a selected node's face-midpoint anchor is simultaneously a
    // valid resize-edge hit and an anchor hit. Anchor creation/reconnect
    // should win there (more specific, more intentional gesture) — verified
    // via findResizeTarget() in CanvasResizeTool.cpp before relying on
    // addAnchorRoute()'s prepend.
    m_compositeTool->addAnchorRoute(m_edgeGestureTool, 12.0);

    // Same bindings as Graffodil::DefaultGraphTool, with the resize route
    // prepended ahead of select/move.
    m_compositeTool->addMouseRoute(m_resizeTool, [this](QGraphicsSceneMouseEvent *ev) {
        return ev->button() == Qt::LeftButton
            && findResizeTarget(this, ev->scenePos()) != nullptr;
    });
    // M2.5 — Alt-drag duplicate: also tried ahead of select/move. Alt isn't
    // matched by any of the plain/Shift/Ctrl/Meta select routes below, so
    // ordering isn't strictly load-bearing here, but this mirrors the
    // resize route's placement per the plan's "tried before the normal
    // select/move route" instruction.
    m_compositeTool->addMouseRoute(m_duplicateDragTool, [this](QGraphicsSceneMouseEvent *ev) {
        return ev->button() == Qt::LeftButton
            && (ev->modifiers() & Qt::AltModifier)
            && findAltDragDuplicateTarget(this, ev->scenePos()) != nullptr;
    });
    m_compositeTool->addMouseRoute(m_selectTool,
        Graffodil::CompositeTool::matchButton(Qt::LeftButton, Qt::NoModifier));
    m_compositeTool->addMouseRoute(m_selectTool,
        Graffodil::CompositeTool::matchButton(Qt::LeftButton, Qt::ShiftModifier));
    m_compositeTool->addMouseRoute(m_selectTool,
        Graffodil::CompositeTool::matchButton(Qt::LeftButton, Qt::ControlModifier));
    m_compositeTool->addMouseRoute(m_selectTool,
        Graffodil::CompositeTool::matchButton(Qt::LeftButton, Qt::MetaModifier));

    m_panZoomTool->setPanButton(Qt::MiddleButton);
    m_panZoomTool->setZoomButton(Qt::NoButton);
    // Deliberately NOT overriding setZoomWheelModifier(): PanZoomTool's own
    // default (Qt::ControlModifier) is what gives us spec §7 item 2 — bare
    // wheel scrolls (falls through unaccepted to QGraphicsView's native
    // scrollbar handling), Ctrl+wheel zooms about the cursor. NOTE:
    // Graffodil::DefaultGraphTool itself sets NoModifier ("plain wheel
    // zooms"), which contradicts both its own comment and this spec's
    // stated intent — one more reason (besides V3's route-order issue) this
    // scene builds its own CompositeTool instead of using DefaultGraphTool.
    m_compositeTool->addMouseRoute(m_panZoomTool,
        Graffodil::CompositeTool::matchButton(Qt::MiddleButton));
    m_compositeTool->addWheelRoute(m_panZoomTool, Graffodil::CompositeTool::matchAnyWheel());

    m_compositeTool->addKeyRoute(m_selectTool);

    setActiveTool(m_compositeTool);

    // M1.5 undo wiring: tool intent signals -> Cmd* on the undo stack.
    // Tools never touch CanvasDocument directly (plan Appendix B rule 1).
    connect(m_selectTool, &Graffodil::SelectMoveTool::dragBegan,
            this, &CanvasScene::onDragBegan);
    connect(m_selectTool, &Graffodil::SelectMoveTool::dragEnded,
            this, &CanvasScene::onDragEnded);
    connect(m_selectTool, &Graffodil::SelectMoveTool::deleteRequested,
            this, &CanvasScene::onDeleteRequested);
    // M3.5 — R-key reverse, wired through the same CmdReverseEdge logic as
    // the "Reverse Direction" context-menu action (spec §3.6).
    connect(m_selectTool, &Graffodil::SelectMoveTool::reverseRequested,
            this, &CanvasScene::onReverseRequested);
    connect(m_resizeTool, &CanvasResizeTool::resizeCommitted,
            this, &CanvasScene::onResizeCommitted);
    connect(m_createEdgeTool, &Graffodil::CreateEdgeTool::edgeRequested,
            this, &CanvasScene::onEdgeRequested);
    // M3.3 — drop-on-empty create-and-connect menu.
    connect(m_createEdgeTool, &Graffodil::CreateEdgeTool::edgeDroppedOnEmpty,
            this, &CanvasScene::onEdgeDroppedOnEmpty);
    // M3.4 — endpoint reconnect.
    connect(m_reconnectTool, &ReconnectEdgeTool::reconnectRequested,
            this, &CanvasScene::onReconnectRequested);
    connect(m_reconnectTool, &ReconnectEdgeTool::reconnectDroppedOnEmpty,
            this, &CanvasScene::onReconnectDroppedOnEmpty);

    // M4.4 — shared resize/connection-point chrome overlay. Not a graph
    // node/edge (doesn't go through addNode()/addEdge()), just a plain
    // scene item added directly, matching "one overlay retargeted to the
    // active node" from the plan rather than per-node children.
    m_chromeOverlay = new CanvasNodeChromeOverlay();
    addItem(m_chromeOverlay);
    connect(this, &QGraphicsScene::selectionChanged, this, &CanvasScene::updateActiveChromeTarget);
}

CanvasScene::~CanvasScene()
{
    // M4.4 — disconnect the chrome-overlay's selectionChanged wiring before
    // any teardown starts. QGraphicsScene's base destructor (which runs
    // AFTER this body returns) deletes any items still in the scene, and
    // deleting a selected item emits selectionChanged() synchronously; by
    // that point this object's dynamic type has decayed to QGraphicsScene
    // (its ~CanvasScene() body has already finished), so a still-live
    // direct connection invoking updateActiveChromeTarget() trips Qt's
    // assertObjectType ("object not of the correct type"). Disconnecting
    // here removes the connection well before item teardown ever reaches
    // that point — same rationale as the finishInlineEdit() precaution
    // below for m_focusConnection.
    disconnect(this, &QGraphicsScene::selectionChanged, this, &CanvasScene::updateActiveChromeTarget);

    // Commit/tear down any in-progress inline edit BEFORE
    // QGraphicsScene::~QGraphicsScene() starts deleting items. Deleting a
    // focused QTextEdit/QLineEdit synchronously fires
    // QApplication::focusChanged, which the edit-start code connects back
    // to this scene (m_focusConnection) to auto-finish on focus loss — if
    // that connection is still live when the base-class destructor deletes
    // the widget out from under it, the signal re-enters
    // finishInlineEdit()/finishGroupLabelEdit() and double-removes/deletes
    // an item that's already mid-teardown (heap corruption). Finishing
    // explicitly here, while the scene is still fully intact, disconnects
    // m_focusConnection as a side effect and leaves nothing for the base
    // destructor to race with.
    finishInlineEdit();
    finishFileCardEdit();
    finishGroupLabelEdit();
}

// ---------------------------------------------------------------------------
// Document management
// ---------------------------------------------------------------------------

void CanvasScene::setDocument(CanvasDocument *doc)
{
    // Disconnect old document
    if (m_document) {
        disconnect(m_document, nullptr, this, nullptr);
    }

    m_document = doc;

    clearAllItems();

    if (!m_document)
        return;

    // Populate scene from document
    populateFromDocument();

    // Connect document signals for live sync
    connect(m_document, &CanvasDocument::nodeAdded, this, &CanvasScene::onNodeAdded);
    connect(m_document, &CanvasDocument::nodeRemoved, this, &CanvasScene::onNodeRemoved);
    connect(m_document, &CanvasDocument::nodeChanged, this, &CanvasScene::onNodeChanged);
    connect(m_document, &CanvasDocument::edgeAdded, this, &CanvasScene::onEdgeAdded);
    connect(m_document, &CanvasDocument::edgeRemoved, this, &CanvasScene::onEdgeRemoved);
    connect(m_document, &CanvasDocument::edgeChanged, this, &CanvasScene::onEdgeChanged);
}

CanvasDocument *CanvasScene::document() const
{
    return m_document;
}

void CanvasScene::populateFromDocument()
{
    if (!m_document)
        return;

    // Create items for all nodes
    const auto nodes = m_document->nodes();
    for (const auto &node : nodes) {
        if (node.type == NodeType::Group) {
            addGroupItemToScene(node);
        } else if (node.type == NodeType::Text) {
            addTextCardItem(node);
        } else if (node.type == NodeType::File) {
            addFileCardItem(node);
        }
    }

    // Create items for all edges
    const auto edges = m_document->edges();
    for (const auto &edge : edges) {
        auto *from = connectableItem(edge.fromNode);
        auto *to = connectableItem(edge.toNode);
        if (from && to) {
            addEdgeItemToScene(from, to, edge);
        }
    }
}

void CanvasScene::clearAllItems()
{
    // Finish any inline edits
    finishInlineEdit();
    finishFileCardEdit();
    finishGroupLabelEdit();

    // M4.4 — every node about to be deleted below; drop any dangling
    // references the chrome overlay / hover tracking would otherwise hold.
    m_hoveredNode = nullptr;
    if (m_chromeOverlay)
        m_chromeOverlay->clear();

    // GraphScene::clearGraph() removes items from the scene registry (and
    // the QGraphicsScene) but does not delete them — we still own them.
    const auto cleared = clearGraph();
    qDeleteAll(cleared.edges);
    qDeleteAll(cleared.nodes);
}

void CanvasScene::wireNodeChromeSignals(CanvasNodeItem *item)
{
    // M4.4 — hover tracking feeds the chrome overlay's "active node when
    // nothing is selected" fallback; geometryChanged keeps the overlay's
    // position glued to its retargeted node during drags/resizes.
    connect(item, &CanvasNodeItem::hoverChanged, this, [this, item](bool hovered) {
        if (hovered) {
            m_hoveredNode = item;
        } else if (m_hoveredNode == item) {
            m_hoveredNode = nullptr;
        }
        updateActiveChromeTarget();
    });
    connect(item, &CanvasNodeItem::geometryChanged, this, [this, item]() {
        if (m_chromeOverlay && m_chromeOverlay->target() == item)
            m_chromeOverlay->syncToTarget();
    });
}

void CanvasScene::updateActiveChromeTarget()
{
    if (!m_chromeOverlay)
        return;

    // Exactly one node selected -> chrome shows its 8 resize handles
    // always, plus the 4 connection dots only if that same node also
    // happens to be hovered right now (Obsidian-style: connection dots are
    // a hover affordance, not a permanent selected-card decoration — see
    // CanvasNodeChromeOverlay.h and the M4.4 task brief).
    const auto selected = selectedItems();
    if (selected.size() == 1) {
        if (auto *node = dynamic_cast<CanvasNodeItem *>(selected.first())) {
            m_chromeOverlay->retarget(node, /*showHandles=*/true, /*showDots=*/(node == m_hoveredNode));
            return;
        }
    }

    // Nothing selected (or a multi-selection, which this chrome doesn't
    // support) but exactly one node is hovered -> show only the connection
    // dots, matching Obsidian's hover-to-reveal-anchors UX.
    if (selected.isEmpty() && m_hoveredNode) {
        m_chromeOverlay->retarget(m_hoveredNode, /*showHandles=*/false, /*showDots=*/true);
        return;
    }

    m_chromeOverlay->clear();
}

// ---------------------------------------------------------------------------
// Item management
// ---------------------------------------------------------------------------

TextCardItem *CanvasScene::addTextCardItem(const CanvasNode &node)
{
    auto *item = new TextCardItem(node);
    addNode(item);

    connect(item, &TextCardItem::editRequested, this, [this, item]() {
        beginInlineEdit(item);
    });
    wireNodeChromeSignals(item);

    // Render text card content via engine if available
    if (m_renderEngine && !node.text.isEmpty()) {
        auto rendered = m_renderEngine->render(node.text);
        item->setRenderedDocument(std::move(rendered));
    }

    return item;
}

GroupItem *CanvasScene::addGroupItemToScene(const CanvasNode &node)
{
    auto *item = new GroupItem(node);
    addNode(item);

    connect(item, &GroupItem::labelEditRequested, this, [this, item]() {
        beginGroupLabelEdit(item);
    });
    wireNodeChromeSignals(item);

    return item;
}

EdgeItem *CanvasScene::addEdgeItemToScene(CanvasNodeItem *from, CanvasNodeItem *to, const CanvasEdge &edge)
{
    auto *item = new EdgeItem(from, to, edge);
    addEdge(item);
    return item;
}

void CanvasScene::removeTextCardItem(const QString &id)
{
    if (auto *item = textCardItem(id)) {
        removeNode(id);
        delete item;
    }
}

void CanvasScene::removeGroupItem(const QString &id)
{
    if (auto *item = groupItem(id)) {
        removeNode(id);
        delete item;
    }
}

void CanvasScene::removeEdgeItem(const QString &id)
{
    if (auto *item = edgeItem(id)) {
        removeEdge(id);
        delete item;
    }
}

// ---------------------------------------------------------------------------
// Render engine and file resolver
// ---------------------------------------------------------------------------

void CanvasScene::setRenderEngine(Corbomite::MarkdownRenderEngine *engine)
{
    m_renderEngine = engine;
    // Cards may already exist (built when the document loaded, before any engine
    // was wired). Re-render them now so they pick up the newly-set engine.
    reRenderAllCards();
}

void CanvasScene::reRenderAllCards()
{
    if (!m_renderEngine)
        return;

    for (auto *node : nodes()) {
        if (auto *card = dynamic_cast<TextCardItem *>(node)) {
            const QString text = card->nodeData().text;
            if (text.isEmpty()) {
                card->setRenderedDocument(nullptr);
                continue;
            }
            card->setRenderedDocument(m_renderEngine->render(text));
        } else if (auto *file = dynamic_cast<FileCardItem *>(node)) {
            renderFileCard(file);
        }
    }
}

Corbomite::MarkdownRenderEngine *CanvasScene::renderEngine() const
{
    return m_renderEngine;
}

void CanvasScene::setFileResolver(FileResolver resolver)
{
    m_fileResolver = std::move(resolver);
}

void CanvasScene::setFileSaver(FileSaver saver)
{
    m_fileSaver = std::move(saver);
}

void CanvasScene::setFilePickerRequestor(FilePickerRequestor requestor)
{
    m_filePickerRequestor = std::move(requestor);
}

void CanvasScene::createFileCardViaPicker(const QPointF &scenePos)
{
    if (!m_document || !m_filePickerRequestor)
        return;

    const QString path = m_filePickerRequestor();
    if (path.isEmpty())
        return;

    // Appendix A default file-card size: 400x400.
    static constexpr int kFileWidth = 400;
    static constexpr int kFileHeight = 400;

    CanvasNode node;
    node.id = CanvasDocument::generateId();
    node.type = NodeType::File;
    node.file = path; // vault-relative (disk contract §3.4)
    node.x = qRound(scenePos.x());
    node.y = qRound(scenePos.y());
    node.width = kFileWidth;
    node.height = kFileHeight;

    m_undoStack->push(new CmdAddCard(m_document, node));

    if (auto *item = fileCardItem(node.id)) {
        clearSelection();
        item->setSelected(true);
    }
}

void CanvasScene::setVaultPathResolver(VaultPathResolver resolver)
{
    m_vaultPathResolver = std::move(resolver);
}

// ---------------------------------------------------------------------------
// M2.3 — drag-drop node creation
// ---------------------------------------------------------------------------

void CanvasScene::dragEnterEvent(QGraphicsSceneDragDropEvent *event)
{
    if (event->mimeData()->hasUrls() || event->mimeData()->hasText()) {
        event->acceptProposedAction();
        return;
    }
    QGraphicsScene::dragEnterEvent(event);
}

void CanvasScene::dragMoveEvent(QGraphicsSceneDragDropEvent *event)
{
    if (event->mimeData()->hasUrls() || event->mimeData()->hasText()) {
        event->acceptProposedAction();
        return;
    }
    QGraphicsScene::dragMoveEvent(event);
}

void CanvasScene::dropEvent(QGraphicsSceneDragDropEvent *event)
{
    if (!m_document) {
        QGraphicsScene::dropEvent(event);
        return;
    }

    const QMimeData *mime = event->mimeData();
    const QPointF dropPos = event->scenePos();

    if (mime->hasUrls()) {
        // text/uri-list: one file-card node per dropped file, vault-relative.
        // Paths outside the vault are rejected (M1/M2 scope: no
        // copy-into-vault — punch-list candidate if users hit this).
        QStringList vaultRelPaths;
        for (const QUrl &url : mime->urls()) {
            if (!url.isLocalFile())
                continue;
            const QString absPath = url.toLocalFile();
            QString relPath = absPath;
            if (m_vaultPathResolver) {
                relPath = m_vaultPathResolver(absPath);
                if (relPath.isEmpty())
                    continue;
            }
            vaultRelPaths << relPath;
        }

        if (vaultRelPaths.isEmpty()) {
            event->ignore();
            return;
        }

        // Appendix A file-card default: 400x400. Grid spacing = one
        // gridSpacing gap (20px) between file nodes when multiple drop
        // at once, laid out at the drop point.
        static constexpr int kFileWidth = 400;
        static constexpr int kFileHeight = 400;
        static constexpr int kGridGap = 20;
        const int cols = qMax(1, qCeil(qSqrt(static_cast<double>(vaultRelPaths.size()))));

        auto *parentCmd = new QUndoCommand(i18n("Drop Files"));
        QStringList newIds;
        for (int i = 0; i < vaultRelPaths.size(); ++i) {
            const int row = i / cols;
            const int col = i % cols;
            CanvasNode node;
            node.id = CanvasDocument::generateId();
            node.type = NodeType::File;
            node.file = vaultRelPaths.at(i);
            node.width = kFileWidth;
            node.height = kFileHeight;
            node.x = qRound(dropPos.x()) + col * (kFileWidth + kGridGap);
            node.y = qRound(dropPos.y()) + row * (kFileHeight + kGridGap);
            newIds << node.id;
            new CmdAddCard(m_document, node, parentCmd);
        }
        m_undoStack->push(parentCmd);

        clearSelection();
        for (const auto &id : std::as_const(newIds)) {
            if (auto *item = fileCardItem(id))
                item->setSelected(true);
        }

        event->acceptProposedAction();
        return;
    }

    if (mime->hasText() && !mime->text().isEmpty()) {
        // text/plain -> new text card, centered on the drop point (same
        // convention as M2.1's double-click creation).
        static constexpr int kTextWidth = 250;
        static constexpr int kTextHeight = 60;

        CanvasNode node;
        node.id = CanvasDocument::generateId();
        node.type = NodeType::Text;
        node.text = mime->text();
        node.width = kTextWidth;
        node.height = kTextHeight;
        node.x = qRound(dropPos.x() - kTextWidth / 2.0);
        node.y = qRound(dropPos.y() - kTextHeight / 2.0);

        m_undoStack->push(new CmdAddCard(m_document, node));

        if (auto *item = textCardItem(node.id)) {
            clearSelection();
            item->setSelected(true);
        }

        event->acceptProposedAction();
        return;
    }

    event->ignore();
}

// ---------------------------------------------------------------------------
// File card management
// ---------------------------------------------------------------------------

FileCardItem *CanvasScene::addFileCardItem(const CanvasNode &node)
{
    auto *item = new FileCardItem(node);
    addNode(item);

    connect(item, &FileCardItem::editRequested, this, [this, item]() {
        beginFileCardEdit(item);
    });
    wireNodeChromeSignals(item);

    renderFileCard(item);
    return item;
}

void CanvasScene::removeFileCardItem(const QString &id)
{
    if (auto *item = fileCardItem(id)) {
        removeNode(id);
        delete item;
    }
}

FileCardItem *CanvasScene::fileCardItem(const QString &id) const
{
    return dynamic_cast<FileCardItem *>(nodeForId(id));
}

void CanvasScene::renderFileCard(FileCardItem *item)
{
    if (!item || !m_renderEngine)
        return;

    QString markdown;
    if (m_fileResolver) {
        markdown = m_fileResolver(item->nodeData().file);
    }

    if (markdown.isEmpty()) {
        item->setRenderedDocument(nullptr);
        return;
    }

    Corbomite::RenderOptions opts;
    opts.subpath = item->nodeData().subpath;

    auto rendered = m_renderEngine->render(markdown, opts);
    item->setRenderedDocument(std::move(rendered));
}

// ---------------------------------------------------------------------------
// Item lookup
// ---------------------------------------------------------------------------

TextCardItem *CanvasScene::textCardItem(const QString &id) const
{
    return dynamic_cast<TextCardItem *>(nodeForId(id));
}

GroupItem *CanvasScene::groupItem(const QString &id) const
{
    return dynamic_cast<GroupItem *>(nodeForId(id));
}

EdgeItem *CanvasScene::edgeItem(const QString &id) const
{
    return dynamic_cast<EdgeItem *>(edgeForId(id));
}

CanvasNodeItem *CanvasScene::connectableItem(const QString &id) const
{
    return dynamic_cast<CanvasNodeItem *>(nodeForId(id));
}

QUndoStack *CanvasScene::undoStack()
{
    return m_undoStack;
}

// ---------------------------------------------------------------------------
// Document signal handlers (live sync)
// ---------------------------------------------------------------------------

void CanvasScene::onNodeAdded(const QString &id)
{
    if (!m_document)
        return;

    // Skip if already present in scene
    if (nodeForId(id))
        return;

    const CanvasNode node = m_document->node(id);
    switch (node.type) {
    case NodeType::Text:
        addTextCardItem(node);
        break;
    case NodeType::File:
        addFileCardItem(node);
        break;
    case NodeType::Group:
        addGroupItemToScene(node);
        break;
    case NodeType::Link:
        break;
    }
}

void CanvasScene::onNodeRemoved(const QString &id)
{
    // M4.4 — drop any dangling reference to the about-to-be-deleted item
    // BEFORE the delete below (both the hover pointer and the chrome
    // overlay's target are raw CanvasNodeItem* with no lifetime tracking
    // of their own).
    if (m_hoveredNode && m_hoveredNode->nodeId() == id)
        m_hoveredNode = nullptr;
    if (m_chromeOverlay && m_chromeOverlay->target() && m_chromeOverlay->target()->nodeId() == id)
        m_chromeOverlay->clear();

    removeTextCardItem(id);
    removeFileCardItem(id);
    removeGroupItem(id);
}

void CanvasScene::onNodeChanged(const QString &id)
{
    if (!m_document)
        return;

    const CanvasNode node = m_document->node(id);
    if (auto *card = textCardItem(id)) {
        card->setNodeData(node);
        if (m_renderEngine && !node.text.isEmpty()) {
            auto rendered = m_renderEngine->render(node.text);
            card->setRenderedDocument(std::move(rendered));
        }
    } else if (auto *file = fileCardItem(id)) {
        file->setNodeData(node);
        renderFileCard(file);
    } else if (auto *group = groupItem(id)) {
        group->setNodeData(node);
    }
}

void CanvasScene::onEdgeAdded(const QString &id)
{
    if (!m_document)
        return;

    // Skip if already present
    if (edgeForId(id))
        return;

    const CanvasEdge edge = m_document->edge(id);
    auto *from = connectableItem(edge.fromNode);
    auto *to = connectableItem(edge.toNode);
    if (from && to) {
        addEdgeItemToScene(from, to, edge);
    }
}

void CanvasScene::onEdgeRemoved(const QString &id)
{
    removeEdgeItem(id);
}

void CanvasScene::onEdgeChanged(const QString &id)
{
    if (!m_document)
        return;

    const CanvasEdge newData = m_document->edge(id);
    auto *item = edgeItem(id);
    if (!item)
        return;

    const CanvasEdge oldData = item->edgeData();
    if (oldData.fromNode != newData.fromNode || oldData.toNode != newData.toNode) {
        // M3.4: Graffodil::GraphEdgeItem's source/target nodes are fixed at
        // construction (m_source/m_target are private, no setter) — a
        // CmdReconnectEdge that retargets an end to a different node can't
        // be applied by EdgeItem::setEdgeData()'s in-place mutation (it only
        // updates anchor-id strings on the SAME nodes). Treat a node-changing
        // update as remove+recreate, same as onEdgeAdded, rather than
        // silently leaving the item pointed at its old node.
        auto *from = connectableItem(newData.fromNode);
        auto *to = connectableItem(newData.toNode);
        removeEdgeItem(id);
        if (from && to)
            addEdgeItemToScene(from, to, newData);
        return;
    }

    // Fast path: color/label/end changes on the same two nodes (existing
    // "Edit Label" menu action, M3.5 direction submenu via CmdSetEdgeEnds)
    // stay in-place. "Reverse Direction"/R-key (CmdReverseEdge) swap
    // fromNode/toNode, so those hit the remove+recreate branch above.
    item->setEdgeData(newData);
}

// ---------------------------------------------------------------------------
// M1.5 undo wiring — Graffodil tool intent signals -> Cmd* (Appendix B rule 1:
// tools never touch CanvasDocument directly)
// ---------------------------------------------------------------------------

void CanvasScene::onDragBegan(const QList<Graffodil::IGraphNode *> &nodes)
{
    m_dragSnapshot.clear();
    m_dragActive = true;
    m_capturedGroups.clear();

    for (auto *node : nodes)
        m_dragSnapshot.insert(node->nodeId(), node->graphicsItem()->pos());

    // M4.3: any group in the drag set freezes its full-containment members
    // for the whole gesture (Appendix A "Group membership"). Those members
    // move reactively through GroupItem::itemChange, not through
    // SelectMoveTool's own per-node loop (its m_draggedNodes is private —
    // see GroupItem::beginDragCapture doc comment), so fold their pre-drag
    // positions into the same snapshot here or onDragEnded would never see
    // them and their move would be silently lost from undo/persistence.
    for (auto *node : nodes) {
        auto *group = dynamic_cast<GroupItem *>(node->graphicsItem());
        if (!group)
            continue;
        m_capturedGroups.append(group);
        const QStringList capturedIds = group->beginDragCapture();
        for (const QString &id : capturedIds) {
            if (m_dragSnapshot.contains(id))
                continue; // already explicitly part of the drag set
            if (auto *item = connectableItem(id))
                m_dragSnapshot.insert(id, item->pos());
        }
    }
}

void CanvasScene::onDragEnded(const QList<Graffodil::IGraphNode *> &nodes)
{
    Q_UNUSED(nodes); // m_dragSnapshot (seeded in onDragBegan) is the complete set
    m_dragActive = false;

    for (auto *group : std::as_const(m_capturedGroups))
        group->endDragCapture();
    m_capturedGroups.clear();

    if (m_dragSnapshot.isEmpty() || !m_document) {
        m_dragSnapshot.clear();
        return;
    }

    QHash<QString, QPointF> oldPositions;
    QHash<QString, QPointF> newPositions;
    for (auto it = m_dragSnapshot.cbegin(); it != m_dragSnapshot.cend(); ++it) {
        auto *item = connectableItem(it.key());
        if (!item)
            continue;
        oldPositions.insert(it.key(), it.value());
        newPositions.insert(it.key(), item->pos());
    }
    m_dragSnapshot.clear();

    if (!oldPositions.isEmpty() && oldPositions != newPositions) {
        m_undoStack->push(new CmdMoveCards(m_document, oldPositions, newPositions));
    }
}

void CanvasScene::onDeleteRequested(const QList<Graffodil::IGraphNode *> &nodes,
                                     const QList<Graffodil::IGraphEdge *> &edges)
{
    if (!m_document)
        return;

    QStringList nodeIds;
    for (auto *node : nodes)
        nodeIds.append(node->nodeId());
    QStringList edgeIds;
    for (auto *edge : edges)
        edgeIds.append(edge->edgeId());

    if (nodeIds.isEmpty() && edgeIds.isEmpty())
        return;

    // Use a parent command so the entire deletion is a single undo step.
    auto *parentCmd = new QUndoCommand(i18n("Delete Selection"));

    // Remove standalone edge selections first.
    for (const auto &edgeId : edgeIds)
        new CmdRemoveEdge(m_document, edgeId, parentCmd);
    // Remove nodes (CmdRemoveCard saves connected edges for undo; works for
    // any node type — text, file, group — since it's generic doc removal).
    for (const auto &nodeId : nodeIds)
        new CmdRemoveCard(m_document, nodeId, parentCmd);

    m_undoStack->push(parentCmd);
}

void CanvasScene::onResizeCommitted(const QString &nodeId, const QRect &oldRect, const QRect &newRect)
{
    if (!m_document)
        return;
    m_undoStack->push(new CmdResizeCard(m_document, nodeId, oldRect, newRect));
}

void CanvasScene::setEdgeEnds(const QString &edgeId, EndType fromEnd, EndType toEnd)
{
    if (!m_document)
        return;
    const CanvasEdge oldData = m_document->edge(edgeId);
    if (oldData.id.isEmpty())
        return;
    CanvasEdge newData = oldData;
    newData.fromEnd = fromEnd;
    newData.toEnd = toEnd;
    m_undoStack->push(new CmdSetEdgeEnds(m_document, oldData, newData));
}

void CanvasScene::reverseEdge(const QString &edgeId, QUndoCommand *parent)
{
    if (!m_document)
        return;
    auto *cmd = new CmdReverseEdge(m_document, edgeId, parent);
    if (!parent)
        m_undoStack->push(cmd);
}

void CanvasScene::onReverseRequested(const QList<Graffodil::IGraphEdge *> &edges)
{
    if (!m_document || edges.isEmpty())
        return;

    // Single edge: push CmdReverseEdge directly so the undo-stack label
    // stays "Reverse Direction" rather than being wrapped in an extra
    // compound step. Multiple edges (R-press over a multi-edge selection):
    // one compound undo step, same idiom as "Delete Selection"/"Drop Files".
    if (edges.size() == 1) {
        reverseEdge(edges.first()->edgeId());
        return;
    }

    auto *parentCmd = new QUndoCommand(i18n("Reverse Direction"));
    for (auto *edge : edges)
        reverseEdge(edge->edgeId(), parentCmd);
    m_undoStack->push(parentCmd);
}

void CanvasScene::onEdgeRequested(Graffodil::IGraphNode *source, const QString &sourceAnchorId,
                                   Graffodil::IGraphNode *target, const QString &targetAnchorId)
{
    if (!m_document || !source || !target)
        return;

    // CreateEdgeTool::commitEdge/mouseReleaseEvent already reject a
    // same-node gesture as a silent cancel before emitting edgeRequested
    // (self-loop is never emitted) — this is defensive belt-and-suspenders,
    // cheap enough to keep.
    if (source == target)
        return;

    CanvasEdge edge;
    edge.id = CanvasDocument::generateId();
    edge.fromNode = source->nodeId();
    edge.toNode = target->nodeId();
    edge.fromSide = sideFromString(sourceAnchorId);
    edge.toSide = sideFromString(targetAnchorId);
    // fromEnd/toEnd already default to None/Arrow (Appendix A/B rule 2) —
    // left unset rather than redundantly assigned.

    m_undoStack->push(new CmdAddEdge(m_document, edge));
}

void CanvasScene::addCardConnectedTo(CanvasNode node, Graffodil::IGraphNode *source,
                                       const QString &sourceAnchorId)
{
    if (!m_document || !source)
        return;

    // toSide = side facing the source (Appendix A/Phase M3.3): pick the new
    // node's own face using the direction vector FROM the new node's center
    // TOWARD the source's center — mirrors CanvasDocument.cpp's symmetric
    // self-heal call for unresolved-side edges on load.
    const QRectF sourceRect = source->nodeBoundingRect();
    const double newCx = node.x + node.width / 2.0;
    const double newCy = node.y + node.height / 2.0;
    const double srcCx = sourceRect.center().x();
    const double srcCy = sourceRect.center().y();
    const Side toSide = pickSideToward(node.width, node.height, srcCx - newCx, srcCy - newCy);

    CanvasEdge edge;
    edge.id = CanvasDocument::generateId();
    edge.fromNode = source->nodeId();
    edge.toNode = node.id;
    edge.fromSide = sideFromString(sourceAnchorId);
    edge.toSide = toSide;
    // fromEnd/toEnd already default to None/Arrow (Appendix A/B rule 2) —
    // left unset rather than redundantly assigned.

    auto *parentCmd = new QUndoCommand(i18n("New Connected Card"));
    new CmdAddCard(m_document, node, parentCmd);
    new CmdAddEdge(m_document, edge, parentCmd);
    m_undoStack->push(parentCmd);

    if (auto *item = connectableItem(node.id)) {
        clearSelection();
        item->setSelected(true);
    }
}

void CanvasScene::onEdgeDroppedOnEmpty(Graffodil::IGraphNode *source, const QString &sourceAnchorId,
                                         const QPointF &scenePos)
{
    if (!m_document || !source)
        return;

    QPoint screenPos;
    const auto sceneViews = views();
    if (!sceneViews.isEmpty()) {
        QGraphicsView *view = sceneViews.first();
        screenPos = view->viewport()->mapToGlobal(view->mapFromScene(scenePos));
    } else {
        screenPos = QCursor::pos();
    }

    QMenu menu;
    menu.addAction(i18n("New text card"), [this, scenePos, source, sourceAnchorId]() {
        if (!m_document)
            return;
        CanvasNode node;
        node.id = CanvasDocument::generateId();
        node.type = NodeType::Text;
        node.x = qRound(scenePos.x());
        node.y = qRound(scenePos.y());
        node.width = 250;
        node.height = 60;
        addCardConnectedTo(node, source, sourceAnchorId);
    });

    menu.addAction(i18n("New file card…"), [this, scenePos, source, sourceAnchorId]() {
        if (!m_document || !m_filePickerRequestor)
            return;
        const QString path = m_filePickerRequestor();
        if (path.isEmpty())
            return;

        // Appendix A default file-card size: 400x400.
        CanvasNode node;
        node.id = CanvasDocument::generateId();
        node.type = NodeType::File;
        node.file = path; // vault-relative (disk contract §3.4)
        node.x = qRound(scenePos.x());
        node.y = qRound(scenePos.y());
        node.width = 400;
        node.height = 400;
        addCardConnectedTo(node, source, sourceAnchorId);
    });

    menu.addAction(i18n("Cancel"), []() {});

    menu.exec(screenPos);
}

void CanvasScene::onReconnectRequested(const QString &edgeId, Graffodil::ArrowEnd end,
                                        Graffodil::IGraphNode *newNode, const QString &newAnchorId)
{
    if (!m_document || !newNode)
        return;

    const CanvasEdge oldEdge = m_document->edge(edgeId);
    if (oldEdge.id.isEmpty())
        return;

    CanvasEdge newEdge = oldEdge;
    const Side side = sideFromString(newAnchorId);
    if (end == Graffodil::ArrowEnd::Source) {
        newEdge.fromNode = newNode->nodeId();
        newEdge.fromSide = side;
    } else {
        newEdge.toNode = newNode->nodeId();
        newEdge.toSide = side;
    }

    if (newEdge.fromNode == oldEdge.fromNode && newEdge.toNode == oldEdge.toNode
        && newEdge.fromSide == oldEdge.fromSide && newEdge.toSide == oldEdge.toSide)
        return; // dropped back exactly where it started — nothing changed

    m_undoStack->push(new CmdReconnectEdge(m_document, oldEdge, newEdge));
}

void CanvasScene::onReconnectDroppedOnEmpty(const QString &edgeId)
{
    if (!m_document)
        return;
    m_undoStack->push(new CmdRemoveEdge(m_document, edgeId));
}

// ---------------------------------------------------------------------------
// Inline text editing
// ---------------------------------------------------------------------------

void CanvasScene::beginInlineEdit(TextCardItem *card)
{
    if (!card)
        return;

    // Finish any existing edit first
    finishInlineEdit();

    m_editingNodeId = card->nodeId();

    m_editWidget = new QTextEdit;
    m_editWidget->setPlainText(card->nodeData().text);
    m_editWidget->setFixedSize(static_cast<int>(card->boundingRect().width()),
                               static_cast<int>(card->boundingRect().height()));
    m_editWidget->setFrameShape(QFrame::NoFrame);

    m_editProxy = addWidget(m_editWidget);
    m_editProxy->setPos(card->pos());
    m_editProxy->setZValue(100);

    m_editWidget->setFocus();

    // Finish editing on focus loss
    connect(m_editWidget, &QTextEdit::destroyed, this, [this]() {
        m_editProxy = nullptr;
        m_editWidget = nullptr;
        m_editingNodeId.clear();
    });

    // Disconnect any previous focus watcher, then connect a new one
    disconnect(m_focusConnection);
    m_focusConnection = connect(qApp, &QApplication::focusChanged, this, [this](QWidget * /*old*/, QWidget *now) {
        if (m_editWidget && now != m_editWidget) {
            finishInlineEdit();
        }
    });
}

void CanvasScene::finishInlineEdit()
{
    if (!m_editProxy || !m_editWidget)
        return;

    const QString newText = m_editWidget->toPlainText();
    const QString nodeId = m_editingNodeId;

    // Remove the proxy widget
    removeItem(m_editProxy);
    delete m_editProxy;
    m_editProxy = nullptr;
    m_editWidget = nullptr;
    m_editingNodeId.clear();

    // Disconnect the tracked focus connection
    disconnect(m_focusConnection);

    // Update the card and document via undo command
    if (auto *card = textCardItem(nodeId)) {
        const QString oldText = card->nodeData().text;
        if (m_document && oldText != newText) {
            m_undoStack->push(
                new CmdEditText(m_document, nodeId, oldText, newText));
        }
        // Re-render if engine is available
        if (m_renderEngine) {
            auto rendered = m_renderEngine->render(newText);
            card->setRenderedDocument(std::move(rendered));
        }
    }
}

void CanvasScene::beginFileCardEdit(FileCardItem *card)
{
    if (!card)
        return;

    finishInlineEdit();
    finishFileCardEdit();

    m_editingFileCardId = card->nodeId();

    // Load file content
    QString content;
    if (m_fileResolver) {
        content = m_fileResolver(card->nodeData().file);
    }

    m_editWidget = new QTextEdit;
    m_editWidget->setPlainText(content);
    m_editWidget->setFixedSize(static_cast<int>(card->boundingRect().width()),
                               static_cast<int>(card->boundingRect().height()));
    m_editWidget->setFrameShape(QFrame::NoFrame);

    m_editProxy = addWidget(m_editWidget);
    m_editProxy->setPos(card->pos());
    m_editProxy->setZValue(100);

    m_editWidget->setFocus();

    connect(m_editWidget, &QTextEdit::destroyed, this, [this]() {
        m_editProxy = nullptr;
        m_editWidget = nullptr;
        m_editingFileCardId.clear();
    });

    disconnect(m_focusConnection);
    m_focusConnection = connect(qApp, &QApplication::focusChanged, this, [this](QWidget *, QWidget *now) {
        if (m_editWidget && now != m_editWidget) {
            finishFileCardEdit();
        }
    });
}

void CanvasScene::finishFileCardEdit()
{
    if (!m_editProxy || !m_editWidget || m_editingFileCardId.isEmpty())
        return;

    const QString newContent = m_editWidget->toPlainText();
    const QString nodeId = m_editingFileCardId;

    removeItem(m_editProxy);
    delete m_editProxy;
    m_editProxy = nullptr;
    m_editWidget = nullptr;
    m_editingFileCardId.clear();
    disconnect(m_focusConnection);

    // Save to file via callback
    if (auto *card = fileCardItem(nodeId)) {
        if (m_fileSaver) {
            m_fileSaver(card->nodeData().file, newContent);
        }
        // Re-render the card
        renderFileCard(card);
    }
}

void CanvasScene::beginGroupLabelEdit(GroupItem *group)
{
    if (!group || m_labelEditProxy)
        return;

    m_editingGroupId = group->nodeId();

    auto *lineEdit = new QLineEdit;
    lineEdit->setText(group->nodeData().label);
    lineEdit->setFixedWidth(static_cast<int>(group->boundingRect().width() - 16));

    m_labelEditProxy = addWidget(lineEdit);
    m_labelEditProxy->setPos(group->pos() + QPointF(8, 8));
    m_labelEditProxy->setZValue(100);

    lineEdit->setFocus();
    lineEdit->selectAll();

    // Finish on return key
    connect(lineEdit, &QLineEdit::returnPressed, this, &CanvasScene::finishGroupLabelEdit);

    // Finish on focus loss
    connect(qApp, &QApplication::focusChanged, this, [this, lineEdit](QWidget * /*old*/, QWidget *now) {
        if (now != lineEdit) {
            finishGroupLabelEdit();
        }
    });
}

void CanvasScene::finishGroupLabelEdit()
{
    if (!m_labelEditProxy)
        return;

    auto *lineEdit = qobject_cast<QLineEdit *>(m_labelEditProxy->widget());
    const QString newLabel = lineEdit ? lineEdit->text() : QString();
    const QString groupId = m_editingGroupId;

    // This method is connected directly to QLineEdit::returnPressed, which
    // fires synchronously from inside the line edit's own keyPressEvent
    // (QWidgetLineControl::processKeyEvent) -- still on the call stack
    // here. A plain `delete proxy` would free the QLineEdit whose method is
    // mid-call, corrupting the heap once control returns. removeItem()
    // itself just detaches from the scene (safe synchronously); deleteLater()
    // defers actual destruction past the current call stack. See the
    // identical fix + rationale on the edge "Edit Label" action above.
    auto *proxy = m_labelEditProxy;
    removeItem(proxy);
    proxy->deleteLater();
    m_labelEditProxy = nullptr;
    m_editingGroupId.clear();

    disconnect(qApp, &QApplication::focusChanged, this, nullptr);

    if (auto *group = groupItem(groupId)) {
        CanvasNode data = group->nodeData();
        data.label = newLabel;
        group->setNodeData(data);

        if (m_document) {
            m_document->updateNode(data);
        }
    }
}

// ---------------------------------------------------------------------------
// Mouse/key event delegation — only the edit-proxy pre-check is
// consumer-owned; everything else routes to GraphScene's own dispatch
// (active tool, focused-editor priority, edge-action/sub-item hooks).
// ---------------------------------------------------------------------------

void CanvasScene::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    // If editing, let the proxy widget handle it (unless click is outside the proxy)
    if (m_editProxy) {
        auto *item = itemAt(event->scenePos(), QTransform());
        if (item == m_editProxy || (item && item->parentItem() == m_editProxy)) {
            QGraphicsScene::mousePressEvent(event);
            return;
        }
        // Click outside proxy -> finish editing, then handle normally
        finishInlineEdit();
        finishFileCardEdit();
    }

    GraphScene::mousePressEvent(event);
}

void CanvasScene::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_editProxy) {
        QGraphicsScene::mouseMoveEvent(event);
        return;
    }
    GraphScene::mouseMoveEvent(event);
}

void CanvasScene::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_editProxy) {
        QGraphicsScene::mouseReleaseEvent(event);
        return;
    }
    GraphScene::mouseReleaseEvent(event);
}

void CanvasScene::keyPressEvent(QKeyEvent *event)
{
    if (m_editProxy) {
        // Let the proxy widget handle all key events during editing
        QGraphicsScene::keyPressEvent(event);
        return;
    }

    // Ctrl+A (select all) is a pre-M1 canvas feature (plan "Working today"
    // list) that Graffodil::SelectMoveTool does not implement (it only owns
    // Delete/Backspace/R). Guarded on !focusItem() so it never steals keys
    // from an in-place label/text editor (group-label, edge-label proxies).
    //
    // Arrow-key nudge was ALSO a pre-M1 stopgap here (flat 1px/10px,
    // moveBy() directly on each selected item — no undo push at all) —
    // M4.2 redesigns it: step = the current CanvasAlignmentStrategy
    // grid-spacing rung for the view's zoom (20/40/80/160, Appendix A),
    // x5 with Shift, and the move is now pushed through CmdMoveCards
    // (same undo path as a mouse drag, via onDragEnded's before/after
    // position-hash shape) so nudging is finally undoable.
    if (!focusItem()) {
        if ((event->modifiers() & Qt::ControlModifier) && event->key() == Qt::Key_A) {
            for (auto *node : nodes())
                node->graphicsItem()->setSelected(true);
            event->accept();
            return;
        }
        if (event->key() == Qt::Key_Left || event->key() == Qt::Key_Right ||
            event->key() == Qt::Key_Up || event->key() == Qt::Key_Down) {
            const QList<Graffodil::IGraphNode *> selected = selectedNodes();
            if (selected.isEmpty() || !m_document) {
                event->accept();
                return;
            }

            // Same views().first()->transform().m11() pattern
            // CanvasAlignmentStrategy::align() uses to read the live zoom
            // scale (no view -> no scale-dependent step, fall back to the
            // scale==1 rung).
            qreal scale = 1.0;
            if (!views().isEmpty())
                scale = views().first()->transform().m11();
            qreal step = CanvasAlignmentStrategy::gridSpacingForScale(scale);
            if (event->modifiers() & Qt::ShiftModifier)
                step *= 5.0;

            qreal dx = 0, dy = 0;
            if (event->key() == Qt::Key_Left) dx = -step;
            else if (event->key() == Qt::Key_Right) dx = step;
            else if (event->key() == Qt::Key_Up) dy = -step;
            else dy = step;

            QHash<QString, QPointF> oldPositions;
            QHash<QString, QPointF> newPositions;
            for (auto *node : selected) {
                QGraphicsItem *item = node->graphicsItem();
                if (!item)
                    continue;
                const QPointF oldPos = item->pos();
                const QPointF newPos = oldPos + QPointF(dx, dy);
                oldPositions.insert(node->nodeId(), oldPos);
                newPositions.insert(node->nodeId(), newPos);
                item->setPos(newPos);
            }
            if (!oldPositions.isEmpty())
                m_undoStack->push(new CmdMoveCards(m_document, oldPositions, newPositions));

            event->accept();
            return;
        }
    }

    GraphScene::keyPressEvent(event);
}

// ---------------------------------------------------------------------------
// M2.1 — double-click empty canvas -> new text card, born in edit mode
// ---------------------------------------------------------------------------

void CanvasScene::mouseDoubleClickEventBackground(const QPointF &scenePos)
{
    if (!m_document)
        return;

    // Appendix A default text-card size: 250x60. "Centered on click point"
    // per Obsidian: the click point becomes the card's center, not its
    // top-left corner.
    static constexpr int kTextWidth = 250;
    static constexpr int kTextHeight = 60;

    CanvasNode node;
    node.id = CanvasDocument::generateId();
    node.type = NodeType::Text;
    node.x = qRound(scenePos.x() - kTextWidth / 2.0);
    node.y = qRound(scenePos.y() - kTextHeight / 2.0);
    node.width = kTextWidth;
    node.height = kTextHeight;

    m_undoStack->push(new CmdAddCard(m_document, node));

    if (auto *item = textCardItem(node.id)) {
        clearSelection();
        item->setSelected(true);
        beginInlineEdit(item);
    }
}

// ---------------------------------------------------------------------------
// M2.4 — clipboard
// ---------------------------------------------------------------------------

QString CanvasScene::serializeSelectionAsCanvasJson() const
{
    if (!m_document)
        return {};

    QSet<QString> selectedIds;
    for (auto *node : selectedNodes())
        selectedIds.insert(node->nodeId());
    if (selectedIds.isEmpty())
        return {};

    // Build the JSON via a scratch CanvasDocument so serialization stays a
    // single source of truth with the real toJson() (same key set, same
    // defaults-omission rules) instead of duplicating it here.
    CanvasDocument temp;
    for (const auto &id : std::as_const(selectedIds)) {
        if (m_document->hasNode(id))
            temp.addNode(m_document->node(id));
    }
    // Edge included only when both endpoints are selected.
    for (const auto &edge : m_document->edges()) {
        if (selectedIds.contains(edge.fromNode) && selectedIds.contains(edge.toNode))
            temp.addEdge(edge);
    }

    return QString::fromUtf8(QJsonDocument(temp.toJson()).toJson(QJsonDocument::Compact));
}

void CanvasScene::pasteCanvasJsonOrText(const QString &clipboardText, const QPointF &pasteCenterScenePos)
{
    if (!m_document || clipboardText.isEmpty())
        return;

    // Canvas-JSON detection: valid JSON object with "nodes" and "edges"
    // arrays — the exact shape serializeSelectionAsCanvasJson() emits and
    // what Obsidian itself puts on the clipboard.
    QJsonParseError err;
    const QJsonDocument jdoc = QJsonDocument::fromJson(clipboardText.toUtf8(), &err);
    if (err.error == QJsonParseError::NoError && jdoc.isObject()) {
        const QJsonObject obj = jdoc.object();
        if (obj.value(QStringLiteral("nodes")).isArray()
            && obj.value(QStringLiteral("edges")).isArray()) {
            CanvasDocument scratch;
            scratch.loadFromJson(obj);

            const auto srcNodes = scratch.nodes();
            const auto srcEdges = scratch.edges();
            if (srcNodes.isEmpty() && srcEdges.isEmpty())
                return;

            // Re-issue every node/edge a fresh 16-hex id; remap edge
            // endpoints; offset position +16px. One compound undo command.
            QHash<QString, QString> idMap;
            auto *parentCmd = new QUndoCommand(i18n("Paste"));
            QStringList newIds;
            for (auto node : srcNodes) {
                const QString newId = CanvasDocument::generateId();
                idMap.insert(node.id, newId);
                node.id = newId;
                node.x += 16;
                node.y += 16;
                newIds << newId;
                new CmdAddCard(m_document, node, parentCmd);
            }
            for (auto edge : srcEdges) {
                if (!idMap.contains(edge.fromNode) || !idMap.contains(edge.toNode))
                    continue; // both endpoints are always in the same clip by construction
                edge.id = CanvasDocument::generateId();
                edge.fromNode = idMap.value(edge.fromNode);
                edge.toNode = idMap.value(edge.toNode);
                new CmdAddEdge(m_document, edge, parentCmd);
            }
            m_undoStack->push(parentCmd);

            clearSelection();
            for (const auto &id : std::as_const(newIds)) {
                if (auto *item = connectableItem(id))
                    item->setSelected(true);
            }
            return;
        }
    }

    // Plain text -> new text card, centered on pasteCenterScenePos (same
    // convention as M2.1/M2.3 gesture-driven text-card creation).
    static constexpr int kTextWidth = 250;
    static constexpr int kTextHeight = 60;

    CanvasNode node;
    node.id = CanvasDocument::generateId();
    node.type = NodeType::Text;
    node.text = clipboardText;
    node.width = kTextWidth;
    node.height = kTextHeight;
    node.x = qRound(pasteCenterScenePos.x() - kTextWidth / 2.0);
    node.y = qRound(pasteCenterScenePos.y() - kTextHeight / 2.0);

    m_undoStack->push(new CmdAddCard(m_document, node));

    if (auto *item = textCardItem(node.id)) {
        clearSelection();
        item->setSelected(true);
    }
}

void CanvasScene::copySelectionToClipboard()
{
    const QString json = serializeSelectionAsCanvasJson();
    if (json.isEmpty())
        return;

    // Obsidian puts canvas-JSON on the clipboard as plain text (enables
    // cross-app paste), so a plain setText() is correct here — no custom
    // mime type.
    QGuiApplication::clipboard()->setText(json);
}

void CanvasScene::cutSelectionToClipboard()
{
    copySelectionToClipboard();
    // Reuse the exact same compound-delete path Delete/Backspace uses.
    onDeleteRequested(selectedNodes(), selectedEdges());
}

// ---------------------------------------------------------------------------
// Context menu helpers
// ---------------------------------------------------------------------------

void CanvasScene::addColorSubmenu(QMenu *parentMenu, const QString &nodeId, const QString &currentColor)
{
    auto *colorMenu = parentMenu->addMenu(i18n("Color"));
    const struct { QString name; QString code; } colors[] = {
        { i18n("Red"),    QStringLiteral("1") },
        { i18n("Orange"), QStringLiteral("2") },
        { i18n("Yellow"), QStringLiteral("3") },
        { i18n("Green"),  QStringLiteral("4") },
        { i18n("Cyan"),   QStringLiteral("5") },
        { i18n("Purple"), QStringLiteral("6") },
    };
    for (const auto &c : colors) {
        colorMenu->addAction(c.name, [this, nodeId, oldColor = currentColor, code = c.code]() {
            if (!m_document) return;
            m_undoStack->push(new CmdChangeColor(m_document, nodeId, oldColor, code));
        });
    }
    colorMenu->addSeparator();
    colorMenu->addAction(i18n("Remove Color"), [this, nodeId, oldColor = currentColor]() {
        if (!m_document) return;
        m_undoStack->push(new CmdChangeColor(m_document, nodeId, oldColor, QString()));
    });
}

// ---------------------------------------------------------------------------
// Context menu
// ---------------------------------------------------------------------------

void CanvasScene::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
{
    if (!m_document) {
        QGraphicsScene::contextMenuEvent(event);
        return;
    }

    const QPointF scenePos = event->scenePos();
    QGraphicsItem *hitItem = itemAt(scenePos, QTransform());

    // Walk up to find a canvas item
    TextCardItem *cardItem = nullptr;
    FileCardItem *fileItem = nullptr;
    GroupItem *grpItem = nullptr;
    EdgeItem *edgItem = nullptr;
    while (hitItem) {
        if (!cardItem) cardItem = dynamic_cast<TextCardItem *>(hitItem);
        if (!fileItem) fileItem = dynamic_cast<FileCardItem *>(hitItem);
        if (!grpItem) grpItem = dynamic_cast<GroupItem *>(hitItem);
        if (!edgItem) edgItem = dynamic_cast<EdgeItem *>(hitItem);
        if (cardItem || grpItem || edgItem || fileItem)
            break;
        hitItem = hitItem->parentItem();
    }

    QMenu menu;

    if (cardItem) {
        // Right-click on a TextCardItem
        menu.addAction(i18n("Edit"), [this, cardItem]() {
            beginInlineEdit(cardItem);
        });

        addColorSubmenu(&menu, cardItem->nodeId(), cardItem->nodeData().color);

        menu.addAction(i18n("Duplicate"), [this, cardItem, scenePos]() {
            if (!m_document)
                return;
            CanvasNode data = cardItem->nodeData();
            data.id = CanvasDocument::generateId();
            data.x += 20;
            data.y += 20;
            m_undoStack->push(new CmdAddCard(m_document, data));
        });

        menu.addSeparator();
        menu.addAction(i18n("Delete"), [this, cardItem]() {
            if (!m_document)
                return;
            m_undoStack->push(
                new CmdRemoveCard(m_document, cardItem->nodeId()));
        });
    } else if (fileItem) {
        // Right-click on a FileCardItem
        addColorSubmenu(&menu, fileItem->nodeId(), fileItem->nodeData().color);

        menu.addSeparator();
        menu.addAction(i18n("Delete"), [this, fileItem]() {
            if (!m_document)
                return;
            m_undoStack->push(
                new CmdRemoveCard(m_document, fileItem->nodeId()));
        });
    } else if (grpItem) {
        // Right-click on a GroupItem
        menu.addAction(i18n("Edit Label"), [this, grpItem]() {
            beginGroupLabelEdit(grpItem);
        });

        addColorSubmenu(&menu, grpItem->nodeId(), grpItem->nodeData().color);

        menu.addAction(i18n("Duplicate"), [this, grpItem]() {
            if (!m_document)
                return;
            CanvasNode data = grpItem->nodeData();
            data.id = CanvasDocument::generateId();
            data.x += 20;
            data.y += 20;
            m_undoStack->push(new CmdAddCard(m_document, data));
        });

        menu.addSeparator();
        menu.addAction(i18n("Delete"), [this, grpItem]() {
            if (!m_document)
                return;
            m_undoStack->push(
                new CmdRemoveCard(m_document, grpItem->nodeId()));
        });
    } else if (edgItem) {
        // Right-click on an EdgeItem
        menu.addAction(i18n("Edit Label"), [this, edgItem]() {
            // Simple inline label edit: use an input dialog approach via proxy widget
            auto *lineEdit = new QLineEdit;
            lineEdit->setText(edgItem->edgeData().label);
            lineEdit->setFixedWidth(150);

            auto *proxy = addWidget(lineEdit);
            proxy->setPos(edgItem->boundingRect().center());
            proxy->setZValue(100);
            lineEdit->setFocus();
            lineEdit->selectAll();

            const QString edgeId = edgItem->edgeId();
            connect(lineEdit, &QLineEdit::returnPressed, this, [this, proxy, lineEdit, edgeId]() {
                const QString newLabel = lineEdit->text();

                // returnPressed fires synchronously from inside QLineEdit's
                // own keyPressEvent (QWidgetLineControl::processKeyEvent),
                // which is still executing on the call stack here. A plain
                // `delete proxy` would free the QLineEdit whose method is
                // mid-call -- control returns into freed memory once this
                // lambda returns (use-after-free / heap corruption).
                // removeItem() itself just detaches from the scene (safe
                // synchronously); deleteLater() is Qt's standard idiom for
                // safely destroying an object from within its own event
                // handler, deferring actual destruction past the current
                // call stack.
                removeItem(proxy);
                proxy->deleteLater();

                if (auto *edge = edgeItem(edgeId)) {
                    CanvasEdge data = edge->edgeData();
                    data.label = newLabel;
                    edge->setEdgeData(data);
                    if (m_document)
                        m_document->updateEdge(data);
                }
            });
        });

        menu.addAction(i18n("Reverse Direction"), [this, edgItem]() {
            reverseEdge(edgItem->edgeId());
        });

        // M3.5 — Direction submenu: Nondirectional / Unidirectional /
        // Bidirectional, mapped to (fromEnd, toEnd) per Appendix A.
        auto *dirMenu = menu.addMenu(i18n("Direction"));
        const QString edgeId = edgItem->edgeId();
        dirMenu->addAction(i18n("Nondirectional"), [this, edgeId]() {
            setEdgeEnds(edgeId, EndType::None, EndType::None);
        });
        dirMenu->addAction(i18n("Unidirectional"), [this, edgeId]() {
            setEdgeEnds(edgeId, EndType::None, EndType::Arrow);
        });
        dirMenu->addAction(i18n("Bidirectional"), [this, edgeId]() {
            setEdgeEnds(edgeId, EndType::Arrow, EndType::Arrow);
        });

        menu.addSeparator();
        menu.addAction(i18n("Delete"), [this, edgItem]() {
            if (!m_document)
                return;
            m_undoStack->push(
                new CmdRemoveEdge(m_document, edgItem->edgeId()));
        });
    } else {
        // Right-click on empty space
        menu.addAction(i18n("New text card"), [this, scenePos]() {
            if (!m_document)
                return;
            CanvasNode node;
            node.id = CanvasDocument::generateId();
            node.type = NodeType::Text;
            node.x = qRound(scenePos.x());
            node.y = qRound(scenePos.y());
            node.width = 250;
            node.height = 60;
            m_undoStack->push(new CmdAddCard(m_document, node));
        });

        menu.addAction(i18n("New file card…"), [this, scenePos]() {
            createFileCardViaPicker(scenePos);
        });

        menu.addAction(i18n("New group"), [this, scenePos]() {
            if (!m_document)
                return;
            CanvasNode node;
            node.id = CanvasDocument::generateId();
            node.type = NodeType::Group;
            node.x = qRound(scenePos.x());
            node.y = qRound(scenePos.y());
            node.width = 400;
            node.height = 300;
            node.label = i18n("Group");
            m_undoStack->push(new CmdAddCard(m_document, node));
        });
    }

    menu.exec(event->screenPos());
}

// --- Cluster R Task 3.5 — image / SVG export --------------------------------

QImage CanvasScene::renderToImage(const QRectF &bounds, bool transparentBg,
                                    bool showEdges, qreal scale)
{
    const QSize sz(qMax(1, qRound(bounds.width()  * scale)),
                   qMax(1, qRound(bounds.height() * scale)));
    QImage img(sz, transparentBg ? QImage::Format_ARGB32
                                  : QImage::Format_RGB32);
    img.fill(transparentBg ? Qt::transparent
                            : backgroundBrush().color());

    // Hide edge items for "just the nodes" export when requested.
    QList<QGraphicsItem *> hidden;
    if (!showEdges) {
        for (auto *edge : edges()) {
            QGraphicsItem *gi = edge->graphicsItem();
            if (gi && gi->isVisible()) {
                gi->setVisible(false);
                hidden.append(gi);
            }
        }
    }

    {
        QPainter p(&img);
        p.setRenderHint(QPainter::Antialiasing);
        render(&p, QRectF(0, 0, sz.width(), sz.height()), bounds);
    }

    // Restore visibility.
    for (auto *item : hidden) item->setVisible(true);
    return img;
}

void CanvasScene::renderToSvg(const QRectF &bounds, QIODevice *out,
                                bool transparentBg, bool showEdges)
{
    if (!out) return;

    QSvgGenerator svg;
    svg.setOutputDevice(out);
    svg.setSize(bounds.size().toSize());
    svg.setViewBox(bounds);
    svg.setTitle(QStringLiteral("Canvas export"));

    QList<QGraphicsItem *> hidden;
    if (!showEdges) {
        for (auto *edge : edges()) {
            QGraphicsItem *gi = edge->graphicsItem();
            if (gi && gi->isVisible()) {
                gi->setVisible(false);
                hidden.append(gi);
            }
        }
    }

    {
        QPainter p(&svg);
        if (!transparentBg) {
            p.fillRect(bounds, backgroundBrush());
        }
        render(&p, bounds, bounds);
    }

    for (auto *item : hidden) item->setVisible(true);
}

} // namespace Canvas
