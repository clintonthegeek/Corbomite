// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QHash>
#include <QPointF>
#include <QRect>
#include <QString>
#include <QUndoCommand>
#include <QVector>

#include "CanvasTypes.h"

namespace Canvas {

class CanvasDocument;

// 1. Move one or more cards
class CmdMoveCards : public QUndoCommand {
public:
    CmdMoveCards(CanvasDocument *doc,
                 const QHash<QString, QPointF> &oldPositions,
                 const QHash<QString, QPointF> &newPositions);
    void redo() override;
    void undo() override;

private:
    CanvasDocument *m_doc;
    QHash<QString, QPointF> m_oldPositions;
    QHash<QString, QPointF> m_newPositions;
};

// 2. Resize a card
class CmdResizeCard : public QUndoCommand {
public:
    CmdResizeCard(CanvasDocument *doc, const QString &nodeId,
                  const QRect &oldRect, const QRect &newRect);
    void redo() override;
    void undo() override;

private:
    CanvasDocument *m_doc;
    QString m_nodeId;
    QRect m_oldRect;
    QRect m_newRect;
};

// 3. Add a card
class CmdAddCard : public QUndoCommand {
public:
    CmdAddCard(CanvasDocument *doc, const CanvasNode &node);
    void redo() override;
    void undo() override;

private:
    CanvasDocument *m_doc;
    CanvasNode m_node;
};

// 4. Remove a card (and its connected edges)
class CmdRemoveCard : public QUndoCommand {
public:
    CmdRemoveCard(CanvasDocument *doc, const QString &nodeId,
                  QUndoCommand *parent = nullptr);
    void redo() override;
    void undo() override;

private:
    CanvasDocument *m_doc;
    QString m_nodeId;
    CanvasNode m_savedNode;
    QVector<CanvasEdge> m_savedEdges;
};

// 5. Add an edge
class CmdAddEdge : public QUndoCommand {
public:
    CmdAddEdge(CanvasDocument *doc, const CanvasEdge &edge);
    void redo() override;
    void undo() override;

private:
    CanvasDocument *m_doc;
    CanvasEdge m_edge;
};

// 6. Remove an edge
class CmdRemoveEdge : public QUndoCommand {
public:
    CmdRemoveEdge(CanvasDocument *doc, const QString &edgeId,
                  QUndoCommand *parent = nullptr);
    void redo() override;
    void undo() override;

private:
    CanvasDocument *m_doc;
    QString m_edgeId;
    CanvasEdge m_savedEdge;
};

// 7. Edit text content of a card
class CmdEditText : public QUndoCommand {
public:
    CmdEditText(CanvasDocument *doc, const QString &nodeId,
                const QString &oldText, const QString &newText);
    void redo() override;
    void undo() override;

private:
    CanvasDocument *m_doc;
    QString m_nodeId;
    QString m_oldText;
    QString m_newText;
};

// 8. Change card color
class CmdChangeColor : public QUndoCommand {
public:
    CmdChangeColor(CanvasDocument *doc, const QString &nodeId,
                   const QString &oldColor, const QString &newColor);
    void redo() override;
    void undo() override;

private:
    CanvasDocument *m_doc;
    QString m_nodeId;
    QString m_oldColor;
    QString m_newColor;
};

} // namespace Canvas
