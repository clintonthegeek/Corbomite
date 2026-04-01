// SPDX-License-Identifier: GPL-3.0-or-later
#include "canvas/CanvasScene.h"
#include "canvas/CanvasCommands.h"
#include "canvas/CanvasDocument.h"
#include "canvas/CanvasTool.h"
#include "canvas/ConnectableItem.h"
#include "canvas/TextCardItem.h"
#include "canvas/FileCardItem.h"
#include "canvas/GroupItem.h"
#include "canvas/EdgeItem.h"
#include "corbomite/core/MarkdownRenderEngine.h"
#include "corbomite/core/RenderOptions.h"

#include <QApplication>
#include <QGraphicsProxyWidget>
#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMenu>
#include <QTextEdit>
#include <QUndoStack>

namespace Canvas {

CanvasScene::CanvasScene(QObject *parent)
    : QGraphicsScene(parent)
    , m_undoStack(new QUndoStack(this))
{
    // Create the default tool
    m_defaultTool = new SelectMoveTool(this, this);
    setActiveTool(m_defaultTool);
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

    m_textCardItems.clear();
    m_fileCardItems.clear();
    m_groupItems.clear();
    m_edgeItems.clear();
    clear();
}

// ---------------------------------------------------------------------------
// Item management
// ---------------------------------------------------------------------------

TextCardItem *CanvasScene::addTextCardItem(const CanvasNode &node)
{
    auto *item = new TextCardItem(node);
    addItem(item);
    m_textCardItems.insert(node.id, item);

    // Connect editRequested to inline editing
    connect(item, &TextCardItem::editRequested, this, [this, item]() {
        beginInlineEdit(item);
    });

    // Forward position changes to edge adjustment
    connect(item, &TextCardItem::positionChanged, this, [this, item]() {
        // Adjust all edges connected to this card
        if (!m_document)
            return;
        const auto edges = m_document->edgesForNode(item->nodeId());
        for (const auto &edge : edges) {
            if (auto *edgeItem = this->edgeItem(edge.id)) {
                edgeItem->adjust();
            }
        }
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
    addItem(item);
    m_groupItems.insert(node.id, item);

    // Connect label edit
    connect(item, &GroupItem::labelEditRequested, this, [this, item]() {
        beginGroupLabelEdit(item);
    });

    return item;
}

EdgeItem *CanvasScene::addEdgeItemToScene(ConnectableItem *from, ConnectableItem *to, const CanvasEdge &edge)
{
    auto *item = new EdgeItem(from, to, edge);
    addItem(item);
    m_edgeItems.insert(edge.id, item);
    return item;
}

void CanvasScene::removeTextCardItem(const QString &id)
{
    if (auto *item = m_textCardItems.take(id)) {
        removeItem(item);
        delete item;
    }
}

void CanvasScene::removeGroupItem(const QString &id)
{
    if (auto *item = m_groupItems.take(id)) {
        removeItem(item);
        delete item;
    }
}

void CanvasScene::removeEdgeItem(const QString &id)
{
    if (auto *item = m_edgeItems.take(id)) {
        removeItem(item);
        delete item;
    }
}

// ---------------------------------------------------------------------------
// Render engine and file resolver
// ---------------------------------------------------------------------------

void CanvasScene::setRenderEngine(Corbomite::MarkdownRenderEngine *engine)
{
    m_renderEngine = engine;
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

// ---------------------------------------------------------------------------
// File card management
// ---------------------------------------------------------------------------

FileCardItem *CanvasScene::addFileCardItem(const CanvasNode &node)
{
    auto *item = new FileCardItem(node);
    addItem(item);
    m_fileCardItems.insert(node.id, item);

    connect(item, &FileCardItem::editRequested, this, [this, item]() {
        beginFileCardEdit(item);
    });

    connect(item, &FileCardItem::positionChanged, this, [this, item]() {
        if (!m_document)
            return;
        const auto edges = m_document->edgesForNode(item->nodeId());
        for (const auto &edge : edges) {
            if (auto *edgeItem = this->edgeItem(edge.id)) {
                edgeItem->adjust();
            }
        }
    });

    renderFileCard(item);
    return item;
}

void CanvasScene::removeFileCardItem(const QString &id)
{
    if (auto *item = m_fileCardItems.take(id)) {
        removeItem(item);
        delete item;
    }
}

FileCardItem *CanvasScene::fileCardItem(const QString &id) const
{
    return m_fileCardItems.value(id, nullptr);
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
// Tool management
// ---------------------------------------------------------------------------

void CanvasScene::setActiveTool(CanvasTool *tool)
{
    if (m_activeTool)
        m_activeTool->deactivate();
    m_activeTool = tool;
    if (m_activeTool)
        m_activeTool->activate();
}

CanvasTool *CanvasScene::activeTool() const
{
    return m_activeTool;
}

// ---------------------------------------------------------------------------
// Item lookup
// ---------------------------------------------------------------------------

TextCardItem *CanvasScene::textCardItem(const QString &id) const
{
    return m_textCardItems.value(id, nullptr);
}

GroupItem *CanvasScene::groupItem(const QString &id) const
{
    return m_groupItems.value(id, nullptr);
}

EdgeItem *CanvasScene::edgeItem(const QString &id) const
{
    return m_edgeItems.value(id, nullptr);
}

ConnectableItem *CanvasScene::connectableItem(const QString &id) const
{
    if (auto *card = textCardItem(id))
        return card;
    if (auto *file = fileCardItem(id))
        return file;
    return nullptr;
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
    if (m_textCardItems.contains(id) || m_groupItems.contains(id) || m_fileCardItems.contains(id))
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
    if (m_edgeItems.contains(id))
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
// Mouse event delegation
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

    if (m_activeTool) {
        m_activeTool->mousePressEvent(event);
        return;
    }
    QGraphicsScene::mousePressEvent(event);
}

void CanvasScene::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_editProxy) {
        QGraphicsScene::mouseMoveEvent(event);
        return;
    }
    if (m_activeTool) {
        m_activeTool->mouseMoveEvent(event);
        return;
    }
    QGraphicsScene::mouseMoveEvent(event);
}

void CanvasScene::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_editProxy) {
        QGraphicsScene::mouseReleaseEvent(event);
        return;
    }
    if (m_activeTool) {
        m_activeTool->mouseReleaseEvent(event);
        return;
    }
    QGraphicsScene::mouseReleaseEvent(event);
}

void CanvasScene::keyPressEvent(QKeyEvent *event)
{
    if (m_editProxy) {
        // Let the proxy widget handle all key events during editing
        QGraphicsScene::keyPressEvent(event);
        return;
    }
    if (m_activeTool) {
        m_activeTool->keyPressEvent(event);
        return;
    }
    QGraphicsScene::keyPressEvent(event);
}

// ---------------------------------------------------------------------------
// Context menu helpers
// ---------------------------------------------------------------------------

void CanvasScene::addColorSubmenu(QMenu *parentMenu, const QString &nodeId, const QString &currentColor)
{
    auto *colorMenu = parentMenu->addMenu(QStringLiteral("Color"));
    const struct { QString name; QString code; } colors[] = {
        { QStringLiteral("Red"),    QStringLiteral("1") },
        { QStringLiteral("Orange"), QStringLiteral("2") },
        { QStringLiteral("Yellow"), QStringLiteral("3") },
        { QStringLiteral("Green"),  QStringLiteral("4") },
        { QStringLiteral("Cyan"),   QStringLiteral("5") },
        { QStringLiteral("Purple"), QStringLiteral("6") },
    };
    for (const auto &c : colors) {
        colorMenu->addAction(c.name, [this, nodeId, oldColor = currentColor, code = c.code]() {
            if (!m_document) return;
            m_undoStack->push(new CmdChangeColor(m_document, nodeId, oldColor, code));
        });
    }
    colorMenu->addSeparator();
    colorMenu->addAction(QStringLiteral("Remove Color"), [this, nodeId, oldColor = currentColor]() {
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
        menu.addAction(QStringLiteral("Edit"), [this, cardItem]() {
            beginInlineEdit(cardItem);
        });

        addColorSubmenu(&menu, cardItem->nodeId(), cardItem->nodeData().color);

        menu.addAction(QStringLiteral("Duplicate"), [this, cardItem, scenePos]() {
            if (!m_document)
                return;
            CanvasNode data = cardItem->nodeData();
            data.id = CanvasDocument::generateId();
            data.x += 20;
            data.y += 20;
            m_undoStack->push(new CmdAddCard(m_document, data));
        });

        menu.addSeparator();
        menu.addAction(QStringLiteral("Delete"), [this, cardItem]() {
            if (!m_document)
                return;
            m_undoStack->push(
                new CmdRemoveCard(m_document, cardItem->nodeId()));
        });
    } else if (fileItem) {
        // Right-click on a FileCardItem
        addColorSubmenu(&menu, fileItem->nodeId(), fileItem->nodeData().color);

        menu.addSeparator();
        menu.addAction(QStringLiteral("Delete"), [this, fileItem]() {
            if (!m_document)
                return;
            m_undoStack->push(
                new CmdRemoveCard(m_document, fileItem->nodeId()));
        });
    } else if (grpItem) {
        // Right-click on a GroupItem
        menu.addAction(QStringLiteral("Edit Label"), [this, grpItem]() {
            beginGroupLabelEdit(grpItem);
        });

        addColorSubmenu(&menu, grpItem->nodeId(), grpItem->nodeData().color);

        menu.addAction(QStringLiteral("Duplicate"), [this, grpItem]() {
            if (!m_document)
                return;
            CanvasNode data = grpItem->nodeData();
            data.id = CanvasDocument::generateId();
            data.x += 20;
            data.y += 20;
            m_undoStack->push(new CmdAddCard(m_document, data));
        });

        menu.addSeparator();
        menu.addAction(QStringLiteral("Delete"), [this, grpItem]() {
            if (!m_document)
                return;
            m_undoStack->push(
                new CmdRemoveCard(m_document, grpItem->nodeId()));
        });
    } else if (edgItem) {
        // Right-click on an EdgeItem
        menu.addAction(QStringLiteral("Edit Label"), [this, edgItem]() {
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

        menu.addAction(QStringLiteral("Reverse Direction"), [this, edgItem]() {
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
        menu.addAction(QStringLiteral("Delete"), [this, edgItem]() {
            if (!m_document)
                return;
            m_undoStack->push(
                new CmdRemoveEdge(m_document, edgItem->edgeId()));
        });
    } else {
        // Right-click on empty space
        menu.addAction(QStringLiteral("New Text Card"), [this, scenePos]() {
            if (!m_document)
                return;
            CanvasNode node;
            node.id = CanvasDocument::generateId();
            node.type = NodeType::Text;
            node.x = qRound(scenePos.x());
            node.y = qRound(scenePos.y());
            node.width = 250;
            node.height = 100;
            m_undoStack->push(new CmdAddCard(m_document, node));
        });

        menu.addAction(QStringLiteral("New Group"), [this, scenePos]() {
            if (!m_document)
                return;
            CanvasNode node;
            node.id = CanvasDocument::generateId();
            node.type = NodeType::Group;
            node.x = qRound(scenePos.x());
            node.y = qRound(scenePos.y());
            node.width = 400;
            node.height = 300;
            node.label = QStringLiteral("Group");
            m_undoStack->push(new CmdAddCard(m_document, node));
        });
    }

    menu.exec(event->screenPos());
}

} // namespace Canvas
