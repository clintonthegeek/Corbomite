// SPDX-License-Identifier: GPL-3.0-or-later
#include "canvas/CanvasCommands.h"
#include "canvas/CanvasDocument.h"

namespace Canvas {

// ---------------------------------------------------------------------------
// CmdMoveCards
// ---------------------------------------------------------------------------

CmdMoveCards::CmdMoveCards(CanvasDocument *doc,
                           const QHash<QString, QPointF> &oldPositions,
                           const QHash<QString, QPointF> &newPositions)
    : QUndoCommand(QObject::tr("Move Cards"))
    , m_doc(doc)
    , m_oldPositions(oldPositions)
    , m_newPositions(newPositions)
{
}

void CmdMoveCards::redo()
{
    for (auto it = m_newPositions.cbegin(); it != m_newPositions.cend(); ++it) {
        CanvasNode node = m_doc->node(it.key());
        node.x = static_cast<int>(it.value().x());
        node.y = static_cast<int>(it.value().y());
        m_doc->updateNode(node);
    }
}

void CmdMoveCards::undo()
{
    for (auto it = m_oldPositions.cbegin(); it != m_oldPositions.cend(); ++it) {
        CanvasNode node = m_doc->node(it.key());
        node.x = static_cast<int>(it.value().x());
        node.y = static_cast<int>(it.value().y());
        m_doc->updateNode(node);
    }
}

// ---------------------------------------------------------------------------
// CmdResizeCard
// ---------------------------------------------------------------------------

CmdResizeCard::CmdResizeCard(CanvasDocument *doc, const QString &nodeId,
                             const QRect &oldRect, const QRect &newRect)
    : QUndoCommand(QObject::tr("Resize Card"))
    , m_doc(doc)
    , m_nodeId(nodeId)
    , m_oldRect(oldRect)
    , m_newRect(newRect)
{
}

void CmdResizeCard::redo()
{
    CanvasNode node = m_doc->node(m_nodeId);
    node.x = m_newRect.x();
    node.y = m_newRect.y();
    node.width = m_newRect.width();
    node.height = m_newRect.height();
    m_doc->updateNode(node);
}

void CmdResizeCard::undo()
{
    CanvasNode node = m_doc->node(m_nodeId);
    node.x = m_oldRect.x();
    node.y = m_oldRect.y();
    node.width = m_oldRect.width();
    node.height = m_oldRect.height();
    m_doc->updateNode(node);
}

// ---------------------------------------------------------------------------
// CmdAddCard
// ---------------------------------------------------------------------------

CmdAddCard::CmdAddCard(CanvasDocument *doc, const CanvasNode &node)
    : QUndoCommand(QObject::tr("Add Card"))
    , m_doc(doc)
    , m_node(node)
{
}

void CmdAddCard::redo()
{
    m_doc->addNode(m_node);
}

void CmdAddCard::undo()
{
    m_doc->removeNode(m_node.id);
}

// ---------------------------------------------------------------------------
// CmdRemoveCard
// ---------------------------------------------------------------------------

CmdRemoveCard::CmdRemoveCard(CanvasDocument *doc, const QString &nodeId,
                             QUndoCommand *parent)
    : QUndoCommand(QObject::tr("Remove Card"), parent)
    , m_doc(doc)
    , m_nodeId(nodeId)
{
    // Save the node and connected edges for restoration on undo
    m_savedNode = doc->node(nodeId);
    m_savedEdges = doc->edgesForNode(nodeId);
}

void CmdRemoveCard::redo()
{
    // removeNode also removes connected edges and emits signals
    m_doc->removeNode(m_nodeId);
}

void CmdRemoveCard::undo()
{
    m_doc->addNode(m_savedNode);
    for (const auto &edge : m_savedEdges) {
        m_doc->addEdge(edge);
    }
}

// ---------------------------------------------------------------------------
// CmdAddEdge
// ---------------------------------------------------------------------------

CmdAddEdge::CmdAddEdge(CanvasDocument *doc, const CanvasEdge &edge)
    : QUndoCommand(QObject::tr("Add Edge"))
    , m_doc(doc)
    , m_edge(edge)
{
}

void CmdAddEdge::redo()
{
    m_doc->addEdge(m_edge);
}

void CmdAddEdge::undo()
{
    m_doc->removeEdge(m_edge.id);
}

// ---------------------------------------------------------------------------
// CmdRemoveEdge
// ---------------------------------------------------------------------------

CmdRemoveEdge::CmdRemoveEdge(CanvasDocument *doc, const QString &edgeId,
                             QUndoCommand *parent)
    : QUndoCommand(QObject::tr("Remove Edge"), parent)
    , m_doc(doc)
    , m_edgeId(edgeId)
{
    m_savedEdge = doc->edge(edgeId);
}

void CmdRemoveEdge::redo()
{
    m_doc->removeEdge(m_edgeId);
}

void CmdRemoveEdge::undo()
{
    m_doc->addEdge(m_savedEdge);
}

// ---------------------------------------------------------------------------
// CmdEditText
// ---------------------------------------------------------------------------

CmdEditText::CmdEditText(CanvasDocument *doc, const QString &nodeId,
                         const QString &oldText, const QString &newText)
    : QUndoCommand(QObject::tr("Edit Text"))
    , m_doc(doc)
    , m_nodeId(nodeId)
    , m_oldText(oldText)
    , m_newText(newText)
{
}

void CmdEditText::redo()
{
    CanvasNode node = m_doc->node(m_nodeId);
    node.text = m_newText;
    m_doc->updateNode(node);
}

void CmdEditText::undo()
{
    CanvasNode node = m_doc->node(m_nodeId);
    node.text = m_oldText;
    m_doc->updateNode(node);
}

// ---------------------------------------------------------------------------
// CmdChangeColor
// ---------------------------------------------------------------------------

CmdChangeColor::CmdChangeColor(CanvasDocument *doc, const QString &nodeId,
                               const QString &oldColor, const QString &newColor)
    : QUndoCommand(QObject::tr("Change Color"))
    , m_doc(doc)
    , m_nodeId(nodeId)
    , m_oldColor(oldColor)
    , m_newColor(newColor)
{
}

void CmdChangeColor::redo()
{
    CanvasNode node = m_doc->node(m_nodeId);
    node.color = m_newColor;
    m_doc->updateNode(node);
}

void CmdChangeColor::undo()
{
    CanvasNode node = m_doc->node(m_nodeId);
    node.color = m_oldColor;
    m_doc->updateNode(node);
}

} // namespace Canvas
