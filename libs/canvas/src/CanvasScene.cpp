// SPDX-License-Identifier: GPL-3.0-or-later
#include "canvas/CanvasScene.h"
#include "canvas/CanvasCommands.h"
#include "canvas/CanvasDocument.h"
#include "canvas/CanvasDuplicateDragTool.h"
#include "canvas/CanvasNodeItem.h"
#include "canvas/CanvasResizeTool.h"
#include "canvas/TextCardItem.h"
#include "canvas/FileCardItem.h"
#include "canvas/GroupItem.h"
#include "canvas/EdgeItem.h"
#include "corbomite/core/MarkdownRenderEngine.h"
#include "corbomite/core/RenderOptions.h"

#include <graffodil/CompositeTool.h>
#include <graffodil/SelectMoveTool.h>
#include <graffodil/PanZoomTool.h>
#include <graffodil/IGraphNode.h>
#include <graffodil/IGraphEdge.h>

#include <QApplication>
#include <QClipboard>
#include <QGraphicsProxyWidget>
#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneDragDropEvent>
#include <QGraphicsSceneMouseEvent>
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
    m_panZoomTool = new Graffodil::PanZoomTool(this);
    m_resizeTool = new CanvasResizeTool(this);
    m_duplicateDragTool = new CanvasDuplicateDragTool(this);
    m_compositeTool = new Graffodil::CompositeTool(this);

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
    // reverseRequested (R key) intentionally ignored in M1 — new behavior,
    // wired through an edge-direction command in M3 (spec §3.6).
    connect(m_resizeTool, &CanvasResizeTool::resizeCommitted,
            this, &CanvasScene::onResizeCommitted);
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

    // GraphScene::clearGraph() removes items from the scene registry (and
    // the QGraphicsScene) but does not delete them — we still own them.
    const auto cleared = clearGraph();
    qDeleteAll(cleared.edges);
    qDeleteAll(cleared.nodes);
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

    if (auto *item = edgeItem(id)) {
        item->setEdgeData(m_document->edge(id));
    }
}

// ---------------------------------------------------------------------------
// M1.5 undo wiring — Graffodil tool intent signals -> Cmd* (Appendix B rule 1:
// tools never touch CanvasDocument directly)
// ---------------------------------------------------------------------------

void CanvasScene::onDragBegan(const QList<Graffodil::IGraphNode *> &nodes)
{
    m_dragSnapshot.clear();
    for (auto *node : nodes)
        m_dragSnapshot.insert(node->nodeId(), node->graphicsItem()->pos());
}

void CanvasScene::onDragEnded(const QList<Graffodil::IGraphNode *> &nodes)
{
    if (m_dragSnapshot.isEmpty() || !m_document) {
        m_dragSnapshot.clear();
        return;
    }

    QHash<QString, QPointF> oldPositions;
    QHash<QString, QPointF> newPositions;
    for (auto *node : nodes) {
        const QString id = node->nodeId();
        if (!m_dragSnapshot.contains(id))
            continue;
        const QPointF oldPos = m_dragSnapshot.value(id);
        const QPointF newPos = node->graphicsItem()->pos();
        oldPositions.insert(id, oldPos);
        newPositions.insert(id, newPos);
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

    removeItem(m_labelEditProxy);
    delete m_labelEditProxy;
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

    // Ctrl+A (select all) and arrow-key nudge (1px / 10px with Shift) are
    // pre-M1 canvas features (plan "Working today" list) that
    // Graffodil::SelectMoveTool does not implement (it only owns
    // Delete/Backspace/R). The migration spec is silent on them — M4.2
    // formally redesigns nudge with grid-snap stepping later — so this is
    // a feature-freeze stopgap kept here rather than silently dropped.
    // Guarded on !focusItem() so it never steals keys from an in-place
    // label/text editor (group-label, edge-label proxies).
    if (!focusItem()) {
        if ((event->modifiers() & Qt::ControlModifier) && event->key() == Qt::Key_A) {
            for (auto *node : nodes())
                node->graphicsItem()->setSelected(true);
            event->accept();
            return;
        }
        if (event->key() == Qt::Key_Left || event->key() == Qt::Key_Right ||
            event->key() == Qt::Key_Up || event->key() == Qt::Key_Down) {
            const qreal step = (event->modifiers() & Qt::ShiftModifier) ? 10.0 : 1.0;
            qreal dx = 0, dy = 0;
            if (event->key() == Qt::Key_Left) dx = -step;
            else if (event->key() == Qt::Key_Right) dx = step;
            else if (event->key() == Qt::Key_Up) dy = -step;
            else dy = step;
            for (auto *item : selectedItems())
                item->moveBy(dx, dy);
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
                removeItem(proxy);
                delete proxy;

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
            if (!m_document)
                return;
            CanvasEdge data = edgItem->edgeData();
            // Swap from/to nodes and sides
            std::swap(data.fromNode, data.toNode);
            std::swap(data.fromSide, data.toSide);
            std::swap(data.fromEnd, data.toEnd);

            const QString edgeId = data.id;
            // Remove and re-create the edge item with swapped source/target
            removeEdgeItem(edgeId);
            m_document->removeEdge(edgeId);

            m_document->addEdge(data);
            auto *from = connectableItem(data.fromNode);
            auto *to = connectableItem(data.toNode);
            if (from && to) {
                addEdgeItemToScene(from, to, data);
            }
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
