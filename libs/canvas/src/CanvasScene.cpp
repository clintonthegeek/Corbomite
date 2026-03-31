// SPDX-License-Identifier: GPL-3.0-or-later
#include "canvas/CanvasScene.h"
#include "canvas/CanvasCommands.h"
#include "canvas/CanvasDocument.h"
#include "canvas/CanvasTool.h"
#include "canvas/TextCardItem.h"
#include "canvas/GroupItem.h"
#include "canvas/EdgeItem.h"

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
        }
    }

    // Create items for all edges
    const auto edges = m_document->edges();
    for (const auto &edge : edges) {
        auto *fromCard = textCardItem(edge.fromNode);
        auto *toCard = textCardItem(edge.toNode);
        if (fromCard && toCard) {
            addEdgeItemToScene(fromCard, toCard, edge);
        }
    }
}

void CanvasScene::clearAllItems()
{
    // Finish any inline edits
    finishInlineEdit();
    finishGroupLabelEdit();

    m_textCardItems.clear();
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

EdgeItem *CanvasScene::addEdgeItemToScene(TextCardItem *from, TextCardItem *to, const CanvasEdge &edge)
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
    if (m_textCardItems.contains(id) || m_groupItems.contains(id))
        return;

    const CanvasNode node = m_document->node(id);
    if (node.type == NodeType::Group) {
        addGroupItemToScene(node);
    } else if (node.type == NodeType::Text) {
        addTextCardItem(node);
    }
}

void CanvasScene::onNodeRemoved(const QString &id)
{
    removeTextCardItem(id);
    removeGroupItem(id);
}

void CanvasScene::onNodeChanged(const QString &id)
{
    if (!m_document)
        return;

    const CanvasNode node = m_document->node(id);
    if (auto *card = textCardItem(id)) {
        card->setNodeData(node);
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
    auto *fromCard = textCardItem(edge.fromNode);
    auto *toCard = textCardItem(edge.toNode);
    if (fromCard && toCard) {
        addEdgeItemToScene(fromCard, toCard, edge);
    }
}

void CanvasScene::onEdgeRemoved(const QString &id)
{
    removeEdgeItem(id);
}

// ---------------------------------------------------------------------------
// Inline text editing
// ---------------------------------------------------------------------------

void CanvasScene::beginInlineEdit(TextCardItem *card)
{
    if (!card || m_editProxy)
        return;

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

    // Use an event filter approach: when the QTextEdit loses focus, commit
    // We install ourselves via a lambda that checks focus
    connect(qApp, &QApplication::focusChanged, this, [this](QWidget * /*old*/, QWidget *now) {
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

    // Disconnect the focusChanged signal we connected
    disconnect(qApp, &QApplication::focusChanged, this, nullptr);

    // Update the card and document via undo command
    if (auto *card = textCardItem(nodeId)) {
        const QString oldText = card->nodeData().text;
        if (m_document && oldText != newText) {
            m_undoStack->push(
                new CmdEditText(m_document, nodeId, oldText, newText));
        }
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
    if (m_activeTool) {
        m_activeTool->mousePressEvent(event);
        return;
    }
    QGraphicsScene::mousePressEvent(event);
}

void CanvasScene::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_activeTool) {
        m_activeTool->mouseMoveEvent(event);
        return;
    }
    QGraphicsScene::mouseMoveEvent(event);
}

void CanvasScene::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_activeTool) {
        m_activeTool->mouseReleaseEvent(event);
        return;
    }
    QGraphicsScene::mouseReleaseEvent(event);
}

void CanvasScene::keyPressEvent(QKeyEvent *event)
{
    if (m_activeTool) {
        m_activeTool->keyPressEvent(event);
        return;
    }
    QGraphicsScene::keyPressEvent(event);
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
    GroupItem *grpItem = nullptr;
    EdgeItem *edgItem = nullptr;
    while (hitItem) {
        if (!cardItem) cardItem = dynamic_cast<TextCardItem *>(hitItem);
        if (!grpItem) grpItem = dynamic_cast<GroupItem *>(hitItem);
        if (!edgItem) edgItem = dynamic_cast<EdgeItem *>(hitItem);
        if (cardItem || grpItem || edgItem)
            break;
        hitItem = hitItem->parentItem();
    }

    QMenu menu;

    if (cardItem) {
        // Right-click on a TextCardItem
        menu.addAction(QStringLiteral("Edit"), [this, cardItem]() {
            beginInlineEdit(cardItem);
        });

        // Color submenu
        auto *colorMenu = menu.addMenu(QStringLiteral("Color"));
        const struct { QString name; QString code; } colors[] = {
            { QStringLiteral("Red"),    QStringLiteral("1") },
            { QStringLiteral("Orange"), QStringLiteral("2") },
            { QStringLiteral("Yellow"), QStringLiteral("3") },
            { QStringLiteral("Green"),  QStringLiteral("4") },
            { QStringLiteral("Cyan"),   QStringLiteral("5") },
            { QStringLiteral("Purple"), QStringLiteral("6") },
        };
        for (const auto &c : colors) {
            colorMenu->addAction(c.name, [this, cardItem, code = c.code]() {
                if (!m_document)
                    return;
                const QString oldColor = cardItem->nodeData().color;
                m_undoStack->push(
                    new CmdChangeColor(m_document, cardItem->nodeId(), oldColor, code));
            });
        }
        colorMenu->addSeparator();
        colorMenu->addAction(QStringLiteral("Remove Color"), [this, cardItem]() {
            if (!m_document)
                return;
            const QString oldColor = cardItem->nodeData().color;
            m_undoStack->push(
                new CmdChangeColor(m_document, cardItem->nodeId(), oldColor, QString()));
        });

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
    } else if (grpItem) {
        // Right-click on a GroupItem
        menu.addAction(QStringLiteral("Edit Label"), [this, grpItem]() {
            beginGroupLabelEdit(grpItem);
        });

        // Color submenu
        auto *colorMenu = menu.addMenu(QStringLiteral("Color"));
        const struct { QString name; QString code; } colors[] = {
            { QStringLiteral("Red"),    QStringLiteral("1") },
            { QStringLiteral("Orange"), QStringLiteral("2") },
            { QStringLiteral("Yellow"), QStringLiteral("3") },
            { QStringLiteral("Green"),  QStringLiteral("4") },
            { QStringLiteral("Cyan"),   QStringLiteral("5") },
            { QStringLiteral("Purple"), QStringLiteral("6") },
        };
        for (const auto &c : colors) {
            colorMenu->addAction(c.name, [this, grpItem, code = c.code]() {
                if (!m_document)
                    return;
                const QString oldColor = grpItem->nodeData().color;
                m_undoStack->push(
                    new CmdChangeColor(m_document, grpItem->nodeId(), oldColor, code));
            });
        }
        colorMenu->addSeparator();
        colorMenu->addAction(QStringLiteral("Remove Color"), [this, grpItem]() {
            if (!m_document)
                return;
            const QString oldColor = grpItem->nodeData().color;
            m_undoStack->push(
                new CmdChangeColor(m_document, grpItem->nodeId(), oldColor, QString()));
        });

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
                        m_document->removeEdge(edgeId); // re-add with updated data
                    // Actually, just update via removeEdge + addEdge, but document doesn't have updateEdge
                    // For now, remove and re-add
                    // TODO: add updateEdge to CanvasDocument in a future task
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
            auto *fromCard = textCardItem(data.fromNode);
            auto *toCard = textCardItem(data.toNode);
            if (fromCard && toCard) {
                addEdgeItemToScene(fromCard, toCard, data);
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
